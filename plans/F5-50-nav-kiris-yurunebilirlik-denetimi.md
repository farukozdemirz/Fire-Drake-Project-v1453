# F5-50: Kiriş (paket adımı) yürünebilirlik denetimi — `BotCore/NavSegment.h` (CLI-08)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
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

### Tur 3

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-50` (taban `gece/2026-10-02-nav` @ `196857d`); düzeltme kod commit'i `5a803fe`; bu rapor + `Durum: UYGULANDI` ayrı commit.
- Değişen dosyalar ve nedenleri (yalnızca talimattaki iki dosya):
  - `BotCore/NavSegment.h`: traversal döngüsü `while (x != ex || z != ez)` yerine `while (true)` + `const double tNext = (tMaxX < tMaxZ) ? tMaxX : tMaxZ; if (tNext > 1.0 + 1e-9) break;` ile t-tabanlı sonlandırmaya çevrildi. Kiriş parametresi [0,1] olduğundan `t = 1` ötesindeki sınır geçişi kirişe ait değildir; bitiş bir köşede (`tMaxX ≈ tMaxZ ≈ 1`) olduğunda artık ötelenmiyor. `guard` fail-closed ağı (`OutOfBounds`) ve döngü gövdesi (x/z/vertex dalları, `visitMain`) değişmedi. Döngü sonrası "end cell plus far side" ziyareti (`ex/ez` + `endXShift/endZShift`) olduğu gibi bırakıldı; kirişin vardığı hücre ve sınırın öte yanı yine ziyaret edilir. Başka mantık değiştirilmedi.
  - `Tests/BotCoreTests/NavSegmentTests.cpp`:
    - Yardımcı `OracleTouchesOutside` eklendi (0,001 m örnekleme; örnek veya sınır yan-komşusu ızgara dışındaysa `true`). `OutOfBounds` doğrulaması için.
    - Yeni test `NavSegment_EndVertex` (`n = 16`, `unit = 4`, `RingEvents` + `MakeNav`): (a) merkez köşe (32,32)'den 8 yönde, `unit` katı köşe uçlara `repeat = 8 m` kirişler; hepsi açıkta `Ok`, iki yönde de. (b) Kirişin değmediği komşu hücreyi (Liang-Barsky kapalı-kare, `tol = 0`) engelleyerek `Ok`; başlangıç köşesini paylaşan dört hücrenin (`(7,7),(8,7),(7,8),(8,8)`) her birini tek tek engelleyerek `BlockedCell` ve `cellX/cellZ` o hücre. (c) Mutasyonda da her kiriş için `a->b` ve `b->a` aynı karar. (d) Köşeye `1e-10 m` kala biten dört kirişte hiç `OutOfBounds` yok; karar `Ok`/`BlockedCell`, raporlanan hücre ızgara içinde.
    - `NavSegment_Symmetry_Oracle` yeniden düzenlendi: yoğun (%20) ve **seyrek (%5)** iki sentetik 64×64 ızgara; 3000 sürekli rastgele kiriş (yoğun) + 1500 kaydırılmış köşe/ızgara-çizgisi uçlu kiriş (tamsayı uçlar; `unit = 1`'de her tamsayı hücre köşesi/kenarı; seyrek ızgarada `Ok` yüzlerce). Sayaçlar: `ok`, `blocked`, `vertex_chords`, `sym`, `safety`, `graze` (sürekli), `vgraze` (köşe), `excess`, `oob` ve `oob_bad` (C++ `OutOfBounds` ama oracle dışa değmiyor). `CHECK`: `sym=0`, `safety=0`, `excess=0`, `oob_bad=0`, `ok >= 300`, `vertex_chords >= 1000`, sürekli `graze <= %0,1`. Köşeye oturtulmuş kirişler kapalı-kare kuralı gereği köşeye teğet olabildiğinden `vgraze` ayrı sayıldı ve bağlanmadı (konservatiflik sınırı yalnız sürekli kümeye uygulanır). `printf` satırı `ok`, `blocked`, `vertex_chords` sayılarını içerir.
    - `NavSegment_RealMap_Straight` değişmedi (`graze=2/2000`; talimattaki "gerekirse" koşulu gerekmedi).
- Derleme çıktısının son satırları (değişen dosyalar `touch` ile yeniden derlendi, `warning C` = 0):
  - Release: `BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Tests\BotCoreTests.exe` (rc=0)
  - Debug: `BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Debug\Tests\BotCoreTests.exe` (rc=0)
- Test çıktısı (Release ve Debug; `170 tests, 0 failed`; yedi eski `NavSegment_*` + yeni `NavSegment_EndVertex` `[ OK ]`, harita var, SKIPPED yok):
  - `NAVSEG oracle: chords=4494 ok=425 blocked=4002 vertex_chords=1494 sym=0 safety=0 graze=0 vgraze=68 excess=0 oob=67 oob_bad=0`
  - `NAVSEG planner: paths=997 segments=4876 segment_bad=0 chords=35878 chord_violations=0 chord_slope_opt=691`
  - `NAVSEG straight: chords=2000 blocked=1722 sym=0 safety=0 graze=2 excess=0`
  - `NAVSEG perf: chords=20000 out_of_bounds=29 ms_p50=0.000100 ms_p95=0.000200 ms_p99=0.000200` (Release); Debug `ms_p95=0.000700`
- Kabul kriterleri öz-değerlendirme (talimat 1-4):
  - 1 ✔ Döngü `while (true)` + `tNext > 1.0 + 1e-9` ile sonlanıyor; `guard` fail-closed korundu; döngü sonrası end-cell/far-side ziyareti değişmedi; başka mantık değişmedi.
  - 2 ✔ `NavSegment_EndVertex` eklendi: (a) 8 yön `Ok`, (b) değmeyen hücre `Ok` + dört köşe hücresi `BlockedCell`/doğru hücre, (c) mutasyonda simetri, (d) `1e-10 m` kala bitişte `OutOfBounds` yok ve hücre ızgara içinde.
  - 3 ✔ Oracle: en az 1000 köşe/çizgi uçlu kiriş (`vertex_chords=1494`), %5 ikinci ızgara, `ok=425 >= 300`; güvenlik 0, simetri 0, `oob_bad=0`; `printf` `ok/blocked/vertex_chords` yazıyor.
  - 4 ✔ Release/Debug rc=0, uyarı 0; `0 failed`; `segment_bad=0 chord_violations=0`; `perf ms_p95=0.000200 <= 0.02`; `git diff --check` boş; iki dosya ASCII + CRLF (334/334, 843/843 satır).
- Plandan sapmalar:
  1. Talimat 3'te belirtilen "kural aynı" kümesine `thinGraze <= %0,1` sınırı yalnızca sürekli kiriş kümesi için uygulandı; köşeye oturtulmuş kirişlerde köşe teğeti (`vgraze`, 68/1494) doğal ve bağlanmadı. Gerekçe: kapalı-kare kuralı köşeye teğeti bilinçli sayar; 0,001 m örneklemeli kaba oracle tek temas noktasını atlar. `excess=0` ve `safety=0` her iki kümede de korunuyor.
  2. `NavSegment_RealMap_Straight` değiştirilmedi (talimat "gerekirse" diyordu; mevcut `excess=0`, `graze=2` yeterli).
- Açık sorular: yok. Küçük yeniden üretim (`NavCheckSegment(16×16 açık iç, 24,32 -> 32,24)`) artık `Ok` döner (eski hata `BlockedCell (12,0)` idi); `NavSegment_EndVertex` (a) bunu doğrudan sabitler.

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

### Tur 2 — 2026-10-02

- **Karar:** DÜZELTME GEREKLİ
- **İncelenen commit:** `4b2ce1f` (`bot/F5-50`; Tur 2 kodu `046ddc8`; taban `gece/2026-10-02-nav` @ `196857d`). Çalışma ağacı temiz. Paralel hat `nav` (`AUTO_LOOP=1`, gece modu): sunuculara dokunulmadı, birleştirme/push yapılmadı.
- **Özet:** Tur 1 talimatının 1-6. maddeleri doğru ve eksiksiz uygulanmış (eğim katmanı isteğe bağlı, guard fail-closed, girdi doğrulaması, tolerans `1e-9`, `chord_violations=0`). Ancak bağımsız çapraz denetimim, uygulayıcının testlerinin hiç denemediği bir sınıfta **yeni bir doğruluk hatası** buldu: kiriş **tam bir hücre köşesinde (vertex) biterken** `z` azalıyorsa traversal bitiş hücresini aşıyor; sonuç kiriş yürünebilir olduğu hâlde reddedilir (hiçbir zaman `Ok` değil, yani fail-closed) ve raporlanan hücre kirişin hiç değmediği bir hücre ya da `OutOfBounds (-1,-1)` olur. Güvenlik yönü (engelli hücreyi kaçırma) **bozulmadı**.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release rc=0, uyarı yok | ✔ | `NavSegment.h` ve `NavSegmentTests.cpp` `touch` ile yeniden derlendi; `tools/build.sh Release` rc=0, `grep -ci warning` = 0 (`/tmp/f550v2_rel.log`) |
| K2 Debug rc=0, uyarı yok | ✔ | `tools/build.sh Debug` rc=0, uyarı 0 |
| K3 testler `0 failed`, yedi yeni ad `[ OK ]` | ✔ | `./tools/run-tests.sh Release --no-build` ve `Debug --no-build`: `169 tests, 0 failed`; yedi `NavSegment_*` `[ OK ]`, SKIPPED yok (harita var) |
| K4 yasak include yok | ✔ | `NavSegment.h:16,18` yalnızca `NavGrid.h` + `<cmath>`; `windows.h|stdafx|GameServer|shared/` eşleşmesi yalnızca `:4` yorumu |
| K5 dinamik bellek / global-static yok | ✔ | `new|malloc|std::vector|static ` yalnızca `:4` yorumu; diziler yığında (`:163-164`, `:246-247`) |
| K6 oracle güvenlik 0, aşırı muhafazakâr ≤ %0,1, simetri 0 | ✔ (testlerin ölçtüğü dağılımda) | `oracle ... sym=0 safety=0 graze=0 excess=0`, `straight ... graze=2 excess=0` (2/2000 = %0,1 sınırda ama ≤). Not: `oracle` testi 3000 kirişin 2947'sini engelli üretiyor (%20 engel yoğunluğu, uzun kiriş), yani güvenlik yönü yalnızca 53 `Ok` kirişte sınanıyor; vertex'te biten kiriş hiç yok (bkz. bulgu 1) |
| K7 planlayıcı segment/kiriş ihlali 0 | ✔ | `NAVSEG planner: paths=997 segments=4876 segment_bad=0 chords=35878 chord_violations=0 chord_slope_opt=691` (yeniden koşuldu; segmentler `checkSlope=true`, kirişler Walk-only) |
| K8 Perf p95 ≤ 0,02 ms | ✔ | Release `ms_p95=0.000200`; Debug `0.000700` |
| K9 yalnızca §4 dosyaları | ✔ | `git diff --stat gece/2026-10-02-nav...bot/F5-50`: kod commit'leri `fae635d` ve `046ddc8` yalnızca `NavSegment.h`, `NavSegmentTests.cpp`, iki `.vcxproj` (birer satır) + plan; `GameServer/`, `shared/`, `AIServer/` farkı 0. (`docs/12`, `docs/STATUS.md` farkı Claude'un Tur 1 doğrulama commit'idir `bbd68c7`.) |
| K10 ASCII + CRLF, `git diff --check` boş | ✔ (iki yeni dosya) | iki dosya ASCII + CRLF; kod/proje dosyalarında `git diff --check` temiz (taban...dal farkında yalnızca Claude'un `docs/STATUS.md` satırı CR/`core.autocrlf` uyarısı verir, uygulayıcı dosyası değil) |
| K11 bağımsız Python süpercover çapraz denetimi | ✘ | Aşağıda: güvenlik ihlali 0 (6000 kiriş), ama 255 gereksiz ret / yanlış hücre raporu, hepsi bitiş-vertex sınıfında |

**K11 yöntemi ve sonucu.** `/tmp/f550k11/` (commit edilmez): C++ harness (`g++ -O2`, gerçek harita `zone71`, `NavSegment.h` olduğu gibi) 6000 kiriş kararı üretti: 2500 rastgele sürekli uçlu (R), 2500 köşe/çizgi/merkez-çapraz uçlu (Q), 1000 vertex'e `1e-13..1e-4` m yakın uçlu (N); her kiriş iki yönde. Python, `Fraction` ile tam rasyonel kapalı-kare kesişimi hesapladı. Sonuç:

- **Güvenlik ihlali (C++ `Ok`, kesin sonuç engelli): 0 / 6000.** Simetri ihlali: 0.
- R (2500, sürekli uçlar): `ok=1639 blocked=861`, **gereksiz ret 0, yanlış hücre 0**.
- Q: 90 gereksiz `BlockedCell`, 46 gereksiz `OutOfBounds(-1,-1)`; N: 66 gereksiz `BlockedCell`, 36 gereksiz `OutOfBounds`. Toplam 238 gereksiz ret + 17 "ret doğru ama raporlanan hücre kirişe değmiyor" = 255. Hepsi uçlardan biri hücre köşesinde veya köşeye ≤ ~1e-8 m yakın olan kirişlerdir (kiriş uç koordinatları `unit` katı).
- Küçük yeniden üretim (sentetik 16x16 ızgara, kenar halkası engelli, iç hücreler açık, `unit = 4`; hepsi `Walk`): `NavCheckSegment(grid, 24, 32, 32, 24)` → `BlockedCell`, `cell=(12,0)`, `cellsTouched=25`. Kirişin değdiği hücreler yalnızca `x 5..8, z 5..8` aralığıdır, hepsi `Walk`; dönen `(12,0)` ızgara kenarındaki engelli hücredir. Kontrol: `(24,24) -> (32,32)` doğru `Ok` döner (`dz > 0`).

**Bulgular (önem sırasıyla)**

1. **Doğruluk, `BotCore/NavSegment.h:195-240` (bitiş hücresi aşımı):** döngü `x != ex || z != ez` koşuluyla biter; `ex/ez = floor(b / unit)`. Kanonik sıralamadan sonra `stepX >= 0`. Bitiş noktası `z` sınırında ve `stepZ < 0` ise kirişin vardığı alt hücre `ez - 1`'dir, ama `ez` üst hücredir. Bitişte `tMaxX ≈ tMaxZ ≈ 1` eşitliğinde vertex dalı (`:224-239`) `x = ex`, `z = ez - 1` yapar; artık `z == ez` hiçbir zaman sağlanamaz, döngü bitmez: sonraki `z` adımları (`:217-223`) kirişin dışındaki hücreleri ziyaret eder ve ilk engelli/ızgara dışı hücrede `BlockedCell` (kirişe değmeyen hücre) ya da guard'da `OutOfBounds(-1,-1)` verir. Sonuç hep ret (fail-closed), `Ok` asla; ama (a) yürünebilir bir kiriş reddedilir, (b) raporlanan `cellX/cellZ` yanlıştır (telemetri/teşhis yanıltır), (c) `OutOfBounds` etiketi yanlış. Pratik olasılık küçümsenmemeli: dünya koordinatları tamsayı olan hedefler (NPC/spawn konumu) `unit = 4` ızgarada 1/16 olasılıkla köşeye düşer. Aynı mekanizma, tolerans (`1e-9` t birimi) içinde köşeye ≤ ~1e-8 m yakın biten kirişlerde de tetiklenir (N sınıfı, 46 vaka `dz >= 0`). Testler yakalamadı çünkü hem `NavSegment_Corner` hem oracle testleri köşede *biten* kirişi denemiyor ve rastgele sürekli uçlar köşeye (ölçü sıfır) düşmüyor.
2. **Test boşluğu, `Tests/BotCoreTests/NavSegmentTests.cpp` (Corner/Symmetry_Oracle):** oracle testi 3000 kirişin %98'ini engelli üretiyor; `Ok` yönü yalnızca 53 kirişte sınanıyor ve uçlar hiç ızgara çizgisi/köşesi üzerinde değil. Plan §5 (`NavSegment_Corner`) köşe/teğet vakalarını açıkça ister; bitiş-köşesi vakası eksik.
3. **Not (engel değil), `NavSegment.h:70-78` dışı:** yok; Tur 1 bulgularının (3-5) hepsi giderildi: `guard` fail-closed (`:203-208`), `isfinite` + `1e9` (`:57-68`), tolerans `1e-9` (`:210`, `:217`; boundaryShift de `1e-9`, `:115-119`). `NavSegment_Basic` `NaN/+inf/1e12` testi `[ OK ]`.
4. **Dürüstlük:** Uygulayıcı Tur 2 raporu doğru; tüm sayılar (`691`, `4876`, `35878`, `169 tests`, `p95`) yeniden üretildi. Rapor "tolerans `1e-9`'a çekildi" diyor; bunun vertex-biten kirişi bozduğunu testler göstermiyordu (bulgu 1, Tur 1 kodunda da var olan bir hata olabilir; bu turun gerilemesi değil, K11 kapsamının genişlemesiyle bulundu).

#### Düzeltme talimatı

```
plans/F5-50-nav-kiris-yurunebilirlik-denetimi.md — Doğrulama Turu 2 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 3" ekle:
Dokunulabilecek dosyalar değişmez: BotCore/NavSegment.h, Tests/BotCoreTests/NavSegmentTests.cpp.
1. BotCore/NavSegment.h: kirişin bir hücre köşesinde (veya tolerans içinde köşeye yakın) bittiği durumda traversal bitiş hücresini aşıyor. Döngü sonlanmasını `x != ex || z != ez` koşulundan t-tabanlı sonlanmaya çevir: `while (true)` içinde `const double tNext = (tMaxX < tMaxZ) ? tMaxX : tMaxZ; if (tNext > 1.0 + 1e-9) break;` (kirişin t parametresi [0,1]; `1e-9` mevcut vertex toleransıyla aynı ölçek, büyütme). Mevcut `guard` fail-closed ağını koru. Bitişteki "end cell plus far side" ziyaretini (`ex/ez` ve `endXShift/endZShift`) olduğu gibi bırak: kirişin vardığı hücreyi ve sınırın öte yanını yine ziyaret eder. Başka mantık değiştirme.
2. NavSegmentTests.cpp: yeni test `NavSegment_EndVertex` ekle (sabit ad). Sentetik ızgara: n=16, unit=4, kenar halkası engelli iç açık (dosyadaki `RingEvents` + `MakeNav`). (a) Yürünebilir bölgede, `unit` katı köşe noktaları arasında 8 yönde 45° ve eksen hizalı kirişler, hem başlangıç hem bitiş köşede (ör. (24,32)->(32,24), (32,24)->(24,32), (24,24)->(32,32), (24,32)->(24,24), (24,24)->(32,24)): hepsi `Ok`. (b) Aynı kirişleri köşenin öte yanındaki (kirişe değmeyen) tek bir hücreyi engelleyerek `Ok` bekle; kirişin köşede değdiği dört hücreden birini engelleyerek `BlockedCell` ve `cellX/cellZ` o hücre bekle. (c) Simetri: her kiriş için a->b ve b->a aynı karar. (d) Köşeye 1e-10 m kala biten kirişte hiçbir zaman `OutOfBounds` dönmesin (karar `Ok` ya da `BlockedCell`, `cellX/cellZ` ızgara içinde).
3. NavSegmentTests.cpp `NavSegment_Symmetry_Oracle` (ve gerekirse `RealMap_Straight`): oracle kirişlerinin bir kısmını (en az 1000) uçları `unit` katı köşelere / ızgara çizgilerine oturtarak üret ve ayrıca engel yoğunluğu düşük (%5) ikinci bir sentetik ızgara kullan ki `Ok` kiriş sayısı yüzlerce olsun. Kural aynı: güvenlik ihlali 0, simetri ihlali 0, ve `OutOfBounds` yalnızca kiriş gerçekten ızgara dışına çıkıyorsa dönsün (oracle: `OutOfBounds` ise ızgara dışına değen hücre olmalı). Bir `printf` satırıyla `ok`, `blocked`, `vertex_chords` sayılarını yaz.
4. `./tools/build.sh Release` ve `Debug` (rc=0, uyarı 0), `./tools/run-tests.sh Release` ve `Debug` (`0 failed`) koş; raporuna `NAVSEG planner:` (beklenen `segment_bad=0 chord_violations=0`), `NAVSEG oracle/straight/perf` ve yeni satırları yapıştır. `NAVSEG planner` `chord_violations=0` kalmalı, `perf` `ms_p95 <= 0.02` kalmalı. `git diff --check` boş, dosyalar ASCII + CRLF.
```

### Tur 3 — 2026-10-02 (otonom döngü kurtarma adımında Claude)

- **Karar:** DOĞRULANDI
- **İncelenen commit:** `4c2dfd0` (`bot/F5-50`; Tur 3 kodu `5a803fe`; taban `gece/2026-10-02-nav` @ `196857d`). Çalışma ağacı temiz. Döngü, `/plan-dogrula` sınırına takılıp kurtarma çağırdığı için bu doğrulamayı kurtarma oturumunda yaptım; birleştirme/push yapılmadı (gece modu: döngü betiği birleştirir).
- **Özet:** Tur 2 bulgusu (kiriş köşede biterken bitiş hücresinin aşılması) giderildi: traversal sonlanması t-tabanlı. Bağımsız çapraz denetimde güvenlik ve simetri ihlali yok; R/Q sınıflarında (sürekli ve köşe/çizgi uçlu 5000 kiriş) gereksiz ret ve yanlış hücre raporu **0** (Tur 2'de 255 vaka).

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release rc=0, uyarı yok | ✔ | `tools/build.sh Release` rc=0, `grep -ci warning` = 0 |
| K2 Debug rc=0, uyarı yok | ✔ | `tools/build.sh Debug` rc=0, uyarı 0 |
| K3 `0 failed`, yedi yeni ad + `NavSegment_EndVertex` `[ OK ]` | ✔ | Release ve Debug `170 tests, 0 failed`; harita var, SKIPPED yok |
| K4/K5 include, dinamik bellek, global-static | ✔ | Tur 2'den beri `NavSegment.h` yalnızca döngü koşulunu değiştirdi; yeni include/bellek yok |
| K6 oracle | ✔ | `oracle: chords=4494 ok=425 blocked=4002 vertex_chords=1494 sym=0 safety=0 graze=0 vgraze=68 excess=0 oob=67 oob_bad=0`; `straight: sym=0 safety=0 graze=2 excess=0` |
| K7 planlayıcı | ✔ | `planner: paths=997 segments=4876 segment_bad=0 chords=35878 chord_violations=0 chord_slope_opt=691` |
| K8 Perf | ✔ | Release `ms_p95=0.000200` |
| K9 yalnızca §4 dosyaları | ✔ | `git diff --stat gece/2026-10-02-nav...bot/F5-50`: `NavSegment.h`, `NavSegmentTests.cpp`, iki `.vcxproj`, plan + Claude'un `docs/12`, `docs/STATUS.md`, `plans/README.md` satırları; `GameServer/`, `shared/`, `AIServer/` farkı 0 |
| K10 ASCII + CRLF, `git diff --check` | ✔ | iki dosya ASCII + CRLF; `git diff --check` rc=0 |
| K11 bağımsız Python tam-rasyonel denetim | ✔ | Aşağıda |

**K11.** Tur 2 harness'i (`/tmp/f550k11`, commit edilmez) yeni `NavSegment.h` ile yeniden koşuldu: 6000 kiriş (R 2500 sürekli, Q 2500 köşe/çizgi/merkez-çapraz uçlu, N 1000 köşeye `1e-13..1e-4` m yakın uçlu). **Güvenlik ihlali 0, simetri ihlali 0.** R ve Q: gereksiz ret 0, yanlış hücre 0. N: 29 gereksiz ret ve 46 "ret doğru ama raporlanan hücre tam kesişimle uyuşmuyor"; hepsinde uç, köşeye ~1e-9 m mertebesinde (vertex toleransı içinde, örn. `683.9999999995`) yakındır, yani kasıtlı muhafazakâr tolerans sınıfıdır (plan §8: "eşitlik toleransı `1e-9` ölçeğindedir; büyütme"; yön her zaman fail-closed). Tur 2'deki köşe-bitişi hatası (Q'da 136, N'de 102 vaka) kalmadı.

**Not (engel değil).** `chord_slope_opt=691` bilgi amaçlıdır: eğim katmanı isteğe bağlı ve varsayılan kapalı (Tur 1 kararı). F5-55 guard'ı kirişleri Walk-only çağırmalı; bağlamlı eğim denetimi gerekirse orada değerlendirilir.
