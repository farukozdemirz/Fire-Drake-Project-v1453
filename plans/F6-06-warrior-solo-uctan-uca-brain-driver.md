# F6-06: Warrior solo uçtan uca: `L0Policy` birleşimi, `BrainDriver` sunucu bağlaması ve oyun içi koşu (T-IGT-WAR-01)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; **kapı G6a: T-IGT-WAR-01**) |
| Branch | `bot/F6-06 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-01..F6-05** `KAPANDI`; F4-01..F4-07, F4-20 (`ScriptRunner`: rakip botu betikle sürmek), F4-24 (cast iptali), F4-28 (sprint/Outrage), F4-37 (leg cutting/Scream), F4-38 (CLI-12 speedcheck), F4-40 (envanter doldurma), F4-45 (warrior skill ölçümü), F4-50/F4-51/F4-52 (algı) `KAPANDI`; **F5-55 dilimleri (`plans/F5-55-...md` §1A, 2026-10-03): **F5-59** `NavService` yaşam döngüsü ve harita yükleme (`HAZIR`; `bot/F5-59` dalı açık), **F5-61** kiriş guard'ı, **F5-62** `/bot goto` + waypoint zinciri + `NavPathfinder`/`NavReach`/maliyet katmanı kurulumu, **F5-63** `NavFollower`/takılma/F5-57 sözleşmesi, **F5-64** bütçe + `NAV_*` telemetri, **F5-65** nav durumu temizliği, **F5-66** çalışma zamanı doğrulama (F5-61..F5-66 `TASLAK`; F5-60/F5-67 yalnızca su denetimi/koşullu düzeltme)**: `NavService`, kiriş guard'ı (CLI-08), yol izleme, takılma bağlaması, `NAV_*` telemetrisi, bütçe (F5-53), nav durumu temizliği (ölüm/respawn), arena sınırı/doğuş yolu (F5-51) `KAPANDI` olmalı; **F4-55** (giriş el sıkışması: karşılıklı görünürlük, KI-DEG-01 kalıcı düzeltme; `HAZIR`) `KAPANDI` olmalı (aksi halde botlar ≥ 3 sn arayla doğurulur); **F4-53/F4-60 `KAPANDI`** (gözlenen durum). EVAL-1v1 koşuları için **F8-05 (ScenarioReset/KI-DEG-05; proje sahibi kuyruğunda F6-01'den önce: `docs/STATUS.md:411`; plan dosyası henüz yok)** — bu planın G6a kabulü onu **gerektirmez**, EVAL-1v1 F6-10'dadır |
| İlgili gereksinim / kabul | **T-IGT-WAR-01** (`docs/15` §4.9: MET-TGT-01 p50 ≤ 4 sn; MET-TGT-02 ≥ %70; MET-ACT-02 ≤ %1; MET-NAV-01 ≤ 2/bot-saat), T-WAR-01/02/03, T-SUR-01/03, T-POT-01..03, AC-WAR-01/02/03/05, AC-SUR-01/03/04, AC-ARCH-02 (bot kapalı regresyon), AC-ARCH-06/AC-LRN-03 (algı sözleşmesi), MET-ACT-01/02/03, MET-POT-01..04, MET-SUR-01..07 |
| Tahmini büyüklük | L (10–11 dosya; sınır aşılırsa **F6-06a** = saf birleşim (1-4. dosyalar) ve **F6-06b** = sunucu bağlaması + koşu olarak bölünür) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 yazım turu) |

---

## 1. Amaç

**Proje sahibinin ilk uçtan uca hedefi:** *"Bir warrior botun hedef seçmesi, navigasyonla yaklaşması, uygun skill + R ve pot kullanması, HP %30'un altına düştüğünde geri çekilmesi ve iyileşince yeniden savaşa katılması."* Bu plan, F6-01..F6-05'in saf mantığını tek `L0Policy::Decide` altında birleştirir (docs/06 §4 öncelik sırası), `BrainDriver` ile sunucuya bağlar (algı → karar → mevcut `ActionExecutor`/`BotFairnessGuard` yolu), telemetri yazar ve hedefi **oyun içinde** Claude koşar. Birim testi geçmek bu hedefin kanıtı **değildir**: kapı G6a oyun içi kanıt ister (`docs/17` §4-§5).

## 1a. Neden TASLAK (HAZIR yapmak için)

Ön koşullar:

1. **F6-01..F6-05 `KAPANDI` ve `gece/2026-10-02`'de.** F6-05'in proje sahibi soruları (eşik: tam %30 mu formül mü; `RECOVER` çıkışı) cevaplanmış ve ADR'ye yazılmış olmalı; aksi halde koşu `threatWeight = 0` ile yapılır (§5).
2. **F5-55 dilimleri `KAPANDI`** (F5-59 `HAZIR`, F5-61..F5-66 `TASLAK`; `NavService` API'si F5-59'da yalnızca `Startup/Shutdown/Ready/Grid/Info` tanımlıdır, yol/takip API'si F5-62/F5-63'te yazılacak): bu plandaki **geçici ad** sözleşmesi (§3 madde 3) gerçek adlarla eşlenir. Beklenen yetenekler: (a) hedefe izleme planı ve bir sonraki ara nokta, (b) `NavReachJudgement` (F5-05), (c) `NavRetreatResult` (F5-07) + arena modunda anchor, (d) `NavProgressVerdict` (F5-57) ve takılma kurtarma merdiveni, (e) kiriş guard'ı (`blocked_chord`) ve `directClear` sorgusu (F5-50), (f) yol bütçesi (F5-53). **Dilim boşluğu (2026-10-03 kontrolü):** `NavRetreatPlanner` (güvenli nokta) çağrısı, `NavReachJudge` sonucunun karar katmanına iletilmesi ve `directClear` (düz kiriş) sorgusu bu dilimlerin satırlarında **yazılı değildir** (`plans/F5-55` §1A; F5-59 §3 "kapsam dışı" yalnızca `NavReach` kurulumunu F5-62'ye bırakır). İlgili dilim planı genişletilmeli ya da ek bir F5 dilimi (öneri, kimlik atanmadı) yazılmalıdır.
3. **Çalışma ortamı:** sunucu açıkken derleme yapılmaz (`./tools/run-servers.sh status`/`stop`); `[BOT] BRAIN=1`, `NAV=1`, `TELEMETRY=decisions` ve `TICK_MS=100` ile koşu; arena A koordinatları ve botların başlangıç yerleşimi (DB bot satırı, bot çevrimdışıyken; KI-DEG-05 geçici çözümü) Claude'da.
4. KI-013 (NP 0 ise `Regene` yok): koşu öncesi bot NP'si ≥ 1000'e **elle** yazılır (yalnızca bot satırları; kişisel veri tabloları okunmaz) — kalıcı çözüm F8-05.

Yazım turunda yeniden doğrulanacak referanslar (`gece/2026-10-02 @ 7891f74` üzerinde Claude doğruladı; satırlar kayabilir; F4-60 sonrası `BotManager.cpp`/`BotSession.cpp` satırları yaklaşık +46 kaydı): `GameServer/Bot/BotManager.cpp:2500` (`FillSelfExtras`), `:2552-2640` (`CommandSnap`: tablo kopyaları `m_obs`/`m_npcs`/`m_team`/`m_hp`/`m_skillEvents`/**`m_status`/`m_healObs`** `m_obsLock` altında, satır içi `PerceptionSnapshot` kurulumu: `BuildSnapshot` `:2634`, `AttachHp` `:2635`, `BuildTeam` `:2636`), `:3087-3290` (`TickSessions`: `TickMove` `:3115`, ad tabanlı `m_attackActive` `:3185` ve `m_castPhase` `:3233` sürücü blokları; `FindSession(s->m_attackTargetName)` `:3199` ve `FindSession(s->m_castTargetName)` `:3259` yalnızca **bot** hedef çözer), `GameServer/Bot/ActionExecutor.cpp:292-340` (`BeginAttack`: `targetName` boş olamaz), `:686-830` (`BeginCast`: hedef **ad** ile), `:342-420` (`TickAttack`: hedef görünümü `AttackTarget{id,x,z}`), `GameServer/Bot/ActionExecutor.h:183-404` (tüm `Begin*/Tick*/Request*` imzaları), `GameServer/Bot/BotSession.h:82-92` (`m_attackTargetName`, `m_castTargetName`), `BotSession.cpp:109-116` (`WIZ_ATTACK` yalnızca kendi yankısı), `:8` (`FillSkillMeta`, F4-60), `GameServer/Bot/Telemetry.h:76` (`Emit(level, ev, bot, name, fields, droppable)`), `docs/16` §3.2/§4 (olay alanları), `tools/check-perception-contract.py` (R1-R5).

## 2. Bağlam (okunması zorunlu)

- `docs/06` §4 (öncelik: 1 geri çekilme, 2 kritik pot, 3 takılma kurtarma, 4 peel, 5 ortak hedefe baskı, 6 kaçışı engelleme, 7 healer baskısı, 8 buff bakımı, 9 regroup; **1-4 acil override**), §6.3 (sözde kod), §7 (alt durumlar), §8 (hata/fallback), §9 (solo farkı: restoration/pot daha erken).
- `docs/13` §3 (**thread: bot kararı ve aksiyonu IOCP thread'inde**), §5.2/§5.2a (algı sözleşmesi: O/P/E/G sınıfları; **G sınıfı karar kodundan okunamaz**), §6 (FSM), §7.1, §8 (`ActionExecutor` zamanlaması), §13 (belirlenim, tick sırası döndürülür).
- `docs/15` §4.9 (**T-IGT-WAR-01**), §4.4 (T-WAR/T-SUR/T-POT), §6a (başlangıç sıfırlama: bu planda **manuel**, F8-05'te otomatik).
- `docs/16` §3.2 (`DECISION`: "seçim değiştiğinde veya en az 1 sn'de bir"; `STATE_CHANGE`; `TARGET_SET`; `POTION`), §4 (`DECISION` şeması: `options` ≤ 5, `reason` kapalı liste, `override`), MET-TGT-01/02/04, MET-ACT-01/02/03, MET-NAV-01/02, MET-POT-01..04, MET-SUR-01..07, MET-IDLE-01.
- `docs/17` §5 G6a ("solo, arena A"; kanıt: T-IGT-WAR-01).
- `docs/12` §13.1 (kiriş denetimi zorunlu), §13.3 (çağıran sözleşmesi: `NotifyReplan`, `OnPacketSent`, `OnPacketRejected`, `moving` eşlemesi), §13.4 (arena/doğuş/dönüş).
- `KNOWN_ISSUES.md`: KI-013 (NP 0 ⇒ `Regene` yok), KI-DEG-01 (`ObsTable` tek yönlü görüş: botları ≥ 3 sn arayla doğur), KI-DEG-05 (`ScenarioRunner` sıfırlamıyor), KI-016/KI-017/KI-021.
- `plans/F4-20-betik-calistirici.md`/`bots/config/*.spec` biçimi: rakip botu betikle sürmek için.

## 3. Kapsam

**Yapılacaklar**

1. **Saf birleşim** `BotCore/PolicyL0.h` (yalnızca `BotCore/*.h`; sunucu başlığı yok):
   - `struct BrainMemory { StateMachine fsm; TargetMemory target; WarriorMemory war; PotMemory pot; SurvivalMemory surv; IncomingEwma ewma; Rng rng; BrainParams params; BotRole role; bool solo; bool brainOn; }` (`BrainMemory::Init(seed, slot, role)`: `seed_bot = DeriveBotSeed(seed_episode, slot)`, `docs/13` §13).
   - `Intent L0Decide(const DecisionInput &, BrainMemory &)` (warrior solo; `docs/06` §4 sırasıyla, **ilk acil kural kazanır, ama yuvalar bağımsızdır**):
     1. `snap.self.dead` ⇒ `Dead` (diğer kararlar yok; `Regene` isteği sürücüde).
     2. `IncomingEwma.Update`; `EvaluateRetreat` (F6-05) ⇒ `Retreat`/`Recover`/`lastStand` durum isteği (`StateMachine::Request`, `override_ = true`, `RuleRetreatHp/Ttd/LastStand`).
     3. `DecidePot` (F6-04) ⇒ `Intent.pot` (aynı tick'te 1. ile birlikte; `override_` acil bantta).
     4. Nav takılma (`nav.stalled/abandon`) ⇒ `RuleStuck` (hareket üretilmez; kurtarma nav'da).
     5. Durum `Retreat`/`Recover`: `RetreatMove`, `RECOVER` eylemleri (F6-05); **hedef seçimi/saldırı yok** (yalnızca takipçiye leg cutting izni).
     6. Durum `Prepare`/`Roam`/`Engage`/`Combat`: `SelectTarget` (F6-02) ⇒ hedef yoksa `Roam` + `Hold` (**solo dolaşma ve EV kararı F6-10'a kadar yok**: hedef varsa doğrudan `Engage`); hedef varsa `DecideWarriorPressure` (F6-03); `hpPoll`.
     7. Durum geçişleri `StateMachine::Request` ile (minimum süreler, salınım sayacı); her geçiş `StateEvent`.
   - `L0Decide` **rastgelelik kullanmaz** (tutarlı belirlenim) ve `B0-NAIVE` ile aynı `DecisionInput`'u tüketir (`PolicyNaive`); `BrainMemory.params.baseline` yok (politika seçimi `BrainDriver`'dadır).
2. **Algı → karar köprüsü** `GameServer/Bot/BrainDriver.{h,cpp}` (yeni; IOCP thread'inde çalışır; `BotCore` `.lib`'e bağlanmaz, başlık-yalnızca/göreli include, ADR-0017):
   - `BrainDriver::BuildSnapshot(BotSession*, now, PerceptionSnapshot&)`: **mevcut** `BotManager.cpp:2598-2640` satır içi kurulumunun ortak fonksiyona çıkarılması (`/bot snap` aynı fonksiyonu çağırır; davranışı değişmez). Fonksiyon ayrıca `m_status` ve `m_healObs` kopyalarını (`m_obsLock` altında, F4-60) `DecisionInput.status/heals`'e verir; `statusKnown` = hedefin/üyenin görünürlüğü sürekli ise `true` (F4-61 gelene dek `true`, `[A]`).
   - `BuildTimers(s, now)` (`SelfTimers`: silah `GetItemPrototype(RIGHTHAND)`, `rWaitMs` = `AttackIntervalMs` − son R'den geçen; `typeGateWaitMs` = `MinGatedSince`; ADR-0017 F4-02/F4-03 alanları), `BuildSkills(role)` (`SkillSpec` = `_MAGIC_TABLE` **değer kopyası**; kimlikler `WarSkillId`; `itemOk` = `CanUseItem`), `BuildPots(s)` (çanta taraması, `PotKindOf`, pot değeri), `BuildNav(s)` (`NavService`).
   - **Yalnızca kendi `CUser`'ı** okunur; başka oyuncunun/botun `CUser`'ı **hiçbir yerde** (G sınıfı yasak; `FindSession`/diğer oturum erişimi yok).
3. **Niyet → aksiyon yürütme** `BrainDriver::Execute(s, intent, snap, now)` (yalnızca mevcut `ActionExecutor::Begin*/Tick*/Request*`, **`CUser::HandlePacket`'e doğrudan çağrı yok**; guard reddi `FAIRNESS_REJECT` olarak kalır, brain yeniden dener):
   - `attack`: hedef kimliği değişirse `EndAttack` + `BeginAttack(s, UnitView.name, 1'000'000, now)`; her tick `TickAttack(s, {id, x, z}, now)` (`UnitView`'dan).
   - `cast`: yürütücü boşta (`m_castPhase == CAST_IDLE`) iken `BeginCast(s, skillId, targetName | "", 1, now)`; `TickCast(s, {id, x, y, z, isSelf}, now)`; `cast.cancel` ⇒ `CancelCast`.
   - `pot`: `BeginPotion(s, itemId, 1, now)` + `TickPotion`.
   - `move`: `FollowTarget`/`GotoPoint`/`Retreat` ⇒ `NavService` (geçici adlar `NavService::SetGoal(slot, kind, x, z, ring)`, `NavService::NextWaypoint(...)`; kiriş guard'ı ve `NotifyReplan/OnPacketSent/OnPacketRejected` çağrı sözleşmesi `docs/12` §13.3, F5-55) ardından `BeginMove`/`TickMove`; `DirectStep` ⇒ `BeginMove(x, z, speedField)` (kiriş guard'ından geçmeli); `Hold` ⇒ `StopMove`.
   - `hpPoll` ⇒ `RequestTargetHp`; `stance` ⇒ `SetStance`.
   - Ölüm: `snap.self.dead` ⇒ `kRegeneMinDeadMs` sonra `RequestRegene` (NP 0 ise `no_np`: bir kez loglanır, **yardımcı yol yok**: KI-013) ⇒ `Respawned` ⇒ arenaya yürüme (`GotoPoint` arena merkezi; F5-51/T-NAV-10).
   - `BotManager.cpp`: `TickSessions` içinde `s->m_brainOn` iken ad tabanlı `m_attackActive`/`m_castPhase` sürücü blokları **atlanır** (yoksa `FindSession(name)` insan/NPC hedefte `target_lost` verir).
4. **Komutlar** (aynı komut çekirdeği, konsol + `BotCommands.txt` + `+bot`; `docs/13` §10): `/bot brain <bot> on|off|l0|naive`, `/bot brain <bot> param <P-ID> <değer>` (`SetParam`: aralık dışı reddedilir, `AC-LRN-04`; koşu için `P-SUR-THREAT-WEIGHT 0`), `/bot brain <bot> status` (durum/hedef/son karar özeti). `[BOT] BRAIN` ini anahtarı **varsayılan `0`** (`ENABLED=0` ve `BRAIN=0` iken **hiçbir** brain kodu çalışmaz; AC-ARCH-02).
5. **Telemetri** (`decisions` seviyesi, `Telemetry::Emit`): `DECISION` (seçim değiştiğinde veya ≥ 1 sn: `state`, `sub`, `obs` {self hp/mp, hedef id/mesafe/HP, `enemies_visible`}, `options` ≤ 5, `chosen`, `reason`, `override`), `STATE_CHANGE` (`StateEvent`), `TARGET_SET` (`TargetEvent`), `POTION` (`item`, `deficit`, `effective`, `cd_ms`; `PotMemory::OnPotResult`).
6. `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj`, `GameServer/proj-GameServer.vcxproj`, `.vcxproj.filters` (yeni dosya satırları).
7. Birim testleri `PolicyL0Tests.cpp` (§5.3).
8. **Çalışma zamanı doğrulaması** (Claude koşar; §6 K12-K16; **tek başına birim testi/derleme kanıt değildir**).

**Kapsam dışı (yapılmayacak)**

- Solo dolaşma, eşleşme değerlendirmesi (EV), kaçınma (`AVOID`): **F6-10** (bu planda hedef görünürse saldırılır). EVAL-1v1 ve 36 profil matrisi: F6-10 + F8-05.
- Priest/mage kararı (F6-07/F6-08/F6-09), W-G peel ve takım (F7), `TeamBlackboard`, summon.
- Parametre dosyası yükleme/`PolicyStore` (F9): yalnızca `/bot brain ... param` ile oturum içi.
- Nav'ın kendisi (F5-55 dilimleri), `ScenarioReset`/`win_rule` (F8-05).
- Ad tabanlı test sürücüsünün (`/bot attack`, `/bot cast`) kaldırılması: **kalır** (rakip botu sürmek için).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/PolicyL0.h` | yeni | §3 madde 1 |
| `Tests/BotCoreTests/PolicyL0Tests.cpp` | yeni | §5.3 |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca bir `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca bir `ClCompile` satırı |
| `GameServer/Bot/BrainDriver.h` | yeni | |
| `GameServer/Bot/BrainDriver.cpp` | yeni | |
| `GameServer/Bot/BotSession.h` | değiştir | `BrainMemory` ve brain bayrakları |
| `GameServer/Bot/BotManager.cpp` | değiştir | `BuildSnapshot` çıkarma, `TickSessions` kancası, `/bot brain`, ad tabanlı blokları atlama |
| `GameServer/Bot/BotManager.h` | değiştir | yalnızca bildirimler (gerekirse) |
| `GameServer/proj-GameServer.vcxproj` | değiştir | `BrainDriver.cpp/.h` |
| `GameServer/proj-GameServer.vcxproj.filters` | değiştir | aynı |

`ActionExecutor.*`, `Telemetry.*`, `ScenarioRunner.*`, `ChatHandler.cpp`, `tools/`, `docs/` **değişmez** (komut kaydı gerekirse `BotManager.cpp` içinde kalır). Listede olmayan dosya gerekirse **durup** Uygulayıcı Raporu'nda soru yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-06 gece/2026-10-02`; ön koşulları doğrula (`git log --oneline | grep -E "F6-0[1-5]|F5-"`); sunucular kapalı (`./tools/run-servers.sh status`; `[UP]` ise `stop`); `Durum` → `UYGULANIYOR`.
2. `PolicyL0.h` yaz; `PolicyL0Tests.cpp`: adlar **sabit**:
   - `L0_Priority_Order`: HP %20 + hedef var ⇒ `Retreat` (hedef/saldırı yok, pot var); HP %50 + hedef ⇒ `Combat` (R + Type1); ölü ⇒ `Dead` (başka yuva yok).
   - `L0_Slots_Independent`: aynı tick'te `attack` + `cast` + `pot` birlikte; `cast` tek.
   - `L0_EndToEnd_Sim_Warrior`: sentetik dünya (hedef 40 m, sabit hasar kaynağı): **hedef seç → yaklaş → R + Type1 → HP < %30'da `Retreat` → `Recover` → HP ≥ %65'te `Roam/Engage` → yeniden `Combat`**; `Oscillations == 0`; ölüm 0 (proje sahibi hedefinin birim düzeyi simülasyonu).
   - `L0_Deterministic_Seeded`: aynı tohum + girdi ⇒ aynı `Intent` dizisi (500 tick); `Rng` çağrılmaz.
   - `L0_Disabled_Features_Absent`: `Peel`, `Regroup`, `ReadyForSummon` üretilmez; `EVALUATE`/`AVOID` yok (F6-10).
