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
| `danger_static` | 4 m | Karşı ulusun guard tower halkası (kapıya ≤ 90 m) **yasaklı** (hücre bayrağı, dışarıdan girilemez; `BotCore/NavDanger.h`, ADR-0006 Eki F5-06), kendi halkası **güvenli işaretli**; canavar spawn dikdörtgenleri + arama menzili (yol maliyeti; henüz yok). Takıma göre iki ayrı katman (Karus/El Morad). |
| `danger_dynamic` | 4 m | Görünür düşmanların etki haritası (melee: 15 m çekirdek, ağırlık 1,0; mage: 45 m menzil diski, ağırlık 0,6 `[A]`; 8 m doğrusal sönüm `[A]`; hücre başına 0–255, en büyük değer birleşimi), 500 ms'de bir statik katmanın kopyası üzerine yeniden kurulur |
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
- Maliyet: adım = mesafe × (1 + 0,5 × (ceza(a) + ceza(b))); ceza(c) = `w_danger`·danger(c) + `w_clear`·max(0, 2 − clearance(c)) + (yasaklı hücrede 10); `w_danger` = 4, `w_clear` = 0,5 `[A]`; ceza ≥ 0 olduğundan octile sezgisel tutarlı kalır. Yasaklı hücreye dışarıdan girilemez (içeriden çıkış serbest); hedef yasaklıysa `InvalidGoal`. Maliyet alanı isteğe bağlıdır (yokken yalnızca mesafe). Eğim cezası yoktur (sert eğim kesmesi `EdgeOpen`'da; T-NAV-02 sonrası). `BotCore/NavDanger.h`, ADR-0006 Eki F5-06, F5-06 planı.
- Sezgisel: octile mesafe (kabul edilebilir).
- İkili yığın (binary heap) açık liste, düğüm havuzu; düğüm limiti `P-NAV-MAX-NODES` = 20 000; aşılırsa hiyerarşik arama (bu dilimde yok: F5-02 yalnızca `NodeLimit` = "bilinmiyor" döndürür, "ulaşılamaz" değil; ADR-0006).
- Yol düzleştirme: hücre merkezleri arasında, ızgara üzerinde Bresenham yürüyüşü engelsizse (her adım `EdgeOpen`; kanonik yön, simetrik) ara noktalar atlanır; açgözlü, en çok `P-NAV-SMOOTH-LOOKAHEAD` = 64 yol hücresi ileri `[A]` (`BotCore/NavSmooth.h`, ADR-0006 Eki F5-03, F5-03 planı). Bresenham `supercover` değildir (bilinen sınırlama, takılma ölçümüyle yeniden değerlendirilir).
- AIServer `CPathFind`'ın sezgisel ve yürünebilirlik hataları (MB-11) bu uygulamaya **taşınmaz**.

### 4.2 Hareketli hedef

- Hedefin son 1 sn'lik hız vektöründen öngörü noktası: `p + v·min(1,5 sn, mesafe/kendi_hız)`.
- Yeniden planlama: hedef, **son planın yapıldığı konumdan** ≥ 6 m yer değiştirdiğinde veya son plandan 500 ms geçtiğinde (hangisi önce). Hız kestirimi en yeni gözlemden geriye 1 sn içindeki en eski örnekle yapılır; aralık < 100 ms ya da en yeni gözlem > 1 sn eskiyse hız 0. Öngörü noktası yürünebilir değilse süre yarıya indirilir (en çok 3 kez), olmazsa hedefin mevcut konumu kullanılır (`BotCore/NavTrack.h`, ADR-0006 Eki F5-04, F5-04 planı).
- Menzil hedefi: yol, hedefin etrafında rol menzili halkasındaki en yakın ulaşılabilir hücreye planlanır (warrior: melee halkası; mage: P-MAG-PREF-RANGE). Halka, öngörü noktasından `[ringMinM, ringMaxM]` metre (hücre merkezi uzaklığı); etkin üst sınır en az hücre köşegeninin yarısıdır (4 m ızgarada 2,83 m). "En yakın" = bota octile uzaklığa göre sıralı `Walk` adaylar; en çok 3 aday için A* denenir `[A]`.

### 4.3 Ulaşılamayan hedef

`unreachable` koşulları (`BotCore/NavReach.h`, ADR-0006 Eki F5-05, F5-05 planı): (1) hedefin rol halkasında botun bileşeniyle bağlı hiç `Walk` hücresi yok. Bileşen, `EdgeOpen` ile bağlı 8 komşulu hücre kümesidir (eğim cepleri dahil); iki hücre farklı bileşendeyse A* `NoPath` verir, bu A* çalıştırılmadan bilinir. Halkada hiç `Walk` hücresi yoksa da (hedef suda/duvarda/dış bantta) ulaşılamazdır. (2) Planlanan yolun A* maliyeti > 3 × düz mesafe **ve** > 120 m (`Detour`; düz mesafe bot hücre merkezi ile hedef hücre merkezi arası). A*'ın `NodeLimit` vermesi ya da ilk 3 halka adayının başarısız olması ulaşılamaz **değil**, "bilinmiyor"dur (yanlış "ulaşılamaz" geçerli hedefi bıraktırır). Ulaşılamaz yargısı kesintisiz `holdMs` = 1,5 sn `[A]` sürerse hedef `TARGET_UNREACHABLE` ile bırakılır: tespitten bırakmaya 1,5 sn (MET-NAV-04 ≤ 3 sn); başka bir yargı seriyi sıfırlar. Bırakma kararı ve olayı karar katmanındadır ([09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) §5.4); `BotCore` yalnızca yargıyı ve "vadesi geldi" bilgisini üretir.

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
| Karşı ulus tower halkası | Karşı ulus kapısına ≤ 90 m (tower'lar 25–50 m halkada, arama menzili 35 m) `[V]` | Girilmez (yol planlayıcısı dışarıdan yasaklı hücreye girmez, `NavDanger.h`); hedef bu bölgeye girerse takip biter (`InvalidGoal`; bırakma kararı karar katmanında); içeride kalan bot en kısa çıkışla çıkar |
| Kendi tower halkası | Kendi kapımıza ≤ 90 m | Geri çekilme ve solo RECOVER için güvenli bölge (hücre `Safe` bayrağı; `wSafe` bonusu, §8, F5-07) |
| Canavar alanları | Spawn dikdörtgeni + arama menzili | Yol maliyetini artırır; test arenasında bulunmaz ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md) §2); F5-06'da yok (ertelendi) |
| Arena sınırı (test modu) | Senaryo tanımı | Bot arena dışına yol planlamaz (arena dairesinin dışı yasaklı: `AddForbidOutsideDisc`) |

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

