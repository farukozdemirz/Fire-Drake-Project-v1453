# F1-04: Level 80 bot karakter kurulum betiği (12 karakter) ve geri alma betiği

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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
