#!/usr/bin/env python3
"""Judges bot skill casts from telemetry against the MAGIC table.

Reads GameServer bot telemetry (JSONL, F3-01/F3-02) and the MAGIC table
(server data, not personal) and prints one row per skill id with the evidence
needed by T-MECH-SKILL-* (docs/05 section 9): how many casts started, their
outcome, the measured MP drop versus MAGIC.Msp, the recast gap versus
MAGIC.ReCastTime and the fairness guard reasons.

Only MAGIC is queried; no other table is read.  The tool never talks to the
server (it only reads files), and it does not copy raw telemetry lines into
the report.  SELFTEST lines are ignored.

Telemetry fields used (F4-41+):
    ACTION_SUBMIT  (type CastStart/CastFly/CastEffect): decision_id, skill, mp
    ACTION_RESULT  (type CastStart/CastFly/CastEffect): decision_id, ok,
                   reason, code, mp_after
    FAIRNESS_REJECT (type Cast): reason, skill

Usage:
    python3 tools/skill-check.py PATH [PATH ...]
        [--magic FILE | --sqlcmd P --server S --db D]
        [--mp-tol N] [--ms-tol N] [--min-n N] [--json] [--out FILE] [--strict]
    python3 tools/skill-check.py --selftest

PATH is a .jsonl file or a folder (scanned recursively for *.jsonl).
"""

import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile

DEFAULT_SQLCMD = "/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
DEFAULT_SERVER = ".\\SQLEXPRESS"
DEFAULT_DB = "FDP_kn_online"

MAGIC_QUERY = ("SELECT MagicNum, RTRIM(EnName), Msp, CastTime, ReCastTime, "
               "Range, Type1, Type2 FROM MAGIC")

USAGE = (
    "Usage:\n"
    "  python3 tools/skill-check.py PATH [PATH ...]\n"
    "      [--magic FILE | --sqlcmd P --server S --db D]\n"
    "      [--mp-tol N] [--ms-tol N] [--min-n N] [--json] [--out FILE] [--strict]\n"
    "  python3 tools/skill-check.py --selftest\n"
    "Options:\n"
    "  --magic FILE  MAGIC rows (MagicNum|EnName|Msp|CastTime|ReCastTime|Range|Type1|Type2)\n"
    "  --mp-tol N    allowed MP drop band +/- N (default 10)\n"
    "  --ms-tol N    recast lower-bound tolerance in ms (default 50)\n"
    "  --min-n N     minimum samples for an MP verdict (default 3)\n"
    "  --json        write one JSON object instead of Markdown\n"
    "  --out FILE    write the report to FILE (UTF-8, LF) instead of stdout\n"
    "  --strict      exit 1 when any skill verdict is FAIL\n"
)

EVENTS = ("ACTION_SUBMIT", "ACTION_RESULT", "FAIRNESS_REJECT")
CAST_TYPES = ("CastStart", "CastFly", "CastEffect", "Cast")
CLOSED_OUTCOMES = ("effected", "missed", "srv_fail", "no_result", "guard_reject")


class InputError(Exception):
    pass


def as_int(value, default=None):
    """Converts a JSON value to int, returning default when it is not numeric."""
    if value is None:
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def parse_magic_lines(lines):
    """Parses sqlcmd-style MAGIC rows into {MagicNum: {...}}."""
    magic = {}
    for line in lines:
        text = line.strip()
        if not text:
            continue
        parts = [part.strip() for part in text.split("|")]
        if len(parts) < 8:
            raise InputError("bad MAGIC row: %s" % text)
        num = as_int(parts[0])
        if num is None:
            raise InputError("bad MAGIC MagicNum: %s" % text)
        magic[num] = {
            "name": parts[1],
            "msp": as_int(parts[2]),
            "cast_time": as_int(parts[3]),
            "recast_time": as_int(parts[4]),
            "range": as_int(parts[5]),
            "type1": as_int(parts[6]),
            "type2": as_int(parts[7]),
        }
    return magic


