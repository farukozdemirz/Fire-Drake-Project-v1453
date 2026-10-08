# G: Server map (SMD) for the new Moradon (zone 21), 1534 client — method for U3

Date: 2026-10-08. Read-only on the repos, the DBs and the downloads. Nothing from the downloads was run. Python was run as `python3 -I`, with scripts in the scratchpad only.
Code references are to `/mnt/c/dev/fdp-u1534` (branch `yukseltme/1534`, HEAD `1bbe5bd5`).
Tags: **[V]** = verified in this session by parse, byte or DB evidence. **[A]** = inferred.
Scripts and outputs are in `scratchpad/k1534/g/` (list in §10).

---

## 0. Verdict (short)

1. **Do not repair ALPHA's `moradon_0826.smd`. Build zone 21 from the client's own files.** [V]
   - ALPHA's file is a **different, later revision** of the map: a soccer field where the client has sea, land in the north-east where the client has water, and about 4,200 more collision faces.
   - Transposing its heights still leaves 11% of cells wrong (max 60 m). Its collision matches the client on only 20,380 of 26,839 faces.
2. **The client files carry everything the SMD needs, in the SMD's own layout** [V]:
   - heights: `Zones/moradon.gtd`, MAPDATA, index `x*n+z`. This is the same layout as our SMDs; Ronark is 263,169 of 263,169 equal.
   - collision: `Zones/moradon.opd`, which starts with a `CN3ShapeMgr` collision block byte-compatible with the SMD one. Our Ronark SMD block equals the old client's `freezone_b.opd` block: same 34,871 faces, all 4,579 cells and shape lists, 17 sub-cell lists differ.
3. **The editor's walkability rule is now known and reproducible** [V]: a tile is **0 (blocked)** iff its 4 m collision sub-cell has any polygon, **or** max−min of its 4 corner heights is **≥ 10.0 m**. Otherwise it is **1**. The last row/column is 1.
   - This reproduces the official event grid of old Moradon, Ronark and Ardream at **100%** from the SMDs' own data.
   - Run end to end from the client `.gtd`/`.opd`, it reproduces the grid at **99.995%** (Ronark, 11–13 of 263,169 tiles differ) and **99.992%** (Ardream, 5 tiles).
   - Other maps differ only where designers painted event ids ≥ 2 or manual blocks (battle zones).
4. **Prototype output:** `out/moradon_1534.smd`, 2,708,222 B, md5 `03429b74…` [V]
   - It loads in the `SMDFile::LoadMap` mirror with 0 trailing bytes.
   - 72.3% of tiles are walkable; the old Moradon has 72.1%.
   - START_POSITION (817,530)+[0..10]² is 121/121 walkable.
   - 64 of 69 ALPHA monster spawn centres are walkable.
   - The town is a separate walkable component, as in the official old Moradon.
5. **Beyond the SMD, U3 needs these changes, or players land in wrong places** [V]:
   - warp records into zone 21 in **6 of our SMDs** (zones 1, 2, 30, 71, 72, 81)
   - **5 of our Lua quests** with old Moradon coordinates
   - ZONE_INFO / START_POSITION / K_OBJECTPOS for zone 21
   - drop the 2 zone-73 warps from ALPHA's list: our `GetWarpList` sends a wrong count when an entry is skipped
6. **AIServer** loads the same SMD and uses only the event grid. `21.aievt` is identical in ALPHA and ours, holds old-map coordinates and is inert. Set ZONE_INFO.RoomEvent 21 → 0, as ALPHA does.
7. **Bots are unaffected.** NavService is zone-71 only. Patching the zone-71 SMD's warp block changes neither its heights nor its events, so the NavGrid fingerprint stays the same.

---

## 1. Inputs, layout and conventions

