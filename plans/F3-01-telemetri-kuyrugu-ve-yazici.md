# F3-01: Telemetri kuyruğu, yazıcı thread ve `PERF_SAMPLE` (`[BOT] TELEMETRY`, varsayılan `summary`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F3 — Telemetri ve test altyapısı (`docs/17` §2) |
| Branch | `bot/F3-01` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F2-02 (`KAPANDI`: `BotManager::Tick()`), F2-06 (`KAPANDI`: `BotManager` son hâli) |
| İlgili gereksinim / kabul | REQ-MET-02, `docs/17` F3 Görev 1 ("Olay şeması ve yazıcı") ve Görev 2'nin `PERF_SAMPLE` kısmı; ADR-0007; MET-PERF-02 (ölçüm altyapısı) |
| Tahmini büyüklük | M (5 dosya, ~330 satır ekleme) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Bot sistemi açıkken (`[BOT] ENABLED=1`) çalışan, oyun thread'ini bloklamayan bir telemetri altyapısı kurmak: sınırlı bir olay kuyruğu, kuyruğu JSONL dosyasına yazan ayrı bir yazıcı thread ve ilk üretici olarak 5 saniyede bir `PERF_SAMPLE` olayı (`BotManager::Tick()` süre dağılımı, oturum/havuz sayıları, kuyruk ve düşürme sayaçları). Sonraki F3 planları (MATCH_START/END, senaryo koşucusu, analiz aracı) ve F4+ (DECISION, ACTION_*) bu altyapıyı kullanır.

