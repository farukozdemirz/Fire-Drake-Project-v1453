# Warrior / Priest / Mage skills (FDP_kn_online MAGIC + MAGIC_TYPE1..9), read-only extract

Full data: skills_warrior.csv (214 rows), skills_priest.csv (374), skills_mage.csv (332): every MAGIC column + the matching MAGIC_TYPEn columns (prefixed T1_..T9_) + derived nation / class_code / tree_cat / req_kind.

## 1. How skill ids encode nation / class / tree (verified in data and GameServer code)

* **MagicNum = N CC T LL** (6 digits) for player class skills:
  * `N` (1st digit) = nation: 1 = Karus, 2 = El Morad.
  * `CC` (digits 2-3) = class type 01..12 (`ClassType` enum in GameServer/User.h): 01 warrior, 02 rogue, 03 mage, 04 priest (beginner), 05/06 warrior novice/master, 07/08 rogue novice/master, 09/10 mage novice/master, 11/12 priest novice/master. So **MagicNum/1000 = the full class code** (`USERDATA.Class`): warriors 101/105/106/201/205/206, mages 103/109/110/203/209/210, priests 104/111/112/204/211/212.
  * `T` (4th digit) = tree/category: 0 = basic (level-gated) skills, 5 = tree 1, 6 = tree 2, 7 = tree 3, 8 = master tree. `LL` = required amount (level for T=0, skill points for T=5..8). Examples: 106560 = Karus master warrior, tree 5 (Attack), 60 points ("sword dancing"); 212545 = El Morad master priest, tree 5 (Heal), 45 points ("Superior healing"); 110774 = Karus master mage, tree 7 (Lightning), 80 points ("Blink").
  * Exceptions: x010/x036/x009 "whipping" (horse) rows use Skill = class*10+9; 190573/190673/190773 and 290xxx (Fire/Ice/Lightning Armor with Skill 1905/1906/1907) belong to non-existent class 190/290 (unusable duplicates); 3xxxxx = NPC/monster/pet skills, 45xxxx-47xxxx transformations/siege, 48xxxx resurrection scrolls, 49xxxx/50xxxx item (potion/scroll) skills.
* **MAGIC.Skill = class code * 10 + category** (e.g. 1065 = class 106, category 5) and **MAGIC.SkillLevel = requirement**. Server checks (GameServer/MagicInstance.cpp):
  * `UserCanCast()`: if Skill <> 0 then `m_sClass == Skill/10` (exact class code: a 106 cannot cast 105xxx rows and vice versa - each class code has its own full copy of the tree) and `Level >= SkillLevel` (outside Chaos Dungeon).
  * `IsAvailable()`: `modulator = Skill % 10`; if modulator <> 0: class must match again and `SkillLevel <= m_bstrSkill[modulator]` (points spent in that category; COMMAND_CAPTAIN fame bypasses); if modulator == 0: `SkillLevel <= Level`.
  * Skill points (User.cpp): free points `m_bstrSkill[0] = (Level-9)*2` = **142 at level 80**; categories 5/6/7 can each hold up to Level (80); category 8 (master) only for mastered classes (class%2==0 and class%100>=6) and max Level-60 (= 20 at level 80). Allocation requires a job-changed class (class%100 > 4). USERDATA.strSkill is the 10-byte array (byte0 free, byte5..8 = cat 5..8).
  * Etc <> 0: Etc is a quest id that must be completed (`CheckExistEvent(Etc, 2)`, Release builds only, GMs exempt). After the local fix only Etc 510-523 remain (see magic_etc_distribution.md).
* **Class progression matters for bots**: CREATE_NEW_CHAR stores the class the client sends (beginner 101/103/104/201/203/204); LOAD_USER_DATA auto-promotes novice -> master (105->106, 109->110, 111->112, ...) only when Level > 59, and never promotes beginner classes. A level-80 PK bot therefore needs Class = 106/206 (warrior), 110/210 (mage), 112/212 (priest) to use the tables below (beginner 1xx codes only have the handful of T=0 basic skills and cannot spend skill points).

## 2. Units and semantics of the columns used below

* `Recast` = MAGIC.ReCastTime in **0.1 s** (server: fail if `now_ms - last_ms < ReCastTime*100`; `now` is UNIXTIME seconds, so effective resolution is 1 s). Per-skill cooldown list `m_CoolDownList`.
* Additional **per-type gate**: after a successful cast, Type1 and Type2 are stored in `m_MagicTypeCooldownList`; another skill of the same type within `PLAYER_SKILL_REQUEST_INTERVAL` (0.7 s, compared on whole seconds -> effectively "not in the same second") fails, for skills < 400000. Type-3 skills with Attribute 0 (potions, heals) clear that gate.
* `Cast` = CastTime (client-side cast bar, 0.1 s units presumably; not checked by the server). `SR` = SuccessRate: only used server-side for the stun chance of lightning (Attribute 3) Type-3 damage on players. `Range` = meters; checked only for casts with a target (`dist >= Range` fails when UseStanding=0; when UseStanding=1 the caster must not be moving and the range is checked at MAGIC_CASTING). Type1/2/3 attack skills are also limited by `isInAttackRange` (15 m + skill range (or weapon range) [+ weapon range for Type1]).
* `MP` = Msp (mana cost; taken in IsAvailable() at MAGIC_EFFECTING; for Type-4 casts with an explicit target it is taken in ExecuteType4 instead - once per buffed target), `HP` = HP cost (HP>=10000 means "Sacrifice": costs 10000 HP).
* Moral: 1 SELF, 2 FRIEND_WITHME, 3 FRIEND_EXCEPTME, 4 PARTY, 6 PARTY_ALL, 7 ENEMY, 10 AREA_ENEMY, 11 AREA_FRIEND, 13 SELF_AREA, 25 CORPSE_FRIEND.
* Type details: T1 hit% = damage multiplier (`TotalHit * Hit/100`), add = flat AddDamage (divided by 3 vs players outside war zones, by 2 in war zones), hitrate (abs) = absolute success % / (rel) = relative to hit/evasion. T3: HP first<0 = damage, >0 = heal; overtime = DoT/HoT total over dur seconds (ticks every 2 s); attr fire/ice/lightning/magic; radius in meters. T4: BuffType name + changed fields (baseline 100 for AttackSpeed/Speed/ACPct/Attack/MagicAttack/MaxHPPct/MaxMPPct/HitRate/AvoidRate/ExpPct, 0 for the rest; Speed=50 = half speed, Speed=1 = rooted). T5: type 1 cure disease(DoT), 2 cure curse(debuffs), 3 resurrection (NeedStone x UseItem), 4 self-resurrect scroll. T7/T9: monster control / stealth. T8: see section 4.
* Passive rows (Type1=Type2=0) have no type row; their effect is applied by stat code / client (descriptions give the values).

## 3. Notable PvP skills (master classes, Karus id; El Morad = id + 100000)

### Warrior (106 / 206)
* Repeatable melee attacks (Type1, Moral ENEMY, no item): sword aura 106557 (57 pts, 100% +250, recast **0.1 s**, 250 MP), hoodwink 106505 / pierce 106515 / Carving 106525 / prick 106535 / Cleave 106545 / thrust 106555 / sword dancing 106560 (recast **0.5 s**; 100-200% (sword dancing 150% +150), MP 30-300), Howling Sword 106570 (200% +200, **0.8 s**, 400 MP), Hell blade 106580 (310% +300, 0.8 s, 400 MP, Etc 511), blooding 106575 (350% +400 + a Type3 DoT of -1000 over 20 s, recast 21 s, Etc 510); 3.1 s group: slash/crash/piercing (basic), Hash/Shear/Sever/multiple shock/mangling. Because of the per-type gate only one Type-1 skill per second actually lands, so a realistic rotation is ~1 Type-1 skill/s (e.g. sword aura / Howling Sword / Cleave alternated) plus R-hits (`PLAYER_R_HIT_REQUEST_INTERVAL` 1.0 s).
* AoE: Quake 106760 (Type3 AREA_ENEMY radius 10, -500 first / -200 end, recast 5.1 s, 160 MP). Ranged: Blade of hate 106725 (-150 magic, 45 m, 5.1 s).
* Debuff / CC: leg cutting 106520 (Type1+T4 Speed 50 for 10 s, recast 5.1 s); Scream 106802 (master 2 pts, 250% +200 + **root Speed=1 for 7 s**, needs Scream Scroll 379063000 kept + consumes Stone of Warrior 379059000, recast 10.1 s); Shock Stun 106820 (master 20, 175% +175 + lightning stun, consumes Stone of Warrior, recast 25.2 s); Exceed Break 106815 (200% + durability damage 1000, stone, 25.4 s). Binding 106630 / provoke 106645 are monster-taunt (T7).
* Buffs: sprint 106001 (Speed 150% 10 s, recast 6 s), Defense 106007 (+50 AC 15 s, 10 s), Gain 106705 (+15 STR 300 s), Rise 106715 (+10 STA), Nimble Wind 106735 (+20 DEX), Outrage 106720 / Frenzy 106755 / berserk Echo 106770 (attack speed 120/130/140% for 30 s; recast 9.1/9.1/10.1 s), Berserker 106775 (AC -300, Attack 120%, 30 s, Etc 510), wall of Iron 106675 (AC x3, Speed 50%, 10 s, Etc 510), restoration 106730 / Regeneration 106750 (HoT 750/1500 over 60 s), Return to life 106740 (+250 HP for 500 MP), pain killer 106710 / Blaze Killer 106731 (100/200 HP -> 200/400 MP), HP Booster 106780 (Etc 511), sacrifice 106660 (heal party member 10000 for 10000 HP), descent 106650 (teleport to party member). Passives: Hinder/Arrest/Bulwark/evading/Iron Skin/iron linker (+10..40% defense with shield), resist/endure/immunity (+30/60/90 all resist), master boldness/Absoluteness/Matchless.

### Priest (112 / 212)
* Heals (Type3 DirectType 1, FRIEND_WITHME unless noted, cast 15, recast 0.1 s unless noted): minor healing 112500 (60), healing 112509 (120), major healing 112518 (240), Great healing 112527 (**960** - description says 720; recast 2.0 s), Massive healing 112536 (960, 160 MP), **Superior healing 112545 (1920 HP, 320 MP, 45 pts)**, Complete healing 112554 (10000 = full, 960 MP, recast 5.4 s); group: Group massive healing 112557 (PARTY_ALL radius 30, 960, 960 MP, 5.4 s), Group complete healing 112560 (10000, 1920 MP, 6.4 s), critical restore 112570 (HoT 3000/20 s party), Past Recovery 112575 (Etc 520) / Past Restore 112580 (Etc 523). HoTs: Light restore 112503 ... Superior restore 112548 (2500 over 30 s).
* Cures: Cure curse 112525 (T5 type 2 = remove debuffs, recast 1.5 s), Cure disease 112535 (T5 type 1 = remove DoTs), Bless of God 112671 (party cure curse). Resurrection: Resurrection of love 112733 / grace 112742 / favors 112754 (CORPSE_FRIEND, 400/600/800 MP, Stone of life 379006000 x 4/10/30 taken from the corpse, recast 25 s).
* Buffs (600 s): AC line Insensibility Skin 112603 (+20) -> shell 112612 (+40) -> armor 112621 (+80) -> shield 112630 (+120) -> barrier 112639 (+160) -> protector 112651 (+200) -> **peel 112660 (+300)** -> Round Insensibility 112673 (party +300, Etc 519) -> Insensibility Guard 112674 (+350, Etc 521). HP line (PARTY target): Grace 112606 (+60) -> Brave (+120) -> Strong (+240) -> Hardness (+480) -> Mightness (+960) -> Heapness 112655 (+1200) / Greatness 112656 (PARTY_ALL +1200) -> **massiveness 112657 (+1500)** -> imposingness 112670 (+2000) -> Massive Binder 112672 (party +2000, Etc 518) -> Superioris 112675 (+2500, Etc 522); Undying 112654 (MaxHP 160%). Resist line: Resist all 112609 (+20 magic/disease/poison) -> Bright mind (+40) -> Calm mind (+60) -> **Fresh mind 112645 (+80)**. Self STR: blasting/wildness/eruption (+30 STR). Counter Curse 112676 (block curses 10 s, Etc 523), master Curse Refraction 112820, Elysian Web 112825.
* Debuffs (ENEMY, 150 s, recast 7.4 s unless noted): **Malice 112703 (AC 75%)**, Confusion 112715 (CHA -30), **Slow 112724 (attack speed 70%)**, Reverse life 112727, **Parasite 112745 (MaxHP 80%)**, **Torment 112757 (AREA_ENEMY radius 10, AC 70%, 9.4 s)**, **Massive 112760 (Attack 80%, 10.4 s)**, Subside 112770 (AoE attack 80%, 15.4 s), Superior Parasite 112771 (MaxHP 70%, Etc 520); MP burn Clear mana 112709 (-480 MP) / Sweep mana 112736 (-960) / Discountis 112772 (AoE -3840, Etc 523); Sleep Wing 112730 / Sleep Carpet 112751 (T7 sleep, 20 s). Gate 112700 (town). Melee Type1 strikes: tilt, Bloody, raving edge, Hades, collapse, Harsh, ruin, Hellish, Judgment 112802 (500% +150), Helis 112815 (400% +400).

