# A_core: GameServer core architecture and bot-integration seams

Repo: Fire-Drake-Project-v1453 @ 0f520272 (main). `shared/version.h:3` sets `__VERSION 1453`. All paths are relative to the repo root.
Tags: **[V]** verified in code, **[I]** inferred from code, **[U]** unknown or needs a runtime test.

---

## 1. Process startup and main loops

**main()** (`GameServer/main.cpp:13-60`) [V]
- Calls StartTimeThread (23), StartConsoleInputThread (26), `new CGameServerDlg` (28), `Startup()` (31) and `ResetBattleZone()` (34).
- The main thread then blocks on `s_hEvent.Wait()` (40) until the console Ctrl handler (62-69) signals it.

**CGameServerDlg::Startup** (`GameServerDlg.cpp:76-208`) [V], in order:
1. GetTimeFromIni (86). This reads the INI and **also starts the 5 timer threads** (376-380). The timers therefore start before the DB and tables load. Possible early-run race [I].
2. `m_socketMgr.Listen(port, MAX_USER)` (97).
3. The AI socket manager reuses the same IOCP and gets 1 session (103-105).
4. `g_DBAgent.Startup` (107-109). DBAgent.cpp:37 starts the DatabaseThread here.
5. About 45 table loaders (110-154). Table names come from `shared/database/*Set.h` GetTableName: ITEM, SET_ITEM, ITEM_EXCHANGE, ITEM_UPGRADE, ITEM_OP, SERVER_RESOURCE, QUEST_HELPER, QUEST_MONSTER, MAGIC, MAGIC_TYPE1..9, K_OBJECTPOS, RENTAL_ITEM, COEFFICIENT, LEVEL_UP, KNIGHTS*, START_POSITION(_RANDOM), BATTLE, ZONE_INFO+EVENT, KING_SYSTEM, EVENT_TRIGGER, MONSTER_SUMMON/RESPAWN lists, PREMIUM_ITEM(_EXP), USER_DAILY_OP, USER_ITEMS, and others.
6. `MapFileLoad` (142): reads ZONE_INFO and loads one SMD per zone into C3DMap (`LoadServerData.cpp:381-405`).
7. `ClearRemainUsers` (159), which calls the CLEAR_REMAIN_USERS proc (`DBAgent.cpp:1920`).
8. Opens 4 log files under ./Logs (162-190).
9. LoadNoticeData (193).
10. `m_luaEngine.Initialise()` (196).
11. AIServerConnect (199).
12. InitServerCommands and InitChatCommands (202-203).
13. `m_socketMgr.RunServer()` (205).

**Threads** [V]

| Thread | Where | Period | Work |
|---|---|---|---|
| TimeThread | shared/TimeThread.cpp:11-16, 26-43 | 1000 ms | Updates global `UNIXTIME` and `g_localTime`. Resolution is **1 s**. |
| ConsoleInputThread | ConsoleInputThread.cpp:7-10, 20-56 | polls every 100 ms | Runs `/` commands through HandleConsoleCommand. |
| Timer_CheckGameEvents | GameServerDlg.cpp:376, 631-641 | 1 s | BattleZoneOpenTimer, TempleEventTimer, ForgettenTempleEventTimer |
| Timer_BifrostTime | :377, 643-712 | 60 s | Bifrost state machine. Calls KickOutZoneUsers (700). |
| Timer_UpdateGameTime | :378, 714-726 | 6 s | UpdateGameTime (1109-1177): King timers; every minute, rank reload (every 30 min, Define.h:185) and PK-zone ranking rewards; every hour, weather and rank reset; every month, NP reset; AG_TIME_WEATHER to the AI server. Reconnects to the AI server when `m_sErrorSocketCount > 3`. |
| Timer_UpdateSessions | :379, 728-758 | 30 s | Session timeout, then `CUser::Update()` for in-game users. |
| Timer_UpdateConcurrent | :380, 760-776 | 60 s | Sends a WIZ_ZONE_CONCURRENT DB request. |
| IOCP worker (**one**) | SocketMgr.cpp:45-55 (single `new Thread`), loop 58-89 | event-driven | All client and AI-socket reads, then HandlePacket. |
| Accept | ListenSocketWin32.h:64-102 | blocking | AssignSocket, then Accept. |
| SocketCleanupThread | SocketMgr.cpp:11-28 | 100 ms | Moves disconnected sessions from active to idle. |
| DatabaseThread | DatabaseThread.cpp:18-21, 31-154 | condition-signalled | Serial DB queue. |

**Per-user periodic logic: `CUser::Update()`** (`User.cpp:472-533`) [V]
- Restores saved magic 2 s after GameStart (474-480).
- HP/MP regen: HPTimeChange runs when `UNIXTIME - m_tHPLastTimeNormal > m_bHPIntervalNormal` (482-483).
  - The interval is 5 s (User.cpp:117).
  - Formulas are at 3325-3376: standing regenerates MP only; sitting regenerates HP and MP.
- DOT/HOT: HPTimeChangeType3 (486-487; 3378-3441).
- Type4 buff expiry (490; 3443-3460). It expires **at most one buff per call** (3455).
- CheckSavedMagic, then type6 (transform) and type9 (stealth) expiry (493-499).
- Blink end check (501-502).
  - BLINK_TIME is 15 s (Define.h:61).
  - BlinkStart returns immediately in zones where you can attack the other nation (User.cpp:4529-4530). So there is no respawn blink in Ronark Land.
  - Blinking is disabled entirely in DEBUG builds (GameServer/stdafx.h:7-9).
- Rival expiry (504-505).
- Auto-save every `PLAYER_SAVE_INTERVAL` = 180 s (User.h:21; User.cpp:507-511).
- Item-expiry scan over the inventory and the 192 warehouse slots (512-532).

Who calls Update():
- After **every** handled packet, on the IOCP thread (User.cpp:465).
- Every 30 s from Timer_UpdateSessions (GameServerDlg.cpp:752-753).

So regen and buff-expiry granularity is packet-driven [V]. A socketless bot would only get an Update every 30 s unless the bot manager calls it [I].

**Timeouts** [V]
- `KOSOCKET_TIMEOUT` is 30 s. `KOSOCKET_LOADING_TIMEOUT` is 30 min, used after authentication but before entering the game (KOSocket.h:12-16).
- Enforced in GameServerDlg.cpp:737-750, only when `DEBUG` is not defined. Debug builds auto-define DEBUG (shared/stdafx.h:19-34).
- `m_lastResponse` is refreshed on every valid packet (KOSocket.cpp:88).

**Speedhack checks** [V] (both on the IOCP thread; GMs exempt)
- `SpeedHackUser`, called from MoveProcess (CharacterMovementHandler.cpp:18-19; User.cpp:2862-2881).
  - Speed limits: 67 for warrior/mage/priest, 90 for rogue or captain, 45 otherwise.
  - Over the limit: Disconnect, a server-wide notice, and a cheat-log entry.
- `SpeedHackTime`, on the client's WIZ_SPEEDHACK_CHECK (User.cpp:3581-3607).
  - Compares squared distance / 100 since the last check against speed+10.
  - If exceeded, warps the player back.

**AFK check:** not found. Grepped `afk|idle|AutoPlay`; the only hits are AI idle-session comments at GameServerDlg.cpp:780-787.

---

## 2. Socket/thread model and locks

**Class chain** [V]: `Socket` (shared/Socket.h:4) → `KOSocket` (KOSocket.h:18) → `CUser : public Unit, public KOSocket` (User.h:110).

**Session manager** [V]
- `KOSocketMgr<CUser> m_socketMgr` (GameServerDlg.h:535) pre-allocates MAX_USER = 3000 CUser objects. Their IDs are 0..2999 and they start in `m_idleSessions` (KOSocketMgr.h:75-80, 100; globals.h:9).
- `SessionMap` is `std::map<uint16, KOSocket*>` (KOSocketMgr.h:8).
- On accept, AssignSocket moves the first idle session to active (KOSocketMgr.h:105-119).
- On disconnect, SocketMgr::OnDisconnect pushes to a queue (SocketMgr.cpp:145-149). The cleanup thread then calls DisconnectCallback, which moves the session from active back to idle (KOSocketMgr.h:133-143).

**Exactly one IOCP worker thread** [V]
- `SpawnWorkerThreads` creates a single thread (SocketMgr.cpp:51).
- The AI socket uses the same completion port (GameServerDlg.cpp:103-104; Socket.cpp:39). `m_aiSocketMgr` never calls RunServer; its only references are GameServerDlg.cpp:104-105, 781, 1106 and 3139.
- So all client packet handlers **and** all AI-server packets (NPC moves and attacks, AISocket.cpp:8-79) run serially on that one thread.
- Read path: HandleReadComplete → KOSocket::OnRead → CUser::HandlePacket (SocketMgr.cpp:107-124; KOSocket.cpp:25-105; User.cpp:217-467).

**User state is still touched from other threads** [V]. There is no per-CUser mutex.
- Timer_UpdateSessions calls `Update()` (GameServerDlg.cpp:753).
- DatabaseThread runs ReqAccountLogIn, ReqSelectCharacter → `SelectCharacter`, and ReqUserLogOut (DatabaseThread.cpp:157-268, 437-462).
- Timer threads kick users out of zones (GameServerDlg.cpp:700) and send loyalty rewards (3349-3370).
- The console runs /kill and /down (ChatHandler.cpp:541-563, 911-916).

**Locks** [V]. All are `std::recursive_mutex` taken through the `Guard` RAII class (shared/stdafx.h:57-66). There is no FastMutex anywhere (grep found none). RWLock is used only by Lua (LuaEngine.h:67).
- **Session map:** `KOSocketMgr::m_lock` (KOSocketMgr.h:68) is taken in `operator[]` (52-61), AssignSocket, OnConnect, DisconnectCallback and SendAll.
  - However, 24 call sites copy `GetActiveSessionMap()` (which returns a reference, :49) **without the lock**. Examples: GameServerDlg.cpp:732, 858, 895, 2942, 3351.
  - That is a racy map copy [V for the missing lock; I for the crash risk].
- **Name maps:** `m_accountNameLock` and `m_characterNameLock` (GameServerDlg.h:517-522; used at 453-547).
- **Regions:** the code takes `C3DMap::m_lock` (Map.h:44) and then `CRegion::m_lock` (Region.h:18) in Send_UnitRegion, GetRegionUserIn and similar (GameServerDlg.cpp:929-934, 1352-1357). CRegion::Add/Remove also lock (Region.cpp:11-26). Regions store unit **IDs** as `std::set<uint16>` (Region.h:9-10).
- **NPC, party and clan arrays:** CSTLMap holds a mutex only for the duration of each call (STLMap.h:11-57). Returned pointers are then used unlocked. `_PARTY_GROUP::uid[]` is modified with no lock (PartyHandler.cpp:221).
- **Per-unit locks:** `m_buffLock` (Unit.h:322), `m_equippedItemBonusLock` (Unit.h:279), `m_arrowLock` (User.h:185), `m_savedMagicLock` (User.h:1066).
- **Socket writes:** `m_writeMutex` via BurstBegin/BurstEnd (Socket.h:39-48; KOSocket.cpp:175-189). Calling `Send()` from any thread is safe.

**What this means for a bot manager** [I]
- A bot thread that calls CUser handlers directly would race with the IOCP thread on the same objects. Example: a player hits a bot, so HpChange runs on the IOCP thread while the bot thread is casting.
- The same kind of race already exists with the 30 s timer and the DB thread, but bot ticks would be 100-250 ms.
- **Lowest-risk seam:** run bot decisions on their own thread, but execute the resulting actions on the IOCP worker.
  - Add a custom `SocketIOEvent` (SocketDefines.h:3-9) and dispatch it at SocketMgr.cpp:84-85.
  - Trigger it with `PostQueuedCompletionStatus`, which the code already uses for shutdown (SocketMgr.cpp:151-154).
  - This serializes bot actions with all player and AI packet handling.
- The alternative is a new `g_timerThreads` entry (GameServerDlg.cpp:376-380). It carries the same raciness as the existing timers.

---

## 3. CUser (User.h / User.cpp)

**Identity** [V]
- `GetID()` returns `GetSocketID()` (User.h:113). The user ID **is** the session index.
- Names: `m_strUserID` and `m_strAccountID` (116-118).

**Combat and state members** (User.h) [V]
- Race `m_bRace` (120); class `m_sClass` (121). `GetClassType` is the class modulo 100 (414-417), and the base-class table is at 393-407.
- Hair and face (123, 130); `m_iExp` (127); loyalty and monthly loyalty (128); clan `m_bKnights` (132).
- `m_sHp`, `m_sMp`, `m_sSp` (134); `m_bStats[STAT_COUNT]` (135); `m_bAuthority` (136).
- `m_bstrSkill[10]` skill points (141). Categories are in GameDefine.h:178-182: `SkillPointFree` = 0 through `SkillPointMaster` = 8.
- `m_sItemArray[INVENTORY_TOTAL]` (142). SLOT_MAX is 14 equipped slots and HAVE_MAX is 28 bag slots (globals.h:226-227).
- Cooldown maps: `m_CoolDownList`, `m_MagicTypeCooldownList`, `m_RHitRepeatList` (176-182).
- Flying arrows (184-185) and transformation state (187-190).
- `m_iMaxHp`, `m_iMaxMp` (236).
- `m_bResHpType` (238): USER_STANDING = 1, SITDOWN = 2, DEAD = 3 (GameDefine.h:117-119).
- `m_bWarp` (239).
- Party: `m_sPartyIndex`, `m_bInParty`, `m_bPartyLeader` (242-244).
- Stealth: `m_bCanSeeStealth`, `m_bInvisibilityType` (246-247).
- Regen fields (257-261); blink expiry `m_tBlinkExpiryTime` (268); `m_bAbnormalType` (270).
- `m_state` (1056), which is either `GAME_STATE_CONNECTED` or `GAME_STATE_INGAME` (31-35).
- Position, zone, nation, level and buffs live in Unit (see §5).
- The current target ID is stored in the socket class, `KOSocket::m_targetID` (KOSocket.h:25, 44). It is set by WIZ_TARGET_HP (User.cpp:326-333).

