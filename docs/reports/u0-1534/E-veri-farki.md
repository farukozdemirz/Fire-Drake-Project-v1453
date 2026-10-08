# E: 1534 data delta for U2 (capes, items, NPCs, zones, quests, version)

Date: 2026-10-08. Scope: read-only analysis for phase U2 (data migration to the 1534 client, ADR-0068).
Nothing was written to either DB, to any repo or worktree, or to the download folders. Python ran only as `python3 -I` with scripts in the scratchpad. Personal-data tables were not read.

Tags: **[V]** = verified this session by decoding, a query, or disassembly. **[A]** = inference.

**Paths**
- New client: `/mnt/c/dev/fdp1534/client/Knight Online` ("new")
- Old client: `/mnt/c/dev/fdp/Client` ("old")
- OUR DB: `.\SQLEXPRESS` / `FDP_kn_online`
- ALPHA DB: `.\SQL2019` / `FDP_alpha1534`
- Our source: `/mnt/c/dev/fdp-merge-final` (branch `main`)
- ALPHA source: `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE`

Prior reports B and C (`docs/reports/u0-1534/`) are reused rather than repeated.

**Scratchpad (`scratchpad/k1534/`)**
- `e_scripts/`:
  - `tbl.py`: the generic decoder
  - `capecmp.py`, `itemset.py`, `itemcmp.py`, `itemattr.py`, `botgear.py`, `itemclass*.py`, `npccmp.py`, `pos21.py`, `mopmap.py`, `mob.py`, `looks.py`, `qhcmp*.py`, `luatalk.py`, `luaitems.py`, `namecoll.py`, `ourunres.py`, `tail.py`
- `e_out/`:
  - `new/*.tsv`, `old/*.tsv`: decoded client tables
  - `db/*.txt`: SELECT dumps of reference tables
  - `scan_new.txt`, `scan_old.txt`, `npccmp.txt`, `qhcmp*.txt`, `pos21.txt`, `item_missing_bases.txt`, `botgear_ids.txt`

---

## 0. Verdict (short)

1. **Decoder [V].** Every `.tbl` in both clients decodes cleanly with the rolling-XOR key 0x0816 / 0x6081 / 0x1608, except:
   - `Slander_us.tbl`: 77 B, byte-identical in both clients, not a table under this key. It is irrelevant.
   - New `Quest_Menu_us.tbl`: 480 valid rows followed by 56 trailing garbage bytes. This is the 2021 repacker edit.

   `Skill_Magic_Main_us.tbl` decodes cleanly in both clients: 1,864 rows × 33 columns (new) and 1,779 × 31 (old).
2. **Capes [V].**
   - Column map of the new `Cloak.tbl`: id, name, gold price, duration, clan grade, clan-point price, minimum clan type (`KNIGHTS.Flag`).
   - The new client knows **168 cape ids that our DB lacks**.
   - ALPHA has the same 224 ids, and price, duration and grade all agree. **ALPHA's `nBuyLoyalty` is 0 and its `byRanking` is wrong on 144 rows** (row 164 even has `byRanking` 22). **Use the client table, not ALPHA.**
   - **Long (royal) capes = the 84 ids whose ranking is 8–12**: x40–x48 and x60–x64 for x = 0..5. The exe switches to the `Cloak_1%.2d.n3cplug` mesh exactly when ranking ∈ [8,12].
3. **Items [V].**
   - The new client resolves **121,706 item ids** (1,401 base rows); the old client resolves 102,805.
   - **35,861 client-resolvable ids are missing in our ITEM. 35,860 of them exist in ALPHA:**
     - 19,418 are under 205 base items we lack entirely
     - 16,443 are extra variants under bases we already have
   - Our ITEM has no id collisions: Kind/Slot/Race/Class/Damage/Ac agree 100% with the client on all 85,844 resolvable ids.
   - All 83 bot-gear ids still resolve. The client changed 21 of them, all for display only:
     - ReqLevel lowered on 19 (e.g. reverse +5 armour 68→60)
     - Race 19→20 on 2 enchant scrolls
   - `ItemClass` for new rows can be derived from client `item_org` column 36 (grade) plus the upgrade digit. The rule matches **99.5%** of our rows.
4. **NPCs/monsters [V].**
   - Npc_us: 74 new ids (and 39 removed). Mob_us: 32 new ids.
   - Client-known ids missing in OUR DB: **82 in K_NPC, all of them in ALPHA**; **65 in K_MONSTER, 60 of them in ALPHA**.
   - **ID collision: our K_NPC 24438/24439/24440** are the zone-64 "Warder 1/Warder 2/Keeper". The 1534 client uses those ids for Legure (Prospective Knight Squad Captain), Pablo and a village board.
   - ALPHA's zone-21 placement (123 rows) matches the client's own Moradon minimap: **93 of 132 markers lie within 20 m of an ALPHA spawn, versus 0 of ours**. It also matches the client's `Zones\21.mob` (74 of ALPHA's 75 ids).
