#!/usr/bin/env python3
"""Generates the U3 new-Moradon (zone 21) database scripts from the ALPHA database.

    db/015_u3_moradon_1534.sql    (+ _rollback.sql)   ZONE_INFO, START_POSITION,
                                                     K_OBJECTPOS and K_NPCPOS, zone 21

Sources (all read only, SELECT only, through SQLCMD.EXE with Windows authentication):
  - ALPHA DB  .\\SQL2019   / FDP_alpha1534: the zone-21 rows of the four tables.
  - OUR DB    .\\SQLEXPRESS / FDP_kn_online: the column lists of our four tables, the
    ids of our K_NPC and K_MONSTER (every spawn id must exist there; the U2 script
    db/014 adds the new ones), and our zone-21 rows for the comparison. When the
    dbo.<table>_Z21_U3_BACKUP tables exist (the script was applied there), the
    comparison reads them instead of the live rows, so the output stays the same.
  - shared/packets.h (enum ObjectType) and AIServer/ServerDlg.cpp (spawn loader):
    the object types our server knows and the ActType rule (< 100 = K_MONSTER).

Scope (plan U3-02, ADR-0068 addendum 2, docs/reports/u0-1534/G-yeni-moradon-smd.md
5 and 7, E-veri-farki.md 4.3): see SCOPE_NOTE below; it is written into the script.

Usage:
    python3 tools/u3-gen-moradon-db.py            # write db/015_u3_moradon_1534*.sql
    python3 tools/u3-gen-moradon-db.py --check    # 0 = db/ up to date

The output is deterministic: rows sorted by all their values, ASCII, LF.
Standard library only; works under python3 -I. The helpers (Db, wrap, comment_block)
come from tools/u2-gen-alpha.py, which is loaded by path.
"""

import argparse
import hashlib
import importlib.util
import os
import re
import struct
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(TOOLS_DIR)


def load_tool(name, filename):
    # Load a sibling tool by path: python3 -I does not put the script directory on sys.path.
    spec = importlib.util.spec_from_file_location(name, os.path.join(TOOLS_DIR, filename))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


u2 = load_tool("u2_gen_alpha", "u2-gen-alpha.py")
GenError = u2.GenError

APPLY = "015_u3_moradon_1534.sql"
ROLLBACK = "015_u3_moradon_1534_rollback.sql"

ZONE = 21
MAP_SIZE = 1024  # new Moradon: n = 257 tiles of 4 m, (n - 1) * 4 (G report 1 and 7)

# The four tables, their zone column and their backup table (plan U3-02 3.2).
TABLES = (
    ("ZONE_INFO", "ZoneNo"),
    ("START_POSITION", "ZoneID"),
    ("K_OBJECTPOS", "ZoneID"),
    ("K_NPCPOS", "ZoneID"),
)
BACKUP_SUFFIX = "_Z21_U3_BACKUP"

# The script must not run in the database of the 1453 client (ADR-0068 addendum 2,
# item 4: during the transition the zone-21 changes go to a separate 1534 copy).
REFUSED_DATABASE = "FDP_kn_online"
TARGET_DATABASE_EXAMPLE = "FDP_kn1534"

# ZONE_INFO 21: the map file is the one that plan U3-01 generates; the rest is ALPHA's
# row (the generator checks it). ServerNo, Type and bz stay ours.
NEW_SMD = "moradon_1534.smd"
ZONE_INFO_SET = (("strZoneName", NEW_SMD), ("InitX", 81590), ("InitZ", 53079), ("InitY", 469), ("RoomEvent", 0))
ZONE_INFO_FROM_ALPHA = ("InitX", "InitZ", "InitY", "RoomEvent")

# START_POSITION 21: ALPHA's row (the generator checks every column): both nations at
# (817,530), range 10 x 10, gate columns 0 (G report 7).
START_POSITION_SET = (("sKarusX", 817), ("sKarusZ", 530), ("sElmoradX", 817), ("sElmoradZ", 530),
                      ("sKarusGateX", 0), ("sKarusGateZ", 0), ("sElmoGateX", 0), ("sElmoGateZ", 0),
                      ("bRangeX", 10), ("bRangeZ", 10))

# K_OBJECTPOS: ALPHA's soccer-field objects (sea in the 1534 client) are never copied
# (ADR-0068 addendum 2, item 3; G report 5.3). Rows whose Type is not in our
# enum ObjectType are not copied either (no server code handles them).
SOCCER_OBJECT_IDS = (1019, 1020, 1021, 1022)
EXPECTED_OBJECT_IDS = (4013, 4014, 5001)  # El Morad gate, Karus gate, anvil (G report 5.3)
PACKETS_HEADER = os.path.join("shared", "packets.h")

# AIServer spawn loader: ActType < 100 -> K_MONSTER, else K_NPC; Limit* are read only
# when DungeonFamily > 0. The generator checks that the loader still says so.
SPAWN_LOADER = os.path.join("AIServer", "ServerDlg.cpp")
SPAWN_LOADER_LINES = ("bool bMonster = (bActType < 100);", "if (pNpc->m_byDungeonFamily > 0)",
                      "pNpc->m_nLimitMinX = iLimitMinX;")
MONSTER_ACT_TYPE_LIMIT = 100

STAGED_OBJECTS = "#u3_objpos"
STAGED_SPAWNS = "#u3_npcpos"

INT_RANGES = u2.INT_RANGES


# ---------------------------------------------------------------------------
# Database access (SELECT only)
# ---------------------------------------------------------------------------

def column_types(db, table):
    """{name: (data_type, max_length, nullable)} and the ordered column list of a table."""
    columns = db.columns(table)
    for name, dtype, _, _, _ in columns:
        if dtype not in INT_RANGES and dtype not in ("char", "varchar", "text", "float"):
            raise GenError("%s.%s on %s has unsupported type %s" % (table, name, db.label(), dtype))
    return columns


def select_expr(name, dtype, length):
    """Every value travels as text that loses no bit: strings and floats as hex."""
    if dtype in ("char", "varchar"):
        return "CONVERT(varchar(%d), CAST(%s AS varbinary(%d)), 2)" % (2 * length, name, length)
    if dtype == "text":
        return "CONVERT(varchar(8000), CAST(CAST(%s AS varchar(4000)) AS varbinary(4000)), 2)" % name
    if dtype == "float":
        return "CONVERT(varchar(16), CAST(%s AS binary(8)), 2)" % name
    return name


