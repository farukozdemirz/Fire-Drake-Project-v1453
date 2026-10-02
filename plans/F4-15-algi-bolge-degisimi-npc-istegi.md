# F4-15: `Perception` dilim 4 — bölge değişiminde `WIZ_REQ_NPCIN` isteği (`TickNpcIn`, CLI-20)

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-15` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-13 (`PendingIds`, `CheckUserIn`, `TickUserIn`, `m_obsLock`) — `KAPANDI` (merge `f1acc48`); F4-14 (`NpcTable`, NPC algı bloğu, `m_npcUnresolved`, `/bot npcs`) — `KAPANDI` (merge `03a5e72`); F3-05 (`BotCore`, birim test çatısı) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/14` §5.2 gözlem sözleşmesi, `docs/03` §16 (NPC satırı) ve yeni CLI-20, AC-LRN-03 / AC-ARCH-06 (statik denetim, F4-12 K5 kalıbı), MET-FAIR-01 |
| Tahmini büyüklük | M (7 kod dosyası; yeni dosya yok, `GameServer` projesine dosya eklenmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

F4-14'ün bilinen sınırını kapatır: bot yeni bir bölgeye geçince sunucu ona `WIZ_NPC_REGION` ile o bölgelerdeki NPC'lerin **kimlik listesini** yollar; NPC ayrıntısı yalnızca istenirse gelir. F4-14'te bu kimlikler yalnızca sayılıyordu (`m_npcUnresolved`), bu yüzden yürüyen botun tablosu yeni bölgede eksik kalıyordu (çalışma zamanında doğrulandı: `sees 0 npc(s) ... unresolved 25`). Gerçek istemci bilmediği NPC kimlikleri için `WIZ_REQ_NPCIN` yollar. Bu planda bot da aynısını yapar: her `Tick()`'te, canlı ve oyundaki her bot için, son `WIZ_NPC_REGION` listesinde **NPC tablosunda olmayan** kimlikleri (en çok 32) tek bir `WIZ_REQ_NPCIN` paketiyle ister; sunucunun cevabı (`WIZ_REQ_NPCIN`, F4-14'te zaten ayrıştırılıyor) tabloya işlenir. İstek hızı yeni **CLI-20** kuralıyla (≥ 1,0 sn aralık, istek başına ≤ 32 kimlik) sınırlıdır. Sonuç: bot yürüyüp yeni bir bölgeye geçince o bölgenin NPC'leri (guard tower'lar, askeri NPC'ler, canavarlar) `/bot npcs`'te görünür.

F4'ün on beşinci dilimidir (ADR-0017 Ek F4-15). Yapı F4-13 (`TickUserIn`) ile **birebir paraleldir**; kalıp aynen yeniden kullanılır. Sonraki dilimler: `PerceptionSnapshot`, betikli test dizileri.

## 2. Bağlam (okunması zorunlu)

- `docs/14` §5.2 ve `docs/03` §16: bot yalnızca bir istemcinin öğrenebileceği bilgiyi kullanır. Bu plan **istemcinin gönderdiği** bir isteği taklit eder; kimlikler yalnızca sunucunun bota yolladığı `WIZ_NPC_REGION` listesinden gelir (bot rastgele/keyfi NPC kimliği **istemez**).
- `plans/F4-13-algi-bolge-degisimi-kullanici-istegi.md` (özellikle §5.3, §5.4, §8) ve `plans/F4-14-algi-npc-canavar-tablosu.md` §2: aynı kalıplar. Bu plan onları tekrar anlatmaz; fark yalnızca paket düzeni ve sınıf/alan adlarıdır.
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `03a5e72` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:340-342` `WIZ_REQ_NPCIN` → `CUser::RequestNpcIn`; `:1260-1298` `RequestNpcIn`: yük = `u16 sayı` + sayı kez `u16 id`; sayı `> 1000` ise 1000'e kırpılır; `:1262` `m_bPointCheckFlag == false` ise **cevapsız döner** (varsayılan `true`, `GameServerDlg.cpp:61`; `AISocket.cpp:129` da `true` yapar; yani pratikte hep cevap gelir, ama cevap gelmezse `FAILED "no_result"` doğru sonuçtur); `id > 2 × NPC_BAND` (20000) veya NPC yok/ölü → atlanır; cevap `WIZ_REQ_NPCIN` = `u16 sayı` + bulunan her NPC için (`u16 id` + `GetNpcInfo`), `SendCompressed` ile gönderilir. **Cevaptaki sayı İSTENEN sayıdır** (`result.put(0, npc_count)`, `:1295`), bulunan değil: ayrıştırıcı (`ParseNpcList`) bunu F4-14'te zaten hata saymaz. **Sunucu isteğin menzilini/hızını denetlemez** → guard botun işi. (Yalnızca Delos'ta `m_sSid == 541` bir NPC için `m_bNation` yan etkisi var: zone 71'i etkilemez.)
  - `GameServer/User.cpp:28-39` `CUser::SendCompressed`: bot alıcısına sıkıştırılmamış paketi **eşzamanlı** verir. Yani `HandlePacket()` döndüğünde cevap `BotSession::OnPacket()`'e ulaşmış ve tabloya işlenmiştir.
  - `GameServer/GameServerDlg.cpp:1591-1614` `GetRegionNpcList`: `WIZ_NPC_REGION` yükü = `u16 sayı` + sayı kez `u16 id`; ölü NPC'ler listede yoktur.
  - `GameServer/Bot/BotSession.cpp:231-285` F4-14 NPC algı bloğu (`OnPacket()`): `:252` `WIZ_REQ_NPCIN` dalı (`ParseNpcList` + `Upsert`), `:263-269` `WIZ_NPC_REGION` dalı (`m_npcUnresolved = m_npcs.Retain(ids, n)`; `ids` tamponu `kNpcMaxUnits * 4` = 512). `:21`/`:26` başlatıcı listesi, `:340-358` `ResetForRespawn()` (kilitli blokta `m_obs.Clear(); m_obsPending.Clear(); m_npcs.Clear();`), `:368-378` `PeekUserInBatch`/`DropUserInBatch` (kalıp).
  - `GameServer/Bot/BotSession.h:47-50` iki yardımcı bildirimi, `:132-135` F4-13 IOCP-yalnızca alanlar, `:137-140` `m_obsLock`/`m_obs`/`m_obsPending`/`m_npcs`, `:159-161` atomikler (`m_obsUnresolved`, `m_userInEcho`, `m_npcUnresolved`).
  - `GameServer/Bot/ActionExecutor.cpp:2642-2775` `TickUserIn` (bu planın birebir kalıbı; dosyanın **sonuna** `TickNpcIn` ekle), `:36` `NextDecisionId`, `:53` `EmitFairnessReject`. `GameServer/Bot/ActionExecutor.h:143-152` `UserInOutcome`, `:300` `TickUserIn` bildirimi (sınıfın **son** üyesi).
  - `GameServer/Bot/BotManager.cpp:2661-2678` `TickSessions()` içinde `TickUserIn` çağrısı ve günlüğü (yeni çağrı **hemen altına**, aynı `else` bloğunun içine); `:2340-2410` `CommandNpcs` (`:2363-2367` kilitli kopya bloğu, `:2389` açıklama satırı `"... no WIZ_REQ_NPCIN is sent yet)"`: bu plan metni değiştirir).
  - `BotCore/Perception.h:391-471` `PendingIds` (`Set`/`Peek` şu an `const ObsTable &` alır: bu planın **tek** mevcut-kod değişikliği, §5.2), `:386-387` `kUserInMaxIds`/`kUserInMinGapMs`, `:475-498` `UserInCheck`/`UserInVerdict`/`CheckUserIn` (kalıp), `:500-` NPC bölümü, `:719` `NpcTable::Find`. `Tests/BotCoreTests/PerceptionTests.cpp:423-540` (`Perception_PendingIds_*`, `Perception_CheckUserIn`, stil), `:415` `MakeUnit`; şu an **61** test.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/Perception.h`):** `PendingIds::Set`/`Peek`'i tablo türünden bağımsız yap (üye şablon; `ObsTable` ve `NpcTable` ikisi de `Find(uint16_t)` sunar), sabitler `kNpcInMaxIds`, `kNpcInMinGapMs`, guard `CheckNpcIn`. Birim testleri (`PerceptionTests.cpp`'ye iki `TEST_CASE`).