5. **Zones [V].** `Zones.tbl` keys are zone×10:
   - 210 → `Zones\Moradon.gtd`, which is now the 1024 m map
   - 710 → `Zones\Freezone_b.gtd`
   - All 53 old rows are byte-identical. The new rows are 870 (In_dungeon05), 930 (dungeon_b2th) and 940 (dragon_a).
6. **Quests [V].** QUEST_HELPER is client-coupled:
   - ALPHA = new client exactly (2,924/2,924 rows identical).
   - OUR DB = old client (3,454 common rows identical).
   - New client vs OUR DB: 156 nIndex values missing in ours (all in ALPHA), 963 differing, and **552 nIndex values reused for different NPCs/Lua**. The new-Moradon NPCs 19001–19004 and 29001 take over index ranges that our Guardsman/Melburic/Moradon/Kape/Rucel/Jalk quests use.
   - Our 114 Lua scripts use 1,908 talk ids. All of them still exist, but **624 now show different text, 268 of them completely different**.
   - Quests therefore cannot be a row copy. They need their own phase.
7. **VERSION [V].** LoginServer uses `MAX(sVersion)` (currently 1473). The launcher in `Server.ini` uses `Files=1534`. Add one row: 1534, history 1473.

   The game-server version check (`cmp ecx,0x5FE`) is in the WIZ_VERSION_CHECK handler and is a U1 code concern, not data.
8. **Numbering [V].** `db/010_bot_crowd.sql` and **`db/011_bot_crowd_clans.sql` already exist**: 011 is on `yukseltme/1534`, `gece/2026-10-08-kalabalik` and `bot/F11-173..183`. **U2 scripts must start at `db/012`.**

---

## 1. Client table decoding [V]

- `e_scripts/tbl.py` copies the algorithm of `tools/client-tbl-quests.py`:
  - decrypt: `plain = enc ^ (key>>8); key = ((enc+key)*0x6081+0x1608) & 0xFFFF`, starting at 0x0816
  - then int32 column count, int32 type per column, int32 row count, then the rows
  - It also tries plaintext as a fallback (no file needed it).
- `scan` mode was run over both `Data/` folders (`e_out/scan_new.txt`, `scan_old.txt`):
  - new: 108 of 109 tables decode cleanly
  - old: all but `Slander_us.tbl` decode cleanly
- Key layout changes:

| table | old | new |
|---|---|---|
| Cloak.tbl | 5 col / 56 | 7 col / 224 |
| item_org_us | 37 / 1,198 | 39 / 1,401 |
| Item_Ext_0..42_us | 53 col each | 53 col each (more rows) |
| Npc_us | 3 / 547 | 7 / 582 |
| Mob_us | 4 / 823 (Korean names) | 4 / 855 (English) |
| MON.tbl | — | 4 / 837 (subset of Mob_us) |
| NPC_Looks | 38 / 260 | 38 / 285 |
| Zones.tbl | 25 / 53 | 25 / 56 |
| Quest_Helper_us | 18 / 3,489 | 18 / 2,924 (`Quest_Helper.tbl` 2,923) |
| Quest_Talk_us | 4 / 2,779 | 4 / 3,475 |
| Quest_Menu_us | 2 / 265 | 2 / 480 (+56 B trailer) |
| Quest_Monster_Exchange_us | 21 / 150 | 21 / 180 |
| Item_Exchange_us | 26 / 1,167 | 26 / 2,318 |
| NpcMopMap_info_us (new) | — | 7 / 1,158 (minimap markers) |

---

## 2. Capes (KNIGHTS_CAPE)

### 2.1 Column map [V]

| col | type | meaning | KNIGHTS_CAPE column | evidence |
|---|---|---|---|---|
| 0 | u32 | cape id | sCapeIndex | ids equal the DB's |
| 1 | str | name (CP949 for Korean rows) | strName (the server does not load it) | `KnightsCapeSet.h` loads `sCapeIndex, nBuyPrice, byGrade, nBuyLoyalty, byRanking` |
| 2 | u32 | gold price | nBuyPrice | all 224 equal ALPHA, all 56 equal ours |
| 3 | u32 | duration (always 0) | nDuration | — |
| 4 | u8 | clan grade requirement (1 best .. 3; 0 = none) | byGrade | equal |
| 5 | u32 | clan-point price (36,000 … 2,880,000) | nBuyLoyalty → `nReqClanPoints`, deducted from `m_nClanPointFund` | `NPCHandler.cpp` HandleCapeChange |
| 6 | u8 | minimum clan type: 2 = Promoted, 3–7 = Accredited 5..1, 8–12 = Royal 5..1 | byRanking (server: `m_byFlag < byRanking` → error −6) | enum `ClanType*` in `GameServer/Knights.h:28-40` |

