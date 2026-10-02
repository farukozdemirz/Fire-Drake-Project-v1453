# F4-06: `ActionExecutor` hedef HP dilimi — `TargetHpReq` ve `BotFairnessGuard` CLI-10 kuralları

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-06` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-05 (duruş dilimi, `OnPacket()` ekleme kalıbı, `SetStance` iskeleti) — `KAPANDI`; F4-02 (`AttackTarget` kalıbı, `m_actionWindow`) — `KAPANDI`; F4-01 — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-10 (`docs/03` §14, bu planla birlikte genişletildi), CLI-11, `docs/03` §16 gözlem sözleşmesi (`P-OBS-TARGETHP-RATE`), Q-18, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı |
| Tahmini büyüklük | M (8 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun altıncı gerçek aksiyonu: bir bot **gerçek `WIZ_TARGET_HP` paketi** ile `CUser::HandlePacket()` üzerinden bir hedefi **seçer ve HP'sini ister**; sunucunun cevabı (`maxHp`, `hp`) botun alıcısına gelir. Sunucu bu pakette **hiçbir menzil/görüş/ulus denetimi yapmaz**, yani guard olmazsa bot haritadaki herhangi bir oyuncunun kesin HP'sini öğrenebilirdi (`docs/03` §16: görüş alanı dışı = yasak). Bu yüzden paket sunucuya gitmeden önce `BotFairnessGuard` kurallarından geçer: hedef botun 3×3 bölgesinde olmalı (`out_of_view`), seçili hedefin yoklaması ≥ 2000 ms arayla (`poll`), CLI-11 aksiyon hızı. Sonuç, sunucunun gönderdiği `WIZ_TARGET_HP` cevabından okunur. Karar katmanı yoktur: aksiyonu `/bot target <bot> <hedef bot>` komutu tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün altıncı dilimidir (ADR-0017 Ek F4-06; hareket → saldırı → cast → pot → duruş → **`TargetHpReq`** → ölüm/`Regene` → `Party`/`Chat` → `Perception`).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-06)" (bu planla birlikte yazıldı): paket düzenleri, guard kuralları, `echo` seçimi, sonuç eşlemesi, kapsam.
- `docs/03` §14 **CLI-10** (bu planla birlikte genişletildi), **CLI-11**; §16 gözlem sözleşmesi tablosu ("Seçili hedefin HP'si: tek seçili hedef, ≤ 2 istek/sn").
- `plans/F4-05-aksiyon-yurutucu-durus.md` §5.2–§5.5 ve Doğrulama Raporu: bu planın kalıpladığı iskelet (saf mantık `BotCore/BotCombat.h` + `ActionExecutor` + `BotSession` durumu + `BotManager` komutu). **Yazılı planı değil, birleşmiş kodu esas al** (`ActionExecutor.cpp` `NextDecisionId` `:36`, `EmitFairnessReject` `:53`, `RejectStance` `:1271`, `SetStance` `:1300`).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `612462e` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:356-362` — `HandlePacket` `case WIZ_TARGET_HP`: `uid = pkt.read<uint16>()`, `echo = pkt.read<uint8>()`, `m_targetID = uid` (seçili hedef), `SendTargetHP(echo, uid)`. Başka bir ön koşul yok.
  - `GameServer/User.cpp:2356-2387` — `CUser::SendTargetHP(echo, tid, damage)`: `tid >= NPC_BAND` ise NPC yolu (`m_bPointCheckFlag` ve `GetNpcPtr`), değilse `GetUserPtr(tid)`; kullanıcı yoksa veya **ölüyse sessizce döner** (`:2375-2378`); aksi halde `Packet result(WIZ_TARGET_HP); result << uint16(tid) << echo << maxhp << hp << uint16(damage); Send(&result);` (`:2384-2386`; `maxhp`/`hp` `int` = 4 bayt; `CUser::Send` botun alıcısına yönlendirir, `User.cpp:19-23` `m_botSink->OnPacket`): cevap yükü **13 bayt**: `u16 tid @0, u8 echo @2, i32 maxHp @3, i32 hp @7, u16 damage @11`. **Menzil, görüş alanı, ulus denetimi yok.**
  - `GameServer/User.cpp:2013`, `GameServer/Npc.cpp:223` — saldırgana her hasar sonrası `SendTargetHP(0, hedef, hasar)` (aynı opcode, `echo = 0`, `damage > 0`): bot saldırı yaptığında da `WIZ_TARGET_HP` alır. `OnPacket()` bunları da kaydeder; yürütücü isteğin hemen öncesinde kaydı sıfırlar ve `tid` + `echo` eşleştirir (aynı thread, saldırı ve istek aynı `Tick()`'te iç içe çalışmaz).
  - `shared/KOSocket.h:25,44` — `uint16 m_targetID` (`GetTargetID()`); sunucuda başka okuyan yok (`grep -rn m_targetID GameServer/` yalnızca `User.cpp:360`). Bot bu alana **yazmaz**; yalnızca `HandlePacket` yazar.
  - `GameServer/Unit.h:84-85` — `GetNewRegionX/Z() = (uint16)(GetX()) / VIEW_DISTANCE`; `shared/globals.h:19` `VIEW_DISTANCE 48`.
  - `GameServer/Bot/BotSession.cpp:25-70` — `OnPacket()` (`WIZ_ATTACK`, `WIZ_MAGIC_PROCESS`, `WIZ_STATE_CHANGE` blokları): bu planın ekleyeceği `WIZ_TARGET_HP` bloğunun kalıbı.
  - `GameServer/Bot/BotManager.cpp:591-640` `ExecuteCommand` (fiil dağıtımı, `unknown command` listesi `:638`), `:1185` `CommandAttack` (iki bot adı arama kalıbı), `:1670` `CommandStance` (kalıp), `:1985-1998` hedef görünümünün `AttackTarget tv = { (int16)t->m_pUser->GetSocketID(), GetX(), GetZ() }` ile doldurulması, `:785-830` `BuildStatusLines()`.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** `kTargetHpPollMs`, `kViewDistance`, `kViewRegionRadius`, `RegionIndex`, `RegionDelta`, `TargetHpCheck`, `TargetHpVerdict`, `CheckTargetHp`. Birim testleri (`CombatTests.cpp`).
2. **`ActionExecutor::RequestTargetHp(s, target, now)`** (tek seferlik aksiyon): ön koşullar → `BotCore::CheckTargetHp` → `WIZ_TARGET_HP` paketi → `HandlePacket` → yayınlanan cevaptan sonuç eşleme → telemetri.
3. **Oturum durumu (`BotSession`):** seçili hedef kimliği, son istek zamanı (spawn başına) ve `OnPacket()`'in doldurduğu `m_targetHpEcho` / `m_targetHpValues`.
4. **Komut (`BotManager`):** `target <bot> <hedef bot>` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten).
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT` / `ACTION_RESULT` (`"type":"TargetHpReq"`), `FAIRNESS_REJECT` (`"type":"TargetHp"`).

**Kapsam dışı (yapılmayacak)**

- **Ne zaman hedef seçilip HP sorulacağı** (karar katmanı, `docs/09`/`docs/10`): yok. `target` komutu görüş/yoklama/hız dışında hiçbir koşula bakmaz.
- **NPC/canavar hedefleri** (`tid >= NPC_BAND`): yok; hedef her zaman başka bir botun oturumudur (`FindSession`). Gerçek oyuncu (insan) hedefi için de komut yok.
- **Gerçek istemcinin seçimde yolladığı ikinci paket** (`echo=1` ve 1 ms sonra `echo=0`): bot tek paket yollar (ADR-0017 Ek F4-06 madde 3).
- **Alınan HP bilgisini bir algı akışına/hedef tablosuna bağlamak** (`Perception`): yok. Cevap yalnızca `ACTION_RESULT` telemetrisine ve komut günlüğüne yazılır; başka bir bot koduna beslenmez.
- **Seçili hedefin otomatik yoklanması (2 sn zamanlayıcısı), hedef ölünce seçimi bırakma:** yok. Her istek elle verilir; oturumda yalnızca "son seçilen hedef" tutulur.
- `ChatHandler.cpp` `+bot` yardım metni (KI-012; Claude doğrulamada günceller). Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya yok; **`GameServer` projesine dosya eklenmez**. `Telemetry.*`, `ScenarioRunner.*` değişmez; `list` satırı değişmez.
- Dokümanları (`docs/03`, `docs/13`, `docs/16`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | Yalnızca ekleme (§5.2); mevcut içerik ve `#include`'lar değişmez |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | Yalnızca ekleme: üç yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `TargetHpTarget`, `TargetHpOutcome`, bir yeni statik fonksiyon |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | Yalnızca dosya sonuna ekleme (§5.4); başka hareket/saldırı/cast/pot/duruş kodu değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca hedef-HP durumu üyeleri |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi, `ResetForRespawn()` ve `OnPacket()`'e **ekleme** bloğu (mevcut bloklar değişmez) |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandTarget` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, komut, `unknown command` listesi |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (Yeni dosya açılmaz: guard `BotCombat.h`'ye eklenir, böylece `BotCore*.vcxproj` değişmez.)

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-06 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotCombat.h` (saf mantık, mevcut `namespace BotCore` içine ekleme)

Mevcut kodun biçimini (tab, Allman, `inline`, İngilizce kısa yorum, ölçülmemiş değer `[A]`) koru. Yeni `#include` gerekmez. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez**. `CheckStance`'tan **sonra**, `namespace`'in kapanışından önce ekle. **`std::min`/`std::max` kullanma** (`GameServer` bu başlığı `windows.h` makrolarıyla birlikte derler); karşılaştırmaları elle yaz:

```cpp
	// --- target HP slice (ADR-0017 Ek F4-06) ---

	constexpr uint32_t kTargetHpPollMs   = 2000;   // docs/03 CLI-10 / Q-18: the client polls the selected target every 2.0 s (p50 2001 ms) [A]
	constexpr int      kViewDistance     = 48;     // shared/globals.h VIEW_DISTANCE: region edge in metres
	constexpr int      kViewRegionRadius = 1;      // the client sees the 3x3 regions around its own (docs/03 section 16)

	// Region index of a coordinate: the server's (uint16)(coord) / VIEW_DISTANCE (Unit.h GetNewRegionX/Z).
	// Clamped to 0..65535 because the server's cast is only defined there.
	inline int RegionIndex(float coord);

	// Chebyshev distance between the region indices of two positions (0 = same region, 1 = adjacent, incl. diagonal).
	inline int RegionDelta(float ax, float az, float bx, float bz);

	struct TargetHpCheck
	{
		int regionDelta;          // RegionDelta(bot position, target position)
		bool sameTarget;          // the request re-polls the currently selected target (same id as the previous request)
		bool hasLast;             // a target HP request was sent earlier in this spawn
		uint32_t sinceLastMs;     // since that request
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum TargetHpVerdict
	{
		TARGETHP_OK = 0,
		TARGETHP_REJECT_VIEW = 1,   // CLI-10 (target outside the 3x3 regions)
		TARGETHP_REJECT_POLL = 2,   // CLI-10 (same target polled again before kTargetHpPollMs)
		TARGETHP_REJECT_RATE = 3    // CLI-11
	};

	// Guard rule for a target HP request. Order: view, poll, rate.
	// Selecting a different target is not rate limited by CLI-10 (a human can click quickly); CLI-11 still applies.
	inline TargetHpVerdict CheckTargetHp(const TargetHpCheck & c);
```

