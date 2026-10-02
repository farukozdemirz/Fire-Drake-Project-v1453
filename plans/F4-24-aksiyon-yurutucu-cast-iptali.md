# F4-24: `ActionExecutor` cast iptali dilimi — `MAGIC_FAIL -100`, hareketle iptal, `UseStanding` otomatik durdurma

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-24` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-03 (cast dilimi: `BeginCast`/`TickCast`/`SubmitCast`, `m_castEcho`) — `KAPANDI`; F4-01 (`TickMove`/`StopMove`) — `KAPANDI`; F4-04 (CLI-11 `m_actionWindow` kullanımı) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-03 (cast sırasında hareket varsa iptal; iptal paketi), CLI-09 (`UseStanding` ⇒ durma + bir tick), CLI-11, MEC-MAG-01, MEC-MAG-07, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 dilim 1 |
| Tahmini büyüklük | M (7 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

F4-03 cast'i bilerek eksik bıraktı (ADR-0017 Ek F4-03 madde 6; `docs/17` F4 kapsamı ADR-0018 ile genişledi): bot bir cast'i iptal edemiyor (`/bot cast <bot> off` seriyi **sessizce** düşürüyor, sunucuya hiçbir şey gitmiyor) ve yürürken `UseStanding = 1` skill'i `not_standing` ile reddediliyor. Bu plan üç davranışı ekler:

1. **Cast iptali:** CASTING gönderilmiş, EFFECTING henüz gönderilmemiş bir cast için botun gerçek istemcinin yaptığı gibi **`WIZ_MAGIC_PROCESS` opcode 4 (`MAGIC_FAIL`), `sData[3] = -100`** paketini `CUser::HandlePacket()` üzerinden göndermesi (`docs/03` CLI-03 `[V]`, ölçülen istemci davranışı).
2. **Hareketle iptal:** cast CASTING aşamasındayken bot yürümeye başlarsa (ya da yürüyorsa) **önce iptal paketi, sonra hareket paketi** (istemci ölçümü: iptal MOVE'dan 5–8 ms önce).
3. **`UseStanding` otomatik durdurma:** `UseStanding = 1` skill'i, bot yürürken seriye alınmışsa bot **önce durma paketi gönderir, bir tick bekler, sonra cast eder** (CLI-09; bugün `not_standing` ile reddediliyor).

Sunucu cast durumu tutmaz (MEC-MAG-01), iptal paketini yalnızca **çağırana** yankılar (`MagicInstance.cpp` `case MAGIC_FAIL: SendSkill(false)`); iptalin sunucu tarafı mekanik etkisi yoktur (MP EFFECTING'te düşülür, `docs/03` MEC-MAG-08). Planın değeri **protokol sadakati ve botun kendi cast durum makinesinin tamamlanmasıdır**: F6 davranışları (mage/priest) cast'i yarıda kesebilmelidir.

F4'ün yirmi dördüncü planıdır (ADR-0018 sırası: **1. cast iptali** → uçan skill'ler → çift tipli → Type4 → alan → ...).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-24)" (bu planla birlikte yazıldı) ve "Ek (F4-03)" (madde 6 bu planla kapanır): paket düzeni, iptal kuralları, `[A]` varsayımları. `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md`.
- `docs/03` §14 **CLI-03** (satır "Cast iptali (yürüyerek)": opcode 4, `sData[3] = -100`, MOVE'dan 5–8 ms önce, opcode 6 yok), **CLI-09**, **CLI-11**, **MEC-MAG-01**, **MEC-MAG-07**, **MEC-MAG-08**.
- `plans/F4-03-aksiyon-yurutucu-cast.md` ve birleşmiş kod: **yazılı planı değil, kodu esas al.**
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `43ad337` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicProcess.cpp:16-53` — `CMagicProcess::MagicPacket`: `u8 opcode, u32 skill`, sonra `i16 caster, i16 target, i16 sData[0..6]` (7 alan; istemci 6 gönderir, 7.'yi sunucu 0 okur). `:46-49` caster kimliği oturumun kendi kimliği olmalı.
  - `GameServer/MagicInstance.cpp:12-43` — `Run()`: `CheckSkillPrerequisites()` (`:296-308`: `MAGIC_FAIL` için `SkillUseOK`) ve `UserCanCast()` (`:320-325`: `MAGIC_FAIL` için erken `SkillUseOK`) geçilir; `case MAGIC_CASTING: case MAGIC_FAIL: SendSkill(bOpcode == MAGIC_CASTING)` → `MAGIC_FAIL` bölgeye değil **yalnızca çağırana** gider (`:763-777` `BuildAndSendSkillPacket`, `bSendToRegion = false`). Yanıt paketi `u8 opcode, u32 skill, i16 caster, i16 target, i16 sData[0..6]` (`:740-747`): `sData[3]` ofset 15.
  - `GameServer/MagicInstance.cpp:327-329` — `UseStanding == 1 && m_sSpeed != 0` ⇒ `SkillUseFail`; `GameServer/CharacterMovementHandler.cpp:18` `m_sSpeed = speed` (durma paketi hız 0 yazar).
  - `shared/packets.h:385` `MAGIC_FAIL = 4`; `:398` `SKILLMAGIC_FAIL_CASTING = -100`.
  - `GameServer/Bot/ActionExecutor.cpp`: `NextDecisionId` `:36`, `EmitFairnessReject` `:53`, `SubmitMove` `:71`, `StopMove` `:261` (her çıkışta `m_moveActive = false` bırakır: `SubmitMove` `:101`, `:133`, `:141-142`), `RejectCast` `:534`, `SubmitCast` `:585` (paket kalıbı `:606-609`, echo okuma `:621-645`), `BeginCast` `:667`, `TickCast` `:759` (hedef görünümü `:793`, `c.standing = !s->m_moveActive` `:842`, CASTING kabulü `:876-881`), `EndCast` `:951`.
  - `GameServer/Bot/ActionExecutor.h:44-51` `CastOutcome`; `:215` `EndCast` bildirimi.
  - `GameServer/Bot/BotSession.cpp:52-69` — `OnPacket()` `WIZ_MAGIC_PROCESS` bloğu: `op` 1..4 ve `caster == m_castSelfId` ise `m_castEcho = valid | op<<48 | uint16(sData3)<<32 | skill`. **`MAGIC_FAIL` yankısı zaten buradan geçer; blok değişmez.** `:11` başlatıcı listesi, `:327-329` `ResetForRespawn()` cast sıfırlama. `BotSession.h:90-102` cast alanları.
  - `GameServer/Bot/BotManager.cpp:2850-3100` — `TickSessions()`: canlı dalda `TickMove` `:2887`, ardından `m_castPhase != CAST_IDLE` bloğu `:2991-3057`; `:1377-1470` `CommandCast` (`off` dalları `:1404` ve `:1451`'de sessiz `EndCast`); `EndCast` kalan çağrıları `:2995` (ölüm), `:3020` (hedef kaybı), `:3163` (despawn).
  - `BotCore/BotCombat.h:244-256` `CheckCastEffect`, `:14` `kMaxActionsPerWindow = 6`. `Tests/BotCoreTests/CombatTests.cpp`: önceki 33 test (toplam 82).
- **Veri notu (Claude yerel `MAGIC` tablosunda doğruladı):** `UseStanding = 1` skill'leri yalnızca `301001..301006` (`Skill = 1010`, sınıf 101) ve `301007/301008` (Type 8, `Skill = 0`); 12 botun hiçbir skill'i `UseStanding = 1` değildir. Bu yüzden otomatik durdurmanın çalışma zamanı doğrulamasını DeepSeek yapamaz; Claude `/plan-dogrula`'da **geçici** bir `MAGIC` düzenlemesiyle sınar (§7, S5). DeepSeek veritabanına dokunmaz.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** `kCastCancelCode`, `CastCancelVerdict`, `CheckCastCancel`, `StandingPlan`, `PlanStanding`. Birim testleri (`CombatTests.cpp`).
2. **`ActionExecutor::CancelCast(s, cause, now)`:** CASTING aşamasındaki seri için guard + `MAGIC_FAIL -100` paketi + yayınlanan yankıdan sonuç eşleme + telemetri; ARMED aşamasındaki seri paketsiz düşürülür.
3. **Hareketle iptal:** `BotManager::TickSessions()` canlı dalında `TickMove`'dan **önce**: bot yürüyorsa ve cast CASTING aşamasındaysa `CancelCast(s, "move", now)`; guard `rate` ile reddederse o tick'te `TickMove` çalıştırılmaz (iptal hareketten önce gitmeli).
4. **`/bot cast <bot|all> off`:** sessiz `EndCast` yerine `CancelCast(s, "cmd", now)`; mesajlar §5.6'da.
5. **`UseStanding` otomatik durdurma:** `TickCast` ARMED aşamasında `UseStanding == 1` skill ve yürüyen bot ⇒ `StopMove` + bir tick bekle (`SENT "stopping"`); guard'daki `not_standing` kuralı **değişmez** (güvenlik ağı).
6. **Oturum durumu:** `BotSession::m_castTargetId` (CASTING'te gönderilen hedef kimliği; iptal paketi aynı hedefi taşır).

**Kapsam dışı (yapılmayacak)**

- Hedef kaybı ve ölümde iptal paketi (`BotManager.cpp:2995`, `:3020` sessiz `EndCast` olarak kalır; ölü bot paket gönderemez, hedef kaybı karar katmanı konusu) ve despawn (`:3163`, değişmez).
- Saldırı/pot/oturma gibi **diğer aksiyonların** cast'i iptal etmesi (docs/03 bunu ölçmedi; yalnızca hareket iptal eder).
- CASTING aşamasında **başlatılan yürüyüşün** engellenmesi: `/bot move` cast sırasında kabul edilir ve cast'i iptal eder (§5.3). Cast zamanı > 0 olan, `UseStanding = 0` bir skill yürürken seriye alınırsa CASTING gönderilir ve bir sonraki tick'te hareketle iptal edilir; bu bilinen sınırdır (karar katmanı F6'da önce durur). Cast zamanı 0 olan skill'ler CASTING aşamasına girmediği için etkilenmez.
- Uçan (FLYING) skill'ler, çift tipli, Type4, alan skill'leri, `opcode 6` (`MAGIC_CANCEL`, buff iptali), `MAGIC_FAIL` ile `sData[0..2]` konumunun gerçek istemci değeriyle doldurulması (bilinmiyor, `[A]`).
- Yeni komut, ini anahtarı, yeni telemetri olayı türü (yalnızca `ACTION_*`/`FAIRNESS_REJECT` altında yeni `type:"CastCancel"`), `list`/`snap` çıktısı değişikliği.
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | yalnızca ekleme: iptal/duruş mantığı (`CheckCastEffect`'ten sonra, "potion slice" başlığından önce) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | 2 yeni test (82 → 84) |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `CancelCast` bildirimi, `CastOutcome` sebep yorumu |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | `CancelCast`, `TickCast` içinde otomatik durdurma + `m_castTargetId` kaydı |
| `GameServer/Bot/BotSession.h` | değiştir | `int16 m_castTargetId` |
| `GameServer/Bot/BotSession.cpp` | değiştir | başlatıcı listesi + `ResetForRespawn()`; **`OnPacket()` değişmez** |
| `GameServer/Bot/BotManager.cpp` | değiştir | `TickSessions()` hareketten önce iptal, `CommandCast` `off` dalları |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`CheckCastEffect`'ten sonra, `// --- potion slice` satırından önce ekle (ASCII, CRLF, `namespace BotCore` içinde; yalnızca `<cstdint>`/`<algorithm>`):

```cpp
	// --- cast cancel and standing plan (ADR-0017 Ek F4-24) ---

	// docs/03 CLI-03 [V]: the client cancels a cast with MAGIC_FAIL (opcode 4) and sData[3] = -100 (SKILLMAGIC_FAIL_CASTING).
	constexpr int16_t kCastCancelCode = -100;

	enum CastCancelVerdict
	{
		CANCEL_OK = 0,
		CANCEL_REJECT_NOT_CASTING = 1,   // no CASTING packet is in flight, nothing to cancel
		CANCEL_REJECT_RATE = 2           // CLI-11
	};

	// Guard rule for the cancel packet. Order: not casting, rate. There is no minimum delay: the human picks the moment
	// (docs/03 CLI-03: 786..1408 ms after CASTING measured, no lower bound).
	inline CastCancelVerdict CheckCastCancel(bool casting, int actionsInWindow)
	{
		if (!casting)
			return CANCEL_REJECT_NOT_CASTING;

		if (actionsInWindow >= kMaxActionsPerWindow)
			return CANCEL_REJECT_RATE;

		return CANCEL_OK;
	}

	enum StandingPlan
	{
		STAND_PROCEED = 0,      // the cast may start (guard still checks CAST_REJECT_NOT_STANDING)
		STAND_STOP_FIRST = 1    // send a stop packet now, cast on a later tick
	};

	// docs/03 CLI-09 / MEC-MAG-07: a UseStanding skill needs speed 0 on the server, so a walking bot stops first and waits
	// at least one tick (the caller returns after the stop; the next Tick() re-evaluates).
	inline StandingPlan PlanStanding(bool needsStanding, bool moving)
	{
		return (needsStanding && moving) ? STAND_STOP_FIRST : STAND_PROCEED;
	}
```

`BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

Dosyanın sonuna iki `TEST_CASE` (mevcut `CHECK_EQ((int)...)` kalıbıyla):

- **`Combat_CastCancel_Guard`:** `kCastCancelCode == -100`; `CheckCastCancel(false, 0) == CANCEL_REJECT_NOT_CASTING`; `CheckCastCancel(false, 6) == CANCEL_REJECT_NOT_CASTING` (sıra: önce "casting değil"); `CheckCastCancel(true, 0) == CANCEL_OK`; `CheckCastCancel(true, 5) == CANCEL_OK`; `CheckCastCancel(true, 6) == CANCEL_REJECT_RATE` (sınır: `kMaxActionsPerWindow`).
- **`Combat_PlanStanding`:** dört kombinasyon: `(true, true) == STAND_STOP_FIRST`; `(true, false) == STAND_PROCEED`; `(false, true) == STAND_PROCEED`; `(false, false) == STAND_PROCEED`.

### 5.3 `BotSession` (durum)

- `BotSession.h`: cast alanlarının yanına `int16 m_castTargetId;   // IOCP thread only: target id sent with CASTING (the cancel packet carries it)`.
- `BotSession.cpp`: başlatıcı listesine `m_castTargetId(-1)`; `ResetForRespawn()` içinde `m_castTargetId = -1`. `OnPacket()`'e **dokunma**.

### 5.4 `ActionExecutor::CancelCast`

`ActionExecutor.h` (`EndCast` bildiriminin yanına) ve `ActionExecutor.cpp` (`EndCast`'ten sonra):

```cpp
// Ends the cast series. Only a series whose CASTING packet is in flight (m_castPhase == CAST_CASTING) needs a packet:
// one WIZ_MAGIC_PROCESS MAGIC_FAIL with sData[3] = -100 through CUser::HandlePacket() after the guard (CLI-03, CLI-11).
// 'cause' ("cmd" / "move") is telemetry text only. Result only from the reply the server published to the caster
// (m_castEcho). NOTHING "idle": no series. NOTHING "dropped": ARMED series, nothing in flight, dropped without a
// packet (or the session cannot send). SENT "cancelled": the echo (MAGIC_FAIL, -100) arrived, series ended.
// FAILED "no_result": no echo, series ended anyway. REFUSED "rate": guard (FAIRNESS_REJECT written), series KEPT.
static CastOutcome CancelCast(BotSession * s, const char * cause, std::chrono::steady_clock::time_point now);
```

Gövde (sıra önemli):

1. `s == nullptr || m_castPhase == CAST_IDLE` → `NOTHING "idle"`.
2. `m_castPhase == CAST_ARMED` → `EndCast(s)`; `NOTHING "dropped"` (paket yok, telemetri yok; F4-03'teki sessiz davranışla aynı).
3. `m_pUser == nullptr`, `!isInGame()` veya `isDead()` → `EndCast(s)`; `NOTHING "dropped"` (paket gönderemez).
4. `nowMs` (`SubmitCast`'taki dönüşümle aynı), `inWindow = s->m_actionWindow.CountInWindow(nowMs)`; `BotCore::CheckCastCancel(true, inWindow)`. `CANCEL_OK` değilse: `decisionId = NextDecisionId(s)`; `EmitFairnessReject(s, user, decisionId, "CastCancel", "CLI-11", "rate", (float)inWindow, (float)BotCore::kMaxActionsPerWindow)`; **seriyi düşürme** (`EndCast` çağırma); `REFUSED "rate"`.
5. `ACTION_SUBMIT` (`TEL_DECISIONS`, `SubmitCast` kalıbı): `"decision_id"`, `"type":"CastCancel"`, `"skill"`, `"target"` (= `m_castTargetId`), `"cause"` (= `cause`), `"since_casting_ms"` (= `now - m_castCastingAt`, ms).
6. Paket: `Packet pkt(WIZ_MAGIC_PROCESS); pkt << uint8(MAGIC_FAIL) << uint32(s->m_castSkillId) << int16(user->GetID()) << int16(s->m_castTargetId) << int16(0) << int16(0) << int16(0) << int16(BotCore::kCastCancelCode) << int16(0) << int16(0);` (21 bayt, `SubmitCast` ile aynı düzen; `sData[3]` = −100). `sData[0..2]` değerinin gerçek istemcide ne olduğu bilinmiyor; sunucu yalnızca yankılar → **`[A]`: 0 gönderilir**.
7. `s->m_castEcho = 0;` → `HandlePacket(pkt)` (gecikme ölç) → `s->m_castPackets++; s->m_actionWindow.Record(nowMs);`.
8. Sonuç: `echo = s->m_castEcho.load()`; geçerli bit ayarlı, `(uint32)(echo & 0xFFFFFFFF) == skillId`, `op == MAGIC_FAIL` ve `code == kCastCancelCode` ise `ok = true`, `reason = "cancelled"`; aksi halde `reason = "no_result"` (ör. echo yok ya da farklı op). `SubmitCast`'taki `(echo >> 48) & 0xF` / `(int16)((echo >> 32) & 0xFFFF)` çözümlemesini aynen kullan.
9. `ACTION_RESULT`: `"decision_id"`, `"type":"CastCancel"`, `"ok"`, `"reason"`, `"op"`, `"code"`, `"latency_us"`.
10. `EndCast(s)`; `ok` ise `SENT "cancelled"`, değilse `FAILED "no_result"`.

`CastOutcome` bildirimindeki sebep listesine `"stopping"`, `"cancelled"`, `"dropped"`, `"idle"` ekle (yalnızca yorum).

**Yasak (AC-LRN-03):** `CancelCast` içinde sunucu nesnesinden başarı çıkarma yok; sonuç yalnızca `m_castEcho`'dan. `GetMagicTable`/`m_MagictableArray` gerekmez.

### 5.5 `TickCast` değişiklikleri (`ActionExecutor.cpp`)

1. **CASTING kabulünde hedef kaydı:** `s->m_castPhase = BotSession::CAST_CASTING; s->m_castCastingAt = now;` satırlarının yanına `s->m_castTargetId = target.id;`.
2. **Otomatik durdurma:** `m == nullptr` bloğundan sonra, hedef görünümü hesabından **önce**:

```cpp
	// CLI-09: a UseStanding skill needs a stop packet and at least one tick before the cast starts. Only the ARMED
	// phase is held; the next Tick() re-evaluates (the guard below still rejects "not_standing" as a safety net).
	if (s->m_castPhase == BotSession::CAST_ARMED
		&& BotCore::PlanStanding(m->sUseStanding == 1, s->m_moveActive) == BotCore::STAND_STOP_FIRST)
	{
		StopMove(s, now);
		out.kind = CastOutcome::SENT;
		out.reason = "stopping";
		return out;
	}
```

`StopMove` her çıkış yolunda `m_moveActive`'i `false` yapar (başarı, handler_noop ve guard reddi; `SubmitMove`), yani döngü oluşmaz. `c.standing = !s->m_moveActive` ve `CheckCastStart` çağrısı **değişmez**.

### 5.6 `BotManager.cpp`

1. **Hareketten önce iptal (`TickSessions()` canlı dalı):** `s->m_deadSeen = false;` satırından sonra, `TickMove` çağrısından önce:

```cpp
	// ADR-0017 Ek F4-24 / CLI-03: moving while a cast waits for EFFECTING cancels it first (cancel packet, then move).
	bool moveHeld = false;
	if (s->m_moveActive && s->m_castPhase == BotSession::CAST_CASTING)
	{
		CastOutcome cancel = ActionExecutor::CancelCast(s, "move", now);
		if (cancel.kind == CastOutcome::REFUSED)
			moveHeld = true;   // rate: the move must not overtake its cancel, retry next tick

		// log: SENT -> "BotManager: bot %s cast cancelled by move after %u packet(s) sent"
		//      FAILED -> "BotManager: bot %s cast cancel failed (%s)"   (cancel.reason)
		//      REFUSED -> "BotManager: bot %s cast cancel deferred (%s)" (cancel.reason)
	}
```

`MoveOutcome outcome = ActionExecutor::TickMove(s, now);` ve onu izleyen `ARRIVED`/`REFUSED`-`FAILED` log blokları `if (!moveHeld) { ... }` içine alınır; `TickUserIn` ve `TickNpcIn` blokları **dışarıda** kalır (davranışları değişmez). Log metinleri yukarıdaki üç satır birebir (doğrulamada aranır).

2. **`CommandCast` `off` dalları** (tek bot `:1451` ve `all` `:1404`): `EndCast(s)` yerine `CastOutcome cancel = ActionExecutor::CancelCast(s, "cmd", now);` (`now` fonksiyonun başında zaten var). Mesajlar:

| `cancel` | Mesaj (tek bot; `all` için aynı satır, bot başına) |
|---|---|
| `SENT` | `BotManager: cmd cast: <ad> cast cancelled after <N> packet(s) sent` (`N = m_castPackets`) |
| `NOTHING "dropped"` | `BotManager: cmd cast: <ad> stopped after <N> packet(s) sent` (eski metin) |
| `NOTHING "idle"` | `BotManager: cmd cast: <ad> not casting` (eski metin) |
| `FAILED` | `BotManager: cmd cast: <ad> cancel failed (<reason>)` |
| `REFUSED` | `BotManager: cmd cast: <ad> cancel refused (<reason>); still casting` |

`all` özet satırı: `BotManager: cmd cast all: %u stopped, %u not casting, %u refused`. `SENT`, `FAILED` ve `dropped` "stopped" sayılır; `idle` "not casting"; `REFUSED` "refused". Başka hiçbir komutun çıktısı değişmez. Kullanım metni (`kUsage`) değişmez.

3. `EndCast` çağrıları `:2995` (ölüm), `:3020` (hedef kaybı) ve `:3163` (despawn) **olduğu gibi** kalır (K9).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.*`, `BotSession.*`, `BotManager.cpp`, `CombatTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı iki yeni test adını (`Combat_CastCancel_Guard`, `Combat_PlanStanding`) içerir ve toplam test sayısı **84**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>`; `grep -n "std::min\|std::max" BotCore/BotCombat.h` yeni satır göstermez.
- [ ] K5: iptal paketi yalnızca `ActionExecutor.cpp`'de oluşuyor ve `HandlePacket` ile işleniyor: `grep -n "kCastCancelCode" GameServer/Bot/*.cpp GameServer/Bot/*.h` yalnızca `ActionExecutor.cpp` satırlarını gösterir; `grep -n "MAGIC_FAIL" GameServer/Bot/*.cpp GameServer/Bot/*.h` `BotManager.cpp`, `BotSession.*` ve `ScenarioRunner`/`ScriptRunner` dosyalarında **yeni** satır göstermez.
- [ ] K6: `CancelCast`'te guard atlanmıyor: `HandlePacket` çağrısından önce `CheckCastCancel` çağrısı ve `CANCEL_OK` dışında `HandlePacket`'sız erken dönüş vardır; iptal için `HandlePacket` çağrısı **tek yerde**; guard `rate` reddinde `EndCast` çağrılmaz (kod okumasıyla).
- [ ] K7: `OnPacket()` değişmedi: `git diff gece/2026-10-02...bot/F4-24 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesinde bilinçli değiştirilen satır(lar)ı gösterir (`OnPacket` bloklarından silinen satır yok); `ResetForRespawn()` yalnızca `m_castTargetId = -1` ekler.
- [ ] K8: otomatik durdurma: `grep -n "PlanStanding" GameServer/Bot/ActionExecutor.cpp` ≥ 1 ve çağrı yalnızca `CAST_ARMED` koşuluyla; `grep -n "c.standing" GameServer/Bot/ActionExecutor.cpp` ve `CheckCastStart(c)` çağrısı değişmedi (`git diff` bu satırları silmiyor); `RejectCast` içindeki `CLI-09`/`not_standing` eşlemesi duruyor.
- [ ] K9: `grep -n 'CancelCast(s, "move"' GameServer/Bot/BotManager.cpp` 1 satır; `grep -n 'CancelCast(s, "cmd"' GameServer/Bot/BotManager.cpp` 2 satır; `grep -c "EndCast" GameServer/Bot/BotManager.cpp` **3** (ölüm, hedef kaybı, despawn); `TickMove` çağrısı yalnızca `moveHeld == false` iken (kod okumasıyla); `TickUserIn`/`TickNpcIn` blokları `if (!moveHeld)` dışında.
- [ ] K10: `ENABLED=0` davranışı değişmez: `git diff gece/2026-10-02...bot/F4-24 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca bu planın açıkladığı satırları gösterir (iki `off` dalındaki `EndCast`/mesaj satırları, `all` özet biçimi, `TickMove` çağrı satırı ve sarmalanan log blokları); `Startup()`/`Tick()`/`BuildStatusLines()`/`BeginDespawn()` ve ini okuma değişmedi.
- [ ] K11: `git diff --stat gece/2026-10-02...bot/F4-24` yalnızca §4'teki 7 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K12: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K13: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K14: gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 82 testin tamamı hâlâ geçiyor; `EmitFairnessReject` çağrıları `"Move"`/`"Attack"`/`"Cast"`/`"Potion"` geçiyor, yeni `"CastCancel"` eklendi.
- [ ] K15: `python3 tools/check-perception-contract.py` `RESULT: PASS` ve sayılar değişmez (`R1 0/0`, `R2 0/28`, `R3 0/18`, `R4 0/0`, `R5 0/0`); yeni kod başka botun `CUser`'ına (`t->m_pUser`) erişmez, yalnızca `s->m_pUser`.
- [ ] K16 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S6 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-24
git diff gece/2026-10-02...bot/F4-24 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-24 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-24 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "std::min\|std::max" BotCore/BotCombat.h
grep -n "kCastCancelCode\|MAGIC_FAIL" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "CheckCastCancel\|HandlePacket\|PlanStanding\|c.standing" GameServer/Bot/ActionExecutor.cpp
grep -n 'CancelCast\|moveHeld' GameServer/Bot/BotManager.cpp
grep -c "EndCast" GameServer/Bot/BotManager.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-24
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; **bir dosyadaki satırlar aynı `Tick()`'te sırayla çalışır**). İptal zamanlaması için **geçici betik** gerekir (`./Scripts/f424_*.txt`, ofsetli adımlar, F4-20; doğrulama sonunda silinir, commit edilmez). Botlar: Karus mage `BotMF_K` (skill `110518`: `CastTime = 10` ⇒ CASTING→EFFECTING ≈ 1080 ms, `Range = 56`, `Msp = 60`), El Morad warrior `BotWP_E`; zone 71, botlar aynı başlangıç noktasındadır. Gözlem `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'dan.

1. **S1 Komutla iptal:** `spawn BotMF_K,BotWP_E`; betik: `0 cast BotMF_K 110518 BotWP_E 1` / `500 cast BotMF_K off`. Beklenen: JSONL `CastStart` `ACTION_SUBMIT`→`ACTION_RESULT` (`ok:true`, `reason:"casting"`); ardından `ACTION_SUBMIT` `type:"CastCancel"`, `cause:"cmd"`, `since_casting_ms` ≈ 400–700, `target` = `BotWP_E` kimliği → `ACTION_RESULT` (`ok:true`, `reason:"cancelled"`, `op:4`, `code:-100`); **`CastEffect` olayı yok**; log `cast cancelled after 2 packet(s) sent`; `list` MP, cast öncesiyle aynı (MP EFFECTING'te düşülür).
2. **S2 Hareketle iptal:** betik: `0 cast BotMF_K 110518 BotWP_E 1` / `400 move BotMF_K <≈5 m yan konum>`. Beklenen: `CastCancel` `ACTION_SUBMIT` (`cause:"move"`) JSONL'de **ilk `Move` `ACTION_SUBMIT`'ten önce** (aynı `t` ya da küçük), `cancelled`; log `cast cancelled by move after 2 packet(s) sent`; bot hedefe yürür ve varır; `CastEffect` yok. Sonra `cast BotMF_K 110518 BotWP_E 1` yeniden atılır ve etkilenir (`effected`): iptal sonrası seri/zamanlayıcı bozulmadı.
3. **S3 ARMED düşürme:** tek `BotCommands.txt`: `cast BotMF_K 110518 BotWP_E 1` ve `cast BotMF_K off` aynı tick'te → `off` ARMED seriyi görür: `stopped after 0 packet(s) sent`, JSONL'de `CastStart`/`CastCancel` **yok**. Canlı ama cast etmeyen bot için `cast BotMF_K off` → `not casting`; `cast all off` → özet `0 stopped, N not casting, 0 refused`.
4. **S4 `rate` reddi (sınanabilirse):** CLI-11 penceresini (6 aksiyon/sn) doldurup CASTING sırasında iptal denenir (ör. aynı saniyede art arda `pot`/`attack` serileri); beklenen `FAIRNESS_REJECT` (`type:"CastCancel"`, `rule:"CLI-11"`, `reason:"rate"`, `value` ≥ 6, `limit` 6), seri **sürer** ve bir sonraki tick'te (pencere açılınca) iptal gider. Üretilemezse raporda "sınanmadı (birim testle kapsandı)" yazılır.
5. **S5 `UseStanding` otomatik durdurma (geçici `MAGIC` düzenlemesi, Claude):** botların skill'i hiçbiri `UseStanding = 1` olmadığından (§2 veri notu) **önce** `MAGIC`'ten tek bir bot skill'inin (ör. `BotWP_E`/`BotWP_K` warrior Type1 skill'i, F4-03'te `cast ... 106560` ile sınanmış) `UseStanding` değeri not alınır, yerelde `UPDATE MAGIC SET UseStanding = 1 WHERE MagicNum = <skill>` yapılır (yalnızca bu satır, oyun verisi tablosu; **kişisel veri tablosuna dokunulmaz**), sunucu yeniden başlatılır (MAGIC açılışta yüklenir). `move BotWP_K <≥ 20 m uzak>` ardından yürürken (≈ 1 sn sonra) `cast BotWP_K <skill> BotWP_E 1`: beklenen `Move` `ACTION_SUBMIT` `speed:0` (durma) → ardından (≥ 1 tick sonra, ≥ 100 ms) `CastEffect` `effected`; **`FAIRNESS_REJECT not_standing` yok**; `srv_fail` yok. Kontrol: kodun eski halinde bu `not_standing` verirdi (birim test `CAST_REJECT_NOT_STANDING` korunur). **Sonunda `UseStanding` eski değerine geri yazılır, sunucu yeniden başlatılır, doğrulanır** (`SELECT`); bu bölüm yapılamazsa raporda "sınanmadı" yazılır ve K8 yalnızca kod okuması + birim testle desteklenmiş sayılır.
6. **S6 Gerilemesiz:** `cast BotMF_K 110518 BotWP_E 3` (3 döngü, iptal yok) önceki gibi `effected` ×3; `cast BotMF_K 110518 self` → `bad_target`; 30 m `move`/`stop` (F4-01); `attack BotWP_K BotWP_E 3` (F4-02); `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama iptal çalışır (`cast cancelled ...` log satırı); `ENABLED=0` → komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde (≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. İnsan istemcisi gerekmez (gerçek istemcide iptalin görünümü `T-ARCH-08` ek maddesi). Temizlik: ini yedekten geri, `MAGIC` geri alındı, bot satırlarının durumu önceki gibi.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve cast/yürüyüş sırasında çalışır; cast etmeyen bot için `CancelCast` çağrılmaz (`TickSessions()` koşulu `m_moveActive && CAST_CASTING`).
- **Thread kuralı (ADR-0005):** `CancelCast` ve otomatik durdurma yalnızca IOCP thread'inde (`Tick()` içinden; komutlar `EnqueueCommand` kuyruğundan) çalışır. `OnPacket()` her thread'den çağrılabilir; bu plan onu **değiştirmez** (`MAGIC_FAIL` yankısı F4-03'ten beri `m_castEcho`'ya yazılıyor). `ActionExecutor` log yazmaz; log satırları `BotManager`'dadır.
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): iptal "başarılı" yalnızca sunucunun çağırana yankıladığı `MAGIC_FAIL` / −100 paketinden (aynı skill kimliği) çıkarılır; sunucu nesnesinden (HP/MP/konum) başarı çıkarılmaz.
- **Bilinen sınırlar `[A]`:** (a) iptal paketindeki `sData[0..2]` ve `target`'ın gerçek istemcide ne olduğu bilinmiyor (`docs/03` yalnızca opcode 4 ve `sData[3] = -100`'ü verir); bot CASTING'teki hedef kimliğini ve sıfır konumu gönderir, sunucu bunları yalnızca yankılar; (b) iptal ile hareket arasındaki gecikme istemcide 5–8 ms, botta aynı tick içinde ≈ 0 ms'dir (sıra korunur, aralık ölçülü değil); (c) hareketle iptal yalnızca CASTING aşamasında; EFFECTING sonrası yürümek cast'i etkilemez; (d) `UseStanding` otomatik durdurması bir tick'i (≥ `TICK_MS`) bekletir, gerçek istemcinin durma→cast gecikmesi ölçülmedi.
- Telemetri hacmi küçüktür (iptal başına ≤ 2 olay); `droppable = false`, `IsEnabled` denetimi önceki aksiyonlarla aynı.
- `list`/`snap` ve `Telemetry.*` değişmez; yalnızca §5.6'daki mesajlar değişir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-24` — `6ffff94 [F4-24] Cast iptali: MAGIC_FAIL -100, hareketle iptal, UseStanding otomatik durdurma`; plan dosyası bu raporla ayrıca commit edilir.
- Değişen dosyalar ve neden:
  - `BotCore/BotCombat.h`: `kCastCancelCode`, `CastCancelVerdict`/`CheckCastCancel`, `StandingPlan`/`PlanStanding` (saf mantık, `CheckCastEffect`'ten sonra, pot diliminden önce).
  - `Tests/BotCoreTests/CombatTests.cpp`: `Combat_CastCancel_Guard` ve `Combat_PlanStanding` (82 → 84).
  - `GameServer/Bot/ActionExecutor.h`: `CancelCast` bildirimi + `CastOutcome` sebep yorumu.
  - `GameServer/Bot/ActionExecutor.cpp`: `CancelCast` gövdesi (`EndCast`'ten sonra), `TickCast`'te `m_castTargetId` kaydı ve `UseStanding` otomatik durdurma.
  - `GameServer/Bot/BotSession.h`: `int16 m_castTargetId`.
  - `GameServer/Bot/BotSession.cpp`: başlatıcı listesi + `ResetForRespawn()` (yalnızca ekleme; `OnPacket()` değişmedi).
  - `GameServer/Bot/BotManager.cpp`: `TickSessions()` canlı dalında `TickMove`'dan önce hareketle iptal (`moveHeld`), `CommandCast` `off` dalları `CancelCast` ile.
- Derleme sonucu (`tools/build.sh Release`; değişen yedi dosya `touch` ile yeniden derlendi, ilgili dosyalarda `warning C`/`error C` yok):
  ```
  BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  `./tools/build.sh Debug` da hatasız bitti (kalan uyarılar yalnızca eski `GameServerDlg.cpp:1143/1802`).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔: Release rc=0; `touch` sonrası yedi dosyada `warning C`/`error C` boş.
  - K2 ✔: Debug rc=0.
  - K3 ✔: `84 tests, 0 failed` (Release + Debug); çıktıda `Combat_CastCancel_Guard` ve `Combat_PlanStanding` var.
  - K4 ✔: yasak include/token eşleşmesi yok; `std::min`/`std::max` yeni satır göstermiyor.
  - K5 ✔: `kCastCancelCode` yalnızca `ActionExecutor.cpp`; `MAGIC_FAIL` yeni satır yalnızca `ActionExecutor.cpp`.
  - K6 ✔ (kod okuması): `CheckCastCancel` `HandlePacket`'tan önce; `CANCEL_OK` değilse `HandlePacket`'sız `REFUSED "rate"`; iptal `HandlePacket` tek yerde; `rate` reddinde `EndCast` yok.
  - K7 ✔: BotSession diff `-` satırları yalnızca başlatıcı satırı; `ResetForRespawn` yalnızca `m_castTargetId = -1` ekler.
  - K8 ✔: `PlanStanding` yalnızca `CAST_ARMED` koşuluyla; `c.standing` ve `CheckCastStart(c)` değişmedi; `RejectCast` `CLI-09`/`not_standing` duruyor.
  - K9 ✔: `CancelCast(s, "move"` 1 satır, `CancelCast(s, "cmd"` 2 satır, `EndCast` sayısı 3; `TickMove` yalnızca `!moveHeld`; `TickUserIn`/`TickNpcIn` dışarıda.
  - K10 ✔: BotManager `-` satırları yalnızca plandaki iki `off` dalı, özet biçimi ve `TickMove` sarmalaması.
  - K11 ✔: `gece/2026-10-02...bot/F4-24` yalnızca §4'teki 7 dosya; vcxproj/sln farkı yok.
  - K12 ✔: `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF; `git diff --check` boş.
  - K13 ✔: `ActionExecutor.*` içinde `printf`/`Sleep`/`lock_guard`/`mutex`/`CreateThread`/`rand(` yok.
  - K14 ✔: `CheckMoveStep` 2, `CheckAttack`/`CheckCastStart`/`CheckCastEffect`/`CheckPotion` ≥ 1; önceki 82 testle birlikte 84 geçiyor; `EmitFairnessReject` yeni `"CastCancel"` çağrısı `ActionExecutor.cpp`'de.
  - K15 ✔: `tools/check-perception-contract.py` → `RESULT: PASS`, `R1 0/0`, `R2 0/28`, `R3 0/18`, `R4 0/0`, `R5 0/0`; yeni kod yalnızca `s->m_pUser` okur.
  - K16: Claude'un `/plan-dogrula` çalışma zamanı senaryoları (S1–S6) — uygulayıcı yapmaz.
- Plandan sapmalar ve gerekçeleri: Yok. (Yalnızca `ActionExecutor.h`'deki `CastOutcome` sebep yorumuna "stopping"/"cancelled"/"dropped"/"idle" eklendiği satırda, düzenleme sırasında oluşan çift girinti fark edilip derlemeden önce aslına döndürüldü; `git diff` yalnızca eklenen yorum satırlarını gösterir.)
- Açık sorular: Yok.


---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-24` @ `<sha>`
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
