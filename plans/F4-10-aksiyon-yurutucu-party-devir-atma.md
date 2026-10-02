# F4-10: `ActionExecutor` party devir ve atma dilimi — `PartyPromote` / `PartyKick` ve `BotFairnessGuard` CLI-17 kuralları

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-02, gece/2026-10-02, merge `3e0e985`) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-10` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-09 (`m_partyLeaveEcho`, `OnPacket()` `PARTY_REMOVE`/`PARTY_DELETE` kaydı, `RejectParty*` kalıbı) — `KAPANDI` (merge `6dc7979`); F4-08 (`m_partyJoinEcho`, `PartyOutcome`, `BotManager` test sürücüsü kalıbı) — `KAPANDI` (merge `851afdc`) |
| İlgili gereksinim / kabul | CLI-17 (`docs/03` §14, bu planla birlikte eklendi), CLI-11, MEC-PTY-03, KI-015 (bu planla birlikte eklendi), AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı |
| Tahmini büyüklük | M (8 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun on ikinci ve on üçüncü gerçek aksiyonları: party lideri bir bot **gerçek `WIZ_PARTY` paketiyle** party'sindeki bir üyeyi lider yapar (`PartyPromote`: `PARTY_PROMOTE` + hedefin oturum kimliği) ya da bir üyeyi party'den atar (`PartyKick`: `PARTY_REMOVE` + **hedefin** oturum kimliği; hedef atılınca yalnızca lider kalıyorsa sunucu party'yi dağıtır, MEC-PTY-03). İkisi de `CUser::HandlePacket()` üzerinden gider. Sunucu yalnızca liderlik denetimi yapar (devirde ayrıca "hedef aynı party'de mi"); atmada hedefin üye olup olmadığına **bakmaz** (KI-015: üye olmayan bir kimlikle `PARTY_REMOVE` ya üyelere sahte bir `PARTY_REMOVE` yayını yapar, ya da lider tek başınaysa party'yi dağıtır) ve hız/gecikme denetimi hiç yoktur. Bu yüzden paketler sunucuya gitmeden önce `BotFairnessGuard` kurallarından geçer: yalnızca lider devreder/atar (`not_leader`), hedef botun party'sinin üyesi olmalıdır (`not_member`; gerçek istemcinin party paneli yalnızca üyeleri listeler), iki devir/atma arası ≥ 1,0 sn (`manage_gap`) ve CLI-11 aksiyon hızı. Karar katmanı yoktur: aksiyonları `/bot ppromote <bot> <hedef bot>` ve `/bot pkick <bot> <hedef bot>` komutları tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün onuncu dilimidir (ADR-0017 Ek F4-10; hareket → saldırı → cast → pot → duruş → hedef HP → `Regene` → `Party` kurulumu → `Party` ret/ayrılma → **`PartyPromote`/`PartyKick`** → `Chat` → `Perception`).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-10)" (bu planla birlikte yazıldı): paket düzenleri, guard kuralları, sonuç eşlemesi, kapsam.
- `docs/03` §10 **MEC-PTY-03**, §14 **CLI-17** (bu planla birlikte eklendi), **CLI-16**, **CLI-11**; `docs/KNOWN_ISSUES.md` **KI-015** (atma, üye olmayan kimlik).
- `plans/F4-09-aksiyon-yurutucu-party-ret-ayrilma.md` §5.2–§5.5: bu planın kalıpladığı iskelet (saf mantık `BotCore/BotCombat.h` + `ActionExecutor` + `BotSession` kayıtları + `BotManager` komutu). **Yazılı planı değil, birleşmiş kodu esas al** (`ActionExecutor.cpp` `RejectPartyLeave` ve `RequestPartyLeave`, `BotManager.cpp` `CommandPartyInvite`/`CommandPartyLeave`).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `6dc7979` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:379-381` — `HandlePacket` `case WIZ_PARTY`: `PartyProcess(pkt)`.
  - `GameServer/PartyHandler.cpp:34-38` — `PartyProcess`: `PARTY_PROMOTE` → `PartyPromote(pkt.read<uint16>())`; `:40-43` `PARTY_REMOVE` → `PartyRemove(pkt.read<uint16>())`. `shared/packets.h:241` `PARTY_REMOVE` = 0x04, `:249` `PARTY_PROMOTE` = 0x1C (alt-opcode sabitleri **adıyla** kullanılır, sayı yazılmaz).
  - `GameServer/PartyHandler.cpp:268-335` — `PartyPromote(sMemberID)`: `!isPartyLeader()` → döner (`:271-272`); hedef kullanıcı yoksa veya `pUser->GetPartyID() != GetPartyID()` → döner (`:275-278`); party yoksa döner; hedefin dizideki yerini bulur (lider her zaman 0; yer 0 çıkarsa döner: **kendini devretme etkisizdir**, `:286-298`); `std::swap(uid[0], uid[pos])`, `m_bPartyLeader` ve `m_bNeedParty` takası, eski liderin `StateChangeServerDirect(6, 0)` ve yeni liderin `(6, 1)` yayınları (`:301-313`); sonunda `Send_PartyMember(GetPartyID(), ...)` ile **tüm üyelere** (devreden dahil) `WIZ_PARTY`: `u8 PARTY_INSERT, u16 yeni lider sid, u8 100 (lider konumu sıfırlama), ad, ...` (`:315-323`).
  - `GameServer/PartyHandler.cpp:337-407` — `PartyRemove(memberid)`: `!isInParty()` veya zone `ZONE_JURAD_MOUNTAIN` → döner; hedef kullanıcı yoksa döner; `memberid != GetSocketID()` (atma) → yalnızca `pParty->uid[0] == GetSocketID()` (lider) atabilir, değilse sessizce döner (`:355-359`). Üye sayımı (`:369-381`): atılan hariç kalan dolu yuva sayısı; **`count == 1`** (atılandan sonra yalnızca lider kalıyor) → liderin `PartyDelete()`'i çağrılır ve `return` edilir, `PARTY_REMOVE` yayını **yapılmaz** (`:383-391`). Aksi halde `WIZ_PARTY`: `u8 PARTY_REMOVE, u16 memberid`, `Send_PartyMember(m_sPartyIndex, ...)` ile **tüm üyelere, atılan ve atan dahil** (`:393-395`; `memberPos >= 0` ise atılanın `m_bInParty`'si yayından **sonra** temizlenir, `:397-401`). **Atılan kimlik party'de değilse** (`memberPos == -1`): `count` tüm üyelerdir; ≥ 2 ise üyelere sahte `PARTY_REMOVE` yayını + AIServer'a `PARTY_REMOVE`, durum değişmez; lider tek başınaysa `count == 1` olur ve party dağıtılır (KI-015, `not_member` kuralının gerekçesi).
  - `GameServer/PartyHandler.cpp:409-443` — `PartyDelete()`: tüm üyelere `WIZ_PARTY`: `u8 PARTY_DELETE` (1 bayt, `Send_PartyMember`, `:433-434`) ve üyelerin `m_bInParty = false` (`:423-431`).
  - `GameServer/GameServerDlg.cpp:1080-1093` — `Send_PartyMember(party, pkt)`: party üyelerinin `Send()`'i (bot alıcısına gider).
  - `GameServer/User.h:313` `isInParty()`, `:319` `isPartyLeader()`, `:420` `GetPartyID()` (`m_sPartyIndex`), `GameServer/Unit.h:54` `GetID()`; `GameServer/User.cpp:205-217` sunucunun kendi kopma yolu (`PartyPromote(uid[1])` + `PartyRemove(GetSocketID())`).
  - `GameServer/Bot/BotSession.cpp:101-139` — `OnPacket()` `WIZ_PARTY` bloğu: **bu planda değişmez**. Zaten kaydeder: `m_partyJoinEcho` (`PARTY_INSERT` uzun yük: `valid | sid << 8 | flag`, devirde `flag = 100`), `m_partyLeaveEcho` (`PARTY_REMOVE` → `valid | 1 << 16 | sid`, `PARTY_DELETE` → `valid | 2 << 16`). `GameServer/Bot/BotSession.h:109-129` party üyeleri, `:174-194` `ResetForRespawn()` (satırlar `BotSession.cpp:142-195` arası).
  - `GameServer/Bot/ActionExecutor.cpp:1718-1750` `RejectPartyInvite` (kalıp), `:1775-1926` `RequestPartyInvite` (`PartyInviteTarget` kullanımı, `inWindow`/`Record` düzeni, `m_partyInviteHasLast`), `:2060-2082` `RejectPartyLeave`, `:2171-2287` `RequestPartyLeave` (en yakın kalıp); `:36` `NextDecisionId`, `:53` `EmitFairnessReject`. `GameServer/Bot/ActionExecutor.h:100-125` `PartyInviteTarget`/`PartyOutcome`, `:212-238` bildirimler. `GameServer/Bot/BotManager.cpp:639-646` fiil dağıtımı, `:650` `unknown command` listesi, `:1859-1935` `CommandPartyInvite` (iki botlu komut kalıbı), `:2031-2076` `CommandPartyLeave`. `BotCore/BotCombat.h:425-563` party dilimleri (F4-08/F4-09; `CheckPartyLeave` `:553-562`, namespace kapanışı `:563`).

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** `kPartyManageGapMs`, `PartyManageCheck`, `PartyManageVerdict`, `CheckPartyManage` (devir ve atma için **ortak** kural). Birim testleri (`CombatTests.cpp`).
2. **`ActionExecutor::RequestPartyPromote(s, target, now)`** ve **`RequestPartyKick(s, target, now)`** (tek seferlik aksiyonlar; iç ortak yardımcı): ön koşullar → guard → `WIZ_PARTY` paketi → `HandlePacket` → yayınlanan cevaplardan sonuç eşleme → telemetri. Yeni `PartyMemberTarget` yapısı (hedef kimliği + "benim party'mde mi" bilgisi).
3. **Oturum durumu (`BotSession`):** son devir/atma zamanı (`m_partyManageHasLast`/`m_partyManageLast`, IOCP thread; `ResetForRespawn()` sıfırlar). `OnPacket()` **değişmez**.
4. **Komutlar (`BotManager`):** `ppromote <bot> <hedef bot>` ve `pkick <bot> <hedef bot>` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten); ortak `CommandPartyManage(args, kick)`. Üyelik bilgisini hedef botun oturumundan okuyan küçük bir yardımcı (`IsSamePartyMember`, §5.5).
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT` / `ACTION_RESULT` (`"type":"PartyPromote"` ve `"PartyKick"`), `FAIRNESS_REJECT` (aynı tipler).

**Kapsam dışı (yapılmayacak)**

- **Kim kimi ne zaman devreder/atar** (karar katmanı; `docs/09` §2, F7): yok. Komutlar guard dışında hiçbir koşula bakmaz.
- **Başkasını atma dışındaki dağıtma (`PARTY_DELETE` paketi), party chat (`WIZ_CHAT`), party arama panosu (`WIZ_PARTY_BBS`), `PARTY_HPCHANGE` izleme:** yok (`Chat` sonraki dilim).
- **Party üye listesi tutmak:** bot durumunda üye listesi yoktur; üyelik bilgisi yalnızca komut sürücüsünde, iki oturumdan okunur (`Perception` diliminde yerini alacak; ADR-0017 Ek F4-10 madde 4).
- **Atılan botun kendi oturum kayıtlarının temizliği:** atılan botun `m_partyEnteredHasAt`'i bayat kalır (zararsızdır: yeniden girişte üzerine yazılır, `pleave` zaten `not_in_party` ile reddedilir). `OnPacket()` IOCP-yalnızca alanlara dokunmaz.
- **Başka bir botun ya da insanın devir/atmasına tepki** (lidere/üyeye gelen `PARTY_INSERT` flag 100, `PARTY_REMOVE`): `m_partyJoinEcho`/`m_partyLeaveEcho` yalnızca kendi isteğinin sonucu için okunur.
- `ChatHandler.cpp` `+bot` yardım metni (KI-012; Claude doğrulamada günceller). Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya yok; **`GameServer` projesine dosya eklenmez**. `Telemetry.*`, `ScenarioRunner.*` değişmez; `list` satırı değişmez; `OnPacket()` değişmez.
- Dokümanları (`docs/03`, `docs/13`, `docs/16`, `docs/15`, `docs/KNOWN_ISSUES.md`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | Yalnızca ekleme (§5.2); mevcut içerik ve `#include`'lar değişmez |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | Yalnızca ekleme: iki yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `PartyOutcome` yorumu genişler; yeni `PartyMemberTarget`; iki yeni statik fonksiyon |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | Yalnızca dosya sonuna ekleme (§5.4); mevcut kod değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca iki alan (§5.3) |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi ve `ResetForRespawn()`; `OnPacket()` **değişmez** |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandPartyManage` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, komut, yardımcı, `unknown command` listesi |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (Yeni dosya açılmaz: guard `BotCombat.h`'ye eklenir, böylece `BotCore*.vcxproj` değişmez.)

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-10 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotCombat.h` (saf mantık, mevcut `namespace BotCore` içine ekleme)

Mevcut kodun biçimini (tab, Allman, `inline`, İngilizce kısa yorum, ölçülmemiş değer `[A]`) koru. Yeni `#include` gerekmez. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez** (yorumlarda da `shared/` dizgisi **yazma**: K4 grep'i takılır). `CheckPartyLeave`'den **sonra**, `namespace`'in kapanışından önce ekle. **`std::min`/`std::max` kullanma**:

```cpp
	// --- party promote / kick slice (ADR-0017 Ek F4-10) ---

	constexpr uint32_t kPartyManageGapMs = 1000;   // docs/03 CLI-17: a human needs at least this long between two leader actions (promote / kick) [A] (unmeasured)

	struct PartyManageCheck
	{
		bool isLeader;            // the bot leads its party (own state; false when it leads nothing)
		bool targetInParty;       // the target is a member of the bot's party (the party panel lists the members)
		bool hasLast;             // a promote or kick was sent earlier in this spawn
		uint32_t sinceLastMs;     // since that action
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum PartyManageVerdict
	{
		PARTYMANAGE_OK = 0,
		PARTYMANAGE_REJECT_LEADER = 1,   // CLI-17 (only the party leader promotes or kicks)
		PARTYMANAGE_REJECT_MEMBER = 2,   // CLI-17 (the target is not in the bot's party; the server does not check this for a kick, KI-015)
		PARTYMANAGE_REJECT_GAP = 3,      // CLI-17 (second leader action before kPartyManageGapMs)
		PARTYMANAGE_REJECT_RATE = 4      // CLI-11
	};

	// Guard rule for a promote or a kick. The caller has already checked that the bot is in a game, alive, in a party,
	// and that the target is another valid player. Order: leader, member, gap (only when a previous action is known), rate.
	inline PartyManageVerdict CheckPartyManage(const PartyManageCheck & c);
```

Gövde (`CheckPartyInvite` kalıbı, aynı yerde `inline`):

- `!c.isLeader` → `PARTYMANAGE_REJECT_LEADER`; `!c.targetInParty` → `PARTYMANAGE_REJECT_MEMBER`; `c.hasLast && c.sinceLastMs < kPartyManageGapMs` → `PARTYMANAGE_REJECT_GAP`; `c.actionsInWindow >= kMaxActionsPerWindow` → `PARTYMANAGE_REJECT_RATE`; aksi halde `PARTYMANAGE_OK`.

