# D: Stats, skill points, classes, ability formulas, items (code research)

Repo: Fire-Drake-Project-v1453 @ 0f520272 (`__VERSION 1453`). Read-only. Tags: **[V]** verified in code, **[I]** inferred, **[U]** unknown (needs a DB or runtime check). Paths are relative to the repo root unless noted. `GameServer/NPCHandler.cpp`, `QuestHandler.cpp` and `MerchantHandler.cpp` are ISO-8859 files, so plain `grep` treats them as binary and drops matches; use `grep -a`. Proc sources were read from the DB agent's dump (`scratchpad/research/db/procs/all/*.sql`). Player tables were not read.

---
## 1. Class / race / nation encoding

**Nation** [V] `shared/globals.h:35-41`: `KARUS=1`, `ELMORAD=2`.

**Class codes** [V] `GameServer/GameDefine.h:4-28`. Code = nation×100 + class type.
| type | 1 beginner W | 2 R | 3 M | 4 P | 5 W novice | 6 W master | 7/8 R nov/mst | 9/10 M nov/mst | 11/12 P nov/mst |
|---|---|---|---|---|---|---|---|---|---|
| Karus | 101 | 102 | 103 | 104 | 105 Berserker | 106 Guardian | 107/108 | 109 Sorcerer/110 Necromancer | 111 Shaman/112 Dark Priest |
| El Morad | 201 | 202 | 203 | 204 | 205 Blade | 206 Protector | 207/208 | 209 Mage/210 Enchanter | 211 Cleric/212 Druid |

`enum ClassType` 1..12, `GameServer/User.h:44-58`. Helpers in `User.h`:
- `GetClassType()` = `m_sClass % 100` (414-417).
- `GetBaseClassType()` maps 1..12 to 1..4 (393-407).
- `isWarrior/isRogue/isMage/isPriest()` call `JobGroupCheck(1..4)` (320-323).
- `isBeginner()` means type ≤ 4 (325-329). `isNovice()` means type 5/7/9/11 (336-341). `isMastered()` means type 6/8/10/12 (348-353).

`JobGroupCheck(id)` (`User.cpp:4615-4637`): if id > 100 the class must match exactly. Ids 1..4 are base groups. Any other id compares `subClass == id`, and since `subClass` is always 1..4, ids 5..12 never match.
- **Bug [V]:** `SetUserAbility` uses `CheckClass(6,12)` for the mastered-warrior/priest "Boldness" +20% AC at HP < 30% (`User.cpp:2272-2280`). It is therefore always false. Only the visual effect 106800 is shown (`HpChange`, `User.cpp:1948-1960`).

**Races** [V] `GameDefine.h:31-37`: 1 Arch Tuarek (Karus warrior only), 2 Tuarek (Karus rogue/priest), 3 Wrinkle Tuarek (Karus mage), 4 Puri Tuarek (Karus priest), 11 Barbarian (El Morad warrior only), 12 El Morad male, 13 El Morad female (all classes).

Valid combos for our three classes, [I] from the JobChange and NationChange mappings (`CharacterHandler.cpp:49-95, 171-432`):
- Karus: warrior race 1; mage race 3 (or 4, from the El Morad female mage transfer, line 84-85 / job change line 294-297); priest race 2 or 4.
- El Morad: warrior race 11/12/13; mage and priest race 12/13.

**The server does not validate race/class combos [V].** `NewCharToAgent` (`CharacterSelectionHandler.cpp:46-78`) checks only:
- `bCharIndex ≤ 2`
- a COEFFICIENT row exists for `sClass`
- STR+STA+DEX+INT+CHA ≤ 300
- each stat ≥ 50

`NEWCHAR_POINTS_REMAINING` (`globals.h:273`) is never used. `CREATE_NEW_CHAR` checks only nation against race (<10 Karus, >10 El Morad; proc lines 23-28).

