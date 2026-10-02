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


def is_int(value):
    return isinstance(value, int) and not isinstance(value, bool)


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
    }
    previous_t = None
    monotonic_warned = False
    perf_index = 0
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
        for event, count in info["event_counts"].items():
            totals[event] = totals.get(event, 0) + count
        ignored += info["ignored_selftest"]
        warnings.extend(info["warnings"])

    report = {
        "files": files,
        "matches": matches,
        "perf": perf,
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
