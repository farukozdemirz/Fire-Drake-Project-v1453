# F5-54: Takılma tespiti: niyet ilerlemesi ile paket gönderimi ayrı izlenir — `BotCore/NavStuck.h`

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-54 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F5-02/F5-03 (yol) — `KAPANDI`; F5-09 (kurtarma aşamaları, nav hattı) bu detektörü **kullanır**: F5-09 yazılmadan önce bu plan ve `docs/12` §13.3 okunmalı |
| İlgili gereksinim / kabul | `docs/12` §10 ve §13.3 (tanım), MET-NAV-01 (≤ 2/bot-saat), MET-NAV-02, AC-NAV-01; `docs/reports/degerlendirme-2026-10-02.md` DEG-17 |
| Tahmini büyüklük | S–M (2 yeni dosya + 2 proje satırı) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

`docs/12` §10'daki tespit ("hareket halindeyken 1,5 sn'de yol üzerindeki ilerleme < 1 m", "4 sn'de aynı iki hücre arasında ≥ 3 salınım") gerçek hareket paketi sıklığıyla (~1,5 sn, CLI-05; botun `kMovePeriodMs = 1500`) uyumsuzdur: konum yalnızca paket başına değişir (iki paket arası ~6,75 m), 1,5 sn'lik pencere bir paket aralığına eşittir (marj sıfır: tek bir geciken/reddedilen paket yanlış alarm üretir) ve 4 sn'de en çok 3 konum örneği olduğundan salınım ölçütü **ulaşılamazdır**. Değerlendirme simülasyonu (tick 100±10 ms ve 110,8±20 ms, %3 olasılıkla +250 ms gecikme) botun **kendi** zamanlamasından yanlış alarm üretmediğini gösterdi (0/~5000 tick); sorun ölçülmüş bir alarm oranı değil, **tanım hatasıdır**. Bu plan tespiti yeniden tanımlar (`docs/12` §13.3): yerel hareket ilerlemesi ile paket gönderimi ayrı izlenir; pencere ardışık ≥ 2 paket periyodudur; guard reddi takılma değildir; salınım paket konumlarından (≥ 4 paket) tanımlanır. Saf mantık; kurtarma aşamaları (F5-09) ve sunucu entegrasyonu (F5-55) kapsam dışıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.3 tablosu: `Niyet ilerlemesi`, `Paket teyidi`, `STUCK` (niyet etkin ve ardışık ≥ 2 paket periyodu ≥ 3,1 sn boyunca yol üzerindeki ilerleme < 1 m), `BLOCKED_BY_GUARD`, `OSCILLATION` (son 8 sn'de ≥ 4 paket konumu, A→B→A→B, ≥ 3 yön değişimi). Hedefe varış adımı (< 1 m) takılma değildir; yeniden planlama (500 ms) paket sıklığından bağımsız.
- `BotCore/BotMotion.h` (`kMovePeriodMs = 1500`, `CheckMoveStep`), `GameServer/Bot/ActionExecutor.cpp` `TickMove` (paket her ≥ 1500 ms'de bir tick'te gönderilir; `elapsedMs` 3000'e kırpılır).
- `BotCore/NavPath.h`/`NavSmooth.h`: yol = hücre/ara nokta dizisi; "yol üzerindeki ilerleme" çağıranın hesapladığı kümülatif mesafedir (bu planda polylin izdüşümü yardımcısı **vardır**, aşağıda).
- Test düzeni: sentetik zaman (milisaniye sayacı) ile belirlenimli simülasyon; `Rng`.

## 3. Kapsam

**Yapılacaklar** (`BotCore/NavStuck.h`; yalnızca standart kütüphane, sunucu başlığı yok, global/static durum yok):

1. `inline float NavPathProgress(const NavPoint * pts, int n, float x, float z)`: `pts` yol ara noktaları (`struct NavPoint { float x, z; }`), `(x,z)` konumunun polylin üzerine **izdüşümünün** başlangıçtan kümülatif mesafesi (metre; yol dışındaki konum en yakın izdüşüm noktasına bağlanır; `n < 2` → 0).
2. `struct NavStuckParams { float minProgressM = 1.0f; int periodMs = 1550; int periods = 2; int toleranceMs = 100; int oscWindowMs = 8000; int oscMinPackets = 4; int oscMinTurns = 3; float arriveM = 1.0f; }` — varsayılan pencere `periods × periodMs + toleranceMs = 3200 ms` (≥ 3,1 sn).
3. `enum class NavStuckKind { None, Stuck, Oscillation, BlockedByGuard }`.
4. `class NavStuckDetector` (sabit boyutlu halka, dinamik bellek yok):
   - `Reset()`; `SetMoving(bool moving, int64_t nowMs)` (hareket niyeti başladı/bitti; başlatınca pencere sıfırlanır); `NotifyReplan(int64_t nowMs)` (yeni yol: ilerleme tabanı sıfırlanır, geçmiş korunmaz).
   - `OnPacketSent(int64_t nowMs, float x, float z, float pathProgressM, float distToGoalM)`: **gönderilmiş** her hareket paketi (konum + o andaki yol ilerlemesi + hedefe uzaklık).
   - `OnPacketRejected(int64_t nowMs)`: guard (CLI-08/CLI-05) paketi reddetti.
   - `NavStuckKind Evaluate(int64_t nowMs, const NavStuckParams &) const`: kural sırası: (1) hareket niyeti yok → `None`; (2) hedefe `arriveM` içinde → `None`; (3) son `periodMs × periods + toleranceMs` içinde en az bir `OnPacketRejected` ve **hiç gönderilmiş paket yok** → `BlockedByGuard`; (4) son `oscWindowMs`'de ≥ `oscMinPackets` gönderilmiş paket, konumlar iki bölge arasında gidip geliyor (ardışık paketlerde ilerleme işareti değişimi ≥ `oscMinTurns`, her ikisi de yol ilerlemesi kazanımı < `minProgressM`) → `Oscillation`; (5) pencere içindeki **en eski** ve **en yeni** gönderilmiş paketin yol ilerlemesi farkı < `minProgressM` ve pencere uzunluğu ≥ `periods × periodMs` boyunca niyet etkin → `Stuck`; aksi `None`. Pencere dolmadan (yeterli paket yok) `None` — **paket gönderimi beklenirken asla `Stuck` denmez**.
   - Sayaçlar: `Stuck/Oscillation/BlockedByGuard` tespit sayıları (`Counts()`), MET-NAV-01 için.
5. Birim testleri (§5.3).

**Kapsam dışı**

- Kurtarma aşamaları (F5-09), `NAV_STUCK`/`NAV_RECOVERY` telemetrisi, `ActionExecutor` kancası (F5-55), hedef bırakma kararı (karar katmanı), `docs/` değişikliği (Claude).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavStuck.h` | yeni | |
| `Tests/BotCoreTests/NavStuckTests.cpp` | yeni | |
| `BotCore/BotCore.vcxproj` | değiştir | tek `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | tek `ClCompile` satırı |

Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-54 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`.
2. `NavStuck.h`; yeni dosyalar ASCII + CRLF.
3. Testler (adlar sabit; sentetik zaman, belirlenimli):
   - `NavStuck_Progress`: polylin izdüşümü (düz, L şeklinde, yol dışı nokta, `n<2`, ilk/son nokta).
   - `NavStuck_NormalWalk_NoAlarm`: 4,5 m/s düz yürüyüş, tick 100±10 ms ve 110,8±20 ms (+%3 olasılıkla +250 ms gecikme, `Rng` sabit tohum), 600 sn sanal süre, paket her ≥ 1500 ms: **sıfır** `Stuck`/`Oscillation`; sprint 6,7 m/s de aynı; tek paketin 2,5 sn gecikmesi (tek seferlik) alarm üretmez.
   - `NavStuck_Stuck`: hareket niyeti etkin, paketler aynı konumda (ilerleme 0): **3200 ms'den önce** `None`, `≥ 3200 ms` sonra `Stuck`; ilerleme 1,2 m/3,1 sn (yavaş ama ilerliyor) → `None`; hedefe 1 m içinde durma → `None`; `SetMoving(false)` → `None`.
   - `NavStuck_Guard`: tüm paketler reddediliyor (`OnPacketRejected`), gönderilmiş paket yok: `BlockedByGuard`, **`Stuck` değil**; reddedilen paketlerin ardından gönderilmiş paket varsa kural 5 normal çalışır.
   - `NavStuck_Oscillation`: 8 sn içinde A,B,A,B,A (5 paket, her biri ~6 m ileri/geri): `Oscillation`; aynı paketler tek yönde ilerliyorsa `None`; 4 sn içinde yalnızca 3 paket: `None` (**dar pencerede salınım ölçütü ulaşılamaz**, tasarım gereği).
   - `NavStuck_Replan`: `NotifyReplan` sonrası ilerleme tabanı sıfırlanır: yeni yolun başında ilk pencere dolmadan `Stuck` denmez; art arda iki `Stuck` tespitinde sayaç 2.
   - `NavStuck_Determinism`: aynı olay dizisi iki kez → aynı karar dizisi; `Reset()` sonrası ilk durum.
4. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni dosyalar için uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; yedi yeni test adı `[ OK ]`; mevcut testler geçer
- [ ] K4: `NavStuck_NormalWalk_NoAlarm`: 600 sn × 3 senaryoda yanlış alarm **0** (satır çıktısı: `ticks`, `packets`, `false_alarms`)
- [ ] K5: `Stuck` yalnızca ardışık ≥ 2 paket periyodu (≥ 3200 ms) sonra; pencere dolmadan hiçbir koşulda `Stuck`/`Oscillation` yok
- [ ] K6: `BotCore/NavStuck.h`'te `windows.h|stdafx|GameServer|shared/` yok; dinamik bellek (`new|malloc|std::vector`) yok; global/static durum yok
- [ ] K7: `git diff --stat gece/2026-10-02-nav...bot/F5-54` yalnızca §4; `GameServer/`, `shared/`, `docs/` farkı 0; ASCII + CRLF; `git diff --check` boş
- [ ] K8 (Claude): bağımsız Python simülasyonuyla `NavStuck_NormalWalk_NoAlarm` senaryosunu çapraz kontrol eder (aynı olay modeli); çalışma zamanı doğrulaması F5-55'te (T-NAV-04)

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavStuck_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02-nav...bot/F5-54
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. Eşikler (`3200 ms`, `1 m`, `8 sn`, `4 paket`) `[A]`'dır; T-NAV-04 (dar geçit/köprü takılma oranı) sonrası güncellenir ve `docs/12` §13.3 ile birlikte değişir.
- Detektör **gönderilmiş** paketlere bakar; paket gönderimini tetiklemez ve `ActionExecutor` zamanlamasını değiştirmez.
- `Stuck` kararı bir **tespit**tir; ne yapılacağı (yeniden planla, yan adım, hedefi bırak) F5-09'dadır.

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
