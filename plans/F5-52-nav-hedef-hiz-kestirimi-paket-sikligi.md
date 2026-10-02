# F5-52: Hareketli hedef hız kestirimi gerçek `WIZ_MOVE` sıklığıyla çalışmıyor (1,5 sn gözlem aralığı → hız hep 0)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-52 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F5-04 (`NavTrack.h`: `NavTargetTracker`, `NavFollower`) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/12` §4.2 (hareketli hedef), §13.2 (hız kestirimi kuralı); CLI-05 (`WIZ_MOVE` ~1,5 sn); T-NAV-06; `docs/reports/degerlendirme-2026-10-02.md` DEG-19 |
| Tahmini büyüklük | S (2 dosya) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

`NavTargetTracker::Velocity` (`BotCore/NavTrack.h` ~satır 238-270) yalnızca **en yeni gözlemden geriye 1000 ms içindeki** örnekleri kullanır ve en yeni gözlem 1000 ms'den eskiyse hızı 0 sayar (`NavFollowParams::velocityWindowMs = 1000`, `minVelocitySpanMs = 100`). Gerçek istemci (ve bot) hareket halinde `WIZ_MOVE`'u **~1,5 sn'de bir** yollar (`docs/03` §13.2, `BotCore::kMovePeriodMs = 1500`); yani 1000 ms pencerede **hiç ikinci örnek yoktur** ve hız her zaman 0 çıkar, `NavFollower` hedefin ilerisine hiç öngörü noktası koymaz. Ölçüm (değerlendirme raporu §5.2, sabit hızlı 4,5 m/s hedef, gözlem aralığı → hız sıfır oranı): 500 ms %0, 1000 ms %0, **1500 ms %100, 1540 ms %100**. Bu plan kestirimi gerçek paket sıklığına uyarlar (`docs/12` §13.2): pencere ≥ 2 paket periyodu, paketin `speed` alanı durma/hız büyüklüğü bilgisi olarak kullanılır, gözlem yaşı öngörüye eklenir.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.2 (kural): hız = son iki gözlem arası konum farkı / süre (aralık 0,4–4,0 sn); en yeni gözlem 4,0 sn'den eskiyse veya son paketin `speed` alanı 0 ise hız 0; `P-NAV-VEL-WINDOW` = 4000 ms, `P-NAV-VEL-MIN-SPAN` = 400 ms `[A]`; öngörü süresine gözlem yaşı eklenir (üst sınır `maxLeadSec`).
- `BotCore/NavTrack.h`: `NavTargetTracker` (`Observe(tMs,x,z)`, `Velocity(nowMs, windowMs, minSpanMs, vx, vz)` ~238), `NavFollowParams` (~120-132), `NavFollower::Update` (~285-380: hız ve öngörü, `NavPredictLead`), `Sample` yapısı.
- Perception tarafı: `UnitObs`'a hız/geçmiş eklenmesi **F4-50**'dedir (`UpdateMove(..., speed, nowMs)`); bu plan `BotCore/Nav*`'ı F4'ten bağımsız tutar: hız alanı çağıranın verdiği isteğe bağlı bir argümandır.
- Mevcut testler `Tests/BotCoreTests/NavTrackTests.cpp`: `NavTrack_Tracker_Velocity` `Velocity()`'i **açık pencere/minSpan argümanlarıyla** çağırır (API uyumlu kalmalı), `NavTrack_Follower_*` varsayılan `NavFollowParams`'ı kullanır.

## 3. Kapsam

**Yapılacaklar**

1. `NavTargetTracker::Observe(tMs, x, z, int16_t speedField = -1)` (varsayılan argümanla **geriye uyumlu**; `-1` = bilinmiyor); `Sample`'a `int16_t speed`.
2. `NavTargetTracker::Velocity(...)` yeni kural (mevcut imza korunur; yeni kural pencere/minSpan **argümanlarıyla** çalışır): en yeni gözlemin `speed == 0` ise `(0,0)`; `m_count < 2` veya `nowMs - newest.t > windowMs` ise `(0,0)`; **son iki** gözlem (en yeni ve ondan bir önceki) arasındaki aralık `minSpanMs` ile `windowMs` arasındaysa hız = konum farkı / aralık, değilse `windowMs` içindeki en eski örnek yerine geriye doğru en yakın uygun örnek (aralık ≥ `minSpanMs`) kullanılır; en yeni `speed > 0` biliniyorsa hız büyüklüğü `speed/10 × 1,1` m/s ile sınırlanır (yön korunur). NaN/∞ girdi güvenli (mevcut kural).
3. `NavFollowParams` varsayılanları: `velocityWindowMs = 4000`, `minVelocitySpanMs = 400`. `NavFollower::Update`: öngörü süresi `lead = min(maxLeadSec, NavPredictLead(...) + gözlemYaşı)` (gözlem yaşı = `nowMs − newest.t` saniye); `ObserveTarget(tMs, x, z, speedField = -1)` ek argüman.
4. Testler (§6 K3). Mevcut testlerden yalnızca eski 1000/100 varsayılanına **örtük** bağlı olan varsa güncellenir ve gerekçesi raporda açıklanır; açık argümanlı çağrıların beklentisi değişmemeli.

