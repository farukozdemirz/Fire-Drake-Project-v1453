# F3-02: Maç bağlamı: `MATCH_START` / `MATCH_END`, `<match>.jsonl` ve `summary.json` (`/bot match start|end`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F3 — Telemetri ve test altyapısı (`docs/17` §2) |
| Branch | `bot/F3-02` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F3-01 (`KAPANDI`: `Telemetry` kuyruğu/yazıcı, `PERF_SAMPLE`), F2-06 (`KAPANDI`: komut çekirdeği, ADR-0015) |
| İlgili gereksinim / kabul | REQ-MET-02, `docs/17` F3 Görev 2 ("MATCH_START/END") ve Görev 1'in "olay şeması" kısmı; ADR-0007 (Ek, F3-02); ADR-0015 (Ek, F3-02) |
| Tahmini büyüklük | M (4 dosya, ~350 satır ekleme) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Telemetriye **maç bağlamı** eklemek: `/bot match start <senaryo> [seed]` ile bir maç açılır (`MATCH_START` olayı, olaylar artık `Logs/bots/<tarih>/<match>.jsonl` dosyasına ve `"match":"<id>"` alanıyla yazılır), `/bot match end [sonuç]` ile kapanır (`MATCH_END` olayı, dosya kapanır, `<match>.summary.json` yazılır). Maç yokken F3-01 davranışı aynen sürer (`live-<HHMMSS>.jsonl`, `"match":"-"`). `ScenarioRunner` (F3-03) bu planın API'sini çağıracak; bu planda senaryo YAML'ı, envanter doldurma ve otomatik başlat/bitir **yoktur**.

