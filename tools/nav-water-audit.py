#!/usr/bin/env python3
"""Water / blocker audit helpers for the zone 71 navigation grid (F5-60).

basins:       independent re-implementation of the nav_measure `water` grid/basin counts (cross-check oracle).
probe-client: list water-related names found in the client data files (facts only, no interpretation).

Usage:
    python3 tools/nav-water-audit.py basins [--navgrid build/nav/zone71.navgrid] [--t -1.0] [--min 200]
    python3 tools/nav-water-audit.py probe-client [--client-dir /mnt/c/dev/fdp/Client]
    python3 tools/nav-water-audit.py --selftest

`basins` prints the same WATER_GRID / WATER_HEIGHT / WATER_BASIN / WATER_BASIN_SUMMARY keys and numbers
as nav_measure `water` (plan F5-60 K3): the percentile index is int(len * q) over the sorted Walk
heights, identical in both implementations. This tool adds no navigation layer and makes no verdict.
"""

import argparse
import array
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_navgrid(path):
    data = open(path, "rb").read()
    magic, n, unit = struct.unpack_from("<8sif", data, 0)
    if magic != b"FDPNAV01" or len(data) != 16 + 6 * n * n:
        raise SystemExit("bad navgrid file: %s" % path)
    ev = array.array("h")
    ev.frombytes(data[16:16 + 2 * n * n])
    ht = array.array("f")
    ht.frombytes(data[16 + 2 * n * n:16 + 6 * n * n])
    return n, unit, ev, ht


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


def label_components(n, in_set):
    """4-connected labels over in_set, scan x-outer/z-inner -> (label, sizes, touches_edge)."""
    label = [-1] * (n * n)
    sizes = []
    edge = []
    for sx in range(n):
        for sz in range(n):
            s = sx * n + sz
            if not in_set[s] or label[s] >= 0:
                continue
            cid = len(sizes)
            sizes.append(0)
            edge.append(False)
            label[s] = cid
            stack = [s]
            while stack:
                cur = stack.pop()
                cx, cz = divmod(cur, n)
                sizes[cid] += 1
                if cx in (0, n - 1) or cz in (0, n - 1):
                    edge[cid] = True
                for nx, nz in ((cx + 1, cz), (cx - 1, cz), (cx, cz + 1), (cx, cz - 1)):
                    if 0 <= nx < n and 0 <= nz < n:
                        ni = nx * n + nz
                        if in_set[ni] and label[ni] < 0:
                            label[ni] = cid
                            stack.append(ni)
    return label, sizes, edge


def height_pct(sorted_h, q):
    if not sorted_h:
        return 0.0
    idx = int(len(sorted_h) * q)
    if idx >= len(sorted_h):
        idx = len(sorted_h) - 1
    return sorted_h[idx]


