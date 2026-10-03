#!/usr/bin/env python3
"""Evaluates match outcomes from GameServer bot telemetry (JSONL).

Implements the win rules of ADR-0031-DEG (killdiff_timed, wipe_first,
timed_score) as an independent oracle over the documented input contract
(docs/16 section 3.3, ADR-0031-DEG Ek F8-01).  The tool only reads files
(CLI mode writes nothing) and never talks to the server or the database.

Input lines are one JSON object each; every line must carry t (int, ms),
match (str) and ev (str).  Lines with match "-" and ev "SELFTEST" are
skipped.  Used events: MATCH_START (team_a, team_b, optional win_rule,
duration_sec, win_margin, early_end_margin, engage_timeout_sec), DAMAGE
(t only; first record = engage), DEATH (bot = dead, killer = killer id),
RESPAWN (bot), TEST_TELEPORT / SETUP_FAIL (presence), MATCH_END (t,
result).  Unknown events and extra fields are ignored.

Usage:
    python3 tools/bot-outcome-eval.py PATH [PATH ...] [--win-rule R]
        [--duration-sec N] [--win-margin N] [--early-end-margin N]
        [--engage-timeout-sec N] [--tick-ms N] [--json] [--strict]
    python3 tools/bot-outcome-eval.py --selftest

PATH is a .jsonl file or a folder (scanned recursively for *.jsonl).
Exit codes: 0 complete, 1 --strict and at least one invalid, 2 usage
error / unreadable path / malformed line / ERROR match (2 beats 1).
"""

import json
import os
import sys
import tempfile

USAGE = (
    "Usage:\n"
    "  python3 tools/bot-outcome-eval.py PATH [PATH ...] [--win-rule R]\n"
    "      [--duration-sec N] [--win-margin N] [--early-end-margin N]\n"
    "      [--engage-timeout-sec N] [--tick-ms N] [--json] [--strict]\n"
    "  python3 tools/bot-outcome-eval.py --selftest\n"
    "Options:\n"
    "  --win-rule R          killdiff_timed | wipe_first | timed_score\n"
    "  --win-margin N        integer 1..8\n"
    "  --early-end-margin N  integer 0 (off) or >= win_margin\n"
    "  --engage-timeout-sec N integer 1..3600\n"
    "  --duration-sec N      integer 1..3600\n"
    "  --tick-ms N           integer 1..1000 (same-tick window, default 100)\n"
    "  --json                write one JSON object instead of OUTCOME lines\n"
    "  --strict              exit 1 when at least one match is invalid\n"
)

WIN_RULES = ("killdiff_timed", "wipe_first", "timed_score")
REASON_ORDER = (
    "SETUP_FAIL", "ABORTED", "TEST_TELEPORT", "NO_ENGAGE",
    "RESPAWN_IN_WIPE_FIRST", "TRUNCATED",
)
RESULT_CODES = ("win_a", "win_b", "draw", "invalid", "no_result")

DEFAULT_WIN_RULE = "killdiff_timed"
DEFAULT_WIN_MARGIN = 2
DEFAULT_EARLY_END_MARGIN = 0
DEFAULT_ENGAGE_TIMEOUT_SEC = 60
DEFAULT_TICK_MS = 100

PARAM_OPTIONS = {
    "--win-rule": "win_rule",
    "--duration-sec": "duration_sec",
    "--win-margin": "win_margin",
    "--early-end-margin": "early_end_margin",
    "--engage-timeout-sec": "engage_timeout_sec",
    "--tick-ms": "tick_ms",
}


def is_int(value):
    return isinstance(value, int) and not isinstance(value, bool)


def is_int_list(value):
    if not isinstance(value, list) or not value:
        return False
    for item in value:
        if not is_int(item):
            return False
    return True


def default_options():
    return {
        "win_rule": None,
        "duration_sec": None,
        "win_margin": None,
        "early_end_margin": None,
        "engage_timeout_sec": None,
        "tick_ms": DEFAULT_TICK_MS,
        "json": False,
        "strict": False,
    }


def parse_cli_int(raw):
    try:
        return int(raw, 10)
    except (TypeError, ValueError):
        return None


def parse_args(argv):
    """Returns (options, paths, error_message). error_message set on usage."""
    opts = default_options()
    paths = []
    index = 0
    while index < len(argv):
        arg = argv[index]
        if arg == "--json":
            opts["json"] = True
            index += 1
        elif arg == "--strict":
            opts["strict"] = True
            index += 1
        elif arg in PARAM_OPTIONS:
            if index + 1 >= len(argv):
                return None, None, "option %s needs a value" % arg
            raw = argv[index + 1]
            value = parse_cli_int(raw)
            if value is None:
                return None, None, "option %s needs an integer" % arg
            opts[PARAM_OPTIONS[arg]] = value
            index += 2
        elif arg.startswith("--"):
            return None, None, "unknown argument: %s" % arg
        else:
            paths.append(arg)
            index += 1

    if opts["win_rule"] is not None and opts["win_rule"] not in WIN_RULES:
        return None, None, "invalid --win-rule: %s" % opts["win_rule"]
    checks = (
        ("win_margin", 1, 8),
        ("duration_sec", 1, 3600),
        ("engage_timeout_sec", 1, 3600),
        ("tick_ms", 1, 1000),
    )
    for name, low, high in checks:
        value = opts[name]
        if value is not None and not (low <= value <= high):
            return None, None, "%s out of range" % name
    if opts["early_end_margin"] is not None and opts["early_end_margin"] < 0:
        return None, None, "early_end_margin out of range"
    return opts, paths, None


def collect_paths(paths):
    """Resolves files/folders to a sorted, de-duplicated .jsonl list."""
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


def parse_file(path, errors):
    """Reads one .jsonl file; malformed lines go to errors, never raises."""
    records = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for lineno, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            try:
                record = json.loads(raw)
            except ValueError:
                errors.append((path, lineno, "bad JSON"))
                continue
            if not isinstance(record, dict):
                errors.append((path, lineno, "not a JSON object"))
                continue
            event = record.get("ev")
            if not isinstance(event, str):
                errors.append((path, lineno, "missing or invalid ev"))
                continue
            if event == "SELFTEST":
                continue
            match = record.get("match")
            if not isinstance(match, str):
                errors.append((path, lineno, "missing or invalid match"))
                continue
            if match == "-":
                continue
            t_value = record.get("t")
            if not is_int(t_value):
                errors.append((path, lineno, "missing or invalid t"))
                continue
            records.append({
                "match": match,
                "t": t_value,
                "ev": event,
                "rec": record,
            })
    return records