**`Tests/BotCoreTests/CombatTests.cpp` (ekleme, mevcut makro stili `CHECK_EQ((int)..., (int)...)`):** iki yeni `TEST_CASE` (dosyanın sonuna, `Combat_PartyLeaveCheck_Boundaries`'ten sonra):

- `Combat_PartyManageCheck_Order`: tüm ihlaller (`isLeader = false, targetInParty = false, hasLast = true, sinceLastMs = 0, actionsInWindow = 6`) → `PARTYMANAGE_REJECT_LEADER`; `isLeader = true` → `..._MEMBER`; `targetInParty = true` → `..._GAP`; `sinceLastMs = 1000` → `..._RATE`; `actionsInWindow = 5` → `PARTYMANAGE_OK`; `hasLast = false, sinceLastMs = 0, actionsInWindow = 0` → `PARTYMANAGE_OK` (önceki eylem yoksa boşluk beklenmez); `hasLast = false, actionsInWindow = 6` → `..._RATE`.
- `Combat_PartyManageCheck_Boundaries`: `isLeader = true, targetInParty = true, hasLast = true, sinceLastMs = 999, actionsInWindow = 0` → `..._GAP`; `1000` → `OK`; `kPartyManageGapMs == 1000`; `sinceLastMs = 1000`: `actionsInWindow = 5` → `OK`, `6` → `..._RATE`; yalnızca `isLeader = false` (diğerleri geçerli) → `..._LEADER`; yalnızca `targetInParty = false` → `..._MEMBER`.

Beklenen toplam test sayısı: 39 + 2 = **41**.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'de `m_partyEnteredAt`'in altına (aynı yorum/hizalama biçimi):

```cpp
	bool m_partyManageHasLast;                             // IOCP thread only: m_partyManageLast is valid for this spawn
	std::chrono::steady_clock::time_point m_partyManageLast;   // IOCP thread only: when the last party promote / kick went out
```

`BotSession.cpp`: başlatıcı listesinde `m_partyEnteredHasAt(false)`'tan sonra `m_partyManageHasLast(false)` ekle (üye bildirim sırasıyla aynı olmalı; derleyici sıra uyarısı vermemeli). `ResetForRespawn()` içine `m_partyManageHasLast = false;` ekle (`m_partyEnteredHasAt = false;` satırının yanına). **`OnPacket()`'e dokunma.**

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

**Başlık (`ActionExecutor.h`):** `PartyInviteTarget`'ın altına:

```cpp
// Caller-supplied view of a party member the leader acts on (ADR-0017 Ek F4-10). Temporary like PartyInviteTarget: the
// /bot ppromote and /bot pkick test driver fills it from the two bot sessions; the Perception slice replaces the source.
struct PartyMemberTarget
{
	int16 id;             // target's socket id (the packet payload)
	bool inMyParty;       // the target is in the acting bot's party (the party panel lists it)
};
```

`PartyOutcome::reason` yorumuna yeni değerleri ekle (SENT: `"promoted"` (PartyPromote), `"kicked"` / `"disbanded"` (PartyKick); REFUSED: `"not_in_party"` (zaten var), guard: `"not_leader"` (zaten var), `"not_member"`, `"manage_gap"`; FAILED: `"no_result"` (zaten var); `peerId`: PartyPromote/PartyKick → hedefin kimliği). `class ActionExecutor` içine (`RequestPartyLeave` bildiriminin altına, aynı yorum kalıbıyla):

```cpp
	// One-shot leader handover (PARTY_PROMOTE + the target's id through CUser::HandlePacket()) after the guard (CLI-17:
	// leader only, target in the bot's party, >= 1 s between leader actions; CLI-11). Preconditions without an event:
	// REFUSED "not_in_game", "dead", "bad_target" (invalid id or the bot itself), "not_in_party". Result only from
	// published replies: the server broadcasts the new leader's member packet (PARTY_INSERT, sid == target, flag 100) to
	// every member including the sender -> SENT "promoted"; none -> FAILED "no_result".
	static PartyOutcome RequestPartyPromote(BotSession * s, const PartyMemberTarget & target,
		std::chrono::steady_clock::time_point now);

	// One-shot kick (PARTY_REMOVE + the target's id through CUser::HandlePacket()) after the same guard and preconditions
	// as RequestPartyPromote. Result only from published replies: the sender's own PARTY_REMOVE with sid == target ->
	// SENT "kicked"; PARTY_DELETE (only the leader remained) -> SENT "disbanded"; neither -> FAILED "no_result".
	static PartyOutcome RequestPartyKick(BotSession * s, const PartyMemberTarget & target,
		std::chrono::steady_clock::time_point now);
```

**`ActionExecutor.cpp`:** yalnızca **dosya sonuna** ekleme (bölüm başlığı `// --- party promote / kick slice (ADR-0017 Ek F4-10) ---`); F4-08/F4-09 koduna **dokunulmaz**:

- **`static PartyOutcome RejectPartyManage(BotSession * s, CUser * user, const char * type, BotCore::PartyManageVerdict verdict, const BotCore::PartyManageCheck & c, int peerId)`** (`RejectPartyInvite` kalıbı): `LEADER` → `rule "CLI-17", reason "not_leader", value 0, limit 1`; `MEMBER` → `"CLI-17", "not_member", value 0, limit 1`; `GAP` → `"CLI-17", "manage_gap", value = sinceLastMs, limit = kPartyManageGapMs`; `RATE` → `"CLI-11", "rate", value = actionsInWindow, limit = kMaxActionsPerWindow`. `decisionId = NextDecisionId(s)`; `EmitFairnessReject(s, user, decisionId, type, ...)`; `REFUSED` + reason + `peerId`. Sunucuya paket **gitmez**.
- **`static PartyOutcome RequestPartyManage(BotSession * s, const PartyMemberTarget & target, std::chrono::steady_clock::time_point now, bool kick)`** (iki genel fonksiyonun ortak gövdesi; `type = kick ? "PartyKick" : "PartyPromote"`):
  - `s == nullptr || s->m_pUser == nullptr || !isInGame()` → `REFUSED "not_in_game"`; `isDead()` → `REFUSED "dead"` (olay yazılmaz).
  - **Ön koşullar (olay yazılmaz):** `target.id < 0 || target.id == (int16)user->GetID()` → `REFUSED "bad_target"`; `!user->isInParty()` → `REFUSED "not_in_party"`. (`isInParty()` botun **kendi** durumudur; yalnızca ön koşul, `HandlePacket`'tan **önce**.)
  - `nowMs`, `inWindow`, `sinceLastMs` (`m_partyManageHasLast` ise `now - m_partyManageLast` ms, değilse 0). `PartyManageCheck c = { user->isPartyLeader(), target.inMyParty, s->m_partyManageHasLast, sinceLastMs, inWindow }`; `verdict != PARTYMANAGE_OK` → `RejectPartyManage` (`peerId = target.id`).
  - **Gönder:** `decisionId = NextDecisionId(s)`; `ACTION_SUBMIT` (`decisions`): `"decision_id","type":"PartyPromote"|"PartyKick","target":<id>` (**`"mode"` anahtarı kullanma**: telemetri ortak `"mode":"live"` alanıyla çakışır, F4-08 Tur 1 bulgusu). Paket: `Packet pkt(WIZ_PARTY, uint8(kick ? PARTY_REMOVE : PARTY_PROMOTE)); pkt << uint16(target.id);` (`PartyHandler.cpp:34-43`: `u8 alt-opcode + u16`). `s->m_castSelfId = user->GetID();` ve kayıtları sıfırla: promote için `s->m_partyJoinEcho = 0;`, kick için `s->m_partyLeaveEcho = 0;` → `user->HandlePacket(pkt)` (gecikme `steady_clock` ile) → `s->m_actionWindow.Record(nowMs); s->m_partyManageHasLast = true; s->m_partyManageLast = now;`.
  - **Sonuç (yalnızca cevap paketinden):** promote: `j = s->m_partyJoinEcho.load()`; geçerli (bit 63) ve `(int)((j >> 8) & 0xFFFF) == (int)target.id` ve `(uint8)(j & 0xFF) == 100` → `SENT "promoted"`; aksi halde `FAILED "no_result"`. Kick: `l = s->m_partyLeaveEcho.load()`; geçerli ve `((l >> 16) & 0xFF) == 1` ve `(int)(l & 0xFFFF) == (int)target.id` → `SENT "kicked"`; geçerli ve `((l >> 16) & 0xFF) == 2` → `SENT "disbanded"` ve `s->m_partyEnteredHasAt = false;` (lider artık party'de değil); aksi halde `FAILED "no_result"`.
  - `ACTION_RESULT`: `"decision_id","type","ok","reason","latency_us","target":<id>`. `peerId = target.id`.
- **`ActionExecutor::RequestPartyPromote`** = `return RequestPartyManage(s, target, now, false);` ve **`RequestPartyKick`** = `return RequestPartyManage(s, target, now, true);`.

`ActionExecutor.cpp`'de (yorumlar dahil) `PARTY_DELETE` dizgesi **geçmez** (K6 grep'i takılır; "disband" sonucu `m_partyLeaveEcho` türünden okunur). `HandlePacket(pkt)` çağrısı bu planda **tek yerde** (`RequestPartyManage`). `CUser::PartyProcess/PartyRequest/PartyInsert/PartyCancel/PartyPromote/PartyRemove/PartyDelete`, `m_bInParty`, `m_bPartyLeader`, `m_sPartyIndex`, `GetPartyID`, `GetPartyPtr`, `CreateParty`, `StateChangeServerDirect` ActionExecutor'da **yok**.

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirim (`CommandPartyLeave`'in altına): `void CommandPartyManage(const std::string & args, bool kick);` (IOCP thread only).

1. **`ExecuteCommand()`:** `pleave` dalının yanına `ppromote` (`CommandPartyManage(args, false)`) ve `pkick` (`CommandPartyManage(args, true)`) fiillerini ekle; "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast, pot, sit, stand, target, regene, pinvite, paccept, pdecline, pleave, ppromote, pkick)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **Yardımcı (aynı dosyada, `CommandPartyManage`'in hemen üstünde, `static`):**
   ```cpp
   // Test driver: the party panel lists who is in the leader's party. Until the Perception slice, membership is read
   // from the two bot sessions (guard input only, never a result).
   static bool IsSamePartyMember(CUser * leader, CUser * target)
   {
   	return leader->isInParty() && target->isInParty() && leader->GetPartyID() == target->GetPartyID();
   }
   ```
   `GetPartyID()` bot kodunda **yalnızca burada** geçer.
3. **`CommandPartyManage(args, kick)`** `CommandPartyInvite` kalıbıyla (iki bot: `<bot> <hedef bot>`; `SplitWords`, `FindSession`, `IsKnownBotName`, `PhaseName`, `WriteBotLog`; `const char * verb = kick ? "pkick" : "ppromote";`). Kullanım: `BotManager: cmd <verb>: usage: <verb> <bot> <target bot>` (`words.size() != 2` iken tek satır ve dön). Hata günlükleri `CommandPartyInvite`'taki gibi (`unknown or not spawned bot '<ad|?>'`, `<bot> not in game (phase X)`, `target <bot> not in game (phase X)`; hepsi `cmd <verb>:` önekiyle). Sonra `PartyMemberTarget tv = { (int16)target->m_pUser->GetSocketID(), IsSamePartyMember(s->m_pUser, target->m_pUser) };` ve `ActionExecutor::RequestPartyKick(s, tv, now)` / `RequestPartyPromote(s, tv, now)`. Sonuç günlüğü: `REFUSED` → `BotManager: cmd <verb>: <bot> refused (<reason>)`; `SENT` → `BotManager: cmd <verb>: <bot> <reason> <target bot>` (yani `promoted`, `kicked` ya da `disbanded` + hedef adı); `FAILED` → `... failed (<reason>)`. `char message[256]`.
4. **`TickSessions()`, `BeginDespawn()`, `BuildStatusLines()`:** **değişmez** (tek seferlik aksiyonlar, `list` biçimi sabit). Başka hiçbir yere dokunma (`ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj`, `.filters`, `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` **değişmez** (yeni dosya yok). Mevcut dosyaların kodlama/satır sonu/BOM durumu korunur.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı iki yeni test adını (`Combat_PartyManageCheck_Order`, `Combat_PartyManageCheck_Boundaries`) içerir ve toplam test sayısı **41**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; dosyada `#include` yalnızca `<algorithm>` ve `<cstdint>`; `grep -n "std::min\|std::max" BotCore/BotCombat.h` yeni satır göstermez.
- [ ] K5: party paketleri yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_PARTY" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma: F4-08/F4-09'un satırları + yeni `RequestPartyManage`) ve `BotSession.cpp` (`OnPacket` kayıt) satırlarını gösterir; `grep -nE "(->|\.)Party(Request|Insert|Cancel|Promote|Process|Remove|Delete)\(|CreateParty|GetPartyPtr|m_bInParty|m_bPartyLeader|m_sPartyIndex|StateChangeServerDirect" GameServer/Bot/*.cpp GameServer/Bot/*.h` boş; `grep -n "GetPartyID" GameServer/Bot/*.cpp GameServer/Bot/*.h` **tek satır** gösterir ve o `BotManager.cpp` içindeki `IsSamePartyMember`'dadır (`ActionExecutor.*`'de yok).
- [ ] K6: guard atlanmıyor: `RequestPartyManage`'da `HandlePacket`'tan önce `bad_target`/`not_in_party` ön koşulları, `CheckPartyManage` çağrısı ve `PARTYMANAGE_OK` dışında erken dönüş; `WIZ_PARTY` için `HandlePacket(pkt)` çağrısı `RequestPartyManage`'da **tek**; devir paketi `PARTY_PROMOTE` + `uint16(target.id)`, atma paketi `PARTY_REMOVE` + `uint16(target.id)` (devirde `PARTY_REMOVE`, atmada `PARTY_PROMOTE` kullanılmaz); `grep -n "PARTY_DELETE" GameServer/Bot/ActionExecutor.cpp` boş.
- [ ] K7: sonuç yalnızca cevap paketinden: `RequestPartyManage` gövdesinde sonuç kararı yalnızca `m_partyJoinEcho` (devir) ve `m_partyLeaveEcho` (atma) kayıtlarından verilir; `isInParty()`/`isPartyLeader()` yalnızca guard girdisi/ön koşul olarak ve `HandlePacket`'tan **önce** geçer (`grep -n "isInParty\|isPartyLeader" GameServer/Bot/ActionExecutor.cpp` yalnızca F4-08/F4-09 satırlarını ve `RequestPartyManage`'in iki girdisini gösterir); `m_sHp`, `m_iMaxHp`, `GetHealth`, `GetMaxHealth` bu fonksiyonların satır aralığında geçmez.
- [ ] K8: mevcut kod yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-10 -- GameServer/Bot/ActionExecutor.cpp | grep '^-' | grep -v '^---'` **boş**; `git diff gece/2026-10-02...bot/F4-10 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki bilinçli değiştirilen satır(lar)ı gösterir ve `OnPacket()` bloğunda diff **yok**.
- [ ] K9: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutla çalışır; `git diff gece/2026-10-02...bot/F4-10 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca `unknown command` mesaj satırını gösterir; `Startup()`/`Tick()`/`TickSessions()`/`BuildStatusLines()`/`BeginDespawn()` ve ini okuma değişmedi.
- [ ] K10: `git diff --stat gece/2026-10-02...bot/F4-10` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K11: değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K12: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(`, `SByte`, `DByte` yok; yeni `ACTION_SUBMIT`/`ACTION_RESULT` alan dizgilerinde `"mode"` anahtarı yok (`grep -n '\\"mode\\"' GameServer/Bot/ActionExecutor.cpp` boş).
- [ ] K13: F4-01..F4-09 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack" ...` ≥ 1, `grep -c "CheckCastStart" ...` ≥ 1, `grep -c "CheckPotion" ...` ≥ 1, `grep -c "CheckStance" ...` ≥ 1, `grep -c "CheckTargetHp" ...` ≥ 1, `grep -c "CheckRegene" ...` ≥ 1, `grep -c "CheckPartyInvite" ...` ≥ 1, `grep -c "CheckPartyAccept" ...` ≥ 1, `grep -c "CheckPartyDecline" ...` ≥ 1, `grep -c "CheckPartyLeave" ...` ≥ 1, `grep -c "CheckPartyManage" ...` ≥ 1; önceki 39 testin tamamı hâlâ geçiyor; `EmitFairnessReject` çağrıları `"Move"`/`"Attack"`/`"Cast"`/`"Potion"`/`"State"`/`"TargetHp"`/`"Regene"`/`"PartyInvite"`/`"PartyAccept"`/`"PartyDecline"`/`"PartyLeave"` geçiyor, yeni `PartyPromote`/`PartyKick` tipleri `RejectPartyManage`'a parametre olarak giriyor.
- [ ] K14 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–8 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-10
git diff gece/2026-10-02...bot/F4-10 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-10 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-10 -- GameServer/Bot/ActionExecutor.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-10 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "std::min\|std::max" BotCore/BotCombat.h
grep -n "WIZ_PARTY" GameServer/Bot/*.cpp
grep -nE "(->|\.)Party(Request|Insert|Cancel|Promote|Process|Remove|Delete)\(|CreateParty|GetPartyPtr|m_bInParty|m_bPartyLeader|m_sPartyIndex|StateChangeServerDirect" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "GetPartyID" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "PARTY_DELETE" GameServer/Bot/ActionExecutor.cpp
grep -n "CheckPartyManage\|HandlePacket\|isInParty\|isPartyLeader\|PARTY_PROMOTE" GameServer/Bot/ActionExecutor.cpp
grep -n "m_sHp\|m_iMaxHp\|GetHealth\|GetMaxHealth" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(\|SByte\|DByte" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
grep -n '\\"mode\\"' GameServer/Bot/ActionExecutor.cpp
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-10
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. AIServer bağlı olmalıdır (`AG_USER_PARTY`). Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; **bir dosyadaki satırlar aynı `Tick()`'te sırayla çalışır**, ayrı dosyalar ≥ 1,05 sn arayla verilmelidir; `manage_gap` testi için bu önemlidir). Botlar: Karus `BotWP_K`, `BotMF_K`, `BotPHD_K`, `BotWG_K`; zone 71, aynı bölge. Gözlem `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'dan. Senaryolar:

1. **Kurulum:** `pinvite BotWP_K BotMF_K` → `created`; ≥ 1,05 sn sonra `paccept BotMF_K` → `joined`; `pinvite BotWP_K BotPHD_K` → `sent`; `paccept BotPHD_K` → `joined` (üç kişilik party, lider `BotWP_K`).
2. **Devir:** `ppromote BotWP_K BotMF_K` → `promoted` (`ACTION_RESULT` `"reason":"promoted"`, `latency_us` ölçülür). Dolaylı kanıt: `pinvite BotMF_K BotWG_K` artık `sent` (yeni lider), `pinvite BotWP_K BotWG_K` `refused (not_leader)` (eski lider üye).
3. **Guard (party: lider `BotMF_K`, üyeler `BotWP_K`, `BotPHD_K`; her komut ≥ 1,05 sn arayla ayrı dosyada):** `pkick BotWP_K BotPHD_K` (WP artık üye) → `refused (not_leader)` + `FAIRNESS_REJECT` (`CLI-17`, `value` 0, `limit` 1); `ppromote BotMF_K BotWG_K` ve `pkick BotMF_K BotWG_K` (`BotWG_K` party'de değil) → `refused (not_member)` (paket gitmedi: üyelere sahte `PARTY_REMOVE` yayını ve party dağılması yok, KI-015 korunuyor); `ppromote BotMF_K BotMF_K` → `refused (bad_target)` (olaysız); `ppromote BotWG_K BotMF_K` (`BotWG_K` party'de değil) → `refused (not_in_party)` (olaysız).
4. **Boşluk ve atma:** aynı `BotCommands.txt` dosyasında iki satır (aynı `Tick()`): `pkick BotMF_K BotPHD_K` → `kicked`, hemen ardından `pkick BotMF_K BotWP_K` → `refused (manage_gap)` (`value` < 1000, `limit` 1000; hedef hâlâ üye, lider hâlâ lider, paket gitmedi). `BotPHD_K`'ya `pleave` → `refused (not_in_party)` (atılan gerçekten party dışında); `pinvite BotMF_K BotPHD_K` → `sent`, `paccept BotPHD_K` → `joined` (atılan takılı kalmadı; üç kişi yeniden).
5. **Devir geri ve dağılma:** ≥ 1,05 sn aralarla `ppromote BotMF_K BotWP_K` → `promoted`; `pkick BotWP_K BotPHD_K` → `kicked`; `pkick BotWP_K BotMF_K` → `disbanded` (yalnızca lider kaldı, `m_partyLeaveEcho` türü 2); sonra `pleave BotWP_K` ve `pleave BotMF_K` → `refused (not_in_party)`, `ppromote BotWP_K BotMF_K` → `refused (not_in_party)`.
6. **İki kişilik devir:** yeniden kur (`pinvite BotWP_K BotMF_K`/`paccept BotMF_K`), `ppromote BotWP_K BotMF_K` → `promoted`; ardından `pkick BotMF_K BotWP_K` → `disbanded`.
7. **Despawn/ENABLED/özet:** lider ve üye despawn temiz, sunucu çökmedi; `RESPAWN_CYCLES=2` ile komut reddedilir; `TELEMETRY=summary` → `ACTION_*` yok ama komutlar çalışır; `ENABLED=0` → dosya tüketilmez/log yok.
8. **Gerilemesiz:** F4-01..F4-09 komutları (`move`/`attack`/`cast`/`pot`/`sit`/`target`/`regene`/`pinvite`/`paccept`/`pdecline`/`pleave`) çalışır; `tick_p95_us` ≤ 500.

Beklenmeyen `no_result` bu planın hatası değil, **sonuç olarak raporlanır** (özellikle devir yayınının devredenin kendi alıcısına gelip gelmediği; gelmiyorsa **durup** raporla, bot sunucu nesnesine bakarak "başarılı" demez). Sınanmayan durumlar (öldürerek ölü bot vb.) raporda açıkça yazılır.

---

## Uygulayıcı Raporu

### Tur 1

- **Durum:** UYGULANDI
- **Branch / commit'ler:** `bot/F4-10` (taban: `gece/2026-10-02`); `ab1d2b0` `[F4-10] PartyPromote/PartyKick dilimi: guard, aksiyonlar ve komutlar`; bu rapor ve `Durum=UYGULANDI` ayrı bir commit'te.
- **Değişen dosyalar (8 kod + plan):**
  - `BotCore/BotCombat.h`: `namespace BotCore` içine `kPartyManageGapMs`, `PartyManageCheck`, `PartyManageVerdict`, `CheckPartyManage` eklendi (yalnızca ekleme; `#include` değişmedi).
  - `Tests/BotCoreTests/CombatTests.cpp`: `Combat_PartyManageCheck_Order` ve `Combat_PartyManageCheck_Boundaries` eklendi (toplam 41 test).
  - `GameServer/Bot/ActionExecutor.h`: `PartyMemberTarget` yapısı, `PartyOutcome::reason` yorumu, `RequestPartyPromote`/`RequestPartyKick` bildirimleri.
  - `GameServer/Bot/ActionExecutor.cpp`: dosya sonuna party promote/kick dilimi (`RejectPartyManage`, ortak `RequestPartyManage`, iki genel fonksiyon) eklendi; mevcut F4-08/F4-09 kodu değişmedi.
  - `GameServer/Bot/BotSession.h/.cpp`: `m_partyManageHasLast`/`m_partyManageLast` alanları, başlatıcı listesi ve `ResetForRespawn()` sıfırlaması; `OnPacket()` değişmedi.
  - `GameServer/Bot/BotManager.h/.cpp`: `CommandPartyManage` bildirimi/fiil dağıtımı, `IsSamePartyMember` yardımcısı, `unknown command` listesi.
- **Derleme:** `./tools/build.sh Release` ve `./tools/build.sh Debug` rc=0. Yeni/değişen dosyalarda uyarı yok; çıkan uyarılar yalnızca eski `GameServerDlg.cpp` satırlarından (816 C4834, 1143/1802 C4267). Son satırlar: `proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe`, `proj-GameServer.vcxproj -> ...\build\bin\x86-Debug\Server\GameServer.exe`.
- **Testler:** `./tools/run-tests.sh Release` ve `Debug` → `41 tests, 0 failed` (her ikisi rc=0); iki yeni test adı çıktıda göründü.
- **Kriter öz-değerlendirmesi:**
  - K1 ✔ (Release rc=0, değişen dosyalarda uyarı yok).
  - K2 ✔ (Debug rc=0).
  - K3 ✔ (iki yeni test, 41 test, iki konfigürasyonda da 0 failed).
  - K4 ✔ (`grep` boş; `std::min`/`std::max` yok; yeni include yok).
  - K5 ✔ (`WIZ_PARTY` yalnızca `ActionExecutor.cpp` + `BotSession.cpp`; party-iç yapı grep'i boş; `GetPartyID` tek satır, `BotManager.cpp:2086` `IsSamePartyMember`).
  - K6 ✔ (`HandlePacket(pkt)` `RequestPartyManage`'da tek, satır 2406; promote `PARTY_PROMOTE`+`uint16(target.id)`, kick `PARTY_REMOVE`+`uint16(target.id)`; `PARTY_DELETE` grep'i boş).
  - K7 ✔ (sonuç yalnızca `m_partyJoinEcho`/`m_partyLeaveEcho`; `isInParty`/`isPartyLeader` yalnızca eski satırlar + `RequestPartyManage`'in iki girdisi; HP alanları geçmiyor).
  - K8 ✔ (`ActionExecutor.cpp` silme yok; `BotSession.cpp` silme yok, `OnPacket()` diff yok).
  - K9 ✔ (`BotManager.cpp` silme yalnızca `unknown command` mesaj satırı).
  - K10 ✔ (`diff --stat` yalnızca §4'teki 8 dosya + plan; vcxproj'lar değişmedi).
  - K11 ✔ (`BotCombat.h` ve `CombatTests.cpp` ASCII + CRLF; `git diff --check` boş).
  - K12 ✔ (yasaklı diziler yok; `"mode"` anahtarı yok).
  - K13 ✔ (`CheckPartyManage` = 1, önceki guard'lar korunuyor; `EmitFairnessReject` yeni iki tipi `RejectPartyManage`'a parametre olarak alıyor).
  - K14 — Claude'un çalışma zamanı doğrulaması (bu raporun kapsamı dışında).
- **Plandan sapmalar:**
  - `RequestPartyManage` ve `RejectPartyManage` plan gereği dosya-yerel `static` yardımcılar olduğu için `ActionExecutor::` niteliksiz; yalnızca iki genel fonksiyon nitelikli. Plan da bunu böyle tanımlıyor.
  - `BotSession.cpp` başlatıcı listesinde `m_partyManageHasLast(false)` yeni satır olarak eklendi (silinen satır yok) — `m_partyEnteredHasAt(false)` sonrası, üye bildirim sırasıyla aynı.
  - `ActionExecutor.cpp` dosya-sonu başlığındaki yorumda `WIZ_PARTY` dizgesi kullanılmadı ("one party packet") ki K5 grep'i paket oluşturma satırlarını göstersin.
- **Açık sorular:** Yok. Çalışma zamanı doğrulamasını (K14, §7 senaryoları 1–8) Claude `/plan-dogrula`'da yapar; planın istediği gibi sunucu bu turda çalıştırılmadı.

### Tur 2

- **Durum:** UYGULANDI
- **Branch / commit'ler:** `bot/F4-10` (taban: `gece/2026-10-02`); bu turda yalnızca `GameServer/Bot/BotManager.cpp` değişti (düzeltme commit'i ayrı); bu rapor ve `Durum=UYGULANDI` ayrı bir commit'te. Diğer dosyalara dokunulmadı (`ActionExecutor.*`, `BotCombat.h`, testler, `docs/**` dahil).
- **Değişen dosyalar (1 kod + plan):**
  - `GameServer/Bot/BotManager.cpp` (`IsSamePartyMember`, ~`2082-2090`): imza `static bool IsSamePartyMember(CUser * leader, BotSession * target)`; dönüş artık `leader->isInParty() && target->m_pUser->isInParty() && leader->GetPartyID() == target->m_pUser->GetPartyID() && (target->m_partyInviteEcho.load() & (1ull << 63)) == 0` (hedefin bekleyen davet kaydı varsa üye sayılmaz; kayıt `PARTY_PERMIT`'te kurulur, kabul/ret'te temizlenir: `ActionExecutor.cpp:1989`, `:2143`). Üst yorum bu gerekçeyle (sunucu davet edileni kabulden önce party'de sayar, `PartyHandler.cpp:154-155`, KI-014) güncellendi. `CommandPartyManage`'deki çağrı `IsSamePartyMember(s->m_pUser, target)` oldu.
- **Derleme:**
  - `./tools/build.sh Release` rc=0; son satırlar: `proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe`, `proj-AIServer.vcxproj -> ...\AIServer.exe`, `BotCoreTests.vcxproj -> ...\BotCoreTests.exe`.
  - `./tools/build.sh Debug` rc=0; son satır: `proj-GameServer.vcxproj -> ...\build\bin\x86-Debug\Server\GameServer.exe`.
  - `BotManager.cpp` için uyarı **yok** (grep eşleşmesi 0). Kalan uyarılar yalnızca eski satırlardan: `GameServerDlg.cpp:816` C4834, `:1143`/`:1802` C4267, `UpgradeHandler.cpp:634`/`:862` C4789.
- **Testler:** `./tools/run-tests.sh Release` ve `Debug` → `41 tests, 0 failed` (her ikisi de; iki yeni test `Combat_PartyManageCheck_Order`/`_Boundaries` çıktıda göründü).
- **Düzeltme talimatı öz-değerlendirmesi (maddeler):**
  - 1 ✔ Yardımcı `BotSession * target` alıyor; dönüş tam olarak istenen ifade; üst yorum güncellendi; çağrı `IsSamePartyMember(s->m_pUser, target)`.
  - 2 ✔ Yalnızca `BotManager.cpp` değişti; başka dosyaya/fonksiyona dokunulmadı; `git status --short` tek satır `M GameServer/Bot/BotManager.cpp`.
  - 3 ✔ İki derleme hatasız; `BotManager.cpp` uyarısı yok; iki koşuda `41 tests, 0 failed`; `grep -n "GetPartyID" GameServer/Bot/*.cpp GameServer/Bot/*.h` tek satır (`BotManager.cpp:2088`); `git diff --check` boş (rc=0); `BotManager.cpp` `ASCII text, with CRLF line terminators`; sunucu çalıştırılmadı.
  - 4 ✔ `Durum` satırı `UYGULANDI`.
- **Plandan sapmalar:** Yok.
- **Açık sorular:** Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: **DÜZELTME GEREKLİ**
- İncelenen: `bot/F4-10` @ `46dcd74` (uygulama commit'i `ab1d2b0`; taban `gece/2026-10-02`; gece modu, `AUTO_LOOP=1`: birleştirme/push yapılmadı). Çalışma ağacı temizdi; sunucular kapalıydı.
- Kriter sonuçları:

| # | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `tools/build.sh Release` rc=0, `BotCombat.h`/`ActionExecutor.cpp`/`BotSession.cpp`/`BotManager.cpp`/`CombatTests.cpp` için `warning`/`error` yok |
| K2 | ✔ | `tools/build.sh Debug` rc=0, aynı dosyalarda uyarı yok |
| K3 | ✔ | `tools/run-tests.sh Release` ve `Debug` rc=0, `41 tests, 0 failed`; `Combat_PartyManageCheck_Order` ve `_Boundaries` `[ OK ]` |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` boş; `#include` yalnızca `<algorithm>`, `<cstdint>` (`:6-7`); eklenen satırlarda `std::min/max` yok |
| K5 | ✔ | `WIZ_PARTY` yalnızca `ActionExecutor.cpp:1840,1984,2139,2238,2396` ve `BotSession.cpp:109`; party-iç yapı grep'i boş; `GetPartyID` tek satır: `BotManager.cpp:2086` (`IsSamePartyMember`) |
| K6 | ✔ | `RequestPartyManage`: `bad_target`/`not_in_party` (`:2352-2362`) → `CheckPartyManage` `:2382` → `!= OK` erken dönüş → `HandlePacket` `:2406` (fonksiyonda tek); paket `PARTY_PROMOTE`/`PARTY_REMOVE` + `uint16(target.id)` (`:2396-2397`); `PARTY_DELETE` grep'i boş |
| K7 | ✔ | sonuç yalnızca `m_partyLeaveEcho` (atma) / `m_partyJoinEcho` (devir); `isInParty`/`isPartyLeader` yalnızca `:2359`, `:2376` (HandlePacket'tan önce); HP alanı grep'i boş |
| K8 | ✔ | `ActionExecutor.cpp` ve `BotSession.cpp` farkında silinen satır yok; `OnPacket()` diff'i yok |
| K9 | ✔ | `BotManager.cpp` silinen tek satır `unknown command` mesajı; diğer `Tick`/`Startup` vb. değişmedi |
| K10 | ✔ | `diff --stat`: yalnızca §4'teki 8 dosya + plan; hiçbir `.vcxproj`/`.filters` farkı yok; `GameServer/` içinde `Bot/` dışı fark yok |
| K11 | ✔ | 8 dosya `ASCII text, with CRLF line terminators`; `git diff --check` rc=0 |
| K12 | ✔ | yasaklı dizgiler (`printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(`, `SByte`, `DByte`) ve `"mode"` anahtarı `ActionExecutor.*`'de yok |
| K13 | ✔ | `CheckMoveStep` 2, diğer 11 guard 1 (`CheckPartyManage` dahil); önceki 39 test geçiyor |
| K14 | ✘ | Çalışma zamanı senaryo 1, 2, 4, 5, 6, 7, 8 geçti; **senaryo 3 (`not_member`) bekleyen davetli hedefte geçmedi** (bulgu 1) |

- **Çalışma zamanı (K14, Release, AIServer bağlı, `TELEMETRY=decisions`, zone 71; `BotWP_K` #2984, `BotMF_K` #2985, `BotPHD_K` #2986, `BotWG_K` #2987):**
  1. Kurulum: `pinvite WP MF` → `created`, `paccept MF` → `joined`, `pinvite WP PHD` → `sent`, `paccept PHD` → `joined` (3 kişi, lider WP).
  2. Devir: `ppromote BotWP_K BotMF_K` → `promoted` (`ACTION_RESULT` `ok:true`, `latency_us:82`). Dolaylı: `pinvite BotMF_K BotWG_K` → `sent`; `pinvite BotWP_K BotWG_K` → `refused (not_leader)`.
  3. Guard: `pkick BotWP_K BotPHD_K` → `refused (not_leader)` (`FAIRNESS_REJECT` `CLI-17 not_leader value:0 limit:1`); `ppromote BotMF_K BotMF_K` → `refused (bad_target)`. **`BotWG_K` bekleyen davetliyken** `ppromote BotMF_K BotWG_K` → `failed (no_result)` (`ACTION_SUBMIT` gitti), `pkick BotMF_K BotWG_K` → **`kicked BotWG_K`** (`ACTION_RESULT ok:true`), `ppromote BotWG_K BotMF_K` → `refused (not_leader)` (beklenen `not_in_party`): bulgu 1. `pdecline BotWG_K` ile davet kapatıldıktan sonra aynı üç komut beklenen sonucu verdi: `ppromote`/`pkick BotMF_K BotWG_K` → `refused (not_member)` (`FAIRNESS_REJECT` `CLI-17 not_member`), `ppromote BotWG_K BotMF_K` → `refused (not_in_party)`.
  4. Boşluk/atma: aynı dosyada `pkick BotMF_K BotPHD_K` → `kicked`, `pkick BotMF_K BotWP_K` → `refused (manage_gap)` (`value:0.00 limit:1000.00`, JSONL'de `ACTION_SUBMIT` yok); `pleave BotPHD_K` → `refused (not_in_party)`; `pinvite BotMF_K BotPHD_K` → `sent`, `paccept` → `joined`.
  5. Devir geri/dağılma: `ppromote BotMF_K BotWP_K` → `promoted`; `pkick BotWP_K BotPHD_K` → `kicked`; `pkick BotWP_K BotMF_K` → `disbanded` (`ACTION_RESULT reason:"disbanded"`); `pleave BotWP_K`/`pleave BotMF_K` → `refused (not_in_party)`; `ppromote BotWP_K BotMF_K` → `refused (not_in_party)`.
  6. İki kişilik: yeniden kurulum sonrası `ppromote BotWP_K BotMF_K` → `promoted`; `pkick BotMF_K BotWP_K` → `disbanded`.
  7. Komut/ömür: argümansız `ppromote BotWP_K` → kullanım satırı; `pkick Ghost BotMF_K`, `ppromote BotWP_K Ghost` → `unknown or not spawned bot '?'`; `despawn BotWP_K` sonrası `ppromote BotWP_K BotMF_K` → `not in game (phase despawned)`; lider ve üye despawn temiz, sunucu 3/3 UP kaldı, `list` doğru. Sınanmadı: `RESPAWN_CYCLES=2` reddi, `TELEMETRY=summary`, `ENABLED=0` (bu planda `Tick()`/ini okuma/`ExecuteCommand` reddi farkta yok), ölü bot (`refused (dead)`).
  8. Gerilemesiz: `sit`/`stand`, `regene` (`not_dead`), `target` (`observed ... hp 1541/1541`), `pdecline` (`no_invite`), `pinvite`/`paccept`/`pleave` önceki çıktıyı verdi; `PERF_SAMPLE` `tick_p95_us` normalde 15-379 (çoğu ~90); üç pencerede (spawn ve iki despawn) 1100-1164 µs, 4 bot toplu spawn/despawn ve logout kaydıyla çakışıyor (sonraki pencere 82-309), `skipped_ticks` 0, `dropped` 0.
- Bulgular (önem sırasına göre):
  1. **[Orta] `GameServer/Bot/BotManager.cpp:2084-2087` (`IsSamePartyMember`):** üyelik bilgisi `leader->isInParty() && target->isInParty() && GetPartyID()` eşitliğinden okunuyor, ama sunucu davet edileni daveti **kabul etmeden** "party'de" sayıyor (`PartyHandler.cpp:154-155`: `PartyRequest` hedefin `m_sPartyIndex` ve `m_bInParty`'sini davet anında yazar; KI-014). Bekleyen davetli bu yüzden `inMyParty = true` oluyor ve `not_member` guard'ı atlanıyor: `pkick` üye olmayan kimliğe gidiyor (KI-015: üyelere sahte `PARTY_REMOVE` yayını) ve gönderenin kendi alıcısına gelen o sahte yayın yüzünden `SENT "kicked"` raporlanıyor (yanlış pozitif; hedef gerçekte atılmadı, hâlâ bekleyen davetli); `ppromote` aynı yüzden `failed (no_result)` veriyor. CLI-17 `not_member` kuralının tam amacı bu durumu engellemekti. Kanıt: yukarıda senaryo 3; plan §7 senaryo 3'ün kendi sırası (senaryo 2'deki `pinvite BotMF_K BotWG_K` → `sent` daveti bekler durumda bırakır) bu durumu doğurur.
  2. **[Not]** Bekleyen davetli hedefin kendi tarafı: `ppromote BotWG_K BotMF_K` (WG bekleyen davetli) `not_leader` döner; plan `not_in_party` bekliyordu. Bu da KI-014 etkisi (`isInParty()` doğru döner) ve zararsızdır (paket gitmez, olay `FAIRNESS_REJECT`); davranışı değiştirme, yalnızca beklentiyi bil: senaryo sırasına `pdecline` eklenince `not_in_party` döner.
  3. **[Not]** Sınanmayanlar (yukarıda 7): kod yolları değişmeyen `Tick()`/ini okuma yüzünden gerilemesiz kabul edildi.
- Temizlik: `GameServer.ini` değişmedi (md5 `265a8e1c35ea12df46f6d006fe894d9b`); `BotCommands.txt` yok; `Logs/bots` → `Logs/bots_old_f410`; sunucular kapatıldı (`0/3 hazır`); DB'ye dokunulmadı (bot HP/konum değişmedi).
- **Düzeltme talimatı** (DeepSeek'e aynen verilecek)

```
plans/F4-10-aksiyon-yurutucu-party-devir-atma.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
1. GameServer/Bot/BotManager.cpp:2084-2087 (IsSamePartyMember): sunucu, davet edileni daveti kabul etmeden "party'de" sayar (PartyHandler.cpp:154-155, KI-014); bu yüzden bekleyen davetli hedef üye sanılıyor. Yardımcıyı hedefin BotSession'ını da alacak şekilde değiştir: static bool IsSamePartyMember(CUser * leader, BotSession * target). Dönüş: leader->isInParty() && target->m_pUser->isInParty() && leader->GetPartyID() == target->m_pUser->GetPartyID() && (target->m_partyInviteEcho.load() & (1ull << 63)) == 0 (hedefin bekleyen davet kaydı varsa üye değildir; kayıt PARTY_PERMIT'te kurulur, kabul/ret'te temizlenir: ActionExecutor.cpp:1989, :2143). Üst yorumu bu gerekçeyle güncelle (İngilizce, kısa). CommandPartyManage'deki çağrıyı IsSamePartyMember(s->m_pUser, target) yap. GetPartyID BotManager.cpp'de hâlâ yalnızca bu yardımcıda geçsin.
2. Başka dosyaya ve başka fonksiyona dokunma (ActionExecutor.*, BotCombat.h, testler, docs/** dahil). Plan §5.5'in diğer maddeleri aynı kalır.
3. ./tools/build.sh Release ve ./tools/build.sh Debug hatasız, BotManager.cpp için uyarı yok. ./tools/run-tests.sh Release ve Debug: 41 tests, 0 failed. grep -n "GetPartyID" GameServer/Bot/*.cpp GameServer/Bot/*.h tek satır göstermeli (BotManager.cpp). git diff --check boş; BotManager.cpp ASCII + CRLF kalmalı. Çıktıları raporuna yaz. Sunucuyu çalıştırma.
4. Durum satırını UYGULANDI yap.
```

### Tur 2 — 2026-10-02

- Karar: **DOĞRULANDI**
- İncelenen: `bot/F4-10` @ `a4eca90` (düzeltme commit'i `df6e2fd`; taban `gece/2026-10-02`; gece modu, `AUTO_LOOP=1`: birleştirme/push yapılmadı). Çalışma ağacı temizdi; sunucular kapalıydı.
- Tur 1 düzeltme talimatının kontrolü: değişiklik yalnızca `GameServer/Bot/BotManager.cpp` (`0fb3234..df6e2fd`: 7 ekleme, 4 silme); `IsSamePartyMember(CUser * leader, BotSession * target)` hedefin `m_partyInviteEcho` bit 63'üne de bakıyor (kayıt `BotSession.cpp:118`'de kurulur, `ActionExecutor.cpp:1989`/`:2143`'te kabul/ret'te temizlenir); çağrı `IsSamePartyMember(s->m_pUser, target)`; `GetPartyID` bot kodunda hâlâ tek satır (`BotManager.cpp:2088`). Başka dosya/fonksiyon değişmedi.
- Kriter sonuçları:

| # | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `tools/build.sh Release` rc=0; değişen 5 dosya için `warning`/`error` yok |
| K2 | ✔ | `tools/build.sh Debug` rc=0; aynı dosyalarda uyarı yok |
| K3 | ✔ | `tools/run-tests.sh Release` ve `Debug` rc=0, `41 tests, 0 failed`; `Combat_PartyManageCheck_Order` ve `_Boundaries` `[ OK ]` |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` boş; `#include` yalnızca `<algorithm>`, `<cstdint>` |
| K5 | ✔ | `WIZ_PARTY` yalnızca `ActionExecutor.cpp:1840,1984,2139,2238,2396` ve `BotSession.cpp:109`; party-iç yapı grep'i boş; `GetPartyID` tek satır `BotManager.cpp:2088` |
| K6 | ✔ | `RequestPartyManage`: `bad_target`/`not_in_party` (`:2352-2362`) → `CheckPartyManage` `:2382` → `HandlePacket` `:2406` (fonksiyonda tek); paket `PARTY_PROMOTE`/`PARTY_REMOVE` + `uint16(target.id)` (`:2396`); `PARTY_DELETE` grep'i boş |
| K7 | ✔ | sonuç yalnızca `m_partyJoinEcho`/`m_partyLeaveEcho`; `isInParty`/`isPartyLeader` guard girdisi olarak `:2359`, `:2376`; HP alanı grep'i boş |
| K8 | ✔ | `ActionExecutor.cpp` ve `BotSession.cpp` farkında silinen satır yok; `OnPacket()` diff'i yok |
| K9 | ✔ | `BotManager.cpp` silinen tek satır `unknown command` mesajı |
| K10 | ✔ | `diff --stat`: yalnızca §4'teki 8 dosya + plan, `STATUS.md`, `README.md`; hiçbir `.vcxproj`/`.filters` farkı yok |
| K11 | ✔ | 8 kod dosyası `ASCII text, with CRLF line terminators`; `git diff --check` kod dosyalarında boş (uyarılar yalnızca `docs/STATUS.md`'de, Claude'un kendi dosyası) |
| K12 | ✔ | yasaklı dizgiler ve `"mode"` anahtarı `ActionExecutor.*`'de yok |
| K13 | ✔ | `CheckMoveStep` 2, diğer 11 guard 1 (`CheckPartyManage` dahil); önceki 39 test geçiyor |
| K14 | ✔ | çalışma zamanı senaryo 1–8 geçti (aşağıda); Tur 1 bulgusu (bekleyen davetli) giderildi |

- **Çalışma zamanı (K14, Release, AIServer bağlı, `TELEMETRY=decisions`, zone 71; `BotWP_K` #2984, `BotMF_K` #2985, `BotPHD_K` #2986, `BotWG_K` #2987):**
  1. Kurulum: `pinvite WP MF` → `created`, `paccept MF` → `joined`, `pinvite WP PHD` → `sent`, `paccept PHD` → `joined`.
  2. Devir: `ppromote BotWP_K BotMF_K` → `promoted` (`ACTION_RESULT` `ok:true`, `latency_us:115`); `pinvite BotMF_K BotWG_K` → `sent` (yeni lider).
  3. **Tur 1 bulgusunun yeniden sınanması (`BotWG_K` bekleyen davetli):** `ppromote BotMF_K BotWG_K` → `refused (not_member)` (Tur 1: `failed (no_result)`); `pkick BotMF_K BotWG_K` → `refused (not_member)` (Tur 1: yanlış pozitif `kicked`); her ikisi `FAIRNESS_REJECT` `CLI-17 not_member value:0 limit:1`, `ACTION_SUBMIT` yok. `ppromote BotWG_K BotMF_K` → `refused (not_leader)` (bilinen KI-014 etkisi, Tur 1 bulgu 2 notu). Diğer guard'lar: `pkick BotWP_K BotPHD_K` → `refused (not_leader)`; `ppromote BotMF_K BotMF_K` → `refused (bad_target)`. `pdecline BotWG_K` sonrası: `ppromote BotWG_K BotMF_K` → `refused (not_in_party)`, `ppromote BotMF_K BotWG_K` → `refused (not_member)`.
  4. Boşluk/atma: aynı dosyada `pkick BotMF_K BotPHD_K` → `kicked`, `pkick BotMF_K BotWP_K` → `refused (manage_gap)` (`value:0.00 limit:1000.00`); `pleave BotPHD_K` → `refused (not_in_party)`; `pinvite BotMF_K BotPHD_K` → `sent`, `paccept` → `joined`.
  5. Devir geri/dağılma: `ppromote BotMF_K BotWP_K` → `promoted`; `pkick BotWP_K BotPHD_K` → `kicked`; `pkick BotWP_K BotMF_K` → `disbanded`; `pleave BotWP_K`/`pleave BotMF_K` ve `ppromote BotWP_K BotMF_K` → `refused (not_in_party)`.
  6. İki kişilik: yeniden kurulum, `ppromote BotWP_K BotMF_K` → `promoted`; `pkick BotMF_K BotWP_K` → `disbanded`.
  7. Komut/ömür: `ppromote BotWP_K` → kullanım satırı; `pkick Ghost BotMF_K` → `unknown or not spawned bot '?'`; `despawn all` sonrası 4 bot temiz (`names cleared yes`), `ppromote` → `not in game (phase despawned)`, `list` pool free 16/16, sunucu 3/3 UP kaldı. Sınanmadı: `RESPAWN_CYCLES=2` reddi, `TELEMETRY=summary`, `ENABLED=0`, ölü bot (`refused (dead)`); bu yollar farkta yok (Tur 1'deki gerekçe aynen geçerli).
  8. Gerilemesiz: `sit`/`stand`, `regene` (`not_dead`), `target` (`observed ... hp 1541/1541`), `pinvite`/`paccept`/`pdecline`/`pleave` beklenen çıktıyı verdi. `PERF_SAMPLE` `tick_p95_us` normalde 9–114 (bir pencere 397); 1091–1422 µs yalnızca 4 botun toplu spawn ve despawn pencerelerinde (Tur 1'deki örüntü; bu planın kodundan bağımsız), `skipped_ticks` 0, `dropped` 0.
- Bulgular: yok (engelleyici ya da önemli). **[Not]** `ppromote`/`pkick` bekleyen davetli *hedef* için artık doğru; bekleyen davetli *gönderen* (`ppromote BotWG_K ...`) hâlâ `not_leader` döner (`isInParty()` doğru, KI-014); paket gitmediği için zararsız.
- Temizlik: `GameServer.ini` değişmedi (md5 `265a8e1c35ea12df46f6d006fe894d9b`); `BotCommands.txt` yok; `Logs/bots` → `Logs/bots_old_f410b`; sunucular kapatıldı (`0/3 hazır`); DB'ye dokunulmadı.
