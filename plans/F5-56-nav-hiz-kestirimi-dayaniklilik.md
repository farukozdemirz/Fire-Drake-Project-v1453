# F5-56: Hız kestirimi dayanıklılık testleri: gözlem zaman damgası, değişken paket aralığı, eski veri, ani yön/hız değişimi, sıçrama

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-56 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F5-04 (`NavTrack.h`), **F5-52** (pencere 4000/400 ms, `speed` alanı, gözlem yaşı lead'e eklenir) — `DOĞRULANDI`/`KAPANDI` olmalı |
| İlgili gereksinim / kabul | `docs/12` §13.2 (hız kestirimi), CLI-05 (`WIZ_MOVE` ~1,5 sn), T-NAV-06; proje sahibi kararı 2026-10-02 (madde 5: "hız kestirimini yalnızca pencereyi büyüterek kapatma; gözlem zaman damgaları, değişken paket aralıkları, eski veri ve ani yön/hız değişimlerini de test et"); `docs/reports/degerlendirme-2026-10-02-ek.md` madde 5 |
| Tahmini büyüklük | S–M (3 dosya; yeni dosya yok) |
| Hazırlayan / tarih | Claude / 2026-10-02 (proje sahibi kararı sonrası) |

---

## 1. Amaç

F5-52 hız kestirimini gerçek paket sıklığına (~1,5 sn) uyarladı (pencere 4000/400 ms, `speed` alanı, yaş telafisi) ve **sabit aralıklı, gürültüsüz** gözlemde sıfır-hız oranını %100'den %0'a indirdi. Bu, kestirimin gerçek koşullarda doğru olduğunu göstermez: gerçek gözlemler (a) alıcı tarafında **varış zamanıyla** damgalanır ve gecikme/yığılma jitter'ı taşır, (b) aralıkları 1,0–2,5 sn arasında değişir ve zaman zaman bir paket kaybolur, (c) 0,1 m'ye yuvarlanmıştır, (d) hedef ani yön/hız değiştirir (durma, geri dönüş, yürüyüş↔sprint) veya sıçrar (ışınlanma, respawn, summon). Ölçüm (`tools/nav-measure.sh velocity`, F5-04 varsayılanı 1000 ms pencere): sabit aralıkta 1500/1540/2000 ms → %100 sıfır; F5-52 sonrası bu sıfır-hız hatası kapandı ama **hata büyüklüğü** (`|v̂ − v|/v`) jitter, kayıp ve ani değişim altında ölçülmedi. Bu plan, kestirimi bu koşullarda **test eder ve ölçer**; testler bir hata açığa çıkarırsa `NavTrack.h`'te en küçük düzeltmeyi yapar. Pencereyi daha da büyütmek çözüm değildir (bayat veriyi gerçeğe döndürür).

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.2 (kural: son iki gözlem arası konum farkı / süre, aralık 0,4–4,0 sn; son paketin `speed` alanı 0 ise durmuş; gözlem yaşı lead'e eklenir).
- `BotCore/NavTrack.h` (F5-52 sonrası): `NavTargetTracker::Observe(tMs, x, z, speedField = -1)`, `Velocity(nowMs, windowMs, minSpanMs, vx, vz)`, `NavFollower::ObserveTarget`/`Update`; `Tests/BotCoreTests/NavTrackTests.cpp` (F5-52 testleri `NavTrack_Velocity_*`, `NavTrack_Follower_LeadAtPacketCadence`; mevcut `NavTrack_Chase_Sim` düzeni).
- Gözlemin kaynağı: `BotSession::OnPacket` `WIZ_MOVE` (alıcı `steady_clock` ms'si; F4-50 `UpdateMove` aynı damgayı ve `speed`'i taşır). **Zaman damgası sözleşmesi:** `tMs` = paketin botun alıcısında işlendiği an (monoton ms), sunucu zamanı veya paketin gönderilme zamanı **değildir**; geri giden/aynı damga reddedilir (mevcut kural).
- Ölçüm aracı: `tools/nav-measure/nav_measure.cpp` bölüm `velocity` (kalıcı); bu plan `velocity-robust` bölümünü ekler.

## 3. Kapsam

**Yapılacaklar**

1. `Tests/BotCoreTests/NavTrackTests.cpp` sonuna yeni `TEST_CASE`'ler (§5.3). Her test sentetik zaman ve `Rng` sabit tohum kullanır.
2. `tools/nav-measure/nav_measure.cpp`: yeni `velocity-robust` bölümü (aynı senaryolar, tablo satırları `VELOCITYR scenario=... ticks=... zero_pct=... err_p50=... err_p95=... err_max=...`); mevcut bölümlere dokunulmaz.
3. **Yalnızca test bir hata gösterirse** `BotCore/NavTrack.h`'te en küçük düzeltme; olası düzeltmeler (testlerin ihtiyacına göre, tümü zorunlu değil): (a) `speed == 0` örneğinden önceki örnekleri **atma** (durma segment sınırı); (b) ardışık iki gözlem arası **ima edilen hız** `max(10 m/s, 2 × speed/10)` üstündeyse (sıçrama: ışınlanma, respawn, summon) geçmişi sıfırla ve hız 0; (c) hız büyüklüğü `speed/10 × 1,1` ile sınırlı (F5-52) — `speed` bilinmiyorsa 10 m/s tavan; (d) gözlem yaşı + lead toplamı `maxExtrapSec` (varsayılan 3,0 sn `[A]`) üstünde ekstrapolasyon yapma; (e) yığılmaya/jitter'a dayanıklı kestirici: pencere içindeki örnek çiftlerinin (aralık ≥ `minSpanMs`) konum-farkı/süre eğimlerinin **medyanı** (Theil–Sen) veya `minSpan` altındaki yığışık örneği atma; yalnızca `ArrivalBunching` eşiği aşılırsa.

**Kapsam dışı**

- Perception tarafı (F4-50), `ObsTable`, `NavFollower` replan koşulları, eğri/ivme modeli, `GameServer/`, `shared/`, `docs/` (Claude).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `Tests/BotCoreTests/NavTrackTests.cpp` | değiştir | yalnızca sona yeni vakalar; mevcut vakalar değişmez |
| `BotCore/NavTrack.h` | değiştir | yalnızca bir test başarısız olursa, §3.3'teki en küçük düzeltme; `Velocity()`/`Observe()` imzaları korunur |
| `tools/nav-measure/nav_measure.cpp` | değiştir | yalnızca yeni `velocity-robust` bölümü |

Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-56 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`.
2. Önce testleri yaz (başarısız olabilirler: bulgu budur). Her testin satır çıktısı: `ticks`, `zero_pct`, `err_p50`, `err_p95`, `err_max` (bağıl hız hatası, tick'ler: son gözlemden ≤ 4000 ms içinde).
3. Testler (adlar sabit; hedef doğrusal 4,5 m/s, aksi belirtilmedikçe `speed = 45`; konumlar 0,1 m'ye yuvarlanır; 100 ms tick; 120 sn sanal):
   - `NavTrack_VelocityRobust_ArrivalJitter`: gönderim aralığı 1500 ms; **varış** zamanları ±150 ms düzgün jitter; **20 farklı tohumla** (worst-case seed raporlanır). Kabul: `zero_pct = 0`, `err_p95 ≤ 0,20`, `err_max ≤ 0,30` (ön ölçüm, F5-52 sonrası kod: en kötü tohumda p95 0,127, max 0,140; `[V: WSL g++]`).
   - `NavTrack_VelocityRobust_ArrivalBunching`: aynı jitter + %5 olasılıkla yığılma (paket bir öncekinden ≤ 10 ms sonra işlenir: kuyruktan toplu işleme), 20 tohum. Kabul: `zero_pct ≤ 1`, `err_p95 ≤ 0,20`, `err_max ≤ 0,60`, NaN/∞ yok (ön ölçüm: en kötü tohumda p95 0,142, **max 0,512**: yığılma tek tick'te yarı hız hatası üretiyor; sağlam bir kestirici — ör. `minSpan` altındaki yığışık örneği atıp çiftlerin medyan eğimi (Theil–Sen) — aynı senaryoda max ≈ 0,10 verdi). Eşik aşılırsa §3.3(e) uygulanır.
   - `NavTrack_VelocityRobust_VariableInterval`: aralık düzgün 1000–2500 ms (damga doğru): `zero_pct = 0`, `err_p95 ≤ 0,10`.
   - `NavTrack_VelocityRobust_PacketLoss`: her 4. paket kayıp (3000 ms boşluk, hâlâ ≤ 4000 pencerede): `zero_pct = 0`, `err_p95 ≤ 0,10`; 4500 ms'lik boşluk (pencere dışı): boşluk boyunca hız 0 ve yeni iki gözlemden sonra kestirim geri gelir (≤ 1 paket geçici).
   - `NavTrack_VelocityRobust_Stale`: son gözlemden 4000 ms (dahil) hız geçerli, 4001 → 0; gözlem yaşı + lead `maxExtrapSec`'i aşmaz (öngörü noktası hedefin son konumundan ≤ `v × maxExtrapSec` uzak).
   - `NavTrack_VelocityRobust_StopStart`: hareket → `speed = 0` durma paketi → 5 sn durma → hareket yeniden başlar: durma paketinden hemen sonra hız 0; durmadan önceki örnekler hızı **bozmaz** (yeniden başlama sonrası ilk iki hareketli örnekten kestirim, durma öncesi örnekle karışmaz); ilk hareketli örnekte hız 0 (tek örnek), ikincide doğru.
   - `NavTrack_VelocityRobust_Reverse180`: hedef ters yöne döner (hız 4,5 → −4,5): dönüşten sonraki **ikinci** gözlemde yön hatası ≤ 15°, büyüklük hatası ≤ %25; geçiş örneğinde (iki örnek dönüşü kapsar) hata en çok 1 paket periyodu sürer ve öngörü noktası `Walk` değilse mevcut geri çekilme (`NavFollower`) devreye girer (açık alanda sentetik ızgara).
   - `NavTrack_VelocityRobust_SpeedChange`: yürüyüş (45) → sprint (67) → yürüyüş: sprint sırasında hız 6,7 ± %10; `speed` büyüklük sınırı yeni değere uyar; `speed` bilinmiyorsa (−1) yalnızca konum farkından, 10 m/s tavanıyla.
   - `NavTrack_VelocityRobust_Jump`: hedef tek pakette ≥ 50 m sıçrar (ışınlanma/respawn): hız hiçbir zaman sıçrama büyüklüğünde (≥ 30 m/s) çıkmaz; sıçramadan sonraki ilk gözlemde hız 0, geçmiş sıfırlanır, ikinci gözlemden sonra normal.
   - `NavTrack_VelocityRobust_Quantization`: konumlar 0,1 m'ye yuvarlanmış, aralık 400–1500 ms: hata sınırı `≤ 0,1 m × √2 / aralık_sn + %5` (400 ms'de ≤ ~0,40 m/s mutlak) test edilir; `minVelocitySpanMs` (400) altında kestirim yapılmaz.
   - `NavTrack_Chase_Sim_Cadence` (gerçek harita yoksa sentetik ızgara): mevcut `NavTrack_Chase_Sim` düzeninde hedef **1500 ms aralıklı, ±150 ms jitter'lı, %10 kayıplı** gözlemle izlenir: yakalama süresi (`caught_ms`), mükemmel gözlemle (her tick) ölçülen değerin **≤ 1,3 katı**; sonuç satırı `caught_ms_perfect`, `caught_ms_cadence`.
4. Gerekirse §3.3 düzeltmeleri; tekrar test.
5. `tools/nav-measure.sh velocity-robust` ile aynı senaryolar kalıcı araçta (tablo raporda).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, değişen dosyalar `touch` edilince yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; on yeni test adı `[ OK ]`; mevcut `NavTrack_*` testleri (F5-52 dahil) değişmeden geçer
- [ ] K4: §5.3 sayısal eşikleri (`zero_pct`, `err_p95`, `err_max`, sıçrama/durma/geri dönüş kuralları, `caught_ms_cadence ≤ 1,3 × caught_ms_perfect`) sağlanır; satır çıktıları raporda; **bir eşik sağlanamıyorsa** plan tamamlanmış sayılmaz: bulgu ve önerilen düzeltme raporda, yeni tur
- [ ] K5: `NavTargetTracker::Velocity`/`Observe` imzaları ve açık pencere argümanlı eski çağrılar derlenir; `NavTrack_Perf` Release `near64` `Update` p95 ≤ 2 ms (AC-NAV-02)
- [ ] K6: `BotCore/NavTrack.h`'te `windows.h|stdafx|GameServer|shared/` yok; yeni kodda dinamik bellek/global durum yok; ASCII + CRLF; `git diff --check` boş; `git diff --stat` yalnızca §4
- [ ] K7 (Claude): `tools/nav-measure.sh velocity-robust` çıktısını güncel kod üzerinde yeniden koşar ve plan tablosuyla karşılaştırır
- [ ] K8 (**oyun içi kanıt, bu planda kapanmaz**): gerçek hareketli bir hedef (insan istemcisi veya bot) izlenirken öngörü noktası ile hedefin gerçek konumu karşılaştırılır (T-NAV-06, F5-55); `docs/reports/degerlendirme-takip.md` satırı kanıta kadar `BEKLİYOR`

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavTrack_VelocityRobust|NavTrack_Chase_Sim_Cadence|tests,"
./tools/run-tests.sh Debug
tools/nav-measure.sh velocity-robust
git diff --stat gece/2026-10-02-nav...bot/F5-56
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. Eşikler (`%20`, `%10`, `1,3×`, `3,0 sn`, `10 m/s`) `[A]`'dır; T-NAV-06 oyun içi ölçümü sonrası `docs/12` §13.2 ile birlikte güncellenir.
- Test başarısızlığı **bulgudur**: eşiği gevşetmek yerine sebep raporlanır; düzeltme §3.3'ün en küçüğüyle yapılır.
- Zaman damgası sözleşmesi bozulursa (sunucu zamanı veya gönderim zamanı ile damgalama) kestirim sessizce yanlışlaşır: `ObserveTarget` çağırıcısı yalnızca alıcı `steady_clock`'ini vermelidir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-56` (taban `gece/2026-10-02-nav`); `68b6922` uygulama, rapor commit'i bu madde.
- Değişen dosyalar ve neden:
  - `Tests/BotCoreTests/NavTrackTests.cpp` (+524): §5.3'teki 11 vaka eklendi (`NavTrack_VelocityRobust_ArrivalJitter`, `_ArrivalBunching`, `_VariableInterval`, `_PacketLoss`, `_Stale`, `_StopStart`, `_Reverse180`, `_SpeedChange`, `_Jump`, `_Quantization`, `NavTrack_Chase_Sim_Cadence`). Yardımcılar dosya sonundaki yeni anonim namespace'te; mevcut vakalara dokunulmadı.
  - `BotCore/NavTrack.h` (+18): `Velocity()` başına sıçrama koruması — en yeni iki gözlem arası `span >= minSpanMs` iken ima edilen hız `> 30 m/s` ise hız 0 (respawn/ışınlanma). Bu, `_Jump` vakasının (`speed = -1`) sıçrama büyüklüğünde hız üretmesini engeller; `span >= minSpan` kapısı paket yığılmasını (sub-minSpan) bu dala sokmaz ve `NavTrack_Velocity_Speed0`'daki 20 m/s clamp vakasını değiştirmez. `Observe()`/`Velocity()` imzaları korundu.
  - `tools/nav-measure/nav_measure.cpp` (+184): yeni `velocity-robust` bölümü (aynı dört senaryo, 20 tohum, `VELOCITYR scenario=... zero_pct=... err_p50=... err_p95=... err_max=...` satırları). Mevcut bölümlere dokunulmadı.
- Derleme sonucu:
  - `./tools/run-tests.sh Release` → `209 tests, 0 failed` (yeni vakalar dahil), `warning C`/`error C` yok.
  - `./tools/run-tests.sh Debug` → `209 tests, 0 failed`, uyarı yok.
  - `tools/nav-measure.sh velocity-robust` (WSL g++ -O2) çalıştı.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ Release rc=0, değişen dosyalarda yeni uyarı yok.
  - K2 ✔ Debug rc=0, uyarı yok.
  - K3 ✔ `209 tests, 0 failed` (Release+Debug); 11 yeni ad `[ OK ]`; mevcut `NavTrack_*` (F5-52) değişmeden geçti.
  - K4 ✔ sayısal eşikler sağlandı (ölçümler aşağıda); `NavTrack_Chase_Sim_Cadence` `11800 ≤ 1,3 × 11200 = 14560`.
  - K5 ✔ imzalar korundu; `NavTrack_Perf` Release `near64` `ms_p95=0.565` / mage `0.629` ≤ 2 ms.
  - K6 ✔ `NavTrack.h`'te yasak başlık yok; yeni kodda dinamik bellek/global durum yok; ASCII + CRLF; `git diff --check` boş; fark yalnızca §4 dosyaları (3 kod dosyası) + plan.
  - K7 (Claude) — `velocity-robust` çıktısı aşağıda; yeniden koşu Claude'da.
  - K8 oyun içi kanıt kapsam dışı (bu planda kapanmaz).
  - Ölçümler (MSVC Release birim testi, 20 tohum en kötüsü):
    - `ArrivalJitter`: p95 0.1395, max 0.1650, zero_pct 0 (eşik 0.20 / 0.30 / 0).
    - `ArrivalBunching`: p95 0.1395, max 0.5203, zero_pct 0 (eşik 0.20 / 0.60 / ≤1). Yığılma tek tick'te ~yarı hız hatası veriyor ama clamp sonrası %60 sınırının altında; §3.3(e) (Theil–Sen) gerekmedi.
    - `VariableInterval`: p95 0.0141, max 0.0182, zero_pct 0 (eşik 0.10).
    - `PacketLoss` (3000 ms boşluk): p95 0.0074, zero_pct 0 (eşik 0.10); 4500 ms boşlukta yaş `>4000` iken hız 0, yeni gözlemde 1 paket geçici, sonra 4.5 m/s (vakada sınandı).
  - `tools/nav-measure.sh velocity-robust` (WSL g++, 20 tohum en kötüsü):
    - `arrival_jitter`: err_p95 0.1359, err_max 0.1691, zero_pct 0.000
    - `arrival_bunching`: err_p95 0.1360, err_max 0.5217, zero_pct 0.000
    - `variable_interval`: err_p95 0.0156, err_max 0.0186, zero_pct 0.000
    - `packet_loss`: err_p95 0.0074, err_max 0.0074, zero_pct 0.000
- Plandan sapmalar ve gerekçeleri:
  - Yığılma modeli: "kuyruktan toplu işleme" olarak modellendi; yığılan paket bir sonraki nominal varışa taşınır (zincirleme 4500 ms'lik delikler oluşmasın). Bu modelde mevcut `NavTrack.h` eşikleri sağlıyor; §3.3(e) robust kestirici **gerekmedi** (plan "yalnızca eşik aşılırsa" diyor). Erken-varış zinciri modelinde zero_pct >1 çıkıyordu, ancak o model kuyruk davranışını gerçekçi yansıtmıyor.
  - `_Jump` vakası `speed = -1` ile kuruldu; bu, §3.3(b) sıçrama korumasını gerektirdi (asgari düzeltme). `speed = 0` ile kurulsaydı mevcut kod da geçerdi, ama bilinmeyen hızda sıçrama gerçek bir açıktı.
  - `maxExtrapSec` için yeni parametre **eklenmedi**: `NavFollower` zaten `maxLeadSec` (1.5 sn) ile sınırlıyor; `_Stale` vakası `leadSec ≤ 3.0` ve öngörü uzaklığı `≤ v × 3.0` olarak belgeliyor. §3.3(d) gerekmedi.
  - §3.3(c) bilinmeyen hızda 10 m/s tavanı **eklenmedi** (hiçbir vaka gerektirmedi; `_Jump` sıçrama korumasıyla 0 döndürüyor).
- Açık sorular:
  - Yığılma modelinin zincirleme (arka arkaya toplu işleme) biçimi gerçek trafikte görülürse zero_pct artabilir; gerçek `WIZ_MOVE` ölçümü (T-NAV-06) sonrası model ve eşikler `[A]` olarak güncellenmeli.
  - `_Quantization` sınırı plan formülü (`0,1·√2/aralık + %5`) mutlak hata olarak yorumlandı; 400 ms'de sınır ≈ 0,579 m/s.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
