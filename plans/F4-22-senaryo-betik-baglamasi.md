# F4-22: Betikli test dizisi, dilim 4 — senaryo–betik bağlaması: senaryo dosyasında `script:` anahtarı

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-22` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F3-03 (`ScenarioRunner`) — `KAPANDI`; F4-20 (`ScriptRunner`) — `KAPANDI` (merge `e36d9d1`); F4-21 (rapor aracı) — `KAPANDI` (merge `d803438`) |
| İlgili gereksinim / kabul | `docs/17` F4 "Görevler 6) Betikli test senaryoları" ve "Kabul: betikli dizilerde sunucuya giden geçersiz aksiyon ≤ %1"; ADR-0017 Eki F4-19 madde 3, Eki F4-20 madde 6 ("senaryo–betik bağlaması sonraki dilimin işidir"), Eki F4-22 |
| Tahmini büyüklük | S (3 kaynak dosya + 1 örnek senaryo; `BotManager.*` ve proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Bugün bir betikli test koşusu iki elle komut ister (`scenario run` ile botları aç, sonra `script run`); betik maç penceresinin dışında kalabilir ve maç bitince kendiliğinden durmaz. Bu plan senaryo dosyasına isteğe bağlı bir `script: <ad>` anahtarı ekler: senaryo dosyası yüklenirken betik **önceden doğrulanır** (ya-hep-ya-hiç), maç açıldığı anda `ScenarioRunner` betiği başlatır, maç kapanmadan önce (tamamlanma, durdurma, iptal, bot kaybı) hâlâ koşuyorsa durdurur. Böylece `SCRIPT_START … SCRIPT_END` olayları, ilgili `<match>.jsonl` dosyasının **içinde** ve `MATCH_START` ile `MATCH_END` arasında yer alır; F4-21'deki rapor aracı betik koşusunu maça bağlı üretir.

`script:` anahtarı olmayan senaryolar ve bot sistemi kapalıyken (`[BOT] ENABLED=0`, varsayılan) hiçbir şey değişmez.

## 2. Bağlam (okunması zorunlu)

- `plans/F4-20-betik-calistirici.md` §5.3 ve `GameServer/Bot/ScriptRunner.cpp`: `Command("run <ad>")` davranışı, `LoadScript`, `Finish`. Bu planda **yalnızca §5.1'deki küçük eklemeler** yapılır.
- `plans/F3-03-senaryo-kosucusu.md` ve `GameServer/Bot/ScenarioRunner.cpp`: durum makinesi (`STATE_PREPARE → RUNNING → CLEANUP`), `LoadScenario` YAML alt kümesi.
- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` Ek (F4-19) madde 3 (betik yaşam döngüsü komutlarını çağıramaz; birleştirme senaryonun işidir) ve Ek (F4-20) madde 6.
- İlgili kod (hepsini açıp doğrula; satırlar `d803438` itibarıyla):
  - `GameServer/Bot/ScenarioRunner.h:24-32` (`struct Scenario`), `:43-52` (üyeler), `:34-41` (özel yöntemler).
  - `GameServer/Bot/ScenarioRunner.cpp:160-163` (kurucu), `:165-476` (`LoadScenario`: `:181-182` `seen*` bayrakları, `:188` varsayılanlar, `:269-270` bilinen anahtarlar, `:428` son `else // duration_sec`, `:449` `fclose`, `:454-475` doğrulama ve `out` ataması), `:515-622` (`CommandRun`: `:527`/`:534` telemetri ve aktif maç kapıları, `:542-549` `LoadScenario`, `:615-619` "loaded" günlüğü), `:624-659` (`StartRun`), `:661-784` (`Tick`: `:687-707` PREPARE→RUNNING geçişi, `:723-731` bot kaybı, `:735-740` süre doldu), `:786-801` (`Abort`), `:815-836` (`Finish`).
  - `GameServer/Bot/ScriptRunner.h:16-20` (genel bölüm), `:22` (`private:`), `:23` (`LoadScript`), `:29-35` (üyeler); `ScriptRunner.cpp:62-65` (kurucu), `:129-174` (`CommandRun`; başarılı yolda `m_running = true` `:155`).
  - `GameServer/Bot/BotManager.h:50` `friend class ScenarioRunner;`, `:144-145` (`m_scenario`, `m_script`), `BotManager.cpp:429-430` (`Tick()` içinde önce `m_scenario.Tick`, sonra `m_script.Tick`).
  - `bots/config/scenario_smoke_2bot.yaml`, `bots/config/script_smoke_2bot.txt`: örnek dosyalar.

## 3. Kapsam

**Yapılacaklar**

