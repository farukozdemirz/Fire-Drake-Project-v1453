# B — Combat, Magic, Potions, Death/Respawn: server-side rules (Fire-Drake v1453, commit 0f52027)

Legend: **[V]** verified in code (file:line), **[I]** inferred from code, **[R]** unknown / needs runtime or DB check.
All paths relative to repo root. Line numbers = file line numbers at commit 0f520272.

**Grep caveat:** `shared/packets.h`, `NPCHandler.cpp`, `QuestHandler.cpp` and `MerchantHandler.cpp` are ISO-8859 files. In a UTF-8 locale plain `grep` treats them as binary, so use `grep -a`.

---

## 0. Global timing facts (these shape every timing rule below)

- **[V]** `UNIXTIME` is a `time_t` with 1-second resolution. A thread refreshes it once per `sleep(1000)` (`shared/TimeThread.cpp` 5, 26-43). Every server-side "cooldown" below compares whole seconds.
- **[V]** Timed effects (DOT/HOT ticks, buff expiry, HP/MP regen, blink end, saved-magic expiry) run only inside `CUser::Update()` (`GameServer/User.cpp` 472-533). Two things call it:
  - every packet the user sends: `HandlePacket` → `Update()` (`User.cpp` 465);
  - a session timer every 30 s (`GameServerDlg.cpp` 728-758, `sleep(30 * SECOND)` 755).
  - **[I]** A server-side bot that sends no packets must call `Update()` itself, often (about once per second), or its DOTs, buffs and regen freeze for up to 30 s.
- **[V]** Constants:
  - `PLAYER_SKILL_REQUEST_INTERVAL 0.7f` (`User.h` 23)
  - `PLAYER_R_HIT_REQUEST_INTERVAL 1.0f` (`User.h` 25)
  - `BLINK_TIME 15` (`Define.h` 61)
  - `CLAN_SUMMON_TIME 180` (`Define.h` 62)
  - `MAX_TYPE3_REPEAT 40` (`Define.h` 10)
  - `MAX_DAMAGE 32000` (`Define.h` 23)
  - `ARROW_EXPIRATION_TIME 5` (`User.h` 91)
  - `VIEW_DISTANCE 48`, the region size (`shared/globals.h` 19)
  - `MAX_PARTY_USERS 8` (`shared/database/structs.h` 291)
- **[V]** Ronark Land = zone 71 (`Define.h` 140). Zone flags are `ZF_ATTACK_OTHER_NATION` only, so it is **not** a war zone. Min level 35 (`Unit.cpp` 1100-1104, `Define.h` 161). `isInPKZone()` is true for zones 71, 72, 73 (`User.h` 376). `isInSafetyArea()` has no case for 71, so it is always false there (`Unit.cpp` 1311-1345).

---

## 1. Normal attack ("R" hit) — `CUser::Attack` (`AttackHandler.cpp` 4-93)

**Packet in (WIZ_ATTACK 0x08, `packets.h` 10):** `u8 bType, u8 bResult, i16 tid, i16 delaytime, i16 distance` (line 10).
- `bType` is only echoed back.
- `bResult` from the client is overwritten (line 36).

**Packet out (to region):** `WIZ_ATTACK, bType, u8 bResult, u16 attackerSocketID, i16 tid` (90-92). Results are `ATTACK_FAIL=0 / SUCCESS=1 / TARGET_DEAD=2` (`shared/globals.h` 345-347). A FAIL packet is still broadcast when validation fails after line 35. Earlier `return`s send nothing.

**Validation order [V]:**
1. `isIncapacitated()` → return (15). This means the attacker is dead, blinded (`m_bIsBlinded`), blinking, or Kaul (`Unit.h` 100).
2. `isInSafetyArea()` → return (18).
3. `RemoveStealth()` runs unconditionally, before any other check (21).
4. Weapon delay/distance check, using **client-supplied** values (23-33):
   - Right-hand item present and not a mage: reject if `delaytime < item.m_sDelay + 10` or `distance > item.m_sRange`.
   - Otherwise (no right-hand item, or a mage): reject if `delaytime < 100`.
   - Attack-speed buffs are **not** used. `m_sAttackSpeedAmount` is never read outside MagicProcess (grep).
5. Target exists, `isInAttackRange(target)` and `CanAttack(target)` (38-40).
   - Range (`User.cpp` 5013-5076): 2D squared distance ≤ (15 + weapon `m_sRange`)², where the weapon is the right hand (durability > 0), else the left hand. With no usable weapon the range is 15. The code comment calls 15 "far too generous" (5022).
   - `Unit::CanAttack` (`Unit.cpp` 855-875): same zone, attacker not incapacitated, target not dead, target not blinking, `isHostileTo(target)`.
6. `isAttackable(target)` (only NPC types are restricted; always true for players, `Unit.cpp` 888-924) and `CanCastRHit` (42). See §9 for CanCastRHit.
7. Temple-event room check (44-47).
8. Target has `BUFF_TYPE_FREEZE` → return, no packet (49-50).
9. The R timestamp is recorded (`m_RHitRepeatList[sid] = UNIXTIME`) **before** damage is computed (52-54).
10. `damage = GetDamage(target)` (56). Zone overrides (58-70): Snow War → 0; Chaos Dungeon → 50; Prison → 1 and costs 20% max MP.
11. If `damage > 0`:
    - `target->HpChange(-damage, this)` (74).
    - Result is TARGET_DEAD or SUCCESS (75-78).
    - Durability: attacker weapons lose `ItemWoreOut(ATTACK)` and the defender's 5 armour slots lose `ItemWoreOut(DEFENCE)` (81-85).

**On kill [V]:** `CUser::HpChange` → `m_sHp == 0` → `OnDeath(attacker)` (`User.cpp` 1981-1982). See §8.

**Not checked [V]:**
- No facing/angle check.
- No line-of-sight check.
- No check on the weapon-disabled state. `BUFF_TYPE_IGNORE_WEAPON` only lowers AP through `SetUserAbility` (`User.cpp` 2092).
- R-hits never trigger mage-armour reflection: `ReflectDamage` is only called from the Type1/2/3 paths.

---

## 2. Magic pipeline

### 2.1 Opcodes (`shared/packets.h` 380-403, read with `grep -a`)

