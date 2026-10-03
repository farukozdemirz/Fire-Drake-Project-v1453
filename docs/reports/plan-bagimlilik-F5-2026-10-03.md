# F5-59 .. F5-67: bağımlılık, dosya çakışması ve doğrulama özeti (taslak, 2026-10-03)

Kaynak: `drafts/nav-a/F5-59`, `F5-60`, `F5-67` ve `drafts/nav-b/F5-61 .. F5-66` taslakları; taban `gece/2026-10-02` @ `9fc2dfe`. Hepsi `[BOT] ENABLED=0` / `[BOT] NAV=0` varsayılanında davranışı **değiştirmez**. Şemsiye: F5-55 (`KAPANDI` olması için F5-59 .. F5-66; F5-67 yalnızca F5-60 `GEREKLİ` derse).

## 1. Tek tablo

| Sıra | Plan | Durum | Bağımlı olduğu | Dokunduğu dosyalar (K = yeni) | Sunucu çalışma zamanı doğrulaması (Claude) | İnsan gerektirir |
|---|---|---|---|---|---|---|
| 1a | **F5-59** `NavService` yaşam döngüsü, SMD'den ızgara, CRC32 | HAZIR | F5-01 (`KAPANDI`) | `BotCore/NavFingerprint.h` (K), `BotCore.vcxproj`, `Tests/.../NavFingerprintTests.cpp` (K), `BotCoreTests.vcxproj`, `GameServer/Bot/NavService.{h,cpp}` (K), `proj-GameServer.vcxproj` + `.filters`, `GameServerDlg.cpp`, `shared/SMDFile.h` | **Evet** (K9-K12: `nav ready` satırı, `crc32` = `nav-export.py`, `NAV=0` davranışsız) | Hayır |
| 1b | **F5-60** su/engel verisi denetimi (çevrimdışı) | HAZIR | F5-01..03, F5-11, F5-50, F5-58 (`KAPANDI`); F5-59'dan bağımsız | `tools/nav-measure/nav_measure.cpp`, `tools/nav-water-audit.py` (K) | Hayır (sunucusuz) | **Evet (dolaylı):** T-NAV-09 insan istemcisi testi; zemin gerçeği yoksa karar `BELİRSİZ` |
| 2 | **F5-61** kiriş guard'ı (CLI-08 `blocked_chord`) | TASLAK | **F5-59** | `BotCore/NavChordGuard.h` (K), `BotCore.vcxproj`, `Tests/.../NavChordGuardTests.cpp` (K), `BotCoreTests.vcxproj`, `ActionExecutor.{h,cpp}` | **Evet** (K9-K12: engel kesen düz `/bot move` reddi, temiz adım, durma, `NAV=0` karşılaştırması) | Hayır |
| 3 | **F5-62** `/bot goto` yol izleme, paylaşılan `NavPathfinder` | TASLAK | **F5-59, F5-61** | `BotCore/NavDrive.h` (K), `BotCore.vcxproj`, `Tests/.../NavDriveTests.cpp` (K), `BotCoreTests.vcxproj`, `GameServer/Bot/NavService.h`, `ActionExecutor.{h,cpp}`, `BotSession.h`, `BotManager.{h,cpp}`, `BotCore/ScriptPlan.h`, `Tests/.../ScriptTests.cpp` (12 dosya) | **Evet** (K10-K15: iki ulus doğuş → arena `goto`, `/bot move` korunur, `NAV=0`, geçersiz hedef) | Hayır |
| 4 | **F5-63** `/bot follow`, hız kestirimi, yeniden plan, takılma/kurtarma | TASLAK | **F5-62** (dolaylı F5-59, F5-61) | `BotCore/NavDrive.h`, `BotCore/NavTrack.h`, `Tests/.../NavDriveTests.cpp`, `Tests/.../NavTrackTests.cpp`, `ActionExecutor.{h,cpp}`, `BotManager.{h,cpp}`, `ScriptPlan.h`, `ScriptTests.cpp` (10 dosya) | **Evet** (K11-K14: 10 dk takip, hedef kaybı, bir takılma vakası; **kabul değil ölçüm**) | Hayır |
| 5 | **F5-64** bütçe zamanlayıcı + telemetri + `PERF_SAMPLE` nav payı | TASLAK | **F5-62, F5-63** | `BotCore/NavDrive.h`, `Tests/.../NavDriveTests.cpp`, `NavService.h`, `ActionExecutor.{h,cpp}`, `BotManager.{h,cpp}` (7 dosya) | **Evet** (K9-K13: 12 botla nav alanları, `NAV_*` olayları, `nav_stale_steps = 0`; **AC-NAV-07 kapanmaz**) | Hayır |
| 6 | **F5-65** nav durumu temizliği (ölüm/respawn/despawn/bölge) | TASLAK | **F5-62** (+ F5-63/F5-64 ile birlikte; sıra §2) | `BotCore/NavDrive.h`, `Tests/.../NavDriveTests.cpp`, `BotSession.{h,cpp}`, `BotManager.{h,cpp}`, `ActionExecutor.cpp` (7 dosya) | **Evet** (K9-K12: ölüm, respawn, despawn; **bölge değişimi/ışınlanma için çalışma zamanı kanıtı yok**) | Hayır |
| 7 | **F5-66** çalışma zamanı doğrulama koşusu (araçlar + `ENV`) | TASLAK | **F5-59 .. F5-65**; 16 bot için **F8-03** | `tools/nav-run.py` (K), `tools/nav-run-report.py` (K), `tools/nav-move-audit.py` (K), `tools/nav-measure/nav_measure.cpp`, `Tests/BotCoreTests/main.cpp`, `tools/nav-regress.py`, `tools/bot-telemetry-report.py` (7 dosya) | **Evet, asıl koşu** (K9-K19: S1-S6; DeepSeek sunucu çalıştırmaz) | Hayır (T-NAV-09 hariç, ayrı) |
| 8 (koşullu) | **F5-67** su katmanı düzeltmesi | TASLAK (koşullu) | F5-60 `KAPANDI` **ve** sonuç `GEREKLİ`; F5-59; F5-61 | `BotCore/NavGrid.h`, `Tests/.../NavGridTests.cpp` (veya `NavWaterTests.cpp` K), `NavSegmentTests.cpp`, `GameServer/Bot/NavService.{h,cpp}`, `BotCore/NavFingerprint.h`, `tools/nav-regress/good.txt` | **Evet** (F5-60 sonucuna göre) | **Evet:** T-NAV-09 sonucu ve maske kaynağı kararı (proje sahibi) |

