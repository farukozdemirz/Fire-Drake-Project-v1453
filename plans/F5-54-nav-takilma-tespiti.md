# F5-54: Takılma tespiti paket sıklığına uyarlanır: `NavStuckParams` hazır ayarı + guard-engeli dedektörü (`BotCore/NavStuck.h` ekleme)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-54 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F5-09 (`NavStuckDetector`/`NavStuckMonitor`, `BotCore/NavStuck.h`) — `KAPANDI`. Bu plan ilk yazımda (değerlendirme, 2026-10-02) "yeni dosya" idi; nav döngüsü F5-09'u önce uyguladığından **F5-09'a eklenen küçük bir değişikliğe** uyarlandı (2026-10-02, birleştirme sırasında) |
| İlgili gereksinim / kabul | `docs/12` §10 ve §13.3 (tanım), MET-NAV-01 (≤ 2/bot-saat), MET-NAV-02, AC-NAV-01; `docs/reports/degerlendirme-2026-10-02.md` DEG-17 |
| Tahmini büyüklük | S (2 mevcut dosyaya ekleme; vcxproj değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme; F5-09 sonrası uyarlama) |

---

## 1. Amaç

`docs/12` §10'daki varsayılanlar (`noProgressMs = 1500`, `oscWindowMs = 4000`) botun **gerçek hareket paketi** sıklığıyla (~1,5 sn, CLI-05; `kMovePeriodMs = 1500`) uyumsuzdur: sunucudaki bot konumu yalnızca paket başına değişir (iki paket arası ~6,75 m). 1,5 sn'lik ilerlemesizlik penceresi bir paket aralığına eşittir (marj sıfır: tek bir geciken veya reddedilen paket yanlış alarm üretir) ve 4 sn'de en çok 3 konum örneği olduğundan salınım ölçütü (≥ 3 geçiş) ulaşılamazdır. `docs/12` §13.3 bunu şöyle düzeltir: `STUCK` = niyet etkin ve ardışık ≥ 2 paket periyodu (≥ 3,1 sn) boyunca ilerleme < 1 m; `OSCILLATION` = son 8 sn'de ≥ 4 paket konumu, A→B→A→B, ≥ 3 yön değişimi; `BLOCKED_BY_GUARD` = guard paketi reddediyor ve gönderilmiş paket yok (takılma sayılmaz).

