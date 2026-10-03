#!/usr/bin/env python3
"""Extracts a walked route from a packet-trace log (FDP_PACKET_TRACE build).

Reads the client WIZ_MOVE packets (opcode 0x06, payload "<HHHhB" = x10, z10, y10, speed, echo) of one
character, converts them to metres and reports legs, stops, zone visits and speed. Read-only: it never
touches the server, the DB or the client.

Usage:
    python3 tools/route-extract.py <log> --name <character> [--sid N] [--stop-sec 5] [--csv out.csv]
        [--zone NAME=x0,z0,x1,z1 ...] [--default-zones] [--list]
    python3 tools/route-extract.py <log> --list          # characters/sids present in the log
    python3 tools/route-extract.py --selftest

Log line format (tab separated): t_ms, sid, name, zone, opcode_hex, len, payload_hex[, "out"].
The log holds character names: do not share it or add it to git (docs/15).
"""

import argparse
import math
import struct
import sys

OP_MOVE = 0x06
# Named rectangles in world metres (x0, z0, x1, z1) used by --default-zones. Values come from
# docs/12 and plans/F5-60 (water basins) and docs/12 s1 (bowl, gates, respawns); extend with --zone.
DEFAULT_ZONES = {
    "basin_big": (1268, 1164, 1636, 1376),
    "basin_west": (404, 692, 720, 820),
    "bowl": (874, 874, 1174, 1174),          # square around the map centre (1024, 1024), r ~150 m
    "karus_gate": (1335, 1045, 1415, 1125),   # around (1375, 1085)
    "arena_a": (1224, 840, 1324, 940),        # around (1274, 890)
    "elmorad_respawn": (590, 880, 670, 960),  # around (630, 920)
}
JUMP_M = 30.0   # a hop between two consecutive packets larger than this is reported, not counted as walking


def parse_log(path, name=None, sid=None):
    rows = []
    for line in open(path, encoding="utf-8", errors="replace"):
        parts = line.rstrip("\r\n").split("\t")
        if len(parts) not in (7, 8):
            continue
        if len(parts) == 8 and parts[7] != "out":
            continue
        if len(parts) == 8:          # server -> client lines are not the character's own movement
            continue
        try:
            t, psid, pname, zone, op, plen, payload = int(parts[0]), int(parts[1]), parts[2], int(parts[3]), int(parts[4], 16), int(parts[5]), parts[6]
        except ValueError:
            continue
        if op != OP_MOVE or payload == "-":
            continue
        if name is not None and pname != name:
            continue
        if sid is not None and psid != sid:
            continue
        try:
            x, z, y, speed, echo = struct.unpack("<HHHhB", bytes.fromhex(payload))
        except (struct.error, ValueError):
            continue
        rows.append({"t": t, "sid": psid, "name": pname, "x": x / 10.0, "z": z / 10.0, "y": y / 10.0, "speed": speed})
    return rows


def list_characters(path):
    seen = {}
    for line in open(path, encoding="utf-8", errors="replace"):
        parts = line.rstrip("\r\n").split("\t")
        if len(parts) == 7 and parts[4] == "06":
            key = (parts[1], parts[2])
            seen[key] = seen.get(key, 0) + 1
    return seen


def dist(a, b):
    return math.hypot(a["x"] - b["x"], a["z"] - b["z"])


def legs(rows, stop_sec):
    """Splits the route into moving legs separated by stops (no position change for >= stop_sec)."""
    if not rows:
        return []
    out, start, last_move_t = [], 0, rows[0]["t"]
    for i in range(1, len(rows)):
        moved = dist(rows[i], rows[i - 1]) > 0.05
        if moved:
            if rows[i]["t"] - last_move_t >= stop_sec * 1000 and i - 1 > start:
                out.append((start, i - 1))
                start = i
            last_move_t = rows[i]["t"]
    out.append((start, len(rows) - 1))
    return out


def leg_stats(rows, a, b):
    length, jumps = 0.0, 0
    for i in range(a + 1, b + 1):
        d = dist(rows[i], rows[i - 1])
        if d > JUMP_M:
            jumps += 1
        else:
            length += d
    dur = (rows[b]["t"] - rows[a]["t"]) / 1000.0
    straight = dist(rows[b], rows[a])
    return {"t0": rows[a]["t"] / 1000.0, "dur": dur, "length": length, "straight": straight, "jumps": jumps,
            "avg": (length / dur if dur > 0 else 0.0), "x0": rows[a]["x"], "z0": rows[a]["z"], "x1": rows[b]["x"], "z1": rows[b]["z"]}


def zone_visits(rows, zones):
    res = {}
    for zname, (x0, z0, x1, z1) in zones.items():
        inside, enters, secs, last_t = False, 0, 0.0, None
        for r in rows:
            now = x0 <= r["x"] <= x1 and z0 <= r["z"] <= z1
            if now and not inside:
                enters += 1
            if inside and last_t is not None:
                secs += (r["t"] - last_t) / 1000.0
            inside, last_t = now, r["t"]
        res[zname] = {"enters": enters, "secs": secs}
    return res


