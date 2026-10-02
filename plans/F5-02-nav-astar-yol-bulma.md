# F5-02: Izgara A* yol bulma (`BotCore/NavPath.h`) ve T-NAV-03 performans testi

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat, `docs/17` §1 "Paralel yürütülebilir işler") |
| Branch | `bot/F5-02` (taban: `gece/2026-10-02-nav`) |
| Bağımlı olduğu planlar | F5-01 (`BotCore/NavGrid.h`, `tools/nav-export.py`): `KAPANDI`, `gece/2026-10-02-nav` içinde (merge `788aa86`) |
| İlgili gereksinim / kabul | `docs/12` §4.1 (A* tanımı), §11 T-NAV-03, AC-NAV-02 (p95 ≤ 2 ms, ≤ 20 000 düğüm), REQ-NAV-01; ADR-0006 (kararlar bu planın temelidir), ADR-0016 (`BotCore` saflığı) |
| Tahmini büyüklük | M (4 dosya: 2 yeni, 2 değişen; ~600 satır) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

`NavGrid` (F5-01) üzerinde, sunucusuz ve belirlenimli bir **ızgara A\*** yol bulucusu yazmak: `BotCore/NavPath.h` (`NavPathfinder`). Sonuç yol hücre listesi (başlangıç ve hedef dahil), metre cinsinden maliyet, genişletilen düğüm sayısı ve **üç ayrı durumdur**: `Found`, `NoPath` (arama bitti, hedefe ulaşılamıyor), `NodeLimit` (düğüm sınırı doldu: bilinmiyor). Plan ayrıca T-NAV-03'ü (1000 rastgele sorgu) ölçülebilir bir birim/performans testi olarak koyar.

