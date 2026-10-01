# H — AIServer analysis for PK bots (commit 0f52027, __VERSION 1453)

Legend: **[V]** verified in code (file:function:lines) · **[I]** inferred from code · **[U]** unknown / needs runtime or DB check.
All paths relative to repo root. Line numbers refer to the file as checked out (CRLF, cp949 comments).

## 0. Summary

- [V] AIServer drives only `CNpc` objects. Each NPC has a K_MONSTER/K_NPC prototype, a NID ≥ 10000, and a coarse state machine ticked by 2 threads per zone. The tick is 250 ms; movement steps take 1500 ms (`MONSTER_SPEED`).
- [V] All combat math runs in **GameServer**. The AI only sends "attack X" (`AG_ATTACK_REQ`) or "cast skill N" (`AG_MAGIC_ATTACK_REQ`), and GameServer computes the damage.
- [V] The NPC info packet sent to clients has no race, class, equipment, clan or party fields. Parties, party skills and clans reject NPCs outright. PvP damage modifiers apply only when `isPlayer()`.
- [V] Chasing normally uses straight-line moves with **no obstacle check**. A* runs only for random wandering (`m_byMoveType==1`) and for `NPC_DUNGEON_MONSTER` chases. The A* uses unsorted-lookup linked lists, an odd heuristic, and the SMD event grid only (no height).
- Conclusion: NPC-based bots cannot meet the "looks/acts like a player, parties, PvP formulas" requirements without deep changes to both servers and the client protocol. Small pieces of AIServer can be reused in a GameServer-side bot: path finding (after a rewrite), surround slots, chase/give-up rules and the scheduler pattern.

---

## 1. AIServer process structure & timing

**main** — `AIServer/main.cpp:main` 11-50 [V]: starts the time thread (21), creates `CServerDlg` (23), calls `Startup()` (26), then blocks on a console/signal condition (31).

**Startup** — `AIServer/ServerDlg.cpp:CServerDlg::Startup` 53-113 [V]:
- Starts 2 timer threads (55-56): `Timer_CheckAliveTest` sends `AG_CHECK_ALIVE_REQ` every 10 s (592-600, 577-590). `Timer_CheckLiveTimes` runs every 1 s (602-631) and kills duration-limited event NPCs.
- Reads `AIServer.ini` for the ODBC DSN and listen port, default 10020 (834-842). Connects to the DB (70) and listens with `MAX_SOCKET`=100 (82; `Define.h` 6).
- Loads tables (88-105): MAGIC, MAGIC_TYPE1/2/4, NPC item / make-item tables, server resources, K_OBJECTPOS, K_MONSTER (`GetNpcTableData(false)`), K_NPC, then `MapFileLoad`, then `CreateNpcThread`. Table names are in `shared/database/NpcTableSet.h` 11 & 93, `NpcPosSet.h` 9-10, `ZoneInfoSet.h` 11 and `MagicTableSet.h` 9.
- `ResumeAI` (417-428) starts every NPC thread plus `ZoneEventThreadProc`.
- The GameServer listener (`GameServerAcceptThread` → `m_socketMgr.RunServer`, 684-687) starts only after the last NPC finishes its first `SetLive`. This call happens inside an NPC thread (`Npc.cpp:SetLive` 787-792). [V]

**Maps** — `ServerDlg.cpp:MapFileLoad` 430-458 iterates ZONE_INFO and calls `MAP::Initialize` (`MAP.cpp` 50-94) [V]. That function:
- loads the SMD through the shared `SMDFile::Load` (55);
- adds gate/lever/anvil/artifact object NPCs (60-72);
- allocates the region grid (74-76);
- loads `<n>.aievt` room events (79-91).

Region count is `mapwidth/48 + 1` (`shared/SMDFile.cpp:LoadMap` 89-90; `VIEW_DISTANCE` 48 in `shared/globals.h` 19; `VIEW_DIST` 48 in `AIServer/Define.h` 11).

**Spawns** — `ServerDlg.cpp:LoadSpawnCallback` 211-415 [V], one row of K_NPCPOS per call:
- `ActType<100` means a monster from K_MONSTER; otherwise an NPC from K_NPC with `MoveType = ActType-100` (268-277).
- `NumNPC` copies are placed at random points inside a rect (299-321).
- Regen time is `RegTime*SECOND` ms (323).
- Patrol points are parsed from the `path` string in 4-char X/Z pairs (334-353).
- Move type 2/3 with no points falls back to 1 (327-332).
- A trap-number filter spawns a row only if `TrapNumber==0` or it equals one static `myrand(1,4)`, with exceptions for some zones (221, 256-260).
- Every NPC gets `m_sNid = index + NPC_BAND(10000)` (`Npc.cpp:Load` 245).

**Threads** — `ServerDlg.cpp:CreateNpcThread` 181-209 [V]:
- For **each zone**, one `CNpcThread` holds all spawn-table NPCs of that zone (192-204), and a second `CNpcThread` holds event NPCs (194).
- Event NPCs are added at runtime by `SpawnEventNpc` (745-798 → `AddNPC` 782).
- Thread count = 2 × (number of zones in ZONE_INFO) + 1 zone-event thread + 2 timers + time thread + socket worker/cleanup threads (`shared/SocketMgr.cpp:SpawnWorkerThreads` 45-55, which starts **one** IOCP worker). [V] The number of zones is [U] (DB).