Uygulama (`BotCore/NavRetreat.h`, ADR-0006 Eki F5-07, F5-07 planı `[Ö]`/`[A]`):

- **Tek geçişli sel:** adaylar ve yol uzunlukları botun hücresinden tek bir sınırlı Dijkstra taramasıyla bulunur (geometrik uzunluk ≤ R, sınır dahil; `EdgeOpen` kenar kuralı). Seçilen adayın yolu taramanın ebeveyn zinciridir. "Ana bileşende" koşulu ayrıca denetlenmez: sel yalnızca botun bileşenine ulaşır.
- **Puan (normalleştirilmiş, ağırlıklar `[A]`):** `s = − wDanger·tehlike/255 − wPath·uzunluk/R + wAnchor·yakınlık + wClear·min(clearance, 3)/3 + wSafe·[Safe]` (`wDanger` 3, `wPath` 1, `wAnchor` 1,5, `wClear` 0,5, `wSafe` 1). `yakınlık = max(0, 1 − uzaklık/R)`; dayanak noktası (party: arka hat, solo: kendi kapısı) çağıran verir. `danger_static` ile `danger_dynamic` tek `NavCostLayer`'da birleşiktir (tek `wDanger`). `Safe` bayrağı (kendi tower halkası) `wSafe` bonusu verir.
- **8 m kuralı:** bir adım, hedef hücre bir melee'ye ≤ 8 m ise ve melee'ye uzaklığı azalıyorsa yasaktır (bölgeye girilmez; bölgenin içinde başlayan bot yalnızca uzaklaşarak çıkar ve düşmanın içinden geçemez). Bölgedeki hücreler aday olamaz. Yalnızca melee tehditleri sayılır.
- **Yasaklı bölge:** dışarıdan girilmez (F5-06 ile aynı kural), içeriden çıkış serbest; yasaklı hücre aday olamaz.
- **Sonuç:** `Found` / `NoCandidate` (hiç aday yok → `last_stand`) / `InvalidStart`. Seçilen hücre başlangıç hücresi olabilir. Rota üzerindeki tehlike puanlanmaz (yalnızca 8 m ve yasaklı kuralları); adayın tehlikesi çok yüksekse `last_stand` sayma kararı karar katmanındadır.

