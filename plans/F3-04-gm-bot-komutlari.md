# F3-04: Oyun içi GM komutu `+bot` (komut çekirdeğine ikinci giriş yolu, anlık `list` yanıtı)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F3 — Telemetri ve test altyapısı (`docs/17` §2) |
| Branch | `bot/F3-04` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F2-06 (`KAPANDI`: komut çekirdeği, `EnqueueCommand`), F3-02 (`KAPANDI`: `match`), F3-03 (`KAPANDI`: `scenario`) |
| İlgili gereksinim / kabul | `docs/17` §2 F3 kapsamı "`+bot`/`/bot` komutları", S11 (`docs/02` §11), `docs/13` §10, ADR-0015 Eki (F3-04) |
| Tahmini büyüklük | S (4 dosya, ~120 satır) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

F3-03 sonunda bot komutları yalnızca sunucu konsolundan (`/bot ...`) ve `BotCommands.txt` dosyasından verilebiliyor. Bu plan sonunda, `[BOT] ENABLED=1` iken, oyun içinde **GM yetkili** bir oyuncu sohbet satırına `+bot ...` yazarak aynı komutları verebilir:

| GM komutu | Davranış |
|---|---|
| `+bot list` | **Hemen** GM'e özel yanıt: oturum/faz/slot özeti (1 sn eskiliğinde anlık görüntüden; kuyruğa girmez) |
| `+bot spawn ...`, `+bot despawn ...`, `+bot match ...`, `+bot scenario ...` | Satır olduğu gibi komut kuyruğuna girer (`EnqueueCommand`); GM'e "kuyruğa alındı, sonuç `Logs/Bot_*.log`'da" der. Yürütme ve doğrulama F2-06/F3-02/F3-03'tekiyle aynıdır |
| `+bot` (argümansız) | Kullanım örneği (iki satır) |

