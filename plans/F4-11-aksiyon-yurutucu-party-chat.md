# F4-11: `ActionExecutor` party chat dilimi — `ChatParty` ve `BotFairnessGuard` CLI-18 kuralları

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-02, gece/2026-10-02, merge `ca677c0`) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-11` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-10 (`RequestPartyManage` iskeleti, `m_actionWindow`, `OnPacket()` kayıt kalıbı) — `KAPANDI` (merge `3e0e985`); F4-08/F4-09 (`isInParty()` ön koşulu, `m_partyInviteEcho` "bekleyen davet" kaydı) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-18 (`docs/03` §13, bu planla birlikte eklendi), CLI-11, MEC-CHT-01, MEC-CHT-02, MEC-CHT-03, MET-CHAT-01 altyapısı, MET-ACT-02, MET-FAIR-01 altyapısı, AC-LRN-03 |
| Tahmini büyüklük | M (8 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun on dördüncü gerçek aksiyonu: party üyesi bir bot **gerçek `WIZ_CHAT` paketiyle** party sohbetine bir mesaj yazar (`ChatParty`: `u8 PARTY_CHAT` + `u16` uzunluklu metin). Paket `CUser::HandlePacket()` üzerinden gider; sunucu mesajı `Send_PartyMember` ile **göndereni de içeren tüm üyelere** yayınlar. Sunucunun tek denetimi: susturulmuş değil, hapiste değil, metin 1–128 bayt, gönderen party'de (MEC-CHT-02; **oran sınırı, tekrar denetimi, karakter denetimi yoktur**). Bu yüzden paket sunucuya gitmeden önce `BotFairnessGuard` kurallarından geçer (CLI-18): metin geçerli olmalı (`bad_text`: 1–128 bayt, yazdırılabilir ASCII, `+` ile başlamaz), iki mesaj arası ≥ 4 sn (`chat_gap`), aynı metin 8 sn içinde tekrar yok (`chat_dup`), dakikada ≤ 6 mesaj (`chat_minute`) ve CLI-11 aksiyon hızı. Sınırlar `docs/09` §12 ve `docs/07` `P-PRI-CHAT-RATE` tasarım değerleridir (insan ölçümü değil). Karar katmanı yoktur: aksiyonu `/bot pchat <bot> <metin>` komutu tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün on birinci dilimidir (ADR-0017 Ek F4-11; hareket → saldırı → cast → pot → duruş → hedef HP → `Regene` → `Party` kurulumu → `Party` ret/ayrılma → `Party` devir/atma → **`Chat` (party chat)** → `Perception` → betikli test dizileri).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-11)" (bu planla birlikte yazıldı): paket düzeni, guard kuralları, sonuç eşlemesi, kapsam.
- `docs/03` §11 **MEC-CHT-01/02/03**, §13 **CLI-18** (bu planla birlikte eklendi), **CLI-11**; `docs/09` §12 (party chat protokolü, sınırlar); `docs/16` §3.2 `CHAT_SENT`.
- `plans/F4-10-aksiyon-yurutucu-party-devir-atma.md` §5.2–§5.5: bu planın kalıpladığı iskelet. **Yazılı planı değil, birleşmiş kodu esas al** (`ActionExecutor.cpp` `RequestPartyLeave` ve `RequestPartyManage`, `BotManager.cpp` `CommandPartyLeave`).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `3e0e985` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:328-330` — `HandlePacket` `case WIZ_CHAT`: `Chat(pkt)`.
  - `GameServer/ChatHandler.cpp:92-278` — `CUser::Chat(Packet &)`. Dosya UTF-8 **BOM'lu** + CRLF; **bu planda dokunulmaz**. `:104` susturulmuş (`isMuted()`) veya hapiste (`ZONE_PRISON`, GM değil) → sessizce döner; `:107-108` `pkt >> chatstr` (uzunluk **`u16`**, `ByteBuffer` varsayılanı `DByte`) ve `chatstr.empty() || size() > 128` → döner; `:112-117` yalnızca **GM** ise `ProcessChatCommand` (`+` önekli komutlar, MEC-CHT-03; bot GM değildir ama guard `+` ile başlayan metni yine de reddeder); `:161` `ChatPacket::Construct(&result, bOutType, ..., bNation, sessID)` (`sessID = GetSocketID()`); `:190-195` `case PARTY_CHAT`: `isInParty()` ise `g_pMain->Send_PartyMember(GetPartyID(), &result)` (party'de değilse **hiçbir şey olmaz**, cevap yok); `:264-275` chat log dosyasına satır yazılır (`PARTY_CHAT`).
  - `GameServer/ChatHandler.h:8-21` — `ChatPacket::Construct`: yük düzeni `u8 tip, u8 ulus, i16 gönderen sid, u8-uzunluklu ad (SByte), u16-uzunluklu mesaj (DByte)`. Alıcının `Packet`'inde okuma konumları (opcode dışarıda): tip `@0`, ulus `@1`, sid `@2`, ad uzunluğu `@4`, ad `@5..`, mesaj uzunluğu `@(5+adUzunluğu)` (`u16`), mesaj `@(7+adUzunluğu)..`.
  - `shared/packets.h:162-180` — `enum ChatType`; `PARTY_CHAT = 3` (`:166`). Sabit **adıyla** kullanılır.
  - `shared/ByteBuffer.h:11-12` varsayılan `m_doubleByte = true`; `:65-77` `operator<<(const std::string &)` uzunluk (DByte: `u16`) + baytlar (`strlen` ile: gömülü NUL metni keser, guard zaten reddeder); `:79-93` `operator>>`. `shared/Packet.h` `Packet(uint8 opcode, uint8 subOpcode)` kurucusu (ilk bayt = alt tip). Bot paketi: `Packet pkt(WIZ_CHAT, uint8(PARTY_CHAT)); pkt << text;`.
  - `GameServer/GameServerDlg.cpp:1080-1093` — `Send_PartyMember(party, pkt)`: party üyelerinin hepsine (gönderen dahil) `Send()`; bot alıcısına gider (`BotSession::OnPacket`).
  - `GameServer/Bot/BotSession.cpp:36-141` — `OnPacket()`; `WIZ_PARTY` bloğu `:109-139` (yeni `WIZ_CHAT` bloğu bunun **hemen ardına**, fonksiyon kapanışından önce eklenir). `:6-26` başlatıcı listesi, `:143-204` `ResetForRespawn()`. `GameServer/Bot/BotSession.h:60-133` alanlar.
  - `GameServer/Bot/ActionExecutor.cpp:30-67` `NextDecisionId`/`EmitFairnessReject` (kalıp), `:2075-2085` `RejectPartyDecline`'in kapanışı ve `:2171-2287` `RequestPartyLeave` (en yakın kalıp: `NOTHING`/`REFUSED`, `inWindow`, `HandlePacket` gecikmesi, sonuç eşleme), `:2475-2485` dosya sonu (`RequestPartyPromote`/`RequestPartyKick` sarmalayıcıları). `GameServer/Bot/ActionExecutor.h:100-129` `PartyOutcome`/`PartyMemberTarget`, `:133` `class ActionExecutor`, `:260-262` son bildirim ve kapanış.
  - `GameServer/Bot/Telemetry.h:96` `Telemetry::EscapeJson` (herkese açık, statik; `BotManager.cpp:973` kullanır), `:76` `Emit(level, ev, bot, name, fields, droppable)`.
  - `GameServer/Bot/BotManager.cpp:591-657` `ExecuteCommand` (fiil = ilk sözcük, `args` = geri kalanı `Trim`'li; `:645-654` `pleave`..`unknown command` listesi), `:2035-2078` `CommandPartyLeave` (tek botlu komut kalıbı), `:2092-2177` `CommandPartyManage` (bunun hemen ardından yeni komut eklenir; sonra `:2179` `ParseSpawnList`). `GameServer/Bot/BotManager.h:86` `CommandPartyManage` bildirimi. Komut dosyası satır tamponu **256 bayt** (`fgets`).
  - `BotCore/BotCombat.h:14-15` `kMaxActionsPerWindow`/`kActionWindowMs`, `:81-120` `ActionRateWindow` (yeni pencere sınıfının kalıbı), `:586-606` `CheckPartyManage` ve namespace kapanışı.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** `kChatMaxLen`, `kChatGapMs`, `kChatDupMs`, `kChatPerMinute`, `kChatMinuteMs`, `IsValidChatText`, `ChatTextHash`, `ChatRateWindow`, `ChatCheck`, `ChatVerdict`, `CheckChat`. Birim testleri (`CombatTests.cpp`).
2. **`ActionExecutor::RequestChatParty(s, text, now)`** (tek seferlik aksiyon): ön koşullar → guard → `WIZ_CHAT` paketi → `HandlePacket` → yayınlanan cevaptan sonuç eşleme → telemetri. Yeni `ChatOutcome` yapısı.
3. **Oturum durumu (`BotSession`):** son mesaj zamanı/özeti, dakikalık pencere, `OnPacket()`'in yazdığı iki alan (`m_chatEcho`, `m_chatEchoHash`). `OnPacket()`'e **yalnızca ekleme** (yeni `WIZ_CHAT` bloğu); `ResetForRespawn()` sıfırlar.
4. **Komut (`BotManager`):** `pchat <bot> <metin>` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten); `CommandPartyChat`.
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT` / `ACTION_RESULT` (`"type":"ChatParty"`), `FAIRNESS_REJECT` (aynı tip), başarıda `CHAT_SENT` (`docs/16` §3.2; kanal + metin).

**Kapsam dışı (yapılmayacak)**

- **Ne zaman ne yazılacağı** (`HEDEF:`/`PATLAT:` çağrıları, `docs/09` §12 mesaj şablonları, `TeamBlackboard`, karar katmanı): yok. Komut guard dışında hiçbir koşula bakmaz.
- **`PARTY_CHAT` dışındaki kanallar:** genel (`GENERAL_CHAT`), fısıltı, bağır (`SHOUT_CHAT`, MP/altın harcar), clan, ittifak, komuta, pazarcı, `SEEKING_PARTY_CHAT`: yok. Bot sohbet kanalı seçmez.
- **ASCII dışı karakterler:** yok (`bad_text`). Türkçe karakterlerin istemcide görünmesi Q-16/T-PTY-09'dur (F7); o zamana kadar guard yalnızca yazdırılabilir ASCII (0x20–0x7E) kabul eder.
- **Gelen sohbeti ayrıştırmak/tepki vermek** (`HEDEF:` ipuçları, `P-TEAM-HUMAN-CALLS`): yok. `OnPacket()` gelen sohbeti yalnızca **kendi isteğinin sonucu** için kaydeder; başkalarının mesajları sadece kaydı üzerine yazar (§5.3 notu).
- `ChatHandler.cpp` (sunucu chat kodu; `+bot` yardım metni dahil, KI-012: Claude doğrulamada günceller) **değişmez**. Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya yok; **`GameServer` projesine dosya eklenmez**. `Telemetry.*`, `ScenarioRunner.*` değişmez; `list` satırı değişmez.
- Dokümanları (`docs/03`, `docs/13`, `docs/16`, `docs/15`, `docs/KNOWN_ISSUES.md`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | Yalnızca ekleme (§5.2); mevcut içerik ve `#include`'lar değişmez |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | Yalnızca ekleme: dört yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | Yeni `ChatOutcome`; bir yeni statik fonksiyon |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | Yalnızca dosya sonuna ekleme (§5.4); mevcut kod değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca altı alan (§5.3) |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi, `ResetForRespawn()` ve `OnPacket()` sonuna **ekleme** |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandPartyChat` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, komut, `unknown command` listesi |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (Yeni dosya açılmaz: guard `BotCombat.h`'ye eklenir, böylece `BotCore*.vcxproj` değişmez.)

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-11 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotCombat.h` (saf mantık, mevcut `namespace BotCore` içine ekleme)

Mevcut kodun biçimini (tab, Allman, `inline`, İngilizce kısa yorum, ölçülmemiş/tasarım değeri etiketi) koru. Yeni `#include` gerekmez (`<string>` **eklenmez**: metin `const char *` + uzunlukla verilir). `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez** (yorumlarda da `shared/` dizgisi **yazma**: K4 grep'i takılır). `CheckPartyManage`'den **sonra**, `namespace`'in kapanışından önce ekle. **`std::min`/`std::max` kullanma**:

```cpp
	// --- party chat slice (ADR-0017 Ek F4-11) ---

	constexpr uint32_t kChatMaxLen    = 128;      // docs/03 MEC-CHT-02: the server drops an empty or > 128 byte message
	constexpr uint32_t kChatGapMs     = 4000;     // docs/03 CLI-18 / docs/09 section 12: 1 message per 4 s per bot (design limit)
	constexpr uint32_t kChatDupMs     = 8000;     // same text again only after 8 s (design limit)
	constexpr int      kChatPerMinute = 6;        // at most 6 messages per minute per bot (design limit)
	constexpr uint32_t kChatMinuteMs  = 60000;

	// A message the guard lets through: 1..kChatMaxLen bytes of printable ASCII (0x20..0x7E), not starting with '+'
	// (the GM command prefix, MEC-CHT-03). Non-ASCII text stays closed until the client character set is measured (Q-16).
	inline bool IsValidChatText(const char * text, uint32_t len);

	// FNV-1a (32 bit) of the message bytes; the sender compares it with the broadcast it receives back.
	inline uint32_t ChatTextHash(const char * text, uint32_t len);

	// Sliding window over the last kChatPerMinute chat timestamps (same shape as ActionRateWindow). Time in ms, any epoch.
	class ChatRateWindow
	{
	public:
		ChatRateWindow() { Clear(); }
		int CountInWindow(uint64_t nowMs) const;   // entries with nowMs - t < kChatMinuteMs (t <= nowMs)
		void Record(uint64_t nowMs);               // overwrites the oldest entry once kChatPerMinute are stored
		void Clear();
	private:
		uint64_t m_times[kChatPerMinute];
		int m_count;
		int m_next;
	};

	struct ChatCheck
	{
		bool textOk;              // IsValidChatText(...) for this message
		bool hasLast;             // a chat message was sent earlier in this spawn
		uint32_t sinceLastMs;     // since that message
		bool sameAsLast;          // ChatTextHash of this message == the hash of that message (only meaningful when hasLast)
		int chatsInMinute;        // ChatRateWindow::CountInWindow(now)
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum ChatVerdict
	{
		CHAT_OK = 0,
		CHAT_REJECT_TEXT = 1,     // CLI-18 (empty, > 128 bytes, non-printable / non-ASCII byte, or leading '+')
		CHAT_REJECT_GAP = 2,      // CLI-18 (second message before kChatGapMs)
		CHAT_REJECT_DUP = 3,      // CLI-18 (the same text again before kChatDupMs)
		CHAT_REJECT_MINUTE = 4,   // CLI-18 (kChatPerMinute messages already in the last kChatMinuteMs)
		CHAT_REJECT_RATE = 5      // CLI-11
	};

	// Guard rule for a party chat message. The caller has already checked that the bot is in a game, alive, in a party
	// with no invitation pending. Order: text, gap (only when a previous message is known), dup (same), minute, rate.
	inline ChatVerdict CheckChat(const ChatCheck & c);
```

