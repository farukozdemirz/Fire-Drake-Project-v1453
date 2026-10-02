# Değerlendirme Eki — 2026-10-02 (proje sahibi kararları sonrası)

> Hazırlayan: Claude (arka plan ajanı, `degerlendirme-2/2026-10-02` dalı; taban `gece/2026-10-02` @ `0dfeca1`). Ana rapor: [degerlendirme-2026-10-02.md](degerlendirme-2026-10-02.md); kalıcı takip: [degerlendirme-takip.md](degerlendirme-takip.md).
> Kapsam: proje sahibinin 7 maddelik kararı. Üretim koduna dokunulmadı; çalışan döngü planlarına (şu an uygulanan: F4-51 `bot/F4-51`, F5-50 `bot/F5-50`) dokunulmadı; çalışma ağaçlarına yazılmadı; sunucu çalıştırılmadı.
> **Dürüstlük notu:** ölçümler ana makinede `g++ -O2` ile (MSVC Release değildir), `gece/2026-10-02-nav` @ `196857d` başlıklarıyla, gerçek zone 71 ızgarasıyla yapıldı; **oyun içi (sunucu+istemci) doğrulama yapılmadı**, hepsi `degerlendirme-takip.md`'de `BEKLİYOR`.

## 1. Yöntem ve kalıcı araçlar

| Araç | Ne yapar | Çalıştırma |
|---|---|---|
| `tools/nav-measure.sh` (+ `tools/nav-measure/nav_measure.cpp`) | Bölümler: `smoothing` (ham yol / düzleştirme / paket kirişi / `NavLineClear` / düz adım), `synthetic`, `velocity`, `arena`, `budget`, `stuck` (gerçek F5-09 detektörü, paket zamanlaması), `all` | `NAV_SRC_ROOT=<BotCore/Nav*.h içeren ağaç> tools/nav-measure.sh smoothing --n 6000` |
| `tools/nav-segment-check.py` | Bağımsız süpercover oracle (kapalı kare, köşe/kenar teması); `--selftest` PASS | `python3 tools/nav-segment-check.py < segments.txt` |

Geçici betiklerden kalıcıya: duvar/segment ölçümü, hız-kadans ölçümü, arena/doğuş yolu, çoklu bot bütçesi, takılma yanlış alarmı `tools/nav-measure.sh` içindedir. Takılma ölçümü artık **gerçek F5-09 `NavStuckDetector`** ile yapılır (önceki Python simülasyonu detektörü modellemiyordu). Hız kestirimi dayanıklılık ön ölçümü (§3.2) hâlâ geçici betikle yapıldı; kalıcı hâli F5-56 `velocity-robust` bölümüdür (HAZIR).

**Ölçüm aracının kendi hatası (kayıt):** ilk sürümde kiriş taraması hücre sınırına tam oturan uç noktalarda yanlış hücre seçiyordu ve 18/250 000 sahte "engelli kiriş" üretti; düzeltildi (sınırda aşağı yöne hareket → alt hücre) ve bağımsız Python oracle ile örnek vektörler çapraz doğrulandı. Aşağıdaki sayılar düzeltilmiş araçtandır.

## 2. Madde madde

### Madde 1 — Öğrenme

- **Değişiklik:** `ADR-0030-DEG` KABUL: aşama 1 = oturum içi kestirim + rol profili (F6–F10); aşama 2 = **karakter bazlı kalıcı öğrenme F12** (kapsamdan çıkarılmadı). `docs/17`: F12 bölümü (kapsam, ön koşullar F9/F8/kararlı karakter kimliği, kabul AC-CHR-01..06, risk, geri alma `δ_c = 0`), faz tablosu, mermaid, G12 kapısı. `docs/14` §6.1: karakter satırı F12'ye bağlandı.
- **Doğrulama kanıtı:** doküman (ADR + `docs/17` + `docs/14`); tablo sütun denetimi 0 hata. Oyun içi: AC-LRN/AC-CHR `BEKLİYOR` (F9/F12).
- **Açık nokta:** F12'nin güncelleme yöntemi (ADR-0008, L1 yöntemi) ve karakter başına minimum maç sayısı F9 sonrası kararı; F9 "iyileşme yok" sonucu çıkarsa F12 ayrı ADR ister.

### Madde 2 — Kazanma kuralı