**Job change paths [V]**
- Client opcodes `WIZ_CLASS_CHANGE` 1..5 (`shared/packets.h:523-527`). `ClassChange()` (`NPCHandler.cpp:156-285`) accepts from the client only REQ, stat reset (3), skill reset (4) and price query (5). The actual class change runs only with `bFromClient=false`. Allowed transitions: 1x1→1x5 or 1x6, 1x5→1x6, and the same for the other classes and 2xx.
- `ClassChangeReq()`: level < 10 means "too low", class type > 4 means "already done" (`User.cpp:3837-3847`).
- Promotion functions in `QuestHandler.cpp` add no level check:
  - `PromoteUserNovice()` (361-387): beginner → nation×100 + {5,7,9,11}.
  - `PromoteUser()` (390-~425): novice → +1, then `SaveEvent(baseClass, 2)`, which marks quest 1/2/3/4 done for W/R/M/P.
- **[V] None of the 114 runtime quest scripts (`C:\dev\fdp\server\Quests\*.lua`) call `PromoteUser*`.** In this sandbox there is no in-game first or second job change.
- The proc `LOAD_USER_DATA` (line 13) auto-promotes novice → master on login when `Level > 59` and Class ∈ {105,107,109,111,205,207,209,211}. It never promotes beginners.
- So a client-created character (class 1x1-1x4) stays a beginner at any level [V].
- `JobChange()` (`CharacterHandler.cpp:140-467`) is the cross-class change scroll. It needs `ITEM_JOB_CHANGE` 800560000, which is the same id as `ITEM_GENDER_CHANGE` (`Define.h:201-202`), and all 14 equipped slots empty. It keeps the tier and calls `AllPointChange(true)` and `AllSkillPointChange(true)`. Copy-paste bugs: the El Morad →mage beginner branch omits warrior (323); →priest omits warrior (365/385/398/408/418).

**Bot implication [I]:** bots must have their master class code (106/110/112 or 206/210/212) written into USERDATA.Class. Skills are class-exact (see 3).

---
## 2. Stat system

**Fields [V]**
- `uint8 m_bStats[5]` (`User.h:135`), indexed by `StatType` STR=0, STA=1, DEX=2, INT=3, CHA=4 (comment "// MP") (`globals.h:333-341`).
- DB columns (proc `UPDATE_USER_DATA`): Strong, Sta, Dex, Intel, Cha. Free points: `m_sPoints` (USERDATA.Points).
- `m_sStatItemBonuses[5]` holds item and set bonuses; `m_bStatBuffs[5]` holds buffs (`User.h:227-228`).
- Accessors in `User.h`:
  - `GetStat` = base (470-476).
  - `getStatTotal` = base + item + buff (536-539).
  - `GetStatWithItemBonus` = base + item (498-501).
- Client naming [I]: STR, HP (=STA), DEX, INT, MP (=CHA).

**Creation [V]:** sum ≤ 300 and each ≥ 50, as above. CREATE_NEW_CHAR (proc lines 49-50) inserts only name, nation, race, class, hair, face and the 5 stats. `GIVE_BEGINNER_ITEM` sets only strItem and Gold. **Neither sets Level, Points or strSkill.** There are no SQL triggers in the DB object list.
- **Starting level 51 is [U]:** most likely a USERDATA column DEFAULT on `Level` (also check the defaults for `Points`, `strSkill`, `Exp`). Check `sys.default_constraints` on USERDATA.
- The code side assumes level 1: starter items are given only if `Level==1 && Exp==0` (`DBAgent.cpp:487-...`).

**Points per level [V]** `LevelChange` (`User.cpp:1765-1826`) normal path (1782-1792):
- If `m_sPoints + statTotal < 297 + 3L + 2·max(0, L-60)`, add 3 (L ≤ 60) or 5 (L > 60).
- The "jump" path (1770-1781) is unreachable: `ExpChange` does `LevelChange(++m_bLevel)` (1696), so `level == GetLevel()`. The Lua `LevelChange` passes `bLevelUp=false` (`User.h:1397-1399`) and does not change the level.
- Invariant total T(L) = 300 + 3(L−1) + 2·max(0, L−60). It appears identically in `CanLevelQualify` (`CharacterMovementHandler.cpp:295-307`, whose zone call is commented out at line 190) and in the stat reset.

**Cap [V]:** `PointChange` (`User.cpp:1833-1851`) rejects when `m_sPoints < 1` or the base stat ≥ `STAT_MAX` 255 (`globals.h:352`). One point per packet; the type byte is 1..5.