- The old `Cloak.tbl` has columns 0–4 only.
- The textures are chosen by id. The exe computes the colour texture at `0x47DE08` and the pattern texture at `0x47DE87`:
  - colour = `id % 10000 % 100` → `Item\Cloak_C_%.2d.dxt`
  - pattern = `id % 10000 / 100` → `Item\Cloak_M_%.2d.dxt`
- Every texture the 224 ids need is in the new client: `cloak_c_00..18`, `c_29..33`, `c_40..48`, `c_60..64`, `c_99` and `M_01..05`.

### 2.2 Which ids are new [V]

- **168 ids are in the new client and not in OUR DB.** The pattern is the same for every prefix p = 0..5 (p×100 +):
  - **10–18**: 9 colours; clan points 36,000; ranking 3 (Accredited 5)
  - **29–33**: Korean names 쇠창살/십자가/체크무늬/대칭/쌍독수리 (iron bars, cross, checkered, symmetric, double eagle); clan points 180,000/288,000/432,000/648,000/864,000; ranking 3–7
  - **40–48**: 9 colours; clan points 360,000; ranking 8 (Royal 5)
  - **60–64**: 왕실5..1 (royal 5..1); clan points 1,080,000 / 1,368,000 / 1,728,000 / 2,160,000 / 2,880,000; ranking 8–12
  - All of these have gold price 0 and grade 0.
- Our 56 ids (0–9, 99, p01–p09) equal the old client and equal the new client on every column except byRanking. Ours is 0 and the client is 2.
- This changes nothing in behaviour, because `HandleCapeChange` already requires `isPromoted()`.
- The bot clan capes 103/206/303/509 (db/009, Flag 2) are unaffected.

### 2.3 ALPHA vs client [V]

- The ids match 224/224. nBuyPrice, nDuration and byGrade match on all rows.
- **nBuyLoyalty differs on 144 rows.** ALPHA has 0 everywhere except x29–x33 for p = 2..5 and 130–133.
- **byRanking differs on 143 rows.** ALPHA has 2 instead of 3/8–12. Row 164 has 22, which no clan type can reach, so that cape could never be bought.
- Only 80 rows are fully equal: the 56 old capes plus 24 of the x29–x33 rows.
- **Conclusion: take the numeric values from the client `Cloak.tbl`.** Taking ALPHA's would make royal capes free and available to any promoted clan.

### 2.4 Long (royal) capes [V]

- At `0x47DD2D` and `0x48F93A`, the exe reads the cape record byte at +0x24 and runs `cmp al,8 / jb / cmp al,0xC / ja`:
  - inside [8,12]: it loads `Item\Cloak_1%.2d.n3cplug`, the long meshes `cloak_101..104` and `111..113`
  - otherwise: it loads `Item\Cloak_%.3d.n3cplug`
- Offset +0x24 is the ranking column, assuming a 16-byte (VC6-style) `std::string` [A]. A 28-byte string would put +0x24 on the always-zero duration column, which would make the code dead.
- **Long capes = 84 ids:** 40–48, 60–64, 140–148, 160–164, 240–248, 260–264, 340–348, 360–364, 440–448, 460–464, 540–548, 560–564.

---

## 3. Items (ITEM)

### 3.1 Client item model [V]

- `item_org_us` row = base item (`dwID`, a multiple of 1000), with `byExtIndex` in column 1.
- `Item_Ext_<byExtIndex>_us` row = variant (`dwID` 0..999, unique per table). **Item id = base + ext id.**
- Ext column 2 is a BaseID that designates the base for unique rows (0 = generic). 204 ext rows point to bases that do not exist (dead rows).
- The column maps were validated against ALPHA, whose ITEM was built from this client. Agreement:
  - 100% on Kind (org 10), Slot (12), Race (13), Class (14), Damage (org 15 + ext 8), Ac (22 + 14), Weight, ItemType (ext 7), ReqRank, ReqTitle
  - 97.7–98.4% on ReqLevel (org 26 + ext 45) and ReqStr/Dex/Int/Cha (org 30–34 + ext 48–52)
- Org column 36 is the item grade, used for ItemClass (§3.4). The new columns 37 and 38 were not identified.

### 3.2 Counts and diffs [V]

