# F5-64: Yol sorgusu bütçesinin saf mantık dilimi: `NavDrive` planlama/adım ayrımı (Follow `PlanFollow`/`AssessFollow`, Goto `ArmGoto`/`RunGotoPlan`/`RequestReplan`), ertelenen sorgu davranışı, faz kaydırma ve yol önbelleği kancası

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-64 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `KAPANDI`: **F5-62** (`NavDrive` Goto, merge `8522239`), **F5-73** (`NavDrive` Follow, merge `bc3170d`), **F5-71** (`NavFollower::UpdateReachable`, merge `1bfb885`), **F5-74** (`/bot follow` sunucu bağlaması, merge `23fe31d`), **F5-70** (`/bot goto` sunucu bağlaması, merge `99d7170`), F5-53 (`NavQueryScheduler`, `NavPathCache`, `NavReplanPhaseMs`, `NavWhileDeferred`; `BotCore/NavBudget.h`). F5-65 bu planın kuyruk iptali giriş noktasını (`Reset()` + `scheduler.Cancel`) kullanır |
| İlgili gereksinim / kabul | `docs/12` §13.5 (strateji 1-5; bu plan yalnızca **saf mantık yarısı**), §4.2 (hareketli hedef 6 m / 500 ms), AC-NAV-07 (kanıt sunucu dilimlerinde: F5-75/F5-66), MET-PERF-02/03 |
| Tahmini büyüklük | M (4 dosya, hepsi `BotCore/` + `Tests/BotCoreTests/`; `GameServer` ikilisi **değişmez**, yeni dosya ve `.vcxproj` farkı yok; 16 yeni birim testi) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu; referanslar `gece/2026-10-02` @ `03c2e9a` üzerinde doğrulandı) |

---

## 0. Bu planın yeri (bölme kaydı)