**Stat reset [V]** `AllPointChange(bIsFree)` (`User.cpp:3908-4158`):
- Cost is `(int)pow(2L, 3.4)`: ×0.4 if L < 30, ×1.5 if L ≥ 60, halved under discount.
- Fails if any of the 14 equip slots is occupied (result 4), or if the stat total is already 290 (result 2).
- Sets the race/class preset (sum 290):
  - W 65/65/60/50/50
  - R 60/60/70/50/50
  - **P 50/50/70/70/50**
  - **M 50/60/60/70/50**
- **Quirk [V]:** for race KARUS_MIDDLE (2), non-warriors (including priests) get the rogue preset (3962-3978). KARUS_BIG with a non-warrior class sets nothing, and the ASSERT at 4137 would fail.
- Free points become `10 + 3(L−1) + 2·max(0, L−60)` (4130-4134).
- Exposed to Lua as `ResetStatPoints`, which is not free (`User.h:1331-1333`).

**Level-80 stat budget (arithmetic):**
- T(80) = 300 + 3·79 + 2·20 = 300 + 237 + 40 = **577 total**.
- After a reset: base 290 + **free 287** (10 + 237 + 40).
- Each stat ≤ 255, so for example a warrior can reach STR 255 (190 points) and put the remaining 97 into STA (65 → 162). This is arithmetic only, not a recommendation.
- **If** the character is created at L51 with total 300 and Points 0 [U: defaults], natural levelling gives only 300 + 3·9 (52-60) + 5·20 (61-80) = **427** at L80, which is 150 short. A stat reset (or writing Strong/Sta/Dex/Intel/Cha/Points to the DB) fixes this.
- Nothing recalculates or validates points on login (`SelectCharacter`, `CharacterSelectionHandler.cpp:134-223`).

---
## 3. Skill points

**Layout [V]:** `uint8 m_bstrSkill[10]` (`User.h:141`), saved as USERDATA.strSkill varchar(10).
- Index 0 = free points.
- 1-4 = legacy categories (unused).
- 5/6/7 = class trees (`SkillPointCat1-3` = `PRO_SKILL1-3`).
- 8 = master (`GameDefine.h:162-183`).
- Index 9 is never allocatable.
- `GetTotalSkillPoints` sums indices 0, 5, 6, 7, 8 (`User.h:541-546`).

**Per level [V]:** at L ≥ 10, +2 free points if total < 2(L−9) (`User.cpp:1790-1791`). Starting at level 10 the target is S(L) = 2(L−9).

**Allocation** `SkillPointChange` (`User.cpp:2971-3000`) [V]:
- Type 5..8 only, and free ≥ 1.
- Tree points + 1 ≤ **Level**.
- Requires class type > 4 (novice or master); **beginners can't allocate**.
- Master (8) also needs an even class code, class type ≥ 6, master points < `MAX_LEVEL−60` = 20, and master points < L−60. The code comment's "23 with level 83 cap" is stale.

**Reset** `AllSkillPointChange` (`User.cpp:3856-3906`) [V]:
- Same cost formula as the stat reset.
- Needs L ≥ 10 and at least one point already allocated in indices 1..8 (otherwise result 2).
- Sets free = 2(L−9) and clears 1..8.

**Level-80 budget:**
- S(80) = 2·(80−9) = **142**.
- Per-tree max **80** (= level).
- Master max **min(20, 80−60) = 20**.
- Example: 80 + 42 + 0 + 20 = 142, so two full trees are impossible (160 > 142).
- If created at L51 with 0 skill points and levelled naturally: 2·29 = 58 at L80. A reset gives 142 (only once ≥ 1 point is allocated) [I, U: strSkill default].

**Cast-time checks [V]:**
- `UserCanCast` (`MagicInstance.cpp:184-188`): if `MAGIC.Skill ≠ 0`, `m_sClass` must equal `Skill/10` **exactly** and Level ≥ `SkillLevel` (Chaos Dungeon is exempt).
- `IsAvailable` (`MagicInstance.cpp:951-958`): `modulator = Skill % 10`.
  - If ≠ 0, the class must match exactly and `SkillLevel ≤ m_bstrSkill[modulator]`. `COMMAND_CAPTAIN` is exempt.
  - Otherwise `SkillLevel ≤ Level`.
