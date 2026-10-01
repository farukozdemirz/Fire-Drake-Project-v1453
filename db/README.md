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
