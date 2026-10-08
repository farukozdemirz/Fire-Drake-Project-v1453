# U2-01: İstemci tablo çözücü (`tools/kotbl.py`) ve 1534 pelerin betiği (`db/012_u2_capes_1534.sql`)

| Alan | Değer |
|---|---|
| Durum | KAPANDI |
| Faz | U2 — Sürüm yükseltme 1534, veri (`docs/17` §2 U, ADR-0068 madde 4) |
| Branch | `bot/U2-01` (taban: `main`) |
| Bağımlı olduğu planlar | — (U1-01 KAPANDI) |
| İlgili gereksinim / kabul | T-UPG-03 hazırlığı (uzun pelerin); `docs/reports/u0-1534/` E raporu §1–§2 |
| Tahmini büyüklük | M (3 yeni araç/betik dosyası + 2 SQL + README bölümü) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

1534 istemcisi 224 pelerin tanır (`Data/Cloak.tbl`, 7 kolon); bizim `KNIGHTS_CAPE` tablomuzda 56 satır var. Eksik 168 pelerin — uzun (royal) pelerinler dahil — DB'ye **istemci tablosundaki değerlerle** eklenecek. ALPHA DB'sindeki değerler kullanılmaz: 144 satırda klan puanı fiyatı 0, 143 satırda kademe şartı yanlış (E raporu §2.3).

Bunun için önce genel, yeniden kullanılabilir bir istemci tablo çözücü (`tools/kotbl.py`) yazılır; U2-02 (eşya/NPC) aynı modülü kullanacak.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0068-surum-yukseltme-1534-hedef-istemci-ve-taban.md` (madde 4).
- E raporu (veri farkı): `docs/reports/u0-1534/E-veri-farki.md` §1 (çözme), §2 (pelerin kolon haritası, yeni kimlikler, uzun pelerinler). Analiz betikleri depo dışında, salt referans: `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/e_scripts/tbl.py`, `capecmp.py` (bakılabilir; depo kurallarına göre yeniden yaz).
- Mevcut çözücü: `tools/client-tbl-quests.py` (yalnız `Skill_Magic_Main_us.tbl`; algoritma ve tip tablosu docstring'de).
- Sunucunun pelerin kullanımı: `shared/database/KnightsCapeSet.h` (yüklenen kolonlar: `sCapeIndex, nBuyPrice, byGrade, nBuyLoyalty, byRanking`), `GameServer/NPCHandler.cpp` `CUser::HandleCapeChange` (`m_byFlag < byRanking` → hata −6; `nBuyLoyalty` klan puanı fonundan düşülür), `GameServer/Knights.h:26-41` (`ClanTypeFlag`).
- DB betik kalıbı: `db/README.md`, `db/001_magic_etc_fix.sql` (+ rollback): `sqlcmd` değişkeni `Target`, İngilizce ASCII yorumlar, idempotent, yedek/kayıt tablosu.
- `KNIGHTS_CAPE` kişisel veri içermeyen bir **fiyat/şart tablosudur**; satırlarını okumak serbesttir (CLAUDE.md'deki `KNIGHTS*` yasağı klan ve üye tablolarını kastediyor; `KNIGHTS`, `KNIGHTS_USER`, `KNIGHTS_ALLIANCE`, `KNIGHTS_RATING` satırlarını **okuma**).

## 3. Kapsam

**Var:**
1. `tools/kotbl.py` — standart kütüphane, Python 3, modül + CLI:
   - `decode(data: bytes) -> Table` (kolon tipleri, satırlar); `load(path)`; şifre çözme algoritması `client-tbl-quests.py` ile aynı (anahtar 0x0816, c1 0x6081, c2 0x1608); tipler 1 int8, 2 uint8, 3 int16, 4 uint16, 5 int32, 6 uint32, 7 string (int32 uzunluk + CP949), 8 float32, 9 float64; bilinmeyen tip → açık hata.
   - Satır sonrasında artık bayt varsa (ör. `Quest_Menu_us.tbl` 56 bayt) hata değil uyarı alanı (`trailing_bytes`).
   - CLI: `info <tbl>` (kolon sayısı/tipleri, satır sayısı, artık bayt), `dump <tbl> [--tsv]` (UTF-8'e çevrilmiş metin), `--selftest`.
   - `--selftest`: rastgele olmayan sentetik bir tabloyu **şifreleyen** yardımcıyla üretip `decode` ile geri okur (tüm tipler, CP949 metin dahil); gerçek istemci dosyası gerekmez. Çıkış `selftest OK`.
2. `tools/u2-gen-capes.py` — `kotbl` ile istemci `Cloak.tbl`'ı okur ve `db/012_u2_capes_1534.sql` + `db/012_u2_capes_1534_rollback.sql` üretir:
   - Varsayılan istemci yolu `/mnt/c/dev/fdp1534/client/Knight Online/Data/Cloak.tbl` (`--client` ile değişir).
   - Kolon eşlemesi (E §2.1): c0 → `sCapeIndex`, c2 → `nBuyPrice`, c3 → `nDuration`, c4 → `byGrade`, c5 → `nBuyLoyalty`, c6 → `byRanking`.
   - `strName`: istemci adı ASCII ise o; değilse (Korece) sabit İngilizce ad: `Cape <id>` + desen eki yoksa yalnız `Cape <id>`. Sunucu `strName`'i yüklemez; görünür etkisi yok.
   - Betiğe **yalnız 7 kolonlu tablo** kabul edilir; 5 kolonlu eski tablo verilirse açık hata.
   - `--check`: betikleri yeniden üretip depodakiyle bayt bayt karşılaştırır (0 = aynı).
3. `db/012_u2_capes_1534.sql`: `Target` değişkeniyle hedef tablo (gerçek kullanımda `KNIGHTS_CAPE`); 224 istemci satırının her biri için **kimlik hedefte yoksa** `INSERT`; eklenen kimlikleri `dbo.$(Target)_U2_ADDED (sCapeIndex smallint PRIMARY KEY, dtAdded datetime)` kayıt tablosuna yazar; mevcut satırlara **dokunmaz** (56 satırın `byRanking` 0↔2 farkı bilinçli olarak bırakılır, davranış farkı yok). Sonunda `inserted=<n> already_present=<m>` basar. İkinci çalıştırma `inserted=0`.
4. `db/012_u2_capes_1534_rollback.sql`: yalnız kayıt tablosundaki kimlikleri siler, kayıt tablosunu düşürür; tekrar çalıştırılabilir.
5. `db/README.md`'ye `## 012` bölümü (Türkçe; uygula/geri al komutları, sunucu kapalı olma şartı **yok** — tablo yalnız açılışta yüklenir, etki için sunucu yeniden başlatılmalı).

