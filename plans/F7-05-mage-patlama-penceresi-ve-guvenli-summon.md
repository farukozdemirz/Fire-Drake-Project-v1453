# F7-05: Mage takım davranışı: debuff sonrası patlama penceresi ve güvenli summon kapıları SUM-01..07 (`BotCore/SummonGate.h`, `BurstPlan.h`, `TeamBlackboard.h` `MemberStatus` ekleme)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F7 — Party koordinasyonu (`docs/17` §2 F7 bloğu; kapılar G7b ve G7c) |
| Branch | `bot/F7-05 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-08** (mage solo/tek: hasar seçimi, MP rezervi `P-MAG-MP-RESERVE`, kiting `KITE`, kendini koruma; `COMBAT` alt durumları `docs/08` §10) · **F6-01** (karar katmanı iskeleti, durum makinesi `docs/13` §6) · **F7-01** (`TeamBlackboard`) · **F7-03** (`TargetCall`: debuff çağrısı = patlama penceresi tetiği) · **F7-04** (`TargetCallReason`, `shortBurstWindow`) · **F7-02** (`BURST_NOW`) · F4-34/F4-35 `KAPANDI` (summon friend `110004/210004` ve Gate/descent atılabilir) · F4-33 `KAPANDI` (diriltme; ölçüm F7-07) · F4-52 `KAPANDI` (olay halkası: `SkillEvent.data[1]` summon sonucu) · F4-36 `KAPANDI` (`no_item` kapısı, Absolute power) · **F4-48 K9/K10 çalışma zamanı** (mage skill ölçümü; §0) |
| İlgili gereksinim / kabul | REQ-MAG-04, REQ-MAG-05, REQ-PTY-09 (summon kısmı); AC-MAG-04 (güvensiz summon ≤ %10; READY → summon p50 ≤ 2 sn: **çalışma zamanı F7-08**), AC-MAG-05 (ölü üyeye summon denemesi = 0: **birim düzeyinde bu planda**), T-MAG-05/06, T-IGT-MAG-01 (SUM-01..03 olmadan summon 0), MET-SUM-01/02, MET-PTY-01; `docs/08` §5, §8, §10, §11; `docs/09` §7, §9; ADR-0018 Ek 10 (güvenlik kapıları F7'de) |
| Tahmini büyüklük | M–L (üst sınır: 10 dosya; ≈ 40 test; yazım turunda **F7-05a summon** (`SummonGate.h`, `MemberStatus`, `Brain.h` geçişleri) / **F7-05b patlama penceresi** (`BurstPlan.h`, `MageCombat.h` bağı) olarak bölünebilir) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 kapanmadan HAZIR yapılmaz) |

---

## 0. Neden TASLAK

**HAZIR yapma ön koşulları:**

1. F6-08 (mage) `KAPANDI`: `KITE`/`SUMMON` alt durumlarının dışlayıcılığı (`docs/08:164`, SUM-07), acil-aksiyon bayrağının (`mageInEmergency`) F6-08'in durum makinesinden gelmesi ve MP rezervi (`P-MAG-MP-RESERVE` 500) F6-08'de tanımlı olmalı. F6-01'in `BotState` enum'u `RespawnHold/ReadyForSummon/Reintegrate`'i **tanımlar ama geçiş tablosunda `false` bırakır**; bu plan geçişleri yasal kılar (§3.1 madde 1). F6-08 `PickSingle`'ın patlama dalı ve `MageCombat.h` imzası bu plana işlenmiş olmalı (§3.3).
2. **Mage skill'lerinin çalışma zamanı ölçümü yapılmış olmalı:** F4-48 K9/K10 "sabaha ertelendi" (plan raporu), `docs/05` §9.6 yok; **Absolute power `110802` ve eşyalı Impact skill'leri botla hiç ölçülmedi** (ADR-0018 Ek 24 (d); STATUS "`BotMI_K` betiği ve eşyalı mage skill'leri" bekliyor). Patlama zinciri (`110802 → 110570/110670 → 110551/110651`) ölçülmemiş skill'lere dayanır; ölçüm F7-05 HAZIR olmadan tamamlanmalı ya da zincirden çıkarılmalı.
3. **`MEC-MAG-21` `[D]` kalanları** çalışma zamanında doğrulanmalı: başka zone/`canTeleport()` false/`m_bWarp` dalları ve "ışınlanan botun eski bölge kayıtları `[A]`" (`docs/03:141` sonu). Summon sonrası hedef botun algı tabloları (3×3 bölge) yenilenmezse `REINTEGRATE` (6 sn) yanlış hedef/boş görüş ile başlar; bu **F7-08 çalışma zamanı** konusudur ve `F7-05` yalnızca tetikleyecek durum geçişini yazar.
4. **No-Recall debuff'ı** (`docs/08:133` SUM-01) botun **gözlemleyebildiği** bir durum mu belirsiz (BuffType kimliği bu taslakta uydurulmadı, `docs/03` Ek A ve `MAGIC_TYPE4` verisinden doğrulanmalı). Gözlenemezse SUM-01'in o kolu `false` sabitlenir; sunucu reddi `srv_fail` (CASTING'te) veya `sData[1] = 0` (ışınlanmadı, MP gitti) olarak yakalanır ve bu plan **sonuç sınıflandırması + geri deneme** ile karşılar.
5. **SUM-04 belirsiz:** "Takımın en az bir priest'i ve bir başka canlı üyesi mage'den ≤ 40 m" (`docs/08:136`): "bir başka" priest dışında mı, summon edilen üyeyi içerir mi? Bu taslak `[A]`: **iki ayrı canlı üye** (biri priest, diğeri priest olmayabilir; ikisi de mage ve summon edilen hedef değil). Claude `docs/08` §8.1'de netleştirmeli.
6. SUM-02 sınırı ("25 m içinde düşman yok"): bu taslak `[A]` `en yakın düşman mesafesi ≥ 25 m` ⇒ güvenli (tam 25,0 m güvenli); aynı belirsizlik SUM-04 `≤ 40 m` için (`40,0` dahil). Doküman netleştirmesi ile birlikte kapanır.
7. Patlama pencerelerinin süreleri: `P-MAG-BURST-WINDOW` 6 sn (`docs/08:47`) tanımlı; **lider çağrısında "kısa pencere"** (`docs/09:177`) süresi **dokümanda yok**: bu taslak `[A]` 3 sn; Claude belirler.
8. `docs/11:140` rol MP eşiği (warrior %50, priest %70, mage %70) `RECOVER` tanımıdır; SUM-05 buna dayanır. Eşikler F6'da `docs/11` planlarıyla yazılmış olmalı (tek kaynak): bu plan o sabitleri **yeniden tanımlamaz**, çağıran `readyMpPct`/rol eşiğini parametre olarak verir.

**Yazım turunda yeniden doğrulanacak referanslar** (okuma: `gece/2026-10-02` @ `f4daa27`):

- `docs/08_MAGE_BEHAVIOR.md:19-27` (diriltme/respawn/summon ayrımı), `:39-51` (`P-MAG-SUMMON-SAFE-RADIUS` 25, `P-MAG-SUMMON-MIN-HP` 0,6, `P-MAG-SUMMON-DELAY` ≤ 2 sn, `P-MAG-BURST-WINDOW` 6 sn, `P-MAG-MP-RESERVE` 500), `:53-62` (öncelikler, madde 3 summon, madde 4 patlama), `:70-80` (§6.2 seçim tablosu: incineration mesafe ≤ 44 m), `:108-141` (§8 akış, SUM-01..07), `:143-150` (§8.2 sıra, §8.3 summon sonrası), `:164` (`KITE` ve `SUMMON` birbirini dışlar), `:166-173` (§11 fallback: 3 sn, 3 fail ⇒ yürüyerek), `:187-193` (AC-MAG-04/05).
- `docs/09_PARTY_COORDINATION_AND_TARGET_SELECTION.md:174-177` (§7: mage'ler `P-MAG-BURST-WINDOW` içinde patlama; debuff yoksa kısa pencere), `:192-207` (§9 ölüm/respawn akışı, `P-PTY-REINTEGRATE-MAX` 6 sn), `:229-236` (`TP` mesajı), `:157-163` (`BURST_NOW`).
- `docs/03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md:141` (MEC-MAG-21: summon sunucu davranışı), `:228-231` (MEC-T8-01..04), `:296-308` (MEC-DTH-01..09: respawn Ronark'ta blink yok; MP dolmaz).
- `docs/05_SKILL_CATALOG_AND_COMBAT_RULES.md:144-160` (§7 mage tablosu: `110802`, `110570`, `110670`, `110551`, `110651`, `110004`).
- `docs/11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md:140` (RECOVER MP eşikleri).
- `docs/13_BOT_ARCHITECTURE_AND_DATA_MODEL.md:211-237` (durum makinesi: `RESPAWNED → READY_FOR_SUMMON → REINTEGRATE → COMBAT`).
- `BotCore/Perception.h`: `TeamView`/`TeamMemberView` (`:1454-1484`; `dead`, `inView`, `dist`, `hp`, `maxHp`), `SkillEvent` (`:1804`; `data[1]`), `SkillEventRing` (`:1840`); `BotCore/BotCombat.h:425-440` (`CastSummonSupported`), `ActionExecutor.cpp:753-771` (summon istisnası), `:810-818` (`wantedTarget`/`bad_target`).

## 1. Amaç

(a) **Güvenli summon:** respawn olmuş **yaşayan** üyeyi mage'in yanına çeken kararı, `docs/08` §8.1'in yedi kapısı (SUM-01..07) ve §8.2 sırası, §11 geri denemesi (3 sn / 3 fail ⇒ yürüyerek / ≥ 30 sn lider kararı), cast sırasındaki yeniden denetim ve **sonucun doğru okunması** (ADR-0018 Ek 10: `effected` ışınlanmayı kanıtlamaz) ile saf mantık olarak eklemek; (b) summon el sıkışmasını taşıyan `MemberStatus` kaydını (`READY_FOR_SUMMON`/`REINTEGRATE`, 300 ms gecikmeli) `TeamBlackboard`'a eklemek; (c) **debuff çağrısından sonra** mage'in `P-MAG-BURST-WINDOW` içinde izleyeceği patlama sırasını (`Absolute power → incineration/Prismatic → Pillar of fire/Ice comet`) belirleyen planlayıcıyı eklemek.

## 2. Bağlam (okunması zorunlu)

- `docs/08` §8 ve §10-§11; `docs/09` §7, §9; MEC-MAG-21, MEC-T8-01..04; ADR-0018 Ek 10 (`:67-68`: güvenlik kapıları karar katmanının işidir; yürütücüye eklenmez; AC-LRN-03).
- **Adalet:** SUM-01..07 yalnızca gözlenebilir bilgiye dayanır: hedefin ölü olması `TeamView.dead` / `PARTY_HPCHANGE` / `WIZ_DEAD` (gözlem sözleşmesi: "Party üyelerinin HP ve MP'si" `docs/03` §16 `:537-560`), mage çevresindeki düşman konumları (bölge paketleri), düşman alan skill'i olayı (`SkillEventRing`, `WIZ_MAGIC_PROCESS` yayını). Düşmanın MP'si/cooldown'ı kullanılmaz.
- `docs/03` MEC-MAG-21: summon tek hedefli yolda "respawn'dan sonra 180 sn" kuralından geçmez; başarısız summon yine MP düşer (`Msp 5`) ve `sData[1] = 0` ile yayınlanır.
- Örnek düzen: F7-01 (`ReservationTable`), `BotCore/NavStuck.h` (sabit boyutlu durum nesneleri).

## 3. Kapsam

**Yapılacaklar** (yeni başlıklar yalnızca standart kütüphane + `TeamBlackboard.h`/`Perception.h`; sunucu başlığı/global-static yok; sabit boyutlu; ASCII + CRLF; yorumlar İngilizce)

### 3.1 `BotCore/TeamBlackboard.h` ekleme: `MemberStatus` (F7-01'e yalnızca ekleme)

1. **Durum türü F6-01'in `BotState`'idir** (`BotState::Combat`, `Retreat`, `Dead`, `RespawnHold`, `Respawned`, `ReadyForSummon`, `Reintegrate`; `docs/09:82`, `docs/13:233`); **ikinci bir durum enum'u tanımlanmaz.** F6-01 `RespawnHold/ReadyForSummon/Reintegrate`'i F7 için **ayırmış ve geçiş tablosunda `false`** bırakmıştır (`drafts/f6/F6-01` §3 madde 1: "F6 kodu bunlara geçiş üretmez"; test `Brain_StateMachine_LegalTransitions`). Bu plan `BotCore/Brain.h` `StateMachine` geçiş tablosunu **yalnızca** şu F7 geçişleri için yasal kılar: `Dead -> Respawned`, `Respawned -> ReadyForSummon`, `Respawned -> Regroup`, `ReadyForSummon -> Reintegrate`, `Reintegrate -> Combat`, `Dead -> RespawnHold`, `RespawnHold -> Respawned`, `RespawnHold -> Reintegrate` (`docs/13` §6 diyagramı; son ikisi F7-07 kullanır) ve `BrainTests.cpp`'deki ilgili F6-01 testini bu geçişleri `true` bekleyecek biçimde günceller. `struct MemberStatus { int16_t id; BotRole role; BotState state; int32_t hp, maxHp, mp, maxMp; float x, z; int32_t lifeStones, hpPots, mpPots; uint64_t tMs; }` (`BotRole`: F6-01).
2. `class MemberBoard` (sabit 8 üye × 4 geçmiş kaydı, kopyalanabilir; `TeamBlackboard`'a üye `members`): `void Post(const MemberStatus &)` (sahibi kendi kaydını yazar; geçmiş halkası); `bool View(int16_t id, int16_t viewer, uint64_t nowMs, MemberStatus & out) const` (kendi kaydı hemen; başkasınınki **`nowMs >= tMs + kTeamCommsDelayMs` olan en yeni kayıt** (`docs/09:75-76`, F7-01 sabiti); uygun kayıt yoksa `false`); `void Remove(int16_t id)` (ayrılma/despawn); `bool Ready(int16_t id, ...)` yoktur (kural §3.2'de).
3. **Durum geçişleri (yalnızca summon akışı):** `BotState NextStateAfterRespawn(bool mpAtRoleThreshold, bool summonerAlive, bool teamRetreat)` ⇒ `BotState::ReadyForSummon` (MP eşiği tamam, summoner var, takım `RETREAT` değil) aksi `BotState::Respawned` (yürüyerek REGROUP kararı F6/F7 durum makinesinindir) (`docs/09:198-203`, `docs/13:233-234`); `BotState NextStateAfterSummon(bool arrived, uint32_t sinceArrivalMs, uint32_t maxMs, bool buffAndHealOk)` ⇒ `BotState::Reintegrate` varışta, `buffAndHealOk || sinceArrivalMs >= maxMs` (`P-PTY-REINTEGRATE-MAX` 6000 ms, `docs/09:204`) ⇒ `BotState::Combat`. `DEAD/RESPAWN_HOLD`/diriltme geçişleri F7-07'nin işidir.

### 3.2 `BotCore/SummonGate.h`

1. **Sabitler** (`[Ö]`, kaynak yorumlu): `kSummonSafeRadiusM = 25.0f`, `kSummonMinHpPct = 60` (`P-MAG-SUMMON-MIN-HP`), `kSummonSupportRadiusM = 40.0f` (SUM-04), `kSummonReadyHpPct = 90` (SUM-05), `kSummonEnemyAreaSkillMs = 3000` (SUM-02), `kSummonOverwhelmPct = 40` (SUM-06), `kSummonWarpGuardMs = 3000` (`[A]`: aynı hedefe önceki summon'dan sonra ışınlanma süresi), `kSummonRetryMs = 3000`, `kSummonMaxFails = 3` (`docs/08:170`), `kSummonWaitWalkMs = 30000` (`docs/08:141`), `kSummonCastMs = 1500`, `kReintegrateMaxMs = 6000`.
2. **`struct SummonCtx`** (çağıranın değer kopyası; her alan hangi SUM kapısına ait olduğu yorumlu): SUM-01: `targetAlive`, `targetInParty`, `targetNoRecall` (gözlenemezse sabit `false`, §0 madde 4), `msSinceLastSummonOfTarget`; SUM-02: `nearestEnemyDistM` (görünür düşmanlar; `1e9f` = yok), `msSinceEnemyAreaSkillOnMage`; SUM-03: `mageHp, mageMaxHp, mageRetreating`; SUM-04: `const SummonMember * members; int nMembers` (`struct SummonMember { int16_t id; BotRole role; bool alive; float distToMage; }`, hedef ve mage listede yoktur); SUM-05: `targetReady` (`BotState::ReadyForSummon` görünür), `targetHp, targetMaxHp, targetMp, targetMaxMp, readyMpPct` (rol eşiği `docs/11:140`: çağıran verir); SUM-06: `ownAlive, visibleEnemies`; SUM-07: `mageInEmergency` (kiting, kendi pot'u, acil aksiyon).
3. **`struct SummonVerdict { bool allowed; uint8_t failedMask; }`** (`bit0..bit6` = SUM-01..07) ve **`SummonVerdict EvaluateSummon(const SummonCtx &)`**: yedi kapı bağımsız hesaplanır, `allowed = (failedMask == 0)`; kurallar: SUM-01 `targetAlive && targetInParty && !targetNoRecall && msSinceLastSummonOfTarget >= kSummonWarpGuardMs`; SUM-02 `nearestEnemyDistM >= kSummonSafeRadiusM && msSinceEnemyAreaSkillOnMage >= kSummonEnemyAreaSkillMs`; SUM-03 `mageHp * 100 >= kSummonMinHpPct * mageMaxHp && !mageRetreating` (tam sayı aritmetiği); SUM-04 (§0 madde 5 `[A]`) mage ≤ 40 m içinde **en az bir canlı priest** ve **ondan farklı en az bir canlı üye**; SUM-05 `targetReady && targetHp*100 >= kSummonReadyHpPct*targetMaxHp && targetMp*100 >= readyMpPct*targetMaxMp`; SUM-06 `ownAlive * 100 >= kSummonOverwhelmPct * visibleEnemies` (`visibleEnemies == 0` ⇒ geçer); SUM-07 `!mageInEmergency`. `maxHp <= 0` ⇒ ilgili kapı **reddedilir** (güvenli taraf).
4. **`bool ReCheckBeforeEffect(const SummonCtx &)`** (cast'in 1,5 sn'lik CASTING→EFFECTING aralığında; SK-02 `docs/05:168`): SUM-01, SUM-02, SUM-03, SUM-07'yi yeniden değerlendirir; biri ihlal edilirse `false` ⇒ çağıran `CastCancel` verir (F4-24; MP düşmez, `MEC-MAG-08`).
5. **Sıra ve geri deneme:** `struct SummonCandidate { int16_t id; BotRole role; uint64_t readySinceMs; }`; `int PickSummonTarget(const SummonCandidate *, int n)` (`docs/08:143-145`): öncelik sınıfı **priest > ana hasar (W-P/M-F) > diğerleri**; aynı sınıfta daha erken `readySinceMs`, eşitse düşük `id`; `uint64_t NextSummonAllowedMs(uint64_t lastCastStartMs)` = `lastCastStartMs + kSummonCastMs` (iki üye ardışık çalışır: `docs/08:145`); `class SummonRetry` (hedef başına, sabit 8): `OnFail(id, nowMs)` (`fails++`, `nextTryMs = nowMs + kSummonRetryMs`), `OnSuccess(id)` (`fails = 0`), `bool MayTry(id, nowMs)`, `bool WalkInstead(id)` (`fails >= kSummonMaxFails`), `bool LeaderWalkDecision(id, readySinceMs, nowMs)` (`nowMs - readySinceMs >= kSummonWaitWalkMs`).
6. **Sonuç sınıflandırması** (ADR-0018 Ek 10; MEC-MAG-21): `enum SummonResult { SUMMON_PENDING = 0, SUMMON_OK, SUMMON_FAILED }` ve `SummonResult ClassifySummon(bool actionEffected, bool eventKnown, int16_t eventData1, float targetDistToMageM, uint32_t sinceEffectMs, uint32_t windowMs)`: `!actionEffected` ⇒ `SUMMON_FAILED` (`srv_fail`/iptal); `eventKnown` ⇒ `data1 == 1 ? OK : FAILED`; olay yoksa `targetDistToMageM <= 3,0` ⇒ `OK`; `sinceEffectMs >= windowMs` (varsayılan 3000 ms `[A]`) ve uzak ⇒ `FAILED`; aksi `PENDING`. **`effected` tek başına `OK` değildir.**
7. **Gecikme ölçüsü:** `bool SummonReactionOk(uint64_t readySeenMs, uint64_t castStartMs)` = `castStartMs - readySeenMs <= 2000` (`P-MAG-SUMMON-DELAY`, AC-MAG-04 p50 kriterinin tek örnek kontrolü; dağılım F7-08).

### 3.3 `BotCore/BurstPlan.h` ve F6-08 bağlaması

**Tek kaynak:** mage'in patlama sırası (M-F incineration `110570` → Pillar of fire `110551`; M-I Prismatic `110670` → Ice comet `110651`; menzil `<= 44 m`, MP rezervi, `SkillReady`, element tahmini) **F6-08 `MageCombat.h` `PickSingle`'dadır** (`drafts/f6/F6-08` §3 madde 2: "patlama penceresi (solo: hedefe yavaşlatma uygulandı <= 6 sn önce ya da hedef HP bilinen < %50)"). Bu plan o sırayı **yeniden yazmaz**; takım için yalnızca (a) pencerenin **takım kaynağını** ve (b) pencerede **Absolute power'ı** (`docs/08:58`: "Absolute power (hazırsa) → incineration/Prismatic → Pillar/Ice comet") ekler.

1. **Sabitler:** `kBurstWindowMs = 6000` (`P-MAG-BURST-WINDOW`, `docs/08:47`), `kBurstShortWindowMs = 3000` (`[A]`, §0 madde 7), `kBurstNowDeadlineMs = 2000` (`docs/09:163`), `kSkillAbsolutePower = 110802` (`docs/05:156`; El Morad `+100000`).
2. **`bool BurstWindowOpen(uint64_t callMs, bool shortWindow, uint64_t nowMs)`** (`nowMs >= callMs && nowMs - callMs < pencere`); `callMs` yoksa pencere yok; saat geri gidiyorsa `false`. **`bool BurstWindowFromTeam(const TargetCall *, uint64_t nowMs, bool burstNow, uint64_t burstNowDeadlineMs)`**: görünür bir `TargetCall` (F7-03; `TCALL_DEBUFF_SUCCESS` pencereyi tam, `TCALL_LEADER` kısa açar) ya da `burstNow && nowMs < burstNowDeadlineMs` ise `true`.
3. **`struct AbsPowerCtx { bool windowOpen; bool absReady, absItemsOk, usedThisWindow; int32_t mp; uint16_t msp; int32_t reserveMp; }`** ve **`enum BurstAdvice { BURST_DEFER = 0, BURST_ABS_POWER }`**, **`BurstAdvice AdviseBurst(const AbsPowerCtx &)`**: pencere açık, `absReady && absItemsOk && !usedThisWindow && mp >= msp + reserveMp` ise `BURST_ABS_POWER` (skill `110802`), aksi `BURST_DEFER` (kalan seçimi F6-08 `PickSingle` yapar; `windowOpen` ona **girdi olarak** verilir). `absItemsOk` çağıranın `SkillSpec.itemOk` değeridir (scroll + Stone of Mage, `no_item` kapısı F4-36).
4. **`BotCore/MageCombat.h` (F6-08'e küçük ekleme):** `PickSingle` patlama penceresi koşulu "solo koşul **veya** `DecisionInput`'tan gelen takım penceresi bayrağı" olur (yeni bayrak `bool teamBurstWindow`, varsayılan `false` ⇒ F6-08 testleri değişmeden geçer); `Absolute power` F6-08'de seçilmiyorsa bu plan çağıran sırada `AdviseBurst`'ü **önce** sorgular.

### 3.4 Birim testleri (adlar bağlayıcı)

`Tests/BotCoreTests/SummonGateTests.cpp` (taban "güvenli" fikstür `SafeCtx()`: hedef canlı/party'de/no-recall yok/son summon 10000 ms önce; en yakın düşman 60 m, düşman alan skill'i 10000 ms önce; mage HP 1000/1000, retreat yok; destek: priest 20 m + W-P 25 m canlı; hedef READY, HP 100/100, MP %80 (eşik %70); `ownAlive 6`, `visibleEnemies 6`; acil aksiyon yok). Her test **bir** kapıyı çevirir:

1. `Summon_Baseline_Allowed` (`failedMask == 0`).
2. SUM-01: `Sum01_TargetDead_Denied` (**AC-MAG-05: ölü hedefe summon yok**; `failedMask == 0x01`); `Sum01_TargetNotInParty_Denied`; `Sum01_TargetNoRecall_Denied`; `Sum01_RecentSummon_Denied` (2999 ms ret, 3000 ms geçer `[A]`).
3. SUM-02: `Sum02_EnemyWithin25m_Denied` (24,9 m ret; 25,0 m geçer `[A]`); `Sum02_EnemyAreaSkillWithin3s_Denied` (2999 ms ret; 3000 geçer).
4. SUM-03: `Sum03_MageHpBelow60Percent_Denied` (`599/1000` ret; `600/1000` geçer); `Sum03_MageRetreating_Denied`; `Sum03_ZeroMaxHp_Denied`.
5. SUM-04: `Sum04_NoPriestNear_Denied`; `Sum04_NoOtherMemberNear_Denied` (yalnızca priest; `[A]` iki ayrı üye); `Sum04_Boundary40m` (40,0 m geçer, 40,1 m ret); `Sum04_DeadPriestDoesNotCount`.
6. SUM-05: `Sum05_NotReadyState_Denied`; `Sum05_TargetHp89_Denied` / `Sum05_TargetHp90_Allowed`; `Sum05_MpBelowRoleThreshold_Denied` (priest/mage %70: %69 ret; warrior %50: %49 ret, %50 geçer; eşik parametredir).
7. SUM-06: `Sum06_Overwhelmed_Denied` (`ownAlive 3`, düşman 10 ⇒ %30 ret); `Sum06_Exactly40Percent_Allowed` (4 / 10); `Sum06_NoEnemies_Allowed`.
8. SUM-07: `Sum07_MageInEmergency_Denied` (kiting/pot).
9. `Sum_FailedMask_ReportsAllFailures` (SUM-02 + SUM-03 + SUM-06 birlikte ⇒ `failedMask == 0b0100110`).
10. `Summon_ReCheckBeforeEffect_CancelsOnNewThreat` (CASTING sırasında en yakın düşman 20 m'ye girer ⇒ `false`; hedef ölür ⇒ `false`; mage HP düşer ⇒ `false`; hiçbir değişiklik ⇒ `true`).
11. `Queue_PriestFirstThenMainDamageThenOthers` (adaylar W-G, M-F, P-HB: sıra P-HB, M-F, W-G); `Queue_SameClass_EarlierReadyWins_ThenLowerId`; `Queue_TwoWaiting_SequentialNotConcurrent` (`NextSummonAllowedMs(1000) == 2500`).
12. `Retry_Fail_ReevaluateAfter3s` (`MayTry` 2999 ms hayır, 3000 evet); `Retry_ThreeFails_WalkInstead`; `Retry_SuccessResets`; `Retry_WaitOver30s_LeaderWalkDecision` (29999 hayır, 30000 evet).
13. `Result_EffectedAlone_IsNotOk` (olay yok, hedef 30 m uzak, `sinceEffect 100 ms` ⇒ `SUMMON_PENDING`; `3000 ms` ⇒ `SUMMON_FAILED`); `Result_EventData1One_Ok`; `Result_EventData1Zero_Failed` (effected olsa da); `Result_NoEvent_TargetWithin3m_Ok`; `Result_NotEffected_Failed`.
14. `Reaction_Within2s_Ok` (2000 ms evet, 2001 hayır).
15. `Member_PostAndView_CommsDelay` (başkası 299 ms'de görmez, 300 ms'de görür; sahibi hemen); `Member_ViewReturnsNewestOlderThanDelay` (geçmiş halkası: iki kayıt, gecikme sonrası yalnızca eskisi görünür); `Member_Remove`.
16. `Member_ReadyAfterRespawn_Transitions` (MP eşiği tamam + summoner var + `RETREAT` değil ⇒ `BotState::ReadyForSummon`; summoner yok ⇒ `BotState::Respawned`; takım `RETREAT` ⇒ `BotState::Respawned`); `Member_Reintegrate_MaxSixSeconds` (5999 ms ve buff/heal tamam değil ⇒ `BotState::Reintegrate`; 6000 ⇒ `BotState::Combat`; buff/heal tamam erken ⇒ `BotState::Combat`).
17. `Summon_Determinism` (aynı bağlam iki kez ⇒ aynı `SummonVerdict`; `members` dizisi sırası değişse de aynı sonuç).

`Tests/BotCoreTests/BurstPlanTests.cpp` (F6-08 `PickSingle` testleri **değişmez**):

18. `Burst_WindowOpen6sAfterCall` (5999 ms açık, 6000 kapalı); `Burst_ShortWindowAfterLeaderCall` (2999 açık, 3000 kapalı `[A]`); `Burst_NoCall_NoWindow`; `Burst_ClockBackwards_Closed`.
19. `Burst_WindowFromTeam_Sources`: `TCALL_DEBUFF_SUCCESS` çağrısı ⇒ tam pencere; `TCALL_LEADER` ⇒ kısa; çağrı yokken `burstNow` ve süre dolmamış ⇒ açık; süre dolunca (`kBurstNowDeadlineMs`) kapalı.
20. `Burst_AbsPower_FirstWhenReady` (pencere açık, hazır, eşya tamam, `mp >= msp + 500` ⇒ `BURST_ABS_POWER`, skill `110802`; El Morad `210802`); `Burst_AbsPower_NotReady_Defers`; `Burst_AbsPowerNoItems_Defers` (`absItemsOk false`); `Burst_AbsPowerUsedThisWindow_Defers`; `Burst_AbsPowerMpBelowReserve_Defers` (`msp 240`: MP 739 ret, 740 izin; rezerv 500).
21. `Burst_WindowClosed_Defers` (pencere kapalıyken Absolute power **atılmaz**).
22. `Burst_TeamWindowFlag_PassesToPickSingle` (F6-08 entegrasyonu: `teamBurstWindow = true` iken `PickSingle` M-F için incineration, M-I için Prismatic seçer; bayrak `false` iken F6-08 solo davranışı **aynen**); `Burst_TeamWindowFlag_DefaultFalse_KeepsF608`.
23. `Burst_Determinism`.

**Kapsam dışı (yapılmayacak)**

- Sunucu bağlaması: `MemberStatus` yazımı/okuması (`BotManager`), `ActionExecutor` üzerinden `CastStart/CastEffect/CastCancel` ve `bad_target` (kendine summon yok: zaten F4-34), `SkillEvent.data[1]` kaynak seçimi, `TP` chat gönderimi (`FormatTeamChat` F7-04), telemetri `SUMMON_*`/`MET-SUM-*`, **oyun içi doğrulama** (T-MAG-05/06, T-IGT-MAG-01, AC-MAG-04): **F7-08**.
- Ölü üyenin **diriltilmesi**, `RESPAWN_HOLD`, `P-PTY-RES-WAIT`, `ResIntent`, diriltme güvenliği: F7-07. Respawn/yürüyerek regroup yolu, `RETREAT/REGROUP` lider kararı ve tam yenilgi ("önce summoner + anchor + bir priest yürür"): ayrı F7 planları (bu seri dışı).
- Mage solo davranışı (hasar seçimi, kiting, Mana Shield, element tahmini, alan fırsatı): F6-08. Gate/Escape/descent kararları (geri çekilme `docs/08` §7, W-G peel): F6/F7 ilgili planlar.
- Summon dışı Type8 (Escape, Blink, Wild advent) ve herhangi bir yürütücü/`BeginCast` değişikliği (`GameServer/Bot/ActionExecutor.cpp` **değişmez**).
- `docs/` değişikliği (Claude), `GameServer/`, `shared/`.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/SummonGate.h` | yeni | §3.2 |
| `BotCore/BurstPlan.h` | yeni | §3.3 |
| `BotCore/TeamBlackboard.h` | değiştir | yalnızca ekleme (§3.1); F7-01/F7-03 satırları değişmez (`git diff` yalnızca `+`) |
| `BotCore/Brain.h` | değiştir | yalnızca `StateMachine` geçiş tablosuna §3.1 madde 1'deki F7 geçişleri (F6-01 satırları değişmez; tablo girdileri `false` → `true`) |
| `Tests/BotCoreTests/BrainTests.cpp` | değiştir | yalnızca `Brain_StateMachine_LegalTransitions`'ın F7 geçişlerini `true` bekleyen satırları + yeni `Brain_StateMachine_F7Transitions` testi |
| `BotCore/MageCombat.h` | değiştir | yalnızca `teamBurstWindow` girdisi (§3.3 madde 4; varsayılan `false`, F6-08 davranışı aynen) |
| `Tests/BotCoreTests/SummonGateTests.cpp` | yeni | testler 1-17 |
| `Tests/BotCoreTests/BurstPlanTests.cpp` | yeni | testler 18-23 |
| `BotCore/BotCore.vcxproj` | değiştir | iki `<ClInclude>` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | iki `<ClCompile>` satırı |