def group_records(records):
    """Groups by match id (first-seen order); stable sort by t inside."""
    order = []
    groups = {}
    for record in records:
        match = record["match"]
        if match not in groups:
            groups[match] = []
            order.append(match)
        groups[match].append(record)
    for match in order:
        groups[match].sort(key=lambda item: item["t"])
    return order, groups


def resolve_rule(opts, start):
    if opts["win_rule"] is not None:
        return opts["win_rule"], None
    value = start.get("win_rule")
    if value is None:
        return DEFAULT_WIN_RULE, None
    if not isinstance(value, str) or value not in WIN_RULES:
        return None, "invalid win_rule"
    return value, None


def resolve_duration(opts, start, rule, team_a, team_b):
    if opts["duration_sec"] is not None:
        return opts["duration_sec"], None
    value = start.get("duration_sec")
    if value is not None:
        if not is_int(value) or not (1 <= value <= 3600):
            return None, "invalid duration_sec"
        return value, None
    if rule == "killdiff_timed":
        size = max(len(team_a), len(team_b))
        return (300 if size >= 6 else 120), None
    return 300, None


def resolve_win_margin(opts, start):
    if opts["win_margin"] is not None:
        return opts["win_margin"], None
    value = start.get("win_margin")
    if value is None:
        return DEFAULT_WIN_MARGIN, None
    if not is_int(value) or not (1 <= value <= 8):
        return None, "invalid win_margin"
    return value, None


def resolve_early_end_margin(opts, start):
    if opts["early_end_margin"] is not None:
        return opts["early_end_margin"], None
    value = start.get("early_end_margin")
    if value is None:
        return DEFAULT_EARLY_END_MARGIN, None
    if not is_int(value) or value < 0:
        return None, "invalid early_end_margin"
    return value, None


def resolve_engage_timeout(opts, start):
    if opts["engage_timeout_sec"] is not None:
        return opts["engage_timeout_sec"], None
    value = start.get("engage_timeout_sec")
    if value is None:
        return DEFAULT_ENGAGE_TIMEOUT_SEC, None
    if not is_int(value) or not (1 <= value <= 3600):
        return None, "invalid engage_timeout_sec"
    return value, None


def judge_killdiff(k_a, k_b, win_margin):
    fark = k_a - k_b
    if fark >= win_margin:
        return "win_a"
    if fark <= -win_margin:
        return "win_b"
    return "draw"


def kill_side(record, team_a, team_b):
    """Returns the killer's team ('a'/'b') for a valid kill, else None."""
    if record["ev"] != "DEATH":
        return None
    raw = record["rec"]
    victim = raw.get("bot")
    if not is_int(victim):
        return None
    killer = raw.get("killer")
    if not is_int(killer) or killer == -1:
        return None
    if victim in team_b and killer in team_a:
        return "a"
    if victim in team_a and killer in team_b:
        return "b"
    return None


def eval_killdiff(records, team_a, team_b, engage_t, window_end,
                  win_margin, early_end_margin, forced_no_result=False):
    """killdiff_timed and timed_score share this simulation."""
    k_a = 0
    k_b = 0
    end = "duration"
    end_t = window_end
    terminated = False
    if engage_t is not None and window_end is not None:
        for record in records:
            if record["t"] < engage_t or record["t"] > window_end:
                continue
            side = kill_side(record, team_a, team_b)
            if side is None:
                continue
            if side == "a":
                k_a += 1
            else:
                k_b += 1
            if (not forced_no_result) and early_end_margin > 0 \
                    and abs(k_a - k_b) >= early_end_margin:
                end = "early_end"
                end_t = record["t"]
                terminated = True
                break
    if forced_no_result:
        result = "no_result"
    else:
        result = judge_killdiff(k_a, k_b, win_margin)
    return {
        "k_a": k_a, "k_b": k_b, "result": result, "end": end,
        "end_t": end_t, "terminated": terminated,
        "alive_a": None, "alive_b": None,
    }


def eval_wipe(records, team_a, team_b, engage_t, window_end, tick_ms):
    """wipe_first: first team to reach zero alive loses (same-tick draw)."""
    deaths = []
    for record in records:
        if record["ev"] != "DEATH":
            continue
        if window_end is not None and record["t"] > window_end:
            continue
        victim = record["rec"].get("bot")
        if is_int(victim) and (victim in team_a or victim in team_b):
            deaths.append((record["t"], victim))

    alive_a = set(team_a)
    alive_b = set(team_b)
    is_1v1 = len(alive_a) == 1 and len(alive_b) == 1

    if is_1v1:
        if not deaths:
            end = "alive_count"
            end_t = window_end
            terminated = False
            result = "draw"
            alive_a_n = len(alive_a)
            alive_b_n = len(alive_b)
        else:
            t0 = deaths[0][0]
            first_victim = deaths[0][1]
            same_t = sum(1 for t, _ in deaths if t == t0) >= 2
            end = "wipe"
            end_t = t0
            terminated = True
            if same_t:
                result = "draw"
                alive_a_n = 0
                alive_b_n = 0
            elif first_victim in team_a:
                result = "win_b"
                alive_a_n = 0
                alive_b_n = 1
            else:
                result = "win_a"
                alive_a_n = 1
                alive_b_n = 0
    else:
        t0 = None
        for (t_value, victim) in deaths:
            if t0 is not None and t_value > t0 + tick_ms:
                break
            if victim in alive_a:
                alive_a.discard(victim)
            elif victim in alive_b:
                alive_b.discard(victim)
            if t0 is None and (len(alive_a) == 0 or len(alive_b) == 0):
                t0 = t_value
        alive_a_n = len(alive_a)
        alive_b_n = len(alive_b)
        if t0 is not None:
            end = "wipe"
            end_t = t0
            terminated = True
            if alive_a_n == 0 and alive_b_n == 0:
                result = "draw"
            elif alive_a_n == 0:
                result = "win_b"
            else:
                result = "win_a"
        else:
            end = "alive_count"
            end_t = window_end
            terminated = False
            if alive_a_n > alive_b_n:
                result = "win_a"
            elif alive_b_n > alive_a_n:
                result = "win_b"
            else:
                result = "draw"

    k_a = 0
    k_b = 0
    if engage_t is not None and end_t is not None:
        for record in records:
            if record["t"] < engage_t or record["t"] > end_t:
                continue
            side = kill_side(record, team_a, team_b)
            if side == "a":
                k_a += 1
            elif side == "b":
                k_b += 1
    return {
        "k_a": k_a, "k_b": k_b, "result": result, "end": end,
        "end_t": end_t, "terminated": terminated,
        "alive_a": alive_a_n, "alive_b": alive_b_n,
    }


