# F5-03: Yol düzleştirme (`BotCore/NavSmooth.h`: Bresenham görüş denetimi + ara nokta ayıklama)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat, `docs/17` §1 "Paralel yürütülebilir işler") |
| Branch | `bot/F5-03` (taban: `gece/2026-10-02-nav`) |
| Bağımlı olduğu planlar | F5-01 (`BotCore/NavGrid.h`) ve F5-02 (`BotCore/NavPath.h`): ikisi `KAPANDI`, `gece/2026-10-02-nav` içinde (merge `788aa86`, `dc1bb10`) |
| İlgili gereksinim / kabul | `docs/12` §4.1 ("Yol düzleştirme: hücre merkezleri arasında, ızgara üzerinde Bresenham yürüyüşü engelsizse ara noktalar atlanır"), REQ-NAV-01; ADR-0006 (Eki F5-03 bu planın kararlarını taşır), ADR-0016 (`BotCore` saflığı) |
| Tahmini büyüklük | S (4 dosya: 2 yeni, 2 değişen; ~450 satır) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

F5-02'nin hücre yolunu (`NavPathResult::cells`, her adım bir komşu hücre) **ara nokta listesine** indirgeyen saf mantık yazmak: `BotCore/NavSmooth.h`. İki parça:

1. `NavLineClear(grid, a, b)`: iki hücre arasındaki **Bresenham yürüyüşü** (tamsayı, belirlenimli, simetrik) yalnızca `NavGrid::EdgeOpen` kenarlarından oluşuyorsa `true`. Yani düzleştirilmiş her doğru parçası, A*'ın yürüyebileceği geçerli bir hücre yürüyüşüdür (yürünebilirlik, eğim ve köşe kesmeme kuralları yeniden yazılmaz, yalnızca `EdgeOpen`'dan gelir).
2. `NavSmoothPath(grid, path, params, out)`: hücre yolundan açgözlü "ileri görünür en uzak nokta" ayıklaması; sonuç ara hücreler (başlangıç ve hedef dahil, yolun alt dizisi) ve metre cinsinden Öklid uzunluğudur.

Bu planın sonunda **hareketli hedef/yeniden planlama, ulaşılamaz hedef kararı, tehlike/clearance maliyeti, hareketi dünya koordinatına çevirme ve `WIZ_MOVE` üretimi yoktur** (F5-04..F5-06 ve sunucu entegrasyonu). `GameServer/`, `AIServer/`, `shared/` değişmez; sunucu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0006-navigasyon-izgara-astar.md` — kararın çerçevesi (ızgara A*, `NodeLimit` ≠ ulaşılamaz). **Eki F5-03** (bu plan yazılırken eklendi) bu planın tasarım kararlarını verir: görünürlük = kenar-geçerli Bresenham yürüyüşü, kanonik yön, ileri bakış penceresi `[A]`.
- `docs/12_NAVIGATION_AND_POSITIONING.md` §4.1 (düzleştirme cümlesi), §11 (AC-NAV-02: A* p95 ≤ 2 ms; düzleştirme bu bütçeyi yemesin diye kendi süre kapısı §5.2 `NavSmooth_Perf`'tedir).
- `BotCore/NavGrid.h` (F5-01) — kullanacağın API: `Walk(x, z)`, `EdgeOpen(x, z, dx, dz)` (`NavGrid.h:316-334`: iki uç `Walk`, eğim ≤ `maxSlope * mesafe`, çaprazda iki ortogonal komşu da `Walk`), `Unit()`, `InBounds`. Dosya F5-01 testleriyle sabitlenmiştir; **değiştirme**.
- `BotCore/NavPath.h` (F5-02) — `NavCell` (`x`, `z`; `==`, `!=`), `NavPathfinder::Find`, `NavPathResult` (`cells` başlangıç→hedef, her ardışık çift komşu), `NavOctile`. **`NavPath.h` değişmez**; `NavSmooth.h` onu `#include "NavPath.h"` ile (tırnaklı, aynı dizin) alır.
- `BotCore/Rng.h` — testlerde `BotCore::Rng` (`NextBelow`, `NextDouble`); global `rand()` yasak.
- `Tests/BotCoreTests/NavPathTests.cpp` — **kopyalanacak kalıplar** (başka `.cpp`'den içe aktarma yok; yeni dosyada kendi kopyalarını yaz): `CellIndex`, `RingEvents`, `MakeNav(n, unit, events, heights)`, gerçek harita yükleme + `SKIPPED` kalıbı, `Rng(20261002)` ile `near64` çifti çekimi, `std::chrono::steady_clock` süre ölçümü ve p50/p95/p99 hesabı.
- `Tests/BotCoreTests/MiniTest.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`), `tools/run-tests.sh` (testi depo kökünde çalıştırır; `build/nav/zone71.navgrid` göreli açılır).

Planı yazarken doğrulanan gerçekler (Python prototipi, 2026-10-02, depoya girmeyen geçici; aynı kenar kuralı, aynı Bresenham ve aynı açgözlü algoritma; **sayılar test eşikleridir**, tam eşitlik yalnızca küçük el yapımı haritalarda istenir):

- Zone 71 ana bileşeni 88 508 hücre; arena A (1274, 890) → hücre (318, 222), B (746, 1106) → hücre (186, 276). A* yolu **150 hücre**, maliyet **660,617 m** (F5-02 doğrulaması ile aynı). Düzleştirilmiş: **13 ara nokta**, uzunluk **635,787 m**; düz çizgi alt sınırı `4 * sqrt(132² + 54²)` = **570,47 m**. (C++ A*'ın beraberlik sırası farklı bir eşit-maliyetli yol seçebilir; bu yüzden C++ testi aralık ister: ara nokta ≤ 25, uzunluk 570,4..661,1.)
- `near64` kümesi (Chebyshev ≤ 64 hücre, 200 bulunan sorgu, prototip tohumu): ortalama 55,1 hücre → 6,1 ara nokta; sorgu başına en kötü `ara nokta / hücre` oranı 0,36; ortalama `uzunluk / A* maliyeti` 0,9575 (en kötü 0,924); `EdgeOpen` çağrısı sayısı sorgu başına p50 592, p95 2 612, en çok 8 838 (≈ 40 µs mertebesi).
- Elle türetilen küçük haritalar (prototipte doğrulandı): 12×12 halka, `x = 6` sütununda `z ∈ [1, 10]` engelli, yalnızca `(6, 5)` açık; yol `(5,4),(5,5),(6,5),(7,5),(7,6)` düzleştirilince `(5,4),(5,5),(7,5),(7,6)` olur (uzunluk 16,0; `(5,4)→(7,6)` ve `(5,4)→(7,5)` köşe kesme nedeniyle kapalı). 20×20 halka, `x = 10` sütununda `z ∈ [1, 13]` engelli: `(5,5)→(15,5)` A* maliyeti 93,255 m (21 hücre) → düzleştirilmiş 4 ara nokta `(5,5),(9,14),(11,14),(15,5)`, uzunluk 86,791 m.

## 3. Kapsam

**Yapılacaklar**

- `BotCore/NavSmooth.h`: `NavLineClear`, `NavSmoothParams`, `NavSmoothResult`, `NavSmoothPath` (§5.1).
- `Tests/BotCoreTests/NavSmoothTests.cpp`: §5.2 test listesi (birim + gerçek harita + süre).
- `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` kayıtları (§5.3).

**Kapsam dışı (yapılmayacak)**

- `NavPath.h`/`NavGrid.h` değişikliği (düzleştirme `NavPathfinder`'a bağlanmaz; `FindSmooth` gibi bir sarmalayıcı **yazma**: birleştirme F5-04'ün işidir).
- Sürekli uzayda (dünya koordinatı) görünürlük denetimi, hücre merkezi dışında serbest nokta, yüksekliği (y) olan dünya yolu (hareket uygulamasında `HeightAt` ile yapılır, sunucu entegrasyonu ayrı plan), `supercover` (tüm kesilen hücreler) çizgi taraması: bu planın tanımı **Bresenham**'dır (bkz. §8 bilinen sınırlama).
- Yol "yumuşatma" (spline, kavis), Theta*/any-angle arama, funnel/portal algoritması, tehlike/clearance'e göre yolu duvardan uzak tutma (F5-06).
- Hareketli hedef, yeniden planlama (F5-04), ulaşılamaz hedef (F5-05), güvenli nokta (F5-07), formasyon (F5-08), takılma (F5-09), LoS (F5-10; `NavLineClear` **LoS değildir**: LoS `advisory` ve engel/yükseklik kuralı farklı, F5-10 ayrı yazılır).
- Yeni `P-NAV-*` ayar dosyası/okuyucusu (`NavSmoothParams` varsayılanları yeterli), telemetri (`NAV_PATH` olayı vb.), sunucu entegrasyonu.
- `GameServer/`, `AIServer/`, `shared/`, `docs/` değişikliği, DB, sunucu çalıştırma. `GameServer/proj-GameServer.vcxproj` değişmez.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavSmooth.h` | yeni | ASCII, CRLF, başlık-yalnızca (satır içi) |
| `Tests/BotCoreTests/NavSmoothTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="NavSmooth.h" />` satırı (BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="NavSmoothTests.cpp" />` satırı (BOM ve CRLF korunur) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`build/nav/zone71.navgrid` üretilen çıktıdır, commit edilmez; yoksa `python3 tools/nav-export.py` ile üret.)

## 5. Uygulama adımları

### 5.1 `BotCore/NavSmooth.h`

Ad alanı `BotCore`; `#include "NavPath.h"` (aynı dizin, tırnaklı; `NavGrid.h`'yi o getirir) ve yalnızca standart başlıklar (`<cmath> <cstddef> <vector>`). Çevredeki kodla aynı stil (`NavPath.h`): tab girinti, Allman, İngilizce yorum, `inline` tanımlar, global/`static` durum yok.

```cpp
namespace BotCore
{
	// True when the Bresenham walk from `a` to `b` exists and every step is an open edge.
	inline bool NavLineClear(const NavGrid & grid, NavCell a, NavCell b);

	struct NavSmoothParams
	{
		// P-NAV-SMOOTH-LOOKAHEAD [A] (ADR-0006 Eki F5-03): how many path cells ahead of the
		// current anchor are tried as the next waypoint. Bounds the cost; values < 1 act as 1
		// (no smoothing: every path cell is kept).
		int maxLookahead = 64;
	};

	struct NavSmoothResult
	{
		std::vector<NavCell> waypoints;   // subsequence of the input path, first and last kept
		float length = 0.0f;              // metres: sum of unit * Euclid distance between waypoints
	};

	// `path` must be a path from NavPathfinder::Find (consecutive cells are neighbours and the
	// steps are open edges); it is trusted, not re-validated. `out` is fully overwritten and
	// must not alias `path`. Deterministic. Empty path -> empty waypoints, length 0.
	inline void NavSmoothPath(const NavGrid & grid, const std::vector<NavCell> & path,
		const NavSmoothParams & params, NavSmoothResult & out);
}
```

**`NavLineClear` kuralı (kesin; her biri §5.2'deki testle sabitlenir):**

1. `a` veya `b` ızgara dışı ya da `!grid.Walk(...)` ise `false` (hemen). `a == b` (ve `Walk`) ise `true`.
2. **Kanonik yön:** `b`, `a`'dan sözlük sırasında (önce `x`, eşitse `z`) küçükse `a` ile `b` yer değiştirir. Böylece `NavLineClear(g, a, b) == NavLineClear(g, b, a)` her zaman (Bresenham beraberlik anları yöne bağımlıdır; kanonik sıra bunu ortadan kaldırır).
3. **Tamsayı Bresenham** (başlangıç `(x, z) = a`; `dx = |b.x - a.x|`, `dz = |b.z - a.z|`, `sx = (a.x < b.x) ? 1 : -1`, `sz = (a.z < b.z) ? 1 : -1`, `err = dx - dz`):

   ```
   while ((x, z) != b):
       e2 = 2 * err
       mx = 0; mz = 0
       if (e2 > -dz) { err -= dz; mx = sx; }
       if (e2 <  dx) { err += dx; mz = sz; }
       if (!grid.EdgeOpen(x, z, mx, mz)) return false
       x += mx; z += mz
   return true
   ```

   (`mx`, `mz` her adımda `{-1,0,1}`'dir ve ikisi birden 0 olamaz; her adım **tek bir komşu kenar**dır. Çapraz adım `EdgeOpen` içinde iki ortogonal komşunun `Walk` olmasını ister: köşe kesme yok.) Yürüyüşün adım sayısı `max(dx, dz)`, toplam uzunluğu `NavOctile(dx, dz, unit)`'e eşittir (bu, §5.2'deki `NavSmooth_Invariants_Random` mülkiyet testinin dayanağıdır).

**`NavSmoothPath` kuralı:**

1. `path` boşsa `waypoints` boş, `length = 0`. Tek hücreyse `waypoints = { path[0] }`, `length = 0`.
2. `waypoints` = `{ path[0] }`; `i = 0`; `last = path.size() - 1`. `i < last` iken: `look = max(params.maxLookahead, 1)`; `j = min(last, i + look)`; **`j > i + 1` ve `!NavLineClear(grid, path[i], path[j])` iken `--j`** (en uzaktan geriye doğru ilk görünür); `path[j]` eklenir, `i = j`. (`j == i + 1` için `NavLineClear` **çağrılmaz**: ardışık yol hücreleri geçerli kenardır, girdi güvenilirdir.)
3. `length` = ardışık ara noktalar için `grid.Unit() * std::sqrt((float)(ddx * ddx + ddz * ddz))` toplamı (`float`; `ddx`, `ddz` hücre farkı, `int` çarpımı taşmaz: ≤ 512²).
4. Sonuç **her zaman** girdinin alt dizisidir, ilk/son korunur ve ardışık her ara nokta çifti `NavLineClear`'dır (bu, `j == i + 1` durumu için yolun geçerli kenarlarından gelir).

Performans notu (zorunlu değil, kapı için serbest): ek bellek ayırma yok (`out.waypoints.clear()` + `reserve(path.size())` yeter); `NavLineClear` kendi içinde bellek ayırmaz.

### 5.2 `Tests/BotCoreTests/NavSmoothTests.cpp`

`#include "MiniTest.h"`, `<BotCore/NavGrid.h>`, `<BotCore/NavPath.h>`, `<BotCore/NavSmooth.h>`, `<BotCore/Rng.h>` (include biçimi `NavPathTests.cpp` gibi). Anonim ad alanında yardımcılar (kendi kopyaların): `CellIndex`, `RingEvents`, `MakeNav`, `SegmentsClear(grid, waypoints)` (her ardışık çift `NavLineClear`), `IsSubsequence(path, waypoints)` (sırayla eşleşen indeksler, ilk/son dahil), `Euclid(grid, a, b)` (`unit * hypot`).

Test adları (kabul kriterleri bu adlara dayanır; her testte `NavSmoothResult`/`NavPathResult` nesnesi `CHECK` öncesi doldurulur, `Found` değilse `REQUIRE`):

| Test adı | İçerik |
|---|---|
| `NavSmooth_LineClear_Basics` | (a) 12×12 halka, düz zemin: yatay `(2,3)→(9,3)`, dikey `(3,2)→(3,9)`, çapraz `(2,2)→(8,8)`, eğimli `(2,3)→(9,7)` → `true`; her çağrı ters yönle **aynı sonuç**. `a == b` Walk hücre → `true`; `a == b` engelli (kenar halkası) → `false`; `a` ya da `b` ızgara dışı `(-1,3)`/`(12,3)` → `false`; engelli uç (`(0,5)`) → `false`. (b) Boşluklu duvar (12×12 halka, `x = 6` sütununda `z ∈ [1,10]` engelli, yalnızca `(6,5)` açık): `(5,5)→(7,5)` → `true`; `(5,4)→(7,6)` → `false`; `(5,4)→(7,5)` → `false`; `(5,4)→(7,4)` → `false`; hepsinde ters yön aynı. (c) Tek engelli hücre `(6,5)` (12×12 halka): `(5,4)→(7,6)` (köşegen, engelden geçer) → `false`; `(5,5)→(7,5)` → `false`; `(5,3)→(7,3)` → `true`. (d) Eğim: 20×20 halka, yükseklik `x < 10` için 0, `x ≥ 10` için 10 m: `(8,5)→(12,5)` → `false` (eşik kenarı eğimden kapalı); `(2,2)→(8,9)` → `true`; `(11,2)→(17,9)` → `true`. |
| `NavSmooth_OpenField` | 40×40 halka, düz zemin, `unit = 4`: `Find((5,5) → (25,17))` sonra `NavSmoothPath` (varsayılan): `waypoints.size() == 2` (`(5,5)` ve `(25,17)`), `length` = `4 * sqrt(544)` = **93,295** (`1e-2` içinde) ve `< cost`. Dik doğru yol `(5,5) → (30,5)`: `waypoints.size() == 2`, `length` = **100,0** (`1e-3`). |
| `NavSmooth_WallGap_NoCornerCut` | Boşluklu duvar haritası (12×12 halka, `x = 6` sütununda `z ∈ [1,10]` engelli, yalnızca `(6,5)` açık). `Find((5,4) → (7,6))` yolu **beş hücre**: `(5,4),(5,5),(6,5),(7,5),(7,6)` (her adım yolun tek geçerli seçeneğidir). `NavSmoothPath`: ara noktalar tam olarak **`(5,4),(5,5),(7,5),(7,6)`**, `length` = **16,0** (`1e-3`); `maxLookahead = 1` ile ara noktalar yolun beş hücresinin tamamıdır. Ters sorgu `(7,6) → (5,4)`: ara nokta sayısı 4, `length` 16,0. |
| `NavSmooth_WallDetour` | 20×20 halka, düz zemin, `x = 10` sütununda `z ∈ [1,13]` engelli: `Find((5,5) → (15,5))`: `Found`, 21 hücre, `cost` ≈ 93,255 (`1e-2`). Düzleştirme: ara nokta sayısı **3 ile 5 arasında**; ilk/son = başlangıç/hedef; `IsSubsequence`; `SegmentsClear`; **`length < cost - 1.0`** (prototip 86,791) ve `length ≥ 40.0 - 1e-3` (düz çizgi alt sınırı); `length ≤ cost + 1e-3`. Sonuç hiçbir ara noktası `x = 10, z ∈ [1,13]` hücresi olmayan geçerli bir yoldur. |
| `NavSmooth_Invariants_Random` | 6 rastgele 28×28 ızgara (`Rng(2000 + k)`, k = 0..5): halka engelli, iç hücrelerin ~%18'i engelli (`NextBelow(100) < 18`), yükseklik `NextDouble() * 3.0`; `Build` sonrası `Walk` hücre listesinden **belirlenimli** 40 çift (`Rng(6000 + k)`). Her `Found` yol için üç `maxLookahead` değeri (`1`, `3`, `64`): ilk = başlangıç, son = hedef; `IsSubsequence`; `SegmentsClear`; `length ≤ cost + 1e-3 * cost + 1e-3` (üçgen eşitsizliği); `length ≥ Euclid(başlangıç, hedef) - 1e-3`; `maxLookahead = 1` ise `waypoints == cells` (birebir). **Mülkiyet:** aynı ızgarada 200 belirlenimli `Walk` çifti (`Rng(7000 + k)`) için `NavLineClear(a, b) == NavLineClear(b, a)`; `NavLineClear` true ise `NavPathfinder::Find` (`maxNodes = 1000000`) `Found` döndürür ve `cost` ≈ `NavOctile(|dx|, |dz|, unit)` (`1e-3 * oct + 1e-3` içinde; geçerli bir Bresenham yürüyüşü octile uzunluğundadır, A* ondan kısa olamaz). Toplamda `found > 0` ve `clearTrue > 0` (`CHECK`). Çıktı: `std::printf("NAVSMOOTH random: paths=%d cells=%d waypoints=%d clear_pairs=%d\n", ...)` (`waypoints` = `maxLookahead = 64` ile toplam). |
| `NavSmooth_Params_Edge` | Boş yol → `waypoints` boş, `length == 0`; tek hücre → 1 ara nokta, `length == 0`; iki komşu hücre `(5,5),(6,5)` → ikisi, `length` = 4,0 (`1e-3`). Önce **dolu** bir `NavSmoothResult` verilip yeniden kullanılır: `out` tamamen üzerine yazılır (eski ara noktalar kalmaz). 40×40 halka düz zemin, elle kurulan yatay yol `(5,5),(6,5),…,(14,5)` (10 hücre): `maxLookahead` **1, 0 ve −5** → ara noktalar 10 hücrenin tamamı; **2** → `x = 5, 7, 9, 11, 13, 14` (6 ara nokta, `length` = 36,0); **64** → `(5,5)`, `(14,5)` (2 ara nokta, `length` = 36,0). Aynı yolun iki çağrısı birebir aynı sonucu verir (belirlenim). |
| `NavSmooth_RealMap` | Gerçek harita (aşağıya bak). |
| `NavSmooth_Perf` | Gerçek harita (aşağıya bak): `near64` kümesi, düzleştirme süresi. |

**Gerçek harita ortak kuralı** (F5-02 ile aynı): dosya yolu `build/nav/zone71.navgrid` (çalışma dizini depo kökü); açılamazsa testi **başarısız yapma**: `std::printf("NAVSMOOTH real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n")` yaz ve dön. `LoadFile` + `Build()` (varsayılan `NavParams`) sonrası `MainComponentCells() == 88508` `REQUIRE` edilir.

`NavSmooth_RealMap`:

- A = `(CellOf(1274), CellOf(890))` = `(318, 222)`, B = `(CellOf(746), CellOf(1106))` = `(186, 276)`; her ikisi `Walk` (`REQUIRE`). `Find(A → B)` varsayılan parametrelerle `Found` (`REQUIRE`), `NavSmoothPath` varsayılan.
- `CHECK`: `cells.size() ≤ 200` (prototip 150); **`waypoints.size() ≤ 25`** (prototip 13) ve `waypoints.size() ≥ 2`; `waypoints.front() == A`, `waypoints.back() == B`; `IsSubsequence`; `SegmentsClear`; **`length` 570,4 ile 661,1 arasında** (düz çizgi 570,47 ≤ `length` ≤ A* maliyeti 660,617 + 0,5); `length ≤ cost + 1e-3 * cost + 1e-3`.
- B → A: aynı kontroller (ara nokta ≤ 25), `length` iki yön arasında **1,0 m** içinde (yollar farklı olabilir, uzunluk aynı sınıfta).
- Yazdır: `std::printf("NAVSMOOTH arena A->B: cells=%d waypoints=%d length=%.3f cost=%.3f\n", ...)`.
- Not (planın doğruladığı sayı tutmazsa): ara nokta sayısı 25'i, ya da uzunluk aralığı dışına çıkarsa **kuralı sayıya uydurma**; dur ve Uygulayıcı Raporu'nda sor.

`NavSmooth_Perf`:

- Hazırlık: `Walk` hücrelerini x-ana taramada (`x` dış, `z` iç döngü) tek `std::vector<BotCore::NavCell>`'e topla (88 508 eleman). Sorgu sayısı `Q` **Release'te 1000, Debug'da 100** (`#ifdef _DEBUG`). Çiftler `Rng(20261002)`'den `NextBelow(count)` ile çekilir; başlangıç == hedef ise ya da Chebyshev mesafesi (`max(|dx|, |dz|)`) 64'ü aşıyorsa çift reddedilir ve yenisi çekilir (`near64`, F5-02'deki kümenin aynısı). Bir `NavPathfinder` ve bir `NavPathResult`/`NavSmoothResult` yeniden kullanılır.
- Her çiftte **`Find`** (süresi ölçülmez), `Found` ise `NavSmoothPath`: **yalnızca `NavSmoothPath` çağrısı** `std::chrono::steady_clock` ile ayrı süre ölçülür. **Isınma:** ölçümden önce aynı kümeden 20 ek çift koşulur ve istatistiğe sayılmaz. Bulunmayan çiftler (`NoPath`/`NodeLimit`) ölçüme girmez.
- İstatistik: bulunan yol sayısı `n`, `cells` toplamı, `waypoints` toplamı, `length / cost` ortalaması (`cost > 0` olan yollar), süre p50/p95/p99 (ms).
- Yazdır (**tam bu biçim**):

```
NAVSMOOTH perf set=near64 paths=<n> cells_mean=<x.x> waypoints_mean=<x.x> length_ratio_mean=<x.xxx> ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
```

- `CHECK`'ler: `n * 100 ≥ Q * 95`; `waypoints toplamı * 100 ≤ cells toplamı * 30` (prototip ≈ %11; yani düzleştirme yolu en az ~3,3 kat kısaltır); `length_ratio_mean ≤ 0.99` (prototip 0,9575) ve `≥ 0.85`; ilk 50 bulunan yolun hepsinde `IsSubsequence` ve `SegmentsClear`. **`#ifndef _DEBUG`** (Release): **`ms_p95 ≤ 0.5`** `[Ö]` (prototip tahmini ≈ 0,05 ms; A* p95 0,54 ms, düzleştirme ona eklenen küçük kısım olmalı). Debug'da süre kapısı yoktur (satırı yine yazdır).

### 5.3 Proje dosyaları

1. `BotCore/BotCore.vcxproj`: `ClInclude` grubuna `<ClInclude Include="NavSmooth.h" />` (`NavPath.h`'den sonra, `Perception.h`'den önce). BOM/CRLF korunur.
2. `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubuna `<ClCompile Include="NavSmoothTests.cpp" />` (`NavPathTests.cpp`'den sonra, `PerceptionTests.cpp`'den önce). BOM/CRLF korunur.
3. Bu iki projenin `.filters` dosyası yoktur (F5-01'de doğrulandı); ek dosya yok.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; `BotCore` ve `BotCoreTests` için **yeni uyarı yok** (Level 4; derleme çıktısında `NavSmooth` geçen `warning` satırı bulunmaz).
- [ ] K2: `./tools/run-tests.sh Release --no-build --list` çıktısı şu sekiz test adını içerir: `NavSmooth_LineClear_Basics`, `NavSmooth_OpenField`, `NavSmooth_WallGap_NoCornerCut`, `NavSmooth_WallDetour`, `NavSmooth_Invariants_Random`, `NavSmooth_Params_Edge`, `NavSmooth_RealMap`, `NavSmooth_Perf`.
- [ ] K3: `build/nav/zone71.navgrid` varken (yoksa `python3 tools/nav-export.py` ile üret) `./tools/run-tests.sh Release --no-build` çıkış kodu 0; çıktıda `88 tests, 0 failed` (önceki 80 + sekiz yeni), tüm eski testler ve sekiz yeni test `[ OK ]`; `SKIPPED` geçmiyor; `NAVSMOOTH arena A->B: …` satırı (`waypoints` ≤ 25, `length` 570,4..661,1) ve `NAVSMOOTH perf set=near64 …` satırı var.
- [ ] K4: Gerçek harita yokken (`build/nav/zone71.navgrid` geçici olarak başka ada taşınarak) `./tools/run-tests.sh Release --no-build NavSmooth_` çıkış kodu 0, iki gerçek harita testinin her biri `SKIPPED` yazar ve diğer altı test geçer; ardından dosya yerine geri konur.
- [ ] K5: `./tools/run-tests.sh Debug` (derleme dahil) çıkış kodu 0 (Debug'da `assert`/sınır hataları yok).
- [ ] K6: Mülkiyet ve invariantlar: `NavSmooth_Invariants_Random` ve `NavSmooth_WallGap_NoCornerCut` `[ OK ]`; çıktıdaki `NAVSMOOTH random: paths=<p> … clear_pairs=<c>` satırında `p > 0` ve `c > 0`.
- [ ] K7: **Süre (Release):** K3 çıktısındaki `NAVSMOOTH perf set=near64` satırında `paths ≥ 950`, `waypoints_mean < cells_mean`, `length_ratio_mean` 0,85..0,99 ve **`ms_p95 ≤ 0.500`**. Uygulayıcı Raporu satırı **aynen** yapıştırır ve `nproc` bilgisini ekler. Kapı tutmuyorsa: testi üç kez koş (en kötüsünü raporla); tutarlı biçimde aşıyorsa kural gevşetilmez: `NavSmooth.h` içinde (§5.1 performans notu) iyileştir; hâlâ aşıyorsa durup ölçümleri "Açık sorular"da raporla.
- [ ] K8: Saflık/kapsam: `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavSmooth.h` çıktısı boş; `git diff --stat gece/2026-10-02-nav...bot/F5-03` yalnızca §4'teki dört dosyayı ve kendi plan dosyasını gösterir (`GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h` yok).
- [ ] K9: F5-01/F5-02 davranışı bozulmadı: `./tools/run-tests.sh Release --no-build Nav_` ve `./tools/run-tests.sh Release --no-build NavPath_` çıkış kodu 0; on `Nav_*` + dokuz `NavPath_*` test `[ OK ]`; `NAVPATH T-NAV-03 set=near64 …` satırında `found ≥ 950` ve `ms_p95 ≤ 2.000` (AC-NAV-02 hâlâ karşılanıyor).

## 7. Doğrulama komutları

```bash
git switch -c bot/F5-03 gece/2026-10-02-nav
python3 tools/nav-export.py            # build/nav/zone71.navgrid yoksa
./tools/build.sh Release
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Release --no-build NavSmooth_
./tools/run-tests.sh Debug
grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavSmooth.h
git diff --stat gece/2026-10-02-nav...bot/F5-03
```

## 8. Kısıtlar ve uyarılar

- **Paralel hat:** sunucuya hiç dokunma (`tools/run-servers.sh` çağırma; ana hat başka çalışma ağacından sunucu çalıştırıyor olabilir). DB'ye bağlanma. Dal tabanı `gece/2026-10-02-nav`.
- Kodlama/satır sonu: `AGENTS.md` §3. Yeni C++ dosyaları ASCII + CRLF + tab + Allman. Yorumlar İngilizce; plan metnindeki Türkçe açıklama koda girmez.
- Başlık-yalnızca: tüm tanımlar `inline`; `static` global durum yok; `#pragma` yalnızca `once` (MSVC Level 4 uyarılarını `static_cast` ile çöz, `#pragma warning` ekleme). `std::abs` yerine elle işaret kullanılabilir (`<cstdlib>` ekleme zorunlu değil; `NavPath.h`'deki gibi).
- Özyineleme yok. `NavLineClear` döngüsü sonlanır çünkü ilk adımda iki uç `Walk` (ızgara içi) denetlenir ve her adım `|dx|,|dz|`'yi tam olarak bir azaltır.
- **`NavLineClear` Bresenham'dır, `supercover` değildir (bilinen sınırlama `[A]`):** iki hücre merkezini birleştiren sürekli doğru bir hücre sınırından tam ortadan geçtiğinde Bresenham yalnızca bir tarafı ziyaret eder; öbür taraftaki hücre yalnızca çapraz adımın köşe kuralıyla (iki ortogonal komşu `Walk`) dolaylı denetlenir, düz adımda denetlenmez. Ayrıca ızgara 4 m'dir ve botun gövde genişliği modellenmez. Sunucu bunu denetlemez; gerçek istemcide duvar takılması çıkarsa F5-09 (takılma kurtarma) yakalar. Bu planda `supercover`'a **geçme**: karar ADR-0006 Eki F5-03'tedir; MET-NAV-01 ölçümü (T-NAV-04) takılma oranı yüksek çıkarsa yeniden değerlendirilir.
- `maxLookahead = 64` `[A]`'dır (prototipte `near64` yolları ortalama 55 hücre; pencere en uzun yolları kapsar, sınır yalnızca en kötü durum maliyetini bağlar). Ayarı dosyadan okuyan kod **yazma**.
- Hiçbir yerde `NavLineClear`'ı "LoS" diye adlandırma/yorumlama: LoS F5-10'dur ve farklı kuraldır (docs/12 §5).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-03` (taban: `gece/2026-10-02-nav`, `4ca3ac8`) — `c18a6ba [F5-03] Yol düzleştirme (BotCore/NavSmooth.h) ve NavSmooth testleri` + rapor commit'i
- Değişen dosyalar ve neden:
  - `BotCore/NavSmooth.h` (yeni): `NavLineClear` (kanonik yönlü tamsayı Bresenham, her adım `EdgeOpen`), `NavSmoothParams` (`maxLookahead = 64`), `NavSmoothResult`, `NavSmoothPath` (açgözlü en uzak görünür ara nokta).
  - `Tests/BotCoreTests/NavSmoothTests.cpp` (yeni): §5.2'deki sekiz test (birim + gerçek harita + süre), yerel yardımcılar `CellIndex`/`Cell`/`RingEvents`/`MakeNav`/`IsSubsequence`/`SegmentsClear`/`Euclid`/`PercentileDouble`; `NavPathTests.cpp` kalıpları kopyalandı.
  - `BotCore/BotCore.vcxproj`: `NavSmooth.h` `ClInclude` satırı (`NavPath.h`'den sonra).
  - `Tests/BotCoreTests/BotCoreTests.vcxproj`: `NavSmoothTests.cpp` `ClCompile` satırı (`NavPathTests.cpp`'den sonra).
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  BotCore.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\BotCore.lib
  Lua.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\Lua.lib
  shared.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\shared.lib
  proj-LogInServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\LogInServer.exe
  proj-GameServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\AIServer.exe
  NavGridTests.cpp
  NavPathTests.cpp
  NavSmoothTests.cpp
  Kod Üretiliyor...
  BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  (Release rc=0; `NavSmooth` geçen `warning` satırı yok; toplam uyarı 0. Debug derlemesi de rc=0.)
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ Release rc=0, yeni uyarı yok (tüm çıktıda 0 `warning`).
  - K2 ✔ `--list` sekiz `NavSmooth_*` adını içeriyor (toplam 88 ad).
  - K3 ✔ `88 tests, 0 failed`; `SKIPPED` yok; `NAVSMOOTH arena A->B: cells=150 waypoints=14 length=630.319 cost=660.617` ve `NAVSMOOTH perf set=near64 paths=997 …` var.
  - K4 ✔ harita geçici taşınınca `NavSmooth_` rc=0; iki gerçek harita testi `SKIPPED`, diğer altı geçti; dosya geri kondu.
  - K5 ✔ `./tools/run-tests.sh Debug` rc=0, `88 tests, 0 failed`.
  - K6 ✔ `NAVSMOOTH random: paths=240 cells=3681 waypoints=1286 clear_pairs=108` (`p>0`, `c>0`).
  - K7 ✔ `NAVSMOOTH perf set=near64 paths=997 cells_mean=51.5 waypoints_mean=5.9 length_ratio_mean=0.959 ms_p50=0.006 ms_p95=0.032 ms_p99=0.060` (`paths=997 ≥ 950`, `waypoints_mean < cells_mean`, oran 0.959, `ms_p95=0.032 ≤ 0.500`); `nproc=16`, AMD Ryzen 7 7800X3D.
  - K8 ✔ saflık grep'i boş (rc=1); `git diff --stat gece/2026-10-02-nav...bot/F5-03` yalnızca dört dosya (791 ekleme).
  - K9 ✔ `Nav_` 10/10, `NavPath_` 9/9; `NAVPATH T-NAV-03 set=near64 found=997 ms_p95=0.548 ≤ 2.000`.
- Plandan sapmalar ve gerekçeleri:
  - Gerçek harita arena A→B düzleştirmesi prototipten biraz farklı çıktı: **14 ara nokta / 630,319 m** (prototip 13 / 635,787; C++ A*'ın eşit-maliyetli beraberlik sırası farklı bir 150 hücreli yol seçiyor). Kural sayıya uydurulmadı; plan aralıkları (≤ 25 ara nokta, 570,4..661,1 m) karşılanıyor, sapma değil.
  - `NavSmooth_Invariants_Random`'da çift tohumu plandaki `Rng(6000 + k)` ve mülkiyet tohumu `Rng(7000 + k)`; perf kümesi `Rng(20261002)` — planla aynı.
- Açık sorular: yok. (Paralel hat: sunuculara/DB'ye dokunulmadı; `GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h` değişmedi.)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02-nav...bot/F5-03` @ `0e852d7` (kod commit'i `c18a6ba`; paralel hat `nav`, otonom mod: birleştirme ve push yapılmadı, sunuculara/DB'ye dokunulmadı)
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `touch BotCore/NavSmooth.h Tests/BotCoreTests/NavSmoothTests.cpp` sonrası `./tools/build.sh Release` rc=0; çıktıda `NavSmoothTests.cpp` derlendi, toplam `warning` satırı 0 |
| K2 | ✔ | `run-tests.sh Release --no-build --list`: 88 ad, sekiz `NavSmooth_*` adının tamamı var |
| K3 | ✔ | `run-tests.sh Release --no-build` rc=0, `88 tests, 0 failed`, `SKIPPED` 0; sekiz `NavSmooth_*` `[ OK ]`; `NAVSMOOTH arena A->B: cells=150 waypoints=14 length=630.319 cost=660.617` (≤ 25 ara nokta, 570,4..661,1); `NAVSMOOTH perf set=near64 paths=997 cells_mean=51.5 waypoints_mean=5.9 length_ratio_mean=0.959 ms_p50=0.006 ms_p95=0.032 ms_p99=0.059` |
| K4 | ✔ | `build/nav/zone71.navgrid` geçici taşındı: `NavSmooth_` rc=0, `8 tests, 0 failed`, iki gerçek harita testi `SKIPPED`, diğer altı `[ OK ]`; dosya geri kondu (`md5sum -c` OK) |
| K5 | ✔ | `./tools/run-tests.sh Debug` rc=0, `88 tests, 0 failed`, `warning` 0; Debug perf satırı `paths=100` (Debug'da süre kapısı yok) |
| K6 | ✔ | `NAVSMOOTH random: paths=240 cells=3681 waypoints=1286 clear_pairs=108` (`p>0`, `c>0`); `NavSmooth_Invariants_Random` ve `NavSmooth_WallGap_NoCornerCut` `[ OK ]` (Release ve Debug) |
| K7 | ✔ | Üç ayrı `NavSmooth_Perf` koşusu: `ms_p95` 0.033 / 0.034 / 0.033 (≤ 0.500), `paths=997`, `waypoints_mean=5.9 < cells_mean=51.5`, `length_ratio_mean=0.959`; `nproc=16` (Ryzen 7 7800X3D, uygulayıcı raporu) |
| K8 | ✔ | saflık grep'i boş (rc=1); `git diff --stat gece/2026-10-02-nav...bot/F5-03`: `BotCore/BotCore.vcxproj` (+1), `BotCore/NavSmooth.h` (+128), `Tests/BotCoreTests/BotCoreTests.vcxproj` (+1), `Tests/BotCoreTests/NavSmoothTests.cpp` (+661), kendi plan dosyası; `GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h` yok; plan farkı yalnızca `Durum` ve Uygulayıcı Raporu |
| K9 | ✔ | `Nav_` 10 test, `NavPath_` 9 test `[ OK ]`, rc=0; `NAVPATH T-NAV-03 set=near64 … found=997 … ms_p95=0.564` (tam koşuda 0.531; ≤ 2.000) |

- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. Not: `BotCore/NavSmooth.h:33-36`: `dx` kanonik sıralamadan sonra negatif olamaz, `ax = dx < 0 ? -dx : dx` gereksiz; planın metnine uyduğu için bırakıldı, davranışı etkilemez.
  2. Not: `NavLineClear` Bresenham'dır, `supercover` değildir (plan §8 ve ADR-0006 Eki F5-03'te bilinen sınırlama `[A]`); sınır MET-NAV-01 / F5-09 ile ölçülecek.
  3. Not: Arena A→B sonucu prototipten farklı (14 ara nokta / 630,319 m, prototip 13 / 635,787): C++ A* beraberlik sırası farklı, eşit maliyetli 150 hücrelik yol seçiyor; plan aralıkları içinde, kural sayıya uydurulmamış.
  4. Doğruluk incelemesi: `NavLineClear` kanonik yön, ızgara dışı/engelli uç, `a == b` ve her adımda tek `EdgeOpen` kenarı plan §5.1'e birebir; `NavSmoothPath` boş/tek hücre, `maxLookahead < 1` ve `j == i + 1` ayrımı plana uygun, sızıntı/aliasing yok; testler planın sayılarına (16,0 m, 93,255 m, 3..5 ara nokta, `36,0` m) birebir; `CHECK` makrosu `do { } while` olduğundan `if` gövdesinde güvenli.
  5. Dosya biçimi: yeni dosyalar ASCII + CRLF (çalışma ağacı) + tab/Allman; vcxproj'larda yalnızca birer satır eklendi, BOM korundu; `build/` commit'te yok.
- Düzeltme talimatı: yok (DOĞRULANDI).
