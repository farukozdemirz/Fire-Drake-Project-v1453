#!/usr/bin/env python3
"""Reports bot gear equip eligibility, item race/class fields and weight limits.

Reads the 12 bot rows written by db/002_bot_characters.sql and the ITEM table.

Usage:
    python3 tools/bot-gear-report.py [--sqlcmd PATH] [--server INSTANCE] [--db DB]
    python3 tools/bot-gear-report.py --selftest
"""

import argparse
import io
import struct
import subprocess
import sys

DEFAULT_SQLCMD = "/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
DEFAULT_SERVER = ".\\SQLEXPRESS"
DEFAULT_DB = "FDP_kn_online"

SLOT_MAX = 14
INVENTORY_COSP = 42              # SLOT_MAX + HAVE_MAX (shared/globals.h)
COSP_BAG1 = INVENTORY_COSP + 5
COSP_BAG2 = INVENTORY_COSP + 6
ITEM_SLOTS = 73
WEIGHT_AMOUNTS = (0, 50, 100, 150, 200, 255)

BOT_QUERY = "SELECT RTRIM(strUserID), Nation, Race, [Class], Level, [Rank], Title, Strong, Sta, Dex, Intel, Cha, CONVERT(varchar(1200), strItem, 2) FROM USERDATA WHERE strUserID LIKE 'Bot%' ORDER BY strUserID"

ITEM_COLUMNS = (
    "Num, RTRIM(strName), Kind, Slot, Race, [Class], Weight, Duration, Ac, Countable, "
    "ReqLevel, ReqLevelMax, ReqRank, ReqTitle, ReqStr, ReqSta, ReqDex, ReqIntel, ReqCha, "
    "StrB, StaB, DexB, IntelB, ChaB, MaxHpB, MaxMpB"
)


def ascii_text(value):
    return value.encode("ascii", "replace").decode("ascii")


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


def parse_slots(hex_text):
    data = bytes.fromhex(hex_text)
    if len(data) != ITEM_SLOTS * 8:
        raise ValueError("strItem is %d bytes, expected %d" % (len(data), ITEM_SLOTS * 8))
    slots = []
    for slot in range(ITEM_SLOTS):
        item_id, duration, count = struct.unpack("<IhH", data[slot * 8:(slot + 1) * 8])
        if item_id != 0:
            slots.append((slot, item_id, duration, count))
    return slots


def load_bots(rows):
    bots = []
    for row in rows:
        if len(row) != 13:
            raise ValueError("unexpected bot row with %d fields" % len(row))
        slots = parse_slots(row[12])
        bots.append({
            "name": row[0],
            "nation": int(row[1]),
            "race": int(row[2]),
            "class": int(row[3]),
            "level": int(row[4]),
            "rank": int(row[5]),
            "title": int(row[6]),
            "strong": int(row[7]),
            "sta": int(row[8]),
            "dex": int(row[9]),
            "intel": int(row[10]),
            "cha": int(row[11]),
            "slots": slots,
            "equipment": [(s, i, d, c) for s, i, d, c in slots if s < SLOT_MAX],
        })
    return bots


def load_items(rows):
    items = {}
    for row in rows:
        if len(row) != 26:
            raise ValueError("unexpected ITEM row with %d fields" % len(row))
        item_id = int(row[0])
        values = [int(value) for value in row[2:]]
        items[item_id] = {
            "id": item_id,
            "name": row[1],
            "kind": values[0],
            "slot": values[1],
            "race": values[2],
            "class": values[3],
            "weight": values[4],
            "duration": values[5],
            "ac": values[6],
            "countable": values[7],
            "req_level": values[8],
            "req_level_max": values[9],
            "req_rank": values[10],
            "req_title": values[11],
            "req_str": values[12],
            "req_sta": values[13],
            "req_dex": values[14],
            "req_intel": values[15],
            "req_cha": values[16],
            "strb": values[17],
            "stab": values[18],
            "dexb": values[19],
            "intelb": values[20],
            "chab": values[21],
            "maxhpb": values[22],
            "maxmpb": values[23],
        }
    return items


def equip_failures(bot, item):
    checks = (
        ("ReqLevel", item["req_level"], bot["level"], lambda req, value: value >= req),
        ("ReqLevelMax", item["req_level_max"], bot["level"], lambda req, value: value <= req),
        ("ReqRank", item["req_rank"], bot["rank"], lambda req, value: value >= req),
        ("ReqTitle", item["req_title"], bot["title"], lambda req, value: value >= req),
        ("ReqStr", item["req_str"], bot["strong"], lambda req, value: value >= req),
        ("ReqSta", item["req_sta"], bot["sta"], lambda req, value: value >= req),
        ("ReqDex", item["req_dex"], bot["dex"], lambda req, value: value >= req),
        ("ReqIntel", item["req_intel"], bot["intel"], lambda req, value: value >= req),
        ("ReqCha", item["req_cha"], bot["cha"], lambda req, value: value >= req),
    )
    return [
        "%s:%d>%d" % (name, required, value)
        for name, required, value, satisfied in checks
        if not satisfied(required, value)
    ]