| Opcode | Value | Server behaviour |
|---|---|---|
| MAGIC_CASTING | 1 | Validated, then broadcast to the region only (`MagicInstance.cpp` 39-42). **No server-side cast timer.** `bCastTime` is loaded (`MagicTableSet.h` 29) but only the AIServer reads it (grep). |
| MAGIC_FLYING | 2 | Arrows/MP for type 2 (44-93). |
| MAGIC_EFFECTING | 3 | Executes the skill (95-133). |
| MAGIC_FAIL | 4 | Echoed to the caster only (40-41). |
| MAGIC_DURATION_EXPIRED | 5 | Server → client only. |
| MAGIC_CANCEL / CANCEL2 | 6 / 13 | Cancels own HoT, buff, transform, stealth (135-141; 2730-2880). |
| MAGIC_CANCEL_TRANSFORMATION | 7 | Server → client only. |
| MAGIC_TYPE4_EXTEND | 8 | Extends a buff once; needs a Moral-240 "duration item" (2882-2940). |
| MAGIC_TRANSFORM_LIST | 9 | Server → client only. |

Fail codes `sData[3]`: −100 casting failed, −103 no effect, −104 missed (`packets.h` 396-403; `MagicInstance.cpp` 706, 1104).

**[V]** There is no casting state machine. Nothing records that a CASTING packet came before EFFECTING; each packet runs independently through `Run()` (10-147). **A bot can send EFFECTING directly, so it must apply cast times itself.**

### 2.2 Packet layout

- **In** (`MagicProcess.cpp` 16-53): `u8 opcode, u32 skillID, i16 casterID, i16 targetID, i16 sData[7]`.
  - `casterID` must equal the sender's own ID and be below NPC_BAND (46-49), otherwise the packet is dropped silently.
  - If the skill is unknown the packet is dropped (24-39). The "skill hack" disconnect cannot fire because `nSkillID` is uint32 and is never < 0.
- **Out:** same field order (`BuildSkillPacket` 727-744).
- AOE aim point = `sData[0]` (X) and `sData[2]` (Z), in world units **[I]**. The same fields are filled with `GetX()`/`GetZ()` in `Unit.cpp` 466-467.

### 2.3 Full validation sequence for a player EFFECTING cast (in execution order)

`Run()` calls `CheckSkillPrerequisites()` first, then `UserCanCast()`, which calls `IsAvailable()` (22-25, 283-285). For opcodes other than FLYING and EFFECTING, `CheckSkillPrerequisites` only checks range at CASTING when `UseStanding==1`, then returns OK (300-310).

| # | Check | Where (MagicInstance.cpp unless noted) | Detail |
|---|---|---|---|
| P0 | Magic table not reloading | MagicProcess.cpp 18 | silent drop |
| P1 | Skill exists, caster ID = self | MagicProcess.cpp 24-49 | silent drop |
| C1 | Zone ≠ Prison | 324 | |
| C2 | `sUseStanding==1` ⇒ `m_sSpeed == 0` | 327-329 | `m_sSpeed` is the last WIZ_MOVE speed (`CharacterMovementHandler.cpp` 18). Bot must send a stop move first. |
| C3 | Type-3 skill with `bAttribute==0` erases the per-type gate entry | 331-345 | Heals and other non-elemental type 3 skip check C7. |
| C4 | Range, safety area, event room (when a target is given) | 349-357 | If `sRange>0` and `UseStanding==0`: fail when `distance ≥ sRange` (real distance, strict `<`). Caster in a safety area and skill ID < 400000 → fail. |
| C5 | Per-skill recast | 360-370 | `diff = (UNIXTIME − last)·1000`; fail if `0 < diff < sReCastTime·100` (sReCastTime is in 0.1 s). Exempt: type 9, saved-magic recasts, TYPE4_EXTEND. **diff == 0 (same second) passes.** |
| C6 | Rogue bow check | 372-384 | rogues only |
| C7 | Per-type gate (types 1-7, skill ID < 400000) for `bType[0]` and `bType[1]` | 386-422 | fail if `UNIXTIME − last < 0.7` (in practice: same second) |
| C8 | Target: NPC must be attackable; target with `BUFF_TYPE_FREEZE` → fail | 426-435 | applies to all skills, heals included |
| U1 | `canUseSkills()` and not dead (dead may only use type 5) | 164-166 | `canUseSkills` = `m_bCanUseSkills && !Kaul && !(!blinded && has BLIND)` (`Unit.h` 102). **Blinking and blinded players can still cast.** |
| U2 | AOE moral (10-13) must have target −1 | 169-179 | |
| U3 | Class/level: if `sSkill≠0`, require `m_sClass == sSkill/10` **and** level ≥ `sSkillLevel` (not in Chaos) | 184-188 | Exact class code (e.g. 106), not the base class |
| U4 | Chaos Dungeon attack window | 190 | |
| U5 | MP ≥ `sMsp` | 193 | |
| U6 | Snow War: only the snowball skill | 197 | |
| U7 | Death taunt on a corpse is handled specially | 203-225 | |
| U8 | `iUseItem≠0` (not type 2/6) ⇒ `CanUseItem(iUseItem)` | 244-247 | For RESURRECTION the **target** must hold `sNeedStone` stones. `CanUseItem`: item class, level range, item present (`User.cpp` 5087-5111). |
| U9 | Class stone: `nBeforeAction ∈ 1..4` ⇒ consume `379058000 + n·1000` instead of UseItem | 251-259 | |
| U10 | CSW transforms only in Delos | 262 | |
| U11 | **Etc quest**: non-GM, `sEtc≠0` ⇒ `CheckExistEvent(sEtc, 2)` | 267-275 | Release builds only (`#if !defined(DEBUG)`). Quest `sEtc` must be in state 2. A missing quest is state 0, so it fails (`QuestHandler.cpp` 137-149). |
| U12 | `bType[0] < 4` with a target ⇒ `isInAttackRange(target, skill)` | 277-280 | see §2.4 |
| I1 | Chaos window | 799 | |
| I2 | Moral check | 804-898 | see §4 |
| I3 | Type3/4/6 prerequisites | 901-925 | see §3 |
| I4 | No-Potion block: type 3, DirectType 1, FirstDamage > 0, UseItem with item class 0, and `!canUsePotions()` → fail | 930-949 | |
| I5 | **Skill-tree check**: `mod = sSkill % 10`. If `mod≠0`: `GetClass() == sSkill/10` and `m_bstrSkill[mod] ≥ sSkillLevel` (COMMAND_CAPTAIN exempt). Else: level ≥ sSkillLevel. | 951-958 | Indices 5-7 = trees 1-3, 8 = master (`GameDefine.h` 176-183) |
| I6 | Type 1 with `sSkill` 1055/2055 (dual wield) or 1056/2056 (two-hand): weapon class checks; weapons must not be disabled | 960-991 | |
| I7 | EFFECTING: MP ≥ sMsp; for type 3/4 items, item class and ReqLevel. **MP is deducted here, before execution** (all types except single-target type 4, which deducts later per successful target, line 1795-1797). HP cost for `0<sHP<10000` with `sMsp==0`; `sHP≥10000` = Sacrifice (−10000 HP, not on self). | 994-1045 | MP is lost even if execution then fails (e.g. target died) [I] |
| E1 | Execute `bType[0]`. On success: record per-skill and per-type cooldowns (unless `BUFF_TYPE_INSTANT_MAGIC`), execute `bType[1]`, `ConsumeItem()` (not type 2) | 108-132 | `ConsumeItem` 2978-2996 |

