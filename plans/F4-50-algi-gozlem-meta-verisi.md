# F4-50: `Perception` dilim 8 — gözlem meta verisi: birim adı, konum yaşı, hız ve kısa konum geçmişi, kaynak etiketi

| Alan | Değer |
|---|---|
| Durum | DÜZELTME GEREKLİ |
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

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-50` (taban `gece/2026-10-02`); `953cb46` `[F4-50] Algı gözlem meta verisi: konum geçmişi, hız, tazelik`; `f645f78` `[F4-50] Sözleşme aracı R5: UnitView.name alanına izin ver`. Uygulama öncesi taban: `0dfeca1` (F4-24 birleşik).
- Değişen dosyalar ve neden:
  - `BotCore/Perception.h`: `PosSample`/`MoveObs`, `PosState`, kaynak sabitleri, `UnitObs`'a `lastMoveMs`/`lastSpeed`/`hist`/`histCount`/`histNext`, `ObsSampleBack`/`ClassifyPos`/`EstimateVelocity`/`EstimatePosition`, `ParseMoveFull`, `ObsTable::UpdateMove`, `Upsert` kayıt örneği tohumlaması, `UnitView` yeni alanları (`name`, `posAgeMs`, `speedField`, `moving`, `vx`, `vz`, `posState`, `src`), `BuildSnapshot` dolumu.
  - `GameServer/Bot/BotSession.cpp`: `WIZ_MOVE` dalı `ParseMoveFull` + `UpdateMove` (kilit sırası ve ayrıştırmanın kilitten önce olması korunur).
  - `GameServer/Bot/BotManager.cpp`: `CommandSee` ve `CommandSnap` enemy/ally satırlarına `name=… pos_age=… speed=… v=(…) pos=…` alanları eklendi; mevcut alanlar/sıra değişmedi.
  - `Tests/BotCoreTests/PerceptionTests.cpp`: dört yeni `TEST_CASE` (`Perception_ParseMoveFull`, `Perception_Obs_MoveHistory`, `Perception_Obs_PosClassify`, `Perception_Snap_MetaFields`).
  - `tools/check-perception-contract.py`: `VIEW_FORBIDDEN["UnitView"]`'dan `"name"` çıkarıldı (`hp` F4-51'e kadar yasak, `NpcView` değişmedi), R5 başlık açıklaması ve `--selftest` V6 vektörleri güncellendi (hp→mp; `UnitView`'a `name` geçerli, `NpcView`'a `name` ihlal).
- Derleme sonucu: `./tools/build.sh Release` rc=0; son satırlar `proj-LogInServer…`, `proj-GameServer…`, `proj-AIServer…`, `BotCoreTests.vcxproj -> …\x86-Release\Tests\BotCoreTests.exe`. `./tools/build.sh Debug` rc=0. Değişen dört dosya `touch` edilip yeniden derlendiğinde `warning`/`error` satırı yok. Testler: Release ve Debug `88 tests, 0 failed` (taban 84; dört yeni ad `[ OK ]`). `python3 tools/check-perception-contract.py --selftest` → `selftest OK`; `python3 tools/check-perception-contract.py` → `RESULT: PASS` (R1-R5 ihlal 0, allowlist R2 28 / R3 18, `files 19`).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (Release rc=0; değişen dosyalarda yeni uyarı yok), K2 ✔ (Debug rc=0), K3 ✔ (88 = 84 + 4; dört yeni ad `[ OK ]`, mevcutlar geçti), K4 ✔ (yasak include grep'i boş; yeni include yok).
  - K5a ✔ (`RESULT: PASS` + `selftest OK`; `UnitView.name` ihlal değil, `NpcView.name` ve `UnitView` `mp`/`cooldown`/`stock`/`inventory`/`item`/`potion` ihlal). K5 ✔ (yeni `GameServer/Bot` satırlarında `g_pMain|GetUserPtr|_PARTY_GROUP|m_pUser->` yok).
  - K6 ✔ (`ParseMoveFull` kısa 10 bayt/null false; `ObsTable` kapasite/taşma testi değişmedi), K8 ✔ (yeni ini/komut/paket/thread/mutex yok; kod yalnızca `OnPacket()`/`/bot see|snap` yolu), K9 ✔ (ASCII + CRLF, `git diff --check` boş, eklenen satırlarda `printf(`/`Sleep`/`CreateThread`/`rand(` yok).
  - K7 △: `ageMs` anlamı ve `/bot see` mevcut alanları korundu (yalnız satır sonuna alan eklendi), ancak `git diff | grep '^-'` boş değil: §3.2'nin zorunlu kıldığı `WIZ_MOVE` dalı değişimi (`ParseMove`/`UpdatePosition` → `ParseMoveFull`/`UpdateMove`) ve format satırları kaçınılmaz `-` satırları üretir; ilgisiz davranış satırı kaldırılmadı.
  - K10 → çalışma zamanı doğrulaması planda Claude'a bırakılmıştır; bu turda sunucu çalıştırılmadı.
- Plandan sapmalar ve gerekçeleri:
  - §3.1 `moving` alanı `speedField > 0` diyor; ancak §5.3 `Snap_MetaFields` "bilgi kaydı + 5 sn, hiç `MOVE` yok → `ageMs` 5000, durağan değilse `posState` stale" bekler. Kayıt paketinde hız `-1` (bilinmiyor) olduğundan testin geçmesi için `moving = (speedField != 0)` uygulandı: bilinmeyen hız muhafazakâr biçimde hareketli sayılır (docs/13 §5.2a'daki durağan/hareketli ayrımına uygun; hız `0` kesin durağan). Bu seçim raporda açıkça belirtilir.
  - K7'nin `^-` grep'i, §3.2/§3.4'ün zorunlu satır değişiklikleri nedeniyle lafzen boş olamaz (yukarıda).
- Açık sorular: Yok. (Not: `moving` yorumu plan metnindeki `speedField > 0` ifadesinden kasıtlı olarak sapar; doğrulamada onay gerekirse §3.1/`docs/13` §5.2a güncellenebilir.)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- **Karar: DÜZELTME GEREKLİ**
- İncelenen commit: `b9674b4` (`bot/F4-50`, taban `gece/2026-10-02` @ `0dfeca1`). Gece modu (`AUTO_LOOP=1`): birleştirme/push yapılmadı. Çalışma ağacı temiz.
- Kapsam: 6 dosya, hepsi §4 listesinde (`Perception.h`, `PerceptionTests.cpp`, `BotSession.cpp`, `BotManager.cpp`, `check-perception-contract.py`, kendi planı). `docs/`, `AGENTS.md`, başka plan değişmedi. Commit mesajları `[F4-50] …`.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release | ✔ | Dört değişen dosya `touch` + `./tools/build.sh Release` rc=0; uyarı yalnızca eski `UpgradeHandler.cpp(634/862)` C4789 |
| K2 Debug | ✔ | `./tools/build.sh Debug` rc=0; `UpgradeHandler` dışında uyarı/hata 0 |
| K3 testler | ✔ | `run-tests.sh Release` ve `Debug`: `88 tests, 0 failed` (taban 84 + 4); `Perception_ParseMoveFull`, `_Obs_MoveHistory`, `_Obs_PosClassify`, `_Snap_MetaFields` `[ OK ]`; mevcut testlere dokunulmadı (diff'te yalnızca `+` satırı) |
| K4 | ✔ | `Perception.h` eklenen satırlarda `windows.h|stdafx|GameServer|shared/|#include` yok |
| K5a | ✔ | `RESULT: PASS`, `--selftest` `selftest OK`; araç diff'inde `UnitView` demetinden yalnızca `"name"` çıktı, `mp/cooldown/stock/inventory/invent/buff/skill/item/potion` ve `NpcView` aynı; selftest `NpcView.name` ihlal, `UnitView.name` geçer vakalarını içeriyor. (Sahte kopyada `mp`/`item` enjeksiyonu denemesi araç izni verilmediği için yapılmadı; kod okumasıyla ve selftest `mp` vakasıyla doğrulandı.) |
| K5 AC-LRN-03 | ✔ | `GameServer/Bot` eklenen satırlarında `g_pMain|GetUserPtr|_PARTY_GROUP|m_pUser->` boş |
| K6 | ✔ | `Perception.h:365` `ParseMoveFull`: `nullptr`/`len < 11` false, sonra `ByteReader`; testte 10 bayt ve `nullptr` false, `0xFFFF` → -1; `ObsTable` kapasite yolu (`Upsert`) değişmedi |
| K7 | ✔ (not) | Eksi satırları: yalnızca `Upsert` imzası, `lastSeenMs`/`UnitView` yorumları, 3 format dizgisi + `WIZ_MOVE` dalı. Davranış kaybı yok; `ageMs` hesabı aynı; `/bot see`/`snap` yeni alanlar satır sonunda |
| K8 | ✔ | Yeni ini/komut/paket/thread/mutex yok; `BotSession.cpp` `WIZ_MOVE` dalında ayrıştırma kilitten önce, `m_obsLock` aynı |
| K9 | ✔ | Blob'lar LF (autocrlf), çalışma ağacı dosyaları tamamen CRLF (`1609/1609`, `418/418`, `3382/3382`, `1977/1977`); `.py` LF; eklenen satırlarda ASCII dışı yok; `git diff --check` rc=0; `printf`(snprintf dışı)/`Sleep`/`CreateThread`/`rand(` yok |
| K10 çalışma zamanı | ertelendi | Bu tur çalıştırılmadı: kod düzeltmesi `moving` anlamını değiştireceği için sunucu sınaması düzeltme turundan sonra yapılır (sunucular `[DOWN]`) |

**Bulgular (önem sırasıyla)**

1. **[Doğruluk, plandan sapma] `moving` bilinmeyen hızı hareketli sayıyor.** `BotCore/Perception.h:1511` (`v.moving = (u.lastSpeed != 0)`), `BotCore/Perception.h:1041` (alan yorumu), `BotCore/Perception.h:82-84` (`ClassifyPos` yorumu), `GameServer/Bot/BotManager.cpp:2354` (`bool moving = (u.lastSpeed != 0)`). Plan §3.1 `moving = speedField > 0` der; `docs/13` §5.2a "durağan birim bayatlamaz, yalnızca OUT/bölge değişimiyle düşer" der. Uygulayıcı bu sapmayı §5.3 test cümlesini geçirmek için yaptı ve raporda açıkladı (dürüstlük ✔). Sonuç: `WIZ_USER_INOUT` ile kaydı gelen ve hiç `WIZ_MOVE` göndermeyen (ayakta duran) bir oyuncu 3,1 sn sonra `POS_STALE`, 6 sn sonra `POS_LOST` olur. Sunucu hareket eden her birim için ~1,5 sn'de bir `WIZ_MOVE` yolladığından kayıttan sonra MOVE gelmemesi birimin **durağan** olduğunu gösterir; "kayıp aday" yalnızca "son paket `speed > 0` ve sonra sessizlik" için anlamlıdır. Düzeltilmezse `docs/09` §5.4 `TARGET_LOST_VIS` ("kayıp aday ≥ 3 sn") F5/F6'da ayakta duran bir hedefi yanlış düşürür. Plan §5.3'ün `Snap_MetaFields` cümlesi belirsizdi (Claude'un plan hatası: "durağan değilse" koşulu); doğru okuma bilinmeyen hız = durağan.
2. **[Not, engel değil] `CommandSee` `moving/posState/v` hesabını `BuildSnapshot` ile ikilemiş** (`BotManager.cpp:2352-2359`). `CommandSee` snapshot kullanmadığı için kabul edilebilir; yalnız bulgu 1 düzeltilirken iki yerin aynı kalmasına dikkat.
3. **[Not, plan hatası] `/bot see` satırında `pos=(x, z)` ile `pos=<fresh|stale|lost>` anahtarı aynı satırda iki kez geçer** (`BotManager.cpp:2365`). Plan §3.4 `pos=<fresh|stale|lost>` biçimini açıkça istedi ve K10 buna göre; değiştirilmez. Betik/ayrıştırıcı yazılırken ilk `pos=` konumdur, son `pos=` tazeliktir; `docs/16` ve STATUS'ta not edilecek (Claude).
4. **[Not] K7 lafzı** (`^-` yalnızca yorum/imza) plan metninin zorunlu `WIZ_MOVE` ve format satırı değişiklikleri nedeniyle lafzen sağlanamaz; uygulayıcının açıklaması doğru, K7 ✔ sayıldı.

**Düzeltme talimatı**

```
plans/F4-50-algi-gozlem-meta-verisi.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
1. BotCore/Perception.h, BuildSnapshot (satır 1511): `v.moving = (u.lastSpeed != 0);` satırını `v.moving = (u.lastSpeed > 0);` yap (plan §3.1: speedField > 0). Bilinmeyen hız (-1) ve durağan (0) ikisi de "hareketli değil" sayılır.
2. BotCore/Perception.h: UnitView.moving alan yorumunu (satır 1041) şu anlama getir: "speedField > 0; unknown speed (-1, no WIZ_MOVE since registration) counts as stationary: the server sends a WIZ_MOVE for every step of a moving unit". ClassifyPos yorumundan (satır 82-84) "an unknown speed is treated as moving (conservative, docs/13 section 5.2a)" ifadesini kaldır; yerine "moving = last WIZ_MOVE speed > 0" yaz. ClassifyPos'un gövdesine dokunma.
3. GameServer/Bot/BotManager.cpp, CommandSee (satır 2354): `bool moving = (u.lastSpeed != 0);` satırını `bool moving = (u.lastSpeed > 0);` yap. Başka satıra dokunma.
4. Tests/BotCoreTests/PerceptionTests.cpp, Perception_Snap_MetaFields: ilk blokta (kayıt t=0, MOVE yok, BuildSnapshot nowMs=5000) beklentileri şöyle değiştir: speedField -1, `CHECK(!v.moving)`, posState POS_FRESH (ageMs ve posAgeMs 5000 aynen kalır), vx/vz 0. Yorumu "unknown speed counts as stationary: no MOVE since registration" yap. Mevcut speed=0 bloğu aynen kalır. Üçüncü blok ekle: aynı kayıt, ardından UpdateMove(5, 10030, 10040, 0, 45, 1000); BuildSnapshot nowMs=5000 (posAge 4000) → moving true, POS_STALE; nowMs=7500 (posAge 6500) → POS_LOST. Yeni TEST_CASE ekleme (toplam 88 kalır).
5. Derle ve sına: `./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` ve `Debug` (88 tests, 0 failed), `python3 tools/check-perception-contract.py` (RESULT: PASS) ve `--selftest`. Değişen dosyalar yalnızca bu üçü olmalı (Perception.h, BotManager.cpp, PerceptionTests.cpp); `git diff --check` boş.
6. Uygulayıcı Raporu'na "Tur 2" ekle: "Plandan sapmalar" bölümünde `moving` sapmasının kaldırıldığını yaz; Durum satırını UYGULANDI yap.
```
