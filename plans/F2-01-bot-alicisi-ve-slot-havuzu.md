# F2-01: Bot alıcısı (`m_botSink`) ve ayrılmış oturum slot havuzu (S1 + S2, varsayılan kapalı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F2 — Bot oturumu (`docs/17` §2) |
| Branch | `bot/F2-01` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F1 fazının DeepSeek işleri bitti (F1-10 `KAPANDI`, `gece/2026-10-02`'ye birleşti) |
| İlgili gereksinim / kabul | ADR-0001 (K-1); `docs/02` §11 S1, S2; `docs/13` §4.1–4.2; AC-ARCH-04 (gerçek bağlantı bot slotu almaz), AC-ARCH-02 (bot kapalıyken davranış aynı) |
| Tahmini büyüklük | S–M (3 yeni dosya + 6 değişen dosya; küçük ekler) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

GameServer'a, botların ileride oturum slotu olarak kullanacağı iki temel parçayı eklemek, **hiçbir bot davranışı olmadan**:

1. **S1:** `KOSocketMgr` içinde, en yüksek kimlikli `N` oturumu boş havuzdan (`m_idleSessions`) ayırıp **bot havuzuna** koymak; bu kimlikler gerçek bağlantılara asla verilmez. `GameServer.ini`'deki `[BOT] ENABLED` (varsayılan `0`) ve `MAX_BOTS` (varsayılan `16`) anahtarlarıyla yönetilir.
2. **S2:** `CUser`'a `IBotSink * m_botSink` alanı ve `Send`/`SendCompressed` geçersiz kılmaları: `m_botSink != nullptr` ise paket soket yerine alıcıya verilir; gerçek oyuncu oturumlarında alan her zaman `nullptr` olduğundan davranış **bit düzeyinde aynıdır**.

Bot spawn/despawn, giriş akışı, tick ve karar yok; onlar F2-02 ve sonrası. Bu plan sonunda `[BOT] ENABLED=0` iken sunucu eskisi gibi çalışır; `ENABLED=1` iken açılışta havuz ayrılır, bir kez kendini sınar ve tek satır durum yazar.

## 2. Bağlam (okunması zorunlu)

- `docs/13` §4.1–4.2 ve `docs/02` §11 (S1, S2 satırları): tasarım ve gerekçe ("alt sınıf yerine alan + sanal geçersiz kılma"). `docs/13` §3.3: oturum haritası değişiklikleri `KOSocketMgr::GetLock()` altında yapılır.
- `AGENTS.md` §2 kural 6 (thread), §3 (bot sistemi varsayılan kapalı; kodlama).
- İlgili kod (dosya:satır, 2026-10-02'de depoda doğrulandı):
  - `shared/KOSocketMgr.h:67-68` `protected: SessionMap m_idleSessions, m_activeSessions; std::recursive_mutex m_lock;`; `:8` `typedef std::map<uint16, KOSocket *> SessionMap;`; `:48-50` `GetIdleSessionMap()`, `GetActiveSessionMap()`, `GetLock()`.
  - `shared/KOSocketMgr.h:74-80` `InitSessions` (`new T(i, this)`, kimlikler `0..N-1`); `:104-119` `AssignSocket` (idle haritanın **ilk** = en düşük kimlikli oturumunu aktife taşır); `:121-131` `OnConnect`; `:133-143` `DisconnectCallback` (aktif → **idle**).
  - `shared/KOSocketMgr.h:82-102` `Listen(...)` `InitSessions(sTotalSessions)` çağırır; yani `Listen` döndüğünde 3000 oturum idle haritadadır.
  - `shared/KOSocket.h:33-34` `virtual bool Send(Packet * pkt);` `virtual bool SendCompressed(Packet * pkt);`. `shared/KOSocket.cpp:140-141` `Send` içinde `if (!IsConnected() …) return false;`: soketsiz oturumda gönderim zaten başarısız olur, bu yüzden geçersiz kılma gerekir.
  - `shared/Socket.cpp:97-100` `Socket::Disconnect()` bağlı değilse hemen döner; yani soketsiz (hiç `m_connected` olmayan) bot oturumu `DisconnectCallback` kuyruğuna girmez. Yine de savunma amaçlı koruma eklenir (adım 2).
  - `GameServer/User.h:107-110` `#include "GameDefine.h"`, `class CGameServerDlg;`, `class CUser : public Unit, public KOSocket`; `:573` `CUser(uint16 socketID, SocketMgr *mgr);`, `:575-577` `OnConnect`/`OnDisconnect`/`HandlePacket`.
  - `GameServer/User.cpp:14` `CUser::CUser(uint16 socketID, SocketMgr *mgr) : KOSocket(socketID, mgr, -1, 16384, 3172), Unit(UnitPlayer)` ve boş gövde.
  - `GameServer/GameServerDlg.cpp:17` `#include "DBAgent.h"`; `:97-101` `if (!g_pMain->m_socketMgr.Listen(m_GameServerPort, MAX_USER)) { … return false; }`; `:104-105` AI soketi. `GameServer/GameServerDlg.h:535` `KOSocketMgr<CUser> m_socketMgr;`.
  - `shared/Ini.h` `CIni(const char *)`, `GetInt(section, key, default)`, `GetBool`; `GameServer/Define.h:3` `#define CONF_GAME_SERVER "./GameServer.ini"`. **Dikkat:** `shared/Ini.cpp:108-120` `GetInt` anahtar yoksa varsayılanı **ini dosyasına yazar** (`SetInt` → `Save`). İlk açılışta `GameServer.ini`'ye `[BOT] ENABLED=0` satırı eklenmesi bu yüzden **beklenen** davranıştır (mevcut anahtarlar da böyle çalışıyor).
  - `shared/globals.h:9` `#define MAX_USER 3000`.
  - `GameServer/DamageTrace.cpp:25-45` log dosyası kalıbı (`./Logs/..._<gün>_<ay>_<yıl>.log`, `fopen("a")`, açılamazsa sessiz).
- **PCH ve alt klasör (denendi, 2026-10-02):** proje `PrecompiledHeader=Use` ve `stdafx.h` kullanır. `GameServer/Bot/` altındaki bir `.cpp`'nin ilk satırı **`#include "stdafx.h"`** olmalıdır; `#include "../stdafx.h"` `C1010` hatası verir (Claude geçici bir `Bot\` dosyasıyla Debug'da denedi: `"stdafx.h"` derlendi, `"../stdafx.h"` `C1010` verdi; deneme geri alındı, depoda iz yok). Alt klasör başlıkları için `#include "../User.h"` gibi göreli yollar serbesttir.
- Dosya kodlamaları: `shared/KOSocketMgr.h` ASCII + CRLF; `GameServer/User.h` ASCII + CRLF; `GameServer/User.cpp` UTF-8 **BOM'lu** + CRLF; `GameServer/GameServerDlg.cpp` UTF-8 **BOM'lu** + CRLF; vcxproj ve filters BOM + CRLF. Hepsi **korunur**. Yeni dosyalar ASCII + CRLF.

## 3. Kapsam

**Yapılacaklar**

- `GameServer/Bot/IBotSink.h`: bot alıcısı arayüzü.
- `shared/KOSocketMgr.h`: ayrılmış oturum havuzu API'si (`ReserveSessions`, `AcquireReservedSession`, `ReleaseReservedSession`, `GetReservedSessionMap`) ve `DisconnectCallback` koruması.
- `GameServer/User.h/.cpp`: `m_botSink` alanı (başlangıç `nullptr`) + `Send`/`SendCompressed` geçersiz kılmaları.
- `GameServer/Bot/BotManager.h/.cpp`: tekil `BotManager`; `Startup()` `[BOT]` ayarlarını okur, etkinse havuzu ayırır ve havuz öz-sınamasını çalıştırır; `AcquireSlot()`/`ReleaseSlot()`.
- `GameServer/GameServerDlg.cpp`: `Startup()` içinde `Listen` başarılı olduktan sonra tek `BotManager::Instance().Startup()` çağrısı.
- vcxproj/filters kayıtları.

**Kapsam dışı (yapılmayacak)**

- Bot spawn/despawn, `GameStart` taklidi, DB'den karakter yükleme, hesap/karakter ataması (F2-02+).
- `BOT_TICK` IOCP olayı, `Update()` çağrısı, zaman aşımı muafiyeti, ranking/ödül dışlaması (sonraki planlar).
- Algı, karar, aksiyon, komutlar (`/bot ...`), telemetri.
- `m_botSink`'i `nullptr` dışında bir değere ayarlayan **gerçek bir alıcı sınıfı** (yalnızca arayüz). Öz-sına yalnızca slot taşımayı sınar, alıcı bağlamaz.
- `AssignSocket`/`OnConnect`/`InitSessions` gövdelerini değiştirmek; `MAX_USER`'i değiştirmek; `shared/` altında başka dosya; `AIServer/`, `LogInServer/`, SQL, `docs/**`, `AGENTS.md`, `opencode.json`.
- Sunucuyu çalıştırmak ya da `GameServer.ini`'yi elle düzenlemek (çalışma zamanı doğrulaması Claude'da, §7 sonu).
- "İyileştirme": başka `Send` çağrılarını değiştirmek, `KOSocket::Send`'e dokunmak, çift kilit/yarış düzeltmeleri.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/IBotSink.h` | yeni | ASCII, CRLF |
| `GameServer/Bot/BotManager.h` | yeni | ASCII, CRLF |
| `GameServer/Bot/BotManager.cpp` | yeni | ASCII, CRLF; ilk satır `#include "stdafx.h"` |
| `shared/KOSocketMgr.h` | değiştir | yalnızca yeni üyeler/metotlar ve `DisconnectCallback` koruması (ASCII + CRLF koru) |
| `GameServer/User.h` | değiştir | `class IBotSink;` ön bildirimi, `m_botSink`, iki sanal metot bildirimi (ASCII + CRLF koru) |
| `GameServer/User.cpp` | değiştir | `#include "Bot/IBotSink.h"`, kurucuya `m_botSink(nullptr)`, iki metot gövdesi (BOM + CRLF koru) |
| `GameServer/GameServerDlg.cpp` | değiştir | `#include "Bot/BotManager.h"` + tek çağrı bloğu (BOM + CRLF koru) |
| `GameServer/proj-GameServer.vcxproj` | değiştir | iki `ClCompile`/`ClInclude` girdisi (BOM + CRLF koru) |
| `GameServer/proj-GameServer.vcxproj.filters` | değiştir | yeni dosyalar için `Source Files` / `Header Files` filtreleri (yeni filtre klasörü açma) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. **Doğrula.** §2'deki `dosya:satır` referanslarını depoda aç (özellikle `KOSocketMgr.h:67-68, 104-143`, `User.h:107-110, 573-577`, `User.cpp:14`, `GameServerDlg.cpp:97-101`). Kayma varsa gerçek yeri bul ve raporla. `Unit.h`'de ve `Socket.h`'de `Send(Packet *)` imzasıyla çakışan bir tanım olmadığını doğrula (`grep -n "Send(" GameServer/Unit.h shared/Socket.h shared/KOSocket.h`): `Socket::Send(const uint8 *, uint32)` farklı imzadır ve `KOSocket::Send(Packet *)` tarafından zaten gizlenmektedir; bu normaldir.