Bu plan sonunda `ENABLED=0` (varsayılan) iken sunucu davranışı ve dosya sistemi **aynıdır** (`Logs/bots/` oluşmaz). `ENABLED=1` iken `[BOT] TELEMETRY=off` ile de davranış F2-06'dakiyle aynıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0007-telemetri-formati-ve-depolama.md`: biçim, kuyruk, taşma politikası, dosya yolu kararları (**önce oku**; bu plan onu uygular).
- `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md` §3.1 (ortak alanlar), §3.2 (`PERF_SAMPLE` satırı), §3.3 (uygulama notları: alan kuralı, seviye eşlemesi, `PERF_SAMPLE` alan listesi, dosya adı), §6.8 (MET-PERF-02), §9 (depolama ve kuyruk).
- `docs/adr/ADR-0005-bot-tick-thread-modeli.md`: `Tick()` yalnızca IOCP thread'inde çalışır. Telemetri üreticileri (`Emit`) **herhangi bir thread'den** çağrılabilir olmalıdır; yazıcı thread kendi thread'idir.
- İlgili kod (dosya:satır, 2026-10-02'de depoda doğrulandı):
  - `GameServer/Bot/BotManager.cpp:1-9` include listesi (`stdafx.h`, `BotManager.h`, `IBotSink.h`, `BotSession.h`, `../../shared/Ini.h`, `<cstdio>`, `<cstring>`, `<ctime>`).
  - `BotManager.cpp:35-48` `static void WriteBotLog(const char * line)`: `./Logs/Bot_<gün>_<ay>_<yıl>.log` dosyasına bir satır ekler (kopyalanacak kalıp).
  - `BotManager.cpp:114-120` `Startup()`: `CIni ini(CONF_GAME_SERVER);` ve `ENABLED` okuma; `:262-266` `if (ok) ParseSpawnList(spawnList); return ok;` (telemetri başlatma noktası, `ParseSpawnList` çağrısı `:264`).
  - `BotManager.cpp:285-292` `StartTicking()` (`new Thread(TimerThreadProc, this)` kalıbı); `:294-303` `Shutdown()` (`m_shuttingDown = true`, zamanlayıcı thread'ini `waitForExit` + `delete`).
  - `BotManager.cpp:337-374` `Tick()`: `:339` `if (m_shuttingDown) return;`, `:342` `m_tickCount++`, `:372-373` `ProcessCommands(); TickSessions();` ve fonksiyon sonu.
  - `BotManager.cpp:623-631` `CommandList()` içindeki `poolFree` hesabı (`std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock()); ... GetReservedSessionMap().size()`; **aynı deseni** kullan).
  - `GameServer/Bot/BotManager.h:13-97`: sınıf bildirimi (`void Tick();` `:52`; `m_skippedTicks` `std::atomic<uint32>` `:89`; son veri üyesi `m_lastCommandPoll` `:96`).
  - `GameServer/GameServerDlg.cpp:105` `BotManager::Instance().Startup()`; `:214` `StartTicking()`; `:3132` `BotManager::Instance().Shutdown()` (`~CGameServerDlg`'in ilk satırı; bu çağrılar **değişmez**).
  - `GameServer/GameServerDlg.cpp:170` `CreateDirectory("Logs",NULL);` (klasör oluşturma kalıbı; Win32 `CreateDirectory`, hata "zaten var" dahil yok sayılır).
  - `shared/stdafx.h:52-55` `<thread>`, `<chrono>`, `<atomic>`, `<mutex>` zaten dahil; `:68` `#define sleep(ms) Sleep(ms)`; `shared/Thread.h:5-21` `Thread(lpfnThreadFunc, void *)` oluşturulur oluşturulmaz başlar, `waitForExit()` join eder; `shared/types.h:5,9` `int64`, `uint64`.
  - `GameServer/proj-GameServer.vcxproj:193-194` `Bot\BotManager.cpp` / `Bot\BotSession.cpp` `ClCompile`, `:285-287` `Bot\BotManager.h` / `Bot\IBotSink.h` / `Bot\BotSession.h` `ClInclude`; `.filters`: `:84-89` iki `ClCompile` (`<Filter>Source Files</Filter>`), `:203-211` üç `ClInclude` (`<Filter>Header Files</Filter>`).
- Uygulayıcı önce şunu kontrol etsin: yukarıdaki satır numaraları kayabilir; fonksiyon adlarıyla yeniden bul ve kaymayı raporda yaz.

## 3. Kapsam

**Yapılacaklar**

- Yeni `GameServer/Bot/Telemetry.h/.cpp`: `Telemetry` tekili (sınırlı kuyruk, yazıcı thread, seviye, JSONL dosyası, öz-sınama).
- `BotManager`: `Startup()` sonunda `Telemetry::Start()`, `Shutdown()` içinde `Telemetry::Stop()`, `Tick()` süresinin ölçülmesi ve 5 saniyede bir `PERF_SAMPLE` olayı.
- İki yeni ini anahtarı: `[BOT] TELEMETRY` (`off|summary|decisions|trace`, varsayılan `summary`) ve `[BOT] TELEMETRY_SELFTEST` (varsayılan `0`).

**Kapsam dışı (yapılmayacak)**

- `PERF_SAMPLE` dışındaki herhangi bir olay üretmek (`MATCH_START/END`, `DECISION`, `STATE_CHANGE`, spawn/despawn olayı vb.): sonraki planlar. `BotManager`'ın mevcut `WriteBotLog` mantığı, log metinleri ve davranışı **değişmez**; oturum faz geçişlerine olay eklenmez.
- Bot başına tick süresi (MET-PERF-01), `summary.json`, `<match>.jsonl` adlandırması, dosya döndürme/rotasyon, `ScenarioRunner`, GM/konsol komutu (`+bot verbose`, `/bot telemetry`), `docs/16`'daki diğer olay tipleri.
- Kilitsiz kuyruk, JSON kütüphanesi, `condition_variable` ile uyandırma, sıkıştırma, ağ üzerinden gönderim ("iyileştirme" olarak ekleme).
- `WriteBotLog`'u ortak yardımcıya çıkarmak (iki küçük kopya kabul edilir), `shared/**`, `User.*`, `SocketMgr`/`KOSocketMgr`, `GameServerDlg.*`, `ChatHandler.*`, SQL.
- Sunucuyu çalıştırmak, `GameServer.ini` düzenlemek, DB'ye bağlanmak (çalışma zamanı doğrulaması Claude'da, §7 sonu).
- `docs/**` dosyalarını değiştirmek (Claude yapar).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/Telemetry.h` | yeni | ASCII, CRLF |
| `GameServer/Bot/Telemetry.cpp` | yeni | ASCII, CRLF |
| `GameServer/Bot/BotManager.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotManager.cpp` | değiştir | ASCII, CRLF |
| `GameServer/proj-GameServer.vcxproj` | değiştir | UTF-8 BOM, CRLF; yalnızca iki satır ekleme |
| `GameServer/proj-GameServer.vcxproj.filters` | değiştir | UTF-8 BOM, CRLF; yalnızca iki giriş ekleme |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. **Dal:** `git switch -c bot/F3-01 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `stop` (exe kilidi).

2. **`GameServer/Bot/Telemetry.h`** (yeni, `#pragma once`; `BotManager.h` gibi `stdafx.h` içermez, include eden `.cpp` önce `stdafx.h` alır):
   ```cpp
   #pragma once

   #include <atomic>
   #include <mutex>
   #include <string>
   #include <vector>

   class Thread;

   // Minimum level an event needs; Telemetry::m_level >= level lets it through (docs/16 section 3.3).
   enum TelemetryLevel
   {
   	TEL_OFF = 0,
   	TEL_SUMMARY = 1,
   	TEL_DECISIONS = 2,
   	TEL_TRACE = 3
   };

   // One queued event. 'ev' is a constant code (never freed); the writer thread builds the JSON line.
   struct TelemetryEvent
   {
   	int64 t;               // steady_clock milliseconds
   	const char * ev;
   	int bot;
   	std::string name;      // empty = field omitted
   	std::string fields;    // ready JSON fragment
   };

   struct TelemetryStats
   {
   	uint64 written;       // lines written to the file
   	uint64 droppedSoft;   // droppable events rejected above SOFT_LIMIT
   	uint64 droppedHard;   // any event rejected above HARD_LIMIT
   	uint32 queueLen;
   };

   // JSONL telemetry (ADR-0007). Emit() may be called from any thread; the writer thread builds
   // the JSON lines and does all disk I/O.
   class Telemetry
   {
   public:
   	static constexpr size_t SOFT_LIMIT = 6144;
   	static constexpr size_t HARD_LIMIT = 8192;

   	static Telemetry & Instance();

   	// Reads [BOT] TELEMETRY / TELEMETRY_SELFTEST from GameServer.ini. Level "off": returns true, does
   	// nothing. Otherwise creates Logs/bots/<YYYY-MM-DD>/live-<HHMMSS>.jsonl and starts the writer
   	// thread. Returns false (and logs one line) when the file cannot be opened; the caller treats
   	// that as "telemetry unavailable", never as a startup failure.
   	// Called once from BotManager::Startup() on the main thread, before the worker threads start.
   	bool Start();
   	// Stops accepting events, writes what is queued, joins the writer, closes the file, logs one
   	// summary line. Idempotent; safe when Start() never ran.
   	void Stop();

   	bool IsEnabled(TelemetryLevel needed) const { return m_running.load() && m_level >= needed; }

   	// Any thread. 'ev' must be a constant code from docs/16 section 3.2 (not escaped). 'name' may be
   	// nullptr (field omitted). 'fields' is a ready JSON fragment WITHOUT braces and without a leading
   	// comma, e.g. "\"tick_n\":50,\"queue_len\":0" (may be empty); the caller guarantees it is valid
   	// JSON. Returns false when the event was not queued (level too low, stopped, or dropped).
   	bool Emit(TelemetryLevel level, const char * ev, int bot, const char * name,
   		const std::string & fields, bool droppable);

   	TelemetryStats GetStats();

   private:
   	Telemetry();
   	static uint32 THREADCALL WriterThreadProc(void * lpParam);
   	void WriterLoop();
   	void WriteBatch(std::vector<TelemetryEvent> & batch);
   	bool RunSelfTest();

   	int m_level;                       // set once in Start(), before the writer thread exists
   	std::atomic<bool> m_running;       // Emit() accepts events
   	std::atomic<bool> m_stopping;      // writer: drain and exit
   	std::atomic<bool> m_paused;        // writer holds its batch (self-test only)
   	Thread * m_writerThread;
   	FILE * m_file;                     // writer thread only after Start()
   	std::string m_filePath;

   	std::mutex m_lock;                 // guards m_queue and the three counters below
   	std::vector<TelemetryEvent> m_queue;
   	uint64 m_written;
   	uint64 m_droppedSoft;
   	uint64 m_droppedHard;
   };
   ```
   `FILE` için `<cstdio>`'yu başlığa ekle. `SOFT_LIMIT`/`HARD_LIMIT` `constexpr` (C++17'de örtük `inline`; referansla geçirilebilir, yine de `std::min` vb. kullanma).

3. **`GameServer/Bot/Telemetry.cpp`** (yeni; ilk satırlar `#include "stdafx.h"`, `#include "Telemetry.h"`, `#include "../../shared/Ini.h"`, `<algorithm>`, `<cstdio>`, `<cstring>`, `<ctime>`). Sabitler (hepsi `static const`): `WRITE_PERIOD_MS = 100`, `SELFTEST_WAIT_MS = 5000`.
   - `static void WriteTelemetryLog(const char * line)`: `BotManager.cpp:35-48`'deki `WriteBotLog`'un birebir kopyası (aynı dosya `./Logs/Bot_<g>_<a>_<y>.log`); durum satırları `Telemetry: ...` önekiyle bu dosyaya gider (yeni kodda `printf` **yok**).
   - `static void JsonEscape(const std::string & in, std::string & out)`: `"` → `\"`, `\` → `\\`, `\n`, `\r`, `\t` → kaçışlar, diğer `< 0x20` → `\u00XX` (`snprintf("\\u%04x")`), `>= 0x80` bayt → `?` (karakter adları ASCII; geçersiz UTF-8 üretme).
   - `Telemetry::Instance()`: yerel `static` tekil (BotManager deseni). Kurucu: `m_level(TEL_OFF), m_running(false), m_stopping(false), m_paused(false), m_writerThread(nullptr), m_file(nullptr), m_written(0), m_droppedSoft(0), m_droppedHard(0)`.
   - **`Start()`:**
     1. `CIni ini(CONF_GAME_SERVER)`; `GetString("BOT", "TELEMETRY", "summary", level)`: `_stricmp` ile `off`/`summary`/`decisions`/`trace` → `m_level`; başka değer → `WriteTelemetryLog("Telemetry: unknown level '<değer>', telemetry off")` (değeri `%s` ile, tampon 160), `m_level = TEL_OFF`. `m_level == TEL_OFF` ise `true` döndür (başka hiçbir şey yapma: dosya/klasör/thread yok). `bool selfTest = ini.GetBool("BOT", "TELEMETRY_SELFTEST", false);`
     2. Klasörler: `CreateDirectory("Logs", NULL); CreateDirectory("Logs/bots", NULL);` ardından `localtime` ile `Logs/bots/%04d-%02d-%02d` ve `CreateDirectory` (dönüş değerleri yok sayılır). Dosya: `Logs/bots/<YYYY-MM-DD>/live-<HHMMSS>.jsonl`, `fopen(path, "ab")`. Açılamazsa `Telemetry: cannot open <path>` logla, `m_level = TEL_OFF`, `false` döndür.
     3. `m_queue.reserve(HARD_LIMIT)`; `m_stopping = false; m_running = true;` `m_writerThread = new Thread(WriterThreadProc, this);` Bir satır logla: `Telemetry: level <ad>, writing <path>` (`<ad>` = `summary`/`decisions`/`trace`).
     4. `selfTest` ise `RunSelfTest()` çağır (sonucu yalnızca loglar, `Start()` dönüşünü etkilemez). `true` döndür.
   - **`Emit(...)`:** `if (!m_running.load() || m_level < level) return false;` `now_ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()`. Olayı kilit dışında kur (`name` null değilse kopyala). `std::lock_guard<std::mutex> lock(m_lock);` sonra: `m_queue.size() >= HARD_LIMIT` → `m_droppedHard++`, `false`; `droppable && m_queue.size() >= SOFT_LIMIT` → `m_droppedSoft++`, `false`; aksi halde `push_back(std::move(event))`, `true`. (Kilit altında yalnızca sayaç ve `push_back`; dize biçimlemesi yok.)
   - **`GetStats()`:** kilit altında sayaçlar ve `m_queue.size()`.
   - **`WriterThreadProc`/`WriterLoop`:** 
     ```
     for (;;)
     {
     	bool stopping = m_stopping.load();
     	if (!m_paused.load() || stopping)
     	{
     		std::vector<TelemetryEvent> batch;
     		{ lock; batch.swap(m_queue); m_queue.reserve(HARD_LIMIT); }
     		if (!batch.empty()) WriteBatch(batch);
     	}
     	if (stopping) break;
     	sleep(WRITE_PERIOD_MS);
     }
     ```
     `m_queue.swap` sonrası `m_queue` kapasitesi kaybolur; `reserve(HARD_LIMIT)` tekrar çağrılır (kilit altında).
   - **`WriteBatch(batch)`:** tek bir `std::string buffer` oluştur; her olay için: `{"t":<t>,"match":"-","bot":<bot>` + (`name` boş değilse `,"name":"<JsonEscape>"`) + `,"ev":"<ev>","mode":"live"` + (`fields` boş değilse `,` + `fields`) + `}\n`. Sayı biçimleme için `snprintf` kullan (`%lld`). Sonra tek `fwrite(buffer.data(), 1, buffer.size(), m_file)` + `fflush(m_file)`; **yazılan satır sayısını** (`batch.size()`) kilit altında `m_written`'a ekle (yazma hatası olsa da sayma: `fwrite` dönüşü `buffer.size()`'dan küçükse `Telemetry: write error` bir kez logla, `m_written`'a **ekleme**).
   - **`Stop()`:** `if (m_writerThread == nullptr && m_file == nullptr) return;` `m_running = false; m_stopping = true;` yazıcıyı `waitForExit` + `delete` (yazıcı kalan kuyruğu bitirir, duraklatma yok sayılır); `fclose(m_file)`; `m_file = nullptr`; `m_writerThread = nullptr`; sayaçları al; `Telemetry: stopped, written %llu, dropped soft %llu, hard %llu` logla (`%llu` için `(unsigned long long)` dönüşümü).
   - **`RunSelfTest()`:** (yalnızca `Start()` içinden, ana thread; yazıcı çalışırken kuyruk boş olmalı) Taşma politikasını belirlenimli sınar:
     1. `TelemetryStats before = GetStats();` `m_paused = true;`
     2. `accepted = 0`; 7000 kez `Emit(TEL_SUMMARY, "SELFTEST", -1, nullptr, "\"i\":<n>", true)`; sonra 3000 kez `Emit(TEL_SUMMARY, "SELFTEST", -1, nullptr, "\"i\":<n>", false)`; true dönenleri `accepted`'a say. (`"\"i\":" + std::to_string(n)` ile alanı kur.)
     3. `m_paused = false;` en çok `SELFTEST_WAIT_MS` boyunca 20 ms aralıkla bekle: `GetStats().queueLen == 0 && GetStats().written >= before.written + accepted` olana kadar.
     4. `after = GetStats()`; beklenen: `accepted == HARD_LIMIT` (8192 = 6144 + 2048), `after.droppedSoft - before.droppedSoft == 7000 - SOFT_LIMIT` (856), `after.droppedHard - before.droppedHard == 3000 - (HARD_LIMIT - SOFT_LIMIT)` (952), `after.written - before.written == accepted`, `queueLen == 0`. Hepsi doğruysa `Telemetry: self-test OK (accepted %u, soft drops %u, hard drops %u, written %u in %lld ms)`, değilse `Telemetry: self-test FAILED (accepted %u, soft %u, hard %u, written %u, queue %u)` logla. Dönüş: bool.
     Not: `before.written`/drops sıfır değildir diye varsayma; fark al. Kuyruğun öz-sınama başında boş olduğu varsayımı `before.queueLen == 0` ile denetlenir; değilse `self-test FAILED (queue not empty at start)` logla ve dön.

4. **`GameServer/Bot/BotManager.h`:**
   - `private:` bölümüne (`void Tick();` bildiriminin `:52` altına):
     ```cpp
     	void RecordTick(std::chrono::steady_clock::time_point tickStart); // IOCP thread only, called from Tick()
     	void EmitPerfSample(std::chrono::steady_clock::time_point now);   // IOCP thread only
     ```
   - Veri üyeleri (`m_lastCommandPoll`'un altına; kurucu başlatıcı listesine **dokunma**, varsayılan kurucular yeter):
     ```cpp
     	std::vector<uint32> m_tickUs;                             // IOCP thread only: Tick() durations (us) of the current 5 s window
     	std::chrono::steady_clock::time_point m_perfWindowStart;  // IOCP thread only
     	bool m_perfWindowOpen = false;                            // IOCP thread only
     ```
     (`m_perfWindowOpen = false;` sınıf içi başlatıcı kullanır; kurucu listesi değişmez.)

5. **`GameServer/Bot/BotManager.cpp`:**
   - Include'lara `#include "Telemetry.h"` (`BotSession.h`'nin altına) ve `#include <algorithm>` (`<cstdio>`'nun üstüne) ekle.
   - `Startup()`: `if (ok) ParseSpawnList(spawnList);` bloğunun (`:263-264`) **hemen sonrasına**, `return ok;`'ten önce:
     ```cpp
     	if (ok)
     		Telemetry::Instance().Start();
     ```
     (Dönüş değeri yok sayılır: telemetri başlayamazsa bot sistemi yine açılır.)
   - `Shutdown()`: zamanlayıcı thread'inin `waitForExit/delete` bloğundan **sonra**, fonksiyon sonunda `Telemetry::Instance().Stop();`.
   - `Tick()`: `if (m_shuttingDown) return;` satırından sonra `std::chrono::steady_clock::time_point tickStart = std::chrono::steady_clock::now();`; fonksiyonun sonunda (`TickSessions();` satırından sonra):
     ```cpp
     	if (Telemetry::Instance().IsEnabled(TEL_SUMMARY))
     		RecordTick(tickStart);
     ```
     `m_tickCount` mantığı, loglar ve `ProcessCommands(); TickSessions();` sırası **değişmez**.
   - `RecordTick(tickStart)`: `end = steady_clock::now()`; `us = duration_cast<microseconds>(end - tickStart).count()`; `!m_perfWindowOpen` ise `m_perfWindowOpen = true; m_perfWindowStart = tickStart; m_tickUs.reserve(4096);`. `m_tickUs.size() < 4096` ise `push_back((uint32)us)` (üst sınır: 20 ms tick'te 5 sn = 250 örnek, 4096 yalnızca taşma koruması). `end - m_perfWindowStart >= 5000 ms` ise `EmitPerfSample(end)`.
   - `EmitPerfSample(now)`: `n = m_tickUs.size()`; `n == 0` ise pencereyi sıfırla ve dön. Sıralanmış kopya: `std::vector<uint32> sorted(m_tickUs); std::sort(...)`. Yüzdelik (en yakın sıra): `idx(p) = (size_t)ceil(p * n) - 1` (`p` = 0.50, 0.95, 0.99; `cmath`/`<math.h>` `stdafx.h` ile gelir), sınırla `n - 1`'e. `maxUs = sorted.back()`. Sayımlar: `inGame` = `m_sessions` içinde `m_phase == BotSession::PHASE_IN_GAME`; `poolFree` = `CommandList()`'teki gibi kilit altında `GetReservedSessionMap().size()`; `skipped = m_skippedTicks.load()`; `TelemetryStats st = Telemetry::Instance().GetStats();` `windowMs = duration_cast<milliseconds>(now - m_perfWindowStart).count()`. Alan parçası (tek `char fields[512]`, `snprintf`):
     ```
     "window_ms":%lld,"tick_n":%u,"tick_p50_us":%u,"tick_p95_us":%u,"tick_p99_us":%u,"tick_max_us":%u,"sessions":%u,"in_game":%u,"pool_free":%u,"skipped_ticks":%u,"queue_len":%u,"written":%llu,"dropped_soft":%llu,"dropped_hard":%llu
     ```
     `Telemetry::Instance().Emit(TEL_SUMMARY, "PERF_SAMPLE", -1, nullptr, fields, true);` Sonra `m_tickUs.clear(); m_perfWindowStart = now;`.
   - Mevcut `WriteBotLog` ve log metinleri değişmez; yeni `BotManager` kodu `printf` kullanmaz.

6. **vcxproj/filters:**
   - `GameServer/proj-GameServer.vcxproj`: `:194` `<ClCompile Include="Bot\BotSession.cpp" />`'nin altına `<ClCompile Include="Bot\Telemetry.cpp" />`; `:287` `<ClInclude Include="Bot\BotSession.h" />`'nin altına `<ClInclude Include="Bot\Telemetry.h" />` (komşu satırların girinti ve CRLF'i aynen).
   - `.filters`: `Bot\BotSession.cpp` girişinin (`:87-89`) altına aynı biçimde `Bot\Telemetry.cpp` (`<Filter>Source Files</Filter>`), `Bot\BotSession.h` girişinin (`:209-211`) altına `Bot\Telemetry.h` (`<Filter>Header Files</Filter>`).

7. **Derle:** `./tools/build.sh Release` ve `./tools/build.sh Debug`. `Telemetry.cpp` ve `BotManager.cpp`'yi (`touch`) yeniden derlenmeye zorla; `Bot\` dosyalarında **uyarı olmamalı** (özellikle `uint64`/`%llu` biçim ve `size_t`→`uint32` dönüşümleri için açık cast kullan). Sunucuyu çalıştırma.

8. **Raporu yaz** (şablon bölümü) ve commit et: `[F3-01] ...`. `git add` ile yalnızca §4'teki dosyaları ve bu planı ekle.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; **yeni uyarı yok** (`Bot\` dosyalarında uyarı olmamalı; çıktıyı yapıştır).
- [ ] K2: `./tools/build.sh Debug` hatasız biter.
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F3-01` yalnızca §4'teki 6 dosyayı (+ bu plan dosyası) içerir; `BotManager.cpp` farkı yalnızca ekleme (`−` satırı yok ya da yalnızca include/`Tick()` bölümü için gerekli) ve `GameServer/Bot/BotManager.cpp`'deki mevcut `ParseSpawnList`, `TickSessions`, `PollDespawn`, `StartSession`, `FailSession`, `BeginDespawn`, `Command*`, `ProcessCommands`, `PollCommandFile` gövdeleri **değişmemiştir** (`git diff ... -- GameServer/Bot/BotManager.cpp` çıktısının ilgili kısmını yapıştır).
- [ ] K4: Thread kuralı: `Telemetry::Emit` ve `GetStats` kilit altında çalışır; `m_file` yalnızca yazıcı thread'inde (ve `Start()`/`Stop()` içinde, yazıcı yokken/bittikten sonra) kullanılır; JSON satırı yalnızca `WriteBatch`'te (yazıcı thread) birleştirilir, `Emit` içinde dize birleştirme yok (`grep -n "m_file\|m_lock\|WriteBatch\|JsonEscape" GameServer/Bot/Telemetry.cpp` çıktısını satır satır açıklayarak yapıştır).
- [ ] K5: `m_lock` altında yalnızca sayaç güncellemesi, `push_back`, `swap`, `size()` ve `reserve` var; kilit altında `fwrite`/`snprintf`/`Emit` yok (`sed -n` çıktısı, satır numarasıyla).
- [ ] K6: Taşma politikası: `SOFT_LIMIT = 6144`, `HARD_LIMIT = 8192`; yumuşak sınırda yalnızca `droppable` olaylar, sert sınırda hepsi düşer ve ayrı sayılır (`sed -n` çıktısı).
- [ ] K7: Seviye ve kapalı davranış: `Start()` `TELEMETRY=off` (veya bilinmeyen değer) iken klasör/dosya/thread **oluşturmadan** döner; `Emit` `m_running` false iken hemen `false` döner; `Tick()` içindeki kanca `IsEnabled(TEL_SUMMARY)` kapısıdır; `ENABLED=0` iken `Telemetry::Start()` hiç çağrılmaz (çağrı `if (ok)` bloğunda ve `m_enabled` kapısının arkasındadır: `Startup()` `!m_enabled` ise en başta döner) (`sed -n` çıktısı).
- [ ] K8: `PERF_SAMPLE` alanları §5.5'teki biçim dizisiyle **birebir** (14 alan, sıra aynı); `ev` ve alan adları `docs/16` §3.3 ile uyumlu (`grep -n "PERF_SAMPLE\|window_ms" GameServer/Bot/*.cpp` çıktısı).
- [ ] K9: `WriteBotLog` ve mevcut log metinleri değişmemiş; yeni log satırlarının hepsi `Telemetry: ` önekli ve `Telemetry.cpp` içinde (`grep -n "Telemetry: " GameServer/Bot/*.cpp` çıktısı); yeni kodda `printf` yok (`grep -n "printf" GameServer/Bot/Telemetry.cpp` yalnızca `snprintf`/`fprintf` içermeli; `fprintf` yalnızca `WriteTelemetryLog` içinde).
- [ ] K10: Kodlama: `file GameServer/Bot/*` hepsi "ASCII text, with CRLF line terminators"; `file GameServer/proj-GameServer.vcxproj*` hâlâ "UTF-8 (with BOM) ... CRLF" (çıktıyı yapıştır); girinti tab, Allman, yorumlar İngilizce.
- [ ] K11: `git diff --stat` `.vcxproj` ve `.filters` için yalnızca ekleme (`+2/−0` ve `+8/−0` civarı) gösterir.
- [ ] K12: `git status --short` boş (yalnızca izinli dosyalar commit'li). Sunucu çalıştırılmadı, `GameServer.ini` ve veritabanı değiştirilmedi.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
git diff --stat gece/2026-10-02...bot/F3-01
grep -n "m_file\|m_lock\|WriteBatch\|JsonEscape" GameServer/Bot/Telemetry.cpp
grep -n "Telemetry: \|PERF_SAMPLE\|window_ms" GameServer/Bot/*.cpp
grep -n "printf" GameServer/Bot/Telemetry.cpp
file GameServer/Bot/* GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile (her senaryodan sonra ini yedekten geri yüklenir; `Logs/bots/` çıktıları kontrol edilir):
1. **Öz-sınama:** `ENABLED=1, MAX_BOTS=16, TELEMETRY=summary, TELEMETRY_SELFTEST=1` → `Bot_*.log`: `Telemetry: level summary, writing ...`, `Telemetry: self-test OK (accepted 8192, soft drops 856, hard drops 952, written 8192 in N ms)`; dosyada 8192 `SELFTEST` satırı.
2. **`PERF_SAMPLE`:** `SPAWN_ON_START=BotWP_K,BotMF_K,BotWP_E,BotPHD_E`, `TELEMETRY_SELFTEST=0`, ≥ 30 sn → dosyada ~6 `PERF_SAMPLE`; hepsi `python3 -c` ile `json.loads` geçerli; `in_game=4`, `sessions=4`, `pool_free=12`, `tick_n` ≈ 50 (`TICK_MS=100` → 5 sn), `tick_p95_us` makul (< 5000 = MET-PERF-02 bütçesi), `dropped_*=0`; sunucu kapatılınca `Telemetry: stopped, written N, dropped soft 0, hard 0` ve dosyadaki satır sayısı = `written`.
3. **`TELEMETRY=off`:** `ENABLED=1`, `TELEMETRY=off` → `Logs/bots/` altında yeni dosya yok; `Telemetry:` satırı yok; bot sistemi F2-06 gibi çalışır. **`TELEMETRY=bogus`:** `unknown level 'bogus', telemetry off`.
4. **`ENABLED=0`:** varsayılan → `Logs/bots/` oluşmaz, `Bot_*.log`'a yeni satır yok, ini'ye `TELEMETRY*` yazılmaz (kaydedilmez).
5. **Gerilemesiz (F2-05):** `SPAWN_ON_START` 4 bot, `DESPAWN_AFTER_SEC=2, RESPAWN_CYCLES=4` → `respawn cycles done: 20 spawns, 20 despawns, 0 failed, 0 stuck, 0 names left, pool free 16/16`; telemetri açıkken de aynı.
6. Sunucu 3/3 UP, `GameServer.log`'a yeni hata yok; sunucu kapatma temiz (yazıcı thread birleşiyor, `Telemetry: stopped` satırı var).

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` dosyaları ASCII; kod yorumları İngilizce. vcxproj/filters BOM'lu: BOM'u ve satır sonlarını koru.
- **Bot sistemi varsayılan kapalı.** Telemetri yalnızca `ENABLED=1` iken başlar; kapalıyken sunucu davranışı ve dosya sistemi değişmez.
- **Oyun thread'ini bloklama:** `Emit` içinde disk G/Ç, `fflush`, dize birleştirme ve uzun kilit yok; kilit altında yalnızca vektör işlemleri. Yazıcı thread'i `sleep(100)` ile yoklar (uyandırma mekanizması kapsam dışı).
- **Thread kuralı (ADR-0005):** `Tick()`'ten gelen çağrılar IOCP thread'indedir; `RecordTick`/`EmitPerfSample` yalnızca oradan çağrılır ve `m_sessions`/`BotSession` alanlarını yalnızca orada okur. `Telemetry` tekilinin diğer üyeleri thread-güvenlidir (yukarıdaki kilit/atomik kuralları).
- **Kapanış:** `Stop()` `BotManager::Shutdown()`'dan çağrılır (`~CGameServerDlg`); IOCP thread'i o sırada hâlâ `Tick()` çalıştırıyorsa `Emit` `m_running == false` ile güvenle `false` döner. `Stop()` çağrıldıktan sonra `Emit` çağrısı zararsızdır.
- **Çökme:** çökmede son ≤ 100 ms olay kaybolabilir (ADR-0007, kabul edilen).
- **Kişisel veri:** `PERF_SAMPLE` yalnızca sayı içerir; bu planda gerçek oyuncu verisi, karakter adı veya hesap kimliği telemetriye yazılmaz.
- Log hacmi: `PERF_SAMPLE` ~720 satır/saat (~250 bayt/satır).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
- Kabul kriterleri öz-değerlendirme:
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar:
- İncelenen:
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|

- Bulgular:
- Düzeltme talimatı:
