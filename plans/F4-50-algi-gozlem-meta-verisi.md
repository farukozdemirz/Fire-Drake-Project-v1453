# F4-50: `Perception` dilim 8 — gözlem meta verisi: birim adı, konum yaşı, hız ve kısa konum geçmişi, kaynak etiketi

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapı G4, §5) |
| Branch | `bot/F4-50 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F4-12 (`ObsTable`), F4-16 (`PerceptionSnapshot`), F4-23 (`tools/check-perception-contract.py`, R5) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/13` §5.2a (gözlem kaynak sınıfları, tazelik, görünürlük, LoS ayrımı); `docs/09` §5.4 `TARGET_LOST_VIS`; `docs/12` §13.2 (hız kestirimi); AC-LRN-03; `docs/reports/degerlendirme-2026-10-02.md` DEG-08/09/10 |
| Tahmini büyüklük | M (4 kod dosyası + 1 araç betiği, yeni dosya yok) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

Bugün `UnitView` yalnızca konum/sınıf/seviye/ölü/oturuyor taşır; `ageMs` "son paketin yaşıdır" (durağan görünür birimde büyür, konum geçerliliğini göstermez), birimin **adı** `UnitObs`'ta bulunduğu halde görünüme geçmez (hedef çağrısı `HEDEF: <ad>` ve `docs/07` §9.2 adı ister), birim başına hız ve konum geçmişi yoktur (F5-04 hedef takibi ve `docs/07` `incoming_*` bunlara ihtiyaç duyar). Bu plan, `docs/13` §5.2a'daki kavramları (kaynak sınıfı, `pos_age`, `moving`, hız kestirimi, tazelik sınıfı) **saf mantık** olarak ekler. Karar/guard/yeni paket yoktur.

## 2. Bağlam (okunması zorunlu)

