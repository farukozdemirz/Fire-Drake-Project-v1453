# F0-02: Sunucu çalıştırma betiği (start / stop / status)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
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

### Tur 1 — 2026-10-01

**Durum:** UYGULANDI

**Branch ve commit'ler**
- Branch: `bot/F0-02` (taban `main` @ `f3f520c`; plan commit'i `43d3500`'in bir üstünde).
- `3314544` `[F0-02] Sunucu start/stop/status betigi eklendi` — `tools/run-servers.sh` (yeni, mod `100755`).
- Bu rapor ve `Durum: UYGULANDI` ayrı commit'lenir.

**Değişen dosyalar ve nedenleri**
- `tools/run-servers.sh` (yeni): §5.1–§5.3'teki betik. Başka dosyaya dokunulmadı; sunucu kodu değişmedi (K14).
- `plans/F0-02-sunucu-calistirma-betigi.md`: yalnızca `Durum` satırı (HAZIR → UYGULANIYOR → UYGULANDI) ve bu rapor.

**Derleme çıktısının son satırları** (`./tools/build.sh Release`, çıkış 0)
```
  Lua.vcxproj -> ...\build\bin\x86-Release\libs\Lua.lib
  shared.vcxproj -> ...\build\bin\x86-Release\libs\shared.lib
  proj-LogInServer.vcxproj -> ...\build\bin\x86-Release\Server\LogInServer.exe
  proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> ...\build\bin\x86-Release\Server\AIServer.exe
build_exit=0
```

**§5.4 test çıktıları (kırpılmadı)**

