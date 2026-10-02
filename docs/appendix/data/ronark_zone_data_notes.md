# Ronark Land (zone 71) - DB extract

## ZONE_INFO (all rows)

| ServerNo | ZoneNo | strZoneName (SMD) | InitX | InitZ | InitY | Type | RoomEvent | bz |
|---|---|---|---|---|---|---|---|---|
| 1 | 1 | karus_051221.smd | 35400 | 161000 | 0 | 1 | 1 | Karus |
| 1 | 2 | elmo_051221.smd | 167000 | 37000 | 0 | 1 | 2 | ElMorad |
| 1 | 11 | k_eslant_20050707.smd | 35400 | 161000 | 0 | 2 | 0 | KarusEslant |
| 1 | 12 | e_eslant_20050707.smd | 167000 | 37000 | 0 | 2 | 0 | ElMoradEslant |
| 1 | 21 | moradon_20060124.smd | 31200 | 40200 | 0 | 1 | 21 | Moradon |
| 1 | 30 | siege_0722.smd | 1000 | 1000 | 0 | 2 | 0 | Delos |
| 1 | 31 | dungeon_1216.smd | 1000 | 1000 | 0 | 2 | 0 | Bifrost |
| 1 | 32 | dungeonb_0925.smd | 1000 | 1000 | 0 | 2 | 0 | DesperationAbyss |
| 1 | 33 | dungeonc_1008.smd | 1000 | 1000 | 0 | 2 | 0 | HellAbyss |
| 1 | 34 | dragon_room_20050728.smd | 1000 | 1000 | 0 | 2 | 0 | DragonCave |
| 1 | 48 | BattleField_20050801.smd | 128 | 123 | 0 | 0 | 0 | Colosseum |
| 1 | 51 | clanfight_b.smd | 13900 | 13900 | 0 | 0 | 0 | OrcArena |
| 1 | 52 | clanfight_b.smd | 13900 | 13900 | 0 | 0 | 0 | BloodDonArena |
| 1 | 53 | clanfight_b.smd | 13900 | 13900 | 0 | 0 | 0 | GoblinArena |
| 1 | 54 | clanfight_b.smd | 13900 | 13900 | 0 | 0 | 0 | CaitharosArena |
| 1 | 55 | clanfight_b.smd | 13900 | 13900 | 0 | 0 | 0 | KellinoTemple |
| 1 | 61 | bat_a_20050718.smd | 1000 | 1000 | 0 | 2 | 61 | NapiesGorge |
| 1 | 62 | bat_b_20050718.smd | 1000 | 1000 | 0 | 2 | 62 | AlseidsPrairie |
| 1 | 63 | bat_c_20050718.smd | 1000 | 1000 | 0 | 2 | 63 | NiedsTriangle |
| 1 | 64 | bat_d_051221.smd | 1000 | 1000 | 0 | 2 | 64 | Nereid'sIsland |
| 1 | 69 | bat_b_20050718.smd | 1000 | 1000 | 0 | 0 | 62 | SnowWar |
| 1 | 71 | freezone_a_20050718.smd | 1000 | 1000 | 0 | 2 | 0 | RonarkLand |
| 1 | 72 | freezone_b_20050718.smd | 1000 | 1000 | 0 | 2 | 0 | Ardream |
| 1 | 81 | In_dungeon_20050718.smd | 1000 | 1000 | 0 | 0 | 0 | MonsterSquad1 |
| 1 | 82 | In_dungeon_02_20050722.smd | 1000 | 1000 | 0 | 0 | 0 | MonsterSquad2 |
| 1 | 83 | In_dungeon_03_b_20050805.smd | 1000 | 1000 | 0 | 0 | 0 | MonsterSquad3 |
| 1 | 84 | dungeon.smd | 1000 | 1000 | 0 | 0 | 0 | BorderDefenseWar |
| 1 | 85 | dungeon.smd | 1000 | 1000 | 0 | 0 | 0 | ChaosDungeon |
| 1 | 87 | dungeon.smd | 1000 | 1000 | 0 | 0 | 0 | JuradMountain |
| 1 | 93 | dungeon_c.smd | 1000 | 1000 | 0 | 0 | 0 | IsiloonArena |
| 1 | 94 | dragon.smd | 1000 | 1000 | 0 | 0 | 0 | FelankorArena |

**Ronark Land = ZoneNo 71** (`freezone_a_20050718.smd`, bz "RonarkLand"); `Define.h`: ZONE_RONARK_LAND 71, ZONE_ARDREAM 72 (freezone_b), ZONE_RONARK_LAND_BASE 73 (no ZONE_INFO row -> not loaded on this server). `CUser::isInPKZone()` = zone 71/72/73. MIN_LEVEL_RONARK_LAND 35 (no max). Type=2 (same as war/abyss zones). ZONE_INFO Init* are 1000/1000 (unused; spawn comes from START_POSITION).

## START_POSITION / START_POSITION_RANDOM / HOME (relevant rows)