Gövdeler (`inline`, aynı dosyada, bildirimlerin altında):

- `IsValidChatText`: `text == nullptr || len < 1 || len > kChatMaxLen` → `false`; `text[0] == '+'` → `false`; her bayt `(unsigned char)text[i]` için `< 0x20 || > 0x7E` → `false`; aksi halde `true`.
- `ChatTextHash`: `uint32_t h = 2166136261u;` her bayt için `h ^= (unsigned char)text[i]; h *= 16777619u;`; `return h;`. Boş girdi (`len == 0`) → `2166136261u` (0x811C9DC5).
- `ChatRateWindow::CountInWindow/Record/Clear`: `ActionRateWindow` ile aynı mantık; dizi boyutu `kChatPerMinute`, pencere `kChatMinuteMs`.
- `CheckChat`: `!c.textOk` → `CHAT_REJECT_TEXT`; `c.hasLast && c.sinceLastMs < kChatGapMs` → `CHAT_REJECT_GAP`; `c.hasLast && c.sameAsLast && c.sinceLastMs < kChatDupMs` → `CHAT_REJECT_DUP`; `c.chatsInMinute >= kChatPerMinute` → `CHAT_REJECT_MINUTE`; `c.actionsInWindow >= kMaxActionsPerWindow` → `CHAT_REJECT_RATE`; aksi halde `CHAT_OK`.

