# F5-55: Navigasyon sunucu entegrasyonu ve gerçek haritada doğrulama (`NavService`, kiriş guard'ı, `/bot goto`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5: gerçek harita) |
| Branch | `bot/F5-55 (taban: gece/2026-10-02-nav)` — F4 hattıyla birleşmeden sonra yeniden tabanlanır (aşağıda) |
| Bağımlı olduğu planlar | F5-01..F5-07 (`KAPANDI`), nav hattı F5-08..F5-10 (formasyon, takılma kurtarma, LoS), **F5-50** (kiriş denetimi), **F5-51** (arena/doğuş), **F5-52** (hız), **F5-53** (bütçe), **F5-54** (takılma tespiti) `KAPANDI` olmalı; F4 hattının `ActionExecutor`/`BotFairnessGuard` kodu (nav hattı yalnızca `BotCore`'dadır: `gece/2026-10-02-nav` F4-15 öncesinden ayrılmıştır) |
| İlgili gereksinim / kabul | CLI-08, AC-NAV-01..07, T-NAV-04/05/09, `docs/12` §13.6, MET-NAV-01/02, MET-PERF-02/03; DEG-18/20/21 |
| Tahmini büyüklük | L (birden çok plana bölünecek: aşağıdaki dilimler) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Neden TASLAK ve amaç

F5 bugüne kadar yalnızca saf mantık (`BotCore/Nav*.h`, birim testleri) olarak ilerliyor; GameServer'a bağlı **hiçbir** navigasyon yok, yürünebilirlik koruması (CLI-08) icrada yok ve hiçbir şey gerçek haritada oyun içinde sınanmadı. `docs/12` §13.6: F5 yalnızca `BotCore` olarak **kapanamaz**. Bu belge, G5 kapısının sunucu tarafı işini dilimlere böler; plan HAZIR yapılmadan önce (F5-08..F5-10 ve F5-50..F5-54 bitince, ve nav/F4 hatları birleşince) her dilim ayrı küçük plan olarak yazılır:

1. **Bağlama (ilk `.cpp` modülü):** `GameServer`'ın `BotCore.lib`'e bağlanması (ADR-0017'deki "ilk `.cpp` modülü (F5)" ertelemesi), `GameServer/Bot/NavService.{h,cpp}`: açılışta `./Nav/zone71.navgrid` (veya SMD'den çalışma zamanı üretimi) yükleme, `NavGrid::Build`, `NavReach::Build`, takım bölge katmanları, `NavPathfinder` örneği (paylaşım kararı F5-53 ölçüm notuyla), `[BOT] NAV=1` ini anahtarı (varsayılan **0**; `ENABLED=0` davranışı değişmez).
2. **Kiriş guard'ı:** `BotFairnessGuard` hareket kuralına CLI-08 `blocked_chord` eklenir (`NavCheckStep`, F5-50); reddedilen paket gönderilmez, `FAIRNESS_REJECT` rule `CLI-08`; düz `/bot move` da korunur.
3. **Yol izleme:** `ActionExecutor` hareket yürütücüsüne waypoint zinciri (ara noktada durma paketi göndermeden devam), `/bot goto <bot> <x> <z>` ve `/bot follow <bot> <hedef bot>` test sürücüleri, `NavFollower` + bütçe zamanlayıcısı (F5-53) + takılma tespiti (F5-54) bağlama, `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` telemetrisi (`docs/16` §3.2).
4. **Çalışma zamanı doğrulaması (Claude + insan):** T-NAV-04 (dar geçit/köprü 50 geçiş: takılma oranı), T-NAV-05 (doğuş → arena yürüme süresi: Karus ~52 sn, El Morad ~142 sn ± %20; ADR-0033-DEG), **T-NAV-09 (yeni): su ve göl kıyısı** (insan istemcisi suya girebiliyor mu, sunucu olay ızgarasıyla uyum; Q-26), T-NAV-02 (eğim kalibrasyonu), AC-NAV-03 (engelli hücreye giren hareket = 0; sunucu tarafı denetim logu), AC-NAV-01/02/04/06/07.

## 2. Ön koşul ve dikkat edilecekler

- **Hat birleşmesi:** `gece/2026-10-02-nav` F4-14 sonrasından (`86c5761`) ayrılmıştır ve F4'ün `Perception`/`ActionExecutor` ilerlemelerini içermez. Bu plan yazılırken nav hattı F4 hattıyla birleşmiş olmalı (proje sahibi kararı); aksi halde plan, F4 tabanında ayrı bir dal olarak, `BotCore/Nav*.h` dosyalarını birleştirerek başlar.
- **Perception bağlantısı:** `NavFollower.ObserveTarget` girdisi `UnitView` konum/hız/yaş alanlarındandır (F4-50); hedef `in_region=false`/`pos_state=lost` iken takip kararı karar katmanındadır (`docs/13` §5.2a). NaN/∞ ve eski gözlem süzülür.
- **Arena modu:** `AddForbidOutsideDisc` yalnızca senaryo `arena:` tanımlıyken; doğuş → arena yolu önbellekten (F5-53); geri çekilme güvenli noktası arena içinde (ADR-0033-DEG).
- **Bütçe:** nav toplamı tick başına ≤ `P-NAV-TICK-BUDGET-MS` (1,5 ms), MET-PERF-02 toplam p95 ≤ 5 ms; telemetri `PERF_SAMPLE`'a nav payı eklenir.
- **Sözleşme:** nav, sunucu `Map`/`SMD` verisini yalnızca başlangıçta bir kez **kendi ızgarasına** aktarır (harita dosyası = herkesin sahip olduğu zone verisi; oyuncu istemcisi de aynı haritayı bilir, `docs/12` §1); çalışma zamanında sunucu dizilerine erişim yok (`tools/check-perception-contract.py` R1-R5 PASS).

## 3. Kabul kriterleri (taslak; her dilim planında somutlaşır)

- [ ] `./tools/build.sh Release|Debug` rc=0; `./tools/run-tests.sh` `0 failed`; `[BOT] NAV=0` ve `ENABLED=0` davranışı değişmez
- [ ] Çalışma zamanı: `/bot goto` ile 3 bot arena A'ya zone 71'de yürür; engelli hücreye giren hareket **0** (sunucu denetim logu, 30 dk); `FAIRNESS_REJECT CLI-08` düz hedef adımında engeli keserken planlayıcı yollarında **0**
- [ ] T-NAV-04 takılma ≤ 2/bot-saat, kurtarma p95 ≤ 5 sn; T-NAV-05 süreler ±%20; T-NAV-09 sonucu docs/12 §13.1 ve Q-26'ya işlenir
- [ ] AC-NAV-07: 16 bot yoğun sorgu yükünde nav toplamı p95 ≤ 1,5 ms/tick (MSVC Release), hiçbir bot 1 sn'den uzun yol beklemez
- [ ] Faz kabulü ayrı: G5 (`docs/17` §5) proje sahibinin onayıyla

## 4. Açık sorular

- `NavPathfinder` bot başına (≈ 4,2 MB × 16) mı, paylaşılan tek örnek mi (IOCP tek thread'inde seri çağrı güvenli; F5-53 ölçüm notu)?
- Izgara dosyasının üretimi/yeri (`Nav/zone71.navgrid` deploy mı, açılışta SMD'den üretim mi)?
- Su/göl: T-NAV-09 sonucu ayrı bir `water` katmanı gerektiriyor mu?
