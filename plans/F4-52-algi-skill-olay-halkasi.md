# F4-52: `Perception` dilim 10 — görülen skill olayları (`WIZ_MAGIC_PROCESS` bölge yayını) için olay halkası

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapı G4; F7 priest/debuff için ön koşul) |
| Branch | `bot/F4-52 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F4-03 (cast), F4-12 (gözlem tabloları) — `KAPANDI`; F4-50/F4-51 ile dosya çakışması yok (yalnızca `Perception.h` sonuna ekleme) |
| İlgili gereksinim / kabul | `docs/03` §16 (skill cast ve etki olayları gözlemlenebilir), `docs/07` §9.2 (debuff başarısı gözlemi), `docs/09` §6.1 (`heal_rate`: hedefe yönelik gözlenen heal olayları), `docs/13` §5.2a; `docs/reports/degerlendirme-2026-10-02.md` DEG-08 |
| Tahmini büyüklük | S–M (5 kod dosyası, yeni dosya yok) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç

Bot bugün `WIZ_MAGIC_PROCESS` paketini yalnızca **kendi** cast'inin yankısı için okur (`BotSession.cpp:60-67`, `m_castEcho`). Başkalarının cast/etki olayları (kim, kime, hangi skill, hangi aşama) algıya girmez: oysa düşmanın heal'i/buff'ı/debuff'ı (`docs/09` §6.1 stall tespiti, `docs/07` §9.2 debuff başarısı, kritik heal fırsatı) yalnızca bu olaylardan izlenebilir ve istemci de aynı paketi alır (`docs/03` §16, `[D]`). Bu plan, bota gelen her `WIZ_MAGIC_PROCESS` paketini sabit boyutlu bir **olay halkasına** yazar ve sorgu yardımcıları verir. Skill sınıflandırması (heal mi, debuff mı) ve gözlenen durum tablosu bu planda **yoktur** (F4-53). Karar/guard/yeni paket yok.

## 2. Bağlam (okunması zorunlu)

- `GameServer/MagicInstance.cpp:735-772` (`BuildSkillPacket`): düzen `u8 opcode, u32 skillId, i16 caster, i16 target, i16 data[7]` = 23 bayt (`docs/03` §14). Bölgeye yalnız `CASTING`, `FLYING`, `EFFECTING` yayınlanır; `MAGIC_FAIL` yalnız çağırana gider (`:41-42`, `SendSkillFailed`): diğer botlar fail görmez (beklenen davranış).
- Opcode sabitleri `shared/packets.h:382-393` (`MAGIC_CASTING=1, FLYING=2, EFFECTING=3, FAIL=4, CANCEL=6, CANCEL_TRANSFORMATION=7, TYPE4_EXTEND=8, FAIL_TRANSFORMATION=10, CANCEL2=13`; ISO-8859 olabilir: `grep -a`). `BotCore` bu başlığı **include etmez**; sabitleri kendi `constexpr`'leriyle yinelemek ve yorumda kaynağı yazmak gerekir.
- NPC cast'leri (`caster >= 10000`) da bölgeye yayınlanır; halka ayrım yapmadan saklar.
- `GameServer/Bot/BotSession.cpp` mevcut `WIZ_MAGIC_PROCESS` dalı (satır 58-68): **dokunulmaz**, yeni ekleme ayrı blok.

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/Perception.h` sonuna:
   - `struct SkillEvent { uint64_t tMs; uint8_t op; uint32_t skillId; int16_t caster, target; int16_t data[7]; }`.
   - `ParseSkillEvent(const uint8_t * data, size_t len, uint64_t nowMs, SkillEvent & out)`: tam **23 bayt** gerekir (daha kısa `false`, fazlası kabul: ilk 23 bayt okunur), `op` 1..13 dışındaysa `false`; bounds-safe; `ByteReader` kullan.
   - Kopyalanabilir `SkillEventRing` (`kSkillEventRing = 64`): `Add(ev)`, `Count()` (≤ 64), `Total()` (tüm zamanlar, taşan dahil), `At(i)` (0 = en yeni), `FindLatest(op, caster, target, nowMs, windowMs)` (parametre `-1`/`0xFF` = joker), `CountIn(op, target, nowMs, windowMs)`, `Clear()`.