**Useful predicates** [V]
- `isDead()`: `m_bResHpType == USER_DEAD || m_sHp <= 0` (User.h:308).
- `isInPKZone()`: zones 72, 71 and 73 (User.h:376).

**Send paths** [V]
- `KOSocket::Send` is virtual (KOSocket.h:33; KOSocket.cpp:140-193). It returns false when `!IsConnected()`.
- `SendCompressed` is virtual and uses LZF for packets of 500 bytes or more (KOSocket.cpp:195-220).
- `CUser::SendToRegion` (User.cpp:4710-4713) → `Send_Region` → Send_UnitRegion over the 3×3 regions → each user's Send (GameServerDlg.cpp:918-954).
- `SendToZone` (User.cpp:4722-4725) → `Send_Zone`, which iterates the whole session map (GameServerDlg.cpp:856-882).

**Lookups** [V]
- By ID: `GetUserPtr(uint16)` is `m_socketMgr[id]` and only finds **active** sessions (GameServerDlg.cpp:549; KOSocketMgr.h:52-61).
- By name: `GetUserPtr(name, TYPE_ACCOUNT | TYPE_CHARACTER)` over upper-cased maps (GameServerDlg.cpp:453-474).
  - AddAccountName runs on DB login success (DatabaseThread.cpp:165).
  - AddCharacterName runs in SendMyInfo (User.cpp:1016).
  - Both are removed in OnDisconnect (User.cpp:178; GameServerDlg.cpp:529-547).
- `GetUnitPtr`: IDs below `NPC_BAND` (10000) are users, everything else is an NPC (GameServerDlg.cpp:557-563; Define.h:67).

---

## 4. Full user lifecycle

Most steps need a real socket. Where a step also needs a database row, it says so.

1. **Accept.** ListenSocketWin32.h:82-99 → AssignSocket → `Socket::Accept` / `_OnConnect` (Socket.cpp:44-67) → `CUser::OnConnect` → `Initialize` (User.cpp:19-169). Runs on the accept thread.
2. **WIZ_VERSION_CHECK.** `VersionCheck` sends the version and a 64-bit public key, then calls EnableCrypto (LoginHandler.cpp:3-19). Until crypto is on, this is the only opcode accepted (User.cpp:223-229).
3. **WIZ_LOGIN.** `LoginProcess` (LoginHandler.cpp:21-55):
   - kicks a duplicate login (41-45);
   - sets `m_strAccountID`;
   - queues a DB request: ReqAccountLogIn (DatabaseThread.cpp:157-172) → `{? = CALL GAME_LOGIN(?, ?)}` (DBAgent.cpp:96) → AddAccountName.
   - Needs an **account row**.
4. **Character list and creation.**
   - SEL_NATION → NATION_SELECT (DBAgent.cpp:112).
   - ALLCHAR_INFO_REQ → selects ACCOUNT_CHAR and USERDATA (126, 161).
   - NEW_CHAR → CREATE_NEW_CHAR (200).
5. **WIZ_SEL_CHAR.** `SelCharToAgent` (CharacterSelectionHandler.cpp:102-132) kicks a duplicate character (118-128), then on the DB thread `ReqSelectCharacter` (DatabaseThread.cpp:247-268) runs:
   - `LOAD_USER_DATA` (DBAgent.cpp:324-545; proc call at 340). This also loads rental data and sealed items (426-427). Needs a **USERDATA row**.
   - The WAREHOUSE select (546-564). It **returns false if there is no row**, which fails character select.
   - LOAD_PREMIUM_SERVICE_USER (626-647) and USER_SAVED_MAGIC (649-684).
   - Then **`SelectCharacter` runs on the DB thread** (CharacterSelectionHandler.cpp:134-223):
     - ban check;
     - server-change redirect (155-163);
     - relog restrictions for PK and war zones (168-187);
     - `SetLogInInfoToDB` → SET_LOGIN_INFO, which uses `GetRemoteIP()` (233-247; DBAgent.cpp:721-738). If that fails, the user is disconnected (DatabaseThread.cpp:1002-1003);
     - `m_bSelectedCharacter = true` (192), `SetUserAbility(false)` (195), `SetRegion` (204).
6. **WIZ_GAMESTART**, two client-driven steps.
   - **Opcode 1** (CharacterSelectionHandler.cpp:264-278):
     - SendMyInfo (User.cpp:883-1020): builds WIZ_MYINFO, calls AddCharacterName (1016) and `Send2AI_UserUpdateInfo(true)` (1019).
     - Then UserInOutForMe, MerchantUserInOutForMe, NpcInOutForMe, notice, time and weather.
   - **Opcode 2** (279-323):
     - `m_state = INGAME` (281);
     - `UserInOut(INOUT_RESPAWN)` (282);
     - BlinkStart and SetUserAbility (307-308);
     - sets the saved-magic timer (319) and `m_tHPLastTimeNormal` (334).
7. **How other players see a player.** `UserInOut` (CharacterMovementHandler.cpp:71-97):
   - adds or removes the user in its CRegion;
   - sends **WIZ_USER_INOUT** to the 3×3 regions, excluding the user;
   - sends AG_USER_INOUT to the AI server.
   - `GetInOut` (63-69) writes a uint16 type, the ID, and **GetUserInfo** (99-161):
     - name, nation, clan, fame, and clan data (alliance, name, grade, mark, cape);
     - level, **race, class**, position, **face, hair**, resHpType, abnormal type, need-party, authority, party-leader flag, invisibility, helmet flag, direction, chicken flag, king flag, NP ranks;
     - **10 equipped slots** (BREAST, LEG, HEAD, GLOVE, FOOT, SHOULDER, RIGHTHAND, LEFTHAND, CTOP, CHELMET), each as item, durability and flag;
     - zone.
   - Bulk forms: WIZ_REQ_USERIN (GameServerDlg.cpp:1307-1376; User.cpp:1200-1223) and WIZ_REGIONCHANGE ID lists (1327-1345).
   - When a move crosses a region boundary: RegisterRegion → Remove/InsertRegion (Unit.cpp:160-193) and Send_Old/NewRegions (GameServerDlg.cpp:957-1004).
