# F5-05: Ulaşılamaz hedef tespiti (`BotCore/NavReach.h`: bileşen tablosu, yargı, bırakma süresi)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat, `docs/17` §1 "Paralel yürütülebilir işler") |
| Branch | `bot/F5-05` (taban: `gece/2026-10-02-nav`) |
| Bağımlı olduğu planlar | F5-01 (`BotCore/NavGrid.h`), F5-02 (`BotCore/NavPath.h`), F5-04 (`BotCore/NavTrack.h`): üçü `KAPANDI`, `gece/2026-10-02-nav` içinde (merge `788aa86`, `dc1bb10`, `0bddd38`) |
| İlgili gereksinim / kabul | `docs/12` §4.3 (bu plana göre güncellendi), §11 T-NAV-07 (altyapısı), AC-NAV-04 / MET-NAV-04 (bırakma ≤ 3 sn), AC-NAV-02 (süre bütçesi); ADR-0006 Eki F5-05 (bu planın kararları), ADR-0016 (`BotCore` saflığı) |
| Tahmini büyüklük | S (5 dosya: 2 yeni, 3 değişen; ~1 000 satır, çoğu test) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

F5-04'ün `NavFollower`'ı yalnızca bilgi taşır: `PathFailed` ve `pathStatus`. Bot ise erişilemeyen bir hedefi (göl cebi, duvarın öbür yanı, eğim cebi) ya da uzun dolambaçlı bir hedefi **bırakmalıdır** (`docs/12` §4.3, MET-NAV-04: tespitten bırakmaya ≤ 3 sn). Bu plan, bunun için saf mantığı yazar: `BotCore/NavReach.h`.

Üç parça:

1. `NavReach`: yürünebilir hücrelerin `EdgeOpen` ile bağlı bileşenleri (8 komşu, A*'ın kenar kuralı). Farklı bileşen = A* `NoPath`; bu artık A* çalıştırmadan bilinir. Neden gerekli: zone 71'de 15 hücrelik bir cebe A* 20 000 düğüm tüketip `NodeLimit` ("bilinmiyor") verir; bileşen karşılaştırması aynı soruyu kesin ve ücretsiz yanıtlar.
2. `NavReachJudge`: bir `NavFollowPlan` + bileşen tablosundan **yargı**: `Reachable` / `Unreachable` (`NoWalkable`, `Component`, `Detour`) / `Unknown` / `None`. Ulaşılamaz yalnızca kanıtlanabildiğinde söylenir.
3. `NavUnreachTracker`: `Unreachable` yargılarının **kesintisiz** serisini izler; seri `holdMs` = 1500 ms `[A]` sürünce `Due()` doğru olur (bırakma vadesi). Başka bir yargı seriyi sıfırlar.

`NavFollower` yalnızca bir alan kazanır: `NavFollowPlan::pathCost` (seçilen yolun A* maliyeti). Yeniden planlama davranışı değişmez.

Bu planın sonunda **hedefi gerçekten bırakma, `TARGET_UNREACHABLE` olayı, takıma bildirme, yeni hedef seçimi, tehlike/clearance maliyeti (F5-06), güvenli nokta, formasyon, takılma kurtarma, telemetri ve sunucu entegrasyonu yoktur.** `GameServer/`, `AIServer/`, `shared/` değişmez; sunucu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0006-navigasyon-izgara-astar.md` — **Eki F5-05** (bu plan yazılırken eklendi): bileşen tabanlı kesin tespit, halka tanımı, `Detour`, yargı sınıfları, 1,5 sn `[A]`, sorumluluk sınırı. Sapma gerekiyorsa dur ve sor.
- `docs/12_NAVIGATION_AND_POSITIONING.md` §4.3 (bu plana göre güncellendi), §11 (T-NAV-07, AC-NAV-04), `docs/16` §6.6 (MET-NAV-04).
- `BotCore/NavGrid.h` — kullanacağın API: `Size()`, `Unit()`, `CellOf(float)`, `CellCenter(int)`, `Walk(x, z)` (ızgara dışı ve `Build` öncesi `false`), `EdgeOpen(x, z, dx, dz)` (simetrik: `Walk` + eğim + köşe kesme yok), `Params()`. **Değiştirme.**
- `BotCore/NavPath.h` — `NavCell`, `NavPathStatus`, `NavSearchParams`, `NavPathfinder::Find` (testlerde). **Değiştirme.**
- `BotCore/NavTrack.h` — `NavFollowPlan` (`NavTrack.h:145-161`), `NavFollowParams` (`:120-132`), `NavFollowStatus`, `NavFollower`, `NavRingCells` (`:69-118`). **Yalnızca `NavFollowPlan::pathCost` eklenir** (§5.1).
- `BotCore/Rng.h` — testlerde `BotCore::Rng` (`NextBelow(uint32_t)`); global `rand()` yasak.
- `Tests/BotCoreTests/NavTrackTests.cpp` — **kopyalanacak kalıplar** (başka `.cpp`'den içe aktarma yok; yeni dosyada kendi kopyalarını yaz): `CellIndex`, `Cell`, `HeightZeros`, `RingEvents`, `WallColumn`, `MakeNav` (`:21-72`), `Dist`, `PercentileDouble`; gerçek harita yükleme + `SKIPPED` kalıbı (`:776-785`), `near64` çift çekimi ve `std::chrono::steady_clock` ölçümü (`NavTrack_Perf`, `:896-1020`).
- `Tests/BotCoreTests/MiniTest.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`; kayan nokta için `CHECK(std::fabs(a - b) <= eps)`), `tools/run-tests.sh` (testi depo kökünde çalıştırır).

Planı yazarken doğrulanan gerçekler (Python prototipi, 2026-10-02, depoya girmeyen geçici; **aynı algoritma ve kenar kuralı**; sayılar test beklentileridir; hepsi hücre merkezlerinde ve sınır dışı değerlerle seçildi, kayan nokta sınır durumu yok). "N×N halka" = `RingEvents(N)` (kenar engelli, iç açık), `unit = 4`, düz zemin; hücre `(x, z)` merkezi dünya `((x + 0.5) * 4, (z + 0.5) * 4)`. Bileşen kimlikleri: tarama sırası (`x` artan, her `x` için `z` artan) ile bulunan ilk hücreye göre.

- **Blok haritası (20×20 halka):** `x ∈ [10, 14]`, `z ∈ [8, 12]` hücrelerinin yüksekliği 20 m (diğerleri 0). Tüm hücreler `Walk`, blok kenarlarından eğimle kapalı. Bileşenler: **2**; dış alan 299 hücre (kimlik 0, ilk hücre `(1, 1)`), blok 25 hücre (kimlik 1, ilk hücre `(10, 8)`). Bot `(22, 22)` = hücre `(5, 5)`, hedef `(50, 42)` = hücre `(12, 10)` (blok merkezi). A* `(5,5) → (12,10)`: `NoPath`, 299 düğüm (F5-04 plan §5.2 ile aynı harita). Halka `[4, 9]` merkezinde: **20** aday, hepsi blok içinde (`a² + b² ∈ [1, 5]` ofsetleri, `|a|,|b| ≤ 2`). Halka `[0, 0]`: tek aday `(12, 10)`. Halka `[0, 13]` (`a² + b² ≤ 10`): **37** aday, 25'i blok içinde, **12**'si dışında (`(±3, 0)`, `(0, ±3)`, `(±3, ±1)`, `(±1, ±3)` ofsetleri); bot `(5, 5)` için en yakın aday dışarıda (`Planned`).
- **Düz 40×40:** 1 bileşen, 1444 hücre. Bot `(22, 22)` (hücre `(5, 5)`), hedef `(82, 22)` (hücre `(20, 5)`), halka `[0, 0]`: A* maliyeti **60,0**, 16 düğüm; `maxNodes = 5` ile `NodeLimit` (5 düğüm).
- **Duvar sütunu 30×30** (`x = 15`, `z ∈ [1, 28]` engelli): sol yarı `Walk` (14 × 28 = **392** hücre, tek bileşen), sağ yarı `Walk` değil; hedef `(90, 42)` (hücre `(22, 10)`) halka `[0, 0]` → aday yok (`NoGoal`).
- **Aralıklı duvar 60×60** (`x = 30`, `z ∈ [1, 56]` engelli, `z ∈ [57, 58]` açık; `Walk` 3308 hücre, tek bileşen): `(28, 5) → (32, 5)` maliyet **427,314** (1604 düğüm), düz 16,0 (oran 26,7) → `Detour`; `(28, 40) → (32, 40)` maliyet **147,314** (oran 9,2) → `Detour`; `(28, 50) → (32, 50)` maliyet **67,314** (oran 4,2 ama < 120 m) → `Reachable`.
- **Rastgele küçük haritalar** (24×24 halka, her iç hücre %8 ihtimalle engelli, %20 ihtimalle yükseklik 6 m): 30 tohumun 30'unda ≥ 2 bileşen; 4500 rastgele çiftin 1618'i farklı bileşende (bu sayılar Python `random` ile; C++ `Rng` aynı dağılımı farklı örneklerle verir, test eşikleri geniş: §5.2).
- **Gerçek harita (zone 71):** 88 508 `Walk` hücre **143 bileşene** ayrılır: 1 ana bileşen **88 279** hücre + 142 cep (toplam **229** hücre). En büyük on iki bileşen boyutları: 88279, 15, 11, 10, 9, 6, 6, 5, 4, 3, 3, 3. Arena A hücresi `(318, 222)` (merkez `(1274, 890)`) ve B `(186, 276)` ana bileşende (kimlik 0, ilk hücre `(15, 104)`). Cep P = hücre `(173, 211)` (merkez `(694, 846)`; kimlik 51, 15 hücre). A* `A → P`: **`NodeLimit`**, 20 000 düğüm (varsayılan sınır); `P → A`: **`NoPath`**, **15** düğüm. Halka `[0, 0]` P merkezinde: tek aday `(173, 211)`. Halka `[30, 45]` P merkezinde: **168** aday, **162**'si ana bileşende (bota en yakın üçü `(183,216), (184,213), (183,215)`, hepsi `Found`). Dolambaç çifti `S = (218, 206)` (merkez `(874, 826)`) → `G = (221, 211)` (merkez `(886, 846)`): ikisi ana bileşende, A* **Found**, maliyet **242,108**, 1556 düğüm, düz mesafe **23,324** (oran 10,4), halka `[0, 0]` tek aday. `near64` çiftlerinin (60 kaynak, 523 629 çift) **%0,78**'i (4 102) `Detour` kuralına (oran > 3 ve > 120 m) takılır.
- Bileşen kuruluşu Python'da yaklaşık 0,8 sn (tüm hazırlık dahil); C++'ta ≈ 10–20 ms beklenir (kapı 100 ms, §6 K7).

## 3. Kapsam

**Yapılacaklar**

- `BotCore/NavReach.h`: `NavReach`, `NavReachVerdict`, `NavUnreachReason`, `NavUnreachParams`, `NavReachJudgement`, `NavReachJudge`, `NavUnreachTracker` (§5.1).
- `BotCore/NavTrack.h`: yalnızca `NavFollowPlan::pathCost` alanı ve doldurulması (§5.1).
- `Tests/BotCoreTests/NavReachTests.cpp`: §5.2 test listesi (birim + simülasyon + gerçek harita + süre).
- `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` kayıtları (§5.3).

**Kapsam dışı (yapılmayacak)**

- `NavGrid.h`/`NavPath.h`/`NavSmooth.h` değişikliği; `NavTrack.h`'de `pathCost` dışında her şey (özellikle `NavFollower` davranışı, parametreler, başka bileşendeki halka adayları için A*'ı atlama; ADR Eki F5-05 madde 7).
- Hedefi gerçekten bırakma, `TARGET_UNREACHABLE` olayı/telemetrisi, takıma bildirme, yeni hedef seçimi, `docs/09` §5.4'teki `R(t) < 0,25` takım düzeyi koşulu (F6/F7, sunucu entegrasyonu).
- Hiyerarşik arama / bölge grafiği, `NodeLimit`'i "ulaşılamaz"a çevirmek (bilinçli olarak `Unknown` kalır).
- Dinamik/yumuşak engeller (tehlike, kule halkası: F5-06), güvenli nokta (F5-07), formasyon (F5-08), takılma kurtarma (F5-09), LoS (F5-10).
- Bileşen tablosunun bayatlığını izleme (çağıran, `NavGrid::Build`'den sonra `NavReach::Build` çağırır), çok iş parçacığı, `NavReach`'i `NavFollower`'a bağlama.
- Rol parametre dosyası okuyucusu; `NavUnreachParams` varsayılanları ve testteki sabitler yeterli.
- `GameServer/`, `AIServer/`, `shared/`, `docs/` değişikliği, DB, sunucu çalıştırma. `GameServer/proj-GameServer.vcxproj` değişmez.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavReach.h` | yeni | ASCII, CRLF, başlık-yalnızca (satır içi) |
| `BotCore/NavTrack.h` | değiştir | yalnızca `NavFollowPlan::pathCost` alanı + iki satır (§5.1); CRLF korunur |
| `Tests/BotCoreTests/NavReachTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="NavReach.h" />` satırı (BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="NavReachTests.cpp" />` satırı (BOM ve CRLF korunur) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`build/nav/zone71.navgrid` üretilen çıktıdır, commit edilmez; yoksa `python3 tools/nav-export.py` ile üret.)

## 5. Uygulama adımları

### 5.1 `BotCore/NavReach.h` ve `NavFollowPlan::pathCost`

**`BotCore/NavTrack.h` değişikliği (üç yer, başka hiçbir şey):**

1. `NavFollowPlan` içinde `int expanded = 0;` satırından (`NavTrack.h:156`) sonra: `float pathCost = 0.0f;         // A* cost (metres) of the chosen route; meaningful when Planned`.
2. `NavFollower::Update` içinde `m_plan.expanded = 0;` (`:356`) satırından sonra `m_plan.pathCost = 0.0f;`.
3. Aynı fonksiyonda `m_plan.goal = m_candidates[static_cast<size_t>(i)];` (`:382`) satırından sonra `m_plan.pathCost = m_path.cost;`.

Mevcut `NavTrack_*` testleri değişmez ve geçmeye devam eder (K9).

**`BotCore/NavReach.h`:** Ad alanı `BotCore`; `#include "NavTrack.h"` (aynı dizin, tırnaklı; `NavSmooth.h`/`NavPath.h`/`NavGrid.h`'yi o getirir) ve yalnızca standart başlıklar (`<cmath> <cstddef> <cstdint> <vector>`). Stil `NavTrack.h` gibi: tab girinti, Allman, İngilizce yorum, `inline`/sınıf içi tanımlar, global/`static` durum yok. Zaman, çağıranın verdiği tek yönlü `int64_t` milisaniye (`BotCore` saat okumaz).

```cpp
namespace BotCore
{
	// Edge-connected components of Walk cells (NavGrid::EdgeOpen, 8 neighbours). Two cells are in
	// different components exactly when NavPathfinder reports NoPath between them.
	class NavReach
	{
	public:
		// Labels the components of `grid` (iterative, uses an explicit stack). Ids are assigned in
		// scan order (x ascending, then z ascending) of each component's first cell. Replaces any
		// previous result. A grid that was not Init'ed/Built yields Size() == 0 / no components.
		// Must be called again after NavGrid::Build with other params.
		void Build(const NavGrid & grid);

		int Size() const;                        // grid side at Build time (0 before)
		int ComponentCount() const;              // 0 when there is no Walk cell
		int ComponentCells(int id) const;        // 0 for an invalid id
		int LargestComponent() const;            // id of the largest (first on ties); -1 if none
		// -1 for off-grid, non-Walk cells and before Build.
		int ComponentOf(int x, int z) const;
		// True when both cells are Walk and in the same component.
		bool Connected(NavCell a, NavCell b) const;
	};

	enum class NavReachVerdict
	{
		None,           // no plan yet (NavFollowStatus::NoTarget)
		Reachable,      // planned, no detour
		Unknown,        // not provable: A* node limit / first tries failed while a ring cell is connected, or the bot's own cell is not Walk
		Unreachable     // provable (see NavUnreachReason)
	};

	enum class NavUnreachReason
	{
		None,           // verdict != Unreachable
		NoWalkable,     // the ring holds no Walk cell (NavFollowStatus::NoGoal)
		Component,      // no ring cell shares the bot's component
		Detour          // planned, but A* cost > detourFactor * straight and > detourMinM
	};

	struct NavUnreachParams
	{
		float detourFactor = 3.0f;   // docs/12 s4.3 [Ö]; <= 0 disables the Detour rule
		float detourMinM = 120.0f;   // docs/12 s4.3 [Ö]
		int   holdMs = 1500;         // [A] ADR-0006 Ek F5-05; MET-NAV-04 budget is 3000 ms
	};

	struct NavReachJudgement
	{
		NavReachVerdict verdict = NavReachVerdict::None;
		NavUnreachReason reason = NavUnreachReason::None;
		int   ringCells = 0;       // Walk cells in the plan's ring (0 when not computed)
		int   ringConnected = 0;   // of those, in the bot's component
		float straightM = 0.0f;    // Planned only: bot cell centre -> goal cell centre
		float pathM = 0.0f;        // Planned only: NavFollowPlan::pathCost
	};

	class NavReachJudge
	{
	public:
		// `plan` and `follow` must be the plan just returned by NavFollower::Update and the
		// params it was called with; botX/botZ the bot position of that call (metres).
		NavReachJudgement Judge(const NavGrid & grid, const NavReach & reach, const NavFollowPlan & plan,
			const NavFollowParams & follow, const NavUnreachParams & params, float botX, float botZ);
	private:
		std::vector<NavCell> m_ring;   // reused, no per-call allocation after warm-up
	};

	class NavUnreachTracker
	{
	public:
		void Reset();
		// Feed one judgement (call whenever NavFollower::Update returned true). Unreachable starts
		// (or continues) the streak; every other verdict (Reachable, Unknown, None) clears it.
		void Feed(int64_t nowMs, const NavReachJudgement & judgement);
		bool Active() const;                 // an Unreachable streak is running
		int64_t SinceMs() const;             // time of the first judgement of the streak; 0 when !Active()
		NavUnreachReason Reason() const;     // reason of the latest Unreachable judgement; None when !Active()
		// Active() && nowMs - SinceMs() >= holdMs (holdMs <= 0: due as soon as Active()).
		bool Due(int64_t nowMs, int holdMs) const;
	};
}
```

**`NavReach::Build` kuralı (kesin):** `n = grid.Size()`; `n < 2` ise tüm durumu temizle (`Size() == 0`) ve dön. Aksi halde `n × n` `int32_t` kimlik dizisini `-1` ile doldur (yeniden kullanılabilir bellek: `assign`). `x` artan, `z` artan taramada `grid.Walk(x, z)` ve kimliği `-1` olan her hücre yeni bileşen başlatır (kimlik = o ana kadarki bileşen sayısı); yığın (`std::vector<int32_t>` üye) ile 8 komşuya `grid.EdgeOpen(cx, cz, dx, dz)` doğruysa ve komşunun kimliği `-1` ise etiketle. Bileşen boyutları `m_sizes`'ta. `ComponentOf` ızgara dışı (`x`/`z` ∉ `[0, Size())`) ve `-1` kimlik için `-1` döner; `Connected(a, b)` her iki `ComponentOf` ≥ 0 ve eşit.

**`NavReachJudge::Judge` kuralı (kesin, bu sırayla; her biri §5.2'deki testle sabitlenir):**

1. `plan.status == NoTarget` → `{None}` (tüm alanlar varsayılan).
2. `botCell = (grid.CellOf(botX), grid.CellOf(botZ))`; `botComp = reach.ComponentOf(botCell.x, botCell.z)`. `plan.status == InvalidStart` ya da `botComp < 0` → `Unknown` (neden `None`, halka sayıları 0).
3. `plan.status == NoGoal` → `Unreachable` / `NoWalkable` (halka sayıları 0).
4. Halka: `NavRingCells(grid, plan.predX, plan.predZ, follow.ringMinM, follow.ringMaxM, botCell, m_ring)`; `ringCells = m_ring.size()`, `ringConnected` = `ComponentOf(c.x, c.z) == botComp` olanların sayısı. `ringCells == 0` ise (parametreler plana uymuyor) `Unreachable` / `NoWalkable`. `ringConnected == 0` ise `Unreachable` / `Component`.
5. `plan.status == Planned`: `straightM` = `CellCenter(botCell)` ile `CellCenter(plan.goal)` arası Öklid (`float`), `pathM = plan.pathCost`. `params.detourFactor > 0` ve `pathM > params.detourFactor * straightM` ve `pathM > params.detourMinM` ise `Unreachable` / `Detour`; değilse `Reachable`.
6. `plan.status == PathFailed` (botun bileşeninde halka adayı var: sınır/ilk denemeler) → `Unknown`.

**`NavUnreachTracker`:** `Feed`: `verdict == Unreachable` ise `!active` ise `active = true`, `since = nowMs`; her durumda `reason = judgement.reason`. Başka bir yargıda `active = false`, `since = 0`, `reason = None`. `Due(now, holdMs)`: `active && now - since >= holdMs`.

MSVC Level 4 uyarılarını `static_cast` ile çöz (`#pragma warning` ekleme). Özyineleme yok. `NavReachJudge` ve `NavReach` kopyalanabilir ama paylaşılan durum yoktur; iş parçacığı güvenli değildir (örnek başına bir bağlam).

### 5.2 `Tests/BotCoreTests/NavReachTests.cpp`

`#include "MiniTest.h"`, `<BotCore/NavGrid.h>`, `<BotCore/NavPath.h>`, `<BotCore/NavSmooth.h>`, `<BotCore/NavTrack.h>`, `<BotCore/NavReach.h>`, `<BotCore/Rng.h>` (include biçimi `NavTrackTests.cpp` gibi). Anonim ad alanında yardımcılar (kendi kopyaların): `CellIndex`, `Cell`, `HeightZeros`, `RingEvents`, `WallColumn`, `MakeNav`, `Dist`, `PercentileDouble`; ayrıca `GapWall(n, x, gapLo, gapHi)` (`RingEvents`'e `x` sütununda `z ∈ [1, n-2]`, `gapLo..gapHi` dışında engelli ekler), `BlockHeights(n)` (yüksekliği 20 olan `x ∈ [10, 14]`, `z ∈ [8, 12]` bloğu, diğerleri 0; `n = 20` için), `Center(grid, cell, &x, &z)` (= `CellCenter`), `Judgement(...)` yardımcıları gerekirse. Her testte `NavPathfinder pf;`, `NavFollower f;`, `NavReach reach;`, `NavReachJudge judge;`, `NavFollowParams fp;` (varsayılan, aksi belirtilmedikçe), `NavUnreachParams up;` kullanılır; bot hızı **0** (öngörü kapalı: yalnızca tek gözlem ya da hız 0'la `Update`; aksi belirtilmedikçe hedef bir kez `ObserveTarget(0, ...)` ile gözlenir). Her `Update` çağrısından sonra `Judge` çağrılır; dönüş alanları `CHECK`/`CHECK_EQ` ile doğrulanır.

| Test adı | İçerik |
|---|---|
| `NavReach_Components_Small` | (a) **Build öncesi:** `NavReach r;` `Size() == 0`, `ComponentCount() == 0`, `ComponentOf(5, 5) == -1`, `LargestComponent() == -1`, `ComponentCells(0) == 0`, `!Connected(Cell(5,5), Cell(5,5))`. (b) **Düz 40×40:** `Size() == 40`, `ComponentCount() == 1`, `ComponentCells(0) == 1444`, `LargestComponent() == 0`, `ComponentOf(5, 5) == 0`, `ComponentOf(0, 0) == -1`, `ComponentOf(-1, 5) == -1`, `ComponentOf(40, 5) == -1`, `ComponentOf(5, 40) == -1`; `Connected((1,1), (38,38))` doğru; `Connected((5,5), (5,5))` doğru; `Connected((0,0), (0,0))` yanlış. (c) **Blok 20×20:** `ComponentCount() == 2`, `ComponentOf(5, 5) == 0`, `ComponentOf(12, 10) == 1`, `ComponentCells(0) == 299`, `ComponentCells(1) == 25`, `ComponentCells(2) == 0`, `ComponentCells(-1) == 0`, `LargestComponent() == 0`; `Connected((5,5), (12,10))` yanlış (iki yönde), `Connected((12,10), (10,8))` doğru, `Connected((5,5), (18,18))` doğru. (d) **Izgara parametresini izler:** aynı blok ızgarasında `NavParams p; p.maxSlope = 100.0f; grid.Build(p);` ardından `r.Build(grid)` → `ComponentCount() == 1`, `ComponentCells(0) == 324`; `grid.Build();` ardından `r.Build(grid)` → yine 2. (e) **Yeniden kurulum:** `r` blok ızgarasıyla kurulduktan sonra düz 40×40 ile `Build` → `Size() == 40`, `ComponentCount() == 1`. (f) **Duvar sütunu 30×30** (`WallColumn(30, 15)`): `ComponentCount() == 1`, `ComponentCells(0) == 392`, `ComponentOf(22, 10) == -1`. |
| `NavReach_Matches_AStar` | Mülkiyet testi: `Connected` ⇔ A* `Found`. 30 tohum (`seed = 0..29`), `Rng rng(1000 + seed)`, `n = 24`: `RingEvents(24)` üzerinde her iç hücre için `rng.NextBelow(100) < 8` ise olay 0; yükseklik `rng.NextBelow(5) == 0 ? 6.0f : 0.0f` (iki çağrı hücre başına sabit sırada: önce olay, sonra yükseklik, `x` artan `z` artan). `MakeNav`, `reach.Build(grid)`; `Walk` hücreleri listesi < 2 ise tohum atlanır. Bölüntü: her `Walk` hücrenin kimliği `[0, ComponentCount())` içinde, `Walk` olmayan hücrelerin `-1`; `ComponentCells(id)` toplamı `Walk` hücre sayısına eşit. 150 çift (`rng.NextBelow(size)` ile iki hücre): `pf.Find(grid, a, b, params, out)` (`params.maxNodes = 100000`); `out.status` yalnızca `Found` ya da `NoPath`; `(out.status == Found) == reach.Connected(a, b)`. Sayaçlar: ≥ 2 bileşenli tohum `multi`, farklı bileşendeki çift `apart`, aynı bileşendeki çift `same`. `REQUIRE(multi >= 15)`, `REQUIRE(apart >= 300)`, `REQUIRE(same >= 300)` (prototip: 30, 1618, 2882). Yazdır: `NAVREACH random maps: seeds=30 multi=<n> pairs=4500 apart=<n> same=<n>`. |
| `NavReach_Judge_Basic` | (a) **`None`:** yeni `f`, `Update` çağrılmadan `judge.Judge(grid, reach, f.Plan(), fp, up, 22, 22)` → `verdict == None`, `reason == None`, `ringCells == 0`, `ringConnected == 0`. (b) **`Reachable`:** 40×40 halka, `ObserveTarget(0, 82, 22)`, bot `(22, 22)`, halka `[0, 0]`: `Update(0)` `true`, `Planned`, `|plan.pathCost - 60| <= 1e-3`; yargı `Reachable`, `reason None`, `ringCells == 1`, `ringConnected == 1`, `straightM` ve `pathM` 60,0 (`1e-3`). (c) **`NoWalkable`:** 30×30 `WallColumn(30, 15)`, hedef `(90, 42)`, bot `(22, 22)`, halka `[0, 0]`: `Planned` olmayan `NoGoal` (`REQUIRE(plan.status == NoGoal)`), yargı `Unreachable` / `NoWalkable`, `ringCells == 0`. (d) **`Component`, halka `[4, 9]`:** 20×20 blok (`BlockHeights`), hedef `(50, 42)`, bot `(22, 22)`: `REQUIRE(plan.status == PathFailed)`, `pathStatus == NoPath`; yargı `Unreachable` / `Component`, `ringCells == 20`, `ringConnected == 0`. Halka `[0, 0]`: `PathFailed`, yargı `Unreachable` / `Component`, `ringCells == 1`, `ringConnected == 0`. (e) **Bot blokun içinde:** bot `(50, 42)`, hedef `(22, 22)`, halka `[0, 0]`: `PathFailed` / `NoPath`; yargı `Unreachable` / `Component`, `ringCells == 1`, `ringConnected == 0`. (f) **Karışık halka `Reachable`:** blok haritası, hedef `(50, 42)`, bot `(22, 22)`, halka `[0, 13]`: `Planned`, `goal` blok dışında (`!(10 <= goal.x && goal.x <= 14 && 8 <= goal.z && goal.z <= 12)`); yargı `Reachable`, `ringCells == 37`, `ringConnected == 12`. (g) **`Unknown` (A* sınırı):** 40×40 halka, hedef `(82, 22)`, bot `(22, 22)`, halka `[0, 0]`, `fp.search.maxNodes = 5`: `REQUIRE(plan.status == PathFailed)`, `pathStatus == NodeLimit`; yargı `Unknown`, `reason None`, `ringCells == 1`, `ringConnected == 1`. (h) **`Unknown` (bot yürünemez):** 40×40 halka, hedef `(82, 22)`, bot `(2, 2)` (hücre `(0, 0)`) ve bot ızgara dışı `(-20, 22)`: `REQUIRE(plan.status == InvalidStart)`, yargı `Unknown`, `ringCells == 0`. (i) `NavUnreachParams` varsayılanları: `detourFactor == 3.0f`, `detourMinM == 120.0f`, `holdMs == 1500`. |
| `NavReach_Judge_Detour` | 60×60 `GapWall(60, 30, 57, 58)`; `REQUIRE(grid.Walk(30, 57))`, `REQUIRE(grid.Walk(28, 5))`, `REQUIRE(grid.Walk(32, 5))`; halka `[0, 0]`. (a) Bot `(28, 5)` merkezi, hedef `(32, 5)` merkezi: `Planned`, `|plan.pathCost - 427.314| <= 0.05`; yargı `Unreachable` / `Detour`, `straightM` 16,0 (`1e-3`), `pathM` = `pathCost`, `ringCells == 1`, `ringConnected == 1`. (b) `(28, 40) → (32, 40)`: `pathCost` 147,314 (`0.05`), `Unreachable` / `Detour`. (c) `(28, 50) → (32, 50)`: `pathCost` 67,314 (`0.05`), oran > 3 ama < 120 m → `Reachable`. (d) Sınırlar: (c)'de `up.detourMinM = 60` → `Detour`; (a)'da `up.detourFactor = 30` → `Reachable` (oran 26,7); (a)'da `up.detourFactor = 0` ve `-1` → `Reachable`; (a)'da `up.detourMinM = 500` → `Reachable`. |
| `NavReach_Tracker` | `NavReachJudgement U(r)` (verdict `Unreachable`, reason `r`), `R` (`Reachable`), `K` (`Unknown`), `N` (`None`) yardımcıları. Yeni izleyici: `!Active()`, `SinceMs() == 0`, `Reason() == None`, `!Due(100000, 0)`. `Feed(1000, U(Component))`: `Active()`, `SinceMs() == 1000`, `Reason() == Component`; `Due(2499, 1500)` yanlış, `Due(2500, 1500)` doğru, `Due(1000, 0)` doğru, `Due(999, 1500)` yanlış. `Feed(1200, U(NoWalkable))`: `SinceMs()` hâlâ 1000, `Reason() == NoWalkable`. `Feed(1300, K)`: `!Active()`, `SinceMs() == 0`, `Reason() == None`, `!Due(100000, 0)`. `Feed(1400, U(Detour))`: `SinceMs() == 1400`; `Feed(1500, R)` → temiz; `Feed(1600, N)` → temiz kalır; `Feed(1700, U(Component))`; `Reset()` → `!Active()`. |
| `NavReach_Drop_Sim` | **Test-yerel** simülasyon: 20×20 blok haritası (BotCore'da yok), `fp.ringMinM = 4`, `fp.ringMaxM = 9`, bot `(22, 22)` sabit, bot hızı 0, tick 100 ms, `t = 0 .. 4000`. Her tick: gözlem (senaryoya göre) → `if (f.Update(grid, pf, t, 22, 22, 0.0f, fp)) tracker.Feed(t, judge.Judge(...))` → `tracker.Due(t, up.holdMs)` ilk doğru olduğunda `drop_ms = t`, döngü biter. **Senaryo A:** hedef `(50, 42)` bir kez `t = 0`'da gözlenir. Beklenen: yeniden planlamalar `t = 0, 500, 1000, 1500` (4 `Feed`), `drop_ms == 1500`; `t < 1500` hiçbir tick'te `Due` yok; `drop_ms - ilk Unreachable yargı zamanı (0) == 1500 <= 3000` (MET-NAV-04). **Senaryo B (seri sıfırlanır):** hedef `t = 0`'da `(50, 42)`, `t = 700`'de `(30, 22)` (yürünebilir), `t = 1500`'de yine `(50, 42)` gözlenir (her gözlem ilgili tick'in `Update`'inden önce). Beklenen: `t = 0` ve `500` `Unreachable`, `t = 700` (`Moved`) ve `t = 1200` (`Interval`) `Reachable` (seri sıfırlanır), `t = 1500` (`Moved`) `Unreachable` (yeni seri `SinceMs() == 1500`), `t = 2000, 2500, 3000` `Unreachable`; `drop_ms == 3000`; `t < 3000` hiçbir tick'te `Due` yok (özellikle `t = 1500..2999`). Yazdır (tam biçim): `NAVREACH drop A: detect_ms=0 drop_ms=<n> feeds=<n>` ve `NAVREACH drop B: detect_ms=1500 drop_ms=<n> feeds=<n>`. |
| `NavReach_RealMap` | Gerçek harita (aşağıya bak). |
| `NavReach_Perf` | Gerçek harita (aşağıya bak): `Judge` süresi. |

**Gerçek harita ortak kuralı** (F5-02..F5-04 ile aynı): dosya yolu `build/nav/zone71.navgrid` (çalışma dizini depo kökü); açılamazsa testi **başarısız yapma**: `std::printf("NAVREACH real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n")` yaz ve dön. `LoadFile` + `Build()` (varsayılan `NavParams`) sonrası `MainComponentCells() == 88508` `REQUIRE` edilir.

`NavReach_RealMap`:

- `reach.Build(grid)` `std::chrono::steady_clock` ile süresi ölçülür (`build_ms`). `CHECK_EQ(reach.ComponentCount(), 143)`; `reach.ComponentCells(reach.LargestComponent()) == 88279`; tüm bileşen boyutlarının toplamı `88508` (bölüntü), ana bileşen dışındakilerin toplamı `229`; boyutlar azalan sıralıysa ilk on ikisi tam `88279, 15, 11, 10, 9, 6, 6, 5, 4, 3, 3, 3`. A = `(318, 222)`, B = `(186, 276)`, P = `(173, 211)`, S = `(218, 206)`, G = `(221, 211)`: hepsi `Walk` (`REQUIRE`); `ComponentOf(A) == reach.LargestComponent()`, `ComponentOf(A) == 0`, `Connected(A, B)`, `Connected(S, G)`, `!Connected(A, P)`, `ComponentCells(ComponentOf(P)) == 15`, `ComponentOf(P) == 51`. **Release'te** `build_ms <= 100.0` `[A]` (Debug'da kapı yok).
- **Cep hedefi, halka `[0, 0]`:** bot A merkezi, hedef P merkezi `(CellCenter(173), CellCenter(211))`, bot hızı 0: `Update(0)` `true`; `status == PathFailed`, `pathStatus == NodeLimit`, `tries == 1`, `expanded == 20000`; yargı `Unreachable` / `Component`, `ringCells == 1`, `ringConnected == 0`.
- **Bot cepte:** bot P merkezi, hedef A merkezi, halka `[0, 0]`: `PathFailed`, `pathStatus == NoPath`, `expanded == 15`, yargı `Unreachable` / `Component`, `ringCells == 1`, `ringConnected == 0`.
- **Mage halkası `[30, 45]`, bot A, hedef P merkezi:** `Planned` (`REQUIRE`), yargı `Reachable`, `ringCells == 168`, `ringConnected == 162`.
- **Dolambaç çifti:** bot S merkezi, hedef G merkezi, halka `[0, 0]`: `Planned` (`REQUIRE`), `|plan.pathCost - 242.108| <= 0.1`; yargı `Unreachable` / `Detour`, `straightM` 23,324 (`0.01`), `pathM > 3 * straightM`, `pathM > 120`.
- **Kontrol (A → B):** bot A merkezi, hedef B merkezi, halka `[0, 0]`: `Planned`, yargı `Reachable` (`pathM / straightM < 1.5`).
- Yazdır: `std::printf("NAVREACH real: components=%d largest=%d pockets=%d build_ms=%.2f; pocket NodeLimit->Component, in-pocket NoPath->Component; mage ring=%d connected=%d; detour path=%.3f straight=%.2f\n", ...)`.
- Not (planın doğruladığı sayı tutmazsa): sayı ya da bileşen kimliği beklentiden saparsa **kuralı sayıya uydurma**; dur ve Uygulayıcı Raporu'nda sor.

`NavReach_Perf`:

- Hazırlık: `NavReach` bir kez kurulur (süre ölçülmez). `Walk` hücrelerini x-ana taramada tek `std::vector<BotCore::NavCell>`'e topla (88 508 eleman). Sorgu sayısı `Q` **Release'te 1000, Debug'da 100** (`#ifdef _DEBUG`). Çiftler `Rng(20261002)`'den `NextBelow(count)` ile çekilir; başlangıç == hedef ise ya da Chebyshev mesafesi 64'ü aşıyorsa çift reddedilir ve yenisi çekilir (`near64`, F5-02..F5-04'teki kümenin aynısı). Bir `NavPathfinder`, bir `NavFollower`, bir `NavReachJudge` yeniden kullanılır.
- Her çift için iki küme (aynı çiftler): **`exact`** (`ringMinM = ringMaxM = 0`) ve **`mage`** (`[30, 45]`). Her çiftte: `follower.Reset()`, `ObserveTarget(0, hedef hücre merkezi)`, `Update(0, ...)` (**ölçülmez**; bot = başlangıç hücre merkezi, hız 0), ardından **yalnızca `Judge` çağrısı** `std::chrono::steady_clock` ile ölçülür. **Isınma:** ölçümden önce aynı kümeden 20 ek çift koşulur, istatistiğe sayılmaz.
- İstatistik (küme başına): `unreachable` (yargı `Unreachable` olanlar), `detour` (neden `Detour` olanlar), süre p50/p95/p99 (ms).
- Yazdır (**tam bu biçim**, küme başına bir satır):

```
NAVREACH perf set=exact judged=<n> unreachable=<n> detour=<n> ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
NAVREACH perf set=mage judged=<n> unreachable=<n> detour=<n> ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
```

- `CHECK`'ler (her çift için, tüm çiftler): `plan.status == Planned` ise yargı `Reachable` ya da `Unreachable`/`Detour` (asla `Component`/`NoWalkable`/`Unknown`; `ringConnected >= 1`); `plan.status == PathFailed` ise `ringConnected == 0` ⇔ yargı `Unreachable`/`Component`, `ringConnected >= 1` ⇔ yargı `Unknown`; `exact` kümesinde `PathFailed` ve `pathStatus == NoPath` ise yargı `Unreachable`/`Component` (tek aday; A* `NoPath` ⇔ farklı bileşen); küme başına `judged == Q`; `unreachable * 100 <= Q * 5` ve `detour * 100 <= Q * 5` (prototip ≈ %0,8). **`#ifndef _DEBUG`** (Release): her kümede **`ms_p95 <= 0.5`** `[A]` (halka yeniden üretimi + tablo bakışı; A* süresi dahil değil). Debug'da süre kapısı yoktur (satırları yine yazdır).

### 5.3 Proje dosyaları

1. `BotCore/BotCore.vcxproj`: `ClInclude` grubuna `<ClInclude Include="NavReach.h" />` (`NavTrack.h` satırından sonra, `Perception.h` satırından önce; F5-04 `NavTrack.h` satırını `:77`'ye koydu). BOM/CRLF korunur.
2. `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubuna `<ClCompile Include="NavReachTests.cpp" />` (`NavTrackTests.cpp` satırından sonra, `PerceptionTests.cpp` satırından önce; F5-04 `NavTrackTests.cpp` satırını `:85`'e koydu). BOM/CRLF korunur.
3. Bu iki projenin `.filters` dosyası yoktur (F5-01'de doğrulandı); ek dosya yok.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; `BotCore` ve `BotCoreTests` için **yeni uyarı yok** (Level 4; derleme çıktısında `NavReach` ya da `NavTrack` geçen `warning` satırı bulunmaz).
- [ ] K2: `./tools/run-tests.sh Release --no-build --list` çıktısı şu sekiz test adını içerir: `NavReach_Components_Small`, `NavReach_Matches_AStar`, `NavReach_Judge_Basic`, `NavReach_Judge_Detour`, `NavReach_Tracker`, `NavReach_Drop_Sim`, `NavReach_RealMap`, `NavReach_Perf`.
- [ ] K3: `build/nav/zone71.navgrid` varken (yoksa `python3 tools/nav-export.py` ile üret) `./tools/run-tests.sh Release --no-build` çıkış kodu 0; çıktıda `106 tests, 0 failed` (önceki 98 + sekiz yeni), tüm eski testler ve sekiz yeni test `[ OK ]`; `SKIPPED` geçmiyor; `NAVREACH random maps: …` satırı (`multi ≥ 15`, `apart ≥ 300`, `same ≥ 300`), `NAVREACH drop A: …` ve `NAVREACH drop B: …` satırları, `NAVREACH real: …` satırı ve iki `NAVREACH perf set=…` satırı var.
- [ ] K4: Gerçek harita yokken (`build/nav/zone71.navgrid` geçici olarak başka ada taşınarak) `./tools/run-tests.sh Release --no-build NavReach_` çıkış kodu 0, `NavReach_RealMap` ve `NavReach_Perf` `SKIPPED` yazar ve diğer altı test geçer; ardından dosya yerine geri konur.
- [ ] K5: `./tools/run-tests.sh Debug` (derleme dahil) çıkış kodu 0 (Debug'da `assert`/sınır hataları yok).
- [ ] K6: Davranış sayıları (Release ve Debug): `NAVREACH drop A: detect_ms=0 drop_ms=1500 feeds=4` ve `NAVREACH drop B: detect_ms=1500 drop_ms=3000 feeds=8` (B'de yeniden planlamalar `t = 0, 500, 700, 1200, 1500, 2000, 2500, 3000`); `NavReach_Judge_Basic` (özellikle `ringCells == 20`/`37`/`168`, `ringConnected == 0`/`12`/`162`), `NavReach_Judge_Detour` ve `NavReach_RealMap` (`components=143 largest=88279 pockets=229`, cep `NodeLimit` → `Component`, cepte bot `NoPath` 15 düğüm) `[ OK ]`.
- [ ] K7: **Süre (Release):** `NAVREACH real: … build_ms=<x>` satırında `build_ms ≤ 100.00`; iki `NAVREACH perf set=…` satırında `judged=1000` ve **`ms_p95 ≤ 0.500`**. Uygulayıcı Raporu satırları **aynen** yapıştırır ve `nproc` bilgisini ekler. Kapı tutmuyorsa: testi üç kez koş (en kötüsünü raporla); tutarlı biçimde aşıyorsa kural gevşetilmez: `NavReach.h` içinde iyileştir (ör. `Build`'de bellek ayırmayı kaldırma, `Judge`'da halka üretimi); hâlâ aşıyorsa durup ölçümleri "Açık sorular"da raporla.
- [ ] K8: Saflık/kapsam: `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavReach.h` çıktısı boş; `git diff --stat gece/2026-10-02-nav...bot/F5-05` yalnızca §4'teki beş dosyayı ve kendi plan dosyasını gösterir (`GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h`, `NavSmooth.h` yok); `git diff gece/2026-10-02-nav...bot/F5-05 -- BotCore/NavTrack.h` tam üç eklenen satır (§5.1) gösterir, silinen satır yok.
- [ ] K9: F5-01..F5-04 davranışı bozulmadı: `./tools/run-tests.sh Release --no-build Nav_` (10 test), `NavPath_` (9), `NavSmooth_` (8) ve `NavTrack_` (10) çıkış kodu 0, hepsi `[ OK ]`; `NAVPATH T-NAV-03 set=near64 …` satırında `found ≥ 950` ve `ms_p95 ≤ 2.000`; `NAVSMOOTH perf set=near64 …` satırında `ms_p95 ≤ 0.500`; `NAVTRACK chase ring=0-0 replans=29 planned=29 caught_ms=11400 …` ve `NAVTRACK perf set=… ms_p95 ≤ 2.000` (aynı sayılar).

## 7. Doğrulama komutları

```bash
git switch -c bot/F5-05 gece/2026-10-02-nav
python3 tools/nav-export.py            # build/nav/zone71.navgrid yoksa
./tools/build.sh Release
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Release --no-build NavReach_
./tools/run-tests.sh Release --no-build NavTrack_
./tools/run-tests.sh Debug
grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavReach.h
git diff --stat gece/2026-10-02-nav...bot/F5-05
git diff gece/2026-10-02-nav...bot/F5-05 -- BotCore/NavTrack.h
```

## 8. Kısıtlar ve uyarılar

- **Paralel hat:** sunucuya hiç dokunma (`tools/run-servers.sh` çağırma; ana hat başka çalışma ağacından sunucu çalıştırıyor olabilir). DB'ye bağlanma. Dal tabanı `gece/2026-10-02-nav`.
- Kodlama/satır sonu: `AGENTS.md` §3. Yeni C++ dosyaları ASCII + CRLF + tab + Allman. Yorumlar İngilizce; plan metnindeki Türkçe açıklama koda girmez. `NavTrack.h`'deki üç satır da CRLF ile eklenir.
- Başlık-yalnızca: tüm tanımlar `inline` (sınıf içi tanımlar zaten satır içidir); `static` global durum yok; `#pragma` yalnızca `once`.
- **Belirlenim:** aynı ızgara ve aynı çağrı sırası her zaman aynı bileşen kimliklerini ve yargıyı verir (rastgelelik yok). Zaman yalnızca parametredir.
- **Ulaşılamaz yalnızca kanıtlanabildiğinde söylenir:** `NodeLimit`, ilk üç adayın başarısızlığı (botun bileşeninde halka adayı varken) ve botun kendi hücresinin yürünemez olması `Unknown`'dur. Bu planın testleri bu ayrımı sıkı sınar; gevşetme.
- `NavReach` bayat olabilir: `NavGrid::Build` yeni parametreyle çağrılırsa `NavReach::Build` yeniden çağrılmalıdır (test (d) bunu gösterir). Bayatlığı otomatik izleme kapsam dışıdır.
- `Judge`, `NavFollower::Update`'in **aynı çağrısının** planı ve parametreleriyle çağrılmalıdır (`plan.predX/predZ` halka merkezidir); başka parametrelerle çağırmak yanlış halka üretir. Bunu başlık yorumunda yaz.
- Hiçbir yerde `NavUnreachTracker::Due`'yu "hedef bırakıldı" diye adlandırma/yorumlama: yalnızca "vadesi geldi"dir; bırakma kararı karar katmanınındır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-05` (taban: `gece/2026-10-02-nav`); `8a93160` [F5-05] Ulasilamaz hedef tespiti (BotCore/NavReach.h) ve NavReach testleri (not: commit başlığında `Ulasilamaz` yazım hatası `Ulamsilamaz` olarak kaldı, `--amend` izinli değil; içerik doğru).
- Değişen dosyalar ve neden:
  - `BotCore/NavReach.h` (yeni): `NavReach` (`EdgeOpen` ile 8 komşulu bileşen tablosu), `NavReachVerdict`/`NavUnreachReason`/`NavUnreachParams`/`NavReachJudgement`, `NavReachJudge` (kesin yargı sırası: None → Unknown (InvalidStart/yürünemez bot) → NoWalkable → Component → Detour/Reachable → PathFailed=Unknown) ve `NavUnreachTracker` (kesintisiz `Unreachable` serisi, `Due`).
  - `BotCore/NavTrack.h`: yalnızca `NavFollowPlan::pathCost` alanı + `Update`'te iki satır (`0.0f` sıfırlama, Found'da `m_path.cost`). Üç ekleme, silme yok.
  - `Tests/BotCoreTests/NavReachTests.cpp` (yeni): sekiz test (`Components_Small`, `Matches_AStar`, `Judge_Basic`, `Judge_Detour`, `Tracker`, `Drop_Sim`, `RealMap`, `Perf`); yardımcılar `NavTrackTests.cpp`'den kopyalandı, `GapWall`/`BlockHeights` eklendi.
  - `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj`: yalnızca `NavReach.h` / `NavReachTests.cpp` kayıt satırı (BOM + CRLF korundu).
- Derleme sonucu:
  - `./tools/build.sh Release` rc=0; `touch` ile zorlanan yeniden derlemede (`NavReach.h`, `NavTrack.h`, `NavReachTests.cpp`) hiç `warning` satırı yok (`NavReach`/`NavTrack` geçen uyarı da yok); son satırlar `BotCoreTests.vcxproj -> ...\BotCoreTests.exe`.
  - `./tools/run-tests.sh Debug` (derleme dahil) rc=0; `106 tests, 0 failed`.
  - nproc=16, Ryzen 7 7800X3D.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ hatasız derleme; yeni uyarı yok (Release, zorlamalı yeniden derleme).
  - K2 ✔ `--list` çıktısı sekiz `NavReach_*` adını içerir; toplam 106.
  - K3 ✔ Release rc=0, `106 tests, 0 failed`, 106 `[ OK ]`, `SKIPPED` yok; satırlar: `NAVREACH random maps: seeds=30 multi=30 pairs=4500 apart=1652 same=2848`; `NAVREACH drop A: detect_ms=0 drop_ms=1500 feeds=4`; `NAVREACH drop B: detect_ms=1500 drop_ms=3000 feeds=8`; `NAVREACH real: components=143 largest=88279 pockets=229 build_ms=8.37; pocket NodeLimit->Component, in-pocket NoPath->Component; mage ring=168 connected=162; detour path=242.108 straight=23.32`; `NAVREACH perf set=exact judged=1000 unreachable=14 detour=11 ms_p50=0.000 ms_p95=0.001 ms_p99=0.001`; `NAVREACH perf set=mage judged=1000 unreachable=28 detour=27 ms_p50=0.012 ms_p95=0.018 ms_p99=0.022`.
  - K4 ✔ `zone71.navgrid` geçici taşındı: `NavReach_` rc=0, `NavReach_RealMap`/`NavReach_Perf` `SKIPPED` yazıp geçti, diğer altısı `[ OK ]`; dosya geri kondu.
  - K5 ✔ `./tools/run-tests.sh Debug` rc=0, `106 tests, 0 failed`.
  - K6 ✔ A: `detect_ms=0 drop_ms=1500 feeds=4`; B: `detect_ms=1500 drop_ms=3000 feeds=8` (Release ve Debug aynı). `Judge_Basic` `ringCells` 20/1/1/1/37 ve `ringConnected` 0/0/0/0/12; gerçek harita `components=143 largest=88279 pockets=229`, cep `NodeLimit` → `Component`, cepte bot `NoPath` 15 düğüm → `Component`, mage 168/162; hepsi `[ OK ]`.
  - K7 ✔ Release `build_ms=8.37 ≤ 100`; iki perf satırı `judged=1000`, `ms_p95` 0.001 / 0.018 ≤ 0.500. (Debug: `build_ms=234.88`, perf mage `ms_p95=0.523`, kapı yok.)
  - K8 ✔ `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavReach.h` boş; `git diff --stat gece/2026-10-02-nav...bot/F5-05` yalnızca beş dosya (plan raporu commit'i sonrası + plan dosyası); `NavTrack.h` diff'i tam üç ekleme, 0 silme.
  - K9 ✔ `Nav_` 10/10; `NavPath_` 9/9 (`set=near64 found=997 ms_p95=0.569`); `NavSmooth_` 8/8 (`set=near64 ms_p95=0.032`); `NavTrack_` 10/10 (`chase ring=0-0 replans=29 planned=29 caught_ms=11400`, `chase ring=30-45 dist_min=35.8 dist_max=43.0`, `perf set=exact ms_p95=0.554` / `set=mage ms_p95=0.692`).
- Plandan sapmalar ve gerekçeleri: Yok. Test yardımcıları plandaki kalıplardan kopyalandı; `Dist`/`Center`/`SegmentsClear` hiçbir testte gerekmediği için (MSVC C4505 uyarısını önlemek adına) tanımlanmadı. RNG tabanlı sayaçlar prototipten farklı (`apart=1652` vs 1618, `same=2848` vs 2882; `multi=30` aynı): C++ `Rng` farklı örnekler üretir, eşikler (`multi ≥ 15`, `apart ≥ 300`, `same ≥ 300`) rahatça karşılanır. `Judge_Basic` (f) `fp.ringMaxM = 13.0f` ile `ringMinM = 0` kullanıldı (plan `[0, 13]` diyor); `NavUnreachParams up` varsayılan.
- Açık sorular: Yok. K7 kapısı tek koşuda geniş farkla tuttuğu için üç koşu yapılmadı.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

—
