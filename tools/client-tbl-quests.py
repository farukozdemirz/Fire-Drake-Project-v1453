#!/usr/bin/env python3
"""Read-only decoder for the client skill table (Client/Data/Skill_Magic_Main_us.tbl).

The client table is encrypted with a rolling XOR (key 0x0816, c1 0x6081, c2 0x1608):
    plain = enc ^ (key >> 8);  key = ((enc + key) * c1 + c2) & 0xFFFF
After decryption the layout is: int32 column count, int32 type per column, int32 row
count, then the rows (type 7 = int32 length + CP949 string, 1 int8, 2 uint8, 3 int16,
4 uint16, 5 int32, 6 uint32, 8 float32, 9 float64). Column 29 is the quest id that
unlocks the skill in the client ("?" icon until the quest is done), column 15 is the
required level / skill points, column 0 is the skill id (MAGIC.MagicNum).

With --server the quest ids are compared with MAGIC.Etc and MAGIC.UseStanding of the
local database (game data only; no user tables are read). The tool never writes
anything and never touches the client files.

Usage:
    python3 tools/client-tbl-quests.py [--client PATH] [--server] [--sqlcmd PATH]
        [--sql-server INSTANCE] [--db DB] [--list]
    python3 tools/client-tbl-quests.py --selftest
"""

import argparse
import struct
import subprocess
import sys

DEFAULT_CLIENT = "/mnt/c/dev/fdp/Client/Data/Skill_Magic_Main_us.tbl"
DEFAULT_SQLCMD = "/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
DEFAULT_SERVER = ".\\SQLEXPRESS"
DEFAULT_DB = "FDP_kn_online"

COL_SKILL_ID = 0
COL_NAME = 1
COL_REQUIRED = 15
COL_QUEST = 29

KEY0 = 0x0816
C1 = 0x6081
C2 = 0x1608

FIXED = {1: ("<b", 1), 2: ("<B", 1), 3: ("<h", 2), 4: ("<H", 2),
         5: ("<i", 4), 6: ("<I", 4), 8: ("<f", 4), 9: ("<d", 8)}


def decrypt(data):
    key = KEY0
    out = bytearray()
    for b in data:
        out.append(b ^ (key >> 8))
        key = ((b + key) * C1 + C2) & 0xFFFF
    return bytes(out)


def encrypt(data):
    key = KEY0
    out = bytearray()
    for p in data:
        e = p ^ (key >> 8)
        out.append(e)
        key = ((e + key) * C1 + C2) & 0xFFFF
    return bytes(out)


def parse(plain):
    """Returns (types, rows, end_offset). Raises ValueError on a malformed table."""
    offset = 0
    (ncol,) = struct.unpack_from("<i", plain, offset)
    offset += 4
    if ncol <= 0 or ncol > 256:
        raise ValueError("bad column count %d" % ncol)
    types = list(struct.unpack_from("<%di" % ncol, plain, offset))
    offset += 4 * ncol
    (nrow,) = struct.unpack_from("<i", plain, offset)
    offset += 4
    rows = []
    for _ in range(nrow):
        row = []
        for t in types:
            if t == 7:
                (n,) = struct.unpack_from("<i", plain, offset)
                offset += 4
                row.append(plain[offset:offset + n].decode("cp949", errors="replace"))
                offset += n
            elif t in FIXED:
                fmt, size = FIXED[t]
                row.append(struct.unpack_from(fmt, plain, offset)[0])
                offset += size
            else:
                raise ValueError("unknown column type %d" % t)
        rows.append(row)
    return types, rows, offset


def load(path):
    raw = open(path, "rb").read()
    plain = decrypt(raw)
    types, rows, end = parse(plain)
    return types, rows, end, len(plain)


def server_rows(sqlcmd, server, db, ids):
    query = ("SET NOCOUNT ON; SELECT MagicNum, Etc, UseStanding FROM MAGIC WHERE MagicNum IN (%s)"
             % ",".join(str(i) for i in ids))
    out = subprocess.run([sqlcmd, "-S", server, "-E", "-d", db, "-W", "-h", "-1", "-s", "|", "-Q", query],
                         capture_output=True, text=True, errors="replace").stdout
    result = {}
    for line in out.splitlines():
        parts = line.strip().split("|")
        if len(parts) >= 3:
            try:
                result[int(parts[0])] = (int(parts[1]), int(parts[2]))
            except ValueError:
                pass
    return result


def build_table(types, rows):
    out = bytearray()
    out += struct.pack("<i", len(types))
    out += struct.pack("<%di" % len(types), *types)
    out += struct.pack("<i", len(rows))
    for row in rows:
        for t, v in zip(types, row):
            if t == 7:
                b = v.encode("cp949")
                out += struct.pack("<i", len(b)) + b
            else:
                out += struct.pack(FIXED[t][0], v)
    return bytes(out)


def run_selftest():
    sample = bytes(range(256)) * 3
    assert decrypt(encrypt(sample)) == sample, "cipher roundtrip"
    types = [6, 7, 5, 5]
    rows = [[110570, "incineration", 70, 53], [110503, "Burn", 3, 0]]
    plain = build_table(types, rows)
    t2, r2, end = parse(decrypt(encrypt(plain)))
    assert t2 == types and r2 == rows and end == len(plain), "table roundtrip"
    print("selftest OK")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument("--client", default=DEFAULT_CLIENT)
    ap.add_argument("--server", action="store_true", help="compare with MAGIC.Etc / MAGIC.UseStanding")
    ap.add_argument("--sqlcmd", default=DEFAULT_SQLCMD)
    ap.add_argument("--sql-server", default=DEFAULT_SERVER)
    ap.add_argument("--db", default=DEFAULT_DB)
    ap.add_argument("--list", action="store_true", help="print every quest-locked skill row")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)
    if args.selftest:
        return run_selftest()

    types, rows, end, size = load(args.client)
    print("CLIENT cols=%d rows=%d end_offset=%d size=%d end_ok=%d"
          % (len(types), len(rows), end, size, 1 if end == size else 0))
    locked = [r for r in rows if r[COL_QUEST] != 0]
    quests = sorted(set(r[COL_QUEST] for r in locked))
    print("QUEST_ROWS %d quest_ids=%d: %s" % (len(locked), len(quests), ",".join(str(q) for q in quests)))
    for q in quests:
        group = [r for r in locked if r[COL_QUEST] == q]
        levels = sorted(set(r[COL_REQUIRED] for r in group))
        print("QUEST %d rows=%d required=%s" % (q, len(group), ",".join(str(v) for v in levels)))
        if args.list:
            for r in group:
                print("  SKILL %d %s required=%d" % (r[COL_SKILL_ID], r[COL_NAME].strip(), r[COL_REQUIRED]))

    if args.server:
        srv = server_rows(args.sqlcmd, args.sql_server, args.db, [r[COL_SKILL_ID] for r in locked])
        etc_equal = use_equal = other = missing = 0
        for r in locked:
            sid = r[COL_SKILL_ID]
            if sid not in srv:
                missing += 1
                continue
            etc, use = srv[sid]
            if etc == r[COL_QUEST]:
                etc_equal += 1
            elif use == r[COL_QUEST] and etc in (0, 1):
                use_equal += 1
            else:
                other += 1
        print("SERVER rows=%d missing=%d etc_equals_quest=%d usestanding_equals_quest=%d other=%d"
              % (len(srv), missing, etc_equal, use_equal, other))
    return 0


if __name__ == "__main__":
    sys.exit(main())
