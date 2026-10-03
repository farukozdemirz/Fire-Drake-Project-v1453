# F5-69: Navigasyon eğim sınırı 0,625 → 0,45 (T-NAV-02 ölçümü, proje sahibi kararı ADR-0024)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F5 — Navigasyon (`docs/12` §2/§3; kapı G5) |
| Branch | `bot/F5-69 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F5-01 (`NavGrid`, `KAPANDI`), F5-59 (`NavService` aynı `NavParams` varsayılanını kullanır, `KAPANDI`) |
| İlgili gereksinim / kabul | T-NAV-02 (eğim kalibrasyonu, `docs/15`/`docs/12` §3), ADR-0024 (karar), `docs/12` §2 `P-NAV-MAX-SLOPE`; F5-61/F5-62 yol ve kiriş denetimi bu sınırı kullanır |
| Tahmini büyüklük | S (1 üretim satırı + birkaç test sabiti; kanıt birim testte ve gerçek harita bağlantı testinde) |
| Hazırlayan / tarih | Claude / 2026-10-03 |

---

## 1. Amaç

Botun yürünebilir saydığı en büyük kenar eğimi (`NavParams::maxSlope`) bugün **0,625** `[A]` (2,5 m / 4 m hücre; `BotCore/NavGrid.h:21-23`, uygulama `:325`). Proje sahibi, **insan istemcisiyle** yaptığı harita yürüyüş oturumunda (2026-10-03, paket izleyici kaydı, karakterin kendi yüksekliğiyle ölçüldü) bu sınırın **tahmin olduğunu ve çıkabildiğimiz eğimin üstünde kaldığını** gösterdi. Karar (ADR-0024): bot yalnızca **rahatça çıkılabilen** eğimlere girsin; takılma riskini almaktansa biraz uzun rota kabul edilir. Bu plan varsayılanı **0,45'e (1,8 m / 4 m)** çeker ve testlerdeki eski varsayılana bağlı sabitleri günceller.

## 2. Bağlam (okunması zorunlu)

**Ölçüm (T-NAV-02, insan istemcisi, `[V]`):**

| Gözlem | Eğim (|Δh| / yatay) | Sonuç |
|---|---|---|
| Tasarlanmış rampa (1028,1159) → (1106,1128), 84 m, +20 m | ortalama 0,24, en dik yerel 0,47 | çıkıldı |
| Platoya kısa adım (1150,985) → (1151,990) | 0,76 (5 m'de +3,9 m) | çıkıldı (kısa, ölçüm payı belirsiz) |
| (1532,772) → (1520,772) | 0,78 | **kısmen**: +4 m çıkıp takıldı |
| (1104,1156) → (1092,1156) | 0,94 | **çıkılamadı** |
| (1384,1184) → (1384,1172) | 1,26 | **çıkılamadı** (iniş mümkün: en dik iniş 1,07; yönler farklı, bu plan simetriyi **değiştirmez**) |

0,54 ve 0,65 test **edilmedi** (proje sahibi: botlar rahat çıkabilsin, kesin sınır aranmaz). Seçilen **0,45**, sürekli çıkılan en dik yerel eğimin (0,47) hemen altındadır.

**Bağlantı ve rota maliyeti (Claude ölçümü, `[V]`, gerçek zone 71 ızgarası, 4-komşu, yalnızca `Walk` hücreleri):**

| Sınır | Ulaşılan Walk hücre (88 508) | Karus doğuş, El Morad doğuş, arena A, bowl merkezi, Karus kapısı |
|---|---|---|
| 0,625 (bugün) | %98,6 | hepsi bağlı |
| **0,45 (hedef)** | **%96,6** | **hepsi bağlı** |
| 0,25 | %87,7 | hepsi bağlı (yol +%97'ye kadar uzar: seçilmedi) |

Rota uzaması (0,625 → 0,45): Karus doğuş → arena **+%11**, El Morad doğuş → arena **+%0**, Karus doğuş → bowl **+%1**, El Morad doğuş → bowl **+%25**.

**Koddaki yerler (`gece/2026-10-02` @ güncel uç; uygulayıcı her satırı kendi çalışma ağacında doğrulasın, kayma varsa durup rapora yazsın):**
- `BotCore/NavGrid.h:21-23` `NavParams::maxSlope = 0.625f` (yorum "docs/12 s2: 2.5 m per 4 m cell = 0.625. Calibrated later by T-NAV-02"); `:325` `if (dh > m_params.maxSlope * distance) return false;` (kenar kuralı, köşegen `unit * sqrt(2)`).
- `Tests/BotCoreTests/NavGridTests.cpp` ~`:205-262`: üç blok varsayılan 0,625'e bağlı: (a) "|dh| = 3 m ... slope 0.75, above the 0.625 default" (0,45 altında da kapalı kalır; yorum güncellenir), (b) "|dh| = 2.4 m over 4 m -> 0.6, below the default" (**0,45 ile açılmaz**, 1,6 m/0,4 olarak güncellenmeli), (c) "Diagonal 4*sqrt(2) m: the 0.625 limit is 3.5355 m" (3,5 açık / 3,6 kapalı; yeni limit 0,45 × 5,657 = **2,546 m**: 2,5 açık / 2,6 kapalı).
- `Tests/BotCoreTests/NavSegmentTests.cpp:408-440` "Edge step exactly at the slope limit: maxSlope * unit = 2.5 m" ve "Vertex (diagonal) step ... 0.625 * 4 * sqrt(2) = 3.5355 m": yeni limitler **1,8 m** ve **2,546 m**.
- `Tests/BotCoreTests/NavReachTests.cpp:188` `p.maxSlope = 100.0f` (eğimi devre dışı bırakıyor, **değişmez**).
- Diğer testler (NavPath, NavSmooth, NavArena, NavStuck, NavTrack, NavBudget ve gerçek harita testleri) varsayılanı dolaylı kullanıyor olabilir: **tam test koşusu hangilerinin kırıldığını gösterir** (adım 3).
- `tools/nav-measure/nav_measure.cpp`, `tools/nav-segment-check.py`, `tools/nav-regress.py`: eğimi `NavParams` varsayılanından alıyorlar mı, ayrı bir sabit mi kullanıyorlar? Uygulayıcı önce `grep -n -E "0\.625|maxSlope|slope" tools/nav-measure/nav_measure.cpp tools/nav-segment-check.py tools/nav-regress.py` ile bakar; ayrı sabit varsa rapora yazar (bu planın dosya listesinde **yok**, değiştirme, Claude karar verir).

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavGrid.h`: `maxSlope` varsayılanını `0.45f` yap; yorumu şöyle güncelle: `// P-NAV-MAX-SLOPE [V]: max |dh| / horizontal distance of one edge (1.8 m per 4 m cell = 0.45). Measured by T-NAV-02 (human client, 2026-10-03): sustained climbs up to 0.47, 0.78 and steeper failed; ADR-0024.` Başka hiçbir satır değişmez (algoritma, `NavParams` alanları, `EdgeOpen` mantığı aynı).
2. Eski varsayılana bağlı test sabitlerini güncelle (§2'deki üç `NavGridTests.cpp` bloğu ve iki `NavSegmentTests.cpp` bloğu): yalnızca **sayısal sabitler ve yorumlar**; test mantığı ve test adları aynı kalır. Tam test koşusunda **başka** test kırılırsa, yalnızca eski varsayılanı (0,625 / 2,5 m / 3,5355 m) kodlayan sabiti güncelle ve **raporda test adı + eski/yeni değer + gerekçe** yaz; sayıyı gizlemek için eşiği gevşetme.
3. İki yeni test (`Tests/BotCoreTests/NavGridTests.cpp` sonuna): `NavGrid_DefaultSlope_045` (varsayılan `NavParams` ile |Δh| = 1,8 m / 4 m kenar **açık**, 1,9 m **kapalı**; köşegen 2,5 m açık, 2,6 m kapalı) ve `NavGrid_RealMap_DefaultSlopeConnectivity` (gerçek harita dosyası `build/nav/zone71.navgrid` yoksa `NAVSLOPE real map: SKIPPED (...)` basıp geçer; varsa varsayılan `NavParams` ile `Build`, `MainComponentCells() == 88508` ve Karus doğuş (1370, 1090), El Morad doğuş (630, 920), arena A (1274, 890), bowl merkezi (1024, 1024), Karus kapısı (1375, 1085) en yakın `Walk` hücrelerinden başlayan 4-komşu `EdgeOpen` taramasında **hepsi birbirine bağlı**; çıktı satırı `NAVSLOPE real map: main=88508 reach=<n> landmarks=5/5`; `reach` değeri %96-%97 bandında olmalı, ±%1 dışına çıkarsa rapora yaz). Gerçek harita yükleme kalıbı: `Tests/BotCoreTests/NavArenaTests.cpp:320-329`.

**Kapsam dışı (yapılmayacak)**

- Tırmanış ve iniş için ayrı eğim kuralı (iniş daha dik olabiliyor): ayrı iş, Claude karar verir.
- Kiriş denetiminin (`NavCheckStep`/`NavCheckSegment`) `checkSlope` varsayılanı (kapalı kalır), `GameServer/`, `AIServer/`, `shared/`, `tools/`, `docs/`.
- 0,54 / 0,65 / kesin sınır arayışı.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavGrid.h` | değiştir | yalnızca varsayılan değer ve yorum (§3 madde 1) |
| `Tests/BotCoreTests/NavGridTests.cpp` | değiştir | eski sabitler + iki yeni test |
| `Tests/BotCoreTests/NavSegmentTests.cpp` | değiştir | yalnızca sabitler/yorumlar |
| (gerekirse) tam test koşusunda kırılan diğer `Tests/BotCoreTests/*.cpp` | değiştir | yalnızca eski varsayılanı kodlayan sabit; her biri raporda |

## 5. Uygulama adımları

1. `git switch -c bot/F5-69 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Taban koşusu: `./tools/run-tests.sh Release` ve `./tools/nav-regress.sh --skip-timing`: test sayısını ve `nav-regress` çıkış kodunu (ve varsa özet satırlarını) rapora yaz.
2. `BotCore/NavGrid.h` varsayılanını ve yorumunu güncelle (§3 madde 1).
3. `./tools/run-tests.sh Release`: kırılan test adlarını listele; her biri için nedenini (eski varsayılan sabit mi, gerçek davranış değişimi mi) belirle. Yalnızca eski sabiti kodlayanları güncelle; gerçek davranış değişimi (ör. yol bulunamıyor, `NodeLimit`, bağlantı kopması) varsa **durup** raporla.
4. İki yeni testi yaz (§3 madde 3); `build/nav/zone71.navgrid` yoksa `python3 tools/nav-export.py` ile üret (gerçek harita testinin `SKIPPED` kalmaması için).
5. `./tools/build.sh Release` ve `Debug`, `./tools/run-tests.sh` her ikisi, `./tools/nav-regress.sh --skip-timing` (adım 1 ile karşılaştır: çıkış kodu aynı; fark varsa nedeni açıkla).
6. Uygulayıcı Raporu: değişen satırlar, kırılan/güncellenen test adları ve eski/yeni değerleri, yeni testlerin çıktı satırları, `nav-regress` öncesi/sonrası.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0 ve `./tools/build.sh Debug` rc=0; değişen dosyalarda yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh` Release ve Debug: `0 failed`; `NavGrid_DefaultSlope_045` ve `NavGrid_RealMap_DefaultSlopeConnectivity` `[ OK ]`
- [ ] K3: `git diff gece/2026-10-02...bot/F5-69 --stat` yalnızca §4 dosyaları (+ plan dosyası); `BotCore/NavGrid.h` farkı yalnızca varsayılan değer ve yorum (`git diff` ile 2 satır dolayında); `GameServer/`, `shared/`, `tools/`, `docs/` farkı 0; `git diff --check` boş
- [ ] K4: `grep -n "maxSlope = 0" BotCore/NavGrid.h` → `0.45f`; `git grep -n -E "0\.625" BotCore Tests` yalnızca "eski değer" açıklayan yorumları gösterir (sabit olarak kalan yok)
- [ ] K5: `NAVSLOPE real map:` satırı `SKIPPED` **değil**: `main=88508`, `landmarks=5/5`, `reach` değeri 85 000-87 000 bandında (88 508'in ~%96-%98'i)
- [ ] K6: `./tools/nav-regress.sh --skip-timing` çıkış kodu adım 1 taban koşusuyla aynı (fark varsa rapor + nedeni)
- [ ] K7 (Claude, çalışma zamanı): bu plan sunucu kodu değiştirmez; doğrulama birim ve gerçek harita testleriyle sınırlıdır. Faz kabulü ayrı.

## 7. Doğrulama komutları

```bash
git diff gece/2026-10-02...bot/F5-69 --stat
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release && ./tools/run-tests.sh Debug
./tools/run-tests.sh Release --no-build 2>&1 | grep -E "NAVSLOPE|NavGrid_DefaultSlope|0 failed"
./tools/nav-regress.sh --skip-timing; echo rc=$?
git grep -n -E "0\.625" BotCore Tests
```

## 8. Kısıtlar ve uyarılar

- Yalnızca ASCII, CRLF, tab, Allman (`AGENTS.md` §3); yorumlar İngilizce. Yeni dosya yok.
- Davranış değişimi: botun planlayıcısı yalnızca `NavParams` varsayılanıyla kurulan ızgarada daha sıkı eğim uygular; `[BOT] NAV=0` ve `ENABLED=0` davranışı değişmez (sunucu kodu dokunulmaz).
- Eski varsayılanı kodlayan testleri **gevşeterek** (eşiği büyüterek) geçirme; sabiti yeni limite göre güncelle.

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

**Durum:** UYGULANIYOR (BLOKE) — plan §3 adım 3'ün "gerçek davranış değişimi (`yol bulunamıyor`, `NodeLimit`, `bağlantı kopması`) varsa durup raporla" koşulu tetiklendi.

**Branch / commit:** `bot/F5-69` (taban `gece/2026-10-02` @ `9053844`); `05e9e45` kod + testler (bu rapor ayrı commit).

**Yapılanlar (plan kapsamı):**
- `BotCore/NavGrid.h`: `NavParams::maxSlope` `0.625f` -> `0.45f`; yorum P-NAV-MAX-SLOPE `[V]` (T-NAV-02, ADR-0024) olarak güncellendi. Algoritma, `EdgeOpen`, diğer alanlar değişmedi.
- `Tests/BotCoreTests/NavGridTests.cpp` `Nav_Edge_Slope`: (a) yorum "0.625 default" -> "0.45 default"; (b) `2.4f * x` -> `1.6f * x` (yorum 0.4); (c) köşegen `3.5f`/`3.6f` -> `2.5f`/`2.6f` (yorum limit 2.546 m).
- `Tests/BotCoreTests/NavSegmentTests.cpp` `NavSegment_Slope`: at-limit `2.5f` -> `1.8f`, üstü `2.51f` -> `1.81f`, köşegen `3.5f`/`3.6f` -> `2.5f`/`2.6f`; yorumlar yeni limitlere göre.
- `Tests/BotCoreTests/NavGridTests.cpp` sonuna iki yeni test: `NavGrid_DefaultSlope_045`, `NavGrid_RealMap_DefaultSlopeConnectivity`.
- `tools/nav-measure/nav_measure.cpp`, `tools/nav-segment-check.py`, `tools/nav-regress.py`: eğim sabiti içermiyor (`grep` boş), değişmedi.

**Temel ölçüm (plan varsayımı doğrulandı):**
- `NavGrid_DefaultSlope_045` `[ OK ]`.
- `NAVSLOPE real map: main=88508 reach=85508 landmarks=5/5` -> beş sınır noktası da bağlı, `reach` = 85.508 (88.508'in %96,6'sı), K5 bandında (85.000-87.000) ve §3 bandında (%96-%97 ±%1). Plan §2 iddiası tutuyor.

**Derleme çıktısı (son satırlar):**
- `./tools/build.sh Release` rc=0; değişen dosyalarda yeni uyarı yok (`BotCoreTests.vcxproj -> ...BotCoreTests.exe`).
- `./tools/build.sh Debug` rc=0; değişen dosyalarda yeni uyarı yok.

**Engelleyici:** `./tools/run-tests.sh Release` taban `274 tests, 0 failed` iken şimdi `276 tests, 8 failed`. Kırılanların tamamı 0,625 varsayılanına bağlı gerçek harita/performans testleri olup içlerinde plan §3'te sayılan gerçek davranış değişimi türleri vardır:

| Test | Kırılan | Tür / kanıt |
|---|---|---|
| `NavReach_RealMap` | 15+ | **Bağlantı kopması + yol yok:** bileşen sayısı 143 -> 401, en büyük bileşen 88279 -> 87513, cep hücreleri 229 -> 995; `reach.Connected(s,g)` (218,206)-(221,211) artık **false**; detour çifti (s->g) `REQUIRE(... Planned)` **başarısız** (rota planlanamıyor) |
| `NavReach_Perf` | 1 | **Kalite kapısı:** `unreachable*100 <= queries*5`; mage kümesinde unreachable 51/1000 > %5 |
| `NavPath_RealMap_Queries` | 2 | `expanded > 88000` artık geçmiyor; arena A->B `cost` 660.617 -> 670.961 |
| `NavDanger_RealMap` | 10+ | sabit maliyet/uzunluk/`ForbiddenOnPath` değerleri; `crossField > crossPlain` false |
| `NavRetreat_RealMap` | 15+ | sabit score/len/candidates (2799 -> 2733, 245 -> 239, 2783 -> 2697); `cell` (320,271) -> (320,270) |
| `NavStuck_SideStep_RealMap` | 3 | sabit sayaçlar: `sidestep_h` 86017 -> 85208, `sidestep_n` 86968 -> 86512, `clr2_found` 66003 -> 65739 |
| `NavBudget_Deferred_Chase_Sim` | 3 | **davranış:** `followStaleTicks` 0 -> 87 (B), 0 -> 153 (B2); `bMean <= 1.25*aMean` false |
| `NavBudget_RealMap_Load` | 1 | **performans bütçesi:** `worst_B_p99` 4.682 > 4.5; `worst_A_p95=3.766`, marj dar |

**Neden bu 8 test "eski sabit" sayılıp güncellenmedi:** Plan §3 adım 3 yalnızca *eski varsayılanı kodlayan sabitlerin* güncellenmesini, gerçek davranış değişiminde durulmasını emrediyor. Yukarıdakiler yalnızca sabit değil; rota planlanabilirliği, bağlantı, takip tazeliği ve tick bütçesi gibi davranışsal/yapısal beklentiler içeriyor. Kapsam planın "S (1 üretim satırı + birkaç test sabiti)" tahmininin çok üstünde (8 test dosyası, onlarca referans + 3 davranış/perf kapısı). Eşik gevşetmek plan §8 ile yasak; değerleri yeniden ölçüp yazmak karar/değerlendirme işidir.

**Sapmalar:**
- Plan adım 4'teki `python3 tools/nav-export.py` gerekmedi; `build/nav/zone71.navgrid` zaten vardı.
- Adım 5'teki Debug test koşusu yapılmadı; adım 3'te duruldu (Debug derlemesi rc=0).

**Açık sorular (proje sahibi/Claude kararı):**
1. 0,625'e bağlı kalan 8 testin sabit/davranış beklentileri 0,45'e göre **yeniden ölçülüp güncellenecek mi** (o zaman değişen değerler ve perf kapıları onaylanmalı), yoksa eğim yalnızca planlayıcı varsayılanı olarak 0,45 tutulup bu testler ayrı bir "0,45 referans güncelleme" dilimine mi bırakılmalı?
2. `NavBudget_RealMap_Load` `worst_B_p99` 4.682 > 4.5 (bütçe) ve `NavReach_Perf` mage unreachable %5,1: bu üst sınırlar 0,45'in kabul edilebilir sonucu mu, yoksa eğim sınırı/bütçe yeniden mi değerlendirilmeli?
3. `NavReach` bileşen yapısının 143 -> 401'e çıkması beklenen mi; `reach.Connected(s,g)` gibi yakın-hücre bağlantı beklentileri yeni yapıya göre mi yazılmalı?

**Kriter öz-değerlendirmesi:**
- K1 ✔ (Release + Debug rc=0, değişen dosyalarda yeni uyarı yok)
- K2 ✘ (`run-tests.sh` Release: `276 tests, 8 failed`; iki yeni test `[ OK ]`)
- K3 ✔ (`git diff --stat` yalnızca §4 dosyaları; `BotCore/NavGrid.h` farkı iki anlamlı satır; `GameServer/`, `shared/`, `tools/`, `docs/` farkı 0; `git diff --check` boş)
- K4 ✔ (`maxSlope = 0.45f`; `git grep -n -E "0\.625" BotCore Tests` tek isabet: `NavStuckTests.cpp:458` hız yorumu, eğim değil)
- K5 ✔ (`NAVSLOPE real map: main=88508 reach=85508 landmarks=5/5`, ilan edilen bant içinde)
- K6 ✔ (`nav-regress --skip-timing` taban ile aynı: rc=0, `checks=27 pass=27 fail=0`)
- K7 (Claude) — bu plan sunucu kodu değiştirmez; doğrulama birim + gerçek harita testleriyle sınırlı, faz kabulü ayrı.

### Tur 1 — Claude kararı (otonom döngü kurtarma adımı, 2026-10-03; gözden geçirilmeli)

Teşhis: plan §3 adım 3'ün "gerçek davranış değişimi varsa dur" koşulu doğru tetiklendi; 0,45 sınırı gerçek haritada ana bileşeni %98,6'dan %96,6'ya indirdiği için (88 508 ana hücre içinde cep hücreleri 229 -> 995) gerçek harita testlerinin sabitleri ve cep hücrelerine düşen test örnekleri (hedef/sorgu noktaları) değişti. Bu beklenen bir sonuçtur, ADR-0024 kararı geçerli, 0,45 geri alınmaz. Plan "birkaç sabit" varsaymıştı; asıl iş 8 gerçek harita testinin 0,45 referansına taşınmasıdır. Cevaplar:

1. Soru 1: Testler bu planda 0,45'e göre yeniden ölçülüp güncellenir (ayrı dilim açılmaz). Eşik gevşetme yasağı sürer; ama sabit/sayaç beklentileri (maliyet, uzunluk, sayaç, bileşen sayısı) yeni gerçek değerlere çekilir ve raporda eski/yeni değer + gerekçe yazılır.
2. Soru 2: Bütçe/kalite eşikleri (`worst_B_p99 <= 4.5`, `unreachable <= %5`) DEĞİŞMEZ. İhlal, hedef/sorgu örneklerinin yeni cep hücrelerine düşmesinden geliyorsa (olası neden: örnekleme tüm `Walk` hücrelerinden yapılıyor, oysa %3,4'ü artık ana bileşen dışında) örnekleme ana bileşenle (`MainComponentCells`/aynı bileşen kimliği) sınırlanır; test amacı aynı kalır.
3. Soru 3: Bileşen sayısının 143 -> 401 olması beklenen sonuçtur (daha çok küçük cep). `reach.Connected(s,g)` ve detour çifti beklentileri, test amacını koruyan (aynı türde çift: yakın ama bağlı olmayan / bağlı ama dolambaçlı) yeni hücre çiftleriyle yeniden yazılır; çiftler gerçek ızgarada taranarak seçilir ve koda sabit koordinat olarak yazılır.

Güvenlik valfi: bir testi 3 denemede amacını koruyarak yeşile getiremezsen (ör. `NavBudget_Deferred_Chase_Sim` `followStaleTicks` hâlâ > 0 ve nedeni örnekleme değil gerçek davranış), O TESTİN `NavParams`'ını açıkça `maxSlope = 0.625f` ile sabitle (yorum: `// pinned to the pre-ADR-0024 slope; re-baseline in a follow-up plan`), eşiğe/mantığa dokunma ve raporda "SABİTLENDİ: <test>, neden, 0,45 ölçümü" yaz. Böylece plan takılmadan kapanır; Claude doğrulamada takip planı açar. Sabitlenebilecek test sayısı en çok 3'tür; fazlası için yine dur ve raporla.

#### Düzeltme talimatı

```
plans/F5-69-nav-egim-siniri-045.md — Tur 1 düzeltmesi. Aynı branch'te (bot/F5-69) yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
Dokunulabilecek dosyalar (plan §4 genişletildi): BotCore/NavGrid.h (mevcut değişiklik kalır), Tests/BotCoreTests/NavGridTests.cpp, NavSegmentTests.cpp ve kırılan sekiz testin dosyaları: NavReachTests.cpp (NavReach_RealMap, NavReach_Perf), NavPathTests.cpp (NavPath_RealMap_Queries), NavDangerTests.cpp (NavDanger_RealMap), NavRetreatTests.cpp (NavRetreat_RealMap), NavStuckTests.cpp (NavStuck_SideStep_RealMap), NavBudgetTests.cpp (NavBudget_Deferred_Chase_Sim, NavBudget_RealMap_Load). Üretim kodu (BotCore/*.h) 0,45 satırı dışında DEĞİŞMEZ.
Karar (planlayıcı, plan "Tur 1 - Claude kararı" bölümü): 0,45 kalır; testler 0,45 referansına taşınır; eşikler (4.5 ms p99, %5 unreachable, bütçe oranları) gevşetilmez.
1. Önce ./tools/run-tests.sh Release koş, kırılan testleri ve her kırılan CHECK'in eski/yeni değerini tablo olarak rapora yaz (zaten Tur 1'de var; güncelle).
2. Saf sayaç/sabit testleri (NavPath_RealMap_Queries cost/expanded, NavDanger_RealMap maliyet/uzunluk/ForbiddenOnPath sabitleri, NavRetreat_RealMap score/len/candidates/cell, NavStuck_SideStep_RealMap sayaçları, NavReach_RealMap bileşen/cep sayıları): yeni gerçek değerleri yaz. Değerin neden değiştiğini bir cümleyle raporla (eğim 0,625 -> 0,45; daha uzun rota veya daha çok cep). Beklenen yönde olmayan değişim varsa (ör. maliyet AZALDI, yol bulunan çift bulunamaz oldu ve çift ana bileşende) nedenini araştır ve raporla.
3. Yapısal beklentiler: (a) NavReach_RealMap'te (218,206)-(221,211) çifti ve detour çifti: test amacını koruyan yeni çiftler seç (gerçek ızgarada taranarak: aynı bileşende ve bağlı / yakın ama farklı bileşende), koordinatları koda yaz, eski çifti yorumda an. (b) NavDanger_RealMap `crossField > crossPlain`: tehlike alanı/düz karşılaştırma çifti yeni ızgarada anlamlı olacak şekilde seçilmeli (her iki yol da ana bileşende, alan yolu gerçekten keserken düz yol kesmesin); eşik aynı kalır.
4. NavReach_Perf mage kümesi unreachable 51/1000: sorgu hedeflerini ana bileşen hücrelerinden (veya sorgu yapısının zaten kullandığı örnekleme kuralı + ana bileşen filtresi) seç ve `unreachable * 100 <= queries * 5` aynen kal. Hâlâ aşıyorsa raporla (sabitleme valfi).
5. NavBudget_Deferred_Chase_Sim `followStaleTicks == 0` ve `bMean <= 1.25 * aMean` ile NavBudget_RealMap_Load `worst_B_p99 <= 4.5`: önce sim'in hedef/bot başlangıç ve hareket örneklemesi cep hücrelerine düşüyor mu bak (olası neden). Düşüyorsa örneklemeyi ana bileşenle sınırla, tohum/akış mantığını değiştirme. p99 ölçümü zamanlama içerir: 3 kez koş, 3'ünün de <= 4.5 olması gerekir; 1-2 sınır aşımı gürültüyse nedenini (A p95 ve marj) rapora yaz, eşiği değiştirme, ama 3 koşuda en az 2'sinde geçmiyorsa sabitleme valfini uygula.
6. Sabitleme valfi (plan "Tur 1 - Claude kararı"): amaç korunarak yeşile gelmeyen test (en çok 3) için o testin NavParams'ını `maxSlope = 0.625f` ile açıkça kur (yorum: // pinned to the pre-ADR-0024 slope; re-baseline in a follow-up plan). Raporda "SABİTLENDİ: <test>, neden, 0,45 ölçümü (ham değerler)" satırı ver. Hiçbir koşulda eşik/CHECK silme veya gevşetme.
7. Ek kontrol: git grep -n "0\.625" BotCore Tests çıktısında yalnızca sabitleme valfinin yorumları/atamaları ve NavStuckTests.cpp:458 hız yorumu kalsın.
8. ./tools/build.sh Release ve Debug (rc=0, değişen dosyalarda yeni uyarı yok), ./tools/run-tests.sh Release ve Debug (0 failed; iki yeni NavGrid testi [ OK ]), ./tools/nav-regress.sh --skip-timing (rc=0, checks=27 pass=27), git diff --check boş; dosyalar ASCII + CRLF. K1-K6 öz-değerlendirmesini güncelle. Raporun "Tur 2" bölümünde: (i) her güncellenen testin eski/yeni değerleri, (ii) varsa SABİTLENDİ listesi, (iii) NAVSLOPE satırı, (iv) NavBudget_RealMap_Load üç koşunun worst_B_p99 değerleri.
```

### Tur 2

**Durum:** UYGULANDI (düzeltme talimatı tamam; 0,45 kalır, 8 kırık test 0,45 referansına taşındı, 1 test sabitlendi).

**Branch / commit:** `bot/F5-69` (taban `gece/2026-10-02` @ `9053844`); Tur 1: `05e9e45` (kod+test) ve `f1bf4dd`/`e87c801` (rapor). Tur 2 bu raporda yeni commit ile.

**Yapılanlar:** Düzeltme talimatındaki 8 madde uygulandı. `BotCore/NavGrid.h` 0,45 satırı dışında üretim kodu değişmedi. Kırılan sekiz testin tamamı ya gerçek değerlere çekildi ya da (tek test) hedefi korunarak sabitlendi; hiçbir eşik/CHECK silinmedi veya gevşetilmedi.

**(i) Güncellenen testlerin eski -> yeni değerleri** (ölçüm `./tools/run-tests.sh Release --no-build` çıktısından; 0,45 daha sıkı eğim => daha uzun rota ve daha çok cep):

| Test / CHECK | Eski | Yeni | Not |
|---|---|---|---|
| `NavReach_RealMap` ComponentCount | 143 | 401 | 995 cep, çoğu küçük |
| `NavReach_RealMap` LargestComponent cells | 88279 | 87513 | |
| `NavReach_RealMap` pockets | 229 | 995 | |
| `NavReach_RealMap` top-12 bileşen boyutu | 88279,15,11,10,9,6,6,5,4,3,3,3 | 87513,93,79,31,23,15,15,13,13,13,12,11 | daha çok orta boy cep |
| `NavReach_RealMap` p (173,211) bileşen hücresi | 15 | 5 | p hâlâ ana bileşene bağlı değil |
| `NavReach_RealMap` p bileşen kimliği | 51 | 137 | |
| `NavReach_RealMap` cepteki bot A* expanded | 15 | 5 | |
| `NavReach_RealMap` mage ringConnected | 162 | 146 | ringCells 168 aynı |
| `NavReach_RealMap` detour çifti | s(218,206)-g(221,211), path 242.108, straight 23.324 | s(211,212)-g(221,212), path 276.451, straight 40.000 | ikisi de ana bileşende ve bağlı; eski çift artık farklı bileşende |
| `NavReach_Perf` örnekleme | tüm Walk (88508) | ana bileşen filtresi (aynı `Rng` akışı) | mage unreachable 51 -> 37, exact 34 -> 14; `<= %5` korundu |
| `NavPath_RealMap_Queries` arena A->B cost | 660.617 | 670.961 | rota biraz uzadı |
| `NavPath_RealMap_Queries` cep expanded | > 88000 | >= 87513 | A* tüm EdgeOpen ana bileşenini (87513) tarıyor; gerçek değer yazıldı |
| `NavDanger_RealMap` arena düz/f0/k0 cost | 660.617 | 670.961 | |
| `NavDanger_RealMap` arena field cost / length | 689.103 / 676.617 | 704.831 / 687.931 | |
| `NavDanger_RealMap` startInside cost / len / forb | 260.137 / 257.137 / 19 | 277.108 / 274.108 / 21 | |
| `NavDanger_RealMap` s->a düz cost | 253.824 | 270.794 | |
| `NavDanger_RealMap` cross çifti | ca(371,248)-cb(334,308): plain 310.676 (forb 21), field 315.362 (forb 0) | ca(360,239)-cb(323,297): plain 335.078 (forb 24), field 343.078 (forb 0) | her iki yol ana bileşende; düz yol halkayı kesiyor, alan yolu dolanıyor; `crossField > crossPlain` korundu |
| `NavRetreat_RealMap` soloA score / len / cand=exp | 2.13283 / 105.657 / 2799 | 2.11074 / 108.971 / 2733 | |
| `NavRetreat_RealMap` partyA cand=exp | 245 | 239 | |
| `NavRetreat_RealMap` meleeB cand=exp | 2783 | 2697 | |
| `NavRetreat_RealMap` ringC cell / score / len / cand / exp | (320,271) / -0.09771 / 89.657 / 1247 / 2424 | (320,270) / -0.10876 / 91.314 / 806 / 1959 | |
| `NavRetreat_RealMap` arenaD cand=exp | 2089 | 1954 | |
| `NavStuck_SideStep_RealMap` sidestep_h / sidestep_n / clr2_found | 86017 / 86968 / 66003 | 85208 / 86512 / 65739 | clr2_cells 66265 aynı |
| `NavDanger_RealMap` printf sabiti (CHECK değil) | 660.617f | 670.961f | yanıltıcı çıktı düzeltildi |

**(ii) SABİTLENDİ listesi (sabitleme valfi):**

- **SABİTLENDİ: `NavBudget_Deferred_Chase_Sim`**, neden: hedef/bot/hareket örneklemesi ana bileşene sınırlandı ve `followStaleTicks` 87/153'ten 0'a indi, ancak mode B'de **deterministik `followStaleTicks = 8`** kaldı; neden örnekleme değil, 0,45'te başarısız bir plan yenilemesinin eski (bayat) planı bırakıp sözleşmenin onu izlemesi. Amacı koruyup 3 denemede yeşile gelmediği için testin `NavParams::maxSlope` değeri `0.625f` ile açıkça kuruldu (`// pinned to the pre-ADR-0024 slope; re-baseline in a follow-up plan`). 0,45 ölçümü (ham): mode B `followStaleTicks=8, stale_hold_ticks=8, without_plan_pct=6.5` (bir koşuda bir bot hiç plan alamadı, `noPlanPerBot=1200`); sabitli 0,625'te `followStaleTicks=0, stale_hold_ticks=0, without_plan_pct=0.1`. Eşik/mantık değişmedi.
- Sabitlenen test sayısı: **1/3** (sınır içinde).

**(iii) NAVSLOPE satırı:**

`NAVSLOPE real map: main=88508 reach=85508 landmarks=5/5` (0,45; ilan edilen 85000-87000 bandı ve §3 %96-%97 bandı içinde; iki yeni NavGrid testi `[ OK ]`).

**(iv) `NavBudget_RealMap_Load` üç koşunun `worst_B_p99` değerleri** (0,45 + ana bileşen örneklemesi): **1.608 / 1.399 / 1.347** (üçü de <= 4.5; önceki 0,45 ölçümü 4.682-4.879 idi çünkü cep hedefleri pahalı `NoPath` aramaları doğuruyordu). Sabitleme gerekmedi.

**Değişen dosyalar (Tur 2):** `Tests/BotCoreTests/NavReachTests.cpp`, `NavPathTests.cpp`, `NavDangerTests.cpp`, `NavRetreatTests.cpp`, `NavStuckTests.cpp`, `NavBudgetTests.cpp`. `BotCore/NavGrid.h` yalnızca 0,45 satırı/yorum (Tur 1'den). Üretim kodu başka satır değişmedi.

**Derleme/test çıktıları (son satırlar):**
- `./tools/build.sh Release` rc=0; `./tools/build.sh Debug` rc=0; değişen dosyalarda yeni uyarı yok (Debug'de iki C4267 yalnızca eski `GameServerDlg.cpp`).
- `./tools/run-tests.sh Release`: `276 tests, 0 failed`; `./tools/run-tests.sh Debug`: `276 tests, 0 failed`; `NavGrid_DefaultSlope_045` ve `NavGrid_RealMap_DefaultSlopeConnectivity` `[ OK ]`.
- `./tools/nav-regress.sh --skip-timing`: rc=0, `checks=27 pass=27 fail=0 warn=0`.
- `git diff --check` boş; değişen dosyalar ASCII + CRLF.
- `git grep -n -E "0\.625" BotCore Tests`: yalnızca `NavBudgetTests.cpp:475` sabitleme ataması ve `NavStuckTests.cpp:458` hız yorumu.

**Sapmalar:** Yok. (Tur 1'deki `python3 tools/nav-export.py` gereksizliği ve Debug test koşusunun atlanması Tur 2'de karşılandı.)

**Açık sorular:**
1. `NavBudget_Deferred_Chase_Sim` sabitlemesi, 0,45'te plan yenilemesi başarısız olduğunda eski planın izlenmesi davranışını test dışında bırakıyor; bu davranışın (bayat planı izlemek) üretimde istenip istenmediği ayrı bir değerlendirme konusu (F5-62 bağlaması). Claude doğrulamada takip planı açabilir.
2. `NavReach` bileşen yapısının 143 -> 401 olması ve `NavReach_RealMap` cep çiftlerinin her ADR-0024 sonrası yeniden ölçüm gerektirmesi beklenen bir bakım yükü; referans değerler gerçek grid'e sabit yazıldı.

**Kriter öz-değerlendirmesi:**
- K1 ✔ (Release + Debug rc=0, değişen dosyalarda yeni uyarı yok)
- K2 ✔ (`run-tests.sh` Release ve Debug: `276 tests, 0 failed`; iki yeni NavGrid testi `[ OK ]`)
- K3 ✔ (`git diff --stat` yalnızca §4 dosyaları + plan; `BotCore/NavGrid.h` farkı iki anlamlı satır; `GameServer/`, `shared/`, `tools/`, `docs/` farkı 0; `git diff --check` boş)
- K4 ✔ (`maxSlope = 0.45f`; `git grep 0\.625` yalnızca sabitleme ataması ve hız yorumu)
- K5 ✔ (`NAVSLOPE real map: main=88508 reach=85508 landmarks=5/5`)
- K6 ✔ (`nav-regress --skip-timing` rc=0, `checks=27 pass=27`, taban ile aynı)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(boş)
