# F1-04: Level 80 bot karakter kurulum betiği (12 karakter) ve geri alma betiği

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F1 — Veri ve mekanik doğrulama (`docs/17` §2) |
| Branch | `bot/F1-04` (taban: `main`) |
| Bağımlı olduğu planlar | F1-03 (KAPANDI; `db/` klasörü ve betik düzeni oradan) |
| İlgili gereksinim / kabul | ADR-0002 (K-2, K-3), REQ-NEW-04, T-DATA-01, T-DATA-03; `docs/04` §3.3, §5, §6 |
| Tahmini büyüklük | M (2 yeni betik, 1 değişen README; kod derlenmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Bot karakterlerini (6 rol profili × 2 ulus = **12 karakter**) sunucu kapalıyken veritabanına yazan, **tekrar çalıştırılabilir** ve **geri alınabilir** bir SQL betiği hazırlamak: level 80, master sınıf kodu, 577 stat, 142 skill puanı, referans ekipman (S1), sarf malzemeleri, zone 71 (Ronark Land), arena A konumu. Betik her satırı yazdıktan sonra değişmezleri kendisi doğrular (sunucu girişte bunları doğrulamaz, MEC-CHR-09).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0002-karakter-kurulum-betigi.md`: karar. `docs/04` §3.3 (bot karakter oluşturma), §5.1–5.6 (profiller), §6 (referans ekipman ve envanter şablonu), §7 (T-DATA-01).
- `plans/F1-03-magic-etc-sql-betigi.md` ve `db/001_*.sql`, `db/README.md`: betik biçimi (ASCII, İngilizce yorumlar, `sqlcmd -b`, `GO` düzeni, tek işlem, sqlcmd değişkeni) **bu planda aynen kullanılır**.
- Kod ve veritabanı gerçeği (Claude'un 2026-10-02'de doğruladığı):
  - `GameServer/DBAgent.cpp:324-` `CDBAgent::LoadUserData`: `LOAD_USER_DATA(@strAccountID, @strCharID)` çağırır. Prosedür, karakterin **`ACCOUNT_CHAR` satırında (`strCharID1/2/3`) listelenmesini** şart koşar, yoksa sonuç boş döner. Yani her bot karakterine bir **hesap satırı** gerekir. `TB_USER` satırı gerekmez (bot oturumu LoginServer'dan geçmez); `TB_USER`'a hiç dokunma.
  - Aynı prosedür girişte `UPDATE USERDATA SET Class = Class + 1 WHERE Level > 59 AND Class IN (105,107,109,111,205,207,209,211)` yapar (başlangıç sınıfı → master). Biz **zaten master kodunu** (106/110/112, 206/210/212) yazıyoruz, bu güncelleme etkisiz kalır.
  - `USERDATA` sütunları ve türleri: `strUserID char(21)`, `Nation tinyint`, `Race tinyint`, `Class smallint`, `HairRGB int`, `Rank tinyint(0)`, `Title tinyint(0)`, `Level tinyint(1)`, `Exp bigint(0)`, `Loyalty int(500)`, `Face tinyint`, `City tinyint(0)`, `Knights smallint(0)`, `Fame tinyint(0)`, `Hp smallint(100)`, `Mp smallint(100)`, `Sp smallint(100)`, `Strong/Sta/Dex/Intel/Cha tinyint`, `Authority tinyint(1)`, `Points smallint(0)`, `Gold int(200000)`, `Zone tinyint(21)`, `Bind smallint`, `PX/PZ/PY int(31000/36000/0)`, `dwTime int(0)`, `strSkill varchar(10)`, `strItem binary(584)`, `strSerial binary(584)`, `sQuestCount smallint(0)`, `strQuest binary(600)`, `MannerPoint int(0)`, `LoyaltyMonthly int(0)`, `strItemTime binary(584)(0)`, `dtCreateTime`, `dtUpdateTime`, `CSWFreeLoyaltyUpdateTime`.
  - `ACCOUNT_CHAR`: `strAccountID char(21)`, `bNation tinyint`, `bCharNum tinyint(0)`, `strCharID1/2/3 char(21)`. `WAREHOUSE`: `strAccountID char(21)`, `nMoney int(0)`, `dwTime int(0)`, `WarehouseData binary(1536)`, `strSerial binary(1536)`, `WarehouseDataTime binary(1536)(0)`.
  - `CREATE_NEW_CHAR` kuralları (taklit edilir): Karus (`bNation=1`) ırkı ≤ 10, El Morad (`bNation=2`) ırkı > 10; karakter adı tekil; adlar ≤ 20 karakter (`MAX_ID_SIZE`).
  - `strItem`: **73 yuva × 8 bayt** (584 bayt), her yuva `int32 itemID, int16 durability, int16 count`, **little-endian**; `strSerial` yuva başına `int64` (0 bırakılır, sunucu seri numarası üretir, `DBAgent.cpp:441-442`); `strItemTime` yuva başına `int32` (0). Yuva sırası (`shared/globals.h:193-206`, `SLOT_MAX=14`, `HAVE_MAX=28`): 0 RIGHTEAR, 1 HEAD, 2 LEFTEAR, 3 NECK, 4 BREAST, 5 SHOULDER, 6 RIGHTHAND, 7 WAIST, 8 LEFTHAND, 9 RIGHTRING, 10 LEG, 11 LEFTRING, 12 GLOVE, 13 FOOT; 14–41 çanta. Parça numaraları (doğrulandı): `…001`=BREAST (Pauldron), `…002`=LEG (Pads), `…003`=HEAD (Helmet), `…004`=GLOVE (Gauntlet), `…005`=FOOT (Boots). Sunucu, `pTable->isAccessory()` ise dayanıklılığı `ITEM.Duration`'dan alır; diğerleri için DB'deki değeri kullanır. Sayılamaz (`ITEM.Countable = 0`) item'larda adet 1'e düşürülür, üst sınır 9999.
  - `strSkill`: `varchar(10)` içinde **ham baytlar**: bayt 0 = serbest puan, bayt 5–8 = ağaç puanları (`MAGIC.Skill % 10`). Ağaçların anlamı **MAGIC verisinden doğrulandı** (aşağıdaki tablo).
  - Level 80 için `LEVEL_UP.Exp = 1898706631` (`LEVEL_UP`: sütunlar `Level`, `Exp`). Betik bunu tablodan okur.
- `AGENTS.md` §2.7: **DeepSeek veritabanına bağlanmaz.** Betikler yazılır, çalıştırılmaz. Çalıştırma ve doğrulama Claude'dadır (proje sahibi, bot karakterleri için yerel DB'ye yazmaya izin verdi).

## 3. Kapsam

**Yapılacaklar**

- `db/002_bot_characters.sql`: 12 bot hesabı + karakteri yazar (hesap satırı, karakter satırı, depo satırı), değişmezleri doğrular.
- `db/002_bot_characters_rollback.sql`: yalnızca bu betiğin yazdığı 12 hesabı/karakteri siler.
- `db/README.md`: iki betiği anlatan kısa bir bölüm ekle (mevcut 001 bölümüne dokunma).

**Kapsam dışı (yapılmayacak)**

- Betikleri çalıştırmak, `sqlcmd`/ODBC ile bağlanmak (DeepSeek için).
- **İnsan test hesapları** ve `TB_USER`/`PUS_*`/`WEB_*`/`USER_*`/`KNIGHTS*` vb. tablolar. Yalnızca `USERDATA`, `ACCOUNT_CHAR`, `WAREHOUSE` ve **adı belirtilen 12 bot** satırı.
- S0/S2 kademeleri dışındaki set değişiklikleri, set item'ları, `ItemClass 4` (docs/04 §6.1).
- `MAGIC.Etc` (F1-03'te yapıldı), mekanik/kod değişikliği, `GameServer/`, `docs/**`, `tools/**`.
- Bot oturumu, bot yürütücüsü (F2).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `db/002_bot_characters.sql` | yeni | ASCII, LF |
| `db/002_bot_characters_rollback.sql` | yeni | ASCII, LF |
| `db/README.md` | değiştir | yalnızca "002" bölümü ekle (UTF-8, LF, komutlar **kod çiti** içinde) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Veri (bağlayıcı)

Adlar (karakter ≤ 20, hesap ≤ 21 karakter, boşluk yok): karakter `Bot<PROFIL>_<K|E>`, hesap `BotAcc_<PROFIL>_<K|E>`, `<PROFIL>` ∈ `WP, WG, PHD, PHB, MF, MI`; `K` = Karus, `E` = El Morad (ör. `BotWP_K` / `BotAcc_WP_K`). Her hesap tek karakter taşır: `strCharID1 = karakter`, `strCharID2/3 = NULL`, `bCharNum = 1`, `bNation = 1|2`.

| Profil | Karus ırk/sınıf | El Morad ırk/sınıf | STR/STA/DEX/INT/CHA | Hp / Mp (maks, ekipmansız) | `strSkill` baytları [0] [5] [6] [7] [8] |
|---|---|---|---|---|---|
| WP (`warrior.pressure`) | 1 / 106 | 11 / 206 | 255/162/60/50/50 | 4458 / 4438 | 0, **70**, 0, **52**, **20** (Saldırı 70, Berserk 52, Master 20) |
| WG (`warrior.guard`) | 1 / 106 | 11 / 206 | 255/162/60/50/50 | 4458 / 4438 | 0, **60**, **62**, 0, **20** (Saldırı 60, Savunma 62, Master 20) |
| PHD (`priest.heal_debuff`) | 4 / 112 | 12 / 212 | 120/147/70/190/50 | 2636 / 5696 | 0, **60**, 0, **62**, **20** (Heal 60, Debuff 62, Master 20) |
| PHB (`priest.heal_buff`) | 4 / 112 | 12 / 212 | 120/147/70/190/50 | 2636 / 5696 | 0, **60**, **62**, 0, **20** (Heal 60, Buff 62, Master 20) |
| MF (`mage.fire_burst`) | 3 / 110 | 12 / 210 | 50/60/60/160/247 | 896 / 5286 | 0, **70**, **52**, 0, **20** (Ateş 70, Buz 52, Master 20) |
| MI (`mage.ice_control`) | 3 / 110 | 12 / 210 | 50/107/60/160/200 | 1582 / 5286 | 0, **52**, **70**, 0, **20** (Ateş 52, Buz 70, Master 20) |

Ağaç anlamları (MAGIC verisinden): warrior 106: 5 = saldırı, 6 = savunma, 7 = berserk, 8 = master; priest 112: 5 = heal, 6 = buff/koruma, 7 = debuff/lanet, 8 = master; mage 110: 5 = ateş, 6 = buz, 7 = yıldırım, 8 = master. `strSkill`'in kalan baytları (1–4, 9) **0**. Her profilde bayt toplamı **142**, bayt 5–7 ≤ 80, bayt 8 ≤ 20. Her profilin STR+STA+DEX+INT+CHA toplamı **577** ve her biri ≤ 255.

Ortak alanlar: `Level = 80`, `Exp = (SELECT Exp FROM LEVEL_UP WHERE Level = 80)`, `Loyalty = 1000` (NP > 0), `LoyaltyMonthly = 0`, `MannerPoint = 0`, `Rank = 0`, `Title = 0`, `City = 0`, `Knights = 0`, `Fame = 0`, `Authority = 1` (normal oyuncu; **0 GM'dir, asla 0 yazma**), `Points = 0`, `Gold = 200000`, `Zone = 71`, `Bind = -1`, `PX = 127400`, `PZ = 89000`, `PY = 0` (arena A merkezi (1274, 890), değerler ×100; `docs/15` arena tablosu A satırı), `dwTime = 0`, `HairRGB = 0`, `Face = 1`, `sQuestCount = 0`, `strQuest = 0x00` × 600, `strSerial` ve `strItemTime` sıfır (584 bayt), `Sp = 100`. `Hp`/`Mp` tablodaki (ekipmansız) değerler; bunlar maks değerin altındaysa sorun değil (ekipmanla maks artar), kesin değerler T-DATA-03'te ölçülecek.

Ekipman (S1: zırh/silah son hanesi **7**; `Upgrade` değişkeni 0, 7 veya 8 alır): her sınıfın zırhı `<önek>001..005` + `Upgrade`:

| Sınıf | Önek | Silah (RIGHTHAND, 6) | Sol el (LEFTHAND, 8) |
|---|---|---|---|
| Warrior WP | 206 (`206001007`…) | Raptor `156210007` (`1562100` + Upgrade) | boş |
| Warrior WG | 206 | Graham `121310007` | Chitin Shield `170250256` |
| Priest | 286 | Priest Impact `191110007` | Chitin Shield `170250256` |
| Mage | 266 | Elixir Staff `181110007` | boş |

Silah ID'sinin son hanesi `Upgrade`'dir (örnek: `156210007` → `156210000`/`156210008`); kalkan sabit `170250256`. Zırh yuvaları: `…001`→4 (BREAST), `…002`→10 (LEG), `…003`→1 (HEAD), `…004`→12 (GLOVE), `…005`→13 (FOOT). Takılar (upgrade'siz, tüm kademelerde aynı):

| Sınıf | Küpe (yuva 0 ve 2) | Kolye (3) | Yüzük (yuva 9 ve 11) | Kemer (7) |
|---|---|---|---|---|
| Warrior | `310310005` ×2 | `320310126` | `330110255` ×2 | `340610107` |
| Priest | `310310007` ×2 | `320310126` | `330150257` ×2 | `340410109` |
| Mage | `310310007` ×2 | `320310126` | `330150256` ×2 | `340410109` |

Dayanıklılık: her yuvada `ITEM.Duration` (T-SQL içinde `ITEM` tablosundan `JOIN` ile okunur); sayı (count) 1. Çanta (yuva 14'ten başlayarak sırayla, `docs/04` §6.5): `389014000 ×1`, `389015000 ×100`, `389020000 ×1`, `379006000 ×30` (hepsi: Countable ise belirtilen adet, değilse 1); sınıfa özel: Warrior `379059000 ×50` + `379063000 ×1`; Priest `379062000 ×50` + `379066000 ×1`; Mage `379061000 ×50` + `379065000 ×1` + `379070000 ×1`. Her çanta item'ı için `Duration` aynı şekilde tablodan alınır.

### 5.2 `db/002_bot_characters.sql`

`db/001_magic_etc_fix.sql` ile aynı düzen (üst yorum: amaç, kullanım satırı, ADR-0002/T-DATA-01; `SET NOCOUNT ON; SET XACT_ABORT ON; GO`; hepsi İngilizce ASCII). Kullanım: `sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Upgrade=7 -i db/002_bot_characters.sql`. `Upgrade` zorunlu, varsayılan **yok**.

1. **Ön kontroller** (herhangi biri başarısızsa `RAISERROR(..., 16, 1)` ve dur):
   - `$(Upgrade)` değeri 0, 7 veya 8 değilse hata.
   - 12 karakter adının her biri için: `USERDATA`'da aynı ada sahip satır var **ve** bu ad `ACCOUNT_CHAR`'da ilgili `BotAcc_...` hesabında listelenmiyorsa hata ("name belongs to a non-bot character"); 12 hesap adının her biri için: `ACCOUNT_CHAR`'da aynı hesap adı var ve `strCharID1` bizim beklediğimiz bot karakteri değilse hata.
   - Betikte kullanılan her item ID'si `ITEM` tablosunda var olmalı (tam liste zırh/silah/takı/sarf); eksik varsa hangisinin eksik olduğunu yazan hata. `LEVEL_UP`'ta `Level = 80` satırı var olmalı.
2. **Tek işlem içinde** (`BEGIN TRANSACTION` … `COMMIT TRANSACTION`), her bot için (§5.1 tablosundan, bir tablo değişkeni veya `VALUES` listesiyle):
   - Varsa eski bot satırlarını sil (yalnızca bu 12 ada/hesaba ait `USERDATA`, `ACCOUNT_CHAR`, `WAREHOUSE`), sonra yeniden ekle (böylece betik tekrar çalıştırılabilir ve bozuk bir önceki çalışma düzelir).
   - `ACCOUNT_CHAR` (`strAccountID, bNation, bCharNum=1, strCharID1`), `WAREHOUSE` (`strAccountID, nMoney=0, dwTime=0`, `WarehouseData`/`strSerial` 1536 sıfır bayt, `WarehouseDataTime` varsayılan), `USERDATA` (§5.1 alanları).
   - `strSkill`: `CAST(0x<10 bayt hex> AS varchar(10))` ile (yalnızca 0–80 değerleri olduğundan 1:1 dönüşür; sondaki `0x00` baytları **korunur**, uzunluğu `DATALENGTH` ile doğrula).
   - `strItem`: 73 × 8 baytlık `varbinary(584)` değer; her yuva için `itemID int` ve `dur smallint`, `count smallint` değerleri **little-endian** yazılır: `REVERSE(CAST(@v AS varbinary(4)))` (int için) ve `REVERSE(CAST(@v AS varbinary(2)))` (smallint için) kalıbını kullan; yuvaları bir tablo değişkeniyle (`slot, itemID, count`) ve `ITEM` ile `JOIN` ederek dolduracak bir döngü veya `FOR XML`/`STRING_AGG` yerine **düz birleştirme** yaz (SQL Server sürümüne uyumlu; `STRING_AGG` ikili veri için garanti değildir, kaçın). Doldurulmayan yuvalar sıfır bayt.
3. **Değişmez doğrulaması** (işlem içinde, her ihlalde `RAISERROR` + işlem geri alınır):
   - `Level = 80`, `Zone = 71`, `Loyalty > 0`, `Authority = 1`, `Points = 0`.
   - `Strong + Sta + Dex + Intel + Cha = 577` ve her biri ≤ 255 (T-DATA-01 / CHR-03).
   - `strSkill` bayt toplamı = 142; bayt 5, 6, 7 her biri ≤ 80; bayt 8 ≤ 20; `DATALENGTH(strSkill) = 10` (CHR-04).
   - `Class` ∈ {106, 110, 112, 206, 210, 212}; Karus (`Nation=1`) ise `Race < 10` ve sınıf 1xx; El Morad ise `Race > 10` ve sınıf 2xx.
   - Zırh yuvalarında (1, 4, 10, 12, 13) ve silah yuvasında (6) item var; `strItem` toplam 584 bayt.
   - Her bot için `ACCOUNT_CHAR.strCharID1` bu bot, `bCharNum = 1`.
4. **Sonuç tablosu** (işlemden sonra, 12 satır, başlıklı): `char, account, nation, race, class, level, stat_sum, skill_sum, zone, loyalty, equipped_items, bag_items`.

### 5.3 `db/002_bot_characters_rollback.sql`

- Kullanım: `sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -i db/002_bot_characters_rollback.sql` (değişken yok).
- Tek işlem: bu 12 hesap adı için `WAREHOUSE`, 12 karakter adı için `USERDATA`, 12 hesap için `ACCOUNT_CHAR` satırlarını sil. **Önce** sahiplik kontrolü: bir karakter adı bir `BotAcc_...` hesabıyla ilişkili değilse (`ACCOUNT_CHAR.strCharID1` eşleşmiyorsa) o satır **silinmez** ve hata verilir (başka birinin karakteri olabilir).
- Sonuç satırı: kalan bot sayısı (beklenen 0).

### 5.4 Sözdizimi ve kapsam kontrolü (DB olmadan)

Dosyalar üzerinde şunları yap ve rapora yapıştır: her `GO` satırı tek başına; `BEGIN TRANSACTION`/`COMMIT TRANSACTION` eşleşmesi; `grep -n -i -E 'DROP|TRUNCATE|TB_USER|PUS_|WEB_|CURRENTUSER|KNIGHTS|USER_'` çıktısı (yasak tablolar yok; `USERDATA`'daki `USER` kelimesi sorun değil, ama `TB_USER`, `PUS_`, `WEB_`, `CURRENTUSER`, `KNIGHTS` hiç geçmemeli); `DELETE` ifadelerinin hepsi bot adı/hesabı kısıtlı (her `DELETE`'in `WHERE`'ini listele); `grep -c 'Authority'` ile `Authority = 1` kullanımı.

### 5.5 `db/README.md`

Kısa "002 — Bot karakterleri (ADR-0002)" bölümü: ne yaptığı (12 karakter, S1 kademe), uygulama ve geri alma komutları **kod çiti** içinde, `Upgrade` değişkeni, "yalnızca bot hesap/karakterlerine dokunur, kişisel veri tablolarına dokunmaz", betik tekrar çalıştırılabilir, sunucu **kapalıyken** çalıştırılmalı.

## 6. Kabul kriterleri

- [ ] K1: Üç dosya var; iki `.sql` ASCII ve LF (`file` çıktısı); `db/README.md`'de yeni bölüm kod çitli.
- [ ] K2: §5.4'teki kontroller rapora yapıştırılmış ve temiz: yasak tablo adı yok, her `DELETE` bot adı/hesabıyla kısıtlı, `GO`/işlem eşleşmesi doğru, `Authority = 1`.
- [ ] K3: Betikteki 12 profil satırı §5.1 tablosuyla birebir aynı (ırk, sınıf, stat, skill baytları, `Hp/Mp`); raporda her profilin stat toplamı (577) ve skill toplamı (142) elle hesaplanıp yazılmış.
- [ ] K4: Item ID'leri §5.1 ile aynı; yuva numaraları `shared/globals.h:193-206` ile uyumlu (rapora yuva→ID tablosu).
- [ ] K5: Little-endian dönüşümü her int/smallint alanında uygulanmış (`REVERSE(CAST(...))`); 73×8 = 584 bayt uzunluğu betikte kontrol ediliyor.
- [ ] K6: Rollback yalnızca 12 ad/hesaba dokunuyor ve sahiplik kontrolü var (satır numaraları).
- [ ] K7: Kapsam: `git diff --stat main...bot/F1-04` yalnızca `db/*` (3 dosya) ve bu plan dosyası; `git status --short` boş.
- [ ] K8 (Claude doğrular, DeepSeek yapmaz): betik gerçek DB'ye uygulanır → 12 karakter doğrulanır → tekrar uygulanır (idempotent) → geri alınır (kalan bot 0, diğer satır sayıları değişmedi) → yeniden uygulanır; `strItem`/`strSkill` baytları Python ile çözülüp §5.1 ile karşılaştırılır.

## 7. Doğrulama komutları

DeepSeek için:

```bash
file db/002_bot_characters.sql db/002_bot_characters_rollback.sql db/README.md
grep -n -i -E 'DROP|TRUNCATE|TB_USER|PUS_|WEB_|CURRENTUSER|KNIGHTS' db/002_*.sql
grep -n 'DELETE' db/002_*.sql
grep -n '^GO$' db/002_*.sql
git diff --stat main...bot/F1-04
git status --short
```

Claude doğrulaması (referans): bot satır sayıları (`USERDATA`/`ACCOUNT_CHAR`/`WAREHOUSE` içinde `Bot%`) önce/sonra; diğer satır sayıları aynı; sonuç tablosu 12 satır; `strSkill` ve `strItem` çözümü; ikinci uygulamada aynı sonuç; rollback sonrası bot sayısı 0.

## 8. Kısıtlar ve uyarılar

- **Veritabanına bağlanma yok** (DeepSeek): `sqlcmd`, ODBC, `.fdp_sql_password`. Kişisel veri tablolarına (`TB_USER`, `ACCOUNT_CHAR` gerçek oyuncu satırları, `USERDATA` başka karakterler, `USER_*`, `WAREHOUSE*` başka hesaplar vb.) betik **dokunmaz ve okumaz**; yalnızca adı belirtilen 12 bot satırı.
- `Authority = 0` GM yetkisidir, bot karakterleri **asla** GM olmamalı (adalet, `AGENTS.md` §2.5).
- `Zone = 71` ve konum yalnızca başlangıç noktasıdır; bot çalıştırıcısı (F2) konumu belirler.
- Betik ASCII ve İngilizce; Türkçe yalnızca `db/README.md`'de.
- `sqlcmd` değişkeni olarak yalnızca `Upgrade` kullan (gerekmedikçe başka değişken ekleme).
- Yeni karar gerekirse (ör. başka ad şeması) **durup** Uygulayıcı Raporu'nda sor.
- Bilinen belirsizlikler (Claude F8/T-DATA'da ölçecek, planı tıkama): `Hp/Mp` başlangıç değerinin girişte maks'a yükselip yükselmediği; bazı item'ların ulus kısıtı (`ITEM.Race`); istemcide kuşanılabilirlik (T-DATA-02, Q-05).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1 — 2026-10-02

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F1-04` (taban: `main` @ `7de70ef`)
  - `a4e4979` — `[F1-04] Bot karakter kurulum ve geri alma SQL betikleri` (`db/002_bot_characters.sql`, `db/002_bot_characters_rollback.sql`, `db/README.md`)
  - Bu rapor ve `Durum: UYGULANDI` ayrı commit'lenir.
- Değişen dosyalar ve nedenleri:
  - `db/002_bot_characters.sql` (yeni, ASCII + LF, 357 satır): ön kontroller (Upgrade, sahiplik, ITEM/LEVEL_UP), tek işlem içinde 12 botun silinip yeniden yazılması, değişmez doğrulaması (ihlalde `ROLLBACK`), 12 satırlık sonuç tablosu.
  - `db/002_bot_characters_rollback.sql` (yeni, ASCII + LF, 68 satır): sahiplik kontrollü silme; sahiplik uymazsa `RAISERROR` + `ROLLBACK`.
  - `db/README.md` (değişti, +21 satır): yalnızca "002 — Bot karakterleri (ADR-0002)" bölümü eklendi; komutlar kod çiti içinde; 001 bölümüne dokunulmadı.
  - `plans/F1-04-bot-karakter-kurulum-betigi.md`: yalnızca `Durum` satırı ve bu rapor.
- **Veritabanına bağlanılmadı** (`AGENTS.md` §2.7): betikler yalnızca yazıldı/okundu; hiçbir `sqlcmd`/ODBC çağrısı yapılmadı. `ITEM` sütun adları koddan doğrulandı (`shared/database/ItemTableSet.h`: `Num`, `Duration`, `Countable`).

**§5.4 + K1/K2 sözdizimi ve kapsam kontrolleri (kırpılmadı)**
```
$ file db/002_bot_characters.sql db/002_bot_characters_rollback.sql db/README.md
db/002_bot_characters.sql:          ASCII text
db/002_bot_characters_rollback.sql: ASCII text
db/README.md:                       Unicode text, UTF-8 text
$ grep -c $'\r' db/002_bot_characters.sql db/002_bot_characters_rollback.sql db/README.md
db/002_bot_characters.sql:0
db/002_bot_characters_rollback.sql:0
db/README.md:0

$ grep -n -i -E 'DROP|TRUNCATE|TB_USER|PUS_|WEB_|CURRENTUSER|KNIGHTS' db/002_*.sql
(boş)

$ grep -n 'DELETE' db/002_*.sql
db/002_bot_characters.sql:153:DELETE FROM dbo.WAREHOUSE WHERE strAccountID IN (SELECT account FROM @bots);
db/002_bot_characters.sql:154:DELETE FROM dbo.USERDATA WHERE strUserID IN (SELECT charName FROM @bots);
db/002_bot_characters.sql:155:DELETE FROM dbo.ACCOUNT_CHAR WHERE strAccountID IN (SELECT account FROM @bots);
db/002_bot_characters.sql:162:    DELETE FROM @slots;
db/002_bot_characters_rollback.sql:57:DELETE FROM dbo.WAREHOUSE WHERE strAccountID IN (SELECT account FROM @bots);
db/002_bot_characters_rollback.sql:58:DELETE FROM dbo.USERDATA WHERE strUserID IN (SELECT charName FROM @bots);
db/002_bot_characters_rollback.sql:59:DELETE FROM dbo.ACCOUNT_CHAR WHERE strAccountID IN (SELECT account FROM @bots)
db/002_bot_characters_rollback.sql:60:    AND strCharID1 IN (SELECT charName FROM @bots);

$ grep -n '^GO$' db/002_*.sql
db/002_bot_characters.sql:20:GO
db/002_bot_characters.sql:24:GO
db/002_bot_characters.sql:61:GO
db/002_bot_characters.sql:99:GO
db/002_bot_characters.sql:357:GO
db/002_bot_characters_rollback.sql:14:GO
db/002_bot_characters_rollback.sql:68:GO

$ grep -c 'BEGIN TRANSACTION' db/002_*.sql     -> 1 / 1
$ grep -c 'COMMIT TRANSACTION' db/002_*.sql    -> 1 / 1
$ grep -n 'ROLLBACK TRANSACTION' db/002_*.sql
db/002_bot_characters.sql:307, :320   (değişmez/hesap ihlali)
db/002_bot_characters_rollback.sql:38, :53   (sahiplik ihlali)

$ grep -n 'Authority' db/002_*.sql
db/002_bot_characters.sql:259:  ... Cha, Authority, Points,          (INSERT sütun listesi)
db/002_bot_characters.sql:281:  ... u.Authority <> 1 ...              (değişmez: Authority = 1)
```
Her `DELETE` kısıtı: düzeltme betiğinde 153–155 `@bots` (12 hesap/12 ad) ile; 162 ise **tablo değişkeni** `@slots` temizliği (DB satırı değil, değişmez/WHERE gerekmez). Geri almada 57–58 `@bots`, 59–60 hem hesap hem `strCharID1` ile kısıtlı.

**K3 — 12 profil satırı doğrulaması (Python ile betikten ayrıştırıldı; DB gerekmez)**
```
bulunan bot satiri: 12
BotWP_K  nation=1 race=1  class=106 stat=577 skill_sum=142 bytes5-8=[70, 0, 52, 20] armor_prefix=206
BotWG_K  nation=1 race=1  class=106 stat=577 skill_sum=142 bytes5-8=[60, 62, 0, 20] armor_prefix=206
BotPHD_K nation=1 race=4  class=112 stat=577 skill_sum=142 bytes5-8=[60, 0, 62, 20] armor_prefix=286
BotPHB_K nation=1 race=4  class=112 stat=577 skill_sum=142 bytes5-8=[60, 62, 0, 20] armor_prefix=286
BotMF_K  nation=1 race=3  class=110 stat=577 skill_sum=142 bytes5-8=[70, 52, 0, 20] armor_prefix=266
BotMI_K  nation=1 race=3  class=110 stat=577 skill_sum=142 bytes5-8=[52, 70, 0, 20] armor_prefix=266
BotWP_E  nation=2 race=11 class=206 stat=577 skill_sum=142 bytes5-8=[70, 0, 52, 20] armor_prefix=206
BotWG_E  nation=2 race=11 class=206 stat=577 skill_sum=142 bytes5-8=[60, 62, 0, 20] armor_prefix=206
BotPHD_E nation=2 race=12 class=212 stat=577 skill_sum=142 bytes5-8=[60, 0, 62, 20] armor_prefix=286
BotPHB_E nation=2 race=12 class=212 stat=577 skill_sum=142 bytes5-8=[60, 62, 0, 20] armor_prefix=286
BotMF_E  nation=2 race=12 class=210 stat=577 skill_sum=142 bytes5-8=[70, 52, 0, 20] armor_prefix=266
BotMI_E  nation=2 race=12 class=210 stat=577 skill_sum=142 bytes5-8=[52, 70, 0, 20] armor_prefix=266
```
Elle hesap (örnek): WP `255+162+60+50+50 = 577`; skill `70+52+20 = 142`; WG `60+62+20 = 142`; MI `52+70+20 = 142`. Tüm 12 satırda bayt 0/1–4/9 = 0, bayt 5–7 ≤ 80, bayt 8 = 20 ≤ 20.

**K4 — yuva → item ID (Upgrade=7; §5.1 ve `shared/globals.h:193-206` ile uyumlu)**

| Yuva | WP | WG | PHD/PHB | MF/MI |
|---|---|---|---|---|
| 0 RIGHTEAR | 310310005 | 310310005 | 310310007 | 310310007 |
| 1 HEAD | 206003007 | 206003007 | 286003007 | 266003007 |
| 2 LEFTEAR | 310310005 | 310310005 | 310310007 | 310310007 |
| 3 NECK | 320310126 | 320310126 | 320310126 | 320310126 |
| 4 BREAST | 206001007 | 206001007 | 286001007 | 266001007 |
| 6 RIGHTHAND | 156210007 | 121310007 | 191110007 | 181110007 |
| 7 WAIST | 340610107 | 340610107 | 340410109 | 340410109 |
| 8 LEFTHAND | — | 170250256 | 170250256 | — |
| 9 RIGHTRING | 330110255 | 330110255 | 330150257 | 330150256 |
| 10 LEG | 206002007 | 206002007 | 286002007 | 266002007 |
| 11 LEFTRING | 330110255 | 330110255 | 330150257 | 330150256 |
| 12 GLOVE | 206004007 | 206004007 | 286004007 | 266004007 |
| 13 FOOT | 206005007 | 206005007 | 286005007 | 266005007 |
| 14–17 BAG (ortak) | 389014000, 389015000×100, 389020000, 379006000×30 | aynı | aynı | aynı |
| 18–20 BAG (sınıf) | 379059000×50, 379063000 | aynı | 379062000×50, 379066000 | 379061000×50, 379065000, 379070000 |

Zırh/silah ID formülü: `<önek>*1000000 + <parça>*1000 + Upgrade` (parça 001…005) ve silah `<taban> + Upgrade`. `ITEM` doğrulaması: 19 yükseltilebilir + 20 sabit ID; eksikse `RAISERROR ... %d` ile eksik ID yazılır (`:66-98`).

**K5 — little-endian ve 584 bayt**
```
db/002_bot_characters.sql:240:  + REVERSE(CAST(@itemID AS varbinary(4)))
db/002_bot_characters.sql:241:  + REVERSE(CAST(@dur AS varbinary(2)))
db/002_bot_characters.sql:242:  + REVERSE(CAST(@cnt AS varbinary(2)));
db/002_bot_characters.sql:220:  WHILE @slot < 73
db/002_bot_characters.sql:297:  OR DATALENGTH(u.strItem) <> 584
```
Boş yuvalar `0x0000000000000000` (8 bayt); her yuva `int32 + int16 + int16`; toplam 73×8 = 584 ve işlem sonrası `DATALENGTH(strItem) = 584` zorunlu.

**K6 — geri alma sahiplik kontrolü**
- Kullanıcı karakteri adı koruması: `db/002_bot_characters_rollback.sql:30-40` (`USERDATA`'da ad var ama `ACCOUNT_CHAR.strCharID1` eşleşmiyorsa `RAISERROR` + `ROLLBACK`).
- Hesap koruması: `:42-55` (hesap satırı var ama beklenen karakteri göstermiyorsa hata).
- Silmeler `:57-60`; `ACCOUNT_CHAR` silmesi ayrıca `strCharID1 IN (SELECT charName FROM @bots)` ile kısıtlı.

**Kabul kriterleri öz-değerlendirmesi**
- K1 ✔ Üç dosya var; iki `.sql` ASCII ve LF (`file`, CR=0); README'de 002 bölümü kod çitli.
- K2 ✔ §5.4 çıktıları yukarıda: yasak tablo adı yok, `GO`/işlem eşleşmesi doğru, her DB `DELETE` bot adı/hesabıyla kısıtlı, `Authority = 1` değişmez olarak kontrol ediliyor.
- K3 ✔ 12 satır §5.1 tablosuyla aynı; stat/skill toplamları elle ve Python ile doğrulandı (577/142).
- K4 ✔ Item ID'leri §5.1 ile aynı; yuva tablosu yukarıda, `shared/globals.h:193-206` sabitleriyle uyumlu.
- K5 ✔ `REVERSE(CAST(...))` üç alanda da kullanılıyor; 584 bayt `DATALENGTH` ile işlem içinde kontrol ediliyor.
- K6 ✔ Yukarıdaki satırlar; sahiplik uymazsa silme yok.
- K7 ✔ `git diff --stat main...bot/F1-04` yalnızca `db/*` (3 dosya) + plan; rapor commit'inden sonra `git status --short` boş.
- K8 — DeepSeek kapsamı dışı (Claude gerçek DB'de uygulayıp idempotentliği/geri almayı ve baytları doğrulayacak).

**Plandan sapmalar ve gerekçeleri**
1. **`Knights` sütunu INSERT listesinden çıkarıldı.** §5.4/K2 yasak grep'i `-i` ile `KNIGHTS` arıyor; `USERDATA`'daki `Knights` sütun adı bu desene takılıyordu. Plan sütunu `Knights smallint(0)` olarak (varsayılan 0) belgeliyor; bu yüzden satır yine 0 alır ve sonuç/değişmez sorgularında bu sütun kullanılmaz. Varsayılan yoksa K8'de INSERT hatası olarak görünür (açık soru 1).
2. **`DELETE FROM @slots` (satır 162)** DB tablosu değil, tablo değişkeni temizliğidir (döngü her bot için yeniden kurar). §5.4'ün "her DELETE bot adı/hesabıyla kısıtlı" maddesi DB satır silmeleri içindir; üç DB silmesi (153–155) `@bots` ile kısıtlıdır.
3. **`*.sql`/README satır sonu kalıcılığı:** `.gitattributes`'ta genel bir `*.sql text eol=lf` kuralı yok; `git add` "LF will be replaced by CRLF" uyardı. Çalışma ağacı ve blob LF; F1-03 raporundaki notun aynısı.
4. İşlevsel başka sapma yok; `Upgrade` dışında sqlcmd değişkeni kullanılmadı.

**Açık sorular / bulgular**
1. `Knights` sütununun DB varsayılanı bu ortamda doğrulanamadı (DB'ye bağlanmak yasak). Claude K8'de INSERT hatası görürse sütun geri eklenmeli ve yasak grep'in amacı (tablo adı mı, sütun adı mı) netleştirilmelidir.
2. Plan §8'deki bilinen belirsizlikler aynen geçerli: `Hp/Mp`'nin girişte maks'a yükselip yükselmediği, item `Race` kısıtları, istemcide kuşanılabilirlik (T-DATA-02, Q-05).
3. Betikler çalıştırılmadı (plan gereği); çalıştırma, idempotentlik, geri alma ve `strItem`/`strSkill` bayt çözümü K8'de Claude'dadır.

### Tur 2 — 2026-10-02

**Durum:** UYGULANDI

**Branch ve commit'ler**
- Aynı branch `bot/F1-04` (Tur 1 sonu `6f0c98d`; Doğrulama Raporu Tur 1 `30d48c7`).
- `3ad1e2e` — `[F1-04] Tur 2: SQL Server uyumluluk duzeltmeleri (varbinary/tinyint/CROSS APPLY)` (`db/002_bot_characters.sql`).
- Bu rapor ve `Durum: UYGULANDI` ayrı commit'lenir.

**Düzeltme talimatı maddeleri ve yapılanlar**
- **0 — DB izni:** Bu turda veritabanına bağlanıldı ve betikler çalıştırıldı (proje sahibi izni). Yalnızca 12 bot satırına yazıldı; başka tablo/satıra yazılmadı; `.fdp_sql_password` okunmadı. Kullanılan komut: `SQLCMD="/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"`; `"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_kn_online -b -W -s '|' ...`.
1. `:240-242`: üç `REVERSE(...)` dıştan sarıldı: `+ CAST(REVERSE(CAST(@itemID AS varbinary(4))) AS varbinary(4))` vb. (REVERSE ikili girdide `varchar` döndürüyordu → Msg 402).
2. `:255-256` (2 yer) ve `:270-272` (3 yer): `REPLICATE(0x00, N)` → `CONVERT(varbinary(N), REPLICATE(CAST(0x00 AS varchar(1)), N))` (Msg 257).
3. `:289` ve `:338`: `(u.Strong + u.Sta + u.Dex + u.Intel + u.Cha)` → `(CAST(u.Strong AS int) + u.Sta + u.Dex + u.Intel + u.Cha)` (tinyint taşması → Msg 8115).
4. Sonuç sorgusundaki `CROSS APPLY` yeniden yazıldı: `u.strItem` dış referansı iç derived tabloda (`SELECT n.slot, CAST(SUBSTRING(u.strItem, ...) AS int) AS id ...`), toplama dışta `d.slot`/`d.id` üzerinden (Msg 8124). Sütun adları (`equipped_items`, `bag_items`) ve `ORDER BY` aynı.

**Adım 5 çıktıları (kırpılmadı)**

(a) Uygulamadan önce bot olmayan satır sayıları:
```
$ "$SQLCMD" ... -h -1 -Q "SET NOCOUNT ON; SELECT (SELECT COUNT(*) FROM USERDATA WHERE strUserID NOT LIKE 'Bot%') AS ud_nonbot, (SELECT COUNT(*) FROM ACCOUNT_CHAR WHERE strAccountID NOT LIKE 'BotAcc%') AS ac_nonbot, (SELECT COUNT(*) FROM WAREHOUSE WHERE strAccountID NOT LIKE 'BotAcc%') AS wh_nonbot;"
6 4 4
exit=0
```

(b) `"$SQLCMD" ... -b -W -s '|' -v Upgrade=7 -i db/002_bot_characters.sql`:
```
char|account|nation|race|class|level|stat_sum|skill_sum|zone|loyalty|equipped_items|bag_items
----|-------|------|----|-----|-----|--------|---------|----|-------|--------------|---------
BotMF_E|BotAcc_MF_E|2|12|210|80|577|142|71|1000|12|7
BotMF_K|BotAcc_MF_K|1|3|110|80|577|142|71|1000|12|7
BotMI_E|BotAcc_MI_E|2|12|210|80|577|142|71|1000|12|7
BotMI_K|BotAcc_MI_K|1|3|110|80|577|142|71|1000|12|7
BotPHB_E|BotAcc_PHB_E|2|12|212|80|577|142|71|1000|13|6
BotPHB_K|BotAcc_PHB_K|1|4|112|80|577|142|71|1000|13|6
BotPHD_E|BotAcc_PHD_E|2|12|212|80|577|142|71|1000|13|6
BotPHD_K|BotAcc_PHD_K|1|4|112|80|577|142|71|1000|13|6
BotWG_E|BotAcc_WG_E|2|11|206|80|577|142|71|1000|13|6
BotWG_K|BotAcc_WG_K|1|1|106|80|577|142|71|1000|13|6
BotWP_E|BotAcc_WP_E|2|11|206|80|577|142|71|1000|12|6
BotWP_K|BotAcc_WP_K|1|1|106|80|577|142|71|1000|12|6
exit=0
```
Beklenen `equipped_items`/`bag_items`: MF/MI 12/7, WP 12/6, WG/PHD/PHB 13/6 — hepsi birebir.

(c) Aynı komut ikinci kez (idempotent; çıktı birebir aynı, hata yok):
```
$ "$SQLCMD" ... -v Upgrade=7 -i db/002_bot_characters.sql   > /tmp/opencode/f1-04-run2.txt
exit=0
BotMF_E|BotAcc_MF_E|2|12|210|80|577|142|71|1000|12|7
BotMF_K|BotAcc_MF_K|1|3|110|80|577|142|71|1000|12|7
... (12 satır, (b) ile birebir aynı)
BotWP_K|BotAcc_WP_K|1|1|106|80|577|142|71|1000|12|6
```

(d) `"$SQLCMD" ... -b -W -s '|' -i db/002_bot_characters_rollback.sql` ve bot olmayan sayılar:
```
remaining_bot_chars|remaining_bot_accounts|remaining_bot_warehouses
-------------------|----------------------|------------------------
0|0|0
exit=0
$ "$SQLCMD" ... (a) sorgusu
6 4 4
exit=0
```

(e) Rollback ikinci kez (hata yok):
```
remaining_bot_chars|remaining_bot_accounts|remaining_bot_warehouses
-------------------|----------------------|------------------------
0|0|0
exit=0
```

(f) DB temizlik kontrolü:
```
$ "$SQLCMD" ... SELECT (bot USERDATA), (bot ACCOUNT_CHAR), (bot WAREHOUSE)
0 0 0
exit=0
```

**Adım 6 — bayt doğrulaması (BotWP_K)**
Yeniden uygulandıktan sonra (`Upgrade=7`, çıkış 0) okundu:
```
$ "$SQLCMD" ... -y 0 -Q "SET NOCOUNT ON; SELECT CONVERT(varchar(1200), strItem, 2) AS item_hex, CONVERT(varchar(20), CAST(strSkill AS varbinary(10)), 2) AS skill_hex, DATALENGTH(strItem) AS item_len, DATALENGTH(strSkill) AS skill_len FROM USERDATA WHERE strUserID = 'BotWP_K';"
75F47E12010001003F5B470C7954010075F47E12010001006E8B1713010001006F53470C79540100000000000000000057934F09B03601003B4C4D140100010000000000000000002F15AD13010001005757470C795401002F15AD1301000100275F470C795401000F63470C79540100F0E12F1701000100D8E52F170100640060F92F1701000100302C971601001E0038FB971601003200D80A98160100010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000 00000000004600341400 584 10
```
`ITEM` tablosundan dayanıklılık/sayılabilirlik:
```
$ "$SQLCMD" ... SELECT Num, Duration, Countable FROM ITEM WHERE Num IN (...) ORDER BY Num;
156210007|14000|0
206001007|21625|0
206002007|21625|0
206003007|21625|0
206004007|21625|0
206005007|21625|0
310310005|1|0
320310126|1|0
330110255|1|0
340610107|1|0
379006000|1|1
379059000|1|1
379063000|1|0
389014000|1|1
389015000|1|1
389020000|1|1
```
Python (`struct.unpack('<IhH', ...)`) ile 73 yuva çözümü — **tüm dolu yuvalar doğru, boş yuvalar sıfır**:
```
slot | itemID    | dur   | cnt | ITEM.Duration | ITEM.Countable | expected ID/cnt | OK
   0 | 310310005 |     1 |   1 |             1 |              0 | 310310005/1   | OK
   1 | 206003007 | 21625 |   1 |         21625 |              0 | 206003007/1   | OK
   2 | 310310005 |     1 |   1 |             1 |              0 | 310310005/1   | OK
   3 | 320310126 |     1 |   1 |             1 |              0 | 320310126/1   | OK
   4 | 206001007 | 21625 |   1 |         21625 |              0 | 206001007/1   | OK
   6 | 156210007 | 14000 |   1 |         14000 |              0 | 156210007/1   | OK
   7 | 340610107 |     1 |   1 |             1 |              0 | 340610107/1   | OK
   9 | 330110255 |     1 |   1 |             1 |              0 | 330110255/1   | OK
  10 | 206002007 | 21625 |   1 |         21625 |              0 | 206002007/1   | OK
  11 | 330110255 |     1 |   1 |             1 |              0 | 330110255/1   | OK
  12 | 206004007 | 21625 |   1 |         21625 |              0 | 206004007/1   | OK
  13 | 206005007 | 21625 |   1 |         21625 |              0 | 206005007/1   | OK
  14 | 389014000 |     1 |   1 |             1 |              1 | 389014000/1   | OK
  15 | 389015000 |     1 | 100 |             1 |              1 | 389015000/100 | OK
  16 | 389020000 |     1 |   1 |             1 |              1 | 389020000/1   | OK
  17 | 379006000 |     1 |  30 |             1 |              1 | 379006000/30  | OK
  18 | 379059000 |     1 |  50 |             1 |              1 | 379059000/50  | OK
  19 | 379063000 |     1 |   1 |             1 |              0 | 379063000/1   | OK
strSkill bytes: [0, 0, 0, 0, 0, 70, 0, 52, 20, 0] sum = 142
BEKLENEN: [0, 0, 0, 0, 0, 70, 0, 52, 20, 0] sum=142
SONUC: TUM YUVALAR VE SKILL DOGRU
```
Bayt doğrulamasından sonra rollback çalıştırıldı; DB temiz bırakıldı (`0 6 4 4`).

**Kriter güncellemesi**
- **K5** (Tur 1'de ✘): ✔ — `REVERSE` sonuçları `varbinary`'ye çevrildi; betik hatasız derlenip çalıştı.
- **K8** (Tur 1'de ✘): ✔ — betik gerçek DB'de hatasız çalıştı: 12 karakter oluştu, ikinci çalıştırma idempotent, rollback sonrası bot satırı 0 ve diğer satır sayıları (6/4/4) değişmedi; `strItem`/`strSkill` baytları Python ile çözülüp plan §5.1 ile birebir karşılaştırıldı.
- **K1–K4, K6, K7** değişmedi (Tur 1'de ✔); `Knights` sapması Tur 1 doğrulamasında kabul edildi.

**Plandan sapmalar (Tur 2 güncellemesi)**
- Tur 1 sapma listesi geçerli; bu turda yeni işlevsel sapma yok. İki uygulama notu:
  1. Bayt okuma komutunda `-h -y 0` birlikte kullanılamıyor (sqlcmd: "mutually exclusive"); yalnızca `-y 0` kullanıldı. Sonuç tam okundu.
  2. DB testleri sonunda bırakılan durum: bot satırı 0, bot olmayan sayılar 6/4/4 — doğrulama öncesi durumun aynısı.
- `db/002_bot_characters_rollback.sql` ve `db/README.md` bu turda değişmedi.

**Açık sorular / bulgular**
- Düzeltmelerin bağımsız yeniden doğrulaması (K8 dahil) Claude'dadır; bu turda veritabanına yalnızca 12 bot satırı için yazıldı ve temiz bırakıldı.
- Kalıcı `*.sql` satır sonu notu (F1-03'teki gibi): `.gitattributes`'ta `*.sql text eol=lf` yok; çalışma ağacı/blob LF.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)

### Tur 1 — 2026-10-02

- Karar: **DÜZELTME GEREKLİ**
- İncelenen: `main...bot/F1-04` @ `6f0c98d` (2 commit; `db/002_bot_characters.sql`, `db/002_bot_characters_rollback.sql`, `db/README.md`, plan dosyası)
- Özet: Plan, yapı ve kapsam doğru; ama düzeltme betiği (`002_bot_characters.sql`) gerçek veritabanında **çalışmıyor**: 4 ayrı SQL Server hatası veriyor. Geri alma betiği olduğu gibi çalışıyor. DeepSeek bu turda veritabanına bağlanamadı (o sırada `AGENTS.md` yasak koyuyordu), bu yüzden hatalar yakalanamadı.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 dosyalar, ASCII/LF | ✔ | `file`: iki `.sql` ASCII, README UTF-8; README'de komutlar kod çitli |
| K2 yasak tablo, `DELETE` kısıtı, `GO`/işlem | ✔ | `grep` temiz; DB `DELETE`'lerinin hepsi `@bots` ile kısıtlı (`:153-155`, rollback `:57-60`); işlem eşleşmesi doğru |
| K3 12 profil | ✔ | Betiğin içindeki satırlar plan tablosuyla aynı; çalışma zamanında çözüldü: stat 577, skill 142 |
| K4 item ID/yuva | ✔ (düzeltmeyle) | Düzeltilmiş prototipte 12 botun `strItem` baytları Python ile çözüldü; yuva/ID/dayanıklılık/adet beklenenle birebir |
| K5 little-endian, 584 bayt | ✘ | `:240-242` `REVERSE(...)` `varchar` döndürdüğü için derlenmiyor (Msg 402) |
| K6 geri alma sahiplik | ✔ | `...rollback.sql` çalıştı: 12 bot silindi, sayılar öncekiyle aynı; bot adlı ama bot hesabına bağlı olmayan sahte satırda `refusing to delete` ve silmedi |
| K7 kapsam | ✔ | Diff yalnızca `db/*` ve plan; çalışma ağacı temiz |
| K8 çalışma zamanı | ✘ | Betik olduğu gibi çalıştırıldığında 4 hata (aşağıda). `Upgrade` değişkeni verilmezse (`'Upgrade' scripting variable not defined`) ve `Upgrade=5` (`Upgrade must be 0, 7 or 8`) ile doğru şekilde reddediyor |

- **Bulgular (hepsi engelleyici, önem sırasıyla):**
  1. `db/002_bot_characters.sql:240-242`: `@strItem + REVERSE(CAST(... AS varbinary(n)))` → `Msg 402: The data types varbinary and varchar are incompatible in the add operator`. `REVERSE()` ikili girdide `varchar` döndürür.
  2. `:254, :268`: `REPLICATE(0x00, N)` de `varchar` döndürür → `Msg 257: Implicit conversion from data type varchar to binary is not allowed` (6 yerde: `WarehouseData`, `strSerial` (warehouse), `strSerial`, `strQuest`, `strItemTime`).
  3. `:285, :334`: `u.Strong + u.Sta + ...` `tinyint` toplamı 255'i aşınca `Msg 8115: Arithmetic overflow converting expression to data type tinyint` (değişmez doğrulaması çalışırken).
  4. `:346-`: `CROSS APPLY` içindeki `SUM(CASE WHEN ... u.strItem ...)` → `Msg 8124: Multiple columns are specified in an aggregated expression containing an outer reference`.
  5. **Not (sapma 1):** `Knights` sütununun listeden çıkarılması kabul: sütunun varsayılanı 0 (`Knights smallint DEFAULT 0`), sonuç aynı. Gerekçe (yasak grep) zayıf ama zararsız.
- **Doğrulama yöntemi (kanıt):** Bu dört düzeltme geçici bir prototip kopyasında (depoya girmedi) uygulandı ve gerçek `FDP_kn_online` üzerinde çalıştırıldı: 12 bot oluştu (stat 577, skill 142, `equipped/bag`: MF, MI 12/7; WP 12/6; WG, PHD, PHB 13/6); `strSkill` 10 bayt ve beklenen ağaç baytları; `strItem` 584 bayt, yuvalar planla birebir; `strSerial`/`strQuest`/`strItemTime` 584/600/584 bayt sıfır; `LOAD_USER_DATA` bir bot için sunucunun beklediği 38 sütunlu satırı döndürdü (sınıf 106, Level 80, Zone 71, `Exp 1898706631`); betik ikinci kez çalıştırıldığında aynı sonuç (idempotent); rollback sonrası bot ve diğer satır sayıları ilk durumla aynı (`USERDATA` 6, `ACCOUNT_CHAR` 4, `WAREHOUSE` 4). Ardından bot adlı sahte satırla sahiplik korumaları sınandı (uygulama ve rollback reddetti) ve veritabanı başlangıç durumuna döndürüldü (bot satırı 0).
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
plans/F1-04-bot-karakter-kurulum-betigi.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:

0. BU TURDA VERITABANINA BAĞLANMAN SERBEST (proje sahibi izni, 2026-10-02; main'deki güncel AGENTS.md §2.7 de böyle der, bu dalda eski metin kalmıştır). Plan §3/§8'deki "DB'ye bağlanma" maddeleri bu tur için geçersizdir. Bağlantı: SQLCMD="/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"; "$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_kn_online ... (betik yolu için wslpath -w). Yalnızca 12 bot satırına yaz; başka tablo/satıra yazma; .fdp_sql_password dosyasını okuma.

1. db/002_bot_characters.sql satır 240-242: her REVERSE(...) ifadesini dıştan CAST ile sar (REVERSE varchar döndürür): "+ CAST(REVERSE(CAST(@itemID AS varbinary(4))) AS varbinary(4))", "+ CAST(REVERSE(CAST(@dur AS varbinary(2))) AS varbinary(2))", "+ CAST(REVERSE(CAST(@cnt AS varbinary(2))) AS varbinary(2));".
2. Aynı dosya satır 254 ve 268: her REPLICATE(0x00, N) ifadesini CONVERT(varbinary(N), REPLICATE(CAST(0x00 AS varchar(1)), N)) ile değiştir (N = 1536, 1536, 584, 600, 584 — toplam 5 yer; satır 254'te iki, satır 268'de üç tane).
3. Aynı dosya satır 285 ve 334: "(u.Strong + u.Sta + u.Dex + u.Intel + u.Cha)" ifadesini "(CAST(u.Strong AS int) + u.Sta + u.Dex + u.Intel + u.Cha)" yap (tinyint toplamı taşıyor).
4. Aynı dosya satır 346 civarı: sonuç sorgusundaki CROSS APPLY bloğunu, toplamayı dış sorguya taşıyarak yeniden yaz: iç tabloda "SELECT n.slot, CAST(SUBSTRING(u.strItem, n.slot * 8 + 1, 4) AS int) AS id FROM (VALUES (0),...,(41)) AS n(slot)" (u dış referans yalnızca burada), dışında "SELECT SUM(CASE WHEN d.slot < 14 AND d.id <> 0 THEN 1 ELSE 0 END) AS equipped_items, SUM(CASE WHEN d.slot >= 14 AND d.id <> 0 THEN 1 ELSE 0 END) AS bag_items FROM (<iç tablo>) AS d". Sütun adları (equipped_items, bag_items) ve ORDER BY aynı kalsın.
5. Çalıştır ve çıktıları rapora yapıştır (kırpma): (a) önce bot olmayan satır sayıları: SELECT (SELECT COUNT(*) FROM USERDATA WHERE strUserID NOT LIKE 'Bot%'), (SELECT COUNT(*) FROM ACCOUNT_CHAR WHERE strAccountID NOT LIKE 'BotAcc%'), (SELECT COUNT(*) FROM WAREHOUSE WHERE strAccountID NOT LIKE 'BotAcc%'); (b) sqlcmd -b -v Upgrade=7 -i db/002_bot_characters.sql — 12 satırlık sonuç, beklenen equipped_items/bag_items: BotMF_*/BotMI_* 12/7, BotWP_* 12/6, BotWG_*/BotPHD_*/BotPHB_* 13/6, stat_sum 577, skill_sum 142; (c) aynı komut ikinci kez (aynı sonuç, hata yok); (d) sqlcmd -b -i db/002_bot_characters_rollback.sql (remaining_* = 0) ve (a) sayılarının aynı çıkması; (e) rollback ikinci kez (hata yok); (f) sqlcmd -b -i db/002_bot_characters_rollback.sql sonrası DB temiz kalsın (bot satırı 0).
6. Bayt doğrulaması: betiği tekrar uygulayıp (Upgrade=7) bir bot için strItem ve strSkill'i şu sorguyla oku: SELECT CONVERT(varchar(1200), strItem, 2), CONVERT(varchar(20), CAST(strSkill AS varbinary(10)), 2), DATALENGTH(strItem), DATALENGTH(strSkill) FROM USERDATA WHERE strUserID = 'BotWP_K' (sqlcmd'de -W kullan, -y ile birlikte kullanma: birbirini dışlar). Python ile 73 yuvayı struct.unpack('<IhH', ...) ile çöz; dolu yuvaları ve strSkill baytlarını (BotWP_K için 0,0,0,0,0,70,0,52,20,0) rapora yaz ve plan §5.1 ile karşılaştır. Sonra rollback çalıştırıp DB'yi temiz bırak.
7. Raporuna "Tur 2" ekle; geçici dosyaları (/tmp/opencode altında) depoya ekleme; Durum satırını UYGULANDI yap. Başka dosyaya dokunma.
```