## 2. Önerilen sıra ve paralellik

1. **Hemen başlayabilir, paralel:** F5-59 ∥ F5-60 (ikisi `HAZIR`, birbirinden bağımsız).
2. **Ardışık (aynı dosyalar, birbirine bağlı):** F5-59 → F5-61 → F5-62 → F5-63 → F5-64 → F5-65 → F5-66. Önerilen sıra `F5-65`'i `F5-64`'ten **sonraya** koyar: `Reset(reason)` ve `NavDriveReset_AllMembers` testi F5-63/F5-64'ün eklediği üyeleri (takip, değerlendirici, monitör, ceza, bütçe isteği, önbellek anahtarı) kapsar. F5-65 yalnızca F5-62'ye bağlıdır (kullanıcı kapsamı); daha erken uygulanırsa F5-63/F5-64 uygulayıcıları kendi üyelerini `Reset`'e ekler (iki planın §3'ünde yazılı).
3. F5-60 sonucu `GEREKLİ` ise F5-67; sıra: F5-61'den önce **veya** birlikte (su kirişi vakası F5-61 testlerine girer); F5-66'dan önce biterse su vakası koşuya eklenir. `GEREKMEZ` ise F5-67 `İPTAL`.
4. F5-66 **en son**. Koşu etkileşimli Claude doğrulamasındadır: `tools/auto-loop.sh` her adımda açık sunucuyu kapatır (`tools/auto-loop.sh:160-167`) ve ölçüm gürültüsünü artırır; koşu sırasında döngü durdurulur (`plans/.auto-loop-stop`). `nav-run.py` döngü çalışıyorsa başlamayı reddeder.

## 3. Dosya çakışma matrisi (kim hangi dosyaya dokunur)

