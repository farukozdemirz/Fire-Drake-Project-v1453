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
| Hareket paketi | `WIZ_MOVE` gerçek istemci sıklığında (T-MECH-CLIENT-04 ile ölçülecek; ölçülene kadar 250 ms) | MEC-MOV-01 |
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

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
| 2026-10-02 | v1.0+ | §4.1 ve §11 T-NAV-03: `NodeLimit` anlamı ve sorgu dağılımı notu (ADR-0006, F5-02 planı) |
| 2026-10-02 | v1.0+ | §4.1 yol düzleştirme: `EdgeOpen` tabanlı Bresenham görünürlüğü, açgözlü ayıklama ve `P-NAV-SMOOTH-LOOKAHEAD` `[A]` (ADR-0006 Eki F5-03, F5-03 planı) |
| 2026-10-02 | v1.0+ | §4.2 hareketli hedef: gözlem/planlama ayrımı, hız kestirimi penceresi, öngörü geri çekilmesi, menzil halkası tanımı (ADR-0006 Eki F5-04, F5-04 planı) |
| 2026-10-02 | v1.0+ | §4.3 ulaşılamaz hedef: bileşen tabanlı kesin tespit, `Detour` kuralı, `NodeLimit` = bilinmiyor, 1,5 sn bırakma süresi `[A]` (ADR-0006 Eki F5-05, F5-05 planı) |
