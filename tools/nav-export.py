#!/usr/bin/env python3
"""Export a zone SMD terrain to the navigation grid binary read by BotCore/NavGrid.h.

Format (little-endian, unaligned, consecutive; see plans/F5-01 section 5.1):
    0             8 bytes   magic ASCII "FDPNAV01"
    8             int32     n (vertices per side; zone 71: 513)
    12            float32   unit (metres per cell; zone 71: 4.0)
    16            2*n*n     int16 events[n*n], index x*n + z
    16 + 2*n*n    4*n*n     float32 heights[n*n], same index

Usage:
    python3 tools/nav-export.py [--map-dir DIR] [--map NAME] [--out PATH]
    python3 tools/nav-export.py --selftest

Defaults: map dir $FDP_MAP_DIR or /mnt/c/dev/fdp/server/Map, map
freezone_a_20050718.smd, out <repo>/build/nav/zone71.navgrid.
"""

import argparse
import array
import os
import struct
import sys
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(ROOT, "docs", "appendix", "tools"))
from smd_parse import parse  # noqa: E402  (path set above)

MAGIC = b"FDPNAV01"


def write_navgrid(path, n, unit, events, heights):
    """Write the section 5.1 binary; creates the parent directory when missing."""
    parent = os.path.dirname(os.path.abspath(path))
    if parent:
        os.makedirs(parent, exist_ok=True)

    ev = array.array("h", events)
    ht = array.array("f", heights)
    if sys.byteorder != "little":
        ev.byteswap()
        ht.byteswap()

    with open(path, "wb") as f:
        f.write(struct.pack("<8sif", MAGIC, n, unit))
        f.write(ev.tobytes())
        f.write(ht.tobytes())


def read_navgrid(path):
    """Read back the section 5.1 binary; raises ValueError on a malformed file."""
    data = open(path, "rb").read()
    if len(data) < 16:
        raise ValueError("file shorter than header (%d bytes)" % len(data))

    magic, n, unit = struct.unpack_from("<8sif", data, 0)
    if magic != MAGIC:
        raise ValueError("bad magic %r" % magic)
    if n < 2:
        raise ValueError("bad n %d" % n)

    expected = 16 + 6 * n * n
    if len(data) != expected:
        raise ValueError("size %d != expected %d" % (len(data), expected))

    ev = array.array("h")
    ev.frombytes(data[16:16 + 2 * n * n])
    ht = array.array("f")
    ht.frombytes(data[16 + 2 * n * n:16 + 6 * n * n])
    if sys.byteorder != "little":
        ev.byteswap()
        ht.byteswap()

    return {"n": n, "unit": unit, "events": ev, "heights": ht, "bytes": data}


def main_component_cells(n, events):
    """Size of the largest 4-connected event==1 component away from the map edge.

    Mirrors the BotCore::NavGrid::Build rule (docs/12 section 2); iterative BFS, no recursion.
    """
    seen = bytearray(n * n)
    best = 0

    for sx in range(n):
        for sz in range(n):
            start = sx * n + sz
            if events[start] != 1 or seen[start]:
                continue

            seen[start] = 1
            stack = [start]
            size = 0
            edge = False
            while stack:
                cur = stack.pop()
                cx = cur // n
                cz = cur % n
                size += 1
                if cx == 0 or cx == n - 1 or cz == 0 or cz == n - 1:
                    edge = True
                if cx + 1 < n:
                    ni = cur + n
                    if events[ni] == 1 and not seen[ni]:
                        seen[ni] = 1
                        stack.append(ni)
                if cx - 1 >= 0:
                    ni = cur - n
                    if events[ni] == 1 and not seen[ni]:
                        seen[ni] = 1
                        stack.append(ni)
                if cz + 1 < n:
                    ni = cur + 1
                    if events[ni] == 1 and not seen[ni]:
                        seen[ni] = 1
                        stack.append(ni)
                if cz - 1 >= 0:
                    ni = cur - 1
                    if events[ni] == 1 and not seen[ni]:
                        seen[ni] = 1
                        stack.append(ni)

            if not edge and size > best:
                best = size

    return best


def summarize(path, bytes_len, n, unit, events, heights, data):
    events0 = sum(1 for v in events if v == 0)
    events1 = sum(1 for v in events if v == 1)
    hmin = min(heights)
    hmax = max(heights)
    main = main_component_cells(n, events)
    crc = zlib.crc32(data) & 0xFFFFFFFF
    print("NAVGRID file=%s bytes=%d n=%d unit=%.1f events0=%d events1=%d "
          "hmin=%.3f hmax=%.3f main_component=%d crc32=%08x"
          % (path, bytes_len, n, unit, events0, events1, hmin, hmax, main, crc))


def run_selftest():
    n = 6
    unit = 4.0
    events = [1] * (n * n)
    for x in range(n):
        for z in range(n):
            if x == 0 or x == n - 1 or z == 0 or z == n - 1:
                events[x * n + z] = 0
    # Wall x=3 splits the interior; the gap (0,2) joins the left half (9 cells, edge touching)
    # to the map border, so the right half (4 cells) is the selected interior component.
    for z in range(1, n - 1):
        events[3 * n + z] = 0
    events[0 * n + 2] = 1
    heights = [float(i) * 0.5 - 3.0 for i in range(n * n)]

    fd, tmp = tempfile.mkstemp(suffix=".navgrid")
    os.close(fd)
    try:
        write_navgrid(tmp, n, unit, events, heights)
        res = read_navgrid(tmp)
        assert res["n"] == n
        assert res["unit"] == unit
        assert list(res["events"]) == events
        assert all(abs(a - b) < 1e-4 for a, b in zip(res["heights"], heights))
        main = main_component_cells(n, events)
        assert main == 4, "expected 4, got %d" % main
    finally:
        os.remove(tmp)


def main():
    ap = argparse.ArgumentParser(description="Export an SMD terrain to a .navgrid file.")
    ap.add_argument("--map-dir", default=os.environ.get("FDP_MAP_DIR", "/mnt/c/dev/fdp/server/Map"))
    ap.add_argument("--map", default="freezone_a_20050718.smd")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "nav", "zone71.navgrid"))
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        run_selftest()
        print("SELFTEST OK")
        return 0

    path = os.path.join(args.map_dir, args.map)
    try:
        res = parse(path, load_warps=False)
    except Exception as e:
        print("NAVGRID FAILED: %s" % e)
        return 1

    n = res["m_nMapSize"]
    unit = res["m_fUnitDist"]
    events = res["events"]
    heights = res["height"]

    try:
        write_navgrid(args.out, n, unit, events, heights)
        back = read_navgrid(args.out)
    except Exception as e:
        print("NAVGRID FAILED: %s" % e)
        return 1

    if back["n"] != n or back["unit"] != unit:
        print("NAVGRID FAILED: read-back header mismatch")
        return 1

    summarize(args.out, len(back["bytes"]), n, unit, events, heights, back["bytes"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
