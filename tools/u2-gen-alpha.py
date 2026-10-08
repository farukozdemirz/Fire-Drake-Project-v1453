#!/usr/bin/env python3
"""Generates the U2 1534 item, NPC and monster scripts from the ALPHA database.

    db/013_u2_items_1534.sql          (+ _rollback.sql)   ITEM rows
    db/014_u2_npc_monster_1534.sql    (+ _rollback.sql)   K_NPC and K_MONSTER rows

Sources (all read only):
  - ALPHA DB  .\\SQL2019   / FDP_alpha1534: the row values (SELECT only).
  - OUR DB    .\\SQLEXPRESS / FDP_kn_online: the column list of our tables
    (INFORMATION_SCHEMA) and the ids that we already have (SELECT only). Ids that
    are recorded in dbo.<table>_U2_ADDED (written by the generated scripts) are not
    counted as ours, so the output stays the same after the scripts were applied.
  - 1534 client Data/*.tbl, decoded with tools/kotbl.py: which ids the client knows.
Both databases are reached with SQLCMD.EXE (Windows authentication, -E).

Scope (plan U2-02, ADR-0068 addendum 1, docs/reports/u0-1534/E-veri-farki.md):
  items     ids that the 1534 client resolves (an item_org_us base row plus an
            Item_Ext_<n>_us variant whose BaseID is 0 or that base; E 3.1), that our
            ITEM lacks and that ALPHA has. Values are ALPHA's (requirements stay
            ALPHA's); ItemClass and ItemExt are computed here (ITEM_CLASS_RULE).
  npcs      K_NPC: client Npc_us ids that our K_NPC lacks, minus EXCLUDED_NPC_IDS;
            K_MONSTER: client Mob_us ids that our K_MONSTER lacks. Rows come from
            ALPHA; strName comes from the client table (NAME_RULE).
Columns: our column list. A column that ALPHA lacks is an error when it is NOT NULL
without a default, unless it is listed in OURS_ONLY_FILL (columns the server does not
load). ALPHA-only columns are not copied. Every value is checked against the type and
length of our column; a value that does not fit is an error.

Usage:
    python3 tools/u2-gen-alpha.py [items|npcs|all] [--client DIR]          # write scripts
    python3 tools/u2-gen-alpha.py [items|npcs|all] [--client DIR] --check  # 0 = db/ up to date

The output is deterministic: rows sorted by id, integers in plain decimal, ASCII, LF.
Standard library only; works under python3 -I.
"""

import argparse
import hashlib
import importlib.util
import os
import re
import subprocess
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(TOOLS_DIR)
DEFAULT_CLIENT_DIR = "/mnt/c/dev/fdp1534/client/Knight Online/Data"
DEFAULT_SQLCMD = "/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
OUR_DB = (".\\SQLEXPRESS", "FDP_kn_online")
ALPHA_DB = (".\\SQL2019", "FDP_alpha1534")

ITEMS_APPLY = "013_u2_items_1534.sql"
ITEMS_ROLLBACK = "013_u2_items_1534_rollback.sql"
NPCS_APPLY = "014_u2_npc_monster_1534.sql"
NPCS_ROLLBACK = "014_u2_npc_monster_1534_rollback.sql"

ITEM_BATCH_ROWS = 1000  # rows per INSERT ... VALUES statement (the SQL Server limit)
ITEM_EXT_TABLES = 64    # Item_Ext_<n>_us.tbl files are looked up for n = 0..63

# Our zone-64 warders use these K_NPC ids; the 1534 client uses them for other NPCs
# (E 4.1). They are never part of the NPC script (ADR-0068 addendum 1).
EXCLUDED_NPC_IDS = (24438, 24439, 24440)

# Columns of our K_NPC/K_MONSTER that ALPHA lacks. They are NOT NULL without a
# default, and the server does not load them (shared/database/NpcTableSet.h); the
# generator checks that the loader column list still does not name them.
OURS_ONLY_FILL = {
    "K_NPC": {"sLightR": 0, "byMoneyType": 0},
    "K_MONSTER": {"sLightR": 0, "byMoneyType": 0},
}
NPC_LOADER_HEADER = os.path.join("shared", "database", "NpcTableSet.h")

# item_org_us.tbl columns (E 3.1): 0 id, 1 byExtIndex, 10 Kind, 36 grade.
ORG_ID, ORG_EXT_INDEX, ORG_KIND, ORG_GRADE = 0, 1, 10, 36
# Item_Ext_<n>_us.tbl columns: 0 variant id (0..999), 2 BaseID, 7 ItemType.
EXT_ID, EXT_BASE_ID, EXT_ITEM_TYPE = 0, 2, 7
KNOWN_GRADES = (0, 1, 2, 3, 5)

ACCESSORY_ITEM_EXT = {91: 18, 92: 19, 93: 20, 94: 21}  # Kind -> ItemExt of class-8 rows
# The named rings Flame Ring, Shio Tears, Imir Ring and Foverin: our ITEM uses
# ItemExt 23 on all their class-8 rows (36 rows), and our ITEM_UPGRADE has the
# matching nOriginType 23 recipes.
SPECIAL_RING_BASES = (330910000, 330920000, 330930000, 330940000)
SPECIAL_RING_ITEM_EXT = 23

ITEM_CLASS_RULE = """\
ItemClass (E 3.4; ALPHA has NULL in every row). grade = client item_org_us
column 36 of the base row, digit = Num % 10, unique = ItemType 4, designated =
the Item_Ext variant names this base as its BaseID (a named unique).
  1. ItemType 3                                    -> 0
  2. grade 5                                       -> 4
  3. grade 3                                       -> 3
  4. grade 1, unique, and digit >= 7 or designated -> 3
  5. grade 1 or 2                                  -> grade + 1 if digit >= 7, else grade
  6. grade 0, Kind 91..94, unique and designated   -> 8 (upgradeable accessory)
  7. otherwise                                     -> 0
Against the E 3.4 wording, rule 4 adds "or designated" and rule 6 reads
"upgradeable" as designated (both measured on our ITEM; plan U2-02 report).
ItemExt: 0, except class 8: Kind 91 -> 18, 92 -> 19, 93 -> 20, 94 -> 21, and 23
under the named-ring bases 330910000, 330920000, 330930000, 330940000."""

NAME_RULE = """\
strName: the client name (Npc_us / Mob_us column 1, trimmed) when it is printable
ASCII and fits our column; else the ALPHA name under the same test; else "Npc <id>"."""


class GenError(Exception):
    pass