8. **Zone change.** `ZoneChange` (CharacterMovementHandler.cpp:309-500):
   - Checks `CanChangeZone` (174-293). Ronark Land requires loyalty > 0 and no active war other than Ardream (260-273).
   - Then UserInOut(OUT), removes the user from the party (417-423), sets `m_bWarp = true` (341), updates map and position, sends WIZ_ZONE_CHANGE teleport (469-471), runs InitType3/4 (480-482) and sends AG_ZONE_CHANGE.
   - **Completion is client-driven.** `RecvZoneChange` handles Loading and then Loaded (668-699); only then does the user get `UserInOut(RESPAWN)`, RecastSavedMagic and `m_bWarp = false`.
   - A same-zone change becomes `Warp` (626-659).
9. **Death and respawn.**
   - `CUser::OnDeath` (User.cpp:4727-4950) when the killer is a player: kill count and rank (4801-4802), `LoyaltyChange` or `LoyaltyDivide` for parties (4811-4868), `GoldChange` (4871), rival and anger gauge (4834-4886), and a zone-wide `SendDeathNotice` (4945; ChatHandler.cpp:341).
   - Respawn needs the client's WIZ_REGENE → `Regene` (AttackHandler.cpp:95-239). In a PK zone, a player with 0 loyalty is kicked out (233-238).
10. **Logout or disconnect.**
    - `Socket::Disconnect` (Socket.cpp:97-120) does nothing when not connected (99-100).
    - Otherwise `CUser::OnDisconnect` (User.cpp:174-208): RemoveSessionNames, UserInOut(OUT), party, clan, ResetWindows, rival.
    - Then `LogOut` (866-878): AG_USER_LOG_OUT, `m_deleted = true`, and a WIZ_LOGOUT DB request.
    - `ReqUserLogOut` (DatabaseThread.cpp:437-462) runs UPDATE_USER_DATA (DBAgent.cpp:916-988), the WAREHOUSE update, UPDATE_SAVED_MAGIC and ACCOUNT_LOGOUT.
    - The cleanup thread frees the slot (SocketMgr.cpp:11-28).
    - **Possible race [I/U]:** the DB dispatcher looks the user up in the *active* map (DatabaseThread.cpp:66-72), but the session leaves that map within about 100 ms. If the DB queue is backed up, the logout save could be skipped silently.
11. **Saving.**
    - Every 180 s from Update → UserDataSaveToAgent → ReqSaveCharacter (User.cpp:507-511, 853-861; DatabaseThread.cpp:464-472).
    - On a server change (CharacterMovementHandler.cpp:460).
    - On `/down`, which saves synchronously (GameServerDlg.cpp:2938-2956).

**Strict requirements** [V]

DB rows:
- An account that GAME_LOGIN accepts.
- A USERDATA row (LOAD_USER_DATA).
- A WAREHOUSE row.
- SET_LOGIN_INFO must return non-zero.
- Premium, saved-magic and skill-shortcut rows are optional.
- An ACCOUNT_CHAR entry is needed only for the character list.

Socket-dependent:
- every Send;
- the timeout;
- `GetRemoteIP`;
- logout driven by Disconnect;
- the client-driven steps: GAMESTART 1/2, zone-change Loaded, REGENE and SPEEDHACK_CHECK.

---

## 5. Unit base class (Unit.h:39-342)

The class is shared with the AI server through `#ifdef` (Unit.h:3-7; Unit.cpp:3-10).

**Fields** [V]
- Position `m_curx/z/y`, `m_bZone`, `m_pMap`, `m_pRegion`, region IDs (215-223).
- Level and nation (227-228).
- Totals: hit, AC, hitrate, evasion (230-233).
- Buff-adjusted amounts (235-244); elemental resistances (247-256); weapon resistances (282-289).
- DOT slots `m_durationalSkills[40]` (291-318).
- **`m_buffMap` (Type4), `m_type9BuffMap`, `m_buffLock`, `m_buffCount`** (320-323).
- State flags: blind, canUseSkills/Potions/Teleport/Stealth, instantCast, block/reflect curses, mirror, undead, kaul, blockPhysical/Magic (325-338).
- Owner `m_oSocketID` and `m_bEventRoom` (340-341).

**Pure virtuals** [V]: GetID, GetName, GetHealth/MaxHealth/Mana/MaxMana, isDead, GetInOut, AddToRegion, **GetDamage**, **HpChange**, **MSpChange**, **isHostileTo** (54, 90, 95-98, 158, 174, 177, 182, 194, 196, 206).

**Virtuals with defaults** [V]
- OnAttack/OnDefend (183-184), HpChangeMagic (195), saved-magic hooks (190-192), StateChangeServerDirect (205).
- **CanAttack** (Unit.cpp:855-875): same zone, not incapacitated, target not dead or blinking, and isHostileTo.
- **isAttackable** (888-924): monuments and gates.
- **CanCastRHit** (926-947): looks the *attacker* up in the session map.
- isSameEventRoom.

**Non-virtual helpers** [V]
- `OnDeath` / `SendDeathAnimation` send WIZ_DEAD to the region (956-968).
- `SendToRegion` (807-810).
- `GetDistance` returns *squared* 2D distance; `isInRange` expects a squared range (84-151).
- `isInAttackRange` (User.cpp:5013+): melee base 15, ranged 65, plus the weapon's `m_sRange`.

**Hostility and Ronark Land** [V]
- `CUser::isHostileTo` (Unit.cpp:1219-1256): opposite nation inside a PVP zone is hostile. Same nation is never hostile, except in arena, temple-event zones and siege.
- Ronark Land has only the `ZF_ATTACK_OTHER_NATION` flag and minimum level 35 (Unit.cpp:1100-1104; Define.h:161).
- **So an 8v8 must be Karus vs El Morad.**

---

## 6. GameServer CNpc vs AIServer NPC

**The GameServer CNpc is a mirror** (Npc.h:6-116) [V]
- Created and updated only from AI packets NPC_INFO_ALL and AG_NPC_INFO (AISocket.cpp:144-214, 277-342).
- Moved by MOVE_RESULT → `CNpc::MoveResult` → WIZ_NPC_MOVE (AISocket.cpp:216-235; Npc.cpp:73-82).
- AG_ATTACK_REQ is applied in the GameServer, which computes the damage with `CNpc::GetDamage` (AISocket.cpp:237-275; Unit.cpp:474-590).
- All NPCs are deleted when the AI link drops (AISocket.cpp:91-96).
- Spawning goes through `SpawnEventNpc` → AG_NPC_SPAWN_REQ (GameServerDlg.cpp:577-590).

