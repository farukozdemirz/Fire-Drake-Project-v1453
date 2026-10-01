# 02 — Depo Analizi ve Entegrasyon Haritası

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01
> İncelenen depo: `ko4life-net/Fire-Drake-Project-v1453`, branch `main`, commit `0f520272ae1f11472623d62bff76fff98562e7b3` (2020-10-12)
> Bu doküman **depo yapısının, çalışma modelinin ve bot entegrasyon noktalarının tek kaynağıdır.** Oyun kurallarının (saldırı, skill, pot, ölüm vb.) ayrıntısı [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)'tedir; burada yalnızca "nerede ve nasıl bağlanır" anlatılır.
> Etiketler: `[D]` depo kodunda doğrulandı · `[V]` yerel sürüm verisiyle (DB/harita) doğrulandı · `[S]` dış kaynak · `[B]` başka sürüm · `[Ö]` öneri · `[A]` açık/çalışma zamanı testi gerekli. Satır referansları `dosya:satır` biçimindedir ve bu commit'e aittir.

---

## 1. İnceleme bağlamı

| Konu | Bulgu | Etiket |
|---|---|---|
| Upstream durumu | `git ls-remote` ile 2026-10-01'de kontrol edildi: upstream `main` = `0f52027`. Depoda toplam 11 commit var; son anlamlı değişiklikler MSVC derleme ve yapılandırma temizliği (2020-10-11/12). | `[D]` |
| Açık PR | Upstream'de birleştirilmemiş PR #10 var (`refs/pull/10/head` = `be26438`). Bu inceleme PR #10'u **kapsamaz**. | `[D]` |
| Fork | Yerel `origin` = `farukozdemirz/Fire-Drake-Project-v1453`, upstream ile aynı commit. Çalışma ağacında yalnızca izlenmeyen `start.md` var. | `[D]` |
| Lisans | GPLv3 (`LICENSE`). Fork'ta yapılan değişiklikler dağıtılırsa (ikili dahil) kaynak kodun aynı lisansla sunulması gerekir. Sunucuyu yalnızca kendi makinende çalıştırmak dağıtım sayılmaz. Bu bir hukuki görüş değildir. | `[D]`/`[Ö]` |
| Sürüm sabiti | [`shared/version.h:3`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/version.h#L3) → `__VERSION 1453`. Kripto anahtarı bu sabite bağlıdır ([`shared/JvCryption.cpp:6-15`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/JvCryption.cpp#L6-L15)). | `[D]` |
| Depoda olmayanlar | Veritabanı (SQL şeması, stored procedure'ler, veriler), harita dosyaları (`*.smd`, `*.aievt`), quest Lua betikleri (`Quests/*.lua`), `.ini` dosyaları, istemci. Bunlar ayrı paketlerden gelir (`start.md` §1). | `[D]` |
| Yerel çalışma ortamı | `/mnt/c/dev/fdp/` altında derlenmiş sunucular, `Map/` (24 SMD + 8 aievt), `Quests/` (114 Lua), istemci; SQL Server Express (MSSQL14) üzerinde `FDP_kn_online` veritabanı. Sunucu loglarına göre 2026-10-01'de bu ortamda sunucu çalıştırılmış ve bir test hesabı Moradon'da canavar öldürmüş. **Bu araştırmada sunucu çalıştırılmadı.** | `[V]` |
| Yerel veri düzeltmesi | Yerel DB'de `MAGIC_BAK_etc` tablosu var; `start.md` §4'teki `UPDATE MAGIC SET Etc = 0 WHERE Etc = 1` düzeltmesinin bu ortamda uygulandığını gösteriyor. | `[V]` |

## 2. Depo yapısı

| Klasör | İçerik | Bot projesiyle ilgisi |
|---|---|---|
| `GameServer/` | Oyun sunucusu: oyuncu (`CUser`), savaş, skill, party, chat, zone, item, DB erişimi, GM komutları | **Botların yaşayacağı ve aksiyonlarının işleneceği yer** |
| `AIServer/` | NPC/canavar yapay zekâsı, NPC durum makinesi, A* pathfinding, oda olayları | Pathfinding ve takip mantığı referans/port kaynağı; botlar burada yaşamamalı (§9) |
| `LogInServer/` | Sunucu listesi ve sürüm/yama akışı | Sunucu tarafı botlar için gerekmez |
| `shared/` | Soket altyapısı, paket tanımları, kripto, SMD harita yükleyici, DB set sınıfları, global sabitler | Harita verisi (`SMDFile`), paket sabitleri, oturum yöneticisi |
| `N3BASE/` | Harita çarpışma verisinin (shape/collision) yükleyicisi | Görüş hattı için potansiyel veri kaynağı ([12](12_NAVIGATION_AND_POSITIONING.md)) |
| `scripting/Lua/` | Lua 5.2.3 | Quest betikleri; bot döngüsü için uygun değil (§12) |
| `db/` | Elle yapılan veri düzeltmelerinin tekrar çalıştırılabilir, geri alınabilir SQL betikleri (`001_magic_etc_fix.sql`: `MAGIC.Etc = 1 → 0`, ADR-0003); kullanım `db/README.md` | Her temiz kurulumda uygulanır; testlerin ön koşulu (KI-001) |

Derleme `[D]`: `KnightOnlineServer.sln`, projeler `v142` toolset, `stdcpp17`, yalnızca Win32 (`GameServer/proj-GameServer.vcxproj:22,28,69,110`). `start.md`'ye göre `v143` ile de derleniyor `[V]`. `build/` klasörü `.gitignore` kapsamındadır; yereldeki ikililer depoya ait değildir. **Depoda test projesi veya test çatısı yoktur** `[D]`.

### 2.1 Debug ve Release derleme farkları `[D]`

Kaynak: `tools/debug-release-diff.sh` çıktısı (13 koşullu blok, grep ile üretilir) ve her satırın elle okunup doğrulanması; kanıt `plans/F0-01-ortam-dogrulama-araclari.md` (T-ENV-02, 2026-10-01). Debug yapılandırması `_DEBUG`, Release `NDEBUG` tanımlar; `shared/stdafx.h:19-45` Debug'da `DEBUG`'ı da tanımlar.

| Konum | Debug'da | Release'de |
|---|---|---|
| `GameServer/GameServerDlg.cpp:737` | Oturum zaman aşımı kopartması **yok** | `KOSOCKET_TIMEOUT` / `KOSOCKET_LOADING_TIMEOUT` aşımında `Disconnect()` |
| `GameServer/MagicInstance.cpp:267` | **Quest kapısı (`sEtc`) yok:** `Etc ≠ 0` skill'ler quest'siz kullanılır | `Etc ≠ 0` ve GM değilse quest tamamlanmış olmalı (KI-001'in nedeni) |
| `GameServer/User.cpp:4527` ⇐ `GameServer/stdafx.h:7` | `DISABLE_PLAYER_BLINKING` tanımlı; `BlinkStart()` gövdesi derlenmez, **blink hiç yok** | Gövde derlenir; saldırılabilir ulus zone'larında (Ronark) yine blink yok |
| `shared/KOSocket.cpp:93` | Handler `false` dönerse yalnızca `TRACE`; bağlantı kopmaz | `goto error_handler` → bağlantı kopar |
| `GameServer/LuaEngine.h:9` | Lua script önbelleği kapalı (her çağrıda yeniden yükler) | Önbellek açık |
| `shared/Thread.cpp:21,41` | Thread istisnasında `printf` + `ASSERT(0)` | Yalnızca `false` döner |
| `shared/database/OdbcCommand.cpp:73,103` | Her SQL `TRACE` ile yazılır (`USE_SQL_TRACE`) | SQL izi yok |
| `shared/stdafx.h:19` | `ASSERT`, `TRACE`, `DebugUtils.h` etkin | `ASSERT`/`TRACE` boş makro |

**Sonuç:** Bot testleri, doğrulamalar ve değerlendirme maçları **Release** derlemesiyle yapılmalıdır. Debug'da quest kapısı, blink, zaman aşımı ve paket-hata davranışı farklı olduğundan sonuçlar yanıltır.

## 3. Süreç mimarisi

```mermaid
flowchart LR
  Client[1453 istemcisi] -- TCP 15100 --> Login[LogInServer]
  Client -- TCP 15001, JvCryption --> Game[GameServer]
  Game -- TCP 10020, tek bağlantı --> AI[AIServer]
  Game -- ODBC KO_GAME / KO_MAIN --> DB[(SQL Server: FDP_kn_online)]
  AI -- ODBC --> DB
  Login -- ODBC --> DB
  Game -. ./Map/*.smd .-> Maps[(SMD haritalar)]
  AI -. ./Map/*.smd + *.aievt .-> Maps
  Game -. ./Quests/*.lua .-> Quests[(Quest betikleri)]
```

- Başlatma sırası: AIServer → GameServer → LogInServer (`start.md` §7) `[V]`.
- **Çalıştırma komutu** (F0-02, `main`'de) `[V]`: `tools/run-servers.sh start [--config Release|Debug]`, `stop [--force]`, `status`. Betik sırayı kendisi uygular (AIServer → GameServer → LogInServer), her sunucuyu yalnızca izinli klasörden çalışan süreç olarak tanır (`C:\dev\fdp\server` veya `build\bin\x86-<Config>\Server`), ve GameServer'ın AIServer'a bağlandığını (`ai≥1`) doğrulamadan `UP` saymaz. Çıkış kodu: 0 başarı, 1 başarısız/reddedildi, 2 kullanım/yapılandırma hatası. Açık istemci varken `stop` reddeder (`--force` gerekir). Ayrıntı: `plans/F0-02-sunucu-calistirma-betigi.md`.
- Ölçülen süreler (Release, 2026-10-01, Doğrulama Tur 2) `[V]`: her sunucu 1–5 sn'de hazır, `start` toplam ~13–17 sn (her çağrı ~1–1,5 sn PowerShell gecikmesi içerir), `stop` ~9 sn. Betiğin kendi yazdığı `(<n> sn)` değeri `SECONDS` çözünürlüğündedir. Bilinen: KI-007 (betik dışında elle açılmış sunucular nazikçe kapanmaz, `stop` zorla kapatır), KI-008 (`stop` satırı hep `0 sn` yazar).
- GameServer, AIServer'a istemci olarak bağlanır. NPC'lerin sahibi ve beyni AIServer'dır, **hasar hesabı ise GameServer'dadır** (§9).

## 4. GameServer çalışma modeli

### 4.1 Başlatma

[`GameServer/main.cpp:13-60`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/main.cpp#L13-L60) → `CGameServerDlg::Startup` ([`GameServer/GameServerDlg.cpp:76-208`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L76-L208)) `[D]`:

1. INI okunur ve **5 zamanlayıcı thread'i başlatılır** ([`GameServer/GameServerDlg.cpp:376-380`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L376-L380)). Zamanlayıcılar tablolar yüklenmeden başlıyor; erken bir yarış durumu ihtimali var `[A]`.
2. Oyuncu soket dinleyicisi (`MAX_USER` = 3000 oturum) ve aynı IOCP üzerine AI soketi kurulur.
3. DB ajanı ve DB thread'i başlar.
4. ~45 tablo yüklenir: ITEM, SET_ITEM, ITEM_UPGRADE, ITEM_OP, MAGIC, MAGIC_TYPE1..9, COEFFICIENT, LEVEL_UP, START_POSITION(_RANDOM), ZONE_INFO, EVENT, K_OBJECTPOS, vb.
5. Her zone için SMD haritası yüklenir ([`GameServer/LoadServerData.cpp:381-405`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/LoadServerData.cpp#L381-L405)).
6. Log dosyaları açılır (`Logs/DeathUser|DeathNpc|Chat|Cheat_*.log`), Lua motoru başlatılır, AIServer'a bağlanılır, GM/konsol komut tabloları kurulur, sunucu çalışmaya başlar.

### 4.2 Thread'ler

| Thread | Konum | Periyot | Görev |
|---|---|---|---|
| TimeThread | [`shared/TimeThread.cpp:26-43`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/TimeThread.cpp#L26-L43) | 1000 ms | Global `UNIXTIME` (**1 sn çözünürlük**) |
| Timer_CheckGameEvents | [`GameServer/GameServerDlg.cpp:631-641`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L631-L641) | 1 sn | Savaş/tapınak olayları |
| Timer_BifrostTime | [`GameServer/GameServerDlg.cpp:643-712`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L643-L712) | 60 sn | Bifrost |
| Timer_UpdateGameTime | [`GameServer/GameServerDlg.cpp:714-726`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L714-L726) | 6 sn | Oyun saati, hava, ranking, AI bağlantı kontrolü |
| **Timer_UpdateSessions** | [`GameServer/GameServerDlg.cpp:728-758`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L728-L758) | **30 sn** | Zaman aşımı kopartması ve oyundaki her kullanıcı için `CUser::Update()` |
| Timer_UpdateConcurrent | [`GameServer/GameServerDlg.cpp:760-776`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L760-L776) | 60 sn | Eşzamanlı kullanıcı sayısı |
| **IOCP worker (tek)** | [`shared/SocketMgr.cpp:44-55`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SocketMgr.cpp#L44-L55) | olay | Tüm istemci paketleri **ve** AIServer paketleri bu tek thread'de seri işlenir |
| Accept | [`shared/ListenSocketWin32.h:64-102`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/ListenSocketWin32.h#L64-L102) | bloklayan | Yeni bağlantı |
| SocketCleanupThread | [`shared/SocketMgr.cpp:11-28`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SocketMgr.cpp#L11-L28) | 100 ms | Kopan oturumları boşa çıkarır |
| DatabaseThread | [`GameServer/DatabaseThread.cpp:31-154`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/DatabaseThread.cpp#L31-L154) | kuyruk | Seri DB isteği işleme |
| ConsoleInputThread | [`GameServer/ConsoleInputThread.cpp:20-56`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ConsoleInputThread.cpp#L20-L56) | 100 ms | `/` konsol komutları |

### 4.3 `CUser::Update()` ve zamanlanmış etkiler

`CUser::Update()` ([`GameServer/User.cpp:472-533`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L472-L533)) şunları işler `[D]`: HP/MP doğal yenilenmesi, DoT/HoT tick'leri, Type4 buff/debuff süresi dolumu (**çağrı başına en fazla bir buff**, [`GameServer/User.cpp:3443-3460`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L3443-L3460)), Type6/Type9 süreleri, blink bitişi, rival süresi, 180 sn'de bir otomatik kayıt, item süre taraması.

`Update()` iki yerden çağrılır `[D]`:
- Kullanıcının gönderdiği **her paketten sonra** ([`GameServer/User.cpp:465`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L465)),
- 30 saniyede bir `Timer_UpdateSessions` ([`GameServer/GameServerDlg.cpp:752-753`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L752-L753)).

**Bot için sonuç:** Paket göndermeyen soketsiz bir varlıkta buff'lar, DoT'lar ve yenilenme 30 saniyeye kadar donar. Bot yöneticisi her bot için `Update()`'i düzenli (öneri: ≥ 1 Hz) çağırmalıdır `[Ö]`. Bot aksiyonları `HandlePacket` üzerinden verilirse her aksiyondan sonra `Update()` zaten çağrılır.

### 4.4 Kilitler ve thread güvenliği

- Tüm kilitler `std::recursive_mutex` + `Guard` RAII sınıfıdır ([`shared/stdafx.h:57-66`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/stdafx.h#L57-L66)) `[D]`.
- Oturum haritası `KOSocketMgr::m_lock` ile korunur ([`shared/KOSocketMgr.h:52-61`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/KOSocketMgr.h#L52-L61)), ancak **24 yerde aktif oturum haritası kilitsiz kopyalanır** (ör. `GameServer/GameServerDlg.cpp:732,858,895`) `[D]`. Çökme riski `[A]`.
- Bölgeler: `C3DMap::m_lock` → `CRegion::m_lock`; bölgeler birim **ID**'lerini `std::set<uint16>` olarak tutar ([`GameServer/Region.h:9-10`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Region.h#L9-L10)) `[D]`.
- `CUser` başına genel bir mutex **yoktur**; yalnızca buff (`m_buffLock`), item bonus, ok ve saved-magic kilitleri vardır (`GameServer/Unit.h:279,322`) `[D]`.
- `CUser` durumu IOCP thread'i dışında da değiştirilir: 30 sn zamanlayıcı, DB thread'i (`SelectCharacter` DB thread'inde çalışır), olay zamanlayıcıları, konsol `[D]`.
- Soket yazma `m_writeMutex` ile korunur; `Send()` her thread'den güvenle çağrılabilir `[D]`.

**Bot için sonuç `[Ö]`:** Bot kararları ayrı bir thread'de üretilebilir, ancak **aksiyonlar IOCP worker thread'inde** çalıştırılmalıdır. Böylece bot aksiyonları gerçek oyuncu paketleri ve AIServer paketleriyle aynı sırada seri işlenir. Kod tabanı bunun için uygun bir mekanizma içeriyor: özel bir `SocketIOEvent` tipi ([`shared/SocketDefines.h:3-9`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SocketDefines.h#L3-L9)) eklenip `PostQueuedCompletionStatus` ile kuyruğa bırakılabilir; bu çağrı kapanış için zaten kullanılıyor ([`shared/SocketMgr.cpp:151-154`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SocketMgr.cpp#L151-L154)). Ayrıntı ve karar: [13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md) ve ADR-0005.

## 5. Oyuncu varlığı: `Unit` → `CUser`

- Sınıf zinciri: `Socket` → `KOSocket` → `CUser : public Unit, public KOSocket` ([`GameServer/User.h:110`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.h#L110)) `[D]`.
- **Kullanıcı kimliği oturum indeksidir:** `GetID()` = `GetSocketID()` ([`GameServer/User.h:113`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.h#L113)). 3000 oturum başlangıçta önceden ayrılır ve kimlikleri 0..2999'dur ([`shared/KOSocketMgr.h:75-80`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/KOSocketMgr.h#L75-L80)) `[D]`.
- `GetUserPtr(id)` yalnızca **aktif oturum haritasında** arar ([`GameServer/GameServerDlg.cpp:549`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L549); [`shared/KOSocketMgr.h:52-61`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/KOSocketMgr.h#L52-L61)). `GetUnitPtr` 10000 altını kullanıcı, üstünü NPC sayar ([`GameServer/GameServerDlg.cpp:557-563`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L557-L563)) `[D]`. GameServer'da `GetUserPtr` çağıran 114 satır var; saldırı hedefi, skill caster/target, alan skill'leri, party, loyalty, DoT kaynağı, Lua ve DB dağıtıcısı bu aramaya dayanır `[D]`.
- `Unit` ortak alanları: konum, zone, seviye, ulus, toplam saldırı/AC/isabet/kaçınma, direnişler, `m_durationalSkills[40]` (DoT/HoT), `m_buffMap` (Type4), durum bayrakları ([`GameServer/Unit.h:215-341`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.h#L215-L341)) `[D]`.
- `Unit` sanal arayüzü: `GetDamage`, `HpChange`, `MSpChange`, `isHostileTo`, `CanAttack`, `isAttackable`, `CanCastRHit`, `OnDeath` ([`GameServer/Unit.h:54-206`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.h#L54-L206), [`GameServer/Unit.cpp:855-947`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L855-L947)) `[D]`.
- Hedef seçimi: istemcinin seçtiği hedef yalnızca `WIZ_TARGET_HP` ile `KOSocket::m_targetID`'ye yazılır ([`GameServer/User.cpp:326-333`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L326-L333)). Savaş bu alanı **kullanmaz**; saldırı ve skill paketleri hedef kimliğini kendileri taşır `[D]`.

## 6. Oyuncu yaşam döngüsü ve bağımlılıkları

| Adım | Kod | DB gerektirir | Gerçek soket/istemci gerektirir |
|---|---|---|---|
| Bağlantı kabul, `Initialize` | [`GameServer/User.cpp:19-169`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L19-L169) | – | Evet |
| Sürüm kontrolü, kripto açılışı | [`GameServer/LoginHandler.cpp:3-19`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/LoginHandler.cpp#L3-L19) | – | Evet |
| Hesap girişi (`GAME_LOGIN`) | [`GameServer/LoginHandler.cpp:21-55`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/LoginHandler.cpp#L21-L55), [`GameServer/DBAgent.cpp:96`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/DBAgent.cpp#L96) | Hesap | Paket |
| Karakter seçimi: `LOAD_USER_DATA`, WAREHOUSE, premium, saved magic, `SET_LOGIN_INFO` (IP kullanır) | [`GameServer/CharacterSelectionHandler.cpp:102-247`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterSelectionHandler.cpp#L102-L247), [`GameServer/DBAgent.cpp:324-738`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/DBAgent.cpp#L324-L738) | USERDATA + **WAREHOUSE satırı zorunlu** | `GetRemoteIP()` |
| `SelectCharacter` (DB thread'inde) → `SetUserAbility`, `SetRegion` | [`GameServer/CharacterSelectionHandler.cpp:134-223`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterSelectionHandler.cpp#L134-L223) | – | – |
| `WIZ_GAMESTART` 1: `SendMyInfo`, isim haritasına ekleme, AI'ye bildirim | [`GameServer/CharacterSelectionHandler.cpp:264-278`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterSelectionHandler.cpp#L264-L278), [`GameServer/User.cpp:883-1020`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L883-L1020) | – | İstemci adımı |
| `WIZ_GAMESTART` 2: `INGAME`, `UserInOut(RESPAWN)` | [`GameServer/CharacterSelectionHandler.cpp:279-323`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterSelectionHandler.cpp#L279-L323) | – | İstemci adımı |
| Diğer oyunculara görünme: `WIZ_USER_INOUT` + `GetUserInfo` (isim, ulus, ırk, sınıf, saç/yüz, party lideri bayrağı, 10 ekipman yuvası) | [`GameServer/CharacterMovementHandler.cpp:63-161`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterMovementHandler.cpp#L63-L161) | – | – |
| Zone değişimi (party'den çıkarır), istemcinin "Loaded" onayı | `GameServer/CharacterMovementHandler.cpp:309-500,668-699` | – | İstemci adımı |
| Ölüm → istemcinin `WIZ_REGENE` isteği → `Regene` | [`GameServer/User.cpp:4727-4949`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L4727-L4949), [`GameServer/AttackHandler.cpp:95-240`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L95-L240) | START_POSITION | İstemci adımı |
| Periyodik kayıt (180 sn), çıkışta `UPDATE_USER_DATA` | [`GameServer/User.cpp:507-511`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L507-L511), [`GameServer/DatabaseThread.cpp:437-472`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/DatabaseThread.cpp#L437-L472) | – | Çıkış `Disconnect` ile tetiklenir |

Bot karakteri için DB'de gerekenler `[D]`/`[I]`: hesap (yaklaşıma göre), USERDATA satırı (ulus, ırk, **master sınıf kodu**, seviye 80, statlar, `strSkill`, `strItem`, zone 71, konum, **loyalty > 0**), hesaba ait WAREHOUSE satırı. Ayrıntı: [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) ve [13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md).

## 7. Görünürlük ve yayın

- Bölge boyutu 48 m (`VIEW_DISTANCE`, [`shared/globals.h:19`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/globals.h#L19)); Ronark Land için 43 × 43 bölge `[V]`.
- `Send_Region` 3×3 bölgeye yayın yapar ([`GameServer/GameServerDlg.cpp:918-954`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L918-L954)); efektif görüş 48–96 m `[D]`/`[I]`.
- Bölge değiştiren her birim için yeni pencerede tam `GetUserInfo` yeniden gönderilir `[D]`.
- `Send_Zone` / `Send_All` her pakette tüm oturum haritasını kopyalar ([`GameServer/GameServerDlg.cpp:856-916`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L856-L916)); PK ölüm bildirimi zone geneline gider ([`GameServer/ChatHandler.cpp:341`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ChatHandler.cpp#L341)) `[D]`. 16 botlu testte sorun değil, çok botlu testte ölçülmelidir ([16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md) MET-PERF-05).

## 8. Mekanik alt sistemlerin konumu (ayrıntı 03'te)

| Alt sistem | Giriş noktası | Kural ayrıntısı |
|---|---|---|
| Normal saldırı (R) | `CUser::Attack` [`GameServer/AttackHandler.cpp:4-93`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L4-L93) | [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §3 |
| Skill ve pot | `CMagicProcess::MagicPacket` [`GameServer/MagicProcess.cpp:16-53`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicProcess.cpp#L16-L53) → `MagicInstance::Run` [`GameServer/MagicInstance.cpp:10-147`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L10-L147) | 03 §4–§6 |
| Hasar | `CUser::GetDamage` [`GameServer/Unit.cpp:208-394`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L208-L394); `MagicInstance::GetMagicDamage` [`GameServer/MagicInstance.cpp:2558-2717`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L2558-L2717) | 03 §7 |
| HP değişimi, ölüm | `CUser::HpChange` [`GameServer/User.cpp:1860-1983`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1860-L1983), `CUser::OnDeath` [`GameServer/User.cpp:4727-4949`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L4727-L4949) | 03 §8 |
| Respawn | `CUser::Regene` [`GameServer/AttackHandler.cpp:95-240`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AttackHandler.cpp#L95-L240) | 03 §8 |
| Hareket | `CUser::MoveProcess` [`GameServer/CharacterMovementHandler.cpp:4-54`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterMovementHandler.cpp#L4-L54) | 03 §9, [12](12_NAVIGATION_AND_POSITIONING.md) |
| Stat/yetenek | `SetUserAbility` [`GameServer/User.cpp:2082-2312`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2082-L2312), `LevelChange` [`GameServer/User.cpp:1765-1826`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1765-L1826), `SkillPointChange` [`GameServer/User.cpp:2971-3000`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2971-L3000) | 03 §2, [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) |
| Item kuşanma | `CUser::ItemMove` [`GameServer/ItemHandler.cpp:541-743`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ItemHandler.cpp#L541-L743) | 04 |
| Party | [`GameServer/PartyHandler.cpp`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/PartyHandler.cpp) | 03 §10, [09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) |
| Chat | [`GameServer/ChatHandler.cpp:89-317`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ChatHandler.cpp#L89-L317) | 03 §11 |
| Zone kuralları | [`GameServer/Unit.cpp:1100-1345`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L1100-L1345), [`GameServer/CharacterMovementHandler.cpp:174-293`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterMovementHandler.cpp#L174-L293) | 03 §12 |

## 9. AIServer: rolü ve neden bot katmanı olmamalı

### 9.1 Yapı `[D]`

- Her zone için 2 NPC thread'i (spawn tablosu NPC'leri ve olay NPC'leri), 250 ms tick (`AIServer/NpcThread.cpp:5,150`; [`AIServer/ServerDlg.cpp:190-205`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/ServerDlg.cpp#L190-L205)).
- NPC durum makinesi: LIVE → STANDING → MOVING/ATTACKING → TRACING ↔ FIGHTING (+ HEALING, CASTING, FAINTING) (`AIServer/Npc.cpp:317-640,3765-3875`). `NPC_BACK` hiçbir yerde set edilmiyor; geri dönüş fonksiyonları ölü kod.
- NPC hareket hızı DB'den okunmuyor; `MONSTER_SPEED` = 1500 ms adım aralığı (`AIServer/NpcTable.h:3,63`).
- `FindEnemyExpand` yorumunun aksine **en uzak** düşmanı seçiyor ([`AIServer/Npc.cpp:1598-1600`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/Npc.cpp#L1598-L1600)); `FindEnemyRegion` X/Z karıştırıyor (`AIServer/Npc.cpp:1452,1454`).
- Hedef takibi A* kullanmıyor: düz çizgi, harita kontrolü yok ([`AIServer/Npc.cpp:2120-2122`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/Npc.cpp#L2120-L2122), [`AIServer/Npc.cpp:3286-3341`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/Npc.cpp#L3286-L3341)).
- A* ([`AIServer/PathFind.cpp`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/PathFind.cpp)): 4 m ızgara, linked-list açık liste (O(n²)), işaretsiz sezgisel ([`AIServer/PathFind.cpp:196`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/PathFind.cpp#L196)) ve **ters yürünebilirlik**: `MAP::IsMovable` olay değeri 0 olan hücreyi geçilebilir sayıyor ([`AIServer/MAP.cpp:124-127`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/MAP.cpp#L124-L127)), oysa Ronark Land verisinde 0 = engelli `[V]` ([12](12_NAVIGATION_AND_POSITIONING.md) §2).
- [`AIServer/Party.cpp`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/Party.cpp) canavar grubu değil, oyuncu party'lerinin EXP paylaşımı için aynası.
- Kuşatma slotları: yakın dövüş NPC'leri bir kullanıcının etrafındaki 8 pozisyondan birini alıyor ([`AIServer/AIUser.cpp:71-121`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/AIUser.cpp#L71-L121)), botlara uyarlanabilir.

### 9.2 Hasar otoritesi `[D]`

- NPC → oyuncu: AIServer `AG_ATTACK_REQ` gönderir, hasarı GameServer `CNpc::GetDamage` ile hesaplar ([`GameServer/AISocket.cpp:237-275`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/AISocket.cpp#L237-L275), [`GameServer/Unit.cpp:474-590`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L474-L590)).
- Oyuncu → NPC: GameServer hesaplar, AIServer'a HP değişimi bildirir.
- NPC skill'leri de GameServer'daki `MagicInstance` hattından geçer ([`AIServer/NpcMagicProcess.cpp:6-35`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/NpcMagicProcess.cpp#L6-L35)).

### 9.3 NPC tabanlı bot engelleri `[D]`

1. NPC görünüm paketi ([`GameServer/Npc.cpp:138-155`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Npc.cpp#L138-L155)) yalnızca model kimliği, iki silah, isim, ulus ve seviye taşır; **ırk, sınıf, yüz/saç, zırh yuvaları yok**. Oyuncu gibi görünemez.
2. Party üyeleri oturum kimliği ile tutulur; party skill'leri NPC'yi reddeder ([`GameServer/MagicInstance.cpp:826-831`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L826-L831)). Party chat yok.
3. PvP hasar formülleri yalnızca hedef `isPlayer()` olduğunda uygulanır (`GameServer/Unit.cpp:245-258,376-387`). NP ödülü, ölüm cezaları farklı.
4. NPC'ler Type 5/6/9 skill kullanamaz, MP'leri yoktur.
5. `CNpc::isHostileTo` hedefi her durumda kullanıcıya cast ediyor ([`GameServer/Unit.cpp:1180-1190`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L1180-L1190)); bot ile guard etkileşiminde tanımsız davranış riski.
6. 1,5 sn adım aralığı ve duvardan geçen takip PvP için çok kaba.

## 10. Bot temsil katmanı seçenekleri

| Ölçüt | (A) Sunucu içi soketsiz `CUser` | (B) Harici headless istemci | (C) AIServer NPC'si |
|---|---|---|---|
| Kurallara sadakat | Yüksek: aynı handler'lar (`Attack`, `MagicInstance`, `ItemMove`, `PartyHandler`) | En yüksek: gerçek istemci yolu | Düşük: NPC formülleri |
| Oyunculara görünüm | Gerçek oyuncu (`WIZ_USER_INOUT`) | Gerçek oyuncu | NPC modeli |
| Party / party chat | Evet (oturum slotunda ise) | Evet | Hayır |
| Sunucu değişikliği | Orta: GameServer + `shared` içinde küçük ekler | Yok | Büyük (her iki sunucu + protokol) |
| Ek altyapı | Bot yöneticisi, soketsiz oturum, istemci adımlarının emülasyonu | JvCryption + CRC + sıra numarası + LZF, dünya durumu ayrıştırma, navigasyon, süreç yönetimi | – |
| Bilgi adaleti | Doğrudan nesne erişimi var; gözlem sözleşmesi kodla uygulanmalı ([14](14_LEARNING_AND_ADAPTATION.md) §5.2) | Doğal olarak yalnızca istemci paketleri | – |
| Performans | Ağ yükü yok | Bot başına TCP + şifreleme | İyi |
| Thread riski | Var; aksiyonlar IOCP thread'ine taşınarak azaltılır | Yeni yarış yok | Var |
| Bakım maliyeti | Orta | Yüksek (protokol ayrıştırıcı) | Yüksek |

**Öneri `[Ö]` (ADR-0001):** Ana yol **(A)**: ayrılmış oturum slotlarında soketsiz `CUser` + bot alıcısı. (B), sunucuyu değiştirmeden doğrulama aracı olarak ileride değerlendirilebilir, örneğin (A) botlarının ürettiği zamanlamaların gerçek istemci yoluyla da kabul edildiğini göstermek için. Temel kapsamda yer almaz. (C) gerçekçi PK için uygun değildir.

## 11. Entegrasyon haritası (seçenek A)

Aşağıdaki tablo **önerilen** değişiklik noktalarıdır `[Ö]`; her satırın dayandığı mevcut kod `[D]`'dir. Uygulama sırası [17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md)'dedir.

| Kimlik | Konu | Mevcut kod (dayanak) | Önerilen değişiklik | Risk |
|---|---|---|---|---|
| S1 | Ayrılmış oturum havuzu | `shared/KOSocketMgr.h:75-80,105-119` (boş oturumlar, `AssignSocket`) | Başlangıçta N oturumu `m_idleSessions`'tan ayır; bot spawn'da `m_activeSessions`'a ekle. Kimlikler < 3000 kalır, AIServer kabul eder ([`AIServer/ServerDlg.cpp:549-563`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/ServerDlg.cpp#L549-L563)). | Gerçek oyuncu kapasitesi N azalır |
| S2 | Bot alıcısı (bot sink) | [`shared/KOSocket.h:33-34`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/KOSocket.h#L33-L34) (`Send`/`SendCompressed` sanal); havuz `CUser` nesnelerini önceden `new T(i, this)` ile oluşturur ([`shared/KOSocketMgr.h:75-80`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/KOSocketMgr.h#L75-L80)) | `CUser`'a `IBotSink* m_botSink` alanı; `CUser`, `Send`/`SendCompressed`'i geçersiz kılar ve bot oturumlarında paketleri bota "algı" olayı olarak verir (HP değişimi, ölüm, party daveti, magic sonucu). Alt sınıf yerine alan seçildi; havuz koduna dokunulmaz ([13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md) §4.2) | Gerçek oyuncu oturumlarında `m_botSink` her zaman `nullptr` olmalı (AC-ARCH-04) |
| S3 | Bot girişi | [`GameServer/CharacterSelectionHandler.cpp:102-323`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterSelectionHandler.cpp#L102-L323), [`GameServer/DatabaseThread.cpp:247-268`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/DatabaseThread.cpp#L247-L268) | Hesap/karakter kimliğini ata, `WIZ_SEL_CHAR` isteğini DB kuyruğuna koy, `GameStart(1)`, `GameStart(2)` adımlarını taklit et | `SET_LOGIN_INFO` IP kullanır (S7) |
| S4 | Bot çıkışı | `GameServer/User.cpp:174-208,866-878`; `Disconnect` soketsizde etkisiz ([`shared/Socket.cpp:97-120`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/Socket.cpp#L97-L120)) | `OnDisconnect` + `LogOut` eşdeğerini açıkça çağır; slotu DB kaydı bittikten sonra iade et | Çıkış kaydı yarış durumu ([`GameServer/DatabaseThread.cpp:62-72`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/DatabaseThread.cpp#L62-L72)) |
| S5 | Zaman aşımı | [`GameServer/GameServerDlg.cpp:737-750`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L737-L750) | Bot tick'inde `m_lastResponse` yenile veya botları zaman aşımı kontrolünden muaf tut | – |
| S6 | Aksiyonların paket olarak işlenmesi | [`GameServer/User.cpp:217-467`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L217-L467) (`HandlePacket`), [`shared/SocketMgr.cpp:58-89`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SocketMgr.cpp#L58-L89) | Bot aksiyonlarını `WIZ_MOVE`, `WIZ_ATTACK`, `WIZ_MAGIC_PROCESS`, `WIZ_ITEM_MOVE`, `WIZ_STATE_CHANGE`, `WIZ_PARTY`, `WIZ_CHAT`, `WIZ_REGENE`, `WIZ_ZONE_CHANGE` paketleri olarak oluşturup IOCP thread'inde `HandlePacket` ile çalıştır | `HandlePacket` kripto/hesap/karakter kapılarını kontrol eder ([`GameServer/User.cpp:223-272`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L223-L272)); bot için bu bayraklar doğru set edilmeli |
| S7 | Bot muafiyetleri | [`GameServer/CharacterSelectionHandler.cpp:233-247`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/CharacterSelectionHandler.cpp#L233-L247), ranking/ödül ([`GameServer/GameServerDlg.cpp:3349-3370`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L3349-L3370)) | Sabit IP veya `SET_LOGIN_INFO` atlama. Botlar ranking, ödül ve duyurulara normal oyuncu gibi dahildir (K-9, ADR-0012) | Ranking/ödüllerin botlarla dolması kabul edildi |
| S8 | `Update()` sıklığı | `GameServer/User.cpp:465,472-533` | Bot yöneticisi her bot için ≥ 1 Hz `Update()` çağırır (IOCP thread'inde) | – |
| S9 | Bot yöneticisi tick'i | [`GameServer/GameServerDlg.cpp:376-380`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L376-L380) (timer thread listesi) | Karar tick'i için yeni zamanlayıcı thread'i; aksiyonlar S6 ile IOCP'ye | ADR-0005 |
| S10 | Navigasyon verisi | [`shared/SMDFile.h:69-70`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SMDFile.h#L69-L70) (`C3DMap` friend), [`shared/SMDFile.cpp:75-206`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/SMDFile.cpp#L75-L206) | Olay ızgarası, yükseklik ve çarpışma verisine salt okunur erişim katmanı ([12](12_NAVIGATION_AND_POSITIONING.md)) | Harita belleği paylaşılır, kopyalanmaz |
| S11 | GM/konsol komutları | [`GameServer/ChatHandler.cpp:12-85`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ChatHandler.cpp#L12-L85), [`GameServer/GameServerDlg.cpp:1609-1622`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L1609-L1622) | `+bot` ve `/bot` komut aileleri ([16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md) §8, [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)) | Yalnızca GM |
| S12 | Telemetri | [`GameServer/GameServerDlg.cpp:162-190`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L162-L190) (mevcut log dosyaları) | Ayrı JSONL yazıcı thread'i ([16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md)) | Disk G/Ç |

### 11.1 "Ortak mekaniği yeniden uygulamama" ilkesi

Botlar oyun kurallarını kendi kodlarında **yeniden hesaplamaz**; aksiyonu, gerçek istemcinin göndereceği paketle aynı biçimde oluşturur ve sunucunun mevcut handler'ına verir (S6). Bu sayede:

- Menzil, cooldown, MP/item maliyeti, sınıf ve skill puanı kontrolleri, hasar formülleri ve buff kuralları **tek yerde** (mevcut sunucu kodunda) kalır.
- Sunucudaki bir kural düzeltildiğinde botlar da otomatik olarak düzeltilmiş kurala tabi olur.

Botun **kendi tarafında** tutması gereken tek mekanik bilgi, sunucunun uygulamadığı ama gerçek istemcinin uyguladığı sınırlardır: cast süresi, silah gecikmesi, hareket hızı ve yavaşlatma etkileri, ayakta kullanılan skill'ler öncesi durma. Bunlar [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §13'teki "istemci tarafı sınırlar" tablosundan okunur ve `BotFairnessGuard` tarafından uygulanır ([13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md)). Bot ayrıca aksiyon göndermeden önce **ön kontrol** yapabilir (menzil, kaynak); bunun amacı geçersiz deneme sayısını azaltmaktır, sunucu kontrolünün yerine geçmez.

## 12. Lua motoru

Tek `lua_State`, global mutex altında, yalnızca quest/NPC diyaloglarından çağrılır (`GameServer/LuaEngine.cpp:184-187,329`; [`GameServer/QuestHandler.cpp:252-279`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/QuestHandler.cpp#L252-L279)) `[D]`. Hareket, hedef seçimi, saldırı, oyuncu skill'i, algı veya zamanlayıcı bağlaması yok. **Bot döngüsü C++'ta olmalı** `[Ö]`. Lua ileride yalnızca politika parametrelerinin betiklenmesi için düşünülebilir; temel kapsamda değil.

## 13. GM ve konsol komutları (test için)

Mevcut ve test için yararlı `[D]` ([`GameServer/ChatHandler.cpp:53-85`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ChatHandler.cpp#L53-L85) tablo):

| Komut | İşlev | Not |
|---|---|---|
| `+give_item İsim ItemID [adet]` | Item verme | Ekipman ve pot hazırlığı |
| `+zonechange Zone` | GM'i zone başlangıç noktasına taşır | x/z verilemez |
| `+monsummon SID` / `+npcsummon SID` | GM konumunda NPC/canavar | Test arenası kirlenmesine dikkat |
| `+monkill` | Seçili NPC'yi öldürür | Arena temizliği |
| `+np_change İsim ±NP` | NP (loyalty) değişimi | Ronark'a girişte NP > 0 şartı |
| `+exp_change İsim ±EXP` | EXP; çağrı başına en fazla 1 seviye | Level 80'e çıkarmak için pratik değil |
| `+tp_all Zone [Hedef]` | Zone'daki oyuncuları çıkarır | Arena sıfırlama |
| `WIZ_OPERATOR` (istemci GM paketi) | ARREST (kullanıcıya git), SUMMON (kullanıcıyı çek), CUTOFF, BAN, MUTE, saldırı kapama | [`GameServer/User.cpp:3480-3579`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L3480-L3579) |
| `/reload_magics`, `/reload_tables` | Tabloları yeniden yükleme | MAGIC verisi düzeltmeleri sonrası |

Eksik olanlar `[D]`: seviye/stat/skill ayarlama, x/z'ye teleport, HP ayarlama, oyuncu öldürme/diriltme, bot spawn. Bunlar `+bot`/`/bot` test komutlarıyla eklenmeli; **yalnızca test modunda ve telemetride işaretli** çalışmalıdır ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)).

## 14. Paket ve kripto katmanı

- Çerçeve: `AA 55 | uint16 len | payload | 55 AA` ([`shared/KOSocket.cpp:36-49`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/KOSocket.cpp#L36-L49)) `[D]`.
- JvCryption; anahtar `__VERSION`'a bağlı; istemci→sunucu her pakette CRC32 + artan sıra numarası ([`shared/KOSocket.cpp:107-138`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/shared/KOSocket.cpp#L107-L138)) `[D]`. **Seçenek (A)'da** paketler doğrudan `HandlePacket`'e verildiği için kripto/CRC katmanı devreye girmez; bu nedenle sunucu tarafında bot için ek bir doğrulama boşluğu oluşmaz, yalnızca bu katmanın kontrolleri atlanır.
- Bot için gerekli opcode'lar ve payload biçimleri [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §14'te listelenir.

## 15. Kod düzeyinde riskler (bot projesini etkileyen)

| Kimlik | Risk | Kanıt | Etki |
|---|---|---|---|
| R-CODE-01 | Kilitsiz oturum haritası kopyaları | `GameServer/GameServerDlg.cpp:732,858,895` | Bot spawn/despawn sıklığı arttıkça çökme ihtimali `[A]` |
| R-CODE-02 | Çıkış kaydının atlanabilmesi | [`GameServer/DatabaseThread.cpp:62-72`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/DatabaseThread.cpp#L62-L72) | Bot karakter verisinin kaybı `[A]` |
| R-CODE-03 | `CUser` başına kilit yok | §4.4 | Bot ve oyuncu aksiyon yarışları; S6 ile azaltılır |
| R-CODE-04 | AIServer A* ters yürünebilirlik | [`AIServer/MAP.cpp:124-127`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/AIServer/MAP.cpp#L124-L127) | Ronark'taki NPC'ler duvarlardan geçebilir veya takılabilir `[A]`; bot navigasyonu bundan bağımsız tasarlanmalı |
| R-CODE-05 | `CNpc::isHostileTo` hatalı cast | [`GameServer/Unit.cpp:1180-1190`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/Unit.cpp#L1180-L1190) | Guard/NPC ile bot etkileşiminde tanımsız davranış `[A]` |
| R-CODE-06 | Zamanlayıcılar tablolar yüklenmeden başlıyor | [`GameServer/GameServerDlg.cpp:376-380`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/GameServerDlg.cpp#L376-L380) | Başlangıç yarışları `[A]` |
| R-CODE-07 | `m_bMaxWeightAmount` başlatılmıyor | [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) | Maksimum ağırlık 0 olabilir `[A]` |

Mekanik düzeydeki hatalar (ör. mage armor yansıması, AC debuff'ının iki kez uygulanması) [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §15'te listelenir.

## 16. Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
| 2026-10-02 | v1.1 | §3'e `tools/run-servers.sh` çalıştırma komutu ve ölçülen açılış/kapanış süreleri eklendi (F0-02) |