| set | count |
|---|---|
| new client base rows / old | 1,401 / 1,198 (205 new, 2 removed: 379079000, 379152000 "Blessed Item Upgrade Scroll") |
| client-resolvable ids (BaseID = 0 or own base) new / old | 121,706 / 102,805 |
| OUR ITEM rows / resolvable by new client | 85,920 / 85,844 (63 are noext/noorg, mostly pet rows 620xxx/630xxx that are unknown to both clients; 13 are ext rows designated to another base) |
| ALPHA ITEM rows / resolvable by new client | 123,720 / 121,704 (2,015 rows have no client base) |
| **client ids missing in OUR ITEM** | **35,861**, of which **35,860 are in ALPHA** (1 is in neither) |
| … under bases ours lacks entirely (205 bases; 204 of them have ALPHA rows) | 19,418 |
| … extra variants under bases ours has | 16,443 |
| ids OK in the old client but not in the new (ignoring BaseID) | 2 (the removed scroll bases above) |

- Predicted kinds of the 35,860 ALPHA rows: Kind 110 4,273; Kind 60 2,800; accessories 91–94: 8,190; armour 210–240 about 6,300; and others. See `itemclass6.py`.
- **ID collisions: none.** Every id we would insert is absent from ours. For all 85,844 shared ids, our Kind/Slot/Race/Class/Damage/Ac equal the new client's 100%. Our ReqLevel matches the client on only 2–9% of rows, because the project lowered ReqLevel; see C §3.5.
- The 14 wired ALPHA scripts (190xx, 29001, 13016, 1443x, 2443x, 18030, 16099) give or take 90 distinct item ids:
  - 15 are missing in ours
  - 6 of those are in ALPHA: 389191000, 700011001, 700012000, 800090000, 800440000, 800450000
  - 9 are in neither ALPHA nor the client: dead rewards in ALPHA's Lua (379290000, 9719xxxxx …)
- ALPHA's `ITEM_EXCHANGE` is **empty (0 rows)**. The client's `Item_Exchange_us` has 1,141 ids that ours lacks. The client table is the only source for those, and its 26-column layout would need mapping. Deferred.

### 3.3 The 83 bot-gear ids (db/002, 004, 005, 006, 007, 008) [V]

- All 83 resolve in the new client.
- 62 have identical client attributes in both clients. **21 changed, all for display only:**
  - ReqLevel lowered on:
    - 15 reverse +5 armour pieces `216/276/296 00{1..5} 005`: 68→60
    - 191110007/8: 75/77→63
    - 170250256: 32→12
    - 190251131: 24→8
  - The +11 reverse pieces did not change.
  - Race 19→20 on 800061000 and 800062000 (enchant scrolls).
- Server-side enforcement uses OUR ITEM, which is unchanged. 59 of the 83 show higher requirements in the client than our server enforces (ours: ReqLevel 1 and lowered stats). Bots are unaffected. A human tester's client may grey these out [A].

### 3.4 ItemClass for new rows (ALPHA has NULL everywhere) [V]

- Kind/Slot alone is not enough: (Kind, ItemType) → ItemClass is only 62% pure in ours.
- The rule that works is grade (client `item_org` column 36) plus the upgrade digit:
  - ItemType 3 → 0
  - grade 5 → 4 (reverse)
  - grade 3 → 3
  - grade 1/2 → grade + 1 if `Num % 10 >= 7`, else grade
  - grade 1, unique (ItemType 4) and `Num % 10 >= 7` → 3
  - grade 0 → 0, except accessories (Kind 91–94, ItemType 4, upgradeable) → 8
- **This matches 85,510 of 85,917 of our rows (99.53%).** The residue is 300 low-grade uniques (ours has 3) and Beginner's weapons.
- For accessories, ours also sets `ItemExt` by Kind when the class is 8: 91→18, 92→19, 93→20 (36 rows 23), 94→21. ALPHA has `ItemExt = 0` on all of them.
- Predicted classes for the 35,861 ALPHA rows: 0: 15,644; 1: 2,558; 2: 4,305; 3: 8,292; 4: 4,742; 8: 320.
- **Recommendation:** compute ItemClass and ItemExt in the generator, not in SQL, and store the rule in the script header.

---

## 4. NPCs and monsters

### 4.1 Client id deltas [V]

**Npc_us: +74 new, −39 removed (vs old client).**
- Removed: 1311, 1312, 4062–4065, 5006–5012, 13017, 13018, 14436, 14437, 17010–12, 18011–18, 27001–10, 28055.
- 90 common NPCs were renamed (translation fixes).
- New ids:

| group | ids |
|---|---|
| prison gates | 4101–4103, 4201–4203 |
| innkeeper | 12001 |
| Prospective Knight Squad Captain Beldan | 14438 |
| Islante Sages Agatha/Pablo | 14439 / 24439 |
| village boards | 14440, 24440, 20004, 20005 |
| lockers and contribution stores | 18020–18029 |
| Witness Messenger | 18030 |
| **new-Moradon NPCs** | **19001 Laidis (El Morad Dispatcher), 19002 Berret (Middle man), 19003 Rage (Mercenary Adjutant), 19004 Osmoond (Mercenary Quartermaster), 19005 Mackin (Fortune Reader), 19006 Dueller (Clan Stadium Manager)** |
| mercenary NPCs | 19007–19016 |
| quest and townsfolk NPCs | 19017–19023, 19037, 19051–19066 |
| **training dummies** | **19067–19072 (Leather/Chain/Iron Scarecrow, two looks each)** |
| Legure (Prospective Knight Squad Captain) | 24438 |
| **Potuman (Karus Dispatcher)** | **29001** |
| Berit Kereve | 30005 |

- **Familiar Trainer Kate = 13016**, now "[Familiar Tamer] Kate". It was already in the old client but **is missing from our K_NPC**.
- **Knight Squad Secretary = 11610 Charel / 21610 Delaga**, renamed from "[Knight Clerk]". They are in our DB in zones 2 and 1, not in Moradon.
- Arena: 15000 [Arena]Ijin and 15002 [Coliseum]Artes are in ours. 19006 Dueller is new.

**Mob_us: +32 new, 0 removed.**

| group | ids |
|---|---|
| zone-71/21 monsters | 959 Antares, 1008 Hyde, 1056 Battalion, 1057 Rotten Eyes, 1058 undying, 1180 Skeleton |
| **scarecrows as monsters** | **4081–4083** |
| **promotion bosses** | **5701 Isiloon, 5702 Felankor** |
| others | 8052–8056, 8060 Blood Don, 8100–8106, 8110 summon bridge, 8111/8112/8161/8162 (merchants as mobs), 8201–8203 |

`MON.tbl` is a new copy of Mob_us minus 18 ids.

**Against the DBs:**
- **K_NPC:** 82 client NPC ids are missing in ours, and **all 82 are in ALPHA**.
  - 71 are new in 1534.
  - 11 were also in the old client: 7002, 13016, 13407, 13408, 14434, 16099, 18019, 24428, 24434, 24436, 24437.
  - ALPHA's K_NPC has 798 ids the client does not know. Copy only client-known ids.
- **K_MONSTER:** 65 client mob ids are missing in ours.
  - **60 are in ALPHA.**
  - 5 are in neither: 4061, 8111, 8112, 8161, 8162.
  - ALPHA has 636 ids the client does not know.
  - The 60 ALPHA monsters' `sItem` drop tables: 55 exist nowhere, 5 only in ALPHA. They would have no drops unless K_MONSTER_ITEM is extended.
- **Collisions (id with a different meaning):**
  - **Real:** our K_NPC **24438/24439/24440** = zone-64 "Warder 1 / Warder 2 / Keeper" (byType 11, placed at zone 64). In the 1534 client those ids are Legure (Knight promotion, ALPHA zone 11), Pablo and the Linnart board. **With the new client our zone-64 warders will show the wrong names.**
  - **Also:** 24441–24443 and 24527–24529 (our zone-64 NPCs) are unknown to both clients, so they have no names. This pre-dates U2.
  - **Name-only drift:** 16 K_NPC and 11 K_MONSTER rows, all translations or renames (e.g. 13015 Iris→Veronica, 12219 Wanigan→Supplies, 8009 Eidolon→Evil Wizard). The looks are the same.
- **Zone 71:** every Ronark Land spawn in ours is known to the new client.
- **NPC_Looks: +25 looks, 0 removed.** They include 4081–4083 and 19067–19069 (`npc_mora_training_*`), 19005 (fortune teller), 14439/14440/24440/20004/20005 (boards), 30001–30005 (Akara statue and priests) and 6700–6705 (dungeon summons).
- **Server code:** our server already has `NPC_SCARECROW = 171` (`shared/globals.h:154`, `Unit.cpp:1171`, `AIServer/Npc.h:149`), the byType that ALPHA's scarecrows use.

### 4.2 Knight-squad promotion (Caitharos, Isiloon, Felankor) [V]

- The new client's Quest_Talk explains the clan → Knight Squad promotion:
  - 4165/4265: "Caitharos is waiting to test you at the coliseum", Key of Honor
  - 6383: "Formal Knight Squad … Isiloon is waiting to test you"
  - 4637: Isiloon → Marble of the Dragon → Red Dragon
  - Texts 11607: "Caitharos Arena"
  - The old client had none of these names.
- Monsters:

| id | name | client | ours (placement) | ALPHA (placement) |
|---|---|---|---|---|
| 2690 | Caitharos | both | yes, zone 54 (119,128) | row only, **no spawn** |
| 5501/5551 | Isiloon | both | yes, zone 33 (5551) | yes, zone 33 |
| 5502/5552/5601/5651 | Servant of Isiloon | both | zone 33 | zone 33 |
| 7035 | Felankor | both | zone 34 (36,67) | row only, **no spawn** |
| **5701** | Isiloon (new) | new only | — | row only, no spawn |
| **5702** | Felankor (new) | new only | — | row only, no spawn |