Other fields:
- **[V]** `bItemGroup`, Type1 `bDelay/bComboType/bComboCount/sComboDamage/sRange` are loaded but never read in GameServer (grep).
- **[V]** `bSuccessRate` is read only for the lightning-stun visual in type 3 (1361). There is **no general success-rate roll**.
- **[V]** `ConsumeItem` passes `RobItem(0)` for item IDs 370001000-370003000, 379063000-379066000, 379069000 and 379070000. `RobItem(0)` returns false (`ItemHandler.cpp` 349), so those items are never consumed.

**Etc quest [V]/[R]:** the code requires quest `sEtc` in state 2. Whether Etc=1 covers "all pots" and Etc 510-523 are the master-skill quests is DB/Lua data and is not in the repo [R]. The setup note's claims match the mechanism. Quest 51 in state 2 also adds +10% defence to the level-70+ warrior passive (`User.cpp` 2241-2245).

### 2.4 Range rules (`Unit::isInAttackRange`, `User.cpp` 5043-5068)

- Applies only when moral is MORAL_ENEMY or ≤ MORAL_PARTY (5046).
- **Type 1:** allowed distance = 15 + (`sRange` if non-zero, else weapon range) + weapon range (5059).
- **Type 3:** allowed distance = 15 + (`sRange` if non-zero, else 15) (5059).
- **Type 2:** allowed distance = bow range + `Type2.sAddRange` + 65 (5067).
- Stricter checks on top of that:
  - C4: distance < `sRange` when `UseStanding==0`.
  - Inside `ExecuteType3` and `ExecuteType4`, each target is skipped silently if distance ≥ `sRange` (1332-1334; 1690, which exempts `BUFF_TYPE_HP_MP`).
  - **[I]** For type 3, the effective maximum range is `sRange` with a strict `<`.

---

## 3. Type-specific execution

### Type 1 — melee skills (`ExecuteType1` 1056-1110; hit/damage in `CUser::GetDamage`, `Unit.cpp` 270-336)

- Target must be alive (1068). Type1 does not check blinking; `CanAttack` is not called here.
- Hit roll:
  - `bHitType ≠ 0`: success if `sHitRate > rand(0,100)` (282).
  - `bHitType = 0`: `GetHitRate((hitrate/evasion)·sHitRate/100)` (287).
- Base: `temp_hit = temp_hit_B · sHit/100` (290). On a hit: `damage = temp_hit + 0.3·rand(0,temp_hit) + 0.99` (333).
- PvP: `sAddDamage /2` in a war zone, **/3 otherwise (Ronark)** (1077-1084). It is added after `GetDamage` (which already includes the PvP /2). It is added **even when the hit roll missed**, unless the target has block-physical (1091-1092).
- Then `HpChange(-damage)` and mage-armour reflection (1097-1100).
- No durability loss on Type 1.
- Combo fields are unused (see the I-rows above).

### Type 2 — archery (note only)

- FLYING phase takes MP and arrows and records flying arrows (44-93).
- EFFECTING requires a matching arrow younger than 5 s and range ≤ (sAddRange·bowRange/100) (1194-1236).

### Type 3 — direct damage/heal/DOT/HOT (`ExecuteType3` 1258-1611)

- **Targets:**
  - AOE (`sTargetID=-1`): units in the 3×3 regions around the caster, then `UserRegionCheck` against `bRadius` around the aim point (1271-1291).
    - The caster is added when FirstDamage > 0 or TimeDamage > 0, i.e. heals (1275-1276).
    - Skipped: GMs, dead, blinking (1284-1289).
  - Single target: must be alive and not blinking (1302-1306).
- **First damage** (1337-1342): if `sFirstDamage<0`, DirectType 1 or 8, and skill ID < 400000 → `GetMagicDamage(target, sFirstDamage, bAttribute)`. Otherwise the raw `sFirstDamage` is used.
  - Target with block-magic: negative damage is skipped (1345).
- **DirectType meanings, non-durational** (`bDuration==0`, 1381-1514):

| DirectType | Effect |
|---|---|
| 1 | HP ± through `HpChangeMagic`. Heals on NPCs are refused. Caster with `BUFF_TYPE_DAMAGE_DOUBLE`: 50% chance (`CheckPercent(500)`) to double a heal. |
| 2 | MP += `sFirstDamage` on players (HP on NPCs) |
| 3 | MP += damage |
| 4 | repair (positive) / acid (negative) durability |
| 5 | % of HP |
| 8 | absorb (odd formula) |
| 9 | % absorb |
| 11 | raw true damage |
| 12 | skipped |
| 13 | 50% chance of durability damage |
| 16 | MP drain, caster gets half as HP |
| 17 | Delos only |
| 19 | Chaos Dungeon |

- **Heals [V]:** `HpChange` clamps at `m_iMaxHp` (`User.cpp` 1941-1942). No overheal tracking. An undead target takes heals as damage (1933-1937).
- **Durational** (`bDuration≠0`, 1517-1586):
  - Initial damage applied first.
  - Per-tick amount = `TimeDamage` (passed through `GetMagicDamage` when negative and attribute ≠ 4) ÷ (`bDuration`/2) ticks (1532-1566).
  - `m_bHPInterval = 2` seconds (1557).
  - Stored in the first free slot of `m_durationalSkills[40]`. **DOTs from repeated casts stack** — there is no de-duplication by skill.
  - Ticks run in `HPTimeChangeType3` (`User.cpp` 3378-3441), driven by `Update()`. The first tick fires on the next `Update()` because `m_tHPLastTime=0`.
  - A DOT kill credits `m_sSourceID` as killer (3396-3399). `HpChange` ignores an attacker in another zone (1869-1870).
