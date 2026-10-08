# C — AlphaGame v1534: expected DB schema vs FDP_kn_online

Date: 2026-10-08. Scope: read-only analysis. Neither database was modified, no downloaded exe was run, and no rows of personal-data tables were read. Row counts of a few personal tables were taken from `sys.dm_db_partition_stats` (metadata only).

Evidence tags: **[V]** = verified this session by a query against both instances or by a script over both sources; **[D]** = read from source code (file:line); **[A]** = inference, not verified.

Paths: ALPHA = `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/1-Game Source`; OUR = `/mnt/c/dev/fdp-merge-final`; ALPHA DB = `.\SQL2019` / `FDP_alpha1534`; OUR DB = `.\SQLEXPRESS` / `FDP_kn_online`. ALPHA line numbers come from the downloaded tree, and OUR line numbers come from `fdp-merge-final` (not from commit `0f52027`).

---

## 0. Verdict

1. **The schema gap is small and additive [V].** All of OUR server's loaders and direct SQL work against the ALPHA DB, because ALPHA's tables are a superset. ALPHA needs the following that OUR DB lacks:
   - 8 tables read at startup: FISHING_ITEM, MINING_ITEM, ITEM_EXCHANGE_EXP, ITEM_EXCHANGE_CRASH, KING_NOMINATION_LIST, MONSTER_STONE_RESPAWN_LIST, MONSTER_JURAID_MOUNTAIN_RESPAWN_LIST, USER_SEAL.
   - 2 tables used only inside procedures: GAME_OPTIONS and PREMIUM_ITEM_LETTER_GIFT.
   - About 85 columns in 10 tables, mostly MAKE_ITEM_GROUP `iItem_31..100`. The others: USERDATA +5, WAREHOUSE +1, KNIGHTS +1, ITEM +2 (+1 unused), KING_SYSTEM +1, MAIL_BOX +1, MONSTER_RESPAWN_LIST(+_INFORMATION) +1 each.
   - 6 new procedures.
   - 5 procedures whose contract changed: LOAD_USER_DATA (result shape), UPDATE_USER_DATA (39→43 params), MAIL_BOX_SEND (10→11), KING_ELECTION_PROC (5→4) and CHANGE_HAIR (see 2.4).
2. **One DB cannot serve both servers unchanged [V].** LOAD_USER_DATA, UPDATE_USER_DATA, MAIL_BOX_SEND and KING_ELECTION_PROC have mutually exclusive positional contracts. Tables can be a common superset, but procedures cannot.
3. **The ALPHA DB does not fully match ALPHA's own source [V].** ALPHA calls these with the wrong arity or with missing objects: CHANGE_HAIR, KING_CANDIDACY_RECOMMEND, KING_CANDIDACY_NOTICE_BOARD_PROC, KING_CHANGE_TAX, ACCOUNT_NATION_TRANSFER, MAIL_BOX_GET_ITEM (6th column), KING_ELECTION_RESULTS, KING_UPDATE_DATABASE, DELETE_CHAR and the SERVER_LIST table. Adopting the ALPHA DB therefore still needs procedure fixes.
4. **The inventory blob keeps slots 0–46 byte-identical [D/V].** ALPHA inserts a fairy slot at 47, which shifts the bags (BAG1/BAG2 48/49) and moves magic-bag slots 49–72 to 50–73. The code's `INVENTORY_TOTAL` becomes 74 (592 B), but ALPHA's DB still has `strItem binary(584)` (73 slots). The bot scripts write only slots 0–41, so their strItem bytes stay valid.
5. **Reference data differs a lot [V].**
   - MAGIC: every one of the 124 bot-used skills differs. ALPHA sets SelfEffect to 0, its Range is about ×0.45 of ours, and ReCastTime differs. MAGIC_TYPE1 Hit/AddDamage, MAGIC_TYPE3 EndDamage and MAGIC_TYPE4 BuffType/ExpPct also differ.
   - ITEM: every common row differs, and **ALPHA's `ItemClass` is NULL in all rows**. ALPHA requires ReqLevel 60 and Dex/Int/Cha 14–16 on the bot reverse armour, where ours requires level 1 and 0.
   - Our hand-made bowl spawn edit (K_NPCPOS, 87 groups deleted, STATUS B4) is not in ALPHA.
6. **Recommendation:** (a) migrate OUR DB forward with additive scripts. This keeps the bot rows, the tuned reference data and the 2017 instance. Any import of ALPHA reference rows is a separate owner decision (section 7). Option (b) costs more because of points 3 and 5.

---

## 1. Method

- Source: regex extraction of every `GetTableName/GetColumns/Fetch*` in `shared/database/*.h` (both trees), every SQL string literal and `{CALL …}` in `GameServer/DBAgent.cpp`, `LoadServerData.cpp`, `AIServer/ServerDlg.cpp` and `LogInServer/DBProcess.cpp`, and a function-level diff of `CDBAgent::*` [D].
- DB: dumps of `INFORMATION_SCHEMA.COLUMNS`, `sys.parameters` and `sys.sql_modules` from both instances, then column, parameter and body diffs [V].
- Before the restore, the `.bak` was decoded directly by parsing the `sysschobjs`, `syscolpars` and `sysobjvalues` pages. This gave the same table, column and procedure lists as the restored DB later did (cross-check passed) [V]. The backup header says `MSSQL15.SQLEXPRESS`, i.e. SQL Server 2019 Express.
- Working files are under `scratchpad/k1534/work/` (see section 9).

---

## 2. What ALPHA reads and writes

### 2.1 Startup loaders (`LOAD_TABLE`; a missing table or column aborts startup, `LOAD_TABLE_ERROR_ONLY` only logs)

"(req)" means `AllowEmptyTable=false`: the table must also be non-empty. All loaders name their columns explicitly except the ones marked `*`, which are positional `SELECT *`.