- 5701 and 5702 are probably meant for the new client zones 930/940 (zones 93/94, dungeon_b2th/dragon_a). Both DBs have ZONE_INFO 93/94, but neither spawns anything there [A].
- ALPHA's 11610/21610 Lua call `PromoteKnight` (see B §4). ALPHA's `14438_Beldan.lua` is a 26-line level-70 stub, and its quest 703 has no kill logic. **No DB holds a working promotion chain.** It is a feature, not a migration item.

### 4.3 Zone 21 placement ALPHA vs ours [V] (`e_out/pos21.txt`)

- **Counts:**
  - ALPHA: 123 rows, 75 ids
  - ours: 138 rows, 58 ids
  - 40 ids are common, all at different coordinates, because the maps differ
- ALPHA-only ids:
  - 14 NPCs missing from our K_NPC: 13016, 19001–19006, 19067–19072, 29001
  - 3 NPCs we have but do not place in 21: 14401, 14402, 24414
  - 18 monster ids. 4 are missing from our K_MONSTER: 1056, 1057, 1058, 1180.
- Ours-only:
  - 6 NPCs: 521 castle-siege, 11020/21020 guards, 13003, 16073/16074 hero statues
  - 12 monsters
- ALPHA new-NPC spawn centres:

| id | NPC | ALPHA spawn |
|---|---|---|
| 13016 | Kate | (887,477) |
| 19001 | Laidis | (866,402) |
| 19002 | Berret | (393,550) |
| 19003 | Rage | (425,528) |
| 19004 | Osmoond | (359,535) |
| 19005 | Mackin | (855,593) |
| 19006 | Dueller | (719,428) |
| 29001 | Potuman | (788,369) |
| 24414 | Nameless warrior | (343,880) |
| 19067–19069 | scarecrows | (758–776, 416/422), 2 each |
| 19070–19072 | moving scarecrows | (758, 390–406), ActType 102 |

- **Client cross-checks:**
  - The client minimap (`NpcMopMap_info_us`, zone codes 30/31 = Moradon, 132 markers) has 93 markers within 20 m of an ALPHA spawn and 0 within 20 m of ours. 23 of the 29 NPC markers sit exactly (0 m) on an ALPHA spawn, among them 13016 Kate, 19002–19004, 19006 Dueller and 24414. The marker labels for 19001–19004 are shifted by one in the client data, and 19001/29001 are 146/102 m away from ALPHA's spawns.
  - `Zones\21.mob` (127 ids; old: 52) contains 74 of ALPHA's 75 ids. It also lists 19007–19066, which ALPHA does not place [A: the official spawn set was larger].
- **Shops:** the selling groups (`iSellingGroup / 1000`) 201/202/203/253/254/255 have ITEM rows in ours.
- **Prerequisites:** ZONE_INFO/START_POSITION 21 and the fixed `moradon_0826.smd` (B §3: transpose the heights and rebuild the event grid).

---

## 5. Zones (`Zones.tbl`) [V]

- The row key is zone × 10 (+ variant).
- 53 rows are byte-identical between the old and new clients. New rows:
  - 870 `In_dungeon05` (Juraid)
  - 930 `dungeon_b2th`
  - 940 `dragon_a`
- **Zone 21 → row 210 → `Zones\Moradon.gtd`.** The name is the same in both clients, but the new file is the 1024 m map (B §3). Both clients reference `Zones\21.mob`; the new file grew from 216 B to 516 B.
- **Zone 71 → row 710 → `Zones\Freezone_b.gtd`**, confirmed. It also references `.opdsub` and `Zones\71.mob`, unchanged at 292 B.
- Rows 850/860 point to `ch_qgs.*`, which is missing in both clients (B §5).

---

## 6. Quests

### 6.1 QUEST_HELPER [V] (`e_out/qhcmp*.txt`)

| pair | only left | only right | common | common but different |
|---|---|---|---|---|
| new client vs ALPHA | 0 | 0 | 2,924 | **0** |
| old client vs ours | 35 | 7 | 3,454 | **0** |
| new client vs ours | **156** | 693 | 2,768 | **963** |

**Missing in ours (156, all in ALPHA), by Lua file:**

