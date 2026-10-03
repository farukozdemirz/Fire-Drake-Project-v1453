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
