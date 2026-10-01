#!/usr/bin/env python3
"""Models level 80 bot derived stats and physical damage from server formulas.

Reads the bot rows written by db/002_bot_characters.sql, the ITEM table, the
COEFFICIENT rows of the six bot classes and the warrior Type1 skills, then
prints four sections:

    S  derived stats, item-less (docs/04 section 4 reference) and with items
    R  normal attack (R) damage for the 6 Karus attackers x 6 defender profiles
    K  warrior Type1 attack skills (tree 5) for WP and WG x 6 defenders
    P  item-less comparison against the docs/04 section 4 values

All arithmetic follows the C++ integer/float semantics (32-bit float rounding,
truncating casts, integer divisions); there is no randomness.

Usage:
    python3 tools/stat-model.py [--sqlcmd PATH] [--server INSTANCE] [--db DB]
    python3 tools/stat-model.py --selftest
"""

import argparse
import io
import struct
import subprocess
import sys

DEFAULT_SQLCMD = "/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
DEFAULT_SERVER = ".\\SQLEXPRESS"
DEFAULT_DB = "FDP_kn_online"

RIGHTHAND = 6
LEFTHAND = 8
SLOT_MAX = 14
MAX_PLAYER_HP = 14000
MAX_DAMAGE = 32000

WEAPON_DAGGER = 1
WEAPON_SWORD = 2
WEAPON_AXE = 3
WEAPON_MACE = 4
WEAPON_SPEAR = 5
WEAPON_SHIELD = 6
WEAPON_BOW = 7
WEAPON_LONGBOW = 8
WEAPON_LAUNCHER = 10
WEAPON_STAFF = 11
WEAPON_MACE2 = 18

PROFILES = ("WP", "WG", "PHD", "PHB", "MF", "MI")

DOC_VALUES = {
    "WP": {"max_hp": 4458, "max_mp": 4438, "total_hit": 1766},
    "WG": {"max_hp": 4458, "max_mp": 4438, "total_hit": 1036},
    "PHD": {"max_hp": 2636, "max_mp": 5696},
    "PHB": {"max_hp": 2636, "max_mp": 5696},
    "MF": {"max_hp": 896, "max_mp": 5286},
    "MI": {"max_hp": 1582, "max_mp": 5286},
}

BOT_QUERY = "SELECT RTRIM(strUserID), Nation, Race, [Class], Level, [Rank], Title, Strong, Sta, Dex, Intel, Cha, CONVERT(varchar(1200), strItem, 2), CONVERT(varchar(20), CAST(strSkill AS varbinary(10)), 2) FROM USERDATA WHERE strUserID LIKE 'Bot%' ORDER BY strUserID"

ITEM_QUERY_COLUMNS = (
    "Num, RTRIM(strName), Kind, Slot, Race, [Class], Damage, Delay, Range, Duration, Ac, "
    "Hitrate, Evasionrate, FireDamage, IceDamage, LightningDamage, PoisonDamage, HPDrain, "
    "MPDamage, MPDrain, MirrorDamage, StrB, StaB, DexB, IntelB, ChaB, MaxHpB, MaxMpB, "
    "SwordAc, AxeAc, MaceAc, SpearAc, DaggerAc, BowAc, FireR, ColdR, LightningR, MagicR, "
    "PoisonR, CurseR"
)

COEFF_QUERY = (
    "SELECT sClass, HP, MP, SP, AC, Hitrate, Evasionrate, ShortSword, Sword, Axe, Club, "
    "Spear, Pole, Staff, Bow FROM COEFFICIENT WHERE sClass IN (106,110,112,206,210,212)"
)

MAGIC_QUERY = (
    "SELECT m.MagicNum, RTRIM(ISNULL(t.Name, '')), m.Skill, m.SkillLevel, m.Type1, t.Type, "
    "t.HitRate, t.Hit FROM MAGIC m JOIN MAGIC_TYPE1 t ON t.iNum = m.MagicNum "
    "WHERE m.Skill IN (1065, 2065) AND m.Type1 = 1 ORDER BY m.MagicNum"
)


