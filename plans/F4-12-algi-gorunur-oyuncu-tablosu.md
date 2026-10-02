# F4-12: `Perception` dilim 1 — görüş alanındaki oyuncular (paketlerden gözlem tablosu) ve `/bot see`

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-12` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-11 (`OnPacket()` kayıt kalıbı, `BotSession` alanları) — `KAPANDI` (merge `ca677c0`); F3-05 (`BotCore`, birim test çatısı) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/14` §5.2 gözlem sözleşmesi, `docs/03` §16, AC-LRN-03 / AC-ARCH-06 (sözleşme dışı algı erişimi 0; bu planda **statik denetimle**), `docs/13` §2 `Perception` |
| Tahmini büyüklük | M (9 dosya; 2'si yeni, `GameServer` projesine dosya eklenmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

`Perception` bileşeninin ilk dilimi: her bot, **gerçek istemcinin alacağı paketlerden** görüş alanındaki (3×3 bölge) diğer oyuncuların bir tablosunu tutar. Kaynaklar yalnızca bot alıcısına gelen paketlerdir: `WIZ_REQ_USERIN` (toplu bilgi), `WIZ_USER_INOUT` (giriş/çıkış), `WIZ_MOVE` (konum), `WIZ_DEAD` (ölüm), `WIZ_REGIONCHANGE` (bölge kimlik listesi: listede olmayanlar tablodan düşer). Sunucunun başka oyuncu nesnelerine, bölge dizilerine veya harita yapılarına **hiç dokunulmaz** (gözlem sözleşmesi, `docs/14` §5.2). Paket ayrıştırma ve tablo mantığı `BotCore/Perception.h` içinde saf, birim testli koddur. Sonucu görmek için `/bot see <bot>` komutu tabloyu `Logs/Bot_*.log`'a döker (ulus, sınıf, seviye, konum, mesafe, ölü/diri, "son görülme"). Karar katmanı ve aksiyon yoktur. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün on ikinci dilimidir (ADR-0017 Ek F4-12). Sonraki dilimler: bölge değişiminde `WIZ_REQ_USERIN` isteği (F4-13), NPC/canavar gözlemi, `PerceptionSnapshot` (`SelfState`, `enemies`/`allies`), betikli test dizileri.

## 2. Bağlam (okunması zorunlu)

- `docs/14` §5.2 (gözlem sözleşmesi) ve `docs/03` §16 (hangi bilgi istemciye hangi paketle gelir): bu plan yalnızca **"görüş alanındaki oyuncular: isim, ulus, ırk, sınıf, seviye, party lideri bayrağı, gizlilik durumu"**, **"konum ve hareket"** ve **"ölüm"** satırlarını uygular. Görünür ekipman kayıtları paketten **atlanır** (bu planda saklanmaz). `docs/13` §2 `Perception`, §4.2 "Algıya giden başlıca paketler".
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `ca677c0` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/CharacterMovementHandler.cpp:63-69` `CUser::GetInOut`: `WIZ_USER_INOUT` yükü = `u16 tip, u16 sid` + (tip ≠ `INOUT_OUT` ise) `GetUserInfo`. `:99-161` `CUser::GetUserInfo`: `pkt.SByte()` ile **dizgeler `u8` uzunluklu** olur; alan sırası aşağıdaki §5.2 tablosundadır. `:45-47` `WIZ_MOVE` yayını: `u16 sid, u16 x, u16 z, u16 y, i16 speed, u8 echo` (konumlar ×10).
  - `GameServer/GameServerDlg.cpp:1320-1338` `UserInOutForMe` ve `:1360-1389` `GetRegionUserIn`: `WIZ_REQ_USERIN` yükü = `u16 sayı` + sayı kez (`u8 0, u16 sid, GetUserInfo`). `:1340-1358` `RegionUserInOutForMe` ve `:1391-1419` `GetRegionUserList`: `WIZ_REGIONCHANGE` yükü = `u16 sayı` + sayı kez `u16 sid`. Bu iki yayın `SendCompressed` ile gider; `GameServer/User.cpp:28-39` `CUser::SendCompressed` bot alıcısına **sıkıştırılmamış** paketi verir.
  - `GameServer/CharacterSelectionHandler.cpp:260-271` `GameStart(1)`: bot girişinde `UserInOutForMe(this)` çağrılır → bot, spawn anında bölgesindeki oyuncuların `WIZ_REQ_USERIN` paketini alır. `GameServer/User.cpp:1231-1253` `CUser::RequestUserIn` (istemci isteği; **bu planda bot istek göndermez**).
  - `GameServer/Unit.cpp:956-964` `SendDeathAnimation`: `WIZ_DEAD` yükü = `u16 sid` (bölge yayını).
  - `GameServer/Define.h:52-58` `InOutType`: `INOUT_IN = 1, INOUT_OUT = 2, INOUT_RESPAWN = 3, INOUT_WARP = 4, INOUT_SUMMON = 5`. `GameServer/GameDefine.h:117-119` `USER_STANDING 1, USER_SITDOWN 2, USER_DEAD 3` (paketteki `m_bResHpType` değeri).
  - `shared/packets.h:8,9,19,23,24` opcode'lar: `WIZ_MOVE 0x06`, `WIZ_USER_INOUT 0x07`, `WIZ_DEAD 0x11`, `WIZ_REGIONCHANGE 0x15`, `WIZ_REQ_USERIN 0x16`.
  - `shared/ByteBuffer.h:103-113` `read<T>()`/`read<T>(pos)` ve `:127-128` `contents()`/`size()`; `shared/Packet.h:42` opcode ayrı saklanır → `contents()` ve `size()` **yalnızca yükü** verir (mevcut `OnPacket()` blokları da `pkt.read<T>(0)` ile yükün ilk baytını okur). `contents()` boş pakette `&_storage[0]` verir: **`size() == 0` iken çağrılmaz.** Sunucu `x86` küçük-endian: çok baytlı alanlar küçük-endian.
  - `GameServer/Bot/BotSession.cpp:33-` `OnPacket()` (son blok `WIZ_CHAT`); `BotSession.h:13` yorumu: `OnPacket()` DB thread'inde, IOCP thread'inde veya 30 sn zamanlayıcı thread'inde çalışabilir → tablo **mutex ile** korunur (aşağıda). `GameServer/Bot/BotSession.h:28-137` alanlar, `BotSession.cpp:171-` `ResetForRespawn()`.
  - `GameServer/Bot/BotManager.cpp:591-657` `ExecuteCommand`, `:661-669` `FindSession`, `:1817-1863` `CommandRegene` (tek botlu komut kalıbı), `:2178-2224` `CommandPartyChat` (yeni komut bunun **hemen ardına**, `ParseSpawnList`'ten önce eklenir). `GameServer/Bot/BotManager.h:87` `CommandPartyChat` bildirimi.
  - `BotCore/BotMotion.h` (başlık-yalnızca, `namespace BotCore`, `inline`, tab/Allman), `Tests/BotCoreTests/MotionTests.cpp` (test stili: `TEST_CASE`, `CHECK`, `CHECK_EQ(int(..), int(..))`), `Tests/BotCoreTests/BotCoreTests.vcxproj` `<ItemGroup>` `ClCompile` listesi, `BotCore/BotCore.vcxproj` `ClInclude` listesi.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/Perception.h`, yeni, yalnızca standart kütüphane):** `UnitObs`, `ByteReader`, `ParseUserInfo`, `ParseUserInOut`, `ParseUserList`, `ParseMove`, `ParseRegionList`, `ObsTable`. Birim testleri (`Tests/BotCoreTests/PerceptionTests.cpp`, yeni).