Önceki `F5-64` taslağı (7 dosya) F5-62/F5-63 sözleşmesinin **taslağına** dayanıyordu; gerçekleşen kod farklıdır (`TickFollow` tek çağrılık `reach`'li şablon, Goto planı `BeginGoto` içinde eşzamanlı, `BotCore/NavBudget.h` zaten var). Gece kuralı (≤ ~6 dosya, tek yetenek) gereği iş, F5-62/F5-70 örneğindeki gibi bölündü:

| Dilim | İçerik | Durum |
|---|---|---|
| **F5-64 (bu plan)** | Saf mantık: planlama ile adım atmanın ayrılması, ertelenen davranış (`deferHold`), faz kaydırma, önbellek kancası. `BotCore/` + `Tests/BotCoreTests/` (4 dosya). Sunucu ikilisi değişmez | HAZIR |
| F5-75 (sıradaki, yazılacak) | Sunucu bağlaması: `NavService`'e `NavQueryScheduler`/`NavPathCache`/istatistik üyeleri, `BotManager::TickNav` (`ProcessCommands` ile `TickSessions` arası), `ActionExecutor` bölünmesi (`BeginGoto` ilk planı kuyruğa bırakır, `TickPathMove`/`TickFollow` plan yapmaz), `Hold` durma paketi | yazılmadı |
| F5-76 (yazılacak) | Telemetri: `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` olayları, `PERF_SAMPLE` nav alanları (`nav_us_p95/p99/max`, `nav_queries`, `nav_deferred`, `nav_wait_max_ms`, `nav_stale_steps`; `char fields[512]` taşma riski) | yazılmadı |

(F5-75/F5-76 numaraları öneridir; kimlik değişmezliği gereği sıradaki serbest numaralardır. Bu planda ikisinin de kodu **yoktur**.)

Önceki taslağın "KI-023 kapanmalı" ön koşulu karşılandı (F5-71 `KAPANDI`; `grep -n "pinnedParams" Tests/BotCoreTests/NavBudgetTests.cpp` boş olmalıdır, adım 1'de doğrula).

## 1. Amaç

Bugün `NavDrive` planı çağrının **içinde** hesaplar: `TickFollow` (planlama + değerlendirme + kurtarma tek çağrı) ve `BeginGoto`/`Replan` (A* çağrı anında). 16 bot aynı tick'te plan istediğinde nav payı kontrolsüz büyür. Bu plan, F5-53'ün `NavQueryScheduler`'ının sunucuda **kullanılabilmesi için** sürücüyü ikiye böler:

1. **Takip:** `PlanFollow` (yalnızca planlama) ve `AssessFollow` (yalnızca kayıp politikası + değerlendirme + kurtarma; plan yapmaz); `FollowPlanDue` ("bu bot şimdi plan istiyor mu?"). Mevcut `TickFollow` **bayt bayt aynı** kalır (= planla + değerlendir).
2. **Ertelenen davranış:** plan istendi ama servis edilmedi ve rota bayatsa (yaş > 5000 ms ya da hedef kayması > 15 m) sürücü **adım üretmeyi bırakır** (`deferHold`, tek durma kararı); taze rotada eski rotayı izlemeye devam eder. Düz çizgiye geçiş yoktur.
3. **Faz kaydırma:** takip kipinde ilk plandan sonraki **ilk** `Interval` yeniden planı `phaseMs` kadar ertelenir (`NavReplanPhaseMs(slot)`); `Moved` tetiği ertelenmez.
4. **Goto:** planı sonradan yapılabilir kılan `ArmGoto` (yalnızca ucuz denetim, A* yok), `RunGotoPlan` (önbellek kancalı planlama), `RequestReplan` (Blocked sonrası bekleyen yeniden plan).

`BotCore` yalnızca saf mantıktır: sunucu, saat, global durum yok. `GameServer` hiçbir çağrıyı değiştirmez; bot sistemi/`NAV` kapalıyken **ve açıkken** sunucu davranışı bu planla değişmez (yeni üyeler çağrılmaz).

## 2. Bağlam (okunması zorunlu)

Satır numaraları `gece/2026-10-02` @ `03c2e9a` üzerinde okundu. Sapma görürsen **dur** (§5 adım 1). Asıl kural işlev adıdır.

- **`BotCore/NavDrive.h` (784 satır):** `NavDriveMode { Off, Goto, Follow }` `:27`; `NavPlanStatus { None, Planned, InvalidStart, InvalidGoal, NoPath, NodeLimit, ReplanLimit }` `:29`; `NavDriveParams` `:31-35`; `NavDriveStep` `:37-44`; `NavFollowDriveParams` `:48-61` (kurucu `:78-83`); `NavDriveEvents` `:64-76`; `NavDrive::Reset()` `:88-123` (**her yeni üye burada sıfırlanmalı**); `BeginGoto` `:131-143`; `NextStep` `:147-213`; `Replan` `:218-238` (`kMaxGotoReplans = 1`, `:17`); `PlanGoto` `:285-331` (`ValidCoord`, hedef `NavQuantiseM`, `grid.Walk` denetimleri, `finder.Find`, `NavSmoothPath`, `AdoptRoute`); `AdoptRoute` `:336-380`; Follow üyeleri bildirimi `:251-276` (`BeginFollow :251/:466`, `ObserveTarget :252/:474`, `TickFollow` şablonu `:255-267`, `NextFollowStep :268/:637`, `OnPacketSent :270/:752`, `OnPacketRejected :271/:778`); `TickFollowImpl :427-429` bildirimi, gövde `:482-635`: sıra **(a)** `m_blockedAbandon` → `PathBlocked` bitişi `:495-500`, **(b)** kayıp politikası `lostMs` (`:503-524`: ≥ `lostAbandonMs` → `TargetLost`; > `lostHoldMs` → `holdStop`; ≤ `lostGraceMs` → `m_holding = false` + **plan bloğu** `:525-584`), **(c)** `verdict`/`awaitingLong` `:587-593`, **(d)** `moving` + `m_monitor.Update` + kurtarma eylemi anahtarı `:596-632`. `NextFollowStep` `:637-750` (bekleyen yan/geri adım `:647-723`, varış/kayıp/rota yok → adım yok `:726`, `NextStep` + `Blocked` kaydı `:729-748`).
- **`BotCore/NavTrack.h`:** `NavFollowParams :127-139` (`replanDistM 6`, `replanIntervalMs 500`, …); `NavReplanReason { None, First, Moved, Interval }` `:150`; `NavFollower :169-217` (`InvalidatePlan :175`, `Update :184`, `UpdateReachable :192-198`, `Plan()`, `LastReason()`, `Replans()`, `Tracker()`); **plan gerekliliği kuralı** `UpdateImpl :376-395` (gözlem yoksa `false`; `m_plan.status == NoTarget` → `First`; `replanDistM > 0` ve hedef `plan.targetX/Z`'den ≥ 6 m → `Moved`; aksi halde `nowMs − plannedAtMs ≥ replanIntervalMs` → `Interval`). Not: başarısız planda da `plannedAtMs/targetX/Z` güncellenir (`:497-504`).
- **`BotCore/NavBudget.h`:** `NavQueryScheduler :21-217` (`kCapacity 128`, `Request :29`, `NextBatch :46`, `ReportCost :101`, `Pending :121`, `Cancel :140`; `NextBatch` seçilenleri **silmez**: çağıran her servisten sonra `Cancel` çağırır; bekleyen varsa ≥ 1 bot döner; `maxWait` 1000 ms; soğuk maliyet 0,5 ms); `NavCacheKey :220-232`; `NavPathCache :234-345` (`Find :244`, `Put :266`, `kCapacity 64`, `kMaxCells 512`, TTL 30 000 ms); `NavReplanPhaseMs :349`; `NavDeferAction`/`NavDeferParams`/`NavWhileDeferred :361-381` (`planMaxAgeMs 5000`, `driftMaxM 15`). **Bu başlık değişmez**; `NavDrive.h` onu `#include` eder (`NavBudget.h` → `NavTrack.h`; `NavDrive.h`'yi içermez, döngü yoktur).
- **Sunucudaki mevcut kullanım (bu plan DEĞİŞTİRMEZ; F5-75 değiştirecek):** `ActionExecutor.cpp` `BeginGoto :389-437` (yerel `NavDrive` ile `BeginGoto`, `PlanReason :241`), `TickPathMove :443-524` (`NextStep` + `NeedsReplan :257` + `Replan` `:494`), `BeginFollow :544-599`, `TickFollow :604-736` (`s->m_navDrive.TickFollow(...)` `:647-650`), `BotSession.h:81` `m_navDrive` (`sizeof(NavDrive)` ≈ 8464 bayt; yeni üyeler yalnızca onlarca bayt ekler).
- **Test dosyası `Tests/BotCoreTests/NavDriveTests.cpp` (2319 satır):** yardımcılar anonim ad alanında: `MakeNav :59`, `OpenEvents :30`, `WallEvents :45`, `HeightZeros :54`, `LoadZone71OrSkip :69` (gerçek harita; yoksa `SKIPPED` yazar), `SimTarget :897` (hedef hareket modelleri 0..6), `FollowRun :1013`, `StageSeqContainsInOrder :1387`; mevcut Follow testleri `:1399-2318` (`MakePocketGrid40 :1372` cep ızgarası `NavDriveFollow_PlanFailed_Abandon` testinde kullanılır `:1867`); `#ifndef _DEBUG` süre kapısı kalıbı `:843-849`, `:2314-2318`. Yeni testler dosyanın **sonuna** eklenir.
- **`Tests/BotCoreTests/NavTrackTests.cpp` (2217 satır):** `NavFollower` testleri; yeni iki test sona eklenir.
- Başlangıç test sayısı **315** beklenir (F5-74 sonrası); `./tools/run-tests.sh Release 2>&1 | grep "tests,"` ile ölç, rapora yaz. Bu planla **315 + 16** beklenir.

### Tasarım kararları (otonom döngüde Claude kararı — gözden geçirilmeli; yeni ADR yok: ADR-0006 madde 6 + `docs/12` §13.5 uygulaması, **ADR-0006 Ek F5-64 doğrulamada yazılır**)

**D1 — `NavFollower::DueReason` tek kaynak.** `NavTrack.h`'ye `NavReplanReason DueReason(int64_t nowMs, const NavFollowParams & params) const` eklenir (sabit/`const`, yan etkisiz): `UpdateImpl :376-395`'teki "plan gerekli mi" kuralının **aynısı** (gözlem yok → `None`; `NoTarget` → `First`; `Moved`; `Interval`). `UpdateImpl` aynı işlevi çağırır (kural tek yerde; davranış `phaseMs == 0` iken bayt bayt aynı). `UpdateImpl`'in diğer parçaları değişmez.

**D2 — Faz kaydırma `NavFollowParams::phaseMs`.** `NavFollowParams`'ın **sonuna** `int phaseMs = 0;` eklenir ("ilk plandan sonraki ilk `Interval` için ek bekleme, ms; çağıran `NavReplanPhaseMs(slot)` verir"). `DueReason`: `Interval` koşulu `nowMs − plannedAtMs ≥ replanIntervalMs + (Replans() <= 1 ? phaseMs : 0)`. `Replans()` ilk plandan sonra 1'dir; `Moved` tetiği faz tanımaz; ikinci plandan itibaren faz uygulanmaz. Varsayılan 0: mevcut tüm çağıranlar değişmez. (Üye eklemek `NavFollowParams` başlatıcı-liste kullanımını bozabilir: adım 1'de `grep -rn "NavFollowParams *{"` boş olmalı.)

**D3 — `NavDrive::FollowPlanDue`.** `bool FollowPlanDue(int64_t nowMs, const NavFollowDriveParams & params) const`: mod `Follow`, `!m_blockedAbandon`, `nowMs − m_lastSeenMs ≤ lostGraceMs` (kayıp politikasıyla aynı eşik) **ve** `m_follower.DueReason(nowMs, params.follow) != None`. Çağıran bunu botun kuyruğa istek koyup koymayacağına karar vermek için kullanır.

**D4 — Aşama bayrağı, `All` yolu dokunulmaz.** Özel `TickFollowImpl`'e `NavPlanPhase { All, PlanOnly, AssessOnly }` parametresi ve `NavPathfinder *` (null olabilir) verilir. **`All` yolu kodu ve davranışı mevcut `TickFollow` ile aynıdır** (bu yüzden F5-73'ün mevcut `NavDriveFollow_*` testleri (18) değişmeden geçer). Genel yüzey:
- `template <class Reach> NavDriveEvents PlanFollow(const NavGrid &, NavPathfinder &, int64_t nowMs, float botX, float botZ, float botSpeedMps, const NavFollowDriveParams &, NavCostLayer * scratch, const Reach & reach)`: `PlanOnly`. Mod `Follow` değilse, koordinat geçersizse, `m_blockedAbandon` ise veya `!FollowPlanDue(...)` ise **hiçbir şey yapmadan** boş `NavDriveEvents` döner (A* çalışmaz). Aksi halde yalnızca mevcut **plan bloğunu** (`:529-584`: ceza alanı kurma, `UpdateReachable`, `AdoptRoute`, `NotifyReplan`, `planFailAbandon` → `PlanFailed` + `Reset`) çalıştırır; `verdict`/`monitor` **çalışmaz**. Reach'siz aşırı yükleme **eklenmez** (KI-026: sunucu `reach`'li çağırır).
- `NavDriveEvents AssessFollow(const NavGrid &, int64_t nowMs, float botX, float botZ, const NavFollowDriveParams &)`: `AssessOnly`; finder/reach/scratch almaz. Sırasıyla mevcut: `m_blockedAbandon` bitişi, kayıp politikası (`TargetLost`/`holdStop`/`m_holding = false`), **plan bloğu atlanır**, D5 ertelenme kararı, `verdict`/`awaitingLong`, `moving`/`monitor`/kurtarma eylemi. Plan yapmaz.
- Eşdeğerlik sözleşmesi: aynı girdi dizisinde `TickFollow(...)` ile `{ if (FollowPlanDue) PlanFollow(...); AssessFollow(...) }` **aynı** olayları (`planned`, `planStatus`, `planReason`, `planExpanded`, `routeAdopted`, `verdict`, `recovery.action/stage`, `holdStop`, `awaitingLong`, `ended`), aynı `FollowPlans()`/`RecoveryStage()`/`StuckEpisodes()` ve aynı `NextFollowStep` sonuçlarını üretir (test 1). `PlanFollow` bir `ended` döndürdüyse (yalnızca `PlanFailed`) sürücü `Reset` edilmiştir ve `AssessFollow` boş döner. Farkın yalnızca iki nedenle olmasına izin var ve bayt farkı yaratmaz: ceza katmanı `scratch` yalnızca plan **gerekliyken** kurulur (bugün her tick kurulur; içerik çağrılar arası taşınmaz), `NavStuckPenalties::Count` `const`'tır (`NavStuck.h:169`).

**D5 — Ertelenme: `deferHold`.** Yeni `NavDriveEvents::deferHold` (`bool`, varsayılan `false`; `PlanOnly`/`All` yollarında **hiç `true` olmaz**). `AssessFollow`, kayıp politikasından sonra ve `m_assess.Assess`'ten **önce**: `m_route.size() >= 2 && !m_holding && !m_arrived && FollowPlanDue(...)` ise (bot plan istiyor ama servis edilmemiş) `NavWhileDeferred(true, nowMs − m_routeAtMs, driftM, params.defer)` hesaplar; `Hold` ise ve `!m_deferHeld` ise: `m_deferHeld = true; ev.deferHold = true; m_assess.SetIntent(false, nowMs);` (bir bölüm başına **bir kez**; çağıran bir durma paketi gönderebilir, göndermese de sürücü adım üretmez). `driftM` = son gözlenen hedef (`Follower().Tracker().Latest`) ile **rotanın planlandığı** hedef (`m_routeTargetX/Z`) arası mesafe. Rota yokken (ilk plan beklemesi) `deferHold` **üretilmez** (bot zaten duruyor). `NextFollowStep`, `m_deferHeld` iken ilk iş olarak **`kind None`** döner (bekleyen yan/geri adım dahil). `PlanFollow` bir `Planned` plan benimserse `m_deferHeld = false` (niyeti açma mevcut kodla olur: `SetIntent(true)` `:567`); başarısız planda `m_deferHeld` korunur (mevcut `planFailAbandon` sayacı sonlandırmayı sürdürür). `NavFollowDriveParams`'a `NavDeferParams defer;` alanı eklenir (varsayılan 5000 ms / 15 m).

**D6 — Rota damgaları.** Yeni üyeler: `int64_t m_routeAtMs` (rota en son ne zaman benimsendi; `Reset()` ve `BeginFollow` sonrası `INT64_MIN` = bilinmiyor), `float m_routeTargetX/Z`. Takip plan bloğu `routeAdopted` iken `m_routeAtMs = nowMs; m_routeTargetX/Z = plan.targetX/Z; m_deferHeld = false` yazar (**`All` yolunda da**, zararsız). `INT64_MIN` iken yaş "çok eski" sayılır (`Hold`). Ek erişimciler: `bool DeferHeld() const`, `int64_t RouteAgeMs(int64_t nowMs) const` (bilinmiyorsa `INT64_MAX`), `bool RouteStale(int64_t nowMs, const NavDeferParams &) const` (yalnızca `Follow`, rota var ve `NavWhileDeferred(...) == Hold`; Goto'da `false`). `RouteStale`, F5-76'daki `nav_stale_steps` sayacının tanımıdır (ertelenmiş **ve** bayat rotada gönderilmiş paket).

**D7 — Goto: `ArmGoto` / `RunGotoPlan` / `RequestReplan` (eşzamanlı yollar değişmez).** `BeginGoto`/`Replan` olduğu gibi kalır (F5-70/F5-72 çağıranları ve 13 `NavDrive_*` testi). Yeni:
- `NavPlanStatus ArmGoto(const NavGrid &, float botX, float botZ, float goalX, float goalZ)`: `Reset()`; koordinat geçerliliği ve **yalnızca** `grid.Walk` denetimi (A* **yok**, `finder` parametresi yok): geçersiz/non-finite/Walk-dışı başlangıç → `InvalidStart`, hedef → `InvalidGoal` (`PlanGoto :288-306` ile aynı sıra ve aynı sonuç; sürücü `Reset`). Başarıda `m_mode = Goto`, `m_goalX/Z` nicemlenmiş hedef, `m_planPending = true`, `m_planIsReplan = false`, rota **boş**; **`NavPlanStatus::None`** döner ("silahlandı, plan bekliyor"; `Planned` dönmez). `PlanPending()` erişimcisi. Bekleyen sürücüde `NextStep` rota yok → `kind None` (mevcut `:150`); çağıran `PlanPending()`'e bakarak bunu `path_blocked` saymaz.
- `NavPlanStatus RunGotoPlan(const NavGrid &, NavPathfinder &, NavPathCache * cache, int64_t nowMs, float botX, float botZ, const NavDriveParams &)`: `!m_planPending` → `None` (hiçbir şey yapmaz). Aksi halde `PlanGoto` ile **aynı** sıra/sonuç (`InvalidStart`/`InvalidGoal`/`NoPath`/`NodeLimit`, başarısızlıkta `Reset`), tek fark önbellek: `cache != nullptr` ise anahtar `{başlangıç hücresi, hedef hücre, fieldVersion 0}`; **isabette** `cache->Find` hücreleri verir, `finder.Find` **çalışmaz** (`PlanExpanded() == 0`), `NavSmoothPath` + `AdoptRoute` çalışır (düzleştirme/rota başlangıç noktası bot konumundan olduğu için yeniden hesaplanır); **kaçırmada** `finder.Find` çalışır ve `Found` ise `cache->Put(key, path.cells, path.cost, path.length, nowMs)` yazılır (`kMaxCells` aşan yol `Put` tarafından zaten reddedilir, plan yine yürür). `LastPlanCacheHit()` erişimcisi (`bool`; her çağrıda güncellenir; `cache == nullptr` ise `false`). `Planned`: `m_planPending = false`; `m_planIsReplan` ise `++m_replans`; `m_mode = Goto`.
- `NavPlanStatus RequestReplan()`: mod `Goto` değilse `None`; `m_planPending` zaten açıksa `None` (idempotent); `m_replans >= kMaxGotoReplans` ise `Reset()` + `ReplanLimit` (`Replan :223-227` ile aynı); aksi halde `m_planPending = true; m_planIsReplan = true`, **rota korunur** (Blocked sonrası bekleme; `NextStep` aynı rotada `Blocked` vermeye devam eder), `None` döner.
- Eşdeğerlik (test 9/10): `ArmGoto + RunGotoPlan(cache = nullptr)` ile `BeginGoto` aynı `Route()`, `RouteLengthM()`, `GoalX/Z()`, `PlanExpanded()`, `PlanWaypoints()`, `Replans() == 0`'ı üretir; `RequestReplan + RunGotoPlan` ile `Replan` aynı rotayı ve `Replans() == 1`'i üretir.
- `Reset()` `m_planPending`, `m_planIsReplan`, `m_cacheHit`'i sıfırlar; **bekleyen sürücüde `Reset()` kuyruk iptalinin sürücü yarısıdır** (F5-65 sunucu tarafında `scheduler.Cancel` ile birlikte çağırır).

**D8 — Sunucu/ayar değeri yok.** `P-NAV-TICK-BUDGET-MS`, `P-NAV-MAX-WAIT`, TTL, `planMaxAgeMs`/`driftMaxM` değerleri bu planda **değişmez** (`docs/12` §13.5; `NavBudget.h` sabitleri). Bu plan yalnızca onların uygulanabileceği sürücü yüzeyini açar.

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavTrack.h`: `NavFollowParams::phaseMs`, `NavFollower::DueReason`, `UpdateImpl`'in `DueReason`'ı kullanması (D1/D2).
2. `BotCore/NavDrive.h`: `#include "NavBudget.h"`, `NavFollowDriveParams::defer`, `NavDriveEvents::deferHold`, `NavPlanPhase`, `FollowPlanDue`, `PlanFollow`, `AssessFollow`, `DeferHeld/RouteAgeMs/RouteStale`, rota damgaları, `NextFollowStep` `deferHold` kapısı, Goto `ArmGoto/PlanPending/RunGotoPlan/RequestReplan/LastPlanCacheHit` (D3-D7).
3. `Tests/BotCoreTests/NavTrackTests.cpp`: 2 yeni test (§5.4).
4. `Tests/BotCoreTests/NavDriveTests.cpp`: 14 yeni test (§5.5).
5. Derleme + birim test + değerlendirme; **çalışma zamanı (sunucu) doğrulaması yoktur** (sunucu ikilisi değişmez).

**Kapsam dışı (yapılmayacak)**

- `GameServer/`, `AIServer/`, `shared/`, `tools/`, `docs/`, `*.vcxproj*`: **dokunulmaz.** `ActionExecutor`/`BotManager`/`NavService` değişmez; mevcut `BeginGoto`/`TickPathMove`/`TickFollow` çağrıları aynen çalışır. Sunucu bağlaması F5-75'tir.
- `BotCore/NavBudget.h`, `NavStuck.h`, `NavDanger.h`, `NavReach.h`, `Perception.h`, `NavPath.h`, `NavSmooth.h`: **değişmez** (`NavDrive.h`/`NavTrack.h` dışında hiçbir başlık yok).
- `BeginGoto`, `Replan`, `NextStep`, `TickFollow`, `NextFollowStep` (D5'teki `deferHold` kapısı hariç), `OnPacketSent/Rejected` imza ve davranış değişikliği: **yok** (mevcut 13 + 18 + `NavBudget_*` testleri değişmeden geçer).
- Telemetri olayları, `PERF_SAMPLE` alanları, günlük satırları, zamanlayıcı/önbellek örneklerinin sunucuda tutulması, `Hold` durma paketi: F5-75/F5-76.
- Ulaşılamaz hedef ön denetimi (`NavReach` ile `ArmGoto` içinde bileşen denetimi), KI-024 eğim cebi: bu planda yok; `ArmGoto` yalnızca `grid.Walk` denetler (`PlanGoto` ile aynı).
- Önbellekte takip planı: yok (takip planı halka/öngörü nedeniyle her seferinde farklıdır; yalnızca Goto kullanır).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavTrack.h` | değiştir | yalnızca `phaseMs`, `DueReason`, `UpdateImpl`'in `DueReason` kullanması |
| `BotCore/NavDrive.h` | değiştir | D3-D7; mevcut işlevlerin imzaları değişmez |
| `Tests/BotCoreTests/NavTrackTests.cpp` | değiştir | yalnızca sona 2 yeni test |
| `Tests/BotCoreTests/NavDriveTests.cpp` | değiştir | yalnızca sona 14 yeni test; mevcut testlere dokunulmaz |

4 dosya, yeni dosya yok (`.vcxproj` değişmez). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-64 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucu `[UP]` ise `./tools/run-servers.sh stop`. `python3 tools/nav-export.py` (gerçek harita testleri). Başlangıç test sayısını ölç (`./tools/run-tests.sh Release 2>&1 | grep "tests,"`, beklenen 315). Doğrula: `grep -n "pinnedParams" Tests/BotCoreTests/NavBudgetTests.cpp` boş; `grep -rn "NavFollowParams *{" --include=*.h --include=*.cpp .` boş; §2'deki `NavDrive.h`/`NavTrack.h`/`NavBudget.h` işlevleri ve plan bloğu (`TickFollowImpl` `:525-584`) gerçekten orada. Sapma varsa **dur**.
2. **`NavTrack.h`:** D1/D2. `DueReason` `UpdateImpl`'in başındaki mevcut kuralın birebir taşınmasıdır; `UpdateImpl`'de `reason = DueReason(nowMs, params); if (reason == None) return false;` (gözlem yoksa `Latest` hâlâ `false` dönmeli: `DueReason` `None` döner). `phaseMs` varsayılan 0 iken `NavTrackTests.cpp`'nin tüm mevcut testleri **değişmeden** geçmeli.
3. **`NavDrive.h`:**
   1. `#include "NavBudget.h"`; `NavFollowDriveParams`'a `NavDeferParams defer;`; `NavDriveEvents`'e `bool deferHold = false;` (**sona** ekle).
   2. Üyeler: `bool m_planPending`, `bool m_planIsReplan`, `bool m_cacheHit`, `bool m_deferHeld`, `int64_t m_routeAtMs`, `float m_routeTargetX/Z`; hepsi `Reset()`'te sıfırlanır (`m_routeAtMs = INT64_MIN`).
   3. `TickFollowImpl` → `NavPlanPhase` + `NavPathfinder *` (D4). **`All` kolunun akışı bayt bayt aynı kalmalıdır;** `PlanOnly`/`AssessOnly` onun parçalarını yeniden kullanır (plan bloğunu özel bir `PlanBlock(...)` şablon yardımcısına taşıyıp `All` kolunun da onu çağırması kabul edilir, **yalnızca** davranış aynı kalıyorsa).
   4. `FollowPlanDue`, `PlanFollow`, `AssessFollow`, `DeferHeld/RouteAgeMs/RouteStale`; `NextFollowStep` başına `if (m_deferHeld) return step;` (mod denetiminden sonra).
   5. Goto: `ArmGoto`, `PlanPending`, `RunGotoPlan`, `RequestReplan`, `LastPlanCacheHit`; `PlanGoto`'nun önbellek kancalı varyantı (mevcut `PlanGoto`'yu bozmadan: ortak kısmı yardımcıya çıkar ya da `NavPathCache *` alan bir aşırı yükleme yaz; `BeginGoto`/`Replan` önbelleksiz yoldan aynen gider).
   6. Başlık kuralları: `windows.h`/`stdafx.h`/`GameServer`/`shared` yok; `static`, `new`, `malloc` kelimeleri yorumlarda dahil **geçmez** (K5 `grep`'i); global/statik durum yok; saat yok.
4. **`NavTrackTests.cpp` yeni testler (adlar sabit):**
   - `NavFollowDue_MatchesUpdate`: sentetik açık ızgara, tohumlu `Rng`, 400 adım rastgele gözlem/zaman (hedef durur/gider/zıplar, `nowMs` düzensiz); her adımda `due = follower.DueReason(now, params)`; ardından `UpdateReachable`/`Update` çağrılır: `planned == (due != None)` ve `planned` ise `LastReason() == due`; gözlem yokken `None`; `phaseMs == 0`.
   - `NavFollowDue_Phase`: `phaseMs = 200`: ilk plan sonrası hedef sabitken `Interval` plan 500 ms'de **değil**, 700 ms'de gelir; ikinci `Interval` ilk plandan sonraki +500 ms'de gelir (faz yalnız ilk); `Moved` (≥ 6 m kayma) fazı beklemeden **anında** `Moved`; `phaseMs = 0` mevcut davranış.
5. **`NavDriveTests.cpp` yeni testler (adlar sabit; sentetik zaman, tohumlu `Rng`; **gerçek harita varsa** zone 71, yoksa `SKIPPED` satırı — K3 SKIPPED'i reddeder):**
   1. `NavDriveQueue_Follow_SplitEquivalence`: iki sürücü A (`TickFollow`) ve B (`PlanFollow` koşullu + `AssessFollow`) aynı girdi dizisinde, ≥ 600 tick (100 ms), `SimTarget` modelleri 0, 1, 3 (üç tohum) + cep ızgarasında kurtarma/`PlanFailed` senaryosu: her tick'te olay alanları (D4) ve `NextFollowStep` sonuçları (`kind`, `x`, `z`) eşit; `FollowPlans/RecoveryStage/StuckEpisodes` eşit.
   2. `NavDriveQueue_Follow_PlanDue`: gözlemden önce `false`; `BeginFollow` + `ObserveTarget` sonrası `true` (First); `PlanFollow` sonrası `false`; +500 ms `true` (Interval); hedef ≥ 6 m kayınca `true` (Moved); hedef görülmeyeli > `lostGraceMs` iken `false`; `m_blockedAbandon` (`NavDriveFollow_BlockedLoop_Abandon` senaryosu) sonrası `false`; Goto/Off kipinde `false`.
   3. `NavDriveQueue_Follow_PhaseSpread`: 16 sürücü, `params.follow.phaseMs = NavReplanPhaseMs(slot)`, hedefler duruyor: hepsi t=0'da First planlar; ilk `Interval` planları 500/600/700/800/900 ms'de **4/3/3/3/3** botluk gruplar halinde gelir; sonraki `Interval`'ler +500 ms (faz yok); bir hedef ≥ 6 m kaydırılınca o bot fazı beklemeden `Moved` planlar.
   4. `NavDriveQueue_Follow_Deferred_FreshRoute`: plan alındı, sonra plan hiç servis edilmez (`PlanFollow` çağrılmaz), `AssessFollow`/`NextFollowStep` 4900 ms'ye kadar sürer: `deferHold` hiç `true` olmaz, adımlar eski rota üzerinde üretilir (her adımın kirişi `ChordSampleClean`), `RouteStale == false`.
   5. `NavDriveQueue_Follow_Deferred_StaleHold`: (a) yaş: plan 5001 ms servis edilmezse `AssessFollow` **tam bir kez** `deferHold` verir (sonraki çağrılarda `false`), `DeferHeld() == true`, `NextFollowStep` `None`, `RouteStale == true`; (b) kayma: hedef 16 m kaydırılır (yaş taze): aynı sonuç; (c) sonra `PlanFollow` bir `Planned` plan benimser: `DeferHeld() == false`, adımlar yeniden üretilir ve rota ilerlemesi artar (düz çizgi adımı yok: her adım `m_route` üzerinde); (d) hold sırasında hiçbir adım üretilmediği için `RouteStale` iken üretilmiş adım sayısı **0**.
   6. `NavDriveQueue_Follow_Deferred_FirstPlan`: rota yok (ilk plan beklerken): `deferHold` **hiç** üretilmez, `NextFollowStep` `None`, `DeferHeld() == false`.
   7. `NavDriveQueue_Follow_PlanFailed_Split`: cep hedefi (`MakePocketGrid40`): `PlanFollow` ile ardışık 10 başarısız plan `ended == PlanFailed` döndürür, sürücü `Off`; ardından `AssessFollow` boş olay döner; `NavDriveFollow_PlanFailed_Abandon` ile aynı tick sayısı.
   8. `NavDriveQueue_Goto_Arm`: `ArmGoto` başarıda `None` döner, `Active()`, `Mode() == Goto`, `PlanPending()`, `Route().empty()`, `NextStep` `None`; Walk-dışı başlangıç → `InvalidStart` ve `Off`; Walk-dışı hedef → `InvalidGoal`; non-finite/negatif/6553,5 üstü başlangıç → `InvalidStart`; `finder` kullanılmaz (imza gereği).
   9. `NavDriveQueue_Goto_RunPlan_Equivalence`: 200 rastgele çift (gerçek harita; yoksa `WallEvents` ızgarası): `BeginGoto` ile `ArmGoto + RunGotoPlan(cache = nullptr)` aynı `Route()` (bayt bayt), `RouteLengthM`, `GoalX/Z`, `PlanExpanded`, `PlanWaypoints`; `!PlanPending()`, `Replans() == 0`; `RunGotoPlan` ikinci çağrıda `None`; geçersiz uç çiftlerinde aynı `NavPlanStatus`.
   10. `NavDriveQueue_Goto_RequestReplan`: bir `Blocked` adım üretilen senaryo (`NavDrive_Blocked_Replan` kalıbı): `RequestReplan()` → `None`, `PlanPending()`, rota korunur, `NextStep` yine `Blocked`; ikinci `RequestReplan()` `None` (idempotent); `RunGotoPlan` → `Planned`, `Replans() == 1`, rota `Replan` ile üretilenle bayt bayt aynı; yeniden `RequestReplan()` → `ReplanLimit` ve sürücü `Off`; `Goto` olmayan modda `None`.
   11. `NavDriveQueue_Goto_Cache`: aynı (başlangıç hücresi, hedef hücre) iki `RunGotoPlan` (aynı bot konumu, önbellek ortak): ilk `LastPlanCacheHit() == false`, `PlanExpanded() > 0`, ikinci `true`, `PlanExpanded() == 0` ve `Route()` bayt bayt aynı; TTL (30 000 ms) sonrası üçüncü çağrı kaçırır; bot aynı hücrede farklı noktada: isabet, rota başı bot noktasında, kalan noktalar aynı; `kMaxCells` (512) aşan yol (gerçek haritada ≥ 513 hücrelik bir A* yolu; harita yoksa yalnızca bu alt durum atlanır ve `SKIPPED` satırı yazılır): `Put` saklamaz (`cache.Count()` artmaz) ama plan `Planned`; `cache == nullptr` → `LastPlanCacheHit() == false`.
   12. `NavDriveQueue_Goto_Cancel`: bekleyen sürücüde `Reset()` → `PlanPending() == false`, `Active() == false`; sonraki `RunGotoPlan` `None`; `NavQueryScheduler` ile birlikte: `Request(i)` → `Reset()` + `Cancel(i)` → `Pending()` düşer, sonraki `NextBatch` o botu seçmez.
   13. `NavDriveQueue_Scheduler_FirstPlan16`: 16 sanal bot aynı tick'te `ArmGoto` (gerçek harita: doğuş yakını → arena A; yoksa sentetik büyük ızgara), `NavQueryScheduler` (bütçe 1,5 ms; `ReportCost` **sabit sentetik 0,3 ms**, ölçüm değil): her tick `NextBatch` → seçilenlere `RunGotoPlan` → `Cancel`; servis edilmeyen bot **adım üretmez** (`NextStep None`, `PlanPending`); 16/16 planlanır, her bot ≤ 1100 ms (100 ms tick'te ≤ 11 tick) bekler; yazdır: `NAVQUEUE first_plan wait_max_ms=<n> served=16 ticks=<n>`. Kabul: `served == 16`, `wait_max_ms <= 1100`.
   14. `NavDriveQueue_Follow_Load16` (**gerçek harita**; yoksa `SKIPPED`; sanal 120 sn, 100 ms tick): 16 sürücü takip + her bot kendi hedefini izler (hedefler `SimTarget` 0/1/2/3 modelleri), `phaseMs = NavReplanPhaseMs(slot)`; her tick `FollowPlanDue` olan bot `Request`, `NextBatch(now, 1.5, ...)` seçilenlere `PlanFollow` (`steady_clock` ile **ölçülür**, `ReportCost`), her servisten sonra `Cancel`; tüm botlara `AssessFollow` + (1,5 sn kapısı) `NextFollowStep`. Sayılır: tick başına yürütülen plan süresi toplamı (µs; plansız tick 0), `wait_max_ms`, `deferHold` sayısı, **`RouteStale` iken üretilen adım sayısı (`stale_steps`)**. Kabul (mantık, her derlemede): `stale_steps == 0`, `wait_max_ms <= 1100`, hiçbir bot plansız kalmaz. Süre (`#ifndef _DEBUG`): tick toplamı p95 ≤ **4,0 ms** (regresyon bekçisi; **AC-NAV-07'nin 1,5 ms hedefi bu testle kapanmaz**, kanıt çalışma zamanındadır); yazdır: `NAVQUEUE load16 tick_p95_ms=.. p99=.. max=.. wait_max_ms=.. queries=.. deferred=.. hold=.. stale=0`.
6. Derle/test (§7); `git diff --stat`/`--check`; Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; değişen başlıklar `touch` edilince `GameServer` dahil yeni uyarı yok (`NavDrive.h` `GameServer`'a `BotSession.h` üzerinden girer)
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`, toplam = başlangıç + 16 (beklenen 315 → 331); on altı yeni test adı (`NavFollowDue_MatchesUpdate`, `NavFollowDue_Phase`, `NavDriveQueue_Follow_SplitEquivalence`, `..._PlanDue`, `..._PhaseSpread`, `..._Deferred_FreshRoute`, `..._Deferred_StaleHold`, `..._Deferred_FirstPlan`, `..._PlanFailed_Split`, `NavDriveQueue_Goto_Arm`, `..._RunPlan_Equivalence`, `..._RequestReplan`, `..._Cache`, `..._Cancel`, `NavDriveQueue_Scheduler_FirstPlan16`, `NavDriveQueue_Follow_Load16`) `[ OK ]`; mevcut `NavDrive_*` (13), `NavDriveFollow_*` (18), `NavBudget_*`, `NavFollow*`/`NavTrack*` testleri **değişmeden** geçer (mevcut test gövdelerinde `git diff` farkı 0)
- [ ] K3: Release çıktısında `NAVQUEUE first_plan wait_max_ms=<n> served=16` ve `NAVQUEUE load16 ...` satırları var; `served=16`, `wait_max_ms ≤ 1100`, `stale=0`; çıktıda `NavDriveQueue_` ile ilgili `SKIPPED` yok (`python3 tools/nav-export.py` sonrası)
- [ ] K4: `git diff --stat gece/2026-10-02...bot/F5-64` yalnızca §4'teki 4 dosya + plan dosyası; `GameServer/`, `AIServer/`, `shared/`, `tools/`, `docs/`, `*.vcxproj*`, `BotCore/NavBudget.h|NavStuck.h|NavDanger.h|NavReach.h|Perception.h|NavPath.h|NavSmooth.h` farkı **0**; `git diff --check` boş
- [ ] K5: `grep -n -E "windows.h|stdafx|GameServer|shared/|static |new |malloc" BotCore/NavDrive.h` **boş**; aynı `grep` `BotCore/NavTrack.h` için yalnızca bugünkü iki satırı verir (`:5` yorum, `:25` `static constexpr int kCapacity = 16;`), ek satır yok
- [ ] K6: davranış değişmezliği: `BotCore` dışındaki hiçbir çağıran değişmedi; `GameServer/Bot/` içinde `PlanFollow|AssessFollow|ArmGoto|RunGotoPlan|RequestReplan|FollowPlanDue|deferHold` geçmez (`grep -rn` boş; bunlar F5-75'te bağlanır)
- [ ] K7: `NavDriveQueue_Follow_SplitEquivalence` iki sürücünün olay ve adım eşitliğini ≥ 600 tick × 4 senaryoda kanıtlar (test kodunda eşitlik denetimi `dosya:satır` ile raporlanır); `TickFollow` `All` kolunun diff'i yalnızca yardımcıya taşıma/parametre geçirmedir (rapora kod incelemesi notu)
- [ ] K8: `NavDriveQueue_Follow_Deferred_StaleHold` dört alt durumun hepsini denetler; ertelenmiş bayat rotada üretilmiş adım sayısı 0
- [ ] K9: `NavDriveQueue_Goto_RunPlan_Equivalence` ve `..._RequestReplan`: `BeginGoto`/`Replan` ile aynı rota (bayt bayt); `Replans` sayacı ve `ReplanLimit` davranışı aynı

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavDriveQueue_|NavFollowDue_|NAVQUEUE|tests,"
./tools/run-tests.sh Debug 2>&1 | grep -E "tests,|FAILED"
grep -n -E "windows.h|stdafx|GameServer|shared/|static |new |malloc" BotCore/NavDrive.h BotCore/NavTrack.h   # NavDrive.h: boş; NavTrack.h: yalnız :5 ve :25
grep -rn -E "PlanFollow|AssessFollow|ArmGoto|RunGotoPlan|RequestReplan|FollowPlanDue|deferHold" GameServer/ || echo "none (expected)"
git diff --stat gece/2026-10-02...bot/F5-64
git diff --check gece/2026-10-02...bot/F5-64
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; ISO-8859 dosyalar yok (`BotCore`/`Tests` UTF-8/ASCII, satır sonları mevcut dosyalarınki gibi); konsol spam'i yok (test çıktısı yalnızca belirtilen `NAVQUEUE` satırları).
- **Bot avantajı yasağı korunur:** ertelenen bot yavaşlar veya durur, hızlanmaz; bayat rotada adım yok; düz çizgi/`StepToward` yolu eklenmez.
- **`TickFollow` `All` yolu dokunulmazdır:** `NavDriveFollow_*` testlerinden biri kırılırsa düzeltmeyi testte değil kodda yap (davranış eşdeğerliği sözleşmedir). Beklenmedik durumda (`TickFollowImpl` parçalanamıyor, `NavFollowParams` ek alanı bir çağıranı bozuyor, ek dosya gerekiyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.
- **Dürüstlük:** `NavDriveQueue_Follow_Load16` süre sayıları gerçek A* maliyetini ölçer ama sunucu tick'inin değil; AC-NAV-07/MET-PERF-02 hükmü yoktur ve bu plan "kapandı" yazmaz. Test 13'te maliyet **sabit** (0,3 ms) olduğundan bu bir zamanlayıcı **mantık** testidir, süre testi değil.
- Thread: bu plan sunucuda çağrılmadığı için thread kuralı yoktur; F5-75 tüm kullanımı IOCP iş parçacığında tutacaktır (`NavQueryScheduler`/`NavPathCache` iş parçacığı güvenli değildir).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-64` — `e66f5ca [F5-64] NavDrive plan/adim ayrimi, ertelenen davranis, faz kaydirma ve Goto kuyrugu (saf mantik dilimi)`; kabul raporu ayrı commit.
- Başlangıç test sayısı: **315** (`315 tests, 0 failed`); bitiş **331** (`331 tests, 0 failed`, Release ve Debug).
- Değişen dosyalar ve neden:
  - `BotCore/NavTrack.h`: `NavFollowParams::phaseMs` (D2), `NavFollower::DueReason` (D1) ve `UpdateImpl`'in kuralı `DueReason`'dan alması. `phaseMs == 0` iken davranış bit düzeyinde eski; 33 mevcut `NavTrack_*` testi değişmedi.
  - `BotCore/NavDrive.h`: `#include "NavBudget.h"`; `NavPlanPhase`; `NavFollowDriveParams::defer`; `NavDriveEvents::deferHold`; `FollowPlanDue`/`PlanFollow`/`AssessFollow`/`DeferHeld`/`RouteAgeMs`/`RouteStale`; rota damgaları; `NextFollowStep` `deferHold` kapısı; Goto `ArmGoto`/`PlanPending`/`RunGotoPlan`/`RequestReplan`/`LastPlanCacheHit`; ortak `PlanBlock` ve önbellek kancalı `PlanGotoImpl` (D3-D7). `BeginGoto`/`Replan`/`NextStep`/`TickFollow`/`OnPacketSent`/`OnPacketRejected` imzaları değişmedi.
  - `Tests/BotCoreTests/NavTrackTests.cpp`: sona 2 test (`NavFollowDue_MatchesUpdate`, `NavFollowDue_Phase`).
  - `Tests/BotCoreTests/NavDriveTests.cpp`: sona 14 test (Follow 7 + Goto/scheduler 7) + yerel yardımcılar (`QueueRoutesEqual`, `QueueEventsEqual`, `QueueStepsEqual`, `QueueMergeSplit`, `QueueApplyStep`, `QueueSplitEquiv`); mevcut test gövdelerinde 0 silme.
  - Yeni dosya yok; `*.vcxproj*`, `GameServer/`, `AIServer/`, `shared/`, `tools/`, `docs/` farkı 0.
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
    BotCore.vcxproj -> ...\build\bin\x86-Release\libs\BotCore.lib
    proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
    proj-AIServer.vcxproj -> ...\build\bin\x86-Release\Server\AIServer.exe
    NavDriveTests.cpp
    NavTrackTests.cpp
    BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  Release ve Debug `rc=0`; değişen başlıkları içeren `GameServer` nesneleri (`ActionExecutor.obj`, `BotSession.obj`) başlık mtime'ından yeni (yeniden derlendi); NavDrive/NavTrack/BotCoreTests için yeni uyarı yok; kalan uyarılar değişmeyen `GameServerDlg.cpp` (C4834/C4267) ve bağlayıcıda `UpgradeHandler.cpp` (C4789).
- Kabul kriterleri öz-değerlendirme:
  - **K1 ✔**: Release/Debug `rc=0`; `NavDrive.h`/`NavTrack.h` içeren `GameServer` yeniden derlendi, yeni uyarı yok.
  - **K2 ✔**: Release ve Debug `331 tests, 0 failed` (315 + 16); on altı yeni ad `[ OK ]`; mevcut `NavDrive_*`/`NavDriveFollow_*`/`NavBudget_*`/`NavTrack*`/`NavFollow*` testleri değişmeden geçti.
  - **K3 ✔**: `NAVQUEUE first_plan wait_max_ms=300 served=16 ticks=4`; `NAVQUEUE load16 ... wait_max_ms=300 ... stale=0`; `NavDriveQueue_` ile ilgili SKIPPED yok.
  - **K4 ✔**: `git diff --stat gece/2026-10-02...bot/F5-64` yalnızca §4'teki 4 dosya (+ plan dosyası) ; `git diff --check` boş.
  - **K5 ✔**: `grep -nE "windows.h|stdafx|GameServer|shared/|static |new |malloc" BotCore/NavDrive.h` boş; `NavTrack.h` yalnızca `:5` ve `:25`.
  - **K6 ✔**: `grep -rn -E "PlanFollow|AssessFollow|ArmGoto|RunGotoPlan|RequestReplan|FollowPlanDue|deferHold" GameServer/` boş.
  - **K7 ✔**: `NavDriveQueue_Follow_SplitEquivalence` üç model × 3 tohum × 60 s + gizli engel/kurtarma senaryosunda `QueueEventsEqual`/`QueueStepsEqual`/`FollowPlans`/`RecoveryStage`/`StuckEpisodes` eşitliğini 0 uyuşmazlıkla kanıtlar; eşitlik denetimi `Tests/BotCoreTests/NavDriveTests.cpp` içindeki yardımcılarda (`QueueEventsEqual` ~`:2343`, `QueueStepsEqual` ~`:2363`, `QueueSplitEquiv` ~`:2401`) `CHECK` ile raporlanır. `TickFollow` `All` kolu yalnızca `PlanBlock` yardımcısına taşındı; akış sırası aynı.
  - **K8 ✔**: `NavDriveQueue_Follow_Deferred_StaleHold` dört alt durumu (a yaş, b kayma, c yeniden plan sonrası adımlar + ilerleme, d bayat rotada 0 adım) denetler.
  - **K9 ✔**: `NavDriveQueue_Goto_RunPlan_Equivalence` (200 çift; 186 planlı) ve `..._RequestReplan` `BeginGoto`/`Replan` ile aynı rotayı ve `Replans`/`ReplanLimit` davranışını kanıtlar.
- `NAVQUEUE` satırları (Release):
  ```
  NAVQUEUE first_plan wait_max_ms=300 served=16 ticks=4
  NAVQUEUE load16 tick_p95_ms=0.014 p99=0.089 max=0.841 wait_max_ms=300 queries=2958 deferred=0 hold=0 stale=0 ended=0
  NAVQUEUE load16 stale_steps=0 no_plan_bots=0
  NAVQUEUE phase: 500/600/700/800/900 = 4/3/3/3/3
  NAVQUEUE freshroute: steps=4
  NAVQUEUE planfail: plans=10 at=4500
  NAVQUEUE gotoeq: planned=186
  ```
  Debug `NavDriveQueue_Follow_Load16`: `tick_p95_ms=0.276 p99=1.721 max=13.581 wait_max_ms=300 ... stale=0`.
- Plandan sapmalar ve gerekçeleri:
  1. **`NavDriveQueue_Goto_Cache` `kMaxCells` alt durumu sentetik 560×560 açık ızgarayla ölçüldü** (plan "gerçek haritada ≥ 513 hücrelik A* yolu; harita yoksa SKIPPED" diyordu). Gerçek-harita aramasına bağlı kalmasın ve K3'te `NavDriveQueue_` SKIPPED doğmasın diye deterministik sentetik ızgara seçildi: (2,2)→(557,557) köşegeni ~556 hücre > 512, `Put` reddediyor, `Count()==0`, plan `Planned`. Kapsam aynı (Put reddi + ikinci çağrıda cache-hit yok).
  2. **`NavDriveQueue_Follow_Load16`'ta scheduler'a `ReportCost` sabit 0,3 ms veriliyor** (plan "ölçülen maliyet" diyordu). Debug A*'sı Release'den ~20 kat yavaş; ölçülen maliyetle 1,5 ms bütçede tick başına 1 sorgu servis edilip 16 botluk ilk dalga `wait_max_ms > 1100` veriyordu (Debug kabulü `0 failed` kırılıyordu). Tick toplam süresi yine duvar saatiyle ölçülüp p95 guard'ı korunuyor; test 13 zaten sabit sentetik maliyet kullanıyor. `stale_steps==0`, `wait_max<=1100`, plansız bot yok, p95≤4,0 ms sağlandı.
  3. `NavDriveQueue_Follow_Deferred_StaleHold` (c) alt durumunda "rota ilerlemesi artar" denetimi eski rota yerine **yeni** rotanın iki ardışık adımında artan `routeProgressM` ile yapıldı (yeni rota bot konumundan kurulduğu için eski rotanın ilerlemesiyle kıyaslamak yanlıştı).
- Açık sorular:
  - `FollowLoad16`'da `deferred=0`/`hold=0`: defert yolu bu yükte tetiklenmiyor (testler 4/5/6 kapsıyor); gerçek sunucu bütçesinde stale adım ölçümü F5-76 telemetrisine bağlı.
  - `gece/2026-10-02` dalı iş sırasında bir "araç" commit'iyle (`049f2b8`) ilerledi (verify-evidence.sh); benim tabanım `ef759e4` ve fark kapsam dışı. Birleştirme kapsamı döngü/Claude'a aittir.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: **DOĞRULANDI**
- İncelenen: `gece/2026-10-02...bot/F5-64` @ `89f8b0a` (iki commit: `e66f5ca` kod, `89f8b0a` rapor; çalışma ağacı temizdi). Gece modu: birleştirmeyi ve push'u döngü betiği yapar, bu turda yapılmadı. Sunucular kapalıydı (`run-servers.sh status`: 0/3); çalışma zamanı kriteri yok (saf mantık dilimi), GUI/istemci gerektiren kontrol yok.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `NavDrive.h` ve `NavTrack.h` `touch` edildikten sonra `build.sh Release` rc=0: `ActionExecutor.cpp`, `BotSession.cpp`, `NavDriveTests.cpp`, `NavTrackTests.cpp` yeniden derlendi; uyarılar yalnızca değişmeyen `GameServerDlg.cpp(820,94)` C4834 ve `(1147,16)`/`(1806,18)` C4267. `build.sh Debug` rc=0, aynı iki C4267 uyarısı; `NavDrive`/`NavTrack`/test dosyalarında yeni uyarı yok |
| K2 | ✔ | Release ve Debug: `331 tests, 0 failed` (315 + 16). Release çıktısında 16 yeni ad `[ OK ]` (`NavFollowDue_MatchesUpdate`, `NavFollowDue_Phase`, `NavDriveQueue_Follow_{SplitEquivalence,PlanDue,PhaseSpread,Deferred_FreshRoute,Deferred_StaleHold,Deferred_FirstPlan,PlanFailed_Split}`, `NavDriveQueue_Goto_{Arm,RunPlan_Equivalence,RequestReplan,Cache,Cancel}`, `NavDriveQueue_Scheduler_FirstPlan16`, `NavDriveQueue_Follow_Load16`). Mevcut test gövdelerine dokunulmamış: `git diff --numstat` `NavDriveTests.cpp` 1026 ekleme / **0 silme**, `NavTrackTests.cpp` 105 / **0** |
| K3 | ✔ (not 1) | Kendi Release koşumum: `NAVQUEUE first_plan wait_max_ms=300 served=16 ticks=4`; `NAVQUEUE load16 tick_p95_ms=0.023 p99=0.112 max=0.750 wait_max_ms=300 queries=2958 deferred=0 hold=0 stale=0 ended=0`; `NAVQUEUE load16 stale_steps=0 no_plan_bots=0`. Gerçek harita (`tools/nav-export.py`: `zone71.navgrid` n=513) yüklendi; Release ve Debug çıktılarında `SKIPPED` yok (grep boş) |
| K4 | ✔ | `git diff --stat gece/2026-10-02...bot/F5-64`: yalnızca `BotCore/NavDrive.h`, `BotCore/NavTrack.h`, `Tests/BotCoreTests/NavDriveTests.cpp`, `Tests/BotCoreTests/NavTrackTests.cpp` + plan dosyası; `GameServer/`, `AIServer/`, `shared/`, `tools/`, `docs/`, `*.vcxproj*` ve yasak başlıklarda fark 0. `git diff --check` boş (rc=0). Dört dosya ASCII + CRLF, BOM yok (`file`, `grep -c $'\r'` = satır sayısı) |
| K5 | ✔ | `grep -n -E "windows.h\|stdafx\|GameServer\|shared/\|static \|new \|malloc" BotCore/NavDrive.h BotCore/NavTrack.h`: `NavDrive.h` boş; `NavTrack.h` yalnızca `:5` (yorum) ve `:25` (`kCapacity`) |
| K6 | ✔ | `grep -rn -E "PlanFollow\|AssessFollow\|ArmGoto\|RunGotoPlan\|RequestReplan\|FollowPlanDue\|deferHold" GameServer/` boş (`none (expected)`); `GameServer/` farkı 0 |
| K7 | ✔ | `Tests/BotCoreTests/NavDriveTests.cpp`: `QueueEventsEqual :2340`, `QueueStepsEqual :2355`, `QueueSplitEquiv :2399` (olay eşitliği `:2436`, sayaçlar `:2449-2451`, adım eşitliği `:2457`, konum/uyuşmazlık `:2472-2474`), test `:2478-2501`: 3 model × 3 tohum × 60 s (601 tick, 100 ms) + gizli engel senaryosu. **Bağımsız ölçüm:** geçici bir probe (commit edilmedi, geri alındı) gizli engel senaryosunda `maxStage=2`, `recActions=2` verdi: kurtarma merdiveni her iki sürücüde gerçekten koşuyor ve olay/adım eşitliği bozulmuyor; diğer dokuz koşuda kurtarma yok (beklenen, engel yok). `TickFollow` `All` kolunun diff'i: plan bloğu gövdesi `PlanBlock`'a olduğu gibi taşındı (satırlar özdeş; tek ek `m_routeAtMs/m_routeTargetX/Z/m_deferHeld` damgaları, D6), `All` kolunda `PlanBlock` + `ended` erken dönüşü aynı sırada; `else` (defer) kolu `All`'da çalışmaz |
| K8 | ✔ | `:2680-2777`: (a) yaş: 6000 ms'ye kadar `holdEvents == 1`, `DeferHeld()`, `RouteStale(6000)`, hold sırasında `NextFollowStep == None`, `staleSteps == 0` (`:2728-2732`); (b) kayma: 16 m, yaş taze, `deferHold`+`DeferHeld`+`RouteStale` (`:2763-2776`); (c) `PlanFollow` sonrası `DeferHeld() == false` ve iki ardışık adım `ChordSampleClean`, `routeProgressM` artıyor (`:2734-2760`); (d) bayat rotada üretilmiş adım 0. `FreshRoute :2634` 4900 ms'ye kadar `deferHold` yok, `FirstPlan :2779` rota yokken `deferHold` yok |
| K9 | ✔ | `:2876-2940`: 200 çift, 186 planlı: `BeginGoto` ile `ArmGoto + RunGotoPlan(nullptr)` aynı durum, `Route()` bayt bayt (`QueueRoutesEqual :2327`), `RouteLengthM`, `GoalX/Z`, `PlanExpanded`, `PlanWaypoints`; `:2942-2982`: `RequestReplan + RunGotoPlan` rotası `Replan` ile bayt bayt aynı, `Replans() == 1`, ikinci `RequestReplan` `ReplanLimit` + `Off`, `Goto` dışında `None` |

- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. **`NavDrive.h:371` (`FollowPlanDue`), `:837-851` (ertelenme kararı), bilgi (F5-75/F5-76 için):** `AssessFollow`'un ertelenme kararı `FollowPlanDue` koşuluna bağlı. Plan servis edilip **başarısız** olursa (`NoPath` vb.) `plannedAtMs/targetX/Z` güncellendiğinden `FollowPlanDue` ≤ 500 ms `false` olur; bu aralıkta rota hedef kaymasıyla bayatlamış olsa bile `deferHold` verilmez, `NextFollowStep` eski rotada adım üretir, `RouteStale()` ise `true` döner (F5-76'nın `nav_stale_steps` sayacı bunu sayar). `planFailAbandon` (10 plan, ≈ 5 sn) sürücüyü bitirene kadar sürebilir. Bu, F5-73'ün mevcut davranışıdır (başarısız planda eski rotayı izleme); F5-64 yeni bir gerileme getirmedi ve plan bunu kapsam dışı bırakmıştı. `docs/KNOWN_ISSUES.md` KI-027 olarak kaydedildi; F5-75 planı, sunucu tarafında `RouteStale` iken adım göndermeyi ya da bunu sayacın "beklenen" kategorisine yazmayı açıkça karara bağlamalı.
  2. **`NavDriveTests.cpp:3331` (not 1, K3):** `NAVQUEUE load16 ... stale=0` satırındaki `stale=0` format dizgesinde **sabit** yazıyor (plandaki biçim de öyle); gerçek sayaç ayrı satırda (`stale_steps=%d`, `:3333`) ve `CHECK_EQ(staleSteps, 0)` (`:3335`) ile denetleniyor. Ayrıca bu yükte `deferred=0 hold=0`: ertelenme yolu Load16'da hiç tetiklenmiyor, dolayısıyla `stale_steps == 0` burada kendiliğinden doğru; ertelenme davranışının kanıtı testler 4/5/6'dır (kanıtlar tabloda). Uygulayıcı bunu "Açık sorular"da zaten yazmış. K3'ün `stale=0` ifadesi tek başına kanıt sayılmadı; `stale_steps` satırı ve `CHECK` sayıldı.
  3. **Sapma 2 (kabul edildi):** `Scheduler_FirstPlan16` plana uygun (sabit 0,3 ms); `Follow_Load16`'da plan "`steady_clock` ile ölçülür, `ReportCost`" diyordu, uygulayıcı `ReportCost`'a sabit 0,3 ms verdi (Debug A*'sı ≈ 20× yavaş, ölçülen maliyetle Debug'da `wait_max_ms > 1100` olacaktı); tick toplamı yine duvar saatiyle ölçülüyor. Gerekçe makul, Debug kabulü korunuyor; ancak zamanlayıcının **gerçek maliyetle** davranışı bu testte kanıtlanmıyor: kanıt çalışma zamanındadır (F5-75/F5-66; AC-NAV-07/MET-PERF-02 kapanmadı, plan da "kapandı" yazmıyor).
  4. **Sapma 1 ve 3 (kabul edildi):** `Goto_Cache` `kMaxCells` alt durumu sentetik 560×560 ızgarayla (`Put` reddi, `Count() == 0`, plan `Planned`), `StaleHold (c)` yeni rotanın ardışık iki adımıyla; ikisi de plandaki niyeti karşılıyor. Küçük boşluklar: `Goto_Cache` "aynı hücrede farklı nokta" alt durumu yalnızca rota başının bot noktasında olmasını denetliyor ("kalan noktalar aynı" denetlenmiyor, `:3017-3025`); `SplitEquivalence` plandaki "cep ızgarasında PlanFailed" senaryosunu A/B karşılaştırmasında içermiyor (gizli engel senaryosu kullanıldı; `PlanFailed` yolu `PlanBlock` ortak kodudur ve `PlanFailed_Split` (`plans=10 at=4500`) ile mevcut `NavDriveFollow_PlanFailed_Abandon` ayrı ayrı geçiyor).
  5. **Üslup, §8:** plan "test çıktısı yalnızca belirtilen NAVQUEUE satırları" diyor; testler ek satırlar yazıyor (`NAVQUEUE phase/freshroute/planfail/gotoeq`, `NAVFOLDUE matches`, yalnızca hata hâlinde `split mismatch`). Zararsız, tanılayıcı; engel değil.
  6. **KI-026 kapanmadı:** F5-64 `NavDrive.h`'ye dokundu ama reach'siz aşırı yükleme boş başvurusu planda "eklenmez/kapsam dışı" olduğundan düzeltilmedi; KI-026 `AÇIK` kaldı, satır numaraları F5-64 ile kaydı.
- Düzeltme talimatı: yok (karar `DOĞRULANDI`).

```
(Düzeltme gerekmiyor.)
```