| file | size | md5 | role |
|---|---|---|---|
| client `Zones/moradon.gtd` | 621,587 | `1867f157…` | heights (n=257) [V] |
| client `Zones/moradon.opd` | 4,981,872 | `b844a8a7…` | collision block @15..2,306,747 + 7,180 shapes [V] |
| client `Zones/moradon.opdext` | 382,419 | `81ce8dcf…` | 416 extra faces; all their tiles are already blocked (§3.4) [V] |
| ALPHA `Map/moradon_0826.smd` | 2,999,762 | `2cfce818…` | donor for warps and object events only [V] |
| ALPHA `Map/Map/moradon.smd` | 2,999,762 | `2cfce818…` | **byte-identical** to `moradon_0826.smd` [V] |
| ours `moradon_20060124.smd` | 847,672 | `68b1f6c3…` | old 512 m Moradon, used as the reference grid [V] |

**New-client header** [V]: `int32 L=7`, 7 tag bytes, `int32 flag(0)`, then data at offset `4+L+4 = 15`. This holds for `.gtd`, `.opd` and `.opdext`.
- `.gtd` @15: `int32 n=257`, then `n²` × `{float32 h; uint32 tileinfo}` (8 B). The remaining 93,176 B are patch, light and grass data and are not needed.
- `.opd` @15: `float 1024, float 1024, int32 faces=26,839, Vector3[faces*3]`, then 64×64 main cells exactly as `N3BASE/N3ShapeMgr.cpp:52-113` (2,743 present). Then `int32 7180` shapes. Each shape record starts with tag `0x041b`, or `0x141b` when it has event fields, then an `int32 len + name + pos(3f) + quat(4f) + scale(3f) …` and ends with `belong, eventID, eventType, npcID, npcStatus`.

**Index convention** [V]: the server indexes both grids as `x*n+z`.
- `SMDFile::GetEventID` (`shared/SMDFile.cpp:200-206`) and `BotCore/NavGrid.h:64` `Idx(x,z)=x*m_n+z`.
- Heights use the same layout (collision-vertex fit, `docs/appendix/research/C_map_party_chat.md` §5).
- **There is no `SMDFile::GetHeight`.** Heights are exposed only as the raw `GetHeights()` and are read only by NavService (zone 71).
- `AIServer/MAP.h:55` declares `GetHeight`, but it is never defined.

---

## 2. Q1 — Heights

| comparison (client `moradon.gtd` vs ALPHA SMD) | exact (<1 mm) | >0.5 m | >2 m | max | mean | median |
|---|---|---|---|---|---|---|
| direct (`x*n+z`) | 4,528 (6.9%) | 59,398 | 55,443 | 75.28 m | 16.12 m | 10.85 m |
| **ALPHA transposed (`z*n+x`)** | **58,738 (88.9%)** | 6,713 | 5,482 | **60.15 m** | 1.68 m | 0.00 m |

- The transpose recipe is `h_fixed[x*n+z] = h_alpha[z*n+x]`. It is correct as an orientation fix [V]. At the gates, START and the Folk and Tale villages it gives the client's 4.74 / −7.83 / 1.33 m exactly.
- The remaining 11% are **not noise**. They are other terrain, concentrated in four areas (64 m blocks with > 0.5 m differences) [V]:
  - south, x 576–896 / z 0–256: client terrain −24…−32 m (sea), ALPHA −6 m flat. This is the soccer field of ALPHA's K_OBJECTPOS 1019–1022 and `29079_Soccer.lua`. The client `.opd` has no object there apart from a ship.
  - north-east, x 640–1000 / z 576–860: e.g. (880,800) is −31.65 m in the client and +4.74 m in ALPHA.
  - the town plaza, where ALPHA is 1–6 m higher
  - the arena / west-of-town strip, x 576–768 / z 256–480
- No other n=257 `.gtd` in the new client matches ALPHA, either direct or transposed. Only `moradon.gtd` reaches 88.9% [V].
- **Recipe:** heights = the client `.gtd` floats, copied as they are (exact by construction). Keep the transposed ALPHA only as a diagnostic: the tool prints the 88.9% figure as a sanity check that the right map is used.
- **Server impact of heights in zone 21 is nil today** [V]. GameServer reads heights only in NavService (zone 71). The AIServer `m_fHeight` is never filled. NPC Y is client-side. Correct heights matter for the event grid (slope rule) and for any future nav use.

---

## 3. Q2 — Collision block