Bu plan sonunda `ENABLED=0` (varsayılan) iken sunucu davranışı ve dosya sistemi aynıdır; `ENABLED=1` iken **maç komutu verilmedikçe** davranış F3-01'dekiyle birebir aynıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0007-telemetri-formati-ve-depolama.md` (özellikle sonundaki "Ek (F3-02)" bölümü: maç dosyası/`summary` kararları) ve `docs/adr/ADR-0015-bot-calisma-zamani-komut-kanali.md` ("Ek (F3-02)": `match` komutu).
- `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md` §3.1 (ortak alanlar: `match`, `mode`, `ts_utc` yalnızca MATCH_START/END'de), §3.2 (`MATCH_START`/`MATCH_END` ek alanları), §3.3 (uygulama notları, F3-02 maddeleri), §9 (depolama: `<match>.jsonl`, `summary.json`).
- `docs/adr/ADR-0005-bot-tick-thread-modeli.md`: bot komutları yalnızca IOCP thread'inde (`Tick()` → `ProcessCommands()`) çalışır.
- `plans/F3-01-telemetri-kuyrugu-ve-yazici.md` (§5 ve Doğrulama Raporu): bu plan onun kodunu genişletir; kuyruk/kilit kuralları (K4, K5) aynen geçerlidir.
- İlgili kod (dosya:satır, 2026-10-02'de depoda doğrulandı, taban `d73c295`):
  - `GameServer/Bot/Telemetry.h:20-28` `TelemetryEvent` (`t`, `ev`, `bot`, `name`, `fields`); `:38-89` `Telemetry` sınıfı (`SOFT_LIMIT`/`HARD_LIMIT` `:43-44`, `Start()` `:53`, `Stop()` `:56`, `Emit()` `:64`, `GetStats()` `:67`, özel üyeler `:76-88`: `m_level`, `m_running`, `m_stopping`, `m_paused`, `m_writerThread`, `m_file`, `m_filePath`, `m_lock`, `m_queue`, `m_written`, `m_droppedSoft`, `m_droppedHard`).
  - `GameServer/Bot/Telemetry.cpp:14-28` `WriteTelemetryLog` (log kalıbı); `:32-65` `JsonEscape` (statik); `:80-148` `Start()` (klasör oluşturma `:107-115`, `fopen(..., "ab")` `:122`); `:150-177` `Stop()`; `:179-209` `Emit()` (sert/yumuşak sınır `:195-205`); `:211-220` `GetStats()`; `:229-252` `WriterLoop()`; `:254-306` `WriteBatch()` (satır biçimi `:263-287`: `{"t":..,"match":"-","bot":..[,"name":..],"ev":"..","mode":"live"[,fields]}`; tek `fwrite`+`fflush` `:290-291`; `m_written += batch.size()` `:304-305`).
  - `GameServer/Bot/BotManager.cpp:538-570` `ExecuteCommand()` (fiil ayrıştırma `:554-556`, `spawn`/`despawn`/`list` dalı `:558-569`, bilinmeyen komut metni `:566-567`: `"BotManager: cmd unknown command '%s' (spawn, despawn, list)"`); `:572-581` `FindSession()`; `:709-737` `CommandList()` (**değişmez**); `:407-460` `EmitPerfSample()` (`p95` `:427`, `maxUs` `:429`, `Emit` çağrısı `:456`, pencere sıfırlama `:458-459`); `:36-51` `WriteBotLog`; `:90-98` `Trim`; `:100-114` `SplitNames`; `:54-72` `BOT_TABLE`/`FindBotEntry`; `:74-88` `PhaseName`.
  - `GameServer/Bot/BotManager.h:53` `RecordTick` bildirimi, `:62` `void CommandList();`, `:63` `FindSession`; `:99-102` son veri üyeleri (`m_tickUs`, `m_perfWindowStart`, `m_perfWindowOpen`).
  - `GameServer/ChatHandler.cpp:1150-1176` `HandleBotCommand`: `/bot <ne olursa olsun>` satırını olduğu gibi `EnqueueCommand`'a verir; yani `/bot match start ...` **ChatHandler değişmeden** çalışır (`ChatHandler.cpp` bu planda değişmez; yardım metnindeki örnek satırlar eski kalır).
  - `GameServer/StdAfx.h:3-4` `../shared/stdafx.h` ve `<math.h>`; `CreateDirectory`/`GetFileAttributes`/`gmtime_s` Windows/CRT başlıklarıyla gelir (F3-01 `CreateDirectory` kullanıyor).
- Uygulayıcı önce şunu kontrol etsin: yukarıdaki satır numaraları kayabilir; fonksiyon adlarıyla yeniden bul ve kaymayı raporda yaz.

## 3. Kapsam

**Yapılacaklar**

- `Telemetry`: maç durumu (`BeginMatch`/`EndMatch`/`IsMatchActive`/`GetMatchId`), kuyrukta sıralı **denetim olayları** (`MATCH_START`, `MATCH_END`), yazıcı thread'inde maç dosyasına geçiş, `match` alanının doldurulması, `summary.json` yazımı, `Stop()` içinde açık maçı `aborted` ile kapatma.
- `BotManager`: `match start <senaryo> [seed]` ve `match end [sonuç]` komutları (`CommandMatch`), `MATCH_START`/`MATCH_END` ek alanları (bileşim, `in_game`, maç boyunca `PERF_SAMPLE` özeti).
- Yeni ini anahtarı **yok**. Yeni dosya **yok** (vcxproj/filters değişmez).

**Kapsam dışı (yapılmayacak)**

- `ScenarioRunner`, senaryo YAML'ı, envanter doldurma, taraf (`-K`/`-E`) ve `mode` seçimi (`mode` her zaman `"live"`), commit/veri hash alanları (`docs/16` §3.2'deki `MATCH_START` listesinin bu planda üretilmeyen kalemleri: `kompozisyonlar` dışındaki ekipman seti, commitler, veri hash'leri → F3-03/F3-06), `+bot` GM komutları, `PERF_SAMPLE` dışındaki olaylar, analiz/rapor aracı.
- `CommandList()`/`CommandSpawn()`/`CommandDespawn()` gövdelerini, `ParseSpawnList`, `TickSessions`, `PollDespawn`, `StartSession`, `FailSession`, `BeginDespawn`, `PollCommandFile`, `ProcessCommands` gövdelerini değiştirmek. `ChatHandler.cpp`, `GameServerDlg.*`, `shared/**`, SQL, `docs/**` (Claude yapar).
- Dosya döndürme/rotasyon, kilitsiz kuyruk, JSON kütüphanesi, `condition_variable`, sıkıştırma ("iyileştirme" olarak ekleme).
- `match status` / `/bot list` çıktısına maç satırı eklemek.
- Aynı anda birden çok maç (en çok **bir** açık maç).
- Sunucuyu çalıştırmak, `GameServer.ini` düzenlemek, DB'ye bağlanmak (çalışma zamanı doğrulaması Claude'da, §7 sonu).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/Telemetry.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/Telemetry.cpp` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotManager.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotManager.cpp` | değiştir | ASCII, CRLF |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. **Dal:** `git switch -c bot/F3-02 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `stop` (exe kilidi).

2. **`GameServer/Bot/Telemetry.h`:**
   - Başlığa `#include <map>` ekle (`<mutex>`'ten sonra).
   - Denetim olayı türü ve `TelemetryEvent` genişlemesi:
     ```cpp
     // Control events travel in the same queue as normal events, so the writer thread sees
     // match boundaries in emit order. They are never dropped (at most two per match).
     enum TelemetryControl
     {
     	TELCTL_NONE = 0,
     	TELCTL_MATCH_START = 1,
     	TELCTL_MATCH_END = 2
     };
     ```
     `TelemetryEvent`'e `bot`'tan sonra `int ctl = 0; // TelemetryControl` ekle (varsayılan başlatıcı; `int bot` ile hizalama dolgusuna oturur, struct boyutunu büyütmez). MATCH_START olayında `name` alanı **maç `.jsonl` dosyasının yolunu** taşır (JSON'a yazılmaz; yorumla belirt: `// MATCH_START only: path of the match .jsonl (not written as a name)`).
   - `Telemetry` sınıfına (public) ekle:
     ```cpp
     	// Any thread (serialized by an internal mutex; BotManager calls them on the IOCP thread).
     	// Opens a match: validates 'scenario' ([A-Za-z0-9_.-]{1,32}, not starting with '.'), picks the
     	// first free id "<scenario>-<seed>-<run>" (run = 1,2,... ; skips ids whose Logs/bots/<date>/<id>.jsonl
     	// or .summary.json already exists), creates the directory and queues MATCH_START.
     	// 'extraFields' is a ready JSON fragment (no braces, no leading comma, may be empty).
     	// Returns false and sets 'error' (short English text) when telemetry is off/stopped, a match
     	// is already active, or the arguments are invalid.
     	bool BeginMatch(const std::string & scenario, uint32 seed, const std::string & extraFields,
     		std::string & matchId, std::string & error);
     	// Queues MATCH_END for the active match ('result' same character rule as 'scenario',
     	// default handled by the caller). Returns false (error "no active match" / "telemetry is off")
     	// otherwise. 'durationMs' = steady_clock milliseconds since BeginMatch.
     	bool EndMatch(const std::string & result, const std::string & extraFields,
     		std::string & matchId, long long & durationMs, std::string & error);
     	bool IsMatchActive();
     	static std::string EscapeJson(const std::string & in);
     ```
   - Özel bölüme ekle:
     ```cpp
     	bool EmitControl(int ctl, const char * ev, const std::string & path, const std::string & fields); // ignores limits
     	bool EndMatchLocked(const std::string & result, const std::string & extraFields,
     		std::string & matchId, long long & durationMs, std::string & error); // m_matchLock held
     	void AppendLine(std::string & buffer, const TelemetryEvent & e);
     	void FlushBuffer(std::string & buffer, uint64 & pending, FILE * target);
     	void OpenMatchFile(const TelemetryEvent & e);   // writer thread
     	void CloseMatch(const TelemetryEvent & e);      // writer thread

     	// Caller-side match state, guarded by m_matchLock (lock order: m_matchLock, then m_lock; never the reverse).
     	std::mutex m_matchLock;
     	bool m_matchActive;
     	std::string m_matchId;
     	std::string m_matchPath;            // .jsonl path of the active match
     	long long m_matchStartMs;           // steady_clock ms
     	uint64 m_matchBaseSoft;             // m_droppedSoft / m_droppedHard when the match began
     	uint64 m_matchBaseHard;
     	uint32 m_matchRun;                  // last run number handed out (process lifetime)

     	// Writer-side match state (writer thread only; Start()/Stop() touch it only while no writer runs).
     	FILE * m_matchFile;                 // nullptr = no match file (also when it could not be opened)
     	std::string m_curMatchId;           // empty = not inside a match
     	std::string m_curMatchPath;
     	std::string m_curStartFields;
     	uint64 m_matchLines;
     	std::map<std::string, uint64> m_matchCounts;   // ev -> lines written in the current match
     ```
     Kurucu başlatıcı listesine (`Telemetry.cpp`) `m_matchActive(false), m_matchStartMs(0), m_matchBaseSoft(0), m_matchBaseHard(0), m_matchRun(0), m_matchFile(nullptr), m_matchLines(0)` ekle.

3. **`GameServer/Bot/Telemetry.cpp`:**
   - Include: `<map>` başlıktan gelir; `<ctime>` zaten var.
   - **Yardımcılar (statik):**
     - `static bool IsSafeToken(const std::string & s)`: uzunluk 1..32, her karakter `[A-Za-z0-9_.-]`, ilk karakter `.` değil.
     - `static void FormatUtc(char * out, size_t size)`: `time_t now = time(nullptr); struct tm utc; gmtime_s(&utc, &now);` `strftime(out, size, "%Y-%m-%dT%H:%M:%SZ", &utc)`.
     - `static bool FileExists(const char * path)`: `GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES`.
     - `Telemetry::EscapeJson`: `JsonEscape`'i çağırıp dizeyi döndürür (`JsonEscape` statik kalır).
   - **`EmitControl(ctl, ev, path, fields)`:** `if (!m_running.load()) return false;` `TelemetryEvent`'i kur (`t` = `Emit()`'teki gibi steady ms, `ev`, `bot = -1`, `ctl`, `name = path`, `fields`); `std::lock_guard<std::mutex> lock(m_lock); m_queue.push_back(std::move(event)); return true;` **Sınır denetimi yok** (denetim olayları düşmez; iki olay HARD_LIMIT'i en çok 2 aşar, kabul).
   - **`BeginMatch(...)`:**
     1. `std::lock_guard<std::mutex> lock(m_matchLock);` `!m_running.load()` → `error = "telemetry is off"`, `false`. `m_matchActive` → `error = "a match is already active (" + m_matchId + ")"`, `false`. `!IsSafeToken(scenario)` → `error = "bad scenario name"`, `false`.
     2. Tarih klasörü: `Start()`'taki kalıp (`CreateDirectory("Logs", NULL); CreateDirectory("Logs/bots", NULL);` + `Logs/bots/%04d-%02d-%02d` yerel tarih, `CreateDirectory`).
     3. Kimlik: `run = m_matchRun + 1`'den başlayarak, `id = scenario + "-" + std::to_string(seed) + "-" + std::to_string(run)`; `path = dir + "/" + id + ".jsonl"`, `summary = dir + "/" + id + ".summary.json"`; ikisinden biri varsa `run++` (en çok 1000 deneme; aşılırsa `error = "no free match id"`, `false`). Bulunan `run` → `m_matchRun = run`.
     4. Taban sayaçları: `m_lock` altında `m_matchBaseSoft = m_droppedSoft; m_matchBaseHard = m_droppedHard;` (kilit sırası `m_matchLock` → `m_lock`).
     5. Alanlar: `"ts_utc":"<FormatUtc>","scenario":"<EscapeJson(scenario)>","seed":<seed>,"run":<run>` + (`extraFields` boş değilse `,` + `extraFields`). `EmitControl(TELCTL_MATCH_START, "MATCH_START", path, fields)` başarısızsa (`!m_running`) `error = "telemetry is off"`, `false`.
     6. `m_matchActive = true; m_matchId = id; m_matchPath = path; m_matchStartMs = <steady ms>;` `matchId = id`; `true`.
   - **`EndMatch(...)`:** `std::lock_guard<std::mutex> lock(m_matchLock); return EndMatchLocked(...);`. **`EndMatchLocked`:** `!m_matchActive` → `error = "no active match"`, `false`; `result` güvenli değilse (`!IsSafeToken`) → `error = "bad result"`, `false`. `durationMs = now_ms - m_matchStartMs`. `m_lock` altında `dropSoft = m_droppedSoft - m_matchBaseSoft`, `dropHard = ...` oku. Alanlar: `"ts_utc":"<utc>","duration_ms":<ms>,"result":"<escaped>","dropped_soft":<n>,"dropped_hard":<n>` + (`extraFields` boş değilse `,` + `extraFields`). `EmitControl(TELCTL_MATCH_END, "MATCH_END", "", fields)`; sonra `matchId = m_matchId; m_matchActive = false; m_matchId.clear(); m_matchPath.clear();` `true`. (`EmitControl` `false` dönerse `m_matchActive = false` yine de yapılır ve `error = "telemetry is off"` ile `false` dön; `Stop()` yolu için.)
   - **`IsMatchActive()`:** `m_matchLock` altında `m_matchActive`.
   - **`Stop()`:** `if (m_writerThread == nullptr && m_file == nullptr) return;` satırından **sonra**, `m_running = false;`'tan **önce**:
     ```cpp
     	{
     		std::lock_guard<std::mutex> lock(m_matchLock);
     		if (m_matchActive)
     		{
     			std::string id, error;
     			long long ms = 0;
     			if (EndMatchLocked("aborted", "", id, ms, error))
     				WriteTelemetryLog(("Telemetry: match " + id + " aborted at shutdown").c_str());
     		}
     	}
     ```
     Yazıcı birleştikten sonra (`fclose(m_file)`'dan önce) `m_matchFile != nullptr` ise `fclose(m_matchFile); m_matchFile = nullptr;` (savunma; normalde `CloseMatch` kapatmıştır). Mevcut kapanış logu aynı kalır.
   - **`WriteBatch(batch)` yeniden yapılandırması:** tek tampon yerine hedef değişince boşaltan döngü:
     ```
     std::string buffer; uint64 pending = 0;
     for each e in batch:
         if (e.ctl == TELCTL_MATCH_START)
         {
             FlushBuffer(buffer, pending, m_matchFile != nullptr ? m_matchFile : m_file);
             OpenMatchFile(e);            // sets m_curMatchId, opens m_matchFile (may stay nullptr), resets counters
         }
         AppendLine(buffer, e); pending++;
         if (!m_curMatchId.empty()) { m_matchLines++; m_matchCounts[e.ev]++; }
         if (e.ctl == TELCTL_MATCH_END)
         {
             FlushBuffer(buffer, pending, m_matchFile != nullptr ? m_matchFile : m_file);
             CloseMatch(e);               // summary.json, fclose, clears match state
         }
     FlushBuffer(buffer, pending, m_matchFile != nullptr ? m_matchFile : m_file);
     ```
     - **`AppendLine`:** F3-01'deki satır biçimi (`:263-287`) aynen, tek fark: `"match":"` + (`m_curMatchId.empty() ? "-" : EscapeJson(m_curMatchId)`) + `"` (kimlik güvenli karakterlerden oluşur; yine de `JsonEscape` uygula). Denetim olayı (`e.ctl != TELCTL_NONE`) için `name` **yazılmaz** (MATCH_START'ta `name` yol taşır). `mode` hep `"live"`.
     - **`FlushBuffer(buffer, pending, target)`:** `buffer.empty()` ise `pending = 0` yapıp dön. `fwrite` + `fflush(target)`; `written == buffer.size()` ise `m_lock` altında `m_written += pending`; değilse F3-01'deki bir kerelik `Telemetry: write error` logu (`m_written`'a ekleme). Her durumda `buffer.clear(); pending = 0;`. (`target == nullptr` olamaz: `m_file` yazıcı çalışırken geçerlidir.)
     - **`OpenMatchFile(e)`:** `m_curMatchId` = `e.name`'den dosya adı gövdesi (yolun son `/` sonrası, `.jsonl` uzantısız); `m_curMatchPath = e.name`; `m_curStartFields = e.fields`; `m_matchLines = 0; m_matchCounts.clear();` `m_matchFile = fopen(e.name.c_str(), "ab");` `nullptr` ise `Telemetry: cannot open <path>, match lines go to the live file` logla (satırlar `m_file`'a düşer, maç kimliği yine de yazılır). Başarıda `Telemetry: match <id> started, writing <path>` logla.
     - **`CloseMatch(e)`:** `summary` yolu = `m_curMatchPath` sonundaki `.jsonl` yerine `.summary.json`. İçerik (tek satır + `\n`, `"wb"` ile): 
       ```
       {"match":"<id>","mode":"live","file":"<id>.jsonl","start":{<m_curStartFields>},"end":{<e.fields>},"lines":<m_matchLines>,"events":{"<ev>":<n>,...}}
       ```
       (`events`: `m_matchCounts` sırasıyla; `MATCH_START` ve `MATCH_END` dahil; alt çizgi/harf dışında karakter içermeyen sabit kodlar.) `fwrite` başarısızsa `Telemetry: summary write error` logla. Sonra `m_matchFile` açıksa `fclose`, `m_matchFile = nullptr; m_curMatchId.clear(); m_curMatchPath.clear(); m_curStartFields.clear();` ve `Telemetry: match <id> ended, <lines> lines, summary <summary yolu>` logla.
   - `Emit()`, `GetStats()`, `WriterLoop()`, `RunSelfTest()` değişmez (yalnızca `WriteBatch`'in yeni biçimi onları etkiler).
   - Yeni log satırlarının hepsi `Telemetry: ` önekli ve `Telemetry.cpp` içinde; yeni kodda `printf` yok (`snprintf`/`fprintf` yalnızca `WriteTelemetryLog`'da).

4. **`GameServer/Bot/BotManager.h`:**
   - `private:` bölümünde `void CommandList();` (`:62`) satırının altına: `void CommandMatch(const std::string & args);`
   - Son veri üyelerinin (`m_perfWindowOpen`, `:102`) altına:
     ```cpp
     	uint32 m_matchPerfSamples = 0;   // IOCP thread only: PERF_SAMPLEs emitted since the last "match start"
     	uint32 m_matchP95MaxUs = 0;      // IOCP thread only: highest tick_p95_us among them
     	uint32 m_matchTickMaxUs = 0;     // IOCP thread only: highest tick_max_us among them
     ```
     (Kurucu listesine dokunma.)

5. **`GameServer/Bot/BotManager.cpp`:**
   - `EmitPerfSample()` içinde, `Emit` çağrısından (`:456`) hemen **önce**: `m_matchPerfSamples++; if (p95 > m_matchP95MaxUs) m_matchP95MaxUs = p95; if (maxUs > m_matchTickMaxUs) m_matchTickMaxUs = maxUs;` (mevcut diğer satırlar değişmez).
   - `ExecuteCommand()`: `else if (_stricmp(verb.c_str(), "list") == 0) CommandList();` dalından sonra yeni dal: `else if (_stricmp(verb.c_str(), "match") == 0) CommandMatch(args);`. Bilinmeyen komut metnini `"(spawn, despawn, list, match)"` yap (tek izinli metin değişikliği).
   - `CommandList()` fonksiyonundan **sonra** (`ParseSpawnList`'ten önce) yeni fonksiyon:
     ```cpp
     void BotManager::CommandMatch(const std::string & args)
     ```
     Ayrıştırma: ilk sözcük `sub` (`start`/`end`, `_stricmp`), kalan `rest` (`Trim`); sözcükleri `SplitNames` yerine boşlukla ayır (`SplitNames` virgülü de ayırır; senaryo adlarında virgül zaten geçersiz, kullanılabilir). Davranış:
     - **`start <senaryo> [seed]`:** senaryo yoksa `BotManager: cmd match start: no scenario given`; `seed` verildiyse yalnızca rakam ve ≤ 10 hane, `strtoull` ile `0..4294967295` aralığında olmalı, değilse `BotManager: cmd match start: bad seed '<değer>'` (tampon 224) ve dön; verilmediyse `0`. Fazladan sözcük → `BotManager: cmd match start: too many arguments`, dön. Bileşim: `m_sessions` içinde `m_phase == BotSession::PHASE_IN_GAME` olanların `m_charName` değerlerinden `"composition":["A","B"]` (her ad `Telemetry::EscapeJson`) ve `"in_game":<n>`; `extra` = `"composition":[...],"in_game":<n>`. `Telemetry::Instance().BeginMatch(scenario, seed, extra, id, error)` başarılıysa `m_matchPerfSamples = 0; m_matchP95MaxUs = 0; m_matchTickMaxUs = 0;` ve `BotManager: cmd match start: <id> started (<n> bot(s) in game)`; başarısızsa `BotManager: cmd match start: refused (<error>)`.
     - **`end [sonuç]`:** `sonuç` verilmediyse `"completed"`; fazladan sözcük → `BotManager: cmd match end: too many arguments`. `extra` = `"in_game":<n>,"perf_samples":<m_matchPerfSamples>,"tick_p95_max_us":<m_matchP95MaxUs>,"tick_max_us":<m_matchTickMaxUs>`. `EndMatch(result, extra, id, durationMs, error)` başarılıysa `BotManager: cmd match end: <id> ended (result <sonuç>, <ms> ms)`; başarısızsa `BotManager: cmd match end: refused (<error>)`.
     - Boş `sub` veya bilinmeyen `sub` → `BotManager: cmd match: usage: match start <scenario> [seed] | match end [result]`.
     Tüm loglar `WriteBotLog` ile; `printf` yok; `char message[320]` tamponları `snprintf` ile (kullanıcı metni `%s` ile, taşma koruması). `PERF_SAMPLE` yalnızca tamamlanmış 5 sn pencerelerini sayar (son kısmi pencere maç özetine girmez; bu bir hata değildir).
   - `RESPAWN_CYCLES != 0` iken `ExecuteCommand`'ın başındaki ret (`:548-552`) `match` komutunu da kapsar (soak modunda maç komutu yoktur); bu değişmez.

6. **Derle:** `./tools/build.sh Release` ve `./tools/build.sh Debug`. `Telemetry.cpp` ve `BotManager.cpp`'yi (`touch`) yeniden derlenmeye zorla; `Bot\` dosyalarında **uyarı olmamalı** (`%llu`/`%lld` için `unsigned long long`/`long long` açık cast, `size_t`→`uint32` için açık cast). Sunucuyu çalıştırma.

7. **Raporu yaz** (şablon bölümü) ve commit et: `[F3-02] ...`. `git add` ile yalnızca §4'teki dosyaları ve bu planı ekle.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; **yeni uyarı yok** (`Bot\` dosyalarında uyarı olmamalı; çıktıyı yapıştır).
- [ ] K2: `./tools/build.sh Debug` hatasız biter.
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F3-02` yalnızca §4'teki 4 dosyayı (+ bu plan dosyası) içerir; `.vcxproj`/`.filters` değişmemiş. `BotManager.cpp` farkında `CommandList`, `CommandSpawn`, `CommandDespawn`, `ParseSpawnList`, `TickSessions`, `PollDespawn`, `StartSession`, `FailSession`, `BeginDespawn`, `ProcessCommands`, `PollCommandFile` gövdelerinde satır değişikliği yok; `-` satırları yalnızca `ExecuteCommand`'daki bilinmeyen komut metni için (`git diff ... -- GameServer/Bot/BotManager.cpp | grep '^-'` çıktısını yapıştır).
- [ ] K4: Thread/kilit kuralı: `m_matchLock` yalnızca `BeginMatch`/`EndMatch`/`IsMatchActive`/`Stop` içinde; kilit sırası her yerde `m_matchLock` → `m_lock` (ters sıra yok); `Emit()` gövdesi F3-01'dekiyle aynı (kilit altında dize birleştirme yok); `m_matchFile`, `m_curMatch*`, `m_matchLines`, `m_matchCounts` yalnızca `WriteBatch`/`AppendLine`/`OpenMatchFile`/`CloseMatch` (yazıcı) ve `Stop()` (yazıcı birleştikten sonra) içinde (`grep -n "m_matchLock\|m_matchFile\|m_curMatchId\|m_matchCounts" GameServer/Bot/Telemetry.cpp` çıktısını satır satır açıklayarak yapıştır).
- [ ] K5: `m_lock` altında yalnızca sayaç güncellemesi, `push_back`, `swap`, `size()` ve `reserve` var (`EmitControl` dahil); kilit altında `fwrite`/`snprintf`/`Emit`/`fopen` yok (`sed -n` çıktısı, satır numarasıyla).
- [ ] K6: Denetim olayları sınırsız: `EmitControl` içinde `HARD_LIMIT`/`SOFT_LIMIT` denetimi yok; normal `Emit()` aynen (`sed -n` çıktısı).
- [ ] K7: Maç yokken biçim aynı: `AppendLine` `m_curMatchId` boşken `"match":"-"` yazar ve satır biçimi F3-01'dekiyle byte düzeyinde aynıdır (`git diff` ile eski/yeni satır kurma kodu karşılaştırması; `"mode":"live"` değişmez).
- [ ] K8: Güvenlik/doğrulama: `IsSafeToken` (`[A-Za-z0-9_.-]`, 1..32, `.` ile başlamaz) hem senaryo hem sonuç için uygulanır; dosya yolu yalnızca doğrulanmış kimlikten kurulur; `seed` `0..4294967295` dışında reddedilir (`sed -n` çıktısı).
- [ ] K9: Kapanış: `Stop()` açık maçı `EndMatchLocked("aborted", ...)` ile kapatır ve `Telemetry: match <id> aborted at shutdown` loglar; `Start()` `TELEMETRY=off` iken yine hiçbir şey oluşturmaz; `BeginMatch` `m_running == false` iken `telemetry is off` ile `false` döner (`sed -n` çıktısı).
- [ ] K10: `summary.json` biçimi: tek satırlık JSON, alanlar `match, mode, file, start, end, lines, events` (bu sırada); `start`/`end` iç içe nesne (anahtar çakışması yok) (`sed -n` çıktısı; çalışma zamanı kanıtı Claude doğrulamasında).
- [ ] K11: Kodlama: `file GameServer/Bot/*` hepsi "ASCII text, with CRLF line terminators"; girinti tab, Allman, yorumlar İngilizce; yeni kodda `printf` yok (`grep -n "printf" GameServer/Bot/Telemetry.cpp GameServer/Bot/BotManager.cpp` yalnızca `snprintf`/`fprintf`; `fprintf` yalnızca `WriteTelemetryLog`/`WriteBotLog`).
- [ ] K12: `git status --short` boş (yalnızca izinli dosyalar commit'li). Sunucu çalıştırılmadı, `GameServer.ini` ve veritabanı değiştirilmedi.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
git diff --stat gece/2026-10-02...bot/F3-02
git diff gece/2026-10-02...bot/F3-02 -- GameServer/Bot/BotManager.cpp | grep '^-'
grep -n "m_matchLock\|m_matchFile\|m_curMatchId\|m_matchCounts\|IsSafeToken" GameServer/Bot/Telemetry.cpp
grep -n "printf" GameServer/Bot/Telemetry.cpp GameServer/Bot/BotManager.cpp
file GameServer/Bot/*
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile (her senaryodan sonra ini yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir, KI-010 notu: kapanış `CTRL_BREAK` ile):
1. **Maç yaşam döngüsü:** `ENABLED=1, MAX_BOTS=16, TELEMETRY=summary, SPAWN_ON_START=BotWP_K,BotMF_K,BotWP_E,BotPHD_E`; botlar `in game` olunca `BotCommands.txt` ile `match start t1 7`, ~12 sn sonra `match end ok` → `Logs/bots/<tarih>/t1-7-1.jsonl`: ilk satır `MATCH_START` (`match":"t1-7-1`, `ts_utc`, `scenario`, `seed`, `run`, `composition` 4 ad, `in_game=4`), araya ≥ 2 `PERF_SAMPLE` (`"match":"t1-7-1"`), son satır `MATCH_END` (`result":"ok`, `duration_ms` ≈ 12000, `dropped_*=0`, `perf_samples`, `tick_p95_max_us`); `t1-7-1.summary.json` `json.loads` geçerli, `lines` = dosya satır sayısı, `events` toplamı = `lines`. `live-*.jsonl`'de aynı pencerede `PERF_SAMPLE` satırı **yok** (hepsi maç dosyasında), maçtan önce/sonra `"match":"-"`.
2. **İkinci maç ve kimlik:** aynı komutlarla `t1-7-2.*` oluşur; `match start bad/name`, `match start t1 abc`, `match start t1 99999999999`, ikinci `match start` (açıkken), `match end` (açık maç yokken), `match end bad/result` → her biri `refused (...)` / `bad seed` mesajıyla, dosya yok/değişmedi.
3. **Kapanışta iptal:** maç açıkken sunucuyu `CTRL_BREAK` ile kapat → `Telemetry: match <id> aborted at shutdown`, dosyada son satır `MATCH_END` (`result":"aborted`), `summary.json` var. (`run-servers.sh stop` yolunda `Stop()` çalışmaz, KI-010; bu yolda maç açıksa dosya `MATCH_END`'siz kalır: bilinen kısıt.)
4. **`TELEMETRY=off`:** `match start x` → `refused (telemetry is off)`, dosya yok. **`ENABLED=0`:** dosya/klasör oluşmaz, `BotCommands.txt` dokunulmadan kalır.
5. **Gerilemesiz:** F3-01 (maçsız 4 bot, `PERF_SAMPLE` `in_game=4`) ve F2-06 (`spawn`/`despawn`/`list`) aynen; `RESPAWN_CYCLES=2` iken `match start` → `cmd rejected (RESPAWN_CYCLES is active)`.
6. Sunucu 3/3 UP, `GameServer.log`'a yeni hata yok.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` dosyaları ASCII; kod yorumları İngilizce.
- **Bot sistemi varsayılan kapalı.** Bu plan yeni davranışı yalnızca `ENABLED=1` **ve** `match` komutu verildiğinde etkinleştirir; kapalıyken sunucu davranışı ve dosya sistemi değişmez.
- **Oyun thread'ini bloklama:** `BeginMatch`/`EndMatch` IOCP thread'inden çağrılır: içlerinde yalnızca küçük işler var (`CreateDirectory`, birkaç `GetFileAttributes`, bir kuyruk ekleme); **dosya açma/yazma yazıcı thread'dedir** (`fopen`/`fwrite`/summary yazımı IOCP thread'inde yapılmaz). `Emit` içinde disk G/Ç yok.
- **Sıralama güvencesi:** maç sınırları kuyrukta sıralı denetim olaylarıdır; `MATCH_START`'tan önce kuyruğa girmiş olaylar eski dosyaya/`"-"` ile, sonra girenler maç dosyasına yazılır. Bunu bozacak ikinci bir kuyruk/yol ekleme.
- **Thread kuralı (ADR-0005):** `CommandMatch` yalnızca IOCP thread'inde (`ExecuteCommand` zinciri) çalışır, `m_sessions`/`BotSession` alanlarını yalnızca orada okur. `Telemetry` maç API'si thread-güvenlidir (`m_matchLock`).
- **Çökme/`stop` yolu:** çökmede veya `taskkill` ile sonlandırmada açık maç `MATCH_END`'siz ve `summary.json`'suz kalabilir (KI-010; kabul edilen).
- **Kişisel veri:** `MATCH_START` bileşimi yalnızca sabit bot karakter adlarını içerir (`BOT_TABLE`); gerçek oyuncu adı/hesap kimliği telemetriye yazılmaz.
- Log hacmi: maç başına 2 ek satır + `summary.json` (~0,5 KB).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release` son satırlar):
- Kabul kriterleri öz-değerlendirme (K1..K12):
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(yok)
