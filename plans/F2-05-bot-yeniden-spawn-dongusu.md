# F2-05: Bot yeniden spawn döngüsü (`RESPAWN_CYCLES`) ve spawn/despawn dayanıklılık altyapısı (T-PERF-06; varsayılan kapalı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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
- Branch / commit'ler: `bot/F2-05` — `<kısa-sha> [F2-05] …`
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
- İncelenen: `gece/2026-10-02...bot/F2-05` @ `<sha>`
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
