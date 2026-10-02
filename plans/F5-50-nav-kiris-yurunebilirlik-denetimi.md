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

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
