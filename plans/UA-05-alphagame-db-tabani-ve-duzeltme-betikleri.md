# UA-05: AlphaGame DB tabanı (`.\SQL2019` → `FDP_alpha_game`) ve düzeltme betikleri

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | UA — Sürüm yükseltme tabanı AlphaGame 1534 (ADR-0069 madde 4) |
| Branch | `bot/UA-05` (taban: `yukseltme/alpha`) |
| Bağımlı olduğu planlar | — (kod planlarından bağımsız; UA-01 ile paralel) |
| İlgili gereksinim / kabul | ADR-0069 madde 4; `docs/reports/u0-1534/C-db-semasi.md` §2.4, §3.5, §5; `E-veri-farki.md` §2, §3.4 |
| Tahmini büyüklük | M |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

AlphaGame'in veritabanını oyun sunucusunun tabanı olarak ayrı bir örnekte kurmak ve bilinen veri kusurlarını eklemeli, geri alınabilir betiklerle düzeltmek. Referans kopya `FDP_alpha1534` **değiştirilmez**; oyun için yeni `FDP_alpha_game` kurulur.

## 2. Bağlam (okunması zorunlu)

- ADR-0069; C §2.4 (AlphaGame kodu ↔ DB prosedür uyumsuzlukları), §3.5, §5 (envanter: istemci ve DB sütunları 73 yuva = 584/292 bayt; UA-03 sunucuyu 73 yuvaya çevirecek), §6; E §2 (pelerin: istemci `Cloak.tbl` doğru, AlphaGame'de 144 satır yanlış), §3.4 (`ItemClass` kuralı, bizim satırlarda %99,5 tutar).
- Mevcut araçlar: `tools/kotbl.py` (istemci tablo çözücü), `tools/u2-gen-capes.py`, `tools/u2-gen-alpha.py` (ItemClass türetme mantığı burada), `db/012`–`db/015` (betik kalıbı: `-v Target=`, kayıt tablosu, idempotent, `_rollback.sql`).
- Yedek dosyası (salt okunur, indirilmiş veri): `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/DB/KN_online.bak` (SQL Server 2019 biçimi).
- SQLCMD: `"/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE" -S '.\SQL2019' -E`.

## 3. Kapsam

**Var:**
1. **Kurulum:** `KN_online.bak` → `.\SQL2019` üzerinde `FDP_alpha_game` (`RESTORE ... WITH MOVE`, örneğin varsayılan veri klasörüne yeni dosya adlarıyla). Komutlar `tools/ua-db-restore.sh` içinde, tekrar çalıştırılınca var olan DB'ye dokunmadan durur (`--force` olmadan).
2. **`db/020_ua_itemclass.sql`** (+ `_rollback.sql`, üreteç `tools/ua-gen-itemclass.py`, `--check`): `ITEM.ItemClass` NULL olan satırlara E §3.4 kuralı. Önceki değerler kayıt tablosunda (`ITEM_UA_ITEMCLASS_BACKUP`). Kuralın bizim DB'deki (`FDP_kn_online` `ITEM`, yalnız `Num, ItemClass` okunur) tutma oranı rapora.
3. **`db/021_ua_capes.sql`** (+ rollback): `KNIGHTS_CAPE`'in istemci `Cloak.tbl` ile farklı sütunları istemci değerine (E §2.1 sütun eşlemesi). Önceki satırlar yedek tabloda.
4. **`db/022_ua_zone21.sql`** (+ rollback): zone 21'in harita dosyası `moradon_1534.smd` (U3-01 SMD'si). `START_POSITION`/`K_OBJECTPOS` zone 21'i `db/015` değerleriyle karşılaştır; yalnız farklar yazılır, fark yoksa raporda "aynı".
5. **`db/023_ua_procs.sql`** (+ rollback): C §2.4'teki, AlphaGame kodunun çağırdığı ama DB'de imzası uymayan ya da olmayan prosedürler. Her biri için AlphaGame kaynağındaki çağrı (`/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/1-Game Source/GameServer/DBAgent.cpp` satırı) ile DB tanımı yan yana; düzeltme kodun çağrısına uyar. Anlamı belirsiz olan (ör. kral sistemi gövde mantığı) düzeltilmez, "Açık noktalar"a yazılır. Prosedür eski gövdeleri yedek tabloda (rollback geri yükler).
6. **Giriş prosedürü incelemesi** (salt okuma, yalnız tanım): hesap açan/otomatik kayıt yapan giriş prosedürleri var mı (A §6 LOW). Bulgu raporda; değişiklik yok.