def basins_data(n, unit, ev, ht, t=-1.0, min_cells=200):
    """Grid/height/basin statistics; pure data, no printing (shared by command and selftest)."""
    events0 = sum(1 for v in ev if v == 0)
    events1 = sum(1 for v in ev if v == 1)
    walk = walk_mask(n, ev)
    walk_count = sum(walk)

    # pockets: event 1 cells that are not Walk, 4-connected, edge-touching components dropped
    event_set = [1 if (ev[i] == 1 and not walk[i]) else 0 for i in range(n * n)]
    plabel, psize, pedge = label_components(n, event_set)
    pockets = 0
    pocket_cells = 0
    for cid, sz in enumerate(psize):
        if not pedge[cid]:
            pockets += 1
            pocket_cells += sz
    pocket_adj = 0
    for x in range(n):
        for z in range(n):
            cid = plabel[x * n + z]
            if cid < 0 or pedge[cid]:
                continue
            adj = False
            for dx in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    if dx == 0 and dz == 0:
                        continue
                    nx, nz = x + dx, z + dz
                    if 0 <= nx < n and 0 <= nz < n and walk[nx * n + nz]:
                        adj = True
            if adj:
                pocket_adj += 1

    hs = sorted(ht[i] for i in range(n * n) if walk[i])
    below = [0, 0, 0, 0, 0]
    for h in hs:
        if h < 0.0:
            below[0] += 1
        if h < -1.0:
            below[1] += 1
        if h < -2.0:
            below[2] += 1
        if h < -4.0:
            below[3] += 1
        if h < -6.0:
            below[4] += 1

    # basins: Walk cells below t, 4-connected; only components with >= min_cells cells
    basin_set = [1 if (walk[x * n + z] and ht[x * n + z] < t) else 0
                 for x in range(n) for z in range(n)]
    blabel, bsize, bedge = label_components(n, basin_set)
    basins = []
    for cid, sz in enumerate(bsize):
        if sz < min_cells:
            continue
        hmin = 1e30
        hmax = -1e30
        xmin, xmax, zmin, zmax = n, -1, n, -1
        edge0 = 0
        for x in range(n):
            for z in range(n):
                if blabel[x * n + z] != cid:
                    continue
                h = ht[x * n + z]
                hmin = min(hmin, h)
                hmax = max(hmax, h)
                xmin = min(xmin, x)
                xmax = max(xmax, x)
                zmin = min(zmin, z)
                zmax = max(zmax, z)
                e0 = False
                for dx in (-1, 0, 1):
                    for dz in (-1, 0, 1):
                        if dx == 0 and dz == 0:
                            continue
                        nx, nz = x + dx, z + dz
                        if 0 <= nx < n and 0 <= nz < n and ev[nx * n + nz] == 0:
                            e0 = True
                if e0:
                    edge0 += 1
        basins.append({"cells": sz, "hmin": hmin, "hmax": hmax,
                       "xmin": xmin * unit, "xmax": xmax * unit,
                       "zmin": zmin * unit, "zmax": zmax * unit, "edge0": edge0})

    return {
        "grid": {"n": n, "unit": unit, "events0": events0, "events1": events1,
                 "walk": walk_count, "event1_not_walk": events1 - walk_count,
                 "pockets": pockets, "pocket_cells": pocket_cells, "pocket_adj": pocket_adj},
        "height": {"hmin": hs[0] if hs else 0.0, "p1": height_pct(hs, 0.01), "p5": height_pct(hs, 0.05),
                   "p50": height_pct(hs, 0.50), "p95": height_pct(hs, 0.95), "hmax": hs[-1] if hs else 0.0,
                   "below0": below[0], "below1": below[1], "below2": below[2],
                   "below4": below[3], "below6": below[4]},
        "basins": basins,
        "summary": {"t": t, "min_cells": min_cells, "count": len(basins),
                    "cells": sum(b["cells"] for b in basins)},
    }


def print_basins(d):
    g = d["grid"]
    print("WATER_GRID n=%d unit=%.1f events0=%d events1=%d walk=%d event1_not_walk=%d pockets=%d pocket_cells=%d pocket_cells_adjacent_to_walk=%d"
          % (g["n"], g["unit"], g["events0"], g["events1"], g["walk"], g["event1_not_walk"],
             g["pockets"], g["pocket_cells"], g["pocket_adj"]))
    h = d["height"]
    print("WATER_HEIGHT walk_hmin=%.2f walk_p1=%.2f walk_p5=%.2f walk_p50=%.2f walk_p95=%.2f walk_hmax=%.2f walk_below_0=%d walk_below_m1=%d walk_below_m2=%d walk_below_m4=%d walk_below_m6=%d"
          % (h["hmin"], h["p1"], h["p5"], h["p50"], h["p95"], h["hmax"],
             h["below0"], h["below1"], h["below2"], h["below4"], h["below6"]))
    for i, b in enumerate(d["basins"]):
        print("WATER_BASIN id=%d cells=%d hmin=%.2f hmax=%.2f x=[%.1f,%.1f] z=[%.1f,%.1f] edge0_touch=%d"
              % (i, b["cells"], b["hmin"], b["hmax"], b["xmin"], b["xmax"], b["zmin"], b["zmax"], b["edge0"]))
    s = d["summary"]
    print("WATER_BASIN_SUMMARY t=%.2f min_cells=%d count=%d cells=%d"
          % (s["t"], s["min_cells"], s["count"], s["cells"]))


