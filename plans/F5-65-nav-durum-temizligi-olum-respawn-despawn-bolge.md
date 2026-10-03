# F5-65: Navigasyon durumunun temizlenmesi: ölüm, respawn, despawn, bölge değişimi/ışınlanma (`NavResetReason`, `BotManager::ClearBotNav`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-65 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F5-62** (`NavDrive`, `m_navDrive`, `AbandonMove`/`StopMove`/`BeginMove` temel `Reset()`) `KAPANDI` olmalı (dolaylı: F5-59, F5-61). **Sıra önerisi:** F5-63 ve F5-64'ten **sonra** (aşağıdaki "Neden TASLAK" madde 2): `Reset` onların eklediği üyeleri (takip, takılma/ilerleme değerlendirici, bütçe kuyruğu girdisi, önbellek anahtarı) kapsamalıdır. Zaten `KAPANDI`: F4-07 (`Regene`), F2-04/F2-05 (despawn/respawn), F5-09/F5-53/F5-57 (sıfırlanacak saf mantık sınıfları). Şemsiye: F5-55 (dilim 7) |
| İlgili gereksinim / kabul | `docs/12` §13.4 (ölüm → doğuş → arena dönüşü), AC-NAV-03 (engelli hücreye giren hareket 0: bayat yolla yürüme guard'a düşmeden önce engellenir), T-NAV-10 (ölüm → respawn → arena dönüşü; yolda takılma 0), `docs/13` §3 (IOCP thread, `OnPacket` her thread'den: yalnız atomik), `docs/03` CLI-14 (ölüm bekleme/respawn); `docs/17` G5 |
| Tahmini büyüklük | S–M (7 dosya; kod az, kanıt birim testte ve ölüm/despawn çalışma zamanı koşusunda) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak) |

---

## Neden TASLAK

F5-62/F5-63/F5-64 kodu depoda yok; bu plan onların **taslak** sözleşmesine dayanır. HAZIR yapmak için:

1. **F5-62 `KAPANDI`.** Varsayılan sözleşme: `BotSession::m_navDrive` (`BotCore::NavDrive`), `NavDrive::Reset()`, `Active()`, `Mode()`; `ActionExecutor::AbandonMove/StopMove/BeginMove` içinde tek satır `m_navDrive.Reset()`.
2. **Reset kapsamı sırası:** `NavDrive`'ın üyeleri F5-63'te (`NavFollower`, `NavProgressAssessor`, `NavStuckMonitor`, `NavStuckPenalties`, hedef gözlemi) ve F5-64'te (`requestedAtMs`, plan zamanı, önbellek anahtarı referansı, faz) büyür; `NavQueryScheduler` (tekil, `NavService`'te) bot başına `Cancel(botId)` ister. **Bu plan F5-63 ve F5-64'ten sonra yazılıp uygulanırsa** `Reset(reason)` ve `NavDriveReset_AllMembers` testi tüm üyeleri kapsar. Daha önce uygulanırsa (yalnız F5-62 sonrası), F5-63/F5-64 uygulayıcıları **kendi eklediği her üyeyi `Reset`'e ve bu testin kontrol listesine eklemekle yükümlüdür** (bu plan o iki planın §3'ünde "Reset yeni üyeleri kapsar" maddesini zaten içerir). Hangi sıranın seçildiği HAZIR yapılırken netleştirilir.
3. Yazım turunda yeniden doğrulanacak referanslar (`gece/2026-10-02` @ `9fc2dfe` üzerinde okundu; **F4-60, F4-55 ve F6-* dilimleri bu dosyalardaki satırları kaydırır / aynı fonksiyonlara dokunur**: HAZIR yapmadan önce çakışma kontrolü şart): `GameServer/Bot/BotManager.cpp:3070-3085` (`TickSessions` ölü bot dalı: `m_deadSeen`/`m_deadSince`, `AbandonMove`, `"move stopped (dead)"` log satırı), `:3354-3372` (`BeginDespawn`: `AbandonMove`, `EndAttack`, `EndCast`, `EndPotion`, `OnDisconnect()`), `:3375-3440` (`PollDespawn`; `ResetForRespawn()` çağrısı `:3435`; `CommandSpawn` `:720` aynı), `:3010-3016` (`m_despawnAfterMs` otomatik despawn); `GameServer/Bot/ActionExecutor.cpp:1895-1950` (`RequestRegene`: `respawned` ⇒ `:1930-1933` `m_deadSeen = false`; yanıt `m_regeneEcho`); `GameServer/Bot/BotSession.cpp:154` / `:218-226` (`OnPacket` `WIZ_REGENE` yanıtı), `:333`/`:399` (`WIZ_DEAD` blokları: **diğer birimlerin** ölümü), `:410-415` (`WIZ_WARP` yanıtı `m_warpEcho`), `:418-` (`ResetForRespawn`), `BotSession.h` (`m_regeneEcho`, `m_warpEcho`: `std::atomic<uint64>`); `GameServer/CharacterMovementHandler.cpp:469` (`WIZ_ZONE_CHANGE` `ZoneChangeTeleport` gönderimi), `:681` (`ZoneChangeLoaded`), `:642-644` (`WIZ_WARP`); `GameServer/Define.h:140` (`ZONE_RONARK_LAND = 71`).
4. **Açık soru (HAZIR öncesi, proje sahibi):** "konum sıçraması" (`PositionJump`) temizliği: botun sunucudaki konumu, **gönderdiği son paketin konumundan** > 1,0 m farklıysa (itme, sunucu geri çekmesi, açıklanamayan ışınlanma) takip/yürüyüş **sonlansın mı**, yoksa yalnızca yeniden mi planlansın? Taslak önerisi: **sonlansın** (durum temizliği; yeniden başlatma karar katmanının/komutun işidir) ve log'a yazılsın; yan etki: bir itmede takip kesilir (F6 karar katmanı bunu yeniden kuracak).

> **Karar (2026-10-03, ADR-0021, proje sahibi):** konum sıçraması (`PositionJump`, > 1,0 m) rota ve takibi **sonlandırır**, yeniden planlama istenir (plandaki "açık soru" bu karara göre okunur).

## 1. Amaç

Bir botun navigasyon durumu (hedefe giden rota, takip/hız kestirimi, takılma-ilerleme değerlendiricisi ve kurtarma aşaması, bütçe kuyruğundaki istek, önbellek anahtarı referansı) şu olaylarda **hemen ve tamamen** sıfırlanır: **ölüm** (`isDead()`), **respawn** (`WIZ_REGENE` yanıtı), **despawn** (`BeginDespawn`/`ResetForRespawn`), **bölge değişimi / ışınlanma** (`WIZ_ZONE_CHANGE`, `WIZ_WARP`, bölge ≠ 71) ve açıklanamayan konum sıçraması. Böylece ölü, yeniden doğmuş veya başka yere taşınmış bir bot **eski yolla yürümeye devam etmez** (eski yolun ara noktalarına doğru yürümek yeni konumdan duvar kesen kirişler üretirdi; guard reddi 'son savunma'dır, bu plan nedenini ortadan kaldırır). `NAV=0` iken ve nav kullanılmıyorken davranış değişmez.

## 2. Bağlam (okunması zorunlu)

- `docs/12` §13.4: ölen bot doğuş noktasında yeniden doğar (arena dışı), arenaya yürüyerek döner (~52 / ~142 sn); dönüş yolu **yeni** planlanır. T-NAV-10: ölüm → respawn → arena dönüşü her ulusta ≥ 10 tekrar, takılma 0 (kanıt F5-66).
- **Bugünkü ölüm/despawn davranışı (F4/F2):** ölü botta `TickSessions` yalnızca `AbandonMove` çağırır (`m_moveActive = false`; log "move stopped (dead)") ve `TickMove` ölü bota **çağrılmaz** (`else` dalı). F5-62'den sonra `m_navDrive` ayrı durumdur; `AbandonMove`'daki tek satırlık `Reset()` ölümde yürüyüşü durdurur, ama (a) takip hedefi/izleyici, assessor ve kurtarma aşaması gibi yeni üyeler F5-63'te `Reset`'e girmezse bayat kalır, (b) respawn'dan sonra `Follow` kipi açık kalabilir, (c) bütçe kuyruğunda bekleyen istek ölü/despawn olmuş botu yürütebilir (`NavQueryScheduler::Cancel` yorumu: "A bot despawned/died while its query was pending"), (d) bölge değişimi/ışınlanma hiçbir yerde ele alınmıyor (`BotSession::OnPacket` `WIZ_WARP`'ı yalnızca `m_warpEcho`'ya yazar; `WIZ_ZONE_CHANGE` hiç işlenmez), (e) `ResetForRespawn` hareket üyelerini sıfırlar ama `m_navDrive`'ı sıfırlamaz.
- Thread: `BotSession::OnPacket` herhangi bir thread'den (DB/IOCP/zamanlayıcı) çalışabilir → **yalnızca atomik** yazar; temizlik IOCP thread'inde `TickSessions`'ta yapılır (`BotSession.h` başlık yorumu; `docs/13` §3).
- Önbellek: `NavPathCache` **tekil ve botlar arası paylaşımlıdır**; tek botun sıfırlanması paylaşılan önbelleği **silmez** (başka botlar kullanıyor, anahtar hücrelerden oluşur ve ızgara hiç değişmez). Bot başına sıfırlanan, `NavDrive` içindeki **anahtar/rota kopyası** referansıdır.
- Bütçe kuyruğu: `NavQueryScheduler::Cancel(botId)` (bekleyen isteği siler; bekleyen yoksa etkisiz); F5-64 `TickNav` zaten "artık gerekmiyorsa `Cancel`" der (**kemer + pantolon**: `ClearBotNav` doğrudan da çağırır).

### Tasarım kararları (Claude önerisi)

- **N1 tek giriş noktası:** `BotCore::NavDrive::Reset(NavResetReason)` (saf; durum + sayaç) ve sunucuda `BotManager::ClearBotNav(BotSession *, size_t index, NavResetReason, now)` = `Reset(reason)` + `NavService::Instance().Scheduler().Cancel(index)` + (yalnızca sürücü **aktifti**yse) tek `Bot_*.log` satırı `BotManager: bot <b> nav cleared (<reason>)`. Tüm olay kancaları bunu çağırır; `ActionExecutor`'daki F5-62 tek satırları `Reset(Stop/NewMove/...)` ile gerekçeli hale gelir, ama ayrı kancalar **önce** `ClearBotNav` çağırır (gerekçe sayacı doğru kalsın).
- **N2 `NavResetReason`:** `Stop, NewMove, Dead, Respawn, Despawn, ZoneChange, Warp, PositionJump` (sayaç `ResetCount(reason)`; `Reset()` argümansız = `Stop`).
- **N3 olay → kanca:**
  - **Ölüm:** `TickSessions` ölü dal, `!s->m_deadSeen` anında (ilk görüş) ve dal içinde `m_navDrive.Active()` iken (yedek): `ClearBotNav(Dead)`; `AbandonMove` (mevcut) ardından.
  - **Respawn:** `ActionExecutor::RequestRegene` `respawned` ⇒ `s->m_navDrive.Reset(Respawn)` (`BotManager` dışında: yalnız sürücü; kuyruk `TickNav` tarafından temizlenir); ayrıca `OnPacket` `WIZ_REGENE` yanıtı `m_navEventSeq`'i artırır (sunucu tarafından başlatılan olası respawn için).
  - **Despawn:** `BeginDespawn` başında `ClearBotNav(Despawn)`; `BotSession::ResetForRespawn` içinde `m_navDrive.Reset(Despawn)` ve atomik sayaçlar sıfırlanır.
  - **Bölge değişimi/ışınlanma:** `OnPacket` `WIZ_WARP` ve `WIZ_ZONE_CHANGE` atomik `m_navEventSeq`'i artırır ve `m_navEventKind`'e (`1` warp, `2` zone change, `3` regene) yazar; `TickSessions` her canlı bot için `m_navEventSeq != m_navSeenSeq` ise `m_navSeenSeq`'i günceller ve `ClearBotNav(ZoneChange|Warp)`; ayrıca `user->GetZoneID() != ZONE_RONARK_LAND` iken sürücü aktifse `ClearBotNav(ZoneChange)` (anket; `NavService` yalnızca zone 71'dir: F5-59).
  - **Konum sıçraması (N4 açık soruya bağlı):** `NavDrive::CheckDiscontinuity(botX, botZ)`: sürücü aktif ve **en az bir paket gönderilmiş** ise bot konumu ile son gönderilen paket konumu arası mesafe > `kPositionJumpM` (1,0 m `[A]`; paket konumu 0,1 m'ye nicemlenir, yuvarlama payı 0,05) → `ClearBotNav(PositionJump)`. Her tick, adım atmadan önce.
- **N5 durma paketi gönderilmez:** temizlik **paket üretmez** (ölü/despawn/ışınlanmış bot paket gönderemez veya göndermemeli; yürüyüş niyeti artık yoktur). Bot yerinde kalır; yeniden yürütme komuta/karar katmanına aittir.
- **N6 `Reset(reason)` kapsamı (tam liste; F5-63/F5-64 sonrası):** mod `Off`; rota/`Route()` boş; hedef (`GoalX/Z`), takip hedefi ve gözlemi; `NavFollower::Reset()` (izleyici ve plan: `Replans() == 0`, `NoTarget`); `NavProgressAssessor::Reset()`; `NavStuckMonitor::Reset()` (**tüm** hafıza dahil: tespit sayacı, merdiven aşaması, kurtarma hafızası); `NavStuckPenalties::Clear()`; bekleyen istek zamanı/`requestedAtMs`, plan zamanı, faz erteleme, önbellek anahtarı referansı, son paket konumu; kurtarma yürütme durumları. **Sayaçlar** (`ResetCount`) sıfırlanmaz (yalnızca `NavDrive` nesnesi başına tanılama).

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/NavDrive.h`: `NavResetReason`, `Reset(NavResetReason)`, `ResetCount`, `CheckDiscontinuity`, serbest yardımcı `NavClearBot(NavDrive &, NavQueryScheduler &, uint16_t id, NavResetReason)`.
2. `BotSession`: atomik `m_navEventSeq`, `m_navEventKind`; IOCP-only `m_navSeenSeq`; `OnPacket` kancası; `ResetForRespawn` sıfırlaması.
3. `BotManager::ClearBotNav` ve kancalar (ölü dal, `BeginDespawn`, olay anketi, bölge denetimi, sıçrama denetimi).
4. `ActionExecutor::RequestRegene` kancası.
5. Birim testleri (§5.6).
6. Çalışma zamanı doğrulaması (Claude; §6 K9-K13): ölüm ve despawn **koşulabilir**; bölge değişimi/ışınlanma için güvenli bir sunucu tetikleyicisi yoktur → birim + kod incelemesi (§8).

**Kapsam dışı (yapılmayacak)**

- Respawn sonrası **otomatik** arenaya dönüş/yeniden yürütme (karar katmanı F6; F5-66'da koşu betiği/komut yapar), ölüm bekleme kuralı (`CLI-14`, F4-07: değişmez).
- `NavPathCache`'in bot başına boşaltılması (paylaşımlıdır), `NavQueryScheduler` yeniden tasarımı (`Cancel` mevcut).
- Telemetri olayları (yeni `NAV_RESET` olayı **yok**; yalnızca `Bot_*.log`; olay eklemek `docs/16` kararıdır).
- `ScenarioRunner`/`ScriptRunner` durum temizliği (komut dosyası/senaryo bot ölünce kendi kuralını işletir; değişmez).
- `BotCore/NavStuck.h`, `NavTrack.h`, `NavBudget.h`, `Perception.h`, `NavGrid.h` **değişmez** (mevcut `Reset()`/`Clear()` API'leri kullanılır); `docs/` (Claude).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavDrive.h` | değiştir | `Reset(reason)`, sayaç, `CheckDiscontinuity`, `NavClearBot`; F5-62/63/64 testleri değişmeden geçer |
| `Tests/BotCoreTests/NavDriveTests.cpp` | değiştir | yalnızca sona yeni `NavDriveReset_*` vakaları |
| `GameServer/Bot/BotSession.h` | değiştir | yalnızca üç üye (`m_navEventSeq`, `m_navEventKind`, `m_navSeenSeq`) |
| `GameServer/Bot/BotSession.cpp` | değiştir | yapıcı başlatma, `OnPacket` (`WIZ_WARP`, `WIZ_ZONE_CHANGE`, `WIZ_REGENE`) atomik artırma, `ResetForRespawn` |
| `GameServer/Bot/BotManager.h` | değiştir | `ClearBotNav` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | `ClearBotNav`, ölü dal, `BeginDespawn`, olay anketi/bölge/sıçrama denetimi |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yalnızca `RequestRegene` `respawned` dalında bir satır (+ `AbandonMove`/`StopMove`/`BeginMove` tek satırlarına gerekçe argümanı) |

7 dosya, yeni dosya yok (`.vcxproj` değişmez). Bu listede olmayan bir dosyaya dokunmak gerekirse (ör. `ScenarioRunner.cpp` `BeginDespawn` çağrısı) **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-65 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucu `[UP]` ise `./tools/run-servers.sh stop`. F5-62 (ve varsa F5-63/F5-64) `NavDrive.h` ve `BotSession.h` üyelerini aç; **tüm üyelerin listesini** raporda çıkar ve her birinin `Reset`'te nasıl sıfırlandığını eşle (N6). Sapma varsa **dur**.
2. **`NavDrive.h`:**

   ```cpp
   enum class NavResetReason { Stop, NewMove, Dead, Respawn, Despawn, ZoneChange, Warp, PositionJump };
   constexpr float kNavPositionJumpM = 1.0f;   // [A] unexplained jump between the last sent packet and the bot position
   // NavDrive additions:
   void Reset(NavResetReason reason);          // N6: everything; ResetCount(reason) += 1; Reset() == Reset(Stop)
   int  ResetCount(NavResetReason reason) const;
   // True when the drive is active, at least one packet was sent and the bot is farther than kNavPositionJumpM
   // from the last sent packet position (the server moved it: knock-back, warp, respawn).
   bool CheckDiscontinuity(float botX, float botZ) const;
   // Free helper: Reset + remove the bot's pending path query (pure, unit-testable).
   inline void NavClearBot(NavDrive & drive, NavQueryScheduler & scheduler, uint16_t id, NavResetReason reason);
   ```

3. **`BotSession`:** `std::atomic<uint32> m_navEventSeq;` (OnPacket, herhangi bir thread), `std::atomic<uint8> m_navEventKind;`, `uint32 m_navSeenSeq;` (IOCP-only); `OnPacket`: `WIZ_WARP` bloğunun (`:410-415`) yanına, `WIZ_REGENE` (`:154`/`:218`) ve `WIZ_ZONE_CHANGE` için `m_navEventKind` yazıp `m_navEventSeq.fetch_add(1)`; yapıcı başlatma listesine eklenir; `ResetForRespawn`: `m_navDrive.Reset(NavResetReason::Despawn); m_navEventSeq = 0; m_navSeenSeq = 0; m_navEventKind = 0;`. Mevcut `m_warpEcho`/`m_regeneEcho` davranışı **değişmez** (`TickSpeedCheck`/`RequestRegene` onlara bakar).
4. **`BotManager`:** `ClearBotNav` (N1); `TickSessions`: (a) ölü dal: `!s->m_deadSeen` anında veya `Active()` iken `ClearBotNav(Dead)`; (b) canlı dal, adım atmadan **önce**: olay anketi (`m_navEventSeq`), bölge denetimi (`GetZoneID() != ZONE_RONARK_LAND` iken aktifse), `CheckDiscontinuity`; `BeginDespawn` başında `ClearBotNav(Despawn)`. Tüm `ClearBotNav` log satırı yalnızca sürücü aktifken. **`index`** = `m_sessions` indeksi (F5-64 `botId`).
5. **`ActionExecutor`:** `RequestRegene` `respawned` dalında (`:1930-1933` civarı) `s->m_navDrive.Reset(BotCore::NavResetReason::Respawn);`; F5-62'nin üç tek satırı gerekçe argümanı alır (`StopMove` → `Stop`, `BeginMove` → `NewMove`, `AbandonMove` → `Stop`; ölü/despawn kancaları `AbandonMove`'dan **önce** `ClearBotNav` çağırdığı için doğru gerekçe sayılır).
6. **Testler** (`NavDriveTests.cpp`; sentetik ızgara; adlar sabit):
   - `NavDriveReset_AllMembers`: sürücüyü tam kurul (Follow: rota, takip planı, hedef izleyici örnekleri, assessor niyeti + paketler, monitör tespit sayacı ≥ 1 ve `Stage() ≥ 1`, penalties ≥ 1, `requestedAtMs`, plan zamanı, önbellek anahtarı, son paket konumu); `Reset(Dead)` sonrası: `Active() == false`, `Mode() == Off`, `Route()` boş, `follower.Replans() == 0` ve plan `NoTarget`, izleyici boş, `assess.Assess(...) == Idle`, `monitor.Stage() == 0`, `monitor.Episodes() == 0`, `penalties.Count(now) == 0`, bekleyen istek yok (`requestedAtMs` geçersiz), önbellek anahtarı geçersiz, `NextStep == None`, `TickFollow` paket üretmez, `CheckDiscontinuity == false`. **Bu testin kontrol listesi `NavDrive`'ın her üyesini ismen sayar** (yeni üye eklenince test kırılacak şekilde: üye sayısı sabitlenir).
   - `NavDriveReset_NoStalePath_AfterRespawn`: A hedefine `goto`, ortada `Reset(Dead)`, bot doğuş noktasına (başka konum) taşınır; `NextStep == None`; B hedefine yeni `BeginGoto`: yeni `Route()[0]` doğuş konumu ve **hiçbir ara nokta A rotasından değildir**; ilk adım B'ye doğrudur; tüm adım kirişleri `CheckMoveChord == Ok`.
   - `NavDriveReset_PerReason`: sekiz gerekçenin her biri etkin sürücüyü `Off` yapar, `ResetCount(reason) == 1`; ikinci kez çağrı idempotent (durum aynı, sayaç 2).
   - `NavDriveReset_Follow_NoResume`: takip kipi, hedef gözlemleri, kurtarma aşaması 2'de iken `Reset(Respawn)`: hedef gözlense bile yeniden **plan yapılmaz** ve paket üretilmez (`BeginFollow` çağrılana dek).
   - `NavDriveReset_SchedulerCancel`: sanal 3 bot kuyrukta; ortadaki `NavClearBot(...)`: `Pending()` 2; sonraki `NextBatch` onu seçmez; yeniden istek yeni zaman damgası alır.
   - `NavDriveReset_PositionJump`: normal adım (bot = paket konumu ± 0,05) → `false`; 5 m sıçrama → `true`; hiç paket yokken → `false`; pasif sürücü → `false`.
