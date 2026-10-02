# F4-02: `ActionExecutor` saldırı dilimi — `Attack` (R) ve `BotFairnessGuard` CLI-01 / CLI-11 kuralları

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-02` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-01 (`ActionExecutor` iskeleti, guard deseni, `move`/`stop`) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-01, CLI-11, MEC-R-04, MEC-R-07; MET-ACT-02, MET-FAIR-01 altyapısı; AC-LRN-03 |
| Tahmini büyüklük | M (10 dosya; yeni `.cpp`/`.h` yok, `proj-GameServer.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun ikinci gerçek aksiyonu: bir bot **gerçek `WIZ_ATTACK` paketini `CUser::HandlePacket()` üzerinden** göndererek başka bir bota normal vuruş (R) yapar. Paket sunucuya gitmeden önce `BotFairnessGuard` kurallarından geçer: silah menzili (MEC-R-04), vuruş aralığı (CLI-01) ve bot başına aksiyon hızı tavanı (CLI-11, saniyede ≤ 6, hareket hariç). Sonuç, sunucunun bölgeye yayınladığı `WIZ_ATTACK` sonuç paketinden okunur (`ACTION_RESULT`; hasar bilgisine **bakılmaz**). Karar katmanı yoktur: aksiyonu `/bot attack` komutu tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün ikinci dilimidir (ADR-0017 Karar 4: hareket → **saldırı** → cast → pot → `Perception`).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` (bu planla birlikte "Ek (F4-02)" bölümü eklendi): kararlar.
- `plans/F4-01-aksiyon-yurutucu-hareket.md` §5.2–§5.5 ve Doğrulama Raporu: bu planın kalıpladığı iskelet (saf mantık `BotCore/*.h` + `ActionExecutor` + `BotSession` durumu + `BotManager` komutu).
- `docs/03` §14 (CLI tablosu ve ölçümler): **CLI-01** (aralık = `silah.Delay × 10 ms`, alt sınır 1,0 sn; `delaytime = Delay + 10`; ölçüm: 27 vuruşta 1640–1644 ms, `Delay=164`), **CLI-02** (R ile skill arasında kilit **yok**: bu planda ilgisiz, cast diliminde), **CLI-11** (≤ 6/sn), **T-MECH-CLIENT-02** (`WIZ_ATTACK`: `type=1, result=1` sabit, `distance` = hedefe mesafe × 10, `delaytime = Delay + 10`).
- `docs/03` MEC-R-01..09 (sunucu R kapıları), `docs/13` §5.2 (`Action`), §8 (sonuç eşleme: `WIZ_ATTACK` sonucu `decisionId` ile eşlenir), `docs/16` §3.2 (olaylar), MET-ACT-02.
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `a104ae3` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/AttackHandler.cpp:5-98` — `CUser::Attack()`: paket `u8 bType, u8 bResult, i16 tid, i16 delaytime, i16 distance` (`:11`); `isIncapacitated()` / `isInSafetyArea()` ise **sessizce** döner (`:16-20`); silah varsa ve mage değilse `delaytime < Delay + 10 || distance > m_sRange` → sessiz dönüş (`:25-31`), eli boşsa `delaytime < 100` → sessiz dönüş (`:33-34`); `isInAttackRange` (`:41`), `CanAttack` (`:41`), `isAttackable && CanCastRHit` (`:43`) başarısızsa **`ATTACK_FAIL` sonucu yayınlanır** (`:37`, `:95-97`); hedef `FREEZE` ise sessiz dönüş (`:50-51`); hasar > 0 ise `ATTACK_SUCCESS` veya `ATTACK_TARGET_DEAD` (`:77-84`). Sonuç `Packet result(WIZ_ATTACK, bType); result << bResult << GetSocketID() << tid; SendToRegion(&result);` (`:95-97`) → **saldıran dahil** bölgedeki herkese gider (`GameServer/Unit.cpp:807-810` `Send_Region(..., nullptr, ...)`).
  - `shared/globals.h:343-350` — `AttackResult`: `ATTACK_FAIL = 0`, `ATTACK_SUCCESS = 1`, `ATTACK_TARGET_DEAD = 2`, `ATTACK_TARGET_DEAD_OK = 3`, `MAGIC_ATTACK_TARGET_DEAD = 4`.
  - `GameServer/User.cpp:5047-5105` — `Unit::isInAttackRange()`: normal R için `15 + silah menzili` (metre, `isInRangeSlow`), yani sunucu menzili çok cömerttir; **bot guard'ı ise daha dardır** (bkz. §5.2: `distance` alanı ≤ `m_sRange`, 0,1 m biriminde).
  - `GameServer/Unit.cpp:855-875` — `Unit::CanAttack()` (aynı zone, canlı, blink değil, `isHostileTo`); `GameServer/Unit.cpp:1219-1260` — `CUser::isHostileTo()`: farklı ulus + `isInPVPZone()` → düşman. `GameServer/Unit.cpp:926-947` — `Unit::CanCastRHit()`: `UNIXTIME − son < PLAYER_R_HIT_REQUEST_INTERVAL (1.0, User.h:25)` ise ret; `UNIXTIME` 1 sn çözünürlüklü.
  - `GameServer/User.cpp:15-39` — `CUser::Send()` / `SendCompressed()`: `m_botSink` doluysa paketi **aynı çağrıda, aynı thread'de** `m_botSink->OnPacket()`'e verir. Yani `HandlePacket(WIZ_ATTACK)` dönmeden önce saldıran botun `BotSession::OnPacket()`'i sonuç paketini görmüştür.
  - `GameServer/User.h:563-570` — `GetItemPrototype(uint8 pos)` (`RIGHTHAND = 6`, `shared/globals.h:199`) → `_ITEM_TABLE *` (`m_sDelay`, `m_sRange`: `GameServer/GameDefine.h:307-308`); `GameServer/User.h:323` `isMage()`; `:431-432` `GetHealth()`/`GetMaxHealth()` (`int16 m_sHp`).
  - `GameServer/Bot/ActionExecutor.cpp` (tamamı, 274 satır) — `NextDecisionId`, `EmitFairnessReject` (şimdilik `"type":"Move"` sabit), `SubmitMove` deseni; `GameServer/Bot/BotSession.h:54-60` yürüyüş durumu; `GameServer/Bot/BotSession.cpp:15-26` `OnPacket()` ve `:26-43` `ResetForRespawn()`.
  - `GameServer/Bot/BotManager.cpp:591-631` `ExecuteCommand()` fiil dağıtımı; `:775-812` `BuildStatusLines()`; `:1008-1070` `CommandMove()`; `:1072-1158` `CommandStop()`; `:1360-1410` `TickSessions()` `PHASE_IN_GAME` dalı; `:1475-1478` `BeginDespawn()` (`AbandonMove` çağrısı).
  - `BotCore/BotMotion.h`, `Tests/BotCoreTests/MotionTests.cpp`, `MiniTest.h` — saf mantık / test deseni.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`, yalnızca başlık, yalnızca standart kütüphane):** vuruş aralığı, `delaytime` alanı, mesafe alanı, `CheckAttack` guard kuralı (menzil → aralık → aksiyon hızı) ve `ActionRateWindow` (CLI-11 kayan pencere). Birim testleri.
2. **`ActionExecutor` (`BeginAttack` / `TickAttack` / `EndAttack`):** `WIZ_ATTACK` paketini oluşturur, guard'dan geçirir, `CUser::HandlePacket()` ile işletir, sunucunun yayınladığı sonuç paketinden `hit` / `killed` / `srv_fail` / `no_result` eşler, telemetri olaylarını üretir. `FAIRNESS_REJECT` yardımcısı `type` alanını parametre alır (F4-01'de `"Move"` sabitti).
3. **Oturum durumu (`BotSession`):** devam eden saldırı serisi (hedef adı, kalan vuruş sayısı, son gönderim zamanı, sayaçlar), CLI-11 penceresi, `OnPacket()` içinde saldıranın kendi `WIZ_ATTACK` sonucunu kaydeden atomik alan.
4. **Komutlar (`BotManager`):** `attack <bot> <hedefbot> [adet]` ve `attack <bot>|all off` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten); her `Tick()`'te seriyi ilerletme; `list` satırına `hp=` ve `attacking=` alanları.
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT`, `ACTION_RESULT` (`"type":"Attack"`), `FAIRNESS_REJECT`.

**Kapsam dışı (yapılmayacak)**

- `CastStart/CastEffect` (skill vuruşu dahil), `UsePotion`, `Sit`, `Regene` (**ölü botu diriltme yok**), `Party*`, `Chat`, `TargetHpReq` aksiyonları; CLI-03, CLI-04, CLI-06, CLI-07, CLI-09 kuralları; **CLI-02** (R ile skill kilidi; F1 ölçümü kilit olmadığını gösterdi, cast diliminde "bağımsız zamanlayıcı" olarak uygulanır).
- Saldırı hızı buff'ının (`BUFF_TYPE_ATTACK_SPEED` vb.) vuruş aralığını kısaltması: buff algısı yok; aralık **üst sınır** `Delay × 10 ms` olarak kalır (buff yokken doğru). Sonraki plan.
- **Hedef seçimi, menzile yaklaşma, kovalama, yönelme (`WIZ_ROTATE`):** karar katmanı/`Perception`/navigasyon işi. `attack` yalnızca test komutudur; **bot menzil dışındaysa saldırmaz** (guard reddi), yürümez.
- **Hedef olarak NPC/canavar/gerçek oyuncu:** yalnızca spawn edilmiş **bot** hedef olarak desteklenir (`attack <bot> <hedefbot>`). `Perception` gelince hedef kimlikleri oradan beslenir (ADR-0017 Eki).
- Ölüm sonrası yaşam döngüsü: hedef ölürse seri biter (`killed`); botun yeniden doğması, ceza/`Regene` yönetimi sonraki F4 planı. Test kısa seri (≤ 5 vuruş) ile yapılır, botların HP'si 32000'dir (`db/002`).
- `ChatHandler.cpp`'deki `+bot` yardım/kullanım metinleri (bilinmeyen fiil zaten komut çekirdeğine geçer; Claude doğrulamada günceller).
- Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni `.cpp`/`.h` dosyası yalnızca `BotCore/` ve `Tests/` altında; **`GameServer` projesine dosya eklenmez**.
- Dokümanları (`docs/13`, `docs/16`, `docs/03`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | yeni | Başlık-yalnızca; `<cstdint>`, `<algorithm>`; sunucu başlığı yok |
| `BotCore/BotCore.vcxproj` | değiştir | Yalnızca `<ClInclude Include="BotCombat.h" />` |
| `Tests/BotCoreTests/CombatTests.cpp` | yeni | `MiniTest.h` ile (bkz. §5.2) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | Yalnızca `<ClCompile Include="CombatTests.cpp" />` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `AttackTarget`, `AttackOutcome`, üç yeni statik fonksiyon |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | Saldırı yolu; `EmitFairnessReject` `type` parametresi |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca saldırı durumu üyeleri ve `#include` |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi, `ResetForRespawn()`, `OnPacket()` içinde `WIZ_ATTACK` sonucu |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandAttack` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, komut, `TickSessions()` çağrısı, `BeginDespawn()` temizliği, `BuildStatusLines()` |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-02 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotCombat.h` (saf mantık)

Tüm içerik `namespace BotCore` içinde, `inline`/`constexpr`. Yalnızca `<algorithm>`, `<cstdint>`. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez** (ADR-0016). Kaynak sabitleri yorumda `docs/03` §14 ölçümüne bağla; ölçülmemiş olanı `[A]` diye işaretle (yorumlar İngilizce, kısa). `BotMotion.h`'nin biçimini (tab, Allman, `inline`) kopyala.

```cpp
namespace BotCore
{
	constexpr uint32_t kRMinIntervalMs      = 1000;  // docs/03 MEC-R-07: server allows one hit per second
	constexpr int16_t  kEmptyHandDelayField = 110;   // [A] server only needs delaytime >= 100 without a weapon
	constexpr int16_t  kEmptyHandRangeField = 20;    // [A] 2.0 m, same as a short weapon (distance field unit 0.1 m)
	constexpr int      kMaxActionsPerWindow = 6;     // docs/03 CLI-11: at most 6 non-move actions per second
	constexpr uint32_t kActionWindowMs      = 1000;

	// Minimum time between two R hits (docs/03 CLI-01: weapon Delay * 10 ms, never below 1 s).
	inline uint32_t AttackIntervalMs(bool hasWeapon, uint16_t weaponDelay);
	// 'delaytime' field of WIZ_ATTACK (docs/03 CLI-01 / T-MECH-CLIENT-02: Delay + 10; kEmptyHandDelayField without weapon).
	inline int16_t AttackDelayField(bool hasWeapon, uint16_t weaponDelay);
	// Weapon range as the 'distance' field limit (MEC-R-04: distance <= m_sRange; kEmptyHandRangeField without weapon).
	inline int16_t AttackRangeField(bool hasWeapon, uint16_t weaponRange);
	// 'distance' field of WIZ_ATTACK: metres * 10, truncated, clamped to 0..32767 (T-MECH-CLIENT-02).
	inline int16_t DistanceField(float meters);

	enum AttackVerdict
	{
		ATTACK_OK = 0,
		ATTACK_REJECT_OUT_OF_RANGE = 1,  // MEC-R-04: distance field above the weapon range
		ATTACK_REJECT_TOO_SOON = 2,      // CLI-01: previous R hit is younger than the interval
		ATTACK_REJECT_RATE = 3           // CLI-11: 6 actions already in the last second
	};

	// BotFairnessGuard rule for a normal attack. Checks in this order: range, interval, rate.
	// hasLast=false (no earlier hit in this series) skips the interval rule.
	// intervalMs below kRMinIntervalMs counts as kRMinIntervalMs.
	// actionsInWindow = ActionRateWindow::CountInWindow(now).
	inline AttackVerdict CheckAttack(bool hasLast, uint32_t sinceLastMs, uint32_t intervalMs,
		int16_t distanceField, int16_t rangeField, int actionsInWindow);

	// Sliding window over the last kMaxActionsPerWindow action timestamps (CLI-11). Time in ms, any epoch.
	class ActionRateWindow
	{
	public:
		ActionRateWindow();
		int CountInWindow(uint64_t nowMs) const;   // entries with nowMs - t < kActionWindowMs (t <= nowMs)
		void Record(uint64_t nowMs);               // overwrites the oldest entry once kMaxActionsPerWindow are stored
		void Clear();
	private:
		uint64_t m_times[kMaxActionsPerWindow];
		int m_count;   // entries stored, 0..kMaxActionsPerWindow
		int m_next;    // ring index of the next write
	};
}
```

Kurallar:

- `AttackIntervalMs`: `hasWeapon ? max(uint32(weaponDelay) * 10, kRMinIntervalMs) : kRMinIntervalMs`.
- `AttackDelayField`: `hasWeapon ? int16(weaponDelay + 10) : kEmptyHandDelayField`.
- `AttackRangeField`: `hasWeapon ? int16(weaponRange) : kEmptyHandRangeField`.
- `DistanceField`: `meters <= 0` → 0; `meters * 10 >= 32767` → 32767; aksi halde `int16(meters * 10.0f)` (kesme, yuvarlama değil).
- `CheckAttack` sırasıyla: (1) `distanceField < 0 || distanceField > rangeField` → `ATTACK_REJECT_OUT_OF_RANGE`; (2) `hasLast && sinceLastMs < max(intervalMs, kRMinIntervalMs)` → `ATTACK_REJECT_TOO_SOON`; (3) `actionsInWindow >= kMaxActionsPerWindow` → `ATTACK_REJECT_RATE`; aksi halde `ATTACK_OK`.
- `ActionRateWindow::CountInWindow(nowMs)`: depolanan her `t` için `nowMs >= t && nowMs - t < kActionWindowMs` ise say. `Record`: `m_times[m_next] = nowMs; m_next = (m_next + 1) % kMaxActionsPerWindow; m_count = min(m_count + 1, kMaxActionsPerWindow)`. `Clear`: `m_count = 0; m_next = 0`. Kurucu `Clear()` çağırır.

**Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)**, `MiniTest.h` makro adlarıyla (`TEST_CASE`, `CHECK`, `CHECK_EQ`; `MotionTests.cpp` deseni, `#include <BotCore/BotCombat.h>`). Enum/`uint`/`int16` karşılaştırmalarını `int`'e çevir (`CHECK_EQ(int(v), int(BotCore::ATTACK_OK))`). En az şu test durumları:

- `Combat_AttackInterval`: `(true,164)` → 1640; `(true,80)` → 1000 (alt sınır); `(true,100)` → 1000; `(false,0)` → 1000; `(true,300)` → 3000.
- `Combat_DelayAndRangeFields`: `AttackDelayField(true,164)` = 174; `(false,0)` = 110; `AttackRangeField(true,20)` = 20; `(true,35)` = 35; `(false,0)` = 20.
- `Combat_DistanceField`: `1.94f` → 19; `2.06f` → 20; `0.0f` → 0; `-3.0f` → 0; `5000.0f` → 32767 (kayan nokta sınırında kalan değerleri, örn. `1.9f`, **kullanma**).
- `Combat_Guard_Range`: `CheckAttack(false,0,1640, 19, 20, 0)` → `OK`; `(…, 20, 20, …)` → `OK`; `(…, 21, 20, …)` → `OUT_OF_RANGE`; `(…, -1, 20, …)` → `OUT_OF_RANGE`.
- `Combat_Guard_Interval`: `(true,1640,1640, 10,20,0)` → `OK`; `(true,1639,1640, …)` → `TOO_SOON`; `(false,0,1640, …)` → `OK` (ilk vuruş); `(true,900,500, …)` → `TOO_SOON` (alt sınır 1000 devrede); `(true,1000,500, …)` → `OK`.
- `Combat_Guard_Order`: menzil dışı ve çok erken birlikte (`(true,0,1640, 99,20, 6)`) → `OUT_OF_RANGE`; menzil içi, çok erken ve pencere dolu (`(true,0,1640, 10,20, 6)`) → `TOO_SOON`; yalnızca pencere dolu (`(true,5000,1640, 10,20, 6)`) → `RATE`; `actionsInWindow = 5` → `OK`.
- `Combat_RateWindow`: boş pencerede `CountInWindow(0) == 0`; `t = 0,100,200,300,400,500` altı kayıt → `CountInWindow(500) == 6`, `CountInWindow(999) == 6`, `CountInWindow(1000) == 5` (t=0 düştü), `CountInWindow(1500) == 1`, `CountInWindow(1600) == 0`; yedinci kayıt (`Record(1000)`) en eskinin (t=0) üzerine yazar → `CountInWindow(1000) == 6`; `Clear()` sonrası `CountInWindow(1000) == 0`; `nowMs < t` olan girdi sayılmaz (`Clear()`, sonra `Record(5000)`: `CountInWindow(4000) == 0`, `CountInWindow(5000) == 1`).

`BotCore/BotCore.vcxproj`'a `<ClInclude Include="BotCombat.h" />` (mevcut `BotMotion.h` satırının yanına), `BotCoreTests.vcxproj`'a `<ClCompile Include="CombatTests.cpp" />` (`MotionTests.cpp` satırının yanına) ekle.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'ye `#include "../../BotCore/BotCombat.h"` ve (hepsi "IOCP thread only", mevcut üyelerin biçiminde yorumla) ekle:

```cpp
	bool m_attackActive;                                   // IOCP thread only: an attack series is in progress
	std::string m_attackTargetName;                        // IOCP thread only: character name of the target bot
	uint32 m_attackLeft;                                   // IOCP thread only: hits still to send in this series
	bool m_attackHasLast;                                  // IOCP thread only: m_attackLastSent is valid for this series
	std::chrono::steady_clock::time_point m_attackLastSent;// IOCP thread only: when the last WIZ_ATTACK went out
	uint32 m_attackSent;                                   // IOCP thread only: WIZ_ATTACK packets sent in this series
	uint32 m_attackHits;                                   // IOCP thread only: of those, results hit/killed
	BotCore::ActionRateWindow m_actionWindow;              // IOCP thread only: CLI-11 window (non-move actions)

	std::atomic<uint64> m_attackEcho;                      // written by OnPacket() (same thread as HandlePacket for own hits)
```

`m_attackEcho` kodlaması (`OnPacket` yazar, `ActionExecutor` okur): `0` = yok; aksi halde `(1ull << 63) | (uint64(attackerId) << 32) | (uint64(tid) << 16) | uint64(bResult)`. Her saldırıdan **önce** `ActionExecutor` `0` yazar.

`BotSession.cpp`:

- Kurucu başlatıcı listesine: `m_attackActive(false), m_attackLeft(0), m_attackHasLast(false), m_attackSent(0), m_attackHits(0), m_attackEcho(0)` (`m_attackTargetName`, `m_actionWindow` varsayılan kurucu). Mevcut başlatıcı sırasını koru (`-Wreorder`: üyeler başlıkta bildirildiği sırayla başlatılmalı).
- `ResetForRespawn()`: `m_attackActive = false`, `m_attackTargetName.clear()`, `m_attackLeft = 0`, `m_attackHasLast = false`, `m_attackSent = 0`, `m_attackHits = 0`, `m_actionWindow.Clear()`, `m_attackEcho = 0`.
- `OnPacket()` (hâlâ **yalnızca atomik** dokunur): `opcode == WIZ_ATTACK` ve `pkt.size() >= 6` iken yük düzeni `u8 bType, u8 bResult, i16 attackerId, i16 tid` (`AttackHandler.cpp:95-96`); `m_attackEcho` içine yukarıdaki kodlamayla yaz. `ByteBuffer::read<T>(pos)` imzasını/`size()` adını `shared/ByteBuffer.h`'de doğrula (`BotSession.cpp:22` `pkt.read<uint8>(0)` kullanıyor); taşan okuma yapma.

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

`ActionExecutor.h`'ye `MoveOutcome`'un yanına:

```cpp
// Caller-supplied view of the target (ADR-0017 Ek F4-02). Temporary: the /bot attack test driver fills it
// from the target bot's session; the Perception slice replaces the source, not this struct.
struct AttackTarget
{
	int16 id;      // target's socket id (WIZ_ATTACK 'tid')
	float x;       // target position, metres
	float z;
};

struct AttackOutcome
{
	enum Kind { NOTHING, SENT, FINISHED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "ok", "hit", "killed", "srv_fail", "no_result",
	                       // "not_in_game", "dead", "bad_target", "out_of_range", "too_soon", "rate"
};
```

ve `ActionExecutor` sınıfına:

```cpp
	// Validates and arms an attack series of 'count' R hits on the target bot 'targetName'; sends nothing yet
	// (the same Tick()'s TickAttack() does). REFUSED (nothing armed) when the session is not in game / dead
	// ("not_in_game", "dead") or count < 1 / targetName empty ("bad_target").
	static AttackOutcome BeginAttack(BotSession * s, const std::string & targetName, uint32 count,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session; NOTHING unless s->m_attackActive and the CLI-01 interval
	// (or the series start) allows a hit. Sends at most one WIZ_ATTACK. 'target' is the caller's current view.
	// SENT: hit sent, series continues. FINISHED: last hit sent or target killed (reason "ok"/"hit"/"killed").
	// REFUSED: guard rejected, series dropped. FAILED: handler produced no result / dead attacker, series dropped.
	static AttackOutcome TickAttack(BotSession * s, const AttackTarget & target,
		std::chrono::steady_clock::time_point now);

	// Clears the attack state without sending anything (stop, despawn, target lost).
	static void EndAttack(BotSession * s);
```

(`#include <string>`.) Davranış ayrıntıları:

- **`BeginAttack`:** `m_attackActive = true`, `m_attackTargetName = targetName`, `m_attackLeft = count`, `m_attackHasLast = false`, `m_attackSent = 0`, `m_attackHits = 0`. Zaten bir seri varsa yenisi eskisinin yerini alır (sessizce). Dönüş `SENT`, `"ok"`. İlk paket aynı `Tick()`'te `TickAttack` ile gider (`m_attackHasLast = false` aralık kuralını atlar).
- **`TickAttack` ön koşullar:** `!m_attackActive` → `NOTHING`. `user == nullptr || !user->isInGame()` → seri düşer (`EndAttack`), `NOTHING`. `user->isDead()` → `EndAttack`, `FAILED`, `"dead"`.
- **Silah bilgisi:** `_ITEM_TABLE * weapon = user->GetItemPrototype(RIGHTHAND);` `hasWeapon = weapon != nullptr`; `delay = hasWeapon ? weapon->m_sDelay : 0`; `range = hasWeapon ? weapon->m_sRange : 0`. (Mage dahil aynı yol: sunucu mage için gecikme denetlemez ama istemci gerçekte silah gecikmesiyle gönderir; `[A]`.) `intervalMs = AttackIntervalMs(hasWeapon, delay)`.
- **Zamanlama (guard'dan önce, erken dönüş `NOTHING`):** `m_attackHasLast` ise ve `now - m_attackLastSent < intervalMs` ise henüz sıra yok → `NOTHING` (bu bir guard reddi **değildir**; guard'ın `TOO_SOON` kuralı yalnızca savunma katmanıdır ve bu planda çalışma zamanında tetiklenmemelidir). Böylece `FAIRNESS_REJECT` yalnızca gerçek ihlalde yazılır.
- **Mesafe/guard:** `dx = target.x - user->GetX()`, `dz = target.z - user->GetZ()`, `meters = sqrt(dx*dx + dz*dz)`, `distanceField = DistanceField(meters)`, `rangeField = AttackRangeField(hasWeapon, range)`. `sinceLastMs = m_attackHasLast ? elapsed : 0`. `nowMs` = `duration_cast<milliseconds>(now.time_since_epoch()).count()`. `verdict = CheckAttack(m_attackHasLast, sinceLastMs, intervalMs, distanceField, rangeField, m_actionWindow.CountInWindow(nowMs))`. `ATTACK_OK` değilse: paketi **gönderme**, `FAIRNESS_REJECT` yaz (`"type":"Attack","rule":"MEC-R-04"|"CLI-01"|"CLI-11","reason":"out_of_range"|"too_soon"|"rate","value":…,"limit":…`: menzilde `value = distanceField`, `limit = rangeField`; aralıkta `value = sinceLastMs`, `limit = max(intervalMs, kRMinIntervalMs)`; pencerede `value = count`, `limit = kMaxActionsPerWindow`), seriyi düşür (`EndAttack`), `REFUSED` + `reason` dön. Kural → metin eşlemesi: `OUT_OF_RANGE` → `"MEC-R-04"`/`"out_of_range"`; `TOO_SOON` → `"CLI-01"`/`"too_soon"`; `RATE` → `"CLI-11"`/`"rate"`.
- **Paket:** `Packet pkt(WIZ_ATTACK); pkt << uint8(1) << uint8(1) << int16(target.id) << AttackDelayField(hasWeapon, delay) << distanceField;` — alan sırası `AttackHandler.cpp:11` okuma sırasıdır (**bType, bResult, tid, delaytime, distance**); `type = 1`, `result = 1` istemcinin sabit gönderdiği değerlerdir (T-MECH-CLIENT-02). Sayı türlerini `AttackHandler.cpp:7`'deki `int16` ile uyumlu yaz (`int16`/`uint8`).
- **`ACTION_SUBMIT`** (yalnızca `IsEnabled(TEL_DECISIONS)`; paketten hemen önce): `"decision_id":N,"type":"Attack","target":<tid>,"distance":<distanceField>,"delay":<delayField>`; `N = NextDecisionId(s)` (F4-01 ile aynı sayaç).
- **Gönderim ve sonuç eşleme:** `s->m_attackEcho = 0;` → `t0` → `user->HandlePacket(pkt)` → `t1`. Sonra `uint64 echo = s->m_attackEcho.load()`: `echo` geçerli (bit 63) **ve** `attackerId == user->GetSocketID()` ise `bResult = echo & 0xFF`:
  - `ATTACK_SUCCESS (1)` → `ok = true`, `"hit"`;
  - `ATTACK_TARGET_DEAD (2)` veya `ATTACK_TARGET_DEAD_OK (3)` → `ok = true`, `"killed"`;
  - `ATTACK_FAIL (0)` (menzil/düşmanlık/`CanCastRHit` kapısı, hasar ≤ 0) → `ok = false`, `"srv_fail"`;
  - başka değer → `ok = false`, `"srv_fail"`.
  Geçerli `echo` yoksa (sunucu sessizce döndü: kör/ölü, güvenli alan, gecikme kapısı, `FREEZE`) → `ok = false`, `"no_result"`. **Hedefin HP'sine veya `GetHealth()`'ine bakılmaz** (hasar sunucunun bir alıcıya verdiği bilgi değildir; sonuç yalnızca yayınlanan sonuç paketidir).
- **Sayaçlar:** gönderilen her paket için `m_attackLastSent = now; m_attackHasLast = true; m_attackSent++; m_actionWindow.Record(nowMs); m_attackLeft--;` (`ok == true` ise `m_attackHits++`). `ACTION_RESULT`: `"decision_id":N,"type":"Attack","ok":true|false,"reason":"<reason>","result":<bResult veya -1>,"latency_us":%lld`.
- **Dönüş:** `no_result` → `FAILED`, `"no_result"`, seri düşer (`EndAttack`); `killed` → `FINISHED`, `"killed"`, seri biter; `m_attackLeft == 0` → `FINISHED`, reason = gönderilen son vuruşun sonucu (`"hit"` veya `"srv_fail"`); aksi halde `SENT`, reason = sonuç (`"hit"`/`"srv_fail"`); seri sürer. `srv_fail` seriyi **düşürmez** (geçici olabilir; adet sınırı sonlandırır).
- **`EndAttack`:** `m_attackActive = false; m_attackLeft = 0; m_attackHasLast = false;` (`m_attackSent`/`m_attackHits` korunur: log satırı sayıları okur; bir sonraki `BeginAttack` sıfırlar).
- `EmitFairnessReject(...)` imzasına `const char * type` parametresi ekle; mevcut iki çağrı (`SubmitMove`, `BeginMove`) `"Move"` geçer; JSON'da `"type":"` + type + `"`. Davranış/çıktı F4-01 ile aynı kalır.
- Thread: yalnızca IOCP thread'inde; kilit, `Sleep`, `printf` yok (`snprintf` yok: F4-01'deki `std::ostringstream`/`FormatFixed` deseni). Konsola/log dosyasına yazma yok; günlüğü `BotManager.cpp` yazar. `ActionExecutor`, `Unit`/`CUser` durumunu **doğrudan değiştirmez** (`m_RHitRepeatList`, HP, `m_sHp` vb. yazılmaz).

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirim: `void CommandAttack(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()` (`:591`):** `stop` dalının yanına `attack` fiilini ekle; "unknown command" günlük satırındaki listeyi `(spawn, despawn, list, match, scenario, move, stop, attack)` yap. `RESPAWN_CYCLES != 0` reddi zaten başta (`:601-605`); dokunma.
2. **`CommandAttack(args)`:** `SplitWords` ile ayrıştır. Biçimler: `attack <bot> <hedefbot> [adet]`, `attack <bot|all> off`. Hata durumlarında **tek** günlük satırı ve dön; kullanım satırı: `BotManager: cmd attack: usage: attack <bot> <target bot> [count] | attack <bot|all> off`.
   - **`off`:** `words.size() == 2 && _stricmp(words[1], "off") == 0`. `all` ise her `PHASE_IN_GAME` oturum için, değilse adı verilen oturum için (`FindSession`; bilinmeyen/spawn edilmemiş ad → `cmd attack: unknown or not spawned bot '<ad|?>'` `IsKnownBotName` kuralıyla, `PHASE_IN_GAME` değil → `cmd attack: <bot> not in game (phase X)`): seri aktifse `EndAttack` ve `cmd attack: <bot> stopped after N hit(s) sent` (`m_attackSent`), değilse `cmd attack: <bot> not attacking`. `all` için özet satırı: `cmd attack all: N stopped, M not attacking`.
   - **Başlatma:** `words.size() == 2 || 3` (ikincisi hedef adı, üçüncüsü isteğe bağlı adet). Saldıran: `FindSession`; yok/`PHASE_IN_GAME` değil → yukarıdaki iki mesaj. Hedef: `FindSession(words[1])`; yok → `cmd attack: unknown or not spawned bot '<ad|?>'` (hedef adı için de `IsKnownBotName` kuralı); `PHASE_IN_GAME` değil → `cmd attack: target <bot> not in game (phase X)`; hedef == saldıran (aynı oturum) → `cmd attack: <bot> refused (bad_target)`. Adet: `ParseIntStrict`, verilmezse 1; `adet < 1 || adet > 100` → kullanım satırı. Sonra `ActionExecutor::BeginAttack(s, hedefOturum->m_charName, adet, now)`: `REFUSED` → `cmd attack: <bot> refused (<reason>)`; aksi halde `cmd attack: <bot> attacking <hedef> (<adet> hit(s))`.
3. **`TickSessions()` `PHASE_IN_GAME` dalı (`:1374-1409`):** mevcut `TickMove` bloğundan **sonra** (aynı `if (s->m_phase == PHASE_IN_GAME)` içinde, `isDead()` dalının dışında/içinde ayrı): `if (s->m_attackActive)`:
   - `s->m_pUser->isDead()` ise: `EndAttack(s)` + `bot <ad> attack stopped (dead)` günlüğü.
   - Aksi halde hedef oturum: `BotSession * t = FindSession(s->m_attackTargetName.c_str());` `t == nullptr || t->m_phase != PHASE_IN_GAME || t->m_pUser == nullptr` ise `EndAttack(s)` + `bot <ad> attack stopped (target_lost)`. Aksi halde `AttackTarget tv = { (int16)t->m_pUser->GetSocketID(), t->m_pUser->GetX(), t->m_pUser->GetZ() };` (**test sürücüsü**: hedef konumu doğrudan hedef botun oturumundan okunur; kod yorumunda bunu belirt, `Perception` dilimi bunu değiştirecek) ve `AttackOutcome o = ActionExecutor::TickAttack(s, tv, now);`:
     - `FINISHED` → `bot <ad> attack finished (<reason>) after N hit(s) sent, M ok` (`m_attackSent`, `m_attackHits`);
     - `REFUSED` / `FAILED` → `bot <ad> attack stopped (<reason>)`;
     - `SENT` / `NOTHING` → günlük yok (spam yok; ayrıntı telemetride).
4. **`BeginDespawn()` (`:1475`):** `ActionExecutor::AbandonMove(s)` çağrısının yanına `ActionExecutor::EndAttack(s)`.
5. **`BuildStatusLines()` (`:775-812`):** oturum satırının sonuna ek alanlar: `hp=<h>/<mh> attacking=<0|1>`; `hp` yalnızca `s->m_pUser != nullptr` iken `%d/%d` (`GetHealth()`/`GetMaxHealth()`), değilse `-`. Mevcut alanların sırası ve biçimi değişmez; yeni alanlar sonuna eklenir (`moverx=…` sonrası). `char message[192]` tamponu yetmiyorsa büyüt (kesilme olmamalı; `snprintf` dönüşünü kontrol etmek zorunda değilsin ama çıktının kesilmediğini `list` ile gör).
6. Başka hiçbir yere dokunma (`ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj` ve `.filters` **değişmez** (yeni GameServer dosyası yok). `BotCore.vcxproj` ve `BotCoreTests.vcxproj` yalnızca birer satır alır (§5.2). Yeni dosyalar yalnızca ASCII, CRLF.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı yedi `Combat_*` testini içerir ve toplam test sayısı F4-01'deki 12'nin üzerinde (19); `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; dosyada `#include` yalnızca `<algorithm>` ve `<cstdint>`.
- [ ] K5: `WIZ_ATTACK` yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_ATTACK" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma) ve `BotSession.cpp` (`OnPacket` sonuç okuma) satırlarını gösterir; `grep -n "Attack(\|HpChange\|m_RHitRepeatList\|m_sHp" GameServer/Bot/` yalnızca `ActionExecutor.*` içindeki `AttackTarget`/`BeginAttack`/`TickAttack`/`EndAttack` tanımlarıyla `BotManager.cpp` çağrılarını gösterir; `CUser::Attack`, `HpChange`, `m_RHitRepeatList`, `m_sHp` bot kodundan doğrudan çağrılmaz/yazılmaz.
- [ ] K6: `ActionExecutor`'da guard atlanmıyor: `TickAttack`'ta `HandlePacket` çağrısından önce `CheckAttack` çağrısı ve `ATTACK_OK` dışında erken dönüş vardır (kod okumasıyla; Claude doğrulamada çalışma zamanında da sınar). `HandlePacket(pkt)` çağrısı `WIZ_ATTACK` için tek yerde.
- [ ] K7: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutlarla çalışır; `git diff gece/2026-10-02...bot/F4-02 -- GameServer/Bot/BotManager.cpp | grep '^-'` yalnızca bilinçli değiştirilen satırları gösterir (`unknown command` mesajı, `BuildStatusLines` biçim satırı); `Startup()`/`Tick()` akışı ve `ini` okuma değişmedi; `OnPacket()` dışındaki `BotSession` mantığı değişmedi.
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F4-02` yalnızca §4'teki 10 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*` değişmemiş.
- [ ] K9: yeni dosyalar ASCII + CRLF (`file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp`); değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `git diff --check` boş.
- [ ] K10: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K11: F4-01 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2 (guard hâlâ hareket yolunda); `EmitFairnessReject`'in iki hareket çağrısı `"Move"` geçiyor; `Motion_*` testleri hâlâ geçiyor.
- [ ] K12 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–6 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-02
git diff gece/2026-10-02...bot/F4-02 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-02 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "WIZ_ATTACK" GameServer/Bot/*.cpp
grep -n "Attack(\|HpChange\|m_RHitRepeatList\|m_sHp" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "CheckAttack\|HandlePacket" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-02
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir. Karus warrior `BotWP_K` ile El Morad warrior `BotWP_E` kullanılır (zone 71; farklı ulus, PVP bölgesi → düşman). Vuruş menzili 0,1 m biriminde `m_sRange` ile sınırlı olduğundan, saldırıdan önce `move BotWP_E <BotWP_K'nın x'i + 1> <z>` ile bot yan yana getirilir (`list` `pos=` ile teyit; mesafe ≤ silah menzili). Silah `Delay`/`Range` değerleri: bot silahının item numarası `tools/bot-gear-report.py` raporundan, `Delay`/`Range` item tablosundan `SELECT` ile alınır (`USERDATA`/yasak tablolar **okunmaz**); alınamazsa `ACTION_SUBMIT`'teki `delay` (= `Delay + 10`) alanından türetilir. Karşı ulus çiftinin birbirine düşman sayılması zone 71 haritasının `canAttackOtherNation()` bayrağına bağlıdır (`Unit.cpp:1291-1296`); `srv_fail` (`result:0`) ve `hp` değişmemesi görülürse bu, plan hatası değil bulgudur: doğrulayıcı nedeni (bayrak/menzil/`CanCastRHit`) çözümleyip raporlar.

1. **Mutlu yol:** `spawn BotWP_K,BotWP_E` → iki `in game`; yan yana getirme; `list`: `hp=` iki bot için de `32000/32000` (veya DB'deki dolu değer); `attack BotWP_K BotWP_E 3` → log `attacking BotWP_E (3 hit(s))`; ~4 sn (Delay×10 ms aralıkla 3 vuruş: ilk vuruş hemen, sonrakiler `Delay×10 ms` sonra) içinde `attack finished (hit) after 3 hit(s) sent, 3 ok`; `list`: `BotWP_E` `hp` düştü (< maks), `BotWP_K` `hp` değişmedi (karşı vuruş yok), `attacking=0`. JSONL (`Logs/bots/<tarih>/live-*.jsonl`): 3 `ACTION_SUBMIT` + 3 `ACTION_RESULT` (`decision_id` eşleşir, `"type":"Attack"`, `ok:true`, `reason:"hit"`, `result:1`, `latency_us` < 5000); `ACTION_SUBMIT` `delay` = silah `Delay + 10`, `distance` ≤ silah `Range`, `target` = `BotWP_E`'nin slot kimliği; ardışık `t` farkı ≥ `Delay × 10` ms ve ≤ `Delay × 10 + 250` ms; `FAIRNESS_REJECT` yok.
2. **`off` ortada:** `attack BotWP_K BotWP_E 10`, ~2 sn sonra `attack BotWP_K off` → `stopped after N hit(s) sent` (N 1–2), `attacking=0`, sonra yeni paket yok; `attack all off` hareketsiz botlarda `not attacking`, özet `0 stopped, 2 not attacking`.
3. **Guard reddi (sunucuya paket gitmez):** botlar birbirinden ≥ 10 m uzakta iken `attack BotWP_K BotWP_E 1` → `attack stopped (out_of_range)`; JSONL'de yalnızca `FAIRNESS_REJECT` (`"type":"Attack"`, `rule":"MEC-R-04"`, `reason":"out_of_range"`, `value` > `limit`), **`ACTION_SUBMIT` yok**, `BotWP_E` `hp` değişmez, `BotWP_K` oturumu kopmaz (`GameServer.log`'da yeni satır yok).
4. **Reddedilen komutlar:** `attack BotWP_K BotWP_K` → `refused (bad_target)`; `attack BotWP_K Nobody` ve `attack Ghost BotWP_E` → tek satır `unknown or not spawned bot '?'` (ad günlüğe girmedi); `attack BotWP_K BotWP_E 0`, `attack BotWP_K BotWP_E 101`, `attack BotWP_K BotWP_E abc`, argümansız → kullanım satırı; `despawn` edilmiş botla `not in game (phase despawned)`; hedef despawn edilmiş → `target ... not in game`; `RESPAWN_CYCLES=2` iken `attack` → `cmd rejected`.
5. **Ölüm ve yaşam döngüsü:** yürütülen seri sırasında hedef `despawn` → `attack stopped (target_lost)`; saldıran `despawn` → temiz despawn (`despawn complete`, `pool free` tam), despawn sonrası yeni paket yok; yeniden `spawn` → `attacking=0`, `hp` DB'deki son değer (bot satırları logout ile kaydedilir: doğrulama sonunda `db/002` ile `Hp=Mp=32000` geri yüklenir). Ölüm yolu (opsiyonel, süre izin verirse): hedefin `hp`'si küçültülmüşse (veya düşük HP'li bir bot varsa) `killed` sonucu ve `FINISHED` (`reason killed`) gözlenir; olmazsa raporda "gözlenmedi" yaz.
6. **Gerilemesiz:** `ENABLED=0` → komut/log/dosya yok; `TELEMETRY=summary` iken `ACTION_*` olayı yazılmaz ama saldırı çalışır (`hp` düşer); F4-01 `move`/`stop` yolu aynı çıktıyı verir (30 m yürüyüş `5 packets`, `moverx` sayısı korunur); `PERF_SAMPLE` `tick_p95_us` F4-01 düzeyinde (2 bot saldırırken p95 ≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. İnsan istemcisi gerekmez (görsel doğrulama `T-ARCH-07`, `docs/STATUS.md` "Proje sahibi testleri").

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. `BotCore.vcxproj`/`BotCoreTests.vcxproj` mevcut kodlama/satır sonlarını korusun (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). `attack` yalnızca `ENABLED=1` iken, üretim dışı test komutudur.
- **Thread kuralı (ADR-0005):** `ActionExecutor` yalnızca IOCP thread'inde (`Tick()` içinden) çalışır; `HandlePacket` zaten bu thread'de koşar. Konsol/`+bot` işleyicisi `BotSession`/`ActionExecutor`'a **dokunmaz**; komutlar `EnqueueCommand` kuyruğundan gelir. `OnPacket()` her thread'den çağrılabilir: orada yalnızca atomik alanlara yaz (`m_attackEcho`, mevcut sayaçlar); `std::string`/`std::chrono` üyelere dokunma.
- **Sonuç yalnızca yayınlanan sonuç paketinden okunur** (AC-LRN-03 / `docs/13` §8). Hedefin `GetHealth()`'i, `m_sHp`'si, hasar değeri aksiyon sonucunu belirlemek için **kullanılmaz**. `BuildStatusLines`'taki `hp=` yalnızca operatör çıktısıdır (botun kendi HP'si; karar girdisi değildir).
- **Test sürücüsü sınırı:** `TickSessions()` hedef konumunu hedef botun `CUser`'ından okur; bu üretim algısı değildir, `Perception` dilimi gelince (ADR-0017 Eki) yerini alır. Bu okumayı `ActionExecutor` içine **taşıma** (`AttackTarget` yapısı arayüzdür).
- **`UNIXTIME` 1 sn çözünürlüklüdür:** vuruşlar ≥ 1000 ms arayla gider (`kRMinIntervalMs`); sunucunun `CanCastRHit` kapısı bu aralıkta geçer. Aralığı 1000 ms'nin altına indirecek bir yol ekleme.
- **Silah menzili bot-taraflıdır:** sunucunun `isInAttackRange`'i 15 m + menzili kabul eder; guard bundan **çok daha dardır** (`distance ≤ m_sRange`, 0,1 m biriminde) çünkü gerçek istemci yalnızca silah menzilindeyken vurur (T-MECH-CLIENT-02). Bu kuralı gevşetme.
- **Bilinen sınırlar `[A]`:** eli boş / mage için `delaytime = 110` ve menzil 2,0 m varsayımı; saldırı hızı buff'ı yok (aralık tavan); hedef konumu 0,1 m niceleme ve tick (≈110 ms) gecikmesi yüzünden `distance` alanı birkaç cm eski olabilir; PK sonrası ölen bot yeniden doğurulmaz (kapsam dışı).
- Telemetri hacmi küçüktür (bot başına ≤ 2 olay / vuruş aralığı); `droppable = false`, `IsEnabled` denetimi hareketle aynı.
- `list` satırı biçimi (`moverx=` sonrası yeni alanlar) F3-04 yanıtını da etkiler; mevcut alan adlarını ve sırasını **değiştirme**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- **Durum:** UYGULANDI. Derleme (Release + Debug) hatasız; birim testleri `19 tests, 0 failed` (Release + Debug). Çalışma zamanı sunucusu **çalıştırılmadı** (plan §5.7 / §7: çalışma zamanı doğrulamasını Claude yapar).
- **Branch ve commit'ler:** `bot/F4-02` (taban: `gece/2026-10-02`). `ca146e9` — `[F4-02] saldiri dilimi: Attack (R) + BotFairnessGuard CLI-01/CLI-11` (uygulama + plan `Durum=UYGULANIYOR`); rapor/`UYGULANDI` commit'i bu raporla birlikte.
- **Değişen dosyalar ve nedenleri (yalnızca §4'teki 10 dosya + plan):**
  - `BotCore/BotCombat.h` (yeni): saf mantık — `AttackIntervalMs`, `AttackDelayField`, `AttackRangeField`, `DistanceField`, `CheckAttack` (menzil→aralık→hız), `ActionRateWindow`; yalnızca `<algorithm>`/`<cstdint>`, sunucu başlığı yok.
  - `BotCore/BotCore.vcxproj`: yalnızca `<ClInclude Include="BotCombat.h" />`.
  - `Tests/BotCoreTests/CombatTests.cpp` (yeni): yedi `Combat_*` testi (`MiniTest.h`).
  - `Tests/BotCoreTests/BotCoreTests.vcxproj`: yalnızca `<ClCompile Include="CombatTests.cpp" />`.
  - `GameServer/Bot/ActionExecutor.h`: `AttackTarget`, `AttackOutcome`, `BeginAttack`/`TickAttack`/`EndAttack`; `#include <string>`.
  - `GameServer/Bot/ActionExecutor.cpp`: saldırı yolu (`WIZ_ATTACK` + `HandlePacket`, sonuç `m_attackEcho`'dan, `FAIRNESS_REJECT` `type` parametreli); `EmitFairnessReject` imzasına `const char * type` eklendi, mevcut iki hareket çağrısı `"Move"` geçiyor.
  - `GameServer/Bot/BotSession.h/.cpp`: saldırı serisi durumu, `m_actionWindow`, atomik `m_attackEcho`; `OnPacket()` `WIZ_ATTACK` sonucunu (6 bayt) atoma yazar; `ResetForRespawn()` temizler; başlatıcı sırası bildirim sırasına uygun.
  - `GameServer/Bot/BotManager.h/.cpp`: `CommandAttack` (konsol/`BotCommands.txt`/`+bot` ortak çekirdek), `ExecuteCommand` `attack` fiili + "unknown command" listesi, `TickSessions()` saldırı ilerletme, `BeginDespawn()` `EndAttack`, `BuildStatusLines` `hp=`/`attacking=` alanları.
  - `plans/F4-02-...md`: `Durum` satırı ve bu rapor.
- **Derleme çıktısının son satırları:**
  - `./tools/build.sh Release`: `proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe` (rc=0; `BotCombat.h`/`ActionExecutor.cpp`/`BotSession.cpp`/`BotManager.cpp`/`CombatTests.cpp` için uyarı yok; kalan uyarılar yalnızca eski `GameServerDlg.cpp:816/1143/1802`).
  - `./tools/build.sh Debug`: `proj-GameServer.vcxproj -> ...\build\bin\x86-Debug\Server\GameServer.exe` (rc=0; aynı geçerli uyarılar).
  - `./tools/run-tests.sh Release`: `19 tests, 0 failed` (rc=0); `./tools/run-tests.sh Debug`: `19 tests, 0 failed` (rc=0).
- **Kriter öz-değerlendirmesi:**
  - K1 ✔ (Release rc=0, ilgili dosyalarda uyarı yok). K2 ✔ (Debug rc=0).
  - K3 ✔ (19 test; yedi `Combat_*`; Release+Debug rc=0).
  - K4 ✔ (`grep` eşleşme yok; include yalnızca `<algorithm>`/`<cstdint>`).
  - K5 ✔ (`WIZ_ATTACK` yalnızca `ActionExecutor.cpp:420` oluşturma + `BotSession.cpp:28` sonuç okuma; `Attack(`/`HpChange`/`m_RHitRepeatList`/`m_sHp` yalnızca yeni `ActionExecutor`/`CommandAttack` yollarında; `CUser::Attack`/`HpChange` çağrısı, `m_sHp` yazımı yok).
  - K6 ✔ (`CheckAttack` satır 375, `HandlePacket(pkt)` satır 426; `ATTACK_OK` dışında erken dönüş; saldırı `HandlePacket` çağrısı tek yerde).
  - K7 ✔ (yeni kod yalnızca `PHASE_IN_GAME` oturumlarında + komutlarda; `Startup()`/`Tick()`/ini/`OnPacket()` dışı `BotSession` mantığı değişmedi; kaldırılan tek satırlar `unknown command` metni ve `BuildStatusLines` biçim satırı, bkz. aşağıdaki `git diff` çıktısı).
  - K8 ✔ (`git diff --stat gece/2026-10-02...bot/F4-02` = §4'teki 10 dosya + plan; `proj-GameServer.vcxproj*` değişmedi).
  - K9 ✔ (`BotCombat.h`/`CombatTests.cpp` ASCII+CRLF; değiştirilen dosyaların CRLF/BOM durumu korundu; `git diff --check` boş).
  - K10 ✔ (`printf`/`Sleep`/`lock_guard`/`mutex`/`CreateThread`/`rand(` yok).
  - K11 ✔ (`CheckMoveStep` sayısı 2; iki hareket `EmitFairnessReject` çağrısı `"Move"` geçiyor; `Motion_*` testleri geçiyor).
  - K12: Claude'un `/plan-dogrula` çalışma zamanı adımı.
- **Plandan sapmalar:**
  1. `Combat_RateWindow` testinde plan `CountInWindow(1500) == 1` istiyor; ancak §5.2'de tanımlı `CountInWindow` koşulu `nowMs - t < kActionWindowMs` ile `{0,100,200,300,400,500}` kayıtlarında `1500 - 500 = 1000` **eşit** olduğundan sayı 0'dır. Kod §5.2'deki tanıma birebir uygulandı; testte plan değeri yerine `CountInWindow(1499) == 1` ve `CountInWindow(1500) == 0` yazıldı (aynı pencere davranışını sınar). Diğer plan değerleri (`500→6`, `999→6`, `1000→5`, `1600→0`, yedinci kayıt sonrası `1000→6`) aynen geçer ve `19 tests, 0 failed` alınır.
  2. `BeginAttack` imzasındaki `now` kullanılmadığından `(void)now;` yazıldı (imza plan gereği sabit; GameServer `Level3` olduğu için uyarı üretmez, yine de niyeti açık kılar).
  3. `TickAttack`'ta `no_result`/`killed` ayrımı `const char *` metin karşılaştırması yerine `result < 0` / `bool killed` ile yapıldı (aynı davranış; daha az kırılgan).
- **Açık sorular:** Yok (sapma 1 bir plan içi test-değeri tutarsızlığıdır; kod tanımı esas alındı ve raporda belgelendi; Claude doğrulamada test değerini isterse plana göre düzeltebilir).

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-02` @ `f43e822` (3 commit: `e3af982` plan, `ca146e9` uygulama, `f43e822` rapor; hepsi `[F4-02] ...` biçiminde, merge/force izi yok). Otonom gece modu (`AUTO_LOOP=1`): birleştirme/push döngü betiğinde, bu turda yapılmadı.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release derleme | ✔ | `./tools/build.sh Release` rc=0; `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` touch'lanıp yeniden derlendi: bu dosyalar için `warning`/`error` satırı 0 (`BotCombat.h` bunlardan include edilir) |
| K2 Debug derleme | ✔ | `./tools/build.sh Debug` rc=0, uyarı/hata satırı yok |
| K3 Birim testleri | ✔ | `tools/run-tests.sh Release` ve `Debug`: 7 `Combat_*` + 6 `Motion_*` + 6 `Rng_*` → `19 tests, 0 failed` |
| K4 `BotCombat.h` saflığı | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/"` boş; `#include` yalnızca `<algorithm>`, `<cstdint>` (`BotCombat.h:6-7`); dosya ASCII |
| K5 Paket yolu | ✔ | `grep WIZ_ATTACK GameServer/Bot/*.cpp`: yalnızca `ActionExecutor.cpp:420` (`Packet pkt(WIZ_ATTACK)`) ve `BotSession.cpp:28` (`OnPacket` sonuç okuma); `Attack(`/`HpChange`/`m_RHitRepeatList`/`m_sHp` grep'i yalnızca `BeginAttack`/`TickAttack`/`EndAttack` tanımları ve `BotManager.cpp` çağrılarını gösteriyor; `CUser::Attack`/`HpChange` çağrısı ve `m_sHp` yazımı yok |
| K6 Guard atlanmıyor | ✔ | `ActionExecutor.cpp:375` `CheckAttack`, `:376-402` `ATTACK_OK` dışında `EndAttack` + `REFUSED` ile erken dönüş; `HandlePacket` `:426` bundan sonra, saldırı için tek çağrı. Çalışma zamanında da sınandı (S3: `ACTION_SUBMIT` yok) |
| K7 `ENABLED=0` değişmez | ✔ | `git diff ... BotManager.cpp \| grep '^-'`: yalnızca `unknown command` metni ve `BuildStatusLines` biçim satırı (3 `-` satırı: 1 + 2); `Startup()`/`Tick()`/ini okuma değişmedi; `BotSession` değişikliği yalnızca yeni üyeler, `ResetForRespawn()` ve `OnPacket()` içi atomik yazım. Çalışma zamanı: `ENABLED=0` → `Bot_*.log` +0 satır, `BotCommands.txt` dokunulmadı, `Logs/bots/` oluşmadı |
| K8 Kapsam | ✔ | `git diff --stat`: §4'teki 10 dosya + plan dosyası; `proj-GameServer.vcxproj*` farkı 0 satır; `docs/`, `.claude/`, `AGENTS.md`, başka plan değişmemiş |
| K9 Kodlama | ✔ | `BotCombat.h`, `CombatTests.cpp`: `ASCII text, with CRLF`; değiştirilen `Bot/*` dosyaları CRLF/ASCII; `.vcxproj` dosyaları BOM + CRLF korunmuş, fark yalnızca birer `+` satır; `git diff --check` boş |
| K10 Yasaklı çağrılar | ✔ | `grep "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand("` `ActionExecutor.*` içinde boş |
| K11 F4-01 gerilemesiz | ✔ | `CheckMoveStep` sayısı 2; `EmitFairnessReject` iki hareket çağrısı `"Move"` geçiyor (`:94`, `:187`); `Motion_*` 6/6 geçti; çalışma zamanı: 30 m yürüyüş `arrived ... after 5 packets`, `moverx=5` |
| K12 Çalışma zamanı | ✔ | S1–S6 aşağıda; hepsi geçti (Release, sunucu 3/3 UP, sonunda 0/3 DOWN, ini md5 geri yüklendi) |

- Çalışma zamanı sınaması (`ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`; `BotWP_K` Karus warrior, `BotWP_E` El Morad warrior, zone 71; silah `Delay=164` (`delay` alanı 174), eski `Logs/bots/` `Logs\bots_old_f402\`'e taşındı):
  - **S1 mutlu yol:** `spawn` → iki `in game`, `list`: `hp=5650/5650` (plan 32000 diyordu; sunucu girişte kırpıyor, hata değil). Botlar zaten 1,77 m ayrıktı (`distance` alanı 17 ≤ menzil). `attack BotWP_K BotWP_E 3` → `attacking BotWP_E (3 hit(s))`, `attack finished (hit) after 3 hit(s) sent, 3 ok`; `BotWP_E` `hp` 5650 → 5270, `BotWP_K` 5650 (karşı vuruş yok), `attacking=0`. JSONL: 3 `ACTION_SUBMIT` + 3 `ACTION_RESULT` (`decision_id` 1–3 eşleşti, `"type":"Attack"`, `ok:true`, `reason:"hit"`, `result:1`, `latency_us` 95–121), `target` = 2985 (`BotWP_E` slotu), `delay:174`; ardışık `t` farkı 1647 / 1644 ms (aralık [1640, 1890] içinde); `FAIRNESS_REJECT` yok.
  - **S2 `off` ortada:** `attack ... 10`, ~2 sn sonra `attack BotWP_K off` → `stopped after 2 hit(s) sent`, yeni paket yok; `attack all off` → `BotWP_K not attacking`, `BotWP_E not attacking`, `0 stopped, 2 not attacking`.
  - **S3 guard reddi:** `BotWP_E` 14,8 m uzağa yürütüldü (`pos=1290.0,890.0`); `attack BotWP_K BotWP_E 1` → `attack stopped (out_of_range)`; JSONL'de yalnızca `FAIRNESS_REJECT` (`"type":"Attack"`, `"rule":"MEC-R-04"`, `"reason":"out_of_range"`, `value:148.00`, `limit:20.00`), **`ACTION_SUBMIT` yok**, `BotWP_E` `hp` değişmedi, oturum kopmadı, `GameServer.log` 32 satır (değişmedi), `Cheat_*.log` boş.
  - **S4 reddedilen komutlar:** `attack BotWP_K BotWP_K` → `refused (bad_target)`; `Nobody`, `Ghost`, `%s` → tek satır `unknown or not spawned bot '?'` (ad günlüğe girmedi); `0`, `101`, `abc`, argümansız, tek argüman, 4 argüman → kullanım satırı; despawn edilmiş saldıran → `not in game (phase despawned)`, despawn edilmiş hedef → `target BotMF_K not in game (phase despawned)`; hiçbirinde JSONL olayı yok. `RESPAWN_CYCLES=2` → `cmd rejected` yolu F2-05 başındaki kapıdır (`ExecuteCommand`, bu planda değişmedi; çalışma zamanında yeniden sınanmadı).
  - **S5 yaşam döngüsü:** seri sürerken hedef `despawn` → `attack stopped (target_lost)`; saldıran `despawn` → `despawned (... names cleared yes)`, sonra JSONL satır sayısı değişmedi (yeni paket yok), `pool free` doğru; yeniden `spawn` → `attacking=0`, `hp` DB'deki son değer (4728/5650). Ters yön (El Morad → Karus) da çalıştı (`BotWP_K` `hp` 5650 → 5395). **Ölüm yolu gözlendi:** `attack BotWP_K BotWP_E 30` → 30/30 `hit` (`attack finished (hit) after 30 hit(s) sent, 30 ok`), `hp` 4728 → 842; ardından `attack ... 10` → vuruş 2 `ok:false, reason:"srv_fail", result:0` (seri **düşmedi**, tasarım gereği), vuruş 8 `ok:true, reason:"killed", result:2` → `attack finished (killed) after 8 hit(s) sent, 7 ok`, `list`: `hp=0/5650`.
  - **S6 gerilemesiz:** `TELEMETRY=summary` (yeni oturum, `BotWP_K` → `BotPHD_E` 3 vuruş) → `live-*.jsonl`'de `ACTION_*`/`FAIRNESS_*` 0 satır, `hp` 3491 → 3211; aynı oturumda 30 m `move` → `after 5 packets`, `moverx=5`; `ENABLED=0` → bot logu +0 satır, `BotCommands.txt` yerinde, `Logs/bots/` yok; `PERF_SAMPLE` `tick_p95_us` en çok 308 µs (`decisions`, 2 bot saldırırken) / 448 µs (`summary`), `skipped_ticks` 0; `GameServer.log` +0 satır; toplam `decisions` koşusunda 53 `ACTION_SUBMIT` = 53 `ACTION_RESULT`, 1 `FAIRNESS_REJECT`.
  - Temizlik: ini yedekten geri yüklendi (md5 `d16463283c0d41074a2d8b6ec4aee203`, önce/sonra aynı), `BotCommands.*` kalmadı, sunucular kapalı. Bot satırları (`BotWP_K/E`, `BotMF_K`, `BotPHD_E`) `Hp=32000` ve `BotWP_E` konumu (1274,0, 890,0) hedefli `UPDATE` ile geri getirildi (yalnızca bu dört bot satırı; kişisel veri tablosu okunmadı). Test artıkları depo dışında `C:\dev\fdp\server\Logs\bots_old_f402\`.
- Bulgular (hepsi not, engel değil):
  1. Kod planla birebir uyumlu; uygulayıcı raporundaki iddialar (commit listesi, dosya listesi, derleme/test çıktısı) gerçekle uyuşuyor. Sapma 1 (`Combat_RateWindow`: `CountInWindow(1500)` plandaki 1 yerine `1499 → 1`, `1500 → 0`) **plan hatasıydı**: `nowMs - t < kActionWindowMs` tanımına göre `1500 - 500 = 1000` sayılmaz; uygulayıcının kodu tanıma uyuyor, karar doğru. Sapma 2 (`(void)now`) ve 3 (`result < 0` ile `no_result` ayrımı) kabul edildi.
  2. `list` botların `hp`'sini 32000 değil sunucunun kırptığı değerle (`5650/5650`) gösterir; plan §7 S1'deki "32000" beklentisi yanlıştı, kod etkilenmez.
  3. Hedef canlıyken bir vuruş `ATTACK_FAIL` (`srv_fail`, `result:0`) döndü (öldürme serisinin 2. vuruşu); guard bunu seri sonu saymıyor (plan §5.4) ve sonraki vuruşlar `hit`/`killed` geldi. Öldüren vuruş `result:2` ile geldi `[V]`. `srv_fail` nedenini ayırt eden alan yok (hasarsız vuruş/kaçırma olası) `[A]`.
  4. `+bot` yardım metni (`ChatHandler.cpp:84`, `:1198`) `move`/`stop`/`attack` fiillerini listelemiyor; kod `GameServer/` altında olduğundan Claude değiştirmedi: `docs/KNOWN_ISSUES.md` KI-012.
  5. Ölü bot (`BotWP_E hp=0`) yeniden doğurulmuyor (kapsam dışı, `Regene` sonraki plan); logout kaydı `hp=0` yazar, doğrulama sonunda DB geri yüklendi (yukarıda).
- Düzeltme talimatı: yok (DOĞRULANDI).
