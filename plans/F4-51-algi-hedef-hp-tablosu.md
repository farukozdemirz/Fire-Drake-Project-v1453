# F4-51: `Perception` dilim 9 — düşman/hedef HP gözlem tablosu (`WIZ_TARGET_HP` yanıtlarından)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapı G4) |
| Branch | `bot/F4-51 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F4-06 (`TargetHpReq`, CLI-10), F4-16 (`PerceptionSnapshot`), F4-23 (`tools/check-perception-contract.py`, R5) — `KAPANDI`; F4-50 önerilir (aynı `UnitView` alanlarına dokunur: F4-50 önce birleşmeli, aksi halde `UnitView` çakışması çözülür) |
| İlgili gereksinim / kabul | `docs/03` §16 (hasar verilen/seçili hedefin HP'si gözlemlenebilir), CLI-10; `docs/09` §5.1 (HP gözlemi seyrektir), §6.1 (`dmg_rate`); `docs/13` §5.2a; `docs/reports/degerlendirme-2026-10-02.md` DEG-08 |
| Tahmini büyüklük | S–M (5 kod dosyası + 1 araç betiği, yeni dosya yok) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

Düşmanın kesin HP'si bugün yalnızca `/bot target` eyleminin **sonucu** olarak (tek kayıt, `BotSession::m_targetHpValues`) görülür; gözlem tablosuna ve `PerceptionSnapshot`'a girmez. Karar katmanı (hedef skoru `K(t)`, `docs/09` §5.2; heal-stall `dmg_rate`, §6.1; `docs/07` kritik heal) düşman/hedef HP'sini **zamanıyla birlikte** ister. Bu plan, botun alıcısına gelen **her** `WIZ_TARGET_HP` paketini (kendi isteğinin cevabı ve hasar verdikten sonra gelen bildirim) kimlik başına bir HP gözlemi olarak saklar, yaşını tutar ve `PerceptionSnapshot` görünümlerine bağlar. Gözlem sınıfı `O` (doğrudan); sunucu nesnesinden HP okunmaz.

## 2. Bağlam (okunması zorunlu)

- `GameServer/User.cpp:2350-2387` `SendTargetHP`: paket `u16 tid, u8 echo, i32 maxHp, i32 hp, u16 damage` (13 bayt); `tid >= NPC_BAND` (10000, `GameServer/Define.h:67`) NPC, değilse oyuncu; ölü oyuncu için paket **gelmez**; `m_bPointCheckFlag` kapalıyken NPC için gelmez.
- `GameServer/Bot/BotSession.cpp:79-90` mevcut `WIZ_TARGET_HP` kaydı (eylem eşlemesi için; **dokunulmaz**, yalnızca yanına ekleme yapılır).
- `docs/03` §16 ve CLI-10: bot yalnızca tek seçili hedefin HP'sini yoklayabilir (≥ 2 sn); hasar verince sunucu zaten bildirim yollar. Bu plan **yeni istek yollamaz**.
- `BotCore/Perception.h`: `PerceptionSnapshot`, `UnitView`/`NpcView` (satır 832/849), `BuildSnapshot` (1276); F4-18 `TeamTable` (party üyelerinin HP'si `PARTY_HPCHANGE` ile gelir ve **ayrıdır**).

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/Perception.h`:
   - `struct TargetHpMsg { uint16_t tid; uint8_t echo; int32_t maxHp, hp; uint16_t damage; }` ve `ParseTargetHp(data, len, TargetHpMsg &)`: **13 bayt** gerekir (`damage` dahil); 11–12 bayt kabul edilmez (sunucu hep 13 yollar), `maxHp <= 0` veya `hp < 0` veya `hp > maxHp` ise `false` (bozuk paket), bounds-safe.
   - `struct HpObs { uint16_t id; int32_t hp, maxHp; uint16_t lastDamage; uint64_t atMs; bool reply; /* echo-driven reply to an own request */ }` ve kopyalanabilir `HpTable` (kapasite `kHpMaxEntries = 32`; kimlik başına tek kayıt, yeni gözlem eskisini ezer; doluyken **en eski `atMs`** olan kayıt atılır; `Invalidate(id)`; `Clear()`; `Find(id)`).
   - `constexpr uint32_t kHpStaleMs = 10000;` (`docs/09` §4.2 `EnemyIntel` 10 sn bayatlık).
   - `UnitView` ve `NpcView`'a: `bool hpKnown; int32_t hp, maxHp; uint16_t lastDamage; uint32_t hpAgeMs; bool hpStale;` (`hpStale = hpAgeMs > kHpStaleMs`).
   - `AttachHp(PerceptionSnapshot &, const HpTable &, uint64_t nowMs)`: listelerdeki her `UnitView`/`NpcView` için tabloda aynı kimlik (oyuncu kimlikleri `< 10000`, NPC kimlikleri `>= 10000`; çakışma olmaz) varsa alanları doldurur; yoksa `hpKnown = false`. `BuildSnapshot` imzası **değişmez**.
