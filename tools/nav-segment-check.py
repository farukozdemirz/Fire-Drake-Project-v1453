#!/usr/bin/env python3
"""Independent conservative super-cover check of straight segments against a navigation grid.

A segment is BLOCKED when it touches (closed squares, including corner and edge contact) any cell that is not
Walk. Walk = event 1 and part of the main component (the largest 4-connected event==1 component that does not
touch the map edge), exactly the BotCore::NavGrid::Build rule (docs/12 s2).

This is the cross-check oracle for BotCore NavCheckSegment (plan F5-50) and for the 2026-10-02 wall-check
investigation (docs/reports/degerlendirme-2026-10-02-ek.md). Pure Python, standard library only.

Usage:
    python3 tools/nav-segment-check.py [--navgrid build/nav/zone71.navgrid] < segments.txt
    python3 tools/nav-segment-check.py --selftest

segments.txt: one segment per line, "x0 z0 x1 z1" in world metres (the grid unit is 4 m, cell = floor(w / 4)).
Output per line: "OK" or "BLOCKED cell=(cx,cz)". Exit code 0 always (selftest: 0 pass, 1 fail).
"""

import argparse
import array
import math
import struct
import sys

EPS = 1e-9


def load_navgrid(path):
    data = open(path, "rb").read()
    magic, n, unit = struct.unpack_from("<8sif", data, 0)
    if magic != b"FDPNAV01" or len(data) != 16 + 6 * n * n:
        raise SystemExit("bad navgrid file: %s" % path)
    ev = array.array("h")
    ev.frombytes(data[16:16 + 2 * n * n])
    return n, unit, ev


def walk_mask(n, ev):
    """events -> main-component Walk mask (same rule as NavGrid::Build)."""
    comp = [-1] * (n * n)
    sizes = []
    edge = []
    for sx in range(n):
        for sz in range(n):
            s = sx * n + sz
            if ev[s] != 1 or comp[s] >= 0:
                continue
            label = len(sizes)
            sizes.append(0)
            edge.append(False)
            comp[s] = label
            stack = [s]
            while stack:
                cur = stack.pop()
                cx, cz = divmod(cur, n)
                sizes[label] += 1
                if cx in (0, n - 1) or cz in (0, n - 1):
                    edge[label] = True
                for nx, nz in ((cx + 1, cz), (cx - 1, cz), (cx, cz + 1), (cx, cz - 1)):
                    if 0 <= nx < n and 0 <= nz < n:
                        ni = nx * n + nz
                        if ev[ni] == 1 and comp[ni] < 0:
                            comp[ni] = label
                            stack.append(ni)
    best, best_size = -1, 0
    for label, size in enumerate(sizes):
        if not edge[label] and size > best_size:
            best, best_size = label, size
    return [1 if (best >= 0 and comp[i] == best) else 0 for i in range(n * n)]


def touched_cells(x0, z0, x1, z1, unit):
    """All cells whose closed square the segment touches (world metres in)."""
    x0, z0, x1, z1 = x0 / unit, z0 / unit, x1 / unit, z1 / unit
    cells = set()

    def on_line(v):
        return abs(v - round(v)) < EPS

    def around(px, pz):
        cx, cz = math.floor(px), math.floor(pz)
        rx, rz = round(px), round(pz)
        ox, oz = on_line(px), on_line(pz)
        if ox and oz:
            for dx in (-1, 0):
                for dz in (-1, 0):
                    cells.add((rx + dx, rz + dz))
        elif ox:
            cells.add((rx - 1, cz))
            cells.add((rx, cz))
        elif oz:
            cells.add((cx, rz - 1))
            cells.add((cx, rz))
        else:
            cells.add((cx, cz))

    around(x0, z0)
    around(x1, z1)
    dx, dz = x1 - x0, z1 - z0
    sx = 1 if dx > 0 else (-1 if dx < 0 else 0)
    sz = 1 if dz > 0 else (-1 if dz < 0 else 0)
    cx = round(x0) - 1 if (sx < 0 and on_line(x0)) else math.floor(x0)
    cz = round(z0) - 1 if (sz < 0 and on_line(z0)) else math.floor(z0)
    ex = round(x1) - 1 if (sx < 0 and on_line(x1)) else math.floor(x1)
    ez = round(z1) - 1 if (sz < 0 and on_line(z1)) else math.floor(z1)
    inf = float("inf")
    t_dx = 1.0 / abs(dx) if sx else inf
    t_dz = 1.0 / abs(dz) if sz else inf
    t_mx = ((cx + 1) - x0) / dx if sx > 0 else ((x0 - cx) / -dx if sx < 0 else inf)
    t_mz = ((cz + 1) - z0) / dz if sz > 0 else ((z0 - cz) / -dz if sz < 0 else inf)
    guard = 0
    while not (cx == ex and cz == ez) and guard < 100000:
        guard += 1
        cells.add((cx, cz))
        if abs(t_mx - t_mz) < EPS:
            cells.add((cx + sx, cz))
            cells.add((cx, cz + sz))
            cx += sx
            cz += sz
            t_mx += t_dx
            t_mz += t_dz
        elif t_mx < t_mz:
            cx += sx
            t_mx += t_dx
        else:
            cz += sz
            t_mz += t_dz
    cells.add((cx, cz))
    return cells


def check(n, unit, walk, x0, z0, x1, z1):
    for (cx, cz) in sorted(touched_cells(x0, z0, x1, z1, unit)):
        if not (0 <= cx < n and 0 <= cz < n) or not walk[cx * n + cz]:
            return (cx, cz)
    return None


def selftest():
    # 8x8 grid, open interior 1..6, one blocked cell at (3,3); unit 4 m
    n, unit = 8, 4.0
    ev = array.array("h", [0] * (n * n))
    for x in range(1, 7):
        for z in range(1, 7):
            ev[x * n + z] = 1
    ev[3 * n + 3] = 0
    walk = walk_mask(n, ev)
    cases = [
        ((6.0, 6.0, 26.0, 6.0), None, "row z=1 open"),
        ((6.0, 14.0, 26.0, 14.0), (3, 3), "straight through the blocked cell"),
        ((10.0, 10.0, 18.0, 18.0), (3, 3), "diagonal through the blocked cell centre"),
        ((8.0, 20.0, 16.0, 12.0), (3, 3), "diagonal through the vertex shared with the blocked cell"),
        ((12.0, 8.0, 12.0, 20.0), (3, 3), "runs exactly along the blocked cell's left edge"),
        ((11.99, 8.0, 11.99, 20.0), None, "1 cm left of the blocked cell's left edge"),
        ((6.0, 6.0, 6.0, 6.0), None, "zero length"),
        ((26.0, 26.0, 6.0, 6.0), (3, 3), "reverse direction, same verdict"),
        ((2.0, 6.0, 26.0, 6.0), (0, 1), "starts in the blocked border cell"),
    ]
    ok = True
    for seg, expect, label in cases:
        got = check(n, unit, walk, *seg)
        rev = check(n, unit, walk, seg[2], seg[3], seg[0], seg[1])
        if (got is None) != (expect is None) or (expect is not None and got != expect):
            ok = False
            print("FAIL", label, "expected", expect, "got", got)
        if (got is None) != (rev is None):
            ok = False
            print("FAIL symmetry", label, got, rev)
    print("selftest", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--navgrid", default="build/nav/zone71.navgrid")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        sys.exit(selftest())
    n, unit, ev = load_navgrid(args.navgrid)
    walk = walk_mask(n, ev)
    for line in sys.stdin:
        parts = line.split()
        if len(parts) != 4:
            continue
        x0, z0, x1, z1 = (float(p) for p in parts)
        hit = check(n, unit, walk, x0, z0, x1, z1)
        print("OK" if hit is None else "BLOCKED cell=(%d,%d)" % hit)


if __name__ == "__main__":
    main()