| Loader (shared/database) | Table | Cols | Loaded at | OUR DB | ALPHA DB |
|---|---|---|---|---|---|
| BattleSet.h (CBattleSet) | BATTLE | 1 | LoadServerData.cpp:326 | ok | ok |
| CoefficientSet.h (CCoefficientSet) | COEFFICIENT | 15 | LoadServerData.cpp:178 (req) | ok | ok |
| EventSet.h (CEventSet) | EVENT | 13 | LoadServerData.cpp:444 | ok | ok |
| EventTimes.h (CEventTimes) | EVENT_TIMES | 7 | not loaded (dead) | **table missing** | **table missing** |
| EventTriggerSet.h (CEventTriggerSet) | EVENT_TRIGGER | 4 | LoadServerData.cpp:404 | ok | ok |
| FishingTableSet.h (CFishingTableSet) | FISHING_ITEM | 3 | LoadServerData.cpp:384 | **table missing** | ok |
| ItemExchangeCrashSet.h (CItemExchangeCreashSet) | ITEM_EXCHANGE_CRASH | 4 | LoadServerData.cpp:91 | **table missing** | ok |
| ItemExchangeExpSet.h (CItemExchangeExpSet) | ITEM_EXCHANGE_EXP | * | LoadServerData.cpp:86 | **table missing** | ok |
| ItemExchangeSet.h (CItemExchangeSet) | ITEM_EXCHANGE | * | LoadServerData.cpp:81 | ok | ok |
| ItemExpirationSet.h (CItemExpirationSet) | ITEM_EXPIRATION | 2 | not loaded (dead) | **table missing** | **table missing** |
| ItemOpSet.h (CItemOpSet) | ITEM_OP | 4 | LoadServerData.cpp:101 | ok | ok |
| ItemTableSet.h (CItemTableSet) | ITEM | 61 | LoadServerData.cpp:71 (req) | **missing: UpgradeNotice, NPbuyPrice** | ok |
| ItemUpgradeSet.h (CItemUpgradeSet) | ITEM_UPGRADE | * | LoadServerData.cpp:96 (req) | ok | ok |
| JuraidMountionListInformationSet.h (CJuraidMountionListInformationSet) | MONSTER_JURAID_MOUNTAIN_RESPAWN_LIST | 12 | LoadServerData.cpp:374 | **table missing** | ok |
| KingCandidacyNoticeBoardSet.h (CKingCandidacyNoticeBoardSet) | KING_CANDIDACY_NOTICE_BOARD | 3 | LoadServerData.cpp:332 | ok | ok |
| KingElectionListSet.h (CKingElectionListSet) | KING_ELECTION_LIST | 5 | LoadServerData.cpp:334 | ok | ok |
| KingNominationListSet.h (CKingNominationListSet) | KING_NOMINATION_LIST | 3 | LoadServerData.cpp:333 | **table missing** | ok |
| KingSystemSet.h (CKingSystemSet) | KING_SYSTEM | 30 | LoadServerData.cpp:331 | **missing: sKingClanID** | ok |
| KnightsAllianceSet.h (CKnightsAllianceSet) | KNIGHTS_ALLIANCE | 4 | LoadServerData.cpp:199 | ok | ok |
| KnightsCapeSet.h (CKnightsCapeSet) | KNIGHTS_CAPE | 5 | LoadServerData.cpp:283 (req) | ok | ok |
| KnightsRankSet.h (CKnightsRankSet) | KNIGHTS_RATING | 3 | LoadServerData.cpp:289, LoadServerData.cpp:294 | ok | ok |
| KnightsSet.h (CKnightsSet) | KNIGHTS | 28 | LoadServerData.cpp:189 | **missing: strAllianceNotice** | ok |
| KnightsSiegeWar.h (CKnightsSiegeWarfare) | KNIGHTS_SIEGE_WARFARE | 38 | LoadServerData.cpp:204 | ok | ok |
| KnightsUserSet.h (CKnightsUserSet) | KNIGHTS_USER | 3 | LoadServerData.cpp:194 | ok | ok |
| LevelUpTableSet.h (CLevelUpTableSet) | LEVEL_UP | 2 | LoadServerData.cpp:183 (req) | ok | ok |
| MagicTableSet.h (CMagicTableSet) | MAGIC | 21 | LoadServerData.cpp:123 (req), ServerDlg.cpp:121 (req) | ok | ok |
| MagicType1Set.h (CMagicType1Set) | MAGIC_TYPE1 | 10 | LoadServerData.cpp:128 (req), ServerDlg.cpp:126 (req) | ok | ok |
| MagicType2Set.h (CMagicType2Set) | MAGIC_TYPE2 | 6 | LoadServerData.cpp:133 (req), ServerDlg.cpp:131 (req) | ok | ok |
| MagicType3Set.h (CMagicType3Set) | MAGIC_TYPE3 | 9 | LoadServerData.cpp:138 (req) | ok | ok |
| MagicType4Set.h (CMagicType4Set) | MAGIC_TYPE4 | 29 | LoadServerData.cpp:143 (req), ServerDlg.cpp:136 (req) | ok | ok |
| MagicType5Set.h (CMagicType5Set) | MAGIC_TYPE5 | 4 | LoadServerData.cpp:148 (req) | ok | ok |
| MagicType6Set.h (CMagicType6Set) | MAGIC_TYPE6 | 24 | LoadServerData.cpp:153 (req) | ok | ok |
| MagicType7Set.h (CMagicType7Set) | MAGIC_TYPE7 | 12 | LoadServerData.cpp:158 (req) | ok | ok |
| MagicType8Set.h (CMagicType8Set) | MAGIC_TYPE8 | 6 | LoadServerData.cpp:163 (req) | ok | ok |
| MagicType9Set.h (CMagicType9Set) | MAGIC_TYPE9 | 12 | LoadServerData.cpp:168 (req) | ok | ok |
| MakeDefensiveTableSet.h (CMakeDefensiveTableSet) | MAKE_DEFENSIVE | 8 | ServerDlg.cpp:146 | ok | ok |
| MakeGradeItemTableSet.h (CMakeGradeItemTableSet) | MAKE_ITEM_GRADECODE | 10 | ServerDlg.cpp:151 (req) | ok | ok |
| MakeItemGroupSet.h (CMakeItemGroupSet) | MAKE_ITEM_GROUP | 101 | ServerDlg.cpp:171 (req) | **missing: iItem_31, iItem_32, iItem_33, iItem_34, iItem_35, iItem_36 (+64)** | ok |
| MakeLareItemTableSet.h (CMakeLareItemTableSet) | MAKE_ITEM_LARECODE | 4 | ServerDlg.cpp:156 (req) | ok | ok |
| MakeWeaponTableSet.h (CMakeWeaponTableSet) | MAKE_WEAPON | 13 | ServerDlg.cpp:141 | ok | ok |
| MiningTableSet.h (CMiningTableSet) | MINING_ITEM | 3 | LoadServerData.cpp:379 | **table missing** | ok |
| MonsterChallenge.h (CMonsterChallenge) | MONSTER_CHALLENGE | 6 | LoadServerData.cpp:339 | ok | ok |
| MonsterChallengeSummonList.h (CMonsterChallengeSummonList) | MONSTER_CHALLENGE_SUMMON_LIST | 10 | LoadServerData.cpp:344 | ok | ok |
| MonsterRespawnListInformationSet.h (CMonsterRespawnListInformationSet) | MONSTER_RESPAWN_LIST_INFORMATION | 10 | LoadServerData.cpp:364 | **missing: byDirection** | ok |
| MonsterRespawnListSet.h (CMonsterRespawnListSet) | MONSTER_RESPAWN_LIST | 4 | LoadServerData.cpp:359 | **missing: sType** | ok |
| MonsterStoneListInformationSet.h (CMonsterStoneListInformationSet) | MONSTER_STONE_RESPAWN_LIST | 12 | LoadServerData.cpp:369 | **table missing** | ok |
| MonsterSummonListSet.h (CMonsterSummonListSet) | MONSTER_SUMMON_LIST | 4 | LoadServerData.cpp:349 | ok | ok |
| MonsterSummonListZoneSet.h (CMonsterSummonListZoneSet) | MONSTER_SUMMON_LIST_ZONE | 4 | LoadServerData.cpp:354 | ok | ok |
| NpcItemSet.h (CNpcItemSet) | K_MONSTER_ITEM | 11 | ServerDlg.cpp:166 (req) | ok | ok |
| NpcPosSet.h (CNpcPosSet) | K_NPCPOS | 20 | ServerDlg.cpp:190 (req) | ok | ok |
| NpcTableSet.h (CNpcTableSet) | K_NPC | 44 | ServerDlg.cpp:181 (req) | ok | ok |
| NpcTableSet.h (CMonTableSet) | K_MONSTER | 44 | ServerDlg.cpp:182 (req) | ok | ok |
| ObjectPosSet.h (CObjectPosSet) | K_OBJECTPOS | 10 | LoadServerData.cpp:419, ServerDlg.cpp:176 (req) | ok | ok |
| PremiumItemExpSet.h (CPremiumItemExpSet) | PREMIUM_ITEM_EXP | 5 | LoadServerData.cpp:394 | ok | ok |
| PremiumItemSet.h (CPremiumItemSet) | PREMIUM_ITEM | 7 | LoadServerData.cpp:389 | ok | ok |
| QuestHelperSet.h (CQuestHelperSet) | QUEST_HELPER | 16 | LoadServerData.cpp:113 | ok | ok |
| QuestMonsterSet.h (CQuestMonsterSet) | QUEST_MONSTER | 21 | LoadServerData.cpp:118 | ok | ok |
| RentalItemSet.h (CRentalItemSet) | RENTAL_ITEM | 11 | LoadServerData.cpp:173 | ok | ok |
| ServerResourceSet.h (CServerResourceSet) | SERVER_RESOURCE | 2 | LoadServerData.cpp:106 (req), ServerDlg.cpp:161 (req) | ok | ok |
| SetItemTableSet.h (CSetItemTableSet) | SET_ITEM | * | LoadServerData.cpp:76 | ok | ok |
| StartPositionRandomSet.h (CStartPositionRandomSet) | START_POSITION_RANDOM | 4 | LoadServerData.cpp:409 | ok | ok |
| StartPositionSet.h (CStartPositionSet) | START_POSITION | 11 | LoadServerData.cpp:321 (req) | ok | ok |
| UnderMonsterChallenge.h (CUnderMonsterChallenge) | UNDER_MONSTER_CHALLENGE | 6 | not loaded (dead) | **table missing** | **table missing** |
| UnderMonsterChallengeSummonList.h (CUnderMonsterChallengeSummonList) | UNDER_MONSTER_CHALLENGE_SUMMON_LIST | 10 | not loaded (dead) | **table missing** | **table missing** |
| UserDailyOpSet.h (CUserDailyOpSet) | USER_DAILY_OP | 9 | LoadServerData.cpp:399 | ok | ok |
| UserItemSet.h (CUserItemSet) | USER_ITEMS | 2 | LoadServerData.cpp:414 | ok | ok |
| UserKnightsRankSet.h (CUserKnightsRankSet) | USER_KNIGHTS_RANK | 6 | LoadServerData.cpp:210 | ok | ok |
| UserPersonalRankSet.h (CUserPersonalRankSet) | USER_PERSONAL_RANK | 6 | LoadServerData.cpp:209 | ok | ok |
| UserSealSet.h (CIUserSealItem) | USER_SEAL | 15 | LoadServerData.cpp:238 | **table missing** | ok |
| ZoneInfoSet.h (CZoneInfoSet) | ZONE_INFO | 4 | LoadServerData.cpp:425 (req), ServerDlg.cpp:441 (req) | ok | ok |

Notes [D]:
- The 4 "dead" loaders (EVENT_TIMES, ITEM_EXPIRATION, UNDER_MONSTER_CHALLENGE*) are never instantiated, so neither DB needs those tables.
- KING_* and ZONE_INFO/EVENT use `LOAD_TABLE_ERROR_ONLY` (`LoadServerData.cpp:331-333, 425, 444`), so a missing KING_NOMINATION_LIST would only log an error.
- Type notes:
  - ALPHA `KingCandidacyNoticeBoardSet` fetches `strNotice` as a String, but the column is `varbinary(1024)` in both DBs, so it would read as hex text. OUR loader fetches it as Binary.
  - ALPHA's `USER_ITEMS` columns are `smallint`/`smallint` (ours `int`/`nchar(586)`). The loader fetches UInt32/UInt64, and the table is empty in ALPHA.

### 2.2 Direct SQL (ALPHA `GameServer/DBAgent.cpp` unless noted) [D]