| ZoneID | KarusX | KarusZ | ElmoradX | ElmoradZ | KarusGateX | KarusGateZ | ElmoGateX | ElmoGateZ | RangeX | RangeZ |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 441 | 1625 | 1859 | 170 | 10 | 10 | 0 | 0 | 0 | 0 |
| 2 | 219 | 1859 | 1595 | 412 | 10 | 10 | 0 | 0 | 0 | 0 |
| 21 | 306 | 352 | 306 | 352 | 10 | 10 | 0 | 0 | 0 | 0 |
| 71 | 1380 | 1090 | 630 | 920 | 10 | 10 | 0 | 0 | 0 | 0 |
| 72 | 848 | 129 | 183 | 898 | 5 | 5 | 0 | 0 | 0 | 0 |

Usage (User.cpp GetStartPosition / AttackHandler.cpp Regene / CUser::Home): /town and death-respawn in zone 71 put Karus at exactly (1380, 1090) and El Morad at exactly (630, 920) (start + rand(0, bRange), positive only; the live row has bRangeX = bRangeZ = 0, the "10 | 10" in the table above are sKarusGateX/Z, not the range; verified by F1-08 `tools/arena-report.py` 2026-10-02), unless the player is bound to a live bind object (m_sBind). /town needs HP >= 50% of max, not dead, not Kaul, not frozen (BUFF_TYPE_FREEZE). Coordinates are in meters (Warp() multiplies by 10).
START_POSITION_RANDOM: 0 rows for zones 71-73 (table only has zone 85 Chaos Dungeon rows).

HOME (2 rows, one per nation; **not loaded by this GameServer** - no HomeSet/m_HomeArray in the repo, kept for reference):

| Nation | ElmoZone X,Z (LX,LZ) | KarusZone X,Z | FreeZone X,Z | BattleZone X,Z | BattleZone2 X,Z |
|---|---|---|---|---|---|
| 1 | 219,1859 (15,15) | 441,1625 (10,10) | 818,128 (10,10) | 820,98 (5,5) | 48,155 (5,5) |
| 2 | 1595,412 (15,15) | 1859,170 (10,10) | 160,910 (10,10) | 113,771 (5,5) | 974,869 (5,5) |

## K_OBJECTPOS / K_OBJECTEVENT (zones 71-73)

| ZoneID | Belong | sIndex | Type | ControlNpcID | Status | PosX | PosY | PosZ | byLife | meaning |
|---|---|---|---|---|---|---|---|---|---|---|
| 71 | 2 | 4019 | 5 | 712 | 1 | 622.0 | 11.6 | 911.0 | 1 | OBJECT_WARP_GATE; Belong 2=El Morad; warp group = ControlNpcID -> SMD warp ids 7120..7129 |
| 71 | 1 | 4020 | 5 | 711 | 1 | 1375.0 | 11.8 | 1085.0 | 1 | OBJECT_WARP_GATE; Belong 1=Karus; warp group = ControlNpcID -> SMD warp ids 7110..7119 |
| 72 | 2 | 4019 | 5 | 722 | 1 | 203.8 | 73.3 | 907.5 | 1 | OBJECT_WARP_GATE; Belong 2=El Morad; warp group = ControlNpcID -> SMD warp ids 7220..7229 |
| 72 | 1 | 4020 | 5 | 721 | 1 | 833.5 | 75.4 | 120.6 | 1 | OBJECT_WARP_GATE; Belong 1=Karus; warp group = ControlNpcID -> SMD warp ids 7210..7219 |

ObjectType enum = shared/packets.h. OBJECT_WARP_GATE (5): `CUser::WarpListObjectEvent` opens the warp list only for the owning nation (sBelong) and lists SMD warps whose sWarpID/10 == ControlNpcID (711 Karus gate, 712 El Morad gate). The warp coordinates themselves live in the map file Map/freezone_a_20050718.smd (warp list after the collision data), not in the DB.

K_OBJECTEVENT rows (no loader for this table exists in the repo; GameServer/AIServer read objects from K_OBJECTPOS):

| sZoneNo | sIndex | byLife | sBelong | sType | sControlNpcId | sStatus | sPosX | sPosY | sPosZ |
|---|---|---|---|---|---|---|---|---|---|
| 71 | 1200 | 1 | 2 | 1 | 0 | 1 | 496 | 79 | 518 |
| 71 | 1200 | 1 | 2 | 1 | 0 | 1 | 527 | 79 | 512 |
| 71 | 1301 | 1 | 0 | 1 | 0 | 1 | 521 | 82 | 562 |

## K_WARPINFO (rows naming Ronark Land)

K_WARPINFO has only (sWarpID, strWarpName, strAnnounce) and is **not loaded by any server code** (warp data comes from SMD `_WARP_INFO`, which has the same first three fields plus dwPay, sZone, fX/fY/fZ, fR, sNation). sWarpID/10 = warp group (= ControlNpcID of the gate object or NPC). 89 rows total; rows that lead to Ronark Land:

| sWarpID | group | strWarpName | strAnnounce |
|---|---|---|---|
| 113 | 11 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 123 | 12 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 133 | 13 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 143 | 14 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 153 | 15 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 163 | 16 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 173 | 17 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 213 | 21 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 223 | 22 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 233 | 23 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 243 | 24 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 253 | 25 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 263 | 26 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 273 | 27 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 35 are allowed entry. Transport fee is 17,000 Noahs. |
| 2117 | 211 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 60 are allowed entry. Transport fee 17,000 Noahs |
| 2127 | 212 | Ronark Land | You will be transported to Ronark Land where players can hunt or wage battle against players of opposing countries. Only players above Level 60 are allowed entry. Transport fee 17,000 Noahs |

## K_NPCPOS for zone 71 joined to K_NPC / K_MONSTER

Loader (AIServer/ServerDlg.cpp LoadSpawnCallback): ActType < 100 -> K_MONSTER row sSid=NpcID; ActType >= 100 -> K_NPC row (move type = ActType-100). NumNPC copies are spawned at random points inside the rectangle LeftX..RightX / TopZ..BottomZ (meters); RegTime = respawn delay (seconds); rows with TrapNumber<>0 are spawned only if TrapNumber equals a per-boot random 1..4. byGroup = nation (1 Karus, 2 El Morad, 0 none). byType = NpcType (shared/globals.h).

Summary: 296 K_NPCPOS rows, 818 spawned units (113 NPC-table units, 705 monster units).

| Class | Nation | rows | units |
|---|---|---|---|
| gate [NPC_GATE2] | 3(?) | 1 | 1 |
| guard tower [NPC_GUARD_TOWER1] | El Morad | 22 | 22 |
| guard tower [NPC_GUARD_TOWER1] | Karus | 22 | 22 |
| monster | none/neutral | 212 | 687 |
| monster (boss) | none/neutral | 10 | 18 |
| monument [NPC_BIFROST_MONUMENT] | none/neutral | 1 | 1 |
| nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 12 | 34 |
| nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 10 | 27 |
| outpost captain NPC [type101] | El Morad | 1 | 1 |
| outpost captain NPC [type102] | Karus | 1 | 1 |
| service npc (merchant/healer/etc.) [NPC_TINKER] | El Morad | 1 | 1 |
| service npc (merchant/healer/etc.) [NPC_TINKER] | Karus | 1 | 1 |
| service npc (merchant/healer/etc.) [NPC_WAREHOUSE] | El Morad | 1 | 1 |
| service npc (merchant/healer/etc.) [NPC_WAREHOUSE] | Karus | 1 | 1 |

### NPC-table entities (guards, towers, gates, monuments, services)