`Perception.h`, `BotCombat.h`, `PriestHeal.h`, `GameServer/` **değişmez**. Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F7-05 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; sunucuları durdur; test sayısını not et.
2. `Brain.h` F7 geçişleri (+ `BrainTests.cpp`: F6-01 testi `true` bekleyecek biçimde, yeni `Brain_StateMachine_F7Transitions`); `TeamBlackboard.h` `MemberStatus` eki + üye testleri (15-16).
3. `SummonGate.h` + testler 1-14, 17.
4. `BurstPlan.h` + `MageCombat.h` `teamBurstWindow` girdisi + testler 18-23 (önce F6-08 testlerinin değişmeden geçtiğini doğrula).
5. İki `.vcxproj` kaydı; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu (açık soruları §0'a bağla).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; test sayısı plan başındakinden **§3.4'teki `TEST_CASE` sayısı kadar** fazla (≈ 45); her ad `[ OK ]`; F7-01..F7-04 testleri değişmeden geçer
- [ ] K3 (SUM-01..07, birim düzeyi): her kapının ayrı reddi ve sınır değerleri testlidir (`Sum01_*`..`Sum07_*`); `Sum01_TargetDead_Denied` ve `Sum_FailedMask_ReportsAllFailures` geçer. **AC-MAG-05 (ölü üyeye summon denemesi = 0) bu planın birim düzeyinde karşılanır; AC-MAG-04 ve T-IGT-MAG-01 çalışma zamanında F7-08'dedir ve bu planla kanıtlanmış sayılmaz**
- [ ] K4 (sonuç okuma): `Result_EffectedAlone_IsNotOk`, `Result_EventData1Zero_Failed`, `Result_NoEvent_TargetWithin3m_Ok` geçer (ADR-0018 Ek 10)
- [ ] K5 (geri deneme/sıra): `Retry_*` (3 sn, 3 fail, 30 sn) ve `Queue_*` (priest > ana hasar > diğer; ardışık) geçer
- [ ] K6 (patlama penceresi): `Burst_WindowFromTeam_Sources`, `Burst_AbsPower_*`, `Burst_WindowClosed_Defers`, `Burst_TeamWindowFlag_*` geçer; F6-08 `PickSingle` testleri **değişmeden** geçer; `Absolute power` kimliği `110802` `docs/05:156` ile birebir; patlama sırası (incineration/Prismatic/Pillar/Ice comet) bu planda **yeniden yazılmamıştır** (`grep -nE '110570|110670|110551|110651' BotCore/BurstPlan.h` yalnızca yorumda)
- [ ] K7 (saflık/adalet): yeni başlıklarda `GameServer|windows.h|stdafx|shared/` yok; `new `/`malloc`/`std::vector`/`std::map`/`printf`/`rand(` yok; global/static değişken yok; düşman MP/cooldown/envanter alanı yok; No-Recall `BuffType` sayısı kodda yok
- [ ] K8 (kapsam/biçim): `git diff gece/2026-10-02...bot/F7-05 -- BotCore/TeamBlackboard.h | grep -c '^-[^-]'` = 0; `git diff --stat` yalnızca §4'teki 10 dosya + plan; `Perception.h`/`BotCombat.h`/`PriestHeal.h`/`ActionExecutor.cpp`/`GameServer/`/`shared/`/`docs/` farkı 0; ASCII + CRLF; `git diff --check` boş
- [ ] K9 (belgeleme): `[A]` kurallar (SUM-04 yorumu, SUM-02/04 sınır dahilliği, 3000 ms kısa pencere, 3000 ms `kSummonWarpGuardMs`, 3,0 m varış eşiği) kodda `[A]` yorumuyla; Uygulayıcı Raporu açık sorularda listeler

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Summon_|Sum0|Sum_|Queue_|Retry_|Result_|Reaction_|Member_|Burst_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F7-05
git diff gece/2026-10-02...bot/F7-05 -- BotCore/TeamBlackboard.h | grep -c '^-[^-]'   # 0
file BotCore/SummonGate.h BotCore/BurstPlan.h Tests/BotCoreTests/SummonGateTests.cpp Tests/BotCoreTests/BurstPlanTests.cpp
git diff --check gece/2026-10-02...bot/F7-05
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; §2.3 (oyun mekaniğini değiştirme: summon güvenliği **karar**dır, sunucu kuralı değildir); §2.5 (bota avantaj yok: yalnızca gözlenebilir bilgi; summon, insanın yapabileceği bir party skill'idir, `docs/03` MEC-T8-01 onay gerektirmez).
- **Dürüstlük:** AC-MAG-04/05 ve MET-SUM-02 yalnızca çalışma zamanında kanıtlanır; birim testleri **kapıların doğruluğunu** gösterir, "güvensiz summon azaldı" kanıtı değildir. Absolute power zinciri F4-48 K9/K10 ve eşyalı skill ölçümü olmadan **ölçülmemiş skill'e** dayanır (§0 madde 2).
- Tüm eşikler `[Ö]`, belirtilenler `[A]`; T-MAG-05/06 sonrası güncellenir ve `docs/08` ile birlikte değişir. ADR-0018 Ek 10: güvenlik kapıları yürütücüye **eklenmez**; bu plan `ActionExecutor`'a dokunmaz.
- Bu plan `MemberStatus.lifeStones` alanını yalnızca saklar; diriltme güvenliği (taş stoğu `docs/07:200`) F7-07'dedir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: (boş)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — (boş)
