# F5-56: Hız kestirimi dayanıklılık testleri: gözlem zaman damgası, değişken paket aralığı, eski veri, ani yön/hız değişimi, sıçrama

| Alan | Değer |
|---|---|
| Durum | DÜZELTME GEREKLİ |
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

### Tur 1 — 2026-10-03

- **Karar:** DÜZELTME GEREKLİ
- **İncelenen commit:** `cf06837` (`bot/F5-56`, taban `gece/2026-10-02-nav`; iki commit: `68b6922` uygulama, `cf06837` rapor). Paralel hat `nav`: sunuculara dokunulmadı.
- **Doğrulama ortamı:** `./tools/build.sh Release` ve `Debug` (`NavTrack.h` ve `NavTrackTests.cpp` `touch` edildi), `./tools/run-tests.sh <cfg> --no-build`, `tools/nav-measure.sh velocity-robust`, ayrıca depoya yazılmayan geçici deneyler (`/tmp/f556/probe.cpp`, host `g++`).

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release rc=0, yeni uyarı yok | ✔ | rc=0; `touch` sonrası `NavTrackTests.cpp` yeniden derlendi; `warning C` 0 |
| K2 Debug rc=0, uyarı yok | ✔ | rc=0; `warning C`/`error C` 0 |
| K3 `0 failed`, yeni adlar `[ OK ]`, mevcut `NavTrack_*` değişmez | ✔ | Release ve Debug `209 tests, 0 failed`; 11 yeni ad `[ OK ]` (plan "on yeni" yazıyor, §5.3 listesi 11 ad; sorun değil); mevcut vakalara dokunulmamış (fark yalnızca dosya sonuna ekleme) |
| K4 §5.3 sayısal eşikler ve kurallar | ✘ (kısmen) | Sayılar raporla uyuşuyor (aşağıda), ancak dört vaka planın istediği şeyi sınamıyor ya da gevşetilmiş: B1-B4 |
| K5 imzalar, `NavTrack_Perf` p95 ≤ 2 ms | ✔ | İmzalar değişmedi; Release `exact` p95 0,557 ms, `mage` 0,629 ms |
| K6 başlık/biçim/`git diff --check`/kapsam | ✔ | `NavTrack.h`'te yasak başlık yok; yeni kodda dinamik bellek/global durum yok; üç kod dosyası ASCII + CRLF (satır sayısı = CRLF sayısı); `git diff --check` boş; fark yalnızca §4 dosyaları + planın kendisi (plan farkı: `Durum` ve Uygulayıcı Raporu) |
| K7 (Claude) `velocity-robust` yeniden koşu | ✔ | `arrival_jitter` p95 0,1359 / max 0,1691; `arrival_bunching` p95 0,1360 / max 0,5217; `variable_interval` p95 0,0156 / max 0,0186; `packet_loss` 0,0074; hepsinde `zero_pct=0.000`. Rapordaki tabloyla birebir aynı |
| K8 oyun içi kanıt | — | Bu planda kapanmaz (`BEKLİYOR`) |

Ölçüm doğrulaması: MSVC Release `NAVTRACK velrobust` satırları rapordaki sayılarla aynı (`ArrivalJitter` p95 0,1395 / max 0,1650; `ArrivalBunching` p95 0,1395 / max 0,5203; `VariableInterval` 0,0141 / 0,0182; `PacketLoss` 0,0074); `NAVTRACK chase_cadence caught_ms_perfect=11200 caught_ms_cadence=11800` (≤ 1,3 × 11200). Uygulayıcı Raporu'ndaki derleme/test/ölçüm iddialarında yanlış yok.

**Bulgular (önem sırasıyla)**

