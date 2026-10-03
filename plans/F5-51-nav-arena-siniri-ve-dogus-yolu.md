# F5-51: Arena sınırı ve doğuş yolu: yasaklı bölgeden çıkış maliyeti A*'ı çökertiyor (`NodeLimit`)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02-nav) |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-51 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F5-02 (A*), F5-06 (`NavDanger.h`, yasaklı/güvenli bölgeler), F5-07 — `KAPANDI` |
| İlgili gereksinim / kabul | ADR-0033-DEG (arena sınırı ve geri çekilme), `docs/12` §7 ve §13.4, AC-NAV-06 (karşı ulus tower halkasına giriş = 0), AC-NAV-02 (A* p95 ≤ 2 ms), T-NAV-05; `docs/reports/degerlendirme-2026-10-02.md` DEG-20 |
| Tahmini büyüklük | S (2 kaynak dosya + 1 yeni test dosyası + proje satırı) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

Test modunda arena sınırı `NavCostLayer::AddForbidOutsideDisc(1274, 890, 60)` ile "arenanın dışı yasaklı" olarak kurulur (`docs/12` §7). Doğan bot arena dışındadır; yasaklı bölgeden **başlayan** yolda her adım `forbiddenPenalty = 10` nedeniyle `1 + 0,5·(10+10) = 11×` pahalıdır, sezgisel (octile) birim maliyetli kaldığı için A* ~11× fazla düğüm genişletir. Ölçüm (değerlendirme raporu §5.3, zone 71, WSL `g++ -O2`):

| Sorgu | Alan yok | Arena sınırı (R=60) |
|---|---|---|
| Karus doğuşu → arena merkezi | 406 düğüm | 4148 düğüm, 0,55 ms |
| El Morad doğuşu → arena merkezi | 2897 düğüm | **`NodeLimit` (20 000 düğüm), 2,7 ms, yol yok** |
| Arena merkezi → Karus doğuşu | Found | `InvalidGoal` (tasarım gereği: arena içindeki bot dışarıya hedefleyemez) |

Sonuç: ölüm → doğuş → arenaya dönüş (T-NAV-05, `docs/09` §9) El Morad için hiç planlanamaz. Bu plan, "dışarıdan **girilemez**" kuralını aynen korurken, yasaklı bölgede **başlayan** bir yolun maliyetinin ve düğüm sayısının çökmesini giderir.

## 2. Bağlam (okunması zorunlu)

- `BotCore/NavDanger.h`: `NavCostParams::forbiddenPenalty` (varsayılan 10, `[A]`), `NavCellPenalty` (satır ~95-119), `AddForbidOutsideDisc` (~299). `docs/12` §4.1 maliyet formülü: `adım = mesafe × (1 + 0,5 × (ceza(a) + ceza(b)))`; "yasaklı hücreye dışarıdan girilemez (içeriden çıkış serbest)".
- `BotCore/NavPath.h` `NavPathfinder::Find` (ağırlıklı dal: `curForbidden`, `zones->Forbidden(nx,nz)` girme kuralı ~satır 195-206; `InvalidGoal` kuralı ~109).
- `BotCore/NavRetreat.h`: `Forbidden` hücre aday olamaz / içeriden çıkış serbest — **davranışı değişmemeli**.
- Mevcut testler: `Tests/BotCoreTests/NavDangerTests.cpp` (yasaklı bölge, halka taraması `500 çiftte yasaklıya giriş 0`), `NavPathTests.cpp`.
- ADR-0033-DEG kararı: arena sınırı yalnızca **arenanın içindeki** bot için "dışarı çıkış yasak"; dışarıdaki bot sınırın içine girebilir, dışarıda serbest yürür; yasaklı-hücre cezası dışarıda uygulanmaz.

## 3. Kapsam

**Yapılacaklar**