**Tick loop** — `AIServer/NpcThread.cpp:NpcThreadProc` 7-158 [V]:
- `#define DELAY 250` (5) and `sleep(DELAY)` at the end of every pass over the thread's NPCs (150).
- Per NPC: if `m_Delay > now - m_fDelayTime`, the NPC is still waiting (54-69). During that wait, a STANDING NPC whose region holds users (`CheckFindEnemy`, `Npc.cpp` 3464-3480) still runs `FindEnemy()`. On success it jumps straight to `NPC_ATTACKING` with delay 0 (61-67).
- HP regen: every 10 000 ms → `HpChange()` adds `MaxHP/20` (71-73; `Npc.cpp` 3453-3462).
- State dispatch (75-129). The handler's return value becomes the next `m_Delay` (136-137), and `m_fDelayTime` is refreshed (132-134).
- The local array `CNpc *pNpcList[32768]` (17) is rebuilt whenever the global `m_TotalNPC` changes (39-40, 21-31). That happens on any spawn.
- `ZoneEventThreadProc` 160-188 runs `CRoomEvent::MainRoom` every 1000 ms (185).

**Timing constants** [V]:

| Constant | Value | Source |
|---|---|---|
| Thread pass | 250 ms | `NpcThread.cpp` 5 |
| Movement / chase step | `m_sSpeed` = `MONSTER_SPEED` 1500 ms (not loaded from DB); objects use 1000 | `NpcTable.h` 3, 63; `Npc.cpp:Load` 274-278 |
| Walk / run distance per step | `bySpeed1/2 × m_sSpeed/1000` m | `Npc.cpp` 280-283 |
| Attack delay | K_MONSTER `sAttackDelay` | `Npc.cpp` 273 |
| Stand time | K_MONSTER `sStandtime` | 286 |
| Faint | 2 s | 29 |
| Event-NPC default regen | `160*MINUTE` = 9600 (ms units, since `MINUTE`=60u) | 299; `globals.h` 25 |
| Cast time | `bCastTime * MINUTE` ms | `NpcMagicProcess.cpp` 29 |

DB values are [U].

---

## 2. NPC state machine (`AIServer/Npc.cpp`, enum `shared/globals.h` 43-60)

**States [V]:** DEAD, LIVE, ATTACKING, ATTACKED (unused), ESCAPE (unused), STANDING, MOVING, TRACING, FIGHTING, STRATEGY (no-op case, `NpcThread.cpp` 107-108), BACK, SLEEPING, FAINTING, HEALING, CASTING.

**Transitions [V]:**
- **LIVE** (`NpcLive` 317-328): `SetLive` 642-812 resets HP/MP, target and path, picks a random point inside the init rect (706-752) and calls `SendNpcInfo` (810) → STANDING with delay `m_sStandTime`. Event NPCs that die are deleted instead of respawned (666-671).
- **STANDING** (`NpcStanding` 533-588): if `RandomMove()` succeeds → MOVING; otherwise stay. Aggro comes from the thread pre-check (NpcThread 61-67).
- **MOVING** (`NpcMoving` 473-531): `FindEnemy()` → ATTACKING (494-508). End of path or step failure → STANDING (510-527). Otherwise step and return `m_sSpeed`.
- **ATTACKING** (`NpcAttacking` 428-471):
  - target within melee general range → FIGHTING (442-447);
  - `GetTargetPath()==-1` → drop the target and RandomMove → MOVING or STANDING (449-461);
  - result 0 → straight line (`IsNoPathFind`, 463-467);
  - otherwise → TRACING (469).
- **TRACING** (`NpcTracing` 330-426):
  - `m_byMoveType==4` (towers) → FIGHTING (354-358);
  - in range → FIGHTING (360-379);
  - target invalid → STANDING;
  - target moved → `ResetPath` (395-405), which on failure → STANDING;
  - otherwise step and `SendMoveResult`; in attack range during the chase also `TracingAttack()` (420-423); returns `m_sSpeed`.
- **FIGHTING** → `Attack()` (2142-2256):
  - out of range → TRACING with `ATTACK_TO_TRACE` (2161-2178);
  - `m_byDirectAttack==1` → `LongAndMagicAttack` (2157-2158, 2265-2324);
  - healer + friendly NPC target → HEALING (2245-2249);
  - GM target → MOVING (2203-2207).
- **HEALING** (`NpcHealing` 3787-3854) casts `m_iMagic3` on the most-damaged friendly NPC (≤90 % HP).
- **CASTING** (`NpcCasting` 3856-3875) sends `MAGIC_EFFECTING`, then restores `m_OldNpcState`.
- **FAINTING** (`RecvAttackReq` 2722-2727, `NpcFainting` 3777-3785) lasts 2 s.
- **DEAD**: `Dead()` 1280-1308 sets `m_Delay = m_sRegenTime`; the thread then moves it to LIVE (NpcThread 110-112).
- **BACK is dead code:** `NpcBack` (590-640) exists, but no code assigns `NPC_BACK`. Likewise `RandomBackMove` (1024-1153), `MoveAttack` (2350-2424) and `IsChangePath` (2426-2442) are never called (repo grep). SLEEPING is unreachable because its entry is commented out (535-539).

