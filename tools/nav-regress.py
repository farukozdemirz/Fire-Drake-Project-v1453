#!/usr/bin/env python3
"""Persistent navigation regression evaluator (plan F5-11, nav line).

Runs tools/nav-measure.sh and evaluates its "KEY k=v ..." output against the
thresholds collected in docs/12 s11/s13 and the F5-51/53/54/56/57/58 plans,
then cross-checks the straight-step examples and the fixed vectors with the
independent tools/nav-segment-check.py oracle.

Usage:
    tools/nav-regress.sh [--n N] [--seed S] [--skip-timing] [--timing-retries K]
                         [--from-file F] [--save F] [--list] [--selftest]

Exit codes: 0 all checks passed, 1 at least one FAIL, 2 environment / usage error.
The evaluator never modifies nav_measure.cpp; it only reads its output. Only the
standard library is used, and nothing is written to the scanned tree.
"""

import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MEASURE = os.path.join(ROOT, "tools", "nav-measure.sh")
SEGMENT_CHECK = os.path.join(ROOT, "tools", "nav-segment-check.py")
NAVGRID = os.path.join(ROOT, "build", "nav", "zone71.navgrid")
GOOD_TXT = os.path.join(ROOT, "tools", "nav-regress", "good.txt")

DEFAULT_N = 3000
DEFAULT_SEED = 20261002
DEFAULT_RETRIES = 2

# Sections to run individually when --skip-timing drops the budget measurements.
NON_TIMING_SECTIONS = ("smoothing", "synthetic", "velocity", "velocity-robust",
                       "arena", "stuck", "progress")

# The three timing checks that are retried (class Z); everything else is not.
Z_IDS = ("budget.near64.query_p95", "sched.B.tick_p95", "sched.B.longest_wait")

KNOWN_KEYS = frozenset((
    "GRID", "SMOOTHING", "LINECLEAR", "STRAIGHT", "SYNTHETIC", "VELOCITY",
    "VELOCITYR", "ARENA", "BUDGET", "BUDGET_SCHED", "STUCK", "STUCK_TRUE",
    "PROGRESS", "PROGRESS_TRUE", "EXAMPLE",
))

ARENA_CASES = ("karus_respawn_to_arena", "elmorad_respawn_to_arena")
STUCK_BAD_MODEL = "tick110.8+-20+3%late250"

VECTOR_RE = re.compile(r"\((-?[\d.]+),(-?[\d.]+)\)->\((-?[\d.]+),(-?[\d.]+)\)")

# Fixed vectors from F5-58 K5 (world metres); the first three must be BLOCKED.
FIXED_VECTORS = (
    ((1566.0, 878.0, 1578.0, 866.0), "BLOCKED"),
    ((926.0, 834.0, 934.0, 826.0), "BLOCKED"),
    ((1102.0, 1034.0, 1106.0, 1046.0), "BLOCKED"),
    ((1274.0, 890.0, 1280.0, 890.0), "OK"),
)


class Line(object):
    __slots__ = ("key", "args", "kv", "raw")

    def __init__(self, key, args, kv, raw):
        self.key = key
        self.args = args
        self.kv = kv
        self.raw = raw


class Result(object):
    __slots__ = ("cid", "cls", "source", "status", "detail")

    def __init__(self, cid, cls, source, status, detail):
        self.cid = cid
        self.cls = cls
        self.source = source
        self.status = status
        self.detail = detail


def parse_output(text):
    lines = []
    for raw in text.splitlines():
        s = raw.strip()
        if not s:
            continue
        parts = s.split()
        key = parts[0]
        args = []
        kv = {}
        for tok in parts[1:]:
            if "=" in tok:
                k, v = tok.split("=", 1)
                kv[k] = v
            else:
                args.append(tok)
        lines.append(Line(key, args, kv, raw))
    return lines


def num(line, key):
    v = line.kv.get(key)
    if v is None:
        return None
    try:
        return float(v)
    except ValueError:
        return None


def fmt(value):
    if value is None:
        return "?"
    if isinstance(value, float):
        if value == int(value) and abs(value) < 1e15:
            return str(int(value))
        return ("%.6f" % value).rstrip("0").rstrip(".")
    return str(value)


def find_error(text):
    for line in text.splitlines():
        if line.startswith("ERROR "):
            return line.strip()
    return None


def ge_detail(name, value, limit):
    if value is None:
        return "%s=?" % name
    if value >= limit:
        return "%s=%s (>= %s)" % (name, fmt(value), fmt(limit))
    return "%s=%s (>= %s) SHORT by %s" % (name, fmt(value), fmt(limit), fmt(limit - value))


def le_detail(name, value, limit):
    if value is None:
        return "%s=?" % name
    if value <= limit:
        return "%s=%s (<= %s)" % (name, fmt(value), fmt(limit))
    return "%s=%s (<= %s) EXCEEDED by %s" % (name, fmt(value), fmt(limit), fmt(value - limit))


def eq_detail(name, value, limit):
    if value is None:
        return "%s=?" % name
    if value == limit:
        return "%s=%s (== %s)" % (name, fmt(value), fmt(limit))
    return "%s=%s (== %s) DIFFERS by %s" % (name, fmt(value), fmt(limit), fmt(abs(value - limit)))


def with_key(lines, key):
    return [ln for ln in lines if ln.key == key]


def select(lines, key, **want):
    out = []
    for ln in lines:
        if ln.key != key:
            continue
        if all(ln.kv.get(k) == v for k, v in want.items()):
            out.append(ln)
    return out


# ---------------------------------------------------------------------------
# Oracle: independent super-cover cross-check via tools/nav-segment-check.py.
# ---------------------------------------------------------------------------

class Oracle(object):
    def __init__(self, navgrid):
        self.navgrid = navgrid
        self.ok = os.path.isfile(navgrid)

    def check(self, vectors):
        payload = "".join("%.6f %.6f %.6f %.6f\n" % v for v in vectors)
        try:
            p = subprocess.run([sys.executable, SEGMENT_CHECK, "--navgrid", self.navgrid],
                               input=payload, capture_output=True, text=True)
        except OSError:
            return None
        if p.returncode != 0:
            return None
        return p.stdout.splitlines()

    def selftest(self):
        try:
            p = subprocess.run([sys.executable, SEGMENT_CHECK, "--selftest"],
                               capture_output=True, text=True)
        except OSError:
            return 2, ""
        return p.returncode, p.stdout


# ---------------------------------------------------------------------------
# Individual checks. Each returns (status, detail); status in PASS/FAIL/WARN/INFO.
# ---------------------------------------------------------------------------

