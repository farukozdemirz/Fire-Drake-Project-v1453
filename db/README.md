# Veritabanı betikleri
Yeni bir `FDP_kn_online` kurulumunda elle yapılan veri düzeltmelerini tekrar
çalıştırılabilir betikler hâline getirir. Betikler yalnızca oyun/kurulum
verisini değiştirir; kişisel veri tablolarına dokunmaz.

## 001 — MAGIC.Etc düzeltmesi (ADR-0003, KI-001)
`MAGIC` içindeki `Etc = 1` satırlar (~1300; potlar, Sprint) görev 1 ister ve
yeni karakterde skill/pot "failed" verir. Betik yalnızca `Etc = 1` satırlarını
`0` yapar; `Etc` 510–523 (usta yetenekleri) korunur. Eski değerler
`dbo.<Target>_ETC_FIX_BACKUP` tablosuna yazılır.

Uygula:
    sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Target=MAGIC -i db/001_magic_etc_fix.sql

Geri al:
    sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Target=MAGIC -i db/001_magic_etc_fix_rollback.sql

İki betik de tekrar çalıştırılabilir. `Target` değişkeni zorunludur (gerçek
kullanımda `MAGIC`); geri alma yalnızca bu betiğin kaydettiği ve hâlâ `0` olan
satırları geri yazar.

## 002 — Bot karakterleri (ADR-0002)
12 bot hesabı/karakteri (6 rol profili × 2 ulus) oluşturur: seviye 80, master
sınıf, 577 stat, 142 skill puanı, S1 (+7) referans ekipman, başlangıç sarf
malzemeleri, `Zone = 71`, arena A konumu. Sunucu **kapalıyken** çalıştırılmalıdır;
yalnızca bot hesap/karakter satırlarına (`USERDATA`, `ACCOUNT_CHAR`,
`WAREHOUSE`) dokunur, kişisel veri tablolarına dokunmaz. Betik tekrar
çalıştırılabilir (önce kendi yazdığı satırları siler, sonra yeniden ekler);
geri alma betiği sahiplik kontrolü yapar ve yalnızca bot satırlarını siler.

Uygula (`Upgrade` zorunlu: 0, 7 veya 8 — zırh ve silah ID'lerinin son hanesi):

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Upgrade=7 -i db/002_bot_characters.sql
```

Geri al:

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -i db/002_bot_characters_rollback.sql
```