**Target acquisition** — `FindEnemy` 1336-1435 [V]:
- Healers first look for hurt friends (1355-1360).
- Users are scanned in the NPC's region plus up to 3 neighbours picked by `FindEnemyRegion` (1438-1564). Bug: `iCurSY`/`iCurEY` use `GetX()` instead of `GetZ()` (1452, 1454).
- `FindEnemyExpand` (1566-1657) skips users who are dead, `!CanAttack`, invisible or GM (1590-1595).
- Selection rule: `if (fDis > range || fDis < fComp) continue;` (1598-1600). This keeps the **farthest** user within `m_bySearchRange`. The Korean comment at 1388 says "closest", so the code does not match the comment; intent is [I].
- Aggressive (`ATROCITY`) NPCs lock on immediately. Passive (`TENDER`, actType 1/2, 765-770) NPCs lock only on users in their damage list (1606-1615).
- Only guards scan for NPC targets (1408-1429).

**Re-targeting when hit** — `ChangeTarget` 2487-2591 [V]. A random roll 0-100 decides:
- <50: keep the old target if its preview damage to the NPC is higher (2519-2526);
- 50-79: keep whichever target is closer (2527-2537);
- 80-94: compare the NPC's own preview damage on each (2538-2544);
- ≥95: switch.

After that: passive states → FIGHTING if within `2×attackRange` (`IsCloseTarget(CUser*)` 2871-2885), otherwise TRACING via `GetTargetPath(1)` (2558-2586).

**Chase limits / give-up** — `GetTargetPath` 1970-2140 [V]:
- The window is `m_bySearchRange+2`, or `m_byTracingRange` if the user has damaged the NPC (2009-2011).
- Chases started by an attack: give up if the target is ≥ `NPC_MAX_MOVE_RANGE` = 100 m away (2001-2007; `Define.h` 50).
- If the NPC has strayed from its tracing start point by more than the range → drop the target (2014-2019).
- Target outside the window → −1 (2054-2059). Distance > range → −1 (2114-2117).
- Straight-line fallback also aborts beyond 100 m (`IsNoPathFind` 3300-3306).

**Return home** [V]: there is no BACK state. After losing the target the NPC goes STANDING. `RandomMove` (814-1022) cycles `m_iPattenFrame`. Frame 0 targets `m_nInitX/Z` (865-869); the other frames are ±4 m jitter (840-889; `m_pPattenPos` is always 0 per constructor 144). If home is >100 m away, `RandomMove` fails (966-977), so [I] the NPC stays where it is until it aggros again. Wandering happens only while a user is within 100 m (`GetUserInView` 3163-3187, called at 821).

**Attack timing** [V]:
- Melee: `SendAttackRequest` then return `m_sAttackDelay` (2232, 2255).
- Proto `m_byMagicAttack` 4/5: 10 % chance of area magic, delay `m_sAttackDelay+1000` (2210-2218).
- Proto `m_byMagicAttack` 2: 10 % chance of a single-target spell (2220-2228).
- Ranged/magic NPCs (`m_byDirectAttack==1`) `MAGIC_CASTING` `m_iMagic1` (2309).
- Range tests subtract `m_fBulk`: melee "general" range = 3 m + bulk (`SHORT_ATTACK_RANGE` 24, 1953/1961); long range 30 m (23, 1946).

**Movement** [V]:
- A* path path: `StepMove` (1718-1813) advances `m_fSecForMetor` metres (walk = `m_fSpeed_1` set at 816; run = `m_fSpeed_2` at 1980, 2478) toward the next waypoint each tick.
- Straight-line path: `StepNoPathMove` (1815-1862) jumps to the stored point.
- `IsNoPathFind` stores only the end point: its loop `while (fDis <= fDistance)` exits after one pass (3316-3332). So [V] a chasing NPC is moved to the end point (2 m + bulk from the target, `CalcAdaptivePosition` 2077, 3219-3225) in **one** step. The client is told speed = distance / 1.5 s (418, 529). [I] Visually the NPC runs at whatever speed covers the gap in one step; actual look is [U] (runtime).
- Server position is applied one tick later (340-341, 1803-1810).
- Each step: `SendMoveResult` → `MOVE_RESULT` (309-315).

**Unreachable targets / stuck** [V]:
- For non-dungeon monsters that have a target, `GetTargetPath` returns 0 **without** A* (2120-2122). `IsNoPathFind` never calls `IsMovable`, so chases ignore walls and blocked cells.
- When A* fails (`PathFind` returns 0, 1257-1258), callers also fall back to `IsNoPathFind` (463-466, 2476-2480, 2578-2583).
- `NPC_DUNGEON_MONSTER` is clamped to its limit rect (1913-1915, 2130-2132, 962-964).
- No stuck detection exists (no position-unchanged counter; grep for "stuck" finds nothing).

**Group behaviour** [V]:
- actType 3/4 sets `m_bHasFriends` (771-776).
- When hit, `FindFriend` (2887-2933) and `FindFriendRegion` (2935-3015) copy the target to same-family NPCs (`m_byFamilyType`) within `m_byTracingRange` (2951, 2982-2994). Bosses use `MonSearchAny` (2589-2590). Helpers are put into TRACING (`NpcStrategy` 3017-3027).
- Melee NPCs claim one of 8 slots, 45° apart, around a user at 2 m + bulk (`IsSurround` 1689-1702 → `AIServer/AIUser.cpp:IsSurroundCheck` 71-121; offsets `GetTargetPath` 2066-2079).
- **`AIServer/Party.cpp` is not monster grouping.** It mirrors **player** parties sent by GameServer through `AG_USER_PARTY` (create/insert/remove/delete, 18-132). The mirror is used to merge party damage when handing out EXP (`SendExpToUserList` 2764-2869).

