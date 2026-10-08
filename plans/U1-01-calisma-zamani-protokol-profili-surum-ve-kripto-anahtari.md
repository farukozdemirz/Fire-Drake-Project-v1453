# U1-01: Çalışma zamanı protokol profili — istemci sürümü ve kripto anahtarı ini'den (`[PROTOCOL] CLIENT_VERSION`)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | U — Sürüm yükseltme 1534 (`docs/17` §2 U, ADR-0068) |
| Branch | `bot/U1-01` (taban: `main`) |
| Bağımlı olduğu planlar | — |
| İlgili gereksinim / kabul | T-UPG-01, T-UPG-05 (`docs/17` §2 U); ADR-0068 madde 3 |
| Tahmini büyüklük | S–M (1 yeni başlık, 1 yeni test dosyası, 6 değişen dosya) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

Sunucu bugün istemci sürümünü (1453) ve JvCryption özel anahtarını **derleme zamanında** sabitler. 1534 istemcisi (`C:\dev\fdp1534\client`) sürüm 1534 ve anahtar `0x1257091582190465` bekler (ADR-0068 Bağlam). Bu plan, ikisini **çalışma zamanında ini'den** seçilebilir yapar; ini anahtarı yoksa veya `0` ise davranış **bugünküyle bayt bayt aynıdır**. Böylece aynı derleme hem eski 1453 istemcisiyle hem 1534 istemcisiyle (yalnız ini değiştirerek) çalışır.

Bu plan **yalnız el sıkışmayı** kapsar (sürüm yanıtı, kripto, giriş sunucusu listesi). Oyun içi paket düzenleri (kullanıcı bilgisi, NPC bilgisi, envanter vb.) sonraki U1 planlarıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0068-surum-yukseltme-1534-hedef-istemci-ve-taban.md` (tamamı).
- `shared/version.h:3` (`#define __VERSION 1453`).
- `shared/JvCryption.cpp:1-15` (özel anahtar `#if __VERSION` makrosu; `CJvCryption::Init`), `shared/JvCryption.h`.
- `shared/KOSocket.cpp:227-233` (`EnableCrypto` → `m_crypto.Init()`).
- `GameServer/LoginHandler.cpp:3-19` (`CUser::VersionCheck`: `result << uint16(__VERSION) << m_crypto.GenerateKey()`).
- `GameServer/GameServerDlg.cpp:225-320` (`GetTimeFromIni`: `CIni ini(CONF_GAME_SERVER)` okumaları).
- `LogInServer/LoginServer.cpp:9` (`m_sLastVersion(__VERSION)`), `:13-50` (`Startup`, `GetInfoFromIni`, "Latest version in database"), `:72-117` (`GetServerList`: `#if __VERSION >= 1888`, `>= 1453`, `< 1600`), `:119-140` (`GetInfoFromIni`).
- `LogInServer/LoginServer.h:15,40` (`GetVersion`, `m_sLastVersion`).
- `LogInServer/LoginSession.cpp:32-37` (`HandleVersion`), `:162-173` (`HandleServerlist`: `#if __VERSION >= 1500` echo), `:191-197` (`HandleSetEncryptionPublicKey`).
- `shared/Ini.cpp:108-120`: `CIni::GetInt` eksik anahtarı varsayılan değerle ini'ye **yazar** (`SetInt` → `Save`). Yani ilk açılışta ini'lere `[PROTOCOL]` bölümü `0` değerleriyle eklenir; bu beklenen davranıştır.
- `GameServer/Bot/BotManager.cpp:3855` (`pUser->EnableCrypto()` bot oturumlarında; bot paketleri kripto katmanından geçmez, etkilenmez).
- Test çatısı: `Tests/BotCoreTests/MiniTest.h`, örnek `Tests/BotCoreTests/MotionTests.cpp`; testler başlıkları `$(SolutionDir)` kökünden ekler (`#include <BotCore/BotMotion.h>`).

## 3. Kapsam

