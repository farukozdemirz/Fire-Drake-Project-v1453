#!/usr/bin/env python3
"""
Read-only SMD parser that mirrors shared/SMDFile.cpp + N3BASE/N3ShapeMgr.* exactly
(Fire-Drake-Project-v1453 @ 0f520272).  Python 3 stdlib only.

Read order (SMDFile::LoadMap, SMDFile.cpp:75-102):
  1. LoadTerrain        int32 m_nMapSize, float32 m_fUnitDist, float32 height[m_nMapSize^2]
  2. CN3ShapeMgr::LoadCollisionData (N3ShapeMgr.cpp:52-114)
        float32 fMapWidth, float32 fMapLength, int32 nCollisionFaceCount,
        __Vector3[nFaceCount*3] (3 x float32 each),
        for fZ in [0, fMapLength) step 16 (z++):            (CELL_MAIN_SIZE = 4*4 = 16 m)
          for fX in [0, fMapWidth) step 16 (x++):
            uint32 bExist; if bExist: __CellMain::Load:
               int32 nShapeCount; uint16 shapeIdx[nShapeCount];
               for z in 0..3: for x in 0..3: __CellSub::Load:
                   int32 nCCPolyCount; uint32 vertIdx[nCCPolyCount*3]
     check: (m_nMapSize-1)*m_fUnitDist == fMapWidth (and == Width() again, Height() returns width)
  3. LoadObjectEvent    int32 count; count * 24 bytes (read into a leaked _OBJECT_EVENT, NOT stored)
  4. LoadMapTile        int16 event[m_nMapSize^2], index = x * m_nMapSize + z (SMDFile::GetEventID)
  5. (GameServer only, bLoadWarpsAndRegeneEvents=true)
     LoadRegeneEvent    int32 count; count * 20 bytes (5 x float32: PosX, PosY, PosZ, AreaZ, AreaX)
     LoadWarpList       int32 count; count * 320 bytes (_WARP_INFO, #pragma pack(1))
"""
import array
import struct
import sys
import os
import zlib
from collections import Counter

MAP_DIR = os.environ.get("FDP_MAP_DIR", "/mnt/c/dev/fdp/server/Map")
OUT_DIR = os.path.dirname(os.path.abspath(__file__))
CELL_MAIN_DEVIDE = 4
CELL_SUB_SIZE = 4
CELL_MAIN_SIZE = CELL_MAIN_DEVIDE * CELL_SUB_SIZE   # 16
MAX_CELL_MAIN = 4096 // CELL_MAIN_SIZE               # 256
VIEW_DISTANCE = 48                                    # shared/globals.h:19


class Reader:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def take(self, n, what):
        if self.p + n > len(self.d):
            raise EOFError("EOF while reading %s at offset %d (need %d bytes, have %d)"
                           % (what, self.p, n, len(self.d) - self.p))
        b = self.d[self.p:self.p + n]
        self.p += n
        return b

    def i32(self, what):
        return struct.unpack("<i", self.take(4, what))[0]

    def u32(self, what):
        return struct.unpack("<I", self.take(4, what))[0]

    def f32(self, what):
        return struct.unpack("<f", self.take(4, what))[0]


def cstr(b):
    b = b.split(b"\0", 1)[0]
    try:
        return b.decode("cp949")
    except Exception:
        return b.decode("latin-1")


def parse(path, load_warps=True):
    data = open(path, "rb").read()
    r = Reader(data)
    res = {"file": os.path.basename(path), "size_bytes": len(data)}

    # 1. LoadTerrain
    n = r.i32("m_nMapSize")
    unit = r.f32("m_fUnitDist")
    res["m_nMapSize"] = n
    res["m_fUnitDist"] = unit
    h = array.array("f")
    h.frombytes(r.take(4 * n * n, "height map"))
    if sys.byteorder != "little":
        h.byteswap()
    res["height"] = h

    # 2. Collision data
    off_coll = r.p
    w = r.f32("fMapWidth")
    l = r.f32("fMapLength")
    res["coll_width"] = w
    res["coll_length"] = l
    nface = r.i32("nCollisionFaceCount")
    res["coll_faces"] = nface
    verts = array.array("f")
    if nface > 0:
        verts.frombytes(r.take(12 * nface * 3, "collision vertices"))
    res["coll_verts"] = verts
    cells_exist = 0
    total_shape_idx = 0
    total_cc_polys = 0
    subcells_with_polys = 0
    # density per 4 m sub-cell, grid of (w/4) x (l/4) ; key (subx, subz)
    nsub_x = int(w // CELL_SUB_SIZE) + 1
    sub_density = {}
    shape_ids = set()
    z = 0
    fz = 0.0
    while fz < l:
        x = 0
        fx = 0.0
        while fx < w:
            bexist = r.u32("cell bExist")
            if bexist:
                cells_exist += 1
                nshape = r.i32("nShapeCount")
                if nshape:
                    idx = struct.unpack("<%dH" % nshape, r.take(2 * nshape, "shape indices"))
                    shape_ids.update(idx)
                total_shape_idx += nshape
                for sz in range(CELL_MAIN_DEVIDE):
                    for sx in range(CELL_MAIN_DEVIDE):
                        # N3ShapeMgr.h:73-77 loads SubCells[x][z] with z outer, x inner
                        npoly = r.i32("nCCPolyCount")
                        if npoly:
                            r.take(4 * npoly * 3, "CC vertex indices")
                            total_cc_polys += npoly
                            subcells_with_polys += 1
                            gx = x * CELL_MAIN_DEVIDE + sx
                            gz = z * CELL_MAIN_DEVIDE + sz
                            sub_density[(gx, gz)] = npoly
            fx += CELL_MAIN_SIZE
            x += 1
        fz += CELL_MAIN_SIZE
        z += 1
    res["coll_main_cells_grid"] = (x, z)
    res["coll_main_cells_exist"] = cells_exist
    res["coll_shape_index_refs"] = total_shape_idx
    res["coll_unique_shape_ids"] = len(shape_ids)
    res["coll_cc_poly_refs"] = total_cc_polys
    res["coll_subcells_with_polys"] = subcells_with_polys
    res["coll_sub_density"] = sub_density
    res["coll_bytes"] = r.p - off_coll
    expected = (n - 1) * unit
    res["size_check_ok"] = (expected == w) and (expected == w)  # Height() returns m_fMapWidth (N3ShapeMgr.h:108)
    res["length_equals_width"] = (w == l)

    # 3. LoadObjectEvent (24 bytes each, discarded by server)
    nobj = r.i32("iEventObjectCount")
    res["object_event_count"] = nobj
    objs = []
    for i in range(nobj):
        raw = r.take(24, "object event")
        # INFERRED layout (original 1.298 Ebenezer reader, not in this repo):
        # int32 sBelong, int16 sIndex, int16 sType, int16 sControlNpcID, int16 sStatus, float x, y, z
        objs.append(struct.unpack("<ihhhhfff", raw))
    res["object_events"] = objs

    # 4. LoadMapTile
    ev = array.array("h")
    ev.frombytes(r.take(2 * n * n, "event grid"))
    if sys.byteorder != "little":
        ev.byteswap()
    res["events"] = ev
    res["offset_after_tiles"] = r.p

    # 5. Regene + warps (GameServer only)
    res["regene"] = []
    res["warps"] = []
    res["warp_note"] = ""
    if load_warps:
        nreg = r.i32("regene count")
        res["regene_count"] = nreg
        for i in range(nreg):
            px, py, pz, az, ax = struct.unpack("<5f", r.take(20, "regene"))
            res["regene"].append(dict(sRegenePoint=i, PosX=px, PosY=py, PosZ=pz, AreaZ=az, AreaX=ax))
        nwarp = r.i32("warp count")
        res["warp_count"] = nwarp
        for i in range(nwarp):
            if r.p + 320 > len(data):
                res["warp_note"] = "EOF inside warp %d (server returns silently: SMDFile.cpp:170-171)" % i
                break
            raw = r.take(320, "warp")
            (sWarpID,) = struct.unpack_from("<h", raw, 0)
            name = cstr(raw[2:34])
            ann = cstr(raw[34:290])
            sUnk0, dwPay, sZone, sUnk1, fX, fY, fZ, fR, sNation, sUnk2 = struct.unpack_from("<HIhHffffhH", raw, 290)
            res["warps"].append(dict(sWarpID=sWarpID, name=name, announce=ann, dwPay=dwPay, sZone=sZone,
                                     fX=fX, fY=fY, fZ=fZ, fR=fR, sNation=sNation,
                                     stored=(sWarpID != 0)))
    res["trailing_bytes"] = len(data) - r.p
    return res


# ---------------- PNG writer (pure python, zlib) ----------------
def write_png(path, width, height, rows_rgb):
    def chunk(tag, payload):
        c = struct.pack(">I", len(payload)) + tag + payload
        return c + struct.pack(">I", zlib.crc32(tag + payload) & 0xffffffff)
    raw = bytearray()
    for row in rows_rgb:
        raw.append(0)
        raw.extend(row)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


def event_color(v):
    if v == 0:
        return (20, 20, 20)        # 0  -> dark (AI IsMovable()==true ; see report)
    if v == 1:
        return (235, 235, 235)     # 1  -> light
    if v < 0:
        return (255, 0, 255)
    # event ids >= 2: hashed bright colors
    hsh = (v * 2654435761) & 0xffffffff
    return (64 + (hsh & 0x7f), 64 + ((hsh >> 8) & 0x7f), 64 + ((hsh >> 16) & 0x7f))


def render(res, tag):
    n = res["m_nMapSize"]
    ev = res["events"]
    h = res["height"]
    # image row = z (north up => flip so z grows upward), column = x
    # events: index x*n + z  (SMDFile::GetEventID)
    rows = []
    for zi in range(n - 1, -1, -1):
        row = bytearray()
        for xi in range(n):
            row.extend(event_color(ev[xi * n + zi]))
        rows.append(row)
    p1 = os.path.join(OUT_DIR, "%s_eventgrid.png" % tag)
    write_png(p1, n, n, rows)

    hmin, hmax = min(h), max(h)
    span = (hmax - hmin) or 1.0
    rows = []
    for zi in range(n - 1, -1, -1):
        row = bytearray()
        for xi in range(n):
            g = int(255 * (h[xi * n + zi] - hmin) / span)   # ASSUMED same x*n+z layout as events
            row.extend((g, g, g))
        rows.append(row)
    p2 = os.path.join(OUT_DIR, "%s_height.png" % tag)
    write_png(p2, n, n, rows)

    # collision density on the 4 m sub-cell grid (w/4 x l/4)
    w = int(res["coll_width"])
    gs = w // CELL_SUB_SIZE
    dens = res["coll_sub_density"]
    mx = max(dens.values()) if dens else 1
    rows = []
    for gz in range(gs - 1, -1, -1):
        row = bytearray()
        for gx in range(gs):
            d = dens.get((gx, gz), 0)
            if d == 0:
                ev_v = ev[min(gx, n - 1) * n + min(gz, n - 1)]
                base = 235 if ev_v == 1 else (20 if ev_v == 0 else 120)
                row.extend((base, base, base))
            else:
                t = min(1.0, d / max(1.0, mx * 0.25))
                row.extend((255, int(200 * (1 - t)), 0))
        rows.append(row)
    p3 = os.path.join(OUT_DIR, "%s_collision.png" % tag)
    write_png(p3, gs, gs, rows)
    return p1, p2, p3


def summarize(res, tag, out):
    n = res["m_nMapSize"]
    unit = res["m_fUnitDist"]
    h = res["height"]
    ev = res["events"]
    P = lambda *a: print(*a, file=out)
    P("=" * 78)
    P("FILE %s  (%d bytes)  [%s]" % (res["file"], res["size_bytes"], tag))
    P("m_nMapSize            = %d vertices per side  -> GetMapSize() = %d tiles" % (n, n - 1))
    P("m_fUnitDist           = %r m per tile" % unit)
    P("world extent          = (m_nMapSize-1)*unit = %r m  (x and z: 0..%r)" % ((n - 1) * unit, (n - 1) * unit))
    P("collision width/length= %r / %r  size check ok=%s  width==length=%s"
      % (res["coll_width"], res["coll_length"], res["size_check_ok"], res["length_equals_width"]))
    xr = int(res["coll_width"] / VIEW_DISTANCE) + 1
    P("regions (VIEW_DISTANCE=48): m_nXRegion = m_nZRegion = %d  (GetXRegionMax = %d)" % (xr, xr - 1))
    hmin, hmax = min(h), max(h)
    P("height min/max/mean   = %.3f / %.3f / %.3f m" % (hmin, hmax, sum(h) / len(h)))
    c = Counter(ev)
    tot = len(ev)
    P("event grid            = %d x %d = %d cells (int16, index x*%d+z)" % (n, n, tot, n))
    P("event histogram (value: count, pct):")
    for v, k in sorted(c.items()):
        P("   %6d : %8d  (%.2f%%)" % (v, k, 100.0 * k / tot))
    # interior tiles only (0..n-2) i.e. the playable tile area
    inner = Counter()
    for xi in range(n - 1):
        base = xi * n
        for zi in range(n - 1):
            inner[ev[base + zi]] += 1
    P("event histogram restricted to x,z in [0, %d) (tiles actually inside world extent):" % (n - 1))
    for v, k in sorted(inner.items()):
        P("   %6d : %8d  (%.2f%%)" % (v, k, 100.0 * k / ((n - 1) ** 2)))
    P("collision: faces=%d (verts=%d), main 16m cells grid=%s, cells present=%d, shape index refs=%d, "
      "unique shape ids=%d, CC poly refs in 4m sub-cells=%d, sub-cells with polys=%d, bytes=%d"
      % (res["coll_faces"], len(res["coll_verts"]) // 3, res["coll_main_cells_grid"], res["coll_main_cells_exist"],
         res["coll_shape_index_refs"], res["coll_unique_shape_ids"], res["coll_cc_poly_refs"],
         res["coll_subcells_with_polys"], res["coll_bytes"]))
    if res["coll_verts"]:
        vs = res["coll_verts"]
        xs, ys, zs = vs[0::3], vs[1::3], vs[2::3]
        P("collision vertex bbox: x %.1f..%.1f  y %.1f..%.1f  z %.1f..%.1f"
          % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))
    P("object events in SMD   = %d (server reads 24 B each and discards; layout below is INFERRED)" % res["object_event_count"])
    for o in res["object_events"]:
        P("   belong=%d index=%d type=%d ctrlNpc=%d status=%d pos=(%.1f, %.1f, %.1f)" % o)
    P("regene events          = %d" % len(res["regene"]))
    for g in res["regene"]:
        P("   [%d] pos=(%.2f, %.2f, %.2f) areaX=%.2f areaZ=%.2f"
          % (g["sRegenePoint"], g["PosX"], g["PosY"], g["PosZ"], g["AreaX"], g["AreaZ"]))
    P("warp list              = %d entries %s" % (len(res["warps"]), res["warp_note"]))
    for wp in res["warps"]:
        P("   id=%d group=%d name=%r zone=%d pos=(%.2f, %.2f, %.2f) r=%.2f nation=%d pay=%d stored=%s announce=%r"
          % (wp["sWarpID"], wp["sWarpID"] // 10, wp["name"], wp["sZone"], wp["fX"], wp["fY"], wp["fZ"], wp["fR"],
             wp["sNation"], wp["dwPay"], wp["stored"], wp["announce"][:60]))
    P("trailing bytes after parse = %d" % res["trailing_bytes"])


def main():
    targets = [("zone71_freezone_a", "freezone_a_20050718.smd"),
               ("zone72_freezone_b", "freezone_b_20050718.smd")]
    if len(sys.argv) > 1:
        targets = [(os.path.splitext(a)[0], a) for a in sys.argv[1:]]
    out = sys.stdout
    for tag, fn in targets:
        path = os.path.join(MAP_DIR, fn)
        try:
            res = parse(path, load_warps=True)
        except Exception as e:
            print("PARSE FAILED for %s: %s" % (fn, e))
            continue
        summarize(res, tag, out)
        pngs = render(res, tag)
        print("PNGs:", *pngs)


if __name__ == "__main__":
    main()