1. Kök nedeni gider: yasaklı hücrede **başlayan** bir sorguda yasaklı hücreler için `forbiddenPenalty` uygulanmaz ya da A* maliyetini sezgiselle uyumlu tutan eşdeğer bir yöntem kullanılır (uygulayıcı seçer; seçimi ve gerekçeyi raporla; seçenekler: (a) başlangıç hücresi yasaklıysa yasaklı hücre cezası 0 sayılır; (c) arena sınırı için ayrı bir "sınır dışı" bayrağı (ceza yok), tower halkası için mevcut bayrak). **Ölçülmüş ön bilgi** (değerlendirme, `forbiddenPenalty` değeri değiştirilerek, zone 71): ceza 0 → Karus 602, El Morad 2924 düğüm (K5'i karşılar); ceza 1,0 → Karus 1694, El Morad **16 250** düğüm (K5'i **karşılamaz**); ceza 3 → El Morad `NodeLimit`. Yani yalnızca cezayı küçültmek (b) yetmez; yasaklı bölgeden başlayan sorguda ceza fiilen 0 olmalıdır. **Değişmez kurallar:** (i) yasaklı olmayan hücreden yasaklıya **girilemez** (AC-NAV-06 ve arena içinde kalma); (ii) yasaklıdan yasaklıya ve yasaklıdan çıkışa izin; (iii) hedef yasaklı ve başlangıç değilse `InvalidGoal`; (iv) `NavRetreat` davranışı ve mevcut tüm testler değişmeden geçer (yalnızca yasaklı-bölge **maliyet** değerlerine dokunan, gerekçesi raporda açıklanan test güncellemesi dışında).
2. Yeni test dosyası `Tests/BotCoreTests/NavArenaTests.cpp` (+ `BotCoreTests.vcxproj` `ClCompile` satırı) — §6 K3.
3. Yalnızca gerekirse `BotCore/NavDanger.h` ve/veya `BotCore/NavPath.h` (en küçük değişiklik).

**Kapsam dışı**

