# F5-53: Çoklu bot yol sorgusu için tick bütçesi, adil kuyruk, faz kaydırma ve yol önbelleği — `BotCore/NavBudget.h`

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-53 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F5-02 (A*), F5-04 (`NavFollower`) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/12` §13.5, AC-NAV-07 (yeni), MET-PERF-02 (16 bot p95 ≤ 5 ms **tüm** BotManager), MET-PERF-03; `docs/reports/degerlendirme-2026-10-02.md` DEG-21 |
| Tahmini büyüklük | M (2 yeni dosya + 2 proje satırı) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

A* tek başına hızlıdır (near64 p95 ~0,4 ms) ama çok bot aynı tick'te sorgu yaparsa toplam, tüm `BotManager` tick bütçesini (16 bot için p95 ≤ 5 ms) aşar. Ölçüm (değerlendirme raporu §5.5, zone 71, WSL `g++ -O2`, tek iş parçacığı, aynı tick'te N bot tek sorgu):

| Sorgu seti | Bot/tick | Tick toplamı p50 / p95 / p99 / max (ms) |
|---|---|---|
| near64 | 16 | 1,43 / 2,83 / 4,51 / 7,24 |
| mid150 (≤ 150 hücre) | 16 | 5,91 / 8,82 / 10,20 / 11,27 |
| tüm harita | 16 | 20,8 / 30,2 / 33,0 / 38,7 |
| near64 | 64 | 6,26 / 9,49 / 12,16 / 12,92 |

Hedef takipçi (`NavFollower`) her 500 ms'de yeniden planlar; hepsinin aynı tick'e düşmesi en kötü durumdur. Bu plan `docs/12` §13.5 stratejisini saf mantık olarak uygular: **tick başına nav bütçesi**, **adil kuyruk** (bekleme sınırlı, tick başına döndürülen sıra), **bot başına yeniden planlama fazı** ve **kısa ömürlü yol önbelleği**. Sunucu entegrasyonu F5-55'tir.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.5 (strateji ve sayılar: `P-NAV-TICK-BUDGET-MS` = 1,5 ms, `P-NAV-MAX-WAIT` = 1 sn, faz `slot % 5 × 100 ms`, önbellek TTL 30 sn), AC-NAV-07, `docs/13` §3.1 (tick bütçesi p95 ≤ 5 ms; aşılırsa botlar gruplara bölünür), ADR-0005 (bot tick'i IOCP thread'inde: sorgular bölünemez, v1'de ayrı iş parçacığı yok).
- `BotCore/NavPath.h` `NavPathfinder::Find` (≈ 4,2 MB iç havuz: bot başına örnek değil **paylaşılan** örnek kullanılabilir; thread-safe değil: tek thread'de seri), `NavPathResult::{cells,length,expanded}`.
- `BotCore/NavTrack.h` `NavFollower::Update` (yeniden planlama koşulları: ilk, 6 m, 500 ms). Bütçe/kuyruk, `NavFollower`'ı değiştirmez: çağıran `Update`'i yalnızca zamanlayıcı izin verince çağırır.
- Test düzeni: `Tests/BotCoreTests/NavPathTests.cpp` (T-NAV-03 performans düzeni), `Rng`.

## 3. Kapsam

**Yapılacaklar** (`BotCore/NavBudget.h`; yalnızca standart kütüphane, sunucu başlığı yok, global/static durum yok):

1. `NavQueryScheduler` (sabit boyutlu: en çok `kNavMaxBots = 128` bot):
   - `void Request(uint16_t botId, int64_t nowMs)`: bot başına **en çok bir** bekleyen istek (tekrar çağrı zaman damgasını **korur**, yenilemez).
   - `int NextBatch(int64_t nowMs, double budgetMs, uint16_t * out, int cap)`: bekleyen botlardan çalıştırılacakları sırayla seçer: önce bekleme süresi `maxWaitMs`'yi (varsayılan 1000) aşanlar, sonra FIFO (en eski istek önce), eşitlikte **dönen** başlangıç (her çağrıda ofset +1). Bir sorgunun maliyeti, o botun/genel son ölçülen maliyetinden (`ReportCost`, EWMA) kestirilir; toplam kestirim `budgetMs`'yi aşıncaya kadar seçer; **ilerleme garantisi:** bekleyen varsa bütçe ne kadar küçük olursa olsun en az **1** bot seçilir.
   - `void ReportCost(uint16_t botId, double ms, int expanded)`: EWMA (α = 0,3) güncellenir; başlangıç kestirimi `initialCostMs = 0,5`.
   - `int Pending() const`, `int64_t OldestWaitMs(int64_t nowMs) const`, `void Cancel(uint16_t botId)` (bot ölünce/despawn), `void Clear()`.
2. `NavPathCache` (sabit kapasite `kNavCacheCap = 64`, LRU): anahtar `(startCell, goalCell, fieldVersion)`; `Find(key, nowMs, NavPathResult-benzeri hafif görünüm)`, `Put(key, cells, length, nowMs)`, TTL `ttlMs` (varsayılan 30000), `Invalidate(fieldVersion)`; hücre dizisi **kopyalanır** (kapasite başına en çok 512 hücre; daha uzun yol önbelleğe yazılmaz).
3. `inline int NavReplanPhaseMs(int slot, int intervalMs = 500, int phases = 5)` = `(slot mod phases) × (intervalMs / phases)` (negatif `slot` güvenli).
4. Birim testleri ve gerçek harita ölçüm testi (§5.3).

**Kapsam dışı**

- Sunucu entegrasyonu (hangi thread, `BotManager::Tick` kancası, telemetri `NAV_PATH`): F5-55. Hiyerarşik arama/bölge grafiği (`NodeLimit` genel çözümü), arka plan iş parçacığı (ADR-0005 v2), `NavPathfinder` havuz paylaşımı kararı (yalnızca ölçüm notu).
- `docs/` değişikliği (Claude).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavBudget.h` | yeni | |
| `Tests/BotCoreTests/NavBudgetTests.cpp` | yeni | |
| `BotCore/BotCore.vcxproj` | değiştir | tek `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | tek `ClCompile` satırı |

Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-53 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`; `python3 tools/nav-export.py` (gerçek harita testi için).
2. `NavBudget.h`; yeni dosyalar ASCII + CRLF.
3. Testler (adlar sabit):
   - `NavBudget_Scheduler_Basic`: boş → 0 seçim; tek istek bütçeyi aşsa bile 1 seçim (ilerleme garantisi); 16 aynı-tick istek, maliyet 0,4 ms, bütçe 1,5 ms → ilk tick'te `floor(1,5/0,4) = 3`, kalanlar sonraki tick'lerde; hepsi `ceil(16/3) = 6` tick içinde servis edilir; bir bot iki kez `Request` ederse tek kayıt ve **ilk** zaman damgası korunur.
   - `NavBudget_Scheduler_Fairness`: 1000 tick simülasyonu (100 ms), 16 bot her 500 ms'de istek (fazlar `NavReplanPhaseMs` ile), sabit maliyet 0,3 ms, bütçe 1,5 ms: **hiçbir bot `maxWaitMs + 100 ms`'den uzun beklemez**; en çok/az servis edilen bot arası fark ≤ %10; `maxWaitMs`'yi aşan bot öncelik alır (yapay yoğunlukta kanıtla).
   - `NavBudget_Scheduler_Cost`: `ReportCost` EWMA'ya girer; pahalı sorgu (4 ms) sonrası aynı tick'te başka sorgu seçilmez ama bütçeyi aşan **tek** sorgu yine çalışır; `Cancel`/`Clear` istekleri siler.
   - `NavBudget_Cache`: ekleme/bulma, TTL (30000 ms sınırı: 30000'de bulunur, 30001'de yok), LRU (65. giriş en eskiyi atar), `Invalidate`, 512 hücreden uzun yol yazılmaz, kopya bağımsız.
   - `NavBudget_ReplanPhase`: 5 ardışık slot için 0/100/200/300/400 ms, `slot 5 → 0`, negatif slot güvenli; 16 bot için en çok 4 bot aynı fazda.
   - `NavBudget_RealMap_Load` (harita yoksa `SKIPPED`): `NavPathfinder` + `NavFollower` ile 16 bot, her biri 500 ms aralıkla near64 hedef takibi, **60 sn sanal süre** (100 ms tick = 600 tick), iki mod: (A) zamanlayıcısız, fazsız (hepsi `Update`'i her tick çağırır; en kötü durum) ve (B) zamanlayıcılı (bütçe 1,5 ms, fazlı). Her modda tick başına nav süresi (gerçek `steady_clock`), `tick_sum p50/p95/p99/max` ve en uzun yol bekleme süresi satır olarak yazdırılır. **Kabul (Release): (B) `tick_sum p95 ≤ 2.0 ms` ve `p99 ≤ 4.5 ms`, en uzun bekleme ≤ 1100 ms; (B) p95, (A) p95'inden en az %30 düşük.**
4. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni dosyalar için uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; altı yeni test adı `[ OK ]` (gerçek harita testi kabul koşusunda harita **var**); mevcut testler geçer
- [ ] K4: `BotCore/NavBudget.h`'te `windows.h|stdafx|GameServer|shared/` grep'i boş; dinamik bellek yok (`new|malloc` yok; `std::vector` yalnızca önbellek hücre dizilerinde, kapasite sınırlı), global/static durum yok
- [ ] K5: AC-NAV-07 ölçümü Release'te (B) modu için §5.3 eşiklerini karşılar; satır çıktısı raporda; MSVC Release değeri raporlanır (Claude ayrıca WSL `g++` ile çapraz kontrol eder)
- [ ] K6: ilerleme garantisi ve `maxWaitMs` öncelik testleri geçer (başlık: açlık yok)
- [ ] K7: `git diff --stat gece/2026-10-02-nav...bot/F5-53` yalnızca §4; `GameServer/`, `shared/`, `docs/` farkı 0; ASCII + CRLF; `git diff --check` boş
- [ ] K8: Uygulayıcı Raporu `NavPathfinder` bot başına (≈ 4,2 MB) vs paylaşılan örnek için **ölçüm notu** içerir (karar F5-55'te)

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavBudget_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02-nav...bot/F5-53
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. Eşik değerleri (`1,5 ms`, `1 sn`, `30 sn`, `64`) `[Ö]`'dür ve `docs/12` §13.5 ile birlikte güncellenir.
- Bütçe, **bir sorguyu bölmez**: tek sorgu `P-NAV-MAX-NODES` ile zaten sınırlıdır. Bütçe aşımı kuyruğa atılan bot için "mevcut yolu izlemeye devam et" demektir; yolsuz bota "dur" denmez.
- Önbellek anahtarına `fieldVersion` (tehlike/yasaklı bölge alanı sürümü) girmezse eski alana göre hesaplanmış yol sızar; sürüm artışında `Invalidate`.
- Ölçüm gürültülüdür (paylaşımlı makine): her mod 3 kez koşulur, en kötü p95 raporlanır.

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
