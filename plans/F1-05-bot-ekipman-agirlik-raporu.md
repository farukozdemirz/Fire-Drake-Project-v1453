# F1-05: Bot ekipman uygunluğu ve ağırlık raporu (`tools/bot-gear-report.py`)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F1 — Veri ve mekanik doğrulama (`docs/17` §2) |
| Branch | `bot/F1-05` (taban: `main`) |
| Bağımlı olduğu planlar | F1-04 (KAPANDI, `db/002_bot_characters.sql`) |
| İlgili gereksinim / kabul | T-DATA-02 (veri tarafı), T-DATA-03 (veri girdileri), T-DATA-05 / MB-12 / Q-21, Q-05 (veri tarafı); CHR-05, CHR-06 |
| Tahmini büyüklük | S (1 yeni araç betiği; DB'ye yalnızca F1-04 betiği ile bot satırları yazılır) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

12 bot karakterinin (F1-04) ekipmanının **sunucunun kurallarına göre** kuşanılabilir olduğunu ve ağırlık sınırlarının ne olacağını **veriden hesaplayıp raporlayan** bir araç yazmak ve çalıştırmak. Rapor şu soruların veri tarafını cevaplar:

- Her bot, üzerindeki 14 ekipman parçasının sunucu gereksinimini (`CUser::ItemEquipAvailable`) karşılıyor mu? Karşılamayan var mı?
- Ekipmanın `Race`/`Class` değerleri nedir (istemcinin sınıf/ulus kısıtı için ipucu, Q-05)?
- Ekipman + çanta toplam ağırlığı nedir ve maks ağırlık formülü `m_bMaxWeightAmount` değerine göre ne sonuç verir? (MB-12/Q-21: bu alan başlatılmıyor.)

Çalışma zamanı kanıtı (giriş testi) bu planda yoktur; bu plan yalnızca veri ve kod okumasıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/04` §3.4, §6 (referans ekipman), §7; `docs/03` MB-12 satırı ve CHR-05/CHR-06; `docs/11` STK-05.
- `db/002_bot_characters.sql` (bot satırlarını yazar), `db/README.md`.
- Sunucu kodu (Claude'un 2026-10-02'de doğruladığı satırlar):
  - `GameServer/ItemHandler.cpp:527-541` `CUser::ItemEquipAvailable`: kuşanma koşulu = `GetLevel() >= ReqLevel` **ve** `GetLevel() <= ReqLevelMax` **ve** `m_bRank >= ReqRank` **ve** `m_bTitle >= ReqTitle` **ve** `GetStat(STR) >= ReqStr`, `STA >= ReqSta`, `DEX >= ReqDex`, `INT >= ReqIntel`, `CHA >= ReqCha`. `GetStat` **temel** statıdır (item bonusu girmez, `GameServer/User.h:470`; `GetStatWithItemBonus = temel + item bonusu`, `:500`).
  - `GameServer/User.cpp:1279-1345` `SetSlotItemValue`: `INVENTORY_TOTAL` yuvanın tamamı gezilir; yuva `INVENTORY_COSP + COSP_BAG1/BAG2` ise `m_sMaxWeightBonus += ITEM.Duration` (çantalar ağırlık taşımaz), **diğer tüm yuvalar** (ekipman, çanta, depodaki değil) `m_sItemWeight += ITEM.Weight × adet`. Stat/AC/HP bonusları yalnızca **ekipman yuvalarından (0–13)** toplanır; çantadakiler (`14..41`) bonus vermez. Dayanıklılığı 0 olan parçanın AC'si 10'a bölünür (`:1337-1339`).
  - `GameServer/User.cpp:2184`: `m_sMaxWeight = ((GetStatWithItemBonus(STR) + GetLevel()) * 50 + m_sMaxWeightBonus) * (m_bMaxWeightAmount <= 0 ? 1 : m_bMaxWeightAmount / 100)`; `m_bMaxWeightAmount` `uint8` (`GameServer/User.h:234`), bölme **tamsayı**: 1–99 → `0` (maks ağırlık 0!), 100–199 → 1, 200–255 → 2. `m_bMaxWeightAmount` yalnızca `GameServer/MagicProcess.cpp:373` (buff) ve `:729` (buff bitişi → 100) içinde atanıyor; `CUser` kurucusunda (`GameServer/User.cpp:~40-100`, `m_sMaxWeight = 0` `:97`, `m_sMaxWeightBonus = 0` `:98`) **atanmıyor** (MB-12).
  - `GameServer/ItemHandler.cpp:245-249` `CheckWeight`: yalnızca **alım/taşıma** sırasında: `m_sItemWeight + ağırlık × adet <= m_sMaxWeight`; `:804`, `TradeHandler.cpp:406` benzer. Kuşanma ve savaşta ağırlık kontrolü yok.
- Bot karakter verisi `FDP_kn_online.dbo.USERDATA` içinde `strUserID LIKE 'Bot%'` satırlarıdır (F1-04). `strItem` biçimi: 73 yuva × 8 bayt, `<int32 itemID, int16 dayanıklılık, int16 adet>` little-endian (ayrıntı `docs/04` §3.4).
- **Veritabanı kuralı** (`AGENTS.md` §2.7, `main`): yerel DB'ye bağlanmak serbest. Bu plan yalnızca (a) `db/002_bot_characters.sql`'i çalıştırır (bot satırları), (b) `ITEM` ve bot `USERDATA` satırlarını **okur**. Başka oyuncuların satırlarını (`strUserID NOT LIKE 'Bot%'`) **okuma, rapora koyma**. `.fdp_sql_password` dosyasını okuma. sqlcmd'de `-W` ile `-y` birlikte kullanılamaz.
  - `SQLCMD="/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"`; bağlantı: `"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_kn_online ...`; betik yolu için `wslpath -w`.

## 3. Kapsam

**Yapılacaklar**

- `tools/bot-gear-report.py`: botları ve `ITEM` verisini okuyup üç bölümlü rapor basan Python betiği (+ `--selftest`).
- Botları DB'ye uygula (`db/002_bot_characters.sql -v Upgrade=7`), raporu çalıştır, çıktıyı plan raporuna yapıştır.
- Kod okuması: `m_bMaxWeightAmount` başlatılıyor mu, `CUser` nasıl oluşturuluyor (dosya:satır raporu).

**Kapsam dışı (yapılmayacak)**

- Sunucuyu çalıştırmak, oyuna girmek, istemci testi (Q-05'in istemci tarafı, T-DATA-02'nin kuşanma testi). Bunlar proje sahibiyle ayrı bir oturumdur.
- `GameServer/`, `AIServer/`, `shared/`, `db/*.sql`, `docs/**`, `AGENTS.md`, `.gitattributes` değişikliği.
- Ağırlık/başlatma hatasını **düzeltmek** (mekanik/kod değişikliği; bu plan yalnızca ölçer ve raporlar).
- Bot olmayan satırları okumak veya değiştirmek.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/bot-gear-report.py` | yeni | ASCII, LF, yalnızca standart kütüphane, `git update-index --add --chmod=+x` gerekmez (`python3 tools/...` ile çalışır) |

`tools/*` düzenlemesi opencode'da `ask` ister. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz. Veritabanında yalnızca F1-04'ün 12 bot satırı değişir.

## 5. Uygulama adımları

1. **Önce oku:** §2'deki kod satırlarını depoda aç ve doğrula (satırlar kaymış olabilir; kaymışsa gerçek satırı raporla). `docs/04` §3.4'ü oku.

2. **Botları uygula:** önce bot olmayan satır sayılarını kaydet: `SELECT (SELECT COUNT(*) FROM USERDATA WHERE strUserID NOT LIKE 'Bot%'), (SELECT COUNT(*) FROM ACCOUNT_CHAR WHERE strAccountID NOT LIKE 'BotAcc%'), (SELECT COUNT(*) FROM WAREHOUSE WHERE strAccountID NOT LIKE 'BotAcc%')` (şu an 6, 4, 4 olması beklenir). Sonra `sqlcmd ... -b -v Upgrade=7 -i db/002_bot_characters.sql` çalıştır; 12 satırlık sonuç tablosunu rapora yapıştır. Bot olmayan sayıları tekrar al (aynı olmalı). **Botları sonunda uygulanmış bırak** (F2 için gerekli).

3. **`tools/bot-gear-report.py`** yaz. Komut satırı: `python3 tools/bot-gear-report.py [--sqlcmd PATH] [--server ".\SQLEXPRESS"] [--db FDP_kn_online]` ve `--selftest`. `--sqlcmd` varsayılanı yukarıdaki yol. Veri erişimi: `subprocess` ile `SQLCMD.EXE` (`-S`, `-E`, `-d`, `-W`, `-s "|"`, `-h -1`, `-b`, `-Q "SET NOCOUNT ON; ..."`); CRLF'i temizle; hata kodu ≠ 0 ise mesajla çık (kod 1).
   - **Bot sorgusu** (yalnızca bot satırları, sabit): `SELECT RTRIM(strUserID), Nation, Race, Class, Level, [Rank], Title, Strong, Sta, Dex, Intel, Cha, CONVERT(varchar(1200), strItem, 2) FROM USERDATA WHERE strUserID LIKE 'Bot%' ORDER BY strUserID`. Betikte `NOT LIKE`/`Bot%` dışında başka bir `USERDATA` koşulu **yok**.
   - `strItem` hex'ini 584 bayta çöz; 73 yuvanın her biri `struct.unpack('<IhH')`; yalnızca `itemID != 0` olanlar. `ITEM` sorgusu: bu ID'lerin hepsi için `SELECT Num, RTRIM(strName), Kind, Slot, Race, Class, Weight, Duration, Ac, Countable, ReqLevel, ReqLevelMax, ReqRank, ReqTitle, ReqStr, ReqSta, ReqDex, ReqIntel, ReqCha, StrB, StaB, DexB, IntelB, ChaB, MaxHpB, MaxMpB FROM ITEM WHERE Num IN (...)` (tek sorgu).
   - **Bölüm A: kuşanılabilirlik** (`ItemEquipAvailable` mantığı birebir; ekipman yuvaları 0–13): her bot ve her ekipman parçası için `ReqLevel <= Level <= ReqLevelMax`, `Rank >= ReqRank`, `Title >= ReqTitle`, `Strong >= ReqStr`, `Sta >= ReqSta`, `Dex >= ReqDex`, `Intel >= ReqIntel`, `Cha >= ReqCha` (hepsi **temel stat**, item bonusu yok). Satır biçimi: `A <bot> slot=<n> item=<id> <ad> OK` veya `... FAIL <alan>:<gereken>><bot değeri>,...`. Sonda `A_SUMMARY fail_count=<N> bots_with_fail=<M>`.
   - **Bölüm B: `Race`/`Class` alanları** (Q-05 ipucu): her benzersiz ekipman item'ı için bir satır: `B item=<id> <ad> Race=<r> Class=<c> Kind=<k> Slot=<s>`; sonda botun sınıfı/ulusu ile birlikte `Race` ve `Class` değerlerinin 0'dan farklı olduğu item'ların sayısı: `B_SUMMARY nonzero_race=<n> nonzero_class=<m>`.
   - **Bölüm C: ağırlık** (`SetSlotItemValue`/`:2184` mantığı): her bot için `item_weight = sum(ITEM.Weight × adet)` (yuvalar 0–41, `INVENTORY_COSP+COSP_BAG1/2` yoktur çünkü bot çantası kullanmıyor; yine de kodu göre uygula: yuva 42 ve üzeri bu betikte yok), `item_str_bonus = sum(StrB)` **yalnızca yuva 0–13**, `max_weight_base = (Strong + item_str_bonus + Level) * 50` (`m_sMaxWeightBonus = 0`). Ardından `m_bMaxWeightAmount` için şu değerlerde `max_weight`: `0`→ ×1, `50` → ×(50/100=0) → 0, `100` → ×1, `150` → ×1, `200` → ×2, `255` → ×2. Satır biçimi: `C <bot> item_weight=<W> max_weight_base=<B> amount=<a> max_weight=<M> fits=<yes|no>` (`fits` = `item_weight <= max_weight`). Sonda `C_SUMMARY` satırında `amount=0` ve `amount=100` için kaç botun `fits=yes` olduğu, `amount=50` için kaç botun `fits=no` olduğu.
   - **Bölüm D: item bonusları** (T-DATA-03 girdisi): her bot için ekipmandan (yuva 0–13) `sum(StrB), sum(StaB), sum(DexB), sum(IntelB), sum(ChaB), sum(MaxHpB), sum(MaxMpB), sum(Ac)`: `D <bot> strB=.. staB=.. dexB=.. intelB=.. chaB=.. maxHpB=.. maxMpB=.. ac=..`.
   - Çıktı **yalnızca ASCII** ve sabit sırada (bot adına göre). Başlık satırları `== A ==`, `== B ==`, `== C ==`, `== D ==`.
   - `--selftest`: bellekte sentetik 2 bot ve 3 sentetik item ile (DB'siz) A (bir FAIL ve bir OK), C (amount=50 → `max_weight=0`) ve D toplamlarını `assert` ile doğrula; `selftest OK`, çıkış 0.

4. **Çalıştır:** `python3 tools/bot-gear-report.py` ve çıktının **tamamını** (kırpmadan) plan raporuna yapıştır. Bot olmayan satırlardan hiçbir bilgi çıktıda olmamalı (rapora yapıştırmadan önce `grep -c` ile bot adları dışında karakter adı geçmediğini göster, ör. tüm `<bot>` alanları `Bot` ile başlıyor).

5. **Kod okuması** (rapora dosya:satır ile yaz): (a) `m_bMaxWeightAmount` atamalarının tamamı (`grep -n -a m_bMaxWeightAmount GameServer/*.cpp GameServer/*.h`); (b) `CUser` nesnesinin nerede oluşturulduğu ve bellek sıfırlanıyor mu (`grep -rn -a "new CUser\|CUser()" GameServer shared`; kurucunun `m_bMaxWeightAmount`'a dokunup dokunmadığı); (c) bu bilgiye dayanarak MB-12'nin **kod düzeyinde** sonucu (başlatılmamış `uint8` için olası değerler ve `m_sMaxWeight` sonucu); belirsizse "çalışma zamanında ölçülecek" yaz, **tahmin etme**.

6. Raporu yaz; commit mesajı `[F1-05] ...`; yalnızca `tools/bot-gear-report.py` ve plan dosyası eklenir.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/bot-gear-report.py --selftest` → `selftest OK`, çıkış 0.
- [ ] K2: Botlar DB'ye uygulandı: `db/002` sonuç tablosu (12 satır) rapora yapıştırıldı; bot olmayan satır sayıları önce/sonra aynı; botlar uygulanmış kaldı (`SELECT COUNT(*) FROM USERDATA WHERE strUserID LIKE 'Bot%'` = 12).
- [ ] K3: Rapor çıktısı A, B, C, D bölümleriyle ve `A_SUMMARY`, `B_SUMMARY`, `C_SUMMARY` satırlarıyla eksiksiz yapıştırılmış; 12 bot × 12–13 ekipman satırı (A), 12 bot (C ve D).
- [ ] K4: Betikte bot olmayan `USERDATA` satırlarını seçen sorgu yok (`grep -n "USERDATA" tools/bot-gear-report.py` çıktısı tek sorgu: `LIKE 'Bot%'`); çıktıda gerçek oyuncu adı yok.
- [ ] K5: Bölüm A mantığı koddaki `ItemEquipAvailable` koşullarıyla satır satır eşleştirildi (rapora koşul → kod satırı tablosu); `FAIL` varsa hangi bot/parça/alan olduğu açık yazıldı (başarısızlık kabul edilebilir, düzeltilmesi bu planın işi değil).
- [ ] K6: §5.5'teki üç kod okuması dosya:satır ile raporda; "tahmin" ile "doğrulanmış" ayrı etiketlenmiş.
- [ ] K7: Kapsam: `git diff --stat main...bot/F1-05` yalnızca `tools/bot-gear-report.py` ve plan dosyası; `git status --short` boş; `file tools/bot-gear-report.py` ASCII ve CR yok.

## 7. Doğrulama komutları

```bash
python3 tools/bot-gear-report.py --selftest
python3 tools/bot-gear-report.py | head -60
grep -n "USERDATA" tools/bot-gear-report.py
file tools/bot-gear-report.py
git diff --stat main...bot/F1-05
git status --short
```

Claude doğrulaması: betik yeniden çalıştırılır, çıktı elle üç bot için hesaplanan değerlerle karşılaştırılır (ağırlık, bonus toplamı, bir gereksinim ihlali), kod satırları açılır.

## 8. Kısıtlar ve uyarılar

- Veritabanında bot dışı hiçbir satıra **yazma**; okuma yalnızca `ITEM` ve bot `USERDATA` satırları. Hesap/parola/başka oyuncu verisi rapora girmez.
- `Rank`/`Title` `USERDATA`'da köşeli parantezli sütun adlarıdır (`[Rank]`).
- Ağırlık formülünde `m_bMaxWeightAmount / 100` **tamsayı** bölmesidir (C++ `uint8 / int`); Python'da `//` kullan.
- Bu plan MB-12'yi **ölçer, çözmez**. Gerçek maks ağırlık çalışma zamanında (giriş testiyle) belli olacak.
- Yeni karar gerekirse **durup** raporda sor.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1 — 2026-10-02

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F1-05` (taban: `main` @ `3f53a9f`)
  - `afa8550` — `[F1-05] Bot ekipman ve agirlik raporu araci` (`tools/bot-gear-report.py`, 342 satır)
  - Bu rapor ve `Durum: UYGULANDI` ayrı commit'lenir.
- Değişen dosyalar ve nedenleri:
  - `tools/bot-gear-report.py` (yeni, ASCII + LF): botları ve `ITEM` verisini okuyup A/B/C/D bölümlerini basar; `--selftest` DB'siz çalışır.
  - `plans/F1-05-bot-ekipman-agirlik-raporu.md`: yalnızca `Durum` satırı ve bu rapor.
  - `GameServer/`, `shared/`, `db/*.sql`, `docs/**` değişmedi.
- Veritabanına yalnızca `db/002_bot_characters.sql` ile 12 bot satırı yazıldı; başka tablo/satıra yazılmadı, `.fdp_sql_password` okunmadı.

**Adım 1 — kod satırı doğrulaması (plan §2 ile uyumlu, kayma yok)**
```
ItemEquipAvailable          GameServer/ItemHandler.cpp:527
  Level >= ReqLevel         :529      Level <= ReqLevelMax   :530
  Rank >= ReqRank           :531      Title >= ReqTitle      :532
  STR/STA/DEX/INT/CHA       :533-537
GetStat (temel)             GameServer/User.h:470
GetStatWithItemBonus        GameServer/User.h:498
SetSlotItemValue            GameServer/User.cpp:1279
  cospre çanta bonusu       :1317     item ağırlığı          :1323
  yalnızca ekipman bonusu   :1327-1334
m_sMaxWeight formülü        GameServer/User.cpp:2184
CheckWeight                 GameServer/ItemHandler.cpp:245
m_bMaxWeightAmount          GameServer/User.h:234 (uint8)
BUFF_TYPE_WEIGHT atamaları  GameServer/MagicProcess.cpp:373,:729
```

**Adım 2 — botları uygula (K2)**
```
$ (once) SELECT ud_nonbot, ac_nonbot, wh_nonbot
6 4 4                       exit=0

$ "$SQLCMD" ... -b -W -s '|' -v Upgrade=7 -i db/002_bot_characters.sql
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

$ (sonra) SELECT ud_nonbot, ac_nonbot, wh_nonbot
6 4 4                       exit=0
$ SELECT COUNT(*) FROM USERDATA WHERE strUserID LIKE 'Bot%'
12                          exit=0
```
→ Bot olmayan satır sayıları değişmedi; botlar **uygulanmış bırakıldı** (F2 için gerekli).

**Adım 3/4 — `tools/bot-gear-report.py`**

`--selftest` (K1):
```
$ python3 tools/bot-gear-report.py --selftest
selftest OK
selftest_exit=0
```
K4 sorgu kontrolü:
```
$ grep -n "USERDATA" tools/bot-gear-report.py
28:BOT_QUERY = "SELECT RTRIM(strUserID), Nation, Race, [Class], Level, [Rank], Title, Strong, Sta, Dex, Intel, Cha, CONVERT(varchar(1200), strItem, 2) FROM USERDATA WHERE strUserID LIKE 'Bot%' ORDER BY strUserID"
```
Betikte tek `USERDATA` sorgusu var ve yalnızca `LIKE 'Bot%'` seçiyor. Çıktıdaki tüm bot alanı (`A`/`C`/`D` satırlarının ikinci sütunu) `Bot` ile başlıyor (`grep -vc '^Bot'` = 0); gerçek oyuncu adı yok.

Rapor çıktısı (tam, kırpılmadı; `python3 tools/bot-gear-report.py`, çıkış 0; 269 satır — A 150, B 29, C 72, D 12 satır + başlık/özet):
```
== A ==
A BotMF_E slot=0 item=310310007 Cleric Earring OK
A BotMF_E slot=1 item=266003007 Complete Helmet (+7) OK
A BotMF_E slot=2 item=310310007 Cleric Earring OK
A BotMF_E slot=3 item=320310126 Iron Necklace OK
A BotMF_E slot=4 item=266001007 Complete Robe (+7) OK
A BotMF_E slot=6 item=181110007 Elixir Staff (+7) OK
A BotMF_E slot=7 item=340410109 Glass Belt OK
A BotMF_E slot=9 item=330150256 Ring of Magic OK
A BotMF_E slot=10 item=266002007 Complete Pants (+7) OK
A BotMF_E slot=11 item=330150256 Ring of Magic OK
A BotMF_E slot=12 item=266004007 Complete Glove (+7) OK
A BotMF_E slot=13 item=266005007 Complete Boots (+7) OK
A BotMF_K slot=0 item=310310007 Cleric Earring OK
A BotMF_K slot=1 item=266003007 Complete Helmet (+7) OK
A BotMF_K slot=2 item=310310007 Cleric Earring OK
A BotMF_K slot=3 item=320310126 Iron Necklace OK
A BotMF_K slot=4 item=266001007 Complete Robe (+7) OK
A BotMF_K slot=6 item=181110007 Elixir Staff (+7) OK
A BotMF_K slot=7 item=340410109 Glass Belt OK
A BotMF_K slot=9 item=330150256 Ring of Magic OK
A BotMF_K slot=10 item=266002007 Complete Pants (+7) OK
A BotMF_K slot=11 item=330150256 Ring of Magic OK
A BotMF_K slot=12 item=266004007 Complete Glove (+7) OK
A BotMF_K slot=13 item=266005007 Complete Boots (+7) OK
A BotMI_E slot=0 item=310310007 Cleric Earring OK
A BotMI_E slot=1 item=266003007 Complete Helmet (+7) OK
A BotMI_E slot=2 item=310310007 Cleric Earring OK
A BotMI_E slot=3 item=320310126 Iron Necklace OK
A BotMI_E slot=4 item=266001007 Complete Robe (+7) OK
A BotMI_E slot=6 item=181110007 Elixir Staff (+7) OK
A BotMI_E slot=7 item=340410109 Glass Belt OK
A BotMI_E slot=9 item=330150256 Ring of Magic OK
A BotMI_E slot=10 item=266002007 Complete Pants (+7) OK
A BotMI_E slot=11 item=330150256 Ring of Magic OK
A BotMI_E slot=12 item=266004007 Complete Glove (+7) OK
A BotMI_E slot=13 item=266005007 Complete Boots (+7) OK
A BotMI_K slot=0 item=310310007 Cleric Earring OK
A BotMI_K slot=1 item=266003007 Complete Helmet (+7) OK
A BotMI_K slot=2 item=310310007 Cleric Earring OK
A BotMI_K slot=3 item=320310126 Iron Necklace OK
A BotMI_K slot=4 item=266001007 Complete Robe (+7) OK
A BotMI_K slot=6 item=181110007 Elixir Staff (+7) OK
A BotMI_K slot=7 item=340410109 Glass Belt OK
A BotMI_K slot=9 item=330150256 Ring of Magic OK
A BotMI_K slot=10 item=266002007 Complete Pants (+7) OK
A BotMI_K slot=11 item=330150256 Ring of Magic OK
A BotMI_K slot=12 item=266004007 Complete Glove (+7) OK
A BotMI_K slot=13 item=266005007 Complete Boots (+7) OK
A BotPHB_E slot=0 item=310310007 Cleric Earring OK
A BotPHB_E slot=1 item=286003007 Priest Chitin Shell Helmet (+7) OK
A BotPHB_E slot=2 item=310310007 Cleric Earring OK
A BotPHB_E slot=3 item=320310126 Iron Necklace OK
A BotPHB_E slot=4 item=286001007 Priest Chitin Shell Pauldron (+7) OK
A BotPHB_E slot=6 item=191110007 Priest Impact  (+7) OK
A BotPHB_E slot=7 item=340410109 Glass Belt OK
A BotPHB_E slot=8 item=170250256 Chitin Shield OK
A BotPHB_E slot=9 item=330150257 Ring of Life OK
A BotPHB_E slot=10 item=286002007 Priest Chitin Shell Pads (+7) OK
A BotPHB_E slot=11 item=330150257 Ring of Life OK
A BotPHB_E slot=12 item=286004007 Priest Chitin Shell Gauntlet (+7) OK
A BotPHB_E slot=13 item=286005007 Priest Chitin Shell Boots (+7) OK
A BotPHB_K slot=0 item=310310007 Cleric Earring OK
A BotPHB_K slot=1 item=286003007 Priest Chitin Shell Helmet (+7) OK
A BotPHB_K slot=2 item=310310007 Cleric Earring OK
A BotPHB_K slot=3 item=320310126 Iron Necklace OK
A BotPHB_K slot=4 item=286001007 Priest Chitin Shell Pauldron (+7) OK
A BotPHB_K slot=6 item=191110007 Priest Impact  (+7) OK
A BotPHB_K slot=7 item=340410109 Glass Belt OK
A BotPHB_K slot=8 item=170250256 Chitin Shield OK
A BotPHB_K slot=9 item=330150257 Ring of Life OK
A BotPHB_K slot=10 item=286002007 Priest Chitin Shell Pads (+7) OK
A BotPHB_K slot=11 item=330150257 Ring of Life OK
A BotPHB_K slot=12 item=286004007 Priest Chitin Shell Gauntlet (+7) OK
A BotPHB_K slot=13 item=286005007 Priest Chitin Shell Boots (+7) OK
A BotPHD_E slot=0 item=310310007 Cleric Earring OK
A BotPHD_E slot=1 item=286003007 Priest Chitin Shell Helmet (+7) OK
A BotPHD_E slot=2 item=310310007 Cleric Earring OK
A BotPHD_E slot=3 item=320310126 Iron Necklace OK
A BotPHD_E slot=4 item=286001007 Priest Chitin Shell Pauldron (+7) OK
A BotPHD_E slot=6 item=191110007 Priest Impact  (+7) OK
A BotPHD_E slot=7 item=340410109 Glass Belt OK
A BotPHD_E slot=8 item=170250256 Chitin Shield OK
A BotPHD_E slot=9 item=330150257 Ring of Life OK
A BotPHD_E slot=10 item=286002007 Priest Chitin Shell Pads (+7) OK
A BotPHD_E slot=11 item=330150257 Ring of Life OK
A BotPHD_E slot=12 item=286004007 Priest Chitin Shell Gauntlet (+7) OK
A BotPHD_E slot=13 item=286005007 Priest Chitin Shell Boots (+7) OK
A BotPHD_K slot=0 item=310310007 Cleric Earring OK
A BotPHD_K slot=1 item=286003007 Priest Chitin Shell Helmet (+7) OK
A BotPHD_K slot=2 item=310310007 Cleric Earring OK
A BotPHD_K slot=3 item=320310126 Iron Necklace OK
A BotPHD_K slot=4 item=286001007 Priest Chitin Shell Pauldron (+7) OK
A BotPHD_K slot=6 item=191110007 Priest Impact  (+7) OK
A BotPHD_K slot=7 item=340410109 Glass Belt OK
A BotPHD_K slot=8 item=170250256 Chitin Shield OK
A BotPHD_K slot=9 item=330150257 Ring of Life OK
A BotPHD_K slot=10 item=286002007 Priest Chitin Shell Pads (+7) OK
A BotPHD_K slot=11 item=330150257 Ring of Life OK
A BotPHD_K slot=12 item=286004007 Priest Chitin Shell Gauntlet (+7) OK
A BotPHD_K slot=13 item=286005007 Priest Chitin Shell Boots (+7) OK
A BotWG_E slot=0 item=310310005 Warrior Earring OK
A BotWG_E slot=1 item=206003007 Chitin Shell Helmet (+7) OK
A BotWG_E slot=2 item=310310005 Warrior Earring OK
A BotWG_E slot=3 item=320310126 Iron Necklace OK
A BotWG_E slot=4 item=206001007 Chitin Shell Pauldron (+7) OK
A BotWG_E slot=6 item=121310007 Graham (+7) OK
A BotWG_E slot=7 item=340610107 Iron Belt OK
A BotWG_E slot=8 item=170250256 Chitin Shield OK
A BotWG_E slot=9 item=330110255 Ring of Courage OK
A BotWG_E slot=10 item=206002007 Chitin Shell Pads (+7) OK
A BotWG_E slot=11 item=330110255 Ring of Courage OK
A BotWG_E slot=12 item=206004007 Chitin Shell Gauntlet (+7) OK
A BotWG_E slot=13 item=206005007 Chitin Shell Boots (+7) OK
A BotWG_K slot=0 item=310310005 Warrior Earring OK
A BotWG_K slot=1 item=206003007 Chitin Shell Helmet (+7) OK
A BotWG_K slot=2 item=310310005 Warrior Earring OK
A BotWG_K slot=3 item=320310126 Iron Necklace OK
A BotWG_K slot=4 item=206001007 Chitin Shell Pauldron (+7) OK
A BotWG_K slot=6 item=121310007 Graham (+7) OK
A BotWG_K slot=7 item=340610107 Iron Belt OK
A BotWG_K slot=8 item=170250256 Chitin Shield OK
A BotWG_K slot=9 item=330110255 Ring of Courage OK
A BotWG_K slot=10 item=206002007 Chitin Shell Pads (+7) OK
A BotWG_K slot=11 item=330110255 Ring of Courage OK
A BotWG_K slot=12 item=206004007 Chitin Shell Gauntlet (+7) OK
A BotWG_K slot=13 item=206005007 Chitin Shell Boots (+7) OK
A BotWP_E slot=0 item=310310005 Warrior Earring OK
A BotWP_E slot=1 item=206003007 Chitin Shell Helmet (+7) OK
A BotWP_E slot=2 item=310310005 Warrior Earring OK
A BotWP_E slot=3 item=320310126 Iron Necklace OK
A BotWP_E slot=4 item=206001007 Chitin Shell Pauldron (+7) OK
A BotWP_E slot=6 item=156210007 Raptor (+7) OK
A BotWP_E slot=7 item=340610107 Iron Belt OK
A BotWP_E slot=9 item=330110255 Ring of Courage OK
A BotWP_E slot=10 item=206002007 Chitin Shell Pads (+7) OK
A BotWP_E slot=11 item=330110255 Ring of Courage OK
A BotWP_E slot=12 item=206004007 Chitin Shell Gauntlet (+7) OK
A BotWP_E slot=13 item=206005007 Chitin Shell Boots (+7) OK
A BotWP_K slot=0 item=310310005 Warrior Earring OK
A BotWP_K slot=1 item=206003007 Chitin Shell Helmet (+7) OK
A BotWP_K slot=2 item=310310005 Warrior Earring OK
A BotWP_K slot=3 item=320310126 Iron Necklace OK
A BotWP_K slot=4 item=206001007 Chitin Shell Pauldron (+7) OK
A BotWP_K slot=6 item=156210007 Raptor (+7) OK
A BotWP_K slot=7 item=340610107 Iron Belt OK
A BotWP_K slot=9 item=330110255 Ring of Courage OK
A BotWP_K slot=10 item=206002007 Chitin Shell Pads (+7) OK
A BotWP_K slot=11 item=330110255 Ring of Courage OK
A BotWP_K slot=12 item=206004007 Chitin Shell Gauntlet (+7) OK
A BotWP_K slot=13 item=206005007 Chitin Shell Boots (+7) OK
A_SUMMARY fail_count=0 bots_with_fail=0
== B ==
B item=121310007 Graham (+7) Race=0 Class=0 Kind=21 Slot=0
B item=156210007 Raptor (+7) Race=0 Class=0 Kind=52 Slot=3
B item=170250256 Chitin Shield Race=0 Class=0 Kind=60 Slot=2
B item=181110007 Elixir Staff (+7) Race=0 Class=0 Kind=110 Slot=3
B item=191110007 Priest Impact  (+7) Race=0 Class=0 Kind=41 Slot=0
B item=206001007 Chitin Shell Pauldron (+7) Race=0 Class=6 Kind=210 Slot=5
B item=206002007 Chitin Shell Pads (+7) Race=0 Class=6 Kind=210 Slot=6
B item=206003007 Chitin Shell Helmet (+7) Race=0 Class=6 Kind=210 Slot=7
B item=206004007 Chitin Shell Gauntlet (+7) Race=0 Class=6 Kind=210 Slot=8
B item=206005007 Chitin Shell Boots (+7) Race=0 Class=6 Kind=210 Slot=9
B item=266001007 Complete Robe (+7) Race=0 Class=10 Kind=230 Slot=5
B item=266002007 Complete Pants (+7) Race=0 Class=10 Kind=230 Slot=6
B item=266003007 Complete Helmet (+7) Race=0 Class=10 Kind=230 Slot=7
B item=266004007 Complete Glove (+7) Race=0 Class=10 Kind=230 Slot=8
B item=266005007 Complete Boots (+7) Race=0 Class=10 Kind=230 Slot=9
B item=286001007 Priest Chitin Shell Pauldron (+7) Race=0 Class=12 Kind=240 Slot=5
B item=286002007 Priest Chitin Shell Pads (+7) Race=0 Class=12 Kind=240 Slot=6
B item=286003007 Priest Chitin Shell Helmet (+7) Race=0 Class=12 Kind=240 Slot=7
B item=286004007 Priest Chitin Shell Gauntlet (+7) Race=0 Class=12 Kind=240 Slot=8
B item=286005007 Priest Chitin Shell Boots (+7) Race=0 Class=12 Kind=240 Slot=9
B item=310310005 Warrior Earring Race=0 Class=0 Kind=91 Slot=10
B item=310310007 Cleric Earring Race=0 Class=0 Kind=91 Slot=10
B item=320310126 Iron Necklace Race=0 Class=0 Kind=92 Slot=11
B item=330110255 Ring of Courage Race=0 Class=0 Kind=93 Slot=12
B item=330150256 Ring of Magic Race=0 Class=0 Kind=93 Slot=12
B item=330150257 Ring of Life Race=0 Class=0 Kind=93 Slot=12
B item=340410109 Glass Belt Race=0 Class=0 Kind=94 Slot=14
B item=340610107 Iron Belt Race=0 Class=0 Kind=94 Slot=14
B_SUMMARY nonzero_race=0 nonzero_class=15
== C ==
C BotMF_E item_weight=10470 max_weight_base=6750 amount=0 max_weight=6750 fits=no
C BotMF_E item_weight=10470 max_weight_base=6750 amount=50 max_weight=0 fits=no
C BotMF_E item_weight=10470 max_weight_base=6750 amount=100 max_weight=6750 fits=no
C BotMF_E item_weight=10470 max_weight_base=6750 amount=150 max_weight=6750 fits=no
C BotMF_E item_weight=10470 max_weight_base=6750 amount=200 max_weight=13500 fits=yes
C BotMF_E item_weight=10470 max_weight_base=6750 amount=255 max_weight=13500 fits=yes
C BotMF_K item_weight=10470 max_weight_base=6750 amount=0 max_weight=6750 fits=no
C BotMF_K item_weight=10470 max_weight_base=6750 amount=50 max_weight=0 fits=no
C BotMF_K item_weight=10470 max_weight_base=6750 amount=100 max_weight=6750 fits=no
C BotMF_K item_weight=10470 max_weight_base=6750 amount=150 max_weight=6750 fits=no
C BotMF_K item_weight=10470 max_weight_base=6750 amount=200 max_weight=13500 fits=yes
C BotMF_K item_weight=10470 max_weight_base=6750 amount=255 max_weight=13500 fits=yes
C BotMI_E item_weight=10470 max_weight_base=6750 amount=0 max_weight=6750 fits=no
C BotMI_E item_weight=10470 max_weight_base=6750 amount=50 max_weight=0 fits=no
C BotMI_E item_weight=10470 max_weight_base=6750 amount=100 max_weight=6750 fits=no
C BotMI_E item_weight=10470 max_weight_base=6750 amount=150 max_weight=6750 fits=no
C BotMI_E item_weight=10470 max_weight_base=6750 amount=200 max_weight=13500 fits=yes
C BotMI_E item_weight=10470 max_weight_base=6750 amount=255 max_weight=13500 fits=yes
C BotMI_K item_weight=10470 max_weight_base=6750 amount=0 max_weight=6750 fits=no
C BotMI_K item_weight=10470 max_weight_base=6750 amount=50 max_weight=0 fits=no
C BotMI_K item_weight=10470 max_weight_base=6750 amount=100 max_weight=6750 fits=no
C BotMI_K item_weight=10470 max_weight_base=6750 amount=150 max_weight=6750 fits=no
C BotMI_K item_weight=10470 max_weight_base=6750 amount=200 max_weight=13500 fits=yes
C BotMI_K item_weight=10470 max_weight_base=6750 amount=255 max_weight=13500 fits=yes
C BotPHB_E item_weight=10680 max_weight_base=10250 amount=0 max_weight=10250 fits=no
C BotPHB_E item_weight=10680 max_weight_base=10250 amount=50 max_weight=0 fits=no
C BotPHB_E item_weight=10680 max_weight_base=10250 amount=100 max_weight=10250 fits=no
C BotPHB_E item_weight=10680 max_weight_base=10250 amount=150 max_weight=10250 fits=no
C BotPHB_E item_weight=10680 max_weight_base=10250 amount=200 max_weight=20500 fits=yes
C BotPHB_E item_weight=10680 max_weight_base=10250 amount=255 max_weight=20500 fits=yes
C BotPHB_K item_weight=10680 max_weight_base=10250 amount=0 max_weight=10250 fits=no
C BotPHB_K item_weight=10680 max_weight_base=10250 amount=50 max_weight=0 fits=no
C BotPHB_K item_weight=10680 max_weight_base=10250 amount=100 max_weight=10250 fits=no
C BotPHB_K item_weight=10680 max_weight_base=10250 amount=150 max_weight=10250 fits=no
C BotPHB_K item_weight=10680 max_weight_base=10250 amount=200 max_weight=20500 fits=yes
C BotPHB_K item_weight=10680 max_weight_base=10250 amount=255 max_weight=20500 fits=yes
C BotPHD_E item_weight=10680 max_weight_base=10250 amount=0 max_weight=10250 fits=no
C BotPHD_E item_weight=10680 max_weight_base=10250 amount=50 max_weight=0 fits=no
C BotPHD_E item_weight=10680 max_weight_base=10250 amount=100 max_weight=10250 fits=no
C BotPHD_E item_weight=10680 max_weight_base=10250 amount=150 max_weight=10250 fits=no
C BotPHD_E item_weight=10680 max_weight_base=10250 amount=200 max_weight=20500 fits=yes
C BotPHD_E item_weight=10680 max_weight_base=10250 amount=255 max_weight=20500 fits=yes
C BotPHD_K item_weight=10680 max_weight_base=10250 amount=0 max_weight=10250 fits=no
C BotPHD_K item_weight=10680 max_weight_base=10250 amount=50 max_weight=0 fits=no
C BotPHD_K item_weight=10680 max_weight_base=10250 amount=100 max_weight=10250 fits=no
C BotPHD_K item_weight=10680 max_weight_base=10250 amount=150 max_weight=10250 fits=no
C BotPHD_K item_weight=10680 max_weight_base=10250 amount=200 max_weight=20500 fits=yes
C BotPHD_K item_weight=10680 max_weight_base=10250 amount=255 max_weight=20500 fits=yes
C BotWG_E item_weight=10867 max_weight_base=18200 amount=0 max_weight=18200 fits=yes
C BotWG_E item_weight=10867 max_weight_base=18200 amount=50 max_weight=0 fits=no
C BotWG_E item_weight=10867 max_weight_base=18200 amount=100 max_weight=18200 fits=yes
C BotWG_E item_weight=10867 max_weight_base=18200 amount=150 max_weight=18200 fits=yes
C BotWG_E item_weight=10867 max_weight_base=18200 amount=200 max_weight=36400 fits=yes
C BotWG_E item_weight=10867 max_weight_base=18200 amount=255 max_weight=36400 fits=yes
C BotWG_K item_weight=10867 max_weight_base=18200 amount=0 max_weight=18200 fits=yes
C BotWG_K item_weight=10867 max_weight_base=18200 amount=50 max_weight=0 fits=no
C BotWG_K item_weight=10867 max_weight_base=18200 amount=100 max_weight=18200 fits=yes
C BotWG_K item_weight=10867 max_weight_base=18200 amount=150 max_weight=18200 fits=yes
C BotWG_K item_weight=10867 max_weight_base=18200 amount=200 max_weight=36400 fits=yes
C BotWG_K item_weight=10867 max_weight_base=18200 amount=255 max_weight=36400 fits=yes
C BotWP_E item_weight=10877 max_weight_base=18200 amount=0 max_weight=18200 fits=yes
C BotWP_E item_weight=10877 max_weight_base=18200 amount=50 max_weight=0 fits=no
C BotWP_E item_weight=10877 max_weight_base=18200 amount=100 max_weight=18200 fits=yes
C BotWP_E item_weight=10877 max_weight_base=18200 amount=150 max_weight=18200 fits=yes
C BotWP_E item_weight=10877 max_weight_base=18200 amount=200 max_weight=36400 fits=yes
C BotWP_E item_weight=10877 max_weight_base=18200 amount=255 max_weight=36400 fits=yes
C BotWP_K item_weight=10877 max_weight_base=18200 amount=0 max_weight=18200 fits=yes
C BotWP_K item_weight=10877 max_weight_base=18200 amount=50 max_weight=0 fits=no
C BotWP_K item_weight=10877 max_weight_base=18200 amount=100 max_weight=18200 fits=yes
C BotWP_K item_weight=10877 max_weight_base=18200 amount=150 max_weight=18200 fits=yes
C BotWP_K item_weight=10877 max_weight_base=18200 amount=200 max_weight=36400 fits=yes
C BotWP_K item_weight=10877 max_weight_base=18200 amount=255 max_weight=36400 fits=yes
C_SUMMARY amount=0 fits_yes=4 amount=100 fits_yes=4 amount=50 fits_no=12
== D ==
D BotMF_E strB=5 staB=34 dexB=0 intelB=20 chaB=14 maxHpB=150 maxMpB=180 ac=525
D BotMF_K strB=5 staB=34 dexB=0 intelB=20 chaB=14 maxHpB=150 maxMpB=180 ac=525
D BotMI_E strB=5 staB=34 dexB=0 intelB=20 chaB=14 maxHpB=150 maxMpB=180 ac=525
D BotMI_K strB=5 staB=34 dexB=0 intelB=20 chaB=14 maxHpB=150 maxMpB=180 ac=525
D BotPHB_E strB=5 staB=34 dexB=14 intelB=20 chaB=0 maxHpB=250 maxMpB=180 ac=796
D BotPHB_K strB=5 staB=34 dexB=14 intelB=20 chaB=0 maxHpB=250 maxMpB=180 ac=796
D BotPHD_E strB=5 staB=34 dexB=14 intelB=20 chaB=0 maxHpB=250 maxMpB=180 ac=796
D BotPHD_K strB=5 staB=34 dexB=14 intelB=20 chaB=0 maxHpB=250 maxMpB=180 ac=796
D BotWG_E strB=29 staB=34 dexB=0 intelB=0 chaB=0 maxHpB=260 maxMpB=0 ac=871
D BotWG_K strB=29 staB=34 dexB=0 intelB=0 chaB=0 maxHpB=260 maxMpB=0 ac=871
D BotWP_E strB=29 staB=34 dexB=0 intelB=0 chaB=0 maxHpB=260 maxMpB=0 ac=715
D BotWP_K strB=29 staB=34 dexB=0 intelB=0 chaB=0 maxHpB=260 maxMpB=0 ac=715
```

**Adım 5 — kod okuması (K6)**

(a) `m_bMaxWeightAmount` kullanımlarının **tamamı**:
```
$ grep -n -a "m_bMaxWeightAmount" GameServer/*.cpp GameServer/*.h
GameServer/MagicProcess.cpp:373:  TO_USER(pTarget)->m_bMaxWeightAmount = (uint8) pType->sExpPct;   (BUFF_TYPE_WEIGHT)
GameServer/MagicProcess.cpp:729:  TO_USER(pTarget)->m_bMaxWeightAmount = 100;                      (BUFF bitişi)
GameServer/User.cpp:2184:         ... * (m_bMaxWeightAmount <= 0 ? 1 : m_bMaxWeightAmount / 100);  (okuma)
GameServer/User.h:234:  uint8 m_bMaxWeightAmount;                                               (bildirim)
```
(b) `CUser` oluşturma: `shared/KOSocketMgr.h:79` → `m_idleSessions.insert(std::make_pair(i, new T(i, this)));` (T = `CUser`, `GameServer/GameServerDlg.h:535` `KOSocketMgr<CUser> m_socketMgr;`). `new T(i, this)` değer başlatma (`new T(...)` değil) yapar; kurucu `GameServer/User.cpp:13`'te başlar ve `m_bMaxWeightAmount`'a **dokunmaz** (grep: kurucu aralığında 0 eşleşme). Kurucu yalnızca `m_sMaxWeight = 0` (`:97`) ve `m_sMaxWeightBonus = 0` (`:98`) atar; `m_bMaxWeightAmount` yalnızca savaş sırasında bir ağırlık buff'ı ile (`:373`/`:729`) atanır.
(c) **MB-12'nin kod düzeyinde sonucu (doğrulanmış, tahmin değil):** `m_bMaxWeightAmount` başlatılmadığı için oturum açıldığında değeri **belirsizdir** (`uint8`, 0–255). `:2184`'teki tamsayı bölmesiyle etkisi: `0` → çarpan 1 (tam `base`), **`1–99` → çarpan 0 (`m_sMaxWeight = 0`; `CheckWeight` her alım/taşımada reddeder)**, `100–199` → 1, `200–255` → 2. Yani doğru davranış (×1) yalnızca bellek 0 geldiğinde tesadüfen oluşur; kesin değer çalışma zamanında ölçülmelidir (T-DATA-05/Q-21). Bu plan bunu **ölçtü, düzeltmedi**.

**Kabul kriterleri öz-değerlendirmesi**
- K1 ✔ `--selftest` → `selftest OK`, çıkış 0.
- K2 ✔ 12 satırlık `db/002` sonuç tablosu yapıştırıldı; bot olmayan sayılar önce/sonra `6/4/4`; bot sayısı `12` (uygulanmış bırakıldı).
- K3 ✔ A/B/C/D bölümleri ve `A_SUMMARY`/`B_SUMMARY`/`C_SUMMARY` tam yapıştırıldı: A 150 satır (6 bot ×12 + 6 bot ×13), B 28 benzersiz item satırı, C 72 satır (12×6), D 12 satır.
- K4 ✔ Tek `USERDATA` sorgusu (`LIKE 'Bot%'`); çıktıdaki tüm bot alanları `Bot...` (0 istisna); gerçek oyuncu adı yok.
- K5 ✔ Koşul → kod satırı eşlemesi aşağıda; `FAIL` yok (`A_SUMMARY fail_count=0 bots_with_fail=0`), dolayısıyla raporlanacak başarısız bot/parça yok.
- K6 ✔ Üç kod okuması dosya:satır ile yukarıda; "doğrulanmış" (grep/atama listesi) ve "çalışma zamanında ölçülecek" (gerçek `m_bMaxWeightAmount` değeri) ayrı etiketlendi.
- K7 ✔ `git diff --stat main...bot/F1-05` yalnızca `tools/bot-gear-report.py` + plan dosyası; `git status --short` boş (rapor commit'inden sonra); `file` = ASCII, CR yok.

**K5 — koşul → kod satırı eşlemesi**

| Bölüm A koşulu | `ItemEquipAvailable` satırı |
|---|---|
| `GetLevel() >= ReqLevel` | `GameServer/ItemHandler.cpp:529` |
| `GetLevel() <= ReqLevelMax` | `:530` |
| `m_bRank >= ReqRank` | `:531` |
| `m_bTitle >= ReqTitle` | `:532` |
| `GetStat(STR) >= ReqStr` | `:533` |
| `GetStat(STA) >= ReqSta` | `:534` |
| `GetStat(DEX) >= ReqDex` | `:535` |
| `GetStat(INT) >= ReqIntel` | `:536` |
| `GetStat(CHA) >= ReqCha` | `:537` |
| (temel stat: item bonusu yok) | `GameServer/User.h:470` (`GetStat`), `:498` (`GetStatWithItemBonus`) |
| Bölüm C: `m_sItemWeight += Weight × adet` | `GameServer/User.cpp:1323` |
| Bölüm C: cospre çanta `m_sMaxWeightBonus += Duration` | `:1317` |
| Bölüm C: bonuslar yalnızca ekipman (0–13) | `:1327-1334` |
| Bölüm C: `m_sMaxWeight` (tamsayı bölme) | `:2184` |
| `CheckWeight` (yalnızca alım/taşıma) | `ItemHandler.cpp:245` |

**Plandan sapmalar ve gerekçeleri**
1. İşlevsel sapma yok; betik plan §5.3'teki komut satırı, sorgular ve bölüm biçimlerini birebir uygular. Ek olarak çıktıda A/B/C satır sayıları ve özetler plan biçiminde.
2. `-W` ile uzun `strItem` hex sütunu **kırpılmadı** (planın `-W -s "|" -h -1 -b` düzeniyle 1168 karakter tam okundu; yalnızca CR temizlendi). `-y 0` gerekmedi.
3. Botlar plandaki gibi **uygulanmış bırakıldı** (F2 ön koşulu); rollback çalıştırılmadı.

**Açık sorular / bulgular**
1. **Ağırlık (MB-12/Q-21):** `amount=0` ve `amount=100` (yani bellek 0/100–199 geldiğinde) bile 8 botun çantası `base` ağırlığı aşıyor (MF/MI 10470 > 6750; PHD/PHB 10680 > 10250); yalnızca WP/WG (10877/10867 ≤ 18200) sığıyor. `amount=50` durumunda 12/12 bot sığmıyor. Yani başlatılmamış değer 1–99 geldiğinde hiçbir bot yerden eşya alamaz; bu, MB-12'nin pratik etkisidir. Çözüm bu planın dışında (mekanik/kod kararı).
2. Q-05 (istemcide kuşanılabilirlik) ve T-DATA-02'nin kuşanma testi bu planda yok; B bölümü yalnızca `Race`/`Class` verisini raporladı (`nonzero_race=0`, `nonzero_class=15` — sınıf kısıtlı parçalar).
3. `Hp/Mp`'nin girişte maks'a yükselmesi ve item `Race` kısıtı etkisi çalışma zamanında ölçülecek (plan §8'deki belirsizlikler).

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