**`Tests/BotCoreTests/CombatTests.cpp` (ekleme, mevcut makro stili `CHECK_EQ((int)..., (int)...)`; mantıksal değerler de `CHECK_EQ((int)..., (int)...)` ile):** dört yeni `TEST_CASE` (dosyanın sonuna, `Combat_PartyManageCheck_Boundaries`'ten sonra):

- `Combat_ChatText_Validity`: `IsValidChatText("TARGET: BotWP_E (Malice)", n)` → 1; boş (`len 0`) → 0; 128 karakterlik `'a'` dizisi → 1; 129 karakterlik → 0; `"+bot list"` → 0; `"a+b"` → 1 (yalnızca ilk karakter); `"a\tb"` → 0; `"a\x7f" "b"` → 0; `"a\xc4" "b"` (ASCII dışı bayt) → 0; `nullptr` → 0. `ChatTextHash("", 0) == 0x811C9DC5u`, `ChatTextHash("a", 1) == 0xE40C292Cu`, `ChatTextHash("a", 1) != ChatTextHash("b", 1)`, aynı metin iki kez aynı özet.
- `Combat_ChatRateWindow`: boş pencere `CountInWindow(0) == 0`; `Record` 0, 4000, 8000, 12000, 16000, 20000 → `CountInWindow(20000) == 6`; `CountInWindow(59999) == 6`; `CountInWindow(60000) == 5` (0 ms'deki girdi `60000 - 0 < 60000` değil); `CountInWindow(64000) == 4` (0 ve 4000 dışarıda), `CountInWindow(80000) == 0` (20000'deki girdi `80000 - 20000 = 60000` ile dışarıda); yedinci `Record(24000)` en eski girdinin (0) üstüne yazar → `CountInWindow(24000) == 6`; `Clear()` sonrası `CountInWindow(24000) == 0`.
- `Combat_ChatCheck_Order`: tüm ihlaller (`textOk = false, hasLast = true, sinceLastMs = 0, sameAsLast = true, chatsInMinute = 6, actionsInWindow = 6`) → `CHAT_REJECT_TEXT`; `textOk = true` → `..._GAP`; `sinceLastMs = 4000` → `..._DUP`; `sameAsLast = false` → `..._MINUTE`; `chatsInMinute = 5` → `..._RATE`; `actionsInWindow = 5` → `CHAT_OK`; `hasLast = false, sinceLastMs = 0, sameAsLast = true, chatsInMinute = 0, actionsInWindow = 0` → `CHAT_OK` (önceki mesaj yoksa boşluk ve tekrar beklenmez).
- `Combat_ChatCheck_Boundaries`: `textOk = true, hasLast = true, sameAsLast = false, chatsInMinute = 0, actionsInWindow = 0`: `sinceLastMs = 3999` → `..._GAP`, `4000` → `CHAT_OK`; `sameAsLast = true`: `7999` → `..._DUP`, `8000` → `CHAT_OK`; `chatsInMinute = 5` → `CHAT_OK`, `6` → `..._MINUTE`; `actionsInWindow = 5` → `CHAT_OK`, `6` → `..._RATE`; sabitler `kChatMaxLen == 128`, `kChatGapMs == 4000`, `kChatDupMs == 8000`, `kChatPerMinute == 6`, `kChatMinuteMs == 60000`.

Beklenen toplam test sayısı: 41 + 4 = **45**.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'de `m_partyManageLast`'in altına (aynı yorum/hizalama biçimi):

```cpp
	bool m_chatHasLast;                                    // IOCP thread only: m_chatLast and m_chatLastHash are valid for this spawn
	std::chrono::steady_clock::time_point m_chatLast;      // IOCP thread only: when the last party chat message went out
	uint32 m_chatLastHash;                                 // IOCP thread only: BotCore::ChatTextHash of that message
	BotCore::ChatRateWindow m_chatWindow;                  // IOCP thread only: CLI-18 per-minute window
```

ve atomik alanların sonuna (`m_partyLeaveEcho`'nun altına):

```cpp
	std::atomic<uint32> m_chatEchoHash;                    // written by OnPacket() BEFORE m_chatEcho: BotCore::ChatTextHash of the last WIZ_CHAT message received (0 when longer than kChatMaxLen)
	std::atomic<uint64> m_chatEcho;                        // written by OnPacket(): valid bit | chat type << 32 | uint16 sender sid of the last WIZ_CHAT received
```

`BotSession.cpp`:

- Başlatıcı listesi: `m_partyManageHasLast(false)`'tan sonra `m_chatHasLast(false), m_chatLastHash(0)`; `m_partyLeaveEcho(0)`'dan sonra `m_chatEchoHash(0), m_chatEcho(0)` (üye bildirim sırasıyla aynı; derleyici sıra uyarısı vermemeli).
- `ResetForRespawn()`: `m_partyManageHasLast = false;`'in yanına `m_chatHasLast = false; m_chatLastHash = 0; m_chatWindow.Clear();`; `m_partyLeaveEcho = 0;`'in yanına `m_chatEchoHash = 0; m_chatEcho = 0;`.
- **`OnPacket()` sonuna ekleme** (`WIZ_PARTY` bloğunun hemen ardı; mevcut bloklara **dokunulmaz**):

```cpp
	// Chat broadcast (ChatHandler.cpp:161, ChatPacket::Construct): u8 type, u8 nation, i16 sender sid, u8-length name,
	// u16-length message. Every chat packet the bot receives is recorded (own party chat echo and other players' chat
	// alike); ActionExecutor::RequestChatParty clears the record before its request and matches type, sender and the
	// message hash afterwards, on the same thread. The hash word is written first so a reader that sees the valid bit
	// also sees the hash.
	if (opcode == WIZ_CHAT && pkt.size() >= 7)
	{
		uint8 type = pkt.read<uint8>(0);
		uint16 sid = pkt.read<uint16>(2);
		uint8 nameLen = pkt.read<uint8>(4);
		size_t msgLenPos = 5 + (size_t)nameLen;
		uint32 hash = 0;
		if (pkt.size() >= msgLenPos + 2)
		{
			uint16 msgLen = pkt.read<uint16>(msgLenPos);
			if (msgLen <= BotCore::kChatMaxLen && pkt.size() >= msgLenPos + 2 + (size_t)msgLen)
			{
				char text[BotCore::kChatMaxLen];
				for (uint16 i = 0; i < msgLen; i++)
					text[i] = (char)pkt.read<uint8>(msgLenPos + 2 + i);
				hash = BotCore::ChatTextHash(text, msgLen);
			}
		}
		m_chatEchoHash = hash;
		m_chatEcho = (1ull << 63) | (uint64(type) << 32) | uint64(sid);
	}
```

(`pkt.size()` ve `pkt.read<T>(pos)` kullanımı `WIZ_PARTY` bloğundaki gibidir. Başka bir oyuncunun sohbeti kaydın üzerine yazabilir; bu durumda aksiyon `no_result` verir, yalnızca yük altında ve nadiren: bilinen sınır, kabul edilmiş.)

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

**Başlık (`ActionExecutor.h`):** `PartyMemberTarget`'ın altına:

```cpp
// Result of ActionExecutor::RequestChatParty (ADR-0017 Ek F4-11).
struct ChatOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed. SENT: "sent" (the bot's own party chat broadcast came back).
	                       // FAILED: "no_result" (no matching broadcast: muted, jailed, no longer in a party...).
	                       // REFUSED: "not_in_game", "dead", "invite_pending", "not_in_party", or a guard verdict
	                       // ("bad_text", "chat_gap", "chat_dup", "chat_minute", "rate")
	int length;            // message length in bytes (0 when refused before the text was looked at)
};
```

`class ActionExecutor` içine (`RequestPartyKick` bildiriminin altına, aynı yorum kalıbıyla):

```cpp
	// One-shot party chat message (WIZ_CHAT: PARTY_CHAT + the text through CUser::HandlePacket()) after the guard (CLI-18:
	// printable ASCII 1..128 bytes not starting with '+', >= 4 s since the last message, not the same text within 8 s,
	// <= 6 messages per minute; CLI-11). Preconditions without an event: REFUSED "not_in_game", "dead", "invite_pending"
	// (an invitation must be accepted or declined first), "not_in_party". Result only from the published reply: the server
	// broadcasts the message to every party member including the sender -> the bot's own WIZ_CHAT (type PARTY_CHAT,
	// sender == its id, same text hash) -> SENT "sent"; none -> FAILED "no_result".
	static ChatOutcome RequestChatParty(BotSession * s, const std::string & text,
		std::chrono::steady_clock::time_point now);
```

**`ActionExecutor.cpp`:** yalnızca **dosya sonuna** ekleme (bölüm başlığı `// --- party chat slice (ADR-0017 Ek F4-11) ---`); mevcut koda **dokunulmaz**:

- **`static ChatOutcome RejectChat(BotSession * s, CUser * user, BotCore::ChatVerdict verdict, const BotCore::ChatCheck & c, uint32 textLen)`** (`RejectPartyManage` kalıbı; `type` = `"ChatParty"`): `TEXT` → `rule "CLI-18", reason "bad_text", value = textLen, limit = kChatMaxLen`; `GAP` → `"CLI-18", "chat_gap", value = sinceLastMs, limit = kChatGapMs`; `DUP` → `"CLI-18", "chat_dup", value = sinceLastMs, limit = kChatDupMs`; `MINUTE` → `"CLI-18", "chat_minute", value = chatsInMinute, limit = kChatPerMinute`; `RATE` → `"CLI-11", "rate", value = actionsInWindow, limit = kMaxActionsPerWindow`. `decisionId = NextDecisionId(s)`; `EmitFairnessReject(s, user, decisionId, "ChatParty", ...)`; `REFUSED` + reason + `length = textLen`. Sunucuya paket **gitmez**.
- **`ChatOutcome ActionExecutor::RequestChatParty(...)`**:
  - `s == nullptr || s->m_pUser == nullptr || !isInGame()` → `REFUSED "not_in_game"`; `isDead()` → `REFUSED "dead"` (olay yazılmaz).
  - **Ön koşullar (olay yazılmaz, `HandlePacket`'tan önce):** `(s->m_partyInviteEcho.load() & (1ull << 63)) != 0` → `REFUSED "invite_pending"` (gerçek istemci bekleyen davetle party sohbetine yazamaz; sunucu davetliyi "party'de" sayar, KI-014); `!user->isInParty()` → `REFUSED "not_in_party"`.
  - `nowMs`, `inWindow = s->m_actionWindow.CountInWindow(nowMs)`, `inMinute = s->m_chatWindow.CountInWindow(nowMs)`, `hash = BotCore::ChatTextHash(text.data(), (uint32)text.size())`, `sinceLastMs` (`m_chatHasLast` ise `now - m_chatLast` ms, değilse 0). `ChatCheck c = { BotCore::IsValidChatText(text.data(), (uint32)text.size()), s->m_chatHasLast, sinceLastMs, s->m_chatHasLast && hash == s->m_chatLastHash, inMinute, inWindow }`; `verdict != CHAT_OK` → `RejectChat`.
  - **Gönder:** `decisionId = NextDecisionId(s)`; `ACTION_SUBMIT` (`decisions`): `"decision_id","type":"ChatParty","channel":"party","len":<n>` (**`"mode"` anahtarı kullanma**: telemetri ortak `"mode":"live"` alanıyla çakışır, F4-08 Tur 1 bulgusu). Paket: `Packet pkt(WIZ_CHAT, uint8(PARTY_CHAT)); pkt << text;` (`ChatHandler.cpp:107-108`: `u8 tip + u16 uzunluklu metin`). `s->m_chatEcho = 0; s->m_chatEchoHash = 0;` → `user->HandlePacket(pkt)` (gecikme `steady_clock` ile) → `s->m_actionWindow.Record(nowMs); s->m_chatWindow.Record(nowMs); s->m_chatHasLast = true; s->m_chatLast = now; s->m_chatLastHash = hash;` (paket gittiği için sonuç `no_result` olsa da sayılır).
  - **Sonuç (yalnızca cevap paketinden):** `e = s->m_chatEcho.load()`; geçerli (bit 63) ve `((e >> 32) & 0xFF) == PARTY_CHAT` ve `(uint16)(e & 0xFFFF) == (uint16)user->GetID()` ve `s->m_chatEchoHash.load() == hash` → `SENT "sent"`; aksi halde `FAILED "no_result"`. (`isInParty()`/`isMuted()` sonuç için **okunmaz**.)
  - `ACTION_RESULT`: `"decision_id","type":"ChatParty","ok","reason","latency_us","len":<n>`. Başarıda ek olarak `CHAT_SENT` olayı (`Telemetry::Instance().Emit(TEL_DECISIONS, "CHAT_SENT", user->GetSocketID(), s->m_charName.c_str(), fields, false)`): `"decision_id":<id>,"channel":"party","len":<n>,"text":"` + `Telemetry::EscapeJson(text)` + `"`.
  - `length = (int)text.size()`.

`ActionExecutor.cpp`'de (yorumlar dahil) `PARTY_DELETE`, `"mode"` dizgesi geçmez. `HandlePacket(pkt)` çağrısı bu planda **tek yerde** (`RequestChatParty`). `CUser::Chat`, `ChatPacket`, `Send_PartyMember`, `Send_NearRegion`, `SendToRegion`, `ProcessChatCommand`, `WriteChatLogFile`, `isMuted` ActionExecutor'da/Bot koduna **yok**.

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirim (`CommandPartyManage`'in altına): `void CommandPartyChat(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()`:** `pkick` dalının yanına `pchat` (`CommandPartyChat(args)`) fiilini ekle; "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast, pot, sit, stand, target, regene, pinvite, paccept, pdecline, pleave, ppromote, pkick, pchat)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **`CommandPartyChat(args)`** `CommandPartyLeave` kalıbıyla, `CommandPartyManage`'in hemen altında (`ParseSpawnList`'ten önce). Metin boşluk içerebildiği için `SplitWords` **kullanılmaz**: `size_t space = args.find(' ');` bot adı = `args.substr(0, space)`, metin = `Trim(args.substr(space + 1))` (`space == npos` veya metin boş → kullanım). Kullanım: `BotManager: cmd pchat: usage: pchat <bot> <text>` (tek satır ve dön). Sonra `CommandPartyLeave`'deki gibi `FindSession` (`unknown or not spawned bot '<ad|?>'`) ve faz denetimi (`<bot> not in game (phase X)`), hepsi `cmd pchat:` önekiyle. `ChatOutcome outcome = ActionExecutor::RequestChatParty(s, text, std::chrono::steady_clock::now());` Sonuç günlüğü (`char message[256]`): `REFUSED` → `BotManager: cmd pchat: <bot> refused (<reason>)`; `SENT` → `BotManager: cmd pchat: <bot> sent (<length> chars)`; `FAILED` → `BotManager: cmd pchat: <bot> failed (<reason>)`.
3. **`TickSessions()`, `BeginDespawn()`, `BuildStatusLines()`:** **değişmez** (tek seferlik aksiyon, `list` biçimi sabit). Başka hiçbir yere dokunma (`ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj`, `.filters`, `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` **değişmez** (yeni dosya yok). Mevcut dosyaların kodlama/satır sonu/BOM durumu korunur.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı dört yeni test adını (`Combat_ChatText_Validity`, `Combat_ChatRateWindow`, `Combat_ChatCheck_Order`, `Combat_ChatCheck_Boundaries`) içerir ve toplam test sayısı **45**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; dosyada `#include` yalnızca `<algorithm>` ve `<cstdint>`; `grep -n "std::min\|std::max" BotCore/BotCombat.h` yeni satır göstermez.
- [ ] K5: sohbet paketi yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_CHAT" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma, tek yer) ve `BotSession.cpp` (`OnPacket` kayıt) satırlarını gösterir; `grep -nE "(->|\.)Chat\(|ChatPacket|Send_PartyMember|Send_NearRegion|SendToRegion|ProcessChatCommand|WriteChatLogFile|isMuted" GameServer/Bot/*.cpp GameServer/Bot/*.h` boş.
- [ ] K6: guard atlanmıyor: `RequestChatParty`'de `HandlePacket`'tan önce `invite_pending`/`not_in_party` ön koşulları, `CheckChat` çağrısı ve `CHAT_OK` dışında erken dönüş; `HandlePacket(pkt)` `RequestChatParty`'de **tek**; paket `PARTY_CHAT` + metin (başka `ChatType` sabiti kullanılmaz: `grep -nE "GENERAL_CHAT|PRIVATE_CHAT|SHOUT_CHAT|KNIGHTS_CHAT|PUBLIC_CHAT|COMMAND_CHAT|SEEKING_PARTY_CHAT" GameServer/Bot/*.cpp GameServer/Bot/*.h` boş); `grep -n "PARTY_CHAT" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp`'de.
- [ ] K7: sonuç yalnızca cevap paketinden: `RequestChatParty` gövdesinde sonuç kararı yalnızca `m_chatEcho`/`m_chatEchoHash` kayıtlarından verilir; `isInParty()` yalnızca ön koşul olarak ve `HandlePacket`'tan **önce** geçer; `m_sHp`, `m_iMaxHp`, `GetHealth`, `GetMaxHealth` bu fonksiyonun satır aralığında geçmez.
- [ ] K8: mevcut kod yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-11 -- GameServer/Bot/ActionExecutor.cpp | grep '^-' | grep -v '^---'` **boş**; `git diff gece/2026-10-02...bot/F4-11 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki bilinçli değiştirilen satır(lar)ı gösterir ve `WIZ_PARTY` bloğu dahil mevcut `OnPacket()` satırlarında **silinen/değişen satır yok** (yalnızca eklenen yeni `WIZ_CHAT` bloğu).
- [ ] K9: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutla çalışır; `OnPacket()`'teki yeni blok yalnızca bot oturumlarına gelen `WIZ_CHAT` için çalışır (bot yoksa hiç çağrılmaz); `git diff gece/2026-10-02...bot/F4-11 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca `unknown command` mesaj satırını gösterir; `Startup()`/`Tick()`/`TickSessions()`/`BuildStatusLines()`/`BeginDespawn()` ve ini okuma değişmedi.
- [ ] K10: `git diff --stat gece/2026-10-02...bot/F4-11` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` ve `GameServer/ChatHandler.cpp` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K11: değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K12: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(`, `SByte`, `DByte` yok; yeni `ACTION_SUBMIT`/`ACTION_RESULT` alan dizgilerinde `"mode"` anahtarı yok (`grep -n '\\"mode\\"' GameServer/Bot/ActionExecutor.cpp` boş); `CHAT_SENT` metni `Telemetry::EscapeJson` ile kaçışlanır (`grep -n "EscapeJson" GameServer/Bot/ActionExecutor.cpp` ≥ 1 satır).
- [ ] K13: F4-01..F4-10 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack" ...` ≥ 1, `grep -c "CheckCastStart" ...` ≥ 1, `grep -c "CheckPotion" ...` ≥ 1, `grep -c "CheckStance" ...` ≥ 1, `grep -c "CheckTargetHp" ...` ≥ 1, `grep -c "CheckRegene" ...` ≥ 1, `grep -c "CheckPartyInvite" ...` ≥ 1, `grep -c "CheckPartyAccept" ...` ≥ 1, `grep -c "CheckPartyDecline" ...` ≥ 1, `grep -c "CheckPartyLeave" ...` ≥ 1, `grep -c "CheckPartyManage" ...` ≥ 1, `grep -c "CheckChat" ...` ≥ 1; önceki 41 testin tamamı hâlâ geçiyor; yeni tip `ChatParty` yalnızca `RejectChat` ve `RequestChatParty` içinde geçer.
- [ ] K14 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-11
git diff gece/2026-10-02...bot/F4-11 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-11 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-11 -- GameServer/Bot/ActionExecutor.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-11 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj GameServer/ChatHandler.cpp
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "std::min\|std::max" BotCore/BotCombat.h
grep -n "WIZ_CHAT" GameServer/Bot/*.cpp
grep -n "PARTY_CHAT" GameServer/Bot/*.cpp
grep -nE "(->|\.)Chat\(|ChatPacket|Send_PartyMember|Send_NearRegion|SendToRegion|ProcessChatCommand|WriteChatLogFile|isMuted" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -nE "GENERAL_CHAT|PRIVATE_CHAT|SHOUT_CHAT|KNIGHTS_CHAT|PUBLIC_CHAT|COMMAND_CHAT|SEEKING_PARTY_CHAT" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "CheckChat\|HandlePacket\|isInParty\|m_chatEcho\|EscapeJson" GameServer/Bot/ActionExecutor.cpp
grep -n "m_sHp\|m_iMaxHp\|GetHealth\|GetMaxHealth" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(\|SByte\|DByte" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
grep -n '\\"mode\\"' GameServer/Bot/ActionExecutor.cpp
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-11
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. AIServer bağlı olmalıdır (`AG_USER_PARTY`). Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; **bir dosyadaki satırlar aynı `Tick()`'te sırayla çalışır**, ayrı dosyalar ≥ 1,05 sn arayla verilmelidir; sohbet boşluğu testleri için bu önemlidir). Botlar: Karus `BotWP_K`, `BotMF_K`, `BotPHD_K`, `BotWG_K`; zone 71, aynı bölge. Gözlem `Logs/bots/<tarih>/live-*.jsonl`, `Logs/Bot_*.log` ve sunucunun chat günlüğünden (`Logs/Chat_*.log`, `PARTY_CHAT` satırı). Senaryolar:

1. **Kurulum:** `pinvite BotWP_K BotMF_K` → `created`; ≥ 1,05 sn sonra `paccept BotMF_K` → `joined` (iki kişilik party).
2. **Gönderim:** `pchat BotWP_K HEDEF: BotWG_K (Malice)` → `sent (23 chars)`; `ACTION_SUBMIT` + `ACTION_RESULT` (`"type":"ChatParty"`, `"ok":true`, `"reason":"sent"`, `latency_us` ölçülür) + `CHAT_SENT` (`"channel":"party"`, `text` aynı); `Logs/Chat_*.log`'da `PARTY_CHAT` satırı (`BotWP_K : HEDEF: BotWG_K (Malice)`). Üyenin yazması: ayrı dosyada `pchat BotMF_K TOPLAN` → `sent` (kendi sayacı, `BotWP_K`'dan bağımsız).
3. **Guard (`BotWP_K`, her komut ayrı dosyada; `T0` = 2'deki gönderim):** `T0 + < 3 sn` → `pchat BotWP_K selam` → `refused (chat_gap)` + `FAIRNESS_REJECT` (`CLI-18`, `value` < 4000, `limit` 4000; paket gitmedi: chat günlüğünde satır yok; reddedilen mesaj sayaçları değiştirmez); `T0 + ≥ 4,05 sn` → `pchat BotWP_K selam` → `sent` (`T1`); `T1 + ≥ 4,05 sn ve < 8 sn` → aynı `pchat BotWP_K selam` → `refused (chat_dup)` (`limit` 8000); `T1 + > 8 sn` → aynı metin → `sent`; ayrıca `pchat BotWP_K +bot list` → `refused (bad_text)` (`value` 9, `limit` 128); 129 karakterlik metin → `refused (bad_text)` (`value` 129); `pchat BotWP_K GERİ` (UTF-8, ASCII dışı) → `refused (bad_text)`; `pchat BotMF_K` (metin yok) → kullanım satırı.
4. **Dakika sınırı:** `BotWP_K` ile 6 farklı metni ≥ 4,05 sn arayla gönder (hepsi `sent`); yedincisi (yine ≥ 4,05 sn sonra, farklı metin) → `refused (chat_minute)` (`value` 6, `limit` 6); ilk mesajdan ≥ 60 sn geçince yeniden `sent`.
5. **Ön koşullar:** `pchat BotWG_K merhaba` (party'de değil) → `refused (not_in_party)` (olaysız, paket gitmedi); bekleyen davetli: `pinvite BotWP_K BotPHD_K` sonrası (kabul etmeden) `pchat BotPHD_K merhaba` → `refused (invite_pending)` (olaysız); `pdecline BotPHD_K` ile temizle; `pchat BotNope x` → `unknown or not spawned bot '?'`.
6. **Despawn/ENABLED/özet:** botlar despawn temiz, sunucu çökmedi; `RESPAWN_CYCLES=2` ile komut reddedilir; `TELEMETRY=summary` → `ACTION_*`/`CHAT_SENT` yok ama komut çalışır ve chat günlüğüne yazılır; `ENABLED=0` → dosya tüketilmez/log yok.
7. **Gerilemesiz:** F4-01..F4-10 komutları (`move`/`attack`/`cast`/`pot`/`sit`/`target`/`regene`/`pinvite`/`paccept`/`pdecline`/`pleave`/`ppromote`/`pkick`) çalışır; `tick_p95_us` ≤ 500.

Beklenmeyen `no_result` bu planın hatası değil, **sonuç olarak raporlanır** (özellikle gönderenin kendi yayınının kendi alıcısına gelip gelmediği; gelmiyorsa **durup** raporla, bot sunucu nesnesine bakarak "başarılı" demez). Sınanmayan durumlar (susturulmuş bot, hapisteki bot vb.) raporda açıkça yazılır.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF; kod yorumları İngilizce; tab girintisi.
- Thread kuralı: tüm yeni oturum alanları (atomikler hariç) yalnızca IOCP thread'inde okunur/yazılır; atomikleri yalnızca `OnPacket()` (aynı thread, kendi mesajı için) yazar. Konsol thread'i yalnızca komut kuyruğuna yazar (ADR-0015).
- `GameServer/ChatHandler.cpp` BOM'lu UTF-8'dir ve bu planda **değişmez**; `OnPacket()` mevcut satırlarına dokunma, yalnızca yeni bloğu ekle.
- Metin tamamen ASCII'dir; `std::string` içinde çok baytlı karakter olursa `IsValidChatText` reddeder (K3 testi). Komut dosyası satırı 255 baytı aşarsa `fgets` satırı böler: bu bilinen bir sınırdır, planın kapsamı dışındadır.
- Bu plana özgü risk: `ChatTextHash` ve sonuç eşlemesi gelen **herhangi** bir `WIZ_CHAT`'i tek kayıtta tutar; birden çok bot aynı party'de aynı anda yazarsa bir botun yayını diğerinin kaydını ezebilir. Bot komutları aynı `Tick()` içinde sırayla çalıştığından (aynı thread) aksiyon içinde (gönder → oku) araya başka paket girmez; yük altında çok iş parçacıklı gelen başka oyuncu sohbeti `no_result` üretebilir ve **sonuç olarak raporlanır**, gizlenmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-11` — `2c2e127 [F4-11] Party chat dilimi: ChatParty + CLI-18 korumasi`; plan `Durum` satırı ve bu rapor ayrı commit'te.
- Değişen dosyalar ve nedenleri (yalnızca §4 listesi, 8 dosya; yeni dosya yok):
  - `BotCore/BotCombat.h` — yalnızca sona ekleme: `kChatMaxLen/kChatGapMs/kChatDupMs/kChatPerMinute/kChatMinuteMs`, `IsValidChatText`, `ChatTextHash`, `ChatRateWindow`, `ChatCheck`, `ChatVerdict`, `CheckChat`. Yeni `#include` yok; `std::min/max` yok.
  - `Tests/BotCoreTests/CombatTests.cpp` — yalnızca sona ekleme: `Combat_ChatText_Validity`, `Combat_ChatRateWindow`, `Combat_ChatCheck_Order`, `Combat_ChatCheck_Boundaries` (41 + 4 = 45).
  - `GameServer/Bot/BotSession.h` — 4 IOCP alanı (`m_chatHasLast`, `m_chatLast`, `m_chatLastHash`, `m_chatWindow`) ve 2 atomik (`m_chatEchoHash`, `m_chatEcho`).
  - `GameServer/Bot/BotSession.cpp` — başlatıcı listesi, `ResetForRespawn()` sıfırlamaları ve `OnPacket()` sonuna `WIZ_CHAT` kayıt bloğu (mevcut `OnPacket()` satırlarına dokunulmadı).
  - `GameServer/Bot/ActionExecutor.h` — `ChatOutcome` ve `RequestChatParty` bildirimi.
  - `GameServer/Bot/ActionExecutor.cpp` — dosya sonuna ekleme: `RejectChat` + `RequestChatParty` (tek `HandlePacket` çağrısı, sonuç yalnızca `m_chatEcho`/`m_chatEchoHash`'ten, başarıda `CHAT_SENT`).
  - `GameServer/Bot/BotManager.h` — `CommandPartyChat` bildirimi.
  - `GameServer/Bot/BotManager.cpp` — `pchat` fiil dağıtımı, `CommandPartyChat` (metin boşluk içerdiği için `SplitWords` yok), `unknown command` listesine `pchat`.
- Derleme sonucu:
  - `./tools/build.sh Release` rc=0; değişen 5 kaynak dosya için uyarı/derleme hatası yok (yalnızca eski `User.cpp` C4834).
  - `./tools/build.sh Debug` rc=0 (eski `GameServerDlg.cpp` C4267).
  - `./tools/run-tests.sh Release` → `45 tests, 0 failed`; dört yeni test adı görüldü.
  - `./tools/run-tests.sh Debug` → `45 tests, 0 failed`.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ — Release rc=0; değişen dosyalarda uyarı yok.
  - K2 ✔ — Debug rc=0.
  - K3 ✔ — 45 test, 0 failed; dört yeni test adı çıktıda (Release + Debug).
  - K4 ✔ — `grep "windows.h\|stdafx\|GameServer\|shared/"` boş; `#include` yalnızca `<algorithm>`, `<cstdint>`; `std::min/max` boş.
  - K5 ✔ (bir sözde-uyum için bkz. sapmalar) — `WIZ_CHAT` yalnızca `ActionExecutor.cpp` (paket) ve `BotSession.cpp` (`OnPacket`); yasak chat API dizgileri boş.
  - K6 ✔ — `PARTY_CHAT` yalnızca `ActionExecutor.cpp`; başka `ChatType` sabiti yok; `HandlePacket(pkt)` `RequestChatParty`'de tek; ön koşullar + `CheckChat` + `CHAT_OK` dışı erken dönüş var.
  - K7 ✔ — `RequestChatParty` sonucu yalnızca `m_chatEcho`/`m_chatEchoHash`'ten; `isInParty()` yalnızca ön koşul; `m_sHp/m_iMaxHp/GetHealth/GetMaxHealth` ActionExecutor'da yok.
  - K8 ✔ — `ActionExecutor.cpp` silinen satır yok; `BotSession.cpp` yalnızca başlatıcı satırı (`m_partyLeaveEcho(0)`); `OnPacket()` mevcut satırları değişmedi.
  - K9 ✔ (kod/diff düzeyinde) — yeni kod yalnızca `PHASE_IN_GAME` + komutla; `BotManager.cpp` silinen tek satır `unknown command` metni; `Startup/Tick/TickSessions/BuildStatusLines/BeginDespawn` ve ini okuma değişmedi. Çalışma zamanı `ENABLED=0` Claude'da.
  - K10 ✔ — `git diff --stat gece/2026-10-02...bot/F4-11` yalnızca §4'teki 8 dosya; vcxproj'lar ve `ChatHandler.cpp` değişmedi.
  - K11 ✔ — değişen dosyalar ASCII + CRLF; `git diff --check` boş.
  - K12 ✔ — yasak dizgiler yok; `\"mode\"` yok; `EscapeJson` kullanılıyor.
  - K13 ✔ — tüm `Check*` sayaçları ≥ 1; `ChatParty` yalnızca `RejectChat`/`RequestChatParty` içinde; 41 eski test hâlâ geçiyor.
  - K14 — Claude'a ait (çalışma zamanı).
- Plandan sapmalar ve gerekçeleri:
  1. `BotSession.cpp` `OnPacket()` `WIZ_CHAT` yorumundan `, ChatPacket::Construct` ifadesi çıkarıldı (yorum `(ChatHandler.cpp:161):` olarak kaldı). Gerekçe: planın §5.3'te verdiği yorum metni `ChatPacket` dizgisini içeriyor, ama aynı planın K5 kabul grep'i (`...|ChatPacket|...`) bu dizgiyi `GameServer/Bot/*.cpp`'de yasaklıyor; plan kendi kabul ölçütüyle çelişiyordu. Kod/anlam değişmedi, yalnızca yorumdaki sınıf adı kaldırıldı; K5 grep'i artık boş. (Not: plan bu satırı **olduğu gibi** istemişti; Claude doğrulamada yorumu geri isteyip K5'i gevşetmeyi seçebilir.)
  2. Başka sapma yok; §5 adımları birebir uygulandı.
- Açık sorular:
  - Yukarıdaki yorum/`K5` çelişkisi için hangi tarafın esas alınacağı (yorum metni mi, grep ölçütü mü) netleşmeli.
  - `RequestChatParty` içinde `s->m_castSelfId` atanmadı (plandan farklı değil; `WIZ_CHAT` kaydı her göndericiyi kaydettiği için gerekmiyor). Doğrulamada kendi yayınının kendi alıcısına gelip gelmediği (SENT vs `no_result`) ölçülmeli.
- Doğrulama komutları (§7) sonuçları: K4, K5, K6, K7, K10, K11, K12, K13 grep'leri beklendiği gibi; K8/K9 silinen satırlar yalnızca yukarıdaki bilinçli satırlar.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-11` @ `368ba19` (kod commit'i `2c2e127`; ikinci commit yalnızca plan raporu)
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `./tools/build.sh Release` rc=0; beş değişen kaynak `touch` ile yeniden derlendi, uyarı çıktısında bu dosyalar yok (yalnızca eski `UpgradeHandler.cpp` C4789) |
| K2 | ✔ | `./tools/build.sh Debug` rc=0, değişen dosyalarda uyarı yok |
| K3 | ✔ | `run-tests.sh Release` ve `Debug`: `45 tests, 0 failed`; dört yeni `Combat_Chat*` test adı çıktıda (Release'de 4 eşleşme) |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/"` boş; `#include` yalnızca `<algorithm>`, `<cstdint>`; `std::min/max` boş |
| K5 | ✔ | `WIZ_CHAT`: `ActionExecutor.cpp:2599` (paket) ve `BotSession.cpp:148` (kayıt); yasak chat API grep'i boş (sapma 1 için bkz. not 1) |
| K6 | ✔ | `ActionExecutor.cpp`: `invite_pending` (`:2551`) / `not_in_party` (`:2557`) ön koşulları, `CheckChat` `:2585`, `CHAT_OK` dışı erken dönüş, tek `HandlePacket` `:2606`; `PARTY_CHAT` yalnızca `ActionExecutor.cpp`; diğer `ChatType` sabiti yok |
| K7 | ✔ | sonuç `:2619-2623` yalnızca `m_chatEcho`/`m_chatEchoHash`; `isInParty()` `:2557` (HandlePacket'tan önce); `m_sHp`/`GetHealth` vb. ActionExecutor'da yok |
| K8 | ✔ | `ActionExecutor.cpp` silinen satır yok; `BotSession.cpp` yalnızca başlatıcı satırı `m_partyLeaveEcho(0)` (virgül eklendi); `OnPacket()` mevcut satırlar değişmedi, yeni blok `:143-168` |
| K9 | ✔ | `BotManager.cpp` silinen tek satır `unknown command` metni; `ExecuteCommand` baş kısmı (`m_respawnCycles` reddi `:601`), `Tick`/`TickSessions`/`BuildStatusLines`/`BeginDespawn` farkta yok; çalışma zamanı `ENABLED=0`: `pchat` dosyası tüketilmedi, `Logs/bots` oluşmadı |
| K10 | ✔ | `git diff --stat`: yalnızca §4'teki 8 dosya + plan; vcxproj/filters ve `ChatHandler.cpp` farkı 0 satır |
| K11 | ✔ | tüm `GameServer/Bot/*` ve iki BotCore dosyası `ASCII text, with CRLF line terminators`; `git diff --check` rc=0 |
| K12 | ✔ | yasak dizgiler boş; `"mode"` anahtarı yok (JSONL'de ortak `"mode":"live"` tek kez); `EscapeJson` 1 satır (`:2642`) |
| K13 | ✔ | `CheckMoveStep` 2, diğer 11 önceki guard ve `CheckChat` 1; `ChatParty` yalnızca `RejectChat` (`:2517`) ve `RequestChatParty` (`:2593`, `:2629`); önceki 41 test geçiyor |
| K14 | ✔ | çalışma zamanı senaryo 1-5, 7 geçti, 6 kısmen (aşağıda; `RESPAWN_CYCLES` reddi sınanmadı) |

- **Çalışma zamanı (K14, Release, AIServer bağlı, `TELEMETRY=decisions`, zone 71; `BotWP_K` #2984, `BotMF_K` #2985, `BotPHD_K` #2986, `BotWG_K` #2987):**
  1. Kurulum: `pinvite WP MF` → `created`, `paccept MF` → `joined`.
  2. Gönderim: `pchat BotWP_K HEDEF: BotWG_K (Malice)` → `sent (23 chars)`; JSONL `ACTION_SUBMIT` + `ACTION_RESULT` (`ChatParty`, `ok:true`, `reason:"sent"`, `latency_us` 97) + `CHAT_SENT` (`channel:"party"`, metin aynı); `Logs/Chat_2_10_2026.log`: `PARTY_CHAT ... BotWP_K : HEDEF: BotWG_K (Malice)`. Üye: `pchat BotMF_K TOPLAN` → `sent (6 chars)` (49 µs, ayrı sayaç). Gönderenin kendi yayını kendi alıcısına geliyor (`sent`, `no_result` yok); `OnPacket()` okuma konumları ve FNV-1a özeti doğru.
  3. Guard (`BotWP_K`): `selam` → `sent`; 1,09 sn sonra → `refused (chat_gap)` (`FAIRNESS_REJECT` `CLI-18`, `value:1091`, `limit:4000`); 5,5 sn sonra aynı metin → `refused (chat_dup)` (`value:5477`, `limit:8000`); ~10,7 sn sonra → `sent`; `+bot list` → `refused (bad_text)` (`value:9`, `limit:128`); 129 karakter → `bad_text` (`value:129`); `GERİ` (UTF-8) → `bad_text` (`value:5`); `pchat BotMF_K` (metin yok) → kullanım satırı. Reddedilen mesajlar chat günlüğüne düşmedi (yalnızca `sent` olanlar).
  4. Dakika sınırı: ilk mesajdan sonra 60 sn pencerede 6 mesaj (`m4`, `m5`, `m7` `sent`, `m6` bir kez `chat_gap`; elle zamanlama) → yedinci `refused (chat_minute)` (`value:6`, `limit:6`); en eski mesaj 60 sn'yi aşınca `m9` → `sent`.
  5. Ön koşullar: `pchat BotWG_K merhaba` → `refused (not_in_party)`; `pinvite BotWP_K BotPHD_K` sonrası `pchat BotPHD_K merhaba` → `refused (invite_pending)`; ikisinde de JSONL'de `ChatParty` olayı yok, chat günlüğüne satır düşmedi; `pdecline BotPHD_K` temizledi; `pchat BotNope x` → `unknown or not spawned bot '?'`.
  6. Despawn/özet/`ENABLED`: `despawn BotWP_K` sonrası `pchat BotWP_K ciao2` → `not in game (phase despawned)`; dört bot despawn temiz (`pool free 16/16`), sunucu 3/3 UP. `TELEMETRY=summary`: `pchat` → `sent (12 chars)`, chat günlüğüne yazıldı, JSONL'de `ACTION_*`/`CHAT_SENT`/`ChatParty` 0 satır. `ENABLED=0`: komut dosyası tüketilmedi, `Logs/bots` oluşmadı.
  7. Gerilemesiz: `sit`, `stand`, `regene` (`not_dead`), `target`, `pinvite`, `paccept`, `ppromote`, `pkick`, `pleave` (`disbanded`), `move`, `stop` çalıştı. `tick_p95_us` normalde 68-120 (bir pencerede 461), komut dosyalarının çok olduğu üç pencerede 1114-1358; aynı üç pencere `sit`/`stand`/`regene`/... gibi eski komutlardan oluşuyor (kontrol), yani `pchat`'e özgü bir gerileme değil (F4-10'daki örüntü); `skipped_ticks` 0, `dropped` 0. **Sınanmadı:** `RESPAWN_CYCLES` reddi (`DESPAWN_AFTER_SEC > 0` ister; ilgili kod `BotManager.cpp:601-604` farkta yok), ölü bot (`refused (dead)`), susturulmuş/hapisteki bot (`no_result`).
- Bulgular (önem sırasıyla): engelleyici yok.
  1. **[Not] Sapma 1 kabul:** `BotSession.cpp:143-147` yorumundan `ChatPacket::Construct` ifadesi çıkarıldı; plan §5.3'ün yorum metni ile K5 grep'i (`ChatPacket` yasak) çelişiyordu. K5 esas alındı, yorum (`ChatHandler.cpp:161`) anlam olarak yeterli. Plan kusuru: Claude'un, bundan sonraki planlarda yorum metnini K5 grep'iyle çakıştırmaması gerekir.
  2. **[Not]** Aynı anda ikinci bir oyuncunun/botun sohbeti kaydı ezebilir (plan §8'de bilinen sınır); aynı `Tick()` içinde sıralı çalıştığından denemede `no_result` görülmedi.
  3. **[Not]** `+bot` yardım metni (KI-012) `pchat` dahil F4 fiillerini listelemiyor; KI-012 güncellendi.
- Düzeltme talimatı: yok (`DOĞRULANDI`).
- Temizlik: `GameServer.ini` yedekten geri yüklendi (md5 `265a8e1c35ea12df46f6d006fe894d9b`, değişmedi); `BotCommands.txt` yok; `Logs/bots` → `Logs/bots_old_f411` (+ `_f411a`, `_f411b`); sunucular kapatıldı (`0/3 hazır`); DB'ye dokunulmadı; `bot/F4-11` temiz.
