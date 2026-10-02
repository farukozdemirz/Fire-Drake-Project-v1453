# F5-01: Navigasyon izgara veri modeli (`BotCore/NavGrid.h`) ve SMD → `.navgrid` dışa aktarma aracı

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat, `docs/17` §1 "Paralel yürütülebilir işler") |
| Branch | `bot/F5-01` (taban: `gece/2026-10-02-nav`) |
| Bağımlı olduğu planlar | Yok (`BotCore` + `BotCoreTests` zaten var: F3-05 `KAPANDI`) |
| İlgili gereksinim / kabul | `docs/12` §1, §2 (`walk`, `slope`, `clearance` katmanları), §6 (y = bilinear), CLI-08; AC-NAV-03 (engelli hücreye giriş = 0) için zemin; ADR-0016 (`BotCore` saflığı) |
| Tahmini büyüklük | M (6 dosya: 2 yeni, 4 değişen; ~450 satır) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Navigasyonun (A*, düzleştirme, takılma kurtarma, ...) üzerine oturacağı **sunucusuz** veri modelini kurmak:

1. `tools/nav-export.py`: zone 71 SMD'sini (`/mnt/c/dev/fdp/server/Map/freezone_a_20050718.smd`) okuyup `build/nav/zone71.navgrid` adlı sade bir ikili dosyaya yazar (olay + yükseklik ızgarası).
2. `BotCore/NavGrid.h` (başlık-yalnızca): bu dosyayı (veya bellekteki diziyi) yükler; `walk`, `clearance` katmanlarını ve kenar (eğim + köşe kesme yok) kuralını hesaplar; yükseklik için bilinear örnekleme verir.
3. `BotCoreTests` birim testleri: sentetik küçük ızgaralarla kural doğrulaması + gerçek harita testi (dosya varsa).