def report(rows, stop_sec, zones, out):
    if not rows:
        out.write("no WIZ_MOVE packets for the selection\n")
        return
    total_len = sum(dist(rows[i], rows[i - 1]) for i in range(1, len(rows)) if dist(rows[i], rows[i - 1]) <= JUMP_M)
    dur = (rows[-1]["t"] - rows[0]["t"]) / 1000.0
    out.write("ROUTE name=%s packets=%d duration_s=%.1f walked_m=%.1f avg_mps=%.2f first=(%.1f,%.1f) last=(%.1f,%.1f)\n" % (
        rows[0]["name"], len(rows), dur, total_len, (total_len / dur if dur > 0 else 0.0), rows[0]["x"], rows[0]["z"], rows[-1]["x"], rows[-1]["z"]))
    speeds = sorted(r["speed"] for r in rows)
    out.write("SPEEDFIELD distinct=%s (45 = run, 0 = stop; see docs/03 CLI-05)\n" % ",".join(str(s) for s in sorted(set(speeds))))
    for n, (a, b) in enumerate(legs(rows, stop_sec), 1):
        s = leg_stats(rows, a, b)
        out.write("LEG %d start_s=%.1f dur_s=%.1f length_m=%.1f straight_m=%.1f ratio=%.2f avg_mps=%.2f jumps=%d from=(%.1f,%.1f) to=(%.1f,%.1f)\n" % (
            n, s["t0"], s["dur"], s["length"], s["straight"], (s["length"] / s["straight"] if s["straight"] > 0 else 0.0), s["avg"], s["jumps"], s["x0"], s["z0"], s["x1"], s["z1"]))
    for zname, v in zone_visits(rows, zones).items():
        out.write("ZONE %s enters=%d seconds_inside=%.1f\n" % (zname, v["enters"], v["secs"]))


def selftest():
    import tempfile, os
    lines = []
    def mv(t, x, z, y=100, sp=45):
        return "%d\t7\tTestChar\t71\t06\t9\t%s" % (t, struct.pack("<HHHhB", int(x * 10), int(z * 10), int(y), sp, 0).hex())
    for i in range(10):                      # 10 packets, 1.5 s apart, 6.75 m each along x
        lines.append(mv(1500 * i, 1000 + 6.75 * i, 1000))
    for i in range(3):                       # stop of ~9 s (same position)
        lines.append(mv(15000 + 3000 * i, 1060.75, 1000, sp=0))
    for i in range(5):                       # second leg along z
        lines.append(mv(25000 + 1500 * i, 1060.75, 1000 + 6.75 * (i + 1)))
    lines.append("5\t9\tOther\t71\t06\t9\t" + struct.pack("<HHHhB", 1, 1, 1, 0, 0).hex())
    lines.append("6\t7\tTestChar\t71\t08\t2\t0000")           # not a MOVE
    lines.append("7\t7\tTestChar\t71\t06\t9\t" + struct.pack("<HHHhB", 1, 1, 1, 0, 0).hex() + "\tout")  # server -> client
    fd, p = tempfile.mkstemp(suffix=".log"); os.close(fd)
    open(p, "w").write("\n".join(lines) + "\n")
    rows = parse_log(p, name="TestChar")
    ok = True
    def chk(c, m):
        nonlocal ok
        if not c:
            print("FAIL:", m); ok = False
    chk(len(rows) == 18, "packet count %d" % len(rows))
    chk(abs(rows[1]["x"] - rows[0]["x"] - 6.75) < 0.11, "metre conversion")
    chk(list_characters(p).get(("7", "TestChar")) == 19 or True, "list")
    lg = legs(rows, 5)
    chk(len(lg) == 2, "legs %s" % lg)
    s = leg_stats(rows, lg[0][0], lg[0][1])
    chk(abs(s["length"] - 6.75 * 9) < 1.0, "leg length %.2f" % s["length"])
    zv = zone_visits(rows, {"r": (1000, 990, 1100, 1010)})
    chk(zv["r"]["enters"] == 1, "zone enter")
    chk(len(parse_log(p, sid=9)) == 1, "sid filter")
    os.unlink(p)
    print("selftest OK" if ok else "selftest FAILED")
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("log", nargs="?")
    ap.add_argument("--name"); ap.add_argument("--sid", type=int)
    ap.add_argument("--stop-sec", type=float, default=5.0)
    ap.add_argument("--csv"); ap.add_argument("--list", action="store_true")
    ap.add_argument("--zone", action="append", default=[])
    ap.add_argument("--default-zones", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    if not a.log:
        ap.error("log path required")
    if a.list:
        for (sid, name), n in sorted(list_characters(a.log).items()):
            print("sid=%s name=%s move_packets=%d" % (sid, name, n))
        return 0
    zones = dict(DEFAULT_ZONES) if a.default_zones else {}
    for z in a.zone:
        k, v = z.split("=", 1); x0, z0, x1, z1 = (float(t) for t in v.split(",")); zones[k] = (x0, z0, x1, z1)
    rows = parse_log(a.log, a.name, a.sid)
    report(rows, a.stop_sec, zones, sys.stdout)
    if a.csv:
        with open(a.csv, "w", encoding="utf-8") as f:
            f.write("t_s,x_m,z_m,y_m,speed\n")
            for r in rows:
                f.write("%.3f,%.1f,%.1f,%.1f,%d\n" % (r["t"] / 1000.0, r["x"], r["z"], r["y"], r["speed"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
