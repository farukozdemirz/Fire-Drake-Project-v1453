# F5-51: Arena sınırı ve doğuş yolu: yasaklı bölgeden çıkış maliyeti A*'ı çökertiyor (`NodeLimit`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu:
- Kabul kriterleri öz-değerlendirme:
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
