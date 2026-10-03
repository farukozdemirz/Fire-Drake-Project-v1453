# F5-69: Navigasyon eğim sınırı 0,625 → 0,45 (T-NAV-02 ölçümü, proje sahibi kararı ADR-0024)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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

(boş)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(boş)