### 3.1 ALPHA vs client [V] (`collcmp.py`)
- Orientation: **not transposed.** 20,380 faces are exactly equal (0.01 m) direct and 0 after swapping x and z. The vertex bbox is identical in both: x 3.2…1075.4, z −16.6…1025.8.
- Content: **a different revision.**
  - ALPHA has 31,077 faces and client 26,839. 10,697 faces are only in ALPHA and 6,459 only in the client.
  - Main cells: ALPHA 2,944, client 2,743.
  - ALPHA's cell lists reference shape ids up to 9,097, but the client `.opd` has 7,180 shapes. ALPHA's object table is therefore not this client's.
  - The differences sit in the town, around the arena and in the south (soccer field).
- Boundary walls (|y| = 5000): ALPHA 896, client 832.

### 3.2 Convention proof: an SMD collision block is the editor's `.opd` block [V] (`collsame.py`, `colldiff.py`)
- Our Ronark SMD vs the old client's `freezone_b.opd`:
  - 34,871 = 34,871 faces
  - all 4,579 cells and their shape lists identical
  - 17 sub-cell polygon lists differ (a 144 B difference)
- The old Moradon SMD (2006-01) vs the old client opd (2006-03): different revisions (9,421 vs 10,282 faces), which is also expected.

### 3.3 Server use [V]
- `CN3ShapeMgr` has no query method. The server uses the block only for the size check, `Width()==Height()==(n-1)*4` (`shared/SMDFile.cpp:81-85`), and for `IsValidPosition` (`:194-198`, bounds only).
- **Recipe:** copy the client block bytes verbatim, `opd[15:2,306,747]` (2,306,732 B).

### 3.4 How to verify
1. Exact triangle-set comparison: done; this is stronger than comparing bboxes.
2. Byte equality of the copied block. Its md5 is checked by the tool.
3. Shape-event cross-check: the client shape events equal ALPHA K_OBJECTPOS (§5.3).
4. `.opdext`: rasterise its 416 faces in x–z. 3,124 tiles are touched and **0** of them are walkable in the generated grid [V], so the extra faces change nothing.

---

## 4. Q3 — Event grid

### 4.1 What the servers read [V]

| consumer | code | meaning |
|---|---|---|
| GameServer movement | `CharacterMovementHandler.cpp:22`, `SMDFile::IsValidPosition` | bounds only (x,z < 1024). The grid is not consulted. |
| GameServer triggers | `GameServer/Map.cpp:105-171` `C3DMap::CheckEvent` | value < 2: nothing (outside war). Value ≥ 2: `EVENT` row (ZoneNum, EventNum). **Zone 21 has no EVENT rows** in ours or in ALPHA, so ids ≥ 2 are useless here. |
| GameServer safety / arena | `Unit.cpp:1263-1281` `isInArena`, `:1311-1345` `isInSafetyArea` | hard-coded rectangles; Moradon is not in `isInSafetyArea`. Neutrality comes from zone flags (`Unit.cpp:1018-1021`). **The grid holds no town flag.** |
| AIServer | `AIServer/MAP.cpp:124-127` `IsMovable = (event==0)`; `PathFind.cpp:335-338`; `Npc.cpp:3227-3283` | 0 = blocked, 1 = open in the data. A* has the known inverted semantics (`docs/appendix/research/H_aiserver.md` §5); chases skip A*. `SetLive` ignores walkability (`Npc.cpp:690`). |
| Bots | `GameServer/Bot/NavService.cpp:191` | zone 71 only |

**So zone 21 needs only 0/1 walkability with official semantics.** Gates are object events from K_OBJECTPOS plus SMD warps, not grid cells.

### 4.2 The editor's rule, recovered and validated [V] (`evrule.py`, `slope2.py`, `rulecheck.py`, `e2e.py`)

Over the three reference maps, every tile with a collision sub-cell is 0: there are **0 "collision but open" tiles**. For the remaining blocked tiles, the corner-height range separates blocked from open perfectly:
- Ardream: blocked min 10.000 m, open max 9.9996 m
- old Moradon: blocked min 11.07 m, open max 9.98 m

