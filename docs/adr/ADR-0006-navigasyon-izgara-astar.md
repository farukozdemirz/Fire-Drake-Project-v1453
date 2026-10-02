# ADR-0006: Navigasyon: SMD ızgarası üzerinde A* (hibrit ilerideki dilim), görüş hattı önce `los_grid` ve `advisory` (otonom döngüde Claude kararı — gözden geçirilmeli)

Durum: KABUL (geçici, proje sahibi gözden geçirecek) · Tarih: 2026-10-02 · Karar veren: Claude (gece modu, paralel hat `nav`, `AUTO_LOOP=1`; kullanıcıya sorulamadı)
İlgili: `docs/12` §1–§5, §11 (T-NAV-03, AC-NAV-02), `docs/18` R-09, `docs/21` ADR tablosu, ADR-0016 (`BotCore` saflığı); planlar F5-01 (veri modeli, KAPANDI), F5-02 (A*)

## Bağlam
`docs/12` §3 üç yaklaşımı karşılaştırır (ızgara A*, navmesh, waypoint) ve "ızgara A* temel, hibrit önerilen uygulama, navmesh yalnızca ızgara yetersiz kalırsa" der. Karar AÇIK görünüyordu (`docs/21` ADR tablosu), çünkü F5 başlamadan onaylanması beklenmişti. Paralel hat F5-01 ile veri modelini (`BotCore/NavGrid.h`, zone 71: 513×513 köşe, 4 m, ana bileşen 88 508 hücre) kurdu ve F5-02 A*'ı yazacak; karar artık gerekli.

F5-02 öncesi, A*'ın başarı ölçütünü (AC-NAV-02: p95 ≤ 2 ms, ≤ 20 000 düğüm) belirlemek için gerçek haritada bir **Python prototipi** (depoya girmeyen, geçici; aynı kenar kuralı: kenara değmeyen en büyük 4-bağlantılı açık bileşen, eğim 0,625, köşe kesme yok, octile) koşturuldu `[V]` (ölçümler 2026-10-02, 150'şer sorgu, tohumlu):

| Sorgu dağılımı (Chebyshev, hücre) | Genişletilen düğüm p50 / p95 / p99 | Sonuç |
|---|---|---|
| ≤ 32 (≤ 128 m) | 99 / 674 / 1 865 | 149 bulundu, 1 yol yok |
| ≤ 50 (≤ 200 m) | 172 / 2 438 / 4 491 | 149 bulundu, 1 yol yok |
| ≤ 150 (≤ 600 m) | 1 194 / 6 512 / 7 944 | 147 bulundu, 3 yol yok |
| tüm ana bileşen, düzgün rastgele | 4 534 / 19 042 / 33 444 | 150 bulundu; 6'sı > 20 000 |
| arena A (1274, 890) → B (746, 1106) | 3 247 | bulundu, maliyet 660,6 m |

Ek bulgular `[V]`: (a) Ana bileşendeki 88 508 hücrenin 229'u eğim kenarlarıyla ana alandan kopuk küçük "cep"lerdir (en büyüğü 15 hücre); A hücresinden cebe sorgu **tüm erişilebilir bölgeyi (88 279 düğüm) tüketip yol yok** sonucunu verir. (b) Dolayısıyla "yol yok" ancak çok pahalı bir aramayla kanıtlanabilir; 20 000 düğüm sınırı bunu keser ve sonucu "bilinmiyor" yapar.