| Dosya | Planlar | Risk |
|---|---|---|
| `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` | F5-59, F5-61, F5-62 (+ F6-01..05, F8-05..07, F4-55, F5-67) | **Düşük ama kesin:** hepsi tek satır ekler; bitişik satır birleştirme çatışması çıkar (elle çözülür). Sıralı birleştirme |
| `GameServer/Bot/NavService.h` | F5-59 (K), F5-62, F5-64, F5-67 | Orta: F5-62 `SharedPathfinder()`, F5-64 zamanlayıcı/önbellek/istatistik, F5-67 maske; ardışık |
| `BotCore/NavDrive.h` | F5-62 (K), F5-63, F5-64, F5-65 | **Yüksek:** dört plan aynı başlığı büyütür; ardışık zorunlu, paralel yazılamaz |
| `Tests/BotCoreTests/NavDriveTests.cpp` | F5-62 (K), F5-63, F5-64, F5-65 | **Yüksek:** aynı (yalnızca sona ekleme önerilir) |
| `GameServer/Bot/ActionExecutor.cpp` / `.h` | F5-61, F5-62, F5-63, F5-64, F5-65 | **Yüksek:** `SubmitMove`, `BeginMove/StopMove/AbandonMove`, `RequestRegene`; F4-60/F4-55/F6-* de dokunabilir |
| `GameServer/Bot/BotManager.cpp` / `.h` | F5-62, F5-63, F5-64, F5-65 | **Yüksek:** `TickSessions`, `Tick()`, `EmitPerfSample`, `ExecuteCommand`; F4-55 (`TickSessions` kapısı), F4-60 (`CommandSnap`), F6-*, F8-03 (`BOT_TABLE`), F8-05/F8-06 (`ScenarioRunner`, `BeginScenarioMatch`) |
| `GameServer/Bot/BotSession.h` / `.cpp` | F5-62 (h), F5-65 | **Yüksek:** F4-60 (`ResetForRespawn`, `WIZ_DEAD` bloğu, yeni üyeler) ile aynı fonksiyonlar |
| `BotCore/ScriptPlan.h`, `Tests/.../ScriptTests.cpp` | F5-62 (`goto`), F5-63 (`follow`) | Orta: aynı dizi/döngü sınırı; ardışık |
| `BotCore/NavTrack.h`, `Tests/.../NavTrackTests.cpp` | F5-63 | Düşük (yalnızca ekleme; F5-64 `ReplanDue`'a bağlı) |
| `tools/nav-measure/nav_measure.cpp` | F5-60 (`water` bölümü), F5-66 (`ENV`) | Orta: aynı `main`/kullanım metni; ardışık |
| `tools/nav-regress/good.txt`, `tools/nav-regress.py` | F5-66 (`.py`: `ENV` anahtarı), F5-67 (`good.txt`) | Düşük |
| `BotCore/NavGrid.h` | F5-67 | F5-01'in dosyası; yalnızca koşullu plan |
| `GameServer/GameServerDlg.cpp`, `shared/SMDFile.h`, `proj-GameServer.vcxproj(.filters)` | F5-59 | `GameServerDlg.cpp` UTF-8 BOM'lu; başka plan yok (F8-05/F8-06 `proj-GameServer.vcxproj`'a dosya ekler: bitişik satır çatışması) |
| `tools/bot-telemetry-report.py` | F5-66 | Düşük (F8-* araçları ayrı dosyalar) |

## 4. Sunucu çalışma zamanı doğrulaması: kim, ne zaman

