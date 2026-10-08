#!/usr/bin/env python3
"""Builds the server map (SMD) of the 1534 client's new Moradon (zone 21) from the
client's own map files, patches the warps that lead into zone 21 on our other maps,
and verifies both offline.

Method and evidence: docs/reports/u0-1534/G-yeni-moradon-smd.md (sections 1-8),
docs/adr/ADR-0068 addendum 2, plan U3-01. Inputs are only read; the generated SMD
files are client-derived data and are never written into the repository or into a
server Map directory (distribution is a separate step).

Commands:
    build          --client-zones DIR --donor SMD [--warps SPEC.json] --out SMD
                   [--fees-from SMD|alpha] [--drop-zones 73]
    patch-inbound  --map-dir DIR --files A.smd,B.smd,... --x 817 --z 530 --out-dir DIR
    verify         --smd SMD [--points JSON] [--spawns FILE] [--patched-dir DIR]
                   [--client-zones DIR] [--donor SMD] [--map-dir DIR] [--zone-info FILE]
    --selftest     synthetic unit tests (always run) and reference-map tests
                   (print SKIP when the files are missing)

SMD layout written by build, in SMDFile::LoadMap order (shared/SMDFile.cpp:75-102):
    int32 n, float32 unit, float32 height[n*n]                  LoadTerrain
    CN3ShapeMgr collision block (N3BASE/N3ShapeMgr.cpp:52-113)   LoadCollisionData
    int32 count, count * 24 B object events (read, discarded)    LoadObjectEvent
    int16 event[n*n], index x*n+z                                LoadMapTile
    int32 count, count * 20 B regene events                      LoadRegeneEvent
    int32 count, count * 320 B _WARP_INFO (structs.h, pack 1)    LoadWarpList

Where each part comes from (G section 7, build steps 1-8):
    heights        client <Zones>/moradon.gtd: n = 257, then n*n MAPDATA records
                   {float32 h; uint32 tile} (8 B), index x*n+z, copied without transpose.
    collision      client <Zones>/moradon.opd: the CN3ShapeMgr block right after the
                   header, copied byte for byte (same layout as the SMD block).
    event grid     rule R (G section 4.2): tile (x,z), x,z < n-1, is 0 (blocked) when its
                   4 m collision sub-cell has polygons or max-min of its 4 corner heights
                   is >= 10.0 m; otherwise 1. Row x = n-1 and column z = n-1 are 1.
    object events  the donor SMD block (ALPHA moradon_0826.smd), copied as is.
    regene         count 0.
    warps          the donor records minus target zones without a ZONE_INFO row (73).
                   Only dwPay is changed: the fee of our old Moradon warp to the same
                   destination (same warp group sWarpID/10, same target zone, same name;
                   the same sWarpID is preferred when several match). A record with no
                   such counterpart keeps the donor fee and is listed as such.
                   --warps SPEC.json replaces the donor list; its entries are written
                   as given (JSON list of {id, name, zone, x, z, pay, nation,
                   [announce], [y], [r]}); --drop-zones still applies.

Client file header: 1534 files start with int32 L, L bytes (encoded map name), int32
flag; data begins at 4+L+4 (moradon: 15). Older files start with int32 version (1|2),
int32 L, L bytes plain name; data begins at 8+L. Both variants are tried and the one
whose payload validates is used.

The output is deterministic: the same inputs give the same SMD bytes and the same
sidecar JSON (sorted keys, ASCII, LF, no time stamps). Standard library only; works
under python3 -I.
"""

import argparse
import array
import hashlib
import importlib.util
import json
import math
import os
import struct
import sys
import tempfile
import zlib

sys.dont_write_bytecode = True

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(TOOLS_DIR)
SMD_PARSE_DIR = os.path.join(REPO_DIR, "docs", "appendix", "tools")
NAV_EXPORT_PATH = os.path.join(TOOLS_DIR, "nav-export.py")

DEFAULT_CLIENT_ZONES = "/mnt/c/dev/fdp1534/client/Knight Online/Zones"
DEFAULT_DONOR = "/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/Server-Files/Map/moradon_0826.smd"
DEFAULT_MAP_DIR = os.environ.get("FDP_MAP_DIR", "/mnt/c/dev/fdp/server/Map")
OLD_MORADON_SMD = "moradon_20060124.smd"
CLIENT_MAP_NAME = "moradon"
ZONE_MORADON = 21

N_EXPECTED = 257
UNIT = 4.0
SLOPE_RANGE_M = 10.0
CELL_MAIN_DEVIDE = 4                                  # N3BASE/N3ShapeMgr.h:6
CELL_SUB_SIZE = 4                                     # N3BASE/N3ShapeMgr.h:7
CELL_MAIN_SIZE = CELL_MAIN_DEVIDE * CELL_SUB_SIZE     # N3BASE/N3ShapeMgr.h:8
MAX_MAP_EXTENT = 4096.0                               # CN3ShapeMgr::Create limit

OBJECT_EVENT_SIZE = 24
REGENE_EVENT_SIZE = 20
# _WARP_INFO, shared/database/structs.h:221-239, #pragma pack(1), 320 bytes
WARP_SIZE = 320
WARP_OFF_NAME = 2
WARP_NAME_LEN = 32
WARP_OFF_ANNOUNCE = 34
WARP_ANNOUNCE_LEN = 256
WARP_OFF_PAY = 292
WARP_OFF_ZONE = 296
WARP_OFF_X = 300
WARP_OFF_Z = 308

DEFAULT_DROP_ZONES = "73"
# K_OBJECTPOS zone 21 gates 4014 (Karus) / 4013 (El Morad) use ControlNpcID 211 / 212;
# CUser::GetWarpList lists the warps whose sWarpID / 10 equals that id.
GATE_WARP_GROUPS = (211, 212)
INBOUND_FILES = ("karus_051221.smd", "elmo_051221.smd", "siege_0722.smd",
                 "freezone_a_20050718.smd", "freezone_b_20050718.smd", "In_dungeon_20050718.smd")
INBOUND_X = 817.0
INBOUND_Z = 530.0
NAV_ZONE71_SMD = "freezone_a_20050718.smd"

# Snapshot of FDP_kn_online ZONE_INFO (ZoneNo -> strZoneName), 2026-10-08.
# verify --zone-info FILE replaces it with a fresh export.
ZONE_INFO_SNAPSHOT = {
    1: "karus_051221.smd", 2: "elmo_051221.smd", 11: "k_eslant_20050707.smd",
    12: "e_eslant_20050707.smd", 21: "moradon_20060124.smd", 30: "siege_0722.smd",
    31: "dungeon_1216.smd", 32: "dungeonb_0925.smd", 33: "dungeonc_1008.smd",
    34: "dragon_room_20050728.smd", 48: "BattleField_20050801.smd", 51: "clanfight_b.smd",
    52: "clanfight_b.smd", 53: "clanfight_b.smd", 54: "clanfight_b.smd", 55: "clanfight_b.smd",
    61: "bat_a_20050718.smd", 62: "bat_b_20050718.smd", 63: "bat_c_20050718.smd",
    64: "bat_d_051221.smd", 69: "bat_b_20050718.smd", 71: "freezone_a_20050718.smd",
    72: "freezone_b_20050718.smd", 81: "In_dungeon_20050718.smd",
    82: "In_dungeon_02_20050722.smd", 83: "In_dungeon_03_b_20050805.smd", 84: "dungeon.smd",
    85: "dungeon.smd", 87: "dungeon.smd", 93: "dungeon_c.smd", 94: "dragon.smd",
}

# Rule R reproduces these official grids from the SMDs' own heights and collision.
RULE_REFERENCE_MAPS = (
    ("old Moradon (zone 21)", OLD_MORADON_SMD),
    ("Ronark Land (zone 71)", "freezone_a_20050718.smd"),
    ("Ardream (zone 72)", "freezone_b_20050718.smd"),
)
# End to end: client .gtd/.opd -> rule R -> compare with the official SMD grid.
E2E_REFERENCE_MAPS = (
    ("Ronark Land (zone 71)", "freezone_b", "freezone_a_20050718.smd"),
    ("Ardream (zone 72)", "freezone", "freezone_b_20050718.smd"),
)
E2E_MIN_AGREEMENT = 0.9999

# verify thresholds (G section 7 expected values, with a margin where the value is a share)
V2_MIN_ALPHA_TRANSPOSED = 0.80      # G: 88.9 %; guards against a wrong client folder
V6_MIN_MONSTER_SHARE = 0.90         # G: 64/69
V7_MAX_WALKABLE_DIFF_PP = 3.0       # G: 72.3 % vs 72.1 %
V9_MIN_DISC_SHARE = 0.85            # r5 disc (G): 81/81 on other maps, 80/81 Folk, 73/81 Tale
V9_WARN_ARRIVAL_SHARE = 0.85        # SelectWarpList arrival box; below -> WARN (not in G)

# V5 reference points (G sections 4.4 and 7). Kinds:
#   square   START_POSITION: x + [0..size], z + [0..size] at 1 m steps
#   disc     1 m points with dx*dx + dz*dz <= r*r
#   arrival  CUser::SelectWarpList offsets: myrand(0, 2r), negated when < r
#   rect     4 m tiles with int(x0/4) <= tx <= int(x1/4), same for z
DEFAULT_POINTS = [
    {"name": "START_POSITION 21 (817,530) +[0..10]^2", "kind": "square", "x": 817, "z": 530,
     "size": 10, "min_share": 1.0},
    {"name": "inbound arrival (817,530) r5", "kind": "disc", "x": 817, "z": 530, "r": 5,
     "min_share": 1.0},
    {"name": "inbound arrival (817,530) SelectWarpList offsets", "kind": "arrival",
     "x": 817, "z": 530, "r": 5, "min_share": 1.0},
    {"name": "Folk Village (411,525) r5", "kind": "disc", "x": 411, "z": 525, "r": 5,
     "min_share": 0.85},
    {"name": "Tale Village (81,919) r5", "kind": "disc", "x": 81, "z": 919, "r": 5,
     "min_share": 0.85},
    {"name": "MINI_ARENA_RESPAWN (734,427) r5", "kind": "disc", "x": 734, "z": 427, "r": 5,
     "min_share": 0.95},
    {"name": "arena A tiles x 684-735 z 440-491", "kind": "rect", "x0": 684, "x1": 735,
     "z0": 440, "z1": 491, "min_share": 0.70},
    {"name": "arena B tiles x 684-735 z 360-411", "kind": "rect", "x0": 684, "x1": 735,
     "z0": 360, "z1": 411, "min_share": 0.70},
]