def c_grid_main_cells(lines, ctx):
    recs = with_key(lines, "GRID")
    if not recs:
        return "FAIL", "missing line GRID"
    nv = num(recs[0], "n")
    mc = num(recs[0], "main_cells")
    if nv == 513 and mc == 88508:
        return "PASS", "n=513 main_cells=88508"
    return "FAIL", "n=%s main_cells=%s (expected n=513, main_cells=88508)" % (fmt(nv), fmt(mc))


def c_smoothing_raw_bad(lines, ctx):
    recs = with_key(lines, "SMOOTHING")
    if not recs:
        return "FAIL", "missing line SMOOTHING"
    rb = num(recs[0], "raw_bad")
    re_ = num(recs[0], "raw_edges")
    if rb == 0 and re_ is not None and re_ > 0:
        return "PASS", "raw_bad=0 raw_edges=%s" % fmt(re_)
    detail = "raw_bad=%s (== 0) raw_edges=%s (> 0)" % (fmt(rb), fmt(re_))
    if rb is not None and rb > 0:
        detail += " EXCEEDED by %s" % fmt(rb)
    return "FAIL", detail


def c_smoothing_smooth_bad(lines, ctx):
    recs = with_key(lines, "SMOOTHING")
    if not recs:
        return "FAIL", "missing line SMOOTHING"
    sb = num(recs[0], "smooth_bad")
    pw = num(recs[0], "paths_with_smooth_bad")
    ss = num(recs[0], "smooth_segments")
    if sb == 0 and pw == 0 and ss is not None and ss > 0:
        return "PASS", "smooth_bad=0 paths_with_smooth_bad=0 smooth_segments=%s" % fmt(ss)
    detail = "smooth_bad=%s (== 0) paths_with_smooth_bad=%s (== 0) smooth_segments=%s (> 0)" % (
        fmt(sb), fmt(pw), fmt(ss))
    if sb is not None and sb > 0:
        detail += " EXCEEDED by %s" % fmt(sb)
    return "FAIL", detail


def c_smoothing_chord_bad(lines, ctx):
    recs = with_key(lines, "SMOOTHING")
    if not recs:
        return "FAIL", "missing line SMOOTHING"
    cb = num(recs[0], "chord_bad")
    ch = num(recs[0], "chords")
    if cb == 0 and ch is not None and ch > 0:
        return "PASS", "chord_bad=0 chords=%s" % fmt(ch)
    detail = "chord_bad=%s (== 0) chords=%s (> 0)" % (fmt(cb), fmt(ch))
    if cb is not None and cb > 0:
        detail += " EXCEEDED by %s" % fmt(cb)
    return "FAIL", detail


def c_smoothing_coverage(lines, ctx):
    recs = with_key(lines, "SMOOTHING")
    if not recs:
        return "FAIL", "missing line SMOOTHING"
    paths = num(recs[0], "paths")
    limit = 0.9 * ctx["n"]
    if paths is not None and paths >= limit:
        return "PASS", "paths=%s (>= 0.9*n=%s)" % (fmt(paths), fmt(limit))
    return "FAIL", ge_detail("paths", paths, limit)


def c_lineclear_false_positive(lines, ctx):
    recs = with_key(lines, "LINECLEAR")
    if not recs:
        return "FAIL", "missing line LINECLEAR"
    fp = num(recs[0], "false_positive")
    cl = num(recs[0], "clear")
    if fp == 0 and cl is not None and cl > 0:
        return "PASS", "false_positive=0 clear=%s" % fmt(cl)
    detail = "false_positive=%s (== 0) clear=%s (> 0)" % (fmt(fp), fmt(cl))
    if fp is not None and fp > 0:
        detail += " EXCEEDED by %s" % fmt(fp)
    return "FAIL", detail


def c_straight_control(lines, ctx):
    recs = with_key(lines, "STRAIGHT")
    if not recs:
        return "FAIL", "missing line STRAIGHT"
    blocked = num(recs[0], "blocked")
    if blocked is not None and blocked > 0:
        return "PASS", "blocked=%s (> 0)" % fmt(blocked)
    return "FAIL", "blocked=%s (> 0): control step found no blocked chord" % fmt(blocked)


def _synthetic(lines, name):
    recs = [ln for ln in lines if ln.key == "SYNTHETIC" and name in ln.args]
    if not recs:
        return "FAIL", "missing line SYNTHETIC %s" % name
    fp = num(recs[0], "false_positive")
    tr = num(recs[0], "trials")
    nc = num(recs[0], "nav_line_clear_true")
    if fp == 0 and tr is not None and tr > 0 and nc is not None and nc > 0:
        return "PASS", "false_positive=0 trials=%s nav_line_clear_true=%s" % (fmt(tr), fmt(nc))
    detail = "false_positive=%s (== 0) trials=%s (> 0) nav_line_clear_true=%s (> 0)" % (
        fmt(fp), fmt(tr), fmt(nc))
    if fp is not None and fp > 0:
        detail += " EXCEEDED by %s" % fmt(fp)
    return "FAIL", detail


def c_synthetic_single_block(lines, ctx):
    return _synthetic(lines, "single_block")


def c_synthetic_random_clutter(lines, ctx):
    return _synthetic(lines, "random_clutter")


def c_velocity_jitter(lines, ctx):
    recs = [ln for ln in lines if ln.key == "VELOCITY" and "jitter" in ln.kv]
    if not recs:
        return "FAIL", "missing line VELOCITY jitter"
    z = num(recs[0], "zero")
    mr = num(recs[0], "max_rel_err")
    if z == 0 and mr is not None and mr <= 0.30:
        return "PASS", "zero=0 max_rel_err=%s (<= 0.30)" % fmt(mr)
    detail = "zero=%s (== 0) max_rel_err=%s (<= 0.30)" % (fmt(z), fmt(mr))
    if z is not None and z > 0:
        detail += " zero EXCEEDED by %s" % fmt(z)
    if mr is not None and mr > 0.30:
        detail += " max_rel_err EXCEEDED by %s" % fmt(mr - 0.30)
    return "FAIL", detail