2. **`shared/KOSocketMgr.h`.**
   - `#include <set>` ekle (mevcut `#include <map>` yanına).
   - Public API (`GetLock()` bildiriminden sonra):
     ```cpp
     	INLINE SessionMap & GetReservedSessionMap() { return m_reservedSessions; }

     	// Moves the `count` highest-numbered idle sessions into the reserved (bot) pool so
     	// AssignSocket() can never hand them to a real connection. Returns how many were moved.
     	uint16 ReserveSessions(uint16 count);

     	// Moves one reserved session into the active map (no socket attached) and returns it,
     	// or nullptr if the pool is empty.
     	T * AcquireReservedSession();

     	// Moves an acquired session back into the reserved pool. Returns false if the
     	// session is not a reserved slot or is not currently in the active map.
     	bool ReleaseReservedSession(T * pSession);
     ```
   - `protected:` bölümüne: `SessionMap m_reservedSessions;` ve `std::set<uint16> m_reservedIds;` (kalıcı olarak ayrılmış kimlikler; hangi haritada olduklarından bağımsız).
   - Gövdeler dosyanın sonuna, mevcut şablon metot gövdeleri gibi (`template <class T> …`). Hepsi `std::lock_guard<std::recursive_mutex> lock(m_lock);` ile başlar. İskelet:
     ```cpp
     template <class T>
     uint16 KOSocketMgr<T>::ReserveSessions(uint16 count)
     {
     	std::lock_guard<std::recursive_mutex> lock(m_lock);
     	uint16 moved = 0;
     	while (moved < count && !m_idleSessions.empty())
     	{
     		auto itr = std::prev(m_idleSessions.end()); // highest id
     		m_reservedIds.insert(itr->first);
     		m_reservedSessions.insert(*itr);
     		m_idleSessions.erase(itr);
     		moved++;
     	}
     	return moved;
     }
     ```
     (`std::prev` için `<iterator>` gerekirse ekle.) `AcquireReservedSession`: `m_reservedSessions` boşsa `nullptr`; değilse `begin()`'i `m_activeSessions`'a taşı, `static_cast<T *>` döndür. `ReleaseReservedSession`: `pSession == nullptr` veya kimliği `m_reservedIds`'te yoksa `false`; `m_activeSessions`'ta bulunamazsa `false`; bulunursa `m_reservedSessions`'a taşı, `true`.
   - **`DisconnectCallback` koruması** (`:133-143`): aktif haritadan bulunan oturumu, kimliği `m_reservedIds`'te ise `m_reservedSessions`'a, değilse **eskisi gibi** `m_idleSessions`'a ekle. Yalnızca bu `if` eklenir; gerisi aynı kalır.
   - `AssignSocket`, `OnConnect`, `InitSessions`, `Listen` gövdelerinde **hiçbir satır değişmez**.