| Lua | rows |
|---|---|
| 14301_Hapa | 60 |
| **19005_Mackin** | **15** (quests 974–980) |
| 13009_Kuger | 13 |
| Eload/Raek/Dela/Hwargo | 5 each |
| Patric | 4 |
| Skaky/Clarence/Menisia | 3 each |
| **14438_Beldan / 24438_Lugur** | **3 each** (quest 703, level 70) |
| Minerva | 2 |
| **11610_charel / 21610_delaga** | **2 each** (quests 953/954) |
| 14407_councilor | 2 |
| **19006_Dueler** | **1** |
| **13016_keite** | **1** |
| **29001_Potuman** | **1** |
| 18030_Move | 1 |
| 24414_Nameless | 1 |
| others | 1 each |

**nIndex reused for another NPC/Lua (552 rows):**

| ours → 1534 client | rows |
|---|---|
| 24406_Guardsman → **19003_Leizy** | 210 |
| 24427_Melburic → 19003 | 148 |
| 24435_Moradon → **19002_Burlet** | 78 |
| 24432_Kape → 19003 | 42 |
| 24430_Rucel → 19003 | 20 |
| Jalk/Melburic → **19004_Osmund** | 24 |
| 14434/24434_highprist → **19001_Lydiss / 29001_Potuman** | 4 |
| others | 26 |

- **All 534 rows of 19001–19004 collide with our live rows.** The remaining 411 differences are level, exp and talk-index edits on the same quests.
- **By quest id** (`sEventDataIndex`):
  - 24 quest ids are new: 703, 950, 953, 954, 957–962, 967–980
  - 47 of ours are gone: Jalk/Melburic/Guardsman/Nameless/Cheina/Teils 801–867, Nusram/Clasras 537–545, Prisia 574–578, …
- QUEST_HELPER and the Lua scripts are coupled through `SaveEvent(nIndex)`. The client uses its own Quest_Helper to draw the quest UI. **A row-level merge is unsafe.**
  - Option (a): replace QUEST_HELPER with ALPHA's (identical to the client) and adopt ALPHA's Lua for every affected NPC.
  - Option (b): keep ours and accept a mismatched quest UI.
- Either option is a quest-phase decision (U3), not U2.

### 6.2 QUEST_MONSTER [V]
- Ours and ALPHA have the same 150 ids (138 identical rows).
- The new client has 180 rows:
  - +36 ids: 939–942, 981–1012
  - −6: 558, 572, 926–929
- **No Quest_Helper row in the 1534 client references the 36 new ids.** Skip them.
- Of the 150 common rows, 39 are semantically different. In these the client lists only one nation's mob (e.g. 320: client 150, ours 100+150). Ours is a superset. Keep ours.

### 6.3 Quest_Talk / Quest_Menu (client-only text; our DB's QUEST_TALK and QUEST_MENU are not loaded by the server) [V]
- Quest_Talk: 0 removed, 696 added, 1,174 changed.
- Quest_Menu: 0 removed, 215 added, 9 changed (name spellings).
- **Our 114 Lua scripts** use:
  - 1,908 SelectMsg header ids: all still exist, **624 have changed text, 268 completely** (similarity < 0.5). Example: 8137 "By the way, I'll give you my best weapon…" → "You must be new to Moradon…".
  - 199 menu ids: all exist; Quest_Menu changes are cosmetic.
- Our quests will show wrong dialogue with the 1534 client. This cannot be fixed in the DB.

---

## 7. VERSION [V]

- OUR VERSION: 20 rows, 1454–1473 (`strFileName`, `strCompressName`, `sHistoryVersion`).
- ALPHA: 1 row, (1534, 1533, `Patch1534.zip`).
- LoginServer (`LogInServer/DBProcess.cpp:14-46`):
  - reads `sVersion, sHistoryVersion, strFilename`
  - `m_sLastVersion = MAX(sVersion)`, so it currently reports 1473
  - `HandlePatches` lists the files with `sVersion > client`
- The new `Server.ini` says `[Version] Files=1534` and `[IncludeExe] Last=1534`.
- **Needed:** one row `(sVersion=1534, strFileName='patch1534.zip', strCompressName='patch1534.zip', sHistoryVersion=1473)`. Then the server version equals the client version, and no patch is offered.
  - What the launcher does when the server reports 1473 < 1534 was not tested [A]. Adding the row removes that ambiguity.
- `GameServer` version 1534 and the JvCryption key are U1 (code), not U2.

---

## 8. Proposed U2 script set (numbering from **012**)

Shared rules for every script:
- Run with the servers stopped.
- `SET XACT_ABORT ON`.
- Write an ownership table (`<TABLE>_U2_ADDED`) or a backup table.
- Each has a `_rollback.sql`.
- Use no cross-instance queries. ALPHA is on a separate instance, so the rows must be exported by a generator in `tools/` into literal INSERTs or a bcp file.

