# F4-07: `ActionExecutor` yeniden doğuş dilimi — `Regene` ve `BotFairnessGuard` CLI-14 kuralları

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-07` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-06 (hedef HP dilimi, `OnPacket()` ekleme kalıbı, `m_actionWindow`) — `KAPANDI`; F4-05 (`SetStance` iskeleti) — `KAPANDI`; F4-02 (ölen botu üreten `attack` serisi) — `KAPANDI`; F4-01 — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-14 (`docs/03` §14, bu planla birlikte eklendi), CLI-11, MEC-DTH-05..09, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı |
| Tahmini büyüklük | M (8 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun yedinci gerçek aksiyonu: ölü bir bot **gerçek `WIZ_REGENE` paketi** (tip 1) ile `CUser::HandlePacket()` üzerinden yeniden doğar; sunucunun cevabı (`WIZ_REGENE`: yeni konum) botun alıcısına gelir ve sonuç oradan okunur. Şimdiye kadar ölen bot (F4-02/F4-06 testlerinde görüldü) ölü kalıyordu ve DB'ye `Hp=0` ile kaydedilip ölü açılıyordu (F4-06 Doğrulama bulgusu 3). Sunucu `Regene`'de **hiçbir bekleme denetimi yapmaz**, yani guard olmazsa bot ölür ölmez aynı tick'te doğabilirdi; insan ölüm animasyonunu görüp düğmeye basmak zorundadır. Bu yüzden paket sunucuya gitmeden önce `BotFairnessGuard` kurallarından geçer: ölümden ≥ 3000 ms sonra (`dead_wait`, CLI-14 `[A]`), CLI-11 aksiyon hızı. Ek olarak NP'si 0 olan bot için `Regene` **gönderilmez** (`no_np`: sunucu bu oyuncuyu doğuş sonunda ana zone'a ışınlar, bot oturumu bunu desteklemez; KI-013). Karar katmanı yoktur: aksiyonu `/bot regene <bot>` komutu tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün yedinci dilimidir (ADR-0017 Ek F4-07; hareket → saldırı → cast → pot → duruş → hedef HP → **`Regene`** → `Party`/`Chat` → `Perception`).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-07)" (bu planla birlikte yazıldı): paket düzenleri, guard kuralları, `no_np` gerekçesi, ölüm anı izleme, sonuç eşlemesi, kapsam.
- `docs/03` §8 **MEC-DTH-05..09** (respawn kuralları), §14 **CLI-14** (bu planla birlikte eklendi), **CLI-11**.
- `plans/F4-06-aksiyon-yurutucu-hedef-hp.md` §5.2–§5.5 ve Doğrulama Raporu: bu planın kalıpladığı iskelet (saf mantık `BotCore/BotCombat.h` + `ActionExecutor` + `BotSession` durumu + `BotManager` komutu). **Yazılı planı değil, birleşmiş kodu esas al** (`ActionExecutor.cpp` `NextDecisionId` `:36`, `EmitFairnessReject` `:53`, `SetStance` `:1300`, `RejectTargetHp`/`RequestTargetHp` dosya sonunda).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `f8a74b1` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:334-336` — `HandlePacket` `case WIZ_REGENE`: `Regene(pkt.read<uint8>())`. Önceki kapılar (kripto, hesap, `m_bSelectedCharacter`) bot için geçilmiştir; ölüyken paket reddedilmez.
  - `GameServer/AttackHandler.cpp:100-245` — `CUser::Regene(regene_type, magicid = 0)`: `!isDead()` ise **sessizce döner** (`:108-109`); `regene_type` 1 veya 2 dışındaysa 1'e çevrilir; tip 2 `RobItem(379006000, 3 * level)` ister (bot **kullanmaz**); `m_StartPositionArray.GetData(GetZoneID())` yoksa döner; `UserInOut(INOUT_OUT)`; bind nesnesi (`m_sBind = -1` botlarda yok) yoksa zone 71'de `GetStartPosition(sx, sz)` (Karus (1380, 1090), El Morad (630, 920), +rand) → `SetPosition`; `m_bResHpType = USER_STANDING`; **`Packet result(WIZ_REGENE); result << GetSPosX() << GetSPosZ() << GetSPosY(); Send(&result);` (`:196-198`)**: cevap yükü **6 bayt**: `u16 x·10 @0, u16 z·10 @2, u16 y·10 @4` (`GameServer/Unit.h:77-79`; `CUser::Send` botun alıcısına yönlendirir); `BlinkStart()` (Ronark'ta `canAttackOtherNation()` olduğundan hemen döner, MEC-DTH-08); `Send_AIServer(AG_USER_REGENE)`; `SetRegion`, `UserInOut(INOUT_RESPAWN)`, `RegionUserInOutForMe`, `RegionNpcInfoForMe`; `HpChange(GetMaxHealth())` (MP **dolmaz**, MEC-DTH-07); **`:240-243` `GetLoyalty() == 0 && (isWarZone() || isInPKZone())` ise `KickOutZoneUser()`** (`User.cpp:4686-4721`: ana zone'a `ZoneChange`).
  - `GameServer/User.h:309` — `isDead()` = `m_bResHpType == USER_DEAD || m_sHp <= 0`; `User.h:427` `GetLoyalty()` (`uint32`); `User.h:377` `isInPKZone()` (zone 71 dahil).
  - `GameServer/Unit.cpp:959-963` — ölüm: `WIZ_DEAD` (`u16 id`) `SendToRegion` ile yayınlanır; bot bunu **kullanmaz** (ölüm anı yürütücüde izlenir, ADR-0017 Ek F4-07 madde 4).
  - `GameServer/Bot/BotManager.cpp:2018-2030` — `TickSessions()` içinde ölü bot dalı (`if (s->m_pUser->isDead()) { if (s->m_moveActive) { ... } } else { TickMove ... }`): bu planın `m_deadSeen` kaydını ekleyeceği yer. `:591-640` `ExecuteCommand` (fiil dağıtımı, `unknown command` listesi `:640`), `:1672` `CommandStance`, `:1724` `CommandTarget` (komut kalıbı).
  - `GameServer/Bot/BotSession.cpp:12-27` başlatıcı listesi, `:29-` `OnPacket()`, `:100-` `ResetForRespawn()`.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** `kRegeneMinDeadMs`, `RegeneCheck`, `RegeneVerdict`, `CheckRegene`. Birim testleri (`CombatTests.cpp`).
