#!/usr/bin/env python3
"""Builds a Markdown/JSON report from GameServer bot telemetry (JSONL).

Inputs are telemetry files written by F3-01/F3-02:
  Logs/bots/<date>/<match>.jsonl   match file (MATCH_START ... MATCH_END)
  Logs/bots/<date>/live-<HHMMSS>.jsonl   process file (no match, match = "-")
  Logs/bots/<date>/<match>.summary.json  per-match summary (cross-check only)

The tool only reads files (apart from --out) and never talks to the server.
Event counts are generic: unknown event types (DECISION, DAMAGE, ...) are
counted and passed through, so later phases can extend the server without
breaking this report.  SELFTEST lines are ignored.  MET-PERF-02 (BotManager
tick) is summarised per source file; the p95_est_us column is a tick_n
weighted estimate, not an exact percentile, and is labelled accordingly.

Usage:
    python3 tools/bot-telemetry-report.py PATH [PATH ...] [--json]
        [--out FILE] [--strict]
    python3 tools/bot-telemetry-report.py --selftest

PATH is a .jsonl file or a folder (scanned recursively for *.jsonl).
"""

import json
import os
import statistics
import sys
import tempfile

USAGE = (
    "Usage:\n"
    "  python3 tools/bot-telemetry-report.py PATH [PATH ...] [--json]\n"
    "      [--out FILE] [--strict]\n"
    "  python3 tools/bot-telemetry-report.py --selftest\n"
    "Options:\n"
    "  --json     write one JSON object instead of Markdown\n"
    "  --out FILE write the report to FILE (UTF-8, LF) instead of stdout\n"
    "  --strict   exit 1 when a match is INVALID or a MET-PERF-02 verdict is FAIL\n"
)

PERF_REQUIRED = (
    "tick_n", "tick_p50_us", "tick_p95_us", "tick_p99_us", "tick_max_us",
    "in_game", "skipped_ticks", "dropped_soft", "dropped_hard",
)

MATCH_TABLE_HEAD = (
    "| match | scenario | seed | run | mode | duration_s | result | "
    "composition | in_game | perf_samples | dropped_soft | dropped_hard | "
    "status | reasons |"
)

PERF_TABLE_HEAD = (
    "| file | windows | ticks | p50_med_us | p95_est_us | p95_worst_us | "
    "p99_worst_us | max_us | in_game_max | skipped_max | dropped_soft (cum.) | "
    "dropped_hard (cum.) | budget_us | verdict |"
)

VERDICT_NOTE = (
    "Verdict uses the worst window p95 (conservative); budget is 5000 us for "
    "<=16 bots and 15000 us for <=64 bots (more bots: no budget); p95_est_us "
    "is a tick_n-weighted estimate and is informational only."
)

ACT_GATE_PCT = 1      # docs/17 F4 acceptance: invalid actions <= 1 % on scripted runs
ACT_LIMIT_PCT = 2     # docs/16 MET-ACT-02: <= 2 %
SRV_INVALID_REASONS = ("srv_fail", "handler_noop")   # plus every reason starting with "refused_"

ACT_NOTE = (
    "Invalid = ACTION_RESULT ok=false with reason srv_fail, handler_noop or "
    "refused_*; no_result (no server confirmation) and guard rejections are "
    "not counted. PASS <= 1 % (F4 gate), WARN <= 2 % (MET-ACT-02), FAIL "
    "above. All action types are included."
)

FAIR_NOTE = (
    "Informational. Rejects never reach the server. \"Violations that reached "
    "the server = 0\" is not measurable from telemetry (verify by code "
    "review)."
)

SCRIPT_NOTE = (
    "Counts are attributed by file order between SCRIPT_START and SCRIPT_END "
    "(approximate: follow-up results of the last step may land after "
    "SCRIPT_END)."
)


def is_int(value):
    return isinstance(value, int) and not isinstance(value, bool)


def add_bot(info, record):
    """Records a non-negative integer bot id for the MET-FAIR-01 estimate."""
    bot = record.get("bot")
    if is_int(bot) and bot >= 0:
        info["bots"].add(bot)


def classify_action_result(ok, reason):
    """Maps one ACTION_RESULT to ok / invalid / no_result / other."""
    if ok is True:
        return "ok"
    if ok is False:
        if isinstance(reason, str) and (reason in SRV_INVALID_REASONS
                                        or reason.startswith("refused_")):
            return "invalid"
        if reason == "no_result":
            return "no_result"
        return "other"
    return "other"


def act_verdict(submit, invalid):
    """PASS <= 1 % (F4 gate), WARN <= 2 % (MET-ACT-02), FAIL above."""
    if submit == 0:
        return "NO_DATA"
    if invalid * 100 <= submit * ACT_GATE_PCT:
        return "PASS"
    if invalid * 100 <= submit * ACT_LIMIT_PCT:
        return "WARN"
    return "FAIL"


def extract_perf_window(record):
    """Returns the window dict when every required field is an int, else None."""
    window = {}
    for key in PERF_REQUIRED:
        value = record.get(key)
        if not is_int(value):
            return None
        window[key] = value
    return window


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
            raise OSError("no such path: %s" % path)
    unique = list(dict.fromkeys(found))
    unique.sort()
    return unique


