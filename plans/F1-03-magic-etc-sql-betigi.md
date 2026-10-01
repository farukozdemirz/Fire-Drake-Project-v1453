# F1-03: `MAGIC.Etc = 1` düzeltmesi için kalıcı, geri alınabilir SQL betiği

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F1 — Veri ve mekanik doğrulama (`docs/17` §2) |
| Branch | `bot/F1-03` (taban: `main`) |
| Bağımlı olduğu planlar | — (F0 KABUL_EDILDI) |
| İlgili gereksinim / kabul | ADR-0003 (K-4), KI-001, T-DATA-06 (`docs/15` §4.1) |
| Tahmini büyüklük | S (3 yeni dosya, kod derlenmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

`MAGIC` tablosundaki `Etc = 1` değerlerini `0` yapan düzeltmeyi (şu an yalnızca proje sahibinin yerel veritabanında elle uygulanmış durumda) depoda duran, **tekrar çalıştırılabilir ve geri alınabilir** SQL betiği hâline getirmek. Yeni bir kuruluma (yeni DB geri yüklemesi) betik aynen uygulanabilsin, istenirse geri alınabilsin.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0003-magic-etc-duzeltmesi.md`: karar: düzeltme depoda geri alma adımı olan SQL betiği olarak tutulur, **`Etc` 510–523 satırlarına dokunulmaz**.
- `docs/KNOWN_ISSUES.md` KI-001: `Etc = 1` satırları görev 1 ister, yeni karakterde skill/pot 'failed' verir.
- Kod (Release derlemesinde): `GameServer/MagicInstance.cpp:267-274`: `pSkill->sEtc != 0 && !CheckExistEvent(pSkill->sEtc, 2)` → `SkillUseFail`. (`#if !defined(DEBUG)` içinde, yani yalnızca Release.) Bu plan **kodu değiştirmez**.
- Yerel veritabanı gerçeği (Claude'un 2026-10-02'de yalnızca `SELECT` ile ölçtüğü değerler):
  - `FDP_kn_online.dbo.MAGIC`: 1839 satır, `MagicNum` benzersiz (ama anahtar/dizin yok), tetikleyici yok.
  - Düzeltme öncesi dağılım: `Etc=0`: 461 satır, **`Etc=1`: 1306 satır**, `Etc` 510–523: 72 satır. Şu an `Etc=1` satırı **0**, 1306 satırın tamamı `0` yapılmış.
  - Elle alınmış tam yedek `MAGIC_BAK_etc` var (düzeltme öncesi tüm satırlar). Bu planın betikleri **bu tabloyu kullanmaz** ve silmez; yalnızca Claude doğrulamada test verisi olarak okur.
- `AGENTS.md` §2.7: veritabanına bağlanma. Bu planda **veritabanına hiç bağlanılmaz**: betikler yazılır, çalıştırılmaz.

## 3. Kapsam

**Yapılacaklar**

- `db/001_magic_etc_fix.sql`: düzeltmeyi uygulayan, **tekrar çalıştırılabilir (idempotent)** betik.
- `db/001_magic_etc_fix_rollback.sql`: düzeltmeyi geri alan betik.
- `db/README.md`: iki betiğin ne yaptığı, nasıl çalıştırılacağı (kısa).

**Kapsam dışı (yapılmayacak)**

- Betikleri çalıştırmak, `sqlcmd`/ODBC ile veritabanına bağlanmak (DeepSeek için). Çalıştırma ve doğrulama Claude'dadır.
- `Etc` 510–523 satırlarına veya `Etc ≠ 1` satırlarına dokunmak.
- `MAGIC` dışındaki tablolar, başka sütunlar, tablo yapısı değişikliği (`MAGIC`'e sütun/dizin ekleme yok).
- `MAGIC_BAK_etc` tablosunu kullanmak, değiştirmek, silmek.
- `GameServer/`, `AIServer/`, `shared/`, `docs/**`, `tools/**`, `.gitattributes` değişikliği.
- Karakter kurulum betiği (ADR-0002) → ayrı plan (F1-04).
- Kurulum notuna (`docs/02`) çalıştırma satırı eklemek (Claude yapar, plan sonrası).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `db/001_magic_etc_fix.sql` | yeni | ASCII, LF |
| `db/001_magic_etc_fix_rollback.sql` | yeni | ASCII, LF |
| `db/README.md` | yeni | UTF-8 (BOM'suz), LF; Türkçe yazılabilir |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

Betikler `sqlcmd` ile çalıştırılacak şekilde yazılır; hedef tablo adı **sqlcmd değişkeni** `Target` ile gelir (gerçek kullanımda `MAGIC`; Claude testte geçici bir kopya tablo verecek). Değişken verilmezse `sqlcmd` hata verir; bu istenen güvenlik davranışıdır, varsayılan değer **koyma**. Tüm yorumlar ve metinler **İngilizce ve ASCII**.

1. **Önce oku:** `AGENTS.md` kodlama kuralları, `docs/adr/ADR-0003-magic-etc-duzeltmesi.md`. Bu klasörde (`db/`) henüz dosya yok; klasörü sen oluştur.

2. **`db/001_magic_etc_fix.sql`** şu yapıda olsun (imzalar ve davranış bağlayıcı, ifadeleri sen yaz):

   - Üst yorum: ne yaptığı, kullanım satırı: `sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Target=MAGIC -i db/001_magic_etc_fix.sql`, ADR-0003/KI-001 referansı, "rows with Etc 510..523 are never touched".
   - `SET NOCOUNT ON; SET XACT_ABORT ON;` ardından `GO`.
   - **Yedek tablosu** (yoksa oluştur): `dbo.$(Target)_ETC_FIX_BACKUP (MagicNum int NOT NULL PRIMARY KEY, OldEtc smallint NOT NULL, FixedAt datetime NOT NULL DEFAULT GETDATE())`. `IF OBJECT_ID(N'dbo.$(Target)_ETC_FIX_BACKUP', N'U') IS NULL CREATE TABLE ...` ardından `GO` (tablonun sonraki toplu iş yürütmesinde görünmesi için).
   - Tek bir **işlem (transaction)** içinde:
     1. `Etc = 1` olup yedekte henüz **olmayan** satırların `(MagicNum, Etc)` değerlerini yedeğe ekle (`INSERT ... SELECT ... WHERE Etc = 1 AND NOT EXISTS (...)`).
     2. `UPDATE dbo.$(Target) SET Etc = 0 WHERE Etc = 1;`
   - Ardından tek sonuç satırı (doğrulama sorgusu, başlıklarıyla): `etc1_remaining` (beklenen 0), `etc_510_523` (düzeltme öncesi ve sonrası aynı kalmalı), `backup_rows`, `total_rows`.
   - `WHERE Etc = 1` dışında **hiçbir** satır güncellenmez. `DELETE`, `TRUNCATE`, `DROP` yok (yalnızca `CREATE TABLE`, `INSERT`, `UPDATE`).
   - Betik ikinci kez çalıştırıldığında hiçbir şeyi bozmaz: `Etc = 1` satırı kalmadığı için yedeğe ekleme ve güncelleme 0 satır etkiler, yedek aynı kalır.

3. **`db/001_magic_etc_fix_rollback.sql`**:

   - Üst yorum: kullanım satırı (`-v Target=MAGIC -i db/001_magic_etc_fix_rollback.sql`), "restores only rows saved by 001_magic_etc_fix.sql".
   - Yedek tablo yoksa `RAISERROR(N'backup table dbo.$(Target)_ETC_FIX_BACKUP not found', 16, 1)` ile hata ver (`sqlcmd -b` bunu hata olarak sayar).
   - Tek işlem içinde: `UPDATE m SET m.Etc = b.OldEtc FROM dbo.$(Target) m JOIN dbo.$(Target)_ETC_FIX_BACKUP b ON b.MagicNum = m.MagicNum WHERE m.Etc = 0;` (yalnızca hâlâ `0` olan, yani düzeltilmiş satırları geri alır; sonradan elle değiştirilmiş satıra dokunmaz). Sonra geri alınan satırların yedek kayıtlarını sil: `DELETE b FROM ... b JOIN ... m ON m.MagicNum = b.MagicNum WHERE m.Etc = b.OldEtc;`.
   - Ardından doğrulama satırı: `etc1_after` (geri alınan sayı), `backup_rows` (kalan, normalde 0).
   - Yedek **tablonun kendisi** silinmez (yalnızca satırları).

4. **`db/README.md`** (kısa, Türkçe): ne için olduğu (ADR-0003/KI-001), iki betiğin adı ve çalıştırma satırı (yukarıdaki), "betikler tekrar çalıştırılabilir", "geri almak için rollback betiği", "Target değişkeni zorunlu (gerçek kullanımda MAGIC)", "bu klasördeki betikler yalnızca oyun/kurulum verisini değiştirir, kişisel veri tablolarına dokunmaz". 10–20 satır.

5. **Sözdizimi kontrolü (veritabanı olmadan):** Veritabanına bağlanamayacağın için şunları dosyalar üzerinde kontrol et ve rapora yapıştır: her `GO` satır başında tek başına; her `BEGIN TRANSACTION` bir `COMMIT TRANSACTION` ile eşleşiyor; `$(Target)` yalnızca belirtilen yerlerde ve her kullanımda tam yazılmış; yasak ifadeler yok (`grep -n -i -E 'DROP|TRUNCATE|DELETE' db/001_magic_etc_fix.sql` boş; rollback'te yalnızca bir `DELETE`, yedek tablo üzerinde).

6. `git add` ile yalnızca üç dosyayı ekle; commit mesajı `[F1-03] ...`. Raporu yaz.

## 6. Kabul kriterleri

- [ ] K1: `db/001_magic_etc_fix.sql`, `db/001_magic_etc_fix_rollback.sql`, `db/README.md` var; ilk ikisi ASCII ve LF (`file` çıktısı rapora).
- [ ] K2: Düzeltme betiğinde `UPDATE` yalnızca `WHERE Etc = 1` ile ve tek bir yerde; `DELETE/TRUNCATE/DROP` yok; `Etc` 510–523 aralığından söz eden tek yer yorum ve doğrulama sorgusu (`grep -n` çıktısı rapora).
- [ ] K3: Hedef tablo adı yalnızca `$(Target)` ile verilmiş (`grep -n 'MAGIC' db/*.sql` çıktısında `dbo.MAGIC` gibi sabit tablo adı yok; yalnızca yorum/kullanım satırlarında `Target=MAGIC` geçer); `GO` satırları ve işlem eşleşmesi §5.4'teki gibi.
- [ ] K4: Rollback yalnızca yedekteki satırları ve yalnızca `m.Etc = 0` olanları geri yazıyor; yedek tablo yoksa `RAISERROR` ile hata veriyor (ilgili satır numaraları rapora).
- [ ] K5: Kapsam: `git diff --stat main...bot/F1-03` yalnızca `db/*` (3 dosya) ve bu plan dosyası; `git status --short` boş.
- [ ] K6 (Claude doğrular, DeepSeek yapmaz): geçici kopya tabloda uygula → idempotent → geri al; sonuç aşağıdaki §7'deki beklenen sayılarla eşleşir.

## 7. Doğrulama komutları

DeepSeek için:

```bash
file db/001_magic_etc_fix.sql db/001_magic_etc_fix_rollback.sql db/README.md
grep -n -i -E 'DROP|TRUNCATE|DELETE|UPDATE' db/*.sql
grep -n 'GO$' db/*.sql
git diff --stat main...bot/F1-03
git status --short
```

Claude doğrulaması (referans, DeepSeek çalıştırmaz): `MAGIC_BAK_etc`'den bir test kopyası oluşturulur (`SELECT * INTO dbo.MAGIC_F103TEST FROM dbo.MAGIC_BAK_etc`), gerçek `MAGIC` tablosuna dokunulmadan:

| Adım | Beklenen |
|---|---|
| Kopya oluştur | `Etc=1`: 1306, `Etc` 510–523: 72, toplam 1839 |
| `001_magic_etc_fix.sql -v Target=MAGIC_F103TEST` | `etc1_remaining=0`, `etc_510_523=72`, `backup_rows=1306`, `total_rows=1839` |
| Aynı betik ikinci kez | Aynı sayılar, hata yok (idempotent) |
| `001_magic_etc_fix_rollback.sql` | `Etc=1` tekrar 1306; kopya `MAGIC_BAK_etc` ile satır satır aynı (`EXCEPT` sorgusu 0 satır); `backup_rows=0` |
| Yedek tablosu yokken rollback | Hata (sıfırdan farklı çıkış kodu) |
| Test tabloları silinir | Gerçek `MAGIC` (Etc=1 sayısı 0, 1839 satır) değişmemiş |

## 8. Kısıtlar ve uyarılar

- **Veritabanına bağlanma yok** (`AGENTS.md` §2.7): `sqlcmd`, ODBC, `.fdp_sql_password` dosyası, hiçbirine dokunma. Betikleri yalnızca yaz.
- Veri değişikliği `[MECH]`-veri değişikliğidir ve ADR-0003 ile onaylıdır; kapsamı büyütme.
- `Etc` 510–523 satırlarına dokunulmaz (Q-04 sonrası değerlendirilecek skill'ler).
- Betik dosyaları ASCII ve **İngilizce** olmalı (T-SQL yorumları dahil); Türkçe yalnızca `db/README.md`'de.
- `GO` satırlarında yorum veya noktalı virgül bırakma; sqlcmd `GO` sonrası metni sayı olarak yorumlar.
- İzin: `opencode.json` `db/` yazmasına engel koymuyor; sorun çıkarsa dur ve raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: —
- Branch / commit'ler: —
- Değişen dosyalar ve neden: —
- Kabul kriterleri öz-değerlendirme: —
- Plandan sapmalar ve gerekçeleri: —
- Açık sorular: —

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
