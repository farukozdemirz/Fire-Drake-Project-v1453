#!/usr/bin/env python3
"""Summarises GameServer FDP_DAMAGE_TRACE logs and compares them with models.

Each log line is tab separated (20 columns, stable order):
    wall_ms t_ms ctx primary a_sid a_name a_class a_nation a_level a_hit
    t_sid t_name t_class t_nation t_level t_ac requested applied hp_before hp_max

ctx is R, - or S<magicNum>. requested is the amount handed to CUser::HpChange
(before MAX_DAMAGE, mirror, mana absorb and mastery reductions; damage negative,
heal positive). applied is hp_after - hp_before. The Python models predict the
requested value (GetDamage / GetMagicDamage), so the comparison is like for
like: misses are not logged, hence the measured average is a hit-only average,
which is what the models produce. Small samples (low n) can be noisy, so LOW_N
is reported instead of OK/FAIL. app_ratio and the t_ac range are side
indicators for Q-08 (AC debuff double application); they are informational.

Usage:
    python3 tools/damage-trace-summary.py LOG [LOG ...] [--stat-model FILE]
        [--spell-model FILE] [--attacker NAME] [--target NAME] [--tol PCT]
        [--min-n N]
    python3 tools/damage-trace-summary.py --selftest
"""

import io
import re
import sys

USAGE = (
    "Usage:\n"
    "  python3 tools/damage-trace-summary.py LOG [LOG ...] [--stat-model FILE]\n"
    "      [--spell-model FILE] [--attacker NAME] [--target NAME]\n"
    "      [--tol PCT] [--min-n N]\n"
    "  python3 tools/damage-trace-summary.py --selftest\n"
    "Options:\n"
    "  --tol PCT          comparison tolerance in percent (default 15)\n"
    "  --min-n N          minimum sample count for OK/FAIL (default 5)\n"
    "  --stat-model FILE  redirected output of tools/stat-model.py\n"
    "  --spell-model FILE redirected output of tools/spell-model.py\n"
)

INT_FIELDS = (
    "wall_ms", "t_ms", "primary", "a_sid", "a_class", "a_nation", "a_level",
    "a_hit", "t_sid", "t_class", "t_nation", "t_level", "t_ac", "requested",
    "applied", "hp_before", "hp_max",
)

COLUMNS = (
    "wall_ms", "t_ms", "ctx", "primary", "a_sid", "a_name", "a_class",
    "a_nation", "a_level", "a_hit", "t_sid", "t_name", "t_class", "t_nation",
    "t_level", "t_ac", "requested", "applied", "hp_before", "hp_max",
)

CTX_RE = re.compile(r"^S\d+$")
PROFILE_RE = re.compile(r"^Bot([A-Z]+)_([KE])$")


def parse_line(line):
    """Parses one 20-column log line; returns a dict or None when invalid."""
    parts = line.rstrip("\r\n").split("\t")
    if len(parts) != 20:
        return None

    record = dict(zip(COLUMNS, parts))
    try:
        for key in INT_FIELDS:
            record[key] = int(record[key])
    except (TypeError, ValueError):
        return None

    ctx = record["ctx"]
    if ctx != "R" and ctx != "-" and not CTX_RE.match(ctx):
        return None
    record["skill"] = int(ctx[1:]) if ctx.startswith("S") else None
    return record


def profile_of(name):
    match = PROFILE_RE.match(name or "")
    return match.group(1) if match else None