**Var:**
1. Yeni başlık `shared/ProtocolProfile.h`: yalnız standart kütüphane (`<cstdint>`) kullanan, **saf** ve test edilebilir fonksiyonlar + süreç genelinde iki çalışma zamanı değeri (C++17 `inline` değişken).
2. `CJvCryption::Init` özel anahtarı `ProtocolProfile`'dan alır.
3. GameServer: `GetTimeFromIni` içinde `[PROTOCOL] CLIENT_VERSION` ve `[PROTOCOL] CRYPTO_KEY` okunur, profile yazılır; `VersionCheck` etkin sürümü gönderir.
4. LogInServer: `GetInfoFromIni` aynı iki anahtarı `LogInServer.ini`'den okur; `HandleVersion`, `HandleServerlist` (echo) ve `GetServerList` (`>=1888`, `>=1453`, `<1600` dalları) etkin sürüme göre **çalışma zamanında** karar verir.
5. Başlangıçta her iki sunucu tek satır bilgi basar: `Protocol: client version %u (%s), crypto key profile %s` (`legacy`/`ini`).
6. Birim testleri: `Tests/BotCoreTests/ProtocolProfileTests.cpp`.

**Yok:** oyun içi paket düzenleri, `__VERSION` makrosunun kaldırılması (diğer `#if __VERSION` kullanımları — `GameServer/DatabaseThread.cpp:190`, `GameServer/User.cpp:5070,5074` — **olduğu gibi kalır**), DB değişikliği, istemci dosyaları, `C:\dev\fdp\server` dağıtımı, `docs/` (Claude günceller).

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `shared/ProtocolProfile.h` | YENİ (header-only) |
| `shared/JvCryption.cpp` | makro → `ProtocolProfile::PrivateKey()` |
| `shared/shared.vcxproj`, `shared/shared.vcxproj.filters` | yeni başlığı listele (yalnız `ClInclude`) |
| `GameServer/GameServerDlg.cpp` | `GetTimeFromIni`'de `[PROTOCOL]` okuma + tek satır bilgi |
| `GameServer/LoginHandler.cpp` | `VersionCheck` etkin sürüm |
| `LogInServer/LoginServer.cpp` | ini okuma, `GetServerList` çalışma zamanı dalları, tek satır bilgi |
| `LogInServer/LoginSession.cpp` | `HandleVersion` yanıtı, `HandleServerlist` echo |
| `Tests/BotCoreTests/ProtocolProfileTests.cpp` | YENİ |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` (ve varsa `.filters`) | yeni test dosyası |

Başka dosyaya dokunmak gerekirse dur ve raporda sor. **`GameServer/Bot/`, `BotCore/` değişmez.**

## 5. Uygulama adımları

### 5.1 `shared/ProtocolProfile.h` (bağlayıcı arayüz)

Kodlama/satır sonu: `shared/version.h` ile aynı (kontrol et: `file shared/version.h`), CRLF, tab girinti, Allman. Yalnız `<cstdint>`; `shared/types.h`/`stdafx.h` **dahil etme** (test projesi de kullanacak).

```cpp
#pragma once

#include <cstdint>

// Runtime protocol profile (ADR-0068). CLIENT_VERSION 0 = legacy: compile-time
// __VERSION behaviour, unchanged from before this file existed.
namespace ProtocolProfile
{
	// Keys as used by the client executables (see ADR-0068 context).
	const uint64_t kKeyLegacy1453 = 0x7412580096385200ULL; // our local 1453 client (modified exe)
	const uint64_t kKeyUsko1453To1699 = 0x1257091582190465ULL; // official 1453..1534 era, AlphaGame/KODevelopers 1534
	const uint64_t kKey1298To1452 = 0x1234567890123456ULL;
	const uint64_t kKey1700Plus = 0x1207500120128966ULL;

	// Process-wide runtime values; set once at startup from the ini, read-only afterwards.
	inline uint16_t g_configuredClientVersion = 0; // 0 = legacy
	inline uint64_t g_configuredCryptoKey = 0;     // 0 = derive from version

	// Effective client version: configured value, or the compile-time version when 0.
	inline uint16_t EffectiveClientVersion(uint16_t configured, uint16_t compileTimeVersion);

	// Private key for a client version; must reproduce the old #if table for the legacy path:
	// >= 1700 -> kKey1700Plus; 1298..1452 -> kKey1298To1452; 1453 -> kKeyLegacy1453;
	// 1454..1699 -> kKeyUsko1453To1699; anything below 1298 -> kKeyLegacy1453 (old #else branch).
	inline uint64_t PrivateKeyForVersion(uint16_t version);

	// configuredKey != 0 wins; otherwise PrivateKeyForVersion(effectiveVersion).
	inline uint64_t ResolvePrivateKey(uint64_t configuredKey, uint16_t effectiveVersion);

	// Login server rules, formerly #if blocks:
	inline bool ServerListEcho(uint16_t version);          // version >= 1500
	inline bool ServerListLanIp(uint16_t version);         // version >= 1888
	inline bool ServerListExtended(uint16_t version);      // version >= 1453
	inline uint8_t ServerListUnknownByte(uint16_t version); // version < 1600 ? 1 : 0

	// Version reply of LS_VERSION_REQ: configured != 0 -> configured; else the DB's latest version.
	inline int16_t LoginVersionReply(uint16_t configured, int16_t dbLatest);

	// Parses the ini CRYPTO_KEY string: "" or "0" -> 0; hex with or without 0x prefix -> value;
	// anything unparsable -> 0 (caller then derives from version). Never throws.
	inline uint64_t ParseCryptoKey(const char * text);

	// Convenience accessors over the inline globals, using the compile-time version passed in.
	inline uint16_t ClientVersion(uint16_t compileTimeVersion);
	inline uint64_t PrivateKey(uint16_t compileTimeVersion);
}
```

Gövdeler aynı başlıkta (`inline`). `ParseCryptoKey` için `strtoull(text, &end, 16)` kullan; baştaki `0x`/`0X`'i atla; boş/geçersiz → 0. `ClientVersion(cv)` = `EffectiveClientVersion(g_configuredClientVersion, cv)`; `PrivateKey(cv)` = `ResolvePrivateKey(g_configuredCryptoKey, ClientVersion(cv))`.

### 5.2 `shared/JvCryption.cpp`

`#define g_private_key` bloğunu kaldır; `#include "ProtocolProfile.h"` ekle; `Init()`:
```cpp
void CJvCryption::Init() { m_tkey = m_public_key ^ ProtocolProfile::PrivateKey(__VERSION); }
```
Eski tablonun 1453 derlemesindeki sonucu (`0x7412580096385200`) legacy yolda aynen çıkmalı (K3).

### 5.3 GameServer

