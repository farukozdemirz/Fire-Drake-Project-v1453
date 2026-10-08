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
`Hp`/`Mp` bilerek yüksek yazılır (32000); sunucu girişte gerçek maksimuma kırpar.

Uygula (`Upgrade` zorunlu: 0, 7 veya 8 — zırh ve silah ID'lerinin son hanesi):

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Upgrade=7 -i db/002_bot_characters.sql
```

Geri al:

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -i db/002_bot_characters_rollback.sql
```

## 003 — Bot quest (skill kilitleri) (ADR-0018 Ek 3, KI-018)

Bot satırlarındaki (`db/002`) quest listesi boştur; bu yüzden quest ile açılan
skill'ler (51–54, 510–523) botta kullanılamaz. Betik, her botun sınıfının
quest'lerini `USERDATA.strQuest`'e **durum 2 (tamamlandı)** olarak yazar; quest
listesi `db/002`'de sıfırken de skill'ler bir insan oyuncununkiyle aynı olur.
- Kayıt düzeni: her quest kaydı 3 bayttır; kimlik **little-endian uint16** (bayt0 = kimlik % 256, bayt1 = kimlik / 256) + durum **uint8** (`DBAgent.cpp:402`).

- **Sunucular kapalıyken** çalıştırılmalıdır: oyundaki bir karakter çıkışta
  bellekteki quest listesini `USERDATA`'ya geri yazar, bu yüzden açıkken yazmak
  işe yaramaz.
- Yalnızca 12 bot satırına (açık ad listesi) dokunur; başka satır/tablo okunmaz.
  Satır içeriği ekrana basılmaz.
- Mevcut kayıtlar (ör. 500 tohum quest'i, kill sayaçları) **korunur**; yalnızca
  gerekli kimlikler durum 2 ile yeniden yazılır. Betik idempotenttir; ikinci
  çalıştırma ancak gerekli kimlikler zaten durum 2 ise `changed=0` bildirir.
- `QuestTestPoints` **zorunludur** ve `0` ya da `1` olmalıdır: `0` `strSkill`'e
  dokunmaz; `1` yalnızca `BotWP_K` ve `BotMF_K` skill puanlarını 80. seviye
  quest skill'leri (Hell blade 106580, Igzination 110575) atılabilsin diye
  yükseltir (toplam 142, ağaç ≤ 80, master ≤ 20). Eski `strSkill` yedeğe alınır.
  Bu sqlcmd sürümü tanımsız değişkende batch'i durdurduğundan değişken her
  çalıştırmada verilmelidir.

Uygula:

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v QuestTestPoints=0 -i db/003_bot_quests.sql
```

Geri al (yedek tablo `dbo.USERDATA_BOT_QUEST_BACKUP` korunur; ikinci geri alma
etkisizdir):

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -i db/003_bot_quests_rollback.sql
```

## 004 — Bot envanter doldurma (ADR-0018 Ek 16, KI-DEG-05)

Bot çantası `USERDATA.strItem` içinde kalıcıdır ve Ronark'ta onu yeniden
dolduran bir oyun eylemi yoktur (`docs/11` STK-04); tüketilen potlar, sınıf
taşları ve diriltme taşları bu yüzden her oturumda azalır. Betik, 12 bot
satırının **çanta yuvalarını (14..21)** senaryo stokuna geri yazar: tüketilmeyen
720 HP (`389014000`) ve 1920 MP (`389020000`) potlarından birer adet, tüketilen
Water of bless (`389015000`, `HpPots`), Potion of Ancient Spirit (`389220000`,
`MpPots`), Stone of life (`379006000`, `LifeStones`), sınıf taşı (`ClassStones`),
sınıf scroll'u ve (yalnızca mage) Spell of impact (`379070000`). Ekipman
(0..13) ve çantanın kalanı (22..72) değişmez.
- **Sunucular kapalıyken** çalıştırılmalıdır: oyundaki bir karakter çıkışta
  bellekteki çantayı `USERDATA`'ya geri yazar (ADR-0032-DEG).
- Yalnızca 12 bot satırına (açık ad listesi) dokunur; başka satır/tablo okunmaz.
  Satır içeriği ekrana basılmaz (yalnızca sayaç satırı).
- Dört değişken **zorunludur** ve `0..9999` tamsayı olmalıdır; `0` o yuvayı boş
  bırakır. `db/002` ile eşdeğer varsayılanlar: `HpPots=100, MpPots=0,
  LifeStones=30, ClassStones=50`. Betik idempotenttir (ikinci çalıştırma
  `changed=0`). Eski 14..21 baytları `dbo.USERDATA_BOT_STOCK_BACKUP` tablosuna
  bir kez alınır (tekrar çalıştırma yedeği ezmez).
- Beklenen çıktı: `BOTSTOCK: rows=12 ok=12 fail=0 changed=<N> hp=... mp=...
  life=... class=...`; geri alma: `BOTSTOCK_ROLLBACK: restored=<N>`.

Uygula (değişkenler her çalıştırmada verilir):

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v HpPots=100 -v MpPots=0 -v LifeStones=30 -v ClassStones=50 -i db/004_bot_inventory.sql
```

Geri al (yedek tablo `dbo.USERDATA_BOT_STOCK_BACKUP` korunur; ikinci geri alma
etkisizdir):

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -i db/004_bot_inventory_rollback.sql
```

İnce sarmalayıcı (sunucuların kapalı olduğunu denetler, `--dry-run` yalnızca
komutu yazdırır):

```bash
tools/bot-refill.sh apply
tools/bot-refill.sh apply --hp-pots 7 --mp-pots 5 --life-stones 3 --class-stones 11
tools/bot-refill.sh rollback
```

## Numara ayırma notu (2026-10-03)

`db/001`..`db/004` uygulanmış/yazılmış betiklerdir ve **yeniden numaralandırılmaz**: 001 `MAGIC.Etc`, 002 bot karakterleri, 003 bot quest (F4-27), 004 bot envanter (F4-40). 8v8 için karakter genişletme betikleri **`db/005`** (ilk 16 karakter, F8-03) ve **`db/006`** (+4 çeşitlilik karakteri, F8-04) olarak ayrılmıştır. Eski kayıtlarda (`docs/reports/*`, kapanmış planlar) 16/20 karakter için geçen "`db/003`" ifadesi bu betikleri kasteder; `db/003` her zaman bot quest betiğidir.

## 012 — 1534 pelerinleri (U2-01, ADR-0068 madde 4)

1534 istemcisinin `Data/Cloak.tbl` tablosu (7 kolon) 224 pelerin tanır;
`KNIGHTS_CAPE` tablosunda 56 satır vardır. Betik eksik 168 pelerini — uzun
(royal) pelerinler dahil — **istemci tablosundaki değerlerle** ekler. ALPHA
DB'sinin değerleri kullanılmaz (klan puanı fiyatı ve kademe şartı yanlış;
`docs/reports/u0-1534/E-veri-farki.md` §2.3).

- Betikler **üretilmiştir**, elle düzenlenmez: `tools/u2-gen-capes.py` istemci
  `Cloak.tbl`'ını `tools/kotbl.py` ile çözer ve iki betiği yazar. Kolon eşlemesi
  c0 `sCapeIndex`, c2 `nBuyPrice`, c3 `nDuration`, c4 `byGrade`, c5 `nBuyLoyalty`,
  c6 `byRanking`. Yalnız 7 kolonlu (1534) tablo kabul edilir; 5 kolonlu eski
  tablo hata verir. `python3 tools/u2-gen-capes.py --check` depodaki betiklerin
  üretici çıktısıyla bayt bayt aynı olduğunu denetler (çıkış 0).
- Yalnızca hedefte olmayan kimlikler eklenir; mevcut satırlara dokunulmaz. Eski
  56 satırda `byRanking` 0, istemcide 2'dir; fark bilinçli olarak bırakılır
  (`HandleCapeChange` zaten terfi etmiş klan ister, davranış farkı yok).
- Eklenen kimlikler `dbo.<Target>_U2_ADDED` kayıt tablosuna yazılır. Betik
  idempotenttir; ikinci çalıştırma `inserted=0` bildirir.
- `byRanking` 8–12 olan 84 satır uzun (royal) pelerinlerdir (x40–48, x60–64).
- Sunucu `strName`'i yüklemez; Korece istemci adları yerine ASCII
  `Cape <id>` (bilinen desen adlarında `Cape <id> <desen>`) yazılır.
- Sunucunun kapalı olması **gerekmez**; ancak tablo yalnızca açılışta yüklenir,
  etki için sunucu yeniden başlatılmalıdır.
- Beklenen çıktı (56 satırlık tabloda ilk uygulama): `inserted=168
  already_present=56` ve `target_rows=224 long_capes=84 logged=168`; geri alma:
  `removed=168 target_rows=56 log_table=absent`.

Uygula (`Target` zorunlu; gerçek kullanımda `KNIGHTS_CAPE`):

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Target=KNIGHTS_CAPE -i db/012_u2_capes_1534.sql
```

Geri al (yalnız kayıt tablosundaki kimlikleri siler, kayıt tablosunu düşürür;
tekrar çalıştırılabilir, ikinci çalıştırma `removed=0`):

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Target=KNIGHTS_CAPE -i db/012_u2_capes_1534_rollback.sql
```

Betikleri yeniden üret (varsayılan istemci yolu
`/mnt/c/dev/fdp1534/client/Knight Online/Data/Cloak.tbl`):

```bash
python3 tools/u2-gen-capes.py [--client PATH]
python3 tools/u2-gen-capes.py --check
```

## 013 — 1534 eşyaları (U2-02, ADR-0068 madde 4 ve Ek 1)

1534 istemcisinin çözebildiği (`item_org_us.tbl` tabanı + `Item_Ext_<n>_us.tbl`
varyantı; varyantın BaseID'si 0 ya da o taban) ama bizim `ITEM` tablomuzda
olmayan 35.861 kimlikten ALPHA DB'sinde (`.\SQL2019` / `FDP_alpha1534`) bulunan
**35.860** satırı ekler (300177146 iki DB'de de yok). Mevcut satırların hiçbiri
değişmez; botların kullandığı 83 eşya aynen kalır.

- Betikler **üretilmiştir**, elle düzenlenmez: `tools/u2-gen-alpha.py` ALPHA ve
  bizim DB'yi `SQLCMD.EXE` ile salt okur (yalnız `SELECT`), istemci tablolarını
  `tools/kotbl.py` ile çözer. `python3 tools/u2-gen-alpha.py --check` depodaki
  betiklerin üretici çıktısıyla bayt bayt aynı olduğunu denetler (çıkış 0).
  Üretici `dbo.ITEM_U2_ADDED` kayıt tablosundaki kimlikleri "bizde var" saymaz;
  betik canlı tabloya uygulandıktan sonra da `--check` 0 kalır.
- Kolonlar bizim 60 kolonumuzdur; ALPHA'ya özgü `UpgradeNotice`, `NPbuyPrice`,
  `Bound` kopyalanmaz. Değerler ALPHA'nınkidir (seviye ve stat şartları dahil;
  ADR-0068 Ek 1).
- ALPHA'da `ItemClass` her satırda NULL'dır. `ItemClass` ve aksesuar `ItemExt`
  üreticide hesaplanır; kural (`docs/reports/u0-1534/E-veri-farki.md` §3.4'ün
  netleştirilmiş hâli) betik başlığında yazılıdır ve bizim mevcut satırlarımızın
  %99,64'ünü (85.539 / 85.845) aynen verir.
- Satırlar önce oturumun geçici tablosu `#u2_items`'a 36 parti hâlinde (en çok
  1000 satır) yüklenir, sonra tek işlemde `INSERT … SELECT … WHERE NOT EXISTS (Num)`
  ile eklenir. Geçici tablo eksikse (ör. `-b` olmadan bir parti hata verdiyse)
  hiçbir satır eklenmez; yine de her zaman `-b` ile çalıştırın.
- Eklenen kimlikler `dbo.<Target>_U2_ADDED` kayıt tablosuna yazılır. Betik
  idempotenttir; ikinci çalıştırma `inserted=0` bildirir.
- Sunucunun kapalı olması **gerekmez**; ancak `ITEM` yalnızca açılışta
  (GameServer) yüklenir, etki için sunucu yeniden başlatılmalıdır. Satır sayısı
  85.920 → 121.780 (+%41,7); DB'de tablo verisi 14,6 MB → 20,7 MB; GameServer
  belleğinde tahmini +7–8 MB [A].
- Beklenen çıktı (85.920 satırlık tabloda ilk uygulama, yaklaşık 30 sn):
  `inserted=35860 already_present=0` ve `target_rows=121780 logged=35860
  staged_not_in_target=0`; geri alma: `removed=35860 target_rows=85920
  log_table=absent`.

Uygula (`Target` zorunlu; gerçek kullanımda `ITEM`):

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Target=ITEM -i db/013_u2_items_1534.sql
```

Geri al (yalnız kayıt tablosundaki kimlikleri siler, kayıt tablosunu düşürür;
tekrar çalıştırılabilir, ikinci çalıştırma `removed=0`):

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Target=ITEM -i db/013_u2_items_1534_rollback.sql
```

## 014 — 1534 NPC ve canavarları (U2-02, ADR-0068 madde 4 ve Ek 1)

İstemcinin `Npc_us.tbl` / `Mob_us.tbl` tablolarının tanıdığı ama bizim `K_NPC` /
`K_MONSTER` tablolarımızda olmayan kimlikleri ALPHA satırlarıyla ekler. ALPHA'nın
istemcide olmayan kimlikleri kopyalanmaz.

- `K_NPC`: bizde olmayan 82 istemci kimliğinin hepsi (hepsi ALPHA'da var).
  Bizim zone-64 bekçilerimizin kimlikleri 24438, 24439, 24440 (1534 istemcisinde
  başka NPC'ler) betikte **yoktur**; bizdeki satırları değişmez.
- `K_MONSTER`: bizde olmayan 65 istemci kimliğinden ALPHA'da bulunan 60'ı
  (4061, 8111, 8112, 8161, 8162 iki DB'de de yok). Düşürme tabloları
  (`K_MONSTER_ITEM`) bu betikte yoktur.
- `strName`: istemci adı (yazdırılabilir ASCII ve kolona, `varchar(30)`, sığıyorsa);
  değilse ALPHA adı; o da değilse `Npc <id>`. `K_NPC`'de 4 satır ALPHA adını alır
  (14438, 19004, 24434: istemci adı 30 karakterden uzun; 24437: ASCII değil),
  `K_MONSTER`'da 4063 `Npc 4063` olur (istemci adı 31 karakter, ALPHA adı Korece).
- Bizim tablolarda olup ALPHA'da olmayan `sLightR` ve `byMoneyType` (NOT NULL,
  varsayılansız) `0` yazılır. Sunucu bu kolonları yüklemez
  (`shared/database/NpcTableSet.h`); üretici bunu her çalıştırmada denetler.
- Kayıt tabloları `dbo.<NpcTarget>_U2_ADDED` ve `dbo.<MonTarget>_U2_ADDED`; betik
  idempotenttir.
- Sunucunun kapalı olması **gerekmez**; `K_NPC`/`K_MONSTER` yalnızca açılışta
  (AIServer) yüklenir, etki için yeniden başlatma gerekir. Yerleşim (`K_NPCPOS`)
  bu betikte yoktur (U3).
- Beklenen çıktı (ilk uygulama): `npc inserted=82 already_present=0`,
  `monster inserted=60 already_present=0`, `npc target_rows=603 logged=82
  staged_not_in_target=0`, `monster target_rows=850 logged=60
  staged_not_in_target=0`; geri alma: `npc removed=82 target_rows=521
  log_table=absent`, `monster removed=60 target_rows=790 log_table=absent`.

Uygula (`NpcTarget` ve `MonTarget` zorunlu; gerçek kullanımda `K_NPC`, `K_MONSTER`):

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v NpcTarget=K_NPC -v MonTarget=K_MONSTER -i db/014_u2_npc_monster_1534.sql
```

Geri al:

```bash
sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v NpcTarget=K_NPC -v MonTarget=K_MONSTER -i db/014_u2_npc_monster_1534_rollback.sql
```

Betikleri yeniden üret (varsayılan istemci klasörü
`/mnt/c/dev/fdp1534/client/Knight Online/Data`; iki DB'ye de Windows kimlik
doğrulamasıyla bağlanır):

```bash
python3 tools/u2-gen-alpha.py [items|npcs] [--client DIR]
python3 tools/u2-gen-alpha.py --check
```