def fetch_rows(db, table, names, where):
    """Rows of table (only the named columns) as dicts; text lengths are cross-checked."""
    types = dict((c[0], c) for c in column_types(db, table))
    missing = [n for n in names if n not in types]
    if missing:
        raise GenError("%s on %s has no column(s) %s" % (table, db.label(), missing))
    select = [select_expr(n, types[n][1], types[n][2]) for n in names]
    texts = [n for n in names if types[n][1] == "text"]
    select += ["ISNULL(DATALENGTH(%s), -1)" % n for n in texts]
    rows = []
    for raw in db.query("SELECT %s FROM dbo.%s WHERE %s" % (", ".join(select), table, where)):
        if len(raw) != len(select):
            raise GenError("%s.%s: got %d fields, expected %d" % (db.label(), table, len(raw), len(select)))
        row = {}
        for name, text in zip(names, raw):
            dtype = types[name][1]
            if text == "NULL":
                row[name] = None
            elif dtype in ("char", "varchar", "text"):
                row[name] = bytes.fromhex(text)
            elif dtype == "float":
                row[name] = struct.unpack(">d", bytes.fromhex(text))[0]
            else:
                row[name] = int(text)
        for name, text in zip(texts, raw[len(names):]):
            expected = int(text)
            actual = -1 if row[name] is None else len(row[name])
            if expected != actual:
                raise GenError("%s.%s.%s: read %d bytes, DATALENGTH is %d" % (db.label(), table, name, actual, expected))
        rows.append(row)
    return rows


def ids_of(db, table):
    return set(int(r[0]) for r in db.query("SELECT sSid FROM dbo.%s" % table))


def table_exists(db, table):
    return db.query("SELECT CASE WHEN OBJECT_ID(N'dbo.%s', N'U') IS NULL THEN 0 ELSE 1 END" % table)[0][0] == "1"


# ---------------------------------------------------------------------------
# Values and SQL text
# ---------------------------------------------------------------------------

def decoded(raw):
    """bytes -> str for char/varchar/text values (printable ASCII, trailing blanks kept)."""
    if raw is None:
        return None
    if any(b < 0x20 or b >= 0x7F for b in raw):
        raise GenError("value %r is not printable ASCII" % raw)
    return raw.decode("ascii")


def float_literal(value):
    if value != value or value in (float("inf"), float("-inf")):
        raise GenError("float value %r cannot be written" % value)
    text = repr(value)  # the shortest text that reads back as the same double
    if "e" in text:
        mantissa, exponent = text.split("e")
        return "%sE%d" % (mantissa, int(exponent))
    return text + "E0"  # an E literal is a float in T-SQL (no decimal rounding)


def sql_value(value, column, where):
    name, dtype, length, nullable, _ = column
    if value is None:
        if not nullable:
            raise GenError("%s: %s is NULL but our column is NOT NULL" % (where, name))
        return "NULL"
    if dtype in INT_RANGES:
        low, high = INT_RANGES[dtype]
        if not isinstance(value, int) or not low <= value <= high:
            raise GenError("%s: %s=%r does not fit %s" % (where, name, value, dtype))
        return str(value)
    if dtype == "float":
        return float_literal(value)
    if dtype in ("char", "varchar", "text"):
        text = decoded(value) if isinstance(value, bytes) else value
        if not u2.is_plain_ascii(text):
            raise GenError("%s: %s=%r is not printable ASCII" % (where, name, text))
        if dtype != "text" and len(text) > length:
            raise GenError("%s: %s=%r is longer than %s(%d)" % (where, name, text, dtype, length))
        if "$(" in text:
            raise GenError("%s: %s=%r contains a sqlcmd variable marker" % (where, name, text))
        return "'" + text.replace("'", "''") + "'"
    raise GenError("%s: column %s has unsupported type %s" % (where, name, dtype))


def staging_ddl(table, columns):
    lines = []
    for name, dtype, length, nullable, _ in columns:
        if dtype in ("char", "varchar"):
            text = "%s %s(%d) COLLATE DATABASE_DEFAULT" % (name, dtype, length)
        elif dtype == "text":
            text = "%s varchar(max) COLLATE DATABASE_DEFAULT" % name
        else:
            text = "%s %s" % (name, dtype)
        lines.append("    " + text + (" NULL" if nullable else " NOT NULL"))
    return "CREATE TABLE %s\n(\n%s\n);" % (table, ",\n".join(lines))


def compare_expr(column):
    """Column expression usable in GROUP BY / EXCEPT (text is not comparable)."""
    name, dtype = column[0], column[1]
    return "CAST(%s AS varchar(max)) AS %s" % (name, name) if dtype == "text" else name


def group_expr(column):
    name, dtype = column[0], column[1]
    return "CAST(%s AS varchar(max))" % name if dtype == "text" else name


def multiset_differ_sql(columns, left, left_where, right, right_where, indent):
    """SQL that counts the (row, count) groups of left that right lacks, and back."""
    sel = u2.wrap([compare_expr(c) for c in columns] + ["COUNT(*) AS n"], indent + "        ")
    grp = u2.wrap([group_expr(c) for c in columns], indent + "        ")

    def side(table, where):
        return "%s    SELECT\n%s\n%s    FROM %s%s\n%s    GROUP BY\n%s" % (
            indent, sel, indent, table, where, indent, grp)

    return "(SELECT COUNT(*) FROM (\n%s\n%s    EXCEPT\n%s) AS d)\n%s+ (SELECT COUNT(*) FROM (\n%s\n%s    EXCEPT\n%s) AS d)" % (
        side(left, left_where), indent, side(right, right_where), indent,
        side(right, right_where), indent, side(left, left_where))


def backups_exist_sql():
    """Number of the four backup tables that exist (an int expression)."""
    return "\n    + ".join("CASE WHEN OBJECT_ID(N'dbo.%s%s', N'U') IS NULL THEN 0 ELSE 1 END" % (t, BACKUP_SUFFIX)
                          for t, _ in TABLES)


def match_sql(pairs, columns, table):
    """'a = 1\n    AND b = 2' for the verification of the constant columns."""
    cols = dict((c[0], c) for c in columns)
    return "\n    AND ".join("%s = %s" % (k, sql_value(v, cols[k], table)) for k, v in pairs)


def row_sort_key(row, names):
    return tuple((0, 0) if row[n] is None else (1, row[n]) for n in names)


# ---------------------------------------------------------------------------
# Code checks
# ---------------------------------------------------------------------------

def read_source(relpath):
    with open(os.path.join(REPO_DIR, relpath), "rb") as f:
        return f.read().decode("latin-1")


def object_types():
    """{value: name} of enum ObjectType in shared/packets.h."""
    text = read_source(PACKETS_HEADER)
    match = re.search(r"enum\s+ObjectType\s*\{([^}]*)\}", text)
    if not match:
        raise GenError("%s: enum ObjectType not found" % PACKETS_HEADER)
    types = {}
    for name, value in re.findall(r"(\w+)\s*=\s*(\d+)", match.group(1)):
        types[int(value)] = name
    if not types:
        raise GenError("%s: enum ObjectType has no values" % PACKETS_HEADER)
    return types