def load_file(path):
    """Reads one .jsonl file line by line; never raises on malformed content."""
    info = {
        "path": path,
        "lines": 0,
        "bad_lines": 0,
        "first_bad_line": None,
        "event_counts": {},
        "ignored_selftest": 0,
        "perf_windows": [],
        "match_start": None,
        "match_end": None,
        "test_teleport": False,
        "mode": "-",
        "warnings": [],
        "act_submit": {},
        "act_result": {},
        "act_reasons": {},
        "fair": {"count": 0, "by": {}},
        "bots": set(),
        "t_first": None,
        "t_last": None,
        "scripts": [],
    }
    previous_t = None
    monotonic_warned = False
    perf_index = 0
    open_script = None
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for lineno, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            info["lines"] += 1
            try:
                record = json.loads(raw)
            except ValueError:
                record = None
            if not isinstance(record, dict):
                info["bad_lines"] += 1
                if info["first_bad_line"] is None:
                    info["first_bad_line"] = lineno
                continue
            event = record.get("ev")
            if not isinstance(event, str):
                info["bad_lines"] += 1
                if info["first_bad_line"] is None:
                    info["first_bad_line"] = lineno
                continue

            t_value = record.get("t")
            if is_int(t_value):
                if previous_t is not None and t_value < previous_t \
                        and not monotonic_warned:
                    info["warnings"].append(
                        "t not monotonic at line %d" % lineno)
                    monotonic_warned = True
                previous_t = t_value

            if info["mode"] == "-" and isinstance(record.get("mode"), str):
                info["mode"] = record["mode"]

            if event == "SELFTEST":
                info["ignored_selftest"] += 1
                continue
            info["event_counts"][event] = \
                info["event_counts"].get(event, 0) + 1

            if is_int(t_value):
                if info["t_first"] is None or t_value < info["t_first"]:
                    info["t_first"] = t_value
                if info["t_last"] is None or t_value > info["t_last"]:
                    info["t_last"] = t_value

            if event == "ACTION_SUBMIT":
                action_type = record.get("type")
                if not isinstance(action_type, str):
                    action_type = "?"
                info["act_submit"][action_type] = \
                    info["act_submit"].get(action_type, 0) + 1
                add_bot(info, record)
                if open_script is not None:
                    open_script["submit"] += 1
            elif event == "ACTION_RESULT":
                action_type = record.get("type")
                if not isinstance(action_type, str):
                    action_type = "?"
                label = classify_action_result(record.get("ok"),
                                               record.get("reason"))
                bucket = info["act_result"].setdefault(
                    action_type,
                    {"ok": 0, "invalid": 0, "no_result": 0, "other": 0})
                bucket[label] += 1
                if label != "ok":
                    reason = record.get("reason")
                    if not isinstance(reason, str):
                        reason = "?"
                    key = (action_type, reason, label)
                    info["act_reasons"][key] = \
                        info["act_reasons"].get(key, 0) + 1
                add_bot(info, record)
                if open_script is not None:
                    if label == "invalid":
                        open_script["invalid"] += 1
                    elif label == "no_result":
                        open_script["no_result"] += 1
            elif event == "FAIRNESS_REJECT":
                action_type = record.get("type")
                if not isinstance(action_type, str):
                    action_type = "?"
                rule = record.get("rule")
                if not isinstance(rule, str):
                    rule = "?"
                reason = record.get("reason")
                if not isinstance(reason, str):
                    reason = "?"
                info["fair"]["count"] += 1
                key = (action_type, rule, reason)
                info["fair"]["by"][key] = info["fair"]["by"].get(key, 0) + 1
                add_bot(info, record)
                if open_script is not None:
                    open_script["rejects"] += 1
            elif event == "SCRIPT_START":
                if open_script is not None:
                    open_script["result"] = "NO_END"
                open_script = {
                    "script": record.get("script"),
                    "steps": record.get("steps"),
                    "duration_ms": record.get("duration_ms"),
                    "start_t": t_value,
                    "end_t": None,
                    "result": None,
                    "steps_run": None,
                    "steps_total": None,
                    "elapsed_ms": None,
                    "max_late_ms": None,
                    "steps_seen": 0,
                    "submit": 0,
                    "invalid": 0,
                    "no_result": 0,
                    "rejects": 0,
                }
                info["scripts"].append(open_script)
            elif event == "SCRIPT_STEP":
                if open_script is not None:
                    open_script["steps_seen"] += 1
            elif event == "SCRIPT_END":
                if open_script is not None:
                    open_script["result"] = record.get("result")
                    open_script["steps_run"] = record.get("steps_run")
                    open_script["steps_total"] = record.get("steps_total")
                    open_script["elapsed_ms"] = record.get("elapsed_ms")
                    open_script["max_late_ms"] = record.get("max_late_ms")
                    open_script["end_t"] = t_value
                    open_script = None

            if event == "MATCH_START":
                if info["match_start"] is None:
                    info["match_start"] = record
                else:
                    warning = "multiple MATCH_START (using first)"
                    if warning not in info["warnings"]:
                        info["warnings"].append(warning)
            elif event == "MATCH_END":
                if info["match_end"] is None:
                    info["match_end"] = record
            elif event == "TEST_TELEPORT":
                info["test_teleport"] = True
            elif event == "PERF_SAMPLE":
                perf_index += 1
                window = extract_perf_window(record)
                if window is None:
                    info["warnings"].append(
                        "perf sample %d skipped (missing field)" % perf_index)
                else:
                    info["perf_windows"].append(window)

    if open_script is not None:
        open_script["result"] = "NO_END"
        open_script["steps_run"] = open_script["steps_seen"]

    base = os.path.basename(path)
    for action_type in sorted(info["act_submit"]):
        submitted = info["act_submit"][action_type]
        bucket = info["act_result"].get(action_type, {})
        resulted = sum(bucket.values())
        if submitted != resulted:
            info["warnings"].append(
                "%s: ACTION_SUBMIT/ACTION_RESULT differ for %s (%d vs %d)"
                % (base, action_type, submitted, resulted))
    other_count = 0
    for bucket in info["act_result"].values():
        other_count += bucket.get("other", 0)
    if other_count > 0:
        info["warnings"].append(
            "%s: %d ACTION_RESULT with unknown ok/reason"
            % (base, other_count))
    for script in info["scripts"]:
        if script["end_t"] is not None and script["steps_run"] is not None \
                and script["steps_run"] != script["steps_seen"]:
            info["warnings"].append(
                "%s: script %s steps_run %s != SCRIPT_STEP %d"
                % (base, script["script"], script["steps_run"],
                   script["steps_seen"]))
    return info


def load_summary(path):
    """Reads the sibling <stem>.summary.json, or None when absent/unreadable."""
    if path.endswith(".jsonl"):
        stem = path[:-len(".jsonl")]
    else:
        stem = path
    summary_path = stem + ".summary.json"
    if not os.path.isfile(summary_path):
        return None
    try:
        with open(summary_path, "r", encoding="utf-8",
                  errors="replace") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        return None
    return data if isinstance(data, dict) else None


def format_value(value):
    if value is None:
        return "-"
    return str(value)


def format_events(mapping):
    if not isinstance(mapping, dict):
        return "-"
    parts = ["%s:%s" % (key, mapping[key]) for key in sorted(mapping)]
    return "{" + ",".join(parts) + "}"


