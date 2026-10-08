# B: ALPHA 1534 maps, quests and the new client vs our 1453 setup

Date: 2026-10-08. All work was read-only on the downloads. Nothing from the downloads was run or loaded. Python was run with `-I` from the scratchpad only.
Tags: **[V]** = verified in this session by byte, parse or DB evidence. **[A]** = inferred.
Scripts and raw outputs are in `scratchpad/k1534/b/`:
- `inv.py`, `inv.txt`: SMD inventory
- `cmp.py`, `cmp71.txt`, `cmp_mor72.txt`: SMD region diffs
- `gtdcmp.py`, `eqcount.py`, `zonematch.py`, `zonematch.txt`: client terrain vs SMD
- `bak_zone.py`: ZONE_INFO rows from the .bak
- `npccmp.py`: K_NPCPOS diff
- `qcmp.py`, `qapi.py`: quests
- `patscan.py`: exe patterns
- `dircmp.py`: client diff
- `defender.txt`

## 0. Verdict (short)

1. **Our Ronark Land (zone 71) nav data survives with the new client [V].**
   - The new client's zone-71 terrain is `Zones\Freezone_b.gtd` (new `Zones.tbl` row 710). It is byte-identical to the one in our client.
   - Its 513×513 height field equals our `freezone_a_20050718.smd` heights exactly: 263,169/263,169 cells, max diff 0.000 m.
   - The only Ronark change in the new client is objects: 4 bowl pillars swapped for a bigger model at the same spots (±2 m), plus 1 brazier added and 1 moved by about 1 m (§1.4).
2. **ALPHA's own zone-71 SMD (`Map/freezone_b.smd`) must NOT be adopted [V].** It is a reworked terrain that does not match this client:
   - only 73% of height cells are equal, max diff 52 m, and up to 33.6 m inside the bowl
   - 22,763 event cells changed, 1,217 of them in the bowl
   - 25 of 106 RoamRouteData points change walk status
   - collision 22,035 faces vs 34,871
3. **New Moradon:** ALPHA zone 21 = `moradon_0826.smd`. It is 1024 m and agrees in size with the client's `moradon.gtd/.tct` (1024 m). Two data defects [V]:
   - its height grid is **transposed** relative to the client: 89% of cells equal when transposed, 7% direct
   - its **event grid is all zeros**
4. **Quests:**
   - None of ALPHA's 599 `.lua` files is identical to any of our 114. All 114 same-named files differ, and their median line similarity is 0.40.
   - ALPHA's own `QUEST_HELPER` wires only 115 Lua names, 113 of which are present. The other 486 files are leftovers.
   - The new-Moradon NPC scripts (190xx, 29001) call only functions our server already registers, except `GetQuestStatus` in 13016_Keite.
5. **Client:**
   - The new client checks protocol version 1534. The `cmp ecx,1534` is at file offset 0x3C03C4 and `mov eax,1534; ret` is at 0x3C1580.
   - It uses JvCryption key `0x1257091582190465` (ALPHA source uses the same key). Our client uses `0x7412580096385200`.
   - No HackShield or XTrap. `d3d8.dll` is crosire d3d8to9 1.9.2.0.
   - Windows Defender: no threats found.

## 1. Map inventory and zone ids

### 1.1 Zone → map (ALPHA)
**Source of truth [V]:** `ZONE_INFO` drives map loading, not the source code.
- `GameServer/LoadServerData.cpp:422` `MapFileLoad()` → `C3DMap::Initialize` → `SMDFile::Load(m_MapName)`
- `MAP_DIR` is `"./map/"` (`shared/globals.h:8`), so the top-level `Server-Files/Map/` is loaded. `Map/Map/` is a stale copy and is not loaded.

ALPHA `ZONE_INFO` was read two ways and the results agree:
- decoded from the raw `DB/KN_online.bak` page at offset 0x734060
- queried from the restored `FDP_alpha1534` on `.\SQL2019`