def parse_model_line(line):
    """Parses one model output line into a record or None.

    Only key=value tokens carry fields (skill names may contain spaces and have
    no '='), the leading type and name tokens are read by position.
    """
    parts = line.rstrip("\r\n").split()
    if not parts:
        return None
    kind = parts[0]
    if kind not in ("R", "K", "M", "H"):
        return None

    record = {"kind": kind, "a_name": None, "t_name": None, "skill": None,
              "fields": {}}
    if kind in ("R", "K", "M"):
        if len(parts) < 2 or "->" not in parts[1]:
            return None
        a_name, t_name = parts[1].split("->", 1)
        record["a_name"] = a_name
        record["t_name"] = t_name
    else:
        if len(parts) < 2:
            return None
        record["a_name"] = parts[1]

    for token in parts[2:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        record["fields"][key] = value

    if kind in ("K", "M", "H"):
        try:
            record["skill"] = int(record["fields"]["skill"])
        except (KeyError, ValueError):
            record["skill"] = None
    return record


def field_float(record, key):
    if record is None:
        return None
    value = record["fields"].get(key)
    if value is None or value == "-":
        return None
    try:
        return float(value)
    except ValueError:
        return None


def load_model_file(path):
    records = []
    with open(path, "r", encoding="latin-1", newline="") as handle:
        for line in handle:
            record = parse_model_line(line)
            if record is not None:
                records.append(record)
    return records


def build_indexes(stat_records, spell_records):
    stat_r = {}
    stat_k = {}
    for record in stat_records:
        if record["kind"] == "R":
            stat_r[(record["a_name"], record["t_name"])] = record
        elif record["kind"] == "K":
            stat_k[(record["a_name"], record["t_name"], record["skill"])] = record

    spell_m = {}
    spell_h = {}
    for record in spell_records:
        if record["kind"] == "M":
            spell_m[(record["a_name"], record["t_name"], record["skill"])] = record
        elif record["kind"] == "H":
            spell_h[(record["a_name"], record["skill"])] = record
    return stat_r, stat_k, spell_m, spell_h


def find_r(stat_r, a_name, t_name):
    record = stat_r.get((a_name, t_name))
    if record is not None:
        return "R", record, "exact"
    return find_by_profile(stat_r, a_name, t_name, None, None)


def find_dmg_skill(stat_k, spell_m, a_name, t_name, skill):
    record = stat_k.get((a_name, t_name, skill))
    if record is not None:
        return "K", record, "exact"
    record = spell_m.get((a_name, t_name, skill))
    if record is not None:
        return "M", record, "exact"

    found = find_by_profile(stat_k, a_name, t_name, skill, "K")
    if found[1] is not None:
        return found
    return find_by_profile(spell_m, a_name, t_name, skill, "M")


def find_h(spell_h, a_name, skill):
    record = spell_h.get((a_name, skill))
    if record is not None:
        return "H", record, "exact"

    a_profile = profile_of(a_name)
    if a_profile is None:
        return None, None, "-"
    for record in spell_h.values():
        if profile_of(record["a_name"]) == a_profile and record["skill"] == skill:
            return "H", record, "profile"
    return None, None, "-"


def find_by_profile(index, a_name, t_name, skill, kind):
    a_profile = profile_of(a_name)
    t_profile = profile_of(t_name)
    if a_profile is None or t_profile is None:
        return None, None, "-"
    for record in index.values():
        if profile_of(record["a_name"]) != a_profile:
            continue
        if profile_of(record["t_name"]) != t_profile:
            continue
        if skill is not None and record["skill"] != skill:
            continue
        return kind, record, "profile"
    return None, None, "-"


def find_dot_tick(spell_m, a_name, t_name, skill):
    record = spell_m.get((a_name, t_name, skill))
    if record is None:
        _, record, _ = find_by_profile(spell_m, a_name, t_name, skill, "M")
    if record is None:
        return None
    return field_float(record, "dot_tick")


def build_a_groups(rows):
    """Groups primary=1 rows by (ctx, attacker, target, kind)."""
    groups = {}
    zero = 0
    for row in rows:
        if row["primary"] != 1:
            continue
        requested = row["requested"]
        if requested == 0:
            zero += 1
            continue
        kind = "dmg" if requested < 0 else "heal"
        key = (row["ctx"], row["a_name"], row["t_name"], kind)
        groups.setdefault(key, []).append(row)
    return groups, zero


def build_d_groups(rows):
    """Groups primary=0 rows by (attacker, target, kind)."""
    groups = {}
    for row in rows:
        if row["primary"] != 0:
            continue
        kind = "dmg" if row["requested"] < 0 else "heal"
        key = (row["a_name"], row["t_name"], kind)
        groups.setdefault(key, []).append(row)
    return groups


def group_summary(events):
    requests = [abs(row["requested"]) for row in events]
    applied = [abs(row["applied"]) for row in events]
    req_sum = sum(requests)
    app_sum = sum(applied)
    lethal = sum(
        1 for row in events
        if row["requested"] < 0 and row["hp_before"] + row["applied"] == 0
    )
    return {
        "n": len(events),
        "req_avg": req_sum / float(len(events)),
        "req_min": min(requests),
        "req_max": max(requests),
        "app_avg": app_sum / float(len(events)),
        "app_ratio": (app_sum / float(req_sum)) if req_sum else 0.0,
        "a_hit_min": min(row["a_hit"] for row in events),
        "a_hit_max": max(row["a_hit"] for row in events),
        "t_ac_min": min(row["t_ac"] for row in events),
        "t_ac_max": max(row["t_ac"] for row in events),
        "lethal": lethal,
    }


def write_a_section(groups, out):
    out.write("== A ==\n")
    for key in sorted(groups):
        ctx, a_name, t_name, kind = key
        summary = group_summary(groups[key])
        out.write(
            "A ctx=%s kind=%s a=%s t=%s n=%d req_avg=%.1f req_min=%d req_max=%d "
            "app_avg=%.1f app_ratio=%.3f a_hit=%d-%d t_ac=%d-%d lethal=%d\n"
            % (ctx, kind, a_name, t_name, summary["n"], summary["req_avg"],
               summary["req_min"], summary["req_max"], summary["app_avg"],
               summary["app_ratio"], summary["a_hit_min"], summary["a_hit_max"],
               summary["t_ac_min"], summary["t_ac_max"], summary["lethal"])
        )


def write_d_section(groups, a_groups, spell_m, out):
    out.write("== D ==\n")
    skills_used = {}
    for (ctx, a_name, _t_name, _kind) in a_groups:
        if ctx.startswith("S"):
            skills_used.setdefault(a_name, set()).add(int(ctx[1:]))

    for key in sorted(groups):
        a_name, t_name, kind = key
        events = groups[key]
        requests = [row["requested"] for row in events]
        ctx_seen = sorted(set(row["ctx"] for row in events))
        line = (
            "D a=%s t=%s kind=%s n=%d req_sum=%d req_avg=%.1f ctx_seen=%s"
            % (a_name, t_name, kind, len(events), sum(requests),
               sum(abs(value) for value in requests) / float(len(events)),
               ",".join(ctx_seen))
        )
        dot_values = []
        for skill in sorted(skills_used.get(a_name, ())):
            dot_tick = find_dot_tick(spell_m, a_name, t_name, skill)
            if dot_tick is not None:
                dot_values.append("%d:%.1f" % (skill, dot_tick))
        if dot_values:
            line += " model_dot_tick=" + ",".join(dot_values)
        out.write(line + "\n")


def range_violations(events, record):
    dmg_min = field_float(record, "dmg_min")
    dmg_max = field_float(record, "dmg_max")
    if dmg_min is None or dmg_max is None:
        return None
    low = min(dmg_min, dmg_max)
    high = max(dmg_min, dmg_max)
    count = 0
    for row in events:
        value = abs(row["requested"])
        if value < low or value > high:
            count += 1
    return count


def write_c_section(a_groups, indexes, out, tol, min_n):
    stat_r, stat_k, spell_m, spell_h = indexes
    out.write("== C ==\n")
    for key in sorted(a_groups):
        ctx, a_name, t_name, kind = key
        events = a_groups[key]
        summary = group_summary(events)
        skill = None
        if ctx.startswith("S"):
            skill = int(ctx[1:])

        model_kind, record, match = None, None, "-"
        if kind == "dmg" and ctx == "R":
            model_kind, record, match = find_r(stat_r, a_name, t_name)
        elif kind == "dmg" and skill is not None:
            model_kind, record, match = find_dmg_skill(
                stat_k, spell_m, a_name, t_name, skill)
        elif kind == "heal" and skill is not None:
            model_kind, record, match = find_h(spell_h, a_name, skill)

        model_value = None
        range_viol = None
        if model_kind == "H":
            model_value = field_float(record, "heal_instant")
        elif model_kind in ("R", "K", "M"):
            model_value = field_float(record, "dmg_avg")
            range_viol = range_violations(events, record)

        if model_value is not None and model_value != 0:
            diff_pct = (summary["req_avg"] - model_value) / model_value * 100.0
        else:
            diff_pct = None

        if summary["n"] < min_n:
            verdict = "LOW_N"
        elif model_value is None or model_value == 0:
            verdict = "NO_MODEL"
        elif abs(diff_pct) <= tol:
            verdict = "OK"
        else:
            verdict = "FAIL"

        out.write(
            "C kind=%s a=%s t=%s skill=%s n=%d meas=%.1f model=%s diff_pct=%s "
            "verdict=%s match=%s range_viol=%s\n"
            % (model_kind if model_kind else "-", a_name, t_name,
               "%d" % skill if skill is not None else "-", summary["n"],
               summary["req_avg"],
               "%.1f" % model_value if model_value is not None else "-",
               "%+.1f" % diff_pct if diff_pct is not None else "-",
               verdict, match,
               "%d" % range_viol if range_viol is not None else "-")
        )


def write_report(all_rows, rows, counters, stat_records, spell_records, out,
                 tol, min_n):
    indexes = build_indexes(stat_records, spell_records)
    a_groups, zero = build_a_groups(rows)
    d_groups = build_d_groups(rows)

    wall_times = [row["wall_ms"] for row in all_rows]
    span_s = (max(wall_times) - min(wall_times)) / 1000.0 if wall_times else 0.0

    out.write("== L ==\n")
    out.write(
        "L files=%d lines=%d parsed=%d bad=%d span_s=%.1f filtered=%d zero=%d\n"
        % (counters["files"], counters["lines"], len(all_rows), counters["bad"],
           span_s, counters["filtered"], zero)
    )
    out.write("L note=misses_not_logged hit_rate_not_measured\n")

    write_a_section(a_groups, out)
    write_d_section(d_groups, a_groups, indexes[2], out)
    write_c_section(a_groups, indexes, out, tol, min_n)
    return 0


def load_logs(paths):
    all_rows = []
    lines = 0
    bad = 0
    for path in paths:
        with open(path, "r", encoding="latin-1", newline="") as handle:
            for raw in handle:
                if not raw.strip():
                    continue
                lines += 1
                row = parse_line(raw)
                if row is None:
                    bad += 1
                    continue
                all_rows.append(row)
    return all_rows, lines, bad


def log_line(*values):
    return "\t".join(str(value) for value in values)


def make_row(wall_ms, ctx, primary, a_name, t_name, requested, applied,
             hp_before=1000, hp_max=32000, a_hit=50, t_ac=100):
    return log_line(
        wall_ms, wall_ms, ctx, primary, 1, a_name, 1, 1, 70, a_hit,
        2, t_name, 1, 2, 70, t_ac, requested, applied, hp_before, hp_max)


def render(log_lines, stat_lines, spell_lines, tol=15, min_n=5):
    rows = []
    bad = 0
    for line in log_lines:
        row = parse_line(line)
        if row is None:
            bad += 1
        else:
            rows.append(row)
    stat_records = [record for record in
                    (parse_model_line(line) for line in stat_lines) if record]
    spell_records = [record for record in
                     (parse_model_line(line) for line in spell_lines) if record]
    buffer = io.StringIO()
    counters = {"files": 1, "lines": len(log_lines), "bad": bad, "filtered": 0}
    write_report(rows, rows, counters, stat_records, spell_records, buffer,
                 tol, min_n)
    return buffer.getvalue()


def run_selftest():
    valid = make_row(0, "R", 1, "BotWP_K", "BotMI_K", -200, -170)
    assert parse_line(valid) is not None

    short = "\t".join(valid.split("\t")[:19])
    assert parse_line(short) is None

    non_int = valid.split("\t")
    non_int[0] = "abc"
    assert parse_line("\t".join(non_int)) is None

    bad_ctx = valid.split("\t")
    bad_ctx[2] = "X12"
    assert parse_line("\t".join(bad_ctx)) is None

    bad_skill = valid.split("\t")
    bad_skill[2] = "Sx"
    assert parse_line("\t".join(bad_skill)) is None

    r_model = ["R BotWP_K->BotMI_K hit_prob=0.920 base=225 dmg_avg=205.0 "
               "dmg_min=180 dmg_max=230 exp=200.0"]
    r_fail = ["R BotWP_K->BotMI_K hit_prob=0.920 base=225 dmg_avg=150.0 "
              "dmg_min=180 dmg_max=230 exp=200.0"]
    r_lines = [
        make_row(0, "R", 1, "BotWP_K", "BotMI_K", -200, -170),
        make_row(100, "R", 1, "BotWP_K", "BotMI_K", -210, -180),
        make_row(200, "R", 1, "BotWP_K", "BotMI_K", -190, -160),
        make_row(300, "R", 1, "BotWP_K", "BotMI_K", -205, -175),
        make_row(400, "R", 1, "BotWP_K", "BotMI_K", -195, -165),
        make_row(500, "R", 1, "BotWP_K", "BotMI_K", -200, -170),
    ]
    text = render(r_lines, r_model, [])
    assert "n=6 req_avg=200.0" in text, text
    assert "C kind=R a=BotWP_K t=BotMI_K skill=- n=6 meas=200.0 model=205.0 " \
           "diff_pct=-2.4 verdict=OK match=exact" in text, text

    text = render(r_lines, r_fail, [])
    assert "verdict=FAIL" in text, text

    low = render(r_lines[:3], r_model, [], min_n=5)
    assert "verdict=LOW_N" in low, low

    h_model = ["H BotPHD_K skill=101006 Heal heal_instant=310 first=310 time=0 "
               "dur=0 radius=0 msp=30 cast_s=1.0 recast_s=0.5 hot_tick=0 "
               "ticks=0 hot_total=0 heal_per_msp=10.33"]
    h_lines = [
        make_row(0, "S101006", 1, "BotPHD_K", "BotWP_K", 300, 300),
        make_row(100, "S101006", 1, "BotPHD_K", "BotWP_K", 300, 300),
        make_row(200, "S101006", 1, "BotPHD_K", "BotWP_K", 300, 300),
    ]
    text = render(h_lines, [], h_model, min_n=3)
    assert "C kind=H a=BotPHD_K t=BotWP_K skill=101006 n=3 meas=300.0 " \
           "model=310.0 diff_pct=-3.2 verdict=OK match=exact" in text, text

    ctx_lines = [
        make_row(0, "-", 0, "BotWP_K", "BotMI_K", -80, -80),
        make_row(100, "-", 0, "BotWP_K", "BotMI_K", -90, -90),
    ]
    text = render(ctx_lines, r_model, [])
    assert "D a=BotWP_K t=BotMI_K kind=dmg n=2 req_sum=-170 req_avg=85.0 " \
           "ctx_seen=-" in text, text
    assert "== A ==\n== D ==" in text, text
    assert "== C ==\n" in text
    assert text.split("== C ==")[1].strip() == "", text

    fallback = render(r_lines, r_model, [])
    assert "match=exact" in fallback, fallback
    e_lines = [
        make_row(0, "R", 1, "BotWP_E", "BotMI_E", -200, -170),
    ]
    fallback = render(e_lines, r_model, [], min_n=1)
    assert "match=profile" in fallback, fallback

    k_model = ["K BotWP_K->BotMI_K skill=109510 Hammer Drop hit_pct=0.9200 "
               "sHit=250 base=300 dmg_avg=210.0"]
    k_record = parse_model_line(k_model[0])
    assert k_record is not None, k_record
    assert k_record["skill"] == 109510, k_record
    assert field_float(k_record, "dmg_avg") == 210.0, k_record

    lethal_lines = [
        make_row(0, "S109510", 1, "BotWP_K", "BotMI_K", -250, -100,
                 hp_before=100),
    ]
    text = render(lethal_lines, [], [], min_n=1)
    assert "lethal=1" in text, text
    assert "app_ratio=0.400" in text, text

    print("selftest OK")
    return 0


def main(argv):
    if "--selftest" in argv:
        return run_selftest()

    options = {"tol": 15.0, "min_n": 5, "stat_model": None, "spell_model": None,
               "attacker": None, "target": None}
    value_options = {
        "--tol": "tol", "--min-n": "min_n", "--stat-model": "stat_model",
        "--spell-model": "spell_model", "--attacker": "attacker",
        "--target": "target",
    }
    logs = []
    index = 0
    while index < len(argv):
        arg = argv[index]
        if arg in value_options:
            if index + 1 >= len(argv):
                sys.stderr.write("error: %s needs a value\n" % arg)
                sys.stderr.write(USAGE)
                return 2
            key = value_options[arg]
            value = argv[index + 1]
            if key in ("tol", "min_n"):
                try:
                    value = float(value)
                    if key == "min_n":
                        value = int(value)
                except ValueError:
                    sys.stderr.write("error: %s needs a number\n" % arg)
                    return 2
            options[key] = value
            index += 2
        elif arg.startswith("--"):
            sys.stderr.write("unknown argument: %s\n" % arg)
            sys.stderr.write(USAGE)
            return 2
        else:
            logs.append(arg)
            index += 1

    if not logs:
        sys.stderr.write(USAGE)
        return 2

    try:
        all_rows, lines, bad = load_logs(logs)
    except OSError as exc:
        sys.stderr.write("error: cannot read log: %s\n" % exc)
        return 2

    stat_records = []
    spell_records = []
    try:
        if options["stat_model"]:
            stat_records = load_model_file(options["stat_model"])
        if options["spell_model"]:
            spell_records = load_model_file(options["spell_model"])
    except OSError as exc:
        sys.stderr.write("error: cannot read model output: %s\n" % exc)
        return 2

    rows = []
    filtered = 0
    for row in all_rows:
        if options["attacker"] is not None and row["a_name"] != options["attacker"]:
            filtered += 1
            continue
        if options["target"] is not None and row["t_name"] != options["target"]:
            filtered += 1
            continue
        rows.append(row)

    counters = {"files": len(logs), "lines": lines, "bad": bad,
                "filtered": filtered}
    write_report(all_rows, rows, counters, stat_records, spell_records,
                 sys.stdout, options["tol"], options["min_n"])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