def _robust(lines, scenario):
    recs = select(lines, "VELOCITYR", scenario=scenario)
    if not recs:
        return "FAIL", "missing line VELOCITYR %s" % scenario
    z = num(recs[0], "zero_pct")
    p95 = num(recs[0], "err_p95")
    mx = num(recs[0], "err_max")
    if scenario == "arrival_jitter":
        ok = z == 0 and p95 is not None and p95 <= 0.20 and mx is not None and mx <= 0.30
        detail = "zero_pct=%s (== 0) err_p95=%s (<= 0.20) err_max=%s (<= 0.30)" % (fmt(z), fmt(p95), fmt(mx))
    elif scenario == "arrival_bunching":
        ok = z is not None and z <= 1 and p95 is not None and p95 <= 0.20 and mx is not None and mx <= 0.60
        detail = "zero_pct=%s (<= 1) err_p95=%s (<= 0.20) err_max=%s (<= 0.60)" % (fmt(z), fmt(p95), fmt(mx))
    else:
        ok = z == 0 and p95 is not None and p95 <= 0.10
        detail = "zero_pct=%s (== 0) err_p95=%s (<= 0.10)" % (fmt(z), fmt(p95))
    if ok:
        return "PASS", detail
    if p95 is not None and scenario != "arrival_jitter" and p95 > (0.10 if scenario not in ("arrival_bunching",) else 0.20):
        detail += " err_p95 EXCEEDED"
    if mx is not None and scenario == "arrival_jitter" and mx > 0.30:
        detail += " err_max EXCEEDED by %s" % fmt(mx - 0.30)
    if mx is not None and scenario == "arrival_bunching" and mx > 0.60:
        detail += " err_max EXCEEDED by %s" % fmt(mx - 0.60)
    return "FAIL", detail


def c_robust_arrival_jitter(lines, ctx):
    return _robust(lines, "arrival_jitter")


def c_robust_arrival_bunching(lines, ctx):
    return _robust(lines, "arrival_bunching")


def c_robust_variable_interval(lines, ctx):
    return _robust(lines, "variable_interval")


def c_robust_packet_loss(lines, ctx):
    return _robust(lines, "packet_loss")


def c_velocity_legacy_window(lines, ctx):
    recs = [ln for ln in lines if ln.key == "VELOCITY" and "cadence_ms" in ln.kv]
    if not recs:
        return "INFO", "no cadence_ms lines"
    parts = ["cadence_ms=%s zero_pct=%s" % (ln.kv.get("cadence_ms"), ln.kv.get("zero_pct"))
             for ln in recs]
    return "INFO", "legacy 1000 ms window: " + " ".join(parts)


def _arena_records(lines, cases, penalties):
    out = []
    for case in cases:
        for pen in penalties:
            recs = select(lines, "ARENA", case=case, **{"forbidden_penalty": pen})
            out.append((case, pen, recs[0] if recs else None))
    return out


def c_arena_respawn_found(lines, ctx):
    records = _arena_records(lines, ARENA_CASES, ("nofield", "10(default)", "0"))
    bad = []
    parts = []
    for case, pen, ln in records:
        status = ln.kv.get("status") if ln else None
        parts.append("%s/%s=%s" % (case, pen, status if status else "MISSING"))
        if status != "Found":
            bad.append((case, pen, status))
    if bad:
        return "FAIL", "status != Found: " + " ".join(parts)
    return "PASS", "all 6 respawn queries Found"


def c_arena_respawn_nodes(lines, ctx):
    records = _arena_records(lines, ARENA_CASES, ("10(default)", "0"))
    bad = []
    parts = []
    for case, pen, ln in records:
        limit = 2000 if case.startswith("karus") else 6000
        expanded = num(ln, "expanded") if ln else None
        parts.append("%s/%s expanded=%s (<= %s)" % (case, pen, fmt(expanded), fmt(limit)))
        if expanded is None or expanded > limit:
            bad.append((case, pen, expanded))
    if bad:
        return "FAIL", " ".join(parts)
    return "PASS", " ".join(parts)


def c_arena_inside_out_design(lines, ctx):
    records = _arena_records(lines, ("arena_to_karus_respawn",), ("10(default)", "0"))
    bad = []
    parts = []
    for case, pen, ln in records:
        status = ln.kv.get("status") if ln else None
        parts.append("%s/%s=%s" % (case, pen, status if status else "MISSING"))
        if status != "InvalidGoal":
            bad.append((case, pen, status))
    if bad:
        return "FAIL", "status != InvalidGoal: " + " ".join(parts)
    return "PASS", "inside-out goal InvalidGoal (policy not relaxed)"


def c_arena_inside_out_nofield(lines, ctx):
    records = _arena_records(lines, ("arena_to_karus_respawn",), ("nofield",))
    case, pen, ln = records[0]
    status = ln.kv.get("status") if ln else None
    if status == "Found":
        return "PASS", "no-field control Found"
    return "FAIL", "no-field control status=%s (expected Found)" % (status if status else "MISSING")


def c_budget_near64_query_p95(lines, ctx):
    if ctx["skip_timing"]:
        return "INFO", "skipped (--skip-timing)"
    recs = select(lines, "BUDGET", **{"set": "near64"})
    if not recs:
        return "FAIL", "missing line BUDGET set=near64"
    ok = True
    parts = []
    for ln in recs:
        q = num(ln, "query_p95")
        good = q is not None and q <= 2.0
        ok = ok and good
        text = "bots=%s query_p95=%s (<= 2.0)" % (ln.kv.get("bots_per_tick"), fmt(q))
        if not good and q is not None:
            text += " EXCEEDED by %s" % fmt(q - 2.0)
        parts.append(text)
    return ("PASS" if ok else "FAIL"), " ".join(parts)


def c_budget_unscheduled(lines, ctx):
    if ctx["skip_timing"]:
        return "INFO", "skipped (--skip-timing)"
    recs = [ln for ln in lines if ln.key == "BUDGET" and ln.kv.get("set") in ("mid150", "whole")]
    if not recs:
        return "INFO", "no unscheduled budget lines"
    parts = ["%s tick_p95=%s" % (ln.kv.get("set"), ln.kv.get("tick_p95")) for ln in recs]
    return "INFO", "scheduler rationale: " + " ".join(parts)


def _sched_value(lines, field, limit, ctx):
    if ctx["skip_timing"]:
        return "INFO", "skipped (--skip-timing)"
    recs = select(lines, "BUDGET_SCHED", mode="B")
    if not recs:
        return "FAIL", "missing line BUDGET_SCHED mode=B"
    value = num(recs[0], field)
    if value is not None and value <= limit:
        return "PASS", "%s=%s (<= %s)" % (field, fmt(value), fmt(limit))
    return "FAIL", le_detail(field, value, limit)


def c_sched_b_tick_p95(lines, ctx):
    return _sched_value(lines, "tick_p95", 1.5, ctx)


def c_sched_b_longest_wait(lines, ctx):
    return _sched_value(lines, "longest_wait_ms", 1000, ctx)