- **Değişiklik:** `ADR-0031-DEG` KABUL: iki ayrı tür. `killdiff_timed` (respawn açık): `fark ≥ win_margin` → galibiyet, `fark ≤ −win_margin` → mağlubiyet, `|fark| < win_margin` → beraberlik; varsayılan `win_margin = 2` (fark −1, 0, +1 = beraberlik); 7 satırlık örnek tablosu (14–11 `win_a`, 10–8 `win_a` sınır dahil, 9–8 `draw`, 8–8 `draw`, 8–9 `draw`, 7–9 `win_b`, 0–0 `draw`/`invalid`) ve `win_margin = 3` değişimi. `wipe_first` (respawn kapalı, ayrı tür): tüm üyeler ölü → rakip galip, aynı tick'te çift wipe `draw`, süre dolunca canlı sayısı, 1v1 özel durumu; örnekler. `timed_score` yalnız ölçüm. Eşikler senaryo anahtarı (`win_margin`, `duration_sec`, `early_end_margin`, `engage_timeout_sec`, `respawn`), `MATCH_START`'a yazılır; pilot kalibrasyonu (20 maç, hedef beraberlik %15–35, ADR eki). `docs/15` §6b, `docs/16` MET-OUT-05 güncel.
- **Doğrulama kanıtı:** doküman; örnek tabloları `ScenarioRunner` birim test vektörü olarak F8 planına kabul kriteri. Oyun içi: 20 maç pilot `BEKLİYOR` (F8).
- **Açık nokta:** `wipe_first` için bot yürütücüsünde "respawn kapalı" modu F8 kodu; ilk-hasar (`engage`) olayının tanımı telemetri alanıyla bağlanmalı.

### Madde 3 — Başlangıç yerleşimi

- **Değişiklik:** `ADR-0032-DEG` KABUL: maç **öncesi** kurulum yerleşimi serbest ve `TEST_TELEPORT` sayılmaz; maç başladıktan sonra hareket/respawn/savaşa dönüş normal mekanikle. **Neden DB yazımı:** konum, HP/MP/NP ve envanter girişte `USERDATA`/`WAREHOUSE` satırından yüklenir (db/002 de bu yoldan kurar); NP/envanter/tam konum için normal oyun eylemi yok (KI-013; satıcı yok; `WIZ_WARP`/`testtp` GM/kurtarma aracı). **Canlı `CUser` ile tutarlılık:** (1) yalnızca bot çevrimdışıyken (`DESPAWNED`, `ReqUserLogOut` son ifadesi tamam) satıra yazılır, canlı nesneye dokunulmaz; (2) yazım çıkış kaydı ile giriş yüklemesi arasında aynı DB kuyruğunda (FIFO); (3) giriş yolu insanla aynı (`SetUserAbility`, bölge kaydı, AIServer); (4) HP/MP kırpma kuralı; (5) spawn sonrası `snap` ile doğrulama, sapmada `SETUP_FAIL`; (6) yazma yalnızca `BOT_TABLE` adlarına, kişisel veri tabloları okunmaz.
- **Doğrulama kanıtı:** kod okuması (`GameServerDlg.cpp:2962-2964` kayıt yolu, `BotManager.cpp` despawn/`m_deleted` bekleme, `db/README.md` 002); oyun içi: iki ardışık maçta `snap` eşitliği ve başlangıç doğrulaması %100 `BEKLİYOR` (F8).
- **Açık nokta:** DB yazımının uygulama yolu (`g_DBAgent` içinde yeni bot-yalnızca yöntem mi, yan süreç SQL mi) F8 planında karar; ADR ikisini de kabul eder, yetki listesini şart koşar.

### Madde 4 — Geri çekilme

- **Değişiklik:** `ADR-0033-DEG` KABUL: arena modunda geri çekilme arena içinde; arena dışındaki bot sınırın içine girebilir (ceza yok). Serbest Ronark için güvenli konuma çekilme / yeniden gruplanma / savaşa dönüş **ayrı alt kalemler** F11-a/b/c, testler T-FREE-06..08, `docs/17` F11'de; `docs/11` §4.3 ve `docs/12` §13.4 mod ayrımı notu.
- **Doğrulama kanıtı:** doküman; birim: F5-51 (HAZIR); oyun içi: T-NAV-10, T-FREE-06..08 `BEKLİYOR`.
- **Açık nokta:** arena içi (60 m) geri çekilmenin taktik etkisi (F6/F7 ölçer); F11 ADR kapılı (A-03).

