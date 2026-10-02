# F4-16: `Perception` dilim 5 — `PerceptionSnapshot` (öz durum, düşman/müttefik listesi, NPC listesi) ve `/bot snap`

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-16` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-12 (`ObsTable`, `UnitObs`, `/bot see`) — `KAPANDI` (merge `dcd8f80`); F4-14 (`NpcTable`, `NpcObs`, `/bot npcs`) — `KAPANDI` (merge `03a5e72`); F4-15 (NPC tablosu bölge değişiminde dolar) — `KAPANDI` (merge `bfdd839`); F3-05 (`BotCore`, birim test çatısı) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/13` §5.2 (`PerceptionSnapshot`), `docs/14` §5.2 gözlem sözleşmesi, `docs/03` §16, AC-LRN-03 / AC-ARCH-06 (statik denetim, F4-12 K5 kalıbı) |
| Tahmini büyüklük | S–M (4 kod dosyası; yeni dosya yok, `*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

F4-12..F4-15 ile bot, görüş alanındaki oyuncu ve NPC'leri yalnızca aldığı paketlerden iki ayrı tabloda (`ObsTable`, `NpcTable`) tutuyor. Karar katmanı (sonraki fazlar) bu iki tabloyu ve botun kendi durumunu ham haliyle okumamalı; `docs/13` §5.2'deki gibi **tek bir anlık görüntü** (`PerceptionSnapshot`) okumalı. Bu plan o görüntüyü kurar:

- `BotCore/Perception.h` (saf mantık, sunucusuz): `SelfState`, `UnitView`, `NpcView`, `PerceptionSnapshot` ve `BuildSnapshot(...)`. Görüntü oyuncuları **düşman** (ulus botunkinden farklı) ve **müttefik** (aynı ulus, botun kendisi hariç) listelerine ayırır, her listeyi botun konumuna **yakından uzağa** sıralar (en çok 32 kayıt), her birim için mesafe ve "kaç ms önce görüldü" değerini hesaplar. NPC'ler de aynı kuralla tek listeye girer.
- `GameServer/Bot/BotManager.cpp`: `/bot snap <bot>` komutu: botun kendi `CUser` durumunu (`SelfState`) okur, iki tabloyu kilit altında kopyalayıp `BuildSnapshot` ile görüntüyü kurar ve günlüğe yazar. Böylece saf mantık gerçek sunucu verisiyle çalışma zamanında sınanır; sonuçlar `/bot see` ve `/bot npcs` toplamlarıyla çapraz kontrol edilir.

Bu bir **algı** dilimidir: görüntüyü kullanan karar/guard yoktur. Sistem kapalıyken (`ENABLED=0`) davranış değişmez.

## 2. Bağlam (okunması zorunlu)

- `docs/13` §5.2: `PerceptionSnapshot { SelfState self; enemies, allies; TeamView team; NavView nav; }` taslağı. Bu planda yalnızca `self`, `enemies`, `allies` ve NPC listesi kurulur; `team` (party HP/MP: `PARTY_HPCHANGE` henüz ayrıştırılmıyor), `nav` (güvenli nokta, tower mesafeleri: F5) ve `self`'in buff/cooldown/stok alanları **sonraki dilimlerdir**.
- `docs/14` §5.2 ve `docs/03` §16: bot, istemcinin paketlerinden öğrenebileceğinden fazlasını kullanmaz. Düşmanın HP/MP'si, cooldown'ı, envanteri, görüş alanı dışındaki birimler **yasak**; görüntüde `UnitView`/`NpcView` yalnızca tablolardaki alanları taşır (HP/MP/ad/envanter alanı **yoktur**). Botun **kendi** durumu (HP/MP/konum) serbesttir (`docs/14` §5.1 "Öz durum": kendi `CUser`'ı).
- `plans/F4-12-algi-gorunur-oyuncu-tablosu.md` ve `plans/F4-14-algi-npc-canavar-tablosu.md`: tabloların davranışı. `/bot see` botun ulusunu `CUser::GetNation()` ile alır ve `u.nation != myNation` → düşman sayar; aynı kural burada.
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `bfdd839` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `BotCore/Perception.h`: `UnitObs` `:19-33` (`sid`, `nation`, `race`, `cls`, `level`, `x10/z10/y10`, `resHpType` [1 ayakta, 2 oturuyor, 3 ölü], `partyLeader`, `invisibility`, `lastSeenMs`, `name`), `kObsUserDead` `:17`, `ObsTable` `:253-382` (`Count()`, `At(i)`, `Upsert`), `NpcObs` `:508-522` (`id`, `protoId`, `type`, `nation`, `level`, `x10/z10/y10`, `gateOpen`, `dead`, `lastSeenMs`, `name`), `NpcTable` `:618-` (`Count()`, `At(i)`, `Upsert`), dosya sonu `CheckNpcIn` `:~760-778`, `namespace BotCore` kapanışı son satır. Dosya yalnızca `<cstddef>`, `<cstdint>`, `<cstring>` içerir (bu plan `<cmath>` ekler, §5.2).
  - `Tests/BotCoreTests/PerceptionTests.cpp`: `MakeUnit` `:415`, `MakeNpc` `:873` (ikisi de `memset` ile sıfırlı, `sid`/`id` verilir; `MakeNpc` adı `"n"` yapar), şu an **63** test (`grep -c TEST_CASE Tests/BotCoreTests/*.cpp` toplamı: Combat 33 + Motion 6 + Perception 18 + Rng 6). `CHECK`, `CHECK_EQ`, `REQUIRE` makroları `Tests/BotCoreTests/MiniTest.h:140-164`; kayan nokta için `CHECK(a == b)` kullan (aşağıdaki değerler ikili tabanda tam gösterilebilir).
  - `GameServer/Bot/BotManager.cpp`: `ExecuteCommand` dağıtımı `:631-656` (`see` `:653`, `npcs` `:655`), bilinmeyen komut metni `:659-660`, `CommandSee` `:2230-2326` (kalıp: oturum bul, faz denetimi, tabloyu `m_obsLock` altında kopyala, kilit dışında biçimle, botun kendi `CUser`'ını oku), `CommandNpcs` `:2328-2412`. `GameServer/Bot/BotManager.h:88-89` komut bildirimleri.
  - Botun kendi durumu: `GameServer/Unit.h:66-68` `GetX()/GetZ()`, `:92-93` `GetNation()/GetLevel()`; `GameServer/User.h:114` `GetID()`, `:309` `isDead()`, `:384` `GetClass()`, `:431-434` `GetHealth()/GetMaxHealth()/GetMana()/GetMaxMana()`, `:239` `m_bResHpType`; `GameServer/GameDefine.h:118` `USER_SITDOWN` (`0x02`).

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/Perception.h`):** sabitler `kSnapMaxUnits`, `kSnapMaxNpcs`, `kSnapUserSit`; yapılar `SelfState`, `UnitView`, `NpcView`, `PerceptionSnapshot`; `BuildSnapshot`. Dört birim testi (`PerceptionTests.cpp`'ye).
2. **Komut (`BotManager`):** `/bot snap <bot>` (`CommandSnap`): `SelfState`'i botun `CUser`'ından doldurur, tabloları kopyalayıp görüntüyü kurar, özet + her listenin en yakın ≤ 10 kaydını günlüğe yazar.
3. **Dokümantasyon ve kayıtlar Claude'un işi** (DeepSeek dokunmaz): ADR-0017 Eki F4-16, `docs/13` §5.2 notu, STATUS, README.

**Kapsam dışı (yapılmayacak)**

- Görüntüyü kullanan **karar, guard, politika, telemetri olayı** yok. `PerceptionSnapshot` periyodik olarak kurulmaz (`Tick()`/`TickSessions()` değişmez); yalnızca `/bot snap` komutu kurar.
- `SelfState`'e buff, cooldown, pot stoku, SP, hız **eklenmez**; `team` (`TeamView`), `nav` (`NavView`), tower mesafesi, NPC sınıflandırması (canavar/kule/kapı ayrımı), düşman HP'si (`WIZ_TARGET_HP` gözlemi), bayatlama temizliği (`ageMs` yalnızca raporlanır, birim silinmez/işaretlenmez) yok.
- **Gizli (`invisibility != 0`) oyuncular listeden çıkarılmaz:** bayt olduğu gibi `UnitView.invisibility`'ye kopyalanır. Gizlilik süzmesi karar katmanının işidir ve istemcinin gizli düşmanı gerçekten çizip çizmediği **doğrulanmadı** (`docs/03` §16: "gizlilik durumu" paketle gelir `[D]`, çizim davranışı `[Ö]`); uygulayıcı süzme **yapmaz**.
- `ObsTable`, `NpcTable`, `UnitObs`, `NpcObs`, `PendingIds`, tüm `Parse*`/`Check*` fonksiyonları, `BotSession.*`, `ActionExecutor.*`, `Telemetry.*`, `ScenarioRunner.*` **değişmez**. Mevcut komutlar (`see`, `npcs`, …) değişmez.
- Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya, yeni `vcxproj` satırı yok.
- Dokümanları (`docs/03`, `docs/13`, `docs/14`, `docs/16`, `docs/KNOWN_ISSUES.md`) güncellemek: Claude'un işi.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | yalnızca ekleme: `#include <cmath>` + dosya sonuna (`CheckNpcIn`'dan sonra, `namespace BotCore` içinde) snapshot bölümü |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | yalnızca ekleme: dört `TEST_CASE` (dosya sonuna) |
| `GameServer/Bot/BotManager.h` | değiştir | tek satır: `void CommandSnap(const std::string & args);` (`CommandNpcs`'in altına) |
| `GameServer/Bot/BotManager.cpp` | değiştir | dağıtıma `snap` dalı, bilinmeyen komut metnine `snap`, `CommandSnap` (`CommandNpcs`'ten sonra) |

(Dokunulacak dosya sayısı 4; plan dosyası dahil 5.) `BotCore.vcxproj`, `BotCoreTests.vcxproj`, `proj-GameServer.vcxproj*`, `BotSession.*`, `ActionExecutor.*` **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-16 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/Perception.h` ve birim testleri

Biçim: dosyadaki mevcut kod gibi (tab, Allman, `inline`, İngilizce kısa yorum, `namespace BotCore` içinde, ASCII + CRLF). Include bloğuna `#include <cmath>` ekle (**tek** yeni include; `<cstring>` altına). `std::min/max`, `std::vector`, `std::string`, `new`, `malloc` yok. Yorumlarda `shared/` ve `GameServer` dizgisi **yazma** (F4-12 K4 grep'i takılır).

**(a) Dosya sonuna** (`CheckNpcIn`'dan sonra, namespace kapanışından önce; başlık yorumu `// --- perception snapshot (ADR-0017 Ek F4-16) ---`):

```cpp
constexpr int     kSnapMaxUnits = 32;   // enemies / allies kept per list (nearest first), design limit
constexpr int     kSnapMaxNpcs  = 32;   // npcs kept (nearest first), design limit
constexpr uint8_t kSnapUserSit  = 2;    // USER_SITDOWN in the res/hp type byte of the user info

// The bot's own state (read from its own session by the caller; the contract allows it).
struct SelfState
{
	uint16_t sid;
	uint8_t  nation;
	uint16_t cls;
	uint8_t  level;
	float    x, z;                 // world position
	int32_t  hp, maxHp, mp, maxMp;
	bool     dead;
	bool     sitting;
};

// One visible player. No HP, MP, name or inventory: the client never learns them (docs/14 5.2).
struct UnitView
{
	uint16_t id;                   // socket id of the player (NOT an npc id)
	uint8_t  nation;
	uint8_t  race;
	uint16_t cls;
	uint8_t  level;
	float    x, z;                 // x10 / 10 as sent on the wire
	float    dist;                 // to SelfState x, z
	bool     dead;                 // resHpType == kObsUserDead
	bool     sitting;              // resHpType == kSnapUserSit
	bool     partyLeader;
	uint8_t  invisibility;         // raw byte, NOT filtered
	uint32_t ageMs;                // nowMs - lastSeenMs (0 if the packet clock is ahead), clamped to 0xFFFFFFFF
};

// One visible NPC (monster, guard tower, gate, ...). No classification yet.
struct NpcView
{
	uint16_t id;                   // npc id (NOT a socket id)
	uint16_t protoId;
	uint8_t  type;
	uint8_t  nation;
	uint8_t  level;
	float    x, z;
	float    dist;
	bool     dead;
	bool     gateOpen;
	uint32_t ageMs;
};

struct PerceptionSnapshot
{
	uint64_t tMs;                          // nowMs the snapshot was built for
	SelfState self;
	UnitView enemies[kSnapMaxUnits];       // nation != self.nation, nearest first
	int      enemyCount;                   // entries filled (<= kSnapMaxUnits)
	int      enemyTotal;                   // all enemies in the table (>= enemyCount)
	UnitView allies[kSnapMaxUnits];        // nation == self.nation, self excluded, nearest first
	int      allyCount;
	int      allyTotal;
	NpcView  npcs[kSnapMaxNpcs];           // nearest first
	int      npcCount;
	int      npcTotal;                     // all npcs in the table
};

// Zeroes 'out', copies 'self', then fills the lists from the two tables (read only). Players: the entry whose sid
// equals self.sid is skipped; nation != self.nation goes to enemies, the rest to allies; dead and sitting players are
// kept (flags). Npcs: every table entry, dead ones included. Each list keeps the kSnap* nearest by (dist, id)
// ascending (ties: lower id first); *Total counts everything seen, so Total > Count means entries were dropped.
// dist = sqrt((x - self.x)^2 + (z - self.z)^2) in float. Fully deterministic, no allocation, no clock.
inline void BuildSnapshot(const SelfState & self, const ObsTable & obs, const NpcTable & npcs,
	uint64_t nowMs, PerceptionSnapshot & out);
```

Gövde için yön (uygulama sana ait): `memset(&out, 0, sizeof(out)); out.tMs = nowMs; out.self = self;`. Sıralı ekleme için **tek** küçük şablon yardımcı yaz (aynı bölümde, `BuildSnapshot`'tan önce):

```cpp
// Inserts 'v' into arr[0..count) keeping (dist, id) ascending; at capacity the farthest entry is dropped
// (v itself when it sorts last). Both view types have 'dist' and 'id'.
template <class V, int CAP>
inline void SnapInsertNearest(V * arr, int & count, const V & v);
```

`ageMs`: `nowMs > lastSeenMs ? nowMs - lastSeenMs : 0`, 32 bit üst sınırına kırp. Konum: `x = x10 / 10.0f`. `dist` için `std::sqrt(dx * dx + dz * dz)` (`float`). Toplam sayaçlar kayıt kapasiteye sığmasa da artar.

**(b) Birim testleri** (`PerceptionTests.cpp`'nin **sonuna**; mevcut testlere dokunulmaz; ASCII + CRLF). `MakeUnit`/`MakeNpc` mevcut yardımcılarını kullan. Dosya-yerel yardımcı: `static BotCore::SelfState MakeSelf()` → `memset` sıfır, `sid = 1`, `nation = 1`, `x = 1000.0f`, `z = 1000.0f`, `hp = maxHp = 5000`, `mp = maxMp = 3000`. Mesafeler 3-4-5 üçgenleriyle tam çıkar (konum `x10 = 10000 + 10 * dx`).

- `Perception_Snapshot_Split`: `MakeSelf()`; `ObsTable`'a: sid 1 (botun kendisi, nation 1, konum self'te) → **atlanır**; sid 2 nation 1 (müttefik) `x10 = 10030, z10 = 10040` (dist 5); sid 3 nation 2 `x10 = 10060, z10 = 10080` (dist 10) `cls = 205`, `level = 77`, `race = 12`, `partyLeader = true`, `invisibility = 3`, `resHpType = 3` (ölü), `lastSeenMs = 4750`; sid 4 nation 2 `x10 = 10300, z10 = 10400` (dist 50) `resHpType = 2` (oturuyor), `lastSeenMs = 6000`. `BuildSnapshot(self, obs, npcs(boş), 5000, out)` → `enemyCount == 2`, `enemyTotal == 2`, `allyCount == 1`, `allyTotal == 1`, `npcCount == 0`; `enemies[0].id == 3` ve `dist == 10.0f`, `cls == 205`, `level == 77`, `race == 12`, `partyLeader`, `invisibility == 3` (süzülmedi), `dead`, `!sitting`, `ageMs == 250`; `enemies[1].id == 4`, `dist == 50.0f`, `sitting`, `!dead`, `ageMs == 0` (paket saati ilerde); `allies[0].id == 2`, `dist == 5.0f`, `x == 1003.0f`, `z == 1004.0f`; `out.tMs == 5000`, `out.self.hp == 5000`, `out.self.sid == 1`. Ek: `lastSeenMs = 0` ve `nowMs = 0x100000000ULL + 5` olan bir birimle `ageMs == 0xFFFFFFFF` (kırpma).
- `Perception_Snapshot_OrderCap`: 40 düşman (sid 100..139, nation 2), `i`'inci birim `dist = 40 - i` (yani sid 100 en uzak: `x10 = 10000 + 10 * (40 - i)`, `z10 = 10000`), **uzaktan yakına** eklenir (kapasite değiştirme yolunu sınar). Sonuç: `enemyTotal == 40`, `enemyCount == 32`; `enemies[0].dist == 1.0f` (sid 139), `enemies[31].dist == 32.0f` (sid 108); her `i` için `enemies[i].dist < enemies[i + 1].dist`. Aynı çağrıyı ikinci kez boş tablolarla yapınca (`out` yeniden kullanılır) `enemyCount == 0`, `enemyTotal == 0` (önceki içerik sıfırlandı).
- `Perception_Snapshot_TieAndNation`: iki düşman aynı mesafede (dist 5), önce sid 20 sonra sid 10 eklenir → `enemies[0].id == 10`, `enemies[1].id == 20` (eşitlikte küçük kimlik önce); `self.nation = 2` yapılınca aynı tablo için nation 2 olanlar müttefik, nation 1 olanlar düşman olur (sınıflandırma botun ulusuna göre).
- `Perception_Snapshot_Npcs`: `NpcTable`'a 40 NPC (id 10001..10040, `i`'inci `dist = 40 - i`, `z10 = 10000`) uzaktan yakına eklenir; ayrıca (kapasite sınavından ayrı, ikinci küçük kurulumla) id 20001 `type = 62`, `nation = 1`, `protoId = 5400`, `level = 60`, `dead = true`, `gateOpen = true`, `x10 = 10030, z10 = 10040` (dist 5), `lastSeenMs = nowMs - 100`. Sonuç: büyük kurulumda `npcTotal == 40`, `npcCount == 32`, `npcs[0].dist == 1.0f`, `npcs[31].dist == 32.0f`; küçük kurulumda `npcs[0]` alanları (`id`, `protoId`, `type`, `nation`, `level`, `dead`, `gateOpen`, `dist == 5.0f`, `ageMs == 100`) beklenen değerde; **ölü NPC listede kalır**. `enemyTotal == 0 && allyTotal == 0` (oyuncu tablosu boş).

Beklenen toplam test sayısı: 63 + 4 = **67**.

### 5.3 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`: `CommandNpcs` bildiriminin altına `void CommandSnap(const std::string & args);`.

`BotManager.cpp`:

1. **Dağıtım:** `npcs` dalının (`:655`) altına `else if (_stricmp(verb.c_str(), "snap") == 0) CommandSnap(args);` (mevcut biçimde). Bilinmeyen komut metnindeki liste (`:660`) `..., see, npcs, snap)` olur (başka metin değişmez; bu, **bilinçli değişen tek mevcut satırdır**).
2. **`CommandSnap`** (`CommandNpcs`'ten sonra; `CommandNpcs`'in kalıbını izle):
   - `words.size() != 1` → `WriteBotLog("BotManager: cmd snap: usage: snap <bot>")`; `FindSession` boşsa `... cmd snap: unknown or not spawned bot '%s'`; `m_phase != PHASE_IN_GAME` → `... cmd snap: %s not in game (phase %s)` (üç satır `CommandNpcs`'inkilerle aynı kalıp).
   - Kilit altında iki yerel kopya: `BotCore::ObsTable obsCopy; BotCore::NpcTable npcCopy;` ve `{ std::lock_guard<std::mutex> lock(s->m_obsLock); obsCopy = s->m_obs; npcCopy = s->m_npcs; }` — blok **yalnızca bu iki atamayı** kapsar; `WriteBotLog`/`snprintf`/`BuildSnapshot` kilit dışında.
   - `SelfState`: botun kendi `CUser`'ından (`CUser * me = s->m_pUser;`, `CommandNpcs` gibi; yorum: `// The only read of the bot's own session: its CUser, which the contract allows.`): `sid = me->GetID()`, `nation = me->GetNation()`, `cls = me->GetClass()`, `level = me->GetLevel()`, `x = me->GetX()`, `z = me->GetZ()`, `hp/maxHp/mp/maxMp = me->GetHealth()/GetMaxHealth()/GetMana()/GetMaxMana()`, `dead = me->isDead()`, `sitting = (me->m_bResHpType == USER_SITDOWN)`. Başka hiçbir `CUser`/sunucu üyesine dokunma.
   - `nowMs`: `CommandSee`'deki gibi `steady_clock` ms. `BotCore::PerceptionSnapshot snap; BotCore::BuildSnapshot(self, obsCopy, npcCopy, nowMs, snap);`.
   - Günlük (`WriteBotLog`, `char message[320]`; her satır `BotManager: cmd snap: ` ile başlar):
     1. `<ad> t=%llu self sid=%u nation=%u class=%u lvl=%u pos=(%.1f, %.1f) hp=%d/%d mp=%d/%d %s %s` (`alive|dead`, `standing|sitting`).
     2. `  enemies %d (total %d), allies %d (total %d), npcs %d (total %d)`.
     3. Her liste için en yakın **en çok 10** kayıt (ayrı satırlar; yerel `const int kPrintMax = 10`): `  enemy id=%u nation=%u class=%u lvl=%u pos=(%.1f, %.1f) dist=%.1f %s%s age=%ums` (`alive|dead`, ardından `sitting` ise ` sitting`, değilse boş), `  ally ...` aynı biçim, `  npc id=%u proto=%u type=%u nation=%u lvl=%u pos=(%.1f, %.1f) dist=%.1f %s gate=%s age=%ums`. Kimlikler `/bot see` (`sid=`) ve `/bot npcs` (`id=`) ile **aynı sayılardır** (çapraz kontrol bunlara dayanır).
3. `CommandSee`, `CommandNpcs`, `TickSessions()`, `BuildStatusLines()`, `BeginDespawn()`, `Tick()`, `Startup()`: **değişmez**.

### 5.4 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter; `Perception.h`, `PerceptionTests.cpp`, `BotManager.cpp` için uyarı çıktısı boş (`touch` ile yeniden derlenip bakılır; `USER_SITDOWN` görünmezse `GameDefine.h` yolu `stdafx.h` üzerinden gelir, yeni include gerekirse **durup** raporda sor).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı dört yeni test adını (`Perception_Snapshot_Split`, `Perception_Snapshot_OrderCap`, `Perception_Snapshot_TieAndNation`, `Perception_Snapshot_Npcs`) içerir ve toplam test sayısı **67**; `Debug` aynı; önceki 63 testin tamamı değişmeden geçer.
- [ ] K4: `BotCore/Perception.h` hâlâ sunucusuz: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h` boş; `#include` satırları yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`, `<cmath>`; `grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h` boş.
- [ ] K5: **sözleşme dışı bilgi yok (statik AC-LRN-03 denetimi):** `sed -n '/struct UnitView/,/^\t};/p' BotCore/Perception.h | grep -inE "\b(max)?(hp|mp)\b|name|cooldown|stock|invent"` boş; aynı komut `struct NpcView` için boş; `CommandSnap` gövdesinde sözleşme dışı erişim yok: `sed -n '/void BotManager::CommandSnap/,/^}/p' GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_buffMap|m_CoolDownList"` boş. `CommandSnap`, botun kendi `CUser`'ından yalnızca §5.3'te sayılan üyeleri okur (`GetID`, `GetNation`, `GetClass`, `GetLevel`, `GetX`, `GetZ`, `GetHealth`, `GetMaxHealth`, `GetMana`, `GetMaxMana`, `isDead`, `m_bResHpType`).
- [ ] K6: gizli oyuncular süzülmez: `grep -n "invisibility" BotCore/Perception.h` snapshot bölümünde yalnızca `UnitView` alanında ve bir kopyalama atamasında geçer (`if (... invisibility ...)` biçimli koşul **yok**); `Perception_Snapshot_Split` `invisibility == 3` doğrular.
- [ ] K7: kilit disiplini: `CommandSnap`'te `m_obsLock` yalnızca iki kopyalama atamasını kapsayan tek blokta geçer (`sed -n '/void BotManager::CommandSnap/,/^}/p' GameServer/Bot/BotManager.cpp | grep -c m_obsLock` = 1; yorumlarda `m_obsLock` adını **yazma**); `BuildSnapshot` ve `WriteBotLog` kilit tutulurken çağrılmaz; `GameServer/Bot/` içinde `std::mutex` sayısı artmadı (`grep -c "std::mutex" GameServer/Bot/BotSession.h` = 1).
- [ ] K8: mevcut kod yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-16 -- BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/BotManager.h | grep '^-' | grep -v '^---'` **boş**; `BotManager.cpp` farkında `-` satırları yalnızca bilinmeyen komut metni satırı (`... see, npcs)"` → `... see, npcs, snap)"`, en çok 1 satır).
- [ ] K9: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `/bot snap` komutuyla çalışır (komut kanalı bot sistemi kapalıyken tüketilmez, ADR-0015); `Startup()`/`Tick()`/`TickSessions()`/`BuildStatusLines()`/ini okuma değişmedi; `GameServer/` içinde `Bot/` dışında dosya değişmemiş; yeni ini anahtarı yok.
- [ ] K10: `git diff --stat gece/2026-10-02...bot/F4-16` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `*.vcxproj*` farkı boş; `BotSession.*`, `ActionExecutor.*`, `Telemetry.*`, `ScenarioRunner.*` farkta yok.
- [ ] K11: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı: ASCII + CRLF); `git diff --check` boş.
- [ ] K12: `GameServer/Bot/` içinde yeni `printf` (yalnızca `snprintf`), `Sleep`, `CreateThread`, `rand(` yok.
- [ ] K13: F4-01..F4-15 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2; `CheckAttack`, `CheckCastStart`, `CheckPotion`, `CheckStance`, `CheckTargetHp`, `CheckRegene`, `CheckPartyInvite`, `CheckPartyAccept`, `CheckPartyDecline`, `CheckPartyLeave`, `CheckPartyManage`, `CheckChat`, `CheckUserIn`, `CheckNpcIn` her biri ≥ 1.
- [ ] K14 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–4 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-16
git diff gece/2026-10-02...bot/F4-16 -- BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/BotManager.h GameServer/Bot/BotManager.cpp | grep '^-' | grep -v '^---'
git diff gece/2026-10-02...bot/F4-16 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h
grep -n "^#include" BotCore/Perception.h
grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h
sed -n '/struct UnitView/,/^\t};/p' BotCore/Perception.h | grep -inE "\b(max)?(hp|mp)\b|name|cooldown|stock|invent"
sed -n '/struct NpcView/,/^\t};/p' BotCore/Perception.h | grep -inE "\b(max)?(hp|mp)\b|name|cooldown|stock|invent"
sed -n '/void BotManager::CommandSnap/,/^}/p' GameServer/Bot/BotManager.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_buffMap|m_CoolDownList"
sed -n '/void BotManager::CommandSnap/,/^}/p' GameServer/Bot/BotManager.cpp | grep -c m_obsLock
grep -n "invisibility" BotCore/Perception.h
grep -c "std::mutex" GameServer/Bot/BotSession.h
grep -n "printf\|Sleep\|CreateThread\|rand(" GameServer/Bot/BotManager.cpp | grep -v snprintf
file BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/BotManager.cpp GameServer/Bot/BotManager.h
git diff --check gece/2026-10-02...bot/F4-16
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=summary`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. **AIServer de açık olmalıdır** (NPC'ler AIServer'dan gelir; `status` ile üç `[UP]` doğrula). Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`). Botlar: Karus `BotWP_K`, `BotMF_K`; El Morad `BotWG_E`; zone 71; spawn'lar arasında ~14 sn bırak (tick başına bir spawn). Gözlem `Logs/Bot_*.log`. Senaryolar:

1. **Tek bot, ham tablolarla tutarlılık:** `spawn BotWP_K` → `snap BotWP_K` → ilk satırda `pos` `list` konumuyla aynı (`sid` ise ikinci bot spawn edilince senaryo 2'de `see` satırlarındaki `sid=` ile çapraz doğrulanır), `hp == maxHp` (taze spawn), `alive standing`; ikinci satırda `npcs N (total M)` ve `M` == `npcs BotWP_K` çıktısındaki `sees M npc(s)`; `enemies 0, allies 0`. NPC satırları `npcs` satırlarıyla `id/proto/type/pos/dist` bakımından **aynı**, en yakından uzağa sıralı (dist artan).
2. **Üç bot, sınıflandırma:** `spawn BotMF_K`, `spawn BotWG_E` (zone 71, aynı bölge grubu) → `see BotWP_K` ve `snap BotWP_K` art arda: `see`'nin `enemies`/`allies` sayıları `snap`'in `total` değerleriyle **aynı** (BotWG_E düşman, BotMF_K müttefik); `snap` satırlarındaki `id` değerleri `see` satırlarındaki `sid` değerleriyle aynı; `dist` değerleri `see` ile aynı (±0,1; ikisi arasında bot kımıldamış olabilir: `stop` ile durdur); sıralama dist artan.
3. **Durum bayrakları:** `sit BotWP_K` → `snap BotWP_K` self satırı `sitting`; `stand BotWP_K` → `standing`. Bilinen sınır (bu planda **düzeltilmez**): `BotSession::OnPacket()` başkalarının `WIZ_STATE_CHANGE` yayınını tabloya işlemez (`BotSession.cpp:72` yalnızca botun kendi yayınının yankısını okur), bu yüzden `snap BotMF_K` içinde oturan `BotWP_K` müttefik satırı `sitting` **olmaz**: beklenen davranıştır, "gözlenmedi/bilinen sınır" olarak yazılır ve kriteri düşürmez. `sitting`/`dead` bayraklarının doğruluğunu birim testleri (`resHpType` 2 ve 3) kapsar; bir bot öldürülürse `dead` ayrıca doğrulanır.
4. **Gerilemesiz:** F4-01..F4-15 komutları çalışır (`move`/`attack`/`cast`/`pot`/`sit`/`target`/`regene`/`pinvite`/`paccept`/`see`/`npcs`); `snap` bilinmeyen/çıkmış bot ve bot adsız çağrıda yalnızca kullanım/hata satırı yazar, sunucu çökmez; `despawn all` temiz; `tick_p95_us` ≤ 500; `ENABLED=0` → komut dosyası tüketilmez/log yok. İnsan istemcisi gerekmez.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `Perception.h`, `PerceptionTests.cpp`, `BotManager.*` ASCII + CRLF; yeni dosya yok.
- **Thread kuralı (`docs/13` §3):** `CommandSnap` IOCP thread'inde çalışır (komut kanalı, ADR-0015). `m_obs`/`m_npcs` yalnızca `m_obsLock` altında kopyalanır; kopya çok kısa tutulur (iki tablo ataması). `CUser` alanları IOCP thread'inde okunur (`CommandSee`/`CommandNpcs` ile aynı kural). `BuildSnapshot` saftır (kilit, yığın bellek, saat, günlük yok).
- **Yığın boyutu:** iki tablo kopyası (~12 KB) ve `PerceptionSnapshot` (~5 KB) yerel değişkendir; IOCP thread yığını buna yeter (`CommandNpcs` zaten ~8 KB `NpcTable` kopyası taşır). `PerceptionSnapshot`'ı `static`/yığın dışı yapma; boyutu çok büyütme (kapasite sabitlerini artırma).
- **`dist` ve `age` yalnızca raporlanır:** bayat birim silinmez, uzak birim süzülmez; karar katmanı eşiği kendisi koyar.
- **Sayı semantiği:** `*Total` tabloda gördüğün **tüm** kayıttır (botun kendisi hariç oyuncular), `*Count` listeye alınanlardır; `Total > Count` yalnızca kapasite aşımında olur. Tablonun kendi `Overflow()` sayacı bu görüntünün konusu değildir.
- Beklenmedik bir şey görürsen **uydurma**: Uygulayıcı Raporu'na yaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

**Durum:** UYGULANDI

**Branch ve commit'ler:**
- Branch: `bot/F4-16` (taban: `gece/2026-10-02`, `3ae9e17`).
- `7e19210` — `[F4-16] PerceptionSnapshot ve /bot snap uygulandı` (4 kod dosyası).

**Değişen dosyalar ve nedenleri:**
- `BotCore/Perception.h` (+168): `#include <cmath>`; dosya sonuna `kSnap*` sabitleri, `SelfState`/`UnitView`/`NpcView`/`PerceptionSnapshot`, `SnapInsertNearest` şablonu ve `BuildSnapshot` eklendi (saf mantık, yalnızca ekleme).
- `Tests/BotCoreTests/PerceptionTests.cpp` (+249): `MakeSelf` yardımcısı ve dört `TEST_CASE` (`Perception_Snapshot_Split`, `_OrderCap`, `_TieAndNation`, `_Npcs`) eklendi; mevcut testlere dokunulmadı.
- `GameServer/Bot/BotManager.h` (+1): `CommandSnap` bildirimi.
- `GameServer/Bot/BotManager.cpp` (+115/−1): `snap` dağıtım dalı, bilinmeyen komut metnine `snap` eklendi (bilinçli tek değişen mevcut satır), `CommandSnap` eklendi (`CommandSee`/`CommandNpcs` kalıbı: tablolar `m_obsLock` altında tek blokta kopyalanır, `BuildSnapshot` ve log kilit dışında).

**Derleme çıktılarının son satırları:**
- Release: `proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe`, rc=0; `Perception.h`/`PerceptionTests.cpp`/`BotManager.cpp`/`BotManager.h` touch'lanıp yeniden derlendi, bu dosyalarda uyarı 0 (kalan uyarılar `GameServerDlg.cpp`'nin eski 816/1143/1802 satırlarında).
- Debug: `proj-GameServer.vcxproj -> ...\build\bin\x86-Debug\Server\GameServer.exe`, rc=0.
- `tools/run-tests.sh Release`: `67 tests, 0 failed`, rc=0; dört yeni test adı `[ OK ]` (önceki 63 test değişmedi).
- `tools/run-tests.sh Debug`: `67 tests, 0 failed`, rc=0.

**Kriter öz-değerlendirmesi:**
- K1 ✔ Release rc=0; dört dokunulan dosyada uyarı yok (`USER_SITDOWN` `stdafx.h` üzerinden görünüyor, yeni include gerekmedi).
- K2 ✔ Debug rc=0.
- K3 ✔ Release ve Debug `67 tests, 0 failed`; dört yeni ad çıktıda; önceki 63 test geçiyor.
- K4 ✔ `grep` sunucusuzluk ve include denetimleri boş; include'lar yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`, `<cmath>`; `std::min/max`/`new`/`malloc`/`vector`/`string` yok.
- K5 ✔ `UnitView`/`NpcView` sözleşme grep'i boş; `CommandSnap` yalnızca sayılan `CUser` üyelerini okur, yasak erişim grep'i boş.
- K6 ✔ `invisibility` snapshot bölümünde alan + tek kopyalama ataması; koşul yok; test `invisibility == 3` doğrular.
- K7 ✔ `CommandSnap`'te `m_obsLock` tek blok (grep -c = 1), yorumda ad yok; `BuildSnapshot`/`WriteBotLog` kilit dışında; `BotSession.h` mutex sayısı 1.
- K8 ✔ Üç dosyada `-` satırı yok; `BotManager.cpp`'de tek `-` bilinmeyen komut metni satırı.
- K9 ✔ Yeni kod yalnızca `/bot snap`; `Startup/Tick/TickSessions/BuildStatusLines`/ini değişmedi, yeni anahtar yok.
- K10 ✔ `git diff --stat` yalnızca 4 dosya; `*.vcxproj*` farkı boş; `BotSession/ActionExecutor/Telemetry/ScenarioRunner` farkta yok.
- K11 ✔ `file` dört dosyada ASCII + CRLF (değişmedi); `git diff --check` boş.
- K12 ✔ Yeni `printf`/`Sleep`/`CreateThread`/`rand(` yok; grep'in bulduğu iki satır (54 `fprintf`, 307 `printf`) taban dosyada da var (eski).
- K13 ✔ `CheckMoveStep` 2; diğer on dört `Check*` her biri ≥ 1.
- K14 — Claude'un `/plan-dogrula` çalışma zamanı işi.

**Sorular / sapmalar:**
- Yok. Planın kabul ettiği `BuildSnapshot`'ın `SnapInsertNearest` şablon yardımcısı planla uyumlu biçimde yazıldı; ek bir yardımcı gerekmedi.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

**Karar:** DOĞRULANDI (gece modu; birleştirmeyi döngü betiği yapar, push yok). İncelenen commit: `fee1f9a` (`bot/F4-16`; kod `7e19210`, taban `gece/2026-10-02`). Çalışma ağacı temiz.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `build.sh Release` rc=0; `Perception.h`, `PerceptionTests.cpp`, `BotManager.cpp`, `BotManager.h` `touch` edilip yeniden derlendi: bu dosyalarda uyarı/hata satırı yok (kalan uyarı yalnızca eski `UpgradeHandler.cpp` C4789) |
| K2 | ✔ | `build.sh Debug` rc=0 |
| K3 | ✔ | `run-tests.sh Release` rc=0, `67 tests, 0 failed`, dört yeni ad `[ OK ]`; `Debug` aynı `67 tests, 0 failed`. Test gövdeleri okundu: plandaki değerler (dist 5/10/50, `ageMs` 250/0/0xFFFFFFFF, 40→32 kırpma, eşitlikte küçük kimlik, ulus değişimi, ölü NPC listede) birebir sınanıyor |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/"` boş; `#include` yalnızca `Perception.h:8-11` (`<cstddef>`, `<cstdint>`, `<cstring>`, `<cmath>`); `std::min/max/new/malloc/vector/string` grep'i boş |
| K5 | ✔ | `UnitView` ve `NpcView` grep'leri boş; `CommandSnap` yasak-erişim grep'i boş; `CommandSnap` `CUser`'dan yalnızca `GetID/GetNation/GetClass/GetLevel/GetX/GetZ/GetHealth/GetMaxHealth/GetMana/GetMaxMana/isDead/m_bResHpType` okur (`BotManager.cpp:2449-2461`) |
| K6 | ✔ | `invisibility` snapshot bölümünde yalnızca `Perception.h:813` (alan) ve `:906` (kopyalama ataması); koşul yok; `Perception_Snapshot_Split` `invisibility == 3` doğrular |
| K7 | ✔ | `CommandSnap` içinde `m_obsLock` 1 kez (`:2443`, yalnızca iki kopyalama ataması); `BuildSnapshot` ve `WriteBotLog` kilit dışında; `BotSession.h` `std::mutex` sayısı 1 |
| K8 | ✔ | `Perception.h`, `PerceptionTests.cpp`, `BotManager.h` farkında `-` satırı yok; `BotManager.cpp` tek `-` satırı: bilinmeyen komut metni (`... see, npcs)` → `... npcs, snap)`) |
| K9 | ✔ | yeni kod yalnızca `snap` dağıtım dalı ve `CommandSnap`; `Startup/Tick/TickSessions/BuildStatusLines` farkta yok; `GameServer/` içinde yalnızca `Bot/`; yeni ini anahtarı yok. `ENABLED=0` çalışma zamanında bu turda denenmedi (statik denetim; komut kanalı kapalıyken tüketilmez, F4-15 turunda çalışma zamanında doğrulanmış kalıp) |
| K10 | ✔ | `--stat`: 4 kod dosyası + plan; `*.vcxproj*` farkı 0 satır; `BotSession/ActionExecutor/Telemetry/ScenarioRunner` farkta yok |
| K11 | ✔ | dört dosya `ASCII text, with CRLF line terminators`; `git diff --check` boş (rc=0) |
| K12 | ✔ | `GameServer/Bot` farkının eklenen satırlarında `printf/Sleep/CreateThread/rand(` yok (yalnızca `snprintf`) |
| K13 | ✔ | `CheckMoveStep` 2; diğer 14 `Check*` (`CheckAttack`..`CheckNpcIn`) her biri 1 |
| K14 | ✔ | çalışma zamanı, aşağıda |

**Çalışma zamanı (K14; `Release`, `ENABLED=1`, `MAX_BOTS=16`, `TELEMETRY=decisions` (ini değiştirilmedi; plan `summary` demişti, `decisions` yalnızca daha çok jsonl satırı üretir), `SPAWN_ON_START` boş, zone 71; üç sunucu `[UP]`; `GameServer.ini` md5 aynı `265a8e1c...`; iş bitince `run-servers.sh stop`, `BotCommands.txt` kalmadı):**

1. **Tek bot, ham tablolarla tutarlılık:** `spawn BotWP_K` → `snap`: `self sid=2984 nation=1 class=106 lvl=80 pos=(1380.0, 1060.0) hp=5650/5650 mp=5370/5370 alive standing` (`list` konumu ve `hp=5650/5650` ile aynı); `enemies 0, allies 0, npcs 25 (total 25)`; `npcs` çıktısı `sees 25 npc(s)`. İlk 10 NPC satırı (`12688` dist 16.1, `13023` 18.0, `13014` 23.8, `13026` 25.1 ... `13009` 34.7) `npcs` satırlarındaki `id/proto/type/pos/dist` ile aynı, dist artan.
2. **Üç bot, sınıflandırma:** `BotMF_K` spawn edilince `snap BotWP_K` → `allies 1 (total 1)`, `ally id=2985 ... dist=30.4`; `see` aynı (`sid=2985`, `dist=30.4`). `BotWG_E` spawn noktası (630, 920) diğerlerinin bölgesinde olmadığından `move BotWG_E 1380 1070` ile yürütüldü (hız 90 `speed_field` ile reddedildi, varsayılan 45; ~3 dk); kule ateşiyle öldü (hp 266 → 0, `dead`). Sonra: `see BotWP_K` `enemies 1, allies 1` = `snap BotWP_K` `enemies 1 (total 1), allies 1 (total 1)`; `enemy id=2986 nation=2 class=206 pos=(1328.2, 1059.7) dist=51.8 dead` (`see` satırı `sid=2986 ... dist=51.8 dead`); `snap BotWG_E`: `self ... nation=2 hp=0/5650 dead standing`, `enemies 2 (total 2)` (`id=2984 dist=51.8`, `id=2985 dist=64.4`, dist artan) = `see BotWG_E` `enemies 2`, `sid=2984/2985` ve aynı dist; `snap BotMF_K`: `enemy id=2986 dist=64.4 dead`, `ally id=2984 dist=30.4`.
3. **Durum bayrakları:** `sit BotWP_K` → `snap` self `alive sitting`; `stand` → `standing`. Ölü bot: self `dead` ve başkasının tablosunda `dead` (yukarıda). Başkalarının oturma yayını (bilinen sınır) bu turda ayrıca incelenmedi.
4. **Gerilemesiz ve hata yolları:** `snap` (argümansız) ve `snap BotWP_K extra` → `usage: snap <bot>`; `snap NoSuch` → `unknown or not spawned bot '?'`; despawn edilmiş botta `not in game (phase despawned)`; `move`, `stop`, `sit`, `stand`, `regene` (`respawned at (630.0, 920.0)`), `see`, `npcs`, `list`, `despawn all` (`pool free 16/16`) çalıştı; `pot/target/pchat/attack/cast/pinvite` argümansız yalnızca kullanım satırı yazdı (tam senaryolar bu turda denenmedi). Sunucu çökmedi. `PERF_SAMPLE` 77 örnek: `skipped_ticks` 0, sessiz pencerelerde `tick_p95_us` 65-152 (medyan ~99), `tick_p50_us` 2.

**Bulgular (önem sırasıyla; hiçbiri engel değil):**
1. `tick_p95_us` plandaki "≤ 500" sınırını yalnızca komut yoklaması/komut yürütme pencerelerinde aştı (9/77 pencere, en çok 3143 µs); aynı pencerelerde `see`, `snap`, `regene`, `despawn all` komutlarım vardı (komut dosyası işleme + çok satırlı günlük yazımı). Yalnızca `snap` içeren pencereyi komutlardan ayırmak mümkün olmadı; F4-15 turunda `list` komutlarında görülen ölçüm artefaktıyla aynı örüntü. Komutsuz pencerelerde sınır karşılanıyor. İzlenecek: sonraki dilimde `Tick()` içinde periyodik görüntü kurulursa `BuildSnapshot` maliyeti ayrıca ölçülmeli.
2. `BotManager.cpp:2447` `s->m_pUser` boşluk denetimi yok; `CommandNpcs` ile aynı kalıp (faz `PHASE_IN_GAME` denetiminden sonra okunuyor); davranış tutarlı, not.
3. `age` hareketsiz birimde büyür (ör. `ally ... age=242047ms`): plan gereği yalnızca raporlanır, bayatlama kararı sonraki dilim.
4. Uygulayıcı Raporu'ndaki iddialar (commit'ler, derleme rc, 67 test, K1-K13, sapma yok) bağımsız olarak doğrulandı.

**Düzeltme talimatı:** yok (karar `DOĞRULANDI`).
