#!/usr/bin/env python3
"""Extra read-only analysis on top of smd_parse.parse(): connectivity of event==1 tiles,
overlap of event==0 tiles with collision sub-cells, and sample lookups. stdlib only."""
import os
import sys
from collections import Counter, deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from smd_parse import parse, MAP_DIR  # noqa: E402


def components(ev, n, val, eight=True):
    """Label connected components of tiles == val inside [0, n-1) x [0, n-1)."""
    m = n - 1
    lab = [0] * (n * n)
    sizes = {}
    cur = 0
    nb = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)] if eight else \
        [(-1, 0), (1, 0), (0, -1), (0, 1)]
    for x0 in range(m):
        for z0 in range(m):
            i0 = x0 * n + z0
            if ev[i0] != val or lab[i0]:
                continue
            cur += 1
            q = deque([(x0, z0)])
            lab[i0] = cur
            cnt = 0
            while q:
                x, z = q.popleft()
                cnt += 1
                for dx, dz in nb:
                    xx, zz = x + dx, z + dz
                    if 0 <= xx < m and 0 <= zz < m:
                        j = xx * n + zz
                        if ev[j] == val and not lab[j]:
                            lab[j] = cur
                            q.append((xx, zz))
            sizes[cur] = cnt
    return lab, sizes


def main():
    fn = sys.argv[1] if len(sys.argv) > 1 else "freezone_a_20050718.smd"
    res = parse(os.path.join(MAP_DIR, fn))
    n = res["m_nMapSize"]
    unit = res["m_fUnitDist"]
    ev = res["events"]
    h = res["height"]
    dens = res["coll_sub_density"]
    print("file", fn, "n", n, "unit", unit)

    # overlap of event value with presence of collision polygons in same 4 m cell (unit==4 -> same grid)
    if unit == 4.0:
        ov = Counter()
        for x in range(n - 1):
            for z in range(n - 1):
                ov[(ev[x * n + z], (x, z) in dens)] += 1
        for (v, hascoll), k in sorted(ov.items()):
            print("event=%d collisionPolysInCell=%s : %d tiles" % (v, hascoll, k))

    lab, sizes = components(ev, n, 1, eight=True)
    top = sorted(sizes.items(), key=lambda kv: -kv[1])[:8]
    print("8-connected components of event==1:", len(sizes), "largest:", [(c, s) for c, s in top])
    lab4, sizes4 = components(ev, n, 1, eight=False)
    top4 = sorted(sizes4.items(), key=lambda kv: -kv[1])[:5]
    print("4-connected components of event==1:", len(sizes4), "largest:", top4)

    samples = []
    for o in res["object_events"]:
        samples.append(("SMD objevt idx=%d" % o[1], o[5], o[7]))
    for w in res["warps"]:
        samples.append(("warp %d dest(z%d)" % (w["sWarpID"], w["sZone"]), w["fX"], w["fZ"]))
    samples += [("ZONE_INFO Init (10,10)", 10.0, 10.0), ("map centre", (n - 1) * unit / 2, (n - 1) * unit / 2)]
    for name, wx, wz in samples:
        tx, tz = int(wx / unit), int(wz / unit)
        if 0 <= tx < n and 0 <= tz < n:
            i = tx * n + tz
            print("%-28s world=(%.1f,%.1f) tile=(%d,%d) event=%d height=%.2f comp8=%d (size %d)"
                  % (name, wx, wz, tx, tz, ev[i], h[i], lab[i], sizes.get(lab[i], 0)))
        else:
            print("%-28s world=(%.1f,%.1f) outside this map" % (name, wx, wz))

    # how many event==1 tiles have NO event==0 8-neighbour vs. how many have >=1 (A* with IsMovable()==(ev==0)
    # can only expand into ev==0 tiles)
    zero_nb = Counter()
    for x in range(1, n - 2):
        for z in range(1, n - 2):
            if ev[x * n + z] != 1:
                continue
            c = 0
            for dx in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    if (dx or dz) and ev[(x + dx) * n + z + dz] == 0:
                        c += 1
            zero_nb[c] += 1
    print("event==1 tiles by number of event==0 neighbours (0..8):", sorted(zero_nb.items()))


if __name__ == "__main__":
    main()


def render_components(fn, tag):
    from smd_parse import write_png
    res = parse(os.path.join(MAP_DIR, fn))
    n = res["m_nMapSize"]
    ev = res["events"]
    lab, sizes = components(ev, n, 1, eight=False)
    top = [c for c, s in sorted(sizes.items(), key=lambda kv: -kv[1])[:6]]
    pal = [(230, 60, 60), (60, 160, 230), (60, 200, 90), (230, 200, 40), (180, 80, 220), (40, 200, 200)]
    rows = []
    for zi in range(n - 1, -1, -1):
        row = bytearray()
        for xi in range(n):
            i = xi * n + zi
            if ev[i] == 0:
                row.extend((0, 0, 0))
            elif lab[i] in top:
                row.extend(pal[top.index(lab[i])])
            else:
                row.extend((200, 200, 200))
        rows.append(row)
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "%s_components4.png" % tag)
    write_png(p, n, n, rows)
    print("components PNG:", p, "top4conn sizes:", [sizes[c] for c in top], "colors red,blue,green,yellow,purple,cyan")