def c_sched_b_served(lines, ctx):
    if ctx["skip_timing"]:
        return "INFO", "skipped (--skip-timing)"
    arec = select(lines, "BUDGET_SCHED", mode="A")
    brec = select(lines, "BUDGET_SCHED", mode="B")
    if not arec or not brec:
        return "FAIL", "missing line BUDGET_SCHED mode=A/B"
    a_served = num(arec[0], "served")
    b_served = num(brec[0], "served")
    pending = num(brec[0], "pending")
    limit = 0.9 * a_served if a_served is not None else None
    if pending == 0 and b_served is not None and limit is not None and b_served >= limit:
        return "PASS", "A.served=%s B.served=%s (>= 0.9*A=%s) B.pending=0" % (
            fmt(a_served), fmt(b_served), fmt(limit))
    return "FAIL", "A.served=%s B.served=%s (>= 0.9*A=%s) B.pending=%s (== 0)" % (
        fmt(a_served), fmt(b_served), fmt(limit), fmt(pending))


def c_sched_a_info(lines, ctx):
    if ctx["skip_timing"]:
        return "INFO", "skipped (--skip-timing)"
    recs = select(lines, "BUDGET_SCHED", mode="A")
    if not recs:
        return "INFO", "no BUDGET_SCHED mode=A line"
    return "INFO", "unscheduled baseline tick_p95=%s" % recs[0].kv.get("tick_p95")


def c_stuck_cadence_false(lines, ctx):
    recs = select(lines, "STUCK", params="cadence_3200")
    if not recs:
        return "FAIL", "missing line STUCK params=cadence_3200"
    bad = []
    for ln in recs:
        fe = num(ln, "false_episodes")
        tk = num(ln, "ticks")
        if fe != 0 or tk is None or tk <= 0:
            bad.append("%s/%s fe=%s ticks=%s" % (ln.kv.get("model"), ln.kv.get("feed"),
                                                 fmt(fe), fmt(tk)))
    if bad:
        return "FAIL", "false_episodes != 0: " + " ; ".join(bad)
    return "PASS", "%d model/feed lines false_episodes=0" % len(recs)


def c_stuck_default_control(lines, ctx):
    recs = [ln for ln in lines
            if ln.key == "STUCK"
            and ln.kv.get("model") == STUCK_BAD_MODEL
            and ln.kv.get("feed") == "every_tick"
            and ln.kv.get("params") == "F5-09_default"]
    if not recs:
        return "FAIL", "missing STUCK default control line"
    fe = num(recs[0], "false_episodes")
    if fe is not None and fe > 0:
        return "PASS", "false_episodes=%s (> 0: known F5-09 defect present)" % fmt(fe)
    return "WARN", "false_episodes=%s (expected > 0: known defect changed, update expectation)" % fmt(fe)


def c_stuck_true_positive(lines, ctx):
    recs = select(lines, "STUCK_TRUE", params="cadence_3200")
    if not recs:
        return "FAIL", "missing line STUCK_TRUE params=cadence_3200"
    v = num(recs[0], "detected_after_ms")
    if v is not None and 3100 <= v <= 3300:
        return "PASS", "detected_after_ms=%s (3100..3300)" % fmt(v)
    return "FAIL", "detected_after_ms=%s (expected 3100..3300)" % fmt(v)


def c_progress_assessor_false(lines, ctx):
    recs = select(lines, "PROGRESS", evaluator="assessor")
    if not recs:
        return "FAIL", "missing line PROGRESS evaluator=assessor"
    bad = []
    for ln in recs:
        fe = num(ln, "false_episodes")
        st = num(ln, "stalled")
        fm = num(ln, "first_ms")
        tk = num(ln, "ticks")
        if fe != 0 or st != 0 or fm != -1 or tk is None or tk <= 0:
            bad.append("%s fe=%s stalled=%s first_ms=%s ticks=%s" % (
                ln.kv.get("model"), fmt(fe), fmt(st), fmt(fm), fmt(tk)))
    if bad:
        return "FAIL", "assessor false positives: " + " ; ".join(bad)
    return "PASS", "%d models false_episodes=0 stalled=0 first_ms=-1" % len(recs)


def c_progress_assessor_true_positive(lines, ctx):
    recs = select(lines, "PROGRESS_TRUE", evaluator="assessor")
    if not recs:
        return "FAIL", "missing line PROGRESS_TRUE evaluator=assessor"
    v = num(recs[0], "detected_after_ms")
    if v is not None and 3100 <= v <= 3300:
        return "PASS", "detected_after_ms=%s (3100..3300)" % fmt(v)
    return "FAIL", "detected_after_ms=%s (expected 3100..3300)" % fmt(v)


def c_progress_old_info(lines, ctx):
    old = [ln for ln in lines if ln.key == "PROGRESS"
           and ln.kv.get("evaluator") in ("F5-09_default", "cadence_3200")]
    info = []
    for ln in old:
        info.append("%s/%s false=%s" % (ln.kv.get("model"), ln.kv.get("evaluator"),
                                        ln.kv.get("false_episodes")))
    st = select(lines, "STUCK_TRUE", params="F5-09_default")
    if st:
        info.append("STUCK_TRUE/F5-09_default=%s" % st[0].kv.get("detected_after_ms"))
    pt = [ln for ln in lines if ln.key == "PROGRESS_TRUE"
          and ln.kv.get("evaluator") in ("F5-09_default", "cadence_3200")]
    for ln in pt:
        info.append("PROGRESS_TRUE/%s=%s" % (ln.kv.get("evaluator"), ln.kv.get("detected_after_ms")))
    return "INFO", "old evaluators: " + " ".join(info) if info else "info"


def _oracle_ready(ctx):
    if ctx["selftest"]:
        return "INFO", "skipped (selftest)"
    oracle = ctx["oracle"]
    if oracle is None or not oracle.ok:
        return "INFO", "skipped: no navgrid"
    return None, None


def c_oracle_straight_examples(lines, ctx):
    straight = [ln for ln in lines if ln.key == "EXAMPLE" and "straight step" in ln.raw]
    st = with_key(lines, "STRAIGHT")
    blocked = num(st[0], "blocked") if st else None
    if blocked is not None and blocked > 0 and not straight:
        return "FAIL", "STRAIGHT.blocked=%s > 0 but no EXAMPLE straight step line" % fmt(blocked)
    status, detail = _oracle_ready(ctx)
    if status:
        return status, detail
    if not straight:
        return "INFO", "no straight step examples"
    vectors = []
    for ln in straight:
        m = VECTOR_RE.search(ln.raw)
        if m:
            vectors.append(tuple(float(g) for g in m.groups()))
    out = ctx["oracle"].check(vectors)
    if out is None or len(out) != len(vectors):
        return "FAIL", "oracle did not answer %d segments" % len(vectors)
    bad = [(v, o) for v, o in zip(vectors, out) if not o.startswith("BLOCKED")]
    if bad:
        return "FAIL", "oracle disagrees: " + " ; ".join("%s -> %s" % (v, o) for v, o in bad[:3])
    return "PASS", "%d straight examples BLOCKED by oracle" % len(vectors)


