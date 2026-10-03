# F5-10: Görüş hattı (LoS, advisory) — `BotCore/NavLos.h` (ızgara + arazi görüş testi, mod politikası, görüşlü konum seçimi)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02-nav) |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat, `docs/17` §1 "Paralel yürütülebilir işler") |
| Branch | `bot/F5-10` (taban: `gece/2026-10-02-nav`) |
| Bağımlı olduğu planlar | F5-01 (`BotCore/NavGrid.h`), F5-02 (`BotCore/NavPath.h`: `NavCell`), F5-04 (`BotCore/NavTrack.h`: `NavRingCells`): hepsi `KAPANDI`, `gece/2026-10-02-nav` içinde; bu planın testleri mevcut 217 testin üstüne eklenir |
| İlgili gereksinim / kabul | `docs/12` §5 (görüş hattı; bu plana göre güncellendi), T-NAV-LOS-01 (oyun içi ölçüm, kapsam dışı), `docs/12` §12 açık soru "Görüş hattı istemci davranışı"; ADR-0006 Eki F5-10 (bu planın kararları), ADR-0016 (`BotCore` saflığı) |
| Tahmini büyüklük | S (4 dosya: 2 yeni, 2 proje satırı; ~ 200 satır başlık, ~ 700 satır test) |
| Hazırlayan / tarih | Claude / 2026-10-03 |

---

## 1. Amaç

Sunucu görüş hattını hiç denetlemez (MEC-R, MEC-MAG); gerçek istemcinin engel arkasına skill kullanmaya izin verip vermediği henüz ölçülmedi (T-NAV-LOS-01). `docs/12` §5 bu yüzden LoS'u **yürünebilirlikten ayırır** ve varsayılan modu `advisory` yapar: LoS aksiyonu **engellemez**, yalnızca konum seçimini yönlendirir (ör. priest heal hedefine "görüşü olan" noktayı seçer). Bu plan, bunun saf mantığını yazar; üç araç:

1. **`NavLosClear`:** iki dünya noktası arasında ucuz, kaba görüş testi = `los_grid` (ışının geçtiği hücrelerde engel var mı) + arazi tümseği testi (göz yüksekliğindeki ışın zemin yüksekliğinin altında kalıyor mu). `los_mesh` (çarpışma üçgenleri) bu planda **yok**.
2. **`NavLosAllows` + `NavLosMode`:** `P-NAV-LOS-MODE` politikası: `Advisory` aksiyonu hiçbir zaman engellemez; `Enforce` yalnızca görüş açıkken izin verir. Hangi modun seçileceği T-NAV-LOS-01 ölçümüne bağlıdır (karar katmanı/ayar).
3. **`NavPickLosCell`:** hedefin etrafındaki menzil halkasından, bota en yakın **görüşü olan** `Walk` hücreyi seçer (priest heal, mage cast için konum yönlendirmesi).

Bu planın sonunda **`los_mesh`, T-NAV-LOS-01 ölçümü, `NavFollower`/`ActionExecutor`/`BotFairnessGuard` bağlaması, karar katmanı, puanlama (F5-07/F5-08'e LoS terimi eklemek), telemetri ve sunucu entegrasyonu yoktur.** `GameServer/`, `AIServer/`, `shared/` değişmez; sunucu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0006-navigasyon-izgara-astar.md` — **Eki F5-10** (bu plan yazılırken eklendi): hücre kuralı (iç kesişim, uç hücreler muaf, sınır/köşe kuralları), arazi kuralı, mod politikası, bilinçli sınırlamalar. Sapma gerekiyorsa dur ve sor.
- `docs/12_NAVIGATION_AND_POSITIONING.md` §5 (bu plana göre güncellendi), §12 (açık sorular).
- `BotCore/NavGrid.h` — kullanacağın API: `Unit()` (`:41`), `CellOf(float)` (`:43`, `floor(world / unit)`), `Event(x, z)` (`:46`; olay ızgarası: `1` = açık, `0` = engelli, **ızgara dışı `-1`**), `HeightAt(wx, wz)` (`:51`; köşe noktalarından bilinear, ızgara kapsamına kırpar), `CellCenter(i)`, `Walk(x, z)`. **Değiştirme.**
- `BotCore/NavPath.h:21` (`NavCell`), `BotCore/NavTrack.h:76` (`NavRingCells(grid, cx, cz, ringMinM, ringMaxM, from, out)`: `Walk` hücreleri, hücre merkezi `[lo, hi]` m içinde, `from`'a octile uzaklık artan, eşitlikte x sonra z; `out` tamamen yeniden yazılır). **Değiştirme.**
- `Tests/BotCoreTests/NavStuckTests.cpp` — kopyalanacak yardımcı kalıbı (yeni dosyada kendi kopyalarını yaz; başka `.cpp`'den içe aktarma yok): `CellIndex`, `Cell`, `HeightZeros`, `RingEvents`, `MakeNav`, `PercentileDouble`; gerçek harita yükleme + `SKIPPED` kalıbı; süre kapısının Debug'da atlanması (`#ifndef _DEBUG`). `Tests/BotCoreTests/MiniTest.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`), `tools/run-tests.sh`.

Planı yazarken doğrulanan gerçekler (C++ prototipi, `g++ -std=c++17`, gerçek `BotCore` başlıklarıyla, 2026-10-03, depoya girmeyen geçici; **aynı kurallar, aynı formüller**; sayılar test beklentileridir; `-O0`, `-O2` ve `-O2 -mfma -ffp-contract=fast` ile gerçek harita sayıları aynı çıktı). Ortak adlar: `unit = 4`; hücre `(x, z)` dünya karesi `[4x, 4x+4) × [4z, 4z+4)`, merkezi `((x + 0.5)·4, (z + 0.5)·4)`; dünya 82 → hücre 20, 50 → 12, 114 → 28, 98 → 24, 62 → 15. Izgaralar (hepsi 40×40, yükseklik 0 olmadıkça): `flat` = `RingEvents(40)` (kenar engelli, iç açık); `one` = `flat` + hücre `(20,20)` engelli; `two` = `flat` + `(20,21)` ve `(21,20)` engelli; `wall` = `flat` + `x = 20`, `z = 10..30` engelli; `split` = `flat` + `x = 20`, `z = 1..38` engelli (harita ikiye bölünür, ana bileşen batı yarıdır: `MainComponentCells() == 722`, `Walk(10,20)` doğru, `Walk(30,20)` yanlış); `hill(H)` = `flat` olayları + **köşe yükseklikleri** `x = 20` ve `x = 21` sütunlarında (tüm `z`) `H`, diğerleri 0 (dünyada x ∈ [80, 84] platosu `H` m, x ∈ [76, 80] ve [84, 88] rampa).

**Tablo 1 — hücre kuralı** (`NavLosGridClear(grid, ax, az, bx, bz)`; her satır **iki yönde** de aynı sonucu verir; a → b dünya koordinatları):

| # | Izgara | a → b | Beklenen |
|---|---|---|---|
| 1 | `flat` | (50,82) → (114,82) | **true** |
| 2 | `flat` | (82,50) → (82,114) | true |
| 3 | `flat` | (82,82) → (98,98) | true |
| 4 | `flat` | (50,50) → (114,114) | true |
| 5 | `flat` | (82,82) → (82,82) | true (aynı nokta) |
| 6 | `flat` | (82,82) → (83,82.5) | true (aynı hücre) |
| 7 | `flat` | (82,82) → (−10,82) | **false** (ızgara dışı hücreler `Event = −1` ≠ 1 ⇒ engelli; ayrıca kenar halkası) |
| 8 | `flat` | (82,82) → (82,700) | false |
| 9 | `one` | (50,82) → (114,82) | **false** (ışın `(20,20)` içinden geçer) |
| 10 | `one` | (50,86) → (114,86) | true (bir hücre kuzeyden) |
| 11 | `one` | (50,78) → (114,78) | true (bir hücre güneyden) |
| 12 | `one` | (82,50) → (82,114) | false |
| 13 | `one` | (50,50) → (114,114) | false |
| 14 | `one` | (66,66) → (98,98) | false |
| 15 | `one` | (78,78) → (90,90) | false |
| 16 | `one` | (76,76) → (84,84) | false (bitiş hücresi `(21,21)`; `(20,20)` ara hücre) |
| 17 | `one` | (50,82) → (82,82) | **true** (bitiş hücresi `(20,20)` engelli ama **uç hücreler muaf**) |
| 18 | `one` | (82,82) → (114,82) | true (başlangıç hücresi muaf) |
| 19 | `one` | (82,82) → (98,98) | true (başlangıç muaf; ışın `(84,84)` köşesinden `(21,21)`'e geçer) |
| 20 | `one` | (81,81) → (83,83) | true (aynı hücre, engelli olsa da) |
| 21 | `one` | (70,82) → (78,82) | true |
| 22 | `one` | (76,92) → (92,76) | **true** (ışın `x + z = 168`; `(20,20)`'ye yalnızca `(84,84)` köşesinde **değer**; köşeye değmek engel değil) |
| 23 | `one` | (76,91.99) → (92,75.99) | **false** (aynı doğru 0,01 m içeri: `(20,20)` iç kısmını keser) |
| 24 | `one` | (76,92.01) → (92,76.01) | true |
| 25 | `one` | (50,80) → (114,80) | **false** (hücre sınırına tam yatan ışın **büyük taraftaki** hücreye ait: `floor(80/4) = 20`, satır 20 engelli) |
| 26 | `one` | (50,79.99) → (114,79.99) | true (satır 19) |
| 27 | `one` | (50,84) → (114,84) | true (`floor(84/4) = 21`) |
| 28 | `one` | (50,83.99) → (114,83.99) | false (satır 20) |
| 29 | `one` | (80,50) → (80,114) | false (sütun 20) |
| 30 | `one` | (79.99,50) → (79.99,114) | true |
| 31 | `one` | (84,50) → (84,114) | true |
| 32 | `one` | (83.99,50) → (83.99,114) | false |
| 33 | `two` | (76,92) → (92,76) | **false** (ışın hem `(20,21)` hem `(21,20)` iç kısmından geçer) |
| 34 | `two` | (70,98) → (98,70) | false |
| 35 | `two` | (76,90) → (92,74) | false |

**Tablo 2 — arazi kuralı** (`hill(H)` ızgaralarında, varsayılan `NavLosParams`; her satır iki yönde aynı; hücre kuralı zaten açıktır, sonuç `NavLosTerrainClear` ve `NavLosClear` için aynıdır; göz yüksekliği 1,6 m, pay 0,25 m ⇒ ışın zeminin 1,85 m üstünden eşik):

| # | a → b | H = 4,0 | H = 1,9 | H = 1,8 | H = 1,5 |
|---|---|---|---|---|---|
| 1 | (50,82) → (114,82) | **false** | **false** | true | true |
| 2 | (82,82) → (114,82) (plato üstünden alçalan) | true | true | true | true |
| 3 | (50,82) → (82,82) (platoya tırmanan) | true | true | true | true |
| 4 | (76,82) → (88,82) | false | false | true | true |
| 5 | (70,82) → (94,82) | false | false | true | true |
| 6 | (84,82) → (100,82) | true | true | true | true |

`hill(1.9)` üzerinde (50,82) → (114,82), yalnızca `NavLosTerrainClear`, parametre değişimleri: varsayılan **false**; `eyeM = 3,0` **true**; `terrainSlackM = 0,5` **true**; `terrain = false` **true**; `sampleM = 0` **true** (kural kapalı); `eyeM = 0` **false**. Aynı çağrıda `NavLosClear` (varsayılan) **false**, `NavLosGridClear` **true** (arazi engeli hücre kuralını etkilemez).

**Tablo 3 — mod politikası:** `NavLosParams()` varsayılanları: `mode == Advisory`, `terrain == true`, `eyeM == 1.6f`, `terrainSlackM == 0.25f`, `sampleM == 2.0f`. `NavLosAllows(Advisory, false) == true`, `(Advisory, true) == true`, `(Enforce, true) == true`, `(Enforce, false) == false`.

**Tablo 4 — görüşlü konum seçimi** (`NavPickLosCell(grid, tx, tz, fromX, fromZ, ringMinM, ringMaxM, params, scratch, out)`; hedef `(tx, tz)`, bot `(fromX, fromZ)`; "adaylar" = çağrıdan sonra `scratch.size()`; sonuç hücre):

| # | Izgara | hedef, bot, halka [min, max] m, params | Beklenen |
|---|---|---|---|
| a | `wall` | (98,82), (62,82), [0, 24] | **true**, `(21,20)`, adaylar **104** (batı yarı duvar yüzünden görüşsüz, ilk görüşlü: duvarın doğusundaki aynı satır) |
| b | `wall` | (98,82), (62,82), [0, 40] | true, `(21,20)`, adaylar **298** |
| c | `wall` | (98,82), (98,82), [0, 24] | true, `(24,20)` (bot zaten görüşte: kendi hücresi) |
| d | `wall` | (62,82), (62,82), [0, 24] | true, `(15,20)` |
| e | `wall` | (62,82), (98,82), [0, 24] | **true**, `(19,20)` (bot duvarın doğusunda, hedef batısında: `(21,20)` görüşsüz, ilk görüşlü batı hücresi) |
| f | `wall` | (98,82), (62,82), [16, 24] | true, `(21,17)`, adaylar **59** |
| g | `wall` | (98,82), (62,82), [0, 10] | true, `(22,20)`, adaylar **21** |
| h | `split` | (98,82), (62,82), [0, 40] | **false**, adaylar **72**, `out` **dokunulmaz** (önce `(-7,-7)`) |
| i | `split` | (98,82), (62,82), [0, 200] | false, adaylar **722** (ana bileşen = tüm `Walk`; hiçbirinin görüşü yok) |
| j | `flat` | (2,2), (62,82), [0, 4] | false, adaylar **0** (dış bant `Walk` değil) |
| k | `hill(4)` | (98,82), (62,82), [0, 24], varsayılan | true, `(20,20)` (plato üstü; batı hücreleri tümsekle engelli), adaylar **113** |
| l | `hill(4)` | aynı, `terrain = false` | true, `(18,20)` (arazi kuralı kapalı ⇒ ilk aday bota en yakın) |
| m | `hill(4)` | aynı, halka [8, 24], varsayılan | true, `(20,20)`, adaylar **104** |

**Referans çapraz doğrulama (`NavLos_Reference`):** 40×40, `RingEvents` + iç hücrelerin yaklaşık %8'i rastgele engelli; `std::mt19937 rng(12345)`; `auto U = [&]() { return (rng() >> 8) * (1.0f / 16777216.0f); };`. **Üretim sırası bağlayıcıdır:** önce ızgara (`x = 0..39` dış döngü, `z = 0..39` iç; kenar hücrelerinde `U()` **çağrılmaz**, iç hücrelerde `U() < 0.08f` ise engelli), sonra 5000 sorgu: `float ax = 24 + U() * 112; float az = 24 + U() * 112; float bx = ax - 24 + U() * 48; float bz = az - 24 + U() * 48;` (bu sırayla). Her sorguda `NavLosGridClear` ile bağımsız referans (hücre başına dilim testi: her engelli hücre için, **uç hücreler hariç**, ışın `t ∈ [0,1]` aralığında hücrenin **açık iç kısmını** `t0 < t1 − 1e-9` ile kesiyor mu; `double`; ışın eksene paralelse ilgili eksende başlangıç koordinatının `(lo, hi)` açık aralığında olması gerekir) karşılaştırılır. Beklenen: uyuşmazlık **0**, iki yönde sonuç farkı (simetri) **0**; açık sorgu sayısı prototipte **3272**/5000 (`[3000, 3500]` aralığı zorunlu, tam değer yazdırılır; üretim sırası doğru ise 3272 çıkar).

**Zone 71 taraması** (`build/nav/zone71.navgrid`; `Walk` **88508**): her `Walk` hücre için (x-ana tarama) `((x * 7 + z * 13) % 5) == 0` ise bu hücre bir **kaynak**dır; beş hücre ofseti `(dx, dz)` ∈ {(5,0), (0,8), (7,7), (−12,5), (15,−10)}; hedef hücre `Walk` değilse (ızgara dışı dahil) atlanır (`skipped`). Konumlar hücre merkezleridir; varsayılan `NavLosParams`. Beklenen (kesin sayılar; ofset başına):

| Ofset | `tested` | `grid` (`NavLosGridClear` true) | `grid_terrain` (`NavLosClear` true) |
|---|---|---|---|
| (5,0) | 14380 | 13230 | 12874 |
| (0,8) | 13389 | 10962 | 10257 |
| (7,7) | 12718 | 9702 | 8783 |
| (−12,5) | 12433 | 7706 | 6720 |
| (15,−10) | 11639 | 5645 | 4529 |
| **toplam** | **64559** | **47245** | **43163** |

`skipped` **23966**; iki yönde fark (`NavLosGridClear` ve `NavLosClear` için ileri ≠ geri) **0**; ihlal (`NavLosClear` true iken `NavLosGridClear` false, ya da `NavLosClear != (NavLosGridClear && NavLosTerrainClear)`) **0**. Süre: `NavLosClear` çağrısı başına p95 ≈ 0,0004 ms (Release; kapı `[A]` ≤ 0,05 ms). **Konum seçimi taraması** (aynı kaynaklar, hedef = kaynak + (7,7) hücre, `Walk` ise): `NavLosClear(kaynak → hedef)` false olanlar için `NavPickLosCell(hedef merkezi, kaynak merkezi, [0, 30] m)`: `reposition` (görüş yok) **3935** (= 12718 − 8783), `found` **3935**, ihlal (sonuç `Walk` değil, merkezi hedefe `> 30` m (`dx*dx + dz*dz > 30.0f * 30.0f`), ya da `NavLosClear(sonuç → hedef)` false) **0**; çağrı başına p95 ≈ 0,012 ms (kapı `[A]` ≤ 0,5 ms). (Hedef hücresinin kendisi hep adaydır ve kendi içinde görüşlüdür; bu yüzden `Walk` hedefte `found` hep doğrudur: "kapı" ihlal sayısıdır, bulunma oranı değil.)

## 3. Kapsam

**Yapılacaklar**

- `BotCore/NavLos.h`: `NavLosMode`, `NavLosParams`, `NavLosGridClear`, `NavLosTerrainClear`, `NavLosClear`, `NavLosAllows`, `NavPickLosCell` (§5.1).
- `Tests/BotCoreTests/NavLosTests.cpp`: §5.2 altı test.
- `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` kayıtları (§5.3).

**Kapsam dışı (yapılmayacak)**

- `los_mesh` (çarpışma üçgenlerine ışın testi): `N3ShapeMgr` verisi `BotCore`'a aktarılmıyor, `_IntersectTriangle` kullanılmıyor; ayrı bir veri hattı gerektirir.
- T-NAV-LOS-01 (istemcinin engel arkasına skill kullanımı): oyun içi ölçüm; bu plan yalnızca iki modu da destekleyen mantığı yazar. `Enforce` seçimi ve guard kuralı (CLI-*) ölçüm sonrasıdır.
- `NavFollower`/`ActionExecutor`/`BotFairnessGuard`/karar katmanı bağlaması, aksiyon engelleme, telemetri (`LOS_BLOCKED` vb.), `P-NAV-LOS-MODE` ini anahtarı: bağlama planlarıdır. `NavLosAllows` yalnızca saf politika işlevidir.
- LoS'u F5-07 (geri çekilme) veya F5-08 (formasyon) puanlamasına eklemek; hedef yüksekliği (ayakta/oturan/binek), bina/duvar yüksekliği (engelli hücre sonsuz yüksek sayılır), su yüzeyi, bitki örtüsü, birim gövdeleri (diğer botlar engel değildir).
- `NavGrid.h`, `NavPath.h`, `NavTrack.h`, `NavSmooth.h`, `NavDanger.h`, `NavReach.h`, `NavRetreat.h`, `NavFormation.h`, `NavStuck.h`, `NavBudget.h`, `NavSegment.h` **değişmez**.
- `GameServer/`, `AIServer/`, `shared/`, `docs/` değişikliği, DB, sunucu çalıştırma. `GameServer/proj-GameServer.vcxproj` değişmez.
- İş parçacığı güvenliği: işlevler saf ve durumsuzdur (`scratch` çağıranın tamponu); global/`static` değişebilir durum yok.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavLos.h` | yeni | ASCII, CRLF, başlık-yalnızca (satır içi); yalnızca `NavTrack.h` (→ `NavSmooth.h`, `NavPath.h`, `NavGrid.h`) + standart başlıklar |
| `Tests/BotCoreTests/NavLosTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="NavLos.h" />` satırı (BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="NavLosTests.cpp" />` satırı (BOM ve CRLF korunur) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`build/nav/zone71.navgrid` üretilen çıktıdır, commit edilmez; yoksa `python3 tools/nav-export.py` ile üret.)

## 5. Uygulama adımları

### 5.1 `BotCore/NavLos.h`

Ad alanı `BotCore`; `#include "NavTrack.h"` (aynı dizin, tırnaklı; `NavGrid`, `NavCell`, `NavRingCells` gelir) ve `<cmath> <cstddef> <cstdint> <vector>`. Stil `NavStuck.h`/`NavFormation.h` gibi: tab girinti, Allman, İngilizce yorum, `inline` tanımlar, değişebilir global/`static` durum yok, `#pragma once`. Dosya başına kısa bir açıklama yorumu (F5-10, `docs/12` §5, ADR-0006 Eki F5-10; saf mantık; advisory by default).

```cpp
namespace BotCore
{
	enum class NavLosMode { Advisory, Enforce };   // P-NAV-LOS-MODE (docs/12 s5); Advisory never blocks an action

	struct NavLosParams
	{
		NavLosMode mode = NavLosMode::Advisory;   // [O] docs/12 s5
		bool  terrain = true;                     // [A] apply the terrain rule
		float eyeM = 1.6f;                        // [A] eye (and target) height above the local ground
		float terrainSlackM = 0.25f;              // [A] ground must exceed the ray by MORE than this to block
		float sampleM = 2.0f;                     // [A] terrain sampling step along the ray; <= 0 disables the terrain rule
	};

	// Cell rule (los_grid). True when every cell whose OPEN interior the segment crosses has
	// Event == 1; the start and the end cell are exempt. See the rules below.
	inline bool NavLosGridClear(const NavGrid & grid, float ax, float az, float bx, float bz);

	// Terrain rule: the eye-height ray must not dip below the ground in between.
	inline bool NavLosTerrainClear(const NavGrid & grid, float ax, float az, float bx, float bz,
		const NavLosParams & params);

	// Both rules.
	inline bool NavLosClear(const NavGrid & grid, float ax, float az, float bx, float bz,
		const NavLosParams & params = NavLosParams());

	// Advisory never blocks; Enforce allows only when the line of sight is clear.
	inline bool NavLosAllows(NavLosMode mode, bool clear);

	// Nearest-to-`from` Walk cell of the ring around the target whose centre has a clear line of
	// sight to the target point. `scratch` receives ALL ring candidates (NavRingCells order).
	inline bool NavPickLosCell(const NavGrid & grid, float tx, float tz, float fromX, float fromZ,
		float ringMinM, float ringMaxM, const NavLosParams & params, std::vector<NavCell> & scratch,
		NavCell & out);
}
```

**`NavLosGridClear` kuralları (bağlayıcı, sırayla; hesap `double` ile, girdiler `float`):**
1. `unit = grid.Unit()`; `unit <= 0` ⇒ `false`.
2. Başlangıç hücresi `(cx, cz) = (grid.CellOf(ax), grid.CellOf(az))`, bitiş hücresi `(ex, ez) = (grid.CellOf(bx), grid.CellOf(bz))`. Aynı hücre ⇒ `true` (hücre engelli olsa da).
3. `dx = bx - ax`, `dz = bz - az` (`double`); `stepX = dx > 0 ? 1 : (dx < 0 ? -1 : 0)`, `stepZ` aynı. Genişletilmiş DDA (Amanatides–Woo): `tMaxX = (stepX != 0) ? (((stepX > 0 ? cx + 1 : cx) * unit) - ax) / dx : 1e300`; `tDeltaX = (stepX != 0) ? unit / |dx| : 1e300`; z için aynı. (`1e300` = "bu eksende sınır geçişi yok".)
4. En çok `maxSteps = |ex - cx| + |ez - cz|` yineleme: `tMaxX < tMaxZ` ⇒ `cx += stepX; tMaxX += tDeltaX`; `tMaxZ < tMaxX` ⇒ `cz += stepZ; tMaxZ += tDeltaZ`; **eşitse** (ışın bir hücre köşesinden geçer) ⇒ **ikisi birlikte** (köşegen adım; köşeye değen yan hücrelere bakılmaz). Adımdan sonra `(cx, cz) == (ex, ez)` ise `true` döndür (bitiş hücresi muaf); aksi hâlde `grid.Event(cx, cz) != 1` ise `false` döndür (ızgara dışı `-1` ⇒ engelli). Döngü bitince `true`.
5. Sonuç: ışının **açık iç kısmını** kesen her ara hücre açık olmalıdır; köşeye değmek engel değildir; hücre sınırına tam yatan ışın `floor` ile büyük taraftaki hücreye ait sayılır (`z = 80` ⇒ satır 20). `Walk` **değil** `Event` kullanılır (göl cepleri/eğim yürünebilirlik meselesidir, görüş değil). `NavGrid::Build` çağrılmamış ızgarada da çalışır.

**`NavLosTerrainClear` kuralları (bağlayıcı):** `params.terrain == false` ya da `params.sampleM <= 0` ya da ışın uzunluğu `len = sqrt(dx*dx + dz*dz)` (`double`) `<= 0` ⇒ `true`. `hA = HeightAt(ax, az) + eyeM`, `hB = HeightAt(bx, bz) + eyeM` (`double`). `n = max(1, ceil(len / sampleM))`. `i = 1 .. n-1` için (uçlar örneklenmez): `t = i / n`; `ground = HeightAt(float(ax + t*dx), float(az + t*dz))`; `ground > hA + t * (hB - hA) + terrainSlackM` (kesin `>`) ise `false`. Hiçbiri engellemezse `true`. (Her iki ucun göz yüksekliği kendi zemininin üstündedir; ışın bu iki göz arasındaki doğrudur.)

**`NavLosClear`** = `NavLosGridClear(...) && NavLosTerrainClear(..., params)`. **`NavLosAllows`** = `mode == NavLosMode::Advisory || clear`.

**`NavPickLosCell` kuralları:** `from = (grid.CellOf(fromX), grid.CellOf(fromZ))` (hücre; `Walk` olması gerekmez); `NavRingCells(grid, tx, tz, ringMinM, ringMaxM, from, scratch)`; `scratch` sırasıyla her aday `c` için `NavLosClear(grid, grid.CellCenter(c.x), grid.CellCenter(c.z), tx, tz, params)`; ilk `true` ⇒ `out = c`, `true` döndür; hiçbiri ⇒ `false` (`out` dokunulmaz). `scratch`, çağrıdan sonra **tüm** halka adaylarını tutar. Not: advisory modda bile görüşsüz bir hücreyi seçmek yasak değildir; karar katmanı `false` sonucunda halkayı genişletir ya da görüşsüz en yakın adayı seçer (bu planda yok).

**Kesin kurallar:** (1) Yukarıdaki kurallar bağlayıcıdır; sayısal beklentiler (Tablo 1–4, referans, gerçek harita) bu ifadelerle üretilmiştir. (2) Heap yok (yalnızca `scratch`); özyineleme yok; NaN/sonsuz girdi denenmez. (3) MSVC Level 4 uyarıları `static_cast` ile çözülür (`#pragma warning` ekleme); kullanılmayan parametre/değişken bırakma. (4) `1e300` dışında sihirli sayı yok: varsayılanlar `NavLosParams` içindedir.

### 5.2 `Tests/BotCoreTests/NavLosTests.cpp`

`#include "MiniTest.h"`, `<BotCore/NavGrid.h>`, `<BotCore/NavPath.h>`, `<BotCore/NavSmooth.h>`, `<BotCore/NavTrack.h>`, `<BotCore/NavLos.h>`, `<algorithm> <chrono> <cmath> <cstdint> <cstdio> <random> <string> <vector>`. Anonim ad alanında yardımcılar (kendi kopyaların; **kullanılmayan yardımcı tanımlama**, MSVC C4505 uyarısı çıkar): `CellIndex`, `Cell`, `HeightZeros`, `RingEvents`, `MakeNav` (`NavStuckTests.cpp` ile aynı), `PercentileDouble`, ve:

- `std::vector<int16_t> BlockedEvents(const std::vector<NavCell> & blocked)`: `RingEvents(40)` + verilen hücreler 0.
- `std::vector<int16_t> WallEvents(int zLo, int zHi)`: `RingEvents(40)` + `x = 20`, `z = zLo..zHi` 0 (`wall` = 10..30, `split` = 1..38).
- `std::vector<float> HillHeights(float h)`: 40×40 köşe yükseklikleri, `x = 20` ve `x = 21` (tüm `z`) = `h`, diğerleri 0 (`CellIndex(40, x, z)` düzeni).
- `bool BothWays(const NavGrid &, float ax, float az, float bx, float bz, bool expected)`: `NavLosGridClear` ileri ve geri sonucunu `CHECK_EQ(…, expected)` ile doğrular (Tablo 1); benzer biçimde arazi için `NavLosTerrainClear`/`NavLosClear` ileri-geri.
- Referans: `bool RefClear(const NavGrid &, double ax, double az, double bx, double bz)`: 40×40 ızgarada tüm hücreleri gez; `Event(x, z) == 1` ise atla; başlangıç ya da bitiş hücresi ise atla; aksi hâlde `t0 = 0`, `t1 = 1`, her eksen için: `d == 0` ise `!(o > lo && o < hi)` ⇒ bu hücre kesişmez; değilse `a = (lo - o) / d`, `b = (hi - o) / d`, `a > b` ise takas, `t0 = max(t0, a)`, `t1 = min(t1, b)`; sonunda `t0 < t1 - 1e-9` ise ışın hücrenin iç kısmını keser ⇒ `false` döndür. Tüm hücreler geçerse `true`. (`lo = x * unit`, `hi = (x + 1) * unit`, `o` = başlangıç koordinatı, `d` = fark.)

| Test adı | İçerik |
|---|---|
| `NavLos_Grid` | **Tablo 1**'in tüm satırları (1–35), her biri iki yönde. |
| `NavLos_Terrain` | **Tablo 2**'nin tüm satırları ve parametre değişimleri; ayrıca `hill(1.9)` üzerinde `NavLosClear == false` iken `NavLosGridClear == true`; `NavLosClear == (NavLosGridClear && NavLosTerrainClear)` her satırda. |
| `NavLos_Mode` | **Tablo 3**: varsayılanlar ve `NavLosAllows` dört durumu. |
| `NavLos_PickCell` | **Tablo 4**'ün tüm satırları (a–m): sonuç hücre, bulundu/bulunamadı, `scratch.size()`; (h)'de `out` değişmez (`out` önce `(-7,-7)` ile doldurulur); `scratch` çağrılar arasında yeniden kullanılır. |
| `NavLos_Reference` | Referans çapraz doğrulama (yukarıdaki üretim sırasıyla 5000 sorgu): uyuşmazlık 0, simetri farkı 0, açık sorgu sayısı `[3000, 3500]`; yazdır: `NAVLOS ref: queries=5000 clear=<n> mismatches=0 asym=0` (prototip: `clear=3272`). |
| `NavLos_RealMap` | Zone 71 taraması (aşağıda). |

**Gerçek harita ortak kuralı** (F5-02..F5-09 ile aynı): dosya yolu `build/nav/zone71.navgrid` (çalışma dizini depo kökü); açılamazsa testi **başarısız yapma**: `std::printf("NAVLOS real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n")` yaz ve dön. `LoadFile` + `Build()` (varsayılan `NavParams`) sonrası `MainComponentCells() == 88508` `REQUIRE` edilir.

`NavLos_RealMap`: yukarıdaki tarama (x-ana, `((x * 7 + z * 13) % 5) == 0`, beş ofset, hedef `Walk` değilse atla): her çift için `NavLosClear` ileri (süre `std::chrono::steady_clock` ile **yalnızca bu çağrı** için ölçülür) ve geri, `NavLosGridClear` ileri ve geri; sayaçlar ofset başına `tested`/`grid`/`grid_terrain`, ayrıca `skipped`, `asym` (ileri ≠ geri, her iki işlev için), `violations`. Konum seçimi taraması: ofset (7,7) çiftlerinde `NavLosClear(kaynak → hedef)` false ise `NavPickLosCell(grid, hedefX, hedefZ, kaynakX, kaynakZ, 0.0f, 30.0f, NavLosParams(), scratch, out)` (süre ölçülür): `reposition`, `found`, `pick_violations` (kural yukarıda). Beklenen: Tablo "Zone 71 taraması" sayıları **tam eşit** (ofset başına ve toplam), `skipped == 23966`, `asym == 0`, `violations == 0`, `reposition == 3935`, `found == 3935`, `pick_violations == 0`. Süre kapıları (`#ifndef _DEBUG`, Debug'da atlanır): `NavLosClear` p95 `<= 0.05` ms, `NavPickLosCell` p95 `<= 0.5` ms. Yazdır: `NAVLOS real: walk=88508 tested=64559 grid=47245 grid_terrain=43163 skipped=23966 asym=0 violations=0 ms_p95=<x.xxxx>`; her ofset için `NAVLOS real off(<dx>,<dz>): tested=<n> grid=<n> grid_terrain=<n>`; `NAVLOS real pick: reposition=3935 found=3935 violations=0 ms_p95=<x.xxxx>`. Not (sayılar tutmazsa): **kuralı sayıya uydurma**; dur ve Uygulayıcı Raporu'nda sor (MSVC'de eşik sınırında birkaç sayı oynarsa farkı ve ofseti raporla).

### 5.3 Proje dosyaları

1. `BotCore/BotCore.vcxproj`: `ClInclude` grubuna `<ClInclude Include="NavLos.h" />` (`NavBudget.h` satırından sonra, `Perception.h` satırından önce; dosyada şu an `:84-85`). BOM/CRLF korunur.
2. `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubuna `<ClCompile Include="NavLosTests.cpp" />` (`NavBudgetTests.cpp` satırından sonra, `PerceptionTests.cpp` satırından önce; şu an `:94-95`). BOM/CRLF korunur.
3. Bu iki projenin `.filters` dosyası yoktur (F5-01'de doğrulandı); ek dosya yok.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; `BotCore` ve `BotCoreTests` için **yeni uyarı yok** (Level 4; `NavLos.h`, `NavLosTests.cpp` dosyalarını `touch` ile zorla yeniden derle; çıktıda `NavLos` geçen `warning` satırı bulunmaz).
- [ ] K2: `./tools/run-tests.sh Release --no-build --list` çıktısı şu altı test adını içerir: `NavLos_Grid`, `NavLos_Terrain`, `NavLos_Mode`, `NavLos_PickCell`, `NavLos_Reference`, `NavLos_RealMap`.
- [ ] K3: `build/nav/zone71.navgrid` varken (yoksa `python3 tools/nav-export.py` ile üret) `./tools/run-tests.sh Release --no-build` çıkış kodu 0; çıktıda `223 tests, 0 failed` (önceki 217 + altı yeni), tüm eski testler ve altı yeni test `[ OK ]`; `SKIPPED` geçmiyor; `NAVLOS ref: …`, `NAVLOS real: …`, beş `NAVLOS real off(…)` ve `NAVLOS real pick: …` satırları var.
- [ ] K4: Gerçek harita yokken (`build/nav/zone71.navgrid` geçici olarak başka ada taşınarak) `./tools/run-tests.sh Release --no-build NavLos_` çıkış kodu 0, `NavLos_RealMap` `SKIPPED` yazar ve diğer beş test geçer; ardından dosya yerine geri konur.
- [ ] K5: `./tools/run-tests.sh Debug` (derleme dahil) çıkış kodu 0 (Debug'da `assert`/sınır hataları yok).
- [ ] K6: Davranış sayıları (Release ve Debug): §2 Tablo 1–4'teki tüm değerler (`NavLos_Grid`, `_Terrain`, `_Mode`, `_PickCell` `[ OK ]`); `NavLos_Reference` (`mismatches=0 asym=0`, `clear` ∈ [3000, 3500]); `NavLos_RealMap` (`tested=64559 grid=47245 grid_terrain=43163 skipped=23966 asym=0 violations=0`; ofset satırlarındaki sayılar Tablo "Zone 71 taraması" ile aynı; `reposition=3935 found=3935 violations=0`). Release'te `ms_p95` kapıları geçer.
- [ ] K7: Saflık/kapsam: `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavLos.h` çıktısı boş; `git diff --stat gece/2026-10-02-nav...bot/F5-10` yalnızca §4'teki dört dosyayı ve kendi plan dosyasını gösterir (`GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h`, `NavTrack.h`, `NavSmooth.h`, `NavStuck.h` vb. yok).
- [ ] K8: Önceki işler bozulmadı: önceki 217 testin hepsi **değişmeden** `[ OK ]` (testler düzenlenmez); `NAVGRID real map: n=513 main=88508 …`, `NAVPATH T-NAV-03 set=near64 … found=997 … expanded_p50=306 expanded_p95=2431`, `NAVREACH real: components=143 largest=88279 pockets=229 …`, `NAVRETREAT real: solo A cell=(159,228) … len=105.657 cand=2799 …`, `NAVFORM real: walk=88508 usable8=72459 …`, `NAVSTUCK real: walk=88508 sidestep_h=86017 sidestep_n=86968 violations=0 clr2_cells=66265 clr2_found=66003 …` satırlarında aynı sayılar.

## 7. Doğrulama komutları

```bash
git switch -c bot/F5-10 gece/2026-10-02-nav
python3 tools/nav-export.py            # build/nav/zone71.navgrid yoksa
./tools/build.sh Release
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Release --no-build NavLos_
./tools/run-tests.sh Debug
grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavLos.h
git diff --stat gece/2026-10-02-nav...bot/F5-10
```

## 8. Kısıtlar ve uyarılar

- **Paralel hat:** sunucuya hiç dokunma (`tools/run-servers.sh` çağırma; ana hat başka çalışma ağacından sunucu çalıştırıyor olabilir). DB'ye bağlanma. Dal tabanı `gece/2026-10-02-nav`.
- Kodlama/satır sonu: `AGENTS.md` §3. Yeni C++ dosyaları ASCII + CRLF + tab + Allman. Yorumlar İngilizce; plan metnindeki Türkçe açıklama koda girmez.
- Başlık-yalnızca: tüm tanımlar `inline`; değişebilir `static`/global durum yok; `#pragma` yalnızca `once`.
- **Belirlenim:** aynı girdiler her zaman aynı çıktıyı verir (rastgelelik yalnızca testte ve `std::mt19937` ile sabit tohum, saat yok). `std::uniform_*_distribution` **kullanma** (taşınabilir değil); yalnızca §2'deki `U()` dönüşümü.
- Sayısal beklentiler (Tablo 1–4, referans, gerçek harita: 1,6 m / 0,25 m / 2 m / hücre kuralı) `[O]`/`[A]` değerleridir: parametre varsayılanlarını ve kuralları değiştirme. **Eşitlik** kuralları (köşeye değme = engel değil, hücre sınırı = büyük taraftaki hücre, arazi `>` kesin, `t0 < t1 - 1e-9`) test beklentisinin parçasıdır; "iyileştirme" olarak çevirme.
- **Bilinçli sınırlamalar (düzeltme isteme):** (1) Engel hücreler sonsuz yüksek sayılır: alçak çit/çatı gibi ayrıntılar yok; olay ızgarası çarpışma poligonlarından türetildiği için bu muhafazakârdır. (2) Göl kıyıları olay ızgarasında engelli hücrelerdir: göl üzerinden görüş "kapalı" çıkar (yanlış negatif; advisory modda yalnızca konum tercihini etkiler). (3) Arazi yalnızca 2 m örneklenir ve bilinear yüzeyin dar tepeleri örneklerin arasına düşebilir. (4) 4 m ızgarada hücre kuralı kabadır: duvarın hücre içindeki ince konumu bilinmez. Gerekçesi ADR-0006 Eki F5-10'dadır.
- Float: MSVC ve g++ arasında son bit farkları olabilir; hücre kuralı `double` ile yapılır ve tam sayılar üretir; arazi kuralında eşik sınırındaki satırlar (`H = 1,85` gibi) testte **kullanılmaz** (1,8 ve 1,9 kullanılır). Beklenti tutmazsa **kuralı değiştirme**, dur ve raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-10` — `2124a1d [F5-10] Gorus hatti (LoS, advisory): NavLos.h + testler`
- Değişen dosyalar ve neden:
  - `BotCore/NavLos.h` (yeni): `NavLosMode`, `NavLosParams`, `NavLosGridClear` (genişletilmiş DDA, `double`, uç hücreler muaf, köşe değmesi engel değil), `NavLosTerrainClear` (göz = zemin + 1,6 m, 2 m örnekleme, 0,25 m pay), `NavLosClear`, `NavLosAllows`, `NavPickLosCell` (`NavRingCells` sırasıyla ilk görüşlü `Walk` hücre). Başlık-yalnızca, `inline`, ASCII+CRLF, yalnızca `NavTrack.h` + standart başlıklar.
  - `Tests/BotCoreTests/NavLosTests.cpp` (yeni): altı test (`NavLos_Grid`, `NavLos_Terrain`, `NavLos_Mode`, `NavLos_PickCell`, `NavLos_Reference`, `NavLos_RealMap`); kendi yardımcı kopyaları, bağımsız dilim-testi referansı, gerçek harita taraması + `SKIPPED` kalıbı, Release süre kapıları.
  - `BotCore/BotCore.vcxproj` / `Tests/BotCoreTests/BotCoreTests.vcxproj`: yalnızca birer `NavLos.h` / `NavLosTests.cpp` satırı (BOM/CRLF korundu).
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
    proj-AIServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\AIServer.exe
    NavLosTests.cpp
    BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  `touch` sonrası `NavLos` içeren uyarı/hata yok. Debug derlemesi de rc=0.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔: Release rc=0, `touch` ile yeniden derlemede `NavLos` geçen uyarı yok.
  - K2 ✔: `--list` altı `NavLos_*` adını içeriyor.
  - K3 ✔: `223 tests, 0 failed`; `NAVLOS ref: queries=5000 clear=3272 mismatches=0 asym=0`; `NAVLOS real: walk=88508 tested=64559 grid=47245 grid_terrain=43163 skipped=23966 asym=0 violations=0 ms_p95=0.0015`; beş ofset satırı Tablo ile birebir; `NAVLOS real pick: reposition=3935 found=3935 violations=0 ms_p95=0.0145`.
  - K4 ✔: dosya taşınınca `NavLos_RealMap` `SKIPPED`, diğer beşi geçti, rc=0; dosya geri kondu.
  - K5 ✔: `./tools/build.sh Debug` rc=0; Debug'da `223 tests, 0 failed`.
  - K6 ✔: Release ve Debug'da Tablo 1–4 tümü `[ OK ]`; referans `mismatches=0 asym=0 clear=3272 ∈ [3000,3500]`; gerçek harita sayıları tam; Release `ms_p95` kapıları geçti (0,0015 ≤ 0,05 ve 0,0145 ≤ 0,5).
  - K7 ✔: saflık grep'i boş; `git diff --stat gece/2026-10-02-nav...bot/F5-10` yalnızca dört dosya + plan.
  - K8 ✔: önceki 217 test değişmeden `[ OK ]`; K8'de sayılan `NAVGRID/NAVPATH/NAVREACH/NAVRETREAT/NAVFORM/NAVSTUCK real` satırlarındaki sayılar aynı.
- Plandan sapmalar ve gerekçeleri: yok. Kurallar ve formüller plandakiyle aynen uygulandı; tüm sayısal beklentiler prototiptekiyle birebir çıktı (referans `clear=3272`, gerçek harita toplamları ve ofset satırları dahil).
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02-nav...bot/F5-10` @ `1f3d1c3` (kod commit'i `2124a1d`; paralel hat `nav`, gece modu, `AUTO_LOOP=1`; sunuculara dokunulmadı, birleştirme/push yapılmadı)
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `NavLos.h` ve `NavLosTests.cpp` `touch` sonrası `./tools/build.sh Release` rc=0 (çıktıda `NavLosTests.cpp` yeniden derlendi); çıktıda `warning`/`error` yok (`/v:minimal` uyarıları da basar; Level 4 açık: `BotCore.vcxproj:50,60`, `BotCoreTests.vcxproj:52,66`) |
| K2 | ✔ | `run-tests.sh Release --no-build --list`: `NavLos_Grid`, `NavLos_Terrain`, `NavLos_Mode`, `NavLos_PickCell`, `NavLos_Reference`, `NavLos_RealMap` (satır 171-176) |
| K3 | ✔ | Release tam koşu rc=0, `223 tests, 0 failed`, 223 `[ OK ]`; gerçek `SKIPPED` satırı yok (`grep -i skipped` yalnızca `skipped=23966` sayacını eşleştirir); `NAVLOS ref: queries=5000 clear=3272 mismatches=0 asym=0`; beş `NAVLOS real off(...)` satırı; `NAVLOS real: walk=88508 tested=64559 grid=47245 grid_terrain=43163 skipped=23966 asym=0 violations=0 ms_p95=0.0009`; `NAVLOS real pick: reposition=3935 found=3935 violations=0 ms_p95=0.0140` |
| K4 | ✔ | `zone71.navgrid` geçici olarak `.bak` adına taşındı: `run-tests.sh Release --no-build NavLos_` rc=0, `NAVLOS real map: SKIPPED (...)`, `6 tests, 0 failed`; dosya geri konuldu (`ls build/nav`) |
| K5 | ✔ | `./tools/run-tests.sh Debug` (derleme dahil) rc=0, çıktıda `warning`/`error` yok, `223 tests, 0 failed` |
| K6 | ✔ | Tablo 1-4 (`NavLos_Grid/_Terrain/_Mode/_PickCell`) Release ve Debug'da `[ OK ]`; testin plan tablolarıyla satır satır karşılaştırması: Tablo 1 satır 1-35, Tablo 2 altı satır x dört `H` + altı parametre değişimi, Tablo 3, Tablo 4 a-m (`NavLosTests.cpp:208-435`); referans `mismatches=0 asym=0 clear=3272`; gerçek harita sayıları (toplam ve ofset başına) tam eşit, Release `ms_p95` 0,0009 ≤ 0,05 ve 0,0140 ≤ 0,5 (Debug'da kapı `#ifndef _DEBUG` ile atlanır; Debug pick p95 0,4003) |
| K7 | ✔ | `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavLos.h` boş (rc=1); `git diff --stat gece/2026-10-02-nav...bot/F5-10`: yalnızca `BotCore/BotCore.vcxproj` (+1), `BotCore/NavLos.h` (+170), `BotCoreTests.vcxproj` (+1), `NavLosTests.cpp` (+637), kendi plan dosyası; `GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h`, `NavTrack.h`, `NavStuck.h` yok; silinen kod satırı 0 |
| K8 | ✔ | Eski 217 test değişmedi (testlerde silinen satır yok, yalnızca yeni dosya); Release'te `NAVGRID real map: n=513 main=88508`, `NAVPATH T-NAV-03 set=near64 ... found=997 ... expanded_p50=306 expanded_p95=2431`, `NAVREACH real: components=143 largest=88279 pockets=229`, `NAVRETREAT real: solo A cell=(159,228) ... len=105.657 cand=2799`, `NAVFORM real: walk=88508 usable8=72459`, `NAVSTUCK real: walk=88508 sidestep_h=86017 sidestep_n=86968 violations=0 clr2_cells=66265 clr2_found=66003` plandaki sayılarla aynı |

- Denetim notları (engel değil):
  - Kod incelemesi: `NavLosGridClear` (`BotCore/NavLos.h:36-98`) planın §5.1 kurallarıyla birebir (`unit <= 0` ret, aynı hücre muaf, `double` genişletilmiş DDA, `1e300` "geçiş yok", eşitlikte köşegen adım, bitiş hücresi muaf, ızgara dışı `Event != 1` engelli, `Walk` kullanılmaz); `NavLosTerrainClear` (`:103-130`) kesin `>`, uçlar örneklenmez, `sampleM <= 0`/`len <= 0` kısayolu; `NavPickLosCell` (`:149-169`) başarısızlıkta `out`'a dokunmaz, `scratch`'i `NavRingCells` ile tamamen yeniden yazar. Heap yok (yalnızca `scratch`), global/`static` değişebilir durum yok, saat yok, yalnızca `NavTrack.h` + standart başlıklar.
  - Biçim: yeni dosyalar ASCII, çalışma ağacında CRLF (indekste LF; mevcut dosyalarla aynı `autocrlf` düzeni), tab girinti, Allman. `.vcxproj` farkları yalnızca birer satır, BOM korunmuş (`EF BB BF`, taban ile aynı), konum plana uygun. `build/` commit edilmemiş, çalışma ağacı temiz.
  - Küçük not: plan `NavLos_Terrain` için "`NavLosClear == (NavLosGridClear && NavLosTerrainClear)` her satırda" der; test bunu yalnızca satır bazında `NavLosClear` ile, `NavLosTerrainClear` ile ise iki satırda (`NavLosTests.cpp:297-298`) ve 1,9 örneğinde sınar. Bileşim kuralı gerçek haritada 64559 çiftte `violations == 0` ile zaten sınanıyor; ek iş istenmez.
  - Uygulayıcı Raporu'ndaki iddialar (commit, dosyalar, 223 test, referans ve harita sayıları, K4) bağımsız olarak yeniden üretildi; fark yok. Rapordaki Release `ms_p95` 0,0015/0,0145, doğrulamada 0,0009/0,0140 (makine yükü farkı; kapıların çok altında).
  - İstemci (GUI) gerektiren kontrol yok; T-NAV-LOS-01 (oyun içi ölçüm) plan gereği kapsam dışı.
- Bulgular (önem sırasıyla): engelleyici bulgu yok.
- Düzeltme talimatı: gerekmiyor.