def check_spawn_loader():
    text = read_source(SPAWN_LOADER)
    for line in SPAWN_LOADER_LINES:
        if line not in text:
            raise GenError("%s no longer contains %r; review the ActType/Limit* notes" % (SPAWN_LOADER, line))


# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

def zone_where(zone_col):
    return "%s = %d" % (zone_col, ZONE)


def in_map(value):
    return 0 <= value < MAP_SIZE


def build(our_db, alpha_db):
    check_spawn_loader()
    known_types = object_types()
    columns = {}
    for table, zone_col in TABLES:
        ours = column_types(our_db, table)
        alpha_names = set(c[0] for c in column_types(alpha_db, table))
        lacking = [c[0] for c in ours if c[0] not in alpha_names]
        if lacking:
            raise GenError("ALPHA %s lacks our column(s) %s" % (table, lacking))
        columns[table] = ours
    names = dict((t, [c[0] for c in columns[t]]) for t in columns)

    # ALPHA rows.
    alpha = {}
    digest = hashlib.sha256()
    for table, zone_col in TABLES:
        rows = fetch_rows(alpha_db, table, names[table], zone_where(zone_col))
        rows.sort(key=lambda r: row_sort_key(r, names[table]))
        alpha[table] = rows
        for row in rows:
            digest.update(repr([row[n] for n in names[table]]).encode("ascii"))

    # ZONE_INFO and START_POSITION constants against ALPHA's single row.
    for table, expected, checked in (("ZONE_INFO", ZONE_INFO_SET, ZONE_INFO_FROM_ALPHA),
                                     ("START_POSITION", START_POSITION_SET, [k for k, _ in START_POSITION_SET])):
        if len(alpha[table]) != 1:
            raise GenError("ALPHA %s has %d zone-21 rows, expected 1" % (table, len(alpha[table])))
        row = alpha[table][0]
        for key, value in expected:
            if key in checked and row[key] != value:
                raise GenError("ALPHA %s.%s is %r; the constant is %r" % (table, key, row[key], value))
    for key, value in START_POSITION_SET:
        if key in ("sKarusX", "sElmoradX"):
            span = (value, value + dict(START_POSITION_SET)["bRangeX"])
        elif key in ("sKarusZ", "sElmoradZ"):
            span = (value, value + dict(START_POSITION_SET)["bRangeZ"])
        else:
            continue
        if not (in_map(span[0]) and in_map(span[1])):
            raise GenError("START_POSITION %s range %s is outside 0..%d" % (key, span, MAP_SIZE - 1))
    init = dict(ZONE_INFO_SET)
    if not (in_map(init["InitX"] / 100.0) and in_map(init["InitZ"] / 100.0)):
        raise GenError("ZONE_INFO Init (%d, %d) / 100 is outside the map" % (init["InitX"], init["InitZ"]))

    # K_OBJECTPOS selection.
    objects = []
    soccer = []
    unknown_type = []
    for row in alpha["K_OBJECTPOS"]:
        if row["sIndex"] in SOCCER_OBJECT_IDS:
            soccer.append(row)
        elif row["Type"] not in known_types:
            unknown_type.append(row)
        else:
            objects.append(row)
    objects.sort(key=lambda r: row_sort_key(r, ["sIndex"] + names["K_OBJECTPOS"]))
    selected_ids = tuple(r["sIndex"] for r in objects)
    if selected_ids != EXPECTED_OBJECT_IDS:
        raise GenError("selected K_OBJECTPOS ids %s, expected %s" % (selected_ids, EXPECTED_OBJECT_IDS))
    if sorted(r["sIndex"] for r in soccer) != list(SOCCER_OBJECT_IDS):
        raise GenError("ALPHA soccer objects are %s, expected %s" % (sorted(r["sIndex"] for r in soccer), SOCCER_OBJECT_IDS))
    for row in objects:
        if not (in_map(row["PosX"]) and in_map(row["PosZ"])):
            raise GenError("K_OBJECTPOS %d at (%r, %r) is outside the map" % (row["sIndex"], row["PosX"], row["PosZ"]))

    # K_NPCPOS: ids, loader assumptions and bounds.
    our_npc = ids_of(our_db, "K_NPC")
    our_mon = ids_of(our_db, "K_MONSTER")
    u2_npc = our_db.logged_ids("K_NPC", "sSid")       # ids that db/014 added (for the notes)
    u2_mon = our_db.logged_ids("K_MONSTER", "sSid")
    spawns = alpha["K_NPCPOS"]
    missing = []
    swapped = 0
    dot_notes = []
    for row in spawns:
        monster = row["ActType"] < MONSTER_ACT_TYPE_LIMIT
        if row["NpcID"] not in (our_mon if monster else our_npc):
            missing.append((row["NpcID"], "K_MONSTER" if monster else "K_NPC"))
        if row["DungeonFamily"] != 0:
            raise GenError("K_NPCPOS NpcID %d has DungeonFamily %d; Limit* would be read" % (row["NpcID"], row["DungeonFamily"]))
        for key in ("LeftX", "RightX", "TopZ", "BottomZ"):
            if not in_map(row[key]):
                raise GenError("K_NPCPOS NpcID %d: %s=%d is outside 0..%d" % (row["NpcID"], key, row[key], MAP_SIZE - 1))
        if row["LimitMinX"] == row["TopZ"] and row["LimitMinZ"] == row["LeftX"] and row["LeftX"] != row["TopZ"]:
            swapped += 1
        if row["DotCnt"] > 0:
            path = decoded(row["path"]) or ""
            if re.match(r"^\d{%d}$" % (8 * row["DotCnt"]), path):
                points = [(int(path[i:i + 4]), int(path[i + 4:i + 8])) for i in range(0, len(path), 8)]
                if not all(in_map(x) and in_map(z) for x, z in points):
                    raise GenError("K_NPCPOS NpcID %d: path point outside the map: %s" % (row["NpcID"], points))
            else:
                dot_notes.append((row["NpcID"], row["ActType"], row["DotCnt"], path))
    if missing:
        raise GenError("spawn ids missing in our tables (apply db/014 first?): %s" % sorted(set(missing)))
    for npc_id, act, dots, path in dot_notes:
        # The loader's move type: ActType for monsters, ActType - 100 for NPCs; types 2
        # and 3 walk the path. A path that is not 8 digits per dot is only acceptable
        # for an NPC or monster that does not walk one.
        move_type = act if act < MONSTER_ACT_TYPE_LIMIT else act - MONSTER_ACT_TYPE_LIMIT
        if move_type in (2, 3):
            raise GenError("K_NPCPOS NpcID %d walks a path (ActType %d) but path is %r for DotCnt %d" % (npc_id, act, path, dots))

    # Our rows (the backup tables when they exist on our side).
    ours = {}
    from_backup = all(table_exists(our_db, t + BACKUP_SUFFIX) for t, _ in TABLES)
    for table, zone_col in TABLES:
        source = table + BACKUP_SUFFIX if from_backup else table
        rows = fetch_rows(our_db, source, names[table], zone_where(zone_col))
        rows.sort(key=lambda r: row_sort_key(r, names[table]))
        ours[table] = rows

    stats = {
        "alpha_counts": dict((t, len(alpha[t])) for t, _ in TABLES),
        "alpha_digest": digest.hexdigest(),
        "objects": objects, "soccer": soccer, "unknown_type": unknown_type,
        "known_types": known_types,
        "spawns": spawns, "swapped": swapped, "dot_notes": dot_notes,
        "our_npc": len(our_npc), "our_mon": len(our_mon), "u2_npc": u2_npc, "u2_mon": u2_mon,
        "ours": ours, "from_backup": from_backup,
    }
    return columns, alpha, stats