- `GetTimeFromIni` (`GameServerDlg.cpp`), ODBC okumalarından sonra uygun yere:
  - `ProtocolProfile::g_configuredClientVersion = (uint16) ini.GetInt("PROTOCOL", "CLIENT_VERSION", 0);` (negatif/65535 üstü değerleri 0'a indir).
  - `std::string strKey; ini.GetString("PROTOCOL", "CRYPTO_KEY", "0", strKey, false); ProtocolProfile::g_configuredCryptoKey = ProtocolProfile::ParseCryptoKey(strKey.c_str());`
  - Tek satır: `printf("Protocol: client version %u (%s), crypto key profile %s\n", ...)` (`legacy` / `ini`). Anahtarın değerini **yazma**.
- `LoginHandler.cpp` `VersionCheck`: `result << uint16(ProtocolProfile::ClientVersion(__VERSION)) << m_crypto.GenerateKey();`
- `GetTimeFromIni`'nin `VersionCheck`/`EnableCrypto`'dan önce çalıştığını (sunucu başlangıcı) kodda doğrula ve raporda satır numarasıyla yaz.

### 5.4 LogInServer

- `GetInfoFromIni` (`LoginServer.cpp`): aynı iki anahtar (`CIni ini(CONF_LOGIN_SERVER)`); tek satır bilgi.
- `GetServerList` (`LoginServer.cpp:72-117`): üç `#if` dalını `const uint16 v = ProtocolProfile::ClientVersion(__VERSION);` ile çalışma zamanı `if`'e çevir (`ServerListLanIp`, `ServerListExtended`, `ServerListUnknownByte`). Yazılan alanların sırası ve tipleri **aynı kalır**.
- `LoginSession::HandleServerlist`: `#if __VERSION >= 1500` bloğu → `if (ProtocolProfile::ServerListEcho(v)) { uint16 echo; pkt >> echo; result << echo; }`.
- `LoginSession::HandleVersion`: `result << ProtocolProfile::LoginVersionReply(ProtocolProfile::g_configuredClientVersion, g_pMain->GetVersion());` — legacy (`0`) iken DB'deki en yüksek sürüm (bugünkü davranış).
- `HandlePatches` **değişmez**.

### 5.5 Birim testleri (`Tests/BotCoreTests/ProtocolProfileTests.cpp`)

`#include <shared/ProtocolProfile.h>`; mevcut test kalıbıyla (MiniTest). En az şu durumlar:
1. Legacy eşdeğerliği: `PrivateKeyForVersion(1453) == 0x7412580096385200`; `(1298)`, `(1452)` → `0x1234567890123456`; `(1700)`, `(2369)` → `0x1207500120128966`; `(1534)`, `(1454)`, `(1699)` → `0x1257091582190465`; `(1097)` → `0x7412580096385200`.
2. `EffectiveClientVersion(0, 1453) == 1453`, `(1534, 1453) == 1534`.
3. `ResolvePrivateKey(0, 1534) == 0x1257091582190465`; `ResolvePrivateKey(0xABCDEFULL, 1534) == 0xABCDEF`.
4. Giriş kuralları: `ServerListEcho(1453)==false`, `(1500)==true`, `(1534)==true`; `ServerListLanIp(1887)==false`, `(1888)==true`; `ServerListExtended(1452)==false`, `(1453)==true`; `ServerListUnknownByte(1534)==1`, `(1600)==0`.
5. `LoginVersionReply(0, 1473) == 1473`; `LoginVersionReply(1534, 1473) == 1534`.
6. `ParseCryptoKey`: `""`→0, `"0"`→0, `"1257091582190465"`→`0x1257091582190465`, `"0x1257091582190465"`→ aynı, `"zz"`→0, `nullptr`→0 (null güvenli olmalı).
7. Testler global değişkenleri değiştirirse sonunda eski değerlerine geri koy.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `./tools/build.sh Debug` hatasız; **yeni uyarı yok** (son 10 satırı yapıştır).
- [ ] K2: `./tools/run-tests.sh Release` → `N tests, 0 failed`; N, `main`'deki sayıdan (2752 ± döngü farkı; önce `main`'de bir kez `./tools/run-tests.sh Release --no-build` sonrasında say ve yaz) **büyük**, fark yeni testlerin sayısına eşit.
- [ ] K3: Legacy bayt eşdeğerliği kod düzeyinde: `CLIENT_VERSION=0`, `CRYPTO_KEY=0` iken (a) `VersionCheck` 1453 gönderir, (b) anahtar `0x7412580096385200`, (c) `HandleServerlist` echo okumaz/yazmaz, (d) `GetServerList` yazdığı alanlar eskisiyle aynı (≥1453 bloğu var, LAN IP yok, bilinmeyen bayt 1), (e) `HandleVersion` DB'deki en yüksek sürümü döner. Her birini test adı veya kod satırıyla eşle.
- [ ] K4: 1534 profili: `CLIENT_VERSION=1534` iken sürüm 1534, anahtar `0x1257091582190465`, echo var, bilinmeyen bayt 1, `HandleVersion` 1534. Testlerle göster.
- [ ] K5: Kapsam: `git diff --stat main...bot/U1-01` yalnız §4 dosyalarını içerir; `GameServer/Bot/`, `BotCore/`, `db/`, `docs/` değişmemiş.
- [ ] K6: Kodlama/satır sonu korunur: `file` çıktısı her değişen dosya için öncekiyle aynı (`git show main:<dosya> | file -` ile karşılaştır); yeni dosyalar CRLF.
- [ ] K7: Anahtar değeri hiçbir `printf`/log satırında yok (`grep -n "printf" ` ile göster).
- [ ] K8: `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
git diff --stat main...bot/U1-01
git diff main...bot/U1-01 -- shared/JvCryption.cpp GameServer/LoginHandler.cpp LogInServer/LoginSession.cpp
grep -n -a "PROTOCOL\|ProtocolProfile" GameServer/GameServerDlg.cpp LogInServer/LoginServer.cpp
file shared/ProtocolProfile.h Tests/BotCoreTests/ProtocolProfileTests.cpp
git status --short
```

## 8. Kısıtlar ve uyarılar