2. `GameServer/Bot/BotSession.{h,cpp}`: `HpTable m_hp` (aynı `m_obsLock` altında, `m_obs`/`m_npcs`/`m_team` gibi); `OnPacket()` içinde `WIZ_TARGET_HP` için **mevcut kaydın yanına** ayrı blok: ayrıştırma kilitten önce, `Upsert` kilit altında; geçerli `echo` ≠ 0 ise `reply = true` (seçim `echo=1`), `0` ise hasar bildirimi/yoklama. `WIZ_DEAD` ve `WIZ_USER_INOUT OUT` / `WIZ_NPC_INOUT OUT` ile ilgili kimlik `Invalidate` edilir (yeni gözlem öncesi eski HP'nin yaşlı doğru gibi görünmemesi için). `ResetForRespawn` tabloyu temizler.
3. `GameServer/Bot/BotManager.cpp` `CommandSnap`: `AttachHp` çağrısı (tabloyu `m_obsLock` altında kopyala) ve `enemy`/`ally`/`npc` satırlarına `hp=<hp>/<max> hp_age=<ms>[ stale]` veya `hp=?` ekle.
4. **Sözleşme aracı (R5 politikası güncellemesi):** `tools/check-perception-contract.py` (F4-23) `VIEW_FORBIDDEN`'de `UnitView` ve `NpcView` için `"hp"` sözcüğünü **kaldırır**: düşman/hedef HP'si `docs/03` §16'da `[D]` serbesttir (yalnızca hasar verilen veya seçili hedef için `WIZ_TARGET_HP` paketinden); `mp, cooldown, stock, inventory, invent, buff, skill, item, potion` ve `NpcView` için `name` yasak olarak **kalır**. Gerekçe ve kaynak güvencesi: HP alanlarının yalnızca alınan pakete bağlı olması R1-R3 (sunucu nesnesine erişim yok) ve K5 ile sağlanır; R5 yalnızca `mp`/`cooldown`/envanter benzeri **gerçekten gönderilmeyen** bilgileri engellemeye devam eder. `--selftest` vektörleri (`UnitView`'a `int32_t hp;` enjekte eden vaka) `int32_t mp;` ile değiştirilir, `hp` alanı eklemenin **geçtiği** bir vaka eklenir; araç başlık açıklaması güncellenir; `python3 tools/check-perception-contract.py` ve `--selftest` PASS olmalı.
5. Birim testleri (§6 K3).

**Kapsam dışı**

