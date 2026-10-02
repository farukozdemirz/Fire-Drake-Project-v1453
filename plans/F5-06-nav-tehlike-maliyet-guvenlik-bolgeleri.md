# F5-06: Tehlike maliyet katmanları ve güvenlik bölgeleri (`BotCore/NavDanger.h`; A*'a isteğe bağlı maliyet alanı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat, `docs/17` §1 "Paralel yürütülebilir işler") |
| Branch | `bot/F5-06` (taban: `gece/2026-10-02-nav`) |
| Bağımlı olduğu planlar | F5-01 (`BotCore/NavGrid.h`), F5-02 (`BotCore/NavPath.h`): `KAPANDI`, `gece/2026-10-02-nav` içinde (merge `788aa86`, `dc1bb10`); F5-05 `KAPANDI` (merge `c5788d7`), bu planın testleri 106 testin üstüne eklenir |
| İlgili gereksinim / kabul | `docs/12` §2 (`danger_static`, `danger_dynamic`, `clearance`), §4.1 (maliyet formülü), §7 (güvenlik bölgeleri), §11 AC-NAV-06 (karşı ulus tower halkasına giriş = 0; altyapısı), AC-NAV-02 (süre bütçesi); ADR-0006 Eki F5-06 (bu planın kararları), ADR-0016 (`BotCore` saflığı) |
| Tahmini büyüklük | S (5 dosya: 2 yeni, 3 değişen; ~1 400 satır, çoğu test) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

F5-02'nin A*'ı yalnızca yatay mesafeyi öder (`NavPath.h` başlık yorumu: "danger and clearance weights belong to F5-06"). Bot ise (a) karşı ulusun guard tower halkasına **girmemeli** (`docs/12` §7, AC-NAV-06), (b) görünür düşmanların (melee 15 m çekirdek, mage 45 m) etrafından **dolanmayı tercih etmeli**, (c) duvara sürtünerek yürümemeli (`clearance`). Bu plan bunun saf mantığını yazar:

1. `NavCostLayer` (`BotCore/NavDanger.h`): hücre başına **tehlike** (0–255), **yasaklı** ve **güvenli** bayrakları. Statik takım katmanı (tower halkaları) bir kez, dinamik tehlike (görünür düşmanlar) her 500 ms'de statik katmanın kopyası üzerine yeniden kurulur.
2. `NavPathfinder::Find`'a **isteğe bağlı** `const NavCostField *` parametresi: adım maliyeti `mesafe × (1 + 0,5 × (ceza(a) + ceza(b)))`; yasaklı hücreye **dışarıdan girilemez**. Parametre verilmezse A* **bit düzeyinde** eskisi gibidir (F5-02 testleri aynen geçer).
3. `NavPathResult::length`: yolun geometrik uzunluğu (metre); `cost` artık ağırlıklı maliyettir.

