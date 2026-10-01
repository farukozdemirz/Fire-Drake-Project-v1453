# F0-02: Sunucu çalıştırma betiği (start / stop / status)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F0 — Ortam ve temel doğrulama (`docs/17` §2) |
| Branch | `bot/F0-02` (taban: `main` @ `43d3500`; F0-01 bu commit'te birleşti) |
| Bağımlı olduğu planlar | F0-01 (KAPANDI) |
| İlgili gereksinim / kabul | T-ENV-01 "üç sunucu ayakta" kısmı (`docs/15` §4.1); F0 kabulü (`docs/17` §2 F0) |
| Tahmini büyüklük | S (1 yeni betik, kod değişikliği yok) |
| Hazırlayan / tarih | Claude / 2026-10-01 (otonom döngü) |

---

## 1. Amaç

Üç sunucunun (AIServer → GameServer → LogInServer) **tek komutla, doğru sırayla** başlatılması, gerçekten hazır olduklarının **ölçülerek** anlaşılması, düzgün durdurulması ve anlık durumlarının görülmesi.

Bu plan sonunda `tools/run-servers.sh` olacak:

```
./tools/run-servers.sh start  [--config Release|Debug] [--keep-on-fail]
./tools/run-servers.sh stop   [--force]
./tools/run-servers.sh status
```

Sonraki fazlar (F1 DB düzeltmesi sonrası yeniden başlatma, F3 ScenarioRunner, F8 değerlendirme) sunucuyu bu betikle yönetecek. Bu plan **sunucu kodunu değiştirmez.**

## 2. Bağlam (okunması zorunlu)

- `AGENTS.md`: tüm kurallar; özellikle §2.7 (veritabanı) ve §3 (kodlama).
- `tools/check-env.sh`: **üslup örneği.** Yeni betik aynı kalıbı kullanır: `set -euo pipefail`, ortam değişkeniyle override, Türkçe çıktının `$'\u..'` kaçışlarıyla yazılması (dosya ASCII kalır), `report` benzeri tek satırlık çıktı.
- `start.md` §6–§7 (depo kökü, izlenmeyen dosya): ini anahtarları, portlar, başlatma sırası.
- `docs/02` §3: süreç mimarisi (portlar 10020 / 15001 / 15100).

### 2.1 Koddan doğrulanmış gerçekler (commit `43d3500`, Claude kontrol etti)

| Gerçek | Kanıt |
|---|---|
| Üç sunucu da başarılı açılışta `s_hEvent.Wait()` ile bekler; **açılış başarısızsa `system("pause")` çağırır** ve bir tuşa basılana kadar açık kalır. `system()` bir **`cmd.exe` alt süreci** başlatır. Betik "başarısız açılış"ı bu alt süreçten tanır. | `AIServer/main.cpp:26,31,35`; `GameServer/main.cpp:31,40,44`; `LogInServer/main.cpp:22,27,31` |
| Üçü de `SetConsoleCtrlHandler` ile kapanış sinyalini yakalar; handler olayı tetikleyip ana thread'in temizlik yapmasını bekler. Yani pencereyi kapatmak (WM_CLOSE → CTRL_CLOSE_EVENT) **düzgün kapanış** demektir. | `AIServer/main.cpp:52-57`; `GameServer/main.cpp:62-67`; `LogInServer/main.cpp:43-48` |
| Tüm dosya yolları **çalışma dizinine göre**: ini (`./AIServer.ini`, `./GameServer.ini`, `./LogInServer.ini`), `./map/`, `./Quests/`, `Logs/`. Bu yüzden exe herhangi bir klasörden çalıştırılabilir; **çalışma dizini `$FDP_RUNTIME_DIR/server` olmalı.** | `AIServer/ServerDlg.cpp:836`; `GameServer/Define.h:3`; `LogInServer/Define.h:5`; `shared/globals.h:7`; `GameServer/LuaEngine.h:5`; `GameServer/GameServerDlg.cpp:162`; `LogInServer/LoginServer.cpp:19` |
| Port anahtarları: AIServer `[SETTINGS] PORT` (vars. 10020); GameServer `[SETTINGS] PORT` (vars. 15001) ve `[AI_SERVER] PORT` (vars. 10020); LogInServer `[SETTINGS] PORT` (vars. 15100). `GameServer.ini`'de `PORT` anahtarı **iki bölümde** var; okuma bölüme duyarlı olmalı. | `AIServer/ServerDlg.cpp:841`; `GameServer/GameServerDlg.cpp:293,295`; `LogInServer/LoginServer.cpp:130` |
| **AIServer** DB'ye bağlanır, **sonra** portu dinler, **sonra** tabloları/haritaları/NPC'leri yükler. Port dinlemesi "DB bağlantısı tamam" demektir. GameServer paketlerini işleyen worker thread'ler ancak tüm NPC'ler hazır olunca başlar ("Monster All Init Success"). Yani GameServer erken bağlansa da veri yarışı olmaz. | `AIServer/ServerDlg.cpp:70,82,88-106`; `AIServer/Npc.cpp:787-791`; `AIServer/ServerDlg.cpp:684-687` |
| **GameServer** portu **en başta** dinler (tablolar yüklenmeden). Bu yüzden port dinlemesi GameServer için hazır olma işareti **değildir.** Tüm tablolar, haritalar, log dosyaları ve Lua yüklendikten **sonra** AIServer'a bağlanır. **GameServer hazır = AIServer portuna `Established` bağlantısı var.** | `GameServer/GameServerDlg.cpp:97` (Listen), `:107-154` (tablolar), `:196` (Lua), `:199` (`AIServerConnect`), `:205` (`RunServer`) |
| **LogInServer** DB'ye bağlanır, sürüm listesini yükler, **en son** portu dinler. Port dinlemesi = hazır. | `LogInServer/LoginServer.cpp:35,42,51` |
| ODBC bağlantısı `SQLDriverConnect(..., 0)` (= `SQL_DRIVER_NOPROMPT`) ile yapılır; olmayan bir DSN iletişim kutusu açmaz, hata döner. Başarısızlık testi (§5.4) buna dayanır. | `shared/database/OdbcConnection.cpp:87` |

### 2.2 Ortamdan doğrulanmış gerçekler (2026-10-01, Claude ölçtü)

| Gerçek | Değer |
|---|---|
| Çalışma dizini | `/mnt/c/dev/fdp/server` (Windows: `C:\dev\fdp\server`) |
| Release çıktısı | `build/bin/x86-Release/Server/{AIServer,GameServer,LogInServer}.exe`; şu an `C:\dev\fdp\server\*.exe` ile **bayt bayt aynı** (md5) |
| Şu an çalışan sunucular | Proje sahibi elle başlatmış: `C:\dev\fdp\server\{AIServer,GameServer,LogInServer}.exe`; portlar 10020/15001/15100 LISTEN; GameServer'ın 10020'ye `Established` bağlantısı var; GameServer'a bağlı istemci yok |
| **Tuzak: ilgisiz süreç** | Yolu okunamayan (`ExecutablePath` boş) bir `GameServer.exe` daha var (ör. pid 4336, sabaha karşı başlamış, büyük olasılıkla başka bir program veya yükseltilmiş yetkiyle çalışan bir süreç). **Betik buna asla dokunmamalı.** Süreçler yalnızca ad ile değil **tam exe yolu** ile tanınmalı (§5.2). |
| Her sunucunun alt süreci | Normalde yalnızca `conhost.exe`. `system("pause")` durumunda ek olarak `cmd.exe` olur. |
| `taskkill.exe /PID <pid>` (**/F olmadan**) | `Start-Process -WindowStyle Minimized` ile açılmış bir konsol sürecini düzgün kapatıyor: `SUCCESS: Sent termination signal…`, süreç ~2 sn içinde çıktı (cmd.exe ile denendi). |
| PowerShell çağrısı | `powershell.exe -NoProfile -NonInteractive -EncodedCommand <base64 UTF-16LE>` tırnaklama sorununu ortadan kaldırıyor. **`$ProgressPreference='SilentlyContinue'` verilmezse** çıktıya `#< CLIXML …` gürültüsü karışıyor. Bir çağrı ~1,3 sn. |

## 3. Kapsam

**Yapılacaklar**

- `tools/run-servers.sh` yazmak (§5.1–§5.3).
- §5.4'teki test sırasını çalıştırıp çıktıları Uygulayıcı Raporu'na yapıştırmak.
- Başlangıçta sunucular çalışıyorsa, işin sonunda **aynı hâle geri getirmek** (§5.4 adım 11).

**Kapsam dışı (yapılmayacak)**

- `GameServer/`, `AIServer/`, `LogInServer/`, `shared/`, `N3BASE/` altında **hiçbir değişiklik**. Konsol çıktısını dosyaya yazdırmak için kod değiştirmek de yok (KI-004 ayrı iş).
- **İstemciyle oyuna girmek.** T-ENV-01'in "insan istemcisi Ronark'a giriyor" kısmı proje sahibinin **elle** yapacağı iştir; bu plan onu yapmaz ve yapmaya çalışmaz.
- `.ini` dosyalarını değiştirmek, oluşturmak veya içeriğini yazdırmak (`PORT` değerleri hariç). İstisna: §5.4'teki **geçici test klasörü** içindeki ini'ler.
- `Logs/` altındaki dosyaları okumak veya yazdırmak (`Login_*.log` hesap adları içerebilir).
- Veritabanına bağlanmak (`sqlcmd` yok). Sunucular DB'ye kendileri bağlanır; betik bağlanmaz.
- Çalışma dizinine exe kopyalamak ("deploy"). Betik exe'yi bulunduğu yerden çalıştırır (§5.1).
- `tools/build.sh`, `tools/check-env.sh`, `tools/auto-loop.sh`'yi değiştirmek; `restart` komutu, Windows servis kaydı, zamanlanmış görev, güvenlik duvarı kuralı eklemek.
- PID dosyası veya başka bir durum dosyası tutmak (süreçler her seferinde yeniden bulunur, §5.2).
- `rm -rf` kullanmak. Geçici test klasörü silinmez; `%TEMP%` altında kalır.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/run-servers.sh` | yeni | LF satır sonu (`.gitattributes` `*.sh eol=lf`), `chmod +x`, git modu `100755`, içerik ASCII |
| `plans/F0-02-sunucu-calistirma-betigi.md` | değiştir | yalnızca `Durum` satırı ve "Uygulayıcı Raporu" bölümü |

Depo dışı, yalnızca test için: `%TEMP%\fdp-f0-02-failtest\server\` (Windows `TEMP` klasörü; §5.4 adım 4). Başka hiçbir depo dışı dosya oluşturulmaz.

Bu listede olmayan bir dosyaya dokunmak gerekirse **dur** ve Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.0 Hazırlık

1. `git switch -c bot/F0-02 main`
2. Bu dosyadaki `Durum` satırını `UYGULANIYOR` yap.

### 5.1 Genel yapı

- `#!/usr/bin/env bash` + `set -euo pipefail`. Yalnızca bash, coreutils, awk/sed/grep, `iconv`, `base64`, `wslpath`, `powershell.exe`, `taskkill.exe`. Yeni bağımlılık yok.
- Başta kısa İngilizce yorum ve kullanım satırları (`tools/check-env.sh` gibi).
- Ortam değişkenleri (hepsi override edilebilir):

| Değişken | Varsayılan | Anlamı |
|---|---|---|
| `FDP_RUNTIME_DIR` | `/mnt/c/dev/fdp` | Çalışma dizini `$FDP_RUNTIME_DIR/server` |
| `FDP_SERVER_BIN_DIR` | `$ROOT/build/bin/x86-$CONFIG/Server` | Çalıştırılacak exe'lerin klasörü. Verilirse `--config`'ten önceliklidir. |
| `FDP_START_TIMEOUT` | `300` | Sunucu başına en fazla bekleme (sn) |
| `FDP_STOP_TIMEOUT` | `20` | Düzgün kapanış için sunucu başına bekleme (sn); sonra zorla |

- Komut satırı: ilk konumsal argüman komuttur (`start` | `stop` | `status`), ardından seçenekler.
  - `start`: `--config Release|Debug` (vars. `Release`), `--keep-on-fail`
  - `stop`: `--force`
  - `status`: seçenek yok
  - `-h|--help` her yerde: kullanım yazar, çıkış `0`.
  - Bilinmeyen komut/seçenek veya komuta ait olmayan seçenek: kullanım `stderr`'e, çıkış `2`.
- **Çıkış kodları:** `0` başarı · `1` işlem başarısız veya reddedildi (açılış başarısız, zaman aşımı, zaten çalışıyor, istemci bağlı, durdurulamadı, `status`'ta 3/3 hazır değil) · `2` kullanım/yapılandırma hatası (eksik exe/ini/klasör, port uyuşmazlığı, `powershell.exe` yok).
- **Varsayılan exe klasörünün Release derlemesi olmasının nedeni:** F0-01 Bulgu 5 (`docs/02` §2.1): Debug derlemesi quest kapısını, blink'i ve oturum zaman aşımını kapatır; bot testleri Release ile yapılır. Ayrıca kod değişince çalışma klasörüne kopyalamayı unutma riski ortadan kalkar.

### 5.2 Yardımcı fonksiyonlar (iskelet; uygulamayı sen yaz)

**PowerShell çağrısı** (Claude tarafından denendi, çalışıyor):

```bash
# Runs a PowerShell snippet without quoting issues; prints its stdout without CR.
ps_run() {
	local script enc
	script="\$ProgressPreference='SilentlyContinue'; \$ErrorActionPreference='SilentlyContinue'; $1"
	enc="$(printf '%s' "$script" | iconv -f utf-8 -t utf-16le | base64 -w0)"
	powershell.exe -NoProfile -NonInteractive -EncodedCommand "$enc" 2>/dev/null | tr -d '\r'
}
```

PowerShell betiğine yalnızca **sayı** (pid, port) ve **`wslpath -w` çıktısı yollar** göm. Yolları tek tırnak içinde ver ve içlerindeki `'` karakterini `''` yap.

**Süreç bulma.** Çıktı biçimi `pid|ad|exe-yolu` (denendi):

```powershell
Get-CimInstance Win32_Process -Filter "Name='AIServer.exe' OR Name='GameServer.exe' OR Name='LogInServer.exe'" |
  ForEach-Object { '{0}|{1}|{2}' -f $_.ProcessId, $_.Name, $_.ExecutablePath }
```

Bir süreç **bizim sunucumuz** sayılır ancak ve ancak:

1. adı üç addan biri, **ve**
2. `ExecutablePath` boş değil, **ve**
3. exe'nin bulunduğu klasör (büyük/küçük harf duyarsız) şu izinli klasörlerden biri: `wslpath -w "$FDP_RUNTIME_DIR/server"`, `wslpath -w "$ROOT/build/bin/x86-Release/Server"`, `wslpath -w "$ROOT/build/bin/x86-Debug/Server"`, `wslpath -w "$FDP_SERVER_BIN_DIR"` (tanımlıysa).

Diğerleri **hiçbir zaman** durdurulmaz. `status` bunları bilgi satırıyla gösterir: `[NOTE] yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)` veya `(izinli klasör dışında: <yol>)`.

**Tek sürecin durumu.** Çıktı tek satır `alive=<0|1> pause=<n> listen=<n> ai=<n> clients=<n>` (Claude çalışan GameServer'da denedi: `alive=1 pause=0 listen=1 ai=1 clients=0`):

```powershell
$p = Get-Process -Id PID
$alive  = [int]($null -ne $p)
$pause  = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=PID AND Name='cmd.exe'").Count
$listen = @(Get-NetTCPConnection -State Listen -LocalPort PORT -OwningProcess PID).Count
$ai = 0; if (AIPORT -gt 0) { $ai = @(Get-NetTCPConnection -State Established -RemotePort AIPORT -OwningProcess PID).Count }
$cli    = @(Get-NetTCPConnection -State Established -LocalPort PORT -OwningProcess PID).Count
"alive=$alive pause=$pause listen=$listen ai=$ai clients=$cli"
```

(`PID`, `PORT`, `AIPORT` bash'ten gömülür; `AIPORT` yalnızca GameServer için verilir, diğerlerinde `0`.)

**Sunucu durumu** bu satırdan şöyle türetilir:

| Durum | Koşul |
|---|---|
| `UP` | `alive=1`, `pause=0`, `listen≥1`; GameServer için ek olarak `ai≥1` |
| `PARTIAL` | yalnızca GameServer: `alive=1`, `pause=0`, `listen≥1`, `ai=0` (AI bağlantısı yok) |
| `STARTING` | `alive=1`, `pause=0`, hazır koşulu yok |
| `FAILED` | `alive=1`, `pause≥1` (açılış başarısız, `system("pause")` bekliyor) |
| `DOWN` | bizim sunucumuz sayılan süreç yok veya `alive=0` |

**Port okuma** (bölüme duyarlı; değer dışında hiçbir şey yazdırma):

```bash
# Prints the PORT value of [section] in an ini file, or nothing.
ini_port() {
	tr -d '\r' < "$1" | awk -v sec="[$2]" '
		/^[[:space:]]*\[/ { in_sec = ($0 == sec); next }
		in_sec && /^[[:space:]]*PORT[[:space:]]*=/ { sub(/^[^=]*=[[:space:]]*/, ""); print; exit }'
}
```

Değer yoksa varsayılan (10020 / 15001 / 15100) kullanılır. Değer var ama 1–65535 arası bir sayı değilse çıkış `2`. GameServer'ın `[AI_SERVER] PORT` değeri AIServer'ın `[SETTINGS] PORT` değerinden farklıysa çıkış `2`, iki değeri de yaz (port gizli bilgi değildir).

### 5.3 Komutlar

**`status`**

1. Süreçleri bul (§5.2), portları oku.
2. Her sunucu için tek satır (sıra: AIServer, GameServer, LogInServer):

```
[UP]       AIServer     pid=34044  port=10020 LISTEN                        C:\dev\fdp\server\AIServer.exe
[UP]       GameServer   pid=34476  port=15001 LISTEN  AI=bağlı  istemci=0    C:\dev\fdp\server\GameServer.exe
[UP]       LogInServer  pid=29804  port=15100 LISTEN                        C:\dev\fdp\server\LogInServer.exe
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 3/3 hazır
```

3. Aynı addan birden çok bizim süreç varsa hepsini ayrı satırda yaz.
4. Çıkış: üçü de `UP` ise `0`, değilse `1`.

**`start`**

1. Ön kontroller (başarısızsa çıkış `2`): `powershell.exe` var; exe klasöründe üç exe var; `$FDP_RUNTIME_DIR/server` var; içinde üç ini var; portlar okunabiliyor ve tutarlı (§5.2).
2. Çalışan kontrolü (başarısızsa çıkış `1`, **hiçbir süreç başlatılmaz**):
   - bizim sunucumuz sayılan herhangi bir süreç varsa, **veya**
   - üç porttan biri herhangi bir süreç tarafından LISTEN ediliyorsa (`Get-NetTCPConnection -State Listen -LocalPort <a>,<b>,<c>`; sahibi pid'i yaz),
   - mesaj: zaten çalışıyor, önce `stop`.
3. Sırayla AIServer → GameServer → LogInServer. Her biri için:
   - Başlat ve pid'i al (denendi):
     ```powershell
     (Start-Process -FilePath '<exe-windows-yolu>' -WorkingDirectory '<server-windows-yolu>' -WindowStyle Minimized -PassThru).Id
     ```
     Dönen değer sayı değilse başarısızlık say.
   - Ekrana: `AIServer başlatılıyor: <exe yolu> (çalışma dizini <yol>)`.
   - 2 sn aralıkla durum yokla (§5.2), en fazla `FDP_START_TIMEOUT` sn. Her ~10 sn'de bir ilerleme satırı yaz (`... GameServer bekleniyor (40 sn)`), uzun bekleyişte çıktı sessiz kalmasın.
   - `UP` → `[UP] <ad> pid=<pid> (<n> sn)`, sonrakine geç.
   - `FAILED` → `[FAIL] <ad>: açılış başarısız (sunucu "pause" bekliyor; ayrıntı konsol penceresinde)`.
   - `alive=0` → `[FAIL] <ad>: süreç kapandı`.
   - süre doldu → `[FAIL] <ad>: <n> sn içinde hazır olmadı (son durum: <durum>)`.
4. Herhangi bir `[FAIL]`'de **sonraki sunucular başlatılmaz** ve:
   - `--keep-on-fail` **yoksa**: bu çağrıda başlatılan tüm süreçleri ters sırayla durdur (başarısız olan `FAILED` ise doğrudan zorla; diğerleri §`stop` adım 3'teki gibi). Çıkış `1`.
   - `--keep-on-fail` **varsa**: hiçbir şeyi durdurma; `inceleme için açık bırakıldı; kapatmak için: tools/run-servers.sh stop` yaz. Çıkış `1`.
5. Hepsi `UP` ise `status` çıktısının aynısını yaz, çıkış `0`.

**`stop`**

1. Bizim sunucumuz sayılan süreçleri bul. Hiç yoksa `çalışan sunucu yok` yaz, çıkış `0`.
2. İstemci koruması: GameServer süreçlerinden birinde `clients>0` ise ve `--force` **yoksa**: `[REFUSE] GameServer'a bağlı <n> istemci var; durdurmak için --force` yaz, **hiçbir şeyi durdurma**, çıkış `1`.
3. Ters sırayla LogInServer → GameServer → AIServer. Her süreç için:
   - Durumu `FAILED` ise: `taskkill.exe /F /T /PID <pid>` → `[STOP] <ad> pid=<pid> (zorla: pause)`.
   - Değilse: `taskkill.exe /PID <pid>` (**/F yok**, düzgün kapanış). `FDP_STOP_TIMEOUT` sn'ye kadar 1–2 sn aralıkla çıkmasını bekle. Çıktıysa `[STOP] <ad> pid=<pid> (nazik, <n> sn)`. Çıkmadıysa `taskkill.exe /F /T /PID <pid>` → `[STOP] <ad> pid=<pid> (zorla: zaman aşımı)`.
4. Sonunda süreçleri yeniden bul. Bizim süreç kalmadıysa çıkış `0`; kaldıysa `[FAIL]` satırı ve çıkış `1`.

### 5.4 Çalıştır ve raporla (sırası önemli)

> **Komut zaman aşımı:** `start` birkaç dakika sürebilir. Komutları çalıştırırken aracın zaman aşımını **en az 600 sn** ver. Araç komutu yarıda keserse sunucular yine de açılır; o zaman `status` ile durumu görüp devam et.
>
> **İnsan koruması:** Adım 1'de GameServer'a bağlı istemci (`istemci>0`) görürsen biri oyunda demektir: **`--force` kullanma**, `Durum: UYGULANIYOR (BLOKE)` yaz, nedenini rapora ekle ve dur.

1. `./tools/build.sh Release`; ardından `./tools/run-servers.sh status; echo "exit=$?"`. **Başlangıç durumunu not et:** üçü `UP` ve yolları `C:\dev\fdp\server\` altında mı?
2. `./tools/run-servers.sh stop; echo "exit=$?"`
3. `./tools/run-servers.sh status; echo "exit=$?"` ve bağımsız port kontrolü (betikten bağımsız; boş çıkmalı):
   ```bash
   powershell.exe -NoProfile -Command "Get-NetTCPConnection -State Listen -LocalPort 10020,15001,15100 -ErrorAction SilentlyContinue | Select-Object LocalPort,OwningProcess"
   ```
4. **Başarısızlık testi** (AIServer olmayan bir DSN ile açılamaz → `system("pause")`):
   ```bash
   FAILDIR="$(wslpath "$(cmd.exe /c echo %TEMP% 2>/dev/null | tr -d '\r')")/fdp-f0-02-failtest"
   mkdir -p "$FAILDIR/server"
   printf '[ODBC]\r\nGAME_DSN=KO_F0_02_YOK\r\n[SETTINGS]\r\nPORT=10020\r\n' > "$FAILDIR/server/AIServer.ini"
   : > "$FAILDIR/server/GameServer.ini"; : > "$FAILDIR/server/LogInServer.ini"
   time FDP_RUNTIME_DIR="$FAILDIR" ./tools/run-servers.sh start; echo "exit=$?"
   ./tools/run-servers.sh status; echo "exit=$?"
   ```
   Beklenen: AIServer `[FAIL] … açılış başarısız`, GameServer ve LogInServer **başlatılmadı**, çıkış `1`; ardından `status` hiçbir bizim süreç göstermez.
5. **`--keep-on-fail` testi:** aynı `FAILDIR` ile `FDP_RUNTIME_DIR="$FAILDIR" ./tools/run-servers.sh start --keep-on-fail; echo "exit=$?"` → çıkış `1`; `./tools/run-servers.sh status` → AIServer `FAILED`; `./tools/run-servers.sh stop; echo "exit=$?"` → `(zorla: pause)`, çıkış `0`; `status` → bizim süreç yok.
6. `time ./tools/run-servers.sh start; echo "exit=$?"` → 3/3 `UP`, yollar `build\bin\x86-Release\Server\` altında. Her sunucunun hazır olma süresini rapora yaz.
7. Çift başlatma: `./tools/run-servers.sh start; echo "exit=$?"` → reddedilir, çıkış `1`. Öncesinde ve sonrasında süreç listesini (§5.2 sorgusu) al; **pid'ler aynı kalmalı.**
8. `./tools/run-servers.sh status; echo "exit=$?"` → `exit=0`.
9. `./tools/run-servers.sh stop; echo "exit=$?"` → üçü de `(nazik, …)` olmalı. `(zorla: zaman aşımı)` çıkarsa bu bir bulgudur: rapora yaz, betiği "düzeltmek" için `FDP_STOP_TIMEOUT`'u büyütme.
10. Debug yolu: `./tools/run-servers.sh start --config Debug; echo "exit=$?"` → 3/3 `UP`, yollar `x86-Debug` altında; ardından `./tools/run-servers.sh stop`.
11. **Başlangıç durumuna dön.** Adım 1'de üçü `C:\dev\fdp\server\` altından çalışıyorduysa:
    `FDP_SERVER_BIN_DIR=/mnt/c/dev/fdp/server ./tools/run-servers.sh start; echo "exit=$?"` → 3/3 `UP`, yollar `C:\dev\fdp\server\` altında. Adım 1'de kapalıydılarsa kapalı bırak.
12. Kullanım hataları: `./tools/run-servers.sh; echo "exit=$?"` (komut yok → `2`), `./tools/run-servers.sh status --force; echo "exit=$?"` (`2`), `./tools/run-servers.sh start --config Foo; echo "exit=$?"` (`2`), `./tools/run-servers.sh -h; echo "exit=$?"` (`0`).
13. Hepsini Uygulayıcı Raporu'na **kırpmadan** yapıştır. Raporda `pid` dışında kişisel/gizli değer olmamalı.

## 6. Kabul kriterleri

- [ ] **K1** `./tools/build.sh Release` hatasız biter.
- [ ] **K2** `tools/run-servers.sh`: `bash -n` temiz; `file` çıktısı `ASCII text executable` (CRLF yok); `git ls-files -s tools/run-servers.sh` modu `100755`.
- [ ] **K3** Kullanım: §5.4 adım 12'deki dört çağrı sırasıyla `2, 2, 2, 0` döner.
- [ ] **K4** `status`, çalışan üç sunucu için `UP` satırları + `Özet: 3/3 hazır` yazar ve `0` döner; durdurulmuş hâlde `1` döner. Yolu okunamayan veya izinli klasör dışındaki aynı adlı süreçler (varsa) `[NOTE]` ile gösterilir ve **hiçbir komut onları durdurmaz** (`stop` öncesi/sonrası süreç listesiyle gösterilir).
- [ ] **K5** `stop` üç sunucuyu **LogInServer → GameServer → AIServer** sırasıyla, `taskkill` **/F olmadan** düzgün kapatır (adım 9'da üçü de `nazik`); sonrasında bağımsız port kontrolü (adım 3 komutu) boş, çıkış `0`.
- [ ] **K6** `start` (varsayılan Release) üç sunucuyu sırayla açar; **GameServer yalnızca AIServer portuna `Established` bağlantısı görüldükten sonra** `UP` sayılır (kodda §5.2 koşulu; çıktıda `AI=bağlı`); çıkış `0`; her sunucunun hazır olma süresi raporda.
- [ ] **K7** Sunucular çalışırken ikinci `start` çıkış `1` ile reddedilir ve yeni süreç açmaz (adım 7: pid listesi aynı).
- [ ] **K8** Başarısızlık testi (adım 4): AIServer `FAILED` olarak tanınır, GameServer/LogInServer başlatılmaz, çıkış `1`, sonrasında bizim süreç kalmaz. Toplam süre `FDP_START_TIMEOUT`'tan belirgin kısadır (zaman aşımına düşmeden, `pause` alt sürecinden anlaşılır).
- [ ] **K9** `--keep-on-fail` (adım 5): başarısız süreç açık kalır, `status` onu `FAILED` gösterir, `stop` onu `(zorla: pause)` ile kapatır.
- [ ] **K10** `--config Debug` (adım 10) üç sunucuyu `x86-Debug` exe'leriyle açar (yollar `status` çıktısında).
- [ ] **K11** İstemci koruması: `stop`, GameServer'da `clients>0` iken `--force` olmadan hiçbir süreci durdurmaz ve `1` döner (kod incelemesiyle doğrulanır; bu ortamda istemci bağlanmadan test edilemez, raporda belirt).
- [ ] **K12** Başlangıç durumu geri getirildi (adım 11): başta çalışıyorlarsa sonunda yine `C:\dev\fdp\server\` exe'leriyle 3/3 `UP`.
- [ ] **K13** Gizlilik: betik ini'lerden yalnızca `PORT` okur (`grep -n 'UID\|PWD\|DSN' tools/run-servers.sh` boş); `Logs/` okumaz; `sqlcmd` çağırmaz; çıktıda parola/kullanıcı adı/bağlantı dizesi yok.
- [ ] **K14** `git diff --stat main...bot/F0-02` yalnızca §4'teki iki dosyayı gösterir; `GameServer/ AIServer/ LogInServer/ shared/ N3BASE/` altında değişiklik yok; betikte `rm -rf` yok.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release
bash -n tools/run-servers.sh && file tools/run-servers.sh && git ls-files -s tools/run-servers.sh
grep -n 'UID\|PWD\|DSN\|sqlcmd\|SQLCMD\|Logs/\|rm -rf' tools/run-servers.sh   # boş olmalı
./tools/run-servers.sh status; echo "exit=$?"
./tools/run-servers.sh stop; echo "exit=$?"
powershell.exe -NoProfile -Command "Get-NetTCPConnection -State Listen -LocalPort 10020,15001,15100 -ErrorAction SilentlyContinue | Select-Object LocalPort,OwningProcess"
./tools/run-servers.sh start; echo "exit=$?"
./tools/run-servers.sh start; echo "exit=$?"     # 1 beklenir
./tools/run-servers.sh stop; echo "exit=$?"
FDP_SERVER_BIN_DIR=/mnt/c/dev/fdp/server ./tools/run-servers.sh start   # başlangıç durumu buysa
git diff --stat main...bot/F0-02
```

## 8. Kısıtlar ve uyarılar

- **Kod değiştirme.** Bu plan yalnızca `tools/run-servers.sh` ekler (K14).
- **Yanlış süreci öldürme riski.** Süreçleri yalnızca adla eşleştirme; §5.2'deki üç koşulun **hepsi** gerekli. `ExecutablePath` boş olan süreç (ör. pid 4336) asla hedef değildir. `taskkill /IM` (adla öldürme) **yasak**; her zaman `/PID`.
- **GameServer'da port ≠ hazır.** GameServer portu tablolardan önce açar (`GameServer/GameServerDlg.cpp:97`). Yalnızca port dinlemesine bakan bir uygulama K6'yı karşılamaz.
- **AIServer'ın NPC yüklemesinin bitişi dışarıdan görülmez.** AIServer `UP` = DB bağlı + port açık. NPC'lerin hazır olduğu yalnızca konsol penceresindeki `Monster All Init Success` satırından anlaşılır (`AIServer/Npc.cpp:790`). Bunu betikte çözmeye çalışma; raporda bilgi olarak geç.
- **Konsol çıktısı yakalanmaz** (KI-004). Sunucular kendi küçültülmüş pencerelerinde açılır; hata ayrıntısı oradadır. Çıktıyı dosyaya yönlendirmeye çalışma.
- **Güvenlik duvarı:** `build\` altındaki exe'ler ilk kez port açınca Windows bir "erişime izin ver" penceresi gösterebilir. Yerel (127.0.0.1) bağlantıları engellemez; testi durdurmaz. Görürsen raporda belirt; kural ekleme.
- **Veritabanı:** Betik DB'ye bağlanmaz. Sunucuların kendi DB erişimi normal işleyiştir. Yasak tablo listesi `AGENTS.md` §2.7.
- **Gizli bilgi:** ini içeriği (port dışında), `Logs/` içeriği, DSN kullanıcı/parolası asla ekrana, rapora veya commit'e girmez.
- `tools/*` düzenleme izni `ask` (`opencode.json`); bu normaldir.
- Ortamdan kaynaklı bir başarısızlık (ör. SQL Server kapalı → sunucular `FAILED`) senin hatan değildir: betik doğru çalışıyor demektir. Ortamı düzeltmeye çalışma, raporda belirt.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

_(henüz yok)_

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

_(henüz yok)_