`ENABLED=0` (varsayılan) iken `+bot` yalnızca "bot sistemi kapalı" yanıtı verir; sunucu davranışı F3-03'tekiyle aynıdır. `docs/13` §10'daki `+bot why|pause|resume|policy|verbose|testtp` bu planın **kapsamı dışındadır** (karar motoru, politika ve duraklatma henüz yok; §3).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0015-bot-calisma-zamani-komut-kanali.md` (ve Ekleri): neden tek komut çekirdeği, neden kuyruk. Bu plan ADR'ye Ek (F3-04) olarak işlenmiştir (Claude yazdı).
- `docs/adr/ADR-0005-bot-tick-thread-modeli.md`: bot durumu yalnızca bot tick'inde (`Tick()`) değişir; `CUser::Chat` farklı bir IOCP worker thread'inde çalışabilir, bu yüzden `+bot` işleyicisi `BotManager` durumuna **dokunamaz**; yalnızca `EnqueueCommand` ve yeni `GetStatusSnapshot` (kilitli kopya) çağırır.
- `plans/F2-06-bot-calisma-zamani-komutlari.md` §5-§7: komut çekirdeğinin davranışı (değişmez).
- İlgili kod (dosya:satır, 2026-10-02'de depoda doğrulandı; ISO-8859 değil, `grep` yeter):
  - `GameServer/ChatHandler.cpp:104-117` `CUser::Chat`: `if (isGM() && ProcessChatCommand(chatstr))` (`:111`) GM komutunu çalıştırır ve sohbet günlüğüne (`WriteChatLogFile`) yazar, yani **GM komutları zaten denetim kaydına düşer**. `:346-374` `ProcessChatCommand`: `+` öneki (`CHAT_COMMAND_PREFIX`, `GameServer/ChatHandler.h:90`), komut adı küçük harfe çevrilir, **argümanlar çevrilmez**, tabloda bulunamazsa `false`.
  - `ChatHandler.cpp:50-86` `CUser::InitChatCommands()` tablosu: son satır `:83` `resetranking`, `:84` `};`. İşleyici imzası `COMMAND_HANDLER` makrosudur (`ChatHandler.h:31`: `bool name(CommandArgs & vargs, const char *args, const char *description)`).
  - `ChatHandler.cpp:1004-1011` `CGameServerDlg::SendHelpDescription(CUser *, std::string)`: GM'e özel (`PUBLIC_CHAT`) tek satır mesaj gönderir; mevcut tüm `+` komutları yanıt için bunu kullanır (örn. `:391`).
  - `ChatHandler.cpp:1150-1179` `CGameServerDlg::HandleBotCommand` (konsol `/bot`; `:1179` `}`, `:1180` boş, `:1181` `SendFormattedResource`): bu plan **buna dokunmaz**, yeni işleyici hemen altına eklenir.
  - `GameServer/User.h:737` `COMMAND_HANDLER(HandleResetPlayerRankingCommand);` (son `COMMAND_HANDLER`); `User.h:306` `isGM()`.
  - `GameServer/Bot/BotManager.h:40-42` `EnqueueCommand` (herhangi bir thread); `:59-68` komut yardımcıları; `:99-103` komut kuyruğu üyeleri; `:113` `m_scenario`.
  - `GameServer/Bot/BotManager.cpp:346-390` `Tick()`: `:383` `ProcessCommands();`, `:384` `TickSessions();`, `:385` `m_scenario.Tick(...)`. `:727-755` `CommandList()` (log biçimi: başlık `BotManager: cmd list: %u session(s), pool free %u/%u`, satır `BotManager: cmd list:   %s phase=%s slot=%s despawns=%u`). `:49-75` `BOT_TABLE`/`FindBotEntry`; `PhaseName`, `Trim`, `SplitNames` aynı dosyanın üst bölümünde `static` yardımcılardır (satırları kendin bul: `grep -n "PhaseName\|static std::string Trim\|SplitNames" GameServer/Bot/BotManager.cpp`).
  - Satır numaraları bu planın yazıldığı `gece/2026-10-02` @ `13526d9`'a aittir; uyuşmazsa fonksiyon adına göre bul ve raporda belirt.

## 3. Kapsam

**Yapılacaklar**

- `BotManager`: `list` biçimlendirmesini tek yardımcıya (`BuildStatusLines`) taşı, 1 sn'de bir güncellenen kilitli anlık görüntü (`m_statusLines`) ve herhangi bir thread'den okunan `GetStatusSnapshot` ekle. `CommandList` aynı yardımcıyı kullanır (log metni **birebir aynı** kalır).
- `ChatHandler.cpp` + `User.h`: GM `+bot` komutu (tabloya satır + işleyici + bildirim).

**Kapsam dışı (yapılmayacak)**

- `+bot why`, `pause`, `resume`, `policy`, `verbose`, `testtp` (karar motoru F6+, politika F9, test teleportu ve duraklatma gerektirir; `docs/13` §10 satırları açık kalır). `/bot enable|disable`.
- `list` çıktısına profil/politika sütunu eklemek (profil/politika henüz yok), `scenario status` çıktısını GM'e iletmek, diğer komutların sonucunu GM'e geri yollamak (sonuç yalnızca `Bot_*.log`'da; GM'e yanıt yolu başka plan).
- Yeni ini anahtarı, yeni log dosyası/telemetri olayı, `BOT_TABLE`/`BotSession`/`ScenarioRunner`/`Telemetry`/`User.cpp`/`SocketMgr` değişikliği, `ExecuteCommand` ve diğer `Command*` gövdelerinin "iyileştirilmesi", konsol `HandleBotCommand` değişikliği.
- Bot hesaplarına GM yetkisi vermek, oyun içi istemciyle test etmek (insan testi T-ARCH-05, `docs/STATUS.md`), sunucuyu çalıştırmak ve `GameServer.ini`/DB değiştirmek dışında DeepSeek'in yapacağı bir çalışma zamanı testi yoktur (§7 sonu).
- `docs/**` dosyalarını değiştirmek (Claude yapar), `AIServer`, `LogInServer`, `shared/**`, SQL, vcxproj/filters (yeni dosya yok).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/BotManager.h` | değiştir | ASCII, CRLF |
| `GameServer/Bot/BotManager.cpp` | değiştir | ASCII, CRLF |
| `GameServer/ChatHandler.cpp` | değiştir | **UTF-8 BOM + CRLF**: BOM ve satır sonları aynen korunmalı; yalnızca §5.6'daki iki ekleme (tablo satırı, işleyici) |
| `GameServer/User.h` | değiştir | ASCII, CRLF; tek satır ekleme |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. `User.h` değişince çok sayıda `.cpp` yeniden derlenir (derleme birkaç dakika sürer, normal).

## 5. Uygulama adımları

1. **Dal:** `git switch -c bot/F3-04 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `stop` (exe kilidi).

2. **`GameServer/Bot/BotManager.h`:**
   - `public:` bölümünde `EnqueueCommand`'ın altına:
     ```cpp
     	// Any thread. Copies the last status snapshot (refreshed by Tick() about once a second) into 'out':
     	// first the header line, then one line per session. Returns false when the bot system is
     	// disabled. 'out' is empty until the first refresh (about one tick after the AI start delay).
     	bool GetStatusSnapshot(std::vector<std::string> & out);
     ```
   - `private:` bölümünde `CommandList();` bildiriminin altına:
     ```cpp
     	void BuildStatusLines(std::vector<std::string> & out);                         // IOCP thread only
     	void RefreshStatusSnapshot(std::chrono::steady_clock::time_point now);         // IOCP thread only
     ```
   - Veri üyeleri (`m_lastCommandPoll`'un altına; kurucu başlatıcı listesine **dokunma**):
     ```cpp
     	std::mutex m_statusLock;                  // guards m_statusLines only
     	std::vector<std::string> m_statusLines;   // written on the IOCP thread, copied by any thread
     	std::chrono::steady_clock::time_point m_lastStatusRefresh; // IOCP thread only
     ```

3. **`GameServer/Bot/BotManager.cpp`: `BuildStatusLines` ve `CommandList`.**
   - Sabit (`COMMAND_FILE_MAX_LINES` sabitinin altına): `static const uint32 STATUS_REFRESH_MS = 1000;`
   - `BuildStatusLines(out)`: `out.clear()`; `CommandList()`'in **şimdiki** gövdesindeki mantığı kullanır (aynı `poolFree` hesabı: `std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock());` + `GetReservedSessionMap().size()`; kilit bloğu fonksiyon başında, `out`'a yazmadan önce kapanır). `out`'a önce başlık: `"%u session(s), pool free %u/%u"` (`m_sessions.size()`, `poolFree`, `m_poolSize`), sonra her oturum için: `"  %s phase=%s slot=%s despawns=%u"` (iki baştaki boşluk dahil; ad `m_charName`, faz `PhaseName(s->m_phase)`, slot: `m_pUser != nullptr` ise `m_slotId` sayı, değilse `-`, `despawns` = `m_despawnCount`). `snprintf` + `char[192]`.
   - `CommandList()` yeniden yazılır: `BuildStatusLines(lines)`; her satır için `WriteBotLog(("BotManager: cmd list: " + lines[i]).c_str())`. Böylece başlık `BotManager: cmd list: 2 session(s), pool free 14/16`, oturum satırı `BotManager: cmd list:   BotWP_K phase=in_game slot=2984 despawns=0` olur: **bugünkü çıktıyla bayt bayt aynı** (F2-06 doğrulamasındaki satırlar). Bu davranış değişmezliği kabul kriteridir (K5).
   - `RefreshStatusSnapshot(now)`: `now - m_lastStatusRefresh < STATUS_REFRESH_MS` ise return; `m_lastStatusRefresh = now`; yerel `std::vector<std::string> lines`; `BuildStatusLines(lines)`; sonra `{ std::lock_guard<std::mutex> lock(m_statusLock); m_statusLines.swap(lines); }` (kilit yalnızca bu blokta, biçimlendirme **kilit dışında**).
   - `GetStatusSnapshot(out)`: `if (!m_enabled) return false;` `{ std::lock_guard<std::mutex> lock(m_statusLock); out = m_statusLines; }` `return true;`.
   - `Tick()` içinde `TickSessions();` satırından hemen sonra: `RefreshStatusSnapshot(std::chrono::steady_clock::now());` (`m_scenario.Tick` ve telemetri satırlarının yerini **değiştirme**).

4. **Kilit kuralı:** `m_statusLock` altında başka kilit alınmaz (özellikle `GetLock()` recursive mutex'i ve `m_commandLock`/`Telemetry` kilitleri **alınmaz**); `BuildStatusLines` zaten kilit dışında çalışır. `m_commandLock` mantığına dokunma.

5. **`GameServer/User.h`:** `COMMAND_HANDLER(HandleResetPlayerRankingCommand);` satırının (`:737`) altına `COMMAND_HANDLER(HandleBotCommand);` ekle (tab girintisi komşu satırlarla aynı).

6. **`GameServer/ChatHandler.cpp`** (UTF-8 BOM + CRLF **koru**; yalnızca iki ekleme; `#include "Bot/BotManager.h"` zaten var):
   - `CUser::InitChatCommands()` tablosunda `resetranking` satırının (`:83`) altına (tab hizası komşu satırlarla aynı):
     ```cpp
     		{ "bot",				&CUser::HandleBotCommand,						"Bot test commands. Arguments: spawn <name>[,<name>...] | despawn <name>|all | list | match start|end ... | scenario run|stop|status ..." },
     ```
   - `CGameServerDlg::HandleBotCommand`'ın bittiği `}`'den (`:1179`) sonra, `SendFormattedResource`'tan önce:
     ```cpp
     COMMAND_HANDLER(CUser::HandleBotCommand)
     {
     	// Runs on an IOCP worker thread: never touches BotManager state, only queues the command
     	// or copies the status snapshot (ADR-0005, ADR-0015 F3-04 addendum).
     	if (!isGM())
     		return false;

     	if (!BotManager::Instance().isEnabled())
     	{
     		g_pMain->SendHelpDescription(this, "Bot system is disabled ([BOT] ENABLED=0 in GameServer.ini)");
     		return true;
     	}

     	if (vargs.empty())
     	{
     		g_pMain->SendHelpDescription(this, "Using Sample : +bot spawn BotWP_K,BotMF_K | +bot despawn all | +bot list");
     		g_pMain->SendHelpDescription(this, "Also : +bot match start <scenario> [seed] | +bot match end [result] | +bot scenario run <name>|stop|status");
     		return true;
     	}

     	// "+bot list" is answered from the status snapshot (up to ~1 s old) and is not queued.
     	if (vargs.size() == 1 && _stricmp(vargs.front().c_str(), "list") == 0)
     	{
     		std::vector<std::string> lines;
     		BotManager::Instance().GetStatusSnapshot(lines);
     		if (lines.empty())
     		{
     			g_pMain->SendHelpDescription(this, "Bot status not available yet, try again in a second");
     			return true;
     		}

     		g_pMain->SendHelpDescription(this, "Bots: " + lines[0]);
     		for (size_t i = 1; i < lines.size(); i++)
     			g_pMain->SendHelpDescription(this, lines[i]);
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
     		g_pMain->SendHelpDescription(this, "Bot command queued; result in Logs/Bot_*.log");
     	else
     		g_pMain->SendHelpDescription(this, "Bot command rejected (queue full)");

     	return true;
     }
     ```
     Notlar: `StrSplit` ardışık boşluklarda boş parça üretebilir (uygulayıcı bunu denemeden varsayma; `shared/` altında `grep -n "StrSplit"` ile bakabilir); işleyici `vargs`'ı olduğu gibi birleştirir, **komut çekirdeği** (`ExecuteCommand` → `Trim`/`SplitNames`) fazla boşlukları zaten tolere eder. Boş parçaları ayıklamak kapsam dışıdır.

7. **Derle:** `./tools/build.sh Release` ve `./tools/build.sh Debug`. `BotManager.cpp` ve `ChatHandler.cpp`'yi (`touch` ile) yeniden derlenmeye zorla ki uyarı listesi tam çıksın; `Bot\` ve eklenen satırlarda uyarı olmamalı. Sunucuyu çalıştırma.

8. **Raporu yaz** (şablon bölümü) ve commit et: `[F3-04] ...`. `git add` ile yalnızca §4'teki dosyaları ve bu planı ekle.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; **yeni uyarı yok** (`Bot\` dosyalarında ve `ChatHandler.cpp`/`User.h`'den gelen eklenen satırlarda uyarı olmamalı; çıktıyı yapıştır).
- [ ] K2: `./tools/build.sh Debug` hatasız biter.
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F3-04` yalnızca §4'teki 4 dosyayı (+ bu plan dosyası) içerir; `User.h` `+1/−0`; `ChatHandler.cpp` fark satırı yalnızca eklemedir (`+N/−0`, N ≈ 50) ve BOM/CRLF bozulmamıştır (çıktıyı yapıştır).
- [ ] K4: Thread kuralı: `+bot` işleyicisi `BotManager`'dan yalnızca `isEnabled()`, `EnqueueCommand` ve `GetStatusSnapshot` çağırır (`sed -n` + `grep -n "BotManager::Instance()" GameServer/ChatHandler.cpp` çıktısını açıklayarak yapıştır); `m_statusLines` yalnızca `m_statusLock` altında okunur/yazılır (`grep -n "m_statusLines\|m_statusLock" GameServer/Bot/*`, satır satır açıkla); `BuildStatusLines` kilit dışında çağrılır ve `m_statusLock` bloğu içinde başka kilit yok.
- [ ] K5: `CommandList` log metni değişmedi: önce/sonra aynı olduğunu göster. `git diff gece/2026-10-02...bot/F3-04 -- GameServer/Bot/BotManager.cpp` içinde `CommandList` gövdesinin eski satırları (`-`) ve yeni gövde yan yana; `BuildStatusLines` başlık biçimi `"%u session(s), pool free %u/%u"` ve satır biçimi `"  %s phase=%s slot=%s despawns=%u"` (baştaki iki boşluk), `CommandList` önekinin `"BotManager: cmd list: "` olduğunu `grep -n "cmd list" GameServer/Bot/BotManager.cpp` ile göster.
- [ ] K6: `Tick()` değişikliği yalnızca `TickSessions();` sonrasına tek satır `RefreshStatusSnapshot(...)` eklemektir (`git diff` kesiti); `RefreshStatusSnapshot` `STATUS_REFRESH_MS` (1000) ile sınırlıdır; `ENABLED=0` iken `Tick` çalışmadığı için hiçbir şey olmaz (zamanlayıcı kurulmaz; `GetStatusSnapshot` `false` döner).
- [ ] K7: `+bot` işleyicisi: `!isGM()` → `return false`; `!isEnabled()` → tek yanıt, kuyruğa/anlık görüntüye dokunmaz; `list` (tek argüman, büyük-küçük harf duyarsız) kuyruğa **girmez**, anlık görüntüyü yanıtlar; diğer her şey `EnqueueCommand`'a satır olarak gider (`sed -n` çıktısı).
- [ ] K8: Konsol `/bot` işleyicisi (`CGameServerDlg::HandleBotCommand`) ve `ExecuteCommand`/`CommandSpawn`/`CommandDespawn`/`CommandMatch`/`PollCommandFile`/`ProcessCommands`/`TickSessions`/`PollDespawn`/`StartSession`/`FailSession`/`BeginDespawn`/`Startup`/`EnqueueCommand` gövdeleri, `BotSession.*`, `ScenarioRunner.*`, `Telemetry.*` **değişmemiştir** (`git diff` içinde bu fonksiyonların satırı yok; yalnızca `CommandList` yeniden yazılmıştır).
- [ ] K9: Yeni kodda `printf` yok; yeni log satırı yok (`+bot` yeni `WriteBotLog` yazmaz; GM komutu zaten sohbet günlüğüne düşer).
- [ ] K10: Kodlama: `file GameServer/Bot/*` hepsi "ASCII text, with CRLF line terminators"; `file GameServer/ChatHandler.cpp` hâlâ "UTF-8 (with BOM) ... CRLF"; `file GameServer/User.h` "ASCII ... CRLF" (çıktıyı yapıştır); girinti tab, Allman, yorumlar İngilizce.
- [ ] K11: Kapalıyken davranış değişmez: `ENABLED=0` iken `m_statusLines` hiç yazılmaz, `+bot` yalnızca "disabled" yanıtı verir (kod incelemesi, `sed -n`).
- [ ] K12: `git status --short` boş (yalnızca izinli dosyalar commit'li). Sunucu çalıştırılmadı, `GameServer.ini`, `BotCommands.txt` ve veritabanı oluşturulmadı/değiştirilmedi.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
git diff --stat gece/2026-10-02...bot/F3-04
git diff gece/2026-10-02...bot/F3-04 -- GameServer/Bot/BotManager.cpp
grep -n "BotManager::Instance()" GameServer/ChatHandler.cpp
grep -n "m_statusLines\|m_statusLock\|RefreshStatusSnapshot\|BuildStatusLines\|GetStatusSnapshot" GameServer/Bot/*.cpp GameServer/Bot/*.h GameServer/ChatHandler.cpp
grep -n "cmd list" GameServer/Bot/BotManager.cpp
file GameServer/Bot/* GameServer/ChatHandler.cpp GameServer/User.h
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile (her senaryodan sonra ini yedekten geri yüklenir, `BotCommands.txt` silinir).
1. **`list` gerilemesizliği (asıl kanıt):** `ENABLED=1, MAX_BOTS=16`, `BotCommands.txt` = `spawn BotWP_K,BotMF_K` + `list`, sonra tekrar `list` → `Bot_*.log` satırları F2-06 doğrulamasındakiyle **aynı biçimde** (`BotManager: cmd list: 2 session(s), pool free 14/16`, `BotManager: cmd list:   BotWP_K phase=in_game slot=2984 despawns=0`). Bu, `+bot list`'in kullandığı biçimlendirmeyi (`BuildStatusLines`) çalışma zamanında sınar.
2. F2-05 gerilemesiz (`RESPAWN_CYCLES=4`, 4 bot → `20 spawns, 20 despawns, 0 failed`), F3-03 `scenario run smoke` gerilemesiz (`finished: 2/2`), `ENABLED=0` (log yok, `BotCommands.txt` yerinde).
3. Sunucu 3/3 UP, `GameServer.log`'a yeni hata yok, `Tick` p95 (`PERF_SAMPLE`) F3-01 değerleriyle aynı mertebede (anlık görüntü 1 Hz).
4. `+bot` oyun içi yolu (GM istemcisi) otomatikleştirilemez: insan testi T-ARCH-05 (`docs/STATUS.md` "Proje sahibi testleri"); kriteri engellemez.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `User.h` ASCII; kod yorumları İngilizce. `ChatHandler.cpp` BOM'lu: editörün BOM'u silmediğini ve satır sonlarını LF'e çevirmediğini `git diff --stat` ve `file` ile kontrol et. Oyuna giden metinler ASCII İngilizce (istemci sohbet satırı).
- **Bot sistemi varsayılan kapalı.** `+bot` yalnızca `ENABLED=1` iken etkindir; üretim sunucusunda kullanılmaz (ADR-0015).
- **Thread kuralı (ADR-0005):** `+bot` işleyicisi bot tick'inden farklı bir IOCP worker thread'inde çalışabilir; `BotManager` durumunu (özellikle `m_sessions`, `BotSession` alanları) **okumaz**. Anlık görüntü kopyası bunun içindir; 1 sn eskiliği kabul edilmiştir.
- `GetStatusSnapshot`'ın `false`'u (kapalı) ile boş `out`'u (henüz yenilenmedi) ayrı durumlardır; işleyici `isEnabled()` ile kapalıyı önceden ayıklar.
- `m_enabled` düz `bool`'dur (F2-06 §8'deki aynı kabul).
- `list` yanıtında oturum sayısı en çok 12'dir; her satır ayrı `SendHelpDescription` paketidir (≤ 13 paket).
- GM yetkisi kontrolü iki kez yapılır (`Chat()` `isGM()` + işleyici `isGM()`); mevcut `+` komutlarındaki desenle aynıdır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- **Durum:** UYGULANDI
- **Branch / commit'ler:** `bot/F3-04` (taban: `gece/2026-10-02` @ `13526d9`). Kod commit'i `e7e33e6` (`[F3-04] Oyun ici GM +bot komutu, anlik list yanit (BuildStatusLines/GetStatusSnapshot)`, 4 dosya). Plan (Durum + bu rapor) ayrı commit edilir.
- **Değişen dosyalar ve nedenleri:**
  - `GameServer/Bot/BotManager.h`: `GetStatusSnapshot` (public, any thread) + `BuildStatusLines`/`RefreshStatusSnapshot` (private, IOCP) bildirimleri ve `m_statusLock`/`m_statusLines`/`m_lastStatusRefresh` üyeleri (plan §5.2).
  - `GameServer/Bot/BotManager.cpp`: `STATUS_REFRESH_MS=1000`; `CommandList` gövdesi `BuildStatusLines`'ı kullanacak şekilde yeniden yazıldı (log metni birebir korundu); `BuildStatusLines`/`RefreshStatusSnapshot`/`GetStatusSnapshot` eklendi; `Tick()` içine `TickSessions()` sonrasına `RefreshStatusSnapshot(...)` eklendi (plan §5.3).
  - `GameServer/ChatHandler.cpp`: `CUser` komut tablosuna `bot` satırı ve `COMMAND_HANDLER(CUser::HandleBotCommand)` gövdesi eklendi (plan §5.6).
  - `GameServer/User.h`: `COMMAND_HANDLER(HandleBotCommand);` bildirimi (plan §5.5).
- **Derleme sonucu (`tools/build.sh Release`, son satırlar):**
  ```
    Kodun üretilmesi tamamlandı
    proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
    proj-AIServer.vcxproj -> ...\build\bin\x86-Release\Server\AIServer.exe
    BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  `rc=0`. Tam derlemede yalnızca eski dosya uyarıları (`MagicProcess.cpp`, `Map.cpp`, `User.cpp`, `UpgradeHandler.cpp`); `BotManager.cpp`/`ChatHandler.cpp` `touch` edilip yeniden derlendi → **0 uyarı**.
  Debug: `rc=0`; 6 uyarı eski dosyalarda (`DBAgent.cpp`, `EventHandler.cpp`, `GameServerDlg.cpp`, `LoginHandler.cpp`, `MagicInstance.cpp`), `BotManager.cpp`/`ChatHandler.cpp` uyarısız.
- **Kabul kriterleri öz-değerlendirme:**
  - K1 ✔ Release rc=0, yeni uyarı yok (kanıt yukarıda).
  - K2 ✔ Debug rc=0.
  - K3 ✔ `git diff --stat gece/2026-10-02...bot/F3-04` yalnızca 4 dosya (`BotManager.cpp` 47, `BotManager.h` 11, `ChatHandler.cpp` 54, `User.h` 1); `User.h` +1/−0; `ChatHandler.cpp` +54/−0.
  - K4 ✔ `BotManager::Instance()` çağrıları: `1154`/`1174` mevcut konsol işleyicisi; yeni işleyicide yalnızca `isEnabled` (`1189`), `GetStatusSnapshot` (`1206`), `EnqueueCommand` (`1227`). `m_statusLines` yazımı yalnızca `771` (`m_statusLock` altında `772`), okuması yalnızca `780` (`m_statusLock` altında `781`); `BuildStatusLines` kilit dışında çağrılır (`769`, `788`).
  - K5 ✔ `BuildStatusLines` başlık `"%u session(s), pool free %u/%u"`, satır `"  %s phase=%s slot=%s despawns=%u"`; `CommandList` öneki `"BotManager: cmd list: "` (`:791`). Diff'te eski gövde `-`, yeni gövde yan yana; log metni bayt bayt aynı.
  - K6 ✔ `Tick()` farkı yalnızca `TickSessions();` sonrası tek satır `RefreshStatusSnapshot(std::chrono::steady_clock::now());`; 1 sn sınırı (`STATUS_REFRESH_MS`); `ENABLED=0`'da `Tick` çalışmaz, `GetStatusSnapshot` `false`.
  - K7 ✔ `!isGM()` → `false`; `!isEnabled()` → tek yanıt; `list` (tek argüman, `_stricmp`) kuyruğa girmez, `GetStatusSnapshot`; diğerleri `EnqueueCommand`'a satır olarak gider (§5.6 kanıt sed çıktısı).
  - K8 ✔ Diğer fonksiyon gövdeleri değişmedi; yalnızca `CommandList` yeniden yazıldı, 3 yeni fonksiyon, `Tick`'e 1 satır, tablo satırı, `User.h` bildirimi.
  - K9 ✔ Yeni kodda `printf`/yeni `WriteBotLog` yok; GM yanıtı `SendHelpDescription`.
  - K10 ✔ `file`: `BotManager.cpp`/`.h` ve `User.h` "ASCII ... CRLF"; `ChatHandler.cpp` "UTF-8 (with BOM) ... CRLF".
  - K11 ✔ `m_statusLines` yalnızca `RefreshStatusSnapshot`'ta (IOCP, `Tick` üzerinden) yazılır; `Tick` `ENABLED=1` değilse çalışmaz; `+bot` önce `isEnabled()` kontrol eder.
  - K12 ✔ `git status --short` yalnızca plan dosyasını gösterir (o da commit'lenecek); sunucu çalıştırılmadı, `GameServer.ini`/`BotCommands.txt`/DB değişmedi.
- **Plandan sapmalar:** Yok. Plan §2'deki satır numaraları kodla uyuştu; tüm eklemeler plandaki metinle aynı.
- **Açık sorular:** Yok. (Çalışma zamanı doğrulaması ve `+bot` oyun içi yolu T-ARCH-05 Claude/proje sahibinde; DeepSeek çalışma zamanı testi yapmadı.)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- **Karar:** DOĞRULANDI
- **İncelenen commit:** `bot/F3-04` @ `96c5f9b` (kod `e7e33e6`; taban `gece/2026-10-02` @ `13526d9`; gece modu, `AUTO_LOOP=1`: birleştirme/push yapılmadı)
- **Kriter tablosu:**

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release | ✔ | `BotManager.cpp` + `ChatHandler.cpp` `touch` edilip `./tools/build.sh Release` rc=0; uyarılar yalnızca eski `UpgradeHandler.cpp(634/862)` C4789 (LTCG); `Bot\`, `ChatHandler.cpp`, `User.h` kaynaklı uyarı 0 |
| K2 Debug | ✔ | aynı iki dosya `touch` + `./tools/build.sh Debug` rc=0; çıktıda `warning` 0 |
| K3 kapsam | ✔ | `git diff --stat gece/2026-10-02...bot/F3-04`: `BotManager.cpp` 47, `BotManager.h` 11, `ChatHandler.cpp` +54/−0, `User.h` +1/−0, plan dosyası; `docs/`, `tools/`, `shared/`, `.claude/` farkı yok. Çalışma ağacı CRLF, depo blobu LF (`core.autocrlf=true`, taban blobuyla aynı); çalışma ağacında LF-only satır 0; `ChatHandler.cpp` blobu `ef bb bf` ile başlıyor (BOM korunmuş) |
| K4 thread | ✔ | `grep -n "BotManager::Instance()" GameServer/ChatHandler.cpp`: `:1154`/`:1174` mevcut konsol işleyicisi, yeni işleyicide yalnızca `isEnabled` (`:1189`), `GetStatusSnapshot` (`:1206`), `EnqueueCommand` (`:1227`). `m_statusLines` yazımı yalnızca `BotManager.cpp:772` (`swap`, `m_statusLock` `:771`), okuması yalnızca `:781` (kilit `:780`); `BuildStatusLines` `:769` ve `:788`'de kilit dışında; kilit bloğunun içinde başka kilit yok |
| K5 `CommandList` aynı | ✔ | diff: başlık `"%u session(s), pool free %u/%u"`, satır `"  %s phase=%s slot=%s despawns=%u"`, `CommandList` öneki `"BotManager: cmd list: "` (`:791`); `poolFree` kilit bloğu aynen taşınmış. **Çalışma zamanında doğrulandı:** `BotManager: cmd list: 2 session(s), pool free 14/16` ve `BotManager: cmd list:   BotWP_K phase=in_game slot=2984 despawns=0` (F2-06 doğrulamasıyla aynı biçim) |
| K6 `Tick` | ✔ | diff'te `Tick()` içinde yalnızca `TickSessions();` sonrasına `RefreshStatusSnapshot(std::chrono::steady_clock::now());` (`:386`); `STATUS_REFRESH_MS=1000` ile sınırlı; `ENABLED=0`'da `Tick` yok, `GetStatusSnapshot` `false` (`:777`) |
| K7 işleyici | ✔ | `ChatHandler.cpp:1182-1233`: `!isGM()` → `false`; `!isEnabled()` → tek yanıt; `list` (`vargs.size()==1`, `_stricmp`) anlık görüntü, kuyruğa girmez; kalan her şey boşlukla birleştirilip `EnqueueCommand`. `StrSplit` (`ChatHandler.h:63`) boş parça üretmez, plandaki boşluk endişesi geçersiz |
| K8 dokunulmayanlar | ✔ | diff yalnızca `CommandList` (yeniden yazım), 3 yeni fonksiyon, `Tick`'e 1 satır, sabit, tablo satırı, işleyici, `User.h` bildirimi; `BotSession.*`, `ScenarioRunner.*`, `Telemetry.*`, konsol `HandleBotCommand` farkta yok |
| K9 printf/log | ✔ | eklenen satırlarda `printf` yok, yeni `WriteBotLog` yok (yalnızca `CommandList`'teki mevcut çağrının yeni biçimi) |
| K10 kodlama | ✔ | `file`: `GameServer/Bot/*` hepsi "ASCII text, with CRLF line terminators"; `ChatHandler.cpp` "UTF-8 (with BOM) text, with CRLF"; `User.h` "ASCII ... CRLF"; tab/Allman, yorumlar İngilizce |
| K11 kapalıyken | ✔ | `m_statusLines` yalnızca `Tick()` yolunda yazılır; **çalışma zamanında** `ENABLED=0`: `BotCommands.txt` 30 sn sonra yerinde, `Bot_*.log` +0 satır |
| K12 temizlik | ✔ | `git status --short` boş; commit'ler `[F3-04] ...` biçiminde; merge/force izi yok |

- **Çalışma zamanı doğrulaması (Release, `GameServer.ini` yedekten geri yüklendi, `BotCommands.txt` silindi, sunucular kapatıldı 0/3):**
  1. `ENABLED=1, MAX_BOTS=16, TELEMETRY=summary`: `spawn BotWP_K,botmf_k` + `spawn Foo` + `list` → `queued`, `unknown bot name 'Foo' ignored`, `cmd list: 2 session(s), pool free 16/16` (iki `queued`); sonra `list` → `pool free 14/16`, `in_game` + slot 2984/2985; `despawn all` → `2 despawning`, iki `despawned`.
  2. F2-05 gerilemesiz: 4 bot, `DESPAWN_AFTER_SEC=2, RESPAWN_CYCLES=4` → `respawn cycles done: 20 spawns, 20 despawns, 0 failed, 0 stuck, 0 names left, pool free 16/16, elapsed 23 s`.
  3. F3-03 gerilemesiz: `scenario run smoke` → `scenario smoke finished: 2/2 run(s) completed`.
  4. `Tick` maliyeti: `PERF_SAMPLE` `tick_p95_us` 63-112, `skipped_ticks` 0 (anlık görüntü 1 Hz, fark gözlenmedi). `GameServer.log`'a yeni hata satırı eklenmedi (son hata satırı çalışmadan önceki).
  5. `+bot` oyun içi yolu (GM istemcisi) otomatikleştirilemedi: T-ARCH-05 insan testi (`docs/STATUS.md` "Proje sahibi testleri (bekleyen)", zaten kayıtlı). Plan bunu kriter dışı bırakmıştı; karara etkisi yok.
- **Bulgular (hepsi not, engel değil):**
  1. Kod kapsamı ve içeriği plandaki metinle birebir; sapma yok, uygulayıcı raporu gerçekle uyuşuyor (commit listesi, dosya satır sayıları, derleme sonucu).
  2. `+bot list` yalnızca tek argümanla anlık görüntüyü kullanır; `+bot list x` kuyruğa girer ve çıktısı yalnızca `Bot_*.log`'da olur (planın tasarımı; konsol yolundaki `LIST extra` davranışıyla tutarlı).
  3. Test artıkları depo dışında: `C:\dev\fdp\server\Scenarios\smoke.yaml` ve `Logs/bots/2026-10-02/` altındaki yeni telemetri dosyaları temizlenmedi (F3-03 notundaki temizlik maddesine eklenir). Doğrulama sırasında sunucu, `ENABLED=0` koşusunda kendi `GetBool` varsayılanını `GameServer.ini`'ye (`[BOT] ENABLED=0`) yazdı; ini yedekten geri yüklendi (md5 aynı).
