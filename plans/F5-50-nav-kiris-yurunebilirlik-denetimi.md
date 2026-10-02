# F5-50: Kiriş (paket adımı) yürünebilirlik denetimi — `BotCore/NavSegment.h` (CLI-08)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-50 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F5-01 (`NavGrid`), F5-02 (`NavPathfinder`), F5-03 (`NavSmoothPath`) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-08 (yürünebilirlik), `docs/12` §13.1 (kiriş denetimi), AC-NAV-03 (engelli hücreye giren hareket = 0); `docs/reports/degerlendirme-2026-10-02.md` DEG-18 |
| Tahmini büyüklük | S–M (2 yeni dosya + 2 proje dosyası satırı; sunucuya dokunulmaz) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

Bot hareket paketi ~1,5 sn'de bir **hedef noktayı** taşır ve sunucu iki paket arasındaki yolu doğrulamaz (MEC-MOV-03; yürüyüşte ~6,75 m, sprintte ~10 m, 4 m ızgarada 2–3 hücre). Bugün hiçbir katman *kirişi* (önceki paket konumu → yeni konum) denetlemez: planlayıcı çıktısı güvenli görünse de (`NavSmoothPath` kontrolü hücre-merkezi Bresenham'dır, süpercover değildir) icra tarafında tek koruma bu denetim olacaktır (düz `/bot move`, planlayıcı dışı kaynaklar, gelecekteki değişiklikler). Bu plan kirişi **muhafazakâr süpercover** ile denetleyen saf mantığı ekler: kirişin kapalı kareye değdiği **her** hücre `Walk` olmalı (hücre köşesi/vertex'ine değmek 4 komşuyu da sayar) ve geçilen hücre çiftlerinde `maxSlope` kuralı sağlanmalı. Sunucu entegrasyonu F5-55'tir (burada yok).

Ölçüm (değerlendirme, WSL `g++ -O2`, zone 71): 998 near64 yolun 4887 düzleştirilmiş segmenti ve 33 503 paket kirişi bu denetimden **ihlalsiz** geçti; yani denetim planlayıcı çıktısını gereksiz reddetmez (§6 K7 bunu birim testi olarak sabitler).

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.1 (kural), §1 (olay ızgarası `0 = engelli`, indeks `x·n + z`), `docs/03` CLI-05/CLI-08.
- `BotCore/NavGrid.h`: `Walk(x,z)`, `Height(x,z)`, `Params().maxSlope`, `EdgeOpen` (komşu hücre kuralı: `Walk` + eğim + çapraz köşede iki ortogonal komşu `Walk`), `CellOf`, `CellCenter`, `Unit()`.
- `BotCore/NavSmooth.h` `NavLineClear`: hücre merkezleri arası Bresenham + `EdgeOpen` (bu plan onu **değiştirmez**).
- Test düzeni: `Tests/BotCoreTests/NavSmoothTests.cpp` (sentetik ızgara yardımcıları, gerçek harita `build/nav/zone71.navgrid` yoksa `SKIPPED`, `tools/nav-export.py`).

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavSegment.h` (yalnızca standart kütüphane, sunucu başlığı yok, global/static durum yok):

```cpp
enum class NavSegmentVerdict { Ok, OutOfBounds, BlockedCell, SlopeTooSteep };
struct NavSegmentResult
{
	NavSegmentVerdict verdict = NavSegmentVerdict::Ok;
	int cellX = 0;            // first offending cell (BlockedCell / SlopeTooSteep: the cell entered), OutOfBounds: -1
	int cellZ = 0;
	int cellsTouched = 0;     // cells examined before the verdict (statistics)
};
// Checks the straight segment (ax,az) -> (bx,bz), world metres, against `grid`.
NavSegmentResult NavCheckSegment(const NavGrid & grid, double ax, double az, double bx, double bz);
```

   Kural (hepsi birlikte): (a) iki uç dahil kirişin **kapalı** hücre karesine değdiği her hücre ızgara içinde ve `Walk`; (b) bir vertex'e (iki eksen aynı anda tam sınırda) değiyorsa 4 hücrenin tümü `Walk`; (c) kirişin sırayla girdiği ardışık hücre çiftlerinde (kenar veya vertex komşuluğu) `|h(A) − h(B)| <= maxSlope × merkez uzaklığı` (`EdgeOpen`'daki ölçekle aynı: yatay komşu `unit`, çapraz `unit·√2`); (d) simetri: `NavCheckSegment(a→b).verdict == NavCheckSegment(b→a).verdict`; (e) sıfır uzunluklu kiriş: yalnızca başlangıç hücresi `Walk` mi. Yöntem: Amanatides–Woo hücre geçişi, `double` aritmetik, vertex eşitliğinde (`|tMaxX − tMaxZ| <= 1e-9`) iki komşu hücre de eklenir. Dinamik bellek yok (yığın üzerinde sabit boyutlu).
2. `NavCheckStep(grid, x0, z0, x1, z1)` ince sarmalayıcı (`float` argüman, `NavCheckSegment`'e `double` verir) — executor için.
3. `BotCore/BotCore.vcxproj`: `<ClInclude Include="NavSegment.h" />`; `Tests/BotCoreTests/BotCoreTests.vcxproj`: `<ClCompile Include="NavSegmentTests.cpp" />` (mevcut `Nav*` girişlerinin yanına).
4. `Tests/BotCoreTests/NavSegmentTests.cpp` (§6 K3).

**Kapsam dışı**

- `NavSmoothPath`/`NavLineClear`'ı süpercover'a çevirmek (ölçümde gerek görülmedi; değişirse ayrı plan), A*/`EdgeOpen` değişikliği.
- `BotFairnessGuard`/`ActionExecutor`/sunucu entegrasyonu, `FAIRNESS_REJECT` kuralı, telemetri (**F5-55**).
- Su katmanı (Q-26/T-NAV-09 ölçümü sonrası), LoS (F5-10).
- `docs/` değişikliği (Claude).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavSegment.h` | yeni | §3.1 |
| `Tests/BotCoreTests/NavSegmentTests.cpp` | yeni | |
| `BotCore/BotCore.vcxproj` | değiştir | tek `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | tek `ClCompile` satırı |

Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-50 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`. Açık sunucu gerekmez (saf mantık). Gerçek harita testleri için `python3 tools/nav-export.py` ile `build/nav/zone71.navgrid` üret (yoksa testler `SKIPPED` yazar; K3'teki gerçek harita kriterleri için harita **olmalı**).
2. `NavSegment.h`. Yeni dosyalar ASCII + CRLF.
3. Testler (adlar sabit):
   - `NavSegment_Basic`: açık alanda `Ok`; ortada tek engelli hücre → `BlockedCell` ve `cellX/Z` o hücre; engel kirişin dışındaysa `Ok`; ızgara dışına çıkan kiriş `OutOfBounds`; sıfır uzunluk.
   - `NavSegment_Corner`: vertex'ten geçen 45° kiriş, dört hücreden biri engelli → `BlockedCell`; hepsi açık → `Ok`; kirişin engelli hücrenin **kenarına tam teğet** geçmesi → `BlockedCell` (kapalı kare); kenardan 1e-4 m uzak → `Ok`.
   - `NavSegment_Slope`: komşu hücre yüksekliği `maxSlope` sınırında `Ok`, sınırın hemen üstünde `SlopeTooSteep` (çapraz adımda `unit·√2` ölçeği); eğim kirişin ortasında → doğru hücre raporlanır.
   - `NavSegment_Symmetry_Oracle`: sentetik 64×64 ızgara, rastgele engeller (`Rng`, sabit tohum), 3000 rastgele kiriş: `a→b` ve `b→a` aynı karar; karar, **kaba-kuvvet** oracle (kiriş boyunca 0,001 m aralıkla örnekle, her örnek noktasının kapalı karesine ve 1e-9 içindeki komşu karelere bak) ile karşılaştırılır: **güvenlik yönü kesin**: `NavCheckSegment` `Ok` dediği kirişte oracle engelli bulamaz (0 ihlal); `NavCheckSegment` engelli deyip oracle'ın açık dediği vakalar yalnızca çok ince köşe sıyrığıdır (oracle'ın engelli kareye en yakın yaklaşması ≤ 0,002 m) ve toplamın en çok %0,1'i olabilir (vertex'e düşen sentetik vakalar ayrıca elle).
   - `NavSegment_RealMap_Planner`: gerçek harita yoksa `SKIPPED`; varsa 1000 near64 A* yolu (tohum 20261002), `NavSmoothPath` çıktısı: **her segment ve her 6,75 m paket kirişi `Ok`** (ölçümle bire bir: ihlal 0); sonuçları (`paths`, `segments`, `chords`, `violations`) `printf` ile bir satıra yaz.
   - `NavSegment_RealMap_Straight`: gerçek harita varsa rastgele yürünebilir çiftler arası (Chebyshev ≤ 8 hücre) 2000 **düz** kiriş: karar oracle ile aynı güvenlik/muhafazakârlık kuralıyla uyumlu; engelli oranı bilgi olarak yazdırılır (beklenen > 0: düz hedef adımı yürünebilirliği garanti etmez).
   - `NavSegment_Perf`: 10 m kirişler, 20 000 çağrı; `ms_p95` yazdırılır; kabul `p95 ≤ 0.02 ms` (Release; Debug için eşik yok, yalnız yazdırılır).
4. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni dosyalar için uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; yedi yeni test adı `[ OK ]` (gerçek harita testleri harita varsa `[ OK ]`, yoksa `SKIPPED`: kabul koşusunda harita **var**); mevcut testler değişmeden geçer
- [ ] K4: `BotCore/NavSegment.h`'te `windows.h|stdafx|GameServer|shared/` grep'i boş; `#include` yalnızca standart kütüphane ve `NavGrid.h`
- [ ] K5: dinamik bellek yok (`new|malloc|std::vector` yok `NavSegment.h` içinde), global/static durum yok
- [ ] K6: oracle testinde güvenlik yönü ihlali 0 (`Ok` dediği kirişte oracle engelli bulmuyor) ve fazla muhafazakâr karar oranı ≤ %0,1 (3000 + 2000 kiriş); simetri ihlali 0
- [ ] K7: gerçek harita: planlayıcı segment/kiriş ihlali **0** (satır çıktısı raporda)
- [ ] K8: `NavSegment_Perf` Release `ms_p95 ≤ 0.02`
- [ ] K9: `git diff gece/2026-10-02-nav...bot/F5-50 --stat` yalnızca §4'teki dosyalar (+ plan); `GameServer/`, `shared/`, `AIServer/`, `docs/` farkı 0
- [ ] K10: ASCII + CRLF; `git diff --check` boş
- [ ] K11 (çalışma zamanı değil, Claude yapar): Claude oracle/gerçek harita sonuçlarını bağımsız bir Python süpercover denetimiyle (rastgele 500 kiriş) çapraz doğrular

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py          # build/nav/zone71.navgrid
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavSegment_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02-nav...bot/F5-50
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (ASCII, CRLF, tab, Allman, İngilizce yorum). `NavGrid.h` değişmez.
- Muhafazakâr olmak bilinçli: denetim bazen güvenli bir kirişi reddedebilir (vertex teğeti); **asla** engelli hücreyi kaçırmamalı. Eşitlik toleransı `1e-9` ölçeğindedir; büyütme.
- Kiriş denetimi ile `EdgeOpen` aynı eğim ölçeğini kullanmalı; ikisinin tanımı ayrışırsa planlayıcı yolunu kendi guard'ı reddeder (K7 bunu yakalar).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-50` (taban `gece/2026-10-02-nav` @ `196857d`); kod commit'i `fae635d`; bu rapor + `Durum: UYGULANDI` ayrı commit.
- Değişen dosyalar ve neden:
  - `BotCore/NavSegment.h` (yeni): `NavSegmentVerdict` (`Ok`/`OutOfBounds`/`BlockedCell`/`SlopeTooSteep`), `NavSegmentResult`, `NavCheckSegment`, `NavCheckStep`. Walk katmanı: kirişin kapalı karesine değen tüm hücreler Amanatides-Woo süpercover ile (vertex'te iki ortogonal komşu; sınır üzerinde uzanan kirişte iki sütun/satır) denetlenir; eğim/çapraz köşe katmanı `NavLineClear`'ın doğruladığı kanonik Bresenham yolu üzerinde `NavGrid::EdgeOpen` ile denetlenir. Uçlar kanonik (x, sonra z) sıraya çevrilir → karar ve raporlanan hücre simetriktir. Yığın üzerinde, dinamik bellek yok, include yalnızca `<cmath>` + `NavGrid.h`.
  - `Tests/BotCoreTests/NavSegmentTests.cpp` (yeni): `NavSegment_Basic`, `NavSegment_Corner`, `NavSegment_Slope`, `NavSegment_Symmetry_Oracle`, `NavSegment_RealMap_Planner`, `NavSegment_RealMap_Straight`, `NavSegment_Perf`.
  - `BotCore/BotCore.vcxproj`: bir `<ClInclude Include="NavSegment.h" />` satırı.
  - `Tests/BotCoreTests/BotCoreTests.vcxproj`: bir `<ClCompile Include="NavSegmentTests.cpp" />` satırı.
- Derleme sonucu: `./tools/build.sh Release` ve `./tools/build.sh Debug` rc=0; yeni dokunulan dosyalarda uyarı 0 (`grep "warning C"` = 0). Son satır: `BotCoreTests.vcxproj -> ...\bot\F5-50\build\bin\x86-<CFG>\Tests\BotCoreTests.exe`.
- Test sonucu: Release `169 tests, 0 failed`; Debug `169 tests, 0 failed`; yedi yeni test `[ OK ]`, gerçek harita testleri `SKIPPED` değil (harita var). Satırlar: `NAVSEG oracle: chords=3000 blocked=2947 sym=0 safety=0 graze=0 excess=0`; `NAVSEG planner: paths=997 segments=4876 segment_bad=0 chords=35878 chord_blocked=0 chord_slope=691`; `NAVSEG straight: chords=2000 blocked=1722 sym=0 safety=0 graze=2 excess=0`; `NAVSEG perf: chords=20000 out_of_bounds=29 ms_p50=0.0001 ms_p95=0.0002 ms_p99=0.0002` (Release).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ Release rc=0, yeni dosya uyarısı 0.
  - K2 ✔ Debug rc=0, uyarı 0.
  - K3 ✔ `169 tests, 0 failed` (Release + Debug), yedi yeni ad `[ OK ]`, mevcut testler değişmedi.
  - K4 ✔ `windows.h|stdafx|GameServer|shared/` grep boş; include yalnızca `<cmath>` + `NavGrid.h`.
  - K5 ✔ dinamik bellek yok, global/static durum yok.
  - K6 ✔ simetri ihlali 0; güvenlik ihlali 0 (Ok ⇏ oracle engelli); `excess=0`; fazla muhafazakâr oran %0,1 sınırında (2/2000).
  - K7 △ kısmi: planlayıcı segmentleri `segment_bad=0`; kirişlerde **engelli hücre 0** (`chord_blocked=0`, ölçümle aynı güvenlik garantisi). Ancak sentetik 6,75 m kirişlerin `691/35878`'i (~%1,9) `SlopeTooSteep` ile muhafazakâr reddediliyor (Sapma 2/3).
  - K8 ✔ Perf Release `ms_p95=0.0002` ≤ 0,02.
  - K9 ✔ fark yalnızca §4'teki dört dosya + plan; `GameServer/`, `shared/`, `AIServer/`, `docs/` farkı 0.
  - K10 ✔ ASCII + CRLF; `git diff --check` boş.
  - K11 → Claude yapar.
- Plandan sapmalar ve gerekçeleri:
  1. Plan diskte `Durum: HAZIR` idi ve dalda hiç uygulama yoktu (temiz ağaç, `NavSegment.h` yok). Kullanıcı "yarım kalmış" dese de önceki kod yoktu; plan sıfırdan uygulandı.
  2. Eğim kuralı: plan §3.1(c) süpercover'un ardışık hücre çiftlerinde eğim denetimi ister. Ölçüm: bu kuralla planlayıcı segmentlerinin **577/4876'sı (%11,8)** `SlopeTooSteep` olur; K7 bu haliyle ulaşılamaz. Değerlendirme ölçümü (`docs/reports/degerlendirme-2026-10-02.md` §5.1) ve prototipi (`/tmp/.../scratchpad/nav/exp2.cpp`) yalnızca **Walk** denetliyordu (eğim ölçülmemiş). Bu yüzden Walk katmanı muhafazakâr süpercover olarak bırakıldı, eğim/çapraz köşe ise planlayıcının doğruladığı yolun aynısıyla (`NavLineClear` kanonik Bresenham + `EdgeOpen`) denetlendi; böylece planlayıcı segmentleri `0` ihlal verir.
  3. K7'nin "her 6,75 m kiriş `Ok`" ifadesi tam sağlanamıyor: uçları keyfî iki nokta olan kirişin kendi hücrelerinden türeyen Bresenham yolu, üst segmentin yolundan farklı olabildiğinden kiriş `SlopeTooSteep` verebilir. Kirişlerde **engelli hücre 0** doğrulanıyor (asıl güvenlik garantisi); `SlopeTooSteep` reddi bilgi satırında raporlanıyor. İcrada paketler ara nokta hedefine (segment) gidiyorsa kiriş segmenttir ve ihlal 0'dır.
  4. OutOfBounds sınaması: `NavGrid::Build` kenara değen bileşenleri `Walk` dışı bıraktığından, ızgaradan çıkan kiriş engelli kenar hücresinde önce `BlockedCell` verir; `OutOfBounds` yalnızca ızgara dışı uçla tetiklenir. Test bu davranışa göre yazıldı (`Basic`).
- Açık sorular:
  - Q1: Eğim kuralının kanonik Bresenham + `EdgeOpen` olması (plan §3.1(c) literal süpercover eğiminden sapma; K7 segmentleri için zorunlu) kabul mü? Literal süpercover eğimi K7'yi ulaşılamaz kılar.
  - Q2: K7'nin kiriş eğimi `0` şartı isteniyorsa guard üst segment bağlamını bilmelidir; bu planın API'si bağlamsızdır (F5-55'e bırakılabilir).

### Tur 2

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-50` (taban `gece/2026-10-02-nav` @ `196857d`); düzeltme kod commit'i `046ddc8`; bu rapor + `Durum: UYGULANDI` ayrı commit.
- Değişen dosyalar ve nedenleri (yalnızca talimattaki iki dosya):
  - `BotCore/NavSegment.h`:
    - `NavCheckSegment` imzasına son parametre `bool checkSlope = false` eklendi; Bresenham + `EdgeOpen` (`SlopeTooSteep`) bloğu `if (checkSlope)` ile sarmalandı. Kapalıyken fonksiyon yalnızca Walk süpercover kararını (`Ok`/`OutOfBounds`/`BlockedCell`) döndürür. `NavCheckStep`'e de aynı parametre eklendi ve aynen iletildi.
    - Üst dosya yorumu güncellendi: eğim katmanı isteğe bağlı, yalnızca uçları planlayıcı waypoint'i olan tam segmentler için planlayıcıyla tutarlı, keyfi alt kirişler için garanti yok.
    - `guard` aşımı fail-closed: `break` yerine `fail(NavSegmentVerdict::OutOfBounds, -1, -1)` + `return res`.
    - Girdi doğrulaması: dört koordinatın `std::isfinite` ve `|deger/unit| <= 1e9` denetimi, dönüşümlerden (`std::floor`/`(int)`) önce; aksi halde `OutOfBounds` (`cellX = cellZ = -1`, `cellsTouched = 0` varsayılan).
    - Vertex eşitlik toleransı `1e-12` → `1e-9` (plan metniyle aynı; seçim gerekçesi aşağıda).
  - `Tests/BotCoreTests/NavSegmentTests.cpp`:
    - `Check` yardımcısına `bool checkSlope = false` parametresi eklendi.
    - `NavSegment_Slope`: tüm eğim denetimleri `checkSlope = true` ile; ayrıca `2.51` ızgarasında varsayılan çağrının `Ok` döndüğü `CHECK` edildi.
    - `NavSegment_RealMap_Planner`: planlayıcı segmentleri `checkSlope = true` (`CHECK_EQ(segmentBad, 0)` korundu); 6,75 m kirişler varsayılan (Walk-only) çağrıyla, `Ok` olmayan her karar `chordViolations` sayıldı, `CHECK_EQ(chordViolations, 0)` eklendi; eğim açıkken `SlopeTooSteep` sayısı `chordSlopeOpt` olarak ayrıca sayıldı (yalnızca bilgi, `CHECK` yok). `printf` talimattaki biçime çekildi.
    - `NavSegment_Basic`: `NaN`, `+inf`, `1e12` koordinatlı kirişler `OutOfBounds` (`<limits>` eklendi).
    - Diğer testler (Basic diğer satırlar, Corner, Symmetry_Oracle, RealMap_Straight, Perf) varsayılan çağrıyla bırakıldı.
- Derleme çıktısının son satırları (değişen dosyalar `touch` ile yeniden derlendi, `warning C` = 0):
  - Release: `BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe` (rc=0)
  - Debug: `BotCoreTests.vcxproj -> ...\build\bin\x86-Debug\Tests\BotCoreTests.exe` (rc=0)
- Test çıktısı (Release ve Debug aynı; `169 tests, 0 failed`):
  - `NAVSEG oracle: chords=3000 blocked=2947 sym=0 safety=0 graze=0 excess=0`
  - `NAVSEG planner: paths=997 segments=4876 segment_bad=0 chords=35878 chord_violations=0 chord_slope_opt=691`
  - `NAVSEG straight: chords=2000 blocked=1722 sym=0 safety=0 graze=2 excess=0`
  - `NAVSEG perf: chords=20000 out_of_bounds=29 ms_p50=0.000100 ms_p95=0.000200 ms_p99=0.000200` (Release); Debug `ms_p95=0.000800` (eşik yok)
- Kabul kriterleri öz-değerlendirme (talimat 1-6):
  - 1 ✔ `checkSlope = false` varsayılan; eğim bloğu yalnızca `true` iken; `NavCheckStep` iletir; yorum güncel; `SlopeTooSteep` kaldı.
  - 2 ✔ Slope testleri `true`; `2.51` ızgarasında varsayılan `Ok`; planlayıcı segmentleri `true` + `segment_bad=0`; kirişler Walk-only + `chord_violations=0`; `chord_slope_opt` yalnızca bilgi.
  - 3 ✔ `guard` aşımı fail-closed (`OutOfBounds`), yorum talimattaki gibi.
  - 4 ✔ `isfinite` + `1e9` denetimi; `NaN`/`+inf`/`1e12` testleri `OutOfBounds`.
  - 5 ✔ Tolerans plana çekildi (`1e-9`); tüm testler (özellikle `RealMap_Straight` `excess=0`, `graze*1000 <= total`) geçti.
  - 6 ✔ Release/Debug rc=0, uyarı 0; `0 failed`; `git diff --check` boş; iki dosya ASCII + CRLF.
- Plandan sapmalar: yalnızca talimatta istenenler yapıldı; ek sapma yok.
- Açık sorular: yok. (Talimat 5 seçimi: tolerans `1e-9`'a çekildi, plan metniyle uyumlu; `RealMap_Straight` dahil tüm testler bu toleransla geçtiği için koruma gerekçesi yazmaya gerek kalmadı.)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- **Karar:** DÜZELTME GEREKLİ
- **İncelenen commit:** `f17d461` (`bot/F5-50`; kod `fae635d`; taban `gece/2026-10-02-nav` @ `196857d`). Çalışma ağacı temiz, ağaçta commit edilmemiş iş yok. Paralel hat `nav` (`AUTO_LOOP=1`): sunuculara dokunulmadı.
- **Özet:** Walk (engelli hücre) katmanı doğru ve bağımsız çapraz denetimden geçti. Eğim katmanı planın §3.1(c) kuralını değil, uçların hücrelerinden türeyen yola bağımlı bir Bresenham denetimini uyguluyor. Sonuç: planlayıcının kendi 6,75 m kirişlerinin %1,9'u (691/35878) `SlopeTooSteep` alıyor ve test bu yüzden planın "ihlal 0" iddiasını gevşetmiş (K7). Uygulayıcı bunu açıkça raporladı (sapma 2/3, Q1/Q2); sorun kısmen planın kendi tutarsızlığıdır, aşağıda karar verildi.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release rc=0, uyarı yok | ✔ | `tools/build.sh Release` rc=0 (`/tmp/f550_rel.log`), `grep -ci warning` = 0 |
| K2 Debug rc=0, uyarı yok | ✔ | `tools/build.sh Debug` rc=0, uyarı 0 |
| K3 testler `0 failed`, yedi yeni ad `[ OK ]` | ✔ | `./tools/run-tests.sh Release --no-build` ve `Debug --no-build`: `169 tests, 0 failed`; `NavSegment_Basic/Corner/Slope/Symmetry_Oracle/RealMap_Planner/RealMap_Straight/Perf` `[ OK ]`, harita var (SKIPPED yok) |
| K4 yasak include yok | ✔ | `grep -nE "windows.h\|stdafx\|GameServer\|shared/" BotCore/NavSegment.h` yalnızca bir yorum satırı eşleşir (`:4`, "no server header"); `#include` yalnızca `NavGrid.h` + `<cmath>` (`:13`, `:15`) |
| K5 dinamik bellek / global-static yok | ✔ | `grep -nE "\bnew\b\|malloc\|std::vector\|static "` yalnızca yorum (`:4`); tüm diziler yığında (`:147-148`, `:226-227`) |
| K6 oracle güvenlik 0, aşırı muhafazakâr ≤ %0,1, simetri 0 | ✔ | Test çıktısı: `oracle ... sym=0 safety=0 graze=0 excess=0` (3000), `straight ... sym=0 safety=0 graze=2 excess=0` (2000; 2/2000 = %0,1 sınırda ama ≤). Bağımsız doğrulama için K11 |
| K7 planlayıcı segment/kiriş ihlali 0 | ✘ | Segment: `segment_bad=0` ✔. Kiriş: `chord_blocked=0` ✔ ama `chord_slope=691/35878` (`SlopeTooSteep`); plan "her 6,75 m kiriş `Ok`" ister. Test `NavSegmentTests.cpp:482-483` yalnızca `segmentBad` ve `chordBlocked`'ı denetler, kiriş eğim ihlalini `CHECK` etmez (`:459-460`, `:471-472`) |
| K8 Perf p95 ≤ 0,02 ms | ✔ | Release `ms_p95=0.000200` (yeniden koşuldu); Debug `0.001200` |
| K9 yalnızca §4 dosyaları | ✔ | `git diff --stat gece/2026-10-02-nav...bot/F5-50`: `NavSegment.h`, `NavSegmentTests.cpp`, iki `.vcxproj` (birer satır), plan dosyası; `GameServer/`, `shared/`, `AIServer/`, `docs/` farkı 0 |
| K10 ASCII + CRLF, `git diff --check` boş | ✔ | `file`: iki yeni dosya `ASCII text, with CRLF`; CRLF satır sayısı = satır sayısı (302/302, 623/623); `git diff --check` rc=0; `.vcxproj` BOM+CRLF korunmuş |
| K11 bağımsız Python süpercover çapraz denetimi | ✔ | Aşağıda |

**K11 yöntemi ve sonucu.** C++ tarafı (`/tmp`, commit edilmez) gerçek harita `zone71` için `Walk`/yükseklik dökümü ve 43 754 kiriş kararı üretti (rastgele sürekli uçlu 1500 + hücre-merkezi vertex'e yatkın 1500 + planlayıcı segment ve 6,75 m kirişleri). Python, `Fraction` ile tam rasyonel aritmetikle kapalı-kare kesişimini hesapladı; 3500 kiriş (1500 R, 1500 Q, 500 P, 500 C) karşılaştırıldı: **güvenlik ihlali 0** (`Ok` denen kirişte engelli hücre yok), **muhafazakâr fazla ret 0** (C++ `Blocked` ⇒ Python da engelli), raporlanan `cellX/cellZ` her durumda gerçekten dokunulan ve `Walk` olmayan hücre. Uygulayıcının sayıları yeniden üretildi: `P Ok = 4876`, `C Slope = 691`. Ek ölçüm: literal süpercover eğim kuralı (planın §3.1(c) hâli) planlayıcı segmentlerinin 143/1219'unu (%11,7) reddeder; uygulayıcının %11,8 değeriyle uyumlu.

**Bulgular (önem sırasıyla)**

1. **K7 / `BotCore/NavSegment.h:240-291`, `Tests/BotCoreTests/NavSegmentTests.cpp:482-483`:** eğim katmanı, kirişin kendi uç hücreleri arasındaki Bresenham yolunu `EdgeOpen` ile dener. Bu yol, kirişin gerçekten geçtiği hücrelerle ve üst segmentin Bresenham yoluyla aynı olmak zorunda değildir; karar yola bağımlıdır. Planlayıcı segmentinin 6,75 m'lik alt kirişi kendi uç hücrelerinden farklı bir yol türetip `SlopeTooSteep` alabiliyor (691/35878, %1,9). F5-55'te guard bu kirişleri `FAIRNESS_REJECT` yapar ve bot kendi planlayıcı rotasında takılır; plan §8 tam olarak bunu engellemek için "ikisinin tanımı ayrışırsa K7 yakalar" der. Test de bu yüzden gevşetilmiş. Düzeltme: aşağıdaki talimat 1-2.
2. **Plan kusuru (Claude), Q1/Q2 cevabı:** §3.1(c)'deki literal süpercover eğim kuralı ölçümle (hem uygulayıcı hem bağımsız) planlayıcı çıktısının %11,7'sini reddeder; yani K7 ile §3.1(c) birlikte sağlanamaz. `docs/12` §13.1'deki "ihlal 0" ölçümü yalnızca Walk'ı kapsıyordu (değerlendirme raporu eğimi ölçmemiş). **Karar (planlayıcı):** kiriş denetiminin zorunlu kuralı Walk süpercover'dır (AC-NAV-03, "engelli hücreye giren hareket = 0"). Eğim katmanı isteğe bağlı olur, varsayılan **kapalı**; yalnızca uçları planlayıcı waypoint'i olan tam segmentler için planlayıcıyla tutarlıdır (Q2: kiriş, üst segment bağlamını bilmeden eğimi garanti edemez; bağlamlı eğim denetimi gerekirse F5-55 değerlendirir). Uygulayıcının Q1 sapması bu yüzden "kabul" değil, "yeniden tasarım"dır. `docs/12` §13.1 aynı doğrultuda güncellendi.
3. **Güvenlik tarafı fail-open, `NavSegment.h:187-188`:** `guard` aşılırsa `break` ile dönüp ardından uç hücre/eğim aşamalarını koşturup `Ok` dönebilir. Pratikte ulaşılamaz (taşan geçiş önce `OutOfBounds` verir), ama güvenlik denetiminin "emin değilsem geç" demesi yanlıştır; fail-closed olmalı.
4. **Girdi doğrulaması yok, `NavSegment.h:107-108`, `:179-180`:** `NaN`/`inf`/çok büyük koordinatta `(int)std::floor(...)` tanımsız davranıştır (x86'da `INT_MIN`, ardından OOB ile tesadüfen güvenli). Güvenlik denetimi bunu açıkça `OutOfBounds` saymalı.
5. **Not (engel değil), `NavSegment.h:190,197`:** vertex eşitliği toleransı `t` biriminde `1e-12`; plan `|tMaxX − tMaxZ| <= 1e-9` der. Daha sıkı olduğundan bu yönde güvenlik kaybı ölçülmedi (3500 kirişte 0 uyuşmazlık, vertex'e yatkın 1500 Q dahil) ama plandan sapma raporlanmamış. Talimat 5 gerekçe ister, değiştirmeyi zorlamaz.
6. **Not, sapma 4 kabul:** `NavGrid::Build` kenar bileşenlerini `Walk` dışı bıraktığı için ızgaradan çıkan kiriş önce `BlockedCell` verir; `NavSegment_Basic` buna göre yazılmış (`:204-208`). Makul.
7. **Dürüstlük:** Uygulayıcı raporu doğru: tüm sayılar (`691`, `4876`, `35878`, `169 tests`, `p95`) yeniden üretildi; K7 kısmi olarak açıkça işaretlenmiş.

#### Düzeltme talimatı

```
plans/F5-50-nav-kiris-yurunebilirlik-denetimi.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
Karar (planlayıcı, Q1/Q2 cevabı): kiriş denetiminin zorunlu kuralı Walk süpercover'dır; eğim katmanı isteğe bağlıdır ve varsayılan KAPALIdır. Plan §3.1(c) ve K7 bu karara göre okunur. Dokunulabilecek dosyalar değişmez: BotCore/NavSegment.h, Tests/BotCoreTests/NavSegmentTests.cpp.
1. BotCore/NavSegment.h: NavCheckSegment imzasına son parametre olarak `bool checkSlope = false` ekle. Mevcut eğim bloğu (Bresenham + EdgeOpen, `SlopeTooSteep`) yalnızca `checkSlope == true` iken çalışsın; `false` iken fonksiyon yalnızca Walk süpercover sonucunu (Ok / OutOfBounds / BlockedCell) döndürsün. NavCheckStep'e de aynı `bool checkSlope = false` parametresini ekle ve aynen ilet. Üstteki dosya yorumunu güncelle: eğim katmanı yalnızca uçları planlayıcı waypoint'i olan tam segmentler için planlayıcıyla tutarlıdır, keyfi alt kirişler için garanti vermez. `NavSegmentVerdict::SlopeTooSteep` kalsın.
2. Tests/BotCoreTests/NavSegmentTests.cpp: (a) `NavSegment_Slope` içindeki tüm eğim denetimlerini `checkSlope = true` ile çağır (`Check` yardımcısına ve `NavCheckSegment` çağrılarına parametre ekle) ve aynı testte "eğim sınırın üstünde" (`2.51`) ızgarasında varsayılan çağrının (`checkSlope` verilmeden) `Ok` döndürdüğünü `CHECK` et; (b) `NavSegment_RealMap_Planner`: planlayıcı segmentlerini `checkSlope = true` ile denetle ve `CHECK_EQ(segmentBad, 0)` koru; 6,75 m kirişleri varsayılan (Walk-only) çağrıyla denetle ve `chordViolations` (`Ok` olmayan her karar) için `CHECK_EQ(chordViolations, 0)` ekle; eğim açıkken kirişlerde kaç `SlopeTooSteep` çıktığını ayrıca say (`chord_slope_opt`) ve yalnızca bilgi olarak yaz (`CHECK` yok). `printf` satırı: `NAVSEG planner: paths=%d segments=%d segment_bad=%d chords=%d chord_violations=%d chord_slope_opt=%d`. Diğer testler (Basic, Corner, Symmetry_Oracle, RealMap_Straight, Perf) varsayılan çağrıyla kalsın.
3. BotCore/NavSegment.h `guard` aşımı (`++guard > guardMax`): `break` yerine `fail(NavSegmentVerdict::OutOfBounds, -1, -1)` çağırıp `res` ile dön (fail-closed). Yorum: "numerical safety net; never reached by a correct traversal".
4. BotCore/NavSegment.h girdi doğrulaması: koordinat dönüşümlerinden (`std::floor` + `(int)`) önce dört girdinin `std::isfinite` olduğunu ve `std::fabs(deger / unit)` değerlerinin `1e9`'u aşmadığını denetle; aksi halde `OutOfBounds` (`cellX = cellZ = -1`, `cellsTouched = 0`) dön. Testler (`NavSegment_Basic`): `NaN`, `+inf` ve `1e12` koordinatlı kirişler `OutOfBounds` dönsün (`<limits>` ekle).
5. BotCore/NavSegment.h `:190,197` vertex toleransı: plan `1e-9` der, kod `1e-12` kullanıyor. Ya toleransı plana (`1e-9`) çek ve testlerin (özellikle `RealMap_Straight` `excess=0`, `graze*1000 <= total`) hâlâ geçtiğini göster, ya da `1e-12`yi korumak için kısa yorum yaz (`t` birimi, hedeflenen metre ölçeği). Hangisini seçtiğini raporuna yaz.
6. `./tools/build.sh Release` ve `Debug` (rc=0, uyarı 0), `./tools/run-tests.sh Release` ve `Debug` (`0 failed`) koş; raporuna yeni `NAVSEG planner:` satırını (beklenen `segment_bad=0 chord_violations=0`) ve `NAVSEG oracle/straight/perf` satırlarını yapıştır; `git diff --check` boş, dosyalar ASCII + CRLF.
```