---

## 3. PathFind.cpp + MAP.cpp

- **API** [V]: `_PathNode* CPathFind::FindPath(start_x, start_y, dest_x, dest_y)` (`PathFind.cpp` 66-100), after `SetMap(w, h, MAP*, min_x, min_y)` (57-64). `CNpc::PathFind` calls it **reversed**, `FindPath(end, start)` (`Npc.cpp` 1243-1244), then walks `Parent` links to get start→end waypoints. Each tile becomes `tile*4 + m_fAdd`, and the last point is the exact end point (1246-1275). There is one `CPathFind` member per NPC (`Npc.h` 209).
- **Grid** [V]: tiles of `TILE_SIZE`=4 m (`Define.h` 21), hard-coded rather than read from the SMD `m_fUnitDist` (`SMDFile.h` 63-64). A per-tile mismatch on maps with another unit size is [I].
- **Blocked cells** [V]: `CPathFind::IsBlankMap` → `MAP::IsMovable` → `SMDFile::GetEventID(x,z) == 0` (`PathFind.cpp` 335-338; `MAP.cpp` 124-127; `SMDFile.cpp` 200-206; out of bounds = −1, so blocked). This is the SMD event-tile grid only. Heights (`m_fHeight`, `SMDFile.cpp` 110-111) are private, and `MAP::GetHeight` is declared but never defined (`MAP.h` 55). The N3Shape collision data is loaded (`SMDFile.cpp` 81-85) but `CN3ShapeMgr` has no query method (`N3ShapeMgr.h` 93-115). So there is **no height or LOS data** in AI pathing. [V]
- **Algorithm** [V]:
  - 8-neighbour expansion (119-146), with diagonal cost 11 and orthogonal cost 10. The macro names are swapped: `LEVEL_TWO_FIND_CROSS`=11 is used for diagonals (6-9, 123-145).
  - Heuristic: Euclidean for the start node (78); for children `h = max(x-dx, y-dy)` **signed, without abs** (196). It is ~10× smaller than g and can be negative, so [I] the search behaves close to Dijkstra and is not a true admissible A*.
  - Open list is a sorted singly-linked list (`Insert` 247-268). `CheckOpen`/`CheckClosed` are linear scans (212-245) → O(n²).
  - `PropagateDown` uses +1 instead of the real step cost (270-309).
  - Iteration cap `2 × (|dx|·w + |dy|·h + 1)` (84-90). `IsBlankMap` does not clamp to the window, so the search may leave it. `calloc` per node, freed on the next call (26-55).
- **Search window** [V]:
  - `RandomMove`: `dist+2` m around the NPC (993-997). Bug: line 997 checks `min_z` instead of `max_z`.
  - `GetTargetPath`: search/tracing range (2043-2046), same bug at 2046.
  - A path of ≥ `MAX_PATH_LINE`=100 nodes is rejected (`Npc.cpp` 1257; `Define.h` 7).
- `IsPathFindCheck` (3227-3283) returns false as soon as the start tile is *movable* (3242-3243). The semantics look inverted, so [I] A* always runs for walkable starts.

---

## 4. NpcMagicProcess & damage authority

- [V] `CNpcMagicProcess::MagicPacket` (`NpcMagicProcess.cpp` 6-35):
  - looks the skill up in the same MAGIC table the AI loads (8-10; `ServerDlg.cpp` 115-118);
  - sends `AG_MAGIC_ATTACK_REQ` {opcode, skillID, caster, target, 3×data} (12-14);
  - for `MAGIC_CASTING` it puts the NPC in `NPC_CASTING` with cast time `bCastTime*60` ms (19-31), then `NpcCasting` sends `MAGIC_EFFECTING`.
- NPC skills come only from proto `iMagic1`/`iMagic3` (`Npc.cpp` 2215, 2225, 2309, 3839, 3852). There is no class rotation. [V]
- [V] GameServer runs these through the **full player magic pipeline**: `AISocket.cpp` 34-35 → `CMagicProcess::MagicPacket` (`GameServer/MagicProcess.cpp` 16-53) with `pCaster=nullptr` → `MagicInstance::Run` (10-147).
  - NPC casters skip class/level/mana/zone checks (`UserCanCast` 181-200).
  - Types 5, 6 and 9 require a player caster (1896, 2063, 2459).
  - Some self-buff types are skipped for NPC casters (1706-1715).
  - Packet reads past the end return 0 (`ByteBuffer.h` 110-116), so `sData[3..6]=0`.