def check_summary(info, summary):
    """Cross-checks one file against its summary.json; returns warning list."""
    warnings = []
    theirs_lines = summary.get("lines")
    if theirs_lines != info["lines"]:
        warnings.append("summary mismatch: lines %s != %s"
                        % (info["lines"], format_value(theirs_lines)))

    ours_events = dict(info["event_counts"])
    ours_events.pop("SELFTEST", None)
    theirs_events = summary.get("events")
    if isinstance(theirs_events, dict):
        theirs_events = {key: value for key, value in theirs_events.items()
                         if key != "SELFTEST"}
        if theirs_events != ours_events:
            warnings.append("summary mismatch: events %s != %s"
                            % (format_events(ours_events),
                               format_events(theirs_events)))

    start = summary.get("start")
    if not isinstance(start, dict):
        start = {}
    if info["match_start"] is not None:
        for field in ("scenario", "seed", "run"):
            ours = info["match_start"].get(field)
            theirs = start.get(field)
            if ours != theirs:
                warnings.append("summary mismatch: start.%s %s != %s"
                                % (field, format_value(ours),
                                   format_value(theirs)))

    end = summary.get("end")
    if not isinstance(end, dict):
        end = {}
    if info["match_end"] is not None:
        ours = info["match_end"].get("duration_ms")
        theirs = end.get("duration_ms")
        if ours != theirs:
            warnings.append("summary mismatch: end.duration_ms %s != %s"
                            % (format_value(ours), format_value(theirs)))
    return warnings


def build_match(info, name):
    """Builds one Matches row, or None when the file carries no match."""
    start = info["match_start"]
    end = info["match_end"]
    if start is None and end is None:
        return None

    if start is not None and isinstance(start.get("match"), str):
        match_id = start["match"]
    elif end is not None and isinstance(end.get("match"), str):
        match_id = end["match"]
    else:
        match_id = "-"

    scenario = start.get("scenario") if start else None
    seed = start.get("seed") if start else None
    run = start.get("run") if start else None

    mode = "-"
    if start is not None and isinstance(start.get("mode"), str):
        mode = start["mode"]
    elif end is not None and isinstance(end.get("mode"), str):
        mode = end["mode"]
    elif info["mode"] != "-":
        mode = info["mode"]

    composition = start.get("composition") if start else None
    if not isinstance(composition, list):
        composition = None
    n_comp = len(composition) if composition is not None else None

    in_game_start = start.get("in_game") if start else None
    duration_ms = end.get("duration_ms") if end else None
    if not is_int(duration_ms) and not isinstance(duration_ms, float):
        duration_ms = None
    result = end.get("result") if end else None
    dropped_soft = end.get("dropped_soft") if end else None
    dropped_hard = end.get("dropped_hard") if end else None

    reasons = []
    if start is not None and end is None:
        reasons.append("NO_END")
    if end is not None and end.get("result") == "aborted":
        reasons.append("ABORTED")
    if (start is not None and n_comp is not None
            and is_int(in_game_start) and in_game_start < n_comp):
        reasons.append("SPAWN_SHORT")
    if is_int(dropped_hard) and dropped_hard > 0:
        reasons.append("DROPPED_HARD")
    if mode == "eval" and info["test_teleport"]:
        reasons.append("TELEPORT_IN_EVAL")
    if info["bad_lines"] > 0:
        reasons.append("BAD_LINES")

    duration_s = None
    if duration_ms is not None:
        duration_s = duration_ms / 1000.0

    return {
        "match": match_id,
        "scenario": scenario,
        "seed": seed,
        "run": run,
        "mode": mode,
        "duration_s": duration_s,
        "result": result,
        "composition": composition,
        "in_game": in_game_start,
        "perf_samples": len(info["perf_windows"]),
        "dropped_soft": dropped_soft,
        "dropped_hard": dropped_hard,
        "status": "VALID" if not reasons else "INVALID",
        "reasons": reasons,
    }


def build_perf_row(name, windows):
    """Aggregates the valid PERF_SAMPLE windows of one file (MET-PERF-02)."""
    row = {
        "file": name,
        "windows": len(windows),
        "ticks": None,
        "p50_med_us": None,
        "p95_est_us": None,
        "p95_worst_us": None,
        "p99_worst_us": None,
        "max_us": None,
        "in_game_max": None,
        "skipped_max": None,
        "dropped_soft_cum": None,
        "dropped_hard_cum": None,
        "budget_us": None,
        "verdict": "NO_DATA",
    }
    if not windows:
        return row

    ticks = sum(window["tick_n"] for window in windows)
    row["ticks"] = ticks
    row["p50_med_us"] = int(round(statistics.median(
        window["tick_p50_us"] for window in windows)))
    if ticks > 0:
        weighted = sum(window["tick_n"] * window["tick_p95_us"]
                       for window in windows)
        row["p95_est_us"] = int(round(weighted / float(ticks)))
    row["p95_worst_us"] = max(window["tick_p95_us"] for window in windows)
    row["p99_worst_us"] = max(window["tick_p99_us"] for window in windows)
    row["max_us"] = max(window["tick_max_us"] for window in windows)
    in_game_max = max(window["in_game"] for window in windows)
    row["in_game_max"] = in_game_max
    row["skipped_max"] = max(window["skipped_ticks"] for window in windows)
    row["dropped_soft_cum"] = max(
        window["dropped_soft"] for window in windows)
    row["dropped_hard_cum"] = max(
        window["dropped_hard"] for window in windows)

    if in_game_max <= 16:
        budget = 5000
    elif in_game_max <= 64:
        budget = 15000
    else:
        budget = None
    row["budget_us"] = budget
    if budget is None:
        row["verdict"] = "NO_BUDGET"
    elif row["p95_worst_us"] <= budget:
        row["verdict"] = "OK"
    else:
        row["verdict"] = "FAIL"
    return row


