# U3-02: Yeni Moradon DB betiği (`db/015_u3_moradon_1534.sql`): ZONE_INFO, START_POSITION, K_OBJECTPOS, K_NPCPOS (zone 21)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | U3 — Sürüm yükseltme 1534, yeni Moradon (`docs/17` §2 U, ADR-0068 Ek 2) |
| Branch | `bot/U3-02` (taban: `main`) |
| Bağımlı olduğu planlar | U2-02 (KAPANDI; zone 21 NPC/canavar kimlikleri), U3-01 (SMD dosya adı `moradon_1534.smd`; paralel yazılabilir) |
| İlgili gereksinim / kabul | T-UPG-02; `docs/reports/u0-1534/G-yeni-moradon-smd.md` §5, §7 "DB / scripts", `E-veri-farki.md` §4.3 |
| Tahmini büyüklük | S–M (1 üretici, 2 SQL, README) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

Yeni Moradon'un DB tarafını tek, geri alınabilir betikte toplamak: zone 21'in harita dosyası adı ve başlangıç noktaları, kapı/örs nesneleri ve NPC/canavar yerleşimi. Betik **ayrı 1534 DB kopyasına** uygulanacak (ADR-0068 Ek 2 madde 4; canlı `FDP_kn_online`'a değil — eski 1453 istemcisinin Moradon'u bozulmasın). Uygulama Claude'un işidir.

## 2. Bağlam (okunması zorunlu)