def c_oracle_fixed_vectors(lines, ctx):
    status, detail = _oracle_ready(ctx)
    if status:
        return status, detail
    vectors = [v for v, _ in FIXED_VECTORS]
    expect = [e for _, e in FIXED_VECTORS]
    out = ctx["oracle"].check(vectors)
    if out is None or len(out) != len(vectors):
        return "FAIL", "oracle did not answer %d fixed vectors" % len(vectors)
    bad = []
    for v, e, o in zip(vectors, expect, out):
        got = "OK" if o.startswith("OK") else ("BLOCKED" if o.startswith("BLOCKED") else o)
        if got != e:
            bad.append("%s expected %s got %s" % (v, e, o))
    if bad:
        return "FAIL", " ; ".join(bad)
    return "PASS", "3 BLOCKED + 1 OK"


def c_oracle_selftest(lines, ctx):
    if ctx["selftest"]:
        return "INFO", "skipped (selftest; run separately)"
    oracle = ctx["oracle"]
    if oracle is None:
        return "INFO", "skipped: no oracle"
    rc, out = oracle.selftest()
    if rc == 0 and "selftest PASS" in out:
        return "PASS", "nav-segment-check.py selftest PASS"
    return "FAIL", "nav-segment-check.py selftest rc=%d out=%s" % (rc, out.strip())


def c_examples_unexpected(lines, ctx):
    bad = [ln for ln in lines if ln.key == "EXAMPLE" and "straight step" not in ln.raw]
    if not bad:
        return "PASS", "0 unexpected EXAMPLE lines"
    shown = " ; ".join(ln.raw.strip()[:120] for ln in bad[:3])
    return "FAIL", "count=%d: %s" % (len(bad), shown)


# ---------------------------------------------------------------------------
# Check metadata (id, class, condition, source). K3/K10 read this table.
# ---------------------------------------------------------------------------

CHECK_META = (
    ("grid.main_cells", "D", "n == 513 and main_cells == 88508", "F5-01, F5-58"),
    ("smoothing.raw_bad", "D", "raw_bad == 0 and raw_edges > 0", "docs/12 s13.1, AC-NAV-03"),
    ("smoothing.smooth_bad", "D", "smooth_bad == 0 and paths_with_smooth_bad == 0 and smooth_segments > 0", "docs/12 s13.1"),
    ("smoothing.chord_bad", "D", "chord_bad == 0 and chords > 0", "docs/12 s13.1"),
    ("smoothing.coverage", "D", "paths >= 0.9 * n", "sample power [A]"),
    ("lineclear.false_positive", "D", "false_positive == 0 and clear > 0", "docs/12 s13.1"),
    ("straight.control", "K", "blocked > 0", "docs/12 s13.1, F5-58"),
    ("synthetic.single_block", "D", "false_positive == 0 and trials > 0 and nav_line_clear_true > 0", "docs/12 s13.1"),
    ("synthetic.random_clutter", "D", "false_positive == 0 and trials > 0 and nav_line_clear_true > 0", "docs/12 s13.1"),
    ("velocity.jitter", "D", "zero == 0 and max_rel_err <= 0.30", "docs/12 s13.2, F5-56 [A]"),
    ("velocity.robust.arrival_jitter", "D", "zero_pct == 0 and err_p95 <= 0.20 and err_max <= 0.30", "F5-56 s5.3"),
    ("velocity.robust.arrival_bunching", "D", "zero_pct <= 1 and err_p95 <= 0.20 and err_max <= 0.60", "F5-56 s5.3"),
    ("velocity.robust.variable_interval", "D", "zero_pct == 0 and err_p95 <= 0.10", "F5-56 s5.3"),
    ("velocity.robust.packet_loss", "D", "zero_pct == 0 and err_p95 <= 0.10", "F5-56 s5.3"),
    ("velocity.legacy_window", "I", "informational only", "docs/12 s13.2"),
    ("arena.respawn.found", "D", "status == Found for both respawns x nofield/10/0", "F5-51, docs/12 s13.4"),
    ("arena.respawn.nodes", "D", "Karus expanded <= 2000; El Morad expanded <= 6000", "F5-51 K5"),
    ("arena.inside_out.design", "D", "status == InvalidGoal for both penalties", "F5-51, AC-NAV-06"),
    ("arena.inside_out.nofield", "K", "status == Found with no field", "F5-51"),
    ("budget.near64.query_p95", "Z", "query_p95 <= 2.0 (ms)", "AC-NAV-02, MET-PERF-03"),
    ("budget.unscheduled", "I", "informational only", "docs/12 s13.5"),
    ("sched.B.tick_p95", "Z", "tick_p95 <= 1.5 (ms)", "AC-NAV-07, P-NAV-TICK-BUDGET-MS"),
    ("sched.B.longest_wait", "Z", "longest_wait_ms <= 1000", "AC-NAV-07, P-NAV-MAX-WAIT"),
    ("sched.B.served", "D", "B.pending == 0 and B.served >= 0.9 * A.served", "F5-53 K5"),
    ("sched.A.info", "I", "informational only", "F5-53"),
    ("stuck.cadence_3200.false", "D", "false_episodes == 0 and ticks > 0 (6 lines)", "AC-NAV-01, MET-NAV-01 [A], docs/12 s13.3, F5-54"),
    ("stuck.default.control", "K", "false_episodes > 0 expected (else WARN)", "docs/12 s13.3"),
    ("stuck.true_positive", "D", "3100 <= detected_after_ms <= 3300", "docs/12 s13.3, F5-57"),
    ("progress.assessor.false", "D", "false_episodes == 0 and stalled == 0 and first_ms == -1", "F5-57, docs/12 s13.3"),
    ("progress.assessor.true_positive", "D", "3100 <= detected_after_ms <= 3300", "F5-57"),
    ("progress.old.info", "I", "informational only", "F5-57"),
    ("oracle.straight_examples", "D", "each EXAMPLE straight step -> BLOCKED", "F5-58 K7"),
    ("oracle.fixed_vectors", "D", "3 fixed vectors BLOCKED, control OK", "F5-58 K5"),
    ("oracle.selftest", "D", "nav-segment-check.py --selftest exit 0 and 'selftest PASS'", "F5-50"),
    ("examples.unexpected", "D", "count of non-straight EXAMPLE lines == 0", "docs/12 s13.1"),
)

