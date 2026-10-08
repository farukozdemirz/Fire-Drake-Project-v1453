# U2-02: 1534 eşya, NPC ve canavar aktarımı (`db/013`, `db/014`; kaynak ALPHA DB, kapsam istemcinin tanıdığı kimlikler)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | U2 — Sürüm yükseltme 1534, veri (`docs/17` §2 U, ADR-0068 madde 4 ve Ek 1) |
| Branch | `bot/U2-02` (taban: U2-01 birleştikten sonraki `main`) |
| Bağımlı olduğu planlar | U2-01 (`tools/kotbl.py`) |
| İlgili gereksinim / kabul | T-UPG-01/04 hazırlığı; `docs/reports/u0-1534/E-veri-farki.md` §3, §4, §8 |
| Tahmini büyüklük | M (1 üretici, 4 SQL, README bölümü) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

1534 istemcisinin tanıdığı ama bizim DB'de olmayan eşya, NPC ve canavar satırlarını, kişisel veri içermeyen referans tablolara (**ITEM, K_NPC, K_MONSTER**) **eklemeli ve geri alınabilir** biçimde aktarmak. Kaynak satırlar ALPHA DB'sidir (`.\SQL2019` → `FDP_alpha1534`); kapsam **yalnız istemcinin tanıdığı kimlikler**. Mevcut satırların hiçbiri değişmez; botların kullandığı 83 eşya ve tüm MAGIC verisi aynen kalır.