| Line | Function | Table: columns (R = read, W = write) |
|---|---|---|
| 125 | GetAllCharID | ACCOUNT_CHAR R strCharID1..3 by strAccountID |
| 162 | LoadCharInfo | USERDATA R Race, Class, HairRGB, Level, Face, Zone, strItem, **strItemTime** (new vs OUR :161) |
| 301 | LoadItemSealData | SEALED_ITEMS R nItemSerial, nItemID, bSealType |
| 599 | LoadWarehouseData | WAREHOUSE R nMoney, WarehouseData, strSerial, **strUserSeal**, WarehouseDataTime |
| 827 | LoadSkillShortcut | USERDATA_SKILLSHORTCUT R nCount, strSkillData |
| 911 | RequestFriendList | FRIEND_LIST R `*` |
| 1087 | UpdateWarehouseData | WAREHOUSE W nMoney, dwTime, WarehouseData, strSerial, **strUserSeal**, WarehouseDataTime |
| 1179 | LoadKnightsAllMembers | USERDATA R Fame, Level, Class, **strMemo**, DATEDIFF(dtUpdateTime) per member |
| 1204 / 1220 / 1239 / 1257 | LoadCapeId / LoadAllianceInfo / LoadKnightsInfo / LoadKnightsAllList | KNIGHTS R sCape; KNIGHTS_ALLIANCE R sSub/sMercenary_1/2; KNIGHTS R Nation, IDName, Members, Points, Ranking |
| 1314, 1482, 1499, **1516**, 1574, 1592 | clan updates | KNIGHTS W Mark/sMarkVersion/sMarkLen, ClanPointFund, strClanNotice, **strAllianceNotice**, sCape+bCapeR/G/B, sCape+Flag |
| 1449, 1466 | RefundNP, UpdateUserAuthority | USERDATA W Loyalty, Authority |
| 1604 | UpdateBattleEvent | BATTLE W byNation, strUserName |
| 1625 | UpdateConCurrentUserCount | CONCURRENT W zoneN_count |
| 1802 | DeleteLetter | MAIL_BOX W bDeleted |
| 2111, 2119 | UpdateNationIntro (AccountDB) | **SERVER_LIST** W strKarusKing… — table absent in **both** DBs |
| 2251–2261 | UpdateSiegeTax | KNIGHTS_SIEGE_WARFARE W tariffs/taxes |
| 2274 | GetClanIDWithCharID | USERDATA R Knights |
| 2295 | LoadLadderRankList | USERDATA ⟗ KNIGHTS R strUserID, IDNum, IDName, sMarkVersion, LoyaltyMonthly, Authority |
| 2333 | LoadKnightsLeaderList | KNIGHTS R IDNum, Chief, IDName, sMarkVersion, Nation, Flag, ClanPointFund, Points |
| 2489 | CharacterSealDelete | USER_SEAL DELETE |
| LogInServer/DBProcess.cpp:20, 54 | version, user count | VERSION R sVersion, sHistoryVersion, strFilename; CONCURRENT R |

Every table and column above exists in both DBs, except USERDATA.strMemo, WAREHOUSE.strUserSeal and KNIGHTS.strAllianceNotice (missing in OUR DB), USER_SEAL (missing in OUR DB) and SERVER_LIST (missing in both) [V].

### 2.3 Stored procedures called (arity = number of `?`/`%d` arguments, excluding the `? =` return value)