| zone | ALPHA SMD (top-level `Map/`) | n (verts) | m | coll faces | warps | our SMD | identical? | new-client terrain match (exact-height cells) |
|---|---|---|---|---|---|---|---|---|
| 1 Karus | karus2004.smd 3,445,844 B | 513 | 2048 | 19,255 | 42 | karus_051221.smd | no | ALPHA 63.3%, ours 97.4% |
| 2 El Morad | elmo2004.smd 3,948,382 | 513 | 2048 | 26,955 | 42 | elmo_051221.smd | no | ALPHA 96.2%, ours 96.2% |
| 11/12 Eslant | k_/e_eslant.smd 1,140,624 | 257 | 1024 | 7,908 | 0 | k_/e_eslant_20050707 | no | 100% / 100% |
| **21 Moradon** | **moradon_0826.smd 2,999,762** | 257 | **1024** | 31,077 | 16 | moradon_20060124 (129 = 512 m) | no | ALPHA 7.0% direct / **89.0% transposed**; ours: size mismatch (512 m vs client 1024 m) |
| 30 Delos | war_a.smd 1,028,528 | 257 | 1024 | 6,916 | 5 | siege_0722 (same size) | no | ALPHA 66.4%, ours 100% |
| 31 Bifrost | dungeon_a.smd (= bifrost.smd, = our dungeon.smd) | 257 | 1024 | 7,159 | 0 | dungeon_1216 | no | ALPHA 41.4%, ours 100% |
| 32 | dungeon_b1th (= dungeon_b.smd) | 129 | 512 | 3,551 | 0 | dungeonb_0925 | **yes** | 100% |
| 33 / 93 | dungeon_b2th | 129 | 512 | 3,108 | 0 | dungeonc_1008 / dungeon_c | **yes** | 88.2% |
| 34 / 94 | dragon_a (= dragon.smd) | 257 | 1024 | 11,616 | 0 | dragon_room (34) / dragon.smd (94, **identical**) | — | client dragon_a is 128 m: size mismatch for ALPHA |
| 48 | arena.smd 109,800 | 65 | 256 | 654 | 0 | BattleField_20050801 108,800 | no (warps tail) | 100% |
| 51-55 | clanfight_b.smd | 65 | 256 | 252 | 0 | clanfight_b.smd | **yes** | 100% |
| 61 | battlezone.smd 1,364,312 | 257 | 1024 | 11,810 | 4 | bat_a 1,363,672 | no (warps 4 vs 2) | 97.1% |
| 62 | battlezone_b 875,122 | 257 | 1024 | 12,792 | 4 | bat_b 874,482 | no | 87.7% |
| 63 | battlezone_d 907,838 | 257 | 1024 | 13,698 | 4 | bat_c_20050718 | **yes** | 89.4% |
| 64 | battlezone_e 670,414 | 257 | 1024 | 7,030 | 14 | bat_d_051221 | **yes** | 79.6% |
| **71 Ronark Land** | **freezone_b.smd 2,439,170** | 513 | 2048 | 22,035 | 4 | **freezone_a_20050718 4,679,624** | **no** | **ALPHA 73.0%, ours 100%** |
| **72 Ardream** | **freezone_a.smd 822,336** | 257 | 1024 | 11,319 | 4 | **freezone_b_20050718 1,522,560** | no | ALPHA 99.5%, ours 99.7% |
| 81-83 | In_dungeon01-03 | 65 | 256 | — | 4 | In_dungeon_2005… | no | 64% / 99.7% / 97.3% (same for both) |
| 84 BDW | In_dungeon04.smd 766,474 | 257 | 1024 | 3,522 | 0 | dungeon.smd (placeholder) | — | client In_dungeon04 is 256 m: **size mismatch** for both |
| 85 Chaos | sky_war_2009 (= In_Dungeon05.smd) | 257 | 1024 | 8,409 | 0 | dungeon.smd | — | client `ch_qgs.*` missing in both clients |
| 87 Juraid | In_Dungeon05.smd 1,331,046 | 257 | 1024 | 8,409 | 0 | dungeon.smd | — | client In_dungeon05: **0% match** for both |