2. **`ActionExecutor::RequestRegene(s, now)`** (tek seferlik aksiyon): ön koşullar → `BotCore::CheckRegene` → `WIZ_REGENE` paketi (tip 1) → `HandlePacket` → yayınlanan cevaptan sonuç eşleme → telemetri.
3. **Oturum durumu (`BotSession`):** `m_deadSeen` / `m_deadSince` (ölüm anı, IOCP thread) ve `OnPacket()`'in doldurduğu `m_regeneEcho`; `TickSessions()` ölü botta kaydı tutar, canlıda siler.
4. **Komut (`BotManager`):** `regene <bot>` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten).
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT` / `ACTION_RESULT` (`"type":"Regene"`), `FAIRNESS_REJECT` (`"type":"Regene"`).

**Kapsam dışı (yapılmayacak)**

- **Ne zaman doğulacağı** (karar katmanı; `docs/09` §9 "diriltme bekleme", P-HD kararı, F6): yok. `regene` komutu ölüm bekleme/NP/hız dışında hiçbir koşula bakmaz.
- **Tip 2 (taşla) doğuş, diriltme skill'i (`RESURRECTION`, `Regene(1, skill)`), `/town`:** yok. Paketin tip baytı her zaman **1**'dir; tip parametresi yoktur.
- **NP'si 0 olan botun kurtarılması** (NP yazma, zone değiştirme desteği): yok; `no_np` reddi ve KI-013 yeterlidir.
- **Doğuş sonrası davranış** (güvenli bölgeye yürüme, MP potu, takıma dönüş): yok; bot doğduğu noktada kalır.
- **Ölümü yayınlanan `WIZ_DEAD` paketinden okumak:** yok (`OnPacket()`'te `WIZ_DEAD` bloğu **eklenmez**).
- `ChatHandler.cpp` `+bot` yardım metni (KI-012; Claude doğrulamada günceller). Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya yok; **`GameServer` projesine dosya eklenmez**. `Telemetry.*`, `ScenarioRunner.*` değişmez; `list` satırı değişmez.
- Dokümanları (`docs/03`, `docs/13`, `docs/16`, `docs/15`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | Yalnızca ekleme (§5.2); mevcut içerik ve `#include`'lar değişmez |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | Yalnızca ekleme: iki yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `RegeneOutcome`, bir yeni statik fonksiyon |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | Yalnızca dosya sonuna ekleme (§5.4); başka hareket/saldırı/cast/pot/duruş/hedef HP kodu değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca ölüm izleme ve `m_regeneEcho` üyeleri |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi, `ResetForRespawn()` ve `OnPacket()`'e **ekleme** bloğu (mevcut bloklar değişmez) |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandRegene` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, komut, `unknown command` listesi, `TickSessions()` ölü bot kaydı (iki ekleme satırı) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (Yeni dosya açılmaz: guard `BotCombat.h`'ye eklenir, böylece `BotCore*.vcxproj` değişmez.)

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-07 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotCombat.h` (saf mantık, mevcut `namespace BotCore` içine ekleme)

Mevcut kodun biçimini (tab, Allman, `inline`, İngilizce kısa yorum, ölçülmemiş değer `[A]`) koru. Yeni `#include` gerekmez. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez** (yorumlarda da `shared/` dizgisi **yazma**: K4 grep'i takılır). `CheckTargetHp`'den **sonra**, `namespace`'in kapanışından önce ekle. **`std::min`/`std::max` kullanma**:

```cpp
	// --- respawn slice (ADR-0017 Ek F4-07) ---

	constexpr uint32_t kRegeneMinDeadMs = 3000;   // docs/03 CLI-14: the death screen and the respawn button take a human at least this long [A] (unmeasured)

	struct RegeneCheck
	{
		uint32_t sinceDeadMs;     // since the bot's death was first noticed
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum RegeneVerdict
	{
		REGENE_OK = 0,
		REGENE_REJECT_WAIT = 1,   // CLI-14 (respawn requested before kRegeneMinDeadMs after the death)
		REGENE_REJECT_RATE = 2    // CLI-11
	};

	// Guard rule for a respawn request. The caller has already checked that the bot is dead. Order: wait, rate.
	inline RegeneVerdict CheckRegene(const RegeneCheck & c);
```

Gövde (`CheckTargetHp` kalıbı, aynı yerde `inline`): `c.sinceDeadMs < kRegeneMinDeadMs` → `REGENE_REJECT_WAIT`; `c.actionsInWindow >= kMaxActionsPerWindow` → `REGENE_REJECT_RATE`; aksi halde `REGENE_OK`.

**`Tests/BotCoreTests/CombatTests.cpp` (ekleme, mevcut makro stili):** iki yeni `TEST_CASE`:

- `Combat_RegeneCheck_Order`: yardımcı `OkRegene()` (`sinceDeadMs = 5000, actionsInWindow = 0`). Hepsi ihlal (`sinceDeadMs = 0, actionsInWindow = 6`) → `REGENE_REJECT_WAIT` (sıra); yalnızca `actionsInWindow = 6` → `REGENE_REJECT_RATE`; `actionsInWindow = 5` → `REGENE_OK`; hepsi geçerli → `REGENE_OK`.
- `Combat_RegeneCheck_Boundaries`: `sinceDeadMs = 2999` → `REGENE_REJECT_WAIT`; `3000` → `REGENE_OK`; `0` → `REGENE_REJECT_WAIT`; `kRegeneMinDeadMs == 3000`.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'de hedef HP üyelerinin altına (aynı yorum/hizalama biçimi):

```cpp
	bool m_deadSeen;                                       // IOCP thread only: the bot was seen dead and m_deadSince is valid
	std::chrono::steady_clock::time_point m_deadSince;     // IOCP thread only: when its death was first noticed (TickSessions or the first regene request)
```