1. `ScriptRunner`: `LoadScript` genel (`public static`) olur; `IsRunning()` ve `RunId()` eklenir (`RunId` her başarılı `run`'da bir artar; başlatan, kendi koşusunu başkasınınkinden ayırt etsin).
2. `ScenarioRunner`: `script: <ad>` anahtarı (isteğe bağlı), yükleme anında betik doğrulaması ve süre uyumu denetimi, maç açılınca betiği başlatma, maç kapanmadan önce kendi başlattığı betiği durdurma.
3. Örnek senaryo `bots/config/scenario_script_smoke_2bot.yaml`.

**Kapsam dışı (yapılmayacak)**

- `BotCore/*`, `Tests/*`, `BotManager.h/.cpp`, `ActionExecutor.*`, `BotSession.*`, `Telemetry.*`, `ChatHandler.cpp`, `User.h`, `proj-GameServer.vcxproj`/`.filters`, `tools/*`, `docs/` **değişmez** (yeni dosya yok; proje dosyası satırı gerekmez). Yeni birim testi yok (zamanlama/durum makinesi mantığı; doğrulama çalışma zamanında Claude'da, §6 K12).
- Betik içeriğinin senaryo botlarıyla uyumu (betikteki bot adlarının senaryo `bots` listesinde olması) **denetlenmez**; bilinmeyen/yanlış bot adı adımın kendi işleyicisinde reddedilir (betik sürer, F4-20 kuralı).
- Senaryo koşusu başına farklı betik, betikte seed/parametre, `script:` için birden çok ad: **yok** (tek ad, tüm koşular aynı betiği kullanır).
- `scenario status` çıktısına betik bilgisi, yeni telemetri olayı/alanı, `MATCH_START` alanı eklemek: **yok** (`script status` ve `SCRIPT_*` olayları yeterli).
- Yeni `GameServer.ini` anahtarı, yeni komut, yeni thread, `+bot` değişikliği: **yok**.
- Betik durdurulunca botların süren eylemlerini iptal etmek: **yok** (F4-20 ile aynı; operatör/betik `stop all` içerir).
- Örnek betik kütüphanesi ve T-MECH-SKILL bot yeniden koşusu: sonraki dilim (arena konumları ve envanter gerektirir).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/ScriptRunner.h` | değiştir | yalnızca §5.1 |
| `GameServer/Bot/ScriptRunner.cpp` | değiştir | yalnızca kurucuda `m_runId(0)` ve `CommandRun`'da `m_runId++` (2 satır) |
| `GameServer/Bot/ScenarioRunner.h` | değiştir | `Scenario::script`, `m_scriptRunId`, `StopScript()` |
| `GameServer/Bot/ScenarioRunner.cpp` | değiştir | §5.2-5.6 |
| `bots/config/scenario_script_smoke_2bot.yaml` | yeni | örnek senaryo |

Dosya sayısı 5 (plan dosyası dahil 6). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.0 Branch

`git switch -c bot/F4-22 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.1 `ScriptRunner` eklemeleri

`ScriptRunner.h`:

- `LoadScript` bildirimini `private:` bölümünden **genel** bölüme taşı (`public:` içinde, `Tick`'ten sonra; imza aynen).
- Genel bölüme ekle:

```cpp
	bool IsRunning() const { return m_running; }
	uint32 RunId() const { return m_runId; }   // +1 per successful "run"; 0 = none yet
```

- Üyelere ekle (`m_maxLateMs`'ten sonra): `uint32 m_runId;                            // see RunId()`

`ScriptRunner.cpp`: kurucu başlatıcı listesine `m_runId(0)` (üye bildirim sırasına uygun: en sonda); `CommandRun`'da `m_running = true;` satırından hemen önce `m_runId++;`. Başka değişiklik yok (`Finish`, `Tick`, günlük metinleri aynen).

### 5.2 `ScenarioRunner.h`

- `struct Scenario`'ya (`durationSec`'ten sonra) `std::string script;                // optional ./Scripts/<name>.txt, empty = none`.
- Özel yöntem: `void StopScript();             // stops the script this scenario started, if still running`.
- Üye (`m_sessions`'tan sonra): `uint32 m_scriptRunId;                 // ScriptRunner::RunId() of the script started by this scenario, 0 = none`.

### 5.3 `ScenarioRunner.cpp` — dosya başı ve kurucu

- `#include "ScriptRunner.h"` (`"BotSession.h"` satırının altına; `BotManager.h` zaten dahil ediyor, açık olması için).
- Sabit (`SCENARIO_CLEANUP_TIMEOUT_MS` satırının altına): `static const uint32 SCENARIO_SCRIPT_MARGIN_MS = 1000;   // script must end this long before duration_sec`.
- Kurucu başlatıcı listesine `, m_scriptRunId(0)` ekle (`m_completedRuns(0)`'dan sonra; üye bildirim sırasıyla uyumlu, çünkü `m_scriptRunId` `m_completedRuns`'tan sonra bildirilir).

### 5.4 `LoadScenario` (`script:` anahtarı)

1. `:181-182` civarına `bool seenScript = false;`; `:184` civarına `std::string script;` (varsayılan boş).
2. `:269-270` `known` ifadesine `|| key == "script"` ekle.
3. `else // duration_sec` dalından **önce** yeni dal (mevcut dalların kalıbıyla; hata metinleri `<name>.yaml:<satır>: ...` biçimli):

```cpp
		else if (key == "script")
		{
			if (seenScript)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": duplicate key 'script'";
				break;
			}
			seenScript = true;

			std::string parsed = Unquote(value);
			if (!IsSafeFileStem(parsed))
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": script: bad script name";
				break;
			}
			script = parsed;
		}
```

4. `fclose(fp)` ve `if (failed) return false;` sonrasında, `bots`/`seeds`/`runs` denetimlerinden **sonra**, `out.name = name;` atamalarından **önce**:

```cpp
	if (!script.empty())
	{
		std::vector<BotCore::ScriptStep> steps;
		std::string scriptError;
		if (!ScriptRunner::LoadScript(script, steps, scriptError))
		{
			error = "script: " + scriptError;
			return false;
		}

		if ((unsigned long long)steps.back().offsetMs + SCENARIO_SCRIPT_MARGIN_MS > (unsigned long long)durationSec * 1000ULL)
		{
			error = "script: last offset " + std::to_string(steps.back().offsetMs)
				+ " ms leaves less than " + std::to_string(SCENARIO_SCRIPT_MARGIN_MS)
				+ " ms before duration_sec (" + std::to_string(durationSec) + " s)";
			return false;
		}
	}
```

5. `out.durationSec = durationSec;` satırından sonra `out.script = script;`.

(`ScriptRunner::LoadScript` boş betiği `SCRIPT_ERR_EMPTY` ile reddeder, bu yüzden `steps.back()` güvenlidir; yine de `steps.empty()` ise `error = "script: no steps"` ile dön.)

### 5.5 `CommandRun`

- `LoadScenario` başarılı olduktan ve `bots.size() > poolSize` denetiminden **sonra**, oturum döngüsünden **önce** yeni kapı:

```cpp
	if (!loaded.script.empty() && m_mgr.m_script.IsRunning())
	{
		snprintf(message, sizeof(message),
			"ScenarioRunner: run %s: refused (a script is already running (use 'script stop'))", name.c_str());
		WriteScenarioLog(message);
		return;
	}
```

- Mevcut "loaded (id ..., N bot(s), ...)" günlük satırından **sonra**, `StartRun` çağrısından önce, `loaded.script` doluysa ek satır:
  `ScenarioRunner: run <ad>: script <betik> (every run, starts when the match opens)`.
  (Boşsa hiçbir ek satır yazılmaz: betiksiz senaryo günlüğü aynı kalır.)

### 5.6 Durum makinesi

- `StartRun` başında (`m_sessions.clear();` ile birlikte) `m_scriptRunId = 0;`.
- `Finish` sonunda (`m_abortReason.clear();` ile birlikte) `m_scriptRunId = 0;`.
- Yeni özel yöntem:

```cpp
void ScenarioRunner::StopScript()
{
	if (m_scriptRunId != 0 && m_mgr.m_script.IsRunning() && m_mgr.m_script.RunId() == m_scriptRunId)
		m_mgr.m_script.Command("stop");

	m_scriptRunId = 0;
}
```

- `Tick` PREPARE dalında, `if (!Telemetry::Instance().IsMatchActive()) { Abort("match start refused", now); return; }` bloğundan **sonra**, `m_state = STATE_RUNNING;` satırından **önce**:

```cpp
				if (!m_scenario.script.empty())
				{
					uint32 before = m_mgr.m_script.RunId();
					m_mgr.m_script.Command("run " + m_scenario.script);

					if (!m_mgr.m_script.IsRunning() || m_mgr.m_script.RunId() == before)
					{
						Abort("script start refused", now);
						return;
					}
					m_scriptRunId = m_mgr.m_script.RunId();
				}
```

  (`Abort` maçı `aborted` kapatır, botları despawn eder, `Finish` özetine `script start refused` yazar. `ScriptRunner::Command` kendi reddini kendi günlüğüne yazar.)
- `Tick` RUNNING dalı, bot kaybı bloğunda `m_mgr.CommandMatch("end bot_lost");` satırından **önce** `StopScript();`; süre doldu bloğunda `m_mgr.CommandMatch("end completed");` satırından **önce** `StopScript();`.
- `Abort`: `if (Telemetry::Instance().IsMatchActive()) m_mgr.CommandMatch("end aborted");` ifadesinden **önce** `StopScript();` (dış kaynaklı maç sonu, `scenario stop`, hazırlık zaman aşımı, bot kaybı yolları buradan geçer; `m_scriptRunId == 0` iken hiçbir şey yapmaz).

### 5.7 Örnek senaryo `bots/config/scenario_script_smoke_2bot.yaml` (ASCII + CRLF; içerik aynen)

```
# Smoke scenario with a script: two idle bots, one seed, 12 s match; the script runs inside the match.
# Copy this file to <server dir>/Scenarios/smoke_script.yaml and bots/config/script_smoke_2bot.txt
# to <server dir>/Scripts/smoke.txt, then: /bot scenario run smoke_script
scenario_id: smoke_script
zone: 71
bots: [BotWP_K, BotWP_E]
seeds: [7]
repeat: 1
duration_sec: 12
script: smoke
```

### 5.8 Derleme

```bash
./tools/build.sh Release
./tools/run-tests.sh                  # birim testler değişmedi: 82 tests, 0 failed
```

Sunucu çalıştırma ve çalışma zamanı denemesi DeepSeek'in işi **değildir** (§7 sonu).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; `ScriptRunner.cpp`, `ScenarioRunner.cpp` ve `BotManager.cpp` `touch` edilip yeniden derlenmiş günlükte bu dosyalara atıf yapan `warning` sayısı 0.
- [ ] K2: `./tools/run-tests.sh` son satırı `82 tests, 0 failed` (rc 0).
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F4-22` yalnızca §4'teki 5 dosyayı + plan dosyasını listeler; `BotCore/`, `Tests/`, `BotManager.*`, `ActionExecutor.*`, `BotSession.*`, `Telemetry.*`, `ChatHandler.cpp`, `User.h`, `*.vcxproj*`, `tools/`, `docs/` altında **hiçbir** dosya yok.
- [ ] K4: `ScriptRunner` farkı küçüktür: `git diff gece/2026-10-02...bot/F4-22 -- GameServer/Bot/ScriptRunner.cpp | grep '^[-+]' | grep -v '^[-+][-+]'` çıktısı en çok 3 satır (kurucu başlatıcı değişimi 1 `-` + 1 `+`, `m_runId++;` 1 `+`); `ScriptRunner.h`'de `LoadScript` artık `public:` altındadır (`grep -n "public:\|private:\|LoadScript" GameServer/Bot/ScriptRunner.h`: `LoadScript` `private:` satırından önce).
- [ ] K5: `script:` olmayan senaryoda etkisizlik: `grep -n "m_script\." GameServer/Bot/ScenarioRunner.cpp` tüm satırları `m_scenario.script.empty()`/`loaded.script.empty()` denetimi arkasında ya da `StopScript()` içinde (ve `StopScript` `m_scriptRunId != 0` ile korunur); `m_scriptRunId` yalnızca PREPARE dalında `RunId()` ile atanır.
- [ ] K6: yalnızca kendi başlattığı betiği durdurur: `StopScript` koşulu tam olarak `m_scriptRunId != 0 && m_mgr.m_script.IsRunning() && m_mgr.m_script.RunId() == m_scriptRunId`; `StopScript` çağrıları tam üç yerde (`Abort`, bot kaybı, süre doldu), her biri ilgili `CommandMatch("end ...")` çağrısından **önce**.
- [ ] K7: yükleme doğrulaması: `LoadScenario`, `ScriptRunner::LoadScript` ve süre uyumu denetimini `out` ataması **öncesinde** yapar (hata durumunda `out` değişmez); `script` anahtarı çift kullanılırsa `duplicate key 'script'`, güvensiz ad `script: bad script name`; `grep -n "SCENARIO_SCRIPT_MARGIN_MS" GameServer/Bot/ScenarioRunner.cpp` tanım + kullanım (en az 2 satır).
- [ ] K8: yasak sözcük denetimi: `grep -nE "Sleep|sleep\(|CreateThread|std::thread|rand\(" GameServer/Bot/ScenarioRunner.cpp GameServer/Bot/ScriptRunner.cpp` boş; `ScenarioRunner.cpp`'ye yeni `fopen` eklenmedi (`grep -c "fopen" GameServer/Bot/ScenarioRunner.cpp` = 2: günlük + `LoadScenario`, plan öncesiyle aynı).
- [ ] K9: komut yolu: betik yalnızca `m_mgr.m_script.Command("run ...")` ile başlatılır ve `Command("stop")` ile durdurulur; `ExecuteCommand`, `ActionExecutor`, `BotFairnessGuard`, `HandlePacket` `ScenarioRunner.cpp`'de **yeni** satırlarda geçmez (`git diff ... -- ScenarioRunner.cpp | grep '^+' | grep -c "ActionExecutor\|BotFairnessGuard\|HandlePacket\|ExecuteCommand"` = 0).
- [ ] K10: örnek senaryo: `bots/config/scenario_script_smoke_2bot.yaml` §5.7'deki metinle aynı, ASCII + CRLF (`file`); `script: smoke` ile `bots/config/script_smoke_2bot.txt` (son ofset 6000 ms) 12 s süreyle uyumlu (6000 + 1000 ≤ 12000).
- [ ] K11: kodlama: değişen/yeni kaynak dosyalar ve örnek senaryo `ASCII text, with CRLF line terminators` (`file`); `git diff --check` boş; mevcut girinti (tab), Allman ve CRLF korunmuş.
- [ ] K12 (Claude, `/plan-dogrula` çalışma zamanı): §7 sonundaki senaryolar S1-S7 geçer. DeepSeek bu kriteri işaretlemez.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-22
git diff gece/2026-10-02...bot/F4-22 -- GameServer/Bot/ScriptRunner.cpp | grep '^[-+]' | grep -v '^[-+][-+]'
grep -n "m_script\.\|StopScript\|m_scriptRunId" GameServer/Bot/ScenarioRunner.cpp
grep -n "public:\|private:\|LoadScript\|IsRunning\|RunId" GameServer/Bot/ScriptRunner.h
file GameServer/Bot/ScenarioRunner.h GameServer/Bot/ScenarioRunner.cpp GameServer/Bot/ScriptRunner.h GameServer/Bot/ScriptRunner.cpp bots/config/scenario_script_smoke_2bot.yaml
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile (`ENABLED=1`, `TELEMETRY=decisions`, `SPAWN_ON_START` boş; her oturumdan sonra ini yedekten geri yüklenir; `Logs/bots/` test dosyaları önceden ayrılır; kapanış `CTRL_BREAK`, KI-010; iş bitince `tools/run-servers.sh stop`). Örnek senaryo `Scenarios/smoke_script.yaml`, örnek betik `Scripts/smoke.txt` olarak kopyalanır; komutlar `BotCommands.txt` ile verilir.

- S1: `scenario run smoke_script` → günlükte `loaded (id smoke_script, 2 bot(s), 1 run(s), 12 s each)`, `script smoke (every run, ...)`, `match open`, sonra `ScriptRunner: run smoke: loaded (7 step(s), last offset 6000 ms)`, yedi `step i/7`, `finished smoke: completed, 7/7`, 12. sn'de `match end completed`, `scenario smoke_script finished: 1/1`. `<match>.jsonl` sırası: `MATCH_START`, `SCRIPT_START`, 7 `SCRIPT_STEP`, `SCRIPT_END` (`result=completed`), `MATCH_END`; `live-*.jsonl`'de maç penceresinde satır yok. `tools/bot-telemetry-report.py` bu dosyada Scripts satırı `completed 7/7` gösterir.
- S2: reddedilen senaryolar (hepsinde bot açılmaz, maç açılmaz): `script: nofile` → `refused (script: cannot open ./Scripts/nofile.txt)`; `Scripts/bad_verb.txt` (`spawn` adımı) → `script: bad_verb.txt:<satır>: bad verb`; son ofset 11500 ms'lik betik, `duration_sec: 12` → `script: last offset 11500 ms leaves less than 1000 ms before duration_sec (12 s)`; `script: ../x` → `script: bad script name`; iki `script:` satırı → `duplicate key 'script'`; `script:` (boş değer) → `script: bad script name`.
- S3: `script run longrun` (çalışırken) ardından `scenario run smoke_script` → `refused (a script is already running (use 'script stop'))`, bot/maç yok; `script stop` sonrası aynı komut çalışır.
- S4: `scenario stop` maçın ~3. saniyesinde → `<match>.jsonl`'de `SCRIPT_END result=stopped` (`steps_run` < 7) **`MATCH_END` (`aborted`) öncesinde**; botlar despawn olur; `script status` → `idle`; kalan adımlar çalışmaz.
- S5: bağımsızlık: `script:` anahtarsız `smoke.yaml` + önceden `script run longrun` (20 sn) → senaryo normal çalışır, maç bitince `longrun` **durdurulmaz** (`script status` hâlâ çalışıyor), `smoke.yaml` günlüğünde `script` satırı yok (F3-03 davranışı aynen).
- S6: `seeds: [7, 8]` (iki koşu) → her koşu kendi maç dosyasında bir `SCRIPT_START`/`SCRIPT_END` çifti; ikinci koşuda betik yeniden başlar (`RunId` artar).
- S7: gerilemesiz: `ENABLED=0` → `Scenarios/`/`Scripts/` okunmaz, log/dosya yok; `TELEMETRY=summary` ile `scenario run smoke_script` → betik yine çalışır, `ScriptRunner` uyarı satırı yazar, `SCRIPT_*` olayı yok (F4-20 davranışı); `script run smoke` tek başına ve `scenario run smoke` (betiksiz) F4-20/F3-03 gibi; `PERF_SAMPLE` `tick_p95_us` F4-20 düzeyinde (artış gözlenirse not düşülür, kriter değil).

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3. Yeni/değişen kaynak dosyalar ASCII + CRLF, tab, Allman; yorumlar İngilizce; `ScenarioRunner.cpp` `C source` olarak algılanıyor (normal), düzeni koru.
- **Thread kuralı (ADR-0005/0015):** `ScenarioRunner::Tick` ve `ScriptRunner::Command` aynı IOCP thread'inde çalışır (`BotManager::Tick()`); kilit eklenmez. `ScenarioRunner::Tick` betiği başlattığı turda `m_script.Tick` aynı turun sonunda çalışır (sıra: `m_scenario.Tick`, sonra `m_script.Tick`); ofset 0 adımı o turda çalışır.
- **Süre sözleşmesi `[A]`:** betik maç açıldığı ana göre ofsetle koşar; son ofset + 1000 ms ≤ `duration_sec × 1000` zorunludur (tick 1 sn'ye kadar uzasa bile son adım maç bitmeden çalışır). Yine de tick daha uzarsa betik `stopped` biter ve bu `SCRIPT_END`'de görünür (rapor aracı bunu gösterir); bu bir hata değil, ölçüm uyarısıdır.
- Betik, senaryo botlarının tamamı `in_game` olduktan sonra başlar (maç açılışıyla aynı an); betik adımları yaşam döngüsü komutu içeremez (ayrıştırıcı reddeder), bu yüzden senaryonun spawn/despawn sahipliği bozulmaz.
- Reddedilen betik adımı senaryoyu/betiği durdurmaz (F4-20); fairness/geçersiz aksiyon oranı F4-21 raporuyla ölçülür.
- Bu plan sunucu davranışını yalnızca senaryo dosyasında `script:` anahtarı bulunduğunda değiştirir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-22` (taban: `gece/2026-10-02`) — `73ce876 [F4-22] Senaryo-betik baglamasi: senaryoda 'script:' anahtari, mac acilinca betik baslar, kapanmadan once durur` (bu rapor/Durum commit'i ayrıca atılır)
- Değişen dosyalar ve neden:
  - `GameServer/Bot/ScriptRunner.h` — `LoadScript` `public:`'e taşındı, `IsRunning()`/`RunId()` eklendi, `m_runId` üyesi (§5.1).
  - `GameServer/Bot/ScriptRunner.cpp` — kurucuya `m_runId(0)` ve `CommandRun`'da `m_runId++` (§5.1).
  - `GameServer/Bot/ScenarioRunner.h` — `Scenario::script`, `StopScript()`, `m_scriptRunId` (§5.2).
  - `GameServer/Bot/ScenarioRunner.cpp` — `#include "ScriptRunner.h"`, `SCENARIO_SCRIPT_MARGIN_MS`, kurucu `m_scriptRunId(0)`, `LoadScenario` `script:` anahtarı + yükleme doğrulaması + süre denetimi, `CommandRun` "zaten betik çalışıyor" kapısı + bilgi satırı, `StartRun`/`Finish` sıfırlama, PREPARE'de betik başlatma, RUNNING bot-kaybı/süre-doldu ve `Abort` öncesi `StopScript()`, yeni `StopScript()` (§5.3-5.6).
  - `bots/config/scenario_script_smoke_2bot.yaml` — yeni örnek senaryo (§5.7).
- Derleme sonucu (`tools/build.sh Release` son satırları):
  ```
    All 14017 functions were compiled because no usable IPDB/IOBJ from previous compilation was found.
    Kodun üretilmesi tamamlandı
    proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  rc=0, `error C` 0. `ScriptRunner.cpp`/`ScenarioRunner.cpp`/`BotManager.cpp` `touch` edilip yeniden derlendi; derleme listesinde bu iki dosya görünür, bu dosyalara atıf yapan uyarı 0 (toplam 5 uyarının tümü eski `GameServerDlg.cpp:816/1143/1802` ve `UpgradeHandler.cpp:634/862` satırlarından).
- Birim testler: `tools/run-tests.sh` son satırı `82 tests, 0 failed` (rc=0).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (rc=0, hedef dosyalarda uyarı 0)
  - K2 ✔ (`82 tests, 0 failed`)
  - K3 ✔ (`git diff --stat gece/2026-10-02...bot/F4-22` yalnızca §4'teki 5 dosya; yasak yollarda fark boş; `docs/` ve `tools/` yok)
  - K4 ✔ (ScriptRunner.cpp'de tam 3 değişen satır; `LoadScript` `public:` altında, satır 21)
  - K5 ✔ (`m_script.` satırları yalnızca `loaded.script.empty()`/`m_scenario.script.empty()` denetimleri arkasında ya da `StopScript()` içinde; `m_scriptRunId` yalnızca PREPARE dalında `RunId()` ile atanır, satır 774)
  - K6 ✔ (`StopScript` koşulu birebir; çağrılar tam üç yerde — `Abort` (878), bot kaybı (806), süre doldu (817) — her biri ilgili `CommandMatch("end ...")` öncesinde)
  - K7 ✔ (yükleme doğrulaması `out` atamasından önce; `duplicate key 'script'`, `script: bad script name`, `script: no steps`; `SCENARIO_SCRIPT_MARGIN_MS` tanım+2 kullanım)
  - K8 ✔ (yasak sözcük grep'i boş; `fopen` sayısı 2, değişmedi)
  - K9 ✔ (yalnızca `m_script.Command("run ...")` ve `Command("stop")`; yeni satırlarda `ExecuteCommand`/`ActionExecutor`/`BotFairnessGuard`/`HandlePacket` yok)
  - K10 ✔ (örnek senaryo §5.7 metniyle aynı, ASCII+CRLF; son ofset 6000 ms, 6000+1000 ≤ 12000)
  - K11 ✔ (`file` beş dosyada `ASCII text, with CRLF line terminators`; `git diff --check` boş)
  - K12 — DeepSeek işaretlemez (Claude `/plan-dogrula` çalışma zamanı).
- Plandan sapmalar ve gerekçeleri: Yok. §5.4 adım 4'teki `steps.empty()` güvenlik dönüşü plan metnine uygun eklendi; `SCENARIO_SCRIPT_MARGIN_MS` `static const uint32`, senaryo dosyası arasındaki diğer sabitlerle aynı stilde.
- Açık sorular: Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-22` @ `4ac9de8` (kod commit'i `73ce876`); gece modu, `AUTO_LOOP=1`: birleştirmeyi döngü betiği yapar, push yok.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `./tools/build.sh Release` rc=0, `error C` 0; `ScriptRunner.cpp`/`ScenarioRunner.cpp`/`BotManager.cpp` `touch` ile yeniden derlendi (derleme listesinde üçü de görünür); yalnızca eski `GameServerDlg.cpp:816/1143/1802` uyarıları, bu dosyalara atıf yapan uyarı 0 |
| K2 | ✔ | `./tools/run-tests.sh` → `82 tests, 0 failed`, rc=0 |
| K3 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-22`: `ScenarioRunner.cpp/.h`, `ScriptRunner.cpp/.h`, `scenario_script_smoke_2bot.yaml` + plan dosyası; `BotCore/`, `Tests/`, `BotManager.*`, `ActionExecutor.*`, `BotSession.*`, `Telemetry.*`, `ChatHandler.cpp`, `User.h`, `*.vcxproj*`, `tools/`, `docs/` farkı boş |
| K4 | ✔ | `ScriptRunner.cpp` farkı tam 3 satır (`-` kurucu, `+` kurucu `m_runId(0)`, `+ m_runId++;` `:155` öncesi); `ScriptRunner.h`: `LoadScript` `:21`, `private:` `:25` |
| K5 | ✔ | `grep -n "m_script\."`: `:609` (`loaded.script.empty()` kapısı), `:766-769` (`m_scenario.script.empty()` bloğu içinde), `:888-889` (`StopScript`, `m_scriptRunId != 0` korumalı); `m_scriptRunId` yalnızca `:774`'te `RunId()` ile atanır (`:692`, `:891`, `:927` sıfırlama) |
| K6 | ✔ | `StopScript` koşulu `:888` birebir; çağrılar tam üç yerde: bot kaybı `:806`, süre doldu `:817`, `Abort` `:878`; üçü de ilgili `CommandMatch("end ...")` öncesinde. Çalışma zamanında S4 ile doğrulandı |
| K7 | ✔ | `LoadScript` + süre uyumu `:492-515`, `out.name = name;` (`:517`) öncesinde; `SCENARIO_SCRIPT_MARGIN_MS` tanım `:21` + 2 kullanım `:508,:511`; `duplicate key 'script'` ve `script: bad script name` çalışma zamanında görüldü (S2) |
| K8 | ✔ | yasak sözcük grep'i boş; `grep -c fopen ScenarioRunner.cpp` = 2 |
| K9 | ✔ | yeni satırlarda `ActionExecutor`/`BotFairnessGuard`/`HandlePacket`/`ExecuteCommand` sayısı 0; betik yalnızca `m_script.Command("run ...")`/`Command("stop")` ile yönetiliyor |
| K10 | ✔ | örnek senaryo §5.7 metniyle aynı (diff okundu), `ASCII text, with CRLF line terminators`; `script_smoke_2bot.txt` son ofset 6000 ms; çalışma zamanında `loaded (7 step(s), last offset 6000 ms)` ve 12 s süreyle kabul edildi |
| K11 | ✔ | `file`: beş dosyada `ASCII text, with CRLF line terminators`; `git diff --check` boş; tab/Allman korunmuş |
| K12 | ✔ | çalışma zamanı S1-S7 geçti (aşağıda) |

- Çalışma zamanı (`Release`, `GameServer.ini` değiştirilip yedekten geri yüklendi: md5 öncesi/sonrası `265a8e1c35ea12df46f6d006fe894d9b`; sunucular iş bitince `stop` → `0/3`; test senaryo/betik/`BotCommands.*` dosyaları silindi; `Logs/bots/2026-10-02/` altında test maç dosyaları kaldı). Komutlar `BotCommands.txt` ile, gözlem `Logs/Bot_2_10_2026.log`:
  1. **S1 ✔.** `scenario run smoke_script` → `loaded (id smoke_script, 2 bot(s), 1 run(s), 12 s each)`, `script smoke (every run, starts when the match opens)`, `match start`, `run smoke: loaded (7 step(s), last offset 6000 ms)`, `match open`, yedi `step i/7` (`late` 0..94 ms), `finished smoke: completed, 7/7 step(s) in 6043 ms`, `match end ... completed, 12093 ms`, `scenario smoke_script finished: 1/1`. `smoke_script-7-1.jsonl` sırası: `MATCH_START`, `SCRIPT_START`, 7 `SCRIPT_STEP`, `SCRIPT_END` (`completed`, `steps_run=7`), `MATCH_END`; `live-202212.jsonl`'de `SCRIPT_`/`MATCH_` satırı 0. `bot-telemetry-report.py` Scripts satırı: `smoke 7/7 completed 6043 ms`; MET-ACT-02 `PASS`.
  2. **S2 ✔.** Altı reddedilen senaryo, hiçbirinde bot/maç açılmadı: `script: cannot open ./Scripts/nofile.txt`; `script: bad_verb.txt:1: bad verb`; `script: last offset 11500 ms leaves less than 1000 ms before duration_sec (12 s)`; `f422_dotdot.yaml:10: script: bad script name`; boş değer `…:10: script: bad script name`; `f422_dup.yaml:11: duplicate key 'script'`.
  3. **S3 ✔.** `script run longrun` çalışırken `scenario run smoke_script` → `refused (a script is already running (use 'script stop'))`, bot/maç yok; `script stop` (`stopped after 12/21`) sonrası aynı komut normal çalıştı (`smoke_script-7-2`, 7/7).
  4. **S4 ✔.** `scenario stop` maçın ~2,8. saniyesinde: `ScriptRunner: stop: smoke: stopped after 3/7`, `match end ... aborted`, botlar despawn, `script status` → `idle`; `smoke_script-7-3.jsonl`: `SCRIPT_END result=stopped steps_run=3` `MATCH_END aborted` öncesinde; kalan adım çalışmadı.
  5. **S5 ✔.** Betiksiz senaryo (`f422_noscript`) + önceden `script run longrun`: senaryo normal bitti (`finished: 1/1`, günlükte `script` satırı yok), maç bittikten sonra `script status` → `longrun step 17/21 done` (durdurulmadı).
  6. **S6 ✔.** `seeds: [7, 8]`: `f422_two-7-5.jsonl` ve `f422_two-8-6.jsonl` her biri kendi `MATCH_START` → `SCRIPT_START` → `SCRIPT_END completed` → `MATCH_END`; ikinci koşuda betik yeniden `loaded`/`7/7`.
  7. **S7 ✔.** `TELEMETRY=summary`: `scenario run smoke_script` betiği çalıştırdı (`7/7`), `ScriptRunner` uyarı satırı yazıldı, maç dosyasında `SCRIPT_*` 0 (`MATCH_START/END` + 3 `PERF_SAMPLE`); betiksiz `scenario run` ve tek başına `script run smoke` F3-03/F4-20 gibi; `PERF_SAMPLE` `tick_p95_us` 433/1099 (decisions koşusunda ~1326 tahmini; artış yok). `ENABLED=0`: `scenario run` komutu dosyası dokunulmadan kaldı, `Bot_*.log` ve `Logs/bots/` dosya sayısı değişmedi.

- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. Not: `Abort` yolunda maç dışarıdan kapanmışsa (`match ended externally`) `StopScript()` maç bitiminden sonra çalışır; `SCRIPT_END` o durumda `MATCH_END`'den sonra `live-*.jsonl`'ye düşer. Plan §5.6 bu yolu açıkça `Abort`'tan geçirdi; beklenen davranış, ölçülmedi (dış `match end` ile aynı anda betik koşusu nadir).
  2. Not: `bot lost` yolunda `StopScript()` iki kez çağrılır (`:806` ve `Abort` içinde `:878`); ikincisi `m_scriptRunId == 0` olduğundan etkisiz, planın tarifi birebir.
  3. Not: Uygulayıcı sapma bildirmedi; `steps.empty()` güvenlik dalı plan metninde isteniyordu. Yeni birim testi planlanmadı (durum makinesi çalışma zamanında sınandı).
- Düzeltme talimatı: gerekmiyor.
