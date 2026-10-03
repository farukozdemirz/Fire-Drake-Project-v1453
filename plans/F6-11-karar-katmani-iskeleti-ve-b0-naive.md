# F6-11: Karar katmanı iskeleti: ortak FSM, niyet (`Intent`) sözleşmesi, parametre kayıt defteri, `SkillReady` ve `B0-NAIVE` baseline'ı (`BotCore/Brain.h` ve 3 yeni başlık; F6-01 taslağının yazılmış hâli)

| Alan | Değer |
|---|---|
| Durum | İPTAL (F6-01'in kopyası; hat `f6`'da F6-01 olarak yapılıyor, 2026-10-03) |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; kapılar G6a/G6b/G6c, §5) |
| Branch | `bot/F6-11 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | Hepsi `KAPANDI`: F3-05 (`BotCore` + `Tests/BotCoreTests` çatısı, `Rng`); F4-03/F4-04/F4-05/F4-24 (`BotCore/BotCombat.h` guard fonksiyonları); F4-16/F4-17/F4-18 (`PerceptionSnapshot`, `SelfState`, `TeamView`); F4-50/F4-51/F4-52 (`UnitView` meta, `HpTable`, `SkillEventRing`); F4-53 (`SkillMeta`, `ObservedStatusTable`, `HealObsRing`, merge `0001d04`). F5 gerekmez (saf mantık). **F5-74'ten bağımsız** (F5-74 yalnızca `GameServer/Bot/` altındaki yedi dosyayı değiştirir; bu plan yalnızca `BotCore/`, `Tests/BotCoreTests/` ve iki `.vcxproj` satırı) |
| İlgili gereksinim / kabul | `docs/13` §6 (FSM), §7 (karar katmanı), §9 (parametre kayıt defteri), §13 (belirlenim); `docs/15` §5 (`B0-NAIVE`); `docs/14` §4 (parametre yüzeyi), AC-LRN-04 (aralık dışı değer reddi); `docs/11` §4 (geri çekilme; ADR-0019), MET-SUR-06 (durum salınımı) altyapısı; ADR-0020 (`B0-NAIVE` ve eşik); T-U (birim) |
| Tahmini büyüklük | M (7 dosya: 4 yeni başlık, 1 yeni test dosyası, 2 proje dosyası satırı; `GameServer` değişmez; sunucu davranışı değişmez: hiçbir yerden çağrılmaz) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu, ön-plan; referanslar `gece/2026-10-02` @ `3bf2ad2` üzerinde doğrulandı) |

---

## 0. Bu plan neden F6-11 (kimlik kaydı)

`plans/F6-01-karar-katmani-iskeleti-ve-b0-naive.md` (TASLAK) bu planın ilk taslağıdır. Otonom döngünün ön-plan düzeneği yalnızca **yeni** bir plan dosyasını kabul eder (`tools/auto-loop.sh` `--diff-filter=A`) ve plan kimlikleri değişmez/yeniden kullanılmaz; bu yüzden yazılmış hâli **F6-11** olarak eklendi ve F6-01 yuvası `İPTAL (yerine F6-11)` yapıldı (F5-63 → F5-74 ile aynı yöntem). `plans/F6-02..F6-10` ve `plans/F7-*` taslaklarındaki "F6-01" ifadeleri **bu planı** anlar (o planlar `/plan-olustur` ile yazılırken düzeltilir; bağımlılık denetimi `F6-11`'e bakar).

## 0a. Taslaktan farklar (yazım turunda bulunanlar; hepsi bu plana işlendi)

1. **ADR-0019 değerleri:** taslak `P-SUR-REENTER-HP` 0,65 ve `P-SUR-THREAT-WEIGHT` 1,0 diyordu (`docs/11`); proje sahibi kararı (ADR-0019, 2026-10-03) geri çekilmeyi **sabit %30** (`threatWeight = 0`) ve yeniden girişi **%85** yapar. Kayıt defteri varsayılanları bu karara göre yazıldı (§3.1 satır 2-3). `docs/11` metni doğrulamada güncellenir (K9).
2. **`ReasonCode` kaynağı düzeltildi:** `EmergencyHealRule`/`DeathAvoidRule` `docs/16` §6.9'da değil **§5.3** "Karar gerekçe kodları"ndadır; §5.3'ün dokuz adı da listeye girer (§3 madde 1).
3. **FSM eksik geçişleri dörtten altıya çıktı:** `Retreat -> Combat` (son direniş, `docs/11` §4.4: kaçış yoksa geri çekilmez) ve `Respawned -> Roam` (solo yeniden doğuş; diyagramda yalnızca party yolu var) eklendi; MET-SUR-06 (`Combat <-> Retreat`) ancak `Retreat -> Combat` kenarıyla ölçülebilir olur.
4. **`SelfTimers` eksikleri:** `standing` (hareket yok) ve `actionsInWindow` (CLI-11) taslakta yoktu, `CheckCastStart` bunları ister; `potWaitMs`/`castGapWaitMs` `SelfState`'te zaten olduğundan tekrarlanmadı.
5. **`SkillSpec` alan adları** MAGIC tablosuyla aynı birimde (`castTime`, `reCastTime`: ham değer; `CastDurationMs`/`CastRecastMs` çağrılır). `SkillReady` desteklenmeyen skill'i (`CastTypesSupported`/`CastMoralSupported`) ve eşya eksikliğini ayrı bayrakla bildirir (ActionExecutor'ın `unsupported_skill`'iyle uyumlu).
6. **Parametre tablosu tamamlandı:** F6-07/F6-08/F6-09 planlarının kullandığı `P-PRI-ENEMY-MELEE-MIN`, `P-PRI-POS-BACK`, `P-PRI-CURE-DOT-MIN`, `P-PRI-BUFF-REFRESH`, `P-PRI-HORIZON`, `P-MAG-BURST-WINDOW` satırları eklendi; her satıra açık `min`/`max` verildi (taslakta bazı satırlarda `—` vardı); çok bileşenli kimlikler ayrı satırlara bölündü (§3.1 kural 3).
7. Test sayısı başlangıcı 260 değil **315** (F5-72 sonrası), hedef 329.

## 1. Amaç

F4 aksiyonları (`ActionExecutor`) ve F5 navigasyonu birbirine bir **karar katmanı** olmadan bağlanamaz. Bu plan, kararın saf mantık (BotCore, sunucusuz, belirlenimli) iskeletini kurar: gözlem (`DecisionInput`) → niyet (`Intent`) sözleşmesi, `docs/13` §6 ortak durum makinesi (geçiş tablosu, minimum durum süresi, salınım sayacı), `docs/13` §9 parametre kayıt defteri (aralık dışı reddi), skill hazırlık denetimi (`SkillReady`: mevcut guard fonksiyonlarının üstünde ince bir katman), seed'li RNG kullanımı ve `B0-NAIVE` baseline politikası. Sonraki F6 planları (hedef seçimi, yaklaşma/saldırı, pot, geri çekilme, rol modülleri) bu sözleşmeyi **doldurur**; hiçbiri `Intent`/`DecisionInput` şeklini değiştirmez.

Bu plan **hiçbir sunucu kodundan çağrılmaz** (`GameServer/` değişmez; sürücü F6-06'dadır): sunucu davranışı ve ikilisi aynı kalır, kanıt yalnızca birim testi ve derlemedir.

## 2. Bağlam (okunması zorunlu)

Satır numaraları `gece/2026-10-02` @ `3bf2ad2` üzerinde okundu. Sapma görürsen **dur** (§5 adım 1).

- `docs/13` §6 (ortak durum makinesi; `:212-240`), §7.1 (`:244`), §7 (karar katmanı: acil kurallar → utility → geçerlilik filtresi → eşitlikte seed'li rastgele), §9 (parametre kayıt defteri: `id/type/default/min/max/learnable`), §13 (`seed_bot = hash(seed_episode, bot_slot)`, global `rand()` yok).
- `docs/15` §5 (`:247`, `B0-NAIVE`: "En yakın düşmana saldırır, rastgele hazır skill, geri çekilme yok, pot %30'da"); `docs/17` F6 satırı ve §5 G6a..G6c. **ADR-0020:** eşik "40 maçta ≥ %65"; `B0-NAIVE` tanımı bu planda `[A]` yazılır. **ADR-0019:** geri çekilme %30 sabit, yeniden giriş %85.
- `docs/14` §4.1-§4.3 (kapalı öğrenme listesi; değişmez kurallar kodda sabit, kayıt defterinde yok).
- `docs/16` §3.2 (`DECISION`, `STATE_CHANGE`, `TARGET_SET`), §4 (`DECISION` şeması: en çok 5 seçenek, `reason` kapalı liste, `override`), §5.2 (`:118`, 13 hedef değişim gerekçesi), §5.3 (`:122`, 9 karar gerekçesi), `MET-SUR-06` (`:182`: 10 sn içinde ≥ 3 `COMBAT ↔ RETREAT`).
- `BotCore/BotCombat.h` (guard fonksiyonları **yeniden kullanılır**, tekrar yazılmaz): `AttackIntervalMs :18`, `AttackRangeField :34`, `DistanceField :40`, `CheckAttack :61` (`kRMinIntervalMs :11`), `CastDurationMs :130`, `CastRecastMs :136`, `CastStartCheck :151` (alanlar `:151-173`), `CastVerdict` (`CAST_OK`..`CAST_REJECT_TOO_EARLY`), `CastWaitMs :187`, `CheckCastStart :217`, `IsFlyingCast :266`, `CastManaNeed :272`, `CastTypesSupported :323`, `CastTypeMoralSupported :337`, `SendsAimPoint :376`, `CastMoralSupported :384`, `IsGatedType :518`, `kCastGapMs`, `kTypeGateMs`, `kPotCooldownMs :600`, `PotionCheck :609`, `CheckPotion :632`, `kMaxActionsPerWindow`.
- `BotCore/Perception.h`: `SelfState :1115` (`hp/maxHp/mp/maxMp`, `dead`, `sitting`, `hpPotStock/mpPotStock`, `potWaitMs`, `castGapWaitMs`, `buffs[]`, `cooldowns[]` (`CooldownView { skillId, remainingMs }`), `inParty`, `partyLeader`), `UnitView :1141` (`id`, `nation`, `cls`, `x/z`, `dist`, `dead`, `invisibility`, `posState`, `moving`, `vx/vz`, `hpKnown/hp/maxHp`; ekipman/MP/cooldown **yok**: AC-LRN-03), `NpcView :1170`, `TeamView :1469`, `PerceptionSnapshot :1555` (`enemies[kSnapMaxUnits=32]`, `enemyCount`), `SkillMeta :1936`, `SkillHealNominal :1969`, `ObservedStatusTable :2014`, `HealObsRing :2323`, `HpTable :428`, `POS_FRESH/STALE/LOST :39-41`.
- `BotCore/Rng.h` (`DeriveBotSeed`, `Rng::NextBelow`, `NextU32`); `Tests/BotCoreTests/MiniTest.h:138-164` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`).
- `GameServer/Define.h:264-273` (`AttributeType`: 0 yok, 1 ateş, 2 buz, 3 yıldırım, 4 hafif büyü, 5 lanet, 6 zehir) ve `shared/database/structs.h:51-62` (`_MAGIC_TYPE3.bAttribute`, `bRadius`): `SkillSpec.element` bu değerin **ham kopyasıdır**.
- `docs/03` §2.1 (`:52-58`, sınıf kodları: warrior 101/105/106, mage 103/109/110, priest 104/111/112; rogue 102/107/108; El Morad +100).
- `tools/check-perception-contract.py`: `BotCore/*.h` taranır (R4: yalnızca standart ve kardeş başlık); yeni dosyalar R1-R5'te PASS kalmalıdır (K4).
- `Tests/BotCoreTests/NavBudgetTests.cpp:497, :927` yerel `struct BotState` tanımlar (adsız ad alanı, ayrı çeviri birimi): **çakışmaz**; yine de `BrainTests.cpp` içinde `using namespace` kullanılmaz, tipler `BotCore::` ile yazılır.