class ToolError(Exception):
    """An input or consistency problem that stops the command with a clear message."""


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------

def read_bytes(path, what):
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError as e:
        raise ToolError("cannot read %s '%s': %s" % (what, path, e.strerror or e))


def md5_hex(data):
    return hashlib.md5(data).hexdigest()


def unpack_from(fmt, data, off, what):
    try:
        return struct.unpack_from(fmt, data, off)
    except struct.error:
        raise ToolError("truncated data while reading %s at offset %d (size %d)" % (what, off, len(data)))


def file_info(path, data):
    return {"path": path, "size": len(data), "md5": md5_hex(data)}


def cstr(raw):
    return raw.split(b"\0", 1)[0].decode("latin-1")


def is_protected_map_dir(path):
    """True for any '<...>/server/Map' directory (live server maps; never written here)."""
    real = os.path.realpath(path)
    return (os.path.basename(real).lower() == "map"
            and os.path.basename(os.path.dirname(real)).lower() == "server")


def check_output_dir(out_dir, input_dirs):
    real = os.path.realpath(out_dir)
    if is_protected_map_dir(real):
        raise ToolError("refusing to write into a server Map directory: %s" % out_dir)
    for d in input_dirs:
        if d and os.path.isdir(d) and os.path.realpath(d) == real:
            raise ToolError("refusing to write into an input directory: %s" % out_dir)