**Yok:** DB'ye gerçek uygulama (Claude yapar), ALPHA DB okuması (pelerinler için gerekmez), eşya/NPC (U2-02), sunucu kodu.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `tools/kotbl.py` | YENİ |
| `tools/u2-gen-capes.py` | YENİ |
| `db/012_u2_capes_1534.sql`, `db/012_u2_capes_1534_rollback.sql` | YENİ (üretilmiş) |
| `db/README.md` | `## 012` bölümü |

`tools/client-tbl-quests.py` **değişmez** (ileride `kotbl`'a geçirilebilir; bu planın işi değil).

## 5. Uygulama adımları

1. `tools/kotbl.py`'yi yaz; `python3 tools/kotbl.py --selftest`.
2. Gerçek dosyalarla duman testi (salt okuma): `info` ile yeni istemci `Cloak.tbl` (7 kolon, 224 satır), eski istemci `/mnt/c/dev/fdp/Client/Data/Cloak.tbl` (5 kolon, 56 satır), yeni `Skill_Magic_Main_us.tbl` (33 kolon, 1864 satır) ve `Quest_Menu_us.tbl` (480 satır + 56 artık bayt). Çıktıları rapora koy.
3. `tools/u2-gen-capes.py`'yi yaz; betikleri üret; `--check` 0.
4. Üretilen SQL'i gözden geçir: 224 `IF NOT EXISTS … INSERT` (ya da tek `INSERT … SELECT … WHERE NOT EXISTS` + `VALUES` listesi), değerler E §2.2 ile tutarlı: x10–18 → `nBuyLoyalty` 36000, `byRanking` 3; x40–48 → 360000, 8; x60–64 → 1080000/1368000/1728000/2160000/2880000, 8..12; x29–33 → 180000/288000/432000/648000/864000, 3..7; tüm yeni satırlarda `nBuyPrice` 0, `byGrade` 0.
5. **Geçici kopya tabloda test** (canlı `KNIGHTS_CAPE`'e dokunma): `SELECT * INTO dbo.KNIGHTS_CAPE_U201TEST FROM dbo.KNIGHTS_CAPE` (`.\SQLEXPRESS`, `FDP_kn_online`, Windows kimlik doğrulaması) → `-v Target=KNIGHTS_CAPE_U201TEST` ile uygula (`inserted=168 already_present=56`, toplam 224) → tekrar uygula (`inserted=0`) → geri al (56 satır, kayıt tablosu yok) → kopya ile orijinalin satır satır aynı olduğunu `EXCEPT` ile göster (0 satır) → kopya tabloyu düşür. Tüm komut ve çıktıları rapora yaz.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/kotbl.py --selftest` → `selftest OK`, çıkış 0.
- [ ] K2: §5.2 duman testi çıktıları beklenen kolon/satır sayılarıyla eşleşir.
- [ ] K3: `python3 tools/u2-gen-capes.py --check` → 0 (depodaki betik üreticinin çıktısıyla aynı).
- [ ] K4: Geçici kopyada: ilk uygulama `inserted=168 already_present=56`; ikinci `inserted=0`; geri alma sonrası `EXCEPT` 0 satır; kopya ve kayıt tabloları düşürüldü (`SELECT name FROM sys.tables WHERE name LIKE 'KNIGHTS_CAPE_U201TEST%'` boş).
- [ ] K5: Uzun pelerin kontrolü: kopyada `byRanking BETWEEN 8 AND 12` olan satır sayısı **84** (E §2.4), kimlikleri x40–48 ve x60–64.
- [ ] K6: SQL dosyaları ASCII, LF (`.gitattributes` `*.sql eol=lf`), yorumlar İngilizce; Python dosyaları LF.
- [ ] K7: Kapsam: `git diff --stat main...bot/U2-01` yalnız §4 dosyaları.
- [ ] K8: `git status --short` temiz; geçici tablo kalmadı.

## 7. Doğrulama komutları

```bash
python3 tools/kotbl.py --selftest
python3 tools/kotbl.py info "/mnt/c/dev/fdp1534/client/Knight Online/Data/Cloak.tbl"
python3 tools/kotbl.py info /mnt/c/dev/fdp/Client/Data/Cloak.tbl
python3 tools/u2-gen-capes.py --check
SQLCMD="/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_kn_online -b -v Target=KNIGHTS_CAPE_U201TEST -i "$(wslpath -w db/012_u2_capes_1534.sql)"
git diff --stat main...bot/U2-01
git status --short
```

## 8. Kısıtlar ve uyarılar

- İndirilen paketteki hiçbir exe/dll çalıştırılmaz; istemci dosyaları **yalnız okunur**, değiştirilmez.
- Canlı `KNIGHTS_CAPE` tablosuna yazma yok (Claude doğrulamadan sonra uygular). `KNIGHTS`, `KNIGHTS_USER`, `KNIGHTS_ALLIANCE`, `KNIGHTS_RATING` satırları okunmaz.
- Python yalnız standart kütüphane; `python3 -I` ile çalıştırılabilir olmalı.
- Git: `AGENTS.md` §2.8. Commit `[U2-01] ...`.

---

## Uygulayıcı Raporu

### Tur 1

**Durum:** UYGULANDI (K1–K8 ✔). Uygulayıcı: Claude alt ajanı (proje sahibinin bu yükseltme fazı için verdiği talimatla, DeepSeek yerine), önceden açılmış çalışma ağacı `/mnt/c/dev/fdp-u2-01`.

**Branch ve commit'ler:** `bot/U2-01` (taban `main` @ `a516b2d5`)
- `fd981615` [U2-01] Durum UYGULANIYOR
- `f7c52739` [U2-01] tools/kotbl.py: genel istemci .tbl cozucu (info, dump, --selftest)
- `5ec27644` [U2-01] tools/u2-gen-capes.py ve uretilmis db/012 pelerin betikleri (224 istemci satiri, 84 uzun pelerin)
- `5509927d` [U2-01] db/README.md: 012 bolumu (1534 pelerin betigi, uygula/geri al)
- bu rapor + `Durum: UYGULANDI` commit'i

**Değişen dosyalar**
- `tools/kotbl.py` (YENİ): modül + CLI. `decode(data) -> Table`, `load(path)`, `encode(types, rows)` (şifreleyen yardımcı), `Table.types/rows/trailing_bytes/bad_strings`. Şifre ve tipler `client-tbl-quests.py` ile aynı (0x0816/0x6081/0x1608; tip 1–9). Bilinmeyen tip, bozuk kolon/satır sayısı, kesik veri ve geçersiz dizgi uzunluğu açık `TableError`; satır sonrası artık bayt hata değil, `trailing_bytes` alanı + stderr uyarısı. CLI: `info`, `dump [--tsv]` (UTF-8), `--selftest`.
- `tools/u2-gen-capes.py` (YENİ): `kotbl`'ı yol ile yükler (`python3 -I` betik dizinini `sys.path`'e koymadığı için), 7 kolonlu `Cloak.tbl`'ı okur, E §2.1 eşlemesiyle iki betiği üretir; `--check` bayt bayt karşılaştırır (0 aynı, 1 farklı, 2 girdi hatası).
- `db/012_u2_capes_1534.sql`, `db/012_u2_capes_1534_rollback.sql` (YENİ, üretilmiş): `Target` değişkeni zorunlu; tek `INSERT … SELECT … WHERE NOT EXISTS` + 224 satırlık `VALUES` tablo değişkeni; eklenen kimlikler `dbo.$(Target)_U2_ADDED`'e; geri alma yalnız kayıttaki kimlikleri siler ve kayıt tablosunu düşürür.
- `db/README.md`: `## 012` bölümü (Türkçe; uygula/geri al/yeniden üret komutları, sunucu kapalı olma şartı yok, etki için yeniden başlatma).

**Derleme:** C++ değişikliği yok; AGENTS.md §4 gereği yine de `./tools/build.sh Release` çalıştırıldı (28 sn, çıkış 0, `error` 0). Son satırlar:
```
  proj-GameServer.vcxproj -> C:\dev\fdp-u2-01\build\bin\x86-Release\Server\GameServer.exe
  ...
  BotCoreTests.vcxproj -> C:\dev\fdp-u2-01\build\bin\x86-Release\Tests\BotCoreTests.exe
```
(Bu çalışma ağacının sunucuları `[DOWN]`; çalışan sunucular `C:\dev\fdp-edit`'e ait, dokunulmadı.)

**Kabul kriterleri**

- ✔ **K1** `python3 tools/kotbl.py --selftest` → `selftest OK`, çıkış 0 (`python3 -I` ile de aynı). Selftest: şifre gidiş-dönüş + sabit bilinen-cevap vektörü (`encrypt(b"KOTBL\x00\x01\xff")` = `43e5b0b8bc4846d2`; `client-tbl-quests.py`'nin `encrypt`'i aynı sonucu veriyor), tüm tipler (1–9, tam sayı sınır değerleri, boş/ASCII/Korece CP949 dizgi, float32/float64), satırsız tablo, 56 artık bayt, 6 hata durumu, TSV kaçışları. Mutasyon denemesi (geçici kopyada şifre ve vektör bozuldu) `selftest FAILED` ve çıkış 1 verdi.
- ✔ **K2** Duman testi (`python3 -I tools/kotbl.py info …`, salt okuma):
```
Cloak.tbl (1534)          size 6762    columns 7  types 6 7 6 6 2 6 2  rows 224   trailing_bytes 0   bad_strings 0
Cloak.tbl (eski istemci)  size 1740    columns 5  types 6 7 6 6 2      rows 56    trailing_bytes 0   bad_strings 0
Skill_Magic_Main_us.tbl   size 350164  columns 33                      rows 1864  trailing_bytes 0   bad_strings 0
Quest_Menu_us.tbl         size 13598   columns 2  types 6 7            rows 480   trailing_bytes 56  (stderr: warning: 56 trailing bytes after the last row)
```
  Ayrıca `dump --tsv` ile yeni `Cloak.tbl` çıktısı, E raporunun analiz dökümüyle (`scratchpad/k1534/e_out/new/Cloak.tbl.tsv`) başlık hariç bayt bayt aynı (`diff` boş).
- ✔ **K3** `python3 tools/u2-gen-capes.py --check` → `client capes=224 long_capes=84 ascii_names=163 generated_names=61`, `same: db/012_u2_capes_1534.sql`, `same: db/012_u2_capes_1534_rollback.sql`, `check OK`, çıkış 0. Negatif denemeler: betiğe bir boşluk eklenince `DIFFERENT … check FAILED`, çıkış 1 (sonra yeniden üretildi, yedekle `cmp` aynı); eski 5 kolonlu `Cloak.tbl` → `error: Cloak.tbl has 5 columns; only the 7-column 1534 client table is accepted …`, çıkış 2.
  §5.4 gözden geçirme (üretilen `VALUES` satırları ayrıştırılarak): 224 satır/224 ayrı kimlik; eski 56 kimlik + 168 yeni; yeni satırların hepsi `nBuyPrice` 0, `nDuration` 0, `byGrade` 0; x10–18 → 36000/3 (54 satır), x29–33 → 180000/288000/432000/648000/864000, 3..7 (30), x40–48 → 360000/8 (54), x60–64 → 1080000/1368000/1728000/2160000/2880000, 8..12 (30); E §2.2'ye aykırı satır 0.
- ✔ **K4** Geçici kopya (`.\SQLEXPRESS`, `FDP_kn_online`, `-E`). Komutlar: `SELECT * INTO dbo.KNIGHTS_CAPE_U201TEST FROM dbo.KNIGHTS_CAPE` (56 satır, `CHECKSUM_AGG(BINARY_CHECKSUM(*))` 878540737 = orijinal) → `"$SQLCMD" … -b -v Target=KNIGHTS_CAPE_U201TEST -i db/012_u2_capes_1534.sql`:
```
### ilk uygulama
inserted=168 already_present=56
target_rows=224 long_capes=84 logged=168
### orijinal satırlar kopyada değişmedi: (KNIGHTS_CAPE EXCEPT kopya) = 0
### ikinci uygulama
inserted=0 already_present=224
target_rows=224 long_capes=84 logged=168
### geri alma (…_rollback.sql)
removed=168 target_rows=56 log_table=absent
### EXCEPT iki yön
copy_except_orig=0  orig_except_copy=0  rows_copy=56  log_table=absent
### geri alma tekrar
removed=0 target_rows=56 log_table=absent
### kopya düşürüldü; SELECT name FROM sys.tables WHERE name LIKE 'KNIGHTS_CAPE_U201TEST%'
(boş)
```
  Ek: olmayan hedef (`Target=KNIGHTS_CAPE_U201TEST_NOPE`) → `Msg 50000 … target table … not found`, çıkış 1, tablo oluşmadı; `Target` verilmeden → `'Target' scripting variable not defined.`, çıkış 1. Canlı `KNIGHTS_CAPE` öncesi/sonrası: 56 satır, checksum 878540737 (değişmedi); `KNIGHTS_CAPE_U2_ADDED` yok.
- ✔ **K5** İlk uygulamadan sonra kopyada `byRanking BETWEEN 8 AND 12` = **84**; kimlikler `40–48, 60–64, 140–148, 160–164, 240–248, 260–264, 340–348, 360–364, 440–448, 460–464, 540–548, 560–564`; x40–48/x60–64 dışında kalan 0. `byRanking` dağılımı: 0→56 (eski satırlar), 3→60, 4..7→6'şar, 8→60, 9..12→6'şar.
- ✔ **K6** `file`: iki SQL dosyası `ASCII text`, CR 0, ASCII dışı bayt 0; `git check-attr eol` → `lf`. Yorumlar İngilizce. `tools/kotbl.py`, `tools/u2-gen-capes.py`: `ASCII text`, CR 0 (Korece desen adları `\uXXXX` kaçışıyla).
- ✔ **K7** `git diff --stat main...bot/U2-01`: `db/012_u2_capes_1534.sql`, `db/012_u2_capes_1534_rollback.sql`, `db/README.md`, plan dosyası (yalnız `Durum` + bu rapor), `tools/kotbl.py`, `tools/u2-gen-capes.py`. `tools/client-tbl-quests.py` değişmedi.
- ✔ **K8** `git status --short` boş (bu commit'ten sonra da kontrol edildi); geçici tablo kalmadı (K4).

**Plandan sapmalar / yorumlar**
1. `strName` kuralı (plan metni "`Cape <id>` + desen eki yoksa yalnız `Cape <id>`" belirsiz): ASCII ad aynen; E §2.2'deki beş Korece **desen** adı (x29–33) → `Cape <id> iron bars|cross|checkered|symmetric|double eagle`; diğer Korece adlar (x60–64 `왕실N`, 0 `기본망토`) → `Cape <id>`. En uzun ad 21 karakter (char(30)). Sunucu `strName`'i yüklemediği için davranış etkisi yok.
2. Planda olmayan ek denetimler (üretici hata verir): artık bayt, geçersiz CP949, yinelenen kimlik, SQL tip aralığı dışı değer, `byRanking > 12` (`ClanTypeRoyal1`), 30 karakteri aşan ad.
3. Betik `inserted=… already_present=…` satırından sonra ikinci bir doğrulama satırı basar (`target_rows=… long_capes=… logged=…`); geri alma `removed=… target_rows=… log_table=…` basar.
4. Kayıt tablosu: `dtAdded datetime NOT NULL DEFAULT GETDATE()` (planda yalnız `dtAdded datetime`).
5. Geri alma, kayıt tablosu yoksa hata vermez (`removed=0`): plan "tekrar çalıştırılabilir" istediği için `001` kalıbındaki `RAISERROR`'dan ayrılır. Hedef tablo yoksa iki betik de `RAISERROR` verir.
6. Üretilen betik başlığına kaynak dosyanın boyutu ve SHA-256'sı yazılır (`3808b568…f94f`); mutlak yol yazılmaz.
7. Test sorgusunda `STRING_AGG` bu sunucuda çalışmadı (SQL Server 14.0, veritabanı uyumluluk düzeyi 100); `FOR XML PATH` kullanıldı. Betikler bu özelliği kullanmaz.

**Kişisel veri / güvenlik:** Yalnız `KNIGHTS_CAPE` (fiyat/şart tablosu) ve kopyası okundu; `KNIGHTS`, `KNIGHTS_USER`, `KNIGHTS_ALLIANCE`, `KNIGHTS_RATING` ve diğer kişisel veri tabloları okunmadı. İstemci dosyaları yalnız `rb` ile okundu; istemci klasöründen hiçbir şey çalıştırılmadı. Parola dosyası okunmadı. Canlı `KNIGHTS_CAPE`'e yazılmadı.

**Açık sorular**
1. `strName` yorumu (sapma 1) planlayıcının kastettiğiyle aynı mı? Farklıysa yalnız `cape_name()` ve yeniden üretim gerekir.
2. Gerçek `KNIGHTS_CAPE`'e uygulama (plan §3 "Yok": Claude yapar) bekliyor; komut `db/README.md` §012'de.

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-08

**Hüküm: DOĞRULANDI.** Kanıt: kendi koşum.

| K | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `python3 -I tools/kotbl.py --selftest` → `selftest OK` |
| K2 | ✔ | Uygulayıcı çıktıları (7×224, 5×56, 33×1864, 480 + 56 artık bayt) |
| K3 | ✔ | `python3 -I tools/u2-gen-capes.py --check` → `client capes=224 long_capes=84 …`, `check OK`, rc 0 |
| K4 | ✔ | Kendi kopyam `KNIGHTS_CAPE_CLDTEST`: uygula `inserted=168 already_present=56`, `target_rows=224 long_capes=84 logged=168`; tekrar `inserted=0`; geri al `removed=168 target_rows=56 log_table=absent`; `EXCEPT` iki yön 0; kopya düşürüldü; canlı tablo 56 satır |
| K5 | ✔ | Örnek: 110 → 36000/3; 140 → 360000/8; 160 → 1080000/8; 164 → 2880000/12; 233 → 864000/7; 560/564 royal |
| K6 | ✔ | Uygulayıcı `file` çıktıları; SQL başlığı kaynak SHA-256 içerir |
| K7 | ✔ | `git diff --stat main...bot/U2-01`: §4 dosyaları + plan |
| K8 | ✔ | temiz |

Sapmalar kabul (strName okuması, ek girdi denetimleri, rollback tekrar çalıştırılabilir). Canlı `KNIGHTS_CAPE` uygulaması Claude tarafından birleştirmeden sonra, DB yedeği alınarak yapılır.
