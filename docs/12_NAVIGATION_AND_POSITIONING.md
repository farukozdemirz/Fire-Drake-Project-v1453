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
| **"Bowl" (kase):** harita merkezi ≈ (1024, 1024), yarıçap ~150 m. Canavar spawn'larının ~%25'i burada (705 canavardan 177'si; 100 m içinde 128) ve türler güçlü (undying, Death knight, Dark eyes, Baron, Cardinal, Harunga, Riote, Atross). İki ırkın savaşacak insan/takım ararken dönüp dolaştığı alan; canavarlar yürüyen oyuncuya saldırır; çok sayıda takılma noktası (engel) var | `[V]` yoğunluk, `[A]` oyuncu davranışı ve engel yoğunluğu | `K_NPCPOS` zone 71 (2026-10-03); proje sahibi gözlemi; T-NAV-12 ile doğrulanacak. Kontrollü arena A (merkeze ~284 m) bilinçli olarak bowl dışındadır; serbest Ronark (F11) bowl'u içerir |
| Göller: iç kısımlar ana alandan kopuk yürünebilir cepler `[V]`; "kıyılar engelli" `[A]` (F5-60: yükseklik-vekil havzaların kıyısı çoğunlukla engelli değil, su olduklarına dair zemin gerçeği yok; bkz. §13.1 "Su") | `[V]`/`[A]` | Bileşen görseli + yükseklik görseli; F5-60 ölçümü |
| Bu harita resmî 2005 Colony Zone haritasıyla örtüşüyor | `[S]` | [19](19_SOURCES_AND_EVIDENCE.md) W-10 |

**Sonuç:** Botun yürünebilirlik kuralını kendisinin uygulaması zorunludur (CLI-08). Aksi halde sunucu duvardan geçen hareketi kabul eder; insan oyuncu bunu yapamaz ve bu bir "gizli avantaj" olur.

## 2. Navigasyon veri katmanları `[Ö]`

GameServer başlangıcında zone 71 için bir kez hesaplanır. `C3DMap`, `SMDFile`'ın friend sınıfıdır ([`shared/SMDFile.h:69-70`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SMDFile.h#L69-L70)); yükseklik ve olay ızgarasına kopyalamadan erişilebilir. **Sunucu yolu (F5-59, `[V]` çalışma zamanı 2026-10-03):** `[BOT] ENABLED=1` ve `NAV=1` iken `NavService::Startup()` (`GameServerDlg.cpp`, `MapFileLoad()` ve `Logs` sonrası, `RunServer()` öncesi) zone 71 olay ve yükseklik dizilerini `SMDFile`'dan bir kez `BotCore::NavGrid`'e kopyalar (`GetMapSize() + 1` = 513), `Build()` eder ve `Ready()` ile sunar; `.navgrid` dosyası sunucuda **okunmaz**. Doğruluk `BotCore/NavFingerprint.h` CRC32 ile kanıtlandı: sunucu günlüğündeki `crc32=4fd154bc` = `tools/nav-export.py` dosyası, `main_cells=88508`, `copy_ms=0.3`, `build_ms=7.6` (Release).

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
Uygulama (`BotCore/NavLos.h`, ADR-0006 Eki F5-10, F5-10 planı `[Ö]`/`[A]`; yalnızca `los_grid` + arazi, `los_mesh` yok; bağlama ve T-NAV-LOS-01 ölçümü bu dilimde yok):

- **Hücre kuralı (`NavLosGridClear`):** ışının açık iç kısmını kestiği her ara hücre `Event == 1` olmalı; başlangıç ve bitiş hücreleri muaf; ızgara dışı engelli; köşeye değmek engel değil; hücre sınırına yatan ışın büyük taraftaki hücreye ait. `Walk` değil `Event` kullanılır (göl/eğim cepleri görüşü kapatmaz).
- **Arazi kuralı (`NavLosTerrainClear`):** göz = zemin + 1,6 m `[A]` her iki uçta; 2 m aralıkla örneklenen zemin ışının 0,25 m `[A]` üstüne çıkarsa engel.
- **Mod:** `NavLosMode::Advisory` (varsayılan) aksiyonu engellemez; `Enforce` yalnızca görüş açıkken izin verir (`NavLosAllows`). `NavPickLosCell`: hedef halkasındaki bota en yakın görüşlü `Walk` hücre (priest heal / mage cast konumu).
- **Ölçüm `[V]`:** zone 71'de rastgele `Walk` çiftlerinde görüş açık oranı 20 m'de %89,5, 40 m'de %69,1 (ofset (7,7)), 72 m'de %38,9 (arazi dahil); çağrı ≈ 0,4 µs. Sınırlamalar: engelli hücre sonsuz yüksek, göl kıyıları görüşü kapatır (yanlış negatif), hedef yüksekliği yok.

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
| AC-NAV-02 | MET-PERF-03 A* p95 ≤ 2 ms (zone 71, ≤ 20 000 düğüm; **kapı kümesi `near64`, MSVC Release**, ADR-0006 madde 4; ≤ 150 hücre ve tüm harita yalnızca raporlanır, `NavPath_Perf_T_NAV_03`) |
| AC-NAV-03 | Engelli hücreye giren bot hareketi = 0 (sunucu tarafı denetim logu, T-NAV-04) |
| AC-NAV-04 | MET-NAV-04 ulaşılamayan hedefi bırakma ≤ 3 sn |
| AC-NAV-05 | Eval modunda `TEST_TELEPORT` = 0 |
| AC-NAV-06 | Karşı ulus tower halkasına giriş = 0 (takip dahil) |

## 12. Açık sorular

- İstemcinin tırmanma/eğim kuralı ve gerçek koşu hızı (T-NAV-01/02).
- Görüş hattı istemci davranışı (T-NAV-LOS-01).
- Çarpışma geometrisinin y ±5000 değerli poligonlarının sınır duvarı olup olmadığı (ızgara yaklaşımını etkilemez).

## 13. Değerlendirme düzeltmeleri (2026-10-02)

> Kaynak: `docs/reports/degerlendirme-2026-10-02.md` (DEG-13, 17–21). Bu bölüm **eklemedir**: §4/§6/§7/§10'daki varsayımları ölçümle düzeltir; F5 planlarının (F5-50..F5-55) kabul dayanağıdır. `gece/2026-10-02-nav` hattında F5-08..F5-10 yazılmadan önce okunmalıdır (özellikle §13.3 ve §13.4). Bu bölümdeki **süre** sayıları WSL `g++ -O2` ile (ORT-G) yapıldı, MSVC Release değildir `[V]`; MSVC Release (ORT-M) sayıları ve hangi sayının neye geçerli olduğu §13.5.1-13.5.2'dedir.

### 13.1 Hareket paketi, adım modeli ve kiriş denetimi (CLI-08)

- Gerçek istemci sürekli harekette `WIZ_MOVE`'u ~1,5 sn'de bir yollar ve paket **hedef noktayı** taşır (`docs/03` §13.2). Bot da aynısını yapar (`kMovePeriodMs = 1500`): iki paket arası yürüyüşte ~6,75 m, sprintte ~10 m tek adımdır. **Ara noktaları sunucu doğrulamaz** (MEC-MOV-03); 4 m ızgarada bir adım 2–3 hücre atlar.
- **Kural (CLI-08, `[Ö]`):** bot her hareket paketinden önce *kirişi* (önceki paket konumu → yeni konum) denetler: kirişin dokunduğu **tüm** hücreler (muhafazakâr süpercover; hücre köşesi/vertex'ine değme dahil) `Walk` olmalı ve kiriş boyunca `EdgeOpen` eğim kuralı sağlanmalı. Yalnızca varış noktasına bakmak yetmez. Reddedilen paket gönderilmez (`FAIRNESS_REJECT`, kural `CLI-08`, sebep `blocked_chord`).
- **Duvar bulgusunun sınıflandırması (yeniden üretilebilir: `tools/nav-measure.sh smoothing --n 6000`, bağımsız çapraz kontrol `tools/nav-segment-check.py`, zone 71, WSL `g++ -O2`, `gece/2026-10-02-nav` @ `196857d`) `[V]`:** ham A* yolu kenarları 0/379 054, `NavSmoothPath` segmentleri 0/33 365, 6,75 m paket kirişleri 0/250 000 engelli hücreye değiyor; `NavLineClear` "açık" dediği 360 288 çiftte yanlış-pozitif 0 (sentetik ızgaralar dahil). **Planlayıcısız düz hedef adımı** (`/bot move`/`BeginMove` gibi, 2–3 hücre uzaklıkta iki yürünebilir hücre arası) 6000 çiftin 447'sinde (%7,45) engelli hücreye değiyor. Yani bulgu ham yolda veya düzleştirmede değil, **icradaki yürünebilirlik denetiminin (CLI-08) eksikliğindedir** (F5-50 kiriş denetimi, F5-58 kalıcı regresyon testleri, F5-55 sunucu guard'ı). Örnek vektörler `docs/reports/degerlendirme-2026-10-02-ek.md` §3 ve F5-50. Denetim planlayıcı çıktısı güvenli olsa da **icra tarafında zorunludur**: düz hedef adımı, planlayıcı dışı kaynaklar ve gelecekteki değişiklikler için tek koruma budur.
- Planlayıcı çıktısı **Walk** denetiminden geçiyor: 998 near64 yolunun 4887 düzleştirilmiş segmenti ve 33 503 paket kirişinde ihlal 0 (rapor §5.1). Bu ölçüm yalnızca `Walk` süpercover'ını kapsıyordu; eğim ölçülmemişti. F5-50 Tur 1 ölçümü `[V: WSL g++ -O2, zone 71]`: kirişin süpercover hücre çiftlerinde `EdgeOpen` eğim kuralı planlayıcı segmentlerinin ~%11,7'sini reddeder (hücre başına yükseklik gürültüsü), uç hücrelerden türeyen Bresenham eğim denetimi ise planlayıcının 6,75 m kirişlerinin %1,9'unu (691/35878) reddeder. Bu yüzden kiriş denetiminin **zorunlu** kuralı `Walk` süpercover'ıdır (AC-NAV-03); eğim katmanı isteğe bağlıdır ve varsayılan kapalıdır (yalnızca tam planlayıcı segmentleri için planlayıcıyla tutarlı). Yukarıdaki kural cümlesindeki "`EdgeOpen` eğim kuralı sağlanmalı" bu karara göre okunur `[Ö]`. Denetim buna rağmen **icra tarafında zorunludur**: düz hedef adımı (`/bot move`), planlayıcı dışı kaynaklar ve gelecekteki değişiklikler için tek koruma budur.
- **Su:** ayrı bir su katmanı yoktur; sunucu ve ortak kodda su/yüzme kavramı yoktur `[V: kod]`, SMD biçiminde su alanı yoktur. Ana bileşen kuralı iç cepleri dışlar (633 cep, 28 501 hücre, ana alana 8-komşu yalnızca 37 hücre) `[V]`. **F5-60 ölçümü (2026-10-03, `plans/F5-60`, `nav_measure water`, Python `basins` ile çapraz doğrulandı) `[V]`:** `Walk` hücrelerinin 3 830'u < −1 m; bunlardan ≥ 200 hücrelik 5 yükseklik-vekil "havza" (3 739 hücre; x 404-720/z 692-820, x 424-512/z 1096-1220, x 812-856/z 884-1064, x 1164-1236/z 940-1112, x 1268-1636/z 1164-1376; `water_t=-1,0`). Havza hücrelerinin yalnızca %9-34'ü bir olay-0 hücresine komşudur: **havzalar su ise veri onları engellemiyor**; "SMD olay ızgarası göl kıyılarını engelli işaretler" ifadesi doğrulanamadı `[A]`. 4 980 bulunan yolun %27,7'si (1 377) havzalara en az bir hücreyle giriyor, düzleştirilmiş segmentlerin %9,4'ü ve 6,75 m kirişlerin %7,7'si değiyor; Karus ve El Morad doğuş→arena rotaları değmiyor. **Zemin gerçeği yok:** istemci `freezone_a.gtd` biçimi çözülemedi (127 × 260 bayt `dtex\*.gtt` tablosu bulundu; sonrası 44 016 bayt, hücre-başına doku/su ızgarası kanıtlanamadı); havzaların su mu vadi mi olduğu bilinmiyor. **Karar: `BELİRSİZ`** ("suya takılmıyor" kabulü verilmez; F5-67 `TASLAK` kalır); istemci davranışı (suya girilebiliyor mu, suda yavaşlama, kıyıda görünmez duvar) `[A]` → T-NAV-09 (proje sahibi insan testi; `docs/STATUS.md` "Proje sahibi testleri (bekleyen)"). **Eğim:** `maxSlope 0,625` `[A]` (T-NAV-02). **Çapraz köşe:** iki ortogonal komşu da `Walk` olmalı (`EdgeOpen`). **T-NAV-09 SONUCU (2026-10-03, proje sahibi, insan istemcisi, paket izleyici `plans/_logs/trace/karus_su`):** çukur 3 (en derin nokta (824, 952), ekran görüntüsünde **görünür su yüzeyi**, `y` en düşük −5,6 m) ve çukur 4 ((1168, 1108), `y` −2,4 m) **gerçek su**; istemci içine **girilip çıkılabiliyor**, görünmez duvar yok. Suda hız **düşmüyor** (hız alanı 67, medyan 6,91 m/s, karada 6,78 m/s). Çukurlar `Walk` hücreleriyle uyumlu; su yürünebilir sayılır, sert engel değildir. Proje sahibi gözlemi: "sadece gereksiz bir rota" (insan oradan gitmez, ama geçilebilir). **Karar: ayrı su katmanı gerekmez, F5-67 İPTAL** `[V]`.

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

Hedefe varış adımı (< 1 m) takılma değildir. *(Hareket niyeti ve gerçek rota ilerlemesinin birlikte değerlendirilmesi F5-57'de `NavProgressAssessor` ile yapıldı `[V]` (sentetik): bekleme (`AwaitingPacket`), guard-engeli (`BlockedByGuard`) ve takılma (`Stalled`) ayrılır; `Stalled` penceresi `2 × 1550 + 100` = 3200 ms `[A]` (tablodaki "≥ 3,1 sn" ile aynı mertebe); U-dönüşünde rota ilerlemesi pozitifse takılma değildir. F5-09 varsayılanı tick'le beslenince gecikmeli tick modelinde 600 sn'de 6 yanlış epizod üretir, `NavPacketCadenceParams()` ve `NavProgressAssessor` 0: `tools/nav-measure.sh stuck` / `progress`. **Çağıran sözleşmesi (F5-55):** her yeni rotada `NotifyReplan` çağrılır (çağrılmazsa değerlendirici paketler arası Öklid yer değiştirmesine düşer); `NavStuckMonitor::Update`'e `moving = (Progressing || Stalled)` verilir, `Idle`/`AwaitingPacket`/`BlockedByGuard` iken `false`; niyet açıkken uzun süre paket yoksa karar `AwaitingPacket` kalır, takılma sayılmaz (karar katmanı/`ActionExecutor` işidir).)* Yeniden planlama (500 ms) paket sıklığından bağımsızdır. Tespit saf mantık olarak `BotCore`'da yazılır (F5-54), kurtarma aşamaları (§10) onun üstüne F5-09'da.

### 13.4 Arena sınırı, ölüm, doğuş ve savaşa dönüş

- **Kural `[Ö]`:** arena sınırı yalnızca **arenanın içindeki** bot için "dışarı çıkış yasak"tır. Arena dışındaki bot (doğuş noktası, summon bekleme, dönüş yolu) sınırın içine girebilir ve dışarıda serbest yürür; yasaklı-hücre cezası dışarıda uygulanmaz. Bugünkü `AddForbidOutsideDisc` + `forbiddenPenalty = 10` bunu sağlamaz: ölçüm (rapor §5.3) El Morad doğuşu → arena için `NodeLimit` (20 000 düğüm, yol yok), Karus için 4148 düğüm; arena içinden dışarıdaki doğuş noktasına hedef `InvalidGoal`. Düzeltme planı F5-51.
- **Geri çekilme:** arena modunda güvenli nokta **arenanın içindedir** (party: arka hat; solo: arenanın kendi ulus tarafı). "Kendi tower halkasına çekil" (`docs/11` §4.3) yalnızca arena modu kapalıyken (serbest Ronark, F11) geçerlidir; arena modunda tower halkası arenanın 233 m (Karus) / 640 m (El Morad) dışındadır. ADR-0033-DEG.
  *Mod ayrımı (proje sahibi kararı 2026-10-02, ADR-0033-DEG):* bu madde yalnızca **arena modu** içindir; serbest Ronark modunda güvenli konuma çekilme, yeniden gruplanma ve savaşa dönüş ayrıca planlanır (`docs/17` F11-a/b/c).
- **Doğuş ve dönüş:** doğan bot arena dışındadır, dönüş yürüyerek (~52 sn Karus, ~142 sn El Morad, 4,5 m/s) veya summon'la olur; arenaya girdikten sonra "savaş alanında kal" kuralı başlar. Dönüş yolu planı (doğuş → arena kenarı) kısa ömürlü önbellekte tutulur (§13.5).

### 13.5 Çoklu bot yol bütçesi

Ölçüm (rapor §5.5, **WSL `g++ -O2`, ORT-G; yalnız `NavPathfinder::Find`; her bot her tick bir sorgu, kuyruksuz üst yük; MSVC değil, gerçek `Tick()` değil** `[V: ORT-G, Y2]`, zone 71, tek iş parçacığı): 16 bot aynı tick'te near64 sorgusu → tick toplamı p95 2,83 ms (max 7,2); mid150 → p95 8,8 ms; tüm harita → p95 30 ms; 64 bot near64 → p95 9,5 ms. Bu sayılar bütçe **gerekçesidir**, kabul kanıtı değildir; MSVC Release sayıları §13.5.1'dedir (aynı sorgu için MSVC ≈ ×1,4-1,6 yavaştır). MET-PERF-02 hedefi tüm BotManager için 16 bot p95 ≤ 5 ms olduğundan nav için ayrı bütçe şarttır. Strateji `[Ö]` (F5-53):

1. **Tick bütçesi** `P-NAV-TICK-BUDGET-MS` = 1,5 ms (MET-PERF-02'nin ~%30'u): tick başına yürütülen sorgular bütçe dolunca durur; kalanlar sonraki tick'e kalır, bot mevcut yolu izlemeye devam eder. Bütçe **yumuşaktır**: tek sorgu bölünmez ve ilerleme garantisi için bütçeyi aşan tek sorgu yine çalışır; bu yüzden tick toplamı p99 bütçenin üstüne çıkabilir (MSVC Release birim testi: p99 4,0-4,2 ms). Hedef yalnız p95'tir.
2. **Kuyruk + adil sıra:** bekleyen sorgular FIFO; bot sırası her tick döndürülür; bir sorgu bölünmez (`P-NAV-MAX-NODES` zaten üst sınır). Bir bot en çok `P-NAV-MAX-WAIT` = 1 sn bekler (aşılırsa öncelik alır).
3. **Yeniden planlama fazı:** 500 ms'lik aralık bot başına kaydırılır (`slot % 5 × 100 ms`); hepsi aynı tick'e düşmez.
4. **Yol önbelleği:** aynı (başlangıç hücresi, hedef hücresi, maliyet alanı sürümü) sorgusu TTL 30 sn içinde yeniden hesaplanmaz (ör. ulus başına doğuş → arena kenarı rotası).
5. Kabul **AC-NAV-07 (yeni):** 16 bot, yoğun sorgu yükünde **`Tick()` içindeki nav payı** p95 ≤ `P-NAV-TICK-BUDGET-MS` (1,5 ms) ve hiçbir bot `P-NAV-MAX-WAIT`'ten (1 sn; tick kuantizasyonu ile ölçümde ≤ 1,1 sn) uzun yol beklemez. İki katman: (a) birim düzeyi, MSVC Release `NavBudget_RealMap_Load` (sentetik 16 sanal bot; test kapısı p95 ≤ 2,0 / p99 ≤ 4,5 / bekleme ≤ 1100; en kötü ölçülen p95 1,451), (b) oyun içi T-NAV-11: `PERF_SAMPLE.nav_us_p95 ≤ 1500` (§13.5.3). AC-NAV-07 yalnız (b) ile kapanır. Ölçüm atfı MET-PERF-**02**'nin nav payıdır (önceki sürümdeki "MET-PERF-03" atfı yanlıştı: MET-PERF-03 tek arama maliyetidir).

#### 13.5.1 Ölçüm matrisi: hangi sayı hangi derleyicide, hangi yükle, neyi ölçüyor

Bu alt bölüm, §13.5'teki ve raporlardaki süre sayılarının **tek referansıdır**. Aynı büyüklük için farklı sayıların çıkması çelişki değil, **farklı ölçüm tanımıdır**; hangi sayının hangi iddiaya dayanak olduğu §13.5.2'dedir.

**Ortamlar** (ölçüm satırlarındaki `ORT-*` kodları):

| Kod | Derleyici ve sürüm | Bayraklar | Makine ve ortam | Not |
|---|---|---|---|---|
| ORT-G | WSL `g++`, **x86-64** (32-bit değil). Sürüm raporlarda **yazılmamış**; bugünkü WSL: `g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0` `[V: 2026-10-03 tarihli `g++ --version` çıktısı]`, 10-02/03 koşularında aynı olduğu doğrulanamadı | `-std=c++17 -O2`, `-march` yok, `-I<kaynak ağaç>` (`tools/nav-measure.sh:33`) | WSL2, aynı ana makine (AMD Ryzen 7 7800X3D, `nproc` = 16, ~31 GiB RAM `[V: bugünkü WSL /proc/cpuinfo ve /proc/meminfo]`). Değerlendirme raporları CPU'yu yazmıyor (yalnızca "ana makine", "WSL"); aynı makine olduğu `[Ç]`. Paralel otonom döngüler koşarken ölçüldüğü için gürültülüdür (`plans/F5-53...md:117` "paylaşımlı makine") | Yalnızca `BotCore/Nav*.h` (saf standart kütüphane); sunucuyla ilgisi yok |
| ORT-M | MSVC **v143** araç seti. `tools/build.sh:24` `/p:PlatformToolset=v143` ile `.vcxproj`'lardaki `v142`'yi ezer (`Tests/BotCoreTests/BotCoreTests.vcxproj:22,28`). Kurulu araç seti dizini **14.44.35207** (VS 2022 Community; `plans/F1-01-paket-izleyici.md:398` `CL.exe` yolu). `Microsoft (R) C/C++ Optimizing Compiler Version 19.xx` başlığı hiçbir depo kaydında **geçmiyor** (`plans/_logs/*.log` 265 dosya: eşleşme yok; `/v:minimal` başlık basmaz); 14.44 ⇒ cl 19.44 `[Ç]` | `BotCoreTests` Release: `Optimization=MaxSpeed` (`/O2`), `RuntimeLibrary=MultiThreaded` (`/MT`), `/W4`, `stdcpp17`; **`/GL` (WholeProgramOptimization) yok** (`BotCoreTests.vcxproj:63-76`, `WholeProgramOptimization` satırı yalnızca `GameServer`'da, `proj-GameServer.vcxproj:23`); **Win32 (x86 32-bit)** (`build.sh:24` `/p:Platform=Win32`) | Aynı Ryzen 7 7800X3D, `nproc=16` (`plans/F5-02-...md:252,283`, `F5-05:297`, `F5-06:335`, `F5-07:339` "Ryzen 7 7800X3D, `nproc=16`"); F5-52/53/54/56/57/58 raporlarında CPU **yazılmamış** (aynı makine `[Ç]`). Windows `.exe`'si WSL'den `tools/run-tests.sh` ile çağrılır | Gerçek `std::chrono::steady_clock` ölçümleri birim testlerinin içinden. Debug'da süre kapısı yok (`#ifndef _DEBUG`) |
| ORT-S | Aynı MSVC v143 (14.44.35207), **`GameServer.exe`** Release | `GameServer` Release: `/O2 /Ob2 /Ot /Oy- /GL /MT /fp:precise` (`plans/F1-01-...md:398`; `proj-GameServer.vcxproj:23,100-105`), Win32 | Aynı makine; sunucu + gerçek IOCP thread'i; tick = `BotManager::Tick()` (`GameServer/Bot/BotManager.cpp:391`) | Yalnızca gerçek sunucuda `PERF_SAMPLE` olayıyla ölçülür. Release olduğu F4-04 doğrulama notunda açık (`docs/STATUS.md:231`: "Release, `TELEMETRY=decisions`"); diğer koşularda her zaman yazılı değil `[Ç]` |

**Yük türleri** (matrisin "ne ölçüyor" sütunu):
- **Y1 saf A\* sorgusu:** tek `NavPathfinder::Find` (varsa +`NavSmoothPath`), harita zone 71 gerçek ızgarası (513×513, 4 m), çiftler Chebyshev ≤ 64 hücre (`near64`).
- **Y2 sentetik N-bot patlaması:** `nav_measure budget`: N bot **her tick** bir `Find`; kuyruk/bütçe/faz yok (`tools/nav-measure/nav_measure.cpp:695-725`).
- **Y3 sentetik 16-bot, fazlı + kuyruklu, yalnız `Find`:** `nav_measure budget-scheduled` (`nav_measure.cpp:728-835`); sorgular kayıtlı rastgele (başlangıç, hedef) çiftleri; tick toplamı = o tick'te yürütülen `Find` süreleri.
- **Y4 sentetik 16-bot, `NavFollower::Update` (A\* + hedef kestirimi + `NavSmoothPath`), gerçek harita, sanal 60 sn:** `NavBudget_RealMap_Load` (`Tests/BotCoreTests/NavBudgetTests.cpp:884-1113`). Botlar yerinde durur (konum güncellenmez), hedefler sabit başlangıçtan ≤ 64 hücre.
- **Y5 gerçek `BotManager::Tick`:** sunucuda `Tick()` başından sonuna ölçülen süre (komutlar, oturum tick'leri, durum anlık görüntüsü, senaryo ve betik tick'i dahil; `BotManager.cpp:396-435`).

**Matris.** Bütün süreler milisaniye (ms), aksi belirtilmedikçe; "p95" yeni ölçümde tick/sorgu üzerinden. Eşik sütunu: *kapı* = birim testinin `CHECK`'i, *hedef* = doküman kabul/ölçüt.

| Kimlik | Araç / test | Ort. | Ne ölçüyor | Sonuç | Eşik (kaynak) | Kanıt |
|---|---|---|---|---|---|---|
| **PM-G1** | Değerlendirme §5.5 geçici betik (sonradan `nav_measure budget`) | G | **Y2**: zone 71, 16 bot **her tick** 1 `Find`, near64; yalnız nav sorgusu; kuyruksuz üst yük | tick toplamı p50/p95/p99/max = 1,43 / **2,83** / 4,51 / 7,24; sorgu p95 0,389; `expanded` p95 2567. mid150: tick p95 8,82; tüm harita: 30,2; 64 bot near64: 9,49 | Hedef yok (yalnız büyüklük); karşılaştırma için MET-PERF-02 5 ms (`docs/16:222`) | `docs/reports/degerlendirme-2026-10-02.md:137-148` (§5.5); `docs/12:230`; rapor `:10,111,158` "MSVC değildir" |
| **PM-G2** | `nav-measure.sh budget` (güncel kod, `196857d`) | G | PM-G1 ile aynı yük (**Y2**), kalıcı araç | near64/16 bot tick p50/p95/p99/max = 1,41 / **2,71** / 5,17 / 7,77; mid150 8,95; tüm harita 26,8; 64 bot 9,98 | — | `docs/reports/degerlendirme-2026-10-02-ek.md:104-111`. PM-G1'den −%4 p95: koşular arası gürültü (tohum/yük) |
| **PM-G3** | `nav-measure.sh budget-scheduled` | G | **Y3**: 16 bot, 500 ms'de bir faz kaydırılmış sorgu, `NavQueryScheduler` bütçe 1,5 ms; yalnız `Find` | **B** (kuyruklu) tick p95 **0,771-0,812**, p99 ≤ 1,301, `longest_wait_ms` 200-400, `served` 1920, `pending` 0. **A** (kuyruksuz) p95 0,771-0,887: **A de fazlı** olduğundan A ≈ B | Araç eşiği (`nav-regress`): B tick p95 ≤ 1,5; bekleme ≤ 1000 (gerileme alarmı, kabul değil) | `plans/F5-53-...md:133,170,208,257,267`; `docs/STATUS.md:189`; `nav_measure.cpp:728-835`; `tools/nav-regress.py:485,489,703,704` |
| **PM-G4** | Değerlendirme §5.3 / `nav_measure arena` | G | Y1: doğuş → arena `Find`, tek sorgu | Karus 0,55-0,56 ms (4148 düğüm); El Morad `NodeLimit` 20 000 düğüm 2,7 ms | Süre eşiği yok (düğüm sayısı bulgusu) | `degerlendirme-2026-10-02.md:121-135`; `...-ek.md:94-102` |
| **PM-G5** | `tools/nav-regress.sh` Z denetimleri (`budget.near64.query_p95`, `sched.B.tick_p95`, `sched.B.longest_wait`) | G | PM-G2/PM-G3 çıktısını eşikle karşılaştırır | Eşikler: sorgu p95 ≤ 2,0; B tick p95 ≤ 1,5; bekleme ≤ 1000 | AC-NAV-02, AC-NAV-07 (`nav-regress.py:701,703,704`); `ADR-0006:147` "host ölçümü MSVC Release değildir" | `tools/nav-regress.py:39,485,489,701-704`; `plans/F5-11-...md:47,209` |
| **PM-M1** | `NavPath_Perf_T_NAV_03` (`NavPathTests.cpp:591-720`) | M | **Y1** near64, 1000 sorgu, tek `Find` | p50 0,075; **p95 0,526-0,540**; p99 0,981; `found` 997; `expanded` p95 2431. `far150`/`global` kümeleri yalnız raporlanır (`global` p95 ≈ **5,5**) | **Kapı** p95 ≤ 2,0 (AC-NAV-02, MET-PERF-03, `ADR-0006` m.4) | `plans/F5-02-...md:252,258,283`; `docs/reports/gece-2026-10-02-nav.md:36,71,87` |
| **PM-M2** | `NavSmooth_Perf` | M | Y1: `NavSmoothPath` süresi | p95 0,032-0,034 | Kapı ≤ 0,5 (`NavSmoothTests.cpp:659`) | `plans/F5-03-...md:247,273` |
| **PM-M3** | `NavTrack_Perf` (`NavFollower::Update`, near64) | M | **Y1+**: tek `Update` = A\* + kestirim + düzleştirme | exact p95 0,546-0,568; mage 0,597-0,629 | **Kapı** ≤ 2,0 (AC-NAV-02; `NavTrackTests.cpp:1199`) | `plans/F5-04-...md:307,330`; `F5-52:107,137`; `F5-56:115` |
| **PM-M4** | `NavReach_Perf` | M | `Build` ve `Judge` | `build_ms` 8,47; judge p95 0,000 / 0,017 | Kapı build ≤ 100, judge ≤ 0,5 (`NavReachTests.cpp:896,1033`) | `plans/F5-05-...md:328` |
| **PM-M5** | `NavDanger_Perf` | M | Y1 tehlike/yasaklı alanla; katman yeniden kurma | zones 0,698; threats 0,739; rebuild 0,080 | Kapı ≤ 2,0 / ≤ 0,5 (`NavDangerTests.cpp:1613,1628`) | `docs/STATUS.md:203`; `plans/F5-06-...md:363` |
| **PM-M6** | `NavRetreat_Perf` | M | geri çekilme seçimi | party 0,049-0,053; solo 0,616 | Kapı party ≤ 0,5, solo ≤ 3,0 (`NavRetreatTests.cpp:1490,1492`) | `plans/F5-07-...md:329,359`; `docs/STATUS.md:202` |
| **PM-M7** | `NavSegment_Perf` | M | kiriş yürünebilirlik denetimi | p95 0,0002 | Kapı ≤ 0,02 (`NavSegmentTests.cpp:841`) | `plans/F5-50-...md:137,174` |
| **PM-M8** | `NavLos_RealMap` | M | görüş hattı / konum seçimi | p95 0,0009-0,0015 / 0,0140-0,0145 | Kapı 0,05 / 0,5 (`NavLosTests.cpp:634-635`) | `plans/F5-10-...md:296,299`; `docs/STATUS.md:183` |
| **PM-M9** | `NavStuck_SideStep_RealMap` | M | yan adım seçimi | p95 0,0017 | Kapı ≤ 0,2 (`NavStuckTests.cpp:1330`) | `plans/F5-09-...md:409` |
| **PM-M10** | `NavArena_Perf` | M | **Y1** doğuş → arena, 200 sorgu | p95 0,199-0,209; `expanded` p95 710 | Kapı ≤ 2,0 (`NavArenaTests.cpp:395,460`; AC-NAV-02) | `plans/F5-51-...md:119,152` |
| **PM-M11-A** | `NavBudget_RealMap_Load` mod A | M | **Y4**, kuyruksuz: 16 bot `Update` her tick (500 ms dolunca hepsi aynı tick'te) | tick toplamı p95: 3,281 (Tur 1) ve **2,954-3,336** (Tur 2: 3,201 / 2,954 / 3,094; Claude doğrulaması 3,336); A modu B1 hatasından etkilenmez | Bilgi; B p95 ≤ 0,70 × A p95 koşulunda karşılaştırma tabanı | `plans/F5-53-...md:133,169,254` |
| **PM-M11-B** | `NavBudget_RealMap_Load` mod B | M | **Y4**, kuyruklu (`NavQueryScheduler` 1,5 ms, faz kaydırmalı) | tick toplamı **p95 1,308-1,451** (en kötü: 1,451), **p99 1,965-4,212** (doğrulamada en kötü 4,157; 4 ek tekrar 4,014-4,246), bekleme 100-500 ms, her bot ≥ 119 servis, `served_B` 5758 ≥ 0,9 × `served_A` 5760 | **Kapı** p95 ≤ **2,0**, p99 ≤ 4,5, bekleme ≤ 1100, p95 ≤ 0,70 × A (`NavBudgetTests.cpp:1112-1115`; `F5-53:86`). **Doküman hedefi AC-NAV-07**: p95 ≤ **1,5** (`docs/12:236`; kodda `CHECK` **yok**) | `plans/F5-53-...md:169,175,254`; `docs/STATUS.md:189`; Tur 1'deki B-p95 0,138-0,139 **geçersiz** (yalnız bot 0 servis ediliyordu, B1, `F5-53:217`) |
| **PM-M12** | `NavBudget_Deferred_Chase_Sim` (B, B2) | M | Y4-benzeri takip simülasyonu, sanal 120 sn: ertelenen botların bekleme/takip davranışı | B: `plan_wait_max` 200, `follow_stale_ticks` 0; B2 (+0,5 ms sentetik maliyet): `plan_wait_p95` 400, max 700, `deferred_ticks` 7347 | `plan_wait_max ≤ 1100`, `p95 ≤ 800`, plansız ≤ %3, `follow_stale_ticks = 0` (kapı). Süre kapısı değil, **davranış** testi | `plans/F5-53-...md:85,168,178,257` |
| **PM-S1** | `PERF_SAMPLE` (F3-01), 4 bot, `TELEMETRY=summary` | S | **Y5 gerçek `Tick()`**, **4 bot**, nav yok, yürüme/dövüş yok (yalnız giriş, spawn, birkaç komut) | 19 pencere (5 sn), `tick_p95_us` **≤ 408**, `tick_max_us` ≤ 579; spawn/despawn döngüsü: p95 ≤ 698, max ≤ 881; F3-02 maç başlangıç penceresi (gerçek `t1-7-1.jsonl`): p50 1 µs, **p95 1270 µs**, max 1638 µs | MET-PERF-02: 16 bot p95 ≤ **5000 µs** (`docs/16:222`) | `plans/F3-01-...md:353,357`; `plans/F3-06-...md:35-36`; `docs/phase-reports/F3-taslak.md:16,48` |
| **PM-S2** | `PERF_SAMPLE`, F4 komut pencereleri, 2-4 bot | S | Y5, aksiyon yürütücü etkin, birkaç bot | `tick_p95_us` 63-112 (F3-04), 84-101 (F4-01 hareket), 94-451 (F4 dilimleri), 666 ve 698'e kadar çıkan pencereler; "komut pencerelerinde 500'ü aşabildi" | MET-PERF-02 5000 µs | `plans/F3-04-...md:282`; `plans/F4-01-...md:400`; `docs/reports/gece-2026-10-02.md:101`; `docs/phase-reports/F4-taslak.md:65`; `docs/STATUS.md:195` |
| **PM-S3** | MET-PERF-02 gerçek: **16 bot, yürürken/çarpışırken, nav bağlı** | S | Y5 hedef yük | **YAPILMADI** (aşağıda §13.5.3) | 16 bot p95 ≤ 5000 µs; 64 bot ≤ 15 000 µs (`docs/16:222`); T-PERF-01/03 (`docs/15:204,206`) | `docs/phase-reports/F4-taslak.md:65` "12-16 bot yürürken ölçüm yok" |

Matris dışında kalanlar (zamanlama ölçümü değil, doğruluk/sayım): değerlendirme §5.1 segment/kiriş (4887 segment, 33 503 kiriş, ihlal 0), §5.2 hız kestirimi (sıfır hız oranı), §5.4 takılma yanlış alarmı (yalnızca sanal saat), `nav_measure velocity-robust/stuck/progress`. Bunlar ORT-G/ORT-M farkından etkilenmez (belirlenimci sayımlar); yalnız zamanlama kapıları etkilenir.

#### 13.5.2 Hangi sayı ne için geçerlidir (kurallar)

1. **Kabul kanıtı yalnız MSVC Release (ORT-M birim testi veya ORT-S sunucu) sayısıdır.** `g++` (ORT-G) sayıları büyüklük, oran ve gerileme alarmı içindir (`tools/nav-measure.sh:10-11`, `nav_measure.cpp:5-7`, `ADR-0006:147`). Hiçbir `g++` sayısı bir AC/MET kabulünü tek başına kapatmaz.
2. **`g++` sayısını MSVC hedefinin üst sınırı sanma.** Aynı kaynak, aynı harita: MSVC Win32 `/O2` (GL'siz) tek `near64` sorgusunda `g++ -O2` x86-64'ten **daha yavaştır** (A\* p50 0,075 vs 0,046; p95 0,526-0,540 vs 0,389; tüm harita p95 ≈ 5,5 vs 3,84). Örnek kümeleri aynı değil (tohum/sorgu çifti farklı, `expanded` p95 2431 vs 2567), bu yüzden oran yaklaşık **×1,4-1,6**'dır `[Ç]`. Yani `g++` değerleri iyimserdir; MSVC bütçe kararında MSVC sayısı kullanılır.
3. **Yük tanımları birbirine çevrilemez.** Aynı "16 bot, near64, tick toplamı p95" etiketli dört sayı dört farklı şeydir: PM-G1 **2,83** (her tick 16 `Find`, kuyruksuz, `g++`); PM-M11-A **2,95-3,34** (16 `Update` aynı tick'te, 500 ms'de bir, MSVC; en yakın karşılığı PM-G1'dir, MSVC/`g++` ≈ ×1,1-1,2 ama yük biçimi farklı); PM-M11-B **1,31-1,45** (kuyruklu + `Update`, MSVC); PM-G3-B **0,77-0,81** (kuyruklu, yalnız `Find`, `g++`). **PM-G3-B ile PM-M11-B karşılaştırılamaz:** biri `Find`, diğeri `Update` (= `Find` + kestirim + `NavSmoothPath`, `BotCore/NavTrack.h:427-436`) çalıştırır ve sorgu çiftleri farklıdır; derleyici farkı ile iş farkı ayrıştırılmamıştır (aynı yük her iki derleyicide koşulmadı) `[doğrulanamadı]`.
4. **§13.5'in "16 bot near64 → p95 2,83 ms"** cümlesi *bütçesiz üst yük* içindir (kuyruk gerekçesi); bütçenin sağlandığını **göstermez**. Bütçe sağlandığı PM-M11-B (MSVC) ile gösterilir.
5. **AC-NAV-07 iki katmanlıdır:** (a) **birim düzeyi** (PM-M11-B): MSVC Release, **yalnız nav sorgusu payı**, sentetik 16 sanal bot; test kapısı p95 ≤ **2,0** (gürültü payı `[Ö]`), p99 ≤ 4,5, bekleme ≤ 1100 (= `P-NAV-MAX-WAIT` 1000 + 1 tick); ölçülen en kötü p95 1,451 aynı zamanda ≤ **1,5**'i de sağlar ama marj ~%3'tür ve 1,5 kodda kapı değildir; (b) **oyun içi** (F5-55 T-NAV-11): gerçek `Tick()` içinde **nav payı** p95 ≤ **1,5 ms**. Doküman hedefi **1,5**'tir; 2,0 yalnızca (a)'nın test kapısıdır. 1,5 iddiası için "(a) geçti" **yeterli değil**, (b) şarttır. p99 hedefi AC-NAV-07'de yoktur; ölçülen p99 4,0-4,2 ms, tek pahalı sorgunun bütçeyi aşmasındandır (bütçe sorguyu bölmez, `F5-53:114`).
6. **MET-PERF-03** (arama başına süre, p95 ≤ 2 ms) **tek sorgudur** (PM-M1, PM-M3, PM-M10 vb., near64). **MET-PERF-02** (tüm `BotManager::Tick`, 16 bot p95 ≤ 5 ms) **toplam tick'tir** ve yalnız `PERF_SAMPLE` (PM-S*) ile ölçülür. AC-NAV-07'nin "ölçüm MSVC Release'te tekrarlanır" cümlesi MET-PERF-**02**'nin nav payına bağlanır, MET-PERF-03'e değil.
7. **Gerçek `Tick` ölçümü yoktur.** PM-S1/PM-S2 gerçek `Tick()`'tir ama 2-4 bot, nav yok; PM-M* ve PM-G* gerçek `Tick()` **değildir**. PM-M11-B nav payı tahmini olarak 1,3-1,5 ms ver­ir; üstüne `Tick()` taban maliyeti (4 bot: p95 0,06-1,3 ms; 16 bot bilinmiyor) eklenir. Toplamın 5 ms'yi sağladığı **kanıtlanmış değildir**; tek kaba üst sınır: 1,451 (nav) + 1,270 (4 botlu en kötü pencere) ≈ 2,7 ms ≪ 5 ms, ancak bu iki sayının toplanması bir **çıkarımdır** (`p95` toplanmaz; aynı tick'te çakışma, 16 bot taban maliyeti ve `Update` dışı nav işleri dahil değil) `[Ç]`.
8. **Persentil tanımı:** `PERF_SAMPLE` ve `nav_measure` en yakın sıra (`ceil(p·n)`); `NavBudget_*` testi `floor(p·(n−1))` (`NavBudgetTests.cpp:78-86`). n = 600 tick için aynı sıraya düşer; n ≈ 50 (5 sn pencere) için bir sıra fark eder (nearest-rank daha yüksek). `bot-telemetry-report.py` hükmünü **en kötü pencerenin** p95'inden verir (`p95_worst_us`), yani tüm koşunun p95'inden muhafazakârdır.
9. **Yeni sayı yazarken şu etiketi ekle:** `[V: <ORT kodu>, <yük türü Y1..Y5>, <araç/test adı>]`. `[V: WSL g++ -O2]` etiketi yalnız ORT-G için; `MSVC Release` etiketi yalnız ORT-M/ORT-S için. Örnek: `[V: ORT-M, Y4, NavBudget_RealMap_Load, worst_B_p95 = 1,451]`.

10. **Kapanmış plan ve raporlardaki ifadeler bu bölüme göre okunur** (tarihsel kayıtlar değiştirilmedi): (a) `plans/F5-11`, `ADR-0006` madde 4 ve `docs/reports/gece-2026-10-03-nav.md` içindeki "MSVC Release kanıtı `docs/12` §13.5'te" ifadesinin dayanağı §13.5.1'dir (PM-M1 tek sorgu, PM-M11-B 16 bot; oyun içi kanıt F5-55/T-NAV-11); (b) `plans/F5-53` Tur 1 raporundaki realm B `tick_p95 = 0,139` **geçersizdir** (B1 hatası: yalnız bot 0 servis ediliyordu); geçerli B sayıları Tur 2'dir (PM-M11-B); (c) `plans/F5-53` K7b ve `plans/F5-55`'teki "`PERF_SAMPLE` nav payı" alanı **henüz yoktur**, F5-55 dilimlerinde eklenir (§13.5.3); (d) `tools/nav-regress.py` Z eşikleri (`tick_p95 ≤ 1,5`, `longest_wait ≤ 1000`) g++ `Find`-yalnız yükünde gerileme alarmıdır, AC-NAV-07 kanıtı değildir (test kapısı bekleme eşiği 1100'dür).

#### 13.5.3 Gerçek `BotManager::Tick` (MET-PERF-02) ölçüm tanımı

**Durum (2026-10-03): ölçen kod VAR, hedef yükte ölçüm YOK.**

- **Var:** `BotManager::Tick()` süresini ölçer (`GameServer/Bot/BotManager.cpp:396` başlangıç, `:434-435` `IsEnabled(TEL_SUMMARY)` kapısıyla `RecordTick`, `:438-455` 4096 örneğe kadar µs biriktirir, 5 sn dolunca `EmitPerfSample` `:457-516`); `"PERF_SAMPLE"` olayı `TEL_SUMMARY` seviyesinde (varsayılan) yazılır (`:512`). Alanlar: `window_ms`, `tick_n`, `tick_p50_us`, `tick_p95_us`, `tick_p99_us`, `tick_max_us`, `sessions`, `in_game`, `pool_free`, `skipped_ticks`, `queue_len`, `written`, `dropped_soft`, `dropped_hard` (`docs/16:71`). Ölçülen aralık: `ProcessCommands`, `TickSessions`, `RefreshStatusSnapshot`, `m_scenario.Tick`, `m_script.Tick` (`:428-432`). Çözümleme aracı: `tools/bot-telemetry-report.py` (MET-PERF-02 tablosu; `in_game_max ≤ 16` → bütçe 5000 µs, ≤ 64 → 15 000 µs, aksi `NO_BUDGET`; hüküm `p95_worst_us`).
- **Yok (1): hedef yük.** `PERF_SAMPLE` yalnız 2-4 botla alınmıştır (PM-S1/PM-S2). F4 taslağı: "12-16 bot yürürken ölçüm yok" (`docs/phase-reports/F4-taslak.md:65`); F3 taslağı: `decisions`/16 bot "ölçülemedi ... 12 bot sınırı" (`F3-taslak.md:48`). Yerel DB'de 12 bot karakteri vardır (`db/002`; 16'ya 4, `full20`'ye 8 eksik: `docs/reports/gece-2026-10-03-nav.md:103`; 16/20 karakter betikleri `db/005`/`db/006`, henüz yok). T-PERF-01/03 çalıştırılmadı.
- **Yok (2): nav payı.** `GameServer/`, `shared/` altında hiçbir `Nav*` başlığı çağrısı ve hiçbir `NavService` sınıfı yoktur (`git grep` boş; `Nav*.h` yalnız `BotCore/` ve birim testlerindedir), `NAV_PATH`/`NAV_*` olayı üretilmez (`docs/16:58-59` yalnız tasarım); F5-55 TASLAK'tır. `plans/F5-53-...md:99` ve `F5-55:38`'in "`PERF_SAMPLE` nav payı" dediği alan **henüz yoktur**.
- **Yok (3): bot başına tick (MET-PERF-01)** (`docs/16:71` "karar katmanı gelince eklenir").

**Ölçüm tanımı (F5-55 / T-NAV-11 / T-PERF-01 için):**

| Madde | Tanım |
|---|---|
| Sürüm | `./tools/build.sh Release` (MSVC v143, Win32, `GameServer` `/O2 /GL`); derleme çıktısındaki araç seti sürümü ve CPU rapora yazılır |
| Senaryo | `[BOT] ENABLED=1, MAX_BOTS=16, TELEMETRY=summary` (karar katmanı olmadan `decisions` ek maliyeti ayrı ölçülür: `docs/17:114` %10 sınırı), `SPAWN_ON_START` 16 bot (8 Karus + 8 El Morad; 16 bot için `db/005` (ilk 16 karakter, F8-03; `db/README.md:127`) uygulanmış olmalı: **önkoşul**; bugün yerel DB'de 12 karakter var), arena A, T-NAV-11: botlar `/bot goto` ile near64 hedef takibi yapar (hedef değişimi 6-10 sn, 500 ms yeniden planlama), en az 30 dk (T-PERF-01) ve ayrıca 5 dk "yoğun sorgu" fazı (tüm botlar aynı anda yeniden planlama) |
| Ölçülen büyüklük 1 (MET-PERF-02) | `PERF_SAMPLE.tick_p95_us` (5 sn pencere, ~46-50 tick) ve `tick_p99_us`, `tick_max_us`; `in_game` = 16 olan pencereler. Hüküm: `bot-telemetry-report.py` `p95_worst_us ≤ 5000` ve `in_game_max = 16`; ayrıca tüm koşu için pencere p95'lerinin medyanı (`p95_est_us`) raporlanır. Pencerede `skipped_ticks` ve `dropped_*` = 0 değilse pencere geçersiz sayılır |
| Ölçülen büyüklük 2 (nav payı, AC-NAV-07 oyun içi) | **Yeni `PERF_SAMPLE` alanları (F5-55 uygulaması):** `nav_us_p95`, `nav_us_p99`, `nav_us_max` (tek `Tick()` içinde `NavQueryScheduler` tarafından seçilen sorguların `Update`/`Find` toplam süresi, `steady_clock` ile `Tick()` içinde ölçülür, aynı 5 sn penceresi), `nav_queries` (pencerede yürütülen), `nav_deferred` (ertelenen bot-tick sayısı), `nav_wait_max_ms` (en uzun istek → servis). Hüküm: `nav_us_p95 ≤ 1500` ve `nav_wait_max_ms ≤ 1100` (+ `docs/12 §13.5` `P-NAV-MAX-WAIT` 1000 ms + tick) |
| Ölçülen büyüklük 3 (MET-PERF-03) | `NAV_PATH` olayı (trace seviyesi; `süre µs`, `düğüm sayısı`, `başarı`, `docs/16:58`) ayrı bir koşuda; p95 ≤ 2 ms |
| Taban | Aynı senaryo `ENABLED=1` ve botsuz (`MAX_BOTS=0`) için sunucu ana timer gecikmesi/CPU (MET-PERF-04); nav kapalı 16 bot (taban) ve nav açık 16 bot farkı = nav'ın gerçek maliyeti |
| Gürültü | En az 3 koşu, en kötü koşu raporlanır; paralel döngü/derleme çalışmazken (ölçüm sırasında `tools/auto-loop.sh` durdurulur); ilk 60 sn (giriş/spawn penceresi, PM-S1'de p95 1270 µs) ısınma sayılır ve ayrıca raporlanır, hükme katılmaz |
| Çıktı | `tools/bot-telemetry-report.py <log dizini>` MET-PERF-02 tablosu + nav payı tablosu; rapor ORT-S ortam satırını (derleyici sürümü, CPU, bot sayısı) taşır |
| Geçmez ise | Önce `nav_us_p95` / taban ayrımı: `tick_p95_us` > 5000 ama `nav_us_p95 ≤ 1500` ise sorun nav dışıdır (örn. `TickSessions` 16 bot); `nav_us_p95 > 1500` ise `P-NAV-TICK-BUDGET-MS` / bütçe kuyruğu yeniden ayarlanır |

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
