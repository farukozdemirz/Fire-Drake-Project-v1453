# F5-66: Navigasyon çalışma zamanı doğrulama koşusu: engelli rota, iki ulus doğuş → arena dönüşü, takip/takılma, nav payı ve ertelenen sorgu, AC-NAV-03 (koşu aracı `tools/nav-run.py`, rapor/denetim araçları, `ENV` satırı)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-66 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F5-59, F5-60, F5-61, F5-62, F5-63, F5-64, F5-65** `KAPANDI` olmalı (F5-67 yalnızca F5-60 `GEREKLİ` derse; bu plan onu beklemez, ama su kirişi vakası varsa koşuya eklenir). 16 bot ölçümü için ayrıca **F8-03** (`db/005` + `BOT_TABLE` 16 giriş) `KAPANDI` ve `db/005` uygulanmış olmalı (ön koşul, §2 "Ön koşullar"). Şemsiye: F5-55 (dilim 8, **son** dilim). T-NAV-09 (su, insan istemcisi) bu planın **dışındadır** (F5-60 + proje sahibi testi, ayrıca izlenir) |
| İlgili gereksinim / kabul | T-NAV-04 (dar geçit/köprü, 50 geçiş), T-NAV-05/T-NAV-10 (doğuş → arena, iki ulus ayrı), T-NAV-06 (hareketli hedef), T-NAV-11 (16 bot tick), AC-NAV-01 (takılma ≤ 2/bot-saat, kurtarma p95 ≤ 5 sn), AC-NAV-03 (engelli hücreye giren hareket 0), AC-NAV-07 (nav payı p95 ≤ 1,5 ms, bekleme ≤ 1,1 sn), MET-NAV-01/02, MET-PERF-02; `docs/12` §13.4, §13.5.1-§13.5.3, §13.6 (G5); `docs/17` G5; B19 (`docs-measure/celiski-duzeltmeleri.md`: `ENV` satırı) |
| Tahmini büyüklük | M (7 dosya: 3 yeni araç + 4 küçük değişiklik; koşuyu Claude yapar ≈ 2-3 saat, sunucu açık) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak) |

---

## Neden TASLAK

Bu plan, **hiçbiri henüz yazılmamış** altı planın (F5-59..F5-65) ürünü olan sunucu davranışını ölçer; ölçülecek komutlar (`/bot goto`, `/bot follow`), telemetri olayları (`NAV_PATH`, `NAV_STUCK`, `NAV_RECOVERY`, `FAIRNESS_REJECT blocked_chord`) ve `PERF_SAMPLE` nav alanları taslak sözleşmeden alındı. HAZIR yapmak için:

1. **F5-59..F5-65 `KAPANDI`;** her birinin gerçek kodunda şu işaretler doğrulanmış olmalı: `NavService::Instance().Ready()`; `FAIRNESS_REJECT` `rule:"CLI-08"`, `reason:"blocked_chord"` (F5-61); `/bot goto <bot> <x> <z> [speed]` ve `Bot_*.log` satırları `cmd goto: ... planned ...`, `arrived at ... after N packets` (F5-62); `/bot follow <bot> <hedef>`, `follow: stuck ...`, `follow ended (...)` (F5-63); `NAV_PATH` alanları (`kind, sx, sz, gx, gz, len_m, nodes, us, ok, status, cache, reason, wait_ms`), `NAV_STUCK`, `NAV_RECOVERY`, `PERF_SAMPLE` `nav_us_p95/p99/max`, `nav_queries`, `nav_deferred`, `nav_wait_max_ms`, `nav_stale_steps` (F5-64); `bot <b> nav cleared (<reason>)` (F5-65). Alan adları farklıysa araçlar (§5) onlara göre yazılır.
2. **Yazım turunda yeniden doğrulanacak referanslar** (bu taslakta `gece/2026-10-02` @ `9fc2dfe` üzerinde okundu): `tools/run-servers.sh` (`FDP_RUNTIME_DIR`, `status/start/stop`), `tools/auto-loop.sh:147-167` (`stop_requested`, `ensure_servers_stopped`: **döngü her adımda açık sunucuyu kapatır**), `tools/nav-measure/nav_measure.cpp:1099` (`GRID` satırı), `tools/nav-regress.py:42` (bilinen anahtar listesi) ve `tools/nav-regress/good.txt`, `Tests/BotCoreTests/main.cpp`, `tools/bot-telemetry-report.py:226-258, 320, 510-560, 840-850` (olay işleme, `PERF_SAMPLE` pencere toplama, MET-PERF-02 hükmü `p95_worst_us`), `tools/nav-segment-check.py` (bağımsız süpercover + `Walk` maskesi), `docs/12` §13.5.3 (`drafts/docs-measure/olcum-matrisi.md` taslak metni; Claude `docs/12`'ye işlediyse oradan), `docs/15:166-168` (T-NAV-09/10/11 tanımı `:168`), `docs/12:166-173` (T-NAV-01..08 tablosu) ve `T-ENV-ARENA-04` (`docs/15:65-66`).
3. **Karar bekleyen konular (proje sahibi/Claude, HAZIR öncesi):** (a) 16 bot koşusu için F8-03'ün uygulanma zamanı (aşağıda "Ön koşullar"); (b) T-NAV-06 (hareketli hedef/kiting) için **kabul eşiği dokümanda tanımlı değil** (`docs/12:171` yalnızca ad verir): bu plan dağılımı ölçer ve raporlar; hüküm eşiği proje sahibi kararıdır; (c) ölüm indüksiyon yöntemi (§2 "S2").

> **Karar (2026-10-03, ADR-0021, proje sahibi):** T-NAV-06 (hareketli hedef takibi) için **sayısal eşik yoktur**; ilk koşuda yalnızca dağılım raporlanır, eşik ölçüm görüldükten sonra proje sahibiyle belirlenir. S3a hüküm yerine ölçüm yazar (plandaki "karar bekleyen konu b" bu karara göre okunur).

## 1. Amaç

Birim testleri geçen ama oyunda hiç görülmemiş navigasyon modüllerinin **gerçek sunucuda** çalıştığını ölçmek ve **yalnızca ölçülenleri** "doğrulandı" yazmak: (1) tek botun engelli rotayı tamamlaması (engel kesen düz hat ile nav yolu karşılaştırması), (2) **Karus ve El Morad ayrı ayrı** doğuş → arena dönüşü (her ulus ≥ 10 tekrar), (3) hareketli hedef takibi ve takılma senaryoları (T-NAV-04/06), (4) nav payı ve toplam `BotManager::Tick` (T-NAV-11; **16 bot ön koşullu**, aksi halde 12 botla **kısmi** ve 16 için ertelenmiş madde), (5) ertelenen sorguda bot davranışı, (6) AC-NAV-03 (engelli hücreye giriş 0; düz `/bot move` ile CLI-08 reddi). DeepSeek **yalnızca koşu/kayıt araçlarını** yazar (F4-41/F4-42 deseni); **sunucuyu çalıştırmaz, ölçmez, hüküm vermez**: koşuyu Claude etkileşimli doğrulamasında yapar. Ayrıca ölçüm çıktılarına derleyici sürümü ve CPU'yu basan `ENV` satırı (B19) eklenir.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.4 (doğuş → arena, ~52 / ~142 sn), §13.5.3 (ölçüm tanımı), §13.6 (G5: "F5 yalnızca `BotCore` olarak kapanamaz"), `docs/15` T-NAV-04/05/06/10/11 ve T-ENV-ARENA-04 (insan ölçümü: Karus 69,8 sn / 323 m, El Morad 165,1 sn / 731 m; **bot beklentisi** A* yolu ÷ 4,5 m/s: Karus ~52 sn, El Morad ~142 sn ± %20, T-NAV-10).
- **Bot başlangıç konumu:** `db/002_bot_characters.sql:264-270` USERDATA `Zone = 71, PX = 127400, PZ = 89000` (ölçek ×100 ise **(1274,0; 890,0) = arena A**; uygulayıcı/Claude doğrular): botlar arenada doğar. Nation doğuş noktaları: Karus (1369,9; 1090,3), El Morad (630,0; 920,0) (`docs/15:65-66`). **Doğuşa gitmek için ölmek gerekir** (`WIZ_REGENE`); bu yüzden "doğuş → arena" koşusu **ölüm → `/bot regene` → arena** zinciridir (T-NAV-10).
- Bot adları (`GameServer/Bot/BotManager.cpp:60-66` `BOT_TABLE`, 12 giriş): `BotWP_K, BotWG_K, BotPHD_K, BotPHB_K, BotMF_K, BotMI_K` (Karus) ve `..._E` (El Morad). `BOT_TABLE`'ın 12'ye sabit olması 16 bot için F8-03'ü ön koşul yapar (`drafts/f8/F8-03-...md`).
- **Koşu ortamı:** sunucu çalışma dizini `/mnt/c/dev/fdp/server/` (bot günlüğü `Logs/Bot_<g>_<a>_<y>.log`, telemetri `Logs/bots/<tarih>/*.jsonl`, komut dosyası `./BotCommands.txt` [`BotManager.cpp:28`], betikler `./Scripts/<ad>.txt`); `tools/run-servers.sh start|stop|status`; ini `[BOT] ENABLED, NAV, MAX_BOTS, SPAWN_ON_START, TELEMETRY, TICK_MS, SPEEDHACK_CHECK`. Uygulayıcı `GameServer.ini`'nin gerçek yolunu `tools/run-servers.sh` ve F4-42 K7 kalıbından doğrular.
- **`tools/auto-loop.sh` ile çakışma (zorunlu not):** otonom gece döngüsü her adımda `ensure_servers_stopped` ile **açık sunucuları kapatır** (`tools/auto-loop.sh:160-167`; durdurma dosyası `plans/.auto-loop-stop`, `:147`). Bu yüzden bu koşu **otonom döngünün içinde çalışmaz**: yalnızca etkileşimli `/plan-dogrula` oturumunda, döngü durmuşken yapılır (`docs/12` §13.5.3 "Gürültü": ölçüm sırasında `tools/auto-loop.sh` durdurulur). `tools/nav-run.py` döngü çalışıyorsa (`pgrep -f auto-loop.sh`) başlamayı **reddeder** (`--force` yok).
- `tools/bot-telemetry-report.py` MET-PERF-02 hükmü `in_game_max ≤ 16` ise 5000 µs bütçesini **otomatik uygular**: 12 botla koşunda bu araç yanıltıcı biçimde `PASS` basar. `nav-run-report.py` bu koşulu **PARTIAL** olarak etiketler (aşağıda).
- AC-NAV-03 kanıtı **bağımsız** olmalıdır: sunucu guard'ının (F5-61) kendi kararına güvenmek döngüseldir. Telemetride `ACTION_SUBMIT` `Move` (`x`, `z`, `speed`, `echo`) **yalnızca guard'dan geçen** paketler için yazılır (`SubmitMove`: ret `ACTION_SUBMIT`'ten önce döner); bağımsız denetim bu konumlardan kirişleri yeniden kurar ve `tools/nav-export.py` ızgarasında Python süpercover'ı ile sınar.
- **Modül bağlama gerçeği (kullanıcı gereksinimi):** birim testleri geçen modüller `GameServer`'dan çağrılmıyorsa oyun davranışı **tamamlanmış sayılmaz**. Bu plan bağlama durumunu **otomatik** raporlar (§5 adım 4 `wiring`).

### Ön koşullar (kaydedilir, uydurulmaz)

| Koşul | Bugünkü durum `[V]` | Etkisi |
|---|---|---|
| 12 bot karakteri (6 + 6) | DB'de var (`db/002`) | 1-3 ve 5-6 koşulları, 4'ün **kısmi** hali yapılabilir |
| 16 bot karakteri | **Yok:** `db/005` (F8-03) yazılmadı; `BOT_TABLE` 12 giriş | (4) için 16 botlu hüküm verilemez: **ertelenmiş madde** "T-NAV-11 / AC-NAV-07 16 bot: ERTELENDİ (F8-03)". F8-03 `KAPANDI` ve `db/005` uygulanınca koşu **aynı araçla** 16 botla tekrarlanır |
| `[BOT] NAV=1` + `ENABLED=1` + `TELEMETRY=trace` | F5-59/F5-64 sonrası | NAV_* olayları `trace` seviyesindedir (`docs/16` §3.3) |
| Otonom döngü kapalı | Claude doğrulama oturumu | döngü sunucuyu kapatır; ölçüm gürültüsü |

**Durum kuralı:** (4)'ün 16 botlu kısmı yapılmadıkça F5-66 `KAPANDI` yazılabilir **ancak** Doğrulama Raporu'nda `ERTELENDİ (F8-03)` satırı ve `docs/reports/degerlendirme-takip.md` T-NAV-11 / AC-NAV-07 satırı `BEKLİYOR` kalır; F5-55 şemsiyesi **kapanamaz** (ve G5 kabulü verilmez).

### Senaryolar (koşuyu Claude yapar; araçlar bunları yürütür/kaydeder)

Hepsi `ENABLED=1, NAV=1, TELEMETRY=trace`, `[V]` etiketi `[V: ORT-S, <Y>, <senaryo>]` (ölçüm kuralı 9). Başarı eşikleri `[A]`/`[Ö]` olarak kaynakla birlikte yazılıdır.

| Kod | Senaryo | Yöntem | Ölçüt (hüküm) |
|---|---|---|---|
| **S1** | Tek bot, engelli rota (T-NAV-04'ün tek geçişlik kısmı, AC-NAV-03 ön koşusu) | `nav-run.py routes` ızgaradan arena A'dan 60-200 m uzakta, düz doğrunun **≥ 3 engelli hücreye değdiği** ve A* yolunun düz mesafenin 1,15-2,5 katı olduğu bir hedef seçer; bot `/bot goto` ile gider; sonra aynı doğrultuda `/bot move` (düz) denenir | `goto`: varır (≤ 1 m), süre ≤ `route_m / 4,5 × 1,15 + 10` sn `[A]`, tüm paket kirişleri `Walk` (denetim aracı), `blocked_chord` 0; düz `/bot move`: ilk adım `FAIRNESS_REJECT` `blocked_chord` ile **reddedilir**, bot yerinde; karşılaştırma tablosu: düz mesafe, engelli hücre sayısı, nav yolu uzunluğu, oran, süre |
| **S2K / S2E** | **Karus ve El Morad ayrı ayrı:** ölüm → `regene` → doğuş → arena A (T-NAV-05, T-NAV-10) | Döngü (her ulusta **≥ 10 tekrar**): kurban (K: `BotWP_K`; E: `BotWP_E`) arena A'da; karşı ulus saldırgan bot yakında `attack` ile onu öldürür (F4-02/F4-07 koşu kalıbı); ≥ 4 sn sonra `regene` (CLI-14); `goto 1275 890`; varışta bir sonraki döngü. Ulus başına ayrı rapor | her tekrarda: `NAV_PATH ok:true` (`NodeLimit`/`InvalidGoal`/`no_path` **0**), süre ∈ ulus bandı (Karus 52 sn, El Morad 142 sn **± %20**: `[41,6; 62,4]` / `[113,6; 170,4]`) **ve** ±%10 `route_m / 4,5`; varışta arena merkezine ≤ `P-ARENA-R` = 60 m; `NAV_STUCK` 0; `blocked_chord` 0; kurban ölüm sonrası `nav cleared (dead)` ve respawn sonrası komutsuz 20 sn hareket yok (F5-65 K9-K10) |
| **S3a** | Hareketli hedef takibi (T-NAV-06) | `BotMF_K` hedef (komut sürücüsü 6-10 sn'de bir yeni `goto` hedefi: arena içinde `near64`), `BotWP_K` ve `BotMI_K` `/bot follow` ile izler; 10 dk | **dağılım raporlanır** (bot-hedef mesafesi p50/p95, yeniden plan sayısı, `NAV_STUCK`); hüküm eşiği tanımlı değil (karar bekleyen konu b); `blocked_chord` 0 |
| **S3b** | Takılma (T-NAV-04: dar geçit/köprü) | `nav-run.py routes --narrow` ızgarada `clearance ≤ 1` olan bir dar geçit/köprü seçer (Claude doğrular); 4 bot **toplam ≥ 1,0 bot-saat** (ör. 15 dk × 4) geçidin iki yanı arasında `goto` ile **toplam ≥ 50 geçiş** | `NAV_STUCK` oranı = olay / bot-saat **≤ 2** (AC-NAV-01/MET-NAV-01) ve **örneklem uyarısıyla** raporlanır; `NAV_RECOVERY` `recovered` süresi p95 ≤ 5000 ms (MET-NAV-02); `Abandon` sayısı ve nedenleri listelenir |
| **S4** | Nav payı ve toplam `Tick` (T-NAV-11, MET-PERF-02, AC-NAV-07) | Tüm botlar yoğun yük: `follow`/`goto` hedef değişimi 6-10 sn, **ve** 5 dk "yoğun sorgu" fazı (tüm botlar tek hedefi izler, hedef ≥ 6 m sıçrar); ≥ 30 dk (ilk 60 sn ısınma, hükme girmez); **3 koşu**, en kötüsü raporlanır; taban koşuları: `MAX_BOTS=0` ve `NAV=0` (aynı botlar düz `/bot move`) | `PERF_SAMPLE` geçerli pencerelerde (`skipped_ticks = 0`, `dropped_* = 0`): `nav_us_p95 ≤ 1500`, `nav_wait_max_ms ≤ 1100`, `tick_p95_us ≤ 5000`, `in_game = N`. **N = 16 değilse hüküm `PARTIAL (N bot)`**; AC-NAV-07/MET-PERF-02/T-NAV-11 "kapandı" **yazılmaz** |
| **S5** | Ertelenen sorguda davranış (F5-64) | S4'ün yoğun fazında `nav_deferred > 0` olan pencereler seçilir; bot kayıtları taranır | `nav_stale_steps = 0` (tüm pencereler); ilk plan beklemesi (`NAV_PATH.wait_ms` en büyüğü / `nav_wait_max_ms`) ≤ 1100; ertelenen botların `ACTION_SUBMIT` `Move` paketleri `echo:3` ile sürer (düz çizgi/durma-başlama yok): ertelenme penceresinde paket oranı raporlanır. **`nav_deferred = 0` ise S5 `KANITLANAMADI` yazılır** (ertelenme hiç oluşmadı) |
| **S6** | AC-NAV-03 (30 dk, tüm hareket) | Tüm koşuların telemetrisi `nav-move-audit.py` ile bağımsız denetlenir; S1'de düz `/bot move` reddi | engelli hücreye giren kiriş **0**; toplam hareket süresi ≥ 30 dk (aksi halde `YETERSİZ SÜRE`); reddedilen kiriş sayısı (yalnızca S1 düz hat) ayrı |

## 3. Kapsam

**Yapılacaklar (DeepSeek: yalnız araçlar; sunucu çalıştırma yok)**

1. `tools/nav-run.py` (yeni, yalnızca standart kütüphane): koşu sürücüsü (`routes`, `run`, `env`, `--selftest`).
2. `tools/nav-run-report.py` (yeni): koşu çıktısından senaryo hükümleri, `wiring` tablosu, `--selftest`.
3. `tools/nav-move-audit.py` (yeni): AC-NAV-03 bağımsız kiriş denetimi, `--selftest`.
4. **B19 `ENV` satırı:** `tools/nav-measure/nav_measure.cpp` (`GRID` satırından sonra), `Tests/BotCoreTests/main.cpp` (test çalıştırıcı başında), `tools/nav-regress.py` (`ENV` bilgi anahtarı olarak bilinir), `tools/bot-telemetry-report.py` (nav payı tablosu ve ORT-S ortam satırı).
5. Çalışma zamanı koşusu **Claude'un** (K9-K19).

**Kapsam dışı (yapılmayacak)**

- **Hiçbir** `GameServer/`, `BotCore/`, `shared/` kodu (F5-59..F5-65'in işi); `Nav*.h` değişmez; sunucuyu başlatma/durdurma/ölçme/hüküm verme (DeepSeek yapmaz).
- T-NAV-09 (su; insan istemcisi), T-NAV-01/02 (hız/eğim kalibrasyonu, insan), T-NAV-07 (ulaşılamaz hedef), T-NAV-08 (formasyon), T-NAV-LOS-01: bu planda yok.
- Arena **sınırının** sunucu bağlaması (`NavCostLayer::AddForbidOutsideDisc`), `NavReach`, `NavDanger`, `NavFormation`, `NavRetreat`, `NavLos` sunucu bağlamaları: F5-59..F5-66'nın **hiçbirinde yok** (aşağıda `wiring` "BAĞLI DEĞİL" olarak listelenir; F5 kabulünde ayrıca ele alınır).
- Veritabanı değişikliği (`db/005` F8-03), `GameServer.ini` kalıcı değişikliği (koşu aracı ini'yi yedekler ve **birebir** geri yükler).
- `docs/` (Claude: `docs/12` §13.1/§13.4/§13.5.3, `docs/15`, `docs/reports/degerlendirme-takip.md`, `docs/STATUS.md`, ADR).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/nav-run.py` | yeni | yalnızca standart kütüphane; ASCII; `--selftest` |
| `tools/nav-run-report.py` | yeni | yalnızca standart kütüphane; ASCII; `--selftest` |
| `tools/nav-move-audit.py` | yeni | bağımsız (`nav-run-report.py`'den **içe aktarma yok**); `tools/nav-segment-check.py` mantığı **kopyalanır**; `--selftest` |
| `tools/nav-measure/nav_measure.cpp` | değiştir | yalnızca `ENV` satırı (F5-60 `water` bölümüyle çakışma riski: `drafts/nav-b/F5-bagimlilik-ozeti.md`) |
| `Tests/BotCoreTests/main.cpp` | değiştir | yalnızca `ENV` satırı |
| `tools/nav-regress.py` | değiştir | yalnızca `ENV` anahtarını bilinen/bilgi anahtarı listesine ekleme (`:42`); `tools/nav-regress/good.txt` **değişmez** |
| `tools/bot-telemetry-report.py` | değiştir | yalnızca nav payı tablosu + ENV satırı; mevcut MET-PERF-02 çıktısı ve `--selftest` değişmeden geçer |

7 dosya + plan (3 yeni). Bu listede olmayan bir dosyaya dokunmak gerekirse (ör. `Scripts/*.txt` betikleri, `bots/config/*`) **durup** Uygulayıcı Raporu'nda soru olarak yaz: koşu betikleri `nav-run.py` içinde üretilir (dosya eklemeden).

## 5. Uygulama adımları

1. `git switch -c bot/F5-66 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucu `[UP]` ise `./tools/run-servers.sh stop` (yalnızca derleme kilidi için). **Sunucu çalıştırma ve gerçek koşu uygulayıcıya düşmez.** F5-59..F5-65 kodundaki olay/alan adlarını `git grep` ile doğrula ve rapora yaz.
2. **`tools/nav-move-audit.py`** (AC-NAV-03): girdi telemetri JSONL dizini/dosyaları (`TELEMETRY ≥ decisions`) + `build/nav/zone71.navgrid` (yoksa `tools/nav-export.py` ile üretir; `--navgrid`). Mantık: bot başına `ACTION_SUBMIT` (`type:"Move"`) ve eşleşen `ACTION_RESULT` (`ok:true`) kayıtlarından konum zinciri kur (`x`, `z`; durma paketi `speed:0`); **zincir kopar:** `ACTION_RESULT` `ok:false`, `Regene` `reason:"respawned"` (yeni başlangıç), `DEATH`, bot adının yeniden `spawn` olması, iki paket arası > 4 × 1,5 sn (bilinmeyen sunucu hareketi); ilk paketin kirişi **denetlenmez** (`unchecked_first` sayılır). Her ardışık çift için `Walk` süpercover'ı (Amanatides-Woo, vertex/köşe dahil, `tools/nav-segment-check.py` mantığı) ile engelli hücre sayısı. Çıktı: bot başına `chords`, `moving_s`, `blocked`, `unchecked_first`; ihlal listesi (zaman, bot, (x0,z0)→(x1,z1), hücre); `AUDIT total_chords=<n> moving_min=<m> blocked=<b>`; çıkış 0 (b = 0) / 1 (b > 0) / 2 (girdi hatası). `--selftest`: sentetik ızgara, **bilerek yerleştirilmiş** engelli kiriş **yakalanır** (negatif kontrol), temiz zincirde 0, zincir kopma kuralları, kısa süre `YETERSİZ SÜRE`, bozuk satır atlanır.
3. **`tools/nav-run.py`:**
   - `routes`: ızgaradan (`--navgrid`) S1 için (başlangıç arena A (1274; 890), hedef: 60-200 m, düz doğru ≥ 3 engelli hücreye değiyor, A* yolu 1,15-2,5 × düz) ve `--reject-vector` için (**ilk paket kirişi engelli hücreye değen** düz hedef, 6,75 m içinde), `--narrow` için dar geçit (`clearance ≤ 1` hücrelerden geçen iki uç nokta) **adaylarını listeler** (Claude seçer/doğrular). Python A* (`heapq`, ≤ 20 000 düğüm) kullanır; bağımsız olmak için `NavPathfinder` çıktısına bakmaz.
   - `run <senaryo>` (`s1|s2k|s2e|s3a|s3b|s4`; `--bots N`, `--cycles N`, `--duration S`, `--runtime-dir`, `--out`): (a) **ön denetim:** `pgrep -f auto-loop.sh` çalışıyorsa **reddet** (çıkış 2); `./tools/run-servers.sh status` beklenmedik `[UP]` ise reddet; (b) `GameServer.ini` **yedekle**, `[BOT]` anahtarlarını yaz (`ENABLED=1, NAV=1, MAX_BOTS, SPAWN_ON_START, TELEMETRY=trace`), bitişte **birebir geri yükle** (bayt karşılaştırması; sunucu çökerse de `finally`); (c) `run-servers.sh start`, `Bot_*.log` içinde botların `in game` satırlarını bekle (zaman aşımı); (d) senaryo komutlarını `BotCommands.txt`'ye zaman çizelgesiyle yaz ve **günlük satırlarına tepki veren** durum makinesiyle (`arrived`, `dead`/`move stopped (dead)`, `respawned`, `follow ended`) ilerlet (S2: kurban `goto` → varış → `attack` (saldırgan) → ölüm → ≥ 4 sn → `regene` → `goto`; ölüm gelmezse zaman aşımı ve tekrar kaydı); (e) `events.jsonl`'a (`ts`, `senaryo`, `tekrar`, `bot`, `olay`, `x`, `z`) her adımı yaz; (f) bitişte `run-servers.sh stop`, günlükleri/telemetriyi `build/nav-run/<ts>-<senaryo>/` altına **kopyala** (kişisel veri yok: yalnız bot günlükleri; kimlik doğrulama belirteci kopyalanmaz: `Bot_*.log` ve `bots/**/*.jsonl` dışında dosya alınmaz). Her adım zaman aşımlı; hata durumunda ini geri yüklenir ve sunucu durdurulur.
   - `env`: tek satır `ENV` (aşağıda).
   - `--selftest`: sahte `GameServer.ini` ve sahte `BotCommands.txt` ile ini yedek/geri yükleme (bayt eşitliği), `auto-loop` algılama (`pgrep` taklidi), durum makinesi (sahte günlük akışı: ölüm gelmez/geç gelir/zaman aşımı), `routes` sentetik ızgarada. **Gerçek sunucu çağrılmaz** (`run-servers.sh` taklit edilir).
   - `ENV` satırı: `ENV host=<hostname> cpu="<model name>" nproc=<n> ram_gib=<x> gxx="<g++ --version ilk satır>" server_linker=<GameServer.exe PE MajorLinkerVersion.MinorLinkerVersion> server_pe_time=<TimeDateStamp UTC> tests_linker=<BotCoreTests.exe aynı>` (PE başlığı `struct` ile okunur; MSVC araç seti sürümü **ikili dosyadan** türetilir, çıkarım değildir; `cl` 19.xx sürümü **yazılmaz**, ancak PE'den okunamazsa `unknown`).
4. **`tools/nav-run-report.py`:** girdi bir koşu dizini (`build/nav-run/<ts>-<senaryo>/`: `Bot_*.log`, `*.jsonl`, `events.jsonl`) ve senaryo adı; çıktı: senaryo tablosu ve hüküm satırları `S1 PASS|FAIL|YETERSIZ`, ... Kurallar (§2 tablosundaki eşikler **sabit** ve çıktıda yazılı): `S2*` her tekrar için süre/rota/`NAV_STUCK`/`NodeLimit` ve ulus bandı, ≥ 10 tekrar yoksa `YETERSIZ`; `S3b` bot-saat ve örneklem uyarısı; `S4` geçerli pencere filtresi (`skipped_ticks = 0`, `dropped_* = 0`, ilk 60 sn ısınma ayrı), 3 koşunun **en kötüsü**, `in_game_max` < 16 ise hüküm **`PARTIAL (<N> bot)`** (asla `PASS`), taban koşularla fark (nav'ın gerçek maliyeti); `S5` `nav_deferred = 0` ise `KANITLANAMADI`; `S6` `nav-move-audit.py` çıktısını okur; **`wiring`** alt komutu: `git grep` ile `GameServer/` içinde şu sembollerin (testler hariç) kullanımını sayar ve tablo basar: `NavService`, `CheckMoveChord`/`NavChordGuard`, `NavPathfinder`, `NavSmoothPath`, `NavFollower`, `NavProgressAssessor`, `NavStuckMonitor`, `NavStuckPenalties`, `NavQueryScheduler`, `NavPathCache`, `NavDrive`, ve **bağlı olması beklenmeyen** `NavReach`, `NavCostLayer`/`AddForbidOutsideDisc` (yalnız ceza katmanı dışında), `NavFormation`, `NavRetreat`, `NavLos` → `BAGLI` / `BAGLI DEGIL`. Her `BAGLI` için çalışma zamanı kanıtı olay adı (ör. `NAV_PATH`, `blocked_chord`) koşu çıktısında **görüldü/görülmedi** işaretlenir; `BAGLI` ama çalışma zamanında hiç görülmeyen modül `KANITSIZ` basılır. `--selftest`: sentetik günlük/telemetri ile her senaryonun `PASS`/`FAIL`/`YETERSIZ`/`PARTIAL`/`KANITLANAMADI` dalı ve negatif kontroller (12 botla `PASS` **basılamaz**).
5. **`ENV` satırları (B19):** `nav_measure.cpp`: `GRID` satırından hemen sonra `ENV compiler=<__VERSION__ veya _MSC_FULL_VER> ptr_bits=<8*sizeof(void*)> cpu="<Linux: /proc/cpuinfo model name>" nproc=<std::thread::hardware_concurrency()>` (Linux dışı `cpu="unknown"`); `Tests/BotCoreTests/main.cpp`: koşu başında `ENV compiler=msvc <_MSC_FULL_VER> ptr_bits=<..> config=Release|Debug cpu="<__cpuid marka dizesi>"`. `nav-regress.py`: `ENV` satırı **bilgi (I)** sınıfı, hükmü etkilemez; `tools/nav-regress/good.txt` ve `--selftest` değişmez. `bot-telemetry-report.py`: MET-PERF-02 tablosundan sonra nav payı tablosu (`nav_us_p95` en kötü pencere, `nav_queries`, `nav_deferred`, `nav_wait_max_ms` en büyük, `nav_stale_steps` toplam; nav alanı yoksa tablo yazılmaz ve çıktı **bugünküyle aynı**) ve `ENV` satırı yoksa uyarı.
6. Derle ve test et (`Tests/BotCoreTests/main.cpp` değiştiği için), araç `--selftest`'leri, `python3 -m py_compile`. Uygulayıcı Raporu; `Durum` → `UYGULANDI`. **Gerçek koşu Claude'un (K9-K19).**

## 6. Kabul kriterleri

DeepSeek kriterleri (K1-K8) ve Claude çalışma zamanı kriterleri (K9-K19) **ayrıdır**; K9-K19 yapılmadan F5-66 `DOĞRULANDI` yazılmaz.

- [ ] K1: `python3 tools/nav-move-audit.py --selftest`, `python3 tools/nav-run-report.py --selftest`, `python3 tools/nav-run.py --selftest` her biri rc=0; selftest çıktısı **negatif kontrolleri** gösterir (audit planted blocked chord'u yakalar; report 12 botta `PASS` basmaz, `nav_deferred = 0`'da `KANITLANAMADI`; run auto-loop varken reddeder, ini'yi bayt bayt geri yükler)
- [ ] K2: `python3 -m py_compile tools/nav-run.py tools/nav-run-report.py tools/nav-move-audit.py tools/bot-telemetry-report.py tools/nav-regress.py` rc=0; `tools/bot-telemetry-report.py --selftest` ve `tools/nav-regress.sh --selftest` değişmeden PASS
- [ ] K3: `./tools/build.sh Release` ve `Debug` rc=0, yeni uyarı yok; `./tools/run-tests.sh Release` ve `Debug` `0 failed`; çıktıda tam bir `ENV compiler=msvc ...` satırı (her yapılandırmada)
- [ ] K4: `tools/nav-measure.sh all 2>/dev/null | grep -c '^ENV '` = 1 ve `GRID` satırı bayt bayt aynı; `tools/nav-regress.sh --from-file tools/nav-regress/good.txt` PASS, `tools/nav-regress/good.txt` farkı **0**
- [ ] K5: `git diff --stat gece/2026-10-02...bot/F5-66` yalnızca §4 (7 dosya, 3 yeni) + plan; **`GameServer/`, `BotCore/`, `shared/`, `Tests/` (main.cpp dışında), `docs/`, `db/`, `.vcxproj` farkı 0**; `git diff --check` boş
- [ ] K6: `python3 tools/nav-run-report.py wiring` çıktısı raporda; her satır `BAGLI`/`BAGLI DEGIL`; beklenen BAGLI listesi (F5-59..F5-65 modülleri) hepsi `BAGLI`, beklenen `BAGLI DEGIL` listesi (`NavReach`, `NavFormation`, `NavRetreat`, `NavLos`, arena yasaklı-disk) açıkça yazılı; farklıysa **sapma** olarak rapora
- [ ] K7: `nav-run.py` hiçbir yerde `GameServer/`/`BotCore/` dosyası yazmaz; `run-servers.sh stop`'u `finally` ile her çıkış yolunda çağırır; kopyalanan dosyalar yalnızca `Bot_*.log` ve `bots/**/*.jsonl` (kişisel veri kuralı; `grep` kanıtı)
- [ ] K8: yeni dosyalar yalnızca ASCII; `file` çıktısı raporda; araç başlıklarında kullanım, çıkış kodları ve "sunucu çalıştırma yalnız Claude doğrulamasında" notu
- [ ] K9 (Claude, çalışma zamanı): ön koşul kaydı: `ENV` satırı (`nav-run.py env`), `GameServer.exe` derleyici sürümü (PE), `NAV=1`, `TELEMETRY=trace`, **otonom döngü durdurulmuş** (`pgrep` boş), açık bot sayısı; sonuçlar etiketli `[V: ORT-S, ...]`
- [ ] K10 (Claude): **S1** hükmü `PASS` (varış, `blocked_chord` 0, düz `/bot move` reddi **görüldü**, karşılaştırma tablosu raporda)
- [ ] K11 (Claude): **S2K** (Karus) ≥ 10 tekrar `PASS` (her tekrar süre bandı + `route_m/4,5` ±%10, takılma 0, `NodeLimit/InvalidGoal` 0, arenada ≤ 60 m); ulus bazlı ayrı tablo
- [ ] K12 (Claude): **S2E** (El Morad) ≥ 10 tekrar `PASS`; ayrı tablo; iki ulus **ayrı** raporlanır (birleştirilmez)
- [ ] K13 (Claude): **S3a** dağılım raporu (eşik yok: hüküm yerine **ölçüm** yazılır); **S3b** `NAV_STUCK` ≤ 2/bot-saat ve p95 kurtarma ≤ 5 sn **veya** bulgu listesi (`FAIL` ise nedeni ve düzeltme planı Claude'da)
- [ ] K14 (Claude): **S4 (12 bot)** `PARTIAL (12 bot)` hükmü ve sayılar (nav payı p95/p99/max, `nav_wait_max_ms`, `tick_p95_us`, taban farkı); **AC-NAV-07/MET-PERF-02/T-NAV-11 kapandı YAZILMAZ**
- [ ] K15 (Claude, **ÖN KOŞUL: F8-03**): **S4 (16 bot)** `PASS`/`FAIL` hükmü (3 koşu en kötüsü; geçerli pencereler); F8-03 yoksa bu kriter `ERTELENDİ (F8-03)` olarak kaydedilir ve F5-55 şemsiyesi kapanmaz
- [ ] K16 (Claude): **S5** `nav_stale_steps = 0` ve ilk plan beklemesi ≤ 1100 ms ve `nav_deferred > 0` pencereleri **bulundu**; bulunmadıysa `KANITLANAMADI` (kriter karşılanmış sayılmaz)
- [ ] K17 (Claude): **S6** `AUDIT ... blocked=0` ve `moving_min ≥ 30`; `YETERSIZ SÜRE` ise koşu uzatılır
- [ ] K18 (Claude): **Bağlama dürüstlüğü:** `wiring` tablosunda `KANITSIZ` satır **yok**; `BAGLI DEGIL` modüller `docs/12` §13.6/G5 notuna ve `docs/STATUS.md`'ye "sunucuda bağlı değil" olarak işlenir; hiçbir `BAGLI DEGIL` modül "oyun davranışı tamamlandı" diye kapatılmaz
- [ ] K19 (Claude): `docs/reports/degerlendirme-takip.md` satırları **ayrı sütunlarda** güncellenir: plan hazır → kod uygulandı → **oyun içinde doğrulandı** (yalnız K10-K17'de kanıtlananlar); T-NAV-09 satırı **dokunulmadan** `BEKLİYOR`; sunucu `stop` ile kapanır, `GameServer.ini` yedekten birebir döner

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
python3 tools/nav-move-audit.py --selftest && python3 tools/nav-run-report.py --selftest && python3 tools/nav-run.py --selftest
python3 -m py_compile tools/nav-run.py tools/nav-run-report.py tools/nav-move-audit.py tools/bot-telemetry-report.py tools/nav-regress.py
python3 tools/bot-telemetry-report.py --selftest && tools/nav-regress.sh --selftest
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "^ENV |tests,"
./tools/run-tests.sh Debug 2>&1 | grep -E "^ENV |tests,"
tools/nav-measure.sh all 2>/dev/null | grep -E "^(GRID|ENV) "
python3 tools/nav-run-report.py wiring
git diff --stat gece/2026-10-02...bot/F5-66
git diff --check gece/2026-10-02...bot/F5-66
# Claude (çalışma zamanı, döngü durmuşken): touch plans/.auto-loop-stop ; python3 tools/nav-run.py env ;
#   python3 tools/nav-run.py run s1 ; run s2k --cycles 10 ; run s2e --cycles 10 ; run s3a ; run s3b ; run s4 --bots 12
#   python3 tools/nav-run-report.py build/nav-run/<ts>-s2k ... ; python3 tools/nav-move-audit.py <jsonl dizinleri>
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2.7: DeepSeek sunucu çalıştırmaz ve oyun verisi üretmez; **kişisel veri kuralı**: yalnızca `Bot%` hesap/karakter günlükleri ve telemetrisi kopyalanır, kimlik doğrulama belirteçleri teslim dosyalarına **kopyalanmaz** (`Bot_*.log` ve `bots/**/*.jsonl` bu tür veri taşımaz; araç yine de `grep` ile kontrol eder).
- **Bot avantajı yasağı:** koşu araçları bota **hiçbir** yetenek vermez; test sürücüsü yalnızca mevcut `/bot` komutlarını (`goto`, `follow`, `attack`, `regene`, `move`, `stop`) yazar. Ölüm indüksiyonu karşı ulus botunun **gerçek** saldırısıdır (HP/konum yazma, teleport yok); `TEST_TELEPORT` kullanılmaz (AC-NAV-05).
- **Dürüstlük (kapsam):** (i) bu koşu **12 botla** yapılabilir; 16 botlu hüküm yoktur ve yazılmaz. (ii) 10 tekrar bir **örneklem**dir: p95/p99 küçük n'de zayıf; rapor `n`'yi yazar. (iii) süre bantları `[A]`: `docs/15` T-NAV-10 beklentisi A* yolu ÷ 4,5 m/s idi; ölçüm bunu **sınar**, insan rotası (T-ENV-ARENA-04) bundan %8-24 uzundur. (iv) Birim testlerinin geçmesi **hiçbir** satırı "oyun içinde doğrulandı" yapmaz (`docs/reports/degerlendirme-takip.md`). (v) `wiring` tablosu **statik** `git grep` ve olay-adı **görülme** denetimidir; bir modülün **doğru** çalıştığının kanıtı yalnızca senaryo hükümleridir.
- Ölçüm gürültüsü: paralel derleme/otonom döngü yokken; 3 koşu, en kötüsü; ilk 60 sn ısınma ayrı raporlanır (`docs/12` §13.5.3).
- `ENV` satırı derleyici sürümünü **çıkarım olmadan** yazar: MSVC için PE bağlayıcı sürümü (`14.44` gibi), `cl` 19.xx **yazılmaz** (kayıtta yok: `docs-measure/celiski-duzeltmeleri.md` C bölümü); WSL `g++` sürümü o günün `g++ --version`'ıdır (geçmiş koşular için doğrulanamaz).
- Çakışma uyarısı: `tools/nav-measure/nav_measure.cpp` F5-60 ile (`water` bölümü) aynı dosyadır; ardışık uygulanmalı.
- Beklenmedik durumda (F5-59..F5-65 olay/alan adları farklı, ek dosya gerekiyor, `GameServer.ini` yolu bulunamıyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-66` — `<kısa-sha> [F5-66] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ … (K9-K19 Claude'un çalışma zamanı kriterleri; DeepSeek sunucu çalıştırmadı)
- F5-59..F5-65 olay/alan adı doğrulaması (`git grep` çıktıları): …
- `wiring` tablosu: …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ / ERTELENDİ (F8-03)
- İncelenen: `gece/2026-10-02...bot/F5-66` @ `<sha>`
- `ENV` satırı ve koşu ortamı: …
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ / ✘ | dosya:satır / komut çıktısı |

- Senaryo hükümleri (S1, S2K, S2E, S3a, S3b, S4 [12/16 bot], S5, S6): …
- Bulgular (önem sırasıyla):
  1. …
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
…
```