## 9. Formasyon ve yığılmanın önlenmesi

Uygulama (`BotCore/NavFormation.h`, ADR-0006 Eki F5-08, F5-08 planı `[Ö]`/`[A]`):

- **Kuşatma yuvaları:** hedefin etrafında 8 pusula yönünde (sıra: 0 = +z, saat yönünün tersine; AIServer kuşatma yuvası fikri, [`AIServer/AIUser.cpp:12-13`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/AIUser.cpp#L12-L13), [`:71-121`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/AIUser.cpp#L71-L121), bot katmanında yeniden uygulanır) çağıranın verdiği yarıçapta yuva noktası (testlerde 2,5 m `[A]`: komşu yuvalar 1,91 m). Yuva **kullanılabilir** = yuva noktasının hücresi `Walk`; kullanılamaz yuva verilmez, yerine taşıma (snap) yoktur. Zone 71'de 88 508 `Walk` hedef hücresinin 72 459'unda sekiz yuvanın hepsi kullanılabilir, en kötüsünde 5 `[V]`. Atama **yapışkan** (üye geçerli yuvasını korur) + **açgözlü en yakın çift** (eşitlikte küçük üye, sonra küçük yuva indeksi); fazla üye yuvasız (`-1`) kalır (ikinci halka/bekleme karar katmanının işidir).
- **Ayrışma vektörü (MET-NAV-06):** aynı party üyelerinden biri başka bir üyeye < 1,5 m ise karşılıklı itme: çift başına `0,5 · (1,5 − d)` (iki üye de uygularsa uzaklık tam 1,5 m), çakışık üyelerde indeks tabanlı belirlenimci yön, toplam 1,0 m'ye kırpılır `[A]`; uygulama duvara çarpmaz (tam, yalnızca-x, yalnızca-z sırasıyla dener). İtme bir yol değildir: hareket katmanı her tick uygular. Ham ölçü `NavCountStackedPairs` (< 1 m çift sayısı; "2 sn'den uzun" süre kuralı telemetri katmanındadır).
- **Priest aralığı:** iki priest birbirinden ≥ 8 m ([07](07_PRIEST_BEHAVIOR.md) §11) kuvvet değil **seçimdir**: aday hücreler arasından diğer priest'lere ≥ 8 m (sınır dahil) olan ilk aday; yoksa en yakın priest'e en uzak aday (`NavPickSpaced`).
- **T-NAV-08 `[V]`:** 8 üye 0,6 m içinde yığılı başlar, hedef etrafında yerleşir: tick 3'ten sonra < 1 m çift yok, yerleşmiş formasyonda en küçük çift uzaklığı 1,91 m (düz ızgara); duvar yanı hedefte 6 yuva atanır, yuvasız iki üye birbirinden 1,5 m'ye ayrışır (zone 71).

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

Uygulama (`BotCore/NavStuck.h`, ADR-0006 Eki F5-09, F5-09 planı `[Ö]`/`[A]`; telemetri, ısı haritası ve `NavFollower` bağlaması bu dilimde yok):

- **Tespit:** `NavStuckDetector`, zaman damgalı konum örneklerinden (en çok 256, ≥ 20 ms aralıklı) iki kural: *ilerlemesizlik* = hareket halindeyken 1,5 sn'de **net yer değiştirme** < 1 m (kesin `<`; yol ilerlemesi ölçüsü bağlama planında, `[A]`) ve *salınım* = 4 sn'de aynı iki hücre arasında (sırasız çift) ≥ 3 geçiş; `NoProgress` önce denetlenir. `moving = false` pencereleri sıfırlar.
- **Merdiven:** `NavStuckMonitor`, tespitte aşama 1'den başlayarak her aşama girişinde **bir kez** eylem döndürür (`Replan`, `SideStep`, `StepBack`, `PenalizeReplan`, `Abandon`); süre sınırları 0,5 / 1 / 1,5 / 1 sn `[O]` dolunca bir üst aşama (tespitten bırakmaya 4 sn). **Başarı** = aşama girişindeki konumdan ≥ 1 m yer değiştirme `[A]` (`recovered`, `recoverMs` = tespitten kurtarmaya, MET-NAV-02); kurtarmadan sonra 10 sn içinde yeni tespit merdivenin **bir sonraki aşamasından** başlar (4'ten sonra doğrudan bırak), aksi hâlde aşama 1 `[A]`. Eylemi yürütmek, "bir önceki yol noktası"nı bilmek ve hedefi bırakmak çağıranın işidir. Salınım bölümünde ilk sıçrama "kurtarıldı" sayılır (iyimser; `kind` ile ayrıştırılır), merdiven yine bırakmayla biter.
- **Aşama 2 (yan adım):** `NavPickSideStep` = `NavRingCells`'ten (12 m `[A]`) ilk `Walk`, açıklık ≥ 2, `NavLineClear` ve yürüme eksenine ±45° içinde olmayan hücre; koridorda (açıklık 1) yoktur.
- **Aşama 4 (ceza):** `NavStuckPenalties` (kapasite 32) hücre cezasını 60 sn tutar ve `NavCostLayer`'a tehlike 255 olarak işler (§4.1 maliyetinde adım ≈ 3 ×); yolu bloklamaz (tek hücrelik koridorda maliyet 180 → 196).
- **T-NAV-04 (birim düzeyi):** gizli tek hücrelik engel önünde sanal bot aşama 2–4 kurtarmalarıyla 27,3 sn'de varır (engelsiz 21,0 sn), üç hücrelik duvarda 17,0 sn'de bırakır `[V]`.

## 11. Test senaryoları ve kabul kriterleri

| Test | Amaç |
|---|---|
| T-NAV-01 | Temel koşu hızı ve hareket paketi sıklığının gerçek istemciyle ölçülmesi |
| T-NAV-02 | Eğim kalibrasyonu: istemcinin tırmanamadığı eğimlerin işaretlenmesi |
| T-NAV-03 | 1000 rastgele A* sorgusu: başarı, süre, düğüm sayısı. Sorgu dağılımı `[Ö]` ([ADR-0006](adr/ADR-0006-navigasyon-izgara-astar.md) madde 4): kapı kümesi Chebyshev ≤ 64 hücre (256 m); ≤ 150 hücre ve tüm harita kümeleri raporlanır. F5-02'de birim/performans testi olarak gerçeklenir |
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
- **Duvar bulgusunun sınıflandırması (yeniden üretilebilir: `tools/nav-measure.sh smoothing --n 6000`, bağımsız çapraz kontrol `tools/nav-segment-check.py`, zone 71, WSL `g++ -O2`, `gece/2026-10-02-nav` @ `196857d`) `[V]`:** ham A* yolu kenarları 0/379 054, `NavSmoothPath` segmentleri 0/33 365, 6,75 m paket kirişleri 0/250 000 engelli hücreye değiyor; `NavLineClear` "açık" dediği 360 288 çiftte yanlış-pozitif 0 (sentetik ızgaralar dahil). **Planlayıcısız düz hedef adımı** (`/bot move`/`BeginMove` gibi, 2–3 hücre uzaklıkta iki yürünebilir hücre arası) 6000 çiftin 447'sinde (%7,45) engelli hücreye değiyor. Yani bulgu ham yolda veya düzleştirmede değil, **icradaki yürünebilirlik denetiminin (CLI-08) eksikliğindedir** (F5-50 kiriş denetimi, F5-58 kalıcı regresyon testleri, F5-55 sunucu guard'ı). Örnek vektörler `docs/reports/degerlendirme-2026-10-02-ek.md` §3 ve F5-50. Denetim planlayıcı çıktısı güvenli olsa da **icra tarafında zorunludur**: düz hedef adımı, planlayıcı dışı kaynaklar ve gelecekteki değişiklikler için tek koruma budur.
- Planlayıcı çıktısı **Walk** denetiminden geçiyor: 998 near64 yolunun 4887 düzleştirilmiş segmenti ve 33 503 paket kirişinde ihlal 0 (rapor §5.1). Bu ölçüm yalnızca `Walk` süpercover'ını kapsıyordu; eğim ölçülmemişti. F5-50 Tur 1 ölçümü `[V: WSL g++ -O2, zone 71]`: kirişin süpercover hücre çiftlerinde `EdgeOpen` eğim kuralı planlayıcı segmentlerinin ~%11,7'sini reddeder (hücre başına yükseklik gürültüsü), uç hücrelerden türeyen Bresenham eğim denetimi ise planlayıcının 6,75 m kirişlerinin %1,9'unu (691/35878) reddeder. Bu yüzden kiriş denetiminin **zorunlu** kuralı `Walk` süpercover'ıdır (AC-NAV-03); eğim katmanı isteğe bağlıdır ve varsayılan kapalıdır (yalnızca tam planlayıcı segmentleri için planlayıcıyla tutarlı). Yukarıdaki kural cümlesindeki "`EdgeOpen` eğim kuralı sağlanmalı" bu karara göre okunur `[Ö]`. Denetim buna rağmen **icra tarafında zorunludur**: düz hedef adımı (`/bot move`), planlayıcı dışı kaynaklar ve gelecekteki değişiklikler için tek koruma budur.
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
| 2026-10-02 | v1.0+ | §4.1 ve §11 T-NAV-03: `NodeLimit` anlamı ve sorgu dağılımı notu (ADR-0006, F5-02 planı) |
| 2026-10-02 | v1.0+ | §4.1 yol düzleştirme: `EdgeOpen` tabanlı Bresenham görünürlüğü, açgözlü ayıklama ve `P-NAV-SMOOTH-LOOKAHEAD` `[A]` (ADR-0006 Eki F5-03, F5-03 planı) |
| 2026-10-02 | v1.0+ | §4.2 hareketli hedef: gözlem/planlama ayrımı, hız kestirimi penceresi, öngörü geri çekilmesi, menzil halkası tanımı (ADR-0006 Eki F5-04, F5-04 planı) |
| 2026-10-02 | v1.0+ | §4.3 ulaşılamaz hedef: bileşen tabanlı kesin tespit, `Detour` kuralı, `NodeLimit` = bilinmiyor, 1,5 sn bırakma süresi `[A]` (ADR-0006 Eki F5-05, F5-05 planı) |
| 2026-10-02 | v1.0+ | §2 `danger_*`, §4.1 maliyet formülü, §7 güvenlik bölgeleri: hücre cezası modeli, yasaklı (sert, içeriden çıkış serbest) ve güvenli bayrağı, bant ilkeli tehlike, ağırlıklar `[A]` (ADR-0006 Eki F5-06, F5-06 planı) |
| 2026-10-02 | v1.0+ | §8 güvenli geri çekilme noktası: tek geçişli sel, normalleştirilmiş puan ve ağırlıklar `[A]`, 8 m melee kuralının kesin biçimi (yaklaşmayan adım), yasaklı kuralı, `Safe` bonusu, `NoCandidate` = `last_stand` sinyali (ADR-0006 Eki F5-07, F5-07 planı) |
| 2026-10-02 | v1.0+ | §9 formasyon ve yığılma: 8 pusula kuşatma yuvası (kullanılabilir = yuva hücresi `Walk`), yapışkan + açgözlü atama, ayrışma vektörü formülü ve kırpma `[A]`, priest ≥ 8 m seçim olarak, MET-NAV-06 ham ölçü (ADR-0006 Eki F5-08, F5-08 planı) |
| 2026-10-02 | v1.0+ | §10 takılma tespiti ve kurtarma: net yer değiştirme tabanlı ilerlemesizlik, hücre geçişi salınımı, merdiven durum makinesi (başarı ölçütü, tırmanma belleği `[A]`), yan adım ve hücre düzeyinde 60 sn ceza (ADR-0006 Eki F5-09, F5-09 planı) |
