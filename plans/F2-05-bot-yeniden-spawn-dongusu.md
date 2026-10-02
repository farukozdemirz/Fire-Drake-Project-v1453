# F2-05: Bot yeniden spawn döngüsü (`RESPAWN_CYCLES`) ve spawn/despawn dayanıklılık altyapısı (T-PERF-06; varsayılan kapalı)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F2 — Bot oturumu (`docs/17` §2) |
| Branch | `bot/F2-05` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F2-03 (`KAPANDI`: spawn, `BotSession`), F2-04 (`KAPANDI`: despawn, `PollDespawn`, `m_despawnAfterMs`) |
| İlgili gereksinim / kabul | T-PERF-06 (`docs/15` §4: spawn/despawn 1000 kez, çökme 0, slot sızıntısı 0 — R-CODE-01), AC-ARCH-05 (despawn sonrası kayıt tam), `docs/17` §2 F2 "Kabul" (1000 spawn/despawn döngüsünde çökme ve slot sızıntısı 0) |
| Tahmini büyüklük | S (4 dosya, ~90 satır ekleme) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

F2-04 sonunda her bot bir kez girip bir kez çıkabiliyor; `PHASE_DESPAWNED` oturum bir daha kullanılmıyor. Bu plan sonunda:

1. Yeni `[BOT] RESPAWN_CYCLES=<n>` (varsayılan `0` = tekrar yok): `DESPAWN_AFTER_SEC` > 0 iken her bot, slotu iade edildikten sonra **aynı `BotSession`'la yeniden kuyruğa girer** ve tekrar spawn olur; her bot toplam `1 + n` kez girip çıkar. İade edilen `CUser` bir sonraki `AcquireSlot` + `OnConnect()` ile sıfırlanıp yeniden kullanılır (gerçek bir oyuncunun aynı slota yeniden bağlanmasıyla aynı yol).
2. Döngü boyunca sızıntı ölçülebilir olur: her 50 despawn'da ilerleme satırı, döngü bitince tek özet satırı (spawn/despawn/başarısız/takılı sayısı, havuzda boş slot, isimleri silinmeyen bot sayısı, süre).