**The AI server owns behavior** [V]
- `NpcThreadProc` ticks every 250 ms (AIServer/NpcThread.cpp:5, 150).
- It holds the FSM (NpcStanding/Moving/Tracing/Attacking), FindEnemy and PathFind (AIServer/Npc.h:363-425).

**How NPCs are rendered** [V]
- WIZ_NPC_INOUT (Npc.cpp:90-99) carries **GetNpcInfo** (138-155): protoID, **m_sPid (model)**, type, selling group, size, weapon1, weapon2, name, nation (0 for monsters), level, position, gate flag, object type, 2× unknown uint16, direction.
- There are **no race, class, face, hair or armor slots**. So an NPC cannot be rendered as an equipped player through this packet.
- Whether some client model ID (m_sPid) looks like a player is unknown [U].
- Player visuals require WIZ_USER_INOUT and GetUserInfo (§4.7).

NPCHandler.cpp only covers user-to-NPC interactions (repair :8, ClientEvent :72, ClassChange :156, NpcEvent :347, ItemTrade :491). It has no NPC AI.

---

## 7. Existing bot, fake-player, auto-play or offline-merchant code

**Not found.**

Search method: `grep -a`, case-insensitive, over *.cpp and *.h in GameServer, AIServer, LogInServer and shared.

| Pattern | Result |
|---|---|
| `bot`, `bots`, `BOT`, `isBot\|m_bIsBot\|IsBot` | none |
| `fake` | only the fake weather packet (GameServerDlg.cpp:1235-1249) |
| `AutoPlay\|autoplay\|AutoHunt\|AutoAttack` | none |
| `offline` | only the clan "is offline" message (Knights.cpp:145, 161) and a KingSystem comment (:1255) |
| `AI_` | only AI-server plumbing |
| `afk`, `idle` | only the AI idle-session comments |
| `pseudo`, `dummy` | unrelated |

No offline merchant exists: the merchant is closed through ResetWindows on disconnect (User.cpp:202). The repo also contains no *.lua quest scripts, *.ini or *.sql files.

---

## 8. GM and console commands

**In-game GM chat commands**
- Prefix `+` (ChatHandler.h:90). Only GMs (`isGM`) can use them (ChatHandler.cpp:109). The table is at ChatHandler.cpp:53-82.

| Command | What it does | Lines |
|---|---|---|
| `+give_item Name ItemID [cnt]` | GiveItem | 380-422 |
| `+zonechange Zone` | Teleport to the zone's START_POSITION; no free x/z | 424-448 |
| `+monsummon SID`, `+npcsummon SID` | Spawn at the GM's position | 450-484 |
| `+monkill` | Kill the currently targeted NPC | 486-504 |
| `+np_change Name ±NP` | Change a player's NP | 622-651 |
| `+exp_change Name ±EXP` | Change a player's EXP; levels up through ExpChange | 653-682 |
| `+gold_change` | Change a player's gold | 684-718 |
| `+np_add`, `+exp_add`, `+money_add %` | Server-wide event bonuses | 720-790 |
| `+tp_all Zone [Target]` | KickOutZoneUsers | 815-845 |
| `+summonknights Clan` | Pull a clan's members | 847-887 |
| `+resetranking Zone` | Reset player ranking | 889-909 |
| `+open1..6`, `+snowopen`, `+close`, `+captain`, `+warresult` | War control | 565-616, 936-976 |
| `+permitconnect`, `+test` | Unban / no-op | — |

**Client GM packet:** WIZ_OPERATOR → `OperatorCommand` (User.cpp:3480-3579).
- ARREST: go to a user. SUMMON: pull a user to the GM. CUTOFF: disconnect.
- BAN, MUTE/UNMUTE, DISABLE/ENABLE_ATTACK.
- WIZ_WARP to coordinates is allowed for GMs only (User.cpp:313-316).

**Console commands**
- Prefix `/` (ChatHandler.h:91). Table at ChatHandler.cpp:12-44; dispatched by GameServerDlg.cpp:1609-1622.

| Command | What it does | Lines |
|---|---|---|
| `/notice` | Server-wide notice | — |
| `/kill Name` | Disconnect a player | 541-563 |
| `/open1..6`, `/snowopen`, `/close`, `/captain`, `/warresult` | War control | — |
| `/down` | Save and kick everyone, then shut sockets down | 911-916 |
| `/discount`, `/alldiscount`, `/offdiscount` | Discounts | — |
| `/santa`, `/offsanta`, `/angel`, `/offangel` | Flying Santa/angel | — |
| `/permanent`, `/offpermanent` | Permanent chat bar | — |
| `/reload_notice` | Reload notices | — |
| `/reload_tables` | Start positions, exchange, upgrade, event trigger, server resource, monster challenge/respawn, ranks | 1054-1089 |
| `/reload_magics` | Reload magic tables | 1091-1116 |
| `/reload_quests` | Reload quest tables | 1119-1126 |
| `/reload_ranks` | Reload ranks | — |
| `/count` | Online count | 1134-1146 |
| `/permitconnect` | Unban | — |

**Missing for testing** (absent from both tables) [V]
- No command to set level, stats or skills. Only the Lua `LevelChange` exists (User.h:1397-1399).
- No teleport to x/z by chat command.
- No command to set HP, kill or revive a player.
- No command to spawn a player or bot.

---

## 9. Lua engine

**Setup** [V]
- Lua 5.2.3 (scripting/Lua/src/lua.h).
- A **single `lua_State`**: SelectAvailableScript always returns `m_luaScript` (LuaEngine.h:63-66; LuaEngine.cpp:184-187).
- Locks: a recursive mutex per execution (LuaEngine.cpp:329) and an RWLock around the bytecode cache (207-254).
- Scripts are compiled lazily from `./Quests/` (LuaEngine.h:5; .cpp:211-238). The cache is off in `_DEBUG` (LuaEngine.h:9-11).

**Only entry point: quest and NPC dialogs** [V]
- `CUser::QuestV2RunEvent` → `ExecuteScript(user, npc, event, step, file)` (QuestHandler.cpp:252-279).
- Globals passed in: UID, STEP, EVENT (LuaEngine.cpp:342-344). The script's user is resolved with `GetUserPtr(UID)` (lua_bindings.cpp:185).

**Bindings** [V]
- Global functions (LuaEngine.cpp:9-112):
  - checks: CheckPercent, HowmuchItem, ShowMap, CheckNation/Class/Level/SkillPoint, clan and loyalty checks;
  - quest helpers: SaveEvent, the exchange functions, SearchQuest, NpcMsg/NpcSay/SelectMsg, ShowEffect, quest-monster counters;
  - `CastSkill`, which makes the *NPC* cast on the user (lua_bindings.cpp:380-396);
  - getters;
  - rewards and costs: Give/RobItem, Gold, Exp, Loyalty;
  - movement and character changes: ZoneChange (+Party/Clan), stat/skill reset, LevelChange, GivePremium, RollDice, Nation/Gender/JobChange.
