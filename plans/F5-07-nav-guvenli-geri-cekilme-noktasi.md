# F5-07: Güvenli geri çekilme noktası (`BotCore/NavRetreat.h`; tek geçişli sel + puanlama)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat, `docs/17` §1 "Paralel yürütülebilir işler") |
| Branch | `bot/F5-07` (taban: `gece/2026-10-02-nav`) |
| Bağımlı olduğu planlar | F5-01 (`BotCore/NavGrid.h`), F5-02 (`BotCore/NavPath.h`: yalnızca `NavCell`), F5-06 (`BotCore/NavDanger.h`): `KAPANDI`, `gece/2026-10-02-nav` içinde (merge `788aa86`, `dc1bb10`, `077a41e`); bu planın testleri 114 testin üstüne eklenir |
| İlgili gereksinim / kabul | `docs/12` §8 (güvenli geri çekilme noktası), §7 (kendi tower halkası = güvenli bölge), `docs/11` §4.3-§4.4 (geri çekilme hareketi, `last_stand`); ADR-0006 Eki F5-07 (bu planın kararları), ADR-0016 (`BotCore` saflığı) |
| Tahmini büyüklük | S (4 dosya: 2 yeni, 2 proje satırı; ~1 500 satır, çoğu test) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Bot düşük HP'de ya da ezildiğinde (`docs/11` §4.2 RETREAT) **nereye** çekileceğini bilmelidir. Bu plan bunun saf mantığını yazar: botun hücresinden başlayan tek bir sınırlı en-kısa-yol taramasıyla (R: party 40 m, solo 150 m) ulaşılabilir hücreleri bulur, her birini `docs/12` §8 puanıyla değerlendirir ve en iyisini yolu ile birlikte döndürür. Kurallar:

1. Yol bir düşman melee'sinin 8 m'sinden yakına **girmez** (bölgenin içinde başlayan bot yalnızca uzaklaşarak çıkar; düşmanın içinden geçemez).
2. Karşı ulus tower halkasına (F5-06 yasaklı bölge) girilmez; halkanın içinde başlayan bot içeriden çıkar; yasaklı hücre aday olamaz.
3. Puan: düşük tehlike, kısa yol, dayanak noktasına (party: arka hat, solo: kendi kapısı) yakınlık, açıklık (clearance) ve kendi tower halkası (`Safe` bayrağı) tercih edilir.
4. Aday yoksa `NoCandidate` döner (`last_stand` sinyali, `docs/11` §4.4).

