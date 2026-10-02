# F4-52: `Perception` dilim 10 — görülen skill olayları (`WIZ_MAGIC_PROCESS` bölge yayını) için olay halkası

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-02, gece/2026-10-02, merge 34aeb41) |
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

### Tur 1 — 2026-10-02 (gece modu)

**Karar: DOĞRULANDI.** İncelenen commit: `bot/F4-52` @ `b088687` (taban `gece/2026-10-02`; iki commit: `91bbf55` kod, `b088687` rapor). Birleştirmeyi döngü betiği yapar.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | Dört kod dosyası `touch` edilip `./tools/build.sh Release` rc=0; log'da `warning` 0 (değişen dosyalarda ve genel) |
| K2 | ✔ | `./tools/build.sh Debug` rc=0; `warning` 0 |
| K3 | ✔ | `run-tests.sh Release --no-build` ve `Debug --no-build`: `96 tests, 0 failed`; taban `PerceptionTests.cpp` 39 → 43 `TEST_CASE` (+4); `Perception_ParseSkillEvent`, `_SkillRing_Basic`, `_SkillRing_Queries`, `_SkillRing_Copy` her iki yapılandırmada `[ OK ]` |
| K4 | ✔ | `grep -nE 'windows\.h|stdafx|GameServer|shared/' BotCore/Perception.h` yalnızca `:1790` yorum satırı (`shared/packets.h` kaynak notu); include yok; sabitler `kMagicCasting/Flying/Effecting/OpMax` yinelendi |
| K5 | ✔ | Eklenen satırlarda `g_pMain\|GetUserPtr\|m_MagictableArray\|_MAGIC_TABLE\|m_pUser->` 0; olay yalnızca `OnPacket` paketinden (`BotSession.cpp:71-84`); `check-perception-contract.py` yeni ihlal çıkarmadı (R3 satırları eski test sürücüsü kayıtları) |
| K6 | ✔ | `BotSession.cpp`/`.h` farkında `-` satırı 0 (mevcut `m_castEcho` bloğu `:58-68` dokunulmadı); `BotManager.cpp` `-` satırları yalnızca `CommandSnap` kullanım dizgisi + bir yorum. Çalışma zamanında `cast` aynı biçimde çalıştı; `ENABLED=0` yolu değişmedi (yalnızca alınan paket işlenir) |
| K7 | ✔ | `grep -n m_skillEvents`: `BotSession.cpp:82` (`lock_guard<m_obsLock>` altında `Add`), `:425` (`ResetForRespawn` kilitli blokta `Clear`), `BotManager.cpp:2566` (kilitli kopya), `BotSession.h:156` (bildirim); `ParseSkillEvent` kilitten önce (`BotSession.cpp:79`) |
| K8 | ✔ | Beş kod dosyası tamamen CRLF (`1924/1924`, `3443/3443`, `470/470`, `180/180`, `2450/2450`; depo LF, `autocrlf=true`); eklenen satırlarda ASCII dışı 0; `git diff --check` rc=0; yeni `printf`(snprintf dışı)/`Sleep`/`CreateThread`/`rand(` yok; yeni ini anahtarı/komut/thread/paket isteği yok (`snap <bot> events` mevcut `snap` fiilinin ikinci argümanı) |
| K9 (çalışma zamanı) | ✔ (fail yolu kısmen kod okumasıyla) | Aşağıda |

**Kapsam:** `git diff --stat gece/2026-10-02...bot/F4-52`: yalnızca planın izin listesindeki 5 dosya + kendi plan dosyası; `docs/`, `AGENTS.md`, başka plan, `shared/` değişmedi. Uygulayıcı Raporu'ndaki sayılar (+137/+202/+1/+16/+43−3) ve test sayısı (92 → 96) doğru.

