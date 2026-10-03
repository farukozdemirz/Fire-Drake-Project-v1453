# F5-62: Yol izleme sürücüsü `NavDrive` — `Goto` kipi (`BotCore/NavDrive.h`, saf mantık; `/bot goto` sunucu bağlaması F5-70'te)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-62 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `KAPANDI`: F5-02 (`NavPathfinder`), F5-03 (`NavSmoothPath`), F5-57 (`NavRoutePoint`, `NavRouteProgressM`), F5-59 (`NavService`), F5-61 (`CheckMoveChord`, merge `gece/2026-10-02`), F5-69 (eğim 0,45). F5-70 (sunucu bağlaması), F5-63, F5-64, F5-65 bu planın `NavDrive`'ına dayanır |
| İlgili gereksinim / kabul | `docs/12` §3-§4.1 (yol bulma/düzleştirme), §13.1 (paket adımı modeli: ~1,5 sn'de bir hedef noktalı `WIZ_MOVE`, ara noktaları sunucu doğrulamaz), §13.4 (doğuş → arena dönüşü); `docs/03` CLI-05/CLI-08; AC-NAV-02 (A* p95 ≤ 2 ms), AC-NAV-03 (kiriş yürünebilirliği; oyunda kanıt F5-66); ADR-0006 Ek F5-62, ADR-0021 madde 1 |
| Tahmini büyüklük | S (4 dosya: 2 yeni, 2 tek satırlık `.vcxproj`; saf mantık, sunucu ikilisi değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu) |

---

## 0. Bu plan neden küçüldü (bölme kaydı)

Önceki TASLAK F5-62 12 dosyalıktı (saf mantık + `NavService` + `ActionExecutor` + `BotManager` + `ScriptPlan`). Gece planı kuralı (≤ ~6 dosya, tek yetenek) ve yazım turunda bulunan tasarım düzeltmeleri (§2 "Tasarım kararları") nedeniyle ikiye bölündü:

- **F5-62 (bu plan):** yalnızca `BotCore/NavDrive.h` (rota kurma, yol üzerinde adım üretme, kiriş denetimli kısaltma, yeniden planlama sınırı) + birim testleri. `GameServer/`, `AIServer/`, `shared/` **değişmez**; sunucu ikilisi bayt bayt aynı kalır.
- **F5-70 (sıradaki plan, bu plandan sonra yazılır):** `NavService::SharedPathfinder()`, `ActionExecutor::BeginGoto/TickPathMove`, `BotSession::m_navDrive`, `/bot goto`, betik fiili `goto`, çalışma zamanı K10-K15 (Claude). Eski taslağın bu bölümleri git geçmişinde durur: `git show f86cca6:plans/F5-62-nav-goto-yol-uzerinden-ilerleme.md` (F5-70 yazılırken oradan alınır; satır numaraları F4-55/F4-60/F5-61 sonrası yeniden doğrulanır).
- Kuyruk sırası: **F5-62 → F5-70 → F5-63 → F5-64 → F5-65 → F5-68 → F4-61 → F5-66**. F5-63/64/65 taslakları "F5-62" derken bu planın saf mantığını **ve** F5-70'in sunucu bağlamasını kasteder; HAZIR yapılırken bağımlılıkları buna göre güncellenir.

## 1. Amaç

`NavDrive` sınıfı, bir botu bilinen bir hedef noktasına **planlayıcı yolu üzerinden** yürütmek için gereken saf mantığı verir: `NavPathfinder::Find` + `NavSmoothPath` ile rota kurar; her ~1,5 sn'de bir sunucuya gidecek `WIZ_MOVE` adım noktasını **rota üzerinde ilerleyerek** (ara noktada durmadan) üretir; her adım noktası, yürütücünün göndereceği **nicemlenmiş** konum için F5-61 kiriş guard'ından (`CheckMoveChord`) geçirilmiş olarak döner. Bu plan sonunda `NavDrive` gerçek harita üzerinde (Karus ve El Morad doğuşundan arena A'ya, 300 rastgele çift) engelli hücreye değmeden varan adım dizileri üretir; hiçbir sunucu davranışı değişmez (bağlama F5-70).

## 2. Bağlam (okunması zorunlu)

Aşağıdaki satırlar `gece/2026-10-02` @ `c96e7d6` üzerinde doğrulandı.

- `docs/12` §13.1 (paket adımı modeli), §13.4 (doğuş → arena), §13.5 (`NavPathfinder` ≈ 4,02 MiB, iş parçacığı güvenli değil). `docs/03` CLI-05/CLI-08 (adım ≤ `MaxStepMeters × 1,10 + 0,15`).
- `BotCore/NavPath.h:64-81` `NavPathfinder::Find(grid, start, goal, NavSearchParams, NavPathResult &, const NavCostField * = nullptr)`; durumlar `Found/NoPath/NodeLimit/InvalidStart/InvalidGoal` (`:34-40`); `NavSearchParams::maxNodes = 20000` (`:44-48`); `NavPathResult` (`cells`, `length`, `expanded`; `:50-57`). Aynı örnek ardışık sorgularda deterministiktir. Bu planda `field = nullptr`.
- `BotCore/NavSmooth.h:84-` `NavSmoothPath(grid, path, params, out)`; `NavSmoothResult{waypoints, length}`; ilk ve son hücre korunur; `NavSmoothParams::maxLookahead = 64`. Düzleştirilmiş segmentler `Walk` süpercover'ında temizdir: 0/33 365 engelli (`docs/12` §13.1 ölçümü `[V]`, `NavSegmentAudit_Planner` testi).
- `BotCore/NavStuck.h:578-` `NavRoutePoint{x,z}` ve `NavRouteProgressM(pts, n, x, z)` (polilin izdüşümünün başlangıçtan kümülatif mesafesi; n < 2 veya NaN → 0).
- `BotCore/NavChordGuard.h:21-37` `kChordIgnoreMeters = 0,08`, `ChordVerdict{Ok,Skipped,BlockedCell,OutOfBounds}`; `CheckMoveChord(const NavGrid *, x0, z0, x1, z1)` (`:39-`); `grid == nullptr` → `Skipped`.
- `BotCore/BotMotion.h:24-27` `MaxStepMeters(speedField, periodMs)` (45 → 1500 ms'de 6,75 m); `:12-15` `kWalkSpeedField = 45`, `kMovePeriodMs = 1500`; `:44-` `StepToward`.
- **Yürütücü nicemlemesi (gerçek kod, bu planın tasarımını belirler):** `GameServer/Bot/ActionExecutor.cpp:91-96` paket konumu `uint16(w * 10.0f + 0.5f) / 10.0f`; `:124-125` F5-61 guard'ı kirişi **botun gerçek konumundan nicemlenmiş paket konumuna** denetler; `:142` reddedilirse yürüyüş biter. Yani `NavDrive` kirişi nicemlenmemiş uç noktayla "Ok" bulup yürütücüde reddedilmesin diye **nicemlenmiş** uçla denetlemelidir.
- `Tests/BotCoreTests/NavChordGuardTests.cpp:49-66` `LoadZone71OrSkip` (`build/nav/zone71.navgrid`; `MainComponentCells() == 88508`), `:69-119` `CollectWalk`/`RandomNear`/`PercentileDouble`/`Quantise` yardımcıları (dosya-yerel; bu planın test dosyasına **kopyalanır**), `:256-343` planlayıcı yolu kiriş testi (kalıp), `:346-` perf testi (kalıp). `Tests/BotCoreTests/NavArenaTests.cpp:103-125` dosya-yerel `NearestWalk`; `:340-342` doğuş/arena noktaları: Karus (1385; 1095), El Morad (635; 925), arena A (1274; 890).
- Ölçüm (geçici deneme, commit edilmez, `[V: WSL g++ -O2, zone 71, maxSlope 0,45]`): `NearestWalk` hücreleri Karus (346, 273), El Morad (158, 231), arena (318, 222), üçü `Walk`; `Find` her ikisinde `Found`, `expanded` 883 (Karus) / 2644 (El Morad), düzleştirilmiş yol 276,0 m (10 nokta) / 701,3 m (11 nokta); ters yön de `Found`. Bu sayılar test bantlarının kaynağıdır (§5 adım 7).
- `Tests/BotCoreTests/MiniTest.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`); filtre: `BotCoreTests.exe <ad-parçası>`; liste: `--list` (şu an 276 satır).
- `tools/check-perception-contract.py`: `BotCore/` altındaki yeni başlık `windows.h`/`stdafx.h`/`GameServer`/`shared` içermemelidir (ADR-0016).

### Tasarım kararları (otonom döngüde Claude kararı — gözden geçirilmeli; ADR-0006 Ek F5-62)

1. **Nicemlenmiş uçla kiriş denetimi `[D]`.** `NavDrive` her adım noktasını yürütücünün nicemlemesiyle (`NavQuantiseM`, `uint16(w*10+0,5)/10`, float) üretir ve kirişi botun **verilen** konumundan o nicemlenmiş noktaya denetler. Böylece `NextStep` "Step" dediği kiriş yürütücüdeki F5-61 guard'ından da geçer.
2. **Hedef önce nicemlenir.** `BeginGoto` hedefi nicemler ve hedef hücresini **nicemlenmiş** noktadan hesaplar (3,98 m → 4,0 m ile hücre değişebilir); `Walk` değilse `InvalidGoal` (ADR-0021 madde 1: en yakın yürünebilir hücreye alınmaz).
3. **Rota uçları.** Rota `[bot, c0, c1..c(k-1), ck, hedef]` (`c` = düzleştirilmiş hücre merkezleri; `c0` botun hücresi, `ck` hedef hücresi) biçiminde kurulur: düzleştirme hücre merkezleri arasında doğrulanmıştır, botun tam konumundan merkeze ve merkezden tam hedef noktasına olan parçalar tek hücre içindedir. **Kestirme:** `bot → c1` (ya da `c1` yoksa `bot → hedef`) kirişi `CheckMoveChord` ile `Ok` ise `c0` atılır; aynı şekilde sondan `c(k-1) → hedef` `Ok` ise `ck` atılır. Hücre merkezine geri dönüp sonra yola girmek 2,8 m'ye kadar gereksiz yürüyüştür; kestirme yalnızca **doğrulanmış** kirişle yapılır.
4. **Kısaltma ve yeniden plan.** Tam adım kirişi engele değerse adım **rotanın sonraki ara noktasına** kısaltılır (`truncated`); o da değerse `Blocked`. `Blocked` alan çağıran `Replan` çağırır; `Replan` **en çok 1 kez** çalışır (`kMaxGotoReplans = 1`), ikincisi `ReplanLimit` döner ve sürücü kapanır. Sonsuz yeniden plan döngüsü yoktur.
5. **Başlangıç `Walk` değilse planlanmaz (`InvalidStart`).** F5-61 D5 muafiyeti kirişi korur ama `Find` `Walk` olmayan başlangıçtan planlayamaz; en yakın yürünebilir hücreye taşıma bu planın konusu değildir (çağıran reddeder; F5-70'te `invalid_start`).

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavDrive.h` (yeni; saf mantık; yalnızca standart kütüphane + kardeş başlıklar; global/static yok; saat yok): `NavQuantiseM`, `NavDriveParams`, `NavPlanStatus`, `NavDriveStep`, `NavDrive` (`Reset`, `BeginGoto`, `NextStep`, `Replan`, erişimciler).
2. `Tests/BotCoreTests/NavDriveTests.cpp` (yeni): §5 adım 6-7'deki 13 `TEST_CASE`.
3. İki `.vcxproj`'a birer satır.

**Kapsam dışı (yapılmayacak)**

- `GameServer/`, `AIServer/`, `shared/` altında **hiçbir** değişiklik: `NavService`, `ActionExecutor`, `BotSession`, `BotManager`, `ScriptPlan.h`, `ScriptTests.cpp`, `/bot goto`, `SharedPathfinder()` (hepsi F5-70).
- `Follow` kipi, hız kestirimi, hedef takibi, takılma tespiti/kurtarma (`NavStuckMonitor`, `NavProgressAssessor` bağlaması) → F5-63. `NavDriveStep::routeProgressM`/`distToGoalM` alanları yalnızca F5-63'ün besleyeceği değerlerdir; bu planda tüketen yoktur.
- Bütçe zamanlayıcı, yol önbelleği, `NAV_*` telemetri, `PERF_SAMPLE` nav payı → F5-64. Durum temizliği nedenleri (`NavResetReason`), konum sıçraması sonlandırması (ADR-0021 madde 3) → F5-65. Bu planda `Reset()` parametresizdir.
- Arena maliyet alanı (`NavCostLayer`), tehlike katmanı, `NavReach` yargısı, hedef bırakma kararı: yok (`Find` her zaman `field = nullptr`).
- `BotCore/NavGrid.h`, `NavPath.h`, `NavSmooth.h`, `NavStuck.h`, `NavSegment.h`, `NavChordGuard.h`, `BotMotion.h` **değişmez**.
- `docs/` (Claude: `docs/12` §13.4/§13.5, ADR-0006 Ek F5-62, `docs/STATUS.md`), `tools/`.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavDrive.h` | yeni | yalnızca ASCII + CRLF, tab, Allman |
| `BotCore/BotCore.vcxproj` | değiştir | tek satır `    <ClInclude Include="NavDrive.h" />`, `NavChordGuard.h` satırından (`:80`) sonra; UTF-8 BOM ve CRLF korunur |
| `Tests/BotCoreTests/NavDriveTests.cpp` | yeni | yalnızca ASCII + CRLF |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | tek satır `    <ClCompile Include="NavDriveTests.cpp" />`, `NavChordGuardTests.cpp` satırından (`:89`) sonra; BOM/CRLF korunur |

Yeni `.cpp` GameServer dosyası yok: `GameServer/proj-GameServer.vcxproj` değişmez. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-62 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucu `[UP]` ise `./tools/run-servers.sh stop`. `python3 tools/nav-export.py` (gerçek harita testleri için `build/nav/zone71.navgrid`). §2'deki imzaları (`NavPathfinder::Find`, `NavSmoothPath`, `NavRoutePoint`, `NavRouteProgressM`, `CheckMoveChord`) açıp doğrula; sapma varsa **dur** (`Durum: UYGULANIYOR (BLOKE)`). Başlangıç test sayısı: `./build/bin/x86-Release/Tests/BotCoreTests.exe --list | wc -l` (rapora yaz).
2. **`BotCore/NavDrive.h`** — sözleşme sabittir, gövde uygulayıcıya aittir:

   ```cpp
   #pragma once
   // Path-following drive for the real move packets (F5-62; docs/12 s13.1, s13.4). Pure logic:
   // standard library and sibling headers only, no global/static state, no clock. One drive per bot.
   // F5-62 carries the Goto mode only; F5-63 adds Follow and the stuck members.
   #include "NavChordGuard.h"
   #include "NavPath.h"
   #include "NavSmooth.h"
   #include "NavStuck.h"      // NavRoutePoint, NavRouteProgressM (F5-57)
   #include <cstdint>
   #include <vector>
   namespace BotCore
   {
   	constexpr int kMaxGotoReplans = 1;
   	constexpr float kMinTruncStepM = 0.15f;   // a truncated step must advance at least this far

   	// Mirrors the executor packet quantisation (ActionExecutor.cpp: uint16(w * 10.0f + 0.5f) / 10.0f).
   	// Precondition 0 <= w <= 6553.5 (callers reject others before quantising).
   	inline float NavQuantiseM(float w);

   	enum class NavDriveMode { Off, Goto };
   	enum class NavPlanStatus { None, Planned, InvalidStart, InvalidGoal, NoPath, NodeLimit, ReplanLimit };

   	struct NavDriveParams
   	{
   		NavSearchParams search;   // P-NAV-MAX-NODES 20000
   		NavSmoothParams smooth;   // P-NAV-SMOOTH-LOOKAHEAD 64
   	};

   	struct NavDriveStep
   	{
   		enum Kind { None, Step, Arrived, Blocked } kind = None;
   		float x = 0.0f, z = 0.0f;      // packet position, already quantised (the executor re-quantises idempotently)
   		float routeProgressM = 0.0f;   // route progress of (x, z); F5-63 feeds NavProgressAssessor
   		float distToGoalM = 0.0f;      // Euclid distance from (x, z) to the goal point
   		bool  truncated = false;       // the full step was cut at a route vertex because its chord was blocked
   	};

   	class NavDrive
   	{
   	public:
   		void Reset();                       // mode Off, route empty, counters 0
   		bool Active() const;
   		NavDriveMode Mode() const;

   		// Plans bot -> goal with the caller's pathfinder (caller guarantees single-thread use) and arms the
   		// walk. Goal is quantised first (design decision 2); route per design decision 3. On any status
   		// other than Planned the drive is Reset (inactive). Replans() = 0 after Planned.
   		NavPlanStatus BeginGoto(const NavGrid &, NavPathfinder &, float botX, float botZ,
   			float goalX, float goalZ, const NavDriveParams &);

   		// Next move packet from the bot's CURRENT position (rules below). Not Active, non-finite input or
   		// maxStepM <= 0 -> kind None. The drive does not move the bot: the caller feeds the next position.
   		NavDriveStep NextStep(const NavGrid &, float botX, float botZ, float maxStepM);

   		// After a Blocked step: re-plans from the bot's current position to the SAME (quantised) goal.
   		// Not Active -> None; Replans() >= kMaxGotoReplans -> Reset + ReplanLimit; otherwise as BeginGoto
   		// with Replans() incremented on Planned (and Reset on failure).
   		NavPlanStatus Replan(const NavGrid &, NavPathfinder &, float botX, float botZ, const NavDriveParams &);

   		const std::vector<NavRoutePoint> & Route() const;
   		float RouteLengthM() const;         // polyline length; 0 when inactive
   		float GoalX() const;                // quantised goal
   		float GoalZ() const;
   		int   Replans() const;
   		int   PlanExpanded() const;         // closed nodes of the last Find (for the F5-70 log line)
   		int   PlanWaypoints() const;        // smoothed waypoints of the last plan, both ends included
   	};
   }
   ```

   `BeginGoto` kuralları (sırayla): (a) bot veya hedef koordinatı sonlu değil / < 0 / `w * 10 > 65535` ise — bot için `InvalidStart`, hedef için `InvalidGoal`; (b) hedefi `NavQuantiseM` ile nicemle; başlangıç hücresi `CellOf(botX/Z)`, hedef hücresi nicemlenmiş hedeften; `grid.Walk` değilse sırasıyla `InvalidStart` / `InvalidGoal` (ızgara dışı dahil; ADR-0021: en yakın hücreye alma yok); (c) `Find` + `NavSmoothPath`; `Found` dışı durum `NoPath`/`NodeLimit`'e eşlenir; (d) rota: §2 karar 3 (başlangıç ve hedef hücre aynıysa rota `[bot, hedef]`); (e) `PlanExpanded`/`PlanWaypoints` doldurulur.

   `NextStep` kuralları (sırayla): (a) `p = NavRouteProgressM(rota, bot)`, `toplam = RouteLengthM()`; (b) `kalan = toplam − p ≤ maxStepM` ise aday = hedef noktası ve `goal = true`; değilse aday = rotanın `p + maxStepM` konumundaki nokta (çok parçalı polilin üzerinde, ara noktalarda durmadan); aday `NavQuantiseM` ile nicemlenir; (c) `CheckMoveChord(&grid, botX, botZ, adayX, adayZ)` `Ok` ise dön: `Arrived` (goal) ya da `Step`; (d) `Ok` değilse **kısaltma:** `p + kMinTruncStepM` ≤ kümülatif mesafe < (`goal ? toplam : p + maxStepM`) koşulunu sağlayan **ilk ara nokta** (rotanın 1..n−2 indisleri) nicemlenir ve aynı kiriş denetimi yapılır; `Ok` ise `Step` + `truncated = true`; ara nokta yoksa ya da yine `Ok` değilse `Blocked` (`x/z` botun konumu). `Skipped` (ızgara yok) bu planda oluşamaz (`const NavGrid &`).

3. `.vcxproj` iki tek satır (§4).
4. `NavQuantiseM`, `Route()` vb. küçük yardımcılar `inline`; `NavDrive` sınıfı başlıkta tanımlanır (`.cpp` yok; `BotCore` başlık-yalnızca kalır).
5. **Test yardımcıları** (`NavDriveTests.cpp`, anonim ad alanı; `NavChordGuardTests.cpp:49-119` ve `NavArenaTests.cpp:103-125`'ten **kopyalanır**, orijinallere dokunulmaz): `LoadZone71OrSkip(grid, tag)` (`NAVDRIVE <tag>: SKIPPED ...` satırı yazar), `CollectWalk`, `RandomNear`, `PercentileDouble`, `NearestWalk`; ek olarak sentetik ızgara üreticileri (aşağıda) ve **yürüyüş simülatörü**: `Walk(drive, grid, startX, startZ, maxStep, ...)` her turda `NextStep` çağırır, `Step`/`Arrived` ise bot konumunu `(x, z)`'ye taşır, `Blocked` ise kaydeder; sonuç yapısı: paket sayısı, `Arrived` mı, `Blocked` sayısı, kısaltılmış adım sayısı, en uzun adım, adım kirişlerinin her biri için **bağımsız örnekleme denetimi** (kiriş boyunca 0,25 m aralıkla noktalar; her noktanın hücresi `grid.Walk`). En çok 2000 tur (sonsuz döngü koruması; aşılırsa `Arrived == false`).
6. **Sentetik testler** (`n = 64`, `unit = 4,0`, yükseklik 0; kenar hücreleri `Event 0` olmalı: yalnızca kenara değmeyen en büyük `Event 1` bileşeni `Walk` olur, `NavGrid::Build`):
   - `NavDrive_Quantise`: `NavQuantiseM(0,04) == 0,0`; `(0,05) == 0,1` (yürütücüyle aynı `uint16(w*10+0,5)`); `(123,456) == 123,5`; nicemlenmiş değer üzerinde idempotent; hata ≤ 0,05 + 1e-4.
   - `NavDrive_Goto_OpenField`: engelsiz iç bölge (1..62), (20,20) merkezinden (200,200)'e; her tur `Step`, sonuncusu `Arrived`; son konum = nicemlenmiş hedef; paket sayısı = `ceil(RouteLengthM() / 6,75)` ± 1; hiçbir adım uzunluğu `6,75 + 0,15 + 1e-3`'ü aşmaz; `Replans() == 0`; hiç `truncated` yok.
   - `NavDrive_Goto_AlreadyThere`: bot hedef noktasında: ilk `NextStep` `Arrived`, `x/z` = hedef; `Replans() == 0`.
   - `NavDrive_Goto_Detour_AllChordsWalk`: ortada tek hücre kalınlığında uzun duvar (örn. `x = 31`, `z = 2..45`; uçlarda geçit): rota uzunluğu ≥ düz mesafe; yürüyüş `Arrived`, `Blocked == 0`; her adım kirişi bağımsız örnekleme denetiminden geçer.
   - `NavDrive_Goto_ChordBlockedTruncates`: aynı duvar, **`maxStepM = 400`** (tek adım tüm rotayı kapsar): ilk `NextStep`'te aday (hedef) duvarı keser → `Step`, `truncated == true`, konum rotanın **ilk ara noktasının nicemlenmişi**, kiriş `Ok`; döngü sürer: tüm adımlar `truncated` ya da son `Arrived`; adım sayısı = ara nokta sayısı + 1; bot varır.
   - `NavDrive_Goto_StatusMapping`: `Walk` olmayan hedef (duvar hücresi) -> `InvalidGoal`; ızgara dışı ve negatif hedef -> `InvalidGoal`; `Walk` olmayan başlangıç -> `InvalidStart`; `NoPath`: iç bölgeyi ikiye bölen sütunun sağındaki tüm hücrelerin yüksekliği 10 m yapılır (`x >= 32`; sol yarı 0 m; iki yarı da `Walk` kalır, ama 10 m > `0,45 x 4` olduğu için aralarındaki her kenar `EdgeOpen`te kapalıdır) ve sol yarıdan sağ yarıya `BeginGoto` -> `NoPath`; açık ızgarada `search.maxNodes = 8` ile uzak hedef -> `NodeLimit`; her durumda `Active() == false`, `Route()` boş.
   - `NavDrive_Goto_EndpointsOffCentre`: 300 çift (`Rng(20261003u)`), her iki uç da hücre içinde **rastgele** (`0,1..3,9 m` içeride, nicemlenmiş), 64×64 ızgarada 12 sabit dikdörtgen engel; hücresi `Walk` değilse yeniden çek; `Planned` olanların hepsi `Arrived`, `Blocked == 0`, `Replans() == 0`, bağımsız örnekleme denetimi ihlali 0; `NAVDRIVE offcentre pairs=<n> planned=<p> blocked=0` satırı. `NoPath` olabilir mi: engeller bağlantıyı koparmayacak yerleştirilir (uygulayıcı `Planned == pairs` bekler; değilse rapora yaz).
   - `NavDrive_Blocked_Replan`: açık ızgarada `BeginGoto` (rota `[bot, hedef]`); ardından `NextStep` **ortasında duvar olan başka bir ızgarayla** (aynı boyut, aynı `Walk` uçları) çağrılır → `Blocked` (ara nokta yok); `Replan` (duvarlı ızgarayla) → `Planned`, `Replans() == 1`, yürüyüş `Arrived`; ikinci bir `Blocked` + `Replan` → `ReplanLimit`, `Active() == false`.
   - `NavDrive_OffRoute`: yürüyüşün ortasında bot rotadan 2 m yana itilir (yürünebilir hücre): sonraki adımlar izdüşümden sürer, `routeProgressM` geriye atlamaz (`>= önceki − 2,0`), varır.
   - `NavDrive_Reset_Inactive`: `Reset()` sonrası `NextStep` → `None`; `Active() == false`; `Route()` boş; `RouteLengthM() == 0`; `Replan` → `None`. `maxStepM <= 0` ve NaN konum → `None`.
7. **Gerçek harita testleri** (`LoadZone71OrSkip`; yoksa `SKIPPED`, `K3` bunu kabul etmez):
   - `NavDrive_RealMap_RespawnReturn`: `NearestWalk` ile Karus (1385; 1095), El Morad (635; 925) → arena (1274; 890); başlangıç ve hedef **hücre merkezi**; `BeginGoto` `Planned`; yürüyüş (`maxStep 6,75`) `Arrived`, `Blocked == 0`, bağımsız örnekleme ihlali 0; `RouteLengthM()` bantları: Karus **268-284 m**, El Morad **680-722 m** (ölçüm 276,0 / 701,3, §2); paket sayısı ≤ `ceil(L / 6,75) + PlanWaypoints() + 2` ve ≥ `ceil(L / 6,75)` (geçici ölçüm prototipi: ikisinde de tam `ceil`, 41 ve 104 paket, kısaltma 0); **ara noktada durmama kanıtı:** ara noktayı kapsayan **kısaltılmamış** adım sayısı `crossed >= 1` (prototip: Karus 8, El Morad 8; `NavRouteProgressM` ile adımdan önceki/sonraki ilerleme ve `Route()` kümülatif mesafeleriyle hesaplanır); satır: `NAVDRIVE respawn karus|elmorad: route_m=<m> waypoints=<w> expanded=<n> packets=<p> crossed=<c> truncated=<t> eta_s=<L/4,5> blocked=0` (`eta_s` bir tahmindir, ölçülmüş süre değildir).
   - `NavDrive_RealMap_Random`: 300 `near64` çift (`Rng(20261003u)`; uçlar hücre içinde rastgele, nicemlenmiş, hücresi `Walk` değilse yeniden çekilir): `Planned` ≥ %85 (kalanı yalnızca `NoPath` veya `NodeLimit`: eğimle kopmuş cepler; `NodeLimit` geçerli bir "planlanmadı" sonucudur, çünkü A* 20 000 düğümde durunca `NavPath.h:31` "bilinmiyor" der, "ulaşılamaz" demez), `NoPath + NodeLimit == pairs − Planned`, `InvalidStart/InvalidGoal == 0`; planlananların tümü `Arrived`; çözülemeyen `Blocked` (Replan sonrası da) **0**; kaç `Blocked` olayı/`Replan` olduğu satırda: `NAVDRIVE random pairs=<n> planned=<p> nopath=<np> nodelimit=<nl> blocked_events=<b> replans=<r> unrecoverable=0 truncated=<t>`.
   - `NavDrive_Perf` (`#ifndef _DEBUG`): `near64` çiftlerinde 2000 `BeginGoto` (tek `NavPathfinder`, ilk çağrı havuz ayırması dışarıda tutulur: önce 1 ısınma çağrısı) p95 ≤ **2,0 ms** (AC-NAV-02); `NextStep` p95 ≤ **0,05 ms**; satır `NAVDRIVE perf: begin_p95_ms=<x> step_p95_ms=<y>`.
8. Derle/test (§7); `check-perception-contract.py` rc=0. Sunucu çalıştırılmaz. Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `./tools/build.sh Debug` rc=0; yeni derleyici uyarısı yok (`NavDrive.h` ve test dosyası için)
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; on üç yeni `NavDrive_*` test adı `[ OK ]` (§5 adım 6-7: `Quantise`, `Goto_OpenField`, `Goto_AlreadyThere`, `Goto_Detour_AllChordsWalk`, `Goto_ChordBlockedTruncates`, `Goto_StatusMapping`, `Goto_EndpointsOffCentre`, `Blocked_Replan`, `OffRoute`, `Reset_Inactive`, `RealMap_RespawnReturn`, `RealMap_Random`, `Perf` = 13 ad); toplam test sayısı = başlangıç (§5 adım 1) + 13; mevcut testlerin hiçbiri değişmez
- [ ] K3: gerçek-harita testleri `SKIPPED` **değil**; raporda `NAVDRIVE respawn karus ...`, `NAVDRIVE respawn elmorad ...`, `NAVDRIVE random ...` ve `NAVDRIVE perf ...` satırları (`blocked=0`, `unrecoverable=0`, her iki doğuş `Planned`, `Arrived`; rota uzunlukları §5 adım 7 bantlarında)
- [ ] K4: `git diff --stat gece/2026-10-02...bot/F5-62` yalnızca §4'teki 4 dosya + bu plan dosyası; `GameServer/`, `AIServer/`, `shared/`, `docs/`, `tools/`, `BotCore/Nav{Grid,Path,Smooth,Stuck,Segment,ChordGuard}.h`, `BotCore/BotMotion.h` farkı **0**; `git diff --check` boş
- [ ] K5: `grep -n -E "windows.h|stdafx|GameServer|shared/|static |malloc|std::chrono" BotCore/NavDrive.h` boş (`constexpr` ad alanı sabitleri `static` değildir); `python3 tools/check-perception-contract.py` rc=0
- [ ] K6: `NextStep` `Step`/`Arrived` döndürdüğü her adımda, `NavQuantiseM` uç noktası ve botun verilen konumuyla `CheckMoveChord == Ok` olduğu testte **bağımsız** kanıtlanır: gerçek haritadaki 300 çift + iki doğuş yürüyüşünde adım kirişlerinin `0,25 m` örnekleme denetimi ihlali 0 (`NAVDRIVE ... blocked=0` satırları)
- [ ] K7: yeni dosyalar yalnızca ASCII + CRLF (`file BotCore/NavDrive.h Tests/BotCoreTests/NavDriveTests.cpp` çıktısı raporda); `BotCore.vcxproj` ve `BotCoreTests.vcxproj` UTF-8 BOM + CRLF korunmuş (`file` çıktısı)
- [ ] K8: sunucu ikilisi değişmez: `git diff --stat gece/2026-10-02...bot/F5-62 -- GameServer shared AIServer` boş (bot sistemi kapalıyken ve açıkken davranış farkı yok: bu plan sunucu koduna dokunmaz)

Bu planda Claude çalışma zamanı kriteri **yoktur** (sunucu çalıştırılmaz); çalışma zamanı doğrulaması F5-70'tedir.

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavDrive_|NAVDRIVE|tests,"
./tools/run-tests.sh Debug 2>&1 | grep -E "NavDrive_|tests,"
./build/bin/x86-Release/Tests/BotCoreTests.exe --list | wc -l
python3 tools/check-perception-contract.py
grep -n -E "windows.h|stdafx|GameServer|shared/|static |malloc|std::chrono" BotCore/NavDrive.h
file BotCore/NavDrive.h Tests/BotCoreTests/NavDriveTests.cpp BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
git diff --stat gece/2026-10-02...bot/F5-62
git diff --check gece/2026-10-02...bot/F5-62
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3: CRLF, tab, Allman, yorumlar İngilizce, yeni dosyalar ASCII; konsol/`printf` yalnızca test çıktısında (`NAVDRIVE ...` satırları, mevcut `NAVCHORD`/`NAVARENA` kalıbı).
- **Bot avantajı yasağı (`docs/03` §13, `docs/13` §3):** yol, botun kendi konumundan ve herkesin sahip olduğu zone verisinden (F5-59 ızgarası) hesaplanır; hedef komutla verilir. `NavDrive` hız/sıklık belirlemez: adım uzunluğu çağıranın verdiği `maxStepM`'dir (yürütücüde `MaxStepMeters`; CLI-05/CLI-08 `SubmitMove` guard'ında korunur).
- Thread: `NavPathfinder` iş parçacığı güvenli değildir; `BeginGoto`/`Replan` çağıranın verdiği örneği kullanır. Paylaşılan örnek ve thread denetimi F5-70'tedir; `NavDrive.h` içinde thread/saat/global yoktur.
- **Dürüstlük:** testler rota/adım mantığını ve gerçek haritada kirişlerin `Walk` olduğunu sınar; botun oyunda engelsiz yürüdüğü F5-70 çalışma zamanı kriterleri ve F5-66 ile kanıtlanır. `eta_s` (`L / 4,5`) tahmindir. Arena sınırı bu planda yoktur (`field = nullptr`, düz mesafe; `docs/reports/plan-bagimlilik-F5-2026-10-03.md` "kapsam boşluğu").
- Bantlar (`268-284`, `680-722`, `≥ %85`) ölçüme dayanır (§2); gerçek değerler bandın dışındaysa **bandı gevşetme**: rapora yaz ve dur (`BLOKE`); neden çoğunlukla başlangıç hücresi/hedef hücresi seçimindeki farktır.
- Beklenmedik durumda (imza farkı, `NavSmoothPath`/`Find` davranış değişimi, `Blocked` beklenenden sık, ek dosya gerekiyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANIYOR (BLOKE)
- Branch / commit'ler: `bot/F5-62` (taban `gece/2026-10-02` @ `271e17e`); kod + bu rapor tek commit'te
- Başlangıç / bitiş test sayısı (`--list | wc -l`): 276 → 289 (13 yeni `NavDrive_*`)
- Değişen dosyalar ve neden:
  - `BotCore/NavDrive.h` (yeni): `NavQuantiseM`, `NavDriveParams`, `NavPlanStatus`, `NavDriveStep`, `NavDrive` (`Reset`/`BeginGoto`/`NextStep`/`Replan` + erişimciler). Saf mantık, başlık-yalnızca, `NavChordGuard.h`/`NavPath.h`/`NavSmooth.h`/`NavStuck.h`.
  - `Tests/BotCoreTests/NavDriveTests.cpp` (yeni): §5 adım 6-7'deki 13 `TEST_CASE` + dosya-yerel yardımcılar.
  - `BotCore/BotCore.vcxproj`: `NavChordGuard.h`'tan sonra `NavDrive.h` (tek satır).
  - `Tests/BotCoreTests/BotCoreTests.vcxproj`: `NavChordGuardTests.cpp`'tan sonra `NavDriveTests.cpp` (tek satır).
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
  NavDriveTests.cpp
  BotCoreTests.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  `Release` ve `Debug` rc=0; `NavDrive.h`/`NavDriveTests.cpp` için yeni uyarı yok (kalan uyarılar eski `GameServerDlg.cpp` C4267, `UpgradeHandler.cpp` C4789).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔: `Release` + `Debug` rc=0, iki dosyada yeni uyarı yok.
  - K2 ✘: 13 yeni testin 12'si `[ OK ]`; `NavDrive_RealMap_Random` FAIL; `289 tests, 1 failed` (Release ve Debug).
  - K3 ~: satırlar üretildi; iki doğuş `Planned`+`Arrived`, `blocked=0`, `unrecoverable=0`; ama `random` satırında `nodelimit=3` (aşağıdaki engelleyici).
  - K4 ✔: yalnızca §4'teki 4 dosya + plan; `GameServer`/`AIServer`/`shared`/`docs`/`tools` farkı 0; `git diff --check` boş.
  - K5 ✔: `grep -n -E "windows.h|stdafx|GameServer|shared/|static |malloc|std::chrono" BotCore/NavDrive.h` boş; `check-perception-contract.py` `RESULT: PASS` rc=0.
  - K6 ✔: `NAVDRIVE random` (300 çift) + iki doğuş yürüyüşünde 0,25 m örnekleme ihlali 0.
  - K7 ✔: yeni iki dosya ASCII + CRLF; iki `.vcxproj` UTF-8 BOM + CRLF korunmuş.
  - K8 ✔: sunucu ikilisi değişmez.
- Gerçek-harita test satırları (`NAVDRIVE ...`):
  ```
  NAVDRIVE offcentre pairs=300 planned=300 blocked=0
  NAVDRIVE respawn karus: route_m=276.0 waypoints=10 expanded=883 packets=41 crossed=8 truncated=0 eta_s=61.3 blocked=0
  NAVDRIVE respawn elmorad: route_m=701.3 waypoints=11 expanded=2644 packets=104 crossed=9 truncated=0 eta_s=155.8 blocked=0
  NAVDRIVE random pairs=300 planned=295 nopath=2 blocked_events=1 replans=1 unrecoverable=0 truncated=0
  NAVDRIVE perf: begin_p95_ms=0.8242 step_p95_ms=0.0007
  ```
- Engelleyici (BLOKE): `NavDrive_RealMap_Random`, planın `NodeLimit == 0` (ve "kalanı yalnızca `NoPath`") beklentisini karşılamıyor; ölçüm `planned=295`, `nopath=2`, **`nodelimit=3`** (Release ve Debug aynı). Neden `NavDrive` değil: A*, başlangıcı ana `EdgeOpen` bölgesinde, hedefi aynı *olay* bileşeni içinde fakat eğimle kopmuş bir cep olan çiftlerde 20 000 düğümü doldurup `NavPathfinder` `NodeLimit` döndürüyor (`NavPath.h:31`: "unknown", "unreachable" değil); `BeginGoto` bunu §5 adım 2'ye göre `NavPlanStatus::NodeLimit`'e eşliyor. Ölçülen çiftler (hücre; ham konum): (391,295)→(331,338) [(1564,1;1181,2)→(1327,6;1352,6)], (255,272)→(290,229) [(1022,7;1091,2)→(1161,6;918,1)], (382,305)→(354,254) [(1530,2;1220,2)→(1418,5;1016,3)]; üçü de arena/plato çevresi. Plan bu iddiayı `reach 85 508/88 508` ile birlikte veriyor, yani ~3 000 kopuk `Walk` hücresi olduğunu biliyor; ne var ki bunlara nişan alan geniş-bölge başlangıçları zorunlu olarak `NodeLimit` üretir (dolayısıyla beklenti, F5-69 `maxSlope 0,45` ile matematiksel olarak da tutarsız). Diğer her şey planla uyumlu: iki doğuş rota uzunluğu plandaki 276,0 / 701,3 m ile **birebir**; `crossed` 8/9 (prototip 8/8+); `Perf` eşikleri rahat (0,82 / 0,0007 ms).
- Plandan sapmalar ve gerekçeleri:
  1. `NavDrive_Goto_EndpointsOffCentre`'a planın açıkça yazmadığı `CHECK_EQ(planned, pairs)` / `CHECK_EQ(arrived, planned)` eklendi (plan beklentisi "uygulayıcı `Planned == pairs` bekler"); ölçüm 300/300, geçti.
  2. `NavQuantiseM` içinde kapsam dışı koordinatlar için ön koşul denetimi (`BeginGoto` yapar).
  3. Plan taslağındaki `static bool ValidCoord` yerine `bool ValidCoord(...) const` kullanıldı; K5 `grep "static "` temiz kalsın diye.
- Açık sorular (**çözüldü: (a)** — Claude kararı, aşağıdaki "BLOKE cevabı"):
  1. (a) **seçildi:** Random testte `NodeLimit` geçerli bir "planlanmadı" sonucu sayılır: kabul `planned ≥ %85` ve `nopath + nodelimit == pairs − planned`, `InvalidStart/InvalidGoal == 0`, `unrecoverable == 0`; ya da
  2. (b) Random çiftler `NavReach` ile `start`'ın `EdgeOpen` bileşenine kısıtlanır (test `NavDrive` yol izlemesini ölçer, ulaşılabilirliği değil); ya da
  3. (c) Plan, random endpoint ofset üretiminin RNG sözleşmesini yazar (uygulama `0,1 + 0,1·NextBelow(39)`; farklı ofset protokolü farklı hücre çiftleri ve farklı `NodeLimit` sayısı verir). `NodeLimit == 0` korunacaksa çift seçiminin cep hücrelerini dışlaması gerekir.

### Tur 2 (Doğrulama Turu 1 düzeltmeleri)

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-62` (taban `gece/2026-10-02`); düzeltme yalnızca `Tests/BotCoreTests/NavDriveTests.cpp`, `BotCore/NavDrive.h`'ye dokunulmadı.
- Talimat uygulaması (aynen):
  1. `printf` biçimi `... nopath=%d nodelimit=%d blocked_events=%d ...` (`nodelimit` artık `nopath`'ten sonra).
  2. `CHECK_EQ(nodelimit, 0);` silindi.
  3. `CHECK_EQ(nopath, pairs - planned);` → `CHECK_EQ(nopath + nodelimit, pairs - planned);`.
  Diğer CHECK'ler aynen korundu (`planned*100 >= pairs*85`, `badstart`/`badgoal` 0, `unrecoverable` 0, `violations` 0); hiçbir eşik gevşetilmedi.
- Değişen dosyalar ve neden: `Tests/BotCoreTests/NavDriveTests.cpp` (+3/−4): yalnızca `NavDrive_RealMap_Random` karar satırı ve `printf` başlığı; test beklentisi `NodeLimit`'i geçerli "planlanmadı" sonucu sayacak biçimde düzeltildi (BLOKE cevabı (a)).
- Derleme sonucu (`tools/build.sh Release` / `Debug` son satırları):
  ```
  NavDriveTests.cpp
  BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  ```
  NavDriveTests.cpp
  BotCoreTests.vcxproj -> ...\build\bin\x86-Debug\Tests\BotCoreTests.exe
  ```
  İkisi rc=0; `NavDrive.h`/`NavDriveTests.cpp` için yeni uyarı yok (kalan uyarılar eski `GameServerDlg.cpp` C4267, `UpgradeHandler.cpp` C4789).
- Test sonucu: `./tools/run-tests.sh Release` ve `Debug` → **`289 tests, 0 failed`** (13 `NavDrive_*` adı `[ OK ]`); `--list | wc -l` = 289.
- Gerçek-harita test satırı (yeni `NAVDRIVE random`):
  ```
  NAVDRIVE random pairs=300 planned=295 nopath=2 nodelimit=3 blocked_events=1 replans=1 unrecoverable=0 truncated=0
  ```
- Kabul kriterleri öz-değerlendirme (bu tur):
  - K2 ✔: `Release` + `Debug` `289 tests, 0 failed`; 13 `NavDrive_*` `[ OK ]`.
  - K3 ✔: `NAVDRIVE respawn karus` / `elmorad` / `NAVDRIVE random` (`planned=295 ≥ %85`, `nopath + nodelimit = 2 + 3 = 5 = pairs − planned`, `unrecoverable=0`) / `NAVDRIVE perf` satırları üretildi; `blocked=0`, iki doğuş `Planned`+`Arrived`.
  - `python3 tools/check-perception-contract.py` rc=0 (K5).
- Plandan sapmalar: yok (talimat birebir uygulandı).
- Açık soru: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F5-62` @ `aac9f50` (3 commit: `b98452c` kod + test, `de53f91` Claude BLOKE cevabı, `aac9f50` Tur 2 test düzeltmesi). Gece modu (`AUTO_LOOP=1`, `AUTO_INTEGRATION_BRANCH=gece/2026-10-02`): birleştirme ve push yapılmadı, birleştirmeyi döngü betiği yapar. Çalışma ağacı temizdi; sunucular kapalıydı (`run-servers.sh status`: 0/3), sunucu açılmadı.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `build.sh Release` ve `Debug` rc=0. `NavDrive.h` ve `NavDriveTests.cpp` `touch` ile zorla yeniden derlendi: günlükte `NavDriveTests.cpp` satırı var, `warning` satırı 0 (iki yapılandırma) |
| K2 | ✔ | `run-tests.sh Release` ve `Debug`: `289 tests, 0 failed`; 13 `NavDrive_*` adının tamamı `[ OK ]` (Quantise, Goto_OpenField, Goto_AlreadyThere, Goto_Detour_AllChordsWalk, Goto_ChordBlockedTruncates, Goto_StatusMapping, Goto_EndpointsOffCentre, Blocked_Replan, OffRoute, Reset_Inactive, RealMap_RespawnReturn, RealMap_Random, Perf); `--list \| wc -l` = 289 = 276 + 13; mevcut test dosyaları diff'te yok |
| K3 | ✔ | Gerçek harita testleri `SKIPPED` değil (`build/nav/zone71.navgrid` mevcut). Doğrulayıcının kendi koşusu: `NAVDRIVE respawn karus: route_m=276.0 ... packets=41 crossed=8 truncated=0 blocked=0`; `respawn elmorad: route_m=701.3 ... packets=104 crossed=9 truncated=0 blocked=0` (bantlar 268-284 / 680-722 içinde, testte `CHECK`, `NavDriveTests.cpp:675-677`); `random pairs=300 planned=295 nopath=2 nodelimit=3 blocked_events=1 replans=1 unrecoverable=0 truncated=0` (295 ≥ %85, `nopath + nodelimit = 5 = pairs − planned`, `NavDriveTests.cpp:774-779`); `perf: begin_p95_ms=0.7776 step_p95_ms=0.0006` (eşik 2,0 / 0,05) |
| K4 | ✔ | `git diff --stat gece/2026-10-02...bot/F5-62`: `BotCore/NavDrive.h`, `BotCore.vcxproj` (+1), `BotCoreTests.vcxproj` (+1), `NavDriveTests.cpp`, plan dosyası ve `plans/README.md` (yalnızca kendi satırının durum sözcüğü). `GameServer shared AIServer docs tools BotCore/Nav{Grid,Path,Smooth,Stuck,Segment,ChordGuard}.h BotCore/BotMotion.h` farkı boş; `git diff --check` rc=0 |
| K5 | ✔ | `grep -n -E "windows.h\|stdafx\|GameServer\|shared/\|static \|malloc\|std::chrono" BotCore/NavDrive.h` boş (rc=1); `check-perception-contract.py` rc=0 |
| K6 | ✔ | `ChordSampleClean` (`NavDriveTests.cpp:170-185`) `CheckMoveChord`'tan bağımsız: 0,25 m aralıkla her noktanın hücresi `grid.Walk`; `Walk` simülatörü her `Step`/`Arrived` için çağırır (`:237`). 300 off-centre çift + 2 doğuş + 295 rastgele çift: `sampleViolations == 0` (`CHECK_EQ`), kayıt satırlarında `blocked=0` / `unrecoverable=0` |
| K7 | ✔ | `file`: `NavDrive.h` ve `NavDriveTests.cpp` "ASCII text, with CRLF line terminators"; iki `.vcxproj` "UTF-8 (with BOM) text, with CRLF line terminators"; `.vcxproj` diff'leri tek satırlık ekleme |
| K8 | ✔ | `git diff --stat ... -- GameServer shared AIServer` boş; sunucu kodu değişmedi |

- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. **Not (F5-70 için):** `NextStep` adım adayını rota üzerinde `p + maxStepM`'de alır (`BotCore/NavDrive.h:99`), kirişi ise botun gerçek konumundan çizer (`:104`). Bot rotadan yana `d` m itilmişse Öklid adım uzunluğu `√(maxStepM² + d²)` olur; 6,75 m adımda `d` ≈ 3,4 m'yi aşarsa CLI-05/CLI-08 sınırını (`maxStep × 1,10 + 0,15` = 7,575) aşar ve `SubmitMove` guard'ı reddeder. Bu planın testi yalnızca 2 m itmeyi sınar (`NavDrive_OffRoute`, 7,04 m, sınır içinde); sunucu bağlamasında (F5-70) ya da takılma kurtarmada (F5-63) büyük sapma `Replan`'a düşmeli ya da adım uzunluğu kırpılmalı. Şu an hata değil (sürücü sunucuya bağlı değil).
  2. **Not:** `NavDrive_RealMap_Random` beklentisi `NodeLimit == 0` yerine "`nopath + nodelimit == pairs − planned`" oldu; BLOKE cevabı (a) ile Claude kararı (plan sonundaki bölüm), eşik gevşetilmedi (`planned ≥ %85`, ölçüm %98,3). Uygulayıcı Tur 2'de talimatı aynen uyguladı (diff +3/−4, yalnızca test dosyası); `BotCore/NavDrive.h` Tur 2'de değişmedi.
  3. **Not:** Uygulayıcı Raporu Tur 1 sapma 2 ("`NavQuantiseM` içinde ön koşul denetimi") belirsiz yazılmış; kodda `NavQuantiseM` denetimsiz (`:22-25`), denetim `BeginGoto`/`PlanGoto`'daki `ValidCoord`'ta (`:186-189`, `:194-197`) ve plan sözleşmesiyle uyumlu. İşlevsel sorun yok.
  4. **Not (taşma):** `NavQuantiseM` adaya yalnızca rota noktalarından (zaten `ValidCoord` geçmiş uçlar ve ızgara hücre merkezleri) uygulanıyor; aralık dışı taşma yolu yok.
  5. Rapor dürüstlüğü: Tur 1/Tur 2 raporundaki test sayıları, rota uzunlukları, paket sayıları ve `NAVDRIVE` satırları doğrulayıcının koşusuyla uyuşuyor (yalnızca `perf` p95 süreleri koşudan koşuya değişir: 0,8242 → 0,7776 ms).
- Düzeltme talimatı: yok (karar `DOĞRULANDI`).

---

## Claude kararı: BLOKE cevabı (otonom döngü, 2026-10-03; gözden geçirilmeli)

**Seçilen: (a).** `NodeLimit`, `NavDrive_RealMap_Random` için geçerli bir "planlanmadı" sonucudur. Gerekçe: `NavPathfinder` ulaşılamaz hedefte düğüm sınırını doldurursa `NodeLimit` döner (`NavPath.h:31`); ~3 000 kopuk `Walk` hücresi (F5-69, `reach 85 508/88 508`) bilinen bir gerçektir, dolayısıyla `NodeLimit == 0` beklentisi yanlıştı. (b) testi ulaşılabilirlik dışına iter, (c) çift seçimini RNG ayrıntısına bağlar; ikisi de gereksiz. Ölçüm (3/300 = %1) `Planned ≥ %85` eşiğinin çok içinde; eşik gevşetilmedi. `BeginGoto`/`NextStep` kodu **değişmez**; yalnızca test beklentisi düzelir. §5 adım 7 ve §6 bu karara göre güncellendi.

## Düzeltme talimatı (DeepSeek'e aynen verilecek)

```
Tests/BotCoreTests/NavDriveTests.cpp, NavDrive_RealMap_Random testinde YALNIZCA şunları değiştir (BotCore/NavDrive.h'ye dokunma):
1. printf satırını şu biçime getir: "NAVDRIVE random pairs=%d planned=%d nopath=%d nodelimit=%d blocked_events=%d replans=%d unrecoverable=%d truncated=%d\n" (nodelimit nopath'ten sonra).
2. CHECK_EQ(nodelimit, 0); satırını sil.
3. CHECK_EQ(nopath, pairs - planned); satırını CHECK_EQ(nopath + nodelimit, pairs - planned); yap.
Diğer CHECK'ler (planned*100 >= pairs*85, badstart/badgoal 0, unrecoverable 0, violations 0) aynen kalır; eşikleri gevşetme.
Sonra: ./tools/build.sh Release ve Debug, ./tools/run-tests.sh Release ve Debug (0 failed, 289 test). Raporun Tur 2 bölümüne yeni NAVDRIVE random satırını, K2/K3 sonucunu yaz; BLOKE açık sorular bölümünü 'çözüldü: (a)' diye işaretle. Bitince Durum: UYGULANDI yap.
```