Normal-angle and edge-slope measures do **not** separate the two classes.

**Rule R** (generator):
```
for x,z in [0, n-2]:  ev[x*n+z] = 0 if subcell(x,z) has polys or (max(h00,h10,h01,h11) - min(...)) >= 10.0 else 1
row x=n-1 and column z=n-1:  1      (observed in all reference maps)
```

| test | agreement |
|---|---|
| Rule R on SMD-internal data: `moradon_20060124`, `freezone_a` (71), `freezone_b` (72), `dungeonb/c`, `clanfight_b`, `dragon_room` | **100.000%** |
| `karus/elmo/eslant/bat_a/dragon/dungeon*` | 95–99.9%; differences are only designer event ids ≥ 2 painted over blocked tiles |
| `bat_b/c/d`, `BattleField` | 83–91%; manually painted blocks and ids |
| **End to end from client files** (`.gtd` + `.opd` → R → compare with the official SMD grid): Ronark, old client | **99.9958%** (11 tiles) |
| same, Ronark, new 1534 client (pillars changed) | **99.9951%** (13 tiles) |
| same, Ardream (`freezone.gtd/opd`) | **99.9924%** (5 tiles) |
| same, old Moradon (client 2006-03 vs SMD 2006-01, different revisions) | 97.85% |

### 4.3 The candidate sources (a)–(d)
- **(a) slopes + (b) collision:** these are the rule. Both are needed.
- **(c) client `.gev` / `.evtsub` / `.flag`:** not walkability [V].
  - New `moradon.gev` is 4 B with 0 events. The old one had 6 rectangles.
  - `.evtsub` holds area-name regions ("moradon_port", "south_field", "north_swampy land", "python ravine").
  - `.flag` and `.gfo` are banner flags.
- **(d) spawns:** used for **validation**, not generation.

### 4.4 Generated grid vs references [V] (`validate.py`, `comps.py`, `variants.py`, `warpcheck.py`)

**Histogram:**
- new: 0 = 18,183 (27.5%), 1 = 47,866 (72.5%)
- old Moradon: 0 = 4,570 (27.5%), 1 = 12,071 (72.5%)

**Components (4-connected):**
- outer land: 27,187 tiles. It contains the Folk Village (411,525), the Tale Village (81,919) and both arenas.
- sea and low ground plus the edge row: 13,558 tiles, mean h −15 m. The editor does not block water.
- **town: 1,558 tiles, contains START.**
- The official old Moradon has the same pattern: START sits in an 858-tile town component cut off from the outside by 4 m blocking at the gates. This is editor behaviour, not a defect.

**Reference points:**
- START (817,530)+[0..10]²: 121/121
- inbound arrival (817,530) r5: 81/81; (807,527) r5: 65/81; (827,528) r5: 79/81
- Folk r5: 80/81; Tale r5: 73/81
- `MINI_ARENA_RESPAWN` (734,427) r5: 81/81
- arena rectangles: 127/169 tiles each
- Karus gate 4014: 162/317 walkable points within 10 m; El Morad gate 4013: 100/317
- **Anvil 5001 (816,606): 0/317.** Every tile around it has collision polygons, from the raised plaza structure. This is harmless: the ObjectEvent check is distance-only (`User.cpp:4661-4672`, `MAX_OBJECT_RANGE` 100 = 10 m squared, `Unit.h:18`, `Unit.cpp:138-141`).

**ALPHA K_NPCPOS zone 21 (123 rows):**
- monsters: 64 of 69 centres walkable; the other 5 are 4 m from a walkable tile; 6–7 of 69 rects are < 50% walkable
- NPCs: 26 of 54 centres walkable; the other 28 stand at stalls and buildings. They are static, and SetLive ignores the grid.
- Baseline, our old map with our DB: NPCs 10/52 and monsters 10/86 on 0.
- A grid built from ALPHA's own data fits ALPHA's NPCs no better (23/54).

**Grid built from ALPHA heights/collision vs from client data:** 5,124–5,379 tiles differ (7.8%); walkable 70.0% vs 72.3%.