**Yok:** kişisel tabloların satırlarını okumak (CLAUDE.md listesi: TB_USER, ACCOUNT_CHAR, USERDATA satırları, USER_*, WAREHOUSE*, MAIL_*, FRIEND_LIST, PUS_*, _SN_*, WEB_*, CURRENTUSER, KNIGHTS*, KING_* — yalnız şema ve satır sayısı serbest); bot hesapları ve satırları (UA-05b, UA-04'ten sonra); bowl doğuş düzenlemesi (UA-05b); `MAGIC*` değişikliği (ADR-0069: AlphaGame değerleri esas); `FDP_alpha1534`, `FDP_kn_online`, `FDP_kn1534`, `FDP_smoke1534` üzerinde herhangi bir yazma.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `tools/ua-db-restore.sh`, `tools/ua-gen-itemclass.py` | yeni |
| `db/020_ua_itemclass.sql`, `db/021_ua_capes.sql`, `db/022_ua_zone21.sql`, `db/023_ua_procs.sql` ve `_rollback.sql` eşleri | yeni |
| `db/README.md` | yalnız yeni satırlar |

## 5. Uygulama adımları

1. Kurulum; `SELECT name, compatibility_level FROM sys.databases` ve tablo/prosedür sayıları rapora.
2. Betikleri yaz; her birini `FDP_alpha_game` üzerinde: uygula → tekrar uygula (değişiklik 0) → geri al → yeniden uygula. Etkilenen tablolar için `CHECKSUM_AGG(BINARY_CHECKSUM(*))` önce/sonra/geri alma sonrası değerleri rapora.
3. Üreteçlerin `--check` çıktıları rapora.

## 6. Kabul kriterleri

- [ ] K1: `FDP_alpha_game` `.\SQL2019`'da ONLINE; `FDP_alpha1534` dokunulmadı (oluşturma/değişiklik tarihi raporda).
- [ ] K2: Dört betik idempotent; rollback sonrası sağlama toplamları kurulum anındakiyle aynı.
- [ ] K3: `ITEM.ItemClass IS NULL` sayısı betik sonrası raporda (hedef 0 ya da kuralın kapsamadığı satırlar gerekçeli liste).
- [ ] K4: `KNIGHTS_CAPE` ↔ `Cloak.tbl` farkı betik sonrası 0 (üreteç `--check`).
- [ ] K5: C §2.4'ün her satırı için durum (düzeltildi / açık + gerekçe).
- [ ] K6: Kişisel tablolardan satır okunmadığı (kullanılan sorguların listesi raporda); `git diff --stat` yalnız §4; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
SQLCMD="/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
"$SQLCMD" -S '.\SQL2019' -E -Q "SELECT name, state_desc, create_date FROM sys.databases"
python3 -I tools/ua-gen-itemclass.py --check
git diff --stat yukseltme/alpha...bot/UA-05
git status --short
```

## 8. Kısıtlar ve uyarılar

- Yedek dosyası indirilmiş veridir; yalnız `RESTORE` ile okunur. Paketteki başka hiçbir dosya çalıştırılmaz.
- Sunucu başlatılmaz. Kimlik bilgisi (parola) hiçbir dosyaya ve rapora yazılmaz.
- Git: `AGENTS.md` §2.8; commit `[UA-05] ...`; push yok.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