def build_action_row(name, info):
    """Builds one MET-ACT-02 row for a file, or None when it has no actions."""
    submit = sum(info["act_submit"].values())
    counts = {"ok": 0, "invalid": 0, "no_result": 0, "other": 0}
    result = 0
    for bucket in info["act_result"].values():
        for key in counts:
            counts[key] += bucket.get(key, 0)
        result += sum(bucket.values())
    if submit == 0 and result == 0:
        return None
    invalid_pct = None
    if submit > 0:
        invalid_pct = round(counts["invalid"] * 100.0 / submit, 2)
    return {
        "file": name,
        "submit": submit,
        "result": result,
        "ok": counts["ok"],
        "invalid": counts["invalid"],
        "no_result": counts["no_result"],
        "other": counts["other"],
        "invalid_pct": invalid_pct,
        "verdict": act_verdict(submit, counts["invalid"]),
    }


def build_action_total(rows):
    """Sums MET-ACT-02 rows; verdict is recomputed from the totals."""
    submit = sum(row["submit"] for row in rows)
    invalid = sum(row["invalid"] for row in rows)
    invalid_pct = None
    if submit > 0:
        invalid_pct = round(invalid * 100.0 / submit, 2)
    return {
        "file": "(total)",
        "submit": submit,
        "result": sum(row["result"] for row in rows),
        "ok": sum(row["ok"] for row in rows),
        "invalid": invalid,
        "no_result": sum(row["no_result"] for row in rows),
        "other": sum(row["other"] for row in rows),
        "invalid_pct": invalid_pct,
        "verdict": act_verdict(submit, invalid),
    }


def build_fair_row(name, info):
    """Builds one MET-FAIR-01 row for a file, or None when it has none."""
    count = info["fair"]["count"]
    if count <= 0:
        return None
    in_game_max = 0
    for window in info["perf_windows"]:
        if window["in_game"] > in_game_max:
            in_game_max = window["in_game"]
    bots = max(len(info["bots"]), in_game_max)
    span_s = None
    if info["t_first"] is not None and info["t_last"] is not None:
        span_s = (info["t_last"] - info["t_first"]) / 1000.0
    bot_hours = None
    if bots > 0 and span_s is not None and span_s > 0:
        bot_hours = bots * span_s / 3600.0
    rate = None
    if bot_hours:
        rate = round(count / bot_hours, 2)
    return {
        "file": name,
        "rejects": count,
        "bots": bots,
        "span_s": span_s,
        "bot_hours_est": round(bot_hours, 4) if bot_hours is not None else None,
        "rejects_per_bot_hour_est": rate,
    }


def build_script_rows(name, info):
    """Builds one Scripts row per SCRIPT_START..SCRIPT_END run of a file."""
    rows = []
    for script in info["scripts"]:
        steps_total = script["steps_total"]
        if steps_total is None:
            steps_total = script["steps"]
        rows.append({
            "file": name,
            "script": script["script"],
            "steps_total": steps_total,
            "steps_run": script["steps_run"],
            "result": script["result"],
            "elapsed_ms": script["elapsed_ms"],
            "max_late_ms": script["max_late_ms"],
            "submit": script["submit"],
            "invalid": script["invalid"],
            "no_result": script["no_result"],
            "rejects": script["rejects"],
        })
    return rows


def display_names(paths):
    """Maps each path to a short name; collisions get the parent folder."""
    counts = {}
    for path in paths:
        base = os.path.basename(path)
        counts[base] = counts.get(base, 0) + 1
    names = {}
    for path in paths:
        base = os.path.basename(path)
        if counts[base] > 1:
            names[path] = os.path.basename(os.path.dirname(path)) + "/" + base
        else:
            names[path] = base
    return names


def gather(paths):
    """Loads every path (cross-checking summaries) and builds the report."""
    infos = []
    for path in paths:
        info = load_file(path)
        summary = load_summary(path)
        if summary is not None:
            info["warnings"].extend(check_summary(info, summary))
        infos.append(info)

    names = display_names(paths)
    files = []
    matches = []
    perf = []
    actions = []
    fairness = []
    scripts = []
    action_reason_totals = {}
    fairness_reason_totals = {}
    totals = {}
    ignored = 0
    warnings = []
    for info in infos:
        name = names[info["path"]]
        match_id = "-"
        if info["match_start"] is not None \
                and isinstance(info["match_start"].get("match"), str):
            match_id = info["match_start"]["match"]
        file_events = sum(info["event_counts"].values())
        files.append({
            "file": name,
            "match": match_id,
            "mode": info["mode"],
            "lines": info["lines"],
            "bad_lines": info["bad_lines"],
            "events": file_events,
        })
        match_row = build_match(info, name)
        if match_row is not None:
            matches.append(match_row)
        perf.append(build_perf_row(name, info["perf_windows"]))
        action_row = build_action_row(name, info)
        if action_row is not None:
            actions.append(action_row)
        fair_row = build_fair_row(name, info)
        if fair_row is not None:
            fairness.append(fair_row)
        scripts.extend(build_script_rows(name, info))
        for key, count in info["act_reasons"].items():
            action_reason_totals[key] = \
                action_reason_totals.get(key, 0) + count
        for key, count in info["fair"]["by"].items():
            fairness_reason_totals[key] = \
                fairness_reason_totals.get(key, 0) + count
        for event, count in info["event_counts"].items():
            totals[event] = totals.get(event, 0) + count
        ignored += info["ignored_selftest"]
        warnings.extend(info["warnings"])

    if len(actions) > 1:
        actions.append(build_action_total(actions))

    action_reasons = [
        {"type": key[0], "reason": key[1], "class": key[2], "count": count}
        for key, count in sorted(
            action_reason_totals.items(),
            key=lambda item: (-item[1], item[0][0], item[0][1], item[0][2]))
    ]
    fairness_reasons = [
        {"type": key[0], "rule": key[1], "reason": key[2], "count": count}
        for key, count in sorted(
            fairness_reason_totals.items(),
            key=lambda item: (-item[1], item[0][0], item[0][1], item[0][2]))
    ]

    report = {
        "files": files,
        "matches": matches,
        "perf": perf,
        "actions": actions,
        "action_reasons": action_reasons,
        "fairness": fairness,
        "fairness_reasons": fairness_reasons,
        "scripts": scripts,
        "events": totals,
        "ignored_selftest": ignored,
        "warnings": warnings,
    }
    return infos, report


def format_cell(value):
    if value is None:
        return "-"
    if isinstance(value, list):
        return ",".join(str(item) for item in value)
    return str(value)