CHECK_FUNCS = {
    "grid.main_cells": c_grid_main_cells,
    "smoothing.raw_bad": c_smoothing_raw_bad,
    "smoothing.smooth_bad": c_smoothing_smooth_bad,
    "smoothing.chord_bad": c_smoothing_chord_bad,
    "smoothing.coverage": c_smoothing_coverage,
    "lineclear.false_positive": c_lineclear_false_positive,
    "straight.control": c_straight_control,
    "synthetic.single_block": c_synthetic_single_block,
    "synthetic.random_clutter": c_synthetic_random_clutter,
    "velocity.jitter": c_velocity_jitter,
    "velocity.robust.arrival_jitter": c_robust_arrival_jitter,
    "velocity.robust.arrival_bunching": c_robust_arrival_bunching,
    "velocity.robust.variable_interval": c_robust_variable_interval,
    "velocity.robust.packet_loss": c_robust_packet_loss,
    "velocity.legacy_window": c_velocity_legacy_window,
    "arena.respawn.found": c_arena_respawn_found,
    "arena.respawn.nodes": c_arena_respawn_nodes,
    "arena.inside_out.design": c_arena_inside_out_design,
    "arena.inside_out.nofield": c_arena_inside_out_nofield,
    "budget.near64.query_p95": c_budget_near64_query_p95,
    "budget.unscheduled": c_budget_unscheduled,
    "sched.B.tick_p95": c_sched_b_tick_p95,
    "sched.B.longest_wait": c_sched_b_longest_wait,
    "sched.B.served": c_sched_b_served,
    "sched.A.info": c_sched_a_info,
    "stuck.cadence_3200.false": c_stuck_cadence_false,
    "stuck.default.control": c_stuck_default_control,
    "stuck.true_positive": c_stuck_true_positive,
    "progress.assessor.false": c_progress_assessor_false,
    "progress.assessor.true_positive": c_progress_assessor_true_positive,
    "progress.old.info": c_progress_old_info,
    "oracle.straight_examples": c_oracle_straight_examples,
    "oracle.fixed_vectors": c_oracle_fixed_vectors,
    "oracle.selftest": c_oracle_selftest,
    "examples.unexpected": c_examples_unexpected,
}


def dump_list():
    out = []
    for cid, cls, cond, source in CHECK_META:
        out.append("%s | %s | %s | %s" % (cid, cls, cond, source))
    return "\n".join(out) + "\n"


def evaluate_all(lines, ctx):
    results = []
    for cid, cls, _cond, source in CHECK_META:
        status, detail = CHECK_FUNCS[cid](lines, ctx)
        results.append(Result(cid, cls, source, status, detail))
    known = KNOWN_KEYS
    unknown = [ln for ln in lines if ln.key not in known]
    return results, unknown


def counts_of(results):
    counts = {"PASS": 0, "FAIL": 0, "WARN": 0, "INFO": 0}
    for r in results:
        counts[r.status] = counts.get(r.status, 0) + 1
    return counts


def render(results, unknown, meta):
    lines = []
    for r in results:
        lines.append("%-5s %-32s %s  [%s]" % (r.status, r.cid, r.detail, r.source))
    if unknown:
        keys = ", ".join(sorted(set(ln.key for ln in unknown)))
        lines.append("%-5s %-32s %s  [unknown]"
                     % ("INFO", "unknown_lines", "count=%d keys=%s" % (len(unknown), keys)))
    counts = counts_of(results)
    if unknown:
        counts["INFO"] += 1
    fails = [r.cid for r in results if r.status == "FAIL"]
    if fails:
        lines.append("FAILED: " + ", ".join(fails))
    checks = counts["PASS"] + counts["FAIL"] + counts["WARN"]
    overall = "PASS" if counts["FAIL"] == 0 else "FAIL"
    lines.append("NAV-REGRESS %s checks=%d pass=%d fail=%d warn=%d info=%d "
                 "unknown_lines=%d (n=%d seed=%d attempts=%d)" % (
                     overall, checks, counts["PASS"], counts["FAIL"], counts["WARN"],
                     counts["INFO"], len(unknown), meta["n"], meta["seed"], meta["attempts"]))
    return "\n".join(lines) + "\n", counts


# ---------------------------------------------------------------------------
# Measurement orchestration.
# ---------------------------------------------------------------------------

class Measure(object):
    """Runs nav_measure. Default goes through nav-measure.sh; the sections used
    by --skip-timing and the timing retries run the already-built binary directly
    (nav-measure.sh recompiles on every call, which would make --skip-timing
    slower than the full run)."""

    def __init__(self):
        out_dir = os.environ.get("NAV_MEASURE_OUT") or os.path.join(ROOT, "build", "nav-measure")
        self.bin = os.path.join(out_dir, "nav_measure")
        self.grid = os.path.join(ROOT, "build", "nav", "zone71.navgrid")
        self.stderr = ""

    def _shell(self, section, n, seed):
        cmd = ["bash", MEASURE, section, "--n", str(n), "--seed", str(seed)]
        return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)

    def all(self, n, seed):
        try:
            p = self._shell("all", n, seed)
        except OSError as exc:
            return None, "cannot run %s: %s" % (MEASURE, exc)
        self.stderr = p.stderr
        if p.returncode != 0:
            return None, (p.stderr or ("nav-measure all rc=%d" % p.returncode))
        return p.stdout, None

    def build(self, n, seed):
        try:
            p = self._shell("__build__", n, seed)
        except OSError as exc:
            return "cannot run %s: %s" % (MEASURE, exc)
        self.stderr = p.stderr
        if p.returncode != 0:
            return (p.stderr or ("nav-measure build rc=%d" % p.returncode))
        return None

    def section(self, section, n, seed):
        cmd = [self.bin, section, "--navgrid", self.grid, "--n", str(n), "--seed", str(seed)]
        try:
            p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        except OSError as exc:
            return None, "cannot run %s: %s" % (self.bin, exc)
        if p.stderr.strip():
            self.stderr = p.stderr
        if p.returncode != 0:
            return None, (p.stderr or ("nav_measure %s rc=%d" % (section, p.returncode)))
        return p.stdout, None

    def sections(self, names, n, seed):
        err = self.build(n, seed)
        if err:
            return None, err
        text = ""
        seen_grid = False
        for name in names:
            out, serr = self.section(name, n, seed)
            if out is None:
                return None, serr
            for line in out.splitlines():
                if line.startswith("GRID "):
                    if seen_grid:
                        continue
                    seen_grid = True
                text += line + "\n"
        return text, self.stderr


def replace_sections(lines, newtext):
    new_lines = parse_output(newtext)
    kept = [ln for ln in lines if ln.key not in ("BUDGET", "BUDGET_SCHED")]
    return kept + new_lines


def tail(text, count=10):
    rows = [r for r in text.splitlines() if r.strip()]
    return "\n".join(rows[-count:])