def f32(value):
    """Rounds a value to the nearest 32-bit float (C++ float)."""
    return struct.unpack("<f", struct.pack("<f", value))[0]


def mul32(*values):
    """Multiplies left to right, rounding to float32 after every step."""
    result = f32(values[0])
    for value in values[1:]:
        result = f32(result * value)
    return result


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
    if len(data) != 73 * 8:
        raise ValueError("strItem is %d bytes, expected %d" % (len(data), 73 * 8))
    slots = []
    for slot in range(73):
        item_id, duration, count = struct.unpack("<IhH", data[slot * 8:(slot + 1) * 8])
        if item_id != 0:
            slots.append((slot, item_id, duration, count))
    return slots


def load_bots(rows):
    bots = []
    for row in rows:
        if len(row) != 14:
            raise ValueError("unexpected bot row with %d fields" % len(row))
        skill_bytes = list(bytes.fromhex(row[13]))
        slots = parse_slots(row[12])
        bots.append({
            "name": row[0],
            "nation": int(row[1]),
            "race": int(row[2]),
            "class": int(row[3]),
            "level": int(row[4]),
            "strong": int(row[7]),
            "sta": int(row[8]),
            "dex": int(row[9]),
            "int": int(row[10]),
            "cha": int(row[11]),
            "skill": skill_bytes,
            "slots": slots,
            "equipment": [(s, i, d, c) for s, i, d, c in slots if s < SLOT_MAX],
        })
    return bots


def load_items(rows):
    items = {}
    for row in rows:
        if len(row) != 40:
            raise ValueError("unexpected ITEM row with %d fields" % len(row))
        item_id = int(row[0])
        v = [int(value) for value in row[2:]]
        items[item_id] = {
            "id": item_id, "name": row[1],
            "kind": v[0], "slot": v[1], "race": v[2], "class": v[3],
            "damage": v[4], "delay": v[5], "range": v[6], "duration": v[7], "ac": v[8],
            "hitrate": v[9], "evarate": v[10],
            "fire": v[11], "ice": v[12], "lightning": v[13], "poison": v[14],
            "hpdrain": v[15], "mpdamage": v[16], "mpdrain": v[17], "mirror": v[18],
            "strb": v[19], "stab": v[20], "dexb": v[21], "intelb": v[22], "chab": v[23],
            "maxhpb": v[24], "maxmpb": v[25],
            "swordac": v[26], "axeac": v[27], "maceac": v[28], "spearac": v[29],
            "daggerac": v[30], "bowac": v[31],
        }
    return items


def load_coefficients(rows):
    coeffs = {}
    for row in rows:
        coeffs[int(row[0])] = {
            "hp": float(row[1]), "mp": float(row[2]), "sp": float(row[3]), "ac": float(row[4]),
            "hitrate": float(row[5]), "evasionrate": float(row[6]),
            "shortsword": float(row[7]), "sword": float(row[8]), "axe": float(row[9]),
            "club": float(row[10]), "spear": float(row[11]), "pole": float(row[12]),
            "staff": float(row[13]), "bow": float(row[14]),
        }
    return coeffs


def load_magics(rows):
    magics = []
    for row in rows:
        magics.append({
            "num": int(row[0]),
            "name": row[1],
            "skill": int(row[2]),
            "level": int(row[3]),
            "hit_type": int(row[5]),
            "hit_rate": int(row[6]),
            "hit": int(row[7]),
        })
    return magics


def zero_bonuses():
    return {
        "ac": 0, "maxhp": 0, "maxmp": 0, "str": 0, "sta": 0, "dex": 0, "int": 0, "cha": 0,
        "hitrate": 100, "evasion": 100,
        "swordR": 0, "axeR": 0, "maceR": 0, "spearR": 0, "daggerR": 0, "bowR": 0,
    }


def item_bonuses(equipment, items):
    totals = zero_bonuses()
    totals["hitrate"] = 100
    totals["evasion"] = 100
    for _slot, item_id, duration, _count in equipment:
        item = items[item_id]
        ac = item["ac"]
        if duration == 0:
            ac //= 10
        totals["ac"] += ac
        totals["maxhp"] += item["maxhpb"]
        totals["maxmp"] += item["maxmpb"]
        totals["str"] += item["strb"]
        totals["sta"] += item["stab"]
        totals["dex"] += item["dexb"]
        totals["int"] += item["intelb"]
        totals["cha"] += item["chab"]
        totals["hitrate"] += item["hitrate"]
        totals["evasion"] += item["evarate"]
        totals["swordR"] += item["swordac"]
        totals["axeR"] += item["axeac"]
        totals["maceR"] += item["maceac"]
        totals["spearR"] += item["spearac"]
        totals["daggerR"] += item["daggerac"]
        totals["bowR"] += item["bowac"]
    return totals


def find_equipment(equipment, slot):
    for entry in equipment:
        if entry[0] == slot:
            return entry
    return None


def weapon_setup(equipment, items, coeff):
    """Returns (sItemDamage, hitcoefficient) per SetUserAbility."""
    right = find_equipment(equipment, RIGHTHAND)
    left = find_equipment(equipment, LEFTHAND)
    s_damage = 0
    hit_coeff = 0.0

    if right is not None:
        right_item = items[right[1]]
        group = right_item["kind"] // 10
        key = {WEAPON_DAGGER: "shortsword", WEAPON_SWORD: "sword", WEAPON_AXE: "axe",
               WEAPON_MACE: "club", WEAPON_MACE2: "club", WEAPON_SPEAR: "spear",
               WEAPON_BOW: "bow", WEAPON_LONGBOW: "bow", WEAPON_LAUNCHER: "bow",
               WEAPON_STAFF: "staff"}.get(group)
        if key is not None:
            hit_coeff = coeff[key]
        damage = right_item["damage"]
        if right[2] == 0:
            damage //= 2
        s_damage += damage

    if left is not None:
        left_item = items[left[1]]
        group = left_item["kind"] // 10
        damage = left_item["damage"]
        if group in (WEAPON_BOW, WEAPON_LONGBOW):
            hit_coeff = coeff["bow"]
            if left[2] == 0:
                damage //= 2
            s_damage = damage
        else:
            if left[2] == 0:
                s_damage += (damage // 2) // 2
            else:
                s_damage += damage // 2

    if s_damage < 3:
        s_damage = 3
    return s_damage, hit_coeff


def class_group(klass):
    if klass in (106, 206):
        return "warrior"
    if klass in (112, 212):
        return "priest"
    return "mage"


def has_shield(equipment, items):
    left = find_equipment(equipment, LEFTHAND)
    return left is not None and items[left[1]]["kind"] // 10 == WEAPON_SHIELD


def derived_stats(bot, items, coeff, use_items):
    equipment = bot["equipment"]
    bonuses = item_bonuses(equipment, items) if use_items else zero_bonuses()
    strength = bot["strong"] + bonuses["str"]
    stamina = bot["sta"] + bonuses["sta"]
    dexterity = bot["dex"] + bonuses["dex"]
    intel = bot["int"] + bonuses["int"]

    s_damage, hit_coeff = weapon_setup(equipment, items, coeff)

    base_ap = bot["strong"] - 150 if bot["strong"] > 150 else 0
    if bot["strong"] == 160:
        base_ap -= 1
    ap_stat = strength
    additional_ap = 3 + base_ap

    group = class_group(bot["class"])
    if group in ("warrior", "priest"):
        core = int(f32(mul32(0.010, s_damage, ap_stat + 40)
                       + mul32(hit_coeff, s_damage, bot["level"], ap_stat))) & 0xFFFF
    else:
        core = int(f32(mul32(0.005, s_damage, ap_stat + 40)
                       + mul32(hit_coeff, s_damage, bot["level"]))) & 0xFFFF
    total_hit = (core + additional_ap) * 100 // 100

    total_ac = int(coeff["ac"] * (bot["level"] + bonuses["ac"]))
    if group == "warrior":
        points = bot["skill"][6]
        defense = 0
        if 5 <= points <= 14:
            defense = 20
        elif 15 <= points <= 34:
            defense = 30
        elif 35 <= points <= 54:
            defense = 40
        elif 55 <= points <= 69:
            defense = 50
        elif 70 <= points <= 80:
            defense = 50  # level 70 quest (event 51) not completed for bots
        if not has_shield(equipment, items):
            defense //= 2
        total_ac += defense * total_ac // 100
    if bot["sta"] > 100:
        total_ac += bot["sta"] - 100

    hitrate = (1 + coeff["hitrate"] * bot["level"] * dexterity) * bonuses["hitrate"] / 100.0
    evasion = (1 + coeff["evasionrate"] * bot["level"] * dexterity) * bonuses["evasion"] / 100.0

    hp_core = mul32(coeff["hp"], bot["level"], bot["level"], stamina)
    max_hp = int(hp_core + 0.1 * (bot["level"] * stamina) + stamina // 5 + bonuses["maxhp"] + 20)
    if max_hp > MAX_PLAYER_HP:
        max_hp = MAX_PLAYER_HP

    if coeff["mp"] != 0:
        int_total = intel + 30
        mp_core = mul32(coeff["mp"], bot["level"], bot["level"], int_total)
        mp_term = mul32(f32(0.1), bot["level"], 2, int_total)
        max_mp = int(mp_core + mp_term + int_total // 5 + bonuses["maxmp"] + 20)
    else:
        mp_core = mul32(coeff["sp"], bot["level"], bot["level"], stamina)
        mp_term = mul32(f32(0.1), bot["level"], stamina)
        max_mp = int(mp_core + mp_term + stamina // 5 + bonuses["maxmp"])

    return {
        "max_hp": max_hp, "max_mp": max_mp, "total_hit": total_hit, "total_ac": total_ac,
        "hitrate": hitrate, "evasion": evasion, "bonuses": bonuses,
    }


def hit_success_probability(rate):
    """Success probability of Unit::GetHitRate (ranges in 0..10000)."""
    if rate >= 5.0:
        return 0.98
    if rate >= 3.0:
        return 0.96
    if rate >= 2.0:
        return 0.94
    if rate >= 1.25:
        return 0.92
    if rate >= 0.8:
        return 0.90
    if rate >= 0.5:
        return 0.80
    if rate >= 0.33:
        return 0.70
    if rate >= 0.2:
        return 0.60
    return 0.50


def normal_hit_damage(hit_b, random_value):
    """(short)(0.85f * B + 0.3f * r), the part before item/AC steps."""
    return int(f32(f32(f32(0.85) * hit_b) + f32(f32(0.3) * random_value)))


def apply_physical_defense(damage, attacker_equipment, items, defender_stats):
    """Unit::GetACDamage (left then right hand) followed by the /2 player step."""
    for slot in (LEFTHAND, RIGHTHAND):
        entry = find_equipment(attacker_equipment, slot)
        if entry is None:
            continue
        group = items[entry[1]]["kind"] // 10
        resist = None
        if group == WEAPON_DAGGER:
            resist = defender_stats["bonuses"]["daggerR"]
        elif group == WEAPON_SWORD:
            resist = defender_stats["bonuses"]["swordR"]
        elif group == WEAPON_AXE:
            resist = defender_stats["bonuses"]["axeR"]
        elif group in (WEAPON_MACE, WEAPON_MACE2):
            resist = defender_stats["bonuses"]["maceR"]
        elif group == WEAPON_SPEAR:
            resist = defender_stats["bonuses"]["spearR"]
        elif group in (WEAPON_BOW, WEAPON_LONGBOW):
            resist = defender_stats["bonuses"]["bowR"]
        if resist is not None:
            damage -= damage * resist // 200
    damage //= 2
    if damage > MAX_DAMAGE:
        damage = MAX_DAMAGE
    return damage


def base_hit_b(attacker_stats, defender_stats):
    temp_ap = attacker_stats["total_hit"] * 100
    return (temp_ap * 200 // 100) // (defender_stats["total_ac"] + 240)


def r_attack(attacker, attacker_stats, items, defender_stats):
    hit_b = base_hit_b(attacker_stats, defender_stats)
    hit_prob = hit_success_probability(attacker_stats["hitrate"] / defender_stats["evasion"])
    damages = []
    for random_value in range(hit_b + 1):
        raw = normal_hit_damage(hit_b, random_value)
        damages.append(apply_physical_defense(raw, attacker["equipment"], items, defender_stats))
    average = sum(damages) / len(damages)
    return {
        "base": hit_b,
        "hit_prob": hit_prob,
        "avg": average,
        "min": min(damages),
        "max": max(damages),
        "expected": hit_prob * average,
    }


def k_attack(attacker, attacker_stats, items, skill, defender_stats):
    hit_b = base_hit_b(attacker_stats, defender_stats)
    temp_hit = int(f32(hit_b * f32(skill["hit"] / 100.0)))
    damages = []
    for random_value in range(temp_hit + 1):
        raw = int(f32(f32(temp_hit + f32(f32(0.3) * random_value)) + f32(0.99)))
        damages.append(apply_physical_defense(raw, attacker["equipment"], items, defender_stats))
    if skill["hit_type"]:
        hit_prob = skill["hit_rate"] / 101.0
    else:
        rate = f32(f32(attacker_stats["hitrate"] / defender_stats["evasion"])
                   * f32(skill["hit_rate"] / 100.0))
        hit_prob = hit_success_probability(rate)
    return {"base": hit_b, "hit_prob": hit_prob, "avg": sum(damages) / len(damages)}


def profile_of(name):
    return name[3:name.index("_")]


def write_report(bots, items, coeffs, magics, out):
    bots_sorted = sorted(bots, key=lambda bot: bot["name"])
    stats = {}
    for bot in bots_sorted:
        coeff = coeffs[bot["class"]]
        stats[bot["name"]] = {
            False: derived_stats(bot, items, coeff, False),
            True: derived_stats(bot, items, coeff, True),
        }

    out.write("== S ==\n")
    for bot in bots_sorted:
        for use_items in (False, True):
            s = stats[bot["name"]][use_items]
            out.write("S %s items=%s max_hp=%d max_mp=%d total_hit=%d total_ac=%d "
                      "hitrate=%.3f evasion=%.3f\n" % (
                          bot["name"], "yes" if use_items else "no", s["max_hp"], s["max_mp"],
                          s["total_hit"], s["total_ac"], s["hitrate"], s["evasion"]))

    defenders = [bot for bot in bots_sorted if bot["nation"] == 1]

    out.write("== R ==\n")
    for attacker in defenders:
        attacker_stats = stats[attacker["name"]][True]
        for defender in defenders:
            defender_stats = stats[defender["name"]][True]
            result = r_attack(attacker, attacker_stats, items, defender_stats)
            out.write("R %s->%s hit_prob=%.3f base=%d dmg_avg=%.1f dmg_min=%d dmg_max=%d exp=%.1f\n" % (
                attacker["name"], defender["name"], result["hit_prob"], result["base"],
                result["avg"], result["min"], result["max"], result["expected"]))

    out.write("== K ==\n")
    for attacker in defenders:
        profile = profile_of(attacker["name"])
        if profile not in ("WP", "WG"):
            continue
        points = attacker["skill"][5]
        skill_number = attacker["class"] * 10 + 5
        attacker_stats = stats[attacker["name"]][True]
        for skill in magics:
            if skill["skill"] != skill_number or skill["level"] > points:
                continue
            for defender in defenders:
                defender_stats = stats[defender["name"]][True]
                result = k_attack(attacker, attacker_stats, items, skill, defender_stats)
                out.write("K %s->%s skill=%d %s hit_pct=%.4f sHit=%d base=%d dmg_avg=%.1f\n" % (
                    attacker["name"], defender["name"], skill["num"],
                    ascii_text(skill["name"]), result["hit_prob"], skill["hit"], result["base"],
                    result["avg"]))

    out.write("== P ==\n")
    by_profile = {}
    for bot in defenders:
        by_profile[profile_of(bot["name"])] = bot
    for profile in PROFILES:
        bot = by_profile.get(profile)
        if bot is None:
            continue
        s = stats[bot["name"]][False]
        doc = DOC_VALUES[profile]
        out.write("P %s max_hp calc=%d doc=%d diff=%d\n" % (
            profile, s["max_hp"], doc["max_hp"], s["max_hp"] - doc["max_hp"]))
        out.write("P %s max_mp calc=%d doc=%d diff=%d\n" % (
            profile, s["max_mp"], doc["max_mp"], s["max_mp"] - doc["max_mp"]))
        if "total_hit" in doc:
            out.write("P %s total_hit calc=%d doc=%d diff=%d\n" % (
                profile, s["total_hit"], doc["total_hit"], s["total_hit"] - doc["total_hit"]))


def make_item(item_id, name, kind=0, damage=0, duration=0, ac=0, strb=0, maxhpb=0, maxmpb=0):
    item = {"id": item_id, "name": name, "kind": kind, "damage": damage, "duration": duration,
            "ac": ac, "strb": strb, "maxhpb": maxhpb, "maxmpb": maxmpb}
    for key in ("slot", "race", "class", "delay", "range", "hitrate", "evarate", "fire", "ice",
                "lightning", "poison", "hpdrain", "mpdamage", "mpdrain", "mirror", "stab",
                "dexb", "intelb", "chab", "swordac", "axeac", "maceac", "spearac", "daggerac",
                "bowac"):
        item[key] = 0
    return item


def run_selftest():
    coeff = {
        "hp": f32(0.003), "mp": 0.0, "sp": f32(0.003), "ac": 1.0,
        "hitrate": f32(0.05), "evasionrate": f32(0.018),
        "shortsword": f32(0.0002), "sword": f32(0.00032), "axe": f32(0.00032),
        "club": f32(0.00032), "spear": f32(0.00032), "pole": f32(0.00032),
        "staff": f32(0.0001), "bow": f32(0.0001),
    }
    items = {
        9001: make_item(9001, "Selftest Raptor", kind=52, damage=175, duration=14000),
        9002: make_item(9002, "Selftest Armor", kind=210, duration=21625, ac=175, strb=29),
    }
    bot = {
        "name": "BotWP_K", "nation": 1, "race": 1, "class": 106, "level": 80,
        "strong": 255, "sta": 162, "dex": 60, "int": 50, "cha": 50,
        "skill": [0, 0, 0, 0, 0, 70, 0, 52, 20, 0],
        "slots": [(6, 9001, 14000, 1), (4, 9002, 21625, 1)],
        "equipment": [(6, 9001, 14000, 1), (4, 9002, 21625, 1)],
    }

    with_items = derived_stats(bot, items, coeff, True)
    without_items = derived_stats(bot, items, coeff, False)

    assert with_items["total_hit"] == 1947, with_items["total_hit"]
    assert without_items["total_hit"] == 1766, without_items["total_hit"]
    assert without_items["max_hp"] == 4458, without_items["max_hp"]

    hit_b = base_hit_b({"total_hit": 1947}, {"total_ac": 1488})
    assert hit_b == 225, hit_b

    for rate, expected in ((5.0, 0.98), (3.0, 0.96), (2.0, 0.94), (1.25, 0.92), (0.8, 0.90),
                           (0.5, 0.80), (0.33, 0.70), (0.2, 0.60), (0.1, 0.50), (0.0, 0.50)):
        assert hit_success_probability(rate) == expected, (rate, hit_success_probability(rate))

    assert normal_hit_damage(225, 0) == 191, normal_hit_damage(225, 0)
    assert normal_hit_damage(225, 225) == 258, normal_hit_damage(225, 225)
    average = sum(normal_hit_damage(225, r) for r in range(226)) / 226.0
    assert average == 224.5, average

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
        ITEM_QUERY_COLUMNS, ",".join(str(item_id) for item_id in item_ids))
    items = load_items(run_query(args.sqlcmd, args.server, args.db, item_query))

    coeffs = load_coefficients(run_query(args.sqlcmd, args.server, args.db, COEFF_QUERY))
    for bot in bots:
        if bot["class"] not in coeffs:
            sys.stderr.write("COEFFICIENT row missing for class %d\n" % bot["class"])
            return 1

    magics = load_magics(run_query(args.sqlcmd, args.server, args.db, MAGIC_QUERY))

    write_report(bots, items, coeffs, magics, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