- [V] **NPC → user physical damage**: AI sends `AG_ATTACK_REQ` (`Npc.cpp` 2258-2263). GameServer `RecvNpcAttack` (`AISocket.cpp` 237-275) computes `CNpc::GetDamage(CUser*)` (`GameServer/Unit.cpp` 491-537: HitB from `m_sTotalHit` vs AC, capped at 2.6×Hit), calls `HpChange`, and broadcasts `WIZ_ATTACK`. **No range check on the GameServer side** [V], so AI range is trusted.
- [V] **User → NPC**: `CUser::Attack` (`AttackHandler.cpp` 4-93) → `CUser::GetDamage` (`Unit.cpp` 208-394) → `CNpc::HpChange` (`GameServer/Npc.cpp` 199-224) → `AG_NPC_HP_CHANGE` to AI (240-245) → AI `RecvNpcHpChange` (`GameSocket.cpp` 362-383) → `RecvAttackReq` (`Npc.cpp` 2665-2733: damage list, faint roll, `ChangeTarget`). Death is decided on the AI side: `HpChange` → `Dead()` → `AG_DEAD` (`Npc.cpp` 2760-2761; `Unit.cpp:SendDeathAnimation` 962-966) → GameServer `RecvNpcDead` → `OnDeath` (`AISocket.cpp` 495-509).
- [V] The AI keeps its own copy of user buffs only through `AISkillOpcodeBuff/RemoveBuff` (`MagicProcess.cpp` 85-107, 622-628).

---

## 5. GameServer ↔ AIServer protocol (`shared/packets.h` 704-776)

Transport [V]:
- **One TCP connection**. GameServer is the client (`InitSessions(1)`, `GameServerDlg.cpp` 104-105, 778-805) and shares the user IOCP port.
- Reconnect check runs every 6 s and fires once more than 3 alive-checks are missed (`GameServerDlg.cpp` 714-726).
- AI socket buffers are 256 KB (`AIServer/GameSocket.h` 14).
- Handlers: AI `CGameSocket::HandlePacket` (`GameSocket.cpp` 29-101); GameServer `CAISocket::HandlePacket` (`AISocket.cpp` 8-79).

| Opcode | Dir | Purpose (sender → handler) |
|---|---|---|
| AI_SERVER_CONNECT 1 | GS→AI, AI→GS | handshake (`GameServerDlg.cpp` 799-801 → `GameSocket.cpp` 103-118 → echo → `AISocket.cpp` 98-108, which calls `SendAllUserInfo`) |
| NPC_INFO_ALL 2 | AI→GS | 20 NPCs per compressed packet (`ServerDlg.cpp` 482-518 → `AISocket.cpp` 144-214) |
| MOVE_RESULT 4 | AI→GS | NPC step (`Npc.cpp` 309-315 → `AISocket.cpp` 216-235 → `CNpc::MoveResult` `Npc.cpp`(GS) 73-82: sets pos immediately, `WIZ_NPC_MOVE`) |
| MOVE_END_RESULT 6 | — | ignored (`AISocket.cpp` 27-28) |
| AG_NPC_INFO 7 | AI→GS | (re)spawn info (`Npc.cpp` 83-89, 810 → `AISocket.cpp` 277-342) |
| AG_NPC_GIVE_ITEM 8 | AI→GS | loot (`Npc.cpp` ~3412 → `AISocket.cpp` 383-447) |
| AG_NPC_GATE_OPEN 9 | both | gates (`Npc.cpp` 571-582; GS `Npc.cpp` 186-188; `AISocket.cpp` 665-690; `GameSocket.cpp` 416-440) |
| AG_NPC_GATE_DESTORY 10, AG_NPC_EVENT_ITEM 12 | AI→GS | handled by GS (`AISocket.cpp` 480-492, 651-663); no AI sender found |
| AG_NPC_INOUT 11 | AI→GS | handler 511-521; AI `SendInOut` (`Npc.cpp` 76-81) is never called |
| AG_NPC_HP_REQ 13 | GS→AI | sent at `AISocket.cpp` 229-231; **no AI handler** |
| AG_NPC_SPAWN_REQ 14 | GS→AI | spawn by sid/zone/pos/count/radius/duration/nation/owner socket (`GameServerDlg.cpp` 577-590 → `GameSocket.cpp` 523-548 → `ServerDlg.cpp` 745-798) |
| AG_NPC_REGION_UPDATE 15 | AI→GS | region change (`Npc.cpp` 59-71, 96-101 → `AISocket.cpp` 344-357) |
| AG_NPC_UPDATE 16 | GS→AI | change proto group/pid (`GameServerDlg.cpp` 599-604 → `ServerDlg.cpp` 810-827) |
| AG_NPC_KILL_REQ 17 | GS→AI | kill a NID, or all NPCs owned by a user ID (`GameSocket.cpp` 550-577) |
| AG_SERVER_INFO 50 | AI→GS | per-zone NPC count (`ServerDlg.cpp` 520-522 → `AISocket.cpp` 110-142) |
| AG_ATTACK_REQ 51 | AI→GS | NPC melee (see §4) |
| AG_DEAD 53 | AI→GS | NPC death |
| AG_SYSTEM_MSG 54 | AI→GS | broadcast chat (`ServerDlg.cpp` 844-849 → `AISocket.cpp` 371-381) |
| AG_CHECK_ALIVE_REQ 55 | both | keepalive, every 10 s (`ServerDlg.cpp` 577-600; `AISocket.cpp` 473-478) |
| AG_ZONE_CHANGE 57 | GS→AI | user zone (`CharacterMovementHandler.cpp` 486-488 → `GameSocket.cpp` 394-407) |
| AG_MAGIC_ATTACK_REQ 58 | both | AI→GS: NPC cast; GS→AI: buff/unbuff sync (`MagicProcess.cpp` 55-64) |
| AG_USER_INFO_ALL 60 / AG_PARTY_INFO_ALL 62 | GS→AI | full resync on connect (`GameServerDlg.cpp` 1661-1701; `GameSocket.cpp` 409-414, 455-472) |
| AG_HEAL_MAGIC 63 | GS→AI | AI handler makes nearby NPCs aggro on the healer (`AIUser.cpp` 123-175); **GameServer never sends it** |
| AG_TIME_WEATHER 64 | GS→AI | night flag (`GameServerDlg.cpp` 1174-1176 → `GameSocket.cpp` 486-494) |
| AG_BATTLE_EVENT 65 | both | war events (`GameSocket.cpp` 496-521; `Npc.cpp` 2837-2868; `AISocket.cpp` 523-648) |
| AG_COMPRESSED 66 | AI→GS | lzf wrapper (= `WIZ_COMPRESS_PACKET` 0x42; `AISocket.cpp` 692-716) |
| AG_USER_INFO 101 / AG_USER_UPDATE 109 | GS→AI | user stats (`User.cpp:Send2AI_UserUpdateInfo` 2068-2073, `GetUserInfoForAI` 3019-3041) |
| AG_USER_INOUT 102 | GS→AI | region in/out (`CharacterMovementHandler.cpp` 90-96 → `GameSocket.cpp` 182-243) |
| AG_USER_MOVE 103 | GS→AI | every client `WIZ_MOVE` (`CharacterMovementHandler.cpp` 51-53 → `GameSocket.cpp` 245-318) |
| AG_USER_SET_HP 105 | GS→AI | every user HP change (`User.cpp` 1965-1970 → `GameSocket.cpp` 345-360) |
| AG_USER_LOG_OUT 106 / AG_USER_REGENE 107 | GS→AI | `User.cpp` 871-873; `AttackHandler.cpp` 202-207 |
| AG_USER_EXP 108 | AI→GS | kill reward per user/party with K_MONSTER `iExp`/`iLoyalty` (`Npc.cpp` 2817-2827 → `AISocket.cpp` 359-369) |
| AG_USER_PARTY 111 | GS→AI | party mirror (`PartyHandler.cpp` 149-151, 263-265, 326-334, 404-406, 435-441 → `Party.cpp`) |
| AG_USER_VISIBILITY 112 | GS→AI | invisibility (`User.cpp` 4496 → `GameSocket.cpp` 442-453) |
| AG_NPC_HP_CHANGE 113 | both | NPC HP delta (GS `Npc.cpp` 240-245; AI `Npc.cpp` 2753-2758) |