### Madde 5 — F5 doğrulamaları

| Alt madde | Yapılan | Kanıt (güncel kod, `tools/nav-measure.sh`) | Açık nokta |
|---|---|---|---|
| Hız kestirimi yalnız pencereyle kapatılmasın | Yeni plan **F5-56** (HAZIR): gözlem zaman damgası sözleşmesi, varış jitter'ı ve yığılma, değişken aralık, paket kaybı, eski veri, ani yön/hız değişimi, ışınlanma sıçraması, yuvarlama, kuyruklu takip simülasyonu; kalıcı `velocity-robust` ölçümü | F5-52 sonrası kod, ön ölçüm (§3.2): jitter ±150 ms (20 tohum) en kötü p95 %12,7 / max %14,0; yığılma eklenince max **%51** (Theil–Sen ≈ %10) → F5-56 eşikleri ve olası düzeltme (§3.3 e) | F5-56 uygulanmadı; oyun içi T-NAV-06 `BEKLİYOR` |
| İki ırk için ölüm→respawn→arena | **F5-55**: T-NAV-10 Karus ve El Morad ayrı (her ulus ≥ 10 tekrar, süre ±%20, takılma/`NodeLimit`/`InvalidGoal` 0); `docs/15` §4.3; F5-51 birimde iki ulus | `arena` bölümü: El Morad doğuşu → arena **varsayılan ceza ile `NodeLimit`**, ceza 0 ile 2924 düğüm (§3.4) | oyun içi `BEKLİYOR`; F5-51 uygulanmadı |
| 16 botta tick ve yol bütçesi, ertelemede davranış | **F5-53** güncellendi: `NavWhileDeferred` sözleşmesi (plan taze → mevcut yolu izle, bayat/yok → dur), kuyruklu takip simülasyonu (kabul: bekleme ≤ 1100 ms, bayat planla sürme 0, takip hatası ≤ 1,25×), `budget-scheduled` kalıcı ölçümü; oyun içi **T-NAV-11** (F5-55) | `budget` bölümü (§3.5): 16 bot near64 tick toplamı p95 2,71 ms, mid150 8,95 ms, tüm harita 26,8 ms (MET-PERF-02 hedefi 5 ms) | zamanlayıcı yok (F5-53 uygulanmadı); oyun içi ölçüm `BEKLİYOR` |
| Duvar denetimi: ham yol / düzleştirme / doğrulama yöntemi? | **Sınıflandırıldı** (§3.1): ham yol 0/379 054, düzleştirme 0/33 365, paket kirişi 0/250 000, `NavLineClear` yanlış-pozitif 0/360 288 (+ sentetik 0/338 000); planlayıcısız düz adım **447/6000 (%7,45)**. Sorun planlayıcıda değil, **icradaki yürünebilirlik denetiminin yokluğunda** (CLI-08). Yeniden üretilebilir örnek vektörleri ve kalıcı regresyon planı **F5-58** | `smoothing` + `synthetic` bölümleri; örnekler §3.1; `tools/nav-segment-check.py` çapraz doğrulama | F5-50 uygulanıyor, F5-58 uygulanmadı; oyun içi AC-NAV-03 `BEKLİYOR` |
| Takılma: niyet + gerçek ilerleme, normal paket aralığında yanlış alarm yok | Yeni plan **F5-57** (`NavProgressAssessor`: niyet/paket/rota ilerlemesi, bekleme-guard-takılma ayrımı, U-dönüşü); F5-54 (paket sıklığı hazır ayarı + guard-engeli) korundu | `stuck` bölümü (§3.3), **gerçek F5-09 detektörü**: tick başına beslemede F5-09 varsayılanı gecikmeli tick modelinde 600 sn'de **6 yanlış epizod** (≈ 36/saat), hazır ayar (3200 ms) **0**; gerçek takılma tespit gecikmesi 1500 ms / 3200 ms | F5-54/F5-57 uygulanmadı; oyun içi T-NAV-04 `BEKLİYOR` |

### Madde 6 — Karakter sayısı