def write_file(path, data):
    parent = os.path.dirname(os.path.abspath(path))
    os.makedirs(parent, exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def json_bytes(obj):
    return (json.dumps(obj, indent=2, sort_keys=True, ensure_ascii=True) + "\n").encode("ascii")


# ---------------------------------------------------------------------------
# Client files (.gtd / .opd / .opdext)
# ---------------------------------------------------------------------------

def client_payload_offsets(data):
    """Candidate data offsets after a client map-file header, most likely first."""
    cands = []
    if len(data) < 8:
        return cands
    a, b = struct.unpack_from("<ii", data, 0)
    if a in (1, 2) and 0 < b <= 64 and 8 + b <= len(data):
        if all(0x20 <= c < 0x7F for c in data[8:8 + b]):
            cands.append(8 + b)
    if 0 < a <= 64 and 4 + a + 4 <= len(data):
        cands.append(4 + a + 4)
    return cands


def plausible_map_size(n):
    return 2 <= n and (n - 1) * UNIT <= MAX_MAP_EXTENT


def parse_gtd(data, what, expected_n=None):
    """Heights of a client .gtd: n, then n*n {float32 h; uint32 tile}; index x*n+z."""
    for off in client_payload_offsets(data):
        if off + 4 > len(data):
            continue
        (n,) = struct.unpack_from("<i", data, off)
        if not plausible_map_size(n) or (expected_n is not None and n != expected_n):
            continue
        base = off + 4
        end = base + 8 * n * n
        if end > len(data):
            continue
        raw = data[base:end]
        heights_raw = b"".join([raw[i:i + 4] for i in range(0, 8 * n * n, 8)])
        heights = array.array("f")
        heights.frombytes(heights_raw)
        if sys.byteorder != "little":
            heights.byteswap()
        if not all(math.isfinite(v) for v in heights):
            raise ToolError("%s: non-finite height values" % what)
        return {"n": n, "offset": off, "heights": heights, "heights_raw": heights_raw,
                "mapdata_end": end}
    if expected_n is not None:
        raise ToolError("%s: no header variant gives terrain size n=%d" % (what, expected_n))
    raise ToolError("%s: no header variant gives a plausible terrain size" % what)


def parse_collision(data, off, what):
    """CN3ShapeMgr::LoadCollisionData mirror; returns offsets and the sub-cells with polygons.

    Main cells: z outer, x inner, 16 m each. Sub-cells (__CellMain::Load): z outer,
    x inner, 4 m each; sub-cell key (x*4+sx, z*4+sz) = the 4 m tile index.
    """
    w, l, nface = unpack_from("<ffi", data, off, what + " header")
    if not (0.0 < w <= MAX_MAP_EXTENT and 0.0 < l <= MAX_MAP_EXTENT):
        raise ToolError("%s: bad map extent %r x %r at offset %d" % (what, w, l, off))
    if nface < 0:
        raise ToolError("%s: negative face count %d" % (what, nface))
    p = off + 12 + 36 * nface
    if p > len(data):
        raise ToolError("%s: %d faces run past the end of the data" % (what, nface))
    subcells = {}
    main_cells = 0
    z = 0
    fz = 0.0
    while fz < l:
        x = 0
        fx = 0.0
        while fx < w:
            (exist,) = unpack_from("<I", data, p, what + " cell flag")
            p += 4
            if exist:
                main_cells += 1
                (nshape,) = unpack_from("<i", data, p, what + " shape count")
                if nshape < 0:
                    raise ToolError("%s: negative shape count at offset %d" % (what, p))
                p += 4 + 2 * nshape
                for sz in range(CELL_MAIN_DEVIDE):
                    for sx in range(CELL_MAIN_DEVIDE):
                        (npoly,) = unpack_from("<i", data, p, what + " sub-cell polygon count")
                        if npoly < 0:
                            raise ToolError("%s: negative polygon count at offset %d" % (what, p))
                        p += 4
                        if npoly:
                            subcells[(x * CELL_MAIN_DEVIDE + sx, z * CELL_MAIN_DEVIDE + sz)] = npoly
                            p += 12 * npoly
                if p > len(data):
                    raise ToolError("%s: cell data runs past the end of the data" % what)
            fx += CELL_MAIN_SIZE
            x += 1
        fz += CELL_MAIN_SIZE
        z += 1
    return {"offset": off, "end": p, "width": w, "length": l, "faces": nface,
            "main_cells": main_cells, "subcells": subcells}


def parse_opd_collision(data, what, n):
    """Collision block of a client .opd for a map of n vertices per side."""
    ext = (n - 1) * UNIT
    for off in client_payload_offsets(data):
        if off + 8 > len(data):
            continue
        w, l = struct.unpack_from("<ff", data, off)
        if w != ext or l != ext:
            continue
        coll = parse_collision(data, off, what + " collision block")
        coll["block"] = data[off:coll["end"]]
        return coll
    raise ToolError("%s: no header variant gives a %g x %g collision block (n=%d)" % (what, ext, ext, n))


def parse_opdext_faces(data, what, n):
    """Extra collision faces of a client .opdext: float w, float l, int32 count, faces."""
    ext = (n - 1) * UNIT
    for off in client_payload_offsets(data):
        if off + 12 > len(data):
            continue
        w, l, nface = struct.unpack_from("<ffi", data, off)
        if w != ext or l != ext or nface < 0 or off + 12 + 36 * nface > len(data):
            continue
        verts = struct.unpack_from("<%df" % (9 * nface), data, off + 12)
        return [((verts[9 * i], verts[9 * i + 2]), (verts[9 * i + 3], verts[9 * i + 5]),
                 (verts[9 * i + 6], verts[9 * i + 8])) for i in range(nface)]
    raise ToolError("%s: no header variant gives a %g x %g face block (n=%d)" % (what, ext, ext, n))


# ---------------------------------------------------------------------------
# Server SMD files
# ---------------------------------------------------------------------------

def parse_smd_layout(data, what, load_warps=True):
    """SMDFile::LoadMap mirror that returns the offset of every block."""
    n, unit = unpack_from("<if", data, 0, what + " terrain header")
    if not plausible_map_size(n):
        raise ToolError("%s: implausible map size n=%d" % (what, n))
    coll_off = 8 + 4 * n * n
    if coll_off > len(data):
        raise ToolError("%s: height map runs past the end of the file" % what)
    coll = parse_collision(data, coll_off, what + " collision block")
    if (n - 1) * unit != coll["width"]:
        raise ToolError("%s: size check failed, (n-1)*unit=%r but collision width=%r "
                        "(SMDFile.cpp:81-85)" % (what, (n - 1) * unit, coll["width"]))
    p = coll["end"]
    obj_off = p
    (nobj,) = unpack_from("<i", data, p, what + " object event count")
    if nobj < 0:
        raise ToolError("%s: negative object event count" % what)
    p += 4 + OBJECT_EVENT_SIZE * nobj
    events_off = p
    p += 2 * n * n
    if p > len(data):
        raise ToolError("%s: object events or event grid run past the end of the file" % what)
    lay = {"n": n, "unit": unit, "collision": coll, "object_events_offset": obj_off,
           "object_event_count": nobj, "events_offset": events_off, "regene_offset": None,
           "regene_count": 0, "warps_offset": None, "warp_count": 0, "warp_offsets": []}
    if load_warps:
        lay["regene_offset"] = p
        (nreg,) = unpack_from("<i", data, p, what + " regene count")
        if nreg < 0:
            raise ToolError("%s: negative regene count" % what)
        p += 4 + REGENE_EVENT_SIZE * nreg
        (nwarp,) = unpack_from("<i", data, p, what + " warp count")
        if nwarp < 0:
            raise ToolError("%s: negative warp count" % what)
        p += 4
        lay["regene_count"] = nreg
        lay["warps_offset"] = p
        lay["warp_count"] = nwarp
        # SMDFile::LoadWarpList stops silently at EOF inside the list (SMDFile.cpp:164-172).
        complete = min(nwarp, max(0, (len(data) - p) // WARP_SIZE))
        lay["warp_offsets"] = [p + WARP_SIZE * i for i in range(complete)]
        p += WARP_SIZE * complete
    lay["end"] = p
    lay["trailing"] = len(data) - p
    return lay


def smd_heights(data, lay):
    h = array.array("f")
    h.frombytes(data[8:8 + 4 * lay["n"] * lay["n"]])
    if sys.byteorder != "little":
        h.byteswap()
    return h


def smd_events(data, lay):
    off = lay["events_offset"]
    ev = array.array("h")
    ev.frombytes(data[off:off + 2 * lay["n"] * lay["n"]])
    if sys.byteorder != "little":
        ev.byteswap()
    return ev


def load_smd(path, what, load_warps=True):
    data = read_bytes(path, what)
    lay = parse_smd_layout(data, "%s '%s'" % (what, path), load_warps)
    return data, lay


def warp_fields(rec):
    (wid,) = struct.unpack_from("<h", rec, 0)
    (pay,) = struct.unpack_from("<I", rec, WARP_OFF_PAY)
    (zone,) = struct.unpack_from("<h", rec, WARP_OFF_ZONE)
    fx, fy, fz, fr, nation = struct.unpack_from("<ffffh", rec, WARP_OFF_X)
    return {"id": wid, "name": cstr(rec[WARP_OFF_NAME:WARP_OFF_NAME + WARP_NAME_LEN]),
            "announce": cstr(rec[WARP_OFF_ANNOUNCE:WARP_OFF_ANNOUNCE + WARP_ANNOUNCE_LEN]),
            "pay": pay, "zone": zone, "x": fx, "y": fy, "z": fz, "r": fr, "nation": nation}


def smd_warp_records(data, lay):
    return [data[off:off + WARP_SIZE] for off in lay["warp_offsets"]]


def make_warp(entry, where):
    """One 320-byte _WARP_INFO from a spec entry."""
    try:
        wid = int(entry["id"])
        name = str(entry["name"]).encode("latin-1")
        announce = str(entry.get("announce", "")).encode("latin-1")
        pay = int(entry["pay"])
        zone = int(entry["zone"])
        x = float(entry["x"])
        y = float(entry.get("y", 0.0))
        z = float(entry["z"])
        r = float(entry.get("r", 5.0))
        nation = int(entry["nation"])
    except KeyError as e:
        raise ToolError("%s: missing field %s" % (where, e))
    except (TypeError, ValueError, UnicodeEncodeError) as e:
        raise ToolError("%s: bad value (%s)" % (where, e))
    if not (0 < wid <= 32767) or not (0 <= pay <= 0xFFFFFFFF) or not (-32768 <= zone <= 32767):
        raise ToolError("%s: id, pay or zone out of range" % where)
    if len(name) >= WARP_NAME_LEN or len(announce) >= WARP_ANNOUNCE_LEN:
        raise ToolError("%s: name (max %d bytes) or announce (max %d bytes) too long"
                        % (where, WARP_NAME_LEN - 1, WARP_ANNOUNCE_LEN - 1))
    rec = bytearray(WARP_SIZE)
    struct.pack_into("<h", rec, 0, wid)
    rec[WARP_OFF_NAME:WARP_OFF_NAME + len(name)] = name
    rec[WARP_OFF_ANNOUNCE:WARP_OFF_ANNOUNCE + len(announce)] = announce
    struct.pack_into("<HIhHffffhH", rec, 290, 0, pay, zone, 0, x, y, z, r, nation, 0)
    return bytes(rec)


def find_fee_counterpart(f, ours):
    """Our old-Moradon warp to the same destination (group, target zone, name), or None."""
    key = (f["id"] // 10, f["zone"], f["name"].strip().lower())
    cands = [o for o in ours if (o["id"] // 10, o["zone"], o["name"].strip().lower()) == key]
    if not cands:
        return None
    same_id = [o for o in cands if o["id"] == f["id"]]
    return (same_id or sorted(cands, key=lambda o: o["id"]))[0]


def build_warp_list(donor_recs, drop_zones, ours, spec):
    """Returns (records, info rows, dropped rows)."""
    records = []
    info = []
    dropped = []
    if spec is not None:
        if not isinstance(spec, list) or not spec:
            raise ToolError("warp spec: expected a non-empty JSON list")
        for i, entry in enumerate(spec):
            if not isinstance(entry, dict):
                raise ToolError("warp spec entry %d: expected an object" % i)
            rec = make_warp(entry, "warp spec entry %d" % i)
            f = warp_fields(rec)
            if f["zone"] in drop_zones:
                dropped.append({"id": f["id"], "name": f["name"], "zone": f["zone"]})
                continue
            records.append(rec)
            info.append(dict(f, alpha_pay=None, pay_source="spec"))
    else:
        for rec in donor_recs:
            f = warp_fields(rec)
            if f["zone"] in drop_zones:
                dropped.append({"id": f["id"], "name": f["name"], "zone": f["zone"]})
                continue
            pay = f["pay"]
            if ours is None:
                source = "alpha (--fees-from alpha)"
            else:
                o = find_fee_counterpart(f, ours)
                if o is None:
                    source = "alpha (no counterpart in our old Moradon)"
                else:
                    pay = o["pay"]
                    source = "ours:%d" % o["id"]
            new = bytearray(rec)
            struct.pack_into("<I", new, WARP_OFF_PAY, pay)
            records.append(bytes(new))
            info.append(dict(warp_fields(new), alpha_pay=f["pay"], pay_source=source))
    ids = [warp_fields(r)["id"] for r in records]
    if not records:
        raise ToolError("the warp list is empty after dropping zones %s" % sorted(drop_zones))
    if 0 in ids or len(set(ids)) != len(ids):
        raise ToolError("warp ids must be non-zero and unique (SMDFile::LoadWarpList drops "
                        "the others): %s" % ids)
    for row in info:
        row.pop("announce", None)
        row.pop("y", None)
    return records, info, dropped


# ---------------------------------------------------------------------------
# Rule R and grid comparisons
# ---------------------------------------------------------------------------

def rule_r_events(n, heights, subcells, threshold=SLOPE_RANGE_M):
    """Editor walkability rule (G section 4.2); edges row/column n-1 stay 1."""
    ev = array.array("h", [1]) * (n * n)
    for x in range(n - 1):
        row = x * n
        nxt = row + n
        for z in range(n - 1):
            if (x, z) in subcells:
                ev[row + z] = 0
                continue
            a = heights[row + z]
            b = heights[nxt + z]
            c = heights[row + z + 1]
            d = heights[nxt + z + 1]
            if max(a, b, c, d) - min(a, b, c, d) >= threshold:
                ev[row + z] = 0
    return ev


def grid_agreement(official, generated):
    """(agree, total, official0_gen_open, official_open_gen0); values >= 1 count as open."""
    o0_g1 = 0
    o1_g0 = 0
    for a, b in zip(official, generated):
        if (a == 0) != (b == 0):
            if a == 0:
                o0_g1 += 1
            else:
                o1_g0 += 1
    total = len(official)
    return total - o0_g1 - o1_g0, total, o0_g1, o1_g0


def walkable_share(n, ev):
    """Share of walkable (event != 0) tiles inside [0, n-1) x [0, n-1)."""
    ok = 0
    for x in range(n - 1):
        row = x * n
        for z in range(n - 1):
            if ev[row + z] != 0:
                ok += 1
    return ok, (n - 1) * (n - 1)


def is_walkable(n, ev, wx, wz):
    tx = int(wx / UNIT)
    tz = int(wz / UNIT)
    if wx < 0 or wz < 0 or tx >= n or tz >= n:
        return False
    return ev[tx * n + tz] != 0


def arrival_offsets(r):
    """Offsets of CUser::SelectWarpList: v = myrand(0, 2r) (inclusive); v < r -> -v."""
    out = set()
    for v in range(0, int(r) * 2 + 1):
        out.add(-v if v < r else v)
    return sorted(out)


def sample_points(p):
    kind = p["kind"]
    x = float(p.get("x", 0))
    z = float(p.get("z", 0))
    if kind == "square":
        s = int(p["size"])
        return [(x + dx, z + dz) for dx in range(s + 1) for dz in range(s + 1)]
    if kind == "disc":
        r = int(p["r"])
        return [(x + dx, z + dz) for dx in range(-r, r + 1) for dz in range(-r, r + 1)
                if dx * dx + dz * dz <= r * r]
    if kind == "arrival":
        offs = arrival_offsets(float(p["r"]))
        return [(x + dx, z + dz) for dx in offs for dz in offs]
    if kind == "rect":
        return [(tx * UNIT, tz * UNIT)
                for tx in range(int(p["x0"] / UNIT), int(p["x1"] / UNIT) + 1)
                for tz in range(int(p["z0"] / UNIT), int(p["z1"] / UNIT) + 1)]
    raise ToolError("unknown point kind '%s'" % kind)


def tri_overlaps_square(tri, x0, z0, x1, z1):
    """Separating-axis test in x-z; touching at an edge or corner does not count."""
    xs = [q[0] for q in tri]
    zs = [q[1] for q in tri]
    if max(xs) <= x0 or min(xs) >= x1 or max(zs) <= z0 or min(zs) >= z1:
        return False
    corners = ((x0, z0), (x1, z0), (x0, z1), (x1, z1))
    for i in range(3):
        ax, az = tri[i]
        bx, bz = tri[(i + 1) % 3]
        nx, nz = bz - az, ax - bx
        if nx == 0.0 and nz == 0.0:
            continue
        tp = [nx * q[0] + nz * q[1] for q in tri]
        sp = [nx * c[0] + nz * c[1] for c in corners]
        if max(tp) <= min(sp) or max(sp) <= min(tp):
            return False
    return True


def faces_tiles(faces, n):
    """4 m tiles in [0, n-1)^2 whose square overlaps a face's x-z projection."""
    tiles = set()
    last = n - 2
    for tri in faces:
        xs = [q[0] for q in tri]
        zs = [q[1] for q in tri]
        tx0 = max(0, int(math.floor(min(xs) / UNIT)))
        tx1 = min(last, int(math.floor(max(xs) / UNIT)))
        tz0 = max(0, int(math.floor(min(zs) / UNIT)))
        tz1 = min(last, int(math.floor(max(zs) / UNIT)))
        for tx in range(tx0, tx1 + 1):
            for tz in range(tz0, tz1 + 1):
                if (tx, tz) not in tiles and tri_overlaps_square(
                        tri, tx * UNIT, tz * UNIT, (tx + 1) * UNIT, (tz + 1) * UNIT):
                    tiles.add((tx, tz))
    return tiles


# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------

def assemble_smd(n, heights_raw, coll_block, obj_block, events, warps):
    ev = array.array("h", events)
    if sys.byteorder != "little":
        ev.byteswap()
    out = bytearray()
    out += struct.pack("<if", n, UNIT)
    out += heights_raw
    out += coll_block
    out += obj_block
    out += ev.tobytes()
    out += struct.pack("<i", 0)
    out += struct.pack("<i", len(warps))
    out += b"".join(warps)
    return bytes(out)


def build_smd_bytes(gtd_data, opd_data, donor_data, ours_data, spec, drop_zones, names=None,
                    expected_n=N_EXPECTED):
    """Pure build: returns (smd bytes, info dict). ours_data None = keep donor fees."""
    names = names or {}
    gtd = parse_gtd(gtd_data, names.get("gtd", "client .gtd"), expected_n=expected_n)
    n = gtd["n"]
    coll = parse_opd_collision(opd_data, names.get("opd", "client .opd"), n)
    dname = "donor '%s'" % names.get("donor", "donor")
    dlay = parse_smd_layout(donor_data, dname)
    if dlay["n"] != n:
        raise ToolError("%s: map size n=%d, the client map has n=%d" % (dname, dlay["n"], n))
    if dlay["trailing"] != 0 or len(dlay["warp_offsets"]) != dlay["warp_count"]:
        raise ToolError("%s: warp list incomplete or trailing bytes present" % dname)
    obj_off = dlay["object_events_offset"]
    obj_block = donor_data[obj_off:obj_off + 4 + OBJECT_EVENT_SIZE * dlay["object_event_count"]]
    ours = None
    if ours_data is not None:
        oname = "fee source '%s'" % names.get("ours", "fees")
        olay = parse_smd_layout(ours_data, oname)
        ours = [warp_fields(r) for r in smd_warp_records(ours_data, olay)]
    warps, warp_info, dropped = build_warp_list(smd_warp_records(donor_data, dlay), drop_zones, ours, spec)
    events = rule_r_events(n, gtd["heights"], coll["subcells"])
    out = assemble_smd(n, gtd["heights_raw"], coll["block"], obj_block, events, warps)
    expected_size = (8 + 4 * n * n + len(coll["block"]) + len(obj_block) + 2 * n * n + 4 + 4
                     + WARP_SIZE * len(warps))
    if len(out) != expected_size:
        raise ToolError("internal: output size %d != expected %d" % (len(out), expected_size))
    lay = parse_smd_layout(out, "generated SMD")
    if lay["trailing"] != 0 or len(lay["warp_offsets"]) != len(warps):
        raise ToolError("internal: the generated SMD does not parse back cleanly")
    walk, tiles = walkable_share(n, events)
    zeros = sum(1 for v in events if v == 0)
    info = {
        "counts": {
            "n": n, "unit": UNIT, "heights": n * n,
            "collision_faces": coll["faces"], "collision_bytes": len(coll["block"]),
            "collision_md5": md5_hex(coll["block"]), "collision_main_cells": coll["main_cells"],
            "collision_subcells_with_polys": len(coll["subcells"]),
            "object_events": dlay["object_event_count"], "regene": 0, "warps": len(warps),
            "events_0": zeros, "events_1": n * n - zeros,
            "walkable_tiles": walk, "tiles": tiles,
        },
        "layout": {
            "heights": [8, 8 + 4 * n * n],
            "collision": [lay["collision"]["offset"], lay["collision"]["end"]],
            "object_events": [lay["object_events_offset"], lay["events_offset"]],
            "events": [lay["events_offset"], lay["events_offset"] + 2 * n * n],
            "regene": [lay["regene_offset"], lay["regene_offset"] + 4],
            "warps": [lay["warps_offset"] - 4, lay["end"]],
        },
        "parameters": {"slope_range_m": SLOPE_RANGE_M, "drop_zones": sorted(drop_zones),
                       "edge_value": 1, "client_header_offset_gtd": gtd["offset"],
                       "client_header_offset_opd": coll["offset"]},
        "warps": warp_info,
        "dropped_warps": dropped,
    }
    return out, info


def parse_zone_list(text, what):
    out = set()
    for part in text.split(","):
        part = part.strip()
        if not part:
            continue
        try:
            out.add(int(part))
        except ValueError:
            raise ToolError("%s: '%s' is not a zone number" % (what, part))
    return out


def load_spec(path):
    if not path:
        return None
    try:
        return json.loads(read_bytes(path, "warp spec").decode("utf-8"))
    except ValueError as e:
        raise ToolError("warp spec '%s' is not valid JSON: %s" % (path, e))


def cmd_build(args):
    zones = args.client_zones
    gtd_path = os.path.join(zones, CLIENT_MAP_NAME + ".gtd")
    opd_path = os.path.join(zones, CLIENT_MAP_NAME + ".opd")
    gtd_data = read_bytes(gtd_path, "client heights")
    opd_data = read_bytes(opd_path, "client collision")
    donor_data = read_bytes(args.donor, "donor SMD")
    spec = load_spec(args.warps)
    drop = parse_zone_list(args.drop_zones, "--drop-zones")
    ours_data = None
    ours_path = None
    if args.fees_from.lower() != "alpha" and spec is None:
        ours_path = args.fees_from or os.path.join(args.map_dir, OLD_MORADON_SMD)
        ours_data = read_bytes(ours_path, "fee source SMD (our old Moradon; or --fees-from alpha)")
    out_path = args.out
    check_output_dir(os.path.dirname(os.path.abspath(out_path)),
                     [zones, os.path.dirname(os.path.abspath(args.donor)), args.map_dir])
    names = {"gtd": gtd_path, "opd": opd_path, "donor": args.donor, "ours": ours_path}
    out, info = build_smd_bytes(gtd_data, opd_data, donor_data, ours_data, spec, drop, names)
    write_file(out_path, out)
    sidecar_path = os.path.splitext(out_path)[0] + ".json"
    side = {
        "tool": "tools/u3-moradon-smd.py build",
        "plan": "U3-01",
        "inputs": {
            "gtd": file_info(gtd_path, gtd_data),
            "opd": file_info(opd_path, opd_data),
            "donor": file_info(args.donor, donor_data),
            "fees_from": file_info(ours_path, ours_data) if ours_data is not None else (
                "spec" if spec is not None else "alpha"),
            "warps_spec": file_info(args.warps, read_bytes(args.warps, "warp spec")) if args.warps else None,
        },
        "output": file_info(out_path, out),
    }
    side.update(info)
    write_file(sidecar_path, json_bytes(side))
    c = info["counts"]
    print("BUILD %s bytes=%d md5=%s" % (out_path, len(out), md5_hex(out)))
    print("  n=%d unit=%.1f faces=%d collision_bytes=%d object_events=%d regene=0 warps=%d"
          % (c["n"], c["unit"], c["collision_faces"], c["collision_bytes"], c["object_events"], c["warps"]))
    print("  event grid: 0=%d 1=%d walkable tiles %d/%d (%.1f%%)"
          % (c["events_0"], c["events_1"], c["walkable_tiles"], c["tiles"],
             100.0 * c["walkable_tiles"] / c["tiles"]))
    for w in info["warps"]:
        print("  warp %d group %d %-18s zone %2d (%.1f,%.1f) r%.0f nation %d pay %d (alpha %s) <- %s"
              % (w["id"], w["id"] // 10, w["name"], w["zone"], w["x"], w["z"], w["r"], w["nation"],
                 w["pay"], w["alpha_pay"], w["pay_source"]))
    for d in info["dropped_warps"]:
        print("  dropped warp %d %s -> zone %d (no ZONE_INFO row)" % (d["id"], d["name"], d["zone"]))
    print("  sidecar %s" % sidecar_path)
    return 0


# ---------------------------------------------------------------------------
# patch-inbound
# ---------------------------------------------------------------------------

def allowed_patch_bytes(rec_off):
    return set(range(rec_off + WARP_OFF_X, rec_off + WARP_OFF_X + 4)) | \
        set(range(rec_off + WARP_OFF_Z, rec_off + WARP_OFF_Z + 4))


def diff_positions(a, b, chunk=65536):
    if len(a) != len(b):
        raise ToolError("internal: diff of different sizes")
    out = []
    for start in range(0, len(a), chunk):
        if a[start:start + chunk] != b[start:start + chunk]:
            out.extend(i for i in range(start, min(len(a), start + chunk)) if a[i] != b[i])
    return out


def patch_inbound_bytes(data, what, x, z):
    """Returns (patched bytes, record rows); only fX/fZ of sZone==21 records change."""
    lay = parse_smd_layout(data, what)
    new = bytearray(data)
    rows = []
    allowed = set()
    for off in lay["warp_offsets"]:
        f = warp_fields(data[off:off + WARP_SIZE])
        if f["zone"] != ZONE_MORADON:
            continue
        struct.pack_into("<f", new, off + WARP_OFF_X, x)
        struct.pack_into("<f", new, off + WARP_OFF_Z, z)
        allowed |= allowed_patch_bytes(off)
        changed = [i for i in range(off, off + WARP_SIZE) if new[i] != data[i]]
        rows.append({"id": f["id"], "name": f["name"], "record_offset": off,
                     "old": [f["x"], f["z"]], "new": [x, z], "changed_bytes": len(changed),
                     "changed_offsets": changed})
    new = bytes(new)
    stray = [i for i in diff_positions(data, new) if i not in allowed]
    if stray:
        raise ToolError("internal: %s: bytes outside fX/fZ changed: %s" % (what, stray[:8]))
    return new, rows


def cmd_patch_inbound(args):
    files = [s.strip() for s in args.files.split(",") if s.strip()]
    if not files:
        raise ToolError("--files is empty")
    check_output_dir(args.out_dir, [args.map_dir])
    x = float(args.x)
    z = float(args.z)
    report = {"tool": "tools/u3-moradon-smd.py patch-inbound", "plan": "U3-01",
              "target": [x, z], "map_dir": args.map_dir, "files": []}
    total = 0
    for name in files:
        if os.path.basename(name) != name:
            raise ToolError("--files takes bare file names inside --map-dir: '%s'" % name)
        src = os.path.join(args.map_dir, name)
        dst = os.path.join(args.out_dir, name)
        data = read_bytes(src, "map")
        if os.path.exists(dst) and os.path.samefile(src, dst):
            raise ToolError("refusing to overwrite the source map %s" % src)
        new, rows = patch_inbound_bytes(data, "'%s'" % src, x, z)
        if not rows:
            raise ToolError("%s has no warp record with sZone == %d; wrong file?" % (src, ZONE_MORADON))
        write_file(dst, new)
        total += len(rows)
        diffs = diff_positions(data, new)
        print("PATCH %s -> %s bytes=%d md5 %s -> %s, records=%d, changed bytes=%d"
              % (name, dst, len(new), md5_hex(data), md5_hex(new), len(rows), len(diffs)))
        for r in rows:
            print("  warp %d %-10s @%d (%.1f,%.1f) -> (%.1f,%.1f) changed %d B at %s"
                  % (r["id"], r["name"], r["record_offset"], r["old"][0], r["old"][1],
                     r["new"][0], r["new"][1], r["changed_bytes"], r["changed_offsets"]))
        report["files"].append({"name": name, "source": file_info(src, data),
                                "output": file_info(dst, new), "records": rows,
                                "changed_bytes": len(diffs)})
    side = os.path.join(args.out_dir, "inbound_patch.json")
    write_file(side, json_bytes(report))
    print("PATCH OK files=%d records=%d report=%s" % (len(files), total, side))
    return 0


# ---------------------------------------------------------------------------
# verify
# ---------------------------------------------------------------------------

def import_smd_parse():
    if SMD_PARSE_DIR not in sys.path:
        sys.path.insert(0, SMD_PARSE_DIR)
    import smd_parse  # noqa: E402  (repo parser that mirrors SMDFile/N3ShapeMgr)
    return smd_parse


def import_nav_export():
    spec = importlib.util.spec_from_file_location("fdp_nav_export", NAV_EXPORT_PATH)
    if spec is None:
        raise ToolError("cannot load %s" % NAV_EXPORT_PATH)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def nav_crc32(nav, smd_path):
    """crc32 that tools/nav-export.py prints for this SMD (NavService fingerprint input)."""
    res = nav.parse(smd_path, load_warps=False)
    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, "zone71.navgrid")
        nav.write_navgrid(out, res["m_nMapSize"], res["m_fUnitDist"], res["events"], res["height"])
        back = nav.read_navgrid(out)
    return zlib.crc32(back["bytes"]) & 0xFFFFFFFF


def read_pipe_table(path, what):
    """Rows of a SQLCMD '-W -s |' export: header line, dash line, data lines."""
    text = read_bytes(path, what).decode("latin-1")
    header = None
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        cells = [c.strip() for c in line.split("|")]
        if header is None:
            header = [c.lower() for c in cells]
            continue
        if all(set(c) <= set("-") for c in cells):
            continue
        if len(cells) != len(header):
            continue
        rows.append(dict(zip(header, cells)))
    if header is None:
        raise ToolError("%s '%s' is empty" % (what, path))
    return header, rows


def load_zone_info(path):
    if not path:
        return dict(ZONE_INFO_SNAPSHOT), "built-in snapshot of FDP_kn_online ZONE_INFO (2026-10-08)"
    header, rows = read_pipe_table(path, "ZONE_INFO export")
    if "zoneno" not in header or "strzonename" not in header:
        raise ToolError("ZONE_INFO export '%s' needs ZoneNo and strZoneName columns" % path)
    return {int(r["zoneno"]): r["strzonename"] for r in rows}, "ZONE_INFO export %s" % path


class Check:
    def __init__(self, cid, title, expected):
        self.cid = cid
        self.title = title
        self.expected = expected
        self.status = "PASS"
        self.measured = []
        self.details = []
        self.warnings = 0

    def fail(self, msg):
        self.status = "FAIL"
        self.details.append("FAIL: " + msg)

    def skip(self, msg):
        if self.status != "FAIL":
            self.status = "SKIP"
        self.details.append("SKIP: " + msg)

    def warn(self, msg):
        self.warnings += 1
        self.details.append("WARN: " + msg)


def rule_reference_tests(map_dir, client_zones, log):
    """Rule R on reference SMDs (expect 100%) and client->grid end to end (>= 99.99%).

    Returns (failures, skips) as message lists; log(msg) receives one line per test.
    """
    failures = []
    skips = []
    for label, fn in RULE_REFERENCE_MAPS:
        path = os.path.join(map_dir, fn)
        if not os.path.isfile(path):
            skips.append("rule R %s: %s missing" % (label, path))
            log("SKIP rule R %s: %s missing" % (label, path))
            continue
        data, lay = load_smd(path, "reference map", load_warps=False)
        gen = rule_r_events(lay["n"], smd_heights(data, lay), lay["collision"]["subcells"])
        agree, total, o0, o1 = grid_agreement(smd_events(data, lay), gen)
        ok = agree == total
        log("%s rule R %-24s %s n=%d agree %d/%d (%.4f%%) official0/gen1 %d official1/gen0 %d"
            % ("OK  " if ok else "FAIL", label, fn, lay["n"], agree, total, 100.0 * agree / total, o0, o1))
        if not ok:
            failures.append("rule R %s: %d/%d" % (label, agree, total))
    for label, client, fn in E2E_REFERENCE_MAPS:
        path = os.path.join(map_dir, fn)
        gtd_path = os.path.join(client_zones, client + ".gtd")
        opd_path = os.path.join(client_zones, client + ".opd")
        missing = [p for p in (path, gtd_path, opd_path) if not os.path.isfile(p)]
        if missing:
            skips.append("e2e %s: missing %s" % (label, ", ".join(missing)))
            log("SKIP e2e %s: missing %s" % (label, ", ".join(missing)))
            continue
        data, lay = load_smd(path, "reference map", load_warps=False)
        n = lay["n"]
        gtd = parse_gtd(read_bytes(gtd_path, "client heights"), gtd_path, expected_n=n)
        coll = parse_opd_collision(read_bytes(opd_path, "client collision"), opd_path, n)
        gen = rule_r_events(n, gtd["heights"], coll["subcells"])
        agree, total, o0, o1 = grid_agreement(smd_events(data, lay), gen)
        heq = sum(1 for a, b in zip(gtd["heights"], smd_heights(data, lay)) if abs(a - b) < 1e-3)
        ok = agree >= E2E_MIN_AGREEMENT * total
        log("%s e2e %-24s %s.gtd/.opd -> %s n=%d heights equal %d/%d, grid agree %d/%d (%.4f%%) "
            "official0/gen1 %d official1/gen0 %d"
            % ("OK  " if ok else "FAIL", label, client, fn, n, heq, n * n, agree, total,
               100.0 * agree / total, o0, o1))
        if not ok:
            failures.append("e2e %s: %d/%d" % (label, agree, total))
    return failures, skips


def cmd_verify(args):
    smd_parse = import_smd_parse()
    out_path = args.smd
    out_data, lay = load_smd(out_path, "generated SMD")
    n = lay["n"]
    heights = smd_heights(out_data, lay)
    ev = smd_events(out_data, lay)
    zones = args.client_zones
    gtd_path = os.path.join(zones, CLIENT_MAP_NAME + ".gtd")
    opd_path = os.path.join(zones, CLIENT_MAP_NAME + ".opd")
    opdext_path = os.path.join(zones, CLIENT_MAP_NAME + ".opdext")
    gtd_data = read_bytes(gtd_path, "client heights")
    opd_data = read_bytes(opd_path, "client collision")
    donor_data = read_bytes(args.donor, "donor SMD")
    dlay = parse_smd_layout(donor_data, "donor '%s'" % args.donor)
    gtd = parse_gtd(gtd_data, gtd_path, expected_n=N_EXPECTED)
    coll = parse_opd_collision(opd_data, opd_path, gtd["n"])
    spec = load_spec(args.warps)
    drop = parse_zone_list(args.drop_zones, "--drop-zones")
    zone_info, zone_info_src = load_zone_info(args.zone_info)
    checks = []

    # V1 loader mirror (repo smd_parse.py)
    c = Check("V1", "smd_parse.parse: size check, trailing bytes, counts",
              "ok / 0 / faces 26839, obj 29, regene 0, warps 14")
    res = smd_parse.parse(out_path, load_warps=True)
    if spec is not None:
        exp_ids = [warp_fields(r)["id"] for r in build_warp_list([], drop, None, spec)[0]]
    else:
        exp_ids = [warp_fields(r)["id"] for r in smd_warp_records(donor_data, dlay)
                   if warp_fields(r)["zone"] not in drop]
    got_ids = [w["sWarpID"] for w in res["warps"]]
    c.measured.append("%s / %d / n %d unit %.1f faces %d, obj %d, regene %d, warps %d, size %d"
                      % ("ok" if res["size_check_ok"] else "BAD", res["trailing_bytes"], res["m_nMapSize"],
                         res["m_fUnitDist"], res["coll_faces"], res["object_event_count"],
                         len(res["regene"]), len(res["warps"]), res["size_bytes"]))
    exp_size = (8 + 6 * n * n + (coll["end"] - coll["offset"]) + 4
                + OBJECT_EVENT_SIZE * dlay["object_event_count"] + 8 + WARP_SIZE * len(exp_ids))
    if not res["size_check_ok"]:
        c.fail("size check")
    if res["trailing_bytes"] != 0 or res["warp_note"]:
        c.fail("trailing bytes %d %s" % (res["trailing_bytes"], res["warp_note"]))
    if res["m_nMapSize"] != N_EXPECTED or res["m_fUnitDist"] != UNIT:
        c.fail("n/unit")
    if res["coll_faces"] != coll["faces"]:
        c.fail("faces %d != client %d" % (res["coll_faces"], coll["faces"]))
    if res["object_event_count"] != dlay["object_event_count"]:
        c.fail("object events %d != donor %d" % (res["object_event_count"], dlay["object_event_count"]))
    if res["regene"]:
        c.fail("regene %d != 0" % len(res["regene"]))
    if sorted(got_ids) != sorted(exp_ids) or len(set(got_ids)) != len(got_ids) or 0 in got_ids:
        c.fail("warp ids %s != expected %s" % (got_ids, exp_ids))
    if res["size_bytes"] != exp_size:
        c.fail("size %d != expected %d" % (res["size_bytes"], exp_size))
    checks.append(c)

    # V2 heights
    c = Check("V2", "heights == client .gtd (bit exact); diagnostic ALPHA transposed",
              "66049/66049; ALPHA^T 88.9%")
    out_hraw = out_data[8:8 + 4 * n * n]
    eq = sum(1 for i in range(0, 4 * n * n, 4) if out_hraw[i:i + 4] == gtd["heights_raw"][i:i + 4])
    ah = smd_heights(donor_data, dlay)
    if dlay["n"] == n:
        at = sum(1 for x in range(n) for z in range(n) if abs(heights[x * n + z] - ah[z * n + x]) < 1e-3)
        ad = sum(1 for i in range(n * n) if abs(heights[i] - ah[i]) < 1e-3)
    else:
        at = ad = 0
    c.measured.append("%d/%d; ALPHA^T %d (%.1f%%), ALPHA direct %d (%.1f%%)"
                      % (eq, n * n, at, 100.0 * at / (n * n), ad, 100.0 * ad / (n * n)))
    if eq != n * n:
        c.fail("heights differ from the client .gtd")
    if at < V2_MIN_ALPHA_TRANSPOSED * n * n:
        c.fail("ALPHA^T agreement below %.0f%%: wrong client folder or donor?" % (100 * V2_MIN_ALPHA_TRANSPOSED))
    checks.append(c)

    # V3 collision block
    c = Check("V3", "collision block md5 == client .opd block", "equal")
    out_block = out_data[lay["collision"]["offset"]:lay["collision"]["end"]]
    c.measured.append("out %s (%d B) / opd %s (%d B @%d)"
                      % (md5_hex(out_block), len(out_block), md5_hex(coll["block"]), len(coll["block"]),
                         coll["offset"]))
    if out_block != coll["block"]:
        c.fail("collision block differs")
    checks.append(c)

    # V4 rule R
    c = Check("V4", "rule R: own data, reference maps 100%, client e2e >= 99.99%",
              "100% (old Moradon, 71, 72); e2e >= 99.99% (71, 72)")
    own = rule_r_events(n, heights, lay["collision"]["subcells"])
    agree, total, _, _ = grid_agreement(ev, own)
    exact_own = list(own) == list(ev)
    c.details.append("own grid == rule R(own heights, own collision): %d/%d, values identical %s"
                     % (agree, total, exact_own))
    if not exact_own:
        c.fail("the SMD grid is not rule R of its own data")
    fails, skips = rule_reference_tests(args.map_dir, zones, c.details.append)
    for m in fails:
        c.fail(m)
    for m in skips:
        c.skip(m)
    c.measured.append("own %d/%d; references %d failed, %d skipped" % (agree, total, len(fails), len(skips)))
    checks.append(c)

    # V5 reference points
    points = DEFAULT_POINTS
    if args.points:
        try:
            points = json.loads(read_bytes(args.points, "points").decode("utf-8"))
        except ValueError as e:
            raise ToolError("points '%s' is not valid JSON: %s" % (args.points, e))
    c = Check("V5", "START, inbound, Folk/Tale, MINI_ARENA, arena rectangles (walkable)",
              "121/121, 81/81, 80/81, 73/81, 81/81, 127/169")
    summary = []
    for p in points:
        pts = sample_points(p)
        ok = sum(1 for (px, pz) in pts if is_walkable(n, ev, px, pz))
        need = float(p.get("min_share", 1.0))
        summary.append("%d/%d" % (ok, len(pts)))
        line = "%-50s %3d/%3d (min %.0f%%)" % (p["name"], ok, len(pts), 100 * need)
        c.details.append(line)
        if ok < need * len(pts):
            c.fail(line)
    c.measured.append(", ".join(summary))
    checks.append(c)

    # V6 spawns
    c = Check("V6", "ALPHA K_NPCPOS zone 21 monster spawn centres walkable", ">= 64/69")
    if not args.spawns:
        c.skip("no --spawns file (SQLCMD export of K_NPCPOS zone 21, see --help)")
        c.measured.append("-")
    else:
        header, rows = read_pipe_table(args.spawns, "spawn export")
        need_cols = ("npcid", "leftx", "topz", "rightx", "bottomz")
        if any(k not in header for k in need_cols) or ("kind" not in header and "acttype" not in header):
            raise ToolError("spawn export needs NpcID, LeftX, TopZ, RightX, BottomZ and kind or ActType")
        rule = "kind column (M = monster)" if "kind" in header else "ActType < 100 (AIServer ServerDlg.cpp:268)"
        mon = mon_ok = npc = npc_ok = 0
        bad = []
        for r in rows:
            try:
                cx = (int(r["leftx"]) + int(r["rightx"])) / 2.0
                cz = (int(r["topz"]) + int(r["bottomz"])) / 2.0
                is_mon = (r["kind"].upper() == "M") if "kind" in header else int(r["acttype"]) < 100
            except ValueError:
                continue
            walk = is_walkable(n, ev, cx, cz)
            if is_mon:
                mon += 1
                mon_ok += walk
                if not walk:
                    bad.append("%s(%.0f,%.0f)" % (r["npcid"], cx, cz))
            else:
                npc += 1
                npc_ok += walk
        c.measured.append("monsters %d/%d, NPCs %d/%d (%s)" % (mon_ok, mon, npc_ok, npc, rule))
        c.details.append("monster centres on blocked tiles: %s" % (" ".join(bad) or "none"))
        if mon == 0 or mon_ok < V6_MIN_MONSTER_SHARE * mon:
            c.fail("monster centres walkable %d/%d < %.0f%%" % (mon_ok, mon, 100 * V6_MIN_MONSTER_SHARE))
    checks.append(c)

    # V7 walkable share
    c = Check("V7", "walkable share vs old Moradon (+-3 pp)", "72.3% vs 72.1%")
    old_path = os.path.join(args.map_dir, OLD_MORADON_SMD)
    if not os.path.isfile(old_path):
        c.skip("%s missing" % old_path)
        c.measured.append("-")
    else:
        odata, olay = load_smd(old_path, "old Moradon", load_warps=False)
        w_new, t_new = walkable_share(n, ev)
        w_old, t_old = walkable_share(olay["n"], smd_events(odata, olay))
        s_new = 100.0 * w_new / t_new
        s_old = 100.0 * w_old / t_old
        c.measured.append("%.1f%% (%d/%d) vs %.1f%% (%d/%d)" % (s_new, w_new, t_new, s_old, w_old, t_old))
        if abs(s_new - s_old) > V7_MAX_WALKABLE_DIFF_PP:
            c.fail("difference %.2f pp" % abs(s_new - s_old))
    checks.append(c)

    # V8 opdext faces
    c = Check("V8", ".opdext faces touch walkable tiles", "0")
    if not os.path.isfile(opdext_path):
        c.skip("%s missing" % opdext_path)
        c.measured.append("-")
    else:
        faces = parse_opdext_faces(read_bytes(opdext_path, "client opdext"), opdext_path, n)
        tiles = faces_tiles(faces, n)
        walk = sorted(t for t in tiles if ev[t[0] * n + t[1]] != 0)
        c.measured.append("%d (faces %d, tiles touched %d)" % (len(walk), len(faces), len(tiles)))
        if walk:
            c.fail("walkable tiles under .opdext faces: %s" % walk[:10])
    checks.append(c)

    # V9 warps
    c = Check("V9", "warps: group in {211,212}, zone in ZONE_INFO, centre + r5 walkable (>= 85%)",
              "all")
    grids = {}
    ok_count = 0
    recs = smd_warp_records(out_data, lay)
    for rec in recs:
        f = warp_fields(rec)
        problems = []
        if f["id"] // 10 not in GATE_WARP_GROUPS:
            problems.append("group %d" % (f["id"] // 10))
        if f["zone"] not in zone_info:
            problems.append("zone %d not in ZONE_INFO" % f["zone"])
            line = "warp %d %s -> zone %d: %s" % (f["id"], f["name"], f["zone"], "; ".join(problems))
            c.fail(line)
            continue
        if f["zone"] == ZONE_MORADON:
            gn, gev, gname = n, ev, os.path.basename(out_path)
        else:
            gname = zone_info[f["zone"]]
            if gname not in grids:
                gpath = os.path.join(args.map_dir, gname)
                gdata, glay = load_smd(gpath, "target map", load_warps=False)
                grids[gname] = (glay["n"], smd_events(gdata, glay))
            gn, gev = grids[gname]
        centre = is_walkable(gn, gev, f["x"], f["z"])
        arr = sample_points({"kind": "arrival", "x": f["x"], "z": f["z"], "r": f["r"]})
        arr_ok = sum(1 for (px, pz) in arr if is_walkable(gn, gev, px, pz))
        disc = sample_points({"kind": "disc", "x": f["x"], "z": f["z"], "r": 5})
        disc_ok = sum(1 for (px, pz) in disc if is_walkable(gn, gev, px, pz))
        if not centre:
            problems.append("centre blocked")
        if disc_ok < V9_MIN_DISC_SHARE * len(disc):
            problems.append("r5 share below %.0f%%" % (100 * V9_MIN_DISC_SHARE))
        line = ("warp %d group %d %-16s zone %2d %-24s (%.1f,%.1f) centre %s, r5 %d/%d, arrival box %d/%d, pay %d"
                % (f["id"], f["id"] // 10, f["name"], f["zone"], gname, f["x"], f["z"],
                   "ok" if centre else "BLOCKED", disc_ok, len(disc), arr_ok, len(arr), f["pay"]))
        c.details.append(line + (" -> " + "; ".join(problems) if problems else ""))
        if problems:
            c.fail("warp %d: %s" % (f["id"], "; ".join(problems)))
        else:
            ok_count += 1
        if arr_ok < V9_WARN_ARRIVAL_SHARE * len(arr):
            c.warn("warp %d: SelectWarpList arrival box (x,z + {-%d..0, %d..%d}) walkable %d/%d < %.0f%%"
                   % (f["id"], int(f["r"]) - 1, int(f["r"]), 2 * int(f["r"]), arr_ok, len(arr),
                      100 * V9_WARN_ARRIVAL_SHARE))
    c.details.append("ZONE_INFO source: %s" % zone_info_src)
    c.measured.append("%d/%d" % (ok_count, len(recs)))
    checks.append(c)

    # V10 inbound patch + zone-71 nav fingerprint
    c = Check("V10", "patch-inbound: only 8 B per sZone==21 record; zone-71 nav crc32 unchanged", "yes")
    pdir = args.patched_dir
    if pdir is None:
        cand = os.path.dirname(os.path.abspath(out_path))
        if all(os.path.isfile(os.path.join(cand, f)) for f in INBOUND_FILES):
            pdir = cand
    if pdir is None:
        c.skip("no --patched-dir and the inbound files are not next to --smd")
        c.measured.append("-")
    else:
        tx = float(args.inbound_x)
        tz = float(args.inbound_z)
        nrec = 0
        nbytes = 0
        for fn in INBOUND_FILES:
            src = os.path.join(args.map_dir, fn)
            dst = os.path.join(pdir, fn)
            a = read_bytes(src, "original map")
            b = read_bytes(dst, "patched map")
            if len(a) != len(b):
                c.fail("%s: size %d != %d" % (fn, len(b), len(a)))
                continue
            alay = parse_smd_layout(a, "'%s'" % src)
            allowed = set()
            in_recs = 0
            wrong = []
            for off in alay["warp_offsets"]:
                fa = warp_fields(a[off:off + WARP_SIZE])
                if fa["zone"] != ZONE_MORADON:
                    continue
                in_recs += 1
                allowed |= allowed_patch_bytes(off)
                fb = warp_fields(b[off:off + WARP_SIZE])
                if fb["x"] != tx or fb["z"] != tz:
                    wrong.append(fa["id"])
            diffs = diff_positions(a, b)
            stray = [i for i in diffs if i not in allowed]
            nrec += in_recs
            nbytes += len(diffs)
            c.details.append("%-26s sZone==21 records %d, differing bytes %d, outside fX/fZ %d, md5 %s"
                             % (fn, in_recs, len(diffs), len(stray), md5_hex(b)))
            if stray or wrong or in_recs == 0 or len(diffs) > 8 * in_recs:
                c.fail("%s: stray %s, not at target %s, records %d" % (fn, stray[:8], wrong, in_recs))
        nav = import_nav_export()
        crc_a = nav_crc32(nav, os.path.join(args.map_dir, NAV_ZONE71_SMD))
        crc_b = nav_crc32(nav, os.path.join(pdir, NAV_ZONE71_SMD))
        c.details.append("zone 71 nav-export crc32: original %08x, patched %08x" % (crc_a, crc_b))
        if crc_a != crc_b:
            c.fail("zone 71 nav crc32 changed")
        c.measured.append("records %d, differing bytes %d; crc32 %08x == %08x" % (nrec, nbytes, crc_a, crc_b))
    checks.append(c)

    print("VERIFY %s bytes=%d md5=%s" % (out_path, len(out_data), md5_hex(out_data)))
    print("%-4s %-5s %-70s | %s" % ("id", "res", "measured", "expected (G section 7)"))
    for ch in checks:
        res = ch.status + ("*" if ch.warnings else "")
        print("%-4s %-5s %-70s | %s" % (ch.cid, res, "; ".join(ch.measured), ch.expected))
    for ch in checks:
        if ch.details:
            print("%s %s" % (ch.cid, ch.title))
            for d in ch.details:
                print("    " + d)
    nfail = sum(1 for ch in checks if ch.status == "FAIL")
    nskip = sum(1 for ch in checks if ch.status == "SKIP")
    npass = len(checks) - nfail - nskip
    nwarn = sum(ch.warnings for ch in checks)
    print("VERIFY %s (%d PASS, %d FAIL, %d SKIP; %d WARN, see * rows)"
          % ("FAILED" if nfail else "OK", npass, nfail, nskip, nwarn))
    return 1 if nfail else 0


# ---------------------------------------------------------------------------
# selftest
# ---------------------------------------------------------------------------

def _client_new(tag, flag, payload):
    return struct.pack("<i", len(tag)) + tag + struct.pack("<i", flag) + payload


def _client_old(version, name, payload):
    return struct.pack("<ii", version, len(name)) + name + payload


def _collision_block(n, polys, nface=1):
    """Synthetic CN3ShapeMgr block; polys maps 4 m tile (x, z) -> polygon count."""
    w = (n - 1) * UNIT
    out = bytearray(struct.pack("<ffi", w, w, nface))
    out += struct.pack("<9f", 0, 0, 0, 4, 0, 0, 0, 0, 4) * nface
    cells = int(math.ceil(w / CELL_MAIN_SIZE))
    for cz in range(cells):
        for cx in range(cells):
            inside = {(sx, sz): polys.get((cx * 4 + sx, cz * 4 + sz), 0)
                      for sz in range(4) for sx in range(4)}
            if not any(inside.values()):
                out += struct.pack("<I", 0)
                continue
            out += struct.pack("<Ii", 1, 1) + struct.pack("<H", 0)
            for sz in range(4):
                for sx in range(4):
                    k = inside[(sx, sz)]
                    out += struct.pack("<i", k) + struct.pack("<3I", 0, 1, 2) * k
    return bytes(out)


def _smd(n, heights, coll, nobj, events, warps, regene=0):
    out = bytearray(struct.pack("<if", n, UNIT))
    out += array.array("f", heights).tobytes()
    out += coll
    out += struct.pack("<i", nobj) + bytes(range(OBJECT_EVENT_SIZE)) * nobj
    out += array.array("h", events).tobytes()
    out += struct.pack("<i", regene) + b"\x01" * (REGENE_EVENT_SIZE * regene)
    out += struct.pack("<i", len(warps)) + b"".join(warps)
    return bytes(out)


def _warp(wid, name, zone, x, z, pay, nation=1):
    return make_warp({"id": wid, "name": name, "announce": "about " + name, "pay": pay, "zone": zone,
                      "x": x, "z": z, "nation": nation}, "selftest warp")


def selftest_synthetic(smd_parse):
    if sys.byteorder != "little":
        raise AssertionError("selftest assumes a little-endian host")
    # 1. client headers: new (int L, L bytes, int flag) and old (int version, int L, name)
    n = 5
    hs = [0.0] * (n * n)
    hs[1 * n + 1] = 10.0
    hs[3 * n + 3] = 9.999
    mapdata = struct.pack("<i", n) + b"".join(struct.pack("<fI", h, 0x101) for h in hs) + b"\x00" * 7
    g_new = parse_gtd(_client_new(b"\x65\x13\xaf\xdb\x0d\x6e\x3b", 0, mapdata), "t.gtd", expected_n=n)
    g_old = parse_gtd(_client_old(2, b"freezone", mapdata), "t.gtd", expected_n=n)
    assert g_new["offset"] == 15 and g_old["offset"] == 16, (g_new["offset"], g_old["offset"])
    assert list(g_new["heights"]) == list(array.array("f", hs)) == list(g_old["heights"])
    assert g_new["heights_raw"] == array.array("f", hs).tobytes()
    try:
        parse_gtd(_client_new(b"abc", 0, mapdata), "t.gtd", expected_n=n + 1)
        raise AssertionError("wrong n accepted")
    except ToolError:
        pass

    # 2. collision block parse: sub-cell order z outer, x inner -> 4 m tile keys
    polys = {(3, 0): 2, (1, 2): 1}
    cb = _collision_block(n, polys)
    opd = _client_new(b"\x65\x13\xaf\xdb\x0d\x6e\x3b", 0, cb + b"SHAPES")
    coll = parse_opd_collision(opd, "t.opd", n)
    assert coll["offset"] == 15 and coll["block"] == cb and coll["subcells"] == polys, coll["subcells"]
    assert coll["main_cells"] == 1 and coll["faces"] == 1

    # 3. rule R: range >= 10.0 m blocks (exactly 10.0 included), 9.999 m does not,
    #    a collision sub-cell blocks, edges row/column n-1 stay 1
    ev = rule_r_events(n, g_new["heights"], coll["subcells"])
    zeros = sorted((i // n, i % n) for i, v in enumerate(ev) if v == 0)
    assert zeros == [(0, 0), (0, 1), (1, 0), (1, 1), (1, 2), (3, 0)], zeros
    assert all(ev[(n - 1) * n + z] == 1 and ev[z * n + n - 1] == 1 for z in range(n))

    # 4. build: fee counterpart by destination, zone drop, layout, repo parser mirror
    donor_warps = [_warp(2111, "Folk Village", 21, 411, 525, 3000),
                   _warp(2113, "Luferson Castle", 1, 437, 1627, 5000),
                   _warp(2114, "Lunar Valley", 1, 1860, 169, 10000),
                   _warp(2117, "Ronark Land Base", 73, 515, 104, 17000)]
    donor = _smd(n, [1.0] * (n * n), _collision_block(n, {}), 2, [1] * (n * n), donor_warps)
    ours = _smd(n, [1.0] * (n * n), _collision_block(n, {}), 0, [1] * (n * n),
                [_warp(2111, "Luferson Castle", 1, 441, 1625, 4000),
                 _warp(2114, "Lunar Valley", 1, 1860, 169, 5000),
                 _warp(2113, "Ronark Land", 71, 1380, 1090, 17000)])
    gtd5 = _client_new(b"moradon", 0, mapdata)
    out, info = build_smd_bytes(gtd5, opd, donor, ours, None, {73}, expected_n=n)
    pays = [(w["id"], w["pay"], w["pay_source"]) for w in info["warps"]]
    assert pays == [(2111, 3000, "alpha (no counterpart in our old Moradon)"),
                    (2113, 4000, "ours:2111"), (2114, 5000, "ours:2114")], pays
    assert [d["id"] for d in info["dropped_warps"]] == [2117]
    lay = parse_smd_layout(out, "selftest out")
    assert lay["trailing"] == 0 and smd_events(out, lay).tolist() == ev.tolist()
    assert out[8:8 + 4 * n * n] == g_new["heights_raw"]
    assert out[lay["collision"]["offset"]:lay["collision"]["end"]] == cb
    dlay = parse_smd_layout(donor, "selftest donor")
    assert out[lay["object_events_offset"]:lay["events_offset"]] == \
        donor[dlay["object_events_offset"]:dlay["events_offset"]]
    for i, off in enumerate(lay["warp_offsets"]):
        src = donor_warps[i]
        diff = [j for j in range(WARP_SIZE) if out[off + j] != src[j]]
        assert all(WARP_OFF_PAY <= j < WARP_OFF_PAY + 4 for j in diff), diff
    out2, _ = build_smd_bytes(gtd5, opd, donor, ours, None, {73}, expected_n=n)
    assert out2 == out
    with tempfile.TemporaryDirectory() as td:
        p = os.path.join(td, "t.smd")
        write_file(p, out)
        r = smd_parse.parse(p, load_warps=True)
    assert r["size_check_ok"] and r["trailing_bytes"] == 0 and r["object_event_count"] == 2
    assert [w["sWarpID"] for w in r["warps"]] == [2111, 2113, 2114] and r["regene"] == []
    spec = [{"id": 2111, "name": "Spec", "zone": 21, "x": 1, "z": 2, "pay": 7, "nation": 1}]
    _, info3 = build_smd_bytes(gtd5, opd, donor, None, spec, {73}, expected_n=n)
    assert info3["warps"][0]["pay"] == 7 and info3["counts"]["warps"] == 1
    try:
        build_smd_bytes(gtd5, opd, donor, None, [spec[0], dict(spec[0])], {73}, expected_n=n)
        raise AssertionError("duplicate warp id accepted")
    except ToolError:
        pass

    # 5. patch-inbound bytes: only fX/fZ of sZone==21 records; idempotent
    warps = [_warp(114, "Moradon", 21, 288, 369, 1000), _warp(115, "Other", 1, 288, 369, 0),
             _warp(116, "Moradon", 21, 817, 600, 0)]
    m = _smd(n, [1.0] * (n * n), _collision_block(n, {}), 1, [1] * (n * n), warps, regene=2)
    pm, rows = patch_inbound_bytes(m, "selftest map", INBOUND_X, INBOUND_Z)
    mlay = parse_smd_layout(m, "selftest map")
    allowed = allowed_patch_bytes(mlay["warp_offsets"][0]) | allowed_patch_bytes(mlay["warp_offsets"][2])
    diffs = diff_positions(m, pm)
    assert diffs and set(diffs) <= allowed and len(pm) == len(m), diffs
    assert [r["id"] for r in rows] == [114, 116]
    assert rows[0]["changed_bytes"] <= 8 and rows[1]["changed_bytes"] <= 4
    assert pm[mlay["warp_offsets"][1]:mlay["warp_offsets"][1] + WARP_SIZE] == warps[1]
    f0 = warp_fields(pm[mlay["warp_offsets"][0]:mlay["warp_offsets"][0] + WARP_SIZE])
    assert (f0["x"], f0["z"], f0["pay"], f0["r"]) == (817.0, 530.0, 1000, 5.0)
    pm2, _ = patch_inbound_bytes(pm, "selftest map", INBOUND_X, INBOUND_Z)
    assert pm2 == pm

    # 6. arrival offsets, x-z overlap, output guard
    assert arrival_offsets(5) == [-4, -3, -2, -1, 0, 5, 6, 7, 8, 9, 10]
    assert len(sample_points({"kind": "disc", "x": 0, "z": 0, "r": 5})) == 81
    assert len(sample_points({"kind": "square", "x": 0, "z": 0, "size": 10})) == 121
    assert len(sample_points({"kind": "rect", "x0": 684, "x1": 735, "z0": 440, "z1": 491})) == 169
    assert faces_tiles([((0.0, 0.0), (8.0, 0.0), (0.0, 8.0))], 9) == {(0, 0), (1, 0), (0, 1)}
    assert faces_tiles([((2.0, 2.0), (6.0, 2.0), (6.0, 2.0))], 9) == {(0, 0), (1, 0)}
    assert is_protected_map_dir("/x/server/Map") and not is_protected_map_dir("/x/out")
    try:
        check_output_dir("/x/server/Map", [])
        raise AssertionError("server Map directory accepted")
    except ToolError:
        pass


def cmd_selftest(args):
    smd_parse = import_smd_parse()
    selftest_synthetic(smd_parse)
    print("selftest synthetic: OK (header, collision, rule R, build, patch bytes, sampling)")
    fails, skips = rule_reference_tests(args.map_dir, args.client_zones, lambda m: print("selftest " + m))
    if fails:
        print("SELFTEST FAILED: %s" % "; ".join(fails))
        return 1
    print("SELFTEST OK%s" % ((" (%d reference tests skipped)" % len(skips)) if skips else ""))
    return 0


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

SPAWNS_HELP = ("SQLCMD '-W -s |' export with NpcID, ActType, LeftX, TopZ, RightX, BottomZ "
               "(optional kind M/N), e.g. on .\\SQL2019 FDP_alpha1534: SELECT NpcID, ActType, "
               "LeftX, TopZ, RightX, BottomZ, NumNPC FROM K_NPCPOS WHERE ZoneID = 21")


def main(argv=None):
    ap = argparse.ArgumentParser(description="Build, patch and verify the new Moradon (zone 21) SMD.")
    ap.add_argument("--selftest", action="store_true", help="run the self tests and exit")
    ap.add_argument("--map-dir", default=DEFAULT_MAP_DIR, help="our server Map directory (read only)")
    ap.add_argument("--client-zones", default=DEFAULT_CLIENT_ZONES, help="1534 client Zones directory")
    sub = ap.add_subparsers(dest="cmd")

    b = sub.add_parser("build", help="build moradon SMD from the client files")
    b.add_argument("--client-zones", default=argparse.SUPPRESS)
    b.add_argument("--donor", default=DEFAULT_DONOR, help="ALPHA moradon_0826.smd (warps, object events)")
    b.add_argument("--warps", help="JSON warp spec replacing the donor list (written as given)")
    b.add_argument("--fees-from", default="",
                   help="SMD whose warp fees are used (default: <map-dir>/%s); 'alpha' keeps donor fees"
                   % OLD_MORADON_SMD)
    b.add_argument("--drop-zones", default=DEFAULT_DROP_ZONES, help="target zones to drop (default 73)")
    b.add_argument("--map-dir", default=argparse.SUPPRESS)
    b.add_argument("--out", required=True, help="output SMD; a sidecar <out>.json is written next to it")

    p = sub.add_parser("patch-inbound", help="move sZone==21 warp targets on other maps")
    p.add_argument("--map-dir", default=argparse.SUPPRESS, help="source maps (read only)")
    p.add_argument("--files", default=",".join(INBOUND_FILES))
    p.add_argument("--x", type=float, default=INBOUND_X)
    p.add_argument("--z", type=float, default=INBOUND_Z)
    p.add_argument("--out-dir", required=True)

    v = sub.add_parser("verify", help="offline checks V1-V10 (exit 1 on any FAIL)")
    v.add_argument("--smd", required=True)
    v.add_argument("--points", help="JSON list of reference points replacing the built-in V5 list")
    v.add_argument("--spawns", help=SPAWNS_HELP)
    v.add_argument("--patched-dir", help="patch-inbound output (default: the --smd directory if it "
                   "holds the inbound files)")
    v.add_argument("--client-zones", default=argparse.SUPPRESS)
    v.add_argument("--donor", default=DEFAULT_DONOR)
    v.add_argument("--warps", help="the warp spec used by build, if any")
    v.add_argument("--drop-zones", default=DEFAULT_DROP_ZONES)
    v.add_argument("--map-dir", default=argparse.SUPPRESS)
    v.add_argument("--zone-info", help="SQLCMD export 'SELECT ZoneNo, strZoneName FROM ZONE_INFO' "
                   "(default: built-in snapshot)")
    v.add_argument("--inbound-x", type=float, default=INBOUND_X)
    v.add_argument("--inbound-z", type=float, default=INBOUND_Z)

    args = ap.parse_args(argv)
    try:
        if args.selftest:
            return cmd_selftest(args)
        if args.cmd == "build":
            return cmd_build(args)
        if args.cmd == "patch-inbound":
            return cmd_patch_inbound(args)
        if args.cmd == "verify":
            return cmd_verify(args)
    except ToolError as e:
        print("ERROR: %s" % e, file=sys.stderr)
        return 2
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
