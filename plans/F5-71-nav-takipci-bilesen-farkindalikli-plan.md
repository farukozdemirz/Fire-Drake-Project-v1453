# F5-71: `NavFollower` bileşen farkındalıklı planlama (`UpdateReachable`) ve KI-023 kapanışı: `NavBudget_Deferred_Chase_Sim` 0,625 sabitlemesinin kaldırılması

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02, merge `1bfb885`) |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-71 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `KAPANDI`: F5-04 (`NavFollower`), F5-05 (`NavReach`), F5-53 (`NavBudget`, chase simülasyonu), F5-69 (eğim 0,45, KI-023'ün kaynağı). **F5-62 ve F5-70'e bağımlı DEĞİLDİR** ve onlarla dosya paylaşmaz (F5-62, `DOĞRULANDI` ve `gece/2026-10-02`ye birleşti: `NavDrive.h`, `NavDriveTests.cpp`, iki `.vcxproj`; bu plan: `NavTrack.h`, `NavTrackTests.cpp`, `NavBudgetTests.cpp`). F5-63 (Follow kipi) bu planın `UpdateReachable`ını kullanır |
| İlgili gereksinim / kabul | `docs/12` §4.2 ("öngörü noktası yürünebilir değilse süre yarıya indirilir"), §4.3 (bileşen farkı A* çalıştırılmadan bilinir), §13.5 (bütçe); ADR-0006 Ek F5-04/F5-05, ADR-0024 (eğim 0,45); KI-023; AC-NAV-02 (`NavTrack_Perf` p95 ≤ 2,0 ms korunur) |
| Tahmini büyüklük | S (3 dosya; saf mantık, sunucu ikilisi değişmez, `NavFollower`ın sunucuda çağıranı yoktur) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu, ön-plan) |

---

## 1. Amaç

KI-023'ü kök nedeniyle kapatır. `NavFollower::Update`, hedefin 1,5 sn ilerisine kestirilen **öngörü noktasını** yalnızca `Walk` diye kabul eder; 0,45 eğim sınırıyla (ADR-0024) ana bileşenin dışında kalan **eğim cepleri** de `Walk` olduğundan nokta bir cebe düşebilir, halkanın tüm hücreleri botun bileşeninde olmaz ve 3 A* denemesi de `NodeLimit` ile (3 × 20 000 genişletme) boşa gider. Bu plan `NavFollower`a isteğe bağlı bileşen etiketi (`NavReach::ComponentOf`) kullanan `UpdateReachable` ekler: öngörü noktası botun bileşeninde değilse süre yarıya indirilir, halkada botun bileşeni dışındaki hücreler A* çalıştırılmadan atlanır. Sonra `NavBudget_Deferred_Chase_Sim`in 0,625 sabitlemesi kaldırılır ve test üretim eğimi 0,45'te (ve `UpdateReachable` ile) geçer. `Update`in davranışı bayt bayt aynı kalır.

## 2. Bağlam (okunması zorunlu)

Satır numaraları `gece/2026-10-02` @ `271e17e` üzerinde doğrulandı; `26f10c4` üzerinde yeniden doğrulandı, kayma yok (bkz. Tazeleme notu).

- `docs/KNOWN_ISSUES.md` KI-023: sabitleme nedeni (`follow_stale_ticks=8`, `noPlanPerBot=1200`, `without_plan_pct=6.5`). `docs/12` §4.2 (öngörü noktası yürünebilir değilse süre yarıya iner), §4.3 (farklı bileşen = A* `NoPath`, A* çalıştırılmadan bilinir).
- `BotCore/NavTrack.h`: `class NavFollower` `:169`, `Update` bildirimi `:180`, `private:` `:188`, `Update` gövdesi `:331-`; öngörü döngüsü `:376-394` (kabul koşulu `:385` `grid.Walk(...)`); `botCell` hesabı `:396-398` (öngörüden **sonra**); aday döngüsü `:422-` (`limit`, `for (int i = 0; i < limit; ++i)`). `NavFollowPlan` alanları `:150-167` (`tries`, `expanded`, `pathStatus`).
- `BotCore/NavReach.h:12` `#include "NavTrack.h"` (yani `NavTrack.h` `NavReach.h`yi **içeremez**, döngüsel include; bu yüzden etiket kaynağı **şablon parametresi**dir). `NavReach::ComponentOf(x, z)` (`:37`): −1 = ızgara dışı / `Walk` değil / `Build` öncesi. `NavReachJudge::Judge` (`:218-`) halkayı `plan.predX/predZ`'den kurar: öngörü noktası cepteyse yargı yanlış "Component" çıkar; bu planın öngörü düzeltmesi onu da doğrular.
- `Tests/BotCoreTests/NavBudgetTests.cpp`: sabitleme bloğu `:471-476` (`pinnedParams`), simülasyondaki `Update` çağrısı `:716` (ikinci `Update` çağrısı `:1048` `NavBudget_RealMap_Load`a aittir ve **değişmez**), `snapWalk` lambda'sı `:613`, hareket bloğu sonu `:814` (`if (!moved && !follow)`). Eşikler `:911-` (B: `followStaleTicks == 0`, `bNoPlanPct <= 3.0`, `bMean <= 1.25 * aMean`; B2 benzeri).
- `Tests/BotCoreTests/NavTrackTests.cpp`: yardımcılar `CellIndex` `:19`, `Cell` `:24`, `HeightZeros` `:32`, `RingEvents` `:38`, `MakeNav` `:61`, `SegmentsClear` `:70`, `Dist` `:80`; mevcut cep senaryosu `NavTrack_Follower_Failures` (c) `:727-` (20 m yükseltilmiş blok; `Update` ile `PathFailed`, `tries == 3`: **değişmeden geçmelidir**). Dosya 1823 satır, sonu `NavTrack_Chase_Sim_Cadence`.
- Sunucuda `NavFollower`/`NavReach` çağıranı yoktur (`grep -rn "NavFollower\|NavReach" GameServer` boş `[V]`): bu planın üretim davranışı etkisi yoktur; ilk çağıran F5-63'tür.

### Kök neden ölçümü `[V: MSVC Release+Debug, geçici deneme, commit edilmedi]`

`NavBudget_Deferred_Chase_Sim`ın sabitlemesi kaldırılınca (0,45) mode B `follow_stale_ticks=8` (4/4 koşu; `deferred_ticks=366`, `hold_ticks=29`). Geçici günlükle:

1. **Neden 1 (bu planın düzelttiği):** bayat bot `PathFailed`, `pathStatus=NodeLimit`, `tries=3`, `expanded=60000`; öngörü hücresi `Walk` ama ana bileşen dışında (`predComp=301`, ana `0`), halkadaki 9 hücrenin 9'u botun bileşeninde değil; hedefin kendisi ana bileşendedir. Başarısız yenileme `planAtMs`'ı yenilemediğinden bot 5 sn sonra "bayat" sayılır; her yenileme 60 000 genişletmeye mal olur (`NavBudget` bütçesi 1,5 ms).
2. **Neden 2 (bu planda çözülmeyen, kayda geçen):** simülasyon botları düzleştirilmiş kirişlerle yürür; kiriş yalnızca `Walk` süpercover'ıyla doğrulandığından cep hücrelerinden geçebilir. Bot bir cep hücresinde durursa planlayıcı oradan **çıkamaz** (cebin kenarları eğimle kapalı): B2 koşusunda bot 10 (başlangıç = B'nin bitiş konumu, hücre (336, 249), bileşen 298) tüm 1200 tur, bot 12 (t = 35 s'de hücre (349, 248)) 144 yenileme boyunca `PathFailed` aldı (`without_plan_pct=6.5`, `follow_stale_ticks=159`). Üretimde de olabilir (düzleştirilmiş rota kirişi cepten geçer, bot orada durur: Follow'da hedef kaybı/tutma, `Replan` başlangıcı). Bu plan bunu **düzeltmez**; test simülasyonunda botları hedef gibi ana bileşende tutar (§5 adım 4) ve doğrulamada KI olarak kayda geçer.
3. **Karşılaştırma (aynı simülasyon, 0,45):** yalnızca ana bileşene kıskaçlama (Neden 2'nin çözümü) `follow_stale_ticks=8`'i **gidermez** (neden 1 sürer); yalnızca `UpdateReachable` B'yi düzeltir ama B2'de neden 2 yüzünden `without_plan_pct=6.5` kalır; **ikisi birlikte** geçer. Dolayısıyla üretim değişikliği (neden 1) gereklidir, yalnızca testi gevşetmek yetmez.
4. **Birlikte uygulanmış prototip, Release (`280 tests, 0 failed`):** A `dist_mean=10.86`; B `deferred_ticks=21 hold_ticks=21 follow_stale_ticks=0 without_plan_pct=0.1 dist_mean=12.33`; B2 `deferred_ticks=7272 hold_ticks=55 follow_stale_ticks=0 without_plan_pct=0.3 dist_mean=16.70`. Debug: B `deferred_ticks=119 hold_ticks=80 plan_wait_max=900 dist_mean=12.59`; B2 `without_plan_pct=0.5 follow_stale_ticks=0` (`280 tests, 0 failed`). Prototip üç ardışık Release koşusunda aynı B satırını verdi (zamanlamaya bağlı sayaçlar ±birkaç tur oynayabilir; değişmezler sabit).

### Tasarım kararları (otonom döngüde Claude kararı, yeni ADR yok; ADR-0006 Ek F5-04/F5-05 ve `docs/12` §4.2/§4.3'ün uygulaması. Doğrulamada `docs/12` §4.2'ye tek cümle eklenir)

1. **API.** `template <class Reach> bool NavFollower::UpdateReachable(grid, pathfinder, nowMs, botX, botZ, botSpeedMps, params, const Reach & reach)`; `Reach` = `NavReach` ya da `int ComponentOf(int x, int z) const` sağlayan herhangi bir tür. `Update` aynı imzayla kalır ve ortak özel şablon `UpdateImpl<Reach>(..., const Reach * reach)`'e `NavNoReach` ve `nullptr` ile yönlenir (`NavNoReach::ComponentOf` hep −1; asla çağrılmaz). Şablon seçilmesinin nedeni döngüsel include'u ve yeni başlık dosyasını (`.vcxproj` değişikliği) önlemektir.
2. **Bot hücresi etiketsizse** (`botComp < 0`: `Walk` değil/ızgara dışı) hiçbir süzme uygulanmaz: davranış `Update` ile aynıdır (A* `InvalidStart` verir).
3. **Öngörü kabulü:** nokta `Walk` **ve** (`botComp < 0` ya da bileşeni `botComp`). Aksi halde mevcut yarıya indirme (4 deneme) sürer; hiçbiri kabul edilmezse öngörü = gözlenen hedef (mevcut davranış).
4. **Aday süzme:** halka adayı bileşeni `botComp`'tan farklıysa A* **çalıştırılmadan** atlanır ve deneme sayılmaz; döngü `m_plan.tries < maxTries` iken tüm adaylar üzerinde sürer (`Update` yolunda her aday denendiğinden eski `limit` ile aynı sonuç). Hiç A* koşulmazsa `status = PathFailed`, `pathStatus = NoPath` (kanıtlı: farklı bileşen), `tries = 0`, `expanded = 0`; `NavReachJudge` bunu `Unreachable/Component` yargılar.
5. `ringMaxTries` artık "en çok bu kadar **A\* koşusu**" demektir (belge yorumuna yazılır).

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavTrack.h`: `UpdateReachable`, `UpdateImpl`, `NavNoReach` (§5 adım 2).
2. `Tests/BotCoreTests/NavTrackTests.cpp`: `#include <BotCore/NavReach.h>` ve 4 yeni test (§5 adım 3).
3. `Tests/BotCoreTests/NavBudgetTests.cpp` (yalnızca `NavBudget_Deferred_Chase_Sim`): sabitleme kalkar, `UpdateReachable` kullanılır, botlar ana bileşende tutulur (§5 adım 4).

**Kapsam dışı (yapılmayacak)**

- `BotCore/NavReach.h`, `NavPath.h`, `NavSmooth.h`, `NavGrid.h`, `NavBudget.h`, `NavStuck.h`, `NavDrive.h` (F5-62'nindir), `.vcxproj` dosyaları: **değişmez**. `GameServer/`, `AIServer/`, `shared/`, `docs/`, `tools/`: değişmez.
- Cebe düşmüş botun çıkarılması (Neden 2), düzleştirmenin cep hücrelerinden kaçınması, `NavService`'e `NavReach` bağlanması, `NavReachJudge` değişikliği: F5-63/F5-68 ve ayrı KI; bu planda yok.
- `Update` imzası/davranışı, `NavFollowParams`, `NavFollowPlan` alan ekleri: yok (F5-63'ün `ReplanDue/InvalidatePlan/field` eklemeleri onun planındadır; F5-63 HAZIR yapılırken bu planın `UpdateImpl`'ine göre yeniden doğrulanır).
- `NavBudget_RealMap_Load` (`:1048` `Update` çağrısı) ve diğer testlerin eşikleri: değişmez; hiçbir eşik gevşetilmez, hiçbir test yeniden sabitlenmez.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavTrack.h` | değiştir | yalnızca ASCII + CRLF, tab, Allman; yeni `#include` yok |
| `Tests/BotCoreTests/NavTrackTests.cpp` | değiştir | yeni include + dosya sonuna 4 test (ASCII + CRLF) |
| `Tests/BotCoreTests/NavBudgetTests.cpp` | değiştir | yalnızca `NavBudget_Deferred_Chase_Sim` (3 küçük düzenleme) |

Yeni dosya yok; `.vcxproj` değişmez. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-71 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucu `[UP]` ise `./tools/run-servers.sh stop`. `python3 tools/nav-export.py`. §2'deki satırları aç ve doğrula (kayma/sapma varsa **dur**, `Durum: UYGULANIYOR (BLOKE)`). Başlangıç ölçümü (rapora yaz): `./tools/run-tests.sh Release 2>&1 | grep -E "tests,"` ve `./build/bin/x86-Release/Tests/BotCoreTests.exe --list | wc -l`.
2. **`BotCore/NavTrack.h`** (doğrulanmış prototip; satır sonu CRLF korunur):
   - `NavFollower` içinde `Update` bildiriminden sonra (public) ekle:

     ```cpp
     		// Same as Update, but with edge-connected component labels (F5-71; `Reach` is NavReach or any
     		// type with `int ComponentOf(int x, int z) const`, -1 = off-grid / not Walk). When the bot's
     		// own cell is labelled: a lead-predicted point is accepted only inside the bot's component,
     		// and ring cells outside it are skipped without an A* run (no try counted; ringMaxTries
     		// bounds A* runs). Bot cell not labelled (not Walk / off-grid): identical to Update.
     		template <class Reach>
     		bool UpdateReachable(const NavGrid & grid, NavPathfinder & pathfinder, int64_t nowMs,
     			float botX, float botZ, float botSpeedMps, const NavFollowParams & params, const Reach & reach)
     		{
     			return UpdateImpl<Reach>(grid, pathfinder, nowMs, botX, botZ, botSpeedMps, params, &reach);
     		}
     ```

   - `private:` bölümünün başına (`m_tracker` öncesi) ekle:

     ```cpp
     		template <class Reach>
     		bool UpdateImpl(const NavGrid & grid, NavPathfinder & pathfinder, int64_t nowMs,
     			float botX, float botZ, float botSpeedMps, const NavFollowParams & params, const Reach * reach);
     ```

   - `inline bool NavFollower::Update(...)` tanımını (`:331`) şu üçe böl: (i) önce `NavNoReach` (yorum: "Stand-in for the Reach parameter of UpdateImpl when no labels are given (never dereferenced)."; `struct NavNoReach { int ComponentOf(int, int) const { return -1; } };`), (ii) ince `Update`: `return UpdateImpl<NavNoReach>(grid, pathfinder, nowMs, botX, botZ, botSpeedMps, params, nullptr);`, (iii) eski gövde `template <class Reach> bool NavFollower::UpdateImpl(<aynı parametreler>, const Reach * reach)` olur (`inline` yazılmaz; şablondur).
   - Gövdede **yalnızca** şu dört değişiklik (başka satıra dokunma):
     1. `botCell` bildirimi ve hesabı (`:396-398`) "Velocity and lead prediction..." yorumunun (`:361`) **önüne** taşınır ve altına eklenir: `const int botComp = reach ? reach->ComponentOf(botCell.x, botCell.z) : -1;` (yorum: `// -1 when no labels were given or the bot cell is not labelled: no component filtering.`). Eski yerdeki `botCell` satırları silinir (`NavRingCells` çağrısı aynı kalır).
     2. Öngörü kabul koşulu (`:385`) şu olur: `if (grid.Walk(grid.CellOf(qx), grid.CellOf(qz)) && (botComp < 0 || reach->ComponentOf(grid.CellOf(qx), grid.CellOf(qz)) == botComp))` (iki satıra bölünebilir).
     3. `const int limit = ...;` iki satırı silinir; döngü `for (size_t i = 0; i < m_candidates.size() && m_plan.tries < maxTries; ++i)` olur ve gövdenin ilk satırı: `if (botComp >= 0 && reach->ComponentOf(m_candidates[i].x, m_candidates[i].z) != botComp) continue;   // provably NoPath: not worth an A* run, not a try` (Allman: `continue;` ayrı satırda, `if` altında).
     4. Gövdede `m_candidates[static_cast<size_t>(i)]` iki yerde (`Find` ve `m_plan.goal`) `m_candidates[i]` olur.
   - Diğer her şey (due kuralı, hız, `NavRingCells`, `NavSmoothPath`, `m_plan` alanları) aynen kalır. Doğrulama: `git diff -U0 BotCore/NavTrack.h | grep -c '^-[^-]'` ≤ 12 (prototipte 9).
3. **`Tests/BotCoreTests/NavTrackTests.cpp`:** `#include <BotCore/NavReach.h>` (`NavTrack.h` include'unun altına). Dosya sonuna anonim ad alanında `MakePocketGrid()` ve 4 test ekle (aşağıdaki kod doğrulanmış prototiptir; g++ ve MSVC'de 4/4 geçti; CRLF ile yaz):

   ```cpp
   namespace
   {
   	// n = 40, unit 4 m, blocked border; a 4x4 plateau (x 20..23, z 18..21) raised 20 m: its cells are
   	// Walk but every edge to the ground is closed by the slope limit, so it is an edge-isolated pocket.
   	BotCore::NavGrid MakePocketGrid()
   	{
   		const int n = 40;
   		std::vector<float> heights = HeightZeros(n);
   		for (int x = 20; x <= 23; ++x)
   		{
   			for (int z = 18; z <= 21; ++z)
   				heights[CellIndex(n, x, z)] = 20.0f;
   		}
   		return MakeNav(n, 4.0f, RingEvents(n), heights);
   	}
   }

   TEST_CASE("NavTrack_Reach_LeadInPocket")
   {
   	BotCore::NavGrid grid = MakePocketGrid();
   	BotCore::NavReach reach;
   	reach.Build(grid);
   	REQUIRE(reach.ComponentCount() == 2);
   	REQUIRE(grid.Walk(20, 19));
   	REQUIRE(reach.ComponentOf(20, 19) != reach.ComponentOf(8, 19));

   	BotCore::NavPathfinder pf;
   	BotCore::NavFollowParams params;
   	params.ringMinM = 0.0f;
   	params.ringMaxM = 3.0f;
   	const float botX = grid.CellCenter(8);
   	const float botZ = grid.CellCenter(19);

   	// Without labels the 1.5 s lead point (83.6, 78) lands on the plateau; every ring cell is in the pocket.
   	BotCore::NavFollower oldF;
   	oldF.ObserveTarget(0, 70.0f, 78.0f);
   	oldF.ObserveTarget(500, 73.4f, 78.0f);
   	CHECK(oldF.Update(grid, pf, 500, botX, botZ, 4.5f, params));
   	CHECK(oldF.Plan().status == BotCore::NavFollowStatus::PathFailed);
   	CHECK(oldF.Plan().pathStatus == BotCore::NavPathStatus::NoPath);
   	CHECK(oldF.Plan().leadSec == 1.5f);
   	CHECK(oldF.Plan().tries == 2);

   	// With labels the lead is halved until its point is back in the bot's component (0.75 s -> (78.5, 78)).
   	BotCore::NavFollower f;
   	f.ObserveTarget(0, 70.0f, 78.0f);
   	f.ObserveTarget(500, 73.4f, 78.0f);
   	CHECK(f.UpdateReachable(grid, pf, 500, botX, botZ, 4.5f, params, reach));
   	CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
   	CHECK(std::fabs(f.Plan().leadSec - 0.75f) < 1e-4f);
   	CHECK(f.Plan().goal == Cell(19, 19));
   	CHECK_EQ(f.Plan().tries, 1);
   	CHECK(reach.ComponentOf(f.Plan().goal.x, f.Plan().goal.z) == reach.ComponentOf(8, 19));
   	CHECK(!f.Plan().smooth.waypoints.empty());
   	CHECK(SegmentsClear(grid, f.Plan().smooth.waypoints));
   }

   TEST_CASE("NavTrack_Reach_SkipPocketCandidates")
   {
   	BotCore::NavGrid grid = MakePocketGrid();
   	BotCore::NavReach reach;
   	reach.Build(grid);

   	BotCore::NavPathfinder pf;
   	BotCore::NavFollowParams params;
   	params.ringMinM = 0.0f;
   	params.ringMaxM = 6.0f;
   	// Target on the pocket's east edge: the ring holds 4 plateau cells (nearest to the bot) and 2
   	// main-component cells (24, 19) and (24, 20) behind it.
   	const float botX = grid.CellCenter(10);
   	const float botZ = grid.CellCenter(19);

   	BotCore::NavFollower oldF;
   	oldF.ObserveTarget(0, 94.0f, 80.0f);
   	CHECK(oldF.Update(grid, pf, 0, botX, botZ, 4.5f, params));
   	CHECK(oldF.Plan().status == BotCore::NavFollowStatus::PathFailed);
   	CHECK_EQ(oldF.Plan().tries, 3);

   	BotCore::NavFollower f;
   	f.ObserveTarget(0, 94.0f, 80.0f);
   	CHECK(f.UpdateReachable(grid, pf, 0, botX, botZ, 4.5f, params, reach));
   	CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
   	CHECK(f.Plan().goal == Cell(24, 19));
   	CHECK_EQ(f.Plan().tries, 1);   // the four plateau cells were skipped, not tried
   	CHECK(f.Plan().expanded > 0);
   	CHECK(SegmentsClear(grid, f.Plan().smooth.waypoints));

   	// maxTries counts A* runs only: with ringMaxTries = 1 the plan still reaches the first main-component cell.
   	BotCore::NavFollowParams one = params;
   	one.ringMaxTries = 1;
   	BotCore::NavFollower g;
   	g.ObserveTarget(0, 94.0f, 80.0f);
   	CHECK(g.UpdateReachable(grid, pf, 0, botX, botZ, 4.5f, one, reach));
   	CHECK(g.Plan().status == BotCore::NavFollowStatus::Planned);
   	CHECK(g.Plan().goal == Cell(24, 19));
   	CHECK_EQ(g.Plan().tries, 1);
   }

   TEST_CASE("NavTrack_Reach_ProvablyUnreachable")
   {
   	BotCore::NavGrid grid = MakePocketGrid();
   	BotCore::NavReach reach;
   	reach.Build(grid);
   	BotCore::NavPathfinder pf;
   	BotCore::NavFollowParams params;
   	params.ringMinM = 0.0f;
   	params.ringMaxM = 3.0f;

   	// (a) Target in the pocket, bot outside: every ring cell is skipped, no A* runs at all.
   	{
   		const float botX = grid.CellCenter(10);
   		const float botZ = grid.CellCenter(19);
   		BotCore::NavFollower oldF;
   		oldF.ObserveTarget(0, 88.0f, 80.0f);
   		CHECK(oldF.Update(grid, pf, 0, botX, botZ, 4.5f, params));
   		CHECK(oldF.Plan().status == BotCore::NavFollowStatus::PathFailed);
   		CHECK_EQ(oldF.Plan().tries, 3);
   		CHECK(oldF.Plan().expanded > 0);

   		BotCore::NavFollower f;
   		f.ObserveTarget(0, 88.0f, 80.0f);
   		CHECK(f.UpdateReachable(grid, pf, 0, botX, botZ, 4.5f, params, reach));
   		CHECK(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
   		CHECK(f.Plan().pathStatus == BotCore::NavPathStatus::NoPath);
   		CHECK_EQ(f.Plan().tries, 0);
   		CHECK_EQ(f.Plan().expanded, 0);
   		CHECK(f.Plan().smooth.waypoints.empty());

   		// The F5-05 judgement of that plan: provably unreachable by component.
   		BotCore::NavReachJudge judge;
   		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), params, BotCore::NavUnreachParams(), botX, botZ);
   		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
   		CHECK(j.reason == BotCore::NavUnreachReason::Component);
   	}

   	// (b) Bot inside the pocket, target outside: the planner cannot leave the pocket (recorded
   	// limitation, KI: bot stranded in a pocket; not fixed by F5-71).
   	{
   		const float botX = grid.CellCenter(21);
   		const float botZ = grid.CellCenter(19);
   		BotCore::NavFollower f;
   		f.ObserveTarget(0, 50.0f, 78.0f);
   		CHECK(f.UpdateReachable(grid, pf, 0, botX, botZ, 4.5f, params, reach));
   		CHECK(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
   		CHECK_EQ(f.Plan().tries, 0);
   		CHECK_EQ(f.Plan().expanded, 0);
   	}

   	// (c) Bot cell not Walk (border cell): no labels apply, identical to Update (InvalidStart).
   	{
   		BotCore::NavFollower a;
   		BotCore::NavFollower b;
   		a.ObserveTarget(0, 50.0f, 78.0f);
   		b.ObserveTarget(0, 50.0f, 78.0f);
   		CHECK(a.Update(grid, pf, 0, 2.0f, 2.0f, 4.5f, params));
   		CHECK(b.UpdateReachable(grid, pf, 0, 2.0f, 2.0f, 4.5f, params, reach));
   		CHECK(a.Plan().status == BotCore::NavFollowStatus::InvalidStart);
   		CHECK(b.Plan().status == BotCore::NavFollowStatus::InvalidStart);
   		CHECK_EQ(a.Plan().tries, b.Plan().tries);
   	}
   }

   TEST_CASE("NavTrack_Reach_SingleComponent_Identical")
   {
   	// One edge-connected component (a wall with a gap): UpdateReachable must equal Update bit for bit.
   	const int n = 40;
   	std::vector<int16_t> events = RingEvents(n);
   	for (int z = 1; z <= n - 2; ++z)
   	{
   		if (z < 18 || z > 21)
   			events[CellIndex(n, 20, z)] = 0;
   	}
   	BotCore::NavGrid grid = MakeNav(n, 4.0f, events, HeightZeros(n));
   	BotCore::NavReach reach;
   	reach.Build(grid);
   	REQUIRE(reach.ComponentCount() == 1);

   	BotCore::NavPathfinder pf;
   	BotCore::Rng rng(20261004u);
   	std::vector<BotCore::NavCell> walk;
   	for (int x = 0; x < n; ++x)
   		for (int z = 0; z < n; ++z)
   			if (grid.Walk(x, z))
   				walk.push_back(Cell(x, z));
   	REQUIRE(!walk.empty());

   	int planned = 0;
   	for (int i = 0; i < 300; ++i)
   	{
   		const BotCore::NavCell bc = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
   		const BotCore::NavCell tc = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
   		const float vx = (float)(rng.NextDouble() * 8.0 - 4.0);
   		const float vz = (float)(rng.NextDouble() * 8.0 - 4.0);
   		BotCore::NavFollowParams params;
   		params.ringMinM = (float)(rng.NextBelow(3u));
   		params.ringMaxM = params.ringMinM + (float)(rng.NextBelow(8u));
   		params.ringMaxTries = 1 + (int)rng.NextBelow(4u);
   		const float tx = grid.CellCenter(tc.x);
   		const float tz = grid.CellCenter(tc.z);

   		BotCore::NavFollower a;
   		BotCore::NavFollower b;
   		a.ObserveTarget(0, tx, tz);
   		b.ObserveTarget(0, tx, tz);
   		a.ObserveTarget(500, tx + vx * 0.5f, tz + vz * 0.5f);
   		b.ObserveTarget(500, tx + vx * 0.5f, tz + vz * 0.5f);
   		const float bx = grid.CellCenter(bc.x);
   		const float bz = grid.CellCenter(bc.z);
   		const bool ra = a.Update(grid, pf, 500, bx, bz, 4.5f, params);
   		const bool rb = b.UpdateReachable(grid, pf, 500, bx, bz, 4.5f, params, reach);
   		const BotCore::NavFollowPlan & pa = a.Plan();
   		const BotCore::NavFollowPlan & pb = b.Plan();
   		const bool same = ra == rb && pa.status == pb.status && pa.pathStatus == pb.pathStatus
   			&& pa.goal == pb.goal && pa.tries == pb.tries && pa.expanded == pb.expanded
   			&& pa.pathCost == pb.pathCost && pa.predX == pb.predX && pa.predZ == pb.predZ
   			&& pa.leadSec == pb.leadSec && pa.smooth.waypoints.size() == pb.smooth.waypoints.size()
   			&& pa.smooth.length == pb.smooth.length;
   		CHECK(same);
   		if (pa.status == BotCore::NavFollowStatus::Planned)
   			++planned;
   	}
   	CHECK(planned > 150);
   }
   ```

   Sınama notu: `oldF` (etiketsiz `Update`) kullanan CHECK'ler senaryonun gerçekten kusuru tetiklediğinin ön koşuludur; bir tanesi bozulursa senaryo geometrisini değil **önce uygulamanı** kontrol et (prototipte hepsi geçti).
4. **`Tests/BotCoreTests/NavBudgetTests.cpp`** yalnızca `NavBudget_Deferred_Chase_Sim` içinde üç düzenleme:
   1. `:471-476` (üç yorum satırı + `pinnedParams` + `grid.Build(pinnedParams);`) şu tek satıra iner: `grid.Build();   // production slope limit (ADR-0024, 0.45)`; hemen altındaki `REQUIRE(grid.MainComponentCells() == 88508);` kalır.
   2. `:716` `bs.follower.Update(grid, pathfinder, t, bs.x, bs.z, 4.5f, fp)` → `bs.follower.UpdateReachable(grid, pathfinder, t, bs.x, bs.z, 4.5f, fp, reach)` (`reach` zaten testte kurulu; `:1048`deki çağrı **dokunulmaz**).
   3. Hareket bloğunun sonunda, `if (!moved && !follow)` satırının (`:814`) hemen **önüne**:

      ```cpp
      				if (moved)
      					snapWalk(bs.x, bs.z);   // a smoothed chord may cross an edge-isolated pocket cell and the planner cannot leave one (KI); keep the follower on the main component like the target
      ```

      (`snapWalk` zaten ana bileşendeyse konumu değiştirmez.) Başka eşik, tohum, mantık değişmez.
5. Derle/test (§7): `./tools/run-tests.sh Release` ve `Debug`; `NavBudget_Deferred_Chase` filtresini Release'te **3 kez** koş ve `NAVBUDGET chase` satırlarını rapora yaz (§6 K3). Sunucu çalıştırılmaz. Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `./tools/build.sh Debug` rc=0; `NavTrack.h`, `NavTrackTests.cpp`, `NavBudgetTests.cpp` için yeni derleyici uyarısı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; dört yeni ad `[ OK ]`: `NavTrack_Reach_LeadInPocket`, `NavTrack_Reach_SkipPocketCandidates`, `NavTrack_Reach_ProvablyUnreachable`, `NavTrack_Reach_SingleComponent_Identical`; toplam test sayısı = başlangıç (§5 adım 1) + 4 (tazeleme sonrası beklenen: `289` + 4 = `293`; prototip `280` tabanlıydı); bu planın değiştirdiği `NavBudget_Deferred_Chase_Sim` dışında hiçbir mevcut testin gövdesi/eşiği değişmez (`NavTrack_Follower_Failures` (c), `NavTrack_Perf`, `NavReach_*`, `NavBudget_RealMap_Load` değişmeden geçer)
- [ ] K3: `NavBudget_Deferred_Chase_Sim` `[ OK ]` (Release **ve** Debug), 0,45'te: `NAVBUDGET chase` satırlarında mode B ve B2 için `follow_stale_ticks=0`, `without_plan_pct <= 3.0`; `dist_mean(B) <= 1.25 × dist_mean(A)` (prototip: Release 12,33 ≤ 13,58, Debug 12,59 ≤ 13,58; aynı A 10,86); Release'te 3 ardışık koşu aynı sonucu verir (sayaçlar ±birkaç tur oynayabilir, değişmezler değil). **Eşiği gevşetme, testi yeniden sabitleme yok:** değerler bu bantların dışındaysa uygulamayı §5 adım 2'ye göre yeniden kontrol et; yine tutmuyorsa **dur** ve ölçümleri `BLOKE` ile raporla
- [ ] K4: `grep -n "pinnedParams\|0\.625" Tests/BotCoreTests/NavBudgetTests.cpp` boş; `git diff Tests/BotCoreTests/NavBudgetTests.cpp` yalnızca §5 adım 4'teki üç düzenlemeyi içerir (`NavBudget_RealMap_Load`un `Update` çağrısı değişmemiş)
- [ ] K5: `git diff --stat gece/2026-10-02...bot/F5-71` yalnızca §4'teki 3 dosya + bu plan dosyası; `BotCore/NavReach.h|NavPath.h|NavSmooth.h|NavGrid.h|NavBudget.h|NavStuck.h`, `.vcxproj`, `GameServer/`, `AIServer/`, `shared/`, `docs/`, `tools/` farkı **0**; `git diff --check` boş; `git diff -U0 BotCore/NavTrack.h | grep -c '^-[^-]'` ≤ 12 (prototipte 9; `Update` bildirimi ve `NavFollowParams`/`NavFollowPlan` satırları değişmemiş)
- [ ] K6: saflık: `git diff -U0 BotCore/NavTrack.h | grep '^+' | grep -n -E "windows.h|stdafx|GameServer|shared/|malloc|std::chrono|#include"` boş; `python3 tools/check-perception-contract.py` rc=0
- [ ] K7: `Update` davranışı değişmedi kanıtı: `NavTrack_Reach_SingleComponent_Identical` (300 senaryo, `Update` ≡ `UpdateReachable`) ve tüm mevcut `NavTrack_*`/`NavReach_*`/`NavBudget_*` testleri geçer (K2)
- [ ] K8: dosya biçimi: `file BotCore/NavTrack.h Tests/BotCoreTests/NavTrackTests.cpp Tests/BotCoreTests/NavBudgetTests.cpp` çıktısı `ASCII text, with CRLF line terminators` (rapora yaz)
- [ ] K9: sunucu ikilisi değişmez: `git diff --stat gece/2026-10-02...bot/F5-71 -- GameServer shared AIServer` boş

Bu planda Claude çalışma zamanı kriteri **yoktur** (`NavFollower` sunucuda çağrılmaz; sunucu çalıştırılmaz).

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavTrack_Reach|NavBudget_Deferred|NAVBUDGET chase|tests,"
./tools/run-tests.sh Debug 2>&1 | grep -E "NavTrack_Reach|NavBudget_Deferred|NAVBUDGET chase|tests,"
for i in 1 2 3; do ./tools/run-tests.sh Release --no-build NavBudget_Deferred_Chase 2>&1 | grep -E "NAVBUDGET chase|tests,"; done
./build/bin/x86-Release/Tests/BotCoreTests.exe --list | wc -l
python3 tools/check-perception-contract.py
git diff -U0 BotCore/NavTrack.h | grep '^+' | grep -n -E "windows.h|stdafx|GameServer|shared/|malloc|std::chrono|#include"
grep -n "pinnedParams\|0\.625" Tests/BotCoreTests/NavBudgetTests.cpp
file BotCore/NavTrack.h Tests/BotCoreTests/NavTrackTests.cpp Tests/BotCoreTests/NavBudgetTests.cpp
git diff --stat gece/2026-10-02...bot/F5-71
git diff --check gece/2026-10-02...bot/F5-71
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3: CRLF, tab, Allman, yorumlar İngilizce, ASCII; test dışında konsol çıktısı yok. `BotCore/` başlık-yalnızca ve sunucusuzdur (ADR-0016): `NavTrack.h`'ye yeni `#include` eklenmez (şablon parametresi bu yüzden seçildi).
- **Gece dalı notu:** F5-62 (`NavDrive`) dala birleşmiştir; bu plan onun dosyalarına ve sembollerine dokunmaz (`NavDrive.h` `NavFollower`/`NavReach` kullanmaz; `NavNoReach`/`UpdateImpl`/`UpdateReachable` adları depoda başka yerde yoktur `[V]`). `Tests/BotCoreTests/*.vcxproj` bu planda **değişmez**; yeni `.cpp` eklemeyin (testler mevcut dosyalardadır).
- Beklenmedik durumda (imza farkı, `Update` davranışının değişmesi, K3 bantlarının tutmaması, ek dosya gerekmesi) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla; **eşik gevşetme ve yeniden sabitleme yasaktır**.
- **Dürüstlük:** bu plan KI-023'ün **birinci** nedenini üretim kodunda düzeltir; **ikinci** neden (düzleştirilmiş kirişin bir cep hücresinde durdurduğu botun planlayıcıdan çıkamaması) çözülmez, yalnızca test simülasyonunda botu ana bileşende tutarak izole edilir ve `NavTrack_Reach_ProvablyUnreachable` (b) ile davranış olarak kayda geçer. `NavFollower` sunucuda çağrılmadığından gerçek oyunda etki yoktur; ilk çağıran F5-63'tür.
- **Doğrulayıcı (Claude) notu, `/plan-dogrula`'da yapılacaklar:** (1) KI-023 `KAPANDI`; (2) yeni KI: "düzleştirilmiş kiriş botu eğim cebi hücresinde durdurabilir, planlayıcı cepten çıkaramaz (`NavTrack_Reach_ProvablyUnreachable` (b), KI-023 ikinci neden)", önerilen çözüm F5-63'te cep kurtarma ya da `NavSmooth` kirişinin ana bileşende kalması (KI numarası, gece dalındaki diğer planlarla çakışmasın diye yazım turunda **açılmadı**); (3) `docs/12` §4.2'ye "öngörü noktası botun bileşeninde değilse de süre yarıya iner (`UpdateReachable`)" cümlesi ve §4.3'e "halkada botun bileşeni dışındaki hücreler A* çalıştırılmadan atlanır"; (4) F5-63 taslağındaki `NavTrack.h` eklerinin (`ReplanDue`/`InvalidatePlan`/`field`) bu planın `UpdateImpl`'ine göre uyarlanması.

## Tazeleme (2026-10-03, ön-plan tazeleme, otonom döngü)

- Taban `271e17e` → `26f10c4` (`gece/2026-10-02`). Arada değişen BotCore/Tests dosyaları: `BotCore/NavDrive.h`, `Tests/BotCoreTests/NavDriveTests.cpp` (F5-62, yeni) ve iki `.vcxproj` (F5-62'nin birer satırı). Bu planın üç dosyası (`NavTrack.h`, `NavTrackTests.cpp`, `NavBudgetTests.cpp`) `271e17e`den beri **değişmedi** `[V: git diff]`.
- Yeniden doğrulanan referanslar (hepsi aynı satırda): `NavTrack.h` `class NavFollower` `:169`, `Update` `:180`/`:331`, `private:` `:188`, öngörü kabulü `:385`, `botCell` `:396-398`, `limit` `:422`, döngü `:425`, `m_candidates[static_cast<size_t>(i)]` `:427`/`:434`, `NavFollowPlan` `:150-167`, `ringMaxTries` `:136`; `NavReach.h` `#include "NavTrack.h"` `:12`, `ComponentOf` `:37`, `Judge` `:218`; `NavTrackTests.cpp` yardımcıları `:19-80`, `NavTrack_Follower_Failures` `:694` (c `:727`), 1823 satır, son test `NavTrack_Chase_Sim_Cadence`; `NavBudgetTests.cpp` sabitleme `:471-477`, `snapWalk` `:613`, `Update` çağrıları `:716` ve `:1048`, `if (!moved && !follow)` `:814`, `NavBudget_Deferred_Chase_Sim` `:463`, `NavBudget_RealMap_Load` `:901`; testte `reach`/`mainComp` zaten kurulu (`:480-482`) ve `NavReach.h` zaten include'lu (`:7`).
- Düzeltilen: (1) F5-62 artık birleşmiş ("paralel uygulanır" ifadeleri güncellendi); (2) tazeleme sonrası test tabanı `289` (F5-62: 276 + 13), kabul K2 beklenen toplamı `293`; prototip ölçümleri (`280 tests`) eski tabana aittir, değişmezler aynı kalır. Çelişen adım/kabul kriteri yok, F5-62'nin yeni kodu bu planın sembolleriyle çakışmaz. Durum: `HAZIR` kalır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-71` — `0c732c4 [F5-71] NavFollower UpdateReachable: bilesen farkindalikli planlama, KI-023 birinci neden` (+ bu rapor commit'i)
- Başlangıç / bitiş test sayısı (`--list | wc -l`): `289` → `293` (dört yeni ad)
- Değişen dosyalar ve neden:
  - `BotCore/NavTrack.h`: `NavFollower::UpdateReachable` (şablon, `Reach` parametresi), özel `UpdateImpl<Reach>(..., const Reach * reach)`, `NavNoReach` yer tutucu; `Update` ince sarmalayıcıya indi; gövdede `botCell` öne taşındı + `botComp`, öngörü kabul koşulu bileşen denetimli, `limit` kaldırıldı, aday döngüsü `tries < maxTries` ile tüm adaylar üzerinde ve bileşen dışı adaylar A* koşmadan atlanıyor. Yeni `#include` yok.
  - `Tests/BotCoreTests/NavTrackTests.cpp`: `#include <BotCore/NavReach.h>` + dosya sonuna `MakePocketGrid()` ve 4 test (`NavTrack_Reach_LeadInPocket`, `NavTrack_Reach_SkipPocketCandidates`, `NavTrack_Reach_ProvablyUnreachable`, `NavTrack_Reach_SingleComponent_Identical`).
  - `Tests/BotCoreTests/NavBudgetTests.cpp`: yalnızca `NavBudget_Deferred_Chase_Sim` (0,625 sabitleme kaldırıldı → `grid.Build()`; `Update` → `UpdateReachable(..., reach)`; hareket bloğu sonuna `if (moved) snapWalk(...)`).
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
  proj-LogInServer.vcxproj -> ...\LogInServer.exe
  proj-GameServer.vcxproj -> ...\GameServer.exe
  proj-AIServer.vcxproj -> ...\AIServer.exe
    NavTrackTests.cpp
    Kod Üretiliyor...
    BotCoreTests.vcxproj -> ...\Tests\BotCoreTests.exe
  ```
  (`Release` rc=0, `Debug` rc=0; değişen dosyalarda yeni uyarı yok.)
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔: `build.sh Release` ve `Debug` rc=0; değişen dosyalarda yeni uyarı yok.
  - K2 ✔: `Release` ve `Debug` `293 tests, 0 failed`; dört yeni ad `[ OK ]`; toplam = 289 + 4; `NavTrack_Follower_Failures` (c), `NavTrack_Perf`, `NavReach_*`, `NavBudget_RealMap_Load` değişmeden geçti.
  - K3 ✔: `NavBudget_Deferred_Chase_Sim` `[ OK ]` iki yapılandırmada; mode B/B2 `follow_stale_ticks=0`, `without_plan_pct` 0,1/0,3 (Release) ve 0,4/0,5 (Debug) ≤ 3,0; `dist_mean(B)` 12,33 ≤ 1,25 × 10,86 = 13,58; Release 3 ardışık koşu aynı.
  - K4 ✔: `grep -n "pinnedParams\|0\.625" Tests/BotCoreTests/NavBudgetTests.cpp` boş; `git diff` yalnızca üç düzenleme, `NavBudget_RealMap_Load`un `Update` çağrısı değişmedi.
  - K5 ✔: `git diff --stat gece/2026-10-02...bot/F5-71` yalnızca §4'teki 3 dosya (+ bu plan); `NavReach.h|NavPath.h|NavSmooth.h|NavGrid.h|NavBudget.h|NavStuck.h`, `.vcxproj`, `GameServer/`, `AIServer/`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş; `NavTrack.h` silinen satır = 9 ≤ 12.
  - K6 ✔: `NavTrack.h` eklenen satırlarda yasak `#include`/sembol yok; `check-perception-contract.py` rc=0 (RESULT: PASS, R1-R5 0).
  - K7 ✔: `NavTrack_Reach_SingleComponent_Identical` 300 senaryo `Update ≡ UpdateReachable`; tüm mevcut nav testleri geçti.
  - K8 ✔: üç dosya `ASCII text, with CRLF line terminators`.
  - K9 ✔: `git diff --stat gece/2026-10-02...bot/F5-71 -- GameServer shared AIServer` boş.
- `NAVBUDGET chase` satırları (Release 3 koşu + Debug):
  - Release (3 koşu birebir aynı): A `dist_mean=10.86`; B `without_plan_pct=0.1 deferred_ticks=21 hold_ticks=21 follow_stale_ticks=0 dist_mean=12.33`; B2 `without_plan_pct=0.3 deferred_ticks=7272 hold_ticks=55 follow_stale_ticks=0 dist_mean=16.70`.
  - Debug: A `dist_mean=10.86`; B `without_plan_pct=0.4 deferred_ticks=119 hold_ticks=80 plan_wait_max=900 follow_stale_ticks=0 dist_mean=12.59`; B2 `without_plan_pct=0.5 deferred_ticks=8665 hold_ticks=90 follow_stale_ticks=0 dist_mean=16.35`.
- Plandan sapmalar ve gerekçeleri: yok; §5 adımları ve §2'deki dört gövde değişikliği aynen uygulandı. `ringMaxTries` "A* koşusu" anlamı yeni `UpdateReachable` yorumunda belgelendi (K5 gereği `NavFollowParams` satırına dokunulmadı).
- Açık sorular: yok. (Not: §2 Neden 2 — düzleştirilmiş kirişin botu eğim cebi hücresinde durdurabilmesi — bu planda çözülmedi; `NavTrack_Reach_ProvablyUnreachable` (b) ile davranış olarak kayda geçti, doğrulayıcıya KI açması önerilir.)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F5-71` @ `42e96c6` (kod commit'i `0c732c4`; gece modu, birleştirmeyi döngü betiği yapar)
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `build.sh Release` ve `Debug` rc=0; iki günlükte `warning` sayısı 0 |
| K2 | ✔ | Release ve Debug `293 tests, 0 failed`; dört `NavTrack_Reach_*` adı `[ OK ]`; `--list` = 293 (289 + 4); diff'te yalnızca `NavBudget_Deferred_Chase_Sim` gövdesi mevcut testlerden değişti |
| K3 | ✔ | Release B: `follow_stale_ticks=0 without_plan_pct=0.1 dist_mean=12.33` (A 10.86 → eşik 13.58); B2: `follow_stale_ticks=0 without_plan_pct=0.3`. Debug B: `0 / 0.4 / 12.59`; B2: `0 / 0.5`. Release 3 ardışık koşu birebir aynı satırlar. Eşik gevşetilmemiş |
| K4 | ✔ | `grep -n "pinnedParams\|0\.625" NavBudgetTests.cpp` boş; diff yalnızca 3 düzenleme (`grid.Build()`, `UpdateReachable(..., reach)`, `if (moved) snapWalk`); `NavBudget_RealMap_Load` `Update` çağrısı değişmemiş |
| K5 | ✔ | `git diff --stat`: `NavTrack.h`, `NavTrackTests.cpp`, `NavBudgetTests.cpp` + plan; `docs/ tools/ GameServer/ AIServer/ shared/ *.vcxproj` farkı 0; `git diff --check` boş; `NavTrack.h` silinen satır 9 ≤ 12 |
| K6 | ✔ | eklenen satırlarda yasak desen yok (grep boş); `check-perception-contract.py` rc=0 |
| K7 | ✔ | `NavTrack_Reach_SingleComponent_Identical` (300 senaryo) ve tüm mevcut nav testleri geçti; `Update` bildirimi aynı, gövde farkı §2'deki dört değişiklik (`NavTrack.h` diff'i satır satır okundu) |
| K8 | ✔ | `file`: üç dosya `ASCII text, with CRLF line terminators` |
| K9 | ✔ | `GameServer shared AIServer` farkı boş |

- Bulgular (önem sırasıyla):
  1. (not) Kod planın §5 adım 2'siyle birebir: `botCell` öne taşındı (`NavTrack.h:389-393`), öngörü kabulü bileşen denetimli (`:419-420`), `limit` kalktı ve döngü `tries < maxTries` (`:454`), bileşen dışı aday A* koşmadan atlanıyor (`:456-457`). `reach` yalnızca `botComp >= 0` iken dereferans edilir; `Update` yolunda `nullptr` ve `botComp = -1` olduğundan `NavNoReach` hiç çağrılmaz. Sorun yok.
  2. (not) Bilinen sınırlama: bot eğim cebi hücresindeyken planlayıcı çıkamaz (KI-023 ikinci neden). KI-024 olarak açıldı; F5-63'te çözülecek. Test simülasyonu botu `snapWalk` ile ana bileşende tutar (plan gereği).
  3. (not) Beklenen kayıtlar yapıldı: KI-023 `KAPANDI`, KI-024 açıldı, `docs/12` §4.2 öngörü/halka cümleleri eklendi. F5-63 taslağındaki `NavTrack.h` eklerinin (`ReplanDue/InvalidatePlan/field`) bu planın `UpdateImpl`'ine göre uyarlanması F5-63 HAZIR yapılırken yapılacak (STATUS'ta kayıtlı).
- Düzeltme talimatı: yok.

