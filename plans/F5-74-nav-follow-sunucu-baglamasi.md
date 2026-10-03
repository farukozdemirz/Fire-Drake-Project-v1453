# F5-74: `/bot follow <bot> <hedef bot> [hız]` sunucu bağlaması: `NavDrive` Follow kipinin `ActionExecutor`/`BotManager`'a bağlanması, `NavReach` ve ceza katmanı `NavService`'te (F5-63'ün sunucu dilimi)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-74 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `KAPANDI`: **F5-73** (`NavDrive` Follow kipi, saf mantık; merge `bc3170d`), **F5-70** (`/bot goto`, `SharedPathfinder()`, `BotSession::m_navDrive`; merge `99d7170`), F5-59 (`NavService`), F5-61 (kiriş guard'ı), F5-71 (bileşen farkındalıklı takipçi), F4-12/F4-50 (`ObsTable` gözlem tablosu, `UnitObs.lastMoveMs/lastSpeed`). **F5-72'den bağımsız** (F5-72 `KAPANDI`/birleşti, `3bf2ad2`; yalnızca `BotCore/ScriptPlan.h`, `ScriptTests.cpp`, `tools/skill-script-gen.py`; bu plan yalnızca `GameServer/Bot/` altındaki yedi dosya) |
| İlgili gereksinim / kabul | `docs/12` §4.2 (hareketli hedef: 6 m / 500 ms, rol halkası), §10 (takılma merdiveni), §13.2 (hız kestirimi), §13.3 (niyet + gerçek ilerleme, çağıran sözleşmesi), T-NAV-04 (takılma), T-NAV-06 (hareketli hedef); `docs/13` §3, §5.2a; CLI-05/CLI-08 (adım/kiriş), CLI-11 |
| Tahmini büyüklük | M (7 dosya, hepsi `GameServer/Bot/`; yeni dosya ve birim testi yok: kanıt derleme + kod incelemesi + Claude'un çalışma zamanı koşusu) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu, ön-plan; referanslar `gece/2026-10-02` @ `169cd9f` üzerinde doğrulandı; **tazelendi** 2026-10-03 @ `3bf2ad2`, bkz. §0a) |

---

## 0. Bu plan neden F5-74 (kimlik kaydı)

`plans/F5-63-nav-hareketli-hedef-takip-ve-takilma-kurtarma.md` (10 dosyalık taslak) önce saf mantık (F5-73, `KAPANDI`) ve sunucu bağlaması olarak bölündü; bağlama dilimi F5-63 yuvasında **yeniden yazılacaktı**. Otonom döngünün ön-plan düzeneği yalnızca **yeni** bir plan dosyasını kabul eder ve plan kimlikleri değişmez/yeniden kullanılmaz; bu yüzden bağlama dilimi **F5-74** olarak yazıldı ve F5-63 yuvası `İPTAL (yerine F5-74)` yapıldı. F5-64/F5-65 taslaklarındaki "F5-63" ifadeleri bu planı anlar (o planlar yazılırken düzeltilir). **Betik fiili `follow`** (`ScriptPlan.h`, 21 → 22) bu planda **yoktur**; F5-72 (`goto`) birleşti (izinli sözlük şimdi 21 fiil), `follow` fiili bu plan `KAPANDI` olunca ayrı küçük dilim olur.

## 0a. Tazeleme (2026-10-03, `gece/2026-10-02` @ `3bf2ad2`)

- `169cd9f..3bf2ad2` arasında yalnızca F5-72 işi birleşti: `BotCore/ScriptPlan.h` (`kVerbs[]` 20 → 21, `goto`), `Tests/BotCoreTests/ScriptTests.cpp`, `tools/skill-script-gen.py` (+ `docs/`, `plans/`). `GameServer/` ve `BotCore/NavDrive.h`/`NavReach.h`/`NavDanger.h`/`Perception.h` farkı **0**; bu planın hiçbir adımı/kabul kriteri çelişmiyor, Durum `HAZIR` kalır.
- Yeniden doğrulananlar: `NavDrive.h` (784 satır; `:27-28`, `:48`, `:64`, `:125-126`, `:251-271`, `:466`, `:474`, `:637`, `:752`, `:778`), `ActionExecutor.cpp` (`SubmitMove :83`, `ValidateWalkStart :199`, `PlanReason :241`, `NeedsReplan :257`, `BeginMove :275`, `TickMove :309` (`Active()` `:320`), `StopMove :350`, `AbandonMove :372`, `BeginGoto :384`, `TickPathMove :438` (koruma `:444`)), `ActionExecutor.h` (`:8`, `:185`, `:188-209`), `BotManager.cpp` (`WriteBotLog :43`, `PhaseName :80`, `SplitWords :122`, `ParseDoubleStrict :138`, `ParseIntStrict :151`, `goto` `:629`, bilinmeyen komut `:670`, `FindSession :675`, `IsKnownBotName :686`, `CommandGoto :1131`, `CommandStop :1208`, `CommandSnap :2634`, `TickSessions :3061`, `!moveHeld` `:3216`), `NavService` (`Grid() .h:39`, `SharedPathfinder .cpp:44`, `Shutdown :145`, `fail("no_walk") :122`, `m_ready.store :133`, "nav ready" `:137`), `BotSession.h` (`m_obsLock :157`, `m_navDrive :81`, `NavDrive.h` include `:11`), `Perception.h` (`UnitObs :49`, `ClassifyPos :85`, `ObsTable::Find :665`), `NavReach.h` (`Build :30`, `ComponentOf :37`), `NavDanger.h` (`NavCostLayer :41`).
- Düzeltilen kaymalar (davranışı etkilemez): `NavService::Startup()` `.cpp:60-142` (62 değil); `BotSession.cpp` yapıcısı `:51` (54 değil), `ResetForRespawn` `:484` (490 değil), saat `:142`. Satır numaraları **sapma denetimi için ipucudur**; asıl kural işlev adıdır.
- Uygulayıcı için taban: `git switch -c bot/F5-74 gece/2026-10-02` artık `3bf2ad2`'yi alır; başlangıç test sayısı **315** beklenir (F5-72 sonrası; `./tools/run-tests.sh Release 2>&1 | grep "tests,"` ile ölç, rapora yaz). K2 "başlangıçla aynı" kuralı değişmez.

## 1. Amaç

`[BOT] NAV=1` iken yeni `/bot follow <bot> <hedef bot> [hız]` komutu botu **hareket eden** başka bir botun çevresindeki halkaya yürütür. F5-73'ün saf mantık sürücüsü (`NavDrive` Follow kipi) sunucuya bağlanır:

- hedefin konumu/hızı yalnızca takipçi botun **kendi gözlem tablosundan** (`m_obs`, gerçek `WIZ_MOVE` paketleri) okunur; görüş dışındaki hedef bilinmez;
- yol, hedef ≥ 6 m kaydığında ya da 500 ms dolduğunda yeniden hesaplanır (`NavFollower`, bileşen farkındalıklı: `NavReach` ile);
- adımlar F5-61 kiriş guard'lı `SubmitMove` ile gerçek `WIZ_MOVE` olarak gider (~1,5 sn aralık);
- takılma `NavStuckMonitor` merdiveniyle (yeniden planla → yan adım → geri adım → cezalı yeniden planla → bırak) kurtarılır, kurtarma adımları da gerçek paketlerdir;
- hedef kaybı, varış, plan başarısızlığı ve engelli yol F5-73'ün olaylarıyla (`NavDriveEvents`) günlüğe yazılır.

`/bot move`, `/bot goto`, `/bot stop` aynen kalır; `move`/`goto`/`stop`/ölüm takibi sonlandırır. `NAV=0` iken `follow` `refused (nav_off)` verir ve sunucu davranışı değişmez.

## 2. Bağlam (okunması zorunlu)

Satır numaraları `gece/2026-10-02` @ `169cd9f` üzerinde okundu, `3bf2ad2`'de yeniden doğrulandı (§0a). Sapma görürsen **dur** (§5 adım 1).

- **`BotCore/NavDrive.h` (784 satır, F5-62 + F5-73; bu plan onu DEĞİŞTİRMEZ):**
  - `enum class NavDriveMode { Off, Goto, Follow }` `:27`, `NavFollowEnd { None, TargetLost, StuckAbandon, PlanFailed, PathBlocked }` `:28`; `Active()` `:125` **Follow için de `true`** döner, `Mode()` `:126`.
  - `NavFollowDriveParams` `:48-61` (halka `[3,0; 6,4]` m, `stuck = NavPacketCadenceParams()`, `lostGraceMs 1000`, `lostHoldMs 6000`, `lostAbandonMs 15000`, `planFailAbandon 10`, `blockedAbandonMs 5000`, `awaitingLogMs 5000`, `stepBackM 3,0`); `NavDriveEvents` `:64-76` (`planned`, `planStatus`, `planReason`, `planExpanded`, `routeAdopted`, `verdict`, `recovery` (`NavRecoveryStep`), `holdStop`, `awaitingLong`, `ended`).
  - Üyeler `:251-276`: `BeginFollow(nowMs)` (`:466`, önce `Reset()`), `ObserveTarget(tMs, x, z, speedField, nowMs)` (`:474`; **her çağrı hedefin "şimdi görüldüğü" sinyalidir**: `m_lastSeenMs = nowMs`, izleyici aynı/eski damgayı reddeder), `TickFollow(grid, finder, nowMs, botX, botZ, botSpeedMps, params, scratch, reach)` şablonu `:255-261` ve reach'siz aşırı yükleme `:262-267`, `NextFollowStep(grid, nowMs, botX, botZ, maxStepM, params)` `:637`, `OnPacketSent(tMs, step)` `:752`, `OnPacketRejected(tMs)` `:778`; erişimciler `RecoveryStage()`, `StuckEpisodes()`, `FollowPlans()`, `Arrived()`.
  - **KI-026:** reach'siz `TickFollow` aşırı yüklemesi `*nullptr` bağlar (`:266`, `:546`). **Sunucu kodu yalnızca `reach`'li aşırı yüklemeyi çağırır** (D6). Sözleşme (F5-73 D3-D10): `TickFollow` tick başına bir kez, adımdan **önce**; yeni rotada sürücü kendisi `NotifyReplan` çağırır; her **paket kararı** için tam bir `OnPacketSent`/`OnPacketRejected`; `ended != None` ise sürücü zaten `Reset` edilmiştir; varış mandalı açıkken `NextFollowStep` hiç adım üretmez; `holdStop` bir kez gelir ve çağıran durma paketi gönderir.
  - F5-73 doğrulama notu 4: kurtarma adımı `Step` döndürünce kesintisiz-`Blocked` sayacı sıfırlanmaz (ihmal edilebilir; çalışma zamanında gözlenir, bu planda düzeltilmez).
- **`BotCore/NavReach.h`:** `NavReach::Build(grid)` `:30`, `ComponentOf(x, z)` `:37`/`:204`, `ComponentCount()`, `LargestComponent()`, `ComponentCells(id)`. Salt-okunur kullanım iş parçacığı güvenlidir (Build sonrası yazılmaz).
- **`BotCore/NavDanger.h:41`:** `NavCostLayer` (`Init(grid)`, `Clear()`); `TickFollow` ceza alanını kurmak için `scratch`'i kendisi `Init/Clear` eder (`NavDrive.h:535-539`) ve **çağrı içinde** kullanıp bırakır; çağrılar arası durum taşımaz.
- **`BotCore/Perception.h`:** `UnitObs` `:49-67` (`sid`, `x10/z10`, `lastMoveMs`, `lastSpeed` (−1 bilinmiyor), `resHpType`), `ObsTable::Find(sid)` `:665`, `ClassifyPos(moving, posAgeMs)` `:85` (`POS_FRESH/STALE/LOST` = 0/1/2; durmuş birim her zaman `POS_FRESH`; hareketli birim 3100/6000 ms'de eskir). `UnitObs.lastMoveMs` ve sürücünün `nowMs`'i **aynı saat**tir: `std::chrono::steady_clock` ms'si (`BotSession.cpp:142`, `:396` `m_obs.UpdateMove(..., nowMs)`).
- **`GameServer/Bot/BotSession.h`:** `m_obsLock` `:157` (`m_obs` `:158` ve diğer algı tablolarını korur; `OnPacket` herhangi bir iş parçacığından çalışabilir), hareket üyeleri `:74-81` (`m_moveActive`, `m_moveTargetX/Z`, `m_moveSpeed`, `m_moveLastSent`, `m_actionSeq`, `m_movePackets`, `m_navDrive` `:81`), `#include`'lar `:3-11` (`NavDrive.h` `:11`). `BotSession.cpp` yapıcısı (`:51`) ve `ResetForRespawn` (`:484`) **değişmez** (D7).
- **`GameServer/Bot/ActionExecutor.cpp`:** `SubmitMove` `:83-194` (CLI-05/CLI-08 adım denetimi, F5-61 kiriş denetimi; **her reddte `m_moveActive = false`** yapar ve `REFUSED` döner: `step_too_long`/`speed_field`/`blocked_chord`; `HandlePacket`; `ok` ise `m_moveLastSent = now; m_movePackets++`; `arrived` ise `m_moveActive = false` `:171-181`; hareket kaydı `ACTION_SUBMIT` x/z içerir), `ValidateWalkStart` `:199-237` (`not_in_game`, `dead`, `sitting`, `speed_field`), `PlanReason` `:241`, `NeedsReplan(step, botX, botZ, maxStep)` `:257-271` (Blocked ya da adım > `maxStep × 1,05 + 0,1` ise `true`), `BeginMove` `:275-307` (`:296` `m_navDrive.Reset()`), `TickMove` `:309-348` (`:320` `if (s->m_navDrive.Active()) return TickPathMove(s, now);`), `StopMove` `:350-370` (`:359` `m_navDrive.Reset()`, durma paketi `SubmitMove(..., 0, 0, true, kMovePeriodMs, now)`), `AbandonMove` `:372-379`, `BeginGoto` `:384-432`, `TickPathMove` `:438-519` (`:444` `!s->m_navDrive.Active()` koruması; adım süresi kırpması `:465-475`).
- **`GameServer/Bot/ActionExecutor.h`:** `MoveOutcome` `:8-16` (yorumda `reason` listesi), `class ActionExecutor` `:185`, `BeginMove/TickMove/StopMove/AbandonMove` `:188-199`, `BeginGoto` `:205`, `TickPathMove` `:209`. **Dosya "No logging, no locking, no console output" kuralıyla yazılmıştır**: günlük ve kilit `BotManager`'dadır.
- **`GameServer/Bot/BotManager.cpp`:** `WriteBotLog` `:43`, `PhaseName` `:80`, `SplitWords` `:122`, `ParseDoubleStrict` `:138`, `ParseIntStrict` `:151`, `ExecuteCommand` dağıtımı `goto` `:629-630`, bilinmeyen komut metni `:670`, `FindSession` `:675`, `IsKnownBotName` `:686`, `CommandGoto` `:1131-1206` (kalıp), `CommandStop` `:1208`, `CommandSnap` `:2634` (`m_obsLock` altında yalnızca kopya `:2674-2692`), `TickSessions` `:3061`; hareket dalı: ölü bot `AbandonMove` `:3179-3186`, cast iptali `:3194-3214`, `if (!moveHeld) { ... ActionExecutor::TickMove(s, now) ... }` `:3216-3236` (ARRIVED/REFUSED/FAILED günlük satırları).
- **`GameServer/Bot/NavService.h/.cpp`:** `Grid()` `.h:38`, `SharedPathfinder()` `.h:46`/`.cpp:44`, özel üyeler `.h:41-56`; `Startup()` `.cpp:60-142` (`NAV` anahtarı denetimi `:72` erken dönüşle `NAV=0`'ı korur, `fail("no_walk")` `:122`, `m_ready.store(true)` `:133`, "nav ready" günlüğü `:137-140`), `Shutdown()` `:145`.
- **`tools/check-perception-contract.py`:** R2 `GetMap`, `GetItem`, `isInParty`... yalnızca allowlist'teki işlevlerde; R3: `s` dışındaki bir değişkenden `->m_pUser` okuması yasak. Yeni kod yalnızca `s->m_pUser` (botun **kendi** `CUser`'ı), kendi `m_obs` tablosu ve `NavService` verisini okur; hedef botun `m_pUser`'ına **dokunmaz** (kimliği `m_selfSid`'den alır).
- F5-70 doğrulama/çalışma zamanı: doğuş → arena A rota 275 m (Karus) / 700 m (El Morad); `BotWP_K`, `BotMF_K`, `BotWG_E` hesapları; `[BOT] ENABLED=1, NAV=1, TELEMETRY=decisions`.

### Tasarım kararları (otonom döngüde Claude kararı — gözden geçirilmeli; yeni ADR yok: ADR-0006 madde 6 + Ek F5-70 + Ek F5-73'ün uygulaması; **ADR-0006 Ek F5-74 doğrulamada yazılır**)

**D1 — Gözlem kaynağı: botun kendi `m_obs` tablosu, anlık görüntü değil.** `BotManager` her takip tick'inde `m_obsLock` altında **tek kayıt** kopyalar (`m_obs.Find((uint16_t)targetSid)`), kilidi bırakır, `FollowObservation`'a çevirir (`found`, `tMs = lastMoveMs`, `x = x10/10`, `z = z10/10`, `speedField = lastSpeed`, `posState = ClassifyPos(lastSpeed > 0, now − lastMoveMs)`). `BuildSnapshot` (`CommandSnap` kalıbı) **kullanılmaz**: tüm tabloları kopyalar, takip için gereksizdir; tek kayıt kopyası tick başına bir kilit + bir arama maliyetidir. Taslağın "250 ms'de bir `UnitView`" kuralı bu yüzden düşer. Sözleşme (R3): yalnızca botun kendi algı tablosu okunur.

**D2 — `ObserveTarget` ne zaman çağrılır.** Yürütücü yalnızca `obs.found && obs.posState != POS_LOST` iken çağırır (her tick; durmuş hedefin damgası eski kalsa da çağrılır: sürücü "görüldü" sayar, izleyici tekrar damgayı reddeder, zararsız). Kayıt yoksa (hedef görüşten çıktı: tablo kaydı siler) ya da hedef hareketli iken > 6 s paket gelmediyse (`POS_LOST`) **hiç çağrılmaz**: F5-73 D3 zamanlayıcıları (tut 6 sn → bırak 15 sn) işler. Hedef yeniden görünürse çağrı yeniden başlar ve takip sürer.

**D3 — Paket kapısı ve adım.** Paket yalnızca `now − m_moveLastSent ≥ kMovePeriodMs` (1500 ms) iken ve en çok tick başına bir tane gider; süre/kırpma `TickPathMove` ile aynıdır (`elapsedMs` en çok 2 × `kMovePeriodMs`, `maxStep = MaxStepMeters(m_moveSpeed, elapsedMs)`). `TickFollow` (planlama) paket kapısından **bağımsız** her tick çalışır. `NextFollowStep` sonucu `NeedsReplan(...)` (`:257`) ise (`Blocked` ya da adım bot rotadan itildiği için guard sınırını aşıyor) **paket gönderilmez**: sayaç artar (`offRouteSkips`), `m_moveLastSent` değişmez; sürücü `Blocked`'te planı zaten geçersiz kılar, aşırı uzun adımda ise ≤ 500 ms içindeki `Interval` yeniden planı botun şu anki konumundan yeni rota kurar. (Böylece `step_too_long` `FAIRNESS_REJECT` gürültüsü oluşmaz.)

**D3a — Kurtarma paketleri.** Yan adım/geri adım ayrı bir kod yolu değildir: sürücü bekleyen eylemi `NextFollowStep` içinde tek bir `Step` olarak döndürür; yürütücü onu sıradan adım gibi gönderir (kapı, kırpma, `SubmitMove` guard'ı, `OnPacketSent/Rejected`). Eylemin günlüğü `ev.recovery` ile yazılır (D8).

**D4 — Guard reddi takibi sonlandırmaz.** `SubmitMove` bir reddte `m_moveActive = false` yapar; Follow'da `blocked_chord`/`step_too_long` reddi için yürütücü `m_moveActive = true` geri koyar, `OnPacketRejected(nowMs)` çağırır, `m_moveLastSent = now` yapar (bir periyot sonra yeniden dene; `FAIRNESS_REJECT` yağmuru yok) ve `guardRejected` bildirir. Diğer ret (`speed_field`) ve `FAILED` (`handler_noop`) **takibi bitirir** (`packet_refused`/`packet_failed`; durma paketi gönderilmez, `SubmitMove` zaten `m_moveActive = false` yaptı; sürücü `Reset`).

**D5 — Varış, bekleme ve bitiş paketleri.** `Arrived` adımı `speed 0`, `echo 0` ile gider ama `SubmitMove`'a `arrived = false` verilir (`m_moveActive` açık kalır: takip kipi sürer); ardından `OnPacketSent(nowMs, step)` varış mandalını kurar. `ev.holdStop` gelince **bir** durma paketi (`SubmitMove(user->GetX(), GetZ(), 0, 0, false, kMovePeriodMs, now)`) gönderilir, takip açık kalır, `OnPacket*` çağrılmaz. `ev.ended != None` ise (sürücü zaten `Reset`) `StopMove(s, now)` ile bir durma paketi gönderilip takip biter (`m_moveActive = false`); bitiş nedeni metni: `target_lost`, `stuck_abandon`, `plan_failed`, `path_blocked`.

**D6 — `NavReach` ve ceza katmanı `NavService`'te.** `NavService` ızgara kurulduktan sonra (`m_grid.Build()` ve `MainComponentCells() > 0` denetiminden sonra, **`m_ready.store(true)` öncesinde**) `m_reach.Build(m_grid)` çalıştırır; sonrası salt-okunur. `const BotCore::NavReach * Reach() const` hazır değilse `nullptr` döner. Tek `NavCostLayer m_scratch` (`BotCore::NavCostLayer & ScratchLayer()`: yalnızca IOCP iş parçacığı, `SharedPathfinder()` ile aynı kural; içeriği çağrılar arasında taşınmaz) cezalı yeniden plan içindir; bellek `TickFollow` ilk ceza kullanımında `Init` ile ayrılır. Yürütücü **yalnızca** `reach`'li `TickFollow` aşırı yüklemesini çağırır (KI-026'daki `*nullptr` yolu sunucuya girmez). "nav ready" günlük satırı **değişmez**; ek bir satır yazılır: `NavService: reach ready: components=%d largest_cells=%d build_ms=%.1f` (aynı `WriteNavLog`; `largest_cells == main_cells` olmalıdır, K17).

**D7 — Oturum üyesi tek: hedef kimliği.** `BotSession.h`'ye `int m_followTargetSid = -1;` (IOCP thread only; Follow'da hedef botun `m_selfSid`'i) eklenir; üye başlatıcıyla bildirildiği için `BotSession.cpp` yapıcısı **değişmez**. Takip durumunun ölüm/respawn/despawn/bölge değişiminde temizlenmesi F5-65'in işidir; bu planda yalnızca mevcut yollar (`AbandonMove` → `m_navDrive.Reset()`, `StopMove`, `BeginMove`) takibi bitirir ve `TickSessions` takip yolunu `m_moveActive && Mode() == Follow` ile kapılar.

**D8 — Günlük satırları (yalnızca `BotManager`, sabit biçimler; Claude bunları `grep -a` ile okur).**
- `BotManager: cmd follow: <bot> following <hedef> (sid <n>) at speed <s>` ya da `BotManager: cmd follow: <bot> refused (<reason>)`.
- `BotManager: bot <b> follow: plan failed (<NoGoal|InvalidStart|PathFailed>, <First|Moved|Interval>)` (yalnızca `planned && planStatus != Planned`; başarılı planlar **günlüğe yazılmaz**, ≈ 2/sn olur).
- `BotManager: bot <b> follow: stuck <NoProgress|Oscillation> at cell (<x>,<z>) stage <n> action <replan|side_step|step_back|penalize_replan|abandon>` (`ev.recovery.action != None`), `BotManager: bot <b> follow: recovered stage <n> in <ms> ms` (`ev.recovery.recovered`).
- `BotManager: bot <b> follow: target out of sight, holding` (`holdStop`), `BotManager: bot <b> follow: no packet for >= 5000 ms` (`awaitingLong`).
- `BotManager: bot <b> follow ended (<neden>) after <packets> packets, plans <n>, stuck episodes <n>` (`m_movePackets`, `FollowPlans()`, `StuckEpisodes()`; **sürücü `Reset` edilmeden önce** okunur: bitişte sayaçlar sıfırlanır, bu yüzden yürütücü `ended` anında sayaçları `FollowOutcome`'a kopyalar).
- Mesafe serisi için ek günlük **yok**: Claude takipçi ve hedef botun `ACTION_SUBMIT` (x, z) telemetrisinden hesaplar (`TELEMETRY=decisions`).

**D9 — Takip, atış ve diğer eylemler.** `TickSessions`'taki mevcut kural "yürüyen (`m_moveActive`) bot cast beklerken önce cast'i iptal eder" (`:3194-3214`) takipçi için de geçerlidir (hareket cast'i iptal eder, CLI-03); bu plan o kuralı değiştirmez. Takip sırasında `attack`/`cast` kararı F6'nın işidir.

## 3. Kapsam

**Yapılacaklar**

1. `NavService.h/.cpp`: `NavReach m_reach`, `Reach()`, `NavCostLayer m_scratch`, `ScratchLayer()`; `Startup()` içinde `Build` + ek günlük satırı (D6).
2. `BotSession.h`: `m_followTargetSid` (D7).
3. `ActionExecutor.h/.cpp`: `FollowObservation`, `FollowOutcome`, `BeginFollow`, `TickFollow`; `TickMove`/`TickPathMove` kip denetimi (`Active()` → `Mode() == Goto`).
4. `BotManager.h/.cpp`: `follow` komutu (`CommandFollow`), gözlem okuma yardımcısı, takip tick'i ve günlük satırları, bilinmeyen komut metnine `follow`.
5. Çalışma zamanı doğrulaması (Claude; §6 K11-K17).

**Kapsam dışı (yapılmayacak)**

- `BotCore/` (tüm başlıklar), `Tests/`, `tools/`, `docs/`, `shared/`, `AIServer/`, `*.vcxproj*`: **dokunulmaz** (yeni dosya ve birim testi yok; saf mantığın testleri F5-73'tedir). Bir `BotCore` değişikliği gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz (özellikle KI-024 cebi ve KI-026 düzeltmesi: ayrı iş).
- Betik fiili `follow` (`ScriptPlan.h`, `ScriptTests.cpp`): F5-72 sonrası ayrı küçük dilim.
- Bütçe zamanlayıcısı/faz kaydırma/ertelenen sorgu/yol önbelleği ve `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` telemetrisi, `PERF_SAMPLE` nav payı (F5-64); ölüm/respawn/despawn/bölge değişimi temizliği (F5-65); `NavReachJudge` (ulaşılamaz hedef hükmü, F5-68), tehlike katmanı, arena sınırı, formasyon, LoS.
- Hedef kimliğini algıdan çözmek / karar katmanı politikaları (takip edilecek hedefi seçmek, takibi bırakmak): F6. Bu plandaki sabitler yalnızca `/bot follow` test komutunun varsayılanıdır.
- `/bot follow` için halka/menzil argümanı, birden çok hedef, takip + saldırı birleşimi: yok.
- Kabul eşiği: T-NAV-04, T-NAV-06, AC-NAV-01 "kapandı" **yazılmaz** (kanıt F5-66).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/NavService.h` | değiştir | yalnızca `#include "../../BotCore/NavReach.h"` + `NavDanger.h`, `Reach()`, `ScratchLayer()` bildirimleri, iki özel üye; mevcut imzalar değişmez |
| `GameServer/Bot/NavService.cpp` | değiştir | `Startup()`'ta `Build` ve ek günlük satırı; `ScratchLayer()`/`Reach()` gövdeleri (satır içi ise yalnızca `.h`) |
| `GameServer/Bot/BotSession.h` | değiştir | yalnızca `m_followTargetSid` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | iki yapı, iki bildirim, `reason` yorumu |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yeni iki işlev + yardımcılar; `TickMove` ve `TickPathMove`'da kip denetimi; başka işlev gövdesi değişmez |
| `GameServer/Bot/BotManager.h` | değiştir | yalnızca `CommandFollow` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | `follow` dağıtımı, bilinmeyen komut metni, `CommandFollow`, statik yardımcılar, `TickSessions` hareket dalında takip kolu |

7 dosya. Hepsi mevcut dosyaların biçimini korur (ASCII + CRLF; `file` ile doğrula, BOM yok). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-74 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. `./tools/run-servers.sh status` `[UP]` ise `./tools/run-servers.sh stop`. §2'deki referansları aç ve doğrula (özellikle `NavDrive.h:251-276`, `ActionExecutor.cpp:83-194/309-348/438-519`, `BotManager.cpp:3216-3236`, `NavService.cpp` `Startup`); sapma varsa **dur**. Başlangıçta `./tools/run-tests.sh Release` çalıştır ve test sayısını rapora yaz (bu plan test eklemez/silmez).
2. **`NavService`:**
   - `NavService.h`: `#include "../../BotCore/NavReach.h"`, `#include "../../BotCore/NavDanger.h"`; `const BotCore::NavReach * Reach() const { return Ready() ? &m_reach : nullptr; }`; `BotCore::NavCostLayer & ScratchLayer() { return m_scratch; }` (yorum: IOCP thread only, içerik çağrılar arasında taşınmaz); özel `BotCore::NavReach m_reach; BotCore::NavCostLayer m_scratch;`.
   - `NavService.cpp` `Startup()`: `m_grid.Build()` ve `MainComponentCells() <= 0` denetiminden sonra, `Info` doldurulurken **`m_ready.store(true)` öncesinde** `m_reach.Build(m_grid)` ölç (`steady_clock`), süreyi sakla; mevcut "nav ready" `snprintf`/`WriteNavLog` **aynen kalır**, hemen ardından ikinci bir `WriteNavLog` satırı: `NavService: reach ready: components=%d largest_cells=%d build_ms=%.1f` (`ComponentCount()`, `ComponentCells(LargestComponent())`). `m_reach.ComponentCount() <= 0` ise mevcut `fail("no_walk")` kalıbıyla `fail("no_reach")` ve `Ready()` **false** kalır.
3. **`BotSession.h`:** `m_navDrive` satırının altına `int m_followTargetSid = -1;   // IOCP thread only: m_selfSid of the bot being followed (F5-74); meaningful while m_navDrive.Mode() == Follow`.
4. **`ActionExecutor.h`:** (`MoveOutcome`'tan sonra; imzalar sabit, alan adları sabit)

   ```cpp
   // The followed unit as the follower's own observation table holds it (F5-74, D1). Filled by BotManager under
   // m_obsLock; the executor never locks.
   struct FollowObservation
   {
   	bool found = false;
   	uint64 tMs = 0;            // UnitObs.lastMoveMs (steady_clock ms: the clock of 'now')
   	float x = 0.0f, z = 0.0f;  // metres
   	int16 speedField = -1;     // UnitObs.lastSpeed; -1 unknown, 0 stationary
   	uint8 posState = BotCore::POS_FRESH;
   };

   // One follow tick (F5-74). 'move' is the packet result of THIS tick (NOTHING when no packet went out).
   struct FollowOutcome
   {
   	MoveOutcome move;
   	BotCore::NavDriveEvents events;
   	bool packetSent = false;      // a WIZ_MOVE was accepted this tick (step, arrival stop or hold stop)
   	bool guardRejected = false;   // D4: blocked_chord/step_too_long, follow continues
   	bool offRouteSkip = false;    // D3: no packet because the step would exceed the guard limit / was Blocked
   	bool ended = false;           // the follow is over (m_moveActive == false, drive Reset)
   	const char * endReason = "";  // "target_lost","stuck_abandon","plan_failed","path_blocked","packet_refused","packet_failed","nav_off","not_in_game"
   	int endPlans = 0;             // FollowPlans() at the moment of the end (the drive is Reset afterwards)
   	int endStuckEpisodes = 0;     // StuckEpisodes() at the moment of the end
   };
   ```
   `MoveOutcome::reason` yorumuna `/bot follow (F5-74)`: `"nav_off", "nav_zone", "bad_target", "target_not_visible"` eklenir. Sınıfa: `BeginFollow(BotSession * s, int targetSid, int16 speedField, const FollowObservation & obs, std::chrono::steady_clock::time_point now)` (`MoveOutcome` döner) ve `TickFollow(BotSession * s, const FollowObservation & obs, std::chrono::steady_clock::time_point now)` (`FollowOutcome` döner); yorumlar İngilizce, `TickMove`'un yorumu "Follow is driven by TickFollow" notunu alır.
5. **`ActionExecutor.cpp`:**
   - `TickMove` (`:320`): `if (s->m_navDrive.Mode() == BotCore::NavDriveMode::Goto) return TickPathMove(s, now);` ve hemen ardından `if (s->m_navDrive.Mode() == BotCore::NavDriveMode::Follow) return out;` (Follow'u `TickFollow` sürer; düz yürüyüşe düşmez). `TickPathMove` (`:444`): `!s->m_navDrive.Active()` → `s->m_navDrive.Mode() != BotCore::NavDriveMode::Goto`. Başka satır değişmez.
   - `BeginFollow`: `ValidateWalkStart(s, speedField, out)` (aynı REFUSED metinleri); `grid = NavService::Instance().Grid()` ve `reach = ...Reach()` boşsa `nav_off`; `user->GetZoneID() != ZONE_RONARK_LAND` → `nav_zone`; `targetSid < 0 || targetSid > 65535 || targetSid == s->m_selfSid` → `bad_target`; `!obs.found` → `target_not_visible`. **Tüm denetimler geçince** (reddedilen `follow` hiçbir üyeyi, sürmekte olan yürüyüşü bozmaz): `nowMs = duration_cast<milliseconds>(now.time_since_epoch())`; `s->m_navDrive.BeginFollow(nowMs)`; `if (obs.posState != POS_LOST) s->m_navDrive.ObserveTarget((int64_t)obs.tMs, obs.x, obs.z, obs.speedField, nowMs)`; `s->m_followTargetSid = targetSid; s->m_moveActive = true; s->m_moveSpeed = speedField; s->m_movePackets = 0; s->m_moveLastSent = now - milliseconds(kMovePeriodMs)`; `SENT/"ok"` döner (paketi aynı tick'in `TickFollow`'u gönderir).
   - `TickFollow` (sıra sabit; her bir adımın sonucu `FollowOutcome`'a işlenir):
     1. `s == nullptr || !s->m_moveActive || Mode() != Follow` → boş sonuç. `user` yok/oyunda değil → `m_moveActive = false`, `Reset()`, `ended`, `"not_in_game"`. `Grid()`/`Reach()` boş → `m_moveActive = false`, `Reset()`, `ended`, `"nav_off"`.
     2. `nowMs`; `obs.found && obs.posState != POS_LOST` ise `ObserveTarget(...)` (D2).
     3. `const BotCore::NavFollowDriveParams params;` `ev = s->m_navDrive.TickFollow(*grid, NavService::Instance().SharedPathfinder(), nowMs, user->GetX(), user->GetZ(), BotCore::MaxStepMeters(s->m_moveSpeed, 1000), params, &NavService::Instance().ScratchLayer(), *reach)` (bot hızı m/s). `out.events = ev`.
     4. `ev.ended != None`: sürücü `Reset` edilmiştir ve sayaçları sıfırlanmıştır; bu yüzden adım 3'ten **önce** `const int plansBefore = FollowPlans(); const int stuckBefore = StuckEpisodes();` okunur ve bitişte `endPlans = plansBefore + (ev.planned ? 1 : 0)`, `endStuckEpisodes = stuckBefore` yazılır (günlük satırı için yaklaşık sayı yeterlidir; rapor "≈" ile yazar). `StopMove(s, now)` ile bir durma paketi gönderilir (sonuç `out.move`), `ended = true`, `endReason` `ev.ended`'ten (`TargetLost` → `"target_lost"`, `StuckAbandon` → `"stuck_abandon"`, `PlanFailed` → `"plan_failed"`, `PathBlocked` → `"path_blocked"`); geri dön.
     5. `ev.holdStop`: `SubmitMove(s, user, user->GetX(), user->GetZ(), 0, 0, false, kMovePeriodMs, now)`; `SENT` ise `packetSent = true`; `REFUSED`/`FAILED` ise D4'ün "diğer ret" kuralı (`packet_refused`/`packet_failed`, bitir) — geri dön (aynı tick'te ikinci paket yok).
     6. Paket kapısı: `elapsed = now − m_moveLastSent < kMovePeriodMs` → geri dön. `elapsedMs` kırpması ve `maxStep` (D3). `step = s->m_navDrive.NextFollowStep(*grid, nowMs, user->GetX(), user->GetZ(), maxStep, params)`; `None` → geri dön. `NeedsReplan(step, ...)` → `offRouteSkip = true`, geri dön (D3). `arrived = (step.kind == Arrived)`; `SubmitMove(s, user, step.x, step.z, arrived ? 0 : s->m_moveSpeed, arrived ? 0 : 3, false, elapsedMs, now)`.
     7. Sonuç: `SENT` → `OnPacketSent(nowMs, step)`, `packetSent = true`. `REFUSED` ve neden `blocked_chord`/`step_too_long` → `m_moveActive = true`, `OnPacketRejected(nowMs)`, `m_moveLastSent = now`, `guardRejected = true`. Diğer `REFUSED`/`FAILED` → `s->m_navDrive.Reset()`, `ended`, `packet_refused`/`packet_failed` (`m_moveActive` zaten `false`). `ARRIVED` **gelmez** (`arrived = false` verildi).
6. **`BotManager.h`:** `void CommandFollow(const std::string & args);` (`CommandGoto` yanına).
7. **`BotManager.cpp`:**
   - `ExecuteCommand`: `goto` kolunun yanına `else if (_stricmp(verb.c_str(), "follow") == 0) CommandFollow(args);`; bilinmeyen komut metnine `follow` ekle (`move, goto, follow, stop, ...`).
   - Statik yardımcılar (`CommandGoto` civarında, `static`): `ReadFollowObservation(BotSession * s, int targetSid, uint64 nowMs, FollowObservation & out)` (D1: `std::lock_guard<std::mutex> lock(s->m_obsLock)` altında yalnızca `m_obs.Find` ve alan kopyası); adlandırıcılar `FollowStatusName/FollowReasonName/FollowStuckKindName/FollowActionName` (D8 metinleri).
   - `CommandFollow(args)`: `CommandGoto` kalıbı (`SplitWords`, 2-3 sözcük, `FindSession`, `IsKnownBotName`, `PHASE_IN_GAME`); hedef oturum: bulunamazsa `unknown or not spawned bot`, `t == s` → `refused (bad_target)`, `t->m_phase != PHASE_IN_GAME` → `target not in game (phase ...)`; hedef kimliği **yalnızca** `t->m_selfSid` (**`t->m_pUser` okunmaz**: R3). Hız isteğe bağlı (`ParseIntStrict`, varsayılan `BotCore::kWalkSpeedField`, aralık `[-32768, 32767]`). `ReadFollowObservation` → `ActionExecutor::BeginFollow(s, t->m_selfSid, speed, obs, now)`; `REFUSED` → D8 `refused (<reason>)`, aksi halde `following` satırı.
   - `TickSessions` hareket dalı (`:3216-3236`): `if (!moveHeld)` içinde, mevcut `TickMove` çağrısından **önce** `if (s->m_moveActive && s->m_navDrive.Mode() == BotCore::NavDriveMode::Follow) { TickFollowSession(s, now); } else { ...mevcut TickMove ve ARRIVED/REFUSED/FAILED günlüğü aynen... }`. `TickFollowSession(s, now)` statik yardımcıdır: `ReadFollowObservation` (hedef kimliği `s->m_followTargetSid`) → `ActionExecutor::TickFollow` → D8 günlük satırları (`planned && planStatus != Planned`, `recovery.action != None`, `recovery.recovered`, `holdStop`, `awaitingLong`, `ended`). Tick başına en çok bir `TickFollow`; mevcut kodun geri kalanı (`TickUserIn`, `TickNpcIn`, `TickSpeedCheck`, saldırı dalı) değişmez.
8. Derle (`./tools/build.sh Release` ve `Debug`), testleri koş (`./tools/run-tests.sh Release` ve `Debug`: sayı başlangıçla aynı, `0 failed`), `python3 tools/check-perception-contract.py` rc=0. Sunucuyu **açma** (K11-K17 Claude'un). Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `./tools/build.sh Debug` rc=0; `NavService.h`, `BotSession.h`, `ActionExecutor.h` `touch` edilip yeniden derlenince **yeni uyarı yok** (`BotSession.h` ve `NavService.h` yeni `NavReach.h`/`NavDanger.h` zincirini GameServer'a ilk kez sokar: raporda `warning` sayısı karşılaştırılır)
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; test sayısı başlangıç ölçümüyle **aynı** (`./tools/run-tests.sh Release 2>&1 | grep "tests,"`)
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F5-74` yalnızca §4'teki yedi dosya + plan dosyası + `plans/README.md` (yalnızca kendi satırının durum sözcüğü); `BotCore/`, `Tests/`, `tools/`, `docs/`, `shared/`, `AIServer/`, `*.vcxproj*`, `BotSession.cpp` farkı **0**
- [ ] K4: `ActionExecutor.cpp` farkı yalnızca şunlardır (`git diff -U0 gece/2026-10-02...bot/F5-74 -- GameServer/Bot/ActionExecutor.cpp` raporda): `TickMove` kip satırları, `TickPathMove` koruması, yeni `BeginFollow`/`TickFollow` (+ yardımcılar); `SubmitMove`, `ValidateWalkStart`, `BeginMove`, `StopMove`, `BeginGoto`, `NeedsReplan` gövdeleri **değişmedi**
- [ ] K5: `TickFollow` kod incelemesi (`dosya:satır` raporda): (a) `NextFollowStep` her tick'te en çok bir kez ve yalnızca paket kapısı açıkken; (b) her `SubmitMove` sonucunda tam bir `OnPacketSent`/`OnPacketRejected` (hold/bitiş durma paketleri hariç); (c) `ObserveTarget` yalnızca `found && posState != POS_LOST` iken; (d) yalnızca `reach`'li `TickFollow` aşırı yüklemesi çağrılıyor (`grep -n "TickFollow(" GameServer/Bot/*.cpp` dokuz argüman: `... &ScratchLayer(), *reach)`); (e) `Arrived` paketi `speed 0` ve `SubmitMove`'a `arrived = false`; (f) ara adımlarda `speed` her zaman `s->m_moveSpeed`
- [ ] K6: `python3 tools/check-perception-contract.py` rc=0 (R1-R5 PASS); yeni satırlarda `grep -n -E "GetMap|GetUserPtr|GetRegion|m_arNpcArray|GetItem|isInParty"` boş; `->m_pUser` yalnızca `s->m_pUser` (hedef oturum için `t->m_pUser` yok)
- [ ] K7: `NavService` farkı: mevcut "nav ready" günlük metni ve `Grid()/Ready()/GetInfo()/Startup()/Shutdown()/SharedPathfinder()` imzaları değişmedi; `m_reach.Build` `m_ready.store(true)`'dan **önce** (`dosya:satır`); `Reach()` hazır değilken `nullptr`
- [ ] K8: `NAV=0` ve `ENABLED=0` davranışı değişmez — kod incelemesiyle: yeni kod yalnızca `follow` komutu ve `Mode() == Follow` iken çalışır; `NavService::Startup()` `NAV=0` iken `Build` çağırmaz (erken dönüş korunur)
- [ ] K9: reddedilen `follow` hiçbir `BotSession` üyesini yazmaz (`BeginFollow`'da tüm denetimler `s->m_navDrive.BeginFollow`'dan önce; `dosya:satır` raporda); sürmekte olan `goto`/`move` bozulmaz
- [ ] K10: dosyalar ASCII + CRLF (`file` çıktısı raporda), BOM yok; `git diff --check` boş
- [ ] K11 (Claude, çalışma zamanı): `GameServer.ini` `[BOT] ENABLED=1, NAV=1, TELEMETRY=decisions`; iki Karus botu görüş içinde (`BotMF_K` hedef, `BotWP_K` takipçi); hedef `/bot goto` ile arena içinde dolaşır (komut dosyası, 60-90 sn'de bir yeni hedef); `/bot follow BotWP_K BotMF_K` **10 dk**: takipçi–hedef mesafe serisi `ACTION_SUBMIT` x/z'den (p50/p95; **bilgi**, beklenti ≈ halka 6,4 m + 4,5 m/s × 1,5 sn gecikme + 3 m `[A]`), `Bot_*.log`'da `follow: stuck` sayısı, `blocked_chord`/`step_too_long` `FAIRNESS_REJECT` **0**, `VIOLATION` **0**, çökme yok; `NavService: reach ready` satırı K17
- [ ] K12 (Claude): hedef görüşten çıkınca (`/bot stop` + hedef uzak bir noktaya `goto`) `target out of sight, holding`, en geç ~15 sn sonra `follow ended (target_lost)` ve bot durur; hedef geri gelince yeni `/bot follow` ile devam
- [ ] K13 (Claude): `/bot move`, `/bot goto`, `/bot stop` takip kipinde doğru bitirir/değiştirir (`move` ve `goto` takibi iptal eder; `stop` durma paketi gönderir, takip biter)
- [ ] K14 (Claude): ret yolları: kendini takip → `refused (bad_target)`; görüşte olmayan hedef → `refused (target_not_visible)`; ölü bot → `refused (dead)`; `NAV=0` → `refused (nav_off)`; ana dünya dışı zone → `refused (nav_zone)`; üçü de sürmekte olan `goto` yürüyüşünü bozmaz
- [ ] K15 (Claude): takılma kurtarma: bilinen gerçek engel noktası (dar geçit/köşe; Claude ızgaradan seçer) ya da `goto` ile gerçek duvar dibine yönlendirilen hedef: `follow: stuck ... action ...` ardından `recovered` ya da `abandon`; sonuç **dürüstçe** raporlanır (tek vaka AC-NAV-01'i kapatmaz; 50 geçiş T-NAV-04 F5-66'da)
- [ ] K16 (Claude): sunucu `./tools/run-servers.sh stop` ile kapanır; `GameServer.ini` ve `BotCommands.*` başlangıçtaki hâline döner; "T-NAV-04/T-NAV-06 kapandı" **yazılmaz**
- [ ] K17 (Claude): açılış günlüğünde `NavService: nav ready: ... main_cells=88508` satırı **değişmemiş** ve ardından `NavService: reach ready: components=<n> largest_cells=88508 build_ms=<ms>`; `build_ms` rapora (bilgi)

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "tests,|FAILED"
./tools/run-tests.sh Debug 2>&1 | grep -E "tests,|FAILED"
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-74
git diff -U0 gece/2026-10-02...bot/F5-74 -- GameServer/Bot/ActionExecutor.cpp
git diff --check gece/2026-10-02...bot/F5-74
file GameServer/Bot/NavService.h GameServer/Bot/NavService.cpp GameServer/Bot/BotSession.h GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp GameServer/Bot/BotManager.h GameServer/Bot/BotManager.cpp
grep -n "TickFollow(" GameServer/Bot/*.cpp
# Claude (çalışma zamanı): ./tools/run-servers.sh start ; komutlar BotCommands.txt ; grep -a "follow\|reach ready" /mnt/c/dev/fdp/server/Logs/Bot_*.log ; ./tools/run-servers.sh stop
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; konsol spam'i yok (yalnızca `WriteBotLog`). Bot sistemi kapalıyken (`ENABLED=0`) ve `NAV=0` iken sunucu davranışı değişmez.
- **Thread kuralı:** `TickFollow`/`BeginFollow`/`SharedPathfinder()`/`ScratchLayer()` yalnızca IOCP (BotManager::Tick) iş parçacığında; `m_obs` yalnızca `m_obsLock` altında okunur ve kilit **tek kayıt kopyası** süresince tutulur (kilit altında günlük/planlama yok).
- **Bot avantajı yasağı:** hedefin konumu/hızı yalnızca botun **kendi** `WIZ_MOVE` gözleminden; görüş dışındaki hedefin yeri bilinmez; hedef botun `CUser`'ı okunmaz (`t->m_pUser` yasak, R3); öngörü en çok 1,5 sn (`NavFollower`); hız ve paket sıklığı `CheckMoveStep`/`CheckMoveChord` sınırında. Hedef kimliğini komut sürücüsü oturum adından çözer (insan oyuncunun "hedefi tıklaması" karşılığı); karar katmanı (F6) kimliği algıdan alacaktır. `follow` yalnızca hedef takipçinin **görüşünde** iken başlar (`target_not_visible`).
- Sunucu konumu yalnızca paketle değişir: "yürümek" = paket göndermek, "durmak" = `speed 0` paketi.
- Eşikler (`3200 ms`, `1 m`, `lostHold/Abandon`, `stepBackM`, halka `[3; 6,4]`) F5-73'te `[A]`'dır; T-NAV-04 sonrası güncellenir (`docs/12` §13.3).
- **Dürüstlük:** bu plan birim testi eklemez; takılma tespitinin ve kurtarmanın gerçek sunucuda ne sıklıkla tetiklendiği (duvar, kıyı, NPC/canavar itişi, sunucu geri çekmesi), KI-024 eğim cebinin takipte sıklığı ve `m_hasBlockedSince` notunun etkisi yalnızca çalışma zamanı (K11-K15, F5-66) ile bilinir. Koşmadığın ya da gözlemlemediğin şeyi "geçti" yazma.
- Beklenmedik durumda (F5-73 sözleşmesi farklı, `BotCore` değişikliği gerekiyor, ek dosya gerekiyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-74` — `<kısa-sha> [F5-74] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Test sayısı (başlangıç / sonra): …
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ … (K11-K17 Claude'un çalışma zamanı kriterleri)
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F5-74` @ `<sha>`
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