2. **Oturum durumu (`BotSession`):** `m_npcPending` (`m_obsLock` altında), `m_npcInEcho` (atomik), IOCP-yalnızca sayaç/zamanlayıcı alanları ve iki kilitli yardımcı üye fonksiyon (`PeekNpcInBatch`, `DropNpcInBatch`). `OnPacket()`'e **yalnızca ekleme** (yeni satırlar mevcut `WIZ_NPC_REGION`/`WIZ_REQ_NPCIN` dallarının içinde; başka blok değişmez).
3. **Aksiyon (`ActionExecutor::TickNpcIn`):** `Tick()` başına bir kez, oyundaki canlı bot için; ≤ 32 kimlik, tek `WIZ_REQ_NPCIN`, `HandlePacket()` ile; sonuç yalnızca cevabın `OnPacket()`'e bıraktığı yankıdan.
4. **Çağrı (`BotManager::TickSessions`) ve `npcs` metni:** `TickNpcIn` çağrısı (bir blok) + sonuç günlüğü; `npcs` açıklama satırı ve sayaçlar.
5. **Dokümantasyon ve kayıtlar Claude'un işi** (DeepSeek dokunmaz): `docs/03` CLI-20, ADR-0017 Eki F4-15, STATUS, README.

**Kapsam dışı (yapılmayacak)**

