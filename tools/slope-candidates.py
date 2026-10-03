#!/usr/bin/env python3
"""Lists walkable slope sites of a navigation grid for the human slope-calibration test (T-NAV-02).

Reads the grid file produced by tools/nav-export.py (format: BotCore/NavGrid.h / plans/F5-01 section 5.1).
For each slope band it picks straight 3-cell runs (12 m at 4 m cells) whose height rises monotonically along
x or z with a slope inside the band, so a human can walk up the run in the client and see whether the
character climbs, stalls or slides. Read-only; no server, DB or client access.

slope = |dh| / unit (dh between two 4-neighbour cells). docs/12 s3: an edge is blocked when |dh| > P-NAV-MAX-STEP
(2.5 m = slope 0.625 [A], or 4 m = slope 1.0), so the bands bracket those two values.

Usage:
    python3 tools/slope-candidates.py [--navgrid PATH] [--per-band 3] [--near X,Z] [--csv out.csv]
    python3 tools/slope-candidates.py --selftest
Default navgrid: <repo>/build/nav/zone71.navgrid. Default --near: map centre (1024,1024) (sites closest first).
"""

import argparse
import math
import os
import struct
import sys
from array import array

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_GRID = os.path.join(ROOT, "build", "nav", "zone71.navgrid")
BANDS = [(0.30, 0.45), (0.45, 0.60), (0.60, 0.70), (0.70, 0.85), (0.85, 1.10), (1.10, 9.99)]
LANDMARKS = {"bowl_centre": (1024.0, 1024.0), "karus_gate": (1375.0, 1085.0), "arena_a": (1274.0, 890.0), "elmorad_respawn": (630.0, 920.0)}


def load(path):
    data = open(path, "rb").read()
    if data[:8] != b"FDPNAV01":
        raise ValueError("bad magic")
    n, unit = struct.unpack_from("<if", data, 8)
    if len(data) != 16 + 6 * n * n:
        raise ValueError("bad size")
    events = array("h"); events.frombytes(data[16:16 + 2 * n * n])
    heights = array("f"); heights.frombytes(data[16 + 2 * n * n:])
    return n, unit, events, heights


def walk_mask(n, events):
    """Largest 4-connected component of event==1 cells that does not touch the map edge (NavGrid::Build, without clearance)."""
    seen = bytearray(n * n)
    best, best_cells = None, 0
    for start in range(n * n):
        if events[start] != 1 or seen[start]:
            continue
        comp, stack, touches = [], [start], False
        seen[start] = 1
        while stack:
            c = stack.pop(); comp.append(c)
            x, z = divmod(c, n)
            if x == 0 or z == 0 or x == n - 1 or z == n - 1:
                touches = True
            for nx, nz in ((x + 1, z), (x - 1, z), (x, z + 1), (x, z - 1)):
                if 0 <= nx < n and 0 <= nz < n:
                    nc = nx * n + nz
                    if events[nc] == 1 and not seen[nc]:
                        seen[nc] = 1; stack.append(nc)
        if not touches and len(comp) > best_cells:
            best, best_cells = comp, len(comp)
    mask = bytearray(n * n)
    for c in best or []:
        mask[c] = 1
    return mask, best_cells


def runs(n, unit, heights, mask, lo, hi):
    """3-cell straight runs, monotonic rise, every step slope in [lo, hi). Returns (lower_x, lower_z, upper_x, upper_z, h0, h1, slope)."""
    out = []
    for x in range(n):
        for z in range(n):
            for dx, dz in ((1, 0), (0, 1)):
                cells = [(x + dx * k, z + dz * k) for k in range(4)]   # 4 cells -> 3 steps
                if not all(0 <= cx < n and 0 <= cz < n and mask[cx * n + cz] for cx, cz in cells):
                    continue
                hs = [heights[cx * n + cz] for cx, cz in cells]
                steps = [hs[i + 1] - hs[i] for i in range(3)]
                if all(s > 0 for s in steps) or all(s < 0 for s in steps):
                    sl = [abs(s) / unit for s in steps]
                    if all(lo <= s < hi for s in sl):
                        a, b = (cells[0], cells[3]) if steps[0] > 0 else (cells[3], cells[0])
                        h0 = heights[a[0] * n + a[1]]; h1 = heights[b[0] * n + b[1]]
                        out.append((a[0] * unit, a[1] * unit, b[0] * unit, b[1] * unit, h0, h1, sum(sl) / 3))
    return out