| NpcID | Name | class | nation | lvl | HP | AC | dmg | atkDelay | atkRange | searchRange | magic1/2/3 | X range | Z range | center | Num | RegTime | ActType | Dir |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 602 | Bifrost Gate | gate [NPC_GATE2] | 3(?) | 80 | 10000 | 480 | 0 | 1000 | 5 | 5 | 0/0/0 | 1027..1028 | 1019..1018 | (1028,1018) | 1 | 60 | 100 | 106 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 666..668 | 886..884 | (667,885) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 597..599 | 914..912 | (598,913) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 665..666 | 930..928 | (666,929) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 594..596 | 870..868 | (595,869) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 655..657 | 949..947 | (656,948) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 573..574 | 885..884 | (574,884) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 634..636 | 954..952 | (635,953) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 651..653 | 873..872 | (652,872) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 613..615 | 864..862 | (614,863) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 633..635 | 865..863 | (634,864) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 601..603 | 897..895 | (602,896) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 613..616 | 956..954 | (614,955) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 577..579 | 936..934 | (578,935) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 669..671 | 907..905 | (670,906) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 574..576 | 911..908 | (575,910) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 646..648 | 924..922 | (647,923) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 649..651 | 910..908 | (650,909) | 1 | 60 | 104 | 0 |
| 5300 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 593..595 | 949..947 | (594,948) | 1 | 60 | 104 | 0 |
| 5310 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 120 | 100000 | 15000 | 0 | 1000 | 20 | 35 | 300199/0/0 | 631..632 | 911..910 | (632,910) | 1 | 60 | 104 | 46 |
| 5310 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 120 | 100000 | 15000 | 0 | 1000 | 20 | 35 | 300199/0/0 | 611..613 | 911..909 | (612,910) | 1 | 60 | 104 | 135 |
| 5310 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 120 | 100000 | 15000 | 0 | 1000 | 20 | 35 | 300199/0/0 | 621..622 | 921..920 | (622,920) | 1 | 60 | 104 | 180 |
| 5310 | Guard tower | guard tower [NPC_GUARD_TOWER1] | El Morad | 120 | 100000 | 15000 | 0 | 1000 | 20 | 35 | 300199/0/0 | 621..622 | 902..900 | (622,901) | 1 | 60 | 104 | 89 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1334..1336 | 1062..1060 | (1335,1061) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1349..1351 | 1087..1085 | (1350,1086) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1354..1356 | 1072..1071 | (1355,1072) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1398..1400 | 1101..1099 | (1399,1100) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1403..1404 | 1086..1085 | (1404,1086) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1343..1345 | 1125..1124 | (1344,1124) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1405..1407 | 1130..1128 | (1406,1129) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1422..1423 | 1088..1086 | (1422,1087) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1414..1416 | 1111..1109 | (1415,1110) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1328..1330 | 1085..1083 | (1329,1084) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1328..1329 | 1110..1108 | (1328,1109) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1361..1363 | 1134..1133 | (1362,1134) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1383..1385 | 1136..1134 | (1384,1135) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1409..1411 | 1050..1049 | (1410,1050) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1366..1368 | 1038..1037 | (1367,1038) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1389..1390 | 1039..1037 | (1390,1038) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1423..1424 | 1066..1065 | (1424,1066) | 1 | 60 | 104 | 0 |
| 5400 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 90 | 100000 | 15000 | 0 | 1000 | 30 | 35 | 300139/0/0 | 1345..1347 | 1044..1042 | (1346,1043) | 1 | 60 | 104 | 0 |
| 5410 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 120 | 100000 | 15000 | 0 | 1000 | 20 | 35 | 300199/0/0 | 1374..1375 | 1077..1076 | (1374,1076) | 1 | 60 | 104 | 90 |
| 5410 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 120 | 100000 | 15000 | 0 | 1000 | 20 | 35 | 300199/0/0 | 1367..1368 | 1085..1084 | (1368,1084) | 1 | 60 | 104 | 136 |
| 5410 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 120 | 100000 | 15000 | 0 | 1000 | 20 | 35 | 300199/0/0 | 1374..1376 | 1093..1091 | (1375,1092) | 1 | 60 | 104 | 180 |
| 5410 | Guard tower | guard tower [NPC_GUARD_TOWER1] | Karus | 120 | 100000 | 15000 | 0 | 1000 | 20 | 35 | 300199/0/0 | 1382..1383 | 1085..1084 | (1382,1084) | 1 | 60 | 104 | 44 |
| 601 | Bifrost Monument | monument [NPC_BIFROST_MONUMENT] | none/neutral | 100 | 700000 | 15000 | 0 | 1000 | 5 | 5 | 0/0/0 | 1013..1015 | 993..991 | (1014,992) | 1 | 600 | 100 | 180 |
| 14001 | Elmorad Warrior | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 45 | 3415 | 324 | 241 | 1500 | 7 | 14 | 0/0/0 | 867..880 | 668..649 | (874,658) | 2 | 40 | 102 | 0 |
| 14001 | Elmorad Warrior | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 45 | 3415 | 324 | 241 | 1500 | 7 | 14 | 0/0/0 | 631..659 | 798..773 | (645,786) | 2 | 40 | 102 | 0 |
| 14004 | Elmorad Commander | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 48 | 3996 | 345 | 284 | 1500 | 7 | 14 | 0/0/0 | 1269..1294 | 1492..1467 | (1282,1480) | 5 | 40 | 101 | 0 |
| 14004 | Elmorad Commander | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 48 | 3996 | 345 | 284 | 1500 | 7 | 14 | 0/0/0 | 941..953 | 683..672 | (947,678) | 2 | 40 | 102 | 0 |
| 14004 | Elmorad Commander | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 48 | 3996 | 345 | 284 | 1500 | 7 | 14 | 0/0/0 | 554..584 | 1040..1014 | (569,1027) | 2 | 40 | 102 | 0 |
| 14004 | Elmorad Commander | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 48 | 3996 | 345 | 284 | 1500 | 7 | 14 | 0/0/0 | 772..775 | 615..612 | (774,614) | 1 | 150 | 102 | 23 |
| 14004 | Elmorad Commander | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 48 | 3996 | 345 | 284 | 1500 | 7 | 14 | 0/0/0 | 783..785 | 603..600 | (784,602) | 1 | 150 | 102 | 25 |
| 14005 | Elmorad Assassin | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 50 | 4417 | 360 | 316 | 1500 | 7 | 14 | 0/0/0 | 1299..1328 | 1578..1563 | (1314,1570) | 3 | 40 | 101 | 0 |
| 14006 | Elmorad Vice Captain | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 55 | 5596 | 396 | 405 | 1500 | 7 | 14 | 0/0/0 | 1501..1533 | 1336..1308 | (1517,1322) | 4 | 40 | 101 | 0 |
| 14006 | Elmorad Vice Captain | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 55 | 5596 | 396 | 405 | 1500 | 7 | 14 | 0/0/0 | 1314..1340 | 1523..1498 | (1327,1510) | 5 | 40 | 101 | 0 |
| 14007 | Elmorad Commanding Officer | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 60 | 6969 | 432 | 522 | 1500 | 7 | 14 | 0/0/0 | 1444..1474 | 1343..1318 | (1459,1330) | 3 | 40 | 101 | 0 |
| 14007 | Elmorad Commanding Officer | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | El Morad | 60 | 6969 | 432 | 522 | 1500 | 7 | 14 | 0/0/0 | 1359..1386 | 1570..1542 | (1372,1556) | 4 | 40 | 101 | 0 |
| 24001 | Karus Warrior | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 45 | 3415 | 324 | 241 | 1500 | 7 | 14 | 0/0/0 | 1168..1183 | 1361..1343 | (1176,1352) | 2 | 40 | 102 | 0 |
| 24001 | Karus Warrior | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 45 | 3415 | 324 | 241 | 1500 | 7 | 14 | 0/0/0 | 1359..1380 | 1201..1181 | (1370,1191) | 2 | 40 | 102 | 0 |
| 24004 | Karus Commander | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 48 | 3996 | 345 | 284 | 1500 | 7 | 14 | 0/0/0 | 753..778 | 535..510 | (766,522) | 5 | 40 | 101 | 0 |
| 24004 | Karus Commander | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 48 | 3996 | 345 | 284 | 1500 | 7 | 14 | 0/0/0 | 1101..1114 | 1342..1329 | (1108,1336) | 2 | 40 | 102 | 0 |
| 24004 | Karus Commander | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 48 | 3996 | 345 | 284 | 1500 | 7 | 14 | 0/0/0 | 1420..1451 | 981..950 | (1436,966) | 2 | 40 | 102 | 0 |
| 24004 | Karus Commander | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 48 | 3996 | 345 | 284 | 1500 | 7 | 14 | 0/0/0 | 1253..1254 | 1398..1397 | (1254,1398) | 1 | 150 | 102 | 118 |
| 24004 | Karus Commander | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 48 | 3996 | 345 | 284 | 1500 | 7 | 14 | 0/0/0 | 1265..1266 | 1382..1381 | (1266,1382) | 1 | 150 | 102 | 116 |
| 24005 | Karus Assassin | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 50 | 4417 | 360 | 316 | 1500 | 7 | 14 | 0/0/0 | 748..777 | 453..436 | (762,444) | 3 | 40 | 101 | 0 |
| 24006 | Karus Vice Captain | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 55 | 5596 | 396 | 405 | 1500 | 7 | 14 | 0/0/0 | 709..734 | 493..468 | (722,480) | 5 | 40 | 101 | 0 |
| 24007 | Karus Commanding Officer | nation soldier NPC (K_NPC byType 0, loader turns it into NPC_GENERAL) [NPC_MONSTER] | Karus | 60 | 6969 | 432 | 522 | 1500 | 7 | 14 | 0/0/0 | 667..693 | 438..419 | (680,428) | 4 | 40 | 101 | 0 |
| 14426 | [Outpost Captain]Della | outpost captain NPC [type101] | El Morad | 50 | 150000 | 7500 | 3000 | 1500 | 7 | 14 | 0/0/0 | 609..610 | 894..893 | (610,894) | 1 | 3600 | 104 | 14 |
| 24426 | [Outpost Captain]Elrod | outpost captain NPC [type102] | Karus | 50 | 150000 | 7500 | 3000 | 1500 | 7 | 14 | 0/0/0 | 1385..1386 | 1101..1100 | (1386,1100) | 1 | 3600 | 104 | 107 |
| 16062 | [sundries]Halber | service npc (merchant/healer/etc.) [NPC_TINKER] | El Morad | 50 | 30000 | 5000 | 1000 | 1500 | 7 | 14 | 0/0/0 | 629..630 | 926..925 | (630,926) | 1 | 60 | 104 | 105 |
| 26062 | Ardin[sundries] | service npc (merchant/healer/etc.) [NPC_TINKER] | Karus | 50 | 30000 | 5000 | 1000 | 1500 | 7 | 14 | 0/0/0 | 1388..1389 | 1075..1073 | (1388,1074) | 1 | 60 | 104 | 155 |
| 16061 | Inn hostess | service npc (merchant/healer/etc.) [NPC_WAREHOUSE] | El Morad | 50 | 50000 | 5000 | 1000 | 1500 | 7 | 14 | 0/0/0 | 635..636 | 897..895 | (636,896) | 1 | 60 | 104 | 164 |
| 26061 | Inn hostess | service npc (merchant/healer/etc.) [NPC_WAREHOUSE] | Karus | 50 | 50000 | 5000 | 1000 | 1500 | 7 | 14 | 0/0/0 | 1359..1362 | 1100..1097 | (1360,1098) | 1 | 60 | 104 | 69 |