def invalid_row(match_id, rule, reasons):
    return {
        "match": match_id, "rule": rule, "result": "invalid",
        "k_a": None, "k_b": None, "diff": None,
        "alive_a": None, "alive_b": None,
        "engage_ms": None, "end_ms": None, "end": None,
        "reasons": list(reasons), "error": None,
    }


def error_row(match_id, message):
    return {
        "match": match_id, "rule": None, "result": None,
        "k_a": None, "k_b": None, "diff": None,
        "alive_a": None, "alive_b": None,
        "engage_ms": None, "end_ms": None, "end": None,
        "reasons": [], "error": message,
    }


def result_row(match_id, rule, simulation, start_t, engage_t):
    diff = simulation["k_a"] - simulation["k_b"]
    return {
        "match": match_id, "rule": rule, "result": simulation["result"],
        "k_a": simulation["k_a"], "k_b": simulation["k_b"], "diff": diff,
        "alive_a": simulation["alive_a"], "alive_b": simulation["alive_b"],
        "engage_ms": (engage_t - start_t) if engage_t is not None else None,
        "end_ms": (simulation["end_t"] - start_t)
                  if simulation["end_t"] is not None else None,
        "end": simulation["end"], "reasons": [], "error": None,
    }


def evaluate_match(match_id, records, opts):
    starts = [r for r in records if r["ev"] == "MATCH_START"]
    ends = [r for r in records if r["ev"] == "MATCH_END"]
    setup_fail = any(r["ev"] == "SETUP_FAIL" for r in records)

    if setup_fail:
        rule = "-"
        if len(starts) == 1:
            value = starts[0]["rec"].get("win_rule")
            if isinstance(value, str) and value in WIN_RULES:
                rule = value
        return invalid_row(match_id, rule, ["SETUP_FAIL"])

    if not starts:
        return error_row(match_id, "no MATCH_START")
    if len(starts) > 1:
        return error_row(match_id, "multiple MATCH_START")

    start = starts[0]["rec"]
    start_t = starts[0]["t"]
    team_a = start.get("team_a")
    team_b = start.get("team_b")
    if not (is_int_list(team_a) and is_int_list(team_b)):
        return error_row(match_id, "missing teams")
    if set(team_a) & set(team_b):
        return error_row(match_id, "overlap teams")

    rule, err = resolve_rule(opts, start)
    if err:
        return error_row(match_id, err)
    duration, err = resolve_duration(opts, start, rule, team_a, team_b)
    if err:
        return error_row(match_id, err)
    win_margin, err = resolve_win_margin(opts, start)
    if err:
        return error_row(match_id, err)
    early_end_margin, err = resolve_early_end_margin(opts, start)
    if err:
        return error_row(match_id, err)
    engage_timeout, err = resolve_engage_timeout(opts, start)
    if err:
        return error_row(match_id, err)
    if early_end_margin != 0 and early_end_margin < win_margin:
        return error_row(match_id, "invalid early_end_margin")

    ta = set(team_a)
    tb = set(team_b)
    tick_ms = opts["tick_ms"]

    if ends:
        obs_end = max(r["t"] for r in ends)
    else:
        obs_end = max(r["t"] for r in records)

    engage_t = None
    for record in records:
        if record["ev"] == "DAMAGE" and record["t"] >= start_t:
            engage_t = record["t"]
            break
    window_end = engage_t + duration * 1000 if engage_t is not None else None

    if rule == "wipe_first":
        simulation = eval_wipe(records, ta, tb, engage_t, window_end, tick_ms)
    else:
        simulation = eval_killdiff(
            records, ta, tb, engage_t, window_end, win_margin,
            early_end_margin, forced_no_result=(rule == "timed_score"))

    aborted = any(r["ev"] == "MATCH_END"
                  and r["rec"].get("result") == "aborted" for r in records)
    teleport = any(r["ev"] == "TEST_TELEPORT" and r["t"] >= start_t
                   for r in records)
    if engage_t is None:
        no_engage = (obs_end - start_t) >= engage_timeout * 1000
    else:
        no_engage = (engage_t - start_t) > engage_timeout * 1000
    respawn_wipe = False
    if rule == "wipe_first" and engage_t is not None:
        for record in records:
            if record["ev"] != "RESPAWN":
                continue
            if record["t"] < engage_t or record["t"] > obs_end:
                continue
            bot = record["rec"].get("bot")
            if is_int(bot) and (bot in ta or bot in tb):
                respawn_wipe = True
                break

    truncated = False
    if not (aborted or teleport or no_engage or respawn_wipe):
        if engage_t is None:
            truncated = True
        elif not simulation["terminated"] \
                and obs_end + tick_ms < window_end:
            truncated = True

    flags = {
        "ABORTED": aborted,
        "TEST_TELEPORT": teleport,
        "NO_ENGAGE": no_engage,
        "RESPAWN_IN_WIPE_FIRST": respawn_wipe,
        "TRUNCATED": truncated,
    }
    reasons = [name for name in REASON_ORDER if flags.get(name)]
    if reasons:
        return invalid_row(match_id, rule, reasons)

    return result_row(match_id, rule, simulation, start_t, engage_t)


def evaluate(records, opts):
    order, groups = group_records(records)
    rows = []
    for match_id in order:
        rows.append(evaluate_match(match_id, groups[match_id], opts))
    return rows


def format_value(value):
    if value is None:
        return "-"
    return str(value)


def format_diff(value):
    if value is None:
        return "-"
    if value > 0:
        return "+%d" % value
    return str(value)


def render_outcome(row):
    if row["error"] is not None:
        return "OUTCOME match=%s ERROR %s" % (row["match"], row["error"])
    reasons = ",".join(row["reasons"]) if row["reasons"] else "-"
    return (
        "OUTCOME match=%s rule=%s result=%s k_a=%s k_b=%s diff=%s "
        "alive_a=%s alive_b=%s engage_ms=%s end_ms=%s end=%s reasons=%s"
        % (row["match"], format_value(row["rule"]), format_value(row["result"]),
           format_value(row["k_a"]), format_value(row["k_b"]),
           format_diff(row["diff"]), format_value(row["alive_a"]),
           format_value(row["alive_b"]), format_value(row["engage_ms"]),
           format_value(row["end_ms"]), format_value(row["end"]), reasons)
    )


def summarize(rows):
    summary = {
        "n": 0, "win_a": 0, "win_b": 0, "draw": 0,
        "invalid": 0, "no_result": 0, "error": 0,
    }
    for row in rows:
        if row["error"] is not None:
            summary["error"] += 1
            continue
        summary["n"] += 1
        if row["result"] in RESULT_CODES:
            summary[row["result"]] += 1
    return summary


