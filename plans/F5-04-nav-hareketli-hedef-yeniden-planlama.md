# F5-04: Hareketli hedef ve yeniden planlama (`BotCore/NavTrack.h`: hız kestirimi, öngörü, menzil halkası, `NavFollower`)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat, `docs/17` §1 "Paralel yürütülebilir işler") |
| Branch | `bot/F5-04` (taban: `gece/2026-10-02-nav`) |
| Bağımlı olduğu planlar | F5-01 (`BotCore/NavGrid.h`), F5-02 (`BotCore/NavPath.h`), F5-03 (`BotCore/NavSmooth.h`): üçü `KAPANDI`, `gece/2026-10-02-nav` içinde (merge `788aa86`, `dc1bb10`, `08ffbc3`) |
| İlgili gereksinim / kabul | `docs/12` §4.2 (hareketli hedef, yeniden planlama, menzil hedefi), §11 T-NAV-06 (altyapısı), AC-NAV-02 (süre bütçesi); ADR-0006 Eki F5-04 (bu planın kararları), ADR-0016 (`BotCore` saflığı) |
| Tahmini büyüklük | S (4 dosya: 2 yeni, 2 değişen; ~1 000 satır, çoğu test) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

F5-02 (A*) ve F5-03 (düzleştirme) tek seferlik, sabit hedefli sorgulardır. Bot ise **hareket eden** bir hedefi (kiting yapan mage, kaçan warrior) izlemek zorundadır. Bu plan, hedef gözlemlerinden hız kestirip öngörü noktası hesaplayan, yeniden planlama zamanını belirleyen, rol menzili halkasında en yakın ulaşılabilir hücreyi seçen ve `NavPathfinder::Find` + `NavSmoothPath`'i tek çağrıda birleştiren saf mantığı yazar: `BotCore/NavTrack.h`.

Dört parça:

1. `NavTargetTracker`: hedefin zaman damgalı gözlemlerinden 1 sn'lik pencerede hız vektörü (sabit boyutlu, bellek ayırmaz).
2. `NavPredictLead`: `docs/12` §4.2'deki `min(1,5 sn, mesafe / kendi_hız)` öngörü süresi.
3. `NavRingCells`: öngörü noktasının çevresindeki `[ringMinM, ringMaxM]` halkasında `Walk` hücreleri, bota octile uzaklığa göre sıralı.
4. `NavFollower`: yukarıdakileri ve A*/düzleştirmeyi birleştiren durum makinesi; ne zaman yeniden planlanacağına karar verir (`First`/`Moved`/`Interval`), halka adayları arasından ilk ulaşılabileni seçer, öngörü noktası yürünemezse geri çekilir.

Bu planın sonunda **ulaşılamaz hedef kararı (`TARGET_UNREACHABLE`, F5-05), tehlike/clearance maliyeti (F5-06), güvenli nokta, formasyon, takılma kurtarma, hareketi dünya koordinatına/`WIZ_MOVE`'a çevirme, telemetri ve sunucu entegrasyonu yoktur.** `GameServer/`, `AIServer/`, `shared/` değişmez; sunucu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0006-navigasyon-izgara-astar.md` — **Eki F5-04** (bu plan yazılırken eklendi): gözlem/planlama ayrımı, hız penceresi, öngörü geri çekilmesi, yeniden planlama koşulu, halka tanımı, `ringMaxTries`. Kararlar orada; sapma gerekiyorsa dur ve sor.
- `docs/12_NAVIGATION_AND_POSITIONING.md` §4.2 (bu plana göre güncellendi), §4.3 (ulaşılamaz hedef: F5-05; bu planda yalnızca `pathStatus` taşınır), §11 (T-NAV-06, AC-NAV-02).
- `BotCore/NavGrid.h` — kullanacağın API: `Size()`, `Unit()`, `CellOf(float)` (= `floor(w / unit)`), `CellCenter(int)` (= `(i + 0.5) * unit`), `Walk(x, z)` (ızgara dışı → `false`). **Değiştirme.**
- `BotCore/NavPath.h` — `NavCell`, `NavOctile`, `NavPathStatus`, `NavSearchParams` (`maxNodes = 20000`), `NavPathResult`, `NavPathfinder::Find` (yeniden kullanılabilir, iş parçacığı güvenli değil). **Değiştirme.**
- `BotCore/NavSmooth.h` — `NavSmoothParams`, `NavSmoothResult`, `NavSmoothPath`, `NavLineClear` (testlerde). **Değiştirme.**
- `BotCore/Rng.h` — testlerde `BotCore::Rng`; global `rand()` yasak.
- `Tests/BotCoreTests/NavSmoothTests.cpp` — **kopyalanacak kalıplar** (başka `.cpp`'den içe aktarma yok; yeni dosyada kendi kopyalarını yaz): `CellIndex`, `Cell`, `RingEvents`, `MakeNav(n, unit, events, heights)`, `IsSubsequence`, `SegmentsClear`, `PercentileDouble`, gerçek harita yükleme + `SKIPPED` kalıbı, `near64` çift çekimi ve `std::chrono::steady_clock` ölçümü (`NavSmooth_Perf`).
- `Tests/BotCoreTests/MiniTest.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`; kayan nokta karşılaştırması için `CHECK(std::fabs(a - b) <= eps)`), `tools/run-tests.sh` (testi depo kökünde çalıştırır).

Planı yazarken doğrulanan gerçekler (Python prototipi, 2026-10-02, depoya girmeyen geçici; **aynı algoritma**, aynı kenar kuralı; sayılar test eşikleri/beklentileridir, tam eşitlik yalnızca küçük el yapımı haritalarda istenir; hepsi hücre merkezlerinde ve sınır dışı değerlerle seçildi, kayan nokta sınır durumu yok):