**Mirrored user data** [V]:
- AI `CUser` (`AIServer/User.h` 34-60) is filled by `ReadUserInfo` (`GameSocket.cpp` 144-180) with: name, zone, nation, level, HP, MP, TotalHit, AttackAmount, TotalAc, ACAmount, hit rate, evasion rate, ItemAc, party number, authority, invisibility, and per-slot equipped item bonuses.
- `m_sMaxHP` is never set (stays 0, `AIUser.cpp` 25). There is no class/race/skill state.
- Position: on `AG_USER_MOVE` with speed≠0, AI `cur` = previous `will` and `will` = the new position (`GameSocket.cpp` 294-303). So the AI's current user position lags **one move packet**. [V]
- Stats are resent only on login (`User.cpp` 1019), level-up (1800), item changes (`ItemHandler.cpp` 738), some ability recomputes (`User.cpp` 2311, 4140) and a type-4 buff path (`MagicInstance.cpp` 1850).

**Sync implications**:
- [V] GameServer moves the NPC to the step's end position immediately (`Npc.cpp`(GS) 77), while the AI applies it on the next tick. So the two servers disagree by up to one step (≤1.5 s).
- [V] NPC death needs a round trip: GS HP→0 → AI `Dead` → `AG_DEAD`.
- [I] Chase reaction is ≥250 ms (thread pass) + 1500 ms (step), plus the one-packet user-position lag. That is too coarse for PvP-grade kiting.
- [V] `SendAllUserInfo` only iterates **socket sessions** (`GameServerDlg.cpp` 1668-1678).

---

## 6. RoomEvent.cpp (brief)

[V] Dungeon "room" scripts are loaded from `./map/<n>.aievt` (`MAP.cpp:LoadRoomEvent` 197-339) for zones whose ZONE_INFO `byRoomEvent>0` (79-91).
- Conditions (`CRoomEvent::CheckEvent` 63-118): kill a specific monster, kill all, survive N minutes, reach a goal rect, kill N of a sid.
- Actions (`RunEvent` 120-198): spawn a monster, open a door, transform a monster, spawn N monsters.
- Driven by `ZoneEventThreadProc` every 1 s and by user position (`GameSocket.cpp:SetUid` 315 → `MAP::IsRoomCheck` 341-392).
- Not relevant to Ronark Land unless that zone has a room event configured [U] (DB).

---

## 7. NPC-based bot evaluation — evidence & blockers

1. **Rendering as a player — blocked.**
   - [V] Clients receive NPCs through `WIZ_NPC_INOUT` + `CNpc::GetNpcInfo` (`GameServer/Npc.cpp` 90-99, 138-155): proto ID, picture ID `m_sPid`, type, size, weapon1/2, name, nation (0 if monster), level, position, gate flag, object type, direction.
   - [V] Players use `WIZ_USER_INOUT` + `CUser::GetUserInfo` (`CharacterMovementHandler.cpp` 63-69, 99-161): nation, clan/alliance/cape, level, **race, class**, face, hair, party-leader flag, invisibility, ranks, and **10 equipment slots**.
   - [U] Whether some K_NPC `sPid` renders a human-looking model is a client-data question. `AG_NPC_UPDATE` changes only the AI proto (`ServerDlg.cpp` 810-827) and does not re-broadcast to GameServer.