- `docs/13` §5.2a ve `docs/03` §16: 3×3 bölge bilgisi görüş hattı değildir; durağan birim bayatlamaz; hareketli birimin paket periyodu ~1,5 sn (CLI-05), tazelik eşikleri `[A]`: taze ≤ 3100 ms, bayat 3100–6000 ms, kayıp aday > 6000 ms.
- `BotCore/Perception.h`: `UnitObs` (satır 20), `ParseMove` (217), `ObsTable::UpdatePosition` (298; `NpcTable`'ınki 666'da, dokunulmaz), `UnitView` (832), `NpcView` (849), `BuildSnapshot` (1276-1344: bu planın hedef fonksiyonu); satırlar `gece/2026-10-02` @ `1a42d6a` itibarıyla.
- `GameServer/Bot/BotSession.cpp` `OnPacket()` `WIZ_MOVE` dalı (satır 230), `GameServer/Bot/BotManager.cpp` `CommandSnap` (satır ~2473-2640) ve `CommandSee` (~2235).
- Sunucu paketi: `WIZ_MOVE` = `u16 sid, u16 x10, u16 z10, u16 y10, i16 speed, u8 echo` (`docs/03` §14; `ParseMove` yalnızca ilk dördünü okuyor).

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/Perception.h`:
   - `struct MoveObs { uint16_t sid, x10, z10, y10; int16_t speed; uint8_t echo; }` ve `ParseMoveFull(data, len, MoveObs & out)` (11 bayt gerekir; kısa paket `false`, bounds-safe). Mevcut `ParseMove` **değişmez**.
   - `UnitObs`'a: `uint64_t lastMoveMs` (son `WIZ_MOVE` veya kayıt/respawn paketinin zamanı), `int16_t lastSpeed` (-1 bilinmiyor; bilgi kaydı hız taşımaz), `PosSample hist[4]` + `uint8_t histCount, histNext` (`struct PosSample { uint64_t tMs; uint16_t x10, z10; int16_t speed; }`; halka). `lastSeenMs` korunur (anlamı "son paketin zamanı", yorumda açıkça yaz).
   - `ObsTable::UpdateMove(sid, x10, z10, y10, speed, nowMs)`: konumu günceller, `lastMoveMs`/`lastSpeed`'i yazar, örneği halkaya ekler (aynı konum ve aynı hızla art arda gelen örnek, son örnekten < 400 ms ise eklenmez). `Upsert` kayıt paketinde `hist`'i tek örnekle (`speed = -1`) başlatır, `lastMoveMs = lastSeenMs`.
   - Saf yardımcılar: `EstimateVelocity(const UnitObs &, uint64_t nowMs, float & vx, float & vz)` (son iki örnek, aralık 400–4000 ms; son paketin hızı 0 ise veya en yeni örnek 4000 ms'den eskiyse `(0,0)`; hız büyüklüğü `lastSpeed/10 × 1,1` ile sınırlı), `EstimatePosition(const UnitObs &, uint64_t nowMs, float & x, float & z)` (ölü hesap: en çok 3000 ms ileri), `ClassifyPos(bool moving, uint32_t posAgeMs)` → `POS_FRESH`(≤ 3100 ms veya durağan) / `POS_STALE`(3100–6000) / `POS_LOST`(> 6000, yalnız hareketliyse).
   - `UnitView`'a: `char name[kObsNameMax]`, `uint32_t posAgeMs` (`nowMs − lastMoveMs`), `int16_t speedField` (-1 bilinmiyor), `bool moving` (`speedField > 0`), `float vx, vz` (m/s), `uint8_t posState` (`POS_*`), `uint8_t src` (`kSrcObserved = 0`, sınıf `O`; sabitler `kSrcObserved/kSrcTeam/kSrcEstimate` tanımlı olsun ama yalnızca `O` kullanılır). `ageMs` **kalır** (davranış değişmez; yorum: son paket yaşı, konum geçerliliği değil).
   - `BuildSnapshot` bu alanları doldurur.
2. `GameServer/Bot/BotSession.cpp`: `WIZ_MOVE` dalı `ParseMoveFull` + `UpdateMove` kullanır (kilit/ayrıştırma sırası değişmez: ayrıştırma kilitten önce).
3. **Sözleşme aracı (R5 politikası güncellemesi):** `tools/check-perception-contract.py` (F4-23) `VIEW_FORBIDDEN`'i `UnitView` için `"name"` sözcüğünü **kaldıracak** biçimde günceller: oyuncu adı istemciye `WIZ_USER_INOUT` kaydıyla **gelir** (`docs/03` §16 `[D]`; hedef çağrısı `HEDEF: <ad>` ve `docs/07` §9.2 adı ister); `UnitView` için yasak sözcükler `mp, cooldown, stock, inventory, invent, buff, skill, item, potion` olarak kalır; `NpcView` listesi **değişmez** (NPC adı karar girdisi değildir). Araç başlık açıklaması (satır ~10 `R5`) ve `--selftest` vektörleri (satır ~578-604: `UnitView`'a `int32_t hp;` enjekte eden vaka `int32_t mp;` ile değiştirilir; yeni vaka: `UnitView`'a `char name[24];` eklemek **geçer**, `NpcView`'a eklemek **R5 ihlali**) güncellenir; `python3 tools/check-perception-contract.py` ve `--selftest` PASS olmalı.
4. `GameServer/Bot/BotManager.cpp`: `CommandSnap` `enemy`/`ally` satırlarına ve `CommandSee` satırlarına `name=<ad> pos_age=<ms> speed=<n|?> v=(<vx>,<vz>) pos=<fresh|stale|lost>` ekler. Başka çıktı değişmez.
5. Birim testleri (§6 K3).

**Kapsam dışı**

- NPC için aynı alanlar (`NpcView`): sonraki dilim. Düşman HP (F4-51), skill olayları (F4-52), gözlenen durum (F4-53).
- Karar/guard/telemetri/periyodik kurulum, yeni komut, yeni ini anahtarı, yeni paket isteği: yok.
- Konum tahmininin karar katmanında kullanımı (F5-52/F6).
- `docs/` değişikliği (Claude yapar).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | yalnızca §3.1; yeni `#include` yok (`<cstdint> <cstring> <cmath> <cstddef>` zaten var) |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | 4 yeni `TEST_CASE` |
| `GameServer/Bot/BotSession.cpp` | değiştir | yalnızca `WIZ_MOVE` dalı |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `CommandSnap`/`CommandSee` çıktı satırları |
| `tools/check-perception-contract.py` | değiştir | yalnızca `VIEW_FORBIDDEN` (`UnitView`'dan `name`) + selftest vektörleri + başlık açıklaması |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F4-50 gece/2026-10-02`; planın `Durum` satırını `UYGULANIYOR` yap. Açık sunucu varsa `./tools/run-servers.sh stop`.
2. `Perception.h`: §3.1 sırasıyla. Eski `UpdatePosition` ve `ParseMove` olduğu gibi kalır (mevcut testler dokunulmadan geçmeli).
3. `PerceptionTests.cpp` yeni testleri (adlar sabit): `Perception_ParseMoveFull`, `Perception_Obs_MoveHistory`, `Perception_Obs_PosClassify`, `Perception_Snap_MetaFields`.
   - `ParseMoveFull`: 11 baytlık gerçek düzen (`speed = 45`, `echo = 3`) true; 10 bayt false; `speed = -1` (0xFFFF) okunur; `nullptr`/0 false.
   - `MoveHistory`: aynı birime 1500 ms aralıklı 4 `UpdateMove` (4,5 m/s doğrusal) → `hist` sırası, `EstimateVelocity` ≈ (4,5; 0) ± %10 (1500 ms aralıkta **sıfır olmamalı**; F5-04 sorunu `docs/12` §13.2), son paket `speed = 0` ise `(0,0)`, 4000 ms'den eski en yeni örnek `(0,0)`, 400 ms'den kısa aralık sıfır, `EstimatePosition` ≤ 3000 ms ileri.
   - `PosClassify`: hareketli 3100 → fresh, 3101 → stale, 6000 → stale, 6001 → lost; durağan 60000 → fresh.
   - `Snap_MetaFields`: `BuildSnapshot` sonrası `UnitView.name` doğru, `posAgeMs` ile `ageMs` farklı davranır (bilgi kaydı + 5 sn sonra hiç `MOVE` yok: `ageMs` 5000, durağan değilse `posState` stale; `speed = 0` MOVE sonrası aynı süre → fresh), `src == kSrcObserved`.
4. `BotSession.cpp` ve `BotManager.cpp` değişiklikleri; `snprintf` kullan, `printf` yok.
5. Derleme ve test: §7.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0; yalnızca değişen dosyalar `touch` edilip yeniden derlenince yeni uyarı yok (eski `UpgradeHandler.cpp` C4789 hariç)
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; toplam test sayısı plan başındaki sayıdan **4 fazla**; dört yeni ad `[ OK ]`; mevcut testler değişmeden geçer
- [ ] K4: `BotCore/Perception.h`'te `windows.h|stdafx|GameServer|shared/` grep'i boş; yeni `#include` yok
- [ ] K5a: `python3 tools/check-perception-contract.py` `RESULT: PASS` (R1-R5; `UnitView.name` artık R5 ihlali **değil**, `NpcView`'a `name` eklemek ihlal) ve `python3 tools/check-perception-contract.py --selftest` PASS; `UnitView`'a `mp`/`cooldown`/`stock`/`inventory`/`item`/`potion` sözcüklü alan eklemek hâlâ ihlal
- [ ] K5 (sözleşme, AC-LRN-03): `git diff gece/2026-10-02...bot/F4-50 -- GameServer/Bot | grep '^+' | grep -E 'g_pMain|GetUserPtr|_PARTY_GROUP|m_pUser->'` boş (yeni satırlarda sunucu nesnesi erişimi yok; `CommandSnap`'in mevcut kendi-`CUser` okuması hariç, o satırlara dokunulmaz)
- [ ] K6: `ParseMoveFull` hiçbir girdide sınır dışı okumaz (kısa/null testleri); `ObsTable` kapasite davranışı değişmez (mevcut taşma testi geçer)
- [ ] K7: mevcut `ageMs` anlamı ve `/bot see` mevcut alanları değişmez (yalnız satır sonuna eklenen alanlar); `git diff ... | grep '^-' | grep -v '^---'` yalnızca yorum/imza satırı içerebilir, davranış satırı yok
- [ ] K8: yeni ini anahtarı, komut, paket isteği, thread, mutex yok; `ENABLED=0` davranışı değişmez (kod yalnızca `OnPacket()`/`/bot snap|see` yolunda)
- [ ] K9: ASCII + CRLF korunur; `git diff --check` boş; `printf`(snprintf dışı)/`Sleep`/`CreateThread`/`rand(` eklenmedi
- [ ] K10 (çalışma zamanı, Claude yapar): üç bot, biri `move` ile yürürken `snap`: yürüyen birimin `speed=45`, `pos=fresh`, `pos_age` < 3100 ms, `v` ≈ (hız yönünde 4,5 m/s ±%15); durduğunda `speed=0`, `v=(0,0)`, `pos=fresh` ve `pos_age` büyür ama `pos` fresh kalır; `name=` sunucu `list` adıyla aynı; gerileme: `see`, `npcs`, `snap` çalışır, `Bot_*.log`'da `WARN`/`ERROR` 0

## 7. Doğrulama komutları

```bash
./tools/run-tests.sh Release 2>&1 | tail -3        # plan başında sayıyı not et
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-50
git diff gece/2026-10-02...bot/F4-50 -- BotCore/Perception.h | grep -nE '^\+.*(windows\.h|stdafx|GameServer|shared/)'
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (ASCII, CRLF, tab, Allman, yorumlar İngilizce). Mevcut testleri silme/zayıflatma.
- `hist` halkası küçük ve sabit boyutlu kalır (dinamik bellek yok); `UnitObs` kopyalanabilir kalmalı (`ObsTable` kopyalanır).
- Bu plan `PerceptionSnapshot`'ı **büyütür**: `memset`/kopya maliyeti `/bot snap` komut penceresinde ölçülebilir; periyodik kurulum bu planda yok (F6 öncesi tick maliyeti ayrıca ölçülmeli, STATUS notu).
- Tazelik eşikleri `[A]` (T-PERC-01 insan ölçümüyle doğrulanır); değiştirmek ADR gerektirmez ama `docs/13` §5.2a ile birlikte güncellenir.

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
