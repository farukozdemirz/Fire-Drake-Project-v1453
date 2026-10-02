# F5-57: Takılma tespiti: hareket niyeti ve gerçek ilerleme birlikte (`NavProgressAssessor`, `BotCore/NavStuck.h` ekleme)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-57 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F5-09 (`NavStuckDetector`/`NavStuckMonitor`, `BotCore/NavStuck.h`), **F5-54** (`NavPacketCadenceParams()` ve `NavGuardBlockDetector`) — `KAPANDI` olmalı |
| İlgili gereksinim / kabul | `docs/12` §13.3 (niyet ilerlemesi ↔ paket teyidi ↔ `STUCK`/`BLOCKED_BY_GUARD`), MET-NAV-01 (≤ 2/bot-saat), MET-NAV-02, AC-NAV-01; proje sahibi kararı 2026-10-02 (madde 5: "takılma tespitini hareket niyeti ve gerçek ilerlemeyle birlikte değerlendir; normal paket aralıkları yanlış alarm üretmesin"); `docs/reports/degerlendirme-2026-10-02-ek.md` madde 5 |
| Tahmini büyüklük | M (2 dosyaya ekleme; vcxproj değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (proje sahibi kararı sonrası) |

---

## 1. Amaç

F5-09 detektörü yalnızca **konum yer değiştirmesine** (`NavStuckDetector::Observe(grid, tMs, x, z, moving, params)`) bakar ve `moving` bayrağını çağırandan alır; F5-54 paket sıklığı için hazır bir ayar (`NavPacketCadenceParams()`, 3200 ms) ve guard-engeli ayrımı ekledi. Eksik olan, **hareket niyetiyle gerçek ilerlemenin birlikte** değerlendirilmesidir: (1) niyet açıkken paket henüz gönderilmedi/gönderilemedi (bekleme) ile paketler gitti ama bot ilerlemiyor (takılma) ayrılmalı; (2) yer değiştirme tek başına **yol üzerindeki** ilerlemeyi göstermez: dar bir U-dönüşünde veya duvar boyunca geri-ileri giderken Öklid yer değiştirmesi küçük/büyük olabilir ama rota ilerlemesi pozitif/negatif; (3) hedefe varış, kısa son adım (< 1 m), waypoint dönüşü ve yeniden planlama takılma sayılmamalı. Ölçüm (`tools/nav-measure.sh stuck`, gerçek F5-09 kodu, bot paket zamanlaması, 600 sn): tick başına beslemede **F5-09 varsayılanı** gecikmeli tick modelinde (110,8±20 ms, %3 olasılıkla +250 ms) **6 yanlış takılma epizodu** üretir (≈ 36/saat/bot; hedef ≤ 2/saat), `NavPacketCadenceParams()` 0; gerçek takılmada tespit gecikmesi 1500 ms (varsayılan) ve 3200 ms (hazır ayar). Bu plan, niyet ve gerçek ilerlemeyi birleştiren saf bir değerlendirici ekler; F5-09/F5-54 API'sine ve testlerine dokunmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.3: `Niyet ilerlemesi` (tick hızında yerel simülasyon, **gönderilmiş** paketlere göre), `Paket teyidi` (paketten sonra botun kendi konumu paket konumuna eşit), `STUCK` (niyet etkin ve ardışık ≥ 2 paket periyodu boyunca yol üzerinde ilerleme < 1 m), `BLOCKED_BY_GUARD`, `OSCILLATION`; hedefe varış takılma değildir.
- `BotCore/NavStuck.h` (F5-09 + F5-54): `NavStuckDetector::Observe`, `NavStuckMonitor::Update(grid, tMs, x, z, moving, ...)` (merdiven), `NavPacketCadenceParams()`, `NavGuardBlockDetector`. **Bu API'leri ve mevcut testleri değiştirme.**
- `GameServer/Bot/ActionExecutor.cpp` `TickMove` (paket her ≥ 1500 ms'de bir tick'te; `elapsedMs` 3000'e kırpılır), `BotCore/BotMotion.h` (`kMovePeriodMs = 1500`): yalnızca oku.
- `BotCore/NavSmooth.h`/`NavPath.h`: yol = ara nokta dizisi; "yol üzerindeki ilerleme" izdüşüm mesafesidir.
- Mevcut test dosyası `Tests/BotCoreTests/NavStuckTests.cpp`: sentetik hareket üreteçlerini oku, yeniden kullan.

## 3. Kapsam

**Yapılacaklar** (`BotCore/NavStuck.h` sonuna **ekleme**; yalnızca standart kütüphane, sunucu başlığı yok, global/static durum yok, dinamik bellek yok):

1. `struct NavRoutePoint { float x, z; }` ve `inline float NavRouteProgressM(const NavRoutePoint * pts, int n, float x, float z)`: `(x, z)` konumunun polylin üzerine **izdüşümünün** başlangıçtan kümülatif mesafesi (metre); yol dışındaki konum en yakın izdüşüm noktasına bağlanır; `n < 2` → 0; NaN/∞ girdi 0.
2. `enum class NavProgressVerdict { Idle, Progressing, AwaitingPacket, Stalled, BlockedByGuard }` ve `struct NavProgressParams { int periodMs = 1550; int periods = 2; int toleranceMs = 100; float minProgressM = 1.0f; float arriveM = 1.0f; int guardWindowMs = 3200; }` (`[A]`, T-NAV-04 sonrası güncellenir).
3. `class NavProgressAssessor` (sabit boyutlu halka, 32 paket örneği; **dinamik bellek yok**):
   - `Reset()`; `SetIntent(bool active, int64_t nowMs)` (hareket niyeti başladı/bitti; başlayınca pencere sıfırlanır); `NotifyReplan(int64_t nowMs, float routeProgressM)` (yeni yol: ilerleme tabanı sıfırlanır, paket geçmişi korunur).
   - `OnPacketSent(int64_t tMs, float x, float z, float routeProgressM, float distToGoalM)`; `OnPacketRejected(int64_t tMs)` (guard CLI-05/CLI-08 reddi).
   - `NavProgressVerdict Assess(int64_t nowMs, const NavProgressParams &) const`: kural sırası: (1) niyet yok → `Idle`; (2) `distToGoalM <= arriveM` (son paketten) → `Progressing` (varış); (3) son `guardWindowMs` içinde ≥ 1 reddedilmiş ve hiç gönderilmiş paket yok → `BlockedByGuard`; (4) niyet etkin, son gönderilmiş paketten bu yana `periodMs + toleranceMs` dolmadı **ve** pencerede henüz `periods` paket yok → `AwaitingPacket` (bekleme, takılma değil); (5) pencerede (`periods × periodMs + toleranceMs`) ≥ `periods` gönderilmiş paket var ve **rota ilerlemesi** (en yeni − en eski, `routeProgressM`) < `minProgressM` → `Stalled`; (6) aksi → `Progressing`. Rota ilerlemesi negatif (yoldan geri gidiş/duvar boyunca salınım) → `Stalled`.
   - **Bütünleşme sözleşmesi (yorum + test):** çağıran `NavStuckMonitor::Update(..., moving = (verdict == Progressing || verdict == Stalled), ...)` verir; `AwaitingPacket`/`BlockedByGuard`/`Idle` iken `moving = false` (monitor "durdu: takılma değil" yoluna girer, çalışan kurtarma iptal edilir; hafıza korunur). `BlockedByGuard` ayrıca `NavGuardBlockDetector` ile aynı sonucu verir (tutarlılık testi).
4. Birim testleri (§5.3) ve `tools/nav-measure/nav_measure.cpp`'ye `progress` bölümü (aşağıdaki senaryolar kalıcı araçta; mevcut bölümlere dokunulmaz).

**Kapsam dışı**

- F5-09'un `NavStuckParams` varsayılanları, `NavStuckKind`, `NavStuckDetector`, `NavStuckMonitor`, `NavPickSideStep`, `NavStuckPenalties` ve F5-54'ün `NavPacketCadenceParams`/`NavGuardBlockDetector`'ı **değişmez**; mevcut testler değişmeden geçer.
- Telemetri (`NAV_STUCK`/`NAV_RECOVERY`), `ActionExecutor` kancası, hedef bırakma kararı (F5-55/karar katmanı), `GameServer/`, `shared/`, `docs/` (Claude).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavStuck.h` | değiştir | yalnızca sona ekleme (`git diff` yalnızca `+` satırları) |
| `Tests/BotCoreTests/NavStuckTests.cpp` | değiştir | yalnızca sona yeni vakalar |
| `tools/nav-measure/nav_measure.cpp` | değiştir | yalnızca yeni `progress` bölümü |

`.vcxproj` dosyalarına dokunulmaz. Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-57 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`.
2. `NavStuck.h` sonuna §3 ekleri (ASCII + CRLF).
3. Testler (adlar sabit; sentetik zaman, `Rng` sabit tohum, belirlenimli; paket üreteci: niyet etkin, paket her ≥ 1500 ms'de bir tick'te, tick modeli 100 / 100±10 / 110,8±20 ms ve %3 olasılıkla +250 ms gecikme; her model 600 sn sanal süre; konum yalnızca paket anında hızla ilerleyen noktaya sıçrar; rota doğrusal veya köşeli polylin):
   - `NavProgress_RouteProgress`: izdüşüm (düz, L şeklinde, yol dışı nokta, ilk/son nokta, `n < 2`, NaN).
   - `NavProgress_NormalWalk_NoAlarm`: 4,5 m/s, 6,7 m/s ve köşeli rota (waypoint'te kısalan adım) için her tick'te `Assess`: **`Stalled` hiç yok**; `AwaitingPacket` yalnızca ilk periyotta/gönderim arasında; `Progressing` baskın. Satır çıktısı: `ticks`, `stalled`, `awaiting`, `progressing`, `false_alarms` (= `stalled`) üç tick modelinde **0**. Aynı akış `NavStuckMonitor::Update` ile (bütünleşme sözleşmesi: `moving` = verdict'e göre) kullanıldığında `Episodes() == 0`.
   - `NavProgress_Stalled`: paketler akıyor ama rota ilerlemesi 0 (aynı konum): `periods × periodMs + toleranceMs` (3200 ms) dolmadan hiçbir zaman `Stalled`; dolunca `Stalled`; yavaş ama ilerleyen (1,2 m/3,2 sn) → `Progressing`.
   - `NavProgress_UTurn_vs_Displacement`: dar U-dönüşünde (yol ilerlemesi pozitif, Öklid yer değiştirmesi < 1 m olan pencere) `Progressing` (F5-09 tek başına `NoProgress` verirdi: aynı akış `NavStuckDetector`'a verilip fark **belgelenir**, `CHECK` ile sabitlenir); duvar boyunca geri-ileri (rota ilerlemesi ≤ 0, yer değiştirme > 1 m) → `Stalled`.
   - `NavProgress_Awaiting`: niyet açık, son paketten `periodMs + toleranceMs` dolmadan → `AwaitingPacket`; tek paketin 2,5 sn gecikmesi (tek seferlik) `Stalled` üretmez.
   - `NavProgress_Guard`: tüm paketler reddediliyor → `BlockedByGuard` (`Stalled` değil); `NavGuardBlockDetector` ile aynı karar; reddedilen paketlerin ardından gönderilmiş paket varsa `BlockedByGuard` biter.
   - `NavProgress_Arrival_Replan`: `distToGoalM <= arriveM` → `Progressing`; `NotifyReplan` sonrası yeni yolun başında pencere dolmadan `Stalled` yok; `SetIntent(false)` → `Idle`.
   - `NavProgress_Determinism`: aynı olay dizisi → aynı karar dizisi; `Reset()` sonrası ilk durum.
4. `tools/nav-measure.sh progress`: aynı senaryolar kalıcı araçta (üç tick modeli × (F5-09 varsayılanı tek başına, F5-54 hazır ayarı tek başına, `NavProgressAssessor` + monitor) yanlış alarm epizodu tablosu; gerçek takılma tespit gecikmesi).
5. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, değişen dosyalar `touch` edilince yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; sekiz yeni test adı `[ OK ]`; F5-09 ve F5-54 testleri değişmeden geçer
- [ ] K4: `NavProgress_NormalWalk_NoAlarm` üç tick modelinde 600 sn × 3 senaryoda yanlış alarm (`Stalled` ve monitor epizodu) **0**; satır çıktısı raporda
- [ ] K5: `Stalled` yalnızca ardışık ≥ `periods` gönderilmiş paket ve ≥ 3200 ms sonra; `AwaitingPacket`/`BlockedByGuard`/`Idle` iken hiçbir koşulda `Stalled` yok; U-dönüşü vakası `Progressing`
- [ ] K6: `BotCore/NavStuck.h`'te `windows.h|stdafx|GameServer|shared/` yok; yeni kodda dinamik bellek (`new|malloc|std::vector`) ve global/static değişken yok; `git diff gece/2026-10-02-nav...bot/F5-57 -- BotCore/NavStuck.h Tests/BotCoreTests/NavStuckTests.cpp` yalnızca `+` satırları
- [ ] K7: `git diff --stat` yalnızca §4; `GameServer/`, `shared/`, `docs/`, `.vcxproj` farkı 0; ASCII + CRLF; `git diff --check` boş
- [ ] K8 (Claude): `tools/nav-measure.sh stuck` ve `progress` çıktılarını güncel kod üzerinde yeniden koşar; F5-09 varsayılanının 6 epizodluk yanlış alarmının `NavProgressAssessor` ile 0'a indiğini doğrular
- [ ] K9 (**oyun içi kanıt, bu planda kapanmaz**): T-NAV-04 (dar geçit/köprü 50 geçiş) ve yürüyüş testlerinde gerçek paket zamanlamasıyla `NAV_STUCK` oranı ≤ 2/bot-saat ve kurtarma p95 ≤ 5 sn (F5-55); `docs/reports/degerlendirme-takip.md` satırı kanıta kadar `BEKLİYOR`

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavProgress_|NavStuck|tests,"
./tools/run-tests.sh Debug
tools/nav-measure.sh stuck && tools/nav-measure.sh progress
git diff --stat gece/2026-10-02-nav...bot/F5-57
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. Eşikler (`3200 ms`, `1 m`, `periods = 2`, 32 örnek) `[A]`'dır; T-NAV-04 sonrası güncellenir ve `docs/12` §13.3 ile birlikte değişir.
- Değerlendirici **gönderilmiş** paketlere bakar; paket göndermeyi tetiklemez ve `ActionExecutor` zamanlamasını değiştirmez. `Stalled` bir tespittir; ne yapılacağı (yeniden planla, yan adım, hedefi bırak) F5-09 merdivenindedir.
- Rota ilerlemesi için çağıranın güncel yolu (waypoint listesi) vermesi gerekir; yol yoksa (`n < 2`) değerlendirici yalnızca yer değiştirmeye düşmez: `Progressing`/`Stalled` kararı için F5-09 detektörü kullanılmaya devam eder (çağıran sözleşmesi).

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