2. **Party membership / party chat — blocked.**
   - [V] `_PARTY_GROUP.uid[8]` holds user IDs (`structs.h` 291-311). Party handlers resolve them through `GetUserPtr` (`PartyHandler.cpp` 91, 188, 228, …), which is `m_socketMgr[id]` (`GameServerDlg.cpp` 549; `KOSocketMgr.h` 52-61).
   - [V] `MORAL_PARTY` explicitly fails for NPC casters or targets (`MagicInstance.cpp` 826-831); clan checks likewise (875-880).
   - [V] The AI party mirror covers users only (`Party.cpp`).
3. **Player skills.**
   - [V] The AI only fires proto `iMagic1`/`iMagic3` (§4).
   - [V] GameServer `CNpc::CastSkill` (`Npc.cpp` 269-284) can run any MAGIC entry through `MagicInstance`, with relaxed checks.
   - [V] But type 5/6/9 and some type-4 self buffs are refused for NPC casters (§4).
   - [V] NPCs have no MP: `GetMana()` returns 0 (`Npc.h` 84-85) and `MSpChange` is `#if 0` (`Npc.cpp` 252-267).
4. **Receiving priest heals/buffs.**
   - [V] Type-3 heals on NPC targets are allowed (`MagicInstance.cpp:CheckType3Prerequisites` 483-500). The HP flows GS→AI as a positive `AG_NPC_HP_CHANGE` (`Npc.cpp`(GS) 226-238; `GameSocket.cpp` 377-382).
   - [V] Type-4 prerequisites pass for NPC targets except HP/MP buffs with `sMaxHPPct==99` (548-556).
   - [V] AI movement ignores speed buffs: no `m_bSpeedAmount` use anywhere in `AIServer/*.cpp`, and speed is fixed at `Npc.cpp` 280-283.
5. **PvP formulas — not applied.**
   - [V] PvP AC/AP class bonuses, item procs, `GetACDamage` and the ÷2 apply only if `pTarget->isPlayer()` (`Unit.cpp` 225-230, 245-258, 376-387; `MagicInstance.cpp` 1077-1078).
   - [V] NPC attacks use the separate monster formula (`Unit.cpp` 491-537).
   - [V] A kill rewards K_MONSTER exp/loyalty (`Npc.cpp` 2821-2827), not PvP NP (`User.cpp:LoyaltyDivide` 3068).
   - [V] PK-zone logic is user-only (`User.h:isInPKZone` 376).
6. **Hostility bug.** [V] GameServer `CNpc::isHostileTo` does `TO_USER(pTarget)->GetClanID()` for **any** target (`Unit.cpp` 1180-1190). [I] This is undefined behaviour when the target is an NPC (bot vs guard/monster), and `pKnights` may be uninitialised during siege.
7. **Movement / AI quality.** [V] 1.5 s steps, straight-line chase through obstacles, no stuck detection, dead BACK state, and the farthest-target selection quirk (§2).
8. **Partly usable.** [V] An event-spawn API with nation and owner socket exists (`GameServerDlg.cpp` 577-590). Owners are excluded from `TracingAttack` (`Npc.cpp` 2337). Event NPCs are deleted on death (666-671).

---

## 8. Reusable pieces for a GameServer-side ("player-like") bot

- **Path finding** [V]: `CPathFind` depends only on `MAP::IsMovable` → `SMDFile::GetEventID`, which GameServer already loads and uses (`GameServer/Map.cpp` 27, 109). It is portable, but [I] needs:
  - `abs()` in the heuristic (196) and a cost-consistent heuristic;
  - a binary heap or hash instead of the linear lists (212-268);
  - clamping `IsBlankMap` to the window;
  - a node pool instead of per-node `calloc`;
  - one instance per bot or per thread (it keeps member state).
- **Waypoint follower**: the `PathFind` → `m_pPoint[]` conversion (`Npc.cpp` 1238-1277) and `StepMove` (1718-1813), "advance N m per tick along waypoints".
- **Geometry helpers**: `GetVectorPosition`, `CalcAdaptivePosition`, `ComputeDestPos`, `GetDistance`, `Yaw2D` (3149-3161, 3219-3225, 3443-3451, 3425-3441).
- **Melee spreading**: 8-slot surround allocation (`AIUser.cpp` 71-121; `Npc.cpp` 2066-2079).
- **Targeting / chase heuristics** (after fixing the bugs above): the region-scan pattern (1566-1657); `ChangeTarget` weighting by preview damage vs distance (2487-2591); chase anchored at its start point with a range give-up (2014-2019) and a 100 m cap.
- **Scheduler pattern**: per-entity `m_Delay` / `m_fDelayTime` with handlers that return the next delay (`NpcThread.cpp` 54-137).
- **Thread-model differences**:
  - [V] AI: 2 threads per zone with no per-NPC lock. The socket thread mutates NPC state (`RecvAttackReq` → `ChangeTarget`, `Npc.cpp` 2665-2733) concurrently; only the damage list is locked (2676).
  - [V] AI: `pNpcList` is built from `m_pNpcs` without `m_lock` (`NpcThread.cpp` 23-31), while `AddNPC` locks (191-195).
  - [V] GameServer: one IOCP worker thread for all client **and** AI packets (`SocketMgr.cpp` 45-55; `GameServerDlg.cpp` 104), plus timer threads (376-380).
  - [V] `Timer_UpdateSessions` runs `CUser::Update` and timeout disconnects every **30 s** (728-757).
  - [I] A GameServer bot loop needs its own thread with locking, or must be posted onto the worker.