Bu planın sonunda **`NavFollower`/`NavReach` bağlaması, canavar alanı dikdörtgenleri, eğim cezası, takılma cezası (F5-09), güvenli geri çekilme noktası seçimi (F5-07: `Safe` bayrağını yalnızca işaretler, tüketmez), formasyon, telemetri ve sunucu entegrasyonu yoktur.** `GameServer/`, `AIServer/`, `shared/` değişmez; sunucu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0006-navigasyon-izgara-astar.md` — **Eki F5-06** (bu plan yazılırken eklendi): maliyet modeli, yasaklı bölge kuralı, bant ilkeli, ağırlıklar `[A]`, bilinçli ertelenenler. Sapma gerekiyorsa dur ve sor.
- `docs/12_NAVIGATION_AND_POSITIONING.md` §2, §4.1, §7 (bu plana göre güncellendi), §11 (AC-NAV-06).
- `BotCore/NavGrid.h` — kullanacağın API: `Size()`, `Unit()`, `CellOf(float)`, `CellCenter(int)`, `Walk(x, z)`, `Clearance(x, z)` (`uint8_t`; yürünebilir hücrede ≥ 1, yürünebilir olmayan hücrede 0), `EdgeOpen`. **Değiştirme.**
- `BotCore/NavPath.h` — değiştirilecek tek mevcut kaynak (§5.2). `NavPathfinder::Find` iskeleti: doğrulama `:89-105`, ana döngü `:123-177`, komşu kenarı `:155-176`, `Reconstruct` `:214-229`.
- `Tests/BotCoreTests/NavReachTests.cpp:19-100` — **kopyalanacak yardımcılar** (başka `.cpp`'den içe aktarma yok; yeni dosyada kendi kopyalarını yaz): `CellIndex`, `Cell`, `HeightZeros`, `RingEvents`, `GapWall`, `MakeNav`, `PercentileDouble`. Gerçek harita yükleme + `SKIPPED` kalıbı ve `near64` çift çekimi + `std::chrono::steady_clock` ölçümü: `NavReachTests.cpp` `NavReach_RealMap` / `NavReach_Perf` (aynı kalıp).
- `BotCore/Rng.h` — testlerde `BotCore::Rng` (`NextBelow(uint32_t)`); global `rand()` yasak.
- `Tests/BotCoreTests/MiniTest.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`; kayan nokta için `CHECK(std::fabs(a - b) <= eps)`), `tools/run-tests.sh` (testi depo kökünde çalıştırır).

Planı yazarken doğrulanan gerçekler (Python prototipi, 2026-10-02, depoya girmeyen geçici; **aynı kenar kuralı, aynı formül**; sayılar test beklentileridir; hücre merkezi ve sınırdan uzak değerlerle seçildi). "N×N halka" = `RingEvents(N)` (kenar engelli, iç açık), `unit = 4`, düz zemin; hücre `(x, z)` merkezi dünya `((x + 0.5) * 4, (z + 0.5) * 4)`; `(20, 20)` hücresinin merkezi `(82, 82)`.

**Bant değerleri (düz 40×40, merkez `(82, 82)`)**, `Danger` = `(uint8_t)(255 · ağırlık · (1 − t) + 0,5)`; hücre `(20 + dx, 20)` (ve belirtilenler):

| Bant | `(0,0)` | `dx=3` | `dx=4` | `dx=5` | `dx=6` | `(3,3)` | `(4,3)` | diğer |
|---|---|---|---|---|---|---|---|---|
| melee: iç 0, dış 15, sönüm 8, ağırlık 1,0 | 255 | 255 | 223 | 96 | 0 | 192 | 96 | `dx=-3`: 255; `dx ≥ 6` ya da `\|dx\| ≥ 6`: 0; sıfır olmayan hücre **101** |
| ranged: iç 0, dış 45, sönüm 8, ağırlık 0,6 | 153 | 153 | 153 | 153 | 153 | 153 | 153 | `dx=11`: 153; `dx=12`: 96; `dx=13`: 19; `dx=14`: 0; `dx=-13`: 19; sıfır olmayan hücre **553** |
| halka: iç 20, dış 30, sönüm 8, ağırlık 1,0 | 0 | 0 | (sınır; **test etme**) | 255 | 255 | 158 | 255 | `dx=7`: 255; `dx=8`: 191; `dx=9`: 64; `dx=10`: 0; `dx=-8`: 191 |
| sönümsüz: iç 0, dış 16, sönüm 0, ağırlık 1,0 | 255 | 255 | 255 (`r = 16`, sınır dahil) | 0 | 0 | | | |

(Hiçbir beklenen değer `x,5` yuvarlama sınırına 0,1'den yakın değildir; `dx=4` halka hücresi `127,5` tam sınırdadır, **test edilmez**.) Melee+ranged aynı merkezde (`AddThreats`): `(0,0)` 255, `dx=4` 223, `dx=5` 153, `dx=12` 96, `dx=13` 19, `(3,3)` 192, sıfır olmayan hücre 553. Max-birleşim: A = melee ağırlık 0,4 merkez `(82, 82)`, B = melee ağırlık 1,0 merkez `(106, 82)` (hücre `(26, 20)`): `(20,20)` = 102, `(21,20)` = 102, `(22,20)` = 223, `(23..27,20)` = 255; ekleme sırası değişince katman aynıdır. Izgara dışına taşan merkez `(-10, 82)`, melee: hücre `(0..3, 20)` = 255, 223, 96, 0; merkez `(-1000, -1000)`: tüm katman 0. Sönüm < 0 sönüm 0 gibidir (`(0, 15)` iç/dış, sönüm −5: `dx=0..3` = 255, `dx=4` = 0); ağırlık 2,0 ağırlık 1,0 gibidir (`dx=4` = 223).

**Bölgeler (düz 40×40):** `AddForbidDisc((82, 82), 20)`: **81** hücre; `(24,20)`, `(25,20)` (`r = 20`, sınır dahil) ve `(23,24)` (`dx=3, dz=4`) yasaklı, `(26,20)` ve `(24,24)` değil. `AddForbidDisc((82, 82), 4)`: tam 5 hücre `(19,20) (20,19) (20,20) (20,21) (21,20)`. `AddForbidOutsideDisc((82, 82), 40)`: **1283** yasaklı (1600 − 317); `(20,20)` ve `(30,20)` (`r = 40`, sınır dahil) değil, `(31,20)`, `(35,20)` yasaklı.

**Hücre cezası (düz 40×40):** `Clearance(1, 10) = 1`, `Clearance(2, 10) = 2`, `Clearance(20, 20) = 19`. Varsayılan parametreler (`wDanger 4`, `wClear 0,5`, `clearFree 2`, `forbiddenPenalty 10`) ile `(1,10)` = 0,5; `(2,10)` = 0; melee bandı (ağırlık 1,0, merkez `(82, 82)`) altında `(20,20)` = 4,0, `(24,20)` (danger 223) = 4 · 223 / 255 = 3,49804; ek olarak `AddForbidDisc((82, 82), 4)` ile `(20,20)` = 14,0.

**A* senaryoları** (hepsi aynı kenar kuralı; maliyet = ağırlıklı, `length` = geometrik):

- **S1 tehlike dolanma:** düz 40×40, melee bandı merkez `(82, 82)` (ağırlık 1,0), başlangıç `(5, 20)`, hedef `(35, 20)`. Alan yok: maliyet **120,0** (30 adım). Varsayılan alan: **Found**, maliyet **139,882** (`|Δ| ≤ 0,05`), yol üzerindeki en büyük `Danger` **0**, `length` = maliyet (yolda ceza yok). `wDanger = 0` ⇒ maliyet 120,0.
- **S2 kaçınılmaz koridor:** `GapWall(30, 15, 14, 15)`, melee bandı merkez `(62, 60)`, başlangıç `(10, 14)`, hedef `(20, 14)`: alan yok maliyet 40,0; varsayılan alan **Found**, maliyet **191,255** (`0,05`), `length` **40,0** (`1e-3`), yol `x = 15` sütununu `z ∈ {14, 15}`'te geçer (tehlike **yumuşak**, geçit varsa yol bulunur).
- **S3 clearance:** düz 20×20, başlangıç `(1, 2)`, hedef `(1, 17)`: alan yok maliyet **60,0** (`x = 1` duvar kenarı); `layer = nullptr` ama varsayılan parametrelerle alan: maliyet **66,142** (`0,05`), `length` **63,314** (`0,01`), yolun uç hücreler dışındaki her hücresinin `Clearance ≥ 2`.
- **S4 yasaklı bölge:** düz 40×40, `AddForbidDisc((82, 82), 20)`: `(5,20) → (35,20)` **Found**, maliyet **139,882** (`0,05`), yolda yasaklı hücre **0**. `(20,20) → (35,20)` (başlangıç içeride): **Found**, maliyet **251,598** (`0,1`), yolun başında tek bir yasaklı önek, çıkışta yeniden girme yok (prototipte 5 yasaklı hücre). `(18,20) → (22,20)` (ikisi içeride): **Found**, maliyet **176,0** (`0,01`; her adım `4 × 11`), 5 hücre, hepsi yasaklı. `(5,20) → (20,20)` (hedef içeride, başlangıç dışarıda): **`InvalidGoal`**, `expanded == 0`, `cells` boş. Alan yokken aynı çiftler `Found`.
- **S4b kapalı koridor:** `GapWall(30, 15, 14, 15)` + `AddForbidDisc((62, 60), 10)` (`(15,14)` ve `(15,15)` yasaklı): `(10,14) → (20,14)`: **`NoPath`**; alan yokken **Found**, maliyet 40,0.
- **S5 arena sınırı:** düz 40×40, `AddForbidOutsideDisc((82, 82), 40)`: `(20,20) → (28,20)` **Found**, maliyet **32,0**; `(20,20) → (35,20)`: **`InvalidGoal`**.

**Gerçek harita (zone 71):** kapılar Karus `(1375, 1085)`, El Morad `(622, 911)` (`docs/03` §12.2; her ikisi de `Walk` hücre değildir). `NavBuildTeamZones` halka 90 m ile: **El Morad takımı** (düşman Karus): yasaklı hücre **1594** (`Walk` olan **1264**), güvenli hücre **1591** (`Walk` olan **1232**); **Karus takımı** (düşman El Morad): yasaklı 1591 / `Walk` 1232, güvenli 1594 / `Walk` 1264. (Halka sınırında eşitlik olamaz: hücre merkezi ile kapı arasındaki `dx² + dz²` paritesi 8100'e eşit olmaya izin vermez; kayan nokta farkı sonucu etkilemez.) Arena A `(318, 222)` → B `(186, 276)`: alan yok maliyet **660,617**; iki takım katmanıyla da `wDanger = wClear = 0` ile **660,617** (`0,01`; yol halkalara girmez); varsayılan parametrelerle maliyet **689,103** (`0,1`), `length` **676,617** (`0,1`) (yalnızca clearance etkisi). Çapraz çift `(371, 248) → (334, 308)` (kapıya 143,5 m ve 153,5 m): alan yok **Found**, maliyet **310,676** (`0,01`), yolda **21** yasaklı hücre; El Morad takımı katmanı + `wDanger = wClear = 0` ile **Found**, maliyet **315,362** (`0,05`), yasaklı hücre **0**. Halkanın içinde, taramada (x artan, z artan) ilk `Walk` yasaklı hücre `(321, 268)` (kapıya 89,68 m): `A → (321, 268)` **`InvalidGoal`**. Kapıya en yakın `Walk` hücre `(342, 272)` (7,07 m; en küçük `dx² + dz²`, eşitlikte taramada ilk): `(342,272) → A`, varsayılan parametreler: **Found**, maliyet **1171,853** (`0,5`), `length` **292,284** (`0,1`), yolda **22** yasaklı hücre, hepsi öneğin içinde (tek içeriden-dışarıya geçiş, dışarıdan-içeriye geçiş 0); alan yok maliyet 253,824. **Halka çevresi örneklemi:** Karus kapısına uzaklığı `(90, 170]` m olan `Walk` hücreler tam **3689** adet; bunlardan Chebyshev ≤ 64 hücre çiftlerinde (141 çiftlik prototip örneklemi): 139 `Found`, 1 `NoPath`, 1 `NodeLimit`; alan yokken A* yolunun yasaklı hücreden geçtiği çift oranı 47/141 ≈ %33.
- **Rastgele küçük haritalar** (24×24, iç hücre %8 engelli, %20 yükseklik 6 m; her haritada 3 tehlike bandı, ilk 15 tohumda 1 yasaklı disk): 30 tohum × 100 çift: prototipte 1861 `Found`, 75 `InvalidGoal`, 1064 `NoPath` (Python `random`; C++ `Rng` farklı örnekler verir, eşikler geniş: §5.3).
- **A* düğüm sayısı (prototip, near64, 80 çift):** statik halka katmanıyla genişletilen düğüm p50 372 / p95 4357; ek 12 rastgele tehlike ile p50 362 / p95 3894 (alan yokken T-NAV-03: p50 306 / p95 2431). Süre kapıları §6 K7.

## 3. Kapsam

**Yapılacaklar**

- `BotCore/NavDanger.h`: `NavCostLayer`, `NavThreatKind`, `NavThreat`, `NavThreatParams`, `NavCostParams`, `NavCostField`, `NavCellPenalty`, `NavZoneParams`, `NavBuildTeamZones` (§5.1).
- `BotCore/NavPath.h`: `#include "NavDanger.h"`, `NavPathResult::length`, `NavPathfinder::Find`'a isteğe bağlı `field` parametresi (§5.2).
- `Tests/BotCoreTests/NavDangerTests.cpp`: §5.3 test listesi (birim + Dijkstra çapraz doğrulama + gerçek harita + süre).
- `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` kayıtları (§5.4).