### Mage (110 / 210)
* Elemental single-target nukes (Type3, ENEMY, SR 30): Fire: Burn 110503, Fire ball 110515, Fire spear 110527, Fire blast 110535 (needs Spell of Fire Blast 370001000, not consumed), Hell fire 110539 (-480 + DoT -1120), Pillar of fire 110551 (-1260), Fire Thorn 110554 (HP-drain -1550, Spell of thorn + Stone of Mage), Manes of fire 110556, Fire Impact 110557, **incineration 110570 (-2500, recast 21.3 s)**, Igzination 110575 (-4500, Etc 517), Vampiric Fire 110574 (MP drain -3000, Etc 517), Fire Staff 110572 (Etc 515). Ice: Freeze/Chill/Ice arrow/Solid/Ice orb/Ice blast/Frostbite/Ice comet (-882, slow to 32%)/Ice Impact/**Prismatic 110670 (-1750 + slow)**, Ice Staff 110672. Lightning: Charge/Counter spell/Lightning/Static hemisphere/Thunder/Thunder blast/Discharge/Static orb/Static Thorn/Thunder Impact/**Stun Cloud 110770 (-1750)**, Light Staff 110772. Lightning damage on players can stun (SuccessRate vs LightningR).
* AoE (AREA_ENEMY radius 15 unless noted, recast 15.3 s unless noted): Fire burst/Ice burst/Thunder burst (radius 8, recast 0.1 s, -588/-412/-412), Inferno 110545 / Blizzard 110645 / Thundercloud 110745, **Supernova 110560 / Frost nova 110660 / Static nova 110760 (-1800/-1260/-1260 + DoT)**, meteor Fall 110571 / ice storm 110671 / Chain lightning 110771 (recast 18.3 s).
* Crowd control: every ice spell carries a T4 Speed debuff (Freeze 50%, Ice comet 32%, Frost nova / ice storm 30% for 20 s); **Freezing Distance 110674 (BUFF_TYPE_FREEZE, Speed=1 for 15 s, Etc 517)**; Light Shock 110762 (AoE DISABLE_TARGETING 5 s); Minor Resist 110825 (AoE -20 all resist, master 20). Self: Mana Shield 110815, Instantly Magic 110820 (next cast instant), Absolute power 110802 (MagicAttack 130%, 30 s), Fire/Ice/Lightning Armor 110573/110673/110773 (Etc 516), resist/frozen-armor buffs.
* Teleport/summon (Type8): summon friend 110004 (WarpType 12, summon party member in same zone, 22500 range), Gate 110015 (town), Escape 110035 (party to town - **blocked in zones > 31, i.e. in Ronark**), Blink 110774 (WarpType 20, 20 m forward, recast 21.3 s, Etc 517).

## 4. All MAGIC_TYPE8 rows (32) with sub-type fields

ExecuteType8 (MagicInstance.cpp): WarpType 1 = send target to its resurrection point (Escape 109035/110035/209035/210035 fail in zones > 31 = Bifrost, i.e. Ronark Land too); 12 = summon (Call Party 490050 / clan 490042 blocked in zones > 31); 20 = blink; 25 = teleport to target; 21 = monster summon; 2/3/5 are TODO in the server. Target fails if it is dead (except 11), already warping or has teleport blocked.

| iNum | Name | Description | Target | Radius | WarpType | meaning | ExpRecover | KickDistance | Skill | SkillLevel | Moral | MP | Cast | Recast | Range | Etc | UseItem |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 105650 | descent | Teleport to the location of a chosen party member | 1 | 30 | 25 | teleport to target (party member for Descent, enemy within Radius for Wild Advent) | 0 | 0 | 1056 | 50 | 4 | 50 | 0 | 91 | 225 | 0 | 0 |
| 106650 | descent | Teleport to the location of a chosen party member | 1 | 30 | 25 | teleport to target (party member for Descent, enemy within Radius for Wild Advent) | 0 | 0 | 1066 | 50 | 4 | 50 | 0 | 91 | 225 | 0 | 0 |
| 108770 | Wild advent | Teleports to an enemy that is within a certain distance from you. | 1 | 40 | 25 | teleport to target (party member for Descent, enemy within Radius for Wild Advent) | 0 | 0 | 1087 | 70 | 7 | 130 | 0 | 250 | 112 | 0 | 0 |
| 109004 | summon friend | Retrieve a party member | 1 | 10000 | 12 | summon target, same zone | 0 | 0 | 1090 | 4 | 4 | 5 | 15 | 1 | 22500 | 0 | 0 |
| 109015 | Gate | Teleport to the chosen resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 1090 | 15 | 1 | 30 | 15 | 100 | 56 | 0 | 0 |
| 109035 | Escape | Teleport all party members to your resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 1090 | 35 | 6 | 400 | 15 | 250 | 22500 | 0 | 0 |
| 110004 | summon friend | Retrieve a party member | 1 | 10000 | 12 | summon target, same zone | 0 | 0 | 1100 | 4 | 4 | 5 | 15 | 1 | 22500 | 0 | 0 |
| 110015 | Gate | Teleport to the chosen resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 1100 | 15 | 1 | 30 | 15 | 100 | 56 | 0 | 0 |
| 110035 | Escape | Teleport all party members to your resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 1100 | 35 | 6 | 400 | 15 | 250 | 22500 | 0 | 0 |
| 110774 | Blink | Able to teleport in front of an opponent 20 meters away. | 1 | 20 | 20 | blink forward Radius meters | 0 | 0 | 1107 | 80 | 1 | 100 | 0 | 213 | 0 | 517 | 0 |
| 111700 | Gate | Teleport to your chosen resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 1117 | 0 | 1 | 20 | 15 | 74 | 56 | 0 | 0 |
| 112700 | Gate | Teleport to your chosen resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 1127 | 0 | 1 | 20 | 15 | 74 | 56 | 0 | 0 |
| 205650 | descent | Teleport to the location of a chosen party member | 1 | 30 | 25 | teleport to target (party member for Descent, enemy within Radius for Wild Advent) | 0 | 0 | 2056 | 50 | 4 | 50 | 0 | 91 | 225 | 0 | 0 |
| 206650 | descent | Teleport to the location of a chosen party member | 1 | 30 | 25 | teleport to target (party member for Descent, enemy within Radius for Wild Advent) | 0 | 0 | 2066 | 50 | 4 | 50 | 0 | 91 | 225 | 0 | 0 |
| 208770 | Wild advent | Teleports to an enemy that is within a certain distance from you. | 1 | 40 | 25 | teleport to target (party member for Descent, enemy within Radius for Wild Advent) | 0 | 0 | 2087 | 70 | 7 | 130 | 0 | 250 | 112 | 0 | 0 |
| 209004 | summon friend | Retrieve a party member | 1 | 10000 | 12 | summon target, same zone | 0 | 0 | 2090 | 4 | 4 | 5 | 15 | 1 | 22500 | 0 | 0 |
| 209015 | Gate | Teleport to the chosen resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 2090 | 15 | 1 | 30 | 15 | 100 | 56 | 0 | 0 |
| 209035 | Escape | Teleport all party members to your resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 2090 | 35 | 6 | 400 | 15 | 250 | 22500 | 0 | 0 |
| 210004 | summon friend | Retrieve a party member | 1 | 10000 | 12 | summon target, same zone | 0 | 0 | 2100 | 4 | 4 | 5 | 15 | 1 | 22500 | 0 | 0 |
| 210015 | Gate | Teleport to the chosen resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 2100 | 15 | 1 | 30 | 15 | 100 | 56 | 0 | 0 |
| 210035 | Escape | Teleport all party members to your resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 2100 | 35 | 6 | 400 | 15 | 250 | 22500 | 0 | 0 |
| 210774 | Blink | Able to teleport in front of an opponent 20 meters away. | 1 | 20 | 20 | blink forward Radius meters | 0 | 0 | 2107 | 80 | 1 | 100 | 0 | 213 | 0 | 517 | 0 |
| 211700 | Gate | Teleport to your chosen resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 2117 | 0 | 1 | 20 | 15 | 74 | 56 | 0 | 0 |
| 212700 | Gate | Teleport to your chosen resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 2127 | 0 | 1 | 20 | 15 | 74 | 56 | 0 | 0 |
| 490042 | Call clan members | Teleport all your clan members to your current spot | 1 | 10000 | 12 | summon target, same zone | 0 | 0 | 0 | 0 | 15 | 0 | 20 | 250 | 22500 | 0 | 389042000 |
| 490050 | Call Party | Teleport all your party members to your current spot | 1 | 10000 | 12 | summon target, same zone | 0 | 0 | 0 | 0 | 6 | 0 | 20 | 250 | 22500 | 0 | 389050000 |
| 490088 | Monster Summons Staff | Retrieves a unique monster | 1 | 10000 | 21 | summon monster (monster staff) | 0 | 0 | 0 | 0 | 1 | 0 | 5 | 250 | 56 | 0 | 700003000 |
| 490093 | Monster Summons Staff | Retrieves a unique monster | 1 | 10000 | 21 | summon monster (monster staff) | 2 | 0 | 0 | 0 | 1 | 0 | 5 | 250 | 56 | 0 | 700004000 |
| 490096 | Monster Summons Staff |  | 1 | 10000 | 21 | summon monster (monster staff) | 1 | 0 | 0 | 0 | 1 | 0 | 5 | 250 | 56 | 0 | 700005000 |
| 490097 | Monster Summons Staff |  | 1 | 10000 | 21 | summon monster (monster staff) | 0 | 0 | 0 | 0 | 1 | 0 | 5 | 250 | 56 | 0 | 700006000 |
| 500022 | Bind Scroll | Teleport to the pre-selected resurrection spot | 1 | 10000 | 1 | to resurrection point / town | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 22500 | 0 | 800002000 |
| 500038 | Scroll of teleport friend | Teleport to a registered friend. | 1 | 10000 | 25 | teleport to target (party member for Descent, enemy within Radius for Wild Advent) | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 15 | 22500 | 0 | 800021000 |

## 5. Per-class tables (generated; all class codes; Karus id shown, El Morad id = Karus id + 100000 and Skill + 1000)

Columns: Req = SkillLevel (character level for "Basic", points in the category otherwise). UseItem shows the item that must be owned; "(+consumes class stone ...)" = BeforeAction 1..4 rule. Etc = required quest id (0 = none).

## Warrior

Nation twin check: 106 of 107 Karus rows have an El Morad twin (MagicNum+100000, Skill+1000) with identical numeric MAGIC columns and identical Type-table values (5 of them differ only in name/description text: 105001, 105005, 105550, 105557, 105560). Rows with real numeric differences: 1.

- 106545 (Cleave) vs 206545: T1.Hit 150->125

### Class 101 / 201 - beginner warrior

#### Basic (Skill=1010, SkillLevel = required character level)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 101001 | 201001 | Sprint | Temporarily increases your running speed | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 60 | 0 | 100 | 0 | T4 SPEED(6) dur=10s Speed=150 |
| 101003 | 201003 | slash | Inflict 120% damage | 1/0 | ENEMY(7) | 3 | 4 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=120% add=+0 hitrate=100(rel) delay=100 |
| 101005 | 201005 | Crash | Increases chances of a successful attack by 1.5 times | 1/0 | ENEMY(7) | 5 | 4 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=150(rel) delay=100 |
| 101007 | 201007 | Defense | Temporarily increases your defense | 4/0 | SELF(1) | 7 | 4 | 0 |  | 0 | 100 | 0 | 100 | 0 | T4 AC(2) dur=10s AC=50 |
| 101009 | 201009 | Piercing | Increases chances of a successful attack by 2.0 times | 1/0 | ENEMY(7) | 9 | 4 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=200(rel) delay=100 |

#### cat 9 (Skill=1019, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 101010 | 201010 | whipping | Whips the horse to temporarily increase the horse's movement speed. | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 255 | 0 | 100 | 0 | T4 SPEED(6) dur=1200s Speed=200 |

### Class 105 / 205 - novice warrior (job-changed)

#### Basic (Skill=1050, SkillLevel = required character level)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 105001 | 205001 | Sprint | Temporarily increases your running speed | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 60 | 0 | 100 | 0 | T4 SPEED(6) dur=10s Speed=150 |
| 105003 | 205003 | Slash | Inflict 120% damage | 1/0 | ENEMY(7) | 3 | 4 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=120% add=+0 hitrate=100(rel) delay=100 |
| 105005 | 205005 | Crash | Increases chances of a successful attack by 1.5 times | 1/0 | ENEMY(7) | 5 | 4 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=150(rel) delay=100 |
| 105007 | 205007 | Defense | Temporarily increases your defense | 4/0 | SELF(1) | 7 | 4 | 0 |  | 0 | 100 | 0 | 100 | 0 | T4 AC(2) dur=15s AC=50 |
| 105009 | 205009 | Piercing | Increases chances of a successful attack by 2.0 times | 1/0 | ENEMY(7) | 9 | 4 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=200(rel) delay=100 |

#### Tree1 (cat 5) Attack (Skill=1055, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 105500 | 205500 | Hash | Adds an additional 30 damage regardless of defense | 1/0 | ENEMY(7) | 0 | 10 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+30 hitrate=100(rel) delay=100 |
| 105505 | 205505 | Hoodwink | Inflict 150% damage | 1/0 | ENEMY(7) | 5 | 30 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=150% add=+0 hitrate=100(rel) delay=100 |
| 105510 | 205510 | Shear | Adds an additional 50 damage regardless of defense | 1/0 | ENEMY(7) | 10 | 20 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+50 hitrate=100(rel) delay=100 |
| 105515 | 205515 | Pierce | Inflict 100% damage with no chance of failure | 1/0 | ENEMY(7) | 15 | 60 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(abs) delay=100 |
| 105520 | 205520 | Leg cutting | An attack that slows down your enemy | 1/4 | ENEMY(7) | 20 | 84 | 0 |  | 0 | 51 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(rel) delay=100; T4 SPEED(6) dur=10s Speed=50 |
| 105525 | 205525 | Carving | Inflict 200% damage | 1/0 | ENEMY(7) | 25 | 90 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=200% add=+0 hitrate=100(rel) delay=100 |
| 105530 | 205530 | Sever | Adds an additional 100 damage regardless of defense | 1/0 | ENEMY(7) | 30 | 40 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+100 hitrate=100(rel) delay=100 |
| 105535 | 205535 | Prick | Inflict 150% damage with no chance of failure | 1/0 | ENEMY(7) | 35 | 120 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=150% add=+0 hitrate=100(abs) delay=100 |
| 105540 | 205540 | Multiple shock | Inflict 150% damage and 50 additional damage | 1/0 | ENEMY(7) | 40 | 60 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=150% add=+50 hitrate=100(rel) delay=100 |
| 105545 | 205545 | Cleave | Inflict 250% damage | 1/0 | ENEMY(7) | 45 | 150 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=250% add=+0 hitrate=100(rel) delay=100 |
| 105550 | 205550 | mangling | 150 additional damage that is uneffected by the opponent's defense | 1/0 | ENEMY(7) | 50 | 60 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+150 hitrate=100(rel) delay=100 |
| 105555 | 205555 | thrust | Inflict 200% damage with no chance of failure | 1/0 | ENEMY(7) | 55 | 200 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=200% add=+0 hitrate=100(abs) delay=100 |
| 105557 | 205557 | sword aura | Inflict 250% damage and 100 additional damage with no chance of failure. | 1/0 | ENEMY(7) | 57 | 250 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=150% add=+250 hitrate=100(abs) delay=100 |
| 105560 | 205560 | sword dancing | Inflict 250% damage and 150 additional damage with no chance of failure. | 1/0 | ENEMY(7) | 60 | 300 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=200% add=+150 hitrate=100(abs) delay=100 |

#### Tree2 (cat 6) Defense (Skill=1056, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 105605 | 205605 | Hinder | [Passive]Increase defense by 10%. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 5 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 105610 | 205610 | resist | [Passive]Increaes all resistance by 30. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 10 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 105615 | 205615 | Arrest | [Passive]Increase defense by 15%. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 15 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 105620 | 205620 | endure | [Passive]Increase all resistance by 60. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 20 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 105630 | 205630 | Binding | Provoke a monster to concentrate its attack on you. | 7/0 | ENEMY(7) | 30 | 30 | 0 |  | 0 | 65 | 67 | 100 | 0 | T7 group=0 monster=0 target=1 state=0 radius=0 hitrate=100 dur=9 dmg=10 |
| 105635 | 205635 | Bulwark | [Passive]Increase defense by 20%. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 35 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 105640 | 205640 | immunity | [Passive]Increase all resistance by 90. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 40 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 105645 | 205645 | provoke | Provoke monsters in a certain area to concentrate their attacks on you. | 7/0 | AREA_ENEMY(10) | 45 | 60 | 0 |  | 15 | 150 | 22 | 100 | 0 | T7 group=0 monster=0 target=1 state=0 radius=30 hitrate=100 dur=12 dmg=10 |
| 105650 | 205650 | descent | Teleport to the location of a chosen party member | 8/0 | PARTY(4) | 50 | 50 | 0 |  | 0 | 91 | 225 | 100 | 0 | T8 target=1 radius=30 warp=25(teleport to target (Descent/Wild Advent)) exprecover=0 kick=0 |
| 105655 | 205655 | evading | [Passive]Increase defense by 25%. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 55 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 105660 | 205660 | sacrifice | Sacrifice your own self to fill the HP one party member | 3/0 | PARTY(4) | 60 | 180 | 10001 |  | 0 | 250 | 67 | 100 | 0 | T3 HP first=10000 |

#### Tree3 (cat 7) Berserk/Passion (Skill=1057, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 105705 | 205705 | Gain | Increase Strength by 15 | 4/0 | SELF(1) | 5 | 10 | 0 |  | 0 | 91 | 0 | 100 | 0 | T4 STATS(7) dur=300s Str=15 |
| 105710 | 205710 | pain killer | Exchange 100 HP for 200 MP | 3/0 | SELF(1) | 10 | 0 | 100 |  | 0 | 45 | 0 | 100 | 0 | T3 MP first=200 |
| 105715 | 205715 | Rise | Increase HP by 10 | 4/0 | SELF(1) | 15 | 30 | 0 |  | 0 | 91 | 0 | 100 | 0 | T4 STATS(7) dur=300s Sta=10 |
| 105720 | 205720 | Outrage | Absorbs damage from MP for 10 seconds | 4/0 | SELF(1) | 20 | 60 | 0 |  | 0 | 91 | 0 | 100 | 0 | T4 ATTACK_SPEED(5) dur=30s AttackSpeed=120 |
| 105725 | 205725 | Blade of hate | Use of a sword to attack an enemy that is far away. | 3/0 | ENEMY(7) | 25 | 60 | 0 |  | 15 | 51 | 45 | 100 | 0 | T3 HP first=-150 attr=magic |
| 105730 | 205730 | restoration | Temporarily increase rate of HP regeneration | 3/0 | SELF(1) | 30 | 105 | 0 |  | 0 | 250 | 0 | 100 | 0 | T3 HP first=0 overtime=750 dur=60s |
| 105731 | 205731 | Blaze Killer | Exchange 200HP for 400 MP | 3/0 | SELF(1) | 30 | 0 | 200 |  | 0 | 65 | 0 | 100 | 0 | T3 MP first=400 |
| 105735 | 205735 | Nimble Wind | Increase your Dexterity by 20 | 4/0 | SELF(1) | 35 | 90 | 0 |  | 0 | 91 | 0 | 100 | 0 | T4 STATS(7) dur=300s Dex=20 |
| 105740 | 205740 | Return to life | Exchange 500 MP for 250 HP | 3/0 | SELF(1) | 40 | 500 | 0 |  | 0 | 51 | 0 | 100 | 0 | T3 HP first=250 |
| 105750 | 205750 | Regeneration | Temporarily increase rate of HP regeneration. Rate of HP regeneration is faster than Restoration. | 3/0 | SELF(1) | 50 | 210 | 0 |  | 0 | 250 | 0 | 100 | 0 | T3 HP first=0 overtime=1500 dur=60s |
| 105755 | 205755 | Frenzy | Absorbs damage from MP for 20 seconds | 4/0 | SELF(1) | 55 | 150 | 0 |  | 0 | 91 | 0 | 100 | 0 | T4 ATTACK_SPEED(5) dur=30s AttackSpeed=130 |
| 105760 | 205760 | Quake | A sword skill that attacks all the opponents around you | 3/0 | AREA_ENEMY(10) | 60 | 200 | 0 |  | 0 | 51 | 45 | 100 | 0 | T3 HP first=-500 end=-200 attr=magic radius=10 |

#### cat 9 (Skill=1059, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 105010 | 205010 | whipping | Whips the horse to temporarily increase the horse's movement speed. | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 255 | 0 | 100 | 0 | T4 SPEED(6) dur=1200s Speed=200 |

### Class 106 / 206 - master warrior

#### Basic (Skill=1060, SkillLevel = required character level)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106001 | 206001 | sprint | Temporarily increase your running speed | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 60 | 0 | 100 | 0 | T4 SPEED(6) dur=10s Speed=150 |
| 106003 | 206003 | slash | Inflict 120% damage | 1/0 | ENEMY(7) | 3 | 4 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=120% add=+0 hitrate=100(rel) delay=100 |
| 106005 | 206005 | crash | Increases your chances of succeeding an attack by 1.5 times | 1/0 | ENEMY(7) | 5 | 4 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=150(rel) delay=100 |
| 106007 | 206007 | Defense | Temporarily increases your defense | 4/0 | SELF(1) | 7 | 4 | 0 |  | 0 | 100 | 0 | 100 | 0 | T4 AC(2) dur=15s AC=50 |
| 106009 | 206009 | piercing | Increases chances of a successful attack by 2.0 times | 1/0 | ENEMY(7) | 9 | 4 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=200(rel) delay=100 |

#### Tree1 (cat 5) Attack (Skill=1065, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106500 | 206500 | Hash | Adds an additional 30 damage regardless of defense | 1/0 | ENEMY(7) | 0 | 10 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+30 hitrate=100(rel) delay=100 |
| 106505 | 206505 | hoodwink | Inflict 150% damage | 1/0 | ENEMY(7) | 5 | 30 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=150% add=+0 hitrate=100(rel) delay=100 |
| 106510 | 206510 | Shear | Adds an additional 50 damage regardless of defense | 1/0 | ENEMY(7) | 10 | 20 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+50 hitrate=100(rel) delay=100 |
| 106515 | 206515 | pierce | Inflict 100% damage with no chance of failure | 1/0 | ENEMY(7) | 15 | 60 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(abs) delay=100 |
| 106520 | 206520 | leg cutting | An attack that slows down your enemy | 1/4 | ENEMY(7) | 20 | 84 | 0 |  | 0 | 51 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(rel) delay=100; T4 SPEED(6) dur=10s Speed=50 |
| 106525 | 206525 | Carving | Inflict 200% damage | 1/0 | ENEMY(7) | 25 | 90 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=200% add=+0 hitrate=100(rel) delay=100 |
| 106530 | 206530 | Sever | Adds an additional 100 damage regardless of defense | 1/0 | ENEMY(7) | 30 | 40 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+100 hitrate=100(rel) delay=100 |
| 106535 | 206535 | prick | Inflict 150% damage with no chance of failure | 1/0 | ENEMY(7) | 35 | 120 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=150% add=+0 hitrate=100(abs) delay=100 |
| 106540 | 206540 | multiple shock | Inflict 150% damage and 50 additional damage | 1/0 | ENEMY(7) | 40 | 60 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=150% add=+50 hitrate=100(rel) delay=100 |
| 106545 | 206545 | Cleave | Inflict 250% damage | 1/0 | ENEMY(7) | 45 | 150 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=150% add=+0 hitrate=100(rel) delay=100 |
| 106550 | 206550 | mangling | Adds an additional 150 damage regardless of defense | 1/0 | ENEMY(7) | 50 | 60 | 0 |  | 0 | 31 | 0 | 100 | 0 | T1 hit=100% add=+150 hitrate=100(rel) delay=100 |
| 106555 | 206555 | thrust | Inflict 200% damage with no chance of failure | 1/0 | ENEMY(7) | 55 | 200 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(abs) delay=100 |
| 106557 | 206557 | sword aura | Inflict 200% damage and 100 additional damage with no change of failure. | 1/0 | ENEMY(7) | 57 | 250 | 0 |  | 0 | 1 | 0 | 100 | 0 | T1 hit=100% add=+250 hitrate=100(abs) delay=100 |
| 106560 | 206560 | sword dancing | Inflict 250% damage and 150 additional damage with no change of failure. | 1/0 | ENEMY(7) | 60 | 300 | 0 |  | 0 | 5 | 0 | 100 | 0 | T1 hit=150% add=+150 hitrate=100(abs) delay=100 |
| 106570 | 206570 | Howling Sword | Inflict 300% damage and 200 additional damage with no change of failure. | 1/0 | ENEMY(7) | 70 | 400 | 0 |  | 0 | 8 | 0 | 100 | 0 | T1 hit=200% add=+200 hitrate=100(abs) delay=100 |
| 106575 | 206575 | blooding | Inflict 200% damage, 150 additional damage, and deals 1000 damage over 20 seconds with no chance of failure. | 1/3 | ENEMY(7) | 75 | 350 | 0 |  | 0 | 210 | 0 | 100 | 510 | T1 hit=350% add=+400 hitrate=100(abs) delay=100; T3 MP first=0 overtime=-1000 dur=20s attr=magic |
| 106580 | 206580 | Hell blade | Inflict 300% damage and 350 additional damage with no change of failure. | 1/0 | ENEMY(7) | 80 | 400 | 0 |  | 0 | 8 | 0 | 100 | 511 | T1 hit=310% add=+300 hitrate=100(abs) delay=100 |

#### Tree2 (cat 6) Defense (Skill=1066, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106605 | 206605 | Hinder | [Passive]Increase defense by 10%. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 5 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106610 | 206610 | resist | [Passive]Increaes all resistance by 30. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 10 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106615 | 206615 | Arrest | [Passive]Increase defense by 15%. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 15 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106620 | 206620 | endure | [Passive]Increase all resistance by 60. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 20 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106630 | 206630 | Binding | Provoke a monster to concentrate its attack on you. | 7/0 | ENEMY(7) | 30 | 30 | 0 |  | 0 | 65 | 67 | 100 | 0 | T7 group=0 monster=0 target=1 state=0 radius=0 hitrate=100 dur=9 dmg=10 |
| 106635 | 206635 | Bulwark | [Passive]Increase defense by 20%. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 35 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106640 | 206640 | immunity | [Passive]Increase all resistance by 90. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 40 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106645 | 206645 | provoke | Provoke monsters in a certain area to concentrate their attacks on you. | 7/0 | AREA_ENEMY(10) | 45 | 60 | 0 |  | 15 | 150 | 22 | 100 | 0 | T7 group=0 monster=0 target=1 state=0 radius=30 hitrate=100 dur=12 dmg=10 |
| 106650 | 206650 | descent | Teleport to the location of a chosen party member | 8/0 | PARTY(4) | 50 | 50 | 0 |  | 0 | 91 | 225 | 100 | 0 | T8 target=1 radius=30 warp=25(teleport to target (Descent/Wild Advent)) exprecover=0 kick=0 |
| 106655 | 206655 | evading | [Passive]Increase defense by 25%. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 55 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106660 | 206660 | sacrifice | Sacrifice your own self to fill the HP one party member | 3/0 | PARTY(4) | 60 | 180 | 10001 |  | 0 | 250 | 67 | 100 | 0 | T3 HP first=10000 |
| 106670 | 206670 | Iron Skin | [Passive]Increase defense by 30%. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 70 | 0 | 0 |  | 0 | 101 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106675 | 206675 | wall of Iron | Defense is tripled for 10 seconds. However, attack rate is decreased by 50% during this time. | 4/0 | SELF(1) | 75 | 250 | 0 |  | 0 | 200 | 0 | 100 | 510 | T4 TRIPLEAC_HALFSPEED(28) dur=10s Speed=50 ACPct=300 |
| 106680 | 206680 | iron linker | [Passive]Increase defense by 40%. If a shield is not equipped, the effect will decrease by half. | 0/0 | SELF(1) | 80 | 0 | 0 |  | 0 | 101 | 0 | 100 | 511 | passive (no type row; handled by client/server stat code) |

#### Tree3 (cat 7) Berserk/Passion (Skill=1067, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106705 | 206705 | Gain | Increase Strength by 15 | 4/0 | SELF(1) | 5 | 10 | 0 |  | 0 | 91 | 0 | 100 | 0 | T4 STATS(7) dur=300s Str=15 |
| 106710 | 206710 | pain killer | Exchange 100 HP for 200 MP | 3/0 | SELF(1) | 10 | 0 | 100 |  | 0 | 45 | 0 | 100 | 0 | T3 MP first=200 |
| 106715 | 206715 | Rise | Increase HP by 10 | 4/0 | SELF(1) | 15 | 30 | 0 |  | 0 | 91 | 0 | 100 | 0 | T4 STATS(7) dur=300s Sta=10 |
| 106720 | 206720 | Outrage | Absorbs damage from MP for 10 seconds | 4/0 | SELF(1) | 20 | 60 | 0 |  | 0 | 91 | 0 | 100 | 0 | T4 ATTACK_SPEED(5) dur=30s AttackSpeed=120 |
| 106725 | 206725 | Blade of hate | Use of a sword to attack an enemy that is far away. | 3/0 | ENEMY(7) | 25 | 60 | 0 |  | 15 | 51 | 45 | 100 | 0 | T3 HP first=-150 attr=magic |
| 106730 | 206730 | restoration | Temporarily increase rate of HP regeneration | 3/0 | SELF(1) | 30 | 105 | 0 |  | 0 | 250 | 0 | 100 | 0 | T3 HP first=0 overtime=750 dur=60s |
| 106731 | 206731 | Blaze Killer | Exchange 200HP for 400 MP | 3/0 | SELF(1) | 30 | 0 | 200 |  | 0 | 65 | 0 | 100 | 0 | T3 MP first=400 |
| 106735 | 206735 | Nimble Wind | Increase your Dexterity by 20 | 4/0 | SELF(1) | 35 | 90 | 0 |  | 0 | 91 | 0 | 100 | 0 | T4 STATS(7) dur=300s Dex=20 |
| 106740 | 206740 | Return to life | Exchange 500 MP for 250 HP | 3/0 | SELF(1) | 40 | 500 | 0 |  | 0 | 51 | 0 | 100 | 0 | T3 HP first=250 |
| 106750 | 206750 | Regeneration | Temporarily increase rate of HP regeneration. Rate of HP regeneration is faster than Restoration. | 3/0 | SELF(1) | 50 | 210 | 0 |  | 0 | 250 | 0 | 100 | 0 | T3 HP first=0 overtime=1500 dur=60s |
| 106755 | 206755 | Frenzy | Absorbs damage from MP for 20 seconds | 4/0 | SELF(1) | 55 | 150 | 0 |  | 0 | 91 | 0 | 100 | 0 | T4 ATTACK_SPEED(5) dur=30s AttackSpeed=130 |
| 106760 | 206760 | Quake | A sword skill that attacks all the opponents around you | 3/0 | AREA_ENEMY(10) | 60 | 160 | 0 |  | 0 | 51 | 45 | 100 | 0 | T3 HP first=-500 end=-200 attr=magic radius=10 |
| 106770 | 206770 | berserk Echo | Temporarily increase attack speed by 40%. | 4/0 | SELF(1) | 70 | 250 | 0 |  | 0 | 101 | 0 | 100 | 0 | T4 ATTACK_SPEED(5) dur=30s AttackSpeed=140 |
| 106775 | 206775 | Berserker | Increase attack speed by 20% for 20 seconds. However, defense is decreased by 300 during this time. | 4/0 | SELF(1) | 75 | 300 | 0 |  | 0 | 255 | 0 | 100 | 510 | T4 ATTACK_SPEED_ARMOR(Berserker)(18) dur=30s AC=-300 Attack=120 |
| 106780 | 206780 | HP Booster | Recovers HP while standing as if you were sitting for a set period of time. | 3/0 | SELF(1) | 80 | 200 | 0 |  | 0 | 255 | 0 | 100 | 511 | T3 HP-booster first=105 dur=20s |

#### Master (cat 8) (Skill=1068, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106800 | 206800 | boldness | [Passive]Increase your defense by 20% when your HP is 30% or lower. | 0/0 | SELF(1) | 0 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106802 | 206802 | Scream | A fail-safe attack that inflicts 250% Damage and 150 additional damage. �Also has a chance to stun the enemy temporarily. � | 1/4 | ENEMY(7) | 2 | 300 | 0 | 379063000 Scream Scroll (+0) (+consumes class stone 379059000) | 0 | 101 | 0 | 100 | 0 | T1 hit=250% add=+200 hitrate=100(abs) delay=100; T4 SPEED(6) dur=7s Speed=1 |
| 106805 | 206805 | Absoluteness | [Passive]Decreases all damages received by 10% | 0/0 | SELF(1) | 5 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106810 | 206810 | Matchless | [Passive]Decreases all damages received by 15% | 0/0 | SELF(1) | 10 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 106815 | 206815 | Exceed Break | Inflict 200% damage with no chance of failure and a chance to reduce the oppoenent's armor's durability by 1000. | 1/3 | ENEMY(7) | 15 | 400 | 0 | 379059000 Stone of Warrior (+0) | 0 | 254 | 0 | 100 | 0 | T1 hit=200% add=+0 hitrate=100(rel) delay=100; T3 dura-dmg first=-1000 |
| 106820 | 206820 | Shock Stun | Inflicts 200% damage and stuns the enemy for 3 seconds. | 1/3 | ENEMY(7) | 20 | 250 | 0 | 379059000 Stone of Warrior (+0) | 0 | 252 | 0 | 100 | 0 | T1 hit=175% add=+175 hitrate=100(rel) delay=100; T3 dt0 first=0 attr=lightning |

#### cat 9 (Skill=1069, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106010 | 206010 | whipping | Whips the horse to temporarily increase the horse's movement speed. | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 255 | 0 | 100 | 0 | T4 SPEED(6) dur=1200s Speed=200 |

## Priest

Nation twin check: 182 of 187 Karus rows have an El Morad twin (MagicNum+100000, Skill+1000) with identical numeric MAGIC columns and identical Type-table values (0 of them differ only in name/description text: -). Rows with real numeric differences: 5.

- 111703 (Malice) vs 211703: Range 90->101
- 111733 (Resurrection of love) vs 211733: T5.ExpRecover 10->60
- 112703 (Malice) vs 212703: Range 56->90
- 112745 (Parasite) vs 212745: Range 56->90
- 112760 (Massive) vs 212760: Range 56->90

### Class 104 / 204 - beginner priest

#### Basic (Skill=1040, SkillLevel = required character level)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 104001 | 204001 | Tiny healing | Heal 15 HP | 3/0 | FRIEND_WITHME(2) | 1 | 5 | 0 |  | 19 | 1 | 56 | 80 | 0 | T3 HP first=15 |
| 104002 | 204002 | Light strike | Light magic attack | 3/0 | ENEMY(7) | 2 | 5 | 0 |  | 15 | 54 | 56 | 50 | 0 | T3 HP first=-29 attr=magic |
| 104004 | 204004 | Strength | Increase Strength by 15 | 4/0 | FRIEND_WITHME(2) | 4 | 10 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 STATS(7) dur=300s Str=15 |
| 104005 | 204005 | Light healing | Heal 30 HP | 3/0 | FRIEND_WITHME(2) | 5 | 10 | 0 |  | 19 | 1 | 56 | 80 | 0 | T3 HP first=30 |
| 104006 | 204006 | Resist poison | Increase resistance to poison by 20 | 4/0 | FRIEND_WITHME(2) | 6 | 10 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=300s PoisonR=20 |
| 104007 | 204007 | Brightness | Light magic attack | 3/0 | ENEMY(7) | 7 | 10 | 0 |  | 15 | 54 | 56 | 50 | 0 | T3 HP first=-77 attr=magic |
| 104008 | 204008 | Tiny restore | Heal 50 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 8 | 15 | 0 |  | 19 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=50 dur=20s |
| 104010 | 204010 | Light Attack | A magic attack that uses light | 3/0 | ENEMY(7) | 18 | 50 | 0 |  | 15 | 52 | 33 | 80 | 0 | T3 MP first=-105 overtime=-10 dur=10s attr=magic |
| 104011 | 204011 | Light Counter | A magic attack that uses light | 3/0 | ENEMY(7) | 24 | 100 | 0 |  | 15 | 54 | 33 | 80 | 0 | T3 MP first=-260 overtime=-30 dur=10s attr=magic |
| 104012 | 204012 | Critical Light | A magic attack that uses light | 3/0 | ENEMY(7) | 33 | 150 | 0 |  | 15 | 54 | 33 | 80 | 0 | T3 MP first=-385 overtime=-50 dur=10s attr=magic |

#### cat 9 (Skill=1049, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 104009 | 204009 | whipping | Whips the horse to temporarily increase the horse's movement speed. | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 255 | 0 | 100 | 0 | T4 SPEED(6) dur=1200s Speed=200 |

### Class 111 / 211 - novice priest

#### Basic (Skill=1110, SkillLevel = required character level)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 111001 | 211001 | Tiny healing | Heal 15 HP | 3/0 | FRIEND_WITHME(2) | 1 | 5 | 0 |  | 19 | 1 | 56 | 80 | 0 | T3 HP first=15 |
| 111002 | 211002 | Light strike | Light magic attack | 3/0 | ENEMY(7) | 2 | 5 | 0 |  | 15 | 54 | 56 | 50 | 0 | T3 HP first=-29 attr=magic |
| 111004 | 211004 | Strength | Increase strength by 15 | 4/0 | FRIEND_WITHME(2) | 4 | 10 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 STATS(7) dur=600s Str=15 |
| 111005 | 211005 | Light healing | Heal 30 HP | 3/0 | FRIEND_WITHME(2) | 5 | 10 | 0 |  | 19 | 1 | 56 | 80 | 0 | T3 HP first=30 |
| 111006 | 211006 | Resist poison | Increase resistance to poison by 20 | 4/0 | FRIEND_WITHME(2) | 6 | 10 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=600s PoisonR=20 |
| 111007 | 211007 | Brightness | Light magic attack | 3/0 | ENEMY(7) | 7 | 10 | 0 |  | 15 | 54 | 56 | 50 | 0 | T3 HP first=-77 attr=magic |
| 111008 | 211008 | Tiny restore | Heal 50 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 8 | 15 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=50 dur=20s |
| 111010 | 211010 | Light Attack | A magic attack that uses light | 3/0 | ENEMY(7) | 18 | 50 | 0 |  | 15 | 52 | 33 | 80 | 0 | T3 MP first=-105 overtime=-10 dur=10s attr=magic |
| 111011 | 211011 | Light Counter | A magic attack that uses light | 3/0 | ENEMY(7) | 24 | 100 | 0 |  | 15 | 54 | 33 | 80 | 0 | T3 MP first=-260 overtime=-30 dur=10s attr=magic |
| 111012 | 211012 | Critical Light | A magic attack that uses light | 3/0 | ENEMY(7) | 33 | 150 | 0 |  | 15 | 54 | 33 | 80 | 0 | T3 MP first=-385 overtime=-50 dur=10s attr=magic |

#### Tree1 (cat 5) Heal (Skill=1115, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 111500 | 211500 | minor healing | Heal 60 HP | 3/0 | FRIEND_WITHME(2) | 0 | 10 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=60 |
| 111503 | 211503 | Light restore | Heal 100 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 3 | 25 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=100 dur=30s |
| 111509 | 211509 | healing | Heal 240 HP | 3/0 | FRIEND_WITHME(2) | 9 | 20 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=120 |
| 111511 | 211511 | collision | A fail-safe attack that inflicts 120% damage. | 1/0 | ENEMY(7) | 12 | 30 | 0 |  | 0 | 1 | 0 | 100 | 0 | T1 hit=200% add=+0 hitrate=100(abs) delay=100 |
| 111512 | 211512 | Restore | Heal 400 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 12 | 30 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=200 dur=30s |
| 111518 | 211518 | major healing | Heal 360 HP | 3/0 | FRIEND_WITHME(2) | 18 | 40 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=240 |
| 111520 | 211520 | shuddering | A fail-safe attack that inflicts 150% damage. | 1/0 | ENEMY(7) | 21 | 40 | 0 |  | 0 | 2 | 0 | 100 | 0 | T1 hit=250% add=+0 hitrate=100(abs) delay=100 |
| 111521 | 211521 | Major restore | Heal 600 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 21 | 100 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=400 dur=30s |
| 111525 | 211525 | Cure curse | Neutralizes any resistance decreasing spells. | 5/0 | FRIEND_WITHME(2) | 25 | 60 | 0 |  | 15 | 15 | 56 | 100 | 0 | T5 type=2 exprecover=0 needstone=0 |
| 111527 | 211527 | Great healing | Heal 720 HP | 3/0 | FRIEND_WITHME(2) | 27 | 80 | 0 |  | 15 | 20 | 56 | 80 | 0 | T3 HP first=960 |
| 111529 | 211529 | blasting | Increase strength by 30. | 4/0 | SELF(1) | 30 | 80 | 0 |  | 0 | 10 | 0 | 100 | 0 | T4 STATS(7) dur=400s Str=30 |
| 111530 | 211530 | Great restore | Heal 800 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 30 | 200 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=800 dur=30s |
| 111535 | 211535 | Cure disease | Neutralizes spells that decrease your HP | 5/0 | FRIEND_WITHME(2) | 35 | 120 | 0 |  | 15 | 15 | 56 | 100 | 0 | T5 type=1 exprecover=0 needstone=0 |
| 111536 | 211536 | Massive healing | Heal 960 HP | 3/0 | FRIEND_WITHME(2) | 36 | 160 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=960 |
| 111539 | 211539 | Massive restore | Heal 1500 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 39 | 375 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=1500 dur=30s |
| 111542 | 211542 | ruin | A fail-safe attack that inflicts 150% damage and an additional 100 damage. | 1/0 | ENEMY(7) | 42 | 100 | 0 |  | 0 | 20 | 0 | 100 | 0 | T1 hit=300% add=+100 hitrate=100(abs) delay=100 |
| 111545 | 211545 | Superior healing | Heal 1920 HP | 3/0 | FRIEND_WITHME(2) | 45 | 320 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=1920 |
| 111548 | 211548 | Superior restore | Heal 2500 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 48 | 625 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=2500 dur=30s |
| 111551 | 211551 | Hellish | A fail-safe attack that inflicts 200% damage and an additional 50 damage. | 1/0 | ENEMY(7) | 51 | 120 | 0 |  | 0 | 30 | 0 | 100 | 0 | T1 hit=350% add=+50 hitrate=100(abs) delay=100 |
| 111554 | 211554 | Complete healing | Commpletely heal the HP of a friend | 3/0 | FRIEND_WITHME(2) | 54 | 960 | 0 |  | 15 | 54 | 56 | 80 | 0 | T3 HP first=10000 |
| 111557 | 211557 | Group massive healing | Heal 960 HP of all the members of your party. | 3/0 | PARTY_ALL(6) | 57 | 960 | 0 |  | 15 | 54 | 56 | 80 | 0 | T3 HP first=960 radius=30 |
| 111560 | 211560 | Group complete healing | Completely heal all the members of your party | 3/0 | PARTY_ALL(6) | 60 | 1920 | 0 |  | 15 | 64 | 56 | 80 | 0 | T3 HP first=10000 radius=30 |

#### Tree2 (cat 6) Buff/Aura (Skill=1116, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 111603 | 211603 | Insensibility Skin | Increase AC by 20 | 4/0 | FRIEND_WITHME(2) | 3 | 10 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=20 |
| 111606 | 211606 | Grace | Increase a party member's HP by 60. | 4/0 | PARTY(4) | 6 | 15 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=60 |
| 111609 | 211609 | Resist all | Increase magic, curse, and poison resistance by 20 | 4/0 | FRIEND_WITHME(2) | 9 | 15 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=600s MagicR=20 DiseaseR=20 PoisonR=20 |
| 111611 | 211611 | Wrath | A fail-safe attack that inflicts 120% damage. | 1/0 | ENEMY(7) | 12 | 30 | 0 |  | 0 | 1 | 0 | 100 | 0 | T1 hit=150% add=+0 hitrate=100(abs) delay=100 |
| 111612 | 211612 | Insensibility shell | Increase AC by 40 | 4/0 | FRIEND_WITHME(2) | 12 | 20 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=40 |
| 111615 | 211615 | Brave | Increase a party member's HP by 240. | 4/0 | PARTY(4) | 15 | 30 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=120 |
| 111620 | 211620 | wield | A fail-safe attack that inflicts 150% damage. | 1/0 | ENEMY(7) | 21 | 40 | 0 |  | 0 | 2 | 0 | 100 | 0 | T1 hit=200% add=+0 hitrate=100(abs) delay=100 |
| 111621 | 211621 | Insensibility armor | Increase AC by 80 | 4/0 | FRIEND_WITHME(2) | 21 | 40 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=80 |
| 111624 | 211624 | Strong | Increase a party member's HP by 360. | 4/0 | PARTY(4) | 24 | 60 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=240 |
| 111627 | 211627 | Bright mind | Increase magic, curse, and poison resistance by 40 | 4/0 | FRIEND_WITHME(2) | 27 | 30 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=600s MagicR=40 DiseaseR=40 PoisonR=40 |
| 111629 | 211629 | wildness | Increase strength by 30. | 4/0 | SELF(1) | 30 | 80 | 0 |  | 0 | 10 | 0 | 100 | 0 | T4 STATS(7) dur=400s Str=30 |
| 111630 | 211630 | Insensibility shield | Increase AC by 120 | 4/0 | FRIEND_WITHME(2) | 30 | 80 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=120 |
| 111633 | 211633 | Hardness | Increase a party member's HP by 720. | 4/0 | PARTY(4) | 33 | 120 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=480 |
| 111636 | 211636 | Calm mind | Increase magic, curse, and poison resistance by 60 | 4/0 | FRIEND_WITHME(2) | 36 | 45 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=600s MagicR=60 DiseaseR=60 PoisonR=60 |
| 111639 | 211639 | Insensibility barrier | Increase AC by 160 | 4/0 | FRIEND_WITHME(2) | 39 | 80 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=160 |
| 111641 | 211641 | Harsh | A fail-safe attack that inflicts 150% damage and an additional 100 damage. | 1/0 | ENEMY(7) | 42 | 100 | 0 |  | 0 | 20 | 0 | 100 | 0 | T1 hit=200% add=+100 hitrate=100(abs) delay=100 |
| 111642 | 211642 | Mightness | Increase a party member's HP by 960. | 4/0 | PARTY(4) | 42 | 240 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=960 |
| 111645 | 211645 | Fresh mind | Increase magic, curse, and poison resistance by 80 | 4/0 | FRIEND_WITHME(2) | 45 | 60 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=600s MagicR=80 DiseaseR=80 PoisonR=80 |
| 111650 | 211650 | collapse | A fail-safe attack that inflicts 200% damage and an additional 50 damage. | 1/0 | ENEMY(7) | 51 | 120 | 0 |  | 0 | 30 | 0 | 100 | 0 | T1 hit=350% add=+50 hitrate=100(abs) delay=100 |
| 111651 | 211651 | Insensibility protector | Increase AC by 200 | 4/0 | FRIEND_WITHME(2) | 51 | 100 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=200 |
| 111654 | 211654 | Undying | Increase the max HP limit of a party member by 60% | 4/0 | PARTY(4) | 54 | 240 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHPPct=160 |
| 111655 | 211655 | Heapness | Increase the max HP limit of a party member by 1200 | 4/0 | PARTY(4) | 54 | 300 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=1200 |
| 111656 | 211656 | Greatness | Increase the max HP limit of all the party member by 1200 | 4/0 | PARTY_ALL(6) | 57 | 570 | 0 |  | 15 | 1 | 101 | 50 | 0 | T4 HP_MP(1) dur=600s radius=30 MaxHP=1200 |
| 111657 | 211657 | massiveness | Increase the max HP limit of a party member by 1500 | 4/0 | PARTY(4) | 57 | 360 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=1500 |
| 111660 | 211660 | Insensibility peel | Increase the AC by 300 | 4/0 | FRIEND_WITHME(2) | 60 | 150 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=300 |

#### Tree3 (cat 7) Debuff/Curse (+talisman attacks) (Skill=1117, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 111700 | 211700 | Gate | Teleport to your chosen resurrection spot | 8/0 | SELF(1) | 0 | 20 | 0 |  | 15 | 74 | 56 | 50 | 0 | T8 target=1 radius=10000 warp=1(to resurrection point (town)) exprecover=0 kick=0 |
| 111703 | 211703 | Malice | Decrease your enemy's defense ability by 25% | 4/0 | ENEMY(7) | 3 | 40 | 0 |  | 15 | 74 | 90 | 50 | 0 | T4 AC(2) dur=150s ACPct=75 |
| 111709 | 211709 | Clear mana | Decrease your enemy's Mana by 480. | 3/0 | ENEMY(7) | 9 | 80 | 0 |  | 15 | 74 | 56 | 50 | 0 | T3 MP first=-480 |
| 111712 | 211712 | tilt | A fail-safe attack that inflicts 120% damage. | 1/0 | ENEMY(7) | 12 | 30 | 0 |  | 0 | 1 | 0 | 100 | 0 | T1 hit=150% add=+0 hitrate=100(abs) delay=100 |
| 111715 | 211715 | Confusion | Decrease your enemy's MP by 30. | 4/0 | ENEMY(7) | 15 | 80 | 0 |  | 15 | 74 | 56 | 50 | 0 | T4 STATS(7) dur=150s Cha=-30 |
| 111721 | 211721 | Bloody | A fail-safe attack that inflicts 150% damage. | 1/0 | ENEMY(7) | 21 | 40 | 0 |  | 0 | 2 | 0 | 100 | 0 | T1 hit=300% add=+0 hitrate=100(abs) delay=100 |
| 111724 | 211724 | Slow | Decrease your enemy's attack speed by 20% | 4/0 | ENEMY(7) | 24 | 120 | 0 |  | 15 | 74 | 56 | 50 | 0 | T4 ATTACK_SPEED(5) dur=150s AttackSpeed=70 |
| 111727 | 211727 | Reverse life | Neutralizes enemy's HP bonus spells. | 4/0 | ENEMY(7) | 27 | 50 | 0 |  | 15 | 94 | 56 | 50 | 0 | T4 HP_MP(1) dur=1s MaxHPPct=99 |
| 111729 | 211729 | Eruption | Increase strength by 30. | 4/0 | SELF(1) | 30 | 80 | 0 |  | 0 | 10 | 0 | 100 | 0 | T4 STATS(7) dur=400s Str=30 |
| 111730 | 211730 | Sleep Wing | Puts a monster to sleep for 20 seconds | 7/0 | ENEMY(7) | 30 | 120 | 0 |  | 15 | 74 | 56 | 50 | 0 | T7 group=0 monster=0 target=2 state=0 radius=0 hitrate=100 dur=20 dmg=0 |
| 111733 | 211733 | Resurrection of love | Resurrect regaining 60% of the original experience lost | 5/0 | CORPSE_FRIEND(25) | 33 | 400 | 0 | 379006000 Stone of life (+0) | 15 | 250 | 11 | 50 | 0 | T5 type=3 exprecover=10 needstone=4 |
| 111736 | 211736 | Sweep mana | Decrease your enemy's Mana by 960 | 3/0 | ENEMY(7) | 36 | 160 | 0 |  | 15 | 74 | 56 | 50 | 0 | T3 MP first=-960 |
| 111739 | 211739 | raving edge | A fail-safe attack that inflicts 150% damage and an additional 100 damage. | 1/0 | ENEMY(7) | 39 | 100 | 0 |  | 0 | 20 | 0 | 100 | 0 | T1 hit=250% add=+100 hitrate=100(abs) delay=100 |
| 111742 | 211742 | Resurrection of grace | Resurrect regaining 70% of the original experience lost | 5/0 | CORPSE_FRIEND(25) | 42 | 600 | 0 | 379006000 Stone of life (+0) | 15 | 250 | 11 | 50 | 0 | T5 type=3 exprecover=10 needstone=10 |
| 111745 | 211745 | Parasite | Decrease your enemy's max HP by 20% | 4/0 | ENEMY(7) | 45 | 100 | 0 |  | 15 | 74 | 101 | 50 | 0 | T4 HP_MP(1) dur=150s MaxHPPct=80 |
| 111750 | 211750 | Hades | A fail-safe attack that inflicts 200% damage and an additional 50 damage. | 1/0 | ENEMY(7) | 51 | 120 | 0 |  | 0 | 30 | 0 | 100 | 0 | T1 hit=350% add=+50 hitrate=100(abs) delay=100 |
| 111751 | 211751 | sleep Carpet | Puts all the monsters in an area to sleep for 20 seconds | 7/0 | AREA_ENEMY(10) | 51 | 240 | 0 |  | 15 | 94 | 56 | 50 | 0 | T7 group=0 monster=0 target=2 state=0 radius=30 hitrate=100 dur=20 dmg=0 |
| 111754 | 211754 | Resurrection of favors | Resurrect regaining 80% of the original experience lost | 5/0 | CORPSE_FRIEND(25) | 54 | 800 | 0 | 379006000 Stone of life (+0) | 15 | 250 | 11 | 50 | 0 | T5 type=3 exprecover=10 needstone=30 |
| 111757 | 211757 | Torment | Decrease all enemy's defense by 30% in a certain area. | 4/0 | AREA_ENEMY(10) | 57 | 150 | 0 |  | 15 | 94 | 56 | 50 | 0 | T4 AC(2) dur=150s radius=10 ACPct=70 |
| 111760 | 211760 | Massive | Decrease your enemy's attack power by 20% | 4/0 | ENEMY(7) | 60 | 180 | 0 |  | 15 | 104 | 101 | 50 | 0 | T4 DAMAGE(4) dur=150s Attack=80 |

#### cat 9 (Skill=1119, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 111009 | 211009 | whipping | Whips the horse to temporarily increase the horse's movement speed. | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 255 | 0 | 100 | 0 | T4 SPEED(6) dur=1200s Speed=200 |

### Class 112 / 212 - master priest

#### Basic (Skill=1120, SkillLevel = required character level)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112001 | 212001 | Tiny healing | Heal 15 HP | 3/0 | FRIEND_WITHME(2) | 1 | 5 | 0 |  | 19 | 1 | 56 | 80 | 0 | T3 HP first=15 |
| 112002 | 212002 | Light strike | Light magic attack | 3/0 | ENEMY(7) | 2 | 5 | 0 |  | 15 | 54 | 56 | 50 | 0 | T3 HP first=-29 attr=magic |
| 112004 | 212004 | Strength | Increase strength by 15 | 4/0 | FRIEND_WITHME(2) | 4 | 10 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 STATS(7) dur=600s Str=15 |
| 112005 | 212005 | Light healing | Heal 30 HP | 3/0 | FRIEND_WITHME(2) | 5 | 10 | 0 |  | 19 | 1 | 56 | 80 | 0 | T3 HP first=30 |
| 112006 | 212006 | Resist poison | Increase resistance to poison by 20 | 4/0 | FRIEND_WITHME(2) | 6 | 10 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=600s PoisonR=20 |
| 112007 | 212007 | Brightness | Light magic attack | 3/0 | ENEMY(7) | 7 | 10 | 0 |  | 15 | 54 | 56 | 50 | 0 | T3 HP first=-77 attr=magic |
| 112008 | 212008 | Tiny restore | Heal 50 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 8 | 15 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=50 dur=20s |
| 112010 | 212010 | Light Attack | A magic attack that uses light | 3/0 | ENEMY(7) | 18 | 50 | 0 |  | 15 | 25 | 33 | 80 | 0 | T3 MP first=-105 overtime=-10 dur=10s attr=magic |
| 112011 | 212011 | Light Counter | A magic attack that uses light | 3/0 | ENEMY(7) | 24 | 100 | 0 |  | 15 | 31 | 33 | 80 | 0 | T3 MP first=-260 overtime=-30 dur=10s attr=magic |
| 112012 | 212012 | Critical Light | A magic attack that uses light | 3/0 | ENEMY(7) | 33 | 150 | 0 |  | 15 | 37 | 33 | 80 | 0 | T3 MP first=-385 overtime=-50 dur=10s attr=magic |

#### Tree1 (cat 5) Heal (Skill=1125, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112500 | 212500 | minor healing | Heal 60 HP | 3/0 | FRIEND_WITHME(2) | 0 | 10 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=60 |
| 112503 | 212503 | Light restore | Heal 100 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 3 | 25 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=100 dur=30s |
| 112509 | 212509 | healing | Heal 240 HP | 3/0 | FRIEND_WITHME(2) | 9 | 20 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=120 |
| 112511 | 212511 | collision | A fail-safe attack that inflicts 120% damage. | 1/0 | ENEMY(7) | 12 | 30 | 0 |  | 0 | 1 | 0 | 100 | 0 | T1 hit=200% add=+0 hitrate=100(abs) delay=100 |
| 112512 | 212512 | Restore | Heal 400 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 12 | 30 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=200 dur=30s |
| 112518 | 212518 | major healing | Heal 360 HP | 3/0 | FRIEND_WITHME(2) | 18 | 40 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=240 |
| 112520 | 212520 | shuddering | A fail-safe attack that inflicts 150% damage. | 1/0 | ENEMY(7) | 21 | 40 | 0 |  | 0 | 2 | 0 | 100 | 0 | T1 hit=250% add=+0 hitrate=100(abs) delay=100 |
| 112521 | 212521 | Major restore | Heal 600 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 21 | 100 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=400 dur=30s |
| 112525 | 212525 | Cure curse | Neutralizes any resistance decreasing spells. | 5/0 | FRIEND_WITHME(2) | 25 | 60 | 0 |  | 15 | 15 | 56 | 100 | 0 | T5 type=2 exprecover=0 needstone=0 |
| 112527 | 212527 | Great healing | Heal 720 HP | 3/0 | FRIEND_WITHME(2) | 27 | 80 | 0 |  | 15 | 20 | 56 | 80 | 0 | T3 HP first=960 |
| 112529 | 212529 | blasting | Increase strength by 30. | 4/0 | SELF(1) | 30 | 80 | 0 |  | 0 | 10 | 0 | 100 | 0 | T4 STATS(7) dur=400s Str=30 |
| 112530 | 212530 | Great restore | Heal 800 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 30 | 200 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=800 dur=30s |
| 112535 | 212535 | Cure disease | Neutralizes spells that decrease your HP | 5/0 | FRIEND_WITHME(2) | 35 | 120 | 0 |  | 15 | 15 | 56 | 100 | 0 | T5 type=1 exprecover=0 needstone=0 |
| 112536 | 212536 | Massive healing | Heal 960 HP | 3/0 | FRIEND_WITHME(2) | 36 | 160 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=960 |
| 112539 | 212539 | Massive restore | Heal 1500 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 39 | 375 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=1500 dur=30s |
| 112542 | 212542 | ruin | A fail-safe attack that inflicts 150% damage and an additional 100 damage. | 1/0 | ENEMY(7) | 42 | 100 | 0 |  | 0 | 20 | 0 | 100 | 0 | T1 hit=300% add=+100 hitrate=100(abs) delay=100 |
| 112545 | 212545 | Superior healing | Heal 1920 HP | 3/0 | FRIEND_WITHME(2) | 45 | 320 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=1920 |
| 112548 | 212548 | Superior restore | Heal 2500 HP over 20 seconds | 3/0 | FRIEND_WITHME(2) | 48 | 625 | 0 |  | 15 | 1 | 56 | 80 | 0 | T3 HP first=0 overtime=2500 dur=30s |
| 112551 | 212551 | Hellish | A fail-safe attack that inflicts 200% damage and an additional 50 damage. | 1/0 | ENEMY(7) | 51 | 120 | 0 |  | 0 | 30 | 0 | 100 | 0 | T1 hit=350% add=+50 hitrate=100(abs) delay=100 |
| 112554 | 212554 | Complete healing | Commpletely heal the HP of a friend | 3/0 | FRIEND_WITHME(2) | 54 | 960 | 0 |  | 15 | 54 | 56 | 80 | 0 | T3 HP first=10000 |
| 112557 | 212557 | Group massive healing | Heal 960 HP of all the members of your party. | 3/0 | PARTY_ALL(6) | 57 | 960 | 0 |  | 15 | 54 | 56 | 80 | 0 | T3 HP first=960 radius=30 |
| 112560 | 212560 | Group complete healing | Completely heal all the members of your party | 3/0 | PARTY_ALL(6) | 60 | 1920 | 0 |  | 15 | 64 | 56 | 80 | 0 | T3 HP first=10000 radius=30 |
| 112570 | 212570 | critical restore | Heals 3000HP over 20 seconds for party members in a certain area. | 3/0 | PARTY_ALL(6) | 70 | 1000 | 0 |  | 15 | 1 | 56 | 70 | 0 | T3 HP first=0 overtime=3000 dur=20s radius=20 |
| 112575 | 212575 | Past Recovery | Heals a friend for 2500 HP and heals for an additional 3000 HP over 20 seconds. | 3/0 | PARTY_ALL(6) | 75 | 1200 | 0 |  | 15 | 1 | 67 | 70 | 520 | T3 HP first=2500 overtime=3000 end=30 dur=20s radius=30 |
| 112580 | 212580 | Past Restore | Heals 6000 HP over 20 seconds for all party members in a certain area. | 3/0 | PARTY_ALL(6) | 80 | 1400 | 0 |  | 15 | 255 | 67 | 70 | 523 | T3 HP first=0 overtime=6000 end=30 dur=20s radius=30 |

#### Tree2 (cat 6) Buff/Aura (Skill=1126, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112603 | 212603 | Insensibility Skin | Increase AC by 20 | 4/0 | FRIEND_WITHME(2) | 3 | 10 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=20 |
| 112606 | 212606 | Grace | Increase a party member's HP by 60. | 4/0 | PARTY(4) | 6 | 15 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=60 |
| 112609 | 212609 | Resist all | Increase magic, curse, and poison resistance by 20 | 4/0 | FRIEND_WITHME(2) | 9 | 15 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=600s MagicR=20 DiseaseR=20 PoisonR=20 |
| 112611 | 212611 | Wrath | A fail-safe attack that inflicts 120% damage. | 1/0 | ENEMY(7) | 12 | 30 | 0 |  | 0 | 1 | 0 | 100 | 0 | T1 hit=150% add=+0 hitrate=100(abs) delay=100 |
| 112612 | 212612 | Insensibility shell | Increase AC by 40 | 4/0 | FRIEND_WITHME(2) | 12 | 20 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=40 |
| 112615 | 212615 | Brave | Increase a party member's HP by 240. | 4/0 | PARTY(4) | 15 | 30 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=120 |
| 112620 | 212620 | wield | A fail-safe attack that inflicts 150% damage. | 1/0 | ENEMY(7) | 21 | 40 | 0 |  | 0 | 2 | 0 | 100 | 0 | T1 hit=200% add=+0 hitrate=100(abs) delay=100 |
| 112621 | 212621 | Insensibility armor | Increase AC by 80 | 4/0 | FRIEND_WITHME(2) | 21 | 40 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=80 |
| 112624 | 212624 | Strong | Increase a party member's HP by 360. | 4/0 | PARTY(4) | 24 | 60 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=240 |
| 112627 | 212627 | Bright mind | Increase magic, curse, and poison resistance by 40 | 4/0 | FRIEND_WITHME(2) | 27 | 30 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=600s MagicR=40 DiseaseR=40 PoisonR=40 |
| 112629 | 212629 | wildness | Increase strength by 30. | 4/0 | SELF(1) | 30 | 80 | 0 |  | 0 | 10 | 0 | 100 | 0 | T4 STATS(7) dur=400s Str=30 |
| 112630 | 212630 | Insensibility shield | Increase AC by 120 | 4/0 | FRIEND_WITHME(2) | 30 | 80 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=120 |
| 112633 | 212633 | Hardness | Increase a party member's HP by 720. | 4/0 | PARTY(4) | 33 | 120 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=480 |
| 112636 | 212636 | Calm mind | Increase magic, curse, and poison resistance by 60 | 4/0 | FRIEND_WITHME(2) | 36 | 45 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=600s MagicR=60 DiseaseR=60 PoisonR=60 |
| 112639 | 212639 | Insensibility barrier | Increase AC by 160 | 4/0 | FRIEND_WITHME(2) | 39 | 80 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=160 |
| 112641 | 212641 | Harsh | A fail-safe attack that inflicts 150% damage and an additional 100 damage. | 1/0 | ENEMY(7) | 42 | 100 | 0 |  | 0 | 20 | 0 | 100 | 0 | T1 hit=200% add=+100 hitrate=100(abs) delay=100 |
| 112642 | 212642 | Mightness | Increase a party member's HP by 960. | 4/0 | PARTY(4) | 42 | 240 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=960 |
| 112645 | 212645 | Fresh mind | Increase magic, curse, and poison resistance by 80 | 4/0 | FRIEND_WITHME(2) | 45 | 60 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 RESISTANCES(8) dur=600s MagicR=80 DiseaseR=80 PoisonR=80 |
| 112650 | 212650 | collapse | A fail-safe attack that inflicts 200% damage and an additional 50 damage. | 1/0 | ENEMY(7) | 51 | 120 | 0 |  | 0 | 30 | 0 | 100 | 0 | T1 hit=350% add=+50 hitrate=100(abs) delay=100 |
| 112651 | 212651 | Insensibility protector | Increase AC by 200 | 4/0 | FRIEND_WITHME(2) | 51 | 100 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=200 |
| 112654 | 212654 | Undying | Increase the max HP limit of a party member by 60% | 4/0 | PARTY(4) | 54 | 240 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHPPct=160 |
| 112655 | 212655 | Heapness | Increase the max HP limit of a party member by 1200 | 4/0 | PARTY(4) | 54 | 300 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=1200 |
| 112656 | 212656 | Greatness | Increase the max HP limit of all the party member by 1200 | 4/0 | PARTY_ALL(6) | 57 | 570 | 0 |  | 15 | 1 | 101 | 50 | 0 | T4 HP_MP(1) dur=600s radius=30 MaxHP=1200 |
| 112657 | 212657 | massiveness | Increase the max HP limit of a party member by 1500 | 4/0 | PARTY(4) | 57 | 360 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 HP_MP(1) dur=600s MaxHP=1500 |
| 112660 | 212660 | Insensibility peel | Increase the AC by 300 | 4/0 | FRIEND_WITHME(2) | 60 | 150 | 0 |  | 15 | 1 | 56 | 50 | 0 | T4 AC(2) dur=600s AC=300 |
| 112670 | 212670 | imposingness | Increase the max HP limit of a party member by 2000 | 4/0 | PARTY(4) | 70 | 460 | 0 |  | 15 | 1 | 56 | 47 | 0 | T4 HP_MP(1) dur=600s MaxHP=2000 |
| 112671 | 212671 | Bless of God | Neutralizes resistance decreasing spells on all party members. | 5/0 | PARTY_ALL(6) | 70 | 230 | 0 |  | 15 | 1 | 45 | 70 | 0 | T5 type=2 exprecover=0 needstone=0 |
| 112672 | 212672 | Massive Binder | Increase the max HP limit of the party member by 2000 | 4/0 | PARTY_ALL(6) | 72 | 960 | 0 |  | 15 | 1 | 56 | 47 | 518 | T4 HP_MP(1) dur=600s radius=30 MaxHP=2000 |
| 112673 | 212673 | Round Insensibility | Increase the defense of all party members by 300. | 4/0 | PARTY_ALL(6) | 74 | 750 | 0 |  | 15 | 1 | 56 | 50 | 519 | T4 AC(2) dur=600s radius=30 AC=300 |
| 112674 | 212674 | Insensibility Guard | Increase the defense of a party member by 350. | 4/0 | FRIEND_WITHME(2) | 76 | 300 | 0 |  | 15 | 1 | 56 | 50 | 521 | T4 AC(2) dur=600s AC=350 |
| 112675 | 212675 | Superioris | Increase the max HP of a party member by 2500. | 4/0 | PARTY(4) | 78 | 690 | 0 |  | 15 | 1 | 56 | 47 | 522 | T4 HP_MP(1) dur=600s MaxHP=2500 |
| 112676 | 212676 | Counter Curse | Blocks all curses for 10 seconds. | 4/0 | PARTY_ALL(6) | 80 | 1200 | 0 | 379062000 Stone of Priest (+0) | 15 | 1 | 56 | 70 | 523 | T4 BLOCK_CURSE(29) dur=10s radius=20 |

#### Tree3 (cat 7) Debuff/Curse (+talisman attacks) (Skill=1127, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112700 | 212700 | Gate | Teleport to your chosen resurrection spot | 8/0 | SELF(1) | 0 | 20 | 0 |  | 15 | 74 | 56 | 50 | 0 | T8 target=1 radius=10000 warp=1(to resurrection point (town)) exprecover=0 kick=0 |
| 112703 | 212703 | Malice | Decrease your enemy's defense ability by 25% | 4/0 | ENEMY(7) | 3 | 40 | 0 |  | 15 | 74 | 56 | 50 | 0 | T4 AC(2) dur=150s ACPct=75 |
| 112709 | 212709 | Clear mana | Decrease your enemy's Mana by 480. | 3/0 | ENEMY(7) | 9 | 80 | 0 |  | 15 | 74 | 56 | 50 | 0 | T3 MP first=-480 |
| 112712 | 212712 | tilt | A fail-safe attack that inflicts 120% damage. | 1/0 | ENEMY(7) | 12 | 30 | 0 |  | 0 | 1 | 0 | 100 | 0 | T1 hit=150% add=+0 hitrate=100(abs) delay=100 |
| 112715 | 212715 | Confusion | Decrease your enemy's MP by 30. | 4/0 | ENEMY(7) | 15 | 80 | 0 |  | 15 | 74 | 56 | 50 | 0 | T4 STATS(7) dur=150s Cha=-30 |
| 112721 | 212721 | Bloody | A fail-safe attack that inflicts 150% damage. | 1/0 | ENEMY(7) | 21 | 40 | 0 |  | 0 | 2 | 0 | 100 | 0 | T1 hit=300% add=+0 hitrate=100(abs) delay=100 |
| 112724 | 212724 | Slow | Decrease your enemy's attack speed by 20% | 4/0 | ENEMY(7) | 24 | 120 | 0 |  | 15 | 74 | 56 | 50 | 0 | T4 ATTACK_SPEED(5) dur=150s AttackSpeed=70 |
| 112727 | 212727 | Reverse life | Neutralizes enemy's HP bonus spells. | 4/0 | ENEMY(7) | 27 | 50 | 0 |  | 15 | 94 | 56 | 50 | 0 | T4 HP_MP(1) dur=1s MaxHPPct=99 |
| 112729 | 212729 | eruption | Increase strength by 30. | 4/0 | SELF(1) | 30 | 80 | 0 |  | 0 | 10 | 0 | 100 | 0 | T4 STATS(7) dur=400s Str=30 |
| 112730 | 212730 | Sleep Wing | Puts a monster to sleep for 20 seconds | 7/0 | ENEMY(7) | 30 | 120 | 0 |  | 15 | 74 | 56 | 50 | 0 | T7 group=0 monster=0 target=2 state=0 radius=0 hitrate=100 dur=20 dmg=0 |
| 112733 | 212733 | Resurrection of love | Resurrect regaining 60% of the original experience lost | 5/0 | CORPSE_FRIEND(25) | 33 | 400 | 0 | 379006000 Stone of life (+0) | 15 | 250 | 11 | 50 | 0 | T5 type=3 exprecover=10 needstone=4 |
| 112736 | 212736 | Sweep mana | Decrease your enemy's Mana by 960 | 3/0 | ENEMY(7) | 36 | 160 | 0 |  | 15 | 74 | 56 | 50 | 0 | T3 MP first=-960 |
| 112739 | 212739 | raving edge | A fail-safe attack that inflicts 150% damage and an additional 100 damage. | 1/0 | ENEMY(7) | 39 | 100 | 0 |  | 0 | 20 | 0 | 100 | 0 | T1 hit=250% add=+100 hitrate=100(abs) delay=100 |
| 112742 | 212742 | Resurrection of grace | Resurrect regaining 70% of the original experience lost | 5/0 | CORPSE_FRIEND(25) | 42 | 600 | 0 | 379006000 Stone of life (+0) | 15 | 250 | 11 | 50 | 0 | T5 type=3 exprecover=10 needstone=10 |
| 112745 | 212745 | Parasite | Decrease your enemy's max HP by 20% | 4/0 | ENEMY(7) | 45 | 100 | 0 |  | 15 | 74 | 56 | 50 | 0 | T4 HP_MP(1) dur=150s MaxHPPct=80 |
| 112750 | 212750 | Hades | A fail-safe attack that inflicts 200% damage and an additional 50 damage. | 1/0 | ENEMY(7) | 51 | 120 | 0 |  | 0 | 30 | 0 | 100 | 0 | T1 hit=350% add=+50 hitrate=100(abs) delay=100 |
| 112751 | 212751 | Sleep Carpet | Puts all the monsters in an area to sleep for 20 seconds | 7/0 | AREA_ENEMY(10) | 51 | 240 | 0 |  | 15 | 94 | 56 | 50 | 0 | T7 group=0 monster=0 target=2 state=0 radius=30 hitrate=100 dur=20 dmg=0 |
| 112754 | 212754 | Resurrection of favors | Resurrect regaining 80% of the original experience lost | 5/0 | CORPSE_FRIEND(25) | 54 | 800 | 0 | 379006000 Stone of life (+0) | 15 | 250 | 11 | 50 | 0 | T5 type=3 exprecover=10 needstone=30 |
| 112757 | 212757 | Torment | Decrease all enemy's defense by 30% in a certain area. | 4/0 | AREA_ENEMY(10) | 57 | 150 | 0 |  | 15 | 94 | 56 | 50 | 0 | T4 AC(2) dur=150s radius=10 ACPct=70 |
| 112760 | 212760 | Massive | Decrease your enemy's attack power by 20% | 4/0 | ENEMY(7) | 60 | 180 | 0 |  | 15 | 104 | 56 | 50 | 0 | T4 DAMAGE(4) dur=150s Attack=80 |
| 112770 | 212770 | Subside | Decrease an enemy's attack by 20% within a certain area. | 4/0 | AREA_ENEMY(10) | 70 | 260 | 0 |  | 15 | 154 | 45 | 50 | 0 | T4 DAMAGE(4) dur=150s radius=20 Attack=80 |
| 112771 | 212771 | Superior Parasite | Decrease an enemy's HP by 30%. Does not apply to unique monsters. | 4/0 | ENEMY(7) | 75 | 350 | 0 |  | 15 | 254 | 112 | 50 | 520 | T4 HP_MP(1) dur=150s MaxHPPct=70 |
| 112772 | 212772 | Discountis | Decrease an enemy's Mana by 3840 in a certain area. | 3/0 | AREA_ENEMY(10) | 80 | 960 | 0 | 379062000 Stone of Priest (+0) | 15 | 254 | 112 | 50 | 523 | T3 MP first=-3840 end=15 radius=15 |

#### Master (cat 8) (Skill=1128, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112800 | 212800 | Daring | [Passive]Increase your defense by 20% when your HP is down to 30% or lower. | 0/0 | SELF(1) | 0 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 112802 | 212802 | Judgment | A fail-safe attack that inflicts 200% damage and an additional 150 damage. | 1/0 | ENEMY(7) | 2 | 200 | 0 | 379066000 Judgment Scroll (+0) (+consumes class stone 379062000) | 0 | 5 | 0 | 100 | 0 | T1 hit=500% add=+150 hitrate=100(abs) delay=100 |
| 112805 | 212805 | Absoluteness | [Passive]Decreases all damages received by 10% | 0/0 | SELF(1) | 5 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 112810 | 212810 | Matchless | [Passive]Decreases all damages received by 15% | 0/0 | SELF(1) | 10 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 112815 | 212815 | Helis | A fail-safe attack that inflicts 250% damage and an additional 400 damage that disregards defense. | 1/0 | ENEMY(7) | 12 | 350 | 0 |  (+consumes class stone 379062000) | 0 | 5 | 0 | 100 | 0 | T1 hit=400% add=+400 hitrate=100(abs) delay=100 |
| 112820 | 212820 | Curse Refraction | Blocks all curses for 10 seconds and has a chance to reflect the curse back onto the caster. | 4/0 | SELF(1) | 15 | 320 | 0 |  | 15 | 1 | 45 | 70 | 0 | T4 BLOCK_CURSE_REFLECT(30) dur=10s |
| 112825 | 212825 | Elysian Web | Increases magic resistance for 20 seconds to reduce any damage received from magic spells by 30%. | 4/0 | AREA_FRIEND(11) | 20 | 640 | 0 | 379062000 Stone of Priest (+0) | 15 | 1 | 56 | 50 | 0 | T4 RESIS_AND_MAGIC_DMG(27) dur=20s radius=15 ExpPct=70 |

#### cat 9 (Skill=1129, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112009 | 212009 | whipping | Whips the horse to temporarily increase the horse's movement speed. | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 255 | 0 | 100 | 0 | T4 SPEED(6) dur=1200s Speed=200 |

## Mage

Nation twin check: 151 of 166 Karus rows have an El Morad twin (MagicNum+100000, Skill+1000) with identical numeric MAGIC columns and identical Type-table values (0 of them differ only in name/description text: -). Rows with real numeric differences: 15.

- 109503 (Burn) vs 209503: T3.FirstDamage -250->-200
- 109518 (Ignition) vs 209518: Range 56->90; T3.TimeDamage -300->-336
- 109543 (specter of fire) vs 209543: T3.FirstDamage -700->-615
- 109551 (Pillar of fire) vs 209551: Range 56->90
- 109603 (Freeze) vs 209603: T3.FirstDamage -200->-118
- 109618 (Solid) vs 209618: Range 56->90; T3.FirstDamage -200->-167
- 109642 (frozen blade) vs 209642: T3.FirstDamage -215->-250
- 109651 (Ice comet) vs 209651: Range 56->90
- 109718 (Static hemisphere) vs 209718: Range 56->90
- 109742 (charged blade) vs 209742: T3.FirstDamage -315->-320
- 109751 (Static orb) vs 209751: Range 56->90
- 110518 (Ignition) vs 210518: Range 56->90
- 110557 (Fire Impact) vs 210557: Range 56->90
- 110618 (Solid) vs 210618: Range 56->90
- 110718 (Static hemisphere) vs 210718: Range 56->90

### Class 103 / 203 - beginner mage

#### Basic (Skill=1030, SkillLevel = required character level)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 103001 | 203001 | Flash | Use magic shocks to attack the enemy | 3/0 | ENEMY(7) | 1 | 4 | 0 |  | 15 | 43 | 56 | 50 | 0 | T3 HP first=-24 attr=fire |
| 103003 | 203003 | Shiver | Continuous attack done for a set period of time | 3/0 | ENEMY(7) | 3 | 5 | 0 |  | 15 | 43 | 56 | 50 | 0 | T3 HP first=0 overtime=-60 dur=20s attr=ice |
| 103005 | 203005 | Flame | Burn your enemy with flames | 3/0 | ENEMY(7) | 5 | 5 | 0 |  | 15 | 43 | 56 | 50 | 0 | T3 HP first=-62 attr=fire |
| 103007 | 203007 | Cold wave | A Glacier attack that slows down your enemy for a short period of time | 3/4 | ENEMY(7) | 7 | 5 | 0 |  | 15 | 43 | 56 | 50 | 0 | T3 HP first=-68 attr=ice; T4 SPEED(6) dur=10s Speed=65 |
| 103009 | 203009 | Spark | Spark Lightning on your enemy. It also has an added effect of neutralizing magic attacks. | 3/0 | ENEMY(7) | 9 | 7 | 0 |  | 15 | 43 | 56 | 50 | 0 | T3 HP first=-88 attr=lightning |

#### cat 9 (Skill=1039, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 103010 | 203010 | whipping | Whips the horse to temporarily increase the horse's movement speed. | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 255 | 0 | 100 | 0 | T4 SPEED(6) dur=1200s Speed=200 |

### Class 109 / 209 - novice mage

#### Basic (Skill=1090, SkillLevel = required character level)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 109001 | 209001 | Flash | Use magic shocks to attack the enemy | 3/0 | ENEMY(7) | 1 | 4 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-24 attr=fire |
| 109003 | 209003 | Shiver | Continuous attack done for a set period of time | 3/0 | ENEMY(7) | 3 | 5 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=0 overtime=-60 dur=20s attr=ice |
| 109004 | 209004 | summon friend | Retrieve a party member | 8/0 | PARTY(4) | 4 | 5 | 0 |  | 15 | 1 | 22500 | 100 | 0 | T8 target=1 radius=10000 warp=12(summon target (same zone)) exprecover=0 kick=0 |
| 109005 | 209005 | Flame | A flame attack | 3/0 | ENEMY(7) | 5 | 5 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-62 attr=fire |
| 109007 | 209007 | Cold wave | An Glacier attack that slows down your enemy for a set period of time | 3/4 | ENEMY(7) | 7 | 7 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-68 attr=ice; T4 SPEED(6) dur=10s Speed=65 |
| 109009 | 209009 | Spark | Spark Lightningity on your enemy. It also has an added effect of neutralizing magic attacks. | 3/0 | ENEMY(7) | 9 | 15 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-88 attr=lightning |
| 109015 | 209015 | Gate | Teleport to the chosen resurrection spot | 8/0 | SELF(1) | 15 | 30 | 0 |  | 15 | 100 | 56 | 30 | 0 | T8 target=1 radius=10000 warp=1(to resurrection point (town)) exprecover=0 kick=0 |
| 109035 | 209035 | Escape | Teleport all party members to your resurrection spot | 8/0 | PARTY_ALL(6) | 35 | 400 | 0 |  | 15 | 250 | 22500 | 30 | 0 | T8 target=1 radius=10000 warp=1(to resurrection point (town)) exprecover=0 kick=0 |

#### Tree1 (cat 5) Flame (Skill=1095, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 109503 | 209503 | Burn | A fail-safe flame attack | 3/0 | ENEMY(7) | 3 | 20 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-250 attr=fire |
| 109506 | 209506 | Resist fire | Increase resistance to fire by 20 | 4/0 | FRIEND_WITHME(2) | 6 | 15 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s FireR=20 |
| 109509 | 209509 | Blaze | Burn your enemy with flames for a set period of time | 3/0 | ENEMY(7) | 9 | 30 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=0 overtime=-280 dur=20s attr=fire |
| 109515 | 209515 | Fire ball | Launch fireball at your enemy from far away | 3/0 | ENEMY(7) | 15 | 50 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-308 attr=fire |
| 109518 | 209518 | Ignition | Spark flames on your enemy. �Additional damage is done to the enemy for a set period of time. | 3/0 | ENEMY(7) | 18 | 60 | 0 |  | 10 | 1 | 56 | 30 | 0 | T3 HP first=-238 overtime=-300 dur=20s attr=fire |
| 109524 | 209524 | Endure fire | Increase resistance to fire by 50 | 4/0 | FRIEND_WITHME(2) | 24 | 50 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s FireR=50 |
| 109527 | 209527 | Fire spear | Launch fire spears at your enemy from far away | 3/0 | ENEMY(7) | 27 | 80 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-588 attr=fire |
| 109533 | 209533 | Fire burst | Launch an explosive burst of fire at your enemy from far away | 3/0 | AREA_ENEMY(10) | 33 | 150 | 0 |  | 15 | 1 | 90 | 30 | 0 | T3 HP first=-588 attr=fire radius=8 |
| 109535 | 209535 | Fire blast | Launch a fire blast at your enemy from far away | 3/0 | ENEMY(7) | 35 | 150 | 0 | 370001000 Spell of Fire Blast (+0) | 15 | 43 | 78 | 30 | 0 | T3 HP first=-840 attr=fire |
| 109539 | 209539 | Hell fire | Burn your enemy with the flames of hell for a set period of time | 3/0 | ENEMY(7) | 39 | 150 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-480 overtime=-1120 dur=20s attr=fire |
| 109542 | 209542 | fire blade | Hit your enemy with a staff. Additional fire damage will be given. | 1/3 | ENEMY(7) | 42 | 100 | 0 |  | 0 | 0 | 22 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(rel) delay=100; T3 HP first=-450 attr=fire |
| 109543 | 209543 | specter of fire | Casts a fail-safe fire spell. | 3/0 | ENEMY(7) | 43 | 75 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-700 attr=fire |
| 109545 | 209545 | Inferno | Retrieve fires of hell onto a specific area. | 3/0 | AREA_ENEMY(10) | 45 | 200 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-504 end=-304 attr=fire radius=15 |
| 109548 | 209548 | Immunity fire | Increase resistance to fire by 80 | 4/0 | FRIEND_WITHME(2) | 48 | 80 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s FireR=80 |
| 109551 | 209551 | Pillar of fire | Engulfs the enemy in a column of fire. | 3/0 | ENEMY(7) | 51 | 160 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=-1260 attr=fire |
| 109554 | 209554 | Fire Thorn | Inflict flame damage and absorb HP. HP absorption applies to other players only. � | 3/0 | ENEMY(7) | 54 | 220 | 0 | 379069000 Spell of thorn (+0) (+consumes class stone 379061000) | 15 | 60 | 56 | 30 | 0 | T3 HP-drain first=-1550 attr=fire |
| 109556 | 209556 | Manes of fire | Casts a fail-safe fire spell. | 3/0 | ENEMY(7) | 56 | 95 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-1015 attr=fire |
| 109557 | 209557 | Fire Impact | Inflict powerful flame damage with continuous damage lasting for a short period of time. | 3/0 | ENEMY(7) | 57 | 220 | 0 | 379070000 Spell of impact (+0) | 15 | 203 | 56 | 30 | 0 | T3 HP first=-1260 overtime=-1000 dur=10s attr=fire |
| 109560 | 209560 | Supernova | Summons a supernova that explodes with great power | 3/0 | AREA_ENEMY(10) | 60 | 400 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-1800 overtime=-600 end=-900 dur=20s attr=fire radius=15 |

#### Tree2 (cat 6) Glacier (Skill=1096, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 109603 | 209603 | Freeze | A fail-safe Glacier attack | 3/4 | ENEMY(7) | 3 | 20 | 0 |  | 15 | 1 | 11 | 100 | 0 | T3 HP first=-200 attr=ice; T4 SPEED(6) dur=10s Speed=50 |
| 109606 | 209606 | Resist cold | Increase resistance to Glacier by 20 | 4/0 | FRIEND_WITHME(2) | 6 | 15 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s ColdR=20 |
| 109609 | 209609 | Chill | An attack that lowers the enemy's body temperature. It slows down your enemy for 20 seconds | 3/4 | ENEMY(7) | 9 | 30 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=0 overtime=-196 dur=20s attr=ice; T4 SPEED(6) dur=11s Speed=48 |
| 109612 | 209612 | Frozen armor | Increases glacier resistance. | 4/0 | FRIEND_WITHME(2) | 12 | 40 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 AC(2) dur=300s AC=60 |
| 109615 | 209615 | Ice arrow | A Glacier magic attack that allows you to attack an opponent from far away. �It also slows down your enemy for a set period of time. | 3/4 | ENEMY(7) | 15 | 50 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-216 attr=ice; T4 SPEED(6) dur=12s Speed=46 |
| 109618 | 209618 | Solid | Inflict Glacier damage to your enemy. It has an added effect of slowing down your enemy. | 3/4 | ENEMY(7) | 18 | 60 | 0 |  | 15 | 1 | 56 | 30 | 0 | T3 HP first=-200 overtime=-236 dur=20s attr=ice; T4 SPEED(6) dur=13s Speed=44 |
| 109624 | 209624 | Endure cold | Increase resistance to Glacier by 50 | 4/0 | FRIEND_WITHME(2) | 24 | 50 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s ColdR=50 |
| 109627 | 209627 | Ice orb | A Glacier magic attack that allows you to attack an opponent from far away. �It also slows down your enemy for a set period of time. | 3/4 | ENEMY(7) | 27 | 80 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-412 attr=ice; T4 SPEED(6) dur=14s Speed=42 |
| 109630 | 209630 | Frozen shell | Increases glacier resistance. | 4/0 | FRIEND_WITHME(2) | 30 | 80 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 AC(2) dur=300s AC=120 |
| 109633 | 209633 | Ice burst | Launch an explosive ball of ice at your enemy from far away | 3/4 | AREA_ENEMY(10) | 33 | 150 | 0 |  | 15 | 1 | 90 | 30 | 0 | T3 HP first=-412 attr=ice radius=8; T4 SPEED(6) dur=15s radius=5 Speed=40 |
| 109635 | 209635 | Ice blast | Launch an Ice Blast at an an enemy from far away. | 3/4 | ENEMY(7) | 35 | 150 | 0 | 370002000 Spell of Glacier Blast (+0) | 15 | 43 | 78 | 30 | 0 | T3 HP first=-588 attr=ice; T4 SPEED(6) dur=16s Speed=38 |
| 109639 | 209639 | Frostbite | Causes frostbite to an enemy. �It has an added effect of slowing down your enemy. | 3/4 | ENEMY(7) | 39 | 150 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-336 overtime=-784 dur=20s attr=ice; T4 SPEED(6) dur=17s radius=5 Speed=36 |
| 109642 | 209642 | frozen blade | Hit your enemy with a staff. Additional glacier damage will be given. | 1/3 | ENEMY(7) | 42 | 100 | 0 |  | 0 | 0 | 22 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(rel) delay=100; T3 HP first=-215 attr=ice |
| 109643 | 209643 | specter of Ice | Casts a fail-safe glacier spell. | 3/0 | ENEMY(7) | 43 | 75 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-431 attr=ice |
| 109645 | 209645 | Blizzard | Snowstorm attack that inflicts damage to all your enemies around you. Also temporarily slows down the enemies. | 3/4 | AREA_ENEMY(10) | 45 | 200 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-353 end=-213 attr=ice radius=15; T4 SPEED(6) dur=18s radius=15 Speed=34 |
| 109648 | 209648 | Immunity cold | Increase resistance to Glacier by 80 | 4/0 | FRIEND_WITHME(2) | 48 | 80 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s ColdR=80 |
| 109651 | 209651 | Ice comet | Summons an ice comet to attack your enemy | 3/4 | ENEMY(7) | 51 | 160 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=-882 attr=ice; T4 SPEED(6) dur=19s Speed=32 |
| 109654 | 209654 | Ice barrier | Increases glacier resistance. | 4/0 | FRIEND_WITHME(2) | 54 | 120 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 AC(2) dur=300s AC=180 |
| 109656 | 209656 | Manes of Ice | Casts a fail-safe glacier spell. | 3/0 | ENEMY(7) | 56 | 95 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-711 attr=ice |
| 109657 | 209657 | Ice Impact | Inflicts powerful Glacier damage with continuous damage lasting for a short period of time. | 3/4 | ENEMY(7) | 57 | 220 | 0 | 379070000 Spell of impact (+0) | 15 | 203 | 56 | 30 | 0 | T3 HP first=-882 overtime=-700 dur=10s attr=ice; T4 SPEED(6) dur=10s Speed=50 |
| 109660 | 209660 | Frost nova | Explosion of ice glacier. Inflicts damage to enemies in a certain area and slows them down | 3/4 | AREA_ENEMY(10) | 60 | 400 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-1260 overtime=-420 end=-630 dur=20s attr=ice radius=15; T4 SPEED(6) dur=20s radius=15 Speed=30 |

#### Tree3 (cat 7) Lightning (Skill=1097, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 109703 | 209703 | Charge | A fail-safe Lightning attack | 3/0 | ENEMY(7) | 3 | 20 | 0 |  | 15 | 1 | 11 | 100 | 0 | T3 HP first=-118 attr=lightning |
| 109706 | 209706 | Resist lightning | Increase resistance to Lightningty by 20 | 4/0 | FRIEND_WITHME(2) | 6 | 15 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s LightningR=20 |
| 109709 | 209709 | Counter spell | A Lightning magic attak that neutralizes enemy's magic attack | 3/0 | ENEMY(7) | 9 | 30 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=0 overtime=-196 dur=20s attr=lightning |
| 109715 | 209715 | Lightning | An Lightning attack that allows you to attack an enemy from far away. It has an added effect of neutralizing enemy's magic attack | 3/0 | ENEMY(7) | 15 | 50 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-216 attr=lightning |
| 109718 | 209718 | Static hemisphere | Form an Lightning sphere around your enemy | 3/0 | ENEMY(7) | 18 | 60 | 0 |  | 15 | 1 | 56 | 30 | 0 | T3 HP first=-167 overtime=-236 dur=20s attr=lightning |
| 109724 | 209724 | Endure lightning | Increase resistance to Lightning by 50 | 4/0 | FRIEND_WITHME(2) | 24 | 50 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s LightningR=50 |
| 109727 | 209727 | Thunder | A Lightning magic that allows you to attack an enemy from far away | 3/0 | ENEMY(7) | 27 | 80 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-412 attr=lightning |
| 109733 | 209733 | Thunder burst | Launch a Lightning sphere to attack an enemy from far away | 3/0 | AREA_ENEMY(10) | 33 | 150 | 0 |  | 15 | 1 | 90 | 30 | 0 | T3 HP first=-412 attr=lightning radius=8 |
| 109735 | 209735 | Thunder blast | Launch a thunder blast to attack an enemy from far away | 3/0 | ENEMY(7) | 35 | 150 | 0 | 370003000 Spell of Thunder Blast (+0) | 15 | 43 | 78 | 30 | 0 | T3 HP first=-588 attr=lightning |
| 109739 | 209739 | Discharge | Shock your enemy with Lightningity for a certain period of time. It has an added effect of neutralizing enemy's magic attack | 3/0 | ENEMY(7) | 39 | 150 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-336 overtime=-784 dur=20s attr=lightning |
| 109742 | 209742 | charged blade | Hit your enemy with a staff. Additional lightning damage will be given. | 1/3 | ENEMY(7) | 42 | 100 | 0 |  | 0 | 0 | 22 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(rel) delay=100; T3 HP first=-315 attr=lightning |
| 109743 | 209743 | specter of thunder | Casts a fail-safe lightning spell. | 3/0 | ENEMY(7) | 43 | 75 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-431 attr=lightning |
| 109745 | 209745 | Thundercloud | Summon a thundercloud. | 3/0 | AREA_ENEMY(10) | 45 | 200 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-353 end=-213 attr=lightning radius=15 |
| 109748 | 209748 | Immunity lightning | Increase resistance to Lightning by 80 | 4/0 | FRIEND_WITHME(2) | 48 | 80 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s LightningR=80 |
| 109751 | 209751 | Static orb | Launch a charged Lightning ball | 3/0 | ENEMY(7) | 51 | 160 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=-882 attr=lightning |
| 109754 | 209754 | Static Thorn | Inflict lightning damage and absorb HP. HP absorption applies to other players only. � | 3/0 | ENEMY(7) | 54 | 220 | 0 | 379069000 Spell of thorn (+0) (+consumes class stone 379061000) | 15 | 60 | 56 | 30 | 0 | T3 HP-drain first=-1100 attr=lightning |
| 109756 | 209756 | Manes of thunder | Casts a fail-safe lightning spell. | 3/0 | ENEMY(7) | 56 | 95 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-711 attr=lightning |
| 109757 | 209757 | Thunder Impact | Inflict powerful Lightning damage with continuous damage lasting for a short period of time. | 3/0 | ENEMY(7) | 57 | 220 | 0 | 379070000 Spell of impact (+0) | 15 | 203 | 56 | 30 | 0 | T3 HP first=-882 overtime=-700 dur=10s attr=lightning |
| 109760 | 209760 | Static nova | A great Lightning explosion | 3/0 | AREA_ENEMY(10) | 60 | 400 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-1500 overtime=-420 end=-630 dur=20s attr=lightning radius=15 |

#### cat 9 (Skill=1099, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 109036 | 209036 | whipping | Whips the horse to temporarily increase the horse's movement speed. | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 255 | 0 | 100 | 0 | T4 SPEED(6) dur=1200s Speed=200 |

### Class 110 / 210 - master mage

#### Basic (Skill=1100, SkillLevel = required character level)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110001 | 210001 | Flash | Use magic shocks to attack the enemy | 3/0 | ENEMY(7) | 1 | 4 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-24 attr=fire |
| 110003 | 210003 | Shiver | Continuous attack done for a set period of time | 3/0 | ENEMY(7) | 3 | 5 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=0 overtime=-60 dur=20s attr=ice |
| 110004 | 210004 | summon friend | Retrieve a party member | 8/0 | PARTY(4) | 4 | 5 | 0 |  | 15 | 1 | 22500 | 100 | 0 | T8 target=1 radius=10000 warp=12(summon target (same zone)) exprecover=0 kick=0 |
| 110005 | 210005 | Flame | A flame attack | 3/0 | ENEMY(7) | 5 | 5 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-62 attr=fire |
| 110007 | 210007 | Cold wave | An Glacier attack that slows down your enemy for a set period of time | 3/4 | ENEMY(7) | 7 | 7 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-68 attr=ice; T4 SPEED(6) dur=10s Speed=65 |
| 110009 | 210009 | Spark | Spark Lightningity on your enemy. It also has an added effect of neutralizing magic attacks. | 3/0 | ENEMY(7) | 9 | 15 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-88 attr=lightning |
| 110015 | 210015 | Gate | Teleport to the chosen resurrection spot | 8/0 | SELF(1) | 15 | 30 | 0 |  | 15 | 100 | 56 | 30 | 0 | T8 target=1 radius=10000 warp=1(to resurrection point (town)) exprecover=0 kick=0 |
| 110035 | 210035 | Escape | Teleport all party members to your resurrection spot | 8/0 | PARTY_ALL(6) | 35 | 400 | 0 |  | 15 | 250 | 22500 | 30 | 0 | T8 target=1 radius=10000 warp=1(to resurrection point (town)) exprecover=0 kick=0 |

#### Tree1 (cat 5) Flame (Skill=1105, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110503 | 210503 | Burn | A fail-safe flame attack | 3/0 | ENEMY(7) | 3 | 20 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-168 attr=fire |
| 110506 | 210506 | Resist fire | Increase resistance to fire by 20 | 4/0 | FRIEND_WITHME(2) | 6 | 15 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s FireR=20 |
| 110509 | 210509 | Blaze | Burn your enemy with flames for a set period of time | 3/0 | ENEMY(7) | 9 | 30 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=0 overtime=-280 dur=20s attr=fire |
| 110515 | 210515 | Fire ball | Launch fireball at your enemy from far away | 3/0 | ENEMY(7) | 15 | 50 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-308 attr=fire |
| 110518 | 210518 | Ignition | Spark flames on your enemy. �Additional damage is done to the enemy for a set period of time. | 3/0 | ENEMY(7) | 18 | 60 | 0 |  | 10 | 1 | 56 | 30 | 0 | T3 HP first=-238 overtime=-336 dur=20s attr=fire |
| 110524 | 210524 | Endure fire | Increase resistance to fire by 50 | 4/0 | FRIEND_WITHME(2) | 24 | 50 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s FireR=50 |
| 110527 | 210527 | Fire spear | Launch fire spears at your enemy from far away | 3/0 | ENEMY(7) | 27 | 80 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-588 attr=fire |
| 110533 | 210533 | Fire burst | Launch an explosive burst of fire at your enemy from far away | 3/0 | AREA_ENEMY(10) | 33 | 150 | 0 |  | 15 | 1 | 90 | 30 | 0 | T3 HP first=-588 attr=fire radius=8 |
| 110535 | 210535 | Fire blast | Launch a fire blast at your enemy from far away | 3/0 | ENEMY(7) | 35 | 150 | 0 | 370001000 Spell of Fire Blast (+0) | 15 | 43 | 78 | 30 | 0 | T3 HP first=-840 attr=fire |
| 110539 | 210539 | Hell fire | Burn your enemy with the flames of hell for a set period of time | 3/0 | ENEMY(7) | 39 | 150 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-480 overtime=-1120 dur=20s attr=fire |
| 110542 | 210542 | fire blade | Hit your enemy with a staff. Additional fire damage will be given. | 1/3 | ENEMY(7) | 42 | 100 | 0 |  | 0 | 0 | 11 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(rel) delay=100; T3 HP first=-336 attr=fire |
| 110543 | 210543 | specter of fire | Casts a fail-safe fire spell. | 3/0 | ENEMY(7) | 43 | 75 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-615 attr=fire |
| 110545 | 210545 | Inferno | Retrieve fires of hell onto a specific area. | 3/0 | AREA_ENEMY(10) | 45 | 200 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-504 end=-304 attr=fire radius=15 |
| 110548 | 210548 | Immunity fire | Increase resistance to fire by 80 | 4/0 | FRIEND_WITHME(2) | 48 | 80 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s FireR=80 |
| 110551 | 210551 | Pillar of fire | Engulfs the enemy in a column of fire. | 3/0 | ENEMY(7) | 51 | 160 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=-1260 attr=fire |
| 110554 | 210554 | Fire Thorn | Inflict flame damage and absorb HP. HP absorption applies to other players only. � | 3/0 | ENEMY(7) | 54 | 220 | 0 | 379069000 Spell of thorn (+0) (+consumes class stone 379061000) | 15 | 60 | 56 | 30 | 0 | T3 HP-drain first=-1550 attr=fire |
| 110556 | 210556 | Manes of fire | Casts a fail-safe fire spell. | 3/0 | ENEMY(7) | 56 | 95 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-1015 attr=fire |
| 110557 | 210557 | Fire Impact | Inflict powerful flame damage with continuous damage lasting for a short period of time. | 3/0 | ENEMY(7) | 57 | 220 | 0 | 379070000 Spell of impact (+0) | 15 | 203 | 56 | 30 | 0 | T3 HP first=-1260 overtime=-1000 dur=10s attr=fire |
| 110560 | 210560 | Supernova | Summons a supernova that explodes with great power | 3/0 | AREA_ENEMY(10) | 60 | 400 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-1800 overtime=-600 end=-900 dur=20s attr=fire radius=15 |
| 110570 | 210570 | incineration | Summons a flaming meteor to strike an enemy. | 3/0 | ENEMY(7) | 70 | 390 | 0 |  | 11 | 213 | 45 | 30 | 0 | T3 HP first=-2500 attr=fire |
| 110571 | 210571 | meteor Fall | Summons flaming meteors to attack enemies within a certain area. | 3/0 | AREA_ENEMY(10) | 70 | 600 | 0 |  | 13 | 183 | 45 | 30 | 0 | T3 HP first=-2100 overtime=-600 end=-1050 dur=20s attr=fire radius=15 |
| 110572 | 210572 | Fire Staff | A powerful fire attack with a staff. | 1/3 | ENEMY(7) | 72 | 300 | 0 |  | 0 | 0 | 22 | 100 | 515 | T1 hit=100% add=+100 hitrate=100(rel) delay=100; T3 HP first=-2500 attr=fire |
| 110573 | 210573 | Fire Armor | Inflict a powerful fire damage and deal continuous fire damage. | 4/0 | SELF(1) | 75 | 250 | 0 | 379061000 Stone of Mage (+0) | 15 | 250 | 0 | 100 | 516 | T4 MAGE_ARMOR(25) dur=120s |
| 110574 | 210574 | Vampiric Fire | Launch a fireball absorbing some of the MP used as HP. | 3/0 | ENEMY(7) | 80 | 350 | 0 |  | 15 | 1 | 56 | 100 | 517 | T3 MP-drain first=-3000 attr=fire |
| 110575 | 210575 | Igzination | Summon a flaming meteor to strike an enemy. | 3/0 | ENEMY(7) | 80 | 390 | 0 |  | 11 | 213 | 78 | 30 | 517 | T3 HP first=-4500 attr=fire |

#### Tree2 (cat 6) Glacier (Skill=1106, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110603 | 210603 | Freeze | A fail-safe Glacier attack | 3/4 | ENEMY(7) | 3 | 20 | 0 |  | 15 | 1 | 11 | 100 | 0 | T3 HP first=-118 attr=ice; T4 SPEED(6) dur=10s Speed=50 |
| 110606 | 210606 | Resist cold | Increase resistance to Glacier by 20 | 4/0 | FRIEND_WITHME(2) | 6 | 15 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s ColdR=20 |
| 110609 | 210609 | Chill | An attack that lowers the enemy's body temperature. It slows down your enemy for 20 seconds | 3/4 | ENEMY(7) | 9 | 30 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=0 overtime=-196 dur=20s attr=ice; T4 SPEED(6) dur=11s Speed=48 |
| 110612 | 210612 | Frozen armor | Increases glacier resistance. | 4/0 | FRIEND_WITHME(2) | 12 | 40 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 AC(2) dur=300s AC=60 |
| 110615 | 210615 | Ice arrow | A Glacier magic attack that allows you to attack an opponent from far away. �It also slows down your enemy for a set period of time. | 3/4 | ENEMY(7) | 15 | 50 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-216 attr=ice; T4 SPEED(6) dur=12s Speed=46 |
| 110618 | 210618 | Solid | Inflict Glacier damage to your enemy. It has an added effect of slowing down your enemy. | 3/4 | ENEMY(7) | 18 | 60 | 0 |  | 15 | 1 | 56 | 30 | 0 | T3 HP first=-167 overtime=-236 dur=20s attr=ice; T4 SPEED(6) dur=13s Speed=44 |
| 110624 | 210624 | Endure cold | Increase resistance to Glacier by 50 | 4/0 | FRIEND_WITHME(2) | 24 | 50 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s ColdR=50 |
| 110627 | 210627 | Ice orb | A Glacier magic attack that allows you to attack an opponent from far away. �It also slows down your enemy for a set period of time. | 3/4 | ENEMY(7) | 27 | 80 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-412 attr=ice; T4 SPEED(6) dur=14s Speed=42 |
| 110630 | 210630 | Frozen shell | Increases glacier resistance. | 4/0 | FRIEND_WITHME(2) | 30 | 80 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 AC(2) dur=300s AC=120 |
| 110633 | 210633 | Ice burst | Launch an explosive ball of ice at your enemy from far away | 3/4 | AREA_ENEMY(10) | 33 | 150 | 0 |  | 15 | 1 | 90 | 30 | 0 | T3 HP first=-412 attr=ice radius=8; T4 SPEED(6) dur=15s radius=5 Speed=40 |
| 110635 | 210635 | Ice blast | Launch an Ice Blast at an an enemy from far away. | 3/4 | ENEMY(7) | 35 | 150 | 0 | 370002000 Spell of Glacier Blast (+0) | 15 | 43 | 78 | 30 | 0 | T3 HP first=-588 attr=ice; T4 SPEED(6) dur=16s Speed=38 |
| 110639 | 210639 | Frostbite | Causes frostbite to an enemy. �It has an added effect of slowing down your enemy. | 3/4 | ENEMY(7) | 39 | 150 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-336 overtime=-784 dur=20s attr=ice; T4 SPEED(6) dur=17s radius=5 Speed=36 |
| 110642 | 210642 | frozen blade | Hit your enemy with a staff. Additional glacier damage will be given. | 1/3 | ENEMY(7) | 42 | 100 | 0 |  | 0 | 0 | 11 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(rel) delay=100; T3 HP first=-236 attr=ice |
| 110643 | 210643 | specter of Ice | Casts a fail-safe glacier spell. | 3/0 | ENEMY(7) | 43 | 75 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-431 attr=ice |
| 110645 | 210645 | Blizzard | Snowstorm attack that inflicts damage to all your enemies around you. Also temporarily slows down the enemies. | 3/4 | AREA_ENEMY(10) | 45 | 200 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-353 end=-213 attr=ice radius=15; T4 SPEED(6) dur=18s radius=15 Speed=34 |
| 110648 | 210648 | Immunity cold | Increase resistance to Glacier by 80 | 4/0 | FRIEND_WITHME(2) | 48 | 80 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s ColdR=80 |
| 110651 | 210651 | Ice comet | Summons an ice comet to attack your enemy | 3/4 | ENEMY(7) | 51 | 160 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=-882 attr=ice; T4 SPEED(6) dur=19s Speed=32 |
| 110654 | 210654 | Ice barrier | Increases glacier resistance. | 4/0 | FRIEND_WITHME(2) | 54 | 120 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 AC(2) dur=300s AC=180 |
| 110656 | 210656 | Manes of Ice | Casts a fail-safe glacier spell. | 3/0 | ENEMY(7) | 56 | 95 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-711 attr=ice |
| 110657 | 210657 | Ice Impact | Inflicts powerful Glacier damage with continuous damage lasting for a short period of time. | 3/4 | ENEMY(7) | 57 | 220 | 0 | 379070000 Spell of impact (+0) | 15 | 203 | 56 | 30 | 0 | T3 HP first=-882 overtime=-700 dur=10s attr=ice; T4 SPEED(6) dur=10s Speed=50 |
| 110660 | 210660 | Frost nova | Explosion of ice glacier. Inflicts damage to enemies in a certain area and slows them down | 3/4 | AREA_ENEMY(10) | 60 | 400 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-1260 overtime=-420 end=-630 attr=ice radius=15; T4 SPEED(6) dur=20s radius=15 Speed=30 |
| 110670 | 210670 | Prismatic | Summon a frozen meteor to strike an enemy. | 3/4 | ENEMY(7) | 70 | 390 | 0 |  | 11 | 213 | 45 | 30 | 0 | T3 HP first=-1750 attr=ice; T4 SPEED(6) dur=10s Speed=50 |
| 110671 | 210671 | ice storm | Summon frozen meteors to attack enemies within a certain area. | 3/4 | AREA_ENEMY(10) | 70 | 600 | 0 |  | 13 | 183 | 45 | 30 | 0 | T3 HP first=-1470 end=-735 attr=ice radius=15; T4 SPEED(6) dur=20s radius=15 Speed=30 |
| 110672 | 210672 | Ice Staff | A powerful ice attack with a staff. | 1/3 | ENEMY(7) | 72 | 300 | 0 |  | 0 | 0 | 22 | 100 | 515 | T1 hit=100% add=+100 hitrate=100(rel) delay=100; T3 HP first=-2500 attr=ice |
| 110673 | 210673 | Ice Armor | Inflicts a powerful ice damage and has a chance to slow down the enemy. | 4/0 | SELF(1) | 75 | 250 | 0 | 379061000 Stone of Mage (+0) | 15 | 250 | 0 | 100 | 516 | T4 MAGE_ARMOR(25) dur=120s |
| 110674 | 210674 | Freezing Distance | Freeze your opponent with ice for a short period of time. The freezing success rate is proportional to the opponent's current HP level. Does not apply to monste | 4/0 | ENEMY(7) | 80 | 350 | 0 | 379061000 Stone of Mage (+0) | 15 | 250 | 56 | 100 | 517 | T4 FREEZE(22) dur=15s Speed=1 |

#### Tree3 (cat 7) Lightning (Skill=1107, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110703 | 210703 | Charge | A fail-safe Lightning attack | 3/0 | ENEMY(7) | 3 | 20 | 0 |  | 15 | 1 | 11 | 100 | 0 | T3 HP first=-118 attr=lightning |
| 110706 | 210706 | Resist lightning | Increase resistance to Lightning by 20 | 4/0 | FRIEND_WITHME(2) | 6 | 15 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s LightningR=20 |
| 110709 | 210709 | Counter spell | A Lightning magic attack that neutralizes enemy's magic attack | 3/0 | ENEMY(7) | 9 | 30 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=0 overtime=-196 dur=20s attr=lightning |
| 110715 | 210715 | Lightning | An Lightning attack that allows you to attack an enemy from far away. It has an added effect of neutralizing enemy's magic attack | 3/0 | ENEMY(7) | 15 | 50 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-216 attr=lightning |
| 110718 | 210718 | Static hemisphere | Form an Lightning sphere around your enemy | 3/0 | ENEMY(7) | 18 | 60 | 0 |  | 15 | 1 | 56 | 30 | 0 | T3 HP first=-167 overtime=-236 dur=20s attr=lightning |
| 110724 | 210724 | Endure lightning | Increase resistance to Lightning by 50 | 4/0 | FRIEND_WITHME(2) | 24 | 50 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s LightningR=50 |
| 110727 | 210727 | Thunder | A Lightning magic that allows you to attack an enemy from far away | 3/0 | ENEMY(7) | 27 | 80 | 0 |  | 15 | 43 | 78 | 30 | 0 | T3 HP first=-412 attr=lightning |
| 110733 | 210733 | Thunder burst | Launch a Lightning sphere to attack an enemy from far away | 3/0 | AREA_ENEMY(10) | 33 | 150 | 0 |  | 15 | 1 | 90 | 30 | 0 | T3 HP first=-412 attr=lightning radius=8 |
| 110735 | 210735 | Thunder blast | Launch a thunder blast to attack an enemy from far away | 3/0 | ENEMY(7) | 35 | 150 | 0 | 370003000 Spell of Thunder Blast (+0) | 15 | 43 | 78 | 30 | 0 | T3 HP first=-588 attr=lightning |
| 110739 | 210739 | Discharge | Shock your enemy with Lightningity for a certain period of time. It has an added effect of neutralizing enemy's magic attack | 3/0 | ENEMY(7) | 39 | 150 | 0 |  | 15 | 43 | 56 | 30 | 0 | T3 HP first=-336 overtime=-784 dur=20s attr=lightning |
| 110742 | 210742 | charged blade | Hit your enemy with a staff. Additional lightning damage will be given. | 1/3 | ENEMY(7) | 42 | 100 | 0 |  | 0 | 0 | 11 | 100 | 0 | T1 hit=100% add=+0 hitrate=100(rel) delay=100; T3 HP first=-236 attr=lightning |
| 110743 | 210743 | specter of thunder | Casts a fail-safe lightning spell. | 3/0 | ENEMY(7) | 43 | 75 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-431 attr=lightning |
| 110745 | 210745 | Thundercloud | Summon a thundercloud. | 3/0 | AREA_ENEMY(10) | 45 | 200 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-353 end=-213 attr=lightning radius=15 |
| 110748 | 210748 | Immunity lightning | Increase resistance to Lightning by 80 | 4/0 | FRIEND_WITHME(2) | 48 | 80 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 RESISTANCES(8) dur=300s LightningR=80 |
| 110751 | 210751 | Static orb | Launch a charged Lightning ball | 3/0 | ENEMY(7) | 51 | 160 | 0 |  | 15 | 53 | 56 | 30 | 0 | T3 HP first=-882 attr=lightning |
| 110754 | 210754 | Static Thorn | Inflict lightning damage and absorb HP. HP absorption applies to other players only. � | 3/0 | ENEMY(7) | 54 | 220 | 0 | 379069000 Spell of thorn (+0) (+consumes class stone 379061000) | 15 | 60 | 56 | 30 | 0 | T3 HP-drain first=-1100 attr=lightning |
| 110756 | 210756 | Manes of thunder | Casts a fail-safe lightning spell. | 3/0 | ENEMY(7) | 56 | 95 | 0 |  | 10 | 1 | 11 | 100 | 0 | T3 HP first=-711 attr=lightning |
| 110757 | 210757 | Thunder Impact | Inflict powerful Lightning damage with continuous damage lasting for a short period of time. | 3/0 | ENEMY(7) | 57 | 220 | 0 | 379070000 Spell of impact (+0) | 15 | 203 | 56 | 30 | 0 | T3 HP first=-882 overtime=-700 dur=10s attr=lightning |
| 110760 | 210760 | Static nova | A great Lightning explosion | 3/0 | AREA_ENEMY(10) | 60 | 400 | 0 |  | 15 | 153 | 56 | 30 | 0 | T3 HP first=-1260 overtime=-420 end=-630 attr=lightning radius=15 |
| 110762 | 210762 | Light Shock | Temporarily blind an enemy with a bolt of light. Does not apply to monsters. | 4/0 | AREA_ENEMY(10) | 62 | 400 | 0 |  | 15 | 1 | 56 | 30 | 0 | T4 DISABLE_TARGETING(20) dur=5s radius=10 |
| 110770 | 210770 | Stun Cloud | Summon an electrically charged meteor to strike an enemy. | 3/0 | ENEMY(7) | 70 | 390 | 0 |  | 11 | 213 | 45 | 30 | 0 | T3 HP first=-1750 attr=lightning |
| 110771 | 210771 | Chain lightning | Summon electrically charged meteors to attack enemies within a certain area. | 3/0 | AREA_ENEMY(10) | 70 | 600 | 0 |  | 13 | 183 | 45 | 30 | 0 | T3 HP first=-1470 overtime=-420 end=-735 attr=lightning radius=15 |
| 110772 | 210772 | Light Staff | A powerful lightning attack with a staff. | 1/3 | ENEMY(7) | 72 | 300 | 0 |  | 0 | 0 | 22 | 100 | 515 | T1 hit=100% add=+100 hitrate=100(rel) delay=100; T3 HP first=-2500 attr=lightning |
| 110773 | 210773 | Lightning Armor | Inflicts a powerful lightning damage and has a chance to temporarily stun. | 4/0 | SELF(1) | 75 | 250 | 0 | 379061000 Stone of Mage (+0) | 15 | 250 | 0 | 100 | 516 | T4 MAGE_ARMOR(25) dur=120s |
| 110774 | 210774 | Blink | Able to teleport in front of an opponent 20 meters away. | 8/0 | SELF(1) | 80 | 100 | 0 |  | 0 | 213 | 0 | 100 | 517 | T8 target=1 radius=20 warp=20(blink forward Radius) exprecover=0 kick=0 |

#### Master (cat 8) (Skill=1108, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110800 | 210800 | Bright Dew | [Passive]Increase your MP recovery speed by 20% when your MP is down to 30% or lower. | 0/0 | SELF(1) | 0 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 110802 | 210802 | Absolute power | Temporarily increase your magic attack power by 30%. | 4/0 | SELF(1) | 2 | 240 | 0 | 379065000 Absolute Power Scroll (+0) (+consumes class stone 379061000) | 0 | 250 | 56 | 100 | 0 | T4 MAGIC_POWER(10) dur=30s MagicAttack=130 |
| 110805 | 210805 | Absoluteness | [Passive]Decreases all damages received by 10% | 0/0 | SELF(1) | 5 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 110810 | 210810 | Matchless | [Passive]Decreases all damages received by 15% | 0/0 | SELF(1) | 10 | 0 | 0 |  | 0 | 0 | 0 | 100 | 0 | passive (no type row; handled by client/server stat code) |
| 110815 | 210815 | Mana Shield | Absorbs 15% of damage received through mana. However, the damage absorbed from mana will be 4 times greater. | 4/0 | SELF(1) | 12 | 150 | 0 |  | 0 | 0 | 0 | 70 | 0 | T4 MANA_ABSORB(31) dur=40s ExpPct=15 |
| 110820 | 210820 | Instantly Magic | Able to cast a spell without any refresh time. | 4/0 | SELF(1) | 15 | 100 | 0 |  | 0 | 255 | 0 | 100 | 0 | T4 INSTANT_MAGIC(23) dur=180s |
| 110825 | 210825 | Minor Resist | Decrease all enemy's resistance by 20% in a certain area. Does not apply to monsters. | 4/0 | AREA_ENEMY(10) | 20 | 450 | 0 | 379061000 Stone of Mage (+0) | 13 | 1 | 56 | 30 | 0 | T4 DECREASE_RESIST(24) dur=10s radius=10 FireR=20 ColdR=20 LightningR=20 MagicR=20 DiseaseR=20 PoisonR=20 |

#### cat 9 (Skill=1109, SkillLevel = required points in this category)

| KarusID | ElMoID | Name | Description | T1/T2 | Moral | Req | MP | HP | UseItem | Cast | Recast(0.1s) | Range | SR | Etc | Type details |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110036 | 210036 | whipping | Whips the horse to temporarily increase the horse's movement speed. | 4/0 | SELF(1) | 1 | 5 | 0 |  | 0 | 255 | 0 | 100 | 0 | T4 SPEED(6) dur=1200s Speed=200 |