def render_text(rows):
    lines = [render_outcome(row) for row in rows]
    summary = summarize(rows)
    line = ("SUMMARY n=%d win_a=%d win_b=%d draw=%d invalid=%d no_result=%d"
            % (summary["n"], summary["win_a"], summary["win_b"],
               summary["draw"], summary["invalid"], summary["no_result"]))
    if summary["error"]:
        line += " error=%d" % summary["error"]
    lines.append(line)
    return "\n".join(lines) + "\n"


def render_json(rows):
    matches = []
    for row in rows:
        matches.append({
            "match": row["match"],
            "rule": row["rule"],
            "result": row["result"],
            "k_a": row["k_a"],
            "k_b": row["k_b"],
            "diff": row["diff"],
            "alive_a": row["alive_a"],
            "alive_b": row["alive_b"],
            "engage_ms": row["engage_ms"],
            "end_ms": row["end_ms"],
            "end": row["end"],
            "reasons": row["reasons"],
            "error": row["error"],
        })
    summary = summarize(rows)
    payload = {"matches": matches, "summary": summary}
    return json.dumps(payload, sort_keys=True) + "\n"


def run(argv):
    """Parses argv, evaluates; returns (exit_code, stdout, stderr)."""
    opts, paths, err = parse_args(argv)
    if err is not None:
        return 2, "", "error: %s\n%s" % (err, USAGE)
    if not paths:
        return 2, "", USAGE

    errors = []
    records = []
    try:
        resolved = collect_paths(paths)
    except OSError as exc:
        return 2, "", "error: cannot read path: %s\n" % exc
    if not resolved:
        return 2, "", "error: no .jsonl files found\n"
    for path in resolved:
        try:
            records.extend(parse_file(path, errors))
        except OSError as exc:
            return 2, "", "error: cannot read file: %s\n" % exc

    rows = evaluate(records, opts)
    out = render_json(rows) if opts["json"] else render_text(rows)
    err_text = ""
    if errors:
        err_text = "".join(
            "ERROR %s:%d: %s\n" % (path, lineno, message)
            for path, lineno, message in errors)

    if errors or any(row["error"] is not None for row in rows):
        return 2, out, err_text
    if opts["strict"] and any(row["result"] == "invalid" for row in rows):
        return 1, out, err_text
    return 0, out, err_text


def write_records(path, records):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        for record in records:
            handle.write(json.dumps(record, sort_keys=True) + "\n")


def rec(match, t, ev, **fields):
    record = {"t": t, "match": match, "ev": ev}
    record.update(fields)
    return record


def start_record(match, team_a, team_b, **fields):
    record = rec(match, 0, "MATCH_START", team_a=team_a, team_b=team_b)
    for key in ("win_rule", "duration_sec", "win_margin", "early_end_margin",
                "engage_timeout_sec"):
        if fields.get(key) is not None:
            record[key] = fields[key]
    return record


def death(match, t, bot, killer=None):
    record = rec(match, t, "DEATH", bot=bot)
    if killer is not None:
        record["killer"] = killer
    return record


def build_records(match, team_a, team_b, deaths=(), respawns=(),
                  damage_t=1000, duration_sec=None, end_t=None,
                  end_result="completed", **fields):
    records = [start_record(match, team_a, team_b, **fields)]
    if damage_t is not None:
        records.append(rec(match, damage_t, "DAMAGE", src=team_a[0],
                           dst=team_b[0], amount=100))
    for spec in deaths:
        records.append(death(match, spec[0], spec[1],
                             spec[2] if len(spec) > 2 else None))
    for spec in respawns:
        records.append(rec(match, spec, "RESPAWN", bot=team_a[0]))
    if end_t is not None:
        records.append(rec(match, end_t, "MATCH_END", result=end_result))
    return records


