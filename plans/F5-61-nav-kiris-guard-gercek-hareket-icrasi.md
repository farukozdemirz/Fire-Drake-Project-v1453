# F5-61: Gerçek hareket icrasında kiriş denetimi: CLI-08 `blocked_chord` (`BotCore/NavChordGuard.h`, `ActionExecutor::SubmitMove`)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02) |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-61 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `KAPANDI` (hepsi `gece/2026-10-02`'ye birleşti): **F5-59** (`NavService`: `Instance()`, `Ready()`, `Grid()`), F5-50 (`NavCheckStep`), F5-58 (kalıcı duvar regresyonu), F4-01 (`SubmitMove`, `CheckMoveStep`), F4-55 (giriş el sıkışması; çalışma zamanı K9-K11 bot girişi buna dayanır). Şemsiye: F5-55 (dilim 3). F5-62 bu planın guard'ına dayanır |
| İlgili gereksinim / kabul | CLI-08 (`docs/03` §13), `docs/12` §13.1 ("Kural (CLI-08)", "Duvar bulgusunun sınıflandırması"), AC-NAV-03 (engelli hücreye giren hareket = 0), DEG-20; `docs/13` §3 (bot avantajı yasağı); ADR-0017 (adalet koruması); `tools/check-perception-contract.py` R1-R5 PASS kalmalı |
| Tahmini büyüklük | S–M (6 dosya; 2'si yeni; kod az, kanıt birim testte + çalışma zamanında) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak) → HAZIR 2026-10-03 (gece modu; referanslar `gece/2026-10-02` @ `aeca42b` üzerinde yeniden doğrulandı) |

---

## Yazım turu doğrulaması (HAZIR yapılırken, `gece/2026-10-02` @ `aeca42b`)

1. **F5-59 sözleşmesi gerçek kodda doğrulandı:** `GameServer/Bot/NavService.h:25` `static NavService & Instance()`, `:37` `bool Ready() const`, `:38` `const BotCore::NavGrid * Grid() const` (hazır değilse `nullptr`), `:31` `Startup()`, `:34` `Shutdown()`; `[BOT] NAV=1` (varsayılan 0). Sapma yok.
2. **Satırlar doğrulandı (taslakla aynı):** `GameServer/Bot/ActionExecutor.cpp:19-24` (`MoveStepLimit`), `:53-72` (`EmitFairnessReject`, `skillId` varsayılan parametresi `:55`), `:76-163` (`SubmitMove`; nicemleme `:84-92`, guard bloğu `:94-110`, `ACTION_SUBMIT` `:112-122`, `HandlePacket` `:128`), `:167-228` (`BeginMove`), `:230-264` (`TickMove`), `:266-284` (`StopMove`: konumunu `user->GetX()/GetZ()` ile `speed=0` paketi olarak yollar). `SubmitMove`'u çağıran **tek** yer `TickMove` ve `StopMove`'dur; `GameServer/Bot/` altında başka `WIZ_MOVE` üreten kod yoktur (`grep -rn WIZ_MOVE GameServer/Bot` yalnızca `:124` üretir). `BotCore/BotMotion.h:15` `kStopSlackMeters = 0.05f`; `BotCore/NavSegment.h:22-43` (`NavSegmentVerdict`, `NavCheckSegment`), `:330` `NavCheckStep`; `BotCore/NavGrid.h:40-52` (`Size()`, `Unit()`, `InBounds`, `CellOf`, `Walk`); `GameServer/Define.h:140` `ZONE_RONARK_LAND 71`; `GameServer/Unit.h:55` `GetZoneID()`; `tools/nav-regress/good.txt:4-6` üç `EXAMPLE straight step` vektörü; `Tests/BotCoreTests/NavArenaTests.cpp:320-329` `SKIPPED` kalıbı; `Tests/BotCoreTests/NavSegmentAuditTests.cpp:32-46` (`LoadZone71OrSkip`), `:98-128` (`BuildAuditPool`), `:200-237` (6,75 m paket kirişi döngüsü).
3. **`tools/check-perception-contract.py`:** R2 kısıtlı semboller (`GetUserPtr`, `GetMap`, ...) listesinde `GetZoneID`/`GetX`/`GetZ`/`NavService` **yok**; yeni kod bunlardan yalnızca botun **kendi** konumunu/bölgesini ve `NavService` ızgarasını okur. R4: `NavChordGuard.h` yalnızca standart kütüphane ve kardeş başlıklar içerir.
4. **Kararlar (otonom döngüde Claude kararı — gözden geçirilmeli; ADR-0006 Ek F5-61):** D5 (başlangıç hücresi `Walk` değilse çıkış muafiyeti) taslaktaki gibi **kabul edildi**; D3 durma/aynı-konum muafiyet eşiği `kStopSlackMeters` (0,05 m) yerine, paket nicemlemesinin köşegen hatasına (en çok 0,0708 m) uyan **`kChordIgnoreMeters = 0.08f`** yapıldı (gerekçe §2 D3). Önkoşul olan F5-59 taslağın "Açık soru" maddesi bu iki karar ile kapandı.

> **Karar (2026-10-03, ADR-0021, proje sahibi):** D5: başlangıç hücresi `Walk` değilse kiriş denetimi başlangıç hücresinden **çıkış noktasına kadar muaftır**; çıkıştan sonraki her hücre `Walk` olmak zorundadır (plandaki öneri onaylandı).

## 1. Amaç

`[BOT] NAV=1` iken botun gönderdiği **her** `WIZ_MOVE` adımı (düz `/bot move` dahil), `CUser::HandlePacket()`'e verilmeden önce **kiriş** (botun o anki konumu → paketin konumu) olarak denetlenir: kirişin dokunduğu hücrelerden biri `Walk` değilse paket **gönderilmez**, `FAIRNESS_REJECT` (`rule:"CLI-08"`, `reason:"blocked_chord"`) yazılır ve yürüyüş durur. Bugün yalnızca adım uzunluğu denetlenir; planlayıcısız düz hedef adımı 6000 çiftin 447'sinde (%7,45) engelli hücreye değiyor (`docs/12` §13.1). `NAV=0` (varsayılan) veya `ENABLED=0` iken davranış **değişmez**.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.1 ilk iki madde: kural CLI-08, "kirişin dokunduğu **tüm** hücreler (muhafazakâr süpercover; hücre köşesi/vertex'ine değme dahil) `Walk` olmalı"; reddedilen paket gönderilmez (`FAIRNESS_REJECT`, kural `CLI-08`, sebep `blocked_chord`); zorunlu katman `Walk` süpercover'ıdır, eğim katmanı varsayılan **kapalıdır** (planlayıcı ile yalnızca tam segmentlerde tutarlı). `docs/12` §13.3: reddedilen paket takılma sayılmaz (`BLOCKED_BY_GUARD`).
- `docs/03` §13 (CLI-05 hız alanı, CLI-08 "ışınlanma yok / adım uzunluğu") ve `docs/13` §3: bot, insanın yapamayacağı hareketi yapamaz.
- Mevcut guard (`GameServer/Bot/ActionExecutor.cpp:94-110`): `BotCore::CheckMoveStep(speed, s->m_moveSpeed, serverLimit, stepMeters, elapsedMs)`; ret olunca `EmitFairnessReject(..., "Move", speedField ? "CLI-05" : "CLI-08", reason, ...)` ve `s->m_moveActive = false`, sonuç `MoveOutcome::REFUSED` + `reason` (`"speed_field"` veya `"step_too_long"`). **Mevcut `"CLI-08"` etiketi adım uzunluğu kuralıdır** (`reason:"step_too_long"`). Yeni kural aynı `rule:"CLI-08"` ailesindedir ama **ayrı ad taşır:** `reason:"blocked_chord"`, saf mantıkta ayrı enum (`ChordVerdict`, aşağıda), `MoveOutcome.reason` = `"blocked_chord"`. Karışmaması için `BotCore::MoveVerdict` enum'una **değer eklenmez** (`BotMotion.h` ve `MotionTests.cpp` değişmez).
- `SubmitMove` hareketin **tek** tıkanma noktasıdır: `TickMove` (`:230`, `/bot move`), `StopMove` (`:266`) ve (F5-62'de) yol izleme hepsi bu fonksiyondan paket gönderir. Guard burada olduğundan sonraki dilimler korumayı otomatik devralır.
- Paket konumu `uint16(nx*10+0.5)/10` ile nicemlenir (`:84-92`); kiriş **nicemlenmiş** paket konumuyla ölçülmelidir (sunucuya giden değer), `nx/nz` ile değil. Kirişin başı = botun sunucudaki güncel konumu (`user->GetX()/GetZ()`); `HandlePacket` konumu paket konumuna taşıdığı için bu, "önceki paket konumu"dur.
- `BotCore/NavSegment.h:330` `NavCheckStep(grid, x0, z0, x1, z1, checkSlope = false)`: `double` süpercover, `Ok / OutOfBounds / BlockedCell` (+ isteğe bağlı `SlopeTooSteep`), `cellX/cellZ` ilk ihlal hücresi; NaN/sonsuz girdi `OutOfBounds` (fail-closed). `NavGrid::CellOf(w) = floor(w / unit)` (`NavGrid.h:276-281`).
- F5-59: `NavService::Instance().Grid()` yalnızca **zone 71** ızgarasıdır. Bot başka bölgedeyse denetim uygulanamaz (`Skipped`).
- **Ön ölçüm `[V ön ölçüm: Claude, Python, 2026-10-03, uygulayıcı yeniden üretmeli]`:** `freezone_a_20050718.smd` olay ızgarasında (`x*513+z`, 4-bağlantılı bileşen) ana bileşen 88 508 hücre (`docs/12` §1 ile aynı); Karus doğuşu (1369,9; 1090,3) → hücre (342, 272) ve El Morad doğuşu (630,0; 920,0) → hücre (157, 230) **ana bileşende** (yani `Walk` olayı 1 + ana bileşen; `clearance` ayrıca değerlendirilmedi); arena A noktaları (1274,5; 892,6) ve (1276,3; 889,0) ana bileşende. Karus doğuş hücresinin güney/doğu komşularından üçü olay 0 (kıyıdadır). Sunucu doğuşta konuma rastgele sapma uygulayabilir (`AttackHandler.cpp` `Regene`; uygulayıcı doğrulasın): başlangıç hücresinin `Walk` olmaması **imkânsız değildir**.

### Tasarım kararları (otonom döngüde Claude kararı — gözden geçirilmeli; ADR-0006 Ek F5-61)

- **D1 ad:** `rule:"CLI-08"`, `reason:"blocked_chord"` (`docs/12` §13.1); ek alanlar `cell_x`, `cell_z`, `verdict` (`BlockedCell`/`OutOfBounds`). Mevcut alanlar (`value` = adım uzunluğu m, `limit` = 0,00) korunur.
- **D2 sıra:** önce `CheckMoveStep` (CLI-05 hız alanı, CLI-08 adım uzunluğu), sonra kiriş. İkisi de ihlalse ilk ihlal raporlanır (`step_too_long`).
- **D3 durma paketi:** kiriş uzunluğu `< kChordIgnoreMeters` (= 0,08 m; `NavChordGuard.h` içinde yeni sabit) denetlenmez: bot **her zaman durabilir**. Neden 0,05 değil: `StopMove` botun **nicemlenmemiş** konumunu (`GetX()` örn. sunucunun doğuş yerleştirmesi) `uint16(x*10+0.5)/10` ile nicemler; eksen başına hata ≤ 0,05 m, köşegen hata ≤ 0,0708 m, yani aynı-konum paketi `kStopSlackMeters`'i aşabilir ve botu hücre kenarında duruyorsa reddedebilirdi. 0,08 m hücrenin (4 m) %2'sidir; gerçek hareket adımları ≥ 0,1 m olduğundan atlama gerçek adım denetimini zayıflatmaz.
- **D4 atlama:** `NavService::Grid() == nullptr` (NAV=0, ENABLED=0, başlangıç hatası) **veya** bot `GetZoneID() != ZONE_RONARK_LAND` ise `Skipped`: davranış, çıktı ve telemetri bugünküyle bayt bayt aynıdır.
- **D5 başlangıç hücresi `Walk` değilse (öneri):** deadlock'u önlemek için kiriş, başlangıç hücresinden **çıktığı noktadan** itibaren denetlenir (başlangıç hücresinin kendisi muaf; çıkış noktası 1e-3 m ileri taşınır); kiriş başlangıç hücresinin içinde bitiyorsa `Ok`. Gerekçe: botun o konuma sunucu yerleştirmesiyle gelmesi botun hatası değildir; ama oradan çıkış yürünebilir hücrelere olmalıdır. Başlangıç konumu ızgara dışındaysa `OutOfBounds` (reddedilir). Alternatif (reddet) deadlock üretir: bot doğuş noktasından hiç yürüyemez.
- **D6 eğim:** `checkSlope = false` (docs/12 §13.1: zorunlu katman `Walk` süpercover'ı).

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavChordGuard.h` (yeni, saf mantık; yalnızca standart kütüphane + `NavSegment.h`, `BotMotion.h`; global/static yok, dinamik bellek yok): `ChordVerdict`, `ChordResult`, `CheckMoveChord`.
2. `ActionExecutor.cpp`: `SubmitMove` içinde `CheckMoveStep` bloğundan **sonra** kiriş denetimi; `EmitFairnessReject`'e isteğe bağlı ek alan parametresi (F4-41'in `skillId` kalıbı); `MoveOutcome.reason` yeni değer `"blocked_chord"` (`ActionExecutor.h` yorumu).
3. Birim testleri `Tests/BotCoreTests/NavChordGuardTests.cpp`.
4. Çalışma zamanı doğrulaması (Claude; §6 K9-K12).

**Kapsam dışı (yapılmayacak)**

- `/bot goto`, yol izleme, `NavPathfinder` (F5-62); takılma/yeniden planlama (F5-63); `NAV_*` telemetrisi (F5-64); durum temizliği (F5-65).
- Eğim katmanı (`checkSlope = true`), su katmanı (F5-60/F5-67), kiriş denetiminin Attack/Cast/diğer aksiyonlara genişletilmesi (yalnız `WIZ_MOVE`).
- `BotCore/BotMotion.h`, `BotCore/NavSegment.h`, `MoveVerdict`, `CheckMoveStep` **değişmez**. `BeginMove` (`bad_target` kuralı) değişmez.
- Oyun mekaniği: hız, adım uzunluğu, paket sıklığı değişmez; yalnızca reddedilen paketler artar.
- `docs/` (Claude: `docs/16` §3.2 `FAIRNESS_REJECT` satırı, `docs/12` §13.1, `docs/STATUS.md`).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavChordGuard.h` | yeni | ASCII + CRLF; yalnızca standart kütüphane ve kardeş başlıklar (R4) |
| `BotCore/BotCore.vcxproj` | değiştir | tek satır `<ClInclude Include="NavChordGuard.h" />` |
| `Tests/BotCoreTests/NavChordGuardTests.cpp` | yeni | |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | tek satır `<ClCompile Include="NavChordGuardTests.cpp" />` |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yalnızca `SubmitMove` guard bloğu ve `EmitFairnessReject` ek alan parametresi; `#include "NavService.h"`, `#include "../../BotCore/NavChordGuard.h"` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca `MoveOutcome::reason` yorumuna `"blocked_chord"` |

Bu listede olmayan bir dosyaya dokunmak gerekirse (ör. `proj-GameServer.vcxproj`: **gerekmez**, yeni `.cpp` yok) **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-61 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop`. F5-59'un `NavService.h` imzalarını açıp doğrula (sapma varsa **dur**, rapora yaz). `python3 tools/nav-export.py` ile `build/nav/zone71.navgrid` üret (gerçek-harita testleri aksi halde `SKIPPED` basar).
2. **`BotCore/NavChordGuard.h`** (iskelet; Allman, tab, CRLF, yorumlar İngilizce):

   ```cpp
   #pragma once
   // Chord walkability guard for the real movement path (F5-61; docs/12 s13.1, CLI-08 "blocked_chord").
   // Pure logic: the standard library, NavSegment.h and BotMotion.h only; no global/static state.
   // Separate from MoveVerdict (BotMotion.h): that enum is the step-length / speed-field rule.
   #include "BotMotion.h"
   #include "NavSegment.h"
   namespace BotCore
   {
   	// Packet positions are quantised to 0.1 m: the diagonal error of a stop-in-place packet is <= 0.0708 m.
	constexpr float kChordIgnoreMeters = 0.08f;

	enum class ChordVerdict { Ok, Skipped, BlockedCell, OutOfBounds };
   	struct ChordResult
   	{
   		ChordVerdict verdict = ChordVerdict::Skipped;
   		int cellX = 0;   // first offending cell; -1 for OutOfBounds
   		int cellZ = 0;
   		bool startExempt = false;   // the start cell was not Walk and was exempted (D5)
   	};
   	// grid == nullptr -> Skipped. Chord shorter than kChordIgnoreMeters -> Ok (a bot can always stop).
   	// Start cell not Walk -> the chord is checked from the point where it leaves the start cell
   	// (moved 1e-3 m past the cell boundary); a chord that ends inside the start cell is Ok.
   	// Start/end outside the grid or non-finite -> OutOfBounds. Slope layer off (checkSlope = false).
   	inline ChordResult CheckMoveChord(const NavGrid * grid, float x0, float z0, float x1, float z1);
   }
   ```

   Gövde: `NavCheckStep(*grid, x0, z0, x1, z1, false)` sonucunu `ChordResult`'a çevirir; D5 için başlangıç hücresi `grid->Walk(CellOf(x0), CellOf(z0))` değilse çıkış noktasını (kirişin hücre sınırı kesişimi) `double` ile hesaplayıp ileri taşır. Tüm üyeler `inline`.
3. **`BotCore/BotCore.vcxproj`** ve **`Tests/BotCoreTests/BotCoreTests.vcxproj`** tek satır eklemeleri.
4. **`GameServer/Bot/ActionExecutor.cpp`** `SubmitMove` (`CheckMoveStep` bloğundan hemen sonra, `NextDecisionId`/`ACTION_SUBMIT` öncesinde):

   ```cpp
   	// F5-61 (CLI-08 blocked_chord): the chord (current position -> packet position) must only touch Walk cells.
   	// Skipped (NAV off / not zone 71) leaves behaviour unchanged.
   	const BotCore::NavGrid * navGrid = user->GetZoneID() == ZONE_RONARK_LAND ? NavService::Instance().Grid() : nullptr;
   	BotCore::ChordResult chord = BotCore::CheckMoveChord(navGrid, user->GetX(), user->GetZ(), packetX, packetZ);
   	if (chord.verdict == BotCore::ChordVerdict::BlockedCell || chord.verdict == BotCore::ChordVerdict::OutOfBounds)
   	{
   		... EmitFairnessReject(s, user, decisionId, "Move", "CLI-08", "blocked_chord", stepMeters, 0.0f, 0, extra);
   		s->m_moveActive = false;
   		out.kind = MoveOutcome::REFUSED;
   		out.reason = "blocked_chord";
   		return out;   // HandlePacket is never called for a rejected chord
   	}
   ```

   `EmitFairnessReject`'e sona `const char * extraFields = nullptr` (ham JSON parçası: `,"cell_x":N,"cell_z":N,"verdict":"BlockedCell"`) eklenir; mevcut çağrılar değişmez ve çıktıları **bayt bayt aynı** kalır. `ActionExecutor.h` `MoveOutcome` yorumuna `"blocked_chord"` ekle.
5. **Testler** (`Tests/BotCoreTests/NavChordGuardTests.cpp`, `MiniTest.h`; adlar sabit; sentetik ızgara `NavGrid::Init` + `Build`, `Rng` sabit tohum):
   - `NavChord_Skipped_NoGrid`: `grid == nullptr` → `Skipped`; hiçbir koşulda `BlockedCell` değil.
   - `NavChord_Synthetic`: 9×9 ızgara, bir duvar sütunu: duvarı kesen kiriş → `BlockedCell` ve doğru hücre; açık satır boyunca → `Ok`; yalnızca hücre köşesine (vertex) değen kiriş duvar köşesindeyse → `BlockedCell` (muhafazakâr); bitiş ızgara dışı → `OutOfBounds`; NaN → `OutOfBounds`; sonuç kiriş yönünden bağımsız (simetri).
   - `NavChord_StopPacket_NeverBlocked`: uzunluk 0, 0,0707 m (köşegen nicemleme; (x,z)→(x+0,05; z+0,05)) ve 0,079 m, başlangıç **engelli** hücrede ve **hücre kenarında** → `Ok`; uzunluk 0,2 m ve engelli hücreye giren kiriş → `BlockedCell` (eşik gerçek adımı gizlemez).
   - `NavChord_StartNotWalk`: başlangıç engelli hücrede; yürünebilir hücrelere çıkan kiriş → `Ok` + `startExempt`; engelli hücreye çıkan → `BlockedCell`; başlangıç hücresi içinde biten → `Ok`; başlangıç ızgara dışı → `OutOfBounds`.
   - `NavChord_RealMap_StraightVectors`: `build/nav/zone71.navgrid` yoksa `NAVCHORD real map: SKIPPED (...)` basıp dön (`NavArenaTests.cpp:320-329` kalıbı); varsa `tools/nav-regress/good.txt:4-6` üç vektörü (`(654,818)->(646,806)` hücre `(163,203)`; `(958,946)->(946,958)` hücre `(239,237)`; `(994,1006)->(990,994)` hücre `(248,250)`) `CheckMoveChord` ile `BlockedCell` ve aynı ihlal hücresi vermeli; satır: `NAVCHORD real map: vectors=3 blocked=3`.
   - `NavChord_RealMap_PlannerPaths_Clean`: gerçek haritada ≥ 200 `near64` planlayıcı yolu (`NavPathfinder` + `NavSmoothPath`, sabit tohum), 6,75 m'lik **paket kirişlerine** bölünmüş (`NavSegmentAuditTests.cpp` yardımcılarını kopyala, `import` yok): kiriş sayısı ≥ 1000, `BlockedCell` **0** (AC-NAV-03 altyapısı; `docs/12` §13.1: 0/250 000). Kirişler `NavSegmentAuditTests.cpp:200-237` gibi **nicemlenmemiş** üretilir ve `blocked=0` olmalıdır (CHECK). Ek bilgi satırı (CHECK **yok**): aynı kirişlerin uçları `uint16(x*10+0.5)/10` ile nicemlenip tekrar denetlenir, `NAVCHORD planner quantised chords=<n> blocked=<m>`; `m > 0` ise Uygulayıcı Raporu'nda bulgu olarak yazılır (sunucuya giden değer nicemlenmiştir; planı bloke etmez, F5-62'de `NavDrive` payı için girdidir).
   - `NavChord_Perf` (`#ifndef _DEBUG`): 20 000 kiriş, `CheckMoveChord` p95 ≤ 0,02 ms (PM-M7 ile aynı kapı).
6. Derle ve test et (§7); `python3 tools/check-perception-contract.py` rc=0.
7. Sunucu çalıştırma bu planda uygulayıcıya düşmez: çalışma zamanı K9-K12'yi Claude yapar. Uygulayıcı Raporu'nu yaz; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; değişen dosyalar `touch` edilince **yeni uyarı yok**
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; yedi yeni test adı `[ OK ]` (`NavChord_Skipped_NoGrid`, `NavChord_Synthetic`, `NavChord_StopPacket_NeverBlocked`, `NavChord_StartNotWalk`, `NavChord_RealMap_StraightVectors`, `NavChord_RealMap_PlannerPaths_Clean`, `NavChord_Perf`); mevcut testler (özellikle `Motion*`) değişmeden geçer
- [ ] K3: gerçek-harita testleri `SKIPPED` **değil**: `NAVCHORD real map: vectors=3 blocked=3` ve `NAVCHORD planner chords>=1000 blocked=0` satırları raporda
- [ ] K4: `git diff --stat gece/2026-10-02...bot/F5-61` yalnızca §4 (6 dosya, 2 yeni) + plan dosyası; `BotCore/BotMotion.h`, `BotCore/NavSegment.h`, `docs/`, `tools/`, `proj-GameServer.vcxproj` farkı **0**; `git diff --check` boş
- [ ] K5: `NavChordGuard.h`'de `grep -n -E "windows.h|stdafx|GameServer|shared/|new |malloc|static " ` boş; `python3 tools/check-perception-contract.py` rc=0 (R1-R5 PASS)
- [ ] K6: `ActionExecutor.cpp`'de mevcut `"CLI-05"`/`"step_too_long"` çağrısı **değişmedi** (`git diff` o satırlarda yok); yeni çağrı `rule:"CLI-08"`, `reason:"blocked_chord"`; kiriş ret yolunda `HandlePacket` çağrısı yok (kod incelemesi, `dosya:satır` raporda)
- [ ] K7: kiriş denetimi `Skipped` iken (`Grid() == nullptr`) `SubmitMove` akışı ön-değişiklikle aynı: `EmitFairnessReject` mevcut çağrılarının çıktısı bayt bayt aynı (yeni parametre varsayılan `nullptr`)
- [ ] K8: yeni dosyalar ASCII + CRLF (`file` çıktısı raporda); `git grep -n "kChordIgnoreMeters" BotCore/NavChordGuard.h` durma paketi muafiyetini gösterir (tanım + kullanım ≥ 2 satır)
- [ ] K9 (Claude, çalışma zamanı; yalnızca bu plan): `GameServer.ini` `[BOT] ENABLED=1, NAV=1, TELEMETRY=decisions`; bir bot doğuş noktasında; Claude, ızgara ve `tools/nav-segment-check.py` mantığıyla **iki yürünebilir hücre arasındaki ama engelli hücreye değen** bir düz hedef bulur (6,75 m içinde) ve `/bot move <bot> <x> <z>` verir: `Bot_*.log`'da `move stopped (blocked_chord)`; telemetride `FAIRNESS_REJECT` `rule:"CLI-08"`, `reason:"blocked_chord"`, `cell_x/cell_z` = hesaplanan hücre; `ACTION_SUBMIT`/`WIZ_MOVE` **yok** (paket gönderilmedi); botun konumu değişmedi (`/bot list`)
- [ ] K10 (Claude): temiz bir hedefe (kiriş tamamen `Walk`) `/bot move` → yürür ve varır (`arrived ... after N packets`); `/bot stop <bot>` her zaman durur (durma paketi reddedilmez)
- [ ] K11 (Claude): `NAV=0` (veya anahtar yok) + `ENABLED=1`: K9'daki **aynı** komut eskisi gibi paketi gönderir (`SENT`, `FAIRNESS_REJECT` yok); yani varsayılan davranış değişmedi
- [ ] K12 (Claude): sunucu `stop` ile kapanır; K9 sonuçları `FAIRNESS_REJECT` sayısıyla birlikte rapora; "AC-NAV-03 kapandı" **yazılmaz** (kanıt F5-66)

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavChord_|NAVCHORD|tests,"
./tools/run-tests.sh Debug
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-61
git diff gece/2026-10-02...bot/F5-61 -- BotCore/BotMotion.h BotCore/NavSegment.h | wc -l   # 0
git diff --check gece/2026-10-02...bot/F5-61
# Claude (çalışma zamanı): ./tools/run-servers.sh start ; echo "move <bot> <x> <z>" >> /mnt/c/dev/fdp/server/BotCommands.txt ; grep -a "blocked_chord" /mnt/c/dev/fdp/server/Logs/Bot_*.log ; ./tools/run-servers.sh stop
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2.5: bu plan botu **kısıtlar**, ona yeni yetenek vermez. Denetim botun kendi konumundan ve herkesin sahip olduğu zone verisinden (F5-59 ızgarası) çalışır; sunucu dizilerine yeni erişim yoktur.
- Denetim **IOCP iş parçacığında** (`SubmitMove`) çalışır; `NavService::Grid()` yalnızca `const` okunur (F5-59: kurulumdan sonra hiç yazılmaz).
- Maliyet: kiriş başına ~0,0002 ms (PM-M7); tick bütçesini etkilemez.
- **Dürüstlük:** birim testleri kiriş mantığını ve sunucuya gitmeden önce reddi sınar; botun oyunda engelli hücreye **hiç girmediği** (AC-NAV-03) yalnızca 30 dk'lık çalışma zamanı denetimiyle (F5-66, bağımsız denetim aracı) kanıtlanır. Kiriş yalnızca **paket konumları arasındaki** doğru parçasını denetler; paketler arası gerçek istemci yolu bilinmez (`docs/12` §13.1: ara noktaları sunucu doğrulamaz).
- D5 (başlangıç hücresi muafiyeti) güvenlik gevşetmesi gibi okunabilir: muafiyet yalnızca **başlangıç hücresi** içindir; çıkış noktasından sonraki her hücre `Walk` olmak zorundadır. Karar otonom döngüde Claude'undur (ADR-0006 Ek F5-61), proje sahibi gözden geçirir.
- Beklenmedik durumda (F5-59 imzası farklı, `SubmitMove` yeniden yazılmış, ek dosya gerekiyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-61` (taban `gece/2026-10-02` @ `aeca42b`)
  - `24dd471 [F5-61] Kiris guard'i: CLI-08 blocked_chord (NavChordGuard + SubmitMove)` (6 kod dosyası + plan)
- Değişen dosyalar ve neden:
  - `BotCore/NavChordGuard.h` (yeni): `ChordVerdict`/`ChordResult`/`CheckMoveChord`, `kChordIgnoreMeters = 0.08f`; `NavCheckSegment` süpercover'ı sarar (D6 eğim kapalı), D3 kısa/aynı-konum muafiyeti, D5 başlangıç hücresi `Walk` değilse çıkış noktasından denetim, `grid == nullptr` → `Skipped`, NaN/ızgara dışı → `OutOfBounds`.
  - `BotCore/BotCore.vcxproj` (değişti): tek `ClInclude` satırı.
  - `Tests/BotCoreTests/NavChordGuardTests.cpp` (yeni): 7 test (aşağıda).
  - `Tests/BotCoreTests/BotCoreTests.vcxproj` (değişti): tek `ClCompile` satırı.
  - `GameServer/Bot/ActionExecutor.cpp` (değişti): `NavService.h` + `NavChordGuard.h` include; `EmitFairnessReject`'e sona `const char * extraFields = nullptr`; `SubmitMove`'da `CheckMoveStep` sonrası kiriş denetimi (satır 119-141): ihlalde `rule:"CLI-08"`, `reason:"blocked_chord"`, ek `cell_x`/`cell_z`/`verdict`, `m_moveActive=false`, `REFUSED`; ret yolunda `HandlePacket` (satır 159) **çağrılmaz**.
  - `GameServer/Bot/ActionExecutor.h` (değişti): yalnızca `MoveOutcome::reason` yorumuna `"blocked_chord"`.
  - `plans/F5-61-…md`: `Durum` ve bu rapor.
- Derleme sonucu (`./tools/build.sh Release`, `Debug`; rc=0/rc=0, son satırlar):
  ```
  proj-AIServer.vcxproj -> ...\AIServer.exe
  BotCoreTests.vcxproj -> ...\Tests\BotCoreTests.exe
  proj-GameServer.vcxproj -> ...\Server\GameServer.exe
  ```
  Değişen dosyalar `touch` edilip yeniden derlendiğinde yalnızca önceden var olan `GameServerDlg.cpp` C4834/C4267 uyarıları görüldü; `ActionExecutor.cpp`/`NavChordGuard*.h/cpp` için yeni uyarı 0.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔: Release rc=0, Debug rc=0; yeni uyarı 0.
  - K2 ✔: Release ve Debug `274 tests, 0 failed`; yedi yeni ad `[ OK ]` (taban 267 + 7); mevcut `Motion*` dahil gerilemesiz.
  - K3 ✔: aşağıdaki `NAVCHORD` satırları (SKIPPED değil).
  - K4 ✔: `git diff --stat gece/2026-10-02...bot/F5-61` yalnızca §4 (6 dosya, 2 yeni) + plan; `BotMotion.h`/`NavSegment.h` farkı 0 satır, `GameServer/proj-GameServer.vcxproj` farkı 0, `docs/`/`tools/` farkı 0; `git diff --check` boş.
  - K5 ✔: `grep -n -E "windows.h|stdafx|GameServer|shared/|new |malloc|static " BotCore/NavChordGuard.h` boş (rc=1); `check-perception-contract.py` `RESULT: PASS` (R1-R5 0), rc=0.
  - K6 ✔: mevcut `CLI-05`/`step_too_long` çağrısı (`ActionExecutor.cpp:102-104`) diff'te **yok**; yeni çağrı `ActionExecutor.cpp:134` (`rule:"CLI-08"`, `reason:"blocked_chord"`); ret dalı `ActionExecutor.cpp:141` `return out;` — `HandlePacket` `ActionExecutor.cpp:159`'dan önce.
  - K7 ✔ (kod okuması): `Grid() == nullptr` iken `CheckMoveChord` → `Skipped`, yeni `if` girmiyor; `EmitFairnessReject` yeni parametresi varsayılan `nullptr` olduğundan mevcut çağrıların çıktısı bayt bayt aynı.
  - K8 ✔: yeni dosyalar `ASCII text, with CRLF`; `git grep -n kChordIgnoreMeters BotCore/NavChordGuard.h` → tanım `:17` + kullanım `:53` (rapor/yorum satırları `:29`, `:101`).
  - K9-K12: Claude'un çalışma zamanı kriterleri; bu turda yapılmadı.
- Gerçek-harita test satırları (`NAVCHORD ...`):
  ```
  NAVCHORD vector[0] cell=(162,203)
  NAVCHORD vector[1] cell=(237,238)
  NAVCHORD vector[2] cell=(248,249)
  NAVCHORD real map: vectors=3 blocked=3
  NAVCHORD planner chords=9138 blocked=0
  NAVCHORD planner quantised chords=9138 blocked=0
  NAVCHORD perf: chords=20000 ms_p95=0.000300
  ```
  Nicemlenmiş planlayıcı kirişleri `blocked=0` (bulgu yok). Debug perf `ms_p95=0.001100` (kapı yalnızca Release, ≤ 0.02 ms).
- Plandan sapmalar ve gerekçeleri:
  1. **§5.5 `NavChord_Synthetic` "bitiş ızgara dışı → `OutOfBounds`"**: standart bir `NavGrid`'de (kenar hücreleri `Walk` değil) içeriden dışarı giden kiriş önce engelli kenar hücresine değdiği için `NavCheckSegment` onu `BlockedCell` döndürür, `OutOfBounds` değil. Test, `OutOfBounds` için **başlangıcı ızgara dışı** olan kirişi (`-1.0`) kullanır ve içeriden dışarı giden kirişi ayrıca `BlockedCell` olarak sabitler; `NavSegmentTests`'teki kenar-dışı örnekleri de böyledir.
  2. **§5.5 `NavChord_RealMap_StraightVectors` "aynı ihlal hücresi"**: `NavCheckSegment` **geçiş sırasındaki ilk** engelli hücreyi verir: `(162,203)`, `(237,238)`, `(248,249)`. Plandaki `(163,203)`, `(239,237)`, `(248,250)` ise `nav_measure`'in kendi `Blocked()`/Python oracle sıralamasından gelir; C++ guard ile yeniden üretilemez. Test, `NavSegmentAuditTests_StraightSteps` kalıbıyla `BlockedCell` + raporlanan hücrenin `Walk` olmaması üzerinden doğrular. K3'ün istediği `vectors=3 blocked=3` satırı üretilir.
  3. **`NavChordGuard.h` başlık yorumu**: plan iskeletindeki "no global/static state" ifadesi düz `static ` dizgesini içerdiği için planın **kendi K5 grep'ini** kirletir; "no global state and no dynamic memory" olarak yazıldı (davranış değişmez).
  4. `#include <algorithm>` ve `#include <cmath>` eklendi (`std::min`, `std::isfinite`, `std::floor`, `std::sqrt`); planın izin verdiği standart kütüphane kapsamında. Test dosyasına `std::abs(int)` için `<cstdlib>` eklendi.
  5. `NavChordGuard.h`'e D5 için `tExit >= 1.0` erken `Ok` dalı eklendi (kiriş başlangıç hücresinden çıkmadan bitiyorsa "içinde biter → Ok" kuralının uç durumu; NaN/negatif ilerleme üretmez).
- Açık sorular:
  - Yok (engelleyici değil). Not: çalışma alanında bana ait olmayan izlenmeyen `plans/.supervisor-stop` dosyası vardır; dokunulmadı ve commit'e alınmadı.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: **DOĞRULANDI**
- İncelenen: `gece/2026-10-02...bot/F5-61` @ `5b050e5` (2 commit: `24dd471` kod, `5b050e5` rapor). Gece modu (`AUTO_LOOP=1`): birleştirmeyi/push'u döngü betiği yapar, bu oturumda yapılmadı.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `ActionExecutor.cpp`, `NavChordGuard.h`, `NavChordGuardTests.cpp` `touch` edildi; `./tools/build.sh Release` rc=0 ve `Debug` rc=0; iki günlükte `warning Cxxxx` / `error Cxxxx` satırı 0 (yeni uyarı yok) |
| K2 | ✔ | `run-tests.sh Release` ve `Debug`: `274 tests, 0 failed`; yedi `NavChord_*` `[ OK ]`; `Motion*` dahil mevcut testler geçti |
| K3 | ✔ | Kendi koşumda: `NAVCHORD real map: vectors=3 blocked=3`; `NAVCHORD planner chords=9138 blocked=0`; `NAVCHORD planner quantised chords=9138 blocked=0`; SKIPPED yok (`build/nav/zone71.navgrid` mevcut) |
| K4 | ✔ | `git diff --stat gece/2026-10-02...bot/F5-61`: 7 dosya = §4'teki 6 + plan (2 yeni: `NavChordGuard.h`, `NavChordGuardTests.cpp`); `BotMotion.h`, `NavSegment.h`, `docs/`, `tools/`, `proj-GameServer.vcxproj` farkı 0 satır; `git diff --check` boş; `ActionExecutor.cpp` tek `-` satırı `uint32 skillId = 0)` imzası |
| K5 | ✔ | `grep -n -E "windows.h|stdafx|GameServer|shared/|new |malloc|static " BotCore/NavChordGuard.h` boş (rc=1); `check-perception-contract.py` rc=0, `RESULT: PASS` |
| K6 | ✔ | Mevcut `CLI-05`/`step_too_long` çağrısı (`ActionExecutor.cpp:109`) ve `BeginMove` çağrısı (`:233`) diff'te yok; yeni çağrı `ActionExecutor.cpp:134` (`"CLI-08"`, `"blocked_chord"`); ret dalı `:139-141` `return out;`, `HandlePacket` `:159`'da, yani ret yolunda çağrılmaz |
| K7 | ✔ | `Grid() == nullptr` veya bölge ≠ 71 → `navGrid = nullptr` → `CheckMoveChord` `Skipped` (`NavChordGuard.h:37-41`) → yeni `if` girmez; `extraFields` varsayılan `nullptr` (`:57`, `:74`), mevcut çağrıların çıktısı aynı. **Çalışma zamanı da doğruladı (K11)** |
| K8 | ✔ | `file`: iki yeni dosya `ASCII text, with CRLF line terminators`; `ActionExecutor.cpp` kodlaması öncesi/sonrası ASCII; `kChordIgnoreMeters` tanım `:17`, kullanım `:53` (+ yorum `:29`, `:101`) |
| K9 | ✔ | **Çalışma zamanı `[V]`** (Release, `bot/F5-61` ucundan; `[BOT] ENABLED=1, NAV=1, TELEMETRY=decisions`; günlükte `nav ready ... crc32=4fd154bc`): bot `BotWP_K` zone 71 (1274,0; 934,0). Hedef seçimi `tools/nav-segment-check.py` oracle'ı ile: (1270; 937) uç hücresi `Walk`, kiriş yalnızca `(317,233)` hücresine (engelli) değiyor. `move BotWP_K 1270 937` → `Bot_3_10_2026.log`: `move stopped (blocked_chord)`; telemetri: `FAIRNESS_REJECT` `rule:"CLI-08"`, `reason:"blocked_chord"`, `value:5.00`, `cell_x:317`, `cell_z:233`, `verdict:"BlockedCell"` (oracle ile aynı hücre); `type:"Move"` `ACTION_SUBMIT` **0**; `list`: konum 1274,0; 934,0 değişmedi |
| K10 | ✔ | **Çalışma zamanı `[V]`:** temiz hedef (1272; 938) → `ACTION_SUBMIT Move` + `ACTION_RESULT ok`, konum 1272,0; 938,0; 20 m'lik temiz hat (1272→1292) `arrived ... after 3 packets`; 40 m'lik yürüyüşte (1292→1252) ikinci paketten sonra `stop` → `stopped at (1278.4, 938.0)`, durma paketi `speed:0` `ACTION_RESULT ok`, `FAIRNESS_REJECT` yok |
| K11 | ✔ | **Çalışma zamanı `[V]`:** sunucu yeniden başlatıldı, `NAV=0` (sunucu ini'yi `NAV=0` ile yeniden yazdı), `ENABLED=1`: aynı `move BotWP_K 1270 937` → `arrived at (1270.0, 937.0) after 1 packets`, `ACTION_SUBMIT Move` + `ACTION_RESULT ok`, `FAIRNESS_REJECT` 0, `NavService` satırı yok |
| K12 | ✔ | `run-servers.sh stop` iki kez de nazik kapanış, `0/3 hazır`; `GameServer.ini` yedekten geri alındı (md5 öncesi = sonrası = `791b71379a7173f877b0dcfc04595a66`), `BotCommands.*` kalmadı. NAV=1 koşusunda toplam `FAIRNESS_REJECT` = **1** (`blocked_chord`), NAV=0 koşusunda **0**. "AC-NAV-03 kapandı" **yazılmadı** (kanıt F5-66) |

- Kod kalitesi / kurallar: `NavChordGuard.h` yalnızca standart kütüphane + kardeş başlıklar (R4); saf mantık, global/dinamik bellek yok. Guard `SubmitMove`'ta `CheckMoveStep`'ten sonra, `ACTION_SUBMIT`/`HandlePacket` öncesinde; NaN/ızgara dışı fail-closed. Bot sistemi ve `NAV` varsayılan kapalı; oyun mekaniği, hız, adım uzunluğu değişmedi (yalnızca reddedilen paketler artar). `NavService::Grid()` yalnızca `const` okunur. Uygulayıcının dört sapması (özellikle `NavCheckSegment`'in geçiş sırasındaki ilk hücreyi vermesi: gerçek koşuda hücre oracle ile aynı çıktı) gerekçeli ve kabul edildi.
- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. **Not (açık risk, `KI-022` açıldı):** `NavChordGuard.h:53` D3 muafiyeti (`< 0,08 m` kiriş denetlenmez) plandaki "gerçek hareket adımları >= 0,1 m" varsayımına dayanıyor; ama `StepToward` son adımı kalan mesafenin tamamı yapar (`BotMotion.h:51`), yani önceki tam adımdan sonra kalan 0 < d < 0,08 m ise bu son paket denetlenmez. `/bot move` hedefi engelli bir hücrenin 0,08 m içindeyse bot en çok 0,08 m engelli hücreye girebilir. Hedef `Walk` hücredeyse etkisi yok; F5-62 yol izleme hedefleri `Walk` hücre merkezleridir; AC-NAV-03 kanıtı (F5-66) bunu ölçer. Karar F5-62 planında (ör. kısa kirişte uç hücresi `Walk` değilse ve başlangıç hücresinden farklıysa reddet) verilmeli.
  2. **Not:** D5 çıkış noktası kirişi köşeden terk ederse köşeye değen iki yan hücre denetlenmez (çıkış noktası 1e-3 m ileri taşınır, `NavChordGuard.h:124-126`). Yalnızca zaten `Walk` olmayan bir hücreden çıkışta ve tam köşede; etkisi ihmal edilebilir, kayıt için not.
  3. **Not (üslup):** `NavChordGuardTests.cpp` içindeki tüm `NavChord_*` adları ve satır biçimleri plana uygun; `OutOfBounds` sentetik testi plandan farklı (başlangıç ızgara dışı) ama uygulayıcının gerekçesi doğru.
- Düzeltme talimatı: yok (`DOĞRULANDI`).
