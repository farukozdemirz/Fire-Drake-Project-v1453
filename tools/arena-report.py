#!/usr/bin/env python3
"""Reproducible data report for test arena A (1274, 890) and backup B (746, 1106).

Reads the local SMD event/height grid (docs/appendix/tools/smd_parse.py) and the
K_NPCPOS / K_NPC / K_MONSTER / START_POSITION rows of zone 71 and prints:

    SPAWN  nearest spawn/tower pay for A and B (docs/15 section 2.3 check)
    GRID   walkability and height of the arena discs (R = 40 / 60 m)
    AXIS   start-position axis search (both ends walkable, flat, connected)
    PATH   event-grid Dijkstra from both respawns to A and B (walk/sprint time)
    CHECK  computed vs documented spawn/tower margins

The tool only reads data; it never writes to the database.

Usage:
    python3 tools/arena-report.py [--sqlcmd PATH] [--server INSTANCE] [--db DB] [--map-dir DIR]
    python3 tools/arena-report.py --selftest
"""

import argparse
import heapq
import math
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(ROOT, "docs", "appendix", "tools"))
from smd_parse import parse  # noqa: E402  (path set above)

DEFAULT_SQLCMD = "/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
DEFAULT_SERVER = ".\\SQLEXPRESS"
DEFAULT_DB = "FDP_kn_online"
DEFAULT_MAP_DIR = os.environ.get("FDP_MAP_DIR", "/mnt/c/dev/fdp/server/Map")
MAP_FILE = "freezone_a_20050718.smd"

POINT_A = (1274.0, 890.0)
POINT_B = (746.0, 1106.0)
WALK_MPS = 4.5
SPRINT_MPS = 6.7
ARENA_RADIUS = 60.0
AXIS_DISTANCE = 35.0
AXIS_CLUSTER_RADIUS = 6.0

DOC_MARGINS = {
    "A": {"monster": 144.0, "tower": 133.0},
    "B": {"monster": 160.0, "tower": 146.0},
}

SPAWN_QUERY = (
    "SELECT p.NpcID, p.ActType, p.LeftX, p.TopZ, p.RightX, p.BottomZ, p.NumNPC, "
    "RTRIM(ISNULL(m.strName, '')), ISNULL(m.byType, 0), ISNULL(m.bySearchRange, 0), "
    "ISNULL(m.byTracingRange, 0), ISNULL(m.byAttackRange, 0), "
    "RTRIM(ISNULL(n.strName, '')), ISNULL(n.byType, 0), ISNULL(n.bySearchRange, 0), "
    "ISNULL(n.byTracingRange, 0), ISNULL(n.byAttackRange, 0) "
    "FROM K_NPCPOS p "
    "LEFT JOIN K_MONSTER m ON p.ActType < 100 AND m.sSid = p.NpcID "
    "LEFT JOIN K_NPC n ON p.ActType >= 100 AND n.sSid = p.NpcID "
    "WHERE p.ZoneID = 71"
)

START_QUERY = (
    "SELECT sKarusX, sKarusZ, sElmoradX, sElmoradZ, bRangeX, bRangeZ "
    "FROM START_POSITION WHERE ZoneID = 71"
)