def format_composition(value):
    if not isinstance(value, list) or not value:
        return "-"
    return ",".join(str(item) for item in value)


def format_reasons(value):
    if not value:
        return "-"
    return ",".join(str(item) for item in value)


def format_duration(value):
    if value is None:
        return "-"
    return "%.1f" % float(value)


def render_markdown(report):
    out = []
    out.append("# Bot telemetry report")
    out.append("")

    out.append("## Files")
    out.append("| file | match | mode | lines | bad_lines | events |")
    out.append("|---|---|---|---|---|---|")
    if report["files"]:
        for row in report["files"]:
            out.append("| %s | %s | %s | %s | %s | %s |"
                       % (format_cell(row["file"]), format_cell(row["match"]),
                          format_cell(row["mode"]), format_cell(row["lines"]),
                          format_cell(row["bad_lines"]),
                          format_cell(row["events"])))
    else:
        out.append("(none)")
    out.append("")

    out.append("## Matches")
    out.append(MATCH_TABLE_HEAD)
    out.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    if report["matches"]:
        for row in report["matches"]:
            out.append(
                "| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |"
                % (format_cell(row["match"]), format_cell(row["scenario"]),
                   format_cell(row["seed"]), format_cell(row["run"]),
                   format_cell(row["mode"]), format_duration(row["duration_s"]),
                   format_cell(row["result"]),
                   format_composition(row["composition"]),
                   format_cell(row["in_game"]),
                   format_cell(row["perf_samples"]),
                   format_cell(row["dropped_soft"]),
                   format_cell(row["dropped_hard"]),
                   format_cell(row["status"]),
                   format_reasons(row["reasons"])))
    else:
        out.append("(none)")
    out.append("")

    out.append("## MET-PERF-02 (BotManager tick)")
    out.append(PERF_TABLE_HEAD)
    out.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    if report["perf"]:
        for row in report["perf"]:
            out.append(
                "| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |"
                % (format_cell(row["file"]), format_cell(row["windows"]),
                   format_cell(row["ticks"]), format_cell(row["p50_med_us"]),
                   format_cell(row["p95_est_us"]),
                   format_cell(row["p95_worst_us"]),
                   format_cell(row["p99_worst_us"]), format_cell(row["max_us"]),
                   format_cell(row["in_game_max"]),
                   format_cell(row["skipped_max"]),
                   format_cell(row["dropped_soft_cum"]),
                   format_cell(row["dropped_hard_cum"]),
                   format_cell(row["budget_us"]), format_cell(row["verdict"])))
    else:
        out.append("(none)")
    out.append("")
    out.append(VERDICT_NOTE)
    out.append("")

    out.append("## MET-ACT-02 (invalid actions)")
    out.append("| file | submit | result | ok | invalid | no_result | other | "
               "invalid_pct | verdict |")
    out.append("|---|---|---|---|---|---|---|---|---|")
    if report["actions"]:
        for row in report["actions"]:
            out.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s |"
                       % (format_cell(row["file"]), format_cell(row["submit"]),
                          format_cell(row["result"]), format_cell(row["ok"]),
                          format_cell(row["invalid"]),
                          format_cell(row["no_result"]),
                          format_cell(row["other"]),
                          format_cell(row["invalid_pct"]),
                          format_cell(row["verdict"])))
    else:
        out.append("(none)")
    out.append("")
    out.append(ACT_NOTE)
    out.append("")

    out.append("## Action failures by reason")
    out.append("| type | reason | class | count |")
    out.append("|---|---|---|---|")
    if report["action_reasons"]:
        for row in report["action_reasons"]:
            out.append("| %s | %s | %s | %s |"
                       % (format_cell(row["type"]),
                          format_cell(row["reason"]),
                          format_cell(row["class"]),
                          format_cell(row["count"])))
    else:
        out.append("(none)")
    out.append("")

    out.append("## MET-FAIR-01 (fairness rejects)")
    out.append("| file | rejects | bots | span_s | bot_hours_est | "
               "rejects_per_bot_hour_est |")
    out.append("|---|---|---|---|---|---|")
    if report["fairness"]:
        for row in report["fairness"]:
            out.append("| %s | %s | %s | %s | %s | %s |"
                       % (format_cell(row["file"]),
                          format_cell(row["rejects"]),
                          format_cell(row["bots"]), format_cell(row["span_s"]),
                          format_cell(row["bot_hours_est"]),
                          format_cell(row["rejects_per_bot_hour_est"])))
    else:
        out.append("(none)")
    out.append("")
    out.append(FAIR_NOTE)
    out.append("")

    out.append("## Fairness rejects by rule")
    out.append("| type | rule | reason | count |")
    out.append("|---|---|---|---|")
    if report["fairness_reasons"]:
        for row in report["fairness_reasons"]:
            out.append("| %s | %s | %s | %s |"
                       % (format_cell(row["type"]), format_cell(row["rule"]),
                          format_cell(row["reason"]),
                          format_cell(row["count"])))
    else:
        out.append("(none)")
    out.append("")

    out.append("## Scripts")
    out.append("| file | script | steps_total | steps_run | result | "
               "elapsed_ms | max_late_ms | submit | invalid | no_result | "
               "rejects |")
    out.append("|---|---|---|---|---|---|---|---|---|---|---|")
    if report["scripts"]:
        for row in report["scripts"]:
            out.append(
                "| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |"
                % (format_cell(row["file"]), format_cell(row["script"]),
                   format_cell(row["steps_total"]),
                   format_cell(row["steps_run"]),
                   format_cell(row["result"]),
                   format_cell(row["elapsed_ms"]),
                   format_cell(row["max_late_ms"]),
                   format_cell(row["submit"]), format_cell(row["invalid"]),
                   format_cell(row["no_result"]),
                   format_cell(row["rejects"])))
    else:
        out.append("(none)")
    out.append("")
    out.append(SCRIPT_NOTE)
    out.append("")

    out.append("## Events")
    out.append("| ev | count |")
    out.append("|---|---|")
    events = report["events"]
    ordered = sorted(events.items(), key=lambda item: (-item[1], item[0]))
    if ordered:
        for event, count in ordered:
            out.append("| %s | %s |" % (format_cell(event),
                                        format_cell(count)))
    else:
        out.append("(none)")
    out.append("")
    out.append("ignored SELFTEST lines: %d" % report["ignored_selftest"])
    out.append("")

    out.append("## Warnings")
    if report["warnings"]:
        for warning in report["warnings"]:
            out.append("- %s" % warning)
    else:
        out.append("(none)")
    out.append("")
    return "\n".join(out)


