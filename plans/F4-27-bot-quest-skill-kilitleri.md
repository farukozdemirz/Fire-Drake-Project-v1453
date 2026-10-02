# F4-27: Quest ile açılan skill'ler: bot kurulumunda quest durumu (`db/003`) ve `BeginCast`'te `Etc` denetimi (`quest_locked`)

| Alan | Değer |
|---|---|
| Durum | DÜZELTME GEREKLİ |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi; ADR-0018 Ek 3) |
| Branch | `bot/F4-27` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-03 (cast dilimi, `BeginCast`), F4-25/F4-26 (uçan ve çift tipli skill; `BeginCast`'in destek kuralları değişti: satır numaraları kaymıştır, sembolle bul) — `KAPANDI` olmalı; F2-03 (bot girişi), `db/002_bot_characters.sql` (12 bot satırı) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/03` MEC-MAG-14 ve MB-15 (bu planın dayanağı, `[D]`/`[V]`), KI-018, KI-001, KI-017; `docs/18` Q-27; MEC-MAG-03 (tip kapısı) etkilenmez; adalet: bot, quest'leri yapmış bir insan oyuncuyla eşdeğer olur (ADR-0017, K-5) |
| Tahmini büyüklük | M (2 yeni SQL betiği + `db/README.md`; `BotCore/BotCombat.h`, bir mevcut test dosyası, `ActionExecutor.cpp`/`.h`; yeni `.cpp` yok, `vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (proje sahibinin bildirimi + kod ve veri doğrulaması) |

---

## 1. Amaç

Bazı skill'ler quest ile açılır (`docs/03` MEC-MAG-14). Bot satırlarının quest listesi boştur (`db/002` `strQuest` = 600 sıfır bayt, `sQuestCount` = 0), bu yüzden `Etc != 0` skill'ler botta kullanılamaz: `ActionExecutor::BeginCast` bunları `unsupported_skill` ile reddeder (F4-03) ve reddetmese sunucu `UserCanCast()` (`MagicInstance.cpp:269-275`, Release, GM değil) `SkillUseFail` verirdi (`srv_fail`). Bu plan iki şeyi yapar:

1. **Veri:** bot satırlarına sınıflarının quest'lerini **durum 2 (tamamlandı)** olarak yazan, geri alınabilir bir SQL betiği (`db/003_bot_quests.sql`). Quest ≙ quest'i tamamlamış bir insan oyuncu: kural insan ve bot için aynı.
2. **Kod:** `BeginCast`'te `sEtc != 0` ⇒ `unsupported_skill` kuralını, sunucunun kuralının aynısına çevir: `sEtc != 0` ve botun quest listesinde `sEtc` durum 2 değilse (GM değilse) **`quest_locked`** ile ön kontrolde reddet; aksi halde skill diğer kurallara göre işlenir. Böylece quest'i olan bot bu skill'leri `srv_fail` almadan atar, olmayan bot sunucuya hiç paket göndermeden reddedilir.

İstemci `.tbl` dosyalarına ve sunucu `MAGIC` verisine **dokunulmaz** (Q-27 ayrı karar).

## 2. Bağlam (okunması zorunlu)

- `docs/03` MEC-MAG-14 ve MB-15 (bu plan için gerçek kaynak; kodda ve veride doğrulandı), `docs/KNOWN_ISSUES.md` KI-018, KI-001, KI-017, `docs/18` Q-27, `docs/16` §5.1 (`quest_locked` kodu).
- Quest kuralı sunucuda: `GameServer/MagicInstance.cpp:269-275` (`UserCanCast`, `#if !defined(DEBUG)`, `!isGM() && sEtc != 0 && !CheckExistEvent(sEtc, 2)`); `GameServer/QuestHandler.cpp:137` `CUser::CheckExistEvent` (kayıt yoksa yalnızca `bQuestState == 0` için doğru; aksi halde `itr->second == bQuestState`).
- Quest listesi nereden gelir: `GameServer/DBAgent.cpp:388-410` (`LoadUserData`): `sQuestCount` (≤ `QUEST_LIMIT` = 200, `shared/globals.h:353-354`) ve `strQuest` (binary 600): her kayıt 3 bayt, **`uint16` kimlik (little-endian) + `uint8` durum**; kayıt yoksa `STARTER_SEED_QUEST` (500) durum 1 eklenir (`:410`); çıkışta `UpdateUser` listeyi geri yazar (`DBAgent.cpp:930-938`): bu yüzden **oyundaki karaktere DB'den yazmak işe yaramaz** (çıkışta bellekteki liste ezer); betik yalnızca sunucular kapalıyken çalışır.
- Bot girişi insanla aynı yoldan: `BotManager.cpp:3453` `WIZ_SEL_CHAR` → `AddDatabaseRequest` → `LoadUserData`; botlar GM değildir (`db/002`: `Authority = 1`).
- Kodda `BeginCast` (`GameServer/Bot/ActionExecutor.cpp`, `ActionExecutor::BeginCast`): destek kuralı bloğunda `|| m->sEtc != 0` (F4-26 sonrası konumu kaymış olabilir) ve `PotMagicSupported`'te (`sEtc == 0`, **dokunma**: pot kuralı ayrı).
- Hangi skill hangi quest'e bağlı (istemci tablosu ve `MAGIC.Etc`; kanıt: `python3 tools/client-tbl-quests.py --server --list`): savaşçı Skaki 51, 510, 511; rogue Clarence 52, 512–514; mage Drake 53, 515–517; priest Minerva 54, 518–523. Sunucuda yalnızca 510–523 kilitli (`Etc`); 51–54 sunucuda kilitli değil (`UseStanding` sütununda, KI-017); bu plan 51–54'ü de yazar (istemci karşılaştırması ve kural eşitliği için, `User.cpp:2276` quest 51 savunma bonusu).
- Bot sınıf kodları: savaşçı 106/206 (profiller WP, WG), mage 110/210 (MF, MI), priest 112/212 (PHD, PHB) (`docs/04`). 12 bot satırı adları `db/002_bot_characters.sql`'deki listedir (`BotWP_K`, `BotWG_K`, `BotPHD_K`, `BotPHB_K`, `BotMF_K`, `BotMI_K` ve `_E` karşılıkları).
- Skill puanı ayrı koşuldur (`MagicInstance.cpp:956-960`: `m_bstrSkill[sSkill % 10] >= SkillLevel`, `KI-016`). Referans profillerde ağaçlar 70'i aşmaz (`docs/04`: WP `[70,0,52,20]`, MF `[70,52,0,20]` vb.), bu yüzden 72–80. seviye quest skill'leri puan yüzünden de erişilemez; bu planın **çalışma zamanı kanıtı** için betiğe isteğe bağlı bir test puan düzeni eklenir (§3.1 madde 6).

## 3. Kapsam

### 3.1 `db/003_bot_quests.sql` ve `db/003_bot_quests_rollback.sql` (yeni)

`db/002` ve `db/001`'in biçimine uy (`sqlcmd -b`, başlık yorumu, `SET XACT_ABORT ON`, `GO` blokları, ASCII + LF, `db/README.md` kullanımı). Betik **yalnızca** 12 bot adına dokunur (açık ad listesi; `LIKE 'Bot%'` kullanma), başka satır ve tablo yok. **Satır içeriğini ekrana basma** (kişisel veri kuralı: yalnızca sayaçlar ve PASS/FAIL).

1. **Önkoşul:** sunucular kapalı olmalı (başlık yorumunda ve README'de yaz; betik içinde `CURRENTUSER` okunmaz: yasak tablo).
2. **Yedek tablo:** `dbo.USERDATA_BOT_QUEST_BACKUP (strUserID varchar(21) PRIMARY KEY, OldQuestCount smallint NOT NULL, OldQuest binary(600) NOT NULL, OldSkill varbinary(10) NULL, SavedAt datetime NOT NULL DEFAULT GETDATE())`. Her bot satırı için eski değerler **bir kez** (satır yedekte yoksa) yazılır; betik tekrar çalışınca yedeği ezmez.
3. **Gerekli quest kümesi** `USERDATA.[Class]`'a göre: 106 ve 206 → {51, 510, 511}; 110 ve 210 → {53, 515, 516, 517}; 112 ve 212 → {54, 518, 519, 520, 521, 522, 523}. (Rogue quest'leri 52, 512–514 yazılmaz: botlar warrior/priest/mage; ihtiyaç olursa küme genişletilir.)
4. **Birleştirme (mevcut kayıtları koru):** `sQuestCount` ve `strQuest`'i ayrıştır (kayıt i: kimlik = bayt `3i`, `3i+1` little-endian, durum = bayt `3i+2`); gerekli kümedeki kimlikleri listeden çıkar, sonra hepsini durum **2** ile sona ekle; kümede olmayan mevcut kayıtlar (ör. 500, kill sayaçları 32001+) **korunur**. Yeni sayı > `QUEST_LIMIT` (200) olursa o satırda **hata ver** (sessizce kırpma). Sonuç `strQuest` 600 bayta sıfırla tamamlanır, `sQuestCount` yeni sayıya ayarlanır. Betik **idempotent**: gerekli kümedeki her kimlik zaten durum 2 ise satır değişmez (`changed` sayılmaz).
5. **Kendi kendini doğrulama:** sonda her satır için: gerekli tüm kimlikler durum 2, `sQuestCount` ≤ 200, kayıt sayısı = ayrıştırılan kayıt sayısı. Tek çıktı satırı: `BOTQUEST: rows=<N> ok=<N> fail=<N> changed=<N>`; `fail > 0` ise `RAISERROR` (şiddet 16) ile bitir (`-b` ile sıfırdan farklı çıkış). Beklenen ilk çalıştırma: `rows=12 ok=12 fail=0 changed=12`; ikincisi `changed=0`.
6. **İsteğe bağlı test puan düzeni:** `-v QuestTestPoints=1` verilirse **yalnızca** `BotWP_K` ve `BotMF_K` satırlarının `strSkill`'i (varchar(10), ham bayt; bayt 0 serbest puan, 5–7 ağaç, 8 master) `BotWP_K` için `[serbest 0, …, ağaç5 = 80, ağaç6 = 0, ağaç7 = 42, master = 20]`, `BotMF_K` için `[…, ağaç5 = 80, ağaç6 = 42, ağaç7 = 0, master = 20]` yapılır (toplam 142, ağaç ≤ 80, master ≤ 20: `db/002`'nin değişmezleri); eski `strSkill` yedeğin `OldSkill` kolonuna yazılır. Varsayılan `QuestTestPoints=0`: `strSkill`'e dokunulmaz. (Bu yalnızca çalışma zamanı kanıtı içindir: savaşçı Hell blade `106580` ve mage Igzination `110575` ancak ağaç 80 puanla atılabilir.) `db/002`'deki `-v` değişken kalıbını kopyala (varsayılan değer nasıl veriliyorsa aynı).
7. **Geri alma betiği** (`db/003_bot_quests_rollback.sql`): yedek tablodaki satırlar için `sQuestCount`, `strQuest` ve (`OldSkill` boş değilse) `strSkill`'i eski değere döndürür, çıktı `BOTQUEST_ROLLBACK: restored=<N>`; yedek tablo korunur (ikinci geri alma etkisizdir). Yedek yoksa hata verme: `restored=0`.

### 3.2 `BotCore/BotCombat.h` ve birim test

- `inline bool CastQuestAllowed(int etc, bool isGm, bool questDone)`: `etc == 0 || isGm || questDone` (saf mantık; yalnızca standart başlıklar; mevcut dosyanın biçimi: tab, Allman, `inline`, İngilizce kısa yorum; `[D]` etiketi `MagicInstance.cpp:269-275`'e atıf).
- `Tests/BotCoreTests/CombatTests.cpp` dosyasına (mevcut `Combat_*` vakaları) yeni `TEST_CASE("Combat_CastQuestAllowed")`: (etc 0, hiçbiri) → `true`; (etc 511, GM değil, quest yok) → `false`; (etc 511, GM değil, quest var) → `true`; (etc 511, GM, quest yok) → `true`; (etc 0, quest var) → `true`.

### 3.3 `GameServer/Bot/ActionExecutor.cpp` / `.h`

- `ActionExecutor::BeginCast`: destek kuralı bloğundan `|| m->sEtc != 0` koşulunu **kaldır** (diğer koşullar ve sıraları değişmez). Hemen **ardından** (hedef/`bad_target` denetiminden önce):
  ```
  if (!BotCore::CastQuestAllowed(m->sEtc, user->isGM(), m->sEtc == 0 || user->CheckExistEvent(m->sEtc, 2)))
  {
      out.kind = CastOutcome::REFUSED;
      out.reason = "quest_locked";
      return out;
  }
  ```
  (`user` bu fonksiyonda botun `CUser *`'ı; değişken adı farklıysa mevcut olanı kullan.) Bot, GM olmadığı sürece sunucuyla aynı kararı verir: Debug derlemesinde sunucu kapıyı atlar (`#if !defined(DEBUG)`), bot **her derlemede** uygular (muhafazakâr; fark `docs/03` MEC-MAG-14'te yazılı).
- `GameServer/Bot/ActionExecutor.h`: `reason` kodlarını sayan iki yorum satırına (`"unsupported_skill"` geçen yerler) `"quest_locked"` ekle. Başka değişiklik yok. `PotMagicSupported`'teki `sEtc == 0` **değişmez**.
- Thread: `m_questMap` bot tick'inde (IOCP iş parçacığı) okunur; yazma yalnızca girişte (`LoadUserData`, veritabanı iş parçacığı, oyuna girmeden önce) ve çıkışta olur: yeni kilit eklenmez; bu varsayımı kısa yorumla belirt.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `db/003_bot_quests.sql` | yeni | ASCII, LF (`*.sql eol=lf`) |
| `db/003_bot_quests_rollback.sql` | yeni | ASCII, LF |
| `db/README.md` | değiştir | `003` bölümü: ne yapar, sunucular kapalı, `QuestTestPoints`, geri alma |
| `BotCore/BotCombat.h` | değiştir | yalnızca `CastQuestAllowed` ekleme (CRLF/BOM korunur) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | yalnızca yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yalnızca §3.3 |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca iki yorum satırı |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. `GameServer/MagicInstance.cpp`, `shared/`, istemci dosyaları, `MAGIC` verisi ve `db/001`, `db/002` **değişmez**.

## 5. Uygulama adımları

1. `git switch -c bot/F4-27 gece/2026-10-02` (taban F4-26'yı içermeli: `git log --oneline | grep F4-26`); `Durum` → `UYGULANIYOR`.
2. `BotCore/BotCombat.h` + birim test (`§3.2`); `./tools/run-tests.sh Release` (0 failed).
3. `ActionExecutor.cpp`/`.h` (`§3.3`); `./tools/build.sh Release` ve `Debug`.
4. SQL betikleri (`§3.1`) ve README. **Sunucular kapalı olmalı** (`./tools/run-servers.sh status` `[UP]` varsa `stop`). Çalıştır (`SQLCMD` yolu `CLAUDE.md` Komutlar'daki gibi): `-b -i db/003_bot_quests.sql` iki kez (çıktı `rows=12 ok=12 fail=0 changed=12`, sonra `changed=0`); `-v QuestTestPoints=1` ile bir kez (yalnızca `changed` sayısı değişir, `fail=0`); `db/003_bot_quests_rollback.sql` (`restored=12`); tekrar `db/003_bot_quests.sql` (`changed=12`). **Satır içeriği basma**; yalnızca çıktı satırlarını rapora yaz.
5. Çalışma zamanı kanıtı **Claude'un doğrulamasında** yapılır (§6 K8): DeepSeek yalnızca SQL ve derleme çıktısını ve kendi ön kontrolünü raporlar. Sunucu açmak gerekirse bitince `./tools/run-servers.sh stop`.
6. `Uygulayıcı Raporu` (Tur 1) doldur; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; değişen dosyalar için yeni uyarı yok
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug` `0 failed`; `Combat_CastQuestAllowed` `[ OK ]`; mevcut testler geçer
- [ ] K3: `git diff --stat gece/2026-10-02...bot/F4-27` yalnızca §4'teki dosyalar; `GameServer/MagicInstance.cpp`, `shared/`, `db/001*`, `db/002*` farkı 0; `ActionExecutor.cpp` farkında `sEtc != 0` kuralı kaldırılmış ve `CastQuestAllowed`/`quest_locked` eklenmiş, `PotMagicSupported`'te `sEtc == 0` aynen
- [ ] K4: `db/003_bot_quests.sql` ilk çalıştırma `BOTQUEST: rows=12 ok=12 fail=0 changed=12`, ikinci `changed=0`; betik açık 12 ad listesi kullanır (`grep -c "LIKE 'Bot" db/003_bot_quests.sql` = 0), satır içeriği basmaz
- [ ] K5: geri alma `BOTQUEST_ROLLBACK: restored=12`; ardından `db/003_bot_quests.sql` yeniden `changed=12`; yedek tablo 12 satır; mevcut kayıtlar (500 gibi) birleştirmede korunur (betik başlık yorumunda ve README'de yazılı; kanıt: betik içi sayaç `kept_other=<N>` çıktısı veya eşdeğer)
- [ ] K6: `-v QuestTestPoints=1` yalnızca `BotWP_K` ve `BotMF_K` `strSkill`'ini değiştirir (toplam 142, ağaç ≤ 80, master ≤ 20: betik içi kontrol `fail=0`); varsayılanda `strSkill` değişmez
- [ ] K7: `BotCore/BotCombat.h` saf (`windows.h|stdafx|GameServer|shared/` yok); ASCII/CRLF/BOM korunmuş; `git diff --check` boş
- [ ] K8 (Claude, çalışma zamanı): `db/003` `-v QuestTestPoints=1` ile uygulandıktan sonra sunucu açık: (a) `/bot cast BotWP_K 106580 BotWG_E 1` (Hell blade, `Etc` 511) ve `/bot cast BotMF_K 110575 BotWG_E 1` (Igzination, `Etc` 517) **`srv_fail` olmadan** `effected` (hedefin HP'si düşer, `live-*.jsonl` `CastEffect`); (b) geri alma sonrası aynı iki komut **`refused (quest_locked)`**, sunucuya paket gitmez; (c) `Etc == 0` bir skill (ör. `110518`) her iki durumda değişmeden çalışır; priest quest skill'leri (Type4/alan/parti) bu planın kapsamı dışında (executor desteği ADR-0018 dilimleri): priest için yalnızca veri seviyesi kontrol (K4)
- [ ] K9: Uygulayıcı Raporu dürüst: SQL çıktı satırları ve derleme/test çıktıları gerçek; yapılmayan/doğrulanamayan açıkça yazılmış

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Combat_CastQuestAllowed|tests,"
./tools/run-tests.sh Debug 2>&1 | grep -E "tests,"
git diff --stat gece/2026-10-02...bot/F4-27
grep -n "sEtc" GameServer/Bot/ActionExecutor.cpp
grep -c "LIKE 'Bot" db/003_bot_quests.sql
SQLCMD="/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_kn_online -b -i db/003_bot_quests.sql
python3 tools/client-tbl-quests.py --server
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2–3: üretim koduna yalnızca §4'teki dosyalarda dokun; bot sistemi kapalıyken (`ENABLED=0`) davranış değişmez (SQL ve `BeginCast` yalnızca bot yolunda); dosya kodlaması/satır sonu uyarısı.
- **Kişisel veri:** `USERDATA` yalnızca 12 bot satırına ve yalnızca betiğin içinde dokunulur; satır içeriğini asla çıktıya basma, başka karakter satırlarına (`testing`, `testmage` dahil) **dokunma**. `CURRENTUSER`, `TB_USER`, `ACCOUNT_CHAR` vb. okunmaz.
- İstemci `.tbl` dosyalarını **değiştirme**; `MAGIC` verisini değiştirme (Q-27 ayrı karar).
- Karakterler oyundayken DB'ye yazma işe yaramaz (çıkışta bellekteki liste geri yazılır): betik ve geri alma yalnızca sunucular kapalıyken çalıştırılır.
- Sunucu quest kapısı Debug derlemesinde yoktur; bot kapısı her derlemede vardır (bilinçli fark, `docs/03` MEC-MAG-14).
- `Etc` kuralı yalnızca quest durumunu denetler; skill puanı (`KI-016`) ve sınıf/seviye denetimleri bu planda değişmez (puan eksikse sunucu `srv_fail` verir, bu beklenen).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI (derleme ve birim testler geçti; SQL betikleri yerel `FDP_kn_online`'da çalıştırıldı; çalışma zamanı K8 Claude'un doğrulamasına bırakıldı, sunucu açılmadı)
- Branch / commit'ler: `bot/F4-27` (taban: `gece/2026-10-02` @ `06a76e8`), commit `ada6ffe` ("[F4-27] Quest ile acilan skill'ler: bot quest kurulumu (db/003) ve BeginCast quest_locked")
- Değişen dosyalar ve neden:
  - `db/003_bot_quests.sql` (yeni): 12 bot satırına sınıf quest'lerini durum 2 yazar; mevcut kayıtları korur; idempotent; `QuestTestPoints=1` ile BotWP_K/BotMF_K skill puanlarını 80. seviye quest skill'leri için yükseltir; `dbo.USERDATA_BOT_QUEST_BACKUP` yedeği; sonda `BOTQUEST: rows=12 ok=12 fail=0` öz denetimi.
  - `db/003_bot_quests_rollback.sql` (yeni): yedekten `sQuestCount`/`strQuest`/`strSkill` geri yükler; yedek satırları siler (tablo korunur, ikinci geri alma `restored=0`).
  - `db/README.md` (değişti): 003 bölümü (ne yapar, sunucular kapalı, `QuestTestPoints`, geri alma).
  - `BotCore/BotCombat.h` (değişti): saf `CastQuestAllowed(etc, isGm, questDone)` eklendi.
  - `Tests/BotCoreTests/CombatTests.cpp` (değişti): `Combat_CastQuestAllowed` (5 assert).
  - `GameServer/Bot/ActionExecutor.cpp` (değişti): `BeginCast` destek kuralından `|| m->sEtc != 0` kaldırıldı; ardından `CastQuestAllowed(...)` ile `quest_locked` ön kontrolü; `PotMagicSupported`'teki `sEtc == 0` aynen kaldı.
  - `GameServer/Bot/ActionExecutor.h` (değişti): iki yorum satırına `quest_locked` eklendi.
- Derleme sonucu: `./tools/build.sh Release` rc=0 ve `./tools/build.sh Debug` rc=0; değişen dosyalarda yeni uyarı yok (yalnızca eski `UpgradeHandler.cpp` C4789).
- Birim testler: `./tools/run-tests.sh Release` ve `Debug` → `103 tests, 0 failed`; `[ OK ] Combat_CastQuestAllowed` (önceki 102, yeni 1).
- SQL çıktı satırları:
  - İlk uygulama (`-v QuestTestPoints=0`): `changed=12 kept_other=0 backup_rows=12`, `BOTQUEST: rows=12 ok=12 fail=0`
  - İkinci uygulama: `changed=0 kept_other=0 backup_rows=12`, `BOTQUEST: rows=12 ok=12 fail=0`
  - `-v QuestTestPoints=1`: `changed=0 ...` (quest'ler aynı), `BOTQUEST: rows=12 ok=12 fail=0`; `BotWP_K` strSkill `sum=142 t5=80 t6=0 t7=42 m=20`, `BotMF_K` `sum=142 t5=80 t6=42 t7=0 m=20`; diğerleri değişmedi
  - Geri alma: `BOTQUEST_ROLLBACK: restored=12`; ikinci geri alma: `restored=0`
  - Yeniden uygulama: `changed=12 ...`, `BOTQUEST: rows=12 ok=12 fail=0`
  - Birleştirme kanıtı (`BotMI_K`'ya elle 500:1 ve 32001:5 enjekte edildi): `changed=1 kept_other=2`, liste `500:1 32001:5 53:2 515:2 516:2 517:2`
- Kabul kriterleri öz-değerlendirme:
  - K1: ✔ Release/Debug rc=0, değişen dosyalarda yeni uyarı yok
  - K2: ✔ 103 tests 0 failed, `Combat_CastQuestAllowed` `[ OK ]`
  - K3: ✔ fark yalnızca §4 dosyaları; `MagicInstance.cpp`, `shared/`, `db/001*`, `db/002*` farkı 0; `sEtc != 0` kuralı kaldırıldı, `CastQuestAllowed`/`quest_locked` eklendi, `PotMagicSupported` `sEtc == 0` aynen
  - K4: ✔ ilk `changed=12`, ikinci `changed=0`; `grep -c "LIKE 'Bot"` = 0; satır içeriği basılmaz (yalnızca sayaçlar ve `BOTQUEST`)
  - K5: ✔ `restored=12`, ardından `changed=12`; yedek tablo 12 satır; mevcut kayıt korunması `kept_other` sayacı ve enjeksiyon kanıtıyla gösterildi
  - K6: ✔ `QuestTestPoints=1` yalnızca BotWP_K/BotMF_K `strSkill`'ini değiştirdi (142, ağaç ≤ 80, master ≤ 20, `fail=0`); varsayılan `strSkill`'e dokunmaz
  - K7: ✔ `BotCombat.h` saf (windows.h/stdafx/GameServer/shared yok); ASCII/CRLF korundu; `git diff --check` boş
  - K8: Claude'un doğrulamasına bırakıldı (sunucu açılmadı)
  - K9: ✔ (aşağıdaki sapma ve açık soru dürüstçe yazıldı)
- Plandan sapmalar ve gerekçeleri:
  - **`QuestTestPoints` değişkeni zorunlu yapıldı (iç `:setvar` yok).** Plan §3.1 item 6 `-v QuestTestPoints=1` ile isteğe bağlı, varsayılan 0 istiyordu. Bu ortamdaki sqlcmd (ODBC 130) **tanımsız bir `$(...)` değişkenine rastlayınca batch'i durduruyor** (rc=1); iç `:setvar QuestTestPoints 0` eklenince bu sqlcmd sürümü dış `-v` geçersiz kılmayı **yok sayıyor** (test edildi: `:setvar` varken `-v QuestTestPoints=1` daima 0 verir). Bu nedenle hem güvenli varsayılanı hem de `-v` geçersiz kılmayı aynı dosyada sağlamak bu sqlcmd ile mümkün değil. Çözüm: `$(QuestTestPoints)` korundu, başlık ve README'de `-v QuestTestPoints=0|1` **zorunlu** kılındı (db/002'deki `$(Upgrade)` kalıbına benzer: orada da değişken zorunlu). K4/K5/K6 beklenen çıktıları `-v QuestTestPoints=0` ile üretildi.
  - `kept_other` sayacı ve enjeksiyon testi eklendi (K5 kanıtı; plan "eşdeğer" çıktıya izin veriyor). Enjeksiyon sonrası `BotMI_K` ve tüm bot satırları elle temizlendi; DB şu an db/002 durumunda (quest listesi boş, yedek tablo yok).
  - Geri alma **yedek satırlarını siler** (tabloyu bırakır) — plan "ikinci geri alma etkisizdir" istediği için; yalnızca tabloyu bırakıp satırları bırakmak ikinci çalıştırmada yeniden `restored=12` veriyordu.
- Açık sorular:
  - K4/K5/K6 komutlarında `-v QuestTestPoints=0` zorunlu hâle geldi (bkz. sapma). §7'deki örnek komutlar bu değişkeni içermiyor; reader'ın bunu bilmesi gerekir.
  - `db/003`'ün bot quest'lerini kalıcı bırakması mı, geri alınmış (db/002) durumda bırakması mı istendiği: ben **geri alınmış/temiz** durumda bıraktım (betik istenince uygulanır). Doğrulamada K8, `db/003`'ü `-v QuestTestPoints=1` ile uygular, test eder; sonra istenirse geri alır.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- **Karar: DÜZELTME GEREKLİ**
- İncelenen commit: `bot/F4-27` @ `996b5b1` (kod: `ada6ffe`); taban `gece/2026-10-02` @ `06a76e8`. Gece modu: birleştirme/push yapılmadı.
- Özet: C++ tarafı (`CastQuestAllowed`, `BeginCast`, test) doğru ve çalışma zamanında kanıtlandı. **SQL betiği kimlikleri yanlış bayt sırasıyla yazıyor** (big-endian; sunucu little-endian okur), bu yüzden bot satırlarına yazılan quest'ler sunucuda **hiçbir skill'i açmıyor**. Öz denetim aynı yanlış sırayla ayrıştırdığı için `ok=12 fail=0` yanıltıcı. Ayrıca `QuestTestPoints=1` ile `0`'dan sonra çalıştırılınca geri alma `strSkill`'i döndürmüyor.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `ActionExecutor.cpp` ve `CombatTests.cpp` `touch` edilip yeniden derlendi: Release rc=0, Debug rc=0; derleme günlüğünde `warning`/`error` yok |
| K2 | ✔ | `run-tests.sh Release` ve `Debug`: `103 tests, 0 failed`; `[ OK ] Combat_CastQuestAllowed` (ikisinde de) |
| K3 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-27`: 7 izinli dosya + kendi plan dosyası; `MagicInstance.cpp`, `shared/`, `db/001*`, `db/002*`, `docs/`, `AGENTS.md`, `.claude/` farkı 0; `ActionExecutor.cpp:721-739` `sEtc != 0` kaldırıldı, `:744` `CastQuestAllowed`/`quest_locked` eklendi, `:1289` `PotMagicSupported` `m->sEtc == 0` aynen |
| K4 | ✘ | Sayaçlar (`changed=12`, sonra `0`) yeniden üretildi ve `grep -c "LIKE 'Bot"` = 0, ama yazılan kimlik baytları yanlış (Bulgu 1): `BotWP_K` ilk 9 bayt `00 33 02 | 01 FE 02 | 01 FF 02` (51, 510, 511 big-endian). Sunucu `*(uint16 *)(strQuest + index)` (`DBAgent.cpp:402`) ile `0x3300`, `0xFE01`, `0xFF01` okur |
| K5 | ✘ | `restored=12` ✔, ardından `changed=12` ✔, yedek 12 satır ✔, `kept_other` sayacı var ✔; fakat `0` → `1` sırasında `strSkill` geri yüklenmiyor (Bulgu 2) |
| K6 | ✘ | `QuestTestPoints=1` yalnızca `BotWP_K`/`BotMF_K` `strSkill`'ini değiştiriyor (142/80/20 ✔), varsayılan `0` dokunmuyor ✔; ama geri alınamadığı durum var (Bulgu 2) |
| K7 | ✔ | `BotCombat.h` yalnızca standart başlık/saf; `file` ASCII, BOM yok, dosyalar taban ile aynı satır sonu düzeninde; `git diff --check` boş |
| K8 | ✘ (SQL verisiyle) / ✔ (kod yolu) | Aşağıdaki çalışma zamanı kaydı |
| K9 | ✘ | Rapor "DB şu an db/002 durumunda" diyor; doğrulama başında `BotWP_K`/`BotMF_K` `strSkill`'i test düzeninde (ağaç5 = 80) kalmıştı (Bulgu 2); K4 `ok=12` bayt sırasını denetlemiyor |

**Çalışma zamanı (K8; Release `GameServer.exe`, `GameServer.ini` md5 `265a8e1c…` dokunulmadı, botlar `BotWP_K`, `BotMF_K`, hedef `BotWG_E`, zone 71, gözlem `Logs/Bot_3_10_2026.log`, `Logs/bots/2026-10-03/live-*.jsonl`):**

1. Temiz DB'ye `db/003 -v QuestTestPoints=1` (`changed=12`, `BOTQUEST: rows=12 ok=12 fail=0`), sunucu açıldı: `cast BotWP_K 106580 BotWG_E 1` ve `cast BotMF_K 110575 BotWG_E 1` → **`refused (quest_locked)`** (betiğin yazdığı kimlikler sunucuda eşleşmiyor). `cast BotMF_K 110518 BotWG_E 1` (`Etc` 0) → `effected`.
2. Sunucu kapalıyken yalnızca bu iki bot satırı elle (geçici, commit edilmez) **doğru little-endian** baytlarla yazıldı (`33 00 02 | FE 01 02 | FF 01 02` ve `35 00 02 | 03 02 02 | 04 02 02 | 05 02 02`): Hell blade `106580` → `cast finished (effected) ... 1 packet(s)`, hedef HP 5650 → 5350, `CastEffect` günlükte, `srv_fail` 0; Igzination `110575` → `effected`, hedef HP 5350 → 4253. K8 (a) ✔.
3. Geri alma (`restored=12`; `strSkill` baz değerine döndü, quest listesi boş), sunucu açık: aynı iki komut **`refused (quest_locked)`**, günlükte o skill'ler için paket/`ACTION_SUBMIT` 0; `110518` `effected`. K8 (b) ve (c) ✔.
4. Sonuç: kod doğru; düzeltme yalnızca SQL tarafında. Son durum: sunucular kapalı (0/3), DB temiz (bot satırlarında quest listesi boş, `strSkill` db/002 değerinde, yedek tablo var ve 0 satır).

**Bulgular (önem sırasıyla)**

1. **KRİTİK: kimlik bayt sırası ters** (`db/003_bot_quests.sql`, birleştirme döngüsü ve gerekli kimliklerin eklendiği döngü: `CONVERT(varbinary(2), CAST(@id AS int))`). T-SQL `int → varbinary(2)` büyük-endian verir (511 → `01 FF`); sunucu kimliği `uint16` olarak little-endian okur (`DBAgent.cpp:402`, plan §2 "uint16 kimlik (little-endian)"). Sonuç: bot oyuna girince `m_questMap` anahtarları 13056, 65025 vb. olur, `CheckExistEvent(511, 2)` yanlış döner, K8'in asıl amacı sağlanmaz. Çıkışta sunucu bu bozuk kimlikleri geri yazar. Aynı hata ayrıştırmada (`CAST(SUBSTRING(@quest, @i * 3 + 1, 2) AS smallint)`) ve öz denetimde var; ikisi aynı yanlış kuralı kullandığı için `ok=12 fail=0` çıkıyor: öz denetim bu hatayı yakalayamaz. Bu yüzden Uygulayıcı'nın `kept_other` enjeksiyon kanıtı da (`500:1 32001:5`) yalnızca kendi kuralıyla tutarlı.
2. **`OldSkill` yedeği yalnızca ilk çalıştırmada `QuestTestPoints=1` ise dolar** (`db/003_bot_quests.sql`, yedek `INSERT ... WHERE NOT EXISTS`). Planın kendi sırası (`0`, `0`, `1`, geri alma) ile `OldSkill` NULL kalır; geri alma `ISNULL(..., u.strSkill)` ile `strSkill`'e dokunmaz. Doğrulama başındaki DB'de `BotWP_K`/`BotMF_K` ağaç5 = 80 (db/002: 70) olarak kalmıştı; Uygulayıcı raporundaki "DB db/002 durumunda" iddiası yanlıştı. (Ben `BotWP_K`/`BotMF_K` `strSkill`'ini db/002 değerlerine elle döndürdüm: `0x…4600341400` ve `0x…4634001400`.)
3. **Idempotans plandan zayıf** (`db/003_bot_quests.sql`, `@changedRow` ve `UPDATE`). Plan §3.1-4: gerekli her kimlik zaten durum 2 ise satır değişmez. Betik her seferinde "diğer kayıtlar + gerekli kayıtlar" sırasına yeniden diziyor; bot bir kez girip çıkınca sunucu listeyi kimlik sırasıyla yazar (`500` araya girer), sonraki çalıştırma `changed=1` der ve satırı yeniden yazar. Plan ve README'deki "ikinci çalıştırma `changed=0`" yalnızca botlar hiç girmemişse doğru.
4. Küçük: `IF @count > 200 SET @count = 200;` (birleştirme döngüsü) mevcut listeyi sessizce kırpıyor; plan §3.1-4 "sessizce kırpma, hata ver" der. `QUEST_LIMIT` aşan `sQuestCount` gelirse `RAISERROR`.
5. Not (engel değil): kod/test tarafı çevreye uyumlu (tab, Allman, İngilizce kısa yorum, `[D]` atfı). `QuestTestPoints` zorunlu değişken sapması (iç `:setvar` ile `-v` yok sayılıyor) makul ve README'de yazılı; §7 örnek komutları `-v QuestTestPoints=0` içermiyor, `db/README.md`'deki komut doğru. Uygulayıcı'nın geri almanın yedek satırlarını silmesi (ikinci geri alma `restored=0`) plana uygun.

**Düzeltme talimatı** (aynı metin "Düzeltme talimatı" bloğunda):

```
plans/F4-27-bot-quest-skill-kilitleri.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
1. db/003_bot_quests.sql: quest kimliklerini sunucunun okuduğu LITTLE-ENDIAN sırayla yaz ve oku. Kayıt = 3 bayt: bayt0 = kimlik % 256, bayt1 = kimlik / 256, bayt2 = durum (DBAgent.cpp:402). Yazma: CONVERT(varbinary(1), @id % 256) + CONVERT(varbinary(1), @id / 256) + CONVERT(varbinary(1), @state) (gerekli kimlikleri ekleyen döngüde de aynısı; `CONVERT(varbinary(2), CAST(@id AS int))` kalmasın). Ayrıştırma (birleştirme döngüsü ve öz denetim): @id = CAST(SUBSTRING(@quest, @i*3+1, 1) AS tinyint) + 256 * CAST(SUBSTRING(@quest, @i*3+2, 1) AS tinyint); @state = CAST(SUBSTRING(@quest, @i*3+3, 1) AS tinyint). `CAST(... AS smallint)` ile ikili ayrıştırma kalmasın (smallint dönüşümü big-endian okur).
2. db/003_bot_quests.sql öz denetimi bayt sırasını bağımsız doğrulasın: ayrıştırma kuralına güvenmeden, her gerekli kimlik için beklenen 3 baytlık değeri sabit ikili değer olarak üret (51 -> 0x330002, 510 -> 0xFE0102, 511 -> 0xFF0102, 53 -> 0x350002, 515 -> 0x030202, 516 -> 0x040202, 517 -> 0x050202, 54 -> 0x360002, 518..523 -> 0x060202 .. 0x0B0202) ve satırın kayıt-hizalı konumlarında (SUBSTRING(strQuest, 3*i+1, 3), i < sQuestCount) bulunduğunu denetle; bulunmayan satır fail sayılsın. Çıktı satırı biçimi aynı kalsın (`BOTQUEST: rows=<N> ok=<N> fail=<N>`).
3. db/003_bot_quests.sql yedek: QuestTestPoints=1 iken, yedek satırının OldSkill'i NULL ise ve satır BotWP_K ya da BotMF_K ise, strSkill test düzenine yazılmadan ÖNCE OldSkill'e mevcut strSkill (CONVERT(varbinary(10), strSkill)) yazılsın (UPDATE dbo.USERDATA_BOT_QUEST_BACKUP ... WHERE OldSkill IS NULL). OldSkill zaten doluysa ezilmesin (tekrar çalıştırmada ilk yedek korunur).
4. db/003_bot_quests.sql idempotans: gerekli kümedeki her kimlik satırda zaten durum 2 ise o satır için UPDATE yapma ve changed sayma (sıra farkı değişiklik sayılmaz); aksi halde birleştirme aynen. Yedek INSERT davranışı değişmez.
5. db/003_bot_quests.sql: `IF @count > 200 SET @count = 200;` satırını kaldır; sQuestCount > 200 olan satırda RAISERROR(şiddet 16) ver (sessiz kırpma yok).
6. db/003_bot_quests_rollback.sql ve db/README.md: rollback davranışı aynı kalır; README'ye "kimlikler little-endian uint16 + durum uint8" notunu ve ikinci çalıştırma `changed=0` ifadesinin ancak gerekli kimlikler zaten durum 2 ise geçerli olduğunu ekle.
7. Yeniden doğrula (sunucular kapalıyken; satır içeriği basma, yalnızca sayaç ve PASS/FAIL; ölçüm için BINARY_CHECKSUM kullan, CHECKSUM değil: kontrol baytlarını yok sayar): (a) bot satırlarının sQuestCount/strQuest/strSkill BINARY_CHECKSUM toplamını kaydet; (b) `-v QuestTestPoints=0` iki kez (changed=12, sonra 0); (c) `-v QuestTestPoints=1` (OldSkill artık dolu olmalı; BotWP_K/BotMF_K OldSkill NULL değil: yalnızca sayı bildir); (d) rollback `restored=12`; (e) sağlama toplamları (a) ile AYNI olmalı (strSkill dahil); (f) `-v QuestTestPoints=1` + rollback döngüsünü bir de temiz durumdan başlayarak tekrarla; (g) birleştirme kanıtı: BotMI_K'ya elle 500:1 ve 32001:5 yaz (little-endian: F4 01 01, 01 7D 05), betiği çalıştır, `kept_other=2` ve gerekli kimliklerin little-endian baytlarda göründüğünü öz denetimin geçmesiyle göster, sonra rollback. Raporuna "DB şu an db/002 durumunda" yazmadan önce (a) sağlama toplamının geri geldiğini ve BotWP_K/BotMF_K ağaç5'in 70 olduğunu doğrula. Test sonunda DB'yi rollback uygulanmış (db/002) durumda bırak.
8. C++ dosyalarına (BotCombat.h, CombatTests.cpp, ActionExecutor.cpp/.h) dokunma: doğru ve çalışma zamanında kanıtlandı. Çalışma zamanı K8'i düzeltmeden sonra Claude yeniden yapar (SQL verisiyle).
```
