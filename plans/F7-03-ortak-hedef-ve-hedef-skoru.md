# F7-03: Ortak hedef: F6-02 hedef seçiminin takıma genişletilmesi (takım erişilebilirliği `R`, paylaşılan hedef, `TargetCall` kaydı, takım override'ları) (`BotCore/TeamTarget.h`; `TargetSelect.h`, `TeamBlackboard.h` ekleme)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F7 — Party koordinasyonu (`docs/17` §2 F7 bloğu; kapı G7c) |
| Branch | `bot/F7-03 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-02** (`BotCore/TargetSelect.h`: `SelectTarget`, `TargetMemory`, `TargetExtras`, `RejectReason`, hedef skoru `K/R/T/D/Risk`, bağlılık, acil override'lar; **hedef skoru tek kaynağıdır, bu plan onu yeniden yazmaz**) · **F6-01** (`ReasonCode`: `TeamCall`, `DebuffCall`, `HealerSwitch`, `Finishable`, `PeelThreat`, `LeaderOrder`; `BrainParams`: `P-TGT-W-*`, `P-TGT-COMMIT-MIN`, `P-TGT-SWITCH-MARGIN`, `P-TGT-REACH-*`; `BotRole`) · **F7-01** (`TeamBlackboard` iskeleti, `kTeamCommsDelayMs`) · **F7-02** (`StallDecision::HEALER_SWITCH/REVERT`: override girdisi) · F5 nav hattı `KAPANDI` (`NavReach`: üye başına yol uzunluğu, çağıranın girdisi) · F4-51 `KAPANDI` (düşman HP) · F4-50 `KAPANDI` (ad, konum yaşı, hız) |
| İlgili gereksinim / kabul | REQ-PTY-04/05/12, REQ-PTY-06 temeli; AC-PTY-01/02 (çalışma zamanı: F7-08), MET-TGT-03/04/05 (çalışma zamanı), T-PTY-02/04, T-IGT-PTY-01; `docs/09` §5, §3 (`caller`/`leader`), §4.2 (`TargetCall`), §4.1/§4.3 (300 ms) |
| Tahmini büyüklük | M (2 yeni dosya + `TargetSelect.h`/`TeamBlackboard.h` eklemeleri + 2 `.vcxproj` = 6 dosya; ≈ 34 birim test) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 kapanmadan HAZIR yapılmaz) |

---

## 0. Neden TASLAK

**HAZIR yapma ön koşulları:**

1. **F6-02 `KAPANDI`** ve `TargetSelect.h` imzaları (`SelectTarget(const DecisionInput &, TargetMemory &, Rng &, const TargetExtras *)`, `TargetExtras`, `TargetChoice`) bu plana işlenmiş olmalı. `drafts/f6/F6-02-hedef-secimi-solo.md` §3: `K`, `T` (cast olayı sayısıyla), `D` (`ourDebuffs`), `Risk`, **`R` solo için sürekli ufuk** (`P-TGT-REACH-HORIZON` 20 sn `[A]`, çünkü `docs/09:119`'un "4 sn içinde ulaşabilen üye oranı" 70 m'lik arena başlangıcında hep 0 verir), bağlılık/marj (`score_new > score_cur + margin·max(|score_cur|, 0,1)` ⇒ **negatif skor sorunu F6-02'de çözülmüş**), override'lar `TargetDead/TargetLostVis/TargetUnreachable/Finishable/SelfDefense` zaten F6-02'dedir. Bu plan **bunları tekrar yazmaz**; ekler: takım `R`'si, paylaşılan hedefin **benimsenmesi**, `HealerSwitch`/takım-ulaşılamaz/bireysel peel override'ları ve `TargetCall` kaydı.
2. **Takım `R(t)` tanımı** (`docs/09:119`: "rol menzil bandına ≤ 4 sn içinde ulaşabilecek canlı üye sayısı / canlı üye sayısı") F6-02'nin solo çelişkisini (§8 "Çelişkiler" 1) takımda da yaşar. Bu taslak `[A]`: **takım `R` = canlı üyelerin F6-02 sürekli `R`'sinin ortalaması**. Claude `docs/09` §5.2 `R` tanımını (solo/takım, ufuk) netleştirmeden plan HAZIR olmaz. MET-TGT-03 paydası ("fiilen katılabilecek": yol uzunluğu ≤ rol menzili + `P-TGT-REACH-SLACK`, `docs/16:145`) **ayrı** bir kavramdır ve bu planda yalnızca yardımcı fonksiyonla (`CanParticipate`) yer alır.
3. **F4-61 / `UnitView` durum alanları** (düşmanda bizim debuff'ımız, Mage Armor, Counter Curse): `TargetExtras.ourDebuffs/armorFlag` F6-02'de F4-53'e bağlıdır; F4-60 `DOĞRULANDI` (merge `7891f74`), görünüm alanları F4-61'dedir. Counter Curse/Curse Refraction **BuffType kimliği bu taslakta uydurulmadı** (`docs/07:176`, `docs/09:121`): gözlenebilirlik ve kimlik `MAGIC_TYPE4` verisinden F4-60 çalışma zamanından doğrulanır; doğrulanamazsa `D`'nin −0,5 kolu devre dışı kalır (F6-02 kararı).
4. Takım içi gözlem paylaşımı (`P` sınıfı, 300 ms gecikme; `docs/13:178`) **bu planda yalnızca** `TargetCall`/paylaşılan hedef için modellenir; lider `SelectTarget`'ı **kendi** `DecisionInput`'uyla çalıştırır (`docs/09:243` `team.shared_snapshot()` bir birleşik görünüm ister: bu planda **yok**, F7-08/ayrı plan; sınırı Uygulayıcı Raporu'nda yazılır).
5. `docs/09:140` `PEEL_THREAT` **bireysel** override'dır; saldıran düşmanın "melee teması" bilgisi algıda **saklanmıyor** (başkalarının `WIZ_ATTACK` yayını tutulmaz, `BotSession.cpp:62-68`, F6-02 kapsam dışı notu). Bu plan peel için girdiyi **çağıranın bayrağı** olarak alır; bayrağın kaynağı (konum yakınlığı kestirimi) F7-08/F4 küçük dilimi `[A]`.

**Yazım turunda yeniden doğrulanacak referanslar** (okuma: `gece/2026-10-02` @ `7891f74`; F6 taslakları `drafts/f6/`):

- `docs/09_PARTY_COORDINATION_AND_TARGET_SELECTION.md:109-125` (§5.2 skor, `P-TGT-*` varsayılanları), `:127-141` (bağlılık ve override tablosu), `:55-64` (roller `caller`/`leader`), `:77-84` (`TargetCall` alanları/ömrü), `:97` (300 ms), `:240-255` (leader_tick pseudocode), `:271-279` (AC-PTY-01/02).
- `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md:143-147` (MET-TGT-01..05; `:145` MET-TGT-03 paydası), `docs/16` §5.2 (hedef değişim gerekçeleri).
- `docs/03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md:537-560` (§16: düşman MP/cooldown/envanter yasak; HP yalnızca vurulan/seçilen hedef).
- `BotCore/Perception.h`: `HpTable` (`:428`), `UnitView` (`:1141`), `kObsNameMax` (`:16`), `ObservedStatusTable` (`:2014`), `BotCore/Rng.h` (simülasyon testi).
- `drafts/f6/F6-01-karar-katmani-iskeleti-ve-b0-naive.md` (§3 `ReasonCode` kapalı listesi, `BrainParams` tablosu §3.1, `BotRole`), `drafts/f6/F6-02-hedef-secimi-solo.md` (§3 madde 1-8).

## 1. Amaç

F6-02'nin **solo** hedef seçimini takıma genişletmek: lider/çağırıcı **ortak hedefi** seçer (takım `R`'si ve takım override'larıyla), blackboard'a `TargetCall`/paylaşılan hedef olarak yazar (300 ms gecikme, 20 sn ömür); üyeler paylaşılan hedefi **benimser** (F6-02 sert filtreleri ve bağlılığıyla) ya da geçersizse solo seçime düşer; `HEALER_SWITCH` ve bireysel `PEEL_THREAT` override'ları ve bağlılık süresi (thrash yok) F6-02 mekanizmasına bağlanır.

## 2. Bağlam (okunması zorunlu)

- `docs/09` §5 (`:101-141`) ve §13 (`:240-255`); `docs/07` §9.2 (`TargetCall` yazımı: F7-04'ün işi, burada yalnızca kayıt tipi).
- F6-02 taslağı (yukarıdaki satırlar); F7-01 `TeamBlackboard.h` (üye ekleme düzeni), F7-02 `HealStall.h` (`StallDecision`).
- `docs/03` §16 ve `docs/13` §5.2a: **düşman MP'si/cooldown'ı/envanteri girdi değildir**; HP bilinmiyorsa sınıf ön tahmini (F6-02 `ClassHpPrior`); `docs/09:107` HP gözlemi seyrek.

## 3. Kapsam

**Yapılacaklar**

### 3.1 `BotCore/TargetSelect.h` (F6-02'ye **yalnızca ekleme**; mevcut davranış değişmez)

1. `TargetExtras`'a iki alan: `const float * teamReach;` (düşman indeksi başına takım `R` 0-1; `nullptr` ⇒ F6-02'nin solo `R`'si **aynen**) ve `uint16_t forcedId; ReasonCode forcedReason;` (`kNoForced = 0xFFFF`: yok). Varsayılan değerler "yok"tur ⇒ F6-02 çağrıları ve testleri **değişmeden** geçer.
2. `SelectTarget` içinde: `teamReach != nullptr` ise aday `R` değeri `teamReach[i]`; `forcedId` geçerli ve adayın **F6-02 sert filtrelerinden** (`Dead/Invisible/PosLost/TooFar/Forbidden/Unreachable/Abandoned`) geçiyorsa **bağlılık yok sayılarak** o hedef `forcedReason` ile seçilir (override mantığı F6-02'deki `TargetDead` vb. ile aynı yerden geçer); filtreden geçemezse `forcedId` yok sayılır (sert filtre override'dan önce gelir: `Forbidden` hedefe takım emri bile verilemez).
3. F6-02'nin `R` formülü bir `inline float ReachScore(float pathM, float speedMps, uint32_t horizonMs)` yardımcısına **çıkarılır** (davranış aynı; F6-02 `Target_Score_Terms_Monotonic` testi değişmeden geçer); takım `R` aynı yardımcıyı kullanır (tek formül).

### 3.2 `BotCore/TeamBlackboard.h` (F7-01'e **yalnızca ekleme**)

1. **`enum TargetCallReason : uint8_t { TCALL_DEBUFF_SUCCESS = 1, TCALL_LOW_HP, TCALL_HEALER_PRESSURE, TCALL_LEADER }`** (`docs/09:80`), **`enum TargetCallState : uint8_t { TCALL_ACTIVE = 1, TCALL_WEAKENED, TCALL_INVALID }`**, **`struct TargetCall { int16_t targetId; char name[kObsNameMax]; TargetCallReason reason; uint32_t debuffSkill; uint64_t tMs; TargetCallState state; int16_t poster; }`** ve **`ReasonCode ReasonForCall(TargetCallReason)`** (`DEBUFF_SUCCESS → DebuffCall`, `LEADER → LeaderOrder`, `HEALER_PRESSURE → HealerSwitch`, `LOW_HP → TeamCall`; `ReasonCode` kapalı listesi F6-01'dedir, **yeni ad eklenmez**).
2. **`class TeamTarget`** (sabit boyut, kopyalanabilir; `TeamBlackboard`'a üye `target`): `void SetShared(int16_t id, ReasonCode why, uint64_t nowMs)`, `bool Shared(int16_t viewer, uint64_t nowMs, int16_t & id, ReasonCode & why) const` (**atayan** hemen, diğerleri `nowMs >= setAtMs + kTeamCommsDelayMs`), `uint32_t SharedAgeMs(uint64_t nowMs) const` (saat geri giderse 0), `void ClearShared()`; `bool Post(const TargetCall &, uint64_t nowMs)` (aynı hedef için çağrı yenilenir; `P-PRI-CALL-DEDUP` 8 sn **burada uygulanmaz**, F7-04 çağrı politikasıdır); `const TargetCall * View(int16_t viewer, uint64_t nowMs) const` (**gönderen** hemen, diğerleri 300 ms sonra; `nowMs - tMs > 20000` ya da `TCALL_INVALID` ⇒ `nullptr`); `void MarkWeakened(int16_t targetId)` (debuff cure edildi: `docs/07:186`), `void OnTargetDead(int16_t targetId)` (`INVALID`, paylaşılan hedef temizlenir), `void Expire(uint64_t nowMs)` (20 sn).
3. Diğer kayıtlar (`TeamPlan`, `MemberStatus`, `EnemyIntel`, `PeelRequest`) bu planda **yoktur**.

### 3.3 `BotCore/TeamTarget.h` (yeni; yalnızca standart kütüphane + `TargetSelect.h`/`TeamBlackboard.h`/`Brain.h`; sunucu başlığı/global-static yok; sabit boyutlu)

1. **Takım erişilebilirliği:** `struct MemberReach { int16_t id; bool alive; float pathM; float speedMps; };` `float TeamReach(const MemberReach * m, int n, uint32_t horizonMs)` = canlı üyelerin `ReachScore(pathM, speedMps, horizonMs)` ortalaması (canlı üye yoksa 0; `[A]`, §0 madde 2); `bool CanParticipate(float pathLenM, float roleRangeM, float slackM)` = `pathLenM <= roleRangeM + slackM` (MET-TGT-03 paydası, `docs/16:145`; telemetri bağlaması F7-08).
2. **Takım override girdileri:** `struct TeamOverrideIn { StallDecision stall; uint16_t healerId; uint16_t oldTarget; float teamReachOfCurrent; uint32_t teamUnreachMs; bool callWeakened; }` ve `struct TeamForce { uint16_t id; ReasonCode reason; }` ile `TeamForce DetectTeamOverride(const TeamOverrideIn &)`: (a) `STALL_HEALER_SWITCH` ⇒ `{healerId, HealerSwitch}` (`docs/09:141,160`); (b) `STALL_REVERT` ⇒ `{oldTarget, HealerSwitch}` (eski hedefe dönüş); (c) mevcut hedefin `teamReach < 0,25` ve `teamUnreachMs >= 3000` ⇒ F6-02'nin `TargetUnreachable` yolu (`forced` **yok**, çağıran hedefi `abandoned[]`'e yazdırır; bu işlev `{kNoForced, TargetUnreachable}` döner ve `TeamForce.id == kNoForced`); aksi `{kNoForced, ReasonCode::Init}` (yok anlamı). Öncelik (a)/(b) > (c). Hedef ölümü/kayıp/finishable F6-02'dedir.
3. **Bireysel peel:** `bool IsIndividualPeelOverride(bool isAnchorOrNearestWarrior, float ownPriestHpPct, bool enemyMeleeContact)` (`hpPct < 40` ve temas; `docs/09:140`: yalnızca anchor ve en yakın warrior); sonuç **yalnızca o üyenin** `TargetExtras.forcedId = saldıran`, `forcedReason = PeelThreat` olarak kurulmasına yarar; takım hedefi **değişmez**.
4. **Benimseme:** `struct SharedView { bool visible; uint16_t id; ReasonCode reason; }` ve `TargetChoice AdoptOrSelect(const DecisionInput &, TargetMemory &, Rng &, const SharedView &, TargetExtras extras)`: `visible` ve `id` geçerliyse `extras.forcedId = id`, `extras.forcedReason = reason` (`TeamCall`/`DebuffCall`/`LeaderOrder`) kurup `SelectTarget`'ı çağırır (sert filtre ve **bağlılık F6-02'dedir**: benimsenen hedef `commitMinMs` boyunca tutulur, flip-flop yok; `forced` yalnızca **hedef değişimi gerektiğinde** gerekçe taşır); `!visible` ya da id sert filtreden geçmezse **solo seçime** (`forcedId = kNoForced`) düşer; `reason` kapalı listeden dışına çıkmaz.
5. **Lider seçimi:** `TargetChoice LeaderSelect(const DecisionInput &, TargetMemory &, Rng &, const float * teamReach, const TeamOverrideIn &, TargetExtras extras)` = `DetectTeamOverride` + `extras.teamReach = teamReach` + `SelectTarget`; sonuç değiştiyse çağıran `TeamTarget::SetShared`'i çağırır (bu planda yalnızca bu bağlamanın **saf** hâli).

### 3.4 Birim testleri (adlar bağlayıcı; yeni `Tests/BotCoreTests/TeamTargetTests.cpp`; F6-02 `TargetSelectTests.cpp` **değişmez**)

Fikstür: üç düşman (A priest `hp 3491 known`, B warrior `5650`, C mage `1541`), dört üye, `BrainParams` varsayılanları, sabit `Rng`.

1. `ReachScore_Refactor_SameAsF602`: F6-02 solo `R` değerleri ile `ReachScore` aynı girdilerde birebir (3 vektör: 0 m, 70 m yürüme, ufuk aşımı); `SelectTarget_NoExtras_UnchangedF602` (`extras == nullptr` ve boş `TargetExtras` aynı `TargetChoice`).
2. `TeamReach_MeanOfMemberReach`: ufuk 20000 ms, üyeler yürüme süresi 5000/10000/20000 ms (`R = 0,75; 0,5; 0`) ⇒ ortalama `0,4167 (±0,001)`; `TeamReach_DeadMembersExcluded` (ölü üye paydada yok); `TeamReach_NoAliveMembers_Zero`; `CanParticipate_Boundary` (yol 18 m, rol menzili 15, slack 3 ⇒ `true`; 18,1 ⇒ `false`).
3. `SelectTarget_TeamReachOverridesSoloR`: `teamReach` verilince sıralama solo `R` ile farklı çıkar (B takım `R` 1,0, A 0,2) ve `teamReach = nullptr` F6-02 sonucunu verir.
4. `SelectTarget_ForcedId_BypassesCommit`: mevcut hedef B, `age 100 ms`; `forcedId = A`, `forcedReason = HealerSwitch` ⇒ A seçilir, gerekçe `HealerSwitch`; `forcedId` yokken B kalır.
5. `SelectTarget_ForcedId_FailsHardFilter_Ignored`: `forcedId` ölü/görünmez/`Forbidden` aday ⇒ yok sayılır, F6-02 sonucu.
6. `Adopt_Shared_Visible_Followed`: görünür paylaşılan hedef geçerli ⇒ üye ona geçer, gerekçe `TeamCall`; `Adopt_DebuffCallReason_Maps` (`reason DebuffCall`), `Adopt_LeaderOrder_Maps`.
7. `Adopt_SharedNotVisibleBeforeCommsDelay_UsesSolo` (299 ms: solo seçim; 300 ms: benimser); `Adopt_SharedFailsHardFilter_FallsBackToSolo`; `Adopt_ClearedShared_ReturnsToSolo`.
8. `Adopt_CommitHoldsAdoptedTarget`: benimsenen hedef `commitMinMs` (4000) boyunca üyeyi tutar; skoru daha yüksek aday olsa da 3999 ms'de değişmez, 4000 ms sonra marjla değişir (F6-02 mekanizması; flip-flop yok).
9. `TeamOverride_HealerSwitch_ForcesHealer` (`STALL_HEALER_SWITCH` ⇒ `{healerId, HealerSwitch}`); `TeamOverride_Revert_ReturnsOldTarget`; `TeamOverride_None_NoForce`; `TeamOverride_Priority_SwitchBeforeUnreachable`.
10. `TeamOverride_TeamUnreachable_3s` (`teamReach 0,2`, 2999 ms ⇒ yok; 3000 ms ⇒ `TargetUnreachable`; `teamReach 0,3` ⇒ yok).
11. `Peel_IndividualOnly`: `IsIndividualPeelOverride(true, 39, true) == true`; `(false, 39, true)`, `(true, 40, true)`, `(true, 39, false)` ⇒ `false`; peel kuran üyenin hedefi değişir, **diğer üyelerin ve `TeamTarget.Shared()` değişmez** (`Peel_NotTeamLevel`).
12. `TargetCall_PostAndView`: gönderen hemen görür; diğer bot `299 ms`'de `nullptr`, `300 ms`'de çağrıyı görür; `TargetCall_Expires_After20s` (`20000` ms görünür, `20001` ms `nullptr`); `TargetCall_RepostRefreshes`; `TargetCall_Weakened_AfterCure` (`state == TCALL_WEAKENED`, hâlâ görünür); `TargetCall_TargetDead_Invalidates` (paylaşılan hedef de temizlenir); `TargetCall_NameTruncatedSafely` (20 baytlık ad + NUL sığar, 25 baytlık ad güvenle kısaltılır, taşma yok); `ReasonForCall_Mapping` (dört eşleme, yeni `ReasonCode` yok).
13. `TeamTarget_SetAndAge` (`SharedAgeMs`; saat geri giderse 0); `TeamTarget_SharedVisibilityDelay` (atayan hemen, diğerleri 300 ms); `TeamTarget_ClearShared`.
14. `TeamTarget_NoThrash_Sim60s` (belirlenim + MET-TGT-04): sabit tohumlu `Rng`; 60 sn, 100 ms tick; lider üç adaya gürültülü (±%10) skorla `LeaderSelect`, dört üye `AdoptOrSelect`; **lider hedef değişimi ≤ 4**, `ScoreMargin` kaynaklı ≤ 2; her üyenin hedefi, değişimden `300 ms + 1 tick` sonra liderinkiyle **aynı** (tutarlılık); çıktı satırı `leader_switches`, `score_margin_switches`, `max_member_lag_ms`.
15. `LeaderSelect_UsesTeamReachAndOverride` (uçtan uca: `HEALER_SWITCH` ⇒ lider healer'ı seçer ve `SetShared` çağrısına uygun `TargetChoice`); `TeamTarget_Determinism` (aynı girdi dizisi ⇒ aynı seçim dizisi).

**Kapsam dışı (yapılmayacak)**

- F6-02'nin skor bileşenlerinin, sert filtrelerinin, bağlılık/marj ve `TargetDead/TargetLostVis/TargetUnreachable/Finishable/SelfDefense` override'larının yeniden yazımı (tek kaynak F6-02).
- Sunucu bağlaması: lider tick'i, `BotSession`'dan `DecisionInput`/`TeamOverrideIn` kurulumu, `TeamTarget::SetShared/Post` çağrıları, `TARGET_SET`/`TARGET_CALL` telemetri olayları, MET-TGT-03 ölçümü, oyun içi doğrulama: **F7-08**. Birleşik takım görünümü (`shared_snapshot`, 300 ms gecikmeli): F7-08/ayrı plan.
- Takım modu (ENGAGE/HOLD/RETREAT/REGROUP), lider/vekil lider (`P-TEAM-LEADER-TIMEOUT`), regroup/retreat, tam yenilgi: ayrı F7 planları (bu seri dışı; `F7-bagimlilik-ozeti.md`).
- Çağrı politikası (`DEBUFF_SUCCESS` koşulları, dedup, chat), rol emirleri: F7-04. Heal-stall kararının **üretimi**: F7-02.
- Düşman MP/cooldown/envanter kullanımı (yasak); `NavService` çağrıları; `R(t)` için yol sorgusu (çağıranın girdisi); `docs/` değişikliği (Claude), `GameServer/`, `shared/`.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/TeamTarget.h` | yeni | §3.3 |
| `Tests/BotCoreTests/TeamTargetTests.cpp` | yeni | §3.4 |
| `BotCore/TargetSelect.h` | değiştir | yalnızca §3.1 (`TargetExtras` iki alan, `forced`/`teamReach` dalı, `ReachScore` çıkarımı); F6-02 davranışı ve testleri aynen |
| `BotCore/TeamBlackboard.h` | değiştir | yalnızca ekleme (§3.2); F7-01 satırları değişmez (`git diff` yalnızca `+`) |
| `BotCore/BotCore.vcxproj` | değiştir | `<ClInclude Include="TeamTarget.h" />` |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | `<ClCompile Include="TeamTargetTests.cpp" />` |

Listede olmayan dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F7-03 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; sunucuları durdur; test sayısını not et; F6-02 testlerinin geçtiğini doğrula.
2. `TargetSelect.h` eklerini yaz (§3.1) ve **önce** F6-02 testlerinin değişmeden geçtiğini (`Target_*`) kontrol et.
3. `TeamBlackboard.h` ekleri (§3.2) + testler 12-13.
4. `TeamTarget.h` (§3.3) + testler 1-11, 14-15; iki `.vcxproj` kaydı.
5. Derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; test sayısı plan başındakinden **§3.4'teki `TEST_CASE` sayısı kadar** fazla (≈ 34); her ad `[ OK ]`; **F6-02 `Target_*` testleri ve F7-01/F7-02 testleri değişmeden geçer**
- [ ] K3 (davranış korunumu): `ReachScore_Refactor_SameAsF602` ve `SelectTarget_NoExtras_UnchangedF602` geçer; `git diff gece/2026-10-02...bot/F7-03 -- BotCore/TargetSelect.h | grep -c '^-[^-]'` yalnızca `ReachScore` çıkarımındaki taşınan satırlar (≤ 5) ve Uygulayıcı Raporu bunları listeler
- [ ] K4 (bağlılık/override): `SelectTarget_ForcedId_BypassesCommit`, `SelectTarget_ForcedId_FailsHardFilter_Ignored`, `Adopt_CommitHoldsAdoptedTarget`, `TeamOverride_*`, `Peel_*` geçer; `PEEL_THREAT` takım hedefini **değiştirmez**; sert filtre (`Forbidden`) takım emrinden önce gelir
- [ ] K5 (thrash): `TeamTarget_NoThrash_Sim60s` çıktısı `leader_switches ≤ 4` ve `score_margin_switches ≤ 2`, `max_member_lag_ms ≤ 400` (60 sn); **MET-TGT-04 gerçek ölçümü (≤ 4/dk) ve AC-PTY-01/02 çalışma zamanında F7-08'de**, bu birim testi gerçek maç davranışının kanıtı değildir
- [ ] K6 (saflık/adalet/tek kaynak): yeni kodda `GameServer|windows.h|stdafx|shared/` yok; `new `/`malloc`/`std::vector`/`std::map`/`printf`/`rand(` yok; global/static değişken yok; düşman MP/cooldown/envanter alanı yok; `TeamTarget.h`'de skor bileşeni (`ComponentK` vb.) **tanımı yok**; yeni `ReasonCode` adı **eklenmedi** (`git diff -- BotCore/Brain.h` boş)
- [ ] K7 (kapsam/biçim): `git diff --stat gece/2026-10-02...bot/F7-03` yalnızca §4'teki 6 dosya + plan; `Perception.h`, `Brain.h`, `GameServer/`, `shared/`, `docs/` farkı 0; ASCII + CRLF; `git diff --check` boş
- [ ] K8 (belgeleme): takım `R` ortalaması `[A]` kodda `[A]` yorumuyla; Uygulayıcı Raporu §0 madde 2, 4, 5'i açık soru olarak listeler

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "ReachScore_|TeamReach_|CanParticipate_|SelectTarget_|Adopt_|TeamOverride_|Peel_|TargetCall_|ReasonForCall_|TeamTarget_|LeaderSelect_|Target_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F7-03
git diff gece/2026-10-02...bot/F7-03 -- BotCore/TeamBlackboard.h | grep -c '^-[^-]'   # 0
git diff gece/2026-10-02...bot/F7-03 -- BotCore/Brain.h | wc -l                        # 0
file BotCore/TeamTarget.h Tests/BotCoreTests/TeamTargetTests.cpp
git diff --check gece/2026-10-02...bot/F7-03
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 ve §2.5 (bota avantaj yok: `docs/03` §16 dışında bilgi yok; takım içi paylaşım 300 ms gecikmeli).
- Tüm ağırlık/eşikler `[Ö]` (`BrainParams`); takım `R` ortalaması ve `forced` sırası `[A]`; T-PTY-02/04 ve EVAL-HEALSTALL sonrası güncellenir, `docs/09` ile birlikte değişir.
- **Dürüstlük:** bu planın testleri takım hedef seçiminin **mantığını** belirlenimli doğrular; "takım ortak hedefe katılıyor" (MET-TGT-03) ve "thrash yok" (MET-TGT-04) oyunda ölçülmeden söylenemez. F6-02'nin `ClassHpPrior` `[I]` değerleri (S1 ekipman, buff'sız) gerçek maçta sapar.
- Seçim yalnızca `id` döndürür; hedefe **atama** (`SetShared`) ve çağrı çağıranın (F7-04/F7-08) işidir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: (boş)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — (boş)