2. `GameServer/Bot/BotSession.{h,cpp}`: `SkillEventRing m_skillEvents` (`m_obsLock` altında); `OnPacket()`'e **ayrı yeni blok**: `opcode == WIZ_MAGIC_PROCESS && pkt.size() >= 23` → ayrıştır (kilit dışında), kilit altında `Add`. Mevcut kendi-cast yankısı bloğu aynen kalır. `ResetForRespawn` halkayı temizler.
3. `GameServer/Bot/BotManager.cpp` `CommandSnap`: `snap <bot> events` biçimi (ikinci argüman `events`): halkanın en yeni ≤ 10 olayını `event age=<ms> op=<n> skill=<id> caster=<id> target=<id> d0=<n> d1=<n> d2=<n>` olarak ve `events total=<n> in_ring=<n>` satırını yazar. Argümansız `snap` çıktısı **değişmez** (yalnız sonuna `events total=<n>` satırı eklenebilir).
4. Birim testleri (§6 K3).

**Kapsam dışı**

- Skill sınıflandırması (heal/debuff/buff/hasar), `MAGIC`/`MAGIC_TYPE*` tablosu okuma, gözlenen buff/debuff tablosu ve bitiş süresi tahmini: **F4-53**.
- Olayların karar/telemetri akışına bağlanması, yeni telemetri olayı, takım içi paylaşım (F6–F7).
- `ScriptPlan.h` izinli fiil listesine `events` eklemek: yok (`snap` zaten izinli; `events` ikinci argüman).
- `docs/` değişikliği.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | yalnızca dosya sonuna ekleme (§3.1) |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | 4 yeni `TEST_CASE` |
| `GameServer/Bot/BotSession.h` | değiştir | `m_skillEvents` |
| `GameServer/Bot/BotSession.cpp` | değiştir | yeni blok + temizlik |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `CommandSnap` |

## 5. Uygulama adımları

