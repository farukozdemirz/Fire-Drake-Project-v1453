# F5-64: Botlar arası yol sorgusu bütçesinin sunucuya bağlanması (`NavQueryScheduler`), ertelenen sorgu davranışı, `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` telemetrisi ve `PERF_SAMPLE` nav payı

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-64 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F5-62** (`NavDrive`, `goto`, paylaşılan `NavPathfinder`) ve **F5-63** (`NavDrive` Follow, `NavFollower::ReplanDue`) `KAPANDI` olmalı (dolaylı: F5-59, F5-61). Zaten `KAPANDI`: F5-53 (`NavQueryScheduler`, `NavPathCache`, `NavReplanPhaseMs`, `NavWhileDeferred`), F3-01 (`PERF_SAMPLE`), F3-01 telemetri altyapısı. Şemsiye: F5-55 (dilim 6). F5-65 bu planın kuyruk/önbellek referansını temizler |
| İlgili gereksinim / kabul | `docs/12` §13.5 (strateji 1-5), §13.5.2 kural 9 (ölçüm etiketi), §13.5.3 ("Ölçülen büyüklük 2": nav payı alanları ve hüküm), AC-NAV-07 (16 bot nav payı p95 ≤ 1,5 ms; hiçbir bot 1 sn'den uzun yol beklemez), MET-PERF-02/03, `docs/16` §3.2 (`NAV_PATH`, `NAV_STUCK`/`NAV_RECOVERY`, `PERF_SAMPLE`), §3.3 (seviye eşlemesi: `NAV_*` = `trace`; `PERF_SAMPLE` alanları satırı `docs/16:71`), T-NAV-11 |
| Tahmini büyüklük | M (7 dosya; saf mantık yeniden düzenlemesi + sunucu bağlaması; kanıtın büyük kısmı çalışma zamanında) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak) |

---

## Neden TASLAK

F5-62/F5-63 kodu depoda yok; bu plan onların **taslak** sözleşmesine dayanır. HAZIR yapmak için:

1. **F5-62 ve F5-63 `KAPANDI`;** sözleşme: `NavDrive` (`Goto`/`Follow`; `BeginGoto`, `BeginFollow`, `NextStep`, `TickFollow(...)` = gözlem + `ReplanDue` + `Update` + `NotifyReplan` + `Assess` + `monitor`), `NavDriveEvents` (planlama/kurtarma sonuçları), `NavFollower::ReplanDue(now, params) const`, `NavService::SharedPathfinder()`, `ActionExecutor::TickPathMove/TickFollow`. F5-63'te `TickFollow` planı **kendi içinde hemen** hesaplar; bu plan onu **planlama** ve **değerlendirme/adım** olarak ikiye böler (geriye uyumlu: tek çağrılık eski kullanım `Immediate` kipiyle aynı kalır).
2. **F5-59 sözleşmesi:** `NavService` (`Instance()`, `Ready()`, `Grid()`); bu plan ona zamanlayıcı/önbellek/istatistik üyeleri ekler.
3. Yazım turunda yeniden doğrulanacak referanslar (`gece/2026-10-02` @ `9fc2dfe` üzerinde okundu): `BotCore/NavBudget.h:21-210` (`NavQueryScheduler`: `kCapacity` 128, `Request`, `NextBatch`, `ReportCost`, `Pending`, `OldestWaitMs`, `Cancel`, `Clear`, `SetMaxWaitMs`; varsayılan `maxWait` 1000 ms, soğuk başlangıç maliyeti 0,5 ms), `:220-345` (`NavCacheKey`, `NavPathCache`: `kCapacity` 64, `kMaxCells` 512, TTL 30 000 ms, `Find`/`Put`/`Invalidate`/`Clear`), `:349-358` (`NavReplanPhaseMs(slot, 500, 5)`), `:361-382` (`NavDeferAction{FollowPlan, Hold}`, `NavDeferParams{planMaxAgeMs 5000, driftMaxM 15}`, `NavWhileDeferred`); `GameServer/Bot/BotManager.cpp:391-436` (`Tick`: `ProcessCommands`, `TickSessions`, `RefreshStatusSnapshot`, `m_scenario.Tick`, `m_script.Tick`, `RecordTick`), `:438-455` (`RecordTick`), `:457-516` (`EmitPerfSample`; **`char fields[512]` `:498`**, mevcut dize ≈ 330 karakter), `BotManager.h:64-65, 140-146` (`m_tickUs`, `m_perfWindowStart`); `GameServer/Bot/Telemetry.h:76` (`Emit(level, ev, bot, name, fields, droppable)`); `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md:58-59, 71` (olay satırları ve `PERF_SAMPLE` alan listesi).
4. **Bağımlı karar (HAZIR öncesi):** `nav_stale_steps` ek alanının kabulü (§2 "G4"): kullanıcı alan listesinde yok, ama "bayat planla sürme 0" kabulünü kanıtlamak için gerekli.
5. **KI-023 (HAZIR öncesi zorunlu):** F5-69 (eğim 0,45, ADR-0024) sonrası `NavBudget_Deferred_Chase_Sim` (`Tests/BotCoreTests/NavBudgetTests.cpp`, `pinnedParams.maxSlope = 0.625f`) 0,625'e sabitli: 0,45'te mode B'de başarısız plan yenilemesinden sonra ertelenen sorgu sözleşmesi bayat planı izliyor (`follow_stale_ticks=8`) ve bir koşuda bir bot hiç plan alamadı (`without_plan_pct=6,5`). Üretim varsayılanı 0,45 olduğundan bu plan HAZIR yapılırken: (a) kök neden `NavBudget.h` `NavWhileDeferred`/zamanlayıcı mı yoksa çağıran tarafta mı, depoda doğrulanır; (b) §3 "`NavBudget.h` değişmez" kuralı gerekiyorsa gerekçeyle gevşetilir; (c) sabitleme kaldırılır ve `followStaleTicks == 0` kabulü 0,45'te yeşil olur (eşik gevşetilmez). KI-023 bu plan `DOĞRULANDI` olunca kapanır.