| Proc | ALPHA call (file:line, args) | OUR call | ALPHA DB params | OUR DB params | Verdict |
|---|---|---|---|---|---|
| ACCOUNT_LOGIN | DBProcess.cpp:89 (?=2) | DBProcess.cpp:89 (?=2) | 2: @strAccountID varchar(21), @strPasswd varchar(28) | 2: @strAccountID varchar(21), @strPasswd varchar(28) | ok |
| ACCOUNT_LOGOUT | DBAgent.cpp:1615 (1) | DBAgent.cpp:1504 (1) | 1: @strAccountID varchar(21) | 1: @strAccountID varchar(21) | ok |
| ACCOUNT_NATION_TRANSFER | DBAgent.cpp:2209 (?=1) | DBAgent.cpp:1967 (?=1) | 5: @AccountID char(21), @NRace1 int, @NRace2 int, @NRace3 int, @NRace4 int | **absent** | ALPHA→ALPHA DB: arity 1≠5; ALPHA→OUR DB: missing; OUR→OUR DB: missing; OUR→ALPHA DB: arity 1≠5 |
| ACCOUNT_PREMIUM | DBProcess.cpp:105 (?=1) | DBProcess.cpp:105 (?=1) | 1: @strAccountID char(20) | 1: @strAccountID char(20) | ok |
| CHANGE_HAIR | DBAgent.cpp:220 (?=5) | DBAgent.cpp:218 (?=5) | 3: @strCharID varchar(21), @bFace tinyint, @nHair int | 5: @strAccountID varchar(21), @strCharID varchar(21), @bType tinyint, @bFace tinyint, @nHair int | ALPHA→ALPHA DB: arity 5≠3; OUR→ALPHA DB: arity 5≠3 |
| CHANGE_NEW_CLANID | DBAgent.cpp:1551 (?=2) | — | 2: @strClanID varchar(30), @strNewClanID varchar(30) | **absent** | ALPHA→OUR DB: missing |
| CHANGE_NEW_ID | DBAgent.cpp:1533 (?=5) | DBAgent.cpp:1440 (?=5) | 5: @byType char(21), @AccountID char(21), @OldCharID char(21), @NewCharID char(21), @nRet smallint OUT | 5: @byType char(21), @AccountID char(21), @OldCharID char(21), @NewCharID char(21), @nRet smallint OUT | ok |
| CLEAR_REMAIN_USERS | DBAgent.cpp:2162 (1) | DBAgent.cpp:1920 (1) | 1: @strServerIP varchar(50) | 1: @strServerIP varchar(50) | ok |
| CREATE_KNIGHTS | DBAgent.cpp:1122 (?=5) | DBAgent.cpp:1094 (?=5) | 5: @sClanID smallint, @bNation tinyint, @bFlag tinyint, @strKnightsName char(21), @strChief char(21) | 5: @sClanID smallint, @bNation tinyint, @bFlag tinyint, @strKnightsName char(21), @strChief char(21) | ok |
| CREATE_NEW_CHAR | DBAgent.cpp:202 (?=12) | DBAgent.cpp:200 (?=12) | 12 params | 12 params | ok |
| DELETE_CHAR | DBAgent.cpp:239 (?=4) | DBAgent.cpp:237 (?=4) | **absent** | **absent** | ALPHA→ALPHA DB: missing; ALPHA→OUR DB: missing; OUR→OUR DB: missing; OUR→ALPHA DB: missing |
| DELETE_FRIEND_LIST | DBAgent.cpp:961 (?=2) | DBAgent.cpp:910 (?=2) | 2: @strUserID char(21), @strFriend char(21) | 2: @strUserID char(21), @strFriend char(21) | ok |
| DELETE_KNIGHTS | DBAgent.cpp:1166 (?=1) | DBAgent.cpp:1136 (?=1) | 1: @sClanID smallint | 1: @sClanID smallint | ok |
| DONATE_CLAN_POINTS | DBAgent.cpp:1381 (3) | DBAgent.cpp:1320 (3) | 3: @strUserID char(21), @sClanID smallint, @nNationalPoints int | 3: @strUserID char(21), @sClanID smallint, @nNationalPoints int | ok |
| GAME_LOGIN | DBAgent.cpp:95 (?=2) | DBAgent.cpp:96 (?=2) | 2: @strAccountID varchar(21), @strPasswd varchar(28) | 2: @strAccountID varchar(21), @strPasswd varchar(28) | ok |
| INSERT_FRIEND_LIST | DBAgent.cpp:943 (?=2) | DBAgent.cpp:892 (?=2) | 2: @strUserID char(21), @strFriend char(21) | 2: @strUserID char(21), @strFriend char(21) | ok |
| INSERT_USER_DAILY_OP | DBAgent.cpp:2173 (9) | DBAgent.cpp:1931 (9) | 9 params | 9 params | ok |
| INSERT_USER_SEALED | DBAgent.cpp:2426 (15) | — | 15 params | **absent** | ALPHA→OUR DB: missing |
| KING_CANDIDACY_NOTICE_BOARD_PROC | DBAgent.cpp:2031 (3) | DBAgent.cpp:1820 (4) | 4: @strCharID char(21), @sNoticeLen smallint, @byNation tinyint, @strNotice binary(1024) | 4: @strUserId char(21), @sNoticeLen smallint, @byNation tinyint, @strNotice varbinary(1024) | ALPHA→ALPHA DB: arity 3≠4; ALPHA→OUR DB: arity 3≠4 |
| KING_CANDIDACY_RECOMMEND | DBAgent.cpp:1913 (?=3) | DBAgent.cpp:1788 (?=4) | 4: @CharID_1 char(21), @CharID_2 char(21), @nNation tinyint, @nRet smallint OUT | 4: @CharID_1 char(21), @CharID_2 char(21), @nNation tinyint, @nRet smallint OUT | ALPHA→ALPHA DB: arity 3≠4; ALPHA→OUR DB: arity 3≠4 |
| KING_CHANGE_TAX | DBAgent.cpp:2093 (?=4) | — | 8 params | 8 params | ALPHA→ALPHA DB: arity 4≠8; ALPHA→OUR DB: arity 4≠8 |
| KING_ELECTION_PROC | DBAgent.cpp:1940 (?=4) | DBAgent.cpp:1732 (?=5); DBAgent.cpp:1752 (?=5) | 4: @strAccountID char(21), @strCharID char(21), @byNation tinyint, @strCandidacyID char(21) | 5: @strAccountID char(21), @strCharID char(21), @byNation tinyint, @strCandidacyID char(21), @nRet smallint OUT | ALPHA→OUR DB: arity 4≠5; OUR→ALPHA DB: arity 5≠4 |
| KING_ELECTION_RESULTS | DBAgent.cpp:1960 (1) | — | **absent** | **absent** | ALPHA→ALPHA DB: missing; ALPHA→OUR DB: missing |
| KING_INSERT_PRIZE_EVENT | DBAgent.cpp:2077 (?=4) | DBAgent.cpp:1843 (4) | 4: @byType tinyint, @byNation tinyint, @nAmount int, @strUserId char(21) | 4: @byType tinyint, @byNation tinyint, @nAmount int, @strUserId char(21) | ok |
| KING_UPDATE_DATABASE | DBAgent.cpp:2060 (?=3) | — | **absent** | **absent** | ALPHA→ALPHA DB: missing; ALPHA→OUR DB: missing |
| KING_UPDATE_ELECTION_LIST | DBAgent.cpp:1884 (6) | — | 6: @byDBType tinyint, @byType tinyint, @byNation tinyint, @nKnights smallint, @nAmount int, @strUserId char(21) | 6: @byDBType tinyint, @byType tinyint, @byNation tinyint, @nKnights smallint, @nAmount int, @strUserId char(21) | ok |
| KING_UPDATE_ELECTION_STATUS | DBAgent.cpp:1822 (2) | DBAgent.cpp:1705 (2) | 2: @byType tinyint, @byNation tinyint | 2: @byType tinyint, @byNation tinyint | ok |
| KING_UPDATE_NOAH_OR_EXP_EVENT | DBAgent.cpp:2044 (7) | DBAgent.cpp:1831 (7) | 7 params | 7 params | ok |
| LOAD_PREMIUM_SERVICE_USER | DBAgent.cpp:685 (3) | DBAgent.cpp:639 (3) | 3: @strAccountID char(20), @bType tinyint OUT, @sTime smallint OUT | 3: @strAccountID char(20), @bType tinyint OUT, @sTime smallint OUT | ok |
| LOAD_RENTAL_DATA | DBAgent.cpp:252 (1) | DBAgent.cpp:250 (1) | 1: @strAccountID char(21) | 1: @strAccountID char(21) | ok |
| LOAD_SEAL_PASSWD | DBAgent.cpp:2521 (1) | — | 1: @strAccountID varchar(21) | **absent** | ALPHA→OUR DB: missing |
| LOAD_SEAL_USER | DBAgent.cpp:2378 (1) | — | 1: @strCharID varchar(21) | **absent** | ALPHA→OUR DB: missing |
| LOAD_USER_DATA | DBAgent.cpp:341 (2) | DBAgent.cpp:340 (2) | 2: @strAccountID varchar(21), @strCharID varchar(21) | 2: @strAccountID varchar(21), @strCharID varchar(21) | ok |
| LOAD_WEB_ITEMMALL | DBAgent.cpp:796 (1) | DBAgent.cpp:749 (1) | 1: @strCharID char(21) | 1: @strCharID char(21) | ok |
| MAIL_BOX_CHECK_COUNT | DBAgent.cpp:1641 (?=1) | DBAgent.cpp:1530 (?=1) | 1: @strRecipientID varchar(21) | 1: @strRecipientID char(21) | ok |
| MAIL_BOX_GET_ITEM | DBAgent.cpp:1778 (?=2) | DBAgent.cpp:1666 (?=2) | 2: @strRecipientID varchar(21), @nLetterID int | 2: @strRecipientID char(21), @nLetterID int | ok |
| MAIL_BOX_READ | DBAgent.cpp:1756 (2) | DBAgent.cpp:1644 (2) | 2: @strRecipientID varchar(21), @nLetterID int | 2: @strRecipientID char(21), @nLetterID int | ok |
| MAIL_BOX_REQUEST_LIST | DBAgent.cpp:1659 (?=2) | DBAgent.cpp:1548 (?=2) | 2: @strRecipientID varchar(21), @bNewLettersOnly tinyint | 2: @strRecipientID char(21), @bNewLettersOnly tinyint | ok |
| MAIL_BOX_SEND | DBAgent.cpp:1739 (?=11) | DBAgent.cpp:1627 (?=10) | 11 params | 10 params | ALPHA→OUR DB: arity 11≠10; OUR→ALPHA DB: arity 10≠11 |
| NATION_SELECT | DBAgent.cpp:111 (?=2) | DBAgent.cpp:112 (?=2) | 2: @strAccountID varchar(21), @bNation tinyint | 2: @strAccountID varchar(21), @bNation tinyint | ok |
| RESET_LOYALTY_MONTHLY | DBAgent.cpp:2142 (0) | DBAgent.cpp:1900 (0) | 0:  | 0:  | ok |
| SAVE_PREMIUM_SERVICE_USER | DBAgent.cpp:878 (4) | DBAgent.cpp:827 (4) | 4: @strAccountID char(20), @strCharID char(20), @bType tinyint, @sTime smallint | 4: @strAccountID char(20), @strCharID char(20), @bType tinyint, @sTime smallint | ok |
| SET_LOGIN_INFO | DBAgent.cpp:781 (?=6) | DBAgent.cpp:734 (?=6) | 6: @strAccountID varchar(20), @strCharID varchar(20), @nServerno smallint, @strServerIP varchar(50), @strClientIP varchar(50), @bInit tinyint | 6: @strAccountID varchar(20), @strCharID varchar(20), @nServerno smallint, @strServerIP varchar(50), @strClientIP varchar(50), @bInit tinyint | ok |
| SKILLSHORTCUT_SAVE | DBAgent.cpp:858 (3) | DBAgent.cpp:807 (3) | 3: @strCharID varchar(21), @nCount smallint, @strSkillData varchar(260) | 3: @strCharID varchar(21), @nCount smallint, @strSkillData varchar(260) | ok |
| UPDATE_ACCOUNT_CHAR | DBAgent.cpp:2507 (3) | — | 3: @strAccountID varchar(21), @strUserID varchar(21), @bRent tinyint | **absent** | ALPHA→OUR DB: missing |
| UPDATE_KNIGHTS | DBAgent.cpp:1138 (?=4) | DBAgent.cpp:1110 (?=4) | 4: @bType tinyint, @strCharID char(21), @sClanID smallint, @bDomination tinyint | 4: @bType tinyint, @strCharID char(21), @sClanID smallint, @bDomination tinyint | ok |
| UPDATE_KNIGHTS_ALLIANCE | DBAgent.cpp:1105 (5) | DBAgent.cpp:1035 (5); DBAgent.cpp:1049 (5); DBAgent.cpp:1063 (5); DBAgent.cpp:1077 (5) | 5: @byType tinyint, @shAlliancIndex smallint, @shKnightsIndex smallint, @byEmptyIndex tinyint, @bySiegeFlag tinyint | 5: @byType tinyint, @shAlliancIndex smallint, @shKnightsIndex smallint, @byEmptyIndex tinyint, @bySiegeFlag tinyint | ok |
| UPDATE_KNIGHT_CASH | DBAgent.cpp:2228 (2) | DBAgent.cpp:1986 (2) | 2: @strAccountID char(21), @KnightCash int | **absent** | ALPHA→OUR DB: missing; OUR→OUR DB: missing |
| UPDATE_RANKS | DBAgent.cpp:2195 (0) | DBAgent.cpp:1953 (0) | 0:  | 0:  | ok |
| UPDATE_SAVED_MAGIC | DBAgent.cpp:757 (21) | DBAgent.cpp:710 (21) | 21 params | 21 params | ok |
| UPDATE_SIEGE | DBAgent.cpp:2238 (6) | DBAgent.cpp:1996 (6) | 6: @sCastleIndex smallint, @sKnightsIndex smallint, @byWarType tinyint, @byWarDay tinyint, @byWarTime tinyint, @byWarMinute tinyint | 6: @sCastleIndex smallint, @sKnightsIndex smallint, @byWarType tinyint, @byWarDay tinyint, @byWarTime tinyint, @byWarMinute tinyint | ok |
| UPDATE_USER_DAILY_OP | DBAgent.cpp:2185 (3) | DBAgent.cpp:1943 (3) | 3: @strCharID char(21), @bType tinyint, @iUnixTime int | 3: @strCharID char(21), @bType tinyint, @iUnixTime int | ok |
| UPDATE_USER_DATA | DBAgent.cpp:1028 (43) | DBAgent.cpp:962 (39) | 43 params | 39 params | ALPHA→OUR DB: arity 43≠39; OUR→ALPHA DB: arity 39≠43 |
| USER_ITEM_SEAL | DBAgent.cpp:898 (?=6) | DBAgent.cpp:847 (?=6) | 6: @strAccountID char(21), @strCharID char(21), @strPasswd char(8), @nItemSerial bigint, @nItemID int, @bSealType tinyint | 6: @strAccountID char(21), @strCharID char(21), @strPasswd char(8), @nItemSerial bigint, @nItemID int, @bSealType tinyint | ok |

The parameter lists shown as counts (CREATE_NEW_CHAR 12, INSERT_USER_DAILY_OP 9, KING_* 7–8, UPDATE_SAVED_MAGIC 21, MAIL_BOX_SEND, UPDATE_USER_DATA) are spelled out in `work/proc_diff_real.txt`. The UPDATE_USER_DATA and MAIL_BOX_SEND parameter lists are also given in 3.3.

**Result-set contracts that changed even though the arity is the same [D/V]:**
- `LOAD_USER_DATA`. ALPHA (`DBAgent.cpp:353-397`) fetches, in order:
  - Nation, Race, Class, HairRGB **(UInt32)**, Rank, Title, Level, Exp, Loyalty, Face, City, Knights, Fame, Hp, Mp, Sp, Str, Sta, Dex, Intel, Cha, Authority, Points, Gold, Zone, Bind, PX, PZ, PY, dwTime, strSkill;
  - then strItem[592], strSerial[592], **strUserSeal[296], strItemTime[296]**, sQuestCount, strQuest[3888], **sQuestDataCount, strQuestData[1296]**, MannerPoint, LoyaltyMonthly, **strMemo**.

  OUR code (`DBAgent.cpp:352-392`) expects the same fields through strSerial, then sQuestCount, strQuest[600], MannerPoint, LoyaltyMonthly, strItemTime[584]. Each DB's LOAD_USER_DATA returns exactly its own server's shape. OUR LOAD_USER_DATA also runs `UPDATE … Class+1` and `EXEC UPDATE_USER_KNIGHTS_RANK / _PERSONAL_RANK / UPDATE_KNIGHTS_RATING` on every load; ALPHA's does neither.