def run_real(args):
    n = args.n
    seed = args.seed
    oracle = Oracle(NAVGRID)
    measure = Measure()
    ctx = {
        "n": n,
        "skip_timing": args.skip_timing,
        "selftest": False,
        "oracle": oracle,
    }
    if args.from_file:
        if not os.path.isfile(args.from_file):
            sys.stderr.write("nav-regress: cannot read %s\n" % args.from_file)
            return 2
        try:
            with open(args.from_file, "r") as fh:
                text = fh.read()
        except OSError as exc:
            sys.stderr.write("nav-regress: cannot read %s: %s\n" % (args.from_file, exc))
            return 2
    elif args.skip_timing:
        text, err = measure.sections(NON_TIMING_SECTIONS, n, seed)
        if text is None:
            sys.stderr.write("nav-regress: environment error: %s\n" % err)
            return 2
    else:
        text, err = measure.all(n, seed)
        if text is None:
            sys.stderr.write("nav-regress: environment error: %s\n%s\n" % (err, tail(err)))
            return 2

    error = find_error(text)
    if error:
        sys.stderr.write("nav-regress: environment error: %s\n%s\n" % (error, tail(text)))
        return 2

    if args.save:
        try:
            with open(args.save, "w") as fh:
                fh.write(text)
        except OSError as exc:
            sys.stderr.write("nav-regress: cannot write %s: %s\n" % (args.save, exc))
            return 2

    lines = parse_output(text)
    results, unknown = evaluate_all(lines, ctx)
    attempts = 1
    timing_active = (not args.skip_timing) and (not args.from_file)

    if timing_active:
        z_pass = {}
        z_last = {}
        for r in results:
            if r.cid in Z_IDS:
                z_last[r.cid] = r
                if r.status == "PASS":
                    z_pass[r.cid] = 1
        for attempt in range(2, args.timing_retries + 2):
            if all(cid in z_pass for cid in Z_IDS):
                break
            attempts = attempt
            btext, berr = measure.section("budget", n, seed)
            if btext is None:
                sys.stderr.write("nav-regress: timing retry failed: %s\n" % berr)
                break
            sched, serr = measure.section("budget-scheduled", n, seed)
            if sched is None:
                sys.stderr.write("nav-regress: timing retry failed: %s\n" % serr)
                break
            merged = replace_sections(lines, btext + sched)
            ctx2 = dict(ctx)
            ctx2["oracle"] = None
            res2, _ = evaluate_all(merged, ctx2)
            for r in res2:
                if r.cid in Z_IDS:
                    z_last[r.cid] = r
                    if r.status == "PASS" and r.cid not in z_pass:
                        z_pass[r.cid] = attempt
        by_id = {r.cid: i for i, r in enumerate(results)}
        for cid in Z_IDS:
            idx = by_id[cid]
            if cid in z_pass:
                results[idx] = Result(cid, results[idx].cls, results[idx].source, "PASS",
                                      z_last[cid].detail + " (attempt %d/%d)" % (z_pass[cid], attempts))
            else:
                results[idx] = Result(cid, results[idx].cls, results[idx].source, "FAIL",
                                      z_last[cid].detail + " (attempts %d/%d)" % (attempts, attempts))

    meta = {"n": n, "seed": seed, "attempts": attempts}
    out, counts = render(results, unknown, meta)
    sys.stdout.write(out)
    return 0 if counts["FAIL"] == 0 else 1


# ---------------------------------------------------------------------------
# Selftest: built-in mutations prove every check class can FAIL.
# ---------------------------------------------------------------------------

MUTATIONS = (
    {"cid": "smoothing.raw_bad", "expect": "FAIL", "contains": ("SMOOTHING",), "sub": (r"\braw_bad=0\b", "raw_bad=1")},
    {"cid": "smoothing.smooth_bad", "expect": "FAIL", "contains": ("SMOOTHING",), "sub": (r"\bsmooth_bad=0\b", "smooth_bad=3")},
    {"cid": "smoothing.chord_bad", "expect": "FAIL", "contains": ("SMOOTHING",), "sub": (r"\bchord_bad=0\b", "chord_bad=2")},
    {"cid": "lineclear.false_positive", "expect": "FAIL", "contains": ("LINECLEAR",), "sub": (r"\bfalse_positive=0\b", "false_positive=1")},
    {"cid": "straight.control", "expect": "FAIL", "contains": ("STRAIGHT",), "sub": (r"\bblocked=\d+\b", "blocked=0")},
    {"cid": "synthetic.single_block", "expect": "FAIL", "contains": ("SYNTHETIC single_block",), "sub": (r"\bfalse_positive=0\b", "false_positive=1")},
    {"cid": "velocity.jitter", "expect": "FAIL", "contains": ("VELOCITY jitter=",), "sub": (r"\bzero=0\b", "zero=5")},
    {"cid": "velocity.robust.arrival_bunching", "expect": "FAIL", "contains": ("scenario=arrival_bunching",), "sub": (r"\berr_max=[\d.]+\b", "err_max=0.61")},
    {"cid": "velocity.robust.variable_interval", "expect": "FAIL", "contains": ("scenario=variable_interval",), "sub": (r"\berr_p95=[\d.]+\b", "err_p95=0.11")},
    {"cid": "arena.respawn.found", "expect": "FAIL", "contains": ("elmorad_respawn_to_arena", "forbidden_penalty=10(default)"), "sub": (r"\bstatus=Found\b", "status=NodeLimit")},
    {"cid": "arena.respawn.nodes", "expect": "FAIL", "contains": ("elmorad_respawn_to_arena", "forbidden_penalty=10(default)"), "sub": (r"\bexpanded=\d+\b", "expanded=6001")},
    {"cid": "arena.inside_out.design", "expect": "FAIL", "contains": ("arena_to_karus_respawn", "forbidden_penalty=10(default)"), "sub": (r"\bstatus=InvalidGoal\b", "status=Found")},
    {"cid": "budget.near64.query_p95", "expect": "FAIL", "contains": ("BUDGET set=near64 bots_per_tick=16",), "sub": (r"\bquery_p95=[\d.]+\b", "query_p95=2.5")},
    {"cid": "sched.B.tick_p95", "expect": "FAIL", "contains": ("BUDGET_SCHED mode=B",), "sub": (r"\btick_p95=[\d.]+\b", "tick_p95=1.6")},
    {"cid": "sched.B.longest_wait", "expect": "FAIL", "contains": ("BUDGET_SCHED mode=B",), "sub": (r"\blongest_wait_ms=\d+\b", "longest_wait_ms=1500")},
    {"cid": "sched.B.served", "expect": "FAIL", "contains": ("BUDGET_SCHED mode=B",), "sub": (r"\bserved=\d+\b", "served=1000")},
    {"cid": "stuck.cadence_3200.false", "expect": "FAIL", "contains": ("STUCK ", "params=cadence_3200"), "sub": (r"\bfalse_episodes=0\b", "false_episodes=1")},
    {"cid": "stuck.true_positive", "expect": "FAIL", "contains": ("STUCK_TRUE params=cadence_3200",), "sub": (r"\bdetected_after_ms=\d+\b", "detected_after_ms=5000")},
    {"cid": "progress.assessor.false", "expect": "FAIL", "contains": ("PROGRESS ", "evaluator=assessor"), "sub": (r"\bfalse_episodes=0\b", "false_episodes=1")},
    {"cid": "progress.assessor.true_positive", "expect": "FAIL", "contains": ("PROGRESS_TRUE evaluator=assessor",), "sub": (r"\bdetected_after_ms=\d+\b", "detected_after_ms=2900")},
    {"cid": "examples.unexpected", "expect": "FAIL", "append": "EXAMPLE smoothing segment (1.0,1.0)->(5.0,5.0) cells (0,0)->(1,1) touches non-walk cell (0,0)"},
    {"cid": "stuck.default.control", "expect": "WARN", "contains": ("STUCK ", "model=tick110.8+-20+3%late250", "feed=every_tick", "params=F5-09_default"), "sub": (r"\bfalse_episodes=\d+\b", "false_episodes=0")},
)


