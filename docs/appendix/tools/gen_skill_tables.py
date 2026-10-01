#!/usr/bin/env python3
"""Generate skill catalog appendix tables from read-only DB extracts (no hand transcription)."""
import csv, os
D = os.environ.get("FDP_DATA_DIR", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data"))
OUT = os.environ.get("FDP_APPENDIX_DIR", os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
BUFF = {1:"HP_MP",2:"AC",3:"SIZE",4:"DAMAGE",5:"ATTACK_SPEED",6:"SPEED",7:"STATS",8:"RESISTANCES",9:"ACCURACY",10:"MAGIC_POWER",
        11:"EXPERIENCE",12:"WEIGHT",13:"WEAPON_DAMAGE",14:"WEAPON_AC",15:"LOYALTY",18:"BERSERKER(ATK_SPEED_ARMOR)",19:"CRITICAL_POINT",
        20:"DISABLE_TARGETING",21:"BLIND",22:"FREEZE",23:"INSTANT_MAGIC",24:"DECREASE_RESIST",25:"MAGE_ARMOR",26:"PROHIBIT_INVIS",
        27:"ELYSIAN_WEB",28:"WALL_OF_IRON",29:"BLOCK_CURSE",30:"BLOCK_CURSE_REFLECT",31:"MANA_ABSORB",32:"IGNORE_WEAPON",
        33:"VARIOUS",40:"SPEED2",43:"ATTACK_RANGE_ARMOR",44:"MIRROR_DAMAGE_PARTY",45:"DAGGER_BOW_DEF",47:"STUN",55:"LOYALTY_AMOUNT",
        150:"NO_RECALL",151:"REDUCE_TARGET",152:"SILENCE",153:"NO_POTIONS",154:"KAUL",155:"UNDEAD",156:"UNSIGHT",157:"BLOCK_PHYSICAL",
        158:"BLOCK_MAGICAL",160:"SLEEP"}
MORAL = {1:"SELF",2:"FRIEND_WITHME",3:"FRIEND_EXCEPTME",4:"PARTY",5:"NPC",6:"PARTY_ALL",7:"ENEMY",8:"ALL",10:"AREA_ENEMY",
         11:"AREA_FRIEND",12:"AREA_ALL",13:"SELF_AREA",14:"CLAN",15:"CLAN_ALL",25:"CORPSE_FRIEND",26:"CORPSE_ENEMY"}
ATTR = {0:"-",1:"ateş",2:"buz",3:"yıldırım",4:"büyü",5:"hastalık",6:"zehir"}
T3DT = {1:"HP",2:"MP",3:"MP±",4:"dayanıklılık",5:"%HP",8:"emme",9:"%emme",11:"saf",16:"MP emme"}
T5 = {}
for r in csv.DictReader(open(os.path.join(D, "magic_type5.csv"))):
    T5[r["iNum"].strip()] = {k: v.strip() for k, v in r.items()}
T5TYPE = {"1":"DoT temizle (REMOVE_TYPE3)","2":"debuff temizle (REMOVE_TYPE4)","3":"diriltme","4":"kendini diriltme","5":"HP_MP buff kaldır"}
def i(v):
    try: return int(float(v))
    except: return 0
def effect(r):
    out = []
    t1, t2 = i(r["Type1"]), i(r["Type2"])
    for t in (t1, t2):
        if t == 1 and r["T1_iNum"]:
            s = "T1 %s%%" % r["T1_Hit"]
            if i(r["T1_AddDamage"]): s += " +%s" % r["T1_AddDamage"]
            s += ", isabet %s%s" % (r["T1_HitRate"], " (sabit)" if i(r["T1_Type"]) else "")
            out.append(s)
        elif t == 3 and r["T3_iNum"]:
            s = "T3 %s" % T3DT.get(i(r["T3_DirectType"]), "dt%s" % r["T3_DirectType"])
            if i(r["T3_FirstDamage"]): s += " %s" % r["T3_FirstDamage"]
            if i(r["T3_TimeDamage"]): s += ", süreli %s/%ssn" % (r["T3_TimeDamage"], r["T3_Duration"])
            if i(r["T3_Attribute"]): s += ", %s" % ATTR.get(i(r["T3_Attribute"]), r["T3_Attribute"])
            if i(r["T3_Radius"]): s += ", r=%s" % r["T3_Radius"]
            out.append(s)
        elif t == 4 and r["T4_iNum"]:
            bt = i(r["T4_BuffType"])
            vals = []
            for k in ("AttackSpeed","Speed","AC","ACPct","Attack","MagicAttack","MaxHP","MaxHPPct","MaxMP","MaxMPPct","HitRate","AvoidRate",
                      "Str","Sta","Dex","Intel","Cha","FireR","ColdR","LightningR","MagicR","DiseaseR","PoisonR","ExpPct","SpecialAmount"):
                v = r.get("T4_" + k, "")
                if v not in ("", "0") and not (k == "ExpPct" and v == "100") and not (k in ("AttackSpeed","Speed","ACPct","Attack","MagicAttack","MaxHPPct","MaxMPPct","HitRate","AvoidRate") and v == "100"):
                    vals.append("%s=%s" % (k, v))
            s = "T4 %s(%d) %s, %ssn" % (BUFF.get(bt, "?"), bt, " ".join(vals), r["T4_Duration"])
            if i(r["T4_Radius"]): s += ", r=%s" % r["T4_Radius"]
            out.append(s)
        elif t == 5:
            x = T5.get(r["MagicNum"])
            if x: out.append("T5 %s%s" % (T5TYPE.get(x["Type"], x["Type"]), (", taş %s" % x["NeedStone"]) if i(x["NeedStone"]) else ""))
            else: out.append("T5 (satır yok)")
        elif t == 8 and r["T8_iNum"]:
            out.append("T8 warp=%s r=%s" % (r["T8_WarpType"], r["T8_Radius"]))
        elif t == 7 and r["T7_nIndex"]:
            out.append("T7 %s" % r["T7_strName"].strip())
        elif t not in (0,):
            out.append("T%d" % t)
    return "; ".join(out)
TREE = {"0":"Temel (seviye)","5":"Ağaç 5","6":"Ağaç 6","7":"Ağaç 7","8":"Master","9":"(kullanılamaz: 9)"}
CMPKEYS = ["Moral","SkillLevel","Msp","HP","UseItem","CastTime","ReCastTime","Type1","Type2","Range","Etc","UseStanding","T1_Hit","T1_HitRate",
           "T1_AddDamage","T3_DirectType","T3_FirstDamage","T3_TimeDamage","T3_Duration","T3_Radius","T4_BuffType","T4_Duration","T4_Radius",
           "T4_AC","T4_ACPct","T4_Attack","T4_MaxHP","T4_MaxHPPct","T4_Speed","T4_AttackSpeed","T8_WarpType","T8_Radius"]
def gen(fname, kcode, title, treenames):
    rows = list(csv.DictReader(open(os.path.join(D, fname))))
    byid = {r["MagicNum"]: r for r in rows}
    ks = [r for r in rows if r["class_code"] == str(kcode)]
    ks.sort(key=lambda r: (r["tree_cat"], i(r["SkillLevel"]), i(r["MagicNum"])))
    L = ["# %s" % title, "",
         "> Otomatik üretildi: yerel `FDP_kn_online` veri tabanının salt okunur dökümünden (`MAGIC`, `MAGIC_TYPE1/3/4/5/7/8`), 2026-10-01. Elle değer girilmemiştir. Etiket: `[V]`.",
         "> Birimler: Cast ve Recast **0,1 sn** biriminden saniyeye çevrildi (sunucu recast'i tam saniye çözünürlükle karşılaştırır, bkz. 03 MEC-MAG-02; cast süresini sunucu uygulamaz, MEC-MAG-01). Menzil metre. `Gerek` = ağaç puanı (Ağaç/Master) veya karakter seviyesi (Temel).",
         "> El Morad ID = Karus ID + 100000. `Fark` sütunu El Morad satırının sayısal olarak farklı olduğu alanları gösterir (Karus değeri → El Morad değeri).",
         "> Tip kısaltmaları: T1 yakın dövüş, T3 doğrudan hasar/heal, T4 buff/debuff, T5 cure/diriltme, T7 özel, T8 ışınlanma. `Etc ≠ 0` = quest kapısı (03 §4.3 U11). `Taş` = `BeforeAction` 1–4 olduğunda tüketilen sınıf taşı (03 §4.3 U9); bu durumda `UseItem` yalnızca gereksinimdir.", ""]
    cur = None
    for r in ks:
        tc = r["tree_cat"]
        if tc != cur:
            cur = tc
            L += ["", "## %s — %s" % (TREE.get(tc, tc), treenames.get(tc, "")), "",
                  "| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |",
                  "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
        eid = str(i(r["MagicNum"]) + 100000)
        e = byid.get(eid)
        if e is None:
            diff = "El Morad satırı yok"
        else:
            d = []
            for k in CMPKEYS:
                if (r.get(k) or "").strip() != (e.get(k) or "").strip():
                    d.append("%s %s→%s" % (k, r.get(k), e.get(k)))
            diff = "; ".join(d)
        L.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            r["MagicNum"], eid if e else "—", r["EnName"].strip(), r["SkillLevel"], MORAL.get(i(r["Moral"]), r["Moral"]),
            r["Msp"] + (("/HP " + r["HP"]) if i(r["HP"]) else ""), "%.1f" % (i(r["CastTime"]) / 10), "%.1f" % (i(r["ReCastTime"]) / 10),
            r["Range"], effect(r).replace("|", "/"), r["UseItem"] if i(r["UseItem"]) else "", (str(379058000 + 1000*i(r["BeforeAction"])) if 1 <= i(r["BeforeAction"]) <= 4 else ""), r["Etc"] if i(r["Etc"]) else "",
            "evet" if i(r["UseStanding"]) else "", diff))
    open(os.path.join(OUT, title.split(" ")[0] + ".md"), "w").write("\n".join(L) + "\n")
    print(title, len(ks))
gen("skills_warrior.csv", 106, "A1_SKILLS_WARRIOR_106_206 — Master Warrior Skill Kataloğu", {"0":"temel","5":"saldırı ağacı","6":"savunma ağacı","7":"berserk/tutku ağacı","8":"master"})
gen("skills_priest.csv", 112, "A2_SKILLS_PRIEST_112_212 — Master Priest Skill Kataloğu", {"0":"temel","5":"heal ağacı","6":"buff ağacı (Aura/Ecstacy)","7":"curse ağacı (Holy Spirit/Talisman)","8":"master"})
gen("skills_mage.csv", 110, "A3_SKILLS_MAGE_110_210 — Master Mage Skill Kataloğu", {"0":"temel","5":"ateş ağacı","6":"buz ağacı","7":"yıldırım ağacı","8":"master"})
