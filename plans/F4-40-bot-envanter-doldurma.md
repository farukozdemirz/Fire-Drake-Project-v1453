# F4-40: Bot envanter doldurma (`db/004`, `tools/bot-refill.sh`) — pot, taş ve scroll stokunu senaryo değerine geri yazan geri alınabilir SQL betiği (ADR-0018 m.8 / Ek 16, ADR-0032-DEG, KI-DEG-05 envanter parçası)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi; ADR-0018 Ek 16) |
| Branch | `bot/F4-40` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F1-04 (`db/002_bot_characters.sql`, 12 bot satırı ve `strItem` düzeni) ve F4-27 (`db/003` betik kalıbı: yedek tablo, öz denetim, geri alma) — `KAPANDI`; F4-04 (pot dilimi, `PotKindOf`), F4-36/F4-37 (taş ve scroll tüketimi) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/11` STK-01..STK-05; `docs/15` §6a ("Pot ve tüketilebilir eşyalar" satırı); ADR-0018 Ek 1 madde 5, Ek 9, Ek 12; ADR-0032-DEG (bot satırına yazım yalnızca bot çevrimdışıyken); KI-DEG-05 (envanter doldurma yok) |
| Tahmini büyüklük | S (2 yeni SQL betiği, 1 yeni kabuk betiği, `db/README.md`; sunucu/`BotCore` kodu **yok**, `vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

Botların çantası DB'de kalıcıdır: çıkışta kaydedilir, sonraki girişte olduğu gibi yüklenir (ADR-0032-DEG bağlamı). Tüketilen potlar, sınıf taşları ve diriltme taşları bu yüzden her oturumda azalır; yeniden stoklayan bir oyun eylemi yoktur (Ronark'ta pot satıcısı yok, STK-04). Bu yüzden T-MECH-SKILL'in botla yeniden koşusu, uzun betikli koşular ve ileride `ScenarioReset` (F8) tükenmiş çantadan başlar.

Bu plan, 12 bot satırının çantasındaki **tüketilebilir eşya yuvalarını (14..21)** senaryo stoğuna geri yazan, tekrar çalıştırılabilir ve geri alınabilir bir SQL betiği (`db/004_bot_inventory.sql`) ile onu güvenle çağıran ince bir kabuk betiği (`tools/bot-refill.sh`) ekler. Karar ADR-0018 Ek 16'dadır: yöntem ADR-0032-DEG'in seçtiği yöntemdir (bot çevrimdışıyken DB satırına yazım); `ScenarioRunner`'a otomatik çağrı **bu planın dışındadır** (F8 `ScenarioReset`).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0032-DEG-senaryo-baslangic-yerlesimi.md`: "Canlı nesneye yazılmaz", yazım yalnızca bot `DESPAWNED` ve sunucular kapalıyken; yalnızca `BOT_TABLE` adları.
- `docs/11` §6 (STK-01: tüketilmeyen 720 HP / 1920 MP potundan birer adet + tüketilen potlardan senaryo stoğu; STK-05 ağırlık).
- `docs/15` §6a tablosu "Pot ve tüketilebilir eşyalar" satırı; `docs/KNOWN_ISSUES.md` KI-DEG-05.
- `db/002_bot_characters.sql` (bu planın kalıbı ve sayısal kaynağı):
  - `:205-216` çanta yerleşimi: yuva 14 `389014000` ×1, 15 `389015000` ×100, 16 `389020000` ×1, 17 `379006000` ×30; savaşçı: 18 `379059000` ×50, 19 `379063000` ×1; priest: 18 `379062000` ×50, 19 `379066000` ×1; mage: 18 `379061000` ×50, 19 `379065000` ×1, 20 `379070000` ×1.
  - `:218-243` `strItem` düzeni: **73 yuva × 8 bayt** (`int32` eşya kimliği, `int16` dayanıklılık, `int16` adet; hepsi little-endian; boş yuva 8 sıfır bayt); yuva `n`'nin baytları `n*8+1 .. n*8+8` (T-SQL 1 tabanlı). Dayanıklılık `ITEM.Duration`, adet `Countable = 0` ise 1, üst sınır 9999 (`:239-243`). Yuva 0..13 ekipman, 14..41 çanta (`shared/globals.h:227,236`: `SLOT_MAX` = 14, `HAVE_MAX` = 28).
  - `:89-97` eşya ön kontrolü kalıbı (`ITEM` tablosunda yoksa `RAISERROR`).
