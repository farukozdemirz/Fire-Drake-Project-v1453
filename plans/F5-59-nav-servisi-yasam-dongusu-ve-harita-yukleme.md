# F5-59: `NavService`: sunucuda navigasyon ızgarasının yaşam döngüsü ve harita yükleme (`[BOT] NAV=1`, SMD belleğinden, `BotCore/NavFingerprint.h`)

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-59 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F5-01 (`NavGrid`, `KAPANDI`). Şemsiye: F5-55 (dilim 1). Başka plana bağımlı değil; F5-61..F5-66 bu planın `NavService`'ine dayanır |
| İlgili gereksinim / kabul | `docs/12` §2 ("GameServer başlangıcında zone 71 için bir kez hesaplanır") ve §13.6 (G5); ADR-0017 madde 3 (başlık-yalnızca / ilk `.cpp` modülü ertelemesi); F5-55 §2 sözleşme ("nav yalnızca başlangıçta bir kez kendi ızgarasına aktarır") ve §4 açık sorusu "ızgara dosyasının üretimi/yeri"; `tools/check-perception-contract.py` R1-R5 PASS kalmalı |
| Tahmini büyüklük | M (10 dosya; 4'ü yeni; kod az, doğrulama çalışma zamanında) |
| Hazırlayan / tarih | Claude / 2026-10-03 |

---

## 1. Amaç

`[BOT] NAV=1` (varsayılan **0**) iken GameServer açılışta zone 71 haritasını **kendi belleğindeki SMD verisinden** (olay + yükseklik ızgarası) bir `BotCore::NavGrid`'e bir kez aktarır, `Build()` eder ve `NavService` üzerinden "hazır mı" / `const NavGrid*` sorgusu sunar; kapanışta hazır bayrağını düşürür. Hiçbir bot davranışı değişmez (hareket icrasına dokunulmaz); sonraki dilimler (F5-61..F5-66) bu ızgarayı kullanır. Aktarımın doğruluğu, ızgaranın `tools/nav-export.py` dosyasıyla **bayt düzeyinde aynı** olduğunu gösteren bir CRC32 parmak iziyle (`BotCore/NavFingerprint.h`) kanıtlanır.

## 2. Bağlam (okunması zorunlu)

Aşağıdaki tüm satır numaraları `gece/2026-10-02` @ `c2c5a08` üzerinde Claude tarafından yeniden doğrulandı. Uygulayıcı yine de her satırı kendi çalışma ağacında açıp doğrulasın; kayma varsa **dur** ve rapora yaz (`AGENTS.md` §7).

- `docs/12_NAVIGATION_AND_POSITIONING.md` §1 (harita 513 × 513, 4 m, olay ızgarası `x·513 + z`), §2 (veri katmanları; "GameServer başlangıcında zone 71 için bir kez hesaplanır", `C3DMap`/`SMDFile` ilişkisi), §13.6 (G5: saf mantık yetmez), `docs/13` §4.4 (`[BOT]` ini tablosu; `NAV` satırını Claude ekler).
- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` madde 3 (satır 12): `GameServer` `BotCore.lib`'e bağlanmaz, göreli `#include`; "ilk `.cpp` içeren modül geldiğinde `ProjectReference` eklenir".
- **Bağlama bulgusu (Claude, bu planın temel kararı).** `BotCore/*.h` içindeki **tüm** `Nav*.h` başlık-yalnızcadır: `BotCore/BotCore.vcxproj:69` yalnızca `Rng.cpp`'yi derler (tek `ClCompile`), hiçbir `Nav*.h` `Rng.h` içermez (`git grep -n -a "Rng" gece/2026-10-02 -- 'BotCore/Nav*.h'` boş) ve `BotCoreTests` 14 test dosyasında (14 ayrı çeviri birimi) aynı `Nav*.h` başlıklarını dahil edip hatasız bağlanır (tanım çiftlenmesi yok `[V: dolaylı]`). Yani `NavService.cpp` yalnızca `#include "../../BotCore/NavGrid.h"` ile çalışır; **`GameServer` için `BotCore.lib` bağlantısı, `ProjectReference`, `.sln` bağımlılığı gerekmez** ve ADR-0017 madde 3 aynen geçerli kalır (ilk `Rng.cpp` tüketicisi gelince eklenir; bu plan `Rng` kullanmaz). `KnightOnlineServer.sln:26` `BotCore` projesi zaten çözümde ve `BotCoreTests` ona bağlı (`Tests/BotCoreTests/BotCoreTests.vcxproj:104-106`); GameServer'a dokunulmaz. Not: `BotCore.vcxproj` `v142`, `tools/build.sh` `/p:PlatformToolset=v143` ile ezer (`tools/build.sh` son satır); bu planı etkilemez.
- Izgara girdisi: `BotCore/NavGrid.h:30` `Init(int n, float unit, std::vector<int16_t> events, std::vector<float> heights)` (diziler **sahiplenilir**; `n < 2`, `unit <= 0` veya boyut ≠ n·n ise `false`), `:88-103` `Load` (dosya biçimi: 8 bayt `"FDPNAV01"`, `int32 n`, `float32 unit`, `int16 events[n*n]`, `float32 heights[n*n]`, boyut = 16 + 6·n·n; indeks `x*n + z`), `:38` `Build()` (`Walk` = olay 1 ve haritanın kenarına değmeyen en büyük 4-bağlantılı bileşen + `clearance`), `:52` `MainComponentCells()`, `:46-47` `Event/Height(x, z)`.
- Gerçek zone 71 testlerde/araçlarda **önceden üretilmiş dosyadan** yüklenir: `Tests/BotCoreTests/NavArenaTests.cpp:320-329` (`grid.LoadFile("build/nav/zone71.navgrid")`, dosya yoksa test `SKIPPED` basıp **geçer**; `REQUIRE(grid.MainComponentCells() == 88508)`), `tools/nav-measure/nav_measure.cpp:1089` (`--navgrid`, varsayılan `build/nav/zone71.navgrid`) ve `tools/nav-export.py:5-12, 52` (biçim; SMD → dosya; `:131-139` özet satırı `crc32=` = `zlib.crc32` tüm dosya baytları). `build/` `.gitignore`'dadır (`build*/`), yani dosya depoda yoktur ve dağıtım gerektirir. Bu worktree'de `build/nav/` **yoktur**: uygulayıcı §5 adım 1'de `python3 tools/nav-export.py` ile üretmelidir, aksi halde gerçek-harita testi sessizce `SKIPPED` olur.
- **Sunucuda harita verisi zaten bellekte:** `GameServer/GameServerDlg.cpp:150` `MapFileLoad()` (`GameServer/LoadServerData.cpp:381-405`) her `ZONE_INFO` satırı için `C3DMap::Initialize` çağırır (`GameServer/Map.cpp:19-38`: `m_smdFile = SMDFile::Load(pZone->m_MapName, true)`); `GameServerDlg::GetZoneByID(int)` (`GameServer/GameServerDlg.cpp:449-451`) bölgeyi döndürür; `C3DMap::m_smdFile` herkese açık (`GameServer/Map.h:43`); `ZONE_RONARK_LAND = 71` (`GameServer/Define.h:140`). `SMDFile` olay ve yükseklik dizilerini **sunucu ömrü boyunca tutar ve bir daha yazmaz**: `shared/SMDFile.cpp:104-113` (`LoadTerrain`: `m_fHeight = new float[n*n]`), `:129-134` (`LoadMapTile`: `m_ppnEvent = new short[n*n]`), `:205` (`GetEventID`: indeks `x * m_nMapSize + z`, yani NavGrid ile aynı yerleşim), yok edici `:208-223`. Erişimciler: `shared/SMDFile.h:32` `GetMapSize()` **`m_nMapSize - 1`** döndürür (zone 71: 512; dizi kenarı n = `GetMapSize() + 1` = 513), `:33` `GetUnitDistance()`, `:37` `GetEventIDs()` (`short *`); **yükseklik için erişimci yoktur** (`:59` `m_fHeight` özel). Python aracı da aynı SMD'yi okur (`docs/appendix/tools/smd_parse.py`: olay indeksi `x * m_nMapSize + z`), yani iki yol aynı bayt dizisini üretmelidir; bu planın parmak izi bunu **sınar**.
- **Başlangıç sırası (kritik):** `BotManager::Startup()` `GameServer/GameServerDlg.cpp:105`'te, `MapFileLoad()` ise `:150`'de çağrılır: yani **bot başlatması harita yüklenmeden önce çalışır**; `NavService::Startup()` bu yüzden `BotManager::Startup()` içine konamaz. `Logs` klasörü `:170`'te oluşturulur (`:199` `// Logs End`, `:201` `LoadNoticeData();`), `RunServer()` `:213`, `StartTicking()` `:214`. Kapanış: `GameServer/GameServerDlg.cpp:3130-3132` yıkıcıda `BotManager::Instance().Shutdown()`.
- Desen kaynakları: `GameServer/Bot/BotManager.cpp:41-56` (`WriteBotLog`: `./Logs/Bot_<gün>_<ay>_<yıl>.log`, açılamazsa sessiz), `:161-166` (`CIni ini(CONF_GAME_SERVER); ini.GetBool("BOT", "ENABLED", false)`; kapalıysa `return true`), `:303-310` (tek durum satırı: `printf` + `WriteBotLog`), `:346-357` (`Shutdown`). `shared/Ini.h:27-29` (`GetInt/GetBool/GetString`).
- Min/max makroları: `shared/stdafx.h:71-77` `min`/`max`'i `#undef` eder; `BotCore` başlıkları GameServer'da zaten dahil ediliyor (`GameServer/Bot/ActionExecutor.cpp:8-9`, `BotSession.h:9-10`), `NavGrid.h` ilk kez dahil edilecek: derleme hatası/uyarısı çıkarsa **başlığı değiştirme**, rapora yaz.
- Sözleşme denetimi: `tools/check-perception-contract.py:27` `SCAN_DIRS = ("GameServer/Bot", "BotCore")`; R4: `BotCore` dosyaları yalnızca standart ve kardeş başlık içerir (`NavFingerprint.h` buna uyar); R1/R2 simge listeleri `GetZoneByID`, `GetEventIDs`, `GetHeights`, `m_smdFile` içermez, ama bu erişim **çalışma zamanında sunucu dizilerine erişim değil, başlangıçta bir kez aktarımdır** (F5-55 §2). Betik değişmez (Claude doğrulamada, gerekirse, `NavService::Startup` için dar bir izin satırı ekler).
- `GameServer/Bot/ScenarioRunner.cpp:318`: senaryolar yalnızca zone 71'i destekler; `NavService` da yalnızca zone 71'i yükler.

### Tasarım kararı: ızgara kaynağı (F5-55 §4 açık sorusunun yanıtı)

**Karar: açılışta, sunucunun bellekteki SMD verisinden; önceden üretilmiş dosya yok.** Gerekçe:

| Seçenek | Artı | Eksi | Sonuç |
|---|---|---|---|
| **A. Bellekteki `SMDFile` dizilerinden kopya (seçildi)** | Sunucunun gerçekten kullandığı harita; dağıtım adımı yok; dosya bayatlaması yok; `docs/12` §2 ve F5-55 §2 sözleşmesine birebir ("`Map`/`SMD` verisini yalnızca başlangıçta bir kez kendi ızgarasına aktarır"); 1,6 MB kopya + `Build` açılışta bir kez | `SMDFile`'a 1 satırlık yükseklik erişimcisi gerekir (`shared/SMDFile.h`); doğruluğu ayrıca kanıtlamak gerekir (parmak izi) | Seçildi |
| B. `./Nav/zone71.navgrid` dosyasından yükleme | `NavGrid::LoadFile` hazır | Dosyayı üreten araç (`nav-export.py`, Python) bir dağıtım adımı olur; SMD değişirse dosya sessizce bayatlar (sunucu ile nav farklı haritayı görür: duvardan geçme riski, CLI-08'in tam tersi); `build/` git'te yok | Reddedildi |
| C. Önce dosya, yoksa SMD | İki yolu da kapsar | İki kod yolu, hangisinin aktif olduğu belirsiz | Reddedildi (karmaşıklık) |

`.navgrid` dosyası **yalnızca araç/test dünyasında** kalır (`tools/nav-*`, `BotCoreTests`); sunucu yolu onu **okumaz**. İki dünyanın aynı ızgarayı gördüğü `NavGridFingerprint` ile kanıtlanır: sunucu günlüğündeki `crc32=` değeri, aynı SMD'den üretilen `.navgrid` dosyasının CRC32'sine (`python3 tools/nav-export.py` çıktısındaki `crc32=`) **eşit** olmalıdır. Karar ADR-0006'ya "Ek F5-59" olarak Claude tarafından işlenir.

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavFingerprint.h` (yeni, saf mantık, yalnızca standart kütüphane ve `NavGrid.h`; global/static yok): `NavCrc32` (zlib uyumlu CRC32, tablo üye dizisi) ve `NavGridFingerprint(const NavGrid &)`: `.navgrid` dosya düzeniyle (başlık 16 bayt + olaylar + yükseklikler) hesaplanan CRC32.
2. `shared/SMDFile.h`: tek satır `INLINE float * GetHeights() { return m_fHeight; }` (`GetEventIDs` yanına). Başka değişiklik yok.
3. `GameServer/Bot/NavService.{h,cpp}` (yeni): tekil sınıf; `Startup()` (`[BOT] ENABLED=1` **ve** `[BOT] NAV=1` ise zone 71 ızgarasını kurar), `Shutdown()`, `Ready()`, `Grid()`, `Info()`. Yalnızca **durumu tutar**; hiçbir bot kodundan çağrılmaz (sonraki dilimler).
4. `GameServerDlg.cpp`'ye iki çağrı (`Startup()` ve yıkıcı).
5. Birim testleri `Tests/BotCoreTests/NavFingerprintTests.cpp` (yeni).
6. Çalışma zamanı doğrulaması (Claude koşar; §6 K9-K12): `NAV=1` günlük satırı, parmak izi eşitliği, `NAV=0`/`ENABLED=0` davranışsızlığı.

**Kapsam dışı (yapılmayacak)**

- Hareket icrası, `ActionExecutor`/`BotManager`/`BotSession` değişikliği, kiriş guard'ı (F5-61), `/bot goto` (F5-62), `NavPathfinder`/`NavReach`/maliyet katmanı kurulumu (F5-62: paylaşım kararı F5-53 ölçüm notuyla orada), telemetri (`NAV_*`, F5-64), bot durumu temizliği (F5-65).
- `NavService`'in `BotManager` durum satırına/`/bot list`'e eklenmesi, `[BOT] NAV_*` başka anahtarlar.
- Zone 71 dışındaki haritalar; `.navgrid` dosyasını sunucuda okumak.
- `BotCore.lib` bağlantısı / `ProjectReference` (§2: gerekmez); `BotCore/NavGrid.h` ve diğer mevcut `Nav*.h` değişmez.
- `docs/` (Claude: `docs/13` §4.4 `NAV` satırı, `docs/12` §2, ADR-0006 Ek F5-59, `docs/STATUS.md`), `tools/` değişikliği.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavFingerprint.h` | yeni | yalnızca ASCII + CRLF; yalnızca standart kütüphane + `NavGrid.h` |
| `BotCore/BotCore.vcxproj` | değiştir | tek satır: `<ClInclude Include="NavFingerprint.h" />` (diğer başlıkların yanına; BOM ve satır sonu korunur) |
| `Tests/BotCoreTests/NavFingerprintTests.cpp` | yeni | |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | tek satır: `<ClCompile Include="NavFingerprintTests.cpp" />` |
| `GameServer/Bot/NavService.h` | yeni | |
| `GameServer/Bot/NavService.cpp` | yeni | |
| `GameServer/proj-GameServer.vcxproj` | değiştir | `ClCompile Include="Bot\NavService.cpp"` (`:199` civarı, diğer `Bot\` satırlarının yanına) ve `ClInclude Include="Bot\NavService.h"` (`:295` civarı) |
| `GameServer/proj-GameServer.vcxproj.filters` | değiştir | aynı iki dosya için `Source Files` / `Header Files` girdileri (mevcut `Bot\...` girdileri `:84-101` ve `:215-235` gibi) |
| `GameServer/GameServerDlg.cpp` | değiştir | `#include "Bot/NavService.h"` (`:19`'un yanına), `LoadNoticeData();` (`:201`) sonrasına `Startup()` çağrısı, yıkıcıda (`:3132`) `BotManager::Shutdown()` sonrasına `Shutdown()` çağrısı; dosya UTF-8 BOM'lu: kodlama bozulmaz |
| `shared/SMDFile.h` | değiştir | yalnızca 1 satır ekleme (`GetHeights`) |

`.vcxproj`/`.filters` dosyalarında satır sonu ve BOM mevcut haliyle korunur. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-59 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. `./tools/run-servers.sh status` çıktısında `[UP]` varsa `./tools/run-servers.sh stop`. Gerçek-harita testinin atlanmaması için ızgara dosyasını üret: `python3 tools/nav-export.py` (varsayılan `build/nav/zone71.navgrid`; çıktı satırı `NAVGRID ... main_component=88508 crc32=<hex>`; **crc32 değerini rapora yaz**).
2. **`BotCore/NavFingerprint.h`** (iskelet; ASCII, CRLF, Allman, tab, yorumlar İngilizce):

   ```cpp
   #pragma once
   // Fingerprint of a NavGrid in the .navgrid file layout (F5-59): CRC32 (zlib / IEEE 802.3) over
   //   "FDPNAV01", int32 n, float32 unit, int16 events[n*n], float32 heights[n*n]  (little-endian host).
   // Lets the server prove that a grid copied from its in-memory SMD data equals the exported file.
   #include "NavGrid.h"
   #include <array>
   #include <cstddef>
   #include <cstdint>
   namespace BotCore
   {
   	class NavCrc32
   	{
   	public:
   		NavCrc32();                                   // builds the 256-entry table (member array, no statics)
   		void Update(const void * data, size_t size);  // streaming
   		uint32_t Value() const;                       // final xor applied; Value() may be called repeatedly
   	private:
   		std::array<uint32_t, 256> m_table;
   		uint32_t m_state;                             // starts at 0xFFFFFFFF
   	};
   	// Events then heights in x-major order (index x * n + z), exactly the file order.
   	inline uint32_t NavGridFingerprint(const NavGrid & grid);   // 0 when grid.Size() < 2
   }
   ```

   `NavGridFingerprint` önce 8 bayt sihirli sözcüğü, `int32 n` ve `float32 unit` değerlerini (bellekteki ham baytları, `memcpy` ile), sonra tüm `Event(x, z)` (`int16_t`) değerlerini `x` dış, `z` iç döngüyle, sonra tüm `Height(x, z)` (`float`) değerlerini aynı sırayla besler. Standart: `NavCrc32` `"123456789"` için `0xCBF43926` vermelidir (zlib/Python `zlib.crc32` ile aynı polinom `0xEDB88320`, başlangıç/son `0xFFFFFFFF`). Tüm üye fonksiyonlar `inline` (başlık-yalnızca; `.cpp` yok).
3. **`BotCore/BotCore.vcxproj`** ve **`Tests/BotCoreTests/BotCoreTests.vcxproj`** tek satır eklemeleri.
4. **`Tests/BotCoreTests/NavFingerprintTests.cpp`**, `MiniTest.h` kalıbı (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`; bkz. `NavGridTests.cpp`); dört test, adlar sabit:
   - `NavFingerprint_Crc32_KnownVectors`: `""` → `0x00000000`, `"123456789"` → `0xCBF43926`, `"a"` → `0xE8B7BE43`; parçalı `Update` (bölünmüş girdi) tek çağrıyla aynı; `Value()` iki kez çağrılınca aynı.
   - `NavFingerprint_InitEqualsLoad`: sentetik n = 9, unit = 4,0; olaylar/yükseklikler `Rng` sabit tohumla; aynı veriden (a) `Init(n, unit, events, heights)` ve (b) elle kurulan `FDPNAV01` bayt tamponu → `NavGrid::Load`: iki ızgaranın parmak izi **eşit** ve tamponun `NavCrc32` değerine eşit (bu, sunucu yolunun `Init` ile dosya yolunun `Load` ile aynı baytları gördüğünü sınar). `Build()` sonrası parmak izi **değişmez** (`Build` olay/yükseklik dizilerini yazmaz).
   - `NavFingerprint_Sensitivity`: tek bir olay değeri, tek bir yükseklik bitini, `n` veya `unit` değerini değiştirmek parmak izini değiştirir (4 durum ayrı CHECK); `Size() < 2` (boş ızgara) → `0`.
   - `NavFingerprint_RealMap_MatchesFile`: `build/nav/zone71.navgrid` yoksa `NAVFP real map: SKIPPED (...)` basıp dön (diğer gerçek-harita testleriyle aynı kalıp, `NavArenaTests.cpp:320-329`); varsa dosya baytlarının `NavCrc32` değeri == `LoadFile` sonrası `NavGridFingerprint`; `Build()` sonrası `MainComponentCells() == 88508`; satır: `NAVFP real map: n=513 cells=263169 main=88508 crc32=%08x match=1`.
5. **`shared/SMDFile.h`**: `INLINE short * GetEventIDs()` satırının (`:37`) altına `INLINE float * GetHeights() { return m_fHeight; }`.
6. **`GameServer/Bot/NavService.h`** (iskelet):

   ```cpp
   #pragma once
   #include <atomic>
   #include <string>
   #include "../../BotCore/NavGrid.h"

   // Owns the navigation grid of zone 71 (F5-59; docs/12 s2). Built once at start-up from the SMD data the
   // server already loaded, never rebuilt, never written afterwards: Grid() is safe to read from any thread
   // while Ready() is true. NavService holds navigation STATE only; it never moves, plans or sends anything.
   class NavService
   {
   public:
   	struct Info
   	{
   		int      zone = 0;
   		int      n = 0;              // vertices per side (513)
   		float    unit = 0.0f;        // metres per cell (4.0)
   		int      mainCells = 0;      // NavGrid::MainComponentCells()
   		uint32_t crc32 = 0;          // BotCore::NavGridFingerprint
   		double   copyMs = 0.0;
   		double   buildMs = 0.0;
   	};

   	static NavService & Instance();

   	// Called once from CGameServerDlg::Startup() after MapFileLoad() and before RunServer(), on the main
   	// thread. [BOT] ENABLED=0 or NAV=0 (default): returns true, does nothing, prints nothing. NAV=1: builds
   	// the grid and writes ONE line (printf + ./Logs/Bot_*.log). A failure is logged ("nav FAILED (<reason>)"),
   	// leaves Ready() false and still returns true: the server must start; bots just have no navigation.
   	bool Startup();
   	// Called from ~CGameServerDlg() after BotManager::Shutdown(): clears the ready flag (the grid memory is
   	// released by the singleton destructor at process exit; no tick can run by then).
   	void Shutdown();

   	bool Enabled() const { return m_enabled; }      // NAV=1 was requested (and the bot system is on)
   	bool Ready() const { return m_ready.load(std::memory_order_acquire); }
   	const BotCore::NavGrid * Grid() const { return Ready() ? &m_grid : nullptr; }
   	const Info & GetInfo() const { return m_info; }

   private:
   	NavService() : m_enabled(false), m_ready(false) {}
   	bool m_enabled;
   	std::atomic<bool> m_ready;
   	BotCore::NavGrid m_grid;
   	Info m_info;
   };
   ```

7. **`GameServer/Bot/NavService.cpp`**: `#include "stdafx.h"` ilk; sonra `NavService.h`, `BotManager.h` (`isEnabled()` için), `<set>` ve `../../shared/SMDFile.h` (`Map.cpp:1-4` ile aynı sıra), `../../BotCore/NavFingerprint.h`, `../../shared/Ini.h`, `<chrono> <cstdio> <ctime> <vector>`. İçerik:
   - Yerel `static void WriteNavLog(const char * line)`: `BotManager.cpp:41-56` `WriteBotLog` ile aynı mantık (aynı dosya; kopya kabul).
   - `Startup()` akışı: (a) `m_enabled = m_ready = false`; (b) `!BotManager::Instance().isEnabled()` → `return true`; (c) `CIni ini(CONF_GAME_SERVER); if (!ini.GetBool("BOT", "NAV", false)) return true;` `m_enabled = true`; (d) `C3DMap * map = g_pMain->GetZoneByID(ZONE_RONARK_LAND)`; `map == nullptr || map->m_smdFile == nullptr` → hata `no_zone`; (e) `SMDFile * smd = map->m_smdFile; const int n = smd->GetMapSize() + 1; const float unit = smd->GetUnitDistance(); short * ev = smd->GetEventIDs(); float * ht = smd->GetHeights();` `ev == nullptr || ht == nullptr || n < 2` → `no_data`; (f) `const size_t cells = (size_t)n * (size_t)n;` `std::vector<int16_t> events(ev, ev + cells); std::vector<float> heights(ht, ht + cells);` zaman ölç (`std::chrono::steady_clock`) → `copyMs`; (g) `m_grid.Init(n, unit, std::move(events), std::move(heights))` başarısız → `init`; (h) `m_grid.Build()` → `buildMs`; `m_grid.MainComponentCells() <= 0` → `no_walk` ve **hazır yapma**; (i) `m_info` doldur (`zone = 71`, `crc32 = BotCore::NavGridFingerprint(m_grid)`), `m_ready.store(true, std::memory_order_release)`; (j) tek satır: `NavService: nav ready: zone 71 cells=%d main_cells=%d unit=%.1f crc32=%08x copy_ms=%.1f build_ms=%.1f` (`printf` + `WriteNavLog`; `cells = n*n`); başarısızlıkta `NavService: nav FAILED (%s)`. **İkinci `Startup()` çağrısı ızgarayı yeniden kurmaz** (`m_ready` zaten `true` ise erken dön). `Shutdown()`: `m_ready.store(false, std::memory_order_release)`.
   - `NavService::Instance()` fonksiyon-yerel statik tekil (`BotManager::Instance()` kalıbı).
   - Yeni kodda `ENABLED=0`/`NAV=0` iken `printf` yok; `printf` yalnızca (j) satırı.
8. **`GameServer/proj-GameServer.vcxproj` ve `.filters`**: yukarıdaki iki dosyayı ekle.
9. **`GameServer/GameServerDlg.cpp`**: `#include "Bot/NavService.h"`; `LoadNoticeData();` sonrasına:

   ```cpp
   	// F5-59: navigation grid of zone 71 ([BOT] NAV=1 only; failure is logged and never fatal).
   	NavService::Instance().Startup();
   ```

   yıkıcıda `BotManager::Instance().Shutdown();` sonrasına `NavService::Instance().Shutdown();`.
10. Derle ve test et (§7); `python3 tools/check-perception-contract.py` çalıştır (rc=0 beklenir). Sunucu çalıştırma bu planda **uygulayıcıya düşmez**: çalışma zamanı doğrulamasını Claude yapar (§6 K9-K12); uygulayıcı yalnızca derleme ve birim testlerini kanıtlar.
11. Uygulayıcı Raporu'nu yaz; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0; `./tools/build.sh Debug` rc=0; değişen/yeni dosyalardan **yeni uyarı yok** (özellikle `NavService.cpp` ve `NavGrid.h`'in GameServer içinde ilk dahil edilişi; uyarı çıkarsa `BotCore` başlığını değiştirme, raporla)
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; dört yeni test adı `[ OK ]` (`NavFingerprint_Crc32_KnownVectors`, `NavFingerprint_InitEqualsLoad`, `NavFingerprint_Sensitivity`, `NavFingerprint_RealMap_MatchesFile`); mevcut testler değişmeden geçer
- [ ] K3: `NavFingerprint_RealMap_MatchesFile` çıktısı `SKIPPED` **değil**, `... main=88508 crc32=<hex> match=1`; `<hex>` değeri `python3 tools/nav-export.py` çıktısındaki `crc32=` ile **aynı** (rapora ikisini de yaz)
- [ ] K4: `git diff gece/2026-10-02...bot/F5-59 --stat` yalnızca §4'teki 10 dosya (4 yeni) ve plan dosyası; `shared/SMDFile.h` farkı **tam 1 eklenen satır**; `BotCore/NavGrid.h` ve diğer mevcut `Nav*.h` farkı 0; `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K5: `BotCore/NavFingerprint.h`: `grep -n -E "windows.h|stdafx|GameServer|shared/" BotCore/NavFingerprint.h` boş; `new|malloc|static ` (yeni kodda) yok; `.cpp` dosyası eklenmedi; `python3 tools/check-perception-contract.py` rc=0 (R1-R5 PASS)
- [ ] K6: `GameServer/Bot/NavService.cpp`: `grep -n "printf" GameServer/Bot/NavService.cpp` yalnızca `Startup()` içindeki hazır/başarısız satırlarını gösterir; `BotManager`/`ActionExecutor`/`BotSession` dosyaları **değişmedi** (K4 ile)
- [ ] K7: `NavService::Startup()` çağrısı `GameServer/GameServerDlg.cpp`'de `MapFileLoad()` (`:150`) ve `Logs` oluşturma (`:170`) **sonrasında**, `RunServer()` (`:213`) **öncesindedir** (dosya:satır kanıtı rapora); `BotManager::Instance().Startup()` (`:105`) çağrısı yerinde değişmedi
- [ ] K8: Yeni dosyalar yalnızca ASCII + CRLF (`file` çıktısı rapora); `GameServerDlg.cpp` UTF-8 BOM'u korundu (`head -c3 | xxd -p` = `efbbbf`)
- [ ] K9 (Claude, çalışma zamanı): çalışma dizinindeki `GameServer.ini`'de `[BOT] ENABLED=1`, `NAV=1` ile sunucu açılışı; konsolda ve `Logs/Bot_*.log`'da **tek** satır `NavService: nav ready: zone 71 cells=263169 main_cells=88508 unit=4.0 crc32=<hex> copy_ms=... build_ms=...`; `main_cells=88508`; `crc32` değeri `python3 tools/nav-export.py` çıktısındaki `crc32=` ile **eşit** (sunucu belleği = araç dosyası); `build_ms` değeri rapora (yumuşak sınır `[A]`: Release'te < 1000 ms; aşılırsa kabul engeli değil, bulgu)
- [ ] K10 (Claude): `NAV=0` (veya anahtar yok) + `ENABLED=1`: `NavService` satırı **yok**, bot başlatma satırları ve davranışı ön-değişiklik sunucusuyla aynı; `ENABLED=0` + `NAV=1`: `NavService` satırı **yok**
- [ ] K11 (Claude): hata yolu: `NAV=1` iken zone 71'i kaldırmadan sunucu davranışı değiştirilemez; bu yüzden hata yolu birim düzeyinde sınanamaz: Claude kodda `no_zone`, `no_data`, `init`, `no_walk` dallarının `Ready()`'yi `false` bıraktığını ve `return true` döndürdüğünü okuyarak doğrular (kod incelemesi, çalışma zamanı kanıtı değil: raporda böyle etiketlenir)
- [ ] K12 (Claude): sunucu `stop` ile kapanır (takılma/çökme yok); Bot günlüğünde ikinci `nav ready` satırı yok

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py                      # build/nav/zone71.navgrid + crc32 satırı
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavFingerprint_|NAVFP|tests,"
./tools/run-tests.sh Debug
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-59
git diff gece/2026-10-02...bot/F5-59 -- shared/SMDFile.h | grep -c "^[-+][^-+]"   # 1
git diff --check gece/2026-10-02...bot/F5-59
# Claude (çalışma zamanı): ./tools/run-servers.sh start ; grep -a "NavService" /mnt/c/dev/fdp/server/Logs/Bot_*.log ; ./tools/run-servers.sh stop
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3: kod satır sonu CRLF, girinti tab, Allman, yorumlar İngilizce, yeni dosyalar ASCII; konsol spam'i yok (yalnızca K6'daki tek satır); bot sistemi/`NAV` kapalıyken sunucu davranışı değişmez.
- `NavService` **IOCP dışı** (ana iş parçacığı, `RunServer()` öncesi) kurulur; kurulduktan sonra `m_grid` hiçbir zaman yazılmaz. Sonraki dilimler `Grid()` ile yalnızca `const` okur; yol bulma durumu (`NavPathfinder`, ~4,2 MB, **iş parçacığı güvenli değil**) ayrı örneklerde tutulur (F5-62).
- Bellek: ızgara ≈ 2,1 MB (`events` 0,5 + `heights` 1,0 + `walk` 0,26 + `clearance` 0,26) + kurulum anında 1,6 MB geçici kopya; yalnızca `NAV=1` iken.
- **Dürüstlük:** birim testleri parmak izinin düzenini ve `Init`/`Load` eşdeğerliğini sınar; sunucunun gerçekten **aynı baytları** kopyaladığı **yalnızca K9** (çalışma zamanı) ile kanıtlanır. `SMDFile::GetMapSize()` `n - 1` döndürür: kodda `+ 1` unutulursa `Init` boyut hatasıyla `false` verir (`init` hatası) ve K9 başarısız olur.
- `crc32` yalnızca bir tutarlılık parmak izidir, güvenlik amaçlı değildir. Dosya biçimi `FDPNAV01` değişirse `NavFingerprint.h` ve `tools/nav-export.py` birlikte güncellenmelidir.
- Bu plan **su/engel verisinin doğruluğunu iddia etmez**: ızgara, `NavGrid::Build` kuralıyla (olay = 1, ana bileşen) kurulur; su katmanı sorusu F5-60'tadır.
- Beklenmedik durumda (derleme planın dışındaki bir nedenle bozulursa, `NavGrid.h` GameServer'da uyarı/hata veriyorsa, başka bir dosya gerekiyorsa) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile soruyu raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-59` — `<kısa-sha> [F5-59] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ … (K9-K12 Claude'un çalışma zamanı kriterleri)
- `nav-export.py` crc32 değeri ve gerçek-harita testi satırı: …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F5-59` @ `<sha>`
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
