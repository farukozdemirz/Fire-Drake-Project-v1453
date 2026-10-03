# F4-40: Bot envanter doldurma (`db/004`, `tools/bot-refill.sh`) — pot, taş ve scroll stokunu senaryo değerine geri yazan geri alınabilir SQL betiği (ADR-0018 m.8 / Ek 16, ADR-0032-DEG, KI-DEG-05 envanter parçası)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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

- Durum: —
- Branch / commit'ler: —
- Değişen dosyalar ve neden: —
- Derleme sonucu: —
- Kabul kriterleri öz-değerlendirme: —
- Plandan sapmalar ve gerekçeleri: —
- Açık sorular: —

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(boş)