### Tasarım kararları (otonom döngüde Claude kararı — gözden geçirilmeli; yeni ADR yok: ADR-0019, ADR-0020 ve `docs/13` §6/§9 uygulaması; **ADR-0020 Ek F6-11 doğrulamada yazılır**)

**D1 — Kayıt defteri yalnızca sayısal ve düz bellek.** Her `BrainParams` alanı `float`'tır (ms, m, sayaç dahil); `ParamDef.offset = offsetof(BrainParams, alan) + indeks * sizeof(float)`; `static_assert(std::is_standard_layout<BrainParams>::value)`. `P-NAV-LOS-MODE` (sayım türü) ve türetilmiş `P-WAR-MELEE-RANGE` kayıt defterinde **yoktur** (sabit/formül, ilgili plandadır). F7 sabitleri (`P-WAR-BURST`, `P-MAG-SUMMON-*`, `P-PTY-*`, `P-TEAM-*`) bu tabloda **yoktur**.

**D2 — `learnable` yalnızca üst veridir.** `E` = `docs/14` §4.1 tablosunda veya §4.2'de adı geçen parametreler (28 satır); diğerleri `H`. `docs/06/07/08`'de "Evet" yazıp `docs/14` §4'te olmayanlar `H` kalır (`docs/14` §4 kapalı listedir; ekleme ADR ister). F6'da öğrenme kodu yoktur.

**D3 — Çok bileşenli kimlikler.** Docs'ta tek kimlikle verilen çok bileşenli parametre ayrı satırlara bölünür: `.MIN/.MAX`, `.M/.MS`, `.R15/.R8`, `.SOLO`, `.EXTRA`; `P-SOLO-EV-A0..A6` tek tek. Ek ad `SetParam` kimliğine **aynen** yazılır (`"P-MAG-PREF-RANGE.MIN"`).

**D4 — `ReasonCode` = `docs/16` §5.2 (13) + §5.3 (9) + 13 yeni kural kodu = 35 ad** (kapalı liste). Yeni 13 ad `docs/16`'ya doğrulamada eklenir (K9); kod adları büyük harfli yılan biçimli telemetri adıyla birebir eşlenir.

**D5 — FSM kenarları:** `docs/13` §6 diyagramındaki 21 kenar, **F7'ye ayrılmış durumlara (`Regroup`, `RespawnHold`, `ReadyForSummon`, `Reintegrate`) giren/çıkan kenarlar hariç** (F6'da `false`), artı altı ek kenar: `Engage -> Roam`, `Combat -> Roam` (hedef öldü), `Engage -> Retreat`, `Combat -> Engage` (menzil dışı/takip), `Retreat -> Combat` (son direniş), `Respawned -> Roam` (solo). `Dead` her durumdan girilebilir (`Dead -> Dead` yok), süreye takılmaz. Diyagramdan F6'da kalan 10 kenar: `Idle -> Prepare`, `Prepare -> Roam`, `Roam -> Engage`, `Engage -> Combat`, `Combat -> Retreat`, `Retreat -> Recover`, `Recover -> Roam`, `Combat -> Dead`, `Retreat -> Dead`, `Dead -> Respawned` (diyagramdaki 21 kenardan 11'i F7'ye ayrılmış durumlara dokunur).

**D6 — `B0-NAIVE` tanımı (`[A]`, §3.2).** HP pot kuralı yalnızca HP için yazılır, MP simetrik eklenir (ADR-0020 madde 2). Naive politika durum makinesini **kendisi** taşır (`NaivePolicy::m_fsm`), `Intent.state` her zaman makinenin gerçek durumudur.

## 3. Kapsam

**Yapılacaklar** (hepsi `BotCore/`, yalnızca standart kütüphane + `BotCore/*.h`; sunucu başlığı, `windows.h`, global/static değişken (`constexpr`/`inline constexpr` sabit hariç), tick yolunda dinamik bellek ve `rand()` yok; ASCII + CRLF + tab + Allman; yorumlar İngilizce)

1. `BotCore/Brain.h` (`#include "Perception.h"`, `"BotCombat.h"`, `"BrainParams.h"`, `"SkillReady.h"`; `namespace BotCore`; sıra: `BrainParams.h` ve `SkillReady.h` `Brain.h`'den bağımsız derlenebilir, `Brain.h` ikisini içerir):
   - `enum class BotRole : uint8_t { WarriorPressure, WarriorGuard, PriestHealDebuff, PriestHealBuff, MageFire, MageIce }` (`docs/04` profil kimlikleri `warrior.pressure`, `warrior.guard`, `priest.heal_debuff`, `priest.heal_buff`, `mage.fire_burst`, `mage.ice_control`); `enum class RoleFamily : uint8_t { Warrior, Rogue, Mage, Priest, Other }`; `inline RoleFamily RoleFamilyOfClass(uint16_t cls)` (`cls % 100`: 1/5/6 `Warrior`; 2/7/8 `Rogue`; 3/9/10 `Mage`; 4/11/12 `Priest`; başka `Other`), `IsMeleeFamily(RoleFamily)` (Warrior, Rogue), `IsCasterFamily` (Mage, Priest); `inline RoleFamily FamilyOfRole(BotRole)`.
   - `enum class BotState : uint8_t { Idle, Prepare, Roam, Engage, Combat, Retreat, Recover, Dead, Respawned, Regroup, RespawnHold, ReadyForSummon, Reintegrate }` (son dördü F7 için **ayrılmış**); `inline const char * BotStateName(BotState)` → `"IDLE"`, `"PREPARE"`, `"ROAM"`, `"ENGAGE"`, `"COMBAT"`, `"RETREAT"`, `"RECOVER"`, `"DEAD"`, `"RESPAWNED"`, `"REGROUP"`, `"RESPAWN_HOLD"`, `"READY_FOR_SUMMON"`, `"REINTEGRATE"` (`STATE_CHANGE`).
   - `enum class SubState : uint8_t { None, Approach, Pressure, Pursue, Disengage, Peel, Evaluate, Avoid, Duel, Chase, Sustain, Support, Rescue, Reposition, Burst, Kite }` + `SubStateName()` (büyük harf adlar; `docs/06` §7, `docs/10` §2, `docs/07` §13, `docs/08` §10).
   - `enum class ReasonCode : uint8_t` (D4): §5.2: `Init`, `TargetDead`, `TargetLostVis`, `TargetUnreachable`, `TeamCall`, `DebuffCall`, `HealerSwitch`, `Finishable`, `PeelThreat`, `SelfDefense`, `Retreat`, `LeaderOrder`, `ScoreMargin`; §5.3: `UtilityMax`, `EmergencyHealRule`, `DeathAvoidRule`, `CommitHold`, `TeamCallFollow`, `ResourceReserve`, `NavRecovery`, `SafetyRule`, `IdleNoOption`; yeni: `RulePotHp`, `RulePotMp`, `RuleRetreatHp`, `RuleRetreatTtd`, `RuleLastStand`, `RuleReenter`, `RuleApproach`, `RulePressure`, `RulePursue`, `RuleDisengage`, `RuleFinish`, `RuleNoTarget`, `RuleStuck`; `inline const char * ReasonName(ReasonCode)` → `"INIT"`, `"TARGET_DEAD"`, ..., `"RULE_POT_HP"`, ... (35 benzersiz ad; `docs/16`'daki adlar **harfi harfine**).
   - `struct Intent` (bir tick'in kararı; hareket, R, cast, pot, HP yoklaması ve duruş için **bağımsız yuvalar**: warrior aynı tick'te R + Type1 + pot gönderebilir, `docs/11` §3.5, CLI-02/CLI-11):
     ```cpp
     enum class MoveKind : uint8_t { None, Hold, FollowTarget, GotoPoint, DirectStep, Retreat };
     struct MoveIntent   { MoveKind kind; uint16_t targetId; float x, z; float ringMinM, ringMaxM; int16_t speedField; };
     struct AttackIntent { bool active; uint16_t targetId; };                       // normal attack (R) series
     struct CastIntent   { bool active; uint32_t skillId; uint16_t targetId; bool self; bool cancel; bool aim; float aimX, aimZ; };   // aim: area skill (Moral 10/6/11) aim point, target id -1 on the wire
     struct PotIntent    { bool active; uint8_t kind; uint32_t itemId; };           // kind 1 = HP, 2 = MP
     struct HpPollIntent { bool active; uint16_t targetId; };                       // WIZ_TARGET_HP (CLI-10: one target, >= 2 s)
     struct StanceIntent { bool active; bool sit; };                                // WIZ_STATE_CHANGE sit/stand (CLI-13), used by RECOVER (F6-05)
     struct Intent {
         BotState state; SubState sub; ReasonCode reason; bool override_;           // override_ = emergency rule (docs/13 section 7.1)
         uint16_t targetId; bool hasTarget;
         MoveIntent move; AttackIntent attack; CastIntent cast; PotIntent pot; HpPollIntent hpPoll; StanceIntent stance;
     };
     ```
     En çok **bir** `cast` yuvası; `attack`/`cast`/`pot` birlikte aktif olabilir; `Intent` düz veri (POD, `memset` ile sıfırlanabilir; `inline Intent MakeIntent()` sıfırlı döndürür).
   - `struct SelfTimers` (karar için gerekli, `SelfState`'te olmayan kendi zamanlayıcıları; sunucu bağlama doldurur): `bool hasWeapon`, `int16_t weaponRangeField` (0,1 m birimi, `AttackRangeField`), `uint32_t rWaitMs` (CLI-01 + aynı sunucu saniyesi; 0 = R şimdi çıkabilir), `uint32_t typeGateWaitMs[8]` (tip 1..7 için MEC-MAG-03 kalan bekleme, indeks 0 kullanılmaz), `bool standing` (yürüme yok), `int actionsInWindow` (`ActionRateWindow::CountInWindow`, CLI-11). (`potWaitMs` ve `castGapWaitMs` `SelfState`'te vardır, tekrarlanmaz.)
   - `struct NavView` (karar anındaki nav bilgisi; F5 bağlama dilimleri doldurur, F6-06 kullanır; yoksa `serviceUp = false` ve karar katmanı nav'sız savunmacı davranır; **Nav başlıklarını include etmez**, düz değerlerdir): `bool serviceUp`; `NavTargetHint targets[8]` (`uint16_t id`, `uint8_t verdict` {0 `None`, 1 `Reachable`, 2 `Unknown`, 3 `Unreachable`: `NavReachVerdict` sırasıyla eşlenir, `NavReach.h:48`}, `float pathM`, `bool inForbidden`, `bool crossesCluster`, `bool directClear`) ve `int targetCount`; `bool blockedByGuard`, `bool stalled`, `bool abandon`; `bool safePointFound`, `float safeX, safeZ, safePathM`, `bool lastStand`; `struct ArenaInfo { bool active; float cx, cz, r; float ownBaseX, ownBaseZ; } arena`.
   - `struct PotView { uint32_t itemId; uint8_t kind; int32_t value; uint32_t stock; }` (kind 1 = HP, 2 = MP; F6-04 doldurur/ekler, bu planda yalnızca bu iskelet) ve `constexpr int kBrainMaxSkills = 64, kBrainMaxPots = 8`.
   - `struct DecisionInput { const PerceptionSnapshot * snap; uint64_t nowMs; BotRole role; bool solo; SelfTimers timers; const SkillSpec * skills; int skillCount; const PotView * pots; int potCount; NavView nav; const ObservedStatusTable * status; const HealObsRing * heals; bool statusKnown; const BrainParams * params; }` (`status`/`heals`: F4-53/F4-60 tablolarının `m_obsLock` altında alınmış **kopyalarının** adresleri; `statusKnown` tablonun taze olduğunu söyler; `params == nullptr` ise varsayılan `BrainParams` kullanılır).
   - `class StateMachine` (`docs/13` §6; belirlenimli, bellek ayırmaz): `Reset(uint64_t nowMs)` (`Idle`, olay halkası ve salınım kaydı boş); `void SetMinRetreatMs(uint32_t)` (varsayılan 3000: `P-SUR-MIN-RETREAT-TIME`); `bool Request(BotState next, SubState sub, ReasonCode why, uint64_t nowMs, bool force = false)`: aynı duruma istek yalnızca `sub`'ı günceller (`true`, olay yok, bekleme sıfırlanmaz); geçiş yasal değilse `false`; **ayrılan durumun** minimum bekleme süresi dolmadıysa (`DwellMs(nowMs) < minDwell[state]`) ve `force == false` ve `next != Dead` ise `false`; aksi halde uygular, `StateEvent` yazar. `minDwell`: `Retreat` = `SetMinRetreatMs` değeri, `Engage` ve `Combat` 500 ms `[A]`, `Recover` 1000 ms `[A]`, diğerleri 0. `force` yalnızca süreyi ezer, yasallığı değil. `State()`, `Sub()`, `uint32_t DwellMs(nowMs)`, `int Oscillations(nowMs, windowMs = 10000)` (ayrı 8 girişli zaman damgası halkasında son `Combat -> Retreat` ve `Retreat -> Combat` geçişleri; `nowMs - t < windowMs` olanlar sayılır; telemetri boşaltması etkilemez: MET-SUR-06 hedef 0, ≥ 3 alarm); 16 girişli `StateEvent { uint64_t tMs; BotState from, to; SubState sub; ReasonCode reason; }` halkası (dolunca en eskiyi atar) ve `bool PopEvent(StateEvent &)` (en eskiden başlayarak boşaltır). Yasal kenarlar D5; `static bool IsLegal(BotState from, BotState to)` herkese açık (test için).
2. `BotCore/BrainParams.h` (yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`, `<type_traits>`): `struct BrainParams` (§3.1'deki **her satır için** bir `float` alan, varsayılan değer alan başlatıcısında; `soloEvA` `float[7]`; yeni alan eklenmez); `struct ParamDef { const char * id; uint16_t offset; float def, lo, hi; bool learnable; }`; `inline constexpr ParamDef kParamTable[]` (§3.1 sırasıyla) ve `inline constexpr int kParamCount`; `enum class ParamResult { Ok, UnknownId, OutOfRange, ConstraintViolated }`; `inline const ParamDef * FindParam(const char * id)` (`strcmp`; yok ise `nullptr`); `inline ParamResult SetParam(BrainParams &, const char * id, float value)` (aralık dışı **reddedilir, değer değişmez**: AC-LRN-04; yazmadan sonra `ValidateAll` bozulursa eski değer geri yazılır ve `ConstraintViolated` döner); `inline float GetParam(const BrainParams &, const char * id, bool * found = nullptr)`; `inline ParamResult ValidateAll(const BrainParams &)` (her alan `[lo, hi]` içinde, aksi halde `OutOfRange`; kısıtlar: `reenterHp >= retreatHp + 0,25` ve `reenterHp <= 0,90`; `priPosBackMinM <= priPosBackMaxM`; `magPrefMinM <= magPrefMaxM`; ihlal `ConstraintViolated`). `inline BrainParams DefaultBrainParams()`.
3. `BotCore/SkillReady.h` (`#include "Perception.h"`, `"BotCombat.h"`): `struct SkillSpec` (sunucu `_MAGIC_TABLE` alt kümesinin **değer kopyası**; sunucu başlığı yok): `uint32_t id`; `uint8_t type0, type1` (`MAGIC.Type1/Type2`), `uint8_t moral`; `uint16_t msp`; `uint8_t castTime` (`MAGIC.CastTime`); `uint16_t reCastTime` (`MAGIC.ReCastTime`, 0,1 sn); `uint16_t range` (m); `bool useStanding`; `uint32_t useItem`, `uint32_t beforeAction`; `uint16_t flyingEffect`; `uint16_t skillTree` (`MAGIC.Skill`), `uint8_t skillLevel`; `SkillMeta meta` (F4-53: Type3/4/5 alanları); `uint8_t element` (`MAGIC_TYPE3.bAttribute` ham kopyası); `uint8_t radius` (`MAGIC_TYPE3/4.bRadius`); `bool itemOk` (kendi çantasında `useItem` var: sunucu bağlama doldurur, karar katmanı çantayı okumaz); `inline int32_t SkillDamageNominal(const SkillMeta &)` = `max(0, -meta.firstDamage)` (heal için `SkillHealNominal` zaten var); `struct SkillReadyView { bool supported; bool ready; CastVerdict why; uint32_t waitMs; uint32_t manaNeed; }`; `inline SkillReadyView SkillReady(const SkillSpec &, const SelfState &, const SelfTimers &, float distM)`; `inline bool IsEnemyMoral(uint8_t)` (7), `IsSelfMoral(uint8_t)` (1), `IsFriendlyMoral(uint8_t)` (2, 4, 6, 11; kendine **dahil değil**).
   `SkillReady` **kendi mantığını yazmaz**; `CastStartCheck`'i şöyle doldurup `CheckCastStart` + `CastWaitMs` çağırır:
   - `supported = CastTypesSupported(type0, type1) && CastTypeMoralSupported(type0, moral) && CastMoralSupported(moral) && (useItem == 0 || itemOk)`; `!supported` ise `ready = false`, `why = CAST_OK` (anlamsız), `waitMs = 0`.
   - `distanceM = distM`, `skillRange = range`, `distanceField = DistanceField(distM)`, `weaponRangeField = timers.weaponRangeField`, `needsStanding = useStanding`, `standing = timers.standing`, `mana = self.mp`, `msp = CastManaNeed(msp, IsFlyingCast(type0, flyingEffect))` (= `manaNeed`), `reCastMs = CastRecastMs(reCastTime)`, `actionsInWindow = timers.actionsInWindow`.
   - **Tekrar bekleme:** `self.cooldowns[]` içinde `skillId == id` olan giriş `r` (`remainingMs`) ise `hasSkillLast = true`, `sinceSkillLastMs = (reCastMs > r) ? reCastMs - r : 0`; giriş yoksa `hasSkillLast = false`.
   - **Tip kapısı:** `typeGated = (id < 400000) && (IsGatedType(type0) || IsGatedType(type1))`; `w = max(typeGateWaitMs[type0] (type0 1..7 ise), typeGateWaitMs[type1] (type1 1..7 ise))`; `w > 0` ise `hasTypeLast = true`, `sinceTypeLastMs = kTypeGateMs - w` (w > `kTypeGateMs` ise 0).
   - **Boşluk:** `self.castGapWaitMs > 0` ise `hasAnyLast = true`, `sinceAnyLastMs = kCastGapMs - castGapWaitMs` (taşarsa 0).
   - `why = CheckCastStart(c)`, `waitMs = CastWaitMs(c)`, `ready = supported && why == CAST_OK`.
4. `BotCore/PolicyNaive.h` (`#include "Brain.h"`, `"Rng.h"`): `class NaivePolicy` (`B0-NAIVE`, §3.2): `void Reset(uint64_t nowMs, uint32_t minRetreatMs = 3000)`; `Intent Decide(const DecisionInput &, Rng &)`; `const StateMachine & Fsm() const`; özel üye `StateMachine m_fsm`.
5. `Tests/BotCoreTests/BrainTests.cpp` (§5 adım 3: 14 test).
6. `BotCore/BotCore.vcxproj` (`ClInclude` dört satır: `Brain.h`, `BrainParams.h`, `SkillReady.h`, `PolicyNaive.h`; mevcut `ClInclude` bloğu `:72-92`), `Tests/BotCoreTests/BotCoreTests.vcxproj` (`ClCompile` bir satır: `BrainTests.cpp`; mevcut blok `:79-102`).

### 3.1 Parametre kayıt defteri içeriği (sahibi: ilgili davranış dokümanı; kayıt defteri kopyadır)

Kurallar: (1) tablo **tam ve kapalıdır**: satır başına bir `BrainParams` alanı ve bir `kParamTable[]` girişi (`kParamCount == 77`), kimlikler benzersiz; (2) `L` sütunu `ParamDef.learnable`'dır (D2: `E` = true, `H` = false; `E` sayısı 28); (3) çok bileşenli kimlikler D3'e göre bölünmüştür; (4) **kalın** varsayılan, docs ile farklıdır ve nedeni kaynak sütunundadır (ADR-0019); (5) sonraki planlar satır **eklemez**, yalnızca kullanır (ekleme gerekirse o planın raporunda soru).

| # | Kimlik | Alan (`BrainParams`) | Varsayılan | min | max | L | Kaynak / etiket |
|---|---|---|---|---|---|---|---|
| 1 | `P-SUR-RETREAT-HP` | `retreatHp` | 0,30 | 0,20 | 0,40 | E | `docs/11` §4.2, `docs/14` §4.1; ADR-0019 madde 1 |
| 2 | `P-SUR-REENTER-HP` | `reenterHp` | **0,85** | 0,45 | 0,90 | E | ADR-0019 madde 2 (`docs/11` 0,65 yerine); kısıt `>= retreatHp + 0,25` |
| 3 | `P-SUR-THREAT-WEIGHT` | `threatWeight` | **0,0** | 0,0 | 2,0 | E | ADR-0019 madde 1 (ilk koşu: formül devre dışı; `docs/11` 1,0) |
| 4 | `P-SUR-MIN-RETREAT-TIME` | `minRetreatMs` | 3000 | 1000 | 10000 | H | `docs/11` §4.2 `[A]` aralık |
| 5 | `P-SUR-RECOVER-MP-WAR` | `recoverMpWar` | 0,50 | 0,30 | 0,90 | H | `docs/11` §5 (kimlik yeni `[A]`) |
| 6 | `P-SUR-RECOVER-MP-PRI` | `recoverMpPri` | 0,70 | 0,30 | 0,90 | H | `docs/11` §5 (kimlik yeni `[A]`) |
| 7 | `P-SUR-RECOVER-MP-MAG` | `recoverMpMag` | 0,70 | 0,30 | 0,90 | H | `docs/11` §5 (kimlik yeni `[A]`) |
| 8 | `P-POT-HP-EMERG` | `potHpEmerg` | 0,35 | 0,20 | 0,50 | H | `docs/11` §3.2 `[A]` aralık |
| 9 | `P-POT-HP-DEFICIT-MIN` | `potHpDeficitMin` | 0,9 | 0,6 | 1,1 | E | `docs/11` §3.2, `docs/14` §4.1 |
| 10 | `P-POT-HP-DEFICIT-MIN.SOLO` | `potHpDeficitMinSolo` | 0,7 | 0,6 | 1,1 | E | `docs/11` §3.2 ("solo'da 0,7") |
| 11 | `P-POT-MP-DEFICIT-MIN` | `potMpDeficitMin` | 0,95 | 0,6 | 1,1 | E | `docs/11` §3.3, `docs/14` §4.1 |
| 12 | `P-WAR-MP-RESERVE` | `warMpReserve` | 700 | 400 | 1200 | H | `docs/06` §3 |
| 13 | `P-WAR-BURST-MP` | `warBurstMp` | 1500 | 1000 | 2500 | H | `docs/06` §3 `[A]` aralık |
| 14 | `P-WAR-FINISH-HP` | `warFinishHp` | 0,25 | 0,15 | 0,40 | H | `docs/06` §3 |
| 15 | `P-WAR-SLOW-TRIGGER` | `warSlowTrigger` | 0,90 | 0,70 | 1,00 | H | `docs/06` §3 `[A]` aralık |
| 16 | `P-WAR-CHASE-MAX-DIST` | `warChaseMaxDistM` | 35 | 20 | 60 | E | `docs/06` §3, `docs/14` §4.1 |
| 17 | `P-WAR-CHASE-MAX-TIME` | `warChaseMaxTimeMs` | 8000 | 4000 | 15000 | E | `docs/06` §3, `docs/14` §4.1 |
| 18 | `P-WAR-STANDOFF-M` | `warStandOffM` | 1,0 | 0,3 | 1,8 | H | **yeni** `[A]` (F6-03) |
| 19 | `P-WAR-NAV-HANDOFF-M` | `warNavHandoffM` | 8,0 | 4 | 16 | H | **yeni** `[A]` (F6-03) |
| 20 | `P-WAR-SPRINT-MIN-DIST` | `warSprintMinDist` | 15,0 | 8 | 30 | H | **yeni** `[A]` (F6-03) |
| 21 | `P-SK-RANGE-MARGIN` | `skRangeMargin` | 1,5 | 0,5 | 3,0 | H | `docs/05` SK-09 |
| 22 | `P-TGT-W-KILL` | `tgtWKill` | 1,2 | 0,0 | 3,0 | E | `docs/09` §5.2, `docs/14` §4.1 |
| 23 | `P-TGT-W-REACH` | `tgtWReach` | 1,0 | 0,0 | 3,0 | E | `docs/09` §5.2, `docs/14` §4.1 |
| 24 | `P-TGT-W-THREAT` | `tgtWThreat` | 0,6 | 0,0 | 3,0 | E | `docs/09` §5.2, `docs/14` §4.1 |
| 25 | `P-TGT-W-DEBUFF` | `tgtWDebuff` | 0,5 | 0,0 | 3,0 | E | `docs/09` §5.2, `docs/14` §4.1 |
| 26 | `P-TGT-W-RISK` | `tgtWRisk` | 1,0 | 0,0 | 3,0 | E | `docs/09` §5.2, `docs/14` §4.1 |
| 27 | `P-TGT-COMMIT-MIN` | `tgtCommitMinMs` | 4000 | 2000 | 8000 | E | `docs/09` §5.2, `docs/14` §4.1 |
| 28 | `P-TGT-SWITCH-MARGIN` | `tgtSwitchMargin` | 0,30 | 0,10 | 0,60 | E | `docs/09` §5.2, `docs/14` §4.1 |
| 29 | `P-TGT-REACH-TIME` | `tgtReachTimeMs` | 4000 | 1000 | 10000 | H | `docs/09` §5.2 `[A]` aralık |
| 30 | `P-TGT-REACH-SLACK` | `tgtReachSlackM` | 3,0 | 0,0 | 10,0 | H | `docs/09` §5.2 `[A]` aralık |
| 31 | `P-TGT-REACH-HORIZON` | `tgtReachHorizonMs` | 20000 | 5000 | 60000 | H | **yeni** `[A]` (F6-02) |
| 32 | `P-TGT-OWN-DPS` | `tgtOwnDps` | 470 | 100 | 2000 | H | **yeni** `[A]` (F6-02) |
| 33 | `P-TGT-MAX-DIST` | `tgtMaxDistM` | 80 | 20 | 150 | H | **yeni** `[A]` (F6-02) |
| 34 | `P-TGT-ABANDON-HOLD-MS` | `tgtAbandonHoldMs` | 10000 | 3000 | 30000 | H | **yeni** `[A]` (F6-02: bırakılan hedef bu süre yeniden seçilmez) |
| 35 | `P-SOLO-ENGAGE-EV` | `soloEngageEv` | 0,1 | −1,0 | 1,0 | E | `docs/10` §3.3, `docs/14` §4.2 |
| 36 | `P-SOLO-DISENGAGE-EV` | `soloDisengageEv` | −0,15 | −1,0 | 1,0 | E | `docs/10` §3.3, `docs/14` §4.2 |
| 37 | `P-SOLO-CHASE-MAX.M` | `soloChaseMaxM` | 60 | 30 | 100 | E | `docs/10` §3.3 ("60 m veya 10 sn"), `docs/14` §4.1 |
| 38 | `P-SOLO-CHASE-MAX.MS` | `soloChaseMaxMs` | 10000 | 4000 | 20000 | E | `docs/10` §3.3, `docs/14` §4.1 |
| 39 | `P-SOLO-OUTNUMBER` | `soloOutnumber` | 1 | 0 | 3 | E | `docs/10` §3.3, `docs/14` §4.2 |
| 40 | `P-SOLO-RECOVER-HP` | `soloRecoverHp` | 0,85 | 0,50 | 1,00 | H | `docs/10` §3.3 |
| 41 | `P-SOLO-RECOVER-MP` | `soloRecoverMp` | 0,70 | 0,30 | 1,00 | H | `docs/10` §3.3 |
| 42 | `P-SOLO-EV-A0` | `soloEvA[0]` | 0,0 | −3,0 | 3,0 | H | `docs/10` §3.3 (katsayı a0) |
| 43 | `P-SOLO-EV-A1` | `soloEvA[1]` | 2,0 | −3,0 | 3,0 | H | `docs/10` §3.3 (katsayı a1) |
| 44 | `P-SOLO-EV-A2` | `soloEvA[2]` | 1,5 | −3,0 | 3,0 | H | `docs/10` §3.3 (katsayı a2) |
| 45 | `P-SOLO-EV-A3` | `soloEvA[3]` | 0,8 | −3,0 | 3,0 | H | `docs/10` §3.3 (katsayı a3) |
| 46 | `P-SOLO-EV-A4` | `soloEvA[4]` | 1,2 | −3,0 | 3,0 | H | `docs/10` §3.3 (katsayı a4) |
| 47 | `P-SOLO-EV-A5` | `soloEvA[5]` | 0,5 | −3,0 | 3,0 | H | `docs/10` §3.3 (katsayı a5) |
| 48 | `P-SOLO-EV-A6` | `soloEvA[6]` | 0,6 | −3,0 | 3,0 | H | `docs/10` §3.3 (katsayı a6) |
| 49 | `P-SOLO-V-KILL` | `soloVKill` | 1,0 | 0,0 | 3,0 | H | `docs/10` §3.3 |
| 50 | `P-SOLO-C-DEATH` | `soloCDeath` | 1,2 | 0,0 | 3,0 | H | `docs/10` §3.3 |
| 51 | `P-SOLO-C-TIME` | `soloCTime` | 0,05 | 0,0 | 3,0 | H | `docs/10` §3.3 |
| 52 | `P-SOLO-P-DEATH-LOSE` | `soloPDeathLose` | 0,6 | 0,0 | 1,0 | H | `docs/10` §3.3 |
| 53 | `P-SOLO-EV-HYST` | `soloEvHyst` | 0,10 | 0,0 | 0,5 | H | `docs/10` §3.3 |
| 54 | `P-SOLO-ROLE-ADJ-MAGE` | `soloRoleAdjMage` | 0,15 | 0,0 | 0,5 | H | `docs/10` §3.3, §4.2 |
| 55 | `P-SOLO-ROAM-R-M` | `soloRoamRadiusM` | 35 | 20 | 55 | H | **yeni** `[A]` (F6-10: arena içi dolaşma halkası) |
| 56 | `P-SOLO-FORCE-ENGAGE` | `soloForceEngage` | 0 | 0 | 1 | H | **yeni** `[A]` (F6-10: 0 = kapalı) |
| 57 | `P-SOLO-POT-REF` | `soloPotRef` | 4 | 1 | 10 | H | **yeni** `[A]` (F6-10: `own_resource_score` pot referansı) |
| 58 | `P-PRI-HEAL-EMERG` | `priHealEmerg` | 0,32 | 0,20 | 0,45 | E | `docs/07` §3, `docs/14` §4.2 |
| 59 | `P-PRI-HEAL-NORMAL` | `priHealNormal` | 0,75 | 0,60 | 0,85 | E | `docs/07` §3, `docs/14` §4.2 |
| 60 | `P-PRI-PREHEAL-K` | `priPreHealK` | 0,8 | 0,0 | 1,5 | E | `docs/07` §3, `docs/14` §4.2 |
| 61 | `P-PRI-OVERHEAL-MAX` | `priOverhealMax` | 0,25 | 0,0 | 0,60 | H | `docs/07` §3 `[A]` aralık |
| 62 | `P-PRI-MP-RESERVE` | `priMpReserve` | 1100 | 800 | 2000 | E | `docs/07` §3, `docs/14` §4.2 |
| 63 | `P-PRI-ENEMY-MELEE-MIN` | `priEnemyMeleeMinM` | 22 | 10 | 40 | H | `docs/07` §3 `[A]` aralık |
| 64 | `P-PRI-POS-BACK.MIN` | `priPosBackMinM` | 20 | 10 | 40 | H | `docs/07` §3 ("20–35 m"); kısıt `MIN <= MAX` |
| 65 | `P-PRI-POS-BACK.MAX` | `priPosBackMaxM` | 35 | 15 | 50 | H | `docs/07` §3 |
| 66 | `P-PRI-CURE-DOT-MIN` | `priCureDotMin` | 800 | 200 | 2000 | H | `docs/07` §3 `[A]` aralık |
| 67 | `P-PRI-BUFF-REFRESH` | `priBuffRefreshMs` | 20000 | 5000 | 60000 | H | `docs/07` §3 (20 sn) `[A]` aralık |
| 68 | `P-PRI-HORIZON.EXTRA` | `priHorizonExtraMs` | 400 | 0 | 1500 | H | `docs/07` §3 ("cast süresi + 0,4 sn"; ufuk = cast + bu değer) |
| 69 | `P-MAG-PREF-RANGE.MIN` | `magPrefMinM` | 30 | 15 | 45 | E | `docs/08` §4, `docs/14` §4.2; kısıt `MIN <= MAX` |
| 70 | `P-MAG-PREF-RANGE.MAX` | `magPrefMaxM` | 45 | 25 | 56 | E | `docs/08` §4, `docs/14` §4.2 |
| 71 | `P-MAG-MELEE-DANGER` | `magMeleeDangerM` | 18 | 8 | 30 | H | `docs/08` §4 `[A]` aralık |
| 72 | `P-MAG-AOE-MIN.R15` | `magAoeMinR15` | 3 | 1 | 6 | E | `docs/08` §4, `docs/14` §4.2 |
| 73 | `P-MAG-AOE-MIN.R8` | `magAoeMinR8` | 2 | 1 | 6 | E | `docs/08` §4, `docs/14` §4.2 |
| 74 | `P-MAG-MP-RESERVE` | `magMpReserve` | 500 | 300 | 1500 | H | `docs/08` §4 `[A]` aralık |
| 75 | `P-MAG-ELEM-SAMPLES` | `magElemSamples` | 3 | 1 | 10 | H | `docs/08` §4 `[A]` aralık |
| 76 | `P-MAG-BURST-WINDOW` | `magBurstWindowMs` | 6000 | 2000 | 15000 | H | `docs/08` §4 ("6 sn"); F6-08 solo karşılığı `[A]` |
| 77 | `P-ACT-LATENCY` | `actLatencyMs` | 250 | 100 | 1000 | H | `docs/16` MET-ACT-01 `[A]` aralık |

`role_adj` (mage +0,10; priest +0,05; W-G −0,05; W-P 0; ADR-0019 gereği ilk koşuda 0), `ttd < 2,5 sn` ve `docs/14` §4.3 değişmez kuralları kodda **sabittir**, kayıt defterinde yoktur.

### 3.2 `B0-NAIVE` tanımı (bu planın kararı `[A]`; `docs/15` §5 satırının işlenebilir hâli; ADR-0020 madde 2)

| Konu | Kural |
|---|---|
| Hedef | `snap->enemies[0..enemyCount)` içinde canlı (`!dead`), `invisibility == 0`, `posState != POS_LOST` olan **en yakın** (`dist` artan, eşitlikte küçük `id`) birim. Skor yok, bağlılık yok: her tick yeniden en yakın seçilir. Uygun düşman yoksa hedef yok. |
| Hareket | Hedef varsa `MoveKind::FollowTarget` (halka: yakın dövüş ailesi (`FamilyOfRole(in.role)`) ise `[0, 2]` m; caster ise `[0,8 × seçilen skill menzili, skill menzili]`, hazır skill yoksa en yüksek `range` değeri; `ringMinM`/`ringMaxM` metre); `nav.serviceUp == false` ise `Hold`. Hedef yoksa `None`. Güvenli nokta/geri çekilme **yok**. |
| Normal saldırı | Aile (`FamilyOfRole(in.role)`) `Warrior` ya da (`Priest` ve `timers.hasWeapon`) iken: `CheckAttack(false, 0, kRMinIntervalMs, DistanceField(hedef.dist), timers.weaponRangeField, timers.actionsInWindow) == ATTACK_OK` ve `timers.rWaitMs == 0` ise `attack = { true, hedefId }`. Mage R atmaz. |
| Skill | `skills[0..min(skillCount, kBrainMaxSkills))` içinden `SkillReady(...).ready` olan ve `IsEnemyMoral(moral)` (Moral 7) ya da (`FamilyOfRole(in.role) == Mage` ve `moral == 10`) olan adaylar arasından, aday sayısı > 0 ise `rng.NextBelow(sayı)` ile **tek biçimli** bir tane (aday yoksa `Rng` hiç çağrılmaz). Alan skill'inde `cast.aim = true`, `aimX/aimZ` = hedef konumu. Kendine/dosta skill (heal, buff) listeye **girmez**. |
| Pot | `pots[]` içinden `kind == 1`, `stock >= 1`, en yüksek `value` (eşitlikte küçük `itemId`) HP potu: `hp/maxHp < 0,30` ve `CheckPotion({stock, hasLast = (self.potWaitMs > 0), sinceLastMs = kPotCooldownMs - self.potWaitMs, actionsInWindow}) == POT_OK` ise `pot = { true, 1, itemId }`; HP koşulu yoksa `mp/maxMp < 0,30` ve `kind == 2` ile aynı kural (HP önce). Tek pot yuvası. `[A]`: "pot %30'da" HP için okunur, MP simetrik eklenmiştir. |
| Durum makinesi | `m_fsm` ile: ilk çağrı `Idle -> Prepare`, ikinci `Prepare -> Roam` (`Reason Init`); `self.dead` ⇒ `Dead` (`force`); ölü iken `!self.dead` olunca `Dead -> Respawned`, sonra `Respawned -> Roam`; `Roam` + hedef var ⇒ `Engage`; `Engage` + hedef menzilde (yakın dövüş: `distanceField <= weaponRangeField`; caster: `dist < seçilen skill menzili`) ⇒ `Combat`; `Combat` + hedef menzil dışı ⇒ `Engage`; `Engage`/`Combat` + hedef yok ⇒ `Roam`. Her `Decide` çağrısında **en çok bir** geçiş (ölüm hariç); makine reddederse `Intent.state` yine makinenin gerçek durumudur ve hareket/saldırı yukarıdaki kurallarla hesaplanır. `Retreat`/`Recover` **asla** üretilmez; `override_ == false`. |
| Adalet | Aynı `ActionExecutor` + `BotFairnessGuard` yolundan geçer (guard reddi saymaz); CLI/mekanik değişmez. |
| Belirlenim | Rastgelelik yalnızca verilen `Rng`'den; aynı `DecisionInput` dizisi + aynı tohum → aynı `Intent` dizisi. |
| Priest/mage | Priest: yalnızca `Moral 7` skill'ler (Judgment `112802`, Helis `112815`, Malice vb.), heal/buff yok. Mage: `Moral 7` ve `Moral 10` (alan). |

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Brain.h` | yeni | tipler, `StateMachine`, `RoleFamilyOfClass`, `NavView`, `DecisionInput` |
| `BotCore/BrainParams.h` | yeni | `BrainParams`, `kParamTable`, `SetParam`, `ValidateAll` |
| `BotCore/SkillReady.h` | yeni | `SkillSpec`, `SkillReady` (guard fonksiyonlarını çağırır) |
| `BotCore/PolicyNaive.h` | yeni | `NaivePolicy` |
| `Tests/BotCoreTests/BrainTests.cpp` | yeni | §5 adım 3 |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca dört `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca bir `ClCompile` satırı |

7 dosya. `GameServer/`, `AIServer/`, `shared/`, `docs/`, `tools/` ve diğer `BotCore`/`Tests` dosyaları **değişmez**. Listede olmayan dosya gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-11 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Başlamadan `./tools/run-tests.sh Release 2>&1 | grep "tests,"` ile mevcut test sayısını kaydet (beklenen **315** `[V: F5-72 doğrulaması, gece/2026-10-02 @ 3bf2ad2]`; farklıysa raporla ve K3'ü "başlangıç + 14" olarak oku). §2'deki referanslardan biri (özellikle `BotCombat.h` imzaları, `Perception.h` yapı alanları) tarif edilenden farklıysa **dur** ve raporla.
2. `BrainParams.h`, `SkillReady.h`, `Brain.h`, `PolicyNaive.h` yaz (§3). `SkillReady()` kendi menzil/MP/recast mantığını yazmaz; `BotCombat.h` fonksiyonlarını çağırır. Tick yolunda `std::vector`/`new` yok (sabit diziler; `StateMachine` olay halkası 16, salınım halkası 8, `NaivePolicy` aday dizisi `kBrainMaxSkills`).
3. `BrainTests.cpp`: aşağıdaki test adları **sabittir** (sentetik zaman, sabit `Rng` tohumu `DeriveBotSeed(1, 0)`; test yardımcıları dosyanın içinde, `namespace`siz `static`/adsız ad alanı; `PerceptionSnapshot` `memset` ile sıfırlanır):
   - `Brain_RoleFamily_ClassCodes`: 101/105/106/201/205/206 `Warrior`; 102/107/108/202/207/208 `Rogue`; 103/109/110/203/209/210 `Mage`; 104/111/112/204/211/212 `Priest`; 0 ve 150 `Other`; `FamilyOfRole` altı rol için beklenen aile.
   - `Brain_StateMachine_LegalTransitions`: `docs/13` §6'daki F7'ye ayrılmış durumlara dokunmayan 10 ok (D5) ve 6 ek kenar `IsLegal == true`; `Prepare -> Regroup`, `Respawned -> ReadyForSummon`, `Dead -> RespawnHold`, `RespawnHold -> Reintegrate`, `Combat -> Regroup` ve `Roam -> Recover`, `Idle -> Combat`, `Dead -> Dead` `false`; `Dead` altı durumdan (`Idle`, `Roam`, `Engage`, `Combat`, `Retreat`, `Recover`) `true`.
   - `Brain_StateMachine_MinDwell`: `Retreat`'e girildikten sonra `Recover` isteği 2999 ms'de reddedilir (`false`, durum değişmez), 3000 ms'de kabul; `force = true` ile 10 ms'de kabul; `Retreat -> Dead` 10 ms'de `force`suz kabul; `SetMinRetreatMs(5000)` sonrası 4999 ms'de red, 5000'de kabul; `Combat` 499 ms'de çıkışı reddeder, 500'de kabul.
   - `Brain_StateMachine_OscillationCount`: zaman çizelgesi `t=0` `Idle->Prepare->Roam->Engage`, `t=500` `->Combat`, `t=1000` `->Retreat`, `t=4000` `->Combat`, `t=4500` `->Retreat`: `Oscillations(4500) == 3`, `Oscillations(11000) == 2`, `Oscillations(14500) == 0`; telemetri `PopEvent` boşaltması sayıyı değiştirmez; 20 geçişten sonra olay halkası en yeni 16'yı sıralı verir, 17. `PopEvent` `false`.
   - `Brain_Params_Defaults_Match_Table`: `kParamCount == 77`, kimlikler benzersiz, `E` sayısı 28; `DefaultBrainParams()` her alanı §3.1 tablosundaki varsayılana **birebir** eşit (`CHECK_EQ`; tabloyu testte satır satır yaz); özellikle `reenterHp == 0.85f`, `threatWeight == 0.0f`, `retreatHp == 0.30f`, `soloEvA[1] == 2.0f`; `GetParam` aynı değeri verir; `ValidateAll(Default) == Ok`; `P-SUR-RETREAT-HP` `learnable == true`, `P-ACT-LATENCY` `false`.
   - `Brain_Params_Reject_OutOfRange`: `SetParam("P-SUR-RETREAT-HP", 0.45f)` → `OutOfRange` ve değer **değişmedi**; `SetParam("P-NO-SUCH", 1.0f)` → `UnknownId`; sınır değerleri `0.20f` ve `0.40f` kabul (kısıt bozulmuyorsa: `reenterHp` 0.85 ile uyumlu); `SetParam("P-MAG-PREF-RANGE.MIN", 30.0f)` ve ek adlı kimlikler çalışır; `SetParam(..., NaN)` `OutOfRange` (NaN `[lo, hi]` içinde sayılmaz).
   - `Brain_Params_Reenter_Constraint`: `SetParam("P-SUR-RETREAT-HP", 0.40f)` → `Ok` (0.40 + 0.25 = 0.65 ≤ 0.85); ardından `SetParam("P-SUR-REENTER-HP", 0.60f)` → `ConstraintViolated`, değer **0.85 kaldı**; `retreatHp = 0.40f`, `reenterHp = 0.60f` ile doğrudan doldurulmuş `BrainParams` için `ValidateAll == ConstraintViolated`; `SetParam("P-SUR-REENTER-HP", 0.91f)` → `OutOfRange`; `priPosBackMinM > priPosBackMaxM` ve `magPrefMinM > magPrefMaxM` `ConstraintViolated`.
   - `Brain_SkillReady_Verdicts`: Carving (`id 106525`, `type0 1`, `type1 0`, `Moral 7`, `Msp 90`, `castTime 5`, `range 0`, `weaponRangeField 20`, `mp 1000`, `distM 1.5`) için: hazır; menzil dışı (`distM 3.5`) `CAST_REJECT_OUT_OF_RANGE`; MP yok (`mp 89`) `CAST_REJECT_NO_MANA`; recast (`self.cooldowns` girişi `remainingMs 400`, `reCastTime 20` → `waitMs == 400`) `CAST_REJECT_RECAST`; tip kapısı (`typeGateWaitMs[1] = 600`) `CAST_REJECT_TYPE_GATE`, `waitMs == 600`; boşluk (`castGapWaitMs = 100`) `CAST_REJECT_GAP`, `waitMs == 100`; `actionsInWindow = 6` `CAST_REJECT_RATE`; desteklenmeyen çift tip (`type0 8`) `supported == false`; `useItem != 0` ve `itemOk == false` `supported == false`; uçan skill (`type0 3`, `flyingEffect != 0`, `msp 100`, `mp 199`) `CAST_REJECT_NO_MANA`, `manaNeed == 200`; `id >= 400000` iken `typeGated` uygulanmaz (`typeGateWaitMs` büyük olsa da hazır).
   - `Brain_Reason_Names_Unique_And_Closed`: 35 `ReasonCode` adı benzersiz ve boş değil; `docs/16` §5.2'deki 13 ad (`INIT`, `TARGET_DEAD`, `TARGET_LOST_VIS`, `TARGET_UNREACHABLE`, `TEAM_CALL`, `DEBUFF_CALL`, `HEALER_SWITCH`, `FINISHABLE`, `PEEL_THREAT`, `SELF_DEFENSE`, `RETREAT`, `LEADER_ORDER`, `SCORE_MARGIN`) ve §5.3'teki 9 ad (`UTILITY_MAX`, `EMERGENCY_HEAL_RULE`, `DEATH_AVOID_RULE`, `COMMIT_HOLD`, `TEAM_CALL_FOLLOW`, `RESOURCE_RESERVE`, `NAV_RECOVERY`, `SAFETY_RULE`, `IDLE_NO_OPTION`) **harfi harfine** mevcut; 13 durum adı ve alt durum adları benzersiz.
   - `Naive_Target_Nearest_Deterministic`: dört düşman (biri ölü, biri `invisibility 1`, biri `POS_LOST`, biri uygun) → uygun olan; iki uygun eşit mesafede → küçük `id`; hiç uygun yok → `hasTarget == false`, `move.kind == None`.
   - `Naive_NoRetreat_NoRecover`: HP %5 ve düşman 1,5 m'de (warrior, `weaponRangeField 20`) iken 30 ardışık `Decide` boyunca durum `Retreat`/`Recover` olmaz, `override_ == false`; ilk iki çağrı `Prepare`, `Roam` (sonra `Engage`, 500 ms sonra `Combat`).
   - `Naive_Pot_At30Percent`: HP 0,31 → pot yok; 0,29 → HP potu (en yüksek `value`); `potWaitMs > 0` → pot yok; stok 0 → pot yok; HP ve MP birlikte düşük → HP önce, MP yalnızca HP koşulu yoksa; MP 0,29 → MP potu.
   - `Naive_RandomSkill_ReadyOnly_Seeded`: aynı tohum aynı seçim dizisi, farklı tohum farklı dizi (64 tick, ≥ 4 aday skill); seçilen her skill `SkillReady().ready` ve `Moral 7`; hazır skill yokken `cast.active == false` ve `Rng` hiç çağrılmamıştır (aynı tohumlu ikinci `Rng` ile sonraki `NextU32` eşit).
   - `Naive_Roles_Covered`: warrior (`attack.active` + `cast` Type1 + pot aynı tick), priest (yalnızca `Moral 7` skill, heal/buff `cast`'e hiç girmez, silahsız priest R atmaz), mage (alan skill'inde `cast.aim == true`, `aimX/aimZ` = hedef konumu; R atmaz) için örnek `Intent`.
4. `BotCore.vcxproj` ve `BotCoreTests.vcxproj` satırlarını ekle (CRLF korunur; mevcut satırların girintisi ve sırası örnek alınır).
5. Derleme ve test (§7); `Durum` → `UYGULANDI` ve Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok (başlangıç uyarı sayısı adım 1'de kaydedilir)
- [ ] K2: `./tools/build.sh Debug` rc=0, yeni uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug` `0 failed`; on dört yeni test adı `[ OK ]` (`Brain_*` dokuz, `Naive_*` beş); mevcut testler değişmeden geçer; toplam = başlangıç + 14 (beklenen `329 tests, 0 failed`)
- [ ] K4: kural-tabanlı denetim: yeni başlıklarda `windows.h|stdafx|GameServer|shared/` **include** yok; yeni kodda `new|malloc|rand(` yok, global/static **değişken** yok (`constexpr`/`inline constexpr` sabit hariç); ASCII + CRLF (`file`), BOM yok; `python3 tools/check-perception-contract.py` PASS (R1-R5)
- [ ] K5: `BrainParams` varsayılanları §3.1 tablosuyla **birebir** (test K3'te), `reenterHp == 0,85` ve `threatWeight == 0,0` (ADR-0019); aralık dışı değer reddedilir ve değer değişmez (AC-LRN-04); kısıtlar (`reenterHp >= retreatHp + 0,25`) çalışır
- [ ] K6: `StateMachine` salınım sayacı `docs/16` MET-SUR-06 tanımıyla (10 sn, `Combat <-> Retreat`) uyumlu; `Retreat` minimum süresi 3000 ms (`P-SUR-MIN-RETREAT-TIME`) ve `SetMinRetreatMs` ile değişir; F7'ye ayrılmış durumlara geçiş `false`
- [ ] K7: belirlenim: aynı girdi + tohum → aynı `Intent` (test K3'te); `Intent`/`DecisionInput` kopyalanabilir, `Intent` `std::is_trivially_copyable`/`is_standard_layout` (`static_assert`); `SkillReady` kendi menzil/MP/recast mantığı yazmaz (`grep -n "CheckCastStart\|CastWaitMs" BotCore/SkillReady.h` ikisini de gösterir)
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F6-11` yalnızca §4'teki yedi dosya (+ plan dosyası); `GameServer/`, `AIServer/`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K9 (Claude, doğrulamada): `docs/13` §6 diyagramına altı ek kenarı ve F7'ye ayrılmış durum notunu işler; `docs/16` §5.3'e 13 yeni kural kodunu ekler; `docs/11` §4.2/`P-SUR-*` satırlarına ADR-0019 varsayılanlarını (0,30 sabit, 0,85, `THREAT-WEIGHT` ilk koşuda 0) not eder; `docs/15` §5 `B0-NAIVE` tanımını §3.2'ye göre günceller ve ADR-0020'ye "Ek F6-11" yazar (tanım proje sahibi incelemesinde onaylanır; `[A]` etiketi kalır); `plans/F6-02..F6-10`, `F7-*` taslaklarındaki "F6-01" ifadelerinin F6-11'i anlattığı `docs/STATUS.md`'de kayıtlıdır
- [ ] K10 (çalışma zamanı, bu planda yok): `B0-NAIVE`'in sunucuda koşusu F6-06/F6-10'dadır; bu plan yalnızca birim düzeyinde kapanır, `GELIŞTIRILDI` sayılmaz (`docs/17` §4: oyun içi kanıt F6-06'da)

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Brain_|Naive_|tests,"
./tools/run-tests.sh Debug 2>&1 | grep "tests,"
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F6-11
grep -nE "#include.*(windows\.h|stdafx|GameServer|shared/)" BotCore/Brain.h BotCore/BrainParams.h BotCore/SkillReady.h BotCore/PolicyNaive.h
git diff gece/2026-10-02...bot/F6-11 -- BotCore | grep -nE "^\+.*(\bnew\b|malloc|rand\()"
git diff --check gece/2026-10-02...bot/F6-11
file BotCore/Brain.h BotCore/BrainParams.h BotCore/SkillReady.h BotCore/PolicyNaive.h Tests/BotCoreTests/BrainTests.cpp
grep -n "CheckCastStart\|CastWaitMs" BotCore/SkillReady.h
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (CRLF, tab, Allman, ASCII yeni dosyalar, yorumlar İngilizce). Oyun mekaniği ve guard kuralları **değişmez**; `BotCombat.h` ve `Perception.h` **değiştirilmez**, yalnızca çağrılır/okunur.
- **Bota avantaj yok** (`docs/03` §13, `docs/13` §2-4, `docs/14` §5.2): `DecisionInput` yalnızca `PerceptionSnapshot` + kendi zamanlayıcıları + nav'dan beslenir; düşman MP/cooldown/envanteri alanı **yoktur** ve eklenmez. `tools/check-perception-contract.py` R1-R5 yeni başlıklarda da PASS kalmalı.
- `B0-NAIVE` bir **alt sınır baseline'ıdır**, `baseline-v1` (L0) değildir; L0 politikası F6-03..F6-10'da kurulur ve F8'de dondurulur.
- MSVC: `offsetof` yalnızca standart-yerleşimli türde (D1 `static_assert`); `inline constexpr` dizi ve `constexpr` yerel değişken biçimleri C++17'dir (proje C++17). Başlıklar yalnızca `inline`/`constexpr` içerir (çok çeviri birimli ODR güvenli).
- Eşikler `[Ö]`/`[A]`'dır; `docs/` sahibi dokümandaki değer değişirse tablo birlikte güncellenir (Claude).
- Çelişkiler (docs ile docs arası ve docs ile kod arası; Claude doğrulamada docs'a işler, K9):
  1. `docs/13` §6 diyagramında §3 D5'teki altı ek kenar yok; `docs/10` §2 `DUEL -> ROAM`, `docs/06` §7 `APPROACH`/`PRESSURE` kullanır.
  2. `docs/13` §6 durum adları ile `docs/10` §2 (`EVALUATE`, `AVOID`, `DUEL`, `CHASE`) ve `docs/06` §7 farklıdır: bu plan ortak durumu `docs/13`, ayrıntıyı `SubState` olarak tutar.
  3. `docs/11` `P-SUR-REENTER-HP 0,65` ve `P-SUR-THREAT-WEIGHT 1,0` ↔ ADR-0019 (0,85 ve ilk koşuda 0): ADR esas alınır; `docs/10` `P-SOLO-RECOVER-HP 0,85` ile artık uyumludur.
  4. `docs/16` §6.9 (`MET-ROLE-01`) `EMERGENCY_HEAL_RULE`/`DEATH_AVOID_RULE` adlarını anar; kod listesi §5.3'tedir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
- Kabul kriterleri öz-değerlendirme:
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar:
- İncelenen:
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | | |

- Bulgular (önem sırasıyla):
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

## İptal notu (2026-10-03, Claude)

Bu plan F6-01 taslağının ön-plan düzeneği tarafından (yeni dosya kuralı yüzünden) F6-11 kimliğiyle yeniden yazılmış hâlidir. Aynı gün proje sahibi F6-01..F6-05'i ikinci hat `f6`'ya verdi (`gece/2026-10-03-f6`, `docs/STATUS.md`); F6-01 orada yazıldı, uygulandı ve doğrulanıyor. İki hattın aynı dosyaları (`BotCore/Brain.h`, `BrainParams.h`, `SkillReady.h`, `PolicyNaive.h`) yazmaması için F6-11 **uygulanmadı** ve iptal edildi; kimlik yeniden kullanılmaz (`docs/21`). F6-11'in daha ayrıntılı ek bölümleri (varsa) F6-01 doğrulama turlarında alınır.