- Our `ZONE_INFO` has the same zone ids, plus 69 SnowWar [V].
- The SMD names for 71/72 are swapped between ALPHA and us, as KI-003 describes. The terrains are the same kind: Ronark is the 2048 m map in both, Ardream the 1024 m map.
- Header for all files: `mapSize = n`, `unitDist = 4.0`, extent = (n−1)·4 m [V].

**ALPHA top-level files not referenced by ZONE_INFO [V]:**
- `BattleZone_f.smd`: heights equal battlezone_e.
- `Border.smd`: heights equal ALPHA's freezone_b (the reworked Ronark).
- `bdw.smd`: heights equal battlezone_d; its warp list is truncated ("EOF inside warp 6").
- `bifrost.smd`: equals dungeon_a.
- `dragon.smd`: equals dragon_a.
- `dungeon_b.smd`: equals dungeon_b1th.

**`Map/Map/` contents (not loaded) [V]:**
- `itemzone_a.smd`: **our Ronark SMD with only the warp block changed.** Heights, collision, objects and event grid are identical. The Moradon warps 7114/7124 are moved to new-Moradon coordinates (807,527)/(827,528), and pay values are set.
- `old_moradon.smd`: identical to our `moradon_20060124.smd` in every region except the 10 warp records. Those have the same ids, positions and pay; only the warp names are Korean (CP949) instead of English. So it is **not byte-identical, but functionally identical**.
- `new_runawar.smd` / `unknown.smd`: copies of the reworked-Ronark terrain.
- `In_dungeon01-03`: 2048 m files under 256 m names, i.e. broken copies.

### 1.2 Zone 71 (Ronark Land): ALPHA `freezone_b.smd` vs our `freezone_a_20050718.smd` [V]

**Byte regions:**
- Header: identical.
- Heights: 75,147 cells changed. Max |Δ| 52.17 m, mean 0.59 m; 41,039 cells over 0.5 m, 20,741 over 2 m.
- Collision: 858,808 vs 3,099,262 bytes; faces 22,035 vs 34,871 (9,174 common).
- Event grid: 22,763 cells changed (13,307 0→1, 9,456 1→0).
- Main walk component: 96,579 vs ours 88,508.

**Areas (ALPHA vs ours):**

| area | height max / mean Δ | event cells changed | walk cells changed | collision faces (ours / ALPHA) |
|---|---|---|---|---|
| bowl, r150 around (1024,1024) | 33.6 m / 3.47 m | 1,217 of 4,421 | 1,475 | 1,816 / 1,101 |
| Karus gate, r60 around (1375,1085) | 0.98 m | 98 | — | 367 / 476 |
| El Morad gate, r60 around (622,911) | 0.08 m | 61 | — | — |
| gate-wait areas (r30) | ≤1 m | 0 and 2 | — | — |

- Our `RoamRouteData.h` (106 points): 25 points change event or walk status, max height Δ 10.6 m. Walkable points: ours 103, ALPHA 79.
- Warps: ALPHA's Moradon warps point to (817,530), the new-Moradon spawn. Ours point to (295,368)/(352,311). There is no regene in either.
- Object events: the same two gate objects in both, at (622,911) and (1375,1085).
- Block map (`blk.py`): ALPHA's differences are spread over the whole map, so this is a different terrain build.
- **Client check:** the client `Freezone_b.gtd` equals our SMD in 100% of cells and ALPHA's SMD in only 73%. **Conclusion: ALPHA's zone-71 map does not match the client. Ours does.**

### 1.3 Zone 72 (Ardream): ALPHA `freezone_a.smd` vs our `freezone_b_20050718.smd` [V]
- Heights: 101 cells differ (max 11.5 m).
- Event grid: ALPHA adds event ids 31 and 32 on 2,005 cells, which look like area triggers [A]. 251 cells go 0→1 and 115 go 1→0.
- Collision: 11,319 vs 12,416 faces. ALPHA's collision vertex table contains garbage floats (bbox about ±3e38). This is a data-quality red flag.
- Warps: re-targeted to new Moradon.
- Client match: ours 99.7%, ALPHA 99.5%. `Zones\freezone.gtd` (row 730/740) equals our 72 SMD exactly.

### 1.4 What changed for Ronark in the new client [V]
- `freezone_b.gtd/.tct/.tlt` and all other terrain files are unchanged.
- Changed object files:
  - `freezone_b.opd`: 4,834,083 → 4,834,811 B
  - `.opdsub`: 13,569 → 13,691 B
  - `.opdext`: same size but different bytes
- Shape-level diff (`opd` names with positions):
  - 4 × `obj_dun_pillar_room2_02` → `obj_dun_pillar_big02` at (973.6,997.2), (976.2,1107.5), (1015.4,1076.5), (1039.7,915.9), all within about 2 m of the old positions
  - `obj_co_dumbul07y` moved from (973.1,998.5) to (973.8,997.3), and a new one added at (1014.9,1076.8)
- All of these are in the bowl. Server nav uses only heights and the event grid, so it is unaffected. Human players may hit slightly larger pillar collision around those 4 spots [A].

### 1.5 START_POSITION / K_NPCPOS / K_OBJECTPOS (DB reference tables) [V]

**START_POSITION:**

| zone | ALPHA | ours |
|---|---|---|
| 21 | (817,530) for both nations, range 10/10 | (306,352) |
| 71 | Karus (1375,1098), El Morad (622,898), range 5 | Karus (1380,1090), El Morad (630,920) |
| 72 | (851,136) / (190,897) | (848,129) / (183,898) |

Our START_POSITION puts 10/10 in the `sKarusGateX/Z` columns rather than `bRange`.

**K_NPCPOS zone 71:**
- ALPHA has 296 rows / 818 spawns; ours has 209 rows / 655 spawns.
- **All 209 of our rows exist unchanged in ALPHA.** ALPHA adds 87 rows across 24 new NPC ids.
- Bowl r150 holds **103 rows / 179 spawns in ALPHA vs 16 / 16 in ours**. ALPHA adds Lupus, Lycaon, Barrkk, Barkirra, Blood Don, Lesath, Shaula, undying, Death knight, bone collector, Cardinal, Duke, Bach, Bishop, Samma, gates, Bifrost Monument 601 and Bifrost Gate 602, and two Orc bandit leaders.
- 94 of the 103 ALPHA bowl spawn centres are on event==1 cells of our SMD.
- Our bowl knowledge (16 spawns) therefore stays valid for our DB. Adopting ALPHA's K_NPCPOS would make the bowl far denser.

**K_OBJECTPOS zone 71:** ALPHA adds 48 type-50 rows (`OBJECT_EFECKT`, visual effects; 12 of them in the bowl) to the same two gate objects.

**K_NPCPOS zone 21:** fully different. ALPHA has 123 rows (new Moradon), ours 138. ALPHA-only NPCs:
- [Trainer] Kate 13016
- Rental booth / Novice Weapon Rental
- Elmo and Karus Dispatch Officers 19001 / 29001
- Entrep Trader 19002
- Mercenary adjutant / supply 19003 / 19004
- Tarot Reader 19005
- Arena Manager 19006
- scarecrows 19067-19072
- Nameless warrior 24414

