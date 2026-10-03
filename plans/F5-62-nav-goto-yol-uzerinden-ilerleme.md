# F5-62: `/bot goto <bot> <x> <z>`: komutla verilen hedefe düz çizgi yerine navigasyon yolu üzerinden ilerleme (`BotCore/NavDrive.h`, paylaşılan `NavPathfinder`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-62 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F5-59** (`NavService`) ve **F5-61** (kiriş guard'ı `CheckMoveChord`) `KAPANDI` olmalı. Zaten `KAPANDI`: F5-02 (`NavPathfinder`), F5-03 (`NavSmoothPath`), F5-05 (`NavReach` yargısı, bu planda kullanılmaz), F5-51 (arena/doğuş yolu), F5-53 (`NavPathfinder` ölçüm notu), F5-57 (`NavRoutePoint`, `NavRouteProgressM`). Şemsiye: F5-55 (dilim 4). F5-63, F5-64, F5-65 bu planın `NavDrive`'ına dayanır |
| İlgili gereksinim / kabul | `docs/12` §3-§4.1 (yol bulma/düzleştirme), §13.1 (paket adımı modeli: ~1,5 sn'de bir hedef noktalı `WIZ_MOVE`, ara noktaları sunucu doğrulamaz), §13.4 (doğuş → arena dönüşü), §13.5 (`NavPathfinder` ≈ 4,2 MB); ADR-0006 madde 6 (sahiplik: havuz paylaşımı kararı sunucu entegrasyonunda); AC-NAV-02 (A* p95 ≤ 2 ms), AC-NAV-03; `docs/13` §3 (IOCP thread kuralı) |
| Tahmini büyüklük | M (12 dosya; 4'ü tek satırlık değişiklik; 2'si yeni; saf mantık + dar sunucu bağlaması) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak) |

---

## Neden TASLAK

Bu plan F5-59 (`NavService`) ve F5-61 (`CheckMoveChord`) kodu depoda yokken yazıldı. HAZIR yapma ön koşulları:

1. **F5-59 ve F5-61 `KAPANDI`.** F5-59 sözleşmesi varsayıldı: `NavService::Instance()`, `Ready()`, `Grid()` (`const NavGrid *`, hazır değilse `nullptr`), `GetInfo()`, `[BOT] NAV=1`. F5-59 `NavPathfinder` **kurmaz** (kendi kapsam dışı listesi: "F5-62: paylaşım kararı"): bu plan `NavService.h`'ye paylaşılan örnek erişimcisini ekler (§2 karar). F5-61: `BotCore::CheckMoveChord`, `ChordVerdict`, `SubmitMove` guard'ı.
2. Yazım turunda yeniden doğrulanacak referanslar (`gece/2026-10-02` @ `9fc2dfe` üzerinde okundu; F4-55/F4-60/F5-61 satırları kaydırır): `GameServer/Bot/ActionExecutor.cpp:167-228` (`BeginMove`), `:230-264` (`TickMove`), `:266-284` (`StopMove`), `:286-290` (`AbandonMove`); `GameServer/Bot/BotManager.cpp:626-629` (`move`/`stop` fiil dağıtımı), `:662-668` (bilinmeyen komut metni), `:1062-1124` (`CommandMove`), `:3070-3095` (`TickSessions` hareket dalı, `TickMove` çağrısı); `GameServer/Bot/BotSession.h:73-78` (hareket üyeleri); `BotCore/ScriptPlan.h:59-80` (`IsScriptVerb`, 20 fiil), `Tests/BotCoreTests/ScriptTests.cpp:44-66` (`Script_VerbWhitelist`); `BotCore/NavPath.h:64-81` (`NavPathfinder::Find`), `BotCore/NavSmooth.h:68-91` (`NavSmoothParams/Result/NavSmoothPath`), `BotCore/NavStuck.h` (`NavRoutePoint`, `NavRouteProgressM`, F5-57).
3. **Karar bekleyen konu (HAZIR öncesi):** `goto` hedefi `Walk` olmayan hücredeyse reddedilir mi, en yakın `Walk` hücreye mi alınır? Bu taslak **reddeder** (`invalid_goal`): "en yakın yürünebilir hücreye al" insanın yapacağı bir seçimdir ama komut sürücüsü (test aracı) için belirsizlik üretir. Karar proje sahibinde (§8).

> **Karar (2026-10-03, ADR-0021, proje sahibi):** `Walk` olmayan hedef **reddedilir** (`invalid_goal`); en yakın yürünebilir hücreye alınmaz. Plandaki "karar bekleyen konu" bu karara göre okunur.