## Karar
1. **Temel yaklaşım ızgara A*'dır** (`docs/12` §3 aynen); navmesh ve waypoint grafiği bu ADR kapsamında **seçilmedi**. Navmesh için tetikleyici `docs/18` R-09: MET-NAV-01 (takılma ≤ 2 / bot-saat) ızgarayla tutmazsa yeni ADR.
2. **A* çekirdeği (`BotCore/NavPath.h`, F5-02):** 8 komşu, köşe kesme yok (kenar kuralı `NavGrid::EdgeOpen`), octile sezgisel, ikili yığın (`std::push_heap`/`pop_heap`, tembel silme) açık liste, hücre başına düğüm dizileri (düğüm havuzu), belirlenimli beraberlik kuralı. Maliyet bu dilimde yalnızca yatay mesafedir (metre); `danger`/`clearance` ağırlıkları F5-06, eğim cezası ileride (sert eğim kesmesi zaten `EdgeOpen`'dadır; yumuşak ceza T-NAV-02 ölçümü olmadan eklenmez).
3. **Düğüm sınırı `P-NAV-MAX-NODES` = 20 000 `[Ö]`** kapatılan (genişletilen) düğümdür; aşılırsa sonuç **`NodeLimit`**'tir ve "ulaşılamaz" **değil**, "bilinmiyor" anlamına gelir. Hiyerarşik arama (`region_graph`) bu dilimde yoktur; `docs/12` §4.1'deki "aşılırsa hiyerarşik arama" ileriki bir plana kalır. Ulaşılamaz hedef kararı (`TARGET_UNREACHABLE`) F5-05'in işidir ve `NoPath`/`NodeLimit` ayrımını kullanır.
4. **T-NAV-03 sorgu dağılımı (AC-NAV-02'nin ölçüm koşulu) `[Ö]`:** "1000 rastgele sorgu" şöyle tanımlanır: tohumlu `Rng` ile ana bileşenden düzgün çekilen başlangıç/hedef çiftleri, **Chebyshev mesafesi ≤ 64 hücre (256 m)** ile sınırlı (arena ölçeği; botun savaş sırasındaki yol bulma sorguları bunlardır). Kapı: Release'te p95 süre ≤ 2 ms ve başarı ≥ %95. **Raporlanan ama kapı olmayan** iki ek küme: ≤ 150 hücre (600 m, respawn → arena yürüyüşünü kapsar) ve sınırsız (tüm harita). Gerekçe: sınırsız çiftlerde p95 genişletme ≈ 19 000 düğümdür; 2 ms bunun için gerçekçi bir bütçe değildir ve bot uzun yürüyüşleri hiyerarşik aramaya/ara noktalara bırakır (`docs/12` §3 hibrit). Bu, `docs/12` §11 T-NAV-03 satırına not düşülerek kayda geçirildi.
5. **Görüş hattı (LoS):** `P-NAV-LOS-MODE` varsayılan `advisory` kalır (`docs/12` §5); ilk gerçekleme `los_grid` (ızgara üzerinde engelli hücre var mı) olacaktır (F5-10). `los_mesh` (N3ShapeMgr üçgenleri) yapılmaz; T-NAV-LOS-01 (istemci engel arkasına skill kullanabiliyor mu) ölçülmeden `enforce` moduna geçilmez.
6. **Yürünebilirlik, yükseklik ve LoS bilgisi yalnızca bot tarafındadır** (CLI-08; sunucu yalnızca harita sınırına bakar): sunucu davranışı ve `GameServer` bu kararla değişmez; `NavPath.h` ADR-0016 uyarınca başlık-yalnızca ve sunucu başlığı içermez.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Navmesh (Recast/Detour) | Düzgün yollar, ince geçitler | Ek bağımlılık/üretim hattı; sunucu çarpışma verisi doğrulanmadı; ADR-0016 bağımlılık yasağı | Izgara yetersiz kalırsa (R-09) |
| Waypoint grafiği | Hızlı, öngörülebilir | El bakımı, arazi değişimine uyumsuz | Yalnızca solo dolaşma rotaları için (ayrı karar) |
| Düzgün rastgele (tüm harita) sorgularla 2 ms kapısı | `docs/12`'nin lafzına en yakın | p95 ≈ 19 000 düğüm; 2 ms'yi tutturmak için erken hiyerarşik arama/önhesaplama gerekir, dilimi büyütür ve kapıyı gerçek kullanımdan koparır | Kapı arena ölçeği, diğerleri raporlanır |
| `NodeLimit` yerine sessizce "yol yok" | Basit | 88 508 hücrelik bölgede yanlış "ulaşılamaz" kararı: bot hedefi yanlış bırakır | Üç durum (Found/NoPath/NodeLimit) ayrı tutulur |

## Sonuçlar
- Olumlu: gereksiz bağımlılık yok; A* sunucusuz, belirlenimli ve milisaniyeler içinde birim testlidir; performans kapısı gerçek kullanıma dayanır; "yol yok" ile "bilinmiyor" ayrımı F5-05'in temelidir.
- Olumsuz: uzun mesafeli sorgular (> 256 m) 2 ms bütçesini aşabilir; hiyerarşik arama olmadan bunlar `NodeLimit` verebilir (dağılımın ~%4'ü, yukarıdaki tablo). Dar geçitlerde 4 m ızgara kaba kalabilir (R-09).
- Geri alma: `BotCore/NavPath.h` ve testi silinir; `NavGrid` ve sunucu etkilenmez.

## Doğrulama
F5-02: `NavPath_*` birim testleri (Dijkstra ile maliyet eşitliği, köşe kesme yok, düğüm sınırı, gerçek harita arena A → B maliyeti ≈ 660,6 m, cep hedefi `NoPath`), `NAVPATH T-NAV-03` satırları (Release p95 ≤ 2 ms kapısı yalnızca ≤ 64 hücre kümesinde).

## Ek F5-03: Yol düzleştirme (otonom döngüde Claude kararı — gözden geçirilmeli)
Tarih: 2026-10-02 · Plan: `plans/F5-03-nav-yol-duzlestirme.md` · Dayanak: `docs/12` §4.1 ("hücre merkezleri arasında, ızgara üzerinde Bresenham yürüyüşü engelsizse ara noktalar atlanır").

1. **Görünürlük tanımı:** `NavLineClear(a, b)` = `a`→`b` tamsayı Bresenham yürüyüşünün her adımı `NavGrid::EdgeOpen`'dır. Yürünebilirlik, eğim ve köşe kesmeme kuralı tek yerde (`EdgeOpen`) kalır; düzleştirilmiş her doğru parçası A*'ın da yürüyebileceği geçerli bir hücre yürüyüşüdür ve uzunluğu `NavOctile`'dir (bu, mülkiyet testinin dayanağı).
2. **Kanonik yön:** Bresenham beraberlik anları yöne bağımlı olduğundan `a`/`b` sözlük sırasına (önce `x`, sonra `z`) çevrilir; `NavLineClear` simetriktir.
3. **Algoritma:** açgözlü, her çıpadan en uzak görünür yol hücresi (geriye doğru ilk görünür), `P-NAV-SMOOTH-LOOKAHEAD` = 64 yol hücresi `[A]` ile sınırlı; en kötü durum maliyeti sınırlı ve belirlenimli. Sonuç girdinin alt dizisidir (ilk/son korunur); Öklid uzunluğu A* maliyetinden büyük olamaz.
4. **Doğrulama (prototip `[V]`, 2026-10-02):** arena A→B 150 hücre / 660,617 m → 13 ara nokta / 635,787 m; `near64` yollarında ortalama 55 → 6 ara nokta, uzunluk oranı 0,9575; düzleştirme `EdgeOpen` çağrısı p95 ≈ 2 600.
5. **Bilinen sınırlama `[A]`:** Bresenham `supercover` değildir (hücre sınırından tam geçen doğrunun öbür tarafı yalnızca çapraz adımın köşe kuralıyla denetlenir), 4 m ızgara ve gövde genişliği yok. `supercover` ya da güvenlik payı (clearance ≥ 2 hücre koşulu) MET-NAV-01/T-NAV-04 takılma ölçümü kötü çıkarsa yeni karar konusudur (R-09 ile aynı tetik).
