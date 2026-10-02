# F4-05: `ActionExecutor` duruş dilimi — `StateSit` (otur/kalk) ve `BotFairnessGuard` CLI-13 kuralları

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-05` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-04 (pot dilimi, `m_castSelfId` kalıbı) — `KAPANDI`; F4-03 (`BeginCast`) — `KAPANDI`; F4-02 (`m_actionWindow`, `BeginAttack`) — `KAPANDI`; F4-01 (`BeginMove`) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-13 (yeni, `docs/03`), CLI-11, MEC-REG (sunucu `HPTimeChange`), `docs/11` STK-03 (oturma kuralının karar tarafı), MET-ACT-02, MET-FAIR-01 altyapısı, AC-LRN-03 |
| Tahmini büyüklük | M (8 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun beşinci gerçek aksiyonu: bir bot **gerçek `WIZ_STATE_CHANGE` (tip 1) paketi** ile `CUser::HandlePacket()` üzerinden **oturur veya kalkar**. Paket sunucuya gitmeden önce `BotFairnessGuard` kurallarından geçer: seri sürerken (yürüme/saldırı/cast) oturma yok, iki duruş paketi arası ≥ 1000 ms, aksiyon hızı tavanı CLI-11 (hepsi `docs/03` CLI-13). Sonuç, sunucunun yayınladığı `WIZ_STATE_CHANGE` paketinden okunur (`m_bResHpType`'a **bakılmaz**). Aynı planda, otururken `move`/`attack`/`cast` başlatılamaz (gerçek istemci önce kalkar; sunucu bunu engellemez, guard engeller). Karar katmanı yoktur: aksiyonu `/bot sit` ve `/bot stand` komutları tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün beşinci dilimidir (ADR-0017 Ek F4-05; hareket → saldırı → cast → pot → **duruş** → `TargetHpReq` → ölüm/`Regene` → `Party`/`Chat` → `Perception`).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-05)" (bu planla birlikte yazıldı): paket düzeni, sonuç eşlemesi, CLI-13, `sitting`/`busy` ayrımı.
- `docs/03` §14 **CLI-13** (bu planla birlikte eklendi), **CLI-11**; `docs/11` STK-03 (oturmanın *ne zaman* yapılacağı karar katmanının işidir).
- `plans/F4-04-aksiyon-yurutucu-pot.md` §5.2–§5.5 ve Doğrulama Raporu: bu planın kalıpladığı iskelet (saf mantık `BotCore/BotCombat.h` + `ActionExecutor` + `BotSession` durumu + `BotManager` komutu). **Yazılı planı değil, birleşmiş kodu esas al** (`ActionExecutor.cpp` `EmitFairnessReject` `:53`, `SubmitPotion` `:988`, `BeginPotion` `:1056`, `TickPotion` `:1150`).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `355feb2` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:376-378` — `HandlePacket` `case WIZ_STATE_CHANGE: StateChange(pkt)`.
  - `GameServer/User.cpp:2710-2723` — `CUser::StateChange`: ölüyse sessizce döner (`:2712`); paketi `u8 bType`, `u16 nBuff` okur (`:2715-2716`); `bType == 1` için `buff` yalnızca `USER_STANDING` (1) veya `USER_SITDOWN` (2) olabilir (`:2721-2723`), aksi halde döner; sonra `StateChangeServerDirect(bType, nBuff)`.
  - `GameServer/User.cpp:2777-2820` — `StateChangeServerDirect`: `bType == 1` → `m_bResHpType = buff` (`:2782-2784`); ardından **`Packet result(WIZ_STATE_CHANGE); result << GetSocketID() << bType << nBuff; SendToRegion(&result)`** (`:2817-2819`): düzen `u16 socketId, u8 bType, u32 nBuff` = 7 bayt. `SendToRegion` göndereni **dışlamaz** (`Unit.cpp:807-810`, `Send_Region(..., nullptr, ...)`), bu yüzden botun kendi alıcısına da gelir `[A]`: doğrulamada Claude teyit eder.
  - `GameServer/User.cpp:3359-3400` — `HPTimeChange`: `m_bResHpType == USER_SITDOWN` iken HP ve MP yenilenmesi (standing: yalnızca MP, düşük oranda); `User.cpp:512-513` `Update()` içinden `m_bHPIntervalNormal` (= 5, `User.cpp:143`) saniyeden eskiyse çağrılır. Bot `Update()` her bot için 1 sn'de bir IOCP thread'inde çalışır (F2-04), yani oturma yenilenmesi botlarda zaten işler.
  - `GameServer/GameDefine.h:117-118` — `USER_STANDING 0x01`, `USER_SITDOWN 0x02`; `GameServer/User.h:239` `uint8 m_bResHpType` (**`public:`** bölümünde: `User.h:113-302`, bot kodu okuyabilir; **yazmaz**).
  - `GameServer/Bot/BotSession.cpp:40-53` — `OnPacket()` `WIZ_MAGIC_PROCESS` bloğu: bu planın ekleyeceği `WIZ_STATE_CHANGE` bloğunun kalıbı (yalnızca botun **kendi** kimliğiyle gelen paket kaydedilir, `m_castSelfId`).
  - `GameServer/Bot/ActionExecutor.cpp:162` `BeginMove`, `:280` `BeginAttack`, `:653` `BeginCast` (hepsinde `not_in_game` ve `dead` kontrolünden hemen sonra `sitting` ön koşulu eklenecek); `:36-69` `NextDecisionId`, `FormatFixed`, `EmitFairnessReject`; `BotManager.cpp:627-634` fiil dağıtımı, `:1523-` `CommandPot()` (kalıp), `:781-830` `BuildStatusLines()`.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** `kStanceToggleMinMs`, `StanceCheck`, `StanceVerdict`, `CheckStance`. Birim testleri (`CombatTests.cpp`).
