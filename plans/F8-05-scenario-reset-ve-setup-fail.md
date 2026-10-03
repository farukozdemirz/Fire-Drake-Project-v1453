# F8-05: `ScenarioReset` — maç öncesi başlangıç yerleşimi ve durum sıfırlama (bot çevrimdışıyken DB yazımı), başlangıç doğrulaması ve `SETUP_FAIL` (ADR-0032-DEG, docs/15 §6a, KI-DEG-05)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F8 — Değerlendirme ve 8v8 (`docs/17` §2; kapı G8 ve, `EVAL-1v1` için, F6 kabulü) |
| Branch | `bot/F8-05 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F8-06** (takım tanımı `team_a`/`team_b` ve `MatchEvents`; yerleşim hangi takımın hangi başlangıç noktasına gideceğini buradan alır), F4-40 `KAPANDI` (`db/004` çanta düzeni; C++ eşdeğeri bu planda), F3-03 `KAPANDI` (`ScenarioRunner`), F2-04/F2-05 `KAPANDI` (despawn/respawn yolu), F4-16/F4-17 `KAPANDI` (`SelfState`/`snap`). F8-07 bu plana bağımlıdır (SETUP_FAIL'in sayıma katılmaması). F8-03/F8-04 gerekmez |
| İlgili gereksinim / kabul | `docs/15` §6a (satır 266-281), ADR-0032-DEG (madde 1-6, "Doğrulama"), KI-013, KI-DEG-05, STK-01 (`docs/11`), CON-03, ARENA-03/05, T-IGT-EVAL-01 ("başlangıç doğrulaması %100; art arda iki maçta `snap` değerleri eşit"), AC-ARCH-05, `docs/16` §7 (geçersiz maç) |
| Tahmini büyüklük | L (12 dosya; HAZIR'dan önce bölme kararı: §0 madde 7) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak; referanslar `gece/2026-10-02` @ `7891f74`) |

---

## 0. Neden TASLAK (HAZIR yapma ön koşulları)

1. **F8-06 bitmiş olmalı:** ScenarioRunner'ın takım tanımı (`team_a`/`team_b` anahtarları ve ulusa göre türetme) F8-06'da yazılır; bu plan onu tüketir. F8-06 olmadan yerleşim "hangi bot hangi tarafta" bilgisinden yoksundur.
2. **ADR-0032-DEG Ek F8-05 (yazım yolu kararı; Claude yazar, proje sahibi onaylar).** ADR madde 2 "kurulum yazımı DB iş parçacığı kuyruğunda (FIFO)" der; kodda `DatabaseThread` kuyruğu `AddRequest(Packet*)` ile çalışır (`GameServer/DatabaseThread.cpp:23`) ve her istek bir `WIZ_*` opkodu ister (`:76` `switch`). Öte yandan IOCP thread'inden doğrudan `g_DBAgent` çağrısı bugün de yapılır (`GameServer/User.cpp:3599, 5888, 5912`) ve `OdbcConnection` bir `recursive_mutex` taşır (`shared/database/OdbcConnection.h:68`). Bu plan **senkron yazımı** önerir `[A]`: sıra garantisi ADR'nin amacını sağlar, çünkü yazım (i) `ReqUserLogOut`'un son ifadesi (`m_deleted = false`, `DatabaseThread.cpp:437-462`) bittikten sonra, yani oturum `PHASE_DESPAWNED` olarak gözlendikten sonra ve (ii) bir sonraki `WIZ_SEL_CHAR` DB isteği kuyruğa girmeden önce yapılır (`ScenarioRunner` aynı IOCP tick'inde `CommandSpawn` çağırmadan önce yazar). `OdbcConnection` paylaşımının eşzamanlı güvenliği ve IOCP thread blokajı (16 bot × ms) **doğrulanmadı**; Uygulayıcı ölçüp raporlar. Alternatif (yeni dahili opkod + `DatabaseThread` kuyruğu) daha fazla dosya demektir; seçim ADR Eki'nde kayda geçer. Karar proje sahibinde.
3. **Buff sızıntısı iddiası düzeltilmeli.** `docs/15` §6a satır 274 "çıkışta bellek içi durum biter" der; kodda kalıcı etkiler `USER_SAVED_MAGIC` tablosuna yazılır ve girişte `LoadSavedMagic` ile geri yüklenip `RecastSavedMagic` ile yeniden uygulanır (`GameServer/DatabaseThread.cpp:258, 455`; `DBAgent.cpp:649-720`; `CharacterHandler.cpp:44`). Saklama iki yerde olur: `nSkillID > 500000` olan etkiler (`MagicInstance.cpp:1791-1795` `[D]`) ve Type9 yürütmesi (`MagicInstance.cpp:2156`, koşulsuz `InsertSavedMagic`). Bot sınıf skill'leri ve pot büyüleri (`490014..490701`) 500000'in altındadır; botun Type9 skill'i kullanıp kullanmadığı **doğrulanmadı**. Beklenen durum boş buff'tır, ama bu varsayımdır; ama `USER_SAVED_MAGIC` bir `USER_*` tablosudur ve bu plan ona **hiç dokunmaz** (CLAUDE.md DB kuralı). Sonuç: buff/cooldown temizliği yazım değil **doğrulama** ile (§3.2: `buffTotal = 0`, `cooldownTotal = 0`) güvence altına alınır; sızıntı görülürse `SETUP_FAIL BUFF` ve proje sahibine soru (tablo yazımı ayrı karar).
4. **Ekipman dayanıklılığı §6a'da yok.** `CUser::ItemWoreOut` her saldırıda/hasarda ekipman `sDuration` değerini düşürür, 0'da eşya kırılır ve `SetUserAbility(false)` ile stat düşer (`GameServer/User.cpp:3270-3345`; çağrı yerleri `AttackHandler.cpp:86-90`, `MagicInstance.cpp:1483-1484`). Çıkışta kaydedilir (`UpdateUser`). Sıfırlanmazsa art arda maçlarda botlar giderek zayıflar. Bu plan yuva 0..13 dayanıklılığını `ITEM.Duration`'a geri yazar (§3.1) ve `docs/15` §6a'ya satır eklenmesini (Claude) ister.
5. **NP, `snap`'te yok.** `/bot snap` çıktısı konum, HP/MP, ölü/diri, oturma, stok, buff/cooldown ve takımı yazar (`BotManager.cpp:2641-2656`); NP (`USERDATA.Loyalty`, `LoadUserData` `m_iLoyalty`) yoktur. NP doğrulaması bu yüzden `CUser::GetLoyalty()` ile sunucu tarafında yapılır (`ActionExecutor.cpp:1919` aynı erişimi kullanır). `snap`'e NP eklemek kapsam dışıdır (§3.3).
6. **Başlangıç noktası yürünebilirliği `[A]`.** ADR-0032 "arena merkezinden ±35 m, karşılıklı" der; tam nokta/yön ve noktanın yürünebilir/boş olduğu `[V]` değildir (arena A merkezi (1274, 890): `docs/15` satır 49-66, ADR-0033-DEG; ızgara F5-55 sonrası sorgulanabilir). Plan, noktayı senaryo anahtarlarıyla (`start_a`, `start_b`) ayarlanabilir yapar ve yürünebilirlik denetimini F5-55 hazır olunca ekler (açık soru).
7. **Boyut:** 12 dosya, ≤ ~10 sınırının üstünde. HAZIR yapmadan önce Claude ikisinden birini seçer: (a) tek plan, iki ayrı commit (S1 = `BotCore` saf mantık + test, adım 1-4; S2 = sunucu bağlama, adım 5-9); (b) iki plan (F8-05 saf mantık + test; sunucu bağlaması yeni numara, F8-05 ona bağlanır). Bu taslak (a) varsayımıyla yazılmıştır.
8. **Proje sahibi izni:** sunucu kodunun `Bot%` satırlarına yazması, `AGENTS.md` §2.7 kapsamındadır (bot satırları serbest) ama planın HAZIR satırı izni açıkça kaydeder.

## 1. Amaç

`ScenarioRunner` her koşuda (maç başında) botlara aynı başlangıcı verir: konum, HP/MP/NP, çanta stoğu, ekipman dayanıklılığı sıfırlanır; buff/cooldown/party/NPC boşluğu doğrulanır; doğrulama başarısızsa maç **başlamaz**, `SETUP_FAIL` ayrı sonuç olarak kaydedilir ve geçerli maç sayısına/kazanma oranına **katılmaz**. KI-DEG-05'in "durum sıfırlama" yarısı kapanır (kazanma kuralı yarısı F8-07).

## 2. Bağlam (okunması zorunlu)

- `docs/15` §6a (satır 266-281: sıfırlama tablosu satır 270-279, "Başlangıç doğrulaması" satır 281), `docs/16` §7 (geçersiz maç), `docs/11` STK-01..STK-05.
- ADR-0032-DEG: "Karar" madde 1-4 ve "Canlı `CUser` ile tutarlılık" madde 1-6 (satır 31-38); "Doğrulama" (satır 53).
- `GameServer/Bot/ScenarioRunner.cpp`: `StartRun` `:689-725` (`CommandSpawn` `:703`, `STATE_PREPARE`), `Tick` `STATE_PREPARE` `:732-` (hepsi `PHASE_IN_GAME` olunca `match start` `:756`), `STATE_CLEANUP` `:826-` (`BeginDespawn` `:829`) (`BeginDespawn`, hepsi `PHASE_DESPAWNED` olunca sonraki koşu `StartRun`), anahtar tablosu `:273-274` (bilinen anahtarlar), `:301` `zone` yalnızca 71, `:325` `bots`, `:451` `duration_sec` (else dalı).
- `GameServer/Bot/BotSession.h:28` (`PHASE_DESPAWNED`: "slot havuza döndü, `m_pUser == nullptr`"), `:61-62` (`m_pUser`, `m_phase`: IOCP thread'i); `BotManager.h:51` `friend class ScenarioRunner`.
- `GameServer/Bot/BotManager.cpp:3427` yorumu: `m_deleted` DB thread'inde `ReqUserLogOut`'un son ifadesi olarak temizlenir; `:683` `IsKnownBotName` (yazım yetki sınırı: yalnızca `BOT_TABLE` adları); `:2500` `FillSelfExtras` (stok sayımı: `ActionExecutor::PotKindOf`).
- `GameServer/DBAgent.cpp:324-420` `LoadUserData`: sütun sırası; konum `PX/PZ/PY` tamsayı = metre × 100 (`/100.0f`, `:378-380`), `Hp`/`Mp` `int16` (`:365-366`, bu yüzden `db/002` 32000 yazar ve girişte maksimuma kırpılır), NP = `Loyalty` (`:360` `m_iLoyalty`), `Zone` (`:376`), `Bind` (`:377`); `UpdateUser` `:916-` (çıkışta yazılan alanlar).
- `db/002_bot_characters.sql:260-274` (sütun listesi ve sabitler: `Zone 71`, `PX 127400`, `PZ 89000`, `PY 0`, `Loyalty 1000`, `Sp 100`, `Bind -1`), `db/004_bot_inventory.sql:205` (`STUFF(strItem, 113, 64, ...)` yuva 14..21) ve F4-40 §3.1 (şablon tablosu; `HpPots=100, MpPots=0, LifeStones=30, ClassStones=50` varsayılanı `db/002` ile bayt bayt aynı).
- `docs/15` satır 49-66 ve ADR-0033-DEG: arena A merkezi (1274, 890), `P-ARENA-R` 60 m; ADR-0032 "başlangıç noktaları arena merkezinden ±35 m".
- KI-013 (`docs/KNOWN_ISSUES.md:20`): NP 0 → `RequestRegene` `no_np` (`ActionExecutor.cpp:1918-1919`); KI-DEG-05 (`:33`).

## 3. Kapsam

### 3.1 S1 — `BotCore/ScenarioReset.h` (yeni, saf mantık; sunucu başlığı yok; `BotCore` kuralları: `docs/13` §11)

- `struct ResetPoint { float x, z; }` ve `ComputeStartPoints(...)`: arena merkezi + `start_a`/`start_b` (metre; varsayılan merkezden ∓35 m x ekseni, `[A]`) ve **takım içi dağılım**: 3 sütunlu, 1,5 m aralıklı ızgara, takım noktasına ortalı; 8 bot için en uzak bot noktadan ≤ 2,2 m (§6a "≤ 3 m" toleransı içinde); aynı girdi aynı çıktıyı verir (belirlenimli).
- `struct ResetRow` (DB'ye yazılacak değerler): `zone = 71`, `px/pz/py` (metre × 100, `int32`), `hp = mp = 32000` (sunucu kırpar, `db/002` ile aynı), `sp = 100`, `np = 1000`, `bag[64]` (yuva 14..21), `equipDurability[14]` ya da "dayanıklılık yeniden hesapla" bayrağı.
- `BuildBagBlock(profile, stock, itemMeta)`: F4-40 §3.1.3 tablosuyla (yuva 14 `389014000`×1, 15 `389015000`×`HpPots`, 16 `389020000`×1, 17 `379006000`×`LifeStones`, 18 sınıf taşı×`ClassStones`, 19 scroll, 20 yalnız mage `379070000`, 21 `389220000`×`MpPots`) **aynı** 64 baytı üretir (kimlik `int32`, dayanıklılık `int16`, adet `int16`, little-endian; `Countable == 0` ise adet 1; üst sınır 9999). `itemMeta` çağırandan gelen arama işlevidir (`Duration`, `Countable`); `BotCore` sunucu tablolarını bilmez.
- `VerifyStart(spec, observed) -> uint32 reasonMask`: saf karşılaştırma. Gözlem alanları: `zone`, `x/z`, `hp/maxHp`, `mp/maxMp`, `np`, `dead`, `buffTotal`, `cooldownTotal`, `hpPotStock`, `mpPotStock`, `inParty`, `visibleNpcCount`, `equipDurabilityOk`. Sebep kodları (sabit metin, `docs/16` §3.2 `SETUP_FAIL` alanı): `POS`, `HP`, `MP`, `NP`, `DEAD`, `ZONE`, `BUFF`, `COOLDOWN`, `STOCK`, `PARTY`, `NPC`, `DUR`, `DB_WRITE`, `TIMEOUT`. Eşikler: konum ≤ 3,0 m, HP == maxHp, MP == maxMp, NP ≥ 1000 (§6a), `buffTotal == 0`, `cooldownTotal == 0`, stok == `FillSelfExtras` ile aynı sayım kuralı (HP pot adedi = `HpPots + 1`, MP pot adedi = `MpPots + 1` varsayılan `db/004` anlamıyla).
- `Tests/BotCoreTests/ScenarioResetTests.cpp` (yeni): `ComputeStartPoints` (8 bot ızgara yarıçapı ≤ 2,2 m; iki takım arası 70 m; belirlenimlilik), `BuildBagBlock` (varsayılan stok yuva baytları elle yazılmış beklenenle; `0` stok boş yuva; `Countable == 0`; 9999 üst sınırı; mage yuva 20), `VerifyStart` (her sebep kodu için bir olumsuz, bir sınır değeri: 3,0 m geçer, 3,1 m `POS`; NP 999 `NP`; hepsi geçerliyken maske 0).

### 3.2 S2 — sunucu bağlama

- **`GameServer/Bot/ScenarioReset.{h,cpp}` (yeni):** `ScenarioReset::Apply(const std::vector<BotSession*> &, const ResetSpecs &, std::string & error)`. Ön koşul: listedeki **her** oturum `PHASE_DESPAWNED` (aksi halde hiç yazmaz, `DB_WRITE` sebebiyle reddeder); her ad `BotManager::IsKnownBotName` ile denetlenir (ADR-0032 madde 6); bot başına **tek** parametreli `UPDATE dbo.USERDATA SET Zone=?, PX=?, PZ=?, PY=?, Hp=?, Mp=?, Sp=?, Loyalty=?, strItem=? WHERE strUserID=?` (`strItem` tamamı: önce aynı bot satırından `SELECT strItem` ile okunur, 0..13 yuvalarının dayanıklılığı `ITEM.Duration`'a ve 14..21 `BuildBagBlock` çıktısına çevrilip 584 bayt olarak yazılır; ekipman kimlikleri ve yuva 22..72 **değişmez**), etkilenen satır == 1 denetimi. `USER_SAVED_MAGIC`, `WAREHOUSE` ve başka satır/tablo **dokunulmaz**. Bot satırı içeriği loga yazılmaz (yalnızca sayaç ve süre: `reset_ms`).
- **`GameServer/DBAgent.{h,cpp}` (değiştir):** `CDBAgent::ResetBotScenarioRow(...)` (yukarıdaki okuma + yazma; `OdbcCommand` kalıbı `UpdateUser` ile aynı). Başka `CDBAgent` yöntemi değişmez.
- **`ScenarioRunner` (`.h/.cpp`):**
  - Yeni senaryo anahtarları (`:273-274` bilinen anahtar listesi, tekrar anahtarı hatası kalıbı korunur): `reset` (`on`/`off`, **varsayılan `off` = bugünkü davranış**, ADR-0032 "geri alma"), `start_a`, `start_b` (`[x, z]` metre), `hp_pots`, `mp_pots`, `life_stones`, `class_stones` (0..9999, varsayılan 100/0/30/50). Takım üyeliği F8-06'dandır (`team_a`/`team_b`; yoksa ulusa göre: Karus A, El Morad B).
  - `reset: on` iken akış: `StartRun` → listedeki oyunda olan bot varsa **önce despawn** (`BeginDespawn`, mevcut `STATE_CLEANUP` bekleme kalıbı) → hepsi `PHASE_DESPAWNED` → `ScenarioReset::Apply` (başarısızsa `SETUP_FAIL DB_WRITE`) → `CommandSpawn` → `STATE_PREPARE` hepsi `PHASE_IN_GAME` olunca **`kSetupSettleMs` (2000 ms, `[A]` adlandırılmış sabit)** bekle (NPC/party/buff bildirimlerinin gelmesi için) → **başlangıç doğrulaması** (aşağıda) → geçerse `match start` (bugünkü yol), geçmezse `SETUP_FAIL`.
  - **Başlangıç doğrulaması (kaynak: canlı `CUser`, ADR-0032 madde 5):** her bot için `m_pUser`'dan konum, `GetHealth/GetMaxHealth`, `GetMana/GetMaxMana`, `GetLoyalty`, ölü/diri, `m_buffMap` sayısı (`m_buffLock` altında, `FillSelfExtras` ile aynı), cooldown (oturumun `m_castSkillLast`), `FillSelfExtras` stok sayımı, `isInParty()`, görünen NPC tablosu boş mu, ekipman dayanıklılığı `ITEM.Duration`'a eşit mi; sonuç `VerifyStart` maskesi. Ayrıca algı katmanı ile tutarlılık: `BuildSnapshot` `SelfState` konum/HP/MP/stok değerleri `CUser` ile aynı olmalıdır (`/bot snap` kaynağı).
  - **`SETUP_FAIL` kaydı:** `Telemetry::Emit(TEL_SUMMARY, "SETUP_FAIL", -1, nullptr, fields, false)`; `fields` = `"run":<n>,"reasons":["POS",...],"bots":["<ad>",...]` (sabit metinler; ad yalnızca bot adı). Kayıt bir maç bağlamı gerektirir: başarısız kurulumda `BotManager` üzerinden `match start`/`match end invalid` çifti açılıp kapatılır (`F8-06` `BeginScenarioMatch`/`EndScenarioMatch`; `MATCH_END` `result = invalid`, `reason = SETUP_FAIL`, `end_state = completed`). `tools/bot-outcome-eval.py` `SETUP_FAIL` varlığını MATCH_START'tan bağımsız `invalid` sayar (`bot-outcome-eval.py` `evaluate_match` ilk dal), yani bu biçim araçla uyumludur. **ADR-0032/docs/15 "MATCH_START yerine SETUP_FAIL" der; bu plan MATCH_START'ı da yazar (açık soru A1).**
  - **Sayım:** `SETUP_FAIL` koşusu `m_completedRuns`'a **girmez**; `Finish` özet satırı `completed=<n> setup_fail=<n>` ayrı yazar; koşu otomatik **yeniden denenmez** `[A]`; art arda 3 `SETUP_FAIL` senaryoyu `aborted (setup_fail_streak)` ile bitirir `[A]` (çünkü ardışık hata genelde kalıcı nedendir).
- **`proj-GameServer.vcxproj` ve `.filters`:** `Bot\ScenarioReset.cpp/.h` eklenir (yeni dosyalar derlenmesi için zorunlu).

### 3.3 Kapsam dışı (yapılmayacak)

- Konum kurulumunun canlı `CUser`'a yazılması, maç içi ışınlama, `TEST_TELEPORT` yolu (F8-06: olay şeması; bu planda emisyon yok).
- `USER_SAVED_MAGIC`/`WAREHOUSE`/başka tablolara yazma; `db/002..004` değişikliği; `tools/bot-refill.sh` (sunucular kapalıyken elle araç olarak kalır).
- `snap`'e NP veya taş sayısı eklemek (`Perception.h` `SelfState`); `BotAgent`/`TeamBlackboard` sıfırlaması (ilgili sınıflar F6/F7'de yazılır; bu plan yalnızca "her koşu yeni oturum" gerçeğini doğrular ve F6/F7 planlarına §6a satırını bırakır); party kurulumu (F7 betiği; bu plan "party yok"u doğrular).
- Maç kuralları, kill/win sayımı (F8-07); olay emisyonu (F8-06); `swap_sides` ve taraf değişimi otomasyonu (F8 harness planı: iki senaryo dosyası ya da ayrı plan); başlangıç noktası yürünebilirlik denetimi (F5-55 sonrası).
- `ENABLED=0` yolu: `reset` anahtarı yoksa/`off` ise tek satır bile çalışmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/ScenarioReset.h` | yeni | saf mantık; ASCII, CRLF; sunucu başlığı yok |
| `Tests/BotCoreTests/ScenarioResetTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj` | değiştir | `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | `ClCompile` satırı |
| `GameServer/Bot/ScenarioReset.h` / `.cpp` | yeni | ASCII, CRLF |
| `GameServer/proj-GameServer.vcxproj` / `.filters` | değiştir | iki yeni dosya |
| `GameServer/Bot/ScenarioRunner.h` / `.cpp` | değiştir | yeni anahtarlar, Reset/Verify/SETUP_FAIL akışı |
| `GameServer/DBAgent.h` / `.cpp` | değiştir | yalnızca `ResetBotScenarioRow` |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz (özellikle `BotManager.cpp`/`BotSession.h` erişimi gerekirse: `friend class ScenarioRunner` zaten var).

## 5. Uygulama adımları

1. `git switch -c bot/F8-05 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucular kapalı (`./tools/run-servers.sh status`).
2. S1: `BotCore/ScenarioReset.h` + testler + iki `vcxproj`; `./tools/run-tests.sh` (yeni vakalar + eski sayı). Commit 1.
3. `DBAgent` yöntemi: bot satırı okuma + tek `UPDATE`; satır sayısı denetimi; adres ad yetki sınırı. Yazım yolunun IOCP thread'inden güvenliğini ölç (`reset_ms`, 16 bot) ve rapora yaz.
4. `ScenarioReset.{h,cpp}` ve `vcxproj/.filters`.
5. `ScenarioRunner`: anahtarlar → akış → doğrulama → `SETUP_FAIL` → sayım. `reset: off`/anahtarsız yolun **bayt bayt aynı** davranışı (mevcut senaryo dosyaları değişmeden çalışır) elle denenir.
6. `./tools/build.sh Release` (ve istenirse Debug); Commit 2.
7. Uygulayıcı Raporu; `Durum` → `UYGULANDI`. Sunucuyu çalıştırmak ve oyun içi doğrulama Claude'undur (K8..K10).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; `./tools/run-tests.sh` `0 failed`; yeni test sayısı = işe başlamadan önceki sayı + yeni vaka (ikisi de rapora)
- [ ] K2: `git diff --stat gece/2026-10-02...bot/F8-05` yalnızca §4'teki dosyalar (+ plan); `git diff --check` boş; `reset` anahtarı olmayan bir senaryo dosyasıyla `ScenarioRunner` çıktısı (log satırları) değişmedi (mevcut senaryo dosyasıyla karşılaştırma)
- [ ] K3 (birim): `ScenarioResetTests` — başlangıç noktaları (8 bot ızgara ≤ 2,2 m, takımlar 70 m arayla), stok bloğu (varsayılan 100/0/30/50 beklenen baytlar; `0` → boş yuva; scroll adedi 1; 9999 sınırı), `VerifyStart` her sebep kodu için olumsuz vaka ve sınır vakaları; `./tools/run-tests.sh` `0 failed`
- [ ] K4 (yetki sınırı): `ScenarioReset::Apply` `BOT_TABLE` dışı adı yazmadan reddeder; yazılan tek tablo `USERDATA` ve tek koşul `WHERE strUserID = ?`: `grep -n "USER_SAVED_MAGIC\|WAREHOUSE\|DELETE\|INSERT" GameServer/Bot/ScenarioReset.cpp GameServer/DBAgent.cpp` yeni koddan **0 eşleşme** döndürür (kanıt `dosya:satır`); bot dışı satır sayısı `COUNT` ile değişmez (§7)
- [ ] K5 (canlı durum ↔ DB tutarlılığı, **spawn sonrası `/bot snap` ile**, ADR-0032 madde 5; Claude çalışma zamanı): `reset: on`, 2 botlu senaryo (`BotWP_K`, `BotWP_E`), bot önce kirletilmiş (script: `move` ile uzaklaştır, `pot` ile pot tüket, saldırı alıp HP düşür): koşu başında `snap` değerleri senaryodan beklenenle aynı: konum ≤ 3 m, `hp == maxHp`, `mp == maxMp`, `stock hp_pot=101 mp_pot=1` (varsayılan 100/0 + tüketilmeyen 1), `buffs 0`, `cooldowns 0`, `team in_party=0`, `alive`; sunucu tarafı doğrulama günlüğü (`Bot_*.log`) `setup verify: ok` ve her bot için `CUser` alanları ile `snap` aynı
- [ ] K6 (art arda iki koşu eşitliği, T-IGT-EVAL-01): aynı senaryo `repeat: 2`, arada gerçek hasar/pot tüketimi/ölüm; iki `MATCH_START`ın başlangıç `snap` kümeleri (konum, `hp`, `mp`, stok, NP) birebir eşit; ikinci koşuda NP **1000** (ilk koşuda ölüm −50 sonrası bile; KI-013 kanıtı)
- [ ] K7 (ekipman dayanıklılığı): iki koşu arasında silah/zırh dayanıklılığı düşmüş bot (saldırı ile) ikinci koşu başında `ITEM.Duration`'a eşit (doğrulama günlüğü `DUR ok`; DB'den satır içeriği okunmaz, sunucu doğrulaması kanıttır)
- [ ] K8 (olumsuz yol: `SETUP_FAIL`; ön koşul: F8-03 birleşmiş ve `db/005` **uygulanmamış**, aksi halde bu madde `BEKLİYOR` yazılır ve yalnızca birim testteki `DB_WRITE` vakası geçerlidir): satırı olmayan bir `BOT_TABLE` adıyla (`BotWP2_K`) `reset: on` koşusu: `UPDATE` 0 satır ⇒ `SETUP_FAIL DB_WRITE`; `Logs/bots/<tarih>/<match>.jsonl` içinde `SETUP_FAIL` satırı ve `MATCH_END` `result:"invalid"`; `python3 tools/bot-outcome-eval.py <dosya>` çıktısı `invalid` ve neden `SETUP_FAIL`; `Finish` özeti `completed=0 setup_fail=1`; art arda 3 başarısızlıkta `aborted (setup_fail_streak)`
- [ ] K9 (sayıma katılmama): iki başarılı + bir `SETUP_FAIL` koşusu olan senaryoda özet `completed=2 setup_fail=1`; `m_completedRuns` 2
- [ ] K10 (`ENABLED=0` ve `reset: off`): `[BOT] ENABLED=0` sunucu davranışı değişmedi (açılış logu, yeni kod yolu çalışmıyor); `reset` anahtarsız eski senaryo (F3-03 örnekleri) aynı sonucu verir
- [ ] K11: Uygulayıcı Raporu dürüst: çalıştırılmayan oyun içi doğrulama açıkça "yapılmadı" yazılmış; IOCP senkron yazım süresi (`reset_ms`) ve `OdbcConnection` paylaşımı gözlemi raporda; bot satırı içeriği rapora girmemiş

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status
./tools/build.sh Release && ./tools/run-tests.sh 2>&1 | tail -3
git diff --stat gece/2026-10-02...bot/F8-05
git diff --check gece/2026-10-02...bot/F8-05
grep -n "USER_SAVED_MAGIC\|WAREHOUSE\|DELETE\|INSERT" GameServer/Bot/ScenarioReset.cpp; echo "rc=$? (beklenen 1: eşleşme yok)"
# Claude, calisma zamani (sunucu acik, [BOT] ENABLED=1, TELEMETRY=summary)
#   /bot scenario run reset-smoke        # reset: on, 2 bot, repeat: 2
#   /bot snap BotWP_K ; /bot snap BotWP_E    # kosu basinda ve ikinci kosu basinda
python3 tools/bot-outcome-eval.py Logs/bots/<tarih>/   # SETUP_FAIL kosusu invalid
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları (yeni `.cpp/.h` ASCII + CRLF; ISO-8859 dosyalarda dönüştürme yok; `DBAgent.cpp` kodlamasını koru, `grep -a`).
- **DB kuralı:** yalnızca `Bot%` satırları ve `BOT_TABLE` adları; satır içeriği (envanter baytları dahil) loga/rapora basılmaz; `USER_*`, `WAREHOUSE*`, `CURRENTUSER` vb. dokunulmaz. Elle tek seferlik `UPDATE` yapma; her yazım koddan.
- **Thread kuralı (`AGENTS.md` §2.6):** `Apply` ve doğrulama IOCP thread'inde (`Tick`); başka thread'den `CUser`/`BotSession` okunmaz. Yazımın bloklama süresi `Tick` bütçesine eklenir: 16 bot için süre ölçülür (`PERF_SAMPLE.tick_max_us` etkisi); bütçeyi (MET-PERF-02) aşarsa dur ve soru yaz.
- Bot avantajı: bu plan **test altyapısıdır**, bot kararlarını değiştirmez; maç başladıktan sonra hiçbir yazım/ışınlama yoktur (ADR-0032 madde 1). `docs/03` §13 CLI kurallarına dokunulmaz.
- `reset: off` varsayılanı ve `[BOT] ENABLED=0` davranışı değişmez.
- Bilinen sınır `[A]`: `kSetupSettleMs` 2000 ms'den önce gelen geç NPC/party bildirimi yanlış `SETUP_FAIL NPC` üretebilir; sabit ayarlanabilir kalır ve raporlanır.