## 1. Amaç

`[BOT] NAV=1` iken yeni `/bot goto <bot> <x> <z> [speed]` komutu botu hedefe **planlayıcı yolu** üzerinden yürütür: `NavPathfinder::Find` + `NavSmoothPath` yolu, her ~1,5 sn'de bir **yol üzerinde ilerleyen** `WIZ_MOVE` adımlarıyla (ara noktada durma paketi göndermeden) izlenir; her adım F5-61 kiriş guard'ından geçer. Mevcut `/bot move` (düz çizgi `TickMove`/`StepToward`) **aynen kalır**. `NavPathfinder` paylaşım kararı F5-53 ölçüm notuyla verilir ve kayda geçer. `NAV=0` iken `goto` reddedilir (`nav_off`), başka hiçbir davranış değişmez.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.1 (paket adımı modeli), §13.4 (doğuş ~52 sn Karus / ~142 sn El Morad, 4,5 m/s), §4.1 (A*; `P-NAV-MAX-NODES` 20 000), `docs/12` §3 düzleştirme (`P-NAV-SMOOTH-LOOKAHEAD` 64). `docs/03` CLI-05/CLI-08 (hız alanı, adım ≤ `MaxStepMeters × 1,10 + 0,15`).
- Mevcut düz yürüyüş (`ActionExecutor.cpp:230-264` `TickMove`): paket her ≥ `kMovePeriodMs` (1500 ms); `elapsedMs` 3000'e kırpılır; `maxStep = BotCore::MaxStepMeters(s->m_moveSpeed, elapsedMs)` (45 → 6,75 m); `StepToward` düz çizgi; vardığında `speed = 0, echo = 0` (durma paketi), aksi halde `speed = m_moveSpeed, echo = 3`; `SubmitMove` (`:76-163`) paketi guard'dan ve `CUser::HandlePacket()`'ten geçirir. **Bu plan `TickMove`/`StepToward`/`SubmitMove` adım mantığını değiştirmez**; yeni bir kip ekler (`TickPathMove`) ve `TickSessions` hangi kipi çağıracağını `Drive.Active()` ile seçer.
- `BotCore/NavPath.h:64-81` `NavPathfinder`: **iş parçacığı güvenli değil**, deterministik (aynı örnekte önceki sorgular sonucu etkilemez), `Find(grid, start, goal, NavSearchParams{maxNodes = 20000}, NavPathResult &, const NavCostField * = nullptr)`; durumlar `Found/NoPath/NodeLimit/InvalidStart/InvalidGoal`; ilk çağrıda iç havuzu kurar (`n=513` → 4,02 MiB + `m_heap.reserve(4096)` ≈ 64 KiB; `plans/F5-53-nav-sorgu-butcesi-ve-onbellek.md:151` K8 ölçüm notu).
- `BotCore/NavSmooth.h:85` `NavSmoothPath(grid, path, params, out)`: `out.waypoints` (`NavCell`, ilk ve son korunur; yol hücre **merkezleri**), `out.length` (m).
- `BotCore/NavStuck.h` (F5-57): `NavRoutePoint{x,z}`, `NavRouteProgressM(pts, n, x, z)` (polilin üzerine izdüşümün başlangıçtan kümülatif mesafesi) — yol ilerlemesi için kullanılır.
- Hareket üyeleri: `GameServer/Bot/BotSession.h:73-78` `m_moveActive`, `m_moveTargetX/Z`, `m_moveSpeed`, `m_moveLastSent`, `m_movePackets`; "meşgul" tanımı `m_moveActive`'e bağlıdır (`ActionExecutor.cpp:880, 942, 1640`; `BotManager.cpp:3086` cast iptali): yol izleme **`m_moveActive = true` tutar**, bu yüzden cast-iptal/duruş/`busy` mantığı değişmeden çalışır.
- Ölü bot dalı: `BotManager.cpp:3070-3085` ölü botta `AbandonMove` çağırır; bu plan `AbandonMove`'a `Drive.Reset()` ekler (tam temizlik F5-65).
- `BotCore/ScriptPlan.h:59-80` betik fiil beyaz listesi (20 fiil) ve `ScriptTests.cpp` `Script_VerbWhitelist`: F5-66 koşuları `Scripts/*.txt` ile yapılacağından `goto` fiili listeye girer; `ScriptRunner.cpp:214` komutu `ExecuteCommand`'a verir (ek kod gerekmez).
- `tools/check-perception-contract.py` R2: `GetMap`/`GetUserPtr` kısıtlı sembollerdir; yeni kod yalnızca botun **kendi** `CUser` konumunu ve `NavService` ızgarasını okur (`GetMap` kullanılmaz; hedef doğrulaması F5-59 ızgarasının `Walk`'udur). `BeginGoto` botun kendi hedefinin `IsValidPosition` denetimini `BeginMove`'daki gibi yapmak isterse `GetMap` allowlist'i bozulur: **yapma**, ızgara denetimi yeter.

### Tasarım kararı: `NavPathfinder` paylaşımı (F5-55 §4 açık sorusunun yanıtı)

**Karar: tek paylaşılan örnek, `NavService`'te; yalnızca IOCP iş parçacığında kullanılır.**

| Seçenek | Bellek | Güvenlik | Sonuç |
|---|---|---|---|
| Bot başına örnek | 16 bot × 4,02 MiB = **64,3 MiB**, 20 bot ≈ 80 MiB, ilk `Find`'da her bot 4 MiB ayırır (Win32, 2 GB adres alanı) | Güvenli (yarış yok) | Reddedildi (bellek, ilk-sorgu ayırma gecikmesi bot sayısıyla çarpılır) |
| **Tek paylaşılan örnek (seçildi)** | **4,02 MiB** | Tüm çağıranlar IOCP iş parçacığındadır: `BotManager::Tick()` (`BotManager.h`: "IOCP worker thread only") → `ProcessCommands` → komutlar; `TickSessions` → hareket. Aynı örnek ardışık çağrılır; `NavPathfinder` deterministik ve önceki sorgudan etkilenmez (`NavPath.h` açıklaması) | Seçildi |
| Sorgu başına geçici örnek | 4 MiB ayır/boşalt | Güvenli | Reddedildi (A* p95 0,5 ms iken ayırma/sıfırlama maliyeti baskın) |

Uygulama: `NavService`'e `BotCore::NavPathfinder & SharedPathfinder()` eklenir (üye `m_pathfinder`; ızgara hazır değilken çağrılmaz). **Thread denetimi:** ilk çağrıda `GetCurrentThreadId()` saklanır; sonraki bir çağrı farklı iş parçacığından gelirse `Bot_*.log`'a **bir kez** `NavService: pathfinder used from thread <n> (first <m>) VIOLATION` yazılır (çalışma zamanı kanıtı: K10). `NavFollower` (F5-63) aynı örneği parametre olarak alır (`NavFollower::Update(grid, pathfinder, ...)`; ADR-0006 madde 6). **Gerekçeli bildirim:** tek-thread varsayımı F5-53 notunun ("tek thread'de seri çağrı") ve `docs/13` §3'ün sonucudur; ileride arka plan iş parçacığı (ADR-0005 v2) eklenirse bu karar yeniden açılır.

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavDrive.h` (yeni, saf mantık; yalnızca standart kütüphane + kardeş başlıklar; global/static yok; `NavDrive` bu planda yalnızca **`Goto`** kipini taşır, F5-63 `Follow` ve takılma üyelerini ekler): rota kurma, yol üzerinde adım üretme (`NextStep`), varış, durum eşlemesi.
2. `NavService.h`: paylaşılan `NavPathfinder` erişimcisi + thread denetimi (F5-59 dosyasına küçük ekleme).
3. `ActionExecutor`: `BeginGoto`, `TickPathMove` (yeni kip); `BeginMove`/`StopMove`/`AbandonMove`'a **tek satır** `Drive.Reset()`. `TickMove`/`StepToward`/`SubmitMove` **değişmez**.
4. `BotSession.h`: `BotCore::NavDrive m_navDrive;` üyesi. `BotManager`: `goto` komutu ve `TickSessions` kip seçimi.
5. `ScriptPlan.h` fiil listesine `goto` (20 → 21) ve `Script_VerbWhitelist` güncellemesi.
6. Birim testleri `Tests/BotCoreTests/NavDriveTests.cpp`.
7. Çalışma zamanı doğrulaması (Claude; §6 K10-K15).

**Kapsam dışı (yapılmayacak)**

- `/bot follow`, hız kestirimi, yeniden yol hesaplama, takılma tespiti/kurtarma (F5-63). **F5-62'de yol bir kez hesaplanır;** hedef sabit olduğundan yeniden hesaplama yoktur (tek istisna: kiriş engeli, aşağıda).
- Bütçe zamanlayıcısı, faz kaydırma, yol önbelleği, `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY`, `PERF_SAMPLE` nav payı (F5-64): bu planda `goto` komutunda yol **anında** hesaplanır (komut başına bir sorgu; tick'te 16 bot aynı anda `goto` alırsa 16 sorgu ≈ 8 ms: `docs/12` §13.5; çalışma zamanı testleri komutları ≥ 1 sn arayla verir).
- Ölüm/respawn/despawn/bölge değişimi temizliği (F5-65; yalnızca `AbandonMove`/`StopMove`/`BeginMove` temel temizliği buradadır).
- Arena modu maliyet alanı (`NavCostLayer::AddForbidOutsideDisc`), tehlike katmanı, `NavReach` yargısı, hedef bırakma kararı: yok. `Find` bu planda `field = nullptr` (yalnızca mesafe) ile çağrılır.
- `BotCore/NavGrid.h`, `NavPath.h`, `NavSmooth.h`, `NavStuck.h`, `NavSegment.h`, `NavChordGuard.h` **değişmez**.
- `docs/` (Claude: `docs/12` §13.4, `docs/13` komut tablosu, ADR-0006 Ek F5-62, `docs/STATUS.md`).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavDrive.h` | yeni | ASCII + CRLF; saf mantık |
| `BotCore/BotCore.vcxproj` | değiştir | tek satır `<ClInclude Include="NavDrive.h" />` |
| `Tests/BotCoreTests/NavDriveTests.cpp` | yeni | |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | tek satır `<ClCompile Include="NavDriveTests.cpp" />` |
| `GameServer/Bot/NavService.h` | değiştir | yalnızca `SharedPathfinder()` + thread denetimi üyeleri (F5-59 `Info`/`Grid` API'si değişmez); `NavService.cpp` yalnızca denetim log satırı gerekirse |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `BeginGoto`, `TickPathMove` bildirimleri |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yeni iki fonksiyon; `BeginMove`/`StopMove`/`AbandonMove`'a tek satır `Drive.Reset()` |
| `GameServer/Bot/BotSession.h` | değiştir | yalnızca `BotCore::NavDrive m_navDrive;` (+ `#include "../../BotCore/NavDrive.h"`) |
| `GameServer/Bot/BotManager.h` | değiştir | `void CommandGoto(const std::string & args);` |
| `GameServer/Bot/BotManager.cpp` | değiştir | `goto` dağıtımı (`:626` civarı), `CommandGoto` (`CommandMove`'u kalıpla), bilinmeyen komut metnine `goto`, `TickSessions`'ta kip seçimi |
| `BotCore/ScriptPlan.h` | değiştir | yalnızca `kVerbs` listesine `"goto"` ve yorumda 20 → 21 |
| `Tests/BotCoreTests/ScriptTests.cpp` | değiştir | yalnızca `Script_VerbWhitelist`: dizi + döngü sınırı 21 |

12 dosya (hedef ~10'u aşar: 2 `.vcxproj` ve 2 fiil-listesi dosyası tek satırlıktır; bölünmesi gerekirse `ScriptPlan.h`/`ScriptTests.cpp` ayrı plana alınır). Yeni `.cpp` yok: `proj-GameServer.vcxproj` değişmez. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-62 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucu `[UP]` ise `./tools/run-servers.sh stop`. F5-59 `NavService.h` ve F5-61 `NavChordGuard.h` imzalarını aç ve doğrula (sapma varsa **dur**). `python3 tools/nav-export.py`.
2. **`BotCore/NavDrive.h`** (iskelet; sözleşme sabit, gövde uygulayıcıya aittir):

   ```cpp
   #pragma once
   // Path-following drive for the real move packets (F5-62; docs/12 s13.1, s13.4). Pure logic: standard
   // library and sibling headers only, no global/static state, no clock (caller-supplied ms stamps).
   // One drive per bot. F5-62 carries the Goto mode only; F5-63 adds Follow and the stuck members.
   #include "NavChordGuard.h"
   #include "NavPath.h"
   #include "NavSmooth.h"
   #include "NavStuck.h"      // NavRoutePoint, NavRouteProgressM (F5-57)
   #include <vector>
   namespace BotCore
   {
   	enum class NavDriveMode { Off, Goto };
   	enum class NavPlanStatus { None, Planned, InvalidStart, InvalidGoal, NoPath, NodeLimit };
   	struct NavDriveParams
   	{
   		NavSearchParams search;   // P-NAV-MAX-NODES 20000
   		NavSmoothParams smooth;   // P-NAV-SMOOTH-LOOKAHEAD 64
   	};
   	struct NavDriveStep
   	{
   		enum Kind { None, Step, Arrived, Blocked } kind = None;
   		float x = 0.0f, z = 0.0f;      // packet position (unquantised; the executor quantises)
   		float routeProgressM = 0.0f;   // for NavProgressAssessor (F5-63)
   		float distToGoalM = 0.0f;
   		bool  truncated = false;       // the full step was cut at a waypoint because the chord was blocked
   	};
   	class NavDrive
   	{
   	public:
   		void Reset();                              // mode Off, route empty (F5-65 extends the reset reasons)
   		bool Active() const;
   		NavDriveMode Mode() const;
   		// Plans botPos -> goal with the SHARED pathfinder (caller guarantees IOCP thread) and arms the walk.
   		// Route = [botPos, centres of the interior smoothed waypoints, exact goal point]. InvalidGoal when the
   		// goal cell is not Walk / off-grid; InvalidStart when the bot's own cell is not Walk (see F5-61 D5:
   		// an exempted non-Walk start cell is NOT planned from: report it, the caller refuses).
   		NavPlanStatus BeginGoto(const NavGrid &, NavPathfinder &, float botX, float botZ,
   			float goalX, float goalZ, const NavDriveParams &, int64_t nowMs);
   		// Next move packet: the point `maxStepM` of ROUTE length ahead of the bot's projection, crossing
   		// waypoints without stopping. Every candidate chord is checked with CheckMoveChord: a blocked chord is
   		// truncated at the next waypoint; still blocked -> kind Blocked (nothing to send). Arrived when the
   		// remaining route length <= maxStepM (the packet carries the exact goal point).
   		NavDriveStep NextStep(const NavGrid &, float botX, float botZ, float maxStepM);
   		const std::vector<NavRoutePoint> & Route() const;
   		float RouteLengthM() const;
   		float GoalX() const; float GoalZ() const;
   		int   Replans() const;                       // plans since Reset (F5-62: 0 or 1; a Blocked step may replan once)
   	};
   }
   ```

   `NextStep` kuralları: (a) bot konumunun rota izdüşümü `p` = `NavRouteProgressM`; (b) aday uç = rotanın `min(p + maxStepM, toplam)` konumundaki nokta; (c) `CheckMoveChord(&grid, botX, botZ, uç)` `Ok` değilse uç, izdüşümden sonraki **ilk ara noktaya** çekilir (`truncated = true`) ve yeniden denetlenir; yine `Ok` değilse `Blocked`; (d) kalan rota uzunluğu ≤ `maxStepM` ise `Arrived` ve uç = tam hedef noktası; (e) `Active() == false` → `None`. Sürücü **bir kez** `Blocked` aldığında çağıran, botun **mevcut konumundan** `BeginGoto` ile yeniden planlar (`Replans() ≤ 1`); ikinci `Blocked` yürüyüşü bitirir (`path_blocked`).
3. **`BotCore.vcxproj` / `BotCoreTests.vcxproj`** tek satır eklemeleri.
4. **`GameServer/Bot/NavService.h`**: `BotCore::NavPathfinder & SharedPathfinder();` (+ ilk-çağıran `DWORD` ve ihlal bayrağı). Yalnızca IOCP iş parçacığı sözleşmesi yorumda yazılır. `Grid()`/`Ready()`/`GetInfo()` **değişmez**.
5. **`ActionExecutor`** (`ActionExecutor.h` bildirim, `.cpp` gövde; `MoveOutcome` yeniden kullanılır; yeni `reason` değerleri sabit metin):
   - `static MoveOutcome BeginGoto(BotSession * s, float tx, float tz, int16 speedField, std::chrono::steady_clock::time_point now);` `BeginMove`'un doğrulamalarını aynen tekrarlar (`not_in_game`, `dead`, `sitting`, `speed_field`; **`bad_target` yerine** ızgara denetimi) ve ek reddetmeler: `nav_off` (`NavService::Instance().Grid() == nullptr`), `nav_zone` (`GetZoneID() != ZONE_RONARK_LAND`), `invalid_goal`, `invalid_start`, `no_path`, `node_limit`. Başarıda: `m_moveActive = true; m_moveTargetX/Z = hedef; m_moveSpeed = speedField; m_movePackets = 0; m_moveLastSent = now - kMovePeriodMs` (ilk paket aynı tick'te), `m_navDrive` rotası kurulu; sonuç `SENT`. Plan süresini `steady_clock` ile ölç, `Bot_*.log` satırına yaz (K11).
   - `static MoveOutcome TickPathMove(BotSession * s, now)`: `TickMove` ile **aynı** süre/kırpma kuralları (`elapsed ≥ kMovePeriodMs`; `elapsedMs ≤ 2 × kMovePeriodMs`; `maxStep = MaxStepMeters(m_moveSpeed, elapsedMs)`); `NavDriveStep step = s->m_navDrive.NextStep(grid, x, z, maxStep)`; `Step` → `SubmitMove(..., speed = m_moveSpeed, echo = 3, arrived = false)`; `Arrived` → `SubmitMove(..., speed = 0, echo = 0, arrived = true)` ve `m_navDrive.Reset()`; `Blocked` → bir kez yeniden planla, değilse `m_moveActive = false`, `REFUSED "path_blocked"`; `SubmitMove` `REFUSED`/`FAILED` dönerse `m_navDrive.Reset()`. **Ara noktada durma paketi yoktur:** paket yalnızca varışta `speed = 0` taşır.
   - `BeginMove`, `StopMove`, `AbandonMove`: ilk geçerli noktada `s->m_navDrive.Reset();` (tek satır; `BeginMove`'da doğrulamalar geçtikten sonra, yeni düz yürüyüş yol yürüyüşünün yerini alır). `TickMove`/`StepToward`/`SubmitMove` **değişmez** (`git diff` kanıtı K6).
6. **`BotSession.h`**: `BotCore::NavDrive m_navDrive;       // IOCP thread only: path-following state (F5-62)`.
7. **`BotManager`**: `ExecuteCommand`'a `goto` → `CommandGoto(args)`; `CommandGoto` = `CommandMove` kalıbı (aynı sözdizimi `goto <bot> <x> <z> [speed]`, aynı ayrıştırma ve `unknown or not spawned`/`not in game` log satırları); sonuç satırı: `BotManager: cmd goto: <bot> planned <w> waypoints, route <m> m, expanded <n>, <ms> ms, walking to (<x>, <z>) at speed <s>` veya `... refused (<reason>)`. `TickSessions` hareket dalında (`:3070-3095`): `s->m_navDrive.Active() ? ActionExecutor::TickPathMove(s, now) : ActionExecutor::TickMove(s, now)`; `ARRIVED`/`REFUSED`/`FAILED` log satırları mevcut kalıpla aynı (`arrived at ... after N packets`, `move stopped (<reason>)`). Bilinmeyen komut metnine `goto` ekle.
8. **`ScriptPlan.h`/`ScriptTests.cpp`**: `"goto"` fiilini ekle; `Script_VerbWhitelist` 21 fiili sınasın (`!IsScriptVerb("goto2")` negatif vakası ekle).
9. **Testler** (`NavDriveTests.cpp`; `MiniTest.h`; sentetik ızgara `Init` + `Build`; hareket simülasyonu = her adımda bot konumunu paket konumuna taşı, `maxStep = 6,75`; adlar sabit):
   - `NavDrive_Goto_OpenField`: engelsiz 64×64: tüm adımlar `Step`, sonuncusu `Arrived` ve konum = hedef; paket sayısı = `ceil(uzunluk / 6,75)`; hiçbir adım `maxStep + 1e-3`'ü aşmaz.
   - `NavDrive_Goto_Detour_AllChordsWalk`: ortada kapılı duvar: rota uzunluğu ≥ düz mesafe; **her** adım kirişi `CheckMoveChord == Ok`; varır.
   - `NavDrive_Goto_NoStopAtWaypoint`: köşeli rotada (L biçimi) ilk `Arrived`'a kadar her adım `Step` ve (kısaltma yokken) uzunluğu `maxStep`'e eşit (`truncated == false`); köşeden geçen adım durmaz.
   - `NavDrive_Goto_ChordBlockedTruncates`: tam adım köşeyi kesip duvara değecek şekilde kurulmuş durum: adım ilk ara noktaya kısaltılır (`truncated`), kiriş `Ok`; ara noktada bitince sonraki adım yoldan devam eder.
   - `NavDrive_Goto_StatusMapping`: `Walk` olmayan hedef → `InvalidGoal`; `Walk` olmayan başlangıç → `InvalidStart`; yalıtılmış cep → `NoPath`; `maxNodes = 8` → `NodeLimit`; hiçbirinde `Active()` olmaz.
   - `NavDrive_Goto_OffRoute`: yürüyüşün ortasında bot yoldan 2 m yana itilir: sonraki adımlar izdüşümden sürer, geriye atlamaz, varır.
   - `NavDrive_Reset_Inactive`: `Reset()` sonrası `NextStep` `None`; `Active() == false`; rota boş.
   - `NavDrive_RealMap_RespawnReturn`: gerçek harita (yoksa `SKIPPED`): Karus (1369,9; 1090,3) ve El Morad (630,0; 920,0) doğuşlarından arena A (1275,0; 890,0) hedefine `BeginGoto` (**doğuş hücresi `Walk` değilse `NearestWalk` ile; rapora yaz**) → `Planned`; simüle yürüyüşte `blocked == 0`, `Arrived` ≤ 1 m; satır `NAVDRIVE respawn karus|elmorad: route_m=<m> packets=<n> eta_s=<route/4,5> blocked=0`.
   - `NavDrive_RealMap_Random`: 300 `near64` çift: hepsi `Planned` (veya dürüstçe `NoPath` sayılır) ve yürüyüşte `blocked == 0`, `Arrived`; `NAVDRIVE random pairs=<n> planned=<p> blocked=0`.
   - `NavDrive_Perf` (`#ifndef _DEBUG`): `BeginGoto` (Find + Smooth) `near64` p95 ≤ 2,0 ms (AC-NAV-02 kapısı; PM-M3 ile uyumlu).
10. Derle/test (§7); `check-perception-contract.py` rc=0. Sunucu çalıştırma uygulayıcıya düşmez (K10-K15 Claude). Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; değişen dosyalar `touch` edilince yeni uyarı yok (özellikle `BotSession.h` `NavDrive.h`'yi ilk kez dahil ettiği için `ActionExecutor.cpp`, `BotManager.cpp`, `ScenarioRunner.cpp`, `ScriptRunner.cpp`, `Telemetry.cpp` yeniden derlenir)
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; on yeni `NavDrive_*` test adı `[ OK ]` (§5 adım 9) ve güncellenmiş `Script_VerbWhitelist`; mevcut testler değişmeden geçer
- [ ] K3: gerçek-harita testleri `SKIPPED` **değil**; raporda `NAVDRIVE respawn karus ...`, `NAVDRIVE respawn elmorad ...` ve `NAVDRIVE random ...` satırları (`blocked=0`; her iki ulus `Planned`)
- [ ] K4: `git diff --stat` yalnızca §4 (12 dosya, 2 yeni) + plan dosyası; `BotCore/NavGrid.h|NavPath.h|NavSmooth.h|NavStuck.h|NavSegment.h|NavChordGuard.h`, `docs/`, `tools/`, `proj-GameServer.vcxproj` farkı **0**; `git diff --check` boş
- [ ] K5: `NavDrive.h`'de `grep -n -E "windows.h|stdafx|GameServer|shared/|static |new |malloc"` boş; `python3 tools/check-perception-contract.py` rc=0 (R1-R5 PASS; `BotSession.h` yeni üyesi R5'i bozmaz)
- [ ] K6: `ActionExecutor.cpp`'de `TickMove`, `StepToward` çağrısı ve `SubmitMove` gövdesinin `git diff`'i **boş**; `BeginMove`/`StopMove`/`AbandonMove` farkı yalnızca birer `m_navDrive.Reset();` satırı (kanıt `git diff -U0` çıktısı raporda)
- [ ] K7: `TickPathMove`'da hiçbir `speed = 0` paketi varış dışında üretilmez (kod incelemesi, `dosya:satır`); `Blocked` yolunda `HandlePacket` çağrısı yok (F5-61 guard'ı `SubmitMove` içindedir)
- [ ] K8: `[BOT] NAV=0` ve `ENABLED=0` davranışı değişmez: `NAV=0` iken `/bot goto` `refused (nav_off)` yazar ve **başka hiçbir** komutun çıktısı/davranışı farklı değildir (K13)
- [ ] K9: yeni dosyalar yalnızca ASCII + CRLF (`file` çıktısı); `NavService.h` UTF-8 BOM durumu F5-59'daki gibi korunur (`head -c3 | xxd -p` raporda)
- [ ] K10 (Claude, çalışma zamanı): `ENABLED=1, NAV=1, TELEMETRY=decisions`; bir Karus botu doğuş noktasından `/bot goto <bot> 1275 890`: `Bot_*.log`'da `cmd goto: ... planned <w> waypoints, route <m> m, expanded <n>, <ms> ms`; bot yürür (`ACTION_SUBMIT` `Move` paketleri, `echo:3`, ara noktada `speed:0` paketi **yok**), varır (`arrived ... after N packets`), `N ≈ ceil(route / 6,75)`; paket sayısı ve süre rapora; `FAIRNESS_REJECT` (`blocked_chord`) **0**; `pathfinder ... VIOLATION` satırı **yok**
- [ ] K11 (Claude): aynı için El Morad botu doğuş noktasından arena A'ya; iki ulusun yol uzunluğu/süresi (`docs/12` §13.4 beklentisi Karus ~52 sn, El Morad ~142 sn ± %20 **yalnızca bilgi**; kabul ölçütü F5-66'dadır)
- [ ] K12 (Claude): düz yürüyüş korunur: `/bot move` aynı bot için eskisi gibi çalışır (düz çizgi, engele değmeyen hedef); `/bot move` bir `goto`'nun ortasında verilirse `goto` iptal olur ve düz yürüyüş başlar; `/bot stop` yol yürüyüşünü durdurur ve botun yeniden `goto` ile başlayabildiği görülür
- [ ] K13 (Claude): `NAV=0`: `/bot goto` `refused (nav_off)`, `/bot move` aynı; `ENABLED=0` iken hiçbir bot satırı yok
- [ ] K14 (Claude): ulaşılamayan/geçersiz hedef: `Walk` olmayan hedef (`invalid_goal`), ana bileşen dışındaki cep hedefi (`no_path` veya `invalid_goal`), ızgara dışı hedef — bot **yürümez**, `Bot_*.log`'da sebep; çökme yok
- [ ] K15 (Claude): sunucu `stop` ile kapanır; ilk-`Find` süresi (havuz ayırma dahil) ve sonraki sorgu süreleri rapora (bilgi: `docs/12` §13.5.2 kuralı 9 etiketi `[V: ORT-S, Y1, goto planı]`)

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavDrive_|NAVDRIVE|Script_VerbWhitelist|tests,"
./tools/run-tests.sh Debug
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-62
git diff -U0 gece/2026-10-02...bot/F5-62 -- GameServer/Bot/ActionExecutor.cpp | grep -E "^[-+]" | grep -v "^+++\|^---" | head -80
git diff --check gece/2026-10-02...bot/F5-62
# Claude (çalışma zamanı): ./tools/run-servers.sh start ; echo "goto <bot> 1275 890" >> /mnt/c/dev/fdp/server/BotCommands.txt ; grep -a -E "cmd goto|arrived|VIOLATION" /mnt/c/dev/fdp/server/Logs/Bot_*.log ; ./tools/run-servers.sh stop
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3: CRLF, tab, Allman, yorumlar İngilizce, yeni dosyalar ASCII; konsol spam'i yok (yalnızca `Bot_*.log`); `NAV=0` iken sunucu davranışı değişmez.
- **Bot avantajı yasağı (`docs/03` §13, `docs/13` §3):** yol, botun kendi konumundan ve herkesin sahip olduğu zone verisinden (F5-59 ızgarası) hesaplanır; hedef komutla verilir (bot hedefin yerini **bulmaz**). Hız: `m_moveSpeed` ≤ sunucu sınırı (CLI-05) ve adım ≤ `MaxStepMeters` (CLI-08) `SubmitMove` guard'ında korunur; yol izleme paketleri **aynı** sıklık ve hızla gider (`kMovePeriodMs`), yani botu hızlandırmaz.
- Thread: tüm çağrılar IOCP iş parçacığında; paylaşılan `NavPathfinder` başka iş parçacığından çağrılırsa karar geçersizdir (`VIOLATION` satırı = BLOKE).
- Bu plan `field = nullptr` ile **düz mesafe** planlar: arena modunda yasaklı-disk kuralı yoktur. "Arena sınırı içinde kal" (docs/12 §13.4) bu planda **sağlanmaz**; arenaya varış hedef noktasında durmakla sağlanır. Arena sınırının sunucu bağlaması hiçbir F5-59..F5-66 diliminde yoktur (açık kapsam boşluğu; `F5-bagimlilik-ozeti.md`).
- **Dürüstlük:** birim testleri rota/adım mantığını ve gerçek haritada kirişlerin `Walk` olduğunu sınar; botun oyunda gerçekten engelsiz yürüdüğü K10-K12 (çalışma zamanı) ve F5-66 ile kanıtlanır. `eta_s` (rota/4,5) bir **tahmindir**, ölçülmüş yürüme süresi değildir. İnsan rotaları ızgara rotasından %8-24 uzundur (`docs/15` T-ENV-ARENA-04).
- Beklenmedik durumda (F5-59/F5-61 imzası farklı, `NavDrive` `BotSession`'ı dahil edince derleme/uyarı sorunu, ek dosya gerekiyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-62` — `<kısa-sha> [F5-62] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ … (K10-K15 Claude'un çalışma zamanı kriterleri)
- Gerçek-harita test satırları (`NAVDRIVE ...`): …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F5-62` @ `<sha>`
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
