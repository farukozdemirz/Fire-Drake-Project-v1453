# F8-03: İlk 8v8 için 16 bot karakteri: `db/005_bot_characters_16.sql` (+ geri alma) ve `BOT_TABLE` genişletmesi (ulus başına 8, C8-A; ADR-0002 Ek F8-02 madde 5)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F8 — Değerlendirme ve 8v8 (`docs/17` §2; kapı G8, `docs/17` §5) |
| Branch | `bot/F8-03 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `db/002` F1-04 `KAPANDI` (satır biçimi, değişmezler), F4-27 `KAPANDI` (`db/003` quest kalıbı), F4-40 `KAPANDI` (`db/004` stok kalıbı), F8-02 `KAPANDI` (`tools/bot-composition-check.py`, kabul aracı). **F6/F7'ye bağımlı değildir**; EVAL-8v8-A koşulmadan önce biter. F8-04 ile bağımsızdır (aşağıda §0) |
| İlgili gereksinim / kabul | `docs/15` §6a "Karakter seti ve kapasite" (satır 283-291: 16 asgari, C8-A), `docs/09` §2.3/§2.4 (C8-A, REQ-PTY-02), `docs/04` §3.3 (satır 65-67), `docs/17` G8, T-DATA-01, T-IGT-EVAL-01 ön koşulu, ADR-0002 Ek F8-02 (ad kuralı), `docs/reports/degerlendirme-takip.md` M6 |
| Tahmini büyüklük | S/M (3 yeni/değişen SQL-doküman dosyası + `BotManager.cpp` tek tablo; ≤ 6 dosya) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak; kod referansları `gece/2026-10-02` @ `7891f74`) |

---

## 0. Neden TASLAK (HAZIR yapma ön koşulları)

Bu plan F6/F7'yi bloklamaz ama 8v8 ölçümünün (EVAL-8v8-A) ön koşuludur; F7 sonrası zamanlanır. HAZIR yapmadan önce:

1. **Proje sahibi izni (DB'ye yazma).** Betik yerel `FDP_kn_online` bot satırlarına yazar (`AGENTS.md` §2.7 "yalnızca plan SQL dosyası" kuralı içinde), ama STATUS'taki kayıt "DB yazma izni sizde" der. İzin satırı plana yazılmadan HAZIR olmaz.
2. **`BOT_TABLE` yöntemi (aşağıda §3.2, karar K-?).** `docs/15` §6a (satır 291) "sabit 12 girişten DB/ini kaynaklı tabloya çevrilmesi" der. Bu plan **sabit tabloyu genişletmeyi** önerir (ADR-0032-DEG madde 6 "yazım yolu yalnızca `BOT_TABLE` adlarına izin verir" ad listesinin kodda kalmasıyla en güvenli); gerekçe §3.2'de. Proje sahibi kabul etmeden HAZIR olmaz; kabul edilirse ADR-0002 Ek F8-03 yazılır ve `docs/15:291` cümlesi düzeltilir (Claude'un işi).
3. **Ad kuralının onayı.** ADR-0002 Ek F8-02 madde 3 (`BotWP2_K` / `BotAccWP2K`) "otonom döngüde Claude kararı, gözden geçirilmeli"dir; `db/005` bunu bağlar. Onay yoksa HAZIR olmaz.
4. **`Upgrade` kademesi:** `db/002` bugün hangi `-v Upgrade` ile uygulandıysa (S1 = 7 beklenir; `docs/04` §3.3) aynısı verilecek. Kademe `[V]` olarak doğrulanmadan (satır içeriği basılmadan: yalnızca `strItem` slot 4 kimliğinin son hanesini karşılaştıran öz denetim sayacı) plan HAZIR olmaz.
5. **Numara:** `db/005` (proje sahibi kuralı: uygulanmış migration yeniden numaralandırılmaz; `db/README.md` "Numara ayırma notu"). `db/003` bot quest, `db/004` bot envanter betiğidir.
6. **DB durumu belirsiz `[A]`:** `db/003`/`db/004`'ün gerçek DB'de uygulanıp uygulanmadığı kayıtlara göre "geri alındı" (`docs/reports/gece-2026-10-03.md` madde 5) ama güncel durum doğrulanmadı. Yeni satırların hangi quest/stok durumuyla doğacağı bu yüzden **betik içinde açıkça** belirlenir (§3.1.5), ikisinin DB'deki anlık durumuna bağlı kalmaz.

**F8-04 ile ilişki:** F8-03 yalnızca **2. W-P ve 2. M-F** (indeks 2) ekler: 12 + 4 = 16. F8-04 yalnızca **3. W-P ve 3. M-F** (indeks 3) ekler: 16 + 4 = 20. İki plan birbirinin satırını okumaz/değiştirmez; hangisi önce uygulanırsa uygulansın `tools/bot-composition-check.py` sayıya göre değerlendirir (F8-02 `build_result`: sayı tabanlı). Her ikisi de `BotManager.cpp` tablosuna kendi bloğunu ekler (birleştirme çakışması en fazla tek satır).

## 1. Amaç

`docs/15` §6a: 8v8 için aynı anda **16 karakter** (ulus başına 8: C8-A = 2 W-P, 1 W-G, P-HD, P-HB, 2 M-F, 1 M-I) gerekir; DB'de bugün 12 vardır (ulus başına 6). Bu plan eksik **4 karakteri** (her ulusta 1 W-P + 1 M-F daha) `db/002` ile aynı stat/skill/ekipman/stok düzeniyle ekleyen geri alınabilir bir migration (`db/005`) ve bu 4 adın sunucuda doğabilmesi için `BotManager.cpp` `BOT_TABLE` genişletmesi yapar. Kabul: `tools/bot-composition-check.py` 16 karakterde `min16_missing = 0` bildirir.

## 2. Bağlam (okunması zorunlu)

- `docs/15` §6a (satır 283-291), `docs/09` §2.4 (C8-A satır 48), `docs/04` §3.3 (satır 57-71): ihtiyaç ve "20 karakter = 16 + 4" dökümü.
- ADR-0002 Ek F8-02 madde 2-3: ulus başına ihtiyaç ve ad kuralı (`Bot<PROFİL><n>_<K|E>`, hesap `BotAcc<PROFİL><n><K|E>`, yalnızca harf/rakam, ≤ `MAX_ID_SIZE` 20 = `shared/globals.h:12`).
- `db/002_bot_characters.sql` (kalıp ve veri kaynağı; satır numaraları `f4daa27`):
  - `:24-26` `Upgrade` doğrulaması (0/7/8; `-v` zorunlu); `:28-63` sahiplik ön denetimleri; `:65-101` `ITEM`/`LEVEL_UP` ön denetimi.
  - `:124-137` `@bots` satırları: `(profile, charName, account, nation, race, class, strong, sta, dex, intel, cha, hp, mp, skillHex)`. W-P K: `:126`, M-F K: `:130`, W-P E: `:132`, M-F E: `:136`. Yeni satırların sayısal sütunları ilgili profil satırıyla **aynen** aynıdır.
  - `:154-157` önceki koşuyu silme (yalnızca betiğin 12 satırı), `:205-216` çanta (14..20), `:218-248` `strItem` 73 yuva × 8 bayt, `:282-328` değişmez denetimleri (Level 80, Zone 71, stat toplamı 577, skill toplamı 142, ekipman kimlikleri, `ACCOUNT_CHAR` bağı).
- `db/002_bot_characters_rollback.sql:16-62`: sahiplik denetimli silme kalıbı.
- `db/003_bot_quests.sql:96-101` (sınıfa göre quest kimlikleri: savaşçı 51/510/511, mage 53/515/516/517, priest 54/518..523; kayıt düzeni 3 bayt, **little-endian** `uint16` + durum 2: `DBAgent.cpp:402`) ve `:39-48` yedek tablo kalıbı. **Bu plan `db/003`'ü değiştirmez**; yeni 4 satır için aynı kayıtlar `db/005` içinde yazılır.
- `db/004_bot_inventory.sql:205` (`STUFF` + `CONVERT(binary(584), ...)` düzeltmesi) ve F4-40 §3.1: çanta yuva 14..21 şablonu (varsayılan stok `HpPots=100, MpPots=0, LifeStones=30, ClassStones=50` = `db/002` ile bayt bayt eşdeğer).
- Kod: `GameServer/Bot/BotManager.cpp:58-66` (`BotAccountEntry` ve 12 girişli `BOT_TABLE`), `:68-76` `FindBotEntry`, `:683-685` `IsKnownBotName`, `:2877` `SPAWN_ON_START` yolundaki ikinci tablo taraması (iki yer de `sizeof(BOT_TABLE)/sizeof(BOT_TABLE[0])` kullanır, sabit 12 yok `[D]`); `GameServer/Bot/ScenarioRunner.cpp:16` `SCENARIO_MAX_BOTS = 16`; `BotManager.cpp:168` `MAX_BOTS` varsayılan 16 (`MAX_POOL` 100: `BotManager.h:18`). Üçü de 16 karaktere yeter.
- Araç: `tools/bot-composition-check.py` (`--sql PATH` tekrarlanabilir; satır deseni yalnızca ilk altı sütuna bağlı; yinelenen ad/hesap dosyalar arası **ERROR**: bu yüzden `db/005` içinde `db/002`'nin 12 adı **bulunmamalı**).

## 3. Kapsam

### 3.1 `db/005_bot_characters_16.sql` (yeni; ASCII, LF, İngilizce yorumlar)

Yapı ve ton `db/002`'yi izler; SQL'i Uygulayıcı yazar (bu plan iskelettir):

1. **Başlık yorumu:** amaç, kullanım satırı, "yalnızca 4 satır", sunucu kapalı, satır içeriği basılmaz, tekrar çalıştırılabilir.
2. **Zorunlu `-v Upgrade`** (0/7/8), `db/002:24-26` ile aynı `RAISERROR`.
3. **Ön denetimler:** (a) sahiplik: 4 ad/hesap ikilisi için `db/002:28-63` mantığı (bot olmayan hesaba ait ad varsa dur); (b) **ikiz kontrolü:** 4 yeni satırın ikizi (`BotWP_K`, `BotMF_K`, `BotWP_E`, `BotMF_E`) `USERDATA`'da yoksa `RAISERROR` ve dur (ikiz yoksa referans yoktur); (c) `ITEM`/`LEVEL_UP` denetimleri `db/002:65-101` ile aynı (yalnızca ilgili profillerin kimlikleri).
4. **`@bots` (yalnızca 4 satır):**

| profile | charName | account | nation | race | class | diğer sütunlar |
|---|---|---|---|---|---|---|
| `WP` | `BotWP2_K` | `BotAccWP2K` | 1 | 1 | 106 | `db/002:126` satırıyla aynen (stat, `hp`, `mp`, `skillHex`) |
| `MF` | `BotMF2_K` | `BotAccMF2K` | 1 | 3 | 110 | `db/002:130` satırıyla aynen |
| `WP` | `BotWP2_E` | `BotAccWP2E` | 2 | 11 | 206 | `db/002:132` satırıyla aynen |
| `MF` | `BotMF2_E` | `BotAccMF2E` | 2 | 12 | 210 | `db/002:136` satırıyla aynen |

   Satır biçimi `db/002:126` ile **birebir** aynı (`    ('WP', 'BotWP2_K',  'BotAccWP2K',  1,  1, 106, ...`): araç ilk altı sütunu bu desenle ayrıştırır (`tools/bot-composition-check.py` `ROW_RE`). Başka `INSERT ... VALUES ('XX','yy',...)` satırı (6+ alanlı, `(` ile başlayan) betiğe konmaz.
5. **Satır üretimi:** `db/002:139-277` döngüsü (zırh/aksesuar/silah/çanta, `strItem` 73 × 8 bayt, `ACCOUNT_CHAR`, `WAREHOUSE`, `USERDATA`) aynen; yalnızca tablo 4 satırlıktır. Ek olarak, yeni satırların **12 ikizle aynı oyun durumunda** doğması için:
   - **Quest:** sınıfa göre `db/003:96-101` kimlikleri durum 2, little-endian 3 bayt kayıtlar, `sQuestCount` karşılığı; kayıt sayısı ≤ 200. (Satırlar yeni olduğundan yedek tablo gerekmez.)
   - **Çanta:** `db/002`'nin varsayılan düzeni (14..20); yuva 21 boş. (`bot-refill.sh` 12 adla sınırlıdır, §3.4.)
6. **Öz denetim (aynı işlemde, ihlalde `ROLLBACK` + `RAISERROR`):** `db/002:282-328` değişmezleri 4 satır için; ek olarak (i) her yeni satırın `strSkill` ve `strItem` yuva 0..13 baytları ikizinin baytlarıyla **eşit** (yalnızca karşılaştırma sonucu sayılır, bayt basılmaz; `Upgrade` uyuşmazlığını yakalar); (ii) betiğin dokunmadığı **12 özgün bot satırının** `CHECKSUM_AGG(BINARY_CHECKSUM(...))` değeri işlemden önce/sonra eşit; (iii) bot adı listesi dışındaki `USERDATA` satır **sayısı** işlemden önce/sonra eşit (yalnızca `COUNT`, satır içeriği okunmaz). Tek çıktı satırı:
   `BOTCHARS16: rows=4 ok=4 fail=0 twin_mismatch=0 orig_unchanged=1 other_rows_unchanged=1 upgrade=<n>`
   Beklenen: `rows=4 ok=4 fail=0`. Betik tekrar çalıştırılınca önce 4 satırı siler, yeniden ekler (aynı çıktı).

### 3.2 `BOT_TABLE` genişletmesi (`GameServer/Bot/BotManager.cpp`)

**Önerilen yöntem `[A]`:** sabit tabloya 4 giriş eklemek. Gerekçe: (a) ADR-0032-DEG madde 6 ve `BotManager.cpp:58` yorumu ("ini listesi yeni hesap getirmez") ad listesinin **kodda** olmasına dayanır; (b) DB'den okumak `ACCOUNT_CHAR`/`USERDATA` (yasak tablolar) okumayı gerektirir; (c) ini kaynaklı tablo, ini'ye yazan herkesin sunucuya "bot hesabı" tanıtması demektir. Tablo dışı seçenekler (ini `[BOT_CHARS]` ya da ad kuralı + üst sınır) proje sahibine tek tek sorulur; seçilirse bu bölüm değişir. Seçilen yöntem ADR-0002 Ek F8-03'e yazılır.

- 4 girişi ayrı, yorumlu bir blok olarak ekle (yorum: `F8-03: C8-A set (db/005)`): `{ "BotWP2_K", "BotAccWP2K" }, { "BotMF2_K", "BotAccMF2K" }, { "BotWP2_E", "BotAccWP2E" }, { "BotMF2_E", "BotAccMF2E" }`.
- `:58` yorumunu "db/002 and db/005 bot characters" olacak şekilde güncelle. Başka kod değişmez: iki tarama (`:68-76`, `:2877`) tablo boyutundan çalışır.
- Bot sistemi kapalıyken (`[BOT] ENABLED=0`) davranış değişmez: tablo yalnızca `ENABLED=1` yollarında okunur.

### 3.3 `db/005_bot_characters_16_rollback.sql` (yeni)

`db/002_bot_characters_rollback.sql:16-62` kalıbı: yalnızca 4 ad/hesap; sahiplik denetimi (ad bot hesabına ait değilse hiçbir şey silme, `RAISERROR`); `WAREHOUSE`, `USERDATA`, `ACCOUNT_CHAR` silme; çıktı `BOTCHARS16_ROLLBACK: removed=<N>` (ilk çağrıda 4, ikincide 0). 12 özgün satıra dokunmaz.

### 3.4 `db/README.md`

`005` bölümü (ne yapar, `-v Upgrade`, sunucular kapalı, 4 ad listesi, geri alma, çıktı satırları, "satır içeriği basılmaz", `db/002` ile birlikte `tools/bot-composition-check.py` komutu). **Mevcut satırlar değişmez; yalnızca ekleme.**

**Kapsam dışı (yapılmayacak)**

- 3. W-P / 3. M-F (20 karakter): F8-04 (`db/006`).
- `db/002`, `db/003`, `db/004`, `tools/bot-refill.sh` dosyalarını değiştirmek. **Bilinen açık `[D]`:** `db/003` (quest) ve `db/004` (stok) ad listeleri 12 adlıdır ve `bot-refill.sh` yeni 4 botu doldurmaz. Yeni botların çantasını senaryo başında dolduran şey F8-05 `ScenarioReset`'tir; bu yüzden ayrı bir `bot-refill` genişletmesi planlanmaz, ihtiyaç doğarsa ayrı küçük plan (proje sahibi kararı).
- `ScenarioRunner` limiti, `MAX_BOTS`, yeni sunucu davranışı, ırk (`Race`) denetimi (ADR-0002 Ek F8-02 madde 5).
- `tools/bot-composition-check.py` değişikliği (aracın kendi `--selftest`'i bu plan için yeterlidir).
- Kişisel veri tabloları: `TB_USER`, `CURRENTUSER`, `KNIGHTS*` vb. okunmaz/yazılmaz; yalnızca `USERDATA`/`ACCOUNT_CHAR`/`WAREHOUSE` bot satırları.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `db/005_bot_characters_16.sql` | yeni | ASCII, LF (`.gitattributes` `*.sql eol=lf`) |
| `db/005_bot_characters_16_rollback.sql` | yeni | ASCII, LF |
| `db/README.md` | değiştir | yalnızca `005` bölümü eklenir |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `BOT_TABLE` (4 giriş) ve `:58` yorumu |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. Yeni `.cpp`/`.h` yok: `proj-GameServer.vcxproj`/`.filters` değişmez. `docs/`, `tools/`, `db/001..004`, `BotCore/`, `Tests/` değişmez.

## 5. Uygulama adımları

1. `git switch -c bot/F8-03 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucular kapalı (`./tools/run-servers.sh status`; `[UP]` varsa `stop`).
2. Şema/ikiz ön kontrolü (yalnızca sayaç): ikiz satırların varlığı ve `DATALENGTH(strItem) = 584`; yoksa **dur** ve rapora yaz.
3. `db/005` ve rollback (§3.1, §3.3); `db/README.md` (§3.4).
4. `tools/bot-composition-check.py` kabul komutları (§7), **DB'ye yazmadan** (araç yalnızca metin okur).
5. SQL denemeleri (sunucular kapalı; yalnızca çıktı satırları rapora): `db/005` uygula → tekrar uygula (aynı çıktı) → geri al (`removed=4`) → tekrar geri al (`removed=0`) → uygula. İş bitince DB'yi **proje sahibinin istediği duruma** bırak (varsayılan: ilk durum = geri al).
6. `BOT_TABLE` (§3.2); `./tools/build.sh Release`; `./tools/run-tests.sh` (kod değişimi yalnızca tablo olduğundan test sayısı değişmez).
7. Uygulayıcı Raporu (Tur 1); `Durum` → `UYGULANDI`. Sunucu açma ve oyun içi doğrulama Claude'undur (K8).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; `./tools/run-tests.sh` `0 failed` ve test sayısı işe başlamadan önceki sayıyla aynı (sayıyı rapora yaz)
- [ ] K2: `git diff --stat gece/2026-10-02...bot/F8-03` yalnızca §4'teki dosyalar (+ plan); `git diff --check` boş; `BotManager.cpp` farkı yalnızca `BOT_TABLE` bloğu ve `:58` yorumu (diff satır sayısı rapora)
- [ ] K3: `python3 tools/bot-composition-check.py --sql db/002_bot_characters.sql --sql db/005_bot_characters_16.sql --strict --target min16; echo rc=$?` rc=0 ve `--json` çıktısında `summary.have == 16`, `summary.min16_missing == 0`, `summary.full20_missing == 4`, `errors == []`; `--strict --target full20` rc=1 (4 eksik, F8-04'e bırakılmış)
- [ ] K4: `db/005` ilk çalıştırma `BOTCHARS16: rows=4 ok=4 fail=0 twin_mismatch=0 orig_unchanged=1 other_rows_unchanged=1 upgrade=<n>`; ikinci çalıştırma aynı satır; `grep -c "LIKE 'Bot" db/005_bot_characters_16.sql` = 0 ve 4 ad açık listede; betik satır içeriğini basmaz (`PRINT`/`SELECT` yalnızca sayaç ve ad/sınıf özet satırı)
- [ ] K5: rollback `BOTCHARS16_ROLLBACK: removed=4`; ikinci `removed=0`; 12 özgün satırın `CHECKSUM_AGG(BINARY_CHECKSUM(...))` değeri uygula/geri al öncesi-sonrası eşit (Claude doğrulaması: betiğin kendi sayacı `orig_unchanged=1`)
- [ ] K6: `BOT_TABLE` adları ile SQL adları eşit: `diff <(grep -aoE '"Bot[A-Z]+[0-9]*_[KE]"' GameServer/Bot/BotManager.cpp | tr -d '"' | sort -u) <(grep -ahoE "^\s*\('[A-Z]+',\s*'Bot[A-Za-z0-9_]+'" db/002_bot_characters.sql db/005_bot_characters_16.sql | grep -oE "Bot[A-Za-z0-9_]+" | sort -u)` boş (16 ad)
- [ ] K7: kodlama: yeni dosyalar yalnızca ASCII ve LF (`file` çıktısı); `BotManager.cpp` kodlaması ve satır sonu (CRLF/BOM) değişmedi; `db/README.md` yalnızca ekleme
- [ ] K8 (Claude, çalışma zamanı; **oyun içi doğrulama ayrı kayıtlıdır, birim/derleme yerine geçmez**): `db/005` uygulanmış, `[BOT] ENABLED=1`: (a) `/bot spawn BotWP2_K,BotMF2_K,BotWP2_E,BotMF2_E` dördü `in_game`; `/bot snap` her biri için sınıf 106/110/206/210, seviye 80, konum ≈ (1274, 890), `hp=max`; (b) ikiz karşılaştırması: `BotWP2_K` ile `BotWP_K` `maxHp`/`maxMp` ve `stock hp_pot`/`mp_pot` aynı (aynı ekipman/stok kanıtı); (c) 16 botun hepsi aynı anda `in_game` (`/bot list`), `ScenarioRunner` 16 isimli senaryoyu `loaded` kabul eder (`bots: expected 1..16 names` hatası yok); (d) sınıf skill'i bir kez atılır (`cast`), `quest_locked` **yok** (quest kayıtları yerinde); (e) iş bitince `despawn all`, sunucular kapatılır, DB istenen duruma
- [ ] K9: Uygulayıcı Raporu dürüst: SQL çıktı satırları gerçek; yapılmayan/doğrulanamayan açıkça yazılmış; satır içeriği/hesap bilgisi rapora girmemiş

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status
python3 tools/bot-composition-check.py --sql db/002_bot_characters.sql --sql db/005_bot_characters_16.sql --strict --target min16; echo rc=$?
python3 tools/bot-composition-check.py --sql db/002_bot_characters.sql --sql db/005_bot_characters_16.sql --json | python3 -c "import json,sys; d=json.load(sys.stdin); s=d['summary']; print(s['have'], s['min16_missing'], s['full20_missing'], d['errors'])"
python3 tools/bot-composition-check.py --sql db/002_bot_characters.sql --sql db/005_bot_characters_16.sql --strict; echo rc=$?   # beklenen rc=1 (full20)
SQLCMD="/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_kn_online -b -v Upgrade=7 -i db/005_bot_characters_16.sql
"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_kn_online -b -i db/005_bot_characters_16_rollback.sql
./tools/build.sh Release && ./tools/run-tests.sh 2>&1 | tail -3
git diff --stat gece/2026-10-02...bot/F8-03
file db/005_bot_characters_16.sql db/005_bot_characters_16_rollback.sql
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları; `.sql` LF + ASCII; `BotManager.cpp` mevcut kodlamasında kalır.
- **Kişisel veri:** `USERDATA` yalnızca 4 yeni bot satırı ve (yalnızca ikiz karşılaştırması/sağlama toplamı için, içerik basılmadan) 12 özgün bot satırı; `testing`, `testmage` gibi başka karakterlere dokunulmaz. Satır içeriği ekrana basılmaz. Elle tek seferlik `UPDATE` yapma.
- **Sunucular kapalıyken** çalıştır: oyundaki karakter çıkışta satırı ezer (ADR-0032-DEG).
- Bu sqlcmd sürümü tanımsız `$(Değişken)`'de batch'i durdurur: `-v Upgrade` her çağrıda verilir; Windows `SQLCMD.EXE` göreli `-i` yolunu depo kökünden açar (açamazsa `wslpath -w`, rapora not).
- `db/002` dosyası ve satır biçimi sabit kabul edilir; `db/002`'yi yeniden çalıştırmak 12 özgün satırı sıfırlar ve `db/005` satırlarına dokunmaz (kapsamı 12 ad). Yeni 4 satırın quest/stok durumu `db/002` yeniden çalıştırmasından etkilenmez.
- Risk: `Upgrade` uyuşmazlığı (öz denetim (i) yakalar); ikiz satır başka bir betikle (`db/003`/`db/004`) değiştirilmişse (i) yalnızca yuva 0..13 ve `strSkill`'i karşılaştırır (quest/stok yuvalarını değil).
- `[BOT] ENABLED=0` varsayılan davranışı değişmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F8-03` — `<kısa-sha> [F8-03] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F8-03` @ `<sha>`
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ / ✘ | dosya:satır / komut çıktısı |

- Bulgular (önem sırasıyla):
  1. …
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
…
```