## 9. KI-DEG-05 kapanış kriteri (Claude, `/plan-dogrula` sırasında `docs/KNOWN_ISSUES.md:33` günceller)

KI-DEG-05 üç parçadır; bu plan yalnızca (a)'yı kapatır:

- (a) **Durum sıfırlama:** K5, K6, K7, K8, K9 çalışma zamanında geçmiş ve `docs/15` §6a'ya "ekipman dayanıklılığı" satırı ve "buff doğrulaması `USER_SAVED_MAGIC` kısıtı" notu eklenmiş olacak. Kayıt: KI satırında "durum sıfırlama KAPANDI (F8-05)".
- (b) **Kazanma kuralı** (`MATCH_END.result` yalnızca `completed`/`aborted`): F8-07 (K'leri geçince).
- (c) **Envanter doldurma otomasyonu:** `ScenarioReset` çanta yuvaları (K5 `stock`) ile bu planda kapanır; `tools/bot-refill.sh` elle araç olarak kalır.

KI-DEG-05 satırı ancak (a)+(b)+(c) bittiğinde `KAPANDI` olur; arada durum "KISMEN KAPANDI (a,c: F8-05)" yazılır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F8-05` — `<kısa-sha> [F8-05] …`
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
- İncelenen: `gece/2026-10-02...bot/F8-05` @ `<sha>`
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