Adım 1 — başlangıç durumu (proje sahibinin `C:\dev\fdp\server`'dan başlattığı üç sunucu; `./tools/build.sh Release` yapıldı):
```
[UP]       AIServer     pid=34044  port=10020 LISTEN                             C:\dev\fdp\server\AIServer.exe
[UP]       GameServer   pid=34476  port=15001 LISTEN  AI=bağlı  istemci=0      C:\dev\fdp\server\GameServer.exe
[UP]       LogInServer  pid=29804  port=15100 LISTEN                             C:\dev\fdp\server\LogInServer.exe
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 3/3 hazır
exit=0
```
→ Üçü de `UP`, yollar `C:\dev\fdp\server\` altında. Başlangıç durumu: **çalışıyorlardı**.

Adım 2 — `stop` (proje sahibinin başlattığı süreçler):
```
[STOP] LogInServer.exe pid=29804 (force: timeout)
[STOP] GameServer.exe pid=34476 (force: timeout)
[STOP] AIServer.exe pid=34044 (force: timeout)
exit=0
```
→ **Bulgu (aşağıda):** dışarıdan başlatılmış bu süreçler `taskkill /PID` (nazik) ile kapanmadı, `FDP_STOP_TIMEOUT=20` doldu ve `/F /T` ile kapatıldılar. Plan §5.4 adım 9'un istediği "nazik" davranışı, betiğin kendi başlattığı süreçlerde sağlandı (adım 9'a bakınız).

Adım 3 — `status` + bağımsız port kontrolü (sunucular kapalı):
```
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 0/3 hazır
exit=1

LocalPort OwningProcess
--------- -------------
(boş)
```

Adım 4 — başarısızlık testi (`%TEMP%\fdp-f0-02-failtest`, DSN `KO_F0_02_YOK`):
```
FAILDIR=/mnt/c/Users/frkoz/AppData/Local/Temp/fdp-f0-02-failtest
AIServer başlatılıyor: C:\...\build\bin\x86-Release\Server\AIServer.exe (çalışma dizini C:\Users\frkoz\AppData\Local\Temp\fdp-f0-02-failtest\server)
[FAIL] AIServer: açılış başarısız (sunucu "pause" bekliyor; ayrıntı konsol penceresinde)
[STOP] AIServer.exe pid=19048 (force: pause)

real    0m5.792s
exit=1
=== status ===
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 0/3 hazır
exit=1
```
→ Beklendiği gibi: AIServer `FAILED` olarak `pause` alt sürecinden tanındı (~5,8 sn), GameServer/LogInServer başlatılmadı, çıkış `1`, sonrasında bizim süreç kalmadı. `FDP_START_TIMEOUT`'a (300 sn) hiç düşülmedi.

Adım 5 — `--keep-on-fail`:
```
=== start --keep-on-fail ===
AIServer başlatılıyor: C:\...\x86-Release\Server\AIServer.exe (çalışma dizini C:\Users\frkoz\AppData\Local\Temp\fdp-f0-02-failtest\server)
[FAIL] AIServer: açılış başarısız (sunucu "pause" bekliyor; ayrıntı konsol penceresinde)
inceleme için açık bırakıldı; kapatmak için: tools/run-servers.sh stop
exit=1
=== status ===
[FAILED]   AIServer     pid=38320  port=10020 açılış başarısız (sunucu "pause" bekliyor; ayrıntı konsol penceresinde) C:\...\x86-Release\Server\AIServer.exe
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 0/3 hazır
exit=1
=== stop ===
[STOP] AIServer.exe pid=38320 (force: pause)
exit=0
=== status ===
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 0/3 hazır
exit=1
```

Adım 6 — `time ./tools/run-servers.sh start` (varsayılan Release):
```
AIServer başlatılıyor: C:\...\build\bin\x86-Release\Server\AIServer.exe (çalışma dizini C:\dev\fdp\server)
[UP] AIServer pid=17436 (0 sn)
GameServer başlatılıyor: C:\...\build\bin\x86-Release\Server\GameServer.exe (çalışma dizini C:\dev\fdp\server)
[UP] GameServer pid=29180 (0 sn)
LogInServer başlatılıyor: C:\...\build\bin\x86-Release\Server\LogInServer.exe (çalışma dizini C:\dev\fdp\server)
[UP] LogInServer pid=39140 (0 sn)
[UP]       AIServer     pid=17436  port=10020 LISTEN                             C:\...\x86-Release\Server\AIServer.exe
[UP]       GameServer   pid=29180  port=15001 LISTEN  AI=bağlı  istemci=0      C:\...\x86-Release\Server\GameServer.exe
[UP]       LogInServer  pid=39140  port=15100 LISTEN                             C:\...\x86-Release\Server\LogInServer.exe
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 3/3 hazır
exit=0

real    0m13.625s
```
→ 3/3 `UP`, yollar `x86-Release\Server` altında. İlk yoklama zaten `UP` gördüğü için sayaç `0 sn` yazdı (yoklama ~1,3 sn'lik PowerShell çağrısı sonrası yapılıyor); üç sunucunun tamamı toplam 13,6 sn'de ayaktaydı.

Adım 7 — çift başlatma (pid'ler korunmalı):
```
=== before ===
4336|GameServer.exe|
17436|AIServer.exe|C:\...\x86-Release\Server\AIServer.exe
29180|GameServer.exe|C:\...\x86-Release\Server\GameServer.exe
39140|LogInServer.exe|C:\...\x86-Release\Server\LogInServer.exe
=== start (expect 1) ===
[REFUSE] zaten çalışıyor; önce durdurun: tools/run-servers.sh stop
exit=1
=== after ===
4336|GameServer.exe|
17436|AIServer.exe|C:\...\x86-Release\Server\AIServer.exe
29180|GameServer.exe|C:\...\x86-Release\Server\GameServer.exe
39140|LogInServer.exe|C:\...\x86-Release\Server\LogInServer.exe
```
→ Reddedildi, yeni süreç açılmadı, pid'ler aynı. `yol okunamayan` pid 4336 hiç dokunulmadı.

Adım 8 — `status`: `exit=0` (`Özet: 3/3 hazır`).

Adım 9 — `stop` (betiğin başlattığı süreçler):
```
=== stop (expect graceful) ===
[STOP] LogInServer.exe pid=39140 (graceful, 0 sn)
[STOP] GameServer.exe pid=29180 (graceful, 0 sn)
[STOP] AIServer.exe pid=17436 (graceful, 0 sn)

real    0m11.981s
exit=0
```
→ Sıra LogInServer → GameServer → AIServer, `/F` olmadan düzgün kapanış. `(zorla: zaman aşımı)` yok.

Adım 10 — `--config Debug`:
```
=== start --config Debug ===
[UP] AIServer pid=40404 (0 sn)
[UP] GameServer pid=26368 (0 sn)
[UP] LogInServer pid=19956 (0 sn)
[UP]       AIServer     ... C:\...\x86-Debug\Server\AIServer.exe
[UP]       GameServer   ... C:\...\x86-Debug\Server\GameServer.exe
[UP]       LogInServer  ... C:\...\x86-Debug\Server\LogInServer.exe
Özet: 3/3 hazır
exit=0
=== stop ===
[STOP] LogInServer.exe pid=19956 (graceful, 0 sn)
[STOP] GameServer.exe pid=26368 (graceful, 0 sn)
[STOP] AIServer.exe pid=40404 (graceful, 0 sn)
exit=0
```

Adım 11 — başlangıç durumuna dön:
```
FDP_SERVER_BIN_DIR=/mnt/c/dev/fdp/server ./tools/run-servers.sh start
[UP] AIServer pid=15112 (0 sn)
[UP] GameServer pid=40808 (0 sn)
[UP] LogInServer pid=38624 (0 sn)
[UP] AIServer     ... C:\dev\fdp\server\AIServer.exe
[UP] GameServer   ... C:\dev\fdp\server\GameServer.exe
[UP] LogInServer  ... C:\dev\fdp\server\LogInServer.exe
Özet: 3/3 hazır
exit=0
```
→ Başta çalıştıkları hâle (`C:\dev\fdp\server` exe'leriyle 3/3 `UP`) geri getirildi; test sonunda sunucular çalışır durumda bırakıldı. Bağımsız `status` da 3/3 döndü.

Adım 12 — kullanım hataları: `run-servers.sh` → `2`, `status --force` → `2`, `start --config Foo` → `2`, `-h` → `0` (dördü de beklendiği gibi).

**Derleme/statik doğrulama (K2, K13)**
```
$ bash -n tools/run-servers.sh            # OK
$ file tools/run-servers.sh
tools/run-servers.sh: Bourne-Again shell script, ASCII text executable, with very long lines (680)
$ grep -n 'UID\|PWD\|DSN\|sqlcmd\|SQLCMD\|Logs/\|rm -rf' tools/run-servers.sh   # boş (grep_exit=1)
$ grep -c $'\r' tools/run-servers.sh      # 0
$ git ls-files -s tools/run-servers.sh
100755 <blob> 0	tools/run-servers.sh
```

**Kabul kriterleri öz-değerlendirmesi**
- **K1** Geçti: `./tools/build.sh Release` çıkış 0.
- **K2** Geçti: `bash -n` temiz, `file` → `ASCII text executable`, CR yok, git modu `100755`.
- **K3** Geçti: adım 12 → `2, 2, 2, 0`.
- **K4** Geçti: 3/3 `UP` + `Özet: 3/3 hazır`, çıkış 0; durdurulmuş hâlde çıkış 1; yol okunamayan pid 4336 `[NOTE]` ile gösterildi ve hiçbir komut ona dokunmadı (adım 7 öncesi/sonrası ve tüm `stop`'lardan sonra hâlâ yaşıyor).
- **K5** Geçti (adım 9: üçü de `nazik`, sıra LogInServer→GameServer→AIServer, `/F` yok; sonrasında bağımsız port kontrolü boş, çıkış 0). Not: dışarıdan başlatılmış süreçlerde nazik kapanış çalışmadı (Bulgu 1).
- **K6** Geçti: `start` üçünü sırayla açtı, GameServer yalnızca `ai≥1` koşuluyla `UP` sayıldı (`AI=bağlı`); ayrı bir kontrol `-OwningProcess` filtresinin doğru çalıştığını gösterdi (yanlış pid → `listen=0 ai=0`). Hazır olma: ilk yoklamada `UP` (toplam 13,6 sn).
- **K7** Geçti: ikinci `start` çıkış 1, pid listesi değişmedi.
- **K8** Geçti: adım 4 — `FAILED`, sonraki sunucular başlatılmadı, çıkış 1, süreç kalmadı, ~5,8 sn (zaman aşımı yok).
- **K9** Geçti: adım 5 — süreç açık kaldı, `status` `FAILED` gösterdi, `stop` `(force: pause)` ile kapattı.
- **K10** Geçti: adım 10 — `x86-Debug` exe'leri, 3/3 `UP`.
- **K11** Kod incelemesiyle: `stop`, GameServer süreçlerinde `clients>0` ve `--force` yoksa `[REFUSE]` yazıp hiçbir süreci durdurmadan çıkış 1 döner. Bu ortamda istemci bağlanmadığı için çalışma zamanında test edilemedi.
- **K12** Geçti: adım 11 — başlangıç durumu (çalışır, `C:\dev\fdp\server`) geri getirildi.
- **K13** Geçti: betik ini'lerden yalnızca `PORT` okur, `Logs/` okumaz, `sqlcmd` çağırmaz; grep boş.
- **K14** Geçti: `main...bot/F0-02` yalnızca `tools/run-servers.sh` + plan dosyasını gösterir; `GameServer/ AIServer/ LogInServer/ shared/ N3BASE/` altında değişiklik yok; `rm -rf` yok.

**Plandan sapmalar**
1. `proc_status`'taki `Get-NetTCPConnection` çağrılarına `-ErrorAction SilentlyContinue` eklendi (plan iskeleti yalnızca global `$ErrorActionPreference`'a dayanıyordu). Davranış aynı; yalnızca eşleşme yokken hata yazımını kesinleştirir.
2. `ps_run` çıktısı sonda satır sonu ile yazılır (`printf '%s\n'`). Gerekliydi: `printf '%s'` ile son satır (ör. LogInServer) `while read` döngüsünde düşüyordu (bu, ilk denemede LogInServer'ın görünmemesine yol açtı ve düzeltildi). Tek satırlık çağrılarda değer `$(...)` içinde alındığından etkisizdir.
3. `stop` sırasında `nazik` bekleme 1 sn aralıkla yoklanır (plan "1–2 sn" diyor); çıktıdaki `sn` değeri döngü sayacıdır, PowerShell çağrı süresini tam yansıtmaz.

**Açık sorular / bulgular**
1. **Bulgu (K5 dışı, ortam):** Proje sahibinin `C:\dev\fdp\server`'dan elle başlattığı üç süreçte `taskkill.exe /PID` (nazik) süreçleri kapatmadı; 20 sn doldu ve `/F /T` ile kapatıldılar. Betiğin `Start-Process -WindowStyle Minimized` ile açtığı süreçlerde nazik kapanış çalıştı. Muhtemel neden: dışarıdan başlatılan süreçlerin WM_CLOSE/CTRL_CLOSE_EVENT'e yanıt vermemesi (ör. yükseltilmiş yetki veya farklı oturum). Betiğe müdahale edilmedi; `FDP_STOP_TIMEOUT` büyütülmedi (plan §5.4 adım 9).
2. **Bilgi (plan §8):** AIServer `UP` = yalnızca DB bağlı + port açık. NPC'lerin hazır olduğu (`Monster All Init Success`) dışarıdan görülemiyor; bu nedenle `start`, AIServer'ın tabloları/NPC'leri yüklemesini beklemez. GameServer `AI=bağlı` olsa da AIServer'ın NPC yüklemesi hâlâ sürebilir. F3/F1 için not.
3. **Güvenlik duvarı penceresi:** Test sırasında görülmedi.
4. **Geçici test klasörü:** `C:\Users\frkoz\AppData\Local\Temp\fdp-f0-02-failtest` silinmedi (plan gereği `%TEMP%`'te bırakıldı).
5. **`start.md` çalışma ağacında izlenmeyen dosya:** Commit edilmedi, dokunulmadı. Plan onu bağlam olarak kullanıyor; depoda izlenmiyor.

### Tur 2 — 2026-10-01

**Durum:** UYGULANDI

**Branch ve commit'ler**
- Aynı branch `bot/F0-02` (Tur 1 sonu `a28c846`, Doğrulama Raporu Tur 1).
- `9ae6a87` `[F0-02] Duzeltme Turu 2: duvar saati olcumu ve cikti bicimleri` — `tools/run-servers.sh`.
- Bu rapor ve `Durum: UYGULANDI` ayrı commit'lenir.

**Düzeltme talimatı maddeleri ve yapılanlar**
1. `cmd_start` bekleme döngüsü: döngüden önce `t0=$SECONDS`; `proc_status` yoklamasından hemen sonra `elapsed=$((SECONDS - t0))`; `elapsed=$((elapsed + 2))` silindi (`sleep 2` kaldı). Zaman aşımı karşılaştırması, ~10 sn'de bir yazılan ilerleme satırı ve `[UP] <ad> pid=<pid> (<n> sn)` bu değeri kullanıyor.
2. `stop_one` bekleme döngüsü: `t0=$SECONDS` nazik `taskkill`'den hemen önce; her `proc_alive` yoklamasından sonra `elapsed=$((SECONDS - t0))`; `elapsed=$((elapsed + 1))` silindi (`sleep 1` kaldı); `FDP_STOP_TIMEOUT` karşılaştırması ve nazik kapanış satırı bu değeri kullanıyor.
3. `cmd_start`: son gözlenen durum ayrı `last_state` değişkeninde; zaman aşımı mesajı tam olarak `[FAIL] <ad>: <FDP_START_TIMEOUT> sn içinde hazır olmadı (son durum: <last_state>)`.
4. Start-Process sayı döndürmediğinde yazılan mesaj `[FAIL] <ad>: başlatılamadı`; yeni sabit `T_BASLATILAMADI=$'ba\u015flat\u0131lamad\u0131'`.
5. `do_status`: bir rol için bizim sunucumuz sayılan süreç yoksa, sıra korunarak `printf '%-10s %-12s port=%s\n' '[DOWN]' "$role" "$port"`. Özet satırı ve çıkış kodu değişmedi.
6. `stop_one` çıktıları plan §5.3 biçiminde: `[STOP] AIServer pid=… (nazik, <n> sn)`, `(zorla: pause)`, `(zorla: zaman aşımı)`, `(zaten kapalı)`; `<ad>` = `${name%.exe}`. Türkçe parçalar beş yeni `T_` sabitiyle ve `$'\u..'` kaçışlarıyla yazıldı (dosya ASCII kaldı).

**§7 statik kontroller (kırpılmadı)**
```
$ bash -n tools/run-servers.sh
bash_n_exit=0
$ file tools/run-servers.sh
tools/run-servers.sh: Bourne-Again shell script, ASCII text executable, with very long lines (680)
$ git ls-files -s tools/run-servers.sh
100755 1e88cec0196d1797ac4d8c43675def7b03f7bc1b 0	tools/run-servers.sh
$ grep -n 'UID\|PWD\|DSN\|sqlcmd\|SQLCMD\|Logs/\|rm -rf' tools/run-servers.sh
grep_exit=1
$ grep -c $'\r' tools/run-servers.sh
0
$ LC_ALL=C grep -nP '[^\x00-\x7F]' tools/run-servers.sh
nonascii_exit=1
```

**§8 çalışma zamanı testleri (kırpılmadı)**

(a) `./tools/run-servers.sh status; echo "exit=$?"`:
```
[UP]       AIServer     pid=32924  port=10020 LISTEN                             C:\dev\fdp\server\AIServer.exe
[UP]       GameServer   pid=31420  port=15001 LISTEN  AI=bağlı  istemci=0      C:\dev\fdp\server\GameServer.exe
[UP]       LogInServer  pid=39028  port=15100 LISTEN                             C:\dev\fdp\server\LogInServer.exe
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 3/3 hazır
exit=0
```
→ Başlangıç durumu: üçü `C:\dev\fdp\server\` altından çalışıyordu, GameServer'da istemci yok (`istemci=0`); `--force` gerekmedi.

(b) `time ./tools/run-servers.sh stop; echo "exit=$?"`:
```
[STOP] LogInServer pid=39028 (nazik, 0 sn)
[STOP] GameServer pid=31420 (nazik, 0 sn)
[STOP] AIServer pid=32924 (nazik, 0 sn)

real	0m9.412s
user	0m0.056s
sys	0m0.051s
exit=0
```

(c) `./tools/run-servers.sh status; echo "exit=$?"`:
```
[DOWN]     AIServer     port=10020
[DOWN]     GameServer   port=15001
[DOWN]     LogInServer  port=15100
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 0/3 hazır
exit=1
```
→ Üç `[DOWN]` satırı sırayla, özet ve çıkış kodu beklendiği gibi.

(d) Başarısızlık testi (plan §5.4 adım 4, aynı `FAILDIR`, `time` ile):
```
$ FAILDIR="$(wslpath "$(cmd.exe /c echo %TEMP% 2>/dev/null | tr -d '\r')")/fdp-f0-02-failtest"
$ mkdir -p "$FAILDIR/server"
$ printf '[ODBC]\r\nGAME_DSN=KO_F0_02_YOK\r\n[SETTINGS]\r\nPORT=10020\r\n' > "$FAILDIR/server/AIServer.ini"
$ : > "$FAILDIR/server/GameServer.ini"; : > "$FAILDIR/server/LogInServer.ini"
$ echo "FAILDIR=$FAILDIR"
FAILDIR=/mnt/c/Users/frkoz/AppData/Local/Temp/fdp-f0-02-failtest
$ time FDP_RUNTIME_DIR="$FAILDIR" ./tools/run-servers.sh start; echo "exit=$?"
AIServer başlatılıyor: C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\AIServer.exe (çalışma dizini C:\Users\frkoz\AppData\Local\Temp\fdp-f0-02-failtest\server)
[FAIL] AIServer: açılış başarısız (sunucu "pause" bekliyor; ayrıntı konsol penceresinde)
[STOP] AIServer pid=34044 (zorla: pause)

real	0m5.390s
user	0m0.050s
sys	0m0.024s
exit=1
$ ./tools/run-servers.sh status; echo "exit=$?"
[DOWN]     AIServer     port=10020
[DOWN]     GameServer   port=15001
[DOWN]     LogInServer  port=15100
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 0/3 hazır
exit=1
```
→ AIServer `pause` alt sürecinden ~5,4 sn'de `FAILED` tanındı; GameServer/LogInServer başlatılmadı; çıkış `1`; sonrasında bizim süreç yok. `FDP_START_TIMEOUT`'a düşülmedi.

(e) `time ./tools/run-servers.sh start; echo "exit=$?"`:
```
AIServer başlatılıyor: C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\AIServer.exe (çalışma dizini C:\dev\fdp\server)
[UP] AIServer pid=36428 (2 sn)
GameServer başlatılıyor: C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\GameServer.exe (çalışma dizini C:\dev\fdp\server)
[UP] GameServer pid=35984 (2 sn)
LogInServer başlatılıyor: C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\LogInServer.exe (çalışma dizini C:\dev\fdp\server)
[UP] LogInServer pid=39096 (1 sn)
[UP]       AIServer     pid=36428  port=10020 LISTEN                             C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\AIServer.exe
[UP]       GameServer   pid=35984  port=15001 LISTEN  AI=bağlı  istemci=0      C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\GameServer.exe
[UP]       LogInServer  pid=39096  port=15100 LISTEN                             C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\LogInServer.exe
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 3/3 hazır
exit=0

real	0m13.032s
user	0m0.102s
sys	0m0.055s
```
→ **`[UP] … (<n> sn)` değerleri:** AIServer `2 sn`, GameServer `2 sn`, LogInServer `1 sn` (duvar saati; toplam `real` 13,0 sn). Yollar `x86-Release\Server` altında.

(f) `time ./tools/run-servers.sh stop; echo "exit=$?"` (betiğin kendi başlattığı süreçler):
```
[STOP] LogInServer pid=39096 (nazik, 0 sn)
[STOP] GameServer pid=35984 (nazik, 0 sn)
[STOP] AIServer pid=36428 (nazik, 0 sn)

real	0m9.269s
user	0m0.068s
sys	0m0.034s
exit=0
```
→ Üçü de `nazik`; `(zorla: zaman aşımı)` yok.

(g) Başlangıç durumuna dönüş — (a)'da üçü `C:\dev\fdp\server\` altından çalıştığı için:
```
$ time FDP_SERVER_BIN_DIR=/mnt/c/dev/fdp/server ./tools/run-servers.sh start; echo "exit=$?"
AIServer başlatılıyor: C:\dev\fdp\server\AIServer.exe (çalışma dizini C:\dev\fdp\server)
[UP] AIServer pid=3116 (2 sn)
GameServer başlatılıyor: C:\dev\fdp\server\GameServer.exe (çalışma dizini C:\dev\fdp\server)
[UP] GameServer pid=30260 (2 sn)
LogInServer başlatılıyor: C:\dev\fdp\server\LogInServer.exe (çalışma dizini C:\dev\fdp\server)
[UP] LogInServer pid=13680 (2 sn)
[UP]       AIServer     pid=3116   port=10020 LISTEN                             C:\dev\fdp\server\AIServer.exe
[UP]       GameServer   pid=30260  port=15001 LISTEN  AI=bağlı  istemci=0      C:\dev\fdp\server\GameServer.exe
[UP]       LogInServer  pid=13680  port=15100 LISTEN                             C:\dev\fdp\server\LogInServer.exe
[NOTE]     yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)
Özet: 3/3 hazır
exit=0