2. **Oturum durumu (`BotSession`):** `ObsTable m_obs` + `std::mutex m_obsLock` + `std::atomic<uint32> m_obsUnresolved`. `OnPacket()`'e **yalnızca ekleme** (yeni blok, mevcut bloklara dokunulmaz); `ResetForRespawn()` tabloyu temizler.
3. **Komut (`BotManager`):** `see <bot>` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten); `CommandSee`. Tablonun kopyasını kilit altında alır, **kilidi bırakıp** biçimler ve günlüğe yazar.
4. **Dokümantasyon ve kayıtlar Claude'un işi** (DeepSeek dokunmaz).

**Kapsam dışı (yapılmayacak)**

- **Bot istek göndermez:** `WIZ_REQ_USERIN` (bölge değişiminde bilinmeyen kimlikler için) bu planda **yok**; `WIZ_REGIONCHANGE`'in bilinmeyen kimlikleri yalnızca sayılır (`m_obsUnresolved`, `see` çıktısında "unresolved"). İstek F4-13'ün işi. Sonuç: bot hareket edip yeni bir bölgeye geçerse, o bölgenin oyuncuları sonraki `WIZ_USER_INOUT`/`WIZ_MOVE` paketleri gelene kadar tabloda **yoktur** (bilinen sınır, kabul edilmiş).
- **NPC/canavar** (`WIZ_NPC_INOUT`, `WIZ_REQ_NPCIN`), pazarcılar (`MerchantUserInOutForMe`): yok (arena A'da canavar/NPC yok, K-6/T-ENV-ARENA-01).
- **Görünür ekipman, klan adı/rütbe, yüz/saç, anormal durum, yetki, yön, krallık/NP rütbeleri:** paketten **okunup atlanır**, tabloda **saklanmaz** (ihtiyaç doğunca sonraki dilim ekler).
- **Hasar/HP gözlemi** (`WIZ_TARGET_HP`, `PARTY_HPCHANGE`, `WIZ_HP_CHANGE`), cast olayları (`WIZ_MAGIC_PROCESS` bölge yayını), buff takibi, `PerceptionSnapshot`/`SelfState`/`NavView`, kümeleme, `lastSeen` zaman aşımı/bayatlama temizliği: yok. Tablo yalnızca paketle gelen **olguları** tutar; tahmin/çıkarım yok.
- **Tabloyu kullanan hiçbir karar veya guard:** yok. `ActionExecutor`, `BotFairnessGuard`, `IsSamePartyMember` (F4-10'daki geçici üyelik okuması) **değişmez**; onları tabloya geçirmek ayrı dilimin işi.
- Telemetri olayı (`PERCEPTION_*`), `list` satırı, `Telemetry.*`, `ScenarioRunner.*`, `ActionExecutor.*`, `ChatHandler.cpp`: **değişmez**. Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. `GameServer` projesine dosya eklenmez.
- Dokümanları (`docs/03`, `docs/13`, `docs/14`, `docs/16`, `docs/15`, `docs/KNOWN_ISSUES.md`) güncellemek: Claude'un işi.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | **yeni** | §5.2; yalnızca `<cstdint>`, `<cstddef>`, `<cstring>` |
| `BotCore/BotCore.vcxproj` | değiştir | Yalnızca `<ClInclude Include="Perception.h" />` satırı (`BotMotion.h` ile `Rng.h` arasına) |
| `Tests/BotCoreTests/PerceptionTests.cpp` | **yeni** | §5.2 sonu: yedi `TEST_CASE` |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | Yalnızca `<ClCompile Include="PerceptionTests.cpp" />` satırı (`MotionTests.cpp` ile `RngTests.cpp` arasına) |
| `GameServer/Bot/BotSession.h` | değiştir | `#include`'lar + üç alan (§5.3) |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi, `ResetForRespawn()` ve `OnPacket()` sonuna **ekleme** |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandSee` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, komut, `unknown command` listesi |

(Dokunulacak dosya sayısı 8; plan dosyası dahil 9.) Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. `GameServer/proj-GameServer.vcxproj*` **değişmez** (`BotCore/Perception.h` başlık-yalnızca; `BotSession.h` onu göreli yolla `#include` eder, `BotCombat.h` gibi).

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-12 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/Perception.h` ve birim testleri

Biçim: `BotMotion.h` gibi (`#pragma once`, `namespace BotCore`, `inline`, tab, Allman, İngilizce kısa yorum). `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez** (yorumlarda da `shared/` ve `GameServer` dizgisi **yazma**: K4 grep'i takılır; dosya adı yerine "the server's chat handler" gibi anlat). `std::min`/`std::max` kullanma. Dosya ASCII + CRLF. `#include <cstddef> <cstdint> <cstring>`.

**Sabitler ve veri:**

```cpp
constexpr int      kObsMaxUnits   = 64;   // table capacity per bot (design limit; one 3x3 region group)
constexpr uint32_t kObsNameMax    = 24;   // stored name buffer, NUL included; MAX_ID_SIZE is 20, longer names are rejected
constexpr uint16_t kObsInOutOut   = 2;    // InOutType: INOUT_OUT (1 in, 3 respawn, 4 warp, 5 summon = present)
constexpr uint8_t  kObsUserDead   = 3;    // USER_DEAD in the m_bResHpType byte of the user info

struct UnitObs
{
	uint16_t sid;
	uint8_t  nation;
	uint8_t  race;
	uint16_t cls;
	uint8_t  level;
	uint16_t x10, z10, y10;       // position x10 as sent on the wire
	uint8_t  resHpType;           // 1 standing, 2 sitting, 3 dead (as in the user info)
	bool     partyLeader;
	uint8_t  invisibility;
	uint64_t lastSeenMs;          // caller's clock (steady_clock ms) of the packet that last touched the unit
	char     name[kObsNameMax];   // NUL terminated
};
```

**`ByteReader`** (küçük-endian okuyucu; taşmada `ok()` false olur ve sonraki okumalar sıfır döner, **asla sınır dışı okumaz**): kurucu `(const uint8_t * data, size_t len)`; `uint8_t U8()`, `uint16_t U16()`, `uint32_t U32()`, `void Skip(size_t n)`, `bool Str(char * out, uint32_t cap)` (`u8` uzunluk + bayt; uzunluk `cap - 1`'den büyükse veya veri yetmiyorsa `ok` false yapar ve `false` döner; `out` NUL ile biter), `bool ok() const`, `size_t pos() const`, `size_t remaining() const`. `data == nullptr` veya `len == 0` ise ilk okuma `ok`'u false yapar.

**`inline bool ParseUserInfo(ByteReader & r, uint16_t sid, uint64_t nowMs, UnitObs & out)`** — sunucunun `GetUserInfo` düzenini (`GetInOut`/`GetRegionUserIn`'den **sonraki**, `sid` zaten okunmuş) okur ve `r`'yi kaydın sonuna ilerletir. Alan sırası (hepsi küçük-endian, dizgeler `u8` uzunluklu):

| Sıra | Alan | Boyut | Saklanır |
|---|---|---|---|
| 1 | karakter adı (`u8` uzunluk + bayt) | 1+n | `name` |
| 2 | ulus | 1 | `nation` |
| 3 | klan kimliği (`i16`) | 2 | hayır |
| 4 | fame | 1 | hayır |
| 5 | **klan bloğu:** ittifak `u16`; klan adı (`u8` uzunluk + bayt); rütbe `u8`; sıralama `u8`; sembol sürümü `u16`; pelerin `u16` | 2+(1+m)+1+1+2+2 | hayır |
| 6 | seviye | 1 | `level` |
| 7 | ırk | 1 | `race` |
| 8 | sınıf | 2 | `cls` |
| 9 | x, z, y (×10) | 2+2+2 | `x10`, `z10`, `y10` |
| 10 | yüz, saç | 1+1 | hayır |
| 11 | `m_bResHpType` | 1 | `resHpType` |
| 12 | anormal durum | 4 | hayır |
| 13 | party aranıyor, yetki | 1+1 | hayır |
| 14 | party lideri | 1 | `partyLeader` (`!= 0`) |
| 15 | görünmezlik | 1 | `invisibility` |
| 16 | miğfer gizli | 1 | hayır |
| 17 | yön (`i16`) | 2 | hayır |
| 18 | civciv bayrağı, krallık bayrağı, iki NP rütbesi (dört `u8`) | 1+1+1+1 | hayır |
| 19 | **10 ekipman kaydı**, her biri `u32 itemNum, i16 durability, u8 flag` = 7 bayt (toplam 70) | 70 | hayır (`Skip(70)`) |
| 20 | bölge kimliği (zone) | 1 | hayır |

Klan bloğunun "klansız" biçimi (`u32 0, u16 0, u8 0, u16 0xFFFF` = 9 bayt) bu birleşik düzenle **aynı bayt sayısıdır** (ittifak 0, ad uzunluğu 0, rütbe 0, sıralama 0, sembol 0, pelerin 0xFFFF): tek ayrıştırıcı iki durumu da okur. Toplam sabit "yük" kısmı: alanlar 6–9 bittikten sonra `1+1+1+4+1+1+1+1+1+2+1+1+1+1 = 18` bayt, sonra 70 bayt ekipman, sonra 1 bayt zone. Başarıda `out.sid = sid; out.lastSeenMs = nowMs; return r.ok();`; başarısızlıkta `false` (r'nin konumu tanımsız sayılır, çağıran bırakır).

**Toplu/tek paket ayrıştırıcılar** (`data`/`len` = paket **yükü**; hepsi `bool`/`int` döner ve taşma yapmaz):

```cpp
// WIZ_USER_INOUT: u16 type, u16 sid, then the user info unless type == kObsInOutOut.
// info = false for OUT (out is untouched except out.sid). Returns false on a malformed packet.
inline bool ParseUserInOut(const uint8_t * data, size_t len, uint64_t nowMs, uint16_t & type, UnitObs & out);

// WIZ_REQ_USERIN: u16 count, then count x (u8 0, u16 sid, user info). Parses at most 'cap' entries and stops at the
// first malformed or missing one; returns the number of entries written to 'out'. A count larger than the data is
// not an error (the entries that fit are returned).
inline int ParseUserList(const uint8_t * data, size_t len, uint64_t nowMs, UnitObs * out, int cap);

// WIZ_MOVE: u16 sid, u16 x, u16 z, u16 y, i16 speed, u8 echo (x, z, y are x10). Needs 11 bytes.
inline bool ParseMove(const uint8_t * data, size_t len, uint16_t & sid, uint16_t & x10, uint16_t & z10, uint16_t & y10);

// WIZ_REGIONCHANGE: u16 count, then count x u16 sid. Writes at most 'cap' ids; returns how many (0 if malformed).
inline int ParseRegionList(const uint8_t * data, size_t len, uint16_t * ids, int cap);
```

`ParseUserInOut` tip değerini (`u16`) olduğu gibi döndürür; `type == kObsInOutOut` ise bilgi **okunmaz** (paket yalnızca 4 bayt olabilir). `ParseMove`: `len < 11` → `false`. `ParseRegionList`: `u16 count`; `count` kadar `u16` okunabildiği kadar (veri yetmezse yetenlerle durur, hata değil); sonuç `<= cap`.

**`ObsTable`** (kopyalanabilir, mutex içermez; kilidi çağıran tutar):

```cpp
class ObsTable
{
public:
	ObsTable() { Clear(); }
	void Clear();                                          // count 0, overflow 0
	int Count() const;
	uint32_t Overflow() const;                             // inserts refused because the table was full
	bool Upsert(const UnitObs & u);                        // insert or refresh by sid; false (and Overflow()++) when new and full
	void Remove(uint16_t sid);                             // WIZ_USER_INOUT OUT
	bool UpdatePosition(uint16_t sid, uint16_t x10, uint16_t z10, uint16_t y10, uint64_t nowMs);   // known units only
	bool MarkDead(uint16_t sid, uint64_t nowMs);           // known units only: resHpType = kObsUserDead
	int Retain(const uint16_t * ids, int n, uint16_t selfSid);   // drops units not listed; returns listed ids (not selfSid) unknown to the table
	const UnitObs * Find(uint16_t sid) const;
	const UnitObs & At(int i) const;                       // 0 <= i < Count()
private:
	UnitObs m_units[kObsMaxUnits];
	int m_count;
	uint32_t m_overflow;
};
```

Davranış ayrıntıları: `Upsert` var olan `sid` için **tüm** alanları yeniler (yerinde); yeni `sid` ve tablo doluysa eklemez. `Remove` boşluğu **son elemanı o yere taşıyarak** kapatır (sıra korunmaz; testler sıraya bağımlı olmaz). `UpdatePosition`/`MarkDead` bilinmeyen `sid` için `false` döner ve tabloyu **değiştirmez**; biliniyorsa `lastSeenMs = nowMs` yapar. `MarkDead` `resHpType = kObsUserDead` yazar. `Retain`: `ids` içinde olmayan her birimi çıkarır (`selfSid` listede olsa da tabloda zaten bulunmaz, bulunursa da çıkarılır: tablo botun kendisini tutmaz); dönüş: `ids` içinde `selfSid`'den farklı olup tabloda **olmayan** kimliklerin sayısı (tekrarlar ayrı sayılır, kasıtlı basitlik). Tüm sınır koşulları (boş tablo, `n == 0`, `ids == nullptr && n == 0`) güvenli olmalı; `n == 0` tabloyu **boşaltır** (bölge boş).

**`Tests/BotCoreTests/PerceptionTests.cpp`** (yeni; `#include "MiniTest.h"`, `#include <BotCore/Perception.h>`, `<vector>`, `<string>`, `<cstring>`; dosya ASCII + CRLF; `MotionTests.cpp` stili). Dosya-yerel yardımcı `struct Buf { std::vector<uint8_t> v; void U8(uint8_t); void U16(uint16_t); void U32(uint32_t); void Str(const char *); }` (küçük-endian ekler; `Str` = `u8` uzunluk + bayt). Dosya-yerel `static void AddUserInfo(Buf & b, const char * name, uint8_t nation, int16_t clan, bool withClanBlock, uint8_t level, uint8_t race, uint16_t cls, uint16_t x10, uint16_t z10, uint16_t y10, uint8_t resHp, bool leader, uint8_t invis)` tüm 20 alanı §5.2 tablosundaki sırayla yazar (`withClanBlock == false` → 9 baytlık klansız blok: `U32(0), U16(0), U8(0), U16(0xFFFF)`; true → `U16(7)` ittifak, `Str("Dragons")`, `U8(3)`, `U8(2)`, `U16(5)`, `U16(9)`). Yedi `TEST_CASE`:

- `Perception_UserInfo_NoClan`: `AddUserInfo(b, "BotWP_K", 1, 0, false, 80, 11, 105, 12740, 8900, 123, 1, true, 0)` → `ParseUserInfo` true; `r.remaining() == 0`; `sid`, `name == "BotWP_K"`, `nation 1`, `level 80`, `race 11`, `cls 105`, `x10 12740`, `z10 8900`, `y10 123`, `resHpType 1`, `partyLeader`, `invisibility 0`, `lastSeenMs` verilen değer.
- `Perception_UserInfo_WithClan`: aynı ama `withClanBlock = true`, `clan = 5`, ad `"BotMF_E"`, ulus 2, `resHp 3`, `leader false`, `invis 1`: tüm saklanan alanlar doğru; `r.remaining() == 0` (klan bloğu doğru atlandı).
- `Perception_UserInfo_Truncated`: bir geçerli kaydın **her** `len < tam uzunluk` öneki için `ParseUserInfo` `false` (ok false); 24 karakterlik ad (`Str` uzunluğu 24) → `false`; 23 karakterlik ad → `true` (ve `name` 23 karakter); `data == nullptr, len 0` → `false`.
- `Perception_ParseUserInOut`: tip 1 (IN), 3, 4, 5: tam kayıt, `type` doğru, `out` dolu; tip 2 (OUT) yalnızca `U16(2), U16(77)` (4 bayt) → `true`, `type == 2`, `out.sid == 77`; tip 1 ile 4 bayt → `false`; 3 bayt → `false`.
- `Perception_ParseUserList`: `U16(2)` + iki kayıt (`U8(0), U16(sid), info`), biri klanlı biri klansız → `ParseUserList` 2, iki kaydın alanları doğru; `U16(3)` ama yalnızca iki kayıt → 2; `cap = 1` → 1; `U16(0)` → 0; ilk kayıt kesik → 0; ikinci kayıt kesik → 1.
- `Perception_ParseMoveAndRegion`: `ParseMove` 11 baytlık geçerli (`sid 5, x10 100, z10 200, y10 3, speed 45, echo 7`) → true + değerler; 10 bayt → false. `ParseRegionList`: `U16(3), U16(1), U16(2), U16(3)` → 3, `ids == {1,2,3}`; `U16(5)` ama iki id → 2; `cap = 2` → 2; `U16(0)` → 0; 1 bayt → 0.
- `Perception_ObsTable`: doldur (`Upsert` 64 farklı sid → hepsi true, `Count() == 64`), 65. yeni sid → false ve `Overflow() == 1`; var olan sid'i yeniden `Upsert` (örn. seviyeyi değiştir) → true ve `Count() == 64`, alan güncellenmiş; `Remove` → `Count() == 63`, `Find` null, **diğer 63 sid'in hepsi bulunuyor**; bilinmeyen `Remove` zararsız; `UpdatePosition` bilinen → true + yeni konum + `lastSeenMs`, bilinmeyen → false ve `Count()` değişmez; `MarkDead` bilinen → `resHpType == 3`, bilinmeyen → false; `Retain({a, b, selfSid, zzz}, 4, selfSid)` (a, b tabloda; zzz değil) → döner 1 (`zzz`), tablo yalnızca a ve b (+ varsa başkaları çıkmış): `Count() == 2`; `Retain(nullptr, 0, self)` → 0 ve `Count() == 0`; `Clear()` sonrası `Count() == 0` ve `Overflow() == 0`.

Beklenen toplam test sayısı: 45 + 7 = **52**.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`: `#include <mutex>` ekle (`<map>`'ten sonra) ve `#include "../../BotCore/Perception.h"` (`BotCombat.h` satırının altına). Alanlar (aynı yorum/hizalama biçimi): `m_chatHasLast` grubunun altına (IOCP-yalnızca grubu bitince, atomiklerden önce):

```cpp
	std::mutex m_obsLock;                                  // guards m_obs: OnPacket() may run on any thread, see below
	BotCore::ObsTable m_obs;                               // guarded by m_obsLock: players in view, from received packets only (Perception, ADR-0017 Ek F4-12)
```

ve atomiklerin sonuna (`m_chatEcho`'nun altına):

```cpp
	std::atomic<uint32> m_obsUnresolved;                   // written by OnPacket(): ids of the last WIZ_REGIONCHANGE that were not in m_obs (no WIZ_REQ_USERIN is sent yet)
```

`BotSession.cpp`:

- Başlatıcı listesi: `m_chatEcho(0)`'dan sonra `m_obsUnresolved(0)` (`m_obs`/`m_obsLock` varsayılan kurucu; üye bildirim sırasına uy, derleyici sıra uyarısı vermemeli).
- `ResetForRespawn()`: `m_chatEcho = 0;`'ın yanına `{ std::lock_guard<std::mutex> lock(m_obsLock); m_obs.Clear(); } m_obsUnresolved = 0;`.
- **`OnPacket()` sonuna ekleme** (`WIZ_CHAT` bloğunun hemen ardı; mevcut bloklara **dokunulmaz**). Yeni `#include` gerekmiyorsa ekleme; `steady_clock` zaten kullanılıyor (`m_partyInviteAtMs` bloğundaki ms hesabını **aynen** kullan):

```cpp
	// Perception (ADR-0017 Ek F4-12): what a client would learn about the other players in view. Only the packets the
	// server sends to this session are read; nothing is fetched from other sessions, regions or the map. The payload
	// starts at contents(); the opcode is stored separately. Parsing happens before the lock is taken.
	if (opcode == WIZ_USER_INOUT || opcode == WIZ_REQ_USERIN || opcode == WIZ_REGIONCHANGE
		|| opcode == WIZ_MOVE || opcode == WIZ_DEAD)
	{
		const uint8 * data = pkt.size() > 0 ? pkt.contents() : nullptr;
		size_t len = pkt.size();
		uint64 nowMs = <steady_clock ms, same expression as the m_partyInviteAtMs block>;
		...
	}
```

  Blok gövdesi (ayrıştırma kilit dışında, tablo işlemi kilit içinde; her dalda `std::lock_guard<std::mutex> lock(m_obsLock);` yalnızca tablo çağrıları için):
  - `WIZ_USER_INOUT`: `BotCore::ParseUserInOut` → başarılıysa tip `kObsInOutOut` ise `m_obs.Remove(out.sid)`, değilse `m_obs.Upsert(out)`. Bozuk paket: **yok sayılır** (tablo değişmez).
  - `WIZ_REQ_USERIN`: `UnitObs list[BotCore::kObsMaxUnits]` (yığın, ~3.7 KB) → `ParseUserList(..., list, kObsMaxUnits)`; kilit altında her giriş için `Upsert`.
  - `WIZ_REGIONCHANGE`: `uint16 ids[BotCore::kObsMaxUnits * 4]` (256 kimlik; fazlası kırpılır) → `ParseRegionList`; kilit altında `m_obsUnresolved = (uint32)m_obs.Retain(ids, n, 0xFFFF)`. `selfSid` olarak `0xFFFF` verilir: `OnPacket()`'te botun kendi kimliği güvenle okunamaz (`m_slotId` IOCP-yalnızca), atomik kimlik alanı eklemek kapsam dışıdır. Sonuç: bot kendi kimliğini tabloya hiç koymaz (bölge yayınları göndereni dışlar) ama kendi kimliği `ids` içinde bulunduğundan "bilinmeyen" sayılır, yani `m_obsUnresolved` ham değeri botun kendisi dahildir; `see` çıktısı bunu açıklama satırıyla belirtir (§5.4). Uygulayıcı bu kararı değiştirmez.
  - `WIZ_MOVE`: `ParseMove` → kilit altında `m_obs.UpdatePosition`.
  - `WIZ_DEAD`: `len >= 2` ise `uint16` sid → kilit altında `m_obs.MarkDead`.

  `OnPacket()` bot **kendi** hareketlerinin `WIZ_MOVE` yayınını da alabilir (`SendToRegion(&result)`); bilinmeyen `sid` için `UpdatePosition` `false` döner, tablo değişmez (bot kendini tabloya koymaz: `WIZ_USER_INOUT` yayını göndereni dışlar; REQ_USERIN'de **kendi kaydı** gelirse `see` onu `self` olarak atlar, §5.4).

### 5.4 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirim (`CommandPartyChat`'in altına): `void CommandSee(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()`:** `pchat` dalının altına `see` (`CommandSee(args)`) fiilini ekle; "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast, pot, sit, stand, target, regene, pinvite, paccept, pdecline, pleave, ppromote, pkick, pchat, see)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **`CommandSee(args)`** `CommandRegene` kalıbıyla, `CommandPartyChat`'in hemen altında (`ParseSpawnList`'ten önce): `SplitWords`; tek sözcük değilse `BotManager: cmd see: usage: see <bot>`; `FindSession` (`unknown or not spawned bot '<ad|?>'`) ve faz denetimi (`<bot> not in game (phase X)`), hepsi `cmd see:` önekiyle. Sonra:
   - Kilit altında **yalnızca** `BotCore::ObsTable copy = s->m_obs;` ve `uint32 unresolved = s->m_obsUnresolved;` (kilidi hemen bırak; günlük yazma ve biçimleme **kilit dışında**).
   - Botun kendi durumu (bu planın tek "kendi nesnesi" okuması, sözleşme gereği izinli: kendi `CUser`'ı): `CUser * me = s->m_pUser;` → `me->GetNation()`, `me->GetX()`, `me->GetZ()`, `me->GetID()`.
   - Her birim için (kendi kimliği `me->GetID()` ile eşleşirse **atla**): `enemy = (u.nation != myNation)`; mesafe = `sqrt((u.x10/10 - myX)² + (u.z10/10 - myZ)²)` (float, `%.1f`); yaş = `now_ms - u.lastSeenMs` (aynı `steady_clock` ms ifadesi; negatif → 0).
   - Günlük (`WriteBotLog`, `char message[320]`): başlık `BotManager: cmd see: <bot> sees <N> unit(s) (enemies <E>, allies <A>, dropped <overflow>, unresolved <unresolved>)` ve ardından birim başına bir satır `BotManager: cmd see:   sid=<sid> <name> <enemy|ally> nation=<n> class=<cls> lvl=<l> pos=(<x>, <z>) dist=<d> <alive|dead> age=<ms>ms`. `N`, `E`, `A` kendi kaydı atlandıktan sonra sayılır. `unresolved` satırı bir kez açıklanır: başlıktan sonra `BotManager: cmd see:   (unresolved counts the last region id list incl. the bot itself; no WIZ_REQ_USERIN is sent yet)`. Boş tablo: başlık `sees 0 unit(s)`.
   - `ENABLED=0` iken `BotManager` komut yolu zaten kapalı: ek denetim gerekmez.
3. **`TickSessions()`, `BeginDespawn()`, `BuildStatusLines()`:** **değişmez** (`list` biçimi sabit). Başka hiçbir yere dokunma.

### 5.5 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`Perception.h`, `PerceptionTests.cpp`, `BotSession.cpp`, `BotManager.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı yedi yeni test adını (`Perception_UserInfo_NoClan`, `Perception_UserInfo_WithClan`, `Perception_UserInfo_Truncated`, `Perception_ParseUserInOut`, `Perception_ParseUserList`, `Perception_ParseMoveAndRegion`, `Perception_ObsTable`) içerir ve toplam test sayısı **52**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h` eşleşme vermez; `#include` satırları yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`; `grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h` boş.
- [ ] K5: **sözleşme dışı erişim yok (statik AC-LRN-03 denetimi):** `grep -nE "GetUserPtr|m_RegionUserArray|GetRegion\(|GetMap\(|GetRegionX|GetRegionZ|g_pMain|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMaxHealth|GetMana|GetMaxMana|m_buffMap|m_CoolDownList" GameServer/Bot/BotSession.cpp` ve `grep -n "CommandSee" -A80 GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|m_RegionUserArray|GetRegion\(|GetMap\(|g_pMain|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMana"` (ikinci komut yalnızca `CommandSee` gövdesine bakar) **boş**; `CommandSee` yalnızca `s->m_pUser` üzerinden `GetNation()`, `GetX()`, `GetZ()`, `GetID()` okur (`grep -n "me->" ` çıktısı yalnızca bu dört çağrı).
- [ ] K6: tablo yalnızca paketle dolar: `m_obs.` çağrıları `BotSession.cpp`'de yalnızca `OnPacket()` ve `ResetForRespawn()` içinde, `BotManager.cpp`'de yalnızca `CommandSee` içinde geçer (`grep -n "m_obs\b\|m_obs\." GameServer/Bot/*.cpp`); `m_obsLock` tutulurken günlük yazılmaz (`CommandSee`'de kilit bloğu yalnızca iki kopyalama satırını kapsar; `OnPacket()` bloğunda ayrıştırma kilit dışındadır).
- [ ] K7: mevcut kod yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-12 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki bilinçli değiştirilen satır(lar)ı gösterir ve mevcut `OnPacket()` bloklarında **silinen/değişen satır yok**; `git diff gece/2026-10-02...bot/F4-12 -- GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'` yalnızca `unknown command` mesaj satırını gösterir.
- [ ] K8: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca bot oturumlarına gelen paketler ve `see` komutu için çalışır; `Startup()`/`Tick()`/`TickSessions()`/`BuildStatusLines()`/`BeginDespawn()` ve ini okuma değişmedi; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K9: `git diff --stat gece/2026-10-02...bot/F4-12` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*` değişmemiş; `BotCore.vcxproj` ve `BotCoreTests.vcxproj` farkı yalnızca birer satır ekleme (`git diff ... | grep '^[-+]' | grep -v '^+++\|^---'` her biri tek `+` satır).
- [ ] K10: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); yeni `Perception.h`/`PerceptionTests.cpp` ASCII + CRLF; `git diff --check` boş.
- [ ] K11: `GameServer/Bot/BotSession.*`/`BotManager.*` içinde yeni `printf`, `Sleep`, `CreateThread`, `rand(` yok; `std::mutex` yalnızca `m_obsLock` için (`grep -n "mutex" GameServer/Bot/BotSession.*` yalnızca `#include <mutex>`, `m_obsLock` bildirimi ve `lock_guard` satırları); `ActionExecutor.*`, `Telemetry.*`, `ScenarioRunner.*` değişmemiş.
- [ ] K12: F4-01..F4-11 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `CheckAttack`, `CheckCastStart`, `CheckPotion`, `CheckStance`, `CheckTargetHp`, `CheckRegene`, `CheckPartyInvite`, `CheckPartyAccept`, `CheckPartyDecline`, `CheckPartyLeave`, `CheckPartyManage`, `CheckChat` her biri ≥ 1; önceki 45 testin tamamı hâlâ geçiyor.
- [ ] K13 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–6 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-12
git diff gece/2026-10-02...bot/F4-12 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-12 -- GameServer/Bot/BotSession.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-12 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h
grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h
grep -nE "GetUserPtr|m_RegionUserArray|GetRegion\(|GetMap\(|GetRegionX|GetRegionZ|g_pMain|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMaxHealth|GetMana|GetMaxMana|m_buffMap|m_CoolDownList" GameServer/Bot/BotSession.cpp
grep -n "CommandSee" -A80 GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|m_RegionUserArray|GetRegion\(|GetMap\(|g_pMain|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMana"
grep -n "m_obs\b\|m_obs\." GameServer/Bot/*.cpp
grep -n "mutex" GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp
grep -n "printf\|Sleep\|CreateThread\|rand(" GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h
file BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp
git diff --check gece/2026-10-02...bot/F4-12
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=summary`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; bir dosyadaki satırlar aynı `Tick()`'te sırayla çalışır). Botlar: Karus `BotWP_K`, `BotMF_K`; El Morad `BotWG_E`, `BotWP_E`; zone 71. Gözlem `Logs/Bot_*.log`. Senaryolar:

1. **Spawn sırası ve REQ_USERIN:** `spawn BotWP_K` ~10 sn sonra `spawn BotMF_K` (ikincisi birincinin bölgesinde doğar) → `see BotMF_K` başlığı `sees 1 unit(s)` ve satırda `BotWP_K`, `ally`, doğru `class`/`lvl`/`pos`; `see BotWP_K` de `BotMF_K`'yı gösterir (`USER_INOUT` yayını, `Upsert`).
2. **Düşman sınıflaması:** `spawn BotWG_E` (aynı bölgede) → `see BotWP_K` üçüncü birim `enemy`, `nation` farklı; `unresolved` sayısı açıklama satırıyla birlikte yazılır.
3. **Hareket:** `move BotWP_K <x> <z>` (birkaç metre) → `see BotMF_K`'de `BotWP_K`'nın `pos` değeri değişir, `age` küçük kalır (`WIZ_MOVE`); `dist` tutarlı (`BotMF_K` ile arası).
4. **Ölüm ve çıkış:** `despawn BotWG_E` → `see BotWP_K` o birimi artık göstermez (`INOUT_OUT`); bir botu öldür (`attack`/`cast` ile) → `see`'de `dead` (`WIZ_DEAD`).
5. **Bölge değişimi / bilinen sınır:** bir botu 3×3 bölge dışına yürüt (`move`) → diğerinin `see`'inde `Retain` ile düşer; yürüyen botun `unresolved` sayısı artar ve tablosu eksik kalır (bilinen sınır, F4-13); `ENABLED=0` → komut dosyası tüketilmez/log yok; `RESPAWN_CYCLES=2` ile `see` reddedilir.
6. **Gerilemesiz:** F4-01..F4-11 komutları çalışır (`move`/`attack`/`cast`/`pot`/`sit`/`target`/`regene`/`pinvite`/`paccept`/`pchat`); `tick_p95_us` ≤ 500; despawn temiz, sunucu çökmedi. İnsan istemcisi gerekmez; insan testi T-ARCH-17 (gerçek oyuncu görünür mü) ayrıca STATUS'ta.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `BotSession.h/.cpp`, `BotManager.*` ASCII + CRLF; yeni dosyalar ASCII + CRLF. Yeni `.cpp`/`.h` `GameServer` projesine eklenmez (yeni dosyalar `BotCore`/test projelerindedir).
- **Thread kuralı (`docs/13` §3):** `OnPacket()` IOCP-yalnızca olmayan bir çağırandır; tabloya yalnızca `m_obsLock` altında dokunulur ve kilit **kısa** tutulur (ayrıştırma ve günlük kilit dışında). `OnPacket()` içinde `new`/dosya/günlük yazma yok. `ByteReader` taşmada sıfır döner, sınır dışı okumaz.
- Sözleşme: yeni kod **başka oyuncu nesnelerine, bölge dizilerine, haritaya veya sunucu küresellerine** erişmez. Bu, ileride `Perception`'ın bütün dilimleri için denetim kuralıdır (K5 grep'i sonraki dilimlerin de kalıbıdır).
- Alan düzeni (§5.2 tablosu) sunucu kaynağından çıkarıldı `[D]`; çalışma zamanında ilk kez bot-bot sınanır. Beklenmedik uzunluk görürsen **uydurma**: ayrıştırıcı `false` döndürür ve paket yok sayılır; Uygulayıcı Raporu'na yaz.
- Bu planın riski: `WIZ_REQ_USERIN` ve `WIZ_USER_INOUT` kayıtlarında klan bloğunun klan-yok/klan-var biçimleri; testler ikisini de kapsar. Gerçek klanlı oyuncu insan testiyle (T-ARCH-17) doğrulanır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-12` (taban `gece/2026-10-02`) — `cc9196a [F4-12] Perception dilim 1: gorunur oyuncu tablosu + /bot see`; bu rapor ayrı commit'te.
- Değişen dosyalar ve neden:
  - `BotCore/Perception.h` (yeni): `UnitObs`, `ByteReader`, `ParseUserInfo`, `ParseUserInOut`, `ParseUserList`, `ParseMove`, `ParseRegionList`, `ObsTable` (yalnızca `<cstddef>/<cstdint>/<cstring>`, sınır dışı okumaz).
  - `Tests/BotCoreTests/PerceptionTests.cpp` (yeni): 7 `TEST_CASE` (dosya-yerel `Buf` + `AddUserInfo`).
  - `BotCore/BotCore.vcxproj`: `ClInclude Perception.h` (BotMotion.h ile Rng.h arasına).
  - `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile PerceptionTests.cpp` (MotionTests.cpp ile RngTests.cpp arasına).
  - `GameServer/Bot/BotSession.h`: `<mutex>` + `../../BotCore/Perception.h`; `m_obsLock`/`m_obs` (IOCP grubu sonu) ve `m_obsUnresolved` (atomikler sonu).
  - `GameServer/Bot/BotSession.cpp`: başlatıcıya `m_obsUnresolved(0)`, `ResetForRespawn()`'a tablo temizliği, `OnPacket()` sonuna yalnızca-ekleme algı bloğu (ayrıştırma kilit dışı, tablo kilit içi).
  - `GameServer/Bot/BotManager.h`: `CommandSee` bildirimi.
  - `GameServer/Bot/BotManager.cpp`: `see` fiil dağıtımı, `unknown command` listesine `see`, `CommandSee` (kilidi bırakıp biçimler/günlüğe yazar; yalnızca `s->m_pUser` üzerinden `GetID/GetNation/GetX/GetZ`).
- Derleme sonucu (`./tools/build.sh Release`, son satırlar; yalnızca eski `GameServerDlg.cpp` uyarıları):
  ```
  BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  `./tools/build.sh Debug` da rc=0; yeni dosyalarda/değişen satırlarda uyarı yok.
- Test sonucu: `./tools/run-tests.sh Release` ve `Debug` → `52 tests, 0 failed` (45 + 7 yeni; yedi yeni ad listede).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ Release rc=0; `Perception.h`/`PerceptionTests.cpp`/`BotSession.cpp`/`BotManager.cpp` için uyarı yok.
  - K2 ✔ Debug rc=0.
  - K3 ✔ iki yapılandırmada `52 tests, 0 failed`; yedi yeni test adı çıktıda.
  - K4 ✔ `windows.h|stdafx|GameServer|shared/` ve `std::min|std::max|new |malloc|std::vector|std::string` grep'leri boş; include'lar yalnızca `<cstddef>/<cstdint>/<cstring>`.
  - K5 ✔ iki sözleşme grep'i boş; `me->` yalnızca dört çağrı (`GetID/GetNation/GetX/GetZ`).
  - K6 ✔ `m_obs.` yalnızca `OnPacket()`/`ResetForRespawn()` (BotSession.cpp) ve `CommandSee` (BotManager.cpp); `CommandSee` kilit bloğu yalnızca iki kopyalama satırı, günlük kilit dışında; `OnPacket()` ayrıştırması kilit dışında.
  - K7 ✔ `BotSession.cpp` tek `-` satırı yalnızca başlatıcı listesi; `OnPacket()` bloklarında silme yok. `BotManager.cpp` tek `-` satırı yalnızca `unknown command` metni.
  - K8 ✔ `GameServer/` içinde yalnızca `Bot/` altındaki dört dosya değişti; `Startup/Tick/TickSessions/BuildStatusLines/BeginDespawn` ve ini okuma değişmedi; yeni ini anahtarı yok.
  - K9 ✔ `--stat` yalnızca §4'teki 8 dosya (+ bu plan dosyası); `proj-GameServer.vcxproj*` farkı boş; iki `vcxproj` farkı birer `+` satır.
  - K10 ✔ `file` tüm değişen/ yeni dosyalarda CRLF (yeni dosyalar ASCII+CRLF), `vcxproj` BOM'ları korundu; `git diff --check` boş.
  - K11 ✔ `printf/Sleep/CreateThread/rand(` yok; `mutex` yalnızca include + `m_obsLock` bildirimi + `lock_guard`; `ActionExecutor.*/Telemetry.*/ScenarioRunner.*` değişmedi.
  - K12 ✔ `CheckMoveStep`=2, diğer 12 guard en az 1; önceki 45 test hâlâ geçiyor.
  - K13 — Claude (`/plan-dogrula` çalışma zamanı); ben sunucuyu çalıştırmadım (§5.5).
- Plandan sapmalar: yok. Bir yorumdaki `new ` dizgesi K4 grep'ine takıldığı için "a fresh unit is added" olarak yeniden yazıldı (davranış aynı; grep temizliği).
- Notlar (engel değil):
  - `GetUserInfo` ekipman yuvası boşken bayt yazmasa da (`if (pItem == nullptr) continue`), `CUser::GetItem` daima `&m_sItemArray[pos]` döndürür (`User.h:557-560`), yani 70 baytlık ekipman bloğu her zaman sabit; `Skip(70)` güvenli (kod okumasıyla doğrulandı).
  - `m_obsUnresolved` botun kendi kimliğini de sayar (`selfSid` olarak `0xFFFF`); `see` bunu açıklama satırıyla belirtir (planda kararlaştırıldığı gibi).
  - `CommandSee` başlıktan hemen sonra açıklama satırını yazar (tablo boş olsa da); plandaki "başlıktan sonra" ifadesine göre.
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-12` @ `<sha>`
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
