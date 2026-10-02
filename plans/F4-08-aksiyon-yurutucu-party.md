# F4-08: `ActionExecutor` party dilimi — `PartyInvite` / `PartyAccept` ve `BotFairnessGuard` CLI-15 kuralları

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-08` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-07 (`OnPacket()` ekleme kalıbı, `Regene` iskeleti) — `KAPANDI`; F4-06 (`RegionDelta` görüş denetimi, `TargetHpTarget` test sürücüsü kalıbı) — `KAPANDI`; F4-05 (`m_stateEcho` kullanımı) — `KAPANDI`; F4-01 — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-15 (`docs/03` §14, bu planla birlikte eklendi), CLI-11, MEC-PTY-01..03, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı |
| Tahmini büyüklük | M (8 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun sekizinci ve dokuzuncu gerçek aksiyonları: bir bot **gerçek `WIZ_PARTY` paketiyle** başka bir botu party'ye davet eder (`PartyInvite`: party yoksa `PARTY_CREATE`, varsa `PARTY_INSERT`), davet edilen bot **gerçek `PARTY_PERMIT 1` paketiyle** kabul eder (`PartyAccept`). Hepsi `CUser::HandlePacket()` üzerinden gider; sonuç yalnızca sunucunun botun alıcısına yayınladığı cevap paketlerinden okunur. Sunucu davet hızı, görüş alanı veya "davet edilen cevap vermeden bekletilmesi" konusunda hiçbir denetim yapmaz: `PartyRequest` davetin hedefini **anında** `m_bInParty = true` yapar (`PartyHandler.cpp:154-155`) ve kabul için bekleme yoktur. Bu yüzden paketler sunucuya gitmeden önce `BotFairnessGuard` kurallarından geçer: yalnızca lider (veya party'siz) davet eder (`not_leader`), davet edilen hedef 3×3 bölgede olmalı (`out_of_view`), iki davet arası ≥ 1,0 sn (`invite_gap`), kabul daveti aldıktan ≥ 1,0 sn sonra (`accept_wait`), CLI-11 aksiyon hızı. Karar katmanı yoktur: aksiyonları `/bot pinvite <bot> <hedef bot>` ve `/bot paccept <bot>` komutları tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün sekizinci dilimidir (ADR-0017 Ek F4-08; hareket → saldırı → cast → pot → duruş → hedef HP → `Regene` → **`Party` kurulumu** → `Promote`/`Chat` → `Perception`).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-08)" (bu planla birlikte yazıldı): paket düzenleri, guard kuralları, sonuç eşlemesi, kapsam.
- `docs/03` §10 **MEC-PTY-01..05** (party kuralları), §14 **CLI-15** (bu planla birlikte eklendi), **CLI-11**; `docs/09` §2 (party'yi gerçek paketlerle kurma).
- `plans/F4-07-aksiyon-yurutucu-regene.md` §5.2–§5.5: bu planın kalıpladığı iskelet (saf mantık `BotCore/BotCombat.h` + `ActionExecutor` + `BotSession` durumu + `BotManager` komutu). **Yazılı planı değil, birleşmiş kodu esas al** (`ActionExecutor.cpp` `NextDecisionId`, `EmitFairnessReject`, `RejectRegene`/`RequestRegene` dosya sonunda; `BotManager.cpp` `CommandTarget`, `CommandRegene`).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `ade97ec` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:379-381` — `HandlePacket` `case WIZ_PARTY`: `PartyProcess(pkt)`. Önceki kapılar (kripto, hesap, `m_bSelectedCharacter`) bot için geçilmiştir.
  - `GameServer/PartyHandler.cpp:6-50` — `PartyProcess`: `u8 alt-opcode` okur. `PARTY_CREATE` (1) ve `PARTY_INSERT` (3): `pkt >> strUserID` (**KO dizgesi, varsayılan çift baytlı uzunluk: `u16 uzunluk + bayt`**; `Packet` varsayılanı `m_doubleByte = true`, `shared/ByteBuffer.h:11`; bu planda `SByte()` **çağrılmaz**), boş ya da `> MAX_ID_SIZE` (20, `shared/globals.h:12`) ise döner; `g_pMain->GetUserPtr(ad, TYPE_CHARACTER)` (`GameServerDlg.cpp:462-480`; ad büyük harfe çevrilir, bot adı `AddCharacterName` ile `User.cpp:1046`'da haritaya girmiştir) yoksa **sessizce döner**; `PartyRequest(hedef sid, create)`. `PARTY_PERMIT` (2): `u8` (1 kabul → `PartyInsert()`, 0 ret → `PartyCancel()`).
  - `GameServer/PartyHandler.cpp:85-166` — `PartyRequest(memberid, bCreate)`: hedef yok / kendisi / zaten party'de (`isInParty()`) → `fail_return` (`errorCode = -1`); ulus farkı (Moradon/Forgotten Temple hariç), zone farkı, Chaos Dungeon → `-3`; seviye bandı (klan ve "chicken" muaf) → `-2`; `!bCreate` iken party yoksa veya dolu (8) → `-1`; `bCreate` iken `isInParty()` ise `-1`, `CreateParty(this)` başarısızsa `-1`. Create başarılıysa `m_bPartyLeader = true; StateChangeServerDirect(6, 1)` (lider 'P' sembolü, `:146-147`) ve `AG_USER_PARTY` AIServer'a. Sonra **hedefe** `pUser->m_sPartyIndex = m_sPartyIndex; pUser->m_bInParty = true` (`:154-155`) ve **hedefe** `WIZ_PARTY`: `u8 PARTY_PERMIT, u16 davet edenin sid, KO dizgesi davet edenin adı` (`:157-159`). `fail_return` (`:162-165`): **davet edene** `WIZ_PARTY`: `u8 PARTY_INSERT, i16 errorCode` (yük **3 bayt**). Başarıda davet edene **hiçbir** pozitif cevap gelmez (create dalında yalnızca aşağıdaki durum değişimi).
  - `GameServer/User.cpp:2777-2822` — `StateChangeServerDirect(bType, nBuff)`: `case 6` (`:2805-2807`) `nBuff = m_bPartyLeader`; her çağrı `WIZ_STATE_CHANGE`: `u16 sid, u8 bType, u32 nBuff` olarak `SendToRegion` ile **kendisine de** yayınlanır (F4-05'te `m_stateEcho` ile doğrulanmış kalıp).
  - `GameServer/PartyHandler.cpp:168-266` — `PartyInsert()` (kabul): `isInParty()` yoksa döner; lider ayrı zone'daysa veya party doluysa `PartyCancel()`; sonra mevcut her üyeyi kabul edene `WIZ_PARTY`: `u8 PARTY_INSERT, i16/u16 üye sid, u8 1, ad, ...` olarak yollar (`:213-241`), **en son** `g_pMain->Send_PartyMember(GetPartyID(), &result)` (`:253-261`) ile kendi katılımını (`u8 PARTY_INSERT, u16 kendi sid, u8 1 (başarı), KO dizgesi ad, i32 maxHp, i32 hp, u16 seviye, u16 sınıf, ...`) **kabul eden dahil tüm üyelere** yayınlar (`GameServerDlg.cpp:1080-1093`; üyelerin `Send()`'i bot alıcısına gider). Yani kabul edenin alıcısında **son** `PARTY_INSERT` kendi katılımıdır.
  - `GameServer/PartyHandler.cpp:52-83` — `PartyCancel()` (ret veya başarısız kabul): davet edene `u8 PARTY_INSERT, i16 -1`. Bu planda **ret yok** (kapsam dışı); yalnızca başarısız kabulde dolaylı çalışır.
  - `GameServer/User.h:313` `isInParty()`, `:319` `isPartyLeader()` (= `isInParty() && m_bPartyLeader`), `:420` `GetPartyID()`; `GameServer/Unit.h:54` `GetID()`. `GameServer/User.h:312` `isInGame()`.
  - `GameServer/Bot/BotSession.cpp:27-96` `OnPacket()` (son blok `WIZ_REGENE`), `:98-` `ResetForRespawn()`. `GameServer/Bot/BotManager.cpp` `ExecuteCommand` fiil dağıtımı (`regene` dalı + `unknown command` listesi), `CommandTarget` (iki bot çözme + `TargetHpTarget` kalıbı), `CommandRegene` (tek bot kalıbı).

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** `kPartyInviteGapMs`, `kPartyAcceptMinMs`, `PartyInviteCheck`, `PartyInviteVerdict`, `CheckPartyInvite`, `PartyAcceptCheck`, `PartyAcceptVerdict`, `CheckPartyAccept`. Birim testleri (`CombatTests.cpp`).
2. **`ActionExecutor::RequestPartyInvite(s, target, now)`** ve **`RequestPartyAccept(s, now)`** (tek seferlik aksiyonlar): ön koşullar → guard → `WIZ_PARTY` paketi → `HandlePacket` → yayınlanan cevaplardan sonuç eşleme → telemetri.
3. **Oturum durumu (`BotSession`):** son davet zamanı (`m_partyInviteHasLast`/`m_partyInviteLast`, IOCP thread) ve `OnPacket()`'in doldurduğu dört kayıt: alınan davet (`m_partyInviteEcho` + `m_partyInviteAtMs`), davet reddi (`m_partyErrorEcho`), katılım (`m_partyJoinEcho`).
4. **Komutlar (`BotManager`):** `pinvite <bot> <hedef bot>` ve `paccept <bot>` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten).
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT` / `ACTION_RESULT` (`"type":"PartyInvite"` ve `"PartyAccept"`), `FAIRNESS_REJECT` (`"type":"PartyInvite"` / `"PartyAccept"`).

**Kapsam dışı (yapılmayacak)**

- **Kim kimi ne zaman davet eder / kabul eder** (karar katmanı; `docs/09` §2 takım kurma, F7): yok. Komutlar guard dışında hiçbir koşula bakmaz.
- **Davet reddi (`PARTY_PERMIT 0`), party'den çıkma/atma (`PARTY_REMOVE`), dağıtma (`PARTY_DELETE`), lider devri (`PARTY_PROMOTE`), party chat (`WIZ_CHAT`), party arama panosu (`WIZ_PARTY_BBS`):** yok; sonraki dilim (F4-09: `PartyPromote` + `Chat`). Cevapsız bırakılan davet, hedefi sunucu tarafında "party'de" tutar (KI-014); test komutları bunu yaratmamak için her davetin ardından `paccept` vermelidir.
- **İnsan oyuncuyu bot adına davet etmek / insandan gelen daveti kabul etmek karar katmanı olmadan:** `pinvite` hedefi yalnızca **bot oturumudur**; `paccept` ise davet kim olursa olsun (insan dahil) alınan daveti kabul eder (insan testi T-ARCH-13).
- **Party yönetimi durumu** (`TeamBlackboard`, üye listesi, HP/MP yayınları `PARTY_HPCHANGE`): yok; `PARTY_INSERT` paketleri yalnızca sonuç eşlemesi için okunur, üye listesi tutulmaz.
- `ChatHandler.cpp` `+bot` yardım metni (KI-012; Claude doğrulamada günceller). Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya yok; **`GameServer` projesine dosya eklenmez**. `Telemetry.*`, `ScenarioRunner.*` değişmez; `list` satırı değişmez.
- Dokümanları (`docs/03`, `docs/13`, `docs/16`, `docs/15`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | Yalnızca ekleme (§5.2); mevcut içerik ve `#include`'lar değişmez |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | Yalnızca ekleme: üç yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `PartyInviteTarget`, `PartyOutcome`, iki yeni statik fonksiyon |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | Yalnızca dosya sonuna ekleme (§5.4); başka hareket/saldırı/cast/pot/duruş/hedef HP/`Regene` kodu değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca party üyeleri (§5.3) |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi, `ResetForRespawn()` ve `OnPacket()`'e **ekleme** bloğu (mevcut bloklar değişmez) |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandPartyInvite`, `CommandPartyAccept` bildirimleri |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, iki komut, `unknown command` listesi |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (Yeni dosya açılmaz: guard `BotCombat.h`'ye eklenir, böylece `BotCore*.vcxproj` değişmez.)

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-08 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotCombat.h` (saf mantık, mevcut `namespace BotCore` içine ekleme)

Mevcut kodun biçimini (tab, Allman, `inline`, İngilizce kısa yorum, ölçülmemiş değer `[A]`) koru. Yeni `#include` gerekmez. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez** (yorumlarda da `shared/` dizgisi **yazma**: K4 grep'i takılır). `CheckRegene`'den **sonra**, `namespace`'in kapanışından önce ekle. **`std::min`/`std::max` kullanma**:

```cpp
	// --- party slice (ADR-0017 Ek F4-08) ---

	constexpr uint32_t kPartyInviteGapMs = 1000;   // docs/03 CLI-15: a human needs at least this long between two party invitations [A] (unmeasured)
	constexpr uint32_t kPartyAcceptMinMs = 1000;   // docs/03 CLI-15: reading the invitation popup and clicking accept takes a human at least this long [A] (unmeasured)

	struct PartyInviteCheck
	{
		bool inParty;             // the bot is in a party (it was invited or it leads one)
		bool isLeader;            // ... and it leads it
		int regionDelta;          // RegionDelta(bot position, target position)
		bool hasLast;             // a party invitation was sent earlier in this spawn
		uint32_t sinceLastMs;     // since that invitation
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum PartyInviteVerdict
	{
		PARTYINVITE_OK = 0,
		PARTYINVITE_REJECT_LEADER = 1,   // CLI-15 (in a party but not its leader: only the leader invites)
		PARTYINVITE_REJECT_VIEW = 2,     // CLI-15 (target outside the 3x3 regions)
		PARTYINVITE_REJECT_GAP = 3,      // CLI-15 (second invitation before kPartyInviteGapMs)
		PARTYINVITE_REJECT_RATE = 4      // CLI-11
	};

	// Guard rule for a party invitation. Order: leader, view, gap, rate.
	inline PartyInviteVerdict CheckPartyInvite(const PartyInviteCheck & c);

	struct PartyAcceptCheck
	{
		uint32_t sinceInviteMs;   // since the invitation reached the bot
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum PartyAcceptVerdict
	{
		PARTYACCEPT_OK = 0,
		PARTYACCEPT_REJECT_WAIT = 1,   // CLI-15 (accepted before kPartyAcceptMinMs after the invitation arrived)
		PARTYACCEPT_REJECT_RATE = 2    // CLI-11
	};

	// Guard rule for accepting an invitation. The caller has already checked that an invitation is pending. Order: wait, rate.
	inline PartyAcceptVerdict CheckPartyAccept(const PartyAcceptCheck & c);
```

Gövdeler (`CheckTargetHp` kalıbı, aynı yerde `inline`):

- `CheckPartyInvite`: `c.inParty && !c.isLeader` → `PARTYINVITE_REJECT_LEADER`; `c.regionDelta > kViewRegionRadius` → `..._VIEW`; `c.hasLast && c.sinceLastMs < kPartyInviteGapMs` → `..._GAP`; `c.actionsInWindow >= kMaxActionsPerWindow` → `..._RATE`; aksi halde `PARTYINVITE_OK`.
- `CheckPartyAccept`: `c.sinceInviteMs < kPartyAcceptMinMs` → `PARTYACCEPT_REJECT_WAIT`; `c.actionsInWindow >= kMaxActionsPerWindow` → `PARTYACCEPT_REJECT_RATE`; aksi halde `PARTYACCEPT_OK`.

**`Tests/BotCoreTests/CombatTests.cpp` (ekleme, mevcut makro stili):** üç yeni `TEST_CASE`:

- `Combat_PartyInviteCheck_Order`: yardımcı `OkInvite()` (`inParty = false, isLeader = false, regionDelta = 0, hasLast = false, sinceLastMs = 0, actionsInWindow = 0`). Hepsi ihlal (`inParty = true, isLeader = false, regionDelta = 2, hasLast = true, sinceLastMs = 0, actionsInWindow = 6`) → `PARTYINVITE_REJECT_LEADER`; lider düzelince (`isLeader = true`) → `..._VIEW`; görüş düzelince (`regionDelta = 1`) → `..._GAP`; boşluk düzelince (`sinceLastMs = 1000`) → `..._RATE`; `actionsInWindow = 5` → `PARTYINVITE_OK`; party'siz (`inParty = false, isLeader = false`) → `PARTYINVITE_OK`; liderken (`inParty = true, isLeader = true`) → `PARTYINVITE_OK`.
- `Combat_PartyInviteCheck_Boundaries`: `sinceLastMs = 999` + `hasLast = true` → `..._GAP`; `1000` → `OK`; `hasLast = false` iken `sinceLastMs = 0` → `OK`; `regionDelta = 1` → `OK`, `2` → `..._VIEW`; `kPartyInviteGapMs == 1000`.
- `Combat_PartyAcceptCheck`: `sinceInviteMs = 999` → `PARTYACCEPT_REJECT_WAIT`; `1000` → `PARTYACCEPT_OK`; `0` → `..._WAIT`; hepsi ihlal (`sinceInviteMs = 0, actionsInWindow = 6`) → `..._WAIT` (sıra); yalnızca `actionsInWindow = 6` → `..._RATE`; `5` → `OK`; `kPartyAcceptMinMs == 1000`.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'de ölüm izleme üyelerinin altına (aynı yorum/hizalama biçimi):

```cpp
	bool m_partyInviteHasLast;                             // IOCP thread only: m_partyInviteLast is valid for this spawn
	std::chrono::steady_clock::time_point m_partyInviteLast;   // IOCP thread only: when the last party invitation went out
```