---

## 5. Q4 — Warps, regene and object events

### 5.1 Zone 21 warp list (gate menus, group = `sWarpID/10` = K_OBJECTPOS ControlNpcID 211 / 212) [V]

ALPHA has 16 records and regene 0. Our old file has 10 records and regene 0.

| ALPHA id | name | target | ALPHA pos | on **our** target map |
|---|---|---|---|---|
| 2111/2121 | Folk Village | 21 | (411,525) r5 | new grid 80/81 |
| 2112/2122 | Tale Village | 21 | (81,919) r5 | 73/81 |
| 2113 | Luferson Castle | 1 | (437,1627) | 81/81 (ours (441,1625) 81/81) |
| 2114 / 2124 | Lunar Valley | 1 / 2 | (1860,169) / (216.5,1847) | 81/81 |
| 2115/2125 | Delos | 30 | (505,252) | 81/81 (ours (506,249) r3 29/29) |
| 2116 / 2126 | Ardream | 72 | (851,136) / (190,897) | 81/81 |
| **2117/2127** | **Ronark Land Base** | **73** | (515,104) / (513,916) | **no ZONE_INFO 73: drop** |
| 2118 / 2128 | Ronark Land | 71 | (1375,1098) / (622,898) | 81/81 (ours (1380,1090) 57/81, (630,920) 81/81) |
| 2123 | El Morad Castle | 2 | (1598,407) | 81/81 |

- **Drop 2117/2127.** `CUser::GetWarpList` (`User.cpp:4528-4563`) writes `uint16(warpList.size())` **before** it filters out entries whose zone does not exist. The 1534 client would get a count one higher than the entries [V]. The same bug already bites in wartime for Ardream/Ronark entries (`:4546-4553`). It is a pre-existing issue and a KNOWN_ISSUES candidate.
- Data notes [V]:
  - ALPHA charges 3000 for Ardream (Karus, 2116) but 17000 for Ardream (El Morad, 2126). This looks like a typo.
  - ALPHA's pays differ from ours (Lunar Valley 10000 vs 5000).
  - IDs are free to renumber: nothing references them except the menu round-trip (`SelectWarpList`, `User.cpp:4454-4503`).
- **Recommendation:** an explicit JSON warp spec = ALPHA's 14 records (without zone 73), out-of-zone coordinates as validated above, pay 2126 → 3000. Whether to keep our old pays is an owner choice.

### 5.2 Warps INTO zone 21 that must be patched (our SMDs; old-Moradon coordinates) [V] (`warps21.py`)

| zone | file | records → old target |
|---|---|---|
| 1 | `karus_051221.smd` | 114 → (288,369) |
| 2 | `elmo_051221.smd` | 214 → (337,318) |
| 30 | `siege_0722.smd` | 3014/3034 → (290,370), 3024/3044 → (337,318) |
| 71 | `freezone_a_20050718.smd` | 7114 → (295,368), 7124 → (352,311) |
| 72 | `freezone_b_20050718.smd` | 7214 → (295,368), 7224 → (352,311) |
| 81 | `In_dungeon_20050718.smd` | 8111 → (285,372), 8121 → (337,318) |

- New target: **(817,530) r5** for every record (81/81 walkable, inside the town). ALPHA uses the same in `freezone_a/b`, `karus2004`, `elmo2004`, `battlezone` and `war_a`.
- Alternative: nation-side (807,527) / (827,528), as in ALPHA `Map/Map/itemzone_a.smd`, `war_a` and `In_dungeon01-03` (65/81 and 79/81).
- **Patch method:** only `fX` @+300 and `fZ` @+308 of each 320 B `_WARP_INFO` record with `sZone==21` (`shared/database/structs.h:221-239`, `pack(1)`). Validate that the byte diff is exactly 8 B per record.

### 5.3 Object events (SMD block and K_OBJECTPOS) [V]
- The server **discards** the SMD object-event block (`SMDFile.cpp:116-127`) and uses `K_OBJECTPOS` (`Map.cpp:174-184`). The tool keeps ALPHA's 29×24 B block for fidelity; a count of 0 would also load.
- The client `.opd` shape events agree with ALPHA K_OBJECTPOS:

| client shape | position | event fields | ALPHA K_OBJECTPOS |
|---|---|---|---|
| `obj_potal_2005_karu` | (797.7, 4.7, 526.9) | 4014 / belong 1 / type 5 / npc 211 | (797.70, 526.88) |
| `obj_el_potal_gate_2005` | (837.4, 5.0, 526.8) | 4013 / belong 2 / type 5 / npc 212 | (837.36, 526.76) |
| `obj_co_itemup` | (816.1, 12.7, 605.5) | 5001 / type 8 (anvil) | (816.14, 607.17) |

- Our K_OBJECTPOS 21 still has the old-map positions (282,374), (338,318), (338,390).
- Do **not** import ALPHA's 1019–1022 (type 0, x 632–710, z 140–180, the soccer field) or `29079_Soccer.lua`. In this client that area is sea.

### 5.4 Other moves to zone 21 [V]
- `ZoneChange(...,0,0)` uses START_POSITION (`CharacterMovementHandler.cpp:364-371`).
- Login with a stored position inside 1024 m is accepted as is (`User.cpp:951-958`). Characters saved in old Moradon will appear at the same numbers on the new map, unless USERDATA zone-21 positions are reset. That is an owner decision: a one-time UPDATE, no reads.
- ZONE_INFO Init is used by `MagicInstance.cpp:2327` and `KickOutZoneUsers`.
- **Our Lua with old-Moradon coordinates:**
  - `13010_Move` (450,300)
  - `17010_Craf` (87,13)
  - `17012_Parias` (450,300)
  - `18006_Execute` (100,100)
  - `27009_Jakara` (87,13)
- ALPHA's versions use (817,451) / (817,432) / (817,448). Note that (817,451) and (817,448) fall on ev=0 tiles (11/29 walkable within 3 m); (817,432) is 19/29.
- **The hard-coded arena constants are already new-map coordinates** (x 684–735 lies outside the old 512 m map): `isInArena` `Unit.cpp:1279-1280`, `MINI_ARENA_RESPAWN` (734,427) `Define.h:170-172`. The client fences run at x 679.6–733.4 and z 440.7–492.3 (A) / 363.6–416.5 (B). Rectangle B is about 5 m south of the fence, an optional tuning item.

---

## 6. Q5 — AIServer and `21.aievt` [V]
- The AIServer loads the same file via ZONE_INFO (`AIServer/MAP.cpp:55`, `SMDFile::Load(name)` with no warps and no regene, `SMDFile.h:17`). It reads heights (unused), collision (size only), object events (skipped) and the event grid (A*).
- `21.aievt`: md5 `1c9daf48…` is identical in ALPHA and ours. It is loaded because **our** ZONE_INFO.RoomEvent for 21 is 21; ALPHA's is 0. The file name is `%d.aievt` of RoomEvent (`MAP.cpp:196-200`).
  - Content: ROOM 01, `POS 326 308 394 432` (old-map coordinates), logic `A 7/6/3`.
  - `CRoomEvent::CheckEvent` handles only logic 1–5 (`RoomEvent.cpp:63-…`), so the room never progresses.
  - No zone-21 spawn has DungeonFamily > 0 (ALPHA 123/123 and ours 138/138 are 0), so "Map Room Npc Fail" (`ServerDlg.cpp:393-400`) cannot fire.
  - It is inert. **Recommendation:** RoomEvent 21 → 0.
- AIServer adds object NPCs for K_OBJECTPOS types gate / anvil / … (`MAP.cpp:58-71`). With the new K_OBJECTPOS, the anvil 5001 lands at (816,607).
- The ALPHA `LimitMin*` columns are swapped by name in zones 1/2/21 [V]. They are read only when `DungeonFamily > 0` (`ServerDlg.cpp:370-376`), so they are irrelevant for zone 21.

---

## 7. Q6 — Generator design: `tools/u3-moradon-smd.py`

