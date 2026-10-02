#!/usr/bin/env python3
"""Summarises GameServer FDP_PACKET_TRACE logs.

Each log line is tab separated:
    t_ms<TAB>sid<TAB>name<TAB>zone<TAB>opcode_hex<TAB>len<TAB>payload_hex

Usage:
    python3 tools/packet-trace-summary.py <log> [--sid N] [--name X]
    python3 tools/packet-trace-summary.py --selftest
"""

import bisect
import io
import math
import statistics
import struct
import sys

OPCODE_NAMES = {
    0x06: "WIZ_MOVE",
    0x08: "WIZ_ATTACK",
    0x09: "WIZ_ROTATE",
    0x22: "WIZ_TARGET_HP",
    0x29: "WIZ_STATE_CHANGE",
    0x31: "WIZ_MAGIC_PROCESS",
    0x41: "WIZ_SPEEDHACK_CHECK",
}

USAGE = (
    "Usage:\n"
    "  python3 tools/packet-trace-summary.py <log> [--sid N] [--name X] [--cli]\n"
    "  python3 tools/packet-trace-summary.py --selftest\n"
)


def opcode_name(opcode):
    return OPCODE_NAMES.get(opcode, "UNKNOWN")


def parse_line(line):
    """Parses one log line; returns a dict or None when the line is invalid."""
    parts = line.rstrip("\r\n").split("\t")
    if len(parts) != 7:
        return None

    t_ms, sid, name, zone, opcode_hex, length, payload = parts
    try:
        record = {
            "t": int(t_ms),
            "sid": int(sid),
            "name": name,
            "zone": int(zone),
            "opcode": int(opcode_hex, 16),
            "len": int(length),
            "payload": b"" if payload == "-" else bytes.fromhex(payload),
        }
    except ValueError:
        return None

    if record["len"] < 0:
        return None
    return record


def intervals(times):
    return [times[i + 1] - times[i] for i in range(len(times) - 1)]


def analyze(rows):
    """Returns (times per opcode, magic sub-opcode counts, move packets per second)."""
    per_opcode = {}
    magic_sub = {}
    per_second = {}
    for row in rows:
        opcode = row["opcode"]
        per_opcode.setdefault(opcode, []).append(row["t"])
        if opcode == 0x31 and row["payload"]:
            sub = row["payload"][0]
            magic_sub[sub] = magic_sub.get(sub, 0) + 1
        if opcode == 0x06:
            second = row["t"] // 1000
            per_second[second] = per_second.get(second, 0) + 1
    return per_opcode, magic_sub, per_second


def format_intervals(stats_out, times):
    if len(times) < 2:
        stats_out.write(" interval_ms avg=- median=- min=- max=-")
        return
    values = intervals(times)
    stats_out.write(
        " interval_ms avg=%.1f median=%.1f min=%d max=%d"
        % (statistics.mean(values), statistics.median(values), min(values), max(values))
    )


def write_report(rows, out):
    per_opcode, magic_sub, per_second = analyze(rows)
    out.write("total_records: %d\n" % len(rows))

    out.write("opcode summary:\n")
    for opcode in sorted(per_opcode):
        times = per_opcode[opcode]
        out.write("  %s (%02x): count=%d" % (opcode_name(opcode), opcode, len(times)))
        format_intervals(out, times)
        out.write("\n")

    out.write("WIZ_ATTACK (08) intervals_ms: %s\n" % (intervals(per_opcode.get(0x08, [])),))

    out.write("WIZ_TARGET_HP (22) intervals_ms: %s\n" % (intervals(per_opcode.get(0x22, [])),))

    out.write("WIZ_MAGIC_PROCESS (31) sub_opcode counts:\n")
    if magic_sub:
        for sub in sorted(magic_sub):
            out.write("  %02x: %d\n" % (sub, magic_sub[sub]))
    else:
        out.write("  (none)\n")

    out.write("WIZ_MOVE (06) packets_per_second:")
    if per_second:
        counts = list(per_second.values())
        out.write(
            " min=%d avg=%.1f max=%d seconds=%d\n"
            % (min(counts), statistics.mean(counts), max(counts), len(counts))
        )
    else:
        out.write(" (none)\n")