7. Derle/test; `check-perception-contract.py` rc=0. Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; değişen dosyalar `touch` edilince yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; altı yeni `NavDriveReset_*` adı `[ OK ]`; F5-62/63/64 testleri değişmeden geçer
- [ ] K3: `NavDriveReset_AllMembers`: raporda `NavDrive`'ın **tüm** üyelerinin listesi ve her birinin test edildiği satır eşlemesi; üye sayısı sabitleme kontrolü mevcut
- [ ] K4: `git diff --stat` yalnızca §4 (7 dosya) + plan; `BotCore/NavStuck.h|NavTrack.h|NavBudget.h|Perception.h|NavGrid.h`, `docs/`, `tools/`, `.vcxproj` farkı **0**; `git diff --check` boş
- [ ] K5: `NavDrive.h`'de `grep -n -E "windows.h|stdafx|GameServer|shared/|static |new |malloc"` boş; `python3 tools/check-perception-contract.py` rc=0
- [ ] K6: `OnPacket` yeni kodu **yalnızca atomik** yazar (`dosya:satır`); `m_warpEcho`/`m_regeneEcho` davranışı değişmedi (`git diff` o satırlarda yok); temizlik **yalnızca** IOCP kodunda (`TickSessions`, `BeginDespawn`, `RequestRegene`)
- [ ] K7: her kanca (ölü dal, `RequestRegene`, `BeginDespawn`, `ResetForRespawn`, olay anketi, bölge denetimi, sıçrama denetimi) kodda **bulunur** ve her biri `dosya:satır` ile raporlanır; temizlik **paket üretmez** (kod incelemesi)
- [ ] K8: `NAV=0`/`ENABLED=0` davranışı değişmez: sürücü hiç aktif olmadığından yeni kodun **hiçbir** log satırı/paketi üretilmez (`ClearBotNav` log yalnızca `Active()` iken); `PERF_SAMPLE` ve komut çıktıları bugünküyle aynı
- [ ] K9 (Claude, çalışma zamanı): **ölüm:** `ENABLED=1, NAV=1, TELEMETRY=decisions`; Karus botu `/bot goto` ile ≥ 60 m uzaktaki noktaya yürürken karşı ulus bot onu öldürür (`/bot attack`; F4-07 koşu kalıbı); `Bot_*.log`'da `bot <b> nav cleared (dead)`; ölümden sonra o bota ait `ACTION_SUBMIT` `Move` paketi **yok** (telemetri sorgusu); bot ölü beklerken konumu değişmez
- [ ] K10 (Claude): **respawn:** `/bot regene <bot>` (≥ 3 sn sonra, CLI-14): bot doğuş noktasında yeniden doğar ve **hiçbir komut verilmeden** ≥ 20 sn hareket etmez (`Move` paketi yok, `/bot list` konumu sabit); ardından yeni `/bot goto <arena>`: ilk `NAV_PATH`/`cmd goto` rotası doğuş konumundan başlar ve eski hedefe giden ara nokta içermez; yeni hedefe varır
- [ ] K11 (Claude): **despawn:** `goto` ortasında `/bot despawn <bot>` sonra `/bot spawn <bot>`: yeniden girişten sonra bot **yürümez** (komut yok); `nav cleared (despawn)` satırı; bütçe kuyruğunda kalıntı yok (`PERF_SAMPLE.nav_*` bota ait sızıntı belirtisi göstermez; F5-64 yoksa kod incelemesi)
- [ ] K12 (Claude): her iki ulus için K9-K10 birer kez (Karus ve El Morad botu; ulus bazlı satır rapora); ölüm → respawn → arena zinciri **koşu betiği olmadan** (komutlarla) çalışır: F5-66'da ≥ 10 tekrar
- [ ] K13 (Claude): **bölge değişimi/ışınlanma:** güvenli bir sunucu tetikleyicisi yoktur (`/bot` warp komutu yok; hız-geri-çekme zorlanamaz): **çalışma zamanı kanıtı yok**; raporda "birim testi (`NavDriveReset_PerReason`, `_PositionJump`) + kod incelemesi (`OnPacket`, `TickSessions`)" olarak etiketlenir ve `docs/reports/degerlendirme-takip.md` satırı `KOŞULLU/BEKLİYOR` kalır; sunucu `stop` ile kapanır

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "NavDriveReset_|NavDrive|tests,"
./tools/run-tests.sh Debug
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-65
git diff --check gece/2026-10-02...bot/F5-65
git grep -n -E "ClearBotNav|NavResetReason" bot/F5-65 -- GameServer BotCore   # kanca konumları
# Claude (çalışma zamanı): ./tools/run-servers.sh start ; komutlar BotCommands.txt ; grep -a "nav cleared" /mnt/c/dev/fdp/server/Logs/Bot_*.log ; ./tools/run-servers.sh stop
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; konsol spam'i yok. Thread kuralı (`AGENTS.md` §2.6, `docs/13` §3): `OnPacket` thread'inden `m_navDrive`'a **dokunma**; yalnızca atomik bayrak.
- **Bot avantajı yasağı:** bu plan botu **durdurur**; yeni yetenek yok. Temizlik paket üretmez; ölü botun "yürümeye devam etmesi" (ve guard'a takılması) zaten adalet ihlali değil ama gereksiz reddi/log gürültüsüdür ve AC-NAV-03 denetimini kirletirdi.
- **Dürüstlük:** birim testleri `NavDrive` durumunun sıfırlandığını ve bayat yolun kullanılmadığını sınar; **sunucu olaylarının** (`WIZ_DEAD`/`WIZ_REGENE`/despawn) gerçekten bu kancaları tetiklediği yalnızca K9-K11 ile kanıtlanır; bölge değişimi/ışınlanma kancası için çalışma zamanı kanıtı **yoktur** (K13) ve öyle yazılır. Konum sıçraması eşiği (1,0 m) `[A]`'dır; paket konumu 0,1 m'ye nicemlenir.
- Çakışma uyarısı: `BotManager.cpp` `TickSessions`'a F4-55 (el sıkışma kapısı), F6-* (karar katmanı) ve F4-60 (`BotSession.cpp` `ResetForRespawn`, `WIZ_DEAD` bloğu) dokunur; ardışık uygulanmalı, bu plan HAZIR yapılmadan önce o dilimlerin son hali okunmalıdır.
- Beklenmedik durumda (F5-62 sözleşmesi farklı, `OnPacket` yeniden yapılandırılmış, ek dosya gerekiyor) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-65` — `<kısa-sha> [F5-65] …`
- Değişen dosyalar ve neden:
  - `…`
- `NavDrive` üye listesi ve `Reset` eşlemesi (K3): …
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ … (K9-K13 Claude'un çalışma zamanı kriterleri)
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F5-65` @ `<sha>`
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