Prototype: `scratchpad/k1534/g/u3_moradon_smd.py`. It follows repo tool conventions: stdlib only, validation through `docs/appendix/tools/smd_parse.py`, and a `--selftest` like `tools/nav-export.py`.

**Commands:**
```
build          --client-zones <.../Knight Online/Zones> --donor <moradon_0826.smd> [--warps <spec.json>] --out <moradon_1534.smd>
patch-inbound  --map-dir /mnt/c/dev/fdp/server/Map --files karus_051221.smd,elmo_051221.smd,siege_0722.smd,
               freezone_a_20050718.smd,freezone_b_20050718.smd,In_dungeon_20050718.smd --x 817 --z 530 --out-dir <dir>
verify         --smd <out> --points <points.json> --spawns <K_NPCPOS export>     (offline report, exit≠0 on failure)
--selftest     rule R on the reference SMDs (expect 100%) and the e2e client→grid tests (≥ 99.99%)
```

**`build` steps:**
1. Parse the client header (`int L`, L bytes, `int flag`) and require the `.gtd` n = 257.
2. Read the heights: MAPDATA stride 8, no transpose.
3. Parse the `.opd` collision from offset `4+L+4`. Require 1024×1024. Copy the block bytes and collect the sub-cells that have polygons.
4. Build the events with rule R (threshold 10.0 m; edges = 1).
5. Take ALPHA's object-event block and regene (count 0).
6. Build the warps from the spec, or from ALPHA minus zone 73.
7. Write in `SMDFile::LoadMap` order: `n, unit, heights, collision, objects, events, regene, warps`.
8. Write a sidecar JSON with the input and output md5s and all counts.

**Expected sizes:**
- 8 + 264,196 + 2,306,732 + 700 + 132,098 + 4 + 4 + 320·W bytes
- **2,708,222 B at W = 14**

**Offline validations (block the deploy):**

| id | check | expected (prototype) |
|---|---|---|
| V1 | `smd_parse.parse(out)`: size check, 0 trailing bytes, counts | ok / 0 / faces 26,839, obj 29, regene 0, warps 14 |
| V2 | heights == client `.gtd` | 66,049/66,049; diagnostic ALPHA^T 88.9% |
| V3 | collision block md5 == `opd[15:end]` | equal |
| V4 | selftest rule R | 100% (Moradon-old, 71, 72); e2e ≥ 99.99% (71, 72) |
| V5 | START square, inbound r5, Folk/Tale, MINI_ARENA, arena rectangles | 121/121, 81/81, 80/81, 73/81, 81/81, 127/169 |
| V6 | ALPHA spawn centres (monsters) walkable | ≥ 64/69 |
| V7 | walkable share vs old Moradon | 72.3% vs 72.1% (±3%) |
| V8 | `.opdext` faces touch walkable tiles | 0 |
| V9 | warp spec: group ∈ {211,212}, target zone in ZONE_INFO, destination walkable on the target map | all |
| V10 | `patch-inbound`: diff only 8 B per `sZone==21` record; `nav-export.py` crc32 for zone 71 unchanged | yes |

**DB / scripts that go with it (not SMD; U3 DB script):**
- ZONE_INFO 21: `strZoneName='moradon_1534.smd'`, Init 81590/53079/469, RoomEvent 0
- START_POSITION 21: 817/530 for both nations, `bRangeX/Z=10`. Ours currently has 10/10 in the Gate columns and bRange 0.
- K_OBJECTPOS 21: 4013 / 4014 / 5001 at the new positions (+ ALPHA type-50 effects optional; no 1019–1022)
- K_NPCPOS 21 from ALPHA (depends on the U2 NPC ids 13016, 19001–19006, 19067–19072, 29001 and monsters 1056–1058, 1180)
- the 5 Lua files
- optional USERDATA zone-21 position reset

---

## 8. Runtime checks (T-UPG-02 and the related ones)

1. **GameServer start:** no "is not a valid map file" or "does not exist" for zone 21. Old file kept for rollback.
2. **AIServer start:**
   - no "Unable to load room event" (RoomEvent 0)
   - "Monster All Init Success - N" with N = Σ NumNPC
   - no "NPC %d in zone %d that does not exist"
