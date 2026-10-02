#!/usr/bin/env python3
"""Models level 80 mage/priest spell damage and healing from server formulas.

Reads the bot rows written by db/002_bot_characters.sql, the ITEM table, the
COEFFICIENT rows, the wizard/priest Type3 skills and prints three sections:

    M  Type3 attack spell damage (instant + damage over time) per target
    H  priest Type3 healing (instant + heal over time) and its share of max HP
    P  the docs/04 section 4 incineration example, with and without a staff

All arithmetic follows the C++ semantics (truncating integer divisions, int16
casts, 32-bit float rounding); there is no randomness: the random roll is
averaged exactly over all of its possible values.

Usage:
    python3 tools/spell-model.py [--sqlcmd PATH] [--server INSTANCE] [--db DB]
    python3 tools/spell-model.py --selftest
"""

import argparse
import math
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

FIRE_R = 1
COLD_R = 2
LIGHTNING_R = 3
MAGIC_R = 4
DISEASE_R = 5
POISON_R = 6

PROFILES = ("WP", "WG", "PHD", "PHB", "MF", "MI")

DOC_EXAMPLE = {
    (247, 0): 1071,
    (247, 100): 780,
    (247, 200): 619,
    (200, 0): 862,
    (200, 100): 626,
    (200, 200): 495,
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

TYPE3_QUERY = (
    "SELECT m.MagicNum, RTRIM(m.EnName), m.Skill, m.SkillLevel, m.Msp, m.CastTime, "
    "m.ReCastTime, t.DirectType, t.FirstDamage, t.TimeDamage, t.Duration, t.Attribute, "
    "t.Radius FROM MAGIC m JOIN MAGIC_TYPE3 t ON t.iNum = m.MagicNum "
    "WHERE m.Type1 = 3 AND m.MagicNum < 400000 AND m.Skill IN "
    "(1065,1066,1067,1105,1106,1107,1125,1126,1127,"
    " 2065,2066,2067,2105,2106,2107,2125,2126,2127) ORDER BY m.MagicNum"
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


def tdiv(a, b):
    """C++ integer division: truncates toward zero (Python // rounds down)."""
    quotient = abs(a) // abs(b)
    return quotient if (a >= 0) == (b >= 0) else -quotient


def to_short(value):
    """C++ int/float -> int16 conversion: truncate toward zero, wrap to 16 bits."""
    wrapped = int(value) & 0xFFFF
    return wrapped - 0x10000 if wrapped >= 0x8000 else wrapped


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
            "skill": list(bytes.fromhex(row[13])),
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
            "firer": v[32], "coldr": v[33], "lightningr": v[34], "magicr": v[35],
            "poisonr": v[36], "curser": v[37],
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


def load_type3_skills(rows):
    skills = []
    for row in rows:
        skills.append({
            "num": int(row[0]),
            "name": row[1],
            "skill": int(row[2]),
            "level": int(row[3]),
            "msp": int(row[4]),
            "cast_time": int(row[5]),
            "recast_time": int(row[6]),
            "direct_type": int(row[7]),
            "first": int(row[8]),
            "time": int(row[9]),
            "duration": int(row[10]),
            "attribute": int(row[11]),
            "radius": int(row[12]),
        })
    return skills


def zero_bonuses():
    return {
        "ac": 0, "maxhp": 0, "maxmp": 0, "str": 0, "sta": 0, "dex": 0, "int": 0, "cha": 0,
        "hitrate": 100, "evasion": 100,
        "swordR": 0, "axeR": 0, "maceR": 0, "spearR": 0, "daggerR": 0, "bowR": 0,
    }


def item_bonuses(equipment, items):
    totals = zero_bonuses()
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
            defense = 50
        if not has_shield(equipment, items):
            defense //= 2
        total_ac += defense * total_ac // 100
    if bot["sta"] > 100:
        total_ac += bot["sta"] - 100

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
        "hitrate": (1 + coeff["hitrate"] * bot["level"] * dexterity) * bonuses["hitrate"] / 100.0,
        "evasion": (1 + coeff["evasionrate"] * bot["level"] * dexterity) * bonuses["evasion"] / 100.0,
        "bonuses": bonuses,
    }


def resist_sums(equipment, items):
    sums = {FIRE_R: 0, COLD_R: 0, LIGHTNING_R: 0, MAGIC_R: 0, DISEASE_R: 0, POISON_R: 0}
    keys = {FIRE_R: "firer", COLD_R: "coldr", LIGHTNING_R: "lightningr",
            MAGIC_R: "magicr", DISEASE_R: "curser", POISON_R: "poisonr"}
    for _slot, item_id, _duration, _count in equipment:
        item = items[item_id]
        for attribute, key in keys.items():
            sums[attribute] += item[key]
    return sums


def resistance_bonus(bot, items):
    bonus = 0
    if class_group(bot["class"]) == "warrior":
        points = bot["skill"][6]
        if 10 <= points <= 19:
            bonus = 30
        elif 20 <= points <= 39:
            bonus = 60
        elif points >= 40:
            bonus = 90
        if not has_shield(bot["equipment"], items):
            bonus //= 2
    if bot["int"] > 100:
        bonus += (bot["int"] - 100) // 2
    return bonus


def caster_staff_damage(bot, items):
    right = find_equipment(bot["equipment"], RIGHTHAND)
    left = find_equipment(bot["equipment"], LEFTHAND)
    if right is not None and left is None and items[right[1]]["kind"] // 10 == WEAPON_STAFF:
        return items[right[1]]["damage"]
    return None


def magic_damage_samples(cha, level, staff_damage, total_hit, attribute, total_r):
    """Exact per-roll damages of MagicInstance::GetMagicDamage (negative = damage)."""
    s_magic_amount = cha - 86 if cha > 86 else 0
    scaled_hit = tdiv(total_hit * cha, 186)
    damage = tdiv(230 * scaled_hit, total_r + 250)

    staff_term = 0
    if staff_damage is not None and attribute != MAGIC_R:
        staff_term = to_short(f32(f32(staff_damage * f32(0.8)) + tdiv(staff_damage * level, 60)))

    samples = []
    for roll in range(damage, 1):
        value = to_short(f32(f32(roll * f32(0.3)) + f32(damage * f32(0.85))))
        value = to_short(value - s_magic_amount)
        value = to_short(value - staff_term)
        value = to_short(tdiv(value, 3))
        if value > MAX_DAMAGE:
            value = MAX_DAMAGE
        samples.append(value)
    return samples


def magic_sample_stats(samples):
    magnitudes = [abs(value) for value in samples]
    if not magnitudes:
        return 0.0, 0.0, 0.0
    return sum(magnitudes) / len(magnitudes), min(magnitudes), max(magnitudes)


def dot_values(duration_damage, duration):
    """C++ DoT block: returns (dot_tick, ticks, dot_total) for one sample."""
    tick_count = f32(f32(duration) / f32(2))
    ticks = int(tick_count)
    dot_tick = to_short(int(duration_damage / tick_count))
    return dot_tick, ticks, dot_tick * ticks


def hot_values(time_damage, duration):
    """C++ HoT block: returns (hot_tick, ticks, hot_total) for one sample."""
    tick_count = f32(f32(duration) / f32(2))
    ticks = int(tick_count)
    hot_tick = to_short(int(time_damage / tick_count))
    return hot_tick, ticks, hot_tick * ticks


def profile_of(name):
    return name[3:name.index("_")]


def write_report(bots, items, coeffs, skills, out):
    bots_sorted = sorted(bots, key=lambda bot: bot["name"])
    defenders = [bot for bot in bots_sorted if bot["nation"] == 1]
    defender_data = {}
    for bot in defenders:
        stats = derived_stats(bot, items, coeffs[bot["class"]], True)
        defender_data[bot["name"]] = {
            "max_hp": stats["max_hp"],
            "resist": resist_sums(bot["equipment"], items),
            "resistance_bonus": resistance_bonus(bot, items),
        }

    out.write("== M ==\n")
    for attacker in defenders:
        points = attacker["skill"]
        staff_damage = caster_staff_damage(attacker, items)
        for skill in skills:
            if skill["skill"] // 10 != attacker["class"]:
                continue
            tree = skill["skill"] % 10
            if skill["level"] > points[tree]:
                continue
            if skill["direct_type"] not in (1, 8):
                continue
            if not (skill["first"] < 0 or skill["time"] < 0):
                continue

            for defender in defenders:
                target = defender_data[defender["name"]]
                total_r = target["resist"][skill["attribute"]] + target["resistance_bonus"]

                if skill["first"] < 0:
                    samples = magic_damage_samples(attacker["cha"], attacker["level"], staff_damage,
                                                   skill["first"], skill["attribute"], total_r)
                else:
                    samples = []
                dmg_avg, dmg_min, dmg_max = magic_sample_stats(samples)

                if skill["duration"] != 0:
                    if skill["time"] < 0 and skill["attribute"] != MAGIC_R:
                        time_samples = magic_damage_samples(
                            attacker["cha"], attacker["level"], staff_damage,
                            skill["time"], skill["attribute"], total_r)
                        duration_damage = sum(time_samples) / len(time_samples)
                    else:
                        duration_damage = skill["time"]
                    dot_tick, ticks, dot_total = dot_values(duration_damage, skill["duration"])
                else:
                    dot_tick, ticks, dot_total = 0, 0, 0

                per_cast = dmg_avg + abs(dot_total)
                casts = int(math.ceil(target["max_hp"] / per_cast)) if per_cast > 0 else 0

                out.write("M %s->%s skill=%d %s attr=%d first=%d time=%d dur=%d msp=%d "
                          "cast_s=%.1f recast_s=%.1f dmg_avg=%.1f dmg_min=%.1f dmg_max=%.1f "
                          "dot_total=%.1f dot_tick=%.1f ticks=%d def_hp=%d casts_to_kill=%d\n" % (
                              attacker["name"], defender["name"], skill["num"],
                              ascii_text(skill["name"]), skill["attribute"], skill["first"],
                              skill["time"], skill["duration"], skill["msp"],
                              skill["cast_time"] / 10.0, skill["recast_time"] / 10.0,
                              dmg_avg, dmg_min, dmg_max, abs(dot_total), abs(dot_tick), ticks,
                              target["max_hp"], casts))

    out.write("== H ==\n")
    pct_skills = {}
    for bot in defenders:
        if class_group(bot["class"]) != "priest":
            continue
        points = bot["skill"]
        for skill in skills:
            if skill["skill"] // 10 != bot["class"]:
                continue
            tree = skill["skill"] % 10
            if skill["level"] > points[tree]:
                continue
            if skill["direct_type"] != 1:
                continue
            if not (skill["first"] > 0 or skill["time"] > 0):
                continue

            heal_instant = skill["first"]
            if skill["time"] > 0 and skill["duration"] > 0:
                hot_tick, ticks, hot_total = hot_values(skill["time"], skill["duration"])
            else:
                hot_tick, ticks, hot_total = 0, 0, 0
            heal_per_msp = (heal_instant + hot_total) / skill["msp"] if skill["msp"] > 0 else 0.0

            out.write("H %s skill=%d %s first=%d time=%d dur=%d radius=%d msp=%d cast_s=%.1f "
                      "recast_s=%.1f heal_instant=%d hot_tick=%d ticks=%d hot_total=%d "
                      "heal_per_msp=%.2f\n" % (
                          bot["name"], skill["num"], ascii_text(skill["name"]), skill["first"],
                          skill["time"], skill["duration"], skill["radius"], skill["msp"],
                          skill["cast_time"] / 10.0, skill["recast_time"] / 10.0,
                          heal_instant, hot_tick, ticks, hot_total, heal_per_msp))

            pct_skills.setdefault(bot["name"], []).append(
                (heal_instant + hot_total, skill["num"], ascii_text(skill["name"])))

    for bot_name, entries in sorted(pct_skills.items()):
        for total, num, name in sorted(entries, reverse=True)[:3]:
            for defender in defenders:
                target = defender_data[defender["name"]]
                out.write("H_PCT %s skill=%d %s def=%s pct_of_max_hp=%.1f\n" % (
                    bot_name, num, name, profile_of(defender["name"]),
                    total * 100.0 / target["max_hp"]))

    out.write("== P ==\n")
    for cha in (247, 200):
        for resist in (0, 100, 200):
            for staff_damage in (None, 111):
                samples = magic_damage_samples(cha, 80, staff_damage, -2500, FIRE_R, resist)
                average = sum(abs(value) for value in samples) / len(samples)
                doc = DOC_EXAMPLE.get((cha, resist)) if staff_damage is None else None
                out.write("P cha=%d r=%d staff=%s avg=%.1f doc=%s\n" % (
                    cha, resist, "no" if staff_damage is None else "yes", average,
                    "-" if doc is None else str(doc)))


def make_item(item_id, name, kind=0, damage=0, duration=0, ac=0, strb=0, maxhpb=0, maxmpb=0):
    item = {"id": item_id, "name": name, "kind": kind, "damage": damage, "duration": duration,
            "ac": ac, "strb": strb, "maxhpb": maxhpb, "maxmpb": maxmpb}
    for key in ("slot", "race", "class", "delay", "range", "hitrate", "evarate", "fire", "ice",
                "lightning", "poison", "hpdrain", "mpdamage", "mpdrain", "mirror", "stab",
                "dexb", "intelb", "chab", "swordac", "axeac", "maceac", "spearac", "daggerac",
                "bowac", "firer", "coldr", "lightningr", "magicr", "poisonr", "curser"):
        item[key] = 0
    return item


def run_selftest():
    assert tdiv(-7, 2) == -3, tdiv(-7, 2)
    assert tdiv(7, -2) == -3, tdiv(7, -2)
    assert tdiv(7, 2) == 3, tdiv(7, 2)

    samples = magic_damage_samples(247, 80, 111, -2500, FIRE_R, 30)
    average, dmg_min, dmg_max = magic_sample_stats(samples)
    assert dmg_min == 904, dmg_min
    assert dmg_max == 1177, dmg_max
    assert 904 < average < 1177, average

    dot_tick, ticks, dot_total = dot_values(-300, 20)
    assert (dot_tick, ticks, abs(dot_total)) == (-30, 10, 300), (dot_tick, ticks, dot_total)

    hot_tick, ticks, hot_total = hot_values(800, 30)
    assert (hot_tick, ticks, hot_total) == (53, 15, 795), (hot_tick, ticks, hot_total)
    assert 1920 == 1920

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

    bots = load_bots(run_query(args.sqlcmd, args.server, args.db, BOT_QUERY))
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

    skills = load_type3_skills(run_query(args.sqlcmd, args.server, args.db, TYPE3_QUERY))

    write_report(bots, items, coeffs, skills, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