- **HoT restrictions** (`CheckType3Prerequisites` 445-532):
  - Single friendly HoT is refused if the target already has any HoT (`m_sHPAmount>0`) (514-523).
  - Party HoT (MORAL_PARTY_ALL) is refused if the caster has any HoT, or is monster-transformed (462-478).
  - A friendly single-target skill on a dead player fails (508-511).
- **Lightning stun visual** (1356-1378; resistance roll as in Type 4): only changes `sData[1]` and the status packet. No server-side stun state.

### Type 4 — buffs/debuffs (`ExecuteType4` 1613-1891; `GrantType4Buff` / `RemoveType4Buff`, `MagicProcess.cpp` 268-1007)

- **Storage [V]:** `m_buffMap : std::map<uint8 buffType, _BUFF_TYPE4_INFO>` (`Unit.h` 30, 320). Each entry holds `{skillID, buffType, isBuff, durationExtended, endTime}` (`structs.h` 314-326). One entry per BuffType.
- `isBuff` is computed at load by `CMagicProcess::IsBuff` (`MagicType4Set.h` 47; `MagicProcess.cpp` 1017-1172).
- **Stacking [V]:**
  - **Buff on a single target that already has that BuffType (buff or debuff) → fail** (`CheckType4Prerequisites` 580-586; `ExecuteType4` 1765, 1771-1777; `GrantType4Buff` 272-274).
  - A stronger buff does **not** replace a weaker one; the old one must expire or be cancelled first.
  - AOE buffs skip such targets silently (1782).
  - **Debuff:** an existing entry of the same type (buff or debuff) is removed, then the new debuff is granted with a fresh `endTime` and the new values (1751-1755, 1800-1811). A slow therefore overrides a speed buff.
  - Debuffs never land on the caster unless the skill explicitly allows it (1760-1762).
- **Debuff landing:**
  - There is no generic success roll.
  - Counter Curse (`m_bBlockCurses`) blocks all debuffs (1718, 1767).
  - Curse Refraction reflects with 25% chance (`CheckPercent(250)`) and otherwise blocks (1722-1736).
  - Speed/slow/stun resistance roll (1816-1843), player targets with MORAL_ENEMY or AREA_ENEMY:
    - `R = target ColdR` (for SPEED2) or `LightningR` (for SPEED/STUN). Raw item resistance only.
    - If `R < 125` then `R = 110`. `max = max(250, R)`. Resisted if `R ≥ rand(0, max−R)`.
    - **[I]** That gives a resist chance of 111/141 ≈ 78.7% when item resist < 125, and **100%** when ≥ 125.
    - A "resist" only sends the old speed to the client. The debuff stays in `m_buffMap` server-side (granted at 1769 and inserted at 1810 before the roll) [V]. It still blocks a later speed buff of the same type [I]. Runtime test advised [R].
- **Duration/expiry [V]:**
  - `endTime = UNIXTIME + sDuration` (1807).
  - `CUser::Type4Duration` removes **one** expired entry per `Update()` (`User.cpp` 3443-3460).
  - Scroll buffs (skill ID > 500000) persist through `InsertSavedMagic` (1786-1793) and are recast after respawn and zone change (`RecastSavedMagic` `User.cpp` 5235-5269).
  - The same scroll on a target that already has it fails (1625-1628).
- **MP:** a single-target type 4 costs MP only per successful grant (1795-1797). AOE type 4 is charged up front (1023-1024).
- **Speed is client-side [V]:** `m_bSpeedAmount` is written but never read for movement validation. Grep finds it only in MagicProcess and `Unit.cpp` 54. Movement speed is checked against fixed per-class limits (`User.cpp` 2862-2881, 3581-3607). **Slow, stun, freeze and Wall-of-Iron movement penalties must be applied by the bot itself.**

### Type 5 — cure and resurrection (`ExecuteType5` 1893-2058; constants `MagicInstance.h` 55-59)

- `REMOVE_TYPE3` (1): clears all DOT slots (`m_sHPAmount<0`); HoTs are kept (1965-1997).
- `REMOVE_TYPE4` (2): removes every **debuff** and recasts lockable scrolls (1999-2020).
- `RESURRECTION` (3):
  - Target must hold `sNeedStone` × `iUseItem`; those are taken from the **target**.
  - The caster receives `sNeedStone/2 + 1` of the item.
  - Then `Regene(1, skillID)` (2028-2037).
- `RESURRECTION_SELF` (4): caster == target and `m_iLostExp ≠ 0` (2022-2027).
- `REMOVE_BLESS` (5): removes `BUFF_TYPE_HP_MP` (2039-2046).
- AOE type 5 iterates **all sessions server-wide**, filtered by `UserRegionCheck(radius = sRange)` (1907-1929).

### Type 6 — transforms (note only)

- Blocked in PvP zones (zone > 2, except Eslant) unless it is an NPC transform (2071-2078).

### Type 7

- Only `bTargetChange==1` deals flat `sDamage` (2155-2226).
- The function returns false at the end, so no cooldown is recorded, `bType[1]` is not executed and no item is consumed (2225).

### Type 8 — warp, summon, resurrect (`ExecuteType8` 2229-2453)

- **Common checks:** target alive (dead only for warp type 11), `canTeleport()` (blocked by `BUFF_TYPE_NO_RECALL`), not already warping (2273-2286).
- **Sub-modes:**

| WarpType | Behaviour |
|---|---|
| 1 | To bind point or nation start position. Skill IDs 109035/110035/209035/210035 fail in zones > 31 (Ronark included) (2290-2324). |
| 2, 3, 5 | Not implemented (2325-2333) |
| 11 | Resurrect: HP to max, `ExpChange(sExpRecover/100)`. Does **not** call `Regene` (no buff reset) (2335-2348). |
| 12 | **Summon within zone** (2350-2368) — see below |
| 13 | Summon across zones (2370-2380) |
| 20 | Blink forward (2382-2402) |
| 21 | Monster staff (2404-2425) |
| 25 | Teleport to target, same zone, distance ≤ sRadius (2427-2442) |

- **Warp type 12 rules:** target must be in the caster's zone and not the caster. It fails in Forgotten Temple, and for skill IDs 490042/490050 in zones > 31.
  - Single target: party membership is enforced only through moral (`MORAL_PARTY` in `IsAvailable` 826-842).
  - AOE: `MORAL_PARTY_ALL` → `UserRegionCheck` requires same party. In **war zones only**, a target that respawned less than 180 s ago is refused (`MagicProcess.cpp` 130-140). `sRadius = 0` means any distance (201).
  - **No consent check and no nation check** beyond party (not found).

### Type 9 — stealth and detect (2455-2556)

- `bStateChange` 1/2: stealth (dispelled on move / on attack); needs `canStealth`; not when already invisible.
- 3/4: see invisible (4 applies to the whole party), sends `WIZ_STEALTH` with the radius.
- 9: summon guard NPC.
- Duplicate state → fail.
- Expiry through `CheckExpiredType9Skills` (`MagicProcess.cpp` 217-242).
- `ExecuteSkill` removes the caster's stealth for types 1, 2, 3 and 7 (662-666).

---

## 4. Moral and area targeting

**Moral enum** (`MagicInstance.h` 23-51):

| Value | Name | Value | Name |
|---|---|---|---|
| 1 | SELF | 10 | AREA_ENEMY |
| 2 | FRIEND_WITHME | 11 | AREA_FRIEND |
| 3 | FRIEND_EXCEPTME | 12 | AREA_ALL |
| 4 | PARTY | 13 | SELF_AREA |
| 5 | NPC | 14 | CLAN |
| 6 | PARTY_ALL | 15 | CLAN_ALL |
| 7 | ENEMY | 25 | CORPSE_FRIEND |
| 8 | ALL | 26 | CORPSE_ENEMY |
| | | 31 | SIEGE_WEAPON |
| | | 240 | EXTEND_DURATION |

**Single-target checks (`IsAvailable` 804-898):**
- SELF: target must be the caster.
- FRIEND_WITHME: self, or a non-hostile target.
- FRIEND_EXCEPTME: non-hostile, not self.
- PARTY: same party ID, or self when not in a party.
- ENEMY: `isHostileTo(target)`, or no target at all.
- CORPSE_FRIEND: target dead, non-hostile, not self.
- CLAN: same clan.

**Area checks (`UserRegionCheck`, `MagicProcess.cpp` 113-202):**
- PARTY_ALL: same party (self only when not in a party).
- AREA_ENEMY and SELF_AREA: hostile.
- AREA_FRIEND: non-hostile.
- AREA_ALL: both units in the arena, PvP zone or temple zone.
- CLAN_ALL: same clan.
- Final test: target within `radius` of the **client-supplied aim point** (`sData[0]`, `sData[2]`); radius 0 means unlimited (200-201).

**Candidate set:** NPCs and users in the 3×3 regions of 48 units around the caster (`GameServerDlg.cpp` 1547-1576; `globals.h` 427-428). For type 3 and type 4, targets beyond `sRange` from the caster are dropped (when `sRange > 0`).
- **[V]** No maximum target count (grep for target caps: not found).
- **[I]** Nothing checks that the aim point itself is within range of the caster.
- **[V]** A Type-4 AOE removes stealth from every enemy-nation player in the 3×3 regions, whether or not they are hit (1645-1652).

**Hostility (`CUser::isHostileTo`, `Unit.cpp` 1219-1256):** true when both players are in an arena; false when both are in a safety area; true for different nations when `isInPVPZone()` (zone flag, or an invaded home zone); true in temple zones; siege-war rules otherwise. In Ronark: the other nation is hostile, your own nation is not.

---

## 5. Potions and consumables

- **[V]** There is no item-use opcode; `packets.h` has no ITEM_USE (`grep -a`). Potions are MAGIC rows cast through `WIZ_MAGIC_PROCESS` / `MAGIC_EFFECTING` with `iUseItem` set. Comment: "requiring an item (i.e. pots)" (`MagicInstance.cpp` 941).
  - The item must be present and usable (`CanUseItem`, U8).
  - It is consumed after success (`ConsumeItem` 2978-2996; `RobItem` `ItemHandler.cpp` 346-420).
- **Cooldown [V]:**
  - Only the per-skill-ID `m_CoolDownList` with `sReCastTime` from the MAGIC row applies (C5). It is keyed by skill ID (`User.h` 15), so the **HP-potion and MP-potion cooldowns are separate server-side**, and so are different potion tiers.
  - Same-second re-use passes the per-skill check (C5 hole).
  - The 0.7 s per-type gate applies only to skill IDs < 400000 (395, 413).
  - **[R]** Potion skill IDs (typically 49xxxx/5xxxxx) must be confirmed in the DB. If they are ≥ 400000, potions have no per-type gate at all.
  - No `m_tLastPotion` or shared potion timer exists (grep "potion": only `canUsePotions`).
- **Restrictions [V]:**
  - Dead → fail (U1).
  - Silenced (`BUFF_TYPE_SILENCE_TARGET` → `m_bCanUseSkills=false`) → fail.
  - Kaul → fail.
  - `BUFF_TYPE_NO_POTIONS` blocks only type-3 DirectType-1 HP heals whose item has class 0 (930-949). MP potions are not blocked by it [I: depends on DirectType 2/3 in DB].
  - `sUseStanding==1` items cannot be used while moving (C2).
  - Undead turns heals into damage.
  - Instant HP heal amount = `sFirstDamage`, clamped to max HP. Critical Point can double it (1391-1394).

---

## 6. Damage, defence and resistance formulas

**Physical (R / Type1 / Type2), `CUser::GetDamage` (`Unit.cpp` 208-394) [V]:**
- `ac = T.m_sTotalAc + max(0, T.m_sACAmount)`. If `0 < T.m_sACPercent < 100`: `ac -= ac·(100−pct)/100` (232-241).
  - **[I]** The AC percentage is also applied to `m_sTotalAc` in `SetUserAbility` (`User.cpp` 2211), so an AC debuff counts twice.
- `ap = m_sTotalHit · m_bAttackAmount` (243).
- PvP: `ap = ap·m_bPlayerAttackAmount/100` (Critical Point); class bonuses `ac·(100+T.AcClassBonus[myClass])/100`, `ap·(100+APClassBonus[TClass])/100` (247-258).
- Target with block-physical → damage 0 (264).
- `B = (ap·2)/(ac+240)` (267).
- R hit: result from `GetHitRate(hitrate/evasion)`; on any non-FAIL result, `damage = 0.85·B + 0.3·rand(0,B)` (351-353). GREAT_SUCCESS gives no extra damage for players. FAIL gives 0.
- Then:
  1. Item bonuses (`Unit::GetMagicDamage` 591-674): +flat elemental `sAmount − sAmount·min(200,R)/200`, added **even after a miss**.
  2. Weapon-type resistances: `dmg −= dmg·T.m_sXxxR/200` per equipped weapon (676-716).
  3. **PvP `/2`** — both branches divide by 2 (383-386).
  4. Cap 32000 (390).
