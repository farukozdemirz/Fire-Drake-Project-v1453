# F4-27: Quest ile açılan skill'ler: bot kurulumunda quest durumu (`db/003`) ve `BeginCast`'te `Etc` denetimi (`quest_locked`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu:
- SQL çıktı satırları (`BOTQUEST:` / `BOTQUEST_ROLLBACK:`):
- Kabul kriterleri öz-değerlendirme:
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD
