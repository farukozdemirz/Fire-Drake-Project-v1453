# F7-06: Priest destek kararları: buff matrisi ve takibi, direnç planı, cure skorlama ve rezervasyon kapısı (`BotCore/PriestBuff.h`, `PriestCure.h`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F7 — Party koordinasyonu (`docs/17` §2 F7 bloğu; kapı G7a) |
| Branch | `bot/F7-06 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-09** (priest buff/cure/debuff, kendine/tek müttefik; **bu an taslağı yok**, `F6-07` §8 "Çelişkiler" 1: "buff/cure/debuff döngüleri (kendine/tek müttefik)" F6-09'dur) · **F6-07** (priest solo heal/pot/MP rezervi) · **F6-01** (`BotRole`, `BotState`, `BrainParams`) · **F7-01** (`ReservationTable`: `RES_CURE`) · **F7-02** (`CureGateAllows`, `PriestTaskOrder`, `BuffCoverageLost`) · **F7-05** (`MemberStatus`, `BotState::Reintegrate` buff önceliği) · F4-28/F4-31/F4-32 `KAPANDI` (Type4 buff, `Moral` 4/6 party buff, cure atılabilir) · F4-42/F4-43 `KAPANDI` (priest buff/cure skill ölçümü, `docs/05` §9.1-§9.2) · F4-53 `KAPANDI` + **F4-60/F4-61** (gözlenen buff/debuff: bu planın girdisi) |
| İlgili gereksinim / kabul | REQ-PRI-03, REQ-PRI-04, REQ-PRI-06 (cure/buff çift atışı); AC-PRI-04 (kapsama ≥ %90, MET-BUFF-03 = 0: **çalışma zamanı F7-08**), AC-PRI-05 (MET-CURE-01 p95 ≤ 3 sn: **çalışma zamanı**), T-PRI-04, T-PRI-05, T-MECH-BUF-01..08 (BuffType çakışmaları; `docs/05` §4); `docs/07` §7, §8, §6 (cure rezervasyonu, buff paylaşımı) |
| Tahmini büyüklük | M (2 yeni başlık + 2 yeni test dosyası + 2 `.vcxproj` = 6 dosya; ≈ 45 test; büyürse **F7-06a buff/direnç** / **F7-06b cure** olarak bölünür) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 kapanmadan HAZIR yapılmaz) |

---

## 0. Neden TASLAK

**HAZIR yapma ön koşulları:**

1. F7-01, F7-02, F7-05 `KAPANDI`; **F6-09** priest solo/self buff ve cure döngüsünü yazmışsa (buff matrisinin tek hedefli hâli, `Cure curse` seçimi) bu plan onu **takım bağlamına genişletir** (çok üye, rezervasyon, `reintegrating` önceliği, direnç planı); tek kaynak: çakışırsa F6-09 kazanır. F6-09 yoksa bu plan tanımlar ve F6-09 kullanır (sıra yazım turunda netleşir).
2. **`docs/07` §7.1 matrisi BUF-HP-01 ile çelişiyor (Claude çözmeli):** matris priest için HP buff'ı olarak massiveness (+1500) der (`docs/07:125`), ama `BUF-HP-01` `argmax(maksHP_buffsuz · 0,6; 1500)` (`docs/07:127`) priest (S1 azami HP 3491 ⇒ 0,6 × 3491 = 2095 > 1500; ekipmansız ~2636 ⇒ 1582 > 1500) için **Undying**'i seçtirir. Warrior (5650 ⇒ 3390) ve mage (M-F 1541 ⇒ 925, M-I 2228 ⇒ 1337) için formül ve matris uyuşur. Yüzde buff'ın hangi HP bileşenine uygulandığı `[A]` (T-MECH-BUF-03, henüz ölçülmedi). Bu taslak matrisi **harfiyen** (priest massiveness) uygular; Claude matris/formül çelişkisini T-MECH-BUF-03'e göre kapatmadan plan HAZIR olmaz.
3. **`docs/07` §8 cure skorunun sayısal ağırlığı yok:** "skor = Σ debuff ağırlıkları × rol ağırlığı × aciliyet"; yalnızca mage ×1,3 verilmiş. Bu taslak `[A]`: Kritik = 3, Yüksek = 2, Orta = 1; rol ağırlığı mage 1,3 / diğerleri 1,0; aciliyet 1,0 (üye HP oranı < 0,5 ise 1,5). Claude değerleri `docs/07` §8'e yazmalı.
4. **DoT takibi yok (Cure disease):** F4-53 Type3 DoT/HoT hedef kaydını **kapsam dışı** bıraktı (`plans/F4-53-…` §3); `P-PRI-CURE-DOT-MIN` (800 HP kalan DoT toplamı) için gözlemlenebilir bir kaynak yok. Bu planın `dotRemaining` girdisi çağıranın değeridir; kaynak tanımlanana kadar sunucu bağlamasında `0` verilir ve **Cure disease kolu devre dışı kalır**. Yazım turunda ya DoT izleyicisi ayrı küçük dilim (F4-6x) olarak eklenir ya da kol kapsam dışı bırakılır.
5. **Gözlenen debuff/buff'ın gerçekten beslenmesi:** `acRemainingMs`/`hpRemainingMs`/`resRemainingMs` ve `*Debuffed` bayrakları F4-60/F4-61 çalışma zamanında doğrulanmış `ObservedStatusTable` verisidir (kaynak sınıfı `E`: bitiş tahmindir; `MAGIC_DURATION_EXPIRED` başkalarına gitmez, MEC-BUF-10). Tahmin hata payı ölçülmeden "buff aktif" kararı sınırlıdır.
6. **Direnç planı skill verisi (doğrulandı, `docs/appendix/data/skills_mage.csv`):** `110548` Immunity fire (ağaç 5, puan 48, `Moral` 2, BuffType 8, **süre 300 sn**), `110648` Immunity cold (ağaç 6, puan 48), `110748` Immunity lightning (ağaç 7, puan 48). M-F `[70,52,0,20]` ve M-I `[52,70,0,20]` profillerinde **ağaç 7 = 0 puan** ⇒ yıldırım direnci **hiçbir referans mage'le atılamaz**; ateş (48 ≤ 52) ve soğuk (48 ≤ 52) **ikisi de iki mage'in de erişiminde** (`docs/07:142` "M-F ateş, M-I buz" ifadesinden geniştir). Bu plan veriye göre yazılır; Claude `docs/07` §7.3'ü güncellemeli. Priest direnç buff'ı Fresh mind `112645` süresi 600 sn. **Mage'in direnç buff'ını atma davranışı (`docs/08` §5 madde 7) bu planda yok** (yalnızca takım kararı `ResistPlan`); mage tarafı F6-08/ayrı küçük plan.
7. P-HB'nin birincil görevi buff kapsaması (`docs/07:111`); `BUFF_COVERAGE_LOST` olayı F7-02 `BuffCoverageLost` ile tanımlı.

**Yazım turunda yeniden doğrulanacak referanslar** (okuma: `gece/2026-10-02` @ `f4daa27`):

- `docs/07_PRIEST_BEHAVIOR.md:117-148` (§7: matris `:121-125`, BUF-HP-01 `:127`, yenileme `:129-134`, direnç `:136-143`, tekrar önleme `:145-148`), `:150-163` (§8 cure tablosu, `:160-161` çoklu debuff, `:163` MET-CURE-01), `:41-49` (`P-PRI-MP-RESERVE`, `P-PRI-BUFF-REFRESH` 20 sn, `P-PRI-CURE-DOT-MIN` 800), `:254` (buff reddedildi ⇒ 30 sn tekrar yok), `:114-115` (cure rezervasyonu, buff paylaşımı), `:225-245` (pseudocode: `best_cure_candidate`, `next_missing_buff`), `:264-281` (T-PRI-04/05, AC-PRI-04/05).
- `docs/05_SKILL_CATALOG_AND_COMBAT_RULES.md:43-57` (§4 BuffType çakışma grupları: AC 2, HP_MP 1, RESISTANCES 8, ATTACK_SPEED 5, SPEED 6, DAMAGE 4; MEC-BUF-01..03), `:129-133` (Insensibility peel `112660`, massiveness `112657`, Undying `112654`, Greatness `112656`, Fresh mind `112645`), `:120-121` (Cure curse `112525`, Cure disease `112535`), `:188-226` (F4-42/F4-43 ölçümleri; `112656` aynı BuffType'ta sessiz atlama).
- `docs/03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md:192-201` (MEC-BUF-01..10), `:138` (MEC-MAG-18: `Moral` 4/6 party buff), `:139` (MEC-MAG-19: Type5 cure sonuç sözleşmesi).
- `docs/08_MAGE_BEHAVIOR.md:147-150` (§8.3: gelen üyeye buff matrisi önceliği), `docs/11` (RECOVER).
- `BotCore/Perception.h`: `ObservedStatusTable` (`:2014`), `StatusObs`, `SkillIsCureDebuff` (`:1962`); `BotCore/BotCombat.h`: `CastMoralSupported`, `CastTypeMoralSupported` (`:337`).

## 1. Amaç

P-HB'nin **hangi üyeye hangi buff'ı ne zaman atacağını** (buff matrisi, eksik/süresi dolan takibi, çift buff ve debuff'la silinmiş buff'ı doğru ele alma, takım direnç planı) ve her iki priest'in **kime cure atacağını** (debuff türü × öncelik × rol ağırlığı skoru, cure rezervasyonu kapısı) saf mantık olarak eklemek; MET-BUFF-03 (tekrarlanan buff) ve MET-CURE-01/02 hedeflerinin **karar tarafını** hazırlamak.

## 2. Bağlam (okunması zorunlu)

- `docs/07` §6-§8, §14 (pseudocode); `docs/05` §4, §6 (BuffType, ID'ler); MEC-BUF-01..03 (aynı BuffType hedefte varken buff reddedilir; debuff eskiyi siler), MEC-BUF-09/10.
- F7-01 `ReservationTable::HasForeignOpen`, F7-02 `CureGateAllows` (iki priest aynı üyeye aynı anda cure atmaz).
- `docs/03` §16 ve AC-LRN-03: yalnızca gözlenebilir bilgi; üyenin gerçek buff listesi okunmaz; `E` sınıfı tahmin etiketi.
- ADR-0018 Ek 8, Ek 19 (`112656` ve BuffType 1 üçlüsü aynı hedefe art arda atılamaz): matriste **hedef başına tek HP buff'ı**.

## 3. Kapsam

**Yapılacaklar** (yeni başlıklar; yalnızca standart kütüphane + `TeamBlackboard.h`/`Perception.h`; sunucu başlığı/global-static yok; sabit boyutlu; ASCII + CRLF; yorumlar İngilizce)

### 3.1 `BotCore/PriestBuff.h`

1. **Sabitler** (`[V]` skill kimlikleri `docs/05:129-133`, `skills_mage.csv`; `+100000` El Morad): `kSkillInsensibilityPeel = 112660`, `kSkillUndying = 112654`, `kSkillMassiveness = 112657`, `kSkillFreshMind = 112645`, `kSkillGreatness = 112656` (**kullanılmaz**, `docs/07:127`), `kSkillImmunityFire = 110548`, `kSkillImmunityCold = 110648`; BuffType: `kBtHpMp = 1`, `kBtAc = 2`, `kBtResist = 8`; `kBuffRefreshMs = 20000` (`P-PRI-BUFF-REFRESH`), `kBuffBlockMs = 30000` (`docs/07:254`), `kBuffMaxMembers = 8`. **Immunity lightning `110748` sabiti yoktur** (ulaşılamaz, §0 madde 6).
2. **`enum BuffKind : uint8_t { BUFFK_AC = 1, BUFFK_HP, BUFFK_RESIST }`**; **`uint32_t HpBuffSkillFor(BotRole role)`** (matris harfiyen, `docs/07:123-125`): warrior (W-P/W-G) Undying, priest ve mage massiveness (§0 madde 2); `uint32_t BuffSkillFor(BuffKind, BotRole, ResistPlan)`; `uint32_t NationId(uint32_t karusId, bool elmorad)`.
3. **`struct ResistInputs { int enemyMagesFire, enemyMagesCold, enemyMagesLightning; }`**, **`enum ResistPlanKind : uint8_t { RESIST_FRESH_MIND = 1, RESIST_IMMUNITY_FIRE, RESIST_IMMUNITY_COLD }`**, **`struct ResistPlan { ResistPlanKind kind; uint32_t skillId; BotRole casterRole; }`** ve `ResistPlan ChooseResistPlan(const ResistInputs &, bool elmorad)` (`docs/07:140-143`): tam olarak **bir** element için `>= 2` düşman mage (gözlenen skill olaylarından, `docs/07:142`) ve o element ateş/soğuksa ⇒ `RESIST_IMMUNITY_FIRE` (`casterRole BotRole::MageFire`) / `RESIST_IMMUNITY_COLD` (`casterRole BotRole::MageIce`); iki element ≥ 2 (karışık), yıldırım ≥ 2 (atılamaz) veya hiçbiri ⇒ `RESIST_FRESH_MIND` (`casterRole BotRole::PriestHealBuff`). Hedef başına tek direnç buff'ı (BuffType 8).
4. **`struct MemberBuffState { int16_t id; BotRole role; bool inRange, reintegrating; uint32_t acRemainingMs, hpRemainingMs, resRemainingMs; bool acDebuffed, hpDebuffed, resDebuffed; }`** (`*RemainingMs` gözlenen **buff**'ın tahmini kalan süresi, `0` = yok/dolmuş; `*Debuffed` aynı BuffType'ta gözlenen **debuff**, MEC-BUF-03).
5. **`class BuffBlockMemory`** (sabit 8 üye × 3 tür, kopyalanabilir): `OnRejected(int16_t target, BuffKind, uint64_t nowMs)` (`nowMs + kBuffBlockMs`; `docs/07:254`: aynı tip bilinmeyen buff), `bool Blocked(int16_t target, BuffKind, uint64_t nowMs) const`; saat geri giderse güvenli.
6. **`struct BuffCtx { bool selfHasBuffTree; bool allowBuffNow; ResistPlan resist; bool elmorad; const MemberBuffState * members; int n; uint64_t nowMs; }`**, **`enum BuffDecisionKind : uint8_t { BUFFD_NONE = 0, BUFFD_CAST, BUFFD_WAIT_EXPIRY, BUFFD_CURE_FIRST }`**, **`struct BuffDecision { BuffDecisionKind kind; int16_t target; BuffKind buff; uint32_t skillId; uint32_t waitMs; }`** ve **`BuffDecision NextBuff(const BuffCtx &, const BuffBlockMemory &)`** (`docs/07:129-134,225-245`): (a) `!selfHasBuffTree` (P-HD) ⇒ `NONE`; (b) `!allowBuffNow` (savaş içinde acil heal ihtiyacı varken; `docs/07:134`) ⇒ `NONE`; (c) adaylar: **`reintegrating` üyeler önce** (`docs/08:149`), sonra `id` artan; menzil dışı üye atlanır; her üyede tür sırası `AC → HP → RESIST` `[A]`; (d) bir tür **eksik** ⇔ `RemainingMs == 0`; eksik ve `Debuffed` ise (debuff aynı tipi tutuyor, buff reddedilir/silinmiştir) ⇒ `BUFFD_CURE_FIRST` (hedef + tür; çağıran `BestCure`'a `reason = BUFF_BLOCKED` ile girer; `docs/07:132`); eksik, `Debuffed` değil ve `Blocked` değilse ⇒ `BUFFD_CAST` (`skillId = BuffSkillFor(...)`); (e) bir tür **etkin** (`RemainingMs > 0`) ⇒ **asla** `CAST` (**SK-03/MET-BUFF-03 = 0**; sunucu süre dolmadan yeniden uygulamayı reddeder, MEC-BUF-02); `0 < RemainingMs <= kBuffRefreshMs` ve başka `CAST`/`CURE_FIRST` adayı yoksa ⇒ `BUFFD_WAIT_EXPIRY` (`waitMs = RemainingMs`; konumu hedefin yakınında planlama F5/F6'nın işi); (f) **Greatness hiçbir yolla seçilmez** (`docs/07:127` "ilk kurulumda tek tek atama"); (g) boş/uygun aday yoksa `NONE`.
7. Diğer priest'in buff'ı atmaması (`docs/07:115`: yalnızca P-HB atar; P-HB ölürse P-HD atamaz) `selfHasBuffTree` ile ve F7-02 `BuffCoverageLost` ile karşılanır; ek kod yok.

### 3.2 `BotCore/PriestCure.h`

1. **Sabitler** (`[A]`, §0 madde 3): `kCueWeightCritical = 3.0f`, `kCueWeightHigh = 2.0f`, `kCueWeightMedium = 1.0f`, `kCueRoleMage = 1.3f` (`docs/07:159`), `kCueUrgencyBase = 1.0f`, `kCueUrgencyLowHp = 1.5f` (HP oranı < 0,5), `kCueDotMin = 800` (`P-PRI-CURE-DOT-MIN`), `kCueParasiteHpRatio = 0.6f`; skill: `kSkillCureCurse = 112525`, `kSkillCureDisease = 112535` (`docs/05:120-121`).
2. **`struct CureMember { int16_t id; BotRole role; bool inRange; float hpRatio; bool speedDebuff, fleeingOrRetreating, hpDebuff, acDebuff, enemyMeleeContact, atkOrDmgDebuff, pressureContinues, buffBlocked; int32_t dotRemaining; }`** (`speedDebuff` = BuffType 6 debuff gözlenmiş: kök/yavaşlatma; `hpDebuff` BuffType 1 debuff = Parasite türü; `acDebuff` BuffType 2 debuff = Malice/Torment; `atkOrDmgDebuff` BuffType 4/5 debuff = Massive/Slow; `buffBlocked` §3.1 `BUFFD_CURE_FIRST` kaynağı).
3. **`enum CuePriority : uint8_t { CUE_NONE = 0, CUE_MEDIUM, CUE_HIGH, CUE_CRITICAL }`** ve **`struct CureChoice { int16_t target; uint32_t skillId; CuePriority priority; float score; }`**; **`CureChoice BestCure(const CureMember *, int n, int16_t self, const ReservationTable &, uint64_t nowMs)`** (`docs/07:152-163`): her üye için eşleşen kurallar (her kural bir `debuff ağırlığı` ekler, Cure curse hepsini kaldırdığından **toplanır**, `docs/07:161`): (i) `speedDebuff && fleeingOrRetreating` ⇒ Kritik; (ii) `hpDebuff && hpRatio < kCueParasiteHpRatio` ⇒ Kritik; (iii) `acDebuff && enemyMeleeContact` ⇒ Yüksek; (iv) `atkOrDmgDebuff && role is warrior && pressureContinues` ⇒ Orta; (v) `buffBlocked` ⇒ Orta (buff yeniden atılabilsin); (vi) `dotRemaining >= kCueDotMin` ⇒ Yüksek, **Cure disease** (`112535`) kolu; `score = Σ ağırlık × rolAğırlığı (mage 1,3) × aciliyet`; mage üzerinde herhangi bir **Kritik** kural ⇒ ek çarpan yok (×1,3 zaten uygulanır). Aday elenir: `!inRange`, `CureGateAllows(table, id, self, nowMs) == false` (diğer priest aynı üyeye cure rezerve etmiş: F7-02), hiç kural eşleşmeyen. Seçim: en yüksek `score`, eşitlikte yüksek öncelik, sonra düşük `id`. `skillId`: curse kuralı eşleşmişse Cure curse, yalnızca DoT kuralı varsa Cure disease; `priority`: eşleşen en yüksek sınıf; hiçbir aday yoksa `target = -1`, `priority = CUE_NONE`.
4. **`bool CureBeforeNormalHeal(const CureChoice &)`** = `priority >= CUE_HIGH` (`docs/07:231-233` pseudocode: yüksek öncelikli cure normal heal'den önce; Orta yalnızca heal ihtiyacı yokken).
5. Cure atışı sırasında rezervasyon yazımı (`ReservationTable::Create(RES_CURE, …)`) çağıranın işidir (sunucu bağlaması); bu plan **kapıyı** sorgular, kaydı yazmaz.

### 3.3 Birim testleri (adlar bağlayıcı)

**`PriestBuffTests.cpp`** (üyeler: W-P `2970`, W-G `2972`, P-HB `2974`, M-F `2975`; `nowMs = 100000`; taban fikstür: herkes menzilde, üç tür etkin 300000 ms kalan, debuff yok, `allowBuffNow true`, `selfHasBuffTree true`):

1. `BuffMatrix_HpBuffByRole`: W-P/W-G ⇒ `112654`; priest ⇒ `112657`; mage ⇒ `112657`; El Morad `+100000`.
2. `BuffMatrix_SkillForKinds`: AC `112660`, direnç Fresh mind `112645`; `Greatness 112656` hiçbir `BuffSkillFor` çıktısında yok.
3. `Buff_NothingMissing_NoCast` (taban fikstür ⇒ `BUFFD_NONE`).
4. `Buff_MissingAc_CastPeel` (`acRemainingMs 0` ⇒ `CAST`, `112660`, ilgili üye); `Buff_ActiveSameBuffType_NeverRecast` (`acRemainingMs 100` ⇒ `CAST` **yok**, **MET-BUFF-03 = 0**); `Buff_ExpiredAtZero_RecastImmediately`.
5. `Buff_ExpiringWithin20s_WaitsForExpiry` (`hpRemainingMs 19000` ⇒ `BUFFD_WAIT_EXPIRY`, `waitMs 19000`, cast **yok**); `Buff_Expiring20001_NoWait` (20001 ⇒ `NONE`); `Buff_WaitYieldsToCastElsewhere` (başka üyede eksik buff varsa o üye için `CAST`).
6. `Buff_AcDebuffed_NeedsCureFirst` (Malice altında AC eksik ⇒ `BUFFD_CURE_FIRST`, `buff == BUFFK_AC`, `CAST` yok); `Buff_HpDebuffed_NeedsCureFirst` (Parasite).
7. `Buff_InBattleEmergency_NoBuff` (`allowBuffNow false` ⇒ `NONE`); `Buff_PhdHasNoBuffTree_None` (`selfHasBuffTree false`).
8. `Buff_OutOfRange_SkippedNextMember`; `Buff_ReintegratingMemberFirst` (id'si büyük `reintegrating` üye önce); `Buff_OrderAcHpResist` (üç tür birlikte eksik ⇒ sırayla AC, HP, RESIST).
9. `Buff_BlockedByUnknownBuff_NotRetriedFor30s` (`OnRejected` sonrası `29999` ms `CAST` yok; `30000` ms tekrar); `BuffBlockMemory_ClockBackwards_Safe`.
10. `Buff_NeverChoosesGreatness` (BuffType 1 eksik birçok üye; hiçbir çıktı `112656` değil).
11. `ResistPlan_DefaultFreshMind` (hiç düşman mage yok); `ResistPlan_TwoFireMages_ImmunityFire` (`casterRole BotRole::MageFire`, `110548`); `ResistPlan_TwoColdMages_ImmunityCold` (`BotRole::MageIce`, `110648`); `ResistPlan_OneMage_FreshMind`; `ResistPlan_MixedElements_FreshMind`; `ResistPlan_TwoLightningMages_FreshMind` (`110748` ulaşılamaz); `ResistPlan_ElMoradIds`.
12. `Buff_Determinism` (üye dizisi ters sırada ⇒ aynı karar).

**`PriestCureTests.cpp`** (`self = 2974`; `ReservationTable` F7-01'den):

13. `Cure_RootedMageRetreating_Critical` (`speedDebuff`+`fleeingOrRetreating`, mage ⇒ `CUE_CRITICAL`, `112525`, `score = 3 × 1,3 × 1,0 = 3,9`).
14. `Cure_ParasiteLowHp_Critical` (`hpDebuff`, `hpRatio 0,59` ⇒ Kritik; `0,6` ⇒ **kural eşleşmez**).
15. `Cure_MaliceWithMeleeContact_High` (`2,0`); `Cure_MaliceNoContact_NoCure`.
16. `Cure_MassiveOnWarriorWithPressure_Medium` (`1,0`); aynı debuff mage'de ⇒ kural eşleşmez.
17. `Cure_BuffBlocked_Medium` (`buffBlocked`).
18. `Cure_DotAtLeast800_CureDisease` (`dotRemaining 800` ⇒ `112535`, `CUE_HIGH`); `Cure_DotBelow800_None` (799).
19. `Cure_MageBeatsWarrior_T_PRI_05` (kök altındaki mage `3,9` vs Malice'li warrior `2,0` ⇒ mage seçilir).
20. `Cure_MultiDebuffMember_HigherScore` (aynı üyede `speedDebuff+fleeing` ve `acDebuff+contact` ⇒ `3 + 2 = 5`); `Cure_LowHpUrgency_x1_5` (`hpRatio 0,49` ⇒ ×1,5).
21. `Cure_ForeignCureReserved_Skipped` (F7-01 tablosunda B'nin `RES_CURE` kaydı ⇒ o üye elenir, sıradaki seçilir); `Cure_OwnReservationDoesNotBlockSelf`.
22. `Cure_OutOfRange_Skipped`; `Cure_NoDebuffs_ReturnsMinusOne`; `Cure_TieBreaks_HigherPriorityThenLowerId`.
23. `Cure_CurseBeatsDiseaseSkill_WhenBoth` (hem curse kuralı hem DoT ⇒ `112525`).
24. `CureBeforeNormalHeal_HighAndCriticalOnly` (Orta ⇒ `false`, Yüksek/Kritik ⇒ `true`).
25. `Cure_Determinism`.

**Kapsam dışı (yapılmayacak)**

- Sunucu bağlaması (`ObservedStatusTable`'dan `MemberBuffState`/`CureMember` kurma, `ActionExecutor` üzerinden `Moral` 2/4 atışı, `RES_CURE` rezervasyonu yazımı, telemetri `BUFF_COVERAGE_LOST`/`CURE_*`, MET-BUFF-01/03 ve MET-CURE-01/02 ölçümü, oyun içi T-PRI-04/05): **F7-08**.
- Priest'in pozisyonlanması ("bitişe 20 sn kala hedefin yakınında olmak"), mage'in direnç buff'ını **atması**, warrior self buff'ları (Defense/Gain çakışması `docs/06` §5: warrior kendi kararı F6), `Strength` buff'ı, scroll buff'ları.
- DoT/HoT izleme kaynağı (§0 madde 4), kritik debuff'ların BuffType tablosundan **sınıflandırılması** (çağıran: `ObservedStatusTable` + `SkillMeta`; bu plan bayrak alır), heal/diriltme/debuff kararları (F6-07/F7-02/F7-04/F7-07).
- `docs/` değişikliği (Claude), `GameServer/`, `shared/`, `AIServer/`.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/PriestBuff.h` | yeni | §3.1 |
| `BotCore/PriestCure.h` | yeni | §3.2 |
| `Tests/BotCoreTests/PriestBuffTests.cpp` | yeni | testler 1-12 |
| `Tests/BotCoreTests/PriestCureTests.cpp` | yeni | testler 13-25 |
| `BotCore/BotCore.vcxproj` | değiştir | iki `<ClInclude>` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | iki `<ClCompile>` satırı |