def weight_report(bot, items):
    item_weight = 0
    max_weight_bonus = 0
    for slot, item_id, _duration, count in bot["slots"]:
        item = items[item_id]
        if slot == COSP_BAG1 or slot == COSP_BAG2:
            max_weight_bonus += item["duration"]
        else:
            item_weight += item["weight"] * count

    str_bonus = sum(items[item_id]["strb"] for _s, item_id, _d, _c in bot["equipment"])
    base = (bot["strong"] + str_bonus + bot["level"]) * 50 + max_weight_bonus
    rows = []
    for amount in WEIGHT_AMOUNTS:
        factor = 1 if amount <= 0 else amount // 100
        max_weight = base * factor
        rows.append((amount, max_weight, item_weight <= max_weight))
    return item_weight, base, rows


def bonus_sums(bot, items):
    totals = {"strb": 0, "stab": 0, "dexb": 0, "intelb": 0, "chab": 0, "maxhpb": 0, "maxmpb": 0, "ac": 0}
    for _slot, item_id, _duration, _count in bot["equipment"]:
        item = items[item_id]
        for key in totals:
            totals[key] += item[key]
    return totals


def write_report(bots, items, out):
    bots = sorted(bots, key=lambda bot: bot["name"])

    out.write("== A ==\n")
    fail_count = 0
    bots_with_fail = 0
    for bot in bots:
        bot_failed = False
        for slot, item_id, _duration, _count in bot["equipment"]:
            item = items[item_id]
            failures = equip_failures(bot, item)
            if failures:
                fail_count += 1
                bot_failed = True
                out.write("A %s slot=%d item=%d %s FAIL %s\n" % (
                    bot["name"], slot, item_id, ascii_text(item["name"]), ",".join(failures)))
            else:
                out.write("A %s slot=%d item=%d %s OK\n" % (
                    bot["name"], slot, item_id, ascii_text(item["name"])))
        if bot_failed:
            bots_with_fail += 1
    out.write("A_SUMMARY fail_count=%d bots_with_fail=%d\n" % (fail_count, bots_with_fail))

    out.write("== B ==\n")
    unique_ids = sorted({item_id for bot in bots for _s, item_id, _d, _c in bot["equipment"]})
    nonzero_race = 0
    nonzero_class = 0
    for item_id in unique_ids:
        item = items[item_id]
        if item["race"] != 0:
            nonzero_race += 1
        if item["class"] != 0:
            nonzero_class += 1
        out.write("B item=%d %s Race=%d Class=%d Kind=%d Slot=%d\n" % (
            item_id, ascii_text(item["name"]), item["race"], item["class"], item["kind"], item["slot"]))
    out.write("B_SUMMARY nonzero_race=%d nonzero_class=%d\n" % (nonzero_race, nonzero_class))

    out.write("== C ==\n")
    amount0_yes = 0
    amount100_yes = 0
    amount50_no = 0
    for bot in bots:
        item_weight, base, rows = weight_report(bot, items)
        fits_by_amount = {}
        for amount, max_weight, fits in rows:
            fits_by_amount[amount] = fits
            out.write("C %s item_weight=%d max_weight_base=%d amount=%d max_weight=%d fits=%s\n" % (
                bot["name"], item_weight, base, amount, max_weight, "yes" if fits else "no"))
        if fits_by_amount[0]:
            amount0_yes += 1
        if fits_by_amount[100]:
            amount100_yes += 1
        if not fits_by_amount[50]:
            amount50_no += 1
    out.write("C_SUMMARY amount=0 fits_yes=%d amount=100 fits_yes=%d amount=50 fits_no=%d\n" % (
        amount0_yes, amount100_yes, amount50_no))

    out.write("== D ==\n")
    for bot in bots:
        totals = bonus_sums(bot, items)
        out.write("D %s strB=%d staB=%d dexB=%d intelB=%d chaB=%d maxHpB=%d maxMpB=%d ac=%d\n" % (
            bot["name"], totals["strb"], totals["stab"], totals["dexb"], totals["intelb"],
            totals["chab"], totals["maxhpb"], totals["maxmpb"], totals["ac"]))