def spawn_summary(rows):
    npc_rows = [r for r in rows if r["ActType"] >= MONSTER_ACT_TYPE_LIMIT]
    mon_rows = [r for r in rows if r["ActType"] < MONSTER_ACT_TYPE_LIMIT]
    return {
        "rows": len(rows), "ids": len(set(r["NpcID"] for r in rows)),
        "npc_rows": len(npc_rows), "npc_ids": sorted(set(r["NpcID"] for r in npc_rows)),
        "mon_rows": len(mon_rows), "mon_ids": sorted(set(r["NpcID"] for r in mon_rows)),
        "num": sum(r["NumNPC"] for r in rows),
    }


def id_list(ids):
    return ", ".join(str(i) for i in sorted(ids)) or "-"


def comparison(stats):
    """Plan U3-02 step 2: our zone-21 spawns against ALPHA's (counts and id groups)."""
    a = spawn_summary(stats["spawns"])
    o = spawn_summary(stats["ours"]["K_NPCPOS"])
    lines = []
    for label, s in (("ALPHA", a), ("ours", o)):
        lines.append("%s: %d rows, %d ids, NumNPC sum %d; NPC (ActType >= 100) %d rows / %d ids; monsters %d rows / %d ids"
                     % (label, s["rows"], s["ids"], s["num"], s["npc_rows"], len(s["npc_ids"]), s["mon_rows"], len(s["mon_ids"])))
    for kind, a_ids, o_ids, u2_ids in (("NPC", a["npc_ids"], o["npc_ids"], stats["u2_npc"]),
                                       ("monster", a["mon_ids"], o["mon_ids"], stats["u2_mon"])):
        common = set(a_ids) & set(o_ids)
        alpha_only = set(a_ids) - set(o_ids)
        lines.append("%s ids: common %d; ALPHA only %d (added by db/014: %s; already ours: %s); ours only %d (%s)"
                     % (kind, len(common), len(alpha_only), id_list(alpha_only & u2_ids),
                        id_list(alpha_only - u2_ids), len(set(o_ids) - set(a_ids)), id_list(set(o_ids) - set(a_ids))))
    return lines


# ---------------------------------------------------------------------------
# Render
# ---------------------------------------------------------------------------

def describe_ours(stats):
    ours = stats["ours"]
    zi = ours["ZONE_INFO"]
    sp = ours["START_POSITION"]
    parts = []
    if len(zi) == 1:
        parts.append("ZONE_INFO '%s' Init %d/%d/%d RoomEvent %d" % (
            decoded(zi[0]["strZoneName"]), zi[0]["InitX"], zi[0]["InitZ"], zi[0]["InitY"], zi[0]["RoomEvent"]))
    else:
        parts.append("ZONE_INFO %d rows" % len(zi))
    if len(sp) == 1:
        r = sp[0]
        parts.append("START_POSITION %d/%d and %d/%d, gates %s/%s/%s/%s, range %d/%d" % (
            r["sKarusX"], r["sKarusZ"], r["sElmoradX"], r["sElmoradZ"], r["sKarusGateX"], r["sKarusGateZ"],
            r["sElmoGateX"], r["sElmoGateZ"], r["bRangeX"], r["bRangeZ"]))
    else:
        parts.append("START_POSITION %d rows" % len(sp))
    parts.append("K_OBJECTPOS %d rows (%s)" % (len(ours["K_OBJECTPOS"]), id_list(r["sIndex"] for r in ours["K_OBJECTPOS"])))
    o = spawn_summary(ours["K_NPCPOS"])
    parts.append("K_NPCPOS %d rows, %d ids" % (o["rows"], o["ids"]))
    return parts


def object_note(row):
    return "%d (Type %d, Belong %d, ControlNpcID %d) at (%.2f, %.2f)" % (
        row["sIndex"], row["Type"], row["Belong"], row["ControlNpcID"], row["PosX"], row["PosZ"])


def render_apply(columns, stats):
    a = spawn_summary(stats["spawns"])
    counts = stats["alpha_counts"]
    u2_ids = sorted((set(a["npc_ids"]) & stats["u2_npc"]) | (set(a["mon_ids"]) & stats["u2_mon"]))
    ours = describe_ours(stats)
    unknown = stats["unknown_type"]
    unknown_types = sorted(set(r["Type"] for r in unknown))
    dot_rows = len(stats["dot_notes"])
    null_text = sum(1 for r in stats["spawns"] if r["path"] == b"NULL")
    header = """\
%(apply)s
GENERATED by tools/u3-gen-moradon-db.py from the ALPHA database; do not edit by hand.
Regenerate it and run the tool with --check instead.
Source (values): %(alpha_label)s, zone 21: ZONE_INFO %(c_zi)d row, START_POSITION %(c_sp)d row,
                 K_OBJECTPOS %(c_obj)d rows, K_NPCPOS %(c_npc)d rows; sha256 of the rows read %(digest)s
Ids checked in:  %(our_label)s dbo.K_NPC %(our_npc)d rows, dbo.K_MONSTER %(our_mon)d rows
Baseline (ours): %(our_label)s, zone 21 (%(baseline_src)s):
                 %(ours0)s;
                 %(ours1)s;
                 %(ours2)s; %(ours3)s

Purpose: the database side of the new Moradon (zone 21) for the 1534 client (phase
U3, ADR-0068 addendum 2; docs/reports/u0-1534/G-yeni-moradon-smd.md 5 and 7). Apply it
ONLY to the separate 1534 database copy (e.g. %(target)s): zone-21 data is chosen by
the database, and the 1453 client must keep the old Moradon in %(refused)s. The
script refuses to run in a database named %(refused)s.

Changes, zone 21 only, in one transaction:
  ZONE_INFO       strZoneName '%(smd)s' (the server map of plan U3-01);
                  InitX/InitZ/InitY %(ix)d/%(iz)d/%(iy)d and RoomEvent 0 (ALPHA's values; with
                  RoomEvent 0 the AIServer no longer loads 21.aievt, G report 6).
                  ServerNo, Type and bz stay ours.
  START_POSITION  both nations %(sx)d/%(sz)d, bRangeX/bRangeZ %(rx)d/%(rz)d, the four gate columns 0
                  (ALPHA's row). Ours had the range in the gate columns and bRange 0.
  K_OBJECTPOS     our rows are replaced by ALPHA's %(n_obj)d rows:
                  %(obj_lines)s
                  Not copied: the soccer objects %(soccer)s (ADR-0068 addendum 2
                  item 3; sea in this client), and %(n_unknown)d rows of Type %(unknown_types)s (effects,
                  sIndex 0): that type is not in our enum ObjectType (shared/packets.h),
                  no server code uses it, and our database has no such row elsewhere.
  K_NPCPOS        our rows are replaced by ALPHA's %(n_spawn)d rows: %(ids)d ids, NumNPC sum %(num)d;
                  %(npc_rows)d NPC rows (ActType >= 100 -> K_NPC, %(npc_ids)d ids) and %(mon_rows)d monster rows
                  (-> K_MONSTER, %(mon_ids)d ids). Every id exists in our K_NPC/K_MONSTER;
                  db/014 (U2) adds %(n_u2)d of them:
%(u2_ids)s.
                  The script checks this again in the target database and changes
                  nothing if an id is missing (the AIServer would stop loading spawns).
                  Values are ALPHA's, copied by column name. In %(swapped)d rows ALPHA's LimitMinX
                  holds TopZ and LimitMinZ holds LeftX; they are copied as they are: the
                  AIServer reads Limit* only when DungeonFamily > 0 (AIServer/ServerDlg.cpp),
                  and DungeonFamily is 0 in every row. %(null_text)d rows carry the 4-character
                  text 'NULL' in path, as in ALPHA; %(dot_rows)d of them have DotCnt > 0 with ActType
                  100 (standing NPCs, the path is not used).
Bounds: START_POSITION with its range, ZONE_INFO Init / 100, the K_OBJECTPOS positions,
every K_NPCPOS rectangle corner and path point lie in 0..%(max)d (map %(size)d m).
Not part of this script: Lua (U3-03), a USERDATA position reset (owner decision),
warp fees (in the SMD, U3-01).

Backups: the first run copies the zone-21 rows of the four tables into
dbo.<table>%(suffix)s (ZONE_INFO, START_POSITION, K_OBJECTPOS, K_NPCPOS);
a later run leaves them as they are (backup=kept).
%(rollback)s writes them back and drops them.

Usage:
  sqlcmd -S .\\SQLEXPRESS -E -d %(target)s -b -i db/%(apply)s

The script is idempotent: running it again gives the same final state. Expected output:
  backup=created deleted_objpos=<ours> deleted_npcpos=<ours> inserted_objpos=%(n_obj)d inserted_npcpos=%(n_spawn)d
  zone_info=1 start=1 objpos=%(n_obj)d npcpos=%(n_spawn)d objpos_differ=0 npcpos_differ=0
(backup=kept on a later run). The servers load these tables only at start-up: restart
GameServer and AIServer, with Map/%(smd)s (U3-01) in place.""" % {
        "apply": APPLY, "rollback": ROLLBACK, "alpha_label": "%s / %s" % u2.ALPHA_DB,
        "our_label": "%s / %s" % u2.OUR_DB,
        "c_zi": counts["ZONE_INFO"], "c_sp": counts["START_POSITION"], "c_obj": counts["K_OBJECTPOS"],
        "c_npc": counts["K_NPCPOS"], "digest": stats["alpha_digest"],
        "our_npc": stats["our_npc"], "our_mon": stats["our_mon"],
        "baseline_src": "the %s tables" % BACKUP_SUFFIX if stats["from_backup"] else "live tables",
        "ours0": ours[0], "ours1": ours[1], "ours2": ours[2], "ours3": ours[3],
        "target": TARGET_DATABASE_EXAMPLE, "refused": REFUSED_DATABASE, "smd": NEW_SMD,
        "ix": dict(ZONE_INFO_SET)["InitX"], "iz": dict(ZONE_INFO_SET)["InitZ"], "iy": dict(ZONE_INFO_SET)["InitY"],
        "sx": dict(START_POSITION_SET)["sKarusX"], "sz": dict(START_POSITION_SET)["sKarusZ"],
        "rx": dict(START_POSITION_SET)["bRangeX"], "rz": dict(START_POSITION_SET)["bRangeZ"],
        "n_obj": len(stats["objects"]),
        "obj_lines": "\n                  ".join(object_note(r) for r in stats["objects"]),
        "soccer": ", ".join(str(i) for i in SOCCER_OBJECT_IDS),
        "n_unknown": len(unknown), "unknown_types": ", ".join(str(t) for t in unknown_types) or "-",
        "n_spawn": a["rows"], "ids": a["ids"], "num": a["num"],
        "npc_rows": a["npc_rows"], "npc_ids": len(a["npc_ids"]),
        "mon_rows": a["mon_rows"], "mon_ids": len(a["mon_ids"]),
        "n_u2": len(u2_ids), "u2_ids": u2.wrap([str(i) for i in u2_ids], " " * 18, width=86),
        "swapped": stats["swapped"], "null_text": null_text, "dot_rows": dot_rows,
        "max": MAP_SIZE - 1, "size": MAP_SIZE, "suffix": BACKUP_SUFFIX,
    }
    out = [u2.comment_block(header), "",
           "SET NOCOUNT ON;", "SET XACT_ABORT ON;", "GO", "",
           "-- Stop early under -b; the main batch below checks everything again.",
           "IF DB_NAME() = N'%s'" % REFUSED_DATABASE,
           "    RAISERROR(N'this script is for the 1534 database copy, not %s (ADR-0068 addendum 2)', 16, 1);" % REFUSED_DATABASE]
    for table, _ in TABLES:
        out += ["IF OBJECT_ID(N'dbo.%s', N'U') IS NULL" % table,
                "    RAISERROR(N'table dbo.%s not found', 16, 1);" % table]
    out += ["GO", ""]

    for temp, table, rows, label in ((STAGED_OBJECTS, "K_OBJECTPOS", stats["objects"], "object"),
                                     (STAGED_SPAWNS, "K_NPCPOS", stats["spawns"], "spawn")):
        cols = columns[table]
        out += ["-- Staging table for the %d %s rows (dropped at the end of the script)." % (len(rows), label),
                "IF OBJECT_ID(N'tempdb..%s', N'U') IS NOT NULL" % temp,
                "    DROP TABLE %s;" % temp,
                staging_ddl(temp, cols), "",
                "INSERT INTO %s (" % temp, u2.wrap([c[0] for c in cols], "    "), ")", "VALUES"]
        values = []
        for row in rows:
            where = "%s %s" % (table, row.get("sIndex", row.get("NpcID")))
            values.append("    (" + ", ".join(sql_value(row[c[0]], c, where) for c in cols) + ")")
        out += [",\n".join(values) + ";", "GO", ""]

    zone_info_cols = dict((c[0], c) for c in columns["ZONE_INFO"])
    start_cols = dict((c[0], c) for c in columns["START_POSITION"])
    zi_set = ",\n".join("    %s = %s" % (k, sql_value(v, zone_info_cols[k], "ZONE_INFO")) for k, v in ZONE_INFO_SET)
    sp_set = ",\n".join("    %s = %s" % (k, sql_value(v, start_cols[k], "START_POSITION")) for k, v in START_POSITION_SET)
    zi_match = match_sql(ZONE_INFO_SET, columns["ZONE_INFO"], "ZONE_INFO")
    sp_match = match_sql(START_POSITION_SET, columns["START_POSITION"], "START_POSITION")

    backups_exist = backups_exist_sql()
    backup_create = []
    for table, zone_col in TABLES:
        backup_create.append("""\
    SELECT
%(cols)s
    INTO dbo.%(table)s%(suffix)s
    FROM dbo.%(table)s
    WHERE %(where)s;""" % {"cols": u2.wrap([c[0] for c in columns[table]], "        "), "table": table,
                          "suffix": BACKUP_SUFFIX, "where": zone_where(zone_col)})

    obj_cols = u2.wrap([c[0] for c in columns["K_OBJECTPOS"]], "    ")
    npc_cols = u2.wrap([c[0] for c in columns["K_NPCPOS"]], "    ")
    out.append("""\
-- Checks, backups and changes in one batch and one transaction. Nothing is changed
-- when a check fails (also when sqlcmd runs without -b).
DECLARE @obj_staged int = (SELECT COUNT(*) FROM %(st_obj)s);
DECLARE @npc_staged int = (SELECT COUNT(*) FROM %(st_npc)s);
DECLARE @backups int =
    %(backups_exist)s;
DECLARE @zone_info int = (SELECT COUNT(*) FROM dbo.ZONE_INFO WHERE ZoneNo = %(zone)d);
DECLARE @start int = (SELECT COUNT(*) FROM dbo.START_POSITION WHERE ZoneID = %(zone)d);
DECLARE @missing_mon int = (SELECT COUNT(*) FROM %(st_npc)s AS s
    WHERE s.ActType < %(limit)d AND NOT EXISTS (SELECT 1 FROM dbo.K_MONSTER AS m WHERE m.sSid = s.NpcID));
DECLARE @missing_npc int = (SELECT COUNT(*) FROM %(st_npc)s AS s
    WHERE s.ActType >= %(limit)d AND NOT EXISTS (SELECT 1 FROM dbo.K_NPC AS n WHERE n.sSid = s.NpcID));
DECLARE @deleted_obj int;
DECLARE @deleted_npc int;
DECLARE @inserted_obj int;
DECLARE @inserted_npc int;
DECLARE @backup varchar(10) = CASE WHEN @backups = 0 THEN 'created' ELSE 'kept' END;

IF DB_NAME() = N'%(refused)s'
BEGIN
    RAISERROR(N'this script is for the 1534 database copy, not %(refused)s; nothing changed', 16, 1);
    RETURN;
END
IF @obj_staged <> %(n_obj)d OR @npc_staged <> %(n_spawn)d
BEGIN
    RAISERROR(N'staging tables have %%d/%%d rows, expected %(n_obj)d/%(n_spawn)d; nothing changed', 16, 1, @obj_staged, @npc_staged);
    RETURN;
END
IF @backups NOT IN (0, 4)
BEGIN
    RAISERROR(N'%%d of the 4 %(suffix)s tables exist (expected 0 or 4); nothing changed', 16, 1, @backups);
    RETURN;
END
IF @zone_info <> 1 OR @start <> 1
BEGIN
    RAISERROR(N'zone %(zone)d has %%d ZONE_INFO and %%d START_POSITION rows (expected 1 and 1); nothing changed', 16, 1, @zone_info, @start);
    RETURN;
END
IF @missing_mon > 0 OR @missing_npc > 0
BEGIN
    RAISERROR(N'%%d spawn rows name a monster missing in K_MONSTER and %%d an NPC missing in K_NPC (apply db/014 first); nothing changed', 16, 1, @missing_mon, @missing_npc);
    RETURN;
END

BEGIN TRANSACTION;

-- Backups: only on the first run.
IF @backups = 0
BEGIN
%(backup_create)s
END

-- ZONE_INFO and START_POSITION: update the zone-21 row in place.
UPDATE dbo.ZONE_INFO SET
%(zi_set)s
WHERE ZoneNo = %(zone)d;

UPDATE dbo.START_POSITION SET
%(sp_set)s
WHERE ZoneID = %(zone)d;

-- K_OBJECTPOS and K_NPCPOS: replace the zone-21 rows.
DELETE FROM dbo.K_OBJECTPOS WHERE ZoneID = %(zone)d;
SET @deleted_obj = @@ROWCOUNT;

INSERT INTO dbo.K_OBJECTPOS (
%(obj_cols)s
)
SELECT
%(obj_cols)s
FROM %(st_obj)s;
SET @inserted_obj = @@ROWCOUNT;

DELETE FROM dbo.K_NPCPOS WHERE ZoneID = %(zone)d;
SET @deleted_npc = @@ROWCOUNT;

INSERT INTO dbo.K_NPCPOS (
%(npc_cols)s
)
SELECT
%(npc_cols)s
FROM %(st_npc)s;
SET @inserted_npc = @@ROWCOUNT;

COMMIT TRANSACTION;

PRINT 'backup=' + @backup
    + ' deleted_objpos=' + CAST(@deleted_obj AS varchar(10))
    + ' deleted_npcpos=' + CAST(@deleted_npc AS varchar(10))
    + ' inserted_objpos=' + CAST(@inserted_obj AS varchar(10))
    + ' inserted_npcpos=' + CAST(@inserted_npc AS varchar(10));
GO
""" % {"st_obj": STAGED_OBJECTS, "st_npc": STAGED_SPAWNS, "backups_exist": backups_exist, "zone": ZONE,
       "limit": MONSTER_ACT_TYPE_LIMIT, "refused": REFUSED_DATABASE, "n_obj": len(stats["objects"]),
       "n_spawn": len(stats["spawns"]), "suffix": BACKUP_SUFFIX, "backup_create": "\n\n".join(backup_create),
       "zi_set": zi_set, "sp_set": sp_set, "obj_cols": obj_cols, "npc_cols": npc_cols})

    out.append("""\
-- Verification: the zone-21 rows hold exactly the new values; the *_differ counts
-- compare the staged rows with the table rows as multisets (0 after a clean run).
DECLARE @zone_info int = (SELECT COUNT(*) FROM dbo.ZONE_INFO WHERE ZoneNo = %(zone)d
    AND %(zi_match)s);
DECLARE @start int = (SELECT COUNT(*) FROM dbo.START_POSITION WHERE ZoneID = %(zone)d
    AND %(sp_match)s);
DECLARE @zone_info_rows int = (SELECT COUNT(*) FROM dbo.ZONE_INFO WHERE ZoneNo = %(zone)d);
DECLARE @start_rows int = (SELECT COUNT(*) FROM dbo.START_POSITION WHERE ZoneID = %(zone)d);
DECLARE @objpos int = (SELECT COUNT(*) FROM dbo.K_OBJECTPOS WHERE ZoneID = %(zone)d);
DECLARE @npcpos int = (SELECT COUNT(*) FROM dbo.K_NPCPOS WHERE ZoneID = %(zone)d);
DECLARE @obj_differ int =
    %(obj_differ)s;
DECLARE @npc_differ int =
    %(npc_differ)s;

PRINT 'zone_info=' + CAST(@zone_info AS varchar(10))
    + ' start=' + CAST(@start AS varchar(10))
    + ' objpos=' + CAST(@objpos AS varchar(10))
    + ' npcpos=' + CAST(@npcpos AS varchar(10))
    + ' objpos_differ=' + CAST(@obj_differ AS varchar(10))
    + ' npcpos_differ=' + CAST(@npc_differ AS varchar(10));

IF @zone_info <> 1 OR @zone_info_rows <> 1 OR @start <> 1 OR @start_rows <> 1
   OR @objpos <> %(n_obj)d OR @npcpos <> %(n_spawn)d OR @obj_differ <> 0 OR @npc_differ <> 0
    RAISERROR(N'zone %(zone)d does not hold the expected rows', 16, 1);

DROP TABLE %(st_obj)s;
DROP TABLE %(st_npc)s;
GO
""" % {"zone": ZONE, "zi_match": zi_match, "sp_match": sp_match, "n_obj": len(stats["objects"]),
       "n_spawn": len(stats["spawns"]), "st_obj": STAGED_OBJECTS, "st_npc": STAGED_SPAWNS,
       "obj_differ": multiset_differ_sql(columns["K_OBJECTPOS"], STAGED_OBJECTS, "",
                                         "dbo.K_OBJECTPOS", " WHERE ZoneID = %d" % ZONE, "    "),
       "npc_differ": multiset_differ_sql(columns["K_NPCPOS"], STAGED_SPAWNS, "",
                                         "dbo.K_NPCPOS", " WHERE ZoneID = %d" % ZONE, "    ")})
    return "\n".join(out)