**Kapsam dışı**

- Perception tablosuna hız/geçmiş ekleme (F4-50), `ObsTable` ile `NavFollower` bağlama (F5-55), eğri/dönüş modellemesi, ivme kestirimi.
- `docs/` değişikliği (Claude).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavTrack.h` | değiştir | yalnızca §3.1-3.3 |
| `Tests/BotCoreTests/NavTrackTests.cpp` | değiştir | yeni `TEST_CASE`'ler; gerekçeli eski test düzeltmesi |

2 kod dosyası (+ plan). Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-52 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`.
2. Önce tekrar üreten test (kırmızı): `NavTrack_Velocity_PacketCadence`. Sonra düzeltme.
3. Testler (adlar sabit):
   - `NavTrack_Velocity_PacketCadence`: sabit 4,5 m/s doğrusal hedef, gözlem aralığı 500/1000/1500/1540/2000 ms: her biri için tüm tick'lerde (100 ms adım, son gözlemden ≤ 4000 ms içinde) hız büyüklüğü 4,5 ± %10; **1500/1540 ms'de sıfır hız yok** (eski davranış %100 sıfırdı).
   - `NavTrack_Velocity_Speed0`: en yeni gözlemin `speed == 0` olduğu durdu paketinden sonra hız 0; `speed` bilinmiyorsa (−1) konum farkından hesaplanır; `speed = 45` iken (4,5 m/s) 20 m/s'lik sıçrama (ışınlanma) 4,95 m/s ile sınırlanır.
   - `NavTrack_Velocity_Stale`: en yeni gözlem 4000 ms'den eskiyse 0 (4000 sınır dahil), 4001 → 0; iki gözlem arası 400 ms'den kısaysa bir önceki uygun örneğe geri gidilir, yoksa 0; tek örnek 0.
   - `NavTrack_Velocity_Turn`: hedef 90° döndü (son iki örnek yeni yönü gösterir): hız son iki örnekten, eski yöne ait örneklerden değil.
   - `NavTrack_Follower_LeadAtPacketCadence`: bot sabit, hedef 1500 ms aralıklı gözlemlerle 4,5 m/s uzaklaşıyor: `NavFollower::Update` plan `leadSec > 0` ve `predX` hedefin son gözleminden **hareket yönünde ileri** (ölçüm: eski kodda `leadSec = 0`); gözlem yaşı lead'e eklenir ve `maxLeadSec`'i aşmaz.
4. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, `touch` ile yeniden derlenen dosyalarda yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; beş yeni test adı `[ OK ]`; mevcut `NavTrack_*` testleri geçer (güncellenen test varsa raporda gerekçe)
- [ ] K4: `NavTrack_Velocity_PacketCadence` 1500 ve 1540 ms için **sıfır hız oranı = %0** (satır çıktısı raporda; eski değer %100)
- [ ] K5: `Velocity()` imzası değişmedi (açık pencere/minSpan argümanlı eski çağrılar derlenir); `Observe` yeni argümanı varsayılanlıdır
- [ ] K6: `BotCore/NavTrack.h`'te `windows.h|stdafx|GameServer|shared/` yok; NaN/∞ girdide `NavFollower::Update` çökmez/NaN üretmez (mevcut testler)
- [ ] K7: `NavTrack_Perf` Release `near64` `Update` p95 ≤ 2 ms (AC-NAV-02) hâlâ geçer (değişim performansı bozmaz)
- [ ] K8: sunucu/`GameServer/`/`shared/`/`docs/` farkı 0; ASCII + CRLF; `git diff --check` boş
- [ ] K9 (Claude): değerlendirme betiğindeki `Velocity` döngüsünü düzeltilmiş başlıkla yeniden koşar: 1500/1540 ms sıfır oranı %0

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavTrack_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02-nav...bot/F5-52
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. 4000/400 ms değerleri `[A]`'dır (T-PERC-01/T-NAV-06 ile doğrulanır); `docs/12` §13.2 ile birlikte güncellenir.
- Hız kestirimi **gözlenen** (`O`) veriyle yapılır; sonuç `E` (tahmin) sınıfıdır (`docs/13` §5.2a): karar katmanı bunu tahmin olarak etiketler.
- Eğri hareketli hedefte kestirim hatalıdır; kabul kriterleri yalnızca doğrusal ve 90° dönüş vakalarını kapsar.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu:
- Kabul kriterleri öz-değerlendirme:
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
