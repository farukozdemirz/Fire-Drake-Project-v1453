# F5-63: Hareketli hedef: `/bot follow <bot> <hedef bot>`, hız kestirimi, yeniden yol hesaplama, takılma tespiti ve kurtarma (`NavDrive` Follow kipi)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-63 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F5-62** (`NavDrive`, `/bot goto`, paylaşılan `NavPathfinder`) `KAPANDI` olmalı (dolaylı: F5-59, F5-61). Zaten `KAPANDI`: F5-04 (`NavFollower`/`NavTargetTracker`), F5-52 + F5-56 (hız kestirimi, gözlem zaman damgası sözleşmesi), F5-09 (`NavStuckMonitor` merdiveni), F5-54 (`NavPacketCadenceParams`, `NavGuardBlockDetector`), F5-57 (`NavProgressAssessor` + çağıran sözleşmesi), F4-50 (`UnitView` konum/hız/yaş alanları). Şemsiye: F5-55 (dilim 5). F5-64 ve F5-65 bu planın durum yapısına dayanır |
| İlgili gereksinim / kabul | `docs/12` §4.2 (hareketli hedef, 6 m / 500 ms), §10 (takılma merdiveni), §13.2 (hız kestirimi), §13.3 (niyet + gerçek ilerleme, çağıran sözleşmesi), T-NAV-04 (takılma), T-NAV-06 (hareketli hedef), AC-NAV-01 (takılma ≤ 2/bot-saat, kurtarma p95 ≤ 5 sn), MET-NAV-01/02; F5-55 §2 "Takılma tespiti sözleşmesi (F5-57)" maddeleri 1-4; `docs/13` §5.2a (gözlem ve karar katmanı), §3 |
| Tahmini büyüklük | M (10 dosya; saf mantık ağırlıklı, kodun çoğu birim testlerinde) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak) |

---

## Neden TASLAK

F5-62'nin `NavDrive`/`NavService::SharedPathfinder()` kodu depoda yok; bu plan onun **taslak** sözleşmesine dayanır. HAZIR yapmak için:

1. **F5-62 `KAPANDI`.** Varsayılan sözleşme (F5-62 §5 adım 2): `BotCore::NavDrive` (`Reset()`, `Active()`, `Mode()`, `BeginGoto`, `NextStep`, `Route()`), `NavDriveStep{None, Step, Arrived, Blocked; x, z, routeProgressM, distToGoalM, truncated}`, `ActionExecutor::TickPathMove`, `BotSession::m_navDrive`, `NavService::Instance().SharedPathfinder()` (yalnızca IOCP thread). F5-62 gerçekte farklıysa §5 uyarlanır.
2. **F5-57 çağıran sözleşmesinin kodda doğrulanması.** F5-57 Tur 1'de `NavProgressAssessor::NotifyReplan` yanlış alarm hatası (B1) bulundu ve Tur 2'de `DOĞRULANDI` oldu; yine de `BotCore/NavStuck.h` `NavProgressAssessor` gövdesi (`NotifyReplan`, `Assess` adım 5) bu plan yazılırken **yeniden okunmalı** ve `NavProgress_Arrival_Replan` testinin replan sonrası `Stalled` üretmediği doğrulanmalıdır.
3. **Sözleşme gerilimi (HAZIR öncesi Claude kararı, aşağıda §2 "G1"):** F5-57 "`moving = (Progressing || Stalled)`, `AwaitingPacket`/`BlockedByGuard`/`Idle` iken `false`, çalışan kurtarma iptal edilir" der; `NavStuckMonitor` yorumu ise "kurtarma eylemi yürütülürken çağıran `moving = true` verir" der. Önerilen çözüm yazılıdır; `tools/nav-measure.sh progress` bölümünün "assessor + monitor" satırının hangi `NavStuckParams` ile koştuğu (`tools/nav-measure/nav_measure.cpp` `RunProgress`) kodda **doğrulanacak**.
4. Yazım turunda yeniden doğrulanacak referanslar (`gece/2026-10-02` @ `9fc2dfe` üzerinde okundu): `BotCore/NavTrack.h:127-195` (`NavFollowParams`, `NavFollowStatus`, `NavFollowPlan`, `NavFollower`), `:331-` (`Update` gövdesi: due kuralı, `Find` çağrısı), `BotCore/NavStuck.h:78-100` (`NavRecoveryAction/Step`, `NavStuckMonitor`), `:124` `NavPickSideStep`, `:162-171` `NavStuckPenalties`, `:481` `NavPacketCadenceParams`, `:494` `NavGuardBlockDetector`, `:578-` F5-57 eklemeleri, `BotCore/NavDanger.h:42-110` (`NavCostLayer`, `NavCostField`), `BotCore/Perception.h:1141-1170` (`UnitView`), `:25-41` (`kPosFreshMs` 3100, `kPosLostMs` 6000, `POS_*`), `GameServer/Bot/BotManager.cpp:2560-2640` (`CommandSnap`: snapshot kurma kalıbı).

## 1. Amaç

`/bot follow <bot> <hedef bot>` komutu botu **hareket eden** bir hedefin (başka bir bot) çevresindeki halkaya yürütür: hedefin konumu ve hızı yalnızca botun **kendi algısından** (`UnitView`: gerçek `WIZ_MOVE` ~1,5 sn aralığı) kestirilir; yol, hedef ≥ 6 m kaydığında veya 500 ms dolduğunda `NavFollower` ile yeniden hesaplanır; yürüyüş niyeti ve gerçek ilerleme `NavProgressAssessor` ile birlikte değerlendirilir, takılma `NavStuckMonitor` merdiveniyle (yeniden planla → yan adım → geri adım → cezalı yeniden planla → bırak) kurtarılır. Düz `/bot move` ve `/bot goto` aynen kalır. `NAV=0` iken `follow` reddedilir.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §4.2 (yeniden planlama: hedef ≥ 6 m kaydı veya 500 ms, rol halkası, `lead = min(1,5 sn, …)`), §13.2 (hız = son iki gözlemin farkı; aralık 0,4-4,0 sn; `P-NAV-VEL-WINDOW` 4000 ms, `P-NAV-VEL-MIN-SPAN` 400 ms; en yeni gözlem 4 sn'den eskiyse veya `speed` alanı 0 ise hız 0), §13.3 (çağıran sözleşmesi), §10 (kurtarma merdiveni; tespitten bırakmaya 4 sn).
- **F5-55 §2 "Takılma tespiti sözleşmesi (F5-57)" (bu planın zorunlu maddeleri):** (1) çağıran **her yeni rotada** `NotifyReplan(nowMs, 0)` çağırır, **paket gönderiminden önce**; (2) gönderilen her paketi `OnPacketSent`, guard reddini `OnPacketRejected` ile bildirir; (3) `NavStuckMonitor::Update`'e `moving = (verdict == Progressing || verdict == Stalled)` verir; (4) niyet açıkken uzun süre paket yoksa karar `AwaitingPacket` kalır: takılma sayılmaz; paket üretilmemesi `ActionExecutor`/karar katmanı işidir (bu plan: bkz. "G3").
- **Gözlem zaman damgası sözleşmesi (F5-56, `plans/F5-56-...md:23`):** `NavFollower::ObserveTarget(tMs, x, z, speedField)`; `tMs` = paketin botun alıcısında işlendiği an (**monoton `steady_clock` ms'si**), sunucu zamanı değil. `UnitView.posAgeMs = nowMs - lastMoveMs`: gözlem zamanı `tMs = snapshot.tMs - posAgeMs`. `NavTargetTracker::Observe` aynı/eski damgayı reddeder (tekrar beslemek zararsız). `speedField`: `UnitView.speedField` (−1 bilinmiyor, 0 durmuş).
- Gözlem yalnızca `UnitView`'dan gelir (`BotCore/Perception.h:1141-1170`): `x, z`, `speedField`, `moving`, `vx/vz` (hazır kestirim: bu plan **kullanmaz**, kendi tracker'ını besler; F5-56 testleri `NavTargetTracker` içindir), `posAgeMs`, `posState` (`POS_FRESH/STALE/LOST`: 3100 / 6000 ms). Hedef görüş alanında değilse (`snapshot.allies/enemies` içinde yok) **gözlem yoktur**: bot hedefin yerini bilemez (`docs/13` §3, `docs/03` §13: bot avantajı yasağı). `tools/check-perception-contract.py` R3: başka oturumun `CUser`'ı **okunmaz**; hedef botun **kimliği** (`m_selfSid`) komut sürücüsünde oturum adından çözülür (`/bot attack` test sürücüsündeki `AttackTarget` kalıbı: yalnız kimlik çözümü, konum/hız oturumdan değil algıdan).
- `NavFollower::Update(grid, pathfinder, nowMs, botX, botZ, botSpeedMps, params)` (`NavTrack.h:331-`): due kuralı **içeridedir** ve çağrılınca **hemen planlar** (`Find` ≤ `ringMaxTries` = 3, ardından `NavSmoothPath`); `NavFollowPlan.status` ∈ `NoTarget/Planned/NoGoal/InvalidStart/PathFailed`; `m_plan.plannedAtMs`; `LastReason()` ∈ `First/Moved/Interval`; **maliyet alanı (`NavCostField`) parametresi yoktur** (`Find`'a `field` geçmez). Bu planın `PenalizeReplan` aşaması ve F5-64'ün bütçe zamanlayıcısı için `NavTrack.h`'ye **ekleme** gerekir (§3 madde 2): `InvalidatePlan()`, `ReplanDue(...) const`, `Update`'e isteğe bağlı `field` (varsayılan `nullptr` = bit düzeyinde eski davranış).
- `NavStuckMonitor::Update(grid, tMs, x, z, moving, params)` kurtarma eylemini **aşama girişinde bir kez** döndürür (`Replan`, `SideStep`, `StepBack`, `PenalizeReplan`, `Abandon`; `NavRecoveryStep{action, stage, kind, cellX, cellZ, recovered, recoveredStage, recoverMs}`); başarı = aşama girişinden ≥ `resumeM` (1 m) yer değiştirme; `NavStuckParams.stageMs = {500, 1000, 1500, 1000}`, `sideStepMaxM` 12 m, `penaltyMs` 60 000. Paket sıklığı için `NavPacketCadenceParams()` (3200 ms / 8000 ms). Yan adım hedefi `NavPickSideStep(grid, x, z, hx, hz, params, scratch, outCell)`; ceza `NavStuckPenalties::Add/Apply` → `NavCostLayer` (`AddDangerBand`) → `NavCostField{&layer, NavCostParams}`.
- Mevcut yürüyüş iskeleti: F5-62 `TickPathMove`, `SubmitMove` (F5-61 guard'lı). Sunucu konumu **yalnız paketle** değişir (bot istemci tarafı hareket etmez): "yürümek" = paket göndermek; "durmak" = `speed 0` paketi (`StopMove` kalıbı) veya paket göndermemek.

### Tasarım kararları (Claude önerisi; uygulayıcı sapmayı raporlar)

- **G1 `moving` kuralı (F5-57 ↔ F5-09 gerilimi):** `moving = (monitor.Stage() > 0) || verdict == Progressing || verdict == Stalled`. Kurtarma aşaması sürerken (`Stage() ≥ 1`) `moving` `true` kalır (F5-09 yorumu: eylem yürütülürken çağıran `true` verir); aksi halde tek yan-adım paketinden sonra gelen `AwaitingPacket` kurtarmayı henüz bitmeden iptal eder. `Stage() == 0` iken F5-57 kuralı aynen geçerlidir. **Birim testle sabitlenir** (`NavDriveFollow_RecoveryNotCancelledByAwaiting`).
- **G2 yeni rota = `NotifyReplan` + `SetIntent`:** `NavFollower` bir plan ürettiğinde (`Update` `true`, `status == Planned`), sürücü **aynı tick'te, paket göndermeden önce** `assess.NotifyReplan(nowMs, 0)` çağırır ve rotayı yeni polilinle değiştirir; `routeProgressM` her paket için **yeni rotaya göre** `NavRouteProgressM` ile hesaplanır. Varış paketi (`distToGoalM ≤ arriveM`) sonrası `SetIntent(false)`; yeni rota gelince `SetIntent(true)`.
- **G3 `AwaitingPacket` ve paket üretilmemesi:** niyet açıkken `kAwaitingMaxMs` (= 5000 ms `[A]`) paket gönderilmemişse sürücü bunu bir **yürütme hatası** olarak `Bot_*.log`'a yazar (`follow: no packet for N ms`, aktif plan/hold durumuyla) ve sayaçlar (`awaiting_long`); takılma sayılmaz (F5-57 sözleşmesi). `Hold` (F5-64, ertelenen sorgu) bu sayaca girmez: `Hold`'da niyet kapalıdır.
- **G4 hedef kaybı (test sürücüsü politikası `[A]`, karar katmanı F6'dadır):** `UnitView` yok veya `posState == POS_LOST` iken yeni plan istenmez, mevcut rota **bayat değilse** (≤ `kTargetLostHoldMs` = 6000 ms) bitirilir, sonra durma paketi; `kTargetLostAbandonMs` = 15 000 ms boyunca gözlem gelmezse takip sonlanır (`follow ended (target_lost)`). Hedef yeniden görünürse takip sürer. Bu, F5-55 §2'nin "takip kararı karar katmanındadır" ifadesiyle çelişmez: F6 kendi politikasını `BeginFollow`/`StopFollow` çağrılarıyla verir; buradaki sabitler yalnızca `/bot follow` test komutunun varsayılanıdır.
- **G5 varış:** takip rotasının sonuna (halka hücresi) varınca `speed 0` durma paketi gönderilir, `Follow` kipi **açık kalır** (rota boş, niyet kapalı); hedef ≥ 6 m kayınca (`Moved`) veya 500 ms'de (`Interval`) yeni plan ve yürüyüş başlar. Durma paketi her varışta bir kez; art arda aynı konumda yeni durma paketi gönderilmez (CLI-11 hız penceresine gereksiz yük bindirmez).
- **G6 halka:** `NavFollowParams.ringMinM = 3`, `ringMaxM = 6` `[A]` (hedefin üstüne yığılmama; çevre botlar `docs/12` §5 formasyon kuralıyla F5-08'dedir, bu planda yok); `/bot follow` isteğe bağlı halka argümanı **yok**.

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavDrive.h`: `Follow` kipi, `NavFollower`/`NavProgressAssessor`/`NavStuckMonitor`/`NavStuckPenalties` üyeleri; `BeginFollow`, `ObserveTarget`, `PlanDue`/`Plan` (planı `NavFollower` üzerinden), takılma/kurtarma durum makinesi, hedef kaybı politikası (G4); `Reset()` yeni üyeleri kapsar.
2. `BotCore/NavTrack.h` (**yalnızca ekleme/geriye uyumlu**): `NavFollower::InvalidatePlan()` (`m_plan = NavFollowPlan()`; izleyici ve `m_replans` korunur), `NavFollower::ReplanDue(nowMs, params) const` (`Update` ile **aynı** due kuralı; ortak özel yardımcıdan), `Update(..., const NavCostField * field = nullptr)` (`field` `Find`'a geçer; `nullptr` iken bit düzeyinde eski).
3. `ActionExecutor` + `BotManager`: `follow` komutu, `BeginFollow`/`TickFollow`, gözlem toplama (`UnitView`), kurtarma eylemlerinin yürütülmesi (yan adım/geri adım paketi), takılma/kurtarma `Bot_*.log` satırları.
4. `ScriptPlan.h`: `follow` fiili (21 → 22).
5. Birim testleri (§5.8).
6. Çalışma zamanı doğrulaması (Claude; §6 K11-K15).

**Kapsam dışı (yapılmayacak)**

- Bütçe zamanlayıcısı/faz kaydırma/ertelenen sorgu davranışı/yol önbelleği (F5-64: `ReplanDue` onun giriş noktasıdır); bu planda plan, `ReplanDue` doğruysa **o tick'te hemen** hesaplanır.
- `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` telemetri olayları ve `PERF_SAMPLE` nav payı (F5-64). Bu planda takılma/kurtarma yalnızca `Bot_*.log`'a ve birim testlerine yazılır.
- Ölüm/respawn/despawn/bölge değişimi temizliği (F5-65; yalnızca `Reset()` yeni üyeleri kapsar).
- `NavReach`/`NavReachJudge` (ulaşılamaz hedef bırakma kararı, F5-05), tehlike katmanı, arena sınırı, formasyon, LoS: yok. Karar katmanı (kiting, rol menzili): F6.
- `NavFollower` davranışının değişmesi: `ReplanDue`/`InvalidatePlan`/`field` eklemeleri dışında `NavTrack.h` ve mevcut `NavTrack_*` testleri değişmez.
- `BotCore/NavStuck.h`, `NavGrid.h`, `NavPath.h`, `NavSmooth.h`, `NavDanger.h`, `NavBudget.h`, `Perception.h` **değişmez**.
- `docs/` (Claude: `docs/12` §13.3 çağıran sözleşmesi güncellemesi, T-NAV-04/06 kayıtları, `docs/STATUS.md`).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavDrive.h` | değiştir | Follow kipi ve yeni üyeler (F5-62 sözleşmesi geriye uyumlu kalır: `Goto` testleri değişmeden geçer) |
| `BotCore/NavTrack.h` | değiştir | yalnızca `InvalidatePlan`, `ReplanDue`, `Update` `field` parametresi; `git diff` yalnızca ekleme + `Update` imzası/`Find` çağrısı |
| `Tests/BotCoreTests/NavDriveTests.cpp` | değiştir | yalnızca sona yeni `NavDriveFollow_*` vakaları (dosya F5-62'de eklendi; `.vcxproj` değişmez) |
| `Tests/BotCoreTests/NavTrackTests.cpp` | değiştir | yalnızca sona üç yeni vaka (§5.8) |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `BeginFollow`, `TickFollow`, `StopFollow` bildirimleri; gözlem yapısı `FollowObservation` |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yeni fonksiyonlar; kurtarma paketi yardımcıları; ceza katmanı için fonksiyon-yerel `NavCostLayer` |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandFollow`, gözlem yardımcısı bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | `follow` dağıtımı/komutu (`CommandGoto` kalıbı), `TickSessions`'ta kip seçimi, `UnitView` gözlem yardımcısı (`CommandSnap` kalıbı), log satırları |
| `BotCore/ScriptPlan.h` | değiştir | yalnızca `kVerbs`'e `"follow"`, yorumda 21 → 22 |
| `Tests/BotCoreTests/ScriptTests.cpp` | değiştir | yalnızca `Script_VerbWhitelist` |

10 dosya. Yeni dosya yok (`.vcxproj` değişmez). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-63 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucu `[UP]` ise `./tools/run-servers.sh stop`. F5-62 `NavDrive.h`/`ActionExecutor` imzalarını ve F5-57 `NavProgressAssessor` gövdesini aç ve doğrula (sapma varsa **dur**). `tools/nav-measure/nav_measure.cpp` `RunProgress`'te "assessor + monitor" satırının `NavStuckParams`'ını oku ve rapora yaz (G1).
2. **`NavTrack.h` ekleri:** `NavFollower::ReplanDue(int64_t nowMs, const NavFollowParams &) const` (`Update`'in due kuralını ortak `DueReason(...)` özel yardımcısına taşı; `Update` aynı yardımcıyı çağırır: **davranış bayt bayt aynı**); `InvalidatePlan()`; `Update(..., const NavCostField * field = nullptr)` ve `Find(..., field)`. `NavTrack_*` mevcut testleri değişmeden geçmeli.
3. **`NavDrive.h` Follow kipi** (iskelet; üyeler özel, sözleşme sabit):

   ```cpp
   enum class NavDriveMode { Off, Goto, Follow };
   enum class NavFollowEnd { None, TargetLost, StuckAbandon, Stopped };
   struct NavDriveEvents   // what the executor must log / act on after one Tick (F5-64 turns these into NAV_* events)
   {
   	bool planned = false;  NavFollowStatus planStatus = NavFollowStatus::NoTarget; NavReplanReason reason = NavReplanReason::None;
   	int  expanded = 0;     NavRecoveryStep recovery;   // action != None once per stage entry
   	NavProgressVerdict verdict = NavProgressVerdict::Idle;
   	bool stuckDetected = false;  NavStuckKind stuckKind = NavStuckKind::None;
   	NavFollowEnd ended = NavFollowEnd::None;
   };
   // NavDrive additions:
   void BeginFollow(int64_t nowMs);                         // Reset + Mode Follow; assess.SetIntent(false) until the first route
   void ObserveTarget(int64_t tMs, float x, float z, int16_t speedField, bool lost, int64_t nowMs);   // lost = absent or POS_LOST
   // One call per bot tick, BEFORE the packet step. Replans (when due) with the shared pathfinder, feeds the
   // assessor (NotifyReplan on a new route), runs the monitor and returns the recovery action to execute.
   NavDriveEvents TickFollow(const NavGrid &, NavPathfinder &, int64_t nowMs, float botX, float botZ,
   	float botSpeedMps, const NavFollowParams &, const NavStuckParams &, const NavProgressParams &, const NavCostLayer * scratchLayer);
   // Packet bookkeeping after SubmitMove (F5-57 contract (2)): exactly one of these per packet decision.
   void OnPacketSent(int64_t tMs, float x, float z, float distToGoalM);   // routeProgress recomputed from the route
   void OnPacketRejected(int64_t tMs);                                    // guard reject (CLI-05/CLI-08)
   ```

   Kurallar: (a) `TickFollow` sırası: gözlem → `ReplanDue` → `Update` → (`Planned` ise rota değiştir + **`NotifyReplan`** + `SetIntent(true)`) → `Assess` → `monitor.Update(moving = G1)` → kurtarma eylemi; (b) `PathFailed/NoGoal/InvalidStart`: mevcut rota korunur (bayatlık F5-64'te), `planned=true` raporlanır (log `follow: plan failed (<status>)`), art arda ≥ `kPlanFailAbandon` (= 10) başarısızlıkta takip `StuckAbandon` ile biter; (c) `Abandon` aşaması → `ended = StuckAbandon`; (d) hedef kaybı G4; (e) tüm süreler çağıran damgasıyla (saat yok).
4. **`ActionExecutor`:** `BeginFollow(s, targetSid, speedField, now)` (`BeginGoto` doğrulamaları: `not_in_game`, `dead`, `sitting`, `speed_field`, `nav_off`, `nav_zone`; kendini takip → `bad_target`); `TickFollow(s, obs, now)`: `drive.ObserveTarget(...)` → `drive.TickFollow(...)` → eylem yürütme: **normal adım** `NextStep` + `SubmitMove` (F5-62 `TickPathMove` ile aynı süre/kırpma; sonuç `OnPacketSent`/`OnPacketRejected`); **`SideStep`**: `NavPickSideStep` hücresine tek paket (`maxStep` ile kısaltılmış düz adım, `SubmitMove` guard'ı **geçerli**: kiriş reddedilirse `OnPacketRejected`, aşama başarısız sayılır); **`StepBack`**: yürüme yönünün tersine `min(maxStep, 3 m)` (`kStepBackM` `[A]`) tek paket; **`PenalizeReplan`**: `penalties.Add(cellX, cellZ, now, params)`, fonksiyon-yerel (`static`) `NavCostLayer` `Init/Clear` + `penalties.Apply(...)`, `drive.InvalidatePlan()` ve `Update(..., &field)`; **`Replan`**: `InvalidatePlan()`; **`Abandon`**: durma paketi, takip biter. Tüm paketler `m_moveLastSent` ve CLI-05/08 hız/adım kurallarına uyar (bir tick'te en çok bir paket, ≥ `kMovePeriodMs` aralık).
5. **`BotManager`:** `follow <bot> <hedef bot>` (`CommandFollow`; hedef oturum `PHASE_IN_GAME` olmalı ve farklı bot olmalı; kimlik `m_selfSid`); `TickSessions`'ta kip seçimi: `Follow` → gözlem yardımcısı + `TickFollow`; `/bot stop <bot>` takibi de bitirir (`StopMove` → `Reset`: F5-62'de eklendi). Gözlem yardımcısı: `CommandSnap` kalıbıyla `BuildSnapshot` (`BotManager.cpp:2598-2640`), hedef `id`'yi `enemies/allies`'te bulur; **en çok her 250 ms'de bir** ve yalnızca `Follow` kipindeki botlar için (`[A]`; maliyet K11'de ölçülür); `FollowObservation{found, x, z, speedField, posAgeMs, posState, snapMs}`. Log satırları: `BotManager: bot <b> follow: stuck <kind> at cell (x,z) stage <n> action <a>`, `... recovered (stage <n>, <ms> ms)`, `... follow ended (<reason>)`, `... follow: no packet for N ms`, `... follow: plan failed (<status>)`.
6. **`ScriptPlan.h`/`ScriptTests.cpp`:** `follow` fiili (22 fiil).
7. Derle/test; `check-perception-contract.py` rc=0 (R3: yeni kod başka oturumun `m_pUser`'ını okumaz). Uygulayıcı Raporu; `Durum` → `UYGULANDI`.
8. **Testler** (adlar sabit; sentetik zaman, `Rng` sabit tohum, belirlenimli; tick modelleri: 100 ms / 100 ± 10 ms / 110,8 ± 20 ms ve %3 olasılıkla +250 ms (`NavProgress_*` ile aynı); hedef paketi ~1,5 sn aralık + ±100 ms jitter; sanal dünya: bot yalnız paketle hareket eder; her paket kirişi `CheckMoveChord` ile denetlenir, "gizli engel" yalnızca simülatörde bulunur):
   - `NavTrack_ReplanDue_MatchesUpdate`: rastgele gözlem/zaman dizilerinde `ReplanDue` ⇔ `Update` `true` (tam eşdeğer); `NavTrack_InvalidatePlan_ForcesFirst`: sonraki `Update` `First`, izleyici korunur; `NavTrack_Update_NullField_Identical`: `field = nullptr` ile eski sonuçla bayt bayt aynı plan (mevcut sabit senaryolarda).
   - `NavDriveFollow_ChaseConstantVelocity`: 4,5 m/s düz giden hedef, 60 sanal sn, açık alan: bot 20 sn sonra hedefe ≤ `ringMax + 4,5 × 1,5 + 3` m (`[A]`) kalır; yeniden plan sayısı ≈ 2/sn'yi aşmaz; `Stalled` yok.
   - `NavDriveFollow_TargetStops_BotStops`: hedef durunca bot halkaya varır, **bir** durma paketi gönderir, hedef ≥ 6 m kayana dek başka paket yok.
   - `NavDriveFollow_ReplanOnMove`: hedef 10 m sıçrar: sonraki due tick'te `Moved` planı; öncesinde `Interval` yok.
   - `NavDriveFollow_Replan_NoFalseStalled`: hedef sürekli köşe döner (her ~2 sn yeniden plan): üç tick modelinde 600 sn `Stalled` **0**, `monitor.Episodes() == 0` (F5-57 B1 regresyonu: her yeni rotada `NotifyReplan`).
   - `NavDriveFollow_NormalChase_NoFalseStuck`: üç tick modeli × 600 sn × (düz, köşeli, dur-kalk hedef): `Episodes() == 0`.
   - `NavDriveFollow_HiddenObstacle_Recovery`: gizli tek hücre engel: tespit ≥ 3,1 sn sonra (paket sıklığı ayarı), merdiven aşamaları sırayla; kurtarma süresi `recoverMs` ≤ 5000 (T-NAV-04 birim düzeyi); **üç hücrelik gizli duvarda** `Abandon` ve takip `StuckAbandon` ile biter.
   - `NavDriveFollow_RecoveryNotCancelledByAwaiting` (G1): yan-adım paketinden sonra paket aralığı boyunca `AwaitingPacket` dönse de aşama iptal olmaz; `moving` kuralı kaldırılırsa test **kırılır** (negatif kontrol).
   - `NavDriveFollow_GuardRejected_NotStuck`: tüm paketler guard'ca reddedilir: `BlockedByGuard`, `monitor.Episodes() == 0`; reddedilmeyen paket gelince normale döner.
   - `NavDriveFollow_TargetLost_Hold`: gözlem kesilir: `kTargetLostHoldMs` sonrası durma paketi; `kTargetLostAbandonMs` sonrası `TargetLost`; gözlem geri gelirse takip sürer.
   - `NavDriveFollow_PenalizeReplan_AvoidsCell`: ceza aşamasından sonra yeni yol cezalı hücreden kaçınır (veya bulunamazsa `Abandon`'a ilerler).
   - `NavDriveFollow_RealMap_Chase` (gerçek harita yoksa `SKIPPED`): hedef bir planlayıcı yolunu yürür; bot izler; `stuck episodes = 0`, engelli kiriş 0, `NAVFOLLOW real map: ticks=... plans=... blocked=0 stuck=0`.
   - `NavDriveFollow_Perf` (`#ifndef _DEBUG`): bir `TickFollow` (plan dahil, near64) p95 ≤ 2,5 ms (A* 2,0 + tracker/assessor payı; PM-M3 ile uyumlu).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; değişen dosyalar `touch` edilince yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; §5.8'deki **tüm** yeni test adları `[ OK ]` (15: üç `NavTrack_*` + on iki `NavDriveFollow_*`); **mevcut** `NavTrack_*`, `NavStuck_*`, `NavProgress_*`, `NavDrive_*` (F5-62) testleri değişmeden geçer
- [ ] K3: `NavDriveFollow_Replan_NoFalseStalled` ve `NavDriveFollow_NormalChase_NoFalseStuck`: üç tick modelinde 600 sn `Stalled` ve monitör epizodu **0** (satır çıktısı raporda); `NavDriveFollow_RecoveryNotCancelledByAwaiting` negatif kontrolle birlikte
- [ ] K4: `NavDriveFollow_HiddenObstacle_Recovery`: tespit gecikmesi ≥ 3100 ms, `recoverMs` ≤ 5000, üç hücrelik duvarda `Abandon` (sayılar raporda)
- [ ] K5: `git diff --stat` yalnızca §4 (10 dosya) + plan; `git diff -- BotCore/NavTrack.h` yalnızca ekleme ve `Update` imza/`Find` çağrısı (`-` satırları ≤ 3, listelenir); `BotCore/NavStuck.h|NavGrid.h|NavPath.h|NavSmooth.h|NavDanger.h|NavBudget.h|Perception.h`, `docs/`, `tools/`, `.vcxproj` farkı **0**; `git diff --check` boş
- [ ] K6: `NavDrive.h`'de `grep -n -E "windows.h|stdafx|GameServer|shared/|static |new |malloc"` boş; `python3 tools/check-perception-contract.py` rc=0 (R3: başka oturumun `m_pUser` okuması yok; `GetMap`/`GetUserPtr` yeni kodda yok)
- [ ] K7: sözleşme maddeleri kodda: `NotifyReplan` **her** `Planned` plan sonrası ve **ilk paketten önce** (`dosya:satır`); `OnPacketSent`/`OnPacketRejected` her `SubmitMove` sonucunda tam bir kez; `moving` kuralı G1 (`dosya:satır`); `TickMove`/`StepToward`/`SubmitMove` ve F5-62 `TickPathMove` yolu `git diff`'te değişmedi (yalnızca yeni fonksiyonlar)
- [ ] K8: `NAV=0`/`ENABLED=0` davranışı değişmez; `NAV=0` iken `follow` `refused (nav_off)`
- [ ] K9: yeni/değişen dosyalarda satır sonu CRLF; yeni kodda ASCII (`file` raporda)
- [ ] K10: `NavFollower::Update`'in `field = nullptr` yolu ve `NavTrack_*` sabit senaryoları bayt bayt aynı sonuç (K2 + `NavTrack_Update_NullField_Identical`)
- [ ] K11 (Claude, çalışma zamanı): `ENABLED=1, NAV=1, TELEMETRY=decisions`; iki bot aynı yerde (görüş içinde): `BotMF_K` `/bot goto` ile dolaşan hedef (komut dosyası, 60-90 sn'de bir yeni hedef; hedef Karus arena bölgesinde), `BotWP_K` `/bot follow BotWP_K BotMF_K` 10 dk: bot hedefin ≤ 12 m (`ringMax + yaklaşık gecikme`, `[A]`, **bilgi**) yakınında tutulur (`/bot snap`/telemetri konumlarından mesafe serisi, p50/p95), `Bot_*.log`'da `follow: stuck` sayısı, `blocked_chord` **0**, hiçbir bot çökmez; `UnitView` anlık görüntü maliyeti (`TickSessions` içinde ölçülen) rapora
- [ ] K12 (Claude): hedef görüşten çıkınca (`/bot stop` + hedef uzak bir noktaya `goto`) `follow ended (target_lost)` ve bot durur; hedef geri gelince yeni `/bot follow` ile devam
- [ ] K13 (Claude): takılma kurtarma: bilinen gerçek engel noktası (dar geçit/köşe; Claude `tools/nav-measure.sh`/ızgaradan seçer) veya `goto` ile gerçek duvar dibine yönlendirme: `follow: stuck ... action ...` ardından `recovered` veya `Abandon`; sonuç **dürüstçe** raporlanır (tek vaka AC-NAV-01'i kapatmaz; 50 geçiş T-NAV-04 F5-66'da)
- [ ] K14 (Claude): `/bot move`, `/bot goto`, `/bot stop` takip kipinde doğru bitirir/değiştirir (`move` takibi iptal eder)
- [ ] K15 (Claude): sunucu `stop` ile kapanır; "T-NAV-04/T-NAV-06 kapandı" **yazılmaz** (kanıt F5-66)

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavDriveFollow_|NavTrack_|NavProgress_|NAVFOLLOW|Script_VerbWhitelist|tests,"
./tools/run-tests.sh Debug
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-63
git diff -U0 gece/2026-10-02...bot/F5-63 -- BotCore/NavTrack.h | grep -E "^-[^-]"
git diff --check gece/2026-10-02...bot/F5-63
# Claude (çalışma zamanı): ./tools/run-servers.sh start ; komutlar BotCommands.txt ; grep -a "follow" /mnt/c/dev/fdp/server/Logs/Bot_*.log ; ./tools/run-servers.sh stop
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; konsol spam'i yok. Bot sistemi kapalıyken sunucu davranışı değişmez.
- **Bot avantajı yasağı:** hedefin konumu/hızı yalnızca botun **kendi** `WIZ_MOVE` gözleminden; görüş dışındaki hedef bilinmez; öngörü (`lead`) en çok 1,5 sn; hız ve paket sıklığı `CheckMoveStep` sınırında. Test sürücüsü hedefin **kimliğini** oturum adından çözer (insan oyuncunun "hedefi tıklaması" karşılığı); bu, algı sözleşmesini (R3) ihlal etmez ama karar katmanı (F6) kimliği algıdan alacaktır.
- Takılma eşikleri (`3200 ms`, `1 m`, `periods = 2`, `kStepBackM = 3`, `kTargetLost*`, `kAwaitingMaxMs`) `[A]`'dır; T-NAV-04 sonrası güncellenir (`docs/12` §13.3).
- **Dürüstlük:** birim testleri **sanal** dünyada sınırlıdır (bot yalnız paketle hareket eder, gizli engel simülatörde); gerçek sunucuda takılmanın (duvar, kıyı, NPC/canavar itişi, sunucu geri çekmesi) ne sıklıkla olduğu yalnızca çalışma zamanı (K11-K13, F5-66) ile bilinir. G4/G5 politikaları test sürücüsü varsayılanıdır, kabul edilmiş karar katmanı davranışı değildir.
- `UnitView` anlık görüntüsü 250 ms'de bir ve yalnızca takip kipindeki botlar için kurulur; 16 bot takipteyken tick maliyeti F5-66'da (nav payından **ayrı**, toplam `Tick()` içinde) ölçülür: nav payı (`nav_us_*`, F5-64) yalnızca planlayıcı süresini kapsar.
- Beklenmedik durumda (F5-62 sözleşmesi farklı, `NavFollower::Update` gövdesi planla uyuşmuyor, ek dosya gerekiyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-63` — `<kısa-sha> [F5-63] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ … (K11-K15 Claude'un çalışma zamanı kriterleri)
- `nav_measure progress` "assessor + monitor" satırının `NavStuckParams`'ı: …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F5-63` @ `<sha>`
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ / ✘ | dosya:satır / komut çıktısı |

- Bulgular (önem sırasıyla):
  1. …
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
…
```
