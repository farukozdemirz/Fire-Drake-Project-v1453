# C: Map / Movement / Zone rules / Pathfinding / Party / Chat / Targeting — research report

Repo: Fire-Drake-Project-v1453 @ 0f520272 (read-only). Runtime data: /mnt/c/dev/fdp/server/Map. All paths are relative to the repo root unless they are absolute.
Tags: **(a)** verified in code · **(b)** verified from data files / DB · **(c)** inferred · **(d)** unknown, needs a runtime test.

> Tooling caveat: the shell's `grep` wrapper skips files that contain EUC-KR bytes because it treats them as binary (for example AIServer/MAP.h). Every negative search below ("no LoS", "no GetHeight", and so on) was re-run with `/usr/bin/grep -a`.

Scripts and outputs (SCRATCH = <çalışma-dizini>/research/map/):
- `smd_parse.py`: stdlib parser that follows SMDFile.cpp exactly and writes PNGs. Its output is in `smd_parse_output.txt`.
- `smd_analyze.py`: connectivity analysis, plus overlap between the collision geometry and the event grid. Outputs are `smd_analyze_71.txt`, `smd_analyze_72.txt` and `orientation_check.txt`.
- PNGs: `zone71_freezone_a_{eventgrid,height,collision,components4}.png` and `zone72_freezone_b_{...}.png`. Image layout: x runs to the right, z runs up, 1 px = 1 tile = 4 m.

---
## 1. SMD format and query API

### 1.1 Read order as parsed (a)
`SMDFile::LoadMap` (shared/SMDFile.cpp:75-102) reads the sections in this order:
1. **LoadTerrain** (104-113): `int32 m_nMapSize`, `float m_fUnitDist`, then `float height[m_nMapSize²]`.
2. **CN3ShapeMgr::LoadCollisionData** (N3BASE/N3ShapeMgr.cpp:52-114):
   - `float fMapWidth`, `float fMapLength`, `int32 nCollisionFaceCount`, then `__Vector3[faces*3]` (12 B each, My_3DStruct.h:13-17).
   - Main cells of 16 m (CELL_MAIN_SIZE = 4×4, N3ShapeMgr.h:6-10). The loop is z-outer / x-inner. Each cell has a `uint32 bExist`. If it is set, `__CellMain::Load` (h:52-78) reads `int32 nShapeCount`, `uint16 shapeIdx[n]`, then 4×4 sub-cells of 4 m. Each sub-cell is `int32 nCCPolyCount` + `uint32 vertIdx[n*3]` (h:20-40).
   - After the load, LoadMap rejects the map unless `(m_nMapSize-1)*m_fUnitDist == Width() == Height()` (SMDFile.cpp:81-85). Note that `Height()` returns m_fMapWidth (N3ShapeMgr.h:108).
   - Maximum map size: 256 main cells × 16 m = 4096 m (h:9, cpp:118-119).
3. Regions are computed next: `m_nXRegion = m_nZRegion = width/VIEW_DISTANCE + 1` (SMDFile.cpp:87-90), with VIEW_DISTANCE = 48 (shared/globals.h:19).
4. **LoadObjectEvent** (115-127): `int32 count`, then `count × 24 B`. Each record is read into a `new _OBJECT_EVENT` that is leaked: it is never stored, and m_ObjectEventArray stays empty. The server uses DB K_OBJECTPOS instead (ObjectPosSet.h; C3DMap::GetObjectEvent, Map.cpp:174-184, which searches g_pMain->m_ObjectEventArray).
5. **LoadMapTile** (129-134): `int16 event[m_nMapSize²]`, indexed `x*m_nMapSize + z` (GetEventID, 200-206; out of range returns -1).
6. Only when `bLoadWarpsAndRegeneEvents` is set (GameServer passes true at Map.cpp:27; AIServer passes nothing, so false, at AIServer/MAP.cpp:55):
   - **LoadRegeneEvent** (136-154): `int32 count`, then 20 B each (5 floats: PosX, PosY, PosZ, AreaZ, AreaX; `#pragma pack(1)` in structs.h:211-220). The key is the index i.
   - **LoadWarpList** (156-180): `int32 count`, then 320 B each (`_WARP_INFO`, structs.h:222-239: id, name[32], announce[256], pad, pay, zone, pad, fX, fY, fZ, fR, nation, pad). Entries with id 0 are dropped. An EOF part-way through is tolerated.

### 1.2 Query API
- `IsValidPosition(x,z,y)` (SMDFile.cpp:194-198) only returns `x < Width() && z < Height()`, and there is a `// TODO`. It does not check the lower bound, the height or walkability (a).
- `GetEventID(x,z)` (200-206) is a raw tile lookup. Other getters: `GetMapSize()` returns m_nMapSize-1, plus `GetUnitDistance`, `GetX/ZRegionMax` and `GetEventIDs` (h:32-37) (a).
- No GetHeight exists. `m_fHeight` is loaded but never read anywhere. `MAP::GetHeight` is declared in AIServer/MAP.h:55 but has no definition (a).
- `CheckEvent` is declared in SMDFile.h:28 but has no definition. The real one is `C3DMap::CheckEvent` (a).
- CN3ShapeMgr has `SubCell(fX,fZ)` inline (N3ShapeMgr.h:94-106) and a public `m_pvCollisions`. Nothing calls either. `SubCell(const __Vector3&, …)` (h:93) is declared but not defined (a).
- There is no line-of-sight code anywhere. A search for Collision / IsBlocked / Intersect / sight / Ray / CheckColl only finds the loader, comments and `BUFF_TYPE_UNSIGHT` (a buff). `_IntersectTriangle` (a ray-triangle test, My_3DStruct.h:263-314) exists but is never called (a).
- **Object collision is parsed only.** Buildings and walls are loaded into memory and never queried at runtime (a).

## 2. GameServer Map and Region

- The zone list is built by `CGameServerDlg::MapFileLoad` (LoadServerData.cpp:381-405). It reads ZONE_INFO, calls `C3DMap::Initialize` for each zone (Map.cpp:19-38), then loads the EVENT table into `m_EventArray` (CEventSet, EventSet.h:9-43) (a).
- ZONE_INFO columns used by GameServer: `ServerNo, ZoneNo, strZoneName, InitX, InitZ, InitY`. Each Init value is divided by 100 (ZoneInfoSet.h:16, 31-38). AIServer uses `ServerNo, ZoneNo, strZoneName, RoomEvent` (h:14). The `Type`/`bz` columns and `_ZONE_INFO::m_bType/isAttackZone` (structs.h:252) are never loaded or used (a).
- Map files are loaded from `MAP_DIR "./map/"` (globals.h:7) with lower-cased names (SMDFile.cpp:19, 33), and each file is loaded once and ref-counted (a).
- ZONE_INFO rows (b, single permitted query):
  - zone 71: `freezone_a_20050718.smd`, Init 1000/1000/0 → (10.0, 10.0), RoomEvent 0.
  - zone 72: `freezone_b_20050718.smd`, Init (10, 10), RoomEvent 0.
  - Corroborating data (b): freezone_a holds warp IDs 7111/7114/7121/7124 (groups 711/712), and freezone_b holds 7211/7214/7221/7224. So **the server loads freezone_a for zone 71 (Ronark Land, 2048 m) and freezone_b for zone 72 (Ardream, 1024 m)**, and the data inside each file agrees with that. The opposite naming in the client's Zones.tbl was not checked (d).
- Region grid: square regions of VIEW_DISTANCE = 48 m. A unit's region is `(uint16)GetX()/48` (Unit.h:84-85). `C3DMap::GetRegion` bounds-checks the index (Map.cpp:40-48). **Zone 71 has 43×43 regions** (2048/48+1) and zone 72 has 22×22 (b+a). `CRegion` holds `std::set<uint16>` collections for users and NPCs plus a loot map (Region.h:8-21). Add/Remove are at Region.cpp:11-48 (a).
- Region changes: `Unit::RegisterRegion` (Unit.cpp:160-175) calls `AddToRegion`, then `RemoveRegion`/`InsertRegion`. Those send WIZ_USER_INOUT to the regions that drop out of or come into view (Send_OldRegions/Send_NewRegions, GameServerDlg.cpp:957-1004). `CUser::AddToRegion` is at CharacterMovementHandler.cpp:56-61 and `UserInOut` at 71-97 (it also sends AG_USER_INOUT to the AI server) (a).
- Broadcast helpers in GameServerDlg.cpp (a):

| Helper | Lines | Who receives it |
|---|---|---|
| `Send_Region` | 918-922 | 3×3 regions via `foreach_region` (globals.h:427) |
| `Send_UnitRegion` | 924-954 | one region; skips the excepted user and users not in game; filters by event room |
| `Send_NearRegion` | 1006-1034 | used by GENERAL chat: own region + 3 neighbours, then the user must be within 32 m (1062) |
| `Send_Zone` | 856-882 | everyone in a zone |
| `Send_All` | 892-916 | everyone online |
| `Send_PartyMember` | 1067-1081 | party members |
| `GetUnitListFromSurroundingRegions` | 1547-1576 | builds the 3×3 unit list used for AOE |

  `Unit::SendToRegion` (Unit.cpp:807-810) is `Send_Region` with no excepted user, so the sender receives the packet too.
- **View distance:** units in the 3×3 block of 48 m regions are visible. That guarantees at least 48 m in every direction and reaches at most about 96 m (c, from geometry).

## 3. Movement, warp, zone change (CharacterMovementHandler.cpp)

### 3.1 MoveProcess (4-54) (a)
- Packet: `uint16 x*10, z*10, y*10, int16 speed, uint8 echo` (15-16). It is ignored if `m_bWarp` is set or the player is dead (7-8).
- Speed: `SpeedHackUser()` (User.cpp:2862-2881) only checks the **client-reported `speed` field**. The limit is 45 by default, 67 for warrior/mage/priest, and 90 for rogue or COMMAND_CAPTAIN. A violation triggers Disconnect plus a global notice. GMs are exempt.
- Position: `IsValidPosition` only (21). There is no check on height, walkability (event grid), collision, distance or time per move. y is accepted from the client.
- On success: SetPosition (33); RegisterRegion (35) refreshes NPC, user and merchant in/out lists for the mover; stealth is removed (42-43); WIZ_MOVE `[sid][x][z][y][speed][echo]` goes to the 3×3 regions (45-47); `CheckEvent` runs (49); AG_USER_MOVE with float coordinates goes to the AI server (51-53).
- The only distance check is `SpeedHackTime` (User.cpp:3581-3607), which runs on WIZ_SPEEDHACK_CHECK. If `((dx²+dz²)/100) >= maxSpeed+10`, the player is warped back to m_LastX/Z. Between two checks that allows about 74.2 m by default, about 87.7 m for warrior/priest/mage, and 100 m for rogue (a for the formula, c for the arithmetic). How often the client sends the check is unknown (d).

### 3.2 CheckEvent (Map.cpp:105-172) (a+b)
- Tile values below 2 mean "no event" (110-137). The only exception is the nation-battle gate logic, which uses EVENT ids 1011/1012.
- Values of 2 and above index the EVENT table: type 1 is a zone change, otherwise a trap (GameEvent.cpp:11-28).
- **Zone 71's grid only contains 0 and 1** (b), so CheckEvent never fires an event in Ronark Land.

### 3.3 Warp, zone change, home, respawn (a)
- **Warp.** WIZ_WARP is accepted from GMs only (User.cpp:313-316). `Warp()` (626-659) checks IsValidPosition, sends WIZ_WARP, runs UserInOut OUT, sets the position and region, runs UserInOut WARP, refreshes the in/out lists and calls ResetWindows.
- **SelectWarpList** (User.cpp:4235-4278) takes a warp id that must exist in the current SMD and match the player's nation or be 0. It applies a random offset of up to ±r and calls ZoneChange. It does **not** check that the player is near a gate. **WarpListObjectEvent** (4423-4434) lists the warp group `sControlNpcID` from K_OBJECTPOS. **ObjectEvent** requires the player to be within `MAX_OBJECT_RANGE` = 100 squared, i.e. 10 m (Unit.h:18; User.cpp:4447-4448).
- **ZoneChange** (309-500):
  - `CanChangeZone` (174-293): GMs are always allowed. Otherwise the level must be within the map's min/max.
  - **Ronark Land (260-273):** requires MIN_LEVEL_RONARK_LAND = 35 and MAX_LEVEL = 80 (Define.h:161, 21). Entry is denied while a war is open unless `m_byBattleZoneType == ZONE_ARDREAM`, and denied when Loyalty is 0 or less. There is no nation restriction.
  - Ronark Land Base (246-259) always returns false.
  - If x and z are both 0, the START_POSITION table is used (331-339). Precedence quirk: Karus gets the exact coordinates; only El Morad gets the random offset (336-337).
  - When the zone actually changes: UserInOut OUT, SetZoneAbilityChange (User.cpp:1131-1183 sends WIZ_ZONEABILITY, adds the player to the PK-zone ranking and clears cooldowns), **the player is removed from their party** (417-423; PartyRemove is outside the if-block), rival removed, ResetWindows.
  - Then the position is set to (x, 0, z), the region set, and WIZ_ZONE_CHANGE(Teleport) plus AG_ZONE_CHANGE are sent.
  - The client completes a two-phase handshake in `RecvZoneChange` (668-699). Loading triggers in/out lists and a Loaded reply; Loaded triggers UserInOut RESPAWN, BlinkStart and RecastSavedMagic, and clears m_bWarp.
  - BlinkStart is a no-op when the map allows attacking the other nation (User.cpp:4529-4530), so Ronark Land has no spawn blink.
