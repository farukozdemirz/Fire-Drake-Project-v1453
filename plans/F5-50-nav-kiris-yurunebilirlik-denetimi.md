# F5-50: Kiriş (paket adımı) yürünebilirlik denetimi — `BotCore/NavSegment.h` (CLI-08) ve duvar bulgusunun kalıcı denetimi

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-50 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F5-01 (`NavGrid`), F5-02 (`NavPathfinder`), F5-03 (`NavSmoothPath`) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-08 (yürünebilirlik), `docs/12` §13.1 (kiriş denetimi), AC-NAV-03 (engelli hücreye giren hareket = 0); `docs/reports/degerlendirme-2026-10-02.md` DEG-18 |
| Tahmini büyüklük | S–M (2 yeni dosya + 2 proje dosyası satırı; sunucuya dokunulmaz) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

Bot hareket paketi ~1,5 sn'de bir **hedef noktayı** taşır ve sunucu iki paket arasındaki yolu doğrulamaz (MEC-MOV-03; yürüyüşte ~6,75 m, sprintte ~10 m, 4 m ızgarada 2–3 hücre). Bugün hiçbir katman *kirişi* (önceki paket konumu → yeni konum) denetlemez: planlayıcı çıktısı güvenli görünse de (`NavSmoothPath` kontrolü hücre-merkezi Bresenham'dır, süpercover değildir) icra tarafında tek koruma bu denetim olacaktır (düz `/bot move`, planlayıcı dışı kaynaklar, gelecekteki değişiklikler). Bu plan kirişi **muhafazakâr süpercover** ile denetleyen saf mantığı ekler: kirişin kapalı kareye değdiği **her** hücre `Walk` olmalı (hücre köşesi/vertex'ine değmek 4 komşuyu da sayar) ve geçilen hücre çiftlerinde `maxSlope` kuralı sağlanmalı. Sunucu entegrasyonu F5-55'tir (burada yok).

**Duvar bulgusunun sınıflandırması (proje sahibi sorusu: ham yolda mı, düzleştirmede mi, doğrulama yönteminde mi?) — yeniden üretilebilir ölçüm** (`tools/nav-measure.sh smoothing --n 6000`, tohum 20261002, zone 71, WSL `g++ -O2`, `gece/2026-10-02-nav` @ `196857d` başlıkları; bağımsız çapraz kontrol `tools/nav-segment-check.py`):

| Katman | Örnek | Engelli hücreye değen | Yorum |
|---|---|---|---|
| Ham A* yolu (hücre merkezi → hücre merkezi kenarları) | 379 054 kenar | **0** | sorun ham yolda değil |
| Düzleştirilmiş yol segmentleri (`NavSmoothPath`) | 33 365 segment | **0** | sorun düzleştirmede değil |
| 6,75 m paket kirişleri (düzleştirilmiş yol üzerinde) | 250 000 kiriş | **0** | köşe kesen kiriş bu haritada bulunmadı |
| Doğrulama yöntemi `NavLineClear` (Bresenham + `EdgeOpen`): "açık" dediği çiftler | 360 288 açık çift | **0 yanlış-pozitif** (sentetik: tek engel 270 320, rastgele kalabalık 20 784 açık çift: **0**) | yöntem bu testlerde muhafazakâr; kuramsal bir açık gösterilemedi |
| **Planlayıcısız düz hedef adımı** (2–3 hücre uzakta iki yürünebilir hücre arası, `/bot move`/`BeginMove` gibi) | 6000 çift | **447 (%7,45)** engelli hücreye değiyor | **asıl açık: icrada yürünebilirlik denetimi yok (CLI-08)** |

Yeniden üretilebilir örnek (zone 71, dünya metresi; iki uç da yürünebilir hücrede, düz adım engelli hücreye değiyor; `tools/nav-segment-check.py` ile doğrulanır): `(1566.0, 878.0) → (1578.0, 866.0)` (engelli hücre (392,219) civarı), `(926.0, 834.0) → (934.0, 826.0)` (hücre (231,207)), `(1102.0, 1034.0) → (1106.0, 1046.0)` (hücre (275,259)). Sonuç: bulgu **doğrulama yönteminin icra katmanında eksikliğidir** (planlayıcı çıktısı güvenli, `NavLineClear` muhafazakâr), ham yol ve düzleştirme değil. Bu plan icradaki denetimi saf mantık olarak ekler ve yukarıdaki tabloyu **kalıcı regresyon testine** çevirir (§5.3 `NavSegment_Audit_*`).

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.1 (kural), §1 (olay ızgarası `0 = engelli`, indeks `x·n + z`), `docs/03` CLI-05/CLI-08.
- `BotCore/NavGrid.h`: `Walk(x,z)`, `Height(x,z)`, `Params().maxSlope`, `EdgeOpen` (komşu hücre kuralı: `Walk` + eğim + çapraz köşede iki ortogonal komşu `Walk`), `CellOf`, `CellCenter`, `Unit()`.
- `BotCore/NavSmooth.h` `NavLineClear`: hücre merkezleri arası Bresenham + `EdgeOpen` (bu plan onu **değiştirmez**).
- Kalıcı ölçüm araçları: `tools/nav-measure.sh` (bölüm `smoothing`, `synthetic`) ve `tools/nav-segment-check.py`. Test düzeni: `Tests/BotCoreTests/NavSmoothTests.cpp` (sentetik ızgara yardımcıları, gerçek harita `build/nav/zone71.navgrid` yoksa `SKIPPED`, `tools/nav-export.py`).

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
   - `NavSegment_Audit_Planner` (gerçek harita yoksa `SKIPPED`): 3000 karışık menzilli A* yolu (tohum 20261002; `Chebyshev` ≤ 40/64/150 dönüşümlü): ham yol kenarları, `NavSmoothPath` segmentleri ve 6,75 m paket kirişlerinin **hepsi** `NavCheckSegment` ile `Ok`; satır çıktısı `raw_edges raw_bad smooth_segments smooth_bad chords chord_bad` (hepsi `*_bad = 0`); ihlal olursa ilk 3 örneği dünya koordinatlarıyla yazdırır.
   - `NavSegment_Audit_LineClear` (harita yoksa `SKIPPED`): `NavLineClear` "açık" dediği (en çok 20 000) çift için `NavCheckSegment` `Ok` olmalı (yanlış-pozitif 0); sentetik 13×13 ızgarada tek engel ve rastgele kalabalık (3000 ızgara) aynı kural; ihlal bulunursa **bu bir bulgudur**: test başarısız olur ve rapora koordinat/ızgara düzeniyle yazılır (kuramsal açığın kanıtı).
   - `NavSegment_Audit_StraightSteps` (harita yoksa `SKIPPED`): sabit regresyon vektörleri: `(1566.0,878.0)→(1578.0,866.0)`, `(926.0,834.0)→(934.0,826.0)`, `(1102.0,1034.0)→(1106.0,1046.0)` her biri `BlockedCell` (ve raporlanan hücre `Walk` değil), `(1274.0,890.0)→(1280.0,890.0)` `Ok`; ayrıca 6000 rastgele 2–3 hücrelik düz adımda engelli oran bilgi olarak yazdırılır (beklenen ≈ %7; yalnızca `> 0` doğrulanır).
   - `NavSegment_Perf`: 10 m kirişler, 20 000 çağrı; `ms_p95` yazdırılır; kabul `p95 ≤ 0.02 ms` (Release; Debug için eşik yok, yalnız yazdırılır).
4. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni dosyalar için uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; on yeni test adı `[ OK ]` (yedi temel + `NavSegment_Audit_Planner`, `NavSegment_Audit_LineClear`, `NavSegment_Audit_StraightSteps`; gerçek harita testleri harita varsa `[ OK ]`, yoksa `SKIPPED`: kabul koşusunda harita **var**); mevcut testler değişmeden geçer
- [ ] K4: `BotCore/NavSegment.h`'te `windows.h|stdafx|GameServer|shared/` grep'i boş; `#include` yalnızca standart kütüphane ve `NavGrid.h`
- [ ] K5: dinamik bellek yok (`new|malloc|std::vector` yok `NavSegment.h` içinde), global/static durum yok
- [ ] K6: oracle testinde güvenlik yönü ihlali 0 (`Ok` dediği kirişte oracle engelli bulmuyor) ve fazla muhafazakâr karar oranı ≤ %0,1 (3000 + 2000 kiriş); simetri ihlali 0
- [ ] K7: gerçek harita: `NavSegment_Audit_Planner` ham/düzleştirme/kiriş ihlali **0** ve `NavSegment_Audit_LineClear` yanlış-pozitif **0** (satır çıktıları raporda); `NavSegment_Audit_StraightSteps` üç sabit vektör `BlockedCell`
- [ ] K8: `NavSegment_Perf` Release `ms_p95 ≤ 0.02`
- [ ] K9: `git diff gece/2026-10-02-nav...bot/F5-50 --stat` yalnızca §4'teki dosyalar (+ plan); `GameServer/`, `shared/`, `AIServer/`, `docs/` farkı 0
- [ ] K10: ASCII + CRLF; `git diff --check` boş
- [ ] K11 (Claude yapar): oracle/gerçek harita sonuçlarını `tools/nav-segment-check.py` (bağımsız Python süpercover; `--selftest` PASS) ile rastgele 500 kirişte çapraz doğrular ve `tools/nav-measure.sh smoothing --n 6000` çıktısıyla `*_bad = 0`, `false_positive = 0`, `STRAIGHT blocked > 0` karşılaştırır (güncel kod üzerinde yeniden ölçüm)
- [ ] K12 (oyun içi kanıt, F5-55'e devredilir, bu planda kapatılmaz): kiriş guard'ı sunucuya bağlanınca düz `/bot move` engel kesen adımı `FAIRNESS_REJECT CLI-08` ile durdurur (T-NAV-04/AC-NAV-03); **bu plan "oyun içinde doğrulandı" durumuna geçmez**

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

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu:
- Kabul kriterleri öz-değerlendirme:
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