3. `BrainDriver.{h,cpp}`, `BotSession.h`, `BotManager.cpp/.h`, proje dosyaları: §3 madde 2-6. **Önce** `BuildSnapshot` çıkarma refaktörünü yap ve `/bot snap` çıktısının **aynı** olduğunu (`cmd snap:` satırları) doğrula; sonra kancayı ekle.
4. `./tools/build.sh Release`, `Debug`, `./tools/run-tests.sh`; K1-K11. `Durum` → `UYGULANDI`; Uygulayıcı Raporu (**çalışma zamanı koşusunu DeepSeek yapmaz**).
5. **Çalışma zamanı koşusu (Claude; `/plan-dogrula` içinde):**
   - **S0 ön hazırlık:** `bots/config` altında rakip bot betikleri; bot satırlarında NP ≥ 1000, HP = MaxHP, envanter `tools/bot-refill.sh apply` (sunucular kapalı), başlangıç konumları arena A (1274, 890) ±35 m ya da test için 15 m; botlar ≥ 3 sn arayla doğurulur (KI-DEG-01); sunucu `[BOT] ENABLED=1 BRAIN=1 NAV=1 TELEMETRY=decisions`.
   - **S1 T-WAR-01 (hareketsiz hedef):** `BotWP_K` brain `l0`, hedef `BotWP_E` hareketsiz; 10 tekrar × 60 sn; ölçüt: MET-TGT-01 p50 ≤ 4 sn (**başlangıç ≤ 15 m**, "Çelişkiler" 2), MET-TGT-02 ≥ %70, MET-ACT-01 ≥ %85, MET-ACT-02 ≤ %1, FAIRNESS_REJECT yürütücü kaynaklı 0.
   - **S2 T-WAR-02 (kaçan hedef):** hedef betikle sürekli yürür; leg cutting/Scream/takip sınırı; takip sınırı aşımı 0; AC-WAR-03.
   - **S3 T-WAR-03 (engelli arazi):** köprü/dar geçit arkasında hedef; MET-NAV-01 ≤ 2/bot-saat; engelli hücreye giren hareket 0.
   - **S4 UÇTAN UCA (proje sahibi hedefi):** `/bot brain BotWP_K param P-SUR-THREAT-WEIGHT 0`; rakip `BotWP_E` betikle (skill + R döngüsü) ya da bot satırında başlangıç HP'si düşük; **tek koşuda sırasıyla** (telemetri satırlarıyla kanıt): (1) `TARGET_SET` (`INIT`), (2) `NAV_PATH` + varışta menzil, (3) `ACTION_RESULT` R ve en az bir Type1 `effected`, (4) `POTION` (HP < 0,35'te), (5) `STATE_CHANGE COMBAT -> RETREAT` (`RETREAT_HP`, `ratio < 0,30` ±1 tick), (6) güvenli noktaya varış, `RECOVER`, HP ≥ 0,65, (7) `RETREAT/RECOVER -> ROAM -> ENGAGE -> COMBAT` ve yeniden `TARGET_SET`, (8) `Oscillations(10 s) == 0`, ölüm 0. 5 tekrar; MET-SUR-01/02/03/07 raporlanır.
   - **S5 T-POT-01/02/03** ve T-SUR-03 (kök altında `last_stand`: rakip bot leg cutting/Scream atarak) ölçümleri.
   - **S6 regresyon:** `[BOT] BRAIN` yok/`0` iken ve `ENABLED=0` iken sunucu davranışı ve log satırları değişmez (AC-ARCH-02); `/bot snap` çıktısı önceki sürümle aynı biçim.
   - **S7 performans notu:** `PERF_SAMPLE` ile 2 brain'li botta tick p95; 16 botlu ölçüm T-NAV-11/T-PERF-01'de.
   - **S8 insan oturumu (proje sahibi):** arena A'da istemciyle botu izler; formda "hedef seçimi", "geri çekilme zamanlaması", "bot gibi" davranış 1–5; **ayrı madde**, otomatik kanıt yerine geçmez.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, değişen dosyalar için yeni uyarı yok; K2: `Debug` rc=0
