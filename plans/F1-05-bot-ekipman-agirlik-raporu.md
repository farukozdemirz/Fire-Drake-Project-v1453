# F1-05: Bot ekipman uygunluğu ve ağırlık raporu (`tools/bot-gear-report.py`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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