2. **`ActionExecutor::SetStance(s, sit, now)`** (tek seferlik aksiyon): ön koşullar → `BotCore::CheckStance` → `WIZ_STATE_CHANGE` paketi → `HandlePacket` → yayınlanan paketten sonuç eşleme → telemetri. Ek olarak `BeginMove`/`BeginAttack`/`BeginCast` otururken `REFUSED "sitting"` döner.
3. **Oturum durumu (`BotSession`):** son duruş paketi zamanı (spawn başına) ve `OnPacket()`'in doldurduğu `m_stateEcho`.
4. **Komutlar (`BotManager`):** `sit <bot>` ve `stand <bot>` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten); `list` satırına `sit=`.
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT` / `ACTION_RESULT` (`"type":"StateSit"`), `FAIRNESS_REJECT` (`"type":"State"`).

**Kapsam dışı (yapılmayacak)**

- **Ne zaman oturulacağı** (`docs/11` STK-03, 60 m kuralı), "oturunca yenilenme bekle" mantığı, düşman yaklaşınca kalkma: karar katmanının işi. `sit` komutu bot yürürken/vururken reddedilir, bunun dışında hiçbir koşula bakmaz (HP/MP dolu da olsa oturur).
- **Emote/animasyon durumları** (`WIZ_STATE_CHANGE` tip 4, selam/kışkırtma), tip 2/3/5/7 (parti arama, dönüşüm, GM görünürlük): yok. Yalnızca tip 1.
- **Pot ile oturma etkileşimi** (Q-06, T-MECH-POT-05: pot oturanı kaldırır mı): ölçülmez; `pot` otururken serbesttir, sonuç gözlemlenip **raporlanır**.
- **Yenilenme hızı ölçümü / model karşılaştırması:** doğrulamada Claude sayar; kodda yok.
- **Seri durdurma (`stop`) komutunun oturmayı kapsaması, oturunca seriyi otomatik durdurma, hareketle otomatik kalkma:** yok. Bot otururken `move` reddedilir; önce `stand` verilir.
- `ChatHandler.cpp` `+bot` yardım metni (KI-012; Claude doğrulamada günceller). Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya yok; **`GameServer` projesine dosya eklenmez**. `Telemetry.*`, `ScenarioRunner.*` değişmez.
- Dokümanları (`docs/03`, `docs/11`, `docs/13`, `docs/16`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | Yalnızca ekleme (§5.2); mevcut içerik ve `#include`'lar değişmez |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | Yalnızca ekleme: iki yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `StanceOutcome`, bir yeni statik fonksiyon; `MoveOutcome`/`AttackOutcome`/`CastOutcome` yorumlarına `"sitting"` |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | Duruş yolu (§5.4) ve üç `Begin*` içine `sitting` ön koşulu; başka hareket/saldırı/cast/pot kodu değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca duruş durumu üyeleri |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi, `ResetForRespawn()` ve `OnPacket()`'e **ekleme** bloğu (mevcut bloklar değişmez) |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandStance` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, komut, `BuildStatusLines()` |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (Yeni dosya açılmaz: guard `BotCombat.h`'ye eklenir, böylece `BotCore*.vcxproj` değişmez.)

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-05 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotCombat.h` (saf mantık, mevcut `namespace BotCore` içine ekleme)

Mevcut kodun biçimini (tab, Allman, `inline`, İngilizce kısa yorum, ölçülmemiş değer `[A]`) koru. Yeni `#include` gerekmez. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez**. `CheckPotion`'dan **sonra**, `namespace`'in kapanışından önce ekle:

```cpp
	// --- stance slice (ADR-0017 Ek F4-05) ---

	constexpr uint32_t kStanceToggleMinMs = 1000;   // docs/03 CLI-13: two stance packets at least 1.0 s apart [A: conservative]

	struct StanceCheck
	{
		bool toSit;               // true = sit down, false = stand up
		bool busy;                // a walk, an attack series or a cast series is in progress (rule applies to toSit only)
		bool hasLast;             // a stance packet was sent earlier in this spawn
		uint32_t sinceLastMs;     // since that packet
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum StanceVerdict
	{
		STANCE_OK = 0,
		STANCE_REJECT_BUSY = 1,     // CLI-13 (sitting down while a walk/attack/cast series runs)
		STANCE_REJECT_TOGGLE = 2,   // CLI-13 (previous stance packet younger than kStanceToggleMinMs)
		STANCE_REJECT_RATE = 3      // CLI-11
	};

	// Guard rule for a stance packet. Order: busy (toSit only), toggle interval, rate.
	// "Already in the requested stance" is NOT a guard rule: the executor refuses it as "no_change" before this call.
	inline StanceVerdict CheckStance(const StanceCheck & c);
```

Gövde: `c.toSit && c.busy` → `STANCE_REJECT_BUSY`; `c.hasLast && c.sinceLastMs < kStanceToggleMinMs` → `STANCE_REJECT_TOGGLE`; `c.actionsInWindow >= kMaxActionsPerWindow` → `STANCE_REJECT_RATE`; aksi halde `STANCE_OK`. Mevcut `CheckPotion` kalıbını izle.

**`Tests/BotCoreTests/CombatTests.cpp` (ekleme, mevcut makro stili):** iki yeni `TEST_CASE`:

- `Combat_StanceCheck_Order`: `toSit = true, busy = true, hasLast, sinceLastMs = 0, actionsInWindow = 6` (üçü de ihlal) → `STANCE_REJECT_BUSY` (sıra); `busy = false` (diğerleri ihlal) → `STANCE_REJECT_TOGGLE`; `sinceLastMs = 999` → `STANCE_REJECT_TOGGLE`; `sinceLastMs = 1000` → `STANCE_OK` (`actionsInWindow = 5`); `hasLast = false` → `STANCE_OK`; `actionsInWindow = 6` (diğerleri geçerli) → `STANCE_REJECT_RATE`; `actionsInWindow = 5` → `STANCE_OK`.
- `Combat_StanceCheck_StandIgnoresBusy`: `toSit = false, busy = true` (diğerleri geçerli) → `STANCE_OK` (kalkmak her zaman serbest, `toggle` ve `rate` hâlâ geçerli: `toSit = false, hasLast, sinceLastMs = 500` → `STANCE_REJECT_TOGGLE`; `actionsInWindow = 6` → `STANCE_REJECT_RATE`); `kStanceToggleMinMs == 1000`.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'de pot üyelerinin altına (aynı yorum/hizalama biçimi):

```cpp
	bool m_stanceHasLast;                                  // IOCP thread only: m_stanceLast is valid for this spawn
	std::chrono::steady_clock::time_point m_stanceLast;    // IOCP thread only: when the last stance packet went out
```

ve atomik üyelerin yanına (`m_castEcho`'nun altına):

```cpp
	std::atomic<uint64> m_stateEcho;                       // written by OnPacket(): own WIZ_STATE_CHANGE broadcast, see BotSession.cpp
```

`BotSession.cpp`: başlatıcı listesine `m_stanceHasLast(false)` ve `m_stateEcho(0)` ekle (mevcut sıraya uy, üye bildirim sırasıyla aynı sırada: derleyici sıra uyarısı vermemeli). `ResetForRespawn()` içine `m_stanceHasLast = false;` ve `m_stateEcho = 0;` ekle.

`OnPacket()`: `WIZ_MAGIC_PROCESS` bloğundan **sonra**, **yalnızca ekleme** (mevcut bloklar bayt bayt aynı kalır):

```cpp
	// State change broadcast: u16 socket id, u8 bType, u32 nBuff (User.cpp:2817-2819). Only the bot's own packet is recorded;
	// m_castSelfId (own id) is written on the IOCP thread before HandlePacket() runs on the same thread.
	if (opcode == WIZ_STATE_CHANGE && pkt.size() >= 7)
	{
		uint16 sid = pkt.read<uint16>(0);
		uint8 bType = pkt.read<uint8>(2);
		uint32 nBuff = pkt.read<uint32>(3);
		if ((int)sid == m_castSelfId.load())
			m_stateEcho = (1ull << 63) | (uint64(bType) << 32) | uint64(nBuff);
	}
```

(`pkt.read<T>(offset)` mevcut bloklardaki gibi bayt ofsetiyle okur; ofset semantiğini `shared/ByteBuffer.h`'den teyit et. `m_castSelfId`'in türü `std::atomic<int>`.)

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

**Başlık (`ActionExecutor.h`):** `PotionOutcome`'dan sonra:

```cpp
struct StanceOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "applied" (SENT), "no_result" (FAILED),
	                       // REFUSED: "not_in_game", "dead", "no_change", "busy", "toggle", "rate"
};
```

`class ActionExecutor` içine (pot bildirimlerinin altına, aynı yorum kalıbıyla):

```cpp
	// One-shot stance change: sit down (sit = true) or stand up. Validates, runs the guard, sends one WIZ_STATE_CHANGE
	// (type 1) through CUser::HandlePacket() and maps the result from the broadcast the server published (m_stateEcho).
	// SENT "applied": the broadcast carried the requested stance. FAILED "no_result": no/other broadcast.
	// REFUSED: "not_in_game", "dead", "no_change" (already in that stance; no event) or a guard verdict ("busy", "toggle",
	// "rate"; FAIRNESS_REJECT written).
	static StanceOutcome SetStance(BotSession * s, bool sit, std::chrono::steady_clock::time_point now);
```

Ayrıca `MoveOutcome`/`AttackOutcome`/`CastOutcome` `reason` yorumlarının listesine `"sitting"` ekle (yalnızca yorum).

**`ActionExecutor.cpp`:**

1. **Üç `Begin*` içinde `sitting` ön koşulu:** `BeginMove`, `BeginAttack`, `BeginCast`'ta `isDead()` kontrolünden **hemen sonra** (diğer doğrulamalardan önce):
   ```cpp
   if (user->m_bResHpType == USER_SITDOWN)
   {
   	out.kind = <Move|Attack|Cast>Outcome::REFUSED;
   	out.reason = "sitting";
   	return out;
   }
   ```
   `BeginPotion` **değişmez** (pot otururken serbest). `TickMove`/`TickAttack`/`TickCast` değişmez (seri sürerken oturma `busy` ile engellendiği için yürüyen seri ile oturan bot birlikte olamaz).
2. **`static const char * StanceReason(BotCore::StanceVerdict v)` ve `static StanceOutcome RejectStance(BotSession * s, CUser * user, BotCore::StanceVerdict v, const BotCore::StanceCheck & c)`** (pot kalıbı, kod bloğunun sonuna): `BUSY` → `rule "CLI-13", reason "busy", value = 1, limit = 0`; `TOGGLE` → `rule "CLI-13", reason "toggle", value = sinceLastMs, limit = kStanceToggleMinMs`; `RATE` → `rule "CLI-11", reason "rate", value = actionsInWindow, limit = kMaxActionsPerWindow`. `decisionId = NextDecisionId(s)`; `EmitFairnessReject(s, user, decisionId, "State", rule, reason, value, limit)`; `REFUSED` + reason. Sunucuya paket **gitmez**.
3. **`ActionExecutor::SetStance`:**
   - `s == nullptr || s->m_pUser == nullptr || !isInGame()` → `REFUSED "not_in_game"`; `isDead()` → `REFUSED "dead"` (sunucu da ölüyü sessizce yutar, `User.cpp:2712`).
   - `bool sittingNow = (user->m_bResHpType == USER_SITDOWN);` `sit == sittingNow` → `REFUSED "no_change"` (olay **yazılmaz**).
   - `nowMs` (F4-04 `TickPotion` ile aynı `steady_clock` → ms dönüşümü: `:1176-1177`), `inWindow = s->m_actionWindow.CountInWindow(nowMs)`.
   - `StanceCheck c = { sit, s->m_moveActive || s->m_attackActive || s->m_castPhase != BotSession::CAST_IDLE, s->m_stanceHasLast, sinceLastMs (hasLast ise now - m_stanceLast, ms), inWindow }`; `verdict != STANCE_OK` → `RejectStance`.
   - **Gönder:** `decisionId = NextDecisionId(s)`; `ACTION_SUBMIT` (`decisions`): `"decision_id","type":"StateSit","to":"sit"|"stand"`. Paket: `Packet pkt(WIZ_STATE_CHANGE); pkt << uint8(1) << uint16(sit ? USER_SITDOWN : USER_STANDING);` (sunucu `u8 + u16` okur, `User.cpp:2715-2716`). `s->m_castSelfId = user->GetID(); s->m_stateEcho = 0;` → `user->HandlePacket(pkt)` (gecikme `steady_clock` ile) → `s->m_actionWindow.Record(nowMs)`; `s->m_stanceHasLast = true; s->m_stanceLast = now;`.
   - **Sonuç:** `echo = s->m_stateEcho.load()`; `(echo & (1ull << 63)) != 0 && ((echo >> 32) & 0xFF) == 1 && (uint32)(echo & 0xFFFFFFFF) == (sit ? USER_SITDOWN : USER_STANDING)` → `applied` (`ok = true`), aksi halde `no_result`.
   - `ACTION_RESULT`: `"decision_id","type":"StateSit","ok","reason","latency_us","state_after":<user->m_bResHpType>`. `state_after` **yalnızca** operatör/doğrulama içindir; sonuç eşlemesi buna **bakmaz** (AC-LRN-03 / `docs/13` §8).
   - Dönüş: `applied` → `SENT "applied"`, değilse `FAILED "no_result"`.

`HandlePacket(pkt)` çağrısı duruş için **tek yerde** (`SetStance`). `m_bResHpType` hiçbir yerde **yazılmaz** (yalnızca okunur); `StateChange`/`StateChangeServerDirect` çağrılmaz.

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirim: `void CommandStance(const std::string & args, bool sit);` (IOCP thread only).

1. **`ExecuteCommand()` (`:627-634`):** `pot` dalının yanına `sit` ve `stand` fiillerini ekle (`CommandStance(args, true)` / `CommandStance(args, false)`); "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast, pot, sit, stand)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **`CommandStance(args, sit)`:** `CommandPot` kalıbını izle (`SplitWords`, `FindSession`, `IsKnownBotName`, `PhaseName`, `WriteBotLog`). `verb = sit ? "sit" : "stand"`. Kullanım satırı (`words.size() != 1` iken **tek** günlük satırı ve dön): `BotManager: cmd <verb>: usage: <verb> <bot>`. Bot: `FindSession(words[0])` yoksa `BotManager: cmd <verb>: unknown or not spawned bot '<ad|?>'` (`IsKnownBotName` ile `?`); `m_phase != PHASE_IN_GAME` ise `BotManager: cmd <verb>: <bot> not in game (phase X)`. Sonra `ActionExecutor::SetStance(s, sit, now)`: `REFUSED` → `BotManager: cmd <verb>: <bot> refused (<reason>)`; `SENT` → `BotManager: cmd <verb>: <bot> <sat down|stood up>`; `FAILED` → `BotManager: cmd <verb>: <bot> failed (<reason>)`.
3. **`TickSessions()`:** **değişmez** (tek seferlik aksiyon, seri yok). **`BeginDespawn()`:** değişmez (duruş durumu `ResetForRespawn()`'da sıfırlanır; `CUser` durumu girişte `USER_STANDING`'e döner, doğrulamada teyit edilir).
4. **`BuildStatusLines()` (`:825-829`):** oturum satırının sonuna `sit=<0|1>` ekle (`s->m_pUser != nullptr && s->m_pUser->m_bResHpType == USER_SITDOWN`); mevcut alanların sırası ve adları **değişmez** (`pot=` sonrası). `char message[256]` kesilmeye başlarsa 320'ye çıkar (`list` çıktısının kesilmediğini gör).
5. Başka hiçbir yere dokunma (`ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj`, `.filters`, `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` **değişmez** (yeni dosya yok). Mevcut dosyaların kodlama/satır sonu/BOM durumu korunur.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı iki yeni test adını (`Combat_StanceCheck_Order`, `Combat_StanceCheck_StandIgnoresBusy`) içerir ve toplam test sayısı **28**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; dosyada `#include` yalnızca `<algorithm>` ve `<cstdint>`.
- [ ] K5: duruş paketi yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_STATE_CHANGE" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma, `SetStance`) ve `BotSession.cpp` (`OnPacket` sonuç okuma) satırlarını gösterir; `grep -n "StateChange\b\|StateChangeServerDirect\|StateChange(" GameServer/Bot/*.cpp GameServer/Bot/*.h` çağrı göstermez; `grep -n "m_bResHpType" GameServer/Bot/*.cpp` yalnızca okuma gösterir (`==` karşılaştırması, telemetri değeri): `grep -nE "m_bResHpType\s*=[^=]" GameServer/Bot/*.cpp` boş.
- [ ] K6: `SetStance`'te guard atlanmıyor: `HandlePacket`'tan önce `CheckStance` çağrısı ve `STANCE_OK` dışında erken dönüş vardır; duruş için `HandlePacket(pkt)` çağrısı tek yerde (kod okumasıyla; Claude çalışma zamanında da sınar).
- [ ] K7: `sitting` ön koşulu üç yerde: `grep -n '"sitting"' GameServer/Bot/ActionExecutor.cpp` `BeginMove`, `BeginAttack`, `BeginCast` içinde (3 satır), `BeginPotion` içinde **yok**; her biri `isDead()` kontrolünden sonra, diğer doğrulamalardan önce.
- [ ] K8: `OnPacket()` yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-05 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki bilinçli değiştirilen satır(lar)ı gösterir; `OnPacket()` mevcut `WIZ_SEL_CHAR`/`WIZ_ATTACK`/`WIZ_MAGIC_PROCESS` blokları değişmedi.
- [ ] K9: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutlarla çalışır; `git diff gece/2026-10-02...bot/F4-05 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca bilinçli değiştirilen satırları gösterir (`unknown command` mesajı, `BuildStatusLines` biçim satırı ve olası tampon boyutu); `Startup()`/`Tick()`/`TickSessions()` akışı ve ini okuma değişmedi.
- [ ] K10: `git diff --stat gece/2026-10-02...bot/F4-05` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K11: değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K12: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K13: F4-01..F4-04 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack" ...` ≥ 1, `grep -c "CheckCastStart" ...` ≥ 1, `grep -c "CheckPotion" ...` ≥ 1; önceki 26 testin tamamı hâlâ geçiyor; `EmitFairnessReject` çağrıları `"Move"`/`"Attack"`/`"Cast"`/`"Potion"` geçiyor, yeni `"State"` eklendi.
- [ ] K14 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–8 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-05
git diff gece/2026-10-02...bot/F4-05 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-05 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-05 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "WIZ_STATE_CHANGE" GameServer/Bot/*.cpp
grep -n "StateChange\|StateChangeServerDirect" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -nE "m_bResHpType" GameServer/Bot/*.cpp
grep -n '"sitting"' GameServer/Bot/ActionExecutor.cpp
grep -n "CheckStance\|HandlePacket" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-05
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; **bir dosyadaki satırlar aynı `Tick()`'te sırayla çalışır**, ayrı dosyalar ≥ 1,05 sn arayla verilmelidir). Botlar: Karus warrior `BotWP_K`, Karus mage `BotMF_K`, El Morad warrior `BotWP_E`; zone 71. Oyuncunun HP'sini `list` satırındaki `hp=` verir (operatör gözlemi; sonuç eşlemesinde kullanılmaz). Gözlem `Logs/bots/<tarih>/live-*.jsonl`'den, telemetri `decisions` ile yapılır. Beklenmeyen `no_result` bu planın hatası değil, **sonuç olarak raporlanır** (özellikle sunucunun `WIZ_STATE_CHANGE` yayınını botun kendi alıcısına gönderip göndermediği: `SendToRegion`/`Send_Region` doğrulaması; gönderilmiyorsa plan düzeltmesi gerekir ve bot kendi `m_bResHpType`'ine bakarak "başarılı" demez).

1. **Otur/kalk (mutlu yol):** `spawn BotWP_K`; `sit BotWP_K` → log `sat down`; JSONL: `ACTION_SUBMIT` (`type:"StateSit"`, `to:"sit"`) → `ACTION_RESULT` (`reason:"applied"`, `state_after:2`, `latency_us` < 5000); `list` satırında `sit=1`; ≥ 1,05 sn sonra `stand BotWP_K` → `stood up`, `to:"stand"`, `applied`, `state_after:1`, `sit=0`; `FAIRNESS_REJECT` yok.
2. **Yenilenme etkisi (sunucu davranışı, salt gözlem):** `spawn BotWP_K,BotWP_E`; botları yan yana getir (`move`), `attack BotWP_K BotWP_E 3` (E'nin HP'si düşer, `list` `hp=`); `sit BotWP_E` → oturan botun HP'si `HPTimeChange` aralığında (~6 sn'de bir) artar (`list` ile ≥ 3 ölçüm); `stand BotWP_E` → HP artışı durur (standing yalnızca MP yeniler). Sayılar raporlanır (artış beklenir; **ölçüm bu planın kabul koşulu değildir**, `[A]` etiketi için not düşülür).
3. **`no_change`:** ayakta `stand BotWP_K` → `refused (no_change)`; oturmuşken `sit` (ayrı dosya, ≥ 1,05 sn) → `refused (no_change)`; her ikisinde JSONL'e **yeni olay yazılmaz** (`ACTION_SUBMIT` yok, `FAIRNESS_REJECT` yok).
4. **`toggle` ve `busy` (guard):** aynı komut dosyasında iki satır `sit BotWP_K` / `stand BotWP_K` (aynı `Tick()`): ilk `sat down` (`applied`), ikinci `refused (toggle)` + JSONL'de `FAIRNESS_REJECT` (`type:"State"`, `rule:"CLI-13"`, `reason:"toggle"`, `value` < 1000, `limit` 1000) ve bot oturur kalır (`sit=1`). Sonra ≥ 1,05 sn bekleyip `stand` → `applied`. `move BotWP_K <x+30> <z>` ve aynı dosyada hemen `sit BotWP_K` → `refused (busy)` + `FAIRNESS_REJECT` (`CLI-13`, `busy`, `value 1`, `limit 0`), yürüyüş sürer ve `arrived` olur, bot ayakta kalır. `attack`/`cast` serisi sürerken `sit` de `busy` (en az biri sınanır).
5. **Otururken başlatma reddi (`sitting`):** `sit BotWP_K`, ≥ 1,05 sn sonra: `move BotWP_K <x+10> <z>` → `refused (sitting)`; `attack BotWP_K BotWP_E 1` → `refused (sitting)`; `cast BotWP_K <geçerli bir Type1 skill> BotWP_E 1` (ya da `BotMF_K` ile 110518) → `refused (sitting)`; üçünde de JSONL'e `ACTION_SUBMIT`/`FAIRNESS_REJECT` **yazılmaz**, bot hareket etmez. `pot BotWP_K 389015000 1` → `effected` (otururken pot serbest) ve sonrasında `sit=1` **veya** `sit=0` (**pot oturanı kaldırıyor mu**: gözlenen sonuç raporlanır, Q-06/T-MECH-POT-05). Sonra `stand` + `move 10 m` → normal yürür (`arrived`).
6. **Ömür döngüsü:** `sit BotWP_K` sonrası `despawn BotWP_K` → temiz despawn (`despawn complete`, `pool free` tam), despawn sonrası yeni paket yok; yeniden `spawn BotWP_K` → `sit=0` (giriş durumu `USER_STANDING`), ilk `sit` hemen gider (`toggle` reddi yok: spawn başına zamanlayıcı sıfırlandı), `applied`.
7. **Reddedilen komutlar:** `sit Ghost` → `unknown or not spawned bot '?'`; `sit` (argümansız) ve `sit BotWP_K x` → kullanım satırı; `despawn` edilmiş botla `not in game (phase despawned)`; `stand` aynı biçimde; `RESPAWN_CYCLES=2` iken `sit` → `cmd rejected` (yeniden sınanmazsa raporda söylenir).
8. **Gerilemesiz:** `attack BotWP_K BotWP_E 3` aynı çıktıyı verir (F4-02); `cast BotMF_K 110518 BotWP_E 2` iki çevrim `effected` (F4-03); `pot BotWP_K 389015000 3` üç `effected` (F4-04); 30 m `move` `5 packets` (F4-01); `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama `sit`/`stand` çalışır; `ENABLED=0` → komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde (≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. İnsan istemcisi gerekmez (görsel doğrulama `T-ARCH-10`, `docs/STATUS.md` "Proje sahibi testleri").

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). `sit`/`stand` yalnızca `ENABLED=1` iken, üretim dışı test komutlarıdır.
- **Thread kuralı (ADR-0005):** `ActionExecutor` yalnızca IOCP thread'inde (komut çekirdeği `Tick()` içinden) çalışır; `HandlePacket` zaten bu thread'de koşar. Konsol/`+bot` işleyicisi `BotSession`/`ActionExecutor`'a **dokunmaz**; komutlar `EnqueueCommand` kuyruğundan gelir. `OnPacket()` her thread'den çağrılabilir: yeni blok yalnızca atomik yazar, başka duruma dokunmaz.
- **Sonuç yalnızca yayınlanan sonuç paketinden okunur** (AC-LRN-03 / `docs/13` §8). Botun kendi `m_bResHpType` değeri aksiyon sonucunu belirlemek için **kullanılmaz** (yalnızca `state_after` telemetrisi ve `BeginMove/Attack/Cast` ön koşulu için okunur; bu botun kendi durumudur, istemci de bilir). Bu okuma `Perception` sözleşmesine girmez; `Perception` dilimi geldiğinde `SelfState`'e taşınır.
- **`m_bResHpType`'e yazma yok:** `StateChange`/`StateChangeServerDirect` çağrılmaz, `m_bResHpType` atanmaz (ADR-0017 Karar 1).
- **Bilinen sınırlar `[A]`:** (a) `SendToRegion`'ın botun kendi alıcısına da `WIZ_STATE_CHANGE` ilettiği varsayımı (kod okumasıyla öyle görünüyor, doğrulamada teyit edilir; iletmiyorsa `no_result` raporlanır ve plan düzeltmesi gerekir: **kendi durumuna bakarak başarı ilan etme**, **durup** raporla); (b) `kStanceToggleMinMs = 1000` istemci ölçümü değil, muhafazakâr; (c) paket düzeni `u8 + u16` sunucu okumasından türetildi, gerçek istemcinin tam baytları (`u32` olabilir) bir F1 `WIZ_STATE_CHANGE` kaydıyla karşılaştırılmadı (sunucu için fark yok: yalnızca ilk üç bayt okunur); (d) oturunca yenilenmenin sayısal etkisi ve potun oturanı kaldırıp kaldırmadığı doğrulamada gözlemlenir, kabul koşulu değildir.
- Telemetri hacmi küçüktür (duruş değişimi başına ≤ 2 olay); `droppable = false`, `IsEnabled` denetimi önceki aksiyonlarla aynı.
- `list` satırı biçimi (`pot=` sonrası yeni alan) F3-04 yanıtını da etkiler; mevcut alan adlarını ve sırasını **değiştirme**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-05` — `<kısa-sha> [F4-05] …`
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
- İncelenen: `gece/2026-10-02...bot/F4-05` @ `<sha>`
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