def summarize_file(path, sid_filter, name_filter, out):
    parsed = 0
    skipped = 0
    filtered = 0
    rows = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.strip():
                continue
            row = parse_line(line)
            if row is None:
                skipped += 1
                continue
            parsed += 1
            if sid_filter is not None and row["sid"] != sid_filter:
                filtered += 1
                continue
            if name_filter is not None and row["name"] != name_filter:
                filtered += 1
                continue
            rows.append(row)

    out.write("parsed_lines: %d\n" % parsed)
    out.write("skipped_lines: %d\n" % skipped)
    out.write("filtered_out_records: %d\n" % filtered)
    write_report(rows, out)


def load_rows(path):
    """Parses every valid log line; returns (rows, parsed, skipped)."""
    rows = []
    parsed = 0
    skipped = 0
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.strip():
                continue
            row = parse_line(line)
            if row is None:
                skipped += 1
                continue
            parsed += 1
            rows.append(row)
    return rows, parsed, skipped


def percentile(values, p):
    """Nearest-rank percentile: idx = max(ceil(p/100*n) - 1, 0)."""
    if not values:
        return None
    ordered = sorted(values)
    index = max(int(math.ceil(p / 100.0 * len(ordered))) - 1, 0)
    return ordered[index]


def format_stats_full(values):
    if not values:
        return "p5=n/a p25=n/a p50=n/a p75=n/a p95=n/a min=n/a max=n/a"
    return "p5=%d p25=%d p50=%d p75=%d p95=%d min=%d max=%d" % (
        percentile(values, 5),
        percentile(values, 25),
        percentile(values, 50),
        percentile(values, 75),
        percentile(values, 95),
        min(values),
        max(values),
    )


def format_stats_short(values):
    if not values:
        return "n=0 p5=n/a p50=n/a p95=n/a min=n/a max=n/a"
    return "n=%d p5=%d p50=%d p95=%d min=%d max=%d" % (
        len(values),
        percentile(values, 5),
        percentile(values, 50),
        percentile(values, 95),
        min(values),
        max(values),
    )


def top5(values):
    if not values:
        return "n/a"
    counts = {}
    for value in values:
        counts[value] = counts.get(value, 0) + 1
    ordered = sorted(counts.items(), key=lambda item: (-item[1], item[0]))
    return ", ".join("%s:%d" % (value, count) for value, count in ordered[:5])


def select_cli_target(rows, sid_filter, name_filter):
    """Selects the rows to analyse; the busiest sid wins when no filter is given."""
    if sid_filter is not None:
        candidates = [row for row in rows if row["sid"] == sid_filter]
        if name_filter is not None:
            candidates = [row for row in candidates if row["name"] == name_filter]
        return candidates

    if name_filter is not None:
        candidates = [row for row in rows if row["name"] == name_filter]
    else:
        candidates = list(rows)

    if not candidates:
        return []

    counts = {}
    for row in candidates:
        counts[row["sid"]] = counts.get(row["sid"], 0) + 1
    best_sid = max(sorted(counts), key=lambda sid: counts[sid])
    return [row for row in candidates if row["sid"] == best_sid]


def target_name(rows):
    counts = {}
    for row in rows:
        counts[row["name"]] = counts.get(row["name"], 0) + 1
    return max(sorted(counts), key=lambda name: counts[name])


