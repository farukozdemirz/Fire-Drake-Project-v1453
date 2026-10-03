# F7-02: İki priest görev koordinasyonu (`BotCore/PriestCoord.h`) ve heal-stall kararı (`BotCore/HealStall.h`), saf mantık

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F7 — Party koordinasyonu (`docs/17` §2 F7 bloğu; kapılar G7a ve G7c) |
| Branch | `bot/F7-02 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F7-01** (`ReservationTable`, `PendingHeals`, `QueryHealGate`/`HealGateDecide`, `HasForeignOpen`) · **F6-01** (`BotRole`, `BrainParams` `P-PRI-HEAL-EMERG`; karar katmanı iskeleti, `Rng`/saat sözleşmesi) · **F6-07** (`BotCore/PriestHeal.h`: `hp_pred`, `PickHeal`, MP rezervi; **bu plan onun "çift priest" hedef süzgecidir, skill seçimini ve `hp_pred`'i yeniden yazmaz**) · F4-53 `KAPANDI` (`HealObsRing`, `ObservedStatusTable`) · F4-60/F4-61 (sunucu bağlaması; çalışma zamanı girdisi için) · F4-51 `KAPANDI` (`HpTable`: düşman HP örnekleri) |
| İlgili gereksinim / kabul | REQ-PRI-06, REQ-PRI-02, REQ-PTY-11; AC-PRI-03/09, AC-PTY-03 (stall tespitinden karara p50 ≤ 3 sn: **çalışma zamanı**, F7-08), MET-HEAL-04, MET-STALL-01, MET-CURE-01 (cure çift atışı), T-PRI-03, T-PTY-03, EVAL-HEALSTALL |
| Tahmini büyüklük | M (üst sınır; 4 yeni dosya + 2 `.vcxproj`; yazım turunda **F7-02a iki priest** / **F7-02b heal-stall** olarak ikiye bölünebilir) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 kapanmadan HAZIR yapılmaz) |

---

## 0. Neden TASLAK

**HAZIR yapma ön koşulları:**

1. F7-01 `KAPANDI` (rezervasyon tablosu ve kapı bu planın girdisi).
2. F6-07 priest solo `KAPANDI`: `ratio`, `deficit`, skill seçimi (`docs/07` §5.2) ve MP rezervi orada tanımlanır; bu plan o fonksiyonların **sonucunu** (hangi hedefe heal) çift priest bağlamında süzer, skill seçimini yeniden yazmaz.
3. `docs/07` §6 "Hedef dağıtımı" cümlesindeki **"üyeye en yakın ve MP'si yüksek priest"** iki ölçütü nasıl birleştirdiğini söylemiyor. Bu taslak `[A]` bir birleştirme önerir (§3.1: mesafe farkı eşiği `kPrimaryDistMarginM`, sonra MP oranı, sonra slot kimliği). **Claude** bu kuralı `docs/07` §6'da netleştirmeden plan HAZIR olmaz (doküman işi; mekanik değil tasarım parametresi).
4. `docs/09` §6.1'deki `stall` tanımı hangi pencerede değerlendirileceğini söylemiyor ("kayan pencereler 5 sn ve 10 sn", `stall`: `net ≤ 0,1·dmg_rate` ve son 6 sn'de HP oranı %5'ten az düştü). Bu taslak `[A]`: **iki pencerede de** koşul sağlanırsa (yanlış alarmı azaltmak için). Doküman netleştirmesi gerekir.
5. Düşman healer'ın kimliği için `HealObs.caster` ve düşman sınıfı (`UnitView.cls`) F4-60/F4-61 sonrası çalışma zamanında doğrulanmış olmalı (hedefe heal atan düşman priest gözlenebiliyor mu?). Doğrulanmamışsa `heal_rate` yalnızca HP artışından türetilir ve `HEALER_SWITCH` kararı verilemez (yalnızca `SPLIT`/`BURST_NOW`): karar yazım turunda.
6. `HpTable`/`HpObs` (`BotCore/Perception.h:418-428`) tek son değeri tutar, **geçmiş tutmaz**; bu plan örnek geçmişini kendi sabit boyutlu halkasında tutar (`HpSampleRing`). F4-51'in "yoklama ≥ 2 sn" kuralı (CLI-10) ve `docs/09:107` "≥ 3 örnek, ≥ 2 gözlemci" örnek sıklığını belirler: yazım turunda gerçek örnek hızı (telemetri `snap hp`) ile tutarlılığı doğrulanır.

**Yazım turunda yeniden doğrulanacak referanslar** (okuma: `gece/2026-10-02` @ `f4daa27`):

- `docs/07_PRIEST_BEHAVIOR.md:52-63` (§4 öncelikler), `:103-115` (§6 iki priest tablosu), `:150-163` (§8 cure; cure rezervasyonu `:114`), `:117-148` (buff paylaşımı `:115`), `:225-245` (pseudocode `filter_unreserved`), `:211` (priest'ler ≥ 8 m ayrı: konum, F5).
- `docs/09_PARTY_COORDINATION_AND_TARGET_SELECTION.md:143-168` (§6 heal-stall: gözlem, karar tablosu, `[I]` heal kapasitesi, histerezis), `:107` (HP gözlemi seyrekliği ve stall örnek şartı), `:97` (300 ms pencerede çift karar), `:274-275` (AC-PTY-03).
- `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md:241` (MET-STALL-01), `:159` (MET-HEAL-04), `:167` (MET-CURE-01), `:238` (MET-CURE-02).
- `docs/15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md:196` (EVAL-HEALSTALL), `:219`, `:238-240` (T-IGT-PRI-01, T-IGT-PTY-01).
- `BotCore/Perception.h`: `HpObs`/`HpTable` (`:418-428`), `HealObs`/`HealObsRing` (`:2296-2307`), `UnitView` (`:1141`; `cls` alanı), `TeamMemberView` (`:1454`).
- Sınıf kodları: `docs/appendix/data/skills_summary_notes.md:9` (MagicNum/1000 = tam sınıf kodu: priest 104/111/112, 204/211/212; `User.h` `ClassType`).

## 1. Amaç

(a) Party'deki **iki priest'in** aynı üyeye çift heal/cure atmasını engelleyen belirlenimli görev paylaşımı: "birincil healer" atama, acil heal hedef dağıtımı (iki bot **aynı paylaşılan görüntüden aynı sonucu** hesaplar), cure rezervasyon kapısı, rol bazlı görev önceliği, tek priest kalınca devralma ve P-HB ölünce `BUFF_COVERAGE_LOST`. (b) **Heal-stall** (`docs/09` §6): ortak hedefin heal ile ayakta tutulduğunu hasar/heal pencerelerinden tespit eden ve `HEALER_SWITCH` / `SPLIT` / `BURST_NOW` / iki-healer / kayıpta-değişim-yok kararını histerezisle veren durum makinesi.

## 2. Bağlam (okunması zorunlu)

- `docs/07` §4-§6, §8, §14 (öncelikler, iki priest, cure, `filter_unreserved`) ve `docs/09` §6 (stall + karar tablosu); AC-PRI-03/09, AC-PTY-03, MET-HEAL-04, MET-STALL-01.
- F7-01 `BotCore/TeamBlackboard.h`: `ReservationTable::HasForeignOpen`, `PendingHeals`, `QueryHealGate`, `HealGateDecide`; F6-07 `BotCore/PriestHeal.h` `hp_pred` (tek kaynak; ad/imza F6-07'den doğrulanır) ve `BrainParams` (`emerg`, `preHealK`).
- `docs/09` §4.1/§4.3 (adalet ve 300 ms gecikme): birincil healer kuralı ve slot kimliği eşitlik bozma 300 ms pencerede çift karar riskinin tek çaresidir (`docs/09:97`).
- `docs/03` §16 (`:537-560`): düşmanın MP'si/cooldown'ı/envanteri bota gönderilmez ⇒ **bu plan hiçbir yerde düşman healer'ın MP'sini kullanmaz** (`docs/09` §6.2 "Sweep mana" fırsatı yalnızca gözlenen heal yoğunluğundan; MP okunmaz).

## 3. Kapsam

**Yapılacaklar** (iki yeni başlık; yalnızca standart kütüphane + `TeamBlackboard.h`/`Perception.h` include'ları; sunucu başlığı/global-static yok; sabit boyutlu diziler; ASCII + CRLF)

### 3.1 `BotCore/PriestCoord.h` (F7-02a)

1. **Sabitler** (`[Ö]`/`[A]`, kaynak yorumlu): `kPrimaryDistMarginM = 5.0f` (`[A]`, `docs/07:112` iki ölçütün birleşimi), `kMaxPriests = 2` (REQ-PTY-02), `kMaxEmergencyCandidates = 8`.
2. **Türler:** rol olarak F6-01 `BotRole` kullanılır (`BotRole::PriestHealDebuff` = P-HD, `BotRole::PriestHealBuff` = P-HB; **ikinci bir rol enum'u tanımlanmaz**); `struct PriestInfo { int16_t id; uint8_t slot; BotRole role; bool alive, retreating, casting; float x, z; int32_t mp, maxMp; }` (`MemberStatus` F7-05'te eklenene kadar çağıranın doldurduğu değer kopyası); `struct HealCandidate { int16_t id; int32_t hp, maxHp; float incomingPerSec; float x, z; }`.
3. **`int16_t ChoosePrimaryHealer(const PriestInfo * p, int n, float memberX, float memberZ, float healRangeM)`:** yalnızca `alive && !retreating` ve üyeye mesafe ≤ `healRangeM` olanlar aday; tek aday ⇒ o; iki aday: mesafe farkı `> kPrimaryDistMarginM` ise yakın olan; değilse `mp/maxMp` oranı yüksek olan; oran eşitse **düşük `slot`** (belirlenimli); aday yoksa `-1`.
4. **`struct HealAssignment { int16_t priest; int16_t target; int why; }` ve `int AssignEmergencyHeals(const PriestInfo * p, int n, const HealCandidate * c, int m, const ReservationTable & table, uint64_t nowMs, uint32_t castMs, const BrainParams & prm, float healRangeM, HealAssignment * out)`:** (a) her aday için `QueryHealGate().pendingTotal` değerini **F6-07 `hp_pred`'ine** geçirip `ratioAfterPending = hp_pred / maxHp` hesaplanır (**ikinci bir `hp_pred` yoktur**); `ratioAfterPending < prm.priHealEmerg` (alan adı F6-01'den doğrulanır) olmayan aday **acil değildir** (atanmaz); (b) acil adaylar `ratioAfterPending` artan, eşitse düşük `id` ile sıralanır; (c) sırayla her aday için önce `ChoosePrimaryHealer` ile birincil priest seçilir, **serbestse** (`!casting`, bu çağrıda atanmamış, `table`'da aynı hedefe kendi açık heal kaydı yok, `HealGateDecide(...)` izin veriyor) ona atanır; birincil meşgulse **diğer priest** (aday ve serbest ise) alır (`docs/07:112`: "birincil healer'ın bir rezervasyonu varken diğeri ikinci acil durumu alır"); (d) bir priest bir çağrıda en çok bir hedefe atanır; (e) çıktı sıralaması priest `slot` artan. **Aynı girdiyle her iki bot da aynı `out` dizisini hesaplar**; kendi payını `out`'tan `priest == self` ile okur (dağıtık ama belirlenimli). Dönüş: atama sayısı.
5. **Cure kapısı:** `bool CureGateAllows(const ReservationTable &, int16_t target, int16_t self, uint64_t nowMs)` = `!table.HasForeignOpen(RES_CURE, target, self, nowMs)` (`docs/07:114`: iki priest aynı üyeye aynı anda cure atmaz).
6. **Görev önceliği:** `enum PriestTask : uint8_t { TASK_EMERG_HEAL = 1, TASK_GROUP_HEAL, TASK_CURE_CRITICAL, TASK_NORMAL_HEAL, TASK_RES, TASK_DEBUFF, TASK_BUFF, TASK_POSITION }`; `int PriestTaskOrder(BotRole role, bool otherPriestAlive, PriestTask * out)` doldurur ve sayıyı döner: iki priest canlıyken P-HB `[EMERG_HEAL, BUFF, GROUP_HEAL, CURE_CRITICAL, NORMAL_HEAL, POSITION]` (`docs/07:111` "buff kapsaması birincil, heal ikincil; acil heal ikisinin de birincisi"), P-HD `[EMERG_HEAL, RES, DEBUFF, GROUP_HEAL, CURE_CRITICAL, NORMAL_HEAL, POSITION]`; **tek priest kalınca** (`otherPriestAlive == false`) devralma sırası `[EMERG_HEAL, CURE_CRITICAL, GROUP_HEAL, RES, BUFF, DEBUFF, NORMAL_HEAL, POSITION]` (`docs/07:113`), rolün ağacında olmayan görev (HD'de `TASK_BUFF`, HB'de `TASK_RES`/`TASK_DEBUFF`) **listeden çıkarılır** (`docs/04` §5.3-§5.4, `docs/07:20,115`).
7. **`bool BuffCoverageLost(const PriestInfo * p, int n)`** = canlı P-HB yok (`docs/07:115`; telemetri `BUFF_COVERAGE_LOST` bağlamanın işi). Diğer priest'in birbirinden ≥ 8 m konumu (`docs/07:211`) **bu planda yok** (konum/F5 formasyon).

### 3.2 `BotCore/HealStall.h` (F7-02b)

1. **Sabitler** (`[Ö]`): `kStallWinShortMs = 5000`, `kStallWinLongMs = 10000` (`docs/09:147`), `kStallFlatRatio = 0.1f` (`net ≤ 0,1·dmg`), `kStallHpDropMaxPct = 5` ve `kStallHpDropWinMs = 6000` (`docs/09:152`), `kStallMinSamples = 3`, `kStallMinObservers = 2` (`docs/09:107`), `kHealerSwitchHystMs = 6000` (`docs/09:168`), `kHealerUnreachMs = 3000` (`R < 0,25`, 3 sn), `kReachSwitchMin = 0.5f`, `kReachSwitchMinTwoHealers = 0.6f`, `kReachSplitMax = 0.5f` (`docs/09:160-162`), `kHealerMeleeRadiusM = 8.0f`.
2. **`class HpSampleRing`** (sabit 32 örnek, kopyalanabilir): `Add(uint64_t tMs, int32_t hp, int32_t maxHp, int16_t observer)`; `int CountIn(uint64_t nowMs, uint32_t winMs)`; `int DistinctObserversIn(nowMs, winMs)`; `bool Delta(uint64_t nowMs, uint32_t winMs, int32_t & dHp, float & hpRatioStart, float & hpRatioEnd)` (pencerenin ilk ve son örneği; en az 2 örnek yoksa `false`). Zaman geri gidiyorsa örnek yok sayılır.
3. **`struct StallMetrics { bool enough; float dmgRate, healRate, net; bool flat; float hpDropPct; }` ve `StallMetrics ComputeStall(const HpSampleRing &, const HealObsRing &, int16_t target, uint64_t nowMs)`:** her iki pencere (5 ve 10 sn) için: örnek yoksa `enough = false` (`stall` **verilmez**, `docs/09:107`); `knownHeal = HealObsRing::SumNominal(target, now, W)`; `dmgRate = max(0, −ΔHP + knownHeal) / W`; `healRate = knownHeal / W`; `net = dmgRate − healRate`; `flat = net ≤ kStallFlatRatio · dmgRate` (**her iki pencerede** `[A]`); ayrıca son 6 sn'de HP oranı düşüşü `< %5` (`hpDropPct`); `stall = enough && flat(5) && flat(10) && hpDrop < 5`. **Bilinen yan etki `[A]`:** `knownHeal` nominaldir, hedef maksimum HP'ye yakınken etkili heal'den büyüktür ⇒ `dmgRate` fazla tahmin edilir; hata çalışma zamanında ölçülür (T-PTY-03).
4. **`struct StallInputs`** (çağıranın doldurduğu değer kopyası): `target`, `targetTtkSec`, `enemyHealerCount`, `healer` (`id`, `reach` R 0-1, `meleeNear` = healer'a ≤ `kHealerMeleeRadiusM` düşman melee sayısı, `ttkSec`), `secondHealerReach`, `ownAlive`, `enemyAlive`, `ownHpRatioSum`, `burstReady` (P-HD debuff + ≥ 2 mage patlama + W-P MP ≥ BURST; çağıranın hesabı), `reachOldTarget`.
5. **`enum StallDecision { STALL_NONE = 0, STALL_HEALER_SWITCH, STALL_SPLIT, STALL_BURST_NOW, STALL_BURST_PARASITE, STALL_REVERT, STALL_HOLD_LOSING }`** ve **`class StallDecider`** (`Reset()`, `StallDecision Update(const StallMetrics &, const StallInputs &, uint64_t nowMs)`; üyeler: `stallSinceMs`, `lastSwitchMs`, `switchedTarget`, `unreachSinceMs`; `uint64_t DecisionLatencyMs() const` = karar anı − `stallSinceMs`, MET-STALL-01/AC-PTY-03 için): karar tablosu (`docs/09:157-164`) sırasıyla: (i) kendi takım kayıpta (canlı < düşman **ve** toplam HP oranı < 0,4) ⇒ `STALL_HOLD_LOSING` (hedef değişimi yok; geri çekilme karar katmanı); (ii) `stall` yoksa `STALL_NONE`; (iii) tek düşman healer, `reach ≥ kReachSwitchMin`, `meleeNear == 0`, `healer.ttkSec < targetTtkSec` ⇒ `STALL_HEALER_SWITCH`; (iv) healer korunuyor (`meleeNear ≥ 1`) veya `reach < kReachSplitMax` ⇒ `STALL_SPLIT` (bir W-P healer'a baskı, takım hedefte kalır ve senkron patlama bekler); (v) iki düşman healer: `reach ≥ kReachSwitchMinTwoHealers` ⇒ `STALL_HEALER_SWITCH`, değilse `STALL_BURST_PARASITE`; (vi) `burstReady` ise ve (iii)/(v) switch yoksa `STALL_BURST_NOW`; **histerezis:** `STALL_HEALER_SWITCH` sonrası `kHealerSwitchHystMs` boyunca yeni healer kararı verilmez (`STALL_NONE`); **geri dönüş:** healer `reach < 0,25` ve `kHealerUnreachMs` sürerse `STALL_REVERT` (eski hedefe dön; yalnızca `switchedTarget` varken). Her çağrı belirlenimlidir (rastgelelik yok).
6. Bu plan karar **verir**, uygulamaz: `HEALER_SWITCH` sonrası rol bazlı emirler (W-P Scream/Shock Stun, P-HD Malice, mage patlaması), `TargetCall`/chat ve `TeamPlan.focus` güncellemesi **F7-04**'tür; hedef skoru/override altyapısı **F7-03**'tür.

### 3.3 Birim testleri (adlar bağlayıcı)

**`PriestCoordTests.cpp`** (kimlikler `kP1 = 2980` slot 0 P-HD, `kP2 = 2981` slot 1 P-HB; üye `kM1 = 2990`, `kM2 = 2991`, `kM3 = 2992`; `maxHp = 3000`, `castMs = 1500`, `healRangeM = 56`):

1. `PrimaryHealer_NearestWins`: P1 10 m, P2 30 m (fark 20 > 5) ⇒ P1.
2. `PrimaryHealer_WithinMargin_HigherMpWins`: 10 m ve 12 m, MP oranı 0,4 / 0,9 ⇒ P2.
3. `PrimaryHealer_TieBreaksBySlot`: aynı mesafe ve MP ⇒ slot 0.
4. `PrimaryHealer_DeadRetreatingOutOfRangeExcluded` (üç ayrı kontrol; hiç aday yok ⇒ `-1`).
5. `AssignHeals_OneEmergency_OnlyPrimaryAssigned`: yalnızca bir üye acil ⇒ bir atama, birincil priest'e; diğer priest atanmaz (**çift heal yok**, AC-PRI-03).
6. `AssignHeals_TwoEmergencies_DistinctPriests`: iki acil üye ⇒ iki farklı priest, her biri farklı hedef; en acil üye kendi birincil healer'ına.
7. `AssignHeals_PrimaryBusy_SecondTakesNextEmergency`: P1 `casting == true` ⇒ en acil üye P2'ye gider (`docs/07:112`).
8. `AssignHeals_ForeignPendingKeepsAboveEmerg_NotAssigned`: üyede yabancı bekleyen 960 heal (F7-01 tablosu) ve `hp_pred` oranı ≥ 0,32 ⇒ aday acil değil, atama yok.
9. `AssignHeals_ForeignPendingStillBelowEmerg_SecondAssigned`: aynı üye hâlâ < 0,32 ⇒ ikinci priest'e atanır (`docs/07:109`).
10. `AssignHeals_IdenticalResultFromBothViewpoints`: aynı girdi dizisi iki kez (priest dizisi ters sırada verilerek) ⇒ aynı `out` (belirlenim).
11. `AssignHeals_OnlyOnePriestAlive_TakesAll` ve `AssignHeals_NoPriestAlive_ReturnsZero`.
12. `CureGate_ForeignCureOpen_Denied`, `CureGate_NoForeign_Allowed`, `CureGate_OwnCureOnly_Allowed`, `CureGate_ForeignCureAfterEnd_StillDenied_UntilExpired`.
13. `TaskOrder_HD_TwoPriests`, `TaskOrder_HB_TwoPriests` (HB'de `TASK_RES`/`TASK_DEBUFF` yok; HD'de `TASK_BUFF` yok), `TaskOrder_SoloTakeover_OrderMatchesDoc` (sıra tam `docs/07:113`).
14. `BuffCoverageLost_WhenNoAliveHB`, `BuffCoverageLost_FalseWhenHBAlive`.

**`HealStallTests.cpp`** (`maxHp = 5650`, hedef `kE = 3010`; düşman healer `kH = 3011`; yardımcı `MakeSamples`):

15. `HpSampleRing_CountsAndObservers`: örnek sayısı/gözlemci sayısı pencereye göre; zaman geri giderse örnek yok sayılır; 32'den fazla örnekte taşmaz.
16. `Stall_NoSamples_NoDecision` (`< 3` örnek ⇒ `enough == false`, `stall == false`); `Stall_SingleObserver_NoDecision` (3 örnek, tek gözlemci; `docs/09:107`).
17. `Stall_Detected_WhenHpFlatAndHealing`: örnekler `t = 0/2000/4000/6000 ms`, HP `4000/3990/4005/3995` (iki gözlemci), `HealObsRing`'e 6 sn'de 3 × nominal 1920 ⇒ `dmgRate ≈ 960,8`, `net ≈ 0,8` ⇒ `stall == true`.
18. `Stall_NotDetected_WhenHpDropping`: aynı heal, HP `4000 → 3000` (6 sn) ⇒ `net ≈ 166,7 > 0,1·dmg (≈ 112,7)` ⇒ `stall == false`.
19. `Stall_HpDropOver5Percent_NotStall`: net düz ama son 6 sn'de HP oranı %6 düşmüş ⇒ `false`.
20. `Stall_RequiresBothWindows`: 5 sn penceresinde düz, 10 sn penceresinde değil (veya tersi) ⇒ `false` (`[A]` kuralı sınanır).
21. `Stall_HealRateFromRingOnlyForTarget`: başka hedefe heal sayılmaz (`HealObsRing::SumNominal(target …)`).
22. `Decision_HealerSwitch_WhenConditionsMet`: tek healer, `reach 0,8`, `meleeNear 0`, `healer.ttk < target.ttk` ⇒ `STALL_HEALER_SWITCH`.
23. `Decision_NoSwitch_WhenHealerTtkNotShorter` (healer TTK ≥ hedef TTK ⇒ `STALL_SPLIT` veya `STALL_BURST_NOW`, tablo sırasına göre).
24. `Decision_Split_WhenHealerProtected` (`meleeNear 1`) ve `Decision_Split_WhenHealerReachBelowHalf` (`reach 0,4`).
25. `Decision_TwoHealers_SwitchOnlyWhenReachAtLeast06` (`0,6` ⇒ switch; `0,55` ⇒ `STALL_BURST_PARASITE`).
26. `Decision_BurstNow_WhenReady` (`burstReady` ve switch koşulu yok ⇒ `STALL_BURST_NOW`).
27. `Decision_TeamLosing_NoChange`: canlı 3 < düşman 5 ve HP toplam oranı 0,3 ⇒ `STALL_HOLD_LOSING` (healer switch koşulları sağlansa bile).
28. `Decision_Hysteresis_NoNewHealerDecisionFor6s`: switch sonrası `5999` ms'de `STALL_NONE`, `6000`'de yeniden karar verebilir.
29. `Decision_Revert_WhenHealerUnreachable3s`: `reach 0,2` 2999 ms ⇒ `STALL_NONE`; 3000 ms ⇒ `STALL_REVERT`; `switchedTarget` yokken `STALL_REVERT` verilmez.
30. `Decision_LatencyIsStallSinceToDecision` (`DecisionLatencyMs()` MET-STALL-01 için) ve `Decision_Determinism` (aynı girdi dizisi ⇒ aynı karar dizisi; `Reset()` ilk durum).

**Kapsam dışı (yapılmayacak)**

- Sunucu bağlaması (`BotSession`'dan `HpSampleRing`/`HealObsRing` beslemesi, `TeamBlackboard`'a `EnemyIntel` yazımı, telemetri `STALL_DETECT`/`STALL_DECISION` olayları), çalışma zamanı ölçümü (AC-PTY-03, T-PTY-03, EVAL-HEALSTALL): **F7-08**.
- Hedef skoru, bağlılık, override altyapısı (`HEALER_SWITCH` override'ının **uygulanması**): F7-03; rol emirleri/çağrı/chat: F7-04.
- Skill seçimi, MP rezervi, overheal kuralı (`docs/07` §5.2): F6-07. Buff/direnç/cure skorlama (`docs/07` §7-§8): F7-06; diriltme (`docs/07` §10): F7-07. Konum (≥ 8 m, arka hat): F5/F7 formasyon planı.
- Düşmanın MP'si/cooldown'ı kullanımı (yasak, `docs/03` §16); düşman healer'ı "MP'si düşük" diye seçme.
- `docs/` değişikliği (Claude), `GameServer/`, `shared/`.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/PriestCoord.h` | yeni | §3.1 |
| `BotCore/HealStall.h` | yeni | §3.2 |
| `Tests/BotCoreTests/PriestCoordTests.cpp` | yeni | §3.3 testler 1-14 |
| `Tests/BotCoreTests/HealStallTests.cpp` | yeni | §3.3 testler 15-30 |
| `BotCore/BotCore.vcxproj` | değiştir | iki `<ClInclude>` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | iki `<ClCompile>` satırı |