def render_json(report):
    return json.dumps(report, sort_keys=True, indent=2) + "\n"


def has_violation(report):
    for match in report["matches"]:
        if match["status"] == "INVALID":
            return True
    for perf in report["perf"]:
        if perf["verdict"] == "FAIL":
            return True
    for action in report["actions"]:
        if action["verdict"] == "FAIL":
            return True
    return False


def make_record(event, **fields):
    record = {"ev": event}
    record.update(fields)
    return record


def make_window(tick_n, p50, p95, p99, max_us, in_game,
                skipped=0, dropped_soft=0, dropped_hard=0):
    return {
        "tick_n": tick_n,
        "tick_p50_us": p50,
        "tick_p95_us": p95,
        "tick_p99_us": p99,
        "tick_max_us": max_us,
        "in_game": in_game,
        "skipped_ticks": skipped,
        "dropped_soft": dropped_soft,
        "dropped_hard": dropped_hard,
    }


def write_jsonl(path, records):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        for record in records:
            handle.write(json.dumps(record) + "\n")


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def run_selftest():
    with tempfile.TemporaryDirectory() as tmp:
        # Case 1: a valid match with a matching summary.json.
        path = os.path.join(tmp, "t1-7-1.jsonl")
        records = [
            make_record("MATCH_START", t=0, match="t1-7-1", mode="live",
                        scenario="t1", seed=7, run=1,
                        composition=["BotWP_K", "BotMF_K", "BotWP_E",
                                     "BotPHD_E"], in_game=4),
            make_record("PERF_SAMPLE", t=100, match="t1-7-1",
                        **make_window(10, 1, 100, 200, 250, 4)),
            make_record("PERF_SAMPLE", t=200, match="t1-7-1",
                        **make_window(10, 2, 200, 300, 350, 4)),
            make_record("PERF_SAMPLE", t=300, match="t1-7-1",
                        **make_window(20, 3, 300, 400, 450, 4)),
            make_record("MATCH_END", t=400, match="t1-7-1", mode="live",
                        duration_ms=15364, result="ok", dropped_soft=0,
                        dropped_hard=0, in_game=4, perf_samples=3),
        ]
        write_jsonl(path, records)
        summary = {
            "match": "t1-7-1", "mode": "live", "file": "t1-7-1.jsonl",
            "start": records[0], "end": records[-1], "lines": 5,
            "events": {"MATCH_START": 1, "PERF_SAMPLE": 3, "MATCH_END": 1},
        }
        write_text(os.path.join(tmp, "t1-7-1.summary.json"),
                   json.dumps(summary) + "\n")

        infos, report = gather([path])
        match = report["matches"][0]
        assert match["status"] == "VALID", match
        assert match["duration_s"] == 15.364, match
        assert match["perf_samples"] == 3, match
        assert report["warnings"] == [], report["warnings"]
        perf = report["perf"][0]
        assert perf["windows"] == 3, perf
        assert perf["ticks"] == 40, perf
        assert perf["p50_med_us"] == 2, perf
        assert perf["p95_est_us"] == 225, perf
        assert perf["p95_worst_us"] == 300, perf
        assert perf["p99_worst_us"] == 400, perf
        assert perf["max_us"] == 450, perf
        assert perf["in_game_max"] == 4, perf
        assert perf["verdict"] == "OK", perf
        assert "| 15.4 |" in render_markdown(report), render_markdown(report)

        # Case 2: every invalid reason on one row, plus NO_END separately.
        bad = os.path.join(tmp, "bad.jsonl")
        bad_records = [
            make_record("MATCH_START", t=0, match="bad-1-1", mode="eval",
                        scenario="bad", seed=1, run=1,
                        composition=["a", "b", "c", "d", "e"], in_game=4),
            make_record("TEST_TELEPORT", t=50, match="bad-1-1"),
            make_record("MATCH_END", t=100, match="bad-1-1", mode="eval",
                        duration_ms=1000, result="aborted", dropped_hard=1),
        ]
        write_text(bad, "not json at all\n" + "\n".join(
            json.dumps(record) for record in bad_records) + "\n")
        bad_infos, bad_report = gather([bad])
        bad_match = [row for row in bad_report["matches"]
                     if row["match"] == "bad-1-1"][0]
        assert bad_match["reasons"] == [
            "ABORTED", "SPAWN_SHORT", "DROPPED_HARD", "TELEPORT_IN_EVAL",
            "BAD_LINES",
        ], bad_match
        assert bad_match["status"] == "INVALID", bad_match
        assert bad_infos[0]["first_bad_line"] == 1, bad_infos[0]
        assert bad_report["perf"][0]["verdict"] == "NO_DATA", bad_report["perf"]

        no_end = os.path.join(tmp, "noend.jsonl")
        write_jsonl(no_end, [
            make_record("MATCH_START", t=0, match="n-1-1", mode="live",
                        scenario="n", seed=1, run=1,
                        composition=["a", "b"], in_game=2),
        ])
        _infos, no_end_report = gather([no_end])
        assert no_end_report["matches"][0]["reasons"] == ["NO_END"], \
            no_end_report["matches"][0]

        # Case 3: MET-PERF-02 budget boundaries.
        ok = build_perf_row("ok", [make_window(1, 0, 5000, 5000, 5000, 16)])
        assert ok["budget_us"] == 5000 and ok["verdict"] == "OK", ok
        fail = build_perf_row("fail", [make_window(1, 0, 5001, 5001, 5001, 16)])
        assert fail["budget_us"] == 5000 and fail["verdict"] == "FAIL", fail
        wide = build_perf_row("wide", [make_window(1, 0, 5001, 5001, 5001, 17)])
        assert wide["budget_us"] == 15000 and wide["verdict"] == "OK", wide
        none = build_perf_row("none", [make_window(1, 0, 1, 1, 1, 65)])
        assert none["budget_us"] is None and none["verdict"] == "NO_BUDGET", none
        nodata = build_perf_row("nodata", [])
        assert nodata["verdict"] == "NO_DATA", nodata

        # Case 4: SELFTEST ignored, unknown event counted, no crash.
        events_path = os.path.join(tmp, "events.jsonl")
        event_records = [
            make_record("SELFTEST", t=0, i=0),
            make_record("DECISION", t=1, match="-"),
            make_record("DECISION", t=2, match="-"),
            make_record("SELFTEST", t=3, i=1),
            make_record("SELFTEST", t=4, i=2),
            make_record("PERF_SAMPLE", t=5, match="-",
                        **make_window(1, 0, 10, 10, 10, 0)),
        ]
        write_jsonl(events_path, event_records)
        _infos, events_report = gather([events_path])
        assert events_report["ignored_selftest"] == 3, events_report
        assert events_report["events"] == {"DECISION": 2, "PERF_SAMPLE": 1}, \
            events_report["events"]
        assert "| SELFTEST |" not in render_markdown(events_report)
        assert "ignored SELFTEST lines: 3" in render_markdown(events_report)

        # Case 5: summary.json cross-check, deliberate mismatch.
        mismatch = os.path.join(tmp, "mm.jsonl")
        mismatch_records = [
            make_record("MATCH_START", t=0, match="mm-1-1", mode="live",
                        scenario="mm", seed=1, run=1, composition=["a"],
                        in_game=1),
            make_record("MATCH_END", t=1, match="mm-1-1", mode="live",
                        duration_ms=500, result="completed", dropped_soft=0,
                        dropped_hard=0, in_game=1, perf_samples=0),
        ]
        write_jsonl(mismatch, mismatch_records)
        bad_summary = dict(summary)
        bad_summary["lines"] = 99
        write_text(os.path.join(tmp, "mm.summary.json"),
                   json.dumps(bad_summary) + "\n")
        _infos, mismatch_report = gather([mismatch])
        assert any("summary mismatch" in item
                   for item in mismatch_report["warnings"]), \
            mismatch_report["warnings"]

        # Case 6: recursive scan, summary not an input, determinism, JSON parse.
        nested = os.path.join(tmp, "nested")
        os.makedirs(os.path.join(nested, "a"))
        os.makedirs(os.path.join(nested, "b"))
        write_jsonl(os.path.join(nested, "a", "t1.jsonl"), [records[0]])
        write_jsonl(os.path.join(nested, "b", "t1.jsonl"), [records[0]])
        write_text(os.path.join(nested, "a", "t1.summary.json"), "{}\n")
        collected = collect_paths([nested])
        bases = [os.path.basename(item) for item in collected]
        assert bases == ["t1.jsonl", "t1.jsonl"], collected
        assert collected == sorted(collected), collected
        names = display_names(collected)
        assert names[collected[0]] == "a/t1.jsonl", names
        assert names[collected[1]] == "b/t1.jsonl", names
        _infos_a, report_a = gather(collected)
        _infos_b, report_b = gather(collected)
        assert render_markdown(report_a) == render_markdown(report_b)
        assert render_json(report_a) == render_json(report_b)
        parsed = json.loads(render_json(report_a))
        assert isinstance(parsed, dict) and "perf" in parsed, parsed

        # Case 7: a synthetic scripted run (MET-ACT-02, MET-FAIR-01, Scripts).
        script_path = os.path.join(tmp, "f4-21-sample.jsonl")
        script_records = [
            make_record("SCRIPT_START", t=1000, script="s1", steps=2,
                        duration_ms=5000, bot=-1),
            make_record("SCRIPT_STEP", t=1010, script="s1", step=1, line=3,
                        offset_ms=0, late_ms=10, verb="move", bot=-1),
            make_record("ACTION_SUBMIT", t=1011, bot=0, type="Move"),
            make_record("ACTION_RESULT", t=1012, bot=0, type="Move", ok=True,
                        reason="ok"),
            make_record("ACTION_SUBMIT", t=1013, bot=1, type="Attack"),
            make_record("ACTION_RESULT", t=1014, bot=1, type="Attack", ok=True,
                        reason="hit"),
            make_record("SCRIPT_STEP", t=2010, script="s1", step=2, line=4,
                        offset_ms=1000, late_ms=10, verb="cast", bot=-1),
            make_record("ACTION_SUBMIT", t=2011, bot=0, type="CastEffect"),
            make_record("ACTION_RESULT", t=2012, bot=0, type="CastEffect",
                        ok=False, reason="srv_fail"),
            make_record("ACTION_SUBMIT", t=2013, bot=0, type="UsePotion"),
            make_record("ACTION_RESULT", t=2014, bot=0, type="UsePotion",
                        ok=False, reason="no_result"),
            make_record("FAIRNESS_REJECT", t=2015, bot=1, type="Attack",
                        rule="CLI-01", reason="too_soon", value=100.0,
                        limit=1000.0),
            make_record("FAIRNESS_REJECT", t=2016, bot=1, type="Attack",
                        rule="CLI-01", reason="too_soon", value=100.0,
                        limit=1000.0),
            make_record("SCRIPT_END", t=3000, script="s1", result="completed",
                        steps_run=2, steps_total=2, elapsed_ms=2000,
                        max_late_ms=10, bot=-1),
        ]
        write_jsonl(script_path, script_records)
        _infos7, report7 = gather([script_path])
        assert len(report7["actions"]) == 1, report7["actions"]
        act = report7["actions"][0]
        assert act["submit"] == 4 and act["result"] == 4, act
        assert act["ok"] == 2 and act["invalid"] == 1, act
        assert act["no_result"] == 1 and act["other"] == 0, act
        assert act["invalid_pct"] == 25.0, act
        assert act["verdict"] == "FAIL", act
        assert has_violation(report7) is True, report7
        assert len(report7["fairness"]) == 1, report7["fairness"]
        fair = report7["fairness"][0]
        assert fair["rejects"] == 2, fair
        assert fair["bots"] == 2, fair
        assert fair["span_s"] == 2.0, fair
        assert abs(fair["bot_hours_est"] - 0.0011) < 1e-9, fair
        assert abs(fair["rejects_per_bot_hour_est"] - 1800.0) < 0.5, fair
        assert report7["fairness_reasons"] == [
            {"type": "Attack", "rule": "CLI-01", "reason": "too_soon",
             "count": 2}], report7["fairness_reasons"]
        assert report7["action_reasons"] == [
            {"type": "CastEffect", "reason": "srv_fail", "class": "invalid",
             "count": 1},
            {"type": "UsePotion", "reason": "no_result", "class": "no_result",
             "count": 1}], report7["action_reasons"]
        assert len(report7["scripts"]) == 1, report7["scripts"]
        run7 = report7["scripts"][0]
        assert run7["script"] == "s1", run7
        assert run7["steps_total"] == 2 and run7["steps_run"] == 2, run7
        assert run7["result"] == "completed", run7
        assert run7["submit"] == 4 and run7["invalid"] == 1, run7
        assert run7["no_result"] == 1 and run7["rejects"] == 2, run7
        assert run7["max_late_ms"] == 10, run7
        assert report7["warnings"] == [], report7["warnings"]
        markdown7 = render_markdown(report7)
        assert "## MET-ACT-02" in markdown7, markdown7
        assert "## MET-FAIR-01" in markdown7, markdown7
        assert "## Scripts" in markdown7, markdown7
        assert "FAIL" in markdown7, markdown7

        # Case 8: classification and verdict boundaries.
        assert classify_action_result(True, "hit") == "ok"
        assert classify_action_result(False, "srv_fail") == "invalid"
        assert classify_action_result(False, "handler_noop") == "invalid"
        assert classify_action_result(False, "refused_level") == "invalid"
        assert classify_action_result(False, "no_result") == "no_result"
        assert classify_action_result(False, "weird") == "other"
        assert classify_action_result("yes", "ok") == "other"
        assert classify_action_result(None, "ok") == "other"
        assert act_verdict(0, 0) == "NO_DATA"
        assert act_verdict(200, 2) == "PASS"
        assert act_verdict(200, 3) == "WARN"
        assert act_verdict(200, 4) == "WARN"
        assert act_verdict(200, 5) == "FAIL"
        assert act_verdict(100, 1) == "PASS"
        assert act_verdict(100, 2) == "WARN"
        assert act_verdict(100, 3) == "FAIL"

        # Case 9: missing SCRIPT_END, warnings, empty sections, determinism.
        open_path = os.path.join(tmp, "open.jsonl")
        write_jsonl(open_path, [
            make_record("SCRIPT_START", t=0, script="s2", steps=3,
                        duration_ms=1000, bot=-1),
            make_record("SCRIPT_STEP", t=10, script="s2", step=1, line=2,
                        offset_ms=0, late_ms=0, verb="move", bot=-1),
            make_record("ACTION_SUBMIT", t=11, bot=2, type="Move"),
            make_record("ACTION_RESULT", t=12, bot=2, type="Move", ok=True,
                        reason="ok"),
            make_record("ACTION_SUBMIT", t=13, bot=2, type="Move"),
        ])
        _infos9, report9 = gather([open_path])
        assert report9["scripts"][0]["result"] == "NO_END", report9["scripts"]
        assert report9["scripts"][0]["steps_run"] == 1, report9["scripts"]
        assert any("ACTION_SUBMIT/ACTION_RESULT differ for Move (2 vs 1)"
                   in item for item in report9["warnings"]), \
            report9["warnings"]

        plain_path = os.path.join(tmp, "plain.jsonl")
        write_jsonl(plain_path, [
            make_record("MATCH_START", t=0, match="p-1-1", mode="live",
                        scenario="p", seed=1, run=1, composition=["a"],
                        in_game=1),
            make_record("PERF_SAMPLE", t=100, match="p-1-1",
                        **make_window(1, 0, 10, 10, 10, 1)),
            make_record("MATCH_END", t=200, match="p-1-1", mode="live",
                        duration_ms=200, result="completed", dropped_soft=0,
                        dropped_hard=0, in_game=1, perf_samples=1),
        ])
        _infos_all, combined = gather([script_path, open_path, plain_path])
        assert any(row["file"] == "(total)" for row in combined["actions"]), \
            combined["actions"]
        _infos_plain, plain_report = gather([plain_path])
        assert plain_report["actions"] == [], plain_report["actions"]
        plain_markdown = render_markdown(plain_report)
        assert "## MET-ACT-02" in plain_markdown, plain_markdown
        assert "(none)" in plain_markdown, plain_markdown
        _infos_c1, combined1 = gather([script_path, open_path, plain_path])
        _infos_c2, combined2 = gather([script_path, open_path, plain_path])
        assert render_markdown(combined1) == render_markdown(combined2)
        assert render_json(combined1) == render_json(combined2)
        parsed9 = json.loads(render_json(combined1))
        assert "actions" in parsed9 and "fairness" in parsed9, parsed9
        assert "scripts" in parsed9, parsed9

    print("selftest OK")
    return 0


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]
    if "--selftest" in argv:
        return run_selftest()

    use_json = False
    strict = False
    out_path = None
    paths = []
    index = 0
    while index < len(argv):
        arg = argv[index]
        if arg == "--json":
            use_json = True
            index += 1
        elif arg == "--strict":
            strict = True
            index += 1
        elif arg == "--out":
            if index + 1 >= len(argv):
                sys.stderr.write("error: --out needs a value\n")
                sys.stderr.write(USAGE)
                return 2
            out_path = argv[index + 1]
            index += 2
        elif arg.startswith("--"):
            sys.stderr.write("unknown argument: %s\n" % arg)
            sys.stderr.write(USAGE)
            return 2
        else:
            paths.append(arg)
            index += 1

    if not paths:
        sys.stderr.write(USAGE)
        return 2

    try:
        resolved = collect_paths(paths)
    except OSError as exc:
        sys.stderr.write("error: cannot read path: %s\n" % exc)
        return 2
    if not resolved:
        sys.stderr.write("error: no .jsonl files found\n")
        return 2

    try:
        _infos, report = gather(resolved)
    except OSError as exc:
        sys.stderr.write("error: cannot read file: %s\n" % exc)
        return 2

    text = render_json(report) if use_json else render_markdown(report)
    if out_path is not None:
        try:
            with open(out_path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(text)
        except OSError as exc:
            sys.stderr.write("error: cannot write output: %s\n" % exc)
            return 2
    else:
        sys.stdout.write(text)

    if strict and has_violation(report):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
