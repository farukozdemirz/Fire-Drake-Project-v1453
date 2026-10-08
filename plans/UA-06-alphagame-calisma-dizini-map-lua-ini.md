# UA-06: AlphaGame çalışma dizini (Map, Quests, ini, ODBC)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | UA — Sürüm yükseltme tabanı AlphaGame 1534 (ADR-0069 madde 5, 6) |
| Branch | `bot/UA-06` (taban: `yukseltme/alpha`) |
| Bağımlı olduğu planlar | — (UA-01 ve UA-05 ile paralel; sunucu bu planda başlatılmaz) |
| İlgili gereksinim / kabul | ADR-0069 madde 5–6; `docs/reports/u0-1534/B-harita-gorev-istemci.md` §1–§4 |
| Tahmini büyüklük | S |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

AlphaGame tabanlı sunucunun çalışacağı yeni çalışma dizinini hazırlamak: `C:\dev\fdpalpha\server` (WSL: `/mnt/c/dev/fdpalpha/server`). Mevcut çalışma dizinleri (`C:\dev\fdp`, `C:\dev\fdp1534`, `C:\dev\fdp1534-smoke`) değişmez.

## 2. Bağlam (okunması zorunlu)

- ADR-0069 madde 6 (haritalar): AlphaGame `Map` seti; zone 21 için `moradon_1534.smd` (kaynak `/mnt/c/dev/fdp1534/server/Map/moradon_1534.smd`, md5 `cfbdc4051edc042d04049ad2a3bee6c1`); zone 71 için AlphaGame `freezone_b.smd` (proje sahibi kararı, kabul edilmiş risk).
- B §4: AlphaGame `Quests/` 601 dosya (599 `.lua` + 2 uzantısız).
- Kaynak (salt okunur, indirilmiş veri): `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/Server-Files/`.
- Mevcut 1534 çalışma dizini ini'leri (`/mnt/c/dev/fdp1534/server/*.ini`) ve `tools/run-servers.sh` (`FDP_RUNTIME_DIR` kullanımı, beklenen dosyalar).

## 3. Kapsam

**Var:**
1. **Kurulum aracı** `tools/ua-runtime-setup.sh` (tekrar çalıştırılabilir, `--check` md5 manifest doğrulaması):
   - `Map/`: AlphaGame `Server-Files/Map/` altındaki yalnız harita veri dosyaları (`*.smd`, `*.aievt` ve sunucunun okuduğu başka veri uzantıları; hangi uzantıların okunduğunu AlphaGame kaynağından göster). Çalıştırılabilir ya da arşiv dosyaları (`*.exe`, `*.rar`, `*.zip`, `*.dll`) **kopyalanmaz**. Ek olarak `moradon_1534.smd`.
   - `Quests/`: AlphaGame `Quests/` dosyaları bayt bayt.
   - `Logs/` boş klasör. Paketin `Logs/` içeriği **kopyalanmaz**.
   - ini: `GameServer.ini`, `AIServer.ini`, `LogInServer.ini` AlphaGame şablonlarından; ODBC DSN'leri `KO_ALPHA_GAME` / `KO_ALPHA_MAIN`; içine hiçbir parola ya da kullanıcı adı yazılmaz (Windows kimlik doğrulaması; AlphaGame kodu UID/PWD istiyorsa boş bırakılır ve rapora not edilir); portlar standart (15001, 15100, 10020). `[BOT]` bölümü bizim 1534 ini'sinden `ENABLED=0` ile.
2. **ODBC kullanıcı DSN'leri** `KO_ALPHA_GAME` ve `KO_ALPHA_MAIN`: sürücü "SQL Server", sunucu `.\SQL2019`, veritabanı `FDP_alpha_game` (UA-05 kurar; DSN DB'den önce tanımlanabilir), `KO_1534_*` ile aynı yapı. Kayıt anahtarı yazımında tırnaklamaya dikkat (geçmişte `ODBC.INI$n` hatası oldu); sonuç `Get-ItemProperty` ile doğrulanır.
3. Zone eşlemesi raporu: AlphaGame `ZONE_INFO`'nun (referans `FDP_alpha1534`, yalnız `ZONE_INFO` okunur) her satırındaki harita dosyası `Map/`'te var mı; eksikler listelenir.

**Yok:** sunucu başlatma; DB değişikliği; kod; depoya Map/Quests dosyası eklemek (yalnız araç ve manifest depoda).

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `tools/ua-runtime-setup.sh` | yeni |
| `tools/ua-runtime-manifest.txt` | yeni (dosya adı, boyut, md5; ini'ler hariç) |
| Depo dışı: `/mnt/c/dev/fdpalpha/server/**`, HKCU ODBC `KO_ALPHA_GAME`, `KO_ALPHA_MAIN` | yeni |

## 5. Uygulama adımları

1. AlphaGame kaynağında harita/ini okuma yollarını göster (dosya:satır).
2. Aracı yaz, çalıştır, `--check` çalıştır.
3. DSN'leri kur ve doğrula.

## 6. Kabul kriterleri

- [ ] K1: `--check` 0 fark; `Map/`'te çalıştırılabilir ya da arşiv dosyası yok; `Logs/` boş.
- [ ] K2: `moradon_1534.smd` md5 `cfbdc4051edc042d04049ad2a3bee6c1`.
- [ ] K3: Üç ini'de parola/kullanıcı adı değeri yok (`grep -i -E "PWD=.+|UID=.+"` boş ya da raporda gerekçeli); DSN adları `KO_ALPHA_*`.
- [ ] K4: İki DSN `Get-ItemProperty` ile doğru (`Server .\SQL2019`, `Database FDP_alpha_game`); başka DSN değişmedi.
- [ ] K5: Zone → harita eşleme tablosu, eksik dosyalar listesi.
- [ ] K6: `git diff --stat` yalnız §4; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
tools/ua-runtime-setup.sh --check
md5sum /mnt/c/dev/fdpalpha/server/Map/moradon_1534.smd
grep -i -E "PWD=.+|UID=.+" /mnt/c/dev/fdpalpha/server/*.ini
powershell.exe -NoProfile -Command "Get-ItemProperty 'HKCU:\SOFTWARE\ODBC\ODBC.INI\KO_ALPHA_GAME'"
git status --short
```

## 8. Kısıtlar ve uyarılar

- İndirilen paketteki hiçbir dosya çalıştırılmaz; araç kaynak yolunu argüman alır.
- Diğer çalışma dizinlerine ve diğer DSN'lere dokunulmaz.
- Git: `AGENTS.md` §2.8; commit `[UA-06] ...`; push yok.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