## 1. Amaç

16 bot aynı anda yol sorguladığında `BotManager::Tick()` içindeki nav payı **tick başına `P-NAV-TICK-BUDGET-MS` = 1,5 ms** ile sınırlanır (yumuşak bütçe: tek sorgu bölünmez), sorgular **adil sırayla** ve **faz kaydırmalı** çalışır, ertelenen botlar **bayat planla sürmez** (taze plan varsa izler, yoksa durup bekler) ve ilk plan beklemesi ≤ 1,1 sn kalır. Sunucu bu işi ölçer: `PERF_SAMPLE`'a `nav_us_p95/p99/max`, `nav_queries`, `nav_deferred`, `nav_wait_max_ms` (+ `nav_stale_steps`) eklenir ve `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` olayları (trace seviyesi) yazılır. `NAV=0` iken `PERF_SAMPLE` satırı bugünkü ile **bayt bayt aynıdır**.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.5 (bütçe stratejisi): (1) tick bütçesi 1,5 ms, **yumuşak** (tek sorgu bölünmez; ilerleme garantisi için bütçeyi aşan tek sorgu yine çalışır; p99 bütçenin üstüne çıkabilir, hedef yalnız p95); (2) FIFO + dönen sıra, bir bot en çok `P-NAV-MAX-WAIT` = 1 sn bekler (aşılırsa öncelik alır); (3) yeniden planlama fazı `slot % 5 × 100 ms`; (4) yol önbelleği TTL 30 sn; (5) AC-NAV-07: nav payı p95 ≤ 1,5 ms, bekleme ölçümde ≤ 1,1 sn.
- `docs/12` §13.5.3 **ölçüm tanımı** (dosya `docs/12`; taslak metin `drafts/docs-measure/olcum-matrisi.md` §13.5.3, "Ölçülen büyüklük 2"): yeni `PERF_SAMPLE` alanları `nav_us_p95`, `nav_us_p99`, `nav_us_max` (tek `Tick()` içinde zamanlayıcının seçtiği sorguların `Update`/`Find` toplam süresi, `steady_clock`, aynı 5 sn penceresi), `nav_queries` (pencerede yürütülen), `nav_deferred` (ertelenen bot-tick sayısı), `nav_wait_max_ms` (en uzun istek → servis); hüküm `nav_us_p95 ≤ 1500` ve `nav_wait_max_ms ≤ 1100`; "Geçmez ise: önce `nav_us_p95`/taban ayrımı". Persentil tanımı `PERF_SAMPLE`: en yakın sıra `ceil(p·n)` (`BotManager.cpp:480-487`).
- `docs/16` §3.3 `PERF_SAMPLE` alanları satırı (`docs/16:71`): nav alanları "F5-55 dilimleriyle eklenir; bugün yoktur": bu plan **koddur**; satır güncellemesi Claude'dadır.
- **Tanım kararları (bu plan, `[Ö]` önerisi; ölçüm kuralı 9 etiketi zorunlu):**
  - `nav_us_*`: her `Tick()` için o tick'te **yürütülen sorguların süre toplamı** (µs); sorgu yürütülmeyen tick'ler **0 µs** olarak pencereye girer (persentil tüm `tick_n` tick'i üzerinden; PM-M11-B "tick toplamı p95" tanımıyla aynı). Yalnızca planlama (`Find`/`Update` + düzleştirme) sayılır; istek toplama/zamanlayıcı seçimi mikrosaniye altıdır ve ayrıca ölçülmez; `UnitView` anlık görüntüsü (F5-63) ve paket gönderimi **nav payına girmez** (toplam `tick_p95_us`'a girer).
  - `nav_queries`: pencerede `Find`/`Update` çalıştıran servis sayısı; önbellek isabeti sorgu **sayılmaz** (ayrı alan eklenmez; `NAV_PATH` `cache:1`).
  - `nav_deferred`: pencerede, her tick için `NextBatch`'ten sonra kuyrukta **kalan** bot sayılarının toplamı (bot-tick).
  - `nav_wait_max_ms`: pencerede bir isteğin `Request` anından servise (yürütmeye) kadar geçen en uzun süre; hemen servis edilen istek 0 sayılır; **ilk plan** (komut anından) ve **yeniden plan** ayrımı yapılmaz (en büyük değer raporlanır; `NAV_PATH.wait_ms` ayrımı taşır).
  - `nav_stale_steps` (**ek alan**, G4): pencerede, plan yaşı `planMaxAgeMs` (5000) veya hedef kayması `driftMaxM` (15 m) eşiğini aşmışken **gönderilmiş** hareket paketi sayısı; zorlanan `Hold` ile **0** olmalıdır (sabit değişmezin ölçümü).