- **Oyun mekaniği değişmez.** Bu plan yalnız el sıkışma katmanıdır.
- `__VERSION` makrosu ve diğer `#if __VERSION` kullanımları kalır.
- Çalışma zamanı değerleri yalnız başlangıçta (ini okunurken) yazılır; IOCP thread'leri yalnız okur. Ek kilit gerekmez; bunu raporda bir cümleyle belirt.
- `C:\dev\fdp\server` dizinine kopyalama/dağıtım **yapma**; sunucuları başlatma (Claude yapacak).
- Git: `AGENTS.md` §2.8 (push/merge yok). Commit mesajı `[U1-01] ...`.
- ISO-8859 dosyalarda `grep -a`; kodlamayı değiştirme.

---

## Uygulayıcı Raporu

### Tur 1

**Durum:** UYGULANDI · **Branch:** `bot/U1-01` (taban `main` @ `1eff1bfe`, ayrı çalışma ağacı `/mnt/c/dev/fdp-u1-01`) · **Uygulayıcı:** Claude ajanı (bu faz için proje sahibinin isteğiyle)

**Commit'ler:**
- `096f2b63` [U1-01] Durum UYGULANIYOR
- `aedf7f08` [U1-01] Runtime protocol profile: client version and crypto key from [PROTOCOL] ini
- `42f5f3d3` [U1-01] ProtocolProfile unit tests (9 cases ...)
- (bu rapor + `Durum: UYGULANDI` ayrı commit)

