# F4-20: Betikli test dizisi, dilim 2 — `ScriptRunner`: betik dosyasını zamanlayıp çalıştıran `/bot script run|stop|status`

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-20` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-19 (`BotCore/ScriptPlan.h`, `ParseScript`) — `KAPANDI` (merge `66342b7`); F3-03 (`ScenarioRunner` kalıbı) — `KAPANDI`; F4-01..F4-18 (betiğin sürdüğü komutlar) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/17` F4 "Görevler 6) Betikli test senaryoları" ve "Kabul: betikli dizilerde sunucuya giden geçersiz aksiyon ≤ %1"; MET-ACT-02, MET-FAIR-01 (`docs/16` §3.4); ADR-0015 (komut kanalı), ADR-0017 Eki F4-19 madde 5-6 |
| Tahmini büyüklük | M (2 yeni dosya + `BotManager.h/.cpp` küçük eklemeler + 2 proje dosyası + 1 örnek betik) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

F4-19 betik biçimini ve doğrulayıcıyı verdi (`BotCore::ParseScript`), ama hiçbir yerden çağrılmıyor. Bu plan **çalıştırıcıyı** ekler: `/bot script run <ad>` `./Scripts/<ad>.txt` dosyasını okur, `ParseScript` ile doğrular (ya-hep-ya-hiç), komut çalıştığı andan itibaren her adımı kendi ofsetinde, mevcut `BotManager::ExecuteCommand` yolundan çalıştırır (yani aynı `ActionExecutor` + `BotFairnessGuard` + gerçek handler yolu; betik bota yeni yetki vermez). Her adım, başlangıç ve bitiş `decisions` seviyesinde telemetriye (`SCRIPT_START` / `SCRIPT_STEP` / `SCRIPT_END`) ve Bot günlüğüne yazılır; böylece sonraki dilim (MET-ACT-02 / MET-FAIR-01 raporu) `ACTION_*` / `FAIRNESS_REJECT` olaylarını betik adımlarıyla ilişkilendirebilir.

Bot sistemi kapalıyken (`[BOT] ENABLED=0`, varsayılan) hiçbir şey değişmez.

## 2. Bağlam (okunması zorunlu)

- `plans/F4-19-betik-ayristirici.md` §5.2-5.3 ve `BotCore/ScriptPlan.h`: biçim, sınırlar ve `ParseScript` sözleşmesi (`ScriptParseResult`, `ScriptErrorText`, `kScriptMaxBytes`). Bu planda **değiştirilmez**.
- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` Ek (F4-19) madde 2-6: betik = sabit zaman damgalı mevcut `/bot` komutları; yaşam döngüsü komutları yasak; çalıştırıcı bu planın işi.
- `docs/adr/ADR-0015-bot-calisma-zamani-komut-kanali.md`: komutlar yalnızca IOCP thread'inde, `Tick()` içinden çalışır.
- `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md` §3.2 (olay tablosu): yeni üç olay bu tabloya Claude tarafından eklenecek (bu plan `docs/`'a dokunmaz).
- İlgili kod (hepsini açıp doğrula; satırlar `66342b7` itibarıyla):
  - `GameServer/Bot/ScenarioRunner.h:12-53` ve `ScenarioRunner.cpp`: **izlenecek kalıp**. `explicit ScenarioRunner(BotManager &)`, `Command(args)`, `Tick(now)` (boşta ilk satırda döner), dosya-statik `WriteScenarioLog` (`:22`), `Trim` (`:38`), `IsSafeFileStem` (`:83`, `[A-Za-z0-9_-]{1,40}`), `Command()` argüman ayrıştırma (`:478-512`), `CommandRun/Stop/Status` (`:515`, `:838`, `:857`).
  - `GameServer/Bot/BotManager.h:8` (`#include "ScenarioRunner.h"`), `:50` (`friend class ScenarioRunner;`), `:52-56` (kurucu başlatıcı listesi, `m_scenario(*this)` en sonda), `:141` (`ScenarioRunner m_scenario;   // IOCP thread only`).
  - `GameServer/Bot/BotManager.cpp:429` `m_scenario.Tick(std::chrono::steady_clock::now());` (`Tick()` içinde, `ProcessCommands()` ve `TickSessions()` sonrası).
  - `GameServer/Bot/BotManager.cpp:591-665` `BotManager::ExecuteCommand`: `:619-620` `scenario` dalı (`m_scenario.Command(args)`), `:661-662` bilinmeyen komut mesajı (komut listesi metni).
  - `GameServer/Bot/Telemetry.h:76` `Emit(level, ev, bot, name, fields, droppable)`; örnek kullanım `ActionExecutor.cpp:115` (`TEL_DECISIONS`, alan parçası ham JSON, süslü parantez ve baştaki virgül yok); botsuz olay için `bot = -1`, `name = nullptr` (`PERF_SAMPLE`, `BotManager.cpp` `EmitPerfSample`).
  - `GameServer/proj-GameServer.vcxproj:197,293` (`Bot\ScenarioRunner.cpp` / `.h`) ve `proj-GameServer.vcxproj.filters:96-98,227-229`.
  - `bots/config/scenario_smoke_2bot.yaml`: örnek dosya biçimi/üst yorum kalıbı.