Bu planın sonunda **yol düzleştirme, hareketli hedef, ulaşılamaz hedef kararı, tehlike/clearance maliyeti, hiyerarşik arama yoktur** (F5-03..F5-06). Sunucu, `GameServer/`, `AIServer/`, `shared/` değişmez; sunucu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0006-navigasyon-izgara-astar.md` — **önce bunu oku**: karar (ızgara A\*), düğüm sınırı anlamı (`NodeLimit` ≠ ulaşılamaz), T-NAV-03 sorgu dağılımı ve Python prototip ölçümleri (bu planın sayıları buradan gelir).
- `docs/12_NAVIGATION_AND_POSITIONING.md` §4.1 (A\* tanımı; **maliyet formülündeki `w_danger`, `w_clear` ve eğim cezası terimleri bu planda yoktur**), §11 (T-NAV-03, AC-NAV-02).
- `BotCore/NavGrid.h` (F5-01) — kullanacağın API: `Size()`, `Unit()`, `InBounds`, `Walk(x, z)`, `EdgeOpen(x, z, dx, dz)` (iki uç `Walk`, eğim kuralı, çaprazda köşe kesme yok, simetrik), `CellOf(float)`, `MainComponentCells()`, `LoadFile`, `Build`. Yol bulucu **`NavGrid`'in kuralını yeniden yazmaz**: komşuluk kararı yalnızca `EdgeOpen`'dan gelir. Dosya F5-01 testleriyle sabitlenmiştir; **davranışı değiştirme** (bkz. §4 kısıt).
- `BotCore/Rng.h` — testlerde `BotCore::Rng` (`NextBelow`, `NextDouble`); global `rand()` yasak.
- `Tests/BotCoreTests/NavGridTests.cpp` — dosya biçimi, `MakeNav`/`RingEvents`/`CellIndex` yardımcıları (anonim ad alanı; **yeni test dosyasında kendi kopyalarını yaz**, başka `.cpp`'den içe aktarma yok), gerçek harita yükleme ve `SKIPPED` kalıbı (`NavGridTests.cpp:409` civarı), `std::chrono::steady_clock` kullanımı (`:442`).
- `Tests/BotCoreTests/MiniTest.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`), `tools/run-tests.sh` (testi depo kökünde çalıştırır; `build/nav/zone71.navgrid` göreli açılır).
- ADR-0016: `BotCore` yalnızca standart kütüphane; `windows.h`, `stdafx.h`, `shared/`, `GameServer/` **içermez**.

Planı yazarken doğrulanan gerçekler (Python prototipi, 2026-10-02; uygulayıcı yeniden doğrulamak zorunda değil ama sayılar test eşikleridir):

- Zone 71 ana bileşeni 88 508 hücre; A (1274, 890) → hücre (318, 222), B (746, 1106) → hücre (186, 276) (`CellOf(w) = floor(w / 4)`).
- A → B en kısa yol maliyeti **660,617 m**, genişletilen düğüm ≈ 3 247 (aynı kenar kuralıyla; C++ beraberlik sırası farklı olabilir, düğüm sayısı kapı değildir).
- A (318, 222) cebe (174, 215) (15 hücrelik, eğim kenarlarıyla kopuk cep; `Walk` = true): **yol yok**; arama erişilebilir 88 279 hücrenin tamamını genişletip biter.
- Sorgu dağılımına göre genişletilen düğüm p95: Chebyshev ≤ 32 hücre 674, ≤ 50 hücre 2 438, ≤ 150 hücre 6 512, tüm harita 19 042 (ayrıntı ADR-0006).

## 3. Kapsam

**Yapılacaklar**

- `BotCore/NavPath.h`: `NavCell`, `NavPathStatus`, `NavSearchParams`, `NavPathResult`, `NavOctile`, `NavPathfinder` (§5.1).
- `Tests/BotCoreTests/NavPathTests.cpp`: §5.2 test listesi (birim + gerçek harita + T-NAV-03 performans).
- `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` kayıtları (§5.3).

**Kapsam dışı (yapılmayacak)**

- Yol düzleştirme / Bresenham, ara nokta üretimi (F5-03), hareketli hedef ve yeniden planlama (F5-04), ulaşılamaz hedef **kararı**/`TARGET_UNREACHABLE` (F5-05: bu plan yalnızca `NoPath`/`NodeLimit` durumunu döndürür), `danger_*` ve `clearance` ağırlıkları (F5-06), güvenli nokta (F5-07), formasyon (F5-08), takılma (F5-09), LoS (F5-10).
- **Hiyerarşik arama / `region_graph`**, çift yönlü arama, JPS, önhesaplama. Düğüm sınırı aşılınca yapılacak tek şey `NodeLimit` döndürmektir.
- Eğim cezası (yumuşak maliyet). Maliyet **yalnızca yatay mesafedir** (ortogonal `unit`, çapraz `unit·√2`); sert eğim kesmesi `EdgeOpen`'dadır.
- "En yakın açık hücre", hedefin çevresindeki halka (menzil hedefi) araması; engelli/`Walk` olmayan hedef için **düzeltme yapılmaz**, `InvalidGoal` döner.
- `GameServer/`, `AIServer/`, `shared/`, `docs/` değişikliği, DB, sunucu çalıştırma, yeni `P-NAV-*` ayar dosyası/okuyucusu (`NavSearchParams` varsayılanları yeterlidir).
- `GameServer/proj-GameServer.vcxproj` değişmez (`BotCore` sunucuya bağlı değil).
- Telemetri (`NAV_PATH` olayı vb.): sunucu tarafı entegrasyon ayrı plandır.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavPath.h` | yeni | ASCII, CRLF, başlık-yalnızca (satır içi) |
| `Tests/BotCoreTests/NavPathTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="NavPath.h" />` satırı (BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="NavPathTests.cpp" />` satırı (BOM ve CRLF korunur) |
| `BotCore/NavGrid.h` | **yalnızca koşullu** | Yalnızca K7 performans kapısı, `NavPath.h` içinde yapılabilecek her iyileştirmeden **sonra** hâlâ tutmuyorsa; yalnızca **ekleme** (ör. `EdgeOpen`'ın satır içi/hızlı bir eşi) yapılabilir. Mevcut yöntemlerin davranışı ve F5-01 testleri değişmez. Yapılırsa Uygulayıcı Raporu'nda ölçümle gerekçelendir. |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`build/nav/zone71.navgrid` üretilen çıktıdır, commit edilmez; yoksa `python3 tools/nav-export.py` ile üret.)

## 5. Uygulama adımları

### 5.1 `BotCore/NavPath.h`

Ad alanı `BotCore`; `#include "NavGrid.h"` (aynı dizin, tırnaklı) ve yalnızca standart başlıklar (`<algorithm> <cmath> <cstddef> <cstdint> <vector>`). Çevredeki kodla aynı stil (`NavGrid.h`): tab girinti, Allman, İngilizce yorum, `inline` tanımlar.

```cpp
namespace BotCore
{
	struct NavCell
	{
		int x = 0;
		int z = 0;
	};
	inline bool operator==(const NavCell & a, const NavCell & b) { return a.x == b.x && a.z == b.z; }
	inline bool operator!=(const NavCell & a, const NavCell & b) { return !(a == b); }

	enum class NavPathStatus
	{
		Found,
		NoPath,         // the reachable region was exhausted without meeting the goal
		NodeLimit,      // maxNodes closed nodes without meeting the goal: "unknown", not "unreachable"
		InvalidStart,   // start out of bounds / not Walk (also before NavGrid::Build)
		InvalidGoal     // goal out of bounds / not Walk
	};

	struct NavSearchParams
	{
		// P-NAV-MAX-NODES [Ö] (docs/12 s4.1, ADR-0006): max number of closed (expanded) nodes.
		int maxNodes = 20000;
	};

	struct NavPathResult
	{
		NavPathStatus status = NavPathStatus::NoPath;
		std::vector<NavCell> cells;   // start..goal inclusive; empty unless Found
		float cost = 0.0f;            // metres (sum of step lengths); 0 unless Found
		int expanded = 0;             // closed nodes, set for every status that ran a search
	};

	// Octile distance in metres between two cells `dx`, `dz` apart (consistent heuristic for
	// unit / unit*sqrt(2) steps): unit * (|dx| + |dz| - 2*min + sqrt(2)*min).
	inline float NavOctile(int dx, int dz, float unit);

	class NavPathfinder
	{
	public:
		// Reusable between queries and grids (buffers are resized when the grid size changes);
		// NOT thread-safe: one instance per thread/context. Deterministic: same grid, params and
		// query give the same result, also after other queries ran on the same instance.
		// `grid` is never modified. `out` is fully overwritten.
		void Find(const NavGrid & grid, NavCell start, NavCell goal, const NavSearchParams & params, NavPathResult & out);

	private:
		// per-cell node pool: g (float), parent (int32), `seen` and `closed` generation stamps
		// (uint32), a generation counter, and the heap vector (all members; no static state).
	};
}
```

Algoritma (kurallar kesindir; her biri §5.2'deki testle sabitlenir):

1. **Doğrulama sırası:** `start` ızgara dışı veya `!grid.Walk(start)` → `InvalidStart`; sonra `goal` için `InvalidGoal`; `NavGrid::Build` çağrılmamışsa `Walk` her yerde false olduğundan `InvalidStart` döner (ayrıca denetim ekleme). Bu durumlarda `expanded = 0`, `cells` boş, `cost = 0`. Ardından `start == goal` → `Found`, `cells = { start }`, `cost = 0`, `expanded = 0`.
2. **Düğüm havuzu:** her hücre için `g`, `parent`, `seen`/`closed` damgaları; sorgu başına `generation` bir artar (taşarsa dizileri sıfırla). Sorgu başında tüm `n·n` diziyi **temizleme** (damga kullan; 263 169 hücreyi her sorguda sıfırlamak 2 ms bütçesini yer). Izgara boyutu değişirse diziler yeniden boyutlandırılır ve sıfırlanır.
3. **Açık liste:** `std::vector` + `std::push_heap`/`std::pop_heap` (ikili yığın), girdi `{ float f; float g; int32_t idx; }`. **Tembel silme:** bir hücre daha iyi `g` ile yeniden eklenebilir; çıkan girdinin hücresi zaten `closed` ise atla. Sıralama (kesin, belirlenimli): küçük `f` önce; eşitse **büyük `g`** önce; eşitse **küçük `idx`** önce.
4. **Komşular ve sıra:** sabit sıra `(+1,0), (-1,0), (0,+1), (0,-1), (+1,+1), (+1,-1), (-1,+1), (-1,-1)` (`dx, dz`). Bir komşu yalnızca `grid.EdgeOpen(x, z, dx, dz)` true ise denenir (kenar zaten iki uç `Walk`, eğim ve köşe kesmeyi sağlar). Adım uzunluğu ortogonal `unit`, çapraz `unit * sqrt(2)` (`float`). `closed` komşu atlanır; yeni `g = g[cur] + adım`; komşu bu sorguda görülmemişse ya da yeni `g` **kesin olarak** daha küçükse `g`, `parent` güncellenir ve `f = g + NavOctile(|x-gx|, |z-gz|, unit)` ile yığına eklenir.
5. **Ana döngü (düğüm sayımı kesin):** yığın boşalana kadar: en iyi girdiyi çıkar; hücresi `closed` ise atla. **`expanded >= params.maxNodes` ise** `NodeLimit` döndür (`out.expanded = expanded`, `cells` boş) — **hedef kontrolünden önce**. Aksi halde hücreyi `closed` yap, `++expanded`; hücre hedefse `Found`: `parent` zincirini hedeften başlangıca izleyip tersine çevir (`cells` başlangıç→hedef, ikisi dahil), `cost = g[goal]`. Komşuları genişlet (madde 4). Yığın boşalırsa `NoPath` (`out.expanded = expanded`). Sonuç: `maxNodes = M` ile bir sorgu, hedef ilk `M` kapatılan düğüm içindeyse `Found` döndürür; `M ≤ 0` ise (başlangıç ≠ hedef) hemen `NodeLimit`, `expanded = 0`.
6. **Optimallik:** sezgisel tutarlıdır (octile, adım maliyetleriyle uyumlu), `closed` düğüm yeniden açılmaz; sonuç Dijkstra ile aynı maliyettedir (float toplama toleransı içinde).
7. `NavGrid`'e dokunma (`const &`); yol bulucu hiçbir global/`static` durum tutmaz (birden çok örnek test edilebilmeli).
8. `NavOctile`: `float` hesap, `dx`/`dz` işaretsiz kullanılır (`std::abs`).

Performans notları (zorunlu değil, kapıya ulaşmak için serbest): `Find` içinde `NavGrid`'i bir kez `Size()`/`Unit()` ile oku, komşu döngüsünde `std::vector::push_back` yerine önceden `reserve` edilmiş yığın kullan, `cells` için `reserve`. `Find` her sorguda **bellek ayırmamalı** (ilk sorgudan sonra; sonuç vektörü büyümesi hariç).

### 5.2 `Tests/BotCoreTests/NavPathTests.cpp`

`#include "MiniTest.h"`, `<BotCore/NavGrid.h>`, `<BotCore/NavPath.h>`, `<BotCore/Rng.h>` (test dosyasındaki include biçimi `NavGridTests.cpp` gibi). Anonim ad alanında yardımcılar: `CellIndex`, `RingEvents` (kenar halkası engelli, iç açık), `MakeNav(n, unit, events, heights)` (Init + Build), `PathIsValid(grid, result)` (her adım komşu: `|dx|,|dz| ≤ 1`, ikisi 0 değil; her adımda `grid.EdgeOpen(prev.x, prev.z, dx, dz)` true; ilk = başlangıç, son = hedef; adım uzunlukları toplamı `cost`'a `1e-3 * cost + 1e-3` içinde eşit), `ReferenceCost(grid, start, goal)` (**test içi bağımsız Dijkstra**: `NavPath.h`'yi kullanmaz; `std::vector<float>` mesafe + basit `O(V²)` ya da `std::priority_queue`; komşuluk `grid.EdgeOpen`, adım `unit`/`unit·√2`; ulaşılamazsa negatif döner).

Test adları (kabul kriterleri bu adlara dayanır):

| Test adı | İçerik |
|---|---|
| `NavPath_Status_Validation` | 12×12 ring, düz: engelli (kenar) hücre başlangıç → `InvalidStart`; ızgara dışı `(-1, 3)` başlangıç → `InvalidStart`; geçerli başlangıç + engelli/dışı hedef → `InvalidGoal`; ikisi de geçersiz → `InvalidStart` (önce başlangıç); hepsinde `expanded == 0`, `cells` boş. `NavGrid::Build` çağrılmamış ızgarada geçerli görünen sorgu → `InvalidStart`. `start == goal` → `Found`, `cells.size() == 1`, `cost == 0`, `expanded == 0`. |
| `NavPath_OpenField_Octile` | 40×40 ring, düz zemin, `unit = 4`: `(5,5)` → `(25,17)`: `Found`; `cost` = `NavOctile(20, 12, 4)` (≈ 99,882) `1e-3` içinde; `cells.size() == 21` (`max(dx,dz) + 1`); `PathIsValid`; ayrıca `NavOctile(3, 4, 4.0f)` = `4 * (3 + 4 - 2*3 + sqrt(2)*3)` `1e-4` içinde (formül sabiti). |
| `NavPath_NoCornerCutting` | 12×12 ring düz zemin, `x = 6` sütununda `z ∈ [1, 10]` engelli, **yalnızca `(6, 5)` açık**; `(5,4)` → `(7,6)`: `Found`, `cost` = **16,0** (`1e-3` içinde), `cells.size() == 5`, yol `(6,5)` üzerinden geçer ve `PathIsValid` (köşe kesme olsaydı 11,31 olurdu). Aynı sorgu ters yönde (`(7,6)` → `(5,4)`) aynı maliyet. |
| `NavPath_NoPath_SlopeCut` | 20×20 ring; yükseklik `x < 10` için 0, `x ≥ 10` için 10 m (eşik kenarı eğim kesmesine takılır; iki yarı da `Walk`: aynı 4-bağlantılı bileşen): `(2,2)` → `(15,10)` → `NoPath`; `expanded` tam olarak **162** (sol yarı: `x ∈ [1, 9]` × `z ∈ [1, 18]`); `cells` boş. Aynı yarı içindeki sorgu `Found`. |
| `NavPath_NodeLimit_Semantics` | 64×64 ring düz zemin, `(2,2)` → `(60,60)`; sınırsız (`maxNodes = 1000000`) koş: `Found`, `E = expanded`. `maxNodes = E` → `Found` (aynı `cost` ve `cells`); `maxNodes = E - 1` → `NodeLimit`, `expanded == E - 1`, `cells` boş, `cost == 0`; `maxNodes = 10` → `NodeLimit`, `expanded == 10`; `maxNodes = 0` ve `-5` → `NodeLimit`, `expanded == 0`; `start == goal` iken `maxNodes = 0` → `Found`. |
| `NavPath_MatchesDijkstra` | 6 rastgele 28×28 ızgara (`Rng(1000 + k)`, k = 0..5): ring engelli, iç hücrelerin ~%18'i engelli (`NextBelow(100) < 18`), her hücrenin yüksekliği `NextDouble() * 3.0` (bazı ortogonal kenarlar eğimden kapanır). Her ızgarada `Build` sonrası `Walk` hücre listesinden **belirlenimli** 40 çift (`Rng(5000 + k)`, `NextBelow`). Her çift için: `Find` (`maxNodes = 1000000`) ile `ReferenceCost`: durum `Found` **ancak ve ancak** referans ≥ 0; `Found` ise `cost` ≈ referans (`1e-3 * ref + 1e-3` içinde) ve `PathIsValid`. Toplamda `found > 0` (`CHECK`). Çıktı: `std::printf("NAVPATH dijkstra: pairs=%d found=%d nopath=%d\n", ...)` (bilgi). |
| `NavPath_Deterministic_Reuse` | Tek bir `NavPathfinder` örneği: sorgu A, sorgu B (farklı uzunluk), sorgu A tekrar → ilk ve üçüncü sonuç `status`/`cells`/`cost`/`expanded` bakımından **birebir aynı**; **taze** bir `NavPathfinder`'ın A sonucu da aynı. Sonra aynı örnek **farklı boyutlu** (ör. 16×16, 40×40, 16×16 sırasıyla) ızgaralarda: her sorgu doğru (`Found`, `PathIsValid`). Damga taşması testi gerekmez. |
| `NavPath_RealMap_Queries` | Gerçek harita (aşağıya bak). |
| `NavPath_Perf_T_NAV_03` | Gerçek harita (aşağıya bak): T-NAV-03, Release'te 3 × 1000 sorgu. |

**Gerçek harita ortak kuralı:** dosya yolu `build/nav/zone71.navgrid` (çalışma dizini depo kökü); açılamazsa testi **başarısız yapma**: `std::printf("NAVPATH real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n")` yaz ve dön. `LoadFile` + `Build()` (varsayılan `NavParams`) sonrası `MainComponentCells() == 88508` `REQUIRE` edilir.

`NavPath_RealMap_Queries`:

- A = `(CellOf(1274), CellOf(890))` = `(318, 222)`, B = `(CellOf(746), CellOf(1106))` = `(186, 276)`; her ikisi `Walk` (`REQUIRE`).
- `Find(A → B)` varsayılan `NavSearchParams`: `Found`; `cost` **660,617 ± 0,5 m**; `expanded ≤ 20000`; `PathIsValid`. `Find(B → A)`: `Found`, maliyet `1e-2` içinde aynı. Yazdır: `std::printf("NAVPATH arena A->B: cost=%.3f expanded=%d cells=%d\n", ...)`.
- Cep: `P = (174, 215)`; `Walk(P)` true (`REQUIRE`). `Find(A → P)`, `maxNodes = 200000`: `NoPath`; `expanded > 88000` ve `expanded ≤ grid.MainComponentCells()` (kesin sayı 88 279'dur, ama kenar eşiği float yuvarlaması kapı yapılmaz). Aynı sorgu varsayılan `maxNodes` ile: `NodeLimit`, `expanded == 20000`.
- Not (planın doğruladığı sayı tutmazsa): `cost` 660,617'den 0,5 m'den fazla ayrılırsa veya `P` `Walk` değilse/`NoPath` yerine `Found` dönerse **kuralı sayıya uydurma**; dur ve Uygulayıcı Raporu'nda sor.

`NavPath_Perf_T_NAV_03` (ADR-0006 madde 4):

- Hazırlık: `Walk` hücreleri x-ana taramada (`x` dış, `z` iç döngü) tek `std::vector<BotCore::NavCell>`'e topla (88 508 eleman).
- Üç küme; sorgu sayısı `Q` **Release'te 1000, Debug'da 100** (`#ifdef _DEBUG`; Debug'da iteratör denetimli vektörler yavaştır). Çiftler `Rng(20261002)`'den (kümeler arasında aynı `Rng` ilerler; sıra: `near64`, `far150`, `global`) `NextBelow(count)` ile çekilir; başlangıç == hedef ise ya da Chebyshev mesafesi (`max(|dx|, |dz|)`) kümenin sınırını aşıyorsa çift reddedilir ve yenisi çekilir. Kümeler: `near64` (Chebyshev ≤ 64 hücre), `far150` (≤ 150), `global` (sınırsız).
- Her kümede: bir `NavPathfinder` örneği (tüm sorgular için), sonuç nesnesi yeniden kullanılır; **ısınma:** ölçümden önce aynı kümeden 20 ek sorgu koşulur ve istatistiğe sayılmaz; her sorgu `std::chrono::steady_clock` ile ayrı süre ölçülür (yalnızca `Find` çağrısı); milisaniye cinsinden sıralanıp p50/p95/p99, `expanded` için p50/p95, durum sayıları.
- Yazdır (küme başına, **tam bu biçim**):

```
NAVPATH T-NAV-03 set=near64 queries=<Q> found=<n> nopath=<n> nodelimit=<n> expanded_p50=<n> expanded_p95=<n> ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
```

- `CHECK`'ler (her küme): `found + nopath + nodelimit == Q`; her sorguda `expanded ≤ 20000`; `Found` sonuçların hepsinde `cells.front()/back()` başlangıç/hedef ve ilk 50 sorgunun `PathIsValid`'i. **Yalnızca `near64`:** `found * 100 ≥ Q * 95` (Release'te `found ≥ 950`); ve **`#ifndef _DEBUG`** (Release; Debug'da `/MTd` `_DEBUG` tanımlar) `ms_p95 ≤ 2.0` (AC-NAV-02). Debug'da süre kapısı yoktur (satırı yine yazdır). `far150` ve `global` yalnızca raporlanır (kapı yok).

### 5.3 Proje dosyaları

1. `BotCore/BotCore.vcxproj`: `ClInclude` grubuna `<ClInclude Include="NavPath.h" />` (`NavGrid.h`'den sonra, `Perception.h`'den önce). BOM/CRLF korunur.
2. `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubuna `<ClCompile Include="NavPathTests.cpp" />` (`NavGridTests.cpp`'den sonra, `PerceptionTests.cpp`'den önce). BOM/CRLF korunur.
3. Bu iki projenin `.filters` dosyası yoktur (F5-01'de doğrulandı); ek dosya yok.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; `BotCore` ve `BotCoreTests` için **yeni uyarı yok** (Level 4; derleme çıktısında `NavPath` geçen `warning` satırı bulunmaz).
- [ ] K2: `./tools/run-tests.sh Release --no-build --list` çıktısı şu dokuz test adını içerir: `NavPath_Status_Validation`, `NavPath_OpenField_Octile`, `NavPath_NoCornerCutting`, `NavPath_NoPath_SlopeCut`, `NavPath_NodeLimit_Semantics`, `NavPath_MatchesDijkstra`, `NavPath_Deterministic_Reuse`, `NavPath_RealMap_Queries`, `NavPath_Perf_T_NAV_03`.
- [ ] K3: `build/nav/zone71.navgrid` varken (yoksa `python3 tools/nav-export.py` ile üret) `./tools/run-tests.sh Release --no-build` çıkış kodu 0; çıktıda tüm eski testler (Rng/Motion/Combat/Perception/Nav_*) ve dokuz yeni test `[ OK ]`; `SKIPPED` geçmiyor; `NAVPATH arena A->B: cost=660.…` satırı (660,117 ile 661,117 arası) ve üç `NAVPATH T-NAV-03 set=…` satırı var.
- [ ] K4: Gerçek harita yokken (`build/nav/zone71.navgrid` geçici olarak başka ada taşınarak) `./tools/run-tests.sh Release --no-build NavPath_` çıkış kodu 0, iki gerçek harita testinin her biri `SKIPPED` yazar ve diğer yedi test geçer; ardından dosya yerine geri konur.
- [ ] K5: `./tools/run-tests.sh Debug` (derleme dahil) çıkış kodu 0 (Debug'da `assert`/sınır hataları yok).
- [ ] K6: Optimallik: `NavPath_MatchesDijkstra` `[ OK ]` ve `NAVPATH dijkstra: pairs=240 found=<n>` satırında `n > 0`.
- [ ] K7: **T-NAV-03 / AC-NAV-02 (Release):** K3 çıktısındaki `set=near64` satırında `found ≥ 950` ve `ms_p95 ≤ 2.000`. Uygulayıcı Raporu üç satırı **aynen** yapıştırır. Kapı tutmuyorsa: önce makinenin yük altında olmadığını anlamak için testi üç kez koş (en kötü değeri raporla); tutarlı biçimde aşıyorsa kural/küme **gevşetilmez**: önce yalnızca `NavPath.h` içinde (§5.1 performans notları) iyileştir, hâlâ aşıyorsa §4'teki koşullu `NavGrid.h` eklemesini yap; o da yetmezse durup ölçümleri "Açık sorular"da raporla (plan `DÜZELTME GEREKLİ` olur).
- [ ] K8: Saflık/kapsam: `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavPath.h` çıktısı boş; `git diff --stat gece/2026-10-02-nav...bot/F5-02` yalnızca §4'teki dosyaları (ve kendi plan dosyasını) gösterir (`GameServer/`, `AIServer/`, `shared/`, `docs/` yok; `NavGrid.h` yalnızca K7'nin koşullu durumunda ve yalnızca eklemeyle).
- [ ] K9: F5-01 davranışı bozulmadı: `./tools/run-tests.sh Release --no-build Nav_` çıkış kodu 0; on `Nav_*` test `[ OK ]` ve `NAVGRID real map: n=513 main=88508 …` satırı var.

## 7. Doğrulama komutları

```bash
git switch -c bot/F5-02 gece/2026-10-02-nav
python3 tools/nav-export.py            # build/nav/zone71.navgrid yoksa
./tools/build.sh Release
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Release --no-build NavPath_
./tools/run-tests.sh Debug
grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavPath.h
git diff --stat gece/2026-10-02-nav...bot/F5-02
```

## 8. Kısıtlar ve uyarılar

- **Paralel hat:** sunucuya hiç dokunma (`tools/run-servers.sh` çağırma; ana hat başka çalışma ağacından sunucu çalıştırıyor olabilir). DB'ye bağlanma. Dal tabanı `gece/2026-10-02-nav`.
- Kodlama/satır sonu: `AGENTS.md` §3. Yeni C++ dosyaları ASCII + CRLF + tab + Allman. Yorumlar İngilizce; plan metnindeki Türkçe açıklama koda girmez.
- Başlık-yalnızca: tüm tanımlar sınıf içinde veya `inline`; `static` global durum yok. `#pragma` yalnızca `once` (MSVC Level 4 uyarılarını `static_cast` ile çöz, `#pragma warning` ekleme). `int`/`size_t` karışımı: indeks `x * n + z` en çok 263 169; işaret dönüşümlerini açık yap.
- Özyineleme yok (yol geri izleme döngüyle). Test içi Dijkstra'da da yığın taşıracak özyineleme yok.
- Bellek: `NavPathfinder` örneği ≈ 4,2 MB (`16 B × 263 169`); bot başına örnek yaratma (sonraki planlar paylaşımı belirler). Bu planda örnek sayısı sınırlama kodu yazma.
- **Eğim parametresi `[A]` kalır** (`NavParams::maxSlope = 0.625`, T-NAV-02): `NavPath` buna dokunmaz, ızgarayı olduğu gibi kullanır. Maliyet kısıtı: ağırlık/ceza terimi **ekleme**; ADR-0006 bunu F5-06'ya bırakır.
- `NodeLimit` "ulaşılamaz" **değildir**: hiçbir yerde `NodeLimit` için "unreachable" yazma; ulaşılamaz kararı F5-05'tir.
- Çalışma ortamı dışı performans gürültüsü: süre kapısı Release'te ve yalnızca `near64` kümesindedir; çıktıdaki `ms_*` değerleri makineye bağlıdır, rapora CPU bilgisini (ör. `nproc`) ekle.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-02` (taban: `gece/2026-10-02-nav`, açılış HEAD `4a69748`) — `a1df693 [F5-02] Izgara A* yol bulucu (BotCore/NavPath.h) ve NavPath testleri`; plan güncellemesi ayrı commit.
- Değişen dosyalar ve neden:
  - `BotCore/NavPath.h` (yeni): `NavCell`, `NavPathStatus`, `NavSearchParams`, `NavPathResult`, `NavOctile`, `NavPathfinder`; ikili yığın + tembel silme, damga tabanlı düğüm havuzu (sorgu başına temizleme yok), belirlenimli beraberlik (küçük `f`, büyük `g`, küçük `idx`), komşuluk yalnızca `NavGrid::EdgeOpen`; maliyet yalnızca yatay mesafe; `Generation` taşarsa damgalar sıfırlanır.
  - `Tests/BotCoreTests/NavPathTests.cpp` (yeni): §5.2'deki dokuz test; test içi bağımsız Dijkstra (`ReferenceCost`) ve `PathIsValid`; gerçek harita sorguları ve T-NAV-03 performans testi.
  - `BotCore/BotCore.vcxproj` (değişti): yalnızca `<ClInclude Include="NavPath.h" />` (BOM + CRLF korundu).
  - `Tests/BotCoreTests/BotCoreTests.vcxproj` (değişti): yalnızca `<ClCompile Include="NavPathTests.cpp" />` (BOM + CRLF korundu).
  - `plans/F5-02-nav-astar-yol-bulma.md` (değişti): `Durum` satırı + bu rapor.
- Derleme sonucu (`tools/build.sh Release` son satırlar; `NavPath` geçen uyarı yok):
  ```
  proj-GameServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\AIServer.exe
    NavPathTests.cpp
    BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ `./tools/build.sh Release` rc=0; tam çıktıda `warning` satırı yok.
  - K2 ✔ `--list` dokuz `NavPath_*` adını içeriyor.
  - K3 ✔ rc=0; `80 tests, 0 failed`; dokuz yeni test `[ OK ]`; `SKIPPED` yok; `NAVPATH arena A->B: cost=660.617 …` ve üç `NAVPATH T-NAV-03 set=…` satırı var.
  - K4 ✔ `zone71.navgrid` geçici taşınınca `NavPath_` rc=0; iki gerçek harita testi `SKIPPED` yazdı, yedi test geçti; dosya geri kondu.
  - K5 ✔ `./tools/run-tests.sh Debug` rc=0; `80 tests, 0 failed`; uyarı yok.
  - K6 ✔ `NavPath_MatchesDijkstra [ OK ]`; `pairs=240 found=238` (`n > 0`).
  - K7 ✔ Release `set=near64` `found=997` (≥ 950) ve `ms_p95=0.536` (≤ 2.000). Ölçüm makinesi: AMD Ryzen 7 7800X3D, `nproc=16`.
  - K8 ✔ `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavPath.h` boş; `git diff --stat` yalnızca §4'teki dört kod dosyası + plan; `GameServer/`, `AIServer/`, `shared/`, `docs/` ve `NavGrid.h` değişmedi.
  - K9 ✔ `Nav_` rc=0; on test `[ OK ]`; `NAVGRID real map: n=513 main=88508 …`.
- `NAVPATH` satırları (K3/K7, Release; aynen):
  ```
  NAVPATH arena A->B: cost=660.617 expanded=3160 cells=150
  NAVPATH T-NAV-03 set=near64 queries=1000 found=997 nopath=1 nodelimit=2 expanded_p50=306 expanded_p95=2431 ms_p50=0.075 ms_p95=0.536 ms_p99=0.981
  NAVPATH T-NAV-03 set=far150 queries=1000 found=997 nopath=1 nodelimit=2 expanded_p50=1454 expanded_p95=5990 ms_p50=0.374 ms_p95=1.547 ms_p99=2.587
  NAVPATH T-NAV-03 set=global queries=1000 found=943 nopath=1 nodelimit=56 expanded_p50=4758 expanded_p95=20000 ms_p50=1.309 ms_p95=5.532 ms_p99=6.130
  ```
- Plandan sapmalar ve gerekçeleri: `NavGrid.h`'e dokunulmadı (K7 kapısı ilk denemede tuttu). `NavOctile`/test içi mutlak değer `std::abs` yerine elle (koşullu işaret) hesaplandı; planın başlık listesine (`<cstdlib>` eklemeden) sadık kalmak için. Bunun dışında algoritma, testler ve proje kayıtları plana birebir uyar.
- Açık sorular: Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02-nav...bot/F5-02` @ `<sha>`
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
