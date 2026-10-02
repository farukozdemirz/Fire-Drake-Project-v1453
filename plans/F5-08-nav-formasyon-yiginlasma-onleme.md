# F5-08: Formasyon ve yığılma önleme (`BotCore/NavFormation.h`; kuşatma yuvaları + ayrışma vektörü + aralıklı seçim)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat, `docs/17` §1 "Paralel yürütülebilir işler") |
| Branch | `bot/F5-08` (taban: `gece/2026-10-02-nav`) |
| Bağımlı olduğu planlar | F5-01 (`BotCore/NavGrid.h`), F5-02 (`BotCore/NavPath.h`: `NavCell`), F5-03 (`BotCore/NavSmooth.h`: `NavLineClear`): `KAPANDI`, `gece/2026-10-02-nav` içinde (merge `788aa86`, `dc1bb10`, `08ffbc3`); F5-04 (`BotCore/NavTrack.h`: `NavRingCells`, yalnızca testte); bu planın testleri 122 testin üstüne eklenir |
| İlgili gereksinim / kabul | `docs/12` §9 (formasyon ve yığılmanın önlenmesi; bu plana göre güncellendi), T-NAV-08, MET-NAV-06; `docs/09` §8 (ön/orta/arka hat, ayrışma), `docs/07` §11 (iki priest ≥ 8 m); ADR-0006 Eki F5-08 (bu planın kararları), ADR-0016 (`BotCore` saflığı) |
| Tahmini büyüklük | S (4 dosya: 2 yeni, 2 proje satırı; ~1 000 satır, çoğu test) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Bir party hedefin etrafında toplandığında botlar aynı noktaya yığılmamalıdır (insan oyuncu üst üste durmaz; `docs/12` §9, MET-NAV-06). Bu plan bunun saf geometri/atama mantığını yazar; üç bağımsız araç:

1. **Kuşatma yuvaları:** hedefin etrafında 8 pusula yönünde, çağıranın verdiği yarıçapta yuva noktaları (AIServer'ın kuşatma yuvası fikri, `AIServer/AIUser.cpp:12-13`, `:71-121`; botlar için yeniden yazılır). `NavAssignSurroundSlots`: her üyeye ayrı bir yürünebilir yuva verir; atama yapışkandır (hedef hareket etse de üye yuvasını korur) ve belirlenimcidir.
2. **Ayrışma vektörü:** aynı party üyelerinden biri başka bir üyeye < 1,5 m ise karşılıklı itme (`NavSeparationVector`) ve bunu duvara/engele çarptırmadan uygulayan `NavApplySeparation`.
3. **Aralıklı seçim:** priest'ler arası ≥ 8 m gibi "başkalarından en az X m uzak" konum seçimi (`NavPickSpaced`, aday listesi çağırandan) ve MET-NAV-06'nın ham ölçüsü (`NavCountStackedPairs`).

T-NAV-08 ("8 kişilik party'nin hedef etrafında yığılmadan yerleşmesi") birim testi olarak gerçeklenir (düz ızgarada ve zone 71'de, duvar yanında).

Bu planın sonunda **rol dağılımı (hangi rol hangi yuvaya/halkaya), rol başına yarıçap, 8'den fazla üye için ikinci halka, `NavFollower` bağlaması (hedef = yuva noktası), hareketli hedefte yeniden yuvalama, düşman/party dışı çarpışma, telemetri (`NAV_*` olayları) ve sunucu entegrasyonu yoktur.** `GameServer/`, `AIServer/`, `shared/` değişmez; sunucu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0006-navigasyon-izgara-astar.md` — **Eki F5-08** (bu plan yazılırken eklendi): yuva düzeni, yapışkan + açgözlü atama, ayrışma formülü, bilinçli ertelenenler. Sapma gerekiyorsa dur ve sor.
- `docs/12_NAVIGATION_AND_POSITIONING.md` §9 (bu plana göre güncellendi), §11 (T-NAV-08), `docs/16` §MET-NAV-06, `docs/09` §8, `docs/07` §11.
- `BotCore/NavGrid.h` — kullanacağın API: `Walk(x, z)`, `CellOf(float)`, `CellCenter(int)`, `Size()`. **Değiştirme.**
- `BotCore/NavPath.h:21-26` — `NavCell` (`x`, `z`, `==`/`!=`). `BotCore/NavSmooth.h:17` — `NavLineClear(grid, a, b)` (`Walk` + her adım `EdgeOpen`). **Değiştirme.**
- `BotCore/NavTrack.h:69` — `NavRingCells(grid, cx, cz, ringMinM, ringMaxM, from, out)`: yalnızca testte aday listesi üretmek için kullanılır (başlık `NavFormation.h`'a **eklenmez**).
- `Tests/BotCoreTests/NavRetreatTests.cpp` — kopyalanacak yardımcı kalıbı (yeni dosyada kendi kopyalarını yaz; başka `.cpp`'den içe aktarma yok): `CellIndex`, `RingEvents`, `MakeNav`; gerçek harita yükleme + `SKIPPED` kalıbı (`NavRetreat_RealMap`).
- `BotCore/Rng.h` — testlerde `BotCore::Rng` (`NextBelow(uint32_t)`); global `rand()` yasak. `Tests/BotCoreTests/MiniTest.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`; kayan nokta için `CHECK(std::fabs(a - b) <= eps)`), `tools/run-tests.sh`.

Planı yazarken doğrulanan gerçekler (C++ prototipi, `g++ -std=c++17`, gerçek `BotCore` başlıklarıyla, 2026-10-02, depoya girmeyen geçici; **aynı kurallar, aynı formüller**; sayılar test beklentileridir; MSVC ile son bit farkları belirtilen toleransların çok altındadır). "Düz 40×40" = `RingEvents(40)` (kenar engelli, iç açık), `unit = 4`, yükseklik 0 (hücre `(x, z)` merkezi `((x + 0.5) * 4, (z + 0.5) * 4)`). Yuva yarıçapı bu planın testlerinde her yerde **2,5 m** (`[A]`; komşu yuvalar arası `2·2,5·sin(22,5°)` = **1,91342 m** ≥ 1,5 m ayrışma eşiği, yani yerleşmiş formasyon itilmez).

**Tablo 1 — yuva geometrisi** (`NavSurroundPoint(s, 82, 82, 2.5, …)`, `h = 0.70710678f`):

| Yuva | Yön `(dx, dz)` | Nokta |
|---|---|---|
| 0 | `(0, 1)` | `(82, 84.5)` |
| 1 | `(-h, h)` | `(80.23223, 83.76777)` |
| 2 | `(-1, 0)` | `(79.5, 82)` |
| 3 | `(-h, -h)` | `(80.23223, 80.23223)` |
| 4 | `(0, -1)` | `(82, 79.5)` |
| 5 | `(h, -h)` | `(83.76777, 80.23223)` |
| 6 | `(1, 0)` | `(84.5, 82)` |
| 7 | `(h, h)` | `(83.76777, 83.76777)` |

Aralık dışı yuva (`-1`, `8`, `100`): yön `(0, 0)`, nokta = merkez. Her komşu çift (7→0 dahil) arası uzaklık **1,91342** (`1e-4`). Kullanılabilirlik (`NavSurroundUsable`: yuva noktasının hücresi `Walk`): düz 40×40 hedef `(82, 82)` ⇒ sekiz yuva da `true`; hedef `(6, 6)` (hücre `(1,1)`, köşe) yarıçap 2,5 ⇒ `{1,1,0,1,0,1,1,1}` (yuva 2 ve 4 duvar hücresinde), yarıçap 4,0 ⇒ `{1,0,0,0,0,0,1,1}`; aralık dışı yuva `false`.

**Tablo 2 — atama** (düz 40×40, hedef `(82, 82)`, yarıçap 2,5, `slots` girişi `-1`, aksi belirtilmedikçe):

| Senaryo | Üyeler | Beklenen `slots`, dönüş |
|---|---|---|
| perm | `i = 0..7`, üye `i` yuva `(i*3)%8` noktasının `(+0.1, -0.1)` ötesinde | `{0,3,6,1,4,7,2,5}`, **8** |
| üç üye | `(82,100)`, `(82,60)`, `(120,82)` | `{0,4,6}`, **3** |
| on üye | `i = 0..7`: yuva `i` noktası `+(0.1, 0.1)`; üye 8 `(82,100)`, üye 9 `(60,82)` | `{0,1,2,3,4,5,6,7,-1,-1}`, **8** |
| yapışkan | 5 üye hepsi `(60,82)`; giriş `{3,3,9,-1,2}` | `{3,1,0,4,2}`, **5** (üye 0 yuva 3'ü, üye 4 yuva 2'yi korur; üye 1'in tekrarlı 3'ü ve üye 2'nin geçersiz 9'u atılır; üye 1 en yakın boş yuva 1'i, üye 2 yuva 0 ile 4 **tam eşit** uzaklıkta (`d² = 490.25`) olduğundan küçük yuva indeksi 0'ı, üye 3 yuva 4'ü alır) |
| köşe | hedef `(6, 6)`; 8 üye hepsi `(40, 40)` | `{7,0,6,1,5,3,-1,-1}`, **6** (kullanılamaz yuva 2 ve 4 verilmez; aynı konumdaki üyelerde eşitlik üye indeksi, eşit yuva uzaklığında yuva indeksi ile bozulur) |
| kullanılamaz giriş | hedef `(6, 6)`; üye 0 `(20, 6)`, üye 1 `(20, 4)`; giriş `{2, 4}` (ikisi de kullanılamaz) | `{6,5}`, **2** |
| sıfır/geçersiz | `count == 0`; `members == nullptr`; `slots == nullptr` (count > 0) | dönüş **0**, `slots` dokunulmaz |
| negatif yarıçap | düz 40×40, hedef `(82, 82)`, üyeler `(60,82)`, `(104,82)`, `(82,60)`; yarıçap `-3` ve `0` | ikisinde de `{0,1,2}`, **3** (negatif 0 sayılır; tüm yuva noktaları merkezle çakışır, her üye için tüm yuvalar eşit uzaklıkta, eşitlik üye sonra yuva indeksiyle bozulur) |

**Tablo 3 — ayrışma** (varsayılan `NavSeparationParams`: `minDistM` 1,5, `maxPushM` 1,0; vektörler `1e-4`):

| Senaryo | Beklenen |
|---|---|
| iki üye `(80,80)`, `(81,80)` (d = 1) | `v0 = (-0.25, 0)`, `v1 = (+0.25, 0)` (her biri `0,5 · (minDist − d)`; ikisi birden uygulanınca uzaklık tam **1,5**) |
| d = 1,5 tam (`(80,80)`, `(81.5,80)`) | `v0 = (0, 0)` (eşik ve üstü itmez) |
| çakışık iki üye `(80,80)` ×2 | `v0 = (-0.53033, +0.53033)`, `v1 = (+0.53033, -0.53033)` (yön = `NavSurroundDir((i + j) % 8)`, işaret `i < j ? + : −`, büyüklük `0,5 · minDist` = 0,75) |
| üç üye `(80,80)`, `(81,80)`, `(80,81)`, üye 0 | `(-0.25, -0.25)` (uzunluk 0,35355) |
| aynı üç üye, üye 1 | `(0.28033, -0.03033)` |
| aynı, `maxPushM = 0,1`, üye 0 | `(-0.07071, -0.07071)` (yön korunur, uzunluk kırpılır); `maxPushM = 0` ve `-1`: `(0, 0)` |
| üç çakışık üye `(80,80)` ×3 | `v0 = (-0.92388, 0.38268)`, `v1 = (0, -1)`, `v2 = (0.92388, 0.38268)` (toplam 1,0 m'ye kırpılır) |
| `self >= count` (ör. 3 üyede 3) | `(0, 0)`; `minDistM <= 0` ⇒ `(0, 0)` |
| `NavApplySeparation`, düz 40×40 | `(4.5,80)+(-1,0.3)` ⇒ **true**, `(4.5, 80.3)` (tam adım duvar hücresine düşer, yalnızca-z adımı kayarak uygulanır); `(4.5,80)+(-1,0)` ⇒ false, konum değişmez; `(80,80)+(0,0)` ⇒ false; `(80,80)+(0.25,-0.5)` ⇒ true, `(80.25, 79.5)`; `(2.0,80)+(0.25,0)` (başlangıç duvar hücresinde) ⇒ false; `(81.9,80)+(0.3,0)` (hücre sınırını geçer) ⇒ true, `(82.2, 80)` |
| `NavCountStackedPairs`, `{(80,80),(80.5,80),(81,80),(100,100)}` | eşik 1,5 ⇒ **3**, 1,0 ⇒ **2**, 0,5 ⇒ **0** (strict `<`); eşik ≤ 0 ve üye sayısı 0/1 ⇒ 0 |

**Tablo 4 — aralıklı seçim** (düz 40×40; aday listesi `NavRingCells(grid, 82, 82, 12, 16, from = (20,20), c)`: **24** hücre, sıra `(17,20) (20,17) (20,23) (23,20) (17,19) (17,21) (19,17) (19,23) (21,17) (21,23) (23,19) (23,21) (17,18) …`, `c[20] = (16,20)`):

| Senaryo | Beklenen indeks |
|---|---|
| diğerleri `{(82,82)}`, `minSepM = 8` | **0** (hücre `(17,20)`, uzaklık 12) |
| `minSepM = 13` | **12** (hücre `(17,18)`, merkez `(70,74)`, uzaklık 14,42; önceki 12 adayın hepsi < 13) |
| `minSepM = 100` (hiçbiri sağlamaz) | **20** (hücre `(16,20)`: en büyük "en yakın diğerine uzaklık" = 16, dört eşit adaydan en erken) |
| diğerleri `{(82,82), (70,82)}` (ikincisi `c[0]` hücre merkezi), `minSepM = 8` | **1** (hücre `(20,17)`) |
| `others == nullptr`, `otherCount == 0` | **0** |
| aday listesi boş | **-1** |
| priest: aday `NavRingCells(grid, 82, 82, 4, 10, from = (20,20), c2)` (**20** hücre, `c2[0] = (19,20)` merkez `(78,82)`), diğer priest `{(70,82)}`, `minSepM = 8` | **0** (uzaklık tam 8,0: **sınır dahil**); `minSepM = 8.01` ⇒ **1** (hücre `(20,19)`, uzaklık 12,65) |

**Tablo 5 — yerleşme (T-NAV-08)**, test içi `SettleSim` (§5.2): 8 üye `(sx + 0.2*(i%4), sz + 0.2*(i/4))` (hepsi 0,6 m içinde; başlangıçta çiftlerin **28**'i < 1,5 m), her tick: yapışkan atama → yuvaya doğru 1,5 m (6 m/s × 0,25 s `[A]`) düz adım → ayrışma (anlık görüntüden vektör, `NavApplySeparation`):

| Senaryo | Beklenen |
|---|---|
| **Düz 40×40**, hedef `(100, 80)`, başlangıç `(40, 80)`, 80 tick | tick 1: atama **8**, yuvalar `{5,4,3,2,6,7,0,1}` (bilgi); `tick ≥ 3` için çift sayısı (< 1,0 m) en çok **0**; yerleşme tick'i (tüm üyeler yuvasına < 0,05 m) **43**; son: her üye kendi yuvasında, en küçük çift uzaklığı **1,91342** (`1e-3`), çift sayısı (< 1,499) **0**; yuva değişmedi; duvar hücresine giren yok |
| **Zone 71**, hedef hücre `(315, 216)` merkezi `(1262, 866)` (duvar yanı: yuva 2 ve 4 kullanılamaz), başlangıç `(1273, 897)` (hücre `(318, 224)`'ün merkezinden `(-1, -1)`; `NavLineClear` `true`), 40 tick | tick 1: atama **6**, yuvalar `{0,7,1,6,5,3,-1,-1}` (bilgi), yuvasız iki üye (6 ve 7) `-1`'de kalır; `tick ≥ 3` için çift sayısı (< 1,0 m) en çok **0**; yerleşme tick'i **26**; son: en küçük çift uzaklığı **1,5000** (`1e-3`; iki yuvasız üye başlangıçta kalır, birbirinden 1,5 m'ye ayrışır: `(1272.75, 897.2)`, `(1274.25, 897.2)`), çift sayısı (< 1,499) **0**; duvar hücresine giren yok |

**Zone 71 kullanılabilirlik dağılımı** (`build/nav/zone71.navgrid`; yarıçap 2,5; her `Walk` hücre hedef olarak, 8 yuvadan kaçı kullanılabilir): `Walk` hücre **88 508**; kullanılabilir yuva sayısı 8 ⇒ **72 459**, 7 ⇒ **11 568**, 6 ⇒ **4 137**, 5 ⇒ **344**, en az **5**, 0–4 yok. Her hücrede 8 üye hedefin merkezinde yığılıyken `NavAssignSurroundSlots` dönüşü `min(8, kullanılabilir)`'dır.

**Rastgele küçük haritalar** (`NavForm_Assign_Matches_Reference`, §5.2): 30 tohum × 10 sorgu = **300** sorgu; prototipte `mismatches=0`, `assigned == min(üye, kullanılabilir)` her sorguda, ortalama atanan **4,863** (bilgi; `Rng` dizisi §5.2'deki sırayla tüketilirse).

## 3. Kapsam

**Yapılacaklar**

- `BotCore/NavFormation.h`: `kNavSurroundSlots`, `NavFormPoint`, `NavSurroundDir`, `NavSurroundPoint`, `NavSurroundUsable`, `NavAssignSurroundSlots`, `NavSeparationParams`, `NavSeparationVector`, `NavApplySeparation`, `NavPickSpaced`, `NavCountStackedPairs` (§5.1).
- `Tests/BotCoreTests/NavFormationTests.cpp`: §5.2 yedi test (birim + sıralama tabanlı bağımsız referans + yerleşme simülasyonları + gerçek harita).
- `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` kayıtları (§5.3).

**Kapsam dışı (yapılmayacak)**

- Rol dağılımı (warrior/mage/priest hangi yuva/halka, rol başına yarıçap `P-WAR-*`/`P-MAG-PREF-RANGE`/`P-PRI-POS-BACK`, party merkezi, ön/orta/arka hat; F7 karar katmanı), 8'den fazla üye için ikinci halka (fazla üye `-1` alır; ne yapacağı çağıranın işidir), `NavFollower`/`NavReach` bağlama (hedef = yuva noktası), hareketli hedefte yuvaların yeniden hesabı ve histerezis, öncelik/yol verme, düşman ya da party dışı oyunculara ayrışma, zamana bağlı MET-NAV-06 ölçümü (`> 2 sn` süre; telemetri katmanı), `NAV_*` olayları, sunucu entegrasyonu.
- Yuva noktasına yaklaştırma yolu (A*), `NavSmoothPath`, tehlike/yasaklı katmanı: yuva "kullanılabilir" yalnızca yuva noktasının hücresinin `Walk` olmasıdır (yasaklı/tehlikeli bölge, yuvaya ulaşılabilirlik ve bileşen kontrolü çağıranın işidir: yuva noktası hücresi `NavReachJudge`/`NavPathfinder` ile sınanır).
- Yuva noktasını hücreye oturtma (snap) ya da kullanılamaz yuvanın yerine yakın yuva önerme: yok; kullanılamaz yuva verilmez.
- `NavPath.h`, `NavDanger.h`, `NavGrid.h`, `NavReach.h`, `NavTrack.h`, `NavSmooth.h`, `NavRetreat.h` **değişmez**.
- `GameServer/`, `AIServer/`, `shared/`, `docs/` değişikliği, DB, sunucu çalıştırma. `GameServer/proj-GameServer.vcxproj` değişmez.
- İş parçacığı güvenliği: işlevler durumsuzdur ve yalnızca girdi/çıktı bellek bölgelerini kullanır (heap ayırması yok).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavFormation.h` | yeni | ASCII, CRLF, başlık-yalnızca (satır içi); yalnızca `NavSmooth.h` (→ `NavPath.h`, `NavGrid.h`) + standart başlıklar |
| `Tests/BotCoreTests/NavFormationTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="NavFormation.h" />` satırı (BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="NavFormationTests.cpp" />` satırı (BOM ve CRLF korunur) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`build/nav/zone71.navgrid` üretilen çıktıdır, commit edilmez; yoksa `python3 tools/nav-export.py` ile üret.)

## 5. Uygulama adımları

### 5.1 `BotCore/NavFormation.h`

Ad alanı `BotCore`; `#include "NavSmooth.h"` (aynı dizin, tırnaklı; `NavCell`, `NavGrid`, `NavLineClear` gelir) ve `<cmath> <cstddef> <vector>`. Stil `NavSmooth.h`/`NavRetreat.h` gibi: tab girinti, Allman, İngilizce yorum, `inline` tanımlar, değişebilir global/`static` durum yok (sabit tablo için işlev içi `static constexpr float tbl[8]` serbest), `#pragma once`. Dosya başına kısa bir açıklama yorumu (F5-08, `docs/12` §9, ADR-0006 Eki F5-08; saf mantık).

```cpp
namespace BotCore
{
	constexpr int kNavSurroundSlots = 8;

	struct NavFormPoint { float x = 0.0f; float z = 0.0f; };       // world metres

	// Unit direction of surround slot `slot` (0..7); (0, 0) for any other value. Table (dx, dz):
	// 0 (0,1)  1 (-h,h)  2 (-1,0)  3 (-h,-h)  4 (0,-1)  5 (h,-h)  6 (1,0)  7 (h,h), h = 0.70710678f.
	inline void NavSurroundDir(int slot, float & dx, float & dz);
	// x = cx + dx * radiusM, z = cz + dz * radiusM (radiusM is NOT clamped here; same float expression for x and z).
	inline void NavSurroundPoint(int slot, float cx, float cz, float radiusM, float & x, float & z);
	// False for an out-of-range slot; otherwise grid.Walk(CellOf(x), CellOf(z)) of the slot point.
	inline bool NavSurroundUsable(const NavGrid & grid, int slot, float cx, float cz, float radiusM);

	// slots[i] (in/out, `count` entries) is the slot of members[i] (-1 = none). Returns the number of
	// members that end with a slot (kept + newly assigned). Rules (all deterministic):
	//  0. count == 0, members == nullptr or slots == nullptr: return 0, nothing touched. radiusM < 0 is 0.
	//  1. Usable slots: NavSurroundUsable for the target (tx, tz).
	//  2. Keep pass (i ascending): a member keeps slots[i] when it is in 0..7, usable and not already
	//     kept; every other member's slots[i] becomes -1.
	//  3. Greedy pass: repeatedly pick, among members with slots[i] < 0 and free usable slots, the pair
	//     with the smallest squared distance member -> slot point (float, dx*dx + dz*dz); ties: smaller
	//     member index first, then smaller slot index (strict `<` while scanning i ascending, s ascending).
	//     Stop when no pair is left. No allocation: "has a slot" is slots[i] >= 0.
	inline int NavAssignSurroundSlots(const NavGrid & grid, float tx, float tz, float radiusM,
		const NavFormPoint * members, size_t count, int * slots);

	struct NavSeparationParams
	{
		float minDistM = 1.5f;     // [O] docs/12 s9: members closer than this push each other apart
		float maxPushM = 1.0f;     // [A] per application; <= 0 disables the push
	};

	// Push of members[self] away from every other member closer than minDistM; (vx, vz) = (0, 0)
	// when self >= count or minDistM <= 0 or maxPushM <= 0. For each other member j (ascending):
	//  - d2 >= minDistM^2: no push (the threshold itself does not push).
	//  - d2 <= 1e-8f (coincident): direction NavSurroundDir((self + j) % 8) times (self < j ? +1 : -1),
	//    magnitude 0.5 * minDistM  (antisymmetric: i and j get opposite vectors, deterministic).
	//  - else: unit vector (self - j) / d times 0.5 * (minDistM - d)  (a pair that both apply ends at
	//    exactly minDistM).
	// The sum is clamped to maxPushM (direction kept). Vectors are computed from the same snapshot.
	inline void NavSeparationVector(const NavFormPoint * members, size_t count, size_t self,
		const NavSeparationParams & params, float & vx, float & vz);

	// Moves (x, z) by (vx, vz) when that stays walkable. Tries, in this order, the full move, the
	// x-only move, the z-only move (sliding along a wall); a candidate with both components 0 is
	// skipped; a candidate is accepted when its cell equals the current cell and that cell is Walk,
	// or NavLineClear(current cell, candidate cell). Returns true and the new position on the first
	// accepted candidate; otherwise false and (nx, nz) = (x, z).
	inline bool NavApplySeparation(const NavGrid & grid, float x, float z, float vx, float vz,
		float & nx, float & nz);

	// Index into `candidates` of the first cell whose centre (grid.CellCenter) is >= minSepM
	// (inclusive) from every `others` point (float sqrt of dx*dx + dz*dz); when none qualifies, the
	// candidate whose nearest other point is farthest (earliest wins ties, strict `>`); -1 when
	// `candidates` is empty. otherCount == 0 (others may be nullptr): index 0.
	inline int NavPickSpaced(const NavGrid & grid, const std::vector<NavCell> & candidates,
		const NavFormPoint * others, size_t otherCount, float minSepM);

	// Number of member pairs (i < j) closer than thresholdM (strict `<`, squared compare); 0 for
	// thresholdM <= 0 (checked explicitly: a negative threshold squared would otherwise count pairs)
	// or count < 2. MET-NAV-06 raw measure (the "for more than 2 s" part is the caller's).
	inline int NavCountStackedPairs(const NavFormPoint * members, size_t count, float thresholdM);
}
```

**Kesin kurallar:** (1) Yukarıdaki yorumlardaki kurallar bağlayıcıdır; sayısal beklentiler (Tablo 1–5) bu ifadelerle üretilmiştir (aynı `float` ifadeleri: `cx + dx * radiusM`, `d2 = dx*dx + dz*dz`; `std::sqrt` `float`). (2) Üye/aday sayıları küçüktür (party ≤ 8 + yedek): `NavAssignSurroundSlots` `O(count · 8 · 8)`, ek veri yapısı yok. (3) `NavPickSpaced` aday hücrenin `Walk` olduğunu denetlemez (aday listesi `NavRingCells` gibi yürünebilir hücreler verir). (4) `NavApplySeparation` başka üyelerle çakışmayı denetlemez (ayrışma zaten onların vektörünü de uygular). (5) MSVC Level 4 uyarıları `static_cast` ile çözülür (`#pragma warning` ekleme); kullanılmayan parametre/değişken bırakma. (6) Özyineleme yok; NaN/sonsuz girdi denenmez.

### 5.2 `Tests/BotCoreTests/NavFormationTests.cpp`

`#include "MiniTest.h"`, `<BotCore/NavGrid.h>`, `<BotCore/NavPath.h>`, `<BotCore/NavSmooth.h>`, `<BotCore/NavTrack.h>`, `<BotCore/NavFormation.h>`, `<BotCore/Rng.h>`, `<algorithm> <cmath> <cstdint> <cstdio> <tuple> <vector>`. Anonim ad alanında yardımcılar (kendi kopyaların; **kullanılmayan yardımcı tanımlama**, MSVC C4505 uyarısı çıkar): `CellIndex`, `Cell`, `RingEvents`, `MakeNav` (F5-07 testleriyle aynı), ve:

- `NavFormPoint Pt(float x, float z)`.
- `float MinPair(const std::vector<NavFormPoint> &)`: en küçük çift uzaklığı (`std::sqrt` ile `float`; < 2 üye için büyük bir değer).
- `std::vector<int> RefAssign(const NavGrid &, float tx, float tz, float r, const std::vector<NavFormPoint> &, std::vector<int> prev, int & assigned)`: **bağımsız referans** (üretimle kod paylaşmaz; **sıralama tabanlı**): kendi 8 yönlü tablosu (`h = 0.70710678f`; `sx = tx + dx[s] * r`, aynı `float` ifadesi), `usable[s] = grid.Walk(grid.CellOf(sx), grid.CellOf(sz))`, `r < 0` ise 0; yapışkan geçiş (üretimdeki kural 2); kalan üyeler × boş kullanılabilir yuvalar için `std::tuple<float, int, int>(d2, i, s)` listesi, `std::sort`, sırayla: üye yuvasız ve yuva boşsa ata.
- `struct SettleResult { std::vector<NavFormPoint> pos; std::vector<int> firstSlots; int firstAssigned; int settleTick; int maxStacked1m; bool offWalk; bool slotsChanged; int initialStacked; }` ve `SettleResult SettleSim(const NavGrid & grid, float tx, float tz, float sx, float sz, int ticks)`; sabitler `n = 8`, `radius = 2.5f`, `kStepM = 1.5f`, varsayılan `NavSeparationParams`. Başlangıç: `pos[i] = (sx + 0.2f * (i % 4), sz + 0.2f * (i / 4))`, `slots` hepsi `-1`, `initialStacked = NavCountStackedPairs(pos, n, 1.5f)`. Her tick `t = 1..ticks`: (1) `NavAssignSurroundSlots(...)`; `t == 1` ise `firstSlots`, `firstAssigned` kaydedilir; `slots != firstSlots` ise `slotsChanged = true`; (2) yuvası olan her üye için yuva noktasına `d = std::sqrt(dx*dx + dz*dz)`: `d <= kStepM` ise tam oraya, değilse `pos += (goal - pos) / d * kStepM`; yuvasız üye yerinde kalır; (3) `snapshot = pos`; her `i` için `NavSeparationVector(snapshot, n, i, params, vx, vz)` ve `NavApplySeparation(grid, pos[i].x, pos[i].z, vx, vz, nx, nz)` doğruysa `pos[i] = (nx, nz)`; (4) ölçümler: `t >= 3` ise `maxStacked1m = max(maxStacked1m, NavCountStackedPairs(pos, n, 1.0f))`; herhangi bir üyenin hücresi `Walk` değilse `offWalk = true`; yuvası olan üyelerin yuva noktasına en büyük uzaklığı `< 0.05f` ise ve `settleTick < 0` ise `settleTick = t` (başlangıç `-1`).

Her testte düz ızgara `MakeNav(40, 4.0f, RingEvents(40), height_zeros)`.

| Test adı | İçerik |
|---|---|
| `NavForm_Slots` | **Tablo 1**: `kNavSurroundSlots == 8`; yön tablosu (`1e-6`) ve aralık dışı yuvalar (`-1`, `8`, `100` ⇒ `(0,0)`, nokta = merkez); yuva noktaları `(1e-4)`; komşu çift uzaklıkları **1,91342** (`1e-4`) ve `>= 1.5`; kullanılabilirlik (düz 40×40 hedef `(82,82)` sekiz `true`; hedef `(6,6)` yarıçap 2,5 ⇒ `{1,1,0,1,0,1,1,1}`, yarıçap 4,0 ⇒ `{1,0,0,0,0,0,1,1}`; aralık dışı yuva `false`). |
| `NavForm_Assign` | **Tablo 2** satırlarının hepsi (perm, üç üye, on üye, yapışkan, köşe, kullanılamaz giriş, sıfır/geçersiz, negatif yarıçap); her satırda atanan yuvalar ikişer ikişer farklı, `0..7` aralığında ve kullanılabilir, `-1` sayısı `count - dönüş`. |
| `NavForm_Assign_Matches_Reference` | 30 tohum (`seed = 0..29`), `Rng rng(6000u + seed)`, `n = 24`; harita: önce tüm hücreler `x` artan `z` artan: kenar hücre engelli (rastgelelik harcanmaz), iç hücre `rng.NextBelow(100) < 8` ise engelli, değilse açık; sonra yükseklik her hücre `x` artan `z` artan `rng.NextBelow(5) == 0 ? 6.0f : 0.0f`; `unit = 4`; `Build()`. `Walk` hücre listesi (x-ana) < 2 ise tohum atlanır. Her tohumda 10 sorgu: hedef hücre `walk[rng.NextBelow(count)]` merkezi; yarıçap `{1.5f, 2.5f, 4.0f}[rng.NextBelow(3)]`; üye sayısı `rng.NextBelow(13)` (0..12); her üye için sırayla `x = NextBelow(960) / 10.0f`, `z = NextBelow(960) / 10.0f`, giriş yuvası `(int)NextBelow(14) - 3`. Her sorguda üretim (girişin kopyasında) ve `RefAssign`: çıktı dizileri ve dönüş **eşit**; `dönüş == min(üye, kullanılabilir yuva sayısı)`; atanan yuvalar farklı ve kullanılabilir. `REQUIRE(trials >= 270)`, `mismatches == 0`. Yazdır: `NAVFORM random assign: trials=<n> mismatches=<n> avg_assigned=<x.xxx>` (prototip: `trials=300 mismatches=0 avg_assigned=4.863`). |
| `NavForm_Separation` | **Tablo 3**'ün tamamı (vektörler, kırpma, çakışıklar, `self >= count`, `minDistM <= 0`, `NavApplySeparation` altı satır, `NavCountStackedPairs` üç eşik + sınır durumları). Ek: (a) d = 1 çiftine her iki vektör uygulanınca yeni uzaklık **1,5** (`1e-4`); çakışık çifte de aynı (**1,5**); (b) **karşılıklılık:** `Rng rng(7000u)`, 200 çift, üye 0 `(80, 80)`, üye 1 `(80 + NextBelow(200)/100.0f - 1, 80 + NextBelow(200)/100.0f - 1)` (her iki fark ekseni `[-1, 0,99]`; hepsi d < 1,5; prototipte 200 çiftin hiçbiri çakışık değil, çakışık durum yukarıdaki satırlarda): `v0 == -v1` (`1e-5`, her iki bileşen; prototip: ihlal 0). |
| `NavForm_PickSpaced` | **Tablo 4**'ün tamamı (aday listelerinin boyutu ve ilk elemanı da `CHECK`): ring `[12,16]` 24 hücre, `c[0] == (17,20)`, `c[12] == (17,18)`, `c[20] == (16,20)`; ring `[4,10]` 20 hücre, `c2[0] == (19,20)`. |
| `NavForm_Settle_Flat` | **Tablo 5** düz satırı: `SettleSim(grid, 100, 80, 40, 80, 80)`. `REQUIRE`/`CHECK`: `initialStacked == 28`; `firstAssigned == 8`; `firstSlots` `{0..7}`'nin bir permütasyonu (bilgi: prototip `{5,4,3,2,6,7,0,1}`; **değerleri sabitleme**); `slotsChanged == false`; `maxStacked1m == 0`; `1 <= settleTick <= 60` (prototip 43); son `MinPair(pos)` `1.91342 ± 1e-3`; `NavCountStackedPairs(pos, 8, 1.499f) == 0`; `offWalk == false`; **belirlenim:** aynı çağrıyı ikinci kez koşup `pos` dizilerinin bit düzeyinde `==` olduğunu denetle. Yazdır: `NAVFORM settle flat: assigned=<n> settle_tick=<n> max_stacked_1m=<n> final_min_pair=<x.xxxx>`. |
| `NavForm_Settle_RealMap` | Gerçek harita (aşağıya bak). |

**Gerçek harita ortak kuralı** (F5-02..F5-07 ile aynı): dosya yolu `build/nav/zone71.navgrid` (çalışma dizini depo kökü); açılamazsa testi **başarısız yapma**: `std::printf("NAVFORM real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n")` yaz ve dön. `LoadFile` + `Build()` (varsayılan `NavParams`) sonrası `MainComponentCells() == 88508` `REQUIRE` edilir.

`NavForm_Settle_RealMap`:

- **Dağılım:** her `Walk` hücre (x-ana tarama) için hedef = hücre merkezi, yarıçap 2,5: kullanılabilir yuva sayısı `u`; sayaçlar; 8 üye hedef merkezinde (`NavFormPoint` hepsi aynı), `slots` hepsi `-1`: `NavAssignSurroundSlots` dönüşü `== min(8, u)`. Beklenen: `Walk` **88508**; `u = 8` ⇒ **72459**, `7` ⇒ **11568**, `6` ⇒ **4137**, `5` ⇒ **344**, `u < 5` ⇒ **0**; dönüş ihlali **0**.
- **Duvar yanı yerleşme:** `SettleSim(grid, 1262.0f, 866.0f, 1273.0f, 897.0f, 40)`; önce `NavLineClear(grid, Cell(318,224), Cell(315,216))` `REQUIRE` ve hedef için `NavSurroundUsable` maskesi `{1,1,0,1,0,1,1,1}`. `CHECK`: `firstAssigned == 6`; `firstSlots`'ta tam iki `-1` (üye 6 ve 7), diğer altısı farklı ve kullanılabilir (yuva 2 ve 4 yok; bilgi: prototip `{0,7,1,6,5,3,-1,-1}`); `slotsChanged == false`; `maxStacked1m == 0`; `1 <= settleTick <= 40` (prototip 26); `MinPair(pos) >= 1.499`; `NavCountStackedPairs(pos, 8, 1.499f) == 0`; yuvasız iki üyenin son konumu başlangıç noktasına (`(1273, 897)`) ≤ 3 m uzakta; `offWalk == false`.
- Yazdır: `std::printf("NAVFORM real: walk=%d usable8=%d usable7=%d usable6=%d usable5=%d usable_lt5=%d assign_violations=%d; settle assigned=%d settle_tick=%d max_stacked_1m=%d final_min_pair=%.4f unassigned=%d\n", ...)`.
- Not (planın doğruladığı sayı tutmazsa): sayı beklentiden saparsa **kuralı sayıya uydurma**; dur ve Uygulayıcı Raporu'nda sor.

### 5.3 Proje dosyaları

1. `BotCore/BotCore.vcxproj`: `ClInclude` grubuna `<ClInclude Include="NavFormation.h" />` (`NavRetreat.h` satırından sonra, `Perception.h` satırından önce; dosyada şu an `:80-81`). BOM/CRLF korunur.
2. `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubuna `<ClCompile Include="NavFormationTests.cpp" />` (`NavRetreatTests.cpp` satırından sonra, `PerceptionTests.cpp` satırından önce; şu an `:88-89`). BOM/CRLF korunur.
3. Bu iki projenin `.filters` dosyası yoktur (F5-01'de doğrulandı); ek dosya yok.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; `BotCore` ve `BotCoreTests` için **yeni uyarı yok** (Level 4; `NavFormation.h`, `NavFormationTests.cpp` dosyalarını `touch` ile zorla yeniden derle; çıktıda `NavFormation` geçen `warning` satırı bulunmaz).
- [ ] K2: `./tools/run-tests.sh Release --no-build --list` çıktısı şu yedi test adını içerir: `NavForm_Slots`, `NavForm_Assign`, `NavForm_Assign_Matches_Reference`, `NavForm_Separation`, `NavForm_PickSpaced`, `NavForm_Settle_Flat`, `NavForm_Settle_RealMap`.
- [ ] K3: `build/nav/zone71.navgrid` varken (yoksa `python3 tools/nav-export.py` ile üret) `./tools/run-tests.sh Release --no-build` çıkış kodu 0; çıktıda `129 tests, 0 failed` (önceki 122 + yedi yeni), tüm eski testler ve yedi yeni test `[ OK ]`; `SKIPPED` geçmiyor; `NAVFORM random assign: …` (`mismatches=0`), `NAVFORM settle flat: …` ve `NAVFORM real: …` satırları var.
- [ ] K4: Gerçek harita yokken (`build/nav/zone71.navgrid` geçici olarak başka ada taşınarak) `./tools/run-tests.sh Release --no-build NavForm_` çıkış kodu 0, `NavForm_Settle_RealMap` `SKIPPED` yazar ve diğer altı test geçer; ardından dosya yerine geri konur.
- [ ] K5: `./tools/run-tests.sh Debug` (derleme dahil) çıkış kodu 0 (Debug'da `assert`/sınır hataları yok).
- [ ] K6: Davranış sayıları (Release ve Debug): §2 Tablo 1–5'teki tüm değerler (`NavForm_Slots`, `_Assign`, `_Separation`, `_PickSpaced`, `_Settle_Flat` `[ OK ]`), `NavForm_Assign_Matches_Reference` (300 sorguda `mismatches=0`), `NavForm_Settle_RealMap` (`walk=88508 usable8=72459 usable7=11568 usable6=4137 usable5=344 usable_lt5=0 assign_violations=0`; `settle assigned=6 max_stacked_1m=0 unassigned=2`; `final_min_pair` 1,4990 ile 1,5010 arasında).
- [ ] K7: Saflık/kapsam: `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavFormation.h` çıktısı boş; `git diff --stat gece/2026-10-02-nav...bot/F5-08` yalnızca §4'teki dört dosyayı ve kendi plan dosyasını gösterir (`GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h`, `NavDanger.h`, `NavReach.h`, `NavTrack.h`, `NavSmooth.h`, `NavRetreat.h` yok).
- [ ] K8: F5-01..F5-07 davranışı bozulmadı: `./tools/run-tests.sh Release --no-build Nav_` (10 test), `NavPath_` (9), `NavSmooth_` (8), `NavTrack_` (10), `NavReach_` (8), `NavDanger_` (8), `NavRetreat_` (8) çıkış kodu 0, hepsi `[ OK ]` (**değişmeden**, testler düzenlenmez); `NAVPATH T-NAV-03 set=near64 …` satırında `found=997`, `expanded_p50=306 expanded_p95=2431`; `NAVDANGER real: elm_forbid=1594 elm_forbid_walk=1264 elm_safe=1591 elm_safe_walk=1232 …`, `NAVREACH real: components=143 largest=88279 pockets=229 …` ve `NAVRETREAT real: … cand=2799 …` satırlarında aynı sayılar.

## 7. Doğrulama komutları

```bash
git switch -c bot/F5-08 gece/2026-10-02-nav
python3 tools/nav-export.py            # build/nav/zone71.navgrid yoksa
./tools/build.sh Release
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Release --no-build NavForm_
./tools/run-tests.sh Release --no-build NavRetreat_
./tools/run-tests.sh Debug
grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavFormation.h
git diff --stat gece/2026-10-02-nav...bot/F5-08
```

## 8. Kısıtlar ve uyarılar

- **Paralel hat:** sunucuya hiç dokunma (`tools/run-servers.sh` çağırma; ana hat başka çalışma ağacından sunucu çalıştırıyor olabilir). DB'ye bağlanma. Dal tabanı `gece/2026-10-02-nav`.
- Kodlama/satır sonu: `AGENTS.md` §3. Yeni C++ dosyaları ASCII + CRLF + tab + Allman. Yorumlar İngilizce; plan metnindeki Türkçe açıklama koda girmez.
- Başlık-yalnızca: tüm tanımlar `inline`; değişebilir `static`/global durum yok; `#pragma` yalnızca `once`.
- **Belirlenim:** aynı girdiler her zaman aynı çıktıyı verir (rastgelelik yok). Eşitlikler yalnızca belirtilen sırayla (üye indeksi, sonra yuva indeksi; aday indeksi) bozulur; **eşitlik bozma kuralını testteki beklentiyi bozmadan "iyileştirme"** (ör. en iyi toplam mesafe eşlemesi / Macar algoritması) olarak değiştirme: açgözlü seçim bilinçli bir karardır (ADR-0006 Eki F5-08).
- Sayısal beklentiler (Tablo 1–5, 2,5 m, 1,5 m, 1,0 m, 1,5 m/tick) `[A]`/`[Ö]` değerleridir: parametre varsayılanlarını ve tablo sabitlerini değiştirme.
- `NavSurroundPoint` yarıçapı kırpmaz; `NavAssignSurroundSlots` negatifi 0 sayar (Tablo 2 son satır). `NavSurroundUsable` yalnızca yuva noktasının hücresine bakar (yasaklı bölge/ulaşılabilirlik değil).
- Yerleşme simülasyonu (`SettleSim`) **testin içindedir**; başlığa taşınmaz (gerçek hareket `ActionExecutor`/`NavFollower` işidir).
- Float: MSVC ve g++ arasında son bit farkları olabilir; testte eşitlik yerine belirtilen toleranslar kullanılır (yalnızca belirlenim testi aynı derleyicide bit düzeyinde karşılaştırır). Tablo 2'deki tam eşitlikler (yapışkan ve köşe satırları) aynı `float` ifadelerinin simetrik terimlerinden doğar (`cx + dx * r` x ve z için aynı ifade); beklenti tutmazsa **kuralı değiştirme**, dur ve raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: —
- Branch / commit'ler: —
- Değişen dosyalar ve neden: —
- Derleme sonucu (`tools/build.sh Release` son 10 satır): —
- Kabul kriterleri öz-değerlendirme: —
- Plandan sapmalar ve gerekçeleri: —
- Açık sorular: —

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

—
