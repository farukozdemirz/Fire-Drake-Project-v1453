# F5-55: Navigasyon sunucu entegrasyonu ve gerçek haritada doğrulama (`NavService`, kiriş guard'ı, `/bot goto`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5: gerçek harita) |
| Branch | `yok (şemsiye plan: kod yazılmaz; dilimler kendi branch'inde: bot/F5-59 .. bot/F5-67, taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F5-01..F5-07 (`KAPANDI`), nav hattı F5-08..F5-10 (formasyon, takılma kurtarma, LoS), **F5-50** (kiriş denetimi), **F5-51** (arena/doğuş), **F5-52** (hız), **F5-53** (bütçe), **F5-54** (takılma tespiti) `KAPANDI` olmalı; F4 hattının `ActionExecutor`/`BotFairnessGuard` kodu (nav hattı yalnızca `BotCore`'dadır: `gece/2026-10-02-nav` F4-15 öncesinden ayrılmıştır); **dilimler F5-59 .. F5-67** (§1A) — şemsiye, hepsi KAPANDI olunca (F5-67 yalnızca gerekirse) KAPANDI olur |
| İlgili gereksinim / kabul | CLI-08, AC-NAV-01..07, T-NAV-04/05/09, `docs/12` §13.6, MET-NAV-01/02, MET-PERF-02/03; DEG-18/20/21 |
| Tahmini büyüklük | L (birden çok plana bölünecek: aşağıdaki dilimler) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Neden TASLAK ve amaç

F5 bugüne kadar yalnızca saf mantık (`BotCore/Nav*.h`, birim testleri) olarak ilerliyor; GameServer'a bağlı **hiçbir** navigasyon yok, yürünebilirlik koruması (CLI-08) icrada yok ve hiçbir şey gerçek haritada oyun içinde sınanmadı. `docs/12` §13.6: F5 yalnızca `BotCore` olarak **kapanamaz**. Bu belge, G5 kapısının sunucu tarafı işini dilimlere böler; plan HAZIR yapılmadan önce (F5-08..F5-10 ve F5-50..F5-54 bitince, ve nav/F4 hatları birleşince) her dilim ayrı küçük plan olarak yazılır:

1. **Bağlama (ilk `.cpp` modülü):** `GameServer`'ın `BotCore.lib`'e bağlanması (ADR-0017'deki "ilk `.cpp` modülü (F5)" ertelemesi), `GameServer/Bot/NavService.{h,cpp}`: açılışta `./Nav/zone71.navgrid` (veya SMD'den çalışma zamanı üretimi) yükleme, `NavGrid::Build`, `NavReach::Build`, takım bölge katmanları, `NavPathfinder` örneği (paylaşım kararı F5-53 ölçüm notuyla), `[BOT] NAV=1` ini anahtarı (varsayılan **0**; `ENABLED=0` davranışı değişmez).
2. **Kiriş guard'ı:** `BotFairnessGuard` hareket kuralına CLI-08 `blocked_chord` eklenir (`NavCheckStep`, F5-50); reddedilen paket gönderilmez, `FAIRNESS_REJECT` rule `CLI-08`; düz `/bot move` da korunur.
3. **Yol izleme:** `ActionExecutor` hareket yürütücüsüne waypoint zinciri (ara noktada durma paketi göndermeden devam), `/bot goto <bot> <x> <z>` ve `/bot follow <bot> <hedef bot>` test sürücüleri, `NavFollower` + bütçe zamanlayıcısı (F5-53) + takılma tespiti (F5-54) bağlama, `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` telemetrisi (`docs/16` §3.2).
4. **Çalışma zamanı (oyun içi) doğrulaması (Claude + insan; her biri `docs/reports/degerlendirme-takip.md` satırında ayrı izlenir, birim testi veya doküman bunların yerine geçmez):** **T-NAV-04** (dar geçit/köprü 50 geçiş: takılma oranı, hareket **niyeti ve gerçek ilerleme** birlikte: normal paket aralıklarında yanlış alarm 0), **T-NAV-05/T-NAV-10 (iki ulus, ayrı ayrı):** Karus **ve** El Morad botu için ölüm → `WIZ_REGENE` → doğuş noktası → arenaya dönüş zinciri oyun içinde (her ulus ≥ 10 tekrar; süreler Karus ~52 sn, El Morad ~142 sn ± %20; arena sınırı içinde kalma; yolda takılma/`NodeLimit`/`InvalidGoal` 0; ulus bazlı ayrı rapor, ADR-0033-DEG), **T-NAV-09 (yeni): su ve göl kıyısı** (insan istemcisi suya girebiliyor mu, sunucu olay ızgarasıyla uyum; Q-26), T-NAV-02 (eğim kalibrasyonu), **T-NAV-11 (yeni): 16 botta oyun içi tick ve yol bulma bütçesi** (gerçek `BotManager` tick'i, MSVC Release, 30 dk: `PERF_SAMPLE` nav payı p95 ≤ 1,5 ms, toplam tick p95 ≤ 5 ms; sorgular ertelendiğinde botların bekleme/takip/mevcut yolu kullanma davranışı: bayat planla sürme 0, ilk plan beklemesi ≤ 1,1 sn), AC-NAV-03 (engelli hücreye giren hareket = 0; sunucu tarafı denetim logu, 30 dk; düz `/bot move` engel kesen adımı `FAIRNESS_REJECT CLI-08` ile durdurur), AC-NAV-01/02/04/06/07.

## 1A. Dilimler (2026-10-03, proje sahibi talebiyle: küçük, bağımlılıkları açık planlar)

Bu şemsiye plan **kod yazmaz**; işi aşağıdaki dilimlere böler. Her dilim ≤ ~1 günlük iş ve ≤ ~10 dosyadır, kendi kabul kriterleri ve (gerekirse) kendi çalışma zamanı kanıtı vardır. `[BOT] ENABLED=0` ve yeni `[BOT] NAV=0` (varsayılan) iken **hiçbir dilim** davranışı değiştirmez. Taban dal hepsi için `gece/2026-10-02`. Bot, insan oyuncunun bilemeyeceği/yapamayacağı bir şey yapamaz (`docs/03` §13, `docs/13` §3): dilimlerin hiçbiri bota harita/konum konusunda insanda olmayan bir bilgi veya yetenek eklemez (nav yalnızca herkesin sahip olduğu zone verisini bir kez kendi ızgarasına aktarır).

| Dilim | Plan | Kapsam (tek satır) | Bağımlılık | Durum |
|---|---|---|---|---|
| 1 | **F5-59** `nav-servisi-yasam-dongusu-ve-harita-yukleme` | `GameServer/Bot/NavService.{h,cpp}`: `[BOT] NAV=1` (varsayılan 0) iken açılışta zone 71 ızgarasını sunucunun **bellekteki SMD verisinden** kurar (`NavGrid::Init` + `Build`), hazır/`Grid()` sorgusu, kapanışta bırakma; `BotCore/NavFingerprint.h` CRC32 parmak iziyle sunucu belleği = `tools/nav-export.py` dosyası kanıtı; log `nav ready: zone 71 cells=… build_ms=…`. **`BotCore.lib` bağlantısı gerekmez** (`Nav*.h` başlık-yalnızca; ADR-0017 m.3 geçerli) | F5-01 | HAZIR |
| 2 | **F5-60** `nav-su-ve-engel-verisi-denetimi` | Sunucusuz çevrimdışı su/engel denetimi: SMD/ızgara su ve geçilemezi nasıl kodluyor, `NavGrid` neyi engel sayıyor, ≥ 5000 çift planlayıcı yolu/kirişi "su adayı" hücreye giriyor mu, doğrulanmış su zemin gerçeği (istemci verisi) bulunabildi mi; karar: ayrı `water` katmanı gerekli mi (`GEREKLİ`/`GEREKMEZ`/`BELİRSİZ`). T-NAV-09 insan testi planın **dışında** (proje sahibi testi) | F5-01..03, F5-11, F5-50, F5-58 | HAZIR |
| 3 | **F5-61** gerçek hareket icrasında segment/engel denetimi | `BotFairnessGuard` hareket kuralına CLI-08 `blocked_chord` (`NavCheckSegment`, F5-50): düz `/bot move` engel kesen adımı `FAIRNESS_REJECT CLI-08` ile reddeder, reddedilen paket gönderilmez; engelli hücreye giren hareket denetim logu (AC-NAV-03 altyapısı) | F5-59; F5-50 ve F4 guard hattı (`CheckMoveStep`, ADR-0017) `KAPANDI` | TASLAK |
| 4 | **F5-62** `/bot goto` nav yolu | Komutla verilen hedefe düz çizgi yerine `NavPathfinder` + `NavSmoothPath` yolunda waypoint zinciriyle ilerleme (ara noktada durma paketi yok); `NavPathfinder` paylaşım kararı (F5-53 ölçüm notu); yol yoksa/`NodeLimit`/`InvalidGoal` davranışı | F5-59, F5-61; F5-02, F5-03, F5-05, F5-51, F5-53 | TASLAK |
| 5 | **F5-63** hareketli hedef: kestirim, yeniden planlama, kurtarma | `NavFollower`: hedef gözleminden (`UnitView`) hız kestirimi (F5-52/F5-56), yeniden planlama (6 m / 500 ms), takılma tespiti ve kurtarma merdiveni (F5-09/F5-54), **F5-57 çağıran sözleşmesi** (`NotifyReplan`, `OnPacketSent/Rejected`, `moving` kuralı) | F5-62; F5-04, F5-09, F5-52, F5-54, F5-56, F5-57; F4 `Perception` (F4-50) | TASLAK |
| 6 | **F5-64** botlar arası yol sorgusu bütçesi ve NAV telemetrisi | `NavBudget` (F5-53) bağlama: tick başına 1,5 ms bütçe, adil kuyruk, faz kaydırma, yol önbelleği; **ertelenen sorgu davranışı** (mevcut yolu izle/bekle, bayat planla sürme yok, ilk plan beklemesi ≤ 1,1 sn); `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` ve `PERF_SAMPLE` nav payı (`docs/16` §3.2) | F5-62; F5-53 | TASLAK |
| 7 | **F5-65** nav durumunun temizlenmesi | Ölüm, respawn (`WIZ_REGENE`), despawn ve bölge değişiminde bot başına nav durumu (yol, waypoint, takip, sorgu kuyruğu, takılma izleyicisi, önbellek referansı) temizlenir; bayat yolla ölü/yeni doğmuş bot yürümez | F5-62 (ve F5-63 ile birlikte); F4-07 (`WIZ_REGENE`), F2-04/F2-05 (despawn/respawn) | TASLAK |
| 8 | **F5-66** çalışma zamanı doğrulama koşusu | **Claude + insan, oyun içi, kod yazmaz:** tek bot engelli rota (T-NAV-04, AC-NAV-03 30 dk, düz `/bot move` reddi); iki ulus ayrı doğuş → arena dönüşü (T-NAV-05/T-NAV-10, ≥ 10 tekrar/ulus, Karus ~52 sn / El Morad ~142 sn ± %20); takip ve takılma; **16 bot nav payı ve `BotManager::Tick`** (T-NAV-11, AC-NAV-07: nav p95 ≤ 1,5 ms, toplam p95 ≤ 5 ms, MSVC Release); ertelenen sorgu davranışı | F5-59 .. F5-65 (F5-67 koşullu); T-NAV-09 sonucu (proje sahibi) | TASLAK |
| 9 | **F5-67** `nav-su-katmani-duzeltmesi` (**koşullu**) | Yalnızca F5-60 `GEREKLİ` derse: `water` işaretli hücreler için `NavGrid` katmanı, planlayıcı maliyeti/engeli, kiriş denetimi ve testler; maske kaynağı ve sert-engel/maliyet kararı F5-60 raporu ve T-NAV-09 sonucuna göre | F5-60 `KAPANDI` + sonuç `GEREKLİ`; F5-59; F5-61 | İPTAL (T-NAV-09: su katmanı gerekmez) |

**Sıra ve paralellik:** F5-59 ∥ F5-60 hemen başlayabilir (ikisi de HAZIR, birbirinden bağımsız). F5-61 F5-59'dan sonra; F5-62 F5-61'den sonra; F5-63, F5-64, F5-65 F5-62'den sonra (F5-63 ile F5-65 aynı bot-durum yapısına dokunduğundan ardışık yazılması önerilir); F5-66 en son. F5-67 yalnızca F5-60 sonucuna göre ve F5-61'den önce **veya** birlikte planlanır (su kirişi vakası F5-61 guard testlerine girer).

**F5-55'in bölümleriyle eşleme:** §1 madde 1 (bağlama) = F5-59; madde 2 (kiriş guard'ı) = F5-61; madde 3 (yol izleme, `/bot goto`, `NavFollower`, bütçe, takılma, telemetri) = F5-62..F5-65; madde 4 (çalışma zamanı doğrulaması) = F5-66, T-NAV-09 = F5-60 (çevrimdışı) + proje sahibi testi, koşullu düzeltme = F5-67.

**Açık soruların karşılığı (eski §4 yerine):**

| Soru | Karşılık |
|---|---|
| `NavPathfinder` bot başına mı paylaşılan mı? | F5-62'de karara bağlanır (F5-53 ölçüm notu; yol bulma durumu iş parçacığı güvenli değil, ≈ 4,2 MB); F5-59 yalnızca `const NavGrid` tutar |
| Izgara dosyasının üretimi/yeri | **Karar (F5-59): dosya yok;** sunucu ızgarayı açılışta bellekteki SMD verisinden kurar; `.navgrid` yalnızca araç/test dünyasındadır; eşitlik CRC32 parmak iziyle kanıtlanır. ADR-0006 Ek F5-59 (Claude) |
| Su/göl ayrı `water` katmanı gerektiriyor mu? | F5-60 (çevrimdışı denetim + karar kuralı) → T-NAV-09 insan testi → gerekirse F5-67 |

**Kapanış kuralı:** F5-55 şemsiye planı, F5-59 .. F5-66 `KAPANDI` olduğunda `KAPANDI` olur; F5-67 yalnızca F5-60 `GEREKLİ` derse yapılır ve o durumda ona da bağlıdır (F5-60 `GEREKMEZ` ise F5-67 `İPTAL` olur ve kapanışı engellemez). **Faz kabulü (G5) ayrıdır** ve proje sahibinin onayıyla verilir (`docs/17` §5): dilim planlarının `DOĞRULANDI` olması fazı kabul ettirmez. **Durum ayrımı (zorunlu):** plan hazır → kod uygulandı → oyun içinde doğrulandı ayrı sütunlardadır; yalnızca birim testi veya doküman güncellemesiyle "doğrulandı" yazılmaz (`docs/reports/degerlendirme-takip.md`). Hiçbir dilimin "oyun içinde doğrulandı" durumu F5-66 öncesinde yazılmaz.

**Şemsiye kabul kriterlerinin (§3) sahibi:**

| §3 kriteri | Hangi dilim kanıtlar |
|---|---|
| Derleme/test, `NAV=0`/`ENABLED=0` davranışı değişmez | her dilim (ortak) |
| `/bot goto` 3 bot arena A'ya, engelli hücreye giren hareket 0 (30 dk), `FAIRNESS_REJECT CLI-08` | kod: F5-61 + F5-62; çalışma zamanı: F5-66 |
| T-NAV-04 (≤ 2/bot-saat, kurtarma p95 ≤ 5 sn) | F5-63 (kod) + F5-66 (oyun içi) |
| T-NAV-10 iki ulus ayrı (ölüm → respawn → arena, ≥ 10 tekrar) | F5-65 (temizlik) + F5-66 (oyun içi) |
| T-NAV-09 sonucu docs/12 §13.1 ve Q-26'ya işlenir | F5-60 (çevrimdışı) + proje sahibi testi; F5-67 gerekirse |
| T-NAV-11 / AC-NAV-07 (16 bot, nav p95 ≤ 1,5 ms, toplam p95 ≤ 5 ms; ertelenen sorgu) | F5-64 (kod) + F5-66 (oyun içi, MSVC Release) |
| Kiriş guard'ı oyun içinde (düz `/bot move` reddi, planlayıcı yolunda CLI-08 reddi 0) | F5-61 (kod) + F5-66 |

## 2. Ön koşul ve dikkat edilecekler

- **Hat birleşmesi (2026-10-03 güncellemesi):** nav hattı (`gece/2026-10-02-nav`) `gece/2026-10-02`'nin ve `main`'in atasıdır (`git merge-base --is-ancestor` doğrulandı); dilimler `gece/2026-10-02` tabanından başlar.
- **Perception bağlantısı:** `NavFollower.ObserveTarget` girdisi `UnitView` konum/hız/yaş alanlarındandır (F4-50); hedef `in_region=false`/`pos_state=lost` iken takip kararı karar katmanındadır (`docs/13` §5.2a). NaN/∞ ve eski gözlem süzülür.
- **Arena modu:** `AddForbidOutsideDisc` yalnızca senaryo `arena:` tanımlıyken; doğuş → arena yolu önbellekten (F5-53); geri çekilme güvenli noktası arena içinde (ADR-0033-DEG).
- **Bütçe:** nav toplamı tick başına ≤ `P-NAV-TICK-BUDGET-MS` (1,5 ms), MET-PERF-02 toplam p95 ≤ 5 ms; telemetri `PERF_SAMPLE`'a nav payı eklenir.
- **Takılma tespiti sözleşmesi (F5-57):** `NavProgressAssessor` kullanılırken çağıran (1) her yeni rotada `NotifyReplan(nowMs, 0)` çağırır, paket gönderiminden **önce** (çağrılmazsa değerlendirici Öklid yer değiştirmesine düşer, U-dönüşünde yanlış alarm riski); (2) gönderilen her paketi `OnPacketSent`, guard reddini `OnPacketRejected` ile bildirir; (3) `NavStuckMonitor::Update`'e `moving = (verdict == Progressing || verdict == Stalled)` verir; (4) niyet açıkken uzun süre paket yoksa karar `AwaitingPacket` kalır: takılma sayılmaz, bu durumun ele alınması (paket üretilmiyor) karar katmanı/`ActionExecutor` tarafındadır. Eşikler `[A]`, T-NAV-04 sonrası güncellenir (`docs/12` §13.3).
- **Sözleşme:** nav, sunucu `Map`/`SMD` verisini yalnızca başlangıçta bir kez **kendi ızgarasına** aktarır (harita dosyası = herkesin sahip olduğu zone verisi; oyuncu istemcisi de aynı haritayı bilir, `docs/12` §1); çalışma zamanında sunucu dizilerine erişim yok (`tools/check-perception-contract.py` R1-R5 PASS).

## 3. Kabul kriterleri (taslak; her dilim planında somutlaşır)

- [ ] `./tools/build.sh Release|Debug` rc=0; `./tools/run-tests.sh` `0 failed`; `[BOT] NAV=0` ve `ENABLED=0` davranışı değişmez
- [ ] Çalışma zamanı: `/bot goto` ile 3 bot arena A'ya zone 71'de yürür; engelli hücreye giren hareket **0** (sunucu denetim logu, 30 dk); `FAIRNESS_REJECT CLI-08` düz hedef adımında engeli keserken planlayıcı yollarında **0**
- [ ] T-NAV-04 takılma ≤ 2/bot-saat, kurtarma p95 ≤ 5 sn; **T-NAV-10 iki ulus ayrı**: ölüm → respawn → arena dönüşü her ulusta ≥ 10 tekrar başarılı (süre ±%20, arena sınırı içinde, takılma 0); T-NAV-09 sonucu docs/12 §13.1 ve Q-26'ya işlenir
- [ ] T-NAV-11: 16 botta oyun içi nav payı p95 ≤ 1,5 ms/tick ve toplam tick p95 ≤ 5 ms (MSVC Release); ertelenen sorguda bayat planla sürme 0, ilk plan beklemesi ≤ 1,1 sn
- [ ] Kiriş guard'ı (F5-50) oyun içinde: düz `/bot move` ile engel kesen adım reddedilir, planlayıcı yollarında CLI-08 reddi 0
- [ ] **Durum ayrımı (zorunlu):** plan hazır → kod uygulandı → oyun içinde doğrulandı ayrı sütunlardadır; yalnızca birim testi veya doküman güncellemesiyle "doğrulandı" yazılmaz (`docs/reports/degerlendirme-takip.md`)
- [ ] AC-NAV-07: 16 bot yoğun sorgu yükünde nav toplamı p95 ≤ 1,5 ms/tick (MSVC Release), hiçbir bot 1 sn'den uzun yol beklemez
- [ ] Faz kabulü ayrı: G5 (`docs/17` §5) proje sahibinin onayıyla

## 4. Açık sorular

Önceki açık sorular §1A'daki "Açık soruların karşılığı" tablosunda yanıtlandı veya ilgili dilime atandı.

