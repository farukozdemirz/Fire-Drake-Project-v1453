# F0-01: Ortam doğrulama araçları ve Debug/Release farkı raporu

| Alan | Değer |
|---|---|
| Durum | KAPANDI (main @ `43d3500`) |
| Faz | F0 — Ortam ve temel doğrulama (`docs/17` §2) |
| Branch | `bot/F0-01` (taban: `main`) |
| Bağımlı olduğu planlar | — (ilk plan) |
| İlgili gereksinim / kabul | T-ENV-01, T-ENV-02 (`docs/15` §4.1); REQ-TST-01 |
| Tahmini büyüklük | S (2 yeni betik, kod değişikliği yok) |
| Hazırlayan / tarih | Claude / 2026-10-01 |

---

## 1. Amaç

Projeyi kuran kişinin (veya yeni bir ajanın) ortamın hazır olup olmadığını **tek komutla** görebilmesi ve Debug/Release derlemeleri arasındaki davranış farklarının kaynaktan **otomatik** çıkarılması.

Bu plan sonunda iki betik olacak:

- `tools/check-env.sh` — ortam sağlık kontrolü (derleyici, depo, çalışma klasörü, veritabanı, ODBC).
- `tools/debug-release-diff.sh` — kaynak koddaki `DEBUG`/`_DEBUG` koşullu bloklarını ve vcxproj tanımlarını listeleyen rapor.

Bu plan **sunucu kodunu değiştirmez.** Sunucuyu çalıştırmak ve istemciyle oyuna girmek bu planın dışındadır; onu proje sahibi elle yapacak.

## 2. Bağlam (okunması zorunlu)

- `AGENTS.md` — tüm kurallar, özellikle §2.7 (veritabanı) ve §3 (kod kuralları).
- `docs/STATUS.md` — ortamın bilinen durumu.
- `start.md` (depo kökü, izlenmeyen dosya) — kurulumun nasıl yapıldığı; betiklerin kontrol edeceği adımlar buradan gelir.
- `tools/build.sh` — mevcut derleme betiği; yeni betikler aynı üslupta yazılacak (`set -euo pipefail`, ortam değişkeniyle override).

Doğrulanmış ortam gerçekleri (bu plan yazılırken kontrol edildi, varsayım değil):

| Gerçek | Değer |
|---|---|
| Derleyici | `/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe` |
| Release çıktı | `build/bin/x86-Release/Server/{AIServer,GameServer,LogInServer}.exe` |
| Debug çıktı | `build/bin/x86-Debug/Server/...` (Debug derlemesi sorunsuz çalışıyor) |
| Çalışma klasörü | `/mnt/c/dev/fdp` → `server/` (3 exe, 3 ini, `Map/` 24 adet `.smd`, `Quests/` 114 adet `.lua`), `Client/` |
| Veritabanı | SQL Server Express `.\SQLEXPRESS`, veritabanı adı `FDP_kn_online` |
| sqlcmd | `/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE` |
| ODBC DSN | `KO_MAIN` ve `KO_GAME` (32-bit) mevcut |
| Sürüm sabiti | `shared/version.h` → `#define __VERSION 1453` |

## 3. Kapsam

**Yapılacaklar**

- `tools/check-env.sh` yazmak (§5.1).
- `tools/debug-release-diff.sh` yazmak (§5.2).
- İkisini de çalıştırıp çıktılarını Uygulayıcı Raporu'na yapıştırmak.

**Kapsam dışı (yapılmayacak)**

