# F5-58: Duvar denetimi bulgusunun kalıcı regresyonu: ham yol, düzleştirme, `NavLineClear` ve düz adım (`NavSegmentAuditTests`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-58 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | **F5-50** (`BotCore/NavSegment.h`, `NavCheckSegment`) — `KAPANDI` olmalı; F5-01/F5-02/F5-03 `KAPANDI` |
| İlgili gereksinim / kabul | CLI-08, AC-NAV-03, `docs/12` §13.1; proje sahibi kararı 2026-10-02 (madde 5: "duvar denetimi bulgusunu netleştir: sorun ham yolda mı, düzleştirmede mi, doğrulama yönteminde mi? yeniden üretilebilir bir örnekle göster" ve madde 7: geçici betiklere dayanan ölçümleri kalıcı testlere/araçlara çevir); `docs/reports/degerlendirme-2026-10-02-ek.md` §3 |
| Tahmini büyüklük | S (1 yeni test dosyası + 1 proje satırı; `BotCore/*` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (proje sahibi kararı sonrası) |

---

## 1. Amaç

Değerlendirme, "yol yalnızca bitiş noktası değil segmentleriyle de engel açısından denetlenmeli" bulgusunu **yeniden üretilebilir ölçümle** sınıflandırdı (`tools/nav-measure.sh smoothing --n 6000`, tohum 20261002, zone 71, `gece/2026-10-02-nav` @ `196857d`; bağımsız çapraz kontrol `tools/nav-segment-check.py`):

| Katman | Örnek | Engelli hücreye değen | Sonuç |
|---|---|---|---|
| Ham A* yolu (hücre merkezi → hücre merkezi kenarları) | 379 054 kenar | **0** | sorun ham yolda değil |
| Düzleştirilmiş yol segmentleri (`NavSmoothPath`) | 33 365 segment | **0** | sorun düzleştirmede değil |
| 6,75 m paket kirişleri (düzleştirilmiş yol üzerinde) | 250 000 kiriş | **0** | köşe kesen kiriş bulunmadı |
| `NavLineClear` (Bresenham + `EdgeOpen`) "açık" dediği çiftler | 360 288 çift (+ sentetik 270 320 tek engel, 20 784 rastgele kalabalık) | **0 yanlış-pozitif** | doğrulama yöntemi bu testlerde muhafazakâr |
| **Planlayıcısız düz hedef adımı** (`/bot move`/`BeginMove` gibi, 2–3 hücre) | 6000 çift | **447 (%7,45)** | **asıl açık: icrada yürünebilirlik denetimi yok (CLI-08)** |

Bu tablo bugün geçici bir betik değil `tools/nav-measure.sh` ile üretilir; ama **birim test paketinde** kalıcı bir koruma yoktur: planlayıcı/düzleştirme/`NavLineClear` değişirse (ör. F5-08/F5-09 sonrası, hiyerarşik arama, düzleştirme parametreleri) bir ihlal sessizce girebilir. Bu plan, tabloyu F5-50'nin `NavCheckSegment`'iyle **kalıcı regresyon testlerine** çevirir ve düz adım açığını sabit koordinatlı örneklerle sabitler. Üretim kodu yazılmaz.

## 2. Bağlam (okunması zorunlu)

- `plans/F5-50-nav-kiris-yurunebilirlik-denetimi.md` (API: `NavCheckSegment(grid, ax, az, bx, bz)` → `NavSegmentResult { verdict, cellX, cellZ, cellsTouched }`, `NavCheckStep`); `docs/12` §13.1.
- `BotCore/NavPath.h` `NavPathfinder::Find`, `BotCore/NavSmooth.h` `NavSmoothPath`/`NavLineClear`, `BotCore/NavGrid.h` (`Walk`, `CellCenter`, `Unit`).
- Ölçüm aracı `tools/nav-measure/nav_measure.cpp` bölüm `smoothing`/`synthetic` (yöntemin referans uygulaması: aynı sayımlar, aynı tohum düzeni) ve `tools/nav-segment-check.py` (bağımsız oracle).
- Test düzeni: `Tests/BotCoreTests/NavSmoothTests.cpp` (gerçek harita `build/nav/zone71.navgrid`, yoksa `SKIPPED`, `tools/nav-export.py`).

## 3. Kapsam

**Yapılacaklar**

1. `Tests/BotCoreTests/NavSegmentAuditTests.cpp` (yeni) + `Tests/BotCoreTests/BotCoreTests.vcxproj` `ClCompile` satırı. Hiçbir `BotCore/*` dosyası değişmez.
2. Üç `TEST_CASE` (§5.3), hepsi gerçek harita yoksa `SKIPPED` (kabul koşusunda harita **olmalı**).

**Kapsam dışı**

- `NavCheckSegment` uygulaması (F5-50), `NavSmoothPath`/`NavLineClear`'i süpercover'a çevirmek (ölçümde ihlal yok; bir test **ihlal bulursa** bu bir bulgudur ve ayrı plan açılır), sunucu guard'ı (F5-55), `docs/`.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `Tests/BotCoreTests/NavSegmentAuditTests.cpp` | yeni | |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | tek `ClCompile` satırı |

Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-58 gece/2026-10-02-nav` (F5-50 birleşmiş olmalı); `Durum` → `UYGULANIYOR`; `python3 tools/nav-export.py`.
2. `NavSegmentAuditTests.cpp` (ASCII + CRLF). Test düzeni `NavSmoothTests.cpp` ile aynı (`MiniTest.h`, `Rng`).
3. Testler (adlar sabit):
   - `NavSegmentAudit_Planner`: 3000 karışık menzilli A* yolu (tohum 20261002; Chebyshev üst sınırı 40/64/150 dönüşümlü; `NavPathfinder::Find` + `NavSmoothPath` varsayılan parametreler): (a) ham yolun her kenarı, (b) düzleştirilmiş yolun her segmenti, (c) düzleştirilmiş yol boyunca 6,75 m'lik her paket kirişi `NavCheckSegment` ile `Ok` olmalı. Tek satır çıktı: `NAVAUDIT planner paths=<n> raw_edges=<n> raw_bad=<n> smooth_segments=<n> smooth_bad=<n> chords=<n> chord_bad=<n>`; `*_bad` hepsi **0**; ihlal olursa ilk 3 örnek dünya koordinatları ve engelli hücreyle yazdırılır ve test başarısız olur (bulgu).
   - `NavSegmentAudit_LineClear`: aynı yol havuzundan en çok 20 000 hücre çifti: `NavLineClear` "açık" ise `NavCheckSegment` `Ok` olmalı (yanlış-pozitif **0**); sentetik 13×13 ızgara (çerçeve engelli, iç 9×9 açık): her tek-engel konumu × tüm hücre çiftleri (≈ 310 000 deneme) ve 3000 rastgele kalabalık ızgara (`Rng` sabit tohum, %22 engel, 40 çift) aynı kural. Satır: `NAVAUDIT lineclear pairs=<n> clear=<n> false_positive=<n>` (`false_positive = 0`). **İhlal bulunursa bu, ölçümde bulunamayan kuramsal açığın kanıtıdır:** test başarısız olur, ızgara düzeni ve çift raporda yazılır.
   - `NavSegmentAudit_StraightSteps`: sabit regresyon vektörleri (zone 71 dünya metresi; iki uç da yürünebilir hücrede, düz adım engelli hücreye değiyor): `(1566.0, 878.0)→(1578.0, 866.0)`, `(926.0, 834.0)→(934.0, 826.0)`, `(1102.0, 1034.0)→(1106.0, 1046.0)` her biri `NavCheckSegment` = `BlockedCell` ve raporlanan hücre `Walk` değil; `(1274.0, 890.0)→(1280.0, 890.0)` `Ok`. Ek: 6000 rastgele (tohum 20261002) 2–3 hücrelik düz adımda engelli oran yazdırılır (`NAVAUDIT straight pairs=<n> blocked=<n>`), kabul yalnızca `blocked > 0` (beklenen ≈ %7,45; planlayıcısız hareketin yürünebilirlik garantisi vermediğinin kanıtı).
4. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni dosya için uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; üç yeni test adı `[ OK ]` (kabul koşusunda harita var, `SKIPPED` değil); mevcut testler değişmeden geçer
- [ ] K4: `NAVAUDIT planner` satırında `raw_bad = smooth_bad = chord_bad = 0` ve `NAVAUDIT lineclear` satırında `false_positive = 0`; satırlar Uygulayıcı Raporu'nda
- [ ] K5: `NavSegmentAudit_StraightSteps` üç sabit vektör `BlockedCell`, kontrol vektörü `Ok`
- [ ] K6: `git diff gece/2026-10-02-nav...bot/F5-58 --stat` yalnızca §4'teki iki dosya (+ plan); `BotCore/`, `GameServer/`, `shared/`, `docs/` farkı 0; ASCII + CRLF; `git diff --check` boş
- [ ] K7 (Claude): `tools/nav-measure.sh smoothing --n 6000` ve `tools/nav-segment-check.py` çıktılarıyla sayımları karşılaştırır (güncel kod üzerinde yeniden ölçüm; aynı sıfırlar, `STRAIGHT blocked > 0`)
- [ ] K8 (**oyun içi kanıt, bu planda kapanmaz**): kiriş guard'ı sunucuya bağlandığında düz `/bot move` engel kesen adımı `FAIRNESS_REJECT CLI-08` ile durdurur, planlayıcı yollarında CLI-08 reddi 0 (F5-55, AC-NAV-03); `docs/reports/degerlendirme-takip.md` satırı kanıta kadar `BEKLİYOR`

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavSegmentAudit_|NAVAUDIT|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02-nav...bot/F5-58
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. Test başarısızlığı **bulgudur**: eşiği gevşetme, koordinatlarla raporla.
- Bu plan yalnızca testtir; `NavSmoothPath`/`NavLineClear` değişikliği gerektiren bir ihlal çıkarsa ayrı plan açılır.

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