- **Home** (User.cpp:3695-3722): refused when the player is below 50% HP, dead, Kaul, frozen, or in zone 84/85/5x. Otherwise it Warps to `GetStartPosition` (3724-3756: START_POSITION for the player's nation plus a random 0..range offset).
- **Regene** (AttackHandler.cpp:95-240): uses a bind point (`m_sBind` object with byLife==1) if one is set; otherwise, in zone 71, `GetStartPosition`, i.e. START_POSITION (162-164). If Loyalty is 0 after respawning in a PK zone, the player is kicked to their nation (235-238).
- There is no server-side collision with gates. Gates are NPCs (AIServer/MAP.cpp:60-71) whose open state only reaches the client (c: movement never checks them).

## 4. Zone rules and Ronark Land

- Zone constants (Define.h:113-150) (a):

| Zone | ID | Zone | ID |
|---|---|---|---|
| KARUS | 1 | ELMORAD | 2 |
| ESLANT | 11/12 | MORADON | 21 |
| DELOS | 30 | BIFROST | 31 |
| ARENA | 48 | BATTLE | 61-66 |
| SNOW | 69 | **RONARK_LAND** | **71** |
| ARDREAM | 72 | RONARK_LAND_BASE | 73 |
| KROWAZ | 75 | BDW | 84 |
| CHAOS | 85 | JURAD | 87 |
| PRISON | 92 | | |

- Zone flags are set in `KOMap::SetZoneAttributes` (Unit.cpp:989-1149). For **71: `ZoneAbilityPVP`, `ZF_ATTACK_OTHER_NATION` only** (1100-1104), so there is no same-nation PK, no talking to or trading with the other nation, and NPCs are not friendly (a).
- `CUser::isInPKZone()` is true for 72, 71 and 73 (User.h:376) (a).
- **Hostility** `CUser::isHostileTo` (Unit.cpp:1219-1256): arena, then both in a safety area means false, then different nation and `isInPVPZone()` (1291-1304, uses canAttackOtherNation) means true, then temple zone, then siege. In zone 71: **different nation is always hostile; same nation is never hostile.** `Unit::CanAttack` (855-875) also requires the same zone, the attacker not incapacitated, and the target not dead or blinking (a).
- **Safety areas:** `isInSafetyArea` (Unit.cpp:1311-1345) has hard-coded boxes for Bifrost, Arena, El Morad, Karus, Battle, Battle2 and Delos, but **no case for 71**, so there is no safe zone in Ronark Land. The switch also falls through between cases, which is a bug (a).
- Hard-coded coordinates in code (a):
  - Moradon arena box: x 684-735, z 440-491 or 360-411 (Unit.cpp:1280-1281).
  - Safety boxes: Unit.cpp:1318-1340.
  - Delos castle owner spawn: (455|555 + rand 0..5, 790 + rand 0..5) (CharacterMovementHandler.cpp:359-365), and (505, 840) + range (User.cpp:3737-3738).
  - Mini-arena respawn: (734, 427) ± 5 (Define.h:170-172).
  - Dodo/Laon camp: (1054, 1141) / (1012, 914) ± 5 (Define.h:175-179).
  - Arena: (135, 115), (120, 115), (128, 125), (135, 120) (ArenaHandler.cpp:219-285).
  - NPC zone changes: zone 2 at (222, 1846), zone 1 at (1865, 168) (NPCHandler.cpp:461-465).
  - **No Ronark-specific coordinates exist in code.**
- Other Ronark Land rules (a):
  - NP per kill comes from ini values: `RONARK_LAND_SOURCE/TARGET` defaults 64/-50 (GameServerDlg.cpp:310-311), and the runtime GameServer.ini also has 64/-50 (b). Party division is handled by `GetLoyaltyDivideSource` (User.cpp:3158-3184).
  - **PvP monument** (NPC type 210): when killed, it sets `m_nPVPMonumentNation[zone]` and sends a MONUMENT_NOTICE to the zone (Npc.cpp:544-555). The holding nation gets +5 NP per kill (PVP_MONUMENT_NP_BONUS, GameDefine.h:163; User.cpp:642-643). A nation's own monument cannot be attacked (Unit.cpp:903-907).
  - Guard towers, gates and levers can never be attacked (908-915). Death to a guard tower in a PK zone produces a DeathNotice (User.cpp:4765-4766).
  - Being killed in a PK zone sets a rival (4884-4886).
  - Kick-outs: zone 71 is emptied when a war opens (GameServerDlg.cpp:2069-2075). Logging in to 71 during a battle sends the player to their native zone (CharacterSelectionHandler.cpp:177-186). The Bifrost timer is shown in 71 (EventHandler.cpp:14-28; User.cpp:1176-1177).
- Spawn and respawn points for 71 come from DB START_POSITION and K_OBJECTPOS bind objects, which the DB agent handles. The zone 71 SMD has **0 regene events**, so `KickOutZoneUser(true, 71)` falls back to the nation's start position (User.cpp:4660-4671) (a+b).

## 5. AIServer pathfinding

- `MAP::IsMovable(x,y)` is `GetEventID(x,y) == 0` (AIServer/MAP.cpp:124-127) (a).
- **Data:** in both maps, 100% of collision sub-cells and 100% of collision vertices with |y| ≤ 1000 fall on tiles with **event == 0**. With a transposed index the figures are only 13% and 12.9% (orientation_check.txt). So **0 = blocked, 1 = open** (b), and the index is confirmed as `x*n+z` with x = ⌊X/4⌋ (b). The height map uses the same layout: the median vertex-to-terrain error is 2.18 m with that layout versus 9.12 m transposed (b/c).
- `CPathFind` (PathFind.cpp) (a):
  - Grid: TILE_SIZE = 4 m, hard-coded (AIServer/Define.h:21).
  - Search window: a box of NPC position ± (distance + 2) m (Npc.cpp:993-997). Line 997 has a bug: it clamps `min_z` instead of `max_z`.
  - Called as `FindPath(end, start)`, i.e. reversed (Npc.cpp:1244).
  - 8 neighbours, no corner-cut check (119-146).
  - Costs: 10 orthogonal, 11 diagonal. The macro names are swapped (6-9).
  - Heuristic: integer Euclidean for the first node (78), but `max(x-dx, y-dy)` **without abs** for children (196). It is in tile units while g is in tenths, so it is weak and can be negative.
  - Open list: a sorted linked list with O(n) insert. Open and closed lookups are linear (212-268). PropagateDown uses +1 instead of 10/11 (282-304).
  - Node limit: stop after `count > 2*(|dx|*cx + |dy|*cy + 1)` (84-90).
  - Path length limit: 100 points (MAX_PATH_LINE, AIServer/Define.h:7; Npc.cpp:1257). NPC_MAX_MOVE_RANGE is 100 m (Define.h:50).
- **Inversion (c, high confidence):** `IsBlankMap` passes `IsMovable` through (338). That means A* only expands into **blocked** (0) tiles, so on Ronark Land it essentially always fails. Meanwhile `IsPathFindCheck` (Npc.cpp:3227-3283) treats `IsMovable()==true` as an obstacle, which is correct for this data.
- Monsters chasing a target skip A* altogether: `GetTargetPath` returns 0 (Npc.cpp:2120-2122), which leads to `IsNoPathFind`, a straight line with no map check (2476-2479, 3286-3341). In practice NPCs walk through obstacles (d: confirm in game).
- `bMove` in SetLive (Npc.cpp:690) is computed but unused (a).
- **GameServer has no pathfinding** (grep) (a).
- **Reuse options for bots in GameServer** (c):
  1. Read the walkable grid directly: `C3DMap` is a friend of SMDFile (SMDFile.h:69-70), so it can reach `m_smdFile->GetEventID()` and the private `m_fHeight` / `m_N3ShapeMgr`. Treat **walkable = (event == 1) and inside the main 4-connected component**.
  2. Write a new A* (binary heap, octile heuristic, no corner cutting) over the 512×512 grid, or port CPathFind with the semantics flipped and the heuristic fixed. Avoid sharing AIServer code as-is.
  3. Height: bilinear interpolation over `m_fHeight[x*513+z]` (4 m spacing).
  4. LoS: either Bresenham over event==0 tiles (cheap, conservative), or a ray test against `m_pvCollisions` using `SubCell()` lists and `_IntersectTriangle`. The very tall polygons at y ±5000 suggest invisible boundary walls (b/c).

## 6. Parsed map numbers (b) — `smd_parse.py`, all bytes consumed, 0 trailing

| | **Zone 71: freezone_a_20050718.smd** | Zone 72: freezone_b_20050718.smd |
|---|---|---|
| file size | 4,679,624 B | 1,522,560 B |
| m_nMapSize / tiles | 513 / 512 | 257 / 256 |
| unit distance | 4.0 m | 4.0 m |
| world extent | 2048 × 2048 m | 1024 × 1024 m |
| regions (48 m) | 43 × 43 | 22 × 22 |
| height min/max/mean | -30.633 / 82.122 / 2.826 m | 0.000 / 207.087 / 81.507 m |
| event histogram (all 513² cells) | 0: 29,522 (11.22%); 1: 233,647 (88.78%) | 0: 13,443 (20.35%); 1: 52,606 (79.65%) |
| event, tiles inside the extent | 0: 29,522 (11.26%); 1: 232,622 (88.74%) | 0: 13,443 (20.51%); 1: 52,093 (79.49%) |
| event values ≥ 2 | none | none |
| collision faces / verts | 34,871 / 104,613 | 12,416 / 37,248 |
| 16 m cells present (of grid) | 4,579 (of 128×128) | 1,661 (of 64×64) |
| shape refs | 3,353 | 783 |
| CC poly refs in 4 m sub-cells | 121,690 in 23,220 sub-cells | 45,586 in 9,023 |
| collision vertex bbox | x 34-2004, y ±5000, z 180.4-1838.2 | x 25.4-1009.9, z 9.7-1015.9 |
| event-0 tiles without collision polys | 6,302 | 4,420 |
| SMD object events (discarded by the server) | 2 | 2 |
| regene events | 0 | 0 |
| warps | 4 | 4 |

- Zone 71 object events. The 24-byte layout is **inferred** as int32 belong, 4×int16, 3×float. It is consistent with the warp groups:
  - belong=2, idx 4019, type 5 (OBJECT_WARP_GATE), ctrl 712, at (622.0, 11.6, 911.0)
  - belong=1, idx 4020, type 5, ctrl 711, at (1375.0, 11.8, 1085.0)
- Zone 71 warps. The coordinates are in the **destination** zone; r = 5, pay = 0:
  - 7111 "Luferson Castle" → zone 1 at (441, 1625), nation 1
  - 7114 "Moradon" → zone 21 at (295, 368), nation 1
  - 7121 "El Morad Castle" → zone 2 at (1595, 412), nation 2
  - 7124 "Moradon" → zone 21 at (352, 311), nation 2
- Connectivity of walkable tiles in zone 71 (event == 1):
  - 4-connected: 643 components. The largest (115,363 tiles) is the **outer area beyond the boundary wall**; it contains the ZONE_INFO Init point (10, 10), tile (2,2).
  - The **main playable area is 88,508 tiles (about 1.42 km²)**, shown blue in components4.png.
  - Several enclosed pockets of 2-4k tiles exist (green, yellow and others). They are probably bases (c).
  - 8-connected: 447 components; the largest are 115,516 and 93,070, because some lines are only one tile thick diagonally.
- Of the open tiles, 188,768 have no blocked neighbour (8-neighbourhood).
- **aievt files:**
  - Text format parsed by `MAP::LoadRoomEvent` (AIServer/MAP.cpp:197-339): lines `TYPE`, `ROOM n`, `NATION`, `POS minX minZ maxX maxZ`, `POSEND`, `A` (logic), `E` (exec), `O`, `L` and `END`; `;` and `/` start comments.
  - The file name comes from **ZONE_INFO.RoomEvent, not the zone id** (200), and it is only loaded when RoomEvent > 0 (79-91). Zone 71 has RoomEvent 0, so **71.aievt is never loaded** (b+a).
  - 71.aievt has 6 ROOMs. It matches 61.aievt except that rooms 5 and 6 use `A 1 11041 0` / `A 1 21041 0` instead of 11031/21031.
  - map.rar contains only aievt files: 61-64, 71, 1, 2, 21 (b).

## 7. Party (PartyHandler.cpp)

- Data structures (a):
  - `_PARTY_GROUP { WORD wIndex; short uid[8]; uint8 bItemRouting; string WantedMessage; uint16 sWantedClass; }`, with **MAX_PARTY_USERS = 8** (structs.h:291-312). `uid[]` holds socket IDs, slot 0 is the leader, and -1 means empty.
  - Parties are stored in `g_pMain->m_PartyArray`, keyed by `m_sPartyIndex` (an atomic counter; GameServerDlg.cpp:606-629).
  - Per-user fields: `m_bInParty`, `m_sPartyIndex` (uint16), `m_bPartyLeader` (User.h:242-243, 312, 318, 419).
- Opcodes: CREATE 1, PERMIT 2, INSERT 3, REMOVE 4, DELETE 5, PROMOTE 0x1C (packets.h:238-249) (a).
- **PartyRequest(memberSid, bCreate)** (85-166):
  - The target must exist, not be the requester, and not already be in a party.
  - Same nation is required except in Moradon and Forgotten Temple. Same zone is required. Not allowed in Chaos.
  - Level rule (unless the requester is "chicken" or both share a clan): the target's level must be within [2/3·L, 1.5·L] or within ±8.
  - On create: `CreateParty` (sets the leader to uid[0]), the 'P' state (StateChange 6,1), and AG_USER_PARTY CREATE to the AI server.
  - The target is pre-marked (`m_sPartyIndex`, `m_bInParty = true`) and receives WIZ_PARTY PERMIT.
- **Accept/decline** comes from the client as `PARTY_PERMIT` (28-33): 1 calls `PartyInsert` (168-266), 0 calls `PartyCancel` (52-83).
  - PartyInsert re-checks that the leader is in the same zone and the party is not full, puts the user in the first free slot, sends the member list both ways, and sends AG_USER_PARTY INSERT.
  - PartyCancel deletes the party if only the leader is left.
- **Promote** (268-335): leader only; swaps uid[0] with the promoted member's slot.
- **Remove/kick** (337-407): a member can remove themselves; only the leader can remove others. If the leader removes themselves, the party is deleted. If one member would be left, the party is deleted.
- **Delete** (409-443).
- **PartyBBS** (446-620): the "seeking party" board.
- Remove and Delete are no-ops in Jurad (339-341, 411-413). Changing zone removes the player from the party (CharacterMovementHandler.cpp:417-423).
- Magic checks (a):
  - `MORAL_PARTY` (single target) requires the target to have the same party ID (MagicInstance.cpp:826-842).
  - `MORAL_PARTY_ALL` (AOE): `UserRegionCheck` requires the same party ID and the radius test `isInRangeSlow(mouseX, mouseZ, radius)` (MagicProcess.cpp:121-142, 200-201). Candidates come from the 3×3 regions (MagicInstance.cpp:1634-1656). If nobody qualifies, the caster alone is used (1663-1664).
  - Group heal (type 3) is refused while any HOT is active on the caster (462-478).
  - Type-9 party stealth is applied to all members (2513-2535).
- **Entry points for a clientless entity** (a for the facts, c for the design):
  - The functions involved are `g_pMain->CreateParty(CUser*)`, `CUser::PartyRequest(sid, true/false)` followed by `invitee->PartyInsert()`, `PartyRemove(sid)`, `PartyPromote(sid)` and `PartyDelete()`.
  - All of them need real `CUser` objects that `GetUserPtr(sid)` can resolve. That lookup searches only `m_activeSessions` (KOSocketMgr.h:52-61). Bots therefore have to be CUser instances moved into the active session map (preallocated slots, lines 79 and 110-117).
  - `KOSocket::Send` silently returns false when not connected (KOSocket.cpp:140-143), so packets addressed to bots are dropped harmlessly. Whether `isInGame()`/state handling works for such users is untested (d).

## 8. Chat (ChatHandler.cpp)

- Types (packets.h:162-189): GENERAL 1, PRIVATE 2, **PARTY 3**, FORCE 4, SHOUT 5, KNIGHTS 6, PUBLIC 7, WAR_SYSTEM 8, PERMANENT 9, END_PERMANENT 10, MONUMENT_NOTICE 11, GM 12, COMMAND 13, MERCHANT 14, ALLIANCE 15, ANNOUNCEMENT 17, SEEKING_PARTY 19, GM_INFO 21, COMMAND_PM 22, CLAN_NOTICE 24, KROWAZ 25, DEATH_NOTICE 26, CHAOS_STONE_ENEMY 27, CHAOS_STONE 28, ANNOUNCEMENT_WHITE 29 (a).
- Inbound packet `CUser::Chat` (89-276): `u8 type`, `string(u16 length) msg`, plus `u8 options` when the type is 19. The message length must be 1-128 (104-106). Muted users (authority) and non-GMs in Prison are dropped (101-102).
- **There is no rate limit or spam/flood check.** Every message is written to the chat log file (267-275) (a).
- **Party chat:** `ChatPacket::Construct(&pkt, PARTY_CHAT, &msg, &senderName, nation, senderSid)` (ChatHandler.h:8-21) builds WIZ_CHAT `[u8 type][u8 nation][i16 sid][u8-len name][u16-len msg]`. It is sent with `g_pMain->Send_PartyMember(GetPartyID(), &pkt)` (ChatHandler.cpp:158, 187-193), and only if `isInParty()`. A server-side entity can send the same thing directly (a+c).
- Other routes: GENERAL uses Send_NearRegion with the 32 m filter; SHOUT costs 20% MP and uses SendToRegion; KNIGHTS goes to the clan; PRIVATE goes to `m_sPrivateChatUser`, which is set by WIZ_CHAT_TARGET type 1 (278-317) (a).
- **GM commands:** these only run when `isGM()` and the message starts with `'+'` with a second character that is not `'+'` (109-114, 344-370). The message is split on spaces, the first word is lower-cased, and it is looked up in `s_commandTable` (51-85: test, give_item, zonechange, monsummon, npcsummon, monkill, open1-6, captain, snowopen, close, np/exp/gold_change, np/exp/money_add, permitconnect, tp_all, summonknights, warresult, resetranking). An unknown "+…" is sent as normal chat. Console commands use the `/` prefix (ChatHandler.h:91; 506+) (a).

## 9. Targeting

- The server records the client's selected target only through **WIZ_TARGET_HP** (`uint16 uid, uint8 echo`). It sets `m_targetID = uid` and replies with the target's HP (User.cpp:326-333, 2322-2353).
- `m_targetID` lives in KOSocket (KOSocket.h:25, 44) and is **not initialised** in the constructor (KOSocket.cpp:6-12).
- It is used only by `+monkill` (ChatHandler.cpp:491-501) and `GetEventTrigger` (User.cpp:5851). It plays no part in combat (a).
- Attacks and skills carry the target explicitly: WIZ_ATTACK `tid` (AttackHandler.cpp:10, 35) and the magic `sTargetID`. Checks are range only: `isInAttackRange` uses a 15 m melee base, the weapon range and the skill range (User.cpp:5013-5060), and magic checks `sRange` (MagicInstance.cpp:304-306, 1332-1333). **There is no LoS check** (a). An attack sends WIZ_ATTACK to the region, plus `SendTargetHP` to the attacker on damage (Npc.cpp:223; User.cpp:1979).
- `Unit::GetDistance` is 2D in X/Z and returns the squared distance (Unit.cpp:138-141). Height is ignored in all range checks (a).

## Unknown / runtime tests (d)
1. How often the client sends WIZ_SPEEDHACK_CHECK, which sets the real teleport tolerance.
2. Whether NPCs in zone 71 really walk through obstacles (the inverted A* finding).
3. The client's Zones.tbl naming and minimap orientation versus the server's x/z axes.
4. START_POSITION and K_OBJECTPOS rows for zone 71 (DB agent).
5. Whether a socket-less CUser kept in m_activeSessions behaves correctly with the GAME_STATE / isInGame checks and AI-server synchronisation.
6. Whether the boundary polygons at y ±5000 are the outer walls.