def load_magic_file(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return parse_magic_lines(handle.readlines())
    except OSError as exc:
        raise InputError("cannot read MAGIC file: %s" % exc)


def load_magic_sql(sqlcmd, server, db):
    command = [
        sqlcmd, "-S", server, "-E", "-d", db,
        "-W", "-s", "|", "-h", "-1", "-b",
        "-Q", "SET NOCOUNT ON; " + MAGIC_QUERY,
    ]
    try:
        result = subprocess.run(command, capture_output=True)
    except OSError as exc:
        raise InputError("cannot run sqlcmd: %s" % exc)
    stdout = result.stdout.decode("utf-8", errors="replace")
    stderr = result.stderr.decode("utf-8", errors="replace")
    if result.returncode != 0:
        raise InputError("sqlcmd failed (exit %d):\n%s"
                         % (result.returncode, stderr.strip()))
    return parse_magic_lines(stdout.replace("\r", "").split("\n"))


def collect_paths(paths):
    """Resolves files/folders to a sorted, de-duplicated .jsonl path list."""
    found = []
    for path in paths:
        if os.path.isdir(path):
            for root, _dirs, names in os.walk(path):
                for name in names:
                    if name.endswith(".jsonl"):
                        found.append(os.path.join(root, name))
        elif os.path.isfile(path):
            found.append(path)
        else:
            raise InputError("no such path: %s" % path)
    unique = list(dict.fromkeys(found))
    unique.sort()
    return unique


def new_stat():
    return {
        "started": 0,
        "effected": 0,
        "missed": 0,
        "srv_fail": 0,
        "no_result": 0,
        "guard_reject": 0,
        "codes": {},
        "rejects": {},
        "deltas": [],
        "effect_times": [],
        "cast_ms": [],
    }


def bump(mapping, key):
    mapping[key] = mapping.get(key, 0) + 1


def analyze(paths, magic, mp_tol, ms_tol, min_n):
    """Merges cast telemetry per bot and returns the skill report."""
    stats = {}
    totals = {
        "files": 0,
        "skipped_lines": 0,
        "open_records": 0,
        "abandoned_records": 0,
    }

    def stat_for(skill):
        if skill not in stats:
            stats[skill] = new_stat()
        return stats[skill]

    for path in paths:
        totals["files"] += 1
        open_by_bot = {}
        pending = {}
        completed = []

        def close(record, bot):
            if open_by_bot.get(bot) is record:
                open_by_bot[bot] = None
            completed.append(record)

        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for raw in handle:
                text = raw.strip()
                if not text:
                    continue
                try:
                    rec = json.loads(text)
                except ValueError:
                    totals["skipped_lines"] += 1
                    continue
                if not isinstance(rec, dict) or "ev" not in rec:
                    totals["skipped_lines"] += 1
                    continue
                ev = rec.get("ev")
                if ev == "SELFTEST":
                    continue
                if ev not in EVENTS:
                    continue
                typ = rec.get("type")
                if typ not in CAST_TYPES:
                    continue

                bot = rec.get("bot")
                t = as_int(rec.get("t"), 0)
                decision_id = rec.get("decision_id")

                if ev == "ACTION_SUBMIT":
                    skill = as_int(rec.get("skill"), 0)
                    if typ == "CastStart":
                        prev = open_by_bot.get(bot)
                        if prev is not None:
                            prev["outcome"] = "abandoned"
                            totals["abandoned_records"] += 1
                        record = {
                            "bot": bot, "skill": skill, "t_start": t,
                            "mp_before": as_int(rec.get("mp")),
                            "t_effect": None, "outcome": None,
                            "mp_after": None, "code": None,
                        }
                        open_by_bot[bot] = record
                        pending[(bot, decision_id)] = record
                    elif typ == "CastFly":
                        record = open_by_bot.get(bot)
                        if record is not None:
                            record["flying"] = True
                            pending[(bot, decision_id)] = record
                    else:  # CastEffect
                        record = open_by_bot.get(bot)
                        if record is None:
                            record = {
                                "bot": bot, "skill": skill, "t_start": t,
                                "mp_before": as_int(rec.get("mp")),
                                "t_effect": None, "outcome": None,
                                "mp_after": None, "code": None,
                            }
                            open_by_bot[bot] = record
                        record["t_effect"] = t
                        pending[(bot, decision_id)] = record
                    continue

                if ev == "ACTION_RESULT":
                    record = pending.get((bot, decision_id))
                    if record is None:
                        continue
                    if typ in ("CastStart", "CastFly"):
                        if rec.get("ok") is False:
                            record["outcome"] = "srv_fail"
                            record["code"] = as_int(rec.get("code"))
                            close(record, bot)
                        continue
                    record["outcome"] = rec.get("reason")
                    record["code"] = as_int(rec.get("code"))
                    record["mp_after"] = as_int(rec.get("mp_after"))
                    close(record, bot)
                    continue

                if ev == "FAIRNESS_REJECT":
                    if typ != "Cast":
                        continue
                    reason = rec.get("reason")
                    skill = as_int(rec.get("skill"), 0)
                    bump(stat_for(skill)["rejects"], reason)
                    record = open_by_bot.get(bot)
                    if record is not None:
                        record["outcome"] = "guard_reject"
                        close(record, bot)
                    continue

        for record in open_by_bot.values():
            if record is not None and record.get("outcome") is None:
                record["outcome"] = "open"
                totals["open_records"] += 1

        for record in completed:
            skill = record["skill"]
            outcome = record["outcome"]
            stat = stat_for(skill)
            stat["started"] += 1
            if outcome in CLOSED_OUTCOMES:
                stat[outcome] += 1
            if outcome == "srv_fail" and record["code"] is not None:
                bump(stat["codes"], record["code"])
            if outcome in ("effected", "missed"):
                if record["mp_before"] is not None and record["mp_after"] is not None:
                    stat["deltas"].append(record["mp_before"] - record["mp_after"])
            if outcome == "effected" and record["t_effect"] is not None:
                stat["effect_times"].append((record["bot"], record["t_effect"]))
            magic_row = magic.get(skill)
            if (outcome in ("effected", "missed")
                    and magic_row is not None
                    and magic_row.get("cast_time") is not None
                    and magic_row["cast_time"] > 0
                    and record["t_effect"] is not None
                    and record["t_start"] is not None):
                stat["cast_ms"].append(record["t_effect"] - record["t_start"])

    return build_report(stats, totals, magic, mp_tol, ms_tol, min_n)


def build_report(stats, totals, magic, mp_tol, ms_tol, min_n):
    rows = []
    for skill in sorted(stats):
        stat = stats[skill]
        magic_row = magic.get(skill)
        name = magic_row["name"] if magic_row else "?"
        mp_exp = magic_row["msp"] if magic_row else None
        recast_exp = None
        if magic_row is not None and magic_row.get("recast_time") is not None:
            recast_exp = magic_row["recast_time"] * 100

        deltas = stat["deltas"]
        mp_min = min(deltas) if deltas else None
        mp_med = int(statistics.median(deltas)) if deltas else None
        mp_max = max(deltas) if deltas else None
        mp_verdict = judge_mp(mp_exp, deltas, mp_tol, min_n)

        min_gap = min_recast_gap(stat["effect_times"])
        recast_verdict = judge_recast(recast_exp, min_gap, ms_tol)

        if stat["effected"] + stat["missed"] + stat["srv_fail"] == 0:
            effect_verdict = "NO_DATA"
        elif stat["srv_fail"] >= 1:
            effect_verdict = "FAIL"
        elif stat["effected"] >= 1:
            effect_verdict = "PASS"
        else:
            effect_verdict = "FAIL"

        cast_ms_med = int(statistics.median(stat["cast_ms"])) if stat["cast_ms"] else None

        verdicts = (mp_verdict, recast_verdict, effect_verdict)
        if "FAIL" in verdicts:
            verdict = "FAIL"
        elif "WARN" in verdicts:
            verdict = "WARN"
        elif all(item in ("NO_DATA", "n/a") for item in verdicts):
            verdict = "NO_DATA"
        else:
            verdict = "PASS"

        rows.append({
            "skill": skill,
            "name": name,
            "started": stat["started"],
            "effected": stat["effected"],
            "missed": stat["missed"],
            "srv_fail": stat["srv_fail"],
            "no_result": stat["no_result"],
            "guard_reject": stat["guard_reject"],
            "rejects": stat["rejects"],
            "code_hist": stat["codes"],
            "mp_exp": mp_exp,
            "mp_delta_min": mp_min,
            "mp_delta_med": mp_med,
            "mp_delta_max": mp_max,
            "mp_n": len(deltas),
            "mp_verdict": mp_verdict,
            "recast_exp_ms": recast_exp,
            "recast_min_gap_ms": min_gap,
            "recast_verdict": recast_verdict,
            "cast_ms_med": cast_ms_med,
            "effect_verdict": effect_verdict,
            "verdict": verdict,
        })

    counts = {"PASS": 0, "WARN": 0, "FAIL": 0, "NO_DATA": 0}
    for row in rows:
        counts[row["verdict"]] += 1

    summary = {
        "files": totals["files"],
        "skipped_lines": totals["skipped_lines"],
        "open_records": totals["open_records"],
        "abandoned_records": totals["abandoned_records"],
        "pass": counts["PASS"],
        "warn": counts["WARN"],
        "fail": counts["FAIL"],
        "no_data": counts["NO_DATA"],
    }
    return {"skills": rows, "summary": summary}


def judge_mp(mp_exp, deltas, mp_tol, min_n):
    if mp_exp is None or len(deltas) < min_n:
        return "NO_DATA"
    low = mp_exp - mp_tol
    high = mp_exp + mp_tol
    all_inside = all(low <= delta <= high for delta in deltas)
    if all_inside:
        return "PASS"
    median = statistics.median(deltas)
    if low <= median <= high:
        return "WARN"
    return "FAIL"


def min_recast_gap(effect_times):
    by_bot = {}
    for bot, t_effect in effect_times:
        by_bot.setdefault(bot, []).append(t_effect)
    smallest = None
    for times in by_bot.values():
        times.sort()
        for index in range(1, len(times)):
            gap = times[index] - times[index - 1]
            if gap < 0:
                continue
            if smallest is None or gap < smallest:
                smallest = gap
    return smallest


def judge_recast(recast_exp, min_gap, ms_tol):
    if recast_exp is None or recast_exp == 0:
        return "n/a"
    if min_gap is None:
        return "NO_DATA"
    if min_gap >= recast_exp - ms_tol:
        return "PASS"
    return "FAIL"


def format_map(mapping):
    if not mapping:
        return "-"
    return " ".join("%s:%d" % (key, mapping[key]) for key in sorted(mapping))


def format_number(value):
    if value is None:
        return "-"
    return str(value)


def render_markdown(report):
    lines = []
    lines.append("# Skill check report")
    lines.append("")
    lines.append("## Skills")
    lines.append("")
    lines.append(
        "| skill | name | started | effected | missed | srv_fail | no_result | "
        "guard_reject | rejects | code_hist | mp_exp | mp_delta_min | mp_delta_med | "
        "mp_delta_max | mp_verdict | recast_exp_ms | recast_min_gap_ms | "
        "recast_verdict | cast_ms_med | effect_verdict | verdict |")
    lines.append(
        "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    if not report["skills"]:
        lines.append("| (none) | | | | | | | | | | | | | | | | | | | | |")
    for row in report["skills"]:
        cells = [
            str(row["skill"]),
            row["name"],
            str(row["started"]),
            str(row["effected"]),
            str(row["missed"]),
            str(row["srv_fail"]),
            str(row["no_result"]),
            str(row["guard_reject"]),
            format_map(row["rejects"]),
            format_map(row["code_hist"]),
            format_number(row["mp_exp"]),
            format_number(row["mp_delta_min"]),
            format_number(row["mp_delta_med"]),
            format_number(row["mp_delta_max"]),
            row["mp_verdict"],
            format_number(row["recast_exp_ms"]),
            format_number(row["recast_min_gap_ms"]),
            row["recast_verdict"],
            format_number(row["cast_ms_med"]),
            row["effect_verdict"],
            row["verdict"],
        ]
        lines.append("| " + " | ".join(cells) + " |")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    summary = report["summary"]
    lines.append("- files: %d" % summary["files"])
    lines.append("- skipped_lines: %d" % summary["skipped_lines"])
    lines.append("- open_records: %d" % summary["open_records"])
    lines.append("- abandoned_records: %d" % summary["abandoned_records"])
    lines.append("- PASS: %d" % summary["pass"])
    lines.append("- WARN: %d" % summary["warn"])
    lines.append("- FAIL: %d" % summary["fail"])
    lines.append("- NO_DATA: %d" % summary["no_data"])
    lines.append("")
    return "\n".join(lines)


def render_json(report):
    return json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def write_jsonl(path, records):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        for record in records:
            handle.write(json.dumps(record) + "\n")


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def run_selftest():
    checks = []
    failures = []

    def check(name, condition):
        checks.append(name)
        if not condition:
            failures.append(name)

    def submit(cast_type, decision_id, bot=1, t=0, skill=110518, mp=None):
        record = {
            "ev": "ACTION_SUBMIT", "t": t, "bot": bot, "type": cast_type,
            "skill": skill, "decision_id": decision_id,
        }
        if mp is not None:
            record["mp"] = mp
        return record

    def result(cast_type, decision_id, bot=1, t=0, ok=True, reason="casting",
               code=0, mp_after=None):
        record = {
            "ev": "ACTION_RESULT", "t": t, "bot": bot, "type": cast_type,
            "decision_id": decision_id, "ok": ok, "reason": reason, "code": code,
        }
        if mp_after is not None:
            record["mp_after"] = mp_after
        return record

    def reject(bot=1, t=0, reason="out_of_range", skill=None):
        record = {
            "ev": "FAIRNESS_REJECT", "t": t, "bot": bot, "type": "Cast",
            "reason": reason,
        }
        if skill is not None:
            record["skill"] = skill
        return record

    def find_row(report, skill):
        for row in report["skills"]:
            if row["skill"] == skill:
                return row
        return None

    with tempfile.TemporaryDirectory() as tmp:
        case_index = [0]

        def run_case(records, magic, mp_tol=10, ms_tol=50, min_n=3):
            case_index[0] += 1
            path = os.path.join(tmp, "case%02d.jsonl" % case_index[0])
            write_jsonl(path, records)
            return analyze([path], magic, mp_tol, ms_tol, min_n)

        magic = {
            110518: {"name": "Fire ball", "msp": 40, "cast_time": 0,
                     "recast_time": 0, "range": 6, "type1": 3, "type2": 0},
            110530: {"name": "Chain cast", "msp": 40, "cast_time": 2000,
                     "recast_time": 30, "range": 6, "type1": 3, "type2": 0},
        }

        # 1. mp_ok: three casts, delta 40 == Msp 40.
        records = []
        for index in range(3):
            records.append(submit("CastEffect", 100 + index, t=1000 * index, mp=100))
            records.append(result("CastEffect", 100 + index, t=1000 * index + 10,
                                  reason="effected", code=0, mp_after=60))
        report = run_case(records, magic)
        row = find_row(report, 110518)
        check("mp_ok", row["mp_verdict"] == "PASS")

        # 2. mp_off: delta 10 == FAIL; 40,40,40,10 == WARN.
        records = []
        for index in range(3):
            records.append(submit("CastEffect", 200 + index, t=1000 * index, mp=50))
            records.append(result("CastEffect", 200 + index, t=1000 * index + 10,
                                  reason="effected", code=0, mp_after=40))
        row = find_row(run_case(records, magic), 110518)
        check("mp_off_fail", row["mp_verdict"] == "FAIL")

        records = []
        for index in range(3):
            records.append(submit("CastEffect", 300 + index, t=1000 * index, mp=100))
            records.append(result("CastEffect", 300 + index, t=1000 * index + 10,
                                  reason="effected", code=0, mp_after=60))
        records.append(submit("CastEffect", 303, t=3000, mp=50))
        records.append(result("CastEffect", 303, t=3010,
                              reason="effected", code=0, mp_after=40))
        row = find_row(run_case(records, magic), 110518)
        check("mp_off_warn", row["mp_verdict"] == "WARN")

        # 3. mp_no_data: only two samples.
        records = []
        for index in range(2):
            records.append(submit("CastEffect", 400 + index, t=1000 * index, mp=100))
            records.append(result("CastEffect", 400 + index, t=1000 * index + 10,
                                  reason="effected", code=0, mp_after=60))
        row = find_row(run_case(records, magic), 110518)
        check("mp_no_data", row["mp_verdict"] == "NO_DATA")

        # 4. recast_ok / recast_fail / recast_no_data (ReCastTime 30 == 3000 ms).
        recast_magic = {
            110518: {"name": "Recast skill", "msp": 40, "cast_time": 0,
                     "recast_time": 30, "range": 6, "type1": 3, "type2": 0},
        }

        def recast_case(times):
            records = []
            for index, t in enumerate(times):
                records.append(submit("CastEffect", 500 + index, t=t, mp=100))
                records.append(result("CastEffect", 500 + index, t=t + 10,
                                      reason="effected", code=0, mp_after=60))
            return run_case(records, recast_magic)

        row = find_row(recast_case([0, 3100, 6200]), 110518)
        check("recast_ok", row["recast_verdict"] == "PASS")
        row = find_row(recast_case([0, 2900]), 110518)
        check("recast_fail", row["recast_verdict"] == "FAIL")
        row = find_row(recast_case([0]), 110518)
        check("recast_no_data", row["recast_verdict"] == "NO_DATA")

        # 5. chain_start_effect: CastStart then CastEffect, cast_ms = 2200.
        records = [
            submit("CastStart", 1, t=1000, skill=110530, mp=100),
            result("CastStart", 1, t=1010, ok=True, reason="casting"),
            submit("CastEffect", 2, t=3200, skill=110530, mp=80),
            result("CastEffect", 2, t=3210, reason="effected", code=0, mp_after=60),
        ]
        row = find_row(run_case(records, magic), 110530)
        check("chain_start_effect_count", row["started"] == 1)
        check("chain_start_effect_ms", row["cast_ms_med"] == 2200)

        # 6. chain_no_cast_time: only CastEffect (CastTime 0).
        records = [
            submit("CastEffect", 1, t=0, skill=110518, mp=100),
            result("CastEffect", 1, t=10, reason="effected", code=0, mp_after=60),
        ]
        row = find_row(run_case(records, magic), 110518)
        check("chain_no_cast_time", row["started"] == 1)

        # 7. flying_chain: CastStart -> CastFly -> CastEffect, single record.
        records = [
            submit("CastStart", 1, t=0, skill=110530, mp=100),
            result("CastStart", 1, t=10, ok=True, reason="casting"),
            submit("CastFly", 2, t=100, skill=110530),
            result("CastFly", 2, t=110, ok=True, reason="flying"),
            submit("CastEffect", 3, t=1100, skill=110530),
            result("CastEffect", 3, t=1110, reason="effected", code=0, mp_after=60),
        ]
        row = find_row(run_case(records, magic), 110530)
        check("flying_chain_count", row["started"] == 1)
        check("flying_chain_effect", row["effected"] == 1)

        # 8. srv_fail_code: code -103 recorded, effect verdict FAIL.
        records = [
            submit("CastEffect", 1, t=0, skill=110518, mp=100),
            result("CastEffect", 1, t=10, ok=False, reason="srv_fail", code=-103),
        ]
        row = find_row(run_case(records, magic), 110518)
        check("srv_fail_code_verdict", row["effect_verdict"] == "FAIL")
        check("srv_fail_code_hist", row["code_hist"] == {-103: 1})

        # 9. guard_reject_attribution: known skill and unknown (missing field).
        records = [
            reject(bot=1, t=0, reason="out_of_range", skill=110518),
            reject(bot=1, t=10, reason="no_mana"),
        ]
        report = run_case(records, magic)
        row = find_row(report, 110518)
        check("guard_reject_attribution", row["rejects"] == {"out_of_range": 1})
        unknown = find_row(report, 0)
        check("guard_reject_unknown", unknown["rejects"] == {"no_mana": 1})

        # 10. abandoned: a second CastStart abandons the first open record.
        records = [
            submit("CastStart", 1, t=0, skill=110530, mp=100),
            submit("CastStart", 2, t=100, skill=110530, mp=100),
        ]
        report = run_case(records, magic)
        check("abandoned", report["summary"]["abandoned_records"] == 1)
        check("abandoned_open", report["summary"]["open_records"] == 1)

        # 11. bad_lines: malformed JSON and a line without ev are skipped.
        path = os.path.join(tmp, "bad_lines.jsonl")
        valid = submit("CastEffect", 1, t=0, skill=110518, mp=100)
        valid_result = result("CastEffect", 1, t=10, reason="effected",
                              code=0, mp_after=60)
        write_text(path, "not json at all\n" + json.dumps({"bot": 1}) + "\n"
                   + json.dumps(valid) + "\n" + json.dumps(valid_result) + "\n")
        bad_report = analyze([path], magic, 10, 50, 3)
        check("bad_lines", bad_report["summary"]["skipped_lines"] == 2)

        # 12. unknown_skill: not in MAGIC, no expected value, no crash.
        records = [
            submit("CastEffect", 1, t=0, skill=999999, mp=100),
            result("CastEffect", 1, t=10, reason="effected", code=0, mp_after=60),
        ]
        row = find_row(run_case(records, magic), 999999)
        check("unknown_skill_name", row["name"] == "?")
        check("unknown_skill_mp", row["mp_exp"] is None and row["mp_verdict"] == "NO_DATA")
        check("unknown_skill_recast", row["recast_verdict"] == "n/a")

        # 13. strict_exit: --strict returns 1 on FAIL and 0 without FAIL.
        magic_path = os.path.join(tmp, "magic.txt")
        write_text(magic_path, "110518|Fire ball|40|0|0|6|3|0\n")
        fail_path = os.path.join(tmp, "strict_fail.jsonl")
        write_jsonl(fail_path, [
            submit("CastEffect", 1, t=0, skill=110518, mp=100),
            result("CastEffect", 1, t=10, ok=False, reason="srv_fail", code=-103),
        ])
        pass_path = os.path.join(tmp, "strict_pass.jsonl")
        write_jsonl(pass_path, [
            submit("CastEffect", 1, t=0, skill=110518, mp=100),
            result("CastEffect", 1, t=10, reason="effected", code=0, mp_after=60),
        ])
        out_path = os.path.join(tmp, "strict_out.txt")
        rc_fail = main([fail_path, "--magic", magic_path, "--strict", "--out", out_path])
        rc_pass = main([pass_path, "--magic", magic_path, "--strict", "--out", out_path])
        check("strict_exit_fail", rc_fail == 1)
        check("strict_exit_pass", rc_pass == 0)

        # 14. json_out: --json output is valid JSON with skill rows.
        records = []
        for index in range(3):
            records.append(submit("CastEffect", 700 + index, t=1000 * index, mp=100))
            records.append(result("CastEffect", 700 + index, t=1000 * index + 10,
                                  reason="effected", code=0, mp_after=60))
        report = run_case(records, magic)
        parsed = json.loads(render_json(report))
        check("json_out", "skills" in parsed and len(parsed["skills"]) == 1)

        # 15. sqlcmd_non_utf8: a fake sqlcmd with a non-UTF-8 byte must not crash.
        if os.name == "posix":
            fake_sqlcmd = os.path.join(tmp, "fake_sqlcmd.sh")
            write_text(fake_sqlcmd,
                       "#!/bin/sh\n"
                       "printf '105660|sacrifice\\250|180|0|250|67|3|0\\n'\n")
            os.chmod(fake_sqlcmd, 0o755)
            try:
                sql_magic = load_magic_sql(fake_sqlcmd, "s", "d")
            except InputError:
                sql_magic = None
            if sql_magic is None:
                check("sqlcmd_non_utf8_failed", False)
            else:
                row_ok = sql_magic.get(105660, {}).get("msp") == 180
                check("sqlcmd_non_utf8", row_ok)
        else:
            check("sqlcmd_non_utf8_skipped", True)

    if failures:
        for name in failures:
            sys.stderr.write("FAILED: %s\n" % name)
    print("selftest: %d checks, %d failed" % (len(checks), len(failures)))
    return 0 if not failures else 1


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]
    if "--selftest" in argv:
        return run_selftest()

    parser = argparse.ArgumentParser(prog="skill-check.py", usage=USAGE)
    parser.add_argument("paths", nargs="*")
    parser.add_argument("--magic")
    parser.add_argument("--sqlcmd", default=DEFAULT_SQLCMD)
    parser.add_argument("--server", default=DEFAULT_SERVER)
    parser.add_argument("--db", default=DEFAULT_DB)
    parser.add_argument("--mp-tol", type=int, default=10)
    parser.add_argument("--ms-tol", type=int, default=50)
    parser.add_argument("--min-n", type=int, default=3)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--out")
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args(argv)

    if not args.paths:
        parser.error("at least one PATH is required")

    try:
        if args.magic:
            magic = load_magic_file(args.magic)
        else:
            magic = load_magic_sql(args.sqlcmd, args.server, args.db)
        resolved = collect_paths(args.paths)
        if not resolved:
            raise InputError("no .jsonl files found")
        report = analyze(resolved, magic, args.mp_tol, args.ms_tol, args.min_n)
    except InputError as exc:
        sys.stderr.write("error: %s\n" % exc)
        return 2

    text = render_json(report) if args.json else render_markdown(report)
    if args.out:
        try:
            write_text(args.out, text)
        except OSError as exc:
            sys.stderr.write("error: cannot write output: %s\n" % exc)
            return 2
    else:
        sys.stdout.write(text)

    if args.strict and report["summary"]["fail"] > 0:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
