# F2-02: `BOT_TICK` IOCP olayı ve bot zamanlayıcı thread'i (S9, ADR-0005, varsayılan kapalı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F2 — Bot oturumu (`docs/17` §2) |
| Branch | `bot/F2-02` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F2-01 (`KAPANDI`, `gece/2026-10-02`'ye birleşti: `BotManager`, `[BOT]` ini anahtarları, `WriteBotLog`) |
| İlgili gereksinim / kabul | ADR-0005 (`docs/adr/ADR-0005-bot-tick-thread-modeli.md`); `docs/13` §3.1; `docs/02` §11 S8, S9; AC-ARCH-02 (bot kapalıyken davranış aynı) |
| Tahmini büyüklük | S (5 dosya, ~120 satır ekleme) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Botların ileride giriş durum makinesini, `Update()` çağrısını ve karar tick'ini çalıştıracağı **tek ortak mekanizmayı** eklemek, **hiçbir bot davranışı olmadan**:

- `shared/SocketMgr`'a yeni bir IOCP olayı `SOCKET_IO_EVENT_BOT_TICK` ve bir geri çağrı kancası (`SetBotTickHandler`), tek-uçuşta gönderim (`PostBotTick`).
- `BotManager`'a bir `BotTickTimer` thread'i (her `TICK_MS`'de `PostBotTick`) ve IOCP thread'inde çalışan boş `Tick()` (yalnızca sayaç ve iki öz-sınama log satırı).

`[BOT] ENABLED=0` (varsayılan) iken **thread açılmaz, kanca kurulmaz**; sunucu eskisi gibi çalışır. `ENABLED=1` iken tick IOCP işçi thread'inde, zamanlayıcıdan farklı bir thread'de çalışır ve bunu log kanıtlar.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0005-bot-tick-thread-modeli.md` ve `docs/13` §3.1: karar ve gerekçe.
- `AGENTS.md` §2 kural 6: bot durumu yalnızca IOCP thread'inde değiştirilir. Bu planda `Tick()` hiçbir `CUser`'a dokunmaz; zamanlayıcı thread'i yalnızca olay gönderir.
- İlgili kod (dosya:satır, 2026-10-02'de depoda doğrulandı):
  - `shared/SocketDefines.h:3-9`: `enum SocketIOEvent` (`READ_COMPLETE=0`, `WRITE_END=1`, `THREAD_SHUTDOWN=2`, `NUM_SOCKET_IO_EVENTS=3`); `class OverlappedStruct` (satır 11 sonrası; `m_overlap`, `m_event`, `Reset(ev)`).
  - `shared/SocketMgr.h:63-74`: `typedef void(*OperationHandler)(Socket * s, uint32 len);`, üç işleyici bildirimi ve `static OperationHandler ophandlers[] = { &HandleReadComplete, &HandleWriteComplete, &HandleShutdown }` (olay değeriyle indekslenir).
  - `shared/SocketMgr.cpp:51`: `m_thread = new Thread(SocketWorkerThread, this)`; **tek** IOCP işçi thread'i. `:58-89` işçi döngüsü: `GetQueuedCompletionStatus(cp, &len, (LPDWORD)&s, &ol_ptr, INFINITE)`, `ov = CONTAINING_RECORD(ol_ptr, OverlappedStruct, m_overlap)`, `if (ov->m_event == SOCKET_IO_THREAD_SHUTDOWN) { delete ov; return 0; }`, `if (ov->m_event < NUM_SOCKET_IO_EVENTS) ophandlers[ov->m_event](s, len);`. `:141` `HandleShutdown` (boş). `:151-154` `ShutdownThreads`: `PostQueuedCompletionStatus(m_completionPort, 0, (ULONG_PTR)0, &ov->m_overlap)` (anahtar `0` kalıbı; bu planın da kullanacağı).
  - `shared/SocketMgr.h:15-23,28,41`: `SocketMgr` sınıfı, `m_completionPort` (`HANDLE`, public), `s_bRunningCleanupThread` (static public). `<atomic>` `shared/stdafx.h:54`'te dahil.
  - `GameServer/GameServerDlg.cpp:213`: `g_pMain->m_socketMgr.RunServer();` (işçi thread'i burada başlar; `Startup()`'ın son iş satırı, `return true` öncesi). `:3125-3131`: `CGameServerDlg::~CGameServerDlg()` önce `g_timerThreads`'i `waitForExit` ile bekler; soket sistemi `:3147-3149` civarında en son kapanır (`m_aiSocketMgr.Shutdown(); m_socketMgr.Shutdown();`). `GameServer/main.cpp:51` `g_bRunning = false` değerini `delete g_pMain`'den önce yazar.
  - `GameServer/GameServerDlg.cpp:736-766` (`Timer_UpdateSessions`): zamanlayıcı thread kalıbı (`while (g_bRunning) { ...; sleep(n * SECOND); }`, `uint32 THREADCALL` imzası, `new Thread(fn)`). `shared/Thread.h`: `Thread(lpfnThreadFunc, void * param)`, `waitForExit()`.
  - `GameServer/Bot/BotManager.h/.cpp` (F2-01): `Instance()`, `Startup()` (`CIni ini(CONF_GAME_SERVER)`, `ENABLED`/`MAX_BOTS`), statik `WriteBotLog(const char * line)`.
  - `shared/Ini.cpp:108-120`: `GetInt` anahtar yoksa varsayılanı ini dosyasına **yazar**. `TICK_MS` yalnızca `ENABLED=1` iken okunmalı (kapalıyken ini'ye yeni satır yazılmasın).

## 3. Kapsam

**Yapılacaklar**

- `shared/SocketDefines.h`: `SOCKET_IO_EVENT_BOT_TICK = 3`, `NUM_SOCKET_IO_EVENTS = 4`.
- `shared/SocketMgr.h/.cpp`: `BotTickHandler` tipi, `SetBotTickHandler`, `PostBotTick`, `HandleBotTick`, `ophandlers`'e dördüncü giriş.
- `GameServer/Bot/BotManager.h/.cpp`: `TICK_MS` okuma, `StartTicking()`, `Shutdown()`, `Tick()` (boş gövde + sayaç + iki log satırı), zamanlayıcı thread'i.
- `GameServer/GameServerDlg.cpp`: `RunServer()` sonrası `StartTicking()`, yıkıcının başında `Shutdown()`.

**Kapsam dışı (yapılmayacak)**

- Bot spawn/despawn, giriş akışı, `Update()` çağrısı, zaman aşımı muafiyeti, ranking/ödül (F2-03+).
- Tick süresi ölçümü/histogramı (MET-PERF-02, F3), tick gruplama, `BotPerception`/karar/aksiyon.
- `Timer_UpdateSessions`'a, `HandlePacket`'e, `CUser`'a veya oturum havuzuna dokunmak.
- `LogInServer/`, `AIServer/`, SQL, `docs/**`, `AGENTS.md`, `opencode.json`, vcxproj dosyaları (yeni dosya yok).
- Sunucuyu çalıştırmak ya da `GameServer.ini`'yi elle düzenlemek (çalışma zamanı doğrulaması Claude'da, §7 sonu).
- "İyileştirme": `ophandlers`'i `.cpp`'ye taşımak, `OverlappedStruct`'ı değiştirmek, `SocketWorkerThread`'e dokunmak, işçi thread sayısını artırmak.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `shared/SocketDefines.h` | değiştir | yalnızca enum (ASCII + CRLF koru) |
| `shared/SocketMgr.h` | değiştir | üye bildirimleri + `HandleBotTick` + `ophandlers` girişi (ASCII + CRLF koru) |
| `shared/SocketMgr.cpp` | değiştir | statik üye tanımları, `PostBotTick`, `SetBotTickHandler`, `HandleBotTick` (ASCII + CRLF koru) |
| `GameServer/Bot/BotManager.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotManager.cpp` | değiştir | ASCII, CRLF |
| `GameServer/GameServerDlg.cpp` | değiştir | iki küçük blok (BOM + CRLF koru); `include "Bot/BotManager.h"` zaten var |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. **Dal:** `git switch -c bot/F2-02 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `stop`.

2. **`shared/SocketDefines.h`:** enum'u şöyle yap (mevcut üç değer ve sırası değişmez):
   ```cpp
   	SOCKET_IO_THREAD_SHUTDOWN		= 2,
   	SOCKET_IO_EVENT_BOT_TICK		= 3,
   	NUM_SOCKET_IO_EVENTS			= 4,
   ```

3. **`shared/SocketMgr.h`:**
   - `SocketMgr` sınıfında (public bölüm; `s_bRunningCleanupThread` yakınına) ekle:
     ```cpp
     	// Bot tick: a timer thread posts SOCKET_IO_EVENT_BOT_TICK; the IOCP worker runs the handler.
     	typedef void (*BotTickHandler)();
     	static void SetBotTickHandler(BotTickHandler handler);
     	// Posts one BOT_TICK event. Returns false (and posts nothing) if the previous one has not
     	// been consumed yet or the post failed, so the queue never accumulates ticks.
     	bool PostBotTick();

     	static BotTickHandler s_botTickHandler;
     	static std::atomic<bool> s_botTickPending;
     ```
   - Dosya sonundaki işleyici bildirimlerine `void HandleBotTick(Socket * s, uint32 len);` ekle.
   - `ophandlers` dizisinin **sonuna** (`&HandleShutdown`'dan sonra, virgülle) `&HandleBotTick` ekle. Mevcut üç giriş ve sırası değişmez; dizi artık 4 eleman (`NUM_SOCKET_IO_EVENTS` ile aynı).

4. **`shared/SocketMgr.cpp`:**
   - Dosya başındaki statik üye tanımlarının yanına:
     ```cpp
     SocketMgr::BotTickHandler SocketMgr::s_botTickHandler = nullptr;
     std::atomic<bool> SocketMgr::s_botTickPending(false);
     ```
   - `SetBotTickHandler(h)`: `s_botTickHandler = h;`. (Yalnızca sunucu başlangıcında, zamanlayıcı thread'i başlamadan çağrılır.)
   - `PostBotTick()`:
     ```cpp
     bool SocketMgr::PostBotTick()
     {
     	// One persistent event object: the worker never deletes BOT_TICK events.
     	static OverlappedStruct ov(SOCKET_IO_EVENT_BOT_TICK);

     	bool expected = false;
     	if (!s_botTickPending.compare_exchange_strong(expected, true))
     		return false; // previous tick still queued or running

     	ov.Reset(SOCKET_IO_EVENT_BOT_TICK);
     	if (!PostQueuedCompletionStatus(m_completionPort, 0, (ULONG_PTR)0, &ov.m_overlap))
     	{
     		s_botTickPending = false;
     		return false;
     	}
     	return true;
     }
     ```
   - `HandleBotTick(Socket * s, uint32 len)` (işleyicilerin yanına): kancayı çağır, **ardından** bekleme bayrağını temizle:
     ```cpp
     void HandleBotTick(Socket * s, uint32 len)
     {
     	if (SocketMgr::s_botTickHandler != nullptr)
     		SocketMgr::s_botTickHandler();
     	SocketMgr::s_botTickPending = false;
     }
     ```
   - `SocketWorkerThread`, `ShutdownThreads` ve diğer işleyicilere **dokunma**.

5. **`GameServer/Bot/BotManager.h`:** `class Thread;` ön bildirimi; üyeler:
   ```cpp
   	// Starts the BotTickTimer thread. No-op unless enabled. Call once after RunServer().
   	void StartTicking();
   	// Joins the timer thread. Idempotent; safe when ticking never started.
   	void Shutdown();

   private:
   	static uint32 THREADCALL TimerThreadProc(void * lpParam);
   	static void TickCallback();
   	void Tick(); // IOCP worker thread only

   	uint32 m_tickMs;
   	Thread * m_timerThread;
   	std::atomic<bool> m_shuttingDown;
   	std::atomic<uint32> m_timerThreadId;
   	std::atomic<uint32> m_skippedTicks;
   	uint32 m_tickCount;      // IOCP thread only
   	uint32 m_tickThreadId;   // IOCP thread only
   	std::chrono::steady_clock::time_point m_firstTickTime; // IOCP thread only
   ```
   Kurucuyu `m_tickMs(100), m_timerThread(nullptr), m_shuttingDown(false), m_timerThreadId(0), m_skippedTicks(0), m_tickCount(0), m_tickThreadId(0)` ile başlat. `<atomic>` ve `<chrono>` başlıkta ya da `.cpp` ve başlıkta gerektiği kadar dahil et.

6. **`GameServer/Bot/BotManager.cpp`:**
   - `Startup()`: `MAX_BOTS` okunduktan **hemen sonra** (etkin yolda, `ENABLED=false` erken dönüşünden sonra) `m_tickMs`'yi `ini.GetInt("BOT", "TICK_MS", 100)` ile oku ve `[20, 1000]` aralığına kıs. Başka satırı değiştirme.
   - `StartTicking()`: `if (!m_enabled || m_timerThread != nullptr) return;` sonra `SocketMgr::SetBotTickHandler(&BotManager::TickCallback); m_timerThread = new Thread(TimerThreadProc, this);`.
   - `TimerThreadProc(void * lpParam)`: `BotManager * self = (BotManager *)lpParam; self->m_timerThreadId = GetCurrentThreadId(); while (g_bRunning && !self->m_shuttingDown) { sleep(self->m_tickMs); if (!g_pMain->m_socketMgr.PostBotTick()) self->m_skippedTicks++; } return 0;`. Başka bir şeye dokunmaz.
   - `TickCallback()`: `Instance().Tick();`.
   - `Tick()` (IOCP thread; **hiçbir `CUser`, harita veya `g_pMain` üyesine dokunma**):
     1. `if (m_shuttingDown) return;`
     2. `m_tickCount++`. İlk tick'te (`m_tickCount == 1`): `m_tickThreadId = GetCurrentThreadId()`, `m_firstTickTime = steady_clock::now()` ve tek log satırı (aşağıda).
     3. `m_tickCount == 101`'de (ilk tick'ten sonraki 100 aralık): geçen süreyi ms olarak hesapla ve tek log satırı yaz (aşağıda). Sonrası: başka log yok.
   - Log satırları (**tam biçim**, yalnızca `WriteBotLog`; konsola `printf` **yazma**):
     - İlk tick, tick thread'i zamanlayıcıdan farklıysa: `BotManager: tick OK on IOCP thread <tid> (timer thread <tid>), period <m_tickMs> ms`
     - Aynıysa (olmamalı): `BotManager: tick CHECK FAILED (tick ran on the timer thread <tid>)`
     - 101. tick: `BotManager: 100 tick intervals in <N> ms (avg <A.A> ms), skipped <S>` — `<A.A>` bir ondalık basamaklı ortalama (`N / 100.0`), `<S>` = `m_skippedTicks`.
     - `<tid>` `(unsigned)` ile `%u` yazılır.
   - `Shutdown()`: `m_shuttingDown = true; if (m_timerThread != nullptr) { m_timerThread->waitForExit(); delete m_timerThread; m_timerThread = nullptr; }`.
   - `WriteBotLog` zaten dosyanın başında tanımlı; yeniden tanımlama.

7. **`GameServer/GameServerDlg.cpp`:**
   - `Startup()` içinde `g_pMain->m_socketMgr.RunServer();` satırından **hemen sonra** `BotManager::Instance().StartTicking();` ekle (başka satır değişmez).
   - `CGameServerDlg::~CGameServerDlg()` gövdesinin **ilk** ifadesi olarak `BotManager::Instance().Shutdown();` ekle (zamanlayıcı thread'leri beklenmeden önce; soket sistemi kapanmadan çok önce).

8. **Derle:** `./tools/build.sh Release` ve `./tools/build.sh Debug`; ikisi de hatasız bitmeli. Tüm çözüm derlenir (`shared` `AIServer` ve `LogInServer`'da da kullanılır).

9. Sunucuyu çalıştırma (§8). Yalnızca kodu commit'le (`[F2-02] ...`), Uygulayıcı Raporu'nu yaz, `Durum`'u `UYGULANDI` yap.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; **yeni uyarı yok** (çıktıdaki hiçbir uyarı `Bot\` dosyalarında, `SocketMgr`/`SocketDefines`'ta veya eklenen satırlarda değil; `./tools/build.sh Release 2>&1 | grep -a "warning" | sort -u` çıktısını yapıştır, eski satırların uyarıları kabul).
- [ ] K2: `./tools/build.sh Debug` hatasız biter.
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F2-02` yalnızca §4 tablosundaki dosyaları içerir; `GameServerDlg.cpp` farkı **≤ 4 ekleme / 0 silme**; `SocketDefines.h` farkı **2 ekleme / 1 silme** (`NUM_SOCKET_IO_EVENTS` satırı); `SocketMgr.h` farkında silinen satır yalnızca `&HandleShutdown` satırı (virgül eklenir); `SocketMgr.cpp` farkı **0 silme** (çıktıyı yapıştır).
- [ ] K4: Enum/dizi: `SOCKET_IO_EVENT_BOT_TICK = 3`, `NUM_SOCKET_IO_EVENTS = 4`; `ophandlers` 4 elemanlı ve sırası `HandleReadComplete, HandleWriteComplete, HandleShutdown, HandleBotTick` (`sed -n` çıktısı). `SocketWorkerThread` ve `ShutdownThreads` gövdeleri `git diff`te **yok**.
- [ ] K5: Tek-uçuşta gönderim: `PostBotTick` bekleme bayrağını `compare_exchange_strong` ile alır, `OverlappedStruct` `static` ve kalıcıdır (hiçbir yerde `new`/`delete` yok); `HandleBotTick` kancayı çağırdıktan **sonra** bayrağı temizler; gönderim başarısızsa bayrak geri temizlenir (`sed -n` çıktısı).
- [ ] K6: Bot kapalıyken davranış: `StartTicking()` `!m_enabled` iken ilk ifadede döner (kanca kurulmaz, thread açılmaz); `TICK_MS` yalnızca etkin yolda okunur, `ENABLED=false` erken dönüşünden **sonra** (`sed -n` çıktısı).
- [ ] K7: Thread kuralı: `Tick()` içinde `g_pMain`, `CUser`, oturum haritası veya `GetLock()` kullanımı **yok**; `TimerThreadProc` yalnızca `PostBotTick` ve sayaç artırır (`grep -n "g_pMain\|CUser\|GetLock" GameServer/Bot/BotManager.cpp` çıktısını, F2-01'den kalan `Startup`/`AcquireSlot`/`ReleaseSlot` ve `TimerThreadProc`'taki `g_pMain->m_socketMgr.PostBotTick()` satırı dışında bir kullanım olmadığını göstererek yapıştır).
- [ ] K8: Log biçimi: `BotManager.cpp`'de üç log dizgesi §5.6'daki **tam** metinlerle bulunur (`grep -n "tick OK on IOCP thread\|tick CHECK FAILED\|tick intervals in"` çıktısı); bu satırlar yalnızca `WriteBotLog` ile yazılır (`printf` yok).
- [ ] K9: Kapanış sırası: `~CGameServerDlg` gövdesinin ilk ifadesi `BotManager::Instance().Shutdown();`; `StartTicking()` çağrısı `RunServer()` satırından hemen sonra (`sed -n` çıktısı).
- [ ] K10: Kodlama: `file shared/SocketDefines.h shared/SocketMgr.h shared/SocketMgr.cpp GameServer/GameServerDlg.cpp GameServer/Bot/*` — `shared/*` ve `Bot/*` "ASCII … CRLF", `GameServerDlg.cpp` "UTF-8 (with BOM) … CRLF" (önceki kodlamayla aynı; çıktıyı yapıştır).
- [ ] K11: `git status --short` boş (yalnızca izinli dosyalar commit'li; `build/` ve `Logs/` depoda değil). Sunucu çalıştırılmadı, `GameServer.ini` değiştirilmedi.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
git diff --stat gece/2026-10-02...bot/F2-02
git diff gece/2026-10-02...bot/F2-02 -- shared GameServer/GameServerDlg.cpp
sed -n '1,25p' shared/SocketDefines.h
sed -n '60,80p' shared/SocketMgr.h
file shared/SocketDefines.h shared/SocketMgr.h shared/SocketMgr.cpp GameServer/GameServerDlg.cpp GameServer/Bot/*
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini`'ye `[BOT] ENABLED=1`, `MAX_BOTS=16` yazıp sunucuyu `tools/run-servers.sh start` ile aç, ~15 sn bekle: `Logs/Bot_*.log`'da F2-01 satırının yanı sıra `BotManager: tick OK on IOCP thread … (timer thread …), period 100 ms` ve `BotManager: 100 tick intervals in ~10000 ms (avg ~100 ms), skipped 0` (ortalama 90–130 ms kabul; Windows zamanlayıcı kaba granülaritesi); sunucu 3/3 `UP`. `TICK_MS=20` ile ortalama ≥ 20 ms. `ENABLED=0` iken bu satırların hiçbiri yazılmaz ve ini'ye `TICK_MS` eklenmez. Sonra `tools/run-servers.sh stop` ve ini'yi eski haline getir (kapanışta çökme yok).

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). Yeni dosya yok.
- **Bot sistemi varsayılan kapalı:** `ENABLED=0` iken ne thread ne kanca ne de olay vardır. Enum'a eklenen değer ve dördüncü `ophandlers` girişi davranışı değiştirmez (işçi yalnızca olay gelirse dizine bakar).
- **`shared/` üç sunucuda kullanılır** (`GameServer`, `AIServer`, `LogInServer`): `SocketMgr`'ın yeni üyeleri yalnızca eklenir. `AIServer`/`LogInServer` kancayı kurmaz; tüm çözüm hatasız derlenmeli (K1/K2).
- **`ophandlers` bir başlık içi `static` dizidir:** her çeviri biriminde kendi kopyası vardır; yalnızca `SocketMgr.cpp`'dekini işçi thread'i kullanır. Diziye ekleme tüm kopyaları tutarlı tutar, ek önlem gerekmez. `HandleBotTick`'in `SocketMgr.cpp`'de tanımlanması tüm projelerde bağlanmasını sağlar.
- **Tek işçi thread'i:** `Tick()`'in diğer paket işleyicileriyle seri çalışması bu varsayıma dayanır (`SocketMgr.cpp:51`). İşçi thread sayısını değiştirme.
- **Tick yavaşlarsa** zamanlayıcı yeni olay göndermez (atlanan tick `skipped` sayacında görünür); kuyruğu şişirme.
- **Kapanış:** `m_shuttingDown` bayrağı yıkıcı sırasında geç gelen bir tick'in iş yapmamasını sağlar; bu planda `Tick()` zaten hiçbir paylaşılan duruma dokunmaz, ancak sonraki planlar bu bayrağa saygı göstermelidir.
- `GetCurrentThreadId()`, `sleep()`, `Thread` ve `g_bRunning` `GameServer/stdafx.h` üzerinden erişilebilir (`GameServerDlg.cpp` kullanıyor); ek başlık gerekirse yalnızca `.cpp`'ye ekle.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F2-02` — `<kısa-sha> [F2-02] …`
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