Bu planın sonunda **karar katmanı (ne zaman geri çekilinir), arka hat hesabı, `NavFollower`/`NavReach` bağlaması, rota üzerindeki tehlike puanı, yol düzleştirme, telemetri ve sunucu entegrasyonu yoktur.** `GameServer/`, `AIServer/`, `shared/` değişmez; sunucu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0006-navigasyon-izgara-astar.md` — **Eki F5-07** (bu plan yazılırken eklendi): tek geçişli sel, 8 m kuralının kesin biçimi, puan ve ağırlıklar `[A]`, bilinçli ertelenenler. Sapma gerekiyorsa dur ve sor.
- `docs/12_NAVIGATION_AND_POSITIONING.md` §8 (bu plana göre güncellendi), §7, §2.
- `BotCore/NavGrid.h` — kullanacağın API: `Size()`, `Unit()`, `CellOf(float)`, `Walk(x, z)`, `Clearance(x, z)` (`uint8_t`), `EdgeOpen(x, z, dx, dz)`. **Değiştirme.**
- `BotCore/NavPath.h` — yalnızca `NavCell` (`:21-26`) için include edilir. **Değiştirme.** `NavPathfinder` havuz/üretim damgası (`m_generation`, `m_seen`, `m_closed`, `ResetPool`) kalıbını (`:64-291`) örnek al; `OpenNode`/`OpenWorse` yığın düzeni (`:223-251`) aynen geçerli (küçük `f` yerine küçük mesafe).
- `BotCore/NavDanger.h` — `NavCostLayer` (`Danger`, `Forbidden`, `Safe`, `Size`), `NavThreat`/`NavThreatKind`, `NavBuildTeamZones`, `AddThreats`. **Değiştirme.**
- `Tests/BotCoreTests/NavDangerTests.cpp:14-120` — **kopyalanacak yardımcılar** (yeni dosyada kendi kopyalarını yaz; başka `.cpp`'den içe aktarma yok): `CellIndex`, `Cell`, `HeightZeros`, `RingEvents`, `GapWall`, `MakeNav`, `PercentileDouble`. Gerçek harita yükleme + `SKIPPED` kalıbı ve süre ölçümü: `NavDangerTests.cpp` `NavDanger_RealMap` / `NavDanger_Perf` (aynı kalıp).
- `BotCore/Rng.h` — testlerde `BotCore::Rng` (`NextBelow(uint32_t)`); global `rand()` yasak.
- `Tests/BotCoreTests/MiniTest.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`; kayan nokta için `CHECK(std::fabs(a - b) <= eps)`), `tools/run-tests.sh` (testi depo kökünde çalıştırır).

Planı yazarken doğrulanan gerçekler (Python prototipi, 2026-10-02, depoya girmeyen geçici; **aynı kenar kuralı, aynı formül**; sayılar test beklentileridir; prototip çift duyarlıklıdır, C++ `float` farkları belirtilen toleransların çok altındadır). "Düz 40×40" = `RingEvents(40)` (kenar engelli, iç açık), `unit = 4`, yükseklik 0; hücre `(x, z)` merkezi `((x + 0.5) * 4, (z + 0.5) * 4)`: `(20,20)` → `(82,82)`, `(20,19)` → `(82,78)`, `(28,20)` → `(114,82)`, `(30,20)` → `(122,82)`, `(24,19)` → `(98,78)`, `(12,20)` → `(50,82)`, `(8,20)` → `(34,82)`. `Cw(x,z)` aşağıda bu merkezi dünya noktası olarak anlatır. Varsayılan parametreler §5.1'dedir (`partyRadiusM` 40, `soloRadiusM` 150, `meleeClearM` 8, `wDanger` 3, `wPath` 1, `wAnchor` 1,5, `wClear` 0,5, `wSafe` 1). Düz 40×40'ta iç hücrelerin `Clearance`'ı duvardan ≥ 3 hücre uzaktaki hücrelerde ≥ 3'tür (`min(clearance, 3) / 3 = 1`, yani `wClear` terimi 0,5); `(20,20)` hücresinde 19.

**Tablo 1 — düz 40×40, katman yok, tehdit yok, başlangıç `(20,20)`:**

| Alt senaryo | Beklenen |
|---|---|
| A party, dayanak yok | `Found`, hücre `(20,20)` (başlangıç), puan **0,5**, uzunluk 0, yol 1 hücre, aday = genişletilen = **285** |
| A solo | aynı; aday = genişletilen = **1444** (38×38 iç hücrenin hepsi 150 m içinde) |
| B party, dayanak `Cw(28,20)` | hücre `(28,20)`, puan **1,2**, uzunluk **32,0**, yol `(20,20)..(28,20)` 9 hücre (x artan, z sabit); solo: puan **1,78667** |
| Sınır dahil: dayanak `Cw(30,20)` | party: hücre `(30,20)`, puan **1,0**, uzunluk **40,0** (R'ye eşit, dahil), yol 11 hücre; `partyRadiusM = 39,9` ile: hücre `(29,20)`, puan **0,94737**, uzunluk 36,0, aday **281** |
| R küçük: `partyRadiusM = 20` | `(20,20)`, aday = genişletilen = **73**; `partyRadiusM = 0` ve `-5` (negatif 0 sayılır): aday = genişletilen = **1**, hücre başlangıç, puan 0,5 (R ≤ 0 iken yol ve yakınlık terimleri 0) |
| Beraberlik: `wDanger = wPath = wClear = 0`, dayanak yok | tüm adaylar puan 0: seçilen **başlangıç** `(20,20)` (en kısa yol), aday 285 |
| Dört eşit aday: `wDanger = wPath = wClear = 0`, `wAnchor = 1`, dayanak dünya noktası `(80,80)` (dört hücre köşesi: `(19,19) (19,20) (20,19) (20,20)` merkezleri eşit uzaklıkta), başlangıç `(15,19)` | seçilen **`(19,19)`** (dört eşitten en kısa yol: 16,0), puan **0,92929** (`1 - 2,828 / 40`), yol 5 hücre |
| Negatif ağırlık: `wPath = -5`, `wDanger = -1` (negatif 0 sayılır), dayanak yok | `(20,20)`, puan 0,5 |

**Tablo 2 — koridor ve melee kuralı.** `GapWall(40, 20, 19, 20)` (x = 20 sütunu duvar, z ∈ {19, 20} geçit), başlangıç `(16,19)`, dayanak `Cw(24,19)` = `(98,78)`, party, melee dünya noktası `M = (82,78)` (geçidin ortasındaki hücre `(20,19)`):

| Alt senaryo | Beklenen |
|---|---|
| C0: katman yok, tehdit yok | `Found`, hücre `(24,19)`, puan **1,2**, uzunluk **32,0**, yol `x = 20` sütunundan `z = 19`'da geçer; aday **254** |
| C1: katman = `AddThreats({M, Melee})`, tehdit listesi `{M, Melee}` | hücre **`(14,19)`**, puan **0,3**, uzunluk **8,0**, adayın `Danger`'ı 0, aday = genişletilen = **204**; yol `(16,19) (15,19) (14,19)`; geçit ve sağ taraf hiç ulaşılamaz |
| C1b: aynı katman, tehdit listesi **boş** | hücre `(26,19)`, puan **0,7**, uzunluk **40,0** (tehlike yumuşak, 8 m kuralı listeden gelir, katmandan değil), aday 254, yol geçitten geçer |
| C1c: katman `AddThreats({M, Ranged})`, tehdit listesi `{M, Ranged}` | 8 m kuralı yok (yalnızca melee sayılır): aday **254**, hücre `(6,19)`, puan **-0,5**, uzunluk **40,0** |

**Tablo 3 — düz 40×40, melee `M = Cw(20,20) = (82,82)`, katman `AddThreats({M, Melee})` (melee: çekirdek 15 m, ağırlık 1, sönüm 8 m):** 8 m bölgesi `(20,20)` etrafındaki disk (merkez uzaklığı ≤ 8 m).

| Alt senaryo | Beklenen |
|---|---|
| D party: başlangıç `(20,21)` (melee'ye 4 m, bölge içinde), dayanak yok | `Found`, hücre **`(20,26)`**, puan **0,0** (`|Δ| ≤ 1e-3`), uzunluk **20,0**, `Danger` 0, yol `(20,21) (20,22) ... (20,26)` 6 hücre, aday **271**, genişletilen **283** (başlangıç bölgede olduğu için aday değil) |
| D solo: aynı | hücre `(20,26)`, puan **0,36667**, aday **1431**, genişletilen **1443** |
| D `wDanger = 0` | hücre **`(19,22)`** (melee'ye 8,94 m, tehlike 255), puan **0,35858**, uzunluk **5,6569**, yol 2 hücre |
| D `meleeClearM = 10` | hücre `(20,26)`, aday **263** |
| D2: başlangıç `(21,20)` (4 m), dayanak `Cw(12,20)` = `(50,82)`, party | hücre **`(12,20)`**, puan **1,01716** (`2e-3`), uzunluk **39,3137** (`1e-2`), `Danger` 0: yol düşmanın etrafından dolanır, **içinden geçmez** (§5.3 değişmezi); aday 271, genişletilen 283 |
| NoCandidate: başlangıç `(20,20)` (melee'nin üstünde), `partyRadiusM = 2` | durum **`NoCandidate`**, aday 0, genişletilen **1**, yol boş |

**Tablo 4 — düz 40×40, bölgeler (katman `AddForbidDisc` / `AddSafeDisc`).** `F2`: `AddForbidDisc(Cw(30,20), 12)` ve `AddSafeDisc(Cw(8,20), 12)`; her ikisi **29** hücre. Başlangıç `(20,20)`.

| Alt senaryo | Beklenen |
|---|---|
| E solo, dayanak `Cw(8,20)` | hücre **`(8,20)`** (`Safe`), puan **2,68**, uzunluk **48,0**, yol 13 hücre, aday = genişletilen = **1415** (= 1444 − 29 yasaklı); `wSafe = 0`: puan **1,68** |
| E party, dayanak `Cw(8,20)` | hücre `(10,20)`, puan **1,7**, uzunluk 40,0, `Safe`, aday = genişletilen = **273** |
| E2 solo, dayanak yok | hücre **`(11,20)`** (`Safe` halkanın en yakın hücresi, merkez uzaklığı tam 12), puan **1,26**, uzunluk 36,0 |
| F: katman `AddForbidDisc(Cw(20,20), 20)` (**81** hücre), başlangıç `(20,20)` (içeride), solo, dayanak yok | hücre **`(15,19)`**, puan **0,35562** (`1e-3`), uzunluk **21,6569** (`1e-2`), aday **1363**, genişletilen **1444**; yolun **5** hücresi yasaklı (başlangıç dahil), son hücre yasaklı değil, yoldaki yasaklı hücreler bir önektir (dışarı çıkınca yeniden girmez); **yol şekli sınanmaz** (eşit uzunlukta başka yollar var) |
| F party | hücre `(15,19)`, puan **-0,04142**, aday **204**, genişletilen **285** |
| F `soloRadiusM = 2` | `NoCandidate`, aday 0, genişletilen 1 (başlangıç yasaklı, aday değil) |

**Gerçek harita (zone 71; `build/nav/zone71.navgrid`).** Kapılar Karus `(1375, 1085)`, El Morad `(622, 911)` (`docs/03` §12.2). Takım katmanı **El Morad**: `NavBuildTeamZones(grid, 1375, 1085, 622, 911, zp, elm)` (düşman Karus halkası yasaklı, kendi halkası güvenli; F5-06: yasaklı 1594, güvenli 1591). Dayanak = kendi kapısı `(622, 911)`. Başlangıç `S_A = (185, 227)` (kapıya 120,0 m; merkez `(742, 910)`):

| Senaryo | Beklenen |
|---|---|
| R-A solo | `Found`, hücre **`(159, 228)`** (`Safe`), puan **2,13283** (`2e-3`), uzunluk **105,657** (`0,05`), `Danger` 0, yol **27** hücre, aday = genişletilen = **2799** |
| R-A party | hücre **`(177, 229)`** (`Safe`), puan **0,61716** (`2e-3`), uzunluk **35,314** (`0,05`), yol **9** hücre, aday = genişletilen = **245** |
| R-B: `elm` + melee `(690, 910)`: katman = `elm` kopyası + `AddThreats`, tehdit listesi `{(690, 910) Melee}`, solo | hücre `(159, 228)`, puan **2,08865** (`2e-3`), uzunluk **112,284** (`0,05`) > R-A uzunluğu (dolambaç), yol 27 hücre, aday = genişletilen = **2783**; yolun her hücre merkezi melee'ye ≥ 8 m (gerçekte en yakın **12,0 m**, `±0,01`) |
| R-C: başlangıç `(342, 272)` (Karus kapısına 7,07 m: düşman halkasının içinde, **Walk**), solo, dayanak yok, katman `elm` | hücre **`(320, 271)`** (yasaklı değil), puan **-0,09771** (`2e-3`), uzunluk **89,657** (`0,05`), `Danger` 0, yol **23** hücre, bunların **22'si yasaklı** (önek), aday **1247**, genişletilen **2424** |
| R-D: başlangıç `(318, 222)` (arena A), solo, dayanak kapı (300 m'den uzak) | hücre = başlangıç, puan **0,5**, uzunluk 0, yol 1 hücre, aday = genişletilen = **2089** (yakınlık 0, kalmak en iyi) |

(Hücre merkezleri: `(185,227)` → `(742, 910)`; `(342,272)` → `(1370, 1090)`. `(342,272)` F5-06 gerçek-harita testindeki `S` hücresidir.)

**Rastgele küçük haritalar:** 24×24, iç hücre %8 engelli, %20 yükseklik 6 m; her haritada 3 tehdit (rastgele tür) + `seed < 15` ise bir yasaklı ve bir güvenli disk; 30 tohum × 60 sorgu: prototipte **1786 `Found`, 14 `NoCandidate`, 0 `InvalidStart`** (Python `random`; C++ `Rng` farklı örnekler verir, eşikler geniş: §5.2).

**Düğüm sayısı:** solo taramada gerçek haritada tipik genişletilen düğüm 2 300–2 800 (en çok ≈ 4 400: R = 150 m'lik sekizgen); party ≈ 250. Süre kapıları §6 K7.

## 3. Kapsam

**Yapılacaklar**

- `BotCore/NavRetreat.h`: `NavRetreatMode`, `NavRetreatStatus`, `NavRetreatParams`, `NavRetreatQuery`, `NavRetreatResult`, `NavRetreatPlanner` (§5.1).
- `Tests/BotCoreTests/NavRetreatTests.cpp`: §5.2 test listesi (birim + bağımsız referans çapraz doğrulama + gerçek harita + süre).
- `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` kayıtları (§5.3).

**Kapsam dışı (yapılmayacak)**

- Karar katmanı: ne zaman geri çekilinir (`docs/11` §4.2), `last_stand` sayma eşiği (adayın tehlikesi çok yüksekse), histerezis, `P-SUR-*` parametreleri. `BotCore` yalnızca aday ve puanı verir.
- Arka hat (party) ve dayanak noktası hesabı (F7): dayanak noktasını **çağıran** verir.
- Rota boyunca tehlike puanı (yalnızca 8 m ve yasaklı kuralları uygulanır), yol düzleştirme (`NavSmoothPath` 8 m kuralını bilmez; sonucu düzleştirmek bu planın dışında), `NavFollower`/`NavReach`/`NavReachJudge` bağlama, `region_graph` ile R > 150 m, canavar alanı, eğim cezası, formasyon.
- `NavPath.h`, `NavDanger.h`, `NavGrid.h`, `NavReach.h`, `NavTrack.h`, `NavSmooth.h` **değişmez**.
- `GameServer/`, `AIServer/`, `shared/`, `docs/` değişikliği, DB, sunucu çalıştırma. `GameServer/proj-GameServer.vcxproj` değişmez.
- `NavRetreatPlanner`'ın iş parçacığı güvenliği (örnek başına bir bağlam), `NavGrid::Build` değişince havuz izlemesi (havuz boyutu `Size()` değişince yeniden kurulur, başka izleme yok).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavRetreat.h` | yeni | ASCII, CRLF, başlık-yalnızca (satır içi); yalnızca `NavGrid.h`, `NavPath.h`, `NavDanger.h` + standart başlıklar |
| `Tests/BotCoreTests/NavRetreatTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="NavRetreat.h" />` satırı (BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="NavRetreatTests.cpp" />` satırı (BOM ve CRLF korunur) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`build/nav/zone71.navgrid` üretilen çıktıdır, commit edilmez; yoksa `python3 tools/nav-export.py` ile üret.)

## 5. Uygulama adımları

### 5.1 `BotCore/NavRetreat.h`

Ad alanı `BotCore`; `#include "NavGrid.h"`, `#include "NavPath.h"` (yalnızca `NavCell` için), `#include "NavDanger.h"` (aynı dizin, tırnaklı) ve `<algorithm> <cmath> <cstddef> <cstdint> <limits> <vector>`. Stil `NavPath.h`/`NavDanger.h` gibi: tab girinti, Allman, İngilizce yorum, sınıf içi/`inline` tanımlar, global/`static` durum yok, `#pragma once`.

```cpp
namespace BotCore
{
	enum class NavRetreatMode { Party, Solo };

	enum class NavRetreatStatus
	{
		Found,
		NoCandidate,    // no candidate cell within the radius: the caller treats it as last_stand (docs/11 s4.4)
		InvalidStart    // start out of bounds / not Walk (also before NavGrid::Build)
	};

	struct NavRetreatParams
	{
		float partyRadiusM = 40.0f;    // [O] docs/12 s8: R in party mode (docs/11 s4.3)
		float soloRadiusM = 150.0f;    // [O] docs/12 s8: R in solo mode
		float meleeClearM = 8.0f;      // [O] docs/12 s8: a path stays out of 8 m of an enemy melee
		float wDanger = 3.0f;          // [A] w1/w2 merged (one NavCostLayer)
		float wPath = 1.0f;            // [A] w3
		float wAnchor = 1.5f;          // [A] w4
		float wClear = 0.5f;           // [A] w5, applied to min(clearance, 3) / 3
		float wSafe = 1.0f;            // [A] bonus for a Safe-flagged cell (own tower ring, docs/12 s7)
	};

	struct NavRetreatQuery
	{
		NavCell start;
		NavRetreatMode mode = NavRetreatMode::Party;
		bool hasAnchor = false;                  // party: backline point; solo: own gate (caller decides)
		float anchorX = 0.0f;                    // world metres
		float anchorZ = 0.0f;
		const NavThreat * threats = nullptr;     // only Melee entries are used (the 8 m rule)
		size_t threatCount = 0;
	};

	struct NavRetreatResult
	{
		NavRetreatStatus status = NavRetreatStatus::NoCandidate;
		NavCell cell;                            // chosen cell; meaningful only when Found (may equal the start)
		float score = 0.0f;
		float pathLengthM = 0.0f;                // geometric length of `path`
		uint8_t danger = 0;                      // layer danger at `cell`
		bool safe = false;                       // layer Safe flag at `cell`
		std::vector<NavCell> path;               // start..cell inclusive; empty unless Found
		int candidates = 0;                      // cells that qualified as candidates
		int expanded = 0;                        // settled (closed) cells
	};

	class NavRetreatPlanner
	{
	public:
		// Reusable between queries and grids (buffers are resized when the grid size changes); NOT
		// thread-safe: one instance per thread/context. Deterministic. `grid` and `layer` are never
		// modified; `layer` may be null (no danger, no forbidden, no safe) and is ignored when
		// layer->Size() != grid.Size(). `out` is fully overwritten.
		void Find(const NavGrid & grid, const NavCostLayer * layer, const NavRetreatQuery & query,
			const NavRetreatParams & params, NavRetreatResult & out);
	private:
		// pools: dist (float), parent (int32), seen/closed/zone stamps (uint32), zoneD2 (float), heap
	};
}
```

**Kesin kurallar:**

1. **Başlangıç:** `!grid.Walk(start)` → `InvalidStart` (`expanded == 0`, `path` boş). Önce `out` tümüyle sıfırlanır (`status = NoCandidate`, `cell = start`, diğerleri 0/boş).
2. **Yarıçap:** `R = mode == Party ? partyRadiusM : soloRadiusM`; `R < 0` ise 0. `clearM = max(0, meleeClearM)`. Negatif ağırlıklar 0 sayılır (`wDanger`, `wPath`, `wAnchor`, `wClear`, `wSafe`). Katman: `zones = (layer != nullptr && layer->Size() == grid.Size()) ? layer : nullptr`.
3. **Melee bölge haritası (sorgu başına):** her `Melee` tehdit için sınır kutusu `floor((t ± clearM) / unit)` ile (ızgaraya kırpılır); kutudaki her hücre için `dx = (cx + 0.5f) * unit - tx`, `dz = ...`, `d2 = dx*dx + dz*dz`; `d2 <= clearM*clearM` ise hücre "bölgede" sayılır ve `zoneD2[idx] = en küçük d2` (birden çok melee'de en yakınınki; üretim damgasıyla, her sorguda diziyi doldurma). Bölgede olmayan hücrenin uzaklığı `+sonsuz` kabul edilir. `Ranged` tehditler ve `threats == nullptr` ya da `threatCount == 0` yok sayılır.
4. **Tarama:** Dijkstra, yığın anahtarı (küçük mesafe, sonra küçük `idx`; `NavPathfinder`'ın `OpenWorse` düzeni ile aynı: eşit mesafede küçük indeks önce çıkar), tembel silme. `dist[start] = 0`. Kenarlar 8 komşu, yalnızca `grid.EdgeOpen(cx, cz, dx, dz)`; adım `unit` ya da `unit * sqrt(2.0f)` (`NavPathfinder` ile aynı ifade). `nd = d + step`; **`nd > R` ise kenar atlanır** (sınır dahil: `nd <= R` kalır). Kapalı (yerleşmiş) hücreye kenar atlanır. Bir kenar `cur → nb` ayrıca şu durumlarda **atlanır**: (a) `zones != nullptr`, `zones->Forbidden(nb)` ve `!zones->Forbidden(cur)` (dışarıdan yasaklıya giriş yok; içeriden serbest); (b) `nb` bölgede ve `zoneD2(nb) < zoneD2(cur)` (karşılaştırma `<`; `cur` bölgede değilse `zoneD2(cur) = +sonsuz`; eşit uzaklıktaki adım serbest). `nb` için `nd < dist[nb]` (ya da ilk görülüş) ise `dist`, `parent` güncellenir ve yığına eklenir (ebeveyn yalnızca **kesin** küçük mesafede değişir).
5. **Aday:** bir hücre yerleştirildiğinde (yığından çıkıp kapatıldığında) `candidates` sayılır ve puanlanır **eğer** `!(zones && zones->Forbidden(cx, cz))` ve hücre bölgede değilse. Başlangıç hücresi de aday olabilir (uzunluk 0). `expanded` her kapatılan hücreyi sayar (aday olmayanlar dahil).
6. **Puan (kesin, `float`, bu sırada):** `s = 0;` `s -= wDanger * (float)danger / 255.0f` (`danger = zones ? zones->Danger(cx, cz) : 0`); `R > 0` ise `s -= wPath * (d / R)`; `hasAnchor && R > 0` ise `da = sqrt((cellCenterX - anchorX)^2 + (cellCenterZ - anchorZ)^2)`, `closeness = max(0, 1 - da / R)`, `s += wAnchor * closeness`; `s += wClear * (float)min((int)grid.Clearance(cx, cz), 3) / 3.0f`; `zones && zones->Safe(cx, cz)` ise `s += wSafe`. (`d` = o hücreye en kısa yol uzunluğu.) Hücre merkezi `(c + 0.5f) * unit`.
7. **En iyi:** en yüksek `s`; **beraberlikte** daha kısa yol, ondan sonra küçük indeks (`x * n + z`). Hücreler (mesafe, indeks) sırasıyla yerleştirildiği için ek karşılaştırma gerekmez: `s > bestScore` (kesin) yeterlidir ve sonradan gelen eşit puanlı hücre kazanamaz. Bu bir **uygulama bilgisidir**; davranış yukarıdaki beraberlik kuralıdır.
8. **Sonuç:** hiç aday yoksa `status = NoCandidate` (`candidates = 0`, `expanded` = yerleştirilen hücre sayısı). Aksi halde `Found`: `cell`, `score`, `pathLengthM = dist[cell]`, `danger`, `safe`, `path` = ebeveyn zinciri (başlangıçtan `cell`'e, her iki uç dahil), `candidates`, `expanded`.
9. **Havuz:** `NavPathfinder` gibi `Size()` değişince `ResetPool`; üretim damgası `uint32_t`, taşmada üç damga dizisini (`seen`, `closed`, `zone`) sıfırlayıp 1'den başlar. Havuz bellek: ≈ 6 MB (513²); bot başına paylaşım kararı sunucu entegrasyonunda.
10. Özyineleme yok. NaN/sonsuz girdi denenmez (çağıranın sorumluluğu). MSVC Level 4 uyarılarını `static_cast` ile çöz (`#pragma warning` ekleme).

### 5.2 `Tests/BotCoreTests/NavRetreatTests.cpp`

`#include "MiniTest.h"`, `<BotCore/NavGrid.h>`, `<BotCore/NavPath.h>`, `<BotCore/NavDanger.h>`, `<BotCore/NavRetreat.h>`, `<BotCore/Rng.h>`, `<algorithm> <chrono> <cmath> <cstdint> <cstdio> <limits> <vector>`. Anonim ad alanında yardımcılar (kendi kopyaların; **kullanılmayan yardımcı tanımlama**, MSVC C4505 uyarısı çıkar): `CellIndex`, `Cell`, `HeightZeros`, `RingEvents`, `GapWall`, `MakeNav`, `PercentileDouble` ve:

- `NavThreat Threat(float x, float z, NavThreatKind kind)`.
- `NavRetreatQuery Query(NavCell start, NavRetreatMode mode, bool hasAnchor, float ax, float az, const std::vector<NavThreat> & threats)`: `threats.empty()` ise `nullptr`/0.
- `double PathLength(const NavGrid &, const std::vector<NavCell> &)`: ardışık adımların `unit` / `unit * sqrt(2.0)` toplamı (çift duyarlık).
- `bool StepAllowed(const NavGrid & grid, const NavCostLayer * zones, const std::vector<NavThreat> & threats, double clearM, NavCell a, NavCell b)`: **bağımsız kural** (üretimdeki kodu paylaşmaz): `grid.EdgeOpen` ve `!(zones && zones->Forbidden(b) && !zones->Forbidden(a))` ve `!(InZone(b) && D2(b) < D2(a))`; `D2(c)` = tüm `Melee` tehditler üzerinde en küçük `(cx*unit+unit/2 - tx)^2 + (...)^2` (çift duyarlık, sınır kutusu yok); `InZone(c)` = `D2(c) <= clearM*clearM`; `D2(a)` bölge dışındaysa karşılaştırmada `+sonsuz` (yani `a` bölgede değilse `InZone(b)` olan her adım yasaktır).
- `void CheckResult(const NavGrid &, const NavCostLayer * zones, const std::vector<NavThreat> & threats, const NavRetreatQuery &, const NavRetreatParams &, const NavRetreatResult &)`: `Found` için: `path.front() == start`, `path.back() == cell`; ardışık her çift `|dx|,|dz| <= 1` ve `StepAllowed`; `cell` yasaklı değil ve bölgede değil; `pathLengthM <= R + 1e-3` ve `|PathLength(path) - pathLengthM| <= 1e-3`; yolda "yasaklı olmayandan yasaklıya" geçiş yok; `danger == zones->Danger(cell)` (zones yoksa 0), `safe == zones->Safe(cell)`; `candidates >= 1`, `expanded >= candidates`. `NoCandidate`/`InvalidStart` için: `path` boş, `candidates == 0`.
- `RefRetreat(grid, zones*, threats, params, query) -> RefOut`: **bağımsız referans** (Bellman-Ford tarzı yinelemeli gevşetme; üretimle yığın/yapı paylaşmaz). `RefOut { int code; double bestScore; int candidates; ... }` ve iki yardımcı: `double DistOf(NavCell)` (ulaşılamayan ya da kutu dışı hücre için `+sonsuz`), `double ScoreOf(NavCell)`. Algoritma: ızgaranın `start ± (ceil(R / unit) + 2)` kutusunda (ızgaraya kırpılır) `dist = +sonsuz`, `dist[start] = 0`; değişim kalmayıncaya kadar kutudaki her hücre `a` için (`dist[a]` sonlu) 8 komşu `b` için `StepAllowed(a, b)` ise `nd = dist[a] + step` (çift duyarlık; `unit` ya da `unit * sqrt(2.0)`), `nd <= R + 1e-9` ve `nd < dist[b] - 1e-12` ise güncelle. Aday = `dist <= R + 1e-9`, yasaklı olmayan, bölgede olmayan her hücre; `ScoreOf` §5.1 madde 6 formülüyle **çift duyarlıkta**. `code`: `0` bulundu (`bestScore` = en yüksek puan, `candidates` = aday sayısı), `1` aday yok, `2` başlangıç `Walk` değil.

Her testte `NavRetreatPlanner planner;`, `NavRetreatResult out;`, `NavRetreatParams params;` (varsayılan) kullanılır; yeni sorgu yeni `out` kullanır.

| Test adı | İçerik |
|---|---|
| `NavRetreat_Basics` | Düz 40×40, katman yok. (a) `NavRetreatParams` varsayılanları: `partyRadiusM == 40.0f`, `soloRadiusM == 150.0f`, `meleeClearM == 8.0f`, `wDanger == 3.0f`, `wPath == 1.0f`, `wAnchor == 1.5f`, `wClear == 0.5f`, `wSafe == 1.0f`; `NavRetreatQuery` varsayılanı `mode == Party`, `!hasAnchor`, `threats == nullptr`, `threatCount == 0`. (b) **InvalidStart:** başlangıç `(0,0)` (duvar), `(-1,5)`, `(40,5)` ve hiç `Build` edilmemiş `NavGrid g0;` ile `(5,5)`: `status == InvalidStart`, `path` boş, `candidates == 0`, `expanded == 0`. (c) **Tablo 1** satırları: A party/solo (hücre, puan, uzunluk, `path.size()`, `candidates == expanded`), B party/solo, sınır dahil (iki sorgu), R küçük (`partyRadiusM` 20, 0, -5), beraberlik (üç ağırlık 0), dört eşit aday, negatif ağırlık. Her `Found` sonucuna `CheckResult`. |
| `NavRetreat_Params` | Düz 40×40, melee `M = (82,82)`, katman `AddThreats({M, Melee})` (Tablo 3'ün kurulumu) ve parametre varyantları: (a) `wDanger = 0` ⇒ `(19,22)`, puan 0,35858, uzunluk 5,6569, `danger == 255`; (b) `meleeClearM = 10` ⇒ `(20,26)`, aday 263; (c) `meleeClearM = 0` (yalnızca merkezi `M` ile çakışan `(20,20)` bölgede): başlangıç `(20,21)`, party ⇒ `(20,26)`, puan 0,0, aday = genişletilen = **283** (`(20,20)` dışarıdan girilemez); `meleeClearM = -3` aynı sonuç; (d) **boyut uyuşmazlığı:** 30×30 düz ızgara için kurulan katman (yasaklı + güvenli disk + tehlike) 40×40 ızgarayla kullanılınca sonuç **katman yokken** aynıdır (Tablo 1 A: `(20,20)`, 0,5, aday 285); (e) **tehdit listesi yok sayılır:** katman aynı (melee tehlikesi var) ama sorguda `threats` dolu işaretçi ile `threatCount == 0`, ve ayrıca `threats == nullptr` ile `threatCount == 3`: 8 m kuralı yok, başlangıç `(20,21)` bölgede sayılmaz ve aday olur; sonuç iki durumda da: hücre **`(20,26)`**, puan **0,0** (`1e-3`), uzunluk 20,0, aday = genişletilen = **285**; (f) yalnızca `Ranged` tehdit listesi: 8 m kuralı yok (Tablo 2 C1c). Her `Found` sonucuna `CheckResult`. |
| `NavRetreat_Melee_Rule` | **Tablo 2** (C0, C1, C1b, C1c; koridor `GapWall(40, 20, 19, 20)`) ve **Tablo 3** D, D solo, D2, NoCandidate. C1'de yolun her hücre merkezi `M`'ye `> 8` m; geçit sütunundan (x = 20) hiç hücre yok. D2'de her ardışık çift için `StepAllowed` (yani `CheckResult`) ve yol `(20,20)` hücresinden geçmez; D'de yolun melee'ye uzaklığı azalmaz (her adımda `d2` küçülmüyor). Sayılar §2 Tablo 2/3. |
| `NavRetreat_Zones` | **Tablo 4**: E solo/party, `wSafe = 0`, E2, F (yasaklı önek özelliği: yoldaki yasaklı hücre sayısı 5, hepsi yolun başında ardışık, son hücre yasaklı değil), F party, F `soloRadiusM = 2` ⇒ `NoCandidate`. Ayrıca **başlangıç dışarıdayken yasaklı bölgeden geçilmez:** düz 40×40, `AddForbidDisc(Cw(20,20), 12)` (**29** hücre), başlangıç `(8,20)`, dayanak `Cw(32,20)`, solo: `Found`, hücre **`(32,20)`**, puan **1,27163** (`2e-3`), uzunluk **109,2548** (`0,02`), yol **25** hücre, yoldaki yasaklı hücre **0**, aday = genişletilen = **1414** (yasaklı 29 hücre ve 150 m'nin dışındaki 1 hücre eksik; 1444 − 29 − 1). Yasaklı ve güvenli bayrağı olmayan katmanda `safe == false`. |
| `NavRetreat_Matches_Reference` | **Bağımsız referans çapraz doğrulama** (rastgele küçük haritalar). 30 tohum (`seed = 0..29`), `Rng rng(5000u + seed)`, `n = 24`; harita `NavDanger_Path_Optimal` gibi (önce olay `NextBelow(100) < 8` engelli, sonra yükseklik `NextBelow(5) == 0 ? 6 : 0`, hücre başına sabit sıra, x artan z artan); `Walk` hücre listesi < 2 ise tohum atlanır. Katman: `layer.Init(grid)`; 3 tehdit: her biri için `rng.NextBelow(96)` ile x, `rng.NextBelow(96)` ile z (metre, tamsayıdan `float`), `rng.NextBelow(2)` ile tür (0 = Melee, 1 = Ranged); `layer.AddThreats(...)` (varsayılan `NavThreatParams`); `seed < 15` ise bir yasaklı disk ve bir güvenli disk (merkez `NextBelow(96)` çiftleri, yarıçap 12). Tek `seed` için parametreler varsayılan; çift `seed` için `wDanger = 5`, `wPath = 2`, `wAnchor = 0,7`, `wClear = 1`, `wSafe = 0,4`, `meleeClearM = 12`, `partyRadiusM = 30`, `soloRadiusM = 90`. 60 sorgu (`q = 0..59`): başlangıç `rng.NextBelow(count)` ile `Walk` hücre (yasaklı/bölgede olanlar da olabilir); mod `q % 2 == 0 ? Party : Solo`; dayanak `q % 3 != 0` ise `NextBelow(96)` çifti, değilse yok. Her sorguda üretim ve `RefRetreat`: ref `2` ⇔ `InvalidStart`; ref `1` ⇔ `NoCandidate`; ref `0` ⇔ `Found` ve `candidates` **eşit**, `|score - refBest| <= 1e-3`, seçilen hücrenin referans puanı `>= refBest - 1e-3`, `|pathLengthM - refDist(cell)| <= 1e-3`; her `Found` için `CheckResult`. Sayaçlar: `found`, `noCandidate`, `invalidStart`; `REQUIRE(found >= 1500)` (prototip 1786 / 1800); uyuşmazlık sayacı `mismatches == 0`. Yazdır: `NAVRETREAT random maps: seeds=30 queries=1800 found=<n> no_candidate=<n> invalid_start=<n> mismatches=<n>`. |
| `NavRetreat_Reuse` | Aynı `planner`: sorgu Q1 (Tablo 3 D2, düz 40×40) → sorgu Q2 (başka ızgara: 30×30, GapWall; havuz yeniden boyutlanır) → Q1 yeniden: ikinci Q1 sonucu ilkiyle **tamamen aynı** (`status`, `cell`, `score` bit düzeyinde `==`, `pathLengthM ==`, `path` eşit, `candidates`, `expanded`). Ayrıca taze bir `planner` ile aynı Q1 sonucu da aynı. 70 ardışık farklı sorgudan sonra (damga artışı) Q1 hâlâ aynı. |
| `NavRetreat_RealMap` | Gerçek harita (aşağıya bak). |
| `NavRetreat_Perf` | Gerçek harita (aşağıya bak): party ve solo süre kapıları. |

**Gerçek harita ortak kuralı** (F5-02..F5-06 ile aynı): dosya yolu `build/nav/zone71.navgrid` (çalışma dizini depo kökü); açılamazsa testi **başarısız yapma**: `std::printf("NAVRETREAT real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n")` yaz ve dön. `LoadFile` + `Build()` (varsayılan `NavParams`) sonrası `MainComponentCells() == 88508` `REQUIRE` edilir. Sabitler: Karus kapısı `(1375, 1085)`, El Morad kapısı `(622, 911)`, `NavZoneParams` varsayılan (90 m). `elm` = `NavBuildTeamZones(grid, 1375.0f, 1085.0f, 622.0f, 911.0f, zp, elm)`.

`NavRetreat_RealMap`:

- **R-A, R-B, R-C, R-D** (§2 gerçek harita tablosu): hücre, puan, uzunluk, yol uzunluğu (hücre sayısı), aday ve genişletilen sayıları, `Safe` (R-A ikisi de `true`; R-C hücresi `Forbidden` değil, yoldaki yasaklı hücre sayısı 22 ve bir önek). Dayanak her sorguda `(622.0f, 911.0f)` (R-C'de dayanak yok). R-B için katman `dyn = elm; dyn.AddThreats(&melee, 1, NavThreatParams())` ile kurulur (tehdit `(690.0f, 910.0f)`, `Melee`) ve tehdit listesi aynı tehdittir; yolun her hücre merkezinin `(690, 910)`'a uzaklığının en küçüğü `>= 8` (beklenen 12,0, `±0,01`). Her `Found` için `CheckResult`.
- **Referans taraması:** `Rng rng(20261002u)`, 30 sorgu: başlangıç `elm` içinde olabilen herhangi bir `Walk` hücre (x-ana tarama listesi, `NextBelow(count)`); 4 tehdit: başlangıç hücresinden Chebyshev ≤ 15 hücre içinde `rng.NextBelow(31)` ile `dx`, `dz` (`-15 + değer`) denenerek bulunan `Walk` hücre merkezleri (bulunamazsa yenisi denenir; sıra Melee, Ranged, Melee, Ranged); katman `dyn = elm; dyn.AddThreats(...)`; mod `q % 2 == 0 ? Party : Solo`; dayanak `q % 2 == 0` ise kapı, değilse yok. Her sorguda üretim ve `RefRetreat` (bu ızgarada kutu `start ± (ceil(R / unit) + 2)`, ızgaraya kırpılır): durum, `candidates`, puan (`1e-3`) ve `CheckResult` aynı kurallarla; `mismatches == 0`.
- Yazdır: `std::printf("NAVRETREAT real: solo A cell=(%d,%d) score=%.5f len=%.3f cand=%d; party A cell=(%d,%d) score=%.5f len=%.3f cand=%d; melee B score=%.5f len=%.3f cand=%d min_melee_m=%.2f; ring C cell=(%d,%d) score=%.5f len=%.3f forb=%d cand=%d exp=%d; arena D stay=%d cand=%d; sweep queries=%d mismatches=%d\n", ...)`.
- Not (planın doğruladığı sayı tutmazsa): sayı beklentiden saparsa **kuralı sayıya uydurma**; dur ve Uygulayıcı Raporu'nda sor.

`NavRetreat_Perf` (gerçek harita, aynı `SKIPPED` kuralı):

- `elm` katmanı kurulur (süre ölçülmez). Başlangıç adayları: `Walk` hücreler (x-ana tarama, tek `std::vector<NavCell>`; yasaklı hücreler dahil olabilir). Sorgu sayısı `Q` **Release'te 1000, Debug'da 100** (`#ifdef _DEBUG`). `Rng(20261002u)`. Her sorgu için: başlangıç `NextBelow(count)`; 8 tehdit (ilk 4 `Melee`, sonraki 4 `Ranged`), her biri başlangıçtan Chebyshev ≤ 20 hücre içinde `Walk` hücre merkezi (`dx`, `dz` = `-20 + NextBelow(41)` denenir, bulunamazsa yeniden); `dyn = elm; dyn.AddThreats(...)` **süre dışında**; dayanak kapı `(622, 911)`. **Isınma:** ölçümden önce 20 ek sorgu koşulur, sayılmaz. Bir `NavRetreatPlanner` yeniden kullanılır.
- İki küme (aynı sorgular, ayrı zamanlayıcı): **`party`** ve **`solo`**; süre yalnızca `Find` çağrısıdır (`std::chrono::steady_clock`).
- İstatistik (küme başına): `found`, `noCandidate`, `invalidStart`, genişletilen düğüm p50/p95, süre p50/p95/p99 (ms).
- Yazdır (**tam bu biçim**):

```
NAVRETREAT perf set=party queries=<n> found=<n> nocandidate=<n> invalidstart=<n> expanded_p50=<n> expanded_p95=<n> ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
NAVRETREAT perf set=solo queries=<n> found=<n> nocandidate=<n> invalidstart=<n> expanded_p50=<n> expanded_p95=<n> ms_p50=<x.xxx> ms_p95=<x.xxx> ms_p99=<x.xxx>
```

- `CHECK`'ler (tüm sorgular): `found + noCandidate + invalidStart == queries`; `invalidStart == 0`; `found * 100 >= Q * 95`; her `Found` sonucunda `path` dolu ve `candidates >= 1`. **`#ifndef _DEBUG`** (Release): `party` için **`ms_p95 <= 0.5`**, `solo` için **`ms_p95 <= 3.0`** `[A]` (geri çekilme kararı en çok 500 ms'de bir verilir; tipik solo taraması ≈ 2 800 hücre). Debug'da süre kapısı yoktur (satırları yine yazdır).

### 5.3 Proje dosyaları

1. `BotCore/BotCore.vcxproj`: `ClInclude` grubuna `<ClInclude Include="NavRetreat.h" />` (`NavReach.h` satırından sonra, `Perception.h` satırından önce; dosyada şu an `:79-80`). BOM/CRLF korunur.
2. `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubuna `<ClCompile Include="NavRetreatTests.cpp" />` (`NavReachTests.cpp` satırından sonra, `PerceptionTests.cpp` satırından önce; şu an `:87-88`). BOM/CRLF korunur.
3. Bu iki projenin `.filters` dosyası yoktur (F5-01'de doğrulandı); ek dosya yok.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; `BotCore` ve `BotCoreTests` için **yeni uyarı yok** (Level 4; `NavRetreat.h`, `NavRetreatTests.cpp` dosyalarını `touch` ile zorla yeniden derle; çıktıda `NavRetreat` geçen `warning` satırı bulunmaz).
- [ ] K2: `./tools/run-tests.sh Release --no-build --list` çıktısı şu sekiz test adını içerir: `NavRetreat_Basics`, `NavRetreat_Params`, `NavRetreat_Melee_Rule`, `NavRetreat_Zones`, `NavRetreat_Matches_Reference`, `NavRetreat_Reuse`, `NavRetreat_RealMap`, `NavRetreat_Perf`.
- [ ] K3: `build/nav/zone71.navgrid` varken (yoksa `python3 tools/nav-export.py` ile üret) `./tools/run-tests.sh Release --no-build` çıkış kodu 0; çıktıda `122 tests, 0 failed` (önceki 114 + sekiz yeni), tüm eski testler ve sekiz yeni test `[ OK ]`; `SKIPPED` geçmiyor; `NAVRETREAT random maps: …` satırı (`found ≥ 1500`, `mismatches=0`), `NAVRETREAT real: …` satırı ve iki `NAVRETREAT perf …` satırı var.
- [ ] K4: Gerçek harita yokken (`build/nav/zone71.navgrid` geçici olarak başka ada taşınarak) `./tools/run-tests.sh Release --no-build NavRetreat_` çıkış kodu 0, `NavRetreat_RealMap` ve `NavRetreat_Perf` `SKIPPED` yazar ve diğer altı test geçer; ardından dosya yerine geri konur.
- [ ] K5: `./tools/run-tests.sh Debug` (derleme dahil) çıkış kodu 0 (Debug'da `assert`/sınır hataları yok).
- [ ] K6: Davranış sayıları (Release ve Debug): §2 Tablo 1–4'teki tüm değerler (`NavRetreat_Basics`, `_Params`, `_Melee_Rule`, `_Zones` `[ OK ]`), `NavRetreat_Matches_Reference` (referans eşitliği, 1800 sorguda uyuşmazlık 0), `NavRetreat_Reuse` ve `NavRetreat_RealMap` (R-A solo `cell=(159,228) cand=2799`, party `cell=(177,229) cand=245`, R-B `cand=2783 min_melee_m≥8`, R-C `cell=(320,271) forb=22`, R-D `stay=1`; referans taramasında `mismatches=0`).
- [ ] K7: **Süre (Release):** `NAVRETREAT perf set=party` satırında **`ms_p95 ≤ 0.500`**, `set=solo` satırında **`ms_p95 ≤ 3.000`**, ikisinde `queries=1000`. Uygulayıcı Raporu satırları **aynen** yapıştırır ve `nproc` bilgisini ekler. Kapı tutmuyorsa: testi üç kez koş (en kötüsünü raporla); tutarlı biçimde aşıyorsa kural gevşetilmez: `NavRetreat.h` içinde iyileştir (ör. tehdit bölge haritasını yalnızca melee varken kur, kenar başına `Forbidden`/zone aramalarını hücre başına bir kez yap, `Clearance`/`Danger` okumalarını yalnızca aday olan hücrelerde yap); hâlâ aşıyorsa durup ölçümleri "Açık sorular"da raporla.
- [ ] K8: Saflık/kapsam: `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavRetreat.h` çıktısı boş; `git diff --stat gece/2026-10-02-nav...bot/F5-07` yalnızca §4'teki dört dosyayı ve kendi plan dosyasını gösterir (`GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h`, `NavDanger.h`, `NavReach.h`, `NavTrack.h`, `NavSmooth.h` yok).
- [ ] K9: F5-01..F5-06 davranışı bozulmadı: `./tools/run-tests.sh Release --no-build Nav_` (10 test), `NavPath_` (9), `NavSmooth_` (8), `NavTrack_` (10), `NavReach_` (8), `NavDanger_` (8) çıkış kodu 0, hepsi `[ OK ]` (**değişmeden**, testler düzenlenmez); `NAVPATH T-NAV-03 set=near64 …` satırında `found=997`, `expanded_p50=306 expanded_p95=2431`; `NAVDANGER real: elm_forbid=1594 elm_forbid_walk=1264 elm_safe=1591 elm_safe_walk=1232 …` ve `NAVREACH real: components=143 largest=88279 pockets=229 …` (aynı sayılar).

## 7. Doğrulama komutları

```bash
git switch -c bot/F5-07 gece/2026-10-02-nav
python3 tools/nav-export.py            # build/nav/zone71.navgrid yoksa
./tools/build.sh Release
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Release --no-build NavRetreat_
./tools/run-tests.sh Release --no-build NavDanger_
./tools/run-tests.sh Debug
grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavRetreat.h
git diff --stat gece/2026-10-02-nav...bot/F5-07
```

## 8. Kısıtlar ve uyarılar

- **Paralel hat:** sunucuya hiç dokunma (`tools/run-servers.sh` çağırma; ana hat başka çalışma ağacından sunucu çalıştırıyor olabilir). DB'ye bağlanma. Dal tabanı `gece/2026-10-02-nav`.
- Kodlama/satır sonu: `AGENTS.md` §3. Yeni C++ dosyaları ASCII + CRLF + tab + Allman. Yorumlar İngilizce; plan metnindeki Türkçe açıklama koda girmez.
- Başlık-yalnızca: tüm tanımlar `inline` (sınıf içi tanımlar zaten satır içidir; sınıf dışı tanımlanan üyeler `inline` bildirilir); `static` global durum yok; `#pragma` yalnızca `once`.
- **Belirlenim:** aynı ızgara, aynı katman, aynı sorgu ve parametreler her zaman aynı sonucu verir (rastgelelik yok; beraberlik kuralı §5.1 madde 7). Zaman yalnızca parametredir.
- **8 m kuralı kesindir ve tek yönlüdür:** hiçbir yol, melee'ye uzaklığı azaltan bir adımla 8 m bölgesine girmez (dışarıdan girilmez; içeride başlayan bot yalnızca uzaklaşarak ya da eşit uzaklıkta kalarak çıkar). Bu kural `StepAllowed` testinde bağımsız yazılır; üretim kodundaki kuralı **testten kopyalama**, ikisini ayrı yaz (çapraz doğrulamanın anlamı bu).
- **Yasaklı bölge kuralı** F5-06 ile aynıdır (dışarıdan girilmez, içeriden çıkış serbest), ama ek olarak yasaklı hücre aday olamaz.
- Puan ağırlıkları `[A]`: değiştirmek sayısal beklentileri (Tablo 1–4, gerçek harita) değiştirir; varsayılanları değiştirme.
- `NavRetreat.h` aday olmayan hücrelerde de genişletme yapar (bölgedeki/yasaklı hücrelerden geçiş için); `expanded >= candidates` her zaman doğrudur.
- Bu planın sonucu rota olarak **olduğu gibi** kullanılabilir; ancak `NavSmoothPath` ile düzleştirilirse kuralın yeniden denetimi çağıranın işidir (ADR-0006 Eki F5-07 madde 9).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-07` (taban: `gece/2026-10-02-nav` @ `b55e422`) — `9bb5325 [F5-07] Guvenli geri cekilme noktasi: NavRetreat.h + testler`; rapor/Durum commit'i bu satırın ardından.
- Değişen dosyalar ve neden:
  - `BotCore/NavRetreat.h` (yeni): `NavRetreatPlanner` (tek geçişli sınırlı Dijkstra seli, 8 m melee kuralı, F5-06 yasaklı kuralı, `docs/12` §8 puanı, üretim damgalı havuz). Başlık-yalnızca, ASCII+CRLF, `<algorithm> <cmath> <cstddef> <cstdint> <limits> <vector>` + `NavGrid.h`/`NavPath.h`/`NavDanger.h`.
  - `Tests/BotCoreTests/NavRetreatTests.cpp` (yeni): sekiz test (Tablo 1–4, bağımsız referans çapraz doğrulama, gerçek harita, süre).
  - `BotCore/BotCore.vcxproj`: yalnızca `<ClInclude Include="NavRetreat.h" />` (BOM/CRLF korundu).
  - `Tests/BotCoreTests/BotCoreTests.vcxproj`: yalnızca `<ClCompile Include="NavRetreatTests.cpp" />` (BOM/CRLF korundu).
- Derleme sonucu (`tools/build.sh Release`, `NavRetreat.h`+`NavRetreatTests.cpp` `touch` sonrası; çıktıda 0 `warning`):
  ```
  BotCore.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\BotCore.lib
  proj-GameServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\AIServer.exe
  NavRetreatTests.cpp
  BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ Release rc=0; `touch` ile yeniden derlenen iki dosya için uyarı 0, tüm derlemede `warning` satırı 0.
  - K2 ✔ `--list` sekiz `NavRetreat_*` adını içerir.
  - K3 ✔ `122 tests, 0 failed` (Release); `NavRetreat_Basics/_Params/_Melee_Rule/_Zones/_Matches_Reference/_Reuse/_RealMap/_Perf` hepsi `[ OK ]`, `SKIPPED` yok; `NAVRETREAT random maps: seeds=30 queries=1800 found=1776 no_candidate=24 invalid_start=0 mismatches=0`; `NAVRETREAT real: …` ve iki `NAVRETREAT perf …` satırı var.
  - K4 ✔ harita geçici taşındığında `NavRetreat_` rc=0, `_RealMap`/`_Perf` `SKIPPED`, diğer altısı geçer; dosya geri kondu (1579030 bayt).
  - K5 ✔ `./tools/run-tests.sh Debug` derleme dahil rc=0, `122 tests, 0 failed`.
  - K6 ✔ Tablo 1–4 testleri `[ OK ]`; referans 1800 sorgu uyuşmazlık 0; gerçek harita R-A solo `cell=(159,228) cand=2799`, party `cell=(177,229) cand=245`, R-B `cand=2783 min_melee_m=8.94`, R-C `cell=(320,271) forb=22`, R-D `stay=1`; tarama 30 sorgu `mismatches=0`.
  - K7 ✔ (Release, `nproc=16`; üç koşu tutarlı): `NAVRETREAT perf set=party queries=1000 found=994 nocandidate=6 invalidstart=0 expanded_p50=202 expanded_p95=268 ms_p50=0.037 ms_p95=0.049 ms_p99=0.054` (kapı ≤ 0,500); `NAVRETREAT perf set=solo queries=1000 found=1000 nocandidate=0 invalidstart=0 expanded_p50=2101 expanded_p95=2954 ms_p50=0.388 ms_p95=0.568 ms_p99=0.635` (kapı ≤ 3,000).
  - K8 ✔ saflık grep'i boş; `git status` yalnızca §4'teki dört dosya + plan dosyası (bkz. plandan sapmalar).
  - K9 ✔ `Nav_` 10/10, `NavPath_` 9/9 (`near64 found=997 expanded_p50=306 expanded_p95=2431`), `NavSmooth_` 8/8, `NavTrack_` 10/10, `NavReach_` 8/8 (`components=143 largest=88279 pockets=229`), `NavDanger_` 8/8 (`elm_forbid=1594 … elm_safe=1591 aria A->B 660.617/689.103`) tümü değişmeden `[ OK ]`.
- Plandan sapmalar ve gerekçeleri:
  1. `NavRetreat_Params` (d) ve `NavRetreat_Zones` karşılaştırmalarında `CheckResult`'a katman/tehdit bağımsız iletildi (plan davranışı aynı).
  2. **`NavRetreat_RealMap` R-B `min_melee_m`:** plan §5.2 "beklenen 12,0, ±0,01" yazıyor, ancak **K6 "`min_melee_m≥8`"** diyor. Ölçülen **8,94** (`sqrt(80)`); aynı hücre/uzunluk (112,284 = planla birebir) ve aynı puan (2,08865) ile 8 m diskini sıyıran en kısa yolun minimum yaklaşması budur; 12,0 bu uzunlukla matematiksel olarak olanaksız (diski 12 m'den uzak dolaşmak yolu uzatırdı), yani §5.2'deki sayı kendi K6'sıyla ve kendi uzunluğuyla çelişiyor. Testi bağlayıcı ölçüt olan `min_melee_m≥8`'e göre yazdım (kural gevşetilmedi; 8 m kuralı `StepAllowed` ile ayrıca sınanıyor).
  3. **En iyi aday seçimi:** plan §5.1 madde 7 "`s > bestScore` (kesin) yeterlidir" diyor; bu, `bestScore` 0 başlatılırsa negatif puanlı adayları hiç seçmez. Planın kendi Tablo 4/F (`-0,04142`) ve R-C (`-0,09771`) `Found` beklentileri için "ilk aday" bayrağı (`hasBest`) kullandım; beraberlik kuralı (daha kısa yol, sonra küçük indeks) korunuyor.
- Açık sorular:
  - §5.2 R-B `min_melee_m=12,0` beklentisi K6 ve ölçümle çelişiyor (yukarıda). Claude'un §5.2'yi `≥8` olarak düzeltmesi veya beklenen değeri teyit etmesi gerekir.
  - `NavRetreat_Matches_Reference` tohumları C++ `Rng` ile üretildiği için Python prototipinden farklı (found=1776 vs 1786); eşikler geniş (≥1500), uyuşmazlık 0.
  - Gerçek harita süre kapıları bu makinede (Ryzen 7 7800X3D, `nproc=16`) rahat; Debug'da solo p95 ≈ 13 ms (kapı yok).

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02-nav...bot/F5-07` @ `<sha>`
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