- `LOAD_WEB_ITEMMALL`: ALPHA fetches a 5th column `itemtime` (`DBAgent.cpp` LoadWebItemMall). Only ALPHA's procedure returns it, but the WEB_ITEMMALL column exists in both DBs.
- `MAIL_BOX_GET_ITEM`: ALPHA fetches column 6 (`nUserSeal`), but **ALPHA's own procedure returns only 5 columns**.

### 2.4 ALPHA source vs ALPHA DB: internal mismatches [V]

| Object | ALPHA call | ALPHA DB | Effect |
|---|---|---|---|
| CHANGE_HAIR | 5 args (`:220`) | 3 params (`@strCharID, @bFace, @nHair`) | error ("too many arguments"). **OUR 5-param version matches ALPHA's call.** |
| KING_CANDIDACY_RECOMMEND | 3 args (`:1913`) | 4 params, `@nRet OUTPUT` with no default | error |
| KING_CANDIDACY_NOTICE_BOARD_PROC | 3 args `(?, %d, ?)` (`:2031`) | 4 params (`@strCharID, @sNoticeLen, @byNation, @strNotice`) | error; the order is also wrong |
| KING_CHANGE_TAX | 4 args (`:2093`) | 8 params | error |
| ACCOUNT_NATION_TRANSFER | 1 arg (`:2209`) | 5 params | error |
| KING_ELECTION_RESULTS, KING_UPDATE_DATABASE, DELETE_CHAR | `:1960`, `:2060`, `:239` | absent | error (DELETE_CHAR is absent in OUR DB too) |
| SERVER_LIST (UpdateNationIntro) | `:2111` | absent | error (OUR DB lacks it too) |
| MAIL_BOX_GET_ITEM | fetches column 6 | 5 columns | nUserSeal is never read |
| UPDATE_USER_DATA | `@strItem` 592 B, `@strItemTime`/`@strUserSeal` 296 B | columns are 584 / 292 / 292 | truncation on save; see section 5 |

---

## 3. Real schema diff: FDP_alpha1534 (2019) vs FDP_kn_online (2017) [V]

Instances: ALPHA = SQL Server 2019 RTM 15.0.2000.5 Express, DB version 904, compat **130**, 54 MB. OURS = SQL Server 2017 RTM 14.0.1000.169 Express, DB version 869, compat **100**. Both use collation `SQL_Latin1_General_CP1_CI_AS`.

### 3.1 Tables
- **Only in ALPHA (23):** ACCOUNT_CHAR_VIEW (view), ACCOUNT_NATION_TRANSFERS_QUEUE, DELLOS_TAX, EVENT_SCHEDULER, **FISHING_ITEM**, **GAME_OPTIONS**, ITEM_EXCHANGE1, **ITEM_EXCHANGE_CRASH**, **ITEM_EXCHANGE_EXP**, **KING_NOMINATION_LIST**, K_NPC_ITEM, **MINING_ITEM**, **MONSTER_JURAID_MOUNTAIN_RESPAWN_LIST**, MONSTER_KILL_NOTICE, **MONSTER_STONE_RESPAWN_LIST**, PET_DATA, **PREMIUM_ITEM_LETTER_GIFT**, PREMIUM_SERVICE_USER, **USER_SEAL**, VIP_WAREHOUSE, VOTE_BUTTONS, VOTE_REWARDS, VOTE_REWARD_LOGS. Bold = needed by ALPHA's source, either by a loader or by a procedure that ALPHA calls (GAME_LOGIN → GAME_OPTIONS + `dbo.IsBanned`; SAVE_PREMIUM_SERVICE_USER → PREMIUM_ITEM_LETTER_GIFT; INSERT_USER_SEALED → USER_SEAL). The rest are unused by the source.
- **Only in OURS (33):**
  - Project-made: BOT_NICK_MAP (db/009), MAGIC_BAK_etc (db/001), K_NPCPOS_BOWL_BACKUP (bowl edit), and 10 USERDATA_*_BACKUP tables (db/002–008 and tests).
  - Legacy web/payment (PUS_*, _SN_*, SN_NEWS*, w_*, EnesKCBSNotice, SERVERS).
  - BEGINNER_ITEM, K_OBJECTEVENT, K_WARPINFO, QUEST_LOG/MENU/TALK.

  OUR source loads none of these at startup.
- **Identical (55)**: same columns and types.

### 3.2 Column/type differences in common tables (28 tables; full list in `work/table_diff_real.txt`)

| Table | Only ALPHA | Only OURS | Type ours→ALPHA | Matters for |
|---|---|---|---|---|
| USERDATA | strUserSeal binary(292), sQuestDataCount smallint, strQuestData binary(1296), iSavedCONT int, strMemo char(21) | – | strItemTime binary(584)→**292**; strQuest binary(600)→**3888**; column order differs | ALPHA load/save; bot scripts |
| WAREHOUSE | strUserSeal binary(768) | – | WarehouseDataTime 1536→**768** | ALPHA warehouse |
| KNIGHTS | strAllianceNotice char(128) | – | – | ALPHA KnightsSet + UpdateAllianceNotice |
| ITEM | UpgradeNotice tinyint, NPbuyPrice int, Bound tinyint | – | – | ALPHA ItemTableSet (Bound unused) |
| KING_SYSTEM | sKingClanID smallint | – | – | ALPHA KingSystemSet |
| MAKE_ITEM_GROUP | iItem_31..iItem_100 int (70) | – | – | ALPHA AIServer (req) |
| MONSTER_RESPAWN_LIST / _INFORMATION | sType tinyint / byDirection tinyint | – | – | ALPHA loaders |
| MAIL_BOX | nUserSeal bigint NOT NULL DEFAULT 0 | – | 4 char→varchar(50/128) | MAIL_BOX_SEND |
| MAIL_ITEM | – | – | char→varchar | – |
| TB_USER | Email, SecurityQuestion/Answer, **strAuthority** tinyint, UserName/Surname, BonusCashPoint, UserPhoneNumber, _KnightCash, _GiftPoint | sepet, StrSoru, strCevap, strAd, strSoyad | strPasswd varchar(28)→64 | ALPHA GAME_LOGIN reads strAuthority |
| K_NPC / K_MONSTER | – | sLightR, byMoneyType | K_MONSTER.strName varchar(30)→20 | not loaded |
| K_NPCPOS | – | – | byDirection tinyint→int; order differs | harmless (named columns) |
| MAGIC | – | – | EnName/KrName char(250)→200, Description 1000→200, **SelfEffect int→tinyint**, UseStanding tinyint→smallint; order differs | data (see 3.5) |
| MAGIC_TYPE1, 5–9 | – | – | Name/Description char shorter | – |
| MAGIC_TYPE3 / 4 | – | Name, Description nchar(500) | TYPE4.ExpPct smallint→tinyint | – |
| SET_ITEM (`SELECT *`) | Unk120 | Unk20, Unk21 | – | loader fetches the first 24 columns only, which are identical |
| EVENT, ITEM_UPGRADE | – | – | char→varchar | – |
| USER_ITEMS | – | – | int/nchar(586)→smallint/smallint | empty in ALPHA |
| VERSION | – | strCompressName | strFilename varchar(50)→char(40) | – |

Nullability and defaults of the new ALPHA columns [V]: all are nullable or defaulted (`sQuestDataCount`/`iSavedCONT` default 0, `strItemTime` default 0, `strMemo` default `'SRGAME'`, `MAIL_BOX.nUserSeal` default 0). Column-list INSERTs written for OUR schema therefore still succeed column-wise.

### 3.3 Procedures and functions (user objects: ALPHA 108 P + 10 FN, OURS ~94 P + FN) [V]
- **Only in ALPHA (30):** ACCOUNT_NATION_TRANSFER, CHANGE_HAIR_INGAME, **CHANGE_NEW_CLANID**, CLEAR_REMAIN_CURRENTUSER, **DB_ROLLBACKFORMAT** (TRUNCATEs every player table: TB_USER, USERDATA, KNIGHTS*, WAREHOUSE, MAIL_*…; never run it), EditSkillPoints, GetNewRace, INMOB_NPC, INSERT_CURRENTUSER, **INSERT_USER_SEALED**, **IsBanned** (fn), isInClan, IsKing, isNationSelected, IsUserOnlineGame, ITEM_UPGRADE_RATE, King_Demote, King_user, **LOAD_SEAL_PASSWD**, **LOAD_SEAL_USER**, Res, **UPDATE_ACCOUNT_CHAR**, UPDATE_ALL, **UPDATE_KNIGHT_CASH**, UPDATE_PET_DATA, UPDATE_PET_DELETE, UPDATE_QUEST_LOG, UPDATE_USER_DATA_QUEST, VIP_STORAGE_PASSWORD, VIP_STORAGE_USE_VAULTKEY. Bold = called by ALPHA's source, directly or via GAME_LOGIN.
- **Only in OURS (21):** ADD_KESN_CODES, ADD_KNIGHT_CASH, CHANGE_KNIGHTS_LEADER, CREATE_INSERT_SCRIPT, DELETE_ACCOUNT, DELETE_USER, EDIT_QUEST, EDIT_SKILLS, INSERTITEM, INSERT_UPGRADE, ITEMLERI_BUL_BANKA, ITEMLERI_ENCODE_BANKA, KNIGHTS_RATING_UPDATE, KRAL_EKLE, MAIN_LOGIN, OTO_RESET, PROC_Enes_Online, PROC_INSERT_CURRENTUSER, UPDATE_USER_STYLE, VIEW_QUESTS, VIEW_SKILLS. None is called by either source.
- **Signature differences (14):**
  - UPDATE_USER_DATA: OURS has 39 params. ALPHA has 43: `@sQuestDataCount` inserted at position 33; `@strItem`/`@strSerial` binary(592); `+@strUserSeal` binary(296); `@strItemTime` binary(296) moved before strQuest; `@strQuest` binary(3888); `+@strQuestData` binary(1296); `+@strMemo` char(21) last.
  - MAIL_BOX_SEND: `+@nUserSeal bigint` before `@nCoins`.
  - KING_ELECTION_PROC: ALPHA drops `@nRet OUTPUT`.
  - CHANGE_HAIR: 5 params in OURS, 3 in ALPHA.
  - UPDATE_KNIGHTS_CAPE: OURS `@sCapeC/R/G/B smallint` vs ALPHA `@bCapeR/G/B tinyint`. **OUR body updates `sCapeC/sCapeR…`, which do not exist in OUR KNIGHTS, so OUR version is broken.** Neither source calls it.
  - UPDATE_WAREHOUSE: `@strItemTime` 1536→768.
  - SHOPPINGMALL_BUY: `+@sItemTime`.
  - KING_CANDIDACY_NOTICE_BOARD_PROC: param name and binary vs varbinary.
  - MAIL_BOX_CHECK_COUNT / GET_ITEM / READ / REQUEST_LIST / MAIL_TAKE_LETTERITEM: char(21)→varchar(21) only.
  - UPDATE_BATTLE_RESULT: name and length only.