- [ ] K3: `./tools/run-tests.sh Release|Debug` `0 failed`; beş yeni test adı `[ OK ]`; mevcut testler (F6-01..F6-05 dahil) değişmeden geçer
- [ ] K4: `BotCore/PolicyL0.h`'te `windows.h|stdafx|GameServer|shared/` yok; `new|malloc|rand(`, global/static değişken yok; ASCII + CRLF
- [ ] K5: `BrainDriver.cpp` **yalnızca** `ActionExecutor` üzerinden aksiyon üretir: `grep -n "HandlePacket" GameServer/Bot/BrainDriver.cpp` boş; başka oturumun `CUser`'ına erişim yok (`grep -nE "FindSession|m_pUser" GameServer/Bot/BrainDriver.cpp`: yalnızca `s->m_pUser`)
- [ ] K6: `[BOT] ENABLED=0` ve `BRAIN=0` iken davranış değişmez (kod yolu kapalı; `git diff` incelemesi + S6)
- [ ] K7: `/bot snap` çıktısı refaktör öncesiyle aynı (karşılaştırma kaydı Uygulayıcı Raporu'nda)
- [ ] K8: `/bot brain ... param` aralık dışı değeri reddeder (`AC-LRN-04`); varsayılan değerler F6-01 tablosuyla aynı
- [ ] K9: `git diff --stat gece/2026-10-02...bot/F6-06` yalnızca §4'teki dosyalar; `ActionExecutor.*`, `Telemetry.*`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş; yeni dosyalar ASCII + CRLF
- [ ] K10 (Claude): `tools/check-perception-contract.py` R1-R5 `BrainDriver.cpp`'yi de kapsar (aracı Claude genişletir) ve PASS; AC-LRN-03 statik denetimi
- [ ] K11 (Claude): `docs/13` §6/§10, `docs/16` §5.2 ve `docs/06` çelişkileri (F6-01..F6-05'in "Çelişkiler" listeleri) docs'a işlenir; ADR (BrainDriver, ad tabanlı sürücünün atlanması) yazılır
- [ ] K12 (**çalışma zamanı, Claude**): S1-S3 ile **T-IGT-WAR-01**: MET-TGT-01 p50 ≤ 4 sn (≤ 15 m başlangıç), MET-TGT-02 ≥ %70, MET-ACT-02 ≤ %1, MET-NAV-01 ≤ 2/bot-saat, CLI-08 reddi planlayıcı yollarında 0
- [ ] K13 (**çalışma zamanı, Claude**): S4 uçtan uca 5/5 tekrarda sekiz adımın hepsi telemetride **sırasıyla** görülür; MET-SUR-06 = 0
- [ ] K14 (**çalışma zamanı, Claude**): S5 T-POT-02 (eksik 1824'te içme, küçük eksikte yok), T-POT-03, AC-SUR-04 (envanterde olmayan pot 0, ortak 2,5 sn), T-SUR-03
- [ ] K15 (**çalışma zamanı, Claude**): S6 regresyon (bot kapalı) + S7 performans notu
- [ ] K16 (**insan, proje sahibi**): S8 oturum formu; yokluğunda G6a `KABUL_EDILDI` olmaz (faz kabulü ayrı: `docs/17` §4)

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] ise ./tools/run-servers.sh stop
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "L0_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F6-06
grep -n "HandlePacket" GameServer/Bot/BrainDriver.cpp
python3 tools/check-perception-contract.py
git diff --check gece/2026-10-02...bot/F6-06
# Çalışma zamanı (Claude): ./tools/run-servers.sh start; BotCommands.txt ile /bot spawn|brain; Logs/bots/<tarih>/*.jsonl; python3 tools/bot-telemetry-report.py <maç.jsonl>
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2-§3: bot aksiyonları gerçek paketler olarak mevcut handler'lara **yalnızca ActionExecutor** üzerinden gider; thread kuralı (IOCP); **bota avantaj yok**: doğrudan HP/MP/konum yazımı, teleport (`TEST_TELEPORT` eval'de yasak), cooldown atlama, görmemesi gereken bilgiyi okuma yasak. Bot sistemi varsayılan **kapalı**.
- Veritabanı: koşu hazırlığında yalnızca **bot satırları** (`Bot%`) ve proje sahibinin test karakteri; kişisel veri tabloları okunmaz.
- Plan tek başına büyükse (10 dosya sınırı) **F6-06a/F6-06b** olarak bölünmesi `Durum`/Branch alanlarıyla Claude tarafından yapılır; uygulayıcı sınırı kendisi aşmaz.
- Çelişkiler/belirsizlikler:
  1. **Kod ↔ docs:** `docs/13` §8 aksiyonu karar katmanının verdiğini söyler; mevcut `ActionExecutor`/`BotManager` sürücüsü **ad tabanlı** ve hedef konumunu hedef botun `CUser`'ından (G sınıfı) okur (`BotManager.cpp:3199`, `:3259`); bu yol yalnızca test sürücüsüdür ve brain modunda **kapatılır** (algıdan beslenen `AttackTarget`/`CastTarget`).
  2. **MET-TGT-01 ↔ arena başlangıcı:** p50 ≤ 4 sn, 70 m başlangıçta fiziksel olarak imkânsızdır (~15,5 sn yürüme, ~10,4 sn sprint); T-IGT-WAR-01 ölçümünde başlangıç mesafesi ≤ 15 m (yürüme 3,3 sn) olacak şekilde tanımlanmalı ya da MET-TGT-01 "ilk etkili menzile varış" yerine "yaklaşma başlangıcı" tanımına çevrilmeli (docs/16 güncellemesi, proje sahibine soru).
  3. **KI-013:** "NP yenileme/kurtarma F6'da karar" notu: bu planın kararı = F6'da yardımcı kod **yok**, koşu öncesi manuel NP; kalıcı çözüm F8-05 `ScenarioReset`.
  4. `docs/17` F6 kapsamındaki "karar logu" teslimatı: `DECISION` olayı bu planda üretilir; `+bot why <isim>` (GM) ayrı küçük plan.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
- Kabul kriterleri öz-değerlendirme (K12-K16 DeepSeek'e ait değildir):
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
