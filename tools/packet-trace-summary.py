#!/usr/bin/env python3
"""Summarises GameServer FDP_PACKET_TRACE logs.

Each log line is tab separated:
    t_ms<TAB>sid<TAB>name<TAB>zone<TAB>opcode_hex<TAB>len<TAB>payload_hex

Usage:
    python3 tools/packet-trace-summary.py <log> [--sid N] [--name X]
    python3 tools/packet-trace-summary.py --selftest
"""

import statistics
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
    "  python3 tools/packet-trace-summary.py <log> [--sid N] [--name X]\n"
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
    index = 1
    while index < len(argv):
        arg = argv[index]
        if arg == "--sid" and index + 1 < len(argv):
            sid_filter = int(argv[index + 1])
            index += 2
        elif arg == "--name" and index + 1 < len(argv):
            name_filter = argv[index + 1]
            index += 2
        else:
            sys.stderr.write("unknown argument: %s\n" % arg)
            sys.stderr.write(USAGE)
            return 2

    summarize_file(path, sid_filter, name_filter, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