**Kapsam dışı (yapılmayacak)**

- `NavFollower`/`NavReach`/`NavReachJudge`/`NavSmooth` değişikliği ve alanı onlara bağlamak (`NavFollowParams`'a alan eklemek, halka adaylarını yasaklıya göre süzmek, `Detour` kuralını `length`'e geçirmek: ayrı küçük plan). **`NavTrack.h`, `NavReach.h`, `NavSmooth.h`, `NavGrid.h` dosyalarına dokunma.**
- Canavar spawn dikdörtgenleri ve arama menzili maliyeti (`docs/12` §7: test arenasında bulunmaz), eğim cezası (ADR-0006 Karar 2: T-NAV-02 olmadan eklenmez), takılan kenara dinamik ceza (F5-09), `Safe` bayrağını tüketen güvenli nokta seçimi (F5-07), yol düzleştirmede maliyet farkındalığı, `region_graph`.
- Gerçek bot/algı verisinden tehlike üretmek (`Perception` → `NavThreat` dönüşümü), 500 ms zamanlayıcı, telemetri, kapı koordinatlarını sunucudan okumak (testteki sabitler yeterli).
- `NavCostLayer`'ın iş parçacığı güvenliği (örnek başına bir bağlam; çağıran kopyalar), `NavGrid::Build` değişince katmanı yeniden kurma izlemesi.
- `GameServer/`, `AIServer/`, `shared/`, `docs/` değişikliği, DB, sunucu çalıştırma. `GameServer/proj-GameServer.vcxproj` değişmez.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavDanger.h` | yeni | ASCII, CRLF, başlık-yalnızca (satır içi); yalnızca `NavGrid.h` + standart başlıklar |
| `BotCore/NavPath.h` | değiştir | yalnızca §5.2'deki değişiklikler; CRLF korunur |
| `Tests/BotCoreTests/NavDangerTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="NavDanger.h" />` satırı (BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="NavDangerTests.cpp" />` satırı (BOM ve CRLF korunur) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`build/nav/zone71.navgrid` üretilen çıktıdır, commit edilmez; yoksa `python3 tools/nav-export.py` ile üret.)

## 5. Uygulama adımları

### 5.1 `BotCore/NavDanger.h`

Ad alanı `BotCore`; `#include "NavGrid.h"` (aynı dizin, tırnaklı) ve yalnızca standart başlıklar (`<algorithm> <cmath> <cstddef> <cstdint> <vector>`). **`NavPath.h`'yi include etme** (tersine `NavPath.h` bunu include eder). Stil `NavGrid.h`/`NavReach.h` gibi: tab girinti, Allman, İngilizce yorum, `inline`/sınıf içi tanımlar, global/`static` durum yok.

```cpp
namespace BotCore
{
	// One team's view of the zone (docs/12 s2 danger_static / danger_dynamic, s7): per cell a danger
	// value (0..255 = 0.0..1.0), a forbidden flag (hard: a path may not ENTER it) and a safe flag
	// (marking only; consumed by the safe-point search, F5-07). Copyable: the dynamic layer of a
	// frame is `dynamic = staticLayer; dynamic.AddThreats(...)` (the copy reuses its capacity).
	class NavCostLayer
	{
	public:
		// Sizes the layer for `grid` (side and unit are copied) and clears it. Size() == 0 when
		// grid.Size() < 2. Must be called before any Add*; the Add* calls of an un-Init'ed layer are no-ops.
		void Init(const NavGrid & grid);
		void Clear();                              // danger 0, no flags; keeps the size

		int     Size() const;                      // grid side at Init (0 before)
		uint8_t Danger(int x, int z) const;        // 0 off-grid and before Init
		bool    Forbidden(int x, int z) const;     // false off-grid and before Init
		bool    Safe(int x, int z) const;          // false off-grid and before Init

		// Danger band around (cx, cz) in metres: weight inside [innerM, outerM] (distance from the
		// cell centre to the point, boundaries inclusive), falling linearly to 0 over fadeM outside
		// both edges. Max-combined with the existing value (order independent). weight is clamped
		// to [0, 1] (weight <= 0 is a no-op); innerM < 0 is 0; outerM < innerM is innerM; fadeM < 0
		// is 0 (a hard edge). Only cells in the bounding box [centre -+ (outerM + fadeM)] are visited
		// (clamped to the grid), so the cost is proportional to the band area, not to the grid.
		void AddDangerBand(float cx, float cz, float innerM, float outerM, float fadeM, float weight);
		void AddThreats(const NavThreat * threats, size_t count, const NavThreatParams & params);

		// Cells whose centre is within radiusM of (cx, cz) (squared distance <= radius^2, boundary
		// inclusive); radiusM < 0 is a no-op. Flags are independent (a cell may be both).
		void AddForbidDisc(float cx, float cz, float radiusM);
		void AddSafeDisc(float cx, float cz, float radiusM);
		// Every cell whose centre is farther than radiusM (test-mode arena bound, docs/12 s7).
		void AddForbidOutsideDisc(float cx, float cz, float radiusM);
	};

	enum class NavThreatKind { Melee, Ranged };

	struct NavThreat
	{
		float x = 0.0f;                           // world metres
		float z = 0.0f;
		NavThreatKind kind = NavThreatKind::Melee;
	};

	struct NavThreatParams
	{
		float meleeCoreM = 15.0f;     // [Ö] docs/12 s2: melee core
		float meleeWeight = 1.0f;     // [A]
		float rangedReachM = 45.0f;   // [Ö] docs/12 s2: mage reach (a disc: everywhere in reach is dangerous)
		float rangedWeight = 0.6f;    // [A] (< melee: a mage can be out-ranged, a melee cannot)
		float fadeM = 8.0f;           // [A] linear falloff outside the core / reach
	};

	struct NavCostParams
	{
		float wDanger = 4.0f;            // [A] docs/12 s4.1 w_danger
		float wClear = 0.5f;             // [A] docs/12 s4.1 w_clear
		int   clearFree = 2;             // [Ö] docs/12 s4.1: max(0, 2 - clearance)
		float forbiddenPenalty = 10.0f;  // [A] per forbidden cell (only reachable when the start is inside)
	};

	struct NavCostField
	{
		const NavCostLayer * layer = nullptr;   // may be null: clearance term only, no forbidden rule
		NavCostParams params;
	};

	// Penalty of one cell (>= 0): wDanger * danger/255 + forbiddenPenalty (when forbidden)
	// + wClear * max(0, clearFree - clearance). Negative weights count as 0. A layer whose
	// Size() != grid.Size() (or null) contributes nothing.
	float NavCellPenalty(const NavGrid & grid, const NavCostLayer * layer, const NavCostParams & params, int x, int z);

	struct NavZoneParams
	{
		float towerRingM = 90.0f;     // [Ö] docs/12 s7: gate-ring radius
	};

	// A team's static layer: the ENEMY gate ring is forbidden, the OWN gate ring is marked safe.
	void NavBuildTeamZones(const NavGrid & grid, float enemyGateX, float enemyGateZ, float ownGateX, float ownGateZ,
		const NavZoneParams & params, NavCostLayer & out);
}
```

(Üyeler ve işlevler sınıf içi/`inline` tanımlanır; yukarıdaki iskelet imzaları verir. Beyan sırası: `NavCostLayer` `NavThreat`/`NavThreatParams`'ı kullandığı için bu ikisi **önce** tanımlanır.)

**Kesin kurallar:**

1. **Depolama:** `std::vector<uint8_t> m_danger` ve `m_flags` (bit 0 = yasaklı, bit 1 = güvenli), her biri `n × n`, indeks `x * n + z` (`NavGrid` ile aynı). Ek olarak `m_n`, `m_unit`.
2. **Hücre merkezi:** `(i + 0.5f) * unit` (`NavGrid::CellCenter`); sınır kutusu `floor(değer / unit)` ile (`NavGrid::CellOf` ile aynı), `[0, n - 1]`'e kırpılır.
3. **Band değeri:** `r = sqrt(dx*dx + dz*dz)` (`float`, merkezden hücre merkezine). `innerM > 0 && r < innerM` ise `t = fadeM > 0 ? (innerM - r) / fadeM : 1`; değilse `r <= outerM` ise `t = 0`; değilse `t = fadeM > 0 ? (r - outerM) / fadeM : 1`. `t >= 1` ise hücre atlanır. `v = weight * (1 - t)`; `d = (int)(255.0f * v + 0.5f)`, 255'e kırpılır; `m_danger[i] = max(m_danger[i], (uint8_t)d)`.
4. **Diskler:** `dx = cellCentreX - cx`, `dz = ...`, karşılaştırma `dx*dx + dz*dz <= radiusM*radiusM` (karekök yok; **içeride** = bayrak). `AddForbidOutsideDisc`: `>` ile (tüm hücreler taranır, sınır kutusu yok).
5. **`AddThreats`:** `Melee` → `AddDangerBand(x, z, 0, meleeCoreM, fadeM, meleeWeight)`; `Ranged` → `AddDangerBand(x, z, 0, rangedReachM, fadeM, rangedWeight)`. `count == 0` ya da `threats == nullptr` no-op.
6. **`NavCellPenalty`** (kesin): `layerOk = layer != nullptr && layer->Size() == grid.Size()`; `p = 0`; `layerOk` ise `p += max(0, wDanger) * (float)layer->Danger(x, z) / 255.0f` ve yasaklıysa `p += max(0, forbiddenPenalty)`; `deficit = params.clearFree - (int)grid.Clearance(x, z)`; `deficit > 0` ise `p += max(0, wClear) * (float)deficit`. `Walk` olmayan hücre için çağrılmaz (A* yalnızca `Walk` hücreler için çağırır).
7. **`NavBuildTeamZones`:** `out.Init(grid); out.AddForbidDisc(enemyGateX, enemyGateZ, params.towerRingM); out.AddSafeDisc(ownGateX, ownGateZ, params.towerRingM);`.
8. NaN/sonsuz girdi denenmez (çağıranın sorumluluğu). `NavCostLayer` kopyalanabilir; `Init` yeniden çağrılırsa önceki içerik atılır.

MSVC Level 4 uyarılarını `static_cast` ile çöz (`#pragma warning` ekleme). Özyineleme yok.

### 5.2 `BotCore/NavPath.h` değişiklikleri (yalnızca şunlar)

1. `#include "NavGrid.h"` satırından sonra `#include "NavDanger.h"`. Başlık yorumundaki "Costs are horizontal distance in metres; danger and clearance weights belong to F5-06." cümlesini şuna çevir: "Without a cost field the cost is the horizontal distance in metres; with a NavCostField (F5-06) each step is weighted by cell penalties and a forbidden zone cannot be entered."
2. `NavPathStatus::InvalidGoal` yorumu: `// goal out of bounds / not Walk, or (cost field only) forbidden while the start is not`.
3. `NavPathResult` içine `float length = 0.0f;` (yorum: `// metres (geometric length of the route); 0 unless Found; equals cost without a field`). `Find` başında `out.length = 0.0f;`. `Found` olduğunda (hem `start == goal` yolunda hem ana döngüde `Reconstruct`'tan sonra) `length` = ardışık hücre çiftlerinin adımlarının (`unit` ya da `unit * sqrt(2)`) **başlangıçtan hedefe sırayla** toplamı. `start == goal` için 0.
4. `Find` imzası: `void Find(const NavGrid & grid, NavCell start, NavCell goal, const NavSearchParams & params, NavPathResult & out, const NavCostField * field = nullptr)`. Davranış (`field == nullptr`): **eskisi gibi, bit düzeyinde** (aynı kayan nokta işlemleri, aynı sıra; mevcut dokuz `NavPath_*` testi ve `T-NAV-03` sayıları değişmez). `field != nullptr`:
   - `zones = (field->layer != nullptr && field->layer->Size() == n) ? field->layer : nullptr` bir kez hesaplanır; yasaklı kuralı yalnızca `zones != nullptr` iken işler.
   - Doğrulama sırası: `!Walk(start)` → `InvalidStart`; `!Walk(goal)` → `InvalidGoal`; `zones` var, `zones->Forbidden(goal)` ve `!zones->Forbidden(start)` → `InvalidGoal` (`expanded == 0`, arama yok); `start == goal` → `Found`.
   - Her genişletmede `penCur = NavCellPenalty(grid, zones, field->params, cx, cz)` bir kez hesaplanır. Her komşuda `EdgeOpen` doğruysa: `zones` var, `zones->Forbidden(nb)` ve `!zones->Forbidden(cur)` ise komşu **atlanır** (dışarıdan yasaklıya geçiş yok; içeriden içeriye ve içeriden dışarıya serbest). Adım maliyeti `step * (1.0f + 0.5f * (penCur + NavCellPenalty(grid, zones, field->params, nx, nz)))`.
   - Sezgisel değişmez (octile): ceza ≥ 0 olduğu için kabul edilebilir ve tutarlıdır (`docs/12` §4.1; ADR-0006 Eki F5-06 madde 1). `out.cost` ağırlıklı maliyettir; `out.length` geometrik uzunluk.
   - `field == nullptr` yolunda döngüye ek dallanma/işlem **eklenmesin** (ölçüm: `NAVPATH T-NAV-03` `ms_p95` kötüleşmez, K9).
5. `NavPath.h`'de başka hiçbir şey değişmez (`NavOctile`, `OpenWorse`, `Reconstruct`, havuz). Doğru olan birden çok `if (field)` yerine döngü dışında bir `const bool weighted = field != nullptr;` ve ayrı adım hesabı yapmaktır; kodu çoğaltma (iki ayrı `Find` yazma).

### 5.3 `Tests/BotCoreTests/NavDangerTests.cpp`

`#include "MiniTest.h"`, `<BotCore/NavGrid.h>`, `<BotCore/NavPath.h>`, `<BotCore/NavDanger.h>`, `<BotCore/Rng.h>` (include biçimi `NavReachTests.cpp` gibi), `<algorithm> <chrono> <cmath> <cstdint> <cstdio> <queue> <vector>`. Anonim ad alanında yardımcılar (kendi kopyaların; **kullanılmayan yardımcı tanımlama**, MSVC C4505 uyarısı çıkar): `CellIndex`, `Cell`, `HeightZeros`, `RingEvents`, `GapWall`, `MakeNav`, `PercentileDouble` ve:

- `float PathLength(const NavGrid &, const std::vector<NavCell> &)`: ardışık adımların `unit`/`unit * sqrt(2)` toplamı (başlangıçtan hedefe sırayla).
- `float PathCost(const NavGrid &, const NavCostLayer *, const NavCostParams &, const std::vector<NavCell> &)`: `Σ step * (1 + 0.5 * (pen(a) + pen(b)))`, `pen` = `NavCellPenalty`.
- `int ForbiddenOnPath(const NavCostLayer &, const std::vector<NavCell> &)` ve `bool ReenteredForbidden(const NavCostLayer &, const std::vector<NavCell> &)` (yolda dışarıdan-yasaklıya geçiş var mı).
- `RefDijkstra(grid, layer*, params, start, goal, &cost) -> int`: **bağımsız referans**: `std::priority_queue` ile Dijkstra; komşular `EdgeOpen` + aynı yasaklı kuralı + aynı adım formülü (`NavCellPenalty` çağrılır); dönüş `0` = bulundu (`cost` doldurulur), `1` = yol yok, `2` = hedef yasaklı ve başlangıç değil (`InvalidGoal` beklenen). `A*` ile aynı kodu paylaşmaz (kendi döngüsü).
- `NavCostField Field(const NavCostLayer *, const NavCostParams &)`.

Her testte `NavPathfinder pf;` ve `NavPathResult out;` kullanılır.

| Test adı | İçerik |
|---|---|
| `NavDanger_Layer_Basics` | (a) **Init öncesi:** `NavCostLayer l;` `Size() == 0`, `Danger(5,5) == 0`, `!Forbidden(5,5)`, `!Safe(5,5)`; `AddForbidDisc(82, 82, 20)`, `AddDangerBand(...)`, `AddThreats(nullptr, 0, p)` çökmez, durum değişmez. (b) **Init:** düz 40×40 ızgara (§2 "düz"), `Size() == 40`, tüm `Danger == 0`, hiçbir bayrak yok; ızgara dışı `(-1,5)`, `(40,5)`, `(5,-1)`, `(5,40)` → 0/`false`. Hiç `Init` edilmemiş `NavGrid g0;` ile `l.Init(g0)` → `Size() == 0`. (c) **Yasaklı disk** `AddForbidDisc(82, 82, 20)`: yasaklı hücre sayısı **81**; `(24,20)`, `(25,20)`, `(23,24)` yasaklı; `(26,20)`, `(24,24)` değil; hiçbir hücre `Safe` değil. `AddForbidDisc(82, 82, -1)` değiştirmez. Yeni katmanda `AddForbidDisc(82, 82, 4)`: yasaklı hücreler tam `(19,20) (20,19) (20,20) (20,21) (21,20)`. (d) **Güvenli disk:** `AddSafeDisc(82, 82, 20)` ⇒ `Safe` sayısı 81, `Forbidden` sayısı değişmez; aynı hücre hem yasaklı hem güvenli olabilir (`(20,20)`). (e) **Dış disk:** `AddForbidOutsideDisc(82, 82, 40)`: yasaklı sayısı **1283**; `(20,20)`, `(30,20)` değil; `(31,20)`, `(35,20)` yasaklı. (f) **Clear/kopya:** katman `a` (yasaklı + tehlike + güvenli içerir) `Clear()` → `Size() == 40`, tüm değerler 0/`false`; `NavCostLayer b = a;` sonra `b.Clear()` → `a` değişmez (kopya bağımsız); `b = a;` (atama) `a` ile aynı içerik. |
| `NavDanger_Band_Values` | Düz 40×40, her alt senaryoda yeni katman; `Danger` = §2 tablosu. (a) melee `(0, 15, 8, 1.0)` merkez `(82, 82)`: `(20,20) 255`, `(23,20) 255`, `(24,20) 223`, `(25,20) 96`, `(26,20) 0`, `(23,23) 192`, `(24,23) 96`, `(17,20) 255`; sıfır olmayan hücre **101**. (b) ranged `(0, 45, 8, 0.6)`: `(20,20)`..`(31,20)` hepsi 153, `(32,20) 96`, `(33,20) 19`, `(34,20) 0`, `(7,20) 19`; sıfır olmayan **553**. (c) halka `(20, 30, 8, 1.0)`: `(20,20) 0`, `(23,20) 0`, `(25,20) 255`, `(26,20) 255`, `(23,23) 158`, `(27,20) 255`, `(28,20) 191`, `(29,20) 64`, `(30,20) 0`, `(12,20) 191`. (d) sönümsüz `(0, 16, 0, 1.0)`: `(24,20) 255` (sınır dahil), `(25,20) 0`; sönüm −5 ile `(0, 15, -5, 1.0)`: `(23,20) 255`, `(24,20) 0`. (e) ağırlık 2,0 `(0, 15, 8, 2.0)`: `(24,20) 223`, `(25,20) 96` (1,0 ile aynı); ağırlık 0 ve −1: katman tamamen 0. (f) **Max-birleşim:** A = melee `(0, 15, 8, 0.4)` merkez `(82, 82)`, B = melee `(0, 15, 8, 1.0)` merkez `(106, 82)`: `(20,20) 102`, `(21,20) 102`, `(22,20) 223`, `(23,20) 255`, `(27,20) 255`; B sonra A eklenince tüm hücreler aynı (`Danger` dizisi hücre hücre eşit). (g) **Izgara dışı merkez:** merkez `(-10, 82)` melee: `(0,20) 255`, `(1,20) 223`, `(2,20) 96`, `(3,20) 0`; merkez `(-1000, -1000)` ranged: tüm katman 0 (çökme yok). |
| `NavDanger_Threats` | Düz 40×40. (a) Varsayılan `NavThreatParams`: `meleeCoreM == 15.0f`, `meleeWeight == 1.0f`, `rangedReachM == 45.0f`, `rangedWeight == 0.6f`, `fadeM == 8.0f`; `NavCostParams`: `wDanger == 4.0f`, `wClear == 0.5f`, `clearFree == 2`, `forbiddenPenalty == 10.0f`; `NavZoneParams::towerRingM == 90.0f`. (b) Tek `Melee` tehlike `(82, 82)`: tablo (a) ile aynı, sıfır olmayan 101; tek `Ranged`: sıfır olmayan 553, `(31,20) 153`. (c) `{Melee, Ranged}` aynı merkezde: `(20,20) 255`, `(24,20) 223`, `(25,20) 153`, `(32,20) 96`, `(33,20) 19`, `(23,23) 192`, sıfır olmayan 553; **sıra bağımsız** (`{Ranged, Melee}` aynı katman). (d) Özel parametre: `fadeM = 0`, `meleeCoreM = 16`: `(24,20) 255`, `(25,20) 0`; `meleeWeight = 0.4f`: `(20,20) 102`. (e) `count == 0` ve `nullptr` katmanı değiştirmez. (f) **Dinamik kare deseni:** `base` katmanı (yasaklı disk + güvenli disk), `dyn = base;` ardından `dyn.AddThreats(...)`: `dyn`'de tehlike var, bayraklar `base` ile aynı; `base`'in `Danger` dizisi hâlâ tümüyle 0 (kopya bağımsız); ikinci karede `dyn = base;` ile tehlike silinir (`Danger` hepsi 0). |
| `NavDanger_Penalty` | Düz 40×40. `NavCostField` varsayılan parametrelerle. (a) `layer = nullptr`: `NavCellPenalty(grid, nullptr, p, 1, 10) == 0.5` (`1e-6`), `(2,10) == 0`, `(20,20) == 0`. (b) Melee bandı (ağırlık 1,0, `(82, 82)`) katmanı: `(20,20) == 4.0`, `(24,20) == 4 * 223 / 255` (`1e-4`), `(30,20) == 0`. (c) Üstüne `AddForbidDisc(82, 82, 4)`: `(20,20) == 14.0`, `(19,20) == 14.0`, `(21,20) == 14.0` (danger 255 + yasaklı); `(22,20) == 4.0` (danger 255, yasaklı değil). (d) Parametreler: `wDanger = 0` ⇒ `(20,20)` = 10.0; `wDanger = -1` ⇒ 10.0 (negatif 0); `forbiddenPenalty = -3` ⇒ `(20,20)` = 4.0; `wClear = 2.0` ⇒ `(1,10)` = 2.0; `clearFree = 3` ⇒ `(2,10)` = 0.5, `(1,10)` = 1.0; `clearFree = 0` ⇒ `(1,10)` = 0. (e) **Boyut uyuşmazlığı:** 30×30 ızgara için kurulan katman 40×40 ızgarayla kullanılınca katkı 0 (`(20,20)` = 0, `(1,10)` yalnızca clearance 0,5). |
| `NavDanger_Path_Field` | Her alt senaryo §2'deki A* senaryosudur (S1–S5), kendi ızgarası ve katmanıyla. (a) **Alan yok ≡ eski:** S1 ızgarasında `Find(..)` (5 argüman) ve `Find(.., nullptr)`: aynı sonuç; `cost == 120.0` (`1e-3`), `|length - cost| <= 1e-3`. (b) **Sıfır ağırlık eşdeğerliği:** 10 tohum × 40 çift, `RingEvents(24)` + rastgele engel/yükseklik (`NavReach_Matches_AStar` üretimi: önce olay `NextBelow(100) < 8`, sonra yükseklik `NextBelow(5) == 0 ? 6 : 0`, hücre başına sabit sıra), `Rng rng(3000 + seed)`: `Find(grid, a, b, sp, plain)` ile `Find(grid, a, b, sp, zero, &field)` (`field.layer = nullptr`, `wDanger = 0`, `wClear = 0`): `status`, `cells`, `expanded` **eşit**, `cost == cost` (bit düzeyinde `==`), `|length - cost| <= 1e-3`. En az 150 çift `Found`. (c) **S1:** `Found`; `|cost - 139.882| <= 0.05`; yol üzerindeki en büyük `Danger == 0`; `|length - cost| <= 0.01`; `wDanger = 0` ⇒ `|cost - 120| <= 1e-3`. (d) **S2:** `Found`, `|cost - 191.255| <= 0.05`, `|length - 40| <= 1e-3`, yol `x = 15` sütunundan geçen hücrenin `z ∈ {14, 15}`. (e) **S3:** `Found`, `|cost - 66.142| <= 0.05`, `|length - 63.314| <= 0.01`, `length < cost`, yolun uç hücreler dışındaki her hücresinde `Clearance >= 2`; alan yokken `cost == 60.0` (`1e-3`). (f) **S4:** dört çift §2'deki gibi (maliyet ve durumlar); yasaklı sayısı yalnızca `ForbiddenOnPath` ile ve `ReenteredForbidden == false` ile sınanır; `(18,20)→(22,20)`: `cells.size() == 5`; `InvalidGoal` durumunda `expanded == 0`, `cells` boş, `cost == 0`, `length == 0`. Aynı üç Found/InvalidGoal çifti alan yokken `Found`. (g) **S4b:** `NoPath` (`cells` boş); alan yokken `Found`, `|cost - 40| <= 1e-3`. (h) **S5:** `Found`, `|cost - 32| <= 1e-3`; `(20,20) → (35,20)` `InvalidGoal`. (i) `start == goal` (alan var): `Found`, `cells.size() == 1`, `cost == 0`, `length == 0`. (j) Alanda `layer` var ama `Size()` uyuşmuyor (30×30 katman, 40×40 ızgara): sonuç alan yokken (yalnızca clearance terimi: `layer = nullptr` ile) aynıdır. |
| `NavDanger_Path_Optimal` | **Dijkstra çapraz doğrulama + özellikler** (rastgele küçük haritalar). 30 tohum (`seed = 0..29`), `Rng rng(4000 + seed)`, `n = 24`; harita `NavReach_Matches_AStar` gibi; `Walk` hücre listesi < 2 ise tohum atlanır. Katman: `layer.Init(grid)`; 3 tehlike bandı: her biri için `rng.NextBelow(96)` ile merkez x, `rng.NextBelow(96)` ile merkez z (metre, tamsayıdan `float`), `rng.NextBelow(2)` ile tür (0 = melee `(0, 15, 8, 1.0)`, 1 = ranged `(0, 45, 8, 0.6)`); `seed < 15` ise bir yasaklı disk (merkez iki `NextBelow(96)`, yarıçap 12) ve bir güvenli disk (aynı biçimde). Parametre: çift `seed` varsayılan `NavCostParams`; tek `seed`: `wDanger = 10`, `wClear = 1.5`, `clearFree = 3`, `forbiddenPenalty = 3`. 100 çift (`rng.NextBelow(count)` ile iki `Walk` hücre; `a == b` olabilir), `params.maxNodes = 100000`. Her çift: `RefDijkstra` ve `pf.Find(.., &field)`; beklenen: ref `2` ⇔ `InvalidGoal` (ve `expanded == 0`); ref `0` ⇔ `Found` ve `|cost - refCost| <= 0.01 + 1e-4 * refCost`; ref `1` ⇔ `NoPath`. `Found` yolları için: ardışık her çift `EdgeOpen`; `ReenteredForbidden == false`; `|PathCost(..) - out.cost| <= 0.01 + 1e-4 * cost`; `|PathLength(..) - out.length| <= 1e-3`; `length <= cost + 1e-3`. **Simetri:** `seed >= 15` (yasaklı disk yok) iken `Found(a→b)` ise `Found(b→a)` ve `|cost(a→b) - cost(b→a)| <= 0.01 + 1e-4 * cost`. Sayaçlar (tüm tohumlar): `found`, `invalidGoal`, `noPath`; `REQUIRE(found >= 1000)`, `REQUIRE(invalidGoal >= 20)`, `REQUIRE(noPath >= 300)` (prototip: 1861 / 75 / 1064). Yazdır: `NAVDANGER random maps: seeds=30 pairs=3000 found=<n> invalid_goal=<n> no_path=<n>`. |
| `NavDanger_RealMap` | Gerçek harita (aşağıya bak). |
| `NavDanger_Perf` | Gerçek harita (aşağıya bak): A* süresi ve dinamik katman yeniden kurma süresi. |

**Gerçek harita ortak kuralı** (F5-02..F5-05 ile aynı): dosya yolu `build/nav/zone71.navgrid` (çalışma dizini depo kökü); açılamazsa testi **başarısız yapma**: `std::printf("NAVDANGER real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n")` yaz ve dön. `LoadFile` + `Build()` (varsayılan `NavParams`) sonrası `MainComponentCells() == 88508` `REQUIRE` edilir. Sabitler: Karus kapısı `(1375, 1085)`, El Morad kapısı `(622, 911)`; arena A hücresi `(318, 222)`, B `(186, 276)`; `NavZoneParams` varsayılan (90 m).

`NavDanger_RealMap`:

- **Takım katmanları:** `NavBuildTeamZones(grid, 1375, 1085, 622, 911, zp, elm)` (El Morad takımı: düşman Karus) ve `NavBuildTeamZones(grid, 622, 911, 1375, 1085, zp, kar)` (Karus takımı). Hücre sayımları (tüm `n × n` hücre taranır): `elm`: yasaklı **1594** (bunlardan `Walk` **1264**), güvenli **1591** (`Walk` **1232**); `kar`: yasaklı **1591** (`Walk` **1232**), güvenli **1594** (`Walk` **1264**). Her iki kapı hücresi `(343, 271)` ve `(155, 227)` `Walk` değildir (bilgi; `CHECK`).
- **Arena A → B:** alan yok: `Found`, `|cost - 660.617| <= 0.01`. `elm` ve `kar` ile ayrı ayrı, `wDanger = 0`, `wClear = 0`: `Found`, `|cost - 660.617| <= 0.01`, yolda yasaklı hücre 0. Varsayılan parametrelerle (`elm`): `|cost - 689.103| <= 0.1`, `|length - 676.617| <= 0.1`, `cost > 660.617`.
- **Hedef halkanın içinde:** taramada (x artan, z artan) ilk `Walk` ve `elm.Forbidden` hücre `F`; `REQUIRE(F == (321, 268))`; `Find(A → F, &elm)`: `InvalidGoal`, `expanded == 0`; alan yokken `Found`.
- **Başlangıç halkanın içinde:** kapıya en yakın `Walk` hücre `S` (en küçük `dx² + dz²`, eşitlikte taramada ilk; `REQUIRE(S == (342, 272))`): `Find(S → A, &elm)` varsayılan parametrelerle `Found`; `|cost - 1171.853| <= 0.5`, `|length - 292.284| <= 0.1`; yolda yasaklı hücre sayısı **22**, hepsi yolun başında (öbek), tek içeriden-dışarıya geçiş, `ReenteredForbidden == false`; alan yokken `|cost - 253.824| <= 0.01`.
- **Çapraz çift:** `a = (371, 248)`, `b = (334, 308)` (ikisi `Walk`, `elm`'de yasaklı değil): alan yok `Found`, `|cost - 310.676| <= 0.01`, yolda **21** yasaklı hücre; `elm` + `wDanger = wClear = 0`: `Found`, `|cost - 315.362| <= 0.05`, yasaklı hücre **0**; `cost` > alan yoksa.
- **Halka çevresi taraması (AC-NAV-06 altyapısı):** adaylar = Karus kapısına `d² ∈ (90², 170²]` olan `Walk` hücreler (x artan, z artan); `REQUIRE(count == 3689)`. `Rng rng(20261002)`; 500 çift, her biri `NextBelow(count)` ile iki aday çekilir, `a == b` ya da Chebyshev mesafesi > 64 ise reddedilir ve yenisi çekilir. Her çift: alan yok `Find` ve `elm` + `wDanger = wClear = 0` ile `Find`. Sayaçlar: `found`, `noPath`, `nodeLimit`, `plainCross` (alan yokken `Found` ve yol yasaklı hücreden geçiyor). Her `Found` alan yolunda yasaklı hücre sayısı **0** (`CHECK`, ilk ihlalde çift koordinatlarını `std::printf` ile yaz). `CHECK(found * 100 >= 500 * 90)`, `CHECK(plainCross >= 100)` (prototip: ≈ %98 ve ≈ %33); alan yolu maliyeti ≥ alan yok maliyeti − 0,01 (`Found` ikisi de ise).
- Yazdır: `std::printf("NAVDANGER real: elm_forbid=%d elm_forbid_walk=%d elm_safe=%d elm_safe_walk=%d; arena A->B cost=%.3f default=%.3f; cross plain=%.3f field=%.3f; start-inside cost=%.3f forb=%d; ring sweep pairs=%d found=%d no_path=%d node_limit=%d plain_cross=%d violations=%d\n", ...)`.
- Not (planın doğruladığı sayı tutmazsa): sayı beklentiden saparsa **kuralı sayıya uydurma**; dur ve Uygulayıcı Raporu'nda sor.

`NavDanger_Perf` (gerçek harita, aynı `SKIPPED` kuralı):

- `elm` katmanı kurulur (süre ölçülmez). Adaylar: `Walk` ve `!elm.Forbidden` hücreler (x-ana tarama, tek `std::vector<NavCell>`). Sorgu sayısı `Q` **Release'te 1000, Debug'da 100** (`#ifdef _DEBUG`). Çiftler `Rng(20261002)`'den `NextBelow(count)` ile çekilir; başlangıç == hedef ise ya da Chebyshev mesafesi 64'ü aşıyorsa reddedilir (`near64`, F5-02..F5-05'teki küme). Bir `NavPathfinder` yeniden kullanılır. **Isınma:** ölçümden önce aynı kümeden 20 ek çift koşulur, sayılmaz.
- İki küme (aynı çiftler): **`zones`**: alan = `elm`, varsayılan `NavCostParams`; **`threats`**: her çift için `dyn = elm; dyn.AddThreats(12 tehlike)` (tehlikeler `Rng`'den: her çift için 12 `Walk` hücre merkezi, ilk 6 `Melee`, sonraki 6 `Ranged`; varsayılan `NavThreatParams`), alan = `dyn`. Yeniden kurma süresi (`dyn = elm` + `AddThreats`) ayrıca ölçülür (`rebuild`); A* süresi (`Find`) yalnızca `Find` çağrısıdır.
- İstatistik (küme başına): `found`, `noPath`, `nodeLimit`, genişletilen düğüm p50/p95, süre p50/p95/p99 (ms). `rebuild`: süre p50/p95/p99 (ms; `Q` örnek).
- Yazdır (**tam bu biçim**):

```
NAVDANGER perf set=zones queries=<n> found=<n> nopath=<n> nodelimit=<n> expanded_p50=<n> expanded_p95=<n> ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
NAVDANGER perf set=threats queries=<n> found=<n> nopath=<n> nodelimit=<n> expanded_p50=<n> expanded_p95=<n> ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
NAVDANGER perf rebuild samples=<n> threats=12 ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
```

- `CHECK`'ler (tüm çiftler): her `Found` yolunda yasaklı hücre sayısı 0 (başlangıç ve hedef yasaklı değildir); `found + noPath + nodeLimit == queries`; `zones`: `found * 100 >= Q * 95`; `threats`: `found * 100 >= Q * 90`. **`#ifndef _DEBUG`** (Release): `zones` ve `threats` için **`ms_p95 <= 2.0`** (AC-NAV-02 ile aynı bütçe `[Ö]`), `rebuild` için **`ms_p95 <= 0.5`** `[A]`. Debug'da süre kapısı yoktur (satırları yine yazdır).

### 5.4 Proje dosyaları

1. `BotCore/BotCore.vcxproj`: `ClInclude` grubuna `<ClInclude Include="NavDanger.h" />` (`NavGrid.h` satırından sonra, `NavPath.h` satırından önce; dosyada şu an `:74-75`). BOM/CRLF korunur.
2. `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubuna `<ClCompile Include="NavDangerTests.cpp" />` (`NavGridTests.cpp` satırından sonra, `NavPathTests.cpp` satırından önce). BOM/CRLF korunur.
3. Bu iki projenin `.filters` dosyası yoktur (F5-01'de doğrulandı); ek dosya yok.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; `BotCore` ve `BotCoreTests` için **yeni uyarı yok** (Level 4; `NavDanger.h`, `NavPath.h`, `NavDangerTests.cpp` dosyalarını `touch` ile zorla yeniden derle; çıktıda `NavDanger` ya da `NavPath` geçen `warning` satırı bulunmaz).
- [ ] K2: `./tools/run-tests.sh Release --no-build --list` çıktısı şu sekiz test adını içerir: `NavDanger_Layer_Basics`, `NavDanger_Band_Values`, `NavDanger_Threats`, `NavDanger_Penalty`, `NavDanger_Path_Field`, `NavDanger_Path_Optimal`, `NavDanger_RealMap`, `NavDanger_Perf`.
- [ ] K3: `build/nav/zone71.navgrid` varken (yoksa `python3 tools/nav-export.py` ile üret) `./tools/run-tests.sh Release --no-build` çıkış kodu 0; çıktıda `114 tests, 0 failed` (önceki 106 + sekiz yeni), tüm eski testler ve sekiz yeni test `[ OK ]`; `SKIPPED` geçmiyor; `NAVDANGER random maps: …` satırı (`found ≥ 1000`, `invalid_goal ≥ 20`, `no_path ≥ 300`), `NAVDANGER real: …` satırı ve üç `NAVDANGER perf …` satırı var.
- [ ] K4: Gerçek harita yokken (`build/nav/zone71.navgrid` geçici olarak başka ada taşınarak) `./tools/run-tests.sh Release --no-build NavDanger_` çıkış kodu 0, `NavDanger_RealMap` ve `NavDanger_Perf` `SKIPPED` yazar ve diğer altı test geçer; ardından dosya yerine geri konur.
- [ ] K5: `./tools/run-tests.sh Debug` (derleme dahil) çıkış kodu 0 (Debug'da `assert`/sınır hataları yok).
- [ ] K6: Davranış sayıları (Release ve Debug): `NavDanger_Band_Values` ve `NavDanger_Threats` (özellikle sıfır olmayan hücre 101 / 553, `223`/`96`/`153`/`19`), `NavDanger_Path_Field` (S1 139,882; S2 191,255; S3 66,142/63,314; S4 251,598 ve 176,0; `InvalidGoal`; S4b `NoPath`; S5 32,0), `NavDanger_Path_Optimal` (Dijkstra eşitliği, 3000 çiftte hiç uyuşmazlık yok) ve `NavDanger_RealMap` (`elm_forbid=1594 elm_forbid_walk=1264 elm_safe=1591 elm_safe_walk=1232`; `arena A->B cost=660.617`; çapraz çift yolu halkaya girmez; halka çevresi taramasında `violations=0`) `[ OK ]`.
- [ ] K7: **Süre (Release):** `NAVDANGER perf set=zones` ve `set=threats` satırlarında **`ms_p95 ≤ 2.000`**, `queries=1000`; `NAVDANGER perf rebuild` satırında **`ms_p95 ≤ 0.500`**. Uygulayıcı Raporu satırları **aynen** yapıştırır ve `nproc` bilgisini ekler. Kapı tutmuyorsa: testi üç kez koş (en kötüsünü raporla); tutarlı biçimde aşıyorsa kural gevşetilmez: `NavDanger.h`/`NavPath.h` içinde iyileştir (ör. ceza hesabını kenar başına yerine hücre başına bir kez yapmak, `Danger`/bayrak okumalarını tek diziye toplamak); hâlâ aşıyorsa durup ölçümleri "Açık sorular"da raporla.
- [ ] K8: Saflık/kapsam: `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavDanger.h` çıktısı boş; `grep -n "include" BotCore/NavDanger.h` içinde `NavPath.h` yok; `git diff --stat gece/2026-10-02-nav...bot/F5-06` yalnızca §4'teki beş dosyayı ve kendi plan dosyasını gösterir (`GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavTrack.h`, `NavReach.h`, `NavSmooth.h` yok); `git diff gece/2026-10-02-nav...bot/F5-06 -- BotCore/NavPath.h` yalnızca §5.2'deki değişiklikleri gösterir (`#include`, yorumlar, `length`, `Find` imzası/gövdesi).
- [ ] K9: F5-01..F5-05 davranışı bozulmadı: `./tools/run-tests.sh Release --no-build Nav_` (10 test), `NavPath_` (9), `NavSmooth_` (8), `NavTrack_` (10), `NavReach_` (8) çıkış kodu 0, hepsi `[ OK ]` (**değişmeden**, testler düzenlenmez); `NAVPATH T-NAV-03 set=near64 …` satırında `found=997` (aynı sayı), `expanded_p50=306 expanded_p95=2431` (aynı sayılar) ve `ms_p95 ≤ 2.000` (alan yok yolunda süre kötüleşmez: F5-05 raporundaki 0,54 ms'ye göre ≤ 1,5 kat); `NAVTRACK chase ring=0-0 replans=29 planned=29 caught_ms=11400 …` ve `NAVREACH real: components=143 largest=88279 pockets=229 …` (aynı sayılar).

## 7. Doğrulama komutları

```bash
git switch -c bot/F5-06 gece/2026-10-02-nav
python3 tools/nav-export.py            # build/nav/zone71.navgrid yoksa
./tools/build.sh Release
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Release --no-build NavDanger_
./tools/run-tests.sh Release --no-build NavPath_
./tools/run-tests.sh Debug
grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavDanger.h
grep -n "include" BotCore/NavDanger.h
git diff --stat gece/2026-10-02-nav...bot/F5-06
git diff gece/2026-10-02-nav...bot/F5-06 -- BotCore/NavPath.h
```

## 8. Kısıtlar ve uyarılar

- **Paralel hat:** sunucuya hiç dokunma (`tools/run-servers.sh` çağırma; ana hat başka çalışma ağacından sunucu çalıştırıyor olabilir). DB'ye bağlanma. Dal tabanı `gece/2026-10-02-nav`.
- Kodlama/satır sonu: `AGENTS.md` §3. Yeni C++ dosyaları ASCII + CRLF + tab + Allman. Yorumlar İngilizce; plan metnindeki Türkçe açıklama koda girmez. `NavPath.h` değişiklikleri de CRLF ile.
- Başlık-yalnızca: tüm tanımlar `inline` (sınıf içi tanımlar zaten satır içidir; `NavCellPenalty` ve `NavBuildTeamZones` `inline` bildirilir); `static` global durum yok; `#pragma` yalnızca `once`.
- **Belirlenim:** aynı ızgara, aynı katman ve aynı çağrı sırası her zaman aynı yolu verir (rastgelelik yok; beraberlik kuralı F5-02'dekiyle aynı). Zaman yalnızca parametredir.
- **Alan yokken A* aynıdır:** `field == nullptr` yolu F5-02'nin davranışını **bit düzeyinde** korur; mevcut testleri düzenleme, geçmezse kodu düzelt. Alan yolundaki ek işler (`NavCellPenalty`, yasaklı denetimi) yalnızca `field != nullptr` iken çalışır.
- **Yasaklı bölge kuralı kesindir:** hiçbir yol dışarıdan yasaklı hücreye girmez (AC-NAV-06); başlangıç içerideyse çıkış serbesttir ve yasaklı ceza (10) en kısa çıkışı seçtirir. Hedef yasaklıysa `InvalidGoal` döner (arama yapılmaz; `NodeLimit` belirsizliği olmaz). Tehlike **yumuşaktır**: geçit yalnızca tehlikeli bölgeden geçiyorsa yol yine bulunur (S2).
- Ceza ≥ 0 olmalıdır (negatif ağırlık 0 sayılır): bu, octile sezgiselinin kabul edilebilirliğinin ve Dijkstra eşitliğinin dayanağıdır. Ağırlıkları değiştirmek Dijkstra testini bozmaz ama sayısal beklentileri (S1–S4, gerçek harita) değiştirir; varsayılanları değiştirme.
- `NavFollower`/`NavReach` bu planda **alan kullanmaz**; `NavFollowPlan::pathCost` ve `Detour` kuralı değişmez. Alan bağlanınca `Detour` kuralı `length` ile çalışmalıdır (ağırlıklı `cost` ile değil); bu ayrı planın konusudur (ADR-0006 Eki F5-06 madde 7).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1
