# F4-38: `ActionExecutor` hız kontrol paketi dilimi — periyodik `WIZ_SPEEDHACK_CHECK` (CLI-12)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-38` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-37 (ADR-0018 Ek 13, dilim 6f) — `KAPANDI` (merge `dd8262e`); F4-13/F4-15 (`TickUserIn`/`TickNpcIn`: otomatik istemci trafiği kalıbı) — `KAPANDI`; F4-01 (`BotCore/BotMotion.h`, hız/adım kuralı) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-12 (`docs/03` §14), MEC-MOV-04, MEC-MOV-09 (yeni, bu planla), AC-LRN-03, ADR-0018 madde 7 / Ek 14 |
| Tahmini büyüklük | S–M (8 dosya; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

Oyundaki bir bot, gerçek istemcinin yaptığı gibi **oyundayken her 10,0 sn'de bir `WIZ_SPEEDHACK_CHECK` paketi** gönderir (`CUser::HandlePacket()` üzerinden). Sunucu bu paketle tek mesafe denetimini yapar: son kontrolden/ışınlanmadan beri ≥ 87,75 m (hız sınırı 67 için) yer değiştirmiş oyuncuyu geri ışınlar (`CUser::SpeedHackTime`). Bot bu denetime **insanla aynı şekilde tabi** olur; sonuç yalnızca sunucunun yayınladığı `WIZ_WARP` paketinden okunur (geri ışınlama varsa `warped`, yoksa `passed`). Karar katmanı ve komut yoktur: paket `Tick()` içinde otomatik gider. `[BOT] SPEEDHACK_CHECK=0` ile kapatılabilir (varsayılan `1`). Bot sistemi kapalıyken (`ENABLED=0`, varsayılan) hiçbir şey değişmez.

F4'ün ADR-0018 madde 7 dilimidir (ADR-0018 Ek 14).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` madde 7 ve "Ek 14" (bu planla birlikte yazıldı); `docs/adr/ADR-0017-...md` "Ek (F4-38)".
- `docs/03` §14 **CLI-12** (`[A]` → bu planla paket düzeni `[V]`), **MEC-MOV-09** (yeni, bu planla), MEC-MOV-04, §13.2 CLI-12 ölçümü (10,0 sn).
- `plans/F4-13-algi-bolge-degisimi-kullanici-istegi.md` ve birleşmiş kod: **yazılı planı değil kodu esas al** — `ActionExecutor::TickUserIn` (`GameServer/Bot/ActionExecutor.cpp:2955`), çağrısı `BotManager.cpp:3087`.
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `dd8262e` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:425-427` — `HandlePacket` oyun içi anahtarı: `case WIZ_SPEEDHACK_CHECK: SpeedHackTime(pkt)`. (`:290-292` karakter seçimi öncesi aynı çağrı; bot oyundadır.)
  - `GameServer/User.cpp:3615-3669` — `CUser::SpeedHackTime`: `!isInGame() || isGM()` ise sessizce döner (`:3617`); sınır `nSpeed` = 45 (varsayılan) / 90 (captain ya da rogue) / 67 (warrior, mage, priest) `+ 10` (`:3620-3627`); `nRange = ((x − m_LastX)² + (z − m_LastZ)²) / 100` (`:3629`); `nRange >= nSpeed` ise `WriteCheatLogFile("... is Warp to Last Position")` ve **`Warp(m_LastX·10, m_LastZ·10)`** (`:3631-3636`), değilse `m_LastX/Z = GetX()/GetZ()` (`:3637-3641`). **Paket yükü okunmaz**: okuyan kod `#if 0` içindedir (`:3643-3668`). Eşik: uzaklık² ≥ `100 × (sınır + 10)` ⇒ hız 67: √7700 = **87,75 m**, 45: 74,16 m, 90: 100 m (konum birimi metre, `WIZ_MOVE` alanı ×10).
  - `m_LastX/Z` yalnızca şurada yazılır: `SpeedHackTime` (geçerse), `Warp` (`CharacterMovementHandler.cpp:639-640`), `ZoneChange` (`:371-372`), `GameStart(2)` (`CharacterSelectionHandler.cpp:323-324`), yeniden doğuş (`AttackHandler.cpp:175-176`); `CUser` başlatıcısı sıfırlar (`User.cpp:193-194`). **`WIZ_MOVE` yazmaz** ⇒ iki kontrol arasında ≥ 87,75 m gidilirse geri ışınlanır; kontrol hiç gelmezse hiç denetlenmez (bot bugüne kadar hiç denetlenmedi). Bot kodunda `SetPosition`/`Warp` çağrısı yoktur (`grep` boş), bu yüzden `m_LastX` bayatlaması yalnızca sunucunun kendi ışınlamalarıyla (hepsi `m_LastX` yazar) mümkündür.
  - `GameServer/CharacterMovementHandler.cpp:625-660` — `CUser::Warp`: `m_bWarp` ise hiçbir şey yapmadan döner (`:629`); geçersiz konumda döner (`:632-636`); sonra **`Packet result(WIZ_WARP); result << sPosX << sPosZ; Send(&result)`** (`:642-644`): yük `u16 x·10, u16 z·10`. `CUser::Send` bot alıcısına eşzamanlı iletir (`User.cpp:19-36`), yani geri ışınlama `HandlePacket` dönmeden **aynı thread'de** botun `OnPacket()`'ine düşer.
  - `shared/packets.h:32` `WIZ_WARP 0x1E`, `:67` `WIZ_SPEEDHACK_CHECK 0x41` (ISO-8859 dosya: `grep -a`).
  - `shared/ByteBuffer.h:38` `operator<<(float)` vardır.
  - **Gerçek istemci yükü `[V]`** (`/mnt/c/dev/fdp/server/Logs/PacketTrace_3_10_2026.log`, `BotWG_K`, ve `PacketTrace_2_10_2026.log`, `testing`; satır biçimi `<ms> 0 <ad> <zone> <op> <boyut> <hex>`): op `41`, boyut 5, `u8 bayrak` + `f32` (little-endian) istemci saati. Oturum başında 4 paket bayrak `01` (t = 0, ~1,1, ~1,5, ~2,4 sn; saat 21,09 → 23,48), sonra bayrak `00`, **10,0 sn aralık**, saat her pakette +10,0 (33,56 → 43,57 → 53,57). İkinci kayıtta toplam 22 paket `01`, 134 paket `00`. Oyuna girmeden önce giden `01` paketleri sunucuda etkisizdir (`:3617` `!isInGame()`), bot onları göndermez. İstemci saati sunucuca okunmadığı için bot saati kendi oyuna giriş süresinden türetir `[A]`.
  - `GameServer/Bot/BotSession.h:66` `m_inGameSince`; `BotManager.cpp:2985` oyuna girişte yazılır. `BotSession.h:140-150` `m_userInHasLast/Last/Requests` ve `m_npcIn*` üyeleri (kalıp); `BotSession.h:179-181` `m_userInEcho`/`m_npcInEcho` atomikleri; `BotSession.cpp:21-28` başlatıcı listesi, `:480-500` `ResetForRespawn()`.
  - `BotManager.cpp:163-198` `Startup()` ini okuma (`[BOT]` anahtarları), `BotManager.h:58` oluşturucu başlatıcı listesi, `:112` `m_respawnCycles`; `BotManager.cpp:3022-3040` canlı bot dalı (`else`, ölü değil), `:3087-3118` `TickUserIn`/`TickNpcIn` çağrıları.
  - `BotCore/BotMotion.h:24-27` `MaxStepMeters`, `:31` `ServerSpeedLimit`, `:86` `CheckMoveStep` içindeki adım payı (`× 1,10 + 0,15`).

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotMotion.h` içine ekleme, yalnızca standart kütüphane):** `kSpeedCheckPeriodMs`, `SpeedCheckDue`, `SpeedCheckClockSeconds`, `SpeedCheckWarpDistance`. Birim testleri (`MotionTests.cpp`).
2. **`ActionExecutor::TickSpeedCheck(s, now)`** (otomatik istemci trafiği, `TickUserIn` kalıbı): vadesi gelince `WIZ_SPEEDHACK_CHECK` paketi → `HandlePacket` → `WIZ_WARP` yankısından sonuç eşleme → telemetri.
3. **Oturum durumu (`BotSession`):** son kontrol zamanı/sayaçlar (spawn başına) ve `OnPacket()`'in doldurduğu `m_warpEcho` (`WIZ_WARP` bloğu: **ekleme**).
4. **`BotManager`:** `Tick()` içinde `TickNpcIn`'den sonra çağrı, `[BOT] SPEEDHACK_CHECK` ini anahtarı (varsayılan `1`), yalnızca geri ışınlamada günlük satırı.
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT` / `ACTION_RESULT` (`"type":"SpeedCheck"`).

**Kapsam dışı (yapılmayacak)**

- **Oyuna girmeden önceki `01` bayraklı paketler**, istemci saatini gerçek istemci gibi taklit etmek (saat sunucuca okunmaz), `#if 0` içindeki süre farkı denetimi.
- **Geri ışınlamaya tepki:** sürüyen yürüyüşü/seriyi durdurmak, konumu yeniden planlamak (karar katmanı/F5). Guard'a uyan bir bot geri ışınlanmaz (§5.2 birim testi); olursa yalnızca günlüğe ve telemetriye yazılır.
- **Rogue/captain (sınır 90) botları:** bot profillerinde yoktur (`docs/01` §2). Bu sınırda 9 m/s hızla 10 sn yürüyen bot eşiğe (100 m) çok yaklaşır; profil eklenirse `MEC-MOV-09` yeniden değerlendirilir.
- **`CLI-11` penceresine sayım, `BotFairnessGuard` reddi, `FAIRNESS_REJECT`:** yok (otomatik istemci trafiği; kural zamanlamadır, vadesi gelmeyen tick sessizce `NOTHING` döner).
- `/bot` komutu, `BuildStatusLines()` değişikliği, `+bot` (`ChatHandler.cpp`), `ScenarioRunner.*`, `Telemetry.*`, `tools/*` (rapor aracı `ok=false reason=warped` kaydını zaten "other" sayar; değişmez).
- `m_LastX/Z` okumak veya yazmak, `SpeedHackTime`/`Warp` çağırmak: **yasak** (AC-LRN-03; yalnızca `HandlePacket` ve yankı).
- Dokümanları (`docs/03`, `docs/17`, ADR'ler) güncellemek: Claude'un işi (bu planla birlikte yapıldı), DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotMotion.h` | değiştir | Yalnızca ekleme (§5.2); mevcut içerik ve `#include`'lar değişmez (`<cmath>` zaten var) |
| `Tests/BotCoreTests/MotionTests.cpp` | değiştir | Yalnızca ekleme: iki yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `SpeedCheckOutcome` + bir yeni statik fonksiyon |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | `TickSpeedCheck` (§5.4); başka kod değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca yeni üyeler |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi, `ResetForRespawn()` ve `OnPacket()`'e **ekleme** bloğu |
| `GameServer/Bot/BotManager.h` | değiştir | `m_speedCheck` üyesi + başlatıcı |
| `GameServer/Bot/BotManager.cpp` | değiştir | ini okuma + `Tick()` çağrısı |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (Yeni dosya açılmaz; `proj-GameServer.vcxproj*`, `BotCore*.vcxproj`, `BotCoreTests.vcxproj` değişmez.)

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-38 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4). **Önce** `./tools/run-tests.sh Release` çalıştır ve mevcut test sayısını not et (F4-37 doğrulamasında 249; yeni sayı +2 olmalı).

### 5.2 `BotCore/BotMotion.h` (saf mantık, mevcut `namespace BotCore` içine ekleme)

Mevcut kodun biçimini (tab, Allman, `inline`, İngilizce kısa yorum, ölçülmemiş değer `[A]`) koru. `CheckMoveStep`'ten **sonra**, `namespace` kapanışından önce ekle. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez**.

```cpp
	// --- speed check slice (ADR-0017 Ek F4-38, docs/03 CLI-12 / MEC-MOV-09) ---

	constexpr uint32_t kSpeedCheckPeriodMs = 10000;   // docs/03 CLI-12 [V]: the client sends WIZ_SPEEDHACK_CHECK every 10.0 s in game

	// True when the next WIZ_SPEEDHACK_CHECK is due: kSpeedCheckPeriodMs after the previous one or, before the first one of
	// this spawn, kSpeedCheckPeriodMs after the bot entered the game.
	inline bool SpeedCheckDue(bool hasLast, uint32_t sinceLastMs, uint32_t sinceInGameMs);

	// Client clock carried by the packet (f32 seconds): time since the bot entered the game. The server never reads it
	// (User.cpp:3643-3668 is #if 0) [A].
	inline float SpeedCheckClockSeconds(uint32_t sinceInGameMs);

	// Distance in metres from which CUser::SpeedHackTime() sends a player back to the last checked position:
	// dist^2 / 100 >= limit + 10 (User.cpp:3627-3631), so dist = sqrt(100 * (limit + 10)).
	inline float SpeedCheckWarpDistance(int16_t serverLimitField);
```

Gövdeler: `SpeedCheckDue` → `hasLast ? sinceLastMs >= kSpeedCheckPeriodMs : sinceInGameMs >= kSpeedCheckPeriodMs`; `SpeedCheckClockSeconds` → `sinceInGameMs / 1000.0f`; `SpeedCheckWarpDistance` → `std::sqrt(100.0f * (serverLimitField + 10.0f))`.

**`Tests/BotCoreTests/MotionTests.cpp` (ekleme, mevcut makro stili):** iki yeni `TEST_CASE`:

- `Motion_SpeedCheckSchedule`: `kSpeedCheckPeriodMs == 10000`; `SpeedCheckDue(false, 0, 9999)` yanlış, `(false, 0, 10000)` doğru; ilk kontrolden sonra yalnızca `sinceLastMs` belirler: `(true, 9999, 50000)` yanlış, `(true, 10000, 0)` doğru; `SpeedCheckClockSeconds(12345)` ≈ 12,345 (±1e-3), `SpeedCheckClockSeconds(0) == 0`.
- `Motion_SpeedCheckWarpDistance`: `SpeedCheckWarpDistance(67)` ≈ 87,7496 (±1e-3), `(45)` ≈ 74,1620, `(90)` ≈ 100,0 (hepsi ±1e-3); sınır arttıkça artar. **Mülkiyet:** botun yürüdüğü hızlar için (`{sınır 67, hız 67}`, `{67, 45}`, `{45, 45}` = `{ServerSpeedLimit, kSprintSpeedField/kWalkSpeedField}` çiftleri) `MaxStepMeters(hız, kSpeedCheckPeriodMs + 1000) * 1.10f + 0.15f < SpeedCheckWarpDistance(sınır)` (`+ 1000` = `TICK_MS` üst sınırı, `BotManager.cpp:176-181`; `1,10`/`0,15` `CheckMoveStep` içindeki adım payıdır, `BotMotion.h:86`): guard'a uyan bir yürüyüş iki kontrol arasında geri ışınlanma eşiğine ulaşamaz. Yorumda sınır 90 çiftinin bilerek yer almadığını (rogue/captain, bot profilinde yok) yaz.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'de `m_npcIn*` üyelerinin altına (aynı yorum/hizalama biçimi):

```cpp
	bool m_speedHasLast;                                   // IOCP thread only: m_speedLast is valid for this spawn
	std::chrono::steady_clock::time_point m_speedLast;     // IOCP thread only: when the last WIZ_SPEEDHACK_CHECK went out
	uint32 m_speedChecks;                                  // IOCP thread only: WIZ_SPEEDHACK_CHECK packets sent in this spawn
	uint32 m_speedWarps;                                   // IOCP thread only: of those, how many the server answered with a WIZ_WARP
```

ve atomik üyelerin yanına (`m_npcInEcho`'nun altına):

```cpp
	std::atomic<uint64> m_warpEcho;                        // written by OnPacket(): valid bit (63) | x << 16 | z (both x10) of the last WIZ_WARP
```

`BotSession.cpp`: başlatıcı listesine `m_warpEcho(0)` ekle (mevcut sıraya uy: üye bildirim sırasıyla aynı sırada, derleyici sıra uyarısı vermemeli; sayaç üyeleri `ResetForRespawn()` ile aynı değerlerle başlar). `ResetForRespawn()` içine `m_speedHasLast = false; m_speedChecks = 0; m_speedWarps = 0; m_warpEcho = 0;` ekle (`m_npcInEcho = 0;` satırının yanına).

`OnPacket()`: mevcut bloklardan **sonra**, **yalnızca ekleme** (mevcut bloklar bayt bayt aynı kalır):

```cpp
	// Server-side warp: u16 x, u16 z (both x10; CharacterMovementHandler.cpp:642-644). CUser::Send() delivers it on the same
	// thread, inside HandlePacket(), so TickSpeedCheck() can read it right after the call.
	if (opcode == WIZ_WARP && pkt.size() >= 4)
	{
		uint16 x = pkt.read<uint16>(0);
		uint16 z = pkt.read<uint16>(2);
		m_warpEcho = (1ull << 63) | (uint64(x) << 16) | uint64(z);
	}
```

(`pkt.read<T>(offset)` mevcut bloklardaki gibi bayt ofsetiyle okur; ofset semantiğini `shared/ByteBuffer.h`'den teyit et.)

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

**Başlık:** `NpcInOutcome`'dan sonra:

```cpp
// Result of ActionExecutor::TickSpeedCheck (ADR-0017 Ek F4-38).
struct SpeedCheckOutcome
{
	enum Kind { NOTHING, SENT, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed. SENT: "passed" (no WIZ_WARP came back). FAILED: "warped" (the server sent
	                       // the bot back: WIZ_WARP echo). NOTHING: "ok" (not due, not in game, dead).
	float warpX;           // valid only when kind == FAILED: the position the WIZ_WARP carried (metres)
	float warpZ;
};
```

`class ActionExecutor` içine (`TickNpcIn`'in altına, aynı yorum kalıbıyla):

```cpp
	// Called once per Tick() for every in-game, living session. Sends one WIZ_SPEEDHACK_CHECK (u8 0, f32 client clock)
	// through CUser::HandlePacket() when BotCore::SpeedCheckDue says so (CLI-12: every 10 s in game, the first one 10 s
	// after entering the game). Result only from the WIZ_WARP the server published during the call (m_warpEcho): none ->
	// SENT "passed", one -> FAILED "warped". Not counted in the CLI-11 window (automatic client traffic); no guard rule can
	// reject it, a tick that is not due returns NOTHING without an event.
	static SpeedCheckOutcome TickSpeedCheck(BotSession * s, std::chrono::steady_clock::time_point now);
```

**`ActionExecutor.cpp`** (`TickNpcIn`'den sonra, "--- speed check slice (ADR-0017 Ek F4-38) ---" başlığıyla):

`TickSpeedCheck`:
- Başlangıç: `out.kind = NOTHING; out.reason = "ok"; out.warpX = out.warpZ = 0;`. `CUser * user = s != nullptr ? s->m_pUser : nullptr; if (user == nullptr || !user->isInGame() || user->isDead()) return out;`
- `sinceInGameMs = duration_cast<milliseconds>(now - s->m_inGameSince)`, `sinceLastMs = s->m_speedHasLast ? now - s->m_speedLast : 0` (`TickUserIn`'deki `uint32` dönüşümü gibi); `!BotCore::SpeedCheckDue(s->m_speedHasLast, sinceLastMs, sinceInGameMs)` → `return out` (olay yok).
- `decisionId = NextDecisionId(s)`; `clock = BotCore::SpeedCheckClockSeconds(sinceInGameMs)`; `decisions` açıksa `ACTION_SUBMIT`: `"decision_id","type":"SpeedCheck","clock":<FormatFixed(clock, 1)>` (mevcut `FormatFixed` yardımcısıyla).
- Paket: `Packet pkt(WIZ_SPEEDHACK_CHECK); pkt << uint8(0) << clock;` (istemci biçimi `u8 bayrak + f32`, §2). `s->m_warpEcho = 0;` → `steady_clock` ile süreli `user->HandlePacket(pkt)` → `s->m_speedHasLast = true; s->m_speedLast = now; s->m_speedChecks++;`.
- Sonuç: `e = s->m_warpEcho.load(); warped = (e & (1ull << 63)) != 0;` `warped` ise `out.warpX = ((e >> 16) & 0xFFFF) / 10.0f`, `out.warpZ = (e & 0xFFFF) / 10.0f`, `s->m_speedWarps++`.
- `ACTION_RESULT` (`decisions`): `"decision_id","type":"SpeedCheck","ok":<!warped>,"reason":"passed"|"warped","latency_us"`, `warped` ise ek `"warp_x","warp_z"` (`FormatFixed`, 1 ondalık). **`user->m_LastX`/`GetX()` sonuç eşlemesinde kullanılmaz** (telemetri değeri olarak da eklenmez).
- Dönüş: `!warped` → `SENT "passed"`; `warped` → `FAILED "warped"` (+ `warpX/warpZ`).

`HandlePacket(pkt)` çağrısı bu paket için **tek yerde** (`TickSpeedCheck`). `m_LastX`, `m_LastZ` hiçbir yerde okunmaz/yazılmaz; `SpeedHackTime`, `Warp` çağrılmaz.

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

1. **`BotManager.h`:** `m_respawnCycles` bildiriminin **hemen altına** `bool m_speedCheck;      // [BOT] SPEEDHACK_CHECK: send WIZ_SPEEDHACK_CHECK every 10 s per in-game bot (1 = on, default)` ekle; oluşturucu başlatıcı listesinde `m_respawnCycles(0)`'dan hemen sonra `m_speedCheck(true)` (bildirim sırasıyla aynı sıra).
2. **`BotManager.cpp` `Startup()`:** `m_respawnCycles = ...` satırından sonra: `m_speedCheck = ini.GetInt("BOT", "SPEEDHACK_CHECK", 1) != 0;`. (`ENABLED=0` ise `Startup()` bu satıra gelmeden döner; davranış değişmez.)
3. **`Tick()` canlı bot dalı:** `TickNpcIn` bloğunun hemen ardına, aynı `else` bloğunun içine:
   ```cpp
   if (m_speedCheck)
   {
   	SpeedCheckOutcome speed = ActionExecutor::TickSpeedCheck(s, now);
   	if (speed.kind == SpeedCheckOutcome::FAILED)
   	{
   		char message[224];
   		snprintf(message, sizeof(message),
   			"BotManager: bot %s speedcheck: server warped it back to (%.1f, %.1f)",
   			s->m_charName.c_str(), speed.warpX, speed.warpZ);
   		WriteBotLog(message);
   	}
   }
   ```
   Başarılı kontrol için günlük satırı **yazılmaz** (16 bot × 10 sn gürültü); telemetri `decisions` yeter.
4. Başka hiçbir yere dokunma (`BeginDespawn()`, `BuildStatusLines()`, `ExecuteCommand()`, `ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotMotion.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `MotionTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı iki yeni test adını (`Motion_SpeedCheckSchedule`, `Motion_SpeedCheckWarpDistance`) içerir ve toplam test sayısı **başlangıç + 2** (başlangıç 249 ise 251); `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotMotion.h` eşleşme vermez; `#include`'lar yalnızca `<algorithm>`, `<cmath>`, `<cstdint>`.
- [ ] K5: paket yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_SPEEDHACK_CHECK" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma) gösterir; `grep -n "WIZ_WARP" GameServer/Bot/*.cpp` yalnızca `BotSession.cpp` (`OnPacket`) gösterir; `grep -n "SpeedHackTime\|SpeedHackUser\|->Warp(\|\.Warp(" GameServer/Bot/*.cpp GameServer/Bot/*.h` boş; `grep -n "m_LastX\|m_LastZ" GameServer/Bot/*.cpp GameServer/Bot/*.h` boş.
- [ ] K6: `TickSpeedCheck` yalnızca vadesi gelince gönderir: `SpeedCheckDue` çağrısı ve `false` iken erken dönüş vardır (kod okumasıyla); `HandlePacket(pkt)` bu fonksiyonda tek yerde; sonuç `m_warpEcho` bitinden eşleniyor.
- [ ] K7: `OnPacket()` yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-38 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesinde bilinçli değiştirilen satır(lar)ı gösterir; mevcut `OnPacket()` blokları değişmedi.
- [ ] K8: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve `Tick()` içinde çalışır; `git diff gece/2026-10-02...bot/F4-38 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` **boş** (yalnızca ekleme); `Startup()`'ta yeni ini satırı `ENABLED` denetiminden sonradır.
- [ ] K9: `git diff --stat gece/2026-10-02...bot/F4-38` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K10: değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı: hepsi ASCII + CRLF); `git diff --check` boş.
- [ ] K11: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K12: F4-01..F4-37 gerilemesiz: `grep -c "TickUserIn\|TickNpcIn" GameServer/Bot/BotManager.cpp` ≥ 2 (değişmedi), `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2; önceki tüm testler hâlâ geçiyor.
- [ ] K13 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S5 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/run-tests.sh Release         # önce: başlangıç sayısını not et
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-38
git diff gece/2026-10-02...bot/F4-38 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-38 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-38 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotMotion.h
grep -an "WIZ_SPEEDHACK_CHECK" GameServer/Bot/*.cpp
grep -an "WIZ_WARP" GameServer/Bot/*.cpp
grep -n "SpeedHackTime\|SpeedHackUser\|m_LastX\|m_LastZ\|->Warp(" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "SpeedCheckDue\|HandlePacket" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
file BotCore/BotMotion.h Tests/BotCoreTests/MotionTests.cpp GameServer/Bot/*.cpp GameServer/Bot/*.h
git diff --check gece/2026-10-02...bot/F4-38
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`). Botlar: Karus warrior `BotWP_K`, Karus mage `BotMF_K`; zone 71. Gözlem `Logs/bots/<tarih>/live-*.jsonl` (`decisions`) ve sunucunun `Logs/Cheat_<g>_<a>_<y>.log` dosyasından (önce satır sayısını not et).

1. **S1 (mutlu yol):** `spawn BotWP_K`, oyuna girişten (`m_inGameSince`) sonra ≥ 35 sn bekle: JSONL'de en az 3 `ACTION_SUBMIT` (`type:"SpeedCheck"`) → `ACTION_RESULT` (`ok:true`, `reason:"passed"`, `latency_us` < 5000); ilki oyuna girişten ≥ 10,0 sn sonra, ardışık ikisinin aralığı 10,0–10,3 sn (`TICK_MS` 100); `clock` değerleri ≈ 10,0 / 20,1 / 30,2; **Cheat günlüğünde yeni `SpeedHack` satırı yok** (ilk kontrol `GameStart(2)`'de yazılan `m_LastX` ile karşılaştırılır; yazılmasaydı satır çıkardı); bot günlüğünde `speedcheck` satırı yok.
2. **S2 (kapatma anahtarı):** ini `SPEEDHACK_CHECK=0`, sunucuyu yeniden aç, `spawn BotWP_K`, 35 sn: JSONL'de `SpeedCheck` olayı **yok**; `ENABLED=0` ile açılışta bot kodu çalışmaz (önceki davranış).
3. **S3 (yürüyüş geri ışınlanmaz):** `move BotWP_K <x> <z>` ile ≥ 2 kontrolü kapsayan uzun bir düz yürüyüş (zone 71, ≥ 60 m; komutun kabul ettiği en yüksek hız alanı, sprint 67 mümkünse): her kontrol `passed`, sunucu konumu hedefe varır, `warped` yok, Cheat günlüğünde yeni satır yok.
4. **S4 (geri ışınlama; isteğe bağlı, geçici yama ile, commit edilmez):** botu iki kontrol arasında ≥ 100 m kaydıran geçici deneme (ör. kontrolleri açıkken `CUser` konumunu `m_curx` ile geçici kodda kaydırmak ya da kontrolsüz başlayıp ≥ 90 m yürüyüp sonra kontrolü açmak): beklenen: bir sonraki kontrol `ACTION_RESULT` `ok:false`, `reason:"warped"`, `warp_x`/`warp_z` = son geçen kontrolün konumu, bot günlüğünde `speedcheck: server warped it back`, Cheat günlüğünde bir `is Warp to Last Position` satırı. Yapılamazsa `[D]` (kod okuması) olarak raporlanır, kriter dışı kalmaz.
5. **S5 (gerilemesiz):** `list`, `move`, `cast`, `pot`, `see` komutları çalışır; `summary` `SpeedCheck` olaylarının `ACTION_RESULT` toplamlarına karışmasının `tools/bot-telemetry-report.py` MET-ACT-02 çıktısını bozmadığını gösterir (`SpeedCheck` `ok:true` satırları geçerli sayılır, `warped` "other"); despawn sonrası yeniden spawn'da sayaçlar sıfırdan başlar (ilk kontrol yine girişten 10 sn sonra).

Beklenmeyen `no_result`/`warped` bu planın hatası değil, **sonuç olarak raporlanır**.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları; bu dosyalar ASCII + CRLF'tir (CRLF'yi koru, yeni satırları CRLF yaz). Kod yorumları İngilizce.
- Thread kuralı: `m_speed*` üyeleri yalnızca IOCP thread'inde; `m_warpEcho` `OnPacket()`'te (herhangi thread) yazılır, `TickSpeedCheck`'te okunur (`std::atomic`).
- **AC-LRN-03:** başka botların/oyuncuların durumuna bakılmaz; `m_LastX/Z` sunucunun iç denetim durumudur, bot ona dokunmaz ve sonucu ondan çıkarmaz.
- Sunucu davranışı değişmez: `SpeedHackTime`, `Warp`, `HandlePacket` olduğu gibi kalır. Bu plan yalnızca botun **bugüne kadar hiç tabi olmadığı** insan denetimine tabi olmasını sağlar (CLI-12 `[A]`: tutarlılık).
- Risk (`MEC-MOV-09`): `m_bWarp` iken `Warp()` sessizce döner; bu durumda sunucu geri ışınlama kararı vermiş olsa da `WIZ_WARP` yayınlanmaz ve bot `passed` görür. Bot yalnızca gördüğünü raporlar; Cheat günlüğü gerçeği gösterir (S1/S4 bu yüzden Cheat günlüğüne de bakar).
- Kod yazarken `TickUserIn`/`TickNpcIn` kalıbından sapma gerekirse **durup** Uygulayıcı Raporu'na yaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-38` — `<kısa-sha> [F4-38] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-38` @ `<sha>`
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