Bu planın sonunda **yol bulma yoktur** (A* F5-02'dir). Sunucu, `GameServer/`, `AIServer/`, `shared/` değişmez; sunucu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/12_NAVIGATION_AND_POSITIONING.md` §1 (doğrulanmış temel: 513×513 köşe, 4 m, `0 = engelli / 1 = açık`, indeks `x·513 + z`) ve §2 (katman tablosu). Bu plan yalnızca `walk`, `slope`, `clearance` satırlarını kapsar.
- `docs/appendix/tools/smd_parse.py` — `parse(path, load_warps=False)` sözlüğü: `m_nMapSize` (513), `m_fUnitDist` (4.0), `events` (`array('h')`, 263 169 eleman), `height` (`array('f')`, aynı uzunluk). `tools/arena-report.py:21-28` bu modülü `sys.path.append(os.path.join(ROOT, "docs", "appendix", "tools"))` + `from smd_parse import parse` ile içe aktarıyor: **aynı kalıbı kullan**.
- `docs/appendix/maps/smd_parse_output.txt` satır 8-12: haritanın referans istatistikleri (olay 0 = 29 522, olay 1 = 233 647, yükseklik −30,633 … 82,122 m).
- Sunucunun hücre eşlemesi: `shared/SMDFile.cpp:200-206` (`GetEventID(x, z)`: sınır dışı `-1`; indeks `m_ppnEvent[x * m_nMapSize + z]`) ve `GameServer/Map.cpp:109` (`(int)(x / GetUnitDistance())`). `BotCore::NavGrid::Event` aynı sözleşmeyi uygular.
- `BotCore/Rng.h` — testlerde rastgele ızgara için `BotCore::Rng` (belirlenimli) kullanılır.
- `Tests/BotCoreTests/MiniTest.h` (makrolar `TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`), `Tests/BotCoreTests/RngTests.cpp` (dosya biçimi örneği), `tools/run-tests.sh`.
- ADR-0016: `BotCore` yalnızca standart kütüphane; `windows.h`, `stdafx.h`, `shared/`, `GameServer/` başlığı **içermez**.

Planı yazarken doğrulanan gerçekler (uygulayıcı yeniden doğrulamak zorunda değil, ama sayılar kabul kriteridir):

- Olay ızgarası yalnızca `0` ve `1` içerir. 4-bağlantılı açık (`=1`) bileşenlerden en büyüğü (116 512 hücre) harita kenarına (`x` veya `z` ∈ {0, 512}) değiyor; **kenara değmeyen en büyük bileşen 88 508 hücre** (`docs/12` §1 ile aynı sayı) ve arena A noktası (1274, 890) ile B noktası (746, 1106) bu bileşendedir. Kenara değmeyen sonraki en büyük bileşen 4 344 hücredir.
- Yükseklik indeksi de `x·513 + z`: `smd_parse.py`'daki nesne olayı (1375, 1085) hücresinde yükseklik `h[x·513+z] = 11,52` (olay kaydı 11,8 m), transpoze indeksle `20,67`. Yani doğru yerleşim `x·n + z`.
- Komşu hücreler arası |Δh| kenar başına en çok 2,5 m'yi aşan 4 902 kenar var (ana bileşen içi, ortogonal, toplam 166 579 kenar) → eğim katmanı gerçekten kenar keser.

## 3. Kapsam

**Yapılacaklar**

- `tools/nav-export.py` (Python 3, yalnızca standart kütüphane): SMD → `.navgrid` ikili dosyası (§5.1 biçim), özet satırı, `--selftest`.
- `BotCore/NavGrid.h`: `NavParams`, `NavGrid` sınıfı (§5.2).
- `Tests/BotCoreTests/NavGridTests.cpp` (§5.3 test listesi).
- `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` kayıtları.
- `tools/run-tests.sh`: testi koşmadan önce depo köküne `cd` (gerçek harita testi `build/nav/zone71.navgrid` dosyasını göreli yoldan açar).

**Kapsam dışı (yapılmayacak)**

- A*, yol düzleştirme, hareketli hedef, ulaşılamaz hedef tespiti, LoS (F5-02..F5-10'un işi). `NavGrid` içine yol bulma, öncelik kuyruğu veya "en yakın açık hücre" araması **ekleme**.
- `danger_static`, `danger_dynamic`, `region_graph` katmanları (sonraki planlar). Karşı ulus tower halkası, arena sınırı, canavar spawn dikdörtgenleri okunmaz.
- Çarpışma geometrisi (N3ShapeMgr yüzleri, `los_mesh`) dışa aktarılmaz.
- DB'ye bağlanmak, sunucu çalıştırmak, `GameServer/`, `AIServer/`, `shared/` değiştirmek. `BotCore` `GameServer`'a bağlanmaz.
- `docs/appendix/tools/smd_parse.py` değiştirilmez (yalnızca içe aktarılır). `docs/` altına dokunulmaz.
- Zone 72 veya başka harita desteği: araç `--map` ile başka SMD'yi okuyabilir ama doğrulanan tek hedef zone 71'dir.
- Yeni `P-NAV-*` parametresi dosyası/okuyucusu: parametreler `NavParams` varsayılanlarıdır, ini/yaml okunmaz.
- `GameServer/proj-GameServer.vcxproj` değişmez (`BotCore` sunucuya bağlı değil).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/nav-export.py` | yeni | LF satır sonu, ASCII, çalıştırılabilir bit gerekmez (`python3 tools/nav-export.py` ile çağrılır) |
| `BotCore/NavGrid.h` | yeni | ASCII, CRLF, başlık-yalnızca (satır içi) |
| `Tests/BotCoreTests/NavGridTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="NavGrid.h" />` satırı (BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="NavGridTests.cpp" />` satırı (BOM ve CRLF korunur) |
| `tools/run-tests.sh` | değiştir | yalnızca testi çalıştırmadan önce `cd "$ROOT"` (LF) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`build/nav/zone71.navgrid` üretilen çıktıdır; `build*/` `.gitignore`'dadır, commit edilmez.)

## 5. Uygulama adımları

### 5.1 `.navgrid` ikili biçimi (araç yazar, `NavGrid::Load` okur)

Little-endian, hizalamasız, ardışık:

| Ofset | Boyut | Alan |
|---|---|---|
| 0 | 8 bayt | sihirli sayı: ASCII `FDPNAV01` |
| 8 | int32 | `n` (köşe sayısı; zone 71: 513) |
| 12 | float32 | `unit` (metre; zone 71: 4.0) |
| 16 | `2·n·n` bayt | `events[n·n]`, int16; indeks `x·n + z` (SMD'deki ham değerler) |
| 16 + 2·n·n | `4·n·n` bayt | `heights[n·n]`, float32; aynı indeks |

Zone 71 için toplam dosya boyutu `16 + 6·263 169 = 1 579 030` bayt. `Load` tam boyut ister (eksik de, fazla da `false`).

### 5.2 `BotCore/NavGrid.h`

Ad alanı `BotCore`; yalnızca `<cstdint> <cstddef> <cstring> <cmath> <vector> <fstream> <algorithm>` gibi standart başlıklar. Çevredeki kodla aynı stil: tab girinti, Allman, İngilizce yorum, `Perception.h`'deki isimlendirme.

```cpp
namespace BotCore
{
	struct NavParams
	{
		// P-NAV-MAX-SLOPE [A]: max |dh| / horizontal distance of one edge
		// (docs/12 s2: 2.5 m per 4 m cell = 0.625). Calibrated later by T-NAV-02.
		float maxSlope = 0.625f;
	};

	class NavGrid
	{
	public:
		// Takes ownership of the arrays; false if n < 2, unit <= 0 or an array size != n*n.
		bool Init(int n, float unit, std::vector<int16_t> events, std::vector<float> heights);
		// Parses the s5.1 format (exact size required); calls Init on success.
		bool Load(const uint8_t * data, size_t size);
		bool LoadFile(const char * path);          // reads whole file, then Load

		// Computes `walk` and `clearance`. Must be called after Init/Load; may be called again
		// with other params. Accessors below return false/0 before the first Build.
		void Build(const NavParams & params = NavParams());

		int   Size() const;                         // n
		float Unit() const;
		bool  InBounds(int x, int z) const;
		int   CellOf(float world) const;            // (int)floor(world / unit)
		float CellCenter(int i) const;              // (i + 0.5) * unit

		int16_t Event(int x, int z) const;          // -1 out of bounds (SMDFile::GetEventID contract)
		float   Height(int x, int z) const;         // vertex height; 0 out of bounds
		bool    Walk(int x, int z) const;           // false out of bounds
		uint8_t Clearance(int x, int z) const;      // 0 out of bounds / not walk
		bool    EdgeOpen(int x, int z, int dx, int dz) const;
		float   HeightAt(float wx, float wz) const; // bilinear over vertices, clamped
		int     MainComponentCells() const;         // size of the component Walk() is based on
		const NavParams & Params() const;
	};
}
```

Kurallar (her biri testle sabitlenir, §5.3):

1. **Olay anlamı:** yalnızca `event == 1` açık hücredir; `0` ve diğer her değer engelli (gerçek haritada yalnızca 0/1 var; `docs/12` §1).
2. **`walk`:** olay-1 hücreleri **4-bağlantılı** bileşenlere ayrılır (`(x±1, z)`, `(x, z±1)`); harita **kenarına değen** (`x` veya `z` ∈ {0, n−1} hücre içeren) bileşenler elenir; kalanlar arasında **en büyüğü** ana bileşendir; eşitlikte x-ana tarama sırasında (`x` dış, `z` iç döngü) ilk karşılaşılan. `Walk(x, z)` = hücre ana bileşende. Kenara değmeyen bileşen yoksa hiçbir hücre `walk` değildir (`MainComponentCells() == 0`). Bileşen etiketleme için yinelemeli (iteratif) BFS kullan; **özyineleme yok** (263 169 hücre yığını taşırır).
3. **`clearance`:** `Walk` olmayan (veya ızgara dışı) en yakın hücreye **Chebyshev** mesafesi (hücre cinsinden, çok kaynaklı BFS, 8 komşu; ızgara dışı engelli sayılır), `uint8_t`'ye 255'te doyurulur. `Walk` olmayan hücrede 0. Örnek: kenar halkası engelli 9×9 ızgarada `(1,1)`=1, `(2,2)`=2, `(3,3)`=3, `(4,4)`=4.
4. **`EdgeOpen(x, z, dx, dz)`** (`dx, dz ∈ {-1, 0, 1}`, ikisi birden 0 değil; aksi halde `false`): iki uç da `Walk` olmalı; `mesafe = unit` (ortogonal) veya `unit·√2` (çapraz); `|Δh| ≤ maxSlope · mesafe` olmalı (`Δh` iki hücrenin `Height` değerleri farkı); **çaprazda ayrıca** `(x+dx, z)` ve `(x, z+dz)` hücrelerinin ikisi de `Walk` olmalı (köşe kesme yok, `docs/12` §4.1). Sonuç **simetrik**: `EdgeOpen(a→b) == EdgeOpen(b→a)`.
5. **`HeightAt(wx, wz)`:** köşe (vertex) yüksekliklerinden bilinear; köşe `(i, j)` dünya konumu `(i·unit, j·unit)`; sorgu `[0, (n−1)·unit]` aralığına sıkıştırılır (clamp). Not: `Event` hücresi `[i·unit, (i+1)·unit)` aralığını kapsar (sunucu eşlemesi), `Height(i, j)` ise o hücrenin alt köşesindeki yüksekliktir; bu 2 m'ye varan yaklaşıklık `[Ö]` kabul edilmiştir, kenar eğim kuralı da bu köşe yüksekliklerini kullanır.
6. `Build` iki dizi ayırır (`walk`: `uint8_t`, `clearance`: `uint8_t`, her biri n·n); `Init` çağrılmadan `Build` güvenli bir işlem yapmaz (sessizce döner).

### 5.3 `Tests/BotCoreTests/NavGridTests.cpp`

`#include "MiniTest.h"` ve `<BotCore/NavGrid.h>`; testler küçük sentetik ızgaralar kurar (yardımcı: `MakeGrid(n, fill, ...)`). Test adları (kabul kriterleri bu adlara dayanır):

| Test adı | İçerik |
|---|---|
| `Nav_Init_Validation` | `n=1`, `unit=0`, yanlış dizi boyutu → `Init` false; geçerli → true |
| `Nav_Event_OutOfBounds` | `Event(-1,0)`, `Event(0,n)` = −1; `Walk`/`Clearance`/`EdgeOpen` sınır dışında false/0 |
| `Nav_Walk_MainComponent` | 12×12; kenar halkası engelli; iç bölgede büyük açık bileşen + duvarla ayrılmış küçük cep + kenara değen (kenar hücresi açık) daha büyük bir bileşen: yalnızca büyük iç bileşen `Walk`; `MainComponentCells()` elle sayılan değer; olay değeri 2 olan hücre `Walk` değil; kenara değmeyen bileşen yoksa `MainComponentCells()==0` |
| `Nav_Clearance_Ring` | 9×9, kenar halkası engelli: `(1,1)=1`, `(2,2)=2`, `(3,3)=3`, `(4,4)=4`, `(4,1)=1`, kenar hücresi 0 |
| `Nav_Clearance_BruteForce` | 24×24 `Rng(12345)` ile rastgele (~%25 engelli) ızgara + engelli kenar halkası; her hücre için `Clearance` = `Walk` olmayan hücreye Chebyshev mesafesi (ızgara dışı engelli) kaba kuvvet O(n⁴) hesabıyla birebir eşit (255'e doyurma dahil değil, küçük ızgarada gerekmez) |
| `Nav_Edge_Slope` | düz zemin → tüm ortogonal kenarlar açık; `h(x,z)=3·x` (0,75 > 0,625) → `x` yönü kapalı, `z` yönü açık; `h(x,z)=2.4·x` (0,6) → `x` yönü açık; `maxSlope` `Build(NavParams)` ile değiştirilince sonuç değişir; çapraz: `(1,1)`→`(2,2)` farkı 3,5 m açık, 3,6 m kapalı (diğer köşeler 0 m yükseklikte) |
| `Nav_Edge_NoCornerCutting` | çaprazın bir ortogonal komşusu engelli → `false`; ikisi de açık → `true` (düz zemin); simetri: tüm hücre/yön çiftleri için `EdgeOpen(a→b) == EdgeOpen(b→a)` (rastgele 16×16 ızgara) |
| `Nav_HeightAt_Bilinear` | `h(i,j) = 0.5·(i·unit) + 0.25·(j·unit)` düzleminde rastgele (belirlenimli) 50 noktada `HeightAt` düzleme `1e-3` içinde eşit; ızgara dışı sorgu kenar değerine sıkışır |
| `Nav_Load_Buffer` | testte §5.1 biçiminde bayt dizisi elle kurulur → `Load` true, `Size/Unit/Event/Height` eşleşir; yanlış sihirli sayı, bir bayt eksik, bir bayt fazla, `n=0` → false |
| `Nav_RealMap_Zone71` | aşağıya bak |

`Nav_RealMap_Zone71`:

- Dosya yolu: `build/nav/zone71.navgrid` (çalışma dizini depo kökü; `run-tests.sh` `cd` yapar). Açılamazsa **testi başarısız yapma**: `std::printf("NAVGRID real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n")` yaz ve dön.
- Açılırsa: `LoadFile` true; `Size()==513`, `Unit()==4.0f`; olay 0 sayısı **29 522**, olay 1 sayısı **233 647**; `Height` min/max −30,633 / 82,122 (her biri `1e-3` içinde); `Build()` sonrası `MainComponentCells()==88508`; arena A `(1274, 890)` ve B `(746, 1106)` noktalarının `CellOf` hücreleri `Walk`; nesne olayı hücresi `(622, 911)` `Walk` değil (olay 0); `HeightAt(1375.0f, 1085.0f)` 11,8 m'ye **1,5 m** içinde (transpoze yerleşim 20,67 m verirdi: indeks doğrulaması).
- Başarı satırı yaz: `std::printf("NAVGRID real map: n=%d main=%d clearance_max=%d build_ms=%.1f\n", ...)` (`clearance_max` = ana bileşendeki en büyük `Clearance`; `build_ms` = `Build` süresi, `std::chrono::steady_clock`; **süre için `CHECK` yok**, yalnızca bilgi).

### 5.4 `tools/nav-export.py`

Başlık docstring'i (İngilizce): amaç, §5.1 biçimi, kullanım. Seçenekler: `--map-dir DIR` (varsayılan `os.environ.get("FDP_MAP_DIR", "/mnt/c/dev/fdp/server/Map")`, `arena-report.py:32` ile aynı), `--map NAME` (varsayılan `freezone_a_20050718.smd`), `--out PATH` (varsayılan `<ROOT>/build/nav/zone71.navgrid`; üst dizin yoksa oluşturulur), `--selftest`.

Akış: `parse(path, load_warps=False)` → `n = res["m_nMapSize"]`, `unit = res["m_fUnitDist"]`, `events`, `height` → `struct.pack("<8sif", b"FDPNAV01", n, unit)` + olay baytları (`array('h')`, little-endian değilse `byteswap`) + yükseklik baytları (`array('f')`) → dosyaya yaz. Ardından dosyayı **geri okuyup** doğrula (sihirli sayı, `n`, uzunluk `16 + 6·n·n`) ve **bağımsız** ana bileşen sayısını Python'da hesapla (§5.2 kural 2'nin aynısı: 4-bağlantılı olay-1 bileşenleri, kenara değmeyenlerin en büyüğü; iteratif BFS). Tek satır özet yaz (alan sırası ve biçim sabit, kabul kriteri buna dayanır):

```
NAVGRID file=<yol> bytes=1579030 n=513 unit=4.0 events0=29522 events1=233647 hmin=-30.633 hmax=82.122 main_component=88508 crc32=<8 hex hane>
```

(`hmin`/`hmax` üç ondalık; `crc32` = `zlib.crc32` ile dosya içeriği, küçük harf 8 hane.) Hata durumunda (`parse` hatası, eksik dosya) `NAVGRID FAILED: <neden>` yaz ve çıkış kodu 1 ile bitir.

`--selftest` (SMD gerekmez): 6×6 sentetik `events`/`height` ile aynı yazıcıyı geçici dosyaya çağırır, geri okur, alanları ve ana bileşen sayısını (kenara değen büyük bileşen elenir, içteki küçük bileşen seçilir) doğrular; hepsi geçerse `SELFTEST OK` yazıp 0 ile çıkar. Yazma/okuma/bileşen mantığı `selftest` ile aynı fonksiyonları kullanmalı (`write_navgrid(path, n, unit, events, heights)`, `read_navgrid(path)`, `main_component_cells(n, events)`).

### 5.5 Proje dosyaları ve betik

1. `BotCore/BotCore.vcxproj`: `ClInclude` grubuna `<ClInclude Include="NavGrid.h" />` (alfabetik sıra: `Perception.h`'den önce, `BotMotion.h`'den sonra). BOM/CRLF korunur.
2. `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubuna `<ClCompile Include="NavGridTests.cpp" />` (`MotionTests.cpp`'den sonra).
3. `tools/run-tests.sh`: `EXE=` kontrolünden sonra, `rc=0` satırından önce `cd "$ROOT"` ekle (yalnızca bu satır).

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/nav-export.py --selftest` çıkış kodu 0 ve çıktının son satırı `SELFTEST OK`.
- [ ] K2: `python3 tools/nav-export.py` çıkış kodu 0 ve çıktısı **tam** şu satırdır (yol hariç; `crc32` raporda yazılır, değeri tutarlı olmalı): `NAVGRID file=… bytes=1579030 n=513 unit=4.0 events0=29522 events1=233647 hmin=-30.633 hmax=82.122 main_component=88508 crc32=<hex>`. `build/nav/zone71.navgrid` dosya boyutu `stat -c %s` ile `1579030`.
- [ ] K3: `python3 tools/nav-export.py` ikinci kez çalıştırılınca aynı `crc32` (belirlenimli çıktı).
- [ ] K4: `./tools/build.sh Release` hatasız biter; `BotCore` ve `BotCoreTests` için **yeni uyarı yok** (Level 4; derleme çıktısında `NavGrid` geçen `warning` satırı bulunmaz).
- [ ] K5: `./tools/run-tests.sh Release --no-build --list` çıktısı şu on test adını içerir: `Nav_Init_Validation`, `Nav_Event_OutOfBounds`, `Nav_Walk_MainComponent`, `Nav_Clearance_Ring`, `Nav_Clearance_BruteForce`, `Nav_Edge_Slope`, `Nav_Edge_NoCornerCutting`, `Nav_HeightAt_Bilinear`, `Nav_Load_Buffer`, `Nav_RealMap_Zone71`.
- [ ] K6: K2'den **sonra** `./tools/run-tests.sh Release --no-build` çıkış kodu 0; çıktıda tüm eski testler (Rng/Motion/Combat/Perception) ve on yeni test `[ OK ]`; `NAVGRID real map: n=513 main=88508 ...` satırı var ve `SKIPPED` geçmiyor.
- [ ] K7: Dosya yokken (`build/nav/zone71.navgrid` geçici olarak başka ada taşınarak) `./tools/run-tests.sh Release --no-build Nav_RealMap` çıkış kodu 0 ve çıktıda `SKIPPED`; ardından dosya yerine geri konur.
- [ ] K8: `./tools/run-tests.sh Debug` (derleme dahil) çıkış kodu 0 (Debug'da `assert`/sınır hataları yok).
- [ ] K9: Saflık: `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavGrid.h` çıktısı boş; `git diff --stat gece/2026-10-02-nav...bot/F5-01` yalnızca §4'teki 6 dosyayı gösterir (`GameServer/`, `AIServer/`, `shared/`, `docs/` yok).
- [ ] K10: `NavGrid::Build` sonrası bellek/zaman: gerçek haritada `build_ms` raporlanır (Release); değer Uygulayıcı Raporu'na yazılır (kapı değil, ölçüm; beklenen onlarca ms).

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py --selftest
python3 tools/nav-export.py
stat -c %s build/nav/zone71.navgrid
./tools/build.sh Release
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Debug
grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavGrid.h
git diff --stat gece/2026-10-02-nav...bot/F5-01
```

## 8. Kısıtlar ve uyarılar

- **Paralel hat:** sunucuya hiç dokunma (`tools/run-servers.sh` çağırma; ana hat başka çalışma ağacından sunucu çalıştırıyor olabilir). DB'ye bağlanma. Dal tabanı `gece/2026-10-02-nav`: `git switch -c bot/F5-01 gece/2026-10-02-nav`.
- Kodlama/satır sonu: `AGENTS.md` §3. Yeni C++ dosyaları ASCII + CRLF + tab + Allman; `.py` ve `.sh` LF. Yorumlar İngilizce. Plan metnindeki Türkçe açıklamalar koda girmez.
- **CLI-08:** bu veri modeli botun yürünebilirlik kuralını **kendisinin** uygulaması içindir; sunucu hareketi yalnızca harita sınırına bakar (`docs/12` §1). Burada oyun mekaniği değiştirilmez.
- **Eğim parametresi `[A]`:** `P-NAV-MAX-SLOPE = 0.625` bir varsayımdır (`docs/12` §2 "başlangıç 2,5 m / 4 m"); T-NAV-02 ölçülene kadar değiştirilmez, koda sabit gömülmez (`NavParams`).
- `Walk` tanımı `docs/12` §2'deki "olay = 1 ve ana bileşende" ifadesine uyar; "ana bileşen = kenara değmeyen en büyük 4-bağlantılı açık bileşen" yorumu bu planda **doğrulanmış sayıyla** (88 508) sabitlenmiştir. Sayı tutmazsa **dur** ve Uygulayıcı Raporu'nda sor; kuralı sayıya uydurmak için değiştirme.
- Başlık-yalnızca: tüm tanımlar sınıf içinde veya `inline`; `static` global durum yok (tek bir `NavGrid` nesnesi örneklenebilmeli; testler birden çok örnek kurar).
- `int` taşması: indeks `x * n + z` en çok 263 169; `size_t` dönüşümlerine dikkat (MSVC Level 4 işaret uyarılarını `static_cast` ile çöz, uyarı bastırma `#pragma` ekleme).
- Dosya okuma `std::ifstream` + ikili mod; Windows'ta yol ayracı sorunu yok (göreli `build/nav/...` kullanılır).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- **Durum:** UYGULANDI
- **Branch / commit:** `bot/F5-01` (taban `gece/2026-10-02-nav`); `41f47db` (6 kod dosyası) + bu rapor/Durum commit'i.
- **Değişen dosyalar:**
  - `BotCore/NavGrid.h` (yeni, 374 satır): `NavParams` + `NavGrid` (§5.2); `walk` (kenara değmeyen en büyük 4-bağlantılı olay-1 bileşeni, iteratif BFS), `clearance` (8 komşu çok kaynaklı BFS → Chebyshev, 255'te doyum), `EdgeOpen` (eğim + köşe kesme yok, simetrik), `HeightAt` (köşe bilinear + clamp), `Load`/`LoadFile` (§5.1, tam boyut), `Event`/`Height`/`Walk`/`Clearance`/`CellOf`. Yalnızca standart başlıklar; `GameServer`/`shared`/`windows.h` yok.
  - `Tests/BotCoreTests/NavGridTests.cpp` (yeni, 469 satır): §5.3'teki 10 test; sentetik ızgaralar (`MakeNav`, `RingEvents`, `Filled`, `HeightZeros`, `CellIndex`) + gerçek harita testi.
  - `tools/nav-export.py` (yeni, 214 satır): `write_navgrid`/`read_navgrid`/`main_component_cells`, `--map-dir`/`--map`/`--out`/`--selftest`; özet satırı + `crc32`; `smd_parse.py`, `arena-report.py` kalıbıyla içe aktarılır.
  - `BotCore/BotCore.vcxproj`: `<ClInclude Include="NavGrid.h" />` (BOM/CRLF korundu).
  - `Tests/BotCoreTests/BotCoreTests.vcxproj`: `<ClCompile Include="NavGridTests.cpp" />` (BOM/CRLF korundu).
  - `tools/run-tests.sh`: EXE kontrolünden sonra `cd "$ROOT"` (göreli `build/nav/...` için).
- **Derleme çıktısı (son satırlar):**
  - Release: `NavGridTests.cpp` → `BotCoreTests.vcxproj -> ...\x86-Release\Tests\BotCoreTests.exe`, rc=0; `NavGrid` geçen `warning` satırı yok.
  - Debug: `71 tests, 0 failed`, rc=0.
- **Kabul kriterleri (öz-değerlendirme):**
  - K1 ✔ `--selftest` → son satır `SELFTEST OK`, rc 0.
  - K2 ✔ satır birebir: `NAVGRID file=… bytes=1579030 n=513 unit=4.0 events0=29522 events1=233647 hmin=-30.633 hmax=82.122 main_component=88508 crc32=4fd154bc`; `stat -c %s` = `1579030`.
  - K3 ✔ iki koşu aynı `crc32=4fd154bc` (belirlenimli).
  - K4 ✔ Release rc=0; derleme çıktısında `NavGrid` içeren uyarı yok.
  - K5 ✔ `--list` on yeni test adını içeriyor.
  - K6 ✔ `71 tests, 0 failed`; `NAVGRID real map: n=513 main=88508 clearance_max=13 build_ms=8.0`; `SKIPPED` yok.
  - K7 ✔ dosya taşınınca `SKIPPED`, rc 0; dosya geri konuldu.
  - K8 ✔ Debug (derleme dahil) rc=0, 71 test.
  - K9 ✔ saflık grep'i boş; `git diff --stat gece/2026-10-02-nav...bot/F5-01` yalnızca §4'teki 6 kod dosyası + bu plan dosyası (`GameServer/`, `AIServer/`, `shared/`, `docs/` yok).
  - K10 ✔ (ölçüm) gerçek haritada `build_ms=8.0` (Release), `clearance_max=13`.
- **Plandan sapmalar:** Yok. Test dosyasına planın listesine ek olarak `<cstdlib>` eklendi (`std::abs(int)`; plan "gibi" diyor). Plandaki doğrulanmış sayılar (29 522 / 233 647 / 88 508 / −30,633 / 82,122) aynen tuttu.
- **Açık sorular:** Yok.

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- **Karar:** DOĞRULANDI (10/10 kriter; otonom paralel hat `nav`, `AUTO_LOOP=1`: birleştirme ve push yapılmadı, birleştirmeyi döngü betiği yapar).
- **İncelenen commit:** `eb463e2` (`bot/F5-01`; taban `gece/2026-10-02-nav`; üç commit: `41f47db` kod, `fb0eb14` rapor, `eb463e2` rapor notu). Çalışma ağacı temiz.
- **Kapsam:** `git diff --stat gece/2026-10-02-nav...bot/F5-01` yedi dosya: §4'teki altı dosya + kendi plan dosyası (yalnızca `Durum` ve Uygulayıcı Raporu). `GameServer/`, `AIServer/`, `shared/`, `docs/` yok. `build/` commit edilmemiş (`git check-ignore build/nav/zone71.navgrid` ignored, `git ls-files build` boş).
- **Biçim:** `NavGrid.h` ve `NavGridTests.cpp` ASCII + CRLF (374/374 ve 469/469 satır CRLF, ASCII dışı 0); `nav-export.py` ve `run-tests.sh` LF, ASCII. İki `.vcxproj` farkı tek satır (`+<ClInclude Include="NavGrid.h" />`, `+<ClCompile Include="NavGridTests.cpp" />`), BOM (`efbbbf`) korunmuş. Bu paylaşılan `BotCore`/`BotCoreTests` projelerinin `.filters` dosyası yok. `#pragma` yalnızca `once`, debug çıktısı yok.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 selftest | ✔ | `python3 tools/nav-export.py --selftest` → `SELFTEST OK`, rc=0 |
| K2 gerçek harita | ✔ | çıktı birebir: `bytes=1579030 n=513 unit=4.0 events0=29522 events1=233647 hmin=-30.633 hmax=82.122 main_component=88508 crc32=4fd154bc`; `stat -c %s` = 1579030 |
| K3 belirlenimli | ✔ | üçüncü koşuda da `crc32=4fd154bc` |
| K4 derleme | ✔ | Release rc=0; `NavGridTests.cpp`/`MotionTests.cpp`/`NavGrid.h` `touch` edilip yeniden derlendi (derleme çıktısında `NavGridTests.cpp` satırı var), `warning` içeren satır 0; projeler Level4 |
| K5 test adları | ✔ | `--list` on `Nav_*` adını veriyor |
| K6 testler | ✔ | `71 tests, 0 failed`, rc=0; on yeni test `[ OK ]`; `NAVGRID real map: n=513 main=88508 clearance_max=13 build_ms=7.5`, `SKIPPED` yok |
| K7 dosya yokken | ✔ | dosya `.bak`'a taşındı → `NAVGRID real map: SKIPPED (...)`, `[ OK ]`, rc=0; dosya geri konuldu (1579030 bayt) |
| K8 Debug | ✔ | `./tools/run-tests.sh Debug` rc=0, `71 tests, 0 failed`, `NavGrid` geçen uyarı yok |
| K9 saflık/kapsam | ✔ | `grep "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavGrid.h` boş; fark kapsamı yukarıda |
| K10 ölçüm | ✔ | `build_ms` Release 7,5 (uygulayıcı 8,0), Debug 257,7; `clearance_max=13` |

Ek denetim: `NavGrid.h` §5.2 kurallarıyla satır satır karşılaştırıldı: yalnızca `event==1` açık (`NavGrid.h:160`); iteratif BFS, kenara değen bileşen elenir, `sizes > bestSize` katı büyüklüğüyle x-ana taramada ilk bileşen kazanır; clearance çok kaynaklı 8 komşulu BFS, 255 doyumu, `Walk` olmayan 0; `EdgeOpen` aralık/`(0,0)` reddi, iki uç `Walk`, `|Δh| <= maxSlope * mesafe`, çaprazda iki ortogonal komşu `Walk` (simetrik); `HeightAt` köşe bilinear + clamp (`i0 <= n-2`); `Load` tam boyut ister, `int64_t` ile boyut hesaplanıyor; `Build`, `Init` öncesi sessizce döner (`m_n < 2`). Mantık Python `main_component_cells` ile bağımsız olarak aynı 88 508'i veriyor (iki ayrı gerçekleme). Sunucuya, DB'ye, `tools/run-servers.sh`'a dokunulmadı. Mekanik, bot avantajı, thread, DB kuralları bu plana uygulanmaz (saf veri modeli; CLI-08 zemini).

**Bulgular (hepsi not, engel değil):**

1. `Tests/BotCoreTests/NavGridTests.cpp:451`: nesne olayı noktası `(622, 911)` için yalnızca `!Walk` denetleniyor; planın "olay 0" ifadesi `Event(...) == 0` ile ayrıca sınanmıyor (olay 1 olup ana bileşen dışında kalan bir hücre de testi geçirirdi). F5-02'de bir test eklenirken `CHECK_EQ(grid.Event(cell), 0)` eklenebilir.
2. `tools/nav-export.py:205`: geri okuma doğrulaması yalnızca başlık (`n`, `unit`) ve uzunluğu denetliyor; `events`/`heights` içeriğini orijinalle karşılaştırmıyor (içerik karşılaştırması yalnızca `--selftest`'te var). `read_navgrid` (`:54`) içinde `open(...).read()` dosyayı açık bırakıyor (CPython'da kapanır). Gerçek haritada `crc32` ve C++ testindeki olay/yükseklik sayıları içeriği zaten doğruluyor.
3. `tools/run-tests.sh`: plan "yalnızca bu satır" dedi; `cd "$ROOT"` ile birlikte bir boş satır da eklenmiş (biçimsel).
4. Hatırlatma: `HeightAt` ve `Height` hücre-köşe (alt köşe) yaklaşıklığını kullanır (plan §5.2 kural 5, `[Ö]`); F5-02 maliyet fonksiyonu bunu bilerek kullanmalı.