Gövdeler: `RegionIndex`: `coord < 0` → 0; `coord > 65535` → 65535; `return (int)((uint16_t)coord) / kViewDistance;`. `RegionDelta`: iki ekseni ayrı farklarla (`dx`, `dz`, mutlak değer elle), büyüğü döndür. `CheckTargetHp`: `c.regionDelta > kViewRegionRadius` → `TARGETHP_REJECT_VIEW`; `c.sameTarget && c.hasLast && c.sinceLastMs < kTargetHpPollMs` → `TARGETHP_REJECT_POLL`; `c.actionsInWindow >= kMaxActionsPerWindow` → `TARGETHP_REJECT_RATE`; aksi halde `TARGETHP_OK`. Mevcut `CheckStance` kalıbını izle (yukarıdaki imzalar bildirimdir; gövdeleri aynı yerde `inline` tanımla).

**`Tests/BotCoreTests/CombatTests.cpp` (ekleme, mevcut makro stili):** üç yeni `TEST_CASE`:

- `Combat_RegionIndex_RegionDelta`: `RegionIndex(0) == 0`, `(47.9f) == 0`, `(48.0f) == 1`, `(95.9f) == 1`, `(96.0f) == 2`, `(-5.0f) == 0`, `(70000.0f) == 65535 / 48` (= 1365). `RegionDelta(10,10,10,10) == 0`; `RegionDelta(47.9f,0,48.0f,0) == 1` (sınırın iki yanı); `RegionDelta(0,0,48,48) == 1` (çapraz); `RegionDelta(0,0,96,0) == 2`; `RegionDelta(0,0,48,96) == 2` (eksenlerin büyüğü); `RegionDelta(100,100,100,148) == 1` (`100/48 = 2`, `148/48 = 3`); simetri: `RegionDelta(a,b) == RegionDelta(b,a)` en az bir çift için.
- `Combat_TargetHpCheck_Order`: yardımcı `OkTargetHp()` (`regionDelta = 0, sameTarget = true, hasLast = true, sinceLastMs = 5000, actionsInWindow = 0`). Hepsi ihlal (`regionDelta = 2, sinceLastMs = 0, actionsInWindow = 6`) → `TARGETHP_REJECT_VIEW` (sıra); `regionDelta = 1` (sınırda, görüş içinde) ve diğerleri ihlal → `TARGETHP_REJECT_POLL`; `sinceLastMs = 1999` → `TARGETHP_REJECT_POLL`; `sinceLastMs = 2000` → `TARGETHP_OK`; `actionsInWindow = 6` (diğerleri geçerli) → `TARGETHP_REJECT_RATE`; `actionsInWindow = 5` → `TARGETHP_OK`.
- `Combat_TargetHpCheck_SwitchAndFirst`: `sameTarget = false, hasLast = true, sinceLastMs = 0` (farklı hedef, hemen) → `TARGETHP_OK`; `sameTarget = true, hasLast = false, sinceLastMs = 0` (spawn sonrası ilk istek) → `TARGETHP_OK`; `sameTarget = false, regionDelta = 2` → `TARGETHP_REJECT_VIEW` (farklı hedef de görüşe bağlı); `kTargetHpPollMs == 2000`, `kViewDistance == 48`, `kViewRegionRadius == 1`.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'de duruş üyelerinin altına (aynı yorum/hizalama biçimi):