def pick(cands, near, k):
    cands.sort(key=lambda c: math.hypot(c[0] - near[0], c[1] - near[1]))
    chosen = []
    for c in cands:           # keep sites at least 40 m apart so they are different places
        if all(math.hypot(c[0] - o[0], c[1] - o[1]) >= 40.0 for o in chosen):
            chosen.append(c)
        if len(chosen) == k:
            break
    return chosen


def nearest_landmark(x, z):
    name, d = min(((k, math.hypot(x - v[0], z - v[1])) for k, v in LANDMARKS.items()), key=lambda t: t[1])
    return name, d


def selftest():
    n, unit = 12, 4.0
    events = array("h", [1] * (n * n))
    for i in range(n):                       # edge ring = 0 so the interior is the main component
        for e in (i * n, i * n + n - 1, i, (n - 1) * n + i):
            events[e] = 0
    heights = array("f", [0.0] * (n * n))
    for x in range(2, 10):                   # ramp along x: 2.0 m per 4 m cell = slope 0.5
        for z in range(2, 10):
            heights[x * n + z] = (x - 2) * 2.0
    mask, cells = walk_mask(n, events)
    ok = cells == 10 * 10 and sum(mask) == 100
    found = runs(n, unit, heights, mask, 0.45, 0.60)
    ok = ok and len(found) > 0 and all(abs(c[6] - 0.5) < 1e-6 for c in found)
    low = [c for c in found if c[0] < c[2]]
    ok = ok and len(low) > 0
    none = runs(n, unit, heights, mask, 0.85, 1.10)
    ok = ok and len(none) == 0
    print("selftest OK" if ok else "selftest FAILED (cells=%d runs=%d)" % (cells, len(found)))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--navgrid", default=DEFAULT_GRID)
    ap.add_argument("--per-band", type=int, default=3)
    ap.add_argument("--near", default="1024,1024")
    ap.add_argument("--csv")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    n, unit, events, heights = load(a.navgrid)
    mask, cells = walk_mask(n, events)
    near = tuple(float(t) for t in a.near.split(","))
    print("GRID n=%d unit=%.1f walk_cells=%d (NavGrid main component, without clearance; compare docs/12: 88508)" % (n, unit, cells))
    rows = []
    for lo, hi in BANDS:
        sites = pick(runs(n, unit, heights, mask, lo, hi), near, a.per_band)
        label = "%.2f-%.2f" % (lo, hi) if hi < 9 else ">=%.2f" % lo
        if not sites:
            print("BAND %s: no 3-cell monotonic run found" % label)
        for i, (x0, z0, x1, z1, h0, h1, sl) in enumerate(sites, 1):
            lm, d = nearest_landmark(x0, z0)
            print("SITE band=%s #%d  lower=(%.0f,%.0f) h=%.1f  ->  upper=(%.0f,%.0f) h=%.1f  slope=%.2f  length=%.0f m  near=%s(%.0f m)" % (
                label, i, x0, z0, h0, x1, z1, h1, sl, math.hypot(x1 - x0, z1 - z0), lm, d))
            rows.append((label, i, x0, z0, h0, x1, z1, h1, sl, lm, d))
    if a.csv:
        with open(a.csv, "w", encoding="utf-8") as f:
            f.write("band,idx,lower_x,lower_z,lower_h,upper_x,upper_z,upper_h,slope,near,near_m\n")
            for r in rows:
                f.write("%s,%d,%.0f,%.0f,%.2f,%.0f,%.0f,%.2f,%.3f,%s,%.0f\n" % r)
    return 0


if __name__ == "__main__":
    sys.exit(main())
