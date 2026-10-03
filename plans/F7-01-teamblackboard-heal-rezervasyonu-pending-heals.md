# F7-01: `TeamBlackboard` iskeleti, rezervasyon tablosu ve `pending_heals` kaynağı (`BotCore/TeamBlackboard.h`) + F6-07 `hp_pred`'in takım kaynağına bağlanması

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F7 — Party koordinasyonu (`docs/17` §2 F7 bloğu, satır 165-176; kapı G7a) |
| Branch | `bot/F7-01 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-07** (`BotCore/PriestHeal.h`: `hp_pred`, `HpHistory`, `KnownHealLog`, `PriestMemory`; **`hp_pred` tek kaynağıdır**, bu plan onu yeniden yazmaz, yalnızca `pending_heals` girdisinin kaynağını tek-sahipli bellekten takım tablosuna bağlar) · **F6-01** (`BotCore/Brain.h` `BotRole`/`BotState`, `BrainParams`: `P-PRI-HEAL-EMERG`, `P-PRI-PREHEAL-K`; karar katmanı dosya yerleşimi) · F4-53 `KAPANDI` (`HealObsRing`, `ObservedStatusTable`) · F4-18 `KAPANDI` (`TeamView`) · F4-60 `KAPANDI`/F4-61 (sunucu bağlaması, §0) |
| İlgili gereksinim / kabul | REQ-PRI-06, REQ-PTY-04/05 temeli; AC-PRI-09 (birim testi kısmının **takım** bölümü: F6-07 tek-sahipli kısmı kapsar), AC-PRI-03 (MET-HEAL-04 ≤ %5: **yalnızca F7-08 çalışma zamanında**), T-PRI-03; `docs/07` §5.1, §6; `docs/09` §4.1-§4.3; `docs/13` §5.2a (`P` sınıfı gecikme) |
| Tahmini büyüklük | M (2 yeni dosya + `PriestHeal.h`/`PriestHealTests.cpp` küçük eklemeleri + 2 `.vcxproj` satırı = 6 dosya; ≈ 32 birim test) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 kapanmadan HAZIR yapılmaz) |

---

## 0. Neden TASLAK

**HAZIR yapma ön koşulları** (hepsi yazım turunda işaretlenir):

1. **F6-07 `KAPANDI`** ve fonksiyon adı/imzası bu plana işlenmiş olmalı. F6-07 taslağında (`drafts/f6/F6-07-priest-heal-dongusu.md` §3 madde 3) `hp_pred(u) = clamp(u.hp − preHealK · incoming_est(u) · horizon + pending_heals(u), 0, u.maxHp)`, `horizon = castMs/1000 + 0,4` ve `pending_heals` = **yalnızca kendi** devam eden cast'imin nominal heal'i (`PriestMemory`); test `PriHeal_HpPred_PendingAdds` (AC-PRI-09 birim düzeyi) F6-07'dedir. **Bu plan ikinci bir `hp_pred`/`IncomingEst`/`HpHistory` yazmaz** (`docs/21` §5 tek kaynak). F6-07 yazılırken `pending_heals` girdisini fonksiyona **tam sayı parametre** olarak geçirmiyorsa, bu planın ilk işi o bağlama noktasını açmaktır (`PriestHeal.h` değişiklik listesinde; formüle dokunulmaz).
2. **F6-01 `KAPANDI`:** `BrainParams` alan adları (`P-PRI-HEAL-EMERG` 0,32, `P-PRI-PREHEAL-K` 0,8 `drafts/f6/F6-01` §3.1 tablosunda **vardır**) ve `BotRole`/`BotState` kullanılır; bu plan **kendi varsayılan literal'lerini tutmaz**. F6-01 tablosunda **olmayan** F7 sabitleri (`kResCastGraceMs`, `kResTimeoutExtraMs`, `kTeamCommsDelayMs`: `docs/07:109`, `docs/09:75,95`) başlıkta `constexpr` `[Ö]` kalır; `BrainParams` kayıt defterine taşınmaları ayrı küçük plandır ("F7-00 parametre bloğu", yazılmadı: `F7-bagimlilik-ozeti.md`), çünkü F6-01 §3.1 "sonraki planlar satır eklemez" der.
3. `docs/09` §4.3 iki ifadesi çelişiyor ve **Claude** (doküman sahibi) netleştirmeli (§5 adım 0): `TAMAMLANDI` satırı "EFFECTING sonucu OK (veya bitiş + 0,3 sn)" derken `ZAMAN_ASIMI` satırı "bitiş + 1,0 sn'de sonuç gelmediyse silinir" der; ikincisi, birincinin "veya bitiş + 0,3 sn" kolu varsa ulaşılamazdır. Bu plan şu okumayı varsayar `[A]`: **`pending_heals` bitişten sonra saymaz; kayıt `bekliyor`da kalır ve `bitiş + 1,0 sn`de `ZAMAN_ASIMI` olur; `TAMAMLANDI` yalnızca açık sonuçla gelir.** Gerekçe: bitişten sonra heal ya uygulanmıştır (HP paketinde görünür, ikinci sayım çift sayım olur) ya da kaybolmuştur. Doküman bu okumaya göre güncellenmedikçe plan HAZIR olmaz.
4. Kendi heal'imizin `HealObsRing`'e girip girmediği (bota **kendi** `EFFECTING` yayını geliyor mu) F4-60 çalışma zamanında doğrulanmış olmalı: F6-07'nin `incoming_est`'i "bilinen heal"i kendi `KnownHealLog`'undan çıkarır; takımda başkasının heal'i `HealObsRing`'ten gelirse kendi heal'i hem log'dan hem halkadan sayılıp **çift sayım** olabilir. Bu karar F7-08'in bağlama adımıdır, bu planın testi değildir; plan yalnızca "tek kaynak" kuralını yazar.
5. `docs/17` §5 G7a ön koşulları (ADR-0018 m.4/m.6/m.8, F4-52/53) `KAPANDI`.

**Yazım turunda yeniden doğrulanacak referanslar** (bu taslağın yazıldığı okuma: `gece/2026-10-02` @ `7891f74`):

- `docs/07_PRIEST_BEHAVIOR.md:70` (formül `+ pending_heals`; **doğrulandı**: işaret doğru, `clamp(… , 0, u.maxhp)`), `:75-80` (`pending_heals` kuralları 1-4), `:103-115` (iki priest tablosu), `:109` (`bitiş = şimdi + cast + 0,3 sn`), `:37-40` (P-PRI-HEAL-EMERG 0,32; P-PRI-PREHEAL-K 0,8; P-PRI-HORIZON `cast süresi + 0,4 sn`), `:281` (AC-PRI-09).
- `docs/09_PARTY_COORDINATION_AND_TARGET_SELECTION.md:70-77` (§4.1 adalet, P-TEAM-COMMS-DELAY 300 ms), `:86-99` (§4.3 yaşam döngüsü), `:107` (HP gözlemi seyrekliği).
- `BotCore/Perception.h`: `kTeamMaxMembers = 8` (`:1191`), `kTeamNone` (`:1192`), `TeamView` (`:1469-1484`), `SkillEvent` (`:1804`), `HealObsRing` (`:2307`), `SkillHealNominal` (`:1969`).
- `Tests/BotCoreTests/MiniTest.h:138-164` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`); `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` (yeni dosya kayıtları; F5-09 planı §5.3 örneği).
- F6 taslakları: `F6-01` (`BrainParams`, `BotRole`), `F6-07` (`PriestHeal.h` `hp_pred` imzası; `PriestMemory`; test `PriHeal_HpPred_PendingAdds`).
- Test sayısı tabanı: yazım anında `259` (F4-53 sonrası); F4-60/F4-61, F6 planları ve nav hattı sayıyı değiştirir ⇒ plan başında yeniden ölçülür.

