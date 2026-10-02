# F4-09: `ActionExecutor` party ret ve ayrılma dilimi — `PartyDecline` / `PartyLeave` ve `BotFairnessGuard` CLI-16 kuralları

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-09` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-08 (`PartyOutcome`, `m_partyInviteEcho`/`m_partyInviteAtMs` kayıtları, `OnPacket()` `WIZ_PARTY` bloğu, `RejectParty*` kalıbı) — `KAPANDI` (merge `851afdc`); F4-07, F4-01 — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-16 (`docs/03` §14, bu planla birlikte eklendi), CLI-11, MEC-PTY-03, KI-014, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı |
| Tahmini büyüklük | M (8 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun onuncu ve on birinci gerçek aksiyonları: bir bot **gerçek `WIZ_PARTY` paketiyle** aldığı party davetini reddeder (`PartyDecline`: `PARTY_PERMIT 0`) ve bulunduğu party'den ayrılır (`PartyLeave`: `PARTY_REMOVE` + kendi oturum kimliği; botun lideri olduğu party'de bu, sunucuda party'nin dağıtılması demektir, MEC-PTY-03). İkisi de `CUser::HandlePacket()` üzerinden gider. `PartyDecline`, KI-014'ün (cevapsız davet hedefi sunucuda "party'de" tutar) bot tarafındaki çözümüdür: reddedilen davet hedefi serbest bırakır ve gereksiz kurulmuş tek kişilik party'yi siler. Sunucu ret/ayrılma hızı ve "davetten hemen sonra / party'ye girer girmez" gecikmesi konusunda hiçbir denetim yapmaz. Bu yüzden paketler sunucuya gitmeden önce `BotFairnessGuard` kurallarından geçer: ret, davet alındıktan ≥ 1,0 sn sonra (`decline_wait`); ayrılma, party'ye girişten ≥ 1,0 sn sonra (`leave_wait`); ikisi de CLI-11 aksiyon hızı. Karar katmanı yoktur: aksiyonları `/bot pdecline <bot>` ve `/bot pleave <bot>` komutları tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün dokuzuncu dilimidir (ADR-0017 Ek F4-09; hareket → saldırı → cast → pot → duruş → hedef HP → `Regene` → `Party` kurulumu → **`Party` ret/ayrılma** → `PartyPromote`/`PartyKick` → `Chat` → `Perception`).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-09)" (bu planla birlikte yazıldı): paket düzenleri, guard kuralları, sonuç eşlemesi, kapsam.
- `docs/03` §10 **MEC-PTY-03** (lider ayrılırsa/tek kişi kalırsa party silinir), §14 **CLI-16** (bu planla birlikte eklendi), **CLI-15**, **CLI-11**; `docs/KNOWN_ISSUES.md` **KI-014**.
- `plans/F4-08-aksiyon-yurutucu-party.md` §5.2–§5.5: bu planın kalıpladığı iskelet (saf mantık `BotCore/BotCombat.h` + `ActionExecutor` + `BotSession` kayıtları + `BotManager` komutu). **Yazılı planı değil, birleşmiş kodu esas al** (`ActionExecutor.cpp` `RejectPartyAccept` ve `RequestPartyAccept`, `BotManager.cpp` `CommandPartyAccept`).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `851afdc` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:379-381` — `HandlePacket` `case WIZ_PARTY`: `PartyProcess(pkt)`.
  - `GameServer/PartyHandler.cpp:6-50` — `PartyProcess`: `u8 alt-opcode`. `PARTY_PERMIT` (2): `pkt.read<uint8>()` 1 → `PartyInsert()`, **0 → `PartyCancel()`** (`:28-33`). `PARTY_REMOVE` (4): `PartyRemove(pkt.read<uint16>())` (`:41-43`). `PARTY_DELETE` (5, `:46-48`) bu planda **kullanılmaz** (aşağıya bak).
  - `GameServer/PartyHandler.cpp:52-83` — `PartyCancel()` (ret): `!isInParty()` → döner; `GetPartyPtr(GetPartyID())` yoksa veya party liderinin oturumu yoksa **hiçbir şey yapmadan** döner (reddeden hâlâ `m_bInParty` kalır, KI-014'ün uç durumu); aksi halde reddedenin `m_bInParty = false`, `m_sPartyIndex = -1`; party'de **yalnızca lider** varsa (`count == 1`) liderin party'si silinir (`pUser->PartyDelete()`, `:68-78`); sonunda **lidere** `WIZ_PARTY`: `u8 PARTY_INSERT, i16 -1` (3 bayt). **Reddedene hiçbir paket gelmez.**
  - `GameServer/PartyHandler.cpp:337-407` — `PartyRemove(memberid)`: `!isInParty()` → döner. `memberid != GetSocketID()` (başkasını atma) → yalnızca lider atabilir; `memberid == GetSocketID()` ve **bu oyuncu lider** → `PartyDelete()` (party dağılır, `:359-366`). Üye kendi ayrılıyorsa: kalan üye sayısı 1 (yalnızca lider) ise **liderin** `PartyDelete()`'i çağrılır ve `return` edilir (`PARTY_REMOVE` yayını yapılmaz, `:383-391`); aksi halde `WIZ_PARTY`: `u8 PARTY_REMOVE, u16 memberid`, `Send_PartyMember(m_sPartyIndex, ...)` ile **ayrılan dahil tüm üyelere** (`:393-395`; üyenin `m_bInParty`'si yayından **sonra** temizlenir, `:397-400`).
  - `GameServer/PartyHandler.cpp:409-444` — `PartyDelete()`: tüm üyelerin `m_bInParty = false`, `m_sPartyIndex = -1` yapar; `WIZ_PARTY`: `u8 PARTY_DELETE` (1 bayt, `Send_PartyMember` ile **tüm üyelere**, ayrılan dahil, `:433-434`); `m_bPartyLeader = false; StateChangeServerDirect(6, 0)` (lider 'P' sembolü kalkar, bu çağrının `WIZ_STATE_CHANGE` yayını bu planda okunmaz).
  - `GameServer/GameServerDlg.cpp:1080-1093` — `Send_PartyMember(party, pkt)`: party üyelerinin `Send()`'i (bot alıcısına gider).
  - `GameServer/User.cpp:205-217`, `GameServer/CharacterMovementHandler.cpp:410-423` — sunucu kendisi, oyuncu kopunca/zone değiştirince `isPartyLeader()` ise `PartyPromote(uid[1])`, sonra `PartyRemove(GetSocketID())` çağırır: yani gerçek bir ayrılma `PARTY_REMOVE` + kendi kimliğidir. Bot `PARTY_DELETE` **göndermez** (gerçek istemcinin liderken hangi alt-opcode'u yolladığı ölçülmedi `[A]`, `T-PARTY-02`; `PARTY_REMOVE` kendi kimliğiyle her iki durumu da sunucuda doğru işler).
  - `GameServer/User.h:313` `isInParty()`, `:319` `isPartyLeader()`; `GameServer/Unit.h:54` `GetID()`; `shared/packets.h:238-250` `PARTY_*` alt-opcode sabitleri.
  - `GameServer/Bot/BotSession.cpp:99-125` — `OnPacket()` `WIZ_PARTY` bloğu (F4-08): `PARTY_PERMIT`, `PARTY_INSERT` (3 bayt = ret kodu; uzun = üye katıldı). `:174-178` `ResetForRespawn()` party kayıtları. `GameServer/Bot/BotSession.h:109-125` party üyeleri.
  - `GameServer/Bot/ActionExecutor.cpp:1922-2023` `RequestPartyAccept` (ret ve ayrılma bunun kalıbıdır), `:1881-1888` `created` dalı, `:2011-2015` `joined` dalı; `:36` `NextDecisionId`, `:53` `EmitFairnessReject`. `GameServer/Bot/BotManager.cpp:638-642` fiil dağıtımı, `:646` `unknown command` listesi, `:1933-1978` `CommandPartyAccept`. `BotCore/BotCombat.h:436-501` party dilimi (F4-08).

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** `kPartyDeclineMinMs`, `kPartyLeaveMinMs`, `PartyDeclineCheck`, `PartyDeclineVerdict`, `CheckPartyDecline`, `PartyLeaveCheck`, `PartyLeaveVerdict`, `CheckPartyLeave`. Birim testleri (`CombatTests.cpp`).
2. **`ActionExecutor::RequestPartyDecline(s, now)`** ve **`RequestPartyLeave(s, now)`** (tek seferlik aksiyonlar): ön koşullar → guard → `WIZ_PARTY` paketi → `HandlePacket` → yayınlanan cevaplardan sonuç eşleme (ayrılma) → telemetri.
3. **Oturum durumu (`BotSession`):** botun party'ye giriş zamanı (`m_partyEnteredHasAt`/`m_partyEnteredAt`, IOCP thread; F4-08'deki iki başarılı yol ayarlar) ve `OnPacket()`'in doldurduğu bir kayıt (`m_partyLeaveEcho`: `PARTY_REMOVE` veya `PARTY_DELETE` alındı).
4. **Komutlar (`BotManager`):** `pdecline <bot>` ve `pleave <bot>` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten).
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT` / `ACTION_RESULT` (`"type":"PartyDecline"` ve `"PartyLeave"`), `FAIRNESS_REJECT` (aynı tipler).

**Kapsam dışı (yapılmayacak)**

- **Kim kimi ne zaman reddeder / ne zaman ayrılır** (karar katmanı; `docs/09` §2, F7): yok. Komutlar guard dışında hiçbir koşula bakmaz.
- **Başkasını atma (`PARTY_REMOVE` başka kimlikle), lider devri (`PARTY_PROMOTE`), `PARTY_DELETE` ile dağıtma, party chat (`WIZ_CHAT`), party arama panosu (`WIZ_PARTY_BBS`):** yok; sonraki dilimler (F4-10: `PartyPromote` + `PartyKick`, sonra `Chat`).
- **Reddin sunucuda bir cevabının olmaması:** reddeden bota hiçbir paket gelmez; bu planda "ret gerçekten işlendi" kanıtı **aranmaz** (`SENT "declined"` = ret paketi gitti, cevap beklenmez `[A]`). Etkisini Claude çalışma zamanında dolaylı gözler (aynı hedef yeniden davet edilebilir, §7 senaryo 1).
- **Başka bir botun ya da insanın reddine/ayrılmasına tepki** (lider bota gelen `PARTY_INSERT -1`, üyeler bota gelen `PARTY_REMOVE`): `m_partyErrorEcho` ve `m_partyLeaveEcho` yalnızca kendi isteğinin sonucu için okunur; bot durumu/üye listesi tutulmaz.
- **Zaman aşımı veya sunucu tarafında bozulan davet kaydını temizleme:** `m_partyInviteEcho` yalnızca kabul/ret ile silinir. Lider daveti verip party'yi dağıtırsa kayıt bayat kalabilir; temizlik `Perception`/party yönetimi diliminin işi.
- `ChatHandler.cpp` `+bot` yardım metni (KI-012; Claude doğrulamada günceller). Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya yok; **`GameServer` projesine dosya eklenmez**. `Telemetry.*`, `ScenarioRunner.*` değişmez; `list` satırı değişmez.
- Dokümanları (`docs/03`, `docs/13`, `docs/16`, `docs/15`, `docs/KNOWN_ISSUES.md`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | Yalnızca ekleme (§5.2); mevcut içerik ve `#include`'lar değişmez |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | Yalnızca ekleme: üç yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `PartyOutcome` yorumu genişler; iki yeni statik fonksiyon |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | Dosya sonuna ekleme (§5.4) ve F4-08'in iki başarı dalına **birer iki satır ekleme** (giriş zamanı); başka kod değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca party üyeleri (§5.3) |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi, `ResetForRespawn()` ve `OnPacket()` `WIZ_PARTY` bloğuna **`else if` ekleme** (mevcut dallar değişmez) |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandPartyDecline`, `CommandPartyLeave` bildirimleri |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, iki komut, `unknown command` listesi |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (Yeni dosya açılmaz: guard `BotCombat.h`'ye eklenir, böylece `BotCore*.vcxproj` değişmez.)

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-09 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotCombat.h` (saf mantık, mevcut `namespace BotCore` içine ekleme)

Mevcut kodun biçimini (tab, Allman, `inline`, İngilizce kısa yorum, ölçülmemiş değer `[A]`) koru. Yeni `#include` gerekmez. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez** (yorumlarda da `shared/` dizgisi **yazma**: K4 grep'i takılır). `CheckPartyAccept`'ten **sonra**, `namespace`'in kapanışından önce ekle. **`std::min`/`std::max` kullanma**:

```cpp
	// --- party decline / leave slice (ADR-0017 Ek F4-09) ---

	constexpr uint32_t kPartyDeclineMinMs = 1000;   // docs/03 CLI-16: reading the invitation popup and clicking decline takes a human at least this long [A] (unmeasured)
	constexpr uint32_t kPartyLeaveMinMs = 1000;     // docs/03 CLI-16: a human needs at least this long between entering a party and leaving it [A] (unmeasured)

	struct PartyDeclineCheck
	{
		uint32_t sinceInviteMs;   // since the invitation reached the bot
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum PartyDeclineVerdict
	{
		PARTYDECLINE_OK = 0,
		PARTYDECLINE_REJECT_WAIT = 1,   // CLI-16 (declined before kPartyDeclineMinMs after the invitation arrived)
		PARTYDECLINE_REJECT_RATE = 2    // CLI-11
	};

	// Guard rule for declining an invitation. The caller has already checked that an invitation is pending. Order: wait, rate.
	inline PartyDeclineVerdict CheckPartyDecline(const PartyDeclineCheck & c);

	struct PartyLeaveCheck
	{
		bool hasEntered;          // the bot created or joined a party in this spawn (the entry time is known)
		uint32_t sinceEnteredMs;  // since that entry
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum PartyLeaveVerdict
	{
		PARTYLEAVE_OK = 0,
		PARTYLEAVE_REJECT_WAIT = 1,   // CLI-16 (leaving before kPartyLeaveMinMs after entering the party)
		PARTYLEAVE_REJECT_RATE = 2    // CLI-11
	};

	// Guard rule for leaving a party. The caller has already checked that the bot is in a party with no invitation pending.
	// Order: wait (only when the entry time is known), rate.
	inline PartyLeaveVerdict CheckPartyLeave(const PartyLeaveCheck & c);
```

Gövdeler (`CheckPartyAccept` kalıbı, aynı yerde `inline`):

- `CheckPartyDecline`: `c.sinceInviteMs < kPartyDeclineMinMs` → `PARTYDECLINE_REJECT_WAIT`; `c.actionsInWindow >= kMaxActionsPerWindow` → `PARTYDECLINE_REJECT_RATE`; aksi halde `PARTYDECLINE_OK`.
- `CheckPartyLeave`: `c.hasEntered && c.sinceEnteredMs < kPartyLeaveMinMs` → `PARTYLEAVE_REJECT_WAIT`; `c.actionsInWindow >= kMaxActionsPerWindow` → `PARTYLEAVE_REJECT_RATE`; aksi halde `PARTYLEAVE_OK`.