3. **Spawn:** a new character, `/town` and death in zone 21 all land in [817..827]×[530..540] on the plaza, not inside geometry.
4. **Stored old position:** a test character saved at old coordinates, e.g. (306,352), logs in at that spot on the new map. This confirms whether the USERDATA reset is needed.
5. **Walk:** 30 min around town, both gates, the arenas, Folk and Tale villages; no disconnect or rubber-band. The server does not gate movement.
6. **Gate menus:** Karus at 4014, El Morad at 4013.
   - The list count equals the entries, with no zone 73.
   - Each destination lands on its target map (1, 2, 30, 71, 72; Folk, Tale) and gold is deducted.
   - Repeat during a war (known count bug).
7. **Inbound:** gates in 71 (7114/7124), 72 (7214/7224), 1 (114), 2 (214), 30 (30x4), 81 (8111/8121) arrive at (817±5, 530±5).
8. **Arena:**
   - attack allowed inside both rectangles and not outside
   - death inside → respawn at (734±5, 427±5)
   - check the 5 m strip at the north edge of arena B
9. **NPCs:**
   - town NPCs appear at their client minimap markers
   - anvil upgrade works within 10 m of (816,606)
   - monsters wander in the outskirts with no mass stuck-in-wall
10. **Lua teleports** (13010, 17010, 17012, 18006, 27009, + 18010/21914/21924 if imported) land in town.
11. **Bots:**
    - the `NavService: nav ready: zone 71 … crc32=…` value is unchanged after `patch-inbound`
    - `run-tests.sh`, the 8v8 and roam scenarios are unchanged (T-UPG-04)
    - bots never enter zone 21; it is gated by `ZONE_RONARK_LAND` in `BrainDriver`, `RoamDriver` and `ActionExecutor`
12. **T-UPG-05 (old client, CLIENT_VERSION=1453):** the old client has the 512 m Moradon. Zone 21 data is selected by the DB, not by the build, so test with the old ZONE_INFO / SMD set or accept that old-client Moradon breaks.

---

## 9. Risks and open points
- **ADR-0068 §5 and `docs/17` U3** currently say "repair `moradon_0826.smd` (transpose + regenerate grid)". This report recommends building from the client files instead, with ALPHA only as the warp / object donor. The wording should be updated when the U3 plan is written.
- Rule R is the editor's **conservative** grid: raised plazas and building floors become 0, and sea and low ground stay 1, both as in the official maps. Fine for the AIServer. If bots ever use zone 21, a water level and a "floor polygon" refinement would be needed (not in U3).
- The client header tag (7 bytes) could differ in other client builds; the tool asserts n=257 and 1024×1024.
- Zone-21 coordinates live in five places that must change together: the SMD warps of 6 zones, the DB (ZONE_INFO, START_POSITION, K_OBJECTPOS, K_NPCPOS), Lua, the client-saved USERDATA positions, and hard-coded arena constants (already correct).
- The `GetWarpList` count bug (wartime) is pre-existing.
- **Owner decisions:**
  - warp pays (ALPHA vs ours)
  - inbound target: one point (817,530) or nation sides
  - USERDATA reset
  - old-client compatibility of zone 21

## 10. Scratchpad artifacts (`scratchpad/k1534/g/`)
- `u3_moradon_smd.py`: generator prototype
- `validate.py`, `warpcheck.py`, `comps.py`, `variants.py`: validation
- `evrule.py`, `slope2.py`, `rulecheck.py`, `e2e.py`: rule discovery and self-tests
- `coll.py`, `collcmp.py`, `collsame.py`, `colldiff.py`: collision
- `hcmp.py`: heights
- `opdscan.py`, `opdev.py`: `.opd` shapes and events
- `warps21.py`: warp scan
- `npcpos21_alpha.txt`, `npcpos21_ours.txt`: spawn exports
- `out/moradon_1534.smd`: prototype output, md5 `03429b745aa3c7cc9d6a34ee65320673`
- `out/moradon_1534_events.png`: grid preview with START, the gates, the villages and the arena marked