- DB dump confirms per-class duplicate rows: 1055-1057 for class 105, 1065-1068 for 106 [V data].
- Modulator-9 rows (e.g. "whipping", Skill 1019/1059/1069, SkillLevel 1) can never pass because index 9 is always 0 [V+data].
- Quest gate: if `sEtc ≠ 0` the caster needs quest `sEtc` state 2. GMs are exempt. The gate is compiled out in DEBUG builds (`MagicInstance.cpp:267-275`).
- `PromoteUser()`'s `SaveEvent(1..4, 2)` would satisfy `Etc = 1..4` for mastered characters [I]. This explains the setup note's `Etc=1` problem for characters never promoted.

Skill-tree passives read directly from the points [V]:
- Warrior tree 6 (`PRO_SKILL2`), defense +20/30/40/50/60%:
  - 60% needs ≥ 70 points and quest 51.
  - Resistance +30/60/90 at 10/20/40 points.
  - Both are halved with no shield (`User.cpp:2228-2269`).
- Every mastered class, master tree (`HpChange`, `User.cpp:1922-1930`):
  - ≥ 10 points: all damage taken ×0.85.
  - 5-9 points: ×0.90.

---
## 4. Ability formulas (`SetUserAbility`, `User.cpp:2082-2312`)

COEFFICIENT is keyed by the exact class code (`CoefficientSet.h`, loaded at `LoadServerData.cpp:163`). Fields: ShortSword, Sword, Axe, Club, Spear, Pole (unused), Staff, Bow, HP, MP, SP, AC, Hitrate, Evasionrate.

**Weapon damage D [V] (2092-2161)**
- Right hand: D = RH.Damage + `m_bAddWeaponDamage` (halved if durability is 0).
- Left hand, not a bow: + (LH.Damage + add)/2 (quartered if broken).
- Left-hand bow: D = bow damage.
- Minimum D is 3.
- `hitcoef` comes from the right-hand Kind/10: dagger→ShortSword, sword→Sword, axe→Axe, mace→Club, spear→Spear, bow/launcher→Bow, staff→Staff. It is 0 when empty-handed.

**Attack `m_sTotalHit` [V] (2166-2205, 2288-2289)**
- Inputs:
  - `S = baseSTR + itemSTR + buffSTR` (rogues use DEX total).
  - `baseAP = max(0, baseSTR−150)`, minus 1 if baseSTR == 160.
  - `addAP = 3 + baseAP` (non-rogues).
- Warrior **and priest**: `Hit = (uint16)(0.010·D·(S+40) + hitcoef·D·L·S)`, then `(Hit + addAP)·(100 + m_byAPBonusAmount)/100`.
- Rogue: 0.007 factor.
- Mage: `0.005·D·(S+40) + hitcoef·D·L`; the second term has no stat.
- +1 if a weapon-damage buff is active.
- Displayed attack = Hit × `m_bAttackAmount`/100 (`SendItemMove`, `User.cpp:3308`).
- Warrior attack therefore grows roughly linearly in STR_total and weapon Damage, times L and the class weapon coefficient.

**AC [V] (2207-2211, 2228-2269, 2291-2299)**
- `TotalAc = (short)(coef.AC·(L + m_sItemAc))·m_sACPercent/100`.
- Then the warrior passive %, +1 if an armour buff is active, and **+ (baseSTA−100) if baseSTA > 100**.
- `m_sItemAc` = Σ item Ac (÷10 if broken) + set ACBonus, then the weapon-AC buff (flat add, or else ×`m_bPctArmourAc`%) (`User.cpp:1332-1338, 1446-1449`).
- **[V]** `GetDamage` applies `m_sACPercent < 100` a second time (`Unit.cpp:240-241`), so AC debuffs are applied twice [I effect].

**Hit / evasion [V] (2213-2215)**
- `Hitrate = (1 + coef.Hitrate·L·DEXtotal)·itemHit/100·(m_bHitRateAmount/100)`.
- `Evasion` is the same with Evasionrate and `m_sAvoidRateAmount`.
- `itemHit` / `itemEva` = 100 + Σ item Hitrate/Evasionrate.
- **Integer division:** `m_bHitRateAmount` is uint8 (`Unit.h:240`) and `m_sAvoidRateAmount` is short (`Unit.h:241`). Values < 100 give 0, 100-199 give 1, 200+ give 2. So accuracy debuffs zero the stat and buffs below 200% do nothing [V arithmetic; I effect].
- The hit table maps hit/eva ratio to great/success/normal/fail bands (`Unit::GetHitRate`, `Unit.cpp:718-805`).