def parse_cli_records(rows):
    """Unpacks payloads per docs/03 section 14; returns (records, bad_len)."""
    records = []
    bad_len = 0
    for row in rows:
        opcode = row["opcode"]
        payload = row["payload"]

        if opcode == 0x08:
            if len(payload) != 8:
                bad_len += 1
                continue
            attack_type, result, tid, delaytime, distance = struct.unpack("<BBhhh", payload)
            records.append({
                "t": row["t"], "op": opcode, "type": attack_type, "result": result,
                "tid": tid, "delaytime": delaytime, "distance": distance,
            })
        elif opcode == 0x31:
            # The real client sends 21 bytes (6 data values); the server reads
            # the missing 7th value as 0. Accept 21..23+ bytes.
            if len(payload) < 21:
                bad_len += 1
                continue
            magic_op, skill, caster, target = struct.unpack("<BIhh", payload[:9])
            data_count = min((len(payload) - 9) // 2, 7)
            data = struct.unpack("<%dh" % data_count, payload[9:9 + 2 * data_count])
            data = data + (0,) * (7 - data_count)
            records.append({
                "t": row["t"], "op": opcode, "magic_op": magic_op, "skill": skill,
                "caster": caster, "target": target, "data": data,
            })
        elif opcode == 0x06:
            if len(payload) != 9:
                bad_len += 1
                continue
            x, z, y, speed, echo = struct.unpack("<HHHhB", payload)
            records.append({
                "t": row["t"], "op": opcode, "x": x / 10.0, "z": z / 10.0,
                "y": y / 10.0, "speed": speed, "echo": echo,
            })
        elif opcode == 0x22:
            if len(payload) != 3:
                bad_len += 1
                continue
            uid, echo = struct.unpack("<HB", payload)
            records.append({"t": row["t"], "op": opcode, "uid": uid, "echo": echo})
        elif opcode == 0x41:
            records.append({"t": row["t"], "op": opcode})

    return records, bad_len


def first_after(sorted_times, t):
    index = bisect.bisect_right(sorted_times, t)
    if index < len(sorted_times):
        return sorted_times[index]
    return None


def write_cli_sections(records, out):
    attacks = sorted([row for row in records if row["op"] == 0x08], key=lambda row: row["t"])
    magic = sorted([row for row in records if row["op"] == 0x31], key=lambda row: row["t"])
    moves = sorted([row for row in records if row["op"] == 0x06], key=lambda row: row["t"])
    targethp = sorted([row for row in records if row["op"] == 0x22], key=lambda row: row["t"])
    speedhack = sorted([row for row in records if row["op"] == 0x41], key=lambda row: row["t"])
    effecting = [row for row in magic if row["magic_op"] == 3]

    out.write("== CLI-01 normal saldiri (WIZ_ATTACK) ==\n")
    out.write("ATTACK count=%d\n" % len(attacks))
    out.write("ATTACK interval_ms %s\n" % format_stats_full(intervals([row["t"] for row in attacks])))
    out.write("ATTACK delaytime top5: %s\n" % top5([row["delaytime"] for row in attacks]))
    out.write("ATTACK distance top5: %s\n" % top5([row["distance"] for row in attacks]))
    out.write("ATTACK type top5: %s result top5: %s\n" % (
        top5([row["type"] for row in attacks]), top5([row["result"] for row in attacks])))

    out.write("== CLI-02 skill ile R arasi ==\n")
    attack_times = [row["t"] for row in attacks]
    effecting_times = [row["t"] for row in effecting if row["skill"] < 490000]
    magic_attack_gaps = []
    for row in effecting:
        if row["skill"] >= 490000:
            continue
        t = first_after(attack_times, row["t"])
        if t is not None:
            magic_attack_gaps.append(t - row["t"])
    attack_magic_gaps = []
    for row in attacks:
        t = first_after(effecting_times, row["t"])
        if t is not None:
            attack_magic_gaps.append(t - row["t"])
    out.write("MAGIC->ATTACK gap_ms (skill sonrasi ilk R): %s\n" % format_stats_short(magic_attack_gaps))
    out.write("ATTACK->MAGIC gap_ms (R sonrasi ilk skill): %s\n" % format_stats_short(attack_magic_gaps))

    out.write("== CLI-03 cast suresi (CASTING -> EFFECTING) ==\n")
    out.write("CAST gap_ms per skill:\n")
    cast_gaps = {}
    cancel_gaps = []
    fail_gaps = []
    pending_skill = None
    pending_time = 0
    for row in magic:
        if row["magic_op"] == 1:
            # A player casts one spell at a time: a new CASTING replaces any
            # pending one (the client may have skipped a cancel message).
            pending_skill = row["skill"]
            pending_time = row["t"]
        elif row["magic_op"] == 3:
            if pending_skill is not None and pending_skill == row["skill"]:
                gap = row["t"] - pending_time
                if 0 <= gap <= 10000:
                    cast_gaps.setdefault(row["skill"], []).append(gap)
                pending_skill = None
        elif row["magic_op"] == 6:
            if pending_skill is not None:
                cancel_gaps.append(row["t"] - pending_time)
                pending_skill = None
        elif row["magic_op"] == 4:
            if pending_skill is not None:
                fail_gaps.append(row["t"] - pending_time)
                pending_skill = None
    if cast_gaps:
        for skill in sorted(cast_gaps):
            out.write("  skill=%d %s\n" % (skill, format_stats_short(cast_gaps[skill])))
    else:
        out.write("  (none)\n")
    out.write("MAGIC cancel (opcode 6) count=%d\n" % sum(1 for row in magic if row["magic_op"] == 6))
    out.write("CANCEL gap_ms (CASTING -> opcode 6): %s\n" % format_stats_short(cancel_gaps))
    out.write("FAIL gap_ms (CASTING -> opcode 4): %s\n" % format_stats_short(fail_gaps))

    out.write("== CLI-04 skill tekrar ==\n")
    per_skill = {}
    for row in effecting:
        per_skill.setdefault(row["skill"], []).append(row["t"])
    if per_skill:
        for skill in sorted(per_skill):
            times = per_skill[skill]
            out.write("skill=%d count=%d interval_ms %s\n" % (
                skill, len(times), format_stats_short(intervals(times))))
    else:
        out.write("(none)\n")
    magic_op_counts = {}
    for row in magic:
        magic_op_counts[row["magic_op"]] = magic_op_counts.get(row["magic_op"], 0) + 1
    out.write("MAGIC opcode counts: %s\n" % " ".join(
        "%d:%d" % (opcode, magic_op_counts.get(opcode, 0)) for opcode in (1, 2, 3, 4, 5, 6)))

    out.write("== CLI-06 pot (skill >= 490000, yalnizca EFFECTING) ==\n")
    pots = [row for row in effecting if row["skill"] >= 490000]
    out.write("POT count=%d interval_ms %s\n" % (
        len(pots), format_stats_short(intervals([row["t"] for row in pots]))))
    pot_counts = {}
    for row in pots:
        pot_counts[row["skill"]] = pot_counts.get(row["skill"], 0) + 1
    if pot_counts:
        out.write("POT per skill: %s\n" % ", ".join(
            "%d:%d" % (skill, pot_counts[skill]) for skill in sorted(pot_counts)))
    else:
        out.write("POT per skill: n/a\n")

    out.write("== CLI-05 hareket (WIZ_MOVE) ==\n")
    out.write("MOVE count=%d\n" % len(moves))
    out.write("MOVE interval_ms %s\n" % format_stats_short(intervals([row["t"] for row in moves])))
    out.write("MOVE speed top5: %s\n" % top5([row["speed"] for row in moves]))
    out.write("MOVE echo top5: %s\n" % top5([row["echo"] for row in moves]))
    move_per_second = {}
    for row in moves:
        second = row["t"] // 1000
        move_per_second[second] = move_per_second.get(second, 0) + 1
    if move_per_second:
        counts = list(move_per_second.values())
        out.write("MOVE packets_per_second min=%d avg=%.1f max=%d\n" % (
            min(counts), statistics.mean(counts), max(counts)))
    else:
        out.write("MOVE packets_per_second min=n/a avg=n/a max=n/a\n")
    run_speeds = []
    for previous, current in zip(moves, moves[1:]):
        dt_ms = current["t"] - previous["t"]
        if previous["speed"] == 0 or dt_ms <= 0 or dt_ms > 2000:
            continue
        distance = math.hypot(current["x"] - previous["x"], current["z"] - previous["z"])
        run_speeds.append(distance / (dt_ms / 1000.0))
    if run_speeds:
        out.write("MOVE run_speed_mps median=%.2f p95=%.2f n=%d\n" % (
            statistics.median(run_speeds), percentile(run_speeds, 95), len(run_speeds)))
    else:
        out.write("MOVE run_speed_mps median=n/a p95=n/a n=0\n")

    out.write("== CLI-11 aksiyon hizi ==\n")
    action_per_second = {}
    for row in records:
        if row["op"] == 0x08 or (row["op"] == 0x31 and row["magic_op"] in (1, 3)):
            second = row["t"] // 1000
            action_per_second[second] = action_per_second.get(second, 0) + 1
    if action_per_second:
        counts = list(action_per_second.values())
        out.write("ACTIONS (ATTACK+MAGIC opcode 1 ve 3) per_second max=%d p95=%d\n" % (
            max(counts), percentile(counts, 95)))
    else:
        out.write("ACTIONS (ATTACK+MAGIC opcode 1 ve 3) per_second max=n/a p95=n/a\n")

    out.write("== CLI-12 / Q-02 speedhack check ==\n")
    out.write("SPEEDHACK count=%d interval_ms %s\n" % (
        len(speedhack), format_stats_short(intervals([row["t"] for row in speedhack]))))

    out.write("== Q-18 hedef HP istegi ==\n")
    out.write("TARGETHP count=%d interval_ms %s\n" % (
        len(targethp), format_stats_short(intervals([row["t"] for row in targethp]))))


def write_cli_output(rows, skipped, out):
    """Writes the CLI report for the already-selected target rows."""
    if not rows:
        out.write("cli_target: none\n")
        out.write("skipped_lines: %d\n" % skipped)
        out.write("bad_len: 0\n")
        return 1

    sid = rows[0]["sid"]
    out.write("cli_target: sid=%d name=%s records=%d\n" % (sid, target_name(rows), len(rows)))
    out.write("skipped_lines: %d\n" % skipped)
    records, bad_len = parse_cli_records(rows)
    write_cli_sections(records, out)
    out.write("bad_len: %d\n" % bad_len)
    return 0


def summarize_file_cli(path, sid_filter, name_filter, out):
    rows, _parsed, skipped = load_rows(path)
    candidates = select_cli_target(rows, sid_filter, name_filter)
    return write_cli_output(candidates, skipped, out)


def run_selftest():
    lines = [
        "0\t7\tTestChar\t71\t06\t11\t0f2c00000000000000000000000000",
        "100\t7\tTestChar\t71\t06\t11\t0f2c00010000000000000000000000",
        "200\t7\tTestChar\t71\t06\t11\t0f2c00020000000000000000000000",
        "250\t7\tTestChar\t71\t08\t15\t0000000a0000",
        "400\t7\tTestChar\t71\t08\t15\t0000000a0000",
        "450\t7\tTestChar\t71\t31\t10\t020001000000000000",
        "500\t7\tTestChar\t71\t31\t10\t020001000000000001",
        "550\t7\tTestChar\t71\t31\t10\t030001000000000002",
        "600\t7\tTestChar\t71\t22\t02\t0102",
        "700\t7\tTestChar\t71\t22\t02\t0103",
        "this line is invalid",
    ]

    rows = []
    skipped = 0
    for line in lines:
        row = parse_line(line)
        if row is None:
            skipped += 1
        else:
            rows.append(row)

    assert skipped == 1, skipped
    assert len(rows) == 10, len(rows)

    per_opcode, magic_sub, per_second = analyze(rows)
    assert len(per_opcode[0x06]) == 3, per_opcode[0x06]
    assert intervals(per_opcode[0x06]) == [100, 100]
    assert intervals(per_opcode[0x08]) == [150]
    assert intervals(per_opcode[0x22]) == [100]
    assert magic_sub == {0x02: 2, 0x03: 1}, magic_sub
    assert per_second == {0: 3}, per_second

    def cli_line(t, opcode, payload):
        return "%d\t5\tSelfTest\t71\t%02x\t%d\t%s" % (t, opcode, len(payload), payload.hex())

    attack_payload = struct.pack("<BBhhh", 0, 1, 0x0102, 1010, 50)
    cast_payload = struct.pack("<BIhh7h", 1, 101, 1, 2, 0, 0, 0, 0, 0, 0, 0)
    effect_payload = struct.pack("<BIhh7h", 3, 101, 1, 2, 0, 0, 0, 0, 0, 0, 0)
    pot_payload = struct.pack("<BIhh7h", 3, 490001, 1, 2, 0, 0, 0, 0, 0, 0, 0)
    cancel_payload = struct.pack("<BIhh7h", 6, 101, 1, 2, 0, 0, 0, 0, 0, 0, 0)
    move_payload_1 = struct.pack("<HHHhB", 1000, 0, 0, 150, 1)
    move_payload_2 = struct.pack("<HHHhB", 1100, 0, 0, 150, 1)
    target_hp_payload = struct.pack("<HB", 123, 1)

    cli_lines = []
    for t in (0, 1000, 2000, 3000, 11000, 12000):
        cli_lines.append(cli_line(t, 0x08, attack_payload))
    cli_lines.append(cli_line(10000, 0x31, cast_payload))
    cli_lines.append(cli_line(10300, 0x31, effect_payload))
    cli_lines.append(cli_line(20000, 0x31, cast_payload))
    cli_lines.append(cli_line(20300, 0x31, effect_payload))
    cli_lines.append(cli_line(20400, 0x31, pot_payload))
    cli_lines.append(cli_line(21000, 0x31, pot_payload))
    cli_lines.append(cli_line(30000, 0x31, cancel_payload))
    cli_lines.append(cli_line(40000, 0x06, move_payload_1))
    cli_lines.append(cli_line(42000, 0x06, move_payload_2))
    cli_lines.append(cli_line(50000, 0x22, target_hp_payload))
    cli_lines.append(cli_line(51000, 0x22, target_hp_payload))
    cli_lines.append(cli_line(60000, 0x41, b""))
    cli_lines.append(cli_line(61500, 0x41, b""))
    cli_lines.append(cli_line(70000, 0x08, b"\x00\x01"))

    cli_rows = []
    for line in cli_lines:
        row = parse_line(line)
        assert row is not None, line
        cli_rows.append(row)

    buffer = io.StringIO()
    cli_exit = write_cli_output(cli_rows, 0, buffer)
    cli_text = buffer.getvalue()
    assert cli_exit == 0, cli_exit
    assert "cli_target: sid=5 name=SelfTest records=20" in cli_text, cli_text
    assert "ATTACK count=6" in cli_text, cli_text
    assert (
        "ATTACK interval_ms p5=1000 p25=1000 p50=1000 p75=1000 p95=8000 min=1000 max=8000"
        in cli_text
    ), cli_text
    assert "ATTACK delaytime top5: 1010:6" in cli_text, cli_text
    assert "skill=101 n=2 p5=300 p50=300 p95=300 min=300 max=300" in cli_text, cli_text
    assert "POT count=2" in cli_text, cli_text
    assert "MOVE run_speed_mps median=5.00 p95=5.00 n=1" in cli_text, cli_text
    assert "bad_len: 1" in cli_text, cli_text

    empty_buffer = io.StringIO()
    empty_exit = write_cli_output([], 3, empty_buffer)
    assert empty_exit == 1, empty_exit
    assert "cli_target: none" in empty_buffer.getvalue(), empty_buffer.getvalue()
    assert "skipped_lines: 3" in empty_buffer.getvalue(), empty_buffer.getvalue()

    cancel_case_lines = [
        cli_line(0, 0x31, cast_payload),
        cli_line(500, 0x31, cancel_payload),
        cli_line(3000, 0x31, cast_payload),
        cli_line(3300, 0x31, effect_payload),
        cli_line(6000, 0x31, cast_payload),
        cli_line(6100, 0x31, cast_payload),
        cli_line(6400, 0x31, effect_payload),
    ]
    cancel_rows = []
    for line in cancel_case_lines:
        row = parse_line(line)
        assert row is not None, line
        cancel_rows.append(row)

    cancel_buffer = io.StringIO()
    cancel_exit = write_cli_output(cancel_rows, 0, cancel_buffer)
    cancel_text = cancel_buffer.getvalue()
    assert cancel_exit == 0, cancel_exit
    assert "skill=101 n=2 p5=300 p50=300 p95=300 min=300 max=300" in cancel_text, cancel_text
    assert (
        "CANCEL gap_ms (CASTING -> opcode 6): n=1 p5=500 p50=500 p95=500 min=500 max=500"
        in cancel_text
    ), cancel_text

    print("selftest OK")
    return 0


def main(argv):
    if "--selftest" in argv:
        return run_selftest()

    if not argv or argv[0] in ("-h", "--help"):
        sys.stderr.write(USAGE)
        return 2 if not argv else 0

    path = argv[0]
    sid_filter = None
    name_filter = None
    cli_mode = False
    index = 1
    while index < len(argv):
        arg = argv[index]
        if arg == "--sid" and index + 1 < len(argv):
            sid_filter = int(argv[index + 1])
            index += 2
        elif arg == "--name" and index + 1 < len(argv):
            name_filter = argv[index + 1]
            index += 2
        elif arg == "--cli":
            cli_mode = True
            index += 1
        else:
            sys.stderr.write("unknown argument: %s\n" % arg)
            sys.stderr.write(USAGE)
            return 2

    if cli_mode:
        return summarize_file_cli(path, sid_filter, name_filter, sys.stdout)
    summarize_file(path, sid_filter, name_filter, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
