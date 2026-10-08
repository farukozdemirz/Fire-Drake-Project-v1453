# UA-04: Bot katmanını AlphaGame tabanına bağlama (kancalar, proje dosyası, paket/hasar izleyicileri)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | UA — Sürüm yükseltme tabanı AlphaGame 1534 (ADR-0069 madde 1) |
| Branch | `bot/UA-04` (taban: `yukseltme/alpha`, UA-01 birleşmiş) |
| Bağımlı olduğu planlar | UA-01 (DOĞRULANDI). UA-02 ve UA-03 paralel; birleştirmeyi Claude yapar |
| İlgili gereksinim / kabul | ADR-0069 madde 1; T-UPG-04; `docs/reports/u0-1534/A-kaynak-kod-karsilastirmasi.md` §5 |
| Tahmini büyüklük | M |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

Bot hattının üst kaynak dosyalarında yaptığı değişiklikleri (kancalar, komutlar, soket olayı, izleyiciler) AlphaGame dosyalarına taşımak ve `GameServer/Bot/*` ile `BotCore`'u AlphaGame GameServer'ına bağlayarak derlemek.

## 2. Bağlam (okunması zorunlu)

- Kanca kaynağı: `git diff 0f52027 gece/2026-10-08-kalabalik -- GameServer AIServer LogInServer shared N3BASE scripting ':!GameServer/Bot'` — 20 dosya, +801/−9 (bot hattı; U1 profil değişikliklerini içermez). Dosyalar: `AttackHandler.cpp`, `CharacterSelectionHandler.cpp`, `ChatHandler.cpp`, `DamageTrace.cpp/.h` (yeni), `DatabaseThread.cpp`, `GameServerDlg.cpp/.h`, `MagicInstance.cpp`, `PacketTrace.cpp/.h` (yeni), `User.cpp/.h`, `proj-GameServer.vcxproj(.filters)`, `shared/KOSocketMgr.h`, `shared/SMDFile.h`, `shared/SocketDefines.h`, `shared/SocketMgr.cpp/.h`.
- A §5: 42 hunk'tan 34'ü doğrudan uygulanıyor; 10 çakışma bölgesi 5 dosyada, hepsi "iki taraf da satır ekledi" türü (ChatHandler komut tabloları, `GameServerDlg::Startup` sırasının `StartUserSocketSystem()`'a taşınması, `CUser` kurucusu gönderme tamponu, `User.h` komut bildirimi, vcxproj).
- Yöntem önerisi: her dosya için `git merge-file <alpha> <0f52027 sürümü> <kalabalik sürümü>` (3 yollu).
- `GameServer/Bot/*` `yukseltme/1534`'ten geliyor ve 1534 düzenlerini `ProtocolProfile::ClientVersion(__VERSION)` ile seçiyor; AlphaGame'de `__VERSION 1534` ve ini'de `[PROTOCOL]` yok, sonuç 1534 `[D]` (`shared/ProtocolProfile.h`, `GameServer/Bot/BotSession.cpp:70-74`). Bu korunmalı.
- ADR-0069 Ek 1: altı dizinde dosyalar bayt bayt saklanır; düzenlenen dosyanın satır sonu ve kodlaması korunur; düzenlenen her dosya `tools/ua-import-alpha.py` `MANUAL_CHANGES` listesine gerekçeyle eklenir, `--check` `PASS` kalır.
- Derleme seçenekleri: `tools/build.sh --packet-trace` / `--damage-trace` (`FdpTraceDefs`) bot hattındaki vcxproj'da tanımlı; AlphaGame vcxproj'una taşınmalı.

## 3. Kapsam

**Var:**
1. 20 dosyanın değişikliklerinin AlphaGame dosyalarına 3 yollu taşınması; her çakışma bölgesinin çözümü raporda.
2. `proj-GameServer.vcxproj(.filters)`: `GameServer/Bot/*.cpp`, `PacketTrace`, `DamageTrace`, `BotCore` bağımlılığı (bot hattındaki gibi), `FdpTraceDefs`.
3. Bot katmanının kullandığı ve AlphaGame'de adı/imzası farklı olan API'ler (A §5 "API yüzeyi": 241'den 240'ı var) için en küçük uyarlama; tercihen `GameServer/Bot/` içinde.
4. `--packet-trace` ve `--damage-trace` derlemeleri çalışır.

**Yok:** bot davranışı/ayarı değişikliği (UA-07), güvenlik (UA-02), paket düzeni (UA-03), DB, sunucu başlatma.

## 4. Dokunulabilecek dosyalar

§2'deki 20 dosya; `GameServer/Bot/**` (yalnız API uyarlaması); `tools/ua-import-alpha.py` (`MANUAL_CHANGES` listesi); `Tests/**` yalnız derleme için zorunluysa (raporda gerekçe).

## 5. Uygulama adımları

1. 3 yollu birleştirme; çakışma tablosu.
2. Proje dosyası; derleme (Release, Debug, Release `--packet-trace --damage-trace`).
3. Testler.

## 6. Kabul kriterleri

- [ ] K1: Release, Debug ve `--packet-trace --damage-trace` Release derlemeleri 0 hata; yeni uyarı yok (UA-01 tabanı 56).
- [ ] K2: `./tools/run-tests.sh Release` 3484 test (ya da gerekçeli yeni sayı), 0 başarısız.
- [ ] K3: 20 dosyanın her hunk'ı için durum (uygulandı / çakışma çözüldü + nasıl).
- [ ] K4: `GameServer.exe` bot sembollerini içerir (ör. derleme çıktısında `BotManager.obj`); `[BOT] ENABLED=0` iken bot kodu çalışmaz (kod alıntısı).
- [ ] K5: `tools/ua-import-alpha.py --check` `PASS`.
- [ ] K6: `git diff --stat` yalnız §4; kodlama korunur; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug && ./tools/build.sh Release --packet-trace --damage-trace
./tools/run-tests.sh Release
python3 -I tools/ua-import-alpha.py --check "/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/1-Game Source" .
git diff --stat yukseltme/alpha...bot/UA-04
git status --short
```

## 8. Kısıtlar ve uyarılar

- Sunucu başlatılmaz; DB erişimi yok.
- UA-02 (`User.cpp` paket dağıtımı ve `GetItem`, `GameServerDlg.cpp` kimlik bilgisi varsayılanları, `LoginHandler.cpp`, `SealHandler.cpp`, `UpgradeHandler.cpp`, `ItemHandler.cpp` `RunSelectExchange`, `CharacterMovementHandler.cpp`, `MerchantHandler.cpp`) ve UA-03 (envanter sabitleri, MyInfo, ağırlıklar, `DatabaseThread.cpp`, `QuestHandler.cpp`, klan dosyaları) paralel çalışıyor: yalnız kanca satırlarını ekle, o fonksiyonların gövdelerini değiştirme.
- Git: `AGENTS.md` §2.8; commit `[UA-04] ...`; push yok.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