**Max HP [V]** `SetMaxHp` (`User.cpp:1036-1071`)
- `HP = (short)(coef.HP·L²·STAt + 0.1·L·STAt + STAt/5 + m_sMaxHPAmount + m_sItemMaxHp + 20)`, where STAt = `getStatTotal(STA)`.
- Capped at 14000 (`MAX_PLAYER_HP`, `Define.h:22`) for non-GMs.
- Snow war: 100 (captain/king 300). Chaos Dungeon: 1000.
- At L80: `coef.HP·6400·STAt + 8·STAt + ⌊STAt/5⌋ + 20 + bonuses`.

**Max MP [V]** `SetMaxMp` (1076-1103)
- If coef.MP ≠ 0 (casters): `MP = coef.MP·L²·I + 0.2·L·I + I/5 + mpAmt + itemMP + 20`, with I = INTtotal + 30. At L80 the middle term is 16·I.
- Otherwise, if coef.SP ≠ 0: the same shape on STAt with coef.SP and no +20.
- No cap.

**Magic damage [V]** `MagicInstance::GetMagicDamage` (`MagicInstance.cpp:2558-2717`), used for type-3 attack spells with `sFirstDamage < 0`, DirectType 1/8, skill id < 400000 (1337-1340):
- `total_hit = sFirstDamage·baseCHA/186`. **This is base CHA only; item CHA doesn't count.**
- `sMagicAmount = max(0, baseCHA−86) + m_sMagicAttackAmount`, where the buff sets `m_sMagicAttackAmount = (bMagicAttack−100)·baseCHA/100` (`MagicProcess.cpp:361-364`).
- `dmg = 230·total_hit/(R+250)`; `dmg = 0.3·rand + 0.85·dmg − sMagicAmount`.
  - R = target elemental resistance + `m_bResistanceBonus`.
  - The values are negative, so subtracting adds damage.
- Staff, non-magic attribute, no left-hand item: subtract `RH.Damage·0.8 + RH.Damage·L/60`, plus item elemental bonuses reduced by R/200.
- `÷3 vs players`, cap 32000.
- `GetWeatherDamage`'s return value is discarded (2707), so it has no effect.
- **Heals are flat** `sFirstDamage`, not stat-scaled (1341-1342). "Critical Point" can double them (1392-1394).

**Resistances [V]**
- Item and set R plus buff add, times the debuff %.
- `m_bResistanceBonus` = warrior passive + rogue Valor + **(baseINT−100)/2 if INT > 100** (2301-2303).

**Weight [V]**
- `MaxWeight = ((STR+itemSTR+L)·50 + bag/set bonus)·(m_bMaxWeightAmount≤0 ? 1 : m_bMaxWeightAmount/100)` (2179), clamped to 32767 (1022-1028).
- **`m_bMaxWeightAmount` is never initialised in `CUser::Initialize`** (only set by a weight buff, `MagicProcess.cpp:373/729`). A garbage value of 1-99 would make MaxWeight 0 [I, U runtime].
- Weight is checked only on acquire (buy/trade/exchange/`CheckWeight`), never on equip (`ItemHandler.cpp:558` is commented out) and never in combat.

**Speed:** there is no server-side speed formula. `m_bSpeedAmount` is stored only. The R-hit throttle is 1 s (`Unit.cpp:926-947`) plus a client-reported weapon Delay/Range check (`AttackHandler.cpp:24-33`).

**Physical damage (context) [V]** `CUser::GetDamage` (`Unit.cpp:208-394`)
- `hitB = (TotalHit·m_bAttackAmount·2)/(AC+240)`, using PvP `m_bPlayerAttackAmount%` and set-class AP/AC %.
- R-hit: `0.85·hitB + 0.3·rand(0,hitB)`. Type-1 skill: `hitB·sHit/100` (+0.3·rand).
- Then item elemental/drain bonuses (`Unit::GetMagicDamage`) and weapon-type AC (`dmg·R/200`).
- **÷2 for players** (both branches, 383-386). Cap 32000.
- Bug [V]: HP-drain items call `pTarget->HpChange(+x)`, which heals the target (`Unit.cpp:629-631`), and return early (655-660).

**Regeneration [V]** (`HPTimeChange`, `User.cpp:3325-3376`, every > 5 s):
- Standing: MP only, `((L(1+L/60)+1)·0.2+3)` = 40 at L80.
- Sitting:
  - HP `L(1+L/30)+3` = 296 at L80.
  - MP `5·MaxMP/(L+29)+3`.

---
## 5. Level

- **Constants [V]:** `MAX_LEVEL 80` (`Define.h:21`). On login, level > 80 disconnects (`CharacterSelectionHandler.cpp:197-201`).
- **LEVEL_UP table [V]:** `[Level],[Exp]` (`LevelUpTableSet.h`), read via `GetExpByLevel` (`GameServerDlg.cpp:424`). `m_iMaxExp = Exp(L)`.
- **ExpChange [V]** (`User.cpp:1634-1712`):
  - Gains are scaled by buff/item %, the global event and premium.
  - At most **one level per call** (returns after `LevelChange`). At L80, exp is capped.
  - No loss if L < 6 or in a war-zone map.
  - Deleveling is possible.
- **Exp loss on death [V]** (`OnDeath`, `User.cpp:4727-4900`):
  - Killed by an NPC: 1% of MaxExp if it was a patrol guard or the victim is in the enemy home zone (1/2); otherwise **5%**. Forgotten Temple has no loss.
  - Killed by a player: loss only in the enemy home zone, 1%. **No exp loss in Ronark Land/Ardream PvP.**
  - Premium `ExpRestorePercent` multiplies the loss.
- **Setting a level [V]:**
  - There is no level GM command. `+exp_change <name> <exp>` (`ChatHandler.cpp:653-682`, GM only, `atoi`, so ≤ 2³¹−1 per call) levels **one level per call**.
  - Lua `LevelChange` doesn't change the level.
  - Practical route [I]: edit USERDATA Level/Class/stats/Points/strSkill offline. Nothing re-validates them on login.
  - Deleveling or a DB edit never unequips items.
- **Zone kick [V]:** if the level is outside the zone range (`User.cpp:1823-1825`). Ronark Land is 35-80, Ardream 35-59 (`Unit.cpp:1100-1108`).

---
## 6. Items

**`_ITEM_TABLE`** (`GameDefine.h:298-386`) and loader columns (`ItemTableSet.h:10`) [V]:
- Num, strName, Kind, Slot, Race, Class, Damage, Delay, Range, Weight, Duration, BuyPrice, SellPrice, Ac, Countable, Effect1, Effect2
- ReqLevel, ReqLevelMax, ReqRank, ReqTitle, ReqStr/Sta/Dex/Intel/Cha
- SellingGroup, ItemType, Hitrate, Evasionrate, Dagger/Sword/Mace/Axe/Spear/BowAc
- Fire/Ice/Lightning/PoisonDamage, HPDrain, MPDamage, MPDrain, MirrorDamage
- StrB/StaB/DexB/IntelB/ChaB, MaxHpB, MaxMpB, Fire/Cold/Lightning/Magic/Poison/CurseR
- ItemClass, ItemExt

Helpers:
- `GetItemGroup` = Kind/10. 2H kinds are 22/32/42/52. Shield group 6, staff group 11, bow groups 7/8.
- `is2Handed` = Slot 3 or 4.

**Slots [V]**
- Equip slot indices: 0 R-ear, 1 head, 2 L-ear, 3 neck, 4 breast, 5 shoulder, 6 RH, 7 waist, 8 LH, 9 R-ring, 10 leg, 11 L-ring, 12 glove, 13 foot (`globals.h:193-206`).
- `SLOT_MAX` 14, `HAVE_MAX` 28 inventory, `COSP_MAX` 5, 2 magic bags × 12; `INVENTORY_TOTAL` = 73; `WAREHOUSE_MAX` 192 (226-253).
- `ItemSlotType` maps ITEM.Slot to positions (`GameDefine.h:60-82`), validated in `IsValidSlotPos` (`ItemHandler.cpp:1058-1205`).

**Equip validation [V]**
- `ItemMove` (`ItemHandler.cpp:541-743`) calls `ItemEquipAvailable` (527-539): `ReqLevel ≤ L ≤ ReqLevelMax`, `m_bRank ≥ ReqRank` (rank 1 = king only, `User.cpp:903-910`), `m_bTitle ≥ ReqTitle`, and the **base** stats ≥ ReqStr/Sta/Dex/Intel/Cha.
- **No race and no class check on equip** server-side. `m_bRace` is used only for set ids (≥ 100) and tradeability (19/20); `m_bClass` only for item-using skills.
- Equip is blocked while trading, merchanting, mining or in Chaos Dungeon. Duplicate-flagged items can't be equipped.

**2H/shield [V]:** `IsValidSlotPos` does not reject.
- A 2H item into a wrong slot, or with the other hand occupied, **swaps LH↔RH and returns true** (1082-1109).
- A 1H item with a 2H in the other hand also swaps (1191-1202).
- Equipping a 2H while holding 1H + shield ends with the 2H in RH, the old 1H in LH and the shield in the bag [I trace].
- **Bots should empty both hands before equipping a 2H.**

**Requirements no longer met [V]**
- Stat reset and job change require all 14 slots empty (`User.cpp:3930-3936`, `CharacterHandler.cpp:151-166`).
- Deleveling, a DB edit or a stat debuff (`m_bStatBuffs`) never unequip anything.
- `SetSlotItemValue` applies the full bonuses with no requirement check (`User.cpp:1274-1450`).

**Bonuses applied [V]** (`SetSlotItemValue`, `User.cpp:1274-1450`):
- Equipped (0-13), cospre (42-46) and bag (47-48) slots.
- Max HP/MP, Ac, stat bonuses, hit/eva, elemental R, weapon-type AC.
- Elemental/drain/mirror damage maps.
- Weight is summed over all 73 slots; bags add their Duration as max-weight bonus.

**Durability [V]**
- Wear is `sqrt(dmg/10)` per R-hit (`AttackHandler.cpp:81-85`, `ItemWoreOut` `User.cpp:3202-3289`). It hits RH/LH weapons (shields skipped) and armour head/breast/leg/glove/foot. Accessories are fixed at their table duration on load (`DBAgent.cpp:454`).
- At 0: AC ÷10, weapon damage ÷2 (LH ÷4).
- Repair `ItemRepair` (`NPCHandler.cpp:8-70`):
  - The NPC range check is commented out.
  - Cost `((Buy−10)/10000 + Buy^0.75)·missing/maxDur`, with the premium discount.
  - Refused if `SellPrice == 2` (enum `SellTypeNoRepairs` compared with SellPrice) [V; I: probably meant SellingGroup].
- Magic Hammer is type-3 DirectType 4 → `REPAIR_ALL` (armour only).

**Countables [V]**
- `Countable≠0` stacks to 9999 (`ITEMCOUNT_MAX`, `globals.h:258`) in the 28 bag slots only (`FindSlotForItem`, `User.cpp:3637-3677`). `GiveItem` caps at 9999 (`ItemHandler.cpp:485`).
- Kind 255 "scrolls" use Duration as a use-count (`RobItem` 408-420, `GiveItem` 504-505).
- On load, a non-countable item with count > 1 becomes 1 (`DBAgent.cpp:444-447`).

---
## 7. Upgrades and sets

**+N [V]:** `ItemUpgrade` (`UpgradeHandler.cpp:67-516`).
- Matches an ITEM_UPGRADE row (`GameDefine.h:460-471`: nIndex, sNpcNum, bOriginType, sOriginItem, nReqItem[8], nReqNoah, bRateType, sGenRate, nGiveItem). Matching uses:
  - `sOriginItem == itemID % 100000`
  - kind filters by bOriginType (282-362)
  - scroll lists per bRateType (141-263), compared with `ITEM.ItemClass`
- Success: `GenRate ≥ myrand(0, myrand(9000,10000))`, +20% with Trina (700002000/379258000) (411-419).
- **New item id = old id + nGiveItem** (440).
- Failure deletes the item (425) and still charges coins.
- A "+N" item is a separate ITEM row whose own Damage/Ac/bonuses apply. There is **no upgrade formula in code** [V]. The "last digit = +N" id convention is [I].
- Minor bug: `memset(pItem, 0, sizeof(pItem))` (487).

**Sets [V]** (`User.cpp:1399-1444`):
- For items with Race ≥ 100, the set id = Race·10000 + Σ slot bits (helmet 2, pauldron 16, pads 512, gloves 2048, boots 4096).
- Bonuses apply only if that exact combination id exists in SET_ITEM.
- Cospre (Kind 252) items look up SET_ITEM by their own item Num (1391-1397).
- `ApplySetItemBonuses` (1452-1485) adds AC, HP, MP, 5 stats, 6 resists, XP%, coin%, NP, max weight, AP% and class-specific AP%/AC%. The class-specific ones apply in PvP against the target's base class (`Unit.cpp:256-257`).

**Procs [V]:** ITEM_OP gives an RH/LH skill proc on PvP physical attack, or an LH proc on defend, at `bTriggerRate`%. Broken items don't proc (`Unit.cpp:397-471`).

---
## 8. Item use (potions, consumables)

**Flow [V]**
1. The client sends `WIZ_MAGIC_PROCESS` with a MAGIC id. The caster id must equal the socket (`MagicProcess.cpp:16-53`).
2. `MagicInstance::Run` (`MagicInstance.cpp:10-147`) calls `CheckSkillPrerequisites` (295-438): cooldown `ReCastTime·100 ms` per skill; a same-type 0.7 s lock only for ids < 400000; no casting in the safety area for ids < 400000.
3. `UserCanCast` (149-293):
   - MP check.
   - **`CanUseItem(MAGIC.UseItem)`** (`User.cpp:5087-5111`): JobGroupCheck(ITEM.Class), **ReqLevel ≤ L ≤ ReqLevelMax**, and the item is present in slots 0..43.
   - The sEtc quest gate.
4. `IsAvailable` (794-1054) blocks healing pots under No-Potion.
5. Execute type 3/4.
6. `ConsumeItem` → `RobItem(UseItem)` (2978-2996). Arrow-like ids (370001000-370003000, 379063000-379070000) are not consumed.

- Class stones: if `BeforeAction` is 1..4, the consumed item is 379058000 + 1000·BeforeAction and UseItem becomes only a requirement (`MagicInstance.cpp:251-254`, `MagicInstance.h:61`).
- The **ITEM.Effect1→MAGIC link** is used server-side only for buff-duration extension scrolls (2908-2925). For potions, the client picks the MAGIC id [I] and the server validates via MAGIC.UseItem.
- `MAGIC.ItemGroup` is loaded but unused [V].
- **[U]** If potions have ReqLevelMax < 80 (or 0), they fail for level-80 bots. Check the ITEM rows.

---
## 9. NP / rank / premium / GM effects

- **No NP- or rank-based stat bonuses** in `SetUserAbility` [V].
- King (`m_bRank=1`) only affects ReqRank items and snow-war HP 300 (`User.cpp:1045`).
- NP > 0 is required to enter Ronark Land/Ardream (`CharacterMovementHandler.cpp:239-271`).
- Premium (`GetPremiumProperty`, `User.cpp:1717-1756`) changes exp%, noah%, drop%, bonus NP per kill (671-677), repair discount, sell %, exp-loss %. It has **no combat effect** [V].
- Combat-relevant DB-driven bonuses: SET_ITEM AP%/AC% (including class-specific), ITEM_OP procs, type-4 buffs (`GrantType4Buff`, `MagicProcess.cpp:268-631`).
- **GM (Authority 0) [V]:**
  - takes no damage (`User.cpp:1881-1882`)
  - no HP cap (1058)
  - skips quest gates (`MagicInstance.cpp:271`)
  - full regen when sitting (3362-3366)
  - **Bots and test opponents must be Authority 1.**

---
## Missing data to finish the numbers [U]
1. USERDATA column defaults for Level (51?), Points, strSkill, Exp, Class handling.
2. COEFFICIENT rows for 106/110/112/206/210/212, needed for HP/MP/AC/attack numbers.
3. LEVEL_UP Exp(51..80).
4. ITEM rows for the intended gear: Damage, Ac, Req*, ReqLevelMax, Slot/Kind, bonuses, the upgrade chain ids. Also potions' ReqLevelMax and Class.
5. SET_ITEM / ITEM_OP for that gear.
6. Quest states for master-skill `Etc` 510-523, and quest 51 (warrior defense 60%).
7. Runtime check of `m_bMaxWeightAmount` and the hit-rate integer-division effects.