- **Same signature (74)**, including GAME_LOGIN, ACCOUNT_LOGIN, CREATE_NEW_CHAR, LOAD_CHAR_INFO, CREATE_KNIGHTS, DONATE_CLAN_POINTS, UPDATE_KNIGHTS(_ALLIANCE), LOAD_USER_DATA (parameters only), NATION_SELECT, SET_LOGIN_INFO.

### 3.4 Body differences of server-called procedures (comments and whitespace ignored; full diffs in `work/proc_body_diffs.txt`) [V]

| Procedure | Result |
|---|---|
| ACCOUNT_LOGIN, CLEAR_REMAIN_USERS, INSERT_FRIEND_LIST, INSERT_USER_DAILY_OP, KING_CANDIDACY_RECOMMEND, KING_CHANGE_TAX, KING_INSERT_PRIZE_EVENT, KING_UPDATE_ELECTION_LIST, KING_UPDATE_NOAH_OR_EXP_EVENT, **LOAD_CHAR_INFO**, LOAD_KNIGHTS_MEMBERS, LOAD_PREMIUM_SERVICE_USER, LOAD_RENTAL_DATA, LOAD_SEALED_ITEM_DATA, RESET_LOYALTY_MONTHLY, UPDATE_RANKS, UPDATE_SAVED_MAGIC, UPDATE_SIEGE, UPDATE_USER_DAILY_OP, USER_ITEM_SEAL | identical |
| ACCOUNT_LOGOUT, ACCOUNT_PREMIUM, DELETE_FRIEND_LIST, SET_LOGIN_INFO, SKILLSHORTCUT_SAVE | only `[dbo].` qualification differs |
| **GAME_LOGIN** | OURS: password compare only. ALPHA adds: reject when `TB_USER.strAuthority = 9`; `dbo.IsBanned` (returns 18); a GAME_OPTIONS maintenance flag (returns 6); a capacity check from GAME_OPTIONS.FreeLimit/PremiumLimit against CURRENTUSER (returns 21). It needs `TB_USER.strAuthority`, which OUR TB_USER lacks. |
| **LOAD_USER_DATA** | Result shape (2.3). OURS also auto-promotes class at level > 59 and runs the 3 rank procedures on every load. |
| **UPDATE_USER_DATA** | Adds strUserSeal, sQuestDataCount, strQuestData and strMemo (3.3). |
| **CREATE_NEW_CHAR** | OURS ends with `EXEC GIVE_BEGINNER_ITEM` (BEGINNER_ITEM table). ALPHA has it commented out; ALPHA's GIVE_BEGINNER_ITEM references the missing BEGINNER_ITEM. |
| **CREATE_KNIGHTS** | OURS rejects a duplicate IDNum **or** name; ALPHA rejects only a duplicate name. |
| **DONATE_CLAN_POINTS** | OURS uses a transaction with rollback; ALPHA uses two plain UPDATEs. |
| **DELETE_KNIGHTS** | OURS uses a transaction and returns 7 if missing. ALPHA also cleans KNIGHTS_ALLIANCE (main/sub/mercenary) and KNIGHTS.sAllianceKnights. |
| UPDATE_KNIGHTS | On leave/kick, OURS sets `Knights = -1` and ALPHA sets `Knights = 0`. |
| UPDATE_KNIGHTS_ALLIANCE | formatting only |
| UPDATE_KNIGHTS_CAPE | see 3.3 (OURS is broken; not called) |
| CHANGE_NEW_ID | ALPHA also renames in BATTLE, USER_DAILY_OP and MAIL_BOX, and deletes USER_SAVED_MAGIC. OURS renames in KING_SYSTEM/KING_ELECTION_LIST. |
| NATION_SELECT | ALPHA's WAREHOUSE insert also fills strUserSeal and WarehouseDataTime. |
| MAIL_BOX_SEND / READ / GET_ITEM / REQUEST_LIST / CHECK_COUNT | nUserSeal, varchar parameters and formatting |
| SAVE_PREMIUM_SERVICE_USER | ALPHA also mails the PREMIUM_ITEM_LETTER_GIFT items. |
| LOAD_WEB_ITEMMALL | `+itemtime` column |
| UPDATE_WAREHOUSE | ALPHA also runs `UPDATE WAREHOUSE SET WarehouseData = <1536-byte starter-bank blob> WHERE WarehouseData = 0x00` (a starter-bank fill). Not called by either source. |
| LOAD_ACCOUNT_CHARID | OURS calls the missing CHECK_USER_JOB_CHANGE_QUEUE and reads strCharID4/5 (absent), so OURS is broken. Not called. |
| KING_ELECTION_PROC, KING_UPDATE_ELECTION_STATUS, KING_CANDIDACY_NOTICE_BOARD_PROC | rewritten |

### 3.5 Reference data [V]

Row counts:

| Table | ALPHA | OURS |
|---|---|---|
| ITEM | 123,720 | 85,920 (85,844 common; 76 only ours; 37,876 only ALPHA) |
| MAGIC | 1,886 (50 IDs only in ALPHA) | 1,839 (3 IDs only in OURS) |
| MAGIC_TYPE1 / 3 / 4 | 216 / 750 / 692 (only ALPHA: 4 / 43 / 66) | 203 / 707 / 627 (only OURS: 7 / 0 / 1) |
| KNIGHTS_CAPE | 224 | 56 |
| LEVEL_UP | max level 83 (Exp@80 = 405,802,438) | max level 80 (Exp@80 = 1,898,706,631) |
| K_NPCPOS zone 71 | 296 | 209 (OURS has the bowl deletion, STATUS B4) |
| MAGIC rows with Etc 510–523 (quest-gated skills) | **0** | 72 |
| MAGIC rows with Etc = 1 | 0 | 0 |

Personal tables in ALPHA (counts only): TB_USER 1, ACCOUNT_CHAR 1, USERDATA 1, WAREHOUSE 1, KNIGHTS 1, KNIGHTS_USER 1, MAIL_BOX 5, USER_SEAL 0, CURRENTUSER 0. This is seed data from the distributor.

**MAGIC for bot skills.** The set is 124 IDs: the `bots/config/skill_*.txt` cast IDs (50 Karus), their El Morad mirrors (+100000) and the docs/05 IDs that exist in MAGIC. Full output: `work/magic_cmp.txt`.

| Table | Identical | Differ | Missing in ALPHA | Columns that differ |
|---|---|---|---|---|
| MAGIC | 0 | **124** | 0 | SelfEffect 124 (ALPHA = 0 everywhere, since the column is tinyint there), Range 89 (ALPHA ≈ ×0.45: 56→25, 78→35, 22→10, 225→100, 22500→10000; 90→25 for Fire/Ice burst), ReCastTime 49 (e.g. Outrage/Frenzy 91→600, Exceed Break 254→600, Shock Stun 252→300, Mana Shield 0→800, Instantly Magic 255→1800, Curse Refraction/Elysian Web 1→1800, many heals/buffs 1→0), FlyingEffect 10, UseStanding 7 (Howling Sword, incineration, meteor fall, prismatic: 51/53→0), CastTime 1 (Mana Shield 0→15) |
| MAGIC_TYPE1 | 8 | 16 | 0 | Hit/AddDamage (Cleave 150→250, El Morad 125→250; Sword aura Hit 100→200, Add 250→150; Sword dancing 150→250; Howling Sword 200→300; Scream Add 200→150; Shock Stun 175/175→200/0; Judgment Hit 500→200; Helis Add 400→0) |
| MAGIC_TYPE3 | 43 | 11 | 0 | EndDamage −304/−900/−1050/−213/−630→0 (Inferno, Supernova, Meteor Fall, Blizzard, Frost nova); Great healing FirstDamage 960→720 |
| MAGIC_TYPE4 | 0 | 51 | 0 | ExpPct 100→0 and SpecialAmount 0→NULL on all; BuffType 6→40 (slows: leg cutting, Scream, Ice arrow/burst/comet, Blizzard, Frost nova, Prismatic); 5→31 (Outrage, Frenzy); Outrage/Frenzy Duration 30→10/20, AttackSpeed 120/130→100 |

