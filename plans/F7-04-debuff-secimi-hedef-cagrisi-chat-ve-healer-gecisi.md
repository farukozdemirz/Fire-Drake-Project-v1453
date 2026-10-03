# F7-04: Debuff seçimi, başarı doğrulaması, hedef çağrısı + party chat (CLI-18 sınırları) ve healer'a hedef değiştirme emirleri (`BotCore/DebuffCall.h`, `TeamChat.h`, `TeamOrders.h`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F7 — Party koordinasyonu (`docs/17` §2 F7 bloğu; kapılar G7a ve G7c) |
| Branch | `bot/F7-04 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F7-03** (`TargetCall`, `TeamTarget`, `ScoreTarget` eşiği) · **F7-02** (`StallDecision`: `HEALER_SWITCH/SPLIT/BURST_NOW/BURST_PARASITE/REVERT`) · **F7-01** (`TeamBlackboard` iskeleti) · **F6-01** (`BotRole`, `ReasonCode`, `BrainParams`) · **F6-07** (priest solo heal) · **F6-09** (priest buff/cure/debuff, kendine/tek müttefik; **bu an taslağı yok**, `F6-07` §8 "Çelişkiler" 1 onu F6-09 olarak adlandırır: P-HD'nin tek hedefli debuff döngüsü ve `ChooseDebuff` benzeri seçim orada olacaksa **bu plan onu çağırır/genişletir, ikinci seçim tablosu yazmaz**; çakışırsa F6-09 kazanır ve bu plan yalnızca takım katmanını (çağrı, chat, emirler) tutar) · F4-28 `KAPANDI` (Type4 debuff atılabilir), F4-11 `KAPANDI` (party chat + `CheckChat`), F4-44 `KAPANDI` (curse skill'leri ölçüldü, `docs/05` §9.3), F4-53 `KAPANDI` + **F4-60/F4-61** (gözlenen durum: debuff başarısı/cure gözlemi için sunucu bağlaması) |
| İlgili gereksinim / kabul | REQ-PRI-08, REQ-PRI-09, REQ-PTY-06, REQ-PTY-11; AC-PRI-06 (MET-DEBUFF-04 yanlış çağrı = 0; MET-CHAT-01 limit ihlali = 0: **çalışma zamanı F7-08**, burada birim düzeyi), AC-PTY-07, MET-DEBUFF-01/02/03/04, MET-CHAT-01, MET-STALL-01; T-PRI-06, T-PTY-09 (`[A]` istemcide chat görünümü: **çalışma zamanı**); `docs/07` §9, `docs/09` §6.2, §7, §12; `docs/03` CLI-18 |
| Tahmini büyüklük | M–L (3 yeni başlık + 3 yeni test dosyası + 2 `.vcxproj` = 8 dosya, ≈ 40 test; yazım turunda **F7-04a** (seçim+çağrı+chat) / **F7-04b** (emirler) olarak bölünebilir) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 kapanmadan HAZIR yapılmaz) |

---

## 0. Neden TASLAK

**HAZIR yapma ön koşulları:**

1. F7-02 ve F7-03 `KAPANDI`; **F6-09** yazılmış ve `KAPANDI` olmalı: P-HD'nin takımsız debuff seçimi orada tanımlanmışsa §3.1 madde 3'teki `ChooseDebuff` **onun üstüne takım girdileri (ortak hedef, küme sayısı) ekler**, yeniden yazılmaz; F6-09 yoksa `ChooseDebuff` bu planda tanımlanır ve F6-09 onu kullanır (sıra yazım turunda netleşir).
2. **F4-60/F4-61** `KAPANDI`, çalışma zamanında doğrulanmış: botun kendi debuff'ı hedefte `ObservedStatusTable`'a **giriyor mu** (kendi `EFFECTING` yayını bota geliyor mu; kaynak sınıfı `E`), `cure` olayı `kStatusCured` üretiyor mu. Girmiyorsa "debuff başarısı" kanıtı yalnızca `ACTION_RESULT` olur ve `docs/07:182` (iki koşul) değişmeli (Claude).
3. **Counter Curse / Curse Refraction** `BuffType` kimliği `MAGIC_TYPE4` verisinden doğrulanmalı (bu taslakta **uydurulmadı**): `docs/07:176`, `docs/09:121`; gözlenebilirlik F4-60 çalışma zamanından. Doğrulanamazsa `ChooseDebuff`'ın `counterCurseSeen` girdisi `false` sabitlenir ve plan bunu açıkça yazar.
4. **Chat karakter kümesi (Q-16, T-PTY-09 `[A]`):** `CLI-18` (`docs/03:397`) botu yalnızca yazdırılabilir ASCII ile sınırlar (`IsValidChatText`, `BotCore/BotCombat.h:956-982`; sunucu denetlemez). `docs/09:230-238`'deki `GERİ` bu yüzden **gönderilemez**; bu plan `GERI` (ASCII) kullanır ve **Claude `docs/09` §12'yi buna göre güncellemeli** (Türkçe karakter istemcide görünüyor mu: Q-16 ölçülene kadar kapalı). Karakter adlarının her zaman ASCII olduğu **doğrulanmadı** `[A]`: ASCII dışı ad ⇒ yalnızca blackboard çağrısı (chat atlanır, sayaç).
5. `docs/07:183` "taktiksel anlam: hedef ortak hedef adayıdır (skor ≥ eşik) ve ulaşılabilir üye sayısı ≥ 2": **eşik değeri dokümanda yok**. Claude `docs/09` §5.2'ye `P-TGT-CALL-MIN` (öneri: ortak hedef seçimi için en iyi skorun %80'i `[A]`) ekleyemezse plan HAZIR olmaz; bu taslakta parametre `callScoreMin` olarak çağıranın verdiği değerdir.
6. Debuff sırası (§3.1 madde 3) `docs/07:169-176` tablosunun **öncelik sırasını** vermediğinden `[A]` bir sıra seçer; Claude onaylamalı.

**Yazım turunda yeniden doğrulanacak referanslar** (okuma: `gece/2026-10-02` @ `f4daa27`):

- `docs/07_PRIEST_BEHAVIOR.md:165-189` (§9 debuff seçimi, çağrı protokolü 1-6, `P-TEAM-HUMAN-CALLS`), `:148` (aynı debuff aktifken tekrar yok, kalan süre > 5 sn), `:253` (debuff fail ⇒ "bağışık" 10 sn), `:49-50` (`P-PRI-CALL-DEDUP` 8 sn, `P-PRI-CHAT-RATE`).
- `docs/09_PARTY_COORDINATION_AND_TARGET_SELECTION.md:157-166` (karar tablosu: `HEALER_SWITCH` emirleri `:160`, `SPLIT` `:161`, iki healer `:162`, `BURST_NOW` `:163`), `:170-177` (§7 debuff ile saldırının zamanlanması; "debuff yoksa lider `LEADER` ile çağırır"), `:227-238` (§12 chat protokolü ve sınırları).
- `docs/05_SKILL_CATALOG_AND_COMBAT_RULES.md:47-55` (BuffType grupları: AC 2, HP_MP 1, DAMAGE 4, ATTACK_SPEED 5), `:122-127` (Malice `112703`, Torment `112757`, Parasite `112745`, Massive `112760`, Slow `112724`, Sweep mana `112736`; El Morad `+100000`), `:170` (SK-04), `:228-243` (F4-44 ölçümü).
- `docs/03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md:397` (CLI-18), `:342-344` (MEC-CHT-01..03: oran sınırı yok, `+` GM öneki).
- `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md:163-166` (MET-DEBUFF-01..04), `:194` (MET-CHAT-01).
- `BotCore/BotCombat.h`: `kChatGapMs/kChatDupMs/kChatPerMinute` (`:950-953`), `IsValidChatText`, `ChatTextHash`, `ChatRateWindow`, `ChatCheck`, `CheckChat` (`:956-1075`); `BotCore/Perception.h`: `ObservedStatusTable::Find` (`:2014-`), `kStatusCured`, `StatusRemainingMs`.

## 1. Amaç

P-HD'nin **ne atacağını** (debuff seçimi), atışın **başarılı sayılıp sayılmayacağını** (sonuç paketi + gözlenen durum), başarılıysa ve taktiksel anlamlıysa **kime nasıl çağıracağını** (blackboard `TargetCall` + sınırlara uyan party chat), ve `HEALER_SWITCH`/`SPLIT`/`BURST_*` kararlarının **rol bazlı emirlere** çevrilmesini saf mantık olarak eklemek. Blackboard her zaman birincil kanaldır; chat yalnızca insanlara görünürlüktür ve hiçbir karar chat'e bağlı değildir (`docs/07:180`).

## 2. Bağlam (okunması zorunlu)

- `docs/07` §9 ve `docs/09` §6.2, §7, §12 (yukarıdaki satırlar); `docs/03` CLI-18, MEC-CHT-01..03.
- F7-03 `TeamTarget::Post/View/MarkWeakened`; F7-02 `StallDecision`.
- AC-LRN-03 (`docs/14` §5.2): debuff başarı kararı yalnızca sunucu sonuç paketi ve gözlenen olaydan çıkar; hedefin gerçek buff listesi okunmaz.
- ADR-0018 Ek 20 (`:97-98`): debuff sonucu yalnızca sunucunun sonuç paketinden okunur; Slow'da direnç zarı `missed`/`srv_fail` üretirse **bulgudur** (SK-04) ⇒ `Missed` sonucu çağrı üretmez.

## 3. Kapsam

**Yapılacaklar** (hepsi yeni başlık; yalnızca standart kütüphane + `TeamBlackboard.h`, `Perception.h`, `BotCombat.h` include'ları; sunucu başlığı/global-static yok; sabit boyutlu; ASCII + CRLF; kod yorumları İngilizce)

### 3.1 `BotCore/DebuffCall.h`

1. **Skill kimlikleri ve BuffType sabitleri** (`docs/05:122-127`, `[V]`): `kSkillMalice = 112703`, `kSkillTorment = 112757`, `kSkillParasite = 112745`, `kSkillMassive = 112760`, `kSkillSlow = 112724`, `kSkillSweepMana = 112736`; `uint32_t DebuffSkillId(DebuffKind, bool elmorad)` (`+100000`); `kBuffTypeHpMp = 1`, `kBuffTypeAc = 2`, `kBuffTypeDamage = 4`, `kBuffTypeAttackSpeed = 5` (`docs/05:47-55`). **Counter Curse/Curse Refraction BuffType sabiti yoktur** (§0 madde 3).
2. **`enum DebuffKind : uint8_t { DEBUFF_NONE = 0, DEBUFF_MALICE, DEBUFF_TORMENT, DEBUFF_PARASITE, DEBUFF_MASSIVE, DEBUFF_SLOW, DEBUFF_SWEEP_MANA }`** ve `struct DebuffCtx { bool targetIsSharedTarget; bool counterCurseSeen; bool immuneMarked; uint32_t acDebuffRemainingMs; uint32_t acDebuffSkill; uint32_t hpDebuffRemainingMs; uint32_t dmgDebuffRemainingMs; uint32_t atkSpeedDebuffRemainingMs; int enemiesNear10mOfAim; bool aimInRange; bool hpBuffSeen; bool targetIsHighHpWarrior; bool enemyPressuresOurSoft; bool enemyPriestMpLowSign; }` (çağıranın `ObservedStatusTable`/algıdan doldurduğu değer kopyası; **düşman MP'si okunmaz**: `enemyPriestMpLowSign` yalnızca gözlenen pot/heal yoğunluğundan türetilir, `docs/07:175`).
3. **`DebuffKind ChooseDebuff(const DebuffCtx &)`** — sıra `[A]` (§0 madde 6), ilk uyan kazanır: (a) `counterCurseSeen` veya `immuneMarked` ⇒ `NONE` (`docs/07:176,253`); (b) `enemiesNear10mOfAim >= 3 && aimInRange` ve hedefte etkin `Torment` yoksa (`acDebuffRemainingMs > 5000 && acDebuffSkill` Torment) ⇒ `TORMENT` (Malice varsa ezer: `docs/07:172`); (c) `acDebuffRemainingMs <= 5000` (etkin AC debuff yok) ve ortak hedef ise ⇒ `MALICE` (`docs/07:169-171`); (d) `(hpBuffSeen || targetIsHighHpWarrior) && hpDebuffRemainingMs <= 5000` ⇒ `PARASITE` (`docs/07:173`; `docs/09:166` "Parasite ve AC debuff'ı patlamadan önce"); (e) `enemyPressuresOurSoft`: `dmgDebuffRemainingMs <= 5000` ⇒ `MASSIVE`, değilse `atkSpeedDebuffRemainingMs <= 5000` ⇒ `SLOW` (`docs/07:174`; Slow yalnızca istemcide etkili, `docs/05:171` SK-05); (f) `enemyPriestMpLowSign` ⇒ `SWEEP_MANA` (düşük öncelik, `docs/07:175`); aksi `NONE`. "Etkin" tanımı: kalan süre > 5000 ms (`docs/07:148`).
4. **Sonuç doğrulaması:** `enum DebuffOutcome { DOUT_OK = 0, DOUT_RESULT_FAILED, DOUT_NOT_OBSERVED, DOUT_MISSED }` ve `DebuffOutcome EvaluateDebuffResult(bool actionEffected, bool actionMissed, bool statusObserved)`: `actionMissed` ⇒ `DOUT_MISSED`; `!actionEffected` ⇒ `DOUT_RESULT_FAILED`; `effected && !statusObserved` ⇒ `DOUT_NOT_OBSERVED` (çağrı üretmez, `docs/07:182` iki koşul); ikisi de ⇒ `DOUT_OK`.
5. **`class DebuffMemory`** (sabit 8 hedef, kopyalanabilir): `MarkImmune(int16_t target, uint64_t nowMs)` (`nowMs + 10000`; `docs/07:253`), `bool IsImmune(target, nowMs)`, `bool ReapplyAllowed(int16_t target, uint32_t skillId, uint32_t recastMs, uint64_t nowMs)` (`docs/07:186` cure sonrası yeniden uygulama: Malice recast `7400` ms); saat geri giderse güvenli.
6. **Çağrı politikası:** `enum CallDecision { CALL_NONE = 0, CALL_POST_ONLY, CALL_POST_AND_CHAT }`; `struct CallCtx { DebuffOutcome outcome; float targetScore, callScoreMin; int reachableMembers; bool hasHdAlive; bool leaderCall; bool nameAscii; bool chatGateAllows; uint64_t nowMs; int16_t target; }`; `CallDecision DecideCall(const CallCtx &, CallDedup &)` (kurallar sırayla): (a) `!leaderCall && outcome != DOUT_OK` ⇒ `CALL_NONE` (**MET-DEBUFF-04 = 0**: başarısız/gözlenmemiş/ıska debuff çağrı üretmez); (b) taktiksel anlam: `targetScore >= callScoreMin && reachableMembers >= 2`, değilse `CALL_NONE` (`docs/07:183`); (c) blackboard her zaman (`CALL_POST_ONLY` en azından); (d) chat yalnızca `nameAscii && chatGateAllows` ve **aynı hedefe `P-PRI-CALL-DEDUP` 8000 ms içinde chat yok** (`CallDedup`: hedef başına son chat zamanı, sabit 8 hedef; blackboard çağrısı yenilenebilir ama chat tekrarlanmaz); `leaderCall` (P-HD yok/ölü, `docs/09:177`): `reason = TCALL_LEADER` ve "kısa patlama penceresi" bayrağı `shortBurstWindow = true` (döner `struct CallResult { CallDecision decision; TargetCallReason reason; bool shortBurstWindow; int skipReason; }`; `skipReason`: `SKIP_NONE, SKIP_NOT_TACTICAL, SKIP_FAILED, SKIP_DEDUP, SKIP_CHAT_GATE, SKIP_NON_ASCII`).
7. `bool ShouldMarkWeakened(bool cureObservedOnTarget)` ve çağıranın `TeamTarget::MarkWeakened` (F7-03) çağırması; **yeniden uygulama kararı** `DebuffMemory::ReapplyAllowed` (`docs/07:186`).

### 3.2 `BotCore/TeamChat.h`

1. `enum TeamChatKind : uint8_t { CHAT_TARGET = 1, CHAT_HEALER, CHAT_BURST, CHAT_REGROUP, CHAT_RETREAT, CHAT_TP }` (`docs/09:229-236`).
2. `int FormatTeamChat(TeamChatKind kind, const char * name, const char * extra, char * out, int cap)`: biçimler `"HEDEF: <ad> (<debuff>)"`, `"HEALER: <ad>"`, `"PATLAT: <ad>"`, `"TOPLAN"`, **`"GERI"`** (ASCII; §0 madde 4), `"TP"`; çıktıya NUL eklenir, yazılan uzunluk döner; çıktı `IsValidChatText` değilse (ASCII dışı ad/ekstra, `+` önek, `> kChatMaxLen`, tampon yetersiz) **`0`** döner ve `out` boş kalır; `cap` taşmaz.
3. `bool NameIsAsciiPrintable(const char * name)` (`DecideCall`'a `nameAscii` girdisi).
4. Gönderim sınırları **burada tekrar uygulanmaz**: `BotCore::CheckChat` (F4-11, `kChatGapMs` 4000, `kChatDupMs` 8000, `kChatPerMinute` 6) `ActionExecutor` tarafında zorunlu emniyet ağıdır; bu plan yalnızca **önceden** aynı kuralı sorgulayıp boşa karar üretmemeyi sağlar: `bool ChatGateAllows(const ChatCheck & c)` = `CheckChat(c) == CHAT_OK` (çağıran `ChatCheck`'i `ChatRateWindow`/son mesaj bilgisinden doldurur; `DecideCall`'ın `chatGateAllows` girdisi budur).

### 3.3 `BotCore/TeamOrders.h`

1. **Rol türü F6-01'in `BotRole`'üdür** (`BotRole::WarriorPressure`, `WarriorGuard`, `PriestHealDebuff`, `PriestHealBuff`, `MageFire`, `MageIce`; `docs/04` §5: 6 profil); **ikinci bir rol enum'u tanımlanmaz.**
2. `enum OrderKind : uint8_t { ORD_NONE = 0, ORD_FOCUS_TARGET, ORD_LOCK_HEALER, ORD_DEBUFF_HEALER, ORD_BURST_TARGET, ORD_PRESSURE_HEALER, ORD_PARASITE_SWEEP, ORD_BURST_NOW, ORD_REVERT_FOCUS }`; `struct RoleOrder { int16_t member; OrderKind kind; int16_t target; uint64_t deadlineMs; }`; `struct OrderMember { int16_t id; BotRole role; bool alive; float distToHealer; }`.
3. `int OrdersForStall(StallDecision d, int16_t healerId, int16_t oldTarget, const OrderMember * m, int n, uint64_t nowMs, RoleOrder * out, int cap)` (`docs/09:157-166`): `HEALER_SWITCH` ⇒ W-P: `ORD_LOCK_HEALER` (Scream/Shock Stun hedefi healer), canlı P-HD: `ORD_DEBUFF_HEALER` (Malice), mage'ler: `ORD_BURST_TARGET` (healer), W-G/P-HB: `ORD_FOCUS_TARGET` (healer; peel ve destek sürer); `SPLIT` ⇒ **healer'a en yakın tek W-P** `ORD_PRESSURE_HEALER` (eşitlikte düşük `id`), diğerleri `ORD_NONE` (hedefte kalır) ve `ORD_BURST_NOW` beklenir; `BURST_NOW` ⇒ patlama yapabilen herkes `ORD_BURST_NOW` (`deadlineMs = nowMs + 2000`); `BURST_PARASITE` ⇒ canlı P-HD `ORD_PARASITE_SWEEP` (Parasite + Sweep mana), sonra patlama; `REVERT` ⇒ herkese `ORD_REVERT_FOCUS` (`target = oldTarget`); `NONE`/`HOLD_LOSING` ⇒ emir yok. P-HD ölü/yoksa `ORD_DEBUFF_HEALER` üretilmez. Çıktı sırası üye `id` artan; `cap` taşmaz.
4. **Chat eşlemesi:** `HEALER_SWITCH` ⇒ lider `CHAT_HEALER`, `BURST_NOW` ⇒ `CHAT_BURST`; çağıranın `FormatTeamChat` + `DecideCall` kapılarıyla birleştirmesi F7-08'dir (bu planda yalnızca eşleme tablosu `ChatKindForDecision(StallDecision)`).

### 3.4 Birim testleri (adlar bağlayıcı)

**`DebuffCallTests.cpp`** (hedef `kE = 3010`, `nowMs = 100000`):

1. `Debuff_CounterCurseSeen_None`; `Debuff_ImmuneMarked_None` (CC/immune diğer koşullardan önce gelir).
2. `Debuff_NoAcDebuff_Malice`; `Debuff_AcDebuffActiveOver5s_NoMalice` (kalan 5001 ms ⇒ Malice yok); `Debuff_AcDebuffRemaining5000_MaliceAllowed` (kalan 5000 ms ⇒ Malice).
3. `Debuff_Cluster3_Torment`; `Debuff_Cluster2_NotTorment`; `Debuff_TormentOverMalice_WhenClusterAndMaliceActive`; `Debuff_TormentActive_NoRepeat`.
4. `Debuff_HpBuffSeen_Parasite_AfterAc` (AC debuff etkin, HP debuff yok ⇒ Parasite); `Debuff_HighHpWarrior_Parasite`; `Debuff_HpDebuffActive_NoParasite`.
5. `Debuff_PressureOnOurSoft_Massive`; `Debuff_MassiveActive_FallsToSlow`; `Debuff_BothActive_None`.
6. `Debuff_PriestMpLowSign_SweepMana_LowestPriority` (başka koşul yokken); `Debuff_NothingApplicable_None`.
7. `Debuff_SkillIds_KarusAndElMorad` (`112703/112757/112745/112760/112724/112736` ve `+100000`).
8. `DebuffResult_EffectedAndObserved_Ok`; `DebuffResult_EffectedButNotObserved_NotOk`; `DebuffResult_SrvFail_NotOk`; `DebuffResult_Missed_NoCall`.
9. `DebuffMemory_ImmuneExpiresAt10s` (`9999` ms bağışık, `10000` ms değil); `DebuffMemory_ReapplyAllowed_AfterRecast` (`7399` ms hayır, `7400` ms evet); `DebuffMemory_Capacity_OldestReplaced`; `DebuffMemory_ClockBackwards_Safe`.
10. `Call_Success_Tactical_PostsAndChats`: `outcome OK`, `score 2,0 >= min 1,6`, `reachable 3` ⇒ `CALL_POST_AND_CHAT`, `reason TCALL_DEBUFF_SUCCESS`.
11. `Call_Failure_NoCallNoChat` (**MET-DEBUFF-04 = 0**: `RESULT_FAILED`, `NOT_OBSERVED`, `MISSED` üçü de `CALL_NONE`, `SKIP_FAILED`).
12. `Call_NotTactical_ScoreBelowMin_NoCall` (`1,5 < 1,6`); `Call_ReachableMembersBelow2_NoCall` (1 üye).
13. `Call_Dedup8s_PostOnlyNoChat`: aynı hedef 7999 ms sonra ⇒ `CALL_POST_ONLY` (`SKIP_DEDUP`), 8000 ms ⇒ `CALL_POST_AND_CHAT`; farklı hedef dedup'a takılmaz.
14. `Call_ChatGateBlocked_PostOnly` (`chatGateAllows false` ⇒ `SKIP_CHAT_GATE`, blackboard çağrısı korunur); `Call_NonAsciiName_PostOnly` (`SKIP_NON_ASCII`); `Call_ChatNeverBlocksBlackboard` (her chat engelinde `decision != CALL_NONE`).
15. `Call_LeaderFallback_WhenNoHdAlive`: `leaderCall`, `hasHdAlive false` ⇒ `reason TCALL_LEADER`, `shortBurstWindow true`, debuff başarısı aranmaz.
16. `Call_WeakenedAfterCure_MarksAndReapplies` (`ShouldMarkWeakened(true)`; `MarkWeakened` sonrası `ReapplyAllowed` recast bekler).
17. `ChatRate_PreCheckMatchesGuard`: `CheckChat` ile aynı `ChatCheck` değerleriyle `chatGateAllows` türetilen yardımcı: 4000 ms boşluk, 6/dk, aynı metin 8000 ms kuralları `CHAT_REJECT_GAP/MINUTE/DUP` ile uyumlu (üç ayrı alt kontrol, tek test).

**`TeamChatTests.cpp`:**

18. `ChatFormat_Hedef`: `"HEDEF: Abc (Malice)"` baytı baytına; `ChatFormat_AllKindsAscii` (altı tür `IsValidChatText == true`, `GERI` ASCII, `GERİ` üretilmez); `ChatFormat_RejectsPlusPrefixAndNonAscii` (`+` ile başlayan ad, `0xC4` bayt ⇒ `0`, `out` boş); `ChatFormat_SmallBuffer_NoOverflow` (cap 8 ⇒ `0`, taşma yok); `ChatFormat_MaxNameLen20_UnderLimit` (20 baytlık ad ≤ 128); `ChatKindForDecision_Mapping`.

**`TeamOrdersTests.cpp`** (üyeler: W-P `2970`, W-P `2971`, W-G `2972`, P-HD `2973`, P-HB `2974`, M-F `2975`, M-I `2976`; healer `3011`):

19. `Orders_HealerSwitch_RolesGetExpectedOrders` (W-P ⇒ `ORD_LOCK_HEALER`, P-HD ⇒ `ORD_DEBUFF_HEALER`, mage ⇒ `ORD_BURST_TARGET`, W-G/P-HB ⇒ `ORD_FOCUS_TARGET`, hepsinin `target == 3011`).
20. `Orders_Split_NearestWpPressuresHealer_OthersHold` (yalnızca en yakın W-P `ORD_PRESSURE_HEALER`; eşit mesafede düşük `id`; diğerleri `ORD_NONE`).
21. `Orders_BurstNow_AllBurstWithin2s` (`deadlineMs == nowMs + 2000`; ölü üye emir almaz).
22. `Orders_BurstParasite_HdParasiteThenSweep` (yalnızca P-HD `ORD_PARASITE_SWEEP`).
23. `Orders_Revert_ReturnsToOldTarget` (herkes `ORD_REVERT_FOCUS`, `target == oldTarget`).
24. `Orders_NoPriestHd_NoDebuffOrder` (P-HD ölü ⇒ `ORD_DEBUFF_HEALER`/`ORD_PARASITE_SWEEP` yok); `Orders_None_And_HoldLosing_NoOrders`; `Orders_CapRespected`; `Orders_Determinism` (üye dizisi karıştırılsa da aynı emir kümesi, `id` artan).

**Kapsam dışı (yapılmayacak)**

- Sunucu bağlaması (gerçek skill atışı, `ObservedStatusTable`'dan `DebuffCtx` kurma, `ActionExecutor` üzerinden `CastStart/Chat` gönderimi, `TeamTarget::Post` çağrısı, telemetri `TARGET_CALL`/`CHAT_SENT`/`DEBUFF_RESULT`, istemcide chat görünümü T-PTY-09, MET-DEBUFF-02 ölçümü): **F7-08**.
- Heal-stall kararının **üretimi** (F7-02) ve hedef skoru (F7-03); regroup/retreat/summon mesajlarının **gönderimi** (`TOPLAN`, `GERI`, `TP` yalnızca biçimlenir; kullanım ilgili planlarda).
- İnsan oyuncu chat çağrılarını ayrıştırma (`P-TEAM-HUMAN-CALLS`, varsayılan kapalı, `docs/07:189`): **yazılmaz**.
- Türkçe/ASCII dışı chat (Q-16, T-PTY-09 sonrası ayrı karar); düşman MP/cooldown okuma (yasak); `ActionExecutor`/`BotFairnessGuard` değişikliği (F4-11 `CheckChat` olduğu gibi kullanılır).
- `docs/` değişikliği (Claude), `GameServer/`, `shared/`.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/DebuffCall.h` | yeni | §3.1 |
| `BotCore/TeamChat.h` | yeni | §3.2 |
| `BotCore/TeamOrders.h` | yeni | §3.3 |
| `Tests/BotCoreTests/DebuffCallTests.cpp` | yeni | testler 1-17 |
| `Tests/BotCoreTests/TeamChatTests.cpp` | yeni | test 18 grubu |
| `Tests/BotCoreTests/TeamOrdersTests.cpp` | yeni | testler 19-24 |
| `BotCore/BotCore.vcxproj` | değiştir | üç `<ClInclude>` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | üç `<ClCompile>` satırı |

`TeamBlackboard.h`, `Perception.h`, `BotCombat.h` **değişmez** (yalnızca include). Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F7-04 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; sunucuları durdur; test sayısını not et.
2. `TeamChat.h` + testler (en küçük, bağımsız); sonra `DebuffCall.h` + testler 1-17; sonra `TeamOrders.h` + testler 19-24.
3. `.vcxproj` kayıtları; derleme ve test (§7).
4. `Durum` → `UYGULANDI`; Uygulayıcı Raporu (açık soruları §0 madde 3-6'ya bağla).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; test sayısı plan başındakinden **§3.4'teki `TEST_CASE` sayısı kadar** fazla (≈ 40); her ad `[ OK ]`; F7-01..F7-03 testleri değişmeden geçer
- [ ] K3 (MET-DEBUFF-04 = 0, birim düzeyi): `Call_Failure_NoCallNoChat` üç başarısızlık türünde `CALL_NONE`; `Call_ChatNeverBlocksBlackboard`; **gerçek maçta ölçüm F7-08'dir**
- [ ] K4 (MET-CHAT-01, birim düzeyi): `ChatRate_PreCheckMatchesGuard` ile `Call_Dedup8s_PostOnlyNoChat`; `FormatTeamChat` çıktılarının tamamı `IsValidChatText == true` (altı tür); `GERI` ASCII, `GERİ` hiçbir yolla üretilmez
- [ ] K5 (CLI-18 çakışmasızlık): `grep -nE 'kChatGapMs|kChatDupMs|kChatPerMinute' BotCore/DebuffCall.h BotCore/TeamChat.h BotCore/TeamOrders.h` yalnızca yorumlarda ya da `CheckChat` çağrısında; sabitler **yeniden tanımlanmaz** (tek kaynak `BotCombat.h`); `BotCombat.h` farkı 0
- [ ] K6 (saflık/adalet): yeni başlıklarda `GameServer|windows.h|stdafx|shared/` yok; `new `/`malloc`/`std::vector`/`std::map`/`printf`/`rand(` yok; global/static değişken yok; `DebuffCtx`/`CallCtx` içinde düşman MP/cooldown/envanter alanı yok; Counter Curse BuffType sayısı kodda **yok** (`grep -nE 'kBuffType[A-Za-z]*Counter'` boş)
- [ ] K7 (kapsam/biçim): `git diff --stat gece/2026-10-02...bot/F7-04` yalnızca §4'teki 8 dosya + plan; `TeamBlackboard.h`/`Perception.h`/`BotCombat.h`/`GameServer/`/`shared/`/`docs/` farkı 0; ASCII + CRLF; `git diff --check` boş
- [ ] K8 (belgeleme): `[A]` sıra ve `GERI` kararı kodda yorumlu; Uygulayıcı Raporu §0 madde 3-6'yı "açık sorular" olarak listeler

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Debuff_|DebuffResult_|DebuffMemory_|Call_|ChatRate_|ChatFormat_|ChatKindForDecision_|Orders_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F7-04
git diff gece/2026-10-02...bot/F7-04 -- BotCore/TeamBlackboard.h BotCore/Perception.h BotCore/BotCombat.h | wc -l   # 0
file BotCore/DebuffCall.h BotCore/TeamChat.h BotCore/TeamOrders.h
git diff --check gece/2026-10-02...bot/F7-04
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; §2.5 (bota avantaj yok; chat, insan oyuncunun yazabileceği sınırlar içinde: CLI-18); §2.3.
- **Dürüstlük:** bu planın testleri karar mantığını sınar; "debuff başarılı mı" kanıtı oyunda `ObservedStatusTable`'ın gerçekten beslendiği F4-60 çalışma zamanına bağlıdır, chat'in istemcide **görünmesi** T-PTY-09 `[A]` olarak ölçülmemiştir. Plan uygulanınca "çağrı çalışıyor" denemez.
- Parametreler `[Ö]`/`[A]`: debuff sırası, `callScoreMin`, 5000 ms "etkin" eşiği (`docs/07:148`), `10000` ms bağışıklık (`docs/07:253`), `8000` ms dedup (`docs/07:49`); T-PRI-06 sonrası güncellenir ve `docs/07` ile birlikte değişir.
- ADR-0018 Ek 20: Slow gibi direnç zarlı debuff'larda `missed`/`srv_fail` **araç hatası değil bulgudur**; `Call` mantığı bunu "çağrı yok" olarak işler, yeniden denemeyi `ReapplyAllowed` sınırlar.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: (boş)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — (boş)