- **[V], probable bugs:**
  - The HP-drain item bonus calls `pTarget->HpChange(+x)`, which **heals the defender**, heals the attacker too, then returns early (629-632, 655-660).
  - The mirror-damage item bonus damages the item's **owner, i.e. the attacker** (667-671).

**Hit table (`Unit::GetHitRate` 718-804) [V]:** the ratio is hitrate/evasion. Probability of a non-FAIL result:
| Ratio | Non-FAIL |
|---|---|
| ≥ 5 | 98% |
| ≥ 3 | 96% |
| ≥ 2 | 94% |
| ≥ 1.25 | 92% |
| ≥ 0.8 | 90% |
| ≥ 0.5 | 80% |
| ≥ 0.33 | 70% |
| ≥ 0.2 | 60% |
| below | 50% |

- **[I] quirk:** `m_fTotalHitrate · (m_bHitRateAmount/100)` uses integer division (`User.cpp` 2213-2215). Any accuracy or evasion debuff below 100 turns into 0.

**Magic (Type 3), `MagicInstance::GetMagicDamage` 2558-2717 [V]:**
- Base value is the negative `sFirstDamage`/`sTimeDamage`; for a player caster it is scaled `·CHA/186` (CHA = magic power).
- `sMagicAmount = max(0, CHA−86) + m_sMagicAttackAmount` (2588-2595).
- Resistance `R = (base + add)·pct/100 + T.m_bResistanceBonus` (2601-2623).
- `d = 230·hit/(R+250)`, then `d = 0.3·rand(0,d) + 0.85·d − sMagicAmount` (2693-2695).
- Staff with an attribute (no left-hand item): extra `−(rh·0.8 + rh·lvl/60)` (2629-2648, 2699-2700).
- Elysian Web: `d·reduction/100` (2701-2702).
- The weather bonus result is **discarded**: the return value of `GetWeatherDamage` is ignored (2707).
- **PvP `/3`** (2709-2710).
- The cap only applies to positive values (2713). `HpChange` clamps to ±32000 (`User.cpp` 1873-1876).

**Defender-side modifiers in `CUser::HpChange` (`User.cpp` 1860-1983) [V]:**
- GM takes no damage.
- Taking damage removes stealth.
- Minak's Thorn mirror: `m_byMirrorAmount`% of the damage is split across the other party members; it divides by (members − 1) (1887-1906).
- Mana absorb: `(dmg·pct/100)` is moved from HP loss to MP loss; ×4 when pct == 15 (1909-1919).
- Master passives: −15% damage at master tree points ≥ 10, −10% at 5-9 (1922-1930).
- Any HP drop to 0 → `OnDeath`.

**Mage-armour reflection (`ReflectDamage` 2942-2976) [V]:** it computes a 25% `reflect_damage` but then applies the **full** damage to the attacker (bug). It then removes the armour buff. It applies to Type 1, Type 2 and Type 3 damage only.

---

## 7. HP/MP natural regeneration (`CUser::HPTimeChange`, `User.cpp` 3325-3376; trigger 482-483)

- **Trigger:** `Update()` with `!isBlinking() && (UNIXTIME − last) > 5` (`m_bHPIntervalNormal = 5`, `User.cpp` 117). In practice a tick needs at least 6 s and an `Update()` call.
- **Standing:** **MP only**: `(int)(((lvl·(1+lvl/60)+1)·0.2)+3)·mp%/100`. At level 80: **40 MP per tick**. Mastered mage (class 110/210) below 30% MP: mp% = 120, so 48 [V formula; arithmetic I]. **No HP regen while standing.**
- **Sitting** (`m_bResHpType == USER_SITDOWN`, set by StateChange type 1, `User.cpp` 2685-2690, 2748-2749):
  - HP += `(int)(lvl·(1+lvl/30))+3`, i.e. **296 HP at level 80**.
  - MP += `(int)((maxMP·5)/(lvl−1+30)+3)·mp%/100`.
- Dead → nothing. Snow War → +5 HP.

---

## 8. Death and respawn

**`CUser::OnDeath` (`User.cpp` 4727-4949) [V]:**
- Ignored if already dead. Sets `USER_DEAD`.
- Closes trade and other windows.
- `InitType3()` clears every DOT and HoT.
- `InitType4()` removes all buffs and debuffs; saved scroll entries are kept (4745-4746).
- **PvP in Ronark** (killer is another player; not war zone; no nation battle):
  - Death notice with coordinates.
  - Rival handling: if the victim is the killer's rival, +150 NP and the rivalry ends (4836-4853).
  - The victim's anger gauge +1, up to 5 (4857-4858).
  - NP:
    - Solo killer: `LoyaltyChange` gives killer +`RONARK_LAND_SOURCE` (default **64**) and victim `RONARK_LAND_TARGET` (default **−50**) (`GameServerDlg.cpp` 310-311; `User.cpp` 2795-2860). A victim with 0 NP gives 0.
    - Party killer: `LoyaltyDivide` (3068-3156). Each **alive** member, at any distance (no range check), gets `((src·3−2)/8 + 2·(8−members)) − 1`, i.e. 22 each for 8 members with defaults [I arithmetic]. The victim gets −50.
    - `SendLoyaltyChange` then adds the NP buff %, event %, item/skill NP bonuses and the monument +5 (586-660).
  - Gold: the killer (or the party, split by level) gets 40% of the victim's gold, and the victim loses 50% (`GoldChange` 4160-4217; called at 4870-4871).
  - **No exp loss** in Ronark: exp loss only applies in an enemy home zone (≤ 2) (4873-4881). An NPC kill costs 5% of max exp (1% for guards or enemy home zone) (4758-4774).
  - If the victim has no rival, the killer becomes the victim's rival for `RIVALRY_DURATION 300 s` (4884-4886; `GameDefine.h` 158).
- Death animation: `WIZ_DEAD` to the region (`Unit.cpp` 956-961).

**Respawn — client sends `WIZ_REGENE u8 type` (`User.cpp` 304-305) → `CUser::Regene` (`AttackHandler.cpp` 95-240) [V]:**
- Ignored if not dead. Any type other than 1 or 2 is treated as 1 (106-107).
- Type 2: needs level > 5 and `RobItem(379006000, 3·level)` (240 stones at level 80). **It still respawns at the town/bind position**, because the position logic does not depend on the type (109-175) [V]. There is no "revive in place" for type 2 in this code [I].
- Fails unless a START_POSITION row exists for the zone (119-121).
- **Ronark position** (`magicid == 0`):
  - A bind object event with `byLife == 1` wins (131-134).
  - Otherwise zone 71 falls to `GetStartPosition`: nation-specific `sKarusX/Z` or `sElmoradX/Z` plus `rand(0, bRange)` (162-164; `User.cpp` 3724-3756).
  - The coordinates are DB data [R].
- Sets `USER_STANDING`, sends `WIZ_REGENE x,z,y` (191-193), re-enters the region (`INOUT_RESPAWN`), sends cure-status packets.
- `HpChange(max)` twice; MP is **not** restored (223-228).
- `InitType4()` then `RecastSavedMagic()`: scroll buffs come back (225-226).
- **Blink:** `BlinkStart()` returns immediately in zones that can attack the other nation (`User.cpp` 4529-4530), so there is **no post-respawn invulnerability in Ronark**. It is also compiled out in Debug builds (`GameServer/stdafx.h` 7-9).
  - Elsewhere: 15 s; `isBlinking` blocks being R-attacked or hit by AOE, blocks own R-attacks, and stops regen. It does **not** block casting.
- Ronark with 0 NP after respawn → `KickOutZoneUser()` to the nation's home zone (231-239; 4652-4688).
- **Resurrected by a skill** (`magicid ≠ 0`, Type5 RESURRECTION): stays at the corpse position, MP set to 0 (182), exp is restored only if `m_sWhoKilledMe == -1` (not a PvP kill) (184-185), no blink, HP full, buffs reset and scrolls recast.

---

## 9. "Skill + R" timing — what the server enforces vs not

**Enforced [V]:**
1. **R gate:** at most one *successful* R per `UNIXTIME` second.
   - `CanCastRHit`: `float(UNIXTIME − last) < 1.0` → fail (`Unit.cpp` 926-947).
   - The timestamp is stored on pass (`AttackHandler.cpp` 52-54).
   - Plus the client-reported `delaytime ≥ weapon.m_sDelay + 10` (or ≥ 100) and `distance ≤ weapon.m_sRange` (27-33). Both are client-controlled values.
2. **Per-skill recast:** `sReCastTime` (0.1 s units), compared in whole seconds. The same second (diff = 0) is allowed (360-370).
3. **Per-type gate:** at most one cast per magic type 1-7 per second, for skill IDs < 400000 (386-422). Exceptions:
   - Non-elemental type 3 (attribute 0) erases its gate (331-345).
   - Types 8 and 9 have no gate.
   - Instant Magic skips recording (116-125).
4. Standing skills require `m_sSpeed == 0` (327-329).
5. Arrows must be "flying" (≤ 5 s) for type 2 (1206-1236).

**Not enforced (the bot must self-limit) [V]:**
- Cast time (`bCastTime` unused).
- CASTING → EFFECTING ordering.
- Any link between R and skills: `m_RHitRepeatList` is not read in `MagicInstance`, and no magic timestamp is read in `Attack` (grep `RHitRepeat`). An R and a Type-1 skill in the same second are both accepted.
- Weapon attack-speed buffs.
- Any global cooldown across magic types; a type-1, type-3 and type-4 cast in the same second all pass.
- Potion timer beyond the per-ID recast.
- The 0.7 s and 1.0 s intervals collapse to "different second" because of the 1-s clock. The real minimum can be anywhere from about 0 s to 2 s of wall time [I].
- The only active anti-cheat is movement distance per `WIZ_SPEEDHACK_CHECK` (`User.cpp` 3581-3607) and speed caps per move packet (2862-2881). The time-based speedhack check is `#if 0` (3609-3634).
- **Recommendation [I]:** to stay fair, a bot should use the client timings itself: weapon `m_sDelay` per R, `bCastTime` before EFFECTING, `sReCastTime` in milliseconds, and a stop before standing skills.

---

## 10. Other relevant rules

- **Durability [V]:** `ItemWoreOut`: `wear = (int)sqrt(damage/10)` per R-hit, on both attacker weapons and 5 defender armour slots (`User.cpp` 3202-3289). At 0 durability the item stops counting (`SetUserAbility`). R range falls back to 15 when the weapon has 0 durability (`User.cpp` 5030-5041).
- **Freeze target [V]:** immune to R-hits (`AttackHandler.cpp` 49) and to **all** targeted skills, friendly ones included (`MagicInstance.cpp` 432). Cannot `/town` (`User.cpp` 3704).
  - **[I]** Because the check runs whenever a target is set, a frozen player also cannot use self-targeted skills or potions sent with its own ID as target.
- **`/town` (`Home`) [V]:** requires HP ≥ 50%, not Kaul, not frozen (`User.cpp` 3695-3722).
- **Same-nation PvP in Ronark [V]:** impossible (`isHostileTo`).
- **Safety area:** none in zone 71 [V].

---

## Table A — BuffType enum (`GameDefine.h` 745-804) with server-side effect

"Buff?" comes from `IsBuff` (`MagicProcess.cpp` 1017-1172). "Server effect" is from `GrantType4Buff` (`MagicProcess.cpp` 268-631) and the places that consume it.