def render_rollback(columns, stats):
    header = """\
%(rollback)s
GENERATED by tools/u3-gen-moradon-db.py; do not edit by hand.
Source (values): %(alpha_label)s, zone 21; sha256 of the rows read %(digest)s

Purpose: undo %(apply)s. Writes the zone-21 rows of ZONE_INFO,
START_POSITION, K_OBJECTPOS and K_NPCPOS back from the dbo.<table>%(suffix)s
tables that the first run of %(apply)s made, then drops those four
tables. ZONE_INFO and START_POSITION are updated in place; the K_OBJECTPOS and
K_NPCPOS zone-21 rows are replaced. Other zones are never touched.

Usage:
  sqlcmd -S .\\SQLEXPRESS -E -d %(target)s -b -i db/%(rollback)s

The script can be run again: without the backup tables it changes nothing
(restored=0 backup_tables=absent). Expected output after %(apply)s:
  restored=1 zone_info=1 start=1 objpos=<ours> npcpos=<ours> objpos_differ=0 npcpos_differ=0 backup_tables=absent
The servers load these tables only at start-up; restart them to see the change.""" % {
        "rollback": ROLLBACK, "apply": APPLY, "alpha_label": "%s / %s" % u2.ALPHA_DB,
        "digest": stats["alpha_digest"], "suffix": BACKUP_SUFFIX, "target": TARGET_DATABASE_EXAMPLE}
    backups_exist = backups_exist_sql()
    updates = []
    for table, zone_col in TABLES[:2]:
        sets = ",\n".join("    t.%s = b.%s" % (c[0], c[0]) for c in columns[table] if c[0] != zone_col)
        updates.append("""\
UPDATE t SET
%(sets)s
FROM dbo.%(table)s AS t
JOIN dbo.%(table)s%(suffix)s AS b
    ON b.%(zone_col)s = t.%(zone_col)s
WHERE t.%(zone_col)s = %(zone)d;""" % {"sets": sets, "table": table,
                                      "suffix": BACKUP_SUFFIX, "zone_col": zone_col, "zone": ZONE})
    replaces = []
    for table, zone_col in TABLES[2:]:
        cols = u2.wrap([c[0] for c in columns[table]], "    ")
        replaces.append("""\
DELETE FROM dbo.%(table)s WHERE %(zone_col)s = %(zone)d;

INSERT INTO dbo.%(table)s (
%(cols)s
)
SELECT
%(cols)s
FROM dbo.%(table)s%(suffix)s;""" % {"table": table, "zone_col": zone_col, "zone": ZONE, "cols": cols,
                                     "suffix": BACKUP_SUFFIX})
    drops = "\n".join("DROP TABLE dbo.%s%s;" % (t, BACKUP_SUFFIX) for t, _ in TABLES)
    out = [u2.comment_block(header), "",
           "SET NOCOUNT ON;", "SET XACT_ABORT ON;", "GO", ""]
    for table, _ in TABLES:
        out += ["IF OBJECT_ID(N'dbo.%s', N'U') IS NULL" % table,
                "    RAISERROR(N'table dbo.%s not found', 16, 1);" % table]
    out += ["GO", ""]
    out.append("""\
DECLARE @backups int =
    %(backups_exist)s;

IF @backups NOT IN (0, 4)
BEGIN
    RAISERROR(N'%%d of the 4 %(suffix)s tables exist (expected 0 or 4); nothing changed', 16, 1, @backups);
    RETURN;
END

IF @backups = 0
BEGIN
    PRINT 'restored=0 backup_tables=absent';
    RETURN;
END

DECLARE @zone_info_rows int = (SELECT COUNT(*) FROM dbo.ZONE_INFO WHERE ZoneNo = %(zone)d);
DECLARE @start_rows int = (SELECT COUNT(*) FROM dbo.START_POSITION WHERE ZoneID = %(zone)d);
DECLARE @zone_info_backup int = (SELECT COUNT(*) FROM dbo.ZONE_INFO%(suffix)s);
DECLARE @start_backup int = (SELECT COUNT(*) FROM dbo.START_POSITION%(suffix)s);

IF @zone_info_rows <> 1 OR @start_rows <> 1 OR @zone_info_backup <> 1 OR @start_backup <> 1
BEGIN
    RAISERROR(N'zone %(zone)d ZONE_INFO/START_POSITION rows %%d/%%d, backup rows %%d/%%d (expected 1 each); nothing changed', 16, 1,
        @zone_info_rows, @start_rows, @zone_info_backup, @start_backup);
    RETURN;
END

BEGIN TRANSACTION;

%(updates)s

%(replaces)s

DECLARE @zone_info int = (SELECT COUNT(*) FROM (
    SELECT
%(zi_cols)s
    FROM dbo.ZONE_INFO WHERE ZoneNo = %(zone)d
    INTERSECT
    SELECT
%(zi_cols)s
    FROM dbo.ZONE_INFO%(suffix)s) AS d);
DECLARE @start int = (SELECT COUNT(*) FROM (
    SELECT
%(sp_cols)s
    FROM dbo.START_POSITION WHERE ZoneID = %(zone)d
    INTERSECT
    SELECT
%(sp_cols)s
    FROM dbo.START_POSITION%(suffix)s) AS d);
DECLARE @objpos int = (SELECT COUNT(*) FROM dbo.K_OBJECTPOS WHERE ZoneID = %(zone)d);
DECLARE @npcpos int = (SELECT COUNT(*) FROM dbo.K_NPCPOS WHERE ZoneID = %(zone)d);
DECLARE @obj_differ int =
    %(obj_differ)s;
DECLARE @npc_differ int =
    %(npc_differ)s;

IF @zone_info <> 1 OR @start <> 1 OR @obj_differ <> 0 OR @npc_differ <> 0
BEGIN
    ROLLBACK TRANSACTION;
    RAISERROR(N'restored rows differ from the backup; nothing changed', 16, 1);
    RETURN;
END

%(drops)s

COMMIT TRANSACTION;

PRINT 'restored=1 zone_info=' + CAST(@zone_info AS varchar(10))
    + ' start=' + CAST(@start AS varchar(10))
    + ' objpos=' + CAST(@objpos AS varchar(10))
    + ' npcpos=' + CAST(@npcpos AS varchar(10))
    + ' objpos_differ=' + CAST(@obj_differ AS varchar(10))
    + ' npcpos_differ=' + CAST(@npc_differ AS varchar(10))
    + ' backup_tables=' + CASE WHEN OBJECT_ID(N'dbo.K_NPCPOS%(suffix)s', N'U') IS NULL THEN 'absent' ELSE 'present' END;
GO
""" % {"backups_exist": backups_exist, "suffix": BACKUP_SUFFIX, "zone": ZONE,
       "updates": "\n\n".join(updates), "replaces": "\n\n".join(replaces), "drops": drops,
       "zi_cols": u2.wrap([c[0] for c in columns["ZONE_INFO"]], "        "),
       "sp_cols": u2.wrap([c[0] for c in columns["START_POSITION"]], "        "),
       "obj_differ": multiset_differ_sql(columns["K_OBJECTPOS"], "dbo.K_OBJECTPOS%s" % BACKUP_SUFFIX, "",
                                         "dbo.K_OBJECTPOS", " WHERE ZoneID = %d" % ZONE, "    "),
       "npc_differ": multiset_differ_sql(columns["K_NPCPOS"], "dbo.K_NPCPOS%s" % BACKUP_SUFFIX, "",
                                         "dbo.K_NPCPOS", " WHERE ZoneID = %d" % ZONE, "    ")})
    return "\n".join(out)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def to_ascii(name, text):
    try:
        data = text.encode("ascii")
    except UnicodeEncodeError:
        raise GenError("%s would not be ASCII" % name)
    if b"$(" in data:
        raise GenError("%s contains a sqlcmd variable marker" % name)
    return data