F5-09 detektörü örnekleri **çağıranın verdiği** konumla besleniyor; bu plan **F5-09'un varsayılanlarına ve mevcut testlerine dokunmadan** (a) paket sıklığı için doğrulanmış bir parametre hazır ayarı ve (b) guard-engeli ayrımı ekler. Böylece sunucu bağlama planı (F5-55) gönderilmiş paket konumlarını besleyip hazır ayarı kullanır ve yanlış alarm üretmez; "varsayılanlar paket konumuyla neden yanlış alarm verir" da bir testle belgelenir.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.3 (tablo: `Niyet ilerlemesi`, `Paket teyidi`, `STUCK`, `BLOCKED_BY_GUARD`, `OSCILLATION`) ve §10.
- `BotCore/NavStuck.h` (F5-09): `NavStuckKind { None, NoProgress, Oscillation }`, `NavStuckParams` (alanlar ve `[O]`/`[A]` etiketleri), `NavStuckDetector::Observe(grid, tMs, x, z, moving, params)` (örnekler arası en az `kMinSpacingMs = 20`, kapasite 256), `NavStuckMonitor::Update(...)`. **Bu API'yi değiştirme.** Salınım kuralının geçişi hücre çiftiyle nasıl eşlediğini (`SameTransition`: sıralı mı sırasız mı) önce oku; hazır ayarın değerleri buna göre doğrulanır.
- `BotCore/BotMotion.h` (`kMovePeriodMs = 1500`), `GameServer/Bot/ActionExecutor.cpp` `TickMove` (paket her ≥ 1500 ms'de bir tick'te gönderilir; `elapsedMs` 3000'e kırpılır): yalnızca oku, değiştirme.
- Mevcut test dosyası `Tests/BotCoreTests/NavStuckTests.cpp` (F5-09): yardımcı sentetik hareket üreticilerini oku, yeniden kullan.

## 3. Kapsam

**Yapılacaklar** (`BotCore/NavStuck.h` sonuna **ekleme**; yalnızca standart kütüphane, sunucu başlığı yok, global/static değişken yok):

1. `inline NavStuckParams NavPacketCadenceParams()`: paket konumuyla beslenen kullanım için hazır ayar. Başlangıç değerleri (`[A]`, T-NAV-04 sonrası güncellenir): `noProgressMs = 3200` (≥ 2 paket periyodu + 100 ms tolerans), `minProgressM = 1.0f`, `oscWindowMs = 8000`, `oscSwings = 3`; diğer alanlar `NavStuckParams` varsayılanı. Salınım kuralının paket konumlarıyla (A,B,A,B,A) tetiklendiği testle doğrulanmalı; `SameTransition` eşleşmesi nedeniyle `oscSwings` farklı çıkarsa değeri **testle bulunan en küçük çalışan değere** ayarla ve Uygulayıcı Raporu'nda belirt.
2. `class NavGuardBlockDetector` (sabit boyutlu, dinamik bellek yok): `Reset()`; `OnPacketSent(int64_t tMs)`; `OnPacketRejected(int64_t tMs)`; `bool Blocked(int64_t nowMs, int windowMs = 3200) const`: son `windowMs` içinde **en az bir** reddedilmiş ve **hiç gönderilmiş paket yok** → `true` (`BLOCKED_BY_GUARD`, takılma değil: bu durum `NavStuckMonitor` merdivenine sokulmaz, çağıran ayrı ele alır); zaman geri giderse (`nowMs` < son olay) `false`. Sabit boyutlu halka (en az 16 olay).
3. Birim testleri (§5.3), `NavStuckTests.cpp` sonuna **ekleme**.

**Kapsam dışı**

- F5-09'un `NavStuckParams` varsayılanları, `NavStuckKind`, `NavStuckDetector`, `NavStuckMonitor`, `NavPickSideStep`, `NavStuckPenalties` ve **mevcut test vakaları değişmez**.
- `NavPathProgress`/yol izdüşümü (bu planda yok: paket konumunda gerçek ilerleme ≥ ~6 m iken takılma < 1 m olduğundan yer değiştirme yeterli; F5-55'te gerekirse eklenir).
- Telemetri (`NAV_STUCK`, `NAV_RECOVERY`), `ActionExecutor` kancası, hedef bırakma kararı, `docs/` değişikliği (Claude), `GameServer/`, `shared/`.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavStuck.h` | değiştir | yalnızca sona ekleme (§3); mevcut satırlar değişmez (`git diff` yalnızca `+` satırları gösterir) |
| `Tests/BotCoreTests/NavStuckTests.cpp` | değiştir | yalnızca sona yeni `TEST_CASE`'ler; mevcut vakalar değişmez |

`.vcxproj` dosyalarına dokunulmaz (dosya eklenmiyor). Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-54 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`.
2. `NavStuck.h` sonuna `NavPacketCadenceParams()` ve `NavGuardBlockDetector` ekle (dosya ASCII + CRLF kalır).
3. `NavStuckTests.cpp` sonuna şu testleri ekle (adlar sabit; sentetik zaman, `Rng` sabit tohum, belirlenimli). Ortak yardımcı: "paket konumu üreteci": hareket niyeti etkin, her ≥ 1500 ms'de bir paket (+%3 olasılıkla +250 ms gecikme), konum yalnızca paket anında hızla ilerleyen noktaya sıçrar; `NavStuckMonitor`/`NavStuckDetector`'a **yalnızca paket anlarında** `Observe`/`Update` çağrılır (gerçek bağlamayla aynı).
   - `NavStuckCadence_NormalWalk_NoAlarm`: 4,5 m/s ve 6,7 m/s (sprint) düz yürüyüş + bir de hedefe yürüyüş, her biri 600 sn sanal süre, **`NavPacketCadenceParams()`** ile: yanlış alarm **0** (satır çıktısı: `packets`, `false_alarms`); tek paketin 2,5 sn gecikmesi (tek seferlik) alarm üretmez.
   - `NavStuckCadence_DefaultsFalseAlarm`: **aynı** düz yürüyüş, F5-09 **varsayılan** `NavStuckParams()` ile paket konumlarıyla beslenince beklenen davranışı belgeler: bu test varsayılanların paket konumuyla neden uygun olmadığını göstermek içindir; gerçek sonucu ölçüp **aynen** doğrula (örn. yanlış alarm ≥ 1 ise `CHECK(falseAlarms >= 1)`; 0 çıkarsa testin adını ve gerekçesini Uygulayıcı Raporu'nda belirt ve `CHECK(falseAlarms == 0)` ile sabitle).
   - `NavStuckCadence_Stuck`: konum sabit (ilerleme 0), paketler akıyor: **3200 ms dolmadan** `None`; `≥ 3200 ms` sonra `NoProgress`; ilerleme 1,2 m/3,2 sn (yavaş ama ilerliyor) → `None`; `moving = false` → `None`.
   - `NavStuckCadence_Oscillation`: 8 sn içinde A,B,A,B,A (5 paket konumu, her biri ~6 m ileri/geri, farklı hücreler): `Oscillation`; aynı paketler tek yönde ilerliyorsa `None`.
   - `NavGuardBlock_Rules`: yalnızca reddedilmiş paketler (gönderilmiş yok) → pencere içinde `Blocked == true`, pencere dışında `false`; reddedilmiş ve ardından gönderilmiş paket varsa `false`; hiç olay yoksa `false`; zaman geri giderse `false`; halka 16'dan fazla olayda taşmadan çalışır.
   - `NavGuardBlock_Determinism`: aynı olay dizisi iki kez → aynı karar dizisi; `Reset()` sonrası ilk durum.
4. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, değişen dosyalar için yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, yeni uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; altı yeni test adı `[ OK ]` (`NavStuckCadence_NormalWalk_NoAlarm`, `NavStuckCadence_DefaultsFalseAlarm`, `NavStuckCadence_Stuck`, `NavStuckCadence_Oscillation`, `NavGuardBlock_Rules`, `NavGuardBlock_Determinism`); mevcut testler (F5-09 dahil) değişmeden geçer
- [ ] K4: `NavStuckCadence_NormalWalk_NoAlarm` üç senaryoda (4,5 m/s, 6,7 m/s, hedefe yürüyüş) 600 sn'de yanlış alarm **0**; çıktı satırı `packets`, `false_alarms` içerir
- [ ] K5: `NoProgress` yalnızca ≥ 3200 ms ilerlemesizlikten sonra; pencere dolmadan hiçbir koşulda `NoProgress`/`Oscillation` yok (hazır ayarla)
- [ ] K6: `BotCore/NavStuck.h`'te `windows.h|stdafx|GameServer|shared/` yok; yeni kodda dinamik bellek (`new|malloc`) ve global/static değişken yok; ASCII + CRLF
- [ ] K7: `git diff gece/2026-10-02-nav...bot/F5-54 -- BotCore/NavStuck.h Tests/BotCoreTests/NavStuckTests.cpp` yalnızca `+` satırları (mevcut satır silinmedi/değişmedi); `git diff --stat` yalnızca §4'teki iki dosya; `GameServer/`, `shared/`, `docs/`, `.vcxproj` farkı 0; `git diff --check` boş
- [ ] K8 (Claude): bağımsız Python simülasyonuyla `NavStuckCadence_NormalWalk_NoAlarm` senaryosunu çapraz kontrol eder; çalışma zamanı doğrulaması F5-55'te (T-NAV-04)

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavStuckCadence_|NavGuardBlock_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02-nav...bot/F5-54
git diff gece/2026-10-02-nav...bot/F5-54 -- BotCore/NavStuck.h | grep -c "^-[^-]"
```

(Son komutun çıktısı `0` olmalı: silinmiş/değişmiş satır yok.)

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. Eşikler (`3200 ms`, `1 m`, `8 sn`, `oscSwings`) `[A]`'dır; T-NAV-04 (dar geçit/köprü takılma oranı) sonrası güncellenir ve `docs/12` §13.3 ile birlikte değişir.
- Hazır ayar ve guard dedektörü **karar vermez**: `NavStuckMonitor` ve bağlama (F5-55) kullanır; `ActionExecutor` zamanlamasını değiştirmez.
- Bu planın mevcut F5-09 testlerini ve varsayılanlarını bozması kabul edilmez: bozulursa **durup** Uygulayıcı Raporu'nda yaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- **Durum:** UYGULANDI. Değişen dosyalarda yeni uyarı yok; `198 tests, 0 failed` (Release ve Debug); altı yeni test `[ OK ]`. Sunucu çalıştırılmadı (sunucusuz `BotCore` işi), sunucular `[DOWN]`.
- **Branch / commit'ler:** `bot/F5-54` (taban: `gece/2026-10-02-nav`). Kod commit'i `c5cda0b` (`[F5-54] NavStuck: paket sıklığı hazır ayarı ve guard-engeli dedektörü`; 2 dosya, +389). Rapor commit'i ayrıdır; push edilmedi.
- **Değişen dosyalar ve neden:**
  - `BotCore/NavStuck.h` (+98, yalnızca ekleme, mevcut satır değişmedi): dosya sonuna `NavPacketCadenceParams()` (noProgressMs 3200, minProgressM 1,0, oscWindowMs 8000, oscSwings 3; diğer alanlar `NavStuckParams` varsayılanı) ve `NavGuardBlockDetector` (16 olaylık sabit halka; `Reset`/`OnPacketSent`/`OnPacketRejected`/`Blocked`; dinamik bellek ve global/static yok).
  - `Tests/BotCoreTests/NavStuckTests.cpp` (+291, yalnızca ekleme; iki yeni `#include`: `BotCore/BotMotion.h`, `BotCore/Rng.h`): ortak paket-konumu üreteci `F554RunCadence` ve altı yeni `TEST_CASE`. Mevcut F5-09 vakaları değişmedi.
- **Derleme sonucu:**
  - `./tools/build.sh Release` → `BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Tests\BotCoreTests.exe` (rc=0).
  - `./tools/build.sh Debug` → `BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Debug\Tests\BotCoreTests.exe` (rc=0).
  - `./tools/run-tests.sh Release` ve `Debug` → `198 tests, 0 failed` (sırasıyla); altı yeni test `[ OK ]`.
- **Çıktı satırları (Release):**
  - `NAVSTUCK cadence normal walk45: packets=398 false_alarms=0 first_ms=-1` (sprint67, target45 aynı: 398/0)
  - `NAVSTUCK cadence normal forced_delay: packets=39 false_alarms=0 first_ms=-1`
  - `NAVSTUCK cadence defaults: packets=397 fed=6001 false_alarms=33 episodes=11 first_ms=61500`
  - `NAVSTUCK cadence preset: packets=397 fed=6001 false_alarms=0 episodes=0 first_ms=-1`
- **Kabul kriterleri öz-değerlendirme:**
  - K1 ✔ (`Release` rc=0, değişen dosyalarda yeni uyarı yok), K2 ✔ (`Debug` rc=0, yeni uyarı yok).
  - K3 ✔ (`198 tests, 0 failed` iki yapılandırmada; altı ad `[ OK ]`; F5-09 dahil mevcut testler değişmedi).
  - K4 ✔ (üç senaryo 600 sn, yanlış alarm 0; çıktı `packets`/`false_alarms` içeriyor).
  - K5 ✔ (`NavStuckCadence_Stuck`: 100 ms örneklemeyle ilk `NoProgress` tam 3200 ms; 1,2 m/3,2 sn ilerleme ve `moving=false` → alarm yok).
  - K6 ✔ (`NavStuck.h`'te yasaklı include yok; yeni satırlarda `new`/`malloc`/`static` yok; ASCII + CRLF, `file` ile teyit).
  - K7 ✔ (`git diff gece/2026-10-02-nav...bot/F5-54 -- BotCore/NavStuck.h Tests/BotCoreTests/NavStuckTests.cpp` yalnızca `+`; iki dosyada silinen satır 0; `--stat` yalnızca bu iki dosya; `GameServer/`, `shared/`, `docs/`, `.vcxproj`/`.filters` farkı 0; `git diff --check` boş).
  - K8 → doğrulamada Claude'da (bağımsız Python simülasyonu; çalışma zamanı F5-55).
- **Plandan sapmalar ve gerekçeleri:**
  - `NavStuckCadence_NormalWalk_NoAlarm` ve tek `2500 ms` gecikme testi, planın üreteci gibi **paket anlarında** beslenir (yanlış alarm 0). `NavStuckCadence_DefaultsFalseAlarm` ise planın "paket konumlarıyla beslenince" ifadesinden bilinçli olarak saparak **her tick (100 ms)** beslenir: F5-09 varsayılanlarının yanlış alarmı (§13.3'teki 600 sn'de ~6 epizot bulgusu) yalnızca tick beslemede görülür; `tools/nav-measure.sh stuck` bunu doğrular (`feed=every_tick` default: 6, `feed=packets_only` default: 0). Test bu nedenle varsayılanla `false_alarms ≥ 1` (ölçülen 33 alarm / 11 epizot) ve hazır ayarla kontrol olarak `0` sınar; plan "gerçek sonucu ölç, 0 çıkarsa sabitle" dediğinden sapma değil, ölçülmüş davranıştır.
  - `NavStuckCadence_Stuck`, `3200 ms` sınırını tam yakalamak için 100 ms'de örnekler (plan üretecinin paket-anı örneklemesiyle 3200 sınırı görülemez); bu `STUCK_TRUE cadence_3200 detected_after_ms=3200` ölçümüyle uyumludur.
  - `NavPacketCadenceParams()` içinde `oscSwings` değiştirilmedi: `SameTransition` sırasız eşleştiği için A,B,A,B dizisi 4. pakette `oscSwings = 3` ile tetikleniyor (mevcut F5-09 salınım testiyle tutarlı), testle doğrulandı.
  - NormalWalk'ta alt sınır `packets >= 340` (plan sayı istemiyordu); %3 gecikme 600 sn'de ~398 paket verir.
- **Açık sorular:** Yok (engel yok). Eşikler `[A]`; T-NAV-04 (dar geçit/köprü takılma oranı) ve F5-55 bağlaması sonrası `docs/12` §13.3 ile birlikte güncellenir.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- **Karar:** DOĞRULANDI
- **İncelenen commit:** `072d6b5` (kod commit'i `c5cda0b`; taban `gece/2026-10-02-nav`; paralel hat `nav`, sunuculara dokunulmadı)

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release rc=0, yeni uyarı yok | ✔ | `./tools/build.sh Release` rc=0; `touch Tests/BotCoreTests/NavStuckTests.cpp` ile yeniden derlemede `NavStuck`/uyarı/`C4xxx` çıktısı yok |
| K2 Debug rc=0, yeni uyarı yok | ✔ | `./tools/build.sh Debug` rc=0, günlükte `warning` 0 |
| K3 `0 failed`, altı yeni test `[ OK ]`, mevcut testler geçer | ✔ | `run-tests.sh Release` ve `Debug`: `198 tests, 0 failed`; altı ad iki yapılandırmada `[ OK ]` |
| K4 üç senaryo 600 sn yanlış alarm 0, çıktı `packets`/`false_alarms` | ✔ | `walk45/sprint67/target45: packets=398 false_alarms=0`; `forced_delay: packets=39 false_alarms=0` |
| K5 `NoProgress` yalnızca ≥ 3200 ms sonra; pencere dolmadan alarm yok | ✔ | `NavStuckTests.cpp` `NavStuckCadence_Stuck`: t<3200 `None`, ilk alarm tam 3200 `NoProgress`; 1,2 m/3,2 sn ve `moving=false` → `None`; `NavStuck.h:241-247` anchor ≤ eşik kuralı |
| K6 yasaklı include/dinamik bellek/static yok, ASCII+CRLF | ✔ | `grep windows.h\|stdafx\|GameServer\|shared/` boş; eklenen satırlarda `new/malloc/static/vector` yok; `file`: ASCII, CRLF (574/574 satır `\r`) |
| K7 yalnızca `+`, kapsam, `diff --check` | ✔ | `grep -c "^-[^-]"` iki dosyada `0`; `--stat`: `NavStuck.h` +98, `NavStuckTests.cpp` +291, plan dosyası; `GameServer/`, `shared/`, `docs/`, `.vcxproj` farkı yok; `git diff --check` boş |
| K8 (Claude) bağımsız Python simülasyonu | ✔ | `/tmp/f554_sim.py` (detektör mantığı bağımsız yeniden yazıldı, 200 tohum × 3 senaryo × 600 sn): ~397 paket, en çok alarm 0; 2500 ms gecikme alarmsız; F5-09 varsayılanı paket-anı beslemede 0, tick beslemede 21–66 alarm (≥ 1), hazır ayar tick beslemede 0 — uygulayıcının ölçümüyle (398 paket / 0; varsayılan 33 alarm) uyumlu |

**Bulgular (engel değil; not):**

1. `Tests/BotCoreTests/NavStuckTests.cpp` `NavStuckCadence_DefaultsFalseAlarm`: plan "paket konumlarıyla besle" derken uygulayıcı bilinçli olarak her tick (100 ms) besledi. Gerekçe doğru: `docs/12` §13.3 notu da yanlış alarmın tick beslemede çıktığını yazar; paket-anı beslemede varsayılan 0 verir (Python ile teyit edildi). Sapma Uygulayıcı Raporu'nda belirtilmiş, kabul edilir.
2. `BotCore/NavStuck.h:494-570` `NavGuardBlockDetector`: halka 16 olay; pencere (3200 ms) içinde > 16 olay olursa en eski `OnPacketSent` ezilebilir ve `Blocked` yanlışlıkla `true` dönebilir. Gerçek kullanımda guard reddi paket başına (≥ 1,5 sn) olduğundan 3,2 sn'de ≤ ~3 olay beklenir; tehlike yok. F5-55 bağlaması reddi tick başına çağırırsa yeniden bakılmalı.
3. `NavStuckCadence_Stuck` 100 ms örnekler (plan üretecinin paket-anı örneklemesi 3200 sınırını gösteremez): kabul, rapora yazılmış.

Çalışma zamanı doğrulaması F5-55'te (T-NAV-04); eşikler `[A]`.
