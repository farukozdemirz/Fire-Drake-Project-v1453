# F3-03: `ScenarioRunner`: senaryo dosyasından bot spawn → maç → despawn döngüsü (`/bot scenario run|stop|status`)

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | F3 — Telemetri ve test altyapısı (`docs/17` §2, Görev 3 "Senaryo dosyaları" ve Kapsam'daki "maç başlat/bitir") |
| Branch | `bot/F3-03` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F3-02 (`KAPANDI`: `match start\|end`, `Telemetry::BeginMatch/EndMatch/IsMatchActive`), F2-06 (`KAPANDI`: komut çekirdeği, ADR-0015), F2-03/F2-04 (`KAPANDI`: spawn/despawn) |
| İlgili gereksinim / kabul | `docs/17` F3 Kapsam ("`ScenarioRunner` (senaryo YAML, envanter doldurma, maç başlat/bitir)"); `docs/15` §6 madde 1 ("seed listesi ve tekrar sayısı senaryo dosyasında sabittir"); ADR-0015 (Ek, F3-03) |
| Tahmini büyüklük | M (7 dosya, ~600 satır ekleme) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Bir **senaryo dosyası** (`./Scenarios/<ad>.yaml`, YAML'ın küçük bir alt kümesi) okuyup şunu otomatik yapan `ScenarioRunner` eklemek: listedeki botları spawn et → hepsi `in game` olunca `match start <senaryo_kimliği> <seed>` → `duration_sec` dolunca `match end completed` → botları despawn et → sıradaki seed/tekrar için başa dön. Bütün koşular bitince tek özet satırı yazar. Komutlar: `/bot scenario run <ad>`, `/bot scenario stop`, `/bot scenario status` (konsol ve `BotCommands.txt`; ADR-0015 komut çekirdeği, yalnızca IOCP thread'i).

Bu, `docs/15` §6'daki değerlendirme protokolünün (sabit seed listesi + tekrar sayısı) ve sonraki fazların (betikli test dizileri, 8v8) altyapısıdır. Bu planda botlar **hareketsizdir ve bir şey yapmaz**; senaryo yalnızca "kim girer, ne kadar kalır, hangi maç kimliğiyle ölçülür"i yönetir.

Bu plan sonunda `ENABLED=0` (varsayılan) iken sunucu davranışı ve dosya sistemi aynıdır; `ENABLED=1` iken **`scenario` komutu verilmedikçe** davranış F3-02'deki ile birebir aynıdır (yeni ini anahtarı yok).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0015-bot-calisma-zamani-komut-kanali.md` ("Ek (F3-03)": `scenario` komutu kararı) ve `docs/adr/ADR-0007-telemetri-formati-ve-depolama.md` (maç dosyası kararı).
- `docs/13_BOT_ARCHITECTURE_AND_DATA_MODEL.md` §5.1 (senaryo YAML örneği; bu plan yalnızca **alt kümesini** destekler, §3'te), §10 (komutlar).
- `docs/15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md` §6 (değerlendirme protokolü özeti).
- `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md` §3.2 (`MATCH_START/END`), §7 (geçersiz maç sayımı: spawn hatası geçersiz maçtır).
- `docs/adr/ADR-0005-bot-tick-thread-modeli.md`: bot durumu yalnızca IOCP thread'inde değişir.
- `plans/F3-02-mac-baslangic-bitis-ve-summary.md` ve `plans/F2-06-bot-calisma-zamani-komutlari.md` (Doğrulama Raporları dahil): bu plan onların komut ve maç kodunu **çağırır**, içlerini değiştirmez.
- İlgili kod (dosya:satır, 2026-10-02'de depoda doğrulandı, taban `aa5e325`):
  - `GameServer/Bot/BotManager.h:44-48` kurucu (başlatıcı listesi `m_namesLeft(0) {}` ile biter); `:56-64` komut bildirimleri (`CommandSpawn` `:60`, `CommandDespawn` `:61`, `CommandList` `:62`, `CommandMatch` `:63`, `FindSession` `:64`); `:71` `BeginDespawn`; `:74` `m_sessions` (`std::vector<BotSession *>`, oturumlar **asla silinmez**); `:86` `m_poolSize`; `:105-107` son veri üyeleri.
  - `GameServer/Bot/BotManager.cpp:38-52` `WriteBotLog` (statik); `:64-73` `FindBotEntry` (statik, `BOT_TABLE` `:56-62`); `:75-89` `PhaseName` (statik); `:91-99` `Trim` (statik); `:345-387` `Tick()` (`ProcessCommands()` `:382`, `TickSessions()` `:383`); `:545-579` `ExecuteCommand()` (`match` dalı `:571-572`, bilinmeyen komut metni `:575-576`); `:581-590` `FindSession()`; `:592-649` `CommandSpawn()` (adları `SplitNames` ile virgül/boşlukla ayırır; `DESPAWNED` oturumu `ResetForRespawn()` ile yeniden kuyruğa alır, `FAILED`/başka fazdakini "ignored" der); `:748-907` `CommandMatch()` (`start <senaryo> <seed>` → `Telemetry::BeginMatch`; `end <sonuç>` → `EndMatch`; bileşimi `PHASE_IN_GAME` oturumlardan kurar); `:1187-1202` `BeginDespawn()`.
  - `GameServer/Bot/BotSession.h:16-26` `Phase` sıralaması; `:42-52` alanlar (hepsi yalnızca IOCP thread'i).
  - `GameServer/Bot/Telemetry.h:70` `IsEnabled(TelemetryLevel)`, `:95` `IsMatchActive()`.
  - `GameServer/proj-GameServer.vcxproj:193-195` (`ClCompile Bot\...`), `:286-289` (`ClInclude Bot\...`); `.filters:84-92` ve `:206-217` aynı kalıp.
- Uygulayıcı önce şunu kontrol etsin: yukarıdaki satır numaraları kayabilir; fonksiyon adlarıyla yeniden bul ve kaymayı raporda yaz.

## 3. Kapsam

**Yapılacaklar**

- Yeni `ScenarioRunner` sınıfı (`GameServer/Bot/ScenarioRunner.h/.cpp`): senaryo dosyası ayrıştırma, durum makinesi, üç alt komut (`run`, `stop`, `status`).
- `BotManager`: `ScenarioRunner m_scenario` üyesi (`friend class ScenarioRunner`), `ExecuteCommand`'a `scenario` dalı, `Tick()` içinde `m_scenario.Tick(now)` çağrısı, ad doğrulama yardımcısı `IsKnownBotName`.
- Örnek senaryo dosyası `bots/config/scenario_smoke_2bot.yaml`.
- Yeni ini anahtarı **yok**. `Telemetry`, `BotSession`, `ChatHandler.cpp`, `GameServerDlg.*` **değişmez**.

**Desteklenen senaryo dosyası biçimi** (`./Scenarios/<ad>.yaml`; GameServer çalışma dizinine göre; `<ad>` = `[A-Za-z0-9_-]{1,40}`, nokta/eğik çizgi yok)

Dosya en çok 4096 bayt ve 64 satır; satır en çok 255 karakter. Her satır: boş, yorum (`#` ile başlayan; ayrıca bir değerin sonunda boşluktan sonra gelen `# ...` kuyruğu atılır) veya **üst düzey** `anahtar: değer`. Girintili satır, `-` ile başlayan satır, `:` içermeyen satır, yinelenen anahtar, tanınmayan anahtar → **hata** (`<ad>.yaml:<satır>: <mesaj>`); docs/13 §5.1'deki `teams`/`arena`/`consumables`/`mode` anahtarları bu planda **desteklenmez** ve hata verir (sonraki planlar ekler). Anahtar karakterleri `[a-z_]`. Değer sözdizimi: *skaler* (isteğe bağlı çift/tek tırnak) veya *akış listesi* `[a, b, c]` (öğeler virgülle ayrılır, her öğe isteğe bağlı tırnaklı, boş öğe/son virgül hata).

| Anahtar | Tür | Zorunlu | Kural |
|---|---|---|---|
| `scenario_id` | skaler | hayır (varsayılan `<ad>`) | `[A-Za-z0-9_.-]{1,32}`, `.` ile başlamaz (maç kimliği öneki; `Telemetry` kuralıyla aynı) |
| `zone` | tamsayı | hayır | yalnızca `71` kabul (botlar DB kaydındaki konumda doğar; başka bölge desteklenmez) |
| `bots` | liste | **evet** | 1..16 ad; her ad `BOT_TABLE`'da (büyük/küçük harf duyarsız); yinelenen ad (duyarsız) hata |
| `seeds` | liste | hayır (varsayılan `[0]`) | 1..32 öğe, her biri `0..4294967295` (yalnızca rakam, ≤ 10 hane) |
| `repeat` | tamsayı | hayır (varsayılan `1`) | `1..100` |
| `duration_sec` | tamsayı | hayır (varsayılan `30`) | `1..3600` |

Koşu listesi: `seeds` sırasıyla her seed `repeat` kez (ör. `seeds: [7, 8]`, `repeat: 2` → 7, 7, 8, 8). Toplam koşu `seeds × repeat` ≤ 200 olmalı (aşarsa hata).

**Kapsam dışı (yapılmayacak)**

- Envanter doldurma (`consumables`), konum/ofset (`spawn_offset`), taraf değişimi, `teams`/`policy`/`arena`/`mode` anahtarları, `gear_set`, çok ulusu ayrı yöneten kurallar, sonuç raporu üretimi (Python aracı F3-06'da var; `ScenarioRunner` rapor yazmaz), `MATCH_START`'a senaryo alanları eklemek (`Telemetry` değişmez; senaryo dosyası bilgisi yalnızca `Bot_*.log`'da).
- Botların konumunu/HP'sini koşular arasında sıfırlamak: botlar her spawn'da DB kaydındaki durumla girer (hareketsiz oldukları için bu planda sorun değil; sıfırlama sonraki planların işi). `scenario run` bu yüzden botları **her koşu sonunda despawn eder**.
- `/bot start`/`/bot stop`, `+bot` GM komutları (F3-04), `BotCore`/birim test (F3-05), `enable`/`disable`.
- `CommandSpawn/Despawn/List/Match`, `ParseSpawnList`, `TickSessions`, `PollDespawn`, `StartSession`, `FailSession`, `BeginDespawn`, `ProcessCommands`, `PollCommandFile`, `Telemetry.*`, `BotSession.*`, `ChatHandler.cpp`, `GameServerDlg.*`, `shared/**`, SQL, `docs/**` gövdelerini/dosyalarını değiştirmek ("iyileştirme" olarak ekleme).
- Aynı anda birden çok senaryo (en çok **bir**), senaryoyu kalıcı kuyruğa alma, sunucu açılışında otomatik senaryo (`SPAWN_ON_START` benzeri ini anahtarı).
- Sunucuyu çalıştırmak, `GameServer.ini` düzenlemek, DB'ye bağlanmak (çalışma zamanı doğrulaması Claude'da, §7 sonu).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/ScenarioRunner.h` | yeni | ASCII, CRLF |
| `GameServer/Bot/ScenarioRunner.cpp` | yeni | ASCII, CRLF |
| `GameServer/Bot/BotManager.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotManager.cpp` | değiştir | ASCII, CRLF |
| `GameServer/proj-GameServer.vcxproj` | değiştir | yalnızca iki satır ekleme (`ClCompile`, `ClInclude`); mevcut kodlama/satır sonu korunur |
| `GameServer/proj-GameServer.vcxproj.filters` | değiştir | yalnızca iki öğe ekleme (`Source Files` / `Header Files` filtresi) |
| `bots/config/scenario_smoke_2bot.yaml` | yeni | ASCII, LF; yeni `bots/config/` klasörü |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. **Dal:** `git switch -c bot/F3-03 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `stop` (exe kilidi).

2. **`GameServer/Bot/ScenarioRunner.h`** (yeni):
   ```cpp
   #pragma once

   #include <chrono>
   #include <string>
   #include <vector>

   class BotManager;
   class BotSession;

   // Runs a scenario file: spawn the listed bots, open a telemetry match per (seed, repeat),
   // close it after duration_sec, despawn the bots, repeat. IOCP thread only (ADR-0005):
   // Command() is called from BotManager::ExecuteCommand(), Tick() from BotManager::Tick().
   class ScenarioRunner
   {
   public:
   	explicit ScenarioRunner(BotManager & mgr);   // trivial: no allocation, no I/O

   	void Command(const std::string & args);      // "run <name>" | "stop" | "status"
   	void Tick(std::chrono::steady_clock::time_point now);   // returns at once while idle

   private:
   	enum State { STATE_IDLE, STATE_PREPARE, STATE_RUNNING, STATE_CLEANUP };

   	struct Scenario
   	{
   		std::string name;                 // file stem
   		std::string id;                   // match id prefix
   		std::vector<std::string> bots;
   		std::vector<uint32> seeds;
   		uint32 repeat;
   		uint32 durationSec;
   	};

   	static bool LoadScenario(const std::string & name, Scenario & out, std::string & error);
   	void CommandRun(const std::string & name);
   	void CommandStop();
   	void CommandStatus();
   	void StartRun(std::chrono::steady_clock::time_point now);   // enters STATE_PREPARE
   	void Abort(const std::string & reason, std::chrono::steady_clock::time_point now);
   	void BeginCleanup(std::chrono::steady_clock::time_point now);
   	void Finish(const std::string & abortNote);   // logs the summary, back to STATE_IDLE

   	BotManager & m_mgr;
   	State m_state;
   	Scenario m_scenario;
   	std::vector<uint32> m_runSeeds;               // flattened run list (seed per run)
   	size_t m_runIndex;                            // 0-based index of the current run
   	uint32 m_completedRuns;
   	std::string m_abortReason;                    // empty = no abort pending
   	std::vector<BotSession *> m_sessions;         // the scenario's bots, resolved in StartRun()
   	std::chrono::steady_clock::time_point m_stateSince;   // when the current state began
   	std::chrono::steady_clock::time_point m_matchSince;   // when the match opened
   };
   ```
   (`uint32` `stdafx.h` üzerinden gelir; `BotManager.h` da başlıkta tür eklemeden kullanıyor.)

3. **`GameServer/Bot/BotManager.h`:**
   - `#include <vector>` satırından sonra `#include "ScenarioRunner.h"`.
   - `class BotManager {` gövdesinin `private:` bölümünün başına (`BotManager() ...` kurucusundan önce) `friend class ScenarioRunner;`.
   - Kurucunun başlatıcı listesini `m_namesLeft(0), m_scenario(*this) {}` ile bitir (şu anki son eleman `m_namesLeft(0)`, `:48`).
   - Komut bildirimlerinin (`:64` `FindSession`) altına: `static bool IsKnownBotName(const std::string & name);   // BOT_TABLE lookup, case-insensitive`.
   - Son veri üyesinin (`m_matchTickMaxUs`, `:107`) altına: `ScenarioRunner m_scenario;   // IOCP thread only`.

4. **`GameServer/Bot/BotManager.cpp`:**
   - Include: `#include "Telemetry.h"` satırından sonra `#include "ScenarioRunner.h"` (zaten başlıktan gelir; yine de açık yazma, `Telemetry.h` gibi).
   - `ExecuteCommand()`: `match` dalından (`:571-572`) sonra yeni dal: `else if (_stricmp(verb.c_str(), "scenario") == 0) m_scenario.Command(args);`. Bilinmeyen komut metnini `"(spawn, despawn, list, match, scenario)"` yap (**tek izinli metin değişikliği**).
   - `FindSession()`'dan (`:581-590`) sonra: `bool BotManager::IsKnownBotName(const std::string & name) { return FindBotEntry(name.c_str()) != nullptr; }`.
   - `Tick()`: `TickSessions();` satırından (`:383`) hemen sonra, telemetri `RecordTick` bloğundan önce: `m_scenario.Tick(std::chrono::steady_clock::now());`.

5. **`GameServer/Bot/ScenarioRunner.cpp`** (yeni). Başlık: `#include "stdafx.h"`, `ScenarioRunner.h`, `BotManager.h`, `BotSession.h`, `Telemetry.h`, `<cstdio>`, `<cstdlib>`, `<cstring>`, `<algorithm>`.
   - **Yerel statik yardımcılar** (`BotManager.cpp`'dekiler `static` olduğu için burada küçük kopyalar; `Telemetry.cpp`'nin `WriteTelemetryLog` kalıbı gibi): `WriteScenarioLog(const char *)` (`BotManager.cpp:38-52` ile aynı gövde: `./Logs/Bot_<gün>_<ay>_<yıl>.log`'a `fopen("a")`), `Trim`, `PhaseText(BotSession::Phase)` (`PhaseName` ile aynı metinler), `IsSafeId(const std::string &)` (`[A-Za-z0-9_.-]{1,32}`, `.` ile başlamaz), `IsSafeFileStem(const std::string &)` (`[A-Za-z0-9_-]{1,40}`), `ParseUint(const std::string &, unsigned long long max, unsigned long long & out)` (yalnızca rakam, 1..10 hane, `<= max`), `Unquote`, `ParseList(const std::string & value, std::vector<std::string> & items)` (köşeli parantez, virgül, boş öğe hatası). Tüm log satırları `ScenarioRunner: ` önekiyle başlar; `char message[320]` + `snprintf` (kullanıcı metni `%s` ile).
   - **`LoadScenario(name, out, error)`:** `IsSafeFileStem(name)` değilse `error = "bad scenario name"`. Yol `"./Scenarios/" + name + ".yaml"` (başka kaynaktan yol kurma). `fopen(..., "r")` başarısızsa `error = "cannot open ./Scenarios/<name>.yaml"`. Satırları `fgets(buffer, 256)` ile oku: 255 karakterden uzun satır, 64 satırı aşan dosya, 4096 baytı aşan toplam → hata (mesaj biçimi `<name>.yaml:<satır>: <ne>`). Her satırı `Trim`; boş veya `#` ile başlıyorsa atla; ilk karakter boşluk/tab ise (kırpmadan önceki ham satır) veya `-` ile başlıyorsa `unsupported YAML construct`; `:` yoksa `expected key: value`; anahtar `[a-z_]+` ve tanınan 6 anahtardan biri değilse `unknown key '<k>'`; yinelenen anahtar `duplicate key '<k>'`. Değerin sonundaki ` #...` kuyruğunu at (ilk `#`'in önünde boşluk/tab olmalı). Her anahtarın kuralları yukarıdaki tabloya göre; ihlalde ilgili anahtarı adlandıran kısa mesaj (`bots: unknown bot 'X'`, `bots: duplicate 'X'`, `seeds: bad seed 'X'` vb.). Dosya sonunda: `bots` yoksa `missing key 'bots'`; `scenario_id` yoksa `name`; `seeds` yoksa `{0}`; `repeat` yoksa 1; `duration_sec` yoksa 30; `seeds.size() * repeat > 200` ise `too many runs (max 200)`. `BotManager::IsKnownBotName` ile ad doğrulaması (bu yüzden `friend`). Dosya kapatılmadan hiçbir yoldan dönülmez.
   - **`CommandRun(name)`:** sırayla reddet (`ScenarioRunner: run <name>: refused (<neden>)`): `m_state != STATE_IDLE` → `already running (<id>)`; `!Telemetry::Instance().IsEnabled(TEL_SUMMARY)` → `telemetry is off`; `Telemetry::Instance().IsMatchActive()` → `a match is already active (use 'match end')`; `LoadScenario` hatası → hatanın kendisi; `bots.size() > m_mgr.m_poolSize` → `bot count <n> exceeds pool size <p>`. Sonra `m_mgr.m_sessions` üzerinde: **listedeki** bir bot `PHASE_DESPAWN_WAIT`/`PHASE_FAILED`/`PHASE_DESPAWN_STUCK` ise `bot <ad> is in phase <faz> (cannot start)`; **listede olmayan** bir oturum `PHASE_QUEUED`/`WAIT_SELECT`/`WAIT_LOADED`/`IN_GAME`/`DESPAWN_WAIT` ise `session <ad> is active but not in the scenario` (listede olmayan `FAILED`/`DESPAWNED`/`DESPAWN_STUCK` oturumlar yok sayılır). Hepsi geçerse `m_scenario`'yu yerleştir, `m_runSeeds`'i düzleştir (her seed `repeat` kez), `m_runIndex = 0`, `m_completedRuns = 0`, `m_abortReason.clear()`, logla: `ScenarioRunner: run <name>: loaded (id <id>, <n> bot(s), <r> run(s), <d> s each)` ve `StartRun(now)`.
   - **`StartRun(now)`:** `m_sessions.clear()`; `m_mgr.CommandSpawn(<adlar virgülle birleşik>)` (zaten `in game`/kuyruktaki botlar için "ignored" satırı basması normaldir); her ad için `m_mgr.FindSession(ad.c_str())`, `nullptr` ise `Abort("spawn command failed", now)` ve dön; bulunanları `m_sessions`'a ekle. `m_state = STATE_PREPARE; m_stateSince = now;` logla: `ScenarioRunner: run <k>/<N> seed <seed>: preparing (<n> bot(s))`.
   - **`Tick(now)`** — `STATE_IDLE` ise hemen dön. Durum başına:
     - `STATE_PREPARE`: herhangi bir oturum `PHASE_FAILED` → `Abort("spawn failed: <ad>", now)`. Hepsi `PHASE_IN_GAME` ise: `m_mgr.CommandMatch("start " + m_scenario.id + " " + std::to_string(seed))`; ardından `!Telemetry::Instance().IsMatchActive()` ise `Abort("match start refused", now)`, değilse `m_state = STATE_RUNNING; m_stateSince = m_matchSince = now;` ve `ScenarioRunner: run <k>/<N>: match open, running <d> s`. Aksi halde `now - m_stateSince > 60000 ms` → `Abort("prepare timeout", now)`.
     - `STATE_RUNNING`: (1) `!IsMatchActive()` → `Abort("match ended externally", now)`; (2) herhangi bir oturum `PHASE_IN_GAME` değilse `m_mgr.CommandMatch("end bot_lost")`, `Abort("bot lost: <ad>", now)`; (3) `now - m_matchSince >= durationSec * 1000` ise `m_mgr.CommandMatch("end completed")`, `m_completedRuns++`, `BeginCleanup(now)`.
     - `STATE_CLEANUP`: her oturum için `PHASE_IN_GAME` ise `m_mgr.BeginDespawn(s, now)` (log satırını `BeginDespawn` kendisi basar). Herhangi biri `PHASE_FAILED`/`PHASE_DESPAWN_STUCK` ise `Finish("cleanup failed: <ad>")` ve dön. Hepsi `PHASE_DESPAWNED` ise: `m_abortReason` doluysa `Finish(m_abortReason)`; değilse `m_runIndex + 1 < m_runSeeds.size()` ise `m_runIndex++; StartRun(now)`; değilse `Finish("")`. Aksi halde `now - m_stateSince > 60000 ms` → `Finish("cleanup timeout")`.
   - **`Abort(reason, now)`:** `m_abortReason` boşsa `reason`'ı ata ve logla `ScenarioRunner: aborting (<reason>)`; `IsMatchActive()` ise `m_mgr.CommandMatch("end aborted")` (zaten `bot_lost` ile kapatılmışsa tekrar çağrılmaz: önce `IsMatchActive` bak); `BeginCleanup(now)`.
   - **`BeginCleanup(now)`:** `m_state = STATE_CLEANUP; m_stateSince = now;` logla `ScenarioRunner: run <k>/<N>: cleanup (despawning)`.
   - **`Finish(note)`:** `note` boşsa `ScenarioRunner: scenario <id> finished: <completed>/<N> run(s) completed`; doluysa `ScenarioRunner: scenario <id> aborted (<note>): <completed>/<N> run(s) completed`. `m_state = STATE_IDLE`, `m_sessions.clear()`, `m_abortReason.clear()`.
   - **`CommandStop()`:** `STATE_IDLE` ise `ScenarioRunner: stop: no scenario running`; `STATE_CLEANUP` ise `ScenarioRunner: stop: already cleaning up` (ve `m_abortReason` boşsa `"stopped by command"` ata ki sıradaki koşu başlamasın); aksi halde `Abort("stopped by command", now)`.
   - **`CommandStatus()`:** boştaysa `ScenarioRunner: status: idle`; değilse `ScenarioRunner: status: <id> run <k>/<N> seed <seed> state <prepare|running|cleanup>, <geçen ms> ms in state, <completed> completed`.
   - **`Command(args)`:** sözcüklere ayır (boşluk/tab); `run <ad>` (tam 2 sözcük), `stop` (1), `status` (1) — `_stricmp`; başka her şey (fazla/eksik argüman dahil) → `ScenarioRunner: cmd scenario: usage: scenario run <name> | scenario stop | scenario status`.
   - Mantık yalnızca IOCP thread'indedir: `std::thread`, mutex, `condition_variable`, ek zamanlayıcı **yok**.

6. **Proje dosyaları:** `proj-GameServer.vcxproj`: `Bot\Telemetry.cpp` `ClCompile` satırından (`:195`) sonra `<ClCompile Include="Bot\ScenarioRunner.cpp" />`; `Bot\Telemetry.h` `ClInclude` satırından (`:289`) sonra `<ClInclude Include="Bot\ScenarioRunner.h" />`. `.filters`: aynı kalıpla (`Bot\Telemetry.cpp` bloğundan sonra `Source Files` filtreli `ClCompile`; `Bot\Telemetry.h` bloğundan sonra `Header Files` filtreli `ClInclude`). Mevcut satır sonu/girinti biçimini koru.

7. **Örnek senaryo** `bots/config/scenario_smoke_2bot.yaml` (ASCII, LF):
   ```yaml
   # Smoke scenario: two idle bots, two seeds, 12 s per match.
   # Copy to <server dir>/Scenarios/smoke.yaml, then: /bot scenario run smoke
   scenario_id: smoke
   zone: 71
   bots: [BotWP_K, BotWP_E]
   seeds: [7, 8]
   repeat: 1
   duration_sec: 12
   ```

8. **Derle:** `./tools/build.sh Release` ve `./tools/build.sh Debug`. `ScenarioRunner.cpp` ve `BotManager.cpp`'yi `touch`'layıp yeniden derlenmeye zorla; `Bot\` dosyalarında **uyarı olmamalı** (`%u`/`%llu` için açık cast, `size_t`→`uint32` için açık cast). Sunucuyu çalıştırma.

9. **Raporu yaz** (şablon bölümü) ve commit et: `[F3-03] ...`. `git add` ile yalnızca §4'teki dosyaları ve bu planı ekle.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; **yeni uyarı yok** (`Bot\` dosyalarında uyarı olmamalı; çıktıyı yapıştır).
- [ ] K2: `./tools/build.sh Debug` hatasız biter.
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F3-03` yalnızca §4'teki 7 dosyayı (+ bu plan dosyasını) içerir; `.vcxproj`/`.filters` farkı yalnızca 2'şer satır ekleme (`git diff ... -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters` çıktısını yapıştır). `BotManager.cpp` farkında `-` satırı yalnızca `ExecuteCommand`'daki bilinmeyen komut metni (`git diff ... -- GameServer/Bot/BotManager.cpp | grep '^-'` çıktısını yapıştır); `CommandSpawn`, `CommandDespawn`, `CommandList`, `CommandMatch`, `ParseSpawnList`, `TickSessions`, `PollDespawn`, `StartSession`, `FailSession`, `BeginDespawn`, `ProcessCommands`, `PollCommandFile` gövdelerinde değişiklik yok.
- [ ] K4: Kapalıyken etkisiz: `ScenarioRunner` kurucusu yalnızca üyeleri başlatır (ayırma/G/Ç yok); `Tick()` ilk satırda `STATE_IDLE` ise döner; `scenario` dışında hiçbir yeni kod yolu çalışmaz; `GameServer.ini`'ye yeni anahtar okuması/yazması yok (`grep -n "ini\|CIni" GameServer/Bot/ScenarioRunner.cpp` boş) (`sed -n` çıktısı).
- [ ] K5: Thread kuralı: `ScenarioRunner.cpp`/`.h` içinde `std::thread`, `mutex`, `lock_guard`, `condition_variable`, `CreateThread`, `Sleep` yok (`grep -n` boş çıktısını yapıştır); `CUser`'a hiç dokunmuyor (`grep -n "CUser\|m_pUser" GameServer/Bot/ScenarioRunner.*` boş); oturum durumunu yalnızca `BotSession::m_phase` okuma ve `m_mgr.BeginDespawn/CommandSpawn/CommandMatch/FindSession` çağrısıyla yönetiyor.
- [ ] K6: Dosya güvenliği: senaryo dosyası yolu yalnızca `IsSafeFileStem` geçmiş addan kuruluyor; `fopen`/`fgets` çağrıları `LoadScenario` ve `WriteScenarioLog` dışında yok; her `return` yolunda `fclose` var (`grep -n "fopen\|fclose\|IsSafeFileStem" GameServer/Bot/ScenarioRunner.cpp` çıktısını satır satır açıklayarak yapıştır).
- [ ] K7: Ayrıştırıcı sıkılığı (kod okuması, her madde için `dosya:satır`): girintili satır, `-` satırı, `:` içermeyen satır, yinelenen anahtar, tanınmayan anahtar (örn. `teams`), anahtar-değer tür hatası, boş liste öğesi/son virgül, `bots` yok/>16/yinelenen/bilinmeyen ad, `seeds` >32 veya `> 4294967295`, `repeat`/`duration_sec` aralık dışı, `zone != 71`, `seeds×repeat > 200`, 4096 bayt/64 satır/255 karakter sınırı → hepsi hata ile `false` döndürüyor ve `m_scenario`'ya yazılmıyor (`out` yalnızca tam başarıda atanıyor ya da hata durumunda kullanılmıyor).
- [ ] K8: Durum makinesi (kod okuması): `PREPARE → RUNNING → CLEANUP → (PREPARE | IDLE)` ve `PREPARE|RUNNING → CLEANUP` (Abort) dışında geçiş yok; `STATE_CLEANUP` tüm oturumlar `PHASE_DESPAWNED` olmadan `IDLE`'a yalnızca `FAILED`/`DESPAWN_STUCK`/60 sn zaman aşımında düşüyor; `RUNNING`'de bot kaybı `match end bot_lost`, dışarıdan bitirilmiş maç `Abort` + (aktif maç yoksa) ek `end` çağrısı yok; `Abort` içinde maç hâlâ aktifse `end aborted` çağrılıyor (`sed -n` çıktısı).
- [ ] K9: Maç kimliği ve komutlar: `match` çağrıları yalnızca `"start <id> <seed>"`, `"end completed"`, `"end bot_lost"`, `"end aborted"` dizeleriyle; `<id>` `IsSafeId` geçmiş `m_scenario.id`; seed `std::to_string(uint32)` (`grep -n 'CommandMatch' GameServer/Bot/ScenarioRunner.cpp` çıktısı).
- [ ] K10: Komut yüzeyi: `Command()` yalnızca `run <ad>`, `stop`, `status` kabul eder; diğer her şey tek `usage` satırı; `ExecuteCommand`'daki `RESPAWN_CYCLES != 0` reddi `scenario`'yu da kapsar (değişmeyen kod, `BotManager.cpp:555-559`) (`sed -n` çıktısı).
- [ ] K11: Kodlama/stil: `file GameServer/Bot/*` hepsi "ASCII text, with CRLF line terminators"; `bots/config/scenario_smoke_2bot.yaml` "ASCII text" (LF); girinti tab, Allman, yorumlar İngilizce; yeni kodda `printf` yok (`grep -n "printf" GameServer/Bot/ScenarioRunner.cpp` yalnızca `snprintf`/`fprintf`; `fprintf` yalnızca `WriteScenarioLog`).
- [ ] K12: Örnek dosya §5.7'deki metinle aynı ve §3 tablosundaki kurallara uyuyor (tüm anahtarlar tabloda, değerler aralıkta, girintili satır yok). `git status --short` boş (yalnızca izinli dosyalar commit'li). Sunucu çalıştırılmadı, `GameServer.ini` ve veritabanı değiştirilmedi.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
git diff --stat gece/2026-10-02...bot/F3-03
git diff gece/2026-10-02...bot/F3-03 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F3-03 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters
grep -n "thread\|mutex\|lock_guard\|CreateThread\|Sleep\|CUser\|m_pUser\|CIni" GameServer/Bot/ScenarioRunner.h GameServer/Bot/ScenarioRunner.cpp
grep -n "fopen\|fclose\|fgets\|IsSafeFileStem\|CommandMatch" GameServer/Bot/ScenarioRunner.cpp
grep -n "printf" GameServer/Bot/ScenarioRunner.cpp
file GameServer/Bot/* bots/config/*
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile (`ENABLED=1, MAX_BOTS=16, TELEMETRY=summary`, `SPAWN_ON_START` boş; her senaryodan sonra ini yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile, KI-010); `bots/config/scenario_smoke_2bot.yaml` sunucu dizinine `Scenarios/smoke.yaml` olarak kopyalanır:
1. **Mutlu yol:** `BotCommands.txt` ile `scenario run smoke` → loglarda sırayla `loaded (id smoke, 2 bot(s), 2 run(s), 12 s each)`, `run 1/2 seed 7: preparing`, iki `in game`, `cmd match start: smoke-7-1 started (2 bot(s) in game)`, ~12 sn sonra `cmd match end: smoke-7-1 ended (result completed, ~12000 ms)`, iki `despawned`, `run 2/2 seed 8`, `smoke-8-2`, `finished: 2/2 run(s) completed`, `list`: `pool free 16/16`. `Logs/bots/<tarih>/smoke-7-1.jsonl` ve `smoke-8-2.jsonl`: ilk satır `MATCH_START` (`composition` 2 ad, `in_game=2`), son satır `MATCH_END` (`result":"completed`); `*.summary.json` geçerli. `python3 tools/bot-telemetry-report.py` iki maçı `OK` gösterir (`SPAWN_SHORT` yok).
2. **`repeat`/seed sırası:** `seeds: [7]`, `repeat: 2` → `smoke-7-N`, `smoke-7-N+1` (N süreç ömrü sayacı).
3. **Reddedilenler** (her biri tek `refused (...)`/hata satırı, dosya/maç oluşmaz): `scenario run ../x`, `scenario run yok`, bilinmeyen anahtar (`teams: x`), girintili satır, bilinmeyen bot adı, yinelenen bot, `repeat: 0`, `seeds: [4294967296]`, `zone: 31`, `duration_sec: 0`, `MAX_BOTS=1` ile 2 botluk senaryo, `TELEMETRY=off`, çalışırken ikinci `scenario run`, açık maç varken `scenario run`, listede olmayan bir bot `in game` iken `scenario run`.
4. **`scenario stop`** koşunun ortasında → maç `result":"aborted"`, botlar despawn, `scenario ... aborted (stopped by command): 0/2`; sonraki koşu başlamaz. **`scenario status`** her durumda tek satır.
5. **Bot kaybı:** koşu sırasında `despawn BotWP_K` → `MATCH_END` `bot_lost`, kalan bot despawn, `aborted (bot lost: BotWP_K)`. **Dışarıdan `match end`** → `aborted (match ended externally)`.
6. **Gerilemesiz:** `ENABLED=0` → `Scenarios/` okunmaz, log/dosya yok, `BotCommands.txt` dokunulmadan; F2-06 (`spawn/despawn/list`) ve F3-02 (`match start|end`) aynen; `RESPAWN_CYCLES=2` iken `scenario run` → `cmd rejected (RESPAWN_CYCLES is active)`; `PERF_SAMPLE` akışı ve `tick_p95_us` F3-01 düzeyinde.
7. Sunucu 3/3 UP, `GameServer.log`'a yeni hata yok.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` dosyaları ASCII; kod yorumları İngilizce.
- **Bot sistemi varsayılan kapalı.** Bu plan yeni davranışı yalnızca `ENABLED=1` **ve** `scenario run` komutu verildiğinde etkinleştirir.
- **Thread kuralı (ADR-0005):** `ScenarioRunner` yalnızca IOCP thread'inde (`ExecuteCommand`/`Tick` zinciri) çalışır; `m_sessions`/`BotSession` alanlarını yalnızca orada okur. Başka thread'den çağrı yok.
- **Oyun thread'ini bloklama:** `Tick()` içinde yalnızca küçük işler (≤ 16 oturum taraması); senaryo dosyası **yalnızca `scenario run` komutunda** bir kez okunur (≤ 4 KB), `Tick()`'te hiç dosya G/Ç yok.
- **Mevcut kodu çağır, kopyalama:** spawn/despawn/maç mantığı `CommandSpawn`/`BeginDespawn`/`CommandMatch`'tedir; `ScenarioRunner` onları çağırır. Spawn/despawn/maç mantığını yeniden yazma. `BeginDespawn`'i yalnızca `PHASE_IN_GAME` oturum için çağır (aksi halde çift `OnDisconnect`).
- Koşular arasında botlar despawn edilir ve yeniden spawn edilir: her koşu ~6-10 sn spawn + ~0,2 sn kayıt maliyeti taşır; bu bilinen ve kabul edilen (konum/HP sıfırlama yok, §3).
- **Kişisel veri:** senaryo dosyası ve loglar yalnızca sabit bot adlarını içerir; gerçek oyuncu adı/hesabı yazılmaz.
- Log hacmi: koşu başına ≈ 8-12 satır `Bot_*.log`.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F3-03` — `<kısa-sha> [F3-03] …`
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
- İncelenen: `gece/2026-10-02...bot/F3-03` @ `<sha>`
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