```cpp
	int m_hpReqTargetId;                                   // IOCP thread only: id of the selected target (last WIZ_TARGET_HP request), -1 = none
	bool m_hpReqHasLast;                                   // IOCP thread only: m_hpReqLast is valid for this spawn
	std::chrono::steady_clock::time_point m_hpReqLast;     // IOCP thread only: when the last WIZ_TARGET_HP request went out
```

ve atomik üyelerin yanına (`m_stateEcho`'nun altına):

```cpp
	std::atomic<uint64> m_targetHpEcho;                    // written by OnPacket(): valid bit | echo << 16 | tid of the last WIZ_TARGET_HP reply
	std::atomic<uint64> m_targetHpValues;                  // written by OnPacket() BEFORE m_targetHpEcho: hp << 32 | maxHp
```

`BotSession.cpp`: başlatıcı listesine `m_hpReqTargetId(-1)`, `m_hpReqHasLast(false)`, `m_targetHpEcho(0)`, `m_targetHpValues(0)` ekle (mevcut sıraya uy, üye bildirim sırasıyla aynı sırada: derleyici sıra uyarısı vermemeli). `ResetForRespawn()` içine `m_hpReqTargetId = -1; m_hpReqHasLast = false; m_targetHpEcho = 0; m_targetHpValues = 0;` ekle.

`OnPacket()`: `WIZ_STATE_CHANGE` bloğundan **sonra**, **yalnızca ekleme** (mevcut bloklar bayt bayt aynı kalır):

```cpp
	// Target HP reply: u16 tid, u8 echo, i32 maxHp, i32 hp, u16 damage (User.cpp:2384-2386). Every WIZ_TARGET_HP the bot
	// receives is recorded (request replies and attacker-side damage notices alike); ActionExecutor::RequestTargetHp
	// clears the record before its request and matches tid + echo afterwards, on the same thread.
	// The values word is written first so a reader that sees the valid bit also sees the values.
	if (opcode == WIZ_TARGET_HP && pkt.size() >= 13)
	{
		uint16 tid = pkt.read<uint16>(0);
		uint8 echo = pkt.read<uint8>(2);
		int32 maxHp = pkt.read<int32>(3);
		int32 hp = pkt.read<int32>(7);
		m_targetHpValues = (uint64(uint32(hp)) << 32) | uint64(uint32(maxHp));
		m_targetHpEcho = (1ull << 63) | (uint64(echo) << 16) | uint64(tid);
	}
```

(`pkt.read<T>(offset)` bayt ofsetiyle okur; mevcut bloklardaki gibi. Ofset semantiğini ve `int32`/`uint64` tür adlarını `shared/ByteBuffer.h`, `shared/types.h`'den teyit et.)

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

**Başlık (`ActionExecutor.h`):** `StanceOutcome`'dan sonra:

```cpp
// Caller-supplied view of the HP request target (ADR-0017 Ek F4-06). Temporary, like AttackTarget: the /bot target
// test driver fills it from the target bot's session; the Perception slice replaces the source, not this struct.
struct TargetHpTarget
{
	int16 id;      // target's socket id (WIZ_TARGET_HP 'uid')
	float x;       // metres
	float z;
};

struct TargetHpOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "observed" (SENT), "no_result" (FAILED),
	                       // REFUSED: "not_in_game", "dead", "bad_target", "out_of_view", "poll", "rate"
	int32 hp;              // from the server's reply; valid only when kind == SENT
	int32 maxHp;
};
```

`class ActionExecutor` içine (`SetStance` bildiriminin altına, aynı yorum kalıbıyla):

```cpp
	// One-shot target selection + HP request: sends one WIZ_TARGET_HP through CUser::HandlePacket() after the guard
	// (CLI-10: target inside the bot's 3x3 regions, same target re-polled at most every 2 s; CLI-11) and maps the result
	// from the reply the server published (m_targetHpEcho / m_targetHpValues). SENT "observed": a reply for that target
	// arrived (hp / maxHp filled). FAILED "no_result": no reply (target dead or unknown to the server).
	// REFUSED: "not_in_game", "dead", "bad_target" (id < 0 or the bot itself) or a guard verdict ("out_of_view", "poll",
	// "rate"; FAIRNESS_REJECT written).
	static TargetHpOutcome RequestTargetHp(BotSession * s, const TargetHpTarget & target,
		std::chrono::steady_clock::time_point now);
```

**`ActionExecutor.cpp`** (dosya sonuna, `SetStance`'ten sonra; yeni bölüm başlığı `// --- target HP slice (ADR-0017 Ek F4-06) ---`):

1. **`static TargetHpOutcome RejectTargetHp(BotSession * s, CUser * user, BotCore::TargetHpVerdict v, const BotCore::TargetHpCheck & c)`** (`RejectStance` kalıbı): `VIEW` → `rule "CLI-10", reason "out_of_view", value = regionDelta, limit = kViewRegionRadius`; `POLL` → `rule "CLI-10", reason "poll", value = sinceLastMs, limit = kTargetHpPollMs`; `RATE` → `rule "CLI-11", reason "rate", value = actionsInWindow, limit = kMaxActionsPerWindow`. `decisionId = NextDecisionId(s)`; `EmitFairnessReject(s, user, decisionId, "TargetHp", rule, reason, value, limit)`; `REFUSED` + reason; `hp = maxHp = 0`. Sunucuya paket **gitmez**.
2. **`ActionExecutor::RequestTargetHp`:**
   - `s == nullptr || s->m_pUser == nullptr || !isInGame()` → `REFUSED "not_in_game"`; `isDead()` → `REFUSED "dead"`. Otururken **serbest** (oturma kontrolü yok).
   - `target.id < 0 || target.id == (int16)user->GetSocketID()` → `REFUSED "bad_target"` (olay yazılmaz).
   - `nowMs` (`SetStance` ile aynı `steady_clock` → ms dönüşümü), `inWindow = s->m_actionWindow.CountInWindow(nowMs)`; `sinceLastMs` (`m_hpReqHasLast` ise `now - m_hpReqLast`, ms).
   - `TargetHpCheck c = { BotCore::RegionDelta(user->GetX(), user->GetZ(), target.x, target.z), s->m_hpReqTargetId == (int)target.id, s->m_hpReqHasLast, sinceLastMs, inWindow }`; `verdict != TARGETHP_OK` → `RejectTargetHp`.
   - **Gönder:** `uint8 echo = (s->m_hpReqTargetId == (int)target.id) ? 0 : 1;` (yeni/farklı hedef = seçim, `echo=1`; seçili hedefin yoklaması `echo=0`). `decisionId = NextDecisionId(s)`; `ACTION_SUBMIT` (`decisions`): `"decision_id","type":"TargetHpReq","target":<id>,"echo":<0|1>`. Paket: `Packet pkt(WIZ_TARGET_HP); pkt << uint16(target.id) << uint8(echo);` (sunucu `u16 + u8` okur, `User.cpp:358-359`). `s->m_targetHpEcho = 0; s->m_targetHpValues = 0;` → `user->HandlePacket(pkt)` (gecikme `steady_clock` ile) → `s->m_actionWindow.Record(nowMs)`; `s->m_hpReqTargetId = target.id; s->m_hpReqHasLast = true; s->m_hpReqLast = now;` (sunucu `m_targetID`'yi cevap olsun olmasın yazar, bot da seçimi her gönderimde günceller).
   - **Sonuç:** `e = s->m_targetHpEcho.load()`; `(e & (1ull << 63)) != 0 && (uint16)(e & 0xFFFF) == (uint16)target.id && (uint8)((e >> 16) & 0xFF) == echo` ise `observed`: `v = s->m_targetHpValues.load()` → `maxHp = (int32)(uint32)(v & 0xFFFFFFFF)`, `hp = (int32)(uint32)(v >> 32)`; aksi halde `no_result`.
   - `ACTION_RESULT`: `"decision_id","type":"TargetHpReq","ok","reason":"observed"|"no_result","latency_us"` ve yalnızca `observed` iken `"hp":<hp>,"max_hp":<maxHp>`. `hp`/`max_hp` **yalnızca cevap paketinden** gelir (sunucu `CUser`/`Unit` nesnesine bakılmaz).
   - Dönüş: `observed` → `SENT "observed"` (`hp`, `maxHp` dolu), değilse `FAILED "no_result"`.

`HandlePacket(pkt)` çağrısı hedef HP için **tek yerde** (`RequestTargetHp`). `m_targetID` yazılmaz, `SendTargetHP` çağrılmaz, hedefin `m_sHp`/`m_iMaxHp`/`GetUserPtr` okunmaz.

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirim: `void CommandTarget(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()` (`:591-640`):** `stand` dalının yanına `target` fiilini ekle (`CommandTarget(args)`); "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast, pot, sit, stand, target)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **`CommandTarget(args)`:** `CommandAttack` / `CommandStance` kalıbını izle (`SplitWords`, `FindSession`, `IsKnownBotName`, `PhaseName`, `WriteBotLog`). Kullanım satırı (`words.size() != 2` iken **tek** günlük satırı ve dön): `BotManager: cmd target: usage: target <bot> <target bot>`. Sırayla: bot yoksa `BotManager: cmd target: unknown or not spawned bot '<ad|?>'`; bot `PHASE_IN_GAME` değilse `BotManager: cmd target: <bot> not in game (phase X)`; hedef oturum yoksa `unknown or not spawned bot '<ad|?>'`; hedef `PHASE_IN_GAME` değilse `BotManager: cmd target: target <hedef> not in game (phase X)`. `TargetHpTarget tv = { (int16)t->m_pUser->GetSocketID(), t->m_pUser->GetX(), t->m_pUser->GetZ() };` (`:1997` ile aynı kaynak; yorum: "Perception replaces this source"). `ActionExecutor::RequestTargetHp(s, tv, std::chrono::steady_clock::now())`: `REFUSED` → `BotManager: cmd target: <bot> refused (<reason>)`; `SENT` → `BotManager: cmd target: <bot> observed <hedef> hp <hp>/<maxHp>`; `FAILED` → `BotManager: cmd target: <bot> failed (<reason>)`. `char message[256]`.
3. **`TickSessions()`, `BeginDespawn()`, `BuildStatusLines()`:** **değişmez** (tek seferlik aksiyon, seri yok, `list` biçimi sabit).
4. Başka hiçbir yere dokunma (`ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj`, `.filters`, `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` **değişmez** (yeni dosya yok). Mevcut dosyaların kodlama/satır sonu/BOM durumu korunur.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı üç yeni test adını (`Combat_RegionIndex_RegionDelta`, `Combat_TargetHpCheck_Order`, `Combat_TargetHpCheck_SwitchAndFirst`) içerir ve toplam test sayısı **31**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; dosyada `#include` yalnızca `<algorithm>` ve `<cstdint>`; `grep -n "std::min\|std::max" BotCore/BotCombat.h` yeni satır göstermez (mevcut kullanım varsa yalnızca eski satırlar).
- [ ] K5: hedef HP paketi yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_TARGET_HP" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma, `RequestTargetHp`) ve `BotSession.cpp` (`OnPacket` sonuç okuma) satırlarını gösterir; `grep -n "SendTargetHP\|m_targetID\|GetTargetID" GameServer/Bot/*.cpp GameServer/Bot/*.h` boş.
- [ ] K6: `RequestTargetHp`'te guard atlanmıyor: `HandlePacket`'tan önce `CheckTargetHp` çağrısı ve `TARGETHP_OK` dışında erken dönüş vardır; hedef HP için `HandlePacket(pkt)` çağrısı tek yerde (kod okumasıyla; Claude çalışma zamanında da sınar).
- [ ] K7: sonuç yalnızca cevap paketinden: `RequestTargetHp` gövdesinde `m_sHp`, `m_iMaxHp`, `GetUserPtr`, `GetHealth`, `GetMaxHealth` geçmez (`grep -n "m_sHp\|m_iMaxHp\|GetUserPtr\|GetHealth\|GetMaxHealth" GameServer/Bot/ActionExecutor.cpp` bu fonksiyonun satır aralığında boş); `hp`/`maxHp` `m_targetHpValues`'tan okunur.
- [ ] K8: `OnPacket()` yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-06 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki bilinçli değiştirilen satır(lar)ı gösterir; `OnPacket()` mevcut `WIZ_SEL_CHAR`/`WIZ_ATTACK`/`WIZ_MAGIC_PROCESS`/`WIZ_STATE_CHANGE` blokları değişmedi.
- [ ] K9: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutla çalışır; `git diff gece/2026-10-02...bot/F4-06 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca `unknown command` mesaj satırını gösterir; `Startup()`/`Tick()`/`TickSessions()`/`BuildStatusLines()` ve ini okuma değişmedi.
- [ ] K10: `git diff --stat gece/2026-10-02...bot/F4-06` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K11: değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K12: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K13: F4-01..F4-05 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack" ...` ≥ 1, `grep -c "CheckCastStart" ...` ≥ 1, `grep -c "CheckPotion" ...` ≥ 1, `grep -c "CheckStance" ...` ≥ 1; önceki 28 testin tamamı hâlâ geçiyor; `EmitFairnessReject` çağrıları `"Move"`/`"Attack"`/`"Cast"`/`"Potion"`/`"State"` geçiyor, yeni `"TargetHp"` eklendi.
- [ ] K14 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-06
git diff gece/2026-10-02...bot/F4-06 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-06 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-06 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "std::min\|std::max" BotCore/BotCombat.h
grep -n "WIZ_TARGET_HP" GameServer/Bot/*.cpp
grep -n "SendTargetHP\|m_targetID\|GetTargetID" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "CheckTargetHp\|HandlePacket" GameServer/Bot/ActionExecutor.cpp
grep -n "m_sHp\|m_iMaxHp\|GetUserPtr\|GetHealth\|GetMaxHealth" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-06
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; **bir dosyadaki satırlar aynı `Tick()`'te sırayla çalışır**, ayrı dosyalar ≥ 1,05 sn arayla verilmelidir). Botlar: Karus warrior `BotWP_K`, El Morad warrior `BotWP_E`, Karus mage `BotMF_K`; zone 71. Gözlem `Logs/bots/<tarih>/live-*.jsonl`'den, telemetri `decisions` ile. Beklenmeyen `no_result` bu planın hatası değil, **sonuç olarak raporlanır** (özellikle sunucunun `WIZ_TARGET_HP` cevabını botun kendi alıcısına gönderip göndermediği: `Send()` doğrulaması; göndermiyorsa **durup** raporla, bot sunucu nesnesine bakarak "başarılı" demez).

1. **Mutlu yol:** `spawn BotWP_K,BotWP_E`; botları aynı bölgeye getir (`move`, ≤ 48 m); `target BotWP_K BotWP_E` → log `observed BotWP_E hp <hp>/<maxHp>` (`list` satırındaki `hp=` ile aynı olmalı: operatör karşılaştırması, sonuç eşlemesi buna bakmaz); JSONL: `ACTION_SUBMIT` (`type:"TargetHpReq"`, `target`, `echo:1`) → `ACTION_RESULT` (`ok:true`, `reason:"observed"`, `hp`, `max_hp`, `latency_us` < 5000); `FAIRNESS_REJECT` yok.
2. **Yoklama (`poll`) ve seçim:** ayrı dosyalarla: ilk `target BotWP_K BotWP_E` (`echo:1`), ≥ 1,05 sn sonra aynı komut → `refused (poll)` + `FAIRNESS_REJECT` (`type:"TargetHp"`, `rule:"CLI-10"`, `reason:"poll"`, `value` ≈ 1000–1900, `limit` 2000), JSONL'de `ACTION_SUBMIT` yok; ≥ 2,05 sn sonra tekrar → `observed`, `echo:0`. `BotWP_K` farklı bir hedefe (`target BotWP_K BotMF_K`, aynı bölgede) hemen → `observed`, `echo:1` (seçim değişimi `poll`a takılmaz).
3. **Görüş (`out_of_view`):** `BotWP_E`'yi `BotWP_K`'dan ≥ 2 bölge (≥ 96 m) uzağa götür (aynı zone 71'de; konumlar `list` `pos=`); `target BotWP_K BotWP_E` → `refused (out_of_view)` + `FAIRNESS_REJECT` (`CLI-10`, `out_of_view`, `value` ≥ 2, `limit` 1), sunucuya paket gitmedi (JSONL'de `ACTION_SUBMIT` yok; `GameServer.log`/paket sayacı değişmedi). Sonra yaklaştır (komşu bölge, 48–96 m aralığında bölge sınırı ötesi) → `observed` (komşu bölge görüş içidir). Bu sınama bölge sınırlarına bağlı: kullanılan konumlar ve bölge indeksleri raporlanır.
4. **`bad_target` / `no_result`:** `target BotWP_K BotWP_K` → `refused (bad_target)`, olay yazılmaz. Hedef bot ölüyse (öldürmek mümkünse `attack BotWP_K BotWP_E <n>` ile; mümkün değilse bu senaryo raporda "sınanmadı" yazılır) `target` → `failed (no_result)` ve `ACTION_RESULT` `ok:false`, `reason:"no_result"`.
5. **Oturum/ömür döngüsü:** `sit BotWP_K` sonrası `target BotWP_K BotWP_E` → `observed` (otururken serbest); `despawn BotWP_E` sonrası `target BotWP_K BotWP_E` → `unknown or not spawned bot` / `not in game (phase despawned)`; `despawn BotWP_K` + yeniden `spawn BotWP_K` sonrası ilk `target` hemen gider (`poll` reddi yok: spawn başına zamanlayıcı ve seçim sıfırlandı, `echo:1`).
6. **Reddedilen komutlar:** `target Ghost BotWP_E` / `target BotWP_K Ghost` → `unknown or not spawned bot '?'`; argümansız ve `target BotWP_K` / `target BotWP_K BotWP_E x` → kullanım satırı; `RESPAWN_CYCLES=2` iken `target` → `cmd rejected` (yeniden sınanmazsa raporda söylenir).
7. **Gerilemesiz:** `attack BotWP_K BotWP_E 3` aynı çıktıyı verir (F4-02; saldırı sırasında gelen `WIZ_TARGET_HP` bildirimleri `target` sonucunu bozmaz: `attack` ardından ≥ 1,05 sn sonra `target` → `observed`); `sit`/`stand` (F4-05), `pot` (F4-04), `cast BotMF_K 110518 BotWP_E 2` (F4-03), 30 m `move` (F4-01) önceki çıktıyı verir; `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama `target` çalışır; `ENABLED=0` → komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde (≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. İnsan istemcisi gerekmez (gerçek istemcide hedef çubuğu görsel doğrulaması `T-ARCH-11`, `docs/STATUS.md` "Proje sahibi testleri").

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). `target` yalnızca `ENABLED=1` iken, üretim dışı test komutudur.
- **Thread kuralı (ADR-0005):** `ActionExecutor` yalnızca IOCP thread'inde (komut çekirdeği `Tick()` içinden) çalışır; `HandlePacket` zaten bu thread'de koşar. Konsol/`+bot` işleyicisi `BotSession`/`ActionExecutor`'a **dokunmaz**; komutlar `EnqueueCommand` kuyruğundan gelir. `OnPacket()` her thread'den çağrılabilir: yeni blok yalnızca iki atomik yazar, başka duruma dokunmaz (yazma sırası: önce `m_targetHpValues`, sonra `m_targetHpEcho`).
- **Sonuç yalnızca yayınlanan cevaptan** (AC-LRN-03 / `docs/13` §8). Sunucu nesnesinden HP okumak (`pUser->m_sHp` vb.) **yasak**: bu, sunucunun menzil denetimi yapmamasını bot lehine kullanmak olurdu. Telemetriye yazılan `hp`/`max_hp` yalnızca cevaptan gelir.
- **`m_targetID`'ye yazma yok:** yalnızca `HandlePacket` yazar (sunucunun kendi handler'ı); `SendTargetHP` bot kodundan çağrılmaz.
- **Bilinen sınırlar `[A]`:** (a) `Send()`'in cevabı botun kendi alıcısına ilettiği varsayımı (kod okumasıyla öyle görünüyor: `CUser::Send` bot alıcısına yönlendirilir, F2-01; doğrulamada teyit edilir; iletmiyorsa `no_result` raporlanır ve plan düzeltmesi gerekir: **kendi başına nesneye bakarak başarı ilan etme**, **durup** raporla); (b) `kTargetHpPollMs = 2000` F1 ölçümü (Q-18 p50 2001 ms) ama tek oturum; (c) görüş yarıçapı 3×3 bölge, bölge indeksi pozisyondan hesaplanır (`m_sRegionX` değil): sunucunun gerçek bölge üyeliği hareket sırasında bir paket gecikebilir, fark en çok bir bölge sınırında görülür; (d) NPC hedefi ve ikinci seçim paketi yok.
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

### Tur 1 — YYYY-MM-DD

- Karar:
- İncelenen:
- Kriter sonuçları:
- Bulgular:
- Düzeltme talimatı:
