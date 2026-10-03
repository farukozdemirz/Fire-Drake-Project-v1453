> **İPTAL — yerine F6-11.** Bu taslağın tasarım notları tarihseldir; geçerli sözleşme `plans/F6-11-karar-katmani-iskeleti-ve-b0-naive.md` planındadır (ADR-0019 varsayılanları, `ReasonCode` kaynağı düzeltmesi, altı ek FSM kenarı, tamamlanmış parametre tablosu). `plans/F6-02..F6-10` ve `plans/F7-*` taslaklarındaki "F6-01" ifadeleri F6-11'i anlar.

# F6-01: Karar katmanı iskeleti: ortak FSM, niyet (`Intent`) sözleşmesi, parametre kayıt defteri ve `B0-NAIVE` baseline'ı (`BotCore/Brain.h` ve 3 yeni başlık)

| Alan | Değer |
|---|---|
| Durum | İPTAL (2026-10-03, ön-plan: yazılmış hâli **F6-11** olarak eklendi; döngü yalnızca yeni plan dosyası kabul ettiğinden ve kimlikler yeniden kullanılmadığından; kimlik yeniden kullanılmaz) |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; kapılar G6a/G6b/G6c, §5) |
| Branch | `bot/F6-01 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F3-05 (`BotCore` + `Tests/BotCoreTests` çatısı, `Rng`) `KAPANDI`; F4-03/F4-04/F4-05/F4-24 (`BotCore/BotCombat.h` guard fonksiyonları) `KAPANDI`; F4-16/F4-17/F4-18 (`PerceptionSnapshot`, `SelfState`, `TeamView`) `KAPANDI`; F4-50/F4-51/F4-52 (`UnitView` meta, `HpTable`, `SkillEventRing`) `KAPANDI`. F5 gerekmez (saf mantık) |
| İlgili gereksinim / kabul | `docs/13` §6 (FSM), §7 (karar katmanı), §9 (parametre kayıt defteri), §13 (belirlenim); `docs/15` §5 (`B0-NAIVE`); `docs/14` §4.1 (aralıklar), AC-LRN-04 (aralık dışı değer reddi); MET-SUR-06 (durum salınımı) altyapısı; T-U (birim) |
| Tahmini büyüklük | M (7 dosya: 4 yeni başlık, 1 yeni test dosyası, 2 proje dosyası satırı; `GameServer` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; planlayıcı yardımcısı, F6 yazım turu) |

---

> **Karar (2026-10-03, ADR-0020, proje sahibi):** "anlamlı yener" eşiği = EVAL-1v1'de `B0-NAIVE`'e karşı **40 maçta en az %65** galibiyet (beraberlik yarım). `B0-NAIVE` tanımı (en yakın düşmana yürür, temel saldırı, HP < %30'da HP pot, kaçmaz/geri çekilmez) bu planda `[A]` olarak yazılır ve plan incelemesinde onaylanır; plandaki tanımsızlık notları bu karara göre okunur.

## 1. Amaç

F4 aksiyonları (`ActionExecutor`) ve F5 navigasyonu birbirine bir **karar katmanı** olmadan bağlanamaz. Bu plan, kararın saf mantık (BotCore, sunucusuz, belirlenimli) iskeletini kurar: gözlem (`DecisionInput`) → niyet (`Intent`) sözleşmesi, `docs/13` §6 ortak durum makinesi (geçiş tablosu, minimum durum süresi, salınım sayacı), `docs/13` §9 parametre kayıt defteri (aralık dışı reddi), seed'li RNG kullanımı ve `B0-NAIVE` baseline politikası. Sonraki F6 planları (hedef seçimi, yaklaşma/saldırı, pot, geri çekilme, rol modülleri) bu sözleşmeyi **doldurur**; hiçbiri `Intent`/`DecisionInput` şeklini değiştirmez.

## 1a. Neden TASLAK (HAZIR yapmak için)

Ön koşullar:

1. F4-53 (`SkillMeta`, `ObservedStatusTable`, `HealObsRing`) **`KAPANDI` ve `gece/2026-10-02`'de** (merge `0001d04`); F4-60 (sunucu bağlaması: `FillSkillMeta`, `m_status`, `m_healObs`) `DOĞRULANDI` ve birleşmiş (merge `7891f74`). Bu nedenle `SkillSpec` **`SkillMeta`'yı içerir** (`BotCore/Perception.h:1936`; `firstDamage`, `timeDamage`, `directType`, `buffType`, `isBuff`, `type5Kind` yeniden tanımlanmaz; §3 madde 3). `UnitView`'a gözlenen durum alanları ve R5 güncellemesi **F4-61**'dedir (planı yok): karar katmanı bu planlarda `ObservedStatusTable`'ı doğrudan sorgular.
2. F5 sunucu bağlama dilimleri (F5-55 dilimleri (`plans/F5-55-...md` §1A, 2026-10-03): **F5-59** `NavService` yaşam döngüsü ve harita yükleme (`HAZIR`; `bot/F5-59` dalı açık), **F5-61** kiriş guard'ı, **F5-62** `/bot goto` + waypoint zinciri + `NavPathfinder`/`NavReach`/maliyet katmanı kurulumu, **F5-63** `NavFollower`/takılma/F5-57 sözleşmesi, **F5-64** bütçe + `NAV_*` telemetri, **F5-65** nav durumu temizliği, **F5-66** çalışma zamanı doğrulama (hepsi `TASLAK`; F5-60/F5-67 yalnızca su denetimi/koşullu düzeltme)) yalnızca F6-06'yı bloklar; bu plan blokajsızdır ve nav dilimlerinden bağımsız HAZIR yapılabilir. Proje sahibi kuyruğu (`docs/STATUS.md:411`, 2026-10-03): F4-60 → F5-59 → F5-60 → F4-55 → F5-61..F5-65 → F4-61 → F5-66 → **F8-05** → **F6-01..F6-08** → F7-01..F7-05 → F8-03/04/06/07.
3. `docs/16` §5.2 kapalı kod listesine yeni `ReasonCode` adlarının işlenmesi (Claude'un docs işi) ve `docs/13` §6'ya eksik geçişlerin işlenmesi (aşağıda "Çelişkiler").

Yazım turunda yeniden doğrulanacak referanslar: `BotCore/Perception.h:1115` (`SelfState`), `:1141` (`UnitView`), `:1170` (`NpcView`), `:1469` (`TeamView`), `:1555` (`PerceptionSnapshot`), `:428` (`HpTable`), `:1840` (`SkillEventRing`); `BotCore/BotCombat.h:151-236` (`CastStartCheck`, `CheckCastStart`, `CastWaitMs`), `:600-640` (`PotionCheck`); `BotCore/Rng.h` (`DeriveBotSeed`, `Rng`); `Tests/BotCoreTests/MiniTest.h:138-164` (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`); test sayısı (`git grep -c '^TEST_CASE'` = 260 `[V: gece/2026-10-02 @ 7891f74]`); `tools/run-tests.sh` çıktı biçimi (`N tests, M failed`).