- Yeni `WIZ_TARGET_HP` isteği, otomatik yoklama, yoklama zamanlayıcısı (CLI-10 aynen; karar katmanı F6'da yoklar). Takım içi HP paylaşımı (`P` sınıfı, F7).
- Hasar hızı (`dmg_rate`) hesabı, HP geçmişi, tahmin (karar katmanı/F6–F7).
- Party üyesi HP'sinin bu tabloya yazılması (`TeamTable` ayrı kaynak).
- `docs/` değişikliği.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | yalnızca §3.1 |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | 4 yeni `TEST_CASE` |
| `GameServer/Bot/BotSession.h` | değiştir | `HpTable m_hp` üyesi |
| `GameServer/Bot/BotSession.cpp` | değiştir | `OnPacket` ek bloğu, `ResetForRespawn` temizliği |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `CommandSnap` |
| `tools/check-perception-contract.py` | değiştir | yalnızca `VIEW_FORBIDDEN` (`UnitView`/`NpcView`'dan `hp`) + selftest vektörleri + başlık açıklaması |

5 kod dosyası + 1 araç betiği. Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F4-51 gece/2026-10-02` (F4-50 birleşmişse onun ucundan); `Durum` → `UYGULANIYOR`; açık sunucuları durdur.
2. `Perception.h` §3.1; testler: `Perception_ParseTargetHp` (13 bayt gerçek düzen `{0x34,0x12 tid, echo 1, maxhp 5650 LE, hp 5266 LE, damage 100}` true; 12 bayt false; `hp > maxHp` false; `maxHp = 0` false; null/0 false), `Perception_HpTable_Upsert` (ezme, 32 kapasite + en eski atılır, `Invalidate`, kopya bağımsızlığı), `Perception_HpTable_Attach` (oyuncu/NPC ayrımı `< 10000` / `>= 10000`, `hpKnown`, `hpAgeMs`, `hpStale` sınırı 10000/10001), `Perception_HpTable_Death` (ölüm/OUT sonrası `Invalidate` ile `hpKnown = false`; `BuildSnapshot` imzası ve `ageMs` değişmedi).
3. `BotSession` ve `BotManager` değişiklikleri.
4. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, değişen dosyalar `touch` edilince yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`, toplam test sayısı plan başındakinden **4 fazla**, dört yeni ad `[ OK ]`
- [ ] K4: `BotCore/Perception.h`'te `windows.h|stdafx|GameServer|shared/` grep'i boş
- [ ] K5a: `python3 tools/check-perception-contract.py` `RESULT: PASS` ve `--selftest` PASS; `UnitView`/`NpcView`'a `mp`/`cooldown`/`stock`/`inventory`/`item`/`potion` sözcüklü alan eklemek hâlâ R5 ihlali, `hp` alanı değil
- [ ] K5 (sözleşme): yeni satırlarda `g_pMain|GetUserPtr|_PARTY_GROUP|m_pUser->` yok; HP yalnızca alınan `WIZ_TARGET_HP` paketinden gelir (`UnitView`/`NpcView` HP'si kendi `CUser`/sunucu nesnesinden **okunmaz**)
- [ ] K6: mevcut `m_targetHpValues`/`m_targetHpEcho` yolu ve `/bot target` davranışı değişmez (`git diff` yalnızca ek satır içerir; `-` satırı yok ya da yalnızca yorum)
- [ ] K7: yeni paket isteği/zamanlayıcı/ini anahtarı/komut yok; `ENABLED=0` davranışı değişmez
- [ ] K8: `m_hp` yalnızca `m_obsLock` altında erişilir (`grep -n m_hp GameServer/Bot/*.cpp` her erişim kilitli blokta); ayrıştırma kilitten önce
- [ ] K9: ASCII + CRLF; `git diff --check` boş; `printf`(snprintf dışı)/`Sleep`/`CreateThread`/`rand(` yok
- [ ] K10 (çalışma zamanı, Claude yapar): iki bot (`BotWP_K` ↔ `BotWP_E`), `target` + `attack` sonrası `snap BotWP_K`: düşman satırında `hp=<x>/<max>` `list` `hp=` ile aynı (hasar sonrası ± en son bildirim), `hp_age` küçük; hedef ölünce satır `dead` ve `hp=?`; hiç hedeflenmemiş üçüncü botta `hp=?`; gerileme `see`/`npcs`/`snap`/`target`/`attack` çalışır, `Bot_*.log`'da `WARN`/`ERROR` 0

## 7. Doğrulama komutları

```bash
./tools/run-tests.sh Release 2>&1 | tail -3     # plan başında sayıyı not et
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release && ./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-51
grep -n "m_hp" GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (ASCII, CRLF, tab, Allman, İngilizce yorum). Mevcut testler korunur.
- Paket 13 bayttan kısa gelirse ve `hp`'si okunabilse bile **kabul edilmez**: sunucu düzeni sabit; kısmi paket gözlem sayılmaz (yanlış HP, yanlış karardan kötüdür).
- `echo` alanı yalnızca `reply` bayrağı içindir; HP gözlemi `echo`'dan bağımsız aynı tabloya yazılır.
- Tablo kapasitesi küçük ve sabit (32); NPC kalabalığında en eski atılır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-51` (taban: `gece/2026-10-02` @ `d3605e2`, F4-50 birleşmiş); commit'ler bu raporla birlikte.
- Değişen dosyalar ve neden:
  - `BotCore/Perception.h`: `kHpMaxEntries`, `kHpStaleMs`, `TargetHpMsg`, `ParseTargetHp` (tam 13 bayt + `maxHp<=0`/`hp<0`/`hp>maxHp` ret), `HpObs`, kopyalanabilir `HpTable` (ezme, dolunca en eski `atMs` atılır, `Invalidate`, `Find`), `UnitView`/`NpcView`'a HP alanları, saf `AttachHp` (`BuildSnapshot` imzası değişmedi).
  - `Tests/BotCoreTests/PerceptionTests.cpp`: 4 yeni `TEST_CASE` (`ParseTargetHp`, `HpTable_Upsert`, `HpTable_Attach`, `HpTable_Death`).
  - `GameServer/Bot/BotSession.h`: `HpTable m_hp` (m_obsLock).
  - `GameServer/Bot/BotSession.cpp`: mevcut `WIZ_TARGET_HP` kaydının yanına ayrı algı bloğu (kilitten önce ayrıştırma, kilit altında `Upsert`), `WIZ_USER_INOUT OUT`/`WIZ_NPC_INOUT OUT`/`WIZ_DEAD` (oyuncu ve NPC) `Invalidate`, `ResetForRespawn` temizliği.
  - `GameServer/Bot/BotManager.cpp`: yalnızca `CommandSnap` — `hpCopy` (kilit altında), `AttachHp` çağrısı, `enemy`/`ally`/`npc` satırlarına `hp=<hp>/<max> hp_age=<ms>[ stale]` veya `hp=?`.
  - `tools/check-perception-contract.py`: `UnitView`/`NpcView` `VIEW_FORBIDDEN`'dan `hp` çıkarıldı, `mp`/`name`/`cooldown`/envanter yasak kaldı; başlık R5 açıklaması güncellendi; selftest'e `hp` alanının `UnitView` ve `NpcView`'da geçtiği iki vaka eklendi.
- Derleme sonucu: `./tools/build.sh Release` rc=0 (yeni uyarı yok), `./tools/build.sh Debug` rc=0; `./tools/run-tests.sh Release` ve `Debug` → `92 tests, 0 failed` (plan başı 88, +4). `python3 tools/check-perception-contract.py` → `RESULT: PASS` (R1 0, R2 0/28, R3 0/18, R4 0, R5 0), `--selftest` → `selftest OK`.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ Release rc=0, touch sonrası değişen dosyalarda uyarı yok.
  - K2 ✔ Debug rc=0, uyarı yok.
  - K3 ✔ Release+Debug `92 tests, 0 failed`, dört yeni ad `[ OK ]`.
  - K4 ✔ `BotCore/Perception.h`'te yasak include/kelime grep'i boş.
  - K5a ✔ araç `RESULT: PASS` + `--selftest OK`; `mp` eklemek hâlâ R5 ihlali (selftest), `hp` alanı `UnitView`/`NpcView`'da geçiyor.
  - K5 ✔ yeni satırlarda `g_pMain`/`GetUserPtr`/`_PARTY_GROUP`/`m_pUser->` yok; HP yalnızca `WIZ_TARGET_HP` paketinden (`AttachHp` saf tabloyu kullanır).
  - K6 ✔ mevcut `m_targetHpValues`/`m_targetHpEcho` kaydı ve `/bot target` yolu değişmedi; `BotSession.cpp` farkında `-` satırı yok.
  - K7 ✔ yeni paket isteği/zamanlayıcı/ini anahtarı/komut yok; `ENABLED=0` yolu değişmedi (yalnızca alınan paket işlenir).
  - K8 ✔ `m_hp` yalnızca `m_obsLock` altında (6 erişim: 5 `OnPacket`/`ResetForRespawn` kilitli blok, 1 `CommandSnap` kilitli kopya); ayrıştırma kilitten önce.
  - K9 ✔ ASCII+CRLF (araç ASCII+LF, mevcut kural), `git diff --check` boş; yeni `printf`/`Sleep`/`CreateThread`/`rand(` yok (bulunan iki `printf` eski satırlar).
  - K10 → Claude yapar (çalışma zamanı).
- Plandan sapmalar ve gerekçeleri: yok.
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
