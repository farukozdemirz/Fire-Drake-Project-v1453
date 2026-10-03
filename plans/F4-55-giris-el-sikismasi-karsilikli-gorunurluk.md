# F4-55: Bot giriş el sıkışmasının serileştirilmesi: karşılıklı görünürlük (KI-DEG-01 kalıcı düzeltme)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapı G4) |
| Branch | `bot/F4-55 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F2-03 (bot girişi / spawn durum makinesi), F4-12, F4-13 (görünür oyuncu tablosu, `WIZ_REQ_USERIN`), F4-54 (teşhis sayaçları, kök neden H2) — hepsi `KAPANDI` (`gece/2026-10-02` içinde birleşik; F4-54 merge `cf22667`) |
| İlgili gereksinim / kabul | KI-DEG-01, Q-27; `docs/03` §13 (CLI-19) ve §16 (3×3 bölge bilgisi); `docs/13` §4.3 (spawn), §5.2a (`in_region`); `docs/reports/degerlendirme-2026-10-02.md` DEG-04 |
| Tahmini büyüklük | S (3 kod/proje dosyası + 1 test dosyası + 1 proje dosyası; saf zamanlama kapısı) |
| Hazırlayan / tarih | Claude / 2026-10-03 |

---

## 1. Amaç

Bot doğuşları ≤ 100 ms arayla (tek `spawn A,B,C` komutu, senaryo, `SPAWN_ON_START`, `RESPAWN_CYCLES`) başlasa bile her bot çifti **birbirini görsün**: `BotManager::TickSessions` giriş el sıkışmasını (`GameStart(1)` → `GameStart(2)`) botlar arasında **serileştirir**; yani bir oturum `GameStart(1)`'i ancak başka hiçbir oturum `PHASE_WAIT_LOADED`'dayken (iki opcode arasındayken) gönderir. Böylece F4-54'te kök neden olarak bulunan H2 penceresi (bkz. §2) bot-bot çiftlerinde yapısal olarak kapanır ve "botları ≥ 3 sn arayla doğur" geçici çözümü kaldırılabilir. Bu plan `ENABLED=0` davranışını değiştirmez, oyun mekaniğine, sunucu koduna ve botun aldığı bilgiye dokunmaz.

## 2. Bağlam (okunması zorunlu)

Aşağıdaki tüm dosya:satır referansları `gece/2026-10-02` @ `f4daa27` (kod `0001d04`'ten beri değişmedi, aradaki commit'ler yalnız belge/plan) üzerinde 2026-10-03'te `git show` / `git grep -a` ile **yeniden doğrulandı** `[D]`; dal ucu kaymışsa Uygulayıcı §5 adım 2'deki ön kontrolü yapar.

- `docs/KNOWN_ISSUES.md` KI-DEG-01 satırı ve `plans/F4-54-algi-tek-yonlu-gorus-teshisi.md` Doğrulama Raporu (12 koşu): ≤ 100 ms aralıkta 6/6 asimetri, 3 sn aralıkta 6/6 simetri; sayaçlar: `region recv 0`, `parse_fail 0`, `userin recv 1`, `userin requests 0 / pending 0`; H1/H3/H4 çürütüldü, H2 doğrulandı `[V]`.
- **Sunucu akışı (okunur, değişmez):**
  - `GameServer/CharacterSelectionHandler.cpp:266-270` `GameStart(1)`: `SendMyInfo(); UserInOutForMe(this); MerchantUserInOutForMe(this); ...` → bölge kullanıcı listesi o andaki bölge üyelerinden üretilir.
  - `GameServer/CharacterSelectionHandler.cpp:282-284` `GameStart(2)`: `m_state = GAME_STATE_INGAME; UserInOut(INOUT_RESPAWN);` → bot bölgeye **ancak şimdi** kaydolur (`GameServer/CharacterMovementHandler.cpp:71-97` `UserInOut`: `GetRegion()->Add(this)`, ardından `SendToRegion`). `SelectCharacter` yalnızca `SetRegion` yapar, bölge dizisine eklemez (`CharacterSelectionHandler.cpp:206`).
  - Liste ve yayın süzgeci `isInGame()`: `GameServer/GameServerDlg.cpp:1377-1379` (`GetRegionUserIn`, liste) ve `:957-959` (`Send_UnitRegion`, yayın); `GameServer/User.h:312`.
  - Sabit duran bota `WIZ_REGIONCHANGE` gelmez: yalnızca `RegisterRegion()` true ise (`GameServer/CharacterMovementHandler.cpp:35-40`).
- **Bot giriş durum makinesi:** `GameServer/Bot/BotManager.cpp`: sabitler `:18-23` (`SELECT_SETTLE_MS = 1000`, `LOADED_DELAY_MS = 200`, `PHASE_TIMEOUT_MS = 15000`); `TickSessions` `:2921`; `PHASE_QUEUED` tick başına en çok bir `StartSession` `:2939-2946`; `PHASE_WAIT_SELECT` `:2948-2977` (`GameStart(1)` `:2965`, `m_phase = PHASE_WAIT_LOADED` `:2967`); `PHASE_WAIT_LOADED` `:2978-3007` (`GameStart(2)` `:2981`, sonuç `PHASE_IN_GAME` veya `FailSession`); `StartSession` `:3438`; `FailSession` `:3482`; toplu özet `:3311-3320`. Bütün giriş yolları (`CommandSpawn` `:688`, `ScenarioRunner.cpp:703`, `SPAWN_ON_START`, `ResetForRespawn` `:720`/`:3435`) aynı durum makinesinden geçer: tek kapı noktası.
- **H2'nin biçimsel hâli** `[D]` (kodun doğrudan sonucu; F4-54 12 koşu ile `[V]` uyumlu): bot X'in bot Y'yi **görmemesi** ⇔ `op1(X) < op2(Y) < op2(X)`; yani Y'nin kaydı X'in anlık görüntüsünden sonra, X'in oyunda olmasından önce. Aynı anda iki yön olamaz (çelişkili eşitsizlik), bu yüzden asimetri **tek yönlüdür**; F4-54 koşu 1 (WP, MF, PHD; 100 ms aralık, el sıkışma 200 ms): `MF!WP, PHD!WP, PHD!MF` — formülle birebir aynı küme. El sıkışma aralıkları `[op1, op2]` ikişer ikişer ayrık ise her çift için ya `op2(Y) < op1(X)` (X, Y'yi listeden alır; Y, X'in `op2` yayınını alır) ya tersi olur: **simetri**.
- Gerçek istemci karşılaştırması: bkz. §3 "Seçenek değerlendirmesi" ve §8.
- `docs/13` §5.2a (gözlem/`in_region` sözleşmesi), `docs/03` CLI-19 (`WIZ_REQ_USERIN` yalnızca son `WIZ_REGIONCHANGE`'in bildirdiği kimlikler için; keyfi kimlik istenmez).
- `GameServer/Bot/ScenarioRunner.cpp:19` `SCENARIO_PREPARE_TIMEOUT_MS = 60000`, `:787-789` (tüm botlar `PHASE_IN_GAME` olmazsa `Abort("prepare timeout")`): giriş yavaşlatan her çözüm bu sınırın altında kalmalı.
- `tools/check-perception-contract.py` (R1-R5): yeni kod `GetUserPtr`, `->m_pUser` (başka oturumun), `GetRegion`, `m_RegionUserArray` kullanmaz.

## 3. Kapsam

### Seçenek değerlendirmesi (kodla) ve seçim

| Seçenek | Kod kanıtı | Sonuç |
|---|---|---|
| **(a1)** `StartSession` arasına asgari süre (ör. ≥ 3 sn) | Sorun zamanlamaya bağlı kalır: gerçek aralık `SELECT_SETTLE_MS` + DB iş parçacığı değişkenliği ile oynar, 3 sn bir marjdır, garanti değil. Ölçek: 16 bot × 3 sn ≈ 48 sn; `SCENARIO_PREPARE_TIMEOUT_MS` (60 sn, `ScenarioRunner.cpp:19`) sınırına yakın, 20+ botta aşar. | **Reddedildi** (yavaş ve garantisiz) |
| **(a2)** El sıkışma serileştirmesi: oturum `GameStart(1)`'i yalnızca başka oturum `PHASE_WAIT_LOADED`'da değilken gönderir | H2 kapanış koşulu ("aralıklar ayrık") yapısal olarak sağlanır; her oturum için kendi paket dizisi ve içeriği **değişmez**. Maliyet: oturum başına ≈ 200-300 ms (`LOADED_DELAY_MS` + tick yuvarlaması); 16 bot ≈ +5 sn, 100 bot ≈ +30 sn. Seçme (`GameStart(1)` öncesi) aşaması paralel kalır. | **SEÇİLDİ** |
| **(b)** `GameStart(2)` sonrası bota bölge listesini yeniden istetmek (`WIZ_REQ_USERIN` / bölge yeniden eşitleme) | `WIZ_REQ_USERIN` **kimlik listesi ister** (`GameServer/User.cpp:1264-1287` `RequestUserIn`); "bölgedekilerin hepsini ver" isteği yoktur. Kimlik listesi veren yollar yalnızca: bölge sınırı geçişi (`CharacterMovementHandler.cpp:35-40`), `WIZ_REGENE` (`AttackHandler.cpp:214-218`), `WIZ_ZONE_CHANGE`'in `Loading` adımı (`CharacterMovementHandler.cpp:674-679`), warp. Hiçbiri doğuş sonrası sabit duran gerçek bir istemcinin yasal olarak tetikleyebileceği bir yol değildir; bota bunları sırf liste almak için yaptırmak ya (i) konumu değiştirir (sahte hareket) ya (ii) gerçek istemcide olmayan bir istek olur. `PendingIds` yalnızca `WIZ_REGIONCHANGE`'den dolar (`BotSession.cpp:320`); kimlikleri tahmin ederek istemek CLI-19'a ("keyfi kimlik istenmez") aykırıdır. | **Reddedildi** (uygulanabilir ve adil bir tetikleyici yok) |
| (c) Bilinmeyen kimlikli `WIZ_MOVE` gelince o kimliği `PendingIds`'e koyup `REQ_USERIN` istemek | Yalnızca kayıp birim hareket ederse işe yarar; durağan birim (F4-54 koşularındaki hâl) için yetmez. Gerçek istemcinin bilinmeyen kimlikli `WIZ_MOVE`'a tepkisi ölçülmedi `[Ö]` (T-PERC-01); yeni bot→sunucu trafiği ekler. | Kapsam dışı; savunma katmanı olarak ayrı plan adayı (önkoşul: T-PERC-01'de istemci davranışının ölçülmesi) |
| (d) Sunucu düzeltmesi (`GameStart(2)` sonrası katılana liste yeniden gönderme / liste ile kayıt atomik) | Gerçek oyuncuları da etkileyen mekanik değişikliği olur (`docs/03` önce güncellenmeli, proje sahibi kararı); yalnızca `m_botSink != nullptr` için yapılırsa bota gerçek istemcinin almadığı bir bilgi yolu açılır (bot avantajı yasağı, `docs/03` §13, `docs/13` §3). F4-54 de sunucu koduna dokunmamayı şart koştu. | Reddedildi; gerçek oyuncu penceresi için bkz. §8 (ayrı karar) |

**Seçim: (a2).** Gerekçe: (1) tek kapı noktasında (`TickSessions`) küçük, `BotCore` mantığıyla modellenip birim sınanabilir; (2) garanti yapısaldır (zamanlama marjına dayanmaz); (3) bot tarafı bilgi/aksiyon yolunu değiştirmez, sunucuya dokunmaz; (4) yalnızca `TickSessions` içinde olduğundan `ENABLED=0`'da (oturum yok, tick yok) etkisizdir.

**Gerçek istemci uyumu / bot avantajı yok `[D]` + yargı:** her bot oturumunun sunucuya gönderdiği paket dizisi (`SEL_CHAR`, `GameStart(1)`, ≥ 200 ms sonra `GameStart(2)`) ve sunucunun ona verdiği cevaplar (liste + yayın) **aynıdır**; yalnızca oturumların birbirine göre **ne zaman** başladığı (operatör/senaryo düzeyi zamanlama, oyun içi aksiyon değil) değişir. Gerçek oyuncular da kendi giriş zamanlarını serbestçe seçer (ve gerçek istemcinin yükleme süresi `LOADED_DELAY_MS`'den uzundur: aynı pencerenin gerçek oyuncu için daha geniş olduğu §8'de). Kapı yalnızca `BotSession::m_phase` (yönetici düzeyi, algı değil) okur: algı/karar katmanına yeni bilgi girmez, `ObsTable` veri kaynağı değişmez.

**Yapılacaklar**

1. `BotCore/SpawnGate.h` (yeni, yalnızca standart başlıklar): iki saf fonksiyon: `HandshakeGateOpen(int sessionsInHandshake)` ve `HandshakeMissesUnit(uint64_t op1X, uint64_t op2X, uint64_t op2Y)` (§2'deki H2 formülü).
2. `BotManager::TickSessions`: `PHASE_WAIT_SELECT` dalında `SELECT_SETTLE_MS` geçtikten sonra, `GameStart(1)` göndermeden önce kapı denetimi: o anda `PHASE_WAIT_LOADED`'daki oturum sayısı (`m_sessions` taraması, dosya-statik yardımcı) `HandshakeGateOpen` ile sınanır; kapalıysa oturum `PHASE_WAIT_SELECT`'te kalır (bu tick'te `GameStart(1)` yok) ve `m_handshakeWaitTicks` artar.
3. `/bot` günlüğüne (`WriteBotLog`, konsol değil) toplu özetten hemen sonra bir satır: kaç tick kapı beklemesi oldu (ölçüm: kapının devreye girdiğinin kanıtı).
4. Saf mantık birim testleri (`Tests/BotCoreTests/SpawnGateTests.cpp`): (i) örtüşen el sıkışma modeli F4-54 koşu 1 asimetri kümesini üretir; (ii) kapılı tick döngüsü modeli 3..16 bot için tüm çiftlerde simetrik ve hiçbir an ikiden fazla değil bir oturum el sıkışmada; (iii) kapı kapatılırsa (her zaman açık) test asimetri bulur (testin boş olmadığının kanıtı).
5. Çalışma zamanı kanıtı Claude'dadır (§6 K9-K11).

**Kapsam dışı (yapılmayacak)**

- Sunucu (`GameServer/*.cpp` bot dışı, `shared/`, `AIServer/`) değişikliği; `docs/`, `KNOWN_ISSUES.md` (Claude günceller).
- Yeni ini anahtarı, komut, thread, paket; `LOADED_DELAY_MS`/`SELECT_SETTLE_MS` değerlerini değiştirmek; `WIZ_REQ_USERIN`/`TickUserIn`/`PendingIds`/`ObsTable` mantığına dokunmak (seçenek b, c).
- Gerçek oyuncu girişleri ile bot girişleri arasındaki pencere (sunucu özelliği, §8).
- Bot oturumlarının sonradan eksik birimleri keşfetmesi (savunma katmanı).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/SpawnGate.h` | yeni | saf mantık, yalnızca `<cstdint>`; ASCII + CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | `<ClInclude Include="SpawnGate.h" />` (BotCore'un `.filters` dosyası yoktur: `git ls-tree` ile doğrulandı) |
| `Tests/BotCoreTests/SpawnGateTests.cpp` | yeni | `#include "MiniTest.h"`, `#include <BotCore/SpawnGate.h>` |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | `<ClCompile Include="SpawnGateTests.cpp" />` (`.filters` yok) |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `TickSessions` kapı denetimi, dosya-statik sayım yardımcısı, `#include <BotCore/SpawnGate.h>`, özet log satırı |
| `GameServer/Bot/BotManager.h` | değiştir | yalnızca `uint32 m_handshakeWaitTicks;` üyesi |

`GameServer/proj-GameServer.vcxproj`/`.filters` değişmez (GameServer'a yeni dosya eklenmiyor). Bu listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. Not: yeni sınıf/fonksiyonlar `Perception.h`'ye eklenmez (algı dosyası F4-60/F4-61'in alanıdır, birleştirme çakışması olmasın diye yeni dosyalar kullanıldı).

## 5. Uygulama adımları

1. `git switch -c bot/F4-55 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; `./tools/run-servers.sh status` `[UP]` ise `./tools/run-servers.sh stop`.
2. **Ön kontrol (satır numaraları kaymış olabilir):** `git grep -n -a -E "LOADED_DELAY_MS|WIZ_GAMESTART|PHASE_WAIT_LOADED" -- GameServer/Bot/BotManager.cpp` ile `TickSessions` içindeki `GameStart(1)` ve `GameStart(2)` yerlerini bul; `git grep -n -a -E "UserInOutForMe\(this\)|UserInOut\(INOUT_RESPAWN\)" -- GameServer/CharacterSelectionHandler.cpp`. Sunucudaki iki opcode'un davranışı §2'deki gibi değilse (ör. `GameStart(1)` artık kaydediyorsa) **dur ve raporla**.
3. `BotCore/SpawnGate.h` (CRLF, tab, Allman, İngilizce yorum):
   ```cpp
   #pragma once
   #include <cstdint>

   namespace BotCore
   {
   	// A spawning session may send GameStart(1) only while no other session sits between its own
   	// GameStart(1) and GameStart(2) (plan F4-55). Pure rule; BotManager counts the sessions.
   	inline bool HandshakeGateOpen(int sessionsInHandshake)
   	{
   		return sessionsInHandshake <= 0;
   	}

   	// Server model of the two-step login (CharacterSelectionHandler.cpp GameStart(1)/(2)):
   	// unit X does NOT learn about unit Y iff Y registered after X took its region snapshot and
   	// before X was in game (X then misses Y's broadcast): op1(X) < op2(Y) < op2(X).
   	inline bool HandshakeMissesUnit(uint64_t op1X, uint64_t op2X, uint64_t op2Y)
   	{
   		return op1X < op2Y && op2Y < op2X;
   	}
   }
   ```
4. `BotManager.cpp`: `#include <BotCore/SpawnGate.h>` (mevcut BotCore include'larının yanına); dosya-statik `static int CountInHandshake(const std::vector<BotSession *> & sessions)`: `m_phase == BotSession::PHASE_WAIT_LOADED` olanları sayar (yalnızca `m_phase`, başka alan yok). `TickSessions`'ta `PHASE_WAIT_SELECT` → `SELECT_OK` → `SELECT_SETTLE_MS` geçti dalında, `Packet pkt(WIZ_GAMESTART, uint8(1));`'den **önce**:
   ```cpp
   if (!BotCore::HandshakeGateOpen(CountInHandshake(m_sessions)))
   {
   	m_handshakeWaitTicks++;
   	break;   // leaves the switch for this session; it retries next tick
   }
   ```
   `break` bu `case` bloğundan (switch'ten) çıkar, `for` döngüsünden değil: bunu `switch` yapısına bakarak doğrula (`break` `for`'a değil `switch`'e bağlanır). Kapı beklemesi `PHASE_TIMEOUT_MS`'e **sayılmaz** (zaman aşımı `SELECT_OK` değilken çalışan `else if` dalındadır): bekleme üst sınırı = bekleyen oturum sayısı × ≈ 0,3 sn; bir oturum `PHASE_WAIT_LOADED`'da takılamaz (`GameStart(2)` 200 ms sonra koşulsuz gönderilir, sonuç `PHASE_IN_GAME` veya `FailSession`), yani kapı kilitlenemez.
5. `BotManager.h`: `uint32 m_handshakeWaitTicks;   // IOCP thread only: ticks a ready session waited for the login handshake gate` ve kurucuda `0` (mevcut `m_spawnOk` gibi başlatılan yerde; başlatıcı listesini silmeden ek satırla).
6. `BotManager.cpp` toplu özet (`:3311-3320`, `m_spawnSummaryDone = true` bloğu): mevcut `spawn complete` satırı **aynen kalır**; hemen ardından ikinci `WriteBotLog`: `"BotManager: spawn handshake: serialized, gate waits %u tick(s)"`. Başka log biçimi değişmez.
7. `Tests/BotCoreTests/SpawnGateTests.cpp` (üç test, tamamı modelde; sunucu/oturum çalıştırmaz):
   - `SpawnGate_RuleOpenOnlyWhenIdle`: `HandshakeGateOpen(0) == true`, `(1) == false`, `(3) == false`.
   - `SpawnGate_OverlapReproducesOneWayView`: üç bot, `op1 = {0, 100, 200}`, `op2 = op1 + 200`; `HandshakeMissesUnit` ile ikili tablo: eksik yönler tam olarak `{1!0, 2!0, 2!1}` (F4-54 koşu 1 ile aynı: MF!WP, PHD!WP, PHD!MF) ve hiçbir çiftte iki yön birden eksik değil.
   - `SpawnGate_GatedLoopIsSymmetric`: 100 ms tick'li küçük model (oturumlar indeks sırasıyla gezilir, hepsi `t = 0`'da seçime hazır; durum: HAZIR / EL_SIKISMADA (`t - op1 >= 200` olunca `op2`) / BITTI; HAZIR oturum `HandshakeGateOpen(sayım)` ile geçer): `N` ∈ {2, 3, 6, 16} için (i) hiçbir çiftte `HandshakeMissesUnit` doğru değil, (ii) hiçbir tick'te el sıkışmadaki oturum sayısı 1'i aşmıyor, (iii) bitiş zamanı `N * 300 ms` altında; aynı model kapı **her zaman açık** ile koşturulunca `N >= 2` için en az bir eksik yön çıkıyor (test boş değil).
   - Test sayısı: dal açıldığında `./tools/run-tests.sh Release` çıktısındaki sayı `T0` (beklenen `259`, F4-53 birleşik); sonuç `T0 + 3`. Üç yeni test adı çıktıda `[ OK ]` olmalı.
8. `BotCore.vcxproj` ve `BotCoreTests.vcxproj`'a ilgili satırlar (§4). Başka proje/filters değişmez.
9. Derle ve test et (§7). `tools/check-perception-contract.py` çalıştır (PASS, R1-R5 yeni ihlal yok; `SpawnGate.h` `BotCore` saflığı R4: yalnızca `<cstdint>`).
10. `git diff --stat gece/2026-10-02...bot/F4-55` ile yalnızca §4'teki dosyaların değiştiğini doğrula; Uygulayıcı Raporu'nu yaz; **sunucu çalıştırma ve oyun içi koşu Claude'dadır**, Raporda "çalışma zamanı koşusu Claude'da" yaz.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0; değişen `.cpp` dosyaları `touch` edilip yeniden derlenince yeni uyarı yok (eski `UpgradeHandler.cpp` C4789 uyarıları hariç)
- [ ] K2: `./tools/build.sh Debug` rc=0, değişen dosyalarda uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; test sayısı `T0 + 3`; `SpawnGate_RuleOpenOnlyWhenIdle`, `SpawnGate_OverlapReproducesOneWayView`, `SpawnGate_GatedLoopIsSymmetric` çıktıda `[ OK ]`
- [ ] K4: `git diff gece/2026-10-02...bot/F4-55 -- GameServer` yalnızca `BotManager.cpp`/`BotManager.h`; `-` satırı yok ya da yalnızca başlatıcı listesi değişimi gerekçesiyle (Raporda açıkla); `GameServer/*.cpp` bot dışı, `shared/`, `AIServer/` farkı yok
- [ ] K5: kapı yalnızca `m_phase` okur: yeni satırlarda `g_pMain|GetUserPtr|GetRegion|m_RegionUserArray|->m_pUser` yok (`git diff ... | grep '^+' | grep -E ...` boş); `tools/check-perception-contract.py` `RESULT: PASS`
- [ ] K6: yeni ini anahtarı/komut/thread/paket yok; `ENABLED=0` yolu değişmez (`TickSessions` oturum yokken ilk satırda döner: `BotManager.cpp:2923-2924`); `PHASE_TIMEOUT_MS`/`SELECT_SETTLE_MS`/`LOADED_DELAY_MS` değerleri aynı (`git diff` bu satırlarda `-` yok)
- [ ] K7: yeni dosyalar ASCII + CRLF; `git diff --check` boş; `BotCore.vcxproj` ve `BotCoreTests.vcxproj` yeni dosyaları içeriyor
- [ ] K8: Uygulayıcı Raporu'nda kapı kod parçası (dosya:satır), özet log satırı biçimi ve "kapı kilitlenemez" gerekçesi (`WAIT_LOADED` her durumda 200 ms sonra çıkar) yazılı
- [ ] K9 (çalışma zamanı, Claude yapar, **tüm 12 koşu**): F4-54 yöntemiyle (`Release` ikililer dal ucundan, `GameServer.ini` md5 önce/sonra aynı, `[BOT] ENABLED=1`, her koşuda sunucular yeniden başlatılır, `Scripts/see_symmetry.txt` / `bots/config/script_see_symmetry.txt`) 3 Karus bot (`BotWP_K`, `BotMF_K`, `BotPHD_K`), 6 doğuş sırası × 2 tekrar = **12 koşu, hepsi tek `spawn` komutuyla (≤ 100 ms aralık, ≥ 3 sn aralık YOK)**: **12/12 simetrik** (her bot diğer ikisini `/bot see` ile görüyor, sunucu `list` konumlarıyla çapraz kontrol). Her koşuda `diag:` sayaçları: `inout_parse_fail 0`, `region recv 0`, `userin stop 0`, `userin recv 1`, `move_unknown 0`; her botta `inout in + userin units = 2`; günlükte `spawn handshake: serialized, gate waits N tick(s)` satırı **N >= 1** (kapı devrede). Sonuç tablosu bu plana ve KI-DEG-01'e işlenir.
- [ ] K10 (çalışma zamanı, Claude yapar): geçici çözümün kaldırılabildiği kanıtı: K9'daki koşular `BotCommands.txt` içinde **ayrı dosya/bekleme olmadan** tek satır `spawn BotWP_K,BotMF_K,BotPHD_K` ile yapılır (F4-54'ün 6/6 asimetrik olduğu koşul); ek olarak (a) 6 Karus bot (`BotWP_K, BotWG_K, BotPHD_K, BotPHB_K, BotMF_K, BotMI_K`) tek `spawn` ile **bir** koşu: hepsi aynı 3×3 bölgede doğuyorsa (Claude sunucu `list` ile doğrular) 30 yönlü ilişkinin tamamı görünür; aynı bölgede değillerse bölge içi çiftler simetrik ve sapma `list` ile açıklanır; (b) `SPAWN_ON_START` ile üç bot **bir** koşu (`GameServer.ini` değişirse md5 geri alınır); (c) (isteğe bağlı kontrol) dal tabanından (`gece/2026-10-02`) derlenmiş ikiliyle 1-2 koşu: asimetri hâlâ üretiliyor (ortam kayması olmadığının kanıtı)
- [ ] K11 (çalışma zamanı, Claude yapar): maliyet ve geri-dönüş yokluğu: 3 botta toplam giriş süresi (`spawn` komutundan son `bot ... in game` günlüğüne) ≤ 6 sn (kapısız ≈ 2,5 sn tabanı + ≤ 0,3 sn/bot), 12 bot tek `spawn` ile ≤ 10 sn ve `spawn complete: 12/12 in game, 0 failed`; `FAILED` oturum yok; `ENABLED=0` ile sunucu açılışı değişmez (bot günlüğü satırı yok)

### KI-DEG-01 kapatma kriteri (Claude uygular; plan DOĞRULANDI + birleştirme sonrası)

KI-DEG-01 yalnızca şu **hepsi** sağlanınca `KAPANDI` yapılır: (1) K1-K8 ✔ ve K9-K11 ✔ (12/12 simetri, ≥ 3 sn aralık olmadan); (2) bu plan `gece/2026-10-02`'ye birleşmiş (`KAPANDI`) ve birleşik ikiliyle K9 özet koşusu (en az 1 tur, 3 bot, tek `spawn`) tekrarlanmış; (3) `docs/KNOWN_ISSUES.md` KI-DEG-01 satırındaki "geçici çözüm ≥ 3 sn" ifadesi kaldırılıp çözüm ve kanıt (plan:K9) yazılmış, `docs/13` §4.3 "Spawn" satırına el sıkışma serileştirmesi eklenmiş, `docs/STATUS.md` güncel; (4) **kalan risk ayrı kayıt:** gerçek oyuncu girişi ile bot (ya da gerçek-gerçek) girişi çakışması sunucu özelliği olarak sürer (§8); bu KI-DEG-01'in parçası sayılmaz, ayrı bir `KI-` satırı olarak açılır. Bu plan uygulanana ve doğrulanana kadar "≥ 3 sn aralıklı doğuş" geçici çözümü geçerlidir ve **kalıcı kapanış sayılmaz**.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status                  # [UP] varsa: ./tools/run-servers.sh stop
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release && ./tools/run-tests.sh Debug
python3 tools/check-perception-contract.py     # RESULT: PASS
git diff --stat gece/2026-10-02...bot/F4-55
git diff gece/2026-10-02...bot/F4-55 -- GameServer | grep -E '^-[^-]'      # beklenen: bos (ya da raporda aciklanmis)
git diff gece/2026-10-02...bot/F4-55 | grep '^+' | grep -E 'g_pMain|GetUserPtr|GetRegion|m_RegionUserArray|->m_pUser'   # beklenen: bos
git diff --check gece/2026-10-02...bot/F4-55
grep -n -a "HandshakeGateOpen\|handshakeWaitTicks\|spawn handshake" GameServer/Bot/BotManager.cpp GameServer/Bot/BotManager.h BotCore/SpawnGate.h
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3: ASCII yeni dosyalar, CRLF, tab, Allman, İngilizce yorum; kodlamayı bozma. Konsola `printf` yok; günlük yalnızca `WriteBotLog`.
- **Dürüstlük:** birim testler yalnızca §2'deki sunucu modelini ve kapı kuralını sınar; "gerçek sunucuda simetri" iddiası **yalnızca** Claude'un K9-K11 koşusuyla yapılır. Uygulayıcı raporunda "oyun içinde doğrulandı" yazma.
- **Kalan risk (kapsam dışı, sunucu özelliği):** aynı el sıkışma penceresi gerçek oyuncular arasında ve gerçek oyuncu ile bot arasında da vardır (gerçek istemcide yükleme ekranı `LOADED_DELAY_MS`'den uzundur, pencere daha geniştir); bu planın kapısı yalnızca bot oturumlarını birbirine karşı serileştirir. Bunu kapatmak sunucu mekaniği değişikliğidir (`docs/03` önce, proje sahibi kararı) ve seçenek (d)'dir; bu planda yapılmaz.
- Seçenek (c) (bilinmeyen kimlikli `WIZ_MOVE` → `REQ_USERIN`) ileride ayrı plan olabilir; önkoşul gerçek istemcinin bu davranışının ölçülmesidir (T-PERC-01), bu planda kod yazma.
- F4-60 (HAZIR, `BotSession.cpp/.h`, `BotManager.cpp` `CommandSnap`, `Perception.h`) ile çakışma: F4-55 `BotManager.cpp`'de yalnızca `TickSessions` ve `BotManager.h`'de bir üye ekler; F4-60 aynı dosyada yalnızca `CommandSnap`'e dokunur: farklı fonksiyonlar, sıra bağımsız birleştirilebilir (çakışma çıkarsa elle çözülür). `Perception.h`/`PerceptionTests.cpp`'e F4-55 dokunmaz.
- Kapı bekleme süresi `ScenarioRunner` `SCENARIO_PREPARE_TIMEOUT_MS` (60 sn) altında kalır: 16 bot ≈ 5 sn, 100 bot ≈ 30 sn (üst sınır tahmini; Claude K11'de 12 botla ölçer, değer plana yazılır).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-55` (taban `gece/2026-10-02` @ `2e85af8`). Uygulama commit'i `06445fb`; Durum/rapor commit'i bu dosyayla aynı commit.
- Değişen dosyalar ve neden:
  - `BotCore/SpawnGate.h` (yeni): `HandshakeGateOpen(int)` + `HandshakeMissesUnit(op1X, op2X, op2Y)` saf mantık, yalnızca `<cstdint>`, ASCII + CRLF.
  - `Tests/BotCoreTests/SpawnGateTests.cpp` (yeni): üç birim testi (kural, F4-54 asimetri modeli, kapılı/kapısız tick döngüsü).
  - `BotCore/BotCore.vcxproj` / `Tests/BotCoreTests/BotCoreTests.vcxproj`: yeni dosya satırları (`.filters` yok, doğrulandı).
  - `GameServer/Bot/BotManager.cpp`: `#include "../../BotCore/SpawnGate.h"`; dosya-statik `CountInHandshake`; `TickSessions` `PHASE_WAIT_SELECT` dalında `GameStart(1)` öncesi kapı denetimi; toplu özetten sonra `spawn handshake` log satırı.
  - `GameServer/Bot/BotManager.h`: `uint32 m_handshakeWaitTicks;` üyesi + kurucuda `m_handshakeWaitTicks(0)`.
- Derleme sonucu: `./tools/build.sh Release` rc=0; `BotManager.cpp` ve `SpawnGateTests.cpp` uyarısız derlendi; kalan uyarılar eski (`GameServerDlg.cpp` C4834/C4267, `UpgradeHandler.cpp` C4789). Son satırlar: `Kodun üretilmesi tamamlandı` / `proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe`. `./tools/build.sh Debug` rc=0 (aynı eski uyarılar).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (`Release` rc=0, yeni uyarı yok).
  - K2 ✔ (`Debug` rc=0, yeni uyarı yok).
  - K3 ✔ (`run-tests.sh Release` ve `Debug`: `267 tests, 0 failed`; üç `SpawnGate_*` testi `[ OK ]`). Dal açılışında T0=264 (plan 259 bekliyordu; dalda F4-60/F5-60 birleşik), sonuç T0+3.
  - K4 ✔ (`GameServer` farkı yalnızca `BotManager.cpp`/`.h`; `-` satırları yalnızca kurucu başlatıcı listesinde; `shared/`, `AIServer/`, bot dışı `GameServer/*.cpp` farkı yok).
  - K5 ✔ (yeni satırlarda `g_pMain|GetUserPtr|GetRegion|m_RegionUserArray|->m_pUser` yok; `check-perception-contract.py` `RESULT: PASS`, R1-R5 ihlal 0, `SpawnGate.h` R4 temiz).
  - K6 ✔ (yeni ini/komut/thread/paket yok; `SELECT_SETTLE_MS`/`LOADED_DELAY_MS`/`PHASE_TIMEOUT_MS` değişmedi; `ENABLED=0` yolu `m_sessions.empty()` erken dönüşüyle aynı).
  - K7 ✔ (yeni dosyalar ASCII + CRLF, izlenenlerde `git diff --check` boş; iki vcxproj yeni satırları içeriyor).
  - K8 ✔ (kapı kodu: `BotManager.cpp:3024-3031`; log: `BotManager.cpp:3389-3391`; kilitlenmez gerekçesi: `PHASE_WAIT_LOADED` her durumda `LOADED_DELAY_MS`=200 ms sonra koşulsuz `GameStart(2)` gönderip `PHASE_IN_GAME`/`FailSession`'a çıkar, dolayısıyla `CountInHandshake` en fazla bir oturum + 200-300 ms için >0 olur).
  - K9/K10/K11: çalışma zamanı koşusu Claude'da (§6 notu); uygulayıcı oyun içi doğrulama yapmadı.
- Plandan sapmalar ve gerekçeleri:
  1. `SpawnGate_OverlapReproducesOneWayView` testinde plan `op1 = {0, 100, 200}`, `op2 = op1 + 200` diyordu; ancak katı pencere (`op1(X) < op2(Y) < op2(X)`) ve 100 ms aralıkta `op1(2) == op2(0) == 200` eşitliği `2!0` yönünü düşürüp sonucu `{1!0, 2!1}` yapar; planın belgelediği F4-54 koşu 1 kümesi ise `{1!0, 2!0, 2!1}`. Plan içi bu tutarsızlık nedeniyle test girdisi el sıkışma aralığı **250 ms** seçildi (`op2 = {250, 350, 450}`); bu, planın *beklenen çıktısını* (üç eksik yön, hiçbir çiftte iki yön) birebir üretir. Kod (`SpawnGate.h`) plandaki gibi katı `<` kaldı; değişen yalnızca testin zaman damgalarıdır.
  2. `BotManager.h` kurucu başlatıcı listesi bir satır kaydırılarak `m_handshakeWaitTicks(0)` eklendi (K4'te izinli `-` satırı gerekçesi).
- Açık sorular:
  1. Sapma 1'deki zamanlama seçimi (250 ms) onaylanıyor mu? Plan metnindeki `+200` ile üretilen küme planın kendi beklediği kümeyle çelişiyor; hangisinin esas alınacağı netleşirse test buna göre sabitlenir.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar:
- İncelenen:
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | | |

- Bulgular (önem sırasıyla):
- Düzeltme talimatı (DeepSeek'e aynen verilecek):