- Class methods for CUser (User.h:1071-1419) and CNpc (Npc.h:98-115).

**Can it host bot scripts?** [I]
- There are no bindings for movement, target selection, attacking, a user casting a skill, nearby-unit perception, timers or callbacks.
- Scripts run synchronously on the calling thread under one global mutex, which they would share with every quest dialog.
- Hosting bots would need new bindings plus a C++ tick driver. Use it at most for policy tuning; the tick loop belongs in C++.

---

## 10. Database thread

**The queue** [V]
- One consumer thread with a FIFO queue, a mutex and a Condition (DatabaseThread.cpp:11-29, 31-154).
- Each request is a packet prefixed with an int16 socket ID (GameServerDlg.cpp:1298-1305).
- **A request is dropped if that ID is not in the active session map** (DatabaseThread.cpp:62-72).
- Handled opcodes (76-146): LOGIN, SEL_NATION, ALLCHAR_INFO_REQ, CHANGE_HAIR, NEW_CHAR, DEL_CHAR, SEL_CHAR, CHAT (clan notice), DATASAVE, KNIGHTS_PROCESS, LOGIN_INFO, BATTLE_EVENT, SHOPPING_MALL, SKILLDATA, FRIEND_PROCESS, NAME_CHANGE, CAPE, LOGOUT, KING, ITEM_UPGRADE (seal), ZONE_CONCURRENT.

**Procs and queries for one character's lifetime** (DBAgent.cpp) [V]

| Proc / query | Line |
|---|---|
| GAME_LOGIN | 96 |
| NATION_SELECT | 112 |
| select ACCOUNT_CHAR | 126 |
| select USERDATA (char list) | 161 |
| CREATE_NEW_CHAR | 200 |
| LOAD_RENTAL_DATA | 250 |
| select SEALED_ITEMS | 299 |
| LOAD_USER_DATA | 340 |
| select WAREHOUSE | 560 |
| LOAD_PREMIUM_SERVICE_USER | 639 |
| select USER_SAVED_MAGIC | 659 |
| UPDATE_SAVED_MAGIC | 710 |
| SET_LOGIN_INFO | 734 |
| skill shortcut select / SKILLSHORTCUT_SAVE | 779 / 807 |
| UPDATE_USER_DATA | 962 |
| UPDATE WAREHOUSE | 1017 |
| ACCOUNT_LOGOUT | 1504 |
| CLEAR_REMAIN_USERS | 1920 |
| UPDATE_RANKS | 1953 |

**Save cadence** [V]
- Every 180 s, on logout, on server change, and on `/down`.
- `m_lastSaveTime` is set in LoadUserData and UpdateUser (DBAgent.cpp:395, 986).

**Connections**
- Two ODBC connections, `m_GameDB` and `m_AccountDB`. OdbcConnection has its own mutex (shared/database/OdbcConnection.cpp:6-211).
- `g_DBAgent` is also called directly from non-DB threads, for example 8 lines in User.cpp and 10 in GameServerDlg.cpp [V].

**SQL definitions are not in the repo** (no *.sql) [U]. LOAD_USER_DATA's column order is only visible through its fetches (DBAgent.cpp:351-392).

**What a bot character needs in the DB** [V/I]
- An account that GAME_LOGIN accepts; whether a bot bypasses this depends on the integration design.
- A USERDATA row with:
  - nation, race and class;
  - level 80;
  - stats and the skill string;
  - `strItem` as a binary of `INVENTORY_TOTAL*8` bytes, plus serials and item times;
  - zone 71 and a position;
  - **loyalty > 0**, which is required to enter and to stay in Ronark Land (CharacterMovementHandler.cpp:267-271; AttackHandler.cpp:235-238).
- A WAREHOUSE row for the account.
- Premium, saved magic and skill shortcuts are optional.

---

## 11. Packet layer

**Framing** [V]
- Layout: `AA 55 | uint16 len | payload | 55 AA` (KOSocket.cpp:36-49, 74-83, 184-187).
- At most 4 fragmented reads (60-66); receive buffer 3172 bytes (User.cpp:12).

**Encryption: JvCryption** [V]
- Enabled by `USE_CRYPTION` (shared/JvCryption.h:3).
- The server sends a public key in its version reply (LoginHandler.cpp:14). The working key is that public key XOR a compile-time private key chosen by `__VERSION` (JvCryption.cpp:6-15).
- Client to server: every packet must carry a CRC32 and a uint32 sequence equal to `++m_sequence` (KOSocket.cpp:111-122).
- Server to client: header 0x1efc and a uint16 sequence that is not incremented (151-166).
- LZF compression via WIZ_COMPRESS_PACKET for packets of 500 bytes or more (195-220).
- A handler returning false disconnects the client in release builds (90-96).
- Quirk: the `WIZ_LOGOSSHOUT` case falls through to `default` and returns false (User.cpp:457-462).

**Relevant opcodes** (shared/packets.h)

| Group | Opcodes |
|---|---|
| Login / selection | LOGIN 0x01, NEW_CHAR 0x02, SEL_CHAR 0x04, SEL_NATION 0x05, ALLCHAR_INFO_REQ 0x0C, GAMESTART 0x0D, MYINFO 0x0E, LOGOUT 0x0F, VERSION_CHECK 0x2B |
| Movement / visibility | MOVE 0x06, USER_INOUT 0x07, ROTATE 0x09, NPC_INOUT 0x0A, NPC_MOVE 0x0B, REGIONCHANGE 0x15, REQ_USERIN 0x16, REQ_NPCIN 0x1D, WARP 0x1E, ZONE_CHANGE 0x27, SPEEDHACK_CHECK 0x41, HOME 0x48 |
| Combat | ATTACK 0x08, DEAD 0x11, REGENE 0x12, HP_CHANGE 0x17, MSP_CHANGE 0x18, EXP_CHANGE 0x1A, TARGET_HP 0x22, STATE_CHANGE 0x29, LOYALTY_CHANGE 0x2A, MAGIC_PROCESS 0x31 |
| Social / other | CHAT 0x10, ITEM_MOVE 0x1F, PARTY 0x2F, CHAT_TARGET 0x35, COMPRESS 0x42, RANK 0x80 |

- Party sub-opcodes: CREATE 1, PERMIT 2, INSERT 3, HPCHANGE 6 (238-243).
- MagicOpcode: CASTING 1, FLYING 2, EFFECTING 3, FAIL 4, DURATION_EXPIRED 5, CANCEL 6, TYPE4_EXTEND 8 (380-389).
- ZoneChange: Loading 1, Loaded 2, Teleport 3 (209-211).

