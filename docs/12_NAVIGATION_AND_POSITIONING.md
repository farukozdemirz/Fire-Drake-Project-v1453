# 12 — Navigasyon ve Konumlandırma

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Harita mekaniği: [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §9 (MEC-MOV-*), §12.2. Entegrasyon noktası: [02](02_REPOSITORY_ANALYSIS_AND_INTEGRATION_MAP.md) S10. Harita görselleri: `appendix/maps/`.
> Bu doküman navigasyon verisinin, yol bulmanın, takılma kurtarmanın ve güvenli konum seçiminin tek kaynağıdır.

---

## 1. Doğrulanmış temel

| Bulgu | Etiket | Kaynak |
|---|---|---|
| Sunucu zone 71 için `freezone_a_20050718.smd` yükler | `[V]` | ZONE_INFO; SMD warp kimlikleri 71xx |
| Harita 513 × 513 köşe, 4 m birim, 2048 × 2048 m | `[V]` | SMD ayrıştırıcı (`appendix/tools/smd_parse.py`) |
| Olay ızgarası: `0 = engelli`, `1 = açık`; indeks `x·513 + z` | `[V]` | Çarpışma poligonlarının %100'ü 0 hücrelerinde (transpoze edilmiş indeksle ~%13) |
| Yükseklik ızgarası aynı yerleşimde; −30,6 … 82,1 m | `[V]` | |
| Çarpışma geometrisi (34 871 yüz) yükleniyor ama **sunucuda hiç sorgulanmıyor** | `[D]` | [`N3BASE/N3ShapeMgr.cpp:52-114`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/N3BASE/N3ShapeMgr.cpp#L52-L114); grep |
| Sunucu hareketi yalnızca harita sınırıyla doğruluyor; **yürünebilirlik, yükseklik, görüş hattı kontrolü yok** | `[D]` | [`shared/SMDFile.cpp:194-198`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SMDFile.cpp#L194-L198), MEC-MOV-03 |
| GameServer'da yol bulma yok; AIServer A* yürünebilirliği ters yorumluyor | `[D]` | [`AIServer/MAP.cpp:124-127`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/MAP.cpp#L124-L127) (MB-11) |
| Ana oynanabilir alan: 4-bağlantılı en büyük iç bileşen, 88 508 hücre (~1,42 km²); iki ulusun respawn noktası ve merkez bu bileşende | `[V]` | `appendix/maps/zone71_components4.png` |
| Haritanın dış bandı (115 363 hücre) ulaşılamaz | `[V]` | |
| Göller: iç kısımlar ana alandan kopuk yürünebilir cepler, kıyılar engelli | `[V]`/`[I]` | Bileşen görseli + yükseklik görseli |
| Bu harita resmî 2005 Colony Zone haritasıyla örtüşüyor | `[S]` | [19](19_SOURCES_AND_EVIDENCE.md) W-10 |

**Sonuç:** Botun yürünebilirlik kuralını kendisinin uygulaması zorunludur (CLI-08). Aksi halde sunucu duvardan geçen hareketi kabul eder; insan oyuncu bunu yapamaz ve bu bir "gizli avantaj" olur.

## 2. Navigasyon veri katmanları `[Ö]`

GameServer başlangıcında zone 71 için bir kez hesaplanır. `C3DMap`, `SMDFile`'ın friend sınıfıdır ([`shared/SMDFile.h:69-70`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SMDFile.h#L69-L70)); yükseklik ve olay ızgarasına kopyalamadan erişilebilir.

| Katman | Hücre | İçerik |
|---|---|---|
| `walk` | 4 m | Olay = 1 ve ana bileşende |
| `slope` | 4 m | Komşu yükseklik farkı; `|Δh| > P-NAV-MAX-STEP` (başlangıç 2,5 m / 4 m) ise kenar engelli `[Ö]`. Gerçek istemcinin tırmanabildiği eğim T-NAV-02 ile kalibre edilir. |
| `clearance` | 4 m | En yakın engelli hücreye mesafe (BFS, hücre) |
| `danger_static` | 4 m | Karşı ulusun guard tower halkası (kapıya ≤ 90 m), canavar spawn dikdörtgenleri + arama menzili. Takıma göre iki ayrı katman (Karus/El Morad). |
| `danger_dynamic` | 4 m | Görünür düşmanların etki haritası (melee: 15 m çekirdek, mage: 45 m halka), 500 ms'de bir güncellenir |
| `region_graph` | 48 m | Bölge düzeyinde bağlantı grafiği (uzun yollar için hiyerarşik arama) |

Bellek: 263 169 hücre × birkaç bayt ≈ birkaç MB.

## 3. Yaklaşım karşılaştırması

| Yaklaşım | Artı | Eksi | Karar |
|---|---|---|---|
| **Izgara A*** (olay ızgarası + eğim + maliyet katmanları) | Veri hazır, sunucunun ve AIServer'ın kullandığı aynı temsil; basit; deterministik | 4 m çözünürlük dar geçitlerde kaba; köşe kesme riski | **Temel yaklaşım** |
| Navmesh (Recast/Detour; çarpışma geometrisi + arazi) | Düzgün yollar, ince geçitler, standart araç | Ek bağımlılık ve üretim hattı; sunucu çarpışma verisinin kalitesi doğrulanmamış (y ±5000 sınır duvarları) | İleride, ızgara yetersiz kalırsa (ADR-0006) |
| Waypoint grafiği | Hızlı, öngörülebilir rotalar | El ile bakım; arazi değişimine uyumsuz | Yalnızca solo dolaşma rotaları (P-SOLO-ROAM-ROUTE) için |
| **Hibrit** (bölge grafiği → ızgara A* → yol düzleştirme → yerel yönlendirme) | Uzun yolda hızlı, yakında hassas | Biraz daha karmaşık | **Önerilen uygulama** |

## 4. Yol bulma

### 4.1 A* tanımı `[Ö]`

- 8 komşu; çapraz geçişte iki ortogonal komşu da açık olmalı (köşe kesme yok).
- Maliyet = mesafe × (1 + `w_danger`·danger + `w_clear`·max(0, 2 − clearance)) + eğim cezası.
- Sezgisel: octile mesafe (kabul edilebilir).
- İkili yığın (binary heap) açık liste, düğüm havuzu; düğüm limiti `P-NAV-MAX-NODES` = 20 000; aşılırsa hiyerarşik arama.
- Yol düzleştirme: hücre merkezleri arasında, ızgara üzerinde Bresenham yürüyüşü engelsizse ara noktalar atlanır.
- AIServer `CPathFind`'ın sezgisel ve yürünebilirlik hataları (MB-11) bu uygulamaya **taşınmaz**.

### 4.2 Hareketli hedef

- Hedefin son 1 sn'lik hız vektöründen öngörü noktası: `p + v·min(1,5 sn, mesafe/kendi_hız)`.
- Yeniden planlama: hedef ≥ 6 m yer değiştirdiğinde veya 500 ms'de bir (hangisi önce).
- Menzil hedefi: yol, hedefin etrafında rol menzili halkasındaki en yakın ulaşılabilir hücreye planlanır (warrior: melee halkası; mage: P-MAG-PREF-RANGE).

### 4.3 Ulaşılamayan hedef

`unreachable` koşulları: A* başarısız; yol uzunluğu > 3 × düz mesafe ve > 120 m; ya da hedef engelli bir cepte (farklı bileşen). Bu durumda hedef `TARGET_UNREACHABLE` ile 3 sn içinde bırakılır ([09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) §5.4, MET-NAV-04).

## 5. Görüş hattı (LoS) ile yürünebilirliğin ayrılması

- **Yürünebilirlik:** yalnızca §2 katmanlarıyla.
- **Görüş hattı:** sunucu kontrol etmiyor (MEC-R, MEC-MAG). İki yaklaşık test tanımlanır:
  - `los_grid`: iki nokta arasında ızgara üzerinde engelli hücre var mı (ucuz, kaba).
  - `los_mesh`: N3ShapeMgr alt hücrelerindeki çarpışma üçgenlerine ışın testi + arazi yüksekliği (doğru, pahalı; [`N3BASE/My_3DStruct.h:263-314`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/N3BASE/My_3DStruct.h#L263-L314) `_IntersectTriangle` mevcut ama kullanılmıyor).
- `P-NAV-LOS-MODE` (varsayılan `advisory`): LoS **aksiyonu engellemez**, yalnızca konum seçimini yönlendirir (ör. priest heal hedefine "görüşü olan" noktayı tercih eder). Gerçek istemcinin engel arkasına skill kullanmaya izin verip vermediği T-NAV-LOS-01 ile ölçülür. İstemci izin vermiyorsa mod `enforce` yapılır ve bot bu durumda aksiyon göndermez (adalet kuralı).
- Algı: bot, istemciye gelen bilgiyle aynı şekilde 3×3 bölgedeki tüm birimleri görür ([03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §16). İnsan istemcisi birimleri duvar arkasında çiziyorsa bu adaletsiz değildir `[A]`.

## 6. Hareket uygulaması

| Kural | Değer | Dayanak |
|---|---|---|
| Hareket paketi | `WIZ_MOVE` gerçek istemci sıklığında: **~1,5 sn'de bir** (ölçüldü, `docs/03` §13.2; eski 250 ms varsayımı kaldırıldı); paket konumu hedef noktadır, ara noktaları sunucu doğrulamaz (§13.1) | MEC-MOV-01, CLI-05 |
| Hız alanı | Gerçek istemcinin gönderdiği değer; asla > 67 (W/M/P) | MEC-MOV-02 |
| Adım uzunluğu | Temel koşu hızı × aralık × (hız buff/debuff çarpanı); temel koşu hızı T-NAV-01 ile ölçülür | CLI-05 |
| Durma | Ayakta skill'ler öncesi `speed=0` paketi | CLI-09 |
| Yükseklik (y) | Arazi yüksekliğinden bilinear enterpolasyon | `[Ö]` |
| `WIZ_SPEEDHACK_CHECK` | Gerçek istemci gönderiyorsa aynı sıklıkla | CLI-12 |

## 7. Güvenlik bölgeleri

| Bölge | Tanım | Bot kuralı |
|---|---|---|
| Karşı ulus tower halkası | Karşı ulus kapısına ≤ 90 m (tower'lar 25–50 m halkada, arama menzili 35 m) `[V]` | Girilmez; hedef bu bölgeye girerse takip biter |
| Kendi tower halkası | Kendi kapımıza ≤ 90 m | Geri çekilme ve solo RECOVER için güvenli bölge |
| Canavar alanları | Spawn dikdörtgeni + arama menzili | Yol maliyetini artırır; test arenasında bulunmaz ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md) §2) |
| Arena sınırı (test modu) | Senaryo tanımı | Bot arena dışına yol planlamaz |

## 8. Güvenli geri çekilme noktası

```
safe_point(bot, mode):
  cand = reachable tiles within R (party: 40 m, solo: 150 m) in main component
  for c in cand:
     s(c) = − w1·danger_dynamic(c) − w2·danger_static(c) − w3·path_len(bot,c)/R
            + w4·(party ? closeness_to_backline(c) : closeness_to_own_towers(c))
            + w5·min(clearance(c), 3)
     reject c if path passes within 8 m of an enemy melee
  return argmax s(c)  (None → last_stand, [11] §4.4)
```

## 9. Formasyon ve yığılmanın önlenmesi

- Rol halkaları: warrior'lar hedefin etrafında 8 yuvadan birini alır (AIServer kuşatma yuvası fikri, [`AIServer/AIUser.cpp:71-121`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/AIUser.cpp#L71-L121), bot katmanında yeniden uygulanır).
- Ayrışma vektörü: aynı party üyeleri arası < 1,5 m ise karşılıklı itme (MET-NAV-06).
- Priest'ler arası ≥ 8 m ([07](07_PRIEST_BEHAVIOR.md) §11).

## 10. Takılma tespiti ve aşamalı kurtarma

Tespit: hareket halindeyken 1,5 sn boyunca yol üzerindeki ilerleme < 1 m, ya da 4 sn içinde aynı iki hücre arasında ≥ 3 salınım.

| Aşama | Aksiyon | Süre sınırı |
|---|---|---|
| 1 | Mevcut konumdan yeniden planla | 0,5 sn |
| 2 | En yakın yüksek açıklıklı hücreye (clearance ≥ 2) yan adım | 1 sn |
| 3 | Bir önceki yol noktasına geri çekil | 1,5 sn |
| 4 | Takılınan kenara dinamik ceza (60 sn), yeniden planla | 1 sn |
| 5 | Hedefi bırak, `NAV_STUCK_ABANDON`; takıma bildir | — |
| T (yalnızca test modu) | `TEST_TELEPORT` ile en yakın güvenli hücreye. **Eval modunda maçı geçersiz kılar** (MET-NAV-05). Normal PK'da yoktur. | — |

Her aşama telemetride `NAV_RECOVERY` olarak kaydedilir; takılma noktaları ısı haritası olarak toplanır (14 §6 global deneyim).

## 11. Test senaryoları ve kabul kriterleri

| Test | Amaç |
|---|---|
| T-NAV-01 | Temel koşu hızı ve hareket paketi sıklığının gerçek istemciyle ölçülmesi |
| T-NAV-02 | Eğim kalibrasyonu: istemcinin tırmanamadığı eğimlerin işaretlenmesi |
| T-NAV-03 | 1000 rastgele A* sorgusu: başarı, süre, düğüm sayısı |
| T-NAV-04 | Dar geçit ve köprü noktalarında 50 geçiş: takılma oranı |
| T-NAV-05 | Respawn noktasından arenaya yürüyüş süresi (summon değerinin hesabı) |
| T-NAV-06 | Hareketli hedef takibi (kiting mage) |
| T-NAV-07 | Ulaşılamayan hedef (göl cebi, tower halkası) bırakma |
| T-NAV-08 | 8 kişilik party'nin hedef etrafında yığılmadan yerleşmesi |
| T-NAV-LOS-01 | İstemcinin engel arkasına skill kullanımına izin verip vermediği |

| Kimlik | Kriter |
|---|---|
| AC-NAV-01 | MET-NAV-01 takılma ≤ 2 / bot-saat; MET-NAV-02 p95 ≤ 5 sn |
| AC-NAV-02 | MET-PERF-03 A* p95 ≤ 2 ms (zone 71, ≤ 20 000 düğüm) |
| AC-NAV-03 | Engelli hücreye giren bot hareketi = 0 (sunucu tarafı denetim logu, T-NAV-04) |
| AC-NAV-04 | MET-NAV-04 ulaşılamayan hedefi bırakma ≤ 3 sn |
| AC-NAV-05 | Eval modunda `TEST_TELEPORT` = 0 |
| AC-NAV-06 | Karşı ulus tower halkasına giriş = 0 (takip dahil) |

## 12. Açık sorular

- İstemcinin tırmanma/eğim kuralı ve gerçek koşu hızı (T-NAV-01/02).
- Görüş hattı istemci davranışı (T-NAV-LOS-01).
- Çarpışma geometrisinin y ±5000 değerli poligonlarının sınır duvarı olup olmadığı (ızgara yaklaşımını etkilemez).

## 13. Değerlendirme düzeltmeleri (2026-10-02)

> Kaynak: `docs/reports/degerlendirme-2026-10-02.md` (DEG-13, 17–21). Bu bölüm **eklemedir**: §4/§6/§7/§10'daki varsayımları ölçümle düzeltir; F5 planlarının (F5-50..F5-55) kabul dayanağıdır. `gece/2026-10-02-nav` hattında F5-08..F5-10 yazılmadan önce okunmalıdır (özellikle §13.3 ve §13.4). Ölçümler WSL `g++ -O2` ile yapıldı, MSVC Release değildir `[V]`.

### 13.1 Hareket paketi, adım modeli ve kiriş denetimi (CLI-08)

- Gerçek istemci sürekli harekette `WIZ_MOVE`'u ~1,5 sn'de bir yollar ve paket **hedef noktayı** taşır (`docs/03` §13.2). Bot da aynısını yapar (`kMovePeriodMs = 1500`): iki paket arası yürüyüşte ~6,75 m, sprintte ~10 m tek adımdır. **Ara noktaları sunucu doğrulamaz** (MEC-MOV-03); 4 m ızgarada bir adım 2–3 hücre atlar.
- **Kural (CLI-08, `[Ö]`):** bot her hareket paketinden önce *kirişi* (önceki paket konumu → yeni konum) denetler: kirişin dokunduğu **tüm** hücreler (muhafazakâr süpercover; hücre köşesi/vertex'ine değme dahil) `Walk` olmalı ve kiriş boyunca `EdgeOpen` eğim kuralı sağlanmalı. Yalnızca varış noktasına bakmak yetmez. Reddedilen paket gönderilmez (`FAIRNESS_REJECT`, kural `CLI-08`, sebep `blocked_chord`).
- **Duvar bulgusunun sınıflandırması (yeniden üretilebilir: `tools/nav-measure.sh smoothing --n 6000`, bağımsız çapraz kontrol `tools/nav-segment-check.py`, zone 71, WSL `g++ -O2`, `gece/2026-10-02-nav` @ `196857d`) `[V]`:** ham A* yolu kenarları 0/379 054, `NavSmoothPath` segmentleri 0/33 365, 6,75 m paket kirişleri 0/250 000 engelli hücreye değiyor; `NavLineClear` "açık" dediği 360 288 çiftte yanlış-pozitif 0 (sentetik ızgaralar dahil). **Planlayıcısız düz hedef adımı** (`/bot move`/`BeginMove` gibi, 2–3 hücre uzaklıkta iki yürünebilir hücre arası) 6000 çiftin 447'sinde (%7,45) engelli hücreye değiyor. Yani bulgu ham yolda veya düzleştirmede değil, **icradaki yürünebilirlik denetiminin (CLI-08) eksikliğindedir** (F5-50 kalıcı denetim testleri, F5-55 sunucu guard'ı). Örnek vektörler `docs/reports/degerlendirme-2026-10-02-ek.md` §3 ve F5-50. Denetim planlayıcı çıktısı güvenli olsa da **icra tarafında zorunludur**: düz hedef adımı, planlayıcı dışı kaynaklar ve gelecekteki değişiklikler için tek koruma budur.
- **Su:** ayrı bir su katmanı yoktur. SMD olay ızgarası göl kıyılarını engelli işaretler, ana bileşen kuralı iç cepleri dışlar `[V]`/`[I]`; istemcinin suya girip girmediği ve suda yavaşlayıp yavaşlamadığı ölçülmedi `[A]` → T-NAV-09 (yeni). **Eğim:** `maxSlope 0,625` `[A]` (T-NAV-02). **Çapraz köşe:** iki ortogonal komşu da `Walk` olmalı (`EdgeOpen`).

### 13.2 Hareketli hedefin gözlemi ve hız kestirimi

Gözlenen hedef (insan veya bot) `WIZ_MOVE`'u ~1,5 sn'de bir gönderir. *(F5-52 pencereyi uyarladı; gözlem zaman damgası sözleşmesi — `tMs` = alıcıda işlenme anı, sunucu/gönderim zamanı değil —, değişken aralık, paket kaybı, eski veri, ani yön/hız değişimi ve sıçrama dayanıklılığı F5-56'da test edilir; pencereyi büyütmek tek başına çözüm değildir.)* F5-04'ün 1000 ms'lik hız penceresi bu sıklıkla **her zaman 0 hız** üretir (rapor §5.2: 1500/1540 ms aralıkta %100 sıfır). Kural `[Ö]`: hız, son iki gözlem arasındaki konum farkının süreye oranıdır (aralık 0,4–4,0 sn arasında); en yeni gözlem 4,0 sn'den eskiyse veya son paketin `speed` alanı 0 ise hız 0 (durmuş). `P-NAV-VEL-WINDOW` = 4000 ms, `P-NAV-VEL-MIN-SPAN` = 400 ms `[A]`. Öngörü süresi `min(1,5 sn, …)` aynı kalır; gözlem yaşı lead'e eklenir (gözlem ne kadar eskiyse hedef o kadar ileride).

### 13.3 Takılma tespiti tanımı (§10'u düzeltir)

§10'daki "1,5 sn'de ilerleme < 1 m" pencere paket aralığına eşittir (marj 0); "4 sn'de aynı iki hücre arasında ≥ 3 salınım" ölçütü ≤ 3 konum örneğiyle (paket başına bir) ulaşılamazdır (rapor §5.4). Yerel hareket ilerlemesi ile paket gönderimi **ayrı** izlenir:

| Kavram | Tanım |
|---|---|
| Niyet ilerlemesi | Tick hızında (100 ms) yerel simülasyon: botun yol üzerindeki kümülatif ilerlemesi, **gönderilmiş paketlere** göre hesaplanır; paket zamanı beklenirken ilerleme "bekliyor" sayılır, takılma değildir |
| Paket teyidi | Her paketten sonra botun kendi konumunun (sunucu) paket konumuna eşit olması |
| `STUCK` | Hareket niyeti etkin ve **ardışık ≥ 2 paket periyodu** (≥ 3,1 sn) boyunca yol üzerindeki ilerleme < 1 m |
| `BLOCKED_BY_GUARD` | Paket guard tarafından reddedildi (CLI-08/CLI-05); `STUCK` sayılmaz, ayrı sayılır |
| `OSCILLATION` | Son 8 sn'de ≥ 4 paket konumu ile A→B→A→B desen (≥ 3 yön değişimi) |

Hedefe varış adımı (< 1 m) takılma değildir. *(Hareket niyeti ve gerçek rota ilerlemesinin birlikte değerlendirilmesi — bekleme/guard-engeli/takılma ayrımı, U-dönüşünde pozitif rota ilerlemesi — F5-57'de; F5-09 varsayılanı tick'le beslenince gecikmeli tick modelinde 600 sn'de 6 yanlış epizod üretir, `NavPacketCadenceParams()` 0: `tools/nav-measure.sh stuck`.)* Yeniden planlama (500 ms) paket sıklığından bağımsızdır. Tespit saf mantık olarak `BotCore`'da yazılır (F5-54), kurtarma aşamaları (§10) onun üstüne F5-09'da.

### 13.4 Arena sınırı, ölüm, doğuş ve savaşa dönüş

- **Kural `[Ö]`:** arena sınırı yalnızca **arenanın içindeki** bot için "dışarı çıkış yasak"tır. Arena dışındaki bot (doğuş noktası, summon bekleme, dönüş yolu) sınırın içine girebilir ve dışarıda serbest yürür; yasaklı-hücre cezası dışarıda uygulanmaz. Bugünkü `AddForbidOutsideDisc` + `forbiddenPenalty = 10` bunu sağlamaz: ölçüm (rapor §5.3) El Morad doğuşu → arena için `NodeLimit` (20 000 düğüm, yol yok), Karus için 4148 düğüm; arena içinden dışarıdaki doğuş noktasına hedef `InvalidGoal`. Düzeltme planı F5-51.
- **Geri çekilme:** arena modunda güvenli nokta **arenanın içindedir** (party: arka hat; solo: arenanın kendi ulus tarafı). "Kendi tower halkasına çekil" (`docs/11` §4.3) yalnızca arena modu kapalıyken (serbest Ronark, F11) geçerlidir; arena modunda tower halkası arenanın 233 m (Karus) / 640 m (El Morad) dışındadır. ADR-0033-DEG.
  *Mod ayrımı (proje sahibi kararı 2026-10-02, ADR-0033-DEG):* bu madde yalnızca **arena modu** içindir; serbest Ronark modunda güvenli konuma çekilme, yeniden gruplanma ve savaşa dönüş ayrıca planlanır (`docs/17` F11-a/b/c).
- **Doğuş ve dönüş:** doğan bot arena dışındadır, dönüş yürüyerek (~52 sn Karus, ~142 sn El Morad, 4,5 m/s) veya summon'la olur; arenaya girdikten sonra "savaş alanında kal" kuralı başlar. Dönüş yolu planı (doğuş → arena kenarı) kısa ömürlü önbellekte tutulur (§13.5).

### 13.5 Çoklu bot yol bütçesi

Ölçüm (rapor §5.5, zone 71, tek iş parçacığı): 16 bot aynı tick'te near64 sorgusu → tick toplamı p95 2,83 ms (max 7,2); mid150 → p95 8,8 ms; tüm harita → p95 30 ms; 64 bot near64 → p95 9,5 ms. MET-PERF-02 hedefi tüm BotManager için 16 bot p95 ≤ 5 ms olduğundan nav için ayrı bütçe şarttır. Strateji `[Ö]` (F5-53):

1. **Tick bütçesi** `P-NAV-TICK-BUDGET-MS` = 1,5 ms (MET-PERF-02'nin ~%30'u): tick başına yürütülen sorgular bütçe dolunca durur; kalanlar sonraki tick'e kalır, bot mevcut yolu izlemeye devam eder.
2. **Kuyruk + adil sıra:** bekleyen sorgular FIFO; bot sırası her tick döndürülür; bir sorgu bölünmez (`P-NAV-MAX-NODES` zaten üst sınır). Bir bot en çok `P-NAV-MAX-WAIT` = 1 sn bekler (aşılırsa öncelik alır).
3. **Yeniden planlama fazı:** 500 ms'lik aralık bot başına kaydırılır (`slot % 5 × 100 ms`); hepsi aynı tick'e düşmez.
4. **Yol önbelleği:** aynı (başlangıç hücresi, hedef hücresi, maliyet alanı sürümü) sorgusu TTL 30 sn içinde yeniden hesaplanmaz (ör. ulus başına doğuş → arena kenarı rotası).
5. Kabul **AC-NAV-07 (yeni):** 16 bot, yoğun sorgu yükünde nav toplamı p95 ≤ `P-NAV-TICK-BUDGET-MS` ve hiçbir bot `P-NAV-MAX-WAIT`'ten uzun yol beklemez; ölçüm MSVC Release'te tekrarlanır (MET-PERF-03).

### 13.6 Gerçek haritada doğrulama kapısı (G5)

F5 yalnızca saf mantık (`BotCore`) olarak kapanamaz. F5 kabulü için: sunucu entegrasyonu (F5-55: `NavService`, kiriş denetimi, `/bot goto`, `NAV_*` telemetrisi) ve oyun içi T-NAV-04/05/09 ile AC-NAV-01..07 çalışma zamanı kanıtı gerekir (`docs/17` §5 G5).

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
