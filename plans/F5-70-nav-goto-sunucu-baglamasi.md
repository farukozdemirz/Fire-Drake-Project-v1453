# F5-70: `/bot goto <bot> <x> <z> [hız]` sunucu bağlaması — paylaşılan `NavPathfinder`, `ActionExecutor::BeginGoto/TickPathMove`, `BotSession::m_navDrive`

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-70 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `KAPANDI`: F5-59 (`NavService`, `NAV=1`), F5-61 (kiriş guard'ı `CheckMoveChord`, `SubmitMove` içinde), F5-62 (`BotCore/NavDrive.h`, `Goto` kipi saf mantık; merge `8522239`), F5-69 (eğim 0,45). **F5-71'e bağımlı DEĞİLDİR** ve onunla dosya paylaşmaz (F5-71: `BotCore/NavTrack.h`, `NavTrackTests.cpp`, `NavBudgetTests.cpp`; bu plan: yalnızca `GameServer/Bot/` altındaki yedi dosya). Not: `BotSession.h` `NavDrive.h`'yi, o da `NavStuck.h` → `NavTrack.h`'yi dahil eder; F5-71 `NavTrack.h`'ye yalnızca bir şablon üye ekler, sıra fark etmez. F5-63 (`Follow`), F5-64, F5-65 bu planın `m_navDrive`/`SharedPathfinder` üzerine kurulur |
| İlgili gereksinim / kabul | `docs/12` §13.1 (paket adımı modeli), §13.4 (doğuş → arena dönüşü), §13.5 (`NavPathfinder` ≈ 4,2 MB); ADR-0006 madde 6 (sahiplik) ve Ek F5-62 (madde 1, 6: bu planın bölünme kaydı), ADR-0021 madde 1 (`Walk` olmayan hedef reddedilir); AC-NAV-02 (A* p95 ≤ 2 ms), AC-NAV-03 (engelli hücreye giren hareket 0; kanıt F5-66); `docs/13` §3 (IOCP thread kuralı); T-NAV-01 (doğuş → arena yolu, çalışma zamanı) |
| Tahmini büyüklük | M (7 dosya, hepsi `GameServer/Bot/`; yeni dosya yok; birim testi yok, kanıt derleme + kod incelemesi + Claude'un çalışma zamanı koşusu) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu, ön-plan; referanslar `gece/2026-10-02` @ `26f10c4` üzerinde doğrulandı) |

---

## 0. Bu plan neden bu kadar (bölme kaydı)

STATUS'taki F5-70 tanımı betik fiili `goto`'yu da içeriyordu. Gece kuralı (≤ ~6 dosya, tek yetenek) nedeniyle **betik fiili ayrı plana** alındı: **F5-72** (`BotCore/ScriptPlan.h` fiil listesi 20 → 21, `Tests/BotCoreTests/ScriptTests.cpp` `Script_VerbWhitelist`; 2 dosya; bu plandan sonra yazılır, numara yazım turunda `plans/README.md`'den yeniden doğrulanır). Bu planın bağlaması bu yüzden yalnızca `BotCommands.txt` / konsol / `+bot` komut yolundan sınanır (`ScriptRunner` komutu `ExecuteCommand`'a verir; fiil beyaz listesi F5-72'dedir). Karar kaydı bu dosyadadır; ADR-0006 **Ek F5-70** doğrulamada Claude tarafından yazılır (bu planı yazarken ADR dosyasına dokunulmadı).

## 1. Amaç

`[BOT] NAV=1` iken yeni `/bot goto <bot> <x> <z> [hız]` komutu botu hedefe **planlayıcı yolu** üzerinden yürütür: F5-62'nin `NavDrive` sürücüsü rotayı kurar, `ActionExecutor` her ~1,5 sn'de bir `NavDrive::NextStep` adımını gerçek `WIZ_MOVE` paketi olarak (F5-61 kiriş guard'lı `SubmitMove` üzerinden) gönderir; ara noktada durma paketi yoktur, varışta `speed = 0` durma paketi gider. Tek bir `NavPathfinder` örneği (`NavService`'te) tüm botlarca paylaşılır, yalnızca IOCP iş parçacığından kullanılır ve ihlali çalışma zamanında bir kez günlüğe yazılır. Mevcut `/bot move` (düz çizgi) **aynen kalır**. `NAV=0` / `ENABLED=0` iken hiçbir davranış değişmez (`goto` `nav_off` ile reddedilir).

## 2. Bağlam (okunması zorunlu)

Satır numaraları `gece/2026-10-02` @ `26f10c4` üzerinde okundu. Sapma görürsen **dur** (§5 adım 1).

- `BotCore/NavDrive.h` (F5-62, 334 satır; **değişmez**): `NavDrive::BeginGoto(grid, finder, botX, botZ, goalX, goalZ, params)` `:67` (önce `Reset()` yapar, başarısızlıkta yine `Reset()`; durum `NavPlanStatus{None, Planned, InvalidStart, InvalidGoal, NoPath, NodeLimit, ReplanLimit}` `:28`), `NextStep(grid, botX, botZ, maxStepM)` `:83` (`NavDriveStep{kind None/Step/Arrived/Blocked, x, z, routeProgressM, distToGoalM, truncated}` `:36`; `x/z` nicemlenmiş), `Replan(grid, finder, botX, botZ, params)` `:154` (en çok `kMaxGotoReplans = 1` kez `:17`; ikincisi `ReplanLimit`, çağrı başarısızsa sürücü `Reset` olur), erişimciler `Active()`, `GoalX()/GoalZ()` (nicemlenmiş hedef), `RouteLengthM()`, `Replans()`, `PlanExpanded()`, `PlanWaypoints()` `:176-182`. `NavDriveParams` varsayılanları: `search.maxNodes = 20000` (`NavPath.h:41`), `smooth.maxLookahead = 64` (`NavSmooth.h:68-73`) — `BotCore::NavDriveParams params;` yeterlidir. `NavDrive` kopyalanabilir (yalnızca `std::vector` + sayılar).
- `GameServer/Bot/NavService.h` (47 satır): `Instance()`, `Startup()`, `Grid()` (`const NavGrid *`, hazır değilse `nullptr`) `:38`, `private:` `:41`; ızgara yalnızca `Startup()`'ta kurulur, `NavPathfinder` **yoktur**. `NavService.cpp`: `WriteNavLog` `:24` (static; `./Logs/Bot_*.log`), `Startup` `:41`, `Shutdown` `:126`; `stdafx.h` ilk satırda (`GetCurrentThreadId` kullanılabilir; `BotManager.cpp:376` aynı çağrıyı yapar).
- `GameServer/Bot/ActionExecutor.cpp`: `SubmitMove` `:83-195` (CLI-05/CLI-08 adım denetimi `:104-118`, F5-61 kiriş denetimi `:122-141` — `NAV=1` + zone 71'de çalışır, başarısızlıkta `m_moveActive = false` ve `REFUSED "blocked_chord"`; paketi `HandlePacket`'e verir; `ok` ise `m_moveLastSent = now; m_movePackets++`, `arrived` ise `m_moveActive = false` `:171-181`), `BeginMove` `:198-258` (doğrulamalar `:205-239`: `not_in_game`, `dead`, `sitting`, `speed_field`; `bad_target` `:241-247` **`GetMap()->IsValidPosition` kullanır**; kurma `:249-256`), `TickMove` `:261-295` (süre kapısı `:279-285`: `elapsed < kMovePeriodMs` → `NOTHING`; `elapsedMs` en çok `2 × kMovePeriodMs`), `StopMove` `:297-315`, `AbandonMove` `:317-321`. Üst `static` yardımcılar: `ServerLimitFor` `:30`, `NextDecisionId` `:38`, `EmitFairnessReject` `:~52`. `#include "NavService.h"` `:4` zaten var.
- `GameServer/Bot/ActionExecutor.h`: `MoveOutcome{kind NOTHING/SENT/ARRIVED/REFUSED/FAILED, reason}` `:8-13` (yorumda `reason` değerleri listelenir), `BeginMove/TickMove/StopMove/AbandonMove` bildirimleri `:186-197`.
- `GameServer/Bot/BotSession.h`: hareket üyeleri `:73-79` (`m_moveActive`, `m_moveTargetX/Z`, `m_moveSpeed`, `m_moveLastSent`, `m_actionSeq`, `m_movePackets`); `#include`'lar `:3-10`. `BotSession.cpp` yapıcısı (`:54`) ve `ResetForRespawn` (`:490`) **değişmez** (§3 karar D3).
- `GameServer/Bot/BotManager.cpp`: `ExecuteCommand` dağıtımı `move` `:627`, `stop` `:629`; bilinmeyen komut metni `:668`; `CommandMove` `:1063-1125` (`SplitWords`, `ParseDoubleStrict` `:138`, `ParseIntStrict` `:151`, `PhaseName` `:80`, `FindSession`, `IsKnownBotName`; kalıbı aynen izlenir); `CommandStop` `:1127`; `TickSessions` ölü bot dalı `AbandonMove` `:3098-3105`; hareket dalı `ActionExecutor::TickMove` `:3137-3158` (`ARRIVED` / `REFUSED` / `FAILED` günlükleri hazırdır; **değişmez**); `BeginDespawn` `AbandonMove` `:3430`. `BotManager.h`: `CommandMove` bildirimi `:77`.
- `tools/check-perception-contract.py`: R2 `GetMap` yalnızca `ActionExecutor::BeginMove`'da (allowlist `:~57`); yeni kod `GetMap` **kullanmaz**, yalnızca botun kendi `CUser` konumunu (`user->GetX/GetZ/GetZoneID`) ve `NavService` ızgarasını okur. R3: `s->m_pUser` dışında `x->m_pUser` okuması yoktur.
- F5-62 doğrulama notları (plan dosyası "Doğrulama Raporu" Tur 1, bulgu 1-2): (1) rotadan yana `d` m itilmiş bot için Öklid adım uzunluğu `√(maxStepM² + d²)` olur ve `d` ≈ 3,4 m'yi aşınca (6,75 m adımda) CLI-08 sınırını (`maxStep × 1,10 + 0,15`) aşar; (2) `NodeLimit` geçerli bir "planlanmadı" sonucudur, `NoPath` değildir (haritada eğimle kopmuş ~3 000 `Walk` hücresi var, 300 rastgele çiftin 3'ü `NodeLimit`). İkisi de aşağıdaki kararlarda ele alınır.

### Tasarım kararları (otonom döngüde Claude kararı — gözden geçirilmeli; yeni ADR yok, ADR-0006 madde 6 + Ek F5-62'nin uygulaması; ADR-0006 Ek F5-70 doğrulamada yazılır)

**D1 — `NavPathfinder` paylaşımı: tek örnek, `NavService`'te, yalnızca IOCP iş parçacığında.**

| Seçenek | Bellek | Güvenlik | Sonuç |
|---|---|---|---|
| Bot başına örnek | 16 bot × 4,02 MiB = **64,3 MiB**; ilk `Find`'da her bot 4 MiB ayırır (Win32, 2 GB adres alanı) | Güvenli | Reddedildi |
| **Tek paylaşılan örnek** | **4,02 MiB** (`plans/F5-53` K8 ölçüm notu) | Tüm çağıranlar IOCP iş parçacığındadır (`BotManager::Tick`, `BotManager.h`: "IOCP worker thread only" → `ProcessCommands` → komutlar; `TickSessions` → hareket); `NavPathfinder` deterministik, önceki sorgudan etkilenmez (`NavPath.h:66-69`) | **Seçildi** |
| Sorgu başına geçici örnek | 4 MiB ayır/boşalt | Güvenli | Reddedildi (A* p95 ~0,8 ms iken ayırma baskın) |

`NavService::SharedPathfinder()` ilk çağıranın iş parçacığı kimliğini saklar; sonraki çağrı başka iş parçacığından gelirse `Bot_*.log`'a **bir kez** `NavService: pathfinder used from thread <n> (first <m>) VIOLATION` yazılır (çağrı yine de sürer; çalışma zamanı kanıtı: sıfır satır). Tek-thread varsayımı `docs/13` §3'ün sonucudur; ileride arka plan iş parçacığı (ADR-0005 v2) eklenirse bu karar yeniden açılır. Ücret: `NavFollower` (F5-63) aynı örneği parametre olarak alır.

**D2 — Kip seçimi `TickMove` içinde.** `ActionExecutor::TickMove` en başta (`!m_moveActive` denetiminden hemen sonra) `if (s->m_navDrive.Active()) return TickPathMove(s, now);` satırını alır; `BotManager::TickSessions` ve onun `ARRIVED/REFUSED/FAILED` günlükleri **değişmez** (yol yürüyüşü aynı günlük satırlarını üretir). `m_moveActive = true` tutulur: cast-iptali, duruş, `busy`, `list` mantığı (`ActionExecutor.cpp:911, 973, 1671`; `BotManager.cpp:861, 3113`) değişmeden çalışır.

**D3 — Sürücü ömrü.** Sürücüyü sıfırlayan noktalar: `BeginMove` (yeni düz yürüyüş yolu iptal eder), `StopMove`, `AbandonMove` (ölüm `:3098`, despawn `:3430`), `TickPathMove` içinde `ARRIVED/REFUSED/FAILED`. `BotSession.cpp::ResetForRespawn` ve yapıcıya **dokunulmaz**: despawn her zaman önce `AbandonMove` çağırır (`BotManager.cpp:3430`) ve sürücü varsayılan `Off`'tur; tam temizlik (bölge değişimi, ışınlanma, konum sıçraması) F5-65'in işidir.

**D4 — Başarısız `goto` mevcut yürüyüşü bozmaz.** `BeginGoto` rotayı **yerel bir `NavDrive`'a** planlar, yalnızca `Planned` ise `s->m_navDrive = plan;` ile kalıcılaştırır ve yürüyüş durumunu kurar. `NavDrive::BeginGoto` kendi nesnesini `Reset()` ettiğinden, doğrudan `s->m_navDrive` üzerinde planlamak sürmekte olan bir yürüyüşü (ve `m_moveActive` ile sürücü arasındaki uyumu) bozardı. Reddedilen `goto` hiçbir üyeyi değiştirmez (`BeginMove`'un reddi gibi). Geçerli `goto` sürmekte olan düz ya da yol yürüyüşünün yerini alır.

**D5 — Rotadan sapma ve engel: tek `Replan`.** `TickPathMove`, `NextStep` sonucu `Blocked` ise **veya** `Step/Arrived` adımının botun konumundan Öklid uzunluğu `maxStep × 1,05 + 0,1 m`'yi aşıyorsa (botun rotadan yana itilmesi; bu eşik guard sınırının `maxStep × 1,10 + 0,15`'inin her zaman altındadır, yani guard reddetmeden önce yakalanır) `NavDrive::Replan` çağırır (botun **şu anki** konumundan, aynı nicemlenmiş hedefe); yeniden planlanan rotada `NextStep` bir daha `Blocked`/aşırı uzunsa yürüyüş `path_blocked` ile biter. `Replan` zaten en çok 1 kez izin verdiğinden sonsuz döngü yoktur; ikinci istek `ReplanLimit` → `replan_limit`. Takılma tespiti ve kurtarma merdiveni F5-63'tür.

