# F4-18: `Perception` dilim 7 — `TeamView`: party üyelerinin HP/MP, sınıf, seviye ve lider bilgisi (`PARTY_INSERT`/`PARTY_HPCHANGE`/`PARTY_REMOVE`/`PARTY_DELETE`) ve `/bot snap` çıktısı

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-18` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-17 (`SelfState` genişletme, `FillSelfExtras`, `/bot snap`) — `KAPANDI` (merge `6f66164`); F4-16 (`PerceptionSnapshot`, `BuildSnapshot`) — `KAPANDI`; F4-12 (`ObsTable`, `OnPacket()` gözlem kalıbı) — `KAPANDI`; F4-08..F4-10 (party aksiyonları: üyelik oluşturmak için) — `KAPANDI`; F3-05 (`BotCore`, birim test çatısı) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/13` §5.2 (`PerceptionSnapshot.team` = `TeamView`), `docs/14` §5.1 "Takım durumu" ve §5.2 gözlem sözleşmesi, `docs/03` §10 MEC-PTY-04 ve §16 ("Party üyelerinin HP ve MP'si — `PARTY_HPCHANGE` — Evet `[D]`"), AC-LRN-03 / AC-ARCH-06 (statik denetim) |
| Tahmini büyüklük | M (5 kod dosyası; yeni dosya yok, `*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

`PerceptionSnapshot`'ta `team` (`TeamView`) alanı henüz yok (`docs/13` §5.2). Karar katmanı (sonraki fazlar) "party arkadaşlarım kim, kimin HP'si düşük, lider kim" sorularını cevaplayabilmeli. Bu planda bot, **kendi alıcısına gelen** party paketlerinden (`WIZ_PARTY` alt-opcode'ları `PARTY_INSERT`, `PARTY_HPCHANGE`, `PARTY_REMOVE`, `PARTY_DELETE`) bir **takım tablosu** tutar ve `/bot snap` çıktısı bu tablodan kurulan `TeamView`'i (üye başına sınıf, seviye, HP/MP, lider, ölü, görüş alanında mı) gösterir.

Saf mantık (`BotCore/Perception.h`): bayt ayrıştırıcı, `TeamTable`, `BuildTeam`; hepsi birim testli. `BotSession` tabloyu `OnPacket()`'te, mevcut `m_obsLock` altında besler. Sunucunun party dizilerine (`_PARTY_GROUP`, `GetPartyPtr`, `uid[]`) ya da başka oyuncu nesnelerine **dokunulmaz**. Karar/guard/telemetri bu planda **yoktur**.

## 2. Bağlam (okunması zorunlu)

- `docs/14_LEARNING_AND_ADAPTATION.md` §5.1 (satır "Takım durumu: party üyelerinin HP/MP oranları, konumları, ölü/canlı… kaynak: party paketi") ve §5.2 gözlem sözleşmesi; `docs/03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md` §16 tablosu: party üyelerinin **HP ve MP**'si `PARTY_HPCHANGE` ile istemciye gelir, bot kullanabilir `[D]`. Düşmanın HP/MP'si yasak kalır; bu plan yalnızca **kendi party'sindeki** üyeleri okur.
- `docs/03` §10 (MEC-PTY-01..05): en çok 8 üye, slot 0 lider, lider ayrılırsa party silinir, MEC-PTY-04 HP/MP yayını.
- `docs/13` §5.2 — `PerceptionSnapshot` taslağı (`TeamView team;`). Uygulama notunu Claude günceller.
- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` Ek F4-17 madde 7 — "party HP (`PARTY_HPCHANGE`, `TeamView`)" sıradaki dilim olarak ayrılmıştı; Claude Ek F4-18'i yazar.
- İlgili kod (hepsini açıp doğrula; satırlar `6f66164` itibarıyla):
  - `shared/packets.h:238-250` — alt-opcode değerleri: `PARTY_PERMIT 0x02`, `PARTY_INSERT 0x03`, `PARTY_REMOVE 0x04`, `PARTY_DELETE 0x05`, `PARTY_HPCHANGE 0x06`, `PARTY_LEVELCHANGE 0x07`.
  - **Paket düzenleri** (`Packet`'in `<<` işleçleri: `uint8` 1 bayt, `uint16`/`int16` 2 bayt, `std::string` = **1 bayt uzunluk + metin** (`shared/ByteBuffer.h:59-71`; varsayılan tek bayt), hepsi little-endian; sunucu paket gövdesi `Packet::contents()` ile alt-opcode baytından başlar):
    - **Üye kaydı** — `GameServer/PartyHandler.cpp:233-241` (katılana, mevcut her üye için), `:254-260` (katılan dahil tüm üyelere yayın), `:315-326` (`PartyPromote`, bayrak 100): `u8 PARTY_INSERT | u16 sid | u8 flag | str name | i16 maxHp | i16 hp | u8 level | u16 class | i16 maxMp | i16 mp | u8 nation` (`m_iMaxHp`/`m_iMaxMp` `short`: `GameServer/User.h:237`; `m_sHp`/`m_sMp` `int16`: `:135`; `GetLevel()` `uint8`: `GameServer/Unit.h:93`; `GetClass()` `uint16`: `User.h:384`; `GetNation()` `uint8`: `Unit.h:92`). `flag` = 1 (katılma) ya da 100 (lider devri: "reset position to leader"). Toplam uzunluk `17 + name.size()` bayt.
    - **Ret/iptal** — aynı alt-opcode, **3 bayt**: `u8 PARTY_INSERT | i16 code` (`PartyHandler.cpp:80-82` ve `:164`). Üye kaydı **değildir**; mevcut `BotSession.cpp:123` aynı ayrımı `pkt.size() == 3` ile yapar.
    - **HP/MP değişimi** — `GameServer/User.cpp:2057-2065` (`SendPartyHPUpdate`, `Send_PartyMember` ile gönderene de gider): `u8 PARTY_HPCHANGE | u16 sid | i16 maxHp | i16 hp | i16 maxMp | i16 mp` = 11 bayt.
    - **Ayrılma/atma** — `PartyHandler.cpp:393-395`: `u8 PARTY_REMOVE | u16 sid` (3 bayt; atılan kişiye de gider, çünkü yayın üyelik silinmeden önce yapılır). **Dağılma** — `PartyHandler.cpp:433-434`: `u8 PARTY_DELETE` (1 bayt).
  - **Kim neyi alır:** `CGameServerDlg::Send_PartyMember` (`GameServer/GameServerDlg.cpp:1080-1093`) üyelerin hepsine, gönderen dahil, `CUser::Send` ile yollar; bot oturumunda bu çağrı `BotSession::OnPacket()`'e düşer. **Lider kendi kaydını almaz** (`PartyInsert` yalnızca *mevcut* üyelerin kaydını katılana yollar, liderin kendisi kendi kaydını görmez); katılan ise kendi kaydını yayından alır. Mevcut üyeler katılana **slot sırasıyla** (lider önce) gelir.
  - `GameServer/User.h:313` `isInParty()` ve `:319` `isPartyLeader()`: botun **kendi** bayrakları (satır içi okuma, başka nesneye dokunmaz).
  - `GameServer/Bot/BotSession.cpp:35-` `OnPacket()` ve içindeki mevcut `WIZ_PARTY` bloğu (`:105-146`: davet/ret/katılma/ayrılma yankıları, F4-08..F4-10) — **değişmez**; yeni kod ayrı blok olarak eklenir. `:172-` Perception blokları örnek (ayrıştırma kilit dışında, tablo `m_obsLock` altında). `:291-` `ResetForRespawn` tabloları `m_obsLock` altında temizler.
  - `GameServer/Bot/BotSession.h:148-152` `m_obsLock` + tablolar; `:158` `m_castSelfId` (yalnızca aksiyon sırasında yazılır, **güvenilir değil**: kendi kimliği için kullanma).
  - `GameServer/Bot/BotManager.cpp:2789-2793` oyuna giriş anı (`m_slotId = GetSocketID()`); `:2418-` `FillSelfExtras`; `:2467-` `CommandSnap` (kopyalama bloğu `:2500-2506`, `self` doldurma `:2509-2523`, `BuildSnapshot` `:2530`, yazdırma `:2540-` ve `enemies … npcs` toplam satırı `:2574`).
  - `BotCore/Perception.h`: `ByteReader` (`:37-`), `ObsTable` (`:254-`, `Find`/`At`/`Count`), `SelfState`/`PerceptionSnapshot` (`:807-`, `:861-`), `BuildSnapshot` (`:907`).
- **Önemli:** F4-16/F4-17 statik sözleşme denetimi (K5) `CommandSnap` ve `FillSelfExtras` gövdelerinde başka oyuncu/NPC/bölge/party **dizilerine** erişimi yasaklıyordu. Bu plan onu korur: takım bilgisi yalnızca `BotSession::m_team`'den (kendi paketlerinden dolmuş) okunur.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/Perception.h`):** sabitler; `TeamObs`, `PartyEvent`, `ParsePartyEvent`; `TeamTable` (`Apply`, `LeaderSid`); `TeamMemberView`, `TeamView`; `PerceptionSnapshot`'a `TeamView team;` alanı; `SelfState`'e `inParty`/`partyLeader`; `BuildTeam`. Beş birim testi.
2. **Oturum (`BotSession.{h,cpp}`):** `BotCore::TeamTable m_team` (`m_obsLock` altında, ikinci mutex yok), `std::atomic<int> m_selfSid` (botun kendi soket kimliği; oyuna girişte yazılır); `OnPacket()`'e **yeni** `WIZ_PARTY` bloğu; `ResetForRespawn` temizliği.
3. **Komut (`BotManager.cpp`):** oyuna girişte `m_selfSid` yazılır; `FillSelfExtras` `inParty`/`partyLeader` doldurur; `CommandSnap` takım tablosunu mevcut kopyalama bloğunda kopyalar, `BuildSnapshot`'tan sonra `BuildTeam` çağırır ve günlüğe bir özet + üye satırları yazar.
4. **Dokümantasyon ve kayıtlar Claude'un işi** (DeepSeek dokunmaz): ADR-0017 Eki F4-18, `docs/13` §5.2 notu, `docs/03` MEC-PTY-04 etiketi, STATUS, README.

**Kapsam dışı (yapılmayacak)**

- Alanları kullanan **karar, guard, politika, telemetri olayı** yok. Snapshot periyodik kurulmaz; yalnızca `/bot snap` kurar. `Tick()`/`TickSessions()` değişmez.
- `PARTY_LEVELCHANGE` (0x07), `PARTY_CLASSCHANGE`, `PARTY_STATUSCHANGE` (0x09), `PARTY_PERMIT` (zaten F4-08'de), party ilanı (`PARTY_BBS`/`PARTY_REGISTER`) **ayrıştırılmaz**: üye kaydındaki seviye/sınıf ilk kayıt anındaki değerdir. (Botlar seviye 80 sabit; ayrı dilim gerekirse eklenir, ADR Eki'nde not.)
- Party üyesinin **konumu** party paketinde gelmez: yalnızca üye aynı zamanda görüş alanında (`m_obs` tablosunda) ise konum/mesafe doldurulur; aksi halde `inView = false`. Gözlem tablosu (`ObsTable`) **değişmez**.
- Takım kararı, ortak hedef, rol, `TeamBlackboard` (botlar arası paylaşım), `NavView` yok.
- Yeni komut, yeni `GameServer.ini` anahtarı, yeni dosya, yeni `vcxproj` satırı yok. `ENABLED=0` iken davranış değişmez.
- `ActionExecutor.*`, `Telemetry.*`, `ScenarioRunner.*`, `BotManager.h`, sunucunun hiçbir dosyası **değişmez**; mevcut `WIZ_PARTY` bloğu (`BotSession.cpp:105-146`) ve F4-08..F4-10'un yankı alanları (`m_partyJoinEcho` vb.) **değişmez**.
- Dokümanlar (`docs/03`, `docs/13`, `docs/14`, `docs/16`, `docs/KNOWN_ISSUES.md`, ADR): Claude'un işi.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | yalnızca ekleme: yeni sabitler/yapılar/sınıf/yardımcılar; `SelfState`'in sonuna iki alan; `PerceptionSnapshot`'ın sonuna `team` alanı |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | yalnızca ekleme: beş `TEST_CASE` (dosya sonuna) |
| `GameServer/Bot/BotSession.h` | değiştir | yalnızca ekleme: `m_team`, `m_selfSid` |
| `GameServer/Bot/BotSession.cpp` | değiştir | kurucuda `m_selfSid(-1)`, `OnPacket()`'te yeni blok, `ResetForRespawn`'da temizlik |
| `GameServer/Bot/BotManager.cpp` | değiştir | giriş anında `m_selfSid`, `FillSelfExtras`'a iki satır, `CommandSnap` kopyalama + `BuildTeam` + yazdırma |

(Dokunulacak dosya sayısı 5; plan dosyası dahil 6.) `BotCore.vcxproj`, `BotCoreTests.vcxproj`, `proj-GameServer.vcxproj*`, `BotManager.h`, `ActionExecutor.*`, `Telemetry.*`, `ScenarioRunner.*`, `GameServer/` içinde `Bot/` dışındaki her dosya **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-18 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/Perception.h` ve birim testleri

Biçim: dosyadaki mevcut kod gibi (tab, Allman, `inline`, İngilizce kısa yorum, `namespace BotCore` içinde, ASCII + CRLF). **Yeni include yok** (`<cstddef>`, `<cstdint>`, `<cstring>`, `<cmath>` yeter). `std::min/max`, `std::vector`, `std::string`, `new`, `malloc` yok. Dosya sunucusuz kalır; sabitler `shared/packets.h` değerlerinin **kopyasıdır** (başlık dahil edilmez), yorumda kaynağı yazılır.

**(a) `SelfState`'e iki alan** (mevcut alanların **sonuna**, `cooldownTotal`'dan sonra):

```cpp
	bool         inParty;           // the bot's own CUser::isInParty()
	bool         partyLeader;       // the bot's own CUser::isPartyLeader()
```

**(b) Sabitler, ayrıştırıcı ve tablo** (`BuildSnapshot`'tan ve F4-17 yardımcılarından sonra, namespace kapanışından önce; başlık yorumu `// --- team view (ADR-0017 Ek F4-18) ---`). `kTeamNone` sabiti `0xFFFF`:

```cpp
constexpr int      kTeamMaxMembers = 8;     // MAX_PARTY_USERS (MEC-PTY-01)
constexpr uint16_t kTeamNone       = 0xFFFF;
constexpr uint8_t  kPartyInsert    = 3;     // packets.h PARTY_INSERT
constexpr uint8_t  kPartyRemove    = 4;     // PARTY_REMOVE
constexpr uint8_t  kPartyDelete    = 5;     // PARTY_DELETE
constexpr uint8_t  kPartyHpChange  = 6;     // PARTY_HPCHANGE
constexpr uint8_t  kPartyFlagJoin  = 1;     // member record flag: joined
constexpr uint8_t  kPartyFlagLeader = 100;  // member record flag: leader moved (PartyPromote)

struct TeamObs
{
	uint16_t sid;
	uint8_t  nation;
	uint8_t  level;
	uint16_t cls;
	int32_t  hp, maxHp, mp, maxMp;
	uint64_t lastSeenMs;         // caller's clock of the packet that last touched the member (record or HP change)
	char     name[kObsNameMax];  // NUL terminated
};

enum PartyEventKind
{
	PARTY_EV_NONE   = 0,
	PARTY_EV_MEMBER = 1,   // PARTY_INSERT member record (flag 1 or 100)
	PARTY_EV_HP     = 2,   // PARTY_HPCHANGE
	PARTY_EV_REMOVE = 3,   // PARTY_REMOVE
	PARTY_EV_DELETE = 4    // PARTY_DELETE
};

struct PartyEvent
{
	PartyEventKind kind;
	uint8_t  flag;               // MEMBER only
	uint16_t sid;                // HP / REMOVE (for MEMBER the sid is member.sid)
	int32_t  hp, maxHp, mp, maxMp;   // HP only
	TeamObs  member;             // MEMBER only
};

// Parses one WIZ_PARTY payload (starts at the sub-opcode byte). Returns true when 'out' holds a MEMBER / HP / REMOVE /
// DELETE event. Everything else returns false and leaves out.kind == PARTY_EV_NONE: other sub-opcodes, the 3-byte
// PARTY_INSERT refusal (i16 code), an unknown member flag (not 1 / 100), a truncated or over-long field (name > kObsNameMax - 1).
// Layouts: see the plan, section 2. Never reads out of bounds; data may be null with len 0.
inline bool ParsePartyEvent(const uint8_t * data, size_t len, uint64_t nowMs, PartyEvent & out);
```

`ParsePartyEvent` gövdesi: `memset(&out, 0, sizeof(out))`, `out.kind = PARTY_EV_NONE`. `ByteReader r(data, len)`; `uint8_t sub = r.U8()`; `!r.ok()` → false. Dallar:

- `sub == kPartyInsert`: `len == 3` → false (ret/iptal). `uint16_t sid = r.U16(); uint8_t flag = r.U8();` `flag` 1 ya da 100 değilse false; `r.Str(out.member.name, kObsNameMax)` başarısızsa false; `maxHp = (int16_t)r.U16()`, `hp = (int16_t)r.U16()`, `level = r.U8()`, `cls = r.U16()`, `maxMp = (int16_t)r.U16()`, `mp = (int16_t)r.U16()`, `nation = r.U8()`; `!r.ok()` → false (kısmi çıktı temizlenir: `memset` + `kind = NONE`). Başarı: `kind = PARTY_EV_MEMBER`, `flag`, `member.sid = sid`, `member.lastSeenMs = nowMs`, `member`'ın değerleri.
- `sub == kPartyHpChange`: `sid = r.U16()`, dört `(int16_t)r.U16()` (`maxHp`, `hp`, `maxMp`, `mp` sırasıyla); `!r.ok()` → false; `kind = PARTY_EV_HP`.
- `sub == kPartyRemove`: `sid = r.U16()`; `!r.ok()` → false; `kind = PARTY_EV_REMOVE`.
- `sub == kPartyDelete`: `kind = PARTY_EV_DELETE`, true (fazladan bayt umurunda değil).
- diğer: false.

HP/MP işaretli `int16`'dır (`m_sHp` `int16`); negatif değer olursa olduğu gibi `int32`'ye taşınır.

```cpp
// Copyable table of the party members the bot has been told about, keyed by sid. No mutex: the caller holds the lock.
// The bot's own record is stored too when the server sent one (members get their own record from the join broadcast;
// a leader never receives its own record) - BuildTeam skips it.
class TeamTable
{
public:
	TeamTable() { Clear(); }
	void Clear();                              // empties the table, forgets the leader hints, zeroes the counters
	int Count() const;
	uint32_t Overflow() const;                 // MEMBER events dropped because the table was full
	uint32_t UnknownHp() const;                // HP events for a sid the table does not know (ignored)
	const TeamObs * Find(uint16_t sid) const;
	const TeamObs & At(int i) const;
	// Applies one parsed event; returns true when the table changed. selfSid = the bot's own socket id (kTeamNone = unknown).
	//  MEMBER: flag 100 -> remember sid as the promoted leader; flag 1 into an EMPTY table -> remember sid as the first
	//          member (slot 0 = leader, see LeaderSid); then insert or refresh by sid. Full table and a new sid -> Overflow()++, false.
	//  HP:     update hp/maxHp/mp/maxMp/lastSeenMs of a known sid; unknown sid -> UnknownHp()++, false.
	//  REMOVE: sid == selfSid -> Clear() (the bot left or was kicked); otherwise remove the sid (clears the leader hints
	//          that pointed at it).
	//  DELETE: Clear().
	bool Apply(const PartyEvent & ev, uint16_t selfSid);
	// Leader resolution, first match wins: (1) the sid of the last flag-100 record; (2) selfSid when the bot itself is the
	// leader (selfIsLeader: a leader never receives its own record); (3) the sid that was first inserted into an empty
	// table (the joiner is sent the existing members in slot order, leader first) [A]; otherwise kTeamNone.
	uint16_t LeaderSid(uint16_t selfSid, bool selfIsLeader) const;
private:
	...
};
```

Alanlar: `TeamObs m_members[kTeamMaxMembers]; int m_count; uint32_t m_overflow, m_unknownHp; uint16_t m_promotedSid, m_firstSid;` (ikisi `kTeamNone` ile başlar). `Remove` son elemanla boşluğu kapatır (`ObsTable::Remove` gibi; sıra korunmaz). `Apply` MEMBER dalında "tablo boş mu" kontrolü **eklemeden önce** yapılır. Bir kayıt aynı sid için yenilenirken (`Upsert`) `m_firstSid`/`m_promotedSid` yalnızca yukarıdaki kurallara göre değişir.

**(c) Görünüm yapıları ve `BuildTeam`** (aynı bölüme, tablonun altına):

```cpp
// One other member of the bot's party. Fields the party packet gives (hp, mp, class, level, name) plus what the bot
// already sees on its own (position, only when the member is in view). No cooldowns, buffs, stock or inventory.
struct TeamMemberView
{
	uint16_t id;
	uint8_t  nation;
	uint8_t  level;
	uint16_t cls;
	int32_t  hp, maxHp, mp, maxMp;
	bool     leader;
	bool     dead;               // hp <= 0, or the observation table marks the member dead
	bool     inView;             // the member is in the observation table (x, z, dist valid)
	float    x, z, dist;         // 0 unless inView; dist to SelfState x, z
	uint32_t ageMs;              // nowMs - lastSeenMs of the last party packet that touched the member (clamped, 0 if the clock is ahead)
	char     name[kObsNameMax];
};

struct TeamView
{
	bool     inParty;            // SelfState.inParty; false -> everything below is empty
	bool     selfLeader;         // SelfState.partyLeader
	uint16_t leaderId;           // kTeamNone = unknown (also when not in a party)
	int      memberCount;        // entries filled (the table holds at most kTeamMaxMembers; the bot itself is never listed)
	int      memberTotal;        // other members in the table (== memberCount unless the capacity were exceeded)
	TeamMemberView members[kTeamMaxMembers];   // ascending id
};

// Zeroes 'out' (leaderId = kTeamNone), then, when self.inParty: leaderId = team.LeaderSid(self.sid, self.partyLeader);
// every table entry except self.sid becomes a TeamMemberView sorted by id ascending (insertion sort, no allocation);
// inView/x/z/dist come from obs.Find(id) (x10/10, z10/10, dist = sqrt(dx^2 + dz^2) in float), dead = hp <= 0 ||
// (inView && obs resHpType == kObsUserDead). When !self.inParty the table is ignored (it may still hold entries of an
// earlier party). Deterministic, no clock besides nowMs.
inline void BuildTeam(const SelfState & self, const TeamTable & team, const ObsTable & obs, uint64_t nowMs, TeamView & out);
```

`PerceptionSnapshot`'ın **sonuna** `TeamView team;` ekle (yorum: `// filled by BuildTeam, not by BuildSnapshot`). `BuildSnapshot` **değişmez** (`memset` `team`'i de sıfırlar: `leaderId` 0 olur, bu yüzden çağıran `BuildTeam` çağırmadan `snap.team`'e güvenmez; testlerde `BuildTeam` çağrılır).

**(d) Beş yeni `TEST_CASE`** (`PerceptionTests.cpp` dosya sonuna; mevcut `Buf` yardımcısı ve `MakeSelf()` kullanılır; `Buf`'a yeni yöntem **ekleme**, `I16` yerine `U16((uint16_t)değer)` kullan; üye kaydı kurmak için dosya-statik küçük bir yardımcı `AddPartyMember(Buf &, uint16 sid, uint8 flag, const char * name, int16 maxHp, int16 hp, uint8 level, uint16 cls, int16 maxMp, int16 mp, uint8 nation)` yaz, başına `0x03` alt-opcode baytını koy):

1. `Perception_Party_ParseMember`: geçerli kayıt (sid 7, flag 1, ad `"BotWP_K"`, `maxHp 3000`, `hp 2500`, level 80, cls 106, `maxMp 1200`, `mp 900`, nation 1) → `true`, `kind == PARTY_EV_MEMBER`, bütün alanlar (ad dahil) doğru, `lastSeenMs == nowMs`; flag 100 → `true`, `flag == 100`; flag 0 ve flag 2 → `false`; ret paketi (`0x03` + `U16(0xFFFF)`, 3 bayt) → `false`; kayıt son baytı kesilmiş (len − 1) → `false`; ad uzunluğu 24 (> kObsNameMax − 1) → `false`; `data == nullptr, len == 0` → `false`; `false` dönen her çağrıdan sonra `out.kind == PARTY_EV_NONE`.
2. `Perception_Party_ParseOthers`: HP paketi (`0x06`, sid 7, `3000, 2500, 1200, 900`) → `true`, `kind == PARTY_EV_HP`, `sid == 7`, dört değer doğru; negatif HP (`U16((uint16_t)-5)`) → `hp == -5`; HP paketi bir bayt kısa → `false`; `0x04` + `U16(9)` → `true`, `PARTY_EV_REMOVE`, `sid == 9`; `0x04` + tek bayt → `false`; `0x05` (tek bayt) → `true`, `PARTY_EV_DELETE`; `0x02` (PERMIT), `0x07` (LEVELCHANGE), `0x09` → `false`.
3. `Perception_Team_Table`: `Apply` ile üç üye (sid 7, 8, 9) ekle → `Count() == 3`; `Find(8)` alanlar doğru; HP olayı sid 8 → `hp`/`mp`/`lastSeenMs` güncellenir, `Count()` aynı; HP olayı sid 99 → `false`, `UnknownHp() == 1`; aynı sid 8 için ikinci MEMBER kaydı yenileme (sayı artmaz); REMOVE sid 7 (`selfSid` 1) → `Count() == 2`, `Find(7) == nullptr`; REMOVE `sid == selfSid` (ör. 8, `selfSid` 8) → `Count() == 0`; DELETE → `Count() == 0` ve sayaçlar sıfır; kapasite: sekiz üye + dokuzuncu yeni sid → `false`, `Overflow() == 1`, `Count() == 8`; dokuzuncu yerine mevcut sid yenilemesi `true`.
4. `Perception_Team_Leader`: (a) boş tablo: `LeaderSid(1, false) == kTeamNone`, `LeaderSid(1, true) == 1`; (b) boş tabloya ilk MEMBER (sid 7, flag 1), sonra sid 8 → `LeaderSid(1, false) == 7` (ilk kayıt), `LeaderSid(1, true) == 1` (bot lider); (c) flag-100 kaydı sid 8 → `LeaderSid(1, false) == 8` ve `LeaderSid(1, true) == 8` (devir önceliklidir); (d) 8 atılır (REMOVE) → promoted ipucu silinir: `LeaderSid(1, false) == 7` (ilk kayıt) ve 7 de atılırsa `kTeamNone`; (e) `Clear()` sonrası ipuçları sıfır; (f) boş olmayan tabloya sonradan gelen flag-1 kaydı ilk kayıt ipucunu **değiştirmez**.
5. `Perception_Team_Build`: `MakeSelf()` (sid 1, ulus 1, konum 1000,1000) + `inParty = true`, `partyLeader = false`; tabloya üç kayıt (sid 3, 2, 1 — **sid 1 botun kendisi**, sırayla `MEMBER` olaylarıyla; ilk eklenen 3): `BuildTeam` → `inParty`, `memberCount == 2`, `memberTotal == 2`, `members[0].id == 2`, `members[1].id == 3` (artan id), bot kendisi listede yok; `leaderId == 3` (ilk kayıt); `members[1].leader`, `!members[0].leader`; HP/MP/cls/level/ad aynen taşınır; gözlem tablosunda yalnızca sid 3 var (konum `x10 = 10300`, `z10 = 10400` → x = 1030, z = 1040, `dist` = 50): `members[1].inView`, `x/z/dist` doğru, `members[0].inView == false` ve `x == z == dist == 0`; `hp = 0` olan üye `dead`; gözlemde `resHpType = kObsUserDead` olan üye `dead`; `ageMs` hesabı (`nowMs − lastSeenMs`, saat geride ise 0); `self.partyLeader = true` → `leaderId == 1`, `selfLeader`; `self.inParty = false` → `inParty == false`, `memberCount == 0`, `leaderId == kTeamNone` (tablo dolu olsa da).

Toplam test sayısı **71 → 76**.

### 5.3 `GameServer/Bot/BotSession.{h,cpp}`

**(a) `BotSession.h`:** `m_obsLock` bloğundaki tabloların altına (`m_npcPending`'in hemen altına):

```cpp
	BotCore::TeamTable m_team;                             // guarded by m_obsLock (the same mutex as m_obs): the bot's party members, from received WIZ_PARTY packets only (Perception, ADR-0017 Ek F4-18)
```

ve atomik alanlar bölümüne (`m_castSelfId`'in altına):

```cpp
	std::atomic<int> m_selfSid;                            // written by BotManager::TickSessions() (IOCP thread) when the bot enters the game, read by OnPacket(): own socket id, -1 = none
```

Yeni `std::mutex` **yok** (`grep -c "std::mutex" BotSession.h` = 1 kalır).

**(b) `BotSession.cpp`:** kurucunun başlatma listesinde `m_castSelfId(-1)` yanına `m_selfSid(-1)`; `ResetForRespawn` içinde mevcut `m_castSelfId = -1;` satırının altına `m_selfSid = -1;` ve `m_obsLock` bloğunda `m_npcPending.Clear();`'in altına `m_team.Clear();`.

**(c) `OnPacket()` yeni blok:** mevcut `WIZ_PARTY` bloğunun (`:105-146`) **bitişinin** ve `WIZ_CHAT` bloğunun arasına, ayrı bir `if` olarak (mevcut bloğa **dokunma**), açıklayıcı İngilizce yorumla:

```cpp
	// Party team table (ADR-0017 Ek F4-18): member records, HP/MP changes, removals and disbands the server sends to this
	// session. Layouts: PartyHandler.cpp:233-260, :315-326, :393-395, :433-434, User.cpp:2057-2065. Parsed before the lock;
	// a REMOVE of the bot's own id (m_selfSid) empties the table. Nothing is read from the server's party arrays.
	if (opcode == WIZ_PARTY)
	{
		uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
		BotCore::PartyEvent ev;
		if (BotCore::ParsePartyEvent(pkt.size() > 0 ? pkt.contents() : nullptr, pkt.size(), nowMs, ev))
		{
			int selfSid = m_selfSid.load();
			std::lock_guard<std::mutex> lock(m_obsLock);
			m_team.Apply(ev, selfSid >= 0 ? (uint16_t)selfSid : BotCore::kTeamNone);
		}
	}
```

(`nowMs` adı başka bir blokta tanımlıysa kapsam çakışması olmaz: yeni blok kendi `{}` kapsamında.)

### 5.4 `GameServer/Bot/BotManager.cpp`

**(a)** `PHASE_WAIT_LOADED` → `PHASE_IN_GAME` geçişinde (`:2789-2792`), `s->m_slotId = s->m_pUser->GetSocketID();` satırının hemen altına bir satır: `s->m_selfSid = (int)s->m_pUser->GetSocketID();` (bu satır, aynı bloğun `isInGame()` doğrulamasından sonra çalışır; başka satıra dokunma).

**(b) `FillSelfExtras`** (`:2418-`): fonksiyonun **sonuna** (buff bloğundan sonra):

```cpp
	self.inParty = me->isInParty();
	self.partyLeader = me->isPartyLeader();
```

**(c) `CommandSnap`:**

1. `BotCore::NpcTable npcCopy;`'nin altına `BotCore::TeamTable teamCopy;`; kopyalama bloğunda `npcCopy = s->m_npcs;`'nin altına `teamCopy = s->m_team;`. Bloğun üstündeki mevcut yorum `(only these two assignments)` artık yanlış: **yalnızca o yorum satırını** `(only these three assignments)` yap (bu, diff'te izin verilen tek `-` satırıdır).
2. `BotCore::BuildSnapshot(self, obsCopy, npcCopy, nowMs, snap);` satırının hemen altına `BotCore::BuildTeam(self, teamCopy, obsCopy, nowMs, snap.team);`.
3. Yazdırma: cooldown satırlarını yazan döngüden **sonra**, `enemies … npcs` toplam satırından **önce** (mevcut satırların metni değişmez):

```
BotManager: cmd snap:   team in_party=%d self_leader=%d leader=%s members %d (total %d)
BotManager: cmd snap:   member id=%u name=%s class=%u lvl=%u hp=%d/%d mp=%d/%d %s%s dist=%.1f age=%ums    (en çok 8; "leader " öneki lider olana, "dead" / "alive", görüşte değilse dist yerine "out_of_view")
```

`leader=` değeri: `leaderId == kTeamNone` ise `unknown`, `leaderId == self.sid` ise `self`, aksi halde `id=<n>` (`snprintf` `%s` için sabit/yerel tampon; `char message[320]` yeter; ad en çok 23 karakter). Üye satırı biçimi (örnek): `member id=2985 name=BotMF_K class=107 lvl=80 hp=3100/3100 mp=1800/1800 leader alive dist=4.2 age=120ms` ya da `… alive out_of_view age=…ms`. Aynı satır biçimi her üye için; alan sırası yukarıdaki gibi, kısa İngilizce `snprintf`.

`CommandSnap` gövdesinde **yeni** `g_pMain`, `GetPartyPtr`, `GetPartyID`, `_PARTY_GROUP`, `uid[` geçmez; `m_obsLock` sayısı hâlâ 1.

### 5.5 Derleme ve test

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release`, `./tools/run-tests.sh Debug`. Değişen dosyaları `touch` edip yeniden derleyerek uyarı çıktısının boş olduğunu doğrula. Commit mesajı: `[F4-18] TeamView: party üyeleri tablosu ve /bot snap team çıktısı uygulandı`. Uygulayıcı Raporu'nu doldur, `Durum` satırını `UYGULANDI` yap.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter; `Perception.h`, `PerceptionTests.cpp`, `BotSession.h`, `BotSession.cpp`, `BotManager.cpp` için uyarı çıktısı boş (`touch` ile yeniden derlenip bakılır; yeni include eklemeden önce **durup** raporda sor).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı beş yeni test adını (`Perception_Party_ParseMember`, `Perception_Party_ParseOthers`, `Perception_Team_Table`, `Perception_Team_Leader`, `Perception_Team_Build`) içerir, toplam test sayısı **76**; `Debug` aynı; önceki 71 testin tamamı değişmeden geçer.
- [ ] K4: `BotCore/Perception.h` hâlâ sunucusuz: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h` boş (yorumlardaki `shared/packets.h` **dosya adı** anmaları da bu grep'e takılır: yorumda `shared/` yazmak yerine `packets.h` yaz); `grep -n "^#include" BotCore/Perception.h` yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`, `<cmath>`; `grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h` boş.
- [ ] K5: **sözleşme denetimi (AC-LRN-03) bozulmadı:** `sed -n '/struct UnitView/,/^\t};/p' BotCore/Perception.h | grep -inE "\b(max)?(hp|mp)\b|name|cooldown|stock|invent"` boş; aynı komut `struct NpcView` için boş; `sed -n '/struct TeamMemberView/,/^\t};/p' BotCore/Perception.h | grep -inE "cooldown|stock|invent|buff"` boş; `sed -n '/void BotManager::CommandSnap/,/^}/p' GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|GetPartyPtr|_PARTY_GROUP|uid\[|m_buffMap|m_CoolDownList"` boş; `sed -n '/static void FillSelfExtras/,/^}/p' GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|CNpc|GetPartyID|GetPartyPtr|_PARTY_GROUP|uid\[|m_CoolDownList|m_MagicTypeCooldownList"` boş; yeni `OnPacket` bloğunda sunucu erişimi yok: `sed -n '/Party team table (ADR-0017 Ek F4-18)/,/^\t}$/p' GameServer/Bot/BotSession.cpp | grep -E "g_pMain|GetUserPtr|GetPartyPtr|_PARTY_GROUP|uid\[|m_pUser"` boş.
- [ ] K6: kilit disiplini: `grep -c "std::mutex" GameServer/Bot/BotSession.h` = 1 (değişmedi); yeni blokta `m_obsLock` tam bir kez (`sed -n '/Party team table (ADR-0017 Ek F4-18)/,/^\t}$/p' GameServer/Bot/BotSession.cpp | grep -c m_obsLock` = 1) ve `ParsePartyEvent` çağrısı kilit **alınmadan önce**; `CommandSnap`'te `m_obsLock` sayısı hâlâ 1 (kopyalama bloğu); `m_team` yalnızca `m_obsLock` tutulurken yazılır/kopyalanır (`grep -n "m_team" GameServer/Bot/*.cpp` her satırı bir `lock_guard` bloğunun içindedir; `BotManager.cpp`'de tek geçiş kopyalama bloğu).
- [ ] K7: mevcut kod yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-18 -- BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` **boş**; `BotManager.cpp` farkında **tek** `-` satırı var ve o, `CommandSnap` kopyalama bloğunun üstündeki `(only these two assignments)` yorumudur (`git diff … -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca o satırı verir).
- [ ] K8: mevcut `WIZ_PARTY` bloğu ve F4-08..F4-10 yankıları aynı: `git diff … -- GameServer/Bot/BotSession.cpp` farkındaki eklenen satırların hiçbiri `m_partyInviteEcho|m_partyErrorEcho|m_partyJoinEcho|m_partyLeaveEcho|m_partyInviteAtMs` içermez (`ResetForRespawn`'daki mevcut satırlar zaten farkta değildir); `BuildSnapshot` gövdesi ve `SelfState`'in önceki alanları değişmedi (K7 ile aynı kanıt).
- [ ] K9: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `OnPacket()` (botlar yokken çağrılmaz) ve `/bot snap` ile çalışır; `Startup()`/`Tick()`/`TickSessions()` içinde yalnızca giriş anında **tek satır** (`m_selfSid`) eklendi; `BuildStatusLines()`/ini okuma değişmedi; yeni ini anahtarı yok; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K10: `git diff --stat gece/2026-10-02...bot/F4-18` yalnızca §4'teki 5 dosyayı (ve plan dosyasını) gösterir; `*.vcxproj*`, `BotManager.h`, `ActionExecutor.*`, `Telemetry.*`, `ScenarioRunner.*` farkta yok.
- [ ] K11: satır sonu/BOM bozulmadı (`file` çıktısı değişiklik öncesiyle aynı: ASCII + CRLF); `git diff --check` boş.
- [ ] K12: `GameServer/Bot/` içinde yeni `printf` (yalnızca `snprintf`), `Sleep`, `CreateThread`, `rand(` yok.
- [ ] K13: F4-01..F4-17 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2; `CheckAttack`, `CheckCastStart`, `CheckPotion`, `CheckStance`, `CheckTargetHp`, `CheckRegene`, `CheckPartyInvite`, `CheckPartyAccept`, `CheckPartyDecline`, `CheckPartyLeave`, `CheckPartyManage`, `CheckChat`, `CheckUserIn`, `CheckNpcIn` her biri ≥ 1; `BuildSnapshot` ve `BuildTeam` `CommandSnap`'ten çağrılır.
- [ ] K14 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–5 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-18
git diff gece/2026-10-02...bot/F4-18 -- BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'
git diff gece/2026-10-02...bot/F4-18 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'
git diff gece/2026-10-02...bot/F4-18 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj GameServer/Bot/BotManager.h GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h
grep -n "^#include" BotCore/Perception.h
grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h
sed -n '/struct UnitView/,/^\t};/p' BotCore/Perception.h | grep -inE "\b(max)?(hp|mp)\b|name|cooldown|stock|invent"
sed -n '/struct NpcView/,/^\t};/p' BotCore/Perception.h | grep -inE "\b(max)?(hp|mp)\b|name|cooldown|stock|invent"
sed -n '/struct TeamMemberView/,/^\t};/p' BotCore/Perception.h | grep -inE "cooldown|stock|invent|buff"
sed -n '/void BotManager::CommandSnap/,/^}/p' GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|GetPartyPtr|_PARTY_GROUP|uid\[|m_buffMap|m_CoolDownList"
sed -n '/static void FillSelfExtras/,/^}/p' GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|CNpc|GetPartyID|GetPartyPtr|_PARTY_GROUP|uid\[|m_CoolDownList|m_MagicTypeCooldownList"
sed -n '/Party team table (ADR-0017 Ek F4-18)/,/^\t}$/p' GameServer/Bot/BotSession.cpp | grep -E "g_pMain|GetUserPtr|GetPartyPtr|_PARTY_GROUP|uid\[|m_pUser"
sed -n '/Party team table (ADR-0017 Ek F4-18)/,/^\t}$/p' GameServer/Bot/BotSession.cpp | grep -c m_obsLock
sed -n '/void BotManager::CommandSnap/,/^}/p' GameServer/Bot/BotManager.cpp | grep -c m_obsLock
grep -c "std::mutex" GameServer/Bot/BotSession.h
grep -n "m_team" GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp
grep -n "printf\|Sleep\|CreateThread\|rand(" GameServer/Bot/BotManager.cpp GameServer/Bot/BotSession.cpp | grep -v snprintf
file BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp
git diff --check gece/2026-10-02...bot/F4-18
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=summary`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. **AIServer de açık olmalıdır** (`status` ile üç `[UP]`). Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`). Karus botları `BotWP_K`, `BotMF_K`, `BotPHD_K` (davet kuralları: aynı ulus, aynı zone 71, seviye aralığı MEC-PTY-02; hepsi seviye 80), spawn'lar arasında ~14 sn; `pinvite`/`paccept`/`ppromote`/`pkick`/`pleave` arasında ≥ 1,05 sn (CLI-15..CLI-17). Gözlem `Logs/Bot_*.log`. Envanter tablolarına (USERDATA vb.) **bakılmaz**. Senaryolar:

1. **İki kişilik party:** `spawn BotWP_K,BotMF_K,BotPHD_K` → `pinvite BotWP_K BotMF_K` → `paccept BotMF_K` → `snap BotWP_K`: `team in_party=1 self_leader=1 leader=self members 1 (total 1)` ve bir `member id=… name=BotMF_K …` satırı (HP/MP `list` çıktısındaki `BotMF_K` `hp=`/`mp=` ile aynı); `snap BotMF_K`: `in_party=1 self_leader=0 leader=id=<BotWP_K'nin id'si>` (ilk kayıt kuralı `[A]`), üye `BotWP_K`; `snap BotPHD_K`: `in_party=0 … members 0 (total 0)`.
2. **Üçüncü üye ve lider:** `pinvite BotWP_K BotPHD_K` → `paccept BotPHD_K` → `snap BotPHD_K`: iki üye (`BotWP_K` lider olarak işaretli, `BotMF_K`), `leader=id=<BotWP_K>`; `snap BotWP_K`: iki üye; `snap BotMF_K`: iki üye. Hepsi aynı kimlik kümesini görür.
3. **Lider devri:** `ppromote BotWP_K BotMF_K` → `snap BotPHD_K`: `leader=id=<BotMF_K>` ve `member … leader` öneki `BotMF_K` satırında; `snap BotMF_K`: `self_leader=1 leader=self`; `snap BotWP_K`: `self_leader=0 leader=id=<BotMF_K>`.
4. **HP/MP güncellemesi:** bir üyenin HP'si/MP'si değişir (yakın bir El Morad botuyla `move`+`attack` ya da `cast`, F4-02/F4-03 yolları; MP için `cast` hedef bota), kısa süre sonra diğer üyenin `snap`'inde o üyenin `hp=`/`mp=` değeri değişmiş ve `age` küçük; değişen değer aynı anda `list` çıktısındaki değerle tutarlı (en fazla birkaç yüz ms gecikme). HP değişimi sağlanamazsa bu senaryo "gözlenmedi" yazılır; birim testleri (`Perception_Party_ParseOthers`, `Perception_Team_Table`) + paket düzeni kod okuması kanıt olur, kriteri düşürmez.
5. **Ayrılma, atma, dağılma ve eski party kalıntısı:** `pkick BotMF_K BotPHD_K` → `snap BotPHD_K`: `in_party=0 members 0`, `snap BotMF_K`: bir üye; `pleave BotMF_K` (lider ayrılır, party dağılır) → üç botun `snap`'i `in_party=0 members 0`; ardından **yeni** party (`pinvite BotPHD_K BotWP_K`, `paccept BotWP_K`) → `snap BotPHD_K`: yalnızca `BotWP_K` (eski üyelerden kalıntı **yok**: tablo `DELETE`/kendi `REMOVE`'unda temizlendi); gerilemesiz: F4-01..F4-17 komutları çalışır (`move`/`attack`/`cast`/`pot`/`sit`/`target`/`regene`/`pinvite`/`paccept`/`pdecline`/`pleave`/`ppromote`/`pkick`/`pchat`/`see`/`npcs`/`snap`), `snap` bilinmeyen/çıkmış bot ve bot adsız çağrıda yalnızca kullanım/hata satırı yazar, `despawn all` temiz, `tick_p95_us` ≤ 500, `ENABLED=0` → komut dosyası tüketilmez/log yok. İnsan istemcisi gerekmez.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `Perception.h`, `PerceptionTests.cpp`, `BotSession.*`, `BotManager.cpp` ASCII + CRLF; yeni dosya yok.
- **Thread kuralı (`docs/13` §3):** `OnPacket()` her thread'den çağrılabilir; `m_team` yalnızca `m_obsLock` altında okunur/yazılır (ikinci mutex yok); ayrıştırma kilit **dışında** yapılır; kilit tutulurken günlük yazma ve başka kilit alma yok. `m_selfSid` atomiktir; `OnPacket()` botun kendi `CUser*`'ına (`m_pUser`, "IOCP thread only") **dokunmaz**. `CommandSnap` kopyayı kilit altında alır, `BuildSnapshot`/`BuildTeam`/`WriteBotLog` kilit bırakıldıktan sonra çalışır.
- **`SelfState` ve `PerceptionSnapshot` büyüdü** (`TeamView` ~ 8 × 70 bayt). `PerceptionSnapshot` yerel değişkendir; `static`/yığın dışı yapma, `kTeamMaxMembers`'ı artırma.
- **Tahmin `[A]`:** "ilk kayıt = lider" kuralı (`LeaderSid` madde 3) sunucu kodundan çıkarıldı (katılana mevcut üyeler slot sırasıyla gelir, `PartyHandler.cpp:216-241`), çalışma zamanında senaryo 1–3'te teyit edilir; teyit edilmezse `LeaderSid` madde 3 kaldırılır ve ADR Eki güncellenir (karar katmanı lideri zaten kendi `PartyPromote`/davet bilgisinden de bilir).
- **Bilinçli eksik:** seviye/sınıf değişimi ve party durum paketleri izlenmez; üyenin konumu yalnızca görüş alanındaysa vardır; `team` periyodik kurulmaz.
- **Sözleşme notu `[Ö]`:** party üyelerinin HP/MP'sinin istemcide party panelinde gösterildiği `docs/03` §16'da `[D]` (paket gelir); botun bunları kullanması insan oyuncunun bildiğiyle aynıdır.
- Beklenmedik bir şey görürsen (ör. paket uzunlukları plandaki düzenden farklı çıkarsa) **uydurma**: Uygulayıcı Raporu'na ham bayt dökümüyle yaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: —
- Branch / commit'ler: —
- Değişen dosyalar ve neden: —
- Derleme sonucu: —
- Kabul kriterleri öz-değerlendirme: —
- Plandan sapmalar ve gerekçeleri: —
- Açık sorular: —

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

—