**`Tests/BotCoreTests/CombatTests.cpp` (ekleme, mevcut makro stili `CHECK_EQ((int)..., (int)...)`):** üç yeni `TEST_CASE` (dosyanın sonuna, `Combat_PartyAcceptCheck`'ten sonra):

- `Combat_PartyDeclineCheck`: `sinceInviteMs = 999, actionsInWindow = 0` → `PARTYDECLINE_REJECT_WAIT`; `1000` → `PARTYDECLINE_OK`; `0` → `..._WAIT`; hepsi ihlal (`sinceInviteMs = 0, actionsInWindow = 6`) → `..._WAIT` (sıra); yalnızca `sinceInviteMs = 1000, actionsInWindow = 6` → `..._RATE`; `actionsInWindow = 5` → `OK`; `kPartyDeclineMinMs == 1000`.
- `Combat_PartyLeaveCheck_Order`: `hasEntered = true, sinceEnteredMs = 0, actionsInWindow = 6` → `PARTYLEAVE_REJECT_WAIT`; `sinceEnteredMs = 1000` → `..._RATE`; `actionsInWindow = 5` → `PARTYLEAVE_OK`; `hasEntered = false, sinceEnteredMs = 0, actionsInWindow = 0` → `PARTYLEAVE_OK` (giriş zamanı bilinmiyorsa bekleme yok); `hasEntered = false, sinceEnteredMs = 0, actionsInWindow = 6` → `..._RATE`.
- `Combat_PartyLeaveCheck_Boundaries`: `hasEntered = true, sinceEnteredMs = 999, actionsInWindow = 0` → `..._WAIT`; `1000` → `OK`; `kPartyLeaveMinMs == 1000`; `actionsInWindow = 5` → `OK`, `6` → `..._RATE` (`sinceEnteredMs = 1000`).

Beklenen toplam test sayısı: 36 + 3 = **39**.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'de `m_partyInviteLast`'ın altına (aynı yorum/hizalama biçimi):

```cpp
	bool m_partyEnteredHasAt;                              // IOCP thread only: m_partyEnteredAt is valid (the bot created or joined a party in this spawn)
	std::chrono::steady_clock::time_point m_partyEnteredAt;    // IOCP thread only: when the bot last created or joined a party
```

ve atomik üyelerin yanına (`m_partyJoinEcho`'nun altına):

```cpp
	std::atomic<uint64> m_partyLeaveEcho;                  // written by OnPacket(): valid bit | kind << 16 | sid of the last PARTY_REMOVE (kind 1, sid = the removed member) or PARTY_DELETE (kind 2, sid 0)
```

`BotSession.cpp`: başlatıcı listesinde `m_partyInviteHasLast(false)`'tan sonra `m_partyEnteredHasAt(false)` (satır `:17`) ve `m_partyJoinEcho(0)`'dan sonra `m_partyLeaveEcho(0)` (satır `:21`) ekle; sıra üye bildirim sırasıyla aynı olmalı (derleyici sıra uyarısı vermemeli). `ResetForRespawn()` içine `m_partyEnteredHasAt = false; m_partyLeaveEcho = 0;` ekle (`:174-178` bloğuna).

`OnPacket()` `WIZ_PARTY` bloğunun **sonuna**, son `else if (sub == PARTY_INSERT && pkt.size() >= 4)` dalından sonra, **yalnızca ekleme** (mevcut dallar bayt bayt aynı kalır):

```cpp
		else if (sub == PARTY_REMOVE && pkt.size() >= 3)
		{
			uint16 sid = pkt.read<uint16>(1);
			m_partyLeaveEcho = (1ull << 63) | (1ull << 16) | uint64(sid);
		}
		else if (sub == PARTY_DELETE)
		{
			m_partyLeaveEcho = (1ull << 63) | (2ull << 16);
		}
```

Bloğun üstündeki yoruma da bir cümle ekle: "PARTY_REMOVE (4) = u16 sid of the member who left; PARTY_DELETE (5) = the party was disbanded (1 byte). ActionExecutor::RequestPartyLeave clears m_partyLeaveEcho before its request and reads it afterwards, on the same thread." (Yorumda `PartyRemove`/`PartyDelete` sunucu fonksiyon adlarını **yazma**: K5 grep'i takılır; `PARTY_REMOVE`/`PARTY_DELETE` sabitleri serbest.)

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

**Başlık (`ActionExecutor.h`):** `PartyOutcome::reason` yorumuna yeni değerleri ekle (SENT: `"declined"` (PartyDecline), `"left"` / `"disbanded"` (PartyLeave); REFUSED: `"invite_pending"`, `"not_in_party"`, guard: `"decline_wait"`, `"leave_wait"`; `peerId`: PartyDecline → davet edenin kimliği, PartyLeave → -1). `class ActionExecutor` içine (`RequestPartyAccept` bildiriminin altına, aynı yorum kalıbıyla):

```cpp
	// One-shot decline of the pending invitation (PARTY_PERMIT 0 through CUser::HandlePacket()) after the guard (CLI-16:
	// >= 1 s after the invitation arrived; CLI-11). The pending invitation is the one OnPacket() recorded
	// (m_partyInviteEcho); none -> REFUSED "no_invite" without an event. The server sends the decliner no reply (it
	// answers the leader), so the result is SENT "declined" once the packet went out ([A]); the invitation record is consumed.
	static PartyOutcome RequestPartyDecline(BotSession * s, std::chrono::steady_clock::time_point now);

	// One-shot party leave (PARTY_REMOVE with the bot's own id through CUser::HandlePacket()) after the guard (CLI-16:
	// >= 1 s after the bot entered the party, when known; CLI-11). Preconditions without an event: REFUSED "invite_pending"
	// (an invitation must be accepted or declined first), "not_in_party". Result only from published replies: the bot's
	// own PARTY_REMOVE (sid == its id) -> SENT "left"; PARTY_DELETE -> SENT "disbanded" (it led the party, or only the
	// leader remained); neither -> FAILED "no_result".
	static PartyOutcome RequestPartyLeave(BotSession * s, std::chrono::steady_clock::time_point now);
```

**`ActionExecutor.cpp`:**

1. **F4-08 koduna iki ekleme (başka satır değişmez):** `RequestPartyInvite`'ta `if (created)` dalında (`reason = "created";` satırından sonra) `s->m_partyEnteredHasAt = true; s->m_partyEnteredAt = now;` ekle. `RequestPartyAccept`'ta `if (joined)` dalında (`out.reason = "joined";` satırından sonra) aynı iki satırı ekle.
2. **Dosya sonuna** (`RequestPartyAccept`'tan sonra; bölüm başlığı `// --- party decline / leave slice (ADR-0017 Ek F4-09) ---`):
   - **`static PartyOutcome RejectPartyDecline(BotSession * s, CUser * user, BotCore::PartyDeclineVerdict verdict, const BotCore::PartyDeclineCheck & c, int peerId)`** (`RejectPartyAccept` kalıbı, tip `"PartyDecline"`): `WAIT` → `rule "CLI-16", reason "decline_wait", value = sinceInviteMs, limit = kPartyDeclineMinMs`; `RATE` → `rule "CLI-11", reason "rate", value = actionsInWindow, limit = kMaxActionsPerWindow`. `decisionId = NextDecisionId(s)`; `EmitFairnessReject(...)`; `REFUSED` + reason + `peerId`. Sunucuya paket **gitmez**.
   - **`static PartyOutcome RejectPartyLeave(BotSession * s, CUser * user, BotCore::PartyLeaveVerdict verdict, const BotCore::PartyLeaveCheck & c)`** aynı kalıpta (tip `"PartyLeave"`): `WAIT` → `CLI-16`, `leave_wait`, `value = sinceEnteredMs`, `limit = kPartyLeaveMinMs`; `RATE` → `CLI-11`, `rate`; `peerId = -1`.
   - **`ActionExecutor::RequestPartyDecline`:** `RequestPartyAccept` ile aynı sırayla: `not_in_game`, `dead` (olay yazılmaz), `no_invite` (olay yazılmaz; `inviter = (int)(inv & 0xFFFF)`), `nowMs`, `sinceInviteMs` (`m_partyInviteAtMs`), `inWindow`, `PartyDeclineCheck c = { sinceInviteMs, inWindow }`, `verdict != PARTYDECLINE_OK` → `RejectPartyDecline` (davet kaydı **silinmez**). **Gönder:** `decisionId = NextDecisionId(s)`; `ACTION_SUBMIT` (`decisions`): `"decision_id","type":"PartyDecline","inviter":<id>`. Paket: `Packet pkt(WIZ_PARTY, uint8(PARTY_PERMIT)); pkt << uint8(0);` (`PartyHandler.cpp:23-29`). `s->m_castSelfId = user->GetID(); s->m_partyInviteEcho = 0;` (davet tüketilir) → `user->HandlePacket(pkt)` (gecikme `steady_clock` ile) → `s->m_actionWindow.Record(nowMs)`. **Sonuç:** reddeden bota cevap gelmez; `SENT "declined"`, `ACTION_RESULT`: `"decision_id","type":"PartyDecline","ok":true,"reason":"declined","latency_us","inviter":<id>`. `peerId = inviter`. (`m_partyErrorEcho`/`m_partyJoinEcho` **okunmaz**: ret yanıtı lidere gider.)
   - **`ActionExecutor::RequestPartyLeave`:**
     - `s == nullptr || s->m_pUser == nullptr || !isInGame()` → `REFUSED "not_in_game"`; `isDead()` → `REFUSED "dead"` (olay yazılmaz). **Ön koşullar (olay yazılmaz):** `(s->m_partyInviteEcho.load() & (1ull << 63)) != 0` → `REFUSED "invite_pending"`; `!user->isInParty()` → `REFUSED "not_in_party"`. (`isInParty()` botun **kendi** durumudur; yalnızca ön koşul, `HandlePacket`'tan **önce**; sonuç kararında kullanılmaz.)
     - `nowMs`, `inWindow`, `sinceEnteredMs` (`m_partyEnteredHasAt` ise `now - m_partyEnteredAt` ms, değilse 0). `PartyLeaveCheck c = { s->m_partyEnteredHasAt, sinceEnteredMs, inWindow }`; `verdict != PARTYLEAVE_OK` → `RejectPartyLeave`.
     - `bool asLeader = user->isPartyLeader();` (yalnızca telemetri alanı, `HandlePacket`'tan **önce**). **Gönder:** `decisionId = NextDecisionId(s)`; `ACTION_SUBMIT`: `"decision_id","type":"PartyLeave","as_leader":true|false` (**`"mode"` anahtarı kullanma**: telemetri ortak `"mode":"live"` alanıyla çakışır, F4-08 Tur 1 bulgusu). Paket: `Packet pkt(WIZ_PARTY, uint8(PARTY_REMOVE)); pkt << uint16(user->GetID());` (`PartyHandler.cpp:38-40`: `u8 alt-opcode + u16`). `s->m_castSelfId = user->GetID(); s->m_partyLeaveEcho = 0;` → `HandlePacket` → `Record(nowMs)`.
     - **Sonuç (yalnızca cevap paketinden):** `l = s->m_partyLeaveEcho.load()`; geçerli (bit 63) ve `((l >> 16) & 0xFF) == 1` ve `(int)(l & 0xFFFF) == (int)user->GetID()` → `SENT "left"`; geçerli ve `((l >> 16) & 0xFF) == 2` → `SENT "disbanded"`; aksi halde `FAILED "no_result"`. Başarıda (`left`/`disbanded`) `s->m_partyEnteredHasAt = false;`.
     - `ACTION_RESULT`: `"decision_id","type":"PartyLeave","ok","reason","latency_us"`. `peerId = -1`.

`ActionExecutor.cpp`'de (yorumlar dahil) `PARTY_DELETE`/`PARTY_PROMOTE` dizgesi **geçmez** (K6 grep'i takılır). `HandlePacket(pkt)` çağrısı `WIZ_PARTY` için bu planda **iki yerde** (her yeni fonksiyonda bir). `CUser::PartyProcess/PartyRequest/PartyInsert/PartyCancel/PartyRemove/PartyDelete/PartyPromote`, `m_bInParty`, `m_bPartyLeader`, `m_sPartyIndex`, `GetPartyID`, `GetPartyPtr`, `CreateParty`, `StateChangeServerDirect` bot kodunda **yok**.

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirimler (`CommandPartyAccept`'in altına): `void CommandPartyDecline(const std::string & args);` ve `void CommandPartyLeave(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()`:** `paccept` dalının yanına `pdecline` (`CommandPartyDecline(args)`) ve `pleave` (`CommandPartyLeave(args)`) fiillerini ekle; "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast, pot, sit, stand, target, regene, pinvite, paccept, pdecline, pleave)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **`CommandPartyDecline(args)`** `CommandPartyAccept` kalıbıyla (tek bot; `SplitWords`, `FindSession`, `IsKnownBotName`, `PhaseName`, `WriteBotLog`). Kullanım: `BotManager: cmd pdecline: usage: pdecline <bot>` (`words.size() != 1` iken tek satır ve dön). Hata günlükleri `CommandPartyAccept`'taki gibi (`unknown or not spawned bot '<ad|?>'`, `<bot> not in game (phase X)`; hepsi `cmd pdecline:` önekiyle). `ActionExecutor::RequestPartyDecline(s, now)`: `REFUSED` → `BotManager: cmd pdecline: <bot> refused (<reason>)`; `SENT` → `BotManager: cmd pdecline: <bot> declined invitation of #<peerId>`; `FAILED` → `... failed (<reason>)`. `char message[256]`.
3. **`CommandPartyLeave(args)`** aynı kalıpta. Kullanım: `BotManager: cmd pleave: usage: pleave <bot>`. `REFUSED` → `BotManager: cmd pleave: <bot> refused (<reason>)`; `SENT` → `BotManager: cmd pleave: <bot> <reason>` (yani `left` ya da `disbanded`); `FAILED` → `... failed (<reason>)`.
4. **`TickSessions()`, `BeginDespawn()`, `BuildStatusLines()`:** **değişmez** (tek seferlik aksiyonlar, `list` biçimi sabit). Başka hiçbir yere dokunma (`ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj`, `.filters`, `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` **değişmez** (yeni dosya yok). Mevcut dosyaların kodlama/satır sonu/BOM durumu korunur.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı üç yeni test adını (`Combat_PartyDeclineCheck`, `Combat_PartyLeaveCheck_Order`, `Combat_PartyLeaveCheck_Boundaries`) içerir ve toplam test sayısı **39**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; dosyada `#include` yalnızca `<algorithm>` ve `<cstdint>`; `grep -n "std::min\|std::max" BotCore/BotCombat.h` yeni satır göstermez.
- [ ] K5: party paketleri yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_PARTY" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma: F4-08'in iki satırı + yeni iki fonksiyon) ve `BotSession.cpp` (`OnPacket` kayıt) satırlarını gösterir; `grep -nE "PartyRequest|PartyInsert|PartyCancel|PartyPromote|PartyProcess|PartyRemove|PartyDelete|CreateParty|GetPartyPtr|GetPartyID|m_bInParty|m_bPartyLeader|m_sPartyIndex|StateChangeServerDirect" GameServer/Bot/*.cpp GameServer/Bot/*.h` boş (`RequestPartyDecline`, `RequestPartyLeave`, `RejectPartyDecline`, `RejectPartyLeave`, `PartyOutcome` gibi ad eşleşmeleri **bu desenle eşleşmez**; eşleşme varsa Uygulayıcı Raporu'nda açıkla).
- [ ] K6: iki yeni fonksiyonda da guard atlanmıyor: `RequestPartyDecline`'da `HandlePacket`'tan önce `no_invite` kontrolü, `CheckPartyDecline` çağrısı ve `PARTYDECLINE_OK` dışında erken dönüş; `RequestPartyLeave`'de `HandlePacket`'tan önce `invite_pending`/`not_in_party` kontrolleri, `CheckPartyLeave` çağrısı ve `PARTYLEAVE_OK` dışında erken dönüş; `WIZ_PARTY` için `HandlePacket(pkt)` çağrısı her yeni fonksiyonda tek (kod okumasıyla; Claude çalışma zamanında da sınar). Ret paketi `PARTY_PERMIT` + `uint8(0)`, ayrılma paketi `PARTY_REMOVE` + `uint16(user->GetID())` (başka kimlik/alt-opcode yok; `grep -n "PARTY_DELETE\|PARTY_PROMOTE" GameServer/Bot/ActionExecutor.cpp` boş).
- [ ] K7: ayrılma sonucu yalnızca cevap paketinden: `RequestPartyLeave` gövdesinde sonuç kararı yalnızca `m_partyLeaveEcho`'dan verilir; `isInParty()`/`isPartyLeader()` yalnızca `RequestPartyInvite` (F4-08), `RequestPartyLeave` ön koşulu/telemetri alanı için ve `HandlePacket`'tan **önce** geçer (`grep -n "isInParty\|isPartyLeader" GameServer/Bot/ActionExecutor.cpp` yalnızca bu satırları gösterir); `m_sHp`, `m_iMaxHp`, `GetHealth`, `GetMaxHealth` bu fonksiyonların satır aralığında geçmez.
- [ ] K8: `OnPacket()` ve F4-08 kodu yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-09 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki bilinçli değiştirilen satır(lar)ı gösterir; `git diff gece/2026-10-02...bot/F4-09 -- GameServer/Bot/ActionExecutor.cpp | grep '^-' | grep -v '^---'` **boş** (yalnızca ekleme).
- [ ] K9: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutla çalışır; `git diff gece/2026-10-02...bot/F4-09 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca `unknown command` mesaj satırını gösterir; `Startup()`/`Tick()`/`TickSessions()`/`BuildStatusLines()`/`BeginDespawn()` ve ini okuma değişmedi.
- [ ] K10: `git diff --stat gece/2026-10-02...bot/F4-09` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K11: değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K12: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(`, `SByte`, `DByte` yok; yeni `ACTION_SUBMIT`/`ACTION_RESULT` alan dizgilerinde `"mode"` anahtarı yok (`grep -n '\\"mode\\"' GameServer/Bot/ActionExecutor.cpp` boş).
- [ ] K13: F4-01..F4-08 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack" ...` ≥ 1, `grep -c "CheckCastStart" ...` ≥ 1, `grep -c "CheckPotion" ...` ≥ 1, `grep -c "CheckStance" ...` ≥ 1, `grep -c "CheckTargetHp" ...` ≥ 1, `grep -c "CheckRegene" ...` ≥ 1, `grep -c "CheckPartyInvite" ...` ≥ 1, `grep -c "CheckPartyAccept" ...` ≥ 1; önceki 36 testin tamamı hâlâ geçiyor; `EmitFairnessReject` çağrıları `"Move"`/`"Attack"`/`"Cast"`/`"Potion"`/`"State"`/`"TargetHp"`/`"Regene"`/`"PartyInvite"`/`"PartyAccept"` geçiyor, yeni `"PartyDecline"` ve `"PartyLeave"` eklendi.
- [ ] K14 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-09
git diff gece/2026-10-02...bot/F4-09 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-09 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-09 -- GameServer/Bot/ActionExecutor.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-09 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "std::min\|std::max" BotCore/BotCombat.h
grep -n "WIZ_PARTY" GameServer/Bot/*.cpp
grep -nE "PartyRequest|PartyInsert|PartyCancel|PartyPromote|PartyProcess|PartyRemove|PartyDelete|CreateParty|GetPartyPtr|GetPartyID|m_bInParty|m_bPartyLeader|m_sPartyIndex|StateChangeServerDirect" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "PARTY_DELETE\|PARTY_PROMOTE" GameServer/Bot/ActionExecutor.cpp
grep -n "CheckPartyDecline\|CheckPartyLeave\|HandlePacket\|isInParty\|isPartyLeader" GameServer/Bot/ActionExecutor.cpp
grep -n "m_sHp\|m_iMaxHp\|GetHealth\|GetMaxHealth" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(\|SByte\|DByte" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
grep -n '\\"mode\\"' GameServer/Bot/ActionExecutor.cpp
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-09
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. AIServer bağlı olmalıdır (`AG_USER_PARTY`). Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; **bir dosyadaki satırlar aynı `Tick()`'te sırayla çalışır**, ayrı dosyalar ≥ 1,05 sn arayla verilmelidir; `decline_wait`/`leave_wait` testleri için bu önemlidir). Botlar: Karus `BotWP_K` (lider), `BotMF_K`, `BotPHD_K`, `BotWG_K`; zone 71, aynı bölge. Gözlem `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'dan. Beklenmeyen `no_result` bu planın hatası değil, **sonuç olarak raporlanır** (özellikle `Send_PartyMember` cevaplarının ayrılan botun kendi alıcısına gelip gelmediği; gelmiyorsa **durup** raporla, bot sunucu nesnesine bakarak "başarılı" demez).

1. **Ret mutlu yolu (KI-014 çözümü):** `spawn BotWP_K,BotMF_K`; `pinvite BotWP_K BotMF_K` → `invited BotMF_K (created)`. Hemen (≤ 1 sn, aynı dosyada) `pdecline BotMF_K` → `refused (decline_wait)` + `FAIRNESS_REJECT` (`type:"PartyDecline"`, `rule:"CLI-16"`, `reason:"decline_wait"`, `value` < 1000, `limit` 1000), JSONL'de `PartyDecline` `ACTION_SUBMIT` yok; davetten ≥ 1,05 sn sonra `pdecline BotMF_K` → `declined invitation of #<BotWP_K sid>`; JSONL `ACTION_SUBMIT` (`type:"PartyDecline"`, `inviter`) → `ACTION_RESULT` (`ok:true`, `reason:"declined"`, `latency_us` < 20000). Tekrar `pdecline BotMF_K` → `refused (no_invite)`, JSONL'de olay yok. **Dolaylı kanıt:** ≥ 1,05 sn sonra `pinvite BotWP_K BotMF_K` → `invited BotMF_K (created)` (BotMF_K artık "party'de" sayılmıyor; ret işlenmeseydi `failed (refused_target)` olurdu) ve `paccept BotMF_K` (≥ 1,05 sn sonra) → `joined`.
2. **Üyenin ayrılması (≥ 3 üyeli party):** `BotWP_K` lider, `BotMF_K` ve `BotPHD_K` üye (create + accept, insert + accept; aralar ≥ 1,05 sn). `pleave BotMF_K` aynı saniyede kabulden hemen sonra → `refused (leave_wait)` + `FAIRNESS_REJECT` (`type:"PartyLeave"`, `CLI-16`, `leave_wait`, `value` < 1000, `limit` 1000); ≥ 1,05 sn sonra `pleave BotMF_K` → `left`; JSONL `ACTION_SUBMIT` (`type:"PartyLeave"`, `as_leader:false`) → `ACTION_RESULT` (`ok:true`, `reason:"left"`). Tekrar `pleave BotMF_K` → `refused (not_in_party)`, JSONL'de olay yok. **Dolaylı kanıt:** `pinvite BotWP_K BotMF_K` → `sent` ve `paccept BotMF_K` → `joined` (BotMF_K party dışındaydı); `BotPHD_K` hâlâ üye (`pleave BotPHD_K` ≥ 1,05 sn sonra → `left`).
3. **İki kişilik party'den ayrılma:** `BotWP_K` (lider) + `BotMF_K`; `pleave BotMF_K` (≥ 1,05 sn sonra) → `disbanded` (`ACTION_RESULT` `reason:"disbanded"`); `pleave BotWP_K` → `refused (not_in_party)` (party silindi, liderin bayrağı kalktı).
4. **Liderin ayrılması:** `BotWP_K` lider, `BotMF_K` + `BotPHD_K` üye; `pleave BotWP_K` → `disbanded` (`as_leader:true`); ardından `pleave BotMF_K`, `pleave BotPHD_K` → ikisi de `refused (not_in_party)` (party dağıldı, MEC-PTY-03).
5. **Ön koşullar ve guard:** davet bekleyen `BotWG_K` (pinvite ile, kabul edilmemiş) için `pleave BotWG_K` → `refused (invite_pending)`, JSONL'de olay yok; ardından `pdecline BotWG_K` (≥ 1,05 sn sonra) ile davet kapatılır; `pleave BotPHD_K` party'siz bota → `refused (not_in_party)`; **CLI-11:** tek `BotCommands.txt` dosyasında altı ardışık `target BotWP_K BotMF_K` / `target BotWP_K BotPHD_K` değişimi, ardından aynı dosyada `pleave BotWP_K` (party'de, ≥ 1 sn eski giriş) → `refused (rate)` + `FAIRNESS_REJECT` (`CLI-11`, `rate`); ölü bot (`attack` ile öldürülen) için `pdecline`/`pleave` → `refused (dead)`, JSONL'de olay yok (sınanamazsa "sınanmadı").
6. **Ömür ve komut doğrulaması:** `despawn BotMF_K` sonrası `pleave BotMF_K` → `unknown or not spawned bot` ya da `not in game (phase despawned)`; `pdecline Ghost` → `unknown or not spawned bot '?'`; argümansız ve fazla argümanlı `pdecline`/`pleave` → kullanım satırı (tek satır); `RESPAWN_CYCLES=2` iken `pdecline`/`pleave` → `cmd rejected` (yeniden sınanmazsa raporda söylenir); party'li bot `despawn` edilince sunucu çökmez, `GameServer.log`'da yeni hata yok.
7. **Gerilemesiz:** önceki dilimlerin komutları (`move`, `attack`, `cast`, `pot`, `sit`/`stand`, `target`, `regene`, `pinvite`/`paccept`) önceki çıktıyı verir; `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama `pdecline`/`pleave` çalışır; `ENABLED=0` → komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde (≤ 1 ms); sunucu 3/3 UP. İnsan istemcisi gerekmez (gerçek istemcide ret/ayrılma görünürlüğü `T-ARCH-14`, ölçüm `T-PARTY-02`, `docs/STATUS.md` "Proje sahibi testleri"). Temizlik: ini yedekten geri, bot satırları (`Hp=Mp=32000`, `PX=127400`, `PZ=89000`, `Loyalty=1000`) hedefli `UPDATE` ile geri yazılır; kişisel veri tabloları okunmaz.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). `pdecline`/`pleave` yalnızca `ENABLED=1` iken, üretim dışı test komutlarıdır.
- **Thread kuralı (ADR-0005):** `ActionExecutor` yalnızca IOCP thread'inde (komut çekirdeği `Tick()` içinden) çalışır; `HandlePacket` zaten bu thread'de koşar. Konsol/`+bot` işleyicisi `BotSession`/`ActionExecutor`'a **dokunmaz**; komutlar `EnqueueCommand` kuyruğundan gelir. `OnPacket()` her thread'den çağrılabilir: yeni dallar yalnızca atomik yazar. `m_partyEnteredHasAt`/`m_partyEnteredAt` yalnızca IOCP thread'inde.
- **Sonuç yalnızca yayınlanan cevaplardan** (AC-LRN-03 / `docs/13` §8). Ayrılmayı `isInParty()` ile doğrulamak yasak (yalnızca ön koşul); ayrılma kanıtı ayrılan botun kendi alıcısına gelen `PARTY_REMOVE` (kendi kimliği) ya da `PARTY_DELETE` paketidir. Ret için kanıt yoktur (§3, `[A]`).
- **Bilinen sınırlar `[A]`:** (a) `kPartyDeclineMinMs = 1000` ve `kPartyLeaveMinMs = 1000` ölçülmedi (gerçek istemcinin davet cevap ve ayrılma gecikmesi, liderin hangi alt-opcode'u yolladığı `T-PARTY-02`); (b) `Send_PartyMember` cevaplarının ayrılan botun alıcısına geldiği varsayımı (`PartyInsert` yayını gibi aynı mekanizma, F4-08'de `[V]`; yine de çalışma zamanında teyit edilir; gelmiyorsa `no_result` raporlanır, **durup** raporla); (c) `PartyCancel`'ın "lider oturumu yok / party yok" uç durumunda reddeden bot sunucuda "party'de" kalır (KI-014 notu); `SENT "declined"` bunu görmez; (d) ayrılma `PARTY_REMOVE` + kendi kimliğiyle yapılır, `PARTY_DELETE` ile değil: liderin ayrılması sunucuda party'yi dağıtır, devir yapmaz (`PartyPromote` F4-10'da; sunucunun kendi kopma yolu `PartyPromote(uid[1])` sonra `PartyRemove`'dur); (e) `AG_USER_PARTY` ve party çıkış/silme yolunun bot oturumunda sorunsuz çalıştığı varsayımı (§7 senaryoları 2–4, AIServer bağlı).
- Telemetri hacmi küçüktür (istek başına ≤ 2 olay); `droppable = false`, `IsEnabled` denetimi önceki aksiyonlarla aynı.
- `list` satırı ve `Telemetry.*` değişmez; mevcut komutların çıktıları **değiştirilmez**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-09` — `6b31acb [F4-09] PartyDecline/PartyLeave dilimi: guard, aksiyonlar ve komutlar` (+ bu rapor/Durum commit'i)
- Değişen dosyalar ve neden:
  - `BotCore/BotCombat.h`: `kPartyDeclineMinMs`/`kPartyLeaveMinMs`, `PartyDeclineCheck`/`PartyDeclineVerdict`/`CheckPartyDecline`, `PartyLeaveCheck`/`PartyLeaveVerdict`/`CheckPartyLeave` (yalnızca ekleme; `#include` değişmedi).
  - `Tests/BotCoreTests/CombatTests.cpp`: `Combat_PartyDeclineCheck`, `Combat_PartyLeaveCheck_Order`, `Combat_PartyLeaveCheck_Boundaries` (dosya sonuna ekleme).
  - `GameServer/Bot/ActionExecutor.h`: `PartyOutcome::reason` yorumu genişletildi; `RequestPartyDecline`/`RequestPartyLeave` bildirimleri.
  - `GameServer/Bot/ActionExecutor.cpp`: F4-08'in `created`/`joined` başarı dallarına party giriş zamanı; dosya sonuna `RejectPartyDecline`/`RejectPartyLeave` + `RequestPartyDecline`/`RequestPartyLeave` (tamamı ekleme; `grep '^-'` boş).
  - `GameServer/Bot/BotSession.h/.cpp`: `m_partyEnteredHasAt`/`m_partyEnteredAt`, `m_partyLeaveEcho`; başlatıcı listesi, `ResetForRespawn()`, `OnPacket()` `WIZ_PARTY` bloğuna `else if` (PARTY_REMOVE/PARTY_DELETE) ve yorum satırları eklendi.
  - `GameServer/Bot/BotManager.h/.cpp`: `CommandPartyDecline`/`CommandPartyLeave`; `pdecline`/`pleave` fiil dağıtımı ve `unknown command` listesi.
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
  proj-LogInServer.vcxproj -> ...\build\bin\x86-Release\Server\LogInServer.exe
  proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> ...\build\bin\x86-Release\Server\AIServer.exe
  BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  `Release`/`Debug` rc=0; `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp`, `BotCombat.h` için uyarı yok (yalnızca eski `GameServerDlg.cpp:816/1143/1802` uyarıları).
- Kabul kriterleri öz-değerlendirme: K1 ✔, K2 ✔, K3 ✔ (`39 tests, 0 failed`, Release+Debug; üç yeni ad çıktıda), K4 ✔ (grep'ler boş; `#include` yalnızca `<algorithm>`/`<cstdint>`), K5 ✔ (`WIZ_PARTY` yalnızca `ActionExecutor.cpp` 4 satır + `BotSession.cpp` 1; sunucu party sembol grep'i boş), K6 ✔ (`PARTY_DELETE`/`PARTY_PROMOTE` grep'i boş; `HandlePacket` her fonksiyonda tek; guard sırası plana uygun), K7 ✔ (`isInParty`/`isPartyLeader` yalnızca 1816/1817 + 2200/2226, hepsi `HandlePacket`'tan önce; `m_sHp` vb. yok), K8 ✔ (`ActionExecutor.cpp` `-` boş; `BotSession.cpp` `-` yalnızca başlatıcı listesi satırı), K9 ✔ (`BotManager.cpp` `-` yalnızca `unknown command` satırı), K10 ✔ (tam olarak §4'teki 8 dosya; dört vcxproj farkı boş; `Bot/` dışında GameServer dosyası yok), K11 ✔ (`file` hepsi ASCII+CRLF; `git diff --check` boş), K12 ✔ (`printf`/`Sleep`/`mutex`/`rand(`/`SByte`/`DByte` yok; `\"mode\"` yok), K13 ✔ (tüm `Check*` sayaçları beklenen; `39 tests, 0 failed`). K14 çalışma zamanı Claude'da.
- Plandan sapmalar ve gerekçeleri: `ActionExecutor.cpp`'de `RequestPartyInvite`'ın `if (created)` gövdesi süslü parantezsiz tek satır olduğundan, giriş zamanı atamaları (`m_partyEnteredHasAt`/`m_partyEnteredAt`) mevcut `if/else` bloğunun hemen ardından ayrı bir `if (created) { ... }` bloğuyla eklendi. Böylece plandaki "başka satır değişmez / yalnızca ekleme" (K8) şartı korunurken davranış birebir aynı kaldı (atamalar yalnızca `created` iken yapılır). `RequestPartyAccept`'ta `joined` dalı süslü olduğundan atamalar doğrudan dalın içine eklendi.
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: **DOĞRULANDI**
- İncelenen: `bot/F4-09` @ `5dd0177` (uygulama commit'i `6b31acb`; taban `gece/2026-10-02`; gece modu, `AUTO_LOOP=1`: birleştirme/push yapılmadı). Çalışma ağacı temizdi; sunucular kapalıydı (`0/3 hazır`).
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `tools/build.sh Release` rc=0; log'da `warning` 0 |
| K2 | ✔ | `tools/build.sh Debug` rc=0; log'da `warning` 0 |
| K3 | ✔ | `tools/run-tests.sh Release` ve `Debug`: `39 tests, 0 failed`; `Combat_PartyDeclineCheck`, `Combat_PartyLeaveCheck_Order`, `Combat_PartyLeaveCheck_Boundaries` `[ OK ]` |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` boş; `#include` yalnızca `<algorithm>` (`:6`), `<cstdint>` (`:7`); eklenen satırlarda `std::min/max` yok |
| K5 | ✔ | `WIZ_PARTY` yalnızca `ActionExecutor.cpp:1840` (F4-08), `:1984`, `:2139`, `:2238` ve `BotSession.cpp:108`; sunucu party sembolleri deseni (`PartyRequest\|PartyInsert\|...\|StateChangeServerDirect`) `Bot/*.cpp,*.h` içinde boş |
| K6 | ✔ | `RequestPartyDecline`: `no_invite` (`:2110`) → `CheckPartyDecline` `:2125` → `!= OK` erken dönüş → `HandlePacket` `:2146`; `RequestPartyLeave`: `invite_pending`/`not_in_party` (`:2197`, `:2203`) → `CheckPartyLeave` `:2221` → erken dönüş → `HandlePacket` `:2245`; her fonksiyonda tek `HandlePacket`; paketler `PARTY_PERMIT`+`uint8(0)` ve `PARTY_REMOVE`+`uint16(user->GetID())`; `PARTY_DELETE\|PARTY_PROMOTE` `ActionExecutor.cpp`'de boş. Çalışma zamanı: reddedilen `decline_wait`/`leave_wait`/`rate` durumlarında JSONL'de `ACTION_SUBMIT` yok |
| K7 | ✔ | `isInParty`/`isPartyLeader` yalnızca `:1816-1817` (F4-08), `:2200`, `:2226`, üçü de `HandlePacket`'tan önce; ayrılma sonucu yalnızca `m_partyLeaveEcho`'dan (`:2252-2262`); `m_sHp\|m_iMaxHp\|GetHealth\|GetMaxHealth` boş |
| K8 | ✔ | `ActionExecutor.cpp` silinen satır yok (yalnızca ekleme); `BotSession.cpp` silinen tek satır başlatıcı listesindeki `m_partyInviteAtMs(0)...m_partyJoinEcho(0)` satırı (sona `,` + `m_partyLeaveEcho(0)` eklendi); `OnPacket()` mevcut dalları aynı |
| K9 | ✔ | `BotManager.cpp` silinen tek satır `unknown command` mesajı; `Tick()`/`TickSessions()`/`BuildStatusLines()`/`BeginDespawn()`/ini okuma farkta yok |
| K10 | ✔ | `diff --stat`: §4'teki 8 dosya + plan dosyası; dört `.vcxproj`/`.filters` farkı boş; `Bot/` dışında dosya yok |
| K11 | ✔ | `file`: 8 dosya `ASCII text, with CRLF line terminators`; `git diff --check` rc=0 |
| K12 | ✔ | `printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(\|SByte\|DByte` ve `\"mode\"` `ActionExecutor.*`'de boş |
| K13 | ✔ | `CheckMoveStep` 2; `CheckAttack`/`CheckCastStart`/`CheckPotion`/`CheckStance`/`CheckTargetHp`/`CheckRegene`/`CheckPartyInvite`/`CheckPartyAccept` 1'er; `EmitFairnessReject` tipleri `Cast`/`Potion`/`State`/`TargetHp`/`Regene`/`PartyInvite`/`PartyAccept` + yeni `PartyDecline`/`PartyLeave`; önceki 36 test geçiyor |
| K14 | ✔ | Çalışma zamanı §7 senaryoları 1–7 geçti (aşağıda; sınanmayanlar notlarda) |

- **Çalışma zamanı (K14, Release, AIServer bağlı, `TELEMETRY=decisions`, zone 71; `BotWP_K` #2984 lider, `BotMF_K` #2985, `BotPHD_K` #2986, `BotWG_K` #2987; komut dosyaları `/tmp/cmd.sh` ile, dosyalar arası ≥ 1,3 sn):**
  1. Ret: `pinvite BotWP_K BotMF_K` → `created`; aynı dosyada `pdecline BotMF_K` → `refused (decline_wait)`, `FAIRNESS_REJECT` `CLI-16 decline_wait value:1.00 limit:1000.00`, `PartyDecline` `ACTION_SUBMIT` yok; sonraki dosyada `pdecline` → `declined invitation of #2984`, `ACTION_SUBMIT` (`inviter:2984`) → `ACTION_RESULT` (`ok:true`, `reason:"declined"`, `latency_us:62`); tekrar → `refused (no_invite)`, JSONL'de olay yok. **Dolaylı kanıt:** ardından `pinvite BotWP_K BotMF_K` → `created` (`failed (refused_target)` değil: KI-014 bot tarafında çözüldü) ve `paccept BotMF_K` → `joined`.
  2. Üye ayrılması (3 üyeli): `pinvite WP PHD` → `sent`; `paccept BotPHD_K` + aynı dosyada `pleave BotPHD_K` → `refused (leave_wait)` (`CLI-16 leave_wait value:0.00 limit:1000.00`, JSONL'de `ACTION_SUBMIT` yok); `pleave BotMF_K` → `left` (`as_leader:false`, `ok:true`, `latency_us:32`); tekrar → `refused (not_in_party)`, olay yok. **Dolaylı kanıt:** `pinvite WP MF` → `sent`, `paccept` → `joined`; `BotPHD_K` hâlâ üyeydi, `pleave BotPHD_K` → `left`.
  3. İki kişilik party: `pleave BotMF_K` → `disbanded` (`ACTION_RESULT` `reason:"disbanded"`); `pleave BotWP_K` → `refused (not_in_party)`.
  4. Lider: WP + MF + PHD; `pleave BotWP_K` → `disbanded` (`as_leader:true`, `latency_us:51`); `pleave BotMF_K`, `pleave BotPHD_K` → ikisi `refused (not_in_party)` (MEC-PTY-03).
  5. Ön koşul/guard: `pinvite WP WG` (kabulsüz) → `pleave BotWG_K` → `refused (invite_pending)`, olay yok; `pdecline BotWG_K` → `declined invitation of #2984`; `pleave BotPHD_K` (party'siz) → `refused (not_in_party)`; **CLI-11:** aynı dosyada altı `target` + `pleave BotWP_K` (party'de, eski giriş) → `refused (rate)`, `FAIRNESS_REJECT` `CLI-11 rate value:6.00 limit:6.00`. Ölü bot sınanmadı (bkz. not 2).
  6. Ömür/komut: `despawn BotMF_K` sonrası `pleave`/`pdecline BotMF_K` → `not in game (phase despawned)`; `pdecline Ghost`/`pleave Ghost` → `unknown or not spawned bot '?'`; argümansız ve fazla argümanlı iki komut → kullanım satırı (tek satır); lider `BotWP_K` ve üye `BotMF_K` `despawn` edildi: sunucu çökmedi, `GameServer.log` 32→32 satır, AIServer bağlı, `list` doğru.
  7. Gerilemesiz: `sit`/`stand` (`sat down`/`stood up`), `regene` (`refused (not_dead)`), `target` (altı ardışık `observed ... hp`), `pinvite`/`paccept` önceki çıktıyı verdi; `PERF_SAMPLE` `tick_p95_us` 92 ve 326 (≤ 1 ms; `tick_p99_us` 2,3 ms tek örnek, `skipped_ticks` 0); sunucu 3/3 UP.
- Bulgular: yok (engelleyici bulgu yok). Notlar:
  1. **[Not] Sapma kabul edildi:** `RequestPartyInvite`'ta `created` giriş zamanı atamaları (`ActionExecutor.cpp:1889-1893`) `if (created) reason = "created"; else {...}` zincirinin ardında ayrı bir `if (created)` bloğu olarak eklendi (planın "iki satır ekleme" ifadesi dalın içindeydi, ama dal süslüsüz tek satır olduğundan K8'in "yalnızca ekleme" şartı bunu gerektiriyordu). Davranış aynı; üslup olarak ileride iki dalı birleştirmek mümkün.
  2. **[Not] Sınanmadı:** öldürülerek ölü bot (`pdecline`/`pleave` → `refused (dead)`; kod `RequestPartyAccept`'taki `isDead()` dalıyla aynı), `RESPAWN_CYCLES=2` reddi (bu planda `ExecuteCommand` reddine dokunulmadı), `TELEMETRY=summary` ve `ENABLED=0` çalışma zamanı (yeni kod yalnızca komutla ve `PHASE_IN_GAME` oturumunda erişilir; `Tick()`/ini okuma farkta yok).
  3. **[Not]** `SENT "declined"` `[A]`: sunucu reddedene paket yollamaz; ret işlendiği yalnızca dolaylı (hedefin yeniden davet edilebilmesi) kanıtlandı. `Send_PartyMember` cevaplarının ayrılan botun kendi alıcısına geldiği teyit edildi (`left`/`disbanded` üç farklı yolda üretildi; `no_result` hiç görülmedi) `[V]`.
  4. **[Not]** İnsan testleri bekliyor: `T-ARCH-14` (gerçek istemcide ret/ayrılma görünürlüğü) ve `T-PARTY-02` (CLI-16 alt sınırları, liderin ayrılırken kullandığı alt-opcode) `docs/STATUS.md` "Proje sahibi testleri".
- Temizlik: `GameServer.ini` değişmedi (md5 `265a8e1c35ea12df46f6d006fe894d9b`, doğrulama öncesi aynı); `BotCommands.txt` tüketildi; `Logs/bots` → `Logs/bots_old_f409`; sunucular kapatıldı (`0/3 hazır`); dört bot satırı hedefli `UPDATE` ile `Hp=Mp=32000`, `PX=127400`, `PZ=89000`, `Loyalty=1000` geri yazıldı (4 satır; kişisel veri tablosu okunmadı).
- Birleştirme: gece modu, döngü betiği `gece/2026-10-02`'ye birleştirir; bu oturumda birleştirme/push yapılmadı.