- **Değişiklik:** `docs/15` §6a, `docs/04` §3.3, `docs/17` G8: **16 = aynı anda gereken** (ulus başına 8; C8-A: 2 W-P, 1 W-G, P-HD, P-HB, 2 M-F, 1 M-I) → bugünkü 12'ye göre ulus başına +1 W-P, +1 M-F. **20 = 16 + 4**: ulus başına 3. W-P ve 3. M-F, C8-B (3 W-P) / C8-C (3 M-F) / C8-D (3 W-P, 2 M-F) kompozisyon çeşitliliği (EVAL-8v8-MIX, `docs/14` §13) için; **yedek ya da test profili değildir** (OP-*, B0-NAIVE, baseline aynı sınıf karakterlerini farklı politikayla oynatır). Aynı anda 16 giriş yapar, 4'ü boşta. Seçenekler: 16 ile başla (yalnız EVAL-8v8-A) ya da 16 + maç arası DB sınıf yeniden yazımı (yavaş, seçilmedi).
- **Doğrulama kanıtı:** doküman; hesap `docs/15` §6a tablosunda. Oyun içi: 8v8 başlatma `BEKLİYOR` (F8, `db/003`).
- **Açık nokta:** proje sahibi: ilk adımda 16 mı 20 mi (F8 ön koşulu).

### Madde 7 — Kabul ve kanıt

- **Değişiklik:** geçici betikler kalıcılaştırıldı (§1); `docs/reports/degerlendirme-takip.md` kuruldu: **plan / kod / oyun içi ayrı sütun**, kapanış kuralı ("yalnızca doküman veya birim testi geçti diye doğrulandı yazılmaz"), her plan satırı için test kimliği ve `BEKLİYOR`/`YAPILDI`; `docs/STATUS.md`'ye bağlantı. Güncel kod üzerinde yeniden ölçüm yapıldı (§3). Planlarda oyun içi kanıt maddeleri açıkça "bu planda kapanmaz" diye ayrıldı (F5-53 K7b, F5-56 K8, F5-57 K9, F5-58 K8).
- **Doğrulama kanıtı:** `tools/nav-segment-check.py --selftest` PASS; `tools/nav-measure.sh` bölümleri derlendi ve koşuldu (çıktılar §3); tablo sütun denetimi.
- **Açık nokta:** oyun içi kanıtların hepsi `BEKLİYOR`; F4 hattında yalnız F4-50 çalışma zamanında doğrulandı (`YAPILDI`).

## 3. Ölçümler (güncel kod, ana makine `g++ -O2`)

### 3.1 Duvar denetimi sınıflandırması (`smoothing --n 6000`, tohum 20261002)

```
SMOOTHING paths=5977 raw_edges=379054 raw_bad=0 smooth_segments=33365 smooth_bad=0 paths_with_smooth_bad=0 chords=250000 chord_bad=0
LINECLEAR pairs=1002585 clear=360288 false_positive=0
STRAIGHT pairs=6000 blocked=447
SYNTHETIC single_block trials=309680 nav_line_clear_true=270320 false_positive=0
SYNTHETIC random_clutter trials=68335 nav_line_clear_true=20784 false_positive=0
```

Yeniden üretilebilir düz-adım örnekleri (iki uç yürünebilir, düz adım engelli hücreye değiyor; `tools/nav-segment-check.py` hepsini `BLOCKED` bulur): `(1566.0,878.0)→(1578.0,866.0)`, `(926.0,834.0)→(934.0,826.0)`, `(1102.0,1034.0)→(1106.0,1046.0)`; kontrol `(1274.0,890.0)→(1280.0,890.0)` `OK`. **Sonuç:** sorun ham yolda ve düzleştirmede değil; doğrulama yöntemi (`NavLineClear`) bu testlerde muhafazakâr; açık, planlayıcısız düz adım için icrada denetimin olmamasıdır (F5-50 + F5-58 + F5-55).

### 3.2 Hız kestirimi (`velocity`; F5-04 varsayılan pencere 1000 ms vs F5-52 sonrası)

`cadence_ms=500/1000 → zero 0%`, `1500/1540/2000 → zero 100%` (1000 ms pencerede, F5-52 öncesi ayar: hata yeniden üretilir; F5-52 sonrası 4000/400 ayarıyla sıfır). Dayanıklılık ön ölçümü (geçici betik, **F5-56 kalıcılaştıracak**; F5-52 sonrası `NavTargetTracker`, sabit 4,5 m/s, 0,1 m yuvarlama, 20 tohum): jitter ±150 ms → en kötü tohumda `err_p95 = 0,127`, `err_max = 0,140`, sıfır hız yok; jitter + %5 yığılma → `err_p95 = 0,142`, **`err_max = 0,512`**; Theil–Sen medyan eğimli deneme aynı senaryoda `err_max ≈ 0,10`. 180° dönüş: dönüşten sonraki ikinci gözlemde doğru yön ve büyüklük (~4,5 m/s), geçiş örneğinde −1,5 m/s (≈ 1 paket periyodu).

### 3.3 Takılma yanlış alarmı (`stuck`, gerçek F5-09 `NavStuckDetector`, 600 sn)

| Tick modeli | Besleme | F5-09 varsayılanı | Hazır ayar (3200/8000/3) |
|---|---|---|---|
| 100±0, 100±10 ms | tick başına / yalnız paket anı | 0 / 0 yanlış epizod | 0 / 0 |
| 110,8±20 ms + %3 olasılıkla +250 ms gecikme | **tick başına** | **6 yanlış epizod** | 0 |
| aynı | yalnız paket anı | 0 | 0 |

Gerçek takılmada (konum sabit) tespit gecikmesi: varsayılan 1500 ms, hazır ayar 3200 ms (MET-NAV-02 p95 ≤ 5 sn içinde).

### 3.4 Arena ve doğuş yolu (`arena`)

| Sorgu | Alan yok | Ceza 10 (varsayılan) | Ceza 0 |
|---|---|---|---|
| Karus doğuşu → arena | 406 düğüm | 4148, 0,56 ms | 602 |
| El Morad doğuşu → arena | 2897 | **`NodeLimit` (20 000)** | 2924 |
| Arena → Karus doğuşu | Found | `InvalidGoal` (tasarım) | `InvalidGoal` |

Ek ön ölçüm (geçici betik, ceza değeri değiştirilerek): ceza 1,0 → El Morad 16 250 düğüm; ceza 3 → `NodeLimit`: cezayı küçültmek yetmez, yasaklı bölgeden başlayan sorguda fiilen 0 gerekir (F5-51).

### 3.5 Çoklu bot sorgu bütçesi (`budget`, aynı tick'te N bot tek sorgu)

| Küme | Bot | Tick toplamı p50 / p95 / p99 / max (ms) |
|---|---|---|
| near64 | 16 | 1,41 / 2,71 / 5,17 / 7,77 |
| mid150 | 16 | 5,67 / 8,95 / 10,03 / 10,68 |
| tüm harita | 16 | 19,7 / 26,8 / 31,3 / 32,5 |
| near64 | 64 | 6,15 / 9,98 / 11,49 / 11,75 |

## 4. Bu çalışmadaki plan/ADR/doküman değişiklikleri

Commit'ler `degerlendirme-2/2026-10-02` dalında, plan değişiklikleri **ayrı commit** (birleştirme penceresi için): araçlar `265e6fd`; ADR/doküman kararları `84bfb65`; F5-53 `66cc544`; F5-55 `04f480c`; **yeni planlar** F5-56 `586dbc2`, F5-57 `d77918b`, F5-58 `df20d1f`; F5-50 (çalışan plan) için yapılan metin değişikliği geri alındı `2856a6c` (duvar bulgusu F5-58'e taşındı); docs/12/15/18 ve takip tablosu ayrı commit'ler.

## 5. Kalan açık noktalar (kısa)

1. Oyun içi doğrulamalar: T-NAV-04/05/06/09/10/11, AC-NAV-03 (F5-55); F4-51/52/54 K10/K9/K8; priest/mage insan hızı (Q-25), T-REGENE-01, T-PERC-01 `BEKLİYOR`.
2. Nav hattı F4 hattıyla birleşmeli (F5-55 öncesi); `NavTrack`/`NavStuck` değişiklikleri F5-56/F5-57 sırasında F5-54'ün adapte metnine bağlı.
3. F8: `db/003` (16 mı 20 mi), `ScenarioReset` DB yazımı yolu, `win_rule` türleri, "respawn kapalı" modu.
4. ADR-0018: önceki raporda listelenen eklemeler (m.6 bölünmesi, party hedefli skill, Type7, FLYING/alan tutarlılığı, stok doldurma ortak sözleşmesi) hâlâ ana hat kararı bekliyor.