def run_query(sqlcmd, server, db, query):
    command = [
        sqlcmd, "-S", server, "-E", "-d", db,
        "-W", "-s", "|", "-h", "-1", "-b",
        "-Q", "SET NOCOUNT ON; " + query,
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write("sqlcmd failed (exit %d):\n%s\n" % (result.returncode, result.stderr.strip()))
        sys.exit(1)
    rows = []
    for line in result.stdout.replace("\r", "").split("\n"):
        if not line.strip():
            continue
        rows.append([field.strip() for field in line.split("|")])
    return rows


def tile(x, z, unit):
    return int(x // unit), int(z // unit)


def walkable(events, n, tx, tz):
    if not (0 <= tx < n and 0 <= tz < n):
        return False
    return events[tx * n + tz] == 1


def walkable_xy(events, n, unit, x, z):
    tx, tz = tile(x, z, unit)
    return walkable(events, n, tx, tz)


def height_at(heights, n, unit, x, z):
    tx, tz = tile(x, z, unit)
    if not (0 <= tx < n and 0 <= tz < n):
        return 0.0
    return heights[tx * n + tz]


def rect_dist(x, z, rect):
    """Euclidean distance from a point to an axis-aligned rectangle (min/max order)."""
    dx = max(rect[0] - x, 0.0, x - rect[2])
    dz = max(rect[1] - z, 0.0, z - rect[3])
    return math.hypot(dx, dz)


def circle_stats(events, heights, n, unit, cx, cz, radius):
    """Tile-center sampling inside a circle. Returns (walk, total, h_min, h_max)."""
    rt = int(radius // unit) + 1
    tcx, tcz = tile(cx, cz, unit)
    walk = total = 0
    h_min = h_max = None
    for tx in range(tcx - rt, tcx + rt + 1):
        for tz in range(tcz - rt, tcz + rt + 1):
            if not (0 <= tx < n and 0 <= tz < n):
                continue
            px, pz = tx * unit + unit / 2.0, tz * unit + unit / 2.0
            if (px - cx) ** 2 + (pz - cz) ** 2 > radius * radius:
                continue
            total += 1
            if events[tx * n + tz] == 1:
                walk += 1
                hval = heights[tx * n + tz]
                h_min = hval if h_min is None else min(h_min, hval)
                h_max = hval if h_max is None else max(h_max, hval)
    return walk, total, h_min, h_max


def dijkstra(events, n, start, goal, allowed=None):
    """8-neighbour Dijkstra in tile units; corners may not cut blocked tiles."""
    if not walkable(events, n, *start) or not walkable(events, n, *goal):
        return None
    if allowed is not None and (not allowed(*start) or not allowed(*goal)):
        return None
    infinity = float("inf")
    dist = {start: 0.0}
    queue = [(0.0, start)]
    while queue:
        current_dist, (tx, tz) = heapq.heappop(queue)
        if current_dist > dist.get((tx, tz), infinity):
            continue
        if (tx, tz) == goal:
            return current_dist
        for dx in (-1, 0, 1):
            for dz in (-1, 0, 1):
                if dx == 0 and dz == 0:
                    continue
                nx, nz = tx + dx, tz + dz
                if not walkable(events, n, nx, nz):
                    continue
                if allowed is not None and not allowed(nx, nz):
                    continue
                if dx != 0 and dz != 0:
                    if not walkable(events, n, tx + dx, tz) or not walkable(events, n, tx, tz + dz):
                        continue
                    step = math.sqrt(2.0)
                else:
                    step = 1.0
                new_dist = current_dist + step
                if new_dist < dist.get((nx, nz), infinity):
                    dist[(nx, nz)] = new_dist
                    heapq.heappush(queue, (new_dist, (nx, nz)))
    return None


def load_spawns(rows):
    spawns = []
    for row in rows:
        if len(row) != 17:
            raise ValueError("unexpected K_NPCPOS row with %d fields" % len(row))
        npc_id = int(row[0])
        act_type = int(row[1])
        xs = [int(row[2]), int(row[4])]
        zs = [int(row[3]), int(row[5])]
        rect = (float(min(xs)), float(min(zs)), float(max(xs)), float(max(zs)))
        num = int(row[6])
        if act_type < 100:
            name, by_type = row[7], int(row[8])
            search, tracing, attack = int(row[9]), int(row[10]), int(row[11])
        else:
            name, by_type = row[12], int(row[13])
            search, tracing, attack = int(row[14]), int(row[15]), int(row[16])
        spawns.append({
            "sid": npc_id, "name": name, "by_type": by_type, "rect": rect,
            "search": search, "tracing": tracing, "attack": attack, "num": num,
            "class": classify_spawn(act_type, by_type),
        })
    return spawns


def classify_spawn(act_type, by_type):
    if act_type < 100:
        return "monster_boss" if by_type == 3 else "monster"
    if by_type == 62:
        return "guard_tower"
    if by_type == 0:
        return "soldier_npc"
    if by_type == 150:
        return "gate"
    if by_type == 155:
        return "monument"
    if by_type in (101, 102):
        return "outpost"
    if by_type in (22, 31):
        return "service"
    return "other"


def cluster_ratio(events, n, unit, x, z, radius):
    walk = total = 0
    step = 2
    limit = int(radius)
    for dx in range(-limit, limit + 1, step):
        for dz in range(-limit, limit + 1, step):
            if dx * dx + dz * dz > radius * radius:
                continue
            total += 1
            if walkable_xy(events, n, unit, x + dx, z + dz):
                walk += 1
    return walk, total


def line_walk_ratio(events, n, unit, p1, p2, samples=71):
    walk = 0
    for i in range(samples):
        t = i / (samples - 1)
        x = p1[0] + (p2[0] - p1[0]) * t
        z = p1[1] + (p2[1] - p1[1]) * t
        if walkable_xy(events, n, unit, x, z):
            walk += 1
    return walk / samples


def confined_axis_path(events, n, unit, center, p1, p2, radius):
    tcx, tcz = tile(center[0], center[1], unit)

    def allowed(tx, tz):
        px, pz = tx * unit + unit / 2.0, tz * unit + unit / 2.0
        return (px - center[0]) ** 2 + (pz - center[1]) ** 2 <= radius * radius

    distance = dijkstra(events, n, tile(p1[0], p1[1], unit), tile(p2[0], p2[1], unit), allowed)
    return None if distance is None else distance * unit


def axis_scan(point_name, center, events, heights, n, unit, out):
    scans = []
    for angle in range(0, 180, 15):
        rad = math.radians(angle)
        dx, dz = math.cos(rad), math.sin(rad)
        p1 = (center[0] + AXIS_DISTANCE * dx, center[1] + AXIS_DISTANCE * dz)
        p2 = (center[0] - AXIS_DISTANCE * dx, center[1] - AXIS_DISTANCE * dz)
        c1w, c1t = cluster_ratio(events, n, unit, p1[0], p1[1], AXIS_CLUSTER_RADIUS)
        c2w, c2t = cluster_ratio(events, n, unit, p2[0], p2[1], AXIS_CLUSTER_RADIUS)
        line = line_walk_ratio(events, n, unit, p1, p2)
        dh = abs(height_at(heights, n, unit, p1[0], p1[1])
                 - height_at(heights, n, unit, p2[0], p2[1]))
        confined = confined_axis_path(events, n, unit, center, p1, p2, ARENA_RADIUS)
        entry = {
            "angle": angle, "p1": p1, "p2": p2,
            "p1_walk": walkable_xy(events, n, unit, p1[0], p1[1]),
            "p2_walk": walkable_xy(events, n, unit, p2[0], p2[1]),
            "c1": (c1w, c1t), "c2": (c2w, c2t), "line": line, "dh": dh,
            "confined": confined,
        }
        scans.append(entry)
        out.write("AXIS pt=%s angle=%d p1=(%.1f,%.1f) p2=(%.1f,%.1f) p1_walk=%s p2_walk=%s "
                  "cluster1=%d/%d cluster2=%d/%d line_walk=%.2f dh=%.2f confined_path_m=%s\n" % (
                      point_name, angle, p1[0], p1[1], p2[0], p2[1],
                      "yes" if entry["p1_walk"] else "no", "yes" if entry["p2_walk"] else "no",
                      c1w, c1t, c2w, c2t, line, dh,
                      "none" if confined is None else "%.1f" % confined))

    accepted = [e for e in scans
                if e["p1_walk"] and e["p2_walk"]
                and e["c1"][0] * 100.0 / e["c1"][1] >= 90.0
                and e["c2"][0] * 100.0 / e["c2"][1] >= 90.0
                and e["line"] >= 0.98 and e["dh"] <= 2.0 and e["confined"] is not None]
    if accepted:
        best = min(accepted, key=lambda e: (e["dh"], -e["line"]))
        out.write("AXIS_BEST pt=%s angle=%d reason=dh=%.2f,line_walk=%.2f,cluster_min=%.1f%%\n" % (
            point_name, best["angle"], best["dh"], best["line"],
            min(best["c1"][0] * 100.0 / best["c1"][1], best["c2"][0] * 100.0 / best["c2"][1])))
    else:
        out.write("AXIS_BEST pt=%s angle=none reason=no_candidate\n" % point_name)


def write_report(spawns, events, heights, n, unit, start_row, out):
    out.write("== SPAWN ==\n")
    summaries = {}
    for point_name, point in (("A", POINT_A), ("B", POINT_B)):
        groups = {
            "monster": [s for s in spawns if s["class"] in ("monster", "monster_boss")],
            "tower": [s for s in spawns if s["class"] == "guard_tower"],
            "npc": [s for s in spawns if s["class"] not in ("monster", "monster_boss", "guard_tower")],
        }
        summary = {}
        for kind, entries in groups.items():
            scored = []
            for spawn in entries:
                distance = rect_dist(point[0], point[1], spawn["rect"])
                margin = distance - spawn["search"]
                margin_trace = distance - max(spawn["search"], spawn["tracing"])
                scored.append((margin, margin_trace, distance, spawn))
            scored.sort(key=lambda item: (item[0], item[3]["sid"]))
            for rank, (margin, margin_trace, distance, spawn) in enumerate(scored[:3], 1):
                out.write("SPAWN pt=%s kind=%s nearest=%d name=%s sid=%d class=%s "
                          "rect_dist=%.1f margin=%.1f margin_trace=%.1f num=%d\n" % (
                              point_name, kind, rank, spawn["name"], spawn["sid"], spawn["class"],
                              distance, margin, margin_trace, spawn["num"]))
            within = sum(1 for item in scored if item[0] < 120.0)
            summary[kind] = (within, scored[0][0] if scored else None)
        summaries[point_name] = summary
        out.write("SPAWN_SUMMARY pt=%s within_120_monster=%d within_120_tower=%d within_120_other=%d "
                  "min_margin_monster=%s min_margin_tower=%s\n" % (
                      point_name, summary["monster"][0], summary["tower"][0], summary["npc"][0],
                      "none" if summary["monster"][1] is None else "%.1f" % summary["monster"][1],
                      "none" if summary["tower"][1] is None else "%.1f" % summary["tower"][1]))

    out.write("== GRID ==\n")
    for point_name, point in (("A", POINT_A), ("B", POINT_B)):
        tx, tz = tile(point[0], point[1], unit)
        center_h = heights[tx * n + tz] if 0 <= tx < n and 0 <= tz < n else 0.0
        out.write("GRID pt=%s center_walk=%s center_h=%.2f\n" % (
            point_name, "yes" if walkable(events, n, tx, tz) else "no", center_h))
        for radius in (40.0, 60.0):
            walk, total, h_min, h_max = circle_stats(events, heights, n, unit,
                                                     point[0], point[1], radius)
            out.write("GRID pt=%s r=%d walk_pct=%.1f walk=%d total=%d h_min=%.2f h_max=%.2f\n" % (
                point_name, int(radius), 100.0 * walk / total, walk, total, h_min, h_max))

    out.write("== AXIS ==\n")
    axis_scan("A", POINT_A, events, heights, n, unit, out)
    axis_scan("B", POINT_B, events, heights, n, unit, out)

    out.write("== PATH ==\n")
    karus = (float(start_row[0]) + float(start_row[4]) / 2.0,
             float(start_row[1]) + float(start_row[5]) / 2.0)
    elmorad = (float(start_row[2]) + float(start_row[4]) / 2.0,
               float(start_row[3]) + float(start_row[5]) / 2.0)
    out.write("START nation=karus table=(%d,%d) range=(%d,%d) center=(%.1f,%.1f)\n" % (
        int(start_row[0]), int(start_row[1]), int(start_row[4]), int(start_row[5]),
        karus[0], karus[1]))
    out.write("START nation=elmorad table=(%d,%d) range=(%d,%d) center=(%.1f,%.1f)\n" % (
        int(start_row[2]), int(start_row[3]), int(start_row[4]), int(start_row[5]),
        elmorad[0], elmorad[1]))
    for nation, origin in (("karus", karus), ("elmorad", elmorad)):
        for point_name, point in (("A", POINT_A), ("B", POINT_B)):
            straight = math.hypot(origin[0] - point[0], origin[1] - point[1])
            distance = dijkstra(events, n, tile(origin[0], origin[1], unit),
                                tile(point[0], point[1], unit))
            if distance is None:
                out.write("PATH from=%s to=%s straight_m=%.1f path_m=none\n" % (
                    nation, point_name, straight))
            else:
                path_m = distance * unit
                out.write("PATH from=%s to=%s straight_m=%.1f path_m=%.1f t_walk_s=%.1f "
                          "t_sprint_s=%.1f\n" % (
                              nation, point_name, straight, path_m,
                              path_m / WALK_MPS, path_m / SPRINT_MPS))

    out.write("== CHECK ==\n")
    for point_name in ("A", "B"):
        for kind, key in (("monster", "min_margin_monster"), ("tower", "min_margin_tower")):
            calc = summaries[point_name][kind][1]
            doc = DOC_MARGINS[point_name][kind]
            if calc is None:
                out.write("CHECK pt=%s %s calc=none doc=%.0f diff=none\n" % (point_name, key, doc))
            else:
                out.write("CHECK pt=%s %s calc=%.1f doc=%.0f diff=%.1f\n" % (
                    point_name, key, calc, doc, calc - doc))


def run_selftest():
    # (a) rect_dist
    rect = (0.0, 0.0, 10.0, 10.0)
    assert rect_dist(5.0, 5.0, rect) == 0.0
    assert rect_dist(15.0, 5.0, rect) == 5.0
    assert abs(rect_dist(13.0, 14.0, rect) - 5.0) < 1e-9

    # (b) synthetic Dijkstra: open grid, orthogonal / diagonal / corner cutting
    n = 12
    events = [1] * (n * n)
    assert dijkstra(events, n, (0, 0), (10, 0)) == 10.0
    assert abs(dijkstra(events, n, (0, 0), (5, 5)) - 5.0 * math.sqrt(2.0)) < 1e-9
    corner = [1] * (n * n)
    corner[1 * n + 0] = 0  # block (1,0): the diagonal (0,0)->(1,1) must not cut it
    detour = dijkstra(corner, n, (0, 0), (1, 1))
    assert detour is not None and abs(detour - math.sqrt(2.0)) > 1e-9 and detour == 2.0, detour

    # (c) circle walkability: fully open grid -> 100%
    open_events = [1] * (20 * 20)
    open_heights = [0.0] * (20 * 20)
    walk, total, _h_min, _h_max = circle_stats(open_events, open_heights, 20, 4.0, 40.0, 40.0, 20.0)
    assert walk == total and walk > 0, (walk, total)

    # (d) axis candidate: a blocked endpoint is rejected
    blocked = [1] * (20 * 20)
    blocked[5 * 20 + 5] = 0
    assert walkable_xy(blocked, 20, 4.0, 22.0, 22.0) is False
    assert walkable_xy(blocked, 20, 4.0, 30.0, 30.0) is True

    print("selftest OK")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--sqlcmd", default=DEFAULT_SQLCMD)
    parser.add_argument("--server", default=DEFAULT_SERVER)
    parser.add_argument("--db", default=DEFAULT_DB)
    parser.add_argument("--map-dir", default=DEFAULT_MAP_DIR)
    args = parser.parse_args(argv)

    if args.selftest:
        return run_selftest()

    map_path = os.path.join(args.map_dir, MAP_FILE)
    if not os.path.isfile(map_path):
        sys.stderr.write("SMD map not found: %s\n" % map_path)
        return 1
    parsed = parse(map_path, load_warps=False)
    n = parsed["m_nMapSize"]
    unit = parsed["m_fUnitDist"]
    events = parsed["events"]
    heights = parsed["height"]

    start_rows = run_query(args.sqlcmd, args.server, args.db, START_QUERY)
    if len(start_rows) != 1 or len(start_rows[0]) != 6:
        sys.stderr.write("START_POSITION row for zone 71 not found\n")
        return 1

    spawns = load_spawns(run_query(args.sqlcmd, args.server, args.db, SPAWN_QUERY))
    write_report(spawns, events, heights, n, unit, start_rows[0], sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