**D6 — Hedef/başlangıç reddi ve neden metinleri.** `Walk` olmayan hedef **reddedilir** (`invalid_goal`; en yakın yürünebilir hücreye alınmaz — ADR-0021 madde 1). `Walk` olmayan başlangıç: `invalid_start` (F5-62 `InvalidStart`; yakın hücreye taşıma yok). `NoPath` → `no_path` (hedef bu yerden ulaşılamaz); **`NodeLimit` → `node_limit` ve komut günlüğünde "not planned" olarak yazılır** (`no_path` DEĞİL: bütçe doldu, ulaşılabilirlik bilinmiyor). `ReplanLimit` → `replan_limit`; `None` (olmaması gereken) → `nav_none`. Bunlara ek yürütücü nedenleri: `nav_off` (`NavService::Instance().Grid() == nullptr`), `nav_zone` (`user->GetZoneID() != ZONE_RONARK_LAND`), ve mevcut `not_in_game`, `dead`, `sitting`, `speed_field`. `bad_target` **kullanılmaz** (`GetMap` allowlist'ini bozmamak için ızgara `Walk` denetimi yeter).

**D7 — Kapsam sınırı.** KI-022 (`/bot move` son adım < 0,08 m) bu planın konusu değildir: `goto`'da son paket (`Arrived`) `NextStep` içinde `CheckMoveChord`'tan geçer ve hedef hücresi `Walk`'tur (F5-62), yani `goto` KI-022'den etkilenmez; `/bot move` davranışı **değişmez**. Arena sınırı (yasaklı disk) bu planda yoktur (`field = nullptr`, düz mesafe).

## 3. Kapsam

**Yapılacaklar**

1. `NavService`: `BotCore::NavPathfinder & SharedPathfinder()` + ilk-çağıran iş parçacığı denetimi (D1).
2. `BotSession.h`: `BotCore::NavDrive m_navDrive;` üyesi (+ `#include "../../BotCore/NavDrive.h"`).
3. `ActionExecutor`: `BeginGoto`, `TickPathMove` (yeni), ortak doğrulamanın `BeginMove`'dan yardımcıya taşınması (davranış aynı), `TickMove` başına kip satırı, `BeginMove/StopMove/AbandonMove`'a sürücü sıfırlama (D2-D6).
4. `BotManager`: `goto` komutu (`CommandGoto`), dağıtım ve bilinmeyen komut metni.
5. Çalışma zamanı doğrulaması (Claude; §6 K11-K16).

**Kapsam dışı (yapılmayacak)**

- Betik fiili `goto` (`ScriptPlan.h`, `ScriptTests.cpp`): **F5-72**.
- `/bot follow`, hız kestirimi, takılma tespiti/kurtarma, `NavFollower` kullanımı (F5-63; F5-71'in `UpdateReachable`'ı da orada kullanılır).
- Sorgu bütçesi/faz kaydırma/önbellek, `NAV_PATH`/`NAV_STUCK`/`NAV_RECOVERY` telemetri olayları, `PERF_SAMPLE` nav payı (F5-64): `goto` komutunda yol **anında** hesaplanır (komut başına bir sorgu; çalışma zamanı testleri komutları ≥ 1 sn arayla verir).
- Ölüm/respawn/bölge değişimi/ışınlanma temizliği (F5-65); yalnızca D3'teki noktalar.
- Arena maliyet alanı, tehlike katmanı, `NavReach` yargısı, `directClear` / `NavRetreatPlanner` sunucuya açılması (F5-68).
- `BotCore/` altındaki **hiçbir** dosya, `Tests/`, `tools/`, `docs/`, `.vcxproj`/`.filters` (yeni `.cpp` yok), `GameServer/Bot/BotSession.cpp`, `BotManager.cpp` içindeki `CommandMove/CommandStop/TickSessions`, `SubmitMove` gövdesi, `StepToward` (hepsi değişmez).
- `docs/` güncellemeleri (Claude doğrulamada: `docs/13` komut tablosu, `docs/12` §13.4, ADR-0006 Ek F5-70, KI-022 notu, `docs/STATUS.md`).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/NavService.h` | değiştir | `#include "../../BotCore/NavPath.h"`, `SharedPathfinder()` bildirimi, üyeler `m_pathfinder`, `m_pfThread`, `m_pfViolation`; `Grid()/Ready()/GetInfo()` **değişmez** |
| `GameServer/Bot/NavService.cpp` | değiştir | yalnızca `SharedPathfinder()` gövdesi (`WriteNavLog` yeniden kullanılır) |
| `GameServer/Bot/BotSession.h` | değiştir | yalnızca `m_navDrive` üyesi + `#include` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `BeginGoto`, `TickPathMove` bildirimi; `MoveOutcome.reason` yorumuna yeni nedenler |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | §5 adım 4 |
| `GameServer/Bot/BotManager.h` | değiştir | `void CommandGoto(const std::string & args);` (`CommandMove` yanına) |
| `GameServer/Bot/BotManager.cpp` | değiştir | `goto` dağıtımı, `CommandGoto`, bilinmeyen komut metnine `goto` |

Hepsi şimdi ASCII + CRLF'dir (BOM yok; `file` ve `head -c3 \| xxd -p` = `237072`); biçim korunur. Yeni `.cpp` yok: `proj-GameServer.vcxproj` değişmez. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-70 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop`. `python3 tools/nav-export.py`. §2'deki dosya/satırları aç ve doğrula (özellikle `NavDrive.h` imzaları, `BeginMove` gövdesi, `TickMove` süre kapısı, `BotManager.cpp` `:627-629`/`:668`/`:3137`); sapma varsa **dur** ve `Durum: UYGULANIYOR (BLOKE)` yaz. Başlangıç ölçümü (rapora): `./tools/run-tests.sh Release 2>&1 | grep -E "tests,"` ve `./build/bin/x86-Release/Tests/BotCoreTests.exe --list | wc -l` (F5-71 henüz birleşmemiş olabilir; sayı yalnızca karşılaştırma içindir, bu plan test eklemez).
2. **`NavService.h` / `NavService.cpp`** (D1):

   ```cpp
   // NavService.h  (after the includes: #include "../../BotCore/NavPath.h")
   	// Shared A* instance (F5-70, docs/13 s3). ONE instance for every bot: NavPathfinder is NOT thread-safe,
   	// so callers must run on the IOCP thread (BotManager::Tick). The first caller's thread id is remembered;
   	// a later call from another thread writes ONE "VIOLATION" line to ./Logs/Bot_*.log (the call proceeds).
   	// Only call while Ready() is true. The ~4 MiB search pool is allocated by the first Find().
   	BotCore::NavPathfinder & SharedPathfinder();
   // private:
   	BotCore::NavPathfinder m_pathfinder;
   	std::atomic<uint32_t> m_pfThread{ 0 };     // thread id of the first SharedPathfinder() caller, 0 = none yet
   	std::atomic<bool> m_pfViolation{ false };  // the VIOLATION line was written
   ```

   ```cpp
   // NavService.cpp
   BotCore::NavPathfinder & NavService::SharedPathfinder()
   {
   	const uint32_t tid = (uint32_t)GetCurrentThreadId();
   	uint32_t first = 0;
   	if (!m_pfThread.compare_exchange_strong(first, tid) && first != tid && !m_pfViolation.exchange(true))
   	{
   		char message[160];
   		snprintf(message, sizeof(message),
   			"NavService: pathfinder used from thread %u (first %u) VIOLATION", (unsigned)tid, (unsigned)first);
   		printf("%s\n", message);
   		WriteNavLog(message);
   	}
   	return m_pathfinder;
   }
   ```

   Not: `NavService()` yapıcısının başlatıcı listesi (`m_enabled(false), m_ready(false)`) kalır; yeni atomikler sınıf-içi başlatıcıyla başlar. `Startup()` ve `Shutdown()` **değişmez** (`Shutdown` sonrası örnek süreç sonuna kadar yaşar).
3. **`BotSession.h`**: `#include "../../BotCore/NavDrive.h"` (diğer BotCore `#include`'larının yanına) ve hareket üyelerinin hemen altına `BotCore::NavDrive m_navDrive;               // IOCP thread only: path-following state of /bot goto (F5-70); Active() only while m_moveActive`.
4. **`ActionExecutor.h` / `.cpp`**:
   - Bildirimler (public, `AbandonMove`'un yanında):

     ```cpp
     	// Validates and arms a walk to (gx,gz) ALONG THE NAVIGATION PATH at 'speedField' (NAV=1, zone 71); sends
     	// nothing yet (the same Tick()'s TickMove() does). REFUSED reasons: "not_in_game", "dead", "sitting",
     	// "speed_field", "nav_off", "nav_zone", "invalid_start", "invalid_goal", "no_path", "node_limit",
     	// "replan_limit", "nav_none". A refused goto changes NOTHING (a walk in progress keeps going).
     	static MoveOutcome BeginGoto(BotSession * s, float gx, float gz, int16 speedField,
     		std::chrono::steady_clock::time_point now);
     	// Path-following tick; TickMove() calls it while s->m_navDrive.Active().
     	static MoveOutcome TickPathMove(BotSession * s, std::chrono::steady_clock::time_point now);
     ```

     `MoveOutcome::reason` yorumuna (`ActionExecutor.h:11-12`) yalnızca gerçekten döndürülen yeni nedenleri ekle: `"nav_off"`, `"nav_zone"`, `"invalid_start"`, `"invalid_goal"`, `"no_path"`, `"node_limit"`, `"replan_limit"`, `"nav_none"`, `"path_blocked"`.
   - **Ortak doğrulama (davranış değişmez):** `BeginMove`'un `:205-239` bloğunu (`user == nullptr || !isInGame` → `not_in_game`; `isDead` → `dead`; `USER_SITDOWN` → `sitting`; hız denetimi + `EmitFairnessReject(... "CLI-05", "speed_field" ...)` → `speed_field`) birebir bir dosya-içi yardımcıya taşı: `static bool ValidateWalkStart(BotSession * s, int16 speedField, MoveOutcome & out)` (`false` dönerse `out` doldurulmuştur). `BeginMove` bunu çağırır, ardından **kendi** `bad_target` bloğuyla (`:241-247`, `GetMap` orada kalır) ve kurmayla devam eder. Taşıma salt mekaniktir: neden metinleri, `EmitFairnessReject` argümanları ve sıra aynı kalır.
   - `BeginMove`: doğrulamalar geçtikten sonra, `s->m_moveActive = true;` satırından **önce** `s->m_navDrive.Reset();` (D3). `StopMove`: `!m_moveActive` erken dönüşünden hemen sonra `s->m_navDrive.Reset();`. `AbandonMove`: `if (s != nullptr) { s->m_moveActive = false; s->m_navDrive.Reset(); }`.
   - `TickMove`: `if (s == nullptr || !s->m_moveActive) return out;` satırından hemen sonra `if (s->m_navDrive.Active()) return TickPathMove(s, now);` (D2). `TickMove`'un geri kalanı, `StepToward` çağrısı ve `SubmitMove` **değişmez**.
   - `BeginGoto` (D1, D4, D6):

     ```cpp
     MoveOutcome ActionExecutor::BeginGoto(BotSession * s, float gx, float gz, int16 speedField,
     	std::chrono::steady_clock::time_point now)
     {
     	MoveOutcome out; out.kind = MoveOutcome::NOTHING; out.reason = "ok";
     	if (!ValidateWalkStart(s, speedField, out)) return out;
     	CUser * user = s->m_pUser;
     	const BotCore::NavGrid * grid = NavService::Instance().Grid();
     	if (grid == nullptr) { /* REFUSED "nav_off" */ }
     	if (user->GetZoneID() != ZONE_RONARK_LAND) { /* REFUSED "nav_zone" */ }
     	BotCore::NavDrive plan;                                   // plan into a LOCAL drive (D4)
     	const BotCore::NavDriveParams params;
     	const BotCore::NavPlanStatus status = plan.BeginGoto(*grid, NavService::Instance().SharedPathfinder(),
     		user->GetX(), user->GetZ(), gx, gz, params);
     	if (status != BotCore::NavPlanStatus::Planned) { /* REFUSED PlanReason(status); nothing else touched */ }
     	s->m_navDrive = plan;
     	s->m_moveActive = true;
     	s->m_moveTargetX = plan.GoalX();  s->m_moveTargetZ = plan.GoalZ();   // quantised goal
     	s->m_moveSpeed = speedField;
     	s->m_movePackets = 0;
     	s->m_moveLastSent = now - std::chrono::milliseconds(BotCore::kMovePeriodMs);   // first packet in this tick
     	out.kind = MoveOutcome::SENT; out.reason = "ok";
     	return out;
     }
     ```

     `PlanReason(NavPlanStatus)` dosya-içi `static const char *`: `InvalidStart`→`"invalid_start"`, `InvalidGoal`→`"invalid_goal"`, `NoPath`→`"no_path"`, `NodeLimit`→`"node_limit"`, `ReplanLimit`→`"replan_limit"`, diğer→`"nav_none"`. `gx/gz` aralık denetimini `NavDrive` yapar (`ValidCoord`), yürütücü ayrıca denetlemez.
   - `TickPathMove` (D2, D5): `TickMove` ile **aynı** süre kapısı ve kırpma (`elapsed < kMovePeriodMs` → `NOTHING`; `elapsedMs` en çok `2 × kMovePeriodMs`); `maxStep = BotCore::MaxStepMeters(s->m_moveSpeed, elapsedMs)`.

     ```cpp
     // pseudo-code; the contract is fixed, the body is yours
     if (s == nullptr || !s->m_moveActive || !s->m_navDrive.Active()) return NOTHING;
     CUser * user = s->m_pUser;
     if (user == nullptr || !user->isInGame()) { s->m_moveActive = false; s->m_navDrive.Reset(); return NOTHING; }
     const BotCore::NavGrid * grid = NavService::Instance().Grid();
     if (grid == nullptr) { fail("nav_off"); }                         // fail(): m_moveActive=false, Reset(), REFUSED
     ... time gate, maxStep ...
     BotCore::NavDriveStep step = s->m_navDrive.NextStep(*grid, user->GetX(), user->GetZ(), maxStep);
     if (NeedsReplan(step, user->GetX(), user->GetZ(), maxStep))      // Blocked, or Euclid(bot, step) > maxStep*1.05 + 0.1
     {
     	const BotCore::NavPlanStatus st = s->m_navDrive.Replan(*grid, NavService::Instance().SharedPathfinder(),
     		user->GetX(), user->GetZ(), BotCore::NavDriveParams());
     	if (st != BotCore::NavPlanStatus::Planned) { fail(PlanReason(st)); }   // Replan already Reset the drive
     	step = s->m_navDrive.NextStep(*grid, user->GetX(), user->GetZ(), maxStep);
     	if (NeedsReplan(step, ...) || step.kind == BotCore::NavDriveStep::None) { fail("path_blocked"); }
     }
     const bool arrived = step.kind == BotCore::NavDriveStep::Arrived;
     MoveOutcome r = SubmitMove(s, user, step.x, step.z, arrived ? 0 : s->m_moveSpeed, arrived ? 0 : 3,
     	arrived, elapsedMs, now);
     if (r.kind != MoveOutcome::SENT) s->m_navDrive.Reset();           // ARRIVED / REFUSED / FAILED end the walk
     return r;
     ```

     Kurallar: (a) **varış dışında hiçbir `speed = 0` paketi üretilmez** (ara noktada durma yok); (b) `Blocked` ve `fail(...)` yollarında `HandlePacket` çağrısı **yoktur**; (c) `step.x/z` zaten nicemlenmiştir, `SubmitMove` aynı nicemlemeyi idempotent uygular; (d) `NeedsReplan` dosya-içi `static bool` (`kind == Blocked`, ya da `kind` `Step/Arrived` iken Öklid adım uzunluğu `maxStep * 1.05f + 0.1f`'den büyük); (e) yeni kodda `GetMap`, `GetUserPtr` veya başka bir botun `m_pUser`'ı **yok**.
5. **`BotManager.h` / `BotManager.cpp`**: `CommandGoto(const std::string & args)` — `CommandMove` (`:1063-1125`) kalıbı: aynı ayrıştırma (`goto <bot> <x> <z> [speed]`, kullanım satırı `"BotManager: cmd goto: usage: goto <bot> <x> <z> [speed]"`), aynı `unknown or not spawned bot` / `not in game (phase ...)` satırları (`cmd goto:` öneki). Planlama süresini `std::chrono::steady_clock` ile `ActionExecutor::BeginGoto` çağrısının etrafında ölç. Sonuç satırları (sabit biçim; Claude çalışma zamanında bunları arar):
   - başarı: `BotManager: cmd goto: <bot> planned <w> waypoints, route <m> m, expanded <n>, <ms> ms, walking to (<x>, <z>) at speed <s>` (`<w>` = `s->m_navDrive.PlanWaypoints()`, `<m>` = `RouteLengthM()` `%.1f`, `<n>` = `PlanExpanded()`, `<ms>` `%.2f`, `<x>/<z>` = `GoalX()/GoalZ()` `%.1f`);
   - ret: `BotManager: cmd goto: <bot> refused (<reason>)`; `reason == "node_limit"` ise sonuna ` [not planned: search budget exhausted, reachability unknown]` eklenir (D6).
   `ExecuteCommand` dağıtımına `else if (_stricmp(verb.c_str(), "goto") == 0) CommandGoto(args);` (`move`'dan sonra), bilinmeyen komut metnine `goto` ekle (`move` ile `stop` arasına). `CommandMove`, `CommandStop`, `TickSessions` ve diğer komutlar **değişmez**.
6. Derle/test (§7). `python3 tools/check-perception-contract.py` rc=0. Sunucu çalıştırma uygulayıcıya düşmez (K11-K16 Claude'un). Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `./tools/build.sh Debug` rc=0; `BotSession.h` ve `NavService.h` `touch` edilip yeniden derlenince **yeni uyarı yok** (`BotSession.h` artık `NavDrive.h` → `NavStuck.h` → `NavTrack.h` zincirini ilk kez GameServer birimlerine taşır; `ActionExecutor.cpp`, `BotManager.cpp`, `ScenarioRunner.cpp`, `ScriptRunner.cpp`, `Telemetry.cpp`, `BotSession.cpp`, `NavService.cpp` yeniden derlenir; Windows makro çakışması (ör. `min`/`max`/`near`/`far`) ve MSVC uyarısı için günlüğü `grep -c "warning"` ile denetle, sayıyı rapora yaz)
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; test sayısı başlangıç ölçümüyle **aynı** (bu plan test eklemez/silmez; `--list | wc -l` eşit)
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F5-70` yalnızca §4'teki yedi dosya + plan dosyası + `plans/README.md` (yalnızca kendi satırının durum sözcüğü); `BotCore/`, `Tests/`, `tools/`, `docs/`, `shared/`, `AIServer/`, `*.vcxproj*`, `BotSession.cpp` farkı **boş**; `git diff --check` boş
- [ ] K4: `ActionExecutor.cpp` farkı yalnızca şunlardır (`git diff -U0 gece/2026-10-02...bot/F5-70 -- GameServer/Bot/ActionExecutor.cpp`): `ValidateWalkStart` (taşınan blok, metinler birebir), `BeginMove`'da yardımcı çağrısı + bir `m_navDrive.Reset();`, `TickMove`'da tek `if (s->m_navDrive.Active()) return TickPathMove(s, now);`, `StopMove`/`AbandonMove`'da birer `Reset`, yeni `BeginGoto`/`TickPathMove`/`PlanReason`/`NeedsReplan`. `SubmitMove` gövdesi, `StepToward` çağrısı ve `TickMove`'un süre/adım mantığı farkı **0**
- [ ] K5: `TickPathMove` içinde `speed` yalnızca `arrived` iken 0 (`dosya:satır` raporda); `Blocked`/`fail` yollarında `HandlePacket` çağrısı yok; yeni fonksiyonlarda `GetMap|GetUserPtr|GetRegion|m_arNpcArray` geçmez (`grep -n` boş, yalnızca `BeginMove`'daki mevcut `GetMap` kalır)
- [ ] K6: `python3 tools/check-perception-contract.py` rc=0 (R1-R5 PASS; `BotSession.h` yeni üyesi R5'i bozmaz, R2 `GetMap` allowlist'i `BeginMove`'da kalır)
- [ ] K7: `NavService.h` `Grid()/Ready()/GetInfo()/Startup()/Shutdown()` imzaları değişmedi; `NavService.cpp` farkı yalnızca `SharedPathfinder()` (`git diff -U0` raporda); `NAV=0` iken `SharedPathfinder()` hiç çağrılmaz (`BeginGoto` `Grid() == nullptr` ile ondan önce döner: kod incelemesi, `dosya:satır`)
- [ ] K8: `BotManager.cpp` farkı yalnızca `goto` dağıtımı, bilinmeyen komut metni ve `CommandGoto`; `CommandMove`, `CommandStop`, `TickSessions` farkı **boş**
- [ ] K9: dosyalar ASCII + CRLF (`file` çıktısı), BOM yok; `git diff --check` boş
- [ ] K10: reddedilen `goto` durumu bozmaz — kod incelemesiyle: `BeginGoto` yerel `plan` nesnesine planlar, yalnızca `Planned` ise `s->m_navDrive = plan` ve yürüyüş üyelerini yazar (`dosya:satır` raporda); `ValidateWalkStart`/`nav_off`/`nav_zone` ret yolları hiçbir `BotSession` üyesini (EmitFairnessReject dışında) yazmaz
- [ ] K11 (Claude, çalışma zamanı): `GameServer.ini` `[BOT] ENABLED=1, NAV=1, TELEMETRY=decisions`; bir Karus botu doğuş noktasından, `Walk` bir arena A hücre merkezine `/bot goto <bot> <x> <z>`: `Bot_*.log`'da `cmd goto: ... planned <w> waypoints, route <m> m, expanded <n>, <ms> ms, walking to (...)`; bot yürür (`ACTION_SUBMIT` `Move`, `echo:3`; ara noktada `speed:0` paketi **yok**), `arrived at ... after N packets`; `N ≈ ceil(route / 6,75)` (±2: hız/zamanlama kırpması); `FAIRNESS_REJECT` (`blocked_chord`/`step_too_long`) **0**; `NavService: pathfinder used from thread ... VIOLATION` satırı **yok**; ilk `goto`'nun planlama süresi (havuz ayırma dahil) ve sonrakiler rapora (bilgi, `[V: Y1]` etiketi)
- [ ] K12 (Claude): aynısı El Morad botu doğuşundan arena A'ya; iki ulusun rota uzunluğu/yürüme süresi rapora (F5-62 ADR Ek madde 5 ölçümü: ~276 m / ~701 m, 4,5 m/s'de ~61 / ~156 sn; **yalnızca bilgi**, `docs/12` §13.4'teki ~52/~142 sn eğim 0,625 dönemine aittir; kabul ölçütü F5-66'dadır)
- [ ] K13 (Claude): `/bot move` korunur: `NAV=1`, aynı bot, engele değmeyen kısa hedef → eskisi gibi düz yürür ve varır; `goto` ortasında `/bot move` verilirse `goto` iptal olur ve düz yürüyüş başlar (sürücü `Reset`); `goto` ortasında `/bot stop <bot>` yürüyüşü durdurur (durma paketi reddedilmez) ve aynı bot sonra yeniden `goto` alabilir
- [ ] K14 (Claude): ret yolları: `Walk` olmayan hedef → `refused (invalid_goal)`; ızgara dışı hedef → `invalid_goal`; ana bileşen dışındaki cep hedefi → `no_path` veya `node_limit` (ikincisinde günlükte "not planned" metni); `goto` ortasında geçersiz bir `goto` → ret **ve eski yürüyüş sürer** (D4); çökme yok; ret satırlarında bot yürümez
- [ ] K15 (Claude): `NAV=0`: `/bot goto` `refused (nav_off)`, `/bot move` aynı çalışır; `ENABLED=0` iken hiçbir bot satırı yok (davranış değişmedi)
- [ ] K16 (Claude): sunucu `./tools/run-servers.sh stop` ile kapanır (`0/3 hazır`); `GameServer.ini` ve `BotCommands.*` başlangıçtaki hâline döner; "AC-NAV-03 kapandı" **yazılmaz** (kanıt F5-66); `Replan`/`off_route` yolunun çalışma zamanı kanıtı yoktur (sunucuda botu rotadan iten bir komut yok): birim düzeyi kanıt F5-62 `NavDrive_Blocked_Replan`/`NavDrive_OffRoute`, yürütücü tarafı K4/K5 kod incelemesi

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "tests,|failed"
./build/bin/x86-Release/Tests/BotCoreTests.exe --list | wc -l
./tools/run-tests.sh Debug 2>&1 | grep -E "tests,|failed"
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-70
git diff -U0 gece/2026-10-02...bot/F5-70 -- GameServer/Bot/ActionExecutor.cpp GameServer/Bot/NavService.cpp
git diff --check gece/2026-10-02...bot/F5-70
file GameServer/Bot/NavService.h GameServer/Bot/NavService.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotManager.h GameServer/Bot/BotManager.cpp
# Claude (çalışma zamanı): ./tools/run-servers.sh start ; echo "goto <bot> <x> <z>" >> /mnt/c/dev/fdp/server/BotCommands.txt ; grep -a -E "cmd goto|arrived|VIOLATION|move stopped" /mnt/c/dev/fdp/server/Logs/Bot_*.log ; ./tools/run-servers.sh stop
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3: CRLF, tab, Allman, yorumlar İngilizce, ASCII; konsola yalnızca `VIOLATION` satırı (nadir) ve mevcut `NavService` kalıbı yazılır, `goto` günlükleri yalnızca `Bot_*.log`'a (`WriteBotLog`); `NAV=0` iken sunucu davranışı değişmez.
- **Bot avantajı yasağı (`docs/03` §13, `docs/13` §3):** yol, botun kendi konumundan ve herkesin sahip olduğu zone verisinden (F5-59 ızgarası) hesaplanır; hedef komutla verilir (bot hedefin yerini **bulmaz**). Hız ve sıklık aynıdır: `m_moveSpeed` ≤ sunucu sınırı (CLI-05), adım ≤ `MaxStepMeters` (CLI-08) `SubmitMove` guard'ında korunur, paketler `kMovePeriodMs` aralığıyla gider; yol izleme botu hızlandırmaz.
- Thread: tüm çağrılar IOCP iş parçacığında (`BotManager::Tick`); `OnPacket()` (herhangi bir thread) `m_navDrive`'a **dokunmaz**. `VIOLATION` satırı çalışma zamanında görülürse plan `BLOKE`.
- Bu plan `field = nullptr` ile **düz mesafe** planlar: "arena sınırı içinde kal" (`docs/12` §13.4) sağlanmaz; arenaya varış hedef noktasında durmakla sağlanır (kapsam boşluğu; `docs/reports/plan-bagimlilik-F5-2026-10-03.md`).
- **Dürüstlük:** F5-62 testleri rota/adım mantığını sınadı; botun oyunda engelsiz yürüdüğü K11-K14 ve F5-66 ile kanıtlanır. Rapor etmediğin ya da koşmadığın şeyi "geçti" yazma. `eta` tahmindir; yürüme süresi ölçülmüş değerdir.
- Beklenmedik durumda (F5-62/F5-61 imzası farklı, `BotSession.h` `NavDrive.h`'yi dahil edince derleme/uyarı/makro sorunu, ek dosya gerekiyor, `ValidateWalkStart` taşıması davranışı değiştiriyor görünüyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-70` — `<kısa-sha> [F5-70] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Başlangıç ve bitiş ölçümü (`tests,` satırı ve `--list | wc -l`): …
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ … (K11-K16 Claude'un çalışma zamanı kriterleri)
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F5-70` @ `<sha>`
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