- **Claude (etkileşimli `/plan-dogrula`, sunucu açık):** F5-59 K9-K12, F5-61 K9-K12, F5-62 K10-K15, F5-63 K11-K15, F5-64 K9-K14, F5-65 K9-K13, F5-66 K9-K19. DeepSeek bu planlarda sunucu **çalıştırmaz**; derleme ve birim testleri DeepSeek'tedir.
- **Otonom döngüde yapılamaz:** döngü sunucuyu kapatır; çalışma zamanı kriterleri döngüde `BEKLİYOR` kalır, etkileşimli oturumda koşulur (planlardaki "Claude" etiketli K'ler).
- **İnsan gerektirenler:** T-NAV-09 (su, F5-60 + proje sahibi testi; sonuç F5-67'yi tetikler/iptal eder), maske kaynağı ve sert-engel/maliyet kararı (F5-67), F5-61 D5 (başlangıç hücresi `Walk` değilse kural), F5-62 hedef `Walk` değilse reddet/snap kararı, F5-65 `PositionJump` sonlandırma kararı, T-NAV-06 kabul eşiği, 16 bot için F8-03 uygulaması (`db/005`), faz kabulü G5.
- **Kapanışta yazılmayacaklar:** AC-NAV-07 ve MET-PERF-02 "kapandı" (16 bot olmadan), T-NAV-11, "suya takılmıyor", bölge değişimi/ışınlanma temizliğinin çalışma zamanı doğrulaması, T-NAV-06 hükmü (eşik yok).

## 5. F5-59 sözleşmesinden sapma / eksik bulgular

1. **`NavPathfinder` yok:** F5-59 yalnızca `const NavGrid` tutar; `NavPathfinder` (4,02 MiB, iş parçacığı güvenli değil) F5-62'de `NavService.h`'ye `SharedPathfinder()` olarak eklenir. F5-59'un "durum tutar, planlamaz" ilkesine uyar ama **F5-59 dosyasının değişmesini** gerektirir (F5-62, F5-64, F5-67 aynı başlığa dokunur).
2. **Ad tutarsızlığı F5-59 içinde:** §3 madde 3 `Info()` der, §5 iskeleti `GetInfo()` tanımlar; planlar `GetInfo()` varsayar (kullanıcı kapsamı da öyle).
3. **Zone sınırı örtük:** `Grid()` yalnızca zone 71 ızgarasıdır, bot bölgesini denetlemez; F5-61/F5-62/F5-65 `GetZoneID() == ZONE_RONARK_LAND` denetimini kendileri yapar (R2 kısıtlı sembol listesinde `GetZoneID` yok).
4. **Zamanlayıcı/önbellek/istatistik yok:** `NavQueryScheduler`, `NavPathCache`, nav istatistikleri F5-64'te `NavService.h`'ye girer.
5. **Başlatma sırası:** `NavService::Startup()` `BotManager::Startup()`'tan sonra (`MapFileLoad()` sonrası) çalışır; botlar `StartTicking()`'ten sonra tick'lendiği için `Ready()` o zaman doğrudur. Bu sıra F5-59'da doğru; planlar `Ready()`'yi her çağrıda kontrol eder.
6. **`Shutdown()` yalnızca bayrağı düşürür:** F5-64'ün tekil zamanlayıcı/önbelleği süreç çıkışında yıkılır; bu planlarda sorun yok, ama `Shutdown()` sonrası tick çalışmamalıdır (F5-59'da `BotManager::Shutdown()` önce çağrılır: doğru).

## 6. Kapsam boşlukları (hiçbir F5-59..F5-66 diliminde yok; ayrıca ele alınmalı)

- **Arena sınırının sunucu bağlaması** (`NavCostLayer::AddForbidOutsideDisc`, F5-51 yalnızca arena modunda): `goto`/`follow` düz mesafe planlar (`field = nullptr`); "arenada kal" kuralı sağlanmaz (F5-62 §8). F5-66 yalnızca **varışta** arena içinde olmayı ölçer.
- **Tehlike/kule katmanı, `NavReach` (ulaşılamaz hedef bırakma), `NavFormation`, `NavRetreat`, `NavLos`:** sunucuya bağlı değil (`wiring` tablosu `BAGLI DEGIL` basacak); F6 karar katmanı gerektirir.
- **`WIZ_ZONE_CHANGE`/`WIZ_WARP` için güvenli çalışma zamanı tetikleyicisi yok:** F5-65 bu kancayı yalnızca birim + kod incelemesiyle kanıtlar.
- **16 karakter:** `db/005` + `BOT_TABLE` (F8-03, `TASLAK`) olmadan AC-NAV-07/T-NAV-11 16 botlu kanıtı yok.
- **Takip için `UnitView` anlık görüntü maliyeti** (`ObsTable` kopyası, 250 ms'de bir) nav payına girmez; toplam `Tick()` içinde F5-66'da görünür.

## 7. Dürüstlük notları (yazım turunda yeniden doğrulanacaklar)

Bu özette ve taslaklarda **doğrulanamayan** noktalar: (1) F5-57 `NavProgressAssessor::NotifyReplan` gövdesinin son hali (`gece/2026-10-02` @ `9fc2dfe`'de Tur 2 `DOĞRULANDI`; yine de yeniden okunacak); (2) `moving` kuralı gerilimi (F5-63 G1) ve `nav_measure progress` "assessor + monitor" satırının `NavStuckParams`'ı; (3) `GameServer.ini` ve `BotCommands.txt` gerçek yolları, `PX/PZ` ölçeği (bot başlangıcı arena A mı); (4) doğuş noktasına sunucu rastgele sapma uyguluyor mu; (5) `WIZ_ZONE_CHANGE`/`WIZ_WARP` paketlerinin bota `OnPacket` ile ulaştığı; (6) Win32 2 GB adres alanı gerekçesinin gerçek bellek kullanımı; (7) `docs/16:58-59, 71` ve `docs/15:65-66` satır numaraları `git grep` ile doğrulandı; (8) Karus/El Morad doğuş hücrelerinin `Walk` olduğu **Python ön ölçümüdür** (olay + ana bileşen; `clearance` bakılmadı).