def load_kotbl():
    # Load tools/kotbl.py by path: python3 -I does not put the script directory on sys.path.
    spec = importlib.util.spec_from_file_location("kotbl", os.path.join(TOOLS_DIR, "kotbl.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# ---------------------------------------------------------------------------
# Database access (SELECT only)
# ---------------------------------------------------------------------------

class Db(object):
    def __init__(self, sqlcmd, server, database):
        self.sqlcmd = sqlcmd
        self.server = server
        self.database = database

    def label(self):
        return "%s / %s" % (self.server, self.database)

    def query(self, sql):
        """Runs one read-only query; returns the rows as lists of strings."""
        args = [self.sqlcmd, "-S", self.server, "-E", "-d", self.database, "-b",
                "-h", "-1", "-W", "-s", "|", "-w", "65535", "-Q", "SET NOCOUNT ON; " + sql]
        try:
            proc = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except OSError as e:
            raise GenError("cannot run %s: %s" % (self.sqlcmd, e))
        out = proc.stdout.decode("ascii", errors="replace")
        if proc.returncode != 0:
            raise GenError("query on %s failed (exit %d): %s\n%s%s" % (
                self.label(), proc.returncode, sql, out, proc.stderr.decode("ascii", errors="replace")))
        return [line.split("|") for line in out.replace("\r\n", "\n").split("\n") if line != ""]

    def columns(self, table):
        """[(name, data_type, max_length, nullable, has_default)] in ordinal order."""
        rows = self.query(
            "SELECT COLUMN_NAME, DATA_TYPE, ISNULL(CHARACTER_MAXIMUM_LENGTH, 0), IS_NULLABLE,"
            " CASE WHEN COLUMN_DEFAULT IS NULL THEN 0 ELSE 1 END"
            " FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'dbo' AND TABLE_NAME = '%s'"
            " ORDER BY ORDINAL_POSITION" % table)
        if not rows:
            raise GenError("table dbo.%s not found on %s" % (table, self.label()))
        columns = [(r[0], r[1].lower(), int(r[2]), r[3] == "YES", r[4] == "1") for r in rows]
        for name, dtype, length, _, _ in columns:
            if dtype in ("char", "varchar") and not 0 < length <= 100:
                raise GenError("%s.%s is %s(%d); rows() reads strings of at most 100 bytes"
                               % (table, name, dtype, length))
        return columns

    def ids(self, table, id_col):
        return [int(r[0]) for r in self.query("SELECT %s FROM dbo.%s" % (id_col, table))]

    def logged_ids(self, table, id_col):
        """Ids recorded by the generated script in dbo.<table>_U2_ADDED (empty if absent)."""
        log = "%s_U2_ADDED" % table
        exists = self.query("SELECT CASE WHEN OBJECT_ID(N'dbo.%s', N'U') IS NULL THEN 0 ELSE 1 END" % log)
        if exists[0][0] != "1":
            return set()
        return set(int(r[0]) for r in self.query("SELECT %s FROM dbo.%s" % (id_col, log)))

    def rows(self, table, columns, id_col, where=""):
        """Fetches the columns; strings travel as hex so that no byte is lost.

        Returns (rows by id, ids that occur more than once)."""
        select = []
        for name, dtype in columns:
            if dtype in ("char", "varchar"):
                # 100 bytes -> 200 hex digits: below sqlcmd's default display width.
                select.append("CONVERT(varchar(200), CAST(%s AS varbinary(100)), 2)" % name)
            else:
                select.append(name)
        sql = "SELECT %s FROM dbo.%s%s" % (", ".join(select), table, where)
        result = {}
        duplicates = set()
        for raw in self.query(sql):
            if len(raw) != len(columns):
                raise GenError("%s.%s: got %d fields, expected %d" % (self.label(), table, len(raw), len(columns)))
            row = {}
            for (name, dtype), text in zip(columns, raw):
                if text == "NULL":
                    row[name] = None
                elif dtype in ("char", "varchar"):
                    row[name] = bytes.fromhex(text)
                else:
                    row[name] = int(text)
            key = row[id_col]
            if key in result:
                duplicates.add(key)
            result[key] = row
        return result, duplicates


# ---------------------------------------------------------------------------
# Values and SQL text
# ---------------------------------------------------------------------------

INT_RANGES = {"tinyint": (0, 255), "smallint": (-32768, 32767), "int": (-2147483648, 2147483647)}


def is_plain_ascii(text):
    return all(0x20 <= ord(ch) < 0x7F for ch in text)


def text_of(raw):
    """Decodes a char/varchar value (bytes) to text; None for non-ASCII bytes."""
    if raw is None:
        return None
    if any(b < 0x20 or b >= 0x7F for b in raw.rstrip(b" ")):
        return None
    return raw.decode("ascii").rstrip(" ")


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
    if dtype in ("char", "varchar"):
        if not is_plain_ascii(value):
            raise GenError("%s: %s=%r is not printable ASCII" % (where, name, value))
        if len(value) > length:
            raise GenError("%s: %s=%r is longer than %s(%d)" % (where, name, value, dtype, length))
        if "$(" in value:
            raise GenError("%s: %s=%r contains a sqlcmd variable marker" % (where, name, value))
        return "'" + value.replace("'", "''") + "'"
    raise GenError("%s: column %s has unsupported type %s" % (where, name, dtype))


def column_ddl(column):
    name, dtype, length, nullable, _ = column
    if dtype in ("char", "varchar"):
        text = "%s %s(%d) COLLATE DATABASE_DEFAULT" % (name, dtype, length)
    elif dtype in INT_RANGES:
        text = "%s %s" % (name, dtype)
    else:
        raise GenError("column %s has unsupported type %s" % (name, dtype))
    return text + (" NULL" if nullable else " NOT NULL")


def staging_table_sql(table, columns, id_col):
    lines = ["    " + column_ddl(c) for c in columns]
    lines.append("    PRIMARY KEY (%s)" % id_col)
    return "CREATE TABLE %s\n(\n%s\n);" % (table, ",\n".join(lines))


def values_sql(rows, columns, label):
    out = []
    for row in rows:
        where = "%s %d" % (label, row[columns[0][0]])
        out.append("    (" + ", ".join(sql_value(row[c[0]], c, where) for c in columns) + ")")
    return out


def wrap(names, indent, width=88):
    """Comma-separated names wrapped to lines (used for long column lists)."""
    lines = []
    current = indent
    for i, name in enumerate(names):
        piece = name + ("," if i < len(names) - 1 else "")
        if current.strip() and len(current) + 1 + len(piece) > width:
            lines.append(current.rstrip())
            current = indent
        current += ("" if current == indent else " ") + piece
    lines.append(current.rstrip())
    return "\n".join(lines)


def comment_block(text):
    return "\n".join(("-- " + line).rstrip() for line in text.split("\n"))


def plan_columns(our_columns, alpha_names, table, computed):
    """Decides, for each of our columns, where its value comes from.

    Returns (insert columns, fill values for ours-only columns, ALPHA-only names)."""
    fill = {}
    insert = []
    our_names = set(c[0] for c in our_columns)
    for column in our_columns:
        name, _, _, nullable, has_default = column
        if name in alpha_names or name in computed:
            insert.append(column)
        elif name in OURS_ONLY_FILL.get(table, {}):
            fill[name] = OURS_ONLY_FILL[table][name]
            insert.append(column)
        elif nullable or has_default:
            continue  # left to NULL / the column default
        else:
            raise GenError("our %s.%s is NOT NULL without a default and ALPHA has no such column" % (table, name))
    alpha_only = sorted(n for n in alpha_names if n not in our_names)
    return insert, fill, alpha_only


def check_fill_not_loaded(fill_columns):
    """The ours-only fill columns must not be read by the server's NPC loader."""
    path = os.path.join(REPO_DIR, NPC_LOADER_HEADER)
    with open(path, "rb") as f:
        text = f.read().decode("latin-1")
    lists = re.findall(r'GetColumns\(\)\s*\{\s*return _T\("([^"]*)"\)', text)
    if not lists:
        raise GenError("%s: no GetColumns() list found" % NPC_LOADER_HEADER)
    loaded = set(name.strip() for part in lists for name in part.split(","))
    used = sorted(set(fill_columns) & loaded)
    if used:
        raise GenError("%s loads %s; a constant fill value is not acceptable" % (NPC_LOADER_HEADER, used))


# ---------------------------------------------------------------------------
# Client tables
# ---------------------------------------------------------------------------

def file_info(path):
    with open(path, "rb") as f:
        raw = f.read()
    return raw, len(raw), hashlib.sha256(raw).hexdigest()


def load_client_table(kotbl, path):
    raw, _, _ = file_info(path)
    try:
        table = kotbl.decode(raw)
    except kotbl.TableError as e:
        raise GenError("cannot decode %s: %s" % (path, e))
    if table.trailing_bytes:
        raise GenError("%s has %d trailing bytes" % (path, table.trailing_bytes))
    return table


def read_client_items(kotbl, data_dir):
    """Returns (resolved, source text): resolved maps item id -> (org row, ext row)."""
    digest = hashlib.sha256()
    files = 0
    total = 0
    org_path = os.path.join(data_dir, "item_org_us.tbl")
    org_table = load_client_table(kotbl, org_path)
    if org_table.columns != 39:
        raise GenError("item_org_us.tbl has %d columns; the 1534 table has 39" % org_table.columns)
    raw, size, sha = file_info(org_path)
    digest.update(("item_org_us.tbl %s\n" % sha).encode("ascii"))
    files += 1
    total += size
    org = {}
    for row in org_table.rows:
        if row[ORG_ID] in org:
            raise GenError("item_org_us.tbl: base %d appears twice" % row[ORG_ID])
        if row[ORG_GRADE] not in KNOWN_GRADES:
            raise GenError("item_org_us.tbl: base %d has unknown grade %d" % (row[ORG_ID], row[ORG_GRADE]))
        org[row[ORG_ID]] = row
    ext = {}
    for index in range(ITEM_EXT_TABLES):
        path = os.path.join(data_dir, "Item_Ext_%d_us.tbl" % index)
        if not os.path.exists(path):
            continue
        table = load_client_table(kotbl, path)
        if table.columns != 53:
            raise GenError("Item_Ext_%d_us.tbl has %d columns, expected 53" % (index, table.columns))
        raw, size, sha = file_info(path)
        digest.update(("Item_Ext_%d_us.tbl %s\n" % (index, sha)).encode("ascii"))
        files += 1
        total += size
        ext[index] = table.rows
    resolved = {}
    for base in sorted(org):
        row = org[base]
        for variant in ext.get(row[ORG_EXT_INDEX], []):
            if variant[EXT_BASE_ID] not in (0, base):
                continue
            num = base + variant[EXT_ID]
            if num in resolved:
                raise GenError("client item id %d resolves twice (bases %d and %d)" % (num, resolved[num][0][ORG_ID], base))
            resolved[num] = (row, variant)
    source = "Data/item_org_us.tbl + %d Item_Ext_<n>_us.tbl files, %d bytes, combined sha256 %s, %d bases, %d resolvable ids" % (
        files - 1, total, digest.hexdigest(), len(org), len(resolved))
    return resolved, source


def read_client_names(kotbl, data_dir, name, columns):
    path = os.path.join(data_dir, name)
    table = load_client_table(kotbl, path)
    if table.columns != columns or table.types[1] != kotbl.TYPE_STRING:
        raise GenError("%s has %d columns (types %s); expected %d with a name in column 1"
                       % (name, table.columns, table.types, columns))
    names = {}
    for row in table.rows:
        if row[0] in names:
            raise GenError("%s: id %d appears twice" % (name, row[0]))
        names[row[0]] = row[1]
    _, size, sha = file_info(path)
    return names, "Data/%s, %d bytes, sha256 %s, %d rows" % (name, size, sha, len(names))


# ---------------------------------------------------------------------------
# Items
# ---------------------------------------------------------------------------

def item_class(num, org_row, ext_row, kind, item_type):
    grade = org_row[ORG_GRADE]
    designated = ext_row[EXT_BASE_ID] != 0
    high = num % 10 >= 7
    if item_type == 3:
        return 0
    if grade == 5:
        return 4
    if grade == 3:
        return 3
    if grade == 1 and item_type == 4 and (high or designated):
        return 3
    if grade in (1, 2):
        return grade + (1 if high else 0)
    if kind in ACCESSORY_ITEM_EXT and item_type == 4 and designated:
        return 8
    return 0


def item_ext(item_cls, org_row, kind):
    if item_cls != 8:
        return 0
    if org_row[ORG_ID] in SPECIAL_RING_BASES:
        return SPECIAL_RING_ITEM_EXT
    return ACCESSORY_ITEM_EXT[kind]


def rule_check(our_db, baseline_ids, resolved):
    """Applies the ItemClass/ItemExt rule to our own ITEM rows (plan 5.3)."""
    rows, _ = our_db.rows("ITEM", [("Num", "int"), ("Kind", "tinyint"), ("ItemType", "tinyint"),
                                   ("ItemClass", "smallint"), ("ItemExt", "smallint")], "Num")
    total = match = ext_total = ext_match = 0
    misses = {}
    for num in sorted(rows):
        if num not in baseline_ids or num not in resolved:
            continue
        row = rows[num]
        org_row, ext_row = resolved[num]
        predicted = item_class(num, org_row, ext_row, row["Kind"], row["ItemType"])
        total += 1
        if predicted == row["ItemClass"]:
            match += 1
        else:
            key = (predicted, row["ItemClass"])
            misses[key] = misses.get(key, 0) + 1
        if row["ItemClass"] == 8:
            ext_total += 1
            if item_ext(8, org_row, row["Kind"]) == row["ItemExt"]:
                ext_match += 1
    return total, match, misses, ext_total, ext_match


def build_items(our_db, alpha_db, kotbl, data_dir):
    resolved, client_source = read_client_items(kotbl, data_dir)
    our_columns = our_db.columns("ITEM")
    alpha_columns = alpha_db.columns("ITEM")
    alpha_names = set(c[0] for c in alpha_columns)
    computed = ("ItemClass", "ItemExt")
    insert, fill, alpha_only = plan_columns(our_columns, alpha_names, "ITEM", computed)
    if insert[0][0] != "Num" or fill:
        raise GenError("unexpected ITEM column plan (first %s, fill %s)" % (insert[0][0], fill))
    for name in ("Num", "strName", "Kind", "ItemType") + computed:
        if name not in [c[0] for c in insert]:
            raise GenError("our ITEM has no %s column" % name)

    our_all = set(our_db.ids("ITEM", "Num"))
    logged = our_db.logged_ids("ITEM", "Num")
    baseline = our_all - logged
    missing = sorted(n for n in resolved if n not in baseline)

    fetch = [(c[0], c[1]) for c in insert if c[0] in alpha_names]
    alpha_rows, alpha_dups = alpha_db.rows("ITEM", fetch, "Num")
    scope = [n for n in missing if n in alpha_rows]
    not_in_alpha = [n for n in missing if n not in alpha_rows]
    dups = sorted(set(scope) & alpha_dups)
    if dups:
        raise GenError("ALPHA ITEM has duplicate rows for in-scope ids %s" % dups[:20])
    if not scope:
        raise GenError("no item is in scope")

    rows = []
    classes = {}
    fallback_names = 0
    alpha_digest = hashlib.sha256()
    for num in scope:
        src = alpha_rows[num]
        org_row, ext_row = resolved[num]
        if src["Kind"] != org_row[ORG_KIND] or src["ItemType"] != ext_row[EXT_ITEM_TYPE]:
            raise GenError("item %d: ALPHA Kind/ItemType %s/%s differ from the client %s/%s" % (
                num, src["Kind"], src["ItemType"], org_row[ORG_KIND], ext_row[EXT_ITEM_TYPE]))
        alpha_digest.update(repr(sorted(src.items())).encode("ascii"))
        row = dict(src)
        name = text_of(src["strName"])
        if name is None:
            name = "Item %d" % num
            fallback_names += 1
        row["strName"] = name
        cls = item_class(num, org_row, ext_row, src["Kind"], src["ItemType"])
        row["ItemClass"] = cls
        row["ItemExt"] = item_ext(cls, org_row, src["Kind"])
        classes[cls] = classes.get(cls, 0) + 1
        rows.append(row)

    check = rule_check(our_db, baseline, resolved)
    stats = {
        "resolvable": len(resolved),
        "our_rows": len(baseline),
        "our_logged": len(logged & our_all),
        "missing": len(missing),
        "scope": len(scope),
        "not_in_alpha": not_in_alpha,
        "alpha_rows": len(alpha_rows),
        "alpha_only_columns": alpha_only,
        "classes": classes,
        "fallback_names": fallback_names,
        "rule": check,
    }
    # E 3.2 split: ids under bases (Num // 1000) of which our ITEM has no row at all.
    our_prefixes = set(n // 1000 for n in baseline)
    under_new = [n for n in scope if n // 1000 not in our_prefixes]
    stats["under_new_bases"] = len(under_new)
    stats["new_bases"] = len(set(n // 1000 for n in under_new))
    source = {
        "client": client_source,
        "alpha": "%s dbo.ITEM, %d rows read; in-scope rows sha256 %s" % (
            alpha_db.label(), len(alpha_rows), alpha_digest.hexdigest()),
        "ours": "%s dbo.ITEM, %d rows (ids recorded in dbo.ITEM_U2_ADDED not counted)" % (
            our_db.label(), len(baseline)),
    }
    return insert, rows, stats, source


def class_summary(classes):
    return ", ".join("%d: %d" % (k, classes[k]) for k in sorted(classes))


def render_items_apply(columns, rows, stats, source):
    names = [c[0] for c in columns]
    total, match, misses, ext_total, ext_match = stats["rule"]
    header = """\
013_u2_items_1534.sql
GENERATED by tools/u2-gen-alpha.py (items) from the ALPHA database and the 1534 client.
Do not edit by hand; regenerate it and run the tool with --check instead.
Source (values): %(alpha)s
Source (scope):  %(client)s
Baseline (ours): %(ours)s

Purpose: add the items that the 1534 client knows and our ITEM lacks (phase U2,
ADR-0068 item 4 and addendum 1). Scope: the ids that the client resolves (an
item_org_us base plus an Item_Ext variant whose BaseID is 0 or that base) and
that our ITEM lacks: %(missing)d ids; %(scope)d of them are in ALPHA and are inserted
here (%(notalpha)s). Existing rows are never changed.
Values are ALPHA's, including the level and stat requirements (ADR-0068 addendum 1);
ALPHA-only columns (%(alphaonly)s) are not copied.

%(rule)s
Rule check on our own ITEM rows that the client resolves: %(match)d of %(total)d
match (%(rate)s); ItemExt on our class-8 rows: %(ext_match)d of %(ext_total)d.
ItemClass of the inserted rows: %(classes)s.
Names: ALPHA's strName; a name that is not printable ASCII would become "Item <id>"
(%(fallback)d rows).

Usage:
  sqlcmd -S .\\SQLEXPRESS -E -d FDP_kn_online -b -v Target=ITEM -i db/013_u2_items_1534.sql

The Target variable is required on purpose (no default); it names the item table.
The rows are first loaded into the session temp table #u2_items in %(batches)d
batches of at most %(batch)d rows, then inserted with one INSERT ... SELECT ... WHERE
NOT EXISTS (Num) in a transaction. Every inserted id is recorded in
dbo.<Target>_U2_ADDED so that 013_u2_items_1534_rollback.sql can remove exactly
those rows. The script is idempotent: running it again inserts nothing (inserted=0).
The servers need not be stopped, but they load ITEM only at start-up: restart them
to see the change.""" % {
        "alpha": source["alpha"], "client": source["client"], "ours": source["ours"],
        "missing": stats["missing"], "scope": stats["scope"],
        "notalpha": "client ids in neither database: %s" % (
            ", ".join(str(n) for n in stats["not_in_alpha"]) or "none"),
        "alphaonly": ", ".join(stats["alpha_only_columns"]) or "none",
        "rule": ITEM_CLASS_RULE, "match": match, "total": total,
        "rate": "%.2f%%" % (100.0 * match / total) if total else "n/a",
        "ext_match": ext_match, "ext_total": ext_total,
        "classes": class_summary(stats["classes"]), "fallback": stats["fallback_names"],
        "batches": (len(rows) + ITEM_BATCH_ROWS - 1) // ITEM_BATCH_ROWS, "batch": ITEM_BATCH_ROWS,
    }
    col_list = wrap(names, "    ")
    out = [comment_block(header), "",
           "SET NOCOUNT ON;", "SET XACT_ABORT ON;", "GO", "",
           "IF OBJECT_ID(N'dbo.$(Target)', N'U') IS NULL",
           "    RAISERROR(N'target table dbo.$(Target) not found', 16, 1);", "GO", "",
           "IF OBJECT_ID(N'dbo.$(Target)', N'U') IS NOT NULL",
           "   AND OBJECT_ID(N'dbo.$(Target)_U2_ADDED', N'U') IS NULL",
           "CREATE TABLE dbo.$(Target)_U2_ADDED",
           "(",
           "    Num int NOT NULL PRIMARY KEY,",
           "    dtAdded datetime NOT NULL DEFAULT GETDATE()",
           ");", "GO", "",
           "-- Staging table for the %d rows (dropped at the end of the script)." % len(rows),
           "IF OBJECT_ID(N'tempdb..#u2_items', N'U') IS NOT NULL",
           "    DROP TABLE #u2_items;",
           staging_table_sql("#u2_items", columns, "Num"), "GO", ""]
    values = values_sql(rows, columns, "item")
    batches = (len(values) + ITEM_BATCH_ROWS - 1) // ITEM_BATCH_ROWS
    for b in range(batches):
        chunk = values[b * ITEM_BATCH_ROWS:(b + 1) * ITEM_BATCH_ROWS]
        out.append("-- Batch %d of %d: ids %s..%s" % (
            b + 1, batches, rows[b * ITEM_BATCH_ROWS]["Num"], rows[b * ITEM_BATCH_ROWS + len(chunk) - 1]["Num"]))
        out.append("INSERT INTO #u2_items (")
        out.append(col_list)
        out.append(")")
        out.append("VALUES")
        out.append(",\n".join(chunk) + ";")
        out.append("GO")
        out.append("")
    out.append("""\
-- Insert the staged rows that the target lacks. A short staging table means that
-- a batch above failed (sqlcmd without -b goes on): then nothing is inserted.
DECLARE @total int = (SELECT COUNT(*) FROM #u2_items);
DECLARE @inserted int;

IF @total <> %(count)d
BEGIN
    RAISERROR(N'#u2_items has %%d rows, expected %(count)d; nothing inserted', 16, 1, @total);
    RETURN;
END

BEGIN TRANSACTION;

-- Record the ids that are about to be inserted.
INSERT INTO dbo.$(Target)_U2_ADDED (Num)
SELECT s.Num
FROM #u2_items AS s
WHERE NOT EXISTS (SELECT 1 FROM dbo.$(Target) AS t WHERE t.Num = s.Num)
  AND NOT EXISTS (SELECT 1 FROM dbo.$(Target)_U2_ADDED AS a WHERE a.Num = s.Num);

-- Insert only the ids that the target does not have.
INSERT INTO dbo.$(Target) (
%(cols)s
)
SELECT
%(cols)s
FROM #u2_items AS s
WHERE NOT EXISTS (SELECT 1 FROM dbo.$(Target) AS t WHERE t.Num = s.Num);

SET @inserted = @@ROWCOUNT;

COMMIT TRANSACTION;

PRINT 'inserted=' + CAST(@inserted AS varchar(10))
    + ' already_present=' + CAST(@total - @inserted AS varchar(10));
GO

-- Verification: row count of the target, ids in the log table, and staged rows
-- that are not in the target with exactly these values (0 after a clean run).
DECLARE @rows int = (SELECT COUNT(*) FROM dbo.$(Target));
DECLARE @logged int = (SELECT COUNT(*) FROM dbo.$(Target)_U2_ADDED);
DECLARE @differ int = (SELECT COUNT(*) FROM (
    SELECT
%(cols8)s
    FROM #u2_items
    EXCEPT
    SELECT
%(cols8)s
    FROM dbo.$(Target)) AS d);
PRINT 'target_rows=' + CAST(@rows AS varchar(10))
    + ' logged=' + CAST(@logged AS varchar(10))
    + ' staged_not_in_target=' + CAST(@differ AS varchar(10));

DROP TABLE #u2_items;
GO
""" % {"count": len(rows), "cols": col_list, "cols8": wrap(names, "        ")})
    return "\n".join(out)


def render_rollback(name, apply_name, source_lines, targets):
    """targets: [(sqlcmd variable, id column, output label, table in real use)]"""
    usage_vars = " ".join("-v %s=%s" % (var, real) for var, _, _, real in targets)
    out = [comment_block("""\
%(name)s
GENERATED by tools/u2-gen-alpha.py; do not edit by hand.
%(source)s

Purpose: undo %(apply)s. Deletes only the ids recorded in the log
table(s) dbo.<target>_U2_ADDED, then drops the log table(s). Rows that existed
before %(apply_short)s was applied are never touched.

Usage:
  sqlcmd -S .\\SQLEXPRESS -E -d FDP_kn_online -b %(vars)s -i db/%(name)s

The variables are required on purpose (no default). The script can be run again:
without a log table it changes nothing (removed=0).
The servers load these tables only at start-up; restart them to see the change.""" % {
        "name": name, "source": "\n".join(source_lines), "apply": apply_name,
        "apply_short": apply_name.split("_")[0], "vars": usage_vars}), "",
        "SET NOCOUNT ON;", "SET XACT_ABORT ON;", "GO", ""]
    for var, _, _, _ in targets:
        out += ["IF OBJECT_ID(N'dbo.$(%s)', N'U') IS NULL" % var,
                "    RAISERROR(N'target table dbo.$(%s) not found', 16, 1);" % var]
    out += ["GO", ""]
    for var, id_col, label, _ in targets:
        out.append("""\
DECLARE @removed int = 0;

IF OBJECT_ID(N'dbo.$(%(var)s)_U2_ADDED', N'U') IS NOT NULL
BEGIN
    BEGIN TRANSACTION;

    -- Remove only the rows that %(apply_short)s inserted.
    DELETE t
    FROM dbo.$(%(var)s) AS t
    JOIN dbo.$(%(var)s)_U2_ADDED AS a
        ON a.%(id)s = t.%(id)s;

    SET @removed = @@ROWCOUNT;

    DROP TABLE dbo.$(%(var)s)_U2_ADDED;

    COMMIT TRANSACTION;
END

DECLARE @rows int = (SELECT COUNT(*) FROM dbo.$(%(var)s));
PRINT '%(label)sremoved=' + CAST(@removed AS varchar(10))
    + ' target_rows=' + CAST(@rows AS varchar(10))
    + ' log_table=' + CASE WHEN OBJECT_ID(N'dbo.$(%(var)s)_U2_ADDED', N'U') IS NULL THEN 'absent' ELSE 'present' END;
GO
""" % {"var": var, "id": id_col, "label": label, "apply_short": apply_name.split("_")[0]})
    return "\n".join(out)


# ---------------------------------------------------------------------------
# NPCs and monsters
# ---------------------------------------------------------------------------

def choose_name(entity_id, client_name, alpha_raw, length):
    for candidate in ((client_name or "").strip(), (text_of(alpha_raw) or "").strip()):
        if candidate and is_plain_ascii(candidate) and len(candidate) <= length and "$(" not in candidate:
            return candidate, candidate == (client_name or "").strip()
    return "Npc %d" % entity_id, None


def build_npc_table(our_db, alpha_db, table, client_names, excluded):
    our_columns = our_db.columns(table)
    alpha_columns = alpha_db.columns(table)
    alpha_names = set(c[0] for c in alpha_columns)
    insert, fill, alpha_only = plan_columns(our_columns, alpha_names, table, ("strName",))
    if insert[0][0] != "sSid" or insert[1][0] != "strName":
        raise GenError("unexpected %s column order: %s" % (table, [c[0] for c in insert[:3]]))
    check_fill_not_loaded(fill)
    name_col = insert[1]

    our_all = set(our_db.ids(table, "sSid"))
    logged = our_db.logged_ids(table, "sSid")
    baseline = our_all - logged
    missing = sorted(i for i in client_names if i not in baseline and i not in excluded)
    fetch = [(c[0], c[1]) for c in insert if c[0] in alpha_names]
    alpha_rows, alpha_dups = alpha_db.rows(table, fetch, "sSid")
    scope = [i for i in missing if i in alpha_rows]
    not_in_alpha = [i for i in missing if i not in alpha_rows]
    dups = sorted(set(scope) & alpha_dups)
    if dups:
        raise GenError("ALPHA %s has duplicate rows for in-scope ids %s" % (table, dups))
    rows = []
    names = {"client": 0, "alpha": [], "generated": []}
    digest = hashlib.sha256()
    for sid in scope:
        src = alpha_rows[sid]
        digest.update(repr(sorted(src.items())).encode("ascii"))
        row = dict(src)
        row.update(fill)
        name, from_client = choose_name(sid, client_names[sid], src["strName"], name_col[2])
        if from_client:
            names["client"] += 1
        elif from_client is None:
            names["generated"].append(sid)
        else:
            names["alpha"].append(sid)
        row["strName"] = name
        rows.append(row)
    stats = {
        "our_rows": len(baseline), "logged": len(logged & our_all), "missing_total": len(
            [i for i in client_names if i not in baseline]),
        "excluded_present": sorted(i for i in excluded if i in client_names and i not in baseline),
        "scope": len(scope), "not_in_alpha": not_in_alpha, "alpha_rows": len(alpha_rows),
        "alpha_only_columns": alpha_only, "fill": fill, "names": names,
        "alpha_digest": digest.hexdigest(),
    }
    return insert, rows, stats


def build_npcs(our_db, alpha_db, kotbl, data_dir):
    npc_names, npc_source = read_client_names(kotbl, data_dir, "Npc_us.tbl", 7)
    mob_names, mob_source = read_client_names(kotbl, data_dir, "Mob_us.tbl", 4)
    npc = build_npc_table(our_db, alpha_db, "K_NPC", npc_names, set(EXCLUDED_NPC_IDS))
    mon = build_npc_table(our_db, alpha_db, "K_MONSTER", mob_names, set())
    source = {"npc_client": npc_source, "mob_client": mob_source,
              "alpha": "%s dbo.K_NPC (%d rows read, in-scope sha256 %s), dbo.K_MONSTER (%d rows read, in-scope sha256 %s)" % (
                  alpha_db.label(), npc[2]["alpha_rows"], npc[2]["alpha_digest"],
                  mon[2]["alpha_rows"], mon[2]["alpha_digest"]),
              "ours": "%s dbo.K_NPC %d rows, dbo.K_MONSTER %d rows (ids in the _U2_ADDED tables not counted)" % (
                  our_db.label(), npc[2]["our_rows"], mon[2]["our_rows"])}
    return npc, mon, source


def name_note(stats):
    n = stats["names"]
    return "%d client names, %d ALPHA names (%s), %d generated (%s)" % (
        n["client"], len(n["alpha"]), ", ".join(str(i) for i in n["alpha"]) or "-",
        len(n["generated"]), ", ".join(str(i) for i in n["generated"]) or "-")


def render_npcs_apply(npc, mon, source):
    npc_cols, npc_rows, npc_stats = npc
    mon_cols, mon_rows, mon_stats = mon
    header = """\
014_u2_npc_monster_1534.sql
GENERATED by tools/u2-gen-alpha.py (npcs) from the ALPHA database and the 1534 client.
Do not edit by hand; regenerate it and run the tool with --check instead.
Source (values): %(alpha)s
Source (scope):  %(npc_client)s
                 %(mob_client)s
Baseline (ours): %(ours)s

Purpose: add the NPCs and monsters that the 1534 client knows and our tables lack
(phase U2, ADR-0068 item 4 and addendum 1). Only ids that the client tables name
are copied; ALPHA's other ids are not.
K_NPC: %(npc_missing)d client ids are missing in ours; %(npc_scope)d are inserted here
(not in ALPHA: %(npc_notalpha)s). The three ids of our zone-64 warders (Warder 1,
Warder 2, Keeper), which the 1534 client uses for other NPCs, are never part of
this script; ours keep their rows.
K_MONSTER: %(mon_missing)d client ids are missing in ours; %(mon_scope)d are inserted
here (not in ALPHA, skipped: %(mon_notalpha)s). Their drop tables (sItem ->
K_MONSTER_ITEM) are not part of this script.
Values are ALPHA's. ALPHA-only columns: %(alphaonly)s. Our columns that ALPHA
lacks get a constant (the server does not load them): %(fill)s.
%(name_rule)s
K_NPC names: %(npc_names)s.
K_MONSTER names: %(mon_names)s.

Usage:
  sqlcmd -S .\\SQLEXPRESS -E -d FDP_kn_online -b -v NpcTarget=K_NPC -v MonTarget=K_MONSTER -i db/014_u2_npc_monster_1534.sql

NpcTarget and MonTarget are required on purpose (no default). The rows are first
loaded into the session temp tables #u2_npcs and #u2_monsters, then inserted with
INSERT ... SELECT ... WHERE NOT EXISTS (sSid) in one transaction. Every inserted id
is recorded in dbo.<NpcTarget>_U2_ADDED / dbo.<MonTarget>_U2_ADDED so that
014_u2_npc_monster_1534_rollback.sql can remove exactly those rows. The script is
idempotent: running it again inserts nothing (inserted=0).
The servers need not be stopped, but they load these tables only at start-up:
restart them to see the change.""" % {
        "alpha": source["alpha"], "npc_client": source["npc_client"], "mob_client": source["mob_client"],
        "ours": source["ours"],
        "npc_missing": npc_stats["missing_total"], "npc_scope": npc_stats["scope"],
        "npc_notalpha": ", ".join(str(i) for i in npc_stats["not_in_alpha"]) or "none",
        "mon_missing": mon_stats["missing_total"], "mon_scope": mon_stats["scope"],
        "mon_notalpha": ", ".join(str(i) for i in mon_stats["not_in_alpha"]) or "none",
        "alphaonly": ", ".join(sorted(set(npc_stats["alpha_only_columns"]) | set(mon_stats["alpha_only_columns"]))) or "none",
        "fill": ", ".join("%s=%d" % (k, v) for k, v in sorted(npc_stats["fill"].items())) or "none",
        "name_rule": NAME_RULE, "npc_names": name_note(npc_stats), "mon_names": name_note(mon_stats),
    }
    out = [comment_block(header), "",
           "SET NOCOUNT ON;", "SET XACT_ABORT ON;", "GO", "",
           "IF OBJECT_ID(N'dbo.$(NpcTarget)', N'U') IS NULL",
           "    RAISERROR(N'target table dbo.$(NpcTarget) not found', 16, 1);",
           "IF OBJECT_ID(N'dbo.$(MonTarget)', N'U') IS NULL",
           "    RAISERROR(N'target table dbo.$(MonTarget) not found', 16, 1);", "GO", ""]
    for var in ("NpcTarget", "MonTarget"):
        out += ["IF OBJECT_ID(N'dbo.$(%s)', N'U') IS NOT NULL" % var,
                "   AND OBJECT_ID(N'dbo.$(%s)_U2_ADDED', N'U') IS NULL" % var,
                "CREATE TABLE dbo.$(%s)_U2_ADDED" % var,
                "(",
                "    sSid smallint NOT NULL PRIMARY KEY,",
                "    dtAdded datetime NOT NULL DEFAULT GETDATE()",
                ");", "GO", ""]
    parts = (("#u2_npcs", npc_cols, npc_rows, "npc", "NpcTarget"),
             ("#u2_monsters", mon_cols, mon_rows, "monster", "MonTarget"))
    for temp, cols, rows, label, _ in parts:
        out += ["-- Staging table for the %d %s rows (dropped at the end of the script)." % (len(rows), label),
                "IF OBJECT_ID(N'tempdb..%s', N'U') IS NOT NULL" % temp,
                "    DROP TABLE %s;" % temp,
                staging_table_sql(temp, cols, "sSid"), ""]
        if rows:
            out += ["INSERT INTO %s (" % temp, wrap([c[0] for c in cols], "    "), ")", "VALUES",
                    ",\n".join(values_sql(rows, cols, label)) + ";"]
        out += ["GO", ""]
    out.append("""\
-- Insert the staged rows that the targets lack. A short staging table means that a
-- batch above failed (sqlcmd without -b goes on): then nothing is inserted.
DECLARE @npc_total int = (SELECT COUNT(*) FROM #u2_npcs);
DECLARE @mon_total int = (SELECT COUNT(*) FROM #u2_monsters);
DECLARE @npc_inserted int;
DECLARE @mon_inserted int;

IF @npc_total <> %(npc_count)d OR @mon_total <> %(mon_count)d
BEGIN
    RAISERROR(N'staging tables have %%d/%%d rows, expected %(npc_count)d/%(mon_count)d; nothing inserted', 16, 1, @npc_total, @mon_total);
    RETURN;
END

BEGIN TRANSACTION;
""" % {"npc_count": len(npc_rows), "mon_count": len(mon_rows)})
    for temp, cols, rows, label, var in parts:
        short = "npc" if label == "npc" else "mon"
        out.append("""\
-- %(label)s: record the ids that are about to be inserted, then insert them.
INSERT INTO dbo.$(%(var)s)_U2_ADDED (sSid)
SELECT s.sSid
FROM %(temp)s AS s
WHERE NOT EXISTS (SELECT 1 FROM dbo.$(%(var)s) AS t WHERE t.sSid = s.sSid)
  AND NOT EXISTS (SELECT 1 FROM dbo.$(%(var)s)_U2_ADDED AS a WHERE a.sSid = s.sSid);

INSERT INTO dbo.$(%(var)s) (
%(cols)s
)
SELECT
%(cols)s
FROM %(temp)s AS s
WHERE NOT EXISTS (SELECT 1 FROM dbo.$(%(var)s) AS t WHERE t.sSid = s.sSid);

SET @%(short)s_inserted = @@ROWCOUNT;
""" % {"label": label, "var": var, "temp": temp, "cols": wrap([c[0] for c in cols], "    "), "short": short})
    out.append("""\
COMMIT TRANSACTION;

PRINT 'npc inserted=' + CAST(@npc_inserted AS varchar(10))
    + ' already_present=' + CAST(@npc_total - @npc_inserted AS varchar(10));
PRINT 'monster inserted=' + CAST(@mon_inserted AS varchar(10))
    + ' already_present=' + CAST(@mon_total - @mon_inserted AS varchar(10));
GO

-- Verification: row counts, logged ids, and staged rows that are not in the
-- target with exactly these values (0 after a clean run).""")
    for temp, cols, rows, label, var in parts:
        out.append("""\
DECLARE @%(label)s_rows int = (SELECT COUNT(*) FROM dbo.$(%(var)s));
DECLARE @%(label)s_logged int = (SELECT COUNT(*) FROM dbo.$(%(var)s)_U2_ADDED);
DECLARE @%(label)s_differ int = (SELECT COUNT(*) FROM (
    SELECT
%(cols8)s
    FROM %(temp)s
    EXCEPT
    SELECT
%(cols8)s
    FROM dbo.$(%(var)s)) AS d);
PRINT '%(label)s target_rows=' + CAST(@%(label)s_rows AS varchar(10))
    + ' logged=' + CAST(@%(label)s_logged AS varchar(10))
    + ' staged_not_in_target=' + CAST(@%(label)s_differ AS varchar(10));
""" % {"label": label, "var": var, "temp": temp, "cols8": wrap([c[0] for c in cols], "        ")})
    out.append("DROP TABLE #u2_npcs;\nDROP TABLE #u2_monsters;\nGO\n")
    return "\n".join(out)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def to_ascii(name, text):
    try:
        return text.encode("ascii")
    except UnicodeEncodeError:
        raise GenError("%s would not be ASCII" % name)


def generate_items(our_db, alpha_db, kotbl, data_dir, report):
    columns, rows, stats, source = build_items(our_db, alpha_db, kotbl, data_dir)
    total, match, misses, ext_total, ext_match = stats["rule"]
    report.append("items: client resolvable=%d, missing in ours=%d, in ALPHA (inserted)=%d, in neither=%s"
                  % (stats["resolvable"], stats["missing"], stats["scope"],
                     ",".join(str(n) for n in stats["not_in_alpha"]) or "-"))
    report.append("items: ours baseline=%d (logged ids excluded: %d), name fallbacks=%d"
                  % (stats["our_rows"], stats["our_logged"], stats["fallback_names"]))
    report.append("items: inserted under %d bases our ITEM lacks entirely=%d, under bases ours has=%d"
                  % (stats["new_bases"], stats["under_new_bases"], stats["scope"] - stats["under_new_bases"]))
    report.append("items: ItemClass of inserted rows: %s" % class_summary(stats["classes"]))
    report.append("items: rule on our rows %d/%d = %.3f%% (misses predicted->ours: %s); ItemExt on class 8: %d/%d"
                  % (match, total, 100.0 * match / total if total else 0.0,
                     ", ".join("%s->%s:%d" % (k[0], k[1], v) for k, v in sorted(misses.items(), key=lambda x: (-x[1], str(x[0])))),
                     ext_match, ext_total))
    source_lines = ["Source (values): " + source["alpha"], "Source (scope):  " + source["client"],
                    "Baseline (ours): " + source["ours"]]
    return {
        ITEMS_APPLY: to_ascii(ITEMS_APPLY, render_items_apply(columns, rows, stats, source)),
        ITEMS_ROLLBACK: to_ascii(ITEMS_ROLLBACK, render_rollback(
            ITEMS_ROLLBACK, ITEMS_APPLY, source_lines, [("Target", "Num", "", "ITEM")])),
    }


def generate_npcs(our_db, alpha_db, kotbl, data_dir, report):
    npc, mon, source = build_npcs(our_db, alpha_db, kotbl, data_dir)
    for label, part in (("K_NPC", npc), ("K_MONSTER", mon)):
        st = part[2]
        report.append("%s: client ids missing in ours=%d, inserted=%d, not in ALPHA=%s, excluded=%s, fill=%s"
                      % (label, st["missing_total"], st["scope"], ",".join(str(i) for i in st["not_in_alpha"]) or "-",
                         ",".join(str(i) for i in st["excluded_present"]) or "-",
                         ",".join("%s=%d" % kv for kv in sorted(st["fill"].items())) or "-"))
        report.append("%s: names: %s" % (label, name_note(st)))
    apply_text = render_npcs_apply(npc, mon, source)
    # Every VALUES row starts with its sSid; no row may carry an excluded id. (The
    # digits can still occur as other values, e.g. as an sPid look id.)
    row_ids = set(int(m) for m in re.findall(r"(?m)^    \((\d+), ", apply_text))
    if row_ids != set(r["sSid"] for r in npc[1]) | set(r["sSid"] for r in mon[1]):
        raise GenError("the VALUES rows of %s do not match the selected ids" % NPCS_APPLY)
    if row_ids & set(EXCLUDED_NPC_IDS):
        raise GenError("excluded NPC ids %s appear in %s" % (sorted(row_ids & set(EXCLUDED_NPC_IDS)), NPCS_APPLY))
    source_lines = ["Source (values): " + source["alpha"], "Source (scope):  " + source["npc_client"],
                    "                 " + source["mob_client"]]
    return {
        NPCS_APPLY: to_ascii(NPCS_APPLY, apply_text),
        NPCS_ROLLBACK: to_ascii(NPCS_ROLLBACK, render_rollback(
            NPCS_ROLLBACK, NPCS_APPLY, source_lines,
            [("NpcTarget", "sSid", "npc ", "K_NPC"), ("MonTarget", "sSid", "monster ", "K_MONSTER")])),
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description="Generate db/013 (ITEM) and db/014 (K_NPC, K_MONSTER) from the ALPHA DB.")
    ap.add_argument("command", nargs="?", default="all", choices=("all", "items", "npcs"))
    ap.add_argument("--client", default=DEFAULT_CLIENT_DIR, help="the 1534 client Data folder")
    ap.add_argument("--sqlcmd", default=DEFAULT_SQLCMD, help="path of SQLCMD.EXE")
    ap.add_argument("--check", action="store_true",
                    help="compare the generated scripts with db/ byte by byte (0 = identical)")
    args = ap.parse_args(argv)

    our_db = Db(args.sqlcmd, *OUR_DB)
    alpha_db = Db(args.sqlcmd, *ALPHA_DB)
    report = []
    outputs = {}
    try:
        kotbl = load_kotbl()
        if args.command in ("all", "items"):
            outputs.update(generate_items(our_db, alpha_db, kotbl, args.client, report))
        if args.command in ("all", "npcs"):
            outputs.update(generate_npcs(our_db, alpha_db, kotbl, args.client, report))
    except (OSError, GenError) as e:
        sys.stderr.write("error: %s\n" % e)
        return 2
    for line in report:
        print(line)

    db_dir = os.path.join(REPO_DIR, "db")
    if args.check:
        differ = 0
        for name in sorted(outputs):
            path = os.path.join(db_dir, name)
            try:
                with open(path, "rb") as f:
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
