# F4-13: `Perception` dilim 2 — bölge değişiminde `WIZ_REQ_USERIN` isteği (`TickUserIn`, CLI-19)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-02, gece/2026-10-02, merge `f1acc48`) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-13` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-12 (`ObsTable`, `OnPacket()` algı bloğu, `m_obsLock`) — `KAPANDI` (merge `dcd8f80`); F3-05 (`BotCore`, birim test çatısı) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/14` §5.2 gözlem sözleşmesi, `docs/03` §16 ve yeni CLI-19, AC-LRN-03 / AC-ARCH-06 (statik denetim, F4-12 K5 kalıbı), MET-FAIR-01 |
| Tahmini büyüklük | M (8 dosya; yeni dosya yok, `GameServer` projesine dosya eklenmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

F4-12'nin bilinen sınırını kapatır: bot yeni bir bölgeye geçince sunucu ona `WIZ_REGIONCHANGE` ile o bölgelerdeki oyuncuların **kimlik listesini** yollar; kimliklerin ayrıntısı yalnızca istenirse gelir. Gerçek istemci bilmediği kimlikler için `WIZ_REQ_USERIN` yollar. Bu planda bot da aynısını yapar: her `Tick()`'te, canlı ve oyundaki her bot için, son `WIZ_REGIONCHANGE` listesinde **tabloda olmayan** kimlikleri (en çok 32) tek bir `WIZ_REQ_USERIN` paketiyle ister; sunucunun cevabı (`WIZ_REQ_USERIN`, F4-12'de zaten ayrıştırılıyor) tabloya işlenir. İstek hızı yeni **CLI-19** kuralıyla (≥ 1,0 sn aralık, istek başına ≤ 32 kimlik) sınırlıdır. Sonuç: bot hareket edip yeni bir bölgeye geçince o bölgenin oyuncuları `/bot see`'de görünür.

F4'ün on üçüncü dilimidir (ADR-0017 Ek F4-13). Sonraki dilimler: NPC/canavar gözlemi, `PerceptionSnapshot`, betikli test dizileri.

## 2. Bağlam (okunması zorunlu)

- `docs/14` §5.2 (gözlem sözleşmesi) ve `docs/03` §16: bot yalnızca bir istemcinin öğrenebileceği bilgiyi kullanır. Bu plan **istemcinin gönderdiği** bir isteği taklit eder; kimlikler yalnızca sunucunun bota yolladığı `WIZ_REGIONCHANGE` listesinden gelir (bot rastgele/keyfi kimlik **istemez**).
- `docs/13` §3 thread kuralı, `docs/03` CLI-10 / CLI-11 satırları (guard sözleşmesi örneği).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `dcd8f80` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:337-339` `WIZ_REQ_USERIN` → `CUser::RequestUserIn`; `:1230-1253` `RequestUserIn`: yük = `u16 sayı` + sayı kez `u16 sid`; sayı `> 1000` ise 1000'e kırpılır; her `sid` için `GetUserPtr`, kullanıcı yok veya oyunda değilse atlanır; cevap `WIZ_REQ_USERIN` = `u16 bulunan` + bulunan kez (`u8 0, u16 sid, GetUserInfo`), `SendCompressed` ile **her zaman** gönderilir (hiç kullanıcı bulunmasa da `bulunan = 0`). **Sunucu isteğin menzilini/hızını denetlemez** → guard botun işi.
  - `GameServer/User.cpp:28-39` `CUser::SendCompressed`: bot alıcısına sıkıştırılmamış paketi **eşzamanlı** verir (`m_botSink->OnPacket`). Yani `HandlePacket()` döndüğünde cevap `BotSession::OnPacket()`'e ulaşmış ve tabloya işlenmiştir.
  - `GameServer/User.cpp:243-` `CUser::HandlePacket`: oyun içi paketler `:305` `switch (command)`e düşer; `WIZ_REQ_USERIN` bot için de erişilebilirdir.
  - `GameServer/GameServerDlg.cpp:1320-1338` `UserInOutForMe` (spawn'da REQ_USERIN), `:1340-1358` `RegionUserInOutForMe` (`WIZ_REGIONCHANGE`: `u16 sayı` + sayı kez `u16 sid`, göndericinin kendisi dahil).
  - `GameServer/Bot/BotSession.cpp:169-222` F4-12 algı bloğu (`OnPacket()`); `:202-208` `WIZ_REGIONCHANGE` dalı: `m_obsUnresolved = Retain(ids, n, 0xFFFF)`; `:194-200` `WIZ_REQ_USERIN` dalı (`ParseUserList` + `Upsert`). `BotSession.cpp:25` başlatıcı listesi, `:227-` `ResetForRespawn()`.
  - `GameServer/Bot/BotSession.h` alanlar (`m_obsLock`/`m_obs` IOCP grubunun sonunda, atomikler `m_chatEcho`/`m_obsUnresolved` ile biter).
  - `GameServer/Bot/ActionExecutor.cpp:36` `NextDecisionId`, `:53` `EmitFairnessReject`, `:1441-` `RequestTargetHp` (tek atımlık aksiyon + guard + yankı sıfırlama + gecikme ölçümü + `ACTION_SUBMIT`/`ACTION_RESULT` kalıbı); `:1380-` desenin `ACTION_RESULT` biçimi. Dosyanın **sonuna** ekleme yap (`RequestChatParty` bitişinden sonra).
  - `GameServer/Bot/ActionExecutor.h:283` sınıf sonu; yeni sonuç yapısı `ChatOutcome`'dan sonra, sınıf bildirimi sınıfın **son** üyesi olarak.
  - `GameServer/Bot/BotManager.cpp:2433` `TickSessions()`; canlı bot dalı: `:2530-` `if (s->m_phase == PHASE_IN_GAME)` bloğu, ölü/canlı ayrımı (`isDead()`), canlı dalda `ActionExecutor::TickMove(s, now)` ve sonuç günlüğü. `:2228-` `CommandSee`; `:2297` açıklama satırı `"... no WIZ_REQ_USERIN is sent yet)"` (bu plan metni değiştirir).
  - `BotCore/Perception.h` (F4-12): `ObsTable` (`Find`, `Retain`, `Upsert`), `kObsMaxUnits = 64`. `BotCore/BotCombat.h:345-370` `RegionIndex`/`RegionDelta` (bu planda kullanılmaz). `Tests/BotCoreTests/PerceptionTests.cpp` (yedi test, stil), `Tests/BotCoreTests/MiniTest.h`.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/Perception.h`, yalnızca ekleme):** `PendingIds` (bekleyen kimlik listesi), sabitler `kObsPendingMax`, `kUserInMaxIds`, `kUserInMinGapMs`, guard `CheckUserIn`. Birim testleri (`PerceptionTests.cpp`'ye üç `TEST_CASE`).
2. **Oturum durumu (`BotSession`):** `m_obsPending` (`m_obsLock` altında), `m_userInEcho` (atomik), IOCP-yalnızca sayaç/zamanlayıcı alanları ve iki kilitli yardımcı üye fonksiyon (`PeekUserInBatch`, `DropUserInBatch`). `OnPacket()`'e **yalnızca ekleme** (yeni satırlar mevcut `WIZ_REGIONCHANGE`/`WIZ_REQ_USERIN` dallarının içinde; başka blok değişmez).
3. **Aksiyon (`ActionExecutor::TickUserIn`):** `Tick()` başına bir kez, oyundaki canlı bot için; ≤ 32 kimlik, tek `WIZ_REQ_USERIN`, `HandlePacket()` ile; sonuç yalnızca cevabın `OnPacket()`'e bıraktığı yankıdan.
4. **Çağrı (`BotManager::TickSessions`) ve `see` metni:** `TickUserIn` çağrısı (bir blok) + sonuç günlüğü; `see`'nin açıklama satırı ve sayaçlar.
5. **Dokümantasyon ve kayıtlar Claude'un işi** (DeepSeek dokunmaz): `docs/03` CLI-19, ADR-0017 Eki F4-13, STATUS, README.

**Kapsam dışı (yapılmayacak)**

- **Yeni komut yok.** `/bot userin` gibi bir test sürücüsü eklenmez; istek tamamen otomatiktir (istemci gibi). Doğrulama `/bot see` ile yapılır.
- Bot **kendi kararıyla keyfi kimlik istemez**: yalnızca son `WIZ_REGIONCHANGE` listesinden gelen ve tabloda bulunmayan kimlikler. `GetUserPtr`, bölge dizileri, harita **okunmaz**.
- CLI-11 (aksiyon hızı) penceresine **sayılmaz** ve ona bakılmaz: bu bir oyuncu aksiyonu değil, istemcinin otomatik ağ trafiğidir (bkz. §5.2 gerekçe). Telemetri `decisions` seviyesinde `ACTION_SUBMIT`/`ACTION_RESULT` (`"type":"UserInReq"`) yazar.
- NPC/canavar (`WIZ_REQ_NPCIN`, `WIZ_NPC_INOUT`), pazarcılar: yok. `lastSeen` bayatlama temizliği, HP/MP gözlemi, `PerceptionSnapshot`, tabloyu kullanan karar/guard: yok. `ActionExecutor`'ın mevcut aksiyonları, `IsSamePartyMember`, `Telemetry.*`, `ScenarioRunner.*`, `ChatHandler.cpp`: **değişmez**.
- Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez.
- İstek sırasında `m_obsLock` tutulurken `HandlePacket()` çağrılmaz (kilit aynı thread'de yeniden alınırsa kilitlenir: `HandlePacket` → `OnPacket()` → `m_obsLock`). Kilit yalnızca `PeekUserInBatch`/`DropUserInBatch`'in içindedir.
- Dokümanları (`docs/03`, `docs/13`, `docs/14`, `docs/16`, `docs/KNOWN_ISSUES.md`) güncellemek: Claude'un işi.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | yalnızca ekleme: §5.2 (dosya sonunda, `ObsTable`'dan sonra, `namespace BotCore` içinde) |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | yalnızca ekleme: üç `TEST_CASE` (§5.2 sonu) |
| `GameServer/Bot/BotSession.h` | değiştir | §5.3: alanlar + iki üye fonksiyon bildirimi + `m_obsUnresolved` yorumu |
| `GameServer/Bot/BotSession.cpp` | değiştir | başlatıcı listesi, `ResetForRespawn()`, `OnPacket()` iki dalına ekleme, iki yardımcı üye fonksiyon |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `UserInOutcome` + `TickUserIn` bildirimi (§5.4) |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | `TickUserIn` (dosya sonuna ekleme) |
| `GameServer/Bot/BotManager.cpp` | değiştir | `TickSessions()` çağrısı + `CommandSee` metin/sayaç değişikliği |

(Dokunulacak dosya sayısı 7; plan dosyası dahil 8.) `BotCore.vcxproj`, `BotCoreTests.vcxproj`, `proj-GameServer.vcxproj*`, `BotManager.h` **değişmez** (yeni dosya ve yeni komut yok). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-13 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/Perception.h` ve birim testleri

Biçim: dosyadaki mevcut kod gibi (tab, Allman, `inline`, İngilizce kısa yorum, `namespace BotCore` içinde, ASCII + CRLF). Yeni `#include` gerekmez. `std::min/max`, `std::vector`, `std::string`, `new`, `malloc` yok. Yorumlarda `shared/` ve `GameServer` dizgisi **yazma** (F4-12 K4 grep'i takılır).

**Sabitler:**

```cpp
constexpr int      kObsPendingMax  = 128;   // ids kept from one WIZ_REGIONCHANGE list (design limit; the table holds 64)
constexpr int      kUserInMaxIds   = 32;    // ids per WIZ_REQ_USERIN request (CLI-19, design limit) [A]
constexpr uint32_t kUserInMinGapMs = 1000;  // min time between two requests (CLI-19, design limit) [A]
```

**`PendingIds`** (kopyalanabilir, mutex içermez; kilidi çağıran tutar; ekleme sırası korunur):

```cpp
class PendingIds
{
public:
	PendingIds() { Clear(); }
	void Clear();                                   // count 0
	int Count() const;
	// Replaces the content with the ids of 'ids' that 'obs' does not know yet. Repeats are skipped; at most kObsPendingMax kept.
	void Set(const uint16_t * ids, int n, const ObsTable & obs);
	// Drops the ids that 'obs' knows by now and 'selfSid'; copies up to 'cap' of the rest to 'out' in order WITHOUT removing them.
	// Returns how many were copied.
	int Peek(const ObsTable & obs, uint16_t selfSid, uint16_t * out, int cap);
	// Removes the listed ids (order of the others is kept).
	void Remove(const uint16_t * ids, int n);
private:
	uint16_t m_ids[kObsPendingMax];
	int m_count;
};
```

**Guard (CLI-19):**

```cpp
struct UserInCheck
{
	int count;               // ids the request would carry
	bool hasLast;            // a request was sent earlier in this spawn
	uint32_t sinceLastMs;    // since that request
};

enum UserInVerdict
{
	USERIN_OK = 0,
	USERIN_REJECT_COUNT = 1,   // CLI-19: count < 1 or > kUserInMaxIds (defensive; the caller already clamps)
	USERIN_REJECT_GAP = 2      // CLI-19: previous request < kUserInMinGapMs ago
};

// Order: count, gap. Not rate limited by CLI-11 (automatic client traffic, see ADR-0017 Ek F4-13).
inline UserInVerdict CheckUserIn(const UserInCheck & c);
```

**Birim testleri** (`PerceptionTests.cpp`'ye, dosyanın sonuna; mevcut testlere dokunulmaz; `MotionTests.cpp`/`PerceptionTests.cpp` stili; dosya ASCII + CRLF). Yardımcı: dosya-yerel `static BotCore::UnitObs MakeUnit(uint16_t sid)` (tüm alanlar sıfır, `sid` verilen) — dosyada zaten benzer yardımcı varsa onu kullan, yeni ad uydurma.

- `Perception_PendingIds_Set`: boş `ObsTable` ile `Set({5, 7, 5, 9}, 4)` → `Count() == 3` (tekrar atlandı); tabloda 7 varken `Set({5, 7, 9}, 3)` → `Count() == 2` ve içerik `{5, 9}` (`Peek` ile okunur); `Set` önceki içeriği **değiştirir** (ikinci çağrı sonrası eski kimlikler yok); 200 farklı kimlikle `Set` → `Count() == BotCore::kObsPendingMax`; `n == 0` → `Count() == 0`; `Clear()` → 0.
- `Perception_PendingIds_PeekRemove`: `Set({1,2,3,4,5})`; tabloya 2'yi `Upsert` et; `Peek(obs, self=4, out, cap=10)` → 3 ve `out == {1, 3, 5}` (2 tabloda, 4 kendisi); `Count()` budanmış hâli verir (3); `Peek(..., cap=2)` → 2 ve `{1, 3}` (**kaldırmaz**: ardından `Count() == 3`); `Remove({3})` → `Count() == 2`, kalan sıra `{1, 5}`; bilinmeyen kimlik `Remove` zararsız; `Remove` ile hepsi → 0, `Peek` → 0.
- `Perception_CheckUserIn`: `hasLast = false` → OK; `count = 0` ve `count = 33` → `USERIN_REJECT_COUNT` (`hasLast = false` olsa bile); `count = 32` → OK; `hasLast = true, sinceLastMs = 999` → `USERIN_REJECT_GAP`; `sinceLastMs = 1000` → OK; sıra: hem `count = 0` hem `sinceLastMs = 0` iken `USERIN_REJECT_COUNT`; sabitler: `kUserInMaxIds == 32`, `kUserInMinGapMs == 1000`, `kObsPendingMax == 128`.

Beklenen toplam test sayısı: 52 + 3 = **55**.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h` (aynı yorum/hizalama biçimi):

- `m_obs`'un altına (kilit korumalı, **aynı grup**): `BotCore::PendingIds m_obsPending;   // guarded by m_obsLock: ids of the last WIZ_REGIONCHANGE the table did not know (Perception, ADR-0017 Ek F4-13)`.
- IOCP-yalnızca alanlar (`m_chatWindow`'dan sonra, `m_obsLock`'tan önce): `bool m_userInHasLast;` `std::chrono::steady_clock::time_point m_userInLast;` `uint32 m_userInRequests;` `uint32 m_userInUnits;` (her biri `// IOCP thread only: ...` yorumlu: son istek zamanı geçerli mi / ne zaman / bu spawn'daki istek sayısı / cevaplarla gelen birim sayısı toplamı).
- Atomik (`m_obsUnresolved`'ın altına): `std::atomic<uint64> m_userInEcho;   // written by OnPacket(): valid bit (63) | number of units parsed from the last WIZ_REQ_USERIN reply`.
- `m_obsUnresolved` yorumunu güncelle (`(no WIZ_REQ_USERIN is sent yet)` ifadesini kaldır; "ids of the last WIZ_REGIONCHANGE that were not in m_obs, the bot itself included" bırak).
- Üye fonksiyonlar (genel bölüm, `ResetForRespawn()`'ın altına; ikisi de **IOCP thread**; içeride `m_obsLock` alır):
  ```cpp
  // IOCP thread only. Copies up to 'cap' ids the table still does not know (selfSid and ids learned meanwhile are dropped) without removing them.
  int PeekUserInBatch(uint16 selfSid, uint16 * out, int cap);
  // IOCP thread only. Removes the ids from the pending list (call after the request went out).
  void DropUserInBatch(const uint16 * ids, int n);
  ```

`BotSession.cpp`:

- Başlatıcı listesi: `m_obsUnresolved(0)`'a `m_userInHasLast(false), m_userInRequests(0), m_userInUnits(0), m_userInEcho(0)` ekle (üye bildirim sırasına uy; derleyici sıra uyarısı vermemeli). `m_obsPending`/`m_userInLast` varsayılan kurucu.
- `ResetForRespawn()`: mevcut `{ lock_guard ... m_obs.Clear(); }` bloğuna `m_obsPending.Clear();` ekle; blok dışına `m_userInHasLast = false; m_userInRequests = 0; m_userInUnits = 0; m_userInEcho = 0;`.
- `OnPacket()` (F4-12 algı bloğu), **yalnızca iki mevcut dala ekleme**:
  - `WIZ_REGIONCHANGE` dalı: mevcut `m_obsUnresolved = ...Retain(...)` satırının hemen altına (aynı kilit altında): `m_obsPending.Set(ids, n, m_obs);`. (`Retain` önce çalışır: düşenler tablodan çıkar, sonra bilinmeyenler bekleyen listeye girer. `Set`'e `n` zaten `ParseRegionList`'in döndürdüğü sayıdır.)
  - `WIZ_REQ_USERIN` dalı: `for` döngüsü ve kilit bloğu bittikten sonra (kilit kapsamı içinde veya dışında fark etmez) `m_userInEcho = (1ull << 63) | (uint64)n;` — **tablo işlemi bittikten sonra** yazılır (yürütücü yankıyı okuduğunda tablo güncel olsun).
- Yardımcılar (dosya sonuna): `PeekUserInBatch` → `std::lock_guard<std::mutex> lock(m_obsLock); return m_obsPending.Peek(m_obs, (uint16_t)selfSid, out, cap);`; `DropUserInBatch` → kilit altında `m_obsPending.Remove(ids, n);`. İçlerinde günlük/yeni nesne yok.
- Mevcut hiçbir `OnPacket()` bloğunda satır silinmez/değişmez (yalnızca başlatıcı listesindeki `m_obsUnresolved(0)` satırı bilinçli değişir).

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

`ActionExecutor.h`: `ChatOutcome`'dan sonra:

```cpp
// Result of ActionExecutor::TickUserIn (ADR-0017 Ek F4-13).
struct UserInOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed. SENT: "received" (the reply arrived). FAILED: "no_result" (no reply).
	                       // REFUSED: "bad_count" (guard CLI-19; FAIRNESS_REJECT written). NOTHING: "ok".
	int requested;         // ids in the request (0 when NOTHING)
	int received;          // units the reply carried; valid only when kind == SENT
};
```

ve sınıfın **sonuna** (son üye):

```cpp
	// Called once per Tick() for every in-game, living session. Sends one WIZ_REQ_USERIN through CUser::HandlePacket()
	// for the ids the last WIZ_REGIONCHANGE listed and the observation table does not know (at most kUserInMaxIds; the
	// bot's own id is never requested) when the CLI-19 guard allows it (>= 1 s since the previous request). NOTHING when
	// there is nothing to ask or the gap has not passed (the ids stay pending). Result only from the reply the server
	// published (m_userInEcho). Not counted in the CLI-11 window (automatic client traffic).
	static UserInOutcome TickUserIn(BotSession * s, std::chrono::steady_clock::time_point now);
```

`ActionExecutor.cpp` (dosya sonuna; mevcut fonksiyonlara dokunulmaz; yeni `#include` gerekmez). Mantık (`RequestTargetHp`/`RequestChatParty` kalıbı):

1. `out = {NOTHING, "ok", 0, 0}`. `user = s != nullptr ? s->m_pUser : nullptr`; `user == nullptr || !user->isInGame() || user->isDead()` → `NOTHING` döndür (olay yok).
2. `uint16 batch[BotCore::kUserInMaxIds]; int n = s->PeekUserInBatch((uint16)user->GetSocketID(), batch, BotCore::kUserInMaxIds);` `n == 0` → `NOTHING`.
3. `BotCore::UserInCheck c;` `c.count = n; c.hasLast = s->m_userInHasLast; c.sinceLastMs = hasLast ? ms(now - s->m_userInLast) : 0;` `verdict = BotCore::CheckUserIn(c);`
   - `USERIN_REJECT_GAP`: **olay yazma, kimlikleri düşürme**; `NOTHING` döndür (zamanlama beklemesi, ihlal değil; her tick'te FAIRNESS_REJECT spamı olmasın).
   - `USERIN_REJECT_COUNT`: `EmitFairnessReject(s, user, NextDecisionId(s), "UserInReq", "CLI-19", "bad_count", (float)n, (float)BotCore::kUserInMaxIds)`; `s->DropUserInBatch(batch, n)` (aynı kimlikler sonsuza dek takılmasın); `REFUSED`/`"bad_count"`, `requested = n`.
4. `decisionId = NextDecisionId(s)`; `decisions` açıksa `ACTION_SUBMIT`: `"decision_id", "type":"UserInReq", "count":n`.
5. Paket: `Packet pkt(WIZ_REQ_USERIN); pkt << uint16(n);` ardından her kimlik için `pkt << uint16(batch[i]);` (sunucu `u16 sayı` + `u16 sid` okur: `User.cpp:1233-1243`). `s->m_userInEcho = 0;` → `HandlePacket(pkt)` (süreyi `steady_clock` ile mikrosaniye ölç; **kilit tutulmadan**).
6. Hemen ardından (sonuç ne olursa): `s->DropUserInBatch(batch, n); s->m_userInHasLast = true; s->m_userInLast = now; s->m_userInRequests++;`. `m_actionWindow.Record(...)` **çağrılmaz** (CLI-11'e sayılmaz).
7. Sonuç: `uint64 e = s->m_userInEcho.load(); bool ok = (e & (1ull << 63)) != 0; int received = ok ? (int)(e & 0xFFFF) : 0;` `ok` ise `s->m_userInUnits += received`.
8. `decisions` açıksa `ACTION_RESULT`: `"decision_id", "type":"UserInReq", "ok", "reason":"received"|"no_result", "latency_us", "count":n, "received":received`.
9. `ok` → `SENT`/`"received"`; değilse `FAILED`/`"no_result"`; `requested = n`, `received` doldur.

Başlık yorumundaki "No logging, no locking" cümlesini aynen bırak (kilit `BotSession` üye fonksiyonlarının içindedir, yürütücüde yok).

### 5.5 `GameServer/Bot/BotManager.cpp`

1. **`TickSessions()`** canlı bot dalında (`isDead()` değilken çalışan `else` bloğu; `TickMove` sonuç günlüğünden **hemen sonra**, aynı `else` bloğu içinde, `m_attackActive` bloğundan önce):

   ```cpp
   UserInOutcome userIn = ActionExecutor::TickUserIn(s, now);
   if (userIn.kind == UserInOutcome::SENT) { ... }
   ```
   Günlük (`WriteBotLog`): `SENT` → `BotManager: bot %s userin requested %d, received %d`; `REFUSED`/`FAILED` → `BotManager: bot %s userin failed (%s)`; `NOTHING` → günlük yok. (Bölge değişimi başına en fazla birkaç satır; sık yazılmaz.) `BeginDespawn()` ile çıkılmış oturumda `TickUserIn` zaten `NOTHING` döner (`isInGame()` kontrolü); ek dal gerekmez.
2. **`CommandSee`:** kilit bloğuna (`copy = s->m_obs;` yanına, aynı kilit altında) `uint32 pending = (uint32)s->m_obsPending.Count();` ekle (kilidi bırakınca değişken kullanılır; biçimleme kilit dışında). Açıklama satırını (`BotManager.cpp:2297`) şu biçime değiştir (kopyala-yapıştır değil, `snprintf` ile, `char note[256]`): `BotManager: cmd see:   (unresolved counts the last region id list incl. the bot itself; userin requests %u, units received %u, pending %u)` — `s->m_userInRequests`, `s->m_userInUnits`, `pending`. Başlık satırı ve birim satırları **değişmez**.
3. `BuildStatusLines()`, `BeginDespawn()`, `ExecuteCommand()`, `Tick()`, `Startup()`: **değişmez**.

### 5.6 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter; `Perception.h`, `PerceptionTests.cpp`, `BotSession.cpp`, `ActionExecutor.cpp`, `BotManager.cpp` için uyarı çıktısı boş.
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı üç yeni test adını (`Perception_PendingIds_Set`, `Perception_PendingIds_PeekRemove`, `Perception_CheckUserIn`) içerir ve toplam test sayısı **55**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h` boş; `#include` satırları hâlâ yalnızca `<cstddef>`, `<cstdint>`, `<cstring>`; `grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h` boş.
- [ ] K5: **sözleşme dışı erişim yok (statik AC-LRN-03 denetimi):** `grep -nE "GetUserPtr|m_RegionUserArray|GetRegion\(|GetMap\(|GetRegionX|GetRegionZ|g_pMain|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMaxHealth|GetMana|GetMaxMana|m_buffMap|m_CoolDownList" GameServer/Bot/BotSession.cpp` boş; `grep -n "TickUserIn" -A75 GameServer/Bot/ActionExecutor.cpp | grep -E "GetUserPtr|m_RegionUserArray|GetRegion\(|GetMap\(|g_pMain|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMana"` boş (yalnızca `TickUserIn` gövdesi); `TickUserIn` botun kendi `CUser`'ından yalnızca `isInGame()`, `isDead()`, `GetSocketID()` okur.
- [ ] K6: kimlikler yalnızca `WIZ_REGIONCHANGE` listesinden gelir: `grep -n "m_obsPending" GameServer/Bot/*.cpp` yalnızca `BotSession.cpp`'de (`OnPacket()` REGIONCHANGE dalı, `ResetForRespawn()`, `PeekUserInBatch`, `DropUserInBatch`) ve `BotManager.cpp`'de (`CommandSee` kilit bloğundaki tek `Count()` satırı) geçer; `ActionExecutor.cpp` `m_obsPending`'e **dokunmaz** (yalnızca `PeekUserInBatch`/`DropUserInBatch`); `Packet pkt(WIZ_REQ_USERIN)` `GameServer/Bot/` içinde **yalnızca** `TickUserIn`'de geçer (`grep -n "WIZ_REQ_USERIN" GameServer/Bot/*.cpp`: `BotSession.cpp` algı bloğu ve `TickUserIn` satırları).
- [ ] K7: kilit disiplini: `HandlePacket(` çağrısı `m_obsLock` tutulurken yapılmaz (`grep -n "m_obsLock" GameServer/Bot/ActionExecutor.cpp` **boş**; `BotSession.cpp`'de `lock_guard` yalnızca `OnPacket()` dalları, `ResetForRespawn()` ve iki yardımcıda); `CommandSee` kilit bloğu yalnızca kopyalama satırlarını (`copy`, `unresolved`, `pending`) kapsar.
- [ ] K8: mevcut kod yalnızca ekleme: `git diff gece/2026-10-02...bot/F4-13 -- GameServer/Bot/BotSession.cpp | grep '^-' | grep -v '^---'` yalnızca başlatıcı listesindeki bilinçli değişen satırı gösterir; `ActionExecutor.cpp`/`ActionExecutor.h` farkında **silinen satır yok**; `BotManager.cpp` farkında `-` satırları yalnızca `CommandSee` açıklama satırı (eski `WriteBotLog("... no WIZ_REQ_USERIN is sent yet)")`) ve gerekiyorsa kilit bloğuna ek.
- [ ] K9: CLI-11'e sayılmaz: `grep -n "TickUserIn" -A75 GameServer/Bot/ActionExecutor.cpp | grep "m_actionWindow"` boş; `PerceptionTests`'te `Perception_CheckUserIn` `actionsInWindow` alanı içermez (guard CLI-11'e bakmaz).
- [ ] K10: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca bot oturumları için çalışır; `Startup()`/`Tick()`/`BuildStatusLines()`/`BeginDespawn()`/ini okuma değişmedi; `GameServer/` içinde `Bot/` dışında dosya değişmemiş; yeni ini anahtarı yok.
- [ ] K11: `git diff --stat gece/2026-10-02...bot/F4-13` yalnızca §4'teki 7 dosyayı (ve plan dosyasını) gösterir; `*.vcxproj*` farkı boş.
- [ ] K12: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `git diff --check` boş.
- [ ] K13: `GameServer/Bot/` içinde yeni `printf`, `Sleep`, `CreateThread`, `rand(` yok; `std::mutex` kullanımı yalnızca mevcut `m_obsLock` (`grep -n "mutex" GameServer/Bot/BotSession.*` yeni satır eklememiş: yalnızca `#include <mutex>`, `m_obsLock` bildirimi ve `lock_guard` satırları); `Telemetry.*`, `ScenarioRunner.*`, `BotManager.h` değişmemiş.
- [ ] K14: F4-01..F4-12 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `CheckAttack`, `CheckCastStart`, `CheckPotion`, `CheckStance`, `CheckTargetHp`, `CheckRegene`, `CheckPartyInvite`, `CheckPartyAccept`, `CheckPartyDecline`, `CheckPartyLeave`, `CheckPartyManage`, `CheckChat` her biri ≥ 1; önceki 52 testin tamamı hâlâ geçiyor.
- [ ] K15 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–5 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-13
git diff gece/2026-10-02...bot/F4-13 -- GameServer/Bot/BotSession.cpp GameServer/Bot/BotManager.cpp GameServer/Bot/ActionExecutor.cpp GameServer/Bot/ActionExecutor.h | grep '^-' | grep -v '^---'
git diff gece/2026-10-02...bot/F4-13 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/Perception.h
grep -n "std::min\|std::max\|new \|malloc\|std::vector\|std::string" BotCore/Perception.h
grep -nE "GetUserPtr|m_RegionUserArray|GetRegion\(|GetMap\(|GetRegionX|GetRegionZ|g_pMain|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMaxHealth|GetMana|GetMaxMana|m_buffMap|m_CoolDownList" GameServer/Bot/BotSession.cpp
grep -n "TickUserIn" -A75 GameServer/Bot/ActionExecutor.cpp | grep -E "GetUserPtr|m_RegionUserArray|GetRegion\(|GetMap\(|g_pMain|m_sItemArray|GetItem\(|GetPartyID|m_sMp|m_sHp|GetHealth|GetMana"
grep -n "TickUserIn" -A75 GameServer/Bot/ActionExecutor.cpp | grep "m_actionWindow"
grep -n "m_obsPending" GameServer/Bot/*.cpp
grep -n "WIZ_REQ_USERIN" GameServer/Bot/*.cpp
grep -n "m_obsLock" GameServer/Bot/ActionExecutor.cpp
grep -n "mutex" GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp
grep -n "printf\|Sleep\|CreateThread\|rand(" GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h
file BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp GameServer/Bot/ActionExecutor.cpp
git diff --check gece/2026-10-02...bot/F4-13
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`). Botlar: Karus `BotWP_K`, `BotMF_K`; El Morad `BotWG_E`; zone 71. Gözlem `Logs/Bot_*.log` ve `Logs/bots/<tarih>/live-*.jsonl`. Senaryolar:

1. **Bölge değişimi, yürüyen bot başkasını görür:** `spawn BotWP_K,BotMF_K`; ~10 sn sonra `move BotMF_K 1380 893` (≈ 114 m, F4-12 doğrulamasındaki yol); `BotMF_K` vardıktan sonra `see BotWP_K` → `BotMF_K` **yok** (`INOUT_OUT`, uzaklık); sonra `move BotWP_K 1330 893` (birkaç bölge geçer; hedef `BotMF_K`'nın 3×3 bölgesine girecek kadar yakın, gerekirse hedefi ayarla) → `Logs/Bot_*.log`'da `BotWP_K userin requested 1, received 1` ve `see BotWP_K` listesinde `BotMF_K` (doğru `pos`, `class`, `ally`, `age` küçük); `see` açıklama satırında `userin requests` ≥ 1, `units received` ≥ 1, `pending 0`. Telemetri: `ACTION_SUBMIT`/`ACTION_RESULT` `"type":"UserInReq"`, `"count":1`, `"received":1`, `"ok":true`.
2. **Hız sınırı:** art arda iki bölge değişimi (< 1 sn arayla, ör. kısa adımlarla bölge sınırını gidip gelmek) → ikinci istek en az 1 sn sonra gider (`UserInReq` `ACTION_SUBMIT` zaman damgaları farkı ≥ 1000 ms); `FAIRNESS_REJECT` yazılmaz (gap beklemesi sessizdir).
3. **Spawn'da gereksiz istek yok:** `spawn BotWP_K`, ~14 sn sonra `spawn BotMF_K` → ikinci botun girişinde `UserInReq` **yazılmaz** (spawn'daki `WIZ_REQ_USERIN`/`WIZ_REGIONCHANGE` tabloyu doldurdu, bekleyen liste boş; gerekirse `userin requests 0`); `see` hâlâ doğru.
4. **Ölü/çıkmış bot:** ölü bot istek yollamaz (`attack` ile `BotMF_K`'yı öldür, `move`/yeniden doğma olmadan `see`'de `userin requests` artmaz); `despawn all` temiz, slot sızıntısı yok, sunucu çökmedi.
5. **Gerilemesiz:** F4-01..F4-12 komutları çalışır (`move`/`attack`/`cast`/`pot`/`sit`/`target`/`regene`/`pinvite`/`paccept`/`pchat`/`see`); `tick_p95_us` ≤ 500; `ENABLED=0` → komut dosyası tüketilmez/log yok. İnsan istemcisi gerekmez; insan ölçümü T-PERC-01 (`docs/STATUS.md`) ayrıca bekler.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `Perception.h`, `PerceptionTests.cpp`, `BotSession.*`, `ActionExecutor.*`, `BotManager.cpp` ASCII + CRLF; yeni dosya yok.
- **Thread kuralı (`docs/13` §3):** `m_obsPending` yalnızca `m_obsLock` altında; `OnPacket()` her thread'den çağrılabilir. **`HandlePacket()` çağrısı sırasında hiçbir kilit tutulmaz** (cevap aynı thread'de `OnPacket()` içinde `m_obsLock` ister: kilit tutulursa kilitlenir). `PeekUserInBatch`/`DropUserInBatch` kısa kilit, kopyalama dışında iş yok.
- `OnPacket()` içinde yeni nesne/dosya/günlük yok; `Set` en çok 128 `uint16` işler.
- **Yankı sırası:** `m_userInEcho` ancak tablo `Upsert`'leri bittikten sonra yazılır; yürütücü `HandlePacket()` döndükten sonra okur (eşzamanlı yayın, `User.cpp:28-39`).
- **Sorunsuz yoksayma:** bozuk/boş cevap (`ParseUserList` 0 döndürür) yankıyı yine de `received = 0` ile yazar; `FAILED "no_result"` yalnızca hiç cevap gelmediyse. Hiçbir durumda uydurma birim eklenmez.
- Bu planın riski: sunucu `RequestUserIn` bilinmeyen/oyunda olmayan kimlikleri sessizce atlar; o kimlikler listede tekrar istenmez (`DropUserInBatch` isteğin hemen ardından çalışır); yeni `WIZ_REGIONCHANGE` yeni liste kurar. Kayıt biçimi F4-12'de çalışma zamanında doğrulandı (bot-bot); klanlı gerçek oyuncu T-ARCH-17'de.
- Beklenmedik bir şey görürsen **uydurma**: Uygulayıcı Raporu'na yaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-13` — `989999e [F4-13] Perception dilim 2: bolge degisiminde WIZ_REQ_USERIN (TickUserIn, CLI-19)` (+ bu rapor/`Durum` commit'i)
- Değişen dosyalar ve neden:
  - `BotCore/Perception.h`: `PendingIds` (bekleyen kimlik listesi), `kObsPendingMax`/`kUserInMaxIds`/`kUserInMinGapMs`, `UserInCheck`/`UserInVerdict`/`CheckUserIn` (CLI-19) eklendi (dosya sonuna, `ObsTable`'dan sonra; yeni include yok).
  - `Tests/BotCoreTests/PerceptionTests.cpp`: dosya-yerel `MakeUnit` yardımcısı + üç `TEST_CASE` (`Perception_PendingIds_Set`, `Perception_PendingIds_PeekRemove`, `Perception_CheckUserIn`).
  - `GameServer/Bot/BotSession.h`: `m_obsPending` (kilit altında), `m_userInHasLast`/`m_userInLast`/`m_userInRequests`/`m_userInUnits` (IOCP), `m_userInEcho` (atomik); `PeekUserInBatch`/`DropUserInBatch` bildirimleri; `m_obsUnresolved` yorumu güncellendi.
  - `GameServer/Bot/BotSession.cpp`: başlatıcı listesi, `ResetForRespawn()`, `OnPacket()` `WIZ_REGIONCHANGE`/`WIZ_REQ_USERIN` dallarına ekleme, iki yardımcı fonksiyon.
  - `GameServer/Bot/ActionExecutor.h`: `UserInOutcome` + `TickUserIn` bildirimi.
  - `GameServer/Bot/ActionExecutor.cpp`: `TickUserIn` (dosya sonuna; guard CLI-19, tek `WIZ_REQ_USERIN`, sonuç `m_userInEcho` yankısından, CLI-11'e sayılmaz).
  - `GameServer/Bot/BotManager.cpp`: `TickSessions()` canlı dalında `TickUserIn` çağrısı + günlük; `CommandSee` bekleyen sayaç ve açıklama satırı.
- Derleme sonucu (`tools/build.sh Release` son satır): `proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe`. Değişen beş dosyada uyarı yok; yalnızca eski `GameServerDlg.cpp` (C4834, 2× C4267) ve `UpgradeHandler.cpp` (2× C4789) uyarıları var. `Debug` de rc=0.
- Test sonucu (`tools/run-tests.sh Release` / `Debug`): her ikisinde `55 tests, 0 failed`; üç yeni test adı çıktıda.
- Kabul kriterleri öz-değerlendirme: K1 ✔ K2 ✔ K3 ✔ K4 ✔ K5 ✔ K6 ✔ K7 ✔ K8 ✔ K9 ✔ K10 ✔ K11 ✔ K12 ✔ K13 ✔ K14 ✔ (K15 Claude).
- Plandan sapmalar: yok. `PendingIds::Peek` plandaki "bilinen ve kendini düşür" cümlesine ve `Perception_PendingIds_PeekRemove` beklentisine göre bilinen/kendini listeden kalıcı düşürür, kopyalanan uygun kimlikleri bırakır (`cap` uygulanırken silmez); `Count()` bu budanmış hâli verir.
- Açık sorular: yok.


---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-13` @ `a667734` (kod commit'i `989999e`; taban `6960129`). Gece modu (`AUTO_LOOP=1`): birleştirme/push döngü betiğinde.
- Kriter sonuçları (15/15 ✔):

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `./tools/build.sh Release` rc=0; beş dosya `touch` ile yeniden derlendi (`ActionExecutor.cpp`, `PerceptionTests.cpp`, `BotManager.cpp`, `BotSession.cpp` derleme satırları log'da), log'da `warning`/`error` 0 |
| K2 | ✔ | `./tools/build.sh Debug` rc=0; `Perception`/`BotSession`/`ActionExecutor`/`BotManager` için uyarı yok |
| K3 | ✔ | `run-tests.sh Release` ve `Debug`: `55 tests, 0 failed`; `Perception_PendingIds_Set`, `Perception_PendingIds_PeekRemove`, `Perception_CheckUserIn` çıktıda `[ OK ]`; test gövdeleri plandaki beklentilerle birebir (tekrar atlama, 200 → 128, `Peek` kaldırmaz, `Remove` sırayı korur, 999/1000 ms sınırı, sıra count→gap) |
| K4 | ✔ | yasak dizge grep'i boş; `#include` yalnızca `Perception.h:8-10`; `std::min/max/new/malloc/vector/string` grep'i boş |
| K5 | ✔ | iki sözleşme grep'i boş (`BotSession.cpp` ve `TickUserIn` gövdesi); `TickUserIn` botun `CUser`'ından yalnızca `isInGame()`, `isDead()`, `GetSocketID()` okur (`ActionExecutor.cpp:2670-2676`) |
| K6 | ✔ | `m_obsPending` yalnızca `BotSession.cpp:210` (REGIONCHANGE dalı), `:291` (`ResetForRespawn`), `:311`/`:317` (iki yardımcı) ve `BotManager.cpp:2268` (`Count()`); `ActionExecutor.cpp`'de yok; `WIZ_REQ_USERIN` paketi yalnızca `ActionExecutor.cpp:2716` (`TickUserIn`), `BotSession.cpp` yalnızca algı bloğu (`:174`, `:195`) |
| K7 | ✔ | `grep m_obsLock ActionExecutor.cpp` boş; `HandlePacket()` (`ActionExecutor.cpp:2723`) kilitsiz; `BotSession.cpp`'de `lock_guard` yalnızca `OnPacket()` dalları, `ResetForRespawn()` ve iki yardımcıda; `CommandSee` kilit bloğu üç kopyalama satırı (`copy`, `unresolved`, `pending`) |
| K8 | ✔ | `-` satırları: `BotManager.cpp` tek açıklama satırı; `BotSession.cpp` yalnızca başlatıcı satırı; `BotSession.h` iki yorum satırı (`m_obsLock`, `m_obsUnresolved`, planın istediği güncelleme); `ActionExecutor.*` ve `Perception.h`/`PerceptionTests.cpp` farkında silinen satır yok |
| K9 | ✔ | `TickUserIn` gövdesinde `m_actionWindow` yok; `Perception_CheckUserIn`'de `actionsInWindow` yok |
| K10 | ✔ | `Startup()/Tick()/BuildStatusLines()/BeginDespawn()/ini` farkta yok; `GameServer/` içinde yalnızca `Bot/BotSession.*`, `ActionExecutor.*`, `BotManager.cpp`; yeni ini anahtarı yok |
| K11 | ✔ | `--stat` yalnızca §4'teki 7 dosya + plan; dört `vcxproj*` farkı 0 satır |
| K12 | ✔ | yedi kod dosyası ASCII + CRLF (`file`); `git diff --check` boş |
| K13 | ✔ | yeni `printf/Sleep/CreateThread/rand(` yok (yalnızca `snprintf`); `mutex` satırları yalnızca `#include`, `m_obsLock` bildirimi, `lock_guard`'lar (iki yeni `lock_guard` planlı yardımcılarda); `Telemetry.*`, `ScenarioRunner.*`, `BotManager.h` farkta yok |
| K14 | ✔ | `CheckMoveStep` 2, diğer 12 guard 1'er; önceki 52 test geçiyor (toplam 55) |
| K15 | ✔ | çalışma zamanı, aşağıda (senaryo 2 ve 4 kısmen, bkz. notlar) |

Çalışma zamanı (K15; `Release`, `ENABLED=1`, `MAX_BOTS=16`, `TELEMETRY=decisions`, `SPAWN_ON_START` boş, zone 71; ini hiç değiştirilmedi, md5 aynı `265a8e1c...`; kapanış `nazik`; eski `Logs/bots` `bots_old_f413`'e taşındı):

1. **Bölge değişimi:** `BotWP_K` (1380, 893) ve `BotMF_K` (1380, 1090) spawn'da birbirini görmüyor (`sees 0`, `userin requests 0`); `move BotWP_K 1380 1060` → günlükte `BotWP_K userin requested 1, received 1`; `see BotWP_K`: `sees 1 unit(s)`, `sid=2985 BotMF_K ally nation=1 class=110 lvl=80 pos=(1380.0, 1090.0) dist=30.0 alive`; açıklama satırı `userin requests 1, units received 1, pending 0`. Telemetri: `ACTION_SUBMIT`/`ACTION_RESULT` `"type":"UserInReq"`, `"count":1`, `"received":1`, `"ok":true`, `"reason":"received"`, `latency_us` 2. ✔
2. **Hız sınırı:** sınırda (x = 1439 / 1441, bölge 29|30) üç gidiş-dönüşle üç ek istek; `ACTION_SUBMIT` zaman damgaları 175209250 → 175214749 → 175220230 ms (aralık ≈ 5,5 sn ≥ 1000); `FAIRNESS_REJECT` sayısı 0. **`gap` beklemesi dalı çalışma zamanında tetiklenemedi:** hareket paketi aralığı 1,5 sn (CLI-05) ve komut dosyası 1 sn aralıkla okunduğu için art arda iki bölge değişimi < 1 sn'ye inmiyor; dal birim testi (`Perception_CheckUserIn`) ve kod okumasıyla (`ActionExecutor.cpp:2689-2691`: olay yok, kimlik düşürülmez) kabul. ✔ (kısmi)
3. **Spawn'da gereksiz istek yok:** `BotWP_K`, 16 sn sonra `BotMF_K` → ikisinde de `userin requests 0`, jsonl'de `UserInReq` yok. ✔
4. **Ölü/çıkmış bot:** `BotWG_E` hareket sırasında canavar/oyun tarafından öldü (`move stopped (dead)`, `hp=0/5650`); ölüyken 12 sn arayla iki `see`: `userin requests 2` sabit. `attack` ile `BotMF_K` öldürme denenmedi (ölü botta yeni bölge değişimi olmadığından `isDead()` kapısı yalnızca pasif sınandı; kapı `ActionExecutor.cpp:2676` kod okumasıyla). `despawn all` 3/3 temiz (`names cleared yes`, `pool free 16/16`), sunucu çökmedi. ✔ (kısmi)
5. **Gerileme:** `regene` (`respawned at (630.0, 920.0)`), `sit`/`stand`, `target`, `pinvite` (`created`), `pchat`, `move`, `pot` kullanım satırı, `see`; `PERF_SAMPLE` `tick_p95_us` en çok 273 (≤ 500), `skipped_ticks` 0; `FAIRNESS_REJECT` 0. `ENABLED=0` bu turda **yeniden denenmedi** (F4-12'de doğrulandı; bu diff `Startup/Tick` ve ini okumaya dokunmuyor, K10). ✔

- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. `BotCore/Perception.h` `PendingIds::Peek`: adı salt-okuma çağrıştırır ama bilinen ve kendi kimlikleri listeden **kalıcı olarak budar** (`Count()` budanmış hâli verir). Yorum ve test bunu belirtiyor, plan ile uyumlu; yalnızca adlandırma notu.
  2. `BotSession.cpp:200`: `m_userInEcho` her `WIZ_REQ_USERIN` cevabında (spawn'daki `UserInOutForMe` dahil) yazılır; `TickUserIn` isteğin hemen öncesinde `0`'a sıfırladığı ve cevap eşzamanlı geldiği için yanlış eşleşme yok. Birden çok kaynaktan cevap gelirse yankı ayrımı gerekir (şimdilik gerekmiyor).
  3. Çalışma zamanı kapsamı: `gap` dalı ve `isDead()` kapısı gerçek trafikle zorlanamadı (yukarıda, 2 ve 4); ileride betikli test dizilerinde (aynı tick'te iki bölge değişimi üreten senaryo) sınanabilir.
  4. Davranış notu: `Peek` sonrası en çok 32 kimlik gider, kalanlar listede kalıp ≥ 1 sn sonra istenir (tablo 64 birimle sınırlı; bu planın tasarımı).
- Düzeltme talimatı: yok (karar `DOĞRULANDI`).

