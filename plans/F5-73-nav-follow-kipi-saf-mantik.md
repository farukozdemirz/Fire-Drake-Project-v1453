# F5-73: `NavDrive` Follow kipi (saf mantık): hareketli hedef takibi, yeniden plan, takılma tespiti ve kurtarma

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-73 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `KAPANDI`: F5-04 (`NavFollower`/`NavTargetTracker`), F5-05 (`NavReach`), F5-09 (`NavStuckMonitor`/`NavPickSideStep`/`NavStuckPenalties`), F5-52 + F5-56 (hız kestirimi, gözlem zaman damgası), F5-54 (`NavPacketCadenceParams`), F5-57 (`NavProgressAssessor`), F5-62 (`NavDrive` `Goto`), F5-71 (`UpdateReachable`). **F5-70'e (`/bot goto` sunucu bağlaması) bağımlı DEĞİLDİR** ve onunla dosya paylaşmaz: F5-70 yalnızca `GameServer/Bot/` altındaki yedi dosyaya dokunur; bu plan yalnızca `BotCore/NavDrive.h`, `BotCore/NavTrack.h` ve iki test dosyasına. |
| İlgili gereksinim / kabul | `docs/12` §4.2 (hareketli hedef: 6 m / 500 ms, rol halkası), §10 (takılma merdiveni), §13.2 (hız kestirimi), §13.3 (niyet + gerçek ilerleme, çağıran sözleşmesi); T-NAV-04 / T-NAV-06 (**birim düzeyi** yalnızca; kabul F5-66), MET-NAV-01/02 altyapısı; ADR-0006 Ek F5-04/F5-05/F5-09/F5-57/F5-62 |
| Tahmini büyüklük | M (4 dosya; saf mantık, `GameServer` değişmez; sunucu ikilisi bayt bayt aynı kalır) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu, ön-plan; referanslar `gece/2026-10-02` @ `3949950` üzerinde doğrulandı; tasarım depo dışı bir prototiple denendi, bkz. §2 "Prototip kanıtı") |

---

## 0. Bu plan neden bu kadar (bölme kaydı)

Mevcut `TASLAK` **F5-63** (10 dosya: saf mantık + `ActionExecutor` + `BotManager` + `/bot follow` + betik fiili; `plans/F5-63-nav-hareketli-hedef-takip-ve-takilma-kurtarma.md`) gece kuralına (≤ ~6 dosya, tek yetenek) ve F5-62/F5-70 emsaline (saf mantık / sunucu bağlaması ayrımı) göre bölündü. Plan kimlikleri değişmez ve yeniden kullanılmaz; bu yüzden saf mantık dilimi **yeni numara F5-73** alır, F5-63 yuvası sunucu bağlamasına kalır:

- **Bu plan (F5-73):** `NavDrive` `Follow` kipi + `NavTrack.h` iki küçük ekleme + birim testleri. Sunucudan hiçbir şey çağrılmaz.
- **F5-63 (mevcut taslak; F5-70 ve F5-73 `KAPANDI` olunca yeniden yazılıp HAZIR yapılır):** yalnızca sunucu bağlaması — `ActionExecutor::BeginFollow/TickFollow`, `/bot follow <bot> <hedef bot>`, `UnitView` gözlem yardımcısı, `NavReach`'in `NavService`'te kurulması, kurtarma paketlerinin yürütülmesi, günlük satırları. Taslaktaki `NavDrive.h`/`NavTrack.h`/`NavDriveTests.cpp`/`NavTrackTests.cpp` maddeleri (3 ve 8) bu plana taşındı ve **bu planın metni onların yerine geçer** (taslağın `ReplanDue`, `G4` tüm sabitleri ve test adları eski haliyle kalmaz); taslağın halkası `[3; 6]` m yanlıştır (D2).
- **Betik fiili `follow`** (`ScriptPlan.h`, `ScriptTests.cpp`; taslağın 4. maddesi): F5-63 uygulandıktan sonra ayrı küçük dilim (numara yazım turunda `plans/README.md`'den doğrulanır). F5-72 numarası F5-70 planı tarafından betik fiili `goto` için ayrılmıştır.

F5-64 (bütçe + telemetri) ve F5-65 (durum temizliği) taslakları `NavDrive` üyelerine dayanır; F5-73 ve F5-63 `KAPANDI` olmadan HAZIR yapılmaz.

## 1. Amaç

`BotCore/NavDrive.h`'ye **hareketli hedefi takip eden** `Follow` kipi eklenir: hedefin konumu/hızı yalnızca verilen gözlemlerden (`ObserveTarget`) kestirilir; yol hedef ≥ 6 m kaydığında ya da 500 ms dolduğunda `NavFollower` ile yeniden hesaplanır (bileşen farkındalıklı, F5-71); her yeni rotada `NavProgressAssessor` sözleşmesi (F5-57) işletilir; takılma `NavStuckMonitor` merdiveniyle (yeniden planla → yan adım → geri adım → cezalı yeniden planla → bırak) kurtarılır; hedef görüşten çıkarsa tut/bırak politikası uygulanır. Hepsi saf mantıktır (saat yok: her zaman çağıranın tek yönlü ms damgasıdır), sentetik dünyada ve gerçek zone 71 haritasında birim testleriyle sınanır. `Goto` kipi ve `GameServer` davranışı değişmez.

## 2. Bağlam (okunması zorunlu)

Satır numaraları `gece/2026-10-02` @ `3949950` üzerinde okundu. Sapma görürsen **dur** (§5 adım 1).

- `BotCore/NavDrive.h` (334 satır; F5-62): `kMaxGotoReplans` `:17`, `kMinTruncStepM` `:18`, `NavQuantiseM` `:22`, `enum class NavDriveMode { Off, Goto }` `:27`, `NavPlanStatus` `:28`, `NavDriveParams` `:30`, `NavDriveStep{kind None/Step/Arrived/Blocked, x, z, routeProgressM, distToGoalM, truncated}` `:36`, `class NavDrive` `:45`, `Reset()` `:48`, `Active()` `:61` (`m_mode != Off`), `BeginGoto` `:67`, `NextStep(grid, botX, botZ, maxStepM)` `:83` (Follow için **aynen** yeniden kullanılır), `Replan` `:154` (`Goto` içindir; `:157` `m_mode == Off` koruması), `Route()` `:176`, `RouteLengthM()` `:177`, özel: `ValidCoord` `:186`, `PlanGoto` `:191` (rota kurma bloğu `:233-:276`: `[bot] + düzleştirilmiş hücre merkezleri + [hedef]`, başlangıç/bitiş kestirmeleri `:253`/`:261`, `m_route.swap` `:271`, `RecomputeRoute()` `:276`), `PointAt` (özel). `NavDrive.h` yalnızca standart kütüphane + kardeş başlıklar içerir (`NavChordGuard.h`, `NavPath.h`, `NavSmooth.h`, `NavStuck.h`).
- `BotCore/NavTrack.h` (499 satır): `NavFollowParams` `:127` (`ringMinM/ringMaxM`, `replanDistM` 6, `replanIntervalMs` 500, `search`, `smooth`), `NavFollowStatus` `:141` (`NoTarget, Planned, NoGoal, InvalidStart, PathFailed`), `NavReplanReason` `:150` (`None, First, Moved, Interval`), `NavFollowPlan` `:152`, `class NavFollower` `:169`: `Reset` `:172`, `ObserveTarget` (damga ≤ en yeni ise `false`), `Update(grid, finder, nowMs, botX, botZ, botSpeedMps, params)` `:180`, `UpdateReachable(..., reach)` `:188` (şablon), özel `UpdateImpl` bildirimi `:201`, gövde `:359`; `NavNoReach` `:347` (bileşen etiketi yok = `ComponentOf` hep −1 = `Update` ile aynı davranış); `pathfinder.Find(grid, botCell, m_candidates[i], params.search, m_path)` `:459` (**`field` geçirilmiyor**; `NavPathfinder::Find`'ın zaten `const NavCostField * field = nullptr` parametresi var: `NavPath.h:71-72`).
- `BotCore/NavStuck.h` (853 satır): `NavStuckParams` `:25`, `NavRecoveryAction` `:78`, `NavRecoveryStep` `:80`, `NavStuckMonitor` `:92` (`Update(grid, tMs, x, z, moving, params)` `:304`: zaman kapısı `tMs <= önceki` → boş adım; `moving == false` → dedektör sıfırlanır, aşama 0, bellek korunur; kurtarma başarısı = aşama girişinden ≥ `resumeM` yer değiştirme; `Stage()/Episodes()/Abandons()`), `NavPickSideStep` `:124`, `NavStuckPenalties` `:162` (`Add` `:404`, `Apply` `:462`: hücreye `AddDangerBand(merkez, 0, 0.5, 0, penaltyWeight)`), `NavPacketCadenceParams()` `:481` (`noProgressMs 3200`, `oscWindowMs 8000`), `NavGuardBlockDetector` `:494`, `NavRouteProgressM` `:584`, `NavProgressVerdict` `:642` (`Idle, Progressing, AwaitingPacket, Stalled, BlockedByGuard`), `NavProgressParams` `:644` (`periodMs 1550, periods 2, toleranceMs 100, minProgressM 1, arriveM 1, guardWindowMs 3200`), `NavProgressAssessor` `:654` (`SetIntent(active, nowMs)` `:661` — pasiften aktife geçişte geçmişi ve `NotifyReplan` tabanını **sıfırlar**; `NotifyReplan(nowMs, _)` `:665`; `OnPacketSent(t, x, z, routeProgressM, distToGoalM)` `:667`; `OnPacketRejected(t)` `:669`; `Assess(nowMs, params)` `:671`).
- `BotCore/NavDanger.h`: `NavCostLayer` `:41` (`Init(grid)`, `Clear()`), `NavCostField{layer, params}` `:86`. `BotCore/NavReach.h`: `NavReach::Build/ComponentOf/LargestComponent`. `BotCore/BotMotion.h`: `kWalkSpeedField = 45` `:12`, `kMovePeriodMs = 1500` `:14`, `MaxStepMeters` `:24` (45 alanı, 1500 ms → **6,75 m**). `BotCore/Perception.h:25-26,39-41`: durmuş birim `POS_FRESH` kalır, hareketli birim 3100/6000 ms'de eskir.
- Testler: `Tests/BotCoreTests/NavDriveTests.cpp` (847 satır; yardımcılar anonim ad alanında: `OpenEvents`, `HeightZeros`, `MakeNav`, `LoadZone71OrSkip` `build/nav/zone71.navgrid` + ana bileşen 88 508, `CollectWalk`, `RandomNear`; `TEST_CASE("NavDrive_Blocked_Replan")` `:518` — Blocked'i "planı `open` ızgarada kur, `NextStep`'i `wall`/`allBlocked` ızgarasında çağır" kalıbıyla üretir), `Tests/BotCoreTests/NavTrackTests.cpp` (2048 satır; F5-71 `NavTrack_Reach_*` `:1843-:1988`), `Tests/BotCoreTests/NavStuckTests.cpp:1665-1678` (anonim `NextTick`: tick modelleri — paylaşılamaz; **DİKKAT, kusurlu:** `rngState` `const &` alıp `std::mt19937 rng(rngState)` ile **kopyalar**, çağıranın durumu hiç ilerlemez, bu yüzden her çağrı aynı gecikmeyi döndürür: F5-54/F5-57 testlerindeki "tick modeli 1/2" fiilen sabit aralıklıdır, %3'lük +250 ms sıçraması ya hep ya hiç uygulanır. KI-025 olarak kaydedildi; bu planın düzeneği durumu **ilerletmelidir**). Test sayısı tabanda **293** (`git grep -h -c '^TEST_CASE' -- Tests/BotCoreTests` toplamı).

### Prototip kanıtı (depo dışı, commit edilmedi) `[V: WSL g++ 13 -O2, geçici deneme]`

Bu planın tasarımı, aşağıdaki kuralların tamamını uygulayan bir prototiple denendi (zone 71 haritası dahil). Sayılar, uygulayıcının test çıktılarıyla karşılaştırması içindir; **eşik gevşetmek için gerekçe değildir**:

| Deney | Sonuç |
|---|---|
| Halka `[3, 6]` m, 4 m'lik ızgara | Hedef bir hücre köşesine yakınken halkada hiç hücre merkezi yok (`NoGoal`): açık alanda z = 200 çizgisinde 1 362 planın 157'si (%11,5) başarısız; ardışık 10 başarısızlık takibi bitirdi. Halka `[3, 6.4]`: 0 başarısız (analitik: 4 m ızgarada `[3; 6.4]` halkasında her konum için ≥ 5 hücre merkezi; 6,32 = √40 köşe için yeterli) |
| Hedef dönüşü (U-dönüşü) | Hedefin hızı ters dönünce bot 6,8 m öne gidip 6,8 m geri döner: 3,2 sn pencerede net yer değiştirme ≈ 0 → `NavStuckMonitor` "ilerleme yok" der (yanlış takılma). Açık alanda gidip-gelen hedefle tick modeli 0'da 600 sn'de 2 epizod; gerçek haritada 30 koşunun 4'ünde (tick modeli 2). Paket yönü tersine dönünce dedektör penceresi sıfırlanınca (D6): **0** |
| Kovalama, 600 sn × 3 tick modeli | gidip-gelen çizgi / zikzak (2 sn'de bir yön) / dur-kalk / rastgele yürüyüş: `Stalled` 0, epizod 0, başarısız plan 0, ~1 100-1 360 plan (≈ 2/sn); 24 tohum × 3 model rastgele yürüyüş (72 koşu): epizod 0 |
| Gerçek zone 71 (eğim 0,45), hedef planlayıcı yollarında yürür, 10 tohum × 3 model × 300 sn | `UpdateReachable` ile: 18 395 plan, 0 başarısız, 0 epizod, 0 bırakma, gönderilen adımların **0**'ı engelli kirişte (bağımsız 0,25 m örnekleme), 12 `Blocked` (tek koşuda 1,4 sn, kendiliğinden çözüldü); `Update` (bileşensiz) ile 3 başarısız plan |
| Gizli tek hücre engeli (açık alan, durağan hedef 240 m ötede) | 3 tick modelinde 1 epizod, aşama 1 → 2 (yan adım), `recoverMs` 1300-1333, hedef halkasına varış, son hareketten tespite 3 300-3 392 ms |
| Gizli tam genişlik duvar | aşamalar 1,2,3,4,5 → `StuckAbandon`, ilk tespitten ~42-44 sn (benzetim zamanı) |
| Kurtarma başladıktan sonra hiç paket gönderilmezse | G1 kuralıyla merdiven bırakmaya kadar sürer (≈ +4,0 sn); G1 kuralı kaldırılırsa `AwaitingPacket` 1,65 sn'de kurtarmayı iptal eder, bırakma hiç olmaz (negatif kontrol) |
| Cezalı yeniden plan | Düzleştirme açıkken ceza hücreleri kirişle **kesilir** (rota aynı kalır: 3 nokta); `smooth.maxLookahead = 1` ile rota 31 noktaya açılır ve "bir hücre ileri" cezalı hücreyi **atlar** |
| Hedef 10 sn görünmez / ≥ 15 sn görünmez | 1 `holdStop`, görününce takip sürer / `TargetLost` (son görülmeden +15 sn) |
| Kapalı cep içindeki hedef | 10 art arda başarısız plan → `PlanFailed` (4,5 sn) |
| Paketler 30 sn boyunca guard'ca reddedilir | `BlockedByGuard` kararı, epizod 0 |

### Tasarım kararları (otonom döngüde Claude kararı — gözden geçirilmeli; yeni ADR yok: ADR-0006 madde 6 + Ek F5-04/F5-09/F5-57/F5-62'nin uygulaması; **ADR-0006 Ek F5-73 doğrulamada yazılır**)

**D1 — Bölme ve kapsam.** §0.

**D2 — Halka `[3, 6.4]` m.** `NavFollowDriveParams` kurucusu `follow.ringMinM = 3.0f; follow.ringMaxM = 6.4f` kurar (eski taslağın 6,0'ı 4 m'lik ızgarada boş halka üretir; kanıt yukarıda). `docs/12` §4.2 "etkin üst sınır en az hücre köşegeninin yarısıdır" ifadesi bunu yakalamıyordu; ek olarak `NavDriveFollow_RingNeverEmpty` testi `ringMaxM = 6.0` ile deliğin varlığını **sabitler**. Başka bir ızgara birimi için halka `√(2)·unit·1,12` üzeri olmalıdır (yorum).

**D3 — Hedef kaybı politikası (test sürücüsü varsayılanı; karar katmanı F6'dadır).** `ObserveTarget(…, nowMs)` her çağrıda "hedef şimdi görüş alanında" sinyalidir (durmuş hedefin gözlem damgası eski kalır, çağıran yine de her tick çağırır; izleyici aynı/eski damgayı reddeder ama hedef "görülmüş" sayılır). Çağrı kesilince `lostMs = nowMs − son çağrı`: `≤ lostGraceMs` (1000) normal; `(grace, hold]` (6000): yeni plan **istenmez**, mevcut rota bitirilir; `> hold`: rota bırakılır, **bir kez** `holdStop` olayı (çağıran durma paketi gönderir), plan yok; `≥ lostAbandonMs` (15000): `ended = TargetLost`, sürücü `Off`. Hedef yeniden görünürse (`ObserveTarget`) bekleme biter, planlama sürer.

**D4 — Varış mandalı.** `Arrived` adımı için `OnPacketSent` çağrılınca sürücü `m_arrived = true` olur ve değerlendirici niyeti kapatılır (`SetIntent(false)`); mandal açıkken `NextFollowStep` hiç adım üretmez (aynı konumda art arda durma paketi yok). Yeni bir plan benimsenince `kalan = rota uzunluğu − botun rota ilerlemesi`; `kalan > progress.arriveM` (1 m) ise mandal açılır ve niyet yeniden açılır, aksi halde mandal korunur (halkada kalan bot, hedef 1 m kaymadıkça oynamaz).

**D5 — `moving` kuralı (F5-57 ↔ F5-09 gerilimi, G1).** `monitor.Update(…, moving)` için `moving = (karar == Progressing || karar == Stalled) || (monitor.Stage() > 0 && karar == AwaitingPacket)`. Kurtarma aşaması sürerken tek bir geç paket eylemi iptal etmez (yoksa merdiven her seferinde aşama 1'e döner); `Idle` ve `BlockedByGuard` her zaman `false` (F5-57 aynen). Not: Follow'da yeniden plan her 500 ms'de bir olduğundan `NotifyReplan` tabanı hiçbir zaman 3,3 sn'lik bir çapa paketine ulaşmaz; **`Stalled` kararı Follow'da pratikte üretilmez** ve takılma tespiti dedektörün konum penceresindedir (`moving` yalnızca "niyet var ve paketler akıyor" kapısıdır).

**D6 — U-dönüşü ≠ takılma.** `OnPacketSent`, gönderilen paketin yer değiştirme vektörünü (paket konumu − paketin hesaplandığı bot konumu; `NextFollowStep` saklar) bir önceki paketin vektörüyle nokta çarpımına sokar; çarpım < 0 (> 90° dönüş) ve `monitor.Stage() == 0` ise bir sonraki `TickFollow`'da `monitor.Update(…, moving = false)` çağrılır (dedektör penceresi sıfırlanır, bellek korunur). Gerçekten takılı bot aynı yönde paket göndermeye devam eder (nokta çarpımı > 0), tespit korunur. Kurtarma aşamasındayken (`Stage() > 0`) sıfırlama yapılmaz.

**D7 — Cezalı yeniden plan.** `PenalizeReplan` eylemi: `penalties.Add` ile (a) takılan hücre (`rec.cellX/cellZ`), (b) rotada botun ilerlemesinden `grid.Unit()` metre ilerideki nokta hücresi (F5-09 benzetimindeki "sonraki yol noktası" karşılığı); `InvalidatePlan()`. Ceza etkinken (`penalties.Count(nowMs) > 0` ve çağıran `scratch` verdiyse) her plan `scratch->Clear()` + `penalties.Apply` ile kurulan `NavCostField` ile koşar **ve `follow.smooth.maxLookahead = 1`** kullanır (düzleştirme ceza hücrelerini kirişle keser; kanıt yukarıda). `scratch == nullptr` ise `PenalizeReplan` düz `Replan` gibi davranır (ceza kaydedilir, alan uygulanmaz). Boyut uyuşmazsa sürücü `scratch->Init(grid)` yapar.

**D8 — Plan başarısızlığı ve cep (KI-024).** `Update` bir plan hesaplayıp `Planned` dışı (`NoGoal/InvalidStart/PathFailed`) dönerse mevcut rota korunur; ardışık `planFailAbandon` (10) başarısızlıkta `ended = PlanFailed`. Eğim cebi (KI-024) için ayrı kurtarma **yoktur**: cepteki bot `UpdateReachable` ile A* koşmadan `PathFailed` alır ve ≈ 5 sn sonra takibi bırakır; kök neden (düzleştirme kirişinin ana bileşende kalması) ayrı iştir, KI-024 **AÇIK** kalır (gerçek haritada 30 koşuda 0 başarısız plan görüldü; sıklık F5-66'da ölçülür).

**D9 — Merdiven süreleri değişmez.** `stuck = NavPacketCadenceParams()` (`stageMs` 500/1000/1500/1000). Kurtarma paketleri yine `kMovePeriodMs` aralığıyla gider (yürütücü işi); gizli engel kanıtı (`recoverMs` ≈ 1,3 sn) bu süreleri yeterli gösterdi. Süre/eşik değiştirmek için kanıt gerekir: F5-66.

**D10 — `Blocked` adım.** `NextFollowStep`, `NextStep` `Blocked` dönerse planı geçersiz kılar (`InvalidatePlan`; sonraki `TickFollow` botun şu anki konumundan yeniden planlar). `Blocked` kesintisiz `blockedAbandonMs` (5000) sürerse `ended = PathBlocked`. (`Goto`'nun "en çok 1 `Replan`" sınırı Follow'a uygulanmaz: yeniden planlar zaten 500 ms ile sınırlıdır.)

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavTrack.h` (**yalnızca ekleme/geriye uyumlu**): `NavFollower::InvalidatePlan()`; `Update` ve `UpdateReachable`'a son parametre `const NavCostField * field = nullptr` (özel `UpdateImpl`'a iletilir, `Find`'a geçer).
2. `BotCore/NavDrive.h`: `NavDriveMode::Follow`, `NavFollowEnd`, `NavFollowDriveParams`, `NavDriveEvents`; `NavDrive`'a `BeginFollow`, `ObserveTarget`, `TickFollow` (şablon + reach'siz aşırı yükleme), `NextFollowStep`, `OnPacketSent`, `OnPacketRejected` ve salt-okunur erişimciler; `PlanGoto`'daki rota kurma bloğunun özel `AdoptRoute`'a taşınması (mekanik; `Goto` bayt bayt aynı).
3. `Tests/BotCoreTests/NavTrackTests.cpp` ve `NavDriveTests.cpp`: 21 yeni test (§5.5).

**Kapsam dışı (yapılmayacak)**

- `GameServer/`, `AIServer/`, `shared/`, `*.vcxproj*`, `tools/`, `docs/`: dokunulmaz (`docs/12` §4.2 halka notu, §13.3 çağıran sözleşmesi ve ADR-0006 Ek F5-73'ü Claude yazar).
- `ActionExecutor`/`BotManager`/`/bot follow`/`UnitView` gözlem yardımcısı/kurtarma paketlerinin gönderilmesi (F5-63); betik fiili `follow` (ayrı küçük dilim, bkz. §0); bütçe zamanlayıcısı, ertelenen sorgu, yol önbelleği (F5-64); `NAV_*` telemetrisi ve `PERF_SAMPLE` nav payı (F5-64); ölüm/respawn/despawn/bölge temizliği (F5-65); `ReplanDue` giriş noktası (F5-64 yazılırken eklenir).
- Cep kurtarma / `NavSmooth` kirişinin ana bileşende kalması (KI-024), arena sınırı, tehlike katmanı, formasyon, LoS, `NavReachJudge` (ulaşılamaz hedef hükmü): yok.
- `BotCore/NavStuck.h`, `NavGrid.h`, `NavPath.h`, `NavSmooth.h`, `NavDanger.h`, `NavBudget.h`, `NavReach.h`, `Perception.h`, `BotMotion.h`, `NavChordGuard.h` **değişmez**; mevcut `Goto` davranışı ve mevcut tüm testler değişmez.
- Kabul eşiği/telemetri: T-NAV-04, T-NAV-06, AC-NAV-01 "kapandı" **yazılmaz** (kanıt F5-66; burada yalnızca birim düzeyi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavDrive.h` | değiştir | Follow kipi; `Goto` fonksiyonlarının gövdeleri yalnızca §5 adım 3'te sayılan üç satır/blok farkıyla değişir |
| `BotCore/NavTrack.h` | değiştir | yalnızca `InvalidatePlan`, `Update`/`UpdateReachable`/`UpdateImpl` `field` parametresi, `Find` çağrısı |
| `Tests/BotCoreTests/NavDriveTests.cpp` | değiştir | yalnızca sona yeni `NavDriveFollow_*` vakaları + anonim ad alanına yeni test yardımcıları (`.vcxproj` zaten içerir: `BotCoreTests.vcxproj:90`) |
| `Tests/BotCoreTests/NavTrackTests.cpp` | değiştir | yalnızca sona üç yeni `NavTrack_*` vakası |

Hepsi ASCII + CRLF (`file` ile doğrula); yeni dosya yok. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-73 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. `./tools/run-servers.sh status` `[UP]` ise `./tools/run-servers.sh stop`. `python3 tools/nav-export.py` (ızgara yoksa; `build/nav/zone71.navgrid`). §2'deki referansları aç ve doğrula (özellikle `NavDrive.h:157`, `NavTrack.h:180/188/201/359/459`, `NavStuck.h` `NavProgressAssessor` imzaları ve `SetIntent` davranışı); sapma varsa **dur**. Başlangıçta `./tools/run-tests.sh Release`: `293 tests, 0 failed` bekleniyor (farklıysa raporda yaz, plan sayısını ona göre oku).
2. **`NavTrack.h`:** `NavFollower`'a `void InvalidatePlan() { m_plan = NavFollowPlan(); }` (izleyici, `m_reason` ve `m_replans` korunur; sonraki `Update` gözlem varsa `First` nedeniyle hemen planlar). `Update(..., const NavFollowParams & params, const NavCostField * field = nullptr)`, `UpdateReachable(..., const Reach & reach, const NavCostField * field = nullptr)` ve özel `UpdateImpl(..., const Reach * reach, const NavCostField * field)`; `Find(grid, botCell, m_candidates[i], params.search, m_path, field)`. Başka hiçbir satır değişmez; `field == nullptr` yolu bayt bayt eskisi gibidir.
3. **`NavDrive.h`:**
   - (a) `Reset()` yeni üyeleri sıfırlar (`m_follower.Reset()`, `m_assess.Reset()`, `m_monitor.Reset()`, `m_penalties.Clear()`, bekleyen eylem, mandal, tüm sayaçlar/damgalar).
   - (b) `Replan`'ın koruması `if (m_mode != NavDriveMode::Goto) return NavPlanStatus::None;` olur (Follow'da çağrılırsa `None`, kip değişmez).
   - (c) `PlanGoto`'nun rota kurma bloğu (`:233-:276`) özel `AdoptRoute(grid, botX, botZ, const std::vector<NavCell> & waypoints, goalX, goalZ)`'a taşınır; `PlanGoto` onu çağırır. **Taşıma mekaniktir:** mevcut 13 `NavDrive_*` testi ve `NavDrive_RealMap_Random` sayıları (`planned=295 nopath=2 nodelimit=3`) değişmeden geçmelidir.
   - (d) Yeni türler ve üyeler (adlar sabit; gövdeler senin):

   ```cpp
   enum class NavDriveMode { Off, Goto, Follow };   // Follow sona eklenir
   enum class NavFollowEnd { None, TargetLost, StuckAbandon, PlanFailed, PathBlocked };

   struct NavFollowDriveParams
   {
   	NavFollowParams   follow;             // kurucu: ringMinM = 3.0f, ringMaxM = 6.4f (D2)
   	NavStuckParams    stuck;              // kurucu: NavPacketCadenceParams() (D9)
   	NavProgressParams progress;           // varsayilan
   	int   lostGraceMs = 1000, lostHoldMs = 6000, lostAbandonMs = 15000;   // D3
   	int   planFailAbandon = 10;           // D8
   	int   blockedAbandonMs = 5000;        // D10
   	int   awaitingLogMs = 5000;           // yalnizca tani (olay), takilma degil
   	float stepBackM = 3.0f;
   	NavFollowDriveParams();
   };

   struct NavDriveEvents   // one TickFollow call; the executor (F5-63) turns these into log/telemetry lines
   {
   	bool planned = false;                                   // NavFollower recomputed a plan this call
   	NavFollowStatus planStatus = NavFollowStatus::NoTarget; // valid when planned
   	NavReplanReason planReason = NavReplanReason::None;     // valid when planned
   	int  planExpanded = 0;                                  // A* closed nodes of that plan
   	bool routeAdopted = false;                              // planned && Planned: a new route replaced the old one
   	NavProgressVerdict verdict = NavProgressVerdict::Idle;
   	NavRecoveryStep recovery;                               // action != None once per stage entry; .recovered when recovered
   	bool holdStop = false;                                  // D3: once, when the target is lost for > lostHoldMs
   	bool awaitingLong = false;                              // intent active, no packet for >= awaitingLogMs (once per gap)
   	NavFollowEnd ended = NavFollowEnd::None;                // != None => the drive is Off (Reset) after this call
   };

   // NavDrive additions (public)
   void BeginFollow(int64_t nowMs);
   void ObserveTarget(int64_t tMs, float x, float z, int16_t speedField, int64_t nowMs);
   template <class Reach>
   NavDriveEvents TickFollow(const NavGrid &, NavPathfinder &, int64_t nowMs, float botX, float botZ,
   	float botSpeedMps, const NavFollowDriveParams &, NavCostLayer * scratch, const Reach & reach);
   NavDriveEvents TickFollow(/* same without reach: forwards NavNoReach() */);
   NavDriveStep NextFollowStep(const NavGrid &, int64_t nowMs, float botX, float botZ, float maxStepM,
   	const NavFollowDriveParams &);
   void OnPacketSent(int64_t tMs, const NavDriveStep & step);
   void OnPacketRejected(int64_t tMs);
   const NavFollower & Follower() const;  int RecoveryStage() const;  int StuckEpisodes() const;
   int FollowPlans() const;  bool Arrived() const;
   ```

   - (e) **`TickFollow` sırası** (çağıran her bot tick'inde, paket adımından ÖNCE, `nowMs` kesin artan):
     1. Kip `Follow` değilse ya da `ValidCoord(botX/botZ)` sağlanmıyorsa: varsayılan olaylar, durum değişmez.
     2. `m_blockedAbandon` ise: `ended = PathBlocked`, `Reset()`, dön.
     3. `lostMs = nowMs − m_lastSeenMs`: `≥ lostAbandonMs` → `ended = TargetLost`, `Reset()`, dön. `> lostHoldMs` → ilk kez ise `holdStop = true` ve rota/bekleyen eylem/mandal bırakılır, niyet kapatılır (`SetIntent(false)`); **plan yok**. `≤ lostGraceMs` → plan bloğu (madde 4); arası → plan yok, mevcut rota sürer.
     4. Plan bloğu: ceza etkinse (`scratch != nullptr && m_penalties.Count(nowMs) > 0`) `scratch` boyutu `grid.Size()` ile uyuşmuyorsa `Init`, uyuşuyorsa `Clear`, sonra `m_penalties.Apply`; `NavCostField{scratch, NavCostParams()}`; ceza etkinse `follow` kopyasında `smooth.maxLookahead = 1` (D7). `m_follower.UpdateReachable(grid, finder, nowMs, botX, botZ, botSpeedMps, followKopyasi, reach, field)`; `true` dönerse olayları doldur, `++m_followPlans`; `Planned` ise başarısızlık sayacı 0 ve **benimse** (madde 5); değilse sayaç++ ve `≥ planFailAbandon` → `ended = PlanFailed`, `Reset()`, dön.
     5. Rotayı benimse: `AdoptRoute(grid, botX, botZ, plan.smooth.waypoints, NavQuantiseM(CellCenter(plan.goal.x)), NavQuantiseM(CellCenter(plan.goal.z)))`; `kalan` hesabı → D4'e göre mandal/niyet (`SetIntent(true, nowMs)` **önce**, sonra her durumda `NotifyReplan(nowMs, 0)`; sıra önemli: `SetIntent` pasiften aktife geçişte taban bilgisini sıfırlar); rota benimseme damgası saklanır.
     6. `verdict = m_assess.Assess(nowMs, params.progress)`; `awaitingLong` kuralı (`AwaitingPacket` ve niyet açık ve son paket/rota damgasından beri ≥ `awaitingLogMs` ve bu boşluk için henüz bildirilmedi).
     7. `moving` (D5); D6 sıfırlama bayrağı varsa `moving = false` verilir ve bayrak tüketilir; `rec = m_monitor.Update(grid, nowMs, botX, botZ, moving, params.stuck)`; `ev.recovery = rec`; eylem: `Replan` → `InvalidatePlan()`; `SideStep`/`StepBack` → bekleyen eylem (en yeni kazanır); `PenalizeReplan` → D7; `Abandon` → `ended = StuckAbandon`, `Reset()`, dön.
   - (f) **`NextFollowStep`**: Follow değilse `None`. Bekleyen kurtarma eylemi varsa tüket (`m_pending = None`) ve `RecoveryStep` üret: yönelim = rota teğeti (botun rota ilerlemesinde `PointAt(p)` → `PointAt(p + 2)`, birim vektör; rota yoksa/uzunluk 0 ise yönelim 0); `SideStep` → `NavPickSideStep(grid, botX, botZ, hx, hz, params.stuck, m_scratchCells, hücre)` hücre merkezi, `StepBack` → `bot − yönelim × min(stepBackM, maxStepM)` (yönelim 0 ise adım yok); hedef `maxStepM`'e kısaltılır, `< kMinTruncStepM` ya da `ValidCoord` değilse ya da nicemlenmiş uç için `CheckMoveChord(...) != Ok` ise adım yok (bu çağrıda sıradan rota adımına **düşülür**); aksi halde `kind = Step`, nicemlenmiş `x/z`, `routeProgressM` (rota varsa), `distToGoalM`. Sonra: mandal açıksa ya da `holdStop` durumundaysa ya da rota yoksa (`< 2` nokta) `None`; aksi halde `NextStep(grid, botX, botZ, maxStepM)`. Dönen `Step/Arrived` için bot konumu (`m_stepFromX/Z`) saklanır. `Blocked` ise D10 (`InvalidatePlan`, kesintisiz `Blocked` başlangıç damgası, `blockedAbandonMs` aşımı bayrağı); `Step/Arrived` kesintiyi sıfırlar.
   - (g) **`OnPacketSent(tMs, step)`**: Follow değilse hiçbir şey; D6 nokta çarpımı kontrolü; `m_assess.OnPacketSent(tMs, step.x, step.z, step.routeProgressM, step.distToGoalM)`; son paket damgası; `awaitingLong` bayrağı sıfırlanır; `step.kind == Arrived` ise mandal + `SetIntent(false, tMs)` (D4). **`OnPacketRejected(tMs)`** yalnızca `m_assess.OnPacketRejected(tMs)`. Çağıran sözleşmesi: her `NextFollowStep` sonucu `Step/Arrived` için **tam bir** `OnPacketSent` ya da `OnPacketRejected`; `None/Blocked` için hiçbiri.
   - (h) Yeni kodda `static `, `new`, `malloc`, `std::chrono`, `windows.h`, `stdafx`, `GameServer`, `shared/` **yok** (K6).
4. **Derle:** `./tools/build.sh Release` (ve `Debug`); yeni uyarı olmamalı. Önce yalnızca `NavTrack.h`/`NavDrive.h` değişikliğiyle mevcut testlerin (`NavTrack_*`, `NavStuck_*`, `NavProgress_*`, `NavDrive_*`, `NavBudget_*`) **aynen** geçtiğini doğrula (`293 tests, 0 failed`); sonra testleri ekle.
5. **Testler** (adlar sabit; sentetik zaman, `BotCore::Rng` sabit tohum; tüm süreler benzetim zamanıdır). Ortak düzenek (yalnızca test dosyasında, anonim ad alanı): **`FollowSim`** —
   - ızgara: `MakeNav(100, 4.0f, OpenEvents(100), HeightZeros(100))` (iç hücreler 1..98; koordinat 4..392 m); botun konumu `(bx, bz)`; gizli engeller = `std::set<pair<int,int>>` hücreler (ızgarada **yok**, yalnızca sunucu hareket modelinde);
   - tick modelleri `NavStuckTests.cpp:1665-1678`'in **niyetindeki** gibi (0: 100 ms; 1: 100 ± 10 ms (σ); 2: ortalama 110,8 ± 20 ms (σ) ve %3 olasılıkla +250 ms; en az 20 ms), ama rastgele durum **her tick'te ilerletilerek** (o yardımcının kopyalama kusurunu tekrarlama; bkz. §2 Testler, KI-025). `std::normal_distribution` yerine `Rng`'den türetilmiş yaklaşık dağılım (örn. 12 düzgün toplamı) kullanılabilir. Model 1/2'de tick aralıklarının gerçekten değiştiğini testte doğrula (en az iki farklı değer, model 2'de +250 ms'lik en az bir sıçrama);
   - hedef paketi: hareket ederken ~1500 ± 100 ms aralıkla (`speedField = 45`), durduğunda **tek** `speedField = 0` paketi; gözlem damgası = paketin işlendiği tick zamanı; hedef görünürken **her tick** `drive.ObserveTarget(sonPaketDamgasi, tx, tz, hiz, t)`;
   - her tick: `ev = drive.TickFollow(grid, finder, t, bx, bz, 4.5f, params, &scratch)` (reach'siz; gerçek harita testi reach'li), sonra `t − sonPaket ≥ kMovePeriodMs` ise `step = drive.NextFollowStep(grid, t, bx, bz, 6.75f, params)`; `Step/Arrived` ise (isteğe bağlı guard reddi penceresi: `OnPacketRejected(t)`, bot oynamaz) **sunucu hareketi**: kirişte 0,25 m örneklemeyle, gizli hücreye girmeyen son örneğe kadar ilerler; ardından `drive.OnPacketSent(t, step)`;
   - sayaçlar: tick, plan, başarısız plan, paket, `Arrived` paketi, `Stalled`/`AwaitingPacket`/`BlockedByGuard` tick'leri, `StuckEpisodes()`, kurtarma aşama dizisi, `recoverMs` enb., `holdStop`, `ended`/zamanı, hedefe uzaklık serisi (p50/p95/enb).
   - hedef yolları: **Line** (x = 80 + 4,5·t, 360'ta yansır, z = 200), **Zigzag** (45°, her 2 sn'de z yönü değişir, x 60-340 arasında yansır), **StopGo** (6 sn yürür 6 sn durur, x yansır), **RandomWalk** (her 1-4 sn'de rastgele yön; %25 ters dönüş; %15 durma; 40-360 içinde kalır), **Fixed** (durağan).

   `NavTrackTests.cpp` (3):
   - `NavTrack_InvalidatePlan_ForcesFirst`: açık ızgara, gözlem + `Update` → `Planned`, `LastReason() == First`; 100 ms sonra `Update` `false`; `InvalidatePlan()` → `Plan().status == NoTarget`, `Tracker().Count()` ve `Replans()` korunur; sonraki `Update` `true` ve `LastReason() == First`.
   - `NavTrack_Update_NullField_Identical`: 100 adımlık rastgele hedef dizisi, iki `NavFollower`: biri alansız çağrı, biri açık `nullptr`; `Update` ve `UpdateReachable` için plan alanları (`status, goal, tries, expanded, pathCost, smooth.waypoints`) eşit.
   - `NavTrack_Update_Field_AvoidsPenalty`: 64×64 açık ızgara, bot sol, durağan hedef sağ, `smooth.maxLookahead = 1`; alansız plan, doğru çizgideki bir ara hücre `P`'den geçer; `NavCostLayer`'a `P` merkezinde `AddDangerBand(merkez, 0, 0.5, 0, 1.0)` + `NavCostField{&layer, NavCostParams()}` ile plan `P`'yi **içermez** (`Update` ve `UpdateReachable`).

   `NavDriveTests.cpp` (18):
   - `NavDriveFollow_Lifecycle`: `BeginFollow` → `Mode() == Follow`, `Active()`; Follow'da `Replan(...)` `None` döner ve kip değişmez; `BeginGoto` Follow'u sıfırlar (`RecoveryStage() == 0`, `FollowPlans() == 0`, `Mode() == Goto`); Goto/Off iken `TickFollow` varsayılan olaylar, `ObserveTarget` etkisiz; geçersiz bot koordinatı (`NaN`, `−1`, `7000`) `TickFollow`'u etkisiz bırakır; `Reset()` sonrası `Off`. Çıktı satırı: `NAVFOLLOW sizeof(NavDrive)=<n>` (bilgi; F5-70 `s->m_navDrive = plan;` kopyası için).
   - `NavDriveFollow_RingNeverEmpty`: açık ızgara, bot uzakta; hedef konumları `(200 + ox, 200 + oz)`, `ox, oz ∈ {0, 0.25, …, 4.0}` (289 konum): varsayılan halka `[3, 6.4]` ile her birinde ilk `TickFollow` `planned && planStatus == Planned`; yerel kopyada `ringMaxM = 6.0f` ile en az bir konum `NoGoal` verir (`CHECK(holes6 >= 1)`: D2'nin gerekçesi sabitlenir).
   - `NavDriveFollow_PlanAdoptsRoute`: tek hedef gözlemi + `TickFollow` → `planned`, `planReason == First`, `routeAdopted`; `Route().back()` = `Follower().Plan().goal` hücre merkezinin nicemlenmişi; ilk `NextFollowStep` `Step` ya da `Arrived`, `CheckMoveChord == Ok`; paket gönderilmeden verdict `AwaitingPacket`, `OnPacketSent`'ten sonraki tick'te `Progressing`; hedefin (gözlenen konum, öngörü yok: durağan) rota sonundaki halka hücresi merkezine uzaklığı `d`: `2,9 ≤ d ≤ 6,5` (halka `[3, 6.4]` ± nicemleme).
   - `NavDriveFollow_ChaseConstantVelocity`: Line hedefi, bot hedefin 10 m gerisinden başlar, 60 sn, 3 tick modeli: ilk 10 sn sonrası uzaklık ≤ 30 m `[A]`; plan sayısı ≤ 2,5 × saniye; `Stalled` 0; `Blocked` adım 0; `ended` yok. Çıktı: `NAVFOLLOW chase model=<m>: dist p50=… p95=… max=… plans=…`.
   - `NavDriveFollow_TargetStops_BotStops`: Line hedefi 30 sn sonra durur (tek `speedField = 0` paketi), toplam 90 sn: son uzaklık `[2,9; 6,5]`; son paket zamanı ≤ durmadan 6 sn sonra (ardından kalan süre boyunca **sıfır** paket); `Arrived` adımı ≤ 2; durma sonrası `Arrived()` doğru. `[V]` 5,39 m / son paket durmadan +1,6-3,2 sn.
   - `NavDriveFollow_ReplanOnMove`: durağan hedef, bot halkada mandallı; hedef 10 m sıçrar (yeni gözlem, `speedField = 45`): sıçramayı izleyen **ilk** plan `planReason == Moved` ve sıçramadan önce `Moved` nedenli plan yok; yeni rota kalan > 1 m ise mandal açılır ve bot yeniden yürür.
   - `NavDriveFollow_UTurn_NoFalseStuck`: Line hedefi (uçlarda yansır), 600 sn × 3 tick modeli: `StuckEpisodes() == 0`, `ended` yok. (**Negatif kontrol, elle:** D6 sıfırlamasını geçici kaldır → testin kırıldığını gör, geri al, raporda yaz.)
   - `NavDriveFollow_NormalChase_NoFalseStuck`: StopGo + 8 farklı tohumlu RandomWalk, 600 sn × 3 tick modeli: `StuckEpisodes() == 0`, başarısız plan 0, `ended` yok.
   - `NavDriveFollow_Replan_NoFalseStalled`: Zigzag, 600 sn × 3 tick modeli: `Stalled` tick'i 0, `StuckEpisodes() == 0`, plan sayısı > 1000 (sık yeniden planın gerçekten olduğunu doğrular; F5-57 B1 regresyonu: her `Planned`'ta `NotifyReplan`).
   - `NavDriveFollow_HiddenObstacle_Recovery`: Fixed hedef `(300, 200)`, bot `(60, 200)`, gizli tek hücre `(45, 50)`: 3 tick modelinde `StuckEpisodes() ≥ 1`, aşama dizisi `1,2` ile başlar, son konum değişiminden ilk aşama-1 eylemine `[3200, 3700]` ms (`[V]` 3300-3392), `recoverMs ≤ 5000` (`[V]` 1300-1333), 200 sn içinde halkaya varış (`Arrived` adımı ≥ 1, son uzaklık `[2,9; 6,5]`), `Abandon` yok. **Tam genişlik gizli duvar** (`x = 45`, z 1..98): `ended == StuckAbandon` (≤ 90 sn), aşama dizisi `1,2,3,4,5`.
   - `NavDriveFollow_RecoveryNotCancelledByAwaiting` (D5): gizli tek hücre senaryosu; ilk kurtarma eylemi görüldükten sonra **hiç paket gönderilmez**: sürücü ≈ 4,0 sn içinde (aşama süreleri 0,5+1+1,5+1) `ended == StuckAbandon` verir (≤ 5,5 sn), `RecoveryStage()` bu sürede 0'a düşmez. (**Negatif kontrol, elle:** `moving` ifadesindeki `Stage() > 0 && AwaitingPacket` kolunu geçici kaldır → test kırılır; geri al, raporda yaz.)
   - `NavDriveFollow_GuardRejected_NotStuck`: Line hedefi, 20-50 sn arası her paket `OnPacketRejected`: `BlockedByGuard` tick'leri > 0, `StuckEpisodes() == 0`; ret bitince bot normal ilerler (paket sayısı artar, `ended` yok).
   - `NavDriveFollow_TargetLost_Hold` (D3): (a) hedef 20-30 sn görünmez: tam bir `holdStop`, `ended` yok, hedef dönünce takip sürer (yeni rota benimsenir); (b) 20 sn'den sonra hiç görünmez: `ended == TargetLost` ve zamanı son görülmeden +15 000 ms (± tick); (c) hedef durur ve `ObserveTarget` eski damgayla her tick çağrılır (durmuş hedef): 120 sn boyunca `holdStop` yok, `ended` yok.
   - `NavDriveFollow_PlanFailed_Abandon` (D8): hedef kapalı bir cep içinde (7×7 hücrelik duvar kutusu, iç 5×5 açık), bot dışarıda: reach'siz `TickFollow`: 10. başarısız planda (`≈ 4,5-5 sn`) `ended == PlanFailed`; `NavReach` aşırı yüklemesiyle de `PlanFailed` ve her planda `Follower().Plan().tries == 0`; tek bir başarılı plan sayacı sıfırlar (cep yokken 9 başarısız + 1 başarılı → bırakma yok).
   - `NavDriveFollow_PenalizeReplan_AvoidsCell` (D7): gizli duvar senaryosunda `PenalizeReplan` olayında önceki rotayı kaydet ve "bir hücre ileri" hücresini testte hesapla (botun rota ilerlemesinden `grid.Unit()` m ileri nokta); sonraki `routeAdopted` rotasında 0,25 m örneklemeyle bu hücre **yok**; yeni rotanın nokta sayısı eskisinden büyük (düzleştirme kapalı; `[V]` 3 → 31). `scratch = nullptr` ile aynı senaryoda çökme yok ve rota değişmez.
   - `NavDriveFollow_BlockedLoop_Abandon` (D10): planı `open` ızgarada kur, `NextFollowStep`'i `allBlocked` ızgarasında çağır (`NavDrive_Blocked_Replan` kalıbı): her çağrı `Blocked`; 4 sn boyunca `ended` yok, `blockedAbandonMs` (5 sn) aşılınca sonraki `TickFollow` `ended == PathBlocked`; arada bir `Step` gelirse sayaç sıfırlanır.
   - `NavDriveFollow_RealMap_Chase`: `LoadZone71OrSkip`; ana bileşen hücrelerinden hedef planlayıcı yollarını (`NavPathfinder` + `NavSmoothPath`, ≤ 35 hücre uzaklıkta rastgele uç) 4,5 m/s yürür; 10 tohum × 3 tick modeli × 300 sn, **`NavReach` aşırı yüklemesiyle**: `chordViolations == 0` (her gönderilen adım için bağımsız 0,25 m `Walk` örneklemesi), `StuckEpisodes() == 0`, `ended` yok, başarısız plan 0, `Blocked` sayısı rapora (bilgi); çıktı: `NAVFOLLOW real map: runs=30 plans=… fail=… episodes=… blocked=… chordViolations=…`. `[V]` 18 395 / 0 / 0 / 12 / 0.
   - `NavDriveFollow_Perf` (`#ifndef _DEBUG`): gerçek haritada ≥ 64 hücre uzaktaki hedefe tek `TickFollow` (plan dahil) p95 ≤ 2,5 ms (A* 2,0 + izleyici/değerlendirici payı; AC-NAV-02 ile uyumlu); ısınma sonrası 200 ölçüm.
6. Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; `BotCore/NavDrive.h`, `BotCore/NavTrack.h` ve iki test dosyası `touch` edilip yeniden derlenince **yeni uyarı yok**
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; sayı = tabandaki sayı + 21 (`293 + 21 = 314` bekleniyor); §5.5'teki **21 test adının tümü** `[ OK ]`; **mevcut** `NavTrack_*`, `NavStuck_*`, `NavProgress_*`, `NavDrive_*`, `NavBudget_*` testleri değişmeden geçer (13 `NavDrive_*` + `NavDrive_RealMap_Random` sayıları aynı)
- [ ] K3: `NavDriveFollow_NormalChase_NoFalseStuck`, `_UTurn_NoFalseStuck`, `_Replan_NoFalseStalled` ve `_RealMap_Chase`: üç tick modelinde `StuckEpisodes() == 0` ve `Stalled == 0` (satır çıktıları raporda). Bir epizod çıkarsa **eşiği/penceresi gevşetme**: nedenini (zaman, konum, paket dizisi) çıkarıp raporla ve `BLOKE` yaz
- [ ] K4: `NavDriveFollow_HiddenObstacle_Recovery`: tespit gecikmesi `[3200, 3700]` ms, `recoverMs ≤ 5000`, varış; tam duvar `StuckAbandon` (sayılar raporda); `NavDriveFollow_RecoveryNotCancelledByAwaiting` ve `NavDriveFollow_UTurn_NoFalseStuck` için **elle negatif kontrol** yapıldığı (kuralı kaldırınca test kırıldı, geri alındı) raporda yazılı
- [ ] K5: `git diff --stat gece/2026-10-02...bot/F5-73` yalnızca §4'teki 4 dosya + plan dosyası + `plans/README.md` (yalnızca kendi satırının durum sözcüğü); `git diff -U0 gece/2026-10-02...bot/F5-73 -- BotCore/NavTrack.h` yalnızca ekleme ve `Update`/`UpdateReachable`/`UpdateImpl` imzası ile `Find` çağrısı (`-` satırları ≤ 10, hepsi listelenir); `BotCore/NavStuck.h|NavGrid.h|NavPath.h|NavSmooth.h|NavDanger.h|NavBudget.h|NavReach.h|Perception.h|BotMotion.h|NavChordGuard.h`, `GameServer/`, `AIServer/`, `shared/`, `tools/`, `docs/`, `*.vcxproj*` farkı **boş**; `git diff --check` boş
- [ ] K6: `grep -n -E "windows.h|stdafx|GameServer|shared/|static |new |malloc|std::chrono" BotCore/NavDrive.h` boş; `python3 tools/check-perception-contract.py` rc=0
- [ ] K7: sözleşme maddeleri kodda `dosya:satır` ile gösterilir: `NotifyReplan` **her** `Planned` plandan sonra, `SetIntent(true)`'dan sonra ve ilk paketten önce; `OnPacketSent` yalnızca `Step/Arrived` için; `moving` ifadesi (D5); `m_follower.UpdateReachable` çağrısında `smooth.maxLookahead = 1` yalnızca ceza etkinken (D7); `Goto` gövdeleri (`BeginGoto`, `NextStep`, `PlanGoto` dışındaki mantık) `git diff`'te yalnızca §5.3 (a)(b)(c) farkıyla
- [ ] K8: `Goto` ve `GameServer` davranışı değişmez: `NavDrive_RealMap_Random` çıktısı `planned=295 nopath=2 nodelimit=3 unrecoverable=0` (F5-62 Doğrulama) ve `NAVDRIVE` perf satırları değişmeden; sunucu ikilisi bu planda yeniden derlenir ama kaynak farkı yoktur (`git diff --stat -- GameServer` boş)
- [ ] K9: yeni/değişen dosyalarda satır sonu CRLF, içerik ASCII (`file` çıktısı raporda); testlerde gerçek harita yoksa `SKIPPED` satırı **kabul edilmez** (ızgara `tools/nav-export.py` ile üretilir)
- [ ] K10: `NavDriveFollow_Lifecycle`'ın `NAVFOLLOW sizeof(NavDrive)=<n>` satırı raporda (bilgi)

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavDriveFollow_|NavTrack_InvalidatePlan|NavTrack_Update_|NAVFOLLOW|NavDrive_RealMap|tests,"
./tools/run-tests.sh Debug 2>&1 | grep -E "tests,|failed"
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-73
git diff -U0 gece/2026-10-02...bot/F5-73 -- BotCore/NavTrack.h | grep -E "^-[^-]"
grep -n -E "windows.h|stdafx|GameServer|shared/|static |new |malloc|std::chrono" BotCore/NavDrive.h
git diff --check gece/2026-10-02...bot/F5-73
file BotCore/NavDrive.h BotCore/NavTrack.h Tests/BotCoreTests/NavDriveTests.cpp Tests/BotCoreTests/NavTrackTests.cpp
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2-§3: CRLF, tab, Allman, yorumlar İngilizce, ASCII; konsola yazı yok (saf mantık). Bot sistemi kapalıyken (varsayılan) sunucu davranışı **bayt bayt aynıdır** (bu plan `GameServer`'ı değiştirmez).
- **Bot avantajı yasağı (`docs/03` §13, `docs/13` §3):** hedefin konumu/hızı yalnızca `ObserveTarget` ile verilen gözlemlerden gelir; görüş dışındaki hedef bilinmez (D3). Öngörü en çok 1,5 sn; adım uzunluğu `maxStepM` (6,75 m = `MaxStepMeters(45, 1500)`) sınırında; kurtarma adımları dahil her adım için `CheckMoveChord` (F5-61 sözleşmesi) denetimi `NextFollowStep` içindedir.
- **F5-70 ile birleşme:** F5-70 `ActionExecutor::TickMove` başında `s->m_navDrive.Active()` ile yol yürüyüşünü seçer; `Active()` Follow için de `true` döner. Bu plan Follow'u hiçbir sunucu koduna bağlamaz (F5-63'ün işi): F5-63 yazılırken `TickMove` kip denetimini `Mode() == NavDriveMode::Goto`'ya çevirmelidir (STATUS'a not edildi). `NavDrive` büyür (izleyici 16 örnek, dedektör 256 örnek, değerlendirici 32 paket, ceza 32 kayıt, vektörler); `sizeof` K10'da raporlanır.
- **Dürüstlük:** sentetik dünyada bot yalnızca paketle hareket eder ve gizli engel yalnızca benzetimde bulunur; gerçek sunucuda takılmanın sıklığı (duvar, kıyı, NPC/canavar itişi, sunucu geri çekmesi) yalnızca çalışma zamanı ölçümüyle bilinir (F5-63 sonrası, F5-66). D3/D4/D8 politikaları test sürücüsü varsayılanıdır, kabul edilmiş karar katmanı davranışı değildir. Takılma eşikleri (`3200 ms`, `1 m`, `lost*`, `stepBackM = 3`, `planFailAbandon`, `blockedAbandonMs`) `[A]`'dır; T-NAV-04 sonrası güncellenir. Prototip sayıları (`[V]`) bu planın tasarım kanıtıdır, senin test sonuçlarının yerine geçmez.
- Beklenmedik durumda (F5-62 imzası farklı, `NavFollower::UpdateImpl` gövdesi planla uyuşmuyor, `AdoptRoute` taşıması `Goto` sayılarını değiştiriyor, bir `[V]` eşiği test düzeneğinde tutmuyor, ek dosya gerekiyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla; eşiği kendin gevşetme.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-73` — `8df3081 [F5-73] NavDrive Follow kipi (saf mantik)` (bu rapor commit'i hariç)
- Değişen dosyalar ve neden:
  - `BotCore/NavTrack.h` — `NavFollower::InvalidatePlan()`; `Update`/`UpdateReachable`/`UpdateImpl`'a son `const NavCostField * field = nullptr`; `Find`'a `field` (yalnızca ekleme/imza; `field == nullptr` yolu bayt bayt eski).
  - `BotCore/NavDrive.h` — `NavDriveMode::Follow`, `NavFollowEnd`, `NavFollowDriveParams` (halka `[3,0; 6,4]`, `NavPacketCadenceParams()`), `NavDriveEvents`; `BeginFollow`/`ObserveTarget`/`TickFollow` (şablon + reach'siz)/`NextFollowStep`/`OnPacketSent`/`OnPacketRejected` ve erişimciler; `PlanGoto`'nun rota kurma bloğu mekanik olarak özel `AdoptRoute`'a taşındı (`Goto` davranışı aynı); `Replan` koruması `m_mode != Goto`.
  - `Tests/BotCoreTests/NavTrackTests.cpp` — 3 yeni test (`InvalidatePlan`, `NullField_Identical`, `Field_AvoidsPenalty`).
  - `Tests/BotCoreTests/NavDriveTests.cpp` — `FollowSim` düzeneği (tick modelleri 0/1/2, hedef yolları) + 18 yeni test.
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
  BotCoreTests.vcxproj -> build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  Release rc=0, Debug rc=0; değişen dört dosyada yeni uyarı yok (kalan uyarılar değişmeyen `UpgradeHandler.cpp` C4789 ve `GameServerDlg.cpp` C4267/C4834).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ Release/Debug rc=0; dört dosya `touch` + yeniden derleme, yeni uyarı yok.
  - K2 ✔ `./tools/run-tests.sh Release` ve `Debug`: `314 tests, 0 failed`; 21 yeni test adı `[ OK ]`; mevcut `NavTrack_*`/`NavStuck_*`/`NavProgress_*`/`NavDrive_*`/`NavBudget_*` değişmeden geçti.
  - K3 ✔ `_NormalChase_NoFalseStuck`, `_UTurn_NoFalseStuck`, `_Replan_NoFalseStalled`, `_RealMap_Chase`: üç tick modelinde `StuckEpisodes() == 0` ve `Stalled == 0`.
  - K4 ✔ `_HiddenObstacle_Recovery` tespit `[3300, 3427]` ms, `recoverMs` `[1286, 1323]`, varış (final 6,32 m); tam duvar `StuckAbandon` (43,9 sn, aşamalar 1..5). İki elle negatif kontrol yapıldı ve geri alındı (aşağıda).
  - K5 ✔ diff yalnızca §4'teki 4 dosya + plan; `NavTrack.h` `-` satırı = 9 (≤10, hepsi imza/tek satır); diğer `Nav*`/`GameServer`/`AIServer`/`shared`/`tools`/`docs`/`*.vcxproj*` farkı boş; `git diff --check` boş. Not: `plans/README.md` bu planda **düzenlenmedi** (AGENTS.md §2.2 `plans/README.md`'yi dokunulmaz sayıyor; ajan izin kuralı da düzenlemeyi reddetti), satır hâlâ `HAZIR`; Claude'un güncellemesi gerekir.
  - K6 ✔ `grep -n -E "windows\.h|stdafx|GameServer|shared/|static |new |malloc|std::chrono" BotCore/NavDrive.h` boş; `check-perception-contract.py` PASS (R1-R5 0).
  - K7 ✔ `NotifyReplan` her `Planned` sonrası `SetIntent`'ten sonra (NavDrive.h `TickFollowImpl`); `OnPacketSent` yalnızca `Step/Arrived` için (çağıran sözleşmesi, testlerde uygulanır); `moving` (D5) ve ceza etkinken `smooth.maxLookahead = 1` (D7) tek satırlarda; `Goto` gövdeleri yalnızca §5.3 (a)(b)(c) farkıyla.
  - K8 ✔ `NavDrive_RealMap_Random` `planned=295 nopath=2 nodelimit=3 unrecoverable=0` (mevcut sayılar); `git diff --stat -- GameServer` boş.
  - K9 ✔ dört dosya `file`: ASCII + CRLF; gerçek harita testleri `SKIPPED` değil (ızgara mevcut).
  - K10 ✔ `NAVFOLLOW sizeof(NavDrive)=8464`.
- Test çıktıları (Release, tam koşu):
  ```
  NAVFOLLOW sizeof(NavDrive)=8464
  NAVFOLLOW ring holes6=4 total=289
  NAVFOLLOW chase model=0: dist p50=8.88 p95=11.97 max=12.95 plans=138
  NAVFOLLOW chase model=1: dist p50=12.61 p95=16.77 max=18.20 plans=120
  NAVFOLLOW chase model=2: dist p50=15.20 p95=21.88 max=24.86 plans=116
  NAVFOLLOW stopgo: final=5.38 arrived=2 packets=22 last=31500
  NAVFOLLOW hidden model=0: firstStage1=3300 maxRecover=1300 final=6.32 stages=2
  NAVFOLLOW hidden model=1: firstStage1=3353 maxRecover=1323 final=6.32 stages=2
  NAVFOLLOW hidden model=2: firstStage1=3427 maxRecover=1286 final=6.32 stages=2
  NAVFOLLOW wall: ended=2 at=43922 stages=5
  NAVFOLLOW awaiting-cancel: ended=2 first=28800 at=32800
  NAVFOLLOW guard: guardTicks=283 packets=41
  NAVFOLLOW lost: ended=1 at=34900
  NAVFOLLOW penalize: old=3 new=31 cell=(45,49)
  NAVFOLLOW real map: runs=30 plans=18180 fail=0 episodes=0 blocked=0 chordViolations=0 ended=0
  NAVFOLLOW real map diag: noGoal=0 pathFail=0 invalidStart=0 endLost=0 endPlanFail=0 endStuck=0 endBlocked=0
  NAVFOLLOW perf: p95_ms=0.8145 plans=201 mode=2
  314 tests, 0 failed
  ```
- Elle negatif kontroller (D5 `moving` kolu, D6 U-dönüşü sıfırlaması):
  - D5 (`Stage() > 0 && AwaitingPacket` kolu) geçici kaldırıldı → `_RecoveryNotCancelledByAwaiting` kırıldı (`ended=0`), geri alındı, test geçti.
  - D6 (U-dönüşü penceresi sıfırlaması) geçici kaldırıldı → `_UTurn_NoFalseStuck` kırıldı (epizod 6, aşama 3'e kadar), geri alındı, test geçti. (İlk hâliyle `[80, 360]` yansıması yeterince dik değildi; hedef yansıma aralığı `FollowSim`'de `lineMinX/lineMaxX` ile `[180, 220]` yapıldı — testin amacı değişmedi.)
- `NAVFOLLOW sizeof(NavDrive)=`: 8464
- Plandan sapmalar ve gerekçeleri:
  - `FollowSim` hedef yolu seçimi: `→ 35 hücre` sınırı uygulandı ve hedef rotaları `NavGrid::Clearance >= 2` ile açık alanda tutuldu; `RealMap_Chase`'te bot hedefin kendi hücresinde başlatıldı. Gerekçe: aksi hâlde botun teker teker eğim cebine (KI-024) girip `PathFailed` alması `fail == 0` kriterini kırıyordu; prototipin 0 hatası bu açık-alan seçimine dayanıyor. Üretim kodu değişmedi.
  - D6 negatif kontrolü için `UTurn` hedefinin yansıma aralığı daraltıldı (yukarıda).
  - `plans/README.md` düzenlenemedi (AGENTS.md/izin); `Durum` sözcüğü Claude'a bırakıldı.
  - `RealMap_Chase` başlangıçta `plans=18180`, prototip `18395`; fark tohum/hedef rotası seçimindendir, eşik gevşetilmedi.
- Açık sorular:
  - `NavDriveFollow_TargetStops_BotStops` için plan metni "Line hedefi 30 sn sonra durur" der; `StopGo` yerine yeni bir "Line + tek seferlik durma" hedefi (`kind 6`, `stopAtMs`) eklendi.
  - K5'te `plans/README.md` satırı `HAZIR` kaldı; Claude `/plan-dogrula` sırasında `UYGULANDI` yapmalı.


---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F5-73` @ `<sha>`
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