def matches_mutation(line, m):
    for needle in m.get("contains", ()):
        if needle not in line:
            return False
    return True


def apply_mutation(text, m):
    out = []
    done = False
    for line in text.splitlines():
        if not done and matches_mutation(line, m):
            if "sub" in m:
                new = re.sub(m["sub"][0], m["sub"][1], line, count=1)
                if new != line:
                    line = new
                    done = True
        out.append(line)
    if "append" in m:
        out.append(m["append"])
        done = True
    return "\n".join(out) + "\n"


def remove_line(text, prefix):
    out = []
    removed = False
    for line in text.splitlines():
        if not removed and line.startswith(prefix):
            removed = True
            continue
        out.append(line)
    return "\n".join(out) + "\n", removed


def evaluate_status(text, cid, n):
    results, _ = evaluate_all(parse_output(text), {
        "n": n, "skip_timing": False, "selftest": True, "oracle": None})
    for r in results:
        if r.cid == cid:
            return r.status, r.detail
    return "MISSING", "check id not found"


def fail_count(text, n):
    results, _ = evaluate_all(parse_output(text), {
        "n": n, "skip_timing": False, "selftest": True, "oracle": None})
    return sum(1 for r in results if r.status == "FAIL")


def run_selftest():
    if not os.path.isfile(GOOD_TXT):
        sys.stdout.write("selftest FAIL: missing %s\n" % GOOD_TXT)
        return 1
    with open(GOOD_TXT, "r") as fh:
        good = fh.read()
    n = 2000
    cases = 0
    ok_count = 0
    failures = []

    cases += 1
    if fail_count(good, n) == 0:
        ok_count += 1
        sys.stdout.write("ok   good.txt -> 0 FAIL\n")
    else:
        status, detail = "FAIL", "good.txt FAIL count %d" % fail_count(good, n)
        failures.append("good.txt: %s" % detail)
        sys.stdout.write("bad  good.txt -> %s\n" % detail)

    for m in MUTATIONS:
        cases += 1
        mutated = apply_mutation(good, m)
        status, detail = evaluate_status(mutated, m["cid"], n)
        if status == m["expect"]:
            ok_count += 1
            sys.stdout.write("ok   %s -> %s\n" % (m["cid"], status))
        else:
            failures.append("%s: expected %s got %s (%s)" % (m["cid"], m["expect"], status, detail))
            sys.stdout.write("bad  %s -> expected %s got %s\n" % (m["cid"], m["expect"], status))

    cases += 1
    deleted, removed = remove_line(good, "GRID")
    status, detail = evaluate_status(deleted, "grid.main_cells", n)
    if removed and status == "FAIL" and "missing line" in detail:
        ok_count += 1
        sys.stdout.write("ok   delete GRID -> FAIL missing line\n")
    else:
        failures.append("delete GRID: status=%s detail=%s" % (status, detail))
        sys.stdout.write("bad  delete GRID -> %s %s\n" % (status, detail))

    cases += 1
    rc, out = Oracle(NAVGRID).selftest()
    if rc == 0 and "selftest PASS" in out:
        ok_count += 1
        sys.stdout.write("ok   nav-segment-check.py --selftest\n")
    else:
        failures.append("nav-segment-check selftest rc=%d out=%s" % (rc, out.strip()))
        sys.stdout.write("bad  nav-segment-check.py --selftest rc=%d\n" % rc)

    cases += 1
    if fail_count("", n) > 0:
        ok_count += 1
        sys.stdout.write("ok   empty input -> FAIL missing line\n")
    else:
        failures.append("empty input did not FAIL")
        sys.stdout.write("bad  empty input -> no FAIL\n")

    cases += 1
    if find_error("ERROR cannot load build/nav/zone71.navgrid\n") is not None:
        ok_count += 1
        sys.stdout.write("ok   ERROR input -> exit 2 path\n")
    else:
        failures.append("ERROR input not detected")
        sys.stdout.write("bad  ERROR input -> not detected\n")

    sys.stdout.write("selftest: %d cases, %d ok\n" % (cases, ok_count))
    if failures:
        for f in failures:
            sys.stdout.write("  - %s\n" % f)
        sys.stdout.write("selftest FAIL\n")
        return 1
    sys.stdout.write("selftest PASS\n")
    return 0


def parse_args(argv):
    ap = argparse.ArgumentParser(
        prog="nav-regress.py",
        description="Persistent navigation regression evaluator (plan F5-11).")
    ap.add_argument("--n", type=int, default=DEFAULT_N)
    ap.add_argument("--seed", type=int, default=DEFAULT_SEED)
    ap.add_argument("--skip-timing", action="store_true", dest="skip_timing")
    ap.add_argument("--timing-retries", type=int, default=DEFAULT_RETRIES, dest="timing_retries")
    ap.add_argument("--from-file", dest="from_file")
    ap.add_argument("--save", dest="save")
    ap.add_argument("--list", action="store_true", dest="list_checks")
    ap.add_argument("--selftest", action="store_true")
    return ap.parse_args(argv)


def main(argv):
    args = parse_args(argv)
    if args.selftest:
        return run_selftest()
    if args.list_checks:
        sys.stdout.write(dump_list())
        return 0
    return run_real(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