Over all 1,191 class and consumable IDs from `docs/appendix/data/*.csv`: MAGIC differs in 1,189 rows; 490121 and 490131 are missing in ALPHA. TYPE1 differs in 80 of 158, TYPE3 in 91 of 495, TYPE4 in 482 of 482. Both servers use the same BuffType enum (`GameDefine.h`: 5 = ATTACK_SPEED, 6 = SPEED, 31 = MANA_ABSORB "Outrage/Frenzy", 40 = SPEED2) and the same `sRange` check (`MagicInstance.cpp`), so the data difference changes behaviour directly [D]. Our `docs/appendix/data/skills_*.csv` (which docs/05 cites) match OUR DB, not ALPHA's.

**ITEM for bot gear.** 83 item IDs appear in db/002, 004, 005, 006, 007 and 008. All exist in both DBs, and **0 are identical**:
- `ItemClass` is NULL in ALPHA for all 123,720 rows; OURS has 3, 4 or 8 on the gear.
- db/007 reverse armour (216/276/296xxx005/011): ReqLevel 1→60, ReqLevelMax 107→99, ReqDex/ReqIntel/ReqCha 0→14 or 16 (mage set ReqStr/ReqDex 0→14/16, ReqIntel +14/16), Effect1 5/11→0, BuyPrice, Duration −1, Droprate→0.
- Weapons 135751101 / 149111071 / 156211041 / 190251131 / 170250267: ReqLevel 1→30/40/60/8/12 and Effect1 11→0.
- Accessories 31x/32x/33x/34x: ItemExt 18–21→0.
- Scrolls 800014000/15000/76000/78000: Race 20→19 and BuyPrice 2,000→50,000,000; 800076000/78000 also ReqLevel 1→62.

Across all common ITEM rows: ItemClass differs in 85,844, Droprate in 78,237, ReqLevel in 75,494, ReqLevelMax in 74,431 and BuyPrice in 63,846. OUR ITEM is a modified table (ReqLevel lowered to 1) [V]. Both servers use ItemClass in `UpgradeHandler.cpp` (ALPHA :408-416 maps scroll classes 1/2/3/4/8; OURS :265/275), so ALPHA's NULLs break upgrade-class logic in either server [D].

**KNIGHTS_CAPE for the db/009 capes** (103, 206, 303, 509): present in both DBs with the same price and grade 1. ALPHA has `byRanking = 2`, OURS `0`.

---

## 4. Source diff: what each server reads (could one DB serve both?)

**ALPHA reads, OURS does not [D]:**
- Loaders for FISHING_ITEM, MINING_ITEM, ITEM_EXCHANGE_EXP, ITEM_EXCHANGE_CRASH, KING_NOMINATION_LIST, MONSTER_STONE_RESPAWN_LIST, MONSTER_JURAID_MOUNTAIN_RESPAWN_LIST and USER_SEAL.
- New columns: ITEM.UpgradeNotice/NPbuyPrice, KING_SYSTEM.sKingClanID, KNIGHTS.strAllianceNotice, MAKE_ITEM_GROUP iItem_31–100, MONSTER_RESPAWN_LIST.sType, MONSTER_RESPAWN_LIST_INFORMATION.byDirection.
- USERDATA strUserSeal/sQuestDataCount/strQuestData/strMemo (via LOAD_USER_DATA, UPDATE_USER_DATA and the direct SELECT at `:1179`), WAREHOUSE.strUserSeal, MAIL_BOX.nUserSeal, WEB_ITEMMALL.itemtime.
- New DBAgent functions: CharacterSealSave/Delete, LoadSealInfo, LoadSealPasswd (character seal), UpdateAccountChar, UpdateCharacterClanName, LoadCapeId, LoadAllianceInfo, LoadLadderRankList, LoadKnightsLeaderList, GetClanIDWithCharID, UpdateAllianceNotice, UpdateElectionVoteList, GetElectionResults, UpdateKingSystemDB, UpdateNationIntro.

**OURS reads, ALPHA does not [D]:**
- CreateAlliance/InsertAlliance/RemoveAlliance/DestoryAlliance (all UPDATE_KNIGHTS_ALLIANCE; ALPHA folds them into UpdateAlliance).
- Direct KING_SYSTEM/KNIGHTS_SIEGE_WARFARE tax UPDATEs (InsertTaxEvent/InsertTaxUpEvent, `:1856-1886`; ALPHA uses KING_CHANGE_TAX instead).
- SendUDP_ElectionStatus `UPDATE KING_SYSTEM SET byType`.
- KING_ELECTION_PROC with 5 args.
- LoadKnightsAllMembers as a single `SELECT strUserID, Fame, Level, Class … WHERE Knights=%d`.

Every column OURS reads exists in ALPHA's DB: all OUR loaders pass against FDP_alpha1534 [V].

