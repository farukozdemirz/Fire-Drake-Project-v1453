# F7-07: Priest diriltme kararı: güvenlik kapıları, skill/taş seçimi, `ResIntent`, `RESPAWN_HOLD` bekleme ve sonuç doğrulama (`BotCore/PriestRes.h`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F7 — Party koordinasyonu (`docs/17` §2 F7 bloğu; kapı G7a) |
| Branch | `bot/F7-07 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F7-01** (`ReservationTable`: `RES_RES`) · **F7-05** (`MemberStatus.lifeStones`, `BotState`, `NextStateAfterRespawn`: diriltme olmazsa summon akışı) · **F7-02** (`BotRole`, görev sırası: diriltme P-HD'ye aittir) · **F6-07** (priest solo: MP/HP rezervi, kendini koruma) · **F6-09** (priest buff/cure/debuff ve **diriltmenin tek müttefik hâli**: `F6-07` §3 kapsam dışı satırı "buff, cure, debuff/hedef çağrısı + chat, diriltme ... F6-09 ve F7"; **bu an taslağı yok**; diriltme orada tanımlanırsa bu plan onu takım güvenlik kapılarıyla genişletir, ikinci `ChooseResSkill` yazmaz) · **F6-01** (`BotRole`, `BotState`: `Dead`, `RespawnHold`; durum makinesi geçişleri F7-05 §3.1 madde 1'de yasal kılınır) · F4-33 `KAPANDI` (diriltme atılabilir, `docs/03` MEC-MAG-20, `docs/05` §9) · F4-07 `KAPANDI` (`WIZ_REGENE`, CLI-14 `dead_wait`) · F4-40 `KAPANDI` (taş stoğu, `db/004`) · F4-53 + **F4-60/F4-61** (ölüm temizliği, gözlem) · F4-52 `KAPANDI` (olay halkası: düşman alan skill'i) |
| İlgili gereksinim / kabul | REQ-PRI-01 (P-HD diriltme), REQ-MAG-05 (diriltme/respawn/summon ayrımı), REQ-PTY-09; AC-PRI-08 (güvensiz diriltme ≤ %10: **çalışma zamanı F7-08**), T-PRI-08 (diriltme güvenlik koşulları), T-IGT-PRI-01; `docs/07` §10, §15 ("Diriltme fail (taş yok) ⇒ respawn sinyali; mage summon akışı"); `docs/09` §9; `docs/08` §2 (diriltme ≠ respawn ≠ summon); `docs/03` MEC-MAG-20, MEC-DTH-01..09 |
| Tahmini büyüklük | S–M (1 yeni başlık + 1 yeni test dosyası + 2 `.vcxproj` = 4 dosya; ≈ 38 test) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 kapanmadan HAZIR yapılmaz) |

---

## 0. Neden TASLAK

**HAZIR yapma ön koşulları:**

1. F7-01, F7-02, F7-05 `KAPANDI`; F7-06'nın `MemberBuffState` türünü bu plan kullanır (diriltme sonrası "buff eksik" kuyruğu).
2. **`docs/09:196` kuralı açık değil (Claude netleştirmeli):** `if u.role is not priest and P-HD alive and res_conditions_likely(u)` ⇒ P-HD `ResIntent` yazar ve ölen bot `RESPAWN_HOLD` bekler; **ölen bir priest bu yoldan bekletilmez**, hemen respawn olur. Gerekçe belgede yok (P-HB ölürse P-HD onu diriltebilir: `docs/07` §10 yalnızca P-HD'nin diriltmesini tanımlar ve ölen rolü sınırlamaz). Bu taslak belgeyi **harfiyen** uygular `[A]`; Claude gerekçeyi yazmadan/kuralı düzeltmeden HAZIR olmaz. `res_conditions_likely(u)` de tanımsız: bu taslak `[A]` "ceset mesafesi hariç `EvaluateRes` kapılarının hepsi geçiyor" tanımını önerir (§3.2 madde 5).
3. **MP kuralı belirsiz:** `docs/07:199` "MP ≥ 800 + rezerv": **rezerv** `P-PRI-MP-RESERVE` (1100) mi başka bir değer mi? Bu taslak `[A]` 1100 (yani 1900) ve `reserveMp` parametresi; en pahalı diriltme (favors) 800 MP'dir, love 400 MP'dir (`docs/05:128`): kural "en pahalı diriltmeyi + rezervi karşılamak" olarak okunmuştur.
4. **Taş stoğu bilgisi:** bot hedefte `MemberStatus.lifeStones` (F7-05) ile bilinir (`db/002`: 30 taş; F4-40 doldurma); **insan oyuncuda bilinmez** `[D]`: `docs/07:200` "deneme yapılır, fail olursa tekrar denenmez". Sunucu taşları **ölü hedeften** alır ve **taş eksikse yayın yine gider** (`effected` diriltmeyi kanıtlamaz, ADR-0018 Ek 9, `docs/03` MEC-MAG-20): sonuç hedefin canlanmasından okunur (§3.1 madde 6).
5. **Düşman alan skill'i olayı:** "son 3 sn'de cesedin 20 m çevresinde düşman alan skill'i yok" (`docs/07:198`) için olay halkasında (F4-52 `SkillEventRing`, `SkillEvent.data[0]/data[2]` hedef noktası, `target == -1` alan yayını) hedef noktası + düşman çağıran bilgisi yeterli mi, `SkillMeta` `Moral` taşımıyor (F4-53 §3): bu plan yalnızca **sorgu sonucunu** (`msSinceEnemyAreaSkillNearCorpse`) girdi alır; hesabı çağıran (F7-08) yapar. Yazım turunda F4-60/F4-61 sonrası `SkillEventRing`'in bu soruyu cevaplayabildiği doğrulanır.
6. CLI-14 (`docs/03:393`): bot ölümünü fark ettiği andan **≥ 3,0 sn** sonra `WIZ_REGENE` yollayabilir (`dead_wait`); `P-PTY-RES-WAIT` 12 sn bunun **üstünde** bir bekleme kararıdır. `RESPAWN_HOLD` süresince `Regene` gönderilmez; NP 0 ise `Regene` zaten gitmez (`no_np`, MEC-DTH-09). Bekleme ve CLI-14 etkileşimi çalışma zamanında doğrulanır.
7. F4-33 doğrulaması: `BotPHD_*` diriltebilir, `BotPHB_*` ağaç yetersiz (KI-016); El Morad kimlikleri `+100000`.

**Yazım turunda yeniden doğrulanacak referanslar** (okuma: `gece/2026-10-02` @ `f4daa27`):

- `docs/07_PRIEST_BEHAVIOR.md:191-203` (§10: koşul tablosu `:195-201`, akış `:203`), `:46` (`P-PRI-RES-SAFE-RADIUS` 15 m), `:42` (`P-PRI-MP-RESERVE` 1100), `:255` (taş yok ⇒ respawn sinyali), `:268-280` (T-PRI-08, AC-PRI-08).
- `docs/09_PARTY_COORDINATION_AND_TARGET_SELECTION.md:192-207` (§9 ölüm akışı, `P-PTY-RES-WAIT` 12 sn, `P-PTY-REINTEGRATE-MAX` 6 sn).
- `docs/08_MAGE_BEHAVIOR.md:19-27` (diriltme/respawn/summon tablosu: diriltilen HP dolu, MP 0, buff'lar sıfır, blink yok).
- `docs/05_SKILL_CATALOG_AND_COMBAT_RULES.md:128` (Resurrection of love/grace/favors `112733/112742/112754`, MP 400/600/800, recast 25 sn, taş 4/10/30 hedeften), `docs/03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md:140` (MEC-MAG-20), `:296-308` (MEC-DTH-01..09), `:393` (CLI-14).
- `docs/adr/ADR-0018-…md` Ek 9 (diriltme dilimi, taş hedeften).
- `BotCore/Perception.h`: `TeamMemberView.dead` (`:1454`), `kStatusCured`/`ClearTarget` (ölümde buff temizliği, MEC-DTH-01); `BotCore/BotCombat.h:401-417` (`CastResurrectionSupported`, `kResurrectionStoneItem = 379006000`).

## 1. Amaç

P-HD'nin ölü bir üyeyi **ne zaman güvenle diriltebileceğini** (`docs/07` §10 koşul tablosu: ceset ≤ 11 m, düşman melee/alan skill'i yok, kendi HP/MP, taş, yerel kazanç), **hangi diriltme skill'ini** (love/grace/favors; taş stoğu ve 25 sn recast'e göre) seçeceğini, ölen botun **bekleyip beklemeyeceğini** (`RESPAWN_HOLD` 12 sn) ve **sonucun nasıl okunacağını** (`effected` ≠ dirildi) saf mantık olarak eklemek; diriltme olmazsa respawn → summon akışına (F7-05) devri belirlemek.

## 2. Bağlam (okunması zorunlu)

- `docs/07` §10, §15; `docs/09` §9; `docs/08` §2; MEC-MAG-20, MEC-DTH-01..09, CLI-14; ADR-0018 Ek 9; F4-33 planı/raporu (`plans/F4-33-aksiyon-yurutucu-diriltme.md`).
- F7-01 `ReservationTable` (`RES_RES`, `HasForeignOpen`), F7-05 `MemberStatus`/`BotState`.
- AC-LRN-03: yalnızca gözlem sözleşmesi; ölü hedefin taş stoğu **yalnızca bot ise blackboard'dan** (`P` sınıfı), insanda bilinmez.
- `docs/03` §16: düşmanın MP'si/cooldown'ı kullanılmaz; düşman melee/alan skill'i yalnızca konum ve skill olaylarından.

## 3. Kapsam

**Yapılacaklar** (`BotCore/PriestRes.h` yeni; yalnızca standart kütüphane + `TeamBlackboard.h`/`PriestCoord.h` include'u; sunucu başlığı/global-static yok; sabit boyutlu; ASCII + CRLF; yorumlar İngilizce)

### 3.1 Sabitler, skill seçimi, sonuç

1. **Sabitler** (`[Ö]`; kaynak yorumlu): `kResRangeM = 11.0f` (`docs/07:197`), `kResSafeRadiusM = 15.0f` (`P-PRI-RES-SAFE-RADIUS`), `kResAreaRadiusM = 20.0f` ve `kResAreaSkillMs = 3000` (`docs/07:198`), `kResSelfHpPct = 60`, `kResReserveMp = 1100` (`[A]`, §0 madde 3), `kResWaitMs = 12000` (`P-PTY-RES-WAIT`), `kResCastMs = 1500`, `kResRecastMs = 25000` (`docs/05:128`), `kResLocalAdvantagePct = 50` (`docs/07:201`: görünür düşman ≤ canlı sayımızın yarısı), `kResVerifyMs = 3000` (`[A]`), `kResUnsafeMs = 5000` (AC-PRI-08). Skill kimlikleri (`docs/05:128` `[V]`): `kSkillResLove = 112733` (MP 400, taş 4), `kSkillResGrace = 112742` (MP 600, taş 10), `kSkillResFavors = 112754` (MP 800, taş 30), El Morad `+100000`.
2. **`struct ResSkillInfo { uint32_t skillId; int32_t mp; int stones; }`** ve `ResSkillInfo ResSkillAt(int index, bool elmorad)` (`0` love, `1` grace, `2` favors).
3. **`uint32_t ChooseResSkill(bool stockKnown, int stock, const uint32_t recastRemainingMs[3], int32_t selfMp, bool elmorad)`**: sırayla love → grace → favors; **ilk** `recastRemainingMs[i] == 0` ve (`!stockKnown` veya `stock >= taş[i]`) ve `selfMp >= mp[i]` olan döner; hiçbiri ⇒ `0`. Gerekçe: en ucuz skill taşı ve MP'yi korur (PvP'de EXP iadesi önemsiz, `docs/07:203`); 25 sn recast iki ölü üye için love → grace sıralamasını doğurur; bilinmeyen stok (insan) ⇒ love denenir.
4. **`class ResAttemptMemory`** (sabit 8 hedef, kopyalanabilir): `MarkAttempted(int16_t target, uint64_t nowMs)`, `MarkFailed(int16_t target)`, `bool MayAttempt(int16_t target) const` (`docs/07:200`: insan hedefte **fail olursa tekrar denenmez**; fail yoksa serbest), `Clear()`.
5. **Sonuç:** `enum ResResult { RES_PENDING = 0, RES_OK, RES_FAILED }` ve `ResResult ClassifyRes(bool actionEffected, bool targetAliveObserved, uint32_t sinceEffectMs)`: `!actionEffected` ⇒ `RES_FAILED`; `targetAliveObserved` ⇒ `RES_OK`; `sinceEffectMs >= kResVerifyMs` ve canlı gözlenmedi ⇒ `RES_FAILED` (taş eksik olabilir: yayın yine gider, ADR-0018 Ek 9); aksi `RES_PENDING`. **`effected` tek başına `OK` değildir.** `bool IsUnsafeRes(uint64_t resSuccessMs, uint64_t deathMs)` = `deathMs >= resSuccessMs && deathMs - resSuccessMs <= kResUnsafeMs` (AC-PRI-08 sayacı; çalışma zamanı ölçümü F7-08).

### 3.2 Güvenlik kapıları

1. **`struct ResCtx`** (çağıranın değer kopyası): `BotRole selfRole; bool selfAlive; int32_t selfHp, selfMaxHp, selfMp; float corpseDistM; float nearestEnemyMeleeDistM; uint32_t msSinceEnemyAreaSkillNearCorpse; bool targetIsBot; bool stockKnown; int stock; uint32_t recastRemainingMs[3]; int ownAlive, visibleEnemies; bool fightStopped; bool mayAttempt; bool elmorad; int32_t reserveMp;` (`nearestEnemyMeleeDistM` **düşman melee** sınıflı görünür birimlerin cesede mesafesi; yoksa `1e9f`).
2. **`struct ResVerdict { bool allowed; uint8_t failedMask; uint32_t skillId; }`** ve **`ResVerdict EvaluateRes(const ResCtx &)`** (`docs/07:195-201`; her bit ayrı kapı, hepsi bağımsız hesaplanır): bit0 **rol/sağ-kalma**: `selfRole == BotRole::PriestHealDebuff` (curse ağacı, `docs/04` §5.3; P-HB ağaç yetersiz, KI-016) ve `selfAlive`; bit1 **ceset menzili**: `corpseDistM <= kResRangeM`; bit2 **güvenli alan**: `nearestEnemyMeleeDistM >= kResSafeRadiusM` (`[A]` sınır dahil) ve `msSinceEnemyAreaSkillNearCorpse >= kResAreaSkillMs`; bit3 **kendi durumu**: `selfHp * 100 >= kResSelfHpPct * selfMaxHp` ve `selfMp >= 800 + reserveMp` (`[A]`, §0 madde 3; `selfMaxHp <= 0` ⇒ ret); bit4 **taş/skill**: `ChooseResSkill(...) != 0` ve `mayAttempt`; bit5 **yerel kazanç**: `fightStopped || visibleEnemies * 100 <= kResLocalAdvantagePct * ownAlive` (`ownAlive == 0` ⇒ ret); `allowed = (failedMask == 0)`, `skillId` yalnızca `allowed` iken dolu.
3. **`ResVerdict`'in yan etkisi yoktur**; rezervasyon yazımı (`ReservationTable::Create(RES_RES, target, self, skillId, 0 /*amount*/, nowMs, kResCastMs)`) ve atış çağıranındır. **Not:** `RES_RES` kaydı `amount` taşımaz; F7-01 `Create`'in `amount <= 0` kuralı yalnızca `RES_HEAL` içindir (F7-01 §3.4: `heal için`), bu yüzden `RES_RES` için `0` kabul edilir.
4. **`bool ResLikely(const ResCtx &)`** (`docs/09:196` `res_conditions_likely`, `[A]` tanım): `EvaluateRes` kapılarından **bit1 (ceset menzili) hariç** hepsi geçiyorsa `true` (P-HD ceseti dolaşarak yaklaşabilir).

### 3.3 Ölüm akışı ve bekleme (`docs/09` §9)

1. **`enum DeathPlan : uint8_t { DP_HOLD_FOR_RES = 1, DP_RESPAWN }`** ve **`DeathPlan PlanOnDeath(BotRole role, bool hdAlive, bool resLikely)`**: `role` priest **değil** (`BotRole::PriestHealDebuff`/`BotRole::PriestHealBuff` dışında) **ve** `hdAlive` **ve** `resLikely` ⇒ `DP_HOLD_FOR_RES`, aksi `DP_RESPAWN` (`docs/09:196-199`; priest ölümü doğrudan respawn: `[A]` harfiyen, §0 madde 2).
2. **`bool HoldExpired(uint64_t deathSeenMs, uint64_t nowMs)`** = `nowMs >= deathSeenMs + kResWaitMs` (saat geri giderse `false`); **`bool ShouldSendRegene(DeathPlan, bool holdExpired, bool resurrectedObserved, uint32_t sinceDeathMs)`**: `resurrectedObserved` ⇒ `false` (zaten dirildi: `WIZ_REGENE` gönderilmez); `DP_RESPAWN` ⇒ `sinceDeathMs >= 3000` (CLI-14 `dead_wait`, `docs/03:393`); `DP_HOLD_FOR_RES` ⇒ yalnızca `holdExpired` ve `sinceDeathMs >= 3000` (guard ayrıca `no_np` vb. uygular; bu fonksiyon yalnızca **karar** katmanıdır).
3. **`struct PostResPlan { BotState state; bool buffMissing; bool mpPotFirst; }`** ve `PostResPlan AfterResurrection()`: `BotState::Combat`, `buffMissing = true` (MEC-DTH-01: ölümde tüm buff'lar silinir), `mpPotFirst = true` (diriltilen **MP 0** ile başlar, `docs/08:23`; blink yok, MEC-DTH-08); **`BotState::Respawned`/`READY_FOR_SUMMON` yoluna girmez** (`docs/07:193`: diriltme ≠ respawn ≠ summon).
4. **Diriltme başarısızlığı ⇒ respawn sinyali** (`docs/07:255`): `DeathPlan OnResFailed()` her zaman `DP_RESPAWN` döner (bekleme sürse de bırakılır; ardından `NextStateAfterRespawn` F7-05 summon akışı). İşlev yalnızca bu devri açık ve testli yapar.

### 3.4 Birim testleri (adlar bağlayıcı; `Tests/BotCoreTests/PriestResTests.cpp`)

Taban fikstür `SafeRes()`: P-HD canlı, HP `1000/1000`, MP `3000`, ceset 5 m, en yakın düşman melee 40 m, düşman alan skill'i 10000 ms önce, hedef bot (stok `30`, bilinir), recast'ler `0`, `ownAlive 6`, `visibleEnemies 3`, `fightStopped false`, `mayAttempt true`, Karus.

1. `Res_Baseline_Allowed` (`failedMask == 0`, `skillId == 112733`).
2. `Res_NotHdRole_Denied` (P-HB; bit0); `Res_SelfDead_Denied`.
3. `Res_CorpseRange_Boundary` (11,0 m geçer; 11,1 m ret, bit1).
4. `Res_EnemyMeleeWithin15m_Denied` (14,9 m ret; 15,0 m geçer `[A]`); `Res_EnemyAreaSkillNearCorpse_Denied` (2999 ms ret; 3000 ms geçer).
5. `Res_SelfHpBelow60_Denied` (`599/1000` ret; `600/1000` geçer); `Res_ZeroMaxHp_Denied`.
6. `Res_SelfMpBelowCostPlusReserve_Denied` (MP 1899 ret; 1900 geçer; `reserveMp` parametresi değişince eşik değişir).
7. `Res_BotStonesBelowFour_Denied` (stok 3 ret; 4 geçer, `112733`); `Res_StockKnownZero_Denied`.
8. `Res_HumanUnknownStock_AttemptsLove` (`stockKnown false` ⇒ geçer, `112733`); `Res_HumanFailedOnce_NotRetried` (`ResAttemptMemory::MarkFailed` ⇒ `MayAttempt false` ⇒ bit4 ret; başka hedef etkilenmez).
9. `Res_NoLocalAdvantage_Denied` (`ownAlive 8`: `visibleEnemies 4` geçer, `5` ret); `Res_FightStopped_AllowedDespiteEnemies`; `Res_OwnAliveZero_Denied`.
10. `Res_FailedMask_ReportsAll` (bit2 + bit3 + bit5 birlikte ⇒ `0b101100`).
11. `ResSkill_LoveFirst` (stok 4..29 ⇒ `112733`); `ResSkill_LoveInRecast_FallsToGrace` (stok 10, love recast ⇒ `112742`); `ResSkill_GraceNeedsTenStones` (stok 9, love recast ⇒ `0`); `ResSkill_FavorsOnlyWhenLoveAndGraceUnavailable` (stok 30, ikisi recast ⇒ `112754`, MP ≥ 800); `ResSkill_AllInRecast_None`; `ResSkill_MpTooLowForGrace_Skips` (MP 500 ⇒ love `400` ok); `ResSkill_ElMoradIds` (`+100000`); `ResSkill_UnknownStock_Love`.
12. `Intent_ReservationBlocksSecondHd` (F7-01 tablosunda `RES_RES` yazılır ⇒ `HasForeignOpen` diğer sahip için `true`); `Intent_ClosesWhenTargetAlive` (`OnTargetGone` değil `Complete`; kayıt kapanır).
13. `ResLikely_IgnoresCorpseDistance` (ceset 40 m, diğer kapılar geçer ⇒ `true`; MP düşük ⇒ `false`).
14. `Death_NonPriestWithHdAlive_Likely_HoldsFor12s` (W-P/M-F ⇒ `DP_HOLD_FOR_RES`); `Death_Priest_RespawnsImmediately` (`[A]` harfiyen); `Death_HdDead_Respawns`; `Death_NotLikely_Respawns`.
15. `Hold_ExpiresAt12s` (11999 ms `false`, 12000 `true`); `Hold_ClockBackwards_NotExpired`.
16. `Regene_DuringHold_NotSent` (`DP_HOLD_FOR_RES`, süre dolmadı ⇒ `false`); `Regene_AfterHoldExpired_Sent` (`sinceDeath ≥ 3000`); `Regene_RespawnPlan_WaitsDeadWait3s` (2999 ms `false`, 3000 `true`); `Regene_ResurrectedDuringHold_NeverSent`.
17. `ResResult_EffectedAlone_NotProof` (`effected`, canlı gözlenmedi, 100 ms ⇒ `RES_PENDING`; 3000 ms ⇒ `RES_FAILED`); `ResResult_AliveObserved_Ok`; `ResResult_NotEffected_Failed`.
18. `PostRes_MpZeroNeedsPotAndBuffs` (`state BotState::Combat`, `buffMissing`, `mpPotFirst`; `BotState::ReadyForSummon` değil).
19. `ResFailed_AlwaysRespawns` (`OnResFailed()` ⇒ `DP_RESPAWN`); `UnsafeRes_Within5s` (4999 ve 5000 ms `true`, 5001 `false`, ölüm diriltmeden önce `false`).
20. `Res_Determinism` (aynı bağlam iki kez ⇒ aynı `ResVerdict`).

**Kapsam dışı (yapılmayacak)**

- Sunucu bağlaması: `ResCtx` kurulumu (ceset konumu, düşman melee listesi, `SkillEventRing`'ten alan skill'i sorgusu, `MemberStatus` stoku), `CastStart/CastEffect` ile gerçek diriltme atışı, `WIZ_REGENE` gönderimi ve `RESPAWN_HOLD` durum makinesi, telemetri (`RES_*`, MET-PTY-01), oyun içi doğrulama (T-PRI-08, AC-PRI-08, T-IGT-PRI-01): **F7-08**.
- Taş yeniden stoklama (`db/004`, `tools/bot-refill.sh`; F4-40) ve taşın "tek bot en çok bir favors ya da yedi love" sınırı yönetimi (`ChooseResSkill` yalnızca bir kararı verir; stok **düşüşünü izleme** F4-60/`MemberStatus` ve F7-08 işi).
- Summon akışı (F7-05), buff/cure (F7-06), respawn sonrası yürüyerek regroup, takım modu (`RETREAT/REGROUP`), tam yenilgi (ayrı F7 planları).
- Yürütücü değişikliği (`ActionExecutor.cpp`/`BotCombat.h`), diriltme dışı Type5 (Bless of God, `RESURRECTION_SELF`), `docs/` değişikliği (Claude), `GameServer/`, `shared/`.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/PriestRes.h` | yeni | §3 |
| `Tests/BotCoreTests/PriestResTests.cpp` | yeni | §3.4 |
| `BotCore/BotCore.vcxproj` | değiştir | `<ClInclude Include="PriestRes.h" />` |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | `<ClCompile Include="PriestResTests.cpp" />` |