## 2. Bağlam (okunması zorunlu)

- `docs/13` §6 (ortak durum makinesi), §7 (karar katmanı: acil kurallar → utility → geçerlilik filtresi → eşitlikte seed'li rastgele), §9 (parametre kayıt defteri: `id/type/default/min/max/learnable`), §13 (`seed_bot = hash(seed_episode, bot_slot)`, global `rand()` yok).
- `docs/15` §5 (`B0-NAIVE` tanımı: "En yakın düşmana saldırır, rastgele hazır skill, geri çekilme yok, pot %30'da"); `docs/17` F6 satırı ve §5 G6a..G6c.
- `docs/14` §4.1 (öğrenmeye açık parametre aralıkları), §4.3 (değişmez kurallar: kodda sabit, kayıt defterinde yok).
- `docs/16` §3.2 (`DECISION`, `STATE_CHANGE`, `TARGET_SET` olay alanları), §4 (`DECISION` şeması: en çok 5 seçenek, `reason` kapalı liste, `override`), §5.2 (hedef değişim gerekçeleri), §6.9 `MET-ROLE-01` (`EMERGENCY_HEAL_RULE`, `DEATH_AVOID_RULE`).
- `BotCore/BotCombat.h`: guard fonksiyonları karar katmanının "hazır mı" önkontrolünde **yeniden kullanılır** (tekrar yazılmaz): `CheckCastStart` (`:217`), `CastWaitMs` (`:187`), `CheckAttack` (`:61`), `CheckPotion` (`:632`), `AttackIntervalMs` (`:18`), `kPotCooldownMs` (`:600`).
- `BotCore/Perception.h`: girdi tipleri (yukarıdaki satırlar). `UnitView` ekipman/MP/cooldown taşımaz (AC-LRN-03); karar katmanı bunları hiçbir koşulda okumaz.

## 3. Kapsam

**Yapılacaklar** (hepsi `BotCore/`, yalnızca standart kütüphane + `BotCore/*.h`; sunucu başlığı, `windows.h`, global/static değişken, tick yolunda dinamik bellek yok; ASCII + CRLF + tab + Allman)

1. `BotCore/Brain.h`:
   - `enum class BotRole : uint8_t { WarriorPressure, WarriorGuard, PriestHealDebuff, PriestHealBuff, MageFire, MageIce }` (`docs/04` profil kimlikleri `warrior.pressure`, `warrior.guard`, `priest.heal_debuff`, `priest.heal_buff`, `mage.fire_burst`, `mage.ice_control`); `inline int RoleFamilyOfClass(uint16_t cls)` (`cls % 100`: 1/5/6 warrior, 3/9/10 mage, 4/11/12 priest, diğer 2/7/8 rogue = `Other`; `docs/03` §2.1) ve `IsMeleeClass`/`IsCasterClass`.
   - `enum class BotState : uint8_t { Idle, Prepare, Roam, Engage, Combat, Retreat, Recover, Dead, Respawned, Regroup, RespawnHold, ReadyForSummon, Reintegrate }` (son dördü F7 için **ayrılmış**: F6 kodu bunlara geçiş üretmez; geçiş tablosunda `false`); `BotStateName()` → `"IDLE"`, `"PREPARE"`, ... (telemetri `STATE_CHANGE`).
   - `enum class SubState : uint8_t { None, Approach, Pressure, Pursue, Disengage, Peel, Evaluate, Avoid, Duel, Chase, Sustain, Support, Rescue, Reposition, Burst, Kite }` (rol/solo alt durumları: `docs/06` §7, `docs/10` §2, `docs/07` §13, `docs/08` §10; `STATE_CHANGE`'e ikinci alan olarak yazılır).
   - `enum class ReasonCode : uint8_t` kapalı liste: `docs/16` §5.2 (`Init`, `TargetDead`, `TargetLostVis`, `TargetUnreachable`, `TeamCall`, `DebuffCall`, `HealerSwitch`, `Finishable`, `PeelThreat`, `SelfDefense`, `Retreat`, `LeaderOrder`, `ScoreMargin`), `docs/16` §6.9 (`EmergencyHealRule`, `DeathAvoidRule`) ve **yeni** (docs/16 güncellemesi gerekir): `RulePotHp`, `RulePotMp`, `RuleRetreatHp`, `RuleRetreatTtd`, `RuleLastStand`, `RuleReenter`, `RuleApproach`, `RulePressure`, `RulePursue`, `RuleDisengage`, `RuleFinish`, `RuleNoTarget`, `RuleStuck`; `ReasonName()` → `"TARGET_DEAD"`, ... (benzersiz adlar).
   - `struct Intent` (bir tick'in kararı; **beş bağımsız yuva** (hareket, R, cast, pot, duruş; artı HP yoklaması), çünkü warrior aynı tick'te R + Type1 + pot gönderebilir, `docs/11` §3.5, CLI-02/CLI-11):
     ```cpp
     enum class MoveKind : uint8_t { None, Hold, FollowTarget, GotoPoint, DirectStep, Retreat };
     struct MoveIntent   { MoveKind kind; uint16_t targetId; float x, z; float ringMinM, ringMaxM; int16_t speedField; };
     struct AttackIntent { bool active; uint16_t targetId; };                       // normal attack (R) series
     struct CastIntent   { bool active; uint32_t skillId; uint16_t targetId; bool self; bool cancel; bool aim; float aimX, aimZ; };   // aim: area skill (Moral 10/6/11) aim point, target id -1 on the wire
     struct PotIntent    { bool active; uint8_t kind; uint32_t itemId; };           // kind 1 = HP, 2 = MP
     struct HpPollIntent { bool active; uint16_t targetId; };                       // WIZ_TARGET_HP (CLI-10: tek hedef, >= 2 sn)
     struct StanceIntent { bool active; bool sit; };                                // WIZ_STATE_CHANGE sit/stand (CLI-13), used by RECOVER (F6-05)
     struct Intent {
         BotState state; SubState sub; ReasonCode reason; bool override_;           // override_ = acil kural (docs/13 section 7.1)
         uint16_t targetId; bool hasTarget;
         MoveIntent move; AttackIntent attack; CastIntent cast; PotIntent pot; HpPollIntent hpPoll; StanceIntent stance;
     };
     ```
     En çok **bir** `cast` yuvası; `attack`/`cast`/`pot` birlikte aktif olabilir; `Intent` düz veri (POD, `memset` ile sıfırlanabilir).
   - `struct SelfTimers` (karar için gerekli, `SelfState`'te olmayan kendi zamanlayıcıları; sunucu bağlama doldurur): `hasWeapon`, `weaponDelay`, `weaponRangeField` (0,1 m birimi, `AttackRangeField`), `rWaitMs` (CLI-01 + aynı sunucu saniyesi; 0 = R şimdi çıkabilir), `typeGateWaitMs[8]` (tip başına MEC-MAG-03 bekleme), `castGapWaitMs` (`SelfState`'ten), `potWaitMs` (`SelfState`'ten).
   - `struct NavView` (karar anındaki nav bilgisi; F5-55 dilimleri doldurur, F6-06 kullanır; yoksa `serviceUp = false` ve karar katmanı nav'sız savunmacı davranır): `serviceUp`; `NavTargetHint targets[8]` (`id`, `verdict` {`None`, `Reachable`, `Unknown`, `Unreachable`: `BotCore::NavReachVerdict` ile eşlenir}, `pathM`, `inForbidden`, `crossesCluster`, `directClear` (hedefe düz çizgi kirişi `Walk` süpercover'ından geçiyor: son yaklaşım için, F5-50 kiriş denetimi)); `blockedByGuard`, `stalled` (`NavProgressVerdict` eşlemesi), `abandon` (`NavRecoveryAction::Abandon`); `safePointFound`, `safeX`, `safeZ`, `safePathM`, `lastStand` (`NavRetreatStatus::NoCandidate`); `ArenaInfo { bool active; float cx, cz, r; float ownBaseX, ownBaseZ; }`.
   - `struct DecisionInput { const PerceptionSnapshot * snap; uint64_t nowMs; BotRole role; bool solo; SelfTimers timers; const SkillSpec * skills; int skillCount; const PotView * pots; int potCount; NavView nav; const ObservedStatusTable * status; const HealObsRing * heals; bool statusKnown; const BrainParams * params; }` (`status`/`heals`: F4-53/F4-60 tablolarının `m_obsLock` altında alınmış **kopyaları**; `statusKnown` tablonun taze olduğunu söyler; `PotView` F6-04'te tanımlanır; bu planda `struct PotView { uint32_t itemId; uint8_t kind; int32_t value; uint32_t stock; };` boş iskelet olarak yer alır, F6-04 yalnızca ekler/doldurur).
   - `class StateMachine` (`docs/13` §6; belirlenimli, bellek ayırmaz): `Reset(nowMs)`; `bool Request(BotState next, SubState sub, ReasonCode why, uint64_t nowMs, bool force = false)` (yasal geçiş tablosu `kLegal[state][state]`, minimum bekleme `minDwellMs[state]`: `Retreat` = `P-SUR-MIN-RETREAT-TIME`, `Engage`/`Combat` 500 ms `[A]`, `Recover` 1000 ms `[A]`, diğerleri 0; `force` yalnızca `Dead` ve süreyi ezen acil geçişler için); `State()`, `Sub()`, `DwellMs(nowMs)`; `int Oscillations(nowMs, windowMs = 10000)` (`Combat <-> Retreat` geçiş sayısı: MET-SUR-06 hedef 0, ≥ 3 alarm); kapasite 16 `StateEvent { tMs, from, to, sub, reason }` halkası ve `bool PopEvent(StateEvent &)` (telemetri boşaltır).
     Yasal geçişler = `docs/13` §6 diyagramı **artı** F6'nın gerektirdiği, diyagramda eksik dört geçiş (§8 "Çelişkiler" 1): `Engage -> Roam`, `Combat -> Roam` (hedef öldü, `docs/10` §2 `DUEL -> ROAM`), `Engage -> Retreat` (yaklaşırken saldırıya uğrama), `Combat -> Engage` (menzil dışı/takip).
2. `BotCore/BrainParams.h`: `struct BrainParams` (alan başına varsayılan = `docs/` değeri) ve `struct ParamDef { const char * id; int fieldOffsetIndex; float def, lo, hi; bool learnable; const char * ownerDoc; }` tablosu `kParamTable[]`; `enum class ParamResult { Ok, UnknownId, OutOfRange, ConstraintViolated }`; `ParamResult SetParam(BrainParams &, const char * id, float value)` (aralık dışı **reddedilir, değer değişmez**: AC-LRN-04); `ParamResult ValidateAll(const BrainParams &)` (kısıt: `reenterHp >= retreatHp + 0.25`, `docs/14` §4.1; `reenterHp <= 0.90`). Başlangıç içeriği (tablo §3.1) bütün F6 planlarının parametrelerini **tek yerde** toplar; sonraki planlar satır eklemez, yalnızca kullanır (ekleme gerekirse o planın raporunda soru).
3. `BotCore/SkillReady.h`: `struct SkillSpec` (sunucu `_MAGIC_TABLE` alt kümesinin **değer kopyası**; sunucu başlığı yok): `id`, `type0`, `type1`, `moral`, `msp`, `castTimeX100`, `reCastX100`, `range` (m), `useStanding`, `useItem`, `beforeAction`, `flyingEffect`, `skillTree` (`MAGIC.Skill`), `skillLevel`, **`SkillMeta meta`** (F4-53: Type3/4/5 alanları; nominal heal `SkillHealNominal(meta, hot)` mevcut, hasar için `inline int32_t SkillDamageNominal(const SkillMeta &)` = `max(0, -meta.firstDamage)` bu planda eklenir), `element` (0 yok, 1 ateş, 2 buz, 3 yıldırım; kaynak `MAGIC_TYPE3.bAttribute`, yazım turunda doğrulanır), `radius` (alan skill'lerinde `MAGIC_TYPE3/4.Radius`), `itemOk` (kendi çantasında `useItem`/sınıf taşı var: sunucu bağlama `CanUseItem` ile doldurur; karar katmanı çantayı okumaz, yalnızca bu bayrağı); `struct SkillReadyView { bool ready; CastVerdict why; uint32_t waitMs; }`; `inline SkillReadyView SkillReady(const SkillSpec &, const SelfState &, const SelfTimers &, float distM, uint64_t nowMs)` (kendi mantığını yazmaz: `CastStartCheck` doldurur, `CheckCastStart` + `CastWaitMs` çağırır; `SelfState.cooldowns` tablosundan skill başına kalan süreyi, `typeGateWaitMs`'tan tip kapısını okur); `inline bool IsFriendlyMoral(uint8_t)`, `IsEnemyMoral(uint8_t)` (7), `IsSelfMoral(uint8_t)` (1).
4. `BotCore/PolicyNaive.h`: `class NaivePolicy` (`B0-NAIVE`, §3.2 tanımı). `Intent Decide(const DecisionInput &, Rng &)`.
5. `Tests/BotCoreTests/BrainTests.cpp` (§5.3).
6. `BotCore/BotCore.vcxproj` (`ClInclude` dört satır), `Tests/BotCoreTests/BotCoreTests.vcxproj` (`ClCompile` bir satır).

### 3.1 Parametre kayıt defteri başlangıç içeriği (sahibi: ilgili davranış dokümanı; kayıt defteri kopyadır)

| Kimlik (alan) | Varsayılan | Aralık `[min, max]` | Kaynak / etiket |
|---|---|---|---|
| `P-SUR-RETREAT-HP` (`retreatHp`) | 0,30 | 0,20–0,40 | `docs/11` §4.2, `docs/14` §4.1 `[Ö]` |
| `P-SUR-REENTER-HP` (`reenterHp`) | 0,65 | 0,45–0,90; `>= retreatHp + 0,25` | `docs/11`, `docs/14` |
| `P-SUR-THREAT-WEIGHT` (`threatWeight`) | 1,0 | 0,0–2,0 | `docs/11` |
| `P-SUR-MIN-RETREAT-TIME` (`minRetreatMs`) | 3000 | 1000–10000 `[A]` | `docs/11` |
| `P-POT-HP-EMERG` (`potHpEmerg`) | 0,35 | 0,20–0,50 `[A]` | `docs/11` §3.2 |
| `P-POT-HP-DEFICIT-MIN` (`potHpDeficitMin`) / solo (`potHpDeficitMinSolo`) | 0,9 / 0,7 | 0,6–1,1 | `docs/11` §3.2, `docs/14` |
| `P-POT-MP-DEFICIT-MIN` (`potMpDeficitMin`) | 0,95 | 0,6–1,1 | `docs/11` §3.3 |
| `P-WAR-MP-RESERVE` (`warMpReserve`) / `P-WAR-BURST-MP` / `P-WAR-FINISH-HP` | 700 / 1500 / 0,25 | 400–1200 / 1000–2500 `[A]` / 0,15–0,40 | `docs/06` §3 |
| `P-WAR-SLOW-TRIGGER` (`warSlowTrigger`) | 0,90 | 0,70–1,00 `[A]` | `docs/06` §3 |
| `P-WAR-CHASE-MAX-DIST` / `P-WAR-CHASE-MAX-TIME` | 35 m / 8000 ms | 20–60 / 4000–15000 | `docs/06` §3 |
| `P-WAR-STANDOFF-M` (`warStandOffM`) | 1,0 | 0,3–1,8 | **yeni** `[A]` (F6-03) |
| `P-WAR-NAV-HANDOFF-M` (`warNavHandoffM`) | 8,0 | 4–16 | **yeni** `[A]` (F6-03) |
| `P-WAR-SPRINT-MIN-DIST` (`warSprintMinDist`) | 15,0 | 8–30 | **yeni** `[A]` (F6-03) |
| `P-SK-RANGE-MARGIN` (`skRangeMargin`) | 1,5 m | 0,5–3,0 | `docs/05` SK-09 |
| `P-TGT-W-KILL/REACH/THREAT/DEBUFF/RISK` | 1,2 / 1,0 / 0,6 / 0,5 / 1,0 | 0,0–3,0 | `docs/09` §5.2 |
| `P-TGT-COMMIT-MIN` / `P-TGT-SWITCH-MARGIN` | 4000 ms / 0,30 | 2000–8000 / 0,10–0,60 | `docs/09`, `docs/14` |
| `P-TGT-REACH-TIME` / `P-TGT-REACH-SLACK` | 4000 ms / 3 m | — | `docs/09` |
| `P-TGT-REACH-HORIZON` (`tgtReachHorizonMs`) / `P-TGT-OWN-DPS` (`tgtOwnDps`) / `P-TGT-MAX-DIST` (`tgtMaxDistM`) | 20000 / 470 / 80 | — | **yeni** `[A]` (F6-02) |
| `P-SOLO-ENGAGE-EV` / `P-SOLO-DISENGAGE-EV` | 0,1 / −0,15 | — | `docs/10` §3.3 |
| `P-SOLO-CHASE-MAX` (m, ms) / `P-SOLO-OUTNUMBER` / `P-SOLO-RECOVER-HP` / `P-SOLO-RECOVER-MP` | 60 m, 10000 ms / 1 / 0,85 / 0,70 | — | `docs/10` |
| `P-PRI-HEAL-EMERG` / `P-PRI-HEAL-NORMAL` / `P-PRI-PREHEAL-K` / `P-PRI-OVERHEAL-MAX` / `P-PRI-MP-RESERVE` | 0,32 / 0,75 / 0,8 / 0,25 / 1100 | 0,20–0,45 / 0,6–0,85 / 0–1,5 / — / 800–2000 | `docs/07` §3 |
| `P-MAG-PREF-RANGE` (min, max) / `P-MAG-MELEE-DANGER` / `P-MAG-AOE-MIN` (r15, r8) / `P-MAG-MP-RESERVE` / `P-MAG-ELEM-SAMPLES` | 30–45 m / 18 m / 3, 2 / 500 / 3 | — | `docs/08` §4 |
| `P-ACT-LATENCY` | 250 ms | — | `docs/16` MET-ACT-01 |
| `P-TGT-ABANDON-HOLD-MS` (`tgtAbandonHoldMs`) | 10000 | 3000–30000 | **yeni** `[A]` (F6-02: bırakılan hedef bu süre yeniden seçilmez) |
| `P-SUR-RECOVER-MP-WAR` / `-PRI` / `-MAG` | 0,50 / 0,70 / 0,70 | 0,30–0,90 `[A]` | `docs/11` §5 (rol MP eşiği) |
| `P-SOLO-EV-A0..A6` (`soloEvA[7]`) | 0; 2,0; 1,5; 0,8; 1,2; 0,5; 0,6 | −3,0–3,0 | `docs/10` §3.3 (L1'e açık) |
| `P-SOLO-V-KILL` / `P-SOLO-C-DEATH` / `P-SOLO-C-TIME` / `P-SOLO-P-DEATH-LOSE` | 1,0 / 1,2 / 0,05 / 0,6 | 0–3 | `docs/10` §3.3 |
| `P-SOLO-EV-HYST` / `P-SOLO-ROLE-ADJ-MAGE` | 0,10 / +0,15 | — | `docs/10` §3.3, §4.2 |
| `P-SOLO-ROAM-R-M` (`soloRoamRadiusM`) | 35 | 20–55 | **yeni** `[A]` (F6-10: arena içi dolaşma halkası) |
| `P-SOLO-FORCE-ENGAGE` (`soloForceEngage`) / `P-SOLO-POT-REF` (`soloPotRef`) | 0 (kapalı) / 4 | 0–1 / 1–10 | **yeni** `[A]` (F6-10: `NO_ENGAGE` korumasıyla EVAL-1v1'de `AVOID` kapatma; `own_resource_score` pot referansı) |

Bu tablo F6-02..F6-10 planlarının parametrelerinin **tamamını** içerir (yazım turunda her planla çapraz kontrol edilir); sonraki planlar satır eklemez.

`role_adj` (mage +0,10; priest +0,05; W-G −0,05; W-P 0) ve `ttd < 2,5 sn` kodda **sabittir** (`docs/14` §4.3 mantığı: öğrenme yüzeyinde değil).

### 3.2 `B0-NAIVE` tanımı (bu planın kararı; `docs/15` §5 satırının işlenebilir hâli; **belirsiz noktalar `[A]` ve §8 "Çelişkiler" 3'te**)

| Konu | Kural |
|---|---|
| Hedef | `snap->enemies` içinde canlı (`!dead`), `invisibility == 0`, `posState != POS_LOST` olan **en yakın** (`dist` artan, eşitlikte küçük `id`) birim. Skor yok, bağlılık yok: her tick yeniden en yakın seçilir. |
| Hareket | Hedefe `MoveKind::FollowTarget` (halka: bot yakın dövüş sınıfıysa `[0, 2]`, caster ise `[skill menzilinin %80'i, menzil]`); nav yoksa (`serviceUp == false`) `Hold`. Güvenli nokta/geri çekilme **yok**. |
| Normal saldırı | Menzildeyse (`distanceField <= weaponRangeField`) ve `rWaitMs == 0` ise `attack.active = true` (yalnızca warrior ve silahlı priest; mage R atmaz). |
| Skill | Rol skill listesinden (`DecisionInput.skills`) **hazır** (`SkillReady().ready`), düşman hedefli (`Moral 7`) ve MP yeten skill'ler arasından tick başına en çok bir tanesi **tek biçimli rastgele** (`Rng::NextBelow`); hazır skill yoksa yok. Kendine/dosta skill (heal, buff) listeye **girmez**. |
| Pot | HP oranı < 0,30 ve `potWaitMs == 0` ve HP potu stoğu ≥ 1 ise HP potu; MP oranı < 0,30 ise MP potu (HP önce). Tek pot yuvası, ortak 2,5 sn (`kPotCooldownMs`). `[A]`: "pot %30'da" HP için okunur, MP simetrik eklenmiştir. |
| Geri çekilme / durum makinesi | **Yok**: durum yalnızca `Idle -> Prepare -> Roam -> Engage -> Combat` ve ölünce `Dead -> Respawned`; `Retreat`/`Recover` üretmez. |
| Adalet | Aynı `ActionExecutor` + `BotFairnessGuard` yolundan geçer (guard reddi saymaz); CLI/mekanik değişmez. |
| Belirlenim | Rastgelelik yalnızca verilen `Rng`'den; aynı `DecisionInput` dizisi + aynı tohum → aynı `Intent` dizisi. |
| Priest/mage | Rol skill listesi: priest → `Moral 7` olanlar (Judgment `112802`, Helis `112815`, Malice vb.); mage → `Moral 7` ve `Moral 10` (alan) skill'leri (alan: hedef noktası = hedef konumu). Heal/buff yok (naive). |

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Brain.h` | yeni | tipler, `StateMachine`, `RoleFamilyOfClass` |
| `BotCore/BrainParams.h` | yeni | `BrainParams`, `kParamTable`, `SetParam`, `ValidateAll` |
| `BotCore/SkillReady.h` | yeni | `SkillSpec`, `SkillReady` (guard fonksiyonlarını çağırır) |
| `BotCore/PolicyNaive.h` | yeni | `NaivePolicy` |
| `Tests/BotCoreTests/BrainTests.cpp` | yeni | §5.3 |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca dört `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca bir `ClCompile` satırı |

`GameServer/`, `shared/`, `docs/`, `tools/` ve diğer BotCore dosyaları **değişmez**. Listede olmayan dosya gerekirse **durup** Uygulayıcı Raporu'nda soru yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-01 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Başlamadan `./tools/run-tests.sh Release` ile mevcut test sayısını kaydet (beklenen 260 `[V: git grep -c '^TEST_CASE' gece/2026-10-02 @ 7891f74]`; F4-60 doğrulama kaydı da 260 yazar).
2. `Brain.h`, `BrainParams.h`, `SkillReady.h`, `PolicyNaive.h` yaz (§3). `SkillReady()` kendi menzil/MP/recast mantığı yazmaz; `BotCombat.h` fonksiyonlarını çağırır. Tick yolunda `std::vector`/`new` yok (sabit diziler; `StateMachine` olay halkası 16).
3. `BrainTests.cpp`: aşağıdaki test adları **sabittir** (sentetik zaman, sabit `Rng` tohumu):
   - `Brain_RoleFamily_ClassCodes`: 105/106/205/206 warrior, 109/110/209/210 mage, 111/112/211/212 priest, 102/107/108 `Other`.
   - `Brain_StateMachine_LegalTransitions`: `docs/13` §6'daki her ok `true`; F7'ye ayrılmış durumlara geçiş `false`; eklenen dört geçiş `true`; `Dead` her durumdan `force` ile.
   - `Brain_StateMachine_MinDwell`: `Retreat`'e girildikten sonra 2999 ms'de çıkış reddedilir, 3000 ms'de kabul; `force` yalnızca `Dead` için.
   - `Brain_StateMachine_OscillationCount`: 10 sn içinde `Combat->Retreat->Combat->Retreat` → `Oscillations == 3`; 10 sn dışına çıkan geçişler sayılmaz; olay halkası 16'yı aşınca en eskiyi atar, `PopEvent` sıralı.
   - `Brain_Params_Defaults_Match_Docs`: tablodaki her varsayılan değer yukarıdaki tabloyla birebir (`CHECK_EQ`).
   - `Brain_Params_Reject_OutOfRange`: `SetParam("P-SUR-RETREAT-HP", 0.45f)` → `OutOfRange` ve değer **değişmedi**; bilinmeyen kimlik → `UnknownId`; sınır değerleri (0,20 ve 0,40) kabul.
   - `Brain_Params_Reenter_Constraint`: `retreatHp = 0.40` iken `reenterHp = 0.60` → `ConstraintViolated`; `ValidateAll` varsayılan → `Ok`.
   - `Brain_SkillReady_Verdicts`: Carving (`Msp 90`, `Range 0`, silah menzili 20) için menzil dışı/MP yok/recast/tip kapısı/boşluk/hazır durumlarının her biri beklenen `CastVerdict` ve `waitMs` (örnek vektörler `BotCombat.h` testlerinden: `Combat_PartyCast_Guard` biçimi).
   - `Brain_Reason_Names_Unique_And_Closed`: her `ReasonCode` adı benzersiz, `docs/16` §5.2'deki 13 ad harfi harfine mevcut.
   - `Naive_Target_Nearest_Deterministic`: dört düşman (biri ölü, biri görünmez, biri `POS_LOST`) → en yakın uygun olan; eşit mesafede küçük `id`.
   - `Naive_NoRetreat_NoRecover`: HP %5 iken bile `Retreat`/`Recover` üretmez; `override_ == false`.
   - `Naive_Pot_At30Percent`: HP 0,31 → pot yok; 0,29 → HP potu; `potWaitMs > 0` → pot yok; stok 0 → pot yok; HP ve MP birlikte → HP önce.
   - `Naive_RandomSkill_ReadyOnly_Seeded`: aynı tohum aynı dizi; farklı tohum farklı dizi (64 tick); seçilen her skill `SkillReady().ready` ve `Moral 7`; hazır skill yokken `cast.active == false`.
   - `Naive_Roles_Covered`: warrior (R + Type1), priest (yalnızca `Moral 7` skill'ler, heal yok), mage (alan skill'inde hedef noktası = hedef konumu) için örnek `Intent`.
4. `BotCore.vcxproj` ve `BotCoreTests.vcxproj` satırlarını ekle (CRLF korunur).
5. Derleme ve test (§7); `Durum` → `UYGULANDI` ve Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok
- [ ] K2: `./tools/build.sh Debug` rc=0, yeni uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug` `0 failed`; on dört yeni test adı `[ OK ]`; mevcut testler değişmeden geçer (toplam = başlangıç + 14)
- [ ] K4: kural-tabanlı denetim: yeni başlıklarda `windows.h|stdafx|GameServer|shared/` yok; yeni kodda `new|malloc`, global/static **değişken** (`constexpr` sabit hariç) ve `rand(` yok; ASCII + CRLF (`file`)
- [ ] K5: `BrainParams` varsayılanları §3.1 tablosuyla **birebir** (test K3'te); aralık dışı değer reddedilir (AC-LRN-04)
- [ ] K6: `StateMachine` salınım sayacı `docs/16` MET-SUR-06 tanımıyla (10 sn, `Combat <-> Retreat`) uyumlu; `Retreat` minimum süresi `P-SUR-MIN-RETREAT-TIME`
- [ ] K7: belirlenim: aynı girdi + tohum → aynı `Intent` (test K3'te); `Intent`/`DecisionInput` POD ve kopyalanabilir
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F6-01` yalnızca §4'teki yedi dosya (+ plan dosyası); `GameServer/`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K9 (Claude): `docs/13` §6 diyagramıyla geçiş tablosunu çapraz kontrol eder; eksik dört geçiş ve yeni `ReasonCode` adları için docs güncellemesini yazar; `B0-NAIVE` belirsizlikleri için proje sahibine tek tek soru açar
- [ ] K10 (çalışma zamanı, bu planda yok): `B0-NAIVE`'in sunucuda koşusu F6-06/F6-10'dadır; bu plan yalnızca birim düzeyinde kapanır, `GELIŞTIRILDI` sayılmaz (`docs/17` §4: oyun içi kanıt F6-06'da)

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Brain_|Naive_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F6-01
grep -nE "windows\.h|stdafx|GameServer|shared/" BotCore/Brain.h BotCore/BrainParams.h BotCore/SkillReady.h BotCore/PolicyNaive.h
git diff gece/2026-10-02...bot/F6-01 -- BotCore | grep -nE "^\+.*(\bnew\b|malloc|rand\()"
git diff --check gece/2026-10-02...bot/F6-01
file BotCore/Brain.h BotCore/BrainParams.h BotCore/SkillReady.h BotCore/PolicyNaive.h
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (CRLF, tab, Allman, ASCII yeni dosyalar, yorumlar İngilizce). Oyun mekaniği ve guard kuralları **değişmez**; `BotCombat.h` yalnızca çağrılır.
- **Bota avantaj yok** (`docs/03` §13, `docs/13` §2-4, `docs/14` §5.2): `DecisionInput` yalnızca `PerceptionSnapshot` + kendi zamanlayıcıları + nav'dan beslenir; düşman MP/cooldown/envanteri alanı **yoktur**. `tools/check-perception-contract.py` R1-R5 yeni başlıklarda da PASS kalmalı (yazım turunda `BotCore/` taramasına yeni dosyaların girip girmediği doğrulanır).
- `B0-NAIVE` bir **alt sınır baseline'ıdır**, `baseline-v1` (L0) değildir; L0 politikası F6-03..F6-10'da kurulur ve F8'de dondurulur.
- Eşikler `[Ö]`/`[A]`'dır; `docs/` sahibi dokümandaki değer değişirse tablo birlikte güncellenir (Claude).
- Çelişkiler (docs ile docs arası ve docs ile kod arası, yazım turunda Claude çözer):
  1. `docs/13` §6 diyagramında `ENGAGE -> ROAM`, `COMBAT -> ROAM`, `ENGAGE -> RETREAT`, `COMBAT -> ENGAGE` yok; `docs/10` §2 `DUEL -> ROAM`, `docs/06` §7 `APPROACH`/`PRESSURE` kullanır.
  2. `docs/13` §6 durum adları ile `docs/10` §2 (`EVALUATE`, `AVOID`, `DUEL`, `CHASE`) ve `docs/06` §7 farklıdır: bu plan ortak durumu `docs/13`, ayrıntıyı `SubState` olarak tutar.
  3. `B0-NAIVE` (`docs/15` §5): "pot %30'da" HP mi MP mi; priest/mage için "en yakın düşmana saldırır" ve "rastgele hazır skill" kümesi tanımsız; `docs/17` F6 "EVAL-1v1'de anlamlı yener" ifadesindeki "anlamlı" (SPRT/Elo eşiği) tanımsız (`docs/15` §8 yalnızca 8v8 için `+100 Elo` SPRT verir).

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