ve atomik üyelerin yanına (`m_targetHpValues`'un altına):

```cpp
	std::atomic<uint64> m_regeneEcho;                      // written by OnPacket(): valid bit | x << 32 | z << 16 | y (all x10) of the last WIZ_REGENE reply
```

`BotSession.cpp`: başlatıcı listesine `m_deadSeen(false)` (`m_hpReqHasLast(false)`'tan sonra) ve `m_regeneEcho(0)` (`m_targetHpValues(0)`'dan sonra) ekle; sıra üye bildirim sırasıyla aynı olmalı (derleyici sıra uyarısı vermemeli). `ResetForRespawn()` içine `m_deadSeen = false; m_regeneEcho = 0;` ekle.

`OnPacket()`: `WIZ_TARGET_HP` bloğundan **sonra**, **yalnızca ekleme** (mevcut bloklar bayt bayt aynı kalır):

```cpp
	// Respawn reply: u16 x*10, u16 z*10, u16 y*10 (AttackHandler.cpp:196-198). ActionExecutor::RequestRegene clears the
	// record before its request and reads it afterwards, on the same thread.
	if (opcode == WIZ_REGENE && pkt.size() >= 6)
	{
		uint16 x = pkt.read<uint16>(0);
		uint16 z = pkt.read<uint16>(2);
		uint16 y = pkt.read<uint16>(4);
		m_regeneEcho = (1ull << 63) | (uint64(x) << 32) | (uint64(z) << 16) | uint64(y);
	}
```

(`pkt.read<T>(offset)` bayt ofsetiyle okur; mevcut bloklardaki gibi.)

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

**Başlık (`ActionExecutor.h`):** `TargetHpOutcome`'dan sonra:

```cpp
struct RegeneOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "respawned" (SENT), "no_result" (FAILED),
	                       // REFUSED: "not_in_game", "not_dead", "no_np", "dead_wait", "rate"
	float x;               // metres, from the server's reply; valid only when kind == SENT
	float z;
};
```

`class ActionExecutor` içine (`RequestTargetHp` bildiriminin altına, aynı yorum kalıbıyla):

```cpp
	// One-shot respawn: sends one WIZ_REGENE (type 1) through CUser::HandlePacket() after the guard (CLI-14: at least
	// 3 s after the death was noticed; CLI-11) and maps the result from the WIZ_REGENE reply the server published
	// (m_regeneEcho). SENT "respawned": the reply arrived (x / z filled). FAILED "no_result": no reply.
	// REFUSED without an event: "not_in_game", "not_dead", "no_np" (loyalty 0: the server would kick the bot out of the
	// zone, KI-013); with FAIRNESS_REJECT: "dead_wait", "rate".
	static RegeneOutcome RequestRegene(BotSession * s, std::chrono::steady_clock::time_point now);
```

**`ActionExecutor.cpp`** (dosya sonuna, `RequestTargetHp`'ten sonra; yeni bölüm başlığı `// --- respawn slice (ADR-0017 Ek F4-07) ---`):

1. **`static RegeneOutcome RejectRegene(BotSession * s, CUser * user, BotCore::RegeneVerdict v, const BotCore::RegeneCheck & c)`** (`RejectTargetHp` kalıbı): `WAIT` → `rule "CLI-14", reason "dead_wait", value = sinceDeadMs, limit = kRegeneMinDeadMs`; `RATE` → `rule "CLI-11", reason "rate", value = actionsInWindow, limit = kMaxActionsPerWindow`. `decisionId = NextDecisionId(s)`; `EmitFairnessReject(s, user, decisionId, "Regene", rule, reason, value, limit)`; `REFUSED` + reason; `x = z = 0`. Sunucuya paket **gitmez**.
2. **`ActionExecutor::RequestRegene`:**
   - `s == nullptr || s->m_pUser == nullptr || !isInGame()` → `REFUSED "not_in_game"`; `!user->isDead()` → `REFUSED "not_dead"` (olay yazılmaz; sunucu da sessizce yok sayardı).
   - `user->GetLoyalty() == 0` → `REFUSED "no_np"` (olay yazılmaz; gerekçe `AttackHandler.cpp:240-243`, KI-013).
   - `nowMs` (`SetStance` ile aynı `steady_clock` → ms dönüşümü). Ölüm kaydı: `!s->m_deadSeen` ise `s->m_deadSeen = true; s->m_deadSince = now;` (ilk fark edilen an). `sinceDeadMs = now - m_deadSince` (ms; `now < m_deadSince` ise 0). `inWindow = s->m_actionWindow.CountInWindow(nowMs)`.
   - `RegeneCheck c = { sinceDeadMs, inWindow }`; `verdict != REGENE_OK` → `RejectRegene`.
   - **Gönder:** `decisionId = NextDecisionId(s)`; `ACTION_SUBMIT` (`decisions`): `"decision_id","type":"Regene","regene_type":1`. Paket: `Packet pkt(WIZ_REGENE); pkt << uint8(1);` (sunucu `u8` okur, `User.cpp:334-336`). `s->m_regeneEcho = 0;` → `user->HandlePacket(pkt)` (gecikme `steady_clock` ile) → `s->m_actionWindow.Record(nowMs)`.
   - **Sonuç:** `e = s->m_regeneEcho.load()`; `(e & (1ull << 63)) != 0` ise `respawned`: `x = (float)((e >> 32) & 0xFFFF) / 10.0f`, `z = (float)((e >> 16) & 0xFFFF) / 10.0f`; `s->m_deadSeen = false`; aksi halde `no_result` (`m_deadSeen` kalır).
   - `ACTION_RESULT`: `"decision_id","type":"Regene","ok","reason":"respawned"|"no_result","latency_us","alive":<true|false>` ve yalnızca `respawned` iken `"x":<x>,"z":<z>` (`FormatFixed(., 1)`). `x`/`z` **yalnızca cevap paketinden** gelir; `alive` yalnızca telemetridir (`user->isDead()` okunur, `state_after` kalıbı); sonuç kararında kullanılmaz.
   - Dönüş: `respawned` → `SENT "respawned"` (`x`, `z` dolu), değilse `FAILED "no_result"`.

`HandlePacket(pkt)` çağrısı `WIZ_REGENE` için **tek yerde** (`RequestRegene`). `CUser::Regene()` doğrudan çağrılmaz; `SetPosition`, `HpChange`, `m_bResHpType`'e yazma, `m_sHp`/`m_iMaxHp` okuma bot kodunda **yok**.

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirim: `void CommandRegene(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()` (`:591-640`):** `target` dalının yanına `regene` fiilini ekle (`CommandRegene(args)`); "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast, pot, sit, stand, target, regene)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **`CommandRegene(args)`:** `CommandStance` kalıbını izle (`SplitWords`, `FindSession`, `IsKnownBotName`, `PhaseName`, `WriteBotLog`). Kullanım satırı (`words.size() != 1` iken **tek** günlük satırı ve dön): `BotManager: cmd regene: usage: regene <bot>`. Sırayla: bot yoksa `BotManager: cmd regene: unknown or not spawned bot '<ad|?>'`; bot `PHASE_IN_GAME` değilse `BotManager: cmd regene: <bot> not in game (phase X)`. `ActionExecutor::RequestRegene(s, std::chrono::steady_clock::now())`: `REFUSED` → `BotManager: cmd regene: <bot> refused (<reason>)`; `SENT` → `BotManager: cmd regene: <bot> respawned at (<x>, <z>)` (`%.1f`); `FAILED` → `BotManager: cmd regene: <bot> failed (<reason>)`. `char message[256]`.
3. **`TickSessions()` (ölü bot dalı, `:2018-2030`):** yalnızca **iki ekleme satırı** (mevcut satırlar değişmez):
   - `if (s->m_pUser->isDead())` bloğunun başına: `if (!s->m_deadSeen) { s->m_deadSeen = true; s->m_deadSince = now; }`
   - `else` bloğunun başına (`TickMove` çağrısından önce): `s->m_deadSeen = false;`
4. **`BeginDespawn()`, `BuildStatusLines()`:** **değişmez** (tek seferlik aksiyon, seri yok, `list` biçimi sabit). Başka hiçbir yere dokunma (`ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj`, `.filters`, `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` **değişmez** (yeni dosya yok). Mevcut dosyaların kodlama/satır sonu/BOM durumu korunur.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı iki yeni test adını (`Combat_RegeneCheck_Order`, `Combat_RegeneCheck_Boundaries`) içerir ve toplam test sayısı **33**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; dosyada `#include` yalnızca `<algorithm>` ve `<cstdint>`; `grep -n "std::min\|std::max" BotCore/BotCombat.h` yeni satır göstermez.
- [ ] K5: respawn paketi yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_REGENE" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma, `RequestRegene`) ve `BotSession.cpp` (`OnPacket` sonuç okuma) satırlarını gösterir; `grep -nE "(->|\.)Regene\(|SetPosition|HpChange|m_bResHpType\s*=[^=]" GameServer/Bot/*.cpp GameServer/Bot/*.h` boş (`RequestRegene`, `RegeneOutcome` gibi ad eşleşmeleri dışında: desen `Regene(` yalnızca `CUser::Regene` çağrısını yakalamalı; eşleşme varsa Uygulayıcı Raporu'nda açıkla).
- [ ] K6: `RequestRegene`'de guard atlanmıyor: `HandlePacket`'tan önce `CheckRegene` çağrısı ve `REGENE_OK` dışında erken dönüş vardır; `WIZ_REGENE` için `HandlePacket(pkt)` çağrısı tek yerde (kod okumasıyla; Claude çalışma zamanında da sınar). `GetLoyalty() == 0` kontrolü `HandlePacket`'tan önce.
- [ ] K7: sonuç yalnızca cevap paketinden: `RequestRegene` gövdesinde `m_sHp`, `m_iMaxHp`, `GetHealth`, `GetMaxHealth`, `GetX`, `GetZ` geçmez (`grep -n "m_sHp\|m_iMaxHp\|GetHealth\|GetMaxHealth\|GetX()\|GetZ()" GameServer/Bot/ActionExecutor.cpp` bu fonksiyonun satır aralığında boş); `x`/`z` `m_regeneEcho`'dan okunur; `isDead()` yalnızca ön koşul ve `alive` telemetri alanında.
- [ ] K8: `OnPacket()` yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-07 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki bilinçli değiştirilen satır(lar)ı gösterir; `OnPacket()` mevcut `WIZ_SEL_CHAR`/`WIZ_ATTACK`/`WIZ_MAGIC_PROCESS`/`WIZ_STATE_CHANGE`/`WIZ_TARGET_HP` blokları değişmedi; `WIZ_DEAD` bloğu yok.
- [ ] K9: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutla çalışır; `git diff gece/2026-10-02...bot/F4-07 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca `unknown command` mesaj satırını gösterir; `Startup()`/`Tick()`/`BuildStatusLines()`/`BeginDespawn()` ve ini okuma değişmedi; `TickSessions()` farkı yalnızca iki ekleme satırı.
- [ ] K10: `git diff --stat gece/2026-10-02...bot/F4-07` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K11: değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K12: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K13: F4-01..F4-06 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack" ...` ≥ 1, `grep -c "CheckCastStart" ...` ≥ 1, `grep -c "CheckPotion" ...` ≥ 1, `grep -c "CheckStance" ...` ≥ 1, `grep -c "CheckTargetHp" ...` ≥ 1; önceki 31 testin tamamı hâlâ geçiyor; `EmitFairnessReject` çağrıları `"Move"`/`"Attack"`/`"Cast"`/`"Potion"`/`"State"`/`"TargetHp"` geçiyor, yeni `"Regene"` eklendi.
- [ ] K14 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-07
git diff gece/2026-10-02...bot/F4-07 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-07 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-07 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "std::min\|std::max" BotCore/BotCombat.h
grep -n "WIZ_REGENE" GameServer/Bot/*.cpp
grep -nE "(->|\.)Regene\(|SetPosition|HpChange|m_bResHpType\s*=[^=]" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "CheckRegene\|HandlePacket\|GetLoyalty" GameServer/Bot/ActionExecutor.cpp
grep -n "m_sHp\|m_iMaxHp\|GetHealth\|GetMaxHealth\|GetX()\|GetZ()" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-07
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; **bir dosyadaki satırlar aynı `Tick()`'te sırayla çalışır**, ayrı dosyalar ≥ 1,05 sn arayla verilmelidir). Botlar: Karus warrior `BotWP_K`, El Morad warrior `BotWP_E`; zone 71. Ölü bot, F4-06 doğrulamasındaki gibi `attack BotWP_K BotWP_E 60` ile üretilir (~41 vuruş). Gözlem `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'dan. Beklenmeyen `no_result` bu planın hatası değil, **sonuç olarak raporlanır** (özellikle sunucunun `WIZ_REGENE` cevabını botun kendi alıcısına gönderip göndermediği: `Send()` doğrulaması; göndermiyorsa **durup** raporla, bot sunucu nesnesine bakarak "başarılı" demez). Bir botun `Regene`'si `AG_USER_REGENE` ile AIServer'a da gider: AIServer kapalıysa bu senaryolar sınanmaz, raporda söylenir.

1. **Mutlu yol ve bekleme:** `spawn BotWP_K,BotWP_E`; `attack BotWP_K BotWP_E 60` ile `BotWP_E` ölsün (`killed`); hemen (≤ 1 sn) `regene BotWP_E` → `refused (dead_wait)` + `FAIRNESS_REJECT` (`type:"Regene"`, `rule:"CLI-14"`, `reason:"dead_wait"`, `value` < 3000, `limit` 3000), JSONL'de `ACTION_SUBMIT` yok; ölümden ≥ 3,05 sn sonra `regene BotWP_E` → `respawned at (x, z)`; JSONL: `ACTION_SUBMIT` (`type:"Regene"`, `regene_type:1`) → `ACTION_RESULT` (`ok:true`, `reason:"respawned"`, `x`, `z`, `alive:true`, `latency_us` < 20000). Beklenen konum El Morad başlangıç noktası (630..630+range, 920..920+range; MEC-DTH-06 `[V]`, bind yok): konum raporlanır, `list` satırındaki `pos=` ile cevaptaki `x`/`z` aynı olmalı (operatör karşılaştırması). `list`: HP maksimum, MP **dolmaz** (MEC-DTH-07; ölmeden önceki MP'den yüksek değil), bot `in_game`.
2. **`not_dead` ve tekrar:** canlı bot için `regene BotWP_K` → `refused (not_dead)`, JSONL'de olay yok. Doğduktan hemen sonra `regene BotWP_E` → `refused (not_dead)`.
3. **Doğuş sonrası bot çalışır:** doğan `BotWP_E` ile `move`/`target`/`attack` komutları (F4-01/F4-06/F4-02) çalışır (ölü değil; yeni konumdan en fazla bir yürüyüş veya `out_of_view` reddi beklenir, bölge farkı raporlanır); `m_deadSeen` temizlenmiş olmalıdır: `BotWP_E` tekrar öldürülünce `dead_wait` yeniden başlar (ikinci ölümde hemen `regene` → `refused (dead_wait)`, ≥ 3,05 sn sonra `respawned`).
4. **Ölü açılış (F4-06 bulgusu 3):** `BotWP_E` öldürülüp doğmadan `despawn BotWP_E`, ardından `spawn BotWP_E` → bot ölü açılır (`list` `hp=0/...` veya `isDead`); ilk `regene` hemen → `refused (dead_wait)` (`m_deadSeen` spawn başına sıfırlandı ve `TickSessions()` ölü botu yeniden fark etti), ≥ 3,05 sn sonra `respawned`. Bu senaryo sınanamazsa raporda "sınanmadı" yazılır.
5. **`no_np` (sınanabilirse):** bot kapalıyken (despawn sonrası, yeni spawn öncesi) `BotWP_E` satırının `Loyalty` değeri hedefli `UPDATE` ile 0 yapılır (yalnızca bu bot satırı; kişisel veri tablosu **okunmaz**), bot spawn edilir, öldürülür, ≥ 3,05 sn sonra `regene BotWP_E` → `refused (no_np)`, JSONL'de olay yok, bot ölü kalır; ardından `Loyalty = 1000` geri yazılır ve `despawn`/`spawn` + `regene` → `respawned`. Sınanmazsa raporda "sınanmadı" yazılır. **Sunucu bot satırı dışında hiçbir şey değiştirilmez.**
6. **Ömür ve reddedilen komutlar:** `despawn BotWP_E` sonrası `regene BotWP_E` → `not in game (phase despawned)`; `regene Ghost` → `unknown or not spawned bot '?'`; argümansız ve `regene BotWP_K BotWP_E` → kullanım satırı (tek satır); `RESPAWN_CYCLES=2` iken `regene` → `cmd rejected` (yeniden sınanmazsa raporda söylenir).
7. **Gerilemesiz:** `attack BotWP_K BotWP_E 3`, `target BotWP_K BotWP_E` (F4-06), `sit`/`stand` (F4-05), `pot` (F4-04), `cast BotMF_K 110518 BotWP_E 2` (F4-03), 30 m `move` (F4-01) önceki çıktıyı verir; `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama `regene` çalışır; `ENABLED=0` → komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde (≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. İnsan istemcisi gerekmez (gerçek istemcide yeniden doğuş görünürlüğü `T-ARCH-12`, `docs/STATUS.md` "Proje sahibi testleri"). Temizlik: ini yedekten geri, bot satırları (`Hp=Mp=32000`, `PX=127400`, `PZ=89000`, `Loyalty=1000`) hedefli `UPDATE` ile geri yazılır.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). `regene` yalnızca `ENABLED=1` iken, üretim dışı test komutudur.
- **Thread kuralı (ADR-0005):** `ActionExecutor` yalnızca IOCP thread'inde (komut çekirdeği `Tick()` içinden) çalışır; `HandlePacket` zaten bu thread'de koşar. Konsol/`+bot` işleyicisi `BotSession`/`ActionExecutor`'a **dokunmaz**; komutlar `EnqueueCommand` kuyruğundan gelir. `OnPacket()` her thread'den çağrılabilir: yeni blok yalnızca tek atomik yazar. `m_deadSeen`/`m_deadSince` yalnızca IOCP thread'inde (`TickSessions()` ve `RequestRegene`).
- **Sonuç yalnızca yayınlanan cevaptan** (AC-LRN-03 / `docs/13` §8). Konum için sunucu nesnesine (`GetX()`/`GetZ()`) bakmak yasak; telemetriye yazılan `x`/`z` yalnızca cevaptan gelir.
- **`CUser::Regene()` doğrudan çağrılmaz**, konum/HP bot kodunda yazılmaz: yalnızca `HandlePacket` (sunucunun kendi handler'ı) yazar.
- **Bilinen sınırlar `[A]`:** (a) `kRegeneMinDeadMs = 3000` ölçülmedi (gerçek istemcinin ölüm → `WIZ_REGENE` gecikmesi `T-REGENE`); (b) `Send()`'in cevabı botun kendi alıcısına ilettiği varsayımı (`WIZ_TARGET_HP` ile aynı mekanizma, F4-06'da `[V]`; doğrulamada teyit edilir; iletmiyorsa `no_result` raporlanır, **kendi başına nesneye bakarak başarı ilan etme**, **durup** raporla); (c) `Regene`'nin `Send_AIServer(AG_USER_REGENE)` ve bölge giriş/çıkış paketlerinin bot oturumunda sorunsuz çalıştığı varsayımı; çalışma zamanında doğrulanır, aksaklık (çökme, hata günlüğü) **durup raporlanır**; (d) ölüm anı `TickSessions()` çözünürlüğünde (tick) fark edilir, ilk ölü görülen komut bir tick geç kalabilir (guard bu yönde muhafazakârdır); (e) NP 0 iken bot ölü kalır (KI-013).
- Telemetri hacmi küçüktür (istek başına ≤ 2 olay); `droppable = false`, `IsEnabled` denetimi önceki aksiyonlarla aynı.
- `list` satırı ve `Telemetry.*` değişmez; mevcut komutların çıktıları **değiştirilmez**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve nedenleri:
- Derleme sonucu (`tools/build.sh Release` son satırlar):
- Kabul kriterleri öz-değerlendirme:
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