`TeamBlackboard.h`, `PriestCoord.h`, `Perception.h`, `BotCombat.h` **değişmez**. Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F7-07 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; sunucuları durdur; test sayısını not et.
2. `PriestRes.h` §3.1-§3.3 + testler 1-20 (sırayla: sabitler/skill seçimi, kapılar, ölüm akışı); derle/çalıştır.
3. İki `.vcxproj` kaydı; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu (açık soruları §0 madde 2-5'e bağla).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; test sayısı plan başındakinden **§3.4'teki `TEST_CASE` sayısı kadar** fazla (≈ 38); her ad `[ OK ]`; F7-01..F7-06 testleri değişmeden geçer
- [ ] K3 (T-PRI-08, birim düzeyi): `Res_*` kapı testlerinin her biri tek kapıyı çevirir ve sınır değerlerini doğrular; `Res_FailedMask_ReportsAll` geçer. **AC-PRI-08 (güvensiz diriltme ≤ %10) çalışma zamanında F7-08'dedir**; bu plan yalnızca `IsUnsafeRes` sayacını sağlar
- [ ] K4 (sonuç okuma): `ResResult_EffectedAlone_NotProof` ve `ResResult_AliveObserved_Ok` geçer (ADR-0018 Ek 9)
- [ ] K5 (akış): `Death_*`, `Hold_*`, `Regene_*`, `PostRes_*`, `ResFailed_AlwaysRespawns` geçer; `docs/09:196` harfiyen uygulanır ve Uygulayıcı Raporu §0 madde 2'yi açık soru olarak listeler
- [ ] K6 (skill verisi): `112733/112742/112754` ve MP/taş değerleri (400/600/800; 4/10/30) `docs/05:128` ile birebir; recast `25000`
- [ ] K7 (saflık/adalet): yeni başlıkta `GameServer|windows.h|stdafx|shared/` yok; `new `/`malloc`/`std::vector`/`std::map`/`printf`/`rand(` yok; global/static değişken yok; insan hedefin taş stoğunu okuyan hiçbir yol yok (`stockKnown == false` ⇒ deneme); düşman MP/cooldown alanı yok
- [ ] K8 (kapsam/biçim): `git diff --stat gece/2026-10-02...bot/F7-07` yalnızca §4'teki 4 dosya + plan; `TeamBlackboard.h`/`PriestCoord.h`/`Perception.h`/`BotCombat.h`/`GameServer/`/`shared/`/`docs/` farkı 0; ASCII + CRLF; `git diff --check` boş
- [ ] K9 (belgeleme): `[A]` kurallar (MP rezervi 1100, sınırların dahilliği, `res_conditions_likely` tanımı, doğrulama penceresi 3000 ms) kodda `[A]` yorumuyla; Uygulayıcı Raporu açık sorularda listeler

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Res_|ResSkill_|Intent_|ResLikely_|Death_|Hold_|Regene_|ResResult_|PostRes_|ResFailed_|UnsafeRes_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F7-07
git diff gece/2026-10-02...bot/F7-07 -- BotCore/TeamBlackboard.h BotCore/PriestCoord.h BotCore/Perception.h BotCore/BotCombat.h | wc -l   # 0
file BotCore/PriestRes.h Tests/BotCoreTests/PriestResTests.cpp
git diff --check gece/2026-10-02...bot/F7-07
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; §2.3 (oyun mekaniğini değiştirme); §2.5 (bota avantaj yok; diriltme insan priest'in yapabildiği bir Type5 skill'idir, CLI-14 respawn tuşu bekleme kuralı korunur, bot ölü hedefin taşını kendi stoğundan değil sunucu kuralıyla hedeften harcatır).
- Tüm eşikler `[Ö]`, belirtilenler `[A]`; T-PRI-08 sonrası güncellenir, `docs/07`/`docs/09` ile birlikte değişir.
- **Dürüstlük:** bu planın testleri karar mantığını doğrular; "güvenli diriltme" ve "diriltme sonrası ölüm ≤ %10" oyunda ölçülmeden söylenemez. Taş eksik olduğunda `effected` döner ve **MP/skill gider** (F4-33 §sınırlar): `ClassifyRes` bunu `RES_FAILED` yapar, ama MP kaybı önlenemez.
- Gate/Escape/summon (ölü üye summon edilemez, MEC-T8-03) bu planda yoktur; ölü üyeyi **diriltmeden** summon etmeye çalışmak F7-05 SUM-01 tarafından reddedilir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: (boş)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — (boş)