**Conclusion [V].** Tables can be one superset. Procedures cannot, for two reasons:
- The LOAD_USER_DATA result shape differs, and positional binding would mis-read strItemTime and the quests.
- UPDATE_USER_DATA (39 vs 43 params, new parameter inserted mid-list), MAIL_BOX_SEND (10 vs 11), KING_ELECTION_PROC (5 vs 4) and CHANGE_HAIR (ALPHA DB 3 vs both sources' 5) are positional and incompatible.

Running both servers against one DB needs a code change in one of them, such as renaming the procedures it calls (`*_1534`).

Existing OUR-side gaps found along the way [V]:
- OUR source calls `UPDATE_KNIGHT_CASH` (`DBAgent.cpp:1986`) and `DELETE_CHAR` (`:237`), but OUR DB has neither, so both calls already fail today.
- OUR `UPDATE_KNIGHTS_CAPE` and `LOAD_ACCOUNT_CHARID` reference objects that do not exist (not called).

---

## 5. Inventory and item blob layout

| | OURS (1453) `shared/globals.h:194-251` | ALPHA (1534) `shared/globals.h:225-282` |
|---|---|---|
| Equip | 0–13 (SLOT_MAX 14) | same |
| Inventory | 14–41 (HAVE_MAX 28) | same |
| Cospre | 42–46: CWING 42, CHELMET 43, CLEFT 44, CRIGHT 45, CTOP 46 (COSP_MAX 5) | 42–46 same, **CFAIRY 47** (COSP_MAX 8 covers 42–49) |
| Bag slots | BAG1 47, BAG2 48 | **BAG1 48, BAG2 49** |
| Magic bags | MBAG1 49–60, MBAG2 61–72 | **MBAG1 50–61, MBAG2 62–73** |
| INVENTORY_TOTAL | **73** | **74** |
| Item record | 8 B: uint32 id, int16 dur, int16 count | same |
| strItem / strSerial (code) | 73×8 = 584 B, 8 B serial | **74×8 = 592 B** |
| strItemTime (code) | uint32 per slot (`DBAgent.cpp:438`); buffer 73×8 | uint32 per slot, buffer 74×4 = 296 |
| strUserSeal | – | uint32 per slot, 74×4 = 296 (new) |
| strQuest | 600 B (200 quests × 3 B) | **3888 B** (1296 × 3 B); same 3-byte record |
| strQuestData | – | 1296 B (kill counters, ≤ 20 entries) |
| **DB columns** | strItem/strSerial binary(584), strItemTime binary(584), strQuest binary(600) | **strItem/strSerial binary(584)**, strUserSeal/strItemTime binary(292), strQuest binary(3888), strQuestData binary(1296) |
| UpdateUser writes | **only slots 0–46** (`DBAgent.cpp:941-955`; bags are never saved) | all 74 slots |

Consequences:
1. **Bot rows stay valid [V/D].** db/002/005/006 write slots 0–20, 004 writes 14–21, 007 writes 1–13 and 008 writes 22–39. All are below 42, so the bytes mean the same thing in ALPHA. The `strItem binary(584)` guards in 002/004/007 pass on ALPHA's DB because it is also 584.
2. **ALPHA saves 592-byte strItem/strSerial and 296-byte strUserSeal/strItemTime into 584/292 columns** (ALPHA's own `UPDATE_USER_DATA`). Slot 73 (the last MBAG2 slot) cannot be stored, and with `ANSI_WARNINGS ON` (the ODBC default) the UPDATE may raise error 8152 "String or binary data would be truncated" **[A]**. Whether SQL Server ignores trailing zero bytes was not tested, because testing needs an INSERT. If OUR DB is migrated, the clean choice is to widen these columns to 592/296 (matching `INVENTORY_TOTAL` 74) and relax the 584 guards. The alternative is to copy ALPHA's 584 as-is and accept the risk.
3. Converting existing OUR rows to ALPHA layout would need slots 47–72 shifted +1. Because OUR UpdateUser zero-pads beyond slot 46 on every save, rows saved by OUR server have zeros there. Only rows written by other tools (none of the bot scripts) could hold data at 47+ [D/A].
4. ALPHA tolerates a wider `strItemTime` (584) on read: `FetchBinary` = `SQLGetData` into a 296-byte buffer returns `SQL_SUCCESS_WITH_INFO`, which counts as success (`shared/database/OdbcCommand.cpp:205-208`) [D]. Narrowing OUR column is therefore optional.

---

## 6. Do our db/001–009 scripts work on each target?

The scripts live in the main repo `db/` (005–008) and `fdp-belge3/db/009` (`fdp-merge-final/db` only has 001–004).

| Script | On OUR DB migrated by (a) | On ALPHA DB (b) |
|---|---|---|
| 001 MAGIC Etc 1→0 | already applied | no-op (ALPHA has 0 rows with Etc = 1) |
| 002 / 005 / 006 characters | OK, if the strItem guard accepts the chosen width | **Fails or truncates:** inserts `strItemTime` as 584 bytes into binary(292) [A: error 8152 likely]. Must use 292, or `DEFAULT`. WAREHOUSE insert without strUserSeal → NULL (OK). Exp for level 80 comes from the target LEVEL_UP (ALPHA 405,802,438). |
| 003 quests | OK | Runs: strQuest 600 → 3888 pads, quest IDs 51/53/54/510–523 exist (12 rows each in QUEST_HELPER). But ALPHA MAGIC has no Etc 510–523 quest gating, so the script is unneeded. |
| 004 inventory | OK | OK (guard 584, slots 14–21; items exist) |
| 007 gear | OK | **Aborts:** the guard requires `ITEM.ItemClass = 4` for 216/276/296 and ALPHA has NULL. Also, ALPHA's armour now needs Dex/Int/Cha 14–16 and ReqLevel 60; the bots' stat builds must be re-checked (`ItemHandler.cpp:583-585` is the equip check). |
| 008 scrolls | OK | OK (slots 22–39; items exist; the Race and ReqLevel 62 changes matter only for level < 62) |
| 009 nicks and clans | OK | Columns OK (strAllianceNotice nullable), capes exist, BOT_NICK_MAP and USER_SAVED_MAGIC/USERDATA_SKILLSHORTCUT exist. **IDNum collision with ALPHA's 1 seed clan not checked** (KNIGHTS rows are off-limits). Points 800000 ≥ ALPHA `GRADE1=720000` (`GameServer.ini`). |
| Bowl spawn edit (STATUS B4, no script) | kept | **Lost.** ALPHA has 296 zone-71 rows vs our 209 (after the edit); a new delete list must be derived. |

---

## 7. Recommendation

### (a) Migrate FDP_kn_online forward (recommended)

Keeps SQL Server 2017 Express, the bot rows, the tuned MAGIC/ITEM data (docs/05 and the CSVs stay valid) and the bowl edit. All work is additive. Exact DDL and bodies can be copied from FDP_alpha1534 (`work/alpha_defs.txt`, INFORMATION_SCHEMA).

1. **M1 tables, with reference data copied from ALPHA:**
   - FISHING_ITEM (200 rows), MINING_ITEM (200), ITEM_EXCHANGE_EXP (194), ITEM_EXCHANGE_CRASH (220), MONSTER_STONE_RESPAWN_LIST (504), MONSTER_JURAID_MOUNTAIN_RESPAWN_LIST (136).
   - Empty: KING_NOMINATION_LIST, USER_SEAL.
   - Optional: GAME_OPTIONS (1 row) and PREMIUM_ITEM_LETTER_GIFT (16 rows), needed only if ALPHA's GAME_LOGIN and SAVE_PREMIUM_SERVICE_USER bodies are adopted.
   - Several loaders are "(req)" or need data to be useful. The respawn lists depend on 1534 maps and zones [A].
2. **M2 columns:**
   - USERDATA: +strUserSeal binary(292 or 296) NULL, +sQuestDataCount smallint NOT NULL DEFAULT 0, +strQuestData binary(1296) NULL, +iSavedCONT int NOT NULL DEFAULT 0, +strMemo char(21) NULL. Widen strQuest to binary(3888) (existing bytes are zero-padded). Optionally widen strItem/strSerial to 592 (see 5.2).
   - WAREHOUSE: +strUserSeal binary(768) NULL.
   - KNIGHTS: +strAllianceNotice char(128) NULL.
   - ITEM: +UpgradeNotice tinyint, +NPbuyPrice int, +Bound tinyint (DEFAULT 0).
   - KING_SYSTEM: +sKingClanID smallint DEFAULT 0.
   - MAKE_ITEM_GROUP: +iItem_31..100 int DEFAULT 0, plus ALPHA's 34 rows if 1534 craft groups are wanted.
   - MONSTER_RESPAWN_LIST: +sType tinyint. MONSTER_RESPAWN_LIST_INFORMATION: +byDirection tinyint.
   - MAIL_BOX: +nUserSeal bigint NOT NULL DEFAULT 0.
   - ADD appends columns at the end, which is harmless: no ALPHA access to these tables is positional.
3. **M3 procedures:**
   - **Replace:** LOAD_USER_DATA (ALPHA shape; decide whether to keep OUR per-login rank EXECs), UPDATE_USER_DATA (43 params), MAIL_BOX_SEND (11), KING_ELECTION_PROC (4), LOAD_WEB_ITEMMALL (+itemtime), NATION_SELECT (fill new warehouse columns).
   - **Create:** INSERT_USER_SEALED, LOAD_SEAL_USER, LOAD_SEAL_PASSWD (TB_USER.strSealPasswd exists in both), UPDATE_ACCOUNT_CHAR, CHANGE_NEW_CLANID, UPDATE_KNIGHT_CASH (TB_USER.CashPoint).
   - **Write correct versions for ALPHA's call shapes:** KING_CANDIDACY_RECOMMEND (`@nRet` default), KING_CANDIDACY_NOTICE_BOARD_PROC (3 args), KING_CHANGE_TAX (4), ACCOUNT_NATION_TRANSFER (1), KING_ELECTION_RESULTS, KING_UPDATE_DATABASE, DELETE_CHAR, SERVER_LIST (or remove the calls).
   - **Keep OUR** CHANGE_HAIR (5 params) and GAME_LOGIN (ALPHA's needs TB_USER.strAuthority + GAME_OPTIONS + IsBanned).
4. **M4 bot scripts:** only if strItem is widened to 592, update the `= 584` guards and the binary(584) variables in 002, 004, 005, 006 and 007.
5. **M5 reference data: a decision for the owner, not a migration step.** Options are to import ALPHA-only rows (37,876 ITEM, 50 MAGIC, KNIGHTS_CAPE 224 vs 56, K_NPC 1384 vs 521, …) where the 1534 client needs them, or to replace values. Replacing MAGIC/ITEM values would invalidate the docs/05 numbers and the bot tuning (3.5). Also decide what to do with ItemClass (keep OURS).
6. Risk: after M3, OUR 1453 server can no longer run against this DB (section 4). Do it on a copy and keep a pre-migration backup.

### (b) Adopt FDP_alpha1534 and re-apply db/001–009

1. **Hosting:** the backup needs SQL Server **2019 or newer** (any edition; Express is fine at 54 MB). The restored DB is compat 130, so it could also be moved to 2017 by scripting schema and data (it cannot be restored there).
   - Point the DSNs (ALPHA defaults to DSN `KN_online` with SQL auth for both game and account) and every `tools/` SQLCMD invocation (`-S '.\SQLEXPRESS'`) at the new instance.
2. **Cleanup:** remove the seed player rows (1 account/character/clan, 5 mails). Never run `DB_ROLLBACKFORMAT`.
3. **Fix ALPHA's own mismatches** (2.4): CHANGE_HAIR (take OURS), the KING_* procedures, ACCOUNT_NATION_TRANSFER, MAIL_BOX_GET_ITEM (+nUserSeal), DELETE_CHAR, SERVER_LIST. Also populate `ITEM.ItemClass` (needed by UpgradeHandler and the db/007 guard), for example from OUR ITEM for the 85,844 common rows.
4. **Re-apply the scripts:** 002/005/006 with strItemTime of 292 bytes, 007 with a relaxed guard and a stat-requirement check, 009 after an IDNum collision check. Re-derive the bowl K_NPCPOS deletion.
5. **Biggest cost:** every bot skill's MAGIC row differs (range, cooldown, damage, buff type). The [D]/[V] values in docs/03 and docs/05, the skill CSVs and the bot tuning would all need re-validation. LEVEL_UP also differs (max level 83, different Exp).

Why (a): (b) inherits ALPHA's broken procedures plus a full re-validation of the data. (a) touches only additive schema and leaves the data decision explicit. The one thing (b) gives for free is 1534-era reference rows, and (a) can import those selectively.

---

## 8. Open or unverified items
- [A] Does `UPDATE … SET strItem = @binary592` into binary(584) raise error 8152 when the extra bytes are zero? Test on a scratch DB.
- [A] Are ALPHA's MAGIC values (e.g. Range ×0.45, SelfEffect 0) what the 1534 client expects? This needs the 1534 client's skill table (TBL) for comparison.
- Not checked (privacy rule): whether KNIGHTS IDNum 2/3/15001/15002 collide with ALPHA's 1 seed clan; whether any OUR USERDATA row has non-zero bytes in slots 47–72 (an aggregate check run by the owner would answer it).
- ALPHA's `GameServer.log` only shows that the DSN was missing, so there is no runtime evidence of which ALPHA paths worked.

## 9. Working files (`/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/`)
- `work/table_diff_real.txt`, `work/proc_diff_real.txt`, `work/proc_body_diffs.txt`: full DB diffs.
- `work/loader_table.md`, `work/proc_table.md`, `work/alpha_sql.txt`, `work/our_sql.txt`, `work/sets.json`: source extraction.
- `work/magic_cmp.txt`, `work/ref/*`: MAGIC/ITEM numeric dumps (reference tables only).
- `work/alpha_defs.txt`, `work/our_defs.txt`: all procedure, view and function definitions from both DBs.
- `work/alpha_columns.txt`, `work/db_columns.txt`: INFORMATION_SCHEMA dumps.
- `work/bak_schema.json`, `work/alpha_modules/`: schema decoded from the raw `.bak` (cross-check).
- `scripts/*.py`: all scripts used (run with `python3 -I`).
