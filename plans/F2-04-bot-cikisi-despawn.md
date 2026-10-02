# F2-04: Bot çıkışı (despawn): `OnDisconnect`/`LogOut` taklidi, slot iadesi, `Update()` ve zaman aşımı muafiyeti (S4, S5, S8; varsayılan kapalı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F2 — Bot oturumu (`docs/17` §2) |
| Branch | `bot/F2-04` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F2-01 (`KAPANDI`: slot havuzu, `ReleaseSlot`), F2-02 (`KAPANDI`: `Tick()`), F2-03 (`KAPANDI`: `BotSession`, `TickSessions`, spawn) |
| İlgili gereksinim / kabul | ADR-0014 (F2-04 eki: çıkışta `AccountLogout` atlanır); `docs/02` §11 S4, S5, S8; `docs/13` §4.3 (yaşam döngüsü); AC-ARCH-01 (kısmen: bot dünyaya girer ve güvenle çıkar), AC-ARCH-02 (bot kapalıyken davranış aynı) |
| Tahmini büyüklük | M (6 kod dosyası, ~170 satır ekleme) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

F2-03'te dünyaya giren botlar süreç sonuna kadar oyunda kalıyordu. Bu plan sonunda:

1. Yeni `[BOT] DESPAWN_AFTER_SEC=<n>` (varsayılan `0` = hiç çıkma): `n` saniye "in game" kalan her bot **gerçek bir oyuncu gibi çıkar**: `CUser::OnDisconnect()` (bölgeden çıkar, isim haritalarından silinir, `WIZ_LOGOUT` DB isteği kuyruğa girer), DB thread'i kaydı bitirince **slot havuza iade edilir** (S4).
2. `Update()` (buff/DoT/HP zamanlayıcıları, 3 dk'lık otomatik kayıt) her oyundaki bot için **saniyede bir, IOCP thread'inde** `BotManager::TickSessions` tarafından çağrılır (S8). Bot oturumları 30 sn'lik `Timer_UpdateSessions` döngüsünden **tamamen çıkarılır** (S5: zaman aşımı muafiyeti + `Update()`'in zamanlayıcı thread'inden çağrılmaması).
3. Botun çıkışında `AccountLogout` (CURRENTUSER silme) çağrılmaz (ADR-0014 eki).

Bot hareketsizdir ve karar vermez. Aynı botu yeniden spawn etmek ve 1000 spawn/despawn dayanıklılık döngüsü F2-05'tir. `ENABLED=0` (varsayılan) veya `DESPAWN_AFTER_SEC=0` iken sunucu davranışı F2-03'tekiyle aynıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0014-bot-oturumu-hesap-dogrulamasi.md` ("Karar" son madde): `ACCOUNT_LOGOUT` yordamı yalnızca `DELETE FROM CURRENTUSER WHERE strAccountID = @strAccountID` çalıştırır; botlar için atlanır. `docs/13` §4.3, `docs/02` §11 S4/S5/S8.
- `AGENTS.md` §2 kural 4 ve 6: `CUser` durumunu yalnızca IOCP thread'i değiştirir (`BeginDespawn`, `Update()` çağrısı, slot iadesi buna uyar).
- İlgili kod (dosya:satır, 2026-10-02'de depoda doğrulandı):
  - `GameServer/User.cpp:200-233` `CUser::OnDisconnect()`: `KOSocket::OnDisconnect()` (yalnızca `TRACE`), `g_pMain->RemoveSessionNames(this)` (hesap + karakter adı haritalarından siler; karakter adı yalnızca `isInGame()` iken), `isInGame()` ise `UserInOut(INOUT_OUT)` (`CharacterMovementHandler.cpp:71-97`: bölgeden çıkarır, bölgeye ve AI sunucusuna duyurur), parti/klan/pencere/rakip temizliği, sonda `LogOut()`. (`:210-217` içindeki girintisiz `PartyRemove(GetSocketID());` satırı `if`'in dışında kalır ve her çıkışta çalışır; bu gerçek oyuncularda da böyledir, **dokunma**.) `OnDisconnect` `public virtual` (`User.h:577`); `m_state`'i değiştirmez (`isInGame()` slot yeniden kullanılana dek true kalır; gerçek oturumlarla aynı).
  - `GameServer/User.cpp:896-908` `CUser::LogOut()`: `m_strUserID.empty()` ise hemen döner; aksi halde `AG_USER_LOG_OUT` AI sunucusuna gider, `m_deleted = true` (**"çıkış bitene dek oturum kullanılamaz"**) ve `WIZ_LOGOUT` DB isteği kuyruğa girer.
  - `GameServer/DatabaseThread.cpp:131-132` `case WIZ_LOGOUT: if (pUser) pUser->ReqUserLogOut();` ve `:60-72` döngüsü: `uid` ile `g_pMain->GetUserPtr(uid)` (`KOSocketMgr::operator[]`, **yalnızca aktif harita**, `shared/KOSocketMgr.h:68-77`) arar; bulunamazsa istek atlanır. Bu yüzden bot slotu DB kaydı bitene dek **aktif haritada kalmalıdır**.
  - `GameServer/DatabaseThread.cpp:437-462` `CUser::ReqUserLogOut()` (**DB thread'inde**): sıralama ve ranking işlemleri, `UpdateUser(UPDATE_LOGOUT)`, `UpdateWarehouseData`, `UpdateSavedMagic`, `:457-458` `if (m_bLogout != 2) g_DBAgent.AccountLogout(GetAccountName());`, ve **son ifade** `:461` `m_deleted = false;`. Bot için tamamlanma işareti budur: `m_deleted` (`shared/Socket.h:60` `IsDeleted()`, public) `LogOut()`'tan sonra `true`, DB kaydı bitince `false` olur.
  - `shared/Socket.cpp:97-120` `Socket::Disconnect()`: `if (!IsConnected()) return;` yani soketsiz bot oturumunda hiçbir şey yapmaz; bu yüzden `OnDisconnect()` doğrudan çağrılır. Gerçek yolda `GetSocketMgr()->OnDisconnect(this)` oturumu ayrı thread ile aktif haritadan boşa taşır (`shared/SocketMgr.cpp:21-26,178-182`, `KOSocketMgr.h:150-163`); bot için bunu `ReleaseSlot` yapar.
  - `GameServer/Bot/BotManager.cpp:524-533` `FailSession` (başarısız oturum iade edilmez, F2-03); `:375-478` `TickSessions` (`:377` ilk ifade `m_sessions.empty() || m_spawnSummaryDone`, **bu plan ikinci koşulu kaldırır**, aşağıda); `:440-445` "in game" log; `ReleaseSlot` `BotManager.cpp` (kilit altında `m_botSink = nullptr` + havuza iade); `Startup()` `:75` `SPAWN_ON_START` okuması.
  - `GameServer/GameServerDlg.cpp:737-766` `Timer_UpdateSessions` (30 sn'lik zamanlayıcı thread'i): `:741` aktif haritanın kopyası, `:744` `CUser * pUser = TO_USER(itr->second);`, `:746-758` `#ifndef DEBUG` zaman aşımı bloğu (`KOSOCKET_TIMEOUT` 30 sn, hesap kimliği dolu ve oyunda değilse `KOSOCKET_LOADING_TIMEOUT` 30 dk; `pUser->Disconnect()` bot için etkisiz), `:761-762` `if (pUser->isInGame()) pUser->Update();`.
  - `GameServer/User.cpp:502-` `CUser::Update()` ve `:495` `HandlePacket`'in sonundaki `Update();` (her işlenen paketten sonra); `GameServer/User.h:21` `PLAYER_SAVE_INTERVAL (3 * 60)`: `Update()` 3 dakikada bir `UserDataSaveToAgent()` ile `WIZ_DATASAVE` DB isteği atar (gerçek oyuncuyla aynı).
  - `GameServer/GameServerDlg.cpp:2947-2965` `KickOutAllUsers`: sunucu kapanışında **aktif haritadaki** her oturumu kaydeder (iade edilmiş slot artık aktif haritada değildir, çifte kayıt olmaz).
  - `GameServer/User.h:110,582` `class IBotSink;` ön bildirimi ve `IBotSink * m_botSink;` (`m_botSink != nullptr` karşılaştırması için tam tanım gerekmez; F2-03'te `CharacterSelectionHandler.cpp`'de aynı kalıp kullanıldı).
  - `shared/Ini.cpp:127-142` `GetString`/`GetInt`: anahtar yoksa varsayılanı ini'ye **yazar** (bu yüzden yeni anahtar yalnızca `ENABLED=1` yolunda okunur).

## 3. Kapsam

**Yapılacaklar**

- `BotSession`: üç yeni faz ve süre/sayaç alanları.
- `BotManager`: ini `DESPAWN_AFTER_SEC`; `TickSessions` oyundaki bota `Update()` (1 sn) ve süre dolunca `BeginDespawn`; `PollDespawn` (DB kaydı bitince slotu iade eder); log satırları ve despawn özeti. Spawn özetinin sayımı sayaçlara taşınır.
- `Timer_UpdateSessions`: bot oturumlarını atlar.
- `CUser::ReqUserLogOut`: bot için `AccountLogout` atlanır.

**Kapsam dışı (yapılmayacak)**

- Aynı botu yeniden spawn etmek, `PHASE_DESPAWNED` oturumunu yeniden kuyruğa almak, spawn/despawn döngüsü (F2-05).
- **Başarısız** (`PHASE_FAILED`) oturumları temizlemek/iade etmek: F2-03'teki gibi slot iade edilmez (DB thread'i hâlâ çalışıyor olabilir).
- `BotSession` nesnelerini silmek (`delete`): oturumlar süreç ömrü boyunca yaşar (en çok 12; `m_botSink` işaretçisi başka thread'lerde okunuyor olabilir).
- Çıkışı tetikleyen komut/paket (`/bot despawn`, S11, F3); ini'de bot başına süre; sabit IP; gerçek istemciyle bot hesabına giriş.
- `Timer_UpdateSessions` dışındaki `m_activeSessions` gezen zamanlayıcılara/`SendAll*` çağrılarına dokunmak: bot oturumunda `Send` yalnızca `BotSession::OnPacket` sayacını artırır (zararsız); diğer thread'lerin `CUser`'a yazması gerçek oyuncularla aynı sınıftır (R-CODE-03) ve sonraki planlarda ele alınır.
- `OnDisconnect`, `LogOut`, `UserInOut`, `HandlePacket`, `Update`, `RemoveSessionNames`, `Socket::Disconnect`, `KickOutAllUsers` gövdelerini değiştirmek ("iyileştirme" yok, ör. `m_deleted`'i atomik yapmak); `GameStart`/`SelectCharacter`; AIServer/LogInServer/shared; SQL; `docs/**`.
- Sunucuyu çalıştırmak, `GameServer.ini`'yi elle düzenlemek, DB'ye bağlanmak (çalışma zamanı doğrulaması Claude'da, §7 sonu).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/BotSession.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotSession.cpp` | değiştir | ASCII, CRLF; yalnızca kurucu başlatıcıları |
| `GameServer/Bot/BotManager.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotManager.cpp` | değiştir | ASCII, CRLF |
| `GameServer/GameServerDlg.cpp` | değiştir | yalnızca `Timer_UpdateSessions` içine tek koşul (UTF-8 BOM + CRLF koru) |
| `GameServer/DatabaseThread.cpp` | değiştir | yalnızca `ReqUserLogOut` içindeki tek koşul (UTF-8 BOM + CRLF koru) |

vcxproj/filters **değişmez** (yeni dosya yok). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. **Dal:** `git switch -c bot/F2-04 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `stop` (exe kilidi).

2. **`GameServer/Bot/BotSession.h`:** `Phase` sabitlerini genişlet (mevcut beşi ve sırası aynen kalır, sona ekle):
   ```cpp
   	PHASE_FAILED,
   	PHASE_DESPAWN_WAIT,   // OnDisconnect() called, waiting for the DB thread to finish the logout save
   	PHASE_DESPAWNED,      // slot returned to the pool, m_pUser == nullptr
   	PHASE_DESPAWN_STUCK   // logout save not confirmed in time; slot deliberately kept
   ```
   `IOCP thread only` alanlar ekle (mevcut `m_selectSeenAt`'ten sonra):
   ```cpp
   	std::chrono::steady_clock::time_point m_inGameSince;   // IOCP thread only
   	std::chrono::steady_clock::time_point m_lastUpdate;    // IOCP thread only
   	std::chrono::steady_clock::time_point m_despawnStart;  // IOCP thread only
   	uint32 m_updateCount;                                  // IOCP thread only
   	uint16 m_slotId;                                       // IOCP thread only, kept for log lines after m_pUser is cleared
   ```
   `BotSession.cpp` kurucusuna `m_updateCount(0), m_slotId(0)` ekle (başlatıcı listesi sırası bildirim sırasıyla aynı; `time_point` alanları varsayılan kurulur). `OnPacket` ve başka hiçbir şeye dokunma.

3. **`GameServer/Bot/BotManager.h`:** `private:` bölümüne ekle; kurucunun başlatıcı listesine `m_despawnAfterMs(0), m_spawnOk(0), m_spawnFailed(0), m_despawnSummaryDone(false)` ekle:
   ```cpp
   	void BeginDespawn(BotSession * s, std::chrono::steady_clock::time_point now);  // IOCP thread only
   	void PollDespawn(BotSession * s, std::chrono::steady_clock::time_point now);   // IOCP thread only

   	uint32 m_despawnAfterMs;     // 0 = never despawn ([BOT] DESPAWN_AFTER_SEC)
   	uint32 m_spawnOk;            // IOCP thread only: sessions that reached PHASE_IN_GAME
   	uint32 m_spawnFailed;        // IOCP thread only: sessions that went through FailSession()
   	bool m_despawnSummaryDone;   // IOCP thread only
   ```
   `m_spawnSummaryDone` kalır. `Startup` yorumundaki "sessions are never freed in F2-03" ifadesini "sessions are never freed" yap.

4. **`GameServer/Bot/BotManager.cpp`:**
   - Sabitler (mevcut `PHASE_TIMEOUT_MS`'in altına, `static const uint32`): `UPDATE_PERIOD_MS = 1000`, `DESPAWN_TIMEOUT_MS = 30000`.
   - `Startup()`: `SPAWN_ON_START` okumasının **hemen altında** (hâlâ `std::lock_guard`'tan önce, `ENABLED` erken dönüşünden sonra):
     ```cpp
     	int despawnSec = ini.GetInt("BOT", "DESPAWN_AFTER_SEC", 0);
     	if (despawnSec < 0)
     		despawnSec = 0;
     	else if (despawnSec > 86400)
     		despawnSec = 86400;
     	m_despawnAfterMs = (uint32)despawnSec * 1000;
     ```
   - `ParseSpawnList`: sonunda mevcut `spawn list:` logundan **sonra** (aynı `if (!m_sessions.empty())` bloğunda), `m_despawnAfterMs != 0` ise log `BotManager: despawn after <n> s (DESPAWN_AFTER_SEC)` (`<n>` = `m_despawnAfterMs / 1000`, `%u`).
   - `FailSession`: gövdeye `m_spawnFailed++;` ekle (log ve yorum aynen kalır).
   - `TickSessions()`:
     - İlk ifadeyi `if (m_sessions.empty()) return;` yap (`|| m_spawnSummaryDone` **kaldırılır**; ilk ifade yine boşta döner). `SPAWN_START_DELAY_MS` kontrolü olduğu gibi kalır.
     - "In game" geçişinde (`s->m_phase = BotSession::PHASE_IN_GAME;` satırının hemen altında): `s->m_inGameSince = now; s->m_lastUpdate = now; s->m_slotId = s->m_pUser->GetSocketID(); m_spawnOk++;` ("in game" log satırı aynen kalır).
     - `switch`'e iki durum ekle (`default` öncesi):
       ```cpp
       case BotSession::PHASE_IN_GAME:
       	if (m_despawnAfterMs != 0
       		&& now - s->m_inGameSince >= std::chrono::milliseconds(m_despawnAfterMs))
       	{
       		BeginDespawn(s, now);
       	}
       	else if (now - s->m_lastUpdate >= std::chrono::milliseconds(UPDATE_PERIOD_MS))
       	{
       		// S8: timed effects/saves; runs on the IOCP thread, never on the 30 s timer thread.
       		s->m_lastUpdate = now;
       		s->m_updateCount++;
       		s->m_pUser->Update();
       	}
       	break;

       case BotSession::PHASE_DESPAWN_WAIT:
       	PollDespawn(s, now);
       	break;
       ```
     - Döngü sonundaki sayım (`okCount`/`failCount`) ve eski özet bloğunu şu şekilde değiştir: döngü içinde `PHASE_IN_GAME` → `inGameCount++`, `PHASE_DESPAWN_WAIT` → `waitCount++`, `PHASE_DESPAWNED` → `releasedCount++`, `PHASE_DESPAWN_STUCK` → `stuckCount++`. Döngüden sonra:
       ```cpp
       if (!m_spawnSummaryDone && m_spawnOk + m_spawnFailed == m_sessions.size())
       {
       	m_spawnSummaryDone = true;
       	// log (same text as F2-03): "BotManager: spawn complete: %u/%u in game, %u failed"
       	// args: m_spawnOk, m_sessions.size(), m_spawnFailed
       }

       if (m_despawnAfterMs != 0 && m_spawnSummaryDone && !m_despawnSummaryDone
       	&& inGameCount == 0 && waitCount == 0)
       {
       	m_despawnSummaryDone = true;
       	size_t poolFree;
       	{
       		std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock());
       		poolFree = g_pMain->m_socketMgr.GetReservedSessionMap().size();
       	}
       	// log: "BotManager: despawn complete: %u/%u released, %u stuck, %u never spawned, pool free %u/%u"
       	// args: releasedCount, m_sessions.size(), stuckCount, m_spawnFailed, poolFree, m_poolSize
       }
       ```
       Spawn özeti log metni F2-03'tekiyle **birebir aynı** kalır.
   - `BeginDespawn(s, now)`:
     ```cpp
     	CUser * pUser = s->m_pUser;
     	// Socket::Disconnect() does nothing without a socket, so run what a real disconnect runs:
     	// OnDisconnect() removes the account/character names, takes the bot out of its region
     	// and queues WIZ_LOGOUT (LogOut() sets m_deleted until the DB thread has saved the bot).
     	pUser->OnDisconnect();
     	s->m_phase = BotSession::PHASE_DESPAWN_WAIT;
     	s->m_despawnStart = now;
     ```
     + log `BotManager: bot <char> despawning (slot <id>)` (`<id>` = `s->m_slotId`).
   - `PollDespawn(s, now)`:
     ```cpp
     	CUser * pUser = s->m_pUser;
     	long long waited = <ms since s->m_despawnStart>;

     	// ReqUserLogOut() clears m_deleted as its last statement (DB thread); LogOut() set it.
     	if (pUser->IsDeleted())
     	{
     		if (waited > DESPAWN_TIMEOUT_MS)
     		{
     			// Releasing now could hand the slot to a new spawn while the DB thread still uses it.
     			s->m_phase = BotSession::PHASE_DESPAWN_STUCK;
     			// log: "BotManager: bot %s despawn TIMEOUT after %lld ms (slot %u kept)"
     		}
     		return;
     	}

     	bool namesCleared = g_pMain->GetUserPtr(s->m_charName, TYPE_CHARACTER) == nullptr
     		&& g_pMain->GetUserPtr(s->m_accountName, TYPE_ACCOUNT) == nullptr;
     	ReleaseSlot(pUser);
     	s->m_pUser = nullptr;
     	s->m_phase = BotSession::PHASE_DESPAWNED;
     	// log (tam biçim):
     	// "BotManager: bot %s despawned (slot %u, logout save %lld ms, updates %u, packets %u, names cleared %s)"
     	// args: charName, m_slotId, waited, m_updateCount, m_packetTotal.load(), namesCleared ? "yes" : "no"
     ```
     `delete` kullanma, `BotSession`'ı vektörden çıkarma. `m_deleted`'in DB thread'i ile IOCP thread'i arasında kilitsiz paylaşıldığını belirten kısa bir yorum ekle (`IsDeleted()` her tick'te yeniden okunur).
   - Yeni kodun log satırları yalnızca `WriteBotLog` ile yazılır (`printf` yok). Tüm `m_pUser`'a dokunan satırlar `TickSessions`/`BeginDespawn`/`PollDespawn` içindedir.

5. **`GameServer/GameServerDlg.cpp`** (`Timer_UpdateSessions`, `CUser * pUser = TO_USER(itr->second);` satırının hemen altı, `#ifndef DEBUG`'tan **önce**), başka satıra dokunma:
   ```cpp
   			// Bot sessions have no socket to time out; BotManager::TickSessions() updates them on the IOCP thread.
   			if (pUser->m_botSink != nullptr)
   				continue;
   ```
   (Fark: +3 / −0, boş satır dahil değil. Tam tanım gerekmez; `IBotSink` yalnızca işaretçi karşılaştırmasında kullanılır. Derleme hatası olursa include **ekleme**, soru olarak yaz.)

6. **`GameServer/DatabaseThread.cpp`** (`ReqUserLogOut`, `:457`): tek satırı değiştir, bir yorum satırı ekle:
   ```cpp
   	// Bots never wrote CURRENTUSER, so there is nothing to delete (ADR-0014).
   	if (m_bLogout != 2 && m_botSink == nullptr)	// zone change logout
   		g_DBAgent.AccountLogout(GetAccountName());
   ```
   (Fark: +2 / −1; satır sonu CRLF, girinti tab.)

7. **Derle:** `./tools/build.sh Release` ve `./tools/build.sh Debug`; ikisi de hatasız. Artımlı derleme uyarı üretmeyebilir: değiştirdiğin her `.cpp`'nin derlendiğinden emin ol (`touch` ile yeniden derlet), `grep -a "warning" | sort -u` çıktısını raporla.

8. Sunucuyu **çalıştırma** (§7 sonu). Kodu commit'le (`[F2-04] ...`), Uygulayıcı Raporu'nu yaz, `Durum`'u `UYGULANDI` yap.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; **yeni uyarı yok** (`Bot\` dosyalarında ve eklenen satırlarda uyarı olmamalı; eski satırların uyarıları kabul; çıktıyı yapıştır).
- [ ] K2: `./tools/build.sh Debug` hatasız biter.
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F2-04` yalnızca §4 tablosundaki 6 dosyayı (+ bu plan dosyası) içerir; `GameServerDlg.cpp` farkı **+3 / −0**, `DatabaseThread.cpp` farkı **+2 / −1** (çıktıyı yapıştır; silinen satır yalnızca `if (m_bLogout != 2)` satırının eski hâli).
- [ ] K4: Gerçek oyuncu davranışı değişmez: `git diff gece/2026-10-02...bot/F2-04 -- GameServer/GameServerDlg.cpp GameServer/DatabaseThread.cpp` çıktısında yalnızca `m_botSink` koşulları vardır; bot olmayan oturum için `Timer_UpdateSessions` aynen çalışır ve `AccountLogout` aynen çağrılır (koşul `m_botSink == nullptr` iken `true`).
- [ ] K5: Thread kuralı: `m_pUser`'a (`OnDisconnect`, `Update`, `IsDeleted`, `GetSocketID`, `ReleaseSlot`) dokunan her satır `TickSessions`/`BeginDespawn`/`PollDespawn` içindedir; bunlar yalnızca `Tick()`'ten çağrılır (`grep -n "m_pUser\|OnDisconnect\|->Update()\|IsDeleted\|ReleaseSlot" GameServer/Bot/BotManager.cpp` çıktısını satır satır açıklayarak yapıştır; `ReleaseSlot` çağrıları yalnızca F2-01'in havuz kurulumu ve `PollDespawn` içindedir).
- [ ] K6: Slot yalnızca DB kaydı bitince iade edilir: `PollDespawn` iade etmeden önce `pUser->IsDeleted()` `false` olmasını bekler; `DESPAWN_TIMEOUT_MS` (30000) aşılırsa slotu **iade etmez** (`PHASE_DESPAWN_STUCK`); `delete` ve vektörden çıkarma yoktur (`grep -n "delete\|erase\|remove" GameServer/Bot/BotManager.cpp` çıktısında oturumla ilgili satır yok; yalnızca F2-02'den `delete m_timerThread`).
- [ ] K7: Sıra: `BeginDespawn` yalnızca `pUser->OnDisconnect()` çağırır ve fazı `PHASE_DESPAWN_WAIT` yapar; `LogOut`/`HandlePacket`/`Disconnect()` doğrudan çağrılmaz (`grep -n "LogOut\|Disconnect" GameServer/Bot/BotManager.cpp` çıktısı: `Disconnect(` yok, `LogOut` yok).
- [ ] K8: `Update()`: `UPDATE_PERIOD_MS = 1000`; `PHASE_IN_GAME` dalında `m_lastUpdate` ile sınırlı; `TickSessions` ilk ifadesi `m_sessions.empty()` iken döner; `m_spawnSummaryDone` artık `TickSessions` girişinde **kullanılmaz** (`sed -n` çıktısı).
- [ ] K9: Ini: `DESPAWN_AFTER_SEC` yalnızca `ENABLED` erken dönüşünden **sonra** okunur (`sed -n` çıktısı, satır numarasıyla); aralık `[0, 86400]` kıskaçlanır; varsayılan `0` iken hiçbir bot çıkmaz.
- [ ] K10: Sabitler ve log biçimi: `UPDATE_PERIOD_MS`/`DESPAWN_TIMEOUT_MS` 1000/30000; §5.4'teki tam metinler `BotManager.cpp`'de bulunur: `despawn after`, `despawning (slot`, `despawned (slot`, `names cleared`, `despawn TIMEOUT after`, `despawn complete:`; `spawn complete: %u/%u in game, %u failed` metni F2-03 ile birebir aynı (`grep -n` çıktısı); yeni kodda `printf` yok.
- [ ] K11: Başarısız spawn'lar F2-03'teki gibi kalır: `FailSession` slot iade etmez, `delete` yapmaz; yalnızca `m_spawnFailed++` eklendi (`sed -n` çıktısı).
- [ ] K12: Kodlama: `file GameServer/Bot/* GameServer/GameServerDlg.cpp GameServer/DatabaseThread.cpp` — `Bot/*` "ASCII … CRLF", diğer ikisi "UTF-8 (with BOM) … CRLF" (çıktıyı yapıştır).
- [ ] K13: `git status --short` boş (yalnızca izinli dosyalar commit'li). Sunucu çalıştırılmadı, `GameServer.ini` ve veritabanı değiştirilmedi.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
git diff --stat gece/2026-10-02...bot/F2-04
git diff gece/2026-10-02...bot/F2-04 -- GameServer/GameServerDlg.cpp GameServer/DatabaseThread.cpp
grep -n "m_pUser\|OnDisconnect\|->Update()\|IsDeleted\|ReleaseSlot\|LogOut\|Disconnect\|delete\|erase" GameServer/Bot/BotManager.cpp
grep -n "DESPAWN_AFTER_SEC\|UPDATE_PERIOD_MS\|DESPAWN_TIMEOUT_MS\|despawn" GameServer/Bot/BotManager.cpp
file GameServer/Bot/* GameServer/GameServerDlg.cpp GameServer/DatabaseThread.cpp
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]`: `ENABLED=1`, `MAX_BOTS=16`, `SPAWN_ON_START=BotWP_K,BotMF_K,BotWP_E,BotPHD_E`, `DESPAWN_AFTER_SEC=20`; sunucuyu `tools/run-servers.sh start` ile aç, ~55 sn bekle (5 sn başlangıç gecikmesi + ~1,3 sn/bot spawn + 20 sn + DB kaydı). Beklenen `Logs/Bot_*.log`: `spawn list: 4 bot(s) queued (...)`, `despawn after 20 s (DESPAWN_AFTER_SEC)`, dört `in game`, `spawn complete: 4/4 in game, 0 failed`, dört `despawning (slot ...)`, dört `despawned (slot ..., logout save <ms> ms, updates <u>, packets <n>, names cleared yes)` (`u` ≈ 17-19, en az 15; tick aralığı ~110 ms olduğundan her çağrı ~1,1 sn'de bir olur), `despawn complete: 4/4 released, 0 stuck, 0 never spawned, pool free 16/16`; sunucu 3/3 UP, çökme yok, GameServer SQL hata satırı yok. Ek: `DESPAWN_AFTER_SEC=0` → despawn satırı yok, botlar açık kalır; `MAX_BOTS=2` + 2 bot → `pool free 2/2`; `ENABLED=0` → bot logu yok ve ini'ye `DESPAWN_AFTER_SEC` eklenmez. Sonra `tools/run-servers.sh stop`, ini yedekten geri yüklenir (bot satırları çıkış kaydıyla güncellenir; `db/002` yeniden uygulanabilir).

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` dosyaları ASCII; kod yorumları İngilizce.
- **Bot sistemi varsayılan kapalı.** `ENABLED=1` ve `SPAWN_ON_START` dolu olsa bile `DESPAWN_AFTER_SEC=0` iken F2-03 davranışı aynıdır (botlar açık kalır); tek fark oyundaki botlara saniyede bir `Update()` çağrılması ve botların `Timer_UpdateSessions`'tan çıkarılmasıdır.
- **`m_deleted` kilitsizdir:** DB thread'i `false` yapar, IOCP thread'i okur (`bool`, atomik değil). x86/MSVC'de pratikte güvenlidir ve gerçek oyuncu yolunda da böyle kullanılır; bunu "düzeltme" amacıyla değiştirme. Güvenlik payı: slot yalnızca `m_deleted == false` görülürse ve 30 sn aşılmadan iade edilir; şüphede slot **sızdırılır**, iade edilmez.
- **Slot ve oturum ömrü:** `PHASE_DESPAWNED` oturum bu planda yeniden kullanılmaz. İade edilen `CUser` bir sonraki `AcquireSlot` + `OnConnect()` ile sıfırlanır (gerçek yeniden bağlantıyla aynı yol); bunu F2-05 sınar.
- **`Update()` ve paket sonrası `Update()`:** `HandlePacket` her pakette `Update()` çağırır; saniyelik çağrı bunun yerine geçmez, ek bir güvencedir. Aynı saniye içinde iki çağrı zararsızdır.
- **Sunucu kapanışı:** `KickOutAllUsers` oyundaki botları kaydeder; `PHASE_DESPAWN_WAIT` içinde kapanış gelirse DB kaydı çifte yazılabilir (gerçek oyuncularda da öyle); kapanış sırasında despawn mantığı eklenmez.
- **ADR-0014 eki:** `AccountLogout` atlanması `docs/adr/ADR-0014-bot-oturumu-hesap-dogrulamasi.md`'de karara bağlandı; ADR'yi değiştirme.
- `OnDisconnect` içindeki dolaylı `PartyRemove` çağrısı ve `UserInOut(INOUT_OUT)`'un AI sunucusuna duyurusu beklenen davranıştır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F2-04` — `<kısa-sha> [F2-04] …`
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
- İncelenen: `gece/2026-10-02...bot/F2-04` @ `<sha>`
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