1. `git switch -c bot/F4-52 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; açık sunucuları durdur.
2. `Perception.h` ekleri. Testler: `Perception_ParseSkillEvent` (elle yazılmış 23 baytlık gerçek düzen: `op=1`, `skill=110518` = bayt `B6 AF 01 00`, `caster=2984`, `target=10001`, `data` sıfır; `op=3` + `data[0..2]` konum; 22 bayt `false`; `op=0` ve `op=14` `false`; null/0 `false`), `Perception_SkillRing_Basic` (ekleme sırası, 64 taşması ve `Total`, `At(0)` en yeni), `Perception_SkillRing_Queries` (`FindLatest` jokerli/jokersiz, pencere dışı olay bulunmaz, `CountIn`), `Perception_SkillRing_Copy` (kopya bağımsız).
3. `BotSession`/`BotManager` değişiklikleri; `snprintf` kullan.
4. Derleme ve test (§7).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, değişen dosyalar `touch` edilince yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`, test sayısı plan başındakinden **4 fazla**, dört yeni ad `[ OK ]`
- [ ] K4: `BotCore/Perception.h`'te `windows.h|stdafx|GameServer|shared/` grep'i boş (opcode sabitleri `BotCore`'da yinelenir, `packets.h` include edilmez)
- [ ] K5 (sözleşme): yeni satırlarda `g_pMain|GetUserPtr|m_MagictableArray|_MAGIC_TABLE|m_pUser->` yok; olay yalnızca alınan pakettendir
- [ ] K6: mevcut `m_castEcho` bloğu ve `/bot cast` davranışı değişmez (`-` satırı yok ya da yalnızca yorum); `ENABLED=0` davranışı değişmez
- [ ] K7: `m_skillEvents` yalnızca `m_obsLock` altında; ayrıştırma kilit dışında (`grep -n m_skillEvents`)
- [ ] K8: yeni ini anahtarı/komut/thread/paket isteği yok; ASCII + CRLF; `git diff --check` boş; `printf`(snprintf dışı)/`Sleep`/`CreateThread`/`rand(` yok
- [ ] K9 (çalışma zamanı, Claude yapar): üç bot aynı bölgede; `cast BotMF_K 110518 BotWP_E` sırasında **kurban** (`BotWP_E`) ve **izleyici** (`BotPHD_K`) için `snap <bot> events`: `op=1` CASTING sonra `op=3` EFFECTING, `caster` = `BotMF_K` kimliği, `target` = `BotWP_E` kimliği, `skill=110518`, EFFECTING `d0..d2` hedef konumu; olay zamanları cast süresine (≈ 1,1 sn) uyar; **fail** yolunda (MP yetersiz bot) izleyicide olay yok; başka bölgedeki botta olay yok; gerileme `snap`/`see`/`cast` çalışır, `Bot_*.log`'da `WARN`/`ERROR` 0

## 7. Doğrulama komutları

```bash
./tools/run-tests.sh Release 2>&1 | tail -3     # plan başında sayıyı not et
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release && ./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-52
grep -n "m_skillEvents" GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (ASCII, CRLF, tab, Allman, İngilizce yorum). `shared/packets.h` ISO-8859 olabilir: `grep -a`, **dokunma**.
- Halka sabit boyutlu (dinamik bellek yok); kilit altında yalnızca kopyalama/ekleme (tick süresini uzatma).
- 23 bayttan kısa paket olay sayılmaz (gerçek istemci 21 bayt gönderebilir: `docs/03` §13.2 notu; ama **sunucu yayını** her zaman 23 bayttır, bu plan sunucu yayınını okur).
- `data[]` anlamı skill'e göre değişir; ham saklanır, yorumlanmaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI (derleme Release/Debug rc=0, `96 tests, 0 failed`; çalışma zamanı K9 Claude'da)
- Branch / commit'ler: `bot/F4-52` (taban `gece/2026-10-02`); commit'ler bu satır eklendiğinde atılacak: kod + rapor `[F4-52] ...`, ardından `Durum` commit'i.
- Değişen dosyalar ve neden:
  - `BotCore/Perception.h` (+137): dosya sonuna `SkillEvent`, `ParseSkillEvent` (tam 23 bayt, op 1..13), `kSkillEventRing = 64` ve kopyalanabilir `SkillEventRing` (`Add`/`Count`/`Total`/`At`/`FindLatest`/`CountIn`/`Clear`) eklendi; opcode sabitleri `shared/packets.h` include edilmeden `kMagicCasting/kMagicFlying/kMagicEffecting/kMagicOpMax` olarak yinelendi, kaynak yorumda. Mevcut hiçbir satır silinmedi.
  - `Tests/BotCoreTests/PerceptionTests.cpp` (+202): `AddSkillEvent` yardımcısı ve 4 yeni `TEST_CASE` (`Perception_ParseSkillEvent`, `_SkillRing_Basic`, `_SkillRing_Queries`, `_SkillRing_Copy`).
  - `GameServer/Bot/BotSession.h` (+1): `BotCore::SkillEventRing m_skillEvents` `m_hp` satırının altına, `m_obsLock` altında.
  - `GameServer/Bot/BotSession.cpp` (+16, mevcut blok dokunulmadı): `WIZ_MAGIC_PROCESS && pkt.size() >= 23` için ayrı yeni blok (kilit dışında ayrıştır, kilit altında `Add`); `ResetForRespawn` içinde `m_skillEvents.Clear()`.
  - `GameServer/Bot/BotManager.cpp` (+43/−3): `CommandSnap` `[events]` biçimini kabul eder; halka kilit altında `eventCopy`'ye kopyalanır; argümansız `snap` sonuna `events total=<n>` satırı, `snap <bot> events` ile en yeni ≤ 10 olay + `events total=<n> in_ring=<n>` yazılır. Silinen 3 satır yalnızca eski kullanım-dizesi/copy-yorumu (cast yankısı bloğuna dokunulmadı).
- Derleme sonucu: `./tools/build.sh Release` ve `./tools/build.sh Debug` rc=0; değişen dosyalarda yeni uyarı yok (yalnızca eski `UpgradeHandler.cpp` C4789). `./tools/run-tests.sh Release --no-build` ve `Debug --no-build`: `96 tests, 0 failed` (plan başında 92).
- Kabul kriterleri öz-değerlendirme: K1 ✔ K2 ✔ K3 ✔ (96 = 92 + 4, dört yeni ad `[ OK ]`) K4 ✔ (Perception.h'te yalnızca yorumda `shared/packets.h` geçer, include yok; `check-perception-contract.py` R4 0) K5 ✔ K6 ✔ (cast echo bloğu birebir; `-` satırları yalnızca `CommandSnap` kullanım/copy yorumu) K7 ✔ (`m_skillEvents` yalnızca `m_obsLock` altında, ayrıştırma kilit dışında) K8 ✔ K9 Claude'da.
- Plandan sapmalar ve gerekçeleri: Yok. Not: `CountIn` imzası plandaki gibi `(op, target, nowMs, windowMs)`; `FindLatest`'te joker için `kSkillOpAny = 0xFF` ve `kSkillIdAny = -1` sabitleri eklendi (plan `0xFF`/`-1` diyordu).
- Açık sorular: Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