- `db/003_bot_quests.sql` ve `db/003_bot_quests_rollback.sql`: ad listesi (`JOIN (VALUES ...)`), tek kez yedek tablosu, `RAISERROR` ile öz denetim, geri almada yedek satırlarını silip tabloyu koruma, `-v` değişkenlerinin **zorunlu** olması (bu sqlcmd sürümü tanımsız değişkende batch'i durdurur). Aynı kalıbı kullan.
- Sunucu tarafı stok görünümü: `GameServer/Bot/BotManager.cpp:2500` `FillSelfExtras` çantadaki (14..41) her eşyayı `ActionExecutor::PotKindOf` (`GameServer/Bot/ActionExecutor.cpp:1438`) ile sınıflar: `bDirectType 1` HP, `2` MP; `/bot snap` bunu `BotManager.cpp:2643` satırında `stock hp_pot=<n> mp_pot=<n>` olarak yazar.
- **Veri gerçekleri (Claude 2026-10-03'te yerel `ITEM`/`MAGIC` tablolarını sorguladı `[D]`; oyun verisi, kişisel veri değil):**

| Eşya | Kimlik | Adet türü | Etki |
|---|---|---|---|
| Water of favors | `389014000` | countable | `MAGIC 490014`: HP +720, **tüketilmez** (`UseItem 0`) |
| Water of bless | `389015000` | countable | `MAGIC 490015`: HP +1440, **tüketilir** (`UseItem 389015000`) |
| Potion of soul | `389020000` | countable | `MAGIC 490020`: MP +1920, **tüketilmez** |
| Potion of Ancient Spirit | `389220000` | countable, `Weight` 30 | `MAGIC 490701`: MP +2160, **tüketilir** (`UseItem 389220000`); `Type1 3`, `Moral 1`, `Msp 0`, `Etc 0`, `ReCastTime 20`, `DirectType 2`: `PotMagicSupported`'e uyar |
| Stone of life | `379006000` | countable | diriltmede **ölü hedeften** düşer (ADR-0018 Ek 9) |
| Stone of Warrior / Priest / Mage | `379059000` / `379062000` / `379061000` | countable | sınıf skill'inde her atışta 1 azalır (Ek 12) |
| Scream Scroll / Judgment Scroll / Absolute Power Scroll / Spell of impact | `379063000` / `379066000` / `379065000` / `379070000` | **countable değil** (adet hep 1) | scroll tüketilmez (Ek 12); yine de yuva şablona geri yazılır |

## 3. Kapsam

### 3.1 `db/004_bot_inventory.sql` (yeni)

`db/003`'ün biçimine uy: başlık yorumu (amaç, kullanım satırı, yuva sahipliği, öz denetim), `SET NOCOUNT ON; SET XACT_ABORT ON; GO`, `GO` blokları, ASCII + LF, İngilizce yorumlar. Yalnızca **12 bot adına** dokunur (açık `VALUES` listesi; `LIKE 'Bot%'` **kullanma**), satır içeriğini **asla** ekrana basma (yalnızca sayaçlar).

1. **Zorunlu `-v` değişkenleri** (hepsi her çalıştırmada verilmeli): `HpPots`, `MpPots`, `LifeStones`, `ClassStones`; her biri `0..9999` tamsayı (`ITEMCOUNT_MAX = 9999`, `shared/globals.h:258`). Geçersizse ilk batch'te `RAISERROR` (şiddet 16) ile dur (ör. `TRY_CAST('$(HpPots)' AS int)` boş ya da aralık dışı).
2. **Eşya ön kontrolü:** şablondaki tüm kimlikler (`389014000, 389015000, 389020000, 389220000, 379006000, 379059000, 379061000, 379062000, 379063000, 379065000, 379066000, 379070000`) `dbo.ITEM`'da yoksa `RAISERROR` (`db/002:89-97` kalıbı).
3. **Şablon (betiğin sahip olduğu yuvalar: 14..21).** Sınıf `USERDATA.[Class]` yerine **ad listesinden** gelir (`BotWP_K`, `BotWG_K`, `BotWP_E`, `BotWG_E` savaşçı; `BotPHD_K`, `BotPHB_K`, `BotPHD_E`, `BotPHB_E` priest; `BotMF_K`, `BotMI_K`, `BotMF_E`, `BotMI_E` mage):

| Yuva | Eşya | Adet | Not |
|---|---|---|---|
| 14 | `389014000` | 1 | tüketilmeyen 720 HP (STK-01) |
| 15 | `389015000` | `HpPots` | 0 ise yuva boş (8 sıfır bayt) |
| 16 | `389020000` | 1 | tüketilmeyen 1920 MP (STK-01) |
| 17 | `379006000` | `LifeStones` | 0 ise boş |
| 18 | sınıf taşı (savaşçı `379059000`, priest `379062000`, mage `379061000`) | `ClassStones` | 0 ise boş |
| 19 | savaşçı `379063000`, priest `379066000`, mage `379065000` | 1 | scroll |
| 20 | yalnızca mage: `379070000`; diğerleri boş | 1 | |
| 21 | `389220000` | `MpPots` | 0 ise boş (`db/002`'de yok; bu betik ekler) |

   `HpPots=100, MpPots=0, LifeStones=30, ClassStones=50` verildiğinde yuva 14..21 **`db/002`'nin ürettiğiyle bayt bayt aynı** olmalıdır (yuva 21 boş). Yuva 22..41 (çantanın kalanı) ve 0..13 (ekipman) **değişmez**.
4. **Bayt üretimi:** `db/002:218-243`'teki döngünün aynısı, yalnızca 14..21 için (8 yuva × 8 = **64 bayt**): kimlik, `ITEM.Duration`, adet (`Countable = 0` ise 1; üst sınır 9999). Yazım `strItem = STUFF(strItem, 14 * 8 + 1, 64, @block)`: sonuç uzunluğu 584 kalmalı (sütun türü `strItem` için `INFORMATION_SCHEMA.COLUMNS` ile **önce kontrol et**; şema bilgisi serbesttir, satır içeriği değildir).
5. **Yedek tablo** `dbo.USERDATA_BOT_STOCK_BACKUP (strUserID varchar(21) NOT NULL PRIMARY KEY, OldSlots varbinary(64) NOT NULL, SavedAt datetime NOT NULL DEFAULT GETDATE())`: her bot satırının **eski 14..21 baytları** satır yedekte yoksa bir kez yazılır; tekrar çalıştırma yedeği ezmez (geri alma, ilk uygulamadan önceki durumu getirir).
6. **Önkoşul kontrolleri (satır içeriği okunmadan):** bot satırlarının hepsi var mı (`rows`), her satırda `DATALENGTH(strItem) = 584` mi. Biri sağlanmıyorsa `fail`'e sayılır.
7. **İdempotans:** hesaplanan blok mevcut 14..21 baytlarına eşitse satır yazılmaz ve `changed`'a sayılmaz.
8. **Öz denetim (yazımdan sonra, aynı işlemde):** her satır için (a) 14..21 baytları şablondan yeniden hesaplananla eşit, (b) 0..13 ve 22..41 baytları işlemden **önce** alınan kopyayla eşit (geçici tablo; ekipman bozulmadı), (c) `DATALENGTH(strItem) = 584`. Tek çıktı satırı:
   `BOTSTOCK: rows=<N> ok=<N> fail=<N> changed=<N> hp=<HpPots> mp=<MpPots> life=<LifeStones> class=<ClassStones>`
   `rows <> 12` ya da `fail > 0` ise işlemi **geri al** ve `RAISERROR` (şiddet 16) ile bitir (`-b` ile sıfırdan farklı çıkış). Beklenen: `rows=12 ok=12 fail=0`; ikinci çalıştırmada `changed=0`.

### 3.2 `db/004_bot_inventory_rollback.sql` (yeni)

`db/003_bot_quests_rollback.sql`'in biçimi: yedek tablo yoksa `BOTSTOCK_ROLLBACK: restored=0`; varsa yedekteki satırların 14..21 baytlarını `STUFF` ile eski değere döndürür, geri yazılan yedek satırlarını siler (tablo korunur, ikinci geri alma `restored=0`). Çıktı: `BOTSTOCK_ROLLBACK: restored=<N>`. Yalnızca 12 bot adı; satır içeriği basılmaz.

### 3.3 `tools/bot-refill.sh` (yeni, ince sarmalayıcı)

`tools/check-env.sh` / `tools/run-servers.sh` kalıbı (`#!/usr/bin/env bash`, `set -euo pipefail`, `ROOT`, `SQLCMD` yolu ortam değişkeniyle geçersiz kılınabilir, **yalnızca ASCII, LF**, çıktı mesajları İngilizce). Kullanım:

```
tools/bot-refill.sh apply [--hp-pots N] [--mp-pots N] [--life-stones N] [--class-stones N] [--dry-run]
tools/bot-refill.sh rollback [--dry-run]
```

- Varsayılanlar `db/002` ile aynı: `--hp-pots 100 --mp-pots 0 --life-stones 30 --class-stones 50`.
- Değerler `0..9999` tamsayı olmalı; aksi halde mesaj + çıkış kodu **2**. Bilinmeyen alt komut/bayrak: kullanım + çıkış kodu **2**; `--help`: kullanım + 0.
- **Sunucu denetimi:** çalıştırmadan önce durum komutunu çağır (varsayılan `"$ROOT/tools/run-servers.sh" status`; ortam değişkeni `FDP_REFILL_STATUS_CMD` ile geçersiz kılınabilir: sınama için). Çıktıda `[UP]`, `[PARTIAL]`, `[STARTING]` ya da `[FAILED]` ile başlayan satır varsa **reddet** (mesaj: servers are running, stop them first; çıkış kodu **1**), SQL'e hiç bağlanma. `--dry-run` bu denetimi ve SQL çağrısını **atlar**; yalnızca çalıştırılacak `sqlcmd` komut satırını (`-v` değişkenleri dahil) yazdırır, çıkış 0.
- Çalıştırma: `"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_kn_online -b -v HpPots=<n> -v MpPots=<n> -v LifeStones=<n> -v ClassStones=<n> -i db/004_bot_inventory.sql` (`rollback` için `db/004_bot_inventory_rollback.sql`, değişkensiz). Betiğin `BOTSTOCK...` satırını aynen yazdır; `sqlcmd` çıkış kodunu ilet. `FDP_SQL_INSTANCE`/`FDP_SQL_DB` ortam değişkenleri varsa `tools/check-env.sh`'taki gibi kullan.
- `--selftest` **ekleme**; sınama için `--dry-run` ve `FDP_REFILL_STATUS_CMD` yeterlidir (K6).

### 3.4 `db/README.md`

`004` bölümü: ne yapar (yuva 14..21, tablo), sunucular kapalıyken çalıştırılır (gerekçe: çıkışta bellekteki çanta DB'ye geri yazılır, ADR-0032-DEG), değişkenler ve `db/002` ile eşdeğer varsayılanlar, `tools/bot-refill.sh` kullanımı, yedek/geri alma, beklenen çıktı satırları, "satır içeriği basılmaz".

**Kapsam dışı (yapılmayacak)**

- `ScenarioRunner` ya da başka sunucu/`BotCore` koduna otomatik çağrı, konum/HP/MP/NP sıfırlama, `SETUP_FAIL`, `ScenarioReset` bayrağı (F8). Bu plan yalnızca **çanta** stoğudur.
- `/bot snap` çıktısına taş sayısı eklemek (taş sayımı yalnızca betiğin öz denetiminde ve DB düzeyindedir), canlı `CUser` çantasına yazmak, sunucu içi `GiveItem`.
- `db/002` veya `db/003` dosyalarını değiştirmek; yuva 0..13 ve 22..41'e dokunmak; bot dışı satırlar; `WAREHOUSE`.
- Ağırlık sınırı denetimi/hesabı (STK-05; bota yalnızca alımda uygulanır, MB-12 ayrı ölçüm).
- Yeni karar: hangi senaryonun hangi stokla çalışacağı F8/senaryo dosyasındadır; buradaki varsayılanlar yalnızca `db/002` eşdeğeridir.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `db/004_bot_inventory.sql` | yeni | ASCII, LF (`.gitattributes`: `*.sql eol=lf`) |
| `db/004_bot_inventory_rollback.sql` | yeni | ASCII, LF |
| `tools/bot-refill.sh` | yeni | ASCII, LF, `chmod +x` (`git update-index --chmod=+x` gerekirse) |
| `db/README.md` | değiştir | yalnızca `004` bölümü eklenir |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. `GameServer/`, `BotCore/`, `shared/`, `docs/`, `db/001..003` **değişmez**.

## 5. Uygulama adımları

1. `git switch -c bot/F4-40 gece/2026-10-02`; `Durum` → `UYGULANIYOR`.
2. Sunucuların kapalı olduğunu doğrula (`./tools/run-servers.sh status`; `[UP]` varsa `stop`).
3. Şema ön kontrolü (yalnızca şema, satır içeriği değil): `strItem` sütun türü/uzunluğu. Beklenen 584 bayt; farklıysa **dur** ve raporda soru yaz.
4. `db/004_bot_inventory.sql` ve `db/004_bot_inventory_rollback.sql` (§3.1, §3.2).
5. `tools/bot-refill.sh` (§3.3) ve `db/README.md` (§3.4).
6. SQL denemeleri (sunucular kapalı; yalnızca çıktı satırlarını rapora yaz, satır içeriği basma). `SQLCMD` yolu `CLAUDE.md` "Komutlar"daki gibi; kabuk betiği üzerinden:
   - `tools/bot-refill.sh apply` (varsayılan 100/0/30/50): `rows=12 ok=12 fail=0`; tekrar: `changed=0`.
   - `tools/bot-refill.sh apply --hp-pots 7 --mp-pots 5 --life-stones 3 --class-stones 11`: `fail=0`; tekrar: `changed=0`.
   - `tools/bot-refill.sh rollback`: `restored=12`; tekrar: `restored=0`.
   - `tools/bot-refill.sh apply` yeniden: çalışır (yedek yeniden alınır), ardından `rollback` ile ilk duruma dön.
7. `--dry-run` ve `FDP_REFILL_STATUS_CMD` denemeleri (§6 K6). Sunucu **açma**; çalışma zamanı kanıtı Claude'un doğrulamasındadır.
8. `./tools/build.sh Release`; `./tools/run-tests.sh Release` (kod değişmediği için yalnızca "derlenir ve sayı değişmedi" kanıtı).
9. Uygulayıcı Raporu (Tur 1) doldur; `Durum` → `UYGULANDI`. Veritabanını **ilk duruma** (son komut `rollback`) bırak.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; `./tools/run-tests.sh Release` `0 failed` ve test sayısı işe başlamadan önceki sayıyla aynı (251; farklıysa taban sayıyı rapora yaz)
- [ ] K2: `git diff --stat gece/2026-10-02...bot/F4-40` yalnızca §4'teki dosyalar (+ bu plan dosyası); `GameServer/`, `BotCore/`, `shared/`, `docs/`, `db/001*`, `db/002*`, `db/003*` farkı 0; `git diff --check` boş
- [ ] K3: `db/004_bot_inventory.sql` ilk çalıştırma (100/0/30/50) `BOTSTOCK: rows=12 ok=12 fail=0 changed=<N> hp=100 mp=0 life=30 class=50`; ikinci `changed=0`; `grep -c "LIKE 'Bot" db/004_bot_inventory.sql` = 0 ve 12 ad açık listede; betik satır içeriğini basmaz (`PRINT`/`SELECT` yalnızca sayaç)
- [ ] K4: 7/5/3/11 ile `fail=0` ve tekrarında `changed=0`; öz denetimdeki ekipman/çanta kalanı karşılaştırması çalışıyor: betikte (b) denetimini sağlayan kod vardır ve `fail=0` (kanıt: dosya:satır). Yuva 22..41 ve 0..13 baytları işlem öncesi/sonrası eşit
- [ ] K5: `rollback` `BOTSTOCK_ROLLBACK: restored=12`; ikinci `restored=0`; yedek tablo korunur ve boştur; apply → rollback → apply dizisi çalışır
- [ ] K6: `tools/bot-refill.sh`: (a) `apply --dry-run` varsayılanlarla `-v HpPots=100 -v MpPots=0 -v LifeStones=30 -v ClassStones=50` içeren komutu yazdırır, rc 0; (b) `apply --hp-pots abc`, `--hp-pots 10000`, `--hp-pots -1`, bilinmeyen alt komut ve bilinmeyen bayrak rc 2; (c) `FDP_REFILL_STATUS_CMD="echo [UP] GameServer"` ile `apply` rc 1 ve SQL çağrılmaz (çıktıda `BOTSTOCK` yok); `FDP_REFILL_STATUS_CMD="echo [DOWN] GameServer"` ile geçer; (d) `--help` rc 0
- [ ] K7: dosya kodlaması: yeni dosyalar yalnızca ASCII ve LF (`file` çıktısı); `bash -n tools/bot-refill.sh` rc 0; `db/README.md` değişikliği yalnızca ekleme (mevcut satırlar değişmedi)
- [ ] K8 (Claude, çalışma zamanı): `apply --hp-pots 7 --mp-pots 5 --life-stones 3 --class-stones 11` sonrası sunucu açık (`[BOT] ENABLED=1`): (a) `/bot spawn BotMF_K` + `/bot snap BotMF_K` ⇒ `stock hp_pot=8 mp_pot=6` (7 Water of bless + 1 Water of favors; 5 Ancient Spirit + 1 Potion of soul); (b) bir tüketilen pot içilir (`/bot pot`), `despawn`, yeniden `spawn`, `snap` stoğun azaldığını ve **kalıcı** olduğunu gösterir; (c) sunucular kapatılır, aynı `apply` tekrar: yalnızca o bot değişir (`changed=1` beklenir; fark varsa raporlanır) ve yeni girişte `snap` `hp_pot=8 mp_pot=6` döner; (d) iş bitince `rollback` (ilk durum)
- [ ] K9: Uygulayıcı Raporu dürüst: SQL çıktı satırları ve komut çıktıları gerçek; yapılmayan/doğrulanamayan açıkça yazılmış; satır içeriği/hesap bilgisi rapora girmemiş

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status
tools/bot-refill.sh apply --dry-run
tools/bot-refill.sh apply
tools/bot-refill.sh apply
tools/bot-refill.sh apply --hp-pots 7 --mp-pots 5 --life-stones 3 --class-stones 11
tools/bot-refill.sh rollback
tools/bot-refill.sh rollback
FDP_REFILL_STATUS_CMD="echo [UP] GameServer" tools/bot-refill.sh apply; echo "rc=$?"
bash -n tools/bot-refill.sh
./tools/build.sh Release && ./tools/run-tests.sh Release 2>&1 | tail -3
git diff --stat gece/2026-10-02...bot/F4-40
grep -c "LIKE 'Bot" db/004_bot_inventory.sql
file db/004_bot_inventory.sql db/004_bot_inventory_rollback.sql tools/bot-refill.sh
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2.7 ve `CLAUDE.md` DB kuralı: `USERDATA` yalnızca 12 bot satırına ve yalnızca betiğin içinde dokunulur; satır içeriği (envanter baytları dahil) çıktıya/rapora **basılmaz**; `testing`, `testmage` gibi başka karakter satırlarına, `CURRENTUSER`, `TB_USER`, `ACCOUNT_CHAR`, `WAREHOUSE*` tablolarına dokunulmaz. Elle tek seferlik `UPDATE` yapma; her değişiklik betik dosyasından.
- **Sunucular kapalıyken** çalıştır: oyundaki karakter çıkışta bellekteki çantayı DB'ye geri yazar ve betiğin yazdığını ezer (ADR-0032-DEG). Sarmalayıcı bunu yalnızca bu çalışma ağacının sunucuları için denetler; başka çalışma ağacında açık sunucu varsa (paralel hat) çalıştırma, o durumda DB'ye hiç yazma.
- Sarmalayıcı bir **araç betiğidir**; sunucu davranışını, bot sistemi kapalıyken dahil, değiştirmez (kod yok).
- `Countable = 0` eşyalar (scroll'lar) adet 1 yazılır; sayılabilir eşya üst sınırı 9999 (`ITEMCOUNT_MAX`). Bu betik yuva 21'in sahibidir: önceki içerik yedeğe alınır.
- Bu sqlcmd sürümü tanımsız `$(Değişken)` için batch'i durdurur: dört `-v` değişkeni her çağrıda verilir (sarmalayıcı bunu sağlar). Windows `SQLCMD.EXE` göreli `-i db/...` yolunu depo kökünden çalıştırıldığında açar; açamazsa `wslpath -w` ile Windows yolu ver (rapora not et).
- Kodlama: yeni dosyalar yalnızca ASCII; `.sql`/`.sh` LF, `AGENTS.md` §3 "yeni dosyalar CRLF" kuralının `.gitattributes` ile LF'ye bağlanan türler için geçerli olmadığını unutma (`db/003` ve `tools/*.sh` LF).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-40` (taban `gece/2026-10-02`); `10fe98d` `[F4-40] db/004 envanter doldurma betikleri, bot-refill.sh ve README`; bu rapor + `Durum` commit'i.
- Değişen dosyalar ve nedenleri:
  - `db/004_bot_inventory.sql` (yeni): yuva 14..21 stok şablonu, tek seferlik yedek, yazım, satır içi öz denetim (a/b/c), tek sayaç satırı.
  - `db/004_bot_inventory_rollback.sql` (yeni): yedekten 14..21 baytlarını geri yazar, yedek satırlarını siler, tabloyu korur.
  - `tools/bot-refill.sh` (yeni): sunucu durumu denetimli ince sarmalayıcı (`apply`/`rollback`, `--dry-run`).
  - `db/README.md`: yalnızca `004` bölümü eklendi (45 satır, mevcut satırlar değişmedi).
  - `plans/F4-40-bot-envanter-doldurma.md`: `Durum` + bu rapor.
- Önkoşul: sunucular kapalı (`tools/run-servers.sh status` → `0/3`, yalnızca ilgisiz pid 4336). Şema ön kontrolü: `INFORMATION_SCHEMA.COLUMNS` `strItem` `binary(584)`; 12 bot satırı `DATALENGTH=584`.
- SQL denemeleri (sunucular kapalı; yalnızca çıktı satırları rapora yazıldı, satır içeriği okunmadı/basılmadı):
  - `apply` (varsayılan 100/0/30/50): `BOTSTOCK: rows=12 ok=12 fail=0 changed=5 hp=100 mp=0 life=30 class=50`; tekrar: `changed=0`.
  - `apply --hp-pots 7 --mp-pots 5 --life-stones 3 --class-stones 11`: `BOTSTOCK: rows=12 ok=12 fail=0 changed=12 hp=7 mp=5 life=3 class=11`; tekrar: `changed=0`.
  - `rollback`: `BOTSTOCK_ROLLBACK: restored=12`; tekrar: `restored=0`; yedek tablo korunur ve boştur (0 satır); `apply → rollback → apply` dizisi çalışır (yedek yeniden alınır).
  - Not: İlk `apply` `changed=5`; DB'de önceki doğrulama koşularından taş/pot tüketimi vardı, 7 satır `db/002` ile zaten bayt-eşitti. İkinci çalıştırma `changed=0`.
  - Betik çalıştırıldıktan sonra (`LIKE 'Bot` yorum düzeltmesi) yeniden `apply`/`rollback` denendi: `changed=5` / `restored=12`.
- Sarmalayıcı (K6): `apply --dry-run` çıktısı `-v HpPots=100 -v MpPots=0 -v LifeStones=30 -v ClassStones=50` içerir, rc 0; `--hp-pots abc` / `10000` / `-1`, bilinmeyen alt komut ve bilinmeyen bayrak rc 2; `FDP_REFILL_STATUS_CMD="echo [UP] GameServer"` → rc 1 ve SQL çağrılmadı (çıktıda `BOTSTOCK` yok); `[DOWN]` ile geçti; `--help` rc 0.
- Derleme: `./tools/build.sh Release` rc=0 (yalnızca eski `GameServer/User.cpp` C4834 ×2; bu plan kod değiştirmez); `./tools/run-tests.sh Release` son satırı `251 tests, 0 failed` (taban 251).
- Kapsam: `git status` yalnızca §4 dosyaları + plan; `GameServer/`, `BotCore/`, `shared/`, `AIServer/`, `docs/`, `db/001*`/`002*`/`003*` farkı 0; `git diff --check` boş.
- Kodlama: yeni dosyalar `ASCII text`, LF (`file`); `bash -n tools/bot-refill.sh` rc 0; `db/README.md` yalnızca ekleme (45/0); `grep -c "LIKE 'Bot" db/004_bot_inventory.sql` = 0; `tools/bot-refill.sh` indekste `100755` (`chmod +x`).
- Kriter öz-değerlendirmesi: K1 ✔ (251/0); K2 ✔; K3 ✔ (`changed=5` ilk, `changed=0` ikinci, `LIKE 'Bot` 0, 12 ad); K4 ✔ (7/5/3/11 `fail=0`, tekrar `changed=0`; (b) ekipman/çanta-kalanı denetimi `db/004_bot_inventory.sql:136,213,215-217`); K5 ✔ (`restored=12` sonra `0`, yedek korunur/boş, dizi çalışır); K6 ✔ (a/b/c/d); K7 ✔; K8 Claude'da (çalışma zamanı, **yapılmadı**); K9 ✔.
- Plandan sapmalar ve gerekçeleri:
  - §3.1 madde 4: `STUFF(strItem, 113, 64, @block)` bu SQL Server'da binary girdiyle `varchar` döndürüyor ve `binary(584)` sütununa doğrudan atanamıyor (`Implicit conversion from data type varchar to binary is not allowed`). Yazım `CONVERT(binary(584), STUFF(...))` olarak sarıldı (`db/004_bot_inventory.sql:205`, `db/004_bot_inventory_rollback.sql:29`). DB harmanı `SQL_Latin1_General_CP1_CI_AS` (tek bayt) olduğundan dönüşüm bayt-korunumlu; `apply` ikinci çalıştırmasında `changed=0` ve 7 satırın `db/002` ile bayt-eşitliği bunu doğrular. Aksi halde betik derlenmezdi.
  - `--dry-run` çıktısı `printf '%q'` ile kaçışlı yazılır (görüntü amaçlı); `-S` değeri çift ters bölü olarak görünebilir (komut satırı yine de K6 a) dizelerini içerir).
- Açık sorular:
  - K8 (Claude, çalışma zamanı): sunucu açıkken (`[BOT] ENABLED=1`) `apply --hp-pots 7 ...` sonrası `/bot snap` stok ölçümü ve pot tüketiminin kalıcılığı. Sunucular bu çalışma ağacında kapalı bırakıldı; DB son `rollback` ile işe başlamadan önceki durumda.
  - Not (sapma değil): uygulama sırasında paralel `nav` hattı ayrı çalışma ağacındadır; bu planda sunucu çalıştırılmadı.

### Tur 2

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-40` (taban `gece/2026-10-02`); düzeltme commit'i bu raporla birlikte.
- Düzeltme (yalnızca Doğrulama Turu 1 talimatındaki maddeler):
  1. `db/004_bot_inventory_rollback.sql`: yedek tablo varlık denetimi ile `UPDATE`/`DELETE`/`COMMIT`/`PRINT restored=<N>` bloğu **tek batch** içinde `IF OBJECT_ID(N'dbo.USERDATA_BOT_STOCK_BACKUP', N'U') IS NULL BEGIN PRINT '...restored=0'; END ELSE BEGIN ... END` biçimine getirildi; aradaki `GO` ve `RETURN` kaldırıldı. Sorgular tablo adını doğrudan içeriyor; ELSE dalı çalışmadığında derleme hatası vermediği geçici bir `/tmp` betiğiyle önce doğrulandı (bu yüzden `sp_executesql` gerekmedi). Mevcut davranış korundu: `CONVERT(binary(584), STUFF(...))`, yalnızca 12 bot adı (açık liste), yedek satırları silinir/tablo korunur, satır içeriği hiç basılmaz.
- Sınama (sunucular kapalı; `./tools/run-servers.sh status` → `0/3`, yalnızca ilgisiz pid 4336; yalnızca çıktı satırları ve rc yazıldı, satır içeriği okunmadı/basılmadı):
  - 2a (yedek tablo yok): `EXEC sp_rename 'dbo.USERDATA_BOT_STOCK_BACKUP','USERDATA_BOT_STOCK_BACKUP_X'` sonrası `tools/bot-refill.sh rollback` → `BOTSTOCK_ROLLBACK: restored=0`, **başka hata satırı yok**, `rc=0`; tablo `EXEC sp_rename '..._X','USERDATA_BOT_STOCK_BACKUP'` ile eski adına döndürüldü, `SELECT COUNT(*)` = `0` (tablo korundu).
  - 2b (normal dizi): `apply` → `BOTSTOCK: rows=12 ok=12 fail=0 changed=5 hp=100 mp=0 life=30 class=50` (rc 0); tekrar `apply` → `changed=0` (rc 0); `rollback` → `BOTSTOCK_ROLLBACK: restored=12` (rc 0); tekrar `rollback` → `BOTSTOCK_ROLLBACK: restored=0` (rc 0). Son komut `rollback`; DB ilk durumda.
- Doğrulama komutları (talimat 3):
  - `bash -n tools/bot-refill.sh` rc 0.
  - `file db/004_bot_inventory_rollback.sql` → `ASCII text`; `grep -c $'\r'` = 0 (LF).
  - `git diff --check` boş.
  - `git status --short` yalnızca `M db/004_bot_inventory_rollback.sql`. `git diff --stat gece/2026-10-02...bot/F4-40` ayrıca `docs/STATUS.md` ve `plans/README.md` gösterir; bunlar **Claude'un `4c976e1` Doğrulama commit'inden** gelir, bu turun değişikliği değildir (AGENTS.md gereği `docs/`/`plans/README.md` dokunulmadı).
  - `./tools/build.sh Release` rc=0; `./tools/run-tests.sh Release` son satır `251 tests, 0 failed`.
- Kriter öz-değerlendirmesi (talimat maddeleri): madde 1 ✔; madde 2a ✔ (`restored=0`, ek hata yok, rc=0, tablo 0 satır); madde 2b ✔ (`rows=12 ok=12 fail=0`, `changed=0`, `restored=12`, `restored=0`, son komut `rollback`); madde 3 ✔ (`bash -n`, ASCII+LF, `git diff --check` boş, build rc=0, `251 tests, 0 failed`).
- Plandan sapmalar: yok.
- Açık sorular: Yukarıdaki `git diff --stat` notu (Claude'un Doğrulama commit'inden gelen `docs/STATUS.md`/`plans/README.md`). K8 (Claude, çalışma zamanı) hâlâ yapılmadı; bu düzeltme yalnızca geri alma betiğini değiştirir, `apply` yolu aynıdır.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DÜZELTME GEREKLİ
- İncelenen: `gece/2026-10-02...bot/F4-40` @ `97d1697` (2 commit: `10fe98d`, `97d1697`). Çalışma ağacı temizdi; otonom gece döngüsünde (`AUTO_LOOP=1`) birleştirme/push yapılmadı. Sunucular kapalıydı (`run-servers.sh status` → `0/3`; ilgisiz pid 4336 yok sayıldı).
- Kriterler:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `./tools/build.sh Release` rc=0, `warning` satırı 0; `./tools/run-tests.sh Release` son satır `251 tests, 0 failed` (taban 251). Kod değişmediği için derleme artımlıdır (yalnızca "derlenir ve sayı değişmedi" kanıtı, plan öyle istiyor) |
| K2 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-40`: yalnızca `db/004_bot_inventory.sql`, `db/004_bot_inventory_rollback.sql`, `db/README.md`, `tools/bot-refill.sh`, bu plan; `GameServer/ BotCore/ shared/ AIServer/ docs/ db/001..003` farkı 0; `git diff --check` boş |
| K3 | ✔ | Kendi koşum (sunucular kapalı): `apply` → `BOTSTOCK: rows=12 ok=12 fail=0 changed=5 hp=100 mp=0 life=30 class=50`; tekrar `changed=0`. `grep -c "LIKE 'Bot" db/004_bot_inventory.sql` = 0; 12 ad açık listede (`db/004_bot_inventory.sql:87-90`); betikte satır içeriği basan `SELECT` yok, tek `PRINT` sayaç satırı (`:229-236`) |
| K4 | ✔ | `apply 7/5/3/11` → `rows=12 ok=12 fail=0 changed=12`; tekrar `changed=0`. (b) denetimi: `:136` (`@keepBefore`, 0..13 + 22..72), `:213` (`@keepAfter`), `:215-217` karşılaştırma; ayrıca `0/0/0/0` ile `changed=12` sonra `changed=0` (boş yuva yolu) |
| K5 | ✔ | `rollback` → `restored=12`; tekrar `restored=0`; yedek tablo korunur, 0 satır (`SELECT COUNT(*)` = 0); `apply → rollback → apply` → `changed=5` (ilk `apply` ile aynı: rollback ilk durumu getiriyor) |
| K6 | ✔ | (a) `apply --dry-run` `-v HpPots=100 -v MpPots=0 -v LifeStones=30 -v ClassStones=50` içeren komutu yazdırır, rc 0; (b) `--hp-pots abc`, `10000`, `-1`, bilinmeyen alt komut, bilinmeyen bayrak, `rollback --hp-pots 3` hepsi rc 2; (c) `FDP_REFILL_STATUS_CMD="echo [UP] GameServer"` ve `"echo [FAILED] x"` rc 1, `servers are running, stop them first`, SQL çağrılmadı; `[DOWN]` ile geçer (gerçek `apply` koşularında varsayılan `run-servers.sh status` ile geçti); (d) `--help` rc 0. Betik: `tools/bot-refill.sh:60-150` |
| K7 | ✔ | `file`: üçü `ASCII text` (sh: `Bourne-Again shell script, ASCII text executable`); CR sayısı 0 (LF); `bash -n` rc 0; indekste `100755`; `db/README.md` farkı yalnızca `+45/-0` |
| K8 | ertelendi | Çalışma zamanı sınaması (sunucu açma, `BotCommands.txt` ile `spawn`/`snap`/`pot`, kalıcılık) bu turda **yapılmadı**: karar zaten düzeltme gerektiriyor; düzeltme yalnızca geri alma betiğini değiştirir, `apply` yolu aynı kalır. Tur 2'de yapılacak |
| K9 | ✔ | Rapordaki SQL çıktı satırları, rc'ler, `changed=5`, `STUFF`→`CONVERT` sapması (`db/004_bot_inventory.sql:205`, `db/004_bot_inventory_rollback.sql:29`) kendi koşumlarımla örtüşüyor; satır içeriği/hesap bilgisi rapora girmemiş. Eksik: rollback'in yedek-tablo-yok yolu sınanmamış (aşağıda bulgu 1) |

- Bulgular (önem sırasına göre):
  1. **`db/004_bot_inventory_rollback.sql:13-17` yedek tablo yokken plana aykırı biçimde hata veriyor.** Plan §3.2: "yedek tablo yoksa `BOTSTOCK_ROLLBACK: restored=0`" (benign, rc 0). Betik `IF OBJECT_ID(...) IS NULL BEGIN PRINT ...; RETURN; END` yazıp `GO` koyuyor; `RETURN` yalnızca o batch'i bitirir, sonraki batch (`:20` `UPDATE ... JOIN dbo.USERDATA_BOT_STOCK_BACKUP`) yine çalışır. Kendi ölçümüm (yedek tablo geçici olarak `sp_rename` ile gizlendi, sonra adı geri verildi, tablo 0 satırla duruyor): `tools/bot-refill.sh rollback` → `BOTSTOCK_ROLLBACK: restored=0` ardından `Msg 208, Level 16 ... Invalid object name 'dbo.USERDATA_BOT_STOCK_BACKUP'` ve **rc=1**. `apply` hiç çalıştırılmamış bir ortamda (yeni kurulum, `apply` öncesi `rollback`) betik hata verir. Uygulayıcı bu yolu sınamadı (raporda yok).
  2. *(not, engel değil)* Öz denetim (a) `@block` ile karşılaştırıyor; `@block` yazımla aynı kodla üretildiği için bu, şablonun bağımsız yeniden hesabı değil. Plan metni "şablondan yeniden hesaplananla eşit" diyor; ek koruma olarak `db/002` ile bayt eşitliği (7 satırda `changed=0`) uygulayıcı tarafından gösterilmiş. Değişiklik gerekmez.
  3. *(not)* `STUFF(binary)` → `CONVERT(binary(584), ...)` sapması gerekçeli ve kanıtlı: `changed=0` tekrarları ve apply→rollback→apply tutarlılığı bayt-korunumunu doğruluyor. Kabul.
  4. *(not)* Geçersiz `-v` değerleri (`HpPots=abc`, `HpPots=10000`) doğrudan `sqlcmd` ile denendi: `RAISERROR` mesajı, rc=1, satıra dokunulmadı (sarmalayıcı zaten rc 2 ile önce reddediyor).
- DB durumu: tüm koşular `rollback` ile bitti (son `restored=12`, yedek tablo 0 satır); yalnızca betik çıktıları okundu, satır içeriği okunmadı/yazılmadı.
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
plans/F4-40-bot-envanter-doldurma.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
1. `db/004_bot_inventory_rollback.sql`: yedek tablo yokken betik `BOTSTOCK_ROLLBACK: restored=0` yazıp **sıfır hatayla** (`-b` ile rc 0) bitmeli. Şu an `IF OBJECT_ID(...) IS NULL BEGIN PRINT ...; RETURN; END` ardından `GO` var; `RETURN` yalnızca o batch'i bitirdiği için sonraki batch'teki `UPDATE ... JOIN dbo.USERDATA_BOT_STOCK_BACKUP` yine çalışıp `Msg 208 Invalid object name` ile rc=1 veriyor. Düzelt: tablo varlık denetimini ve UPDATE/DELETE/COMMIT/`PRINT restored=<N>` bloğunu **aynı batch** içinde `IF OBJECT_ID(N'dbo.USERDATA_BOT_STOCK_BACKUP', N'U') IS NULL BEGIN PRINT 'BOTSTOCK_ROLLBACK: restored=0'; END ELSE BEGIN ... END` biçimine getir (sorgular tablo adını doğrudan içerirse ve derleme yine hata veriyorsa `EXEC sp_executesql` ile çalıştır). Mevcut davranış (yedek varken `restored=12`, ikinci çalıştırmada `restored=0`, tablo korunur, yalnızca 12 bot adı, satır içeriği basılmaz, `CONVERT(binary(584), STUFF(...))`) aynen kalmalı. Arada `GO` ile bölünüp tabloya bağlı kalan ifade bırakma.
2. Sınama (sunucular kapalıyken, `tools/run-servers.sh status` ile doğrula; yalnızca çıktı satırlarını ve rc'yi rapora yaz):
   a. Yedek tablo yok durumu: yedek tabloyu geçici olarak `EXEC sp_rename 'dbo.USERDATA_BOT_STOCK_BACKUP','USERDATA_BOT_STOCK_BACKUP_X'` ile gizle, `tools/bot-refill.sh rollback` çalıştır: çıktı `BOTSTOCK_ROLLBACK: restored=0`, **başka hata satırı yok, rc=0**. Sonra tabloyu `EXEC sp_rename 'dbo.USERDATA_BOT_STOCK_BACKUP_X','USERDATA_BOT_STOCK_BACKUP'` ile eski adına döndür (tablo korunmalı, 0 satır).
   b. Normal dizi: `tools/bot-refill.sh apply` (`rows=12 ok=12 fail=0`), tekrar (`changed=0`), `tools/bot-refill.sh rollback` (`restored=12`), tekrar (`restored=0`, rc=0). Son komut `rollback` olsun (DB ilk durumda kalsın).
3. `bash -n tools/bot-refill.sh`; `file db/004_bot_inventory_rollback.sql` ASCII + LF kalmalı (`grep -c $'\r'` = 0); `git diff --check` boş. `git diff --stat gece/2026-10-02...bot/F4-40` plan dosyası + §4 dosyaları dışında bir şey göstermemeli. `./tools/build.sh Release` ve `./tools/run-tests.sh Release` (`251 tests, 0 failed`) tekrar çalıştır.
```

### Tur 2 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-40` @ `c4dd60b` (Tur 2 farkı `97d1697..c4dd60b`: yalnızca `db/004_bot_inventory_rollback.sql` + plan/kayıt dosyaları). Çalışma ağacı temizdi; otonom gece döngüsünde (`AUTO_LOOP=1`, `AUTO_INTEGRATION_BRANCH=gece/2026-10-02`) birleştirme/push yapılmadı, birleştirmeyi döngü betiği yapar. Sunucular başta kapalıydı (`0/3`; ilgisiz pid 4336 yok sayıldı); çalışma zamanı sınaması için açılıp **kapatıldı**.
- Kriterler:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `./tools/build.sh Release` rc=0, `warning` satırı 0; `./tools/run-tests.sh Release` son satır `251 tests, 0 failed` (taban 251) |
| K2 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-40`: `db/004_bot_inventory.sql`, `db/004_bot_inventory_rollback.sql`, `db/README.md`, `tools/bot-refill.sh`, bu plan, ayrıca Claude'un kendi doğrulama commit'lerinden `plans/README.md` ve `docs/STATUS.md`; `GameServer/ BotCore/ shared/ AIServer/ Tests/ db/001..003` farkı 0; `git diff --check` boş |
| K3 | ✔ | Kendi koşum: `apply` → `BOTSTOCK: rows=12 ok=12 fail=0 changed=5 hp=100 mp=0 life=30 class=50`, tekrar `changed=0`; `grep -c "LIKE 'Bot"` = 0; 12 ad açık listede; satır içeriği basılmıyor (Tur 1'de okunmuştu, Tur 2'de `apply` dosyası değişmedi) |
| K4 | ✔ | `apply 7/5/3/11` → `rows=12 ok=12 fail=0 changed=12`, tekrar `changed=0`; (b) denetimi `db/004_bot_inventory.sql:136,213,215-217` (Tur 1'de okundu, dosya değişmedi) |
| K5 | ✔ | `rollback` → `restored=12`, tekrar `restored=0`, rc=0; yedek tablo korunur, 0 satır; `apply → rollback → apply` çalışır. **Tur 1 bulgusu giderildi:** yedek tablo gizlenince (`sp_rename` ile `_X`, sonra adı geri verildi) `rollback` → `BOTSTOCK_ROLLBACK: restored=0`, başka hata satırı yok, **rc=0**; sonra tablo `SELECT COUNT(*)` = 0. Kod: `db/004_bot_inventory_rollback.sql:19-51` varlık denetimi ve UPDATE/DELETE/COMMIT/PRINT aynı batch'te, aradaki `GO` yok; `CONVERT(binary(584), STUFF(...))` `:30`, 12 ad açık liste `:34-36,:44-46` |
| K6 | ✔ | (a) `apply --dry-run` `-v HpPots=100 -v MpPots=0 -v LifeStones=30 -v ClassStones=50` içerir, rc 0; (b) `--hp-pots abc`, `10000`, `-1`, bilinmeyen alt komut, bilinmeyen bayrak rc 2; (c) `FDP_REFILL_STATUS_CMD="echo [UP] GameServer"` rc 1 ("servers are running, stop them first", `BOTSTOCK` yok), `[DOWN]` ile rc 0; (d) `--help` rc 0 |
| K7 | ✔ | `file`: iki `.sql` `ASCII text`, `.sh` `Bourne-Again shell script, ASCII text executable`; CR sayısı 0; `bash -n` rc 0; indekste `100755`; `db/README.md` farkı `45/0` |
| K8 | ✔ | Çalışma zamanı (`GameServer.ini`'ye geçici `[BOT] ENABLED=1`, komutlar `BotCommands.txt` ile; her komuttan sonra dosya tüketildi): `apply 7/5/3/11` (`changed=12`) sonrası (a) `spawn BotMF_K` + `snap` → `stock hp_pot=8 mp_pot=6`; (b) `pot BotMF_K 389015000 1` → `snap` `hp_pot=7 mp_pot=6`, `despawn` (`logout save`), yeniden `spawn`, `snap` → `hp_pot=7 mp_pot=6` (**kalıcı**); (c) `despawn`, sunucular kapatıldı (`0/3`), aynı `apply` → `changed=1` (yalnızca tüketim yaşayan bot), yeniden açılış, `spawn` + `snap` → `hp_pot=8 mp_pot=6`; (d) `despawn all`, sunucular kapatıldı, `GameServer.ini` yedekten geri yüklendi (`cmp` aynı), `BotCommands.txt` kalmadı, `rollback` → `restored=12`, tekrar `restored=0` |
| K9 | ✔ | Tur 2 raporundaki çıktı satırları ve rc'ler kendi koşumlarımla örtüşüyor (`restored=0` rc=0 yedek-yok yolu, `changed=5`/`0`, `restored=12`/`0`); satır içeriği/hesap bilgisi rapora girmemiş |

- Bulgular:
  1. *(not, engel değil)* Tur 1 engelleyicisi (yedek tablo yokken `Msg 208` + rc=1) giderildi; tek batch yapısı doğru ve yedek varken davranış değişmedi.
  2. *(not)* Öz denetim (a) `@block` ile karşılaştırıyor (şablonun bağımsız yeniden hesabı değil); ek koruma `db/002` ile bayt eşitliği (Tur 1 notu), değişiklik gerekmez.
  3. *(not)* K8 ölçümü: Water of bless adedi `hp_pot` sayacını bir azaltır; `/bot pot ... 389015000` dört `-v` değerinden bağımsız çalışır. `/bot snap` yalnızca pot sayar, taş sayısı DB düzeyindedir (plan kapsamı).
- DB ve ortam durumu: tüm koşular `rollback` ile bitti (son `restored=12` → `restored=0`, yedek tablo 0 satır; yalnızca betik çıktıları okundu); sunucular kapalı; `GameServer.ini` ve `BotCommands.txt` ilk duruma döndürüldü. Yazım yalnızca 12 bot satırına ve betikler üzerinden yapıldı (bot çıkış kaydı çalışma zamanı sınamasında `BotMF_K` satırına kendi yazdı; `rollback` yedekten 14..21 yuvalarını yeniden getirdi).