**Değişen dosyalar (satırlar HEAD'e göre):**
- `shared/ProtocolProfile.h` (YENİ, 134 satır): §5.1 arayüzü birebir; gövdeler aynı başlıkta `inline`. Sabitler, iki `inline` global, `EffectiveClientVersion`, `PrivateKeyForVersion`, `ResolvePrivateKey`, `ServerListEcho/LanIp/Extended/UnknownByte`, `LoginVersionReply`, `ParseCryptoKey`, `ClientVersion`, `PrivateKey`. Proje başlığı dahil edilmedi (yalnız `<cstdint>`, `<cstdlib>`).
- `shared/JvCryption.cpp:4,6-7`: `#define g_private_key` bloğu kaldırıldı; `Init()` → `m_public_key ^ ProtocolProfile::PrivateKey(__VERSION)`.
- `shared/shared.vcxproj:150`: `<ClInclude Include="ProtocolProfile.h" />`.
- `GameServer/GameServerDlg.cpp:9` include; `:250-265` `GetTimeFromIni` içinde ODBC/MARS okumalarından hemen sonra (ilk erken `return`'den önce) `[PROTOCOL] CLIENT_VERSION` (negatif / 0xFFFF üstü → 0) ve `CRYPTO_KEY` okuma + tek satır `Protocol: ...` bilgisi.
- `GameServer/LoginHandler.cpp:2` include; `:15` `VersionCheck` → `uint16(ProtocolProfile::ClientVersion(__VERSION))`.
- `LogInServer/LoginServer.cpp:5` include; `:86` `const uint16 v = ProtocolProfile::ClientVersion(__VERSION);`; `:95-96` LAN IP, `:104-114` ≥1453 bloğu ve bilinmeyen bayt çalışma zamanı `if`'i (alan sırası/tipleri aynı); `:131-146` `GetInfoFromIni` içinde aynı iki anahtar + tek satır bilgi.
- `LogInServer/LoginSession.cpp:3` include; `:36` `HandleVersion` → `LoginVersionReply(g_configuredClientVersion, GetVersion())`; `:167-173` `HandleServerlist` echo çalışma zamanı `if`'i. `HandlePatches` değişmedi.
- `Tests/BotCoreTests/ProtocolProfileTests.cpp` (YENİ, 170 satır, 9 `TEST_CASE`); `Tests/BotCoreTests/BotCoreTests.vcxproj:100` `ClCompile`.

**Başlangıç sırası (§5.3 isteği):** GameServer'da `GetTimeFromIni()` `GameServerDlg.cpp:90`'da, `m_socketMgr.Listen` (`:101`) ve `BotManager::Instance().Startup()` (`:107`) **önce** çağrılır; `VersionCheck` (`User.cpp:286`) ve bot `EnableCrypto` (`Bot/BotManager.cpp:3855`) ancak bunlardan sonra çalışabilir. LogInServer'da `GetInfoFromIni()` `LoginServer.cpp:16`'da, `Listen` (`:52`) ve `UpdateServerList`'i çağıran `Timer_UpdateUserCount` iş parçacığı (`:59`) **önce**. Değerler yalnız bu başlangıç noktasında yazılır, IOCP/zamanlayıcı iş parçacıkları yalnız okur; bu yüzden ek kilit gerekmez.

**Derleme / test çıktıları:**
- Taban (değişiklikten önce, bu dalda, `main` @ `1eff1bfe` içeriği): `./tools/run-tests.sh Release` → `331 tests, 0 failed` (rc=0). Taban Release derlemesinde 68 uyarı (hepsi mevcut kod).
- `./tools/build.sh Release` (rc=0), son satırlar:
  ```
      0 functions were new in current compilation
      4 functions had inline decision re-evaluated but remain unchanged
    Kodun üretilmesi tamamlandı
    Kod üretiliyor
    proj-AIServer.vcxproj -> C:\dev\fdp-u1-01\build\bin\x86-Release\Server\AIServer.exe
    13 of 14838 functions (<0.1%) were compiled, the rest were copied from previous compilation.
      6 functions were new in current compilation
      70 functions had inline decision re-evaluated but remain unchanged
    Kodun üretilmesi tamamlandı
    proj-GameServer.vcxproj -> C:\dev\fdp-u1-01\build\bin\x86-Release\Server\GameServer.exe
  ```
  Artımlı derlemede 6 uyarı; hepsi tabandaki uyarıların eklenen satırlar kadar kaymış hâli: `LoginServer.cpp(245→261) C4834`, `LoginSession.cpp(84→85) C4267`, `GameServerDlg.cpp(820→838) C4834`, `(1147→1165) C4267`, `(1806→1824) C4267`, `LoginHandler.cpp(38→39) C4267`. `ProtocolProfile.h`, `JvCryption.cpp`, `ProtocolProfileTests.cpp` (W4) için uyarı yok.
- `./tools/build.sh Debug` (rc=0, bu çalışma ağacında sıfırdan tam derleme), son satırlar:
  ```
    TradeHandler.cpp
    Unit.cpp
    UpgradeHandler.cpp
    User.cpp
    Kod Üretiliyor...
    proj-GameServer.vcxproj -> C:\dev\fdp-u1-01\build\bin\x86-Debug\Server\GameServer.exe
  ```
  7 uyarı: dördü yukarıdaki kaymış C4267'ler (`LoginSession.cpp(85)`, `GameServerDlg.cpp(1165)`, `(1824)`, `LoginHandler.cpp(39)`), üçü dokunulmayan dosyalarda (`DBAgent.cpp(1809)`, `EventHandler.cpp(267)`, `MagicInstance.cpp(1867)`). Yeni uyarı yok.
- `./tools/run-tests.sh Release` → `340 tests, 0 failed` (rc=0).
- `./tools/run-tests.sh Debug --no-build Protocol_` → `9 tests, 0 failed`.

**Kabul kriterleri:**
- ✔ K1: Release ve Debug hatasız (rc=0), yeni uyarı yok (yukarıdaki karşılaştırma).
- ✔ K2: `340 tests, 0 failed`; taban 331; fark 9 = yeni `TEST_CASE` sayısı (`Protocol_PrivateKeyForVersionMatchesLegacyTable`, `_EffectiveClientVersion`, `_ResolvePrivateKey`, `_LoginServerListRules`, `_LoginVersionReply`, `_ParseCryptoKey`, `_LegacyProfileHandshake`, `_Client1534ProfileHandshake`, `_ConfiguredCryptoKeyOverridesVersion`).
- ✔ K3 (legacy, `CLIENT_VERSION=0`, `CRYPTO_KEY=0`), test `Protocol_LegacyProfileHandshake` (`ProtocolProfileTests.cpp:119`) + kod:
  - (a) `ClientVersion(1453)==1453` → `LoginHandler.cpp:15` aynı `uint16` 1453'ü yazar.
  - (b) `PrivateKey(1453)==0x7412580096385200` → `JvCryption.cpp:7`; tablo eşdeğerliği ayrıca `Protocol_PrivateKeyForVersionMatchesLegacyTable`.
  - (c) `ServerListEcho(1453)==false` → `LoginSession.cpp:168` echo okumaz/yazmaz.
  - (d) `ServerListLanIp(1453)==false`, `ServerListExtended(1453)==true`, `ServerListUnknownByte(1453)==1` → `LoginServer.cpp:95,104,109`: eski `#if` 1453 derlemesiyle aynı alanlar, aynı sıra ve tipler (`uint8(...)` korunmuş).
  - (e) `LoginVersionReply(0, 1473)==1473` → `LoginSession.cpp:36` DB'deki en yüksek sürümü (`GetVersion()`, `int16`) aynı tiple yazar.
- ✔ K4 (`CLIENT_VERSION=1534`), test `Protocol_Client1534ProfileHandshake` (`:137`): sürüm 1534, anahtar `0x1257091582190465`, echo var, LAN IP yok, ≥1453 bloğu var, bilinmeyen bayt 1, `HandleVersion` 1534. `CRYPTO_KEY` ini değerinin sürüm türetmesini ezdiği: `Protocol_ConfiguredCryptoKeyOverridesVersion` (`:153`).
- ✔ K5: `git diff --stat main...bot/U1-01` → yalnız §4 dosyaları + bu plan dosyası (Durum/rapor): `GameServer/GameServerDlg.cpp`, `GameServer/LoginHandler.cpp`, `LogInServer/LoginServer.cpp`, `LogInServer/LoginSession.cpp`, `Tests/BotCoreTests/BotCoreTests.vcxproj`, `Tests/BotCoreTests/ProtocolProfileTests.cpp`, `shared/JvCryption.cpp`, `shared/ProtocolProfile.h`, `shared/shared.vcxproj`, plan. `GameServer/Bot/`, `BotCore/`, `db/`, `docs/` değişmedi (`git diff --name-only ... | grep -E '^(GameServer/Bot/|BotCore/|db/|docs/)'` boş).
- ✔ K6: `git show main:<f> | file -` ile `git show HEAD:<f> | file -` 7 değişen dosyanın hepsinde AYNI (depo `core.autocrlf=true`: dizin LF, çalışma ağacı CRLF; çalışma ağacında hepsi "with CRLF line terminators"). BOM'lu üç dosyada (`GameServerDlg.cpp`, `shared.vcxproj`, `BotCoreTests.vcxproj`) BOM `efbbbf` main ve HEAD'de duruyor. Yeni dosyalar: `shared/ProtocolProfile.h: C++ source, ASCII text, with CRLF line terminators`, `Tests/BotCoreTests/ProtocolProfileTests.cpp: C source, ASCII text, with CRLF line terminators`.
- ✔ K7: Eklenen tek `printf`'ler `GameServerDlg.cpp:262-265` ve `LoginServer.cpp:143-146`; argümanlar sürüm sayısı ve `"ini"`/`"legacy"` dizgeleri. `ProtocolProfile.h`/`JvCryption.cpp` içinde `printf` yok; `printf.*(CryptoKey|PrivateKey|0x1257|0x7412)` araması boş.
- ✔ K8: `git status --short` temiz (son commit sonrası).

**Plandan sapmalar:**
1. `shared/ProtocolProfile.h` `<cstdint>`'e ek olarak `<cstdlib>` içerir: §5.1'in istediği `strtoull` bu başlıktadır. Yine yalnız standart kütüphane; `types.h`/`stdafx.h` dahil değil.
2. §5.4 `#if` dallarını `GetServerList` (`LoginServer.cpp:72-117`) içinde tarif ediyor; kodda üç dal `LoginServer::UpdateServerList()`'tedir (aynı satır aralığı, `:78-117`). `GetServerList` yalnız önbellekteki paketi ekler, değişmedi. Dönüşüm `UpdateServerList`'te yapıldı (aynı dosya, aynı üç dal).
3. `shared/shared.vcxproj.filters` depoda yok; oluşturulmadı. Yeni başlık yalnız `shared.vcxproj`'a `ClInclude` olarak eklendi. `BotCoreTests` için de `.filters` yok.
4. `ParseCryptoKey` plandaki asgari tariften daha sıkı: baştaki/sondaki boşluğu tolere eder; işaret (`"-1"`), sonda çöp (`"12zz"`), çift önek (`"0x0x12"`) ve 16'dan fazla anlamlı hex hane (taşma) → 0. Hepsi `Protocol_ParseCryptoKey`'de test edildi.

**Açık sorular / notlar:**
1. K2'deki "2752" tabanı bu dalda yok: `main` @ `1eff1bfe` üzerinde `run-tests.sh Release` `331 tests` raporluyor. 2752 başka bir dalın veya CHECK sayısının değeri olabilir; karşılaştırma 331 → 340 ile yapıldı.
2. `PrivateKeyForVersion` 1454..1699 için (planın bağlayıcı tablosu gereği) `0x1257091582190465` döner. Eski `#else` dalı bu sürümlerde `0x7412580096385200` verirdi. Bu yalnız derleme zamanı `__VERSION`'ı 1453 olmayan bir derlemeyi veya ini'de 1454..1699 verilmesini etkiler; legacy 1453 yolu bayt bayt aynı (K3 b).
3. İlk açılışta `CIni` eksik anahtarları yazacağı için `GameServer.ini` ve `LogInServer.ini`'ye `[PROTOCOL] CLIENT_VERSION=0`, `CRYPTO_KEY=0` eklenecek (planda beklenen davranış). Sunucular başlatılmadı, dağıtım yapılmadı; gerçek konsol satırı Claude'un sunucu denemesinde görülecek.

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-08

**Hüküm: DOĞRULANDI.** Kanıt: kendi koşum (aşağıdaki komutlar) + kod incelemesi.

| K | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | Uygulayıcı: Release/Debug hatasız, yeni uyarı yok (değişen dosyalardaki uyarılar yalnız satır kayması). Ayrıca birleşik hatta (`yukseltme/1534`) `./tools/build.sh Release` rc=0; uyarılar önceden var olan C4834/C4789 |
| K2 | ✔ | `bot/U1-01`: `./tools/run-tests.sh Release --no-build` → `340 tests, 0 failed` (taban `main` 331 + 9). Birleşik hat (`gece/2026-10-08-kalabalik` + U1-01): `3470 tests, 0 failed`; kalabalık tabanı aynı koşuda `3461 tests, 0 failed` → fark tam 9 (Protocol_*) |
| K3 | ✔ | Kod incelemesi: legacy yolda `ClientVersion(__VERSION)` = 1453, `PrivateKeyForVersion(1453)` = eski `#else` anahtarı; `UpdateServerList` (planın "GetServerList" dediği yer; sapma 2 kabul) alan sırası/tipleri aynı, bilinmeyen bayt 1, LAN IP yok; `HandleServerlist` echo yalnız ≥ 1500; `HandleVersion` `LoginVersionReply(0, db)` = DB değeri, tip `int16` (eski `short`) → aynı 2 bayt. Test `Protocol_LegacyProfileHandshake` |
| K4 | ✔ | `Protocol_Client1534ProfileHandshake`: 1534, `0x1257…`, echo var, bayt 1, yanıt 1534 |
| K5 | ✔ | `git diff --stat main...bot/U1-01`: yalnız §4 dosyaları + plan; `GameServer/Bot`, `BotCore`, `db`, `docs` yok |
| K6 | ✔ | Uygulayıcı `file` karşılaştırması; birleşik hatta `BotCoreTests.vcxproj` BOM+CRLF korundu |
| K7 | ✔ | İki `printf` yalnız sürüm ve `ini`/`legacy` yazar |
| K8 | ✔ | `git status --short` temiz |

Sapmalar (kabul): (1) `<cstdlib>` eklendi (`strtoull`); (2) `#if` dalları `UpdateServerList`'te, `GetServerList` değişmedi; (3) `shared.vcxproj.filters` yok, oluşturulmadı; (4) `ParseCryptoKey` plandan katı — testli. Not: 1454–1699 için anahtar artık `0x1257…` (eski kodda bu sürümler derlenmiyordu; yalnız 1453 yolu bağlayıcıydı).

**Birleşik hat:** `yukseltme/1534` = `gece/2026-10-08-kalabalik` (tüm gece hatları) + `bot/U1-01` (merge `020e4a1d`); çakışmalar yalnız `BotCoreTests.vcxproj`, `KNOWN_ISSUES.md`, `STATUS.md` (iki taraf da satır ekledi; ikisi de korundu).

**Çalışma zamanı:** henüz denenmedi (T-UPG-01 insan testi; ayrı sunucu dizini `C:\dev\fdp1534\server` ile).
