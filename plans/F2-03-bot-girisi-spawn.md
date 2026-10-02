# F2-03: Bot girişi (spawn): hesap/karakter ataması, `WIZ_SEL_CHAR` DB isteği, `GameStart(1/2)` taklidi (S3, S7; varsayılan kapalı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F2 — Bot oturumu (`docs/17` §2) |
| Branch | `bot/F2-03` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F2-01 (`KAPANDI`: slot havuzu, `m_botSink`, `CUser::Send` geçersiz kılma), F2-02 (`KAPANDI`: `BotManager::Tick()` IOCP thread'inde); F1-04 (`KAPANDI`: 12 bot hesabı/karakteri DB'de, `db/002`) |
| İlgili gereksinim / kabul | ADR-0014 (`docs/adr/ADR-0014-bot-oturumu-hesap-dogrulamasi.md`, bu planla birlikte yazıldı); `docs/02` §11 S3, S7; `docs/13` §4.3 (Spawn satırı); AC-ARCH-01 (kısmen: bot dünyaya girer), AC-ARCH-02 (bot kapalıyken davranış aynı) |
| Tahmini büyüklük | M (7 dosya, ~250 satır ekleme) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

`[BOT] ENABLED=1` ve yeni `[BOT] SPAWN_ON_START=<karakter adları>` ile sunucu açılışında, listelenen bot karakterlerinin (`db/002`'nin 12 sabit botundan) **gerçek bir oyuncu gibi dünyaya girmesi**:

1. `BotManager::Tick()` (IOCP thread'i) boş bir slotu alır, oturumu başlatır (`OnConnect()` + hesap adı + bot alıcısı), `WIZ_SEL_CHAR` DB isteğini kuyruğa koyar.
2. DB thread'i mevcut `ReqSelectCharacter` → `SelectCharacter` yolunu çalıştırır; cevap bot alıcısına gelir.
3. Tick, cevap geldikten sonra `WIZ_GAMESTART` opcode 1 ve ardından 2 paketlerini `HandlePacket` ile işletir; bot `isInGame()` olur, bölgedeki oyunculara görünür.
4. Her aşama `Logs/Bot_*.log`'a yazılır.

Bot hareketsizdir ve karar vermez. Çıkış/despawn, `Update()` sıklığı ve zaman aşımı muafiyeti bu planda **yoktur** (F2-04). `ENABLED=0` (varsayılan) veya `SPAWN_ON_START` boşken sunucu davranışı F2-02'dekiyle aynıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0014-bot-oturumu-hesap-dogrulamasi.md`: hesap doğrulaması ve `SET_LOGIN_INFO` atlanır (kararın gerekçesi). `docs/13` §4.3 (yaşam döngüsü tablosu), `docs/02` §11 S3/S7.
- `AGENTS.md` §2 kural 4 ve 6: bot aksiyonu gerçek istemci paketi gibi `HandlePacket`'e verilir; `CUser` durumunu yalnızca IOCP thread'i değiştirir. Bu planın istisnası yoktur ama **DB thread'i giriş sırasında `CUser`'a yazar** (aşağıda "settle gecikmesi").
- İlgili kod (dosya:satır, 2026-10-02'de depoda doğrulandı):
  - `GameServer/CharacterSelectionHandler.cpp:134-255` `CUser::SelectCharacter` (**DB thread'inde** çalışır, `DatabaseThread.cpp:267` çağırır): `:192` `m_bSelectedCharacter = true;`, `:193` `Send(&result);` (cevabın ilk baytı `bResult`), **ardından** `:195` `SetUserAbility(false);`, `SetRegion(...)`, klan işleri. `:189` `SetLogInInfoToDB(bInit);` (bot için atlanacak). Başarısızlık yolları `Disconnect()` çağırır; bot (soketsiz) oturumda `Disconnect()` etkisizdir (`shared/Socket.cpp:97-` `if (!IsConnected()) return;`), yani **cevap hiç gelmeyebilir** (zaman aşımı gerekir).
  - `GameServer/CharacterSelectionHandler.cpp:257-` `CUser::GameStart(Packet&)`: `if (isInGame()) return;`, ilk payload baytı `opcode`; `opcode == 1` (`:264`): `SendMyInfo(); UserInOutForMe; MerchantUserInOutForMe; NpcInOutForMe; SendNotice; SendTime; SendWeather;` ve `Packet result(WIZ_GAMESTART); Send(&result);`; `opcode == 2` (`:279`): `m_state = GAME_STATE_INGAME; UserInOut(INOUT_RESPAWN); ... BlinkStart(); SetUserAbility();`.
  - `GameServer/User.cpp:243-308` `CUser::HandlePacket`: sırayla kapılar: `isCryptoEnabled()` (`:249`), `m_strAccountID.empty()` (hesap), `!m_bSelectedCharacter` (karakter); sonra `case WIZ_GAMESTART: GameStart(pkt);` (`:307`). Bot için üç kapı da açık olmalı: `EnableCrypto()` (public, `shared/KOSocket.cpp:227`; `USE_CRYPTION` `shared/JvCryption.h:3`'te tanımlı), `m_strAccountID` (public alan), `m_bSelectedCharacter` (`SelectCharacter` kendisi koyar).
  - `GameServer/User.cpp:45-52` `CUser::OnConnect()`: `KOSocket::OnConnect()` (`m_remaining/m_usingCrypto/m_sequence` sıfırlar, `m_lastResponse = UNIXTIME`; `shared/KOSocket.cpp:14-24`) + `Initialize()` (`:54`, `m_strUserID/m_strAccountID` temizler, `m_state = GAME_STATE_CONNECTED`, `m_bLogout = 0`...). `DBAgent::LoadUserData` (`DBAgent.cpp:324-335`) `GetName().empty()` ve `!m_bLogout` ister: **spawn'dan önce `OnConnect()` zorunlu** (F2-01 slotları hiç `Initialize` edilmemiştir).
  - `GameServer/DatabaseThread.cpp:247-268` `ReqSelectCharacter`: `pkt >> strCharID >> bInit;` `LoadUserData`, `LoadWarehouseData`, `LoadPremiumServiceUser`, `LoadSavedMagic` hepsi doğruysa `SelectCharacter`. `:60-72` DB döngüsü: ilk iki bayt `uid`, `g_pMain->GetUserPtr(uid)` (aktif oturum haritası; slot `AcquireSlot` ile oraya taşınmış olmalı) yoksa istek atlanır.
  - `GameServer/CharacterSelectionHandler.cpp:102-` `SelCharToAgent` kalıbı: `result << strUserID << bInit; g_pMain->AddDatabaseRequest(result, this);` (bot aynı paketi kendisi oluşturur; `WIZ_SEL_CHAR`).
  - `GameServer/GameServerDlg.cpp:462` `GetUserPtr(std::string, NameType)` (`TYPE_ACCOUNT`/`TYPE_CHARACTER`), `:490` `AddAccountName(CUser*)`, `:1307` `AddDatabaseRequest(Packet&, CUser*)`; `GameServerDlg.h:304,320,155`.
  - `GameServer/GameServerDlg.cpp:737-766` `Timer_UpdateSessions`: aktif haritadaki **bot oturumlarına da** `Update()` çağırır (zamanlayıcı thread'i, mevcut gerçek oyuncu davranışıyla aynı) ve 30 sn yanıtsızlıkta `Disconnect()` çağırır (bot için etkisiz). Bu planda dokunulmaz; muafiyet ve IOCP'ye taşıma F2-04.
  - `GameServer/Bot/BotManager.cpp` (F2-02 sonrası): `Startup()` `:35` `ENABLED`, `:39` `MAX_BOTS`, `:47` `TICK_MS`; `AcquireSlot()`/`ReleaseSlot()`; `Tick()` `:222-` (`m_tickCount`, `m_firstTickTime` ilk tick'te); statik `WriteBotLog(const char *)`.
  - `GameServer/Bot/IBotSink.h`: `virtual void OnPacket(Packet & pkt) = 0;` (`CUser::Send`/`SendCompressed` her iki thread'den çağırabilir: DB thread'i, IOCP, 30 sn zamanlayıcı).
  - `db/002_bot_characters.sql:31-37` (sabit tablo): `BotWP_K/BotAccWPK, BotWG_K/BotAccWGK, BotPHD_K/BotAccPHDK, BotPHB_K/BotAccPHBK, BotMF_K/BotAccMFK, BotMI_K/BotAccMIK` ve `_E` karşılıkları `BotAccWPE, BotAccWGE, BotAccPHDE, BotAccPHBE, BotAccMFE, BotAccMIE`. Hepsi `Zone = 71`, `PX/PZ = 127400/89000` (konum 1274, 890; `db/002:271`).
  - Zone 71'e giriş: `SelectCharacter` `:168-181` yalnızca `m_byBattleOpen` (savaş açık) iken zone 71'e girişi engeller (GM değilse); savaş kapalıyken engel yok. Zone 71 `ZF_ATTACK_OTHER_NATION`, savaş bölgesi değil (`Unit.cpp:1100-1103`).
  - `shared/ByteBuffer.h:110` `read<T>(size_t pos) const` konumlu okuma (okuma imlecini oynatmaz); `shared/Packet.h:15` `Packet(uint8 opcode, uint8 subOpcode)` ikinci bayt payload'a yazılır.
  - `shared/Ini.cpp:127-142` `GetString`: anahtar yoksa varsayılanı ini'ye **yazar** (bu yüzden `SPAWN_ON_START` yalnızca `ENABLED=1` yolunda okunur, `ini.GetBool` erken dönüşünden sonra).
  - Opcode sabitleri `shared/packets.h`: `WIZ_SEL_CHAR 0x04`, `WIZ_USER_INOUT 0x07`, `WIZ_GAMESTART 0x0D`, `WIZ_MYINFO 0x0E`, `WIZ_REQ_USERIN 0x16` (dosya ISO-8859 olabilir: `grep -a`).

## 3. Kapsam

**Yapılacaklar**

- `Bot/BotSession.h/.cpp` (yeni): bir bot karakterinin spawn durumunu ve `IBotSink` uygulamasını tutan sınıf.
- `BotManager`: ini `SPAWN_ON_START`, 12'lik sabit bot tablosu, spawn kuyruğu, `Tick()` içinden çağrılan durum makinesi (`TickSessions`, `StartSession`), log satırları.
- `CUser::SelectCharacter`'da bot oturumu için `SetLogInInfoToDB` atlanır (ADR-0014).
- vcxproj ve filters'a iki yeni dosya.

**Kapsam dışı (yapılmayacak)**

- Despawn/çıkış, `OnDisconnect`/`LogOut`, slotu havuza iade (F2-04). Oturumlar süreç ömrü boyunca açık kalır; `BotSession` nesneleri **bilerek silinmez** (`CUser::m_botSink` işaret eder).
- `Update()` sıklığı, zaman aşımı muafiyeti, `m_activeSessions` gezen `SendAll*`/zamanlayıcı davranışı (F2-04).
- Sabit IP, `AccountLogin` taklidi, `CURRENTUSER`/`TB_USER`'a yazmak (ADR-0014: yapılmaz).
- Bot hareketi, saldırısı, algı/karar/aksiyon, `/bot spawn` komutu (S11), ranking/ödül/duyuru ayarları (F2-05+/F3+).
- Gelen paketleri çözmek/yorumlamak (alıcı yalnızca sayar; algı F4).
- 12 dışında karakter/hesap spawn etmek; ini'den gelen rastgele hesap adı kabul etmek.
- `Timer_UpdateSessions`, `HandlePacket`, `GameStart`, `ReqSelectCharacter`, `LoadUserData` gövdelerini değiştirmek ("iyileştirme" yok); AIServer/LogInServer/shared; SQL; `docs/**`.
- Sunucuyu çalıştırmak, `GameServer.ini`'yi elle düzenlemek, DB'ye bağlanmak (çalışma zamanı doğrulaması Claude'da, §7 sonu).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/BotSession.h` | yeni | ASCII, CRLF |
| `GameServer/Bot/BotSession.cpp` | yeni | ASCII, CRLF; ilk `#include "stdafx.h"` |
| `GameServer/Bot/BotManager.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotManager.cpp` | değiştir | ASCII, CRLF |
| `GameServer/CharacterSelectionHandler.cpp` | değiştir | tek koşul (UTF-8 BOM + CRLF koru) |
| `GameServer/proj-GameServer.vcxproj` | değiştir | `Bot\BotSession.cpp` (ClCompile), `Bot\BotSession.h` (ClInclude); BOM + CRLF koru |
| `GameServer/proj-GameServer.vcxproj.filters` | değiştir | aynı iki dosya, `Source Files` / `Header Files` filtreleri; BOM + CRLF koru |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. **Dal:** `git switch -c bot/F2-03 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `stop` (exe kilidi).

2. **`GameServer/Bot/BotSession.h`** (yeni):
   ```cpp
   #pragma once

   #include <atomic>
   #include <chrono>
   #include <string>
   #include "IBotSink.h"

   class CUser;

   // One bot character's login state plus the receiver for the packets its CUser would send
   // to a client. OnPacket() may run on the DB thread, the IOCP thread or the 30 s timer
   // thread, so it touches atomics only. Every other member is IOCP thread only.
   class BotSession : public IBotSink
   {
   public:
   	enum Phase
   	{
   		PHASE_QUEUED,       // waiting for its turn (one spawn per tick)
   		PHASE_WAIT_SELECT,  // WIZ_SEL_CHAR request queued, waiting for the DB thread's reply
   		PHASE_WAIT_LOADED,  // GameStart(1) done, waiting to send GameStart(2)
   		PHASE_IN_GAME,
   		PHASE_FAILED
   	};

   	enum SelectResult { SELECT_PENDING = 0, SELECT_OK = 1, SELECT_FAILED = 2 };

   	BotSession(const char * charName, const char * accountName);

   	// Any thread. Counts the packet; WIZ_SEL_CHAR also records the select result.
   	virtual void OnPacket(Packet & pkt);

   	const std::string m_charName;
   	const std::string m_accountName;

   	CUser * m_pUser;                                       // IOCP thread only
   	Phase m_phase;                                         // IOCP thread only
   	std::chrono::steady_clock::time_point m_phaseStart;    // IOCP thread only
   	bool m_selectSeen;                                     // IOCP thread only
   	std::chrono::steady_clock::time_point m_selectSeenAt;  // IOCP thread only

   	std::atomic<int> m_selectResult;                       // SelectResult, set by OnPacket
   	std::atomic<uint32> m_packetTotal;
   	std::atomic<uint32> m_opcodeCount[256];
   };
   ```
   Not: `std::atomic<uint32>` varsayılan kurulumda **sıfırlanmaz**; kurucu `m_opcodeCount`'ı döngüyle sıfırlar.

3. **`GameServer/Bot/BotSession.cpp`** (yeni; `#include "stdafx.h"`, `#include "BotSession.h"`):
   - Kurucu: ad alanlarını kopyalar, `m_pUser = nullptr`, `m_phase = PHASE_QUEUED`, `m_selectSeen = false`, `m_selectResult = SELECT_PENDING`, `m_packetTotal = 0`, 256 sayacı sıfırla.
   - `OnPacket`:
     ```cpp
     void BotSession::OnPacket(Packet & pkt)
     {
     	uint8 opcode = pkt.GetOpcode();
     	m_packetTotal++;
     	m_opcodeCount[opcode]++;

     	// SelectCharacter()'s reply: first payload byte is bResult (0 = failed).
     	if (opcode == WIZ_SEL_CHAR)
     		m_selectResult = (pkt.read<uint8>(0) != 0) ? SELECT_OK : SELECT_FAILED;
     }
     ```
     Bu dosyada `g_pMain`, `CUser` üyesi veya `WriteBotLog` **kullanılmaz** (yalnızca atomikler).

4. **`GameServer/CharacterSelectionHandler.cpp`** (satır `:189`): tek çağrıyı koşula bağla, başka satıra dokunma:
   ```cpp
   	// Bot sessions skip the CURRENTUSER/TB_USER bookkeeping (ADR-0014).
   	if (m_botSink == nullptr)
   		SetLogInInfoToDB(bInit);
   ```
   (`m_botSink` `User.h:582`'de public; `Bot/IBotSink.h` zaten `User.cpp`'de dahil, bu dosyada `m_botSink`'in tipi eksik olsa da `!= nullptr` karşılaştırması için tam tanım gerekmez; derleme hatası olursa `#include "Bot/IBotSink.h"` ekleme **yapma**, soru olarak yaz.)

5. **`GameServer/Bot/BotManager.h`:** `#include <string>`, `#include <vector>`; `class BotSession;` ön bildirimi; `private:` bölümüne ekle (kurucuda `m_spawnSummaryDone(false)` başlat):
   ```cpp
   	// Spawn list from [BOT] SPAWN_ON_START (parsed in Startup(); sessions are never freed in F2-03).
   	void ParseSpawnList(const std::string & list);
   	void TickSessions();                          // IOCP thread only, called from Tick()
   	void StartSession(BotSession * s);            // IOCP thread only
   	void FailSession(BotSession * s, const char * reason); // IOCP thread only

   	std::vector<BotSession *> m_sessions;
   	bool m_spawnSummaryDone;                      // IOCP thread only
   ```

6. **`GameServer/Bot/BotManager.cpp`:**
   - Dosya başı: `#include "BotSession.h"`, `<cstring>` (gerekirse `<string>`/`<algorithm>`).
   - Sabit tablo (`WriteBotLog`'un altında):
     ```cpp
     struct BotAccountEntry { const char * charName; const char * accountName; };
     static const BotAccountEntry BOT_TABLE[] =
     {
     	{ "BotWP_K", "BotAccWPK" }, { "BotWG_K", "BotAccWGK" }, { "BotPHD_K", "BotAccPHDK" },
     	{ "BotPHB_K", "BotAccPHBK" }, { "BotMF_K", "BotAccMFK" }, { "BotMI_K", "BotAccMIK" },
     	{ "BotWP_E", "BotAccWPE" }, { "BotWG_E", "BotAccWGE" }, { "BotPHD_E", "BotAccPHDE" },
     	{ "BotPHB_E", "BotAccPHBE" }, { "BotMF_E", "BotAccMFE" }, { "BotMI_E", "BotAccMIE" }
     };
     ```
     Sabitler: `SPAWN_START_DELAY_MS = 5000` (ilk tick'ten sonra; AI sunucusu bağlansın), `SELECT_SETTLE_MS = 1000`, `LOADED_DELAY_MS = 200`, `PHASE_TIMEOUT_MS = 15000` (dosya içi `static const`).
   - `Startup()`: etkin yolda, `TICK_MS` okuma bloğunun **hemen sonrasında** (ve `std::lock_guard`'tan önce) `std::string spawnList; ini.GetString("BOT", "SPAWN_ON_START", "", spawnList);` oku. `ParseSpawnList(spawnList)` çağrısını, havuz kurulumu **başarılı** bittikten sonra (`ok == true`, `message` yazıldıktan sonra, `return ok` öncesi) yap; havuz kurulamadıysa liste işlenmez.
   - `ParseSpawnList(list)`: virgülle böl, her parçayı baştan/sondan boşluktan arındır, boşları atla. Her ad için `BOT_TABLE`'da **büyük/küçük harf duyarsız** (`_stricmp`) ara:
     - bulunamazsa log `BotManager: SPAWN_ON_START: unknown bot name '<name>' ignored`;
     - zaten kuyruktaysa sessizce atla;
     - `m_sessions.size() >= m_poolSize` ise log `BotManager: SPAWN_ON_START: '<name>' ignored (pool size <P>)`;
     - aksi halde `m_sessions.push_back(new BotSession(entry.charName, entry.accountName))`.
     Sonunda `m_sessions` boş değilse log: `BotManager: spawn list: <n> bot(s) queued (<char1>,<char2>,...)` (tablodaki **kanonik** yazımla, virgülle, boşluksuz).
   - `Tick()`: mevcut sayaç/log mantığının **sonuna** (erken `return`'lerden sonra, gövdenin son ifadesi) `TickSessions();` ekle. Mevcut iki log satırı ve sayaç davranışı aynen kalır.
   - `TickSessions()` (IOCP thread; `m_sessions.empty() || m_spawnSummaryDone` ise hemen dön; ilk tick'ten `SPAWN_START_DELAY_MS` geçmeden dön; geçen süre `m_firstTickTime` ile `steady_clock::now()` farkıdır). Her tick'te tüm oturumlar için, **en fazla bir** `PHASE_QUEUED` oturumu başlatarak (`startedThisTick`):
     - `PHASE_QUEUED` → (bu tick'te başka başlatma yoksa) `StartSession(s)`.
     - `PHASE_WAIT_SELECT`:
       - `m_selectResult == SELECT_FAILED` → `FailSession(s, "select rejected")`.
       - `m_selectResult == SELECT_OK`: ilk görüldüğünde `m_selectSeen = true; m_selectSeenAt = now;`. `SELECT_SETTLE_MS` geçince `Packet pkt(WIZ_GAMESTART, uint8(1)); s->m_pUser->HandlePacket(pkt);` sonra `m_phase = PHASE_WAIT_LOADED; m_phaseStart = now;`.
         **Settle gerekçesi (koda yorum olarak yaz):** `SelectCharacter` yanıtı `Send(&result)` ile **önce** verir, `SetUserAbility(false)`/`SetRegion` DB thread'inde **sonra** çalışır (`CharacterSelectionHandler.cpp:193-197`); gerçek istemci yükleme süresiyle bunu örter, bot için 1 sn bekleriz.
       - `m_selectResult == SELECT_PENDING` ve `now - m_phaseStart > PHASE_TIMEOUT_MS` → `FailSession(s, "select timeout")`.
     - `PHASE_WAIT_LOADED`: `LOADED_DELAY_MS` geçince `Packet pkt(WIZ_GAMESTART, uint8(2)); s->m_pUser->HandlePacket(pkt);` ardından `s->m_pUser->isInGame()` ise `m_phase = PHASE_IN_GAME` ve log (aşağıda), değilse `FailSession(s, "game start failed")`.
     - `PHASE_IN_GAME`/`PHASE_FAILED`: iş yok.
     - Döngü sonunda tüm oturumlar `PHASE_IN_GAME` veya `PHASE_FAILED` ise **bir kez** `m_spawnSummaryDone = true` ve log `BotManager: spawn complete: <ok>/<total> in game, <fail> failed`.
   - `StartSession(s)` (IOCP thread):
     1. `g_pMain->GetUserPtr(s->m_charName, TYPE_CHARACTER) != nullptr` → `FailSession(s, "character already online")`; return. Aynı şekilde `GetUserPtr(s->m_accountName, TYPE_ACCOUNT)` → `"account already online"`.
     2. `CUser * pUser = AcquireSlot(); if (pUser == nullptr) { FailSession(s, "no free slot"); return; }`
     3. Oturumu hazırla (bu sırayla): `pUser->OnConnect();` (`Initialize` dahil; gerçek bağlantının aldığı çağrı), `pUser->EnableCrypto();`, `pUser->m_strAccountID = s->m_accountName;`, `pUser->m_botSink = s;`, `g_pMain->AddAccountName(pUser);`, `s->m_pUser = pUser;`. (`OnConnect` `m_botSink`'e dokunmaz; `m_botSink`'i **DB isteğinden önce** koy: `SelectCharacter` cevabı alıcıya gider.)
     4. `Packet req(WIZ_SEL_CHAR); req << s->m_charName << uint8(1); g_pMain->AddDatabaseRequest(req, pUser);` (`bInit = 1`; `SelCharToAgent` ile aynı biçim: önce ad dizgesi, sonra bayt).
     5. `s->m_phase = PHASE_WAIT_SELECT; s->m_phaseStart = now;` ve log `BotManager: bot <char> spawning (slot <id>, account <acc>)` (`<id>` = `pUser->GetSocketID()`).
   - `FailSession(s, reason)`: `m_phase = PHASE_FAILED`; log `BotManager: bot <char> spawn FAILED (<reason>)`. Slotu **iade etme, oturumu silme** (DB thread'i hâlâ çalışıyor olabilir; temizlik F2-04).
   - "In game" log satırı (**tam biçim**, `WriteBotLog`; `<h>`/`<mh>` = `GetHealth()`/`GetMaxHealth()`):
     `BotManager: bot <char> in game (slot <id>, zone <z>, pos <x.x>,<z.z>, hp <h>/<mh>, packets <n>, myinfo <k>)` — `<z>` = `GetZoneID()` (`unsigned`), `<x.x>`/`<z.z>` = `GetX()`/`GetZ()` `%.1f`, `<n>` = `m_packetTotal`, `<k>` = `m_opcodeCount[WIZ_MYINFO]`.
   - Bu planın log satırları yalnızca `WriteBotLog` ile yazılır (konsola `printf` yok).

7. **vcxproj + filters:** `Bot\BotManager.cpp`'nin yanına `<ClCompile Include="Bot\BotSession.cpp" />`; `Bot\IBotSink.h`'nin yanına `<ClInclude Include="Bot\BotSession.h" />`. `.filters`'ta `BotManager.cpp` bloğunun (`Source Files`) ve `IBotSink.h` bloğunun (`Header Files`) birebir kalıbıyla iki blok ekle.

8. **Derle:** `./tools/build.sh Release` ve `./tools/build.sh Debug`; ikisi de hatasız. Tüm çözüm derlenir.

9. Sunucuyu **çalıştırma** (§7 sonu). Kodu commit'le (`[F2-03] ...`), Uygulayıcı Raporu'nu yaz, `Durum`'u `UYGULANDI` yap.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; **yeni uyarı yok** (`./tools/build.sh Release 2>&1 | grep -a "warning" | sort -u` çıktısını yapıştır; `Bot\` dosyalarında ve eklenen satırlarda uyarı olmamalı, eski satırların uyarıları kabul).
- [ ] K2: `./tools/build.sh Debug` hatasız biter.
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F2-03` yalnızca §4 tablosundaki 7 dosyayı içerir; `CharacterSelectionHandler.cpp` farkı **≤ 3 ekleme / 1 silme** (silinen satır yalnızca `SetLogInInfoToDB(bInit);`'in eski girintili hâli; çıktıyı yapıştır).
- [ ] K4: Alıcı iş parçacığı güvenliği: `BotSession.cpp`'de `g_pMain`, `CUser`, `WriteBotLog`, `GetLock` **yok**; `OnPacket` yalnızca atomik alanlara yazar, `m_opcodeCount` kurucuda sıfırlanır (`grep -n "g_pMain\|CUser\|WriteBotLog\|GetLock" GameServer/Bot/BotSession.cpp` boş + kurucu/`OnPacket` `sed -n` çıktısı).
- [ ] K5: Thread kuralı: `m_pUser`'a (`HandlePacket`, `OnConnect`, `EnableCrypto`, alan atamaları, `isInGame`, `GetX` vb.) dokunan her satır `StartSession`/`TickSessions` içindedir; bunlar yalnızca `Tick()`'ten çağrılır. `Startup()` ve `ParseSpawnList` `CUser`'a dokunmaz (F2-01'in havuz kodu hariç) (`grep -n "m_pUser\|HandlePacket\|OnConnect\|EnableCrypto" GameServer/Bot/BotManager.cpp` çıktısını açıklayarak yapıştır).
- [ ] K6: Yalnızca sabit 12'lik tablo: `ParseSpawnList` tablo dışı adı kuyruğa **almaz** (log `unknown bot name`); eşleme `_stricmp` ile büyük/küçük harf duyarsız; havuz boyutunu aşan ad `ignored (pool size …)` ile atlanır; aynı ad iki kez kuyruğa girmez (`sed -n` çıktısı).
- [ ] K7: Bot kapalıyken/liste boşken davranış aynı: `SPAWN_ON_START` yalnızca `ENABLED` erken dönüşünden **sonra** okunur (`sed -n` çıktısı, satır numarasıyla); `ParseSpawnList` yalnızca havuz kurulumu başarılıysa çağrılır; `TickSessions` `m_sessions.empty()` iken ilk ifadede döner; `SelectCharacter` farkı yalnızca `m_botSink == nullptr` koşuludur (gerçek oyuncu için `SetLogInInfoToDB` aynen çağrılır).
- [ ] K8: `HandlePacket` kullanımı: `GameStart` adımları `HandlePacket` ile işletilir (doğrudan `GameStart()` çağrısı **yok**); iki paket `Packet(WIZ_GAMESTART, uint8(1))` ve `uint8(2)`; `WIZ_SEL_CHAR` isteği `Packet req(WIZ_SEL_CHAR); req << charName << uint8(1)` ve `AddDatabaseRequest(req, pUser)` ile (`grep -n "GameStart\|WIZ_GAMESTART\|WIZ_SEL_CHAR" GameServer/Bot/*` çıktısı).
- [ ] K9: Sıra ve gecikmeler: `m_botSink` DB isteğinden **önce** atanır; `OnConnect()` `AcquireSlot()`'tan sonra ve `m_strAccountID` atamasından **önce** çağrılır; `SELECT_SETTLE_MS`/`LOADED_DELAY_MS`/`PHASE_TIMEOUT_MS`/`SPAWN_START_DELAY_MS` sabitleri 1000/200/15000/5000 (`sed -n` çıktısı).
- [ ] K10: Log biçimi: §5.6'daki tam metinler `BotManager.cpp`'de bulunur: `spawn list:`, `unknown bot name`, `ignored (pool size`, `spawning (slot`, `spawn FAILED (`, `in game (slot`, `spawn complete:` (`grep -n` çıktısı); yalnızca `WriteBotLog` ile yazılır, bu planın eklediği kodda `printf` yok.
- [ ] K11: Başarısız oturum iade edilmez/silinmez: `FailSession` slotu `ReleaseSlot`'a vermez ve `delete` kullanmaz; kodda bunu açıklayan bir yorum vardır.
- [ ] K12: Kodlama: `file GameServer/Bot/* GameServer/CharacterSelectionHandler.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters` — `Bot/*` "ASCII … CRLF", diğerleri "UTF-8 (with BOM) … CRLF" (önceki kodlamayla aynı; çıktıyı yapıştır).
- [ ] K13: `git status --short` boş (yalnızca izinli dosyalar commit'li). Sunucu çalıştırılmadı, `GameServer.ini` ve veritabanı değiştirilmedi.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
git diff --stat gece/2026-10-02...bot/F2-03
git diff gece/2026-10-02...bot/F2-03 -- GameServer/CharacterSelectionHandler.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters
grep -n "g_pMain\|CUser\|WriteBotLog\|GetLock" GameServer/Bot/BotSession.cpp
grep -n "m_pUser\|HandlePacket\|OnConnect\|EnableCrypto\|GameStart\|WIZ_SEL_CHAR\|SPAWN_ON_START\|_stricmp" GameServer/Bot/BotManager.cpp
file GameServer/Bot/* GameServer/CharacterSelectionHandler.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]`: `ENABLED=1`, `MAX_BOTS=16`, `SPAWN_ON_START=BotWP_K,BotMF_K,BotWP_E,BotPHD_E`; sunucuyu `tools/run-servers.sh start` ile aç, ~25 sn bekle. Beklenen `Logs/Bot_*.log`: `spawn list: 4 bot(s) queued (...)`, her bot için `spawning (slot 2999…)` ve `in game (slot …, zone 71, pos 1274.0,890.0, hp <h>/<mh>, packets <n>, myinfo 1)` (`h == mh > 0`), `spawn complete: 4/4 in game, 0 failed`; sunucu 3/3 UP. Ek: `SPAWN_ON_START=Foo,BotWP_K` → `unknown bot name 'Foo' ignored`; `MAX_BOTS=2` + 4 ad → iki `ignored (pool size 2)`; kayıtsız harf (`botwp_k`) çalışır; `SPAWN_ON_START` boş → F2-02 çıktısı; `ENABLED=0` → hiçbir bot satırı yok ve ini'ye `SPAWN_ON_START` eklenmez. Sonra `tools/run-servers.sh stop`, ini ve gerekirse bot satırlarını geri yükle (bot satırları sunucu `PLAYER_SAVE_INTERVAL` (3 dk) kaydıyla güncellenebilir; `db/002` yeniden uygulanabilir).

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` dosyaları ASCII; kod yorumları İngilizce.
- **Bot sistemi varsayılan kapalı.** `ENABLED=1` olsa bile `SPAWN_ON_START` boşsa hiçbir bot oluşmaz (F2-02 davranışı).
- **Slot ve oturum ömrü:** Başarılı veya başarısız hiçbir oturum bu planda kapatılmaz. Sunucu yeniden başlayana kadar `CUser` slotları ve `BotSession` nesneleri yaşar. `Disconnect()` bot için etkisizdir; `KickOutAllUsers` (`GameServerDlg.cpp:2947`) kapanışta botları da kaydeder, bu istenen davranıştır.
- **DB thread yarışı:** `SelectCharacter` cevaptan **sonra** `CUser`'a yazmaya devam eder (`SetUserAbility(false)`, `SetRegion`). `SELECT_SETTLE_MS` bu yüzden vardır; sabiti kısaltma. Kalan yarış (aynı oturumda 30 sn zamanlayıcının `Update()` çağırması) gerçek oyuncularda da vardır (R-CODE-03) ve F2-04'te giderilir.
- **Zamanlayıcı etkileşimi:** Aktif haritadaki bot oturumlarına `Timer_UpdateSessions` `Update()` çağırır. Bu planda dokunma; beklenen ve zararsızdır. Bot `isInGame()` olmadan önce de `Update()` alabilir (gerçek oturumlarla aynı sınıf).
- **`m_botSink` ve `Send`:** `CUser::Send` ile gelen her paket `OnPacket`'e gider; `OnPacket` yalnızca sayar, **paketi değiştirmez ve saklamaz** (`CUser::Send` aynı `Packet`'i çağırandan sonra yeniden kullanır).
- **Veri:** Bot karakterleri `db/002` ile kurulmuş olmalıdır; kurulmamışsa spawn `select rejected` ile başarısız olur (kod hatası değildir). Hesap/karakter adlarını başka yerden okuma, DB'ye bağlanma.
- **Hesap kapısı:** `LoginServer` bot hesaplarını tanımaz (ADR-0014); gerçek istemciyle bot hesabına girmek çalışma zamanında `CURRENTUSER`'sız bir oturum yaratır. Bot hesaplarıyla insan girişi bu plan sırasında yapılmaz (`SPAWN_ON_START`'a yazılan botun hesabı çevrimiçi ise `account already online`).
- `HandlePacket`, `GameStart` ve `SelectCharacter`'ın içini "bota uysun" diye değiştirme; yalnızca §5.4'teki tek koşul serbest.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F2-03` — `<kısa-sha> [F2-03] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F2-03` @ `<sha>`
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ / ✘ | dosya:satır / komut çıktısı |

- Bulgular (önem sırasıyla):
  1. …
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
…
```
