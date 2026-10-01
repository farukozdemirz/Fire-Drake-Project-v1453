#!/usr/bin/env python3
"""Read-only: find test-arena candidates in Ronark Land (zone 71) far from NPC spawns and guard towers.
Coordinates: world meters, x right, z up; tile = 4 m; event index x*n+z; 1=walkable, 0=blocked."""
import os, sys, csv, math
from collections import deque
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from smd_parse import parse, MAP_DIR, write_png
from smd_analyze import components

res = parse(os.path.join(MAP_DIR, "freezone_a_20050718.smd"))
n = res["m_nMapSize"]; U = res["m_fUnitDist"]; ev = res["events"]; h = res["height"]
lab, sizes = components(ev, n, 1, eight=False)
def tile(x, z): return int(x // U), int(z // U)
def labat(x, z):
    tx, tz = tile(x, z); return lab[tx * n + tz]
keypts = {"karus_respawn": (1385, 1095), "elmo_respawn": (635, 925), "karus_gate": (1375, 1085),
          "elmo_gate": (622, 911), "bifrost_monument": (1014, 992)}
for k, (x, z) in keypts.items():
    l = labat(x, z); print(k, (x, z), "component", l, "size", sizes.get(l))
main = max((l for l in sizes), key=lambda l: sizes[l] if labat(1014, 992) != l else sizes[l])
# main playable = component reachable from both respawns? check
rows = list(csv.DictReader(open(os.environ.get("FDP_NPCPOS_CSV", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "ronark_npcpos.csv")))))
spawns = []; towers = []
for r in rows:
    lx, rx = sorted([float(r["LeftX"]), float(r["RightX"])]); tz, bz = sorted([float(r["TopZ"]), float(r["BottomZ"])])
    sr = float(r["bySearchRange"] or 0); cls = r["class"]
    item = (lx, tz, rx, bz, sr, cls, r["strName"], int(r["NumNPC"] or 0))
    if "guard tower" in cls: towers.append(item)
    elif any(s in cls for s in ("monster", "soldier", "monument", "gate")): spawns.append(item)
print("spawn rows", len(spawns), "tower rows", len(towers))
def rect_dist(x, z, it):
    lx, tz, rx, bz = it[:4]
    dx = max(lx - x, 0, x - rx); dz = max(tz - z, 0, z - bz); return math.hypot(dx, dz)
# clearance: BFS distance (tiles) to nearest blocked tile
INF = 10**9; clr = [INF] * (n * n); q = deque()
for i in range(n * n):
    if ev[i] != 1: clr[i] = 0; q.append(i)
while q:
    i = q.popleft(); x, z = divmod(i, n)
    for dx, dz in ((1,0),(-1,0),(0,1),(0,-1)):
        xx, zz = x+dx, z+dz
        if 0 <= xx < n and 0 <= zz < n:
            j = xx*n+zz
            if clr[j] > clr[i] + 1: clr[j] = clr[i] + 1; q.append(j)
lk = labat(1385, 1095); le = labat(635, 925)
print("respawn components karus/elmo:", lk, le, "same:", lk == le)
cands = []
R = 40.0
for tx in range(0, n - 1, 2):
    for tz in range(0, n - 1, 2):
        i = tx * n + tz
        if ev[i] != 1: continue
        x, z = tx * U + 2, tz * U + 2
        dspawn = min((rect_dist(x, z, s) - s[4] for s in spawns), default=1e9)
        dtower = min((rect_dist(x, z, t) - t[4] for t in towers), default=1e9)
        # walkable fraction & height spread within R
        tot = walk = 0; hs = []
        rt = int(R // U)
        for ax in range(tx - rt, tx + rt + 1, 2):
            for az in range(tz - rt, tz + rt + 1, 2):
                if (ax - tx) ** 2 + (az - tz) ** 2 > rt * rt or not (0 <= ax < n and 0 <= az < n): continue
                tot += 1; j = ax * n + az
                if ev[j] == 1 and lab[j] == lab[i]: walk += 1; hs.append(h[j])
        if tot == 0: continue
        wf = walk / tot; hsp = (max(hs) - min(hs)) if hs else 0
        cands.append((x, z, lab[i], sizes[lab[i]], dspawn, dtower, wf, hsp, clr[i] * U,
                      math.hypot(x - 1375, z - 1085), math.hypot(x - 622, z - 911)))
print("tiles evaluated", len(cands))
big = [c for c in cands if c[3] > 20000]
def show(title, lst, k=12):
    print("\n==", title)
    print("x z comp compsize d_spawn_edge(m, minus searchR) d_tower(minus searchR) walk_frac_R40 height_spread clearance d_karusGate d_elmoGate")
    for c in lst[:k]: print("%5.0f %5.0f %4d %6d %7.1f %7.1f %5.2f %6.1f %5.0f %6.0f %6.0f" % c)
best = sorted([c for c in big if c[6] > 0.85 and c[7] < 8], key=lambda c: -min(c[4], c[5]))
show("Best isolated open flat discs (R=40m) in big components", best)
# near Karus gate: 80-200 m from Karus gate, on the big component
kg = sorted([c for c in big if 90 <= c[9] <= 220 and c[6] > 0.8], key=lambda c: (-min(c[4], c[5])))
show("Near Karus gate (90-220 m)", kg)
iso = [c for c in big if c[4] > 0 and c[5] > 0]
print("\nfraction of big-component tiles with no spawn search-area overlap and no tower range:", len(iso), "/", len(big))

# ---- refinement: main playable component only (the one containing both respawns)
MAINC = lk
mc = [c for c in cands if c[2] == MAINC]
best_main = sorted([c for c in mc if c[6] > 0.85 and c[7] < 12], key=lambda c: -min(c[4], c[5]))
show("Best isolated discs in MAIN playable component", best_main, 15)
kg2 = sorted([c for c in mc if 90 <= c[9] <= 300 and c[6] > 0.85], key=lambda c: -min(c[4], c[5]))
show("Main component, 90-300 m from Karus gate", kg2, 15)
eg2 = sorted([c for c in mc if 90 <= c[10] <= 300 and c[6] > 0.85], key=lambda c: -min(c[4], c[5]))
show("Main component, 90-300 m from El Morad gate (mirror check)", eg2, 8)
# PNG overlay
img = []
def inrect(x, z, it, pad):
    return it[0]-pad <= x <= it[2]+pad and it[1]-pad <= z <= it[3]+pad
sel = kg2[0]
for py in range(n - 1):
    tz = n - 2 - py
    row = []
    for tx in range(n - 1):
        i = tx * n + tz; x, z = tx * U + 2, tz * U + 2
        if ev[i] != 1: c = (30, 30, 30)
        elif lab[i] == MAINC: c = (150, 190, 230)
        else: c = (200, 200, 200)
        for s in spawns:
            if inrect(x, z, s, 0): c = (230, 60, 60); break
            elif inrect(x, z, s, s[4]) and c != (230, 60, 60): c = (240, 160, 160)
        for t in towers:
            if inrect(x, z, t, t[4]): c = (250, 200, 40)
        if math.hypot(x - sel[0], z - sel[1]) <= 40: c = (40, 170, 60)
        for (kx, kz) in ((1385, 1095), (635, 925)):
            if math.hypot(x - kx, z - kz) <= 10: c = (120, 0, 160)
        row.append(c)
    img.append([v for px in row for v in px])
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "zone71_arena_overlay.png")
write_png(out, n - 1, n - 1, img)
print("overlay:", out, "selected center", sel[:2])