- `GameServer/`, `AIServer/`, `LogInServer/`, `shared/`, `N3BASE/` altında **hiçbir değişiklik**. Bu plan kod değiştirmez.
- `docs/` altına yazmak. Rapordan dokümanı Claude üretecek.
- Sunucuyu başlatmak, oyun istemcisini açmak, oyuna girmek.
- Veritabanına yazmak (`UPDATE`/`INSERT`/`ALTER` yasak; yalnızca `SELECT`).
- `tools/build.sh`'yi değiştirmek.
- Eksik çıkan bir şeyi "düzeltmek" (ör. DSN oluşturmak, dosya kopyalamak). Betik yalnızca **raporlar**.
- CI yapılandırması, yeni bağımlılık, paket kurulumu.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/check-env.sh` | yeni | `opencode.json`'da `tools/*` izni "ask"; proje sahibi onay verecek |
| `tools/debug-release-diff.sh` | yeni | aynı |
| `plans/F0-01-ortam-dogrulama-araclari.md` | değiştir | yalnızca `Durum` satırı ve "Uygulayıcı Raporu" bölümü |

Bu listede olmayan bir dosyaya dokunmak gerekirse **dur** ve Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.0 Hazırlık

1. `git switch -c bot/F0-01 main`
2. Bu dosyadaki `Durum` satırını `UYGULANIYOR` yap.

### 5.1 `tools/check-env.sh`

Ortak kurallar:

- `#!/usr/bin/env bash` + `set -euo pipefail`. Yalnızca bash, coreutils, grep, sed/awk kullan. Yeni bağımlılık yok.
- Satır sonu **LF** (`.gitattributes` zaten `*.sh text eol=lf` diyor). Dosyayı `chmod +x` yap.
- Yol değişkenleri ortamdan override edilebilir olmalı:
  - `FDP_RUNTIME_DIR` (varsayılan `/mnt/c/dev/fdp`)
  - `MSBUILD` (varsayılan §2'deki yol)
  - `SQLCMD` (varsayılan §2'deki yol)
  - `FDP_SQL_INSTANCE` (varsayılan `.\SQLEXPRESS`), `FDP_SQL_DB` (varsayılan `FDP_kn_online`)
- Argümanlar: `--skip-db` (veritabanı kontrollerini atla), `-h|--help`.
- **Gizli bilgi yazdırma yasağı:** `.ini` dosyalarının içeriğini, kullanıcı adı/parola alanlarını, bağlantı dizelerini ekrana yazma. İni dosyaları için yalnızca "dosya var mı" ve "şu anahtar tanımlı mı" kontrolü yap, **değerini yazdırma**.

Çıktı biçimi — her kontrol tek satır:

```
[PASS] T-01  MSBuild bulundu            /mnt/c/.../MSBuild.exe
[FAIL] R-03  version.h __VERSION 1453   bulunan: 1298
[SKIP] D-02  MAGIC tablosu              (--skip-db)
```

Sonda özet: `Özet: 18 PASS, 1 FAIL, 2 SKIP`. **Çıkış kodu:** en az bir `FAIL` varsa `1`, yoksa `0`. `SKIP` çıkış kodunu etkilemez.

Kontroller (kimlikleri aynen kullan):

**T — Araç zinciri**

| ID | Kontrol |
|---|---|
| T-01 | `$MSBUILD` dosyası var |
| T-02 | `tools/build.sh` var ve çalıştırılabilir (`-x`) |
| T-03 | `git` komutu var |

**R — Depo**

| ID | Kontrol |
|---|---|
| R-01 | Depo kökü bulundu (`git rev-parse --show-toplevel`) ve içinde `KnightOnlineServer.sln` var |
| R-02 | HEAD kısa SHA'sı ve dal adı yazdırılır (bu kontrol her zaman PASS, bilgi amaçlı) |
| R-03 | `shared/version.h` içinde `__VERSION` değeri `1453` |
| R-04 | Çalışma ağacı temiz mi (`git status --porcelain` boş mu). Kirliyse `PASS` değil, bilgi amaçlı `WARN` yaz ve çıkış kodunu etkileme |

**W — Çalışma klasörü** (`$FDP_RUNTIME_DIR`)

| ID | Kontrol |
|---|---|
| W-01 | `server/` klasörü var |
| W-02 | `server/AIServer.exe`, `server/GameServer.exe`, `server/LogInServer.exe` üçü de var (eksikleri listele) |
| W-03 | `server/AIServer.ini`, `server/GameServer.ini`, `server/LogInServer.ini` üçü de var |
| W-04 | `server/GameServer.ini` içinde `[ODBC]`, `[ZONE_INFO]`, `[AI_SERVER]` bölümleri tanımlı (**değerleri yazdırma**) |
| W-05 | `server/Map/*.smd` sayısı ≥ 24 (bulunan sayıyı yaz) |
| W-06 | `server/Map/freezone_a_20050718.smd` var (Ronark Land haritası) |
| W-07 | `server/Quests/*.lua` sayısı ≥ 114 (bulunan sayıyı yaz) |
| W-08 | `Client/KnightOnline.exe` ve `Client/Server.ini` var |

**D — Veritabanı** (`--skip-db` verilirse hepsi SKIP)

Bağlantı biçimi (Windows kimlik doğrulaması):

```bash
"$SQLCMD" -S "$FDP_SQL_INSTANCE" -E -d "$FDP_SQL_DB" -h -1 -W -Q "SET NOCOUNT ON; <sorgu>"
```

| ID | Kontrol | Sorgu / beklenen |
|---|---|---|
| D-01 | sqlcmd var ve veritabanına bağlanabiliyor | `SELECT 1` |
| D-02 | Zorunlu tablolar var | `sys.tables` içinde şunların hepsi: `MAGIC`, `MAGIC_TYPE1`, `MAGIC_TYPE3`, `MAGIC_TYPE4`, `ITEM`, `COEFFICIENT`, `LEVEL_UP`, `ZONE_INFO`, `K_NPCPOS`, `START_POSITION` — eksikleri listele |
| D-03 | Ronark Land harita kaydı | `SELECT RTRIM(strZoneName) FROM ZONE_INFO WHERE ZoneNo = 71` → `freezone_a_20050718.smd` |
| D-04 | KI-001 (MAGIC.Etc) durumu | `SELECT COUNT(*) FROM MAGIC WHERE Etc = 1` → `0` ise PASS. `0` değilse **FAIL** ve satıra şunu ekle: `KI-001 düzeltmesi uygulanmamış (bkz. docs/KNOWN_ISSUES.md)` |
| D-05 | İstemci sürüm uyumu | `SELECT MAX(sVersion) FROM VERSION` değerini oku. `$FDP_RUNTIME_DIR/Client/Server.ini` içindeki `[Version] Files=` değerini oku. Eşitse PASS, değilse FAIL (iki değeri de yaz — bunlar gizli bilgi değil) |

**Veritabanı kuralı (AGENTS.md §2.7):** Yalnızca yukarıdaki sorgular. `TB_USER`, `ACCOUNT_CHAR`, `USERDATA`, `USER_*`, `WAREHOUSE*`, `MAIL_*`, `FRIEND_LIST`, `PUS_*`, `_SN_*`, `WEB_*`, `KNIGHTS*`, `KING_*` tablolarına **hiç dokunma**.

**O — ODBC**

| ID | Kontrol |
|---|---|
| O-01 | 32-bit `KO_MAIN` DSN tanımlı |
| O-02 | 32-bit `KO_GAME` DSN tanımlı |

Komut: `powershell.exe -NoProfile -Command "Get-OdbcDsn -Platform '32-bit' \| Select-Object -ExpandProperty Name"` çıktısında adı ara. `powershell.exe` yoksa ikisini de `SKIP` yap (WSL dışı ortam).

### 5.2 `tools/debug-release-diff.sh`

Amaç: Release ve Debug derlemeleri arasındaki **davranış farklarının** kaynaktaki yerlerini otomatik çıkarmak.

- Aynı ortak kurallar (shebang, `set -euo pipefail`, LF, `chmod +x`).
- Argüman: `-h|--help`. Çıktı stdout'a markdown.
- **Sonuçları kodun içine gömmek (hardcode) yasaktır.** Her satır `grep` ile kaynaktan üretilmelidir. Dosya veya satır numarası sabit yazılmış bir betik reddedilir.

Rapor üç bölümden oluşur:

**Bölüm 1 — Koşullu bloklar.** `GameServer/ AIServer/ LogInServer/ shared/ N3BASE/` altındaki `*.cpp` ve `*.h` dosyalarında, önişlemci koşulu (`#if`, `#ifdef`, `#ifndef`, `#elif`) şu belirteçlerden birini içeren satırları bul: `DEBUG`, `_DEBUG`, `NDEBUG`, `DISABLE_PLAYER_BLINKING`, `USE_SQL_TRACE`.

> `USE_SQL_TRACE` listede, çünkü yalnızca `shared/stdafx.h`'nin debug bloğunda tanımlanıyor; yani o da bir Debug/Release farkıdır. Her biri için `dosya:satır` + satırın kendisi + varsa aynı satırdaki `//` yorumu yazdır.

- **Önemli:** `GameServer/NPCHandler.cpp`, `GameServer/QuestHandler.cpp`, `GameServer/MerchantHandler.cpp` ve `shared/packets.h` ISO-8859 kodlamalıdır; düz `grep` bunları ikili dosya sanıp atlar. **`grep -a` kullan** (`AGENTS.md` §3).
- Çıktıdaki `\r` (CR) karakterlerini temizle (dosyalar CRLF).

**Bölüm 2 — vcxproj önişlemci tanımları.** Dört proje dosyasından (`GameServer/proj-GameServer.vcxproj`, `AIServer/proj-AIServer.vcxproj`, `LogInServer/proj-LogInServer.vcxproj`, `shared/shared.vcxproj`) `<PreprocessorDefinitions>` satırlarını çıkar ve proje × yapılandırma tablosu olarak yazdır.

**Bölüm 3 — Özet tablo.** Bölüm 1'deki her koşul için tek satır: `dosya:satır`, koşul, **Debug'da ne olur / Release'de ne olur**. Bu sütunu betik üretemez; bu yüzden betik bu sütunu `(elle doldurulacak)` olarak bırakır, sen de Uygulayıcı Raporu'nda kendi okuduğun koda göre doldurursun (kaynağı okuyarak, tahmin etmeden).

### 5.3 Çalıştır ve raporla

1. `./tools/check-env.sh` çalıştır, çıktısını kaydet.
2. `./tools/check-env.sh --skip-db` çalıştır (SKIP yolu çalışıyor mu).
3. Hata yolunu sına: `FDP_RUNTIME_DIR=/tmp/yok-boyle-bir-yer ./tools/check-env.sh --skip-db` → `W-*` kontrolleri FAIL vermeli ve çıkış kodu `1` olmalı (`echo $?` ile göster).
4. `./tools/debug-release-diff.sh` çalıştır, çıktısını kaydet.
5. `./tools/build.sh Release` ve `./tools/build.sh Debug` çalıştır.
6. Hepsini Uygulayıcı Raporu'na yapıştır (uzun çıktıları kırpma; `check-env.sh` çıktısı kısa olmalı zaten).

## 6. Kabul kriterleri

- [ ] **K1** `./tools/build.sh Release` hatasız biter.
- [ ] **K2** `./tools/build.sh Debug` hatasız biter.
- [ ] **K3** `tools/check-env.sh` çalışır; §5.1'deki **tüm** kontrol kimlikleri (T-01..03, R-01..04, W-01..08, D-01..05, O-01..02) çıktıda görünür; sonda özet satırı vardır.
- [ ] **K4** Ortam sağlamken `check-env.sh` çıkış kodu `0`; bozuk `FDP_RUNTIME_DIR` ile çıkış kodu `1` (rapora `echo $?` çıktısı konur).
- [ ] **K5** `check-env.sh` çıktısında hiçbir parola, kullanıcı adı veya bağlantı dizesi yok; `.ini` dosyalarının içeriği basılmıyor.
- [ ] **K6** `debug-release-diff.sh` Bölüm 1'de en az şu 7 konumu bulur (dosya:satır olarak): `shared/stdafx.h:19`, `GameServer/stdafx.h:7`, `GameServer/User.cpp:4527`, `GameServer/MagicInstance.cpp:267`, `GameServer/GameServerDlg.cpp:737`, `shared/KOSocket.cpp:93`, `shared/database/OdbcCommand.cpp:73`. (Satır numaraları bu commit'e aittir; betik bunları **grep ile bulmalı**, listeden okumamalı.)
- [ ] **K7** `debug-release-diff.sh` Bölüm 2'de dört proje için Debug `_DEBUG`, Release `NDEBUG` tanımlarını gösterir.
- [ ] **K8** Her iki betik de LF satır sonlu, `chmod +x`, `bash -n` ile sözdizimi hatasız.
- [ ] **K9** `git diff --stat main...bot/F0-01` yalnızca §4'teki üç dosyayı gösterir.
- [ ] **K10** Depo kaynak kodunda (`GameServer/`, `AIServer/`, `LogInServer/`, `shared/`, `N3BASE/`) **hiçbir** değişiklik yok.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release
./tools/build.sh Debug
bash -n tools/check-env.sh && bash -n tools/debug-release-diff.sh
./tools/check-env.sh; echo "exit=$?"
./tools/check-env.sh --skip-db; echo "exit=$?"
FDP_RUNTIME_DIR=/tmp/yok-boyle-bir-yer ./tools/check-env.sh --skip-db; echo "exit=$?"
./tools/debug-release-diff.sh
git diff --stat main...bot/F0-01
file tools/*.sh            # "CRLF" görünmemeli
```

## 8. Kısıtlar ve uyarılar

- **Kod değiştirme.** Bu plan yalnızca `tools/` altına betik ekler (K10).
- **Kodlama tuzağı:** ISO-8859 dosyalarında `grep -a` zorunlu; aksi halde `shared/packets.h` gibi dosyalar sessizce atlanır ve rapor eksik çıkar.
- **CRLF:** Kaynak dosyalar CRLF'dir; `grep` çıktısındaki `\r` temizlenmeli. Betiklerin **kendisi** LF olmalı.
- **Veritabanı:** yalnızca `SELECT`, yalnızca §5.1 D-01..D-05'teki sorgular. Yasak tablo listesi `AGENTS.md` §2.7.
- **Gizli bilgi:** ini dosyalarının içeriği, DSN kullanıcı/parolası asla ekrana veya commit'e girmez.
- `tools/*` düzenleme izni `ask`; proje sahibi onay isteyecek, bu normaldir.
- Bir kontrol ortamda **gerçekten** başarısız olursa (ör. DSN yok), bu senin hatan değildir: betik doğru çalışıyor demektir. Ortamı düzeltmeye çalışma, raporda belirt.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F0-01` (taban: `main` @ `0f52027`)
  - `81e754e` — `[F0-01] Ortam kontrol betigi ekle`
  - `90e097e` — `[F0-01] Debug/Release fark raporu betigi ekle`
  - `3f13b3c` — `[F0-01] Uygulayici raporu ve Durum UYGULANDI`
- Değişen dosyalar ve nedenleri:
  - `tools/check-env.sh` (yeni): T-01..T-03, R-01..R-04, W-01..W-08, D-01..D-05, O-01..O-02 kontrolleri. `--skip-db`, `-h/--help`; `FDP_RUNTIME_DIR`, `MSBUILD`, `SQLCMD`, `FDP_SQL_INSTANCE`, `FDP_SQL_DB` ortamdan override edilebilir. Veritabanında yalnızca D-01..D-05'teki `SELECT` sorguları çalışır. `.ini` içeriği/değeri yazdırılmaz; yalnızca bölüm anahtarlarının varlığı kontrol edilir.
  - `tools/debug-release-diff.sh` (yeni): Bölüm 1 `grep -a -rnE` ile koşullu `#if/#ifdef/#ifndef/#elif` satırlarını bulur; Bölüm 2 dört `.vcxproj` dosyasını ayrıştırıp tanım tablosu + Debug/Release belirteç özeti üretir; Bölüm 3 aynı koşulların özet iskeletini `(elle doldurulacak)` sütunuyla yazar. Hiçbir dosya/satır bilgisi gömülü değildir.
  - `plans/F0-01-ortam-dogrulama-araclari.md` (değişti): yalnızca `Durum` satırı ve bu rapor.
- Derleme çıktısı — `./tools/build.sh Release` (çıkış kodu `0`, son 5 satır):
  ```text
    Lua.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\libs\Lua.lib
    shared.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\libs\shared.lib
    proj-LogInServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\LogInServer.exe
    proj-GameServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\GameServer.exe
    proj-AIServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\AIServer.exe
  ```
- Derleme çıktısı — `./tools/build.sh Debug` (çıkış kodu `0`, son 5 satır):
  ```text
    Lua.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Debug\libs\Lua.lib
    shared.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Debug\libs\shared.lib
    proj-LogInServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Debug\Server\LogInServer.exe
    proj-GameServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Debug\Server\GameServer.exe
    proj-AIServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Debug\Server\AIServer.exe
  ```
- `./tools/check-env.sh; echo "exit=$?"` (tam çıktı):
  ```text
  [PASS] T-01  MSBuild bulundu                      /mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe
  [PASS] T-02  tools/build.sh çalıştırılabilir /mnt/c/Users/frkoz/OneDrive/Desktop/Fire-Drake-Project-v1453/tools/build.sh
  [PASS] T-03  git komutu var                       /usr/bin/git
  [PASS] R-01  depo kökü                          /mnt/c/Users/frkoz/OneDrive/Desktop/Fire-Drake-Project-v1453
  [PASS] R-02  HEAD ve dal                          bot/F0-01 @ 0f52027
  [PASS] R-03  version.h __VERSION 1453             bulunan: 1453
  [WARN] R-04  çalışma ağacı temiz             değişiklik: 9
  [PASS] W-01  server/ klasoru                      /mnt/c/dev/fdp/server
  [PASS] W-02  server/*.exe (3)                     3/3
  [PASS] W-03  server/*.ini (3)                     3/3
  [PASS] W-04  GameServer.ini bölümleri           [ODBC] [ZONE_INFO] [AI_SERVER]
  [PASS] W-05  Map/*.smd sayısı (>=24)            24
  [PASS] W-06  freezone_a_20050718.smd              var
  [PASS] W-07  Quests/*.lua sayısı (>=114)        114
  [PASS] W-08  Client dosyalari                     2/2
  [PASS] D-01  sqlcmd bağlantısı                 SELECT 1
  [PASS] D-02  zorunlu tablolar                     10/10
  [PASS] D-03  ZONE_INFO ZoneNo=71                  freezone_a_20050718.smd
  [PASS] D-04  MAGIC Etc=1                          0
  [PASS] D-05  istemci sürümü                    DB=1473, Client/Server.ini=1473
  [PASS] O-01  32-bit DSN KO_MAIN                   tanımlı
  [PASS] O-02  32-bit DSN KO_GAME                   tanımlı
  Özet: 21 PASS, 0 FAIL, 0 SKIP, 1 WARN
  exit=0
  ```
  (R-04 WARN: `docs/`, `plans/`, `AGENTS.md`, `tools/` vb. altyapı dosyaları hâlâ untracked; STATUS.md'deki blokaj maddesi. WARN çıkış kodunu etkilemez.)
- `./tools/check-env.sh --skip-db; echo "exit=$?"` (yalnızca D satırları; tam çıktı):
  ```text
  [PASS] T-01  MSBuild bulundu                      /mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe
  [PASS] T-02  tools/build.sh çalıştırılabilir /mnt/c/Users/frkoz/OneDrive/Desktop/Fire-Drake-Project-v1453/tools/build.sh
  [PASS] T-03  git komutu var                       /usr/bin/git
  [PASS] R-01  depo kökü                          /mnt/c/Users/frkoz/OneDrive/Desktop/Fire-Drake-Project-v1453
  [PASS] R-02  HEAD ve dal                          bot/F0-01 @ 0f52027
  [PASS] R-03  version.h __VERSION 1453             bulunan: 1453
  [WARN] R-04  çalışma ağacı temiz             değişiklik: 9
  [PASS] W-01  server/ klasoru                      /mnt/c/dev/fdp/server
  [PASS] W-02  server/*.exe (3)                     3/3
  [PASS] W-03  server/*.ini (3)                     3/3
  [PASS] W-04  GameServer.ini bölümleri           [ODBC] [ZONE_INFO] [AI_SERVER]
  [PASS] W-05  Map/*.smd sayısı (>=24)            24
  [PASS] W-06  freezone_a_20050718.smd              var
  [PASS] W-07  Quests/*.lua sayısı (>=114)        114
  [PASS] W-08  Client dosyalari                     2/2
  [SKIP] D-01  sqlcmd bağlantısı                 (--skip-db)
  [SKIP] D-02  zorunlu tablolar                     (--skip-db)
  [SKIP] D-03  ZONE_INFO ZoneNo=71                  (--skip-db)
  [SKIP] D-04  MAGIC Etc=1                          (--skip-db)
  [SKIP] D-05  istemci sürümü                       (--skip-db)
  [PASS] O-01  32-bit DSN KO_MAIN                   tanımlı
  [PASS] O-02  32-bit DSN KO_GAME                   tanımlı
  Özet: 16 PASS, 0 FAIL, 5 SKIP, 1 WARN
  exit=0
  ```
- `FDP_RUNTIME_DIR=/tmp/yok-boyle-bir-yer ./tools/check-env.sh --skip-db; echo "exit=$?"` (W satırları; tam çıktı):
  ```text
  [PASS] T-01  MSBuild bulundu                      /mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe
  [PASS] T-02  tools/build.sh çalıştırılabilir /mnt/c/Users/frkoz/OneDrive/Desktop/Fire-Drake-Project-v1453/tools/build.sh
  [PASS] T-03  git komutu var                       /usr/bin/git
  [PASS] R-01  depo kökü                          /mnt/c/Users/frkoz/OneDrive/Desktop/Fire-Drake-Project-v1453
  [PASS] R-02  HEAD ve dal                          bot/F0-01 @ 0f52027
  [PASS] R-03  version.h __VERSION 1453             bulunan: 1453
  [WARN] R-04  çalışma ağacı temiz             değişiklik: 9
  [FAIL] W-01  server/ klasoru                      yok: /tmp/yok-boyle-bir-yer/server
  [FAIL] W-02  server/*.exe (3)                     eksik: AIServer.exe GameServer.exe LogInServer.exe
  [FAIL] W-03  server/*.ini (3)                     eksik: AIServer.ini GameServer.ini LogInServer.ini
  [FAIL] W-04  GameServer.ini bölümleri           dosya yok
  [FAIL] W-05  Map/*.smd sayısı (>=24)            bulunan: 0
  [FAIL] W-06  freezone_a_20050718.smd              yok
  [FAIL] W-07  Quests/*.lua sayısı (>=114)        bulunan: 0
  [FAIL] W-08  Client dosyalari                     eksik: Client/KnightOnline.exe Client/Server.ini
  [SKIP] D-01  sqlcmd bağlantısı                 (--skip-db)
  [SKIP] D-02  zorunlu tablolar                     (--skip-db)
  [SKIP] D-03  ZONE_INFO ZoneNo=71                  (--skip-db)
  [SKIP] D-04  MAGIC Etc=1                          (--skip-db)
  [SKIP] D-05  istemci sürümü                       (--skip-db)
  [PASS] O-01  32-bit DSN KO_MAIN                   tanımlı
  [PASS] O-02  32-bit DSN KO_GAME                   tanımlı
  Özet: 8 PASS, 8 FAIL, 5 SKIP, 1 WARN
  exit=1
  ```
- `./tools/debug-release-diff.sh` (tam çıktı):
  ```text
  ## Bölüm 1 — Koşullu bloklar

  Kaynak: GameServer AIServer LogInServer shared N3BASE altındaki `*.cpp`/`*.h`; `grep -a -rnE`. Belirteçler: `DEBUG`, `_DEBUG`, `NDEBUG`, `DISABLE_PLAYER_BLINKING`, `USE_SQL_TRACE`.

  - `GameServer/GameServerDlg.cpp:737` — `#ifndef DEBUG // ignore timeouts in debug builds, as we'll probably be pausing it with the debugger.`
  - `GameServer/LuaEngine.h:9` — `#ifdef _DEBUG`
  - `GameServer/MagicInstance.cpp:267` — `#if !defined(DEBUG)`
  - `GameServer/User.cpp:4527` — `#if !defined(DISABLE_PLAYER_BLINKING)`
  - `GameServer/stdafx.h:7` — `#if defined(DEBUG)`
  - `shared/KOSocket.cpp:93` — `#ifndef _DEBUG`
  - `shared/Thread.cpp:21` — `#ifdef _DEBUG`
  - `shared/Thread.cpp:41` — `#ifdef _DEBUG`
  - `shared/database/OdbcCommand.cpp:73` — `#ifdef USE_SQL_TRACE`
  - `shared/database/OdbcCommand.cpp:103` — `#ifdef USE_SQL_TRACE`
  - `shared/stdafx.h:19` — `#if defined(_DEBUG) || defined(DEBUG)`
  - `shared/stdafx.h:32` — `#	ifndef DEBUG`
  - `shared/stdafx.h:36` — `#	ifndef _DEBUG`

  ## Bölüm 2 — vcxproj önişlemci tanımları

  `<PreprocessorDefinitions>` satırları (proje x yapılandırma):

  | Proje | Yapılandırma | Araç | Tanımlar |
  |---|---|---|---|
  | proj-GameServer.vcxproj | Debug | Midl | _DEBUG;%(PreprocessorDefinitions) |
  | proj-GameServer.vcxproj | Debug | ClCompile | WIN32;GAMESERVER;_WINSOCK_DEPRECATED_NO_WARNINGS;_DEBUG;_WINDOWS;_3DSERVER;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions) |
  | proj-GameServer.vcxproj | Debug | ResourceCompile | _DEBUG;%(PreprocessorDefinitions) |
  | proj-GameServer.vcxproj | Release | Midl | NDEBUG;%(PreprocessorDefinitions) |
  | proj-GameServer.vcxproj | Release | ClCompile | WIN32;GAMESERVER;_WINSOCK_DEPRECATED_NO_WARNINGS;NDEBUG;_WINDOWS;_3DSERVER;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions) |
  | proj-GameServer.vcxproj | Release | ResourceCompile | NDEBUG;%(PreprocessorDefinitions) |
  | proj-AIServer.vcxproj | Release | Midl | NDEBUG;%(PreprocessorDefinitions) |
  | proj-AIServer.vcxproj | Release | ClCompile | WIN32;AI_SERVER;_WINSOCK_DEPRECATED_NO_WARNINGS;NDEBUG;_WINDOWS;_3DSERVER;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions) |
  | proj-AIServer.vcxproj | Release | ResourceCompile | NDEBUG;%(PreprocessorDefinitions) |
  | proj-AIServer.vcxproj | Debug | Midl | _DEBUG;%(PreprocessorDefinitions) |
  | proj-AIServer.vcxproj | Debug | ClCompile | WIN32;AI_SERVER;_WINSOCK_DEPRECATED_NO_WARNINGS;_DEBUG;_WINDOWS;_3DSERVER;_REPENT;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions) |
  | proj-AIServer.vcxproj | Debug | ResourceCompile | _DEBUG;%(PreprocessorDefinitions) |
  | proj-LogInServer.vcxproj | Release | Midl | NDEBUG;%(PreprocessorDefinitions) |
  | proj-LogInServer.vcxproj | Release | ClCompile | WIN32;LOGIN_SERVER;_WINSOCK_DEPRECATED_NO_WARNINGS;NDEBUG;_WINDOWS;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions) |
  | proj-LogInServer.vcxproj | Release | ResourceCompile | NDEBUG;%(PreprocessorDefinitions) |
  | proj-LogInServer.vcxproj | Debug | Midl | _DEBUG;%(PreprocessorDefinitions) |
  | proj-LogInServer.vcxproj | Debug | ClCompile | WIN32;LOGIN_SERVER;_WINSOCK_DEPRECATED_NO_WARNINGS;_DEBUG;_WINDOWS;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions) |
  | proj-LogInServer.vcxproj | Debug | ResourceCompile | _DEBUG;%(PreprocessorDefinitions) |
  | shared.vcxproj | Debug | ClCompile | WIN32;_WIN32;_MBCS;_CRT_SECURE_NO_WARNINGS;_WINSOCK_DEPRECATED_NO_WARNINGS;%(PreprocessorDefinitions) |
  | shared.vcxproj | Release | ClCompile | WIN32;_WIN32;_MBCS;_CRT_SECURE_NO_WARNINGS;_WINSOCK_DEPRECATED_NO_WARNINGS;%(PreprocessorDefinitions) |

  Debug/Release belirteç özeti (`RuntimeLibrary` örtük tanımları dahil):

  | Proje | Debug belirteci | Release belirteci |
  |---|---|---|
  | proj-GameServer.vcxproj | _DEBUG (explicit) | NDEBUG (explicit) |
  | proj-AIServer.vcxproj | _DEBUG (explicit) | NDEBUG (explicit) |
  | proj-LogInServer.vcxproj | _DEBUG (explicit) | NDEBUG (explicit) |
  | shared.vcxproj | _DEBUG (implicit via RuntimeLibrary=MultiThreadedDebug) | NDEBUG (implicit via RuntimeLibrary=MultiThreaded) |

  ## Bölüm 3 — Özet tablo

  "Debug / Release" sütunu kaynak okunarak elle doldurulur; bu betik yalnızca konumları çıkarır.

  | Dosya:satır | Koşul | Debug / Release |
  |---|---|---|
  | `GameServer/GameServerDlg.cpp:737` | `#ifndef DEBUG // ignore timeouts in debug builds, as we'll probably be pausing it with the debugger.` | (elle doldurulacak) |
  | `GameServer/LuaEngine.h:9` | `#ifdef _DEBUG` | (elle doldurulacak) |
  | `GameServer/MagicInstance.cpp:267` | `#if !defined(DEBUG)` | (elle doldurulacak) |
  | `GameServer/User.cpp:4527` | `#if !defined(DISABLE_PLAYER_BLINKING)` | (elle doldurulacak) |
  | `GameServer/stdafx.h:7` | `#if defined(DEBUG)` | (elle doldurulacak) |
  | `shared/KOSocket.cpp:93` | `#ifndef _DEBUG` | (elle doldurulacak) |
  | `shared/Thread.cpp:21` | `#ifdef _DEBUG` | (elle doldurulacak) |
  | `shared/Thread.cpp:41` | `#ifdef _DEBUG` | (elle doldurulacak) |
  | `shared/database/OdbcCommand.cpp:73` | `#ifdef USE_SQL_TRACE` | (elle doldurulacak) |
  | `shared/database/OdbcCommand.cpp:103` | `#ifdef USE_SQL_TRACE` | (elle doldurulacak) |
  | `shared/stdafx.h:19` | `#if defined(_DEBUG) \|\| defined(DEBUG)` | (elle doldurulacak) |
  | `shared/stdafx.h:32` | `#	ifndef DEBUG` | (elle doldurulacak) |
  | `shared/stdafx.h:36` | `#	ifndef _DEBUG` | (elle doldurulacak) |
  ```

  **Not (betik değil, uygulayıcı gözlemi):** `GameServer/LuaEngine.h`, `GameServer/User.cpp`, `GameServer/MagicInstance.cpp`, `GameServer/GameServerDlg.cpp` ve `GameServer/stdafx.h` dosyaları `_DEBUG` yerine `DEBUG` kullanır; `GameServer/stdafx.h:3` `shared/stdafx.h`'ı include ettiği ve `shared/stdafx.h:32` Debug bloğunda `DEBUG`'ü tanımladığı için bu dosyalarda da belirteç Debug'da etkindir.

### Bölüm 3 — elle doldurulmuş Debug/Release davranış tablosu

Kaynak okunarak dolduruldu; her satır için ilgili kod bloğu bu commit'te `main` ile aynıdır (bu plan kod değiştirmez).

| Dosya:satır | Koşul | Debug'da ne olur | Release'de ne olur |
|---|---|---|---|
| `GameServer/GameServerDlg.cpp:737` | `#ifndef DEBUG` | Zaman aşımı bloğu derlenmez; oturumlar `KOSOCKET_TIMEOUT` aşımında atılmaz (debugger ile duraklatma için). | `KOSOCKET_TIMEOUT` / `KOSOCKET_LOADING_TIMEOUT` aşımında `Disconnect()` çağrılır. |
| `GameServer/LuaEngine.h:9` | `#ifdef _DEBUG` | `LUA_SCRIPT_CACHE_DISABLED` tanımlanır; Lua script'leri önbelleğe alınmaz, her çağrıda yeniden yüklenir. | Önbellek etkin; script bytecode'u bir kez derlenip saklanır. |
| `GameServer/MagicInstance.cpp:267` | `#if !defined(DEBUG)` | Quest kapısı derlenmez; `sEtc != 0` olan yetenekler quest tamamlanmadan kullanılabilir. | `sEtc != 0` ve kullanıcı GM değilse quest (`CheckExistEvent(sEtc, 2)`) tamamlanmış olmalı; değilse `SkillUseFail`. |
| `GameServer/User.cpp:4527` | `#if !defined(DISABLE_PLAYER_BLINKING)` | `DISABLE_PLAYER_BLINKING` (`GameServer/stdafx.h:7`, `DEBUG` iken) tanımlı olduğundan `BlinkStart()` gövdesi derlenmez; blink çalışmaz. | Gövde derlenir; yine de `canAttackOtherNation()` bölgesinde (arena dahil) blink yapılmaz. |
| `GameServer/stdafx.h:7` | `#if defined(DEBUG)` | `DISABLE_PLAYER_BLINKING` tanımlanır. | Tanımlanmaz. |
| `shared/KOSocket.cpp:93` | `#ifndef _DEBUG` | `HandlePacket` false dönerse yalnızca `TRACE` yazılır; soket bağlı kalır (hata işleyiciye gidilmez). | `goto error_handler` → `Disconnect()`. |
| `shared/Thread.cpp:21` | `#ifdef _DEBUG` | `std::thread` kurulamazsa istisna `printf` ile yazılır ve `ASSERT(0)` düşer. | Yalnızca `false` döner; log/assert yok. |
| `shared/Thread.cpp:41` | `#ifdef _DEBUG` | `join()` istisnasında `printf` + `ASSERT(0)`. | Yalnızca `false` döner. |
| `shared/database/OdbcCommand.cpp:73` | `#ifdef USE_SQL_TRACE` | `Execute()`'a verilen SQL `TRACE` ile yazılır (`USE_SQL_TRACE` yalnızca Debug bloğunda tanımlı). | SQL izi yok. |
| `shared/database/OdbcCommand.cpp:103` | `#ifdef USE_SQL_TRACE` | `Prepare()`'a verilen SQL `TRACE` ile yazılır. | SQL izi yok. |
| `shared/stdafx.h:19` | `#if defined(_DEBUG) \|\| defined(DEBUG)` | `ASSERT=assert`, `TRACE=FormattedDebugString`, `USE_SQL_TRACE` ve `DebugUtils.h` etkin. | `ASSERT` ve `TRACE` boş makro; debug araçları derlenmez. |
| `shared/stdafx.h:32` | `#	ifndef DEBUG` | Debug bloğu içinde: `DEBUG` tanımlı değilse tanımlanır (iki belirteç tutarlı kalsın). | Blok derlenmez. |
| `shared/stdafx.h:36` | `#	ifndef _DEBUG` | Debug bloğu içinde: `_DEBUG` tanımlı değilse tanımlanır. | Blok derlenmez. |

### Kabul kriterleri öz-değerlendirmesi

- K1 ✔ `./tools/build.sh Release` çıkış kodu 0.
- K2 ✔ `./tools/build.sh Debug` çıkış kodu 0.
- K3 ✔ Tam çıktıda T-01..03, R-01..04, W-01..08, D-01..05, O-01..02 kimliklerinin tamamı ve `Özet:` satırı görünür (22 kontrol).
- K4 ✔ Sağlam ortamda çıkış kodu 0; `FDP_RUNTIME_DIR=/tmp/yok-boyle-bir-yer` ile çıkış kodu 1 (`echo $?` çıktıları yukarıda).
- K5 ✔ Çıktıda parola/kullanıcı adı/bağlantı dizesi yok. `W-04` yalnızca `[ODBC] [ZONE_INFO] [AI_SERVER]` bölüm adlarını yazar, değerleri yazmaz. D-05 iki sürüm numarasını yazar (plan gereği; gizli değil). O-01/02 yalnızca DSN adını yazar.
- K6 ✔ Bölüm 1 istenen 7 konumun tamamını buldu (`shared/stdafx.h:19`, `GameServer/stdafx.h:7`, `GameServer/User.cpp:4527`, `GameServer/MagicInstance.cpp:267`, `GameServer/GameServerDlg.cpp:737`, `shared/KOSocket.cpp:93`, `shared/database/OdbcCommand.cpp:73`) + 6 ek konum; hepsi `grep` ile üretildi, dosya/satır gömülü değil.
- K7 ✔ Bölüm 2'de dört proje için Debug `_DEBUG`, Release `NDEBUG` görünür. `shared.vcxproj`'da açık tanım yok; `RuntimeLibrary=MultiThreadedDebug` / `MultiThreaded` üzerinden örtük olarak (MSVC `/MTd` → `_DEBUG`, `/MT` → `NDEBUG`) tabloda gösterildi.
- K8 ✔ `file tools/*.sh` → "ASCII text executable" (CRLF yok); `chmod +x` yapıldı; `bash -n` ikisinde de hatasız. Dosyalar AGENTS.md §3 gereği ASCII tutuldu; Türkçe çıktılar `\u` kaçışlarıyla üretiliyor.
- K9 ✔ Commit öncesi `git diff --cached --stat main` ve commit sonrası `git diff --stat main...bot/F0-01` yalnızca üç dosyayı gösterir: `plans/F0-01-...md`, `tools/check-env.sh`, `tools/debug-release-diff.sh`. Altyapı dosyaları (`docs/`, `plans/README.md`, `tools/build.sh` vb.) untracked oldukları için farkta görünmez; R-04'te WARN olarak raporlanır. Kaynak ağaçta değişiklik yok (K10).
- K10 ✔ `GameServer/`, `AIServer/`, `LogInServer/`, `shared/`, `N3BASE/` altında hiçbir değişiklik yok (`git status --short` temiz; yalnızca untracked altyapı dosyaları var).

### Plandan sapmalar ve gerekçeleri

1. **Özet satırına WARN eklendi:** `Özet: X PASS, Y FAIL, Z SKIP, W WARN`. Plan örneği WARN içermiyordu; R-04 bilgi amaçlı WARN'ı raporda görünür kalsın diye dördüncü kategori eklendi. WARN çıkış kodunu etkilemez (plan gereği).
2. **D-01 başarısızsa D-02..D-05 SKIP yazılır** (`baglanti yok` notuyla), FAIL değil. Bu ortamda D-01 PASS olduğu için tetiklenmedi; --skip-db durumu plandaki gibi tüm D'leri SKIP yapar.
3. **Bölüm 2'ye `RuntimeLibrary` özeti eklendi:** dört projede K7'nin Debug `_DEBUG`/Release `NDEBUG` koşulunu `shared.vcxproj` için de gösterebilmek amacıyla, vcxproj'dan okunan `RuntimeLibrary` değerinden örtük belirteç türetildi. Sonuçlar yine kaynaktan üretiliyor, gömülü değil.
4. **Dosya içeriği ASCII, çıktı Türkçe:** AGENTS.md §3 "yeni dosyalar yalnızca ASCII" ile planın Türkçe çıktı beklentisi `\u` kaçışlarıyla uzlaştırıldı; çalışma zamanı çıktısı plandaki gibi (`Özet:`, `çalıştırılabilir`, `bölümleri`...).
5. **D-02 ilk koşuda tüm tabloları eksik gösterdi:** sqlcmd satır sonları CRLF olduğu için `grep -x` eşleşmedi; betiğe `tr -d '\r'` eklendi ve ikinci koşuda `10/10 PASS` alındı. Bu bir betik hatasının düzeltilmesidir, kapsam dışına çıkılmadı.

### Açık sorular

- **R-04 WARN:** `docs/`, `plans/`, `AGENTS.md`, `CLAUDE.md`, `opencode.json`, `.claude/`, `tools/`, `start.md`, `.gitattributes` hâlâ untracked (STATUS.md blokajı). Proje sahibi bunları `main`'e commit'ledikten sonra R-04 PASS dönecek; betikte değişiklik gerekmez.
- Bu branch `main`'den açıldığında altyapı dosyaları `main`'de olmadığı için, plan dosyası ve iki betik branch commit'lerinde "yeni dosya" olarak görünür. Doğrulama `main...bot/F0-01` üzerinden yapıldığında K9 yine üç dosya gösterir; ancak altyapı commit'i `main`'e girince bu branch'in rebase/merge ihtiyacı doğabilir (kararı proje sahibi verir; AGENTS.md gereği merge/rebase yapılmadı).

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-01

- **Karar: DOĞRULANDI**
- İncelenen: `main...bot/F0-01` @ `c710379` (5 commit; kaynak ağaçta değişiklik yok)
- Yöntem: Uygulayıcının çıktılarına güvenilmedi; derlemeler, betiklerin tüm modları ve Debug/Release tablosu **bağımsız** yeniden çalıştırıldı/kontrol edildi.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release derleme | ✔ | `./tools/build.sh Release` exit 0, 0 uyarı/hata satırı. *Kaynak değişmediği için bu kriter yalnızca araç zincirinin sağlığını gösterir.* |
| K2 Debug derleme | ✔ | `./tools/build.sh Debug` exit 0, 0 uyarı/hata satırı (aynı not) |
| K3 22 kontrol + özet | ✔ | T-01..03, R-01..04, W-01..08, D-01..05, O-01..02 hepsi çıktıda; `Özet: 21 PASS, 0 FAIL, 0 SKIP, 1 WARN` |
| K4 çıkış kodları | ✔ | Sağlam ortam exit 0; `FDP_RUNTIME_DIR=/tmp/yok-boyle-bir-yer` → 8 FAIL, exit 1; bilinmeyen argüman → exit 2 (ek) |
| K5 gizli bilgi yok | ✔ | Çıktı ve betiklerde parola/UID/token taraması boş (`pwd` yalnızca kabuk komutu). `.ini`'den yalnızca bölüm adları okunuyor (`tools/check-env.sh` W-04) |
| K6 7 konum, grep ile | ✔ | 7'si de bulundu (+6 ek). **Bağımsız Python regex ile 13/13 konum birebir aynı.** Betikte gömülü `dosya:satır` yok |
| K7 vcxproj tanımları | ✔ | 24 satırlık tablo, dört proje; `shared.vcxproj` için `RuntimeLibrary` (`shared/shared.vcxproj:58,73`) üzerinden örtük, açıkça belirtilmiş |
| K8 biçim | ✔ | `file`: ASCII text executable; CR yok; git modu `100755`; `bash -n` temiz |
| K9 yalnızca 3 dosya | ✔ | `git diff --name-status main...bot/F0-01`: `A plans/F0-01…`, `A tools/check-env.sh`, `A tools/debug-release-diff.sh` |
| K10 kaynak kodu değişmedi | ✔ | Fark listesinde `GameServer/ AIServer/ LogInServer/ shared/ N3BASE/` yok |

**Ek kontroller:** DB sorguları yalnızca `SELECT` (sys.tables, ZONE_INFO, MAGIC, VERSION); yasak tablo adı geçmiyor. Uygulayıcının elle doldurduğu Debug/Release tablosunun 13 satırı tek tek kaynaktan okunarak doğrulandı (`GameServer/LuaEngine.h:9`, `shared/KOSocket.cpp:93`, `shared/Thread.cpp:21,41`, `shared/database/OdbcCommand.cpp:68-76,98-106`, `GameServer/MagicInstance.cpp:267`, `shared/stdafx.h:19-45`); tek yanlış, aşağıdaki Bulgu 2.

**Bulgular (hiçbiri engelleyici değil):**

1. *Düşük* — `tools/debug-release-diff.sh:141`: Bölüm 3 tablosunda koşul metni olduğu gibi yazılıyor; `shared/stdafx.h:19` satırındaki `||` Markdown tablosunu bozuyor (rapora yapıştırırken uygulayıcı elle `\|` yapmış). Betikte `|` karakteri kaçışlanmalı.
2. *Düşük* — Uygulayıcı notu "`GameServer/LuaEngine.h` … `_DEBUG` yerine `DEBUG` kullanır" diyor; `GameServer/LuaEngine.h:9` aslında `#ifdef _DEBUG`. Tablonun kendisi doğru, yalnızca not yanlış.
3. *Bilgi* — Rapor 3 commit sayıyor; `b5ae18d` (K9 kanıtı düzeltmesi) ve `c710379` (yalnızca dosya modu 644→755) listede yok.
4. *Bilgi* — `check-env.sh` sütun hizası UTF-8 baytlarıyla hesaplandığı için Türkçe etiketlerde kayık (kozmetik).
5. *Bilgi, F0 için önemli* — Tablodan çıkan somut sonuç: **Debug derlemesi** quest kapısını (`MagicInstance.cpp:267`), blink'i (`User.cpp:4527` ⇐ `GameServer/stdafx.h:7`), oturum zaman aşımını (`GameServerDlg.cpp:737`) ve Lua önbelleğini kapatıyor; paket-handler hatasında bağlantıyı koparmıyor (`KOSocket.cpp:93`). Bot testlerinin **Release** derlemesiyle yapılması gerekir (aksi halde KI-001 ve blink davranışı yanıltır).

**F0 hakkında:** Bu plan T-ENV-02'yi karşıladı (Debug/Release farkı belgelendi). T-ENV-01'in "üç sunucu ayakta + insan istemcisi Ronark'a giriyor" kısmı bu planın kapsamında değildi ve **hâlâ açık**; F0 çıkış koşulu bu yüzden henüz sağlanmadı.

Birleştirme (kullanıcı onayıyla, bu rapor commit edildikten sonra): `git switch main && git merge --no-ff bot/F0-01`. Not: altyapı dosyaları (`docs/`, `AGENTS.md`, `plans/README.md` vb.) hâlâ hiçbir branch'te commit'li değil; `main`'e önce onlar girmeli ya da bu branch'in farkı yalnızca üç dosya olarak kalır.