## 2. Zones in ALPHA but not ours [V]
- **By ZONE_INFO:** no new zone ids; ours additionally has 69.
- **New real maps where we use placeholders:** 84 BDW `In_dungeon04.smd`, 85 Chaos `sky_war_2009.smd`, 87 Juraid `In_Dungeon05.smd`. None of these match the client terrain (see the table).
- **Defined in `Define.h` but with no ZONE_INFO row:** 35 Delos Castellan, 56 Lost Temple, 65 Zipang, 66 Oreads, 73 Ronark Land Base, 75 Krowaz, 92 Prison, 97/98 Winner Castle. `65.aievt`, `66.aievt`, `67.aievt`, `69.aievt`, `73.aievt` and `166.aievt` exist.
- New Moradon warps to zone 73 "Ronark Land Base" (515,104)/(513,916), but zone 73 has no map row. Dead warp [V].
- `.aievt`: `1/2/21/61-64/71` are byte-identical to ours.

## 3. New Moradon extent vs client [V]
- `moradon_0826.smd`: n=257, unit 4 → 1024 m.
- Client `moradon.gtd`: n=257. `moradon.tct` is 2,797,056 B, the 1024 m class. **They agree.** Our old client `moradon.gtd` is n=129 (512 m).
- Defects in `moradon_0826.smd`:
  - (a) Heights are stored transposed versus the client and versus every other SMD. At the 123 ALPHA NPC centres the median height Δ is 10.7 m direct and 0.0 m transposed.
  - (b) The event grid is 66,049 × 0, so our `NavGrid` (Walk needs event==1) would find nothing walkable.
  - (c) About 11% of cells differ even when transposed: 2008 SMD vs 2007 client edits [A].
- `ZONE_INFO` init is (815.90, 530.79, 4.69).

## 4. Quests [V]

**Counts:**
- ALPHA has 601 files: 599 `.lua` plus 2 extensionless (`25163_Rea`, `25167_Magaret`).
- Ours has 114.
- Identical: 0; whitespace-only: 0; changed: 114 (all of ours); ALPHA-only: 487; ours-only: 0.
- Of the 114 changed files, 113 are below 0.9 line similarity (median 0.40). Example: `14406_guardsman` is 1,658 lines in ours and 410 in ALPHA.

**What ALPHA actually wires up:** ALPHA's `QUEST_HELPER` (2,924 rows) references 115 Lua names, 113 of them present. **486 files are unreferenced.** These include junk such as "- Kopya", "- Copy" and "yedek" files, plus 25xxx/29xxx/31xxx scripts from newer servers.

**Wired ALPHA scripts we do not have:**
- 13016_Keite
- 19001_Lydiss, 19002_Burlet (85 helper rows), 19003_Leizy (421 rows, 1,196 lines), 19004_Osmund, 19005_Mackin, 19006_Dueler (→ zone 48)
- 29001_Potuman
- 14438_Beldan, 24438_Lugur, 14439_Agata, 24439_Pablo
- 16099_Asan (→ zones 1, 2, 71)
- 18030_Move, 21611_delaga
- 14434/24434_highprist (missing in both)

All except Keite (`GetQuestStatus`) use only Lua globals we already register. They still need DB rows: QUEST_HELPER, exchanges, talk ids, NPCs [A].

**Topic scan:**
- PromoteUser / PromoteUserNovice is used by job-change NPCs (Skaky, Clarence, Dreak, Minerva, Kaisan). PromoteKnight is used by 11610_charel and 21610_delaga. Both are registered in our server; our versions do not call them.
- Caitharos, Isiloon and Felankor appear only in an unreferenced `koland.lua` comment.
- **No pet scripts.**
- Juraid / BDW / Chaos references exist only in unreferenced files.

**Lua API gap:**
- ALPHA registers 112 globals; we register 100.
- ALPHA-only globals: CheckGiveSlot, EventSoccerMember, EventSoccerStard, GetPremiumTime, GetQuestStatus, GiveCash, GiveKnightCash, Logos, SendClanNameChange, ShowBulletinBoard, SpawnEventSystem, TempleOperations.
- **Wired ALPHA scripts** call these non-registered (for us) names:
  - ChangePosition (14 mortify files; also not registered in ALPHA → dead code)
  - CheckGiveSlot (1), GetQuestStatus (1), SendClanNameChange (1)
  - CheckPrison (1) and GiveLogTimeItem (1), which ALPHA does not register either
- **Unreferenced files** call about 30 names that neither server registers, e.g. QuestMonsterCount (42 files), SendNpcKillID (62), Draki*, Temple*, JuraidTempleEventJoin. They come from a newer codebase.
- Our own quests already call 11 unregistered names (baseline noise).

## 5. Client binaries [V]

**KnightOnLine.exe (5,059,072 B):**
- Section layout: `.nero` (code; ours is named `.text`), `.data`, unnamed, `.rsrc`, `.idata`. PE timestamp is zeroed, as in ours. It looks like an unpacked dump, as does ours.

**Pattern search:**

| pattern | new exe | our exe |
|---|---|---|
| `81 F9 FE 05 00 00` (cmp ecx,1534) | 0x3C03C4 | — |
| `B8 FE 05 00 00 C3` (mov eax,1534; ret) | 0x3C1580 | — |
| 0x05AD (1453) | none | 0x365004 / 0x3660F0 |
| 0x05E1 | none | none |
| 0x05E2 | one hit, a `jne/jmp` false positive | — |

**Crypto keys:**
- Not present as 8-byte constants in either exe.
- They are pushed as two imm32 halves (`68 15095712 68 65041982`):
  - new exe: `0x1257091582190465` at 0x6E920, 0x6EA3D, 0x6EC1B, 0x6ED17, 0x3C037D
  - our exe: `0x7412580096385200` at 0x61CF0 and following, 0x364FBD
- ALPHA `shared/JvCryption.cpp` uses `0x1257091582190465` for `__VERSION` 1534. Our server uses `0x7412580096385200`.
- **Our server needs version 1534 and the 1534 key to accept this client.**

**Server address:** both exe and Launcher read `Server.ini [Server] Count/IP%d`. The exe also reads `[Version] Files`, `[Join] Registration site`.
- `Server.ini`: `IP0=127.0.0.1`, `Files=1534`, `Registration site=knightsempire.com`, `[IncludeExe] Last=1534`. Ours has 1473/1473.

**Anti-cheat:** there are no HackShield, XTrap or GameGuard files or strings in the new client. Our exe still contains XTrap strings and `URLDownloadToFileA`.

**New exe URLs:** only `knight.mgame.com` and `k2shop.knightonlineworld.com`. There are no IP literals. Our exe contains `211.115.86.66` and `knightonlinedrake.com`.

**d3d8.dll (114,688 B, new; our client has none):**
- crosire **d3d8to9 1.9.2.0** (2019 build, MIT wrapper)
- exports only `Direct3DCreate8`
- imports d3d9, kernel32, user32, gdi32, shell32 (ShellExecuteW, used only to open the microsoft.com DirectX download URL) and the VC140 CRT
- no network imports

**Launcher.exe (2,506,753 B):**
- PE timestamp 2008-07-09, file date 2023-04-30
- official K2 Network patcher: WININET/WSOCK32 FTP patching, EULA text
- URLs: `knightonlineworld.com`, `/announce.php`, `knight-online.net/register/join.htm`; no IP literal

**Option.ini:** 1024×768, 16-bit, `WindowMode=0`, `[PetOption]` and `[BufferLine]` sections.

**Other:**
- `info/UTD*_<account>.txt`: per-account UI files from the packager (names not copied).
- `log.klg`: an encrypted client log.

**Data tables:**
- Newest file: `Quest_Menu_us.tbl` dated 2021-04-11 (repacker edit). The rest are 2007-09-12 or older. Ours are 2006-07 or older.
- `Cloak.tbl`: 6,762 B, 224 rows, 7 columns; ours is 1,740 B, 56 rows, 5 columns. **The format changed.**
- `item_org_us.tbl`: 39 columns, 1,401 rows; ours 37 columns, 1,198 rows.
- `npc_us.tbl`: 7 columns, 582 rows; ours 3 columns, 547 rows.

**Zones:**
- `Zones.tbl` adds rows 870 (In_dungeon05, Juraid), 930 (dungeon_b2th, Isiloon) and 940 (dragon_a, Felankor). All other rows are identical.
- Rows 850/860 point to `ch_qgs.*`, which is missing in both clients.

**Assets:**
- Long capes: `cloak_101-104`, `cloak_111-113` (.n3mesh and .n3cplug), `cloak_004.n3mesh`, `cloak_c_10…48`, `cloak_c_60-64` are new. The new client has 82 cloak files, ours 30.
- Pets: `npc_pet_bf*` assets and `co_pet_*` UIs exist in **both** clients (50 vs 36 files). The client side is present; the server has no pet code or quests.
- New Moradon NPC visuals: `npcimg` +22 files; the client NPC table grew.

## 6. New client vs our client (counts) [V]

| dir | changed | new |
|---|---|---|
| total (22,575 vs 19,622 files) | 401 | 2,959 (6 of ours gone, all in `symbol_us`; 19,215 identical) |
| Object | 101 | 973 |
| fx | 2 | 701 |
| Item | 48 | 606 |
| UI | 15 | 315 |
| UI_US | 39 | 146 |
| Chr | 20 | 84 |
| symbol_us | 2 | 66 |
| npcimg | 3 | 22 |
| Misc | — | 20 |
| Zones | 87 | 11 (all `in_dungeon05.*` plus `moradon_xmas.flag`) |
| Data | 79 | 9 |
| root | 5 | `d3d8.dll` |

Data new tables: caption_us, cml, fortune_us, mon, npcmopmap_info_us, quest_helper, quest_npc_desc, skill_magic_5, tiptable_us.

Changed zone terrains: battlezone, battlezone_b, battlezone_d, dungeon_b1th/b2th, elmo2004, eslantzone, karus2004, moradon (now 1024 m), moradon_xmas. Ronark (`freezone_b`) changed only in object files.

## 7. Verdict and actions

- **Keep for zone 71:** our `freezone_a_20050718.smd` heights and event grid.
  - The NavService grid, `RoamRouteData.h` / `RoamRoutes` and `docs/appendix/maps` for zone 71 do **not** need regenerating for the new client [V].
  - Optional: re-check bowl clearance near the 4 swapped pillars with a human client [A].
- **Do not adopt:**
  - ALPHA `freezone_b.smd` (zone 71)
  - `Border.smd`, `new_runawar.smd`, `unknown.smd`
  - ALPHA `karus2004.smd`, `war_a.smd`, `dungeon_a.smd`: all match the client worse than ours
- **Adopt only if new Moradon is wanted:**
  - `moradon_0826.smd`, but **transpose the height grid and rebuild the event grid** before use. As shipped it is unusable for nav.
  - Re-target the Moradon warps in our zone-71 and zone-72 SMDs: copy only the warp block, as ALPHA's `itemzone_a.smd` shows (to 807,527 / 827,528).
  - Take ZONE_INFO 21 and START_POSITION 21 from ALPHA.
  - Take the wired new-Moradon quest files 190xx, 29001 and 13016 together with their QUEST_HELPER and exchange rows.
- **Placeholders:** the ALPHA maps for 84/85/87 do not match this client's terrain. Keep our placeholders.
- **Leave alone:** the 486 unreferenced ALPHA Lua files.
- **Red flags:**
  - Defender scan of `C:\dev\fdp1534`: "found no threats" (exit 0).
  - No network or injection strings in d3d8.dll.
  - The exe code section is renamed `.nero` (cosmetic, from the unpacker).
  - The Launcher still uses an FTP patcher. Its patch URL comes from the login server (`[DOWNLOAD] URL=ftp.yoursite.net` in ALPHA's LogInServer.ini), so do not run it against a public server.
  - ALPHA Ardream collision contains garbage vertices.
  - `bdw.smd`'s warp list is truncated.