- Hiyerarşik/bölgesel arama (`NodeLimit`'i genel olarak çözmek; ADR-0006 "bu dilimde yok"): bu plan yalnızca yasaklı başlangıç kaynaklı çöküşü giderir.
- Arena içi geri çekilme noktası seçimi (F5-07 `NavRetreat` zaten sınırlı sel yapar; arena-dışı "kendi tower halkası" adayı arena modunda geçersizdir: ADR-0033, F6 kararı), yol önbelleği/bütçe (F5-53), sunucu entegrasyonu (F5-55).
- Arena sınırının yarıçapı/merkezi, K-6 kararı, `docs/` değişikliği (Claude).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavDanger.h` | değiştir | yalnızca ceza/bayrak mantığı (§3.1) |
| `BotCore/NavPath.h` | değiştir | yalnızca gerekirse, ağırlıklı dal |
| `Tests/BotCoreTests/NavArenaTests.cpp` | yeni | |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | tek `ClCompile` satırı |
| `Tests/BotCoreTests/NavDangerTests.cpp` | değiştir | yalnızca maliyet değerine bağlı bir test kırılırsa, gerekçeli |

Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-51 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`; `python3 tools/nav-export.py` ile harita (gerçek harita testleri için şart).
2. Önce **tekrar üreten** testleri yaz (kırmızı olsun): `NavArena_RealMap_Respawn` (aşağıda). Sonra düzeltmeyi yap.
3. Testler (adlar sabit):
   - `NavArena_Synthetic`: 128×128 açık ızgara, orta noktada disk `AddForbidOutsideDisc`; dışarıdan başlayan sorgu diske giriş **Found**, düğüm sayısı alan-yok sorgusunun **≤ 2,5 katı**; diskin içinden dışarı hedef `InvalidGoal`; disk içi başlangıç ve hedef `Found` ve yol hiç dışarı çıkmaz; dışarıdan içeri giren yolda içeri girdikten sonra **hiçbir** hücre dışarıda değil.
   - `NavArena_Tower_Unchanged`: düşman tower halkası (`NavBuildTeamZones`) içinden başlayan bot halkadan **en kısa** çıkışla çıkar (yasaklı hücre sayısı geometrik asgari + en çok 2); halkaya dışarıdan giriş yok (500 rastgele çift, mevcut halka taraması aynen). Mevcut `NavDanger_*`/`NavRetreat_*` testleri geçer.
   - `NavArena_RealMap_Respawn` (harita yoksa `SKIPPED`): `AddForbidOutsideDisc(1274, 890, 60)`; başlangıçlar en yakın `Walk` hücresine oturtulmuş Karus doğuşu `(1385, 1095)` ve El Morad doğuşu `(635, 925)`; hedef arena merkezi `(1274, 890)`: ikisi de `Found`, **düğüm ≤ 6000**, `ms` Release'te `≤ 2.0`; yol, arena diskine girdikten sonra bir daha dışarı çıkmaz; arena merkezinden `(1385, 1095)`'e `InvalidGoal`; iki ulus için yol uzunluğu `no field` sorgusunun **≤ 1,15 katı**.
   - `NavArena_Perf`: 200 doğuş→arena sorgusu (rastgele ±15 m başlangıç jitter'ı), `ms_p95` ve `expanded_p95` yazdırılır; kabul `ms_p95 ≤ 2.0` (Release).
4. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, değişen dosyalar `touch` edilince yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; dört yeni test adı `[ OK ]` (gerçek harita testi kabul koşusunda harita **var**, `SKIPPED` değil); mevcut `Nav*` testlerinin tümü geçer (test sayısı yalnızca +4)
- [ ] K4: AC-NAV-06 regresyonu: mevcut "500 çiftte yasaklıya giriş 0" testi değişmeden geçer; yeni testte arena içindeki bot dışarı çıkamıyor (ihlal 0)
- [ ] K5: El Morad doğuşu → arena `Found`, düğüm ≤ 6000 (değerlendirme ölçümü: 20 000'de `NodeLimit`); Karus ≤ 2000 düğüm (ölçüm: 4148); `ms_p95 ≤ 2.0` Release
- [ ] K6: `BotCore/*` yeni satırlarda `windows.h|stdafx|GameServer|shared/` yok; sunucu dosyası farkı 0 (`git diff gece/2026-10-02-nav...bot/F5-51 --stat` yalnızca §4)
- [ ] K7: ASCII + CRLF; `git diff --check` boş
- [ ] K8: Uygulayıcı Raporu seçilen yöntemi (a/b/c), reddedilen yöntemleri ve **ölçülmüş** önce/sonra düğüm sayılarını içerir
- [ ] K9 (Claude): değerlendirme betiğiyle aynı üç sorguyu bağımsız yeniden koşar (WSL `g++`), sayıları rapora karşılaştırır

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavArena_|NavDanger_|NavRetreat_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02-nav...bot/F5-51
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. F5-06'nın `[A]` işaretli ağırlıkları (`wDanger 4`, `wClear 0,5`, `forbiddenPenalty 10`) ADR-0006 Eki F5-06'da kayıtlı: değiştirilen değer varsa ADR eki **Claude**'a rapor edilir (kendi başına ADR yazma).
- "Yasaklıya girilemez" kuralı **gevşetilmez**: arenanın içindeki botun dışarı yol planlamaması K-6'nın parçasıdır.
- Çözüm genel olmalı: yalnızca arena için değil, yasaklı bölgeden başlayan her sorgu için (tower halkası içinde yakalanan bot dahil).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI (Release ve Debug derleme rc=0; `183 tests, 0 failed`; dört yeni `NavArena_*` testi `[ OK ]`, harita mevcut, `SKIPPED` yok).
- Branch / commit'ler: `bot/F5-51` (taban `gece/2026-10-02-nav`). Commit'ler bu raporla birlikte atılır (kod + test + rapor).
- Değişen dosyalar ve neden:
  - `BotCore/NavPath.h` (seçilen yöntem **(a)**): `NavPathfinder::Find` içinde sorgu başlangıcı yasaklıysa (`zones->Forbidden(start)`), o sorgu için `costParams = field->params` kopyasında `forbiddenPenalty = 0` yapılır; ağırlıklı dalın iki `NavCellPenalty` çağrısı `costParams` kullanır. "Yasaklıya dışarıdan girilemez" kapısı (`!curForbidden && zones->Forbidden(nx,nz)`) ve "başlangıç yasaklı değilken yasaklı hedef `InvalidGoal`" kuralı değişmedi; yalnızca yasaklı **başlangıç** sorgusunun ceza değeri değişti.
  - `Tests/BotCoreTests/NavArenaTests.cpp` (yeni): dört test.
  - `Tests/BotCoreTests/BotCoreTests.vcxproj`: tek `ClCompile` satırı.
  - `Tests/BotCoreTests/NavDangerTests.cpp`: yalnızca yasaklı başlangıç maliyetine bağlı beklenen değerler (§3.1 istisnası). `PathCost` ve `RefDijkstra` referansları da aynı kurala hizalandı (`EffectiveParams`).
- Seçilen yöntem ve reddedilenler: **(a)** uygulandı — yasaklı başlangıçta yasaklı cezası 0. **(b)** (yalnız `forbiddenPenalty` küçültme) reddedildi: plan ön bilgisi ve bağımsız ölçüm ceza 1,0'da El Morad'ı 16 250 düğümde bırakıyor. **(c)** (arena sınırı için ayrı bayrak) reddedildi: yöntem genel olmalı (tower halkası içinde yakalanan bot dahil); ayrı bayrak yalnız arena sınırında işe yarar ve `NavCostLayer`'a ikinci bir bayrak biti eklerdi.
- Derleme sonucu: `./tools/build.sh Release` rc=0, `./tools/build.sh Debug` rc=0; değişen dosyalarda yeni uyarı yok (`NavPath.h`, `NavArenaTests.cpp`, `NavDangerTests.cpp` yeniden derlendi). Test: `./tools/run-tests.sh Release` → `183 tests, 0 failed`; `Debug` → `183 tests, 0 failed`.
- Ölçülen önce/sonra (bağımsız WSL `g++ -O2` yeniden üretimi, `HeuristicLab` ile aynı hesap, NodeLimit 20 000, zone 71, R=60, `build/nav/zone71.navgrid`):
  - Karus doğuşu `(346,273)` → arena `(318,222)`: ceza 10 (önce) **4148** düğüm `Found`; ceza 0 (sonra) **602** `Found`; alansız kontrol **602**.
  - El Morad doğuşu `(158,231)` → arena: ceza 10 (önce) **`NodeLimit`** (2 000'de kesildi); ceza 0 (sonra) **2924** `Found`; alansız kontrol **2924**.
  - Birim testi `NavArena_RealMap_Respawn` (Win32 Release, aynı sorgular): Karus `expanded=602` `len=278,108` (`plain_len=271,480`), El Morad `expanded=2924` `len=740,500` (`plain_len=727,814`); her ikisi `Found`, düğüm ≤ 6000/2000, yol diske girdikten sonra dışarı çıkmıyor, uzunluk alansızın ≤ 1,15 katı.
  - `NavArena_Perf`: 200 jitter'lı doğuş→arena sorgusu `found=200`, `ms_p50=0,161` `ms_p95=0,209` `expanded_p95=710` (Release, kapı ≤ 2,0 ms).
- Kabul kriterleri öz-değerlendirme:
  - K1: ✔ Release rc=0, değişen dosyalarda yeni uyarı yok.
  - K2: ✔ Debug rc=0, yeni uyarı yok.
  - K3: ✔ Release ve Debug `183 tests, 0 failed`; dört yeni `NavArena_*` testi `[ OK ]`; harita mevcut (`SKIPPED` yok); mevcut `Nav*` testleri geçer; test sayısı 179 → 183 (+4).
  - K4: ✔ Mevcut "500 çiftte yasaklıya giriş 0" testi değişmeden geçer (`violations=0`); `NavArena_Tower_Unchanged` yeni 500 çiftte de `violations=0`.
  - K5: ✔ Karus 602 ≤ 2000, El Morad 2924 ≤ 6000, `ms_p95=0,209` ≤ 2,0 Release.
  - K6: ✔ `BotCore/NavPath.h` yeni satırlarında `windows.h|stdafx|GameServer|shared/` yok; `GameServer`/`shared`/`AIServer` farkı 0 (yalnız §4 dosyaları).
  - K7: ✔ Tüm dosyalar ASCII + CRLF (dosya kodlamaları/BOM korundu); `git diff --check` boş.
  - K8: ✔ Bu rapor yöntemi (a), reddedilen (b)/(c) ve ölçülmüş önce/sonra düğüm sayılarını içerir.
  - K9 (Claude): bağımsız yeniden koşu bekliyor.
- Plandan sapmalar ve gerekçeleri:
  - `NavArena_RealMap_Respawn`'da "düğüm ≤ 6000 / ≤ 2000 / `ms` ≤ 2,0" kabulü tuttu; plan metnindeki "düğüm ≤ 6000" El Morad için geçerli, Karus için daha sıkı (≤ 2000) kabul edildi (K5).
  - `NavArena_Tower_Unchanged`'de "yasaklı hücre sayısı geometrik asgari + en çok 2": düz kenarlı 8-komşu ızgarada disk yayı düz bir eksen ışınıyla birebir çakışmadığından mesafe düz kenar boyunca döner; bu yüzden asgari, **alansız en kısa çıkış** (`plainForbidden=23` düğüm) ile ölçüldü ve `+2` toleransı korundu. Işın tabanlı `ray_best=16` yalnız tanı için yazdırılır.
  - `NavDangerTests.cpp` beklenen değerleri (yasaklı başlangıç yolu) güncellendi; §3.1'in izin verdiği "yalnızca maliyet değerine bağlı test güncellemesi" kapsamında.
  - `NavDanger_RealMap` start-inside ölçümü: maliyet 1171,853 → **260,137**, uzunluk 292,284 → **257,137**, yasaklı hücre 22 → **19** (yol değişti, maliyet artık geometrik varyant).
- Açık sorular: yok. (ADR-0006 Eki F5-06'daki `[A]` ağırlıkları ve varsayılan `forbiddenPenalty=10` **değiştirilmedi**; değişen yalnız yasaklı başlangıç sorgusunda cezanın sıfırlanmasıdır.)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- **Karar: DOĞRULANDI.** İncelenen commit: `bot/F5-51` @ `7890357` (tek commit, `[F5-51]` biçiminde; taban `gece/2026-10-02-nav`). Gece modu, paralel hat `nav`: sunuculara dokunulmadı, birleştirme/push yapılmadı (birleştirmeyi döngü betiği yapar).
- Kapsam: 4 kod/test dosyası + proje satırı + plan dosyası; hepsi §4 listesinde. `GameServer/`, `AIServer/`, `shared/`, `docs/` farkı 0. `git diff --check` boş; dört dosya ASCII + CRLF (dosya başına `crlf` satır sayısı = satır sayısı); `build/` commit edilmemiş.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release | ✔ | Değişen üç dosya `touch` edildi, `./tools/build.sh Release` rc=0; çıktıda `warning`/`error` yok (0 satır); `BotCoreTests.exe` yeniden bağlandı |
| K2 Debug | ✔ | `touch` sonrası `./tools/build.sh Debug` rc=0; `warning`/`error` 0 satır |
| K3 testler | ✔ | `run-tests.sh Release` ve `Debug`: `183 tests, 0 failed`. `NavArena_Synthetic`, `NavArena_Tower_Unchanged`, `NavArena_RealMap_Respawn`, `NavArena_Perf` dördü de `[ OK ]` (iki yapılandırmada); `build/nav/zone71.navgrid` mevcut, `SKIPPED` yok. Test sayısı tabanda 179 (`TEST_CASE` sayımı), şimdi 183 = +4 |
| K4 AC-NAV-06 | ✔ | Mevcut "500 çiftte yasaklıya giriş 0" taraması `NavDangerTests.cpp`'de değişmedi (diff'te o bölgeye dokunulmadı), geçiyor; `NavArena_Tower_Unchanged` yeni 500 çiftte `found=500 violations=0`; sentetikte disk içi başlangıç→hedef yolu 0 yasaklı hücre, `ReenteredForbidden`/`leftAgain` kontrolleri yeşil |
| K5 sayılar | ✔ | Karus `expanded=602` (≤ 2000), El Morad `expanded=2924` (≤ 6000, `Found`); `NavArena_Perf` Release `ms_p95=0.199`, `found=200/200`, `expanded_p95=710`; El Morad tekil sorgu 0,866 ms (≤ 2,0) |
| K6 | ✔ | `git diff gece/2026-10-02-nav...bot/F5-51 --stat` yalnızca §4 dosyaları + plan; `NavPath.h` yeni satırlarında `windows.h\|stdafx\|GameServer\|shared/` yok |
| K7 | ✔ | ASCII + CRLF (`file` ve `grep -c $'\r$'` ile), `git diff --check` rc=0 |
| K8 | ✔ | Rapor yöntem (a)'yı, reddedilen (b)/(c)'yi gerekçesiyle ve ölçülmüş önce/sonra sayılarını içeriyor; sayılar K9 ile doğrulandı |
| K9 | ✔ | Aşağıdaki bağımsız yeniden koşu |

**K9 — bağımsız yeniden koşu** (WSL `g++ -std=c++17 -O2`, aynı `zone71.navgrid`, `AddForbidOutsideDisc(1274, 890, 60)`, varsayılan `NavCostParams`/`NavSearchParams`; taban ve dal başlıkları ayrı `git archive` kopyalarıyla; betik `/tmp`'de, commit edilmedi):

| Sorgu | Taban (`gece/2026-10-02-nav`) | `bot/F5-51` | Alansız |
|---|---|---|---|
| Karus doğuşu → arena | Found, **4148** düğüm, len 274,794 | Found, **602** düğüm, len 278,108 | 406 düğüm, len 271,480 |
| El Morad doğuşu → arena | **`NodeLimit`**, 20 000 düğüm | Found, **2924** düğüm, len 740,500 | 2897 düğüm, len 727,814 |
| Arena → Karus doğuşu | `InvalidGoal` | `InvalidGoal` (değişmedi, tasarım) | Found |

Uygulayıcı sayıları (602, 2924, 4148, `NodeLimit`) ve plan değerlendirme ölçümüyle birebir uyuşuyor. Yol uzunluğu alansızın 1,024× (Karus) ve 1,017× (El Morad) katı (≤ 1,15).

**Kod incelemesi** (`BotCore/NavPath.h:99-111, 191, 217`): sorgu başlangıcı yasaklıysa yalnızca o sorgunun yerel `costParams` kopyasında `forbiddenPenalty = 0`; `field->params` değişmiyor, `NavCostLayer`'a bayrak eklenmedi. Girme kapısı (`!curForbidden && zones->Forbidden(nx,nz)`) ve `InvalidGoal` kuralı (`zones->Forbidden(goal) && !zones->Forbidden(start)`) satırları değişmedi, yani değişmezler (i)-(iii) korunuyor. `NavCostLayer::Forbidden` ızgara dışında ve `Init` öncesinde `false` döndürdüğü için `InvalidStart` kontrolünden önce çağrılması güvenli (`NavDanger.h:52`). Çözüm genel (yasaklı başlangıçlı her sorgu), yalnız arenaya özel değil. `NavRetreat.h` değişmedi; mevcut `NavRetreat_*` testleri geçiyor. Varsayılan `forbiddenPenalty=10` ve `[A]` ağırlıkları değişmedi: ADR eki gerekmiyor.

**Bulgular** (engel değil, not):

1. `Tests/BotCoreTests/NavArenaTests.cpp:250-259` `NavArena_Tower_Unchanged`: plan "geometrik asgari + en çok 2" diyordu; uygulayıcı alansız en kısa çıkışı (`plainForbidden=23`) ölçüt aldı ve ışın tabanlı `ray_best=16`'yı yalnız yazdırıyor. Sapma raporda gerekçelendirilmiş (8-komşu ızgarada disk yayı eksen ışınıyla çakışmıyor); alansız en kısa çıkış fiilen aynı ölçüt, kabul.
2. `Tests/BotCoreTests/NavDangerTests.cpp` güncellemeleri (`NavDanger_Path_Field` 251,598→60,0 ve 176,0→16,0; `NavDanger_RealMap` start-inside maliyet 1171,853→260,137, uzunluk 292,284→257,137, yasaklı hücre 22→19; `PathCost`/`RefDijkstra` referansları `EffectiveParams` ile hizalandı): hepsi yalnızca yasaklı-başlangıç **maliyet** değerleri, §4'ün "maliyet değerine bağlı test" istisnası içinde ve raporda gerekçeli. Referans Dijkstra'nın aynı kurala hizalanması, A*'ın optimalliğini bağımsız yoldan denemeye devam ediyor (fix'ten bağımsız aynı kural, kabul).
3. `NavArena_RealMap_Respawn` ve `NavArena_Perf` Debug'da zaman kapısı koymuyor (`#ifndef _DEBUG`); plan kapıyı yalnızca Release için istiyor. Uygun.
4. Çalışma zamanı/sunucu kriteri bu hatta yok (kapsam dışı); arena modunun oyun içi doğuş→dönüş akışı F5-55'te doğrulanacak.