### Monsters (grouped by monster id)

| NpcID | Name | lvl | HP | AC | atk/dmg | exp | NP(loyalty) | group | type | rows | units | RegTime (s) | spawn rectangles (LeftX..RightX x TopZ..BottomZ) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 504 | Dark eyes | 31 | 1193 | 210 | 0/89 | 600000000 | 0 | 0 | 0 | 3 | 12 | 8000 | 1058..1104 x 968..921; 1055..1105 x 1072..1036; 918..965 x 969..924 |
| 1004 | undying | 33 | 1202 | 336 | 0/104 | 600000000 | 0 | 0 | 0 | 5 | 30 | 8000 | 1038..1090 x 957..905; 925..986 x 957..900; 933..988 x 1069..1025; 996..1035 x 999..986; 1042..1090 x 1082..1028 |
| 408 | Mastodon | 44 | 4044 | 633 | 0/380 | 600000000 | 0 | 0 | 0 | 4 | 16 | 40 | 204..264 x 1609..1580; 1766..1817 x 377..348; 306..354 x 1696..1655; 1667..1705 x 343..304 |
| 506 | Lobo | 45 | 17790 | 972 | 0/483 | 600000000 | 0 | 0 | 0 | 4 | 4 | 16000 | 996..1034 x 941..855; 1057..1135 x 1011..971; 896..972 x 1015..966; 1001..1033 x 1142..1041 |
| 1104 | Death knight | 45 | 2846 | 324 | 0/241 | 600000000 | 0 | 0 | 0 | 2 | 20 | 8000 | 959..1067 x 1085..1044; 961..1061 x 938..898 |
| 507 | Lupus | 50 | 23010 | 1080 | 0/650 | 600000000 | 0 | 0 | 0 | 4 | 4 | 16000 | 1058..1126 x 1034..950; 903..971 x 1032..941; 946..1083 x 927..889; 944..1087 x 1106..1066 |
| 2801 | Orc bandit Warrior | 50 | 2301 | 360 | 1/572 | 72133 | 0 | 0 | 0 | 4 | 8 | 40 | 1922..1938 x 1388..1374; 1887..1900 x 1373..1360; 168..181 x 636..624; 134..149 x 674..661 |
| 2804 | Orc bandit archer | 50 | 1380 | 360 | 1/520 | 72133 | 0 | 0 | 0 | 4 | 8 | 40 | 1912..1929 x 1312..1300; 1852..1866 x 1362..1347; 125..140 x 624..613; 175..187 x 637..626 |
| 2807 | Orc bandit Sorcerer | 50 | 920 | 360 | 1/520 | 600000000 | 0 | 0 | 0 | 4 | 8 | 40 | 1846..1858 x 1381..1368; 1891..1906 x 1321..1309; 155..170 x 615..605; 102..115 x 653..638 |
| 613 | Blood Don | 52 | 15211 | 299 | 0/380 | 600000000 | 0 | 0 | 0 | 6 | 6 | 3600 | 1099..1125 x 1017..975; 912..949 x 1020..960; 962..1008 x 1089..1043; 1022..1071 x 1089..1041; 1021..1061 x 939..899; 973..1007 x 939..900 |
| 1203 | Baron | 52 | 4056 | 374 | 0/359 | 600000000 | 0 | 0 | 0 | 2 | 10 | 8000 | 1059..1096 x 1064..930; 930..971 x 1060..930 |
| 1204 | Cardinal | 54 | 4455 | 388 | 0/396 | 600000000 | 0 | 0 | 0 | 2 | 10 | 8000 | 1026..1040 x 1011..972; 990..1002 x 1009..972 |
| 508 | Lycaon | 55 | 29155 | 1188 | 0/832 | 600000000 | 0 | 0 | 0 | 6 | 6 | 16000 | 924..1001 x 1077..1035; 1028..1097 x 948..908; 1089..1129 x 1018..964; 908..950 x 1019..957; 918..1000 x 948..905; 1031..1108 x 1077..1033 |
| 607 | Barrkk | 55 | 29155 | 792 | 0/998 | 600000000 | 0 | 0 | 0 | 4 | 4 | 16000 | 968..1064 x 941..887; 1071..1106 x 1054..932; 923..957 x 1046..917; 973..1064 x 1094..1045 |
| 1205 | Duke | 55 | 29155 | 594 | 0/832 | 600000000 | 0 | 0 | 0 | 5 | 5 | 16000 | 971..1067 x 931..869; 1066..1107 x 1069..914; 927..966 x 1067..905; 996..1038 x 1004..980; 969..1064 x 1117..1046 |
| 1211 | Cardinal | 55 | 4664 | 396 | 0/416 | 600000000 | 0 | 0 | 0 | 4 | 20 | 40 | 124..154 x 907..882; 173..203 x 858..833; 1871..1901 x 1154..1129; 1900..1930 x 1107..1082 |
| 1304 | Harunga | 55 | 29155 | 792 | 0/1109 | 600000000 | 0 | 0 | 0 | 2 | 10 | 8000 | 1067..1112 x 1045..929; 923..968 x 1062..924 |
| 608 | Barkirra | 60 | 36300 | 864 | 0/1254 | 600000000 | 0 | 0 | 0 | 4 | 4 | 16000 | 967..1065 x 914..872; 1085..1114 x 1054..930; 919..947 x 1052..914; 975..1069 x 1122..1080 |
| 1106 | bone collecter | 60 | 36300 | 864 | 0/1567 | 600000000 | 0 | 0 | 0 | 4 | 4 | 16000 | 1097..1128 x 975..941; 909..948 x 971..923; 912..955 x 1053..1013; 1100..1129 x 1054..1012 |
| 1206 | Bishop | 60 | 36300 | 648 | 0/1045 | 600000000 | 0 | 0 | 0 | 5 | 5 | 16000 | 986..1049 x 910..878; 983..1054 x 1132..1071; 1058..1136 x 1002..980; 894..973 x 1003..977; 993..1037 x 1000..985 |
| 2802 | Orc bandit Warrior | 60 | 3630 | 432 | 1/919 | 117096 | 0 | 0 | 0 | 2 | 4 | 60 | 1920..1933 x 1362..1350; 116..131 x 646..631 |
| 2805 | Orc bandit archer | 60 | 2178 | 432 | 1/836 | 117096 | 0 | 0 | 0 | 2 | 4 | 60 | 1885..1900 x 1399..1385; 120..133 x 673..662 |
| 2808 | Orc bandit Sorcerer | 60 | 1452 | 432 | 1/1003 | 600000000 | 0 | 0 | 0 | 2 | 4 | 60 | 1927..1944 x 1328..1314; 149..163 x 666..651 |
| 1207 | Bach | 65 | 44520 | 702 | 0/1291 | 600000000 | 0 | 0 | 0 | 6 | 6 | 16000 | 991..1002 x 1009..973; 1009..1092 x 942..903; 1059..1103 x 1076..1004; 935..1009 x 1082..1039; 928..970 x 978..907; 1028..1040 x 1009..974 |
| 1305 | Javana | 65 | 44520 | 936 | 0/1722 | 600000000 | 0 | 0 | 0 | 6 | 6 | 16000 | 1060..1103 x 993..922; 1060..1107 x 1058..995; 970..1064 x 1117..1082; 931..968 x 991..915; 932..971 x 1076..992; 959..1064 x 919..879 |
| 8011 | Dragon Tooth commander | 68 | 12668 | 601 | 3/805 | 191539 | 0 | 0 | 0 | 6 | 30 | 45 | 1657..1723 x 1666..1638; 1699..1735 x 1623..1577; 1646..1683 x 1623..1594; 310..358 x 408..377; 369..400 x 364..325; 314..341 x 356..304 |
| 1311 | Riote | 70 | 43112 | 756 | 0/1000 | 600000000 | 25 | 0 | 0 | 8 | 8 | 3600 | 1047..1089 x 955..912; 935..986 x 955..910; 936..987 x 1068..1025; 1047..1095 x 1071..1026; 982..1052 x 1111..1053; 980..1055 x 927..881 ... |
| 1741 | Troll | 70 | 21556 | 315 | 0/2098 | 600000000 | 0 | 0 | 0 | 4 | 20 | 40 | 1923..1953 x 1604..1579; 91..121 x 369..354; 124..143 x 420..392; 1874..1904 x 1633..1608 |
| 2122 | Apostle | 70 | 21556 | 315 | 1/1574 | 296887 | 0 | 0 | 0 | 4 | 20 | 45 | 523..554 x 415..382; 460..491 x 483..454; 1538..1572 x 1584..1545; 1460..1497 x 1601..1563 |
| 2221 | Harpy | 70 | 16167 | 504 | 1/1180 | 600000000 | 0 | 0 | 0 | 12 | 60 | 45 | 1510..1534 x 707..684; 1445..1471 x 702..675; 1429..1456 x 653..626; 1530..1556 x 653..629; 1454..1480 x 602..574; 1498..1525 x 617..591 ... |
| 908 | Lesath | 75 | 64485 | 1080 | 0/1263 | 600000000 | 0 | 0 | 0 | 4 | 4 | 16000 | 923..988 x 952..892; 933..983 x 1078..1026; 1044..1097 x 954..897; 1044..1102 x 1081..1029 |
| 914 | Shaula | 75 | 64485 | 1080 | 0/1263 | 600000000 | 0 | 0 | 0 | 4 | 4 | 16000 | 906..974 x 1049..924; 952..1087 x 1094..1038; 948..1073 x 946..879; 1060..1112 x 1057..930 |
| 8012 | Doom Soldier | 76 | 55014 | 1039 | 3/1545 | 511511 | 0 | 0 | 0 | 4 | 20 | 30/35 | 371..415 x 1709..1677; 1616..1652 x 329..302; 251..293 x 1659..1622; 1717..1754 x 355..319 |
| 1742 | Troll Warrior | 80 | 38190 | 360 | 0/3760 | 600000000 | 0 | 0 | 0 | 4 | 20 | 40 | 178..205 x 333..315; 1906..1936 x 1695..1670; 118..141 x 282..263; 1857..1887 x 1752..1727 |
| 2222 | Raven Harpy | 80 | 27496 | 648 | 1/1918 | 600000000 | 0 | 0 | 0 | 15 | 75 | 45 | 1247..1274 x 271..247; 1183..1208 x 277..257; 1233..1269 x 371..338; 779..803 x 1741..1716; 903..936 x 1399..1367; 1170..1202 x 373..343 ... |
| 8013 | Dark Knight | 82 | 88014 | 535 | 3/2421 | 773955 | 0 | 0 | 0 | 6 | 30 | 50 | 446..488 x 1070..1028; 1516..1554 x 1065..1027; 1537..1579 x 989..955; 426..468 x 999..958; 1688..1728 x 906..870; 268..308 x 1164..1122 |
| 8014 | Apostle of Piercing Cold | 84 | 51067 | 1412 | 1/3152 | 872719 | 0 | 0 | 0 | 4 | 20 | 50 | 1764..1806 x 1109..1069; 293..328 x 946..910; 1797..1836 x 1169..1124; 212..252 x 914..879 |
| 1402 | Atross | 85 | 89650 | 1224 | 0/1774 | 600000000 | 50 | 0 | 3 | 8 | 8 | 3600 | 994..1032 x 904..873; 994..1037 x 1121..1076; 956..1078 x 942..910; 952..1077 x 1071..1038; 1062..1115 x 1050..936; 923..972 x 1052..932 ... |
| 8004 | Troll Shaman | 85 | 41556 | 1104 | 0/2924 | 600000000 | 0 | 0 | 0 | 2 | 10 | 45 | 1874..1906 x 1580..1553; 162..184 x 437..410 |
| 8016 | Apostle of Flames | 88 | 55867 | 1568 | 1/3524 | 927319 | 0 | 0 | 0 | 4 | 20 | 50 | 1096..1133 x 1284..1246; 866..905 x 768..739; 1200..1240 x 1250..1210; 748..779 x 816..780 |
| 1315 | Samma | 90 | 104370 | 1296 | 0/4149 | 600000000 | 0 | 0 | 0 | 5 | 5 | 16000 | 1008..1021 x 997..986; 968..1072 x 1099..1040; 1063..1111 x 1073..921; 919..970 x 1070..916; 967..1060 x 941..884 |
| 2421 | Stone golem | 100 | 11000 | 720 | 1/3119 | 600000000 | 0 | 0 | 0 | 4 | 20 | 40 | 1561..1592 x 297..279; 1611..1640 x 378..354; 442..481 x 1708..1677; 393..427 x 1656..1638 |
| 2813 | Orc bandit officer | 100 | 55384 | 720 | 1/3327 | 444368 | 0 | 0 | 0 | 2 | 2 | 8000 | 118..168 x 654..618; 1884..1924 x 1383..1344 |
| 2814 | Orc bandit officer | 100 | 27692 | 720 | 1/3327 | 444368 | 0 | 0 | 0 | 2 | 2 | 8000 | 113..160 x 667..621; 1871..1938 x 1383..1324 |
| 2815 | Orc bandit officer | 100 | 27692 | 720 | 1/3327 | 444368 | 0 | 0 | 0 | 2 | 2 | 8000 | 113..165 x 664..613; 1873..1945 x 1382..1326 |
| 2422 | Giant golem | 110 | 14000 | 990 | 1/3468 | 600000000 | 0 | 0 | 0 | 4 | 16 | 40 | 180..210 x 1497..1472; 187..217 x 1557..1532; 1836..1866 x 489..464; 1810..1840 x 418..393 |
| 5801 | Ego | 110 | 35005 | 2015 | 1/3835 | 600000000 | 0 | 0 | 9 | 3 | 20 | 30 | 1673..1705 x 1279..1243; 1665..1689 x 1259..1235; 347..391 x 789..744 |
| 2817 | Orc bandit leader | 120 | 227340 | 864 | 1/6218 | 696457 | 0 | 0 | 0 | 1 | 1 | 16000 | 1064..1119 x 1014..930 |
| 2818 | Orc bandit leader | 120 | 227340 | 864 | 1/6218 | 696457 | 0 | 0 | 0 | 1 | 1 | 16000 | 918..968 x 1075..991 |
| 5901 | Glutton | 130 | 61775 | 2234 | 1/5253 | 600000000 | 0 | 0 | 0 | 3 | 20 | 30 | 1637..1661 x 1300..1271; 1625..1645 x 1279..1259; 371..407 x 748..712 |
| 6001 | Wrath | 145 | 81009 | 2525 | 1/7055 | 600000000 | 0 | 0 | 0 | 2 | 20 | 30 | 1537..1573 x 1428..1396; 427..483 x 692..628 |
| 6101 | Sloth | 160 | 82203 | 3456 | 1/13047 | 600000000 | 0 | 0 | 3 | 2 | 10 | 30 | 1445..1481 x 1396..1360; 543..607 x 676..624 |
| 6201 | Lust | 175 | 120533 | 4055 | 1/16077 | 600000000 | 0 | 0 | 0 | 2 | 5 | 30 | 1421..1441 x 1453..1420; 503..547 x 628..595 |
| 6301 | Envy | 190 | 160355 | 5077 | 1/19035 | 600000000 | 0 | 0 | 0 | 2 | 10 | 30 | 1457..1481 x 1440..1408; 579..619 x 620..583 |
| 6401 | Greed | 210 | 217755 | 6073 | 1/22003 | 600000000 | 0 | 0 | 0 | 2 | 4 | 30 | 1497..1517 x 1461..1440; 523..551 x 575..551 |