Kararlar (ADR-0068 Ek 1, geri alınabilir varsayılanlar):
- Eşya kapsamı: istemcinin çözebildiği ve bizde olmayan **tüm** kimlikler (E §3.2: 35.860; hepsi ALPHA'da var).
- Yeni eşyaların seviye/stat şartları **ALPHA değerleriyle** kalır (istemcinin gösterdiğiyle tutarlı); bizim mevcut satırlarımızdaki düşürülmüş şartlara dokunulmaz.
- NPC kimlikleri 24438, 24439, 24440 **aktarılmaz** (bizim zone 64 bekçileri; E §4.1 çakışma).
- Yeni canavarların düşürme tablosu (`K_MONSTER_ITEM`) bu planda yok.

## 2. Bağlam (okunması zorunlu)

- `docs/reports/u0-1534/E-veri-farki.md` §3 (eşya modeli, sayılar, ItemClass kuralı §3.4), §4.1 (NPC/canavar farkları, çakışmalar), §8 (betik kuralları).
- `docs/reports/u0-1534/C-db-semasi.md` (ITEM/K_NPC/K_MONSTER şema farkları; ALPHA `ITEM.ItemClass` tamamen NULL).
- `tools/kotbl.py` (U2-01): `item_org_us.tbl` (kolon 36 = eşya derecesi), `Item_Ext_<n>_us.tbl`, `Npc_us.tbl`, `Mob_us.tbl`.
- Betik kalıbı: `db/012_u2_capes_1534.sql` (+ rollback), `db/README.md`.
- Sunucu yükleyicileri (kolon listeleri): `shared/database/ItemTableSet.h`, `shared/database/NpcTableSet.h` (K_NPC ve K_MONSTER aynı set ile yüklenir; kontrol et), `GameServer/LoadServerData.cpp`, `AIServer/` NPC yüklemesi.

## 3. Kapsam

**Var:**
1. `tools/u2-gen-alpha.py` (standart kütüphane; ALPHA ve bizim DB'yi `SQLCMD.EXE` ile **salt okur**, istemci tablolarını `kotbl` ile okur):
   - Alt komut `items`: bizim `ITEM` kolon listesini `INFORMATION_SCHEMA`'dan alır; ALPHA `ITEM`'den aynı adlı kolonları seçer; kapsam = istemcinin çözebildiği (`item_org` tabanı + `Item_Ext` varyantı; E §3.1) **ve** bizde olmayan kimlikler. `ItemClass` ve aksesuar `ItemExt` değerlerini E §3.4 kuralıyla **üreticide** hesaplar (kural betik başlığına yazılır). Bizim tabloda olup ALPHA'da olmayan NOT NULL kolon varsa açık hata.
   - Alt komut `npcs`: K_NPC (istemci `Npc_us` kimlikleri, eksi 24438/24439/24440) ve K_MONSTER (istemci `Mob_us` kimlikleri) için aynı yaklaşım; `strName` istemci tablosundan (ASCII'ye çevrilemeyen karakter varsa ALPHA adı; o da değilse `Npc <id>`).
   - `--check`: betikleri yeniden üretip depodakiyle bayt bayt karşılaştırır.
   - Çıktı deterministik (kimliğe göre sıralı, sabit sayı biçimi).
2. `db/013_u2_items_1534.sql` (+ `_rollback.sql`): `sqlcmd` değişkeni `Target` (gerçek kullanımda `ITEM`); 1000'lik `INSERT … VALUES` blokları yerine **önce `#u2_items` geçici tablosuna** blok blok yükle, sonra tek `INSERT … SELECT … WHERE NOT EXISTS (Num)`; eklenen kimlikleri `dbo.$(Target)_U2_ADDED (Num int PRIMARY KEY)` tablosuna yaz; `SET XACT_ABORT ON`; sonunda `inserted=… already_present=…`. Rollback yalnız kayıt tablosundakileri siler ve kayıt tablosunu düşürür.
3. `db/014_u2_npc_monster_1534.sql` (+ `_rollback.sql`): değişkenler `NpcTarget` (gerçekte `K_NPC`) ve `MonTarget` (gerçekte `K_MONSTER`); aynı desen, kayıt tabloları `$(NpcTarget)_U2_ADDED`, `$(MonTarget)_U2_ADDED`.
4. `db/README.md`: `## 013`, `## 014` bölümleri (sunucu kapalıyken çalıştırma şartı yok; tablolar açılışta yüklenir — etki için yeniden başlatma).

**Yok:** canlı tablolara uygulama (Claude yapar), `K_NPCPOS`/Moradon yerleşimi (U3), görevler (U3), `K_MONSTER_ITEM`, `ITEM_EXCHANGE`, mevcut satır güncellemesi.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `tools/u2-gen-alpha.py` | YENİ |
| `db/013_u2_items_1534.sql`, `db/013_u2_items_1534_rollback.sql` | YENİ (üretilmiş) |
| `db/014_u2_npc_monster_1534.sql`, `db/014_u2_npc_monster_1534_rollback.sql` | YENİ (üretilmiş) |
| `db/README.md` | `## 013`, `## 014` |
| `tools/kotbl.py` | yalnız hata düzeltmesi gerekirse (raporla) |

## 5. Uygulama adımları

1. Üreticiyi yaz; `items` ve `npcs` çıktılarını üret; `--check` 0.
2. Sayıları E raporuyla karşılaştır: eşya ≈ 35.860 (fark varsa nedenini açıkla); K_NPC 79; K_MONSTER 60. `ItemClass` dağılımı E §3.4 tahminiyle (0: 15.644; 1: 2.558; 2: 4.305; 3: 8.292; 4: 4.742; 8: 320) karşılaştırılır.
3. Kural doğrulaması: §3.4 ItemClass kuralını **bizim mevcut ITEM satırlarımıza** uygulayıp eşleşme oranını yeniden ölç (E: %99,53); rapora yaz.
4. **Geçici kopya tablolarda test** (`.\SQLEXPRESS`, `FDP_kn_online`, `-E`): `ITEM_U202TEST`, `K_NPC_U202TEST`, `K_MONSTER_U202TEST` (`SELECT * INTO`); uygula → beklenen `inserted`; ikinci uygulama `inserted=0`; geri al → `EXCEPT` ile orijinalle aynı (0 satır); kopyaları ve kayıt tablolarını düşür. Bot eşyaları (`db/007_bot_gear.sql` kimlikleri) uygulama öncesi/sonrası kopyada değişmemiş (`EXCEPT` 0).
5. Bellek etkisi notu: kopyada uygulama sonrası `ITEM` satır sayısı ve tahmini artış rapora.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/u2-gen-alpha.py --check` → 0.
- [ ] K2: §5.2 sayıları raporda; E raporundan sapma açıklanmış.
- [ ] K3: §5.3 kural eşleşme oranı ≥ %99,5.
- [ ] K4: §5.4 geçici kopya testi: ilk uygulama beklenen sayılar, ikinci `inserted=0`, geri alma `EXCEPT` 0, bot eşyaları değişmedi, geçici tablo kalmadı.
- [ ] K5: 24438, 24439, 24440 betikte **yok** (`grep` ile göster); ALPHA'nın istemcide olmayan K_NPC/K_MONSTER kimlikleri betikte yok.
- [ ] K6: SQL ASCII, LF; yorumlar İngilizce; betik başlığında kaynak, kural ve kapsam yazılı.
- [ ] K7: `git diff --stat main...bot/U2-02` yalnız §4 dosyaları; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
python3 tools/u2-gen-alpha.py --check
grep -c "24438\|24439\|24440" db/014_u2_npc_monster_1534.sql
ls -la db/013_u2_items_1534.sql db/014_u2_npc_monster_1534.sql
git diff --stat main...bot/U2-02
git status --short
```

## 8. Kısıtlar ve uyarılar

- ALPHA DB ve bizim DB yalnız `SELECT` ile okunur; canlı `ITEM`/`K_NPC`/`K_MONSTER`'a yazma yok.
- Kişisel veri tabloları okunmaz (`AGENTS.md` §2.7).
- İndirilen paketteki exe/dll çalıştırılmaz.
- `db/013` büyük olacak (~15–20 MB); tek dosya olarak commit edilir (üretici deterministik, `--check` ile yeniden üretilebilir).
- Git: `AGENTS.md` §2.8; commit `[U2-02] ...`.

---

## Uygulayıcı Raporu

### Tur 1

**Durum:** UYGULANDI (Claude uygulayıcı ajanı, 2026-10-08; proje sahibinin bu faz için istediği şekilde DeepSeek yerine).

**Branch ve commit'ler:** `bot/U2-02` (taban `main` @ 062d3b41)
- 1f9bf74b `[U2-02] tools/u2-gen-alpha.py: ...`
- b901cff4 `[U2-02] db/013 (35.860 esya) ve db/014 (82 NPC, 60 canavar) ...`
- 626d8763 `[U2-02] db/README.md: 013 ve 014 bolumleri`
- (bu rapor ve `Durum` satırı ayrı commit)

**Değişen dosyalar**
- `tools/u2-gen-alpha.py` (YENİ): alt komutlar `items`, `npcs`, `all` (varsayılan) ve `--check`. ALPHA (`.\SQL2019`/`FDP_alpha1534`) ve bizim DB'yi (`.\SQLEXPRESS`/`FDP_kn_online`) `SQLCMD.EXE -E` ile yalnız `SELECT` ile okur. Metin kolonlarını onaltılık (`varbinary`) olarak çeker, bayt kaybı olmaz. İstemci tablolarını `tools/kotbl.py` ile çözer.
  - Kolon listesi bizim `INFORMATION_SCHEMA`'dan gelir. Her değer bizim kolon tipine ve uzunluğuna göre denetlenir; sığmayan değer, NOT NULL kolona NULL, ASCII olmayan metin ve `$(` içeren metin açık hata verir.
  - Kapsam içindeki bir kimliğin ALPHA'da birden fazla satırı varsa açık hata verir. ALPHA `K_NPC`'de 27009 ve 32532 çift satırlı; ikisi de kapsam dışı.
  - Çıktı deterministiktir: kimliğe göre sıralı, düz ondalık sayılar, ASCII, LF.
- `db/013_u2_items_1534.sql` (8.315.037 B, 36.540 satır) ve `_rollback.sql`: üretilmiş.
- `db/014_u2_npc_monster_1534.sql` (44.749 B) ve `_rollback.sql`: üretilmiş.
- `db/README.md`: `## 013` ve `## 014` bölümleri eklendi (CRLF korundu).
- `tools/kotbl.py` değişmedi (`--selftest` OK).

**Betik deseni** (`db/012` ile aynı):
- Zorunlu sqlcmd değişkenleri: 013 için `Target`, 014 için `NpcTarget` ve `MonTarget`.
- `SET XACT_ABORT ON`.
- Satırlar önce oturumun geçici tablosuna yüklenir:
  - 013: `#u2_items`, 36 parti × en çok 1000 satır, her parti ayrı `GO` batch'i. Geçici tablo batch'ler arasında yaşıyor; aşağıdaki K4 testi bunu gösteriyor.
  - 014: `#u2_npcs` ve `#u2_monsters`.
- Ardından tek işlemde `INSERT … SELECT … WHERE NOT EXISTS (Num/sSid)` çalışır.
- Kayıt tabloları: `dbo.$(Target)_U2_ADDED (Num int PK, dtAdded)`, 014 için `sSid smallint PK`.
- Ek korumalar:
  - Geçici tablonun satır sayısı beklenenden farklıysa (ör. `-b` olmadan bir parti hata verdiyse) hiçbir satır eklenmez (`RAISERROR` + `RETURN`).
  - Son satırdaki `staged_not_in_target` sayacı, geçici tablodaki ve hedefte birebir aynı değerlerle bulunmayan satırları sayar (`EXCEPT`); temiz çalıştırmada 0'dır.
- Geri alma yalnız kayıt tablosundakileri siler, kayıt tablosunu düşürür ve tekrar çalıştırılabilir.

**K2: sayılar ve E raporuyla karşılaştırma** (üretici çıktısı, `python3 tools/u2-gen-alpha.py --check`)

| Ölçü | Bu uygulama | E raporu | Not |
|---|---|---|---|
| İstemcinin çözdüğü eşya kimliği | 121.706 | 121.706 | aynı |
| Bizde olmayan / ALPHA'da olan | 35.861 / **35.860** | 35.861 / 35.860 | aynı; 300177146 iki DB'de de yok |
| Bizde hiç satırı olmayan tabanların altında | 19.417 (204 taban) | 19.418 (205 taban, 204'ünde ALPHA satırı var) | fark, ALPHA'da olmayan tek kimlik |
| Bizde olan tabanların ek varyantı | 16.443 | 16.443 | aynı |
| ASCII olmayan eşya adı (yedek ad `Item <id>`) | 0 | — | ALPHA ITEM adlarının tamamı ASCII |
| K_NPC: bizde olmayan istemci kimliği / eklenen | 82 / **82** | 82 / 79 (plan) | **sapma 1**, aşağıda |
| K_MONSTER: bizde olmayan / eklenen | 65 / **60** | 65 / 60 | ALPHA'da olmayanlar: 4061, 8111, 8112, 8161, 8162 |

ItemClass dağılımı (eklenen 35.860 satır) ve E §3.4 tahmini:

| Sınıf | Bu uygulama | E tahmini | Fark nedeni |
|---|---|---|---|
| 0 | 15.701 | 15.644 | +57 aksesuar, sınıf 8 yerine 0 (madde 6) |
| 1 | 2.537 | 2.558 | −21: derece 1 "designated unique", sınıf 3 oldu (madde 4) |
| 2 | 4.305 | 4.305 | — |
| 3 | 8.313 | 8.292 | +21 (madde 4) |
| 4 | 4.742 | 4.742 | — |
| 8 | 262 | 320 | E'nin `itemclass6.py` betiği ALPHA satırlarında `itemext=-99` verir; `-99 != 0` doğru olduğu için derece 0 olan tüm ItemType 4 aksesuarları 8 saydı. Bu, kuralın harfiyen hâlinin 319'una karşılık gelir; aradaki 1 fark E'nin 35.861'lik paydası. |

ItemExt (sınıf 8): 18: 86, 19: 33, 20: 63, 21: 72, 23: 8. Değer 23, isimli yüzüklerin tabanlarına (330910000–330940000) özgüdür.

**K3: kuralın bizim satırlarımızdaki eşleşme oranı.** Ölçüm istemcinin çözdüğü kendi satırlarımız (85.845) üzerinde yapıldı:

| Kural | Eşleşme |
|---|---|
| Uygulanan kural (betik başlığındaki 7 madde) | **85.539 / 85.845 = %99,644** |
| ItemExt kuralı, bizim sınıf 8 satırlarımızda | 610 / 610 |

Kalan farklar (tahmin → bizdeki): 1→3: 162, 2→3: 69, 8→0: 37, 3→0: 11, 2→0: 10, 0→1: 6, 0→3: 6, 1→0: 4, 0→8: 1.

E §3.4 metni iki noktada belirsiz ya da eksikti; ölçerek netleştirdim (**sapma 2**):

| Varyant | Eşleşme | Not |
|---|---|---|
| E'nin %99,53 ölçümü | 85.438 / 85.845 = %99,526 | "upgradeable" yerine **bizim** `ItemExt` değerimizi kullanır. Döngüsel: yeni satırlara uygulanamaz, ALPHA'da `ItemExt` her yerde 0. |
| Harfiyen E, "upgradeable" = tüm ItemType 4 aksesuarlar | %99,423 | — |
| Harfiyen E, "upgradeable" = designated (istemci varyantının BaseID'si tabanı gösteriyor) | %99,483 | — |
| Yukarıdaki + madde 4'e "veya designated" eki (uygulanan kural) | %99,644 | Bizdeki derece 1 designated unique satırların 203'ü sınıf 3, 1'i sınıf 1 (`UpgradeHandler`'daki parşömen sınıfını etkiler). |

**K4: geçici kopya testi** (`.\SQLEXPRESS`, `FDP_kn_online`, `-E`)

Kopyalar: `ITEM_U202TEST`, `K_NPC_U202TEST`, `K_MONSTER_U202TEST` (`SELECT * INTO`). Son çalıştırma commit'lenmiş betiklerle yapıldı. Çıktı:
```
== 1. items apply #1   inserted=35860 already_present=0
                       target_rows=121780 logged=35860 staged_not_in_target=0   exit=0  (35,7 sn)
== 2. items apply #2   inserted=0 already_present=35860
                       target_rows=121780 logged=35860 staged_not_in_target=0   exit=0  (34,4 sn)
== 3. bot_in_item|bot_in_copy|bot_copy_except_orig|bot_orig_except_copy|bot_logged = 83|83|0|0|0
      orig_except_copy|unlogged_copy_except_orig = 0|0
== 4. items rollback #1  removed=35860 target_rows=85920 log_table=absent  exit=0
      items rollback #2  removed=0 target_rows=85920 log_table=absent      exit=0
      copy_except_orig|orig_except_copy|copy_rows = 0|0|85920
== 5. npc inserted=82 already_present=0 / monster inserted=60 already_present=0
      npc target_rows=603 logged=82 staged_not_in_target=0
      monster target_rows=850 logged=60 staged_not_in_target=0                 exit=0
      2. uygulama: npc inserted=0 already_present=82 / monster inserted=0 already_present=60
      npc_orig_except_copy|mon_orig_except_copy|warders_logged|warders_changed = 0|0|0|0
== 6. npc removed=82 target_rows=521 log_table=absent / monster removed=60 target_rows=790 log_table=absent
      2. geri alma: npc removed=0 ... / monster removed=0 ...
      n1|n2|m1|m2 (kopya EXCEPT orijinal, iki yön) = 0|0|0|0
== 7. u202_tables_left = 0; *_U2_ADDED olarak yalnız önceden var olan KNIGHTS_CAPE_U2_ADDED kaldı;
      gerçek tablolar item|npc|mon = 85920|521|790
```

- **Bot eşyaları:** E §3.3'teki 83 kimlik kullanıldı (db/002, 004–008). `db/007_bot_gear.sql` `main`'de yok; son sürümü (934d356f) salt okundu ve 35 kimliğinin hepsi bu 83'ün içinde. Test sırasında bot satırlarıyla orijinal arasındaki `EXCEPT` iki yönde de 0 çıktı ve hiçbiri kayıt tablosuna girmedi.
- **Hata durumları da denendi:**
  - `Target` verilmezse: `'Target' scripting variable not defined.`, exit=1.
  - Tablo yoksa: `target table dbo.ITEM_NO_SUCH_TABLE not found`, exit=1, kayıt tablosu oluşmadı.
- **Uygulamadan sonra `--check`:** Bizim DB okumaları kopyalara yönlendirilerek benzetildi. Kopyalar uygulanmış durumdayken üretilen dört dosya depodakilerle bayt bayt aynı çıktı ("same" × 4). Üretici `*_U2_ADDED`'daki kimlikleri "bizde var" saymadığı için canlı uygulamadan sonra da K1 0 kalır.

**Bellek etkisi**
- `ITEM` satır sayısı 85.920 → 121.780 (+35.860, +%41,7).
- `sp_spaceused` tablo verisi: 14.632 KB → 20.736 KB.
- GameServer: `_ITEM_TABLE` Win32'de yaklaşık 132 B, artı `std::map` düğümü ve yığın ek yükü. Yeni adların 25.984'ü 15 karakteri aştığı için ayrıca yığın ayırır. Toplam tahmini **+7–8 MB [A]**; ölçülmedi.
- AIServer `ITEM` yüklemez.
- `K_NPC` +82, `K_MONSTER` +60 satır; etkisi önemsiz.

**K5**
```
$ grep -c "24438\|24439\|24440" db/014_u2_npc_monster_1534.sql
1
143:    (14440, 'Laiva Village Board', 24440, 100, 0, 0, 2, 1, 98, ...
$ grep -cE "^    \((24438|24439|24440), " db/014_u2_npc_monster_1534.sql
0
```
- Planın grep'inin bulduğu tek satır, NPC 14440'ın (Laiva Village Board) **`sPid` = 24440** görünüm (look) değeridir. Bu bir NPC kimliği değil, ALPHA değeri olduğu gibi kaldı. Satır kimliği olarak 24438, 24439 ve 24440 betikte yok.
- Üretici her çalıştırmada bunu denetler: VALUES satır kimlikleri seçilen kümeye eşit olmalı ve dışlanan kimlik içermemeli. Başlık bu kimlikleri rakamla değil "zone-64 warders" diye anar.
- Betikteki kimliklerin istemci tablolarıyla karşılaştırması: npc 82 satır, `Npc_us`'ta olmayan 0; monster 60 satır, `Mob_us`'ta olmayan 0; çift kimlik 0. Eşya: 35.860 satır, çift kimlik 0; tamamı istemcinin çözdüğü kimlikler (kapsam bu şekilde kuruldu).
- ALPHA'nın istemcide olmayan kimliklerinin hiçbiri betikte yok.

**K6**
- Dört SQL dosyası ve üretici için `file`: "ASCII text"; CR sayısı 0, ASCII olmayan bayt 0.
- Yorumlar İngilizce.
- Betik başlıklarında şunlar yazılı:
  - kaynak: ALPHA tablo, okunan satır sayısı, kapsam içi satırların sha256'sı; istemci dosyaları, bayt sayısı ve sha256
  - bizim taban çizgimiz
  - kapsam
  - ItemClass/ItemExt kuralı ve ölçümü
  - ad kuralı
  - kullanım, idempotentlik ve geri alma

**K1**
```
$ python3 tools/u2-gen-alpha.py --check
... same: db/013_u2_items_1534.sql / same: db/013_u2_items_1534_rollback.sql
    same: db/014_u2_npc_monster_1534.sql / same: db/014_u2_npc_monster_1534_rollback.sql
check OK            (exit 0, ~6 sn)
```

**K7**
```
$ git diff --stat main...bot/U2-02      (rapor commit'inden önce)
 db/013_u2_items_1534.sql                | 36540 ++++
 db/013_u2_items_1534_rollback.sql       |    49 +
 db/014_u2_npc_monster_1534.sql          |   467 +
 db/014_u2_npc_monster_1534_rollback.sql |    76 +
 db/README.md                            |   100 +
 tools/u2-gen-alpha.py                   |  1077 +
```
Rapor commit'iyle buna yalnız bu plan dosyası eklenir (`Durum` satırı ve bu bölüm; AGENTS §2.2). `git status --short` temiz.

**Derleme:** Çalıştırılmadı. Bu planda derlenen dosya (C++/vcxproj) değişmedi ve kabul kriterlerinde derleme yok.

**Kriter öz-değerlendirmesi**

| Kriter | Sonuç | Not |
|---|---|---|
| K1 | geçti | |
| K2 | geçti | sapmalar açıklandı |
| K3 | geçti | %99,644 ≥ %99,5; kural netleştirmesi sapma 2'de |
| K4 | geçti | |
| K5 | geçti, açıklamalı | planın grep'i 1 satır sayar: 14440'ın `sPid` değeri |
| K6 | geçti | |
| K7 | geçti | |

**Plandan sapmalar**
1. **K_NPC 79 değil 82.** 24438, 24439 ve 24440 **bizim** `K_NPC`'mizde zaten var (Warder 1, Warder 2, Keeper; byType 11). Bu yüzden E'nin "bizde olmayan 82" kümesinde hiç yer almazlar. E §8'deki "82 eksi 3 = 79" hesabı hatalı. Dışlama yine de üreticide açıkça uygulanır: bekçileri olmayan bir DB'de bu kimlikler eklenmez.
2. **ItemClass kuralı netleştirildi** (K3 tablosu):
   - "upgradeable": istemcinin Item_Ext varyantı tabanı BaseID olarak gösteriyor (designated).
   - Madde 4'e "veya designated" eklendi.
   - ItemExt'e isimli yüzük tabanları için 23 istisnası eklendi. Kaynak: bizim ITEM'deki 36 satır ve ITEM_UPGRADE'deki nOriginType 23 tarifleri.
   - Harfiyen E kuralı (%99,423 / %99,483) K3 eşiğinin altında kalıyordu.
3. **`sLightR` ve `byMoneyType`** (bizde NOT NULL ve varsayılansız, ALPHA'da yok):
   - Plan ve E §3'e göre bu durum "açık hata" olmalıydı. Bunun yerine üreticideki dar bir izin listesiyle (`OURS_ONLY_FILL`) `0` yazılıyor.
   - Üretici, sunucunun `NpcTableSet.h` yükleyicisinin bu kolonları okumadığını her çalıştırmada denetler; okursa hata verir. Kaynakta başka kullanım yok (`grep`).
   - Bu izin listesi dışındaki her NOT NULL kolon hâlâ açık hatadır.
4. **Ad kuralına "kolona sığar" koşulu eklendi.** İstemci adlarından 4'ü `varchar(30)`'u aşıyor ya da ASCII değil:

   | Kimlik | Durum | Kullanılan ad |
   |---|---|---|
   | 14438 | 41 karakter | ALPHA adı `[Reserve Knight Capt` |
   | 19004 | 33 karakter | ALPHA adı |
   | 24434 | 32 karakter | ALPHA adı |
   | 24437 | U+2019 içeriyor | ALPHA adı `[Cardinal Servant] T` |
   | K_MONSTER 4063 | istemci adı 31 karakter, ALPHA adı CP949 Korece | `Npc 4063` |

   Kesip yazmak yerine plandaki zincir izlendi.
5. **Taban çizgisi `*_U2_ADDED`'ı hariç tutar.** Bu planda yazılı değil; `--check`'in canlı uygulamadan sonra da 0 kalması için gerekli.
6. **`db/013` boyutu 8,3 MB.** Planın 15–20 MB tahmininin altında; tek dosya.
7. Kayıt tablolarında `db/012`'deki gibi bir `dtAdded` kolonu da var (planın şemasında yalnız kimlik vardı).

**Açık sorular**
1. K_NPC sayısı 82 (sapma 1) kabul mü?
2. `sLightR`/`byMoneyType` = 0 kabul mü? Alternatif: bizdeki benzer NPC'lerden değer seçmek; sunucu okumadığı için davranış farkı yok.
3. Netleştirilmiş ItemClass kuralı (sapma 2) kabul mü? Harfiyen kurala dönülürse etki: 21 satır sınıf 3→1, 57 satır sınıf 0→8.
4. 4063 için `Npc 4063` mü, kesilmiş istemci adı (`Elmorad Castle Gate of War Zon`) mı?
5. Bilgi: yeni 60 canavarın `sItem` düşürme tabloları bizde yok, dolayısıyla düşürmeleri olmayacak. Bu, plan kapsamı dışında (E §4.1).

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-08

**Hüküm: DOĞRULANDI.** Kanıt: kendi koşum (duman DB'si `FDP_smoke1534`, canlı DB'nin kopyası) + kod/başlık incelemesi.

| K | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `python3 -I tools/u2-gen-alpha.py --check` → `check OK`, rc 0 |
| K2 | ✔ | 35.860 eşya (E ile aynı), K_NPC 82 (E'nin "79" hesabı hatalı: 24438–24440 zaten bizde, eksikler arasında hiç yoktu), K_MONSTER 60 |
| K3 | ✔ | Kural eşleşmesi %99,644 (uygulayıcı; kuralın netleştirilmiş hali betik başlığında) |
| K4 | ✔ | `FDP_smoke1534`'te: 013 `inserted=35860` (36 s), tekrar `inserted=0`; 014 NPC 82 / canavar 60; geri alma `removed=35860`, `82`, `60`, tablolar 85.920/521/790'a döndü; yeni satırlarda `ItemClass IS NULL` 0. Yeniden uygulandı (duman ortamı için). Duman sunucuları bu verilerle yeniden başlatıldı: AIServer/GameServer/LogInServer UP, bot altyapısı hazır |
| K5 | ✔ | Satır kimliği olarak 24438/24439/24440 yok (`^\((24438|24439|24440),` → 0); tek eşleşme 14440'ın `sPid` değeri |
| K6 | ✔ | ASCII, LF |
| K7 | ✔ | diff yalnız §4 + plan; temiz |

Sapmalar kabul: (1) K_NPC 82; (2) ItemClass kuralının netleştirilmesi (adlı unique varyant = yükseltilebilir; 4 adlı yüzük ItemExt 23); (3) `sLightR`/`byMoneyType` = 0 (sunucu okumuyor); (4) 5 ad geri dönüş zinciri, 4063 → `Npc 4063` (kabul; görünür etkisi yok). Canlı DB uygulaması Claude tarafından birleştirmeden sonra yedekle yapılır.
