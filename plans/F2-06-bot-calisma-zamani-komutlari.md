# F2-06: Bot çalışma zamanı komutları: konsol `/bot spawn|despawn|list` ve `BotCommands.txt` komut dosyası (varsayılan kapalı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F2 — Bot oturumu (`docs/17` §2) |
| Branch | `bot/F2-06` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F2-03 (`KAPANDI`: spawn, `BotSession`), F2-04 (`KAPANDI`: `BeginDespawn`), F2-05 (`KAPANDI`: `ResetForRespawn`) |
| İlgili gereksinim / kabul | `docs/17` §2 F2 kapsamı "`/bot spawn/despawn` (minimum)", S11 (`docs/02` §11), `docs/13` §10, ADR-0015 |
| Tahmini büyüklük | S–M (4 dosya, ~230 satır ekleme) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

F2-05 sonunda botlar yalnızca sunucu açılışında (`[BOT] SPAWN_ON_START`) ve ini zamanlayıcılarıyla yönetiliyor. Bu plan sonunda, `[BOT] ENABLED=1` iken, çalışma zamanında şu üç komut kullanılabilir:

| Komut | İşlev |
|---|---|
| `spawn <ad>[,<ad>...]` | Adı verilen sabit botları (yalnızca `BOT_TABLE`'daki 12 bot) girişe sokar; daha önce çıkmış (`PHASE_DESPAWNED`) bir bot aynı oturumla yeniden girer |
| `despawn <ad>[,<ad>...]` veya `despawn all` | Oyunda olan (`PHASE_IN_GAME`) botları güvenli çıkışla (F2-04 `BeginDespawn`) kaldırır |
| `list` | Her oturumun adı, fazı, slotu ve çıkış sayısı + havuzdaki boş slot sayısı |

İki giriş yolu vardır, ikisi de **aynı komut çekirdeğini** kullanır (ADR-0015):
1. **Konsol:** `/bot spawn BotWP_K,BotMF_K`, `/bot despawn all`, `/bot list` (sunucu penceresine insan yazar).
2. **Komut dosyası:** GameServer çalışma dizinindeki `BotCommands.txt` (satır başına bir komut, başında `/bot` **olmadan**: `spawn BotWP_K`). Sunucu saniyede bir bakar, dosyayı alıp çalıştırır ve siler. Otomasyon (Claude doğrulaması, gece döngüsü, ileride F3) bu yolu kullanır; konsola betikle yazılamaz.

Sonuçlar `Bot_*.log`'a yazılır. `ENABLED=0` (varsayılan) iken konsol komutu yalnızca "bot sistemi kapalı" der, dosya hiç aranmaz; sunucu davranışı F2-05'tekiyle aynıdır. Bu, F2'nin son kod planıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0015-bot-calisma-zamani-komut-kanali.md`: neden iki yol, neden kuyruk, neden dosya (**önce oku**).
- `docs/adr/ADR-0005-bot-tick-thread-modeli.md`: bot durumu yalnızca IOCP thread'inde değişir. Konsol thread'i `BotManager` durumuna **dokunamaz**; yalnızca kilitli kuyruğa ekler.
- `plans/F2-04-bot-cikisi-despawn.md` §2/§8 ve `plans/F2-05-bot-yeniden-spawn-dongusu.md` §2/§8: spawn/despawn durum makinesi ve slot ömrü kuralları (değişmez).
- İlgili kod (dosya:satır, 2026-10-02'de depoda doğrulandı):
  - `GameServer/Bot/BotManager.cpp:29-42` `WriteBotLog(const char *)` (`static`, `./Logs/Bot_<gün>_<ay>_<yıl>.log`); `:46-53` `BotAccountEntry` ve `BOT_TABLE[12]` (`charName`, `accountName`); `:12-20` zaman sabitleri (`SPAWN_START_DELAY_MS = 5000`, ...).
  - `BotManager.cpp:265-301` `Tick()`: `:271-` ilk tick'te `m_firstTickTime` kurulur, `:300` `TickSessions();`; `ProcessCommands()` bu çağrının **hemen öncesine** gelir.
  - `BotManager.cpp:303-414` `ParseSpawnList` (ad ayrıştırma + `BOT_TABLE` arama + `m_sessions.push_back(new BotSession(...))`; **değişmez**, yalnızca örnek).
  - `BotManager.cpp:416-` `TickSessions`: `m_sessions.empty()` ise erken döner (`:417-418`), ilk tick'ten 5 sn geçmeden spawn başlatmaz (`:421-423`); `PHASE_QUEUED` oturumları tick başına bir tane başlatır. Komutla eklenen/yeniden kuyruğa alınan oturumları bu döngü zaten işler.
  - `BotManager.cpp:581` `BeginDespawn(BotSession *, time_point)` (yalnızca IOCP thread'i; `OnDisconnect()` + `PHASE_DESPAWN_WAIT` + `despawning` logu); `:598` `PollDespawn` slotu DB kaydı bitince iade eder.
  - `GameServer/Bot/BotSession.h:16-26` `Phase` sabitleri; `:38-` alanlar (`m_charName`, `m_pUser`, `m_phase`, `m_slotId`, `m_despawnCount`); `BotSession.cpp:24-34` `ResetForRespawn()` (yalnızca IOCP thread'i, slot iade edildikten sonra çağrılır).
  - `GameServer/ChatHandler.cpp:10-47` `CGameServerDlg::InitServerCommands()` komut tablosu (`:43` `warresult` son satır, `:44` `};`); `:506-531` `ProcessServerCommand` (önek `/` = `SERVER_COMMAND_PREFIX`, `GameServer/ChatHandler.h:91`; komut adı küçük harfe çevrilir, **argümanlar çevrilmez**); `:1134-1145` `HandleCountCommand` (konsol komut işleyicisi örneği, `printf` kullanır); `ChatHandler.h:31-33` `COMMAND_HANDLER` makrosu ve `CommandArgs = std::list<std::string>`.
  - `GameServer/GameServerDlg.h:590-592` `COMMAND_HANDLER(...)` bildirimleri (`HandleWarResultCommand` son satır).
  - `GameServer/ConsoleInputThread.cpp:20-52`: konsol komutları **ayrı bir thread**'de (`ConsoleInputThread`) `g_pMain->HandleConsoleCommand` ile çalışır; bu yüzden işleyici `BotManager` durumuna dokunamaz.
  - `GameServer/Define.h:3` `CONF_GAME_SERVER "./GameServer.ini"`: sunucu dosyaları çalışma dizinine göre `./` ile açar; komut dosyası da `./BotCommands.txt`.

## 3. Kapsam

**Yapılacaklar**

- `BotManager`: thread-güvenli komut kuyruğu (`EnqueueCommand`), komut dosyası yoklaması, komut yürütücüsü (`spawn`, `despawn`, `list`), `Tick()` içinden çağrı.
- `ChatHandler.cpp` + `GameServerDlg.h`: `/bot` konsol komutu (tabloya satır + ince işleyici).

**Kapsam dışı (yapılmayacak)**

- Oyun içi GM komutu `+bot` (`CUser::InitChatCommands`), `/bot enable|disable|scenario|start|stop`, `+bot list|why|pause|policy|verbose|testtp` (hepsi F3+). Yalnızca **sunucu konsolu** `/bot` ve dosya.
- Yeni ini anahtarı, yeni sabit bot, `BOT_TABLE` değişikliği, `BotSession`/`IBotSink`/`User.*`/`SocketMgr`/`KOSocketMgr` değişikliği, `ParseSpawnList`/`TickSessions`/`PollDespawn`/`StartSession`/`BeginDespawn` gövdelerinin "iyileştirilmesi" ve özet (`spawn complete`, `despawn complete`) mantığının değiştirilmesi.
- Komutlarla spawn edilen botlar için özet satırlarının doğruluğunu sağlamak: `spawn complete: N/M` özeti `m_sessions.size()` ile bir kez yazılır, komutla gelen sonraki botlar için tekrar yazılmaz; bu **kabul edilen** davranıştır (komutların kendi logları vardır).
- `PHASE_FAILED` / `PHASE_DESPAWN_STUCK` oturumlarını kurtarmak, `PHASE_WAIT_SELECT`/`PHASE_WAIT_LOADED` fazındaki botu `despawn` etmek (bu fazlardaki bot "ignored (phase ...)" ile atlanır).
- `RESPAWN_CYCLES != 0` iken komutları çalıştırmak (reddedilir, §5.6).
- Sunucuyu çalıştırmak, `GameServer.ini` düzenlemek, DB'ye bağlanmak, konsola/dosyaya komut vermek (çalışma zamanı doğrulaması Claude'da, §7 sonu).
- `docs/**` dosyalarını değiştirmek (Claude yapar), `AIServer`, `LogInServer`, `shared/**`, SQL.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/BotManager.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotManager.cpp` | değiştir | ASCII, CRLF |
| `GameServer/ChatHandler.cpp` | değiştir | **UTF-8 BOM + CRLF**: BOM ve satır sonları aynen korunmalı; yalnızca §5.8'deki üç ekleme (include, tablo satırı, işleyici) |
| `GameServer/GameServerDlg.h` | değiştir | UTF-8, CRLF; tek satır ekleme |

vcxproj/filters **değişmez** (yeni dosya yok). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. **Dal:** `git switch -c bot/F2-06 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `stop` (exe kilidi).

2. **`GameServer/Bot/BotManager.h`:**
   - Include listesine `<mutex>` ekle (alfabetik: `<chrono>` ile `<string>` arasına).
   - `public:` bölümünde `Shutdown()`'ın altına ekle:
     ```cpp
     	// Any thread. Queues one command line ("spawn <names>", "despawn <name|all>", "list") for the
     	// IOCP thread. Returns false when the bot system is disabled or the queue is full (64 lines).
     	bool EnqueueCommand(const std::string & line);
     ```
   - `private:` bölümüne (`Tick()` bildiriminin altına) ekle:
     ```cpp
     	// Runtime commands (ADR-0015). All of these run on the IOCP thread, called from Tick().
     	void ProcessCommands();                       // polls ./BotCommands.txt, then drains m_commandQueue
     	void PollCommandFile(std::chrono::steady_clock::time_point now);
     	void ExecuteCommand(const std::string & line);
     	void CommandSpawn(const std::string & args);
     	void CommandDespawn(const std::string & args);
     	void CommandList();
     	BotSession * FindSession(const char * charName);
     ```
   - Veri üyeleri (mevcut `m_firstTickTime`'ın altına; kurucu başlatıcı listesine **dokunma**, varsayılan kurucular yeter):
     ```cpp
     	std::mutex m_commandLock;                // guards m_commandQueue only
     	std::vector<std::string> m_commandQueue; // filled by any thread, drained on the IOCP thread
     	std::chrono::steady_clock::time_point m_lastCommandPoll; // IOCP thread only
     ```

3. **`GameServer/Bot/BotManager.cpp`: sabitler ve yardımcılar.** `CYCLE_PROGRESS_EVERY` sabitinin (`:20`) altına:
   ```cpp
   static const char * COMMAND_FILE = "./BotCommands.txt";
   static const char * COMMAND_FILE_CLAIMED = "./BotCommands.processing";
   static const uint32 COMMAND_POLL_MS = 1000;
   static const size_t COMMAND_QUEUE_MAX = 64;
   static const size_t COMMAND_FILE_MAX_LINES = 64;
   ```
   `BOT_TABLE`'ın (`:46-53`) altına, hepsi `static`:
   - `const BotAccountEntry * FindBotEntry(const char * name)`: `BOT_TABLE`'da `_stricmp` ile arar, bulamazsa `nullptr` (`ParseSpawnList`'in arama döngüsünü **değiştirme**, kendi kopyanı yaz).
   - `const char * PhaseName(BotSession::Phase phase)`: `PHASE_QUEUED` → `"queued"`, `PHASE_WAIT_SELECT` → `"wait_select"`, `PHASE_WAIT_LOADED` → `"wait_loaded"`, `PHASE_IN_GAME` → `"in_game"`, `PHASE_FAILED` → `"failed"`, `PHASE_DESPAWN_WAIT` → `"despawn_wait"`, `PHASE_DESPAWNED` → `"despawned"`, `PHASE_DESPAWN_STUCK` → `"despawn_stuck"`, varsayılan `"?"`.
   - `std::string Trim(const std::string &)`: baştaki/sondaki `" \t\r\n"` kırpar.
   - `void SplitNames(const std::string & text, std::vector<std::string> & out)`: `,`, boşluk ve tab ile ayırır, boş parçaları atar.

4. **`EnqueueCommand`** (`Shutdown()`'ın altına):
   ```cpp
   bool BotManager::EnqueueCommand(const std::string & line)
   {
   	if (!m_enabled)
   		return false;

   	std::lock_guard<std::mutex> lock(m_commandLock);
   	if (m_commandQueue.size() >= COMMAND_QUEUE_MAX)
   		return false;

   	m_commandQueue.push_back(line);
   	return true;
   }
   ```

5. **`Tick()` ve `ProcessCommands`:** `Tick()` içinde `TickSessions();` satırından (`:300`) hemen önce `ProcessCommands();` ekle. Yeni fonksiyonlar:
   - `ProcessCommands()`: `now = steady_clock::now()`; `now - m_firstTickTime < SPAWN_START_DELAY_MS` ise **hiçbir şey yapma** (return; komutlar kuyrukta/dosyada bekler, AI sunucusu bağlansın diye). Sonra `PollCommandFile(now)`; ardından kilit altında `m_commandQueue`'yu yerel bir `std::vector<std::string>`'e `swap` ile boşalt, kilidi **bırak**, her satır için `ExecuteCommand(line)` çağır (yürütücü kilit tutmaz).
   - `PollCommandFile(now)`: `now - m_lastCommandPoll < COMMAND_POLL_MS` ise return; aksi halde `m_lastCommandPoll = now`. Sonra: `remove(COMMAND_FILE_CLAIMED)` (önceki çökmeden kalmış olabilir, hata yok sayılır); `rename(COMMAND_FILE, COMMAND_FILE_CLAIMED) != 0` ise return (dosya yok **veya** yazan süreç hâlâ açık tutuyor; bir sonraki yoklamada tekrar denenir; bu yol sessizdir, log yazma). `fopen(COMMAND_FILE_CLAIMED, "r")`; açılamazsa `WriteBotLog("BotManager: command file could not be read")` yaz ve return. `fgets` ile (tampon 256) satır satır oku: `Trim`, boş satırları ve `#` ile başlayanları atla, **en fazla `COMMAND_FILE_MAX_LINES` komut** çalıştır (fazlası için tek satır `BotManager: command file: more than 64 lines, rest ignored`), her biri için `ExecuteCommand(satır)`. Bitince `fclose`, `remove(COMMAND_FILE_CLAIMED)`. Çalıştırılan komut sayısı > 0 ise `BotManager: command file: %u command(s) executed` yaz.

6. **`ExecuteCommand(line)`:** (yalnızca IOCP thread'i)
   - `line`'ı `Trim`'le; boşsa return. İlk sözcüğü (ilk boşluğa kadar) `verb`, kalanını `Trim`'lenmiş `args` olarak ayır. Her çalıştırılan komut için önce `BotManager: cmd '<satır>'` yaz (`snprintf`, tampon 320, `%s` ile; satır en fazla 255 karakter olduğundan taşma olmaz, yine de `snprintf` kırpar).
   - `m_respawnCycles != 0` ise `BotManager: cmd rejected (RESPAWN_CYCLES is active)` yaz ve return (soak modunda elle müdahale yok, ADR-0015).
   - `_stricmp(verb, "spawn") == 0` → `CommandSpawn(args)`; `"despawn"` → `CommandDespawn(args)`; `"list"` → `CommandList()`; aksi halde `BotManager: cmd unknown command '<verb>' (spawn, despawn, list)`.

7. **Komut gövdeleri** (hepsi `WriteBotLog` ile yazar; yeni kodda `printf` **yok**):
   - `FindSession(charName)`: `m_sessions` içinde `_stricmp(m_charName.c_str(), charName) == 0` olanı döner, yoksa `nullptr`.
   - `CommandSpawn(args)`: `args` boşsa `BotManager: cmd spawn: no names given` ve return. `SplitNames` ile adları al; her ad için sayaçlar `queued`, `ignored`:
     - `FindBotEntry(ad) == nullptr` → `BotManager: cmd spawn: unknown bot name '%s' ignored`, `ignored++`.
     - `FindSession(entry->charName)` bulunursa: fazı `PHASE_DESPAWNED` ise `s->ResetForRespawn()` çağır, `BotManager: cmd spawn: %s queued again`, `queued++`; başka bir fazdaysa `BotManager: cmd spawn: %s ignored (phase %s)` (`PhaseName`), `ignored++`.
     - Oturum yoksa `m_sessions.push_back(new BotSession(entry->charName, entry->accountName))`, `BotManager: cmd spawn: %s queued`, `queued++`. (Aynı komutta aynı ad iki kez geçerse ikincisi `FindSession` ile bulunur ve `ignored (phase queued)` olur: ayrıca ele alma.)
     - Sonunda `BotManager: cmd spawn: %u queued, %u ignored`.
     Slot yoksa/başka sorunda oturum `StartSession` içinde `FailSession` ile düşer (mevcut davranış, **değiştirme**); havuz boyutu burada ayrıca kontrol edilmez.
   - `CommandDespawn(args)`: `args` boşsa `BotManager: cmd despawn: no names given`. `now = steady_clock::now()`. `args` (küçük harfe çevrilmiş kopyası) `"all"` ise tüm `m_sessions` için: `PHASE_IN_GAME` olana `BeginDespawn(s, now)`, `despawning++`; diğerleri `notInGame++` (log yok). Sonunda `BotManager: cmd despawn all: %u despawning, %u not in game`. Aksi halde adları `SplitNames` ile ayır; her ad için: oturum yoksa `BotManager: cmd despawn: %s ignored (no such session)` (bilinmeyen/hiç spawn edilmemiş); `PHASE_IN_GAME` ise `BeginDespawn(s, now)`; başka fazda `BotManager: cmd despawn: %s ignored (phase %s)`. (`BeginDespawn` kendi `despawning` logunu yazar.)
   - `CommandList()`: kilit altında `m_socketMgr.GetReservedSessionMap().size()` ile `poolFree` al (mevcut `TickSessions` desenini kopyala: `std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock());`). Önce `BotManager: cmd list: %u session(s), pool free %u/%u`, sonra her oturum için `BotManager: cmd list:   %s phase=%s slot=%s despawns=%u` (slot: `m_pUser != nullptr` ise `m_slotId`'yi sayı olarak, değilse `-`; sayıyı `char slot[16]` içine `snprintf` ile bas).

8. **`GameServer/ChatHandler.cpp`** (UTF-8 BOM + CRLF **koru**; yalnızca üç ekleme):
   - Dosya başı include'larına (`#include "../shared/DateTime.h"`'ın altına): `#include "Bot/BotManager.h"`.
   - `InitServerCommands()` tablosunda `warresult` satırının (`:43`) altına (tab hizası komşu satırlarla aynı):
     ```cpp
     		{ "bot",				&CGameServerDlg::HandleBotCommand,				"Bot test commands. Arguments: spawn <name>[,<name>...] | despawn <name>|all | list" },
     ```
   - `HandleCountCommand`'ın (`:1134-1145`) altına:
     ```cpp
     COMMAND_HANDLER(CGameServerDlg::HandleBotCommand)
     {
     	// Runs on the console thread: only queues the command; BotManager runs it on the IOCP thread.
     	if (!BotManager::Instance().isEnabled())
     	{
     		printf("Bot system is disabled ([BOT] ENABLED=0 in GameServer.ini)\n");
     		return true;
     	}

     	if (vargs.empty())
     	{
     		printf("Using Sample : /bot spawn BotWP_K,BotMF_K | /bot despawn all | /bot list\n");
     		return true;
     	}

     	std::string line;
     	for (const std::string & word : vargs)
     	{
     		if (!line.empty())
     			line += " ";
     		line += word;
     	}

     	if (BotManager::Instance().EnqueueCommand(line))
     		printf("Bot command queued; result in Logs/Bot_*.log\n");
     	else
     		printf("Bot command rejected (queue full)\n");

     	return true;
     }
     ```
     (`args` parametresini **kullanma**: `"/bot"` tek başına yazılınca `ProcessServerCommand` `args`'ı dizinin dışına işaret ettirir, bu mevcut bir tuhaflıktır; `vargs` güvenlidir.)

9. **`GameServer/GameServerDlg.h`:** `COMMAND_HANDLER(HandleWarResultCommand);` satırının (`:592`) altına `COMMAND_HANDLER(HandleBotCommand);` ekle (tab girintisi komşu satırlarla aynı).

10. **Derle:** `./tools/build.sh Release` ve `./tools/build.sh Debug`. `BotManager.cpp`, `ChatHandler.cpp` ve `GameServerDlg.cpp`'yi (`touch` ile) yeniden derlenmeye zorla ki uyarı listesi tam çıksın; `Bot\` ve eklenen satırlarda uyarı olmamalı. Sunucuyu çalıştırma.

11. **Raporu yaz** (şablon bölümü) ve commit et: `[F2-06] ...`. `git add` ile yalnızca §4'teki dosyaları ve bu planı ekle.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; **yeni uyarı yok** (`Bot\` dosyalarında ve `ChatHandler.cpp`/`GameServerDlg.cpp`'nin eklenen satırlarında uyarı olmamalı; çıktıyı yapıştır).
- [ ] K2: `./tools/build.sh Debug` hatasız biter.
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F2-06` yalnızca §4'teki 4 dosyayı (+ bu plan dosyası) içerir; `ChatHandler.cpp` fark satırı yalnızca eklemedir (`+N/−0`, N ≈ 35) ve BOM/CRLF bozulmamıştır (çıktıyı yapıştır); `GameServerDlg.h` `+1/−0`.
- [ ] K4: Thread kuralı: `m_sessions`, `BotSession` alanları, `BeginDespawn`, `ResetForRespawn`, `ExecuteCommand`, `CommandSpawn/Despawn/List`, `PollCommandFile` yalnızca `ProcessCommands`→`Tick()` zincirinden çağrılır; konsol thread'inden çağrılan **tek** `BotManager` üyesi `EnqueueCommand` (ve `isEnabled()`) olmalı (`grep -n "EnqueueCommand\|ExecuteCommand\|PollCommandFile\|ProcessCommands\|CommandSpawn\|CommandDespawn\|CommandList\|FindSession" GameServer/Bot/*.cpp GameServer/Bot/*.h GameServer/ChatHandler.cpp` çıktısını satır satır açıklayarak yapıştır).
- [ ] K5: `m_commandLock` yalnızca `EnqueueCommand` içinde ve `ProcessCommands`'ın `swap` bloğunda tutulur; `ExecuteCommand` çağrılırken **kilit tutulmaz** (`sed -n` çıktısı, satır numarasıyla).
- [ ] K6: `ProcessCommands` ilk tick'ten itibaren `SPAWN_START_DELAY_MS` dolmadan hiçbir komut çalıştırmaz ve dosyaya dokunmaz (`sed -n`); komut dosyası yoklaması `COMMAND_POLL_MS` ile sınırlıdır; dosya önce `rename` ile alınır, sonra okunur, sonra silinir (`sed -n`).
- [ ] K7: `ExecuteCommand` başında `m_respawnCycles != 0` reddi vardır; `spawn` yalnızca `PHASE_DESPAWNED` oturumu yeniden kuyruğa alır (`PHASE_FAILED`/`PHASE_DESPAWN_STUCK`/aktif fazlar `ignored (phase ...)`); `despawn` yalnızca `PHASE_IN_GAME` botu `BeginDespawn`'a verir (`sed -n` çıktısı).
- [ ] K8: Yeni log metinleri §5.5-§5.7'dekiyle **birebir** (`grep -n "cmd \|command file" GameServer/Bot/BotManager.cpp` çıktısı); yeni kodda `printf` yok (`ChatHandler.cpp`'deki konsol işleyicisi hariç; orada yalnızca üç `printf`).
- [ ] K9: `ParseSpawnList`, `TickSessions`, `PollDespawn`, `StartSession`, `FailSession`, `BeginDespawn`, `Startup`, `BotSession.*` gövdeleri **değişmemiştir** (`git diff gece/2026-10-02...bot/F2-06 -- GameServer/Bot/BotManager.cpp` içinde bu fonksiyonların satırı yok; çıktının ilgili kısmını yapıştır).
- [ ] K10: Kapalıyken davranış değişmez: `ENABLED=0` iken `EnqueueCommand` `false` döner, `Tick()` hiç çalışmaz (zamanlayıcı yok), komut dosyası hiç aranmaz; `/bot` konsol işleyicisi yalnızca `printf` yazar (kod incelemesi, `sed -n`).
- [ ] K11: Kodlama: `file GameServer/Bot/*` hepsi "ASCII text, with CRLF line terminators"; `file GameServer/ChatHandler.cpp` hâlâ "UTF-8 (with BOM) ... CRLF"; `file GameServer/GameServerDlg.h` "UTF-8 ... CRLF" (çıktıyı yapıştır); girinti tab, Allman, yorumlar İngilizce.
- [ ] K12: `git status --short` boş (yalnızca izinli dosyalar commit'li). Sunucu çalıştırılmadı, `GameServer.ini`, `BotCommands.txt` ve veritabanı oluşturulmadı/değiştirilmedi.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
git diff --stat gece/2026-10-02...bot/F2-06
grep -n "EnqueueCommand\|ExecuteCommand\|PollCommandFile\|ProcessCommands\|CommandSpawn\|CommandDespawn\|CommandList\|FindSession\|m_commandLock" GameServer/Bot/*.cpp GameServer/Bot/*.h GameServer/ChatHandler.cpp
grep -n "cmd \|command file" GameServer/Bot/BotManager.cpp
file GameServer/Bot/* GameServer/ChatHandler.cpp GameServer/GameServerDlg.h
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile (her senaryodan sonra ini yedekten geri yüklenir, `BotCommands.txt` silinir). Dosya GameServer çalışma dizinine (`GameServer.ini`'nin yanına) yazılır:
1. **Komut dosyası, spawn/list:** `ENABLED=1, MAX_BOTS=16`, `SPAWN_ON_START` boş, `DESPAWN_AFTER_SEC=0`. Sunucu açıldıktan ≥ 10 sn sonra `BotCommands.txt` = `spawn BotWP_K,botmf_k` + `spawn Foo` + `list`. Beklenen (≤ ~10 sn): dosya kaybolur (ve `.processing` kalmaz); `cmd spawn: BotWP_K queued`, `BotMF_K queued`, `unknown bot name 'Foo' ignored`, `cmd spawn: 2 queued, 1 ignored`; iki `in game`, `spawn complete: 2/2 in game, 0 failed`; `cmd list: 2 session(s) ...`.
2. **despawn:** `despawn BotWP_K` → `despawning` + `despawned (logout save ...)`, `names cleared yes`; `despawn all` → `cmd despawn all: 1 despawning, 1 not in game`, ikinci `despawned`; tekrar `despawn all` → `0 despawning, 2 not in game`; `list` → `pool free 16/16`, iki oturum `despawned`.
3. **Yeniden spawn:** `spawn BotWP_K` → `queued again` → yeniden `in game` (slot yeniden kullanımı); hemen ardından `spawn BotWP_K` → `ignored (phase in_game)`; `despawn Foo` → `ignored (no such session)`; `despawn BotMF_K` (zaten çıkmış) → `ignored (phase despawned)`.
4. **Reddetme:** `SPAWN_ON_START=BotWP_K, DESPAWN_AFTER_SEC=2, RESPAWN_CYCLES=2` ile açıp dosyaya `spawn BotMF_K` → `cmd rejected (RESPAWN_CYCLES is active)`, bot spawn olmaz; döngü kendi `respawn cycles done` satırıyla bitmeye devam eder.
5. **Gerilemesiz (F2-05):** `SPAWN_ON_START=BotWP_K,BotMF_K,BotWP_E,BotPHD_E, DESPAWN_AFTER_SEC=2, RESPAWN_CYCLES=4` ve dosya yok → `respawn cycles done: 20 spawns, 20 despawns, 0 failed, 0 stuck, 0 names left, pool free 16/16`.
6. **`ENABLED=0`:** `BotCommands.txt` yazılır → 30 sn sonra dosya **yerinde**, `Bot_*.log`'da yeni satır yok; ini'de yeni anahtar yok.
7. Sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Konsol `/bot` yolu otomatikleştirilemez: insan testi olarak `docs/STATUS.md`'deki "Proje sahibi testleri" listesine eklenir (T-ARCH-04).

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` dosyaları ASCII; kod yorumları İngilizce. `ChatHandler.cpp` BOM'lu: editörün BOM'u silmediğini ve satır sonlarını LF'e çevirmediğini `git diff --stat` ve `file` ile kontrol et.
- **Bot sistemi varsayılan kapalı.** Yeni komut yüzeyi yalnızca `ENABLED=1` iken etkindir. Üretim sunucusunda `ENABLED=1` kullanılmaz; komut dosyası kanalı bu yüzden kabul edilebilir (ADR-0015).
- **Thread kuralı (ADR-0005):** konsol thread'i yalnızca `EnqueueCommand` çağırır. `m_enabled` düz `bool`'dur; yalnızca `Startup()`'ta bir kez yazılır, konsol thread'i sonradan okur (kabul edilen, zararsız yarış; atomik yapma, kapsam dışı).
- **Konsol thread'i `g_pMain`'den önce başlar** (`GameServer/main.cpp:26` ve `:28`); açılıştan önce yazılan `/bot` `ProcessServerCommand`'ın mevcut davranışına tabidir, bu planın konusu değil. İşleyici `isEnabled()` ile (Startup öncesinde `false`) korunur.
- **Slot ve oturum ömrü kuralları değişmez** (F2-04/F2-05): `PHASE_FAILED`/`PHASE_DESPAWN_STUCK` oturum süreç sonuna kadar kalır; komutla bile kurtarılmaz. Oturumlar silinmez; en çok 12 oturum (12 sabit ad) vardır.
- **Özetler:** `spawn complete` özeti süreç başına bir kez yazılır (`m_spawnSummaryDone`); komutla spawn edilen botların kendi `cmd ...` ve `in game` satırları vardır. Bunu "düzeltmeye" çalışma.
- **Dosya yarışı:** `rename` başarısız olursa (dosya henüz yazılıyorsa) sessizce bir sonraki yoklamaya bırakılır; yarım dosya okunmaz. Dosya sunucu tarafından silinir; yazan taraf yeniden yazabilir.
- Log hacmi: komut başına birkaç satır; `Bot_*.log` günlük dosyadır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F2-06` — `<kısa-sha> [F2-06] …`
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
- İncelenen: `gece/2026-10-02...bot/F2-06` @ `<sha>`
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