3. **`GameServer/Bot/IBotSink.h`.**
   ```cpp
   #pragma once

   class Packet;

   // Receiver for the packets a bot session would otherwise send to a client socket.
   // Real (socket) sessions never have a sink.
   class IBotSink
   {
   public:
   	virtual ~IBotSink() {}
   	virtual void OnPacket(Packet & pkt) = 0;
   };
   ```

4. **`GameServer/User.h` / `User.cpp`.**
   - `User.h`: `class CGameServerDlg;` satırının (`:109`) yanına `class IBotSink;`. `CUser` içinde, `HandlePacket` bildiriminden (`:577`) sonra:
     ```cpp
     	// Bot sessions only: receives the packets that would have been sent to a client socket.
     	// Always nullptr for real connections.
     	IBotSink * m_botSink;

     	virtual bool Send(Packet * pkt);
     	virtual bool SendCompressed(Packet * pkt);
     ```
   - `User.cpp`: include bloğunun sonuna `#include "Bot/IBotSink.h"`. Kurucu başlatma listesine `m_botSink(nullptr)` ekle (`:14`). Gövdeler (kurucudan sonra veya `OnConnect`'ten önce, bitişik):
     ```cpp
     bool CUser::Send(Packet * pkt)
     {
     	if (m_botSink != nullptr)
     	{
     		m_botSink->OnPacket(*pkt);
     		return true;
     	}

     	return KOSocket::Send(pkt);
     }

     bool CUser::SendCompressed(Packet * pkt)
     {
     	// Bots receive the uncompressed packet.
     	if (m_botSink != nullptr)
     	{
     		m_botSink->OnPacket(*pkt);
     		return true;
     	}

     	return KOSocket::SendCompressed(pkt);
     }
     ```
     Not: `KOSocket::SendCompressed` kendi içinde `Send(&result)` çağırır (sanal) ve `CUser::Send`'e döner; `m_botSink == nullptr` iken bu eskisiyle aynıdır.

5. **`GameServer/Bot/BotManager.h`.**
   ```cpp
   #pragma once

   class CUser;

   class BotManager
   {
   public:
   	static const uint16 MAX_POOL = 100;

   	static BotManager & Instance();

   	// Reads [BOT] ENABLED / MAX_BOTS from GameServer.ini. Disabled (default): returns true
   	// and does nothing else. Enabled: reserves the slot pool, runs the pool self-test and
   	// writes one status line. Returns false if the pool could not be set up.
   	// Called once from CGameServerDlg::Startup() before the worker threads start.
   	bool Startup();

   	bool isEnabled() const { return m_enabled; }
   	uint16 GetPoolSize() const { return m_poolSize; }

   	// IOCP thread only (used from F2-02 on). The slot is moved to the active session map.
   	CUser * AcquireSlot();
   	// Clears m_botSink and returns the slot to the pool.
   	void ReleaseSlot(CUser * pUser);

   private:
   	BotManager() : m_enabled(false), m_poolSize(0) {}

   	bool m_enabled;
   	uint16 m_poolSize;
   };
   ```
   (`uint16` `stdafx.h` zincirinden gelir; başlık `stdafx.h`'den sonra dahil edilir.)

6. **`GameServer/Bot/BotManager.cpp`.** İlk satır `#include "stdafx.h"`, sonra `#include "BotManager.h"`, `#include "IBotSink.h"`, `#include "../../shared/Ini.h"` gerekiyorsa (diğer dosyalar `../shared/Ini.h` kullanıyor; `GameServer/Bot/` içinden bir seviye daha yukarı). İçerik:
   - `Instance()`: `static BotManager instance; return instance;`.
   - `Startup()`:
     1. `CIni ini(CONF_GAME_SERVER);` `m_enabled = ini.GetBool("BOT", "ENABLED", false);` `false` ise **hemen** `return true` (`MAX_BOTS` okunmaz, hiçbir şey yazılmaz/yazdırılmaz).
     2. `int requested = ini.GetInt("BOT", "MAX_BOTS", 16);` değeri `1..MAX_POOL` aralığına kıs (`MAX_POOL` ve `MAX_USER`'ı geçemez).
     3. `auto & mgr = g_pMain->m_socketMgr;` `std::lock_guard<std::recursive_mutex> lock(mgr.GetLock());` (kilit sınama boyunca tutulur; özyinelemeli kilit olduğu için `AcquireSlot`/`ReleaseSlot` içeriden çağrılabilir).
     4. `m_poolSize = mgr.ReserveSessions(requested);` `m_poolSize != requested` ise sonucu `false` say.
     5. **Havuz öz-sınaması** (yalnızca `m_poolSize > 0` ise; hata → `ok = false`, ilk hatada dur):
        - Her ayrılmış kimlik (`mgr.GetReservedSessionMap()` anahtarları) `>= MAX_USER - m_poolSize` ve `mgr.GetIdleSessionMap()` ile `mgr.GetActiveSessionMap()` içinde **yok**;
        - `m_poolSize` kez `AcquireSlot()`: her biri `nullptr` değil, kimlikler birbirinden farklı, her biri `GetActiveSessionMap()`'te var ve `GetReservedSessionMap()`'te yok;
        - bir `AcquireSlot()` daha (havuz tükendi) → `nullptr` dönmeli;
        - alınanların hepsini `ReleaseSlot()` ile iade et; sonra `GetReservedSessionMap().size() == m_poolSize` ve iade edilenlerin hiçbiri `GetActiveSessionMap()`/`GetIdleSessionMap()`'te yok.
        Sınama sırasında `m_botSink` hiçbir oturumda `nullptr` dışına ayarlanmaz.
     6. **Durum satırı** (tek satır; hem `printf` hem `./Logs/Bot_<gün>_<ay>_<yıl>.log` dosyasına `fopen("a")` ile; dosya açılamazsa sessizce geç — `DamageTrace.cpp:25-45` kalıbı): başarıda
        `BotManager: reserved %u sessions (ids %u-%u), pool self-test OK`
        başarısızlıkta `BotManager: pool setup FAILED (%s)` (`%s` kısa İngilizce neden: `reserve`, `range`, `acquire`, `exhaust`, `release`). `ids` aralığı havuzun en küçük ve en büyük kimliğidir (`MAX_BOTS=16` için `2984-2999`).
     7. `return ok;`.
   - `AcquireSlot()`: `mgr.GetLock()` altında `return mgr.AcquireReservedSession();`.
   - `ReleaseSlot(CUser * pUser)`: `pUser == nullptr` ise dön; kilit altında `pUser->m_botSink = nullptr; mgr.ReleaseReservedSession(pUser);`.
   - Konsol/log başka hiçbir yerde yazılmaz.

7. **`GameServer/GameServerDlg.cpp`.** `:17` `#include "DBAgent.h"` satırından sonra `#include "Bot/BotManager.h"`. `:97-101` Listen bloğunun `}` kapanışından **hemen sonra**:
   ```cpp
   	if (!BotManager::Instance().Startup())
   	{
   		printf(_T("ERROR : Failed to set up the bot session pool.\n"));
   		return false;
   	}
   ```
   Başka hiçbir yere dokunma (kapanış çağrısı gerekmez; havuz `m_socketMgr` ile birlikte yok olur).

8. **vcxproj/filters.** `proj-GameServer.vcxproj`: `<ClCompile Include="DamageTrace.cpp" />` satırının hemen altına `<ClCompile Include="Bot\BotManager.cpp" />`; `<ClInclude Include="DamageTrace.h" />` satırının hemen altına `<ClInclude Include="Bot\BotManager.h" />` ve `<ClInclude Include="Bot\IBotSink.h" />`. `.filters`: `DamageTrace.cpp` / `DamageTrace.h` girdilerindeki gibi (`Source Files` / `Header Files`), yeni `Filter` klasörü tanımlama.

9. **Derle ve doğrula** (§6). Uyarı sayısını saymak için bu plandan önceki duruma karşılaştırma gerekmez; yeni uyarı kriteri K1'de tanımlıdır.

10. Raporu bu plan dosyasının "Uygulayıcı Raporu"na yaz; her kriterin çıktısını yapıştır.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; **yeni uyarı yok**: çıktıdaki hiçbir uyarı `Bot\` dosyalarında, `KOSocketMgr.h`'de veya bu planın eklediği satırlarda değildir (`./tools/build.sh Release 2>&1 | grep -a "warning" | sort -u` çıktısını, her uyarının dosya:satırını eklenen/değişen satırlarla karşılaştırarak yapıştır; eski satırların uyarıları kabul).
- [ ] K2: `./tools/build.sh Debug` hatasız biter.
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F2-01` yalnızca §4 tablosundaki dosyaları içerir; `GameServerDlg.cpp` farkı **≤ 10 ekleme / 0 silme**; `User.h` farkı **≤ 10 ekleme / 0 silme**; `User.cpp` farkı **≤ 25 ekleme / 1 silme** (yalnızca kurucu başlatma satırı değişir); `shared/KOSocketMgr.h` farkında silinen satırlar yalnızca `DisconnectCallback` gövdesindeki değişen satırdır (fark çıktısını yapıştır).
- [ ] K4: Havuz değişmezi: `AssignSocket`, `OnConnect`, `InitSessions`, `Listen` gövdeleri `git diff gece/2026-10-02...bot/F2-01 -- shared/KOSocketMgr.h` içinde **değişmemiş** (fark çıktısında bu fonksiyonların satırları yok); `DisconnectCallback`'te kimliği `m_reservedIds`'te olan oturum `m_reservedSessions`'a, diğerleri `m_idleSessions`'a gider (`sed -n` çıktısını yapıştır).
- [ ] K5: Bot kapalıyken davranış: `grep -n "m_botSink" GameServer/*.cpp GameServer/*.h GameServer/Bot/* shared/*` çıktısında `m_botSink`'e `nullptr` dışında bir değer atayan satır **yok** (yalnızca kurucu `m_botSink(nullptr)`, `ReleaseSlot` içindeki `= nullptr`, `Send`/`SendCompressed` içindeki denetim/çağrılar, bildirim) — çıktıyı yapıştır. `BotManager::Startup()` `ENABLED` `false` iken `ini.GetInt("BOT", "MAX_BOTS", …)`'a ulaşmadan döner (`sed -n` ile göster).
- [ ] K6: `Send`/`SendCompressed` geçersiz kılmaları adım 4'teki gibidir; `m_botSink == nullptr` yolu son ifade olarak `KOSocket::Send(pkt)` / `KOSocket::SendCompressed(pkt)` döndürür (`grep -n` ve `sed -n` çıktısı).
- [ ] K7: Havuz öz-sınaması: `BotManager.cpp`'de adım 6.5'in tüm denetimleri (aralık, benzersiz kimlik, aktif/ayrılmış harita üyeliği, tükenme, iade sonrası denetim) ve adım 6.6'daki **tam** durum satırı biçimi (`reserved %u sessions (ids %u-%u), pool self-test OK`) bulunur (`grep -n` çıktısı).
- [ ] K8: Thread/kilit: `BotManager::Startup()`, `AcquireSlot()` ve `ReleaseSlot()` oturum haritalarına yalnızca `GetLock()` altında dokunur; `KOSocketMgr` yeni metotları `m_lock` ile başlar (`sed -n` çıktısı).
- [ ] K9: Kodlama: `file shared/KOSocketMgr.h GameServer/User.h GameServer/User.cpp GameServer/GameServerDlg.cpp GameServer/Bot/*` — `KOSocketMgr.h`/`User.h` "ASCII … CRLF", `User.cpp`/`GameServerDlg.cpp` "UTF-8 (with BOM) … CRLF", yeni dosyalar "ASCII text, with CRLF" (önceki kodlamalarla aynı; çıktıyı yapıştır).
- [ ] K10: `git status --short` boş (yalnızca izinli dosyalar commit'li; `build/` ve `Logs/` depoda değil). Sunucu çalıştırılmadı, `GameServer.ini` değiştirilmedi.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
git diff --stat gece/2026-10-02...bot/F2-01
git diff gece/2026-10-02...bot/F2-01 -- shared/KOSocketMgr.h GameServer/GameServerDlg.cpp GameServer/User.h
file shared/KOSocketMgr.h GameServer/User.h GameServer/User.cpp GameServer/GameServerDlg.cpp GameServer/Bot/*
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini`'ye `[BOT] ENABLED=1`, `MAX_BOTS=16` yazıp sunucuyu `tools/run-servers.sh start` ile aç: `Logs/Bot_*.log`'da `BotManager: reserved 16 sessions (ids 2984-2999), pool self-test OK`; sunucu `UP`; `ENABLED=0` iken log satırı yok; yeni bir istemci girişi sorunsuz. Sonra sunucuyu `tools/run-servers.sh stop` ile kapat ve ini'yi eski haline getir.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). Yeni dosyalar ASCII.
- **Bot sistemi varsayılan kapalı:** `[BOT] ENABLED` anahtarı ini'de yoksa `CIni::GetInt` onu `0` olarak dosyaya yazar (mevcut ini davranışı); bu beklenen ve kabul edilmiştir. Başka hiçbir çalışma zamanı etkisi olmamalı.
- **Thread kuralı (`AGENTS.md` §2.6):** `Startup()` ana thread'de, IOCP işçi thread'leri başlamadan önce çalışır. `AcquireSlot`/`ReleaseSlot` sonraki planlarda IOCP thread'inde çağrılacak; bu planda yalnızca öz-sınamada ve `GetLock()` altında kullanılır. `m_botSink` yalnızca IOCP thread'inde atanmalıdır (bu planda atanmaz).
- Havuz kimlikleri 3000'in altında kalır; AIServer kabul eder (`docs/13` §4.1). Gerçek oyuncu kapasitesi `MAX_BOTS` kadar azalır (`ENABLED=1` iken); `ENABLED=0` iken azalmaz.
- `Send`/`SendCompressed` her paket gönderiminde çağrılan sıcak yoldur: ek maliyet tek bir `nullptr` karşılaştırmasıdır; başka iş yapma.
- `KOSocketMgr<T>` `LogInServer`'da (`KOSocketMgr<LoginSession>`, `LogInServer/LoginServer.h:30`) ve `shared/ClientSocketMgr.h:9` üzerinden `AIServer`'da da kullanılıyor: `shared/` değişikliği onların derlemesini de etkiler. Yeni üyeler yalnızca eklenir, o sunucularda kullanılmaz; `tools/build.sh` tüm çözümü derler, üç sunucunun derlemesi hatasız bitmelidir (K1/K2).
- Bu plana özgü risk: R-CODE-01 (oturum haritalarının kilitsiz kopyalanması). Havuz öz-sınaması aktif haritaya açılışta, `Timer_UpdateSessions` başlamadan önce kısa süre oturum ekler/çıkarır; bu yüzden tüm sınama `GetLock()` altında ve zamanlayıcı thread'leri başlamadan yapılır. Yeni kilitsiz kopya **ekleme**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F2-01` — `<kısa-sha> [F2-01] …`
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
- İncelenen: `gece/2026-10-02...bot/F2-01` @ `<sha>`
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
