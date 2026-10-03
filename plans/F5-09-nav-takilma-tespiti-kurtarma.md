# F5-09: Takılma tespiti ve aşamalı kurtarma (`BotCore/NavStuck.h`; ilerlemesizlik/salınım dedektörü + kurtarma merdiveni + yan adım + dinamik ceza)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02-nav) |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat, `docs/17` §1 "Paralel yürütülebilir işler") |
| Branch | `bot/F5-09` (taban: `gece/2026-10-02-nav`) |
| Bağımlı olduğu planlar | F5-01 (`BotCore/NavGrid.h`), F5-02 (`BotCore/NavPath.h`: `NavCell`, `NavPathfinder`), F5-03 (`BotCore/NavSmooth.h`: `NavLineClear`), F5-04 (`BotCore/NavTrack.h`: `NavRingCells`), F5-06 (`BotCore/NavDanger.h`: `NavCostLayer`, `NavCostField`): hepsi `KAPANDI`, `gece/2026-10-02-nav` içinde (F5-08 merge `69ced3b`); bu planın testleri 129 testin üstüne eklenir |
| İlgili gereksinim / kabul | `docs/12` §10 (takılma tespiti ve aşamalı kurtarma; bu plana göre güncellendi), T-NAV-04, MET-NAV-01/02 (`docs/16`), AC-NAV-01; ADR-0006 Eki F5-09 (bu planın kararları), ADR-0016 (`BotCore` saflığı) |
| Tahmini büyüklük | S (4 dosya: 2 yeni, 2 proje satırı; ~ 450 satır başlık, ~ 1 100 satır test) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Bir bot hedefe yürürken takılırsa (duvara/dar geçide dayanma, iki hücre arasında gidip gelme) bunu ≤ 1,5 sn'de fark etmeli ve aşamalı bir merdivenle kurtulmaya çalışmalı, kurtulamazsa hedefi bırakmalıdır (`docs/12` §10, AC-NAV-01: ≤ 2 takılma / bot-saat, MET-NAV-02: kurtarma p95 ≤ 5 sn). Bu plan bunun saf mantığını yazar; dört bağımsız araç:

1. **`NavStuckDetector`:** zaman damgalı konum örneklerinden iki kural: *ilerlemesizlik* (hareket halindeyken 1,5 sn'de net yer değiştirme < 1 m) ve *salınım* (4 sn'de aynı iki hücre arasında ≥ 3 geçiş).
2. **`NavStuckMonitor`:** dedektörün üstünde kurtarma merdiveni durum makinesi (aşama 1 yeniden planla 0,5 sn → 2 yan adım 1 sn → 3 geri çekil 1,5 sn → 4 ceza + yeniden planla 1 sn → 5 bırak). Her çağrıda en çok bir "şimdi şunu yap" adımı döndürür; eylemi **çağıran** yürütür. Kurtarma başarısı, "tırmanma belleği" ve MET-NAV-02 için kurtarma süresi burada hesaplanır.
3. **`NavPickSideStep`:** aşama 2'nin hedef hücresi (en yakın yüksek açıklıklı, görünür, yürüme yönüne yan hücre).
4. **`NavStuckPenalties`:** aşama 4'ün 60 sn'lik dinamik cezası; `NavCostLayer`'a tehlike olarak işlenir, `NavPath.h` değişmez.

T-NAV-04'ün (dar geçit/köprü takılma oranı) birim düzeyindeki karşılığı olarak, tek hücrelik gizli bir engel ve üç hücrelik gizli bir duvar önünde yürüyen sanal botun merdivenle çözülmesi (ve çözülemezse bırakması) test içi bir simülasyonla sınanır.

Bu planın sonunda **telemetri olayları (`NAV_STUCK`, `NAV_RECOVERY`, `NAV_STUCK_ABANDON`), takılma ısı haritası, takıma bildirim, test modu `TEST_TELEPORT` (aşama T), `NavFollower`/`ActionExecutor` bağlaması (gerçek yol ilerlemesi, eylemlerin yürütülmesi), yol-ilerlemesi tabanlı ilerleme ölçüsü ve sunucu entegrasyonu yoktur.** `GameServer/`, `AIServer/`, `shared/` değişmez; sunucu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0006-navigasyon-izgara-astar.md` — **Eki F5-09** (bu plan yazılırken eklendi): ilerleme ölçüsü, salınım tanımı, merdiven kuralları, başarı ölçütü ve tırmanma belleği, ceza modeli, bilinçli sınırlamalar. Sapma gerekiyorsa dur ve sor.
- `docs/12_NAVIGATION_AND_POSITIONING.md` §10 (bu plana göre güncellendi), §11 (T-NAV-04, AC-NAV-01), `docs/16` MET-NAV-01/02.
- `BotCore/NavGrid.h` — kullanacağın API: `Walk(x, z)`, `Clearance(x, z)`, `CellOf(float)`, `CellCenter(int)`, `Size()`. **Değiştirme.**
- `BotCore/NavPath.h:21-27` (`NavCell`), `BotCore/NavSmooth.h:17` (`NavLineClear`), `BotCore/NavTrack.h:69` (`NavRingCells(grid, cx, cz, ringMinM, ringMaxM, from, out)`: `Walk` hücreleri, `from`'a octile uzaklık artan, eşitlikte x sonra z), `BotCore/NavDanger.h:41-69` (`NavCostLayer::Init/Size/Danger/AddDangerBand`), `:86-90` (`NavCostField`). **Değiştirme.**
- `BotCore/NavPath.h:71` — `NavPathfinder::Find(grid, start, goal, params, out, field)`: yalnızca testte (ceza + A* ve simülasyon).
- `Tests/BotCoreTests/NavFormationTests.cpp` ve `NavRetreatTests.cpp` — kopyalanacak yardımcı kalıbı (yeni dosyada kendi kopyalarını yaz; başka `.cpp`'den içe aktarma yok): `CellIndex`, `Cell`, `RingEvents`, `MakeNav`, `PercentileDouble`; gerçek harita yükleme + `SKIPPED` kalıbı; süre kapısının Debug'da atlanması (`#ifndef _DEBUG`). `Tests/BotCoreTests/MiniTest.h` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`), `tools/run-tests.sh`.

Planı yazarken doğrulanan gerçekler (C++ prototipi, `g++ -std=c++17`, gerçek `BotCore` başlıklarıyla, 2026-10-02, depoya girmeyen geçici; **aynı kurallar, aynı formüller**; sayılar test beklentileridir; MSVC ile son bit farkları belirtilen toleransların çok altındadır). "Düz 40×40" = `RingEvents(40)` (kenar engelli, iç açık), `unit = 4`, yükseklik 0; hücre `(x, z)` merkezi `((x + 0.5) * 4, (z + 0.5) * 4)`: dünya x = 82 → hücre 20, x = 86 → hücre 21, x = 6 → hücre 1. Tüm dedektör/merdiven senaryoları **100 ms** adımla örneklenir (aksi belirtilmedikçe); "ilk ateşleme" = dedektörün `None` dışında döndürdüğü ilk `t`.

**Tablo 1 — ilerlemesizlik** (`NavStuckDetector`, varsayılan param, `moving = true`; konum fonksiyonu `t`'ye göre, z her yerde 82):

| # | Senaryo | Beklenen |
|---|---|---|
| a | durağan x = 82, t = 0..6000 | ilk ateşleme **1500** `NoProgress`; t = 1400'de `None`; t = 1600 ve 1700'de de `NoProgress` (mandallama yok) |
| b | x = 82 + 0,05 · (t/100) (0,5 m/s) | ilk **1500** `NoProgress` |
| c | x = 82 + 0,0625 · (t/100) (1,5 sn'de 0,9375 m) | ilk **1500** |
| d | x = 82 + 0,125 · (t/100) (1,25 m/s), t ≤ 6000 | hiç ateşlemez |
| e | x = 82 (t < 1500), **83,0** (t ≥ 1500) (1,5 sn'de tam 1,0 m) | t = 1500..2900 hiçbiri ateşlemez (eşitlik ateşlemez, `<` kesin); ilk ateşleme **3000** |
| f | x = 82 (t < 1500), 82,999 (t ≥ 1500) | ilk **1500** |
| g | durağan, zamanlar sırayla `0, 700, 1480, 1500` | `None, None, None, NoProgress` (1500'te çapa t = 0 bulunur). Zamanlar `0, 700, 1499, 1500` ise dört sonuç da `None` (1500 örneği, 1499'a 20 ms'den yakın olduğu için düşer) |
| h | durağan, `moving = false` yalnızca t = 1100'de, t = 0..4000 | t = 1100 çağrısından sonra `Count() == 0`; t > 1100 için ilk ateşleme **2700** (pencere 1200'de yeniden başlar) |
| i | durağan 0..1400 (15 örnek) ardından `Observe` 1410, 1400, 1419, 1420 | her ilk üçü `None` ve `Count() == 15`; 1420 kabul edilir (`Count() == 16`) |
| j | durağan, 25 ms aralık, t = 0..6000 | ilk **1500**; durağan, 20 ms aralık, t = 0..6000: ilk **1500**, t = 6000'de `NoProgress`, `Count() == 256` (kapasite) |
| k | durağan; `noProgressMs = 800` / `noProgressMs = 0` / `minProgressM = 0` | ilk **800** / hiç / hiç (kural kapalı) |

**Tablo 2 — salınım** (flat 40×40; "A↔B" = x 82 ↔ 86 = hücre 20 ↔ 21; z = 82):

| # | Senaryo | Beklenen |
|---|---|---|
| a | x = ((t/500) tek) ? 86 : 82 | ilk **1500** `Oscillation` (geçişler 500, 1000, 1500); t = 1400'de `None`; 1600, 1700'de de ateşler |
| b | (a) ve `oscSwings = 4` | ilk **2000** `Oscillation` |
| c | (a) ve `oscWindowMs = 1000`, t ≤ 6000 | hiç ateşlemez |
| d | x = ((t/1500) tek) ? 86 : 82, t ≤ 12000 | ilk **4500** `Oscillation` (geçişler 1500, 3000, 4500) |
| e | dikey: x = 82, z = ((t/500) tek) ? 86 : 82 | ilk **1500** `Oscillation` |
| f | düz yürüyüş x = 10 + 0,6 · (t/100), t ≤ 20000 | hiç ateşlemez |
| g | üç hücre devriye: x = {82, 86, 90}[(t/1000) % 3], t ≤ 12000 | hiç ateşlemez |
| h | titreşim: x = ((t/500) tek) ? 84,1 : 83,9 (hücre 20 ↔ 21, sapma 0,2 m) | ilk **1500**, tür **`NoProgress`** (iki kural da doğru; `NoProgress` önce denetlenir) |
| i | (bilgi) x = ((t/1000) tek) ? 86 : 82 | ilk **2000** `NoProgress` (1,5 sn önceki konuma geri dönmüş) |

**Tablo 3 — merdiven** (`NavStuckMonitor`, varsayılan param, `moving = true`, 100 ms adım; "olay" = `action != None` ya da `recovered`; hücre `(cellX, cellZ)` eylem anındaki bot hücresi):

| # | Senaryo | Beklenen olaylar (t, ne) |
|---|---|---|
| L1 | durağan x = 82, t = 0..8000 | t ≤ 1400: olay yok, `Stage() == 0`. **1500** `Replan` aşama 1 tür `NoProgress` hücre (20,20); **2000** `SideStep` 2; **3000** `StepBack` 3; **4500** `PenalizeReplan` 4; **5500** `Abandon` 5 (sonra `Stage() == 0`); **7100** `Replan` 1 (yeni bölüm: 5600'de tohumlanan pencere 1500 ms sonra); **7600** `SideStep` 2. t = 8000'de: `Episodes() == 2`, `Abandons() == 1`, `Stage() == 2` |
| L2 | x = 82 (t < 1600), **83,0** (t ≥ 1600) | 1500 `Replan` 1; **1600** `recovered` (`recoveredStage` 1, `recoverMs` **100**; tam 1,0 m başarıdır, `>=`); sonra `Stage() == 0`, `Episodes() == 1` |
| L2b | x = 82 (t < 1600), 82,999 (t ≥ 1600), t ≤ 2200 | 1500 `Replan` 1; kurtarma yok; **2000** `SideStep` 2 |
| L3 | x = 82 (t < 2100), **86** (t ≥ 2100), t = 0..8000 | 1500 `Replan` 1 hücre (20,20); 2000 `SideStep` 2; **2100** `recovered` (aşama 2, `recoverMs` **600**); **3600** `StepBack` **3** tür `NoProgress` hücre **(21,20)** (tırmanma belleği: kurtarmadan 1500 ms sonra yeni tespit, ≤ 10 000); **5100** `PenalizeReplan` 4; **6100** `Abandon` 5; **7700** `Replan` 1 (Abandon belleği sıfırlar). `Episodes() == 3`, `Abandons() == 1` |
| L3b | x = 82 (t < 1700), 83,2 (t ≥ 1700), t ≤ 3300; `escalateWindowMs = 1500` ve ayrıca `1499` | ikisinde de 1500 `Replan` 1, **1700** `recovered` (aşama 1, 200); sonraki tespit **3200**: pencere 1500 ⇒ `SideStep` **2** (eşitlik tırmandırır, `<=`); pencere 1499 ⇒ `Replan` **1** |
| L4 | x = 82 (t < 4600), 83,5 (t ≥ 4600), t ≤ 7000 | 1500 `Replan`, 2000 `SideStep`, 3000 `StepBack`, 4500 `PenalizeReplan`; **4600** `recovered` (aşama **4**, `recoverMs` **3100**); **6100** `Abandon` **5** tür `NoProgress` (aşama 4'ten sonra yeni tespit doğrudan bırakır). `Episodes() == 2`, `Abandons() == 1`, `Stage() == 0` |
| L5 | durağan, `moving = false` yalnızca t = 2100'de, t ≤ 4200 | 1500 `Replan`, 2000 `SideStep`; t = 2100'de olay yok ve `Stage() == 0` (iptal); **3700** `Replan` **1** (iptal belleği silmez ama aşama 0'dı: son kurtarma yok ⇒ aşama 1); **4200** `SideStep` 2 |
| L6 | durağan t = 0..1500 (1500'de `Replan`, `Stage() == 1`); sonra `Update(1500, x = 200)`, `Update(1400, x = 200)`, `Update(1600, x = 200)` | ilk ikisi **boş adım** (`action == None`, `recovered == false`) ve `Stage() == 1`, `Episodes() == 1` (zaman kapısı: `tMs <=` son kabul edilen zaman); üçüncüsü `recovered` (aşama 1, `recoverMs` 100) |
| L7 | salınım: x = ((t/500) tek) ? 86 : 82, t = 0..9500 | **1500** `Replan` 1 tür `Oscillation` hücre (21,20); **2000** `recovered` (1, 500); **3500** `SideStep` 2; 4000 `recovered` (2, 500); **5500** `StepBack` 3; 6000 `recovered` (3, 500); **7500** `PenalizeReplan` 4; 8000 `recovered` (4, 500); **9500** `Abandon` 5. `Episodes() == 5`, `Abandons() == 1` (bilinçli sınırlama: salınımda her sıçrama "kurtarıldı" sayılır; ADR-0006 Eki F5-09 madde 6) |
| L8 | durağan t = 0..2000 (`Stage() == 2`, `Episodes() == 1`), `Reset()` | `Stage() == 0`, `Episodes() == 0`, `Abandons() == 0`; ardından `Update(0, …)` kabul edilir (zaman kapısı sıfırlanır), boş adım |
| L9 | ilk çağrı `Update(0, …, moving = false)` | boş adım, `Stage() == 0` |

**Tablo 4 — yan adım** (`NavPickSideStep`, varsayılan param (`sideStepMaxM` 12, `sideStepClearance` 2); sonuç hücre; "yön" = `(hx, hz)`; ızgaralar `MakeNav(40, 4, …)`; "koridor-N" = yalnızca `x = 1..38`, `z` aralığı açık, geri kalan engelli):

| # | Izgara, konum (dünya), yön, değişiklik | Beklenen |
|---|---|---|
| A | düz, (6, 82) = hücre (1,20) duvara bitişik, yön (1,0) | **true**, `(2,19)` (ileri `(2,20)` yan filtreyle elenir; `(1,19)`/`(1,21)` açıklık 1; `(2,19)`: cos = 0,7071 sınırı **dahil**) |
| B | aynı, yön (0,0) | **true**, `(2,20)` (yan filtre yok; octile 4'te önce açıklığı 1 olanlar elenir) |
| C | aynı, yön (0,1) | **true**, `(2,20)` |
| D | aynı, yön (−1,0) | **true**, `(2,19)` (filtre cos değerinin mutlak değerine bakar: ters yön aynı sonuç) |
| E | düz, (82, 82) = hücre (20,20), yön (1,0) | **true**, `(20,19)` |
| F | aynı, yön (0,0) | **true**, `(19,20)` |
| G | aynı, yön (0,−1) | **true**, `(19,20)` |
| H | koridor-1 (`z = 20`), (42, 82) = hücre (10,20), yön (1,0) ve yön (0,0) | ikisi de **false** (tüm hücreler açıklık 1) |
| I | koridor-3 (`z = 19..21`), (42, 78) = hücre (10,19), yön (1,0) | **true**, `(10,20)` (tek açıklık ≥ 2 hücreleri `z = 20`) |
| J | koridor-3, (42, 82) = hücre (10,20), yön (1,0) | **false** (açıklık ≥ 2 hücreler yalnızca yürüme ekseninde: yan filtre eler) |
| K | koridor-3, (42, 82), yön (0,0) | **true**, `(9,20)` |
| L | koridor-5 (`z = 18..22`), (42, 78) = hücre (10,19), yön (1,0) | **true**, `(10,20)` |
| M | koridor-5, (42, 74) = hücre (10,18), yön (1,0) | **true**, `(10,19)` |
| N | düz, (6, 82), yön (1,0); `sideStepMaxM` = 2 / 4 / 8 | **false** / **false** / **true** `(2,19)` |
| O | düz, (6, 82), yön (1,0); `sideStepClearance` = 1 / 3 | **true** `(1,19)` / **true** `(3,18)` |
| P | düz + `x = 21` sütunu engelli (yalnızca `z = 5` açık), (82, 82) = hücre (20,20), yön (1,0) | **true**, `(19,19)` (`NavLineClear` ve açıklık sağlanır; `(20,*)` açıklık 1) |
| Q | aynı ızgara, yön (0,0) | **true**, `(19,20)` |
| R | düz, (2, 2) = engelli hücre `(0,0)`; ve (−5, 82) = ızgara dışı | ikisi **false** |

Başarısız çağrılarda `out` dokunulmaz.

**Tablo 5 — ceza** (`NavStuckPenalties`, varsayılan param: `penaltyMs` 60 000, `penaltyWeight` 1,0; düz 40×40):

| Senaryo | Beklenen |
|---|---|
| `Add(20,20, 1000)` | `Count(1000) == 1`; `Active(20,20, 60999) == true`, `Active(20,20, 61000) == false` (`nowMs < bitiş`); `Count(60999) == 1`, `Count(61000) == 0` |
| yenileme | ardından `Add(20,20, 2000)`: `Active(…, 61999)` true, `Active(…, 62000)` false, `Count == 1`; daha kısa bitişli `Add(20,20, 1500)` bitişi kısaltmaz (`61999` hâlâ etkin) |
| `penaltyMs = 0` | `Add` hiçbir şey eklemez (`Count == 0`) |
| kapasite | 32 farklı hücre `(i, 1)` (`i = 0..31`), `penaltyMs = 1000 + 10·i`: `Count(0) == 32`; 33. `Add(100, 1)` (varsayılan param) en erken bitenin yerine geçer: `Active(0,1) == false`, `Active(1,1) == true`, `Active(100,1) == true`, `Count(0) == 32`; `Clear()` ⇒ `Count == 0` |
| `Apply` | `Add(20,20, 1000)` ve `Add(20,20, 2000)` sonrası boş katmanda `Apply(…, nowMs = 2000, …)`: `Danger(20,20) == 255`, `Danger(19,20) == Danger(21,20) == Danger(20,21) == 0`; `nowMs = 62000` (süresi dolmuş) ⇒ `Danger(20,20) == 0`; `penaltyWeight = 0,5` ⇒ **128**; `penaltyWeight = 0` ⇒ 0; `Init` edilmemiş katman ya da boyutu uyuşmayan katman (20×20 ızgara için `Init` edilmiş) ⇒ hiçbir hücre değişmez; zaten `AddForbidDisc(82, 82, 3)` olan katmanda yasaklı bayrak korunur, `Danger(20,20) == 255` |
| A* ile | düz 40×40, `(5,20)` → `(35,20)`: katmansız (`field` yok) uzunluk **120,000**, 31 hücre, `(20,20)`'den geçer; ceza uygulanmış katmanla (`NavCostField{&layer}`): **Found**, uzunluk **123,3137** (`1e-3`), maliyet **123,3137** (`1e-3`), 31 hücre, `(20,20)`'den **geçmez**, `(20,19)`'dan geçer; süresi dolmuş ceza ile (`nowMs = 62000`) yine 120,000 ve `(20,20)`'den geçer |
| tek hücrelik koridor | `z = 20` koridoru (`x = 1..38`), aynı sorgu: cezasız maliyet **180,000** (açıklık terimi), cezalı **Found**, uzunluk **120,000**, maliyet **196,000** (ceza yolu **bloklamaz**) |

**Simülasyon (`NavStuck_Sim`, §5.2)** — düz 40×40, başlangıç hücre `(5,20)`, hedef `(35,20)`, adım 100 ms, 0,6 m/adım (6 m/s):

| Senaryo | Beklenen |
|---|---|
| kontrol: gizli engel yok | hedefe varır: `reachMs == 21000` (`[20000, 22000]` tolerans); `Episodes() == 0`; `blockedTicks == 0` |
| **sütun:** gizli engel hücresi `(20,20)` | hedefe **varır**; `reachMs ≈ 27300` (`[24000, 31000]`); aşama dizisi tam **{1, 2, 3, 4}**; `episodes == 3`, `abandons == 0`, `recovered == 3` (aşama/süre: `(2, 700)`, `(3, 200)`, `(4, 200)`), en büyük `recoverMs` **700** (≤ 5000, MET-NAV-02); `blockedTicks` 39 (bilgi; > 0) |
| **duvar-3:** gizli engel hücreleri `(20,19)`, `(20,20)`, `(20,21)` | hedefe **varamaz** (`reached == false`): aşama dizisi tam **{1, 2, 3, 4, 5}**, `episodes == 3`, `abandons == 1`, `recovered == 2` (`(2, 700)`, `(3, 200)`), en büyük `recoverMs` 700; `blockedTicks` 49 (bilgi; > 0) |

**Zone 71 yan adım istatistiği** (`build/nav/zone71.navgrid`; her `Walk` hücre, konum = hücre merkezi, varsayılan param): `Walk` hücre **88 508**; yön (1,0) ile bulunan **86 017**; yön (0,0) ile **86 968**; bulunan her sonuç için ihlal (`Walk`, açıklık ≥ 2, `NavLineClear`, başlangıç hücresi değil, merkezden ≤ 12 m, yön (1,0) için yan kuralı) **0**; açıklığı ≥ 2 olan hücre **66 265**, bunların yön (1,0) ile bulunan **66 003** (bulunamayan 262). Süre: çağrı başına p95 ≈ 0,001 ms (Release; kapı `[A]` ≤ 0,2 ms).

## 3. Kapsam

**Yapılacaklar**

- `BotCore/NavStuck.h`: `NavStuckKind`, `NavStuckParams`, `NavStuckDetector`, `NavRecoveryAction`, `NavRecoveryStep`, `NavStuckMonitor`, `NavPickSideStep`, `NavStuckPenalties` (§5.1).
- `Tests/BotCoreTests/NavStuckTests.cpp`: §5.2 yedi test.
- `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` kayıtları (§5.3).

**Kapsam dışı (yapılmayacak)**

- Telemetri olayları (`NAV_STUCK`, `NAV_RECOVERY`, `NAV_STUCK_ABANDON`), takılma ısı haritası, takıma bildirim, `TEST_TELEPORT` (aşama T), MET-NAV-01/02 hesabı: telemetri katmanıdır (`BotCore` yalnızca adımları ve `recoverMs`'i üretir).
- `NavFollower`/`ActionExecutor`/`BotMotion` bağlaması: eylemleri (`Replan`, `SideStep`, `StepBack`, `PenalizeReplan`, `Abandon`) yürütmek, "bir önceki yol noktası"nı bilmek, `moving` bayrağını üretmek, hedefi bırakmak çağıranın işidir. Yol ilerlemesi (kalan yol uzunluğu) ölçüsü ve `NavFollower` ile `wps` bütünleşmesi sonraki plandır.
- Kenar (hücre çifti) cezası: ceza **hücre düzeyindedir** (`NavPath.h` değişmez); kenar yönü bilgisi yoktur.
- `NavPath.h`, `NavDanger.h`, `NavGrid.h`, `NavReach.h`, `NavTrack.h`, `NavSmooth.h`, `NavRetreat.h`, `NavFormation.h` **değişmez**.
- Güvenli hücreye ışınlanma, canavar/kule alanları, LoS (F5-10), karar katmanı, sunucu entegrasyonu.
- `GameServer/`, `AIServer/`, `shared/`, `docs/` değişikliği, DB, sunucu çalıştırma. `GameServer/proj-GameServer.vcxproj` değişmez.
- İş parçacığı güvenliği: sınıflar tek bir bağlamda (bot başına bir örnek) kullanılır, iç kilit yok; global/`static` değişebilir durum yok.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavStuck.h` | yeni | ASCII, CRLF, başlık-yalnızca (satır içi); yalnızca `NavTrack.h` (→ `NavSmooth.h`, `NavPath.h`, `NavGrid.h`, `NavDanger.h`) + standart başlıklar |
| `Tests/BotCoreTests/NavStuckTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="NavStuck.h" />` satırı (BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="NavStuckTests.cpp" />` satırı (BOM ve CRLF korunur) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (`build/nav/zone71.navgrid` üretilen çıktıdır, commit edilmez; yoksa `python3 tools/nav-export.py` ile üret.)

## 5. Uygulama adımları

### 5.1 `BotCore/NavStuck.h`

Ad alanı `BotCore`; `#include "NavTrack.h"` (aynı dizin, tırnaklı; `NavGrid`, `NavCell`, `NavLineClear`, `NavRingCells`, `NavCostLayer` gelir) ve `<cmath> <cstddef> <cstdint> <vector>`. Stil `NavFormation.h`/`NavRetreat.h` gibi: tab girinti, Allman, İngilizce yorum, `inline` tanımlar, değişebilir global/`static` durum yok (işlev içi `static constexpr` tablo serbest), `#pragma once`. Dosya başına kısa bir açıklama yorumu (F5-09, `docs/12` §10, ADR-0006 Eki F5-09; saf mantık, saat yok: tüm zamanlar çağıranın verdiği tek yönlü milisaniye damgalarıdır).

```cpp
namespace BotCore
{
	enum class NavStuckKind { None, NoProgress, Oscillation };

	struct NavStuckParams
	{
		int   noProgressMs = 1500;        // [O] docs/12 s10; <= 0 disables the no-progress rule
		float minProgressM = 1.0f;        // [O] docs/12 s10; <= 0 disables the no-progress rule
		int   oscWindowMs = 4000;         // [O] docs/12 s10; <= 0 disables the oscillation rule
		int   oscSwings = 3;              // [O] docs/12 s10; <= 0 disables the oscillation rule
		float resumeM = 1.0f;             // [A] recovery succeeded: displacement from the stage-entry position
		int   escalateWindowMs = 10000;   // [A] a new detection this soon after a recovery climbs the ladder
		int   stageMs[4] = { 500, 1000, 1500, 1000 };   // [O] docs/12 s10 stages 1..4
		float sideStepMaxM = 12.0f;       // [A]
		int   sideStepClearance = 2;      // [O] docs/12 s10 stage 2
		int   penaltyMs = 60000;          // [O] docs/12 s10 stage 4; <= 0: NavStuckPenalties::Add is a no-op
		float penaltyWeight = 1.0f;       // [A] danger weight 0..1 (1.0 = 255)
	};

	class NavStuckDetector
	{
	public:
		static constexpr int kCapacity = 256;     // samples; covers 4 s at the 20 ms minimum spacing
		static constexpr int kMinSpacingMs = 20;

		void Reset();                             // forget all samples
		int  Count() const;                       // stored samples
		// Feeds one sample and returns the verdict for the window ending at tMs.
		NavStuckKind Observe(const NavGrid & grid, int64_t tMs, float x, float z, bool moving,
			const NavStuckParams & params);
		// private: fixed ring of {t, x, z, cellX, cellZ} (no allocation)
	};

	enum class NavRecoveryAction { None, Replan, SideStep, StepBack, PenalizeReplan, Abandon };

	struct NavRecoveryStep
	{
		NavRecoveryAction action = NavRecoveryAction::None;  // what the caller must do NOW (once per stage entry)
		int stage = 0;                       // 1..5 when action != None, else 0 (5 = Abandon)
		NavStuckKind kind = NavStuckKind::None;   // reason of the episode; None when action == None
		int cellX = 0;                       // bot cell at the moment of the action (the stuck cell); 0 when action == None
		int cellZ = 0;
		bool recovered = false;              // movement resumed during this call (never together with an action)
		int  recoveredStage = 0;             // 1..4 when recovered
		int  recoverMs = 0;                  // detection -> recovery (MET-NAV-02), when recovered
	};

	class NavStuckMonitor
	{
	public:
		void Reset();                        // everything: detector, stage 0, time gate, memory, counters
		NavRecoveryStep Update(const NavGrid & grid, int64_t tMs, float x, float z, bool moving,
			const NavStuckParams & params);
		int Stage() const;                   // 0 = not recovering, 1..4 = ladder stage in progress
		int Episodes() const;                // detections since Reset (MET-NAV-01 raw count)
		int Abandons() const;                // stage-5 results since Reset
		// private: NavStuckDetector, stage, time gate, episode data, escalation memory, counters
	};

	// Stage-2 target: nearest cell (see rules) to step aside to.
	inline bool NavPickSideStep(const NavGrid & grid, float x, float z, float hx, float hz,
		const NavStuckParams & params, std::vector<NavCell> & scratch, NavCell & out);

	class NavStuckPenalties
	{
	public:
		static constexpr int kCapacity = 32;
		void Clear();
		void Add(int x, int z, int64_t nowMs, const NavStuckParams & params);
		int  Count(int64_t nowMs) const;                       // active entries
		bool Active(int x, int z, int64_t nowMs) const;
		void Apply(const NavGrid & grid, int64_t nowMs, const NavStuckParams & params, NavCostLayer & layer) const;
		// private: fixed array of {x, z, expiryMs} + count
	};
}
```

**`NavStuckDetector::Observe` kuralları (bağlayıcı, sırayla):**
0. `moving == false`: `Reset()` ve `None` döndür (diğer her kuraldan önce; zaman kontrolü yok).
1. Örnek **düşer** (`None`, `Count()` değişmez): `Count() > 0` ve `tMs < en yeni örneğin t'si + kMinSpacingMs` (eski, eşit ve 20 ms'den yakın zamanları kapsar).
2. Örnek `{tMs, x, z, grid.CellOf(x), grid.CellOf(z)}` olarak saklanır; kapasite dolunca en eski örnek ezilir.
3. **`NoProgress`** (`noProgressMs > 0` ve `minProgressM > 0` iken): çapa = saklı örnekler içinde `t <= tMs - noProgressMs` olan **en yeni** örnek; yoksa bu kuraldan karar çıkmaz. `dx*dx + dz*dz < minProgressM*minProgressM` (`float`, kesin `<`) ⇒ `NoProgress`. (Çapa tam 1500 ms önce olmak zorunda değildir; en yeni uygun örnektir.)
4. **`Oscillation`** (`oscSwings > 0` ve `oscWindowMs > 0` iken): pencere = `t >= tMs - oscWindowMs` (dahil) olan saklı örnekler, eski → yeni. **Geçiş** = pencerede ardışık iki örneğin hücreleri farklıysa (`x` ya da `z` farklı). Çift **sırasızdır** (A→B ile B→A aynı çifttir). Bir çiftin geçiş sayısı `>= oscSwings` ise `Oscillation`. Yığında en çok `kCapacity` geçişlik sabit dizi; öbek ayırma yok.
5. `NoProgress` (3) ve `Oscillation` (4) birlikte doğruysa `NoProgress` döner; ikisi de yoksa `None`.

**`NavStuckMonitor::Update` kuralları (bağlayıcı, sırayla; "detector" = iç `NavStuckDetector`):**
0. **Zaman kapısı:** `Reset()`'ten beri en az bir çağrı kabul edildiyse ve `tMs <= son kabul edilen tMs` ise boş `NavRecoveryStep` döndür, hiçbir şey değişmez. Aksi hâlde `tMs` kabul edilir.
1. `moving == false`: detector `Reset()`; `Stage()` = 0 (süren kurtarma **iptal**: bot durdurulmuş, takılmamış); "son kurtarma" belleği (kural 3) **silinmez**; boş adım.
2. `Stage() == 0`: `kind = detector.Observe(grid, tMs, x, z, true, params)`. `None` ⇒ boş adım. Aksi hâlde **yeni bölüm**: `Episodes()` +1, `stuckAt = tMs`, bölüm türü = `kind`, ilk aşama = (`sonKurtarmaAşaması > 0` ve `tMs - sonKurtarmaMs <= escalateWindowMs`) ? `sonKurtarmaAşaması + 1` : `1` ⇒ `Enter(ilk)`.
3. `Stage() in 1..4`: (a) **Başarı:** aşama girişindeki konumdan yer değiştirme `dx*dx + dz*dz >= resumeM*resumeM` ise: `recovered = true`, `recoveredStage = Stage()`, `recoverMs = static_cast<int>(tMs - stuckAt)`; `sonKurtarmaAşaması = Stage()`, `sonKurtarmaMs = tMs`; `Stage()` = 0; detector `Reset()` ve bu örnekle `Observe` (tohum; sonuç yok sayılır). Adım döner. (b) Aksi hâlde `tMs - girişMs >= stageMs[Stage() - 1]` ise `Enter(Stage() + 1)`. (c) Aksi hâlde boş adım. (a) (b)'den önce denetlenir.
4. **`Enter(k)`:** adımın `kind` = bölüm türü, `cellX/cellZ = grid.CellOf(x), grid.CellOf(z)`, `stage = k`. `k <= 4`: `action` = `{Replan, SideStep, StepBack, PenalizeReplan}[k - 1]`; `Stage() = k`; giriş zamanı/konumu = şimdiki. `k >= 5`: `action = Abandon`, `stage = 5`, `Abandons()` +1; monitor boşa döner (`Stage()` = 0, detector `Reset()`, `sonKurtarmaAşaması = 0`, `sonKurtarmaMs = 0`; `Episodes()` değişmez).
5. `Reset()` her şeyi sıfırlar (detector, aşama, zaman kapısı, bellek, sayaçlar).
6. Çağıran sözleşmesi (yorum olarak yaz): eylem aşama girişinde **bir kez** döner; çağıran eylem sürerken `moving = true` vermeye devam eder; `Abandon` sonrası hedefi bırakır.

**`NavPickSideStep` kuralları:** `from = (grid.CellOf(x), grid.CellOf(z))`; `from` `Walk` değilse `false`. Adaylar `NavRingCells(grid, x, z, 0.0f, params.sideStepMaxM, from, scratch)` (sıra: `from`'a octile artan, eşitlikte x sonra z). İlk aday `c` için tümü sağlanırsa seçilir: (1) `c != from`; (2) `grid.Clearance(c.x, c.z) >= params.sideStepClearance`; (3) yön verildiyse (`h2 = hx*hx + hz*hz > 0`): `dx = c.x - from.x`, `dz = c.z - from.z` (hücre farkı, `float`), `dot = dx*hx + dz*hz`; **reddedilir** `dot*dot > 0.5f * (dx*dx + dz*dz) * h2` ise (yürüme eksenine ±45° içindeki hücreler elenir: "yan"; sınır dahil izinli); (4) `NavLineClear(grid, from, c)`. Seçilirse `out = c`, `true`; hiç aday sağlamazsa `false` (`out` dokunulmaz). `scratch` çağıranın yeniden kullanılan tamponudur (işlev `clear()` eder).

**`NavStuckPenalties` kuralları:** `Add`: `params.penaltyMs <= 0` ise hiçbir şey yapmaz; bitiş = `nowMs + params.penaltyMs`; aynı `(x, z)` varsa bitiş `max(eski, yeni)` olur (kısaltma yok); yoksa boş yuvaya; yuva yoksa **en erken biten** girişin yerine (eşitte en küçük indeks). `Active(x, z, nowMs)`: aynı hücrede `nowMs < bitiş` olan giriş var. `Count(nowMs)`: `nowMs < bitiş` olan giriş sayısı. `Apply`: `layer.Size() != grid.Size()` ise hiçbir şey yapmaz; aksi hâlde indeks sırasıyla her etkin giriş için `layer.AddDangerBand(grid.CellCenter(x), grid.CellCenter(z), 0.0f, 0.5f, 0.0f, params.penaltyWeight)` (yalnızca o hücre; mevcut tehlike ile `max`; bayraklara dokunmaz). Çağıran her çerçevede `layer = static; layer.AddThreats(...); penalties.Apply(...)` yapar (ceza kendiliğinden süresi dolar).

**Kesin kurallar:** (1) Yukarıdaki kurallar bağlayıcıdır; sayısal beklentiler (Tablo 1–5, simülasyon) bu ifadelerle üretilmiştir. (2) Tüm sınıflar sabit boyutludur (heap yok; `NavStuckDetector` ≈ 6 KB, yığında `Transition` dizisi ≈ 4 KB). (3) MSVC Level 4 uyarıları `static_cast` ile çözülür (`#pragma warning` ekleme); kullanılmayan parametre/değişken bırakma. (4) Özyineleme yok; NaN/sonsuz girdi denenmez. (5) `NavStuckMonitor` her zaman bir `NavStuckParams` ister; çağrılar arasında param değişebilir (durum param'a bağlı değildir).

### 5.2 `Tests/BotCoreTests/NavStuckTests.cpp`

`#include "MiniTest.h"`, `<BotCore/NavGrid.h>`, `<BotCore/NavPath.h>`, `<BotCore/NavDanger.h>`, `<BotCore/NavSmooth.h>`, `<BotCore/NavTrack.h>`, `<BotCore/NavStuck.h>`, `<algorithm> <chrono> <cmath> <cstdint> <cstdio> <string> <vector>`. Anonim ad alanında yardımcılar (kendi kopyaların; **kullanılmayan yardımcı tanımlama**, MSVC C4505 uyarısı çıkar): `CellIndex`, `Cell`, `RingEvents`, `MakeNav` (F5-08 testleriyle aynı), `PercentileDouble`, ve:

- `std::vector<int16_t> CorridorEvents(int n, int zLo, int zHi)`: tüm hücreler engelli, `x = 1..n-2` ve `z = zLo..zHi` açık (Tablo 4 "koridor-N": N = 1 ⇒ `20..20`, 3 ⇒ `19..21`, 5 ⇒ `18..22`).
- `std::vector<int16_t> EastWallEvents(int n)`: `RingEvents(n)` + `x = 21` sütununda `z = 1..n-2` engelli, yalnızca `z = 5` açık (Tablo 4 P/Q).
- `int FirstFire(const NavGrid &, int stepMs, int endMs, PosFn pos, NavStuckKind & kind, const NavStuckParams & = NavStuckParams())`: yeni `NavStuckDetector`; `t = 0, stepMs, … ≤ endMs`; `pos(t, x, z)` konumu verir; `moving = true`; ilk `None` dışı sonuçta `t` ve `kind` döner; hiç yoksa `-1` (`PosFn` bir `std::function<void(int, float &, float &)>` ya da şablon parametresi olabilir).
- Merdiven testleri için `struct Event { int64_t t; NavRecoveryAction action; int stage; NavStuckKind kind; int cellX; int cellZ; bool recovered; int recoveredStage; int recoverMs; }` ve `std::vector<Event> RunLadder(const NavGrid &, int endMs, PosFn pos, MovingFn moving, const NavStuckParams &, NavStuckMonitor & monitor)`: `t = 0, 100, … ≤ endMs`; her çağrıda `monitor.Update(grid, t, x, z, moving(t), params)`; `action != None` ya da `recovered` ise olay kaydedilir.

| Test adı | İçerik |
|---|---|
| `NavStuck_Detect_NoProgress` | **Tablo 1**'in tüm satırları (a–k); ek: her satırda `kind` doğrulanır; satır (g)'de dört zaman dizisi; (i)'de `Count()` değerleri. |
| `NavStuck_Detect_Oscillation` | **Tablo 2**'nin tüm satırları (a–i); ek: (a)'da `kind == Oscillation`, (h)'de `NoProgress`; `oscSwings = 0` ve `oscWindowMs = 0` ile (a) hiç ateşlemez. |
| `NavStuck_Recovery_Ladder` | **Tablo 3**'ün tüm satırları (L1–L9): olay listesi tam eşit (`t`, eylem, aşama, tür, hücre; `recovered` olayları için `recoveredStage`, `recoverMs`), sayaç değerleri, `Stage()`. L6/L8/L9 satırları doğrudan `Update` çağrılarıyla. |
| `NavStuck_SideStep` | **Tablo 4**'ün tüm satırları (A–R); ek: başarısız çağrıda `out` değişmez (`out` önce `(-7, -7)` ile doldurulur); `scratch` tamponu çağrılar arasında yeniden kullanılır (boyutu bir sonraki çağrıda yeniden yazılır). |
| `NavStuck_Penalties` | **Tablo 5**'in tüm satırları (A* ve tek hücrelik koridor dahil); A* için `NavPathfinder` ve `NavSearchParams` varsayılan, `NavCostField field; field.layer = &layer;`. |
| `NavStuck_Sim` | Simülasyon (aşağıda): kontrol, sütun, duvar-3. |
| `NavStuck_SideStep_RealMap` | Gerçek harita (aşağıya bak). |

**`NavStuck_Sim` — test içi `RunStuckSim`** (başlığa taşınmaz; gerçek hareket `ActionExecutor`/`NavFollower` işidir). Imza: `SimOut RunStuckSim(const NavGrid & grid, const std::vector<NavCell> & pillars, NavCell start, NavCell goal, int64_t maxMs)`; `SimOut { bool reached; int64_t reachMs; std::vector<int> stages; int episodes, abandons, recovered, maxRecoverMs, blockedTicks; }`. Varsayılan `NavStuckParams params`, `NavStuckMonitor monitor`, `NavStuckPenalties penalties`, `NavCostLayer layer`, `NavPathfinder pathfinder`, `NavSearchParams`, `NavPathResult path`, `std::vector<NavCell> scratch`; `struct Pt { float x, z; }`, `std::vector<Pt> wps` (kalan yol noktaları), `Pt passed` (son geçilen yol noktası; başlangıçta başlangıç hücresinin merkezi). Konum `(x, z)` başlangıçta başlangıç hücresinin merkezi.

- `plan(fx, fz, now)`: `layer.Init(grid); penalties.Apply(grid, now, params, layer);` `NavCostField field; field.layer = &layer;` `pathfinder.Find(grid, hücre(fx, fz), goal, search, path, &field);` `wps` = `path.cells[1..]`'in hücre merkezleri (`path.status != Found` ise `wps` boş kalır; düz haritada bu olmaz).
- Başlangıçta `plan(x, z, 0)`. Her `t = 0, 100, … ≤ maxMs`:
  1. `wps` boşsa: `reached = true`, `reachMs = t`, döngüden çık.
  2. `step = monitor.Update(grid, t, x, z, true, params)`. `step.recovered` ise `recovered++` ve `maxRecoverMs = max(maxRecoverMs, step.recoverMs)`.
  3. `step.action != None` ise `stages.push_back(step.stage)` ve: **Replan** ⇒ `plan(x, z, t)`. **SideStep** ⇒ `NavPickSideStep(grid, x, z, wps[0].x - x, wps[0].z - z, params, scratch, c)` bulursa `plan(merkez(c), t)` ve `wps.insert(wps.begin(), merkez(c))`; bulamazsa `plan(x, z, t)`. **StepBack** ⇒ `pw = passed; plan(pw.x, pw.z, t); wps.insert(wps.begin(), pw)`. **PenalizeReplan** ⇒ `penalties.Add(step.cellX, step.cellZ, t, params)`; `penalties.Add(hücre(wps[0]), t, params)`; `plan(x, z, t)`. **Abandon** ⇒ `wps.clear()`; döngüden çık (`reached = false`). Eylemden sonra `wps` boşsa bu adımın hareketini atla (`continue`).
  4. Hareket: `d` = `wps[0]`'a uzaklık; `d <= 0.6f` ise sonraki konum `wps[0]`, değilse `0.6f` ilerle. Sonraki konumun hücresi `pillars` içindeyse `blockedTicks++` ve konum **değişmez**; değilse konum güncellenir ve `d <= 0.6f` idiyse `passed = wps[0]` ve `wps` ilk elemanı silinir.
- Sonunda `episodes = monitor.Episodes()`, `abandons = monitor.Abandons()`.
- Üç koşu (Tablo "Simülasyon"): kontrol (`pillars` boş), sütun (`{(20,20)}`), duvar-3 (`{(20,19), (20,20), (20,21)}`); başlangıç `(5,20)`, hedef `(35,20)`, `maxMs = 60000`. `REQUIRE`/`CHECK`: Tablo "Simülasyon"daki tüm beklentiler (aşama dizileri **tam** eşit; `reachMs` aralıkları; sütunda `recovered == 3` ve her `recoverMs <= 5000`). Yazdır: `NAVSTUCK sim pillar: reached=<0/1> reach_ms=<n> episodes=<n> abandons=<n> recovered=<n> max_recover_ms=<n> blocked_ticks=<n> stages=<1,2,3,4>`, `NAVSTUCK sim wall3: …` ve `NAVSTUCK sim control: reached=<0/1> reach_ms=<n> episodes=<n> blocked_ticks=<n>` (prototip: pillar `reached=1 reach_ms=27300 episodes=3 abandons=0 recovered=3 max_recover_ms=700 blocked_ticks=39 stages=1,2,3,4`; wall3 `reached=0 reach_ms=0 episodes=3 abandons=1 recovered=2 max_recover_ms=700 blocked_ticks=49 stages=1,2,3,4,5`; control `reached=1 reach_ms=21000 episodes=0 blocked_ticks=0`).

**Gerçek harita ortak kuralı** (F5-02..F5-08 ile aynı): dosya yolu `build/nav/zone71.navgrid` (çalışma dizini depo kökü); açılamazsa testi **başarısız yapma**: `std::printf("NAVSTUCK real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n")` yaz ve dön. `LoadFile` + `Build()` (varsayılan `NavParams`) sonrası `MainComponentCells() == 88508` `REQUIRE` edilir.

`NavStuck_SideStep_RealMap`: her `Walk` hücre (x-ana tarama) için konum = hücre merkezi; (1) `NavPickSideStep(…, 1, 0, …)` — her çağrı `std::chrono::steady_clock` ile süre ölçülür; bulunan sayısı; bulunan her sonuç için ihlal denetimi (sonuç `Walk`, `Clearance >= 2`, `NavLineClear(from, sonuç)`, `sonuç != from`, merkezden uzaklık `<= 12.0f + 0.01f`, yan kuralı `dot*dot <= 0.5f * (dx*dx + dz*dz) * 1.0f`); (2) `NavPickSideStep(…, 0, 0, …)` bulunan sayısı; (3) `Clearance >= 2` hücre sayısı ve bunlardan (1)'de bulunanlar. Beklenen: `Walk` **88508**; (1) bulunan **86017**; (2) bulunan **86968**; ihlal **0**; açıklığı ≥ 2 hücre **66265**, bunlardan bulunan **66003**. Süre kapısı (`#ifndef _DEBUG`, Debug'da atlanır): çağrı başına `p95 <= 0.2` ms. Yazdır: `NAVSTUCK real: walk=88508 sidestep_h=86017 sidestep_n=86968 violations=0 clr2_cells=66265 clr2_found=66003 ms_p95=<x.xxxx>`. Not (plan sayısı tutmazsa): **kuralı sayıya uydurma**; dur ve Uygulayıcı Raporu'nda sor.

### 5.3 Proje dosyaları

1. `BotCore/BotCore.vcxproj`: `ClInclude` grubuna `<ClInclude Include="NavStuck.h" />` (`NavFormation.h` satırından sonra, `Perception.h` satırından önce; dosyada şu an `:81-82`). BOM/CRLF korunur.
2. `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubuna `<ClCompile Include="NavStuckTests.cpp" />` (`NavFormationTests.cpp` satırından sonra, `PerceptionTests.cpp` satırından önce; şu an `:89-90`). BOM/CRLF korunur.
3. Bu iki projenin `.filters` dosyası yoktur (F5-01'de doğrulandı); ek dosya yok.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; `BotCore` ve `BotCoreTests` için **yeni uyarı yok** (Level 4; `NavStuck.h`, `NavStuckTests.cpp` dosyalarını `touch` ile zorla yeniden derle; çıktıda `NavStuck` geçen `warning` satırı bulunmaz).
- [ ] K2: `./tools/run-tests.sh Release --no-build --list` çıktısı şu yedi test adını içerir: `NavStuck_Detect_NoProgress`, `NavStuck_Detect_Oscillation`, `NavStuck_Recovery_Ladder`, `NavStuck_SideStep`, `NavStuck_Penalties`, `NavStuck_Sim`, `NavStuck_SideStep_RealMap`.
- [ ] K3: `build/nav/zone71.navgrid` varken (yoksa `python3 tools/nav-export.py` ile üret) `./tools/run-tests.sh Release --no-build` çıkış kodu 0; çıktıda `136 tests, 0 failed` (önceki 129 + yedi yeni), tüm eski testler ve yedi yeni test `[ OK ]`; `SKIPPED` geçmiyor; `NAVSTUCK sim pillar: …`, `NAVSTUCK sim wall3: …`, `NAVSTUCK sim control: …` ve `NAVSTUCK real: …` satırları var.
- [ ] K4: Gerçek harita yokken (`build/nav/zone71.navgrid` geçici olarak başka ada taşınarak) `./tools/run-tests.sh Release --no-build NavStuck_` çıkış kodu 0, `NavStuck_SideStep_RealMap` `SKIPPED` yazar ve diğer altı test geçer; ardından dosya yerine geri konur.
- [ ] K5: `./tools/run-tests.sh Debug` (derleme dahil) çıkış kodu 0 (Debug'da `assert`/sınır hataları yok).
- [ ] K6: Davranış sayıları (Release ve Debug): §2 Tablo 1–5'teki tüm değerler (`NavStuck_Detect_NoProgress`, `_Detect_Oscillation`, `_Recovery_Ladder`, `_SideStep`, `_Penalties` `[ OK ]`), `NavStuck_Sim` (`pillar`: aşamalar `1,2,3,4`, `episodes=3 abandons=0 recovered=3`, `reached=1`, `max_recover_ms=700`; `wall3`: aşamalar `1,2,3,4,5`, `episodes=3 abandons=1 recovered=2`, `reached=0`; `control`: `reached=1 reach_ms=21000 episodes=0 blocked_ticks=0`), `NavStuck_SideStep_RealMap` (`walk=88508 sidestep_h=86017 sidestep_n=86968 violations=0 clr2_cells=66265 clr2_found=66003`).
- [ ] K7: Saflık/kapsam: `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavStuck.h` çıktısı boş; `git diff --stat gece/2026-10-02-nav...bot/F5-09` yalnızca §4'teki dört dosyayı ve kendi plan dosyasını gösterir (`GameServer/`, `AIServer/`, `shared/`, `docs/`, `NavGrid.h`, `NavPath.h`, `NavDanger.h`, `NavReach.h`, `NavTrack.h`, `NavSmooth.h`, `NavRetreat.h`, `NavFormation.h` yok).
- [ ] K8: F5-01..F5-08 davranışı bozulmadı: `./tools/run-tests.sh Release --no-build Nav_` (10 test), `NavPath_` (9), `NavSmooth_` (8), `NavTrack_` (10), `NavReach_` (8), `NavDanger_` (8), `NavRetreat_` (8), `NavForm_` (7) çıkış kodu 0, hepsi `[ OK ]` (**değişmeden**, testler düzenlenmez); `NAVPATH T-NAV-03 set=near64 …` satırında `found=997`, `expanded_p50=306 expanded_p95=2431`; `NAVDANGER real: elm_forbid=1594 elm_forbid_walk=1264 elm_safe=1591 elm_safe_walk=1232 …`, `NAVREACH real: components=143 largest=88279 pockets=229 …`, `NAVRETREAT real: … cand=2799 …` ve `NAVFORM real: walk=88508 usable8=72459 …` satırlarında aynı sayılar.

## 7. Doğrulama komutları

```bash
git switch -c bot/F5-09 gece/2026-10-02-nav
python3 tools/nav-export.py            # build/nav/zone71.navgrid yoksa
./tools/build.sh Release
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Release --no-build NavStuck_
./tools/run-tests.sh Release --no-build NavForm_
./tools/run-tests.sh Debug
grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavStuck.h
git diff --stat gece/2026-10-02-nav...bot/F5-09
```

## 8. Kısıtlar ve uyarılar

- **Paralel hat:** sunucuya hiç dokunma (`tools/run-servers.sh` çağırma; ana hat başka çalışma ağacından sunucu çalıştırıyor olabilir). DB'ye bağlanma. Dal tabanı `gece/2026-10-02-nav`.
- Kodlama/satır sonu: `AGENTS.md` §3. Yeni C++ dosyaları ASCII + CRLF + tab + Allman. Yorumlar İngilizce; plan metnindeki Türkçe açıklama koda girmez.
- Başlık-yalnızca: tüm tanımlar `inline`; değişebilir `static`/global durum yok; `#pragma` yalnızca `once`.
- **Belirlenim:** aynı girdiler her zaman aynı çıktıyı verir (rastgelelik, saat, `rand()` yok). Zaman yalnızca parametredir; testte gerçek saat yalnızca süre kapısında (`steady_clock`).
- Sayısal beklentiler (Tablo 1–5, simülasyon: 1,5 sn / 1 m / 4 sn / 3 geçiş / 0,5–1–1,5–1 sn / 12 m / 60 sn / 10 sn / ağırlık 1,0) `[O]`/`[A]` değerleridir: parametre varsayılanlarını ve kuralları değiştirme. **Eşitlik** kuralları (örn. `<` / `>=` / `<=` yönleri) test beklentisinin parçasıdır; "iyileştirme" olarak çevirme.
- **Bilinçli sınırlamalar (düzeltme isteme):** (1) ilerleme ölçüsü net yer değiştirmedir, yol ilerlemesi değil: 1,5 sn önceki noktaya dönen bot (gidiş-geliş) `NoProgress` sayılır (Tablo 2 i). (2) Salınımda her sıçrama (≥ 1 m) "kurtarıldı" sayılır; kalıcı kurtarma `recoverMs` tek başına söylemez, tırmanma belleği merdiveni yine bırakmaya götürür (Tablo 3 L7). (3) Ceza hücre düzeyindedir, tek hücrelik koridorda yolu bloklamaz (maliyeti artırır). Bunların gerekçesi ADR-0006 Eki F5-09'dadır.
- Float: MSVC ve g++ arasında son bit farkları olabilir; testte eşitlik yerine belirtilen toleranslar kullanılır. Eşik sınırı (`== 1,0 m`) olan satırlar (Tablo 1 e, Tablo 3 L2) tam temsil edilebilen değerlerle (83,0 − 82,0 = 1,0) yazılmıştır; beklenti tutmazsa **kuralı değiştirme**, dur ve raporla.
- Simülasyon sonuçları (`reachMs`, `blockedTicks`) float adımlarına bağlıdır: aralık/`> 0` ile sınanır; aşama dizileri ve sayaçlar **tam** eşit olmalıdır (tutmazsa kuralı sayıya uydurma, dur ve raporla).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-09` (taban `gece/2026-10-02-nav` @ `2ae9072`) — `367a92c [F5-09] Takılma tespiti ve aşamalı kurtarma: NavStuck.h + birim testleri`; ardından rapor/`Durum` commit'i
- Değişen dosyalar ve neden:
  - `BotCore/NavStuck.h` (yeni, 476 satır): `NavStuckKind`, `NavStuckParams`, `NavStuckDetector`, `NavRecoveryAction`, `NavRecoveryStep`, `NavStuckMonitor`, `NavPickSideStep`, `NavStuckPenalties`; başlık-yalnızca, saf mantık, saat yok. §5.1 kuralları birebir.
  - `Tests/BotCoreTests/NavStuckTests.cpp` (yeni, 1331 satır): yedi test (`NavStuck_Detect_NoProgress`, `NavStuck_Detect_Oscillation`, `NavStuck_Recovery_Ladder`, `NavStuck_SideStep`, `NavStuck_Penalties`, `NavStuck_Sim`, `NavStuck_SideStep_RealMap`); Tablo 1–5 + simülasyon + gerçek harita.
  - `BotCore/BotCore.vcxproj`: `NavStuck.h` `ClInclude` satırı (`NavFormation.h` sonrası, `Perception.h` öncesi).
  - `Tests/BotCoreTests/BotCoreTests.vcxproj`: `NavStuckTests.cpp` `ClCompile` satırı (`NavFormationTests.cpp` sonrası, `PerceptionTests.cpp` öncesi).
  - `plans/F5-09-nav-takilma-tespiti-kurtarma.md`: yalnızca `Durum` satırı ve bu rapor.
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
  BotCore.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\BotCore.lib
  Lua.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\Lua.lib
  shared.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\shared.lib
  proj-LogInServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\LogInServer.exe
  proj-GameServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\AIServer.exe
  NavFormationTests.cpp
  NavStuckTests.cpp
  Kod Üretiliyor...
  BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ `NavStuck.h`/`NavStuckTests.cpp` `touch` ile yeniden derlendi; tüm çözüm çıktısında `warning`/`error` satırı yok.
  - K2 ✔ `--list` çıktısında yedi `NavStuck_*` adı var (K2 listesi birebir).
  - K3 ✔ `136 tests, 0 failed`, 129 eski + 7 yeni tümü `[ OK ]`, `SKIPPED` sayısı 0; `NAVSTUCK sim control/pillar/wall3` ve `NAVSTUCK real` satırları var.
  - K4 ✔ Harita geçici taşındığında `NavStuck_` rc=0, `NavStuck_SideStep_RealMap` `SKIPPED` yazdı, diğer altı test geçti; dosya geri kondu.
  - K5 ✔ `tools/run-tests.sh Debug` rc=0, `136 tests, 0 failed`.
  - K6 ✔ Tablo 1–5 ve simülasyon değerleri Release ve Debug'da geçti: pillar `reached=1 reach_ms=27300 episodes=3 abandons=0 recovered=3 max_recover_ms=700 blocked_ticks=39 stages=1,2,3,4`; wall3 `reached=0 episodes=3 abandons=1 recovered=2 max_recover_ms=700 blocked_ticks=49 stages=1,2,3,4,5`; control `reached=1 reach_ms=21000 episodes=0 blocked_ticks=0`; gerçek harita `walk=88508 sidestep_h=86017 sidestep_n=86968 violations=0 clr2_cells=66265 clr2_found=66003 ms_p95=0.0009`.
  - K7 ✔ Saflık grep'i boş; `git diff --stat gece/2026-10-02-nav...bot/F5-09` yalnızca §4'teki dört dosya + kendi plan dosyasını gösteriyor.
  - K8 ✔ `Nav_` 10/10, `NavPath_` 9/9, `NavSmooth_` 8/8, `NavTrack_` 10/10, `NavReach_` 8/8, `NavDanger_` 8/8, `NavRetreat_` 8/8, `NavForm_` 7/7; sayılar değişmedi (`near64 found=997 expanded_p50=306 expanded_p95=2431`, `elm_forbid=1594 … elm_safe_walk=1232`, `components=143 largest=88279 pockets=229`, `cand=2799`, `NAVFORM real walk=88508 usable8=72459 …`).
- Plandan sapmalar ve gerekçeleri: yok. Plan §5.1/§5.2 birebir uygulandı; sayısal beklentiler prototiple aynı çıktı.
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02-nav...bot/F5-09` @ `3c70edf` (kod commit'i `367a92c`; paralel hat `nav`, otonom mod: sunuculara dokunulmadı, birleştirme/push yapılmadı)
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `NavStuck.h` ve `NavStuckTests.cpp` `touch` ile yeniden derlendi; `./tools/build.sh Release` rc=0. Çıktıda 3 `warning` var, üçü de `GameServer/GameServerDlg.cpp` (:816 C4834, :1143 ve :1802 C4267); bu dal o dosyaya dokunmuyor (önceden var). `NavStuck` geçen satır yalnızca `NavStuckTests.cpp` derleme adı, `warning` yok. |
| K2 | ✔ | `run-tests.sh Release --no-build --list`: yedi `NavStuck_*` adı (`Detect_NoProgress`, `Detect_Oscillation`, `Recovery_Ladder`, `SideStep`, `Penalties`, `Sim`, `SideStep_RealMap`). |
| K3 | ✔ | `build/nav/zone71.navgrid` var; Release `--no-build` rc=0, `136 tests, 0 failed`, `[ OK ]` = 136, `SKIPPED` = 0. `NAVSTUCK sim control/pillar/wall3` ve `NAVSTUCK real` satırları var. |
| K4 | ✔ | Harita `.k4bak` adına taşındı: `NavStuck_` rc=0, `7 tests, 0 failed`, `NAVSTUCK real map: SKIPPED …` yazdı, diğer altı test `[ OK ]`; dosya geri kondu (1 579 030 bayt, aynı). |
| K5 | ✔ | `./tools/run-tests.sh Debug` (derleme dahil) rc=0, `136 tests, 0 failed`; Debug çıktısında `NavStuck` uyarısı/hatası yok. |
| K6 | ✔ | Release ve Debug: `sim control: reached=1 reach_ms=21000 episodes=0 blocked_ticks=0`; `sim pillar: reached=1 reach_ms=27300 episodes=3 abandons=0 recovered=3 max_recover_ms=700 blocked_ticks=39 stages=1,2,3,4`; `sim wall3: reached=0 reach_ms=0 episodes=3 abandons=1 recovered=2 max_recover_ms=700 blocked_ticks=49 stages=1,2,3,4,5`; `real: walk=88508 sidestep_h=86017 sidestep_n=86968 violations=0 clr2_cells=66265 clr2_found=66003` (`ms_p95=0.0017` Release, Debug'da kapı atlanır). Tablo 1–5 testleri `[ OK ]`; test gövdeleri okundu: Tablo 1 a–k, Tablo 3 L1–L9, sim beklentileri plandaki değerlerle birebir (`NavStuckTests.cpp:413-593`, `:701-925`, `:1188-1243`). |
| K7 | ✔ | `grep -n "windows.h\|stdafx.h\|GameServer\|shared/" BotCore/NavStuck.h` boş. `git diff --stat gece/2026-10-02-nav...bot/F5-09`: `BotCore/NavStuck.h`, `BotCore/BotCore.vcxproj` (+1), `Tests/BotCoreTests/NavStuckTests.cpp`, `Tests/BotCoreTests/BotCoreTests.vcxproj` (+1), kendi plan dosyası; `GameServer/`, `AIServer/`, `shared/`, `docs/` ve diğer Nav başlıkları yok. Plan dosyasında yalnızca `Durum` ve Uygulayıcı Raporu değişmiş. |
| K8 | ✔ | `Nav_` 10, `NavPath_` 9, `NavSmooth_` 8, `NavTrack_` 10, `NavReach_` 8, `NavDanger_` 8, `NavRetreat_` 8, `NavForm_` 7: hepsi rc=0 ve tümü `[ OK ]`. Sayılar değişmedi: `near64 found=997 expanded_p50=306 expanded_p95=2431`; `elm_forbid=1594 elm_forbid_walk=1264 elm_safe=1591 elm_safe_walk=1232`; `components=143 largest=88279 pockets=229`; `cand=2799`; `NAVFORM real walk=88508 usable8=72459`. Eski testlere dokunulmamış (fark dosya listesinde yok). |

- Ek denetimler: (a) Kodlama/biçim: `NavStuck.h` ve `NavStuckTests.cpp` ASCII, tüm satırlar CRLF (476/476 ve 1331/1331), girinti tab (boşlukla başlayan satır 0), BOM yok; iki `.vcxproj` değişikliği yalnızca tek `ClInclude`/`ClCompile` satırı, BOM ve CRLF korunmuş; `build/` commit edilmemiş. (b) Saflık: `#pragma once` dışında pragma yok, değişebilir global/`static` durum yok (`kAction` işlev içi `static constexpr` tablo, plan izin veriyor), heap yok (sabit diziler), saat yok (`steady_clock` yalnızca testte süre ölçümünde). (c) `NavStuck.h` kuralları plan §5.1 ile satır satır karşılaştırıldı: `Observe` kuralları 0–5 (`:209-285`; `moving=false` → `Reset`, 20 ms aralık düşürme, çapa = eşik ve öncesi en yeni örnek, kesin `<`, salınım sırasız çift ve `>= oscSwings`, `NoProgress` önceliği); `Update` kuralları 0–4 (`:304-363` zaman kapısı `<=`, iptal belleği silmez, başarı `>=` ve ardından tohumlama, `stageMs` `>=`, escalate `<=`; `Enter` `:365-397` Abandon'da bellek sıfırlanır, `Episodes` değişmez); `NavPickSideStep` (`:124-160`); `NavStuckPenalties` (`:399-475`: kısaltma yok, en erken biten yerine geçer, `Apply` boyut denetimi). Sapma yok. (d) Uygulayıcı Raporu'ndaki iddialar (derleme, 136/0, K4, sim ve gerçek harita sayıları, K8 sayıları) bağımsız koşuyla aynen doğrulandı. (e) Kapsam: değişen dosyalar yalnızca `BotCore/`, `Tests/BotCoreTests/`, `plans/` (paralel hat kuralı).
- Bulgular (önem sırasıyla):
  1. Engelleyici bulgu yok.
  2. Not (engel değil): `NavStuckDetector::Observe` yığında 256 × 8 bayt (≈ 2 KB) `Transition` dizisi tutuyor; plan "≈ 4 KB" demişti. Boyut planın sınırının altında, davranış etkisi yok.
  3. Not (engel değil): Debug'da gerçek harita çağrı süresi kapısı atlanıyor (plan böyle istiyor); Debug `ms_p95=0.0458`, Release `ms_p95=0.0017` ms (kapı ≤ 0,2 ms).
  4. Not: planın "bilinçli sınırlamaları" (net yer değiştirme ölçüsü, salınımda her sıçramanın "kurtarıldı" sayılması, hücre düzeyinde ceza) uygulandı ve testlerle sabitlendi (Tablo 2 i, L7, Tablo 5 tek hücrelik koridor); düzeltme istenmez.
- Düzeltme talimatı: yok (karar DOĞRULANDI).