def probe_client(client_dir):
    """List water-related ASCII names in the client data files. Facts only, no interpretation."""
    zones = os.path.join(client_dir, "Zones")
    targets = []
    if os.path.isdir(zones):
        for fn in sorted(os.listdir(zones)):
            if fn.startswith("freezone_a."):
                targets.append(os.path.join(zones, fn))
    pattern = re.compile(rb"[\x20-\x7e]{5,}")
    kw = re.compile(rb"water|lake|river|pond", re.IGNORECASE)
    for path in targets:
        try:
            data = open(path, "rb").read()
        except OSError as e:
            print("PROBE error file=%s: %s" % (path, e))
            continue
        rel = os.path.relpath(path, client_dir)
        for m in pattern.finditer(data):
            if kw.search(m.group()):
                print("PROBE file=%s offset=%d name=%s" % (rel, m.start(), m.group().decode("ascii", "replace")))

    for sub in ("Misc/river", "Object"):
        d = os.path.join(client_dir, sub)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if re.search(r"water|lake|river|pond", fn, re.IGNORECASE):
                print("PROBE_NAME dir=%s name=%s" % (sub, fn))


def selftest():
    n = 12
    unit = 4.0
    ev = array.array("h", [0] * (n * n))
    ht = array.array("f", [0.0] * (n * n))
    for x in range(2, 9):                        # main component: 7x7 = 49 cells, interior
        for z in range(2, 9):
            ev[x * n + z] = 1
    ev[0 * n + 5] = 1                            # edge-touching component, ignored
    ev[9 * n + 9] = 1                            # pocket cell, diagonal contact with main (8,8)
    ev[10 * n + 9] = 1                           # pocket cell
    for (x, z) in ((3, 3), (3, 4), (3, 5), (4, 3)):
        ht[x * n + z] = -2.0                     # basin: 4 cells
    ht[6 * n + 6] = -3.0                         # 2-cell dip below the min filter
    ht[6 * n + 7] = -3.0

    d = basins_data(n, unit, ev, ht, t=-1.0, min_cells=3)
    g, h, s = d["grid"], d["height"], d["summary"]
    assert g["events0"] == 92 and g["events1"] == 52, g
    assert g["walk"] == 49 and g["event1_not_walk"] == 3, g
    assert g["pockets"] == 1 and g["pocket_cells"] == 2 and g["pocket_adj"] == 1, g
    assert h["hmin"] == -3.0 and h["hmax"] == 0.0, h
    assert h["p1"] == -3.0 and h["p5"] == -2.0 and h["p50"] == 0.0 and h["p95"] == 0.0, h
    assert h["below0"] == 6 and h["below1"] == 6 and h["below2"] == 2, h
    assert h["below4"] == 0 and h["below6"] == 0, h
    assert s["count"] == 1 and s["cells"] == 4, s
    b = d["basins"][0]
    assert b["cells"] == 4 and b["hmin"] == -2.0 and b["hmax"] == -2.0, b
    assert (b["xmin"], b["xmax"], b["zmin"], b["zmax"]) == (12.0, 16.0, 12.0, 20.0), b
    assert b["edge0"] == 0, b

    d2 = basins_data(n, unit, ev, ht, t=-1.0, min_cells=2)
    assert d2["summary"]["count"] == 2 and d2["summary"]["cells"] == 6, d2["summary"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--selftest", action="store_true")
    sub = ap.add_subparsers(dest="cmd")
    b = sub.add_parser("basins")
    b.add_argument("--navgrid", default=os.path.join(ROOT, "build", "nav", "zone71.navgrid"))
    b.add_argument("--t", type=float, default=-1.0)
    b.add_argument("--min", type=int, default=200)
    p = sub.add_parser("probe-client")
    p.add_argument("--client-dir", default="/mnt/c/dev/fdp/Client")
    args = ap.parse_args()

    if args.selftest:
        selftest()
        print("SELFTEST OK")
        return 0
    if args.cmd == "basins":
        n, unit, ev, ht = load_navgrid(args.navgrid)
        print_basins(basins_data(n, unit, ev, ht, args.t, args.min))
        return 0
    if args.cmd == "probe-client":
        probe_client(args.client_dir)
        return 0
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