Bu, F2'nin son DeepSeek işidir: 1000 spawn/despawn dayanıklılık testi bu plan doğrulanırken **Claude** tarafından çalıştırılır (§7 sonu). `ENABLED=0` (varsayılan) veya `RESPAWN_CYCLES=0` iken sunucu davranışı F2-04'tekiyle aynıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/15` §4 T-PERF-06; `docs/17` §2 F2 "Kabul/Riskler" (R-CODE-01 kilitsiz harita kopyaları, R-CODE-02 çıkış kaydı yarışı).
- `plans/F2-04-bot-cikisi-despawn.md` §2 ve §8 ("Slot ve oturum ömrü"): despawn'ın nasıl çalıştığı. Bu planın **yeniden yazmadığı** mevcut kod, sadece genişlettiği yerler aşağıda.
- İlgili kod (dosya:satır, 2026-10-02'de depoda doğrulandı; hepsi `GameServer/Bot/`):
  - `BotManager.cpp:79-84` `DESPAWN_AFTER_SEC` okuması (`ENABLED` erken dönüşü `:58-59`'dan sonra; `CIni::GetInt` anahtar yoksa varsayılanı ini'ye **yazar**, bu yüzden yeni anahtar da yalnızca bu yolda okunur).
  - `BotManager.cpp:382-387` `ParseSpawnList` sonundaki `despawn after` logu (`!m_sessions.empty()` bloğu içinde).
  - `BotManager.cpp:391-539` `TickSessions`: `:403-499` oturum döngüsü ve `switch`; `:409-416` `PHASE_QUEUED` (tick başına en çok bir `StartSession`); `:501-509` faz sayımı; `:511-520` spawn özeti (`m_spawnOk + m_spawnFailed == m_sessions.size()`); `:522-538` despawn özeti (`inGameCount == 0 && waitCount == 0`).
  - `BotManager.cpp:558-595` `PollDespawn`: `:566-580` `IsDeleted()` bekleme ve `PHASE_DESPAWN_STUCK`; `:582-594` isim kontrolü, `ReleaseSlot`, `m_pUser = nullptr`, `PHASE_DESPAWNED`, `despawned (...)` logu.
  - `BotManager.cpp:597-639` `StartSession`: `GetUserPtr(...)` ile "character/account already online" kontrolü, `AcquireSlot`, `OnConnect()`, `EnableCrypto()`, `m_botSink = s`, `AddAccountName`, `WIZ_SEL_CHAR` DB isteği; `m_phaseStart` burada kurulur. `:641-652` `FailSession` (`m_spawnFailed++`, slot iade **edilmez**).
  - `BotSession.h:16-26` `Phase` sabitleri; `:38-50` alanlar; `BotSession.cpp:4-10` kurucu başlatıcıları ve `m_opcodeCount` sıfırlama döngüsü; `BotSession.cpp:12-22` `OnPacket` (atomikler).
  - `BotManager.h:39-42` kurucu başlatıcı listesi; `:56-61` `private:` oturum alanları (`m_spawnOk`, `m_spawnFailed`, `m_despawnSummaryDone`).
  - `GameServer/User.cpp:45-49` `CUser::OnConnect()` → `Initialize()` (`:54-195`): `m_strUserID`/`m_strAccountID` temizlenir, `m_state = GAME_STATE_CONNECTED`, istatistik/parti/pencere alanları sıfırlanır; `m_botSink` ve `m_deleted` **sıfırlanmaz** (`m_botSink`'i `ReleaseSlot` null yapar, `m_deleted`'i DB thread'i `false` yapar). Yeniden kullanımın doğruluğu çalışma zamanında sınanır (§7 sonu); bu planda `User.cpp`'ye dokunulmaz.
  - `shared/KOSocketMgr.h:200-231` `AcquireReservedSession` (`m_reservedSessions.begin()` = en düşük boş kimlik, yani yeni iade edilen slot çoğunlukla hemen yeniden verilir) ve `ReleaseReservedSession` (kilit altında).

## 3. Kapsam

**Yapılacaklar**

- `BotSession`: tamamlanan despawn sayacı (`m_despawnCount`) ve oturumu yeni bir spawn'a hazırlayan `ResetForRespawn()`.
- `BotManager`: ini `RESPAWN_CYCLES`; `PollDespawn` slotu iade ettikten sonra oturumu yeniden kuyruğa alır; spawn/despawn özet koşulları döngüyle uyumlu hâle gelir; ilerleme ve final özet satırları; despawn sayacı ve isimleri silinmeyen bot sayacı.

**Kapsam dışı (yapılmayacak)**

- `Timer_UpdateSessions` ve diğer `m_activeSessions` kopyalarına (`GameServerDlg.cpp`, `ChatHandler.cpp`, `MagicInstance.cpp`, `PartyHandler.cpp`, `User.cpp`) kilit eklemek. Kilitsiz kopyalar önceden vardır (R-CODE-01); bot döngüsü bunları sıklaştırır. Dayanıklılık testi çökme gösterirse ayrı bir plan (F2-06) yazılır. **Bu planda düzeltme yok.**
- `PHASE_FAILED` ve `PHASE_DESPAWN_STUCK` oturumlarını yeniden kuyruğa almak veya slotlarını iade etmek: bu iki fazdaki oturum döngüden çıkar ve süreç sonuna kadar öyle kalır (F2-03/F2-04 kararı).
- `BotSession` silmek (`delete`), vektörden çıkarmak, oturum eklemek; spawn listesini çalışma zamanında değiştirmek; ini'de bot başına döngü sayısı; bekleme süresi anahtarı (`RESPAWN_DELAY`); `/bot` komutları (S11, F3); sabit IP; gerçek istemciyle bot hesabına giriş.
- `User.cpp`, `User.h`, `DatabaseThread.cpp`, `GameServerDlg.cpp`, `shared/**`, `AIServer`, `LogInServer`, SQL ve `docs/**` dosyalarını değiştirmek; `OnDisconnect`/`LogOut`/`Initialize`/`m_deleted` gövdelerini "iyileştirmek" (ör. `Initialize()`'a eksik alan eklemek: bulursan Uygulayıcı Raporu'na **bulgu** olarak yaz, düzeltme).
- Sunucuyu çalıştırmak, `GameServer.ini`'yi elle düzenlemek, DB'ye bağlanmak (çalışma zamanı doğrulaması Claude'da, §7 sonu).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/BotSession.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotSession.cpp` | değiştir | ASCII, CRLF; kurucu başlatıcıları + `ResetForRespawn()` |
| `GameServer/Bot/BotManager.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotManager.cpp` | değiştir | ASCII, CRLF |

vcxproj/filters **değişmez** (yeni dosya yok). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. **Dal:** `git switch -c bot/F2-05 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `stop` (exe kilidi).

2. **`GameServer/Bot/BotSession.h`:** `m_slotId` alanının hemen altına ekle:
   ```cpp
   	uint32 m_despawnCount;                                 // IOCP thread only, completed despawns (slot returned)
   ```
   Kamu bölümünde `OnPacket`'in altına ekle:
   ```cpp
   	// IOCP thread only. Call after the slot was returned to the pool (m_pUser already null):
   	// puts the session back into PHASE_QUEUED with fresh per-spawn counters.
   	void ResetForRespawn();
   ```
   Başka hiçbir üyeye dokunma.

3. **`GameServer/Bot/BotSession.cpp`:** kurucu başlatıcı listesine `m_slotId(0)`'dan sonra `m_despawnCount(0)` ekle (sıra bildirimle aynı: `m_updateCount(0), m_slotId(0), m_despawnCount(0), m_selectResult(...)`). Dosyanın sonuna ekle:
   ```cpp
   void BotSession::ResetForRespawn()
   {
   	m_pUser = nullptr;
   	m_phase = PHASE_QUEUED;
   	m_selectSeen = false;
   	m_updateCount = 0;
   	m_selectResult = SELECT_PENDING;
   	m_packetTotal = 0;
   	for (int i = 0; i < 256; i++)
   		m_opcodeCount[i] = 0;
   }
   ```
   `OnPacket`'e ve kurucunun gövdesine dokunma. (`m_phaseStart`, `m_inGameSince`, `m_lastUpdate`, `m_despawnStart`, `m_slotId` zaten sonraki `StartSession`/"in game"/`BeginDespawn` adımlarında yeniden kurulur.)

4. **`GameServer/Bot/BotManager.h`:** `private:` bölümündeki `m_despawnSummaryDone`'ın altına ekle ve kurucunun başlatıcı listesinin **sonuna** `m_respawnCycles(0), m_despawnOk(0), m_namesLeft(0)` ekle:
   ```cpp
   	uint32 m_respawnCycles;      // [BOT] RESPAWN_CYCLES: extra spawns per bot after the first (0 = none)
   	uint32 m_despawnOk;          // IOCP thread only: despawns that returned their slot
   	uint32 m_namesLeft;          // IOCP thread only: despawns whose account/character name was still registered
   ```

5. **`GameServer/Bot/BotManager.cpp`:**
   - Sabit (mevcut `DESPAWN_TIMEOUT_MS`'in altına): `static const uint32 CYCLE_PROGRESS_EVERY = 50;`
   - `Startup()`: `DESPAWN_AFTER_SEC` bloğunun (`:79-84`) hemen altında (`std::lock_guard`'tan önce, `ENABLED` erken dönüşünden sonra):
     ```cpp
     	int respawnCycles = ini.GetInt("BOT", "RESPAWN_CYCLES", 0);
     	if (respawnCycles < 0)
     		respawnCycles = 0;
     	else if (respawnCycles > 100000)
     		respawnCycles = 100000;
     	m_respawnCycles = (uint32)respawnCycles;
     ```
   - `ParseSpawnList`: `despawn after` logunun (`:382-387`) **hemen altında**, aynı `if (!m_sessions.empty())` bloğunda:
     ```cpp
     		if (m_respawnCycles != 0)
     		{
     			if (m_despawnAfterMs == 0)
     			{
     				m_respawnCycles = 0;
     				WriteBotLog("BotManager: RESPAWN_CYCLES ignored (DESPAWN_AFTER_SEC is 0)");
     			}
     			else
     			{
     				snprintf(message, sizeof(message),
     					"BotManager: respawn cycles: %u per bot (RESPAWN_CYCLES), %llu spawns planned",
     					(unsigned)m_respawnCycles,
     					(unsigned long long)m_sessions.size() * (1ULL + m_respawnCycles));
     				WriteBotLog(message);
     			}
     		}
     ```
   - `TickSessions()` (sıfır yeni `printf`; yalnızca `WriteBotLog`):
     - `size_t inGameCount = 0, waitCount = 0, releasedCount = 0, stuckCount = 0;` satırını `size_t busyCount = 0, releasedCount = 0, stuckCount = 0;` yap.
     - Döngü sonundaki faz sayımını şöyle değiştir: `PHASE_QUEUED`, `PHASE_WAIT_SELECT`, `PHASE_WAIT_LOADED`, `PHASE_IN_GAME`, `PHASE_DESPAWN_WAIT` → `busyCount++`; `PHASE_DESPAWNED` → `releasedCount++`; `PHASE_DESPAWN_STUCK` → `stuckCount++` (`PHASE_FAILED` sayılmaz). Sayım `switch`'ten **sonra** yapılır; böylece `PollDespawn` içinde aynı turda yeniden kuyruğa alınan oturum `PHASE_QUEUED` olarak `busyCount`'a girer.
     - Spawn özeti koşulunu `m_spawnOk + m_spawnFailed >= m_sessions.size()` yap (`==` değil: yeniden spawn'lar sayaçları aynı turda aşabilir). Log metni **birebir aynı** kalır (`spawn complete: %u/%u in game, %u failed`).
     - Despawn özeti koşulunu `m_despawnAfterMs != 0 && m_spawnSummaryDone && !m_despawnSummaryDone && busyCount == 0` yap. Mevcut `despawn complete: ...` log metni ve argümanları **aynen kalır**. Hemen ardından (aynı blokta) ekle:
       ```cpp
       		if (m_respawnCycles != 0)
       		{
       			long long elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(
       				std::chrono::steady_clock::now() - m_firstTickTime).count();
       			// log, tam biçim:
       			// "BotManager: respawn cycles done: %u spawns, %u despawns, %u failed, %u stuck, %u names left, pool free %u/%u, elapsed %lld s"
       			// args: m_spawnOk, m_despawnOk, m_spawnFailed, stuckCount, m_namesLeft, poolFree, m_poolSize, elapsedSec
       		}
       ```
       (`poolFree` mevcut bloktaki değişkendir; `message` tamponu en az 288 bayt kalsın.)
   - `PollDespawn`: `namesCleared`/`ReleaseSlot`/`PHASE_DESPAWNED`/`despawned (...)` log sırası **aynen kalır**; `despawned (...)` logundan **sonra** ekle:
     ```cpp
     	s->m_despawnCount++;
     	m_despawnOk++;
     	if (!namesCleared)
     		m_namesLeft++;

     	if (m_respawnCycles != 0 && m_despawnOk % CYCLE_PROGRESS_EVERY == 0)
     	{
     		// log (pool free read under the socket manager lock, same as the despawn summary):
     		// "BotManager: cycle progress: %u spawns, %u despawns, %u failed, pool free %u/%u"
     		// args: m_spawnOk, m_despawnOk, m_spawnFailed, poolFree, m_poolSize
     	}

     	// Slot is back in the pool and the DB save is done, so the same session can spawn again.
     	if (s->m_despawnCount <= m_respawnCycles)
     		s->ResetForRespawn();
     ```
     (`ResetForRespawn()` log satırlarındaki `m_updateCount`/`m_packetTotal` değerleri yazıldıktan sonra çağrıldığı için sıfırlama logu bozmaz; sırayı değiştirme.)
   - `StartSession`, `FailSession`, `BeginDespawn`, `Tick`, `Startup`'ın havuz kurulumu ve diğer her şey **değişmez**. `delete`/`erase` ekleme.

6. **Derle:** `./tools/build.sh Release` ve `./tools/build.sh Debug`; ikisi de hatasız. Değiştirdiğin her `.cpp`'yi `touch` ile yeniden derlet; `grep -a "warning" | sort -u` çıktısını raporla (yeni uyarı olmamalı, özellikle kullanılmayan değişken: `inGameCount`/`waitCount` tamamen kaldırılmış olmalı).

7. Sunucuyu **çalıştırma** (§7 sonu). Kodu commit'le (`[F2-05] ...`), Uygulayıcı Raporu'nu yaz, `Durum`'u `UYGULANDI` yap.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; **yeni uyarı yok** (`Bot\` dosyalarında uyarı olmamalı; çıktıyı yapıştır).
- [ ] K2: `./tools/build.sh Debug` hatasız biter.
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F2-05` yalnızca §4'teki 4 dosyayı (+ bu plan dosyası) içerir; plan dosyasında yalnızca `Durum` ve Uygulayıcı Raporu değişmiştir (çıktıyı yapıştır).
- [ ] K4: Ini: `RESPAWN_CYCLES` yalnızca `ENABLED` erken dönüşünden **sonra** okunur (`sed -n` çıktısı, satır numarasıyla); aralık `[0, 100000]` kıskaçlanır; varsayılan `0` iken hiçbir bot yeniden spawn olmaz; `DESPAWN_AFTER_SEC == 0` iken `RESPAWN_CYCLES` yok sayılır ve `ignored` logu yazılır.
- [ ] K5: Yeniden kuyruğa alma yalnızca `PollDespawn` içinde, `ReleaseSlot` + `m_pUser = nullptr` + `PHASE_DESPAWNED` **sonrasında** yapılır (`sed -n` çıktısı). `PHASE_FAILED` ve `PHASE_DESPAWN_STUCK` hiçbir yerde `PHASE_QUEUED`'a dönmez (`grep -n "PHASE_QUEUED\|ResetForRespawn" GameServer/Bot/*.cpp GameServer/Bot/*.h` çıktısını satır satır açıklayarak yapıştır).
- [ ] K6: Toplam spawn sayısı `1 + RESPAWN_CYCLES`: `m_despawnCount <= m_respawnCycles` koşulu ile (`RESPAWN_CYCLES=0` → hiç yeniden kuyruk; `=1` → ikinci spawn, ikinci despawn'dan sonra yok). Bu mantığı iki satırlık bir tabloyla açıkla (rapor).
- [ ] K7: `ResetForRespawn()` yalnızca §5.3'teki alanları sıfırlar; atomikler (`m_selectResult`, `m_packetTotal`, `m_opcodeCount`) atama ile sıfırlanır (kopya/yeniden kurma yok).
- [ ] K8: Özetler: `spawn complete: %u/%u in game, %u failed` ve `despawn complete: ...` metinleri F2-03/F2-04 ile **birebir aynı**; `>=` ve `busyCount == 0` koşulları vardır; yeni metinler `respawn cycles:`, `RESPAWN_CYCLES ignored`, `cycle progress:`, `respawn cycles done:` `BotManager.cpp`'de tam biçimde bulunur (`grep -n` çıktısı); yeni kodda `printf` yok.
- [ ] K9: Thread kuralı: `m_pUser`'a, `ResetForRespawn`, `m_despawnOk`, `m_namesLeft`, `m_spawnOk` sayaçlarına dokunan her satır `TickSessions`/`StartSession`/`FailSession`/`BeginDespawn`/`PollDespawn` içindedir (`Tick()`'ten çağrılan IOCP thread'i); `BotSession::ResetForRespawn` yalnızca `PollDespawn`'dan çağrılır (`grep -n` çıktısı).
- [ ] K10: Kapalıyken davranış değişmez: `RESPAWN_CYCLES=0` yolunda `PollDespawn`'daki tek fark sayaç artışları ve `m_despawnCount <= 0` (hiç doğru değil) karşılaştırmasıdır; `ENABLED=0` yolunda hiçbir yeni kod çalışmaz (`Startup()` `:58-59` erken dönüşü öncesinde yeni satır yok).
- [ ] K11: Kodlama: `file GameServer/Bot/*` hepsi "ASCII text, with CRLF line terminators" (çıktıyı yapıştır); girinti tab, Allman, yorumlar İngilizce.
- [ ] K12: `git status --short` boş (yalnızca izinli dosyalar commit'li). Sunucu çalıştırılmadı, `GameServer.ini` ve veritabanı değiştirilmedi.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
git diff --stat gece/2026-10-02...bot/F2-05
grep -n "RESPAWN_CYCLES\|m_respawnCycles\|PHASE_QUEUED\|ResetForRespawn\|m_despawnCount\|m_despawnOk\|m_namesLeft\|busyCount" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "respawn cycles\|cycle progress\|spawn complete\|despawn complete" GameServer/Bot/BotManager.cpp
file GameServer/Bot/*
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile (her senaryodan sonra ini yedekten geri yüklenir):
1. **Gerilemesiz:** `ENABLED=1, MAX_BOTS=16, SPAWN_ON_START=BotWP_K,BotMF_K,BotWP_E,BotPHD_E, DESPAWN_AFTER_SEC=20`, `RESPAWN_CYCLES` yok/`0` → F2-04'teki gibi: dört `in game`, dört `despawned`, `despawn complete: 4/4 released, 0 stuck, 0 never spawned, pool free 16/16`; `respawn cycles` satırı **yok**.
2. **Küçük döngü:** aynı dört bot, `DESPAWN_AFTER_SEC=2, RESPAWN_CYCLES=4` → `respawn cycles: 4 per bot (RESPAWN_CYCLES), 20 spawns planned`, 20 `in game`, 20 `despawned (... names cleared yes)`, `respawn cycles done: 20 spawns, 20 despawns, 0 failed, 0 stuck, 0 names left, pool free 16/16`.
3. **`RESPAWN_CYCLES` + `DESPAWN_AFTER_SEC=0`:** `RESPAWN_CYCLES ignored` logu, botlar açık kalır.
4. **T-PERF-06 (1000 döngü):** 12 botun tamamı, `DESPAWN_AFTER_SEC=1, RESPAWN_CYCLES=83` (12 × 84 = 1008 spawn), beklenen ~5 dk: sunucu 3/3 UP (çökme 0), `respawn cycles done: 1008 spawns, 1008 despawns, 0 failed, 0 stuck, 0 names left, pool free 16/16`, her 50 despawn'da ilerleme satırı, GameServer SQL hata satırı yok, işlem bellek (working set) başlangıç/bitiş farkı kaydedilir. Bot kayıtları `db/002` ile sıfırlanabilir; **USERDATA satırları okunmaz** (gizlilik kuralı; kayıt bütünlüğü `logout save <ms> ms` satırlarından ve SQL hatası olmamasından izlenir).
5. **`ENABLED=0`:** yeni bot log satırı 0, ini'ye `RESPAWN_CYCLES` yazılmaz.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` dosyaları ASCII; kod yorumları İngilizce.
- **Bot sistemi varsayılan kapalı.** `ENABLED=1` olsa bile `RESPAWN_CYCLES` anahtarı yoksa/`0` ise F2-04 davranışı aynıdır.
- **Slot yeniden kullanımı:** `AcquireReservedSession` en düşük boş kimliği verdiğinden bot çoğu zaman az önce iade edilen `CUser`'ı hemen geri alır. `Initialize()` her alanı sıfırlamayabilir (`m_deleted`, `m_botSink` bilinçli olarak dışarıda); eksik sıfırlama bulursan düzeltme, Uygulayıcı Raporu'nda **bulgu** olarak belgele (§3 kapsam dışı).
- **Takılma güvenliği değişmez:** şüphede slot **sızdırılır** (F2-04): `PHASE_DESPAWN_STUCK`/`PHASE_FAILED` oturum döngüden çıkar, havuz küçülür; özet satırındaki `stuck`/`failed`/`pool free` bunu gösterir. Döngü bitmeden havuz tükenirse kalan oturumlar `no free slot` ile `FailSession`'a düşer ve özet yine yazılır (`busyCount` 0'a iner).
- **Bilinen risk (R-CODE-01):** `Timer_UpdateSessions` (30 sn) ve diğer zamanlayıcılar `m_activeSessions`'ı kilitsiz kopyalar; döngü bu haritayı saniyede birkaç kez değiştirir. Dayanıklılık testi bunu açığa çıkarırsa sonuç F2-06'nın girdisidir; bu planda ele alınmaz.
- Log hacmi: 1008 spawn ≈ 4000 satır (~0,6 MB); `Bot_*.log` günlük dosyadır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F2-05` (taban: `gece/2026-10-02`)
  - `1dea271` `[F2-05] Bot yeniden spawn dongusu: RESPAWN_CYCLES, ResetForRespawn, ilerleme ve final ozet loglari` (kod)
  - Rapor + `Durum: UYGULANDI` commit'i bu raporun altındaki commit'tir.
- Değişen dosyalar ve neden:
  - `GameServer/Bot/BotSession.h`: `m_despawnCount` alanı + `ResetForRespawn()` bildirimi.
  - `GameServer/Bot/BotSession.cpp`: kurucuya `m_despawnCount(0)`; `ResetForRespawn()` gövdesi (yalnızca §5.3'teki alanlar).
  - `GameServer/Bot/BotManager.h`: `m_respawnCycles`, `m_despawnOk`, `m_namesLeft` alanları ve kurucu başlatıcıları.
  - `GameServer/Bot/BotManager.cpp`: `CYCLE_PROGRESS_EVERY=50`; `Startup()`'ta `RESPAWN_CYCLES` okuma/kıskaç; `ParseSpawnList`'te `ignored`/`respawn cycles` logları; `TickSessions`'ta `busyCount` faz sayımı, `>=` spawn özeti, `busyCount == 0` despawn özeti + `respawn cycles done`; `PollDespawn`'da sayaçlar, `cycle progress` logu ve `ResetForRespawn()` koşulu.
- Derleme sonucu (`./tools/build.sh Release`, rc=0; son satırlar):
  ```
    BotManager.cpp
    BotSession.cpp
    proj-AIServer.vcxproj -> ...\x86-Release\Server\AIServer.exe
    Kod üretiliyor
    0 of 13169 functions ( 0.0%) were compiled, the rest were copied from previous compilation.
    proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  Bu turda Release'te hiç `warning` satırı yok. `./tools/build.sh Debug`, rc=0; yalnızca önceden var olan iki `GameServerDlg.cpp` uyarısı (C4267, satır 1143 ve 1802); `Bot\` dosyalarında uyarı yok.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (Release rc=0; bu turda uyarı yok — Bot dosyaları dahil).
  - K2 ✔ (Debug rc=0; iki uyarı eski `GameServerDlg.cpp` satırlarında).
  - K3 ✔ (`git diff --stat gece/2026-10-02...bot/F2-05`: 4 Bot dosyası + plan dosyası; aşağıda).
  - K4 ✔; `grep -n`: `:58 m_enabled = ini.GetBool`, `:59 if (!m_enabled)`, `:87 int respawnCycles = ini.GetInt(...RESPAWN_CYCLES...)` — okuma erken dönüşten sonra; `[0,100000]` kıskaç `:88-91`; `DESPAWN_AFTER_SEC == 0` iken `m_respawnCycles = 0` + `ignored` logu `:397-403` (yalnızca `!m_sessions.empty()` bloğunda, spawn listesi boşsa hiç okunmaz/uygulanmaz).
  - K5 ✔; sıra `PollDespawn` içinde `:624 ReleaseSlot(pUser)`, `:625 s->m_pUser = nullptr`, `:626 s->m_phase = PHASE_DESPAWNED`, `:636 m_despawnCount++`, `:658 ResetForRespawn()`. `PHASE_QUEUED` atamaları yalnızca `BotSession.cpp:6` (kurucu) ve `BotSession.cpp:27` (`ResetForRespawn`); `PHASE_FAILED`/`PHASE_DESPAWN_STUCK` hiçbir yerde `PHASE_QUEUED`'a dönmez. `ResetForRespawn` yalnızca `BotManager.cpp:658`'den çağrılır (grep çıktısı 33 satır, yukarıdaki komut).
  - K6 ✔ tablo:
    | `RESPAWN_CYCLES` | despawn sonrası `m_despawnCount` | `<= m_respawnCycles`? | toplam spawn |
    |---|---|---|---|
    | 0 | 1 | 1<=0 → hayır | 1 |
    | 1 | 1 → 2 | 1<=1 evet, 2<=1 hayır | 2 |
    | n | k=1..n+1 | k<=n → n kez evet | 1+n |
    `RESPAWN_CYCLES=4`, 4 bot → 4×5 = 20 spawn (plan §7.2 ile uyumlu).
  - K7 ✔ (`ResetForRespawn` gövdesi §5.3 ile birebir: `m_pUser`, `m_phase`, `m_selectSeen`, `m_updateCount`, `m_selectResult`, `m_packetTotal`, `m_opcodeCount[256]`; atomiklere düz atama, kopya/yeniden kurma yok).
  - K8 ✔; `spawn complete: %u/%u in game, %u failed` (`:544`) ve `despawn complete: ...` (`:561`) metinleri F2-03/F2-04 ile birebir; `>=` `:538`, `busyCount == 0` `:550`. Yeni metinler: `RESPAWN_CYCLES ignored` `:402`, `respawn cycles:` `:407`, `respawn cycles done:` `:572`, `cycle progress:` `:650`. Yeni kodda `printf` yok (`git diff | grep "^+" | grep -i printf` boş).
  - K9 ✔; `m_pUser`/sayaç satırları yalnızca `TickSessions` (`:426-579`), `BeginDespawn` (`:581-598`), `PollDespawn` (`:600-659`), `StartSession` (`:661-707`), `FailSession` içinde; hepsi `Tick()` → IOCP thread zincirinden. `ResetForRespawn()` yalnızca `PollDespawn`'dan (`:658`).
  - K10 ✔; `ENABLED` erken dönüşü `:59`'da; ilk yeni satır `:87` (sonrasında). `RESPAWN_CYCLES=0` yolunda `PollDespawn` farkı: `:636-639` sayaç artışları + `:657` `m_despawnCount <= 0` (hiç doğru değil) karşılaştırması. Ayrıca §7'deki "respawn cycles yok" gözlemi plan gereği.
  - K11 ✔; `file GameServer/Bot/*`: beşi de `ASCII text, with CRLF line terminators`; girinti tab, Allman, yorumlar İngilizce.
  - K12 ✔; tur sonunda `git status --short` boş; sunucu çalıştırılmadı (`run-servers.sh status`: 0/3, hepsi `[DOWN]`), `GameServer.ini` ve DB'ye dokunulmadı.
- Plandan sapmalar ve gerekçeleri: Yok. §5.5'teki iki yorum bloğu ("log, tam biçim"/"log") gerçek `snprintf` çağrılarına çevrildi; formatlar ve argüman sırası plandakiyle birebir. `ignored` logu plan gereği `if (!m_sessions.empty())` bloğunun içinde kaldı.
  - Bulgular (kapsam dışı, düzeltme yapılmadı): Bu turda `Initialize()`'da eksik sıfırlama bulunmadı (kod okuması; doğrulaması §7.2/§7.4 koşusunda Claude'da). `RESPAWN_CYCLES` yalnızca `SPAWN_ON_START` listesi boş değilken okunur/uygulanır; liste boşsa anahtar ini'ye yazılır ama log üretilmez (plan §5.5'in `!m_sessions.empty()` koşulu).
- Açık sorular: Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F2-05` @ `ff7bacf` (uygulama `1dea271`, rapor `ff7bacf`); çalışma ağacı temiz; otonom gece modu (`AUTO_LOOP=1`), birleştirme/push yapılmadı.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `BotManager.cpp`/`BotSession.cpp` touch'lanıp `./tools/build.sh Release` rc=0 (tam kod üretimi, 13169 işlev). Uyarılar yalnızca `UpgradeHandler.cpp(634)/(862) C4789` (eski satırlar, F2-04 doğrulamasında da var); `Bot\` altında uyarı yok |
| K2 | ✔ | Aynı iki dosya touch'lanıp `./tools/build.sh Debug` rc=0; `warning`/`error` satırı yok; log'da `BotManager.cpp`, `BotSession.cpp` yeniden derlendi |
| K3 | ✔ | `git diff --stat gece/2026-10-02...bot/F2-05`: `BotManager.cpp` 78, `BotManager.h` 7, `BotSession.cpp` 14, `BotSession.h` 5, plan dosyası 46; 5 dosya, +133/−17. Plan dosyasında yalnızca `@@ -2,7` (`Durum`) ve `@@ -222,16` (Uygulayıcı Raporu) hunk'ları; merge commit yok |
| K4 | ✔ | `BotManager.cpp:58-60` `ENABLED` erken dönüşü, `:87` `RESPAWN_CYCLES` okuması (sonra), `:88-91` `[0,100000]` kıskaç; `:397-403` `DESPAWN_AFTER_SEC==0` → `m_respawnCycles=0` + `ignored` logu. Çalışma zamanı: senaryo 1 (anahtar yok) `respawn cycles` satırı yok, 4 spawn/4 despawn; senaryo 3 (`DESPAWN_AFTER_SEC=0, RESPAWN_CYCLES=3`) `RESPAWN_CYCLES ignored (DESPAWN_AFTER_SEC is 0)`, botlar 100+ sn açık kaldı |
| K5 | ✔ | Sıra `PollDespawn`: `:624 ReleaseSlot`, `:625 m_pUser = nullptr`, `:626 PHASE_DESPAWNED`, `:636 m_despawnCount++`, `:657-658 ResetForRespawn()`. `grep`: `PHASE_QUEUED` atamaları yalnızca `BotSession.cpp:6` (kurucu) ve `:27` (`ResetForRespawn`); `BotManager.cpp:434,526` yalnızca `case`/sayım okuması. `PHASE_FAILED` (`:709`) ve `PHASE_DESPAWN_STUCK` (`:611`) atama sonrası hiçbir yerde `PHASE_QUEUED`'a dönmez |
| K6 | ✔ | `:657` `m_despawnCount <= m_respawnCycles`: n=0 → 1<=0 hayır (1 spawn); n=1 → 1<=1 evet, 2<=1 hayır (2 spawn); genel `1+n`. Çalışma zamanı: `RESPAWN_CYCLES=4` → botların her biri 5 kez `in game` (toplam 20); `=83` → her bot 84 kez (toplam 1008) |
| K7 | ✔ | `BotSession.cpp:24-33` gövde plan §5.3 ile birebir; yalnızca `m_pUser`, `m_phase`, `m_selectSeen`, `m_updateCount`, `m_selectResult`, `m_packetTotal`, `m_opcodeCount[256]`; atomiklere düz atama |
| K8 | ✔ | `grep -n`: `:407 respawn cycles:`, `:402 RESPAWN_CYCLES ignored`, `:650 cycle progress:`, `:572 respawn cycles done:`; `:544 spawn complete: %u/%u in game, %u failed` ve `:561 despawn complete: ...` metinleri değişmedi; `:538` `>=`, `:550` `busyCount == 0`; eklenen satırlarda `printf` yalnızca `snprintf`. Çalışma zamanı: tüm metinler loga planın biçimiyle yazıldı |
| K9 | ✔ | `m_pUser`/`m_despawnOk`/`m_namesLeft`/`m_spawnOk`/`ResetForRespawn` yalnızca `TickSessions` (`:426-579`), `BeginDespawn`, `PollDespawn`, `StartSession`, `FailSession` içinde; `ResetForRespawn()` tek çağrı `:658` (`PollDespawn`). Hepsi `Tick()` → IOCP thread'i; çalışma zamanında `tick OK on IOCP thread` |
| K10 | ✔ | `ENABLED=0` erken dönüşü `:59` öncesinde yeni satır yok (ilk yeni satır `:87`). `RESPAWN_CYCLES=0` yolunda `PollDespawn` farkı yalnızca `:636-639` sayaçları ve `:657` (`<= 0` hiç doğru değil). Çalışma zamanı: senaryo 1 F2-04 çıktısıyla aynı (`logout save 109-110 ms, updates 18, packets 17`, `pool free 16/16`); senaryo 5 (`ENABLED=0, RESPAWN_CYCLES=5`) bot log satırı 0, ini'ye ek anahtar yazılmadı |
| K11 | ✔ | `file GameServer/Bot/*`: beşi "ASCII text, with CRLF line terminators"; eklenen satırlarda boşluk girintisi yok (`grep`), Allman, yorumlar İngilizce |
| K12 | ✔ | `git status --short` boş (derleme ve çalışma zamanı testlerinden sonra da); uygulayıcı sunucu çalıştırmadı (doğrulama öncesi 0/3 `[DOWN]`) |

- Çalışma zamanı doğrulaması (Release, `GameServer.ini`'ye `[BOT]` eklendi, her senaryodan sonra yedekten geri yüklendi, md5 `d1646328...` aynı; sunucular kapatıldı, 0/3 UP):
  1. **Gerilemesiz** (4 bot, `DESPAWN_AFTER_SEC=20`, `RESPAWN_CYCLES` yok): dört `in game`, dört `despawned (... logout save 109-110 ms, names cleared yes)`, `despawn complete: 4/4 released, 0 stuck, 0 never spawned, pool free 16/16`; `respawn cycles` satırı yok.
  2. **Küçük döngü** (4 bot, `DESPAWN_AFTER_SEC=2, RESPAWN_CYCLES=4`): `respawn cycles: 4 per bot (RESPAWN_CYCLES), 20 spawns planned`; 20 `in game`, 20 `despawned`, 20 `names cleared yes`, 0 `FAILED`/`TIMEOUT`; sırayla `spawn complete: 4/4`, `despawn complete: 4/4 released ... pool free 16/16`, `respawn cycles done: 20 spawns, 20 despawns, 0 failed, 0 stuck, 0 names left, pool free 16/16, elapsed 23 s` (son iki satır logun sonunda).
  3. **`RESPAWN_CYCLES=3` + `DESPAWN_AFTER_SEC=0`:** `RESPAWN_CYCLES ignored (DESPAWN_AFTER_SEC is 0)`, dört bot açık, despawn satırı yok (40 sn+ sonra da).
  4. **T-PERF-06** (12 bot, `DESPAWN_AFTER_SEC=1, RESPAWN_CYCLES=83`): `1008 spawns planned`; 1008 `in game`, 1008 `despawned`, 1008 `names cleared yes`, 0 `FAILED`, 0 `TIMEOUT`; her bot tam 84 kez ve hep aynı slotta (2984-2995; `Initialize()` ile yeniden kullanım sorunsuz); `logout save` min/maks/ort 106/112/108,8 ms; 20 `cycle progress` satırı (`50 despawns` … `1000 despawns`); `respawn cycles done: 1008 spawns, 1008 despawns, 0 failed, 0 stuck, 0 names left, pool free 16/16, elapsed 235 s`; sunucu 3/3 UP (çökme 0); `GameServer.log` satır sayısı 32 → 32 (yeni SQL hatası yok); `GameServer.exe` bellek ~T+45 sn / koşu sonu: working set 133,4 → 133,7 MB, private 175,8 → 175,8 MB, handle 246 → 243.
  5. **`ENABLED=0`** (`RESPAWN_CYCLES=5` ile): bot log satırı 0; ini'ye `RESPAWN_CYCLES` yazılmadı (yalnızca benim eklediğim satır vardı, geri yüklendi).
- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. Not (T-PERF-06 sonucu): 1008 spawn/despawn'da çökme 0, slot sızıntısı 0 (`pool free 16/16`), isim sızıntısı 0, bellek sabit. `docs/17` F2 "Kabul" 1000 döngü koşulu bu koşuyla karşılandı; R-CODE-01 (kilitsiz `m_activeSessions` kopyaları) bu koşuda tetiklenmedi. Koşu ~4 dk olduğundan `Timer_UpdateSessions` (30 sn) yalnızca ~8 kez çalıştı; yarışın yokluğunu kanıtlamaz, F2-04 raporundaki dar pencere notu geçerli. Uzun soak (ör. 10 000 döngü) gerekirse proje sahibi kararı.
  2. Not (düşük): `TickSessions` `:538` `>=` ile `spawn complete` özeti, bir botun yeniden spawn'ı başka botun ilk spawn'ından önce biterse metindeki `%u/%u`'i yanıltabilir (yalnızca log; bu koşularda hep `4/4`, `12/12`). Düzeltme gerekmez.
  3. Not: Uygulayıcı raporundaki "Release'de hiç `warning` satırı yok" artımlı derlemeye dayanıyor; tam kod üretiminde `UpgradeHandler.cpp` C4789 uyarıları (eski, F2-02/F2-04 notlarıyla aynı) görünür. K1 ölçütü (Bot'ta ve yeni satırlarda uyarı yok) yine karşılanır.
  4. Not: Koşular bot satırlarını DB'de logout kaydıyla günceller (gerçek oyuncuyla aynı yol); gerekirse `db/002` yeniden uygulanır, USERDATA okunmadı (gizlilik kuralı).
  5. Not: Uygulayıcı raporu dürüst: commit listesi, dosya listesi, satır numaraları, K4/K5/K8 iddiaları gerçekle uyuşuyor; plandan sapma yok (yalnızca yorum bloklarını `snprintf`'e çevirmek, plan zaten bunu istiyordu).
- Düzeltme talimatı: yok (karar DOĞRULANDI).

```
(yok)
```