## 3. Kapsam

**Yapılacaklar**

1. `GameServer/Bot/ScriptRunner.h/.cpp` (yeni): `ScriptRunner` sınıfı, `Command()` (`run <ad>` | `stop` | `status`) ve `Tick(now)`.
2. `BotManager`: `script` komutu `ExecuteCommand`'ta `m_script.Command(args)`'a yönlenir, `Tick()` her turda `m_script.Tick(now)` çağırır, `friend class ScriptRunner;`.
3. `bots/config/script_smoke_2bot.txt` (yeni): örnek betik.
4. `proj-GameServer.vcxproj` ve `.filters`: iki yeni dosya.

**Kapsam dışı (yapılmayacak)**

- `BotCore/ScriptPlan.h`, `Tests/BotCoreTests/*` ve mevcut `BotCore/*.h` **değişmez** (yeni birim testi yok: zamanlama mantığı `std::chrono` + `ExecuteCommand` çağrısıdır, saf bir parça kalmadı; doğrulama çalışma zamanında Claude'dadır, §6 K12).
- `ScenarioRunner.*`, `ActionExecutor.*`, `BotSession.*`, `Telemetry.*`, `ChatHandler.cpp`, `User.h` **değişmez**. `+bot script ...` (oyun içi GM komutu) **yok**: `script` yalnızca konsol ve `BotCommands.txt` yolundan çalışır (diğer F4 komutları gibi).
- Betik–senaryo birleştirmesi (senaryo botları açsın, hazır olunca betik başlasın, maç bitince betik dursun): **yok**. İki yetenek bağımsız çalışır (ADR-0017 Eki F4-19 madde 3); bağlama sonraki bir dilimin konusudur. Betik çalışırken maç/senaryo açık olabilir ya da olmayabilir; çalıştırıcı bunu denetlemez.
- Yeni `GameServer.ini` anahtarı, yeni komut kuyruğu, yeni thread, zamanlayıcı: **yok** (adımlar `BotManager::Tick()` turunda, mevcut IOCP thread'inde çalışır; çözünürlük `TICK_MS`).
- Adım sonucunu okumak/beklemek (başarılı mı, reddedildi mi), koşul, tekrar, dallanma: **yok**. Sonucu `ACTION_RESULT` / `FAIRNESS_REJECT` telemetrisi taşır (F4-01..F4-11).
- Çalışan bir betik durdurulunca botların süren eylemlerini (yürüyüş serisi, saldırı serisi) iptal etmek: **yok**; operatör `stop all` / `attack all off` kullanır. Bu davranış `script stop` logunda yazılır.
- Sunucu kapanışında çalışan betik için `SCRIPT_END` yazmak: **yok** (`Shutdown()` değişmez).
- Telemetri raporu (`tools/bot-telemetry-report.py`) değişikliği, `docs/` değişikliği (Claude yapar), bot adı/argüman ön doğrulaması.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/ScriptRunner.h` | yeni | sınıf bildirimi |
| `GameServer/Bot/ScriptRunner.cpp` | yeni | tüm mantık |
| `GameServer/Bot/BotManager.h` | değiştir | yalnızca `#include "ScriptRunner.h"`, `friend class ScriptRunner;`, kurucu başlatıcısı, `ScriptRunner m_script;` |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `script` dalı, `Tick()` kancası, bilinmeyen komut mesajına `script` |
| `GameServer/proj-GameServer.vcxproj` | değiştir | yalnızca iki satır (`ClCompile` + `ClInclude`) |
| `GameServer/proj-GameServer.vcxproj.filters` | değiştir | yalnızca iki öğe (`ClCompile` + `ClInclude`, mevcut `ScenarioRunner` öğeleri gibi) |
| `bots/config/script_smoke_2bot.txt` | yeni | örnek betik |

Dosya sayısı 7 (plan dosyası dahil 8). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-20 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `GameServer/Bot/ScriptRunner.h`

`ScenarioRunner.h` biçiminde (`#pragma once`, tab, Allman, İngilizce kısa yorumlar, CRLF). Include'lar: `<chrono>`, `<string>`, `<vector>`, `"../../BotCore/ScriptPlan.h"`. `uint32` türü `stdafx.h`'den gelir (`ScenarioRunner.h` da aynı varsayımla `uint32` kullanıyor; `.h` içinde `stdafx.h` dahil etme).

Adlar ve imzalar **bağlayıcıdır**:

```cpp
#pragma once

#include <chrono>
#include <string>
#include <vector>
#include "../../BotCore/ScriptPlan.h"

class BotManager;

// Runs a test script (./Scripts/<name>.txt, parsed by BotCore::ParseScript): every step is a
// normal /bot command executed through BotManager::ExecuteCommand() at its offset (ADR-0017
// Ek F4-20). IOCP thread only: Command() is called from ExecuteCommand(), Tick() from
// BotManager::Tick().
class ScriptRunner
{
public:
	explicit ScriptRunner(BotManager & mgr);   // trivial: no allocation, no I/O

	void Command(const std::string & args);    // "run <name>" | "stop" | "status"
	void Tick(std::chrono::steady_clock::time_point now);   // returns at once while idle

private:
	static bool LoadScript(const std::string & name, std::vector<BotCore::ScriptStep> & steps, std::string & error);
	void CommandRun(const std::string & name);
	void CommandStop();
	void CommandStatus();
	void Finish(const char * result);          // logs the summary, emits SCRIPT_END, back to idle

	BotManager & m_mgr;
	bool m_running;
	std::string m_name;
	std::vector<BotCore::ScriptStep> m_steps;
	size_t m_next;                             // index of the next step to run
	std::chrono::steady_clock::time_point m_start;
	uint32 m_maxLateMs;                        // largest (actual - planned) offset so far
};
```

### 5.3 `GameServer/Bot/ScriptRunner.cpp`

Başlık: `#include "stdafx.h"`, `"ScriptRunner.h"`, `"BotManager.h"`, `"Telemetry.h"`; `<cstdio>`, `<cstring>`, `<ctime>`.

Dosya-statik yardımcılar (`ScenarioRunner.cpp`'dekilerin küçük kopyaları; o dosyaya dokunma, kendi dosyasında kalsın): `WriteScriptLog(const char *)` (`ScenarioRunner.cpp` `WriteScenarioLog` ile aynı: `./Logs/Bot_<gün>_<ay>_<yıl>.log`, açılamazsa sessizce atlanır), `IsSafeFileStem(const std::string &)` (`[A-Za-z0-9_-]{1,40}`), `SplitWords(args, words)` (boşluk/sekme; `BotManager.cpp:121` `SplitWords` ile aynı davranış, o da dosya-statiktir). Sabit: `SCRIPT_LOG_CMD_MAX = 120` (günlüğe yazılan komut metni bu kadar karakterle kesilir; arabellek taşmasın).

**Kurucu:** `m_mgr(mgr), m_running(false), m_next(0), m_maxLateMs(0)`; başka hiçbir şey (ayırma/G/Ç yok).

**`LoadScript(name, steps, error)`:**

1. `IsSafeFileStem(name)` değilse `error = "bad script name"`, `false`.
2. Yol `"./Scripts/" + name + ".txt"` (başka kaynaktan yol kurma). `fopen(path, "rb")` başarısızsa `error = "cannot open " + path`, `false`.
3. En çok `BotCore::kScriptMaxBytes + 1` bayt `fread` ile tek `std::string`'e oku (arabellek: `char buffer[BotCore::kScriptMaxBytes + 1]` yığında 8 KB kabul; ya da `std::string text(kScriptMaxBytes + 1, '\0')` + `resize`). `fclose` **her yolda** çağrılmalı (dosya açıkken hiçbir yoldan dönme). Dosya 8192 bayttan uzunsa metin 8193 bayt olur ve ayrıştırıcı `SCRIPT_ERR_TOO_LARGE` döndürür (bu planda boyut denetimi **yapılmaz**, ayrıştırıcıya bırakılır).
4. `BotCore::ParseScript(text)`; `result.error != BotCore::SCRIPT_OK` ise `error = name + ".txt" + (errorLine > 0 ? ":" + std::to_string(errorLine) : "") + ": " + BotCore::ScriptErrorText(result.error)`, `false`. Aksi halde `steps = result.steps`, `true`.

**`Command(args)`:** `SplitWords`; `words.size() == 2 && _stricmp(words[0], "run") == 0` → `CommandRun(words[1])`; `words.size() == 1 && "stop"` → `CommandStop()`; `words.size() == 1 && "status"` → `CommandStatus()`; aksi halde `WriteScriptLog("ScriptRunner: cmd script: usage: script run <name> | script stop | script status")`.

**`CommandRun(name)`** (her reddetmede tek satır `ScriptRunner: run <name>: refused (<neden>)`, başka yan etki yok):

1. `m_running` ise `refused (already running (<m_name>))`.
2. `LoadScript` başarısızsa `refused (<error>)`.
3. Başarılıysa: `m_name = name; m_steps` = yüklenen adımlar; `m_next = 0; m_maxLateMs = 0; m_start = std::chrono::steady_clock::now(); m_running = true;`
4. Günlük: `ScriptRunner: run <name>: loaded (<N> step(s), last offset <ms> ms)`.
5. `Telemetry::Instance().IsEnabled(TEL_DECISIONS)` false ise ek uyarı satırı: `ScriptRunner: run <name>: warning (telemetry below 'decisions': ACTION_* and FAIRNESS_REJECT events are not recorded)`; betik yine de başlar.
6. `SCRIPT_START` olayı (`Emit(TEL_DECISIONS, "SCRIPT_START", -1, nullptr, fields, false)`), `fields` = `"script":"<EscapeJson(name)>","steps":<N>,"duration_ms":<son adımın ofseti>`.
7. Bu çağrıda adım **çalıştırma** (ilk adımlar bir sonraki `Tick()`'te, `now - m_start >= offset` kuralıyla çalışır; ofset 0 olan adım da bir sonraki turda).

**`Tick(now)`:**

1. `if (!m_running) return;` ilk satır.
2. `long long elapsed = duration_cast<milliseconds>(now - m_start).count();`
3. `while (m_next < m_steps.size() && (long long)m_steps[m_next].offsetMs <= elapsed)`: şu adım için
   - `late = elapsed - offsetMs` (≥ 0); `m_maxLateMs = max(m_maxLateMs, late)`.
   - Günlük: `ScriptRunner: step <i+1>/<N> (line <L>, +<offset> ms, late <late> ms): <komut, SCRIPT_LOG_CMD_MAX ile kesilmiş>`.
   - `SCRIPT_STEP` olayı (`TEL_DECISIONS`, `bot = -1`, `name = nullptr`, droppable `false`), `fields` = `"script":"<ad>","step":<i+1>,"line":<L>,"offset_ms":<offset>,"late_ms":<late>,"verb":"<ilk sözcük>"`. İlk sözcük, komutun ilk boşluk/sekmeye kadar olan kısmıdır (ayrıştırıcı bunun izinli bir verb, yani yalnızca harf olduğunu garanti etti; yine de `Telemetry::EscapeJson` ile sar).
   - `m_mgr.ExecuteCommand(m_steps[m_next].command);`
   - `m_next++`.
   - **Dikkat (yeniden giriş):** `ExecuteCommand` içinden `m_steps` değişmez (ayrıştırıcı `script`, `spawn`, `despawn`, `match`, `scenario` verb'lerini zaten reddettiği için betik adımı çalıştırıcıyı yeniden başlatamaz/durduramaz); yine de döngü her turda `m_next`/`m_steps.size()`'ı yeniden okur, adım komutunu `ExecuteCommand`'a **kopya** (`const std::string &` bir yerel kopya) olarak verir.
4. Döngüden sonra `m_next == m_steps.size()` ise `Finish("completed")`.

**`CommandStop()`:** boştaysa `ScriptRunner: stop: no script running`; aksi halde önce `ScriptRunner: stop: <ad>: stopped after <m_next>/<N> step(s); running bot actions are not cancelled (use 'stop all' / 'attack all off')` günlüğe, sonra `Finish("stopped")`.

**`CommandStatus()`:** boştaysa `ScriptRunner: status: idle`; aksi halde `ScriptRunner: status: <ad> step <m_next>/<N> done, <elapsed> ms elapsed, next in <kalan> ms` (`kalan` = bir sonraki adımın ofseti − elapsed, 0'ın altına inmez; `m_next == size` ise bu duruma gelinmez çünkü `Tick` bitirir).

**`Finish(result)`:** `elapsed = now - m_start` (ms); günlük `ScriptRunner: finished <ad>: <result>, <m_next>/<N> step(s) in <elapsed> ms (max late <m_maxLateMs> ms)`; `SCRIPT_END` olayı (`TEL_DECISIONS`, droppable `false`), `fields` = `"script":"<ad>","result":"<result>","steps_run":<m_next>,"steps_total":<N>,"elapsed_ms":<elapsed>,"max_late_ms":<m_maxLateMs>`; sonra `m_running = false; m_steps.clear(); m_name.clear(); m_next = 0;`.

Ortak kural: tüm günlük arabelleği `char message[400]` ve yalnızca `snprintf` (taşma yok); ham komut metni asla `%s` ile kesilmeden yazılmaz. Telemetri `fields` metni `std::string` olarak kurulur (`std::to_string`).

### 5.4 `BotManager` eklemeleri

- `BotManager.h`: `#include "ScriptRunner.h"` (`ScenarioRunner.h` satırının altına); `friend class ScriptRunner;` (`friend class ScenarioRunner;` altına); kurucu başlatıcı listesinde `m_scenario(*this)` ifadesinden **sonra** `, m_script(*this)` (üye bildirim sırasıyla uyumlu olsun diye aşağıdaki üyeyi `m_scenario`'dan hemen sonra bildir); `ScriptRunner m_script;   // IOCP thread only` (`ScenarioRunner m_scenario;` satırının altına).
- `BotManager.cpp`:
  - `#include "ScriptRunner.h"` gerekmez (`BotManager.h` zaten dahil ediyor).
  - `ExecuteCommand`: `scenario` dalının altına `else if (_stricmp(verb.c_str(), "script") == 0) m_script.Command(args);`.
  - Bilinmeyen komut mesajındaki listeye `script` ekle (`... match, scenario, script, move, ...`).
  - `Tick()`: `m_scenario.Tick(...)` satırının altına `m_script.Tick(std::chrono::steady_clock::now());`.

### 5.5 Örnek betik `bots/config/script_smoke_2bot.txt` (ASCII + CRLF; içerik aynen)

```
# Smoke script: needs BotWP_K and BotWP_E in game (use: /bot spawn BotWP_K,BotWP_E).
# Copy to <server dir>/Scripts/smoke.txt, then: /bot script run smoke
0 list
500 snap BotWP_K
1000 sit BotWP_K
3000 stand BotWP_K
4500 see BotWP_K
5000 npcs BotWP_E
6000 stop all
```

(Bu dosya `ParseScript`'ten geçmelidir: 7 adım, son ofset 6000. `BotCoreTests`'e test **eklenmez**; Claude doğrulamada ayrıştırıcıyla denetler.)

### 5.6 Proje dosyaları

`proj-GameServer.vcxproj`: `Bot\ScenarioRunner.cpp` satırının altına `    <ClCompile Include="Bot\ScriptRunner.cpp" />`, `Bot\ScenarioRunner.h` satırının altına `    <ClInclude Include="Bot\ScriptRunner.h" />`. `proj-GameServer.vcxproj.filters`: `ScenarioRunner` öğelerinin birebir kopyası (`Source Files` / `Header Files` süzgeci) `ScriptRunner` için. Mevcut girinti/CRLF/BOM düzenini koru; başka satır değişmez.

### 5.7 Derleme

```bash
./tools/build.sh Release
./tools/run-tests.sh                  # birim testler değişmedi: 82 tests, 0 failed
```

Sunucu çalıştırma ve çalışma zamanı denemesi DeepSeek'in işi **değildir** (§7 sonu).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter (yeni uyarı yok; `ScriptRunner.cpp` ve `BotManager.cpp` `touch` edilip yeniden derlenmiş günlükte `warning` sayısı 0).
- [ ] K2: `./tools/run-tests.sh` son satırı `82 tests, 0 failed` (birim testler değişmedi, rc 0).
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F4-20` yalnızca §4'teki dosyaları listeler (plan dosyası dahil 8); `BotCore/`, `Tests/`, `ScenarioRunner.*`, `ActionExecutor.*`, `BotSession.*`, `Telemetry.*`, `ChatHandler.cpp`, `docs/` altında **hiçbir** dosya yok.
- [ ] K4: `BotManager` eklemeleri tam olarak §5.4'tür: `git diff gece/2026-10-02...bot/F4-20 -- GameServer/Bot/BotManager.h GameServer/Bot/BotManager.cpp | grep '^[-+]' | grep -v '^[-+][-+]'` çıktısında `-` ile başlayan satır yalnızca kurucu başlatıcı satırı ile bilinmeyen-komut mesajı satırıdır (2 satır), geri kalanı `+`.
- [ ] K5: boşta etkisizlik: `ScriptRunner::Tick` ilk deyimi `if (!m_running) return;`; kurucu yalnızca üyeleri başlatır; `grep -n "CIni\|\.ini\|GetPrivateProfile" GameServer/Bot/ScriptRunner.cpp` boş; `Scripts/` yalnızca `script run` ile okunur (`grep -n "fopen" GameServer/Bot/ScriptRunner.cpp` tam iki satır: biri `WriteScriptLog` içinde günlük için, biri `LoadScript` içinde).
- [ ] K6: dosya güvenliği: yol yalnızca `"./Scripts/" + name + ".txt"` ile kurulur ve `name` önce `IsSafeFileStem`'den geçer; `fclose` her yolda çağrılır (kod okumasıyla: `fopen` ile `fclose` arasında `return` yok).
- [ ] K7: yasak sözcük denetimi: `grep -nE "Sleep|sleep\(|CreateThread|std::thread|rand\(|printf\(" GameServer/Bot/ScriptRunner.cpp` yalnızca `snprintf` çağrılarını ve `WriteScriptLog` içindeki tek `fprintf`'i gösterir; `Sleep`/`thread`/`rand` yok.
- [ ] K8: telemetri olayı adları ve seviye: `grep -n '"SCRIPT_START"\|"SCRIPT_STEP"\|"SCRIPT_END"' GameServer/Bot/ScriptRunner.cpp` üç `Emit` çağrısı, hepsi `TEL_DECISIONS`, `bot = -1`; `SCRIPT_STEP` alanları `script`, `step`, `line`, `offset_ms`, `late_ms`, `verb`; `SCRIPT_END` alanları `script`, `result`, `steps_run`, `steps_total`, `elapsed_ms`, `max_late_ms`; `SCRIPT_START` alanları `script`, `steps`, `duration_ms`.
- [ ] K9: komut yolu: adım yürütme tek noktadan, `m_mgr.ExecuteCommand(` çağrısı `ScriptRunner.cpp`'de **tam bir** yerde; `ActionExecutor`/`BotFairnessGuard` doğrudan çağrılmıyor (`grep -n "ActionExecutor\|BotFairnessGuard\|HandlePacket" GameServer/Bot/ScriptRunner.cpp` boş).
- [ ] K10: örnek betik: `bots/config/script_smoke_2bot.txt` §5.5'teki metinle aynı, ASCII + CRLF (`file`), 7 adım.
- [ ] K11: kodlama: iki yeni kaynak dosya ve örnek betik `ASCII text, with CRLF line terminators` (`file`); `git diff --check` boş; `vcxproj`/`filters` farkı yalnızca eklenen satırlar (dört öğe).
- [ ] K12 (Claude, `/plan-dogrula` çalışma zamanı): §7 sonundaki senaryolar S1-S6 geçer. DeepSeek bu kriteri işaretlemez.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-20
grep -n "m_mgr.ExecuteCommand(" GameServer/Bot/ScriptRunner.cpp
grep -n '"SCRIPT_START"\|"SCRIPT_STEP"\|"SCRIPT_END"' GameServer/Bot/ScriptRunner.cpp
file GameServer/Bot/ScriptRunner.h GameServer/Bot/ScriptRunner.cpp bots/config/script_smoke_2bot.txt
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile (`ENABLED=1`, `TELEMETRY=decisions`, `SPAWN_ON_START` boş; her oturumdan sonra ini yedekten geri yüklenir; `Logs/bots/` test dosyaları önceden ayrılır; kapanış `CTRL_BREAK`, KI-010). `bots/config/script_smoke_2bot.txt` sunucu dizinine `Scripts/smoke.txt` olarak kopyalanır.

- S1: `spawn BotWP_K,BotWP_E` → ikisi `in game` → `script run smoke`: Bot günlüğünde `loaded (7 step(s), last offset 6000 ms)`, yedi `step i/7` satırı artan `+offset`, `late` değerleri ≥ 0 ve `TICK_MS` mertebesinde, sonda `finished smoke: completed, 7/7 step(s)`; `live-*.jsonl` içinde 1 `SCRIPT_START`, 7 `SCRIPT_STEP`, 1 `SCRIPT_END` (`steps_run=7`); `sit`/`stand` ardışık `ACTION_*` olayları üretir.
- S2: çalışırken ikinci `script run smoke` → `refused (already running (smoke))`; `script status` ilerlemeyi gösterir; `script stop` ortada → `stopped after k/7`, kalan adımlar çalışmaz, `SCRIPT_END result=stopped`.
- S3: reddedilen betikler: `spawn` verb'li adım (`Scripts/bad_verb.txt`) → `refused (bad_verb.txt:<satır>: bad verb)`, hiçbir adım çalışmaz, `SCRIPT_START` yazılmaz; olmayan dosya → `cannot open`; `script run ../x` → `bad script name`; 8193 baytlık dosya → `file too large`; ofset azalan dosya → `offsets must not decrease`.
- S4: adalet ölçümü: ardışık `sit`/`stand` 1000 ms'den kısa (`1000 sit`, `1200 stand`) → `FAIRNESS_REJECT` (`toggle`), betik yine `completed` (reddedilen adım betiği durdurmaz).
- S5: `TELEMETRY=summary` → `warning (telemetry below 'decisions' ...)` satırı, betik yine çalışır, `SCRIPT_*` olayı yazılmaz; `script` (argümansız) → usage satırı.
- S6: gerilemesiz: `ENABLED=0` → `Scripts/` okunmaz, log/dosya yok; `RESPAWN_CYCLES=2` iken `script run` → `cmd rejected (RESPAWN_CYCLES is active)`; `scenario run` ve diğer F4 komutları aynen; `PERF_SAMPLE` `tick_p95_us` F4-18 düzeyinde (betik koşarken artış gözlenirse not düşülür, kriter değil).

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3. Yeni kaynak dosyalar ASCII + CRLF, tab, Allman; yorumlar İngilizce. `vcxproj`/`BotManager.*` mevcut CRLF/girinti/BOM düzenini korur.
- **Thread kuralı (ADR-0005/0015):** `Command()` ve `Tick()` yalnızca IOCP thread'inde çalışır (`ExecuteCommand` ve `BotManager::Tick()` zaten orada); kilit eklenmez, yeni thread yok.
- **Zamanlama doğruluğu:** adım zamanı `TICK_MS` (varsayılan 100 ms) çözünürlüğündedir; adım en erken ofsetinde, bir sonraki turda çalışır, hiçbir zaman erken değil. `late_ms` bunu ölçer. Tick uzun sürerse (ör. 500 ms), o turda vadesi gelmiş **tüm** adımlar sırayla çalışır; sıra bozulmaz, adım atlanmaz.
- **`ExecuteCommand` ilk satırı** (`BotManager.cpp:591+`) komutu `BotManager: cmd '...'` olarak zaten günlüğe yazar; çalıştırıcının kendi `step` satırı ek bilgidir (adım no, ofset, gecikme), yineleme sayılmaz.
- **Reddedilen adım betiği durdurmaz** (ör. fairness reddi, bilinmeyen bot adı): her komutun kendi işleyicisi hatayı günlüğe yazar; betik devam eder. Bu kasıtlıdır (MET-FAIR-01 reddi ölçer).
- Bot sistemi kapalıyken (`ENABLED=0`) `Tick()` hiç çalışmaz, `Command()` hiç çağrılmaz: davranış değişmez.
- Bu plan sunucu davranışını yalnızca `script` komutu verildiğinde değiştirir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-20` (taban: `gece/2026-10-02`) — `ae00bde [F4-20] Betik calistirici: ScriptRunner (/bot script run|stop|status, Scripts/<ad>.txt, SCRIPT_* telemetrisi)`; plan raporu/Durum commit'i bu turun sonunda.
- Değişen dosyalar ve neden:
  - `GameServer/Bot/ScriptRunner.h` (yeni): plan §5.2'deki sınıf bildirimi aynen (`Command`, `Tick`, `LoadScript`, `CommandRun/Stop/Status`, `Finish`, üyeler).
  - `GameServer/Bot/ScriptRunner.cpp` (yeni): plan §5.3 aynen; dosya-statik `WriteScriptLog`, `IsSafeFileStem`, `SplitWords`; `SCRIPT_LOG_CMD_MAX=120`; yol yalnızca `"./Scripts/" + name + ".txt"`; üç `SCRIPT_*` `TEL_DECISIONS` olayı; adımlar tek noktadan `m_mgr.ExecuteCommand(...)`.
  - `GameServer/Bot/BotManager.h`: `#include "ScriptRunner.h"`, `friend class ScriptRunner;`, kurucu başlatıcısına `m_script(*this)`, üye `ScriptRunner m_script;` (§5.4).
  - `GameServer/Bot/BotManager.cpp`: `script` dalı, `Tick()` kancası, bilinmeyen komut listesine `script` (§5.4).
  - `GameServer/proj-GameServer.vcxproj` / `.filters`: `ScriptRunner.cpp`/`.h` öğeleri (§5.6).
  - `bots/config/script_smoke_2bot.txt` (yeni): §5.5'teki örnek betik.
- Derleme sonucu (`tools/build.sh Release`, `ScriptRunner.*`/`BotManager.*` `touch` edilip yeniden derlendi; yeni dosyalara atıf yapan uyarı 0; kalan uyarılar eski `GameServerDlg.cpp`/`UpgradeHandler.cpp` satırlarında):
  ```
  ScriptRunner.cpp
  BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  Kod üretiliyor
  proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  `rc=0`.
- Birim testler: `tools/run-tests.sh` son satırı `82 tests, 0 failed` (`Script_OffsetRules`, `Script_LineRules`, `Script_Limits`, `Script_ErrorLineNumbers` dahil).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ Release rc=0; yeni dosyalara atıf yapan `warning` 0.
  - K2 ✔ `82 tests, 0 failed`.
  - K3 ✔ `git diff --stat gece/2026-10-02...bot/F4-20` yalnızca §4'teki 7 dosyayı listeler; `BotCore/`, `Tests/`, `ScenarioRunner.*`, `ActionExecutor.*`, `BotSession.*`, `Telemetry.*`, `ChatHandler.cpp`, `docs/` yok.
  - K4 ✔ `BotManager.*` `-` satırları tam iki tane: kurucu başlatıcı satırı ve bilinmeyen-komut mesajı satırı; kalanı `+`.
  - K5 ✔ `Tick` ilk deyimi `if (!m_running) return;`; kurucu yalnızca üyeleri başlatır; ini grep'i boş; `fopen` tam iki satır (günlük + `LoadScript`).
  - K6 ✔ yol yalnızca `"./Scripts/" + name + ".txt"`, önce `IsSafeFileStem`; `fopen` ile `fclose` arasında `return` yok.
  - K7 ✔ grep yalnızca `snprintf` çağrıları + `WriteScriptLog` içindeki tek `fprintf`; `Sleep`/`thread`/`rand` yok.
  - K8 ✔ üç `Emit`, hepsi `TEL_DECISIONS`, `bot=-1`; alan adları plan ile aynı.
  - K9 ✔ `m_mgr.ExecuteCommand(` tek yerde (`:213`); `ActionExecutor`/`BotFairnessGuard`/`HandlePacket` grep'i boş.
  - K10 ✔ dosya §5.5 metniyle aynı, ASCII + CRLF, `ParseScript` ile bağımsız denetlendi: `error=0`, 7 adım, son ofset 6000.
  - K11 ✔ iki kaynak + betik `ASCII text, with CRLF line terminators`; `git diff --check` boş; `vcxproj`/`filters` farkı yalnızca eklenen dört öğe.
  - K12 (Claude): DeepSeek işaretlemez.
- Plandan sapmalar ve gerekçeleri: Yok. `std::max` yerine düz `if` karşılaştırması kullanıldı (`<algorithm>` planın include listesinde yok diye); `SCRIPT_LOG_CMD_MAX` birimi `static const size_t` (plan "Sabit" der; `ScenarioRunner.cpp`'deki sabitlerle aynı biçim).
- Açık sorular: Yok.


---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-20` @ `<sha>`
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