- **Things GameServer bots must supply themselves** [V]:
  - A session object: `GetUserPtr` = `m_socketMgr[id]`.
  - Protection from the timeout disconnect (`GameServerDlg.cpp` 737-749).
  - AI mirroring via `AG_USER_INFO/INOUT/MOVE` (`SendAllUserInfo` only walks sessions).
  - Collision: server movement is checked against map bounds only (`CharacterMovementHandler.cpp` 21; `SMDFile.cpp` 194-198).
  - Height: no accessor exists (`SMDFile.h` 59 is private).
  - Attack/magic entry points: these are packet handlers keyed on the socket ID (`AttackHandler.cpp` 4-93).

---

## 9. Performance

- **NPC count** [U]: printed at startup (`ServerDlg.cpp` 207) and depends on K_NPCPOS. Hard limits [V]: 32 768 per thread (`NpcThread.cpp` 17, a 128 KB stack array on Win32 [I]); uint16 NIDs offset by 10 000 (`Npc.cpp` 245); `Atomic<uint16> m_TotalNPC` (`ServerDlg.h` 126).
- **Per-pass cost** [V]: every 250 ms, every STANDING NPC whose region holds a user runs `FindEnemy`. That scans users in ≤4 regions of 48 m. Each candidate does `GetUserPtr` under `m_userLock` (`ServerDlg.cpp` 539-547) while the zone-wide `pMap->m_lock` is held (`Npc.cpp` 1580-1589). So cost is O(NPCs_near_users × users_nearby) per pass, contending with `RegionUserAdd/Remove` (`MAP.cpp` 129-162) from the socket thread [I].
- **Other scans** [V]:
  - `GetUserInView` scans regions within 100 m (≈5×5 [I]) on every RandomMove (3163-3187).
  - `FindFriend` scans regions within `m_bySearchRange` (2896-2913).
  - A* is O(n²) (§3).
  - Full O(N) passes over all NPCs: `RecvBattleEvent` (`GameSocket.cpp` 509-520), owner-kill (557-569), `AllNpcInfo` on connect (`ServerDlg.cpp` 486-512).
  - Any spawn forces every NPC thread to rebuild its list (`NpcThread.cpp` 39-40).
- **Network** [V]: every NPC step produces one `MOVE_RESULT`, then a GS `WIZ_NPC_MOVE` `SendToRegion` (`Npc.cpp`(GS) 80-81). Every user move produces one `AG_USER_MOVE`.
- [U] Ronark Land map size, region count, NPC density and real CPU/latency all need a runtime test (map files are not in the repo).

---

## 10. Pros / cons (strictly from evidence)

| | NPC-based bots (AIServer `CNpc`) | Player-like GameServer bots (`CUser`) |
|---|---|---|
| Looks like a player (race/class/gear/clan) | ✗ NPC packet lacks these fields (`Npc.cpp`(GS) 138-155) | ✓ `GetUserInfo` (`CharacterMovementHandler.cpp` 99-161) |
| Party, party skills, party chat | ✗ `MagicInstance.cpp` 826-831; party = user IDs | ✓ existing `PartyHandler` / `MORAL_PARTY` path |
| PvP damage formulas, NP/PK logic | ✗ only for `isPlayer()` (`Unit.cpp` 245-258, 376-387); monster formula 491-537 | ✓ automatic |
| Player skills (all types), MP | partial: `CastSkill` exists; types 5/6/9 and MP missing | ✓ same pipeline as humans (needs internal call path) |
| Existing AI brain (aggro, chase, assist, timers, threads) | ✓ ready (`Npc.cpp`, `NpcThread.cpp`), but coarse: 1.5 s steps, straight chase through walls, BACK dead | ✗ none in GameServer; must be written (can port §8 pieces) |
| Spawn/despawn API | ✓ `AG_NPC_SPAWN_REQ` / `KILL_REQ` | ✗ needs a fake session (`GetUserPtr` = socket map) and timeout bypass |
| Monsters/guards react to the bot | ✗ only guards target NPCs (`Npc.cpp` 1408-1429); `isHostileTo` UB risk | ✓ if mirrored to AI via `AG_USER_*` (must be added, since sessions only) |
| Pathing / collision | A* exists (event grid only); chase skips it | must port A*; server has no collision or height query |
| Threading risk | unsynchronised NPC state between NPC threads and socket thread | one IOCP worker + timers; bot thread must synchronise |

## 11. Runtime / DB checks still needed [U]
- Number of zones and NPCs (startup log), and Ronark Land K_NPCPOS rows.
- K_MONSTER `sAttackDelay`, `bySpeed1/2` and ranges for the candidate templates.
- How the client renders an NPC whose `sPid` points at a human model.
- Ronark Land map size (region count).
- Actual client-visible chase smoothness with one-step straight moves.
- The real `WIZ_MOVE` frequency from clients (drives the AI position lag).