def make_item(item_id, name, weight, duration, ac, strb, maxhpb, req_str,
              req_level=0, req_level_max=80, kind=0, slot=0, race=0, item_class=0):
    return {
        "id": item_id, "name": name, "kind": kind, "slot": slot, "race": race, "class": item_class,
        "weight": weight, "duration": duration, "ac": ac, "countable": 0,
        "req_level": req_level, "req_level_max": req_level_max, "req_rank": 0, "req_title": 0,
        "req_str": req_str, "req_sta": 0, "req_dex": 0, "req_intel": 0, "req_cha": 0,
        "strb": strb, "stab": 0, "dexb": 0, "intelb": 0, "chab": 0, "maxhpb": maxhpb, "maxmpb": 0,
    }


def make_bot(name, strong, equipment):
    return {
        "name": name, "nation": 1, "race": 1, "class": 106, "level": 80, "rank": 0, "title": 0,
        "strong": strong, "sta": 100, "dex": 50, "intel": 50, "cha": 50,
        "slots": equipment,
        "equipment": [(s, i, d, c) for s, i, d, c in equipment if s < SLOT_MAX],
    }


def run_selftest():
    items = {
        1001: make_item(1001, "Selftest Sword", 10, 5, 10, 5, 20, 200),
        1002: make_item(1002, "Selftest Armor", 20, 5, 30, 0, 0, 100),
        1003: make_item(1003, "Selftest Boots", 1, 1, 1, 0, 0, 0),
    }
    bots = [
        make_bot("BotSelf_A", 150, [(0, 1001, 5, 1), (6, 1002, 5, 1)]),
        make_bot("BotSelf_B", 255, [(0, 1003, 1, 1)]),
    ]

    assert equip_failures(bots[0], items[1001]) == ["ReqStr:200>150"], equip_failures(bots[0], items[1001])
    assert equip_failures(bots[0], items[1002]) == [], equip_failures(bots[0], items[1002])

    item_weight, base, rows = weight_report(bots[0], items)
    assert item_weight == 30, item_weight
    assert base == (150 + 5 + 80) * 50, base
    fits_by_amount = {amount: fits for amount, _max, fits in rows}
    max_by_amount = {amount: max_weight for amount, max_weight, _fits in rows}
    assert max_by_amount[50] == 0, max_by_amount
    assert fits_by_amount[50] is False, fits_by_amount[50]
    assert max_by_amount[0] == base and max_by_amount[100] == base, max_by_amount
    assert max_by_amount[200] == base * 2 and max_by_amount[255] == base * 2, max_by_amount

    totals = bonus_sums(bots[0], items)
    assert totals["strb"] == 5, totals
    assert totals["ac"] == 40, totals
    assert totals["maxhpb"] == 20, totals

    buffer = io.StringIO()
    write_report(bots, items, buffer)
    text = buffer.getvalue()
    assert "A BotSelf_A slot=0 item=1001 Selftest Sword FAIL ReqStr:200>150" in text, text
    assert "A BotSelf_A slot=6 item=1002 Selftest Armor OK" in text, text
    assert "A_SUMMARY fail_count=1 bots_with_fail=1" in text, text
    assert "C BotSelf_A item_weight=30 max_weight_base=11750 amount=50 max_weight=0 fits=no" in text, text
    assert "D BotSelf_A strB=5 staB=0 dexB=0 intelB=0 chaB=0 maxHpB=20 maxMpB=0 ac=40" in text, text

    print("selftest OK")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--sqlcmd", default=DEFAULT_SQLCMD)
    parser.add_argument("--server", default=DEFAULT_SERVER)
    parser.add_argument("--db", default=DEFAULT_DB)
    args = parser.parse_args(argv)

    if args.selftest:
        return run_selftest()

    bot_rows = run_query(args.sqlcmd, args.server, args.db, BOT_QUERY)
    bots = load_bots(bot_rows)
    if not bots:
        sys.stderr.write("no bot rows found\n")
        return 1

    item_ids = sorted({item_id for bot in bots for _s, item_id, _d, _c in bot["slots"]})
    item_query = "SELECT %s FROM ITEM WHERE Num IN (%s)" % (
        ITEM_COLUMNS, ",".join(str(item_id) for item_id in item_ids))
    item_rows = run_query(args.sqlcmd, args.server, args.db, item_query)
    items = load_items(item_rows)

    missing = [item_id for item_id in item_ids if item_id not in items]
    if missing:
        sys.stderr.write("ITEM rows missing for: %s\n" % ",".join(str(item_id) for item_id in missing))
        return 1

    write_report(bots, items, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