- **Yeni komut yok.** `/bot npcin` gibi bir test sürücüsü eklenmez; istek tamamen otomatiktir (istemci gibi). Doğrulama `/bot npcs` ile yapılır.
- Bot **kendi kararıyla keyfi NPC kimliği istemez**: yalnızca son `WIZ_NPC_REGION` listesinden gelen ve NPC tablosunda bulunmayan kimlikler. `GetNpcPtr`, `m_arNpcArray`, bölge dizileri, harita **okunmaz**.
- CLI-11 (aksiyon hızı) penceresine **sayılmaz** ve ona bakılmaz (gerekçe F4-13 §5.2 / ADR-0017 Ek F4-13 madde 3: oyuncu aksiyonu değil, istemcinin otomatik ağ trafiği). Telemetri `decisions` seviyesinde `ACTION_SUBMIT`/`ACTION_RESULT` (`"type":"NpcInReq"`) yazar.
- Kullanıcı isteğinin (`TickUserIn`, `m_userIn*`, `m_obsPending` kullanımı) davranışı **değişmez**; NPC isteği kendi sayaç/zamanlayıcısını kullanır (iki istek aynı tick'te de gidebilir: ayrı paketler, ayrı CLI-19/CLI-20 sayaçları `[A]`).
- NPC HP'si, `WIZ_OBJECT_EVENT`, bayatlama temizliği, `PerceptionSnapshot`, NPC'yi kullanan karar/guard, pazarcılar: yok. `NpcTable`, `ParseNpc*`, `ObsTable`, `ParseUserList`/`ParseRegionList`, `UserInCheck`/`CheckUserIn`, mevcut `ActionExecutor` aksiyonları, `IsSamePartyMember`, `Telemetry.*`, `ScenarioRunner.*`, `ChatHandler.cpp`, `BotManager.h`, `CommandSee`: **değişmez**.
- Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez.
- İstek sırasında `m_obsLock` tutulurken `HandlePacket()` çağrılmaz (`HandlePacket` → `OnPacket()` → `m_obsLock`: aynı thread'de yeniden alınırsa kilitlenir). Kilit yalnızca `PeekNpcInBatch`/`DropNpcInBatch`'in içindedir; `ActionExecutor.cpp` `m_obsLock`'a hiç dokunmaz.
- Dokümanları (`docs/03`, `docs/13`, `docs/14`, `docs/16`, `docs/KNOWN_ISSUES.md`) güncellemek: Claude'un işi.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | §5.2: `PendingIds`'in iki imzası (şablon) + dosya sonuna ekleme (`NpcTable`'dan sonra, `namespace BotCore` içinde) |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | yalnızca ekleme: iki `TEST_CASE` (§5.2 sonu) |
| `GameServer/Bot/BotSession.h` | değiştir | §5.3: alanlar + iki üye fonksiyon bildirimi; yalnızca ekleme |
| `GameServer/Bot/BotSession.cpp` | değiştir | başlatıcı listesi, `ResetForRespawn()`, `OnPacket()` iki dalına ekleme, iki yardımcı üye fonksiyon |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `NpcInOutcome` + `TickNpcIn` bildirimi (§5.4) |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | `TickNpcIn` (dosya sonuna ekleme) |
| `GameServer/Bot/BotManager.cpp` | değiştir | `TickSessions()` çağrısı + `CommandNpcs` metin/sayaç değişikliği |

(Dokunulacak dosya sayısı 7; plan dosyası dahil 8.) `BotCore.vcxproj`, `BotCoreTests.vcxproj`, `proj-GameServer.vcxproj*`, `BotManager.h` **değişmez** (yeni dosya ve yeni komut yok). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-15 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/Perception.h` ve birim testleri

Biçim: dosyadaki mevcut kod gibi (tab, Allman, `inline`, İngilizce kısa yorum, `namespace BotCore` içinde, ASCII + CRLF). Yeni `#include` gerekmez. `std::min/max`, `std::vector`, `std::string`, `new`, `malloc` yok. Yorumlarda `shared/` ve `GameServer` dizgisi **yazma** (F4-12 K4 grep'i takılır).

**(a) `PendingIds` şablonlaştırma (mevcut koddaki tek değişiklik).** İki üye fonksiyonun yalnızca imzası değişir; gövdeler aynen kalır (`obs.Find(id) != nullptr` her iki tabloda geçerlidir):

```cpp
		// Replaces the content with the ids of 'ids' that 'obs' does not know yet. ... (mevcut yorum aynen)
		template <class TableT>
		void Set(const uint16_t * ids, int n, const TableT & obs)

		// Drops the ids that 'obs' knows by now and 'selfSid'; ... (mevcut yorum aynen)
		template <class TableT>
		int Peek(const TableT & obs, uint16_t selfSid, uint16_t * out, int cap)
```

Mevcut `Perception_PendingIds_Set`/`_PeekRemove` testleri **dokunulmadan** geçmelidir (`ObsTable` ile çağrılar tür çıkarımıyla derlenir). Sınıf yorumunu "a WIZ_REGIONCHANGE listed" → "a WIZ_REGIONCHANGE or WIZ_NPC_REGION list" olarak güncelle (tek yorum satırı).

**(b) Sabitler ve guard (dosya sonuna, `NpcTable`'dan sonra, namespace kapanışından önce; başlık yorumu `// --- region-change NPC request (ADR-0017 Ek F4-15) ---`):**

```cpp
constexpr int      kNpcInMaxIds   = 32;    // ids per WIZ_REQ_NPCIN request (CLI-20, design limit) [A]
constexpr uint32_t kNpcInMinGapMs = 1000;  // min time between two requests (CLI-20, design limit) [A]

// Guard input for a WIZ_REQ_NPCIN request (CLI-20).
struct NpcInCheck
{
	int count;               // ids the request would carry
	bool hasLast;            // a request was sent earlier in this spawn
	uint32_t sinceLastMs;    // since that request
};

enum NpcInVerdict
{
	NPCIN_OK = 0,
	NPCIN_REJECT_COUNT = 1,   // CLI-20: count < 1 or > kNpcInMaxIds (defensive; the caller already clamps)
	NPCIN_REJECT_GAP = 2      // CLI-20: previous request < kNpcInMinGapMs ago
};

// Order: count, gap. Not rate limited by CLI-11 (automatic client traffic, see ADR-0017 Ek F4-15).
inline NpcInVerdict CheckNpcIn(const NpcInCheck & c);
```

`CheckNpcIn` gövdesi `CheckUserIn`'inkinin aynısıdır (sıra: önce sayı, sonra aralık). `PendingIds`'in kapasitesi (`kObsPendingMax = 128`) NPC için de yeter (NPC tablosu da 128).

**Birim testleri** (`PerceptionTests.cpp`'nin **sonuna**; mevcut testlere dokunulmaz; dosya ASCII + CRLF). Dosya-yerel yardımcı: `static BotCore::NpcObs MakeNpc(uint16_t id)` (`memset` ile sıfır, `id` verilen, ad `"n"`); dosyada benzeri varsa onu kullan, yeni ad uydurma.

- `Perception_PendingIds_Npc`: boş `NpcTable` ile `Set({10001, 10002, 10001, 10003}, 4, npcs)` → `Count() == 3` (tekrar atlandı); tabloya `MakeNpc(10002)` `Upsert` edilince `Set({10001, 10002, 10003}, 3, npcs)` → `Count() == 2`; `Peek(npcs, 0xFFFF, out, 10)` → 2 ve `out == {10001, 10003}`; `Peek(..., cap=1)` → 1 (**kaldırmaz**: ardından `Count() == 2`); tabloya sonradan `10001` eklenir → `Peek` → 1 ve `{10003}` (`Count() == 1`, budama); `Remove({10003})` → `Count() == 0`; `ObsTable` ile çağrı hâlâ derlenir (kısa: `BotCore::ObsTable obs; pending.Set(ids, 1, obs)` → `Count() == 1`).
- `Perception_CheckNpcIn`: `hasLast = false` → `NPCIN_OK`; `count = 0` ve `count = 33` → `NPCIN_REJECT_COUNT` (`hasLast = false` olsa bile); `count = 32` → OK; `hasLast = true, sinceLastMs = 999` → `NPCIN_REJECT_GAP`; `sinceLastMs = 1000` → OK; sıra: hem `count = 0` hem `sinceLastMs = 0` iken `NPCIN_REJECT_COUNT`; sabitler: `kNpcInMaxIds == 32`, `kNpcInMinGapMs == 1000`.

Beklenen toplam test sayısı: 61 + 2 = **63**.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h` (aynı yorum/hizalama biçimi):

- Yardımcı bildirimler (`DropUserInBatch`'in altına, genel bölüm; ikisi de **IOCP thread**, içeride `m_obsLock` alır):
  ```cpp
  // IOCP thread only. Copies up to 'cap' NPC ids the NPC table still does not know (ids learned meanwhile are dropped) without removing them.
  int PeekNpcInBatch(uint16 * out, int cap);
  // IOCP thread only. Removes the ids from the NPC pending list (call after the request went out).
  void DropNpcInBatch(const uint16 * ids, int n);
  ```
- IOCP-yalnızca alanlar (`m_userInUnits`'ten sonra, `m_obsLock`'tan önce; her biri `// IOCP thread only: ...` yorumlu): `bool m_npcInHasLast;` `std::chrono::steady_clock::time_point m_npcInLast;` `uint32 m_npcInRequests;` `uint32 m_npcInUnits;` (son istek zamanı geçerli mi / ne zaman / bu spawn'daki istek sayısı / cevaplarla gelen NPC sayısı toplamı).
- Kilit korumalı (`m_npcs`'in altına, aynı grup): `BotCore::PendingIds m_npcPending;   // guarded by m_obsLock: ids of the last WIZ_NPC_REGION the NPC table did not know (Perception, ADR-0017 Ek F4-15)`. `m_obsLock` yorumu F4-14'te olduğu gibi kalır.
- Atomik (`m_npcUnresolved`'ın altına): `std::atomic<uint64> m_npcInEcho;   // written by OnPacket(): valid bit (63) | number of NPCs parsed from the last WIZ_REQ_NPCIN reply`.
- `m_npcUnresolved` yorumunu güncelle (yalnızca "ids of the last WIZ_NPC_REGION that were not in m_npcs" kalır; F4-14 yorumunda ertelenen istek ifadesi varsa kaldır).

`BotSession.cpp`:

- Başlatıcı listesi: `m_userInUnits(0)`'ın yanına `m_npcInHasLast(false), m_npcInRequests(0), m_npcInUnits(0)`; `m_npcUnresolved(0)`'ın yanına `m_npcInEcho(0)` (üye bildirim sırasına uy; derleyici sıra uyarısı vermemeli). `m_npcPending`/`m_npcInLast` varsayılan kurucu.
- `ResetForRespawn()`: mevcut kilitli bloğa `m_npcPending.Clear();` ekle; blok dışına `m_npcInHasLast = false; m_npcInRequests = 0; m_npcInUnits = 0; m_npcInEcho = 0;`.
- `OnPacket()` (F4-14 NPC algı bloğu), **yalnızca iki mevcut dala ekleme**:
  - `WIZ_NPC_REGION` dalı: mevcut `m_npcUnresolved = ...Retain(...)` satırının hemen altına (aynı kilit altında): `m_npcPending.Set(ids, n, m_npcs);`. (`Retain` önce çalışır: listede olmayanlar tablodan düşer, sonra bilinmeyenler bekleyen listeye girer.)
  - `WIZ_REQ_NPCIN` dalı: `for` döngüsü ve `NoteDropped` satırı bittikten sonra: `m_npcInEcho = (1ull << 63) | (uint64)n;` — **tablo işlemi bittikten sonra** yazılır (yürütücü yankıyı okuduğunda tablo güncel olsun). Bu dal spawn'daki toplu kaydı da işler; yankı her seferinde yazılır, yürütücü isteğinden hemen önce `0`'a sıfırladığı için karışmaz (F4-13 bulgusu 2).
- Yardımcılar (dosya sonuna): `PeekNpcInBatch` → `std::lock_guard<std::mutex> lock(m_obsLock); return m_npcPending.Peek(m_npcs, (uint16_t)0xFFFF, out, cap);`; `DropNpcInBatch` → kilit altında `m_npcPending.Remove(ids, n);`. İçlerinde günlük/yeni nesne yok.
- Mevcut hiçbir `OnPacket()` bloğunda satır silinmez/değişmez (yalnızca başlatıcı listesi ve bir yorum satırı bilinçli değişir).

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

`ActionExecutor.h`: `UserInOutcome`'dan sonra:

```cpp
// Result of ActionExecutor::TickNpcIn (ADR-0017 Ek F4-15).
struct NpcInOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed. SENT: "received" (the reply arrived). FAILED: "no_result" (no reply).
	                       // REFUSED: "bad_count" (guard CLI-20; FAIRNESS_REJECT written). NOTHING: "ok".
	int requested;         // ids in the request (0 when NOTHING)
	int received;          // NPCs the reply carried; valid only when kind == SENT
};
```

ve sınıfın **sonuna** (son üye; `TickUserIn`'in altına):

```cpp
	// Called once per Tick() for every in-game, living session. Sends one WIZ_REQ_NPCIN through CUser::HandlePacket()
	// for the ids the last WIZ_NPC_REGION listed and the NPC table does not know (at most kNpcInMaxIds) when the CLI-20
	// guard allows it (>= 1 s since the previous request). NOTHING when there is nothing to ask or the gap has not
	// passed (the ids stay pending). Result only from the reply the server published (m_npcInEcho). Not counted in the
	// CLI-11 window (automatic client traffic).
	static NpcInOutcome TickNpcIn(BotSession * s, std::chrono::steady_clock::time_point now);
```

`ActionExecutor.cpp` (dosya sonuna, `TickUserIn`'den sonra; başlık yorumu `// --- region-change NPC request slice (ADR-0017 Ek F4-15) ---`; mevcut fonksiyonlara dokunulmaz; yeni `#include` gerekmez). Mantık `TickUserIn`'in **birebir kopyası**, şu farklarla:

1. Ön koşul aynı: `user == nullptr || !user->isInGame() || user->isDead()` → `NOTHING`.
2. `uint16 batch[BotCore::kNpcInMaxIds]; int n = s->PeekNpcInBatch(batch, BotCore::kNpcInMaxIds);` (botun kendi kimliği NPC kimliği olamaz: `selfSid` parametresi yok). `n == 0` → `NOTHING`.
3. `BotCore::NpcInCheck c;` alanları `s->m_npcInHasLast` / `s->m_npcInLast` ile; `BotCore::CheckNpcIn(c)`. `NPCIN_REJECT_GAP` → olay yazma, kimlikleri düşürme, `NOTHING`. `NPCIN_REJECT_COUNT` → `EmitFairnessReject(s, user, NextDecisionId(s), "NpcInReq", "CLI-20", "bad_count", (float)n, (float)BotCore::kNpcInMaxIds)`; `s->DropNpcInBatch(batch, n)`; `REFUSED`/`"bad_count"`.
4. `decisions` açıksa `ACTION_SUBMIT`: `"decision_id", "type":"NpcInReq", "count":n`.
5. Paket: `Packet pkt(WIZ_REQ_NPCIN); pkt << uint16(n);` ardından her kimlik için `pkt << uint16(batch[i]);` (sunucu `u16 sayı` + `u16 id` okur: `User.cpp:1260-1280`). `s->m_npcInEcho = 0;` → `HandlePacket(pkt)` (süre `steady_clock` ile mikrosaniye; **kilit tutulmadan**).
6. Hemen ardından (sonuç ne olursa): `s->DropNpcInBatch(batch, n); s->m_npcInHasLast = true; s->m_npcInLast = now; s->m_npcInRequests++;`. `m_actionWindow.Record(...)` **çağrılmaz**.
7. Sonuç: `uint64 e = s->m_npcInEcho.load(); bool ok = (e & (1ull << 63)) != 0; int received = ok ? (int)(e & 0xFFFF) : 0;` `ok` ise `s->m_npcInUnits += received`.
8. `decisions` açıksa `ACTION_RESULT`: `"decision_id", "type":"NpcInReq", "ok", "reason":"received"|"no_result", "latency_us", "count":n, "received":received`.
9. `ok` → `SENT`/`"received"`; değilse `FAILED`/`"no_result"`; `requested = n`, `received` doldur.

Not: `received`, **ayrıştırılan NPC sayısıdır** (cevap başlığındaki "istenen" sayı değil); bulunamayan/ölü kimlikler cevapta yer almaz, dolayısıyla `received < requested` hata değildir.

Başlık yorumundaki "No logging, no locking" cümlesini aynen bırak.

### 5.5 `GameServer/Bot/BotManager.cpp`

1. **`TickSessions()`** canlı bot dalında, `TickUserIn` sonuç günlüğü bloğundan **hemen sonra** (aynı `else` bloğunun içinde, `m_attackActive` bloğundan önce):

   ```cpp
   NpcInOutcome npcIn = ActionExecutor::TickNpcIn(s, now);
   if (npcIn.kind == NpcInOutcome::SENT) { ... }
   ```
   Günlük (`WriteBotLog`, `char message[224]`): `SENT` → `BotManager: bot %s npcin requested %d, received %d`; `REFUSED`/`FAILED` → `BotManager: bot %s npcin failed (%s)`; `NOTHING` → günlük yok. (Bölge değişimi başına en fazla birkaç satır.) `BeginDespawn()` ile çıkılmış oturumda `TickNpcIn` zaten `NOTHING` döner (`isInGame()` kontrolü); ek dal gerekmez.
2. **`CommandNpcs`:** kilit bloğuna (`copy = s->m_npcs;` yanına, aynı kilit altında) `uint32 pending = (uint32)s->m_npcPending.Count();` ekle (kilidi bırakınca değişken kullanılır; biçimleme kilit dışında). Açıklama satırını (`BotManager.cpp:2389`, şu an düz `WriteBotLog("...")` ile) `snprintf` ile (`char note[256]`) şu biçime çevir: `BotManager: cmd npcs:   (unresolved counts the ids of the last WIZ_NPC_REGION list that the table did not know; npcin requests %u, npcs received %u, pending %u)` — `s->m_npcInRequests`, `s->m_npcInUnits`, `pending`. Başlık satırı ve NPC satırları **değişmez**.
3. `CommandSee()`, `BuildStatusLines()`, `BeginDespawn()`, `ExecuteCommand()`, `Tick()`, `Startup()`: **değişmez**.

### 5.6 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter; `Perception.h`, `PerceptionTests.cpp`, `BotSession.cpp`, `ActionExecutor.cpp`, `BotManager.cpp` için uyarı çıktısı boş.
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı iki yeni test adını (`Perception_PendingIds_Npc`, `Perception_CheckNpcIn`) içerir ve toplam test sayısı **63**; `Debug` aynı; önceki 61 testin tamamı değişmeden geçer (`Perception_PendingIds_Set`, `Perception_PendingIds_PeekRemove` dahil).
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h` boş; `#include` satırları hâlâ yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`; `grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h` boş.
- [ ] K5: **sözleşme dışı erişim yok (statik AC-LRN-03 denetimi):** `grep -nE "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|GetRegionX|GetRegionZ|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMaxHealth|GetMana|GetMaxMana|m_buffMap|m_CoolDownList" GameServer/Bot/BotSession.cpp` boş; `grep -n "TickNpcIn" -A80 GameServer/Bot/ActionExecutor.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionNpcArray|GetRegion\(|GetMap\(|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMana"` boş (yalnızca `TickNpcIn` gövdesi); `TickNpcIn` botun kendi `CUser`'ından yalnızca `isInGame()`, `isDead()`, `GetSocketID()` okur.
- [ ] K6: kimlikler yalnızca `WIZ_NPC_REGION` listesinden gelir: `grep -n "m_npcPending" GameServer/Bot/*.cpp` yalnızca `BotSession.cpp`'de (`OnPacket()` NPC_REGION dalı, `ResetForRespawn()`, `PeekNpcInBatch`, `DropNpcInBatch`) ve `BotManager.cpp`'de (`CommandNpcs` kilit bloğundaki tek `Count()` satırı) geçer; `ActionExecutor.cpp` `m_npcPending`'e **dokunmaz** (yalnızca `PeekNpcInBatch`/`DropNpcInBatch`); `Packet pkt(WIZ_REQ_NPCIN)` `GameServer/Bot/` içinde **yalnızca** `TickNpcIn`'de geçer (`grep -n "WIZ_REQ_NPCIN" GameServer/Bot/*.cpp`: `BotSession.cpp` algı bloğu, `TickNpcIn` ve `BotManager.cpp` açıklama satırı).
- [ ] K7: kilit disiplini: `grep -n "m_obsLock" GameServer/Bot/ActionExecutor.cpp` **boş**; `HandlePacket(` çağrısı `m_obsLock` tutulurken yapılmaz; `BotSession.cpp`'de yeni `lock_guard` yalnızca `PeekNpcInBatch` ve `DropNpcInBatch`'te (mevcut `OnPacket()`/`ResetForRespawn()` kilitleri aynen); `CommandNpcs` kilit bloğu yalnızca kopyalama satırlarını (`copy`, `unresolved`, `pending`) kapsar.
- [ ] K8: mevcut kod yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-15 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki bilinçli değişen satırları gösterir; `ActionExecutor.cpp`/`ActionExecutor.h` farkında **silinen satır yok**; `BotManager.cpp` farkında `-` satırları yalnızca `CommandNpcs` açıklama satırı (`WriteBotLog("... no WIZ_REQ_NPCIN ...")` → `snprintf`); `BotCore/Perception.h` farkında `-` satırları yalnızca `PendingIds::Set` ve `Peek` imzaları ile sınıf yorumu (en çok 3-4 satır); `PerceptionTests.cpp` farkında silinen satır **yok**.
- [ ] K9: CLI-11'e sayılmaz: `grep -n "TickNpcIn" -A80 GameServer/Bot/ActionExecutor.cpp | grep "m_actionWindow"` boş; `Perception_CheckNpcIn` `actionsInWindow` alanı içermez. Kullanıcı isteği dokunulmaz: `TickUserIn` gövdesi farkta yok.
- [ ] K10: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca bot oturumları için çalışır; `Startup()`/`Tick()`/`BuildStatusLines()`/`BeginDespawn()`/ini okuma/`CommandSee` değişmedi; `GameServer/` içinde `Bot/` dışında dosya değişmemiş; yeni ini anahtarı yok.
- [ ] K11: `git diff --stat gece/2026-10-02...bot/F4-15` yalnızca §4'teki 7 dosyayı (ve plan dosyasını) gösterir; `*.vcxproj*` farkı boş.
- [ ] K12: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı: ASCII + CRLF); `git diff --check` boş.
- [ ] K13: `GameServer/Bot/` içinde yeni `printf`, `Sleep`, `CreateThread`, `rand(` yok (yalnızca `snprintf`); `std::mutex` sayısı `BotSession.h`'de hâlâ 1 (`grep -c "std::mutex" GameServer/Bot/BotSession.h`); `Telemetry.*`, `ScenarioRunner.*`, `BotManager.h` değişmemiş.
- [ ] K14: F4-01..F4-14 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `CheckAttack`, `CheckCastStart`, `CheckPotion`, `CheckStance`, `CheckTargetHp`, `CheckRegene`, `CheckPartyInvite`, `CheckPartyAccept`, `CheckPartyDecline`, `CheckPartyLeave`, `CheckPartyManage`, `CheckChat`, `CheckUserIn` her biri ≥ 1; `CheckNpcIn` ≥ 1.
- [ ] K15 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–5 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-15
git diff gece/2026-10-02...bot/F4-15 -- GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp GameServer/Bot/ActionExecutor.cpp GameServer/Bot/ActionExecutor.h BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp | grep '^-' | grep -v '^---'
git diff gece/2026-10-02...bot/F4-15 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h
grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h
grep -nE "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionUserArray|m_RegionNpcArray|GetRegion\(|GetMap\(|GetRegionX|GetRegionZ|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMaxHealth|GetMana|GetMaxMana|m_buffMap|m_CoolDownList" GameServer/Bot/BotSession.cpp
grep -n "TickNpcIn" -A80 GameServer/Bot/ActionExecutor.cpp | grep -E "GetUserPtr|GetNpcPtr|m_arNpcArray|m_RegionNpcArray|GetRegion\(|GetMap\(|g_pMain|CNpc|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMana"
grep -n "TickNpcIn" -A80 GameServer/Bot/ActionExecutor.cpp | grep "m_actionWindow"
grep -n "m_npcPending" GameServer/Bot/*.cpp
grep -n "WIZ_REQ_NPCIN" GameServer/Bot/*.cpp
grep -n "m_obsLock" GameServer/Bot/ActionExecutor.cpp
grep -c "std::mutex" GameServer/Bot/BotSession.h
grep -n "printf\|Sleep\|CreateThread\|rand(" GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h
file BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/ActionExecutor.cpp GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp
git diff --check gece/2026-10-02...bot/F4-15
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. **AIServer de açık olmalıdır** (NPC'ler AIServer'dan gelir; `status` ile üç `[UP]` doğrula). Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`). Botlar: Karus `BotWP_K`, `BotMF_K`; El Morad `BotWG_E`; zone 71. Gözlem `Logs/Bot_*.log` ve `Logs/bots/<tarih>/live-*.jsonl`. Senaryolar (F4-14 doğrulamasında ölçülen yol: arena Karus tarafı; koordinatlar `list` çıktısıyla ayarlanır):

1. **Bölge değişimi, bilinmeyen NPC'ler istenir:** `spawn BotWP_K` → `npcs BotWP_K` (spawn'da F4-14'te 21 NPC görüldü; `npcin requests 0`) → `move` ile arena dışına (ör. `1380 893`) → `npcs BotWP_K` → `sees 0 npc(s)` → `move` ile tower halkasına geri (ör. `1380 1060`) → varınca `Logs/Bot_*.log`'da `BotWP_K npcin requested N, received M` (N ≈ F4-14'te görülen `unresolved 25`, N ≤ 32, 0 < M ≤ N) ve `npcs BotWP_K` listesinde tower'lar (proto 5400/5410, `nation=1`, `alive`, `dist` botun konumuyla tutarlı) **görünür**; `npcs` açıklama satırında `npcin requests` ≥ 1, `npcs received` ≥ 1, `pending 0`. Telemetri: `ACTION_SUBMIT`/`ACTION_RESULT` `"type":"NpcInReq"`, `"count"`, `"received"`, `"ok":true`, `"reason":"received"`. Satırların `name/type/lvl/pos` değeri F4-14'te spawn'da görülenle ve yerel DB `K_NPC` ile (yasak tablo değil) tutarlı olmalı.
2. **Hız sınırı / 32 sınırı:** art arda iki bölge değişiminde ikinci `NpcInReq` en az 1 sn sonra gider (`ACTION_SUBMIT` zaman damgaları farkı ≥ 1000 ms); `FAIRNESS_REJECT` yazılmaz. >32 bekleyen kimlik üretilemezse (arena çevresinde 25 civarı) "gözlenmedi" yazılır (kod yolu `Peek` `cap` testli; kriteri düşürmez). `gap` dalı gerçek trafikle tetiklenemezse F4-13'te olduğu gibi "kısmi" kabul edilir.
3. **Spawn'da gereksiz istek yok:** `spawn BotWP_K`, ~14 sn sonra `spawn BotMF_K` → ikisinde de `npcin requests 0` (spawn'daki `WIZ_REQ_NPCIN`/`WIZ_NPC_REGION` tabloyu doldurdu, bekleyen liste boş), jsonl'de `NpcInReq` yok; `npcs` hâlâ doğru.
4. **Kullanıcı isteği bozulmadı:** aynı yürüyüşte `see` açıklama satırında `userin requests` sayaçları F4-13 davranışıyla uyumlu (bağımsız artar); ölü/çıkmış bot istek yollamaz (`despawn all` temiz, slot sızıntısı yok, sunucu çökmedi).
5. **Gerilemesiz:** F4-01..F4-14 komutları çalışır (`move`/`attack`/`cast`/`pot`/`sit`/`target`/`regene`/`pinvite`/`paccept`/`pchat`/`see`/`npcs`); `tick_p95_us` ≤ 500; `ENABLED=0` → komut dosyası tüketilmez/log yok. İnsan istemcisi gerekmez; insan ölçümü T-PERC-01 (`docs/STATUS.md`, `WIZ_REQ_NPCIN` kapsamı F4-15 ile eklenecek) ayrıca bekler.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `Perception.h`, `PerceptionTests.cpp`, `BotSession.*`, `ActionExecutor.*`, `BotManager.cpp` ASCII + CRLF; yeni dosya yok.
- **Thread kuralı (`docs/13` §3):** `m_npcPending` yalnızca `m_obsLock` altında; `OnPacket()` her thread'den çağrılabilir. **`HandlePacket()` çağrısı sırasında hiçbir kilit tutulmaz** (cevap aynı thread'de `OnPacket()` içinde `m_obsLock` ister: kilit tutulursa kilitlenir). `PeekNpcInBatch`/`DropNpcInBatch` kısa kilit, kopyalama dışında iş yok. `OnPacket()` içinde yeni nesne/dosya/günlük yok; `Set` en çok 128 `uint16` işler.
- **Yankı sırası:** `m_npcInEcho` ancak tablo `Upsert`'leri bittikten sonra yazılır; yürütücü `HandlePacket()` döndükten sonra okur (eşzamanlı yayın, `User.cpp:28-39`).
- **Sayı semantiği:** sunucunun cevap başlığındaki sayı *istenen* sayıdır; `received` ve `m_npcInUnits` **ayrıştırılan** NPC sayısıdır. Bozuk/boş cevap `received = 0` ile yankı yazar; `FAILED "no_result"` yalnızca hiç cevap gelmediyse (ör. `m_bPointCheckFlag == false`). Hiçbir durumda uydurma NPC eklenmez.
- **Tekrar istenmeme:** sunucu bulunamayan/ölü kimlikleri sessizce atlar; o kimlikler listede tekrar istenmez (`DropNpcInBatch` isteğin hemen ardından çalışır); yeni `WIZ_NPC_REGION` yeni liste kurar. NPC yeniden doğunca `WIZ_NPC_INOUT` IN gelir (F4-14).
- **Bölge listesi kısalığı:** `WIZ_NPC_REGION` `Retain` önce tabloyu budar. Bu yüzden iki hızlı bölge değişiminde ikinci liste birincinin bekleyen kimliklerini **değiştirir** (`Set` önceki içeriği siler): istenmemiş kimlikler kaybolabilir; bölge dışında kalmışlarsa zaten gereksizdir (tasarım, F4-13 ile aynı).
- Beklenmedik bir şey görürsen **uydurma**: Uygulayıcı Raporu'na yaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

(henüz yok)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