def main(argv=None):
    ap = argparse.ArgumentParser(description="Generate db/015 (zone 21: ZONE_INFO, START_POSITION, K_OBJECTPOS, K_NPCPOS) from the ALPHA DB.")
    ap.add_argument("--sqlcmd", default=u2.DEFAULT_SQLCMD, help="path of SQLCMD.EXE")
    ap.add_argument("--check", action="store_true",
                    help="compare the generated scripts with db/ byte by byte (0 = identical)")
    args = ap.parse_args(argv)

    our_db = u2.Db(args.sqlcmd, *u2.OUR_DB)
    alpha_db = u2.Db(args.sqlcmd, *u2.ALPHA_DB)
    try:
        columns, alpha, stats = build(our_db, alpha_db)
        outputs = {APPLY: to_ascii(APPLY, render_apply(columns, stats)),
                   ROLLBACK: to_ascii(ROLLBACK, render_rollback(columns, stats))}
    except (OSError, GenError) as e:
        sys.stderr.write("error: %s\n" % e)
        return 2

    counts = stats["alpha_counts"]
    print("ALPHA zone 21: ZONE_INFO %d, START_POSITION %d, K_OBJECTPOS %d, K_NPCPOS %d rows"
          % (counts["ZONE_INFO"], counts["START_POSITION"], counts["K_OBJECTPOS"], counts["K_NPCPOS"]))
    print("K_OBJECTPOS: selected %s; soccer not copied %s; unknown Type not copied %d rows (Type %s); known types %s"
          % (id_list(r["sIndex"] for r in stats["objects"]), id_list(r["sIndex"] for r in stats["soccer"]),
             len(stats["unknown_type"]), id_list(set(r["Type"] for r in stats["unknown_type"])),
             id_list(stats["known_types"])))
    print("K_NPCPOS: LimitMin swapped rows %d; DotCnt > 0 without a numeric path: %s"
          % (stats["swapped"], ", ".join("%d (ActType %d, DotCnt %d, path %r)" % n for n in stats["dot_notes"]) or "-"))
    print("ours (%s): %s" % ("backup tables" if stats["from_backup"] else "live", "; ".join(describe_ours(stats))))
    for line in comparison(stats):
        print("compare: " + line)

    db_dir = os.path.join(REPO_DIR, "db")
    if args.check:
        differ = 0
        for name in sorted(outputs):
            try:
                with open(os.path.join(db_dir, name), "rb") as f:
                    current = f.read()
            except OSError:
                current = None
            if current == outputs[name]:
                print("same: db/%s" % name)
            else:
                print("DIFFERENT: db/%s" % name)
                differ += 1
        print("check %s" % ("OK" if differ == 0 else "FAILED (%d file(s) differ)" % differ))
        return 0 if differ == 0 else 1

    for name in sorted(outputs):
        with open(os.path.join(db_dir, name), "wb") as f:
            f.write(outputs[name])
        print("wrote db/%s (%d bytes)" % (name, len(outputs[name])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