**Çalışma zamanı (K9; `Release`, `GameServer.ini` değiştirilmedi (`ENABLED=1`, `TELEMETRY=decisions`); `BotMF_K` (2984), `BotWP_E` (2985), `BotPHD_K` (2986) zone 71 aynı bölgede; `BotWG_E` (2987) zone 71 ama (630, 920)'de, yani uzak bölgede; iş bitince `run-servers.sh stop`, `0/3`; `BotCommands.*` kalmadı):**

1. **Kurban ve izleyici ✔.** `cast BotMF_K 110518 BotWP_E` sonrası `snap BotWP_E events` ve `snap BotPHD_K events` aynı iki olayı verdi: `op=1 skill=110518 caster=2984 target=2985 d0=1274 d1=0 d2=892` (age 4402 ms / 4405 ms) ve `op=3 … d0=1274 d1=1 d2=892` (age 3301 ms / 3304 ms). CASTING→EFFECTING aralığı ≈ 1,10 sn (cast süresine uyar); EFFECTING `d0..d2` hedef konumu (1274, 892) ile aynı. Caster'ın kendi halkasında da aynı iki olay var (sunucu yayını caster'a da gider).
2. **İkinci cast ✔.** `BotPHD_K` `total` 2 → 4, aynı biçim (age 3302 / 4409 ms yeni, 50704 / 51805 ms eski).
3. **Başka bölgedeki bot ✔.** İki cast boyunca `snap BotWG_E events`: `total=0 in_ring=0`.
4. **NPC cast'leri ✔ (plan §2).** `BotWP_E` doğum noktasındayken NPC `caster=12955` olayları (`skill=300113`, `target=2985`) halkaya düştü (`total=10`); ayrım yapmadan saklanıyor.
5. **Fail yolu: kısmen.** Bot ön denetimi (`bad_skill`) geçersiz skill'i ve sınıfa uymayan skill'i istemci tarafında reddediyor (`cast BotMF_K 999999 …`, `cast BotPHD_K 110518 …` → `refused (bad_skill)`, paket gitmez); MP yetersizliği komutla kurulamadı. Sunucu tarafı **kod okumasıyla** doğrulandı: `MagicInstance.cpp:705-717` `SendSkillFailed` yalnızca çağırana (`pSkillCaster->isPlayer()`) yollar, bölgeye yayınlamaz; ilk (menzil dışı) cast `out_of_range` ile istemci tarafında durdu ve hiçbir botta olay üretmedi (`total=0`). Plan K9'un "izleyicide fail olayı yok" beklentisi bu yüzden canlı değil, kodla kanıtlandı; engel değil.
6. **Gerileme ✔.** Argümansız `snap BotPHD_K` sonuna `events total=4` ekledi (plan izin verdi), `see`, `cast`, `list`, `regene`, `move` çalıştı; hatalı `snap BotPHD_K foo` → `usage: snap <bot> [events]`; `Bot_2_10_2026.log` içinde bu oturumda `WARN`/`ERROR` 0.

**Bulgular (hepsi not, engel değil)**

1. **[Bilgi] NPC cast'leri 64'lük halkayı hızla doldurabilir** (`BotCore/Perception.h:1800` `kSkillEventRing`, `BotSession.cpp:71-84`). Doğum noktasında tek NPC (12955) birkaç saniyede 10 olay üretti; yoğun NPC bölgesinde bir botun ilgilendiği oyuncu olayları eskiyip atılabilir. Plan §2 ayrım yapmamayı açıkça istedi; F4-53'te sınıflandırma/süzme eklenirken (ör. yalnızca oyuncu ya da ilgili hedefe yönelik olayları ayrı tutmak) dikkate alınmalı.
2. **[Bilgi] `ParseSkillEvent` op aralığı reddinde `out`'u kısmen yazmış bırakır** (`BotCore/Perception.h:1816-1832`); çağıran `false`'ta `out`'u kullanmıyor (`BotSession.cpp:78-84`), zararsız. Plan yalnızca null için "dokunmaz" dedi.
3. **[Üslup] `At(i)` `while (idx < 0)` döngüsü** (`Perception.h:1868`) tek adımda biter (en çok bir tur); `%` ile eşdeğer, okunabilirlik tercihi.
4. **[Bilgi] Fail olayları görünmez (beklenen):** `MAGIC_FAIL` yalnız çağırana gider; F4-53 debuff "başarısız" gözlemini bu halkadan çıkaramaz, EFFECTING yokluğu/süre aşımı olarak yorumlamalıdır (`docs/07` §9.2 ile F4-53 planında ele alınmalı).
