# U1-01: Çalışma zamanı protokol profili — istemci sürümü ve kripto anahtarı ini'den (`[PROTOCOL] CLIENT_VERSION`)

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
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

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