`TeamBlackboard.h`, `Perception.h`, `BotCombat.h`, `PriestCoord.h` **değişmez** (yalnızca include). Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F7-06 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; sunucuları durdur; test sayısını not et.
2. `PriestBuff.h` + testler 1-12; derle/çalıştır.
3. `PriestCure.h` + testler 13-25.
4. İki `.vcxproj` kaydı; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu (açık soruları §0 madde 2-6'ya bağla).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; test sayısı plan başındakinden **§3.3'teki `TEST_CASE` sayısı kadar** fazla (≈ 45); her ad `[ OK ]`; F7-01..F7-05 testleri değişmeden geçer
- [ ] K3 (MET-BUFF-03 = 0, birim düzeyi): `Buff_ActiveSameBuffType_NeverRecast`, `Buff_AcDebuffed_NeedsCureFirst`, `Buff_HpDebuffed_NeedsCureFirst`, `Buff_BlockedByUnknownBuff_NotRetriedFor30s`, `Buff_NeverChoosesGreatness` geçer; **gerçek kapsama (MET-BUFF-01 ≥ %90) ve MET-BUFF-03 ölçümü çalışma zamanında F7-08'dedir**
- [ ] K4 (T-PRI-05, birim düzeyi): `Cure_MageBeatsWarrior_T_PRI_05` `3,9 > 2,0`; skor örnekleri `3,9`, `2,0`, `1,0`, `5,0` testlidir; **MET-CURE-01 p95 ≤ 3 sn çalışma zamanında**
- [ ] K5 (iki priest, cure): `Cure_ForeignCureReserved_Skipped` geçer; F7-01 `HasForeignOpen` ve F7-02 `CureGateAllows` yalnızca çağrılır (tekrar yazılmaz: `grep -nE 'HasForeignOpen|CureGateAllows' BotCore/PriestCure.h` çağrıdır)
- [ ] K6 (skill verisi): kod sabitleri `docs/05:120-133` ve `skills_mage.csv` ile birebir (`112660/112654/112657/112645/112525/112535`, `110548/110648`); `110748` ve No-Recall/Counter Curse gibi doğrulanmamış kimlikler **yok** (`grep -nE '110748' BotCore/PriestBuff.h` yalnızca yorumda)
- [ ] K7 (saflık/adalet): yeni başlıklarda `GameServer|windows.h|stdafx|shared/` yok; `new `/`malloc`/`std::vector`/`std::map`/`printf`/`rand(` yok; global/static değişken yok; üyenin gerçek buff listesini okuyan alan yok (yalnızca `E` sınıfı gözlem)
- [ ] K8 (kapsam/biçim): `git diff --stat gece/2026-10-02...bot/F7-06` yalnızca §4'teki 6 dosya + plan; `TeamBlackboard.h`/`Perception.h`/`BotCombat.h`/`PriestCoord.h`/`GameServer/`/`shared/`/`docs/` farkı 0; ASCII + CRLF; `git diff --check` boş
- [ ] K9 (belgeleme): `[A]` kurallar (cure ağırlıkları, aciliyet, tür sırası AC→HP→RESIST, priest massiveness matrisi) kodda `[A]` yorumuyla; Uygulayıcı Raporu §0 madde 2-4'ü açık soru olarak listeler

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "BuffMatrix_|Buff_|BuffBlockMemory_|ResistPlan_|Cure_|CureBeforeNormalHeal_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F7-06
git diff gece/2026-10-02...bot/F7-06 -- BotCore/TeamBlackboard.h BotCore/Perception.h BotCore/BotCombat.h BotCore/PriestCoord.h | wc -l   # 0
file BotCore/PriestBuff.h BotCore/PriestCure.h
git diff --check gece/2026-10-02...bot/F7-06
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; §2.3 (oyun mekaniğini değiştirme); §2.5 (bota avantaj yok; buff/cure kararları yalnızca gözlem + kendi cast olaylarından).
- Tüm eşikler `[Ö]`, belirtilenler `[A]`; T-PRI-04/05 ve T-MECH-BUF-03 sonrası güncellenir, `docs/07` ile birlikte değişir.
- **Dürüstlük:** buff kalan süresi `E` sınıfıdır (tahmin; başkasının buff'ının bitişi gözlenemez, MEC-BUF-10). Kapsama (MET-BUFF-01) ve cure gecikmesi (MET-CURE-01) bu planla **kanıtlanmaz**; birim testi karar fonksiyonunun doğruluğunu gösterir.
- Priest massiveness matrisi ve `BUF-HP-01` çelişkisi (§0 madde 2) çözülmeden "doğru HP buff'ı seçiliyor" denemez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: (boş)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — (boş)