`BotCore/TeamBlackboard.h` ve `BotCore/Perception.h` **değişmez** (yalnızca include edilir). Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F7-02 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; sunucuları durdur; test sayısını not et.
2. `PriestCoord.h` (§3.1) ve testleri 1-14; derle/çalıştır.
3. `HealStall.h` (§3.2) ve testleri 15-30; derle/çalıştır.
4. İki `.vcxproj` kaydı (`PriestCoord.h`, `HealStall.h`, iki test dosyası).
5. Derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; test sayısı plan başındakinden **§3.3'teki `TEST_CASE` sayısı kadar** fazla (≈ 40; yazım turunda kesinleştirilir); §3.3'teki her ad `[ OK ]`
- [ ] K3 (çift heal engeli, birim düzeyi): `AssignHeals_OneEmergency_OnlyPrimaryAssigned`, `AssignHeals_TwoEmergencies_DistinctPriests`, `AssignHeals_ForeignPendingKeepsAboveEmerg_NotAssigned` ve `AssignHeals_IdenticalResultFromBothViewpoints` geçer; **MET-HEAL-04 ≤ %5 çalışma zamanında F7-08'de ölçülür**, bu planda kanıt değildir
- [ ] K4 (stall, birim düzeyi): `Stall_Detected_WhenHpFlatAndHealing` `net ≈ 0,8` ve `stall == true`; `Stall_NotDetected_WhenHpDropping` `false`; örnek şartı (`< 3` örnek veya `< 2` gözlemci) ⇒ karar yok; **AC-PTY-03 (p50 ≤ 3 sn) ve MET-STALL-01 çalışma zamanında F7-08'de ölçülür**
- [ ] K5 (saflık): yeni başlıklarda yalnızca standart kütüphane ile `TeamBlackboard.h`/`Perception.h` include'u; `GameServer|windows.h|stdafx|shared/` yok; `new `/`malloc`/`std::vector`/`std::map`/`printf`/`rand(` yok; global/static değişken yok
- [ ] K6 (adalet): eklenen satırlarda düşman MP/cooldown/envanter okuyan alan yok (`grep -nE 'mp|cooldown|inventory' BotCore/HealStall.h` yalnızca `PriestInfo.mp` (kendi takım) ve yorumlarda); `StallInputs` yalnızca gözlem sözleşmesi alanları içerir
- [ ] K7 (biçim ve kapsam): ASCII + CRLF; sekme/Allman; `git diff --check` boş; `git diff --stat gece/2026-10-02...bot/F7-02` yalnızca §4'teki 6 dosya + plan dosyası; `GameServer/`, `shared/`, `docs/`, `BotCore/TeamBlackboard.h`, `BotCore/Perception.h` farkı 0
- [ ] K8 (belgeleme): `kPrimaryDistMarginM` ve "iki pencerede de düz" kuralı kodda `[A]` yorumuyla işaretli; Uygulayıcı Raporu bu iki kuralı açık sorular olarak listeler

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "PrimaryHealer_|AssignHeals_|CureGate_|TaskOrder_|BuffCoverageLost_|HpSampleRing_|Stall_|Decision_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F7-02
git diff gece/2026-10-02...bot/F7-02 -- BotCore/TeamBlackboard.h BotCore/Perception.h | wc -l   # 0
file BotCore/PriestCoord.h BotCore/HealStall.h Tests/BotCoreTests/PriestCoordTests.cpp Tests/BotCoreTests/HealStallTests.cpp
git diff --check gece/2026-10-02...bot/F7-02
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; §2.3 (oyun mekaniğini değiştirme: bu plan yalnızca karar mantığıdır); §2.5 (bota avantaj yok: yalnızca gözlenebilir girdi).
- Tüm eşikler `[Ö]`, iki kural `[A]` (§0 madde 3-4); T-PRI-03/T-PTY-03 sonrası güncellenir ve `docs/07`/`docs/09` ile birlikte değişir.
- **Dürüstlük:** bu plan uygulandığında "iki priest çift heal yapmıyor" **kanıtlanmış olmaz**; yalnızca karar fonksiyonlarının birim düzeyinde doğru olduğu kanıtlanır. Oyun içi kanıt F7-08 + T-IGT-PRI-01'dir.
- Plan 1 günden büyükse (testler dahil ≈ 40 vaka) **F7-02a** (§3.1, testler 1-14) ve **F7-02b** (§3.2, testler 15-30) olarak bölünür; bölme yazım turunda kararlaştırılır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: (boş)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — (boş)