- `NavQueryScheduler` yalnızca **seçim** yapar (kendi araması yoktur; `NextBatch` seçilenleri kuyruktan silmez: çağıran her servisten sonra `Cancel` çağırmalı; ilerleme garantisi: bekleyen varsa en az bir bot döner). Maliyet tahmini bot başına EWMA (0,7/0,3), yoksa genel EWMA, yoksa 0,5 ms. Bot kimlikleri **yoğun dizinlerdir** 0..127: `BotManager::m_sessions` indeksi (oturumlar hiç silinmez; `BotManager.h` "sessions are never freed") `botId` olarak kullanılır.
- **Havuz/önbellek paylaşımı:** zamanlayıcı ve `NavPathCache` **tek** örnektir, `NavService`'te (F5-62 karar tablosu: tüm çağıranlar IOCP iş parçacığında; `NavQueryScheduler`/`NavPathCache` iş parçacığı güvenli değildir, ek kilit **yoktur**).
- `NavFollower::Update` plan yapar ve `plannedAtMs`/`Reason` içeride tutar; F5-63 `ReplanDue` due kuralını dışarı açtı. Önbellek yalnızca **`goto` planında** kullanılır (anahtar: başlangıç hücresi, hedef hücre, `fieldVersion` = 0 [F5-62/63: alan yok]; isabette `NavSmoothPath` yeniden çalışır, `Find` çalışmaz). Takip planı halka hücresi/öngörü nedeniyle her seferinde farklıdır: önbelleğe **girmez**. Doğuş noktası jitter'ı nedeniyle isabet oranı düşük olabilir: oran **raporlanır, kabul ölçütü değildir** (`docs/12` §13.5 madde 4 "ulus başına doğuş → arena kenarı").
- `BotManager::Tick()` (`:391-436`) sırası: `ProcessCommands` → `TickSessions` → `RefreshStatusSnapshot` → `m_scenario.Tick` → `m_script.Tick`. Nav aşaması **`ProcessCommands` ile `TickSessions` arasına** girer (komutla gelen yeni `goto/follow` aynı tick'te istenir; adım atan `TickSessions` güncel planı kullanır).

### Tasarım kararları (Claude önerisi)

- **G1 iki aşamalı tick:** `TickNav(now)` (yeni, IOCP): (a) hareket kipindeki her canlı bot için `drive.PlanDue(nowMs)` ise `scheduler.Request(i, nowMs)` (zaten bekleyen isteğin zaman damgası korunur), artık gerekmiyorsa `Cancel(i)`; (b) `NextBatch(nowMs, 1.5, out, cap)`; (c) seçilen her bot için planı yürüt (zamanla), `ReportCost(id, ms, expanded)`, `Cancel(id)`, istatistik; (d) `nav_deferred += scheduler.Pending()`. `TickSessions` adımları (`TickPathMove`/`TickFollow`) **plan yapmaz**; plan durumuna göre adım atar veya `Hold` uygular.
- **G2 ertelenen davranış (`NavWhileDeferred`):** bot bir plan beklerken (`PlanDue` ama servis edilmedi): `hasPlan` (aktif rota var), `planAgeMs = now − plannedAt`, `targetDriftM` = (takip) son gözlenen hedef ile plan hedefi arası mesafe / (goto) 0 → `FollowPlan`: **mevcut rotada** adım atmaya devam; `Hold`: **bir kez** `speed 0` durma paketi (`StopMove` kalıbı, `SubmitMove` guard'lı), niyet `SetIntent(false)`, paket göndermez; plan gelince niyet `true`, `NotifyReplan` (F5-63 G2). **Düz çizgiye geçiş ve bayat planla sürme yoktur.** İlk plan (rota yok) beklerken bot zaten duruyordur (durma paketi gerekmez).
- **G3 faz kaydırma:** takip kipinde ilk plandan sonraki **ilk** `Interval` yeniden planı `NavReplanPhaseMs(slotIndex)` kadar ertelenir (`PlanDue` içinde, bir kez; sonraki aralıklar 500 ms). `Moved` (≥ 6 m) tetiği **ertelenmez**. Faz, ilk plan beklemesine eklenmez (≤ 1,1 sn bütçesini yemez).
- **G4 `nav_stale_steps` değişmezi:** `SubmitMove` çağrısından hemen önce (yol/takip adımı) plan yaşı ve kayma hesaplanır; eşik aşılmışsa **adım atılmaz** (zaten `Hold`) ve sayaç **artmaz**; aşılmış olmasına rağmen bir paket gönderilirse (yürütme hatası) sayaç artar ve `Bot_*.log`'a `nav: stale step sent` yazılır. Kabul: sayaç **0**.
- **G5 seviye ve düşürme:** `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` `TEL_TRACE`, **düşürülemez** (`droppable = false`; `docs/16` §3.3: yalnızca `PERF_SAMPLE`/`DECISION` düşer); `PERF_SAMPLE` mevcut `TEL_SUMMARY` yolu.

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavDrive.h`: `PlanDue(nowMs, slot)`, `RunPlan(...)` (planlama: `goto` → önbellek + `Find` + düzleştirme; takip → `NavFollower::Update`), `TickFollow` ikiye bölünür (`RunPlan` + `TickAssess`; eski tek çağrılık `Immediate` korunur), ertelenen durum (`NavDeferAction` kararı, plan yaşı/kayma), faz kaydırma, `requestedAtMs`/`wait` izleme, `staleStep` değişmezi.
2. `GameServer/Bot/NavService.h`: `NavQueryScheduler`, `NavPathCache` ve nav istatistik üyeleri (`Scheduler()`, `Cache()`, `Stats()`).
3. `BotManager`: `TickNav`, `Tick()` içine yerleştirme, `PERF_SAMPLE` alan genişlemesi, `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` yayını, `CommandGoto`'nun ilk planı zamanlayıcıya bırakması (eşzamanlı yalnızca ucuz denetimler).
4. `ActionExecutor`: `Hold` durma paketi, adım öncesi bayatlık denetimi, `BeginGoto`'nun plan çağrısını `RunPlan` kuyruğuna devretmesi.
5. Birim testleri (`NavDriveTests.cpp` eklemeleri).
6. Çalışma zamanı doğrulaması (Claude; §6 K9-K14): küçük ölçüm (12 bot), **AC-NAV-07 kapatma iddiası yok**.

**Kapsam dışı (yapılmayacak)**

- 16 bot koşusu ve AC-NAV-07/MET-PERF-02 hükmü (F5-66; 16 karakter ön koşulu orada).
- `tools/bot-telemetry-report.py` nav payı tablosu ve `ENV` satırı (F5-66, kayıt araçları).
- Arka plan iş parçacığı (ADR-0005 v2), hiyerarşik arama, `P-NAV-MAX-NODES`/`P-NAV-TICK-BUDGET-MS` değer değişikliği (değerler `docs/12` §13.5'tedir; `[Ö]`).
- Durum temizliği olayları (F5-65: yalnızca `Cancel(botId)` çağrısını bırakacağı **giriş noktası** bu planda `NavDrive::Reset()` + `scheduler.Cancel(i)` birlikte çağrılabilir şekilde hazırdır).
- `BotCore/NavBudget.h`, `NavTrack.h` (F5-63'teki ekler dışında), `NavStuck.h`, `Perception.h` **değişmez**; `Telemetry.*` değişmez; `docs/` (Claude: `docs/16` §3.2/§3.3, `docs/12` §13.5.3, `docs/STATUS.md`).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavDrive.h` | değiştir | bölme + ertelenen davranış + faz + önbellek kancası; F5-62/63 testleri değişmeden geçer |
| `Tests/BotCoreTests/NavDriveTests.cpp` | değiştir | yalnızca sona yeni vakalar (§5.7) |
| `GameServer/Bot/NavService.h` | değiştir | yalnızca zamanlayıcı/önbellek/istatistik üyeleri ve erişimciler |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `Hold` yardımcısı bildirimi |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | `Hold` paketi, bayatlık denetimi, `BeginGoto`/`BeginFollow` plan devri |
| `GameServer/Bot/BotManager.h` | değiştir | `TickNav`, `m_navUs` pencere vektörü ve sayaçlar |
| `GameServer/Bot/BotManager.cpp` | değiştir | `Tick()` yerleşimi, `TickNav`, `EmitPerfSample` (`char fields[...]` büyütülür), `NAV_*` yayını |

7 dosya, yeni dosya yok (`.vcxproj` değişmez). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-64 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucu `[UP]` ise `./tools/run-servers.sh stop`. F5-62/F5-63 `NavDrive.h` ve `ActionExecutor` imzalarını ve `NavBudget.h` API'sini aç/doğrula (sapma varsa **dur**). `python3 tools/nav-export.py`.
2. **`NavDrive.h`:** (a) `bool PlanDue(int64_t nowMs, int slot) const` (Goto: ilk plan bekliyor veya kiriş-engeli yeniden planı; Follow: `follower.ReplanDue(...)` **ve** G3 faz kuralı); (b) `NavDriveEvents RunPlan(const NavGrid &, NavPathfinder &, NavPathCache *, int64_t nowMs, float botX, float botZ, float botSpeedMps, ...)`: planı yürütür (Goto: önbellek `Find` → isabet `cells`, kaçırma `Find` + `Put`; Follow: `Update`), `NotifyReplan` (F5-63 G2), plan zamanı/`plannedAtMs`, `requestedAtMs` sıfırlanır; (c) `TickFollow` = `RunPlan` (yalnızca `Immediate` kipinde) + `TickAssess(...)` (gözlem/assessor/monitor/kurtarma; **plan yapmaz**); (d) `NavDeferAction DeferDecision(nowMs, driftM, const NavDeferParams &) const` (`NavWhileDeferred`'i `hasPlan`/`planAgeMs` ile besler); (e) `RequestedAtMs()`; (f) sayaç `StaleStepCheck(nowMs, driftM)`.
3. **`NavService.h`:** `BotCore::NavQueryScheduler m_sched; BotCore::NavPathCache m_cache; struct NavStats { uint32 queries, deferred, staleSteps; uint32 waitMaxMs; ... }`; `Scheduler()`, `Cache()`, `Stats()` (tümü IOCP-only, yorumda). `Startup()` zamanlayıcıyı `SetMaxWaitMs(1000)`, `Cache().SetTtlMs(30000)` ile varsayılan değerlerde bırakır (kodla yeniden tanımlama yok).
4. **`BotManager::TickNav(now)`** ve `Tick()`: `ProcessCommands(); TickNav(now); TickSessions();` (`NavService::Ready()` değilse `return`). `std::chrono::steady_clock` ile **yalnızca yürütülen planların** süre toplamı `m_navTickUs` (µs); tick sonunda `m_navUs.push_back(m_navTickUs)` (`m_tickUs` ile aynı boyut sınırı 4096; `Telemetry::IsEnabled(TEL_SUMMARY)` kapısı `RecordTick` ile aynı). `NAV_PATH` (planı yürütürken): `{"kind":"goto|follow","sx":..,"sz":..,"gx":..,"gz":..,"len_m":..,"nodes":..,"us":..,"ok":true|false,"status":"Found|NoPath|NodeLimit|InvalidStart|InvalidGoal","cache":0|1,"reason":"First|Moved|Interval","wait_ms":..}` (`docs/16:58`: başlangıç, hedef, uzunluk, düğüm sayısı, süre µs, başarı). `NAV_STUCK`: tespit anında `{"kind":"NoProgress|Oscillation","x":..,"z":..,"cell_x":..,"cell_z":..,"stage":1}`; `NAV_RECOVERY`: her aşama girişinde `{"stage":n,"action":"Replan|SideStep|StepBack|PenalizeReplan|Abandon","x":..,"z":..}` ve kurtarmada `{"stage":n,"result":"recovered","ms":recoverMs}` / bırakmada `{"result":"abandon"}` (`docs/16:59`: konum, aşama, süre, sonuç). Hepsi `TEL_TRACE`, `droppable = false`, `bot` = soket kimliği, `name` = karakter adı.
5. **`EmitPerfSample`:** nav alanları **yalnızca `NavService::Instance().Ready()` iken** eklenir (aksi halde dize bugünküyle bayt bayt aynı): `,"nav_us_p95":..,"nav_us_p99":..,"nav_us_max":..,"nav_queries":..,"nav_deferred":..,"nav_wait_max_ms":..,"nav_stale_steps":..` (persentil mevcut `ceil(p·n)` yöntemiyle, `m_navUs` sıralanarak). **`char fields[512]` yetmez** (mevcut ≈ 330 + yeni ≈ 110 = 440; `snprintf` kırpması sessiz bozuk JSON üretir): 768'e çıkar ve dönüş değeri ≥ boyut ise `Bot_*.log`'a uyarı yaz. Pencere sonunda `m_navUs`/sayaçlar sıfırlanır.
6. **`ActionExecutor`:** `Hold`: `StopMove` kalıbında tek durma paketi (`SubmitMove` guard'lı; hareket niyeti kapalı), yeniden planı beklerken paket gönderilmez; `TickPathMove`/`TickFollow` adım öncesi `StaleStepCheck`: eşik aşılmışsa **adım atılmaz**; `BeginGoto` yalnızca ucuz denetimler (`nav_off`, `nav_zone`, `invalid_goal`, `invalid_start` — `Find` çalıştırmaz); `no_path`/`node_limit` planlama anında `Bot_*.log`'a (`goto: plan failed (<status>)`) ve yürüyüş **başlamaz**. F5-62 K14 beklentisi bu yönde değişir.
7. **Testler** (`NavDriveTests.cpp`; adlar sabit; sentetik zaman/`Rng` tohumu; **gerçek harita varsa** zone 71, yoksa `SKIPPED`):
   - `NavDriveBudget_SplitEquivalence`: `TickFollow(Immediate)` ile `PlanDue` + `RunPlan` + `TickAssess` aynı sentetik dizide aynı planlar/kararları üretir (F5-63 sonuçları bayt bayt aynı).
   - `NavDriveBudget_Deferred_FollowsFreshPlan`: bütçe 0 (hiç servis yok), plan taze: bot mevcut rotada **adım atar**, `staleSteps == 0`; plan yaşı 5000 ms'yi veya kayma 15 m'yi aşınca `Hold`: **tek** durma paketi, sonra paket yok; plan gelince `NotifyReplan` + yürüyüş sürer (düz çizgi/`StepToward` çağrısı **yok**).
   - `NavDriveBudget_FirstPlan_Wait`: 16 sanal bot aynı tick'te ilk plan ister (`NavQueryScheduler` + gerçek `NavDrive::RunPlan`, bütçe 1,5, maliyet sentetik 0,5 ms + gerçek ölçülen): her bot ≤ 1100 ms'de planlanır (`wait_max ≤ 1100`); servis edilmeyen bot **hareket etmez**; `NAVBUDGET first_plan wait_max_ms=<n> served=16`.
   - `NavDriveBudget_PhaseSpread`: 16 bot aynı anda takip başlatır: ilk `Interval` planları `NavReplanPhaseMs(slot)` ile dağılır (5 faz); tek tick'te en çok ⌈16/5⌉ + `Moved` kaynaklı plan; `Moved` tetiği ertelenmez.
   - `NavDriveBudget_Cache_Goto`: aynı (başlangıç, hedef) ikinci `goto` `Find` çalıştırmaz (sayaç), düzleştirme çalışır; TTL sonrası yeniden `Find`; aynı yol sonucu (bayt bayt aynı waypoint'ler); `kMaxCells` aşan yol saklanmaz ama yürünür.
   - `NavDriveBudget_Cancel_OnReset`: bekleyen isteği olan bot `Reset()` + `Cancel` ile kuyruktan çıkar; `Pending()` düşer; sonraki `NextBatch` onu seçmez.
   - `NavDriveBudget_RealMap_Load16` (`#ifndef _DEBUG`; gerçek harita; **sentetik** 16 sanal bot, takip + `goto`, sanal 120 sn): tick toplamı (yürütülen sorgular) p95 ≤ 2,0 ms (PM-M11-B kapısı; **AC-NAV-07 1,5 hedefi bu testle kapanmaz**), bekleme ≤ 1100, `stale_steps == 0`; satır `NAVBUDGET load16 tick_p95_ms=.. p99=.. wait_max_ms=.. queries=.. deferred=.. stale=0`.
8. Derle/test (§7); `check-perception-contract.py` rc=0. Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; değişen dosyalar `touch` edilince yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; yedi yeni `NavDriveBudget_*` adı `[ OK ]` (Release'te `RealMap_Load16` süre kapısıyla); F5-62/F5-63/F5-53 testleri (`NavBudget_*`, `NavDrive_*`, `NavDriveFollow_*`) değişmeden geçer
- [ ] K3: `NavDriveBudget_FirstPlan_Wait`: `wait_max_ms ≤ 1100`, servis 16/16; `NavDriveBudget_Deferred_FollowsFreshPlan`: `stale_steps = 0`, `Hold` **tek** durma paketi
- [ ] K4: `git diff --stat` yalnızca §4 (7 dosya) + plan; `BotCore/NavBudget.h|NavTrack.h|NavStuck.h|Perception.h`, `Telemetry.*`, `docs/`, `tools/`, `.vcxproj` farkı **0**; `git diff --check` boş
- [ ] K5: `NavDrive.h`'de `grep -n -E "windows.h|stdafx|GameServer|shared/|static |new |malloc"` boş; `python3 tools/check-perception-contract.py` rc=0
- [ ] K6: `EmitPerfSample`: `NavService::Ready()` değilken `fields` dizesi **ön-değişiklikle aynı** (kod incelemesi: nav alanları tek `if`'in içinde); `char fields[...]` ≥ 768 ve `snprintf` kırpma denetimi (`dosya:satır` raporda)
- [ ] K7: `TickNav` yalnızca **yürütülen planların** süresini `m_navTickUs`'a ekler; `Tick()` sırası `ProcessCommands → TickNav → TickSessions` (`dosya:satır`); `scheduler.Cancel` her servis ve her `Reset`'ten sonra çağrılır (`Pending()` sızıntısı yok: test + kod)
- [ ] K8: ertelenen bot yolunda `StepToward`/düz `TickMove` çağrısı **yoktur** (kod incelemesi: `Hold`/`FollowPlan` dışında dal yok)
- [ ] K9 (Claude, çalışma zamanı): `ENABLED=1, NAV=1, TELEMETRY=trace` (NAV_* için; `PERF_SAMPLE` `summary`'de de yazılır), 12 bot spawn (bugünkü sınır), hepsi `follow`/`goto` döngüsünde 10 dk: `PERF_SAMPLE` satırlarında `nav_us_p95`, `nav_us_p99`, `nav_us_max`, `nav_queries`, `nav_deferred`, `nav_wait_max_ms`, `nav_stale_steps` **mevcut ve sayısal**; `nav_stale_steps` tüm pencerelerde **0**; `skipped_ticks`/`dropped_*` ≠ 0 olan pencere geçersiz sayılır; sonuçlar `[V: ORT-S, Y5, PERF_SAMPLE nav alanları, 12 bot]` etiketiyle rapora (**kısmi; AC-NAV-07 ve MET-PERF-02 16 bot kanıtı DEĞİLDİR**)
- [ ] K10 (Claude): `NAV=0` ve `ENABLED=1`: `PERF_SAMPLE` satırı nav alanı **içermez** ve alan sırası/boşluğu bugünküyle aynı (`tick_*` alanlarının önceki koşuyla bayt karşılaştırması); `NAV=1` iken bot yokken (`MAX_BOTS=0` veya hiç hareket yok) nav alanları 0
- [ ] K11 (Claude): `NAV_PATH` satırları (trace): `ok:true` oranı, `us` dağılımı, `wait_ms` en büyüğü; `NAV_STUCK`/`NAV_RECOVERY` satırları `Bot_*.log` `follow: stuck` satırlarıyla bire bir eşleşir (sayı farkı **0**)
- [ ] K12 (Claude): ertelenen sorgu davranışı gözlemi: 12 botu aynı tick'te `goto`'ya verip (komut dosyası) ilk plan beklemesi (`NAV_PATH.wait_ms`, `nav_wait_max_ms`) ve ertelenen botların bekleyip yürüdüğü; **düz çizgi adımı yok** (telemetride `ACTION_SUBMIT` Move konumları plan rotası üzerinde); bayat plan eşiğinin aşılması gerçek sunucuda zorlanamaz (bütçe koda sabittir, ini anahtarı yoktur): `Hold` davranışının kanıtı birim testindedir ve çalışma zamanında yalnızca `nav_stale_steps = 0` değişmezi izlenir
- [ ] K13 (Claude): önbellek: aynı doğuştan aynı arena hedefine art arda iki `goto`: ikinci `NAV_PATH` `cache:1` olabilir (isabet oranı **raporlanır**; 0 olması kabul engeli değildir)
- [ ] K14 (Claude): sunucu `stop` ile kapanır; "AC-NAV-07 kapandı" **yazılmaz**; K9 sayıları `docs/12` §13.5.3 hüküm sütunuyla yan yana ama **hüküm verilmeden** raporlanır (hüküm F5-66, 16 bot)

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavDriveBudget_|NAVBUDGET|NavBudget_|tests,"
./tools/run-tests.sh Debug
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-64
git diff --check gece/2026-10-02...bot/F5-64
# Claude (çalışma zamanı): ./tools/run-servers.sh start ; grep -a '"PERF_SAMPLE"' /mnt/c/dev/fdp/server/Logs/bots/*/live-*.jsonl | tail -5 ; python3 tools/bot-telemetry-report.py <log dizini> ; ./tools/run-servers.sh stop
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; konsol spam'i yok; bot sistemi/`NAV` kapalıyken sunucu davranışı ve `PERF_SAMPLE` çıktısı değişmez.
- **Bot avantajı yasağı:** bütçe botu **yavaşlatır** (yol hesabı beklemesi), hızlandırmaz; bayat planla "kısa yoldan" sürmek yoktur; ertelenen bot durur (insan gecikmesine benzer bir bekleme).
- Thread: tüm zamanlayıcı/önbellek erişimi IOCP iş parçacığında; ek kilit yok. Yeni bir iş parçacığı bu kararı geçersiz kılar.
- **Dürüstlük:** `nav_us_*` bu planın **tanımıdır** (yürütülen planların toplamı); `docs/12` §13.5.3 metni onaylanmış tanım değil önerilen ölçüm tanımıdır, Claude `docs/12`'ye işlerken değerlendirir. 12 bot ölçümü 16 bot yükünün **yerine geçmez**. `PERF_SAMPLE` pencerelerinde (5 sn, ~46-50 tick) `p95` en yakın sıradır; kuyruğun ilk 60 sn (giriş/spawn) ısınması hükme katılmaz (§13.5.3 "Gürültü").
- `P-NAV-TICK-BUDGET-MS` yumuşaktır: tek sorgu bölünmez, bütçeyi aşan sorgu yine çalışır; p99 > 1,5 ms **beklenir** ve ihlal sayılmaz (hedef yalnız p95).
- Beklenmedik durumda (F5-62/63 sözleşmesi farklı, `TickFollow` bölünemiyor, `fields` dizesi başka yerde kullanılıyor, ek dosya gerekiyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-64` — `<kısa-sha> [F5-64] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ … (K9-K14 Claude'un çalışma zamanı kriterleri)
- Gerçek-harita test satırları (`NAVBUDGET ...`): …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F5-64` @ `<sha>`
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