| script | source | rows | idempotency key | rollback | risks / notes |
|---|---|---|---|---|---|
| **db/012_u2_capes_1534.sql** | client `Data/Cloak.tbl` (numeric columns); English placeholder names for the CP949 rows, because the server does not read `strName` | INSERT 168; optional UPDATE of 56 rows byRanking 0→2 | `NOT EXISTS (sCapeIndex)`; UPDATE only where byRanking = 0 | DELETE the 168 ids listed in `KNIGHTS_CAPE_U2_ADDED`; restore byRanking from `KNIGHTS_CAPE_U2_BACKUP` | Royal and accredited capes now cost clan points and need Flag ≥ 3/8–12; this is the intended 1534 behaviour. **Do not copy ALPHA's rows** (loyalty 0, wrong ranking, 164 → 22). Bot capes are unaffected. |
| **db/013_u2_items_1534.sql** (+ generator `tools/u2-gen-items.py`) | ALPHA ITEM rows that are absent in ours **and** resolvable by the new client; OUR 61-column list (drop ALPHA's UpgradeNotice/NPbuyPrice/Bound) | 35,860 (or a scoped subset: 19,418 under new bases, plus the 6 quest-reward ids) | `NOT EXISTS (Num)`; record in `ITEM_U2_ADDED(Num)` | DELETE ITEM by `ITEM_U2_ADDED` | No id collisions (Kind etc. agree 100%). ItemClass and accessory ItemExt come from the §3.4 rule (99.5%). ALPHA balance values (ReqLevel 60, etc.) are inconsistent with our ReqLevel-1 policy, which is an **owner decision** (keep ALPHA's or lower to 1). Bot gear is not touched. Size about 15–20 MB of SQL; prefer bcp into a staging table. ITEM grows +42% (memory). |
| **db/014_u2_npc_monster_1534.sql** | ALPHA K_NPC / K_MONSTER rows, **client-known ids only**; names from client Npc_us/Mob_us | K_NPC 79 (82 minus 24438/24439/24440); K_MONSTER 60 | `NOT EXISTS (sSid)`; `K_NPC_U2_ADDED` / `K_MONSTER_U2_ADDED` | DELETE by the ADDED tables | **Collision 24438–24440** (our zone-64 warders). **Owner decision:** skip these three (Legure/Pablo/board unavailable) or renumber our warders (K_NPC + K_NPCPOS zone 64). New monsters have no drop tables (sItem missing). Do not bulk-copy ALPHA (798 + 636 ids are unknown to the client). |
| **db/015_u2_moradon_1534.sql** | ALPHA ZONE_INFO 21, START_POSITION 21, K_NPCPOS zone 21 | 1 + 1 + 123 (replacing our 138) | backup `K_NPCPOS_Z21_U2_BACKUP` (138 rows) plus the old ZONE_INFO/START_POSITION rows; re-run deletes zone 21 and reinserts | restore zone 21 from the backups | **Depends on 014 and on the fixed `moradon_0826.smd`** (transposed heights, rebuilt event grid; B §3). Also re-target the zone-71/72 SMD Moradon warps (B §7), which is a map task. Only if the owner adopts new Moradon. |
| **db/016_u2_version_1534.sql** | constant | 1 | `NOT EXISTS (sVersion = 1534)` | `DELETE WHERE sVersion = 1534 AND strFileName = 'patch1534.zip'` | Old 1473 launchers would be offered `patch1534.zip`; acceptable because that client is retired. LoginServer reads VERSION at startup. |
| *(deferred to U3)* `db/017_u3_quests_1534.sql` | ALPHA QUEST_HELPER (= client) + ALPHA Lua | 2,924 replacing 3,461, or 156 additive rows | — | backup table | 552 nIndex collisions and 268 changed talk texts. Not a U2 copy. QUEST_MONSTER: keep ours. ITEM_EXCHANGE: the client table is the only source (ALPHA has 0 rows). |

Recommended order: 016 → 012 → 013 → 014 → (decision) 015.

All are additive except 015, and the 56-row byRanking update in 012 has no behavioural effect.

---

## 9. Open items / risks

- [A] The 16-byte `std::string` assumption behind the cape +0x24 offset. It is consistent with the code (the other reading makes it dead), but no live client test was done.
- [A] Launcher behaviour when the server version is lower than the client's. Moot once the 1534 row is added.
- [A] The intended spawns for 19007–19066 (in `21.mob`, not placed by ALPHA) and for 5701/5702 (zones 93/94).
- **Owner decisions:**
  - ITEM scope: all 35,860 rows, or new bases only
  - ReqLevel policy for new ITEM rows (ALPHA values or 1)
  - the 24438–24440 collision
  - adopting new Moradon at all (015, plus the map work)
  - the quest strategy (U3)
- Pre-existing, unrelated to U2:
  - zone-64 NPCs 24441–24443 and 24527–24529 have no client names
  - 2 removed scroll bases (379079000, 379152000) exist in our ITEM but are invisible in the new client