## 1. Amaç

Takım içi paylaşılan veri yapısının (`TeamBlackboard`) **rezervasyon bölümünü** saf mantık olarak eklemek: bir priest heal'e karar verince `{hedef, skill, miktar, bitiş = şimdi + cast + 0,3 sn}` kaydı yazar; **`pending_heals(u)` bekleyen heal toplamıdır** ve F6-07'nin `hp_pred(u)` fonksiyonuna girdi olarak verildiğinde `hp_pred` **artar** (`maxhp` ile sınırlı); iptal, hedefin ölmesi, menzil dışına çıkması, sahibin ölmesi, tamamlanma ve süre aşımında kayıt silinir ve `pending_heals` 0 olur; ikinci priest aynı hedefe yalnızca `ratio_after_pending < EMERG` ise heal'e izin alır. F6-07'nin tek-sahipli `pending` kaynağı bu tabloya bağlanır.

## 2. Bağlam (okunması zorunlu)

- `docs/07` §5.1 (`:67-80`): formül, `pending_heals` kuralları, `incoming_est` net HP'den ayrıştırma; §6 (`:103-115`) iki priest kuralı; §16 AC-PRI-09 (`:281`).
- `docs/09` §4 (`:66-99`): blackboard adalet kuralı (yalnızca gözlem sözleşmesine uygun bilgi, 300 ms gecikme), kayıt tipleri, rezervasyon yaşam döngüsü ve telemetri adları (`RES_CREATE`, `RES_DONE`, `RES_CANCEL` (neden), `RES_EXPIRED`).
- `docs/13` §5.2a (`:169-197`): `P` sınıfı = takımdan alınan, `P-TEAM-COMMS-DELAY` gecikmeli; `docs/13` §3 (ADR-0005): tüm bot tick'leri tek IOCP thread'inde ⇒ tablo kilitsizdir, çağıran seri çağırır.
- F6-07 taslağı (`hp_pred`, `PriestMemory`) ve F6-01 taslağı (`BrainParams`, `BotRole`): bu planın dayandığı ve **yeniden tanımlamadığı** türler.
- Örnek düzen: `BotCore/NavStuck.h` (`NavGuardBlockDetector`: sabit boyutlu halka, dinamik bellek yok) ve `Tests/BotCoreTests/NavStuckTests.cpp` (adlandırılmış testler, sabit tohum).