- G raporu §5 (warp grupları, nesneler `K_OBJECTPOS` 4013/4014/5001, ALPHA futbol nesneleri 1019–1022 **alınmaz**), §7 sonundaki "DB / scripts" listesi, §8 denetimleri.
- E raporu §4.3 (zone 21 yerleşimi ALPHA vs bizim; istemci minimap kanıtı).
- Betik kalıbı: `db/013_u2_items_1534.sql`, `db/014_u2_npc_monster_1534.sql` (+ rollback), `tools/u2-gen-alpha.py` (ALPHA'yı `SQLCMD` ile okuyan deterministik üretici; `--check`).
- Sunucu yükleyicileri: `shared/database/ZoneInfoSet.h`, `StartPositionSet.h`, `ObjectPosSet.h`, `NpcPosSet.h`; AIServer NPC yerleşimi.
- ALPHA DB: `.\SQL2019` → `FDP_alpha1534` (`SELECT`). Bizim DB: `.\SQLEXPRESS` → `FDP_kn_online` (`SELECT`; test için kopya tablolar).

## 3. Kapsam

**Var:**
1. `tools/u3-gen-moradon-db.py` (veya `tools/u2-gen-alpha.py`'ye `moradon` alt komutu — hangisi daha az kopya üretiyorsa; raporda gerekçe): ALPHA'dan zone 21 `K_NPCPOS` satırları (kimlikleri bizim `K_NPC`/`K_MONSTER`'da **olmayan** satır varsa betik hatayla durur — U2-02 sonrası hepsi olmalı), `K_OBJECTPOS` zone 21 (futbol 1019–1022 hariç; nesne türü 50 efektler raporla), ve sabitler.
2. `db/015_u3_moradon_1534.sql` (+ `_rollback.sql`), `SET XACT_ABORT ON`, tek işlem:
   - Yedek tabloları: `ZONE_INFO_Z21_U3_BACKUP`, `START_POSITION_Z21_U3_BACKUP`, `K_OBJECTPOS_Z21_U3_BACKUP`, `K_NPCPOS_Z21_U3_BACKUP` (yalnız ilk çalıştırmada doldurulur; ikinci çalıştırma yedeğe dokunmaz).
   - `ZONE_INFO` 21: `strZoneName='moradon_1534.smd'`, `InitX/InitZ/InitY = 81590/53079/469`, `RoomEvent = 0`.
   - `START_POSITION` 21: her iki ulus 817/530, `bRangeX = bRangeZ = 10`, kapı kolonları G §7'ye göre (bugünkü yanlış yerleşim düzeltilir).
   - `K_OBJECTPOS` 21: bizim zone 21 satırları silinir, ALPHA'dan seçilenler eklenir.
   - `K_NPCPOS` 21: bizim zone 21 satırları (138) silinir, ALPHA'nın satırları (≈123) eklenir.
   - Sonunda sayılar (`zone_info=1 start=1 objpos=… npcpos=…`).
   - Yeniden çalıştırılabilir (aynı son durum).
3. Rollback: yedeklerden zone 21'i geri yazar ve yedek tablolarını düşürür; tekrar çalıştırılabilir.
4. `db/README.md`: `## 015` (yalnız 1534 DB'sine uygulanır uyarısıyla).

**Yok:** Lua (U3-03), `USERDATA` konum sıfırlaması (proje sahibi kararı), warp ücretleri (SMD içinde; U3-01), canlı DB'ye uygulama.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `tools/u3-gen-moradon-db.py` **veya** `tools/u2-gen-alpha.py` | üretici |
| `db/015_u3_moradon_1534.sql`, `db/015_u3_moradon_1534_rollback.sql` | YENİ (üretilmiş) |
| `db/README.md` | `## 015` |

## 5. Uygulama adımları

1. Üreticiyi yaz; betikleri üret; `--check` 0.
2. Bizim zone 21 satırları ile ALPHA'nınkileri tabloya koy (sayı, kimlik grupları; satır içeriği kişisel veri değil — referans tablolar).
3. **Test:** `FDP_smoke1534` duman DB'sinde (`.\SQLEXPRESS`, U2 verisi uygulanmış canlı kopya) uygula → sayılar; tekrar uygula → aynı son durum; geri al → `EXCEPT` ile 4 tablonun zone 21 satırları uygulama öncesiyle aynı; yeniden uygula (duman ortamı için bırak). **`FDP_kn_online`'a yazma.**
4. Doğrulama noktaları: yeni `START_POSITION` ve tüm `K_NPCPOS` merkezleri yeni haritanın sınırları içinde (0..1024).

## 6. Kabul kriterleri

- [ ] K1: Üretici `--check` 0; betik başlığında kaynak, kapsam ve sayılar.
- [ ] K2: §5.3 testi: sayılar, tekrar uygulama, geri alma `EXCEPT` 0, yeniden uygulama; `FDP_kn_online` dokunulmadı (önce/sonra zone 21 satır sayıları aynı).
- [ ] K3: Futbol nesneleri 1019–1022 betikte yok; zone 21 `K_NPCPOS` satırlarının tüm NPC/canavar kimlikleri bizim tablolarda mevcut.
- [ ] K4: SQL ASCII, LF; yorumlar İngilizce.
- [ ] K5: `git diff --stat main...bot/U3-02` yalnız §4; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
python3 -I tools/<üretici> --check
SQLCMD="/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_smoke1534 -b -i "$(wslpath -w db/015_u3_moradon_1534.sql)"
git diff --stat main...bot/U3-02
git status --short
```

## 8. Kısıtlar ve uyarılar

- Yazma yalnız `FDP_smoke1534`'e (test); canlı `FDP_kn_online`'a ve ALPHA DB'ye yazma yok. Kişisel veri tabloları okunmaz.
- İndirilen paketteki exe/dll çalıştırılmaz.
- Git: `AGENTS.md` §2.8; commit `[U3-02] ...`.

---

## Uygulayıcı Raporu

### Tur 1

**Durum:** UYGULANDI (Claude uygulayıcı ajanı, 2026-10-08; proje sahibinin bu faz için istediği şekilde DeepSeek yerine).

**Branch ve commit'ler:** `bot/U3-02` (taban `main` @ 435cf68e)
- e2cd99fc `[U3-02] u3-gen-moradon-db.py ve db/015: yeni Moradon zone 21 DB betigi + geri alma`
- 6f1514ff `[U3-02] db/README.md: ## 015 (yalniz 1534 DB kopyasina uygulanir)`
- (bu rapor ve `Durum` satırı ayrı commit)

**Değişen dosyalar**
- `tools/u3-gen-moradon-db.py` (YENİ): üretici; `--check` ile depodaki betikleri bayt bayt karşılaştırır.
  - **Neden ayrı dosya:** `tools/u2-gen-alpha.py`'ye alt komut eklemek yerine onu yol ile yükler (`importlib`, `kotbl.py` deseni) ve `Db`, `wrap`, `comment_block`, `INT_RANGES`, `logged_ids` yardımcılarını yeniden kullanır. Kopyalanan kod yok. U2 aracı ve onun `--check`'i değişmedi (`u2-gen-alpha.py npcs --check` → `check OK`). Bu planın ek ihtiyaçları U2 aracında yok ve oraya eklemek onun `all` akışını ve istemci klasörü bağımlılığını karıştırırdı: `float` ve `text` kolonları, yedek tablosu, zone 21 sabitleri.
  - Okuma: ALPHA (`.\SQL2019`/`FDP_alpha1534`) ve bizim DB (`.\SQLEXPRESS`/`FDP_kn_online`), yalnız `SELECT`. Metinler onaltılık, `float`'lar `binary(8)` olarak çekilir, kayıp olmaz. `text` kolonu ayrıca `DATALENGTH` ile karşılaştırılır.
  - Koddan denetimler:
    - `shared/packets.h` `enum ObjectType` → bilinen nesne türleri.
    - `AIServer/ServerDlg.cpp` → `bMonster = (bActType < 100)` ve Limit* yalnız `DungeonFamily > 0`. Satırlar değişirse üretici durur.
  - Hata ile durduğu durumlar:
    - bizim `K_NPC`/`K_MONSTER`'da olmayan yerleşim kimliği (ActType < 100 → `K_MONSTER`, değilse `K_NPC`)
    - `DungeonFamily ≠ 0`
    - harita dışı koordinat (0..1023): başlangıç + aralık, `Init`/100, nesne, dikdörtgen köşesi, yol noktası
    - yol yürüyen (hareket türü 2/3) satırda sayısal olmayan `path`
    - ALPHA'nın `ZONE_INFO` / `START_POSITION` satırının sabitlerden farklı olması
    - seçilen nesnelerin {4013, 4014, 5001}'den farklı olması
    - futbol satırlarının {1019..1022}'den farklı olması
  - Bizim taban satırlar `*_Z21_U3_BACKUP` tabloları varsa onlardan okunur (kesin geçişte bizim DB'ye uygulanırsa `--check` yine 0 kalır).
- `db/015_u3_moradon_1534.sql` (28.7 KB) ve `db/015_u3_moradon_1534_rollback.sql` (10.2 KB): üretilmiş; ASCII, LF, yorumlar İngilizce.
- `db/README.md`: `## 015` bölümü (yalnız 1534 DB kopyası uyarısıyla; CRLF korundu).

**Betik tasarımı** (`db/013`/`014` deseni; değişken yok, §7 komutu aynen çalışır)
- `SET XACT_ABORT ON`. Satırlar oturumun geçici tablolarına yüklenir: `#u3_objpos` (3), `#u3_npcpos` (123).
- Ardından tek batch ve tek işlemde şunlar yapılır:
  - denetimler (hepsi `RAISERROR` + `RETURN`, hiçbir şey değişmez):
    - DB adı `FDP_kn_online` değil
    - geçici tablo sayıları doğru
    - yedek tablo sayısı 0 ya da 4
    - zone 21'de `ZONE_INFO` ve `START_POSITION` satırları birer tane
    - yerleşim kimliklerinin hepsi hedefteki `K_MONSTER`/`K_NPC`'de var
  - yedekler yalnız ilk çalıştırmada alınır (`SELECT … INTO dbo.<tablo>_Z21_U3_BACKUP`)
  - `ZONE_INFO`/`START_POSITION` yerinde `UPDATE` edilir
  - `K_OBJECTPOS`/`K_NPCPOS`'ta zone 21 satırları silinip yeniden yazılır
- Doğrulama batch'i: sabit kolonlar, sayılar ve `objpos_differ`/`npcpos_differ`. Bunlar geçici tablo ile hedef arasındaki **çoklu küme** karşılaştırmasıdır: iki yönde `GROUP BY … COUNT(*)` + `EXCEPT`. Beklenen değil ise `RAISERROR` verir.
- Geri alma:
  - `ZONE_INFO`/`START_POSITION` yedekten `UPDATE` edilir; `K_OBJECTPOS`/`K_NPCPOS` zone 21 silinip yedekten yazılır.
  - Yedekle `INTERSECT`/`EXCEPT` denetimi yapılır; uymazsa `ROLLBACK`.
  - Ardından yedek tabloları düşürülür.
  - Yedek tablosu yoksa `restored=0 backup_tables=absent`.

**Değerler (G §5.3, §7)**

| Tablo | Bizim (önce) | 015 sonrası |
|---|---|---|
| `ZONE_INFO` 21 | `moradon_20060124.smd`, Init 31200/40200/0, RoomEvent 21 | `moradon_1534.smd`, Init 81590/53079/469, RoomEvent 0 (ServerNo/Type/bz bizim) |
| `START_POSITION` 21 | 306/352 ×2, kapı kolonları 10/10/0/0, aralık 0/0 | 817/530 ×2, kapı kolonları 0/0/0/0, aralık 10/10 (= ALPHA satırı) |
| `K_OBJECTPOS` 21 | 4013 (338.10, 318.10), 4014 (282.04, 373.94), 5001 (337.89, 389.95) | 4013 (837.36, 526.76), 4014 (797.70, 526.88), 5001 (816.14, 607.17) |
| `K_NPCPOS` 21 | 138 satır, 58 kimlik, NumNPC 531 | ALPHA'nın 123 satırı, 75 kimlik, NumNPC 423 |

**§5.2: zone 21 yerleşimi, bizim ve ALPHA** (üretici çıktısı; E §4.3 ile aynı)

| | ALPHA | Bizim |
|---|---|---|
| Satır / kimlik / NumNPC | 123 / 75 / 423 | 138 / 58 / 531 |
| NPC (ActType ≥ 100) satır / kimlik | 48 / 38 | 51 / 27 |
| Canavar satır / kimlik | 75 / 37 | 87 / 31 |

| Kimlik grubu | NPC | Canavar |
|---|---|---|
| Ortak | 21 | 19 |
| Yalnız ALPHA, `db/014` ekledi | 13016, 19001–19006, 19067–19072, 29001 (14) | 1056, 1057, 1058, 1180 (4) |
| Yalnız ALPHA, zaten bizde vardı | 14401, 14402, 24414 | 256, 353, 452, 552–555, 650, 652, 653, 950, 953, 954, 1154 (14) |
| Yalnız bizim (silinir) | 521, 11020, 13003, 16073, 16074, 21020 | 152, 153, 161, 252, 255, 352, 751, 754, 755, 851, 853, 854 |

ALPHA `K_OBJECTPOS` 21: toplam 33 satır.
- **Alınanlar:** 4013, 4014, 5001.
- **Alınmayanlar:**
  - futbol 1019–1022 (tür 0, x 632–710, z 140–180)
  - 26 adet tür 50 satırı (efekt, hepsi `sIndex` 0). Tür 50, bizim `enum ObjectType`'ta yok ve hiçbir sunucu kodu kullanmıyor. Bizim DB'de hiçbir zone'da tür 50 yok (tür histogramı: 0, 1, 3, 5, 6, 8, 9).

**K1:** `python3 -I tools/u3-gen-moradon-db.py --check` → `same: db/015_u3_moradon_1534.sql`, `same: db/015_u3_moradon_1534_rollback.sql`, `check OK`, çıkış 0.
- Başka bir çalışma dizininden de çıkış 0.
- Negatif deneme: rollback dosyasına 1 bayt eklenince `DIFFERENT … check FAILED (1 file(s) differ)`, çıkış 1; geri konunca 0.
- Betik başlığında şunlar var:
  - kaynak: ALPHA tablo/satır sayıları + okunan satırların sha256'sı `d9ca9189…`
  - kimlik denetiminin yapıldığı DB (`K_NPC` 603, `K_MONSTER` 850)
  - bizim taban değerlerimiz
  - kapsam, alınmayanlar ve nedenleri
  - beklenen çıktı

**K2: `FDP_smoke1534` testi** (§7 komutu: `-S .\SQLEXPRESS -E -d FDP_smoke1534 -b -i …`)

Önce `FDP_smoke1534` ile `FDP_kn_online` karşılaştırıldı. Dört tablonun hem zone 21 hem diğer zone satırları çoklu küme `EXCEPT` ile iki yönde de 0 çıktı; bayt düzeyindeki anlık görüntüler de aynıydı. U3 yedek tablosu yoktu.
```
== uygula #1   backup=created deleted_objpos=3 deleted_npcpos=138 inserted_objpos=3 inserted_npcpos=123
               zone_info=1 start=1 objpos=3 npcpos=123 objpos_differ=0 npcpos_differ=0      exit=0
   ALPHA ile birebir (double/bayt): K_OBJECTPOS 3=3 True, K_NPCPOS 123=123 True, ZONE_INFO/START_POSITION = sabitler -> ALL_EQUAL
   yedek tabloları = uygulama öncesi zone 21 satırları (4 tablo, bayt düzeyi aynı)
   diğer zone'lar (EXCEPT, kn_online'a karşı): 0|0 x 4 tablo
== uygula #2   backup=kept deleted_objpos=3 deleted_npcpos=123 inserted_objpos=3 inserted_npcpos=123
               zone_info=1 start=1 objpos=3 npcpos=123 objpos_differ=0 npcpos_differ=0      exit=0
   son durum (tablolar + yedekler) #1 ile bayt düzeyi aynı
== geri al #1  restored=1 zone_info=1 start=1 objpos=3 npcpos=138 objpos_differ=0 npcpos_differ=0 backup_tables=absent   exit=0
   EXCEPT (smoke vs FDP_kn_online = uygulama öncesi), iki yön:
     ZONE_INFO zone21 0|0, other 0|0;  START_POSITION zone21 0|0, other 0|0
     K_OBJECTPOS zone21 0|0, other 0|0; K_NPCPOS zone21 0|0, other 0|0
   anlık görüntü uygulama öncesiyle bayt düzeyi aynı (yedek tabloları yok)
== geri al #2  restored=0 backup_tables=absent   exit=0
== yeniden uygula (duman ortamı için bırakıldı)
               backup=created deleted_objpos=3 deleted_npcpos=138 inserted_objpos=3 inserted_npcpos=123
               zone_info=1 start=1 objpos=3 npcpos=123 objpos_differ=0 npcpos_differ=0      exit=0
   durum #1 ile bayt düzeyi aynı; ALPHA ile ALL_EQUAL
```
- **`FDP_kn_online` dokunulmadı.** Önce ve sonra şunlar aynı: `zi|sp|obj|npc = 1|1|3|138`, `CHECKSUM_AGG` (4 tablo), `K_NPCPOS` 2276 / `K_OBJECTPOS` 219 satır, U3 yedek tablosu 0. Zone 21 anlık görüntüsü bayt düzeyinde aynı. Bu DB'ye hiçbir betik çalıştırılmadı; yalnız `SELECT` yapıldı.
- **Float'lar:** literaller gerçek double'ın en kısa `repr`'i + `E0` olarak yazıldı (ör. `4.78032E0`). Uygulama sonrası değerler ALPHA'nın `binary(8)` değerleriyle aynı (yukarıdaki `ALL_EQUAL`).
- **§5.4 sınırlar** (uygulanmış duman DB'sinden):
  - `K_NPCPOS` merkezleri x 48–907, z 166.5–972.5; 0..1023 dışında köşe 0
  - başlangıç 817..827 × 530..540
  - `Init`/100 = (815.90, 530.79)
  - nesneler: yukarıdaki tablo
  - Üretici, yol noktalarını da denetler (19070–19072: (758..776, 390..406)).

**K3**
- Futbol nesneleri: `grep -nE '\b10(19|20|21|22)\b' db/015_u3_moradon_1534.sql` yalnız yorum satırı 29'u ("Not copied: the soccer objects …") bulur. `#u3_objpos` VALUES satırları yalnız 4013, 4014, 5001'dir; uygulanmış DB'de `sIndex BETWEEN 1019 AND 1022` 0 satır.
- Kimlikler: üretici `FDP_kn_online`'a karşı denetler (eksik olsa durur). Betik de hedef DB'de denetler (`@missing_mon`/`@missing_npc`). Uygulanmış `FDP_smoke1534`'te sonuç: `rows21=123 missing_in_K_MONSTER=0 missing_in_K_NPC=0 resolved=123`. 75 kimliğin 75'i `FDP_kn_online`'da da var.

**K4:** iki SQL dosyası ve üretici `ASCII text`. CR 0, ASCII dışı bayt 0, `$(` 0 (`file` SQL dosyalarına sezgisel olarak "HTML document, ASCII text" diyor). `git ls-files --eol`: `i/lf w/lf attr/text eol=lf`. Yorumlar İngilizce.

**K5:** `git diff --stat main...bot/U3-02` (rapor commit'inden önce): yalnız §4 dosyaları (`db/015_u3_moradon_1534.sql` 516, `_rollback.sql` 233, `db/README.md` 77, `tools/u3-gen-moradon-db.py` 986 satır). Bu commit'le plan dosyası da eklenir (yalnız `Durum` ve bu bölüm). `git status --short` boş.

**Derleme:** Çalıştırılmadı. Bu planda derlenen dosya (C++/vcxproj) değişmedi ve kabul kriterlerinde derleme yok (U2-02 ile aynı). Sunuculara dokunulmadı.

**Plandan sapmalar / yorumlar**
1. **`FDP_kn_online` koruması (ek):** betik `DB_NAME() = 'FDP_kn_online'` ise hata verip hiçbir şeyi değiştirmez. Bu, ADR-0068 Ek 2 madde 4'ü uygular. Kesin geçişte (tek DB) üreticideki `REFUSED_DATABASE` sabiti kaldırılıp betik yeniden üretilmelidir. Bu yol test edilmedi; `FDP_kn_online`'a hiçbir betik çalıştırılmadı.
2. **Tür 50 satırları alınmadı.** Plan "raporla" diyordu; rapor betik başlığında ve burada. Sunucuda karşılığı yok. `sIndex` 0 oldukları için `GetObjectEvent(0)` aramalarına da karışabilirlerdi; `m_sBind` varsayılanı -1 olduğundan pratikte karışmazlar.
3. **ALPHA değerleri olduğu gibi kopyalandı:**
   - 123 satırın hepsinde `LimitMinX = TopZ`, `LimitMinZ = LeftX` (ad olarak yer değiştirmiş). AIServer bunları yalnız `DungeonFamily > 0` iken okur; hepsi 0.
   - 13 satırda `path` 4 karakterlik `'NULL'` metni. Yükleyici `memset` + `atoi` ile bunu boş yol gibi okur. 2'si (11021) `DotCnt 2`, ActType 100: duran NPC, yol kullanılmaz.

**Açık sorular**
1. `LimitMinX/LimitMinZ` düzeltilsin mi (yer değiştirsin mi)? Çalışma zamanında etkisi yok; şimdilik ALPHA ile birebir.
2. Tür 50 efekt satırları istenirse üreticiye eklenebilir. Sunucu kullanmıyor; önerim almamak.
3. `FDP_kn1534`'e uygulamadan önce orada `db/014`'ün uygulanmış olması gerekir; betik eksik kimlikte durur. Bu DB'ye erişim görev kapsamım dışındaydı, okunmadı.

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-08

**Hüküm: DOĞRULANDI.** Kanıt: kendi koşum (`--check`) + betik incelemesi + uygulayıcının `FDP_smoke1534` test kanıtı (`scratchpad/u302/`).

| K | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `python3 -I tools/u3-gen-moradon-db.py --check` → `check OK`; `u2-gen-alpha.py --check` hâlâ OK (yükleyerek yeniden kullanım) |
| K2 | ✔ | Duman DB'sinde uygula/tekrar/geri al (`EXCEPT` 0)/yeniden uygula; `FDP_kn_online` önce/sonra aynı |
| K3 | ✔ | 1019–1022 yok; 75 kimliğin hepsi tablolarda (betik de hedefte denetler) |
| K4 | ✔ | ASCII, LF |
| K5 | ✔ | diff yalnız §4 + plan |

Ek emniyet: betik `FDP_kn_online`'da çalışmayı reddeder (ilk parti `-b` ile durur; ana işlem partisi `DB_NAME()` tekrar denetler ve `RETURN`). Tek DB geçişinde `REFUSED_DATABASE` kaldırılıp betik yeniden üretilmeli. Kabul edilen kararlar: ALPHA `LimitMin` değerleri olduğu gibi (çalışma zamanı etkisi yok), tür 50 efekt satırları alınmadı.