| Val | Name | Buff? | Server-side effect on grant |
|---|---|---|---|
| 0 | NONE | buff | — (not handled in Grant → returns false) |
| 1 | HP_MP | buff if MaxHP/MP > 0 or pct ≥ 100 | `m_sMaxHPAmount` / `m_sMaxMPAmount` (flat or pct−100); heals the added amount (1852-1853) |
| 2 | AC | by sign/pct | `m_sACAmount += sAC` or `m_sACPercent += pct−100` |
| 3 | SIZE | buff | visual giant/dwarf by skill ID |
| 4 | DAMAGE | bAttack ≥ 100 | `m_bAttackAmount = bAttack` (multiplies AP) |
| 5 | ATTACK_SPEED | ≥ 100 | `m_sAttackSpeedAmount` — **unused server-side** |
| 6 | SPEED | bSpeed ≥ 100 | `m_bSpeedAmount` — **client-only** |
| 7 | STATS | no negative stat | STR/STA/DEX/INT/CHA stat buffs |
| 8 | RESISTANCES | buff | `m_bAdd*R` |
| 9 | ACCURACY | hit & avoid ≥ 100 | hitrate/avoid multipliers (integer /100 quirk) |
| 10 | MAGIC_POWER | ≥ 100 | `m_sMagicAttackAmount = (bMagicAttack−100)·CHA/100` → more magic damage |
| 11 | EXPERIENCE | buff | exp % |
| 12 | WEIGHT | buff | max weight |
| 13 | WEAPON_DAMAGE | > 0 | `m_bAddWeaponDamage` |
| 14 | WEAPON_AC | by sign | armour AC |
| 15 | LOYALTY | buff | NP gain % (`SendLoyaltyChange` 629) |
| 16 | NOAH_BONUS | buff | gold % |
| 17 | PREMIUM_MERCHANT | buff | flag |
| 18 | ATTACK_SPEED_ARMOR (Berserker) | buff | +AC, attack speed |
| 19 | DAMAGE_DOUBLE (Critical Point) | buff | PvP AP % (`m_bPlayerAttackAmount`); 50% chance of a double heal |
| 20 | DISABLE_TARGETING | debuff | `m_bIsBlinded` → cannot R-attack (can still cast) |
| 21 | BLIND | debuff | blinded (players only) |
| 22 | FREEZE | debuff | target immune to R and to skills; no `/town` |
| 23 | INSTANT_MAGIC | buff | next cast records no cooldown |
| 24 | DECREASE_RESIST | debuff | `m_bPct*R = 100−x` |
| 25 | MAGE_ARMOR | buff | reflect (`sSkill % 100` → 5/6/7 fire/ice/lightning [R]) |
| 26 | PROHIBIT_INVIS | buff | `m_bCanStealth` flag (removal sets it false — odd) |
| 27 | RESIS_AND_MAGIC_DMG (Elysian Web) | buff | `m_bMagicDamageReduction = sExpPct` |
| 28 | TRIPLEAC_HALFSPEED (Wall of Iron) | buff | +300% AC; speed halved (client) |
| 29 | BLOCK_CURSE | buff | blocks all debuffs |
| 30 | BLOCK_CURSE_REFLECT | buff | 25% reflect, otherwise block |
| 31 | MANA_ABSORB | buff | damage → MP loss |
| 32 | IGNORE_WEAPON | debuff | weapons disabled (AP drops) |
| 33 | VARIOUS_EFFECTS | buff | AC/attack/NP |
| 35, 36 | PASSION_OF_SOUL / FIRM_DETERMINATION | buff | no-op (pet) |
| 40 | SPEED2 (Cold Wave) | debuff | speed % (client) |
| 42 | UNK_EXPERIENCE | buff | no-op |
| 43 | ATTACK_RANGE_ARMOR | buff | +100 AC, radius |
| 44 | MIRROR_DAMAGE_PARTY (Minak's Thorn) | buff | mirror % to party |
| 45 | DAGGER_BOW_DEFENSE (Eskrima) | debuff | dagger/bow R amount |
| 47 | STUN | debuff | `m_bSpeedAmount` only (client-side stun) |
| 55 | LOYALTY_AMOUNT | buff | +2 NP per kill |
| 150 | NO_RECALL | debuff | `canTeleport=false` (blocks summon/warp) |
| 151 | REDUCE_TARGET | debuff | enlarge, AC % |
| 152 | SILENCE_TARGET | debuff | no skills and no potions |
| 153 | NO_POTIONS | debuff | blocks class-0 HP heal items |
| 154 | KAUL_TRANSFORMATION | debuff | incapacitated, +500 AC, no `/town` |
| 155 | UNDEAD | debuff | heals → damage |
| 156 | UNSIGHT | debuff | caster blinded |
| 157 | BLOCK_PHYSICAL_DAMAGE | buff | physical damage = 0 |
| 158 | BLOCK_MAGICAL_DAMAGE | buff | magic damage = 0 |
| 159 | UNK_POTION | buff | no-op |
| 160 | SLEEP | debuff | no-op server-side |
| 163, 164, 165 | INVISIBILITY_POTION / GODS_BLESSING / HELP_COMPENSATION | buff | no-op |
| 48 (unnamed) | — | buff | |
| 166 (unnamed) | — | debuff | |
| other | — | debuff | logs a warning (1170) |

Grant/remove asymmetry: removing DAMAGE subtracts `bAttack` (or `bAttack−100`), so a debuff drops the value to 0. `SendItemMove` resets 0 back to 100 (`User.cpp` 3302-3303) [I].

---

## Table B — Ordered validation checks

**R hit (`AttackHandler.cpp`):**
| # | Check | Line |
|---|---|---|
| R1 | attacker not dead / blinded / blinking / Kaul | 15 |
| R2 | attacker not in a safety area | 18 |
| R3 | (stealth removed) | 21 |
| R4 | client delay ≥ weapon delay + 10, or ≥ 100 | 27, 32 |
| R5 | client distance ≤ weapon range (non-mage) | 28 |
| R6 | target exists | 38 |
| R7 | 2D distance ≤ 15 + weapon range | 39; `User.cpp` 5072-5075 |
| R8 | same zone, target alive and not blinking, hostile | 40; `Unit.cpp` 855-875 |
| R9 | NPC attackable | 42 |
| R10 | 1 R per second | 42; `Unit.cpp` 926-947 |
| R11 | temple room | 44 |
| R12 | target not frozen | 49 |
| R13 | damage > 0 → apply | 72 |

**Magic EFFECTING:** P0-P1 → C1-C8 → U1-U12 → I1-I7 → E1, as listed in §2.3 with line numbers.

**Type-specific (after E1):**
- T3: target alive and not blinking (1302); per-target `sRange` (1332); block-magic (1345).
- T4: same-type buff refusal (580-586, 1765); curse block/reflect (1718-1736); speed/stun resistance (1816-1843).
- T5: dead/alive matching (1939-1946).
- T8: alive, `canTeleport`, not warping, zone rules (2273-2362).

---

## Unknowns / runtime tests [R]

1. Ronark START_POSITION coordinates (DB).
2. Potion MAGIC rows (ID range vs 400000, `sReCastTime`, DirectType, Etc).
3. Weapon `m_sDelay` / `m_sRange` values.
4. `sSkillLevel` of level-80 skills (can it exceed level 80?).
5. Whether resisted slows/stuns still block speed buffs in play.
6. Which skill IDs the client actually uses for party summon (Type 8 warp 12 moral).
7. Mage armour `sSkill % 100` values.
8. Real wall-clock jitter of the 1-second `UNIXTIME` gates.
