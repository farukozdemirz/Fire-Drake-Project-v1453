# U2-01: İstemci tablo çözücü (`tools/kotbl.py`) ve 1534 pelerin betiği (`db/012_u2_capes_1534.sql`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