- Haritalar: "N×N halka" = `RingEvents(N)` (kenar engelli, iç açık), düz zemin, `unit = 4`; hücre `(x, z)` merkezi dünya `((x + 0.5) * 4, (z + 0.5) * 4)`.
- 40×40: bot hücre `(5, 5)` (dünya `(22, 22)`), sabit hedef `(82, 22)` = hücre `(20, 5)` merkezi → halka `[0, 0]` tek aday `(20, 5)`; yol 16 hücre, düzleştirilmiş 2 ara nokta `(5,5), (20,5)`, uzunluk **60,0**. Hedef `(88, 22)`: aday `(21, 5)` ve `(22, 5)` (ikisi 2 m), bota yakın olan `(21, 5)`.
- Öngörü (40×40): 0/250/500/750/1000 ms'de `x = 82, 84, 86, 88, 90` (`z = 22`) → hız `(8, 0)`; bot `(22, 22)`, hız 8 m/s: mesafe 68 → öngörü süresi 1,5 → nokta `(102, 22)` → hedef hücre **`(25, 5)`**. Bot `(82, 22)`: mesafe 8 → süre 1,0 → `(98, 22)` → **`(24, 5)`**. Bot hızı 40 → süre 1,5 → `(25, 5)`; `maxLeadSec = 0` ya da bot hızı 0 → `(22, 5)`. Gözlem `now − en_yeni > 1000` (now = 2001) → hız 0 → `(22, 5)`; now = 2000 (tam 1000) → hâlâ hız 8 → `(25, 5)`.
- Geri çekilme (40×40, `x = 20` sütununda `z ∈ [1, 38]` engelli → sol yarı ana bileşen/`Walk`, sağ yarı değil): `z = 82`, `x = 66, 68, 70, 72, 74` (0..1000 ms, hız 8), bot `(22, 82)`, hız 8: süre 1,5 → `x = 86` (hücre 21, `Walk` değil) → 0,75 → `80` (hücre 20, duvar) → 0,375 → `77` (hücre 19, `Walk`) → öngörü `(77, 82)`, süre 0,375, hedef hücre **`(19, 20)`**.
- Menzil halkası (60×60): hedef `(122, 122)` (hücre `(30, 30)` merkezi), halka `[30, 45]`: bot `(10, 30)` → aday seçimi **`(19, 30)`** (halkanın bota bakan kenarı, merkeze 44 m), düzleştirilmiş `(10,30), (19,30)`, uzunluk 36,0; bot zaten halkada `(22, 30)` (merkeze 32 m) → hedef **`(22, 30)`**, tek ara nokta; bot hedefin üstünde `(30, 30)` → dört eşit aday `(22,30), (38,30), (30,22), (30,38)` (hepsi 8 hücre), beraberlik `x` sonra `z` → **`(22, 30)`**.
- `NavRingCells` (30×30): merkez `(64, 62)`, halka `[0, 0]` → `(15, 15), (16, 15)` (bota `(5, 15)` yakın olan önce). Merkez `(62, 62)`, halka `[9, 13]`, `from = (5, 15)` → **16 aday**, ilk üçü `(12,15), (12,14), (12,16)`.
- Başarısızlık: 30×30, `x = 15` sütununda `z ∈ [1, 28]` engelli (sol yarı `Walk`, sağ yarı değil); hedef `(90, 42)` (hücre `(22, 10)`, `Walk` değil), halka `[0, 0]` → aday yok (**`NoGoal`**). Hedef `(66, 42)` (hücre `(16, 10)`, `Walk` değil), halka `[0, 13]`, bot `(5, 5)` → aday var, ilk (bota en yakın) **`(13, 9)`**, `Planned`. 20×20, `x ∈ [10, 14]`, `z ∈ [8, 12]` bloğu 20 m yüksek (tüm hücreler `Walk`, bloğa kenar eğimden kapalı): hedef `(50, 42)` (hücre `(12, 10)`), halka `[4, 9]` → tüm adaylar blok içinde → 3 deneme, hepsi `NoPath` (**`PathFailed`**, `tries = 3`); `ringMaxTries = 1` → `tries = 1`; halka `[0, 0]` → `tries = 1`.
- Kovalama simülasyonu (80×80 halka; bot `(82, 162)` hız 9 m/s, hedef `(122, 162)` `+x` yönünde 6 m/s; tick 100 ms, gözlem her 200 ms, `T = 0..14 000` ms): halka `[0, 0]`: 29 yeniden planlama (hepsi `First`/`Interval`), 29 `Planned`, hedefe ≤ 6 m yaklaşma ilk **11 400 ms**'de, son mesafe 3,7 m. Halka `[30, 45]`: 29 yeniden planlama, 29 `Planned`, tüm tick'lerde bot–hedef mesafesi **35,8..43,0 m** (başlangıç 40).
- Gerçek harita (zone 71, ana bileşen 88 508 `Walk`): arena A hücre `(318, 222)` (merkez `(1274, 890)`), B hücre `(186, 276)` (merkez `(746, 1106)`). Halka `[0, 0]`, A→B: hedef `(186, 276)`, tek deneme, yol 150 hücre, düzleştirilmiş 13 ara nokta / 635,787 m (C++ A*'ın beraberlik sırası biraz farklı bir yol seçebilir: ara nokta ≤ 25, uzunluk 570,4..661,1 aralığı istenir, F5-03 ile aynı). Halka `[30, 45]`, A→B: hedef `(196, 271)` (B merkezine 44,72 m), 11 ara nokta / 584,828 m; B→A: `(308, 227)`, 11 ara nokta / 585,795 m. Bot `A`, hedef `(1310, 890)` (36 m) → hedef `(318, 222)`, tek ara nokta.
- `near64` kümesi (Chebyshev ≤ 64 hücre, 150 tohumlu çift): halka `[0, 0]` 148/150 `Planned` (1 `NodeLimit`, 1 `NoPath`), hepsi 1 denemede; halka `[30, 45]` 149/150 `Planned` (146 çift 1 denemede, 3 çift 2, 1 çift 3).

## 3. Kapsam

**Yapılacaklar**

- `BotCore/NavTrack.h`: `NavTargetTracker`, `NavPredictLead`, `NavRingCells`, `NavFollowParams`, `NavFollowStatus`, `NavReplanReason`, `NavFollowPlan`, `NavFollower` (§5.1).
- `Tests/BotCoreTests/NavTrackTests.cpp`: §5.2 test listesi (birim + simülasyon + gerçek harita + süre).
- `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` kayıtları (§5.3).

**Kapsam dışı (yapılmayacak)**

- `NavGrid.h`/`NavPath.h`/`NavSmooth.h` değişikliği (`NavFollower` bunları kullanır; yeni parametre eklemek için bile dokunma).
- Ulaşılamaz hedef **kararı** (3 sn kuralı, `TARGET_UNREACHABLE`, `> 3×` yol uzunluğu kuralı; F5-05): bu planda yalnızca `NavFollowPlan::pathStatus` (son A* durumu) taşınır, "ulaşılamaz" ilan edilmez.
- Çok-hedefli A* (halkanın tamamına tek aramada gitme), halka için hiyerarşik arama, `NavPathfinder`'ın iç havuzuna dokunma, `NavPathfinder` sahipliği (çağıran verir), bot başına havuz paylaşımı kararı.
- Tehlike/clearance/yumuşak ceza maliyeti (F5-06), güvenli nokta (F5-07), formasyon/yığılmayı önleme (F5-08), takılma kurtarma (F5-09), LoS (F5-10).
- Hareketi uygulama: dünya koordinatına çevirme, `WIZ_MOVE`, hız alanı, `y` (yükseklik) enterpolasyonu, adım üretme. (Testteki "bot hareketi" yalnızca test-yerel basit bir simülasyondur, `BotCore`'a girmez.)
- Rol parametre dosyaları/okuyucusu (`P-MAG-PREF-RANGE`, `P-WAR-*` değerleri): `NavFollowParams` varsayılanları ve testteki sabitler yeterli; ayarı dosyadan okuyan kod yazma.
- Telemetri (`NAV_PATH` olayı vb.), sunucu entegrasyonu, algı (`Perception`) bağlama, hedef seçimi.
- `GameServer/`, `AIServer/`, `shared/`, `docs/` değişikliği, DB, sunucu çalıştırma. `GameServer/proj-GameServer.vcxproj` değişmez.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavTrack.h` | yeni | ASCII, CRLF, başlık-yalnızca (satır içi) |
| `Tests/BotCoreTests/NavTrackTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="NavTrack.h" />` satırı (BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="NavTrackTests.cpp" />` satırı (BOM ve CRLF korunur) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`build/nav/zone71.navgrid` üretilen çıktıdır, commit edilmez; yoksa `python3 tools/nav-export.py` ile üret.)

## 5. Uygulama adımları

### 5.1 `BotCore/NavTrack.h`

Ad alanı `BotCore`; `#include "NavSmooth.h"` (aynı dizin, tırnaklı; `NavPath.h` ve `NavGrid.h`'yi o getirir) ve yalnızca standart başlıklar (`<algorithm> <cmath> <cstddef> <cstdint> <vector>`). Stil `NavPath.h`/`NavSmooth.h` gibi: tab girinti, Allman, İngilizce yorum, `inline` tanımlar, global/`static` durum yok (`static constexpr` sınıf sabiti serbest). Konum birimi **dünya metresi** (`float`, `x`/`z`; ızgara hücre `(x, z)` ↔ dünya `(x, z)`), zaman birimi çağıranın verdiği tek yönlü `int64_t` **milisaniye** (`BotCore` saat okumaz), hız m/s.

```cpp
namespace BotCore
{
	// Timestamped target observations -> velocity. Fixed storage, no allocation.
	class NavTargetTracker
	{
	public:
		static constexpr int kCapacity = 16;

		void Clear();
		// False (sample dropped) when tMs <= the newest stored time; otherwise stored. When all
		// kCapacity slots are used the oldest sample is overwritten.
		bool Observe(int64_t tMs, float x, float z);
		int  Count() const;   // 0..kCapacity
		// False when empty. Otherwise the newest sample.
		bool Latest(int64_t & tMs, float & x, float & z) const;
		// vx = vz = 0 unless: >= 2 samples, nowMs - newest.t <= windowMs, and the span between the
		// newest sample and the oldest sample with t >= newest.t - windowMs is >= minSpanMs (> 0).
		// Then v = (newest - oldest) / (span / 1000.0f).
		void Velocity(int64_t nowMs, int windowMs, int minSpanMs, float & vx, float & vz) const;
	};

	// Lead time in seconds: min(maxLeadSec, dist / ownSpeedMps); 0 when ownSpeedMps <= 0,
	// maxLeadSec <= 0 or dist <= 0.
	inline float NavPredictLead(float dist, float ownSpeedMps, float maxLeadSec);

	// Fills `out` (fully overwritten) with the Walk cells whose centre is within [lo, hi] metres of
	// the world point (cx, cz), nearest to `from` first. Effective radii: hi = max(ringMaxM,
	// 0.5 * unit * sqrt(2)); lo = min(max(ringMinM, 0), hi). Order: NavOctile(cell - from)
	// ascending, ties by x then z. The scan window is the square of cells within
	// ceil(hi / unit) + 1 of the cell containing (cx, cz), clipped by Walk (which is false off-grid).
	inline void NavRingCells(const NavGrid & grid, float cx, float cz, float ringMinM, float ringMaxM,
		NavCell from, std::vector<NavCell> & out);

	struct NavFollowParams
	{
		float maxLeadSec = 1.5f;        // docs/12 s4.2 [Ö]
		int   velocityWindowMs = 1000;  // docs/12 s4.2 [Ö]
		int   minVelocitySpanMs = 100;  // [A] ADR-0006 Ek F5-04
		float replanDistM = 6.0f;       // docs/12 s4.2 [Ö]; <= 0 disables the Moved trigger
		int   replanIntervalMs = 500;   // docs/12 s4.2 [Ö]
		float ringMinM = 0.0f;          // role ring around the predicted point (metres)
		float ringMaxM = 0.0f;
		int   ringMaxTries = 3;         // [A] ADR-0006 Ek F5-04; values < 1 act as 1
		NavSearchParams search;         // P-NAV-MAX-NODES
		NavSmoothParams smooth;         // P-NAV-SMOOTH-LOOKAHEAD
	};

	enum class NavFollowStatus
	{
		NoTarget,       // no observation yet (or after Reset)
		Planned,        // path found and smoothed
		NoGoal,         // the ring holds no Walk cell
		InvalidStart,   // the bot's own cell is not Walk / off-grid
		PathFailed      // every tried ring cell failed (see pathStatus: NoPath / NodeLimit)
	};

	enum class NavReplanReason { None, First, Moved, Interval };

	struct NavFollowPlan
	{
		NavFollowStatus status = NavFollowStatus::NoTarget;
		NavPathStatus pathStatus = NavPathStatus::NoPath;  // status of the LAST A* run; meaningful for Planned, InvalidStart, PathFailed
		NavCell goal;                 // chosen ring cell; meaningful when Planned
		float targetX = 0.0f;         // latest observed target position used for this plan
		float targetZ = 0.0f;
		float predX = 0.0f;           // ring centre (predicted point after back-off)
		float predZ = 0.0f;
		float leadSec = 0.0f;         // lead time actually applied (0: no velocity or no walkable prediction)
		int   tries = 0;              // A* runs of this plan (0 for NoGoal)
		int   expanded = 0;           // closed nodes summed over those runs
		int64_t plannedAtMs = 0;
		NavSmoothResult smooth;       // start..goal waypoints and length; empty unless Planned
	};

	class NavFollower
	{
	public:
		void Reset();   // forget observations and plan: status NoTarget, reason None, Replans() 0
		// Feed a target position when the perception layer reports one (tMs = observation time).
		// False when dropped (tMs <= newest stored time).
		bool ObserveTarget(int64_t tMs, float x, float z);
		// Call once per bot tick. Returns true when a plan was (re)computed during this call
		// (Plan() then holds the result, also for NoGoal/InvalidStart/PathFailed). Returns false when
		// nothing was due or no target was observed yet (Plan() unchanged).
		bool Update(const NavGrid & grid, NavPathfinder & pathfinder, int64_t nowMs,
			float botX, float botZ, float botSpeedMps, const NavFollowParams & params);

		const NavFollowPlan & Plan() const;
		NavReplanReason LastReason() const;  // reason of the last replan; None before the first
		int Replans() const;                 // replans since Reset
		const NavTargetTracker & Tracker() const;
	};
}
```

**`NavFollower::Update` kuralı (kesin; her biri §5.2'deki testle sabitlenir):**

1. `Latest(...)` boşsa `false` (plan değişmez). Aksi halde `(tx, tz)` = en yeni gözlem.
2. **Yeniden planlama koşulu**, bu sırayla: hiç planlanmadıysa `First`; değilse `replanDistM > 0` ve hedefin `(tx, tz)` konumu **son planın yapıldığı** `(planTx, planTz)` konumundan Öklid `>= replanDistM` ise `Moved`; değilse `nowMs - plannedAtMs >= replanIntervalMs` ise `Interval`; hiçbiri değilse `false` (A* çalışmaz). Başarısız (`NoGoal`/`InvalidStart`/`PathFailed`) plan da `plannedAtMs`/`planTx,planTz`'yi günceller (aynı aralıkla yeniden denenir, her tick'te değil).
3. **Hız ve öngörü:** `Velocity(nowMs, velocityWindowMs, minVelocitySpanMs, vx, vz)`; `dist` = bot ile `(tx, tz)` arası Öklid; `lead0 = NavPredictLead(dist, botSpeedMps, maxLeadSec)`. `(vx, vz) != (0, 0)` ve `lead0 > 0` ise `lead = lead0` ile başla ve **en çok 4 kez**: `(qx, qz) = (tx + vx*lead, tz + vz*lead)`; `grid.Walk(grid.CellOf(qx), grid.CellOf(qz))` ise `(predX, predZ) = (qx, qz)`, `leadSec = lead`, dur; değilse `lead *= 0.5f` (sıra: `lead0`, `lead0/2`, `lead0/4`, `lead0/8`). Hiçbiri yürünebilir değilse ya da hız 0 ise `(predX, predZ) = (tx, tz)`, `leadSec = 0`.
4. **Aday halka hücreleri:** `botCell = (CellOf(botX), CellOf(botZ))`; `NavRingCells(grid, predX, predZ, ringMinM, ringMaxM, botCell, cands)`. Boşsa: `status = NoGoal`, `tries = 0`, `smooth` boş (`waypoints` boş, `length 0`), `pathStatus` = `NoPath`; bitir.
5. **Deneme döngüsü:** en çok `max(ringMaxTries, 1)` aday, sıralı: `pathfinder.Find(grid, botCell, cand, params.search, path)`; `tries++`, `expanded += path.expanded`, `pathStatus = path.status`. `Found` ise `goal = cand`, `NavSmoothPath(grid, path.cells, params.smooth, plan.smooth)`, `status = Planned`, döngüden çık. `InvalidStart` ise `status = InvalidStart` ve döngüden çık (bot hücresi yürünemez; diğer adaylar denenmez). Başka durumlarda sıradaki aday. Döngü `Found`'suz biterse `status = PathFailed` (son durum `pathStatus`'ta), `smooth` boş.
6. Kayıt: `plannedAtMs = nowMs`, `(planTx, planTz) = (tx, tz)`, `targetX/Z = (tx, tz)`, `LastReason`, `Replans()++`. `true` dön.

Üyeler: `NavTargetTracker`, `NavFollowPlan`, `NavPathResult` ve `std::vector<NavCell>` (aday listesi) **yeniden kullanılır** (her `Update`'te yeni vektör ayırma; `NavRingCells` içinde yalnızca `out` büyür). `Reset()` `NavFollowPlan`'ı varsayılana döndürür (`smooth.waypoints.clear()`).

MSVC Level 4 uyarılarını `static_cast` ile çöz (`#pragma warning` ekleme). Özyineleme yok.

### 5.2 `Tests/BotCoreTests/NavTrackTests.cpp`

`#include "MiniTest.h"`, `<BotCore/NavGrid.h>`, `<BotCore/NavPath.h>`, `<BotCore/NavSmooth.h>`, `<BotCore/NavTrack.h>`, `<BotCore/Rng.h>` (include biçimi `NavSmoothTests.cpp` gibi). Anonim ad alanında yardımcılar (kendi kopyaların): `CellIndex`, `Cell`, `RingEvents`, `MakeNav`, `HeightZeros`, `IsSubsequence`, `SegmentsClear`, `PercentileDouble`; ayrıca `WorldOf(grid, cell, &x, &z)` (= `CellCenter`), `Dist(ax, az, bx, bz)` (`std::sqrt` kayan nokta), `WallColumn(n, x)` (`RingEvents`'e `x` sütununda `z ∈ [1, n-2]` engelli ekler), `Observe5(follower, x0, dx, z)` (0, 250, 500, 750, 1000 ms'de `x0 + k*dx`).

Her testte `follower.Update(...)` dönüşü ve `Plan()` alanları `CHECK`/`CHECK_EQ` ile doğrulanır; haritalar küçük ve elle türetilmiştir (§2'deki değerler). Hepsinde `NavPathfinder pf;` bir tane, `NavFollowParams params;` varsayılan (aksi belirtilmedikçe), bot hızı 8 m/s.

| Test adı | İçerik |
|---|---|
| `NavTrack_Tracker_Velocity` | Boş: `Count() == 0`, `Latest` `false`, hız `(0, 0)`. Tek örnek: hız `(0, 0)`. `t = 0, 250, 500, 750, 1000` ve `x = 100, 102, 104, 106, 108` (`z = 50`): `Velocity(1000, 1000, 100, ...)` = `(8, 0)` (`1e-3`); `Latest` = `(1000, 108, 50)`. `Observe(1250, 110, 50)` sonrası `Velocity(1250, ...)` = `(8, 0)` (pencere `t >= 250`). Aralık < 100: yeni izleyici, `t = 0` ve `t = 50` → hız `(0, 0)`. Bayatlık: `now = newest + 1000` hâlâ `(8, 0)`, `now = newest + 1001` → `(0, 0)`. Monoton değil: `Observe(newest)` ve `Observe(newest - 10)` → `false`, `Count()` değişmez. `z` bileşeni: `(x, z) = (50, 100)`'den `+0` / `-2` her 250 ms → `vz = -8`. Taşma: `t = 10*k`, `x = 100 + 0.1*k`, `k = 0..39` → `Count() == 16`, `vx` = 10 (`5e-2`), `vz` = 0. `Clear()` sonrası `Count() == 0`. |
| `NavTrack_PredictLead` | `(40, 8, 1.5)` → 1,5; `(4, 8, 1.5)` → 0,5; `(12, 8, 1.5)` → 1,5; `(8, 0, 1.5)`, `(8, -3, 1.5)`, `(8, 8, 0)`, `(0, 8, 1.5)`, `(-5, 8, 1.5)` → 0 (`1e-6`). |
| `NavTrack_RingCells` | 30×30 halka, düz. (a) Merkez `(62, 62)`, halka `[0, 0]`, `from = (5, 15)` → tek aday `(15, 15)`; merkez `(64, 62)` → `(15, 15), (16, 15)` bu sırayla. (b) Merkez `(62, 62)`, halka `[9, 13]`, `from = (5, 15)` → **16** aday, ilk üçü `(12,15), (12,14), (12,16)`; her aday `Walk`, merkeze uzaklığı `[9, 13]` içinde; sıra: ardışık octile uzaklıkları azalmaz. Kaba kuvvetle (tüm ızgara taraması) bulunan küme ile aynı elemanlar (sayı eşitliği + her biri `std::find`). (c) `(15, 15)` hücresi engelli yapılmış harita (`events = 0`, `Build` sonrası): halka `[0, 0]`, merkez `(62, 62)` → aday yok (boş); ızgara dışı merkez `(-40, 62)` → boş, çökmez. (d) Büyük halka `[30, 45]`, 60×60, merkez `(122, 122)` → boş değil, her aday `Walk`, `[30, 45]` içinde. (e) `ringMinM > ringMaxM` (`[20, 10]`): etkin `lo = hi = 10`; merkezden tam 10 m uzaklıkta hücre merkezi yoktur (uzaklıklar `4 * sqrt(a² + b²)`) → boş. Negatif `ringMinM` (`[-5, 0]`) → `[0, 0]` ile aynı sonuç (`(15, 15)`). |
| `NavTrack_Follower_Triggers` | 40×40 halka, düz. Yeni izleyici: `Update` `false`, `Plan().status == NoTarget`, `Replans() == 0`, `LastReason() == None`. `ObserveTarget(0, 82, 22)`; bot `(22, 22)`. `Update(0)` → `true`, `LastReason() == First`, `Replans() == 1`, `status == Planned`, `goal == (20, 5)`, `smooth.waypoints` tam `(5,5), (20,5)`, `smooth.length` = 60,0 (`1e-3`), `tries == 1`, `leadSec == 0`, `plannedAtMs == 0`, `targetX/Z == 82/22`. `Update(100)` ve `Update(499)` → `false`, `Replans()` değişmez. `Update(500)` → `true`, `Interval`, `Replans() == 2`. `Update(999)` `false`; `Update(1000)` `true` (3). `ObserveTarget(1050, 87.5, 22)` (5,5 m) → `Update(1050)` `false`. `ObserveTarget(1100, 88, 22)` (planlandığı `(82, 22)`'den tam 6,0 m) → `Update(1100)` `true`, `LastReason() == Moved`, `Replans() == 4`, `goal == (21, 5)` (hız 0: aralık 50 ms < 100); `plannedAtMs == 1100`. `Update(1599)` `false`; `Update(1600)` `true`, `Interval` (5). `params.replanDistM = 0`: yeni izleyici, `Update(0)` planlar, `ObserveTarget(100, 140, 22)` (58 m) sonra `Update(100)` `false` (Moved devre dışı), `Update(500)` `true`. `params.replanIntervalMs = 200`: yeni izleyici, `Update(0)` `true`, `Update(199)` `false`, `Update(200)` `true`. `Reset()` sonrası `Plan().status == NoTarget`, `Replans() == 0`, `Tracker().Count() == 0`; `Update` `false`. Monoton değil gözlem: `ObserveTarget` (aynı veya daha eski `t`) `false`. |
| `NavTrack_Follower_Prediction` | 40×40 halka, düz. Beşli gözlem `Observe5(f, 82, 2, 22)` (82..90 m, hız 8). (a) Bot `(22, 22)`, `Update(1000)` → `predX = 102` (`1e-3`), `predZ = 22`, `leadSec = 1.5`, `goal == (25, 5)`. (b) Bot `(82, 22)`, hız 8 → `leadSec = 1.0`, `predX = 98`, `goal == (24, 5)`. (c) Bot `(22, 22)`, hız 40 → `leadSec = 1.5`, `goal == (25, 5)`; hız 0 → `leadSec = 0`, `goal == (22, 5)`; `params.maxLeadSec = 0` → `(22, 5)`. (d) Bayatlık: `Update(2001)` → `leadSec = 0`, `goal == (22, 5)`; yeni izleyici, `Update(2000)` → `goal == (25, 5)`. (e) **Geri çekilme:** 40×40, `WallColumn(40, 20)` (sol yarı `Walk`, sağ değil; `REQUIRE(!grid.Walk(25, 10))` ve `REQUIRE(grid.Walk(10, 10))`), gözlemler `Observe5(f, 66, 2, 82)`, bot `(22, 82)` hız 8, `Update(1000)` → `leadSec = 0.375` (`1e-6`), `predX = 77` (`1e-3`), `goal == (19, 20)`, `status == Planned`. |
| `NavTrack_Follower_Ring` | 60×60 halka, düz; hedef `(122, 122)` bir kez gözlenir (`ObserveTarget(0, ...)`, hız 0), `params.ringMinM = 30`, `ringMaxM = 45`. (a) Bot `(42, 122)` (hücre `(10, 30)`): `Planned`, `goal == (19, 30)`, `smooth.waypoints` tam `(10,30), (19,30)`, `smooth.length` = 36,0 (`1e-3`), `tries == 1`, `Dist(goal merkezi, hedef)` = 44 (`1e-3`). (b) Bot zaten halkada `(90, 122)` (hücre `(22, 30)`, merkeze 32 m): `Planned`, `goal == (22, 30)`, `waypoints.size() == 1`, `length == 0`, `tries == 1`. (c) Bot hedefin üstünde `(122, 122)`: `goal == (22, 30)` (dört eşit aday, beraberlik `x` sonra `z`). (d) Varsayılan halka `[0, 0]`, bot `(42, 122)`: `goal == (30, 30)`, `waypoints.size() == 2`. |
| `NavTrack_Follower_Failures` | (a) **`NoGoal`**: 30×30, `WallColumn(30, 15)` (`REQUIRE(grid.Walk(5, 5))`, `REQUIRE(!grid.Walk(22, 10))`); hedef `(90, 42)`, bot `(22, 22)`, halka `[0, 0]`: `Update(0)` `true`, `status == NoGoal`, `tries == 0`, `smooth.waypoints` boş, `smooth.length == 0`. (b) Aynı harita, hedef `(66, 42)` (`Walk` olmayan hücre `(16, 10)`), halka `[0, 13]`, bot `(22, 22)`: `Planned`, `goal == (13, 9)`, `tries == 1`, hedef merkezine uzaklık ≤ 13, `goal` `Walk`. (c) **`PathFailed`**: 20×20, `RingEvents`, `x ∈ [10, 14]`, `z ∈ [8, 12]` hücrelerinin yüksekliği 20 (diğerleri 0), `REQUIRE(grid.Walk(12, 10))` ve `REQUIRE(grid.Walk(5, 5))`; hedef `(50, 42)`, bot `(22, 22)`: halka `[4, 9]` → `status == PathFailed`, `pathStatus == NoPath`, `tries == 3`, `smooth.waypoints` boş, `expanded > 0`; `ringMaxTries = 1` → `tries == 1`; `ringMaxTries = 0` ve `-2` → `tries == 1`; `ringMaxTries = 2` → `tries == 2`; halka `[0, 0]` → `tries == 1`, `PathFailed`. Ardından blok **içindeki** bot `(50, 42)` (hücre `(12, 10)`) ile aynı hedef, halka `[4, 9]`: `Planned`, `goal == (11, 10)` (dört eşit aday, beraberlik `x` sonra `z`; hepsi blok içinde ve ulaşılabilir), `tries == 1`. (d) **`InvalidStart`**: 40×40 halka, hedef `(82, 22)`, bot `(2, 2)` (hücre `(0, 0)`, engelli): `Update(0)` `true`, `status == InvalidStart`, `pathStatus == InvalidStart`, `tries == 1` (aday sayısı ne olursa olsun döngü durur); bot ızgara dışı `(-20, 22)` → aynı. Başarısız plan da aralıkla yeniden denenir: `Update(100)` `false`, `Update(500)` `true`. (e) `Reset()` sonrası `Plan().status == NoTarget` ve `Plan().smooth.waypoints` boş. |
| `NavTrack_Chase_Sim` | 80×80 halka, düz; **test-yerel** simülasyon (BotCore'da yok): `bot = (82, 162)` (`double`), hedef `(122, 162)`, hedef hızı `(+6, 0)` m/s, bot hızı 9 m/s; `t = 0, 100, ..., 14 000` ms; her tick: `t % 200 == 0` ise `ObserveTarget(t, tx, tz)`; `Update(t, bot, 9.0f)`; `true` dönerse `replans++`, `Planned` ise `planned++` ve bot rotası = `smooth.waypoints` hücre merkezleri (sıra 1'den, yani başlangıç hücresi atlanır; tek ara nokta ise rota boş), değilse rota boş; bot `0,9 m` ilerler (rotada sıradaki noktaya doğru, ulaşırsa kalanla sonraki noktaya); hedef `0,6 m` ilerler; tick sonunda mesafe `d` ölçülür, `d <= 6` ilk kez ise `caught_ms = t + 100`. **Halka `[0, 0]`:** `replans == 29`, `planned == 29`, `caught_ms` 0'dan büyük ve `<= 13 000`, son mesafe `<= 8,0`. **Halka `[30, 45]`** (bot/hedef aynı başlangıç): `replans == 29`, `planned == 29`, **her** tick'te `d` `[28, 47]` içinde. Yazdır (tam biçim): `NAVTRACK chase ring=0-0 replans=<n> planned=<n> caught_ms=<n> final_dist=<x.x>` ve `NAVTRACK chase ring=30-45 replans=<n> planned=<n> dist_min=<x.x> dist_max=<x.x>`. |
| `NavTrack_RealMap` | Gerçek harita (aşağıya bak). |
| `NavTrack_Perf` | Gerçek harita (aşağıya bak): `near64` kümesi, `Update` süresi. |

**Gerçek harita ortak kuralı** (F5-02/F5-03 ile aynı): dosya yolu `build/nav/zone71.navgrid` (çalışma dizini depo kökü); açılamazsa testi **başarısız yapma**: `std::printf("NAVTRACK real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n")` yaz ve dön. `LoadFile` + `Build()` (varsayılan `NavParams`) sonrası `MainComponentCells() == 88508` `REQUIRE` edilir.

`NavTrack_RealMap`:

- A = `(CellOf(1274), CellOf(890))` = `(318, 222)`, B = `(CellOf(746), CellOf(1106))` = `(186, 276)`; ikisi `Walk` (`REQUIRE`). Bot A merkezinde (`CellCenter`), hedef B merkezi, hız 8 m/s.
- **Tam halka `[0, 0]`:** `ObserveTarget(0, B merkezi)`, `Update(0)` → `Planned`, `goal == B`, `tries == 1`, `waypoints.size()` 2..25, ilk = A, son = B, `IsSubsequence` gerekmez (yol Follower'ın içinde), `SegmentsClear`; `length` 570,4..661,1.
- **Mage halkası `[30, 45]`:** aynı çift, `Planned`, `goal` hedef merkezine uzaklığı `[30, 45]` içinde ve `Walk`, `waypoints.size()` 2..25, ilk = A, son = `goal`, `SegmentsClear`, `length <= tam halka length - 20.0` (prototip 635,787 → 584,828), `tries <= 3`. B → A yönü için de aynı kontroller (hedef A merkezi).
- **Halkada bot:** bot A merkezi, hedef `(1310, 890)` (36 m), halka `[30, 45]` → `goal == A`, `waypoints.size() == 1`.
- **Hareketli hedef (gerçek harita):** `NavPathfinder::Find(A → B)` yolu (`REQUIRE(cells.size() >= 109)`); hedef gözlemleri `t = 0, 250, 500, 750, 1000` ms'de yolun `cells[100], cells[102], cells[104], cells[106], cells[108]` hücre merkezleri (hedef yol üzerinde, hep `Walk`); bot A merkezi, hız 8, halka `[0, 0]`, `Update(1000)`: `Planned` (`REQUIRE`), `leadSec` `[0, 1.5]` içinde, `grid.Walk(CellOf(predX), CellOf(predZ))`, `goal` `Walk` ve hedefin öngörü noktasına uzaklığı `<= 2.9` m, `waypoints.front() == A`, `SegmentsClear`.
- Yazdır: `std::printf("NAVTRACK real exact: waypoints=%d length=%.3f; mage: goal=(%d,%d) d=%.2f waypoints=%d length=%.3f\n", ...)`.
- Not (planın doğruladığı sayı tutmazsa): ara nokta sayısı ya da uzunluk aralığı dışına çıkarsa **kuralı sayıya uydurma**; dur ve Uygulayıcı Raporu'nda sor.

`NavTrack_Perf`:

- Hazırlık: `Walk` hücrelerini x-ana taramada tek `std::vector<BotCore::NavCell>`'e topla (88 508 eleman). Sorgu sayısı `Q` **Release'te 1000, Debug'da 100** (`#ifdef _DEBUG`). Çiftler `Rng(20261002)`'den `NextBelow(count)` ile çekilir; başlangıç == hedef ise ya da Chebyshev mesafesi (`max(|dx|, |dz|)`) 64'ü aşıyorsa çift reddedilir ve yenisi çekilir (`near64`, F5-02/F5-03'teki kümenin aynısı). Bir `NavPathfinder` ve bir `NavFollower` yeniden kullanılır.
- Her çift için iki küme (aynı çiftler): **`exact`** (`ringMinM = ringMaxM = 0`) ve **`mage`** (`[30, 45]`). Her çiftte: `follower.Reset()`, `ObserveTarget(0, hedef hücre merkezi)`, **yalnızca `Update(0, ...)` çağrısı** `std::chrono::steady_clock` ile ölçülür (aday üretimi + A* + düzleştirme dahil); bot = başlangıç hücre merkezi, hız 8. **Isınma:** ölçümden önce aynı kümeden 20 ek çift koşulur ve istatistiğe sayılmaz.
- İstatistik (küme başına): `status == Planned` sayısı `n`, `tries` ortalaması (tüm çiftler), süre p50/p95/p99 (ms; `n`'den bağımsız, tüm çiftler).
- Yazdır (**tam bu biçim**, küme başına bir satır):

```
NAVTRACK perf set=exact planned=<n> tries_mean=<x.xx> ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
NAVTRACK perf set=mage planned=<n> tries_mean=<x.xx> ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
```

- `CHECK`'ler: her kümede `n * 100 >= Q * 95` (prototip %98,7 / %99,3); `tries_mean` 1,0 ile 1,5 arasında; ilk 50 `Planned` plan için `SegmentsClear` ve (`mage` kümesinde) hedefe uzaklık `[30, 45]` içinde (`exact` kümesinde `<= 3`). **`#ifndef _DEBUG`** (Release): her kümede **`ms_p95 <= 2.0`** `[Ö]` (AC-NAV-02 ile aynı bütçe; A* p95 ≈ 0,55 ms, aday üretimi ve düzleştirme küçük ek). Debug'da süre kapısı yoktur (satırları yine yazdır).

### 5.3 Proje dosyaları

1. `BotCore/BotCore.vcxproj`: `ClInclude` grubuna `<ClInclude Include="NavTrack.h" />` (`NavSmooth.h`'den sonra, `Perception.h`'den önce). BOM/CRLF korunur.
2. `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubuna `<ClCompile Include="NavTrackTests.cpp" />` (`NavSmoothTests.cpp`'den sonra, `PerceptionTests.cpp`'den önce). BOM/CRLF korunur.
3. Bu iki projenin `.filters` dosyası yoktur (F5-01'de doğrulandı); ek dosya yok.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; `BotCore` ve `BotCoreTests` için **yeni uyarı yok** (Level 4; derleme çıktısında `NavTrack` geçen `warning` satırı bulunmaz).
- [ ] K2: `./tools/run-tests.sh Release --no-build --list` çıktısı şu on test adını içerir: `NavTrack_Tracker_Velocity`, `NavTrack_PredictLead`, `NavTrack_RingCells`, `NavTrack_Follower_Triggers`, `NavTrack_Follower_Prediction`, `NavTrack_Follower_Ring`, `NavTrack_Follower_Failures`, `NavTrack_Chase_Sim`, `NavTrack_RealMap`, `NavTrack_Perf`.
- [ ] K3: `build/nav/zone71.navgrid` varken (yoksa `python3 tools/nav-export.py` ile üret) `./tools/run-tests.sh Release --no-build` çıkış kodu 0; çıktıda `98 tests, 0 failed` (önceki 88 + on yeni), tüm eski testler ve on yeni test `[ OK ]`; `SKIPPED` geçmiyor; `NAVTRACK real exact: …` satırı (`waypoints` ≤ 25, `length` 570,4..661,1; mage `d` 30..45), iki `NAVTRACK chase …` satırı ve iki `NAVTRACK perf set=…` satırı var.
- [ ] K4: Gerçek harita yokken (`build/nav/zone71.navgrid` geçici olarak başka ada taşınarak) `./tools/run-tests.sh Release --no-build NavTrack_` çıkış kodu 0, `NavTrack_RealMap` ve `NavTrack_Perf` `SKIPPED` yazar ve diğer sekiz test geçer; ardından dosya yerine geri konur.
- [ ] K5: `./tools/run-tests.sh Debug` (derleme dahil) çıkış kodu 0 (Debug'da `assert`/sınır hataları yok).
- [ ] K6: Davranış sayıları: K3 çıktısında `NAVTRACK chase ring=0-0 replans=29 planned=29 caught_ms=<≤13000>` (`caught_ms` pozitif) ve `NAVTRACK chase ring=30-45 replans=29 planned=29 dist_min=<≥28> dist_max=<≤47>`; `NavTrack_Follower_Triggers`, `NavTrack_Follower_Prediction` (özellikle geri çekilme `(19, 20)`), `NavTrack_Follower_Ring` ve `NavTrack_Follower_Failures` (`NoGoal`, `PathFailed` `tries == 3`, `InvalidStart`) `[ OK ]` (Release ve Debug).
- [ ] K7: **Süre (Release):** K3 çıktısındaki iki `NAVTRACK perf set=…` satırında `planned ≥ 950` ve **`ms_p95 ≤ 2.000`**. Uygulayıcı Raporu satırları **aynen** yapıştırır ve `nproc` bilgisini ekler. Kapı tutmuyorsa: testi üç kez koş (en kötüsünü raporla); tutarlı biçimde aşıyorsa kural gevşetilmez: `NavTrack.h` içinde iyileştir (ör. `NavRingCells`'te karşılaştırıcı yerine ön hesaplı anahtar, bellek ayırmayı kaldırma); hâlâ aşıyorsa durup ölçümleri "Açık sorular"da raporla.
- [ ] K8: Saflık/kapsam: `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavTrack.h` çıktısı boş; `git diff --stat gece/2026-10-02-nav...bot/F5-04` yalnızca §4'teki dört dosyayı ve kendi plan dosyasını gösterir (`GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h`, `NavSmooth.h` yok).
- [ ] K9: F5-01..F5-03 davranışı bozulmadı: `./tools/run-tests.sh Release --no-build Nav_` (10 test), `NavPath_` (9) ve `NavSmooth_` (8) çıkış kodu 0, hepsi `[ OK ]`; `NAVPATH T-NAV-03 set=near64 …` satırında `found ≥ 950` ve `ms_p95 ≤ 2.000`; `NAVSMOOTH perf set=near64 …` satırında `ms_p95 ≤ 0.500`.

## 7. Doğrulama komutları

```bash
git switch -c bot/F5-04 gece/2026-10-02-nav
python3 tools/nav-export.py            # build/nav/zone71.navgrid yoksa
./tools/build.sh Release
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Release --no-build NavTrack_
./tools/run-tests.sh Debug
grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavTrack.h
git diff --stat gece/2026-10-02-nav...bot/F5-04
```

## 8. Kısıtlar ve uyarılar

- **Paralel hat:** sunucuya hiç dokunma (`tools/run-servers.sh` çağırma; ana hat başka çalışma ağacından sunucu çalıştırıyor olabilir). DB'ye bağlanma. Dal tabanı `gece/2026-10-02-nav`.
- Kodlama/satır sonu: `AGENTS.md` §3. Yeni C++ dosyaları ASCII + CRLF + tab + Allman. Yorumlar İngilizce; plan metnindeki Türkçe açıklama koda girmez.
- Başlık-yalnızca: tüm tanımlar `inline` (sınıf içi tanımlar zaten satır içidir); `static` global durum yok; `#pragma` yalnızca `once`.
- **Belirlenim:** aynı gözlem dizisi ve aynı çağrı sırası her zaman aynı planı verir (rastgelelik yok; beraberlik kuralları `x`, sonra `z`). Zaman yalnızca parametredir.
- **Hız kestirimi bayat gözlemle yapılmaz** (`now - newest > velocityWindowMs` → hız 0): bu, hedefin kaybolması (algı hatası) halinde botun eski hız vektörüyle duvara koşmasını önler.
- `NavFollower` **hedef seçmez/bırakmaz** ve "ulaşılamaz" demez: `PathFailed` ve `pathStatus` yalnızca bilgidir; kararı F5-05 verecek (`NoPath` ile `NodeLimit` ayrımı korunur, ADR-0006 madde 3).
- `NavRingCells` pencere maliyeti `(2*ceil(hi/unit) + 3)²` hücredir; halka üst sınırı 64 m'yi aşan çağrılar test edilmez (docs/12'deki roller ≤ 45 m).
- Hiçbir yerde `NavFollower` çıktısını "hareket" ya da "paket" diye adlandırma/yorumlama: hücre yolu ve ara noktalardır; dünya koordinatına/hız alanına çevirme sunucu entegrasyonundadır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-04` (taban `gece/2026-10-02-nav`); `8b796bf` — `[F5-04] Hareketli hedef ve yeniden planlama (BotCore/NavTrack.h) ve NavTrack testleri`; bu rapor ve `Durum` satırı ayrı commit.
- Değişen dosyalar ve neden:
  - `BotCore/NavTrack.h` (yeni, 414 satır): `NavTargetTracker` (16 örnekli sabit halka, hız penceresi/bayat örnek kuralları), `NavPredictLead`, `NavRingCells` (etkin yarıçap `hi = max(ringMaxM, 0.5·unit·√2)`, `lo` kıskacı, `NavOctile` sırası, beraberlik `x`→`z`, `ceil(hi/unit)+1` pencere), `NavFollowParams`/`NavFollowStatus`/`NavReplanReason`/`NavFollowPlan`/`NavFollower` (`First`/`Moved`/`Interval` kararı, en çok 4 öngörü geri çekilmesi, en çok `ringMaxTries` aday denemesi, `InvalidStart`'ta döngü durur; başarısız plan da `plannedAtMs`/hedef konumunu günceller). Saf başlık; `<algorithm> <cmath> <cstddef> <cstdint> <vector>` ve `"NavSmooth.h"` dışında bağımlılık yok.
  - `Tests/BotCoreTests/NavTrackTests.cpp` (yeni, 1020 satır): §5.2'deki on test; kendi yardımcı kopyaları (`CellIndex`, `Cell`, `RingEvents`, `WallColumn`, `MakeNav`, `HeightZeros`, `SegmentsClear`, `Dist`, `PercentileDouble`, `Observe5`); gerçek harita yükleme + `SKIPPED` kalıbı; `near64` perf kümesi ve `std::chrono::steady_clock` ölçümü.
  - `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` (değişen): yalnızca birer `ClInclude`/`ClCompile` satırı; BOM ve CRLF korundu.
  - `GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h`, `NavSmooth.h` değişmedi; yeni dosya/ini yok.
- Derleme sonucu:
  - `./tools/build.sh Release` rc=0; `BotCoreTests.cpp`/`NavTrack*` için yeni uyarı yok (değişen iki dosya `touch`'lanıp yeniden derlendi, çıktıda `warning` yok); son satır `BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Tests\BotCoreTests.exe`.
  - `./tools/run-tests.sh Debug` rc=0; `98 tests, 0 failed`.
- Kabul kriterleri öz-değerlendirme (hepsi karşılandı):
  - K1 ✔ Release rc=0, yeni uyarı 0. K2 ✔ `--list` on `NavTrack_*` adını içeriyor (toplam 98).
  - K3 ✔ `./tools/run-tests.sh Release --no-build` rc=0, `98 tests, 0 failed`, `SKIPPED` yok; `NAVTRACK real exact: waypoints=14 length=630.319; mage: goal=(196,271) d=44.72 waypoints=13 length=587.021`; iki `NAVTRACK chase …`; iki `NAVTRACK perf set=…`.
  - K4 ✔ `zone71.navgrid` geçici taşınınca `NavTrack_` rc=0; `NavTrack_RealMap`/`NavTrack_Perf` `SKIPPED`, diğer sekiz `[ OK ]`; dosya geri kondu (md5 aynı boyut 1 579 030).
  - K5 ✔ Debug derleme dahil rc=0, `98 tests, 0 failed`.
  - K6 ✔ `NAVTRACK chase ring=0-0 replans=29 planned=29 caught_ms=11400 final_dist=3.7`; `ring=30-45 replans=29 planned=29 dist_min=35.8 dist_max=43.0`; `Follower_Triggers`/`Follower_Prediction` (geri çekilme `(19,20)`, `leadSec=0.375`)/`Follower_Ring`/`Follower_Failures` (`NoGoal`, `PathFailed tries=3`, `InvalidStart`) Release ve Debug'da `[ OK ]`.
  - K7 ✔ Release perf: `set=exact planned=997 tries_mean=1.00 ms_p50=0.082 ms_p95=0.546 ms_p99=0.976`; `set=mage planned=998 tries_mean=1.01 ms_p50=0.070 ms_p95=0.597 ms_p99=1.083`; `planned ≥ 950`, `ms_p95 ≤ 2.000`. Ölçüm makinesi: Ryzen 7 7800X3D, `nproc=16`, Release.
  - K8 ✔ Saflık grep'i boş; `git diff --stat` yalnızca dört dosya (plan dosyası tabanda commit'li olduğu için diff'te görünmez): `BotCore/BotCore.vcxproj` +1, `BotCore/NavTrack.h` +414, `Tests/BotCoreTests/BotCoreTests.vcxproj` +1, `Tests/BotCoreTests/NavTrackTests.cpp` +1020.
  - K9 ✔ `Nav_` 10/10; `NavPath_` 9/9 (`near64 found=997`, `ms_p95=0.551 ≤ 2.000`); `NavSmooth_` 8/8 (`near64 ms_p95=0.033 ≤ 0.500`).
- Plandan sapmalar ve gerekçeleri: Yok. Sayılar plan aralığında (`exact waypoints=14` prototip 13'e karşı, kabul ≤ 25; `mage length=587.021` prototip 584.828, kural yalnızca `≤ exact−20` ve `d∈[30,45]`; `perf planned` %99.7/%99.8). `build/nav/zone71.navgrid` zaten vardı; `nav-export.py` çalıştırılmadı.
- Açık sorular: Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

—