**Payloads the handlers read** [V]
- **MOVE:** u16 x×10, z×10, y×10, i16 speed, u8 echo (CharacterMovementHandler.cpp:15).
- **ATTACK:** u8 type, u8 result, i16 target ID, i16 delaytime, i16 distance (AttackHandler.cpp:10).
  - The client's delaytime must be at least the weapon's `m_sDelay + 10`, and distance must be within `m_sRange` (27-33).
- **MAGIC_PROCESS:** u8 opcode, u32 skill, i16 caster (must equal own ID and be below NPC_BAND), i16 target, 7× i16 data (MagicProcess.cpp:22, 41-49).

**Server-side checks that matter for bots** [V]
- **R-hit:** 1 s per attacker (User.h:25; Unit.cpp:926-947; AttackHandler.cpp:42-54).
- **Skill recast:** UNIXTIME-second granularity (MagicInstance.cpp:360-370). A 0-second difference passes, so a same-second recast is not blocked.
- **Type cooldown:** 0.7 s (User.h:23; MagicInstance.cpp:396-421).
- **Speed:** see §1.
- **Position:** the only position check is the SMD `IsValidPosition` (CharacterMovementHandler.cpp:21).

---

## 12. Feasibility comparison

### (A) Server-side "socketless CUser" bot