## 3. Kapsam

**Yapılacaklar** (`BotCore/TeamBlackboard.h` yeni; yalnızca standart kütüphane, sunucu başlığı/`windows.h`/global-static değişken yok; sabit boyutlu diziler, `new`/`malloc`/`std::vector` yok; ASCII + CRLF; `namespace BotCore`)

1. **Sabitler** (`[Ö]`, kaynak yorumuyla; `BrainParams`'ta olmayanlar, §0 madde 2): `kResCastGraceMs = 300` (`docs/07:109`), `kResTimeoutExtraMs = 1000` (`docs/09:95`), `kTeamCommsDelayMs = 300` (`docs/09:75-76`, P-TEAM-COMMS-DELAY), `kResCapacity = 32`, `kResClosedRing = 16`. **Ufuk** (`cast + 0,4 sn`) ve `emerg`/`preHealK` F6-07/`BrainParams`'tan gelir; burada tanımlanmaz.
2. **Türler:** `enum ResKind : uint8_t { RES_HEAL = 1, RES_CURE, RES_RES, RES_SUMMON, RES_PEEL }` (`docs/09:81`); `enum ResState : uint8_t { RES_FREE = 0, RES_PENDING, RES_DONE, RES_CANCEL, RES_TARGET_GONE, RES_OWNER_GONE, RES_EXPIRED }`; `enum ResCancelWhy { CANCEL_MAGIC_FAIL = 1, CANCEL_MOVED, CANCEL_SILENCE, CANCEL_NO_MP, CANCEL_GUARD_REJECT }` (`docs/09:92`); `enum ResGoneWhy { GONE_DEAD = 1, GONE_INVISIBLE, GONE_OUT_OF_RANGE }` (`docs/09:93`); `enum ResCreateStatus { RES_CREATE_OK, RES_CREATE_DUPLICATE, RES_CREATE_FULL, RES_CREATE_BAD_ARG }`.
3. **`struct Reservation`** (`uint32_t id; ResKind kind; int16_t target, owner; uint32_t skillId; int32_t amount; uint64_t startMs, endMs; ResState state;`) ve **`struct ResEvent`** (kapanmış kayıt günlüğü: `id, kind, target, owner, state, why (neden kodu), tMs`).
4. **`class ReservationTable`** (kopyalanabilir, kilitsiz, sabit boyut; `Clear()`):
   - `uint32_t Create(ResKind kind, int16_t target, int16_t owner, uint32_t skillId, int32_t amount, uint64_t nowMs, uint32_t castMs, ResCreateStatus * status = nullptr)`: başarıda `id > 0` (tek yönlü artan, yeniden kullanılmaz); `endMs = nowMs + castMs + kResCastGraceMs`; **aynı `(kind, target, owner)` için tek açık kayıt** (ikincisi `RES_CREATE_DUPLICATE`, 0 döner); `amount <= 0` (yalnızca `RES_HEAL` için; `RES_RES`/`RES_CURE`/`RES_SUMMON`/`RES_PEEL` `amount = 0` kabul eder), `target < 0`, `owner < 0` ⇒ `RES_CREATE_BAD_ARG`; tablo doluysa `RES_CREATE_FULL` (süresi dolmuş kayıt önce `Expire(nowMs)` ile temizlenmez: temizlik çağıranın işidir).
   - `bool Complete(uint32_t id, uint64_t nowMs)` (`RES_DONE`; kayıt kapanır, yer boşalır, `ResEvent` yazılır); `bool Cancel(uint32_t id, ResCancelWhy why, uint64_t nowMs)`; `int OnTargetGone(int16_t target, ResGoneWhy why, uint64_t nowMs)` (hedefin tüm açık kayıtları `RES_TARGET_GONE`; kapatılan sayı döner); `int OnOwnerGone(int16_t owner, uint64_t nowMs)` (`RES_OWNER_GONE`); `int Expire(uint64_t nowMs)` (`state == RES_PENDING` ve `nowMs >= endMs + kResTimeoutExtraMs` olanlar `RES_EXPIRED`).
   - `int32_t PendingHeals(int16_t target, uint64_t nowMs, uint32_t horizonMs, int16_t viewer) const`: yalnızca `kind == RES_HEAL`, `state == RES_PENDING`, aynı `target`, **`nowMs <= endMs` ve `endMs <= nowMs + horizonMs`** olan kayıtların `amount` toplamı. Görünürlük: `owner == viewer` ise hemen, değilse `nowMs >= startMs + kTeamCommsDelayMs` iken (`docs/09:97`). Saat geri giderse (`nowMs < startMs`) yabancı kayıt sayılmaz, çökme/taşma yok. `PendingHealsForeign(target, nowMs, horizonMs, viewer)` yalnızca `owner != viewer` toplamı (kapı için).
   - `bool HasForeignOpen(ResKind kind, int16_t target, int16_t viewer, uint64_t nowMs) const` (hedefte, `viewer`'dan başka bir sahibe ait, `RES_PENDING` ve görünür [`PendingHeals` ile aynı 300 ms gecikme] açık kayıt var mı; `nowMs > endMs` olan kayıt **sayılır** çünkü cure/diriltme/summon kapısı bitiş sonrası sonuç gelene kadar tekrarı önlemelidir; F7-02 cure kapısı ve F7-05 (summon) ve F7-07 (diriltme) rezervasyonları kullanır),
   - `const Reservation * FindOpen(ResKind, int16_t target, int16_t owner) const`, `int OpenCount() const`, `const ResEvent & LastClosed(int i) const`, `struct ResCounters { uint32_t created, done, cancelled, targetGone, ownerGone, expired; }` `const ResCounters & Counters() const` (telemetri adlarının sayaç karşılıkları: `RES_CREATE`/`RES_DONE`/`RES_CANCEL`/`RES_EXPIRED`; olay yazımı sunucu bağlamasının işidir).
5. **`class TeamBlackboard`** (iskelet): üye olarak `ReservationTable res;` ve `void Clear();`. Diğer kayıt tipleri (`TeamPlan`, `TargetCall`, `MemberStatus`, `EnemyIntel`, `PeelRequest`: `docs/09:77-84`) **bu planda yoktur**; sonraki planlar (F7-02/F7-03/F7-05) bu sınıfa **yalnızca ekleme** ile üye ekler.
6. **İkinci priest kapısı** (saf, inline; **`hp_pred` yok**): `struct HealGateInfo { int32_t pendingTotal, pendingForeign; bool ownOpen; }` ve `HealGateInfo QueryHealGate(const ReservationTable &, int16_t target, int16_t self, uint64_t nowMs, uint32_t horizonMs)` (`pendingTotal = PendingHeals(...)`, `pendingForeign = PendingHealsForeign(...)`, `ownOpen = FindOpen(RES_HEAL, target, self) != nullptr`); `enum HealGateWhy { GATE_OK_NO_FOREIGN = 1, GATE_OK_EMERG, GATE_DENY_FOREIGN_PENDING, GATE_DENY_OWN_OPEN }` ve `HealGateWhy HealGateDecide(const HealGateInfo &, float ratioAfterPending, float emerg)`: (a) `ownOpen` ⇒ `GATE_DENY_OWN_OPEN`; (b) `pendingForeign == 0` ⇒ `GATE_OK_NO_FOREIGN`; (c) aksi `ratioAfterPending < emerg` (**kesin küçüktür**) ⇒ `GATE_OK_EMERG`, değilse `GATE_DENY_FOREIGN_PENDING` (`docs/07:109`). **`ratioAfterPending`'i çağıran hesaplar**: `QueryHealGate().pendingTotal` değerini F6-07 `hp_pred`'ine geçirir ve `hp_pred / maxHp` alır (`docs/07:80`); böylece `TeamBlackboard.h` `PriestHeal.h`'ye include bağımlılığı taşımaz.
7. **F6-07 bağlaması** (`BotCore/PriestHeal.h`, **küçük ekleme**): F6-07'nin tek-sahipli `pending` hesabı, takım tablosu verildiğinde `QueryHealGate().pendingTotal`'a yönlendirilir (tablo verilmezse F6-07'nin eski davranışı **aynen** kalır: F6-07 testleri değişmeden geçer); `hp_pred` formülü, `horizon`, `clamp` değişmez.
8. **Birim testleri** (§5 adım 3; adlar bağlayıcı; her kapanma nedeni için **ayrı** test).

**Kapsam dışı (yapılmayacak)**

- Sunucu bağlaması (`GameServer/Bot/`: parti başına tek `TeamBlackboard` örneğinin sahipliği, `BotManager` yaşam döngüsü, `RES_*` telemetri olayları, heal `EFFECTING` sonucundan `Complete`/`Cancel` çağırma, `MAGIC_FAIL`'den iptal, `WIZ_DEAD`/menzil kaybından `OnTargetGone`, `BrainDriver` bağlaması): **F7-08** (öneri; bu taslak serisinde yazılmadı, `F7-bagimlilik-ozeti.md`).
- `hp_pred`/`IncomingEst`/skill seçimi/MP rezervi (`docs/07` §5.2): F6-07 (tek kaynak). Birincil healer atama, eşitlik bozma (slot kimliği), cure/diriltme/summon rezervasyonlarının **kullanımı**: F7-02/F7-06/F7-07. Bu planda `RES_CURE`/`RES_RES`/`RES_SUMMON`/`RES_PEEL` türleri yalnızca tabloda **saklanır**; `PendingHeals` onları saymaz.
- MET-HEAL-04 (çift heal oranı) ölçümü ve çalışma zamanı: yalnızca F7-08 (oyun içi T-PRI-03); bu plan MET-HEAL-04 için **kanıt üretmez**.
- Diğer `TeamBlackboard` kayıtları, `BrainParams` kayıt defterine F7 sabitlerinin taşınması, `docs/` değişikliği (Claude), `GameServer/`, `shared/`.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/TeamBlackboard.h` | yeni | §3.1-§3.6 |
| `Tests/BotCoreTests/TeamBlackboardTests.cpp` | yeni | §5 adım 3 (testler 7-31) |
| `BotCore/PriestHeal.h` | değiştir | yalnızca §3.7 bağlama noktası (F6-07 satırları değişmez; `git diff` yalnızca `+` ve bağlama satırı) |
| `Tests/BotCoreTests/PriestHealTests.cpp` | değiştir | yalnızca sona ekleme: testler 1-6 (`HpPredBoard_*`) |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca `<ClInclude Include="TeamBlackboard.h" />` (`Perception.h` satırından sonra; BOM ve CRLF korunur) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca `<ClCompile Include="TeamBlackboardTests.cpp" />` |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

0. **(Claude, plan HAZIR yapılmadan önce)** `docs/09` §4.3 `TAMAMLANDI`/`ZAMAN_ASIMI` ifadesini §0 madde 3'teki okumaya göre netleştir ve `docs/07` §5.1 madde 1'e "bitişten sonra sayılmaz" cümlesini ekle (doküman işi, DeepSeek dokunmaz). F6-07'nin `hp_pred` imzasını bu plana işle.
1. `git switch -c bot/F7-01 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; sunucuları durdur (`tools/run-servers.sh stop`); test sayısını not et (`./tools/run-tests.sh Release 2>&1 | tail -3`).
2. `BotCore/TeamBlackboard.h`'yi §3'e göre yaz; iki `.vcxproj` kaydını ekle (F5-09 planı §5.3 örneği); `PriestHeal.h` bağlama noktasını aç (§3.7).
3. Testler. Ortak yardımcılar `MakeTable()` ve sabitler `kA = 2984` (sahip A), `kB = 2985` (sahip B), `kT = 2986` (hedef). Örnek heal: `Create(RES_HEAL, kT, kA, 112554, 960, 10000, 1500)` ⇒ `endMs = 11800`; ufuk `1500 + 400 = 1900` ms (F6-07 `horizon`). Test adları (bağlayıcı):

   **`PriestHealTests.cpp` sonuna: `hp_pred` + takım kaynağı** (F6-07 `hp_pred` fonksiyonu çağrılır; `maxHp = 3000`, `preHealK = 0,8` (BrainParams varsayılanı), gelen hasar 500/sn; `pending` değeri `ReservationTable::PendingHeals`'ten alınır, elle yazılmaz):
   1. `HpPredBoard_NoReservation_MatchesFormula`: `hp 2000`, tablo boş ⇒ `2000 − 0,8·500·1,9 = 1240`.
   2. `HpPredBoard_PendingIncreasesPrediction`: aynı girdi + A'nın `RES_HEAL 960` kaydı ⇒ `2200`; `CHECK(sonuç > 1240)`.
   3. `HpPredBoard_ClampedToMaxHp`: `hp 2800`, gelen 0, kayıt 960 ⇒ `3000` (3760 değil); `hp == maxHp` ⇒ `3000`.
   4. `HpPredBoard_ClampedToZero`: `hp 300`, gelen 1000/sn, kayıt yok ⇒ `0`; kayıt 500 ⇒ `0` (−720); kayıt 1500 ⇒ `280`.
   5. `HpPredBoard_ForeignOwnerCountsAfterCommsDelay`: B kaydı `10000`'de; A için `10299`'da `hp_pred` kayıtsız değeri, `10300`'de `+960` artmış.
   6. `HpPredBoard_ClosedReservationNoLongerCounts`: kayıt `Complete`/`Cancel`/hedef ölümü sonrası `hp_pred` kayıtsız değere döner (her üç neden ayrı alt kontrol).

   **Kayıt yaşam döngüsü** (`TeamBlackboardTests.cpp`; her biri ayrı test; kayıt `10000` ms'de yazılmış, hedef `kT`, görüntüleyen `kA`):
   7. `Reservation_EndIsNowPlusCastPlus300`: `FindOpen(...)->endMs == 11800`, `startMs == 10000`, `state == RES_PENDING`, `Counters().created == 1`.
   8. `PendingHeals_AllOpen_Counted`: `10000` ms'de `PendingHeals == 960`.
   9. `PendingHeals_Completed_IsZero`: `Complete(id, 11600)` ⇒ `PendingHeals(11600) == 0`, `Counters().done == 1`, `LastClosed(0).state == RES_DONE`, `OpenCount() == 0`; ikinci `Complete` `false`.
   10. `PendingHeals_Cancelled_IsZero`: `Cancel(id, CANCEL_MAGIC_FAIL, 10500)` ⇒ 0, `Counters().cancelled == 1`; `Reservation_CancelReasons_AreRecorded`: beş neden için ayrı kayıt, `LastClosed(0).why` nedeni taşır.
   11. `PendingHeals_TargetDied_IsZero`: `OnTargetGone(kT, GONE_DEAD, 10800)` ⇒ dönüş 1, `PendingHeals == 0`, `Counters().targetGone == 1`.
   12. `PendingHeals_TargetOutOfRange_IsZero`: `GONE_OUT_OF_RANGE` aynı sonuç (`why` kaydı farklı); `PendingHeals_TargetInvisible_IsZero`: `GONE_INVISIBLE`.
   13. `PendingHeals_OwnerDied_IsZero`: `OnOwnerGone(kA, 10900)` ⇒ 0, `Counters().ownerGone == 1`; başka sahibin kaydı etkilenmez.
   14. `PendingHeals_ExpiredAfterEnd_IsZero`: `11800`'de hâlâ 960 (`nowMs <= endMs`), `11801`'de **0** (kayıt hâlâ `RES_PENDING`: `OpenCount() == 1`).
   15. `Reservation_Timeout_ClosesAtEndPlus1000`: `Expire(12799)` ⇒ 0 kapanır; `Expire(12800)` ⇒ 1, `Counters().expired == 1`, `LastClosed(0).state == RES_EXPIRED`.
   16. `Reservation_OneOpenPerKindTargetOwner`: aynı üçlü ikinci kez ⇒ `RES_CREATE_DUPLICATE`, `id 0`; farklı sahip ⇒ OK; farklı tür ⇒ OK; kapandıktan sonra aynı üçlü tekrar OK ve yeni `id` öncekinden büyük.
   17. `Reservation_RejectsBadArguments` (heal için `amount <= 0`, `target < 0`, `owner < 0`; `RES_CURE amount 0` **kabul**) ve `Reservation_CapacityFull_RejectsNew` (32 kayıt doldur, 33.sü `RES_CREATE_FULL`; biri kapanınca yeniden OK).
   18. `Reservation_IdsNeverReused` (1000 oluştur-kapat döngüsü, kimlikler kesin artan) ve `Reservation_CopyIsIndependent` (kopyaya ekleme kaynağı bozmaz).

   **`pending_heals` okuması**:
   19. `PendingHeals_OtherTarget_NotCounted`; `PendingHeals_CureKind_NotCounted` (`RES_CURE` kaydı heal toplamına girmez).
   20. `PendingHeals_BeyondHorizon_NotCounted`: `cast 3000` ⇒ `endMs 13300`; `10000`'de ufuk 1900 ⇒ 0; `11500`'de (13300 ≤ 13400) ⇒ miktar.
   21. `PendingHeals_SumsAcrossOwners`: A 960 + B 1920 ⇒ görüntüleyen A için (B kaydı gecikmeyi geçtikten sonra) 2880.
   22. `PendingHeals_ForeignInvisibleBeforeCommsDelay`: B `10000`'de yazar; A `10299`'da 0 görür, `10300`'de 960; B kendi kaydını `10000`'de hemen görür; `PendingHeals_ClockBackwards_NoUnderflow` (`nowMs < startMs`).

   **Kapı (ikinci priest)** (`ratioAfterPending` doğrudan verilir; `hp_pred` ile uçtan uca yol testler 5-6'dadır):
   23. `HealGate_NoForeignPending_Allowed` (`why == GATE_OK_NO_FOREIGN`, `pendingForeign == 0`).
   24. `HealGate_ForeignPending_AboveEmerg_Denied`: yabancı 960, `ratioAfterPending 0,787`, `emerg 0,32` ⇒ `GATE_DENY_FOREIGN_PENDING`.
   25. `HealGate_ForeignPending_BelowEmerg_Allowed`: yabancı 700, oran `0,267` ⇒ `GATE_OK_EMERG`.
   26. `HealGate_RatioEqualsEmerg_Denied`: oran tam `0,32f` ⇒ ret (kesin `<`); oran `0,3199f` ⇒ izin. (Çağıran `ratio` ürettiğinde eşitlik deterministik olsun diye test, `1000.0f / 3125.0f == 0.32f` eşitliğini de `CHECK` eder.)
   27. `HealGate_OwnOpenHeal_Denied`; `HealGate_ForeignExpired_AllowedAgain` (yabancı kayıt bitişi geçince `pendingForeign == 0` ⇒ kapı yeniden açılır).
   28. `HealGate_UsesPendingTotalForRatio`: yabancı 960 + kendi başka hedefteki kayıt (sayılmaz) ⇒ `QueryHealGate().pendingTotal` yalnızca aynı hedefi toplar.

   **Belirlenim/belgeleme**:
   29. `Board_SimultaneousDecision_BothCreate`: iki sahip aynı 300 ms penceresinde (`10000` ve `10100`) aynı hedefe yazarsa tablo ikisini de kabul eder (**bilerek**: pencerede çift heal'i önlemek F7-02'nin birincil healer/slot eşitlik bozmasıdır; test bu sınırı belgeler).
   30. `ForeignOpen_Rules`: `RES_CURE` kaydı B'den, görüntüleyen A: gecikme dolmadan `false`, sonra `true`; kendi kaydı `false`; `RES_HEAL` türü sorulursa tür eşleşmesi aranır; `nowMs > endMs` iken (kayıt hâlâ `RES_PENDING`) `true`, `Expire` sonrası `false`.
   31. `Board_Deterministic`: aynı çağrı dizisi iki tabloda aynı `PendingHeals`/`Counters`/`LastClosed` dizisini verir; `TeamBlackboard::Clear()` ilk durumu geri getirir.
   32. `PriestHealBinding_NoBoard_KeepsF607Behavior`: tablo verilmediğinde F6-07'nin tek-sahipli `pending` davranışı (iptal/ölüm/tamamlanma sonrası 0) aynen sürer (F6-07 testlerinin ikincil çapraz kontrolü).
4. Derleme ve test (§7). Çalışma zamanı denetimi yok (sunucu kodu değişmez).
5. `Durum` → `UYGULANDI`; Uygulayıcı Raporu (AGENTS.md §5).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0; yeni/değişen dosyalarda yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, yeni uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; test sayısı plan başındakinden **tam §5.3'teki `TEST_CASE` sayısı kadar** fazla (taslak sayım ≈ 32; yazım turunda kesinleştirilir); §5.3'teki **her ad** `[ OK ]` (özellikle `PendingHeals_Completed_IsZero`, `PendingHeals_Cancelled_IsZero`, `PendingHeals_TargetDied_IsZero`, `PendingHeals_TargetOutOfRange_IsZero`, `PendingHeals_OwnerDied_IsZero`, `PendingHeals_ExpiredAfterEnd_IsZero`, `HpPredBoard_PendingIncreasesPrediction`, `HpPredBoard_ClampedToMaxHp`, `HealGate_*`); F6-07 testleri (`PriHeal_*`) **değişmeden** geçer
- [ ] K4 (AC-PRI-09 haritası): birim testi kısmı karşılanır: (i) bekleyen heal `hp_pred`'i artırır ve `maxhp`'yi aşmaz (testler 2-3; F6-07'nin tek-sahipli testi ayrıca); (ii) iptal/hedef ölümü/menzil dışı/sahip ölümü/tamamlanma/süre aşımı sonrası `pending_heals == 0` (testler 6, 9-15); (iii) ikinci priest yalnızca `ratio_after_pending < EMERG` iken (testler 23-27). **MET-HEAL-04 ≤ %5 ve T-PRI-03 bu planda ölçülmez**; Uygulayıcı Raporu bunu açıkça yazar
- [ ] K5 (saflık ve tek kaynak): `grep -nE '#include|windows\.h|stdafx|GameServer|shared/' BotCore/TeamBlackboard.h` yalnızca standart kütüphane başlıkları (`PriestHeal.h` include **edilmez**); eklenen satırlarda `new `/`malloc`/`std::vector`/`std::map`/`printf`/`rand(` yok ve global/static değişken yok (`constexpr` sabitler hariç); `TeamBlackboard.h`'de `hp_pred`/`PredictHp`/`IncomingEst` tanımı **yok** (`grep -nE 'PredictHp|IncomingEst|hp_pred' BotCore/TeamBlackboard.h` yalnızca yorumlarda)
- [ ] K6 (biçim): iki yeni dosya ASCII + CRLF (`file` çıktısı `with CRLF line terminators`); girinti sekme, Allman; `git diff --check` boş
- [ ] K7 (kapsam): `git diff --stat gece/2026-10-02...bot/F7-01` yalnızca §4'teki 6 dosya + bu plan dosyası; `GameServer/`, `shared/`, `docs/`, `AIServer/`, `tools/` farkı 0; `PriestHeal.h` farkında silinen/değişen F6-07 satırı yok (yalnızca bağlama ekleri); iki `.vcxproj` farkı tek satır
- [ ] K8 (sabitler): `kResCastGraceMs == 300`, `kResTimeoutExtraMs == 1000`, `kTeamCommsDelayMs == 300` ve her birinin yanında `docs/07`/`docs/09` kaynak yorumu var (`grep -n 'docs/0[79]' BotCore/TeamBlackboard.h`); `emerg`/`preHealK`/ufuk için kodda yeni literal **yok** (BrainParams/F6-07)

## 7. Doğrulama komutları

```bash
./tools/run-tests.sh Release 2>&1 | tail -3        # plan başında sayıyı not et
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "HpPredBoard_|Reservation_|PendingHeals_|HealGate_|Board_|ForeignOpen_|PriestHealBinding_|PriHeal_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F7-01
git diff gece/2026-10-02...bot/F7-01 -- BotCore/PriestHeal.h | grep -c '^-[^-]'   # F6-07 satırı silinmedi/değişmedi: 0 (ya da yalnızca bağlama satırı)
file BotCore/TeamBlackboard.h Tests/BotCoreTests/TeamBlackboardTests.cpp
git diff --check gece/2026-10-02...bot/F7-01
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (CRLF, sekme, Allman, yorumlar İngilizce, yeni dosya `.vcxproj` kaydı), §2.5 (bota avantaj yok: bu plan yalnızca kendi tarafın bilgisini paylaşır; `P` sınıfı gecikme 300 ms modellenir, anlık paylaşım yok).
- **Adalet (docs/09 §4.1):** tabloya yazılan her şey botun gözlem sözleşmesine uygun bilgidir (kendi cast kararı, sahibin kendi kaydı). Düşmanın MP'si/cooldown'ı bu planda hiçbir yerde yok.
- Eşikler (`300`, `1000`) `[Ö]` tasarım parametresidir, doğrulanmış oyun değeri değildir; T-PRI-03 (çalışma zamanı) sonrası güncellenir.
- Bu planın en büyük riskleri: (a) §0 madde 3'teki doküman çelişkisi, (b) F6-07'nin `hp_pred` imzasının bu planın bağlama noktasıyla uyuşmaması. Uygulayıcı bunlardan biri yüzünden sapmak zorunda kalırsa **durup** Uygulayıcı Raporu'nda soru olarak yazar.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: (boş)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — (boş)
