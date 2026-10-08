# UA-05b: AlphaGame DB'de oyun içi düzenlemeler ve botların taşınması

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | UA — Sürüm yükseltme tabanı AlphaGame 1534 (ADR-0069 madde 4) |
| Branch | `bot/UA-05b` (bölüm A) ve `bot/UA-05c` (bölüm B), taban `yukseltme/alpha` |
| Bağımlı olduğu planlar | UA-05 (DOĞRULANDI) |
| İlgili gereksinim / kabul | Proje sahibi isteği 2026-10-09; MB-01 (`docs/03`), `db/003`–`db/011`, UA-H S-8 |
| Tahmini büyüklük | M + L |
| Hazırlayan / tarih | Claude / 2026-10-09 |

---

## 1. Amaç

Proje sahibinin 2026-10-09 isteği: (A) potlar tükenmesin; görev kilitli skill'ler açılsın; Ronark bowl'da yalnız Attross ve Riote kalsın; `DreamOfKings` klanı A1 olsun. (B) Eski sunucudaki bot karakterleri (klanları, 80. seviye, teçhizatı, skill görevleriyle) yeni sunucuda aynı nick'lerle olsun; `BotWP_K` botunun nick'i `Xeli0n` yerine yeni bir nick (proje sahibi `Xelion`'u aldı).

## 2. Bağlam (okunması zorunlu)

- Hedef DB: `.\SQL2019` → `FDP_alpha_game`. **Bu planda hedef DB'ye yazılmaz.** Betikler bir sahne kopyasında (`FDP_alpha_stage_a` / `FDP_alpha_stage_b`, `BACKUP ... COPY_ONLY` + `RESTORE ... WITH MOVE`) denenir; gerçek DB'ye Claude sunucular kapalıyken uygular.
- MB-01 (`docs/03` §6.2, ADR-0009): eski veride NPC pot skill'lerinin `MAGIC.UseItem = 0`, sunucu potu ne denetliyor ne tüketiyordu. AlphaGame verisinde potlar tüketiliyor (UA-H §4.5).
- Skill görev kilidi: istemci `Skill_Magic_Main_us.tbl` sütun 29 = görev kimliği (ör. Howling Sword 106570 → 51, blooding 106575 → 510, Hell blade 106580 → 511). Eski çözüm `db/003_bot_quests.sql`: 51–54 ve 510–523 görevleri durum 2. AlphaGame `strQuest` 3888 bayt (1296 × 3 bayt: u16 kimlik + u8 durum), sayaç `sQuestCount` (C §5).
- Bowl: eski düzenleme `FDP_kn1534.dbo.K_NPCPOS_BOWL_BACKUP` (silinen satırlar) ve `FDP_kn1534.dbo.K_NPCPOS` zone 71 (kalanlar); bowl alanı ve korunan canavarlar oradan çıkarılır. AlphaGame zone 71 `K_NPCPOS` 296 satır.
- Botlar: `db/005`–`db/011` (hesap, karakter, teçhizat, scroll, nick, klan); kaynak satırlar `.\SQLEXPRESS` `FDP_kn_online` (canlı 1453; yalnız bot satırları: `BOT_NICK_MAP` ve `db/010` kalabalık listesi). `BotCore/BotNames.h` ve `tools/botnames.py` eski adları nick'lere eşler.
- Kişisel veri: yalnız bot satırları ve proje sahibinin kendi kayıtları (`Xelion`, `DreamOfKings`) okunur; başka satır okunmaz.

## 3. Kapsam

**Bölüm A (`bot/UA-05b`, `db/024`–`db/027` + rollback):**
1. `db/024`: HP/MP pot skill'lerinde `MAGIC.UseItem = 0` (MB-01'in yeniden üretimi). Kapsam: oyuncunun NPC'den alabildiği HP/MP potları ve AlphaGame'in pot listesindeki diğer HP/MP potları; liste raporda (skill, eski `UseItem`, eşya adı). Scroll'lar ve diğer tüketilebilirler dışarıda.
2. `db/025`: `Xelion` için skill görevleri 51–54 ve 510–523 durum 2 (mevcut görevler korunur, sayaç güncellenir).
3. `db/026`: zone 71 bowl canavarları silinir, Attross ve Riote kalır (kimlikler `K_MONSTER`'dan); eski düzenlemenin alan kuralı raporda; silinen satırlar yedek tabloda.
4. `db/027`: `DreamOfKings` (IDNum 1) `Flag = 7` (Accredited 1), `sCape = 0`.

**Bölüm B (`bot/UA-05c`, `db/028`+ ve kod):**
5. Bot hesapları, karakterleri (80. seviye), klanları (HeaveN, Endless, BraveHeartH, ZOGiST ve `db/011` klanları) ve üyelikleri `FDP_kn_online` bot satırlarından AlphaGame şemasına; teçhizat (`strItem` 0–41 yuvaları, reverse +11/+5 karışık zırhlar, +0 aksesuarlar, silahlar) eski satırlardan aynen; her eşya kimliği AlphaGame `ITEM`'de ve 1534 istemcisinde doğrulanır, eksik olan raporda; skill görevleri bot satırlarının `strQuest`'inden.
6. `BotWP_K` nick'i `Xeli0n` → `VoRteX` (iki DB'de kullanılmadığı doğrulanır): DB satırı ve `BotCore/BotNames.h` + `tools/botnames.py` (+ testler).

**Yok:** bot davranışı/ayarı (UA-07); `[BOT] ENABLED`; `FDP_alpha_game`'e yazma (Claude yapar); `FDP_kn_online`/`FDP_kn1534`'e yazma.

## 4. Dokunulabilecek dosyalar

A: `db/024_*`–`db/027_*` (+ rollback), `db/README.md`. B: `db/028_*`+ (+ rollback), gerekirse `tools/ua-gen-bots.py`, `BotCore/BotNames.h`, `tools/botnames.py`, `Tests/BotCoreTests/*` (yalnız nick testi), `db/README.md`.

## 5. Kabul kriterleri

- [ ] K1: Her betik sahne kopyasında uygula → tekrar (değişiklik 0) → geri al → uygula; etkilenen tabloların sağlama toplamları raporda.
- [ ] K2: A1: pot listesi; A2: `Xelion` görev listesi; A3: bowl'da kalan canavar kimlikleri yalnız Attross/Riote; A4: klan bayrağı 7.
- [ ] K3: B: bot sayısı, klan üyelikleri, her botun teçhizatı eski satırla bayt bayt aynı (0–41), eksik eşya 0 ya da gerekçeli liste.
- [ ] K4: B: `./tools/build.sh Release` ve `./tools/run-tests.sh Release` 0 başarısız (nick değişikliği).
- [ ] K5: Okunan kişisel satırlar yalnız bot ve proje sahibi kayıtları (sorgu listesi raporda).
- [ ] K6: `git diff --stat` yalnız §4; `git status --short` temiz.

## 6. Kısıtlar

- Sunucu başlatılmaz/durdurulmaz. `FDP_alpha_game`, `FDP_alpha1534`, `FDP_kn_online`, `FDP_kn1534`, `FDP_smoke1534`'e yazılmaz; sahne kopyaları iş sonunda bırakılır (Claude siler).
- Git: `AGENTS.md` §2.8; commit `[UA-05b] ...` / `[UA-05c] ...`; push yok.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
