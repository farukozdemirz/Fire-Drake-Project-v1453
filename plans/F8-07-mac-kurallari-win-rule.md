# F8-07: Maç kuralları ayrımı — `win_rule` (`killdiff_timed` / `timed_score` süreli + respawn'lı ve `wipe_first` ilk tam yok oluşla biten, respawn kapalı), eşikler, beraberlik/geçersiz sonuç ve `SETUP_FAIL`'in sayıma katılmaması (ADR-0031-DEG)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F8 — Değerlendirme ve 8v8 (`docs/17` §2; kapı G8; F6 `EVAL-1v1` ve F7 `EVAL-2v2..5v5` için ön koşul) |
| Branch | `bot/F8-07 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F8-06** (`MatchEvents` sayaçları, takım anahtarları, `BeginScenarioMatch`/`EndScenarioMatch`; `KAPANDI` olmalı), **F8-05** (`SETUP_FAIL` ve koşu sayımı; `KAPANDI` olmalı), F8-01 `KAPANDI` (oracle), F4-07 `KAPANDI` (`ActionExecutor::RequestRegene`), F3-03 `KAPANDI`. Pilot (K11) ayrıca `baseline-v1` (F6/F8 sonrası) ister |
| İlgili gereksinim / kabul | ADR-0031-DEG (tamamı + Ek F8-01 madde 3-5), `docs/15` §6b (satır 293-315), MET-OUT-01/02/03/05, T-IGT-EVAL-01 (geçersiz maç ≤ %10, beraberlik raporu), CLI-14 (doğuş ≥ 3 sn), KI-013 (NP), KI-DEG-05 (b parçası) |
| Tahmini büyüklük | M (6 dosya + 2 `vcxproj`) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak; referanslar `gece/2026-10-02` @ `7891f74`) |

---

## 0. Neden TASLAK (HAZIR yapma ön koşulları)

1. **F8-06 ve F8-05 `KAPANDI` olmalı:** kural motoru F8-06'nın sayaçlarını (`killsA/B`, `aliveA/B`, `engageMs`, `respawns`) ve `team_a/team_b` tanımını tüketir; `SETUP_FAIL` sayımı F8-05'in akışına bağlanır.
2. **Karar B1 — `MATCH_END` sonuç alanı (çelişki, proje sahibine).** ADR-0031-DEG: `MATCH_END.result ∈ win_a | win_b | draw | invalid | no_result`, teknik sonlanma (`completed`/`aborted`) **ayrı alan**. Oysa `tools/bot-outcome-eval.py` ve `docs/16` §3.3 (satır 77) `MATCH_END.result == "aborted"` değerini ABORTED geçersizliği sayar (`bot-outcome-eval.py:544-545`). Önerim: sunucu **ikisini birlikte yazar**: tamamlanan maçta `result` = ADR kodu; teknik olarak kesilen maçta `result = "aborted"` (araç sözleşmesine uyum) **ve** her durumda `end_state ∈ completed|aborted|bot_lost|stopped|setup_fail` + `reason` listesi. Böylece araç değişmez. Alternatif (sunucu yalnızca ADR kodunu yazar, araç `end_state`'i okur) `tools/bot-outcome-eval.py` değişikliği ister. Seçim ADR-0031-DEG'e Ek F8-07 olarak yazılır (Claude), `docs/16` §3.3 satır 75-77 güncellenir.
3. **Karar B2 — zaman başlangıcı farkı.** ADR "süre `engage` anından işler"; `ScenarioRunner` bugün süreyi maç açılışından sayar (`ScenarioRunner.cpp:815` `elapsed >= durationSec * 1000`, `m_matchSince` `:778, :814`). Araç `window_end = engage + duration` ister (`bot-outcome-eval.py:535` `window_end`): sunucu maçı `start + duration`'da bitirirse araç TRUNCATED döndürür. Bu plan süreyi **engage'den** sayar (ADR); karar teyit edilir.
4. **Karar B3 — respawn sürücüsü.** `killdiff_timed` "respawn açık"tır, ama bugün bot doğuşunu yalnızca `/bot regene` komutu ve betik tetikler (`BotManager.cpp:1889` `ActionExecutor::RequestRegene`); karar katmanı (F6) yoktur. Bu plan `ScenarioRunner`'a **geçici bir otomatik respawn sürücüsü** koyar (ölü bot için `RequestRegene`, CLI-14 koruması içeride); F6 `Brain` gelince sürücü kapatılır ve karar Brain'e geçer (`respawn_driver: scenario|brain` anahtarı F6 planında). Proje sahibi: geçici sürücü kabul mü?
5. **Karar B4 — seviye denetimi.** Skorlu kural (`killdiff_timed`, `wipe_first`) oracle çapraz denetimi için `DAMAGE/DEATH/RESPAWN` olaylarını ister; bunlar `decisions` seviyesindedir (`docs/16` §3.3 satır 69) ve varsayılan `TELEMETRY=summary`'de yazılmaz. Öneri: skorlu kural `TEL_DECISIONS` yoksa koşuyu **reddeder** (`refused (telemetry level too low for win_rule)`); `timed_score` reddetmez (yalnızca uyarı). Sunucu kararı olay kaybına bağışık olması için `MatchEvents` sayaçlarından verilir (F8-06 §3.2).
6. **Pilot (K11) `baseline-v1` ister:** ADR "baseline-v1 aynı-aynıya 20 maç, taraf değişimli, hedef beraberlik %15-35" der; `baseline-v1` F6/F8 sonrası vardır. Pilot protokolü bu planda **yazılı**, çalıştırılması ayrı madde (`BEKLİYOR`) ve ayrı insan/Claude oturumudur; birim test ve kısa çalışma zamanı doğrulaması pilot yerine **geçmez**.
7. **`TEST_TELEPORT` (F8-06 açık soru A2)** çözülene kadar geçersizlik nedeni olarak sunucuda üretilmez; araçta kalır.

## 1. Amaç

`ScenarioRunner`'a ADR-0031-DEG'in üç senaryo türünü (`killdiff_timed`, `wipe_first`, `timed_score`) **ayrı kurallar** olarak ekler: `win_rule` anahtarı ve eşikleri (`win_margin`, `duration_sec`, `early_end_margin`, `engage_timeout_sec`, `respawn`, `resurrection`) senaryo dosyasından okunur (kodda sabit değil), `MATCH_START`'a yazılır, maç kuralın bitiş koşulunda sonlanır ve `MATCH_END` ADR sonuç koduyla (`win_a`/`win_b`/`draw`/`invalid`/`no_result`) kapanır. `SETUP_FAIL` ve teknik sonlanmış koşular geçerli maç/kazanma sayımına katılmaz, ayrı sayılır. Sunucu sonucu `tools/bot-outcome-eval.py` oracle'ı ile **aynı** çıkmalıdır (iki gerçekleme birbirini denetler, ADR Ek F8-01 madde 1).

## 2. Bağlam (okunması zorunlu)

- ADR-0031-DEG: "Ortak tanımlar" (satır 15-20), "Tür 1" tablosu ve örnekler (satır 22-47), "Tür 2" (satır 49-60), "Tür 3" (satır 62-64), "Yapılandırılabilirlik ve pilot" (satır 66-68), "Doğrulama" (satır 83-85), Ek F8-01 madde 3-5 (satır 87-): sınırlar dahil, varsayılan süreler, `wipe_first` "aynı tick" = 100 ms, geçersizlik öncelik sırası `SETUP_FAIL` > `ABORTED` > `TEST_TELEPORT` > `NO_ENGAGE` > `RESPAWN_IN_WIPE_FIRST` > `TRUNCATED`.
- `docs/15` §6b (satır 293-315): `win_rule` tablosu, `killdiff_timed` ve `wipe_first` örnekleri, pilot kalibrasyonu; `docs/16` MET-OUT-01..05 (satır 211-215), §7 (satır 244-251).
- `tools/bot-outcome-eval.py` (oracle; okunacak): `resolve_*` `:227-283` (varsayılanlar ve aralıklar), `judge_killdiff` `:285`, `eval_killdiff` `:312`, `eval_wipe` `:348-441`, `evaluate_match` `:476-583`, `REASON_ORDER`; `--selftest` vakaları ADR tablolarının makine karşılığıdır.
- `GameServer/Bot/ScenarioRunner.cpp`: bilinen anahtarlar `:273-274`; `durationSec` varsayılan 30 `:192`; `duration_sec` dalı `:451-466` (1..3600); `STATE_RUNNING` `:~773-822` (`bot_lost` `:807`, süre dolumu `:815-819` `"end completed"`), `Abort` `:867-884` (`"end aborted"` `:881`), `Finish` `:906-928`, `CommandStatus` `:949-`.
- `GameServer/Bot/ActionExecutor.cpp:1895` `RequestRegene` (tek seferlik `WIZ_REGENE` tip 1; reddi: `no_np` `:1918-1919`, CLI-14 3 sn `BotCore/BotCombat.h:749` `kRegeneMinDeadMs`); `BotManager.cpp:1889` komut çağrısı.
- F8-06 çıktıları (plan): `MatchEvents::GetCounters()`, `BeginScenarioMatch`/`EndScenarioMatch`, `team_a`/`team_b`.

## 3. Kapsam

### 3.1 S1 — `BotCore/ScenarioRules.h` (yeni, saf mantık) ve testleri

- `enum WinRule { KILLDIFF_TIMED, WIPE_FIRST, TIMED_SCORE }`, `struct RuleParams { rule, durationSec, winMargin, earlyEndMargin, engageTimeoutSec, respawn, resurrection, tickMs }`.
- `ValidateRuleParams(params, sizeA, sizeB, error)`: ADR aralıkları: `win_margin` 1..8; `early_end_margin` 0 ya da ≥ `win_margin`; `duration_sec` ve `engage_timeout_sec` 1..3600 (Ek F8-01 madde 3e); `wipe_first` ile `respawn=on` **reddedilir** (ADR "doğrulama reddeder"); `wipe_first` için `resurrection` varsayılan off.
- `DefaultDurationSec(rule, teamSize)`: `killdiff_timed` takım boyutu ≥ 6 ise 300 değilse 120; `wipe_first` ve `timed_score` 300 (Ek F8-01 madde 3c `[A]`). **`win_rule` anahtarsızsa varsayılan `timed_score` ve `duration_sec` varsayılanı bugünkü 30 kalır** (geriye uyumluluk; `[A]`).
- `class RuleEngine`: `Begin(params, startMs, sizeA, sizeB)`, `OnEngage(tMs)`, `OnKill(side, tMs)`, `OnDeath(side, tMs, aliveA, aliveB)`, `OnRespawn(side, tMs)`, `Poll(nowMs) -> Verdict { ended, result, reasonMask, end }`. Kurallar (ADR/Ek F8-01 ile **aynı**): süre `engage`'den işler; `engage_timeout_sec` içinde hasar yoksa `invalid NO_ENGAGE` (sınır: `engage − start == timeout` geçerli); `killdiff_timed`: `fark ≥ margin` `win_a`, `≤ −margin` `win_b`, aksi `draw`, `early_end_margin > 0 && |fark| ≥ early_end_margin` erken biter; sınırlar dahil (`fark == margin` galibiyet, `t == engage + duration` sayılır); `wipe_first`: bir takım 0 canlı ⇒ diğeri galip (anında); iki takım `tickMs` içinde 0 canlı ⇒ `draw` (karar `t0 + tickMs` dolunca verilir); süre dolunca fazla canlı galip, eşitse `draw`; 1v1'de ilk ölen kaybeder, yalnızca **aynı `t`** beraberlik; `wipe_first`'te `OnRespawn` gelirse `invalid RESPAWN_IN_WIPE_FIRST`; `timed_score`: `no_result`.
- `Tests/BotCoreTests/ScenarioRulesTests.cpp` (yeni): ADR-0031-DEG ve `docs/15` §6b tablolarının **tamamı** test vektörü: `killdiff_timed` (14,11) `win_a`; (10,8) `win_a`; (9,8) `draw`; (8,8) `draw`; (8,9) `draw`; (7,9) `win_b`; (3,12) `win_b`; (0,0)+hasar `draw`, hasarsız `invalid NO_ENGAGE`; `win_margin = 3` ile (14,11) `win_a`, (10,8) ve (7,9) `draw`; 2v2 120 sn (3,1) `win_a`, (2,1) `draw`; erken bitiş; sınır vakaları (`t == engage + duration`; `engage − start == timeout`); `wipe_first` 8v8: B'nin son botu ölür A'da 3 canlı `win_a`; süre dolar A4/B2 `win_a`; A3/B3 `draw`; aynı tick'te son ölümler `draw`; 1v1 ilk ölen kaybeder, aynı `t` `draw`; `wipe_first` + respawn `invalid`; `timed_score` `no_result`; `ValidateRuleParams` hata vakaları; `DefaultDurationSec`.

**Beraberlik ve geçersiz sonuç tanımı (kural motoru ve sayım için tek kaynak; ADR-0031-DEG + Ek F8-01 madde 4):**

| Sonuç | Ne zaman | Kazanma oranına etkisi (MET-OUT-01) |
|---|---|---|
| `win_a` / `win_b` | `killdiff_timed`: `fark >= win_margin` / `<= -win_margin`; `wipe_first`: rakip takım tam ölü ya da süre sonunda canlı sayısı fazla | 1 / 0 |
| `draw` | `killdiff_timed`: `\|fark\| < win_margin` (hasar vardı); `wipe_first`: iki takım `tick_ms` içinde 0 canlı, süre sonunda canlı eşit, 1v1'de aynı `t`'de ölüm | 0,5 |
| `no_result` | yalnızca `timed_score` | paya girmez |
| `invalid` + neden | `NO_ENGAGE` (sunucu), `RESPAWN_IN_WIPE_FIRST` (sunucu), `ABORTED` (sunucu, bkz. karar B1), `SETUP_FAIL` (F8-05; **ayrı sayaç**), `TEST_TELEPORT` (araçta; sunucu emisyonu yok, A2), `TRUNCATED` (yalnızca araç; sunucu üretmez) | paydadan **dışlanır**; `SETUP_FAIL` ayrıca `completed` ve `invalid` sayısına katılmaz |

Öncelik: `SETUP_FAIL` > `ABORTED` > `TEST_TELEPORT` > `NO_ENGAGE` > `RESPAWN_IN_WIPE_FIRST` > `TRUNCATED`; birden çok neden varsa `reason` listesinde bu sırayla yazılır.

### 3.2 S2 — `ScenarioRunner` (`.h/.cpp`)

- **Yeni anahtarlar** (`:273-274` listesine; tekrar anahtarı hatası ve aralık hatası biçimi mevcut kalıpla): `win_rule` (`killdiff_timed` | `wipe_first` | `timed_score`; yoksa `timed_score`), `win_margin` (1..8, varsayılan 2), `early_end_margin` (varsayılan 0), `engage_timeout_sec` (1..3600, varsayılan 60), `respawn` (`on`/`off`; varsayılan `killdiff_timed` → on, `wipe_first` → off), `resurrection` (`on`/`off`; varsayılan off). `duration_sec` verilmişse o, verilmemişse `DefaultDurationSec` (yalnızca `win_rule` açıkça verilmişse; aksi halde 30). `LoadScenario` sonunda `ValidateRuleParams`; hata `refused (<neden>)`.
- **`MATCH_START` alanları** (F8-06 `BeginScenarioMatch` `extraFields`): `win_rule`, `duration_sec`, `win_margin`, `early_end_margin`, `engage_timeout_sec`, `respawn`, `resurrection`, `tick_ms` (F8-06'daki `timed_score` yer tutucusu gerçek değerle değişir).
- **Koşu döngüsü (`STATE_RUNNING`):** bugünkü `elapsed >= durationSec` denetimi, her tick `MatchEvents::GetCounters()` + `RuleEngine::Poll` ile **değişir**: karar `ended` olunca `EndScenarioMatch` (`result`, `end_state = completed`, `reason`, `k_a`, `k_b`, `alive_a`, `alive_b`, `engage_ms`, `win_rule`) sonra mevcut `BeginCleanup`. Bot kaybı ve `stop` komutu **teknik sonlanma** olarak `result = aborted` (karar B1) ile ve `end_state` ayrımıyla kapanır (`ScenarioRunner.cpp:807, 881` çağrıları `bot_lost`/`aborted` serbest belirteç yerine). Üst sınır (takılma koruması): `engage_timeout_sec + duration_sec + kSlackMs` sonra `end_state = aborted`.
- **Respawn sürücüsü (karar B3, yalnızca `respawn: on`):** her tick, `m_sessions` içinde ölü (`m_pUser->isDead()`) botlar için `ActionExecutor::RequestRegene(s, now)`; sonuç `REFUSED` (CLI-14 bekleme, `no_np`) sessizce yeniden denenir, `no_np` bir kez `Bot_*.log`'a yazılır. `respawn: off` iken sürücü çalışmaz ve ölü bot ölü kalır. (Mevcut `/bot regene` komutu ve betik yolu değişmez.)
- **Seviye denetimi (karar B4):** skorlu kuralda `Telemetry::IsEnabled(TEL_DECISIONS)` yoksa `CommandRun` reddi.
- **Sayım ve özet:** koşu sonuçları kodlara göre sayılır: `Finish` özeti `completed=<n> win_a=<n> win_b=<n> draw=<n> invalid=<n> no_result=<n> setup_fail=<n> aborted=<n>`; `SETUP_FAIL` (F8-05) `completed`'a ve `win_a/win_b/draw`'a **girmez**, `invalid` dalının içinde de sayılmaz (ayrı `setup_fail`); `invalid` ve `setup_fail` kazanma oranı paydasından dışlanır (MET-OUT-01: bu hesap araçtadır; sunucu yalnızca ayrı sayaç yazar). `CommandStatus` aynı sayaçları gösterir.
- `ScenarioRunner.h`: yeni üyeler (`RuleParams m_rule`, `RuleEngine m_engine`, sayaçlar).

### 3.3 Kapsam dışı (yapılmayacak)

- Pilot **koşusu** ve eşik kalibrasyonu (`win_margin` ayarı ADR Eki, kod değişmez); `baseline-v1`, OP-* profilleri, SPRT/Wilson/bootstrap toplulaştırması, taraf değişimi otomasyonu (`start_a`/`start_b` ile iki senaryo dosyası ya da F8 harness planı).
- `TEST_TELEPORT` emisyonu (F8-06 A2), `THIRD_PARTY` (şema yok), `RESURRECT` ayrı olayı, bot karar katmanı (F6), `Brain` respawn kararı.
- `tools/bot-outcome-eval.py`, `docs/*`, `ActionExecutor`, `BotManager` değişikliği (sürücü `RequestRegene`'yi olduğu gibi çağırır; oyun mekaniği ve CLI-14 koruması değişmez).
- Maç içi herhangi bir DB yazımı/ışınlama (ADR-0032 madde 1).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/ScenarioRules.h` | yeni | saf; ASCII, CRLF |
| `Tests/BotCoreTests/ScenarioRulesTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | birer satır |
| `GameServer/Bot/ScenarioRunner.h` / `.cpp` | değiştir | anahtarlar, motor, sürücü, sayım |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`BotManager`/`MatchEvents` F8-06'da hazır varsayılır; ek arayüz gerekirse dur ve sor.)

## 5. Uygulama adımları

1. `git switch -c bot/F8-07 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Taban test sayısını kaydet.
2. S1: `ScenarioRules.h` + testler + `vcxproj`; ADR/`docs/15` vektörlerinin tamamı yeşil; Commit 1.
3. S2: anahtarlar + doğrulama; `MATCH_START` alanları; motor entegrasyonu (süre engage'den); respawn sürücüsü; sayaçlar/özet; seviye denetimi; Commit 2.
4. Geriye uyumluluk elle: `win_rule` anahtarsız eski senaryo aynı davranış (30 sn varsayılan, `timed_score`, `no_result`).
5. `./tools/build.sh Release` (+ Debug); `./tools/run-tests.sh`.
6. Uygulayıcı Raporu; `Durum` → `UYGULANDI`. Çalışma zamanı kanıtları (K6-K10) ve pilot Claude'undur.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; `./tools/run-tests.sh` `0 failed`; test sayısı = taban + yeni vakalar (ikisi rapora)
- [ ] K2: `git diff --stat gece/2026-10-02...bot/F8-07` yalnızca §4'teki dosyalar (+ plan); `git diff --check` boş; `tools/`, `docs/`, `ActionExecutor.*`, `BotManager.*` farkı 0
- [ ] K3 (birim, ADR vektörleri): `ScenarioRulesTests` §3.1'deki **tüm** ADR-0031-DEG/`docs/15` §6b vakalarını içerir ve geçer (her vaka adı rapora tablo olarak; ADR tablosundaki satır sayısı ile test vakası eşlemesi); `./tools/run-tests.sh` `0 failed`
- [ ] K4 (anahtar doğrulama): geçersiz senaryolar `ScenarioRunner: run <ad>: refused (...)` satırıyla reddedilir: `win_rule: bogus`; `win_margin: 0` ve `9`; `early_end_margin: 1` (`win_margin: 2` iken); `duration_sec: 0` ve `3601`; `win_rule: wipe_first` + `respawn: on`; `team` ile uyumsuz boş takım; skorlu kural + `TELEMETRY=summary` (log metni rapora)
- [ ] K5 (geriye uyumluluk): `win_rule` anahtarsız F3-03 senaryosu önceki log satırlarıyla ve `duration_sec` varsayılanı 30 ile çalışır; `MATCH_END.result` `no_result`, `end_state` `completed`
- [ ] K6 (çalışma zamanı, `killdiff_timed`; Claude): 2v2, `respawn: on`, `duration_sec: 120`, `win_margin: 2`, `TELEMETRY=decisions`: maç `engage + 120 sn`de biter (`engage_ms` + `duration_ms` tutarlı), `MATCH_END.result` ∈ {win_a, win_b, draw}; **sunucu sonucu = `python3 tools/bot-outcome-eval.py <jsonl>` `result`** (aynı `k_a/k_b`, `rule=killdiff_timed`, `reasons=-`); ölen bot ≥ 3 sn sonra `RESPAWN` yazar (sürücü, CLI-14)
- [ ] K7 (çalışma zamanı, `wipe_first`): 1v1 (`respawn: off`): bir bot ölünce maç **anında** biter (`end_ms` ≪ `duration_sec`), `result = win_a|win_b`, `RESPAWN` satırı **yok**; sunucu = oracle; ikinci koşu: iki bot aynı tick'te ölemiyorsa `draw` doğrulaması yalnızca birim testtedir (çalışma zamanında zorlanamaz: rapora yaz)
- [ ] K8 (çalışma zamanı, geçersiz sonuç): saldırmayan iki bot, `engage_timeout_sec: 20`: `result = invalid`, `reason` `NO_ENGAGE`, oracle aynı; `/bot scenario stop` ile kesilen maç `result = aborted`, `end_state = aborted` ve oracle `ABORTED` (karar B1 sonrası biçimde)
- [ ] K9 (sayım): F8-05'in `SETUP_FAIL` koşusunu içeren 3 koşuluk senaryoda (`setup_fail=1`, `win_*`/`draw` toplamı 2) `Finish` özeti ve `CommandStatus` sayaçları tutarlı; `completed` `SETUP_FAIL`'i saymaz
- [ ] K10 (`ENABLED=0` ve geri alma): `[BOT] ENABLED=0` davranışı değişmedi; `win_rule: timed_score` her zaman eski davranışa (kural yok, `no_result`) geri alma yoludur
- [ ] K11 (**pilot, ayrı madde; `BEKLİYOR` olabilir**): `baseline-v1` aynı-aynıya 8v8, 20 maç = 10 seed × 2 taraf (iki senaryo dosyası: `start_a`/`start_b` yer değiştirmiş), `win_rule: killdiff_timed`, 300 sn, `win_margin: 2`; rapor: `win_a/win_b/draw/invalid` sayıları, `fark` ortalaması ve standart sapması (`bot-outcome-eval.py --json`), beraberlik oranı `draw / (win_a + win_b + draw)`; **hedef %15-35**; `invalid ≤ %10` (T-IGT-EVAL-01); dışındaysa `win_margin` değişikliği ADR Eki olarak yazılır (kod değişmez). `baseline-v1` yoksa bu madde `BEKLİYOR` yazılır ve plan K1-K10 ile `DOĞRULANDI` olabilir; pilot geçmeden KI-DEG-05 (b) ve T-IGT-EVAL-01 `KAPANDI` olmaz
- [ ] K12: Uygulayıcı Raporu dürüst: çalıştırılmayan çalışma zamanı ve pilot maddeleri "yapılmadı" yazılmış

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/run-tests.sh 2>&1 | tail -3
git diff --stat gece/2026-10-02...bot/F8-07
git diff --check gece/2026-10-02...bot/F8-07
# Claude, calisma zamani (sunucu acik, [BOT] ENABLED=1, TELEMETRY=decisions)
#   /bot scenario run f8-07-killdiff-2v2 ; /bot scenario run f8-07-wipe-1v1 ; /bot scenario run f8-07-noengage
python3 tools/bot-outcome-eval.py Logs/bots/<tarih>/ --json | python3 -m json.tool | head -60
python3 tools/bot-outcome-eval.py Logs/bots/<tarih>/<match>.jsonl; echo rc=$?
# sunucu sonucu: jsonl icindeki MATCH_END satiri
grep -a '"ev":"MATCH_END"' Logs/bots/<tarih>/<match>.jsonl
# pilot (K11): iki senaryo dosyasi x 10 seed; sonuc ozeti
python3 tools/bot-outcome-eval.py Logs/bots/<tarih>/ --json | python3 -c "import json,sys; s=json.load(sys.stdin)['summary']; n=s['win_a']+s['win_b']+s['draw']; print(s, 'draw_rate=%.2f'%(s['draw']/n if n else 0))"
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları (yeni dosyalar ASCII + CRLF; yorumlar İngilizce).
- **Oyun mekaniği değişmez:** respawn sürücüsü yalnızca mevcut `RequestRegene`'yi (CLI-14 korumalı, gerçek `WIZ_REGENE` paketi) çağırır; bota avantaj yok (`docs/03` §13, `docs/13` §3); sürücü insanın "doğ" düğmesine basmasının eşdeğeridir. `respawn: off` yalnızca **doğuş isteği göndermemek**tir, sunucu mekaniğini kapatmaz.
- **Thread kuralı:** `Tick` ve sürücü IOCP thread'inde; sayaçlar `MatchEvents`'in kilitli kopyasından okunur (F8-06).
- **Kurallar uydurulmaz:** ADR-0031-DEG ve Ek F8-01'de `[A]` işaretli olanlar (varsayılan süreler, sınırlar dahil, aynı tick 100 ms, öncelik sırası) dışında yeni kural eklenmez; ADR ile oracle (`bot-outcome-eval.py`) çelişirse **durup** raporda soru olarak yaz; ikisinden birini sessizce değiştirme.
- **Eşikler kodda sabit değildir** (yalnızca varsayılan): senaryo anahtarıdır ve `MATCH_START`'a yazılır (tekrar edilebilirlik, `docs/15` §6b).
- NP (KI-013): her ölüm −50; `killdiff_timed` uzun koşusunda NP tükenirse `RequestRegene` `no_np` reddi bot doğamamasına yol açar; bu durum `reason`'a değil `Bot_*.log`'a yazılır ve maç geçerli kalır `[A]`; F8-05 her koşuda NP'yi 1000'e sıfırlar. `reset: off` ile koşan senaryoda uyarı yazılır.
- `[BOT] ENABLED=0` varsayılan davranışı değişmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F8-07` — `<kısa-sha> [F8-07] …`
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
- İncelenen: `gece/2026-10-02...bot/F8-07` @ `<sha>`
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
