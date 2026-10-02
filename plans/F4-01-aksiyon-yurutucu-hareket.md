# F4-01: `ActionExecutor` çekirdeği — hareket (`Move`/`Stop`) ve `BotFairnessGuard` hız/adım kuralları

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-01` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F2-03/F2-04 (bot oturumu), F2-06 + F3-04 (komut çekirdeği), F3-01 (telemetri), F3-05 (`BotCore` + test çatısı) — hepsi `KAPANDI` |
| İlgili gereksinim / kabul | CLI-05, CLI-08 (kısmen), `docs/13` §8; MET-ACT-02, MET-FAIR-01 altyapısı; AC-LRN-03 (aksiyonlar yalnızca gerçek handler üzerinden) |
| Tahmini büyüklük | M (12 dosya; 4'ü yalnızca proje/liste satırı) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun ilk gerçek aksiyonu: bir bot **gerçek `WIZ_MOVE` paketini `CUser::HandlePacket()` üzerinden** göndererek bir hedef noktaya yürür ve durur. Aksiyon sunucuya ulaşmadan önce `BotFairnessGuard` kuralından (hız alanı tavanı, "ışınlanma yok" adım sınırı) geçer; sonuç `ACTION_SUBMIT` / `ACTION_RESULT` / `FAIRNESS_REJECT` telemetri olaylarına yazılır. Karar katmanı yoktur: aksiyonu `/bot move` komutu tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

Bu plan F4'ün ilk dilimidir; `Attack`, `CastStart/Effect`, `UsePotion` vb. sonraki planlarda aynı `ActionExecutor` iskeletine eklenir (`docs/13` §5.2 `ActionType`).

## 2. Bağlam (okunması zorunlu)

- `docs/13` §2 (bileşen tablosu: `ActionExecutor`, `BotFairnessGuard`), §3.1 (tick IOCP thread'inde, aksiyonlar doğrudan `HandlePacket`), §5.2 (`Action`/`ActionResult` taslağı), §8 (sonuç eşleme).
- `docs/03` §14 (CLI tablosu): **CLI-05** (hız alanı, yavaşlatma/stun bot tarafında), **CLI-08** (yürünebilirlik; **bu planda yalnızca "ışınlanma yok" adım sınırı**, ızgara denetimi F5), **CLI-12** (speedhack paketi: bu planda yok).
- `docs/03` MEC-MOV-01..05 ve §14 ölçüm satırları: `WIZ_MOVE` `u16 x·10, z·10, y·10, i16 speed, u8 echo`; yürüyüş `speed=45` (≈ 4,5 m/s), koşu/sprint `speed=67` (≈ 6,7 m/s); sürekli hareket sırasında istemci **~1,5 sn'de bir** paket gönderir; durma paketi `speed=0, echo=0`, hareket `echo=3`. Sunucu mesafe/zaman denetlemez (MEC-MOV-03).
- `docs/16` §3.2 (olay tipleri `ACTION_SUBMIT`, `ACTION_RESULT`, `FAIRNESS_REJECT`; seviye `decisions`) ve §3.3.
- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` (bu planla birlikte yazıldı): kararlar ve gerekçeleri.
- `docs/adr/ADR-0005`, `ADR-0015`, `ADR-0016`.
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `6122256` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/CharacterMovementHandler.cpp:4-54` — `CUser::MoveProcess()`: `m_bWarp || isDead()` ise sessizce döner (`:7`); paketi `will_x, will_z, will_y, speed, echo` sırasıyla okur (`:15`); `m_sSpeed = speed; SpeedHackUser();` (`:18-19`); `GetMap()->IsValidPosition()` başarısızsa sessizce döner (`:21`); `SetPosition(real_x, real_y, real_z)` (`:33`); bölge değişiminde in/out paketleri (`:35-40`); `WIZ_MOVE` yayını (`:45-47`).
  - `GameServer/User.cpp:2896-2915` — `CUser::SpeedHackUser()`: GM değilse hız alanı üst sınırı **45** (varsayılan), **67** (warrior/mage/priest), **90** (rogue veya `COMMAND_CAPTAIN` fame); aşılırsa `Disconnect()` + sunucu duyurusu. **Bot bu sınıra asla yaklaşmamalı** (aşarsa botun oturumu kopar).
  - `GameServer/User.cpp:243-321` — `CUser::HandlePacket()`; oyun içi paketler `:304` sonrasındaki `switch`'te, `WIZ_MOVE` → `MoveProcess` (`:319-321`). Bot oturumu giriş sırasında aynı yoldan geçiyor: `GameServer/Bot/BotManager.cpp:1112-1113` ve `:1128-1129` (`s->m_pUser->HandlePacket(pkt)`).
  - `shared/SMDFile.cpp:194-198` — `IsValidPosition`: yalnızca harita sınırı (`x < Width && z < Height`).
  - `GameServer/Bot/BotManager.cpp:40` — `WriteBotLog()` **`static`**'tir (yalnızca bu `.cpp`'de görünür). `ActionExecutor` log yazmaz; sonuç yapısı döner, günlüğü `BotManager.cpp` yazar.
  - `GameServer/Bot/BotManager.cpp:549-585` — `ExecuteCommand()` fiil dağıtımı; `:1068-1176` — `TickSessions()`, `PHASE_IN_GAME` durumu `:1155-1168`; `:1233-1248` — `BeginDespawn()`; `:729-759` — `BuildStatusLines()`.
  - `GameServer/Bot/BotSession.h` / `.cpp` — oturum durumu; `ResetForRespawn()` `BotSession.cpp:24`.
  - `GameServer/Bot/Telemetry.h:76-77` — `Telemetry::Emit(level, ev, bot, name, fieldsJson, droppable)`; `IsEnabled(level)`.
  - `BotCore/Rng.h`, `Tests/BotCoreTests/MiniTest.h`, `RngTests.cpp` — saf mantık/test deseni.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotMotion.h`, yalnızca başlık, yalnızca standart kütüphane):** hız birimi dönüşümü, adım hesabı, sunucu hız sınırı yansıması ve `BotFairnessGuard` hareket kuralı (`CheckMoveStep`). Birim testleri.
2. **`ActionExecutor` (`GameServer/Bot/ActionExecutor.h/.cpp`):** `Move`/`Stop` aksiyonlarını `WIZ_MOVE` paketine çevirir, guard'dan geçirir, `CUser::HandlePacket()` ile işletir, sonucu doğrular (paket gerçekten konumu değiştirdi mi), telemetri olaylarını üretir.
3. **Oturum durumu (`BotSession`):** devam eden yürüyüşün hedefi/zamanlayıcısı.
4. **Komutlar (`BotManager`):** `move <bot> <x> <z> [speed]` ve `stop <bot>|all` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten); her `Tick()`'te yürüyüşü ilerletme; `list` satırına konum/hareket bilgisi.
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT`, `ACTION_RESULT`, `FAIRNESS_REJECT`.

**Kapsam dışı (yapılmayacak)**

- `Attack`, `CastStart/CastEffect`, `UsePotion`, `Sit`, `Regene`, `Party*`, `Chat`, `TargetHpReq` aksiyonları; CLI-01..04, CLI-06, CLI-07, CLI-09, CLI-11 (aksiyon hızı tavanı) kuralları: sonraki F4 planları.
- **CLI-08 yürünebilirlik ızgarası, yükseklik (`y`) sorgusu, yol bulma, engel/duvar denetimi:** F5. Bu planda `move` düz çizgi yürür ve `y`'yi **botun mevcut `y` değerinde tutar** (`GetSPosY()`); arazi yüksekliği bilinmez `[A]`. `move` yalnızca test komutudur; üretim davranışı değil.
- CLI-12 (`WIZ_SPEEDHACK_CHECK` periyodik paketi): yok. (Sunucu yalnızca istemci gönderirse kontrol eder, MEC-MOV-04; bot göndermediği için kontrol hiç çalışmaz.)
- Hız/yavaşlatma/stun/Wall of Iron buff'larının hareket hızına uygulanması (CLI-05'in ikinci yarısı): buff algısı yok; sonraki plan. Bu planda hız alanı sabit (varsayılan 45) veya komutla verilen değer.
- `WIZ_ROTATE` (yön paketi), zone değişimi, ölüm/regene yönetimi.
- `Perception`, `Brain`, karar kimliği (`decision_id` bu planda yalnızca oturum başına artan sayaç).
- `BotCore`'un `GameServer`'a **bağlanması** (`ProjectReference`/`.lib`): bu planda yoktur; `BotMotion.h` başlık-yalnızca olduğundan göreli `#include` ile kullanılır (bkz. ADR-0017 §Karar 3). Yeni üçüncü taraf kütüphane yok.
- Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez.
- `ChatHandler.cpp`'deki yardım/kullanım metinlerinin güncellenmesi (bilinmeyen fiil zaten komut çekirdeğine geçer; metinleri Claude doğrulamada günceller veya sonraki plana bırakır).
- Dokümanları (`docs/13`, `docs/16`, `docs/03`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotMotion.h` | yeni | Başlık-yalnızca; `<cstdint>`, `<cmath>`, `<algorithm>`; sunucu başlığı yok |
| `BotCore/BotCore.vcxproj` | değiştir | Yalnızca `<ClInclude Include="BotMotion.h" />` |
| `Tests/BotCoreTests/MotionTests.cpp` | yeni | `MiniTest.h` ile (bkz. §5.2) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | Yalnızca `<ClCompile Include="MotionTests.cpp" />` |
| `GameServer/Bot/ActionExecutor.h` | yeni | |
| `GameServer/Bot/ActionExecutor.cpp` | yeni | |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca yürüyüş durumu üyeleri |
| `GameServer/Bot/BotSession.cpp` | değiştir | Kurucu başlatıcıları + `ResetForRespawn()` sıfırlama |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandMove`, `CommandStop` bildirimleri |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, komutlar, `TickSessions()` çağrısı, `BeginDespawn()` temizliği, `BuildStatusLines()` |
| `GameServer/proj-GameServer.vcxproj` | değiştir | `ActionExecutor.cpp` / `.h` satırları |
| `GameServer/proj-GameServer.vcxproj.filters` | değiştir | Aynı iki dosya, mevcut `Bot` süzgeci |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-01 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotMotion.h` (saf mantık)

Tüm içerik `namespace BotCore` içinde, `inline`/`constexpr`. Yalnızca `<cstdint>`, `<cmath>`, `<algorithm>`. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez** (ADR-0016). Kaynak sabitleri yorumda `docs/03` §14 ölçümüne bağla; ölçülmemiş olanı `[A]` diye işaretle (yorumlar İngilizce, kısa).

```cpp
namespace BotCore
{
	constexpr int16_t  kWalkSpeedField   = 45;    // docs/03 CLI-05: measured walking speed field
	constexpr int16_t  kSprintSpeedField = 67;    // docs/03 CLI-05: sprint field (warrior/mage/priest server limit)
	constexpr uint32_t kMovePeriodMs     = 1500;  // docs/03 CLI-05: client sends WIZ_MOVE about every 1.5 s while moving
	constexpr float    kStopSlackMeters  = 0.05f; // quantisation slack: packet positions are multiples of 0.1 m

	// Speed field unit is 0.1 m/s (docs/03 CLI-05).
	inline float SpeedFieldToMps(int16_t speedField);                          // speedField / 10.0f
	// Distance a bot may cover in 'periodMs' at 'speedField'.
	inline float MaxStepMeters(int16_t speedField, uint32_t periodMs);         // mps * periodMs / 1000

	// Server-side speed field limit, mirrors CUser::SpeedHackUser() (GameServer/User.cpp:2896).
	// captainOrRogue -> 90; else warriorMagePriest -> 67; else 45.
	inline int16_t ServerSpeedLimit(bool captainOrRogue, bool warriorMagePriest);

	struct StepResult { float x; float z; bool arrived; };
	// Moves (x,z) toward (tx,tz) by at most maxStep metres along the straight line.
	// distance <= maxStep (or maxStep <= 0 with distance == 0) -> exactly (tx,tz), arrived = true.
	inline StepResult StepToward(float x, float z, float tx, float tz, float maxStep);

	// BotFairnessGuard movement rule (docs/13 s2, docs/03 CLI-05 / CLI-08 "no teleport").
	enum MoveVerdict
	{
		MOVE_OK = 0,
		MOVE_REJECT_SPEED_FIELD = 1,   // CLI-05: speed field negative or above the server limit
		MOVE_REJECT_STEP_TOO_LONG = 2  // CLI-08: step longer than the bot could have walked
	};
	// packetSpeedField : value that goes into the packet (0 for a stop packet).
	// movingSpeedField : speed the bot is walking at (45 or 67...); used for the step bound.
	// stepMeters       : distance between the current position and the packet position (>= 0).
	// elapsedMs        : time since the previous move packet; values below kMovePeriodMs count as kMovePeriodMs.
	inline MoveVerdict CheckMoveStep(int16_t packetSpeedField, int16_t movingSpeedField,
		int16_t serverLimit, float stepMeters, uint32_t elapsedMs);
}
```

`CheckMoveStep` kuralları (sırayla):

1. `packetSpeedField < 0 || packetSpeedField > serverLimit || movingSpeedField < 0 || movingSpeedField > serverLimit` → `MOVE_REJECT_SPEED_FIELD`.
2. `limit = MaxStepMeters(movingSpeedField, max(elapsedMs, kMovePeriodMs)) * 1.10f + 0.15f` (`%10` istemci/zamanlama toleransı + 0,15 m niceleme payı). `stepMeters > limit` → `MOVE_REJECT_STEP_TOO_LONG`.
3. Aksi halde `MOVE_OK`. (Durma paketi de aynı kuraldan geçer: `packetSpeedField = 0`, `movingSpeedField` yürüyüş hızı.)

**Birim testleri (`Tests/BotCoreTests/MotionTests.cpp`)**, `MiniTest.h` makro adlarıyla (`TEST_CASE`, `CHECK`, `CHECK_EQ`; `RngTests.cpp` deseni, include `<BotCore/BotMotion.h>`). Kayan nokta karşılaştırması için `CHECK(std::fabs(a - b) < 1e-4f)`; enum karşılaştırması için `int`'e çevir (`CHECK_EQ(int(v), int(BotCore::MOVE_OK))`). En az şu test durumları:

- `Motion_SpeedUnits`: `SpeedFieldToMps(45) == 4.5`, `(67) == 6.7`; `MaxStepMeters(45, 1500) == 6.75`.
- `Motion_ServerSpeedLimit`: `(false,false)=45`, `(false,true)=67`, `(true,false)=90`, `(true,true)=90`.
- `Motion_StepToward`: kısmi adım (3-4-5 üçgeni: (0,0)→(3,4), maxStep 2.5 → (1.5, 2.0), `arrived=false`); tam varış (`maxStep` ≥ mesafe → tam hedef, `arrived=true`); mesafe 0 → `arrived=true`, konum değişmez.
- `Motion_Guard_SpeedField`: `CheckMoveStep(100, 45, 67, 0,1500)` → `REJECT_SPEED_FIELD`; `(-1, ...)` → aynı; `(67, 67, 67, 6.0f, 1500)` → `OK`; `(90, 90, 67, ...)` → `REJECT_SPEED_FIELD`; `(90, 90, 90, ...)` → kural 1 geçer.
- `Motion_Guard_StepBound`: `(45,45,67, 6.75f, 1500)` → `OK`; `(45,45,67, 30.0f, 1500)` → `REJECT_STEP_TOO_LONG`; sınır: `limit = 6.75*1.10+0.15 = 7.575`; `7.5f` → `OK`, `7.7f` → `REJECT_STEP_TOO_LONG`; `elapsedMs = 100` (< 1500) → 1500 sayılır (aynı sınır); `elapsedMs = 3000` → sınır `13.5*1.10+0.15 = 15.0`: `14.9f` → `OK`.
- `Motion_Guard_StopPacket`: `(0, 45, 67, 4.0f, 1500)` → `OK` (durma paketi hâlâ yürüyüş hızıyla sınırlı), `(0, 45, 67, 30.0f, 1500)` → `REJECT_STEP_TOO_LONG`.

`BotCore/BotCore.vcxproj`'a `<ClInclude Include="BotMotion.h" />`, `BotCoreTests.vcxproj`'a `<ClCompile Include="MotionTests.cpp" />` ekle (mevcut `Rng` satırlarının yanına).

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'ye (hepsi "IOCP thread only", mevcut üyelerin biçiminde yorumla) ekle:

```cpp
	bool m_moveActive;                                     // IOCP thread only: a walk is in progress (ActionExecutor)
	float m_moveTargetX;                                   // IOCP thread only
	float m_moveTargetZ;                                   // IOCP thread only
	int16 m_moveSpeed;                                     // IOCP thread only: speed field of the walk (packet speed while walking)
	std::chrono::steady_clock::time_point m_moveLastSent;  // IOCP thread only: when the last WIZ_MOVE went out
	uint32 m_actionSeq;                                    // IOCP thread only: per-spawn counter used as decision_id
	uint32 m_movePackets;                                  // IOCP thread only: WIZ_MOVE packets sent in the current walk
```

`BotSession.cpp`: kurucunun başlatıcı listesine `m_moveActive(false), m_moveTargetX(0), m_moveTargetZ(0), m_moveSpeed(0), m_actionSeq(0), m_movePackets(0)`; `ResetForRespawn()` içinde aynı alanları sıfırla (`m_moveActive = false`, `m_actionSeq = 0`, `m_movePackets = 0`).

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

`#include "stdafx.h"` (`.cpp`'de ilk satır), `#include "ActionExecutor.h"`, `"BotSession.h"`, `"Telemetry.h"`, `"../../BotCore/BotMotion.h"`, `"../Map.h"` (yalnızca `IsValidPosition` için gerekirse; `MoveProcess` aynı çağrıyı `CharacterMovementHandler.cpp:21`'de yapıyor, `GetMap()` dönüş türüyle derlendiğini kontrol et).

```cpp
struct MoveOutcome
{
	enum Kind { NOTHING, SENT, ARRIVED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "ok", "not_in_game", "dead", "bad_target",
	                       // "speed_field", "step_too_long", "handler_noop"
};

class ActionExecutor
{
public:
	// All functions: IOCP thread only. 'now' is the tick time.

	// Validates and arms a walk to (tx,tz) at 'speedField'; sends nothing yet (the same Tick()'s TickMove() does).
	// REFUSED (nothing armed) when: session not in game / user dead ("not_in_game", "dead"),
	// target outside 0 <= x,z and x*10,z*10 <= 65535 or map->IsValidPosition(tx, tz, user y) false ("bad_target"),
	// speedField < 1 or above the server limit ("speed_field", with FAIRNESS_REJECT emitted, see below).
	static MoveOutcome BeginMove(BotSession * s, float tx, float tz, int16 speedField,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session; NOTHING unless s->m_moveActive and
	// now - s->m_moveLastSent >= kMovePeriodMs. Sends one walking packet or, when the remaining
	// distance <= the step, the final stop packet at the target (kind ARRIVED).
	static MoveOutcome TickMove(BotSession * s, std::chrono::steady_clock::time_point now);

	// Ends an active walk: sends a stop packet at the current position (kind ARRIVED, reason "ok")
	// and clears m_moveActive. NOTHING when no walk is active.
	static MoveOutcome StopMove(BotSession * s, std::chrono::steady_clock::time_point now);

	// Clears the walk state without sending anything (despawn).
	static void AbandonMove(BotSession * s);
};
```

Davranış ayrıntıları:

- **Sunucu hız sınırı:** `BotCore::ServerSpeedLimit(user->GetFame() == COMMAND_CAPTAIN || user->isRogue(), user->isWarrior() || user->isMage() || user->isPriest())` — `User.cpp:2903-2906` ile aynı sınıflandırma. (`COMMAND_CAPTAIN` `GameDefine.h:142`.)
- **`BeginMove`:** `m_moveActive = true`, hedefi/hızı yaz, `m_movePackets = 0`, `m_moveLastSent = now - std::chrono::milliseconds(BotCore::kMovePeriodMs)` (aynı `Tick()`'teki `TickMove` hemen ilk paketi göndersin). Zaten yürüyorsa yeni hedef eskisinin yerini alır (sessizce).
- **`TickMove` adım hesabı:** `elapsedMs = clamp(now - m_moveLastSent, kMovePeriodMs, 2 * kMovePeriodMs)` (duraksamada adım 2 periyodu aşmasın). `maxStep = MaxStepMeters(m_moveSpeed, elapsedMs)`. Konum: `user->GetX()`, `user->GetZ()` (botun **kendi** durumu; algı sözleşmesine aykırı değil). `StepToward` ile sonraki nokta `(nx, nz)`:
  - `arrived == false`: **yürüme paketi**: `speed = m_moveSpeed`, `echo = 3`.
  - `arrived == true`: **durma paketi**: `speed = 0`, `echo = 0`, konum = hedef; sonuç `ARRIVED`, `m_moveActive = false`.
- **Paket oluşturma:** `Packet pkt(WIZ_MOVE); pkt << uint16(will_x) << uint16(will_z) << uint16(will_y) << int16(speed) << uint8(echo);` — `will_x = uint16(nx * 10.0f + 0.5f)`, `will_z` aynı, `will_y = user->GetSPosY()` (`Unit.h:78`). Alan sırası `MoveProcess`'in okuma sırasıdır (`CharacterMovementHandler.cpp:15`): **x, z, y**, sonra hız, echo.
- **Guard (her paketten önce, sunucuya gitmeden):** `stepMeters` = botun mevcut konumu ile **niceleme sonrası** paket konumu (`will_x/10.0f`, `will_z/10.0f`) arasındaki mesafe. `BotCore::CheckMoveStep(speed, m_moveSpeed, limit, stepMeters, elapsedMs)` `MOVE_OK` değilse: paketi **gönderme**, `FAIRNESS_REJECT` yaz, yürüyüşü bırak (`m_moveActive = false`), `REFUSED` + `reason` (`"speed_field"` / `"step_too_long"`) dön. (`BeginMove`'daki hız denetimi de aynı fonksiyonu `stepMeters = 0` ile çağırır.)
- **Gönderim ve sonuç eşleme:** `user->HandlePacket(pkt)` (dönüş değeri yok sayılır). Hemen ardından doğrula: `user->isInGame()` ve `|user->GetX() - will_x/10.0f| < 0.05f` ve `|user->GetZ() - will_z/10.0f| < 0.05f`. Biri değilse (ölü, `m_bWarp`, geçersiz konum, kopma) → `FAILED`, `"handler_noop"`, `m_moveActive = false`. Başarılıysa `m_moveLastSent = now`, `m_movePackets++`, `SENT`/`ARRIVED`, `"ok"`.
- **Telemetri** (yalnızca `Telemetry::Instance().IsEnabled(TEL_DECISIONS)` iken biçimlendir; hepsi `bot` = `user->GetSocketID()`, `name` = `s->m_charName.c_str()`, `droppable = false`):
  - `ACTION_SUBMIT`: `"decision_id":N,"type":"Move","x":%.1f,"z":%.1f,"speed":%d,"echo":%u` (paketten hemen önce; `N = ++s->m_actionSeq`).
  - `ACTION_RESULT`: `"decision_id":N,"type":"Move","ok":true|false,"reason":"<reason>","latency_us":%lld` (`HandlePacket` süresi, `steady_clock`).
  - `FAIRNESS_REJECT`: `"decision_id":N,"type":"Move","rule":"CLI-05"|"CLI-08","reason":"speed_field"|"step_too_long","value":%.2f,"limit":%.2f` (`value`/`limit`: hız alanı için alan değerleri, adım için metre). Bu olaylarda paket gönderilmediği için `ACTION_SUBMIT`/`ACTION_RESULT` **yazılmaz** (MET-ACT-02 paydası yalnızca sunucuya verilen aksiyonlardır).
  - `reason` değerleri yukarıdaki sabit metinlerdir; kullanıcı girdisi JSON'a girmez.
- **`StopMove`:** hedef = botun mevcut konumu, `speed = 0, echo = 0`; guard `stepMeters = 0` ile geçer; aksi halde `TickMove`'un durma paketiyle aynı yol.
- **`AbandonMove`:** yalnızca `m_moveActive = false`.
- Thread: yalnızca IOCP thread'inde çağrılır; kilit, `Sleep`, `printf` yok. Konsola yazma yok; günlüğü `BotManager.cpp` yazar.

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.cpp` başına `#include "ActionExecutor.h"`. `BotManager.h`'ye özel bildirimler: `void CommandMove(const std::string & args);` ve `void CommandStop(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()` (`:549`):** `match`/`scenario` dallarının yanına `move` ve `stop` fiillerini ekle; "unknown command" günlük satırındaki listeyi `(spawn, despawn, list, match, scenario, move, stop)` yap. `RESPAWN_CYCLES != 0` reddi zaten başta (`:559-563`); dokunma.
2. **`CommandMove(args)`:** `<ad> <x> <z> [speed]` boşlukla ayrılmış (`std::istringstream` veya mevcut `Trim`/`SplitNames` yardımcıları; ad büyük/küçük harf duyarsız `FindSession`). Sayı ayrıştırma `strtod` + bitiş işaretçisi denetimiyle (`nan`/`inf`/artık karakter reddedilir); `speed` tamsayı, verilmezse `BotCore::kWalkSpeedField`. Hata durumlarında **tek** günlük satırı ve dön: eksik/bozuk argüman (`cmd move: usage: move <bot> <x> <z> [speed]`), bilinmeyen/spawn edilmemiş bot (`cmd move: unknown or not spawned bot '<ad>'`; ad günlüğe yalnızca bot tablosundaki geçerli bir ad değilse kaçışsız basılmaz: `IsKnownBotName`'e uymayan adlar için ad yerine `'?'`), oturum `PHASE_IN_GAME` değil. Sonra `ActionExecutor::BeginMove(...)`: `REFUSED` → `cmd move: <bot> refused (<reason>)`; aksi halde `cmd move: <bot> walking to (x, z) at speed N`.
3. **`CommandStop(args)`:** `all` veya bot adı. Her `PHASE_IN_GAME` oturum için `ActionExecutor::StopMove`; sonucu tek satırla günlükle (`cmd stop: <bot> stopped at (x, z)` / `cmd stop: <bot> not moving`).
4. **`TickSessions()` `PHASE_IN_GAME` dalı (`:1155-1168`):** mevcut `DESPAWN_AFTER` / `Update()` mantığına **dokunma**; dalın sonunda (despawn başlamadıysa) `ActionExecutor::TickMove(s, now)` çağır ve sonucu günlükle: `ARRIVED` → `bot <ad> arrived at (x, z) after N packets` (konum `s->m_pUser->GetX()/GetZ()`); `REFUSED`/`FAILED` → `bot <ad> move stopped (<reason>)`; `SENT`/`NOTHING` → günlük yok (spam yok; ayrıntı telemetride). `TickMove`'u çağırmadan önce `s->m_pUser->isDead()` ise `AbandonMove(s)` + `bot <ad> move stopped (dead)` günlüğü.
5. **`BeginDespawn()` (`:1233`):** `pUser->OnDisconnect()` çağrısından **önce** `ActionExecutor::AbandonMove(s)`.
6. **`BuildStatusLines()` (`:729-759`):** oturum satırının sonuna ek alanlar: `pos=<x>,<z> moving=<0|1> moverx=<N>`; `pos` yalnızca `s->m_pUser != nullptr` iken `%.1f,%.1f`, değilse `-`. `moverx` = `s->m_opcodeCount[WIZ_MOVE].load()` (botun **aldığı** `WIZ_MOVE` paketleri; başka botların hareketinin bölge yayını bu botun alıcısına ulaştığını gösterir). Mevcut alanların sırası ve biçimi değişmez; yeni alanlar sonuna eklenir. `char message[192]` tamponunu gerekirse büyüt (kesilme olmamalı).
7. Başka hiçbir yere dokunma (`ScenarioRunner` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj`: `<ClCompile Include="Bot\ActionExecutor.cpp" />` (`:193-196` bloğuna) ve `<ClInclude Include="Bot\ActionExecutor.h" />` (`:287-291` bloğuna). `.filters`: aynı iki dosya, mevcut `Bot` süzgeciyle (`:84-93` ve `:209-221` desenini kopyala). Yeni dosyalar yalnızca ASCII, CRLF.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (yeni uyarı yok; `BotMotion.h` ve `ActionExecutor.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Motion_*` testlerini içerir (en az 6 test durumu, §5.2) ve toplam test sayısı F3-05'teki 6'nın üzerinde.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotMotion.h` eşleşme vermez; dosyada `#include` yalnızca `<cstdint>`, `<cmath>`, `<algorithm>`.
- [ ] K5: `WIZ_MOVE` yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_MOVE" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma) ve `BotManager.cpp` (`m_opcodeCount[WIZ_MOVE]`, `BuildStatusLines`) satırlarını gösterir; `MoveProcess`/`SetPosition`/`m_curx` bot kodundan doğrudan çağrılmaz/yazılmaz (`grep -n "MoveProcess\|SetPosition\|m_curx\|m_curz" GameServer/Bot/` boş).
- [ ] K6: `ActionExecutor`'da guard atlanmıyor: `HandlePacket` çağrısından önce `CheckMoveStep` çağrısı ve `MOVE_OK` dışında erken dönüş vardır (kod okumasıyla; Claude doğrulamada çalışma zamanında da sınar).
- [ ] K7: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutlarla çalışır; `git diff gece/2026-10-02...bot/F4-01 -- GameServer/Bot/BotManager.cpp | grep '^-'` yalnızca bilinçli değiştirilen satırları gösterir (`unknown command` mesajı, `BuildStatusLines` biçim satırı); `Startup()`/`Tick()` akışı ve `ini` okuma değişmedi.
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F4-01` yalnızca §4'teki dosyaları gösterir.
- [ ] K9: yeni dosyalar ASCII + CRLF (`file BotCore/BotMotion.h Tests/BotCoreTests/MotionTests.cpp GameServer/Bot/ActionExecutor.*`); değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı).
- [ ] K10: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K11 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–6 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-01
git diff gece/2026-10-02...bot/F4-01 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-01 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotMotion.h
grep -n "WIZ_MOVE" GameServer/Bot/*.cpp
grep -n "MoveProcess\|SetPosition\|m_curx\|m_curz" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
file BotCore/BotMotion.h Tests/BotCoreTests/MotionTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir:

1. **Mutlu yol:** `spawn BotWP_K,BotMF_K` → iki `in game`; `list` ile başlangıç `pos=`; `move BotWP_K <x0+30> <z0>` (spawn konumundan 30 m) → loglarda `walking to`, ~7 sn içinde `bot BotWP_K arrived at (...) after N packets` (N = 5: 4 yürüme + 1 durma, ±1); `list`: `pos` hedefe ≤ 0,1 m, `moving=0`; **`BotMF_K` satırında `moverx` > 0** (gerçek `WIZ_MOVE` yayını bölgeye ulaştı). `Logs/bots/<tarih>/live-*.jsonl`: her paket için bir `ACTION_SUBMIT` + bir `ACTION_RESULT` (`decision_id` eşleşir, `ok:true`, `reason:"ok"`, `latency_us` < 5000); yürüme paketlerinde `speed` = 45, `echo` = 3, son paket `speed` = 0, `echo` = 0; ardışık paketlerin `t` farkı 1500–1700 ms (paket, 1500 ms dolduktan sonraki ilk tick'te çıkar; tick ≈ 110 ms), ardışık `x` farkı = 4,5 m/s × gerçek geçen süre ≈ 6,75–7,6 m (hiçbiri guard sınırı olan 1,10 × adım + 0,15 m'yi aşmaz, `FAIRNESS_REJECT` yok).
2. **`stop` ortada:** `move` sonrası ~2 sn'de `stop BotWP_K` → `stopped at (...)`, konum hedefe varmadan durur, `moving=0`, durma paketi (`speed` 0) telemetride; `stop all`, hareketsiz botta `not moving`.
3. **Guard reddi (sunucuya paket gitmez):** `move BotWP_K <x> <z> 100` (warrior sınırı 67) → `refused (speed_field)`, `FAIRNESS_REJECT` (`rule":"CLI-05"`, `value":100`, `limit":67`), **`ACTION_SUBMIT` yok**, bot oturumu **kopmaz** (`list`'te hâlâ `in_game`, `GameServer.log`'da speedhack/`Disconnect` satırı yok); `move BotWP_K <x> <z> 67` kabul edilir ve adımlar 6,7 m/s × gerçek geçen süre ≈ 10,05–11,4 m; `move ... 0` ve `move ... abc` → `usage`/`refused`.
4. **Reddedilen hedefler:** harita dışı (`move BotWP_K 7000 7000`, `-5 10`, `nan 3`) → `bad_target`/usage, paket yok; spawn edilmemiş/bilinmeyen ad → tek satır `unknown or not spawned`; `despawn` edilmiş bot → `not in game`.
5. **Yaşam döngüsü:** yürürken `despawn BotWP_K` → temiz despawn (`despawn complete`, `pool free` tam), durmuş yürüyüş devam etmez; aynı bot yeniden `spawn` → `moving=0`, `moverx` 0'dan başlar; yürürken `scenario`/`RESPAWN_CYCLES` yolları etkilenmez (`RESPAWN_CYCLES=2` iken `move` → `cmd rejected`).
6. **Gerilemesiz:** `ENABLED=0` → komut/log/dosya yok; `TELEMETRY=summary` iken `ACTION_*` olayı yazılmaz ama hareket çalışır; `PERF_SAMPLE` `tick_p95_us` F3-01 düzeyinde (16 bot yok, 2 bot yürürken p95 ≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. İnsan istemcisi gerekmez (görsel doğrulama `T-ARCH-06`, `docs/STATUS.md` "Proje sahibi testleri").

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. `proj-GameServer.vcxproj` ve `.filters` mevcut kodlama/satır sonlarını korusun (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). `move`/`stop` yalnızca `ENABLED=1` iken, üretim dışı test komutudur.
- **Thread kuralı (ADR-0005):** `ActionExecutor` yalnızca IOCP thread'inde (`Tick()` içinden) çalışır; `HandlePacket` zaten bu thread'de koşar. Başka thread'den (konsol, `+bot` işleyicisi) `BotSession`/`ActionExecutor`'a **dokunma**; komutlar `EnqueueCommand` kuyruğundan gelir.
- **`SpeedHackUser()` botu koparır:** hız alanı sınırı sınıfa bağlıdır (45/67/90). Guard bunu `CheckMoveStep` içinde zorunlu tutar; guard'ı **atlayan** (doğrudan `HandlePacket`/`MoveProcess` çağıran) hiçbir yol ekleme. `isGM()` bot hesabı varsayılmaz.
- **`WriteBotLog` `static`'tir:** `ActionExecutor.cpp`'ye taşıma/kopyalama yapma; sonuç yapısıyla `BotManager.cpp`'ye ilet. `Telemetry.h`'yi değiştirme.
- **`BotCore` başlığı sunucu başlığı içermez** (ADR-0016). `GameServer`'dan `BotCore`'a **göreli yol** ile (`"../../BotCore/BotMotion.h"`) include edilir; `ProjectReference` ekleme.
- **Bilinen sınırlar `[A]`:** adım periyodu 1,5 sn ve hız alanı 45/67 `docs/03` §14 ölçümlerinden; sunucu konumu her pakette hedef noktaya atlar (gerçek istemci de böyle gönderir, `docs/03` CLI-05 notu); `y` korunur (arazi yüksekliği F5). `move` düz çizgidir ve duvar/yükseklik denetlemez (CLI-08 ızgarası F5); test noktalarını düz ve boş seç.
- Telemetri hacmi küçüktür (bot başına ≤ 2 olay / 1,5 sn); `droppable = false` kullanılır, `decisions` seviyesi dışında hiçbir olay biçimlendirilmez (`IsEnabled` denetimi).
- `list` satırı biçimi (`despawns=` sonrası yeni alanlar) F3-04 yanıtını da etkiler; mevcut alan adlarını ve sırasını **değiştirme**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-01` — `<kısa-sha> [F4-01] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Test sonucu (`tools/run-tests.sh Release` son satırlar):
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
- İncelenen: `gece/2026-10-02...bot/F4-01` @ `<sha>`
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