ve atomik üyelerin yanına (`m_regeneEcho`'nun altına):

```cpp
	std::atomic<uint64> m_partyInviteAtMs;                 // written by OnPacket() BEFORE m_partyInviteEcho: steady_clock ms when the invitation arrived
	std::atomic<uint64> m_partyInviteEcho;                 // written by OnPacket(): valid bit | inviter sid of the last PARTY_PERMIT; cleared by PartyAccept
	std::atomic<uint64> m_partyErrorEcho;                  // written by OnPacket(): valid bit | uint16(error code) of the last PARTY_INSERT refusal (payload of 3 bytes)
	std::atomic<uint64> m_partyJoinEcho;                   // written by OnPacket(): valid bit | sid << 8 | flag of the last PARTY_INSERT member packet
```

`BotSession.cpp`: başlatıcı listesine `m_partyInviteHasLast(false)` (`m_deadSeen(false)`'ten sonra) ve `m_partyInviteAtMs(0), m_partyInviteEcho(0), m_partyErrorEcho(0), m_partyJoinEcho(0)` (`m_regeneEcho(0)`'dan sonra) ekle; sıra üye bildirim sırasıyla aynı olmalı (derleyici sıra uyarısı vermemeli). `ResetForRespawn()` içine `m_partyInviteHasLast = false; m_partyInviteAtMs = 0; m_partyInviteEcho = 0; m_partyErrorEcho = 0; m_partyJoinEcho = 0;` ekle.

`OnPacket()`: `WIZ_REGENE` bloğundan **sonra**, **yalnızca ekleme** (mevcut bloklar bayt bayt aynı kalır). `<chrono>` zaten `BotSession.h` üzerinden gelir:

```cpp
	// Party packets (PartyHandler.cpp): u8 sub-opcode first. PARTY_PERMIT (2) = an invitation arrived: u16 inviter sid + name.
	// PARTY_INSERT (3) with a 3-byte payload = the bot's own invitation was refused: i16 error code. PARTY_INSERT with
	// a longer payload = a member joined: u16 sid, u8 flag (1 = success, 100 = leader moved), name, ...
	// ActionExecutor::RequestPartyInvite / RequestPartyAccept clear the records before their request and read them
	// afterwards, on the same thread.
	if (opcode == WIZ_PARTY && pkt.size() >= 1)
	{
		uint8 sub = pkt.read<uint8>(0);
		if (sub == PARTY_PERMIT && pkt.size() >= 5)
		{
			uint16 sid = pkt.read<uint16>(1);
			uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
			m_partyInviteAtMs = nowMs;
			m_partyInviteEcho = (1ull << 63) | uint64(sid);
		}
		else if (sub == PARTY_INSERT && pkt.size() == 3)
		{
			int16 code = pkt.read<int16>(1);
			m_partyErrorEcho = (1ull << 63) | uint64(uint16(code));
		}
		else if (sub == PARTY_INSERT && pkt.size() >= 4)
		{
			uint16 sid = pkt.read<uint16>(1);
			uint8 flag = pkt.read<uint8>(3);
			m_partyJoinEcho = (1ull << 63) | (uint64(sid) << 8) | uint64(flag);
		}
	}
```

(`pkt.read<T>(offset)` bayt ofsetiyle okur; mevcut bloklardaki gibi. `PARTY_PERMIT`/`PARTY_INSERT` `shared/packets.h:238-250` tanımlarıdır, `stdafx.h` üzerinden görünür.)

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

**Başlık (`ActionExecutor.h`):** `RegeneOutcome`'dan sonra:

```cpp
// Caller-supplied view of the party invitation target (ADR-0017 Ek F4-08). Temporary, like TargetHpTarget: the
// /bot pinvite test driver fills it from the target bot's session; the Perception slice replaces the source.
struct PartyInviteTarget
{
	int16 id;            // target's socket id (not sent; self / invalid check)
	std::string name;    // target's character name (the PARTY_CREATE / PARTY_INSERT payload)
	float x;             // metres
	float z;
};

struct PartyOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed. SENT: "created" (PartyInvite, PARTY_CREATE confirmed by the bot's own
	                       // leader state broadcast), "sent" (PartyInvite, PARTY_INSERT: no refusal reply), "joined" (PartyAccept).
	                       // FAILED: "refused_target" (-1), "refused_level" (-2), "refused_zone" (-3), "refused_other", "no_result".
	                       // REFUSED: "not_in_game", "dead", "bad_target", "no_invite", or a guard verdict
	                       // ("not_leader", "out_of_view", "invite_gap", "accept_wait", "rate")
	int peerId;            // PartyInvite: the target's id; PartyAccept: the inviter's id; -1 = none
};
```

`class ActionExecutor` içine (`RequestRegene` bildiriminin altına, aynı yorum kalıbıyla):

```cpp
	// One-shot party invitation: sends one WIZ_PARTY (PARTY_CREATE when the bot is in no party, PARTY_INSERT when it leads
	// one) with the target's name through CUser::HandlePacket() after the guard (CLI-15: leader only, target in the 3x3
	// regions, >= 1 s between invitations; CLI-11). Result only from published replies: a refusal arrives as WIZ_PARTY
	// PARTY_INSERT + i16 code (FAILED "refused_*"); PARTY_CREATE is confirmed by the bot's own WIZ_STATE_CHANGE type 6
	// (leader flag 1) -> SENT "created"; PARTY_INSERT has no positive reply -> SENT "sent" (no refusal reply; [A]).
	static PartyOutcome RequestPartyInvite(BotSession * s, const PartyInviteTarget & target,
		std::chrono::steady_clock::time_point now);

	// One-shot acceptance of the pending invitation (PARTY_PERMIT 1 through CUser::HandlePacket()) after the guard
	// (CLI-15: >= 1 s after the invitation arrived; CLI-11). The pending invitation is the one OnPacket() recorded
	// (m_partyInviteEcho); none -> REFUSED "no_invite" without an event. SENT "joined": the bot's own PARTY_INSERT member
	// packet (sid == its id, flag 1) arrived. FAILED "no_result": it did not (e.g. the leader changed zone).
	static PartyOutcome RequestPartyAccept(BotSession * s, std::chrono::steady_clock::time_point now);
```

(`ActionExecutor.h` zaten `<string>` içeriyorsa ek `#include` gerekmez; yoksa ekle.)

**`ActionExecutor.cpp`** (dosya sonuna, `RequestRegene`'den sonra; yeni bölüm başlığı `// --- party slice (ADR-0017 Ek F4-08) ---`):

1. **`static PartyOutcome RejectPartyInvite(BotSession * s, CUser * user, BotCore::PartyInviteVerdict v, const BotCore::PartyInviteCheck & c, int peerId)`** (`RejectTargetHp` kalıbı): `LEADER` → `rule "CLI-15", reason "not_leader", value = 0, limit = 0`; `VIEW` → `rule "CLI-15", reason "out_of_view", value = regionDelta, limit = kViewRegionRadius`; `GAP` → `rule "CLI-15", reason "invite_gap", value = sinceLastMs, limit = kPartyInviteGapMs`; `RATE` → `rule "CLI-11", reason "rate", value = actionsInWindow, limit = kMaxActionsPerWindow`. `decisionId = NextDecisionId(s)`; `EmitFairnessReject(s, user, decisionId, "PartyInvite", rule, reason, value, limit)`; `REFUSED` + reason + `peerId`. Sunucuya paket **gitmez**. **`RejectPartyAccept`** aynı kalıpta (tip `"PartyAccept"`): `WAIT` → `CLI-15`, `accept_wait`, `value = sinceInviteMs`, `limit = kPartyAcceptMinMs`; `RATE` → `CLI-11`, `rate`.
2. **`ActionExecutor::RequestPartyInvite`:**
   - `s == nullptr || s->m_pUser == nullptr || !isInGame()` → `REFUSED "not_in_game"`; `isDead()` → `REFUSED "dead"` (olay yazılmaz); `target.id < 0 || target.id == (int16)user->GetID() || target.name.empty() || target.name.size() > 20` → `REFUSED "bad_target"` (olay yazılmaz; 20 = `MAX_ID_SIZE`).
   - `nowMs` (`SetStance` ile aynı `steady_clock` → ms dönüşümü), `inWindow = s->m_actionWindow.CountInWindow(nowMs)`, `sinceLastMs` (`m_partyInviteHasLast` ise `now - m_partyInviteLast` ms, değilse 0).
   - `PartyInviteCheck c = { user->isInParty(), user->isPartyLeader(), BotCore::RegionDelta(user->GetX(), user->GetZ(), target.x, target.z), s->m_partyInviteHasLast, sinceLastMs, inWindow }`; `verdict != PARTYINVITE_OK` → `RejectPartyInvite`. (`isInParty()`, `isPartyLeader()`, `GetX()`, `GetZ()` botun **kendi** durumudur; yalnızca guard girdisi ve paket tipi seçimi için, `HandlePacket`'tan **önce**.)
   - **Gönder:** `bool create = !c.inParty`; `decisionId = NextDecisionId(s)`; `ACTION_SUBMIT` (`decisions`): `"decision_id","type":"PartyInvite","target":<id>,"mode":"create"|"insert"`. Paket: `Packet pkt(WIZ_PARTY, uint8(create ? PARTY_CREATE : PARTY_INSERT)); pkt << target.name;` (sunucu `u8 alt-opcode` + KO dizgesi okur, `PartyHandler.cpp:11-16`; `SByte()` **çağrılmaz**, varsayılan çift baytlı uzunluk sunucunun okuduğu biçimdir). `s->m_castSelfId = user->GetID(); s->m_partyErrorEcho = 0; s->m_stateEcho = 0;` → `user->HandlePacket(pkt)` (gecikme `steady_clock` ile) → `s->m_actionWindow.Record(nowMs); s->m_partyInviteHasLast = true; s->m_partyInviteLast = now;`.
   - **Sonuç (yalnızca cevap paketlerinden):** (a) `e = s->m_partyErrorEcho.load()` geçerliyse (bit 63) `code = (int16)(e & 0xFFFF)`: `-1` → `refused_target`, `-2` → `refused_level`, `-3` → `refused_zone`, diğer → `refused_other` → `FAILED`. (b) Değilse `create` ise `st = s->m_stateEcho.load()`: geçerli ve `((st >> 32) & 0xFF) == 6` ve `(uint32)(st & 0xFFFFFFFF) == 1` → `SENT "created"`, aksi halde `FAILED "no_result"`. (c) Değilse (`insert`) → `SENT "sent"`.
   - `ACTION_RESULT`: `"decision_id","type":"PartyInvite","ok","reason","latency_us"` ve (a) durumunda `"code":<int>`. `peerId = target.id`.
3. **`ActionExecutor::RequestPartyAccept`:**
   - `not_in_game` / `dead` (olay yazılmaz). `inv = s->m_partyInviteEcho.load()`; bit 63 yoksa → `REFUSED "no_invite"` (olay yazılmaz). `inviter = (int)(inv & 0xFFFF)`.
   - `nowMs`; `invMs = s->m_partyInviteAtMs.load()`; `sinceInviteMs = nowMs >= invMs ? (uint32)(nowMs - invMs) : 0`; `inWindow`. `PartyAcceptCheck c = { sinceInviteMs, inWindow }`; `verdict != PARTYACCEPT_OK` → `RejectPartyAccept` (davet kaydı **silinmez**: bot sonra tekrar deneyebilir).
   - **Gönder:** `decisionId = NextDecisionId(s)`; `ACTION_SUBMIT`: `"decision_id","type":"PartyAccept","inviter":<id>`. Paket: `Packet pkt(WIZ_PARTY, uint8(PARTY_PERMIT)); pkt << uint8(1);` (`PartyHandler.cpp:28-31`). `s->m_castSelfId = user->GetID(); s->m_partyJoinEcho = 0; s->m_partyInviteEcho = 0;` (davet tüketilir) → `HandlePacket` → `Record(nowMs)`.
   - **Sonuç:** `j = s->m_partyJoinEcho.load()` geçerli, `(int)((j >> 8) & 0xFFFF) == (int)user->GetID()` ve `(uint8)(j & 0xFF) == 1` → `SENT "joined"` (`peerId = inviter`); aksi halde `FAILED "no_result"`. (`PartyInsert` sırası: mevcut üyeler önce, kabul edenin kendi katılımı **son** gönderilir; kayıt "son yazan kazanır" olduğundan doğrudur.)
   - `ACTION_RESULT`: `"decision_id","type":"PartyAccept","ok","reason","latency_us","inviter":<id>`.

`HandlePacket(pkt)` çağrısı `WIZ_PARTY` için **iki yerde** (her fonksiyonda bir). `CUser::PartyProcess/PartyRequest/PartyInsert/...` doğrudan çağrılmaz; `m_bInParty`, `m_bPartyLeader`, `m_sPartyIndex`, `GetPartyID`, `GetPartyPtr`, `CreateParty`, `StateChangeServerDirect` bot kodunda **yok**.

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirimler: `void CommandPartyInvite(const std::string & args);` ve `void CommandPartyAccept(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()`:** `regene` dalının yanına `pinvite` (`CommandPartyInvite(args)`) ve `paccept` (`CommandPartyAccept(args)`) fiillerini ekle; "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast, pot, sit, stand, target, regene, pinvite, paccept)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **`CommandPartyInvite(args)`:** `CommandTarget` kalıbını izle (iki bot, `SplitWords`, `FindSession`, `IsKnownBotName`, `PhaseName`, `WriteBotLog`). Kullanım satırı (`words.size() != 2` iken **tek** günlük satırı ve dön): `BotManager: cmd pinvite: usage: pinvite <bot> <target bot>`. Hata günlükleri `CommandTarget`'taki gibi (`unknown or not spawned bot '<ad|?>'`, `<bot> not in game (phase X)`, `target <bot> not in game (phase X)`; hepsi `cmd pinvite:` önekiyle). Test sürücüsü: `PartyInviteTarget tv = { (int16)target->m_pUser->GetSocketID(), target->m_charName, target->m_pUser->GetX(), target->m_pUser->GetZ() };` (yorum: "Test driver: the target comes straight from the target bot's session; the Perception slice replaces this source, not PartyInviteTarget"). `ActionExecutor::RequestPartyInvite(s, tv, now)`: `REFUSED` → `BotManager: cmd pinvite: <bot> refused (<reason>)`; `SENT` → `BotManager: cmd pinvite: <bot> invited <hedef> (<reason>)`; `FAILED` → `BotManager: cmd pinvite: <bot> failed (<reason>)`. `char message[256]`.
3. **`CommandPartyAccept(args)`:** `CommandRegene` kalıbı (tek bot). Kullanım: `BotManager: cmd paccept: usage: paccept <bot>`; `REFUSED` → `... refused (<reason>)`; `SENT` → `BotManager: cmd paccept: <bot> joined party of #<peerId>`; `FAILED` → `... failed (<reason>)`.
4. **`TickSessions()`, `BeginDespawn()`, `BuildStatusLines()`:** **değişmez** (tek seferlik aksiyonlar, seri yok, `list` biçimi sabit). Başka hiçbir yere dokunma (`ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj`, `.filters`, `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` **değişmez** (yeni dosya yok). Mevcut dosyaların kodlama/satır sonu/BOM durumu korunur.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı üç yeni test adını (`Combat_PartyInviteCheck_Order`, `Combat_PartyInviteCheck_Boundaries`, `Combat_PartyAcceptCheck`) içerir ve toplam test sayısı **36**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; dosyada `#include` yalnızca `<algorithm>` ve `<cstdint>`; `grep -n "std::min\|std::max" BotCore/BotCombat.h` yeni satır göstermez.
- [ ] K5: party paketleri yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_PARTY" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma, `RequestPartyInvite`/`RequestPartyAccept`) ve `BotSession.cpp` (`OnPacket` kayıt) satırlarını gösterir; `grep -nE "PartyRequest|PartyInsert|PartyCancel|PartyPromote|PartyProcess|PartyRemove|PartyDelete|CreateParty|GetPartyPtr|GetPartyID|m_bInParty|m_bPartyLeader|m_sPartyIndex|StateChangeServerDirect" GameServer/Bot/*.cpp GameServer/Bot/*.h` boş (`RequestPartyInvite`, `RequestPartyAccept`, `PartyOutcome`, `PartyInviteTarget` gibi ad eşleşmeleri **bu desenle eşleşmez**; eşleşme varsa Uygulayıcı Raporu'nda açıkla).
- [ ] K6: iki fonksiyonda da guard atlanmıyor: `RequestPartyInvite`'ta `HandlePacket`'tan önce `CheckPartyInvite` çağrısı ve `PARTYINVITE_OK` dışında erken dönüş; `RequestPartyAccept`'ta `HandlePacket`'tan önce `no_invite` kontrolü, `CheckPartyAccept` çağrısı ve `PARTYACCEPT_OK` dışında erken dönüş; `WIZ_PARTY` için `HandlePacket(pkt)` çağrısı her fonksiyonda tek (kod okumasıyla; Claude çalışma zamanında da sınar).
- [ ] K7: sonuç yalnızca cevap paketlerinden: `RequestPartyInvite`/`RequestPartyAccept` gövdelerinde sonuç kararı yalnızca `m_partyErrorEcho`, `m_stateEcho`, `m_partyJoinEcho`, `m_partyInviteEcho`'dan verilir; `isInParty()`/`isPartyLeader()` yalnızca `RequestPartyInvite`'ta guard girdisi/paket tipi seçimi için ve `HandlePacket`'tan **önce** geçer (`grep -n "isInParty\|isPartyLeader" GameServer/Bot/ActionExecutor.cpp` yalnızca bu satırları gösterir); `m_sHp`, `m_iMaxHp`, `GetHealth`, `GetMaxHealth` bu fonksiyonların satır aralığında geçmez.
- [ ] K8: `OnPacket()` yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-08 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki bilinçli değiştirilen satır(lar)ı gösterir; `OnPacket()` mevcut `WIZ_SEL_CHAR`/`WIZ_ATTACK`/`WIZ_MAGIC_PROCESS`/`WIZ_STATE_CHANGE`/`WIZ_TARGET_HP`/`WIZ_REGENE` blokları değişmedi.
- [ ] K9: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutla çalışır; `git diff gece/2026-10-02...bot/F4-08 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca `unknown command` mesaj satırını gösterir; `Startup()`/`Tick()`/`TickSessions()`/`BuildStatusLines()`/`BeginDespawn()` ve ini okuma değişmedi.
- [ ] K10: `git diff --stat gece/2026-10-02...bot/F4-08` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K11: değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K12: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(`, `SByte`, `DByte` yok.
- [ ] K13: F4-01..F4-07 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack" ...` ≥ 1, `grep -c "CheckCastStart" ...` ≥ 1, `grep -c "CheckPotion" ...` ≥ 1, `grep -c "CheckStance" ...` ≥ 1, `grep -c "CheckTargetHp" ...` ≥ 1, `grep -c "CheckRegene" ...` ≥ 1; önceki 33 testin tamamı hâlâ geçiyor; `EmitFairnessReject` çağrıları `"Move"`/`"Attack"`/`"Cast"`/`"Potion"`/`"State"`/`"TargetHp"`/`"Regene"` geçiyor, yeni `"PartyInvite"` ve `"PartyAccept"` eklendi.
- [ ] K14 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-08
git diff gece/2026-10-02...bot/F4-08 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-08 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-08 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "std::min\|std::max" BotCore/BotCombat.h
grep -n "WIZ_PARTY" GameServer/Bot/*.cpp
grep -nE "PartyRequest|PartyInsert|PartyCancel|PartyPromote|PartyProcess|PartyRemove|PartyDelete|CreateParty|GetPartyPtr|GetPartyID|m_bInParty|m_bPartyLeader|m_sPartyIndex|StateChangeServerDirect" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "CheckPartyInvite\|CheckPartyAccept\|HandlePacket\|isInParty\|isPartyLeader" GameServer/Bot/ActionExecutor.cpp
grep -n "m_sHp\|m_iMaxHp\|GetHealth\|GetMaxHealth" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(\|SByte\|DByte" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-08
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; **bir dosyadaki satırlar aynı `Tick()`'te sırayla çalışır**, ayrı dosyalar ≥ 1,05 sn arayla verilmelidir; `invite_gap` ve `accept_wait` testleri için bu önemlidir). Botlar: Karus `BotWP_K` (lider), `BotMF_K`, `BotPHD_K`; El Morad `BotWP_E`; zone 71. Karus botları aynı başlangıç bölgesinde doğar (görüş denetimi geçer); El Morad botu ~750 m uzakta. Gözlem `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'dan. Beklenmeyen `no_result` bu planın hatası değil, **sonuç olarak raporlanır** (özellikle sunucunun `WIZ_PARTY` cevaplarını botun kendi alıcısına gönderip göndermediği: `Send()` doğrulaması; göndermiyorsa **durup** raporla, bot sunucu nesnesine bakarak "başarılı" demez). Party kurma/katılma `AG_USER_PARTY` ile AIServer'a da gider: AIServer kapalıysa sınanmaz, raporda söylenir.

1. **Mutlu yol (create + accept):** `spawn BotWP_K,BotMF_K,BotPHD_K`; `pinvite BotWP_K BotMF_K` → `invited BotMF_K (created)`; JSONL: `ACTION_SUBMIT` (`type:"PartyInvite"`, `mode:"create"`) → `ACTION_RESULT` (`ok:true`, `reason:"created"`, `latency_us` < 20000). Hemen (≤ 1 sn) `paccept BotMF_K` → `refused (accept_wait)` + `FAIRNESS_REJECT` (`type:"PartyAccept"`, `rule:"CLI-15"`, `reason:"accept_wait"`, `value` < 1000, `limit` 1000), JSONL'de `PartyAccept` `ACTION_SUBMIT` yok; davetten ≥ 1,05 sn sonra `paccept BotMF_K` → `joined party of #<BotWP_K sid>`; `ACTION_RESULT` (`ok:true`, `reason:"joined"`, `inviter`). Tekrar `paccept BotMF_K` → `refused (no_invite)`, JSONL'de olay yok (davet tüketildi).
2. **Mevcut party'ye ekleme (insert):** `pinvite BotWP_K BotPHD_K` (≥ 1,05 sn sonra) → `invited BotPHD_K (sent)`, JSONL `mode:"insert"`, `reason:"sent"`; `paccept BotPHD_K` ≥ 1,05 sn sonra → `joined`. Üç botluk party kuruldu; sunucu `GameServer.log`'a yeni hata yazmadı; operatör doğrulaması: `BotWP_K` lider olarak `sit`/`stand`/`pot` komutlarını çalıştırmaya devam eder (party kurulumu bot oturumunu bozmadı).
3. **Guard ve reddedilen davetler:** `pinvite BotMF_K BotWP_K` (BotMF_K üye, lider değil) → `refused (not_leader)` + `FAIRNESS_REJECT` (`CLI-15`, `not_leader`); yeni spawn edilen `BotWG_K` için aynı dosyada ardışık iki `pinvite BotWP_K BotWG_K` → ilki `sent`, ikincisi `refused (invite_gap)` (`value` < 1000, `limit` 1000; paket sunucuya gitmez), ardından `paccept BotWG_K` ile davet kapatılır (KI-014); `pinvite BotWP_K BotWP_E` → `refused (out_of_view)` (`value` ≥ 2, `limit` 1); sunucu reddi: zaten üye olan `BotMF_K` için `pinvite BotWP_K BotMF_K` → `failed (refused_target)` ve `ACTION_RESULT` `code:-1`, `ok:false`. Sunucu reddi `-3` (ulus/zone) ve `-2` (seviye) sınanamazsa raporda "sınanmadı" yazılır.
4. **`CLI-11` ve ölü bot:** tek `BotCommands.txt` dosyasında altı ardışık `target BotWP_K BotMF_K` / `target BotWP_K BotPHD_K` değişimi (her biri önceki hedeften farklı olduğundan `poll` almaz, hepsi CLI-11 penceresine yazılır), ardından aynı dosyada `pinvite BotWP_K BotWG_K` → `refused (rate)` + `FAIRNESS_REJECT` (`CLI-11`, `rate`); ölü bot: `attack` ile öldürülen bot için `pinvite`/`paccept` → `refused (dead)`, JSONL'de olay yok (sınanamazsa "sınanmadı").
5. **Ömür ve komut doğrulaması:** `despawn BotMF_K` (party üyesi) sonrası `pinvite BotWP_K BotMF_K` → `target BotMF_K not in game (phase despawned)`; `pinvite Ghost BotWP_K` → `unknown or not spawned bot '?'`; argümansız ve `pinvite BotWP_K` / `paccept BotWP_K BotMF_K` → kullanım satırı (tek satır); `RESPAWN_CYCLES=2` iken `pinvite` → `cmd rejected` (yeniden sınanmazsa raporda söylenir).
6. **Party üyesi ayrılırken sunucu sağlığı:** party'li botlardan biri `despawn` edildiğinde sunucu çökmez, `GameServer.log`'da yeni hata yok, kalan bot `in_game` kalır; lider (`BotWP_K`) `despawn` edilince de aynı (party silinir, MEC-PTY-03). Sonuç raporlanır (`CUser::OnDisconnect` party temizliği bot oturumunda sorunsuz çalıştığı `[A]`; aksaklık **durup** raporlanır).
7. **Gerilemesiz:** önceki dilimlerin komutları (`move` F4-01, `attack` F4-02, `cast` F4-03, `pot` F4-04, `sit`/`stand` F4-05, `target` F4-06, `regene` F4-07) party'li ve party'siz botlarda önceki çıktıyı verir; `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama `pinvite`/`paccept` çalışır; `ENABLED=0` → komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde (≤ 1 ms); sunucu 3/3 UP. İnsan istemcisi gerekmez (gerçek istemcide party paneli görünürlüğü `T-ARCH-13`, `docs/STATUS.md` "Proje sahibi testleri"). Temizlik: ini yedekten geri, bot satırları (`Hp=Mp=32000`, `PX=127400`, `PZ=89000`, `Loyalty=1000`) hedefli `UPDATE` ile geri yazılır; kişisel veri tabloları okunmaz.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). `pinvite`/`paccept` yalnızca `ENABLED=1` iken, üretim dışı test komutlarıdır.
- **Thread kuralı (ADR-0005):** `ActionExecutor` yalnızca IOCP thread'inde (komut çekirdeği `Tick()` içinden) çalışır; `HandlePacket` zaten bu thread'de koşar. Konsol/`+bot` işleyicisi `BotSession`/`ActionExecutor`'a **dokunmaz**; komutlar `EnqueueCommand` kuyruğundan gelir. `OnPacket()` her thread'den çağrılabilir: yeni bloklar yalnızca atomik yazar (`m_partyInviteAtMs` **önce**, `m_partyInviteEcho` sonra yazılır). `m_partyInviteHasLast`/`m_partyInviteLast` yalnızca IOCP thread'inde.
- **Sonuç yalnızca yayınlanan cevaplardan** (AC-LRN-03 / `docs/13` §8). Party üyeliğini `isInParty()` ile doğrulamak yasak (yalnızca guard girdisi); katılımı kabul edenin alıcısına gelen kendi `PARTY_INSERT` paketi kanıtlar.
- **Sunucu `Packet` dizge biçimi:** `pkt << ad` varsayılan **çift baytlı** uzunluk yazar ve sunucunun `PartyProcess` okuması da öyledir; `SByte()` çağırmak paketi bozar (K12 yakalar).
- **Bilinen sınırlar `[A]`:** (a) `kPartyInviteGapMs = 1000` ve `kPartyAcceptMinMs = 1000` ölçülmedi (gerçek istemcinin davet aralığı ve `PARTY_PERMIT` cevap gecikmesi `T-PARTY-01`); (b) `PARTY_INSERT` ile davetin sunucuya ulaştığının **pozitif** kanıtı davet edende yoktur (`SENT "sent"` = ret cevabı gelmedi); pozitif kanıt davet edilenin `PARTY_PERMIT` kaydıdır ve `paccept` çalıştığında `no_invite` olmaması bunu kanıtlar; (c) `Send()`'in `WIZ_PARTY`/`WIZ_STATE_CHANGE` cevaplarını botun kendi alıcısına ilettiği varsayımı (`Send_PartyMember` dahil; `WIZ_TARGET_HP`/`WIZ_REGENE` ile aynı mekanizma, F4-06/F4-07'de `[V]`; doğrulamada teyit edilir; iletmiyorsa `no_result` raporlanır, **durup** raporla); (d) cevapsız bırakılan davet hedefi sunucuda "party'de" tutar (KI-014); (e) davet edilen insan ise ve cevap vermezse aynı durum geçerlidir (insan bunu kendi istemcisinden çözer); (f) `AG_USER_PARTY` ve party çıkış/silme yolunun bot oturumunda sorunsuz çalıştığı varsayımı (§7 senaryo 6).
- Telemetri hacmi küçüktür (istek başına ≤ 2 olay); `droppable = false`, `IsEnabled` denetimi önceki aksiyonlarla aynı.
- `list` satırı ve `Telemetry.*` değişmez; mevcut komutların çıktıları **değiştirilmez**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-08` (taban: `gece/2026-10-02`); uygulama commit'i `234880f` (8 dosya); rapor/durum commit'i bu turda.
- Değişen dosyalar ve nedenleri:
  - `BotCore/BotCombat.h`: `kPartyInviteGapMs`, `kPartyAcceptMinMs`, `PartyInviteCheck`/`PartyInviteVerdict`/`CheckPartyInvite`, `PartyAcceptCheck`/`PartyAcceptVerdict`/`CheckPartyAccept` eklendi (saf mantık, `CheckRegene`'den sonra). Yalnızca ekleme; `#include`'lar ve mevcut içerik değişmedi.
  - `Tests/BotCoreTests/CombatTests.cpp`: üç `TEST_CASE` eklendi (`Combat_PartyInviteCheck_Order`, `Combat_PartyInviteCheck_Boundaries`, `Combat_PartyAcceptCheck`; toplam 33 → 36).
  - `GameServer/Bot/ActionExecutor.h`: `PartyInviteTarget`, `PartyOutcome` ve `RequestPartyInvite`/`RequestPartyAccept` bildirimleri eklendi.
  - `GameServer/Bot/ActionExecutor.cpp`: dosya sonuna party dilimi eklendi (`RejectPartyInvite`, `RejectPartyAccept`, iki aksiyon). Mevcut hareket/saldırı/cast/pot/duruş/hedef HP/`Regene` kodu değişmedi.
  - `GameServer/Bot/BotSession.h`: `m_partyInviteHasLast`/`m_partyInviteLast` (IOCP) ve dört atomik kayıt (`m_partyInviteAtMs`, `m_partyInviteEcho`, `m_partyErrorEcho`, `m_partyJoinEcho`).
  - `GameServer/Bot/BotSession.cpp`: başlatıcı listesi + `ResetForRespawn()` + `OnPacket()`'e ekleme bloğu. Mevcut `WIZ_SEL_CHAR`/`WIZ_ATTACK`/`WIZ_MAGIC_PROCESS`/`WIZ_STATE_CHANGE`/`WIZ_TARGET_HP`/`WIZ_REGENE` blokları değişmedi.
  - `GameServer/Bot/BotManager.h`: `CommandPartyInvite`/`CommandPartyAccept` bildirimleri.
  - `GameServer/Bot/BotManager.cpp`: `pinvite`/`paccept` fiil dağıtımı, iki komut ve `unknown command` listesi. `Startup()`/`Tick()`/`TickSessions()`/`BuildStatusLines()`/`BeginDespawn()` ve ini okuma değişmedi.
- Derleme sonucu:
  - `./tools/build.sh Release` rc=0; son satır: `proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe`.
  - `./tools/build.sh Debug` rc=0; son satır: `proj-GameServer.vcxproj -> ...\build\bin\x86-Debug\Server\GameServer.exe`.
  - Değişen dosyalarda uyarı yok (`BotCombat.h`, `ActionExecutor.*`, `BotSession.*`, `BotManager.*`, `CombatTests.cpp`); kalan uyarılar eski satırlarda (`Map.cpp`, `User.cpp`, `GameServerDlg.cpp:1143/1802`, `UpgradeHandler.cpp`).
  - `./tools/run-tests.sh Release` ve `Debug`: `36 tests, 0 failed` (rc=0); üç yeni test adı çıktıda.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (Release rc=0, ilgili dosyalarda uyarı yok).
  - K2 ✔ (Debug rc=0).
  - K3 ✔ (36 test, üç yeni ad, Release+Debug yeşil).
  - K4 ✔ (`windows.h`/`stdafx`/`GameServer`/`shared/` eşleşmesi yok; `#include` yalnızca `<algorithm>`, `<cstdint>`; `std::min`/`std::max` yok).
  - K5 ✔ (`WIZ_PARTY` yalnızca `ActionExecutor.cpp:1840/1978` ve `BotSession.cpp:104`; doğrudan party çağrısı/kalıntı deseni boş — iki yorum satırı "sunucu" sözcüğü kullanacak biçimde yeniden yazıldı, aşağıya bakınız).
  - K6 ✔ (kod okuması: iki fonksiyonda `HandlePacket(pkt)` bir kez; guard `HandlePacket`'tan önce; `PARTYINVITE_OK`/`PARTYACCEPT_OK` dışında erken dönüş; `no_invite` kontrolü önce).
  - K7 ✔ (`isInParty`/`isPartyLeader` yalnızca `ActionExecutor.cpp:1816-1817`, guard girdisi; `m_sHp`/`m_iMaxHp`/`GetHealth`/`GetMaxHealth` bu fonksiyonlarda yok; sonuç yalnızca `m_partyErrorEcho`/`m_stateEcho`/`m_partyJoinEcho`/`m_partyInviteEcho`'dan).
  - K8 ✔ (`BotSession.cpp` eksi satır yalnızca başlatıcı listesinin bilinçli satırı).
  - K9 ✔ (`BotManager.cpp` eksi satır yalnızca `unknown command` mesajı).
  - K10 ✔ (`git diff --stat` yalnızca 8 dosya; proje/filtre/`BotCore.vcxproj`/`BotCoreTests.vcxproj` farkı boş; `GameServer/` içinde `Bot/` dışı dosya değişmedi).
  - K11 ✔ (`file`: 8 dosya ASCII+CRLF; `git diff --check` boş).
  - K12 ✔ (`printf`/`Sleep`/`lock_guard`/`mutex`/`CreateThread`/`rand(`/`SByte`/`DByte` eşleşmesi yok).
  - K13 ✔ (`CheckMoveStep`=2, diğer guard'lar ≥1; 36 test geçiyor; `EmitFairnessReject` tipleri `Move`/`Attack`/`Cast`/`Potion`/`State`/`TargetHp`/`Regene` + yeni `PartyInvite`/`PartyAccept`).
  - K14: Claude'un çalışma zamanı doğrulaması (bu turda yapılmadı).
- Plandan sapmalar ve gerekçeleri:
  - §5.4'teki iki yorum satırı `PartyProcess()`/`PartyInsert()` adlarını içeriyordu; K5'in doğrudan-çağrı grep'ini temiz tutmak için yorumlar sunucu adı geçmeyecek biçimde yeniden yazıldı (`"the same bound the server applies..."`, `"The server sends the existing members first..."`). Kod davranışı ve planın özü değişmedi.
  - `RejectPartyAccept` de `peerId` = davet eden sid döndürür (plan bunu açıkça yazmıyordu; `RejectPartyInvite` kalıbıyla tutarlılık için).
  - `pinvite` hedefi bot oturumu bulunamazsa plan beklenen `unknown or not spawned bot '<ad|?>'` biçimi kullanıldı; hedefin kendi oturumu `s` ile aynıysa `RequestPartyInvite` içindeki `bad_target` reddi uygulanır (komut katmanı ayrıca engellemez).
- Açık sorular:
  - Yok (bloke eden). Not: `Send()`'in `WIZ_PARTY`/`WIZ_STATE_CHANGE` cevaplarını bot alıcısına ilettiği varsayımı `[A]`; çalışma zamanında Claude teyit edecek (plan §8-c). İnsan testleri T-ARCH-13 ve T-PARTY-01 bekliyor.

### Tur 2

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-08` (taban: `gece/2026-10-02`); düzeltme commit'i bu turda (yalnızca `GameServer/Bot/ActionExecutor.cpp`, 1 satır).
- Değişen dosyalar ve nedenleri:
  - `GameServer/Bot/ActionExecutor.cpp` (`RequestPartyInvite`, `ACTION_SUBMIT` alanları, Doğrulama Turu 1 bulgu 1): `"mode":` alan adı `"invite_mode":` yapıldı; değerler (`"create"`/`"insert"`) ve tek satır dışındaki hiçbir kod değişmedi. Böylece telemetri satırının ortak `mode` alanı (`Telemetry.cpp`, `"live"`) ile yinelenen anahtar çakışması bitti.
- Derleme sonucu:
  - `./tools/build.sh Release` rc=0; `ActionExecutor.cpp` yeniden derlendi, **uyarı yok**.
  - `./tools/build.sh Debug` rc=0; `ActionExecutor.cpp` yeniden derlendi, **uyarı yok**.
  - Kalan uyarılar eski satırlarda (`GameServerDlg.cpp:816/1143/1802`).
  - `./tools/run-tests.sh Release`: `36 tests, 0 failed` (rc=0); `Debug`: `36 tests, 0 failed` (rc=0).
  - `grep -n '"mode"' GameServer/Bot/ActionExecutor.cpp` → çıktı boş (rc=1).
- Plandan sapmalar: Yok; düzeltme talimatı birebir uygulandı. Başka dosyaya dokunulmadı (`docs/**`, `BotCombat.h`, testler, `GameServer/**` diğer dosyaları dahil); sunucu çalıştırılmadı.
- Açık sorular: Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: **DÜZELTME GEREKLİ**
- İncelenen: `bot/F4-08` @ `aecc6ed` (uygulama commit'i `234880f`; taban `gece/2026-10-02`; gece modu, `AUTO_LOOP=1`: birleştirme/push yapılmadı). Çalışma ağacı temizdi.
- Kriter sonuçları:

| # | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` `touch`'lanıp `tools/build.sh Release` yeniden derlendi: rc=0, 4 dosya derlendi, değişen dosyalarda `warning` yok (toplam 2 uyarı, eski dosyalarda) |
| K2 | ✔ | `tools/build.sh Debug` rc=0 |
| K3 | ✔ | `tools/run-tests.sh Release` ve `Debug`: `36 tests, 0 failed`; `Combat_PartyInviteCheck_Order`, `Combat_PartyInviteCheck_Boundaries`, `Combat_PartyAcceptCheck` `[ OK ]` |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` boş (rc=1); `#include` yalnızca `<algorithm>`, `<cstdint>` (`:6-7`); eklenen satırlarda `std::min/max` yok |
| K5 | ✔ | `WIZ_PARTY` yalnızca `ActionExecutor.cpp:1840`, `:1978` ve `BotSession.cpp:104`; doğrudan party çağrısı/kalıntı deseni `GameServer/Bot/*.cpp,*.h` içinde boş |
| K6 | ✔ | `CheckPartyInvite` `:1823` → `HandlePacket` `:1848`; `no_invite` `:1946-1953` → `CheckPartyAccept` `:1964` → `HandlePacket` `:1986`; her ikisinde `!= OK` erken dönüş; her fonksiyonda tek `HandlePacket`. Çalışma zamanı: reddedilen `accept_wait`/`invite_gap`/`out_of_view`/`rate`/`not_leader` durumlarında JSONL'de `ACTION_SUBMIT` yok |
| K7 | ✔ | `isInParty`/`isPartyLeader` yalnızca `:1816-1817` (guard girdisi, `HandlePacket`'tan önce); `m_sHp\|m_iMaxHp\|GetHealth\|GetMaxHealth` yeni aralıkta boş; sonuç yalnızca `m_partyErrorEcho`/`m_stateEcho`/`m_partyJoinEcho`/`m_partyInviteEcho` |
| K8 | ✔ | `BotSession.cpp` silinen tek satır başlatıcı listesindeki `m_regeneEcho(0)` satırı; `OnPacket()` mevcut blokları değişmedi, party bloğu yalnızca ekleme |
| K9 | ✔ | `BotManager.cpp` silinen tek satır `unknown command` mesajı. `ENABLED=0` çalışma zamanında: `pinvite`/`paccept` dosyası tüketilmedi (`BotCommands.txt` yerinde), `Bot_*.log` satır farkı 0, `Logs/bots` oluşmadı |
| K10 | ✔ | `diff --stat`: yalnızca §4'teki 8 dosya + plan; dört `.vcxproj`/`.filters` farkı 0 satır; `docs/`, `CLAUDE.md`, `AGENTS.md`, `.claude/`, `tools/` değişmemiş |
| K11 | ✔ | `file`: 8 dosya `ASCII text, with CRLF line terminators`; `git diff --check` rc=0 |
| K12 | ✔ | `printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(\|SByte\|DByte` `ActionExecutor.*`'de boş |
| K13 | ✔ | `CheckMoveStep` 2, diğer altı guard 1; `EmitFairnessReject` tipleri `Move`(2)/`Attack`/`Cast`/`Potion`/`State`/`TargetHp`/`Regene` + yeni `PartyInvite`/`PartyAccept`; önceki 33 test geçiyor; çalışma zamanında F4-01..F4-07 komutları (aşağıda 7) önceki çıktıyı verdi |
| K14 | ✘ | Çalışma zamanı §7 senaryoları 1–7 davranış olarak geçti (aşağıda), ancak `PartyInvite` `ACTION_SUBMIT` satırı ortak `mode` alanını yinelenen anahtarla gölgeliyor (bulgu 1) |

- **Çalışma zamanı (K14, Release, AIServer bağlı, `TELEMETRY=decisions`, zone 71; `BotWP_K`/`BotMF_K`/`BotPHD_K`/`BotWG_K`/`BotWP_E`):**
  1. Mutlu yol: `pinvite BotWP_K BotMF_K` → `invited BotMF_K (created)`; JSONL `ACTION_SUBMIT` (`PartyInvite`, `target:2985`) → `ACTION_RESULT` (`ok:true`, `reason:"created"`, `latency_us:76`). Aynı dosyada hemen `paccept BotMF_K` → `refused (accept_wait)`, `FAIRNESS_REJECT` `CLI-15 accept_wait value:1.00 limit:1000.00`, `PartyAccept` `ACTION_SUBMIT` yok; 2,5 sn sonra `paccept BotMF_K` → `joined party of #2984`, `ACTION_RESULT` `ok:true`, `reason:"joined"`, `inviter:2984`, `latency_us:80`; tekrar `paccept` → `refused (no_invite)`, JSONL'de olay yok. **`[A]`(c) doğrulandı:** sunucunun `WIZ_STATE_CHANGE` (lider bayrağı) ve `WIZ_PARTY` (`PARTY_PERMIT`, kendi `PARTY_INSERT`) cevapları botun alıcısına geliyor.
  2. Insert: `pinvite BotWP_K BotPHD_K` → `invited BotPHD_K (sent)` (`mode:insert`, `latency_us:6`); 2,5 sn sonra `paccept BotPHD_K` → `joined party of #2984`. Üç botluk party kuruldu; `GameServer.log` 32→32 satır (yeni hata yok). Lider `BotWP_K` sonradan `sit`/`stand`/`target`/`pot` komutlarını çalıştırdı.
  3. Guard/ret: `pinvite BotMF_K BotWP_K` → `refused (not_leader)` (`CLI-15`, `value:0 limit:0`); `BotWG_K` için aynı dosyada iki `pinvite` → `sent` + `refused (invite_gap)` (`value:0.00 limit:1000.00`), `paccept BotWG_K` → `joined`; `pinvite BotWP_K BotWP_E` → `refused (out_of_view)` (`value:13.00 limit:1.00`); zaten üye `BotMF_K` için `pinvite` → `failed (refused_target)`, `ACTION_RESULT` `ok:false`, `code:-1`. Sunucu reddi `-2`/`-3` sınanmadı.
  4. CLI-11: altı ardışık `target` + `pinvite BotWP_K BotWG_K` → `refused (rate)`, `FAIRNESS_REJECT` `CLI-11 rate value:6.00 limit:6.00`. Ölü bot: `BotWG_K` `despawn` (party üyesi) sonrası bot satırı `Hp=0` ile hedefli `UPDATE`'lenip `spawn` (`hp 0/5650`): `pinvite BotWG_K BotWP_K` ve `paccept BotWG_K` → `refused (dead)`, JSONL'de olay 0 (öldürerek üretilmedi; F4-07 ile aynı kısayol).
  5. `despawn BotMF_K` sonrası `pinvite BotWP_K BotMF_K` → `target BotMF_K not in game (phase despawned)`; `pinvite Ghost BotWP_K` → `unknown or not spawned bot '?'`; argümansız, `pinvite BotWP_K`, `paccept`, `paccept BotWP_K BotMF_K` → kullanım satırı (tek satır); `paccept Ghost` → `unknown or not spawned bot '?'`; `pinvite BotWP_K BotWP_K` → `refused (bad_target)`. `RESPAWN_CYCLES=2` reddi yeniden sınanmadı (bu planda dokunulmadı).
  6. Party üyesi (`BotWG_K`, `BotMF_K`) ve lider (`BotWP_K`) `despawn` edildi: sunucu çökmedi, `GameServer.log` değişmedi (32 satır), kalan botlar `in_game` kaldı (`list`), AIServer bağlı kaldı. Sonuç: `[A]`(f) bot oturumunda sorunsuz.
  7. Gerilemesiz: `sit`/`stand`, `target` (`observed hp 3491/3491`), `pot ... 2` (`effected`), `regene` (`not_dead`), `move` (`arrived`) önceki çıktıyı verdi; `PERF_SAMPLE` `tick_p95_us` 83–132 (≤ 1 ms). `attack`/`cast` yeniden çalıştırılmadı (bu planda kod yolu değişmedi; party botlarında karşılıklı El Morad hedefi 750 m uzakta). `TELEMETRY=summary`: `pinvite` → `created`, `paccept` → `joined`, ikinci `paccept` → `no_invite`; JSONL'de `ACTION_*`/`FAIRNESS_*` 0, `PERF_SAMPLE` 6. `ENABLED=0`: komut dosyası tüketilmedi, bot logu 0 satır, `Logs/bots` oluşmadı.
- Bulgular (önem sırasına göre):
  1. **[Orta] `ActionExecutor.cpp:1834`:** `PartyInvite` `ACTION_SUBMIT` alanlarına `"mode":"create"|"insert"` yazılıyor, ama telemetri satırının ortak alanı zaten `"mode":"live"` (`Telemetry.cpp:549`; `docs/16` §ortak alanlar: `mode` = `train/eval/live/debug`). Satır yinelenen anahtar içeriyor: `..."mode":"live","decision_id":34,"type":"PartyInvite","target":2985,"mode":"create"`. Python `json` ve `jq` son değeri alır → `mode` `create`/`insert` okunur; ortak alan bozulur, `tools/bot-telemetry-report.py:147` gibi okuyucular yanlış mod görebilir. Kaynak planın kendi metnidir (§5.4 "mode":"create"|"insert"); uygulayıcı planı birebir izledi, ama düzeltilmeli: alan adı `invite_mode` olur (değerler aynı). Ortak alan adlarını başka bir aksiyon alanı yeniden kullanmamalı; önceki dilimlerde (`regene_type`, `kind`) bu yapılmamıştı.
  2. **[Not] Sınanmadı:** sunucu reddi `-2` (seviye) ve `-3` (ulus/zone); `RESPAWN_CYCLES=2` ile `pinvite` reddi; öldürülerek (yalnızca `Hp=0` açılışıyla) ölü bot. Kod yolları `-1` ile aynı eşleme tablosunu kullanır.
  3. **[Not]** `RejectPartyInvite`/`RejectPartyAccept` `decision_id` tüketir (önceki dilimlerle aynı); `invite_gap` aynı dosyada ardışık `pinvite`'ta `value:0.00` (aynı `Tick()`, beklenen).
  4. **[Not]** `PartyInviteTarget.id`, `target->m_pUser->GetSocketID()`'den geliyor ve `bad_target` self kontrolü `GetID()` ile yapılıyor (aynı değer; sınandı: `pinvite BotWP_K BotWP_K` → `bad_target`).
- Temizlik: `GameServer.ini` doğrulama öncesi bulunduğu hâle (proje sahibinin yarım kalan test oturumundan `[BOT] ENABLED=1, TELEMETRY=decisions`; md5 `265a8e1c35ea12df46f6d006fe894d9b`) geri yüklendi; `BotCommands.txt` kaldırıldı; eski `Logs/bots` içeriği `bots_old_f408pre`/`bots_old_f408a`/`bots_old_f408b` altına taşındı; sunucular kapatıldı (`0/3 hazır`). Beş bot satırı (`BotWP_K`, `BotMF_K`, `BotPHD_K`, `BotWG_K`, `BotWP_E`) hedefli `UPDATE` ile `Hp=Mp=32000`, `PX=127400`, `PZ=89000`, `Loyalty=1000` geri yazıldı (5 satır; kişisel veri tablosu okunmadı). Not: `BotWP_K` satırının proje sahibinin testinden kalan konumu (1371.9, 1093.9) bu geri yazımla standart başlangıç konumuna döndü; ekipman/dayanıklılık alanlarına dokunulmadı. `pid=4336` `GameServer.exe` (yol okunamadı) doğrulamadan önce de vardı, dokunulmadı.
- **Düzeltme talimatı** (DeepSeek'e aynen verilecek)

```
plans/F4-08-aksiyon-yurutucu-party.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
1. GameServer/Bot/ActionExecutor.cpp:1834 (RequestPartyInvite, ACTION_SUBMIT alanları): "\"mode\":\"" ifadesini "\"invite_mode\":\"" yap. Değerler aynı kalsın ("create" / "insert"); telemetri satırının ortak "mode" alanı (Telemetry.cpp) ile çakışma biter. Başka satıra, başka fonksiyona ve ACTION_RESULT alanlarına dokunma.
2. ./tools/build.sh Release ve ./tools/build.sh Debug hatasız, ActionExecutor.cpp için uyarı yok. ./tools/run-tests.sh Release ve Debug: 36 tests, 0 failed. grep -n '\\"mode\\"' GameServer/Bot/ActionExecutor.cpp çıktısı boş olmalı. Çıktıları raporuna yaz.
3. Başka dosyaya dokunma (docs/**, BotCombat.h, testler, GameServer/** içindeki diğer dosyalar dahil). Sunucuyu çalıştırma. Durum satırını UYGULANDI yap.
```

### Tur 2 — 2026-10-02

- Karar: **DOĞRULANDI**
- İncelenen: `bot/F4-08` @ `bb80817` (Tur 2 düzeltme commit'i `40b8304`; taban `gece/2026-10-02`; gece modu, `AUTO_LOOP=1`: birleştirme/push yapılmadı). Çalışma ağacı temizdi; sunucular kapalıydı (`0/3 hazır`).
- Kriter sonuçları:

| # | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `ActionExecutor.cpp` `touch` + `tools/build.sh Release` rc=0, `ActionExecutor.cpp` yeniden derlendi; log'daki 2 uyarı eski dosyalarda (Bot/, BotCore, Tests için `warning` yok) |
| K2 | ✔ | `tools/build.sh Debug` rc=0, Bot/BotCore/Tests için uyarı yok |
| K3 | ✔ | `tools/run-tests.sh Release` ve `Debug` rc=0, `36 tests, 0 failed`; üç yeni test `[ OK ]` |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` boş (Tur 1'den beri değişmedi) |
| K5 | ✔ | `WIZ_PARTY` yalnızca `ActionExecutor.cpp:1840`, `:1978`, `BotSession.cpp:104`; doğrudan party çağrısı deseni boş |
| K6 | ✔ | Tur 1 kod okuması geçerli (Tur 2 farkı tek JSON anahtarı); guard ve `HandlePacket` sırası değişmedi |
| K7 | ✔ | Tur 2 farkı kodu etkilemiyor; Tur 1 bulguları geçerli |
| K8 | ✔ | `BotSession.cpp` silinen tek satır başlatıcı listesindeki `m_regeneEcho(0)` satırı |
| K9 | ✔ | `BotManager.cpp` silinen tek satır `unknown command` mesajı |
| K10 | ✔ | Dört `.vcxproj`/`.filters` farkı 0; kod farkı yalnızca §4'teki 8 dosya |
| K11 | ✔ | `file`: `Bot/*.cpp,*.h`, `BotCombat.h`, `CombatTests.cpp` ASCII+CRLF; `git diff --check` yalnızca `docs/STATUS.md` (Claude'un kendi doküman satırlarında markdown sonda boşluk) gösteriyor, kod dosyalarında boş |
| K12 | ✔ | `printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(\|SByte\|DByte` `ActionExecutor.*`'de boş |
| K13 | ✔ | Tur 2 farkı yalnızca bir alan adı; önceki 33 test geçiyor (`36 tests, 0 failed`) |
| K14 | ✔ | Çalışma zamanı §7 senaryoları 1–7 Tur 1'de geçti (bkz. Tur 1); tek bulgu (yinelenen `mode` anahtarı) `ActionExecutor.cpp:1834`'te `"invite_mode"` olarak düzeltildi, `grep -n '\"mode\"' ActionExecutor.cpp` boş |

- **Tur 1 düzeltme talimatı denetimi:** (1) `git show 40b8304`: tek satır, `"mode"` → `"invite_mode"`, değerler (`create`/`insert`) aynı; (2) derleme/test çıktıları rapordaki iddialarla uyuşuyor (kendim yeniden çalıştırdım); (3) başka dosyaya dokunulmadı (`40b8304` yalnızca `ActionExecutor.cpp`; `bb80817` yalnızca plan dosyası).
- Bulgular: yok. Notlar:
  1. **[Not]** Çalışma zamanında `invite_mode` anahtarı Tur 2'de yeniden sınanmadı: değişiklik tek bir string sabiti, derleme ve kod okuması (`ActionExecutor.cpp:1832-1836`) yeterli bulundu. Telemetri okuyucuları (`tools/bot-telemetry-report.py`) `mode` alanını okur, `invite_mode` kullanmaz; etki yok.
  2. **[Not]** Sunucu reddi `-2` (seviye) ve `-3` (ulus/zone), `RESPAWN_CYCLES=2` ile `pinvite` reddi ve öldürerek ölü bot sınanmadı (Tur 1'deki notla aynı); kod yolları `-1` ile aynı eşleme tablosunu kullanır.
  3. **[Not]** İnsan testleri bekliyor: `T-ARCH-13` (gerçek istemcide party paneli) ve `T-PARTY-01` (CLI-15 alt sınır ölçümü) `docs/STATUS.md` "Proje sahibi testleri".
- Birleştirme: gece modu, döngü betiği `gece/2026-10-02`'ye birleştirir; bu oturumda birleştirme/push yapılmadı.