1. **B1 (yüksek) `BotCore/NavTrack.h:260-276`: sıçrama koruması yalnızca en yeni iki gözlemi denetliyor; plan "hız hiçbir zaman sıçrama büyüklüğünde çıkmaz" diyor.** Geçici deneyle (`speed = -1`, 0/1500/3000 ms'de normal yürüyüş, 4500 ms'de +50 m, **4505 ms'de** yığılmış ikinci paket): `Velocity` = **33,2 m/s** (sıçrama büyüklüğü). Sebep: en yeni iki örnek arası 5 ms < `minSpanMs` olduğu için koruma (`jspan >= minSpanMs` kapısı) çalışmıyor; seçim döngüsü sıçramanın öncesindeki 3000 ms örneğiyle eşleştiriyor. Ayrıca bilinmeyen hızda (`speed = -1`) 2500 ms aralıklı +53 m sıçrama **21,3 m/s**, 1500 ms aralıklı +42,5 m **28,3 m/s** üretiyor (eşik 30 sabit; plan §3.3(b)/(c) bilinmeyen hız için 10 m/s tavanı istiyor, uygulanmadı). `NavTrack_VelocityRobust_Jump` yalnızca tek ve 1500 ms aralıklı ≥ 50 m senaryoyu sınıyor.
2. **B2 (orta) `Tests/BotCoreTests/NavTrackTests.cpp` `NavTrack_VelocityRobust_Quantization`: sınır plandan gevşek ve test 400/1000 ms'de boş (vacuous).** Plan sınırı `0,1 m × √2 / aralık_sn + %5` ve "400 ms'de ≤ ~0,40 m/s mutlak" der; test `+ 0,05×4,5 = +0,225` kullanıyor (400 ms'de 0,579) ve raporda "plan formülü mutlak hata olarak yorumlandı" diye geçiyor, ama formülün kendisi 0,40'ı vermiyor. Üstelik 4,5 m/s × 0,4 s = 1,8 m ve 4,5 × 1,0 = 4,5 m, 0,1 m ızgarasının tam katı: bu aralıklarda nicemleme farkı **her zaman 0** (deneyle en kötü mutlak hata 400 ms'de 0,000, 1000 ms'de 0,000, 700 ms'de 0,071, 1500 ms'de 0,033). Yani planın vurguladığı 400 ms durumu hiç sınanmıyor. Plan §8: eşiği gevşetme.
3. **B3 (orta) `NavTrack_VelocityRobust_Reverse180`: "geçiş örneği" (dönüş iki gözlemin arasında) sınanmıyor.** Test dönüşü tam bir örnek anına hizalıyor (3000 ms'de 13,5 → 4500 ms'de 6,75). Plan: "geçiş örneğinde (iki örnek dönüşü kapsar) hata en çok 1 paket periyodu sürer". Deneyle (dönüş 3750 ms'de, 4500 ms'de x = 13,5): geçiş örneğinde hız **0,0** (gerçek −4,5), sonraki örnekte (6000 ms) −4,5: yani 1 paket geçici, gereksinim karşılanıyor; ama bu davranış testte yok, ileride değişirse yakalanmaz. Bkz. B4 `SpeedChange`.
4. **B4 (orta) `NavTrack_VelocityRobust_SpeedChange`: bilinmeyen hızda 10 m/s tavanı ve `speed` sınırının yeni değere uyumu sınanmıyor.** Plan: "`speed` büyüklük sınırı yeni değere uyar; `speed` bilinmiyorsa (−1) yalnızca konum farkından, 10 m/s tavanıyla". Test yalnızca temiz örnekleri okuyor (6000 ms'de 6,7, 7500 ms'de 4,5): konum gürültüsü olsa kırpmanın 7,37 (sprint) mi 4,95 (yürüyüş) mi olduğu görülmüyor. `speed = -1` için tavan hem uygulanmadı hem sınanmadı (B1 ile ilişkili).
5. **B5 (düşük, not) `_StopStart` 5 sn duruş:** test yalnızca plandaki 5 sn'lik durmayı sınıyor; bu sürede durma paketi 4000 ms penceresinden çıktığı için "durma öncesi örnekle karışmaz" iddiası pencere ile örtülüyor. Kısa durma (2 sn) denemesinde ilk hareketli örnekte hız 3,375 m/s çıkıyor (durma paketiyle eşleşiyor; gerçek 4,5). Bu davranış kabul edilebilir (alçak yönlü, bir paket), plan 5 sn dediği için engel değil; yalnızca kayıt.
6. **B6 (düşük, not) yığılma modeli:** `GenCadence` yığılan paketi **bir sonraki** nominal varışa çekiyor (kuyrukta bekleyip toplu işlenme; gerçekçi). Plan ifadesi ("bir öncekinden ≤ 10 ms sonra") bu modelle uyumlu okunabiliyor; en kötü `max` 0,5203 ≤ 0,60 ve `zero_pct = 0`. Uygulayıcı "erken-varış zinciri modelinde zero_pct > 1 çıkıyordu" diyor: bu ölçüm raporda yok. Zincirleme yığılma gerçek `WIZ_MOVE` ölçümüne (T-NAV-06, F5-55) bırakılıyor; engel değil.

**Notlar (engel değil):** `maxExtrapSec` için yeni parametre eklenmemesi doğru: `NavFollower` `NavTrack.h:387-388`'de lead'i zaten `maxLeadSec = 1,5 sn` ile sınırlıyor, `_Stale` testi (`leadSec ≤ 3,0`) bu yüzden gerçekte daha gevşek bir üst sınırı sınıyor; isterseniz `leadSec ≤ params.maxLeadSec` ile sıkılaştırın. Sıçrama eşiği olarak 30 m/s seçimi mevcut `NavTrack_Velocity_Speed0` (500 ms'de 20 m/s konum sıçraması 4,95'e kırpılır) vakasıyla uyumlu tutulmuş; bilinen `speed` için bu eşik korunmalı. Talimat 1 depoya yazılmayan geçici bir kopyada denendi: yığılmış sıçrama 33,2 → 0, bilinmeyen hızda +53 m/2500 ms 21,3 → 0, +42,5 m/1500 ms 28,3 → 0, bilinen hızda +53 m/2500 ms 4,95 (kırpma, değişmedi); `velocity-robust` dört satırı değişmedi (`arrival_bunching` p95 0,1360 / max 0,5217, `zero_pct` 0).

**Düzeltme talimatı:**

```
plans/F5-56-nav-hiz-kestirimi-dayaniklilik.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
1. BotCore/NavTrack.h Velocity(): sıçrama korumasını "en yeni iki örnek" yerine SEÇİLEN ÇİFTE uygula. Mevcut `if (m_count >= 2) { jspan ... }` bloğunu kaldır; `haveOld` ve `span > 0` kontrollerinden sonra (vx/vz hesabından ÖNCE) ima edilen hızı (sqrt(dx*dx+dz*dz) * 1000 / span, dx/dz = newest - old) hesapla; `limit = (newest.speed < 0) ? 10.0f : 30.0f` (bilinmeyen hızda 10 m/s tavanı, plan §3.3(b)/(c); bilinen hızda mevcut 30 m/s ve mevcut NavTrack_Velocity_Speed0 20 m/s kırpma vakası değişmez), `implied > limit` ise vx = vz = 0 bırakıp dön. Böylece (a) sıçramadan hemen sonra yığılmış (< minSpanMs) paket sıçramanın öncesiyle eşleşip sıçrama büyüklüğünde hız üretmez, (b) hız bilinmediğinde 2500 ms aralıklı +53 m sıçrama (21,3 m/s) 0 döner. Mevcut NavTrack_* testleri (F5-52 ve NavTrack_Velocity_Speed0 dahil) değişmeden geçmeli; geçmeyen olursa durup raporda soru olarak yaz. Başlık yorumunu İngilizce, tek kısa blok olarak güncelle.
2. Tests/BotCoreTests/NavTrackTests.cpp NavTrack_VelocityRobust_Jump: şu alt vakaları ekle (mevcutlara dokunma): (a) speed = -1; 0/1500/3000 ms normal 4,5 m/s yürüyüş, 4500 ms'de +50 m sıçrama, 4505 ms'de yığılmış ikinci paket: Velocity(4505) büyüklüğü < 1e-3 (30'dan küçük DEĞİL, sıfır); (b) speed = -1; 0/1500 ms normal, 4000 ms'de +53 m (2500 ms aralık): büyüklük < 1e-3; (c) speed = -1, ikili örnek 9 m/s ima ediyor: hız ≈ 9 (geçer), 11 m/s ima ediyor: 0. Sıçramadan sonraki iki temiz gözlemle hızın 4,5'e döndüğünü tekrar doğrula.
3. NavTrackTests.cpp NavTrack_VelocityRobust_Quantization: sınırı plana uydur: `bound = 0.1 * sqrt(2) / (interval/1000.0) + 0.05` (m/s, mutlak; 400 ms'de ≈ 0,404). Testi gerçekten nicemleme hatası üretecek biçimde değiştir: hareket x VE z eksenlerinde (örn. vx = 3,2, vz = 3,1 m/s; konumların ikisi de Quant1), aralıklar 0,1 m ızgarasına hizalı olmayan değerlerden (400, 413, 577, 700, 911, 1237, 1500 ms) ve t0 sürekli kaydırılarak; her biri için en az 200 yineleme; ayrıca gözlenen EN KÖTÜ mutlak hatayı `std::printf("NAVTRACK quant interval=%d worst_abs=%.3f bound=%.3f\n", ...)` ile yaz ve en az bir aralıkta worst_abs > 0,05 olduğunu CHECK et (testin boş olmadığının kanıtı). 400 ms altında kestirim yapılmayan mevcut son vaka kalsın.
4. NavTrackTests.cpp NavTrack_VelocityRobust_Reverse180: geçiş örneği alt vakası ekle (mevcutları değiştirme): 0/1500/3000 ms'de x = 0/6,75/13,5 (speed 45); hedef 3750 ms'de döner; 4500 ms'de x = 13,5, 6000 ms'de x = 6,75. 4500 ms'de büyüklük hatası en çok 1 paket sürer (hız 0 ya da herhangi bir değer; yön −x ya da 0 olmalı, +x ve büyüklük > 4,95 olmamalı), 6000 ms'de vx ≈ −4,5 (±1e-3) ve vz ≈ 0.
5. NavTrackTests.cpp NavTrack_VelocityRobust_SpeedChange: (a) yürüyüşe dönüş paketinde (speed 45) konumu gürültülü ver (örn. 7500 ms'de x = 41,5 → ima edilen 5,27 m/s): vx tam 4,95 ± 1e-3 (sprint sınırı 7,37'ye KIRPILMAMALI); (b) sprint paketinde (speed 67) aynı gürültüyle vx 7,37 ± 1e-3'e kırpılır; (c) speed = -1: ikili örnek 6,7 m/s ima ederse ≈ 6,7 (geçer; madde 1'in 10 m/s tavanı altında).
6. İsteğe bağlı küçük sıkılaştırma: NavTrack_VelocityRobust_Stale içinde `CHECK(f.Plan().leadSec <= 3.0f)` yerine `CHECK(f.Plan().leadSec <= params.maxLeadSec + 1e-4f)` kullan (yorumu güncelle).
7. Maddelerden sonra: ./tools/build.sh Release ve Debug (rc=0, yeni uyarı yok), ./tools/run-tests.sh Release ve Debug (0 failed; 11 yeni ad hâlâ [ OK ]), tools/nav-measure.sh velocity-robust (satırlar; ArrivalBunching zero_pct ≤ 1, p95 ≤ 0,20, max ≤ 0,60 hâlâ sağlanmalı: değerleri Tur 2 raporuna öncesi/sonrası olarak yaz). Quantization'ın yeni NAVTRACK quant satırlarını rapora ekle. git diff --check boş, ASCII + CRLF, yalnızca §4 dosyaları.
```
