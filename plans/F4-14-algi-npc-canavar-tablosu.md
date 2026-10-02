# F4-14: `Perception` dilim 3 — görüş alanındaki NPC/canavar/kule tablosu (`WIZ_REQ_NPCIN`, `WIZ_NPC_INOUT`, `WIZ_NPC_MOVE`, `WIZ_NPC_REGION`, `WIZ_DEAD`) ve `/bot npcs`

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-14` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-12 (`ByteReader`, `ParseRegionList`, `ObsTable`, `OnPacket()` algı bloğu, `m_obsLock`) — `KAPANDI` (merge `dcd8f80`); F4-13 (`PendingIds`, `m_obsPending`) — `KAPANDI` (merge `f1acc48`); F3-05 (`BotCore`, birim test çatısı) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/14` §5.2 gözlem sözleşmesi, `docs/03` §16 (yeni NPC satırı), `docs/13` §5.2 (`PerceptionSnapshot` kaynağı), AC-LRN-03 / AC-ARCH-06 (statik denetim, F4-12 K5 kalıbı) |
| Tahmini büyüklük | M (6 kod dosyası; yeni dosya yok, `GameServer` projesine dosya eklenmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

F4-12 yalnızca **oyuncuları** gözlemler. Arena A'nın çevresinde sonucu belirleyen varlıklar ise NPC'lerdir: guard tower'lar (arama menzili 35 m, saldırı 20–30 m, `docs/03` §12.3), ulus askeri NPC'leri, canavarlar, kapılar. Gerçek istemci bunları `WIZ_REQ_NPCIN` (toplu kayıt), `WIZ_NPC_INOUT` (giriş/çıkış), `WIZ_NPC_MOVE` (hareket), `WIZ_NPC_REGION` (bölge değişiminde kimlik listesi) ve `WIZ_DEAD` (ölüm) paketlerinden öğrenir. Bu plan tamamlandığında bot alıcısına gelen bu paketlerden bir **NPC gözlem tablosu** kurulur ve `/bot npcs <bot>` komutu tabloyu günlüğe döker. Sonuç: `Perception`'ın sonraki dilimleri (`PerceptionSnapshot`, tower mesafesi, tehlike bölgesi) bota görünür her NPC'yi paketle öğrenmiş olur; sunucunun NPC dizilerine hiç dokunulmaz.

F4'ün on dördüncü dilimidir (ADR-0017 Ek F4-14). Sonraki dilimler: bölge değişiminde bilinmeyen NPC kimliklerini `WIZ_REQ_NPCIN` ile isteme (CLI-20, F4-15), `PerceptionSnapshot`, betikli test dizileri.

## 2. Bağlam (okunması zorunlu)

- `docs/14` §5.2 ve `docs/03` §16: bot yalnızca bir istemcinin öğrenebileceği bilgiyi kullanır. Bu plan "görüş alanındaki NPC'ler" için `docs/03` §16'ya eklenen satırı uygular (NPC paketleri istemciye gider `[D]`). Ekipman/silah kayıtları paketten **atlanır**.
- `docs/03` §12.3 (tower'lar, ulus NPC'leri), `docs/15` §2 (arena A'da tower menzili): tablonun asıl tüketicisi tower mesafesi ve tehlike bölgesi hesabıdır (bu planda tüketici yok).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `f1acc48` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/Npc.cpp:138-155` `CNpc::GetNpcInfo`: `pkt.SByte()` ile **dizgeler `u8` uzunluklu** olur. Alan sırası: `i16 protoId` (`m_sSid`), `i16 pictureId`, `u8 type`, `i32 sellingGroup`, `i16 size`, `i32 weapon1`, `i32 weapon2`, **ad** (`u8` uzunluk + bayt), `u8 nation` (canavarda 0), `u8 level`, `u16 x, u16 z, u16 y` (×10), `u32 gateOpen`, `u8 objectType`, `u16 0`, `u16 0`, `i8 direction`. Ad dışı sabit uzunluk 37 bayt, ad ile toplam `38 + n`.
  - `GameServer/Npc.cpp:90-97` `CNpc::GetInOut`: `WIZ_NPC_INOUT` yükü = **`u8` tip** (kullanıcıdaki gibi `u16` değil), `u16 id` + (tip ≠ `INOUT_OUT` ise) `GetNpcInfo`. `GameServer/Define.h:52-58` `INOUT_IN = 1, INOUT_OUT = 2, INOUT_RESPAWN = 3, INOUT_WARP = 4, INOUT_SUMMON = 5`.
  - `GameServer/Npc.cpp:73-81` `CNpc::MoveResult`: `WIZ_NPC_MOVE` yükü = `u16 id, u16 x, u16 z, u16 y, u16 speed` (10 bayt, konumlar ×10, **yankı baytı yok**).
  - `GameServer/GameServerDlg.cpp:1478-1539` `NpcInOutForMe` / `GetRegionNpcIn`: `WIZ_REQ_NPCIN` yükü = `u16 sayı` + sayı kez (`u16 id, GetNpcInfo`); **kullanıcı listesindeki gibi `u8 0` işaret baytı yoktur**. Ölü NPC'ler listeye girmez. `:1541-1556` `RegionNpcInfoForMe` / `:1591-1614` `GetRegionNpcList`: `WIZ_NPC_REGION` yükü = `u16 sayı` + sayı kez `u16 id` (düzen `WIZ_REGIONCHANGE` ile aynı, F4-12 `ParseRegionList` okur). İkisi de `SendCompressed` ile gider; `GameServer/User.cpp:28-39` bot alıcısına sıkıştırılmamış paketi verir.
  - `GameServer/User.cpp:1260-1298` `CUser::RequestNpcIn` (istemci isteği): cevabın başındaki sayı **istenen** sayıdır, bulunan değil (bulunamayan/ölü kimlikler atlanır) → ayrıştırıcı "sayı veriden büyük olabilir" durumunu hata saymaz (F4-15'te önemli). **Bu planda bot istek göndermez.**
  - `GameServer/CharacterSelectionHandler.cpp:269-271` `GameStart(1)`: bot girişinde `NpcInOutForMe(this)` çağrılır → bot spawn anında bölgesindeki NPC'lerin `WIZ_REQ_NPCIN` paketini alır. `GameServer/CharacterMovementHandler.cpp:37` bölge değişiminde `RegionNpcInfoForMe` (`WIZ_NPC_REGION`); `GameServer/AttackHandler.cpp:219` yeniden doğuşta aynı çağrılar.
  - `GameServer/Unit.cpp:956-964` `SendDeathAnimation`: `WIZ_DEAD` yükü = `u16 id` (bölge yayını, **NPC ve oyuncu için aynı**). `GameServer/Npc.cpp:319-342` `CNpc::OnDeath`: NPC ölünce `WIZ_DEAD` gider, NPC bölgeden **çıkarılır ama `WIZ_NPC_INOUT` OUT gönderilmez**; yeniden doğunca `SendInOut(INOUT_IN)` (`GameServer/AISocket.cpp:212`, `:341`) ile `WIZ_NPC_INOUT` IN gelir.
  - `GameServer/Define.h:67` `NPC_BAND = 10000`: NPC kimlikleri oyuncu kimliklerinden ayrık aralıktadır (`WIZ_DEAD` iki tablodan en çok birine uyar).
  - `shared/packets.h` (ISO-8859: `grep -a`): `WIZ_NPC_INOUT 0x0A`, `WIZ_NPC_MOVE 0x0B`, `WIZ_DEAD 0x11`, `WIZ_NPC_REGION 0x1C`, `WIZ_REQ_NPCIN 0x1D`.
  - Yerel DB (`K_NPC.strName`, `K_MONSTER.strName`, yasak tablo değil): en uzun ad **30** karakter (`K_NPC` sütunu 30) → ad tamponu 32 bayt (NUL dahil, en çok 31 karakter) yeter.
  - `BotCore/Perception.h` (F4-12/F4-13; başlık-yalnızca, `namespace BotCore`, `inline`, tab/Allman): `ByteReader` (taşmada sıfır döner), `ParseRegionList`, `ObsTable` (kalıp), `PendingIds`. `GameServer/Bot/BotSession.h:137-139,158`, `BotSession.cpp:26` (başlatıcı listesi), `:174-226` (`OnPacket()` algı bloğu), `:286-293` (`ResetForRespawn()` kilitli temizlik bloğu). `GameServer/Bot/BotManager.cpp:653-654` (`see` dağıtımı) ve `:658` ("unknown command" listesi), `:2228-2316` `CommandSee` (kalıp), `BotManager.h:88` bildirimi.
  - `Tests/BotCoreTests/PerceptionTests.cpp` (dosya-yerel `Buf`, `AddUserInfo`; `TEST_CASE`, `CHECK`, `CHECK_EQ(int(..), int(..))`); şu an **55** test.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/Perception.h`, ekleme):** `NpcObs`, `ParseNpcInfo`, `ParseNpcInOut`, `ParseNpcList`, `ParseNpcMove`, `NpcTable`. `ParseRegionList` `WIZ_NPC_REGION` için **aynen** kullanılır. Birim testleri (`PerceptionTests.cpp`, ekleme: 6 yeni `TEST_CASE`).
2. **Oturum durumu (`BotSession`):** `NpcTable m_npcs` (`m_obsLock` altında, ikinci tablo için ikinci kilit **yok**) + `std::atomic<uint32> m_npcUnresolved`. `OnPacket()`'e **yeni ayrı blok** (mevcut bloklara dokunulmaz); `ResetForRespawn()` tabloyu temizler.
3. **Komut (`BotManager`):** `npcs <bot>` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten); `CommandNpcs`. Tablonun kopyasını kilit altında alır, **kilidi bırakıp** biçimler ve günlüğe yazar.
4. **Dokümantasyon ve kayıtlar Claude'un işi** (DeepSeek dokunmaz).

**Kapsam dışı (yapılmayacak)**

- **Bot istek göndermez:** bölge değişiminde bilinmeyen NPC kimlikleri için `WIZ_REQ_NPCIN` bu planda **yok** (F4-15, CLI-20); `WIZ_NPC_REGION`'ın bilinmeyen kimlikleri yalnızca sayılır (`m_npcUnresolved`). Sonuç: bot yeni bir bölgeye yürüyünce o bölgenin NPC'leri, `WIZ_NPC_INOUT`/`WIZ_NPC_MOVE` gelene kadar tabloda **yoktur** (bilinen sınır, kabul edilmiş). `PendingIds`, `ActionExecutor`, `BotFairnessGuard` **değişmez**.
- **Görünür silah, satış grubu, boyut, resim kimliği, yön, bilinmeyen `u16` çifti:** paketten **okunup atlanır**, saklanmaz. Kapı durumu yalnızca bilgi paketindeki `gateOpen` olarak saklanır; `WIZ_OBJECT_EVENT` (kapı/kaldıraç değişimi) **işlenmez** (değer "son bilgi paketi anındaki"dır).
- **NPC HP'si** (`WIZ_TARGET_HP` ile NPC hedefi, `WIZ_NPC_HP_CHANGE`...), NPC saldırı/cast olayları, NPC sahibi/atanmış hedef, kümeleme, bayatlama temizliği, ulus/düşman sınıflaması (canavarın ulusu 0 gelir; sınıflama karar katmanının işi): yok. Tablo yalnızca paketle gelen **olguları** tutar.
- **Tabloyu kullanan hiçbir karar veya guard:** yok. `ActionExecutor` NPC hedefi desteklemez (`TargetHpReq` NPC kapsamı F4-06 eki gereği ayrı dilim).
- Pazarcılar (`MerchantUserInOutForMe`), `PerceptionSnapshot`/`SelfState`/`NavView`, telemetri olayı (`PERCEPTION_*`), `list`/`see` çıktısı, `Telemetry.*`, `ScenarioRunner.*`, `ActionExecutor.*`: **değişmez**. Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. `GameServer` projesine dosya eklenmez; `BotCore.vcxproj` ve `BotCoreTests.vcxproj` **değişmez** (`Perception.h` ve `PerceptionTests.cpp` zaten listede).
- `ChatHandler.cpp` `+bot` yardım metni (KI-012): değişmez (Claude KI-012'yi günceller).
- Dokümanları (`docs/03`, `docs/13`, `docs/14`, `docs/16`, `docs/15`, `docs/KNOWN_ISSUES.md`) güncellemek: Claude'un işi.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | Yalnızca **ekleme** (dosyanın sonuna, `namespace BotCore` içinde); mevcut satırlar değişmez |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | Yalnızca ekleme: dosya-yerel `AddNpcInfo` yardımcısı + altı `TEST_CASE` |
| `GameServer/Bot/BotSession.h` | değiştir | İki alan (§5.3); yalnızca ekleme |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesine bir üye, `ResetForRespawn()` ve `OnPacket()` sonuna **ekleme** |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandNpcs` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, "unknown command" listesi, `CommandNpcs` |

(Dokunulacak dosya sayısı 6; plan dosyası dahil 7.) Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. `GameServer/proj-GameServer.vcxproj*` **değişmez**.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-14 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/Perception.h` (ekleme) ve birim testleri

Biçim: dosyanın mevcut kısmı gibi (`inline`, tab, Allman, İngilizce kısa yorum). Dosyanın sonuna, mevcut son `}` (namespace kapanışı) **öncesine** yeni bir bölüm ekle: `// --- NPC observation (ADR-0017 Ek F4-14) ---`. Yeni `#include` **yok**. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` geçme; yorumlarda `shared/` ve `GameServer` dizgilerini **yazma**; `new `, `malloc`, `std::vector`, `std::string`, `std::min`, `std::max` yok (F4-12 K4 grep'i takılır). Dosya ASCII + CRLF.

**Sabitler ve veri:**

```cpp
constexpr int      kNpcMaxUnits = 128;   // table capacity per bot (design limit; a 3x3 region group of the arena holds far fewer)
constexpr uint32_t kNpcNameMax  = 32;    // stored name buffer, NUL included; the longest NPC name in the tables is 30
constexpr uint8_t  kNpcInOutOut = 2;     // InOutType: INOUT_OUT, ONE byte in WIZ_NPC_INOUT (1 in, 3 respawn, 4 warp, 5 summon = present)

struct NpcObs
{
	uint16_t id;
	uint16_t protoId;             // prototype id as sent (the server's m_sSid)
	uint8_t  type;                // NPC type byte as sent
	uint8_t  nation;              // 0 for monsters (the server sends 0)
	uint8_t  level;
	uint16_t x10, z10, y10;       // position x10 as sent on the wire
	bool     gateOpen;            // gate flag of the last info packet (not updated by object events)
	uint8_t  objectType;
	bool     dead;                // set by WIZ_DEAD, cleared by a fresh info record
	uint64_t lastSeenMs;          // caller's clock (steady_clock ms) of the packet that last touched the NPC
	char     name[kNpcNameMax];   // NUL terminated, every byte after the NUL is zero
};
```

**`inline bool ParseNpcInfo(ByteReader & r, uint16_t id, uint64_t nowMs, NpcObs & out)`** — sunucunun `GetNpcInfo` düzenini (`id` zaten okunmuş) okur ve `r`'yi kaydın sonuna ilerletir. Alan sırası (küçük-endian, dizge `u8` uzunluklu):

| Sıra | Alan | Boyut | Saklanır |
|---|---|---|---|
| 1 | prototip kimliği | 2 | `protoId` |
| 2 | resim kimliği | 2 | hayır |
| 3 | tip | 1 | `type` |
| 4 | satış grubu | 4 | hayır |
| 5 | boyut | 2 | hayır |
| 6 | silah 1, silah 2 | 4+4 | hayır |
| 7 | ad (`u8` uzunluk + bayt) | 1+n | `name` |
| 8 | ulus | 1 | `nation` |
| 9 | seviye | 1 | `level` |
| 10 | x, z, y (×10) | 2+2+2 | `x10`, `z10`, `y10` |
| 11 | kapı açık (`u32`) | 4 | `gateOpen` (`!= 0`) |
| 12 | nesne tipi | 1 | `objectType` |
| 13 | bilinmeyen iki `u16` | 2+2 | hayır |
| 14 | yön (`i8`) | 1 | hayır |

Ad `r.Str(local, kNpcNameMax)` ile okunur (32'den uzun → `false`). `out.name` **tamamen sıfırlanır** (`kNpcNameMax` bayt), sonra yalnızca NUL'a kadar olan baytlar kopyalanır (F4-12 doğrulama bulgusu 1: başlatılmamış yığın baytları `name`'e geçmemeli). Başarıda `out.id = id; out.dead = false; out.lastSeenMs = nowMs; return r.ok();`.

**Tek/toplu paket ayrıştırıcılar** (`data`/`len` = paket **yükü**; taşma yapmaz):

```cpp
// WIZ_NPC_INOUT: u8 type, u16 id, then the NPC info unless type == kNpcInOutOut.
// For OUT only out.id is written. Returns false on a malformed packet (needs 3 bytes).
inline bool ParseNpcInOut(const uint8_t * data, size_t len, uint64_t nowMs, uint8_t & type, NpcObs & out);

// WIZ_REQ_NPCIN: u16 count, then count x (u16 id, NPC info) with NO marker byte. Parses at most 'cap' entries and
// stops at the first malformed or missing one; returns the number written to 'out'. 'declared' receives the count the
// packet states (0 when the count itself is unreadable). A count larger than the data is not an error.
inline int ParseNpcList(const uint8_t * data, size_t len, uint64_t nowMs, NpcObs * out, int cap, uint16_t & declared);

// WIZ_NPC_MOVE: u16 id, u16 x, u16 z, u16 y, u16 speed (x, z, y are x10). Needs 10 bytes.
inline bool ParseNpcMove(const uint8_t * data, size_t len, uint16_t & id, uint16_t & x10, uint16_t & z10, uint16_t & y10);
```

**`NpcTable`** (kopyalanabilir, mutex içermez; kilidi çağıran tutar; `ObsTable` ile aynı kalıp, `ObsTable` **değişmez**):

```cpp
class NpcTable
{
public:
	NpcTable() { Clear(); }
	void Clear();                                          // count 0, overflow 0
	int Count() const;
	uint32_t Overflow() const;                             // NPCs not stored: refused inserts plus NoteDropped()
	void NoteDropped(uint32_t n);                          // adds n to Overflow() (list entries beyond the parse cap)
	bool Upsert(const NpcObs & n);                         // insert or refresh by id; false (and Overflow()++) when new and full
	void Remove(uint16_t id);                              // WIZ_NPC_INOUT OUT (order not preserved)
	bool UpdatePosition(uint16_t id, uint16_t x10, uint16_t z10, uint16_t y10, uint64_t nowMs);   // known NPCs only
	bool MarkDead(uint16_t id, uint64_t nowMs);            // known NPCs only: dead = true
	int Retain(const uint16_t * ids, int n);               // drops every NPC not listed; returns how many listed ids are unknown (repeats counted separately)
	const NpcObs * Find(uint16_t id) const;
	const NpcObs & At(int i) const;
};
```

**Birim testleri** (`PerceptionTests.cpp`'in sonuna; mevcut `Buf` kullanılır; dosya-yerel `AddNpcInfo(Buf&, ...)` yardımcısı §5.2 tablosundaki sırayla yazar). Altı `TEST_CASE`, adları **aynen**:

1. `Perception_NpcInfo_Parse`: ad `"Karus Guard Tower"`, tip, ulus 1, seviye 90, konum, `gateOpen` 0/1, `objectType`; `r.pos()` tam `38 + n`; ad bitişinden sonraki tüm `name[]` baytları 0 (kısa ad, tamponun kalanı sıfır); ad uzunluğu 31 geçerli, 32 → `false`.
2. `Perception_NpcInfo_Truncated`: geçerli bir kaydın her `len < tam` ön eki `ParseNpcInfo` ile `false` verir ve sınır dışı okumaz (F4-12 `Perception_UserInfo_Truncated` kalıbı); `data == nullptr` → `false`.
3. `Perception_ParseNpcInOut`: IN (tip 1) tam kayıt → `true`, `type == 1`, alanlar; RESPAWN (3) aynı; OUT (tip 2, yalnızca 3 bayt) → `true`, `out.id` doğru, bilgi okunmaz; 2 baytlık paket → `false`; IN ama kayıt kesik → `false`.
4. `Perception_ParseNpcList`: sayı 2 + iki kayıt (işaret baytı yok) → 2 yazılır, `declared == 2`, kimlikler/adlar doğru; sayı 5 ama yalnızca 2 kayıt → 2 yazılır, `declared == 5`; `cap = 1` → 1 yazılır; ikinci kayıt kesik → 1 yazılır; boş/`nullptr` → 0 ve `declared == 0`.
5. `Perception_ParseNpcMove`: 10 bayt → `true`, `id/x10/z10/y10` doğru; 9 bayt → `false`; 11 bayt (fazlası yok sayılır) → `true`.
6. `Perception_NpcTable`: `Upsert` ekle/yenile (aynı `id` ikinci kez sayıyı artırmaz); tablo 128 doluyken yeni `id` → `false`, `Overflow()` artar; `NoteDropped(3)` `Overflow()`'a eklenir; `Remove`; `UpdatePosition` bilinen/bilinmeyen; `MarkDead` bilinenin `dead`'ini true yapar, sonra aynı `id` için yeni `Upsert` (taze kayıt) `dead`'i false yapar; `Retain`: listede olmayanlar düşer, listede olup tabloda olmayan kimlikler `unresolved`'a sayılır (tekrar eden kimlik ayrı sayılır), boş liste tabloyu boşaltır; `Clear` sayacı da sıfırlar.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h` (alanlar, mevcut `m_obsPending` satırının altına; yorum/hizalama biçimi aynı):

```cpp
	BotCore::NpcTable m_npcs;                              // guarded by m_obsLock (the same mutex as m_obs): NPCs in view, from received packets only (Perception, ADR-0017 Ek F4-14)
```

ve atomiklerin sonuna (`m_userInEcho`'nun altına):

```cpp
	std::atomic<uint32> m_npcUnresolved;                   // written by OnPacket(): ids of the last WIZ_NPC_REGION that were not in m_npcs
```

`BotSession.cpp`:

- Başlatıcı listesi: `m_userInEcho(0)`'dan sonra `m_npcUnresolved(0)` (üye bildirim sırasına uy; derleyici sıra uyarısı vermemeli). `m_obsLock`'un yorumu ("guards m_obs and m_obsPending") **değişmez**.
- `ResetForRespawn()`: mevcut kilitli blokta (`m_obs.Clear(); m_obsPending.Clear();`) bir satır `m_npcs.Clear();` ekle; `m_obsUnresolved = 0;`'ın yanına `m_npcUnresolved = 0;`.
- **`OnPacket()` sonuna ekleme** (mevcut algı bloğunun hemen ardı; mevcut bloklara **dokunulmaz**, koşul satırı değişmez). `data`, `len`, `nowMs` bu blokta yeniden hesaplanır (mevcut bloğun ifadeleriyle **aynı** `steady_clock` ms ifadesi):

```cpp
	// Perception (ADR-0017 Ek F4-14): what a client would learn about the NPCs in view. Same rules as above: only the
	// packets the server sends to this session are read; parsing happens before the lock is taken.
	if (opcode == WIZ_NPC_INOUT || opcode == WIZ_REQ_NPCIN || opcode == WIZ_NPC_REGION
		|| opcode == WIZ_NPC_MOVE || opcode == WIZ_DEAD)
	{
		...
	}
```

  Gövde (her dalda `std::lock_guard<std::mutex> lock(m_obsLock);` yalnızca tablo çağrıları için):
  - `WIZ_NPC_INOUT`: `BotCore::ParseNpcInOut` → başarılıysa tip `kNpcInOutOut` ise `m_npcs.Remove(npc.id)`, değilse `m_npcs.Upsert(npc)`. Bozuk paket yok sayılır.
  - `WIZ_REQ_NPCIN`: `NpcObs list[BotCore::kNpcMaxUnits]` (yığın, ~7 KB) → `ParseNpcList(..., list, kNpcMaxUnits, declared)`; kilit altında her giriş için `Upsert`; Sunucu cevabında sayı **istenen** sayı olabilir (bulunamayanlar atlanır), yani `declared > n` her zaman "bırakıldı" demek değildir; bu yüzden `m_npcs.NoteDropped(declared - n)` yalnızca `n == kNpcMaxUnits` (ayrıştırıcı üst sınıra dayandı) ve `declared > n` iken çağrılır.
  - `WIZ_NPC_REGION`: `uint16 ids[BotCore::kNpcMaxUnits * 4]` (512 kimlik; fazlası kırpılır) → `ParseRegionList` (mevcut); kilit altında `m_npcUnresolved = (uint32)m_npcs.Retain(ids, n);`.
  - `WIZ_NPC_MOVE`: `ParseNpcMove` → kilit altında `m_npcs.UpdatePosition`.
  - `WIZ_DEAD`: `len >= 2` ise `uint16` id → kilit altında `m_npcs.MarkDead` (oyuncu kimlikleri NPC tablosunda bulunmaz, `false` döner; oyuncu tarafını mevcut blok zaten işler).

### 5.4 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirim (`CommandSee`'nin altına): `void CommandNpcs(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()`:** `see` dalının altına `npcs` (`CommandNpcs(args)`) fiilini ekle; "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast, pot, sit, stand, target, regene, pinvite, paccept, pdecline, pleave, ppromote, pkick, pchat, see, npcs)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **`CommandNpcs(args)`** `CommandSee` kalıbıyla, `CommandSee`'nin hemen altında (`ParseSpawnList`'ten önce): `SplitWords`; tek sözcük değilse `BotManager: cmd npcs: usage: npcs <bot>`; `FindSession` (`unknown or not spawned bot '<ad|?>'`) ve faz denetimi (`<bot> not in game (phase X)`), hepsi `cmd npcs:` önekiyle. Sonra:
   - Kilit altında **yalnızca** `BotCore::NpcTable copy = s->m_npcs;` ve `uint32 unresolved = s->m_npcUnresolved.load();` (kilidi hemen bırak; günlük yazma ve biçimleme **kilit dışında**).
   - Botun kendi durumu (bu planın tek "kendi nesnesi" okuması, sözleşmece izinli): `CUser * me = s->m_pUser;` → **yalnızca** `me->GetX()`, `me->GetZ()`.
   - Her NPC için: mesafe = `sqrt((x10/10 - myX)² + (z10/10 - myZ)²)` (float, `%.1f`); yaş = `now_ms - lastSeenMs` (aynı `steady_clock` ms ifadesi; negatif → 0). Ulus/düşman sınıflaması **yapılmaz** (ham `nation` yazılır).
   - Günlük (`WriteBotLog`, `char message[320]`): başlık `BotManager: cmd npcs: <bot> sees <N> npc(s) (dead <D>, dropped <overflow>, unresolved <unresolved>)`, ardından açıklama satırı `BotManager: cmd npcs:   (unresolved counts the ids of the last WIZ_NPC_REGION list that the table did not know; no WIZ_REQ_NPCIN is sent yet)`, ardından NPC başına bir satır `BotManager: cmd npcs:   id=<id> proto=<protoId> type=<type> <name> nation=<n> lvl=<l> pos=(<x>, <z>) dist=<d> <alive|dead> gate=<open|closed> obj=<objectType> age=<ms>ms`. Boş tablo: başlık `sees 0 npc(s)`. Satır sınırı yok (tablo en çok 128 satır).
   - `ENABLED=0` iken komut yolu zaten kapalı: ek denetim gerekmez.
3. **`TickSessions()`, `BeginDespawn()`, `BuildStatusLines()`, `CommandSee()`:** **değişmez**.

### 5.5 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`Perception.h`, `PerceptionTests.cpp`, `BotSession.cpp`, `BotManager.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı altı yeni test adını (`Perception_NpcInfo_Parse`, `Perception_NpcInfo_Truncated`, `Perception_ParseNpcInOut`, `Perception_ParseNpcList`, `Perception_ParseNpcMove`, `Perception_NpcTable`) içerir ve toplam test sayısı **61**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h` eşleşme vermez; `#include` satırları yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`; `grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h` boş.
- [ ] K5: **sözleşme dışı erişim yok (statik AC-LRN-03 denetimi):** `grep -nE "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|GetRegionX|GetRegionZ|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMaxHealth|GetMana|GetMaxMana|m_buffMap|m_CoolDownList" GameServer/Bot/BotSession.cpp` ve `grep -n "CommandNpcs" -A80 GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionNpcArray|GetRegion\(|GetMap\(|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMana"` **boş**; `CommandNpcs` yalnızca `s->m_pUser` üzerinden `GetX()`, `GetZ()` okur (`grep -n "me->"` çıktısı bu iki çağrı).
- [ ] K6: NPC tablosu yalnızca paketle dolar: `m_npcs.` çağrıları `BotSession.cpp`'de yalnızca `OnPacket()` ve `ResetForRespawn()` içinde, `BotManager.cpp`'de yalnızca `CommandNpcs` içinde geçer (`grep -n "m_npcs" GameServer/Bot/*.cpp`); `m_obsLock` tutulurken günlük yazılmaz (`CommandNpcs`'te kilit bloğu yalnızca iki kopyalama satırı; `OnPacket()` ayrıştırması kilit dışında). `ParseNpcInfo` sonrası `name[]`'in NUL sonrası baytları 0 (K3 testi 1 bunu denetler).
- [ ] K7: mevcut kod yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-14 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki değişen satırı gösterir ve mevcut `OnPacket()` bloklarında silinen/değişen satır yok; `git diff gece/2026-10-02...bot/F4-14 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca "unknown command" mesaj satırını gösterir; `BotCore/Perception.h` ve `Tests/BotCoreTests/PerceptionTests.cpp` farkında **silinen satır yok** (`grep '^-' | grep -v '^---'` boş).
- [ ] K8: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca bot oturumlarına gelen paketler ve `npcs` komutu için çalışır; `Startup()`/`Tick()`/`TickSessions()`/`BuildStatusLines()`/`BeginDespawn()`, `CommandSee` ve ini okuma değişmedi; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K9: `git diff --stat gece/2026-10-02...bot/F4-14` yalnızca §4'teki 6 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` farkı boş.
- [ ] K10: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı: `.cpp/.h` ASCII + CRLF); `git diff --check` boş.
- [ ] K11: `GameServer/Bot/BotSession.*`/`BotManager.*` içinde yeni `printf`, `Sleep`, `CreateThread`, `rand(` yok; `std::mutex` yalnızca mevcut `m_obsLock` için (ikinci mutex yok: `grep -c "std::mutex" GameServer/Bot/BotSession.h` = 1); `ActionExecutor.*`, `Telemetry.*`, `ScenarioRunner.*` değişmemiş.
- [ ] K12: F4-01..F4-13 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `CheckAttack`, `CheckCastStart`, `CheckPotion`, `CheckStance`, `CheckTargetHp`, `CheckRegene`, `CheckPartyInvite`, `CheckPartyAccept`, `CheckPartyDecline`, `CheckPartyLeave`, `CheckPartyManage`, `CheckChat`, `CheckUserIn` her biri ≥ 1; önceki 55 testin tamamı hâlâ geçiyor.
- [ ] K13 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–5 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-14
git diff gece/2026-10-02...bot/F4-14 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-14 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-14 -- BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp | grep '^-' | grep -v '^---'
git diff gece/2026-10-02...bot/F4-14 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h
grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h
grep -nE "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|GetRegionX|GetRegionZ|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMaxHealth|GetMana|GetMaxMana|m_buffMap|m_CoolDownList" GameServer/Bot/BotSession.cpp
grep -n "CommandNpcs" -A80 GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionNpcArray|GetRegion\(|GetMap\(|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMana"
grep -n "m_npcs" GameServer/Bot/*.cpp
grep -c "std::mutex" GameServer/Bot/BotSession.h
grep -n "printf\|Sleep\|CreateThread\|rand(" GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h
file BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp
git diff --check gece/2026-10-02...bot/F4-14
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=summary`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. **AIServer de açık olmalıdır** (NPC'ler AIServer'dan gelir; `run-servers.sh start` üçünü de açar, `status` ile `[UP]` doğrula). Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`). Botlar: Karus `BotWP_K`, `BotMF_K`; El Morad `BotWG_E`; zone 71. Gözlem `Logs/Bot_*.log`. Senaryolar:

1. **Spawn ve toplu kayıt:** `spawn BotWP_K` → `npcs BotWP_K`: arena A çevresinde NPC bulunmayabilir (T-ENV-ARENA-01: 120 m içinde varlık hedefi 0); `sees 0 npc(s)` ise bu geçerli bir sonuçtur, bir sonraki senaryoya geç. Dönüştürülmüş NPC varsa her satırın `name/type/lvl/pos` değeri yerel DB'deki `K_NPC`/`K_MONSTER` satırıyla (yasak tablo değil) ve `list` çıktısındaki bot konumuyla tutarlı olmalı.
2. **Tower halkası:** `BotWP_K`'yı öldür (`attack`/`cast` ile bir El Morad botundan) → `regene BotWP_K` (Karus doğuş noktası ≈ (1380, 1090), tower halkasının içi) → `npcs BotWP_K`: Karus guard tower'ları (`docs/03` §12.3: 5400/5410) `alive`, konumları tower halkasında, `dist` ≤ ~50 m; bu, `WIZ_NPC_REGION` + sunucunun toplu kaydı (yeniden doğuşta `AttackHandler.cpp:219`) sonrası **bilinen sınır** nedeniyle tablonun eksik kalabileceğini de gösterir: `unresolved` > 0 ve tablo boş/eksikse (F4-15 kapatacak) bu **hata değildir**; yalnızca `WIZ_NPC_INOUT`/`WIZ_NPC_MOVE` ile gelenler tabloda olur, kayıt `unresolved` açıklamasıyla birlikte raporlanır.
3. **Hareket:** hareketli bir NPC/canavar görünürse (`type` canavar) iki `npcs` çıktısı arasında `pos` değişir ve `age` küçülür (`WIZ_NPC_MOVE`); görünmüyorsa "gözlenmedi" yazılır (kriteri düşürmez, kod yolu birim testli).
4. **Ölüm ve çıkış:** bir NPC öldürülebilirse (`attack`) `dead` görünür (`WIZ_DEAD`); bot bölgeyi terk edince `WIZ_NPC_REGION` `Retain` ile düşen kayıtlar `npcs` çıktısından kaybolur. Gözlenemeyen durumlar "gözlenmedi" diye rapora yazılır.
5. **Gerilemesiz:** `see`, `move`, `attack`, `cast`, `pot`, `sit`, `target`, `regene`, `pinvite`, `pchat` komutları ve F4-13 `userin` sayaçları (`see` açıklama satırı) beklendiği gibi; `npcs` kullanım/bilinmeyen-bot/fazla-argüman/not-in-game yolları ve "unknown command" listesinde `npcs` var; `ENABLED=0` → komut dosyası tüketilmez; `RESPAWN_CYCLES=2` ile `npcs` reddedilir; `PERF_SAMPLE` `tick_p95_us` ≤ 500; sunucu çökmedi, `despawn all` temiz, iş bitince `run-servers.sh stop`. İnsan istemcisi gerekmez.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `BotSession.h/.cpp`, `BotManager.*`, `Perception.h`, `PerceptionTests.cpp` ASCII + CRLF; mevcut dosyalarda yalnızca ekleme (satır sonu karışmasın: `git diff --check`).
- **Thread kuralı (`docs/13` §3):** `OnPacket()` IOCP-yalnızca olmayan bir çağırandır; her iki tabloya yalnızca `m_obsLock` altında dokunulur, kilit **kısa** tutulur (ayrıştırma ve günlük kilit dışında), `OnPacket()` içinde `new`/dosya/günlük yazma yok. `ByteReader` taşmada sıfır döner, sınır dışı okumaz.
- Sözleşme: yeni kod **sunucunun NPC dizilerine, bölge dizilerine, haritaya veya küresellere** erişmez (K5 grep'i sonraki dilimlerin de kalıbıdır).
- Alan düzeni (§5.2 tablosu) sunucu kaynağından çıkarıldı `[D]`; çalışma zamanında ilk kez bot-AIServer NPC'leriyle sınanır. Beklenmedik uzunluk görürsen **uydurma**: ayrıştırıcı `false` döndürür ve paket yok sayılır; Uygulayıcı Raporu'na yaz. `GetNpcInfo`'daki `m_sSid` `uint16`, `GetProtoID()` `short` döner: ikisi de 2 bayt yazılır (aynı bayt dizisi).
- `isMonster()` için ulus baytı 0 gelir; `nation`'ı "düşman mı" kararı için **kullanma** (bu planda karar yok).
- Bu planın riski: `WIZ_NPC_INOUT` tip baytı **tek bayt** (oyuncu paketinde `u16`); `WIZ_REQ_NPCIN` kayıtlarında işaret baytı **yok** (oyuncu paketinde var). İkisi de testlerle ayrı ayrı kilitlenir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-14` — `<kısa-sha> [F4-14] …`
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
- İncelenen: `gece/2026-10-02...bot/F4-14` @ `<sha>`
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