def run_selftest():
    results = []

    def case(name, func):
        try:
            func()
            results.append((name, True, ""))
        except AssertionError as exc:
            results.append((name, False, str(exc) or "assertion failed"))
        except Exception as exc:  # noqa: BLE001 - report any failure
            results.append((name, False, "error: %r" % exc))

    def to_parsed(records):
        return [{"match": r["match"], "t": r["t"], "ev": r["ev"], "rec": r}
                for r in records]

    def call(records, args=()):
        opts, _paths, err = parse_args(list(args))
        if err is not None:
            raise AssertionError("parse_args failed: %s" % err)
        return evaluate(to_parsed(records), opts)

    def one(records, args=()):
        rows = call(records, args)
        if len(rows) != 1:
            raise AssertionError("expected 1 row, got %d" % len(rows))
        return rows[0]

    def expect(row, **fields):
        for key, value in fields.items():
            if row.get(key) != value:
                raise AssertionError("%s: %r != %r" % (key, row.get(key), value))

    def rc_of(records, args=()):
        opts, _paths, err = parse_args(list(args))
        if err is not None:
            raise AssertionError("parse_args failed: %s" % err)
        rows = evaluate(to_parsed(records), opts)
        if any(r["error"] is not None for r in rows):
            return 2
        if opts["strict"] and any(r["result"] == "invalid" for r in rows):
            return 1
        return 0

    # --- killdiff_timed: pure judge and event level -----------------------
    def pure_cases():
        pairs = [
            ("kd_pure_14_11", 14, 11, "win_a"),
            ("kd_pure_10_08", 10, 8, "win_a"),
            ("kd_pure_09_08", 9, 8, "draw"),
            ("kd_pure_08_08", 8, 8, "draw"),
            ("kd_pure_08_09", 8, 9, "draw"),
            ("kd_pure_07_09", 7, 9, "win_b"),
            ("kd_pure_03_12", 3, 12, "win_b"),
        ]
        for name, k_a, k_b, expected in pairs:
            if judge_killdiff(k_a, k_b, 2) != expected:
                raise AssertionError("%s: judge mismatch" % name)
        if judge_killdiff(14, 11, 3) != "win_a":
            raise AssertionError("judge margin 3 (14,11)")
        if judge_killdiff(10, 8, 3) != "draw":
            raise AssertionError("judge margin 3 (10,8)")
        if judge_killdiff(7, 9, 3) != "draw":
            raise AssertionError("judge margin 3 (7,9)")
    case("kd_pure_judge_boundaries", pure_cases)

    default_a = (1, 2, 3, 4, 5, 6, 7, 8)
    default_b = (11, 12, 13, 14, 15, 16, 17, 18)

    def kd_case(name, a_kills, b_kills, expected, team_a=default_a,
                team_b=default_b, args=(), end_t=301000, **fields):
        deaths = []
        t_value = 2000
        for index in range(a_kills):
            deaths.append((t_value, team_b[index % len(team_b)], team_a[0]))
            t_value += 1000
        for index in range(b_kills):
            deaths.append((t_value, team_a[index % len(team_a)], team_b[0]))
            t_value += 1000
        records = build_records(name, list(team_a), list(team_b),
                                deaths=deaths, end_t=end_t, **fields)
        row = one(records, args)
        if row["result"] != expected:
            raise AssertionError("%s: %s != %s" % (name, row["result"],
                                                   expected))
        return row

    # ADR table (8v8, 300 s, win_margin 2): 7 rows + 0-0 cases
    case("kd_14_11", lambda: expect(
        kd_case("kd_14_11", 14, 11, "win_a"),
        result="win_a", k_a=14, k_b=11, diff=3))
    case("kd_10_08", lambda: expect(
        kd_case("kd_10_08", 10, 8, "win_a"),
        result="win_a", k_a=10, k_b=8, diff=2))
    case("kd_09_08", lambda: expect(
        kd_case("kd_09_08", 9, 8, "draw"), result="draw", diff=1))
    case("kd_08_08", lambda: expect(
        kd_case("kd_08_08", 8, 8, "draw"), result="draw", diff=0))
    case("kd_08_09", lambda: expect(
        kd_case("kd_08_09", 8, 9, "draw"), result="draw", diff=-1))

    case("kd_07_09", lambda: expect(
        kd_case("kd_07_09", 7, 9, "win_b"),
        result="win_b", k_a=7, k_b=9, diff=-2))

    case("kd_03_12", lambda: expect(
        kd_case("kd_03_12", 3, 12, "win_b"),
        result="win_b", k_a=3, k_b=12))

    def kd_00_00_damage():
        records = build_records("kd_00_00_damage", [1, 2], [3, 4],
                                end_t=121000)
        row = one(records)
        expect(row, result="draw", k_a=0, k_b=0, diff=0)
    case("kd_00_00_damage", kd_00_00_damage)

    def kd_00_00_nodamage():
        records = build_records("kd_00_00_nodamage", [1, 2], [3, 4],
                                damage_t=None, end_t=130000)
        row = one(records)
        expect(row, result="invalid", reasons=["NO_ENGAGE"])
    case("kd_00_00_nodamage", kd_00_00_nodamage)

    # win_margin variations
    case("kdm3_14_11", lambda: expect(
        kd_case("kdm3_14_11", 14, 11, "win_a", win_margin=3),
        result="win_a"))
    case("kdm3_10_08", lambda: expect(
        kd_case("kdm3_10_08", 10, 8, "draw", win_margin=3),
        result="draw"))
    case("kdm3_07_09", lambda: expect(
        kd_case("kdm3_07_09", 7, 9, "draw", win_margin=3),
        result="draw"))
    case("kdm1_09_08", lambda: expect(
        kd_case("kdm1_09_08", 9, 8, "win_a", win_margin=1),
        result="win_a"))

    # small team (2v2, default 120 s)
    def kd2v2_03_01():
        records = build_records("kd2v2_03_01", [1, 2], [3, 4],
                                deaths=[(2000, 3, 1), (3000, 4, 2),
                                        (4000, 1, 3), (5000, 3, 2)],
                                end_t=121000)
        expect(one(records), result="win_a", k_a=3, k_b=1)
    case("kd2v2_03_01", kd2v2_03_01)

    def kd2v2_02_01():
        records = build_records("kd2v2_02_01", [1, 2], [3, 4],
                                deaths=[(2000, 3, 1), (3000, 1, 3),
                                        (4000, 4, 2)],
                                end_t=121000)
        expect(one(records), result="draw", k_a=2, k_b=1)
    case("kd2v2_02_01", kd2v2_02_01)

    def kd_default_duration_2v2():
        records = build_records("kd_default_duration_2v2", [1, 2], [3, 4],
                                end_t=121000)
        row = one(records)
        expect(row, result="draw", engage_ms=1000, end_ms=121000)
    case("kd_default_duration_2v2", kd_default_duration_2v2)

    def kd_default_duration_8v8():
        a = [1, 2, 3, 4, 5, 6, 7, 8]
        b = [11, 12, 13, 14, 15, 16, 17, 18]
        records = build_records("kd_default_duration_8v8", a, b, end_t=301000)
        row = one(records)
        expect(row, result="draw", engage_ms=1000, end_ms=301000)
    case("kd_default_duration_8v8", kd_default_duration_8v8)

    # window and counting
    def win_kill_at_window_end():
        records = build_records("win_kill_at_window_end", [1, 2], [3, 4],
                                end_t=121000)
        records.append(death("win_kill_at_window_end", 121000, 3, 1))
        expect(one(records), result="draw", k_a=1, k_b=0)
    case("win_kill_at_window_end", win_kill_at_window_end)

    def win_kill_after_window_end():
        records = build_records("win_kill_after_window_end", [1, 2], [3, 4],
                                end_t=121001)
        records.append(death("win_kill_after_window_end", 121000, 3, 1))
        records.append(death("win_kill_after_window_end", 121001, 4, 2))
        expect(one(records), result="draw", k_a=1, k_b=0)
    case("win_kill_after_window_end", win_kill_after_window_end)

    def win_kill_before_engage():
        records = build_records("win_kill_before_engage", [1, 2], [3, 4],
                                end_t=121000)
        records.append(death("win_kill_before_engage", 500, 3, 1))
        expect(one(records), result="draw", k_a=0, k_b=0)
    case("win_kill_before_engage", win_kill_before_engage)

    def win_killer_missing():
        records = build_records("win_killer_missing2", [1, 2], [3, 4],
                                end_t=121000)
        records.append(rec("win_killer_missing2", 5000, "DEATH", bot=3))
        expect(one(records), result="draw", k_a=0, k_b=0)
    case("win_killer_missing", win_killer_missing)

    def win_killer_ally():
        records = build_records("win_killer_ally2", [1, 2], [3, 4],
                                end_t=121000)
        records.append(death("win_killer_ally2", 5000, 3, 4))
        expect(one(records), result="draw", k_a=0, k_b=0)
    case("win_killer_ally", win_killer_ally)

    def win_killer_monster():
        records = build_records("win_killer_monster2", [1, 2], [3, 4],
                                end_t=121000)
        records.append(death("win_killer_monster2", 5000, 3, 99))
        expect(one(records), result="draw", k_a=0, k_b=0)
    case("win_killer_monster", win_killer_monster)

    def win_killer_self():
        records = build_records("win_killer_self2", [1, 2], [3, 4],
                                end_t=121000)
        records.append(death("win_killer_self2", 5000, 3, 3))
        expect(one(records), result="draw", k_a=0, k_b=0)
    case("win_killer_self", win_killer_self)

    def win_killer_unknown():
        records = build_records("win_killer_unknown2", [1, 2], [3, 4],
                                end_t=121000)
        records.append(death("win_killer_unknown2", 5000, 3, -1))
        expect(one(records), result="draw", k_a=0, k_b=0)
    case("win_killer_unknown", win_killer_unknown)

    def win_respawn_ignored_killdiff():
        records = build_records("win_respawn_ignored_killdiff", [1, 2], [3, 4],
                                deaths=[(2000, 3, 1), (4000, 4, 2)],
                                respawns=[3000], end_t=121000)
        expect(one(records), result="win_a", k_a=2, k_b=0)
    case("win_respawn_ignored_killdiff", win_respawn_ignored_killdiff)

    # early end
    def early_end_4():
        records = build_records("early_end_4", [1, 2], [3, 4],
                                deaths=[(2000, 3, 1), (3000, 4, 2),
                                        (4000, 3, 1), (5000, 4, 2),
                                        (6000, 1, 3), (7000, 2, 4)],
                                end_t=121000, win_margin=2,
                                early_end_margin=4)
        row = one(records)
        expect(row, result="win_a", k_a=4, k_b=0, end="early_end",
               end_ms=5000)
    case("early_end_4", early_end_4)

    def early_end_off():
        records = build_records("early_end_off", [1, 2], [3, 4],
                                deaths=[(2000, 3, 1), (3000, 4, 2),
                                        (4000, 3, 1), (5000, 4, 2),
                                        (6000, 1, 3), (7000, 2, 4)],
                                end_t=121000, win_margin=2,
                                early_end_margin=0)
        row = one(records)
        expect(row, result="win_a", k_a=4, k_b=2, end="duration",
               end_ms=121000)
    case("early_end_off", early_end_off)

    def early_end_negative():
        records = build_records("early_end_negative", [1, 2], [3, 4],
                                deaths=[(2000, 1, 3), (3000, 2, 4),
                                        (4000, 1, 3), (5000, 2, 4),
                                        (6000, 3, 1), (7000, 4, 2)],
                                end_t=121000, win_margin=2,
                                early_end_margin=4)
        row = one(records)
        expect(row, result="win_b", end="early_end", end_ms=5000)
    case("early_end_negative", early_end_negative)

    def early_end_lt_win_margin_error():
        records = build_records("early_end_lt_win_margin_error", [1, 2], [3, 4],
                                deaths=[(2000, 3, 1)], end_t=121000,
                                win_margin=2, early_end_margin=1)
        row = one(records)
        if row["error"] is None:
            raise AssertionError("expected ERROR row")
        if rc_of(records) != 2:
            raise AssertionError("expected rc 2")
    case("early_end_lt_win_margin_error", early_end_lt_win_margin_error)

    # engage
    def engage_at_timeout_ok():
        records = build_records("engage_at_timeout_ok", [1, 2], [3, 4],
                                damage_t=60000, end_t=180000)
        row = one(records)
        expect(row, result="draw", engage_ms=60000, end_ms=180000)
    case("engage_at_timeout_ok", engage_at_timeout_ok)

    def engage_after_timeout_invalid():
        records = build_records("engage_after_timeout_invalid", [1, 2], [3, 4],
                                damage_t=60001, end_t=180000)
        expect(one(records), result="invalid", reasons=["NO_ENGAGE"])
    case("engage_after_timeout_invalid", engage_after_timeout_invalid)

    def engage_timeout_param():
        records = build_records("engage_timeout_param", [1, 2], [3, 4],
                                damage_t=10001, end_t=130000)
        expect(one(records, ("--engage-timeout-sec", "10")),
               result="invalid", reasons=["NO_ENGAGE"])
    case("engage_timeout_param", engage_timeout_param)

    def engage_none_short_log_truncated():
        records = build_records("engage_none_short_log_truncated", [1, 2],
                                [3, 4], damage_t=None, end_t=5000)
        expect(one(records), result="invalid", reasons=["TRUNCATED"])
    case("engage_none_short_log_truncated", engage_none_short_log_truncated)

    # wipe_first
    def wipe_b_last_dies_a3():
        records = build_records("wipe_b_last_dies_a3", [1, 2, 3], [4, 5],
                                deaths=[(2000, 4, 1), (3000, 5, 2)],
                                end_t=10000, win_rule="wipe_first")
        row = one(records)
        expect(row, result="win_a", end="wipe", end_ms=3000,
               alive_a=3, alive_b=0, k_a=2, k_b=0)
    case("wipe_b_last_dies_a3", wipe_b_last_dies_a3)

    def wipe_timeout_a4_b2():
        records = build_records("wipe_timeout_a4_b2", [1, 2, 3, 4],
                                [5, 6, 7, 8], deaths=[(2000, 7, 1),
                                                      (3000, 8, 2)],
                                end_t=301000, win_rule="wipe_first")
        row = one(records)
        expect(row, result="win_a", end="alive_count", end_ms=301000,
               alive_a=4, alive_b=2)
    case("wipe_timeout_a4_b2", wipe_timeout_a4_b2)

    def wipe_timeout_3_3():
        records = build_records("wipe_timeout_3_3", [1, 2, 3], [4, 5, 6],
                                end_t=301000, win_rule="wipe_first")
        row = one(records)
        expect(row, result="draw", end="alive_count", alive_a=3, alive_b=3)
    case("wipe_timeout_3_3", wipe_timeout_3_3)

    def wipe_same_tick_draw():
        records = build_records("wipe_same_tick_draw", [1, 2], [3, 4],
                                deaths=[(2000, 1, 3), (2500, 3, 1),
                                        (5000, 2, 4), (5100, 4, 2)],
                                end_t=10000, win_rule="wipe_first")
        row = one(records)
        expect(row, result="draw", end="wipe", end_ms=5000,
               alive_a=0, alive_b=0)
    case("wipe_same_tick_draw", wipe_same_tick_draw)

    def wipe_101ms_win():
        records = build_records("wipe_101ms_win", [1, 2], [3, 4],
                                deaths=[(2000, 1, 3), (2500, 3, 1),
                                        (5000, 2, 4), (5101, 4, 2)],
                                end_t=10000, win_rule="wipe_first")
        row = one(records)
        expect(row, result="win_b", end="wipe", end_ms=5000,
               alive_a=0, alive_b=1)
    case("wipe_101ms_win", wipe_101ms_win)

    def wipe_a_wiped_b_wins():
        records = build_records("wipe_a_wiped_b_wins", [1, 2], [3, 4],
                                deaths=[(2000, 1, 3), (3000, 2, 4)],
                                end_t=10000, win_rule="wipe_first")
        expect(one(records), result="win_b")
    case("wipe_a_wiped_b_wins", wipe_a_wiped_b_wins)

    def wipe_unknown_bot_ignored():
        records = build_records("wipe_unknown_bot_ignored", [1, 2], [3, 4],
                                deaths=[(2000, 99, 1), (3000, 98, 2)],
                                end_t=301000, win_rule="wipe_first")
        row = one(records)
        expect(row, result="draw", end="alive_count", alive_a=2, alive_b=2)
    case("wipe_unknown_bot_ignored", wipe_unknown_bot_ignored)

    def wipe_1v1_first_dies_loses():
        records = build_records("wipe_1v1_first_dies_loses", [1], [2],
                                deaths=[(2000, 1, 2)], end_t=10000,
                                win_rule="wipe_first")
        row = one(records)
        expect(row, result="win_b", end="wipe", end_ms=2000,
               alive_a=0, alive_b=1)
    case("wipe_1v1_first_dies_loses", wipe_1v1_first_dies_loses)

    def wipe_1v1_same_t_draw():
        records = build_records("wipe_1v1_same_t_draw", [1], [2],
                                deaths=[(2000, 1, 2), (2000, 2, 1)],
                                end_t=10000, win_rule="wipe_first")
        row = one(records)
        expect(row, result="draw", end="wipe", alive_a=0, alive_b=0)
    case("wipe_1v1_same_t_draw", wipe_1v1_same_t_draw)

    def wipe_respawn_invalid():
        records = build_records("wipe_respawn_invalid", [1, 2], [3, 4],
                                deaths=[(2000, 3, 1)], respawns=[5000],
                                end_t=10000, win_rule="wipe_first")
        expect(one(records), result="invalid",
               reasons=["RESPAWN_IN_WIPE_FIRST"])
    case("wipe_respawn_invalid", wipe_respawn_invalid)

    def wipe_tick_ms_param():
        records = build_records("wipe_tick_ms_param", [1, 2], [3, 4],
                                deaths=[(2000, 1, 3), (2500, 3, 1),
                                        (5000, 2, 4), (5060, 4, 2)],
                                end_t=10000, win_rule="wipe_first")
        row = one(records, ("--tick-ms", "50"))
        expect(row, result="win_b", end="wipe", alive_a=0, alive_b=1)
    case("wipe_tick_ms_param", wipe_tick_ms_param)

    # timed_score
    def ts_no_result_with_counts():
        records = build_records("ts_no_result_with_counts", [1, 2], [3, 4],
                                deaths=[(2000, 3, 1), (3000, 4, 2),
                                        (4000, 1, 3)],
                                end_t=301000, win_rule="timed_score")
        row = one(records)
        expect(row, result="no_result", k_a=2, k_b=1, end="duration")
    case("ts_no_result_with_counts", ts_no_result_with_counts)

    def ts_no_engage_invalid():
        records = build_records("ts_no_engage_invalid", [1, 2], [3, 4],
                                damage_t=None, end_t=130000,
                                win_rule="timed_score")
        expect(one(records), result="invalid", reasons=["NO_ENGAGE"])
    case("ts_no_engage_invalid", ts_no_engage_invalid)

    # invalid reasons
    def inv_setup_fail_no_start():
        records = [rec("inv_setup_fail_no_start", 0, "SETUP_FAIL")]
        row = one(records)
        expect(row, result="invalid", rule="-", reasons=["SETUP_FAIL"])
    case("inv_setup_fail_no_start", inv_setup_fail_no_start)

    def inv_test_teleport():
        records = build_records("inv_test_teleport", [1, 2], [3, 4],
                                end_t=121000)
        records.append(rec("inv_test_teleport", 500, "TEST_TELEPORT"))
        expect(one(records), result="invalid", reasons=["TEST_TELEPORT"])
    case("inv_test_teleport", inv_test_teleport)

    def inv_aborted_end():
        records = build_records("inv_aborted_end", [1, 2], [3, 4],
                                end_t=121000, end_result="aborted")
        expect(one(records), result="invalid", reasons=["ABORTED"])
    case("inv_aborted_end", inv_aborted_end)

    def inv_truncated_killdiff():
        records = build_records("inv_truncated_killdiff", [1, 2], [3, 4],
                                end_t=116000)
        expect(one(records), result="invalid", reasons=["TRUNCATED"])
    case("inv_truncated_killdiff", inv_truncated_killdiff)

    def inv_not_truncated_within_tick():
        records = build_records("inv_not_truncated_within_tick", [1, 2],
                                [3, 4], end_t=120900)
        row = one(records)
        expect(row, result="draw", end_ms=121000)
    case("inv_not_truncated_within_tick", inv_not_truncated_within_tick)

    def inv_reason_order():
        records = build_records("inv_reason_order", [1, 2], [3, 4],
                                damage_t=None, end_t=130000)
        records.append(rec("inv_reason_order", 500, "TEST_TELEPORT"))
        expect(one(records), result="invalid",
               reasons=["TEST_TELEPORT", "NO_ENGAGE"])
    case("inv_reason_order", inv_reason_order)

    with tempfile.TemporaryDirectory() as tmp:
        counter = [0]

        def run_records(records, args=()):
            counter[0] += 1
            path = os.path.join(tmp, "case%d.jsonl" % counter[0])
            write_records(path, records)
            return run(list(args) + [path])

        # --- file / CLI cases --------------------------------------------
        def cli_multi_match_order_and_summary():
            records = [
                rec("m1", 0, "MATCH_START", team_a=[1, 2], team_b=[3, 4]),
                rec("m1", 1000, "DAMAGE"),
                rec("m1", 2000, "DEATH", bot=3, killer=1),
                rec("m1", 121000, "MATCH_END", result="completed"),
                rec("-", 0, "DECISION"),
                rec("m2", 0, "MATCH_START", team_a=[1, 2], team_b=[3, 4]),
                rec("m2", 1000, "DAMAGE"),
                rec("m2", 121000, "MATCH_END", result="completed"),
                rec("x", 0, "SELFTEST", i=0),
            ]
            rc, out, _err = run_records(records)
            if rc != 0:
                raise AssertionError("rc %d" % rc)
            if out.count("OUTCOME ") != 2:
                raise AssertionError("expected 2 OUTCOME lines")
            if "SUMMARY n=2" not in out:
                raise AssertionError("summary wrong: %r" % out)
            if out.index("match=m1") > out.index("match=m2"):
                raise AssertionError("match order wrong")
        case("cli_multi_match_order_and_summary",
             cli_multi_match_order_and_summary)

        def cli_sort_by_t():
            base = build_records("s", [1, 2], [3, 4],
                                 deaths=[(2000, 3, 1)], end_t=121000)
            base.append(rec("s", 500, "DAMAGE2"))
            rc_a, out_a, _e = run_records(base)
            rc_b, out_b, _e = run_records(list(reversed(base)))
            if rc_a != 0 or rc_b != 0 or out_a != out_b:
                raise AssertionError("sort by t changed result")
        case("cli_sort_by_t", cli_sort_by_t)

        def cli_bad_json_line_rc2():
            counter[0] += 1
            path = os.path.join(tmp, "bad%d.jsonl" % counter[0])
            with open(path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write("not json\n")
                handle.write(json.dumps(
                    build_records("b", [1, 2], [3, 4], end_t=121000)[0]) + "\n")
            rc, _out, err = run([path])
            if rc != 2:
                raise AssertionError("rc %d != 2" % rc)
            if ":1: " not in err:
                raise AssertionError("missing line number: %r" % err)
        case("cli_bad_json_line_rc2", cli_bad_json_line_rc2)

        def cli_missing_teams_error_rc2():
            records = [rec("mt", 0, "MATCH_START")]
            rc, out, _err = run_records(records)
            if rc != 2 or "ERROR" not in out:
                raise AssertionError("rc %d out %r" % (rc, out))
        case("cli_missing_teams_error_rc2", cli_missing_teams_error_rc2)

        def cli_overlap_teams_error():
            records = [rec("ot", 0, "MATCH_START", team_a=[1, 2],
                           team_b=[2, 3])]
            rc, out, _err = run_records(records)
            if rc != 2 or "ERROR" not in out:
                raise AssertionError("rc %d out %r" % (rc, out))
        case("cli_overlap_teams_error", cli_overlap_teams_error)

        def cli_strict_rc1():
            records = build_records("st", [1, 2], [3, 4], damage_t=None,
                                    end_t=130000)
            rc, _out, _err = run_records(records, ("--strict",))
            if rc != 1:
                raise AssertionError("rc %d != 1" % rc)
        case("cli_strict_rc1", cli_strict_rc1)

        def cli_strict_rc0_when_valid():
            records = build_records("sv", [1, 2], [3, 4], end_t=121000)
            rc, _out, _err = run_records(records, ("--strict",))
            if rc != 0:
                raise AssertionError("rc %d != 0" % rc)
        case("cli_strict_rc0_when_valid", cli_strict_rc0_when_valid)

        def cli_json_keys():
            records = build_records("js", [1, 2], [3, 4],
                                    deaths=[(2000, 3, 1)], end_t=121000)
            rc, out, _err = run_records(records, ("--json",))
            if rc != 0:
                raise AssertionError("rc %d" % rc)
            data = json.loads(out)
            if data["summary"]["n"] != 1:
                raise AssertionError("summary.n wrong")
            if data["matches"][0]["result"] != "draw":
                raise AssertionError("matches[0].result wrong")
        case("cli_json_keys", cli_json_keys)

        def cli_param_priority():
            records = build_records("pp", [1, 2], [3, 4],
                                    deaths=[(2000, 3, 1), (3000, 4, 2),
                                            (4000, 1, 3)],
                                    end_t=121000, win_margin=2)
            rc, out, _err = run_records(records, ("--win-margin", "3"))
            if rc != 0 or "result=draw" not in out:
                raise AssertionError("CLI margin not applied: %r" % out)
        case("cli_param_priority", cli_param_priority)

        def cli_win_margin_9_rc2():
            records = build_records("wm9", [1, 2], [3, 4], end_t=121000)
            rc, _out, _err = run_records(records, ("--win-margin", "9"))
            if rc != 2:
                raise AssertionError("rc %d != 2" % rc)
        case("cli_win_margin_9_rc2", cli_win_margin_9_rc2)

        def cli_missing_path_rc2():
            missing = os.path.join(tmp, "does-not-exist.jsonl")
            rc, _out, _err = run([missing])
            if rc != 2:
                raise AssertionError("rc %d != 2" % rc)
        case("cli_missing_path_rc2", cli_missing_path_rc2)

        def cli_directory_scan():
            nested = os.path.join(tmp, "scan")
            os.makedirs(nested)
            write_records(os.path.join(nested, "a.jsonl"),
                          build_records("da", [1, 2], [3, 4], end_t=121000))
            write_records(os.path.join(nested, "b.jsonl"),
                          build_records("db", [1, 2], [3, 4], end_t=121000))
            rc, out, _err = run([nested])
            if rc != 0 or out.count("OUTCOME ") != 2:
                raise AssertionError("directory scan failed: %r" % out)
        case("cli_directory_scan", cli_directory_scan)

        def sample_file_expected():
            here = os.path.dirname(os.path.abspath(__file__))
            sample = os.path.join(here, "bot-outcome-eval", "sample.jsonl")
            if not os.path.isfile(sample):
                raise AssertionError("sample file missing")
            rc, out, _err = run([sample])
            expected = (
                "OUTCOME match=SMP-win-1-1 rule=killdiff_timed "
                "result=win_a k_a=3 k_b=1 diff=+2 alive_a=- alive_b=- "
                "engage_ms=1000 end_ms=121000 end=duration reasons=-\n"
                "OUTCOME match=SMP-draw-1-1 rule=killdiff_timed "
                "result=draw k_a=2 k_b=1 diff=+1 alive_a=- alive_b=- "
                "engage_ms=1000 end_ms=121000 end=duration reasons=-\n"
                "OUTCOME match=SMP-wipe-1-1 rule=wipe_first "
                "result=win_a k_a=2 k_b=0 diff=+2 alive_a=2 alive_b=0 "
                "engage_ms=500 end_ms=7000 end=wipe reasons=-\n"
                "OUTCOME match=SMP-noeng-1-1 rule=killdiff_timed "
                "result=invalid k_a=- k_b=- diff=- alive_a=- alive_b=- "
                "engage_ms=- end_ms=- end=- reasons=NO_ENGAGE\n"
                "SUMMARY n=4 win_a=2 win_b=0 draw=1 invalid=1 no_result=0\n"
            )
            if rc != 0:
                raise AssertionError("rc %d" % rc)
            if out != expected:
                raise AssertionError("sample output mismatch:\n%r" % out)
        case("sample_file_expected", sample_file_expected)

    failed = [item for item in results if not item[1]]
    for name, ok, detail in results:
        if ok:
            sys.stdout.write("PASS %s\n" % name)
        else:
            sys.stdout.write("FAIL %s: %s\n" % (name, detail))
    total = len(results)
    if failed:
        sys.stdout.write("SELFTEST FAIL failed=%d of %d\n"
                         % (len(failed), total))
        return 1
    sys.stdout.write("SELFTEST PASS n=%d\n" % total)
    return 0


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]
    if "--selftest" in argv:
        return run_selftest()
    code, out, err = run(argv)
    if out:
        sys.stdout.write(out)
    if err:
        sys.stderr.write(err)
    return code


if __name__ == "__main__":
    sys.exit(main())