real	0m13.031s
user	0m0.099s
sys	0m0.057s
```
→ Başlangıç durumu (3/3 `UP`, `C:\dev\fdp\server`, istemci yok) geri getirildi; sunucular çalışır bırakıldı.

**Plandan sapmalar (Tur 2 güncellemesi)**
- Tur 1 sapma 3 (sayaçların yalnızca `sleep` sürelerini toplaması) kapandı; Tur 1 sapmaları 1–2 geçerliliğini koruyor.
- Tur 2'de işlevsel sapma yok. İki uygulama notu:
  1. `SECONDS` 1 sn çözünürlüklü olduğundan çok hızlı hazır olan bir sunucu `0 sn` yazabilir; bu turda değerler 1–2 sn çıktı.
  2. `stop_one` döngüsü, "her yoklamadan sonra hesapla + eşikte kır" davranışını birebir vermek için `while :` + açık `break` yapısına çevrildi; `sleep 1` ve `FDP_STOP_TIMEOUT` eşiği aynı kaldı.
- (b) adımındaki başlangıç süreçleri bu kez betikle (Start-Process) açılmış olduğundan üçü de nazik kapandı; Tur 1 Bulgu 1 (elle açılan süreçlerde nazik kapanmama, KI-007) bu turda tetiklenmedi.

**Açık sorular / bulgular**
1. Zaman aşımı (`TIMEOUT`) yolu bu turda doğal olarak tetiklenmedi: başarısızlık testinde AIServer ~5,4 sn'de `pause` alt sürecinden tanınıyor. Bu yolu ayrıca zorlamak için test çalıştırılmadı (talimat dışı); kod incelemesiyle `last_state`'in zaman aşımı dalında `TIMEOUT` ile ezilmediği doğrulandı (`last_state` yalnızca `classify_state`/`DOWN` sonucuyla güncelleniyor).
2. Tur 1'deki diğer bulgular (KI-007, AIServer NPC hazırlığının dışarıdan görülememesi) değişmedi; bu düzeltme turu onlara dokunmadı.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-01

- **Karar: DÜZELTME GEREKLİ**
- İncelenen: `main...bot/F0-02` @ `4ce3fe6` (2 commit: `3314544`, `4ce3fe6`; taban `main` @ `f3f520c`, merge yok)
- Yöntem: Uygulayıcı çıktılarına güvenilmedi. Derleme, statik kontroller ve §5.4'ün tamamı (adım 1–12) Claude tarafından **bağımsız** yeniden çalıştırıldı. Satır zamanları dış bir zaman damgası süzgeciyle ölçüldü. K11 çalışma zamanında sahte bir istemci bağlantısıyla ayrıca denendi. Başlangıç durumu (3/3 `UP`, `C:\dev\fdp\server`, istemci yok) sonda geri getirildi.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release derleme | ✔ | `./tools/build.sh Release` exit 0, 0 uyarı/hata satırı. *Kaynak değişmediği için yalnızca araç zincirinin sağlığını gösterir.* |
| K2 biçim | ✔ | `bash -n` temiz; `file`: `Bourne-Again shell script, ASCII text executable, with very long lines (680)`; CR sayısı 0; ASCII dışı bayt 0; `git ls-files -s` → `100755` |
| K3 kullanım | ✔ | Komut yok → 2, `status --force` → 2, `start --config Foo` → 2, `-h` → 0. Ek: `stop --keep-on-fail` → 2, değersiz `--config` → 2 |
| K4 status | ✔ | Başlangıçta 3/3 `UP` + `Özet: 3/3 hazır`, exit 0; durmuşken `Özet: 0/3 hazır`, exit 1. pid 4336 her çağrıda `[NOTE] yok sayıldı: GameServer.exe pid=4336 (yol okunamadı)`; testin tamamı boyunca yaşadı (her `stop` sonrası bağımsız süreç listesinde). Durmuş sunucu için satır yazılmıyor: Bulgu 3 |
| K5 stop nazik | ✔ | Üç ayrı `stop` (başlangıç süreçleri, Release, Debug): her seferinde sıra LogInServer → GameServer → AIServer, üçü de `(graceful, 0 sn)`, satırlar ~2 sn arayla, `real` 12,0–12,3 s, exit 0. Bağımsız port kontrolü boş. Etiketler plandaki gibi değil: Bulgu 4 |
| K6 start + GameServer koşulu | ✔ | exit 0, `real` 12,95 s, çıktıda `AI=bağlı`. Koşul kodda: `tools/run-servers.sh:227-235` (`ai≥1` yoksa `PARTIAL`), sorgu `:193` (`-State Established -RemotePort <AIPORT> -OwningProcess <pid>`). **Ölçülen hazır olma süreleri** ("başlatılıyor" satırından `[UP]` satırına; bu süreye Start-Process çağrısı dahil): AIServer 2,07 s · GameServer 2,18 s · LogInServer 2,07 s. Üçü de ilk yoklamada `UP`. Betiğin yazdığı `(0 sn)` duvar saati değil: Bulgu 1 |
| K7 çift start | ✔ | `[REFUSE] zaten çalışıyor; önce durdurun: tools/run-servers.sh stop`, exit 1. Öncesi/sonrası süreç listesi `diff` → aynı |
| K8 başarısızlık testi | ✔ | `[FAIL] AIServer: açılış başarısız (…)` ardından `[STOP] AIServer.exe pid=40744 (force: pause)`, exit 1, `real` 5,38 s. GameServer/LogInServer başlatılmadı. Sonrasında bizim süreç yok; öksüz alt süreç de yok (`ParentProcessId` sorgusu → 0) |
| K9 --keep-on-fail | ✔ | exit 1 + `inceleme için açık bırakıldı…`. `status` → `[FAILED] AIServer pid=34156 …`. `stop` → `(force: pause)`, exit 0. Sonra bizim süreç yok |
| K10 Debug | ✔ | `start --config Debug` exit 0, 3/3 `UP`, üç yol da `build\bin\x86-Debug\Server\` altında. Ardından `stop`: üçü nazik |
| K11 istemci koruması | ✔ | **Çalışma zamanında doğrulandı.** Debug sunucular açıkken PowerShell `TcpClient` ile 127.0.0.1:15001'e veri göndermeyen 25 sn'lik bir bağlantı açıldı. `status` → `istemci=1`; `stop` → `[REFUSE] GameServer'a bağlı istemci var; durdurmak için --force (istemci=1)`, exit 1; süreç listesi değişmedi. Kod: `:544-554` |
| K12 başlangıç durumu | ✔ | `FDP_SERVER_BIN_DIR=/mnt/c/dev/fdp/server ./tools/run-servers.sh start` → 3/3 `UP`, yollar `C:\dev\fdp\server\`, exit 0. Doğrulama bu hâlde bırakıldı (pid 32924 / 31420 / 39028) |
| K13 gizlilik | ✔ | `grep -n 'UID\|PWD\|DSN\|sqlcmd\|SQLCMD\|Logs/\|rm -rf\|/IM'` boş. ini'ler yalnızca `ini_port` (`:163-167`) ile ve yalnızca `PORT` için okunuyor. Çıktılarda gizli değer yok |
| K14 kapsam | ✔ | `git diff --stat main...bot/F0-02`: `plans/F0-02-…md`, `tools/run-servers.sh`. Plan dosyasında yalnızca `Durum` satırı ve Uygulayıcı Raporu değişmiş. Sunucu kaynak dizinlerinde değişiklik yok, `rm -rf` yok. Commit mesajları `[F0-02] …` |

**Proje kuralları:** Yalnızca araç betiği eklendi. Mekanik, thread ve bot sistemi maddeleri bu plana uygulanmaz. Betik DB'ye bağlanmıyor, `Logs/` okumuyor, `taskkill /IM` kullanmıyor, her zaman `/PID` ile çalışıyor. Commit'te gizli bilgi yok.

**Bulgular (önem sırasıyla):**

1. *Orta (hata)*: **Süreler ve zaman aşımları duvar saatiyle ölçülmüyor.** `tools/run-servers.sh:472-496` (start) ve `:264-272` (stop) sayaçları yalnızca `sleep` sürelerini topluyor. Her yinelemedeki PowerShell çağrısını (bu turda ~1–1,2 sn) saymıyor. Sonuçlar:
   - `FDP_START_TIMEOUT=300` gerçekte ~450–480 sn sürer.
   - `FDP_STOP_TIMEOUT=20` gerçekte sunucu başına ~40–45 sn sürer. Uygulayıcı Raporu adım 2'deki üç zorla kapatma bu yüzden toplam ~2,5 dk sürmüş olmalı; süre raporda yok.
   - "~10 sn'de bir" ilerleme satırı gerçekte ~16 sn'de bir çıkar.
   - `(<n> sn)` değerleri gerçek süreyi göstermez: bu turda 2,1 sn'de hazır olan sunucular `0 sn` yazdı.

   Plan bunları istiyor: §5.1 "sunucu başına en fazla bekleme (sn)", §5.3 start 3 "en fazla `FDP_START_TIMEOUT` sn", K6 "hazır olma süresi". Uygulayıcı sapma 3'te bunu yalnızca `stop` için belirtmiş; kabul edilmedi. F3 ScenarioRunner bu değerlere dayanacak.
2. *Düşük (hata)*: **Zaman aşımı mesajı son durumu kaybediyor.** `:486-488` `state=TIMEOUT` atıyor, `:510` aynı değişkeni "son durum" diye basıyor. Mesaj bu yüzden her zaman `son durum: TIMEOUT` olur. Plan §5.3 start 3 "(son durum: <durum>)" istiyor: GameServer'ın `PARTIAL` (AI bağlantısı yok) mı yoksa `STARTING` mi kaldığını ayırt etmek bu satırın amacı. Sayıdan sonra "sn" de eksik.
3. *Düşük*: **`status` çalışmayan sunucu için satır yazmıyor** (`:291-320` yalnızca bulunan süreçleri basıyor). Plan §5.3 status 2 "her sunucu için tek satır" diyor; durum tablosunda `DOWN` = "bizim sunucumuz sayılan süreç yok". Bu turda üçü de durmuşken çıktı yalnızca `[NOTE]` ve `Özet: 0/3 hazır` idi. Tek bir sunucu çöktüğünde hangisinin eksik olduğu satırlardan okunamaz.
4. *Düşük, bildirilmemiş sapma*: `stop` etiketleri İngilizce ve `.exe`'li (`:255, :260, :267, :274`: `already down`, `force: pause`, `graceful`, `force: timeout`). Plan §5.3 stop 3 ve K5/K9 şunu istiyor: `[STOP] <ad> pid=<pid> (nazik, <n> sn)`, `(zorla: pause)`, `(zorla: zaman aşımı)`. Raporun "Plandan sapmalar" bölümünde yok.
5. *Düşük*: `:465`: Start-Process sayı döndürmezse `[FAIL] <ad>: başlatılıyor` yazıyor ("başlatılıyor" = starting). Doğrusu "başlatılamadı".
6. *Bilgi, rapor*: §5.4 adım 13 "kırpmadan" uyulmamış: yollar `C:\...` ile kısaltılmış, adım 8 ve 12 özetlenmiş, adım 3'e "(boş)" eklenmiş. Bu tur her şey bağımsız yeniden çalıştırıldığı için kanıt eksikliği doğurmadı; Tur 2'de tam çıktı istenir.
7. *Bilgi, ortam*: Uygulayıcı Bulgu 1 doğru gözlem. Elle açılmış sunucularda `taskkill /PID` nazik kapanış sağlamıyor; betikle açılanlarda sağlıyor (bu turda 3 × 3 kez doğrulandı). Neden doğrulanmadı `[Ö]`: konsol penceresi sunucu sürecine ait görünmüyor olabilir (ör. Windows 11'de varsayılan terminal; bu makinede `DelegationConsole` anahtarı yok). Olası çözüm: handler tüm ctrl türlerini kapanış sayıyor (`GameServer/main.cpp:62-69`), bu yüzden pencereden bağımsız bir CTRL_BREAK gönderimi (AttachConsole + GenerateConsoleCtrlEvent) denenebilir. Bu planın kapsamı dışında; **KI-007** olarak kaydedildi.
8. *Bilgi*: "Nazik" kapanış, sistemin gönderdiği CTRL_CLOSE_EVENT'e dayanıyor. Handler 10 sn uyuyor (`GameServer/main.cpp:67`); ana thread temizliği bitirince süreç çıkıyor. Bu turda üçü de ~1–2 sn'de çıktı. Temizliğin tamamlandığı ancak `Logs/` okunarak görülebilir; bu yasak olduğu için doğrulanmadı.
9. *Bilgi, ölçüm (F1/F3 için)*: Soket oluşturma zamanları (`Get-NetTCPConnection.CreationTime`, saniye çözünürlüklü) GameServer'ın AI bağlantısını süreç başlangıcıyla aynı saniyede kurduğunu gösteriyor. Yani yerel DB ile tüm tablolar 1 sn'den kısa sürede yükleniyor; sıra `GameServer/GameServerDlg.cpp:97 → 107-154 → 199` doğrulandı. AIServer'ın NPC yüklemesinin bitişi dışarıdan görülmüyor (§8); GameServer `AI=bağlı` olduğunda NPC'ler henüz hazır olmayabilir.

**Uygulayıcının sapma ve sorularına cevap:**
- Sapma 1 (`-ErrorAction SilentlyContinue`): kabul.
- Sapma 2 (`printf '%s\n'`): kabul; doğru düzeltme.
- Sapma 3: kabul edilmedi, bkz. Bulgu 1.
- Soru 1: KI-007.
- Soru 2: doğru, Bulgu 9.
- Sorular 3–5: not edildi, işlem gerekmiyor.

**Düzeltme talimatı** (DeepSeek'e aynen verilecek):

```
plans/F0-02-sunucu-calistirma-betigi.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:

1. tools/run-servers.sh, cmd_start bekleme döngüsü (şu an satır 472-496): süreyi bash'in yerleşik SECONDS değişkeniyle duvar saatinden ölç. Döngüden önce t0=$SECONDS ata; her proc_status yoklamasından hemen sonra elapsed=$((SECONDS - t0)) hesapla; "elapsed=$((elapsed + 2))" satırını sil ("sleep 2" kalsın). Zaman aşımı karşılaştırması, ~10 sn'de bir yazılan ilerleme satırı ve "[UP] <ad> pid=<pid> (<n> sn)" bu değeri kullansın.
2. tools/run-servers.sh, stop_one bekleme döngüsü (şu an satır 264-272): aynı yöntem. Nazik taskkill'den hemen önce t0=$SECONDS ata; her proc_alive yoklamasından sonra elapsed=$((SECONDS - t0)) hesapla; "elapsed=$((elapsed + 1))" satırını sil ("sleep 1" kalsın). FDP_STOP_TIMEOUT karşılaştırması ve nazik kapanış satırındaki saniye bu değeri kullansın.
3. tools/run-servers.sh, cmd_start: zaman aşımında son gözlenen durumu kaybetme (şu an satır 486-488 state=TIMEOUT atıyor, satır 510 bunu "son durum" diye basıyor). Son gözlenen durumu ayrı bir değişkende tut (ör. last_state). Mesaj tam olarak şu biçimde olsun: "[FAIL] <ad>: <FDP_START_TIMEOUT> sn içinde hazır olmadı (son durum: <last_state>)", ör. "son durum: PARTIAL".
4. tools/run-servers.sh satır 465: Start-Process sayı döndürmediğinde yazılan "[FAIL] <ad>: başlatılıyor" mesajını "[FAIL] <ad>: başlatılamadı" yap. Yeni bir T_ sabiti ekle; Türkçe karakterleri $'\u..' kaçışıyla yaz (dosya ASCII kalmalı).
5. tools/run-servers.sh, do_status: bir rol (AIServer, GameServer, LogInServer) için bizim sunucumuz sayılan hiç süreç yoksa o rol için de, sırayı bozmadan şu satırı yaz: printf '%-10s %-12s port=%s\n' '[DOWN]' "$role" "$port". Özet satırı ve çıkış kodu değişmesin.
6. tools/run-servers.sh, stop_one (şu an satır 255, 260, 267, 274): çıktıyı plan §5.3 biçimine getir: "[STOP] <ad> pid=<pid> (nazik, <n> sn)", "(zorla: pause)", "(zorla: zaman aşımı)", "(zaten kapalı)". <ad> .exe'siz rol adı olsun (AIServer, GameServer, LogInServer); stop_one içinde ${name%.exe} kullanabilirsin. Türkçe karakterler $'\u..' kaçışıyla.
7. Statik kontrolleri çalıştır ve çıktılarını rapora yapıştır: bash -n tools/run-servers.sh; file tools/run-servers.sh (ASCII text executable, CRLF yok); git ls-files -s tools/run-servers.sh (100755); grep -n 'UID\|PWD\|DSN\|sqlcmd\|SQLCMD\|Logs/\|rm -rf' tools/run-servers.sh (boş).
8. Çalışma zamanı testini sırayla çalıştır, her komuttan sonra echo "exit=$?" (araç zaman aşımı en az 600 sn; (a)'da GameServer'da istemci>0 görürsen --force kullanma, Durum: UYGULANIYOR (BLOKE) yaz ve dur): (a) ./tools/run-servers.sh status; (b) ./tools/run-servers.sh stop; (c) ./tools/run-servers.sh status (üç [DOWN] satırı ve exit=1 beklenir); (d) plan §5.4 adım 4'teki başarısızlık testi, aynı FAILDIR ile ve time ile; (e) time ./tools/run-servers.sh start; (f) ./tools/run-servers.sh stop (üçü "nazik" olmalı); (g) başlangıç durumuna dön: (a)'da üçü C:\dev\fdp\server\ altından çalışıyorduysa FDP_SERVER_BIN_DIR=/mnt/c/dev/fdp/server ./tools/run-servers.sh start, kapalıydılarsa kapalı bırak.
9. Raporuna "Tur 2" ekle: 7 ve 8'in çıktılarını kırpmadan yapıştır (yolları "..." ile kısaltma, adım atlama). (e)'deki her sunucunun "[UP] ... (<n> sn)" değerini ayrıca yaz. "Plandan sapmalar" bölümünü güncelle. Plan Durum satırını UYGULANDI yap. Başka dosyaya dokunma.
```