**Coupling points**: every place that assumes a real connection [V unless noted]
1. **ID is the socket index** (User.h:113). Party `uid[]`, region sets, the DB queue and AI packets all carry it.
2. **Users are found only through the active session map** (GameServerDlg.cpp:549). This affects:
   - region broadcasts (943, 1053, 1364, 1395, 1445);
   - GetUnitPtr (557-563) → Attack target (AttackHandler.cpp:35), MagicInstance caster and target (17, 20) and AoE (1279, 1637, 2177);
   - parties (GameServerDlg.cpp:1075; PartyHandler.cpp:64, 91, 188, 228);
   - LoyaltyChange (User.cpp:2807), the DOT source (3396), Lua (lua_bindings.cpp:185);
   - the DB dispatcher (DatabaseThread.cpp:68);
   - the R-hit check and Attack's self-lookup (Unit.cpp:929; AttackHandler.cpp:52).
   - In total, 114 lines in GameServer/*.cpp call GetUserPtr.
3. **ID range:** user IDs must be below 10000 (GameServerDlg.cpp:559). The session pool is fixed at MAX_USER = 3000 (GameServerDlg.cpp:97). **The AI server rejects IDs ≥ MAX_USER** (AIServer/ServerDlg.cpp:549-563).
4. **Broadcasts and timers iterate the active map**: GameServerDlg.cpp:816-916, plus 24 copies of the map in total.
5. **Send / SendCompressed are virtual** (KOSocket.h:33-34) and drop packets when not connected (KOSocket.cpp:142). A faked "connected" flag on an invalid fd would WSASend and then Disconnect (SocketWin32.cpp:8-46).
6. **Disconnect is non-virtual** (Socket.h:14) and does nothing when not connected (Socket.cpp:99-100).
   - So `/kill`, OPERATOR_CUTOFF, the speedhack kick and the duplicate-login kick have no effect on a socketless bot.
   - The OnDisconnect → LogOut → ReqUserLogOut save path never runs.
7. **Timeout:** the timer disconnects the session and skips its Update after 30 s without `m_lastResponse` (GameServerDlg.cpp:738-749). That field is protected and only set by OnRead and OnConnect (KOSocket.cpp:22, 88).
8. **HandlePacket gates:** it requires crypto on, an account ID set, and a character selected (User.cpp:223-272). `m_usingCrypto` is protected (KOSocket.h:46).
9. **Sequence and CRC** are checked only in DecryptPacket (KOSocket.cpp:107-138), so packets injected straight into HandlePacket skip them.
10. **GetRemoteIP** is used for SET_LOGIN_INFO (CharacterSelectionHandler.cpp:244). For a socket that was never accepted, `m_client` is uninitialized [I].
11. **DB requests** are keyed by socket ID and dropped when it is inactive. There is also the logout race described in §4.10.
12. **Name maps** are needed for party invites by name (PartyHandler.cpp:16-24), GM commands and duplicate-login detection.
13. **Client-driven state steps** must be emulated: GAMESTART 1/2, ZONE_CHANGE Loaded, REGENE, SPEEDHACK_CHECK.
14. **Update() cadence** (User.cpp:465; GameServerDlg.cpp:752-753).
15. **The `m_deleted` flag**: LogOut sets it (User.cpp:876), ReqUserLogOut clears it (DatabaseThread.cpp:461), and IsDeleted is checked at SocketMgr.cpp:109, 128.
16. **The target ID lives in the socket class** (KOSocket.h:44).
17. **AI-server registration**: AG_USER_INOUT (CharacterMovementHandler.cpp:90-96), Send2AI_UserUpdateInfo (User.cpp:1019), SendAllUserInfo (GameServerDlg.cpp:1661-1701).
18. **Rewards and rankings include everyone in the map who is in that zone**: SetPlayerRankingRewards (GameServerDlg.cpp:3349-3370; default reward zones 71, 72, 73 at :298) and AddPlayerRank (CharacterMovementHandler.cpp:525).

**Minimal seams** [I, proposals]
- **S1: reserved slot pool.** At startup, remove N IDs from `m_idleSessions` (under `GetLock()`) so the accept path never hands them out. When a bot spawns, insert it into `m_activeSessions`. IDs stay below MAX_USER, so the AI server accepts them and every lookup, region, party and DB path works unchanged.
- **S2: a bot subclass.** Either `CBotUser : CUser` or a `m_bIsBot` flag, overriding Send/SendCompressed. They can be no-ops, or feed a perception sink (HP_CHANGE, DEAD, PARTY PERMIT, ZONE_CHANGE). `TO_USER` is a `static_cast` (Define.h:285), so a subclass is compatible.
- **S3: bot login.** Set the account and character IDs, then queue WIZ_SEL_CHAR with AddDatabaseRequest. That leads to SelectCharacter. Then call GameStart(1) and GameStart(2) with crafted packets, call RecvZoneChange(Loaded) right after each zone change, and call Regene(1) after death.
- **S4: explicit bot logout.** Mirror OnDisconnect (User.cpp:174-208) and LogOut, then return the slot to the pool only after the DB logout has finished.
- **S5: timeout.** Refresh `m_lastResponse` on every bot tick, or skip bots in Timer_UpdateSessions.
- **S6: actions as packets.** Feed MOVE, ATTACK, MAGIC_PROCESS, ITEM_MOVE, STATE_CHANGE and PARTY packets to `HandlePacket(pkt)`. This reuses the same handlers as real players and therefore the same rules. Run them on the IOCP thread through a custom IOCP event (SocketDefines.h:3-9; SocketMgr.cpp:84-85).
- **S7: bot-specific bypasses.** Skip SET_LOGIN_INFO and ACCOUNT_LOGOUT, or pass a fixed IP. Optionally exclude bots from rankings and rewards.

**Assessment**
- Pros: identical combat, skill, loyalty and party rules; seen as real players through WIZ_USER_INOUT; no network overhead.
- Cons: thread-safety work (§2); uses MAX_USER slots; changes in GameServer plus one enum in shared; one DB row set per bot; economy side effects (NP and Knight Cash rewards, rankings, zone-wide death notices).

### (B) External headless client bots
- **Fidelity:** maximal. They go through the real login, speedhack, attack-delay and timeout paths.
- **Visibility:** identical to real players. **Server changes:** none.
- **What has to be built:**
  - JvCryption with the version's private key (JvCryption.cpp:6-13), CRC32 and the sequence counter (KOSocket.cpp:111-122), and LZF;
  - world-state parsing of MYINFO, USER_INOUT, NPC_INOUT and REGIONCHANGE;
  - SMD-based navigation (the server only validates positions);
  - a keep-alive more often than every 30 s.
- **Per-bot cost:** a real account and character row, and one of the 3000 sessions.
- **LoginServer:** probably not needed. GameServer's LoginProcess only checks GAME_LOGIN (LoginHandler.cpp:21-50) [I; the proc body is U].
- **Risks:** the client's exact packet cadence and values cannot be seen from server code [U]; process management. It adds no new races.

### (C) NPC-based bots in AIServer
- **Pros:** reuses the AI FSM and pathfinding (250 ms tick) and SpawnEventNpc.
- **Cons:**
  - Rendered as an NPC or monster model with no race, class or gear (Npc.cpp:138-155).
  - Uses NPC damage and AC (Unit.cpp:474-590), not player formulas.
  - No player skill tree or buff logic; only one-off `CNpc::CastSkill` (Npc.cpp:269-284).
  - Cannot be partied, because party `uid` holds user socket IDs.
  - Different death and reward paths: a player killed by an NPC loses EXP, not NP (User.cpp:4754-4781); an NPC kill goes through `CNpc::OnDeath` (Npc.cpp:319-465), which awards no NP.
  - `CNpc::isHostileTo` can dereference an uninitialized `pKnights` while siege war is open (Unit.cpp:1180-1190) [V; I crash risk].
- **Fidelity: low.** Only useful as training dummies.

**Recommendation** [I]: use (A) with seams S1-S7. Keep (B) as a server-unchanged option and as a validation tool. (C) is not suitable for realistic PK.

---

## 13. Build and dev environment, capacity

**Solution and projects** [V]
- Projects: AIServer, GameServer, LogInServer, Lua, shared (KnightOnlineServer.sln:6-24).
- **No test project or test framework.**

**GameServer vcxproj** [V]
- Win32 Debug/Release only (4-10).
- Toolset **v142** (22, 28), **stdcpp17** (69, 110), MultiByte (21, 27), static CRT /MT(d) (63, 102).
- Defines: `WIN32; GAMESERVER; _WINSOCK_DEPRECATED_NO_WARNINGS; _WINDOWS; _3DSERVER; _CRT_SECURE_NO_WARNINGS`, plus `_DEBUG` or `NDEBUG` (62, 100).
- Links ws2_32, Lua.lib and shared.lib (76, 117). Output: `build\bin\<plat>-<cfg>\Server\` (45-50).
- The other projects also use v142 and C++17. AIServer defines `AI_SERVER` and LogInServer defines `LOGIN_SERVER`. shared and Lua are static libraries.
- Prebuilt x86-Release executables and PDBs exist in build/bin/x86-Release/Server.

**Logging** [V]
- printf to the console.
- TRACE maps to OutputDebugString in debug builds and to nothing in release (shared/stdafx.h:19-43; DebugUtils.cpp:4-21).
- Log files `./Logs/{DeathUser,DeathNpc,Chat,Cheat}_d_m_y.log` (GameServerDlg.cpp:162-190, 3260-3283).
- SQL errors go through ReportSQLError (DBAgent.cpp:61).

**Capacity constants** [V]

| Constant | Value | Where |
|---|---|---|
| MAX_USER | 3000 | globals.h:9 |
| NPC_BAND / INVALID_BAND | 10000 / 30000 | Define.h:67-68 |
| VIEW_DISTANCE (region size) | 48 | globals.h:19 |
| MAX_PARTY_USERS | 8 | shared/database/structs.h:291 |
| MAX_LEVEL | 80 | Define.h:21 |
| MAX_PLAYER_HP / MAX_DAMAGE | 14000 / 32000 | Define.h:22-23 |
| MAX_TYPE3_REPEAT / MAX_TYPE4_BUFF | 40 / 50 | Define.h:10-11 |
| CUser buffers (send / recv) | 16384 / 3172 bytes | User.cpp:12 |
| AI NPC tick | 250 ms | AIServer/NpcThread.cpp:5 |

**Regions and hot loops**
- Region index is position / 48 (Unit.h:84-85). Broadcasts cover 3×3 regions (globals.h:427-428), roughly a 144×144 window.
- Each region change resends full GetUserInfo for everyone in that window [V].
- Send_Zone and Send_All **copy the whole session map for every packet** (GameServerDlg.cpp:858, 895). Each PK death notice triggers one of these (ChatHandler.cpp:341) [V].
- `CUser::Update` scans about INVENTORY_TOTAL + 192 slots after every packet (User.cpp:512-532) [V].
- For k units packed into one 3×3 window, every move, attack or skill goes to k recipients. That is O(k²) packets per tick, which is fine at k = 16 and grows with spectators [I].

---

## Open questions requiring a runtime test
1. Does the 1453 client render a WIZ_USER_INOUT entity whose ID is 3000 or above? This only matters if bots use IDs outside the session pool.
2. Does any client NPC model (m_sPid) look like an equipped human? This decides option C's visuals.
3. What do the GAME_LOGIN, LOAD_USER_DATA, SET_LOGIN_INFO, ACCOUNT_LOGOUT and UPDATE_USER_DATA procs actually contain? They are not in the repo. Does GAME_LOGIN need state left by the LoginServer?
4. Is ReqUserLogOut ever skipped when the DB queue is more than 100 ms behind (the logout race in §4.10)?
5. Do the unlocked session-map copies crash under heavy connect/disconnect churn?
6. What packet cadence does a real client use (MOVE frequency, SPEEDHACK_CHECK interval, attack delaytime/echo)? Bots need this to pass the server's checks.
7. Does the 1-second UNIXTIME resolution cause problems for bot skill timing (same-second recast passes the cooldown check)?
8. How much latency does the single IOCP worker add if all bot actions are marshalled onto it (16-100 bots at 4-10 Hz)?
9. Do Ronark Land guards and monsters treat bots registered via AG_USER_INOUT exactly like players?
10. Are the `./Quests/*.lua` scripts deployed on the target server? They are not in the repo.
