# UA-H — docs/03 mekanik farkı: bizim sunucu → AlphaGame 1534

Tarih: 2026-10-08 · Kapsam: salt-okunur kod ve veri karşılaştırması (ADR-0069 madde 7, faz UA-07 girdisi). Hiçbir sunucu çalıştırılmadı, hiçbir şey derlenmedi, AlphaGame dizininde hiçbir şey yürütülmedi; DB'lerde yalnız referans tabloları `SELECT` ile okundu (`MAGIC`, `MAGIC_TYPE1..5/8`, `ITEM`, `LEVEL_UP`, `COEFFICIENT`, `START_POSITION`, `ZONE_INFO`, `K_NPC`, `K_NPCPOS`, `K_OBJECTPOS`).

**Kaynaklar ve satır tabanı**

- **OURS** = `/mnt/c/dev/fdp-u1534` (`yukseltme/1534`, `438f776c`). `docs/03` bu daldaki sürümdür (v1.30; `main`'deki docs/03 MEC-R-10, MEC-MAG-26, MEC-BUF-11 içermez, eskidir).
- **ALPHA** = `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/1-Game Source/`. Yollar bu köke görelidir (`GameServer/…`, `shared/…`, `AIServer/…`). UA-01 bu kaynağı bayt bayt içe aktardı: `AttackHandler.cpp`, `MagicInstance.cpp`, `MagicProcess.cpp`, `User.cpp`, `Unit.cpp`, `ItemHandler.cpp`, `CharacterMovementHandler.cpp` dosyalarının `/mnt/c/dev/fdp-alpha-int` (`yukseltme/alpha`) kopyası `cmp` ile aynı; yani buradaki ALPHA satır numaraları docs/03'ün yeni kaynak satırları olarak doğrudan kullanılabilir.
- Bot tarafı: `BotCore/…`, `GameServer/Bot/…`, `bots/config/…` (OURS).
- DB: `FDP_kn1534` (`.\SQLEXPRESS`, bizim 1534 test verisi; `MAGIC` değerleri canlı `FDP_kn_online` ile aynı kökten) ve `FDP_alpha1534` (`.\SQL2019`).

**Etiketler:** `[D]` kodda iki taraf da okundu (satır verildi; bu raporu yazan tarafından yeniden okunan satırlar); `[D*]` satır alt incelemede okundu, bu rapor için ayrıca yeniden okunmadı; `[V]` veri sorgusuyla doğrulandı; `[A]` varsayım ya da çalışma zamanı ölçümü gerekli; `[I]` koddan çıkarım (çalışma zamanı etkisi gözlenmedi).

---

> Claude doğrulaması (2026-10-08): ilk 3 kritik iddia yeniden okundu `[V]`: `MAGIC_TYPE4.ExpPct` `FDP_alpha_game`'de 692 satırın hepsinde 0 (bizde 599 satır 100, diğerleri 1–200) ve `MagicProcess.cpp:521-523` + `MagicInstance.cpp:3074-3075` bunu büyü hasarı çarpanı olarak kullanıyor; ayakta MP yenilenmesi `User.cpp:3907` yorum satırı; `AttackHandler.cpp:10-26` `delaytime`/`distance`'ı okuyor ama denetlemiyor (sunucunun kendi `isInAttackRange` ve `CanCastRHit` denetimi duruyor). Karar maddeleri S-1..S-12 proje sahibine tek tek sorulacak (CLAUDE.md).

## 1. Özet

**Sayım (docs/03'teki 148 kimlik: 113 MEC, 20 CLI, 15 MB):**

| Sonuç | Adet | Kimlikler |
|---|---|---|
| **Değişir** (davranış ya da kuralın dayandığı veri değişir; metin yeniden yazılmalı) | **40** | MEC-CHR-04; MEC-R-04, -06, -08; MEC-MAG-09, -10, -11, -13, -14, -15, -16, -17, -18, -20, -21, -22, -24, -25; MEC-BUF-05, -09, -10, -11; MEC-T8-01; MEC-DTH-03, -06, -09, -11; MEC-MOV-01, -02, -04, -09; CLI-05, -06, -10; MB-01, -03, -09, -10, -12, -15 |
| **Aynı kalır** (yalnız kaynak satırı ALPHA'ya taşınır; bazılarına not eklenir) | **105** | geri kalan MEC-CHR, MEC-R, MEC-MAG, MEC-BUF, MEC-POT, MEC-DMG, MEC-DTH, MEC-MOV, MEC-PTY, MEC-CHT, MEC-ZON, MEC-T3-01, MEC-T8-02..04, MEC-AOE-01, 17 CLI, 9 MB (liste §2.2) |
| **Doğrulanamaz / yeniden ölçülmeli** | **3** | MEC-CHR-03 (AlphaGame DB prosedürü ve Lua okunmadı), MEC-MAG-26 (mantık aynı, saniye sınırı kayması: yeniden ölçüm), MEC-ZON-04 (kule saldırılamazlığı aynı; AIServer kule davranışı ve verisi değişti, AIServer karşılaştırılmadı) |

Ek olarak docs/03'te kimliği olmayan **24 yeni AlphaGame davranışı** (§3) ve botun ayarını doğrudan değiştiren **veri farkları** (§4) var. AlphaGame kodu ile AlphaGame verisi birlikte değerlendirildi; ADR-0069 madde 4 gereği `MAGIC`/`MAGIC_TYPE*` AlphaGame'inkidir.

**Bot için en önemli değişiklikler (önem sırasıyla):**

1. **Skill menzilleri yaklaşık 2,2 kat küçülür** `[V]`: mage 56 → 25, 78 → 35, burst 90 → 25, 70. seviye 45 → 20; priest heal/cure/buff/debuff 56 → 25, Greatness 101 → 45; diriltme 11 → 5 m. Guard'lar canlı veriyi okur, ama elle kodlanmış mesafeler (`BrainParams.h:78-79` mage bandı 30-45 m, `PriestCure.h:36` 56, `PriestBuffBind.h:18`/`DebuffBind.h:29` 52, `Survival.h:33` 56, `PriestRes.h:30` 11) eski menzile göre: mage menzil dışında park eder, priest yaklaşmaz.
2. **Yavaşlatma mekaniği**: AlphaGame verisinde yavaşlatmalar `BuffType 6 → 40`; tutma zarı yeni (`SuccessRate 100` olan leg cutting/Scream/Freeze oyuncuya **her zaman** tutar, buz büyüleri ~%25 / %4); direnilen atış `data[1] = 0` ile yayınlanır (ALPHA `GameServer/MagicInstance.cpp:2081-2151`) `[D]` `[V]`. Bot kendi yavaşlatılmasını yalnız `BuffType 6` ile tanır (`BotCore/WarriorPressure.h:161-168`): AlphaGame'de yavaşlatılan bot **sprintle kaçmaya devam eder** (adalet hatası, §5 F-9).
3. **Ayakta MP yenilenmesi yok** (ALPHA `GameServer/User.cpp:3907` yorum satırı; bizde L80'de ~40 MP / 6 sn) `[D]`; yalnız otururken. Botun MP ekonomisi ve "savaş öncesi %90 MP" beklemesi (`BotCore/RoamPrep.h:92-98`) buna göre değil.
4. **MB-01 kapanır**: AlphaGame `MAGIC.UseItem` NPC potlarında dolu `[V]`, 720 HP / 1920 MP potları tüketilir. Botlar bunlardan birer adet taşıyor (`db/004_bot_inventory.sql:158-165`, `db/010_bot_crowd.sql:217-230`); taşıma kapasitesi de yarıya iner (ALPHA `GameServer/User.cpp:2460`).
5. **AlphaGame `MAGIC_TYPE4.ExpPct` 692 satırın hepsinde 0** `[V]`: Elysian Web `m_bMagicDamageReduction = 0` ⇒ büyü hasarı ×0 (**tam büyü bağışıklığı**, ALPHA `GameServer/MagicProcess.cpp:521-523`, `MagicInstance.cpp:3074-3075`); Mana Shield/Outrage mana emmesi 0 `[D]`, çalışma zamanı `[A]`. Büyük olasılıkla veri kusuru (§6 S-3).
6. **Skill yankısı sözleşmesi bozulur**: leg cutting, Scream, Shock Stun Type1 yankısı gönderilmez (ALPHA `MagicInstance.cpp:1240-1241`); ölü hedefe Type1 ve tek hedefli Type8 yankısız `false` (`:1184-1188`, `:2587-2591`); AC debuff'larında döngü sonrası ek yankı (`:2172-2177`); grup heal çağıranı kendiliğinden eklemez (`:1410-1431`) `[D]`. Botun `effected/missed/no_result` eşlemesi (`GameServer/Bot/ActionExecutor.h:318-335`) ve testleri yeniden yazılmalı.
7. **R'de istemci `delaytime`/`distance` denetimi kaldırıldı** (MEC-R-04; ALPHA `GameServer/AttackHandler.cpp:10-26`) `[D]`: sunucu R hızını yalnız "saniyede bir" ile sınırlar; bot guard'ı (CLI-01) kalmalı.
8. **Hareket denetimleri gevşedi**: `WIZ_MOVE` hız alanı herkes için 90 (`User.cpp:3413`), speedhack mesafe toleransı +15 ve geri ışınlama ancak 3. ardışık ihlalde, 10.'da kopma (`User.cpp:4146-4181`) `[D]`.
9. **Bekleme/cast süreleri** `[V]`: Outrage, restoration, Exceed Break, Absolute power 600; Shock Stun 300; Elysian Web, Curse Refraction 1800; Fire/Ice Armor 1200; **Mana Shield `CastTime 0 → 15`, recast 800** (bot anlık sayar); Outrage `BuffType 5 → 31` (bot `HasOutrage` artık hep yanlış).
10. **Hasar modeli**: AP'de ek AP ve AP yüzdesi iki kez (STR 255 savaşçıda +108 AP; ALPHA `User.cpp:2487`), Berserker +%20 saldırı (`MagicProcess.cpp:468-473` `[D*]`), savaşçı Type1 `Hit` artışları (Cleave 150 → 250 vb.), Great healing 960 → 720 `[V]`. Bot hedef/öldürme süresi sabitleri (`BrainParams.h:48` `tgtOwnDps 470`, `TargetSelect.h:212-221`) eskir.

Diğer önemli: quest kapısı verisi boş (`Etc` hep 0), mage zırhı INT şartı (174 > bot INT 160), `WIZ_TARGET_HP` ölü hedefe de cevap, hareket yayını hareket edene geri gelmez, PvP öldürmesine +10 000 EXP, kuleler 70 m'den görür.

---

## 2. Etkilenen docs/03 kuralları

### 2.1 Değişen (40) ve doğrulanamayan (3) kurallar

Sütunlar: kimlik · bizim kural (kısa) · AlphaGame davranışı (dosya:satır) · etiket · önerilen yeni docs/03 metni (kısa) · etkilenen bot bileşeni.

| Kimlik | Bizim kural | AlphaGame davranışı | Etiket | Önerilen docs/03 metni | Bot bileşeni |
|---|---|---|---|---|---|
| MEC-CHR-04 | Kuşanırken ırk/sınıf denetimi yok; seviye, rütbe, unvan, taban stat | Sınıf denetimi eklendi: `ItemClassAvailable` (`ITEM.Class` ↔ `GetClass() % 100`, 1-15 grupları) `GameServer/ItemHandler.cpp:525-570`, çağrı `:613`; stat/seviye `:574-586` aynı; ırk yok. OURS `ItemHandler.cpp:534-546`, `:568` | `[D]` | "Kuşanırken seviye aralığı, rütbe, unvan, taban stat ve `ITEM.Class` sınıf grubu denetlenir; ırk denetlenmez; denetim yalnız `WIZ_ITEM_MOVE`'da" | `db/007_bot_gear.sql`, `db/010_bot_crowd.sql` (ekipmanı doğrudan yazar), docs/04 §2; §4.6 |
| MEC-R-04 | Silahlı ve mage değilse `delaytime ≥ Delay+10`, `distance ≤ Range`; boş el `≥ 100` | **Kaldırıldı**: alanlar okunur (`AttackHandler.cpp:10`), denetim yok (`:12-26`). OURS `:22-32` | `[D]` | "Sunucu `delaytime`/`distance` alanlarını yok sayar; R aralığı yalnız istemci kuralıdır (CLI-01)" | `BotCore/BotCombat.h:28-47`, `:61-78` `CheckAttack`; `GameServer/Bot/ActionExecutor.cpp:886-931` (`FAIRNESS_REJECT` kural adı "MEC-R-04" → "CLI-01") |
| MEC-R-06 | Aynı zone, hedef canlı, blink değil, düşman | Ek: aynı event room (`Unit.cpp:862-863` `[D*]`); **canavar dönüşümündeki hedefe saldırılamaz** (`Unit.cpp:875`) | `[D]` | "+ hedef canavar dönüşümünde değil (`isMonsterTransformation`)" | `BotCore/TargetSelect.h` (dönüşmüş insan hedefi R ile boşa denenir) `[A]` |
| MEC-R-08 | Hedefte FREEZE ⇒ sessiz ret | Yalnız **oyuncu** hedefte (`AttackHandler.cpp:41-42`); skill tarafında da yalnız oyuncu (`MagicInstance.cpp:527`, §4.3 C8) | `[D]` | "Oyuncu hedefte FREEZE ⇒ sessiz ret (NPC'de yok)" | — |
| MEC-MAG-09 | Genel başarı zarı yok; `bSuccessRate` yalnız yıldırım stun görseli | `bSuccessRate` iki yerde okunur: (1) Type3, `DirectType` 0/1, oyuncu hedef, id < 400000, **yıldırım ve buz**: zar `SR ≤ rand(0,(100−SR)·10)` başarısızsa direnç ikinci şans; başarısızlıkta hedef kimlikli EFFECTING `data[1] = 0` (`MagicInstance.cpp:1501-1528`, `:1737-1741`); 12 asa/bıçak kimliğinde id+80000 Type4 debuff (`:1742-1817` `[D*]`, kimlikler `MagicInstance.h:95-121` `[D*]`); (2) Type4 hız/stun tutma zarı (`:2084-2123`, MEC-BUF-05). OURS `MagicInstance.cpp:1361-1383` | `[D]` `[V]` | Yeni metin: iki zar yolu ve olasılıklar (MEC-BUF-05 formülü) | `BotCore/Perception.h:2147-2197` (`data[1]==0` yok sayılır), `GameServer/Bot/BotSession.cpp:200-217` (`victims` fazla sayılabilir) |
| MEC-MAG-10 | `UNIXTIME` 1 sn; iş parçacığı ~saniyede bir; gerçek aralık 0-2 sn | Hâlâ tam saniye, ama iş parçacığı 1 ms'de bir döner: saniye sınırı duvar saatine ~1 ms içinde oturur (`shared/TimeThread.cpp:31-41`); `UNIXTIME2` tanımlı, kullanılmıyor. OURS `shared/TimeThread.cpp:38` `sleep(1000)` | `[D]` | "Saniye sınırı duvar saatine hizalı; 'farklı saniye' kapılarında gerçek alt aralık 0-1 sn" | `BotCombat.h:153` `kTypeGateMs 1000` (güvenli), MEC-MAG-26 `StampSecondHold` |
| MEC-MAG-11 | `sRange` önkoşulu; Type3 kurban atlaması; `isInAttackRange` | Önkoşul ve Type3 atlaması aynı (`MagicInstance.cpp:332-342`, `:441-450`, `:1476-1478`). **Yeni:** Type1 kurban başına `sRange` atlaması (`:1200-1202`: hasar yok, yankı yok, `false`, recast yazılmaz); Type4 kurban atlamasında HP_MP muafiyeti kalktı (`:1917-1919`). Veri: `Range` ~2,2× küçük (§4.2); `UseStanding` hep 0 ⇒ 70. seviye skill'lerde (KI-017) EFFECTING menzil denetimi artık işler | `[D]` `[V]` | Kural + "Type1'de de kurban başına `sRange`" + veri tablosu docs/05'e | `BotCombat.h:177-183` `CastInRange` (canlı veri); elle kodlu mesafeler: `BrainParams.h:73`, `:78-79`; `PriestCure.h:36`; `PriestBuffBind.h:18`; `DebuffBind.h:29`; `Survival.h:33`; `MageStep.h:20`; `MageCombat.h:49`; `PriestRes.h:30` |
| MEC-MAG-13 | `{3,4}`: yanıtı yalnız Type4 | `Run` aynı (`:108-132`). Buz büyüsü oyuncu hedefte zar başarısızsa Type3 de `data[1] = 0` EFFECTING yayınlar (`:1737-1741`; SR 30'da ≈ %74,7, soğuk direnci ≥ 126'da ≈ %95,7); direnilen Type4 yankısı `data[1] = 0` (`:2148-2149`); boş alan `{3,4}`'te Type4 yankısı yok (`:1892-1893`) | `[D]` | "Buz `{3,4}`'te oyuncu hedefe Type3 başarısızlık yankısı da gelebilir; son paket Type4'tür" | `BotSession.cpp:200-217` (son paket + `victims`), `ActionExecutor.cpp:1173-1203` |
| MEC-MAG-14 | `Etc != 0` ⇒ quest durum 2 (Release); 72 satır `Etc` = quest, 32 satır `UseStanding` = quest | Kod aynı (`MagicInstance.cpp:302-308`; `QuestHandler.cpp:138-150` `[D*]`). **Veri:** AlphaGame `MAGIC.Etc` ve `UseStanding` 1886 satırın hepsinde 0 `[V]` ⇒ sunucu hiçbir skill'i quest'e bağlamaz. Quest yüklemesi değişti: `QUEST_ARRAY_SIZE 3888` (1296 quest), yeni `strQuestData` (`DBAgent.cpp:391-436`, `shared/globals.h:393-394` `[D*]`); quest 51 savunma bonusu `User.cpp:2526` `[D*]` | `[D]` `[V]` | "Sunucu kapısı kodda var, AlphaGame verisinde etkisiz; skill kilidi yalnız istemci tablosunda (kolon 29)" | `BotCombat.h:591-594` `CastQuestAllowed`, `ActionExecutor.cpp:1346-1354` `quest_locked` (`sEtc`'e bakar ⇒ hiç kilitlemez); `db/003_bot_quests.sql` `strQuest` biçimi `[A]` |
| MEC-MAG-15 | Tek hedef Type4: aynı `BuffType` buff reddi, debuff yenileme, MP başarıda, yankı `{d0,bResult,d2,süre,d4,hız,d6}`, direnilen hız debuff'ı `bResult=1` | Çekirdek aynı (`MagicInstance.cpp:2011-2049`, `:2062-2065`, `:2138-2151`). Değişen: (a) direnç zarı (MEC-BUF-05), direnilen atış `data[1] = 0` (`:2148-2149`), INFLICT durumu yok; (b) `data[5]` hedefteki aynı `BuffType` kaydının hızı (`:2109-2119`); (c) `BuffType` 2/20/24'te döngü sonrası istemci `sData` ile ek `SendSkill()` (`:2172-2177`) ⇒ Malice/Torment/Defense'te son yankı `code` = çağıranın `sData[3]` (0) | `[D]` | (a)-(c) eklenir | `BotSession.cpp:200-217`, `ActionExecutor.cpp:1196-1200` (`data[1]` okunmaz ⇒ direnilen yavaşlatma `effected`), `Perception.h:2156` |
| MEC-MAG-16 | Düşman alan: hedef −1, kurban başına yayın, boş alan başarılı | Çekirdek aynı (`MagicInstance.cpp:1410-1443`, `:1853-1896`; `MagicProcess.cpp:169-174`). Değişen: buz/yıldırım oyuncu kurbanında ek `data[1]=0` yankısı (`victims` fazla sayılabilir `[I]`); gizlilik yalnız gerçek kurbandan (MEC-BUF-09); boş `{3,4}`'te Type4 yankısı yok; ağaç/fosil/mülteci/ortak/sınır anıtı NPC'leri elenir (`MagicProcess.cpp:120-139`) | `[D]` | Fark maddeleri eklenir | `ActionExecutor.cpp:1214-1215` `victims`; `bots/config/skill_mage_k_area*.spec` |
| MEC-MAG-17 | Uçan alan = MAG-12 + MAG-16 | MAG-12 aynı (`MagicInstance.cpp:80-89`, `:1093-1113`); MAG-16 farkları geçer; Thunder burst (SR 30) her oyuncu kurbanı için yankı, `data[1]=1` yalnız ≈ %25 | `[D]` | Not eklenir | `BotCombat.h:296-349` (değişmez) |
| MEC-MAG-18 | Moral 4 tek üye; Moral 6 grup; çağıran heal'de her zaman kurban | Moral 4 aynı. **Grup heal'de çağıran kendiliğinden eklenmez** (`MagicInstance.cpp:1410-1431`; OURS `:1280-1281` ekler): party'deki çağıran yalnız hedef noktasının `Radius`'u içindeyse, party'siz çağıran her zaman (`MagicProcess.cpp:149-150`). 112570/112575 (+El Morad) DoT/HoT'lu hedefi atlar (`MagicInstance.cpp:1480-1481`). Greatness üye başına `sRange` (`:1917-1919`), `Range 101 → 45` `[V]` | `[D]` `[V]` | "Çağıran yalnız `UserRegionCheck` ile kurban olur" | `GameServer/Bot/ActionExecutor.h:330-332` ("party heal always heals caster"), `bots/config/skill_priest_k.spec` (112557/112560/112656) |
| MEC-MAG-20 | Diriltme: taş hedeften, `Range 11`, MP 0, yerinde | Kod aynı (`MagicInstance.cpp:279-294`, `:960-970` `[D*]`, `:2312-2321` `[D*]`). **Veri:** `Range 11 → 5` `[V]`. Skill diriltmesinden sonra NP 0 ise bölgeden atma (`AttackHandler.cpp:250-254`) | `[D]` `[V]` | `Range 5`; NP 0 maddesi | `BotCore/PriestRes.h:30` `kPresRangeM = 11` |
| MEC-MAG-21 | Summon: ölü/başka zone/self hedefte yayın `sData[1]=0`, MP gider | Ölü ya da blink oyuncu tek hedef **döngüden önce `false`** (`MagicInstance.cpp:2587-2591`): yankı yok, MP gitmiş ⇒ bot `no_result` (bizde `effected`); case 12'de party + blink şartı (`:2692-2698`) | `[D]` | "Ölü hedefte yankı yok (`no_result`), MP düşer" | `BotCore/SummonGate.h`, `BotCombat.h:465-474`; F4-34 beklentisi |
| MEC-MAG-22 | Gate/descent | Gate aynı (Escape > 31'de ret, Gate geçer; `MagicInstance.cpp:2623-2657` `[D*]`); descent aynı party şartı (`:2767-2770` `[D*]`); ölü hedefte yankı yok (`:2587-2591`) | `[D]` | Not eklenir | — (beyin Gate/descent kullanmaz, `BrainDriver.cpp:424-434`) |
| MEC-MAG-24 | `{1,3}`/`{1,4}`: iki yankı, bot son paketi görür; ölü hedefte Type1 yankısı | `ExecuteType1` yeniden yazıldı (`MagicInstance.cpp:1144-1244`): **Type1 yankısı yok** 106520/206520/105520/205520 (leg cutting), 106802/206802 (Scream), 106820/206820 (Shock Stun) (`:1240-1241`); ölü/blink oyuncu hedef yankısız `false` (`:1184-1188`); kurban başına `sRange` (`:1200-1202`); silah aşınması (`:1234-1235`). leg cutting/Scream `SR 100` ⇒ yavaşlatma oyuncuya her zaman tutar `[V]`. KI-021 (dönüşümlü −103) doğrulanmadı `[A]` | `[D]` `[V]` | Liste ve "Shock Stun yalnız Type3 yankısı (`sData[3]` 0/−104); leg cutting/Scream yalnız Type4" | `ActionExecutor.h:318-335` (`missed` via −104 sözleşmesi), `BotCore/WarriorPressure.h:27`, `:246-253`, `:372-376` |
| MEC-MAG-25 | Elysian Web: `ReCastTime 1`, `Range 56`, büyü hasarı azaltma | Kod aynı (`MagicProcess.cpp:197-200`; `MagicInstance.cpp:1853-1896`). **Veri:** `ReCastTime 1 → 1800`, `Range 56 → 25`, `ExpPct 70 → 0` `[V]` ⇒ `m_bMagicDamageReduction = 0` (`MagicProcess.cpp:521-523`) ⇒ büyü hasarı ×0 (`MagicInstance.cpp:3074-3075`) | `[D]` `[V]`, etki `[A]` | Veri satırları + §15'e MB (bkz. S-3) | `BotCombat.h:398-413`; `TargetSelect.h` (mage hedefi Elysian altında) |
| MEC-BUF-05 | Hız/stun debuff'ı oyuncuya: direnç < 125 ⇒ ≈ %78,7 direnme, ≥ 125 ⇒ %100 (SR'den bağımsız) | Yalnız oyuncu, Moral 7/10, `BuffType` 6/40/47 (`MagicInstance.cpp:2084-2123`). Tutma `P = p_s + (1−p_s)·p_r`; `p_s = SR/((100−SR)·10+1)` (SR ≥ 100 ⇒ 1); `p_r = 31/141 ≈ %22` (R < 125, R 110'a zorlanır), `1/126` (R = 125), 0 (R ≥ 126); R: `BuffType 40` ⇒ ColdR, diğer ⇒ LightningR (`:2097`). **SR 30 ⇒ %25,3 / %4,3; SR 50 ⇒ %29,8 / %10,0; SR 100 ⇒ %100.** Bizde %21,3 / %0 (OURS `:1821-1848`) | `[D]` `[V]` (SR verisi) | Formül ve tablo | `BotCore/WarriorPressure.h:27` (`kWarLegCuttingHoldMs`), `MageCombat.h:53` (`kMagSlowHoldMs`), `:428-429` (yavaşlatma = `BuffType 6`), `:1423-1424` |
| MEC-BUF-09 | Type4 alan, 3×3'teki tüm düşman ulus oyuncularının gizliliğini kaldırır | Yalnız `UserRegionCheck` ve `sRange`'i geçen **gerçek kurbandan**, PK zone'da farklı ulus ise (`MagicInstance.cpp:1921-1950`); OURS `:1650-1657` | `[D]` | Metin değişir | yok (bot gizlilik kullanmaz; `TargetSelect.h:272` yalnız görünmezi süzer) |
| MEC-BUF-10 | Başkalarının Type4 etkisi yayından | Düzen aynı; ekler: AC/20/24 ek yankısı (`:2172-2177`), direnilen yavaşlatma `data[1]=0` (bot zaten yok sayar), 8 kimlikte Type1 yankısı yok, buz/yıldırım Type3 `data[1]=0` yankısı, asa yankıları **skill id + 80000** ile (`:1742-1817` `[D*]`), Mage Armor kalkınca `MAGIC_DURATION_EXPIRED` hedefe gider (`MagicProcess.cpp:1071-1076` `[D*]`) | `[D]` | Fark maddeleri | `BotCore/Perception.h:2147-2197`, `:2387-2398` (id+80000 tanınmaz `[A]`) |
| MEC-BUF-11 | Süreli scroll'lar | Kod aynı (`MagicInstance.cpp:1850`, `:2033-2049`, `:2179`); `isLockableScroll` + 48, 171 (`User.h:433`). **Veri:** 500034/500035 `ReCastTime 255 → 0`; 800076000/800078000 `ReqLevel 1 → 62` `[V]` | `[D]` `[V]` | "Attack+/Speed+ recast 0" | `BotCore/ScrollUse.h:70-94`, `:493-515` (tam şekil eşleşmesi: Attack+/Speed+ `unsupported_item` olur) |
| MEC-T8-01 | Summon party'si yalnız moral alanıyla zorlanır | case 12 içinde de party + blink şartı (`MagicInstance.cpp:2692-2698`) ⇒ her WarpType 12 skill'inde | `[D]` | Metin değişir | — |
| MEC-DTH-03 | NP +64/−50; party 22; anıt +5 | NP aynı (ini `GameServerDlg.cpp:346-347`, party bölüşümü `User.cpp:3682-3731`, anıt `:768-769` `[D*]`). **Yeni:** NP kazanan her öldürmede +10 000 EXP ve Meat Dumpling (`User.cpp:857-866`; `Define.h:209`, `:243`), party'de NP alan her canlı üyeye (`:3698-3699` `[D*]`); eşya 508216000 AlphaGame `ITEM`'de yok ⇒ `GiveItem` sessizce `false` (`ItemHandler.cpp:463-465`) `[V]` | `[D]` `[V]` | "+10 000 EXP / öldürme (PK zone)" | çanta/ağırlık; `GameServer/Bot/BrainDriver.cpp:2386` (skill listesi bir kez kurulur) |
| MEC-DTH-06 | START_POSITION 71: Karus (1380,1090), El Morad (630,920) + rand(0..10) | Kod yolu aynı (`AttackHandler.cpp:155-185` `[D*]`); bind nesnesi varsa bind koordinatı (`:149-154`). **Veri:** Karus (1375,1098), El Morad (622,898), `bRangeX/Z 5/5`; loader sütun sırası farklı ama adla okur (`shared/database/StartPositionSet.h:10-26`) `[V]` | `[D]` `[V]` | Yeni koordinat + rand(0..5); bizim satırdaki `bRange 0` notu (§4.7) | `BotCore/RoamRouteData.h:195-219` |
| MEC-DTH-09 | NP 0 iken Ronark respawn ⇒ ana zone | Skill diriltmesinden sonra da (`AttackHandler.cpp:250-254`); PK zone'da NP taşması `Home()` (`User.cpp:712-718` `[D*]`, ölüyken etkisiz) | `[D]` | "Her diriltme yolunda" | `ActionExecutor.cpp:2774-2780` `no_np` |
| MEC-DTH-11 | Rival 300 sn, rival öldürme +150 | Süre/bonus aynı; **bonus ölenden düşülmez** (`User.cpp:3382-3387`; OURS `:3066` `loyalty_target -= bonusNP`) | `[D]` | "Ölen −50 (−200 değil)" | — |
| MEC-MOV-01 | `WIZ_MOVE` düzeni | Düzen aynı (`CharacterMovementHandler.cpp:15` `[D*]`); yayın **hareket edene geri gönderilmez** (`:49` `SendToRegion(&result, this, room)`; OURS `:48`) | `[D]` | "Yayın hareket eden hariç" | `GameServer/Bot/BotSession.cpp:498-507` (kendi kaydı yayından güncellenmez) `[A]` |
| MEC-MOV-02 | W/M/P hız alanı > 67 ⇒ kopma | Sınıf sınırları hesaplanır ama `nMaxSpeed = 90;` hepsini ezer (`User.cpp:3401-3413`); > 90 ⇒ kopma + duyuru (`:3415-3421`) | `[D]` | "Herkes için 90" | `BotCore/BotMotion.h:31-38` `ServerSpeedLimit` (67: güvenli ama eski) |
| MEC-MOV-04 | `WIZ_SPEEDHACK_CHECK` ile tek mesafe denetimi, ilk ihlalde geri ışınlama | Tolerans +15, 3. ardışık ihlalde geri ışınlama, 10.'da kopma, iyi denetimde sayaç sıfır (`User.cpp:4146-4181`) | `[D]` | Metin değişir | `ActionExecutor.cpp:4086-4094` (ilk iki ihlal "geçti") |
| MEC-MOV-09 | Eşik `√(100·(sınır+10))` = 74,16 / 87,75 / 100 m | Sınıf sınırı burada 45/67/90 kalır (`:4151-4157`); eşik `√(100·(sınır+15))` = **77,46 / 90,55 / 102,47 m**; ihlalde `m_LastX/Z` güncellenmez | `[D]` | Yeni eşikler | `BotCore/BotMotion.h:126-129` `SpeedCheckWarpDistance` |
| CLI-05 | Hız 45/67, paket 1,5 sn, yavaşlatma yüzdesi bota uygulanır | Kural aynı kalır; değişen dayanak: yavaşlatmalar `BuffType 40` (§4.4), sunucu sınırı 90, kendi hareket yayını gelmez | `[V]` | "Yavaşlatma = `BuffType` 6, 40 ve stun 47" | `BotCore/WarriorPressure.h:161-168` `HasSlowDebuff` (yalnız 6), `TargetSelect.h:25`; yalnız `Survival.h:305-310` 40'ı tanır |
| CLI-06 | Envanterde pot; MB-01 potlarında sayı azalmaz; ortak 2,5 sn | MB-01 kalkar (§4.5): her pot tüketilir | `[V]` | "Tüm potlar tüketilir" | `BotCombat.h:666-673`; `GameServer/Bot/BrainDriver.cpp:564` (720/1920 = "tüketilmez"); `BotCore/RoamPrepFill.h:49` (9999); `db/004`, `db/010` stok |
| CLI-10 | Hedef HP; ölü hedefe cevap yok | İstek aynı; `SendTargetHP` ölü oyuncuyu süzmez (`User.cpp:2607-2638`, `:2627`); öldürücü vuruşta da saldırana `WIZ_TARGET_HP` (`:2257-2259`, `OnDeath`'ten önce) | `[D]` | "Ölü hedefe `hp 0` cevabı gelir" | `ActionExecutor.h:396` (`no_result` beklentisi) |
| MB-01 | 720 HP/1920 MP NPC potları `UseItem=0` | AlphaGame verisinde yok `[V]` | `[V]` | "Kapandı (AlphaGame verisi)" | §4.5 |
| MB-03 | Mage Armor tam hasarı yansıtır | `ReflectDamage` yalnız saldıran **oyuncu** ise; giyenden saldırana proc skill (Karus 190573/190673/190773, El Morad 2905xx..) `bIsRunProc` ile, sonra Mage Armor kalkar (`MagicInstance.cpp:3358-3404`); proc verisi `DirectType 15`, `−1400/−1120` + DoT `−700/−490` 20 sn, Ice Armor +10 sn `SPEED2` `[V]`; proc giyenin önkoşullarından geçer `[I]` | `[D]` `[V]` | Yeni metin | `BotCore/TargetSelect.h:453-457` (Mage Armor riski +0,4) |
| MB-09 | Direnilen yavaşlatma haritada kalır | Harita kaydı hâlâ zardan önce eklenir (`MagicInstance.cpp:2067-2079`); yayın artık `data[1]=0` | `[D]` | Not | — |
| MB-10 | Type7 başarıda `false` | Döngü sonunda `true` (`MagicInstance.cpp:2550` `[D*]`) | `[D*]` | "Kapandı" | kapsam dışı sınıflar |
| MB-12 | `m_bMaxWeightAmount` başlatılmıyor | Hâlâ başlatılmıyor; formül **yarıya** indi: `(((STR+L)·50+bonus)·çarpan)/2` (`User.cpp:2460`; OURS `:2400`) | `[D]` | Formül | pot taşıma kapasitesi (§6 S-8) |
| MB-15 | 70. seviye quest kimlikleri `UseStanding`'de | AlphaGame verisinde `Etc` ve `UseStanding` hep 0 `[V]` | `[V]` | "AlphaGame verisinde yok" | `BotCore/MageCombat.h:198-199` (`useStanding != 0` ⇒ bekle; artık tetiklenmez) |
| MEC-CHR-03 | Oyun içi sınıf yükseltme yok; `LOAD_USER_DATA` girişte yükseltir | AlphaGame prosedürleri ve Lua okunmadı (`C` raporu §2.4: prosedür uyumsuzlukları) | `[A]` | UA-05/UA-06 sonrası yeniden doğrula | `db/002`..`db/011` uyarlaması |
| MEC-MAG-26 | Pot tip-3 damgası; aynı saniyedeki ikinci Type3 FLYING/EFFECTING −103 | Mantık aynı (`MagicInstance.cpp:392-407`, `:116-125` `[D*]`, `:479-517`); yalnız saniye sınırı duvar saatine hizalandı (MEC-MAG-10) ⇒ `live-070426` sayıları (pot < 1000 ms ⇒ FAIL) geçerliliğini yitirebilir | `[D]`, ölçüm `[A]` | "Yeniden ölçülecek" | `ActionExecutor.cpp:1095-1125` `StampSecondHold`, `BotCombat.h:154` |
| MEC-ZON-04 | Kuleler oyuncuca saldırılamaz; AIServer davranışı `[A]` | `NPC_GUARD_TOWER1/2` saldırılamaz aynı (`Unit.cpp:914-921` `[D*]`); kendi ulus PvP anıtı artık saldırılabilir (`:909-913` `[D*]`). **Veri:** kule `bySearchRange 35 → 70`, `byAttackRange 20 → 30` (iç kule), `sAttackDelay 1000 → 600` `[V]`; AIServer `Npc.cpp` ~434 satır farklı, karşılaştırılmadı | `[D]` `[V]` `[A]` | "Kule menzilleri AlphaGame verisi; AIServer davranışı doğrulanmalı" | `BotCore/RoamTowers.h:21-41` (`kRoamTowerSearchM 35`, `kRoamMageTowerZoneM 40`) |

### 2.2 Aynı kalan kurallar (105)

Davranış aynı; docs/03'te yalnız kaynak satırı ALPHA'ya taşınır. Not gerekenler ayrıca yazıldı.

| Kimlikler | ALPHA kaynağı | Not |
|---|---|---|
| MEC-CHR-01, -02, -05, -07, -09, -11..-16 | `MagicInstance.cpp:182-186`, `User.cpp:3527`, `:2161-2162`, `shared/globals.h:392`, `MagicInstance.cpp:2964-2970`, `:1484-1489`, `User.cpp:2579-2588`, `Define.h:23`, `User.cpp:2202-2210` `[D*]` | — |
| MEC-CHR-06, -08, -10 | `User.cpp:2053-2072`, `:3525-3533`; `CharacterSelectionHandler.cpp:187`, `:203-207` `[D*]` | **Oyuncu seviye sınırı `GAMEMAXLEVEL` ini anahtarı** (`GameServerDlg.cpp:323`, varsayılan ve paket ini 80); `MAX_LEVEL 83` (`Define.h:22`) yalnız zone seviye aralığı. Master ağacı sınırı `m_byMaxLevel − 60`; `GetRace()==0` girişte koparır |
| MEC-R-01, -02, -03, -05, -07, -09 | `AttackHandler.cpp:12-18`, `:20-27`; `Unit.cpp:932-953`; `User.h:27` | — |
| MEC-R-10 | `AttackHandler.cpp:21`, `:108-110`; `Unit.cpp:720-806` `[D*]` | **İki ağaçta da düzeltme:** `isInAttackRange`/`CanAttack`/`isAttackable`/`CanCastRHit` reddi sessiz değildir, `bResult = 0` ile yayınlanır (OURS `AttackHandler.cpp:36-43`, `:95-97`); yani aynı saniyedeki ikinci R da `0` yankısı üretir. Yalnız MEC-R-01/-02, (bizde) -04, -08 ve tapınak/hapis dönüşleri sessiz |
| MEC-MAG-01..08, -12, -19, -23 | `MagicInstance.cpp:36-146`, `:452-463`, `:479-517`, `:392-407`, `:389-390`, `:1112-1113`, `:2062-2065`, `:80-89`, `:2249-2338` `[D*]`, `:279-294` | MAG-01: cast süresi hâlâ uygulanmıyor (`bCastTime` yalnız yüklenir). MAG-05: pot verisi değişti (MB-01). MAG-07: AlphaGame'de `UseStanding=1` satırı yok, kural tetiklenmez |
| MEC-T3-01, MEC-AOE-01 | `MagicInstance.cpp:557-572` `[D*]`; `:1414-1431`, `:1857-1874` | — |
| MEC-BUF-01..04, -06..-08 | `Unit.h:31`, `MagicInstance.cpp:1977-2049`, `MagicProcess.cpp:586-597`, `User.cpp:3997-4014` `[D*]`, `AttackHandler.cpp:245-246` | BUF-06: SPEED2 oyuncuda `m_bSpeedAmount = 100` yazar, sunucu okumaz |
| MEC-T8-02..04 | `MagicProcess.cpp:155-160`; `MagicInstance.cpp:2587-2591`, `:2625-2633` `[D*]` | T8-03: ölü hedefte artık yankı da yok |
| MEC-POT-01..05 | `MagicProcess.cpp:17-50`; `MagicInstance.cpp:452-463`, `:1031-1043` `[D*]` | Veri: tüm potlar tüketilir (MB-01) |
| MEC-DMG-01..07 | `Unit.cpp:217-395`, `:720-806`, `MagicInstance.cpp:2935-3090`, `Define.h:24`, `User.cpp:2167-2210` `[D*]` | Formüller aynı; AP girdisi değişti (§3 N-01), Berserker (N-02) |
| MEC-DTH-01, -02, -04, -05, -07, -08, -10 | `User.cpp:5500-5501`, `:5776-5784`, `:4805-4847`, `AttackHandler.cpp:121-125`, `:243-248`, `User.cpp:5269-5279`, `:4270-4292` `[D*]` | DTH-08: diğer PvP zone'larda artık 10 sn blink; DTH-10: bind nesnesi varsa önce oraya |
| MEC-MOV-03, -05..-08 | `CharacterMovementHandler.cpp:22`, `:35-43`, `User.cpp:339-342`, `CharacterMovementHandler.cpp:561-572`, `:400-413` `[D*]` | MOV-08: zone 71 aralığı 35-83, oyuncu sınırı ini 80 |
| MEC-PTY-01..05 | `shared/database/structs.h:378`, `PartyHandler.cpp:73-86`, `:27-50`, `User.cpp:2303-2311` `[D*]` | Event room eşitliği eklendi (oda 0'da etkisiz) |
| MEC-CHT-01..03 | `ChatHandler.h` (aynı), `ChatHandler.cpp:112-120`, `:199-205` `[D*]` | — |
| MEC-ZON-01..03 | `Unit.cpp:1289`, `:1307-1309`, `:1433-1494`; `GameServerDlg.cpp:2422-2428` `[D*]` | ZON-02: zone 73 için yeni güvenli kutu |
| CLI-01..04, -07..-09, -11..-20 | — (istemci kuralları) | CLI-01 dayanağından MEC-R-04 düşer; CLI-04 veri (recast) değişti (§4.3); CLI-09 tetiklenmez; CLI-12 sunucu eşikleri MEC-MOV-09; CLI-13 artık tek MP yenilenme yolu; CLI-19/20: `RequestNpcIn` kimlik süzgeci 10000-30000 (`User.cpp:1483` `[D*]`), `WIZ_REGIONCHANGE` üç parçalı (`GameServerDlg.cpp:1520-1544` `[D*]`, bizim 1534 profilimizle aynı). Tüm CLI ölçümleri 1453 istemcisiyle yapıldı; 1534 istemcisiyle yeniden ölçüm ayrı iştir (T-UPG) |
| MB-02, -04..-08, -11, -13, -14 | `MagicInstance.cpp:459`; `User.cpp:2490-2496`, `Unit.cpp:249-250`, `:631-632`, `:669-673`, `User.cpp:2556-2564` `[D*]`; `AIServer/MAP.cpp:131-134`; `AttackHandler.cpp:127-134` `[D*]`; `CharacterMovementHandler.cpp:480-481` | Hatalar AlphaGame'de de duruyor |

---

## 3. docs/03'te kimliği olmayan, PK botlarını etkileyen AlphaGame davranışları

Önerilen kimlikler geçicidir (docs/21 §5: kimlikler değişmez; yeni kimlik verirken mevcut seriye eklenmeli).

| Öneri | Davranış | ALPHA kaynağı | Etiket | Önerilen docs/03 metni | Bot bileşeni |
|---|---|---|---|---|---|
| N-01 (MEC-DMG-08) | AP'de `(H + a)·(100+b)/100` sınıf dalından sonra **bir kez daha** uygulanır; `a = 3 + max(0, STR−150)` ⇒ STR 255 savaşçı +108 AP (b = 0), priest/mage +3 | `User.cpp:2471-2487` (`:2487`); OURS `:2412-2426` | `[D]` | "AP = ((H+a)(100+b)/100 + a)(100+b)/100" | `BrainParams.h:48` `tgtOwnDps`, `TargetSelect.h:212-221`, `SoloEval.h:35-42`, `PotPolicy.h:35` |
| N-02 (MEC-BUF-12) | Berserker (`BuffType 18`) +%20 fiziksel saldırı (`m_bAttackAmount += Attack−100`); yalnız 106775/206775 (`SkillLevel 75`, ağaç 7: bot yapılarında açılmaz, insan rakipte olabilir) | `MagicProcess.cpp:468-473`, `:839-845` `[D*]` | `[D*]` `[V]` | Ek A satırı | hedef riski |
| N-03 (MEC-REG-01) | Ayakta MP yenilenmesi yok; oturunca HP `L(1+L/30)+3`, MP `(maxMP·5/(L+29)+3)·%` (5 sn'den uzun aralıkla) | `User.cpp:3875-3924` (`:3907`); OURS `:3584` | `[D]` | Yeni kural | `BotCore/RoamPrep.h:92-98`, `:364-369`; `PotPolicy.h:477-484`; `Survival.h:399-458`, `:735-748` |
| N-04 (MEC-MAG-27) | Bilinmeyen skill kimliği gönderen oyuncu koparılır, sunucuya duyurulur | `MagicProcess.cpp:25-40`; OURS `:30` (`< 0` hiç doğru değil) | `[D]` | Yeni kural | yalnız bellekteki `MAGIC`'ten gönderilir (`ActionExecutor.cpp:1266-1272`, `:1490-1497`); test betikleri dikkat |
| N-05 (MEC-DTH-12) | PK zone'da NP kazandıran öldürmede +10 000 EXP (çarpansız); party'de NP alan her üye | `User.cpp:857-866`, `Define.h:209` | `[D]` | Yeni kural | seviye sınırı ini 80 ⇒ etki küçük |
| N-06 (MEC-CHR-17) | Taşıma kapasitesi yarı | `User.cpp:2460` | `[D]` | MB-12 ile birlikte | pot stoğu |
| N-07 (MEC-MAG-28) | Asa/bıçak id+80000: frozen blade 110642, Ice Staff 110672 (+El Morad, +109642) oyuncuya **her zaman** (SR 100) 2-3 sn `SPEED2` (hız 50/34) ekler, yankı skill id + 80000; yıldırım eşlerinin `MAGIC_TYPE4` satırı yok ⇒ hasardan sonra yankısız `false` | `MagicInstance.cpp:1501-1528`, `:1742-1817` `[D*]`; veri `[V]` | `[D]` `[V]` | Yeni kural | MI mage (`[6]=80`) bu skill'leri açar; bot listelerinde yok (`MageCombat.h:71-87`) |
| N-08 (MEC-MAG-29) | Type3 buz/yıldırım (DirectType 0/1, oyuncu): zar başarısızsa ek `data[1]=0` yankısı | `MagicInstance.cpp:1501-1528`, `:1737-1741` | `[D]` | MAG-09/13'e bağlı | `BotSession.cpp:200-217` |
| N-09 | 112570/112575 (+El Morad) DoT ya da HoT'u olan oyuncuyu atlar | `MagicInstance.cpp:1480-1481` | `[D]` | MAG-18'e not | priest heal seçimi `[A]` |
| N-10 | Kaynağı oyunda olmayan DoT/HoT ne işler ne biter (yuva dolu, `m_bType3Flag` açık kalır ⇒ MEC-T3-01 HoT reddi, N-09 atlaması) | `User.cpp:3944-3950` | `[D]` `[I]` | Yeni MB | `PriestHeal.h:554`, `:903` (HoT kararları) |
| N-11 | Skill vuruşunda silah aşınması: Type1/2 `ItemWoreOut(ATTACK, dmg)`, mage Type3 kurban başına | `MagicInstance.cpp:1234-1235`, `:1380-1381` `[D*]`, `:1819-1820` `[D*]` | `[D]` | Yeni kural | uzun koşuda dayanıklılık `[A]` |
| N-12 | Vampiric Fire (DirectType 16) çağıranı tam miktar iyileştirir (bizde yarı); 110574 `FirstDamage −3000`, `ReCastTime 1 → 600` | `MagicInstance.cpp:1635-1640` | `[D]` `[V]` | §5.2'ye | MF mage açar (`[5]=80`), bot listesinde yok |
| N-13 | `AG_HEAL_MAGIC`: başkasına heal AIServer'a bildirilir, NPC aggro'su healer'a dönebilir | `MagicInstance.cpp:1822-1828` `[D*]` | `[D*]` `[I]` | AIServer notu | canavar yakınında priest `[A]` |
| N-14 | 300000-399999 oyuncuya kapalı; 490024 yalnız GM; ticaret/pazar açıkken cast yok; ağaç/fosil vb. NPC hedef reddi | `MagicInstance.cpp:350-366`, `:198-229` `[D*]` | `[D]` | §4.3'e | — |
| N-15 | `ExecuteType6`: dönüşüm denetimleri artık iptal etmiyor (`//return false`); doğrudan gönderilen Type6 kimliği Ronark'ta dönüştürebilir; canavar dönüşümündeki oyuncu R'ye bağışık (skill'e değil) | `MagicInstance.cpp:2355-2376` (`:2375`), `Unit.cpp:875` | `[D]` `[I]` | Yeni MB | adalet §5 F-13 |
| N-16 | Type9 (gizlilik): etkin durumu yeniden atma sessiz `false`; `bStateChange >= 7 \|\| <= 8` hep doğru | `MagicInstance.cpp:2840-2841`, `:2921-2931` `[D*]` | `[D*]` | MB adayı | rakip rogue `[A]` |
| N-17 | Type1 alan yolu (hedef −1); AlphaGame verisinde oyuncu Type1 alan skill'i yok ⇒ uykuda | `MagicInstance.cpp:1158-1181` | `[D]` `[V]` | Not | — |
| N-18 | `WIZ_MOVE` hareket edene yansımaz | `CharacterMovementHandler.cpp:49` | `[D]` | MOV-01 | `BotSession.cpp:498-507` |
| N-19 | Hedef HP: ölü hedefe ve öldürücü vuruşta cevap | `User.cpp:2627`, `:2257-2259` | `[D]` | CLI-10 / §16 | `ActionExecutor.h:396` |
| N-20 | Oyuncu seviye sınırı `GAMEMAXLEVEL` (80); `ExpChange` bölge seviye aralığı dışındakini atar (zone 71: 35-83) | `GameServerDlg.cpp:323`; `User.cpp:1980-1983` `[D*]` | `[D]` | §1.3 | — |
| N-21 | Boşta kalma zaman aşımı 30 sn, bot istisnası yok (bizde `GameServerDlg.cpp:768-770` bot atlaması) | `GameServerDlg.cpp:832-859` `[D*]` | `[D*]` | Kanca (UA-04) | süreç içi botlar koparılabilir |
| N-22 | `WIZ_GENIE` (oto-av): hareket, R ve skill paketlerini aynı işleyicilere geçirir (`m_GenieTime == 0` olsa da) | `GenieHandler.cpp:80-99` | `[D]` | Not | adalet §5 F-14 |
| N-23 | Kendi ulus PvP anıtı `isAttackable`; Bifrost anıtı yalnız Bifrost evresinde düşman | `Unit.cpp:909-913`, `:1207` `[D*]` | `[D*]` | ZON notu | — |
| N-24 | Paket düzeni (mekanik değil): `WIZ_EXP_CHANGE` `u8 1, u32`; ağırlık alanları `u16` (bizim 1534 profilimiz `u32`) | `User.cpp:1987`, `:2089`, `:2128` `[D*]` | `[D*]` | ADR-0069 madde 3 | UA-03 |

Yan bulgular (mekanik dışı): OURS `GameServer/User.cpp:522-527` `case WIZ_LOGOSSHOUT:` `break`'siz `default: return false`'a düşer (Release'de gönderen kopar; AlphaGame `:486`'da `break` var) `[D*]`. Event room denetimleri (`Unit.cpp:862`, `:1197`, `:1289`; `User.cpp:1450`) oda 0'da etkisiz.

---

## 4. Veri tarafı değişiklikleri (bot ayarını değiştirenler)

Kaynak: `FDP_kn1534` (`.\SQLEXPRESS`, bizim 1534 test DB'si) ile `FDP_alpha1534` (`.\SQL2019`, AlphaGame DB'si); yalnız `MAGIC`, `MAGIC_TYPE1..5/8`, `ITEM`, `LEVEL_UP`, `COEFFICIENT`, `START_POSITION`, `ZONE_INFO`, `K_NPC`, `K_NPCPOS`, `K_OBJECTPOS` okundu (2026-10-08, hepsi `[V]`). Sunucu kodu her iki tarafta bu tabloları **sütun adıyla** yükler (`shared/database/MagicTableSet.h:10`, `MagicType4Set.h:10` iki ağaçta aynı; `StartPositionSet.h` sırası farklı ama adla okur), yani DB değeri doğrudan kurala geçer. Bot da skill/eşya verisini sunucunun bellekteki tablolarından okur (`GameServer/Bot/ActionExecutor.cpp:1266`, `:1490`, `:1569-1629`), yani veri değişimi guard'lara kendiliğinden gelir; elle kodlanmış kopyalar ayrıca listelenmiştir.

### 4.1 Bot skill kümesi ve özet sayılar `[V]`

- Bot skill kümesi = bot sınıflarının (106/110/112, 206/210/212) son yapılarındaki ağaç puanlarıyla (`db/007_bot_gear.sql:229-243`, `db/010_bot_crowd.sql:124-140`: WP `[5]80 [7]52 [8]10`, WG `[5]80 [6]60 [8]2`, MF `[5]80 [6]50 [8]12`, MI `[5]50 [6]80 [8]12`, PHD `[5]60 [7]70 [8]12`, PHB `[5]54 [6]68 [8]20`, PHD2 `[5]60 [7]75 [8]7`) açılabilen `MAGIC` satırları: **394 satır** (iki ulus). Hepsi iki DB'de de var (eksik satır yok; AlphaGame'in bilinmeyen-skill kopması bu küme için tetiklenmez).
- Bu 394 satırda fark eden sütunlar: `Range` 258, `ReCastTime` 144 (112'si `1 → 0`), `Etc` 20, `UseStanding` 12, `FlyingEffect` 22 (hep `x → 1`, sıfırdan farklılık korunur), `CastTime` 2, `SelfEffect` 394 (sunucu okumaz). `Msp`, `SkillLevel`, `Skill`, `Moral`, `Type1/2`, `UseItem`, `BeforeAction`, `SuccessRate` bu kümede **aynı**.
- `bots/config/skill_*.txt` kimlikleri (50 Karus + El Morad karşılıkları) bu kümenin alt kümesidir; aşağıdaki tablolar bot kodunun elle kodladığı kimlikleri de kapsar (`BotCore/WarriorPressure.h:58-68`, `MageCombat.h:71-87`, `PriestHeal.h:60-64`, `PriestBuff.h:33-38`, `PriestCure.h:44-45`, `PriestRes.h:77-79`, `DebuffCall.h:39-46`).

### 4.2 Menzil (`MAGIC.Range`, metre; MEC-MAG-11) `[V]`

| Bizde → AlphaGame | Adet (Karus) | Bot skill'leri |
|---|---|---|
| 56 → 25 | 89 | Pillar of fire 110551, Supernova 110560, Fire Impact 110557, Hell fire 110539, Inferno 110545, Ice comet 110651, Frost nova 110660, Blizzard 110645, Ice Impact 110657, Absolute power 110802; tüm priest heal'leri 112527/536/545/548/554/557/560, cure 112525/112535, buff'lar 112645/654/657/660, 110548/110648, Elysian Web 112825, debuff'lar 112703/724/736/745/757/760; Gate 110015 |
| 78 → 35 | 7 | Fire ball 110515, Fire spear 110527, Ice arrow 110615, Ice orb 110627, Fire blast 110535, Ice blast 110635, Igzination 110575 |
| 90 → 25 | 2 | Fire burst 110533, Ice burst 110633 (El Morad 212703/212745/212760 de 90 → 25) |
| 45 → 20 | 7 | incineration 110570, meteor Fall 110571, Prismatic 110670, ice storm 110671, subside 112770, Curse Refraction 112820, 106725 |
| 101 → 45 | 1 | Greatness 112656 |
| 112 → 25 | 1 | Superior Parasite 112771 |
| 11 → 5 | 11 | Burn 110503, fire/frozen blade 110542/110642, Resurrection 112733/112742/112754 (MEC-MAG-20: diriltme menzili 11 → 5 m) |
| 22 → 5 | 2 | Fire Staff 110572, Ice Staff 110672 |
| 67 → 30, 22 → 10, 33 → 15 | 2 / 1 / 3 | Binding 106630, sacrifice 106660; provoke 106645; 112010-112012 |
| 225 → 100, 22500 → 10000 | 1 / 2 | descent 106650 (asıl sınır `MAGIC_TYPE8.Radius 30`, değişmedi); summon friend 110004, Escape 110035 (fiilen sınırsız kalır) |
| 0 (silah menzili) | değişmez | tüm savaşçı Type1 skill'leri, Judgment 112802, Helis 112815 |

Pot ve scroll satırları da 56 → 25 (`490013..490082`, `490701`, `500010`, `500053`); kendine atıldıkları için etkisiz.

### 4.3 Bekleme ve cast süresi (`ReCastTime`/`CastTime`, 0,1 sn) `[V]`

| Skill | Bizde → AlphaGame | Not |
|---|---|---|
| Outrage 106720 | ReCast 91 → **600** | ayrıca Type4 `BuffType 5 → 31`, `Duration 30 → 10`, `AttackSpeed 120 → 100` |
| restoration 106730, Regeneration 106750 | 250 → **600** | |
| Exceed Break 106815 | 254 → **600** | |
| Shock Stun 106820 | 252 → **300** | (son yapılarda WP `[8]=10`, WG `[8]=2`: SkillLevel 20 açılmıyor) |
| sword aura 106557 | 1 → 5 | |
| sacrifice 106660 | 250 → 600 | botta kapalı |
| Absolute power 110802 | 250 → **600** | `BotCore/BurstPlan.h:22-23` |
| Fire Armor 110573, Ice Armor 110673 | 250 → **1200** | |
| Vampiric Fire 110574 | 1 → 600 | |
| Freezing Distance 110674 | 250 → 600 | |
| **Mana Shield 110815** | ReCast 0 → **800**, CastTime **0 → 15** | bot anlık sayıyor (`BotCore/MageCombat.h:1306-1325`, `:1836-1850`) |
| Superior Parasite 112771 | 254 → 600 | |
| **Curse Refraction 112820** | 1 → **1800** | |
| **Elysian Web 112825** | 1 → **1800** | |
| Escape 110035 | 250 → 600 | Ronark'ta zaten reddedilir |
| Great healing 112527 | 20 → 0 | |
| ~112 satır | 1 → 0 | 0,1 sn → 0; pratik etkisi yok (CLI-04 istemci aralığı belirleyici) |
| Pot 490063 (720 HP Store) | 20 → 25 | `PotSupported` sınırı 25 (`BotCore/BotCombat.h:670-673`) içinde |
| Attack+ / Speed+ scroll 500034/500035 | 255 → 0 | MEC-BUF-11 "25 500 ms" satırı geçersiz |

`CastTime` bot kümesinde yalnız Mana Shield'da değişti (0 → 15). Bot zaman çizelgeleri (`bots/config/skill_*.txt`) bizim `MAGIC`'ten üretilmiştir (`tools/skill-script-gen.py:17-20`); yeniden üretilmelidir.

### 4.4 Etki değerleri (`MAGIC_TYPE1/3/4/5`) `[V]`

- **Type4 `ExpPct` AlphaGame'de 692 satırın hepsinde 0** (bizde çoğu 100; Mana Shield 15, Elysian Web 70). Kod iki ağaçta aynı alanı okur: Elysian Web `m_bMagicDamageReduction = sExpPct` (ALPHA `GameServer/MagicProcess.cpp:521-523`) ve büyü hasarında `if (reduction < 100) damage = damage * reduction / 100` (ALPHA `GameServer/MagicInstance.cpp:3074-3075`) ⇒ AlphaGame verisiyle **Elysian Web altındaki oyuncu büyü hasarı almaz** (bizde %30 azaltma) `[D]` + `[V]`, çalışma zamanı `[A]`. Mana Shield/Outrage `m_bManaAbsorb = sExpPct` (ALPHA `MagicProcess.cpp:540-542`) ⇒ 0, yani mana emme yok. Bu büyük olasılıkla AlphaGame verisinin içe aktarma kusurudur; UA-05 düzeltme betiğine aday (bkz. §6 S-3).
- Type4 `HitRate`/`AvoidRate` AlphaGame'de sabit 100 (bizde Illusion 107630/108630 `HitRate 10`, ice shot 108562 `150`): rogue isabet debuff'ı AlphaGame verisiyle etkisiz.
- **Yavaşlatmalar `BuffType 6 → 40` (SPEED2):** leg cutting 106520, Scream 106802, Cold wave 110007, Freeze 110603, Chill 110609, Ice arrow 110615, Solid 110618, Ice orb 110627, Ice burst 110633, Ice blast 110635, Frostbite 110639, Blizzard 110645, Ice comet 110651, Ice Impact 110657, Frost nova 110660, Prismatic 110670, ice storm 110671 (El Morad eşleri de). `Speed`, `Duration`, `SuccessRate` aynı. Direnç zarı SPEED2'de `ColdR`, SPEED'de `LightningR` okur (ALPHA `MagicInstance.cpp:2097`; bizde de `:1828`), yani bu skill'lerin direnç kaynağı **yıldırım direncinden soğuk direncine** geçer.
- **id + 80000 debuff satırları:** AlphaGame `MAGIC_TYPE4`'te yalnız frozen blade 190642/290642 (`BuffType 40`, `Speed 50`, 2 sn), Ice Staff 190672/290672 (`40`, `34`, 3 sn) ve 189642 var; Light Staff/charged blade (`+80000`) satırı yok. Mage Armor proc'ları 190573/190673/190773 (+2905xx..) `MAGIC` ve `MAGIC_TYPE3` (`DirectType 15`, `FirstDamage -1400/-1120`, `TimeDamage -700/-490`, 20 sn) ve Ice Armor proc'u için `MAGIC_TYPE4` (`40`, `Speed 46`, 10 sn) AlphaGame'de var.
- Type1: savaşçı skill'leri güçlenir: Cleave 106545 `Hit 150 → 250`, sword dancing 106560 `150 → 250`, Howling Sword 106570 `200 → 300`, sword aura 106557 `Hit 100 → 200, AddDamage 250 → 150`, thrust 106555 `100 → 200`; Scream `AddDamage 200 → 150`; Shock Stun `Hit 175 → 200, AddDamage 175 → 0`; blooding `350/400 → 200/150`. Priest melee zayıflar: Judgment 112802 `Hit 500 → 200`, Helis 112815 `AddDamage 400 → 0`, 112511..112750 `Hit` −%25..−%50. Mage asaları: Fire/Ice Staff `AddDamage 100 → 200`, `FirstDamage -2500 → -1103 / -883`.
- Type3 heal: healing 112509 `120 → 240`, major healing 112518 `240 → 360`, Restore 112512 `TimeDamage 200 → 400`, Major restore 112521 `400 → 600`, **Great healing 112527 `960 → 720`** (bot heal listesinde, `BotCore/PriestHeal.h:60-64`). Superior/Complete/grup heal'leri aynı. `EndDamage` farkları sunucuda okunmaz `[D]`.
- Type4 buff: Brave 112615 `MaxHP 120 → 240`, Strong 112624 `240 → 360`, Hardness 112633 `480 → 720`. Greatness/Undying/massiveness/peel aynı.
- Type5: diriltme `ExpRecover 10 → 60/70/80` (PvP ölümünde exp kaybı olmadığından Ronark'ta etkisiz). Type8: yalnız `KickDistance 0 → NULL`; `Radius` (descent 30) aynı. Type2: fark yok.

### 4.5 Potlar (MB-01) ve scroll'lar `[V]`

- AlphaGame `MAGIC.UseItem` NPC potlarında **dolu**: 490013 → 389013000, 490014 → 389014000, 490019 → 389019000, 490020 → 389020000, 490062/490081/490082 → kendi eşyaları. Yani MB-01 (tüketilmeyen 720 HP / 1920 MP pot) AlphaGame verisinde **yoktur**; pot kontrol edilir ve tüketilir (MEC-POT-01 kodu aynıysa).
- Bot stoğu bu anomaliye göre kurulmuştur: `db/002_bot_characters.sql:207` ve `db/004_bot_inventory.sql:158-165` 389014000 ×1, 389020000 ×1; kalabalık botlar (`db/010_bot_crowd.sql:217-230`) yalnız bunları ×1 taşır. AlphaGame'de ilk kullanımdan sonra bu botların HP/MP potu biter. Bot tarafı `BrainDriver.cpp:564` "tüketilmeyen pot"u `UseItem` ile değil değerle (720/1920) tanır ve `RoamPrepFill.h:49` `kRoamPrepUnlimitedPots = 9999` sayar: AlphaGame'de sessizce yanlış olur.
- Scroll'lar (MEC-BUF-11): HP Scroll 2000 800078000 ve Scroll of Armor 350 800076000 `ReqLevel 1 → 62` (botlar 80. seviye: etkilemez); dört scroll'da `Race 20 → 19` (CanUseItem ırk okumaz: ALPHA `GameServer/User.cpp:6031-6036`); Attack+/Speed+ recast 25,5 sn → 0. `BotCore/ScrollUse.h:70-94` tam şekil eşleşmesi ister (recast 0 / 25500): Attack+/Speed+ satırı AlphaGame'de şekil dışı kalır ⇒ `unsupported_item` olası `[A]`.

### 4.6 Bot ekipmanı (`ITEM`) `[V]`

| Eşya | Bizde → AlphaGame | Bot etkisi |
|---|---|---|
| Mage zırhı `276001005..276005011` (Complete Robe/Pants/Helmet/Glove/Boots +5/+1) | `ReqIntel` 160/156/152/144/148 → **174/176, 170/172, 166/168, 158/160, 162/164**; `ReqStr`/`ReqDex` 0 → 14/16; `ReqLevel 1 → 60` | Mage botları **INT 160** (`db/002_bot_characters.sql:130-131`, `:136-137`; `db/010_bot_crowd.sql:132`, `:140`): AlphaGame'de robe, pants, helmet ve boots kuşanma koşulunu sağlamaz (`ItemEquipAvailable` taban stat ister, ALPHA `GameServer/ItemHandler.cpp:574-586`). Kuşanılmış eşya girişte yeniden denetlenmiyorsa (`[A]`) etkisi yalnız yeniden kuşanmada görülür; insan oyuncu bu eşyayı giyemez ⇒ adalet sorunu |
| Savaşçı zırhı `216xxx005/011` | `ReqDex/ReqIntel/ReqCha` 0 → 14/16, `ReqLevel 1 → 60` | WP/WG DEX 60, INT 50, CHA 50: sağlar |
| Priest zırhı `296xxx005/011` | `ReqDex/ReqCha` 0 → 14/16, `ReqLevel 1 → 60` | PHD/PHB DEX 70, CHA 50: sağlar |
| Silahlar 149111071, 135751101, 156211041, 181110007, 190251131, kalkan 170250267 | yalnız `ReqLevel` (1 → 30..64) ve `ReqLevelMax` (→ 99) | 80. seviye bot: sağlar. `Damage`, `Delay`, `Range` **aynı** (CLI-01 R aralığı `Delay × 10` değişmez) |
| Takılar 3103..3406 | yalnız `ItemClass` | — |
| Tümü | `ItemClass` her satırda NULL (bizde 0/3/4/8) | ADR-0069 madde 4: E §3.4 kuralıyla düzeltilecek |

AlphaGame kuşanırken ek olarak **sınıf** denetler: `ItemClassAvailable` (ALPHA `GameServer/ItemHandler.cpp:525-570`, çağrı `:613`); bizde yalnız `ItemEquipAvailable` (`GameServer/ItemHandler.cpp:534-546`, çağrı `:568`) ⇒ MEC-CHR-04 değişir. Bot ekipmanının `ITEM.Class` değeri AlphaGame'de 0 (silah, kalkan, takı), 6 (savaşçı zırhı), 10 (mage zırhı), 12 (priest zırhı): master sınıflarla (106/110/112 ve 2xx) uyumlu, sınıf denetimi geçer `[V]`.

### 4.7 Seviye, katsayı, başlangıç noktası, kuleler `[V]`

- `COEFFICIENT`: fark yok.
- `LEVEL_UP`: AlphaGame 1-83 tam (bizde 24, 74, 77 satırı yok); 60-80 eşikleri AlphaGame'de düşük (ör. 79: 1 763 786 231 → 395 802 438; 80: 1 898 706 631 → 405 802 438), 81-83 ek. PvP başına +10 000 EXP ile 80 → 81 için ~40 000 öldürme gerekir: bot seviye atlaması pratikte yok, ama `ExpChange`'in bölge dışı atma kuralı izlenmeli.
- `START_POSITION` zone 71: bizde Karus (1380, 1090), El Morad (630, 920), `bRangeX/Z` 0/0 (canlı `FDP_kn_online` aynı); AlphaGame Karus **(1375, 1098)**, El Morad **(622, 898)**, `bRangeX/Z` **5/5**. Not: bizim satırda `sKarusGateX/Z = 10/10`, `bRange = 0/0`; MEC-DTH-06'daki "+rand(0..10)" bu yüzden yeniden denetlenmeli. Bot sabitleri `BotCore/RoamRouteData.h:195-219` (`{1385,1095}`, `{635,925}`) eski noktaya göre.
- `ZONE_INFO` 71: harita `freezone_a_20050718.smd` → `freezone_b.smd` (ADR-0069 madde 6, kabul edilen risk), `Type 2` aynı.
- Kuleler (`K_NPC`): 5300/5400/5310/5410 `bySearchRange 35 → 70`, `byAttackRange` 30/20 → **30/30**, `sAttackDelay 1000 → 600`, `byActType 0 → 5` (AIServer'da 5 ve 0 aynı dala düşer: ALPHA `AIServer/Npc.cpp:836-842`). Konumlar (`K_NPCPOS`) birebir aynı; AlphaGame ayrıca Bifrost Monument'ı (601) (1013, 993)'e koyar, HP 700 000 → 350 000; zone 71 NPC toplamı 655 → 818. Kapılar (`K_OBJECTPOS` tip 5) aynı; ek 16 tip-50 (`OBJECT_EFECKT`, görsel) nesne. Bot sabitleri `BotCore/RoamTowers.h:21-41` (`kRoamTowerSearchM = 35`) eski kule menziline göre.

## 5. Adalet ve hile önleme sonuçları

İlke (docs/03 §0, §13; K-5): bot gerçek istemcinin yapamadığı bir şeyi yapamaz; sunucu, gerçek istemcinin gönderemeyeceği paketi kabul etmemelidir. AlphaGame bazı sunucu denetimlerini kaldırdı ya da gevşetti; bot guard'ları bunların aynasıdır ve **kalmalıdır**.

| # | AlphaGame'de değişen sunucu denetimi | Kanıt | Botun aynası (kalmalı) | Sonuç / öneri |
|---|---|---|---|---|
| F-1 | R'de istemci `delaytime`/`distance` denetimi **kaldırıldı** (MEC-R-04). Sunucu yalnız `isInAttackRange` (15 m + silah) ve saniyede 1 R (`CanCastRHit`) uygular | ALPHA `GameServer/AttackHandler.cpp:10-26` (paket okunur, denetim yok); bizde `GameServer/AttackHandler.cpp:22-32`; ALPHA `Unit.cpp:932-953` | `BotCore/BotCombat.h:28-47` (`Delay+10`, `distance`), `:61-78` `CheckAttack`; `GameServer/Bot/ActionExecutor.cpp:886-931` (`FAIRNESS_REJECT` "MEC-R-04") | Bot guard'ı CLI-01'e yeniden bağlanmalı (kural adı MEC-R-04 → CLI-01 "istemci aralığı"); sunucu artık silah gecikmesini de menzil alanını da denetlemiyor: hileli istemci R'yi 1 sn'de bir atar (gerçek istemci `Delay × 10 ms`, ör. 1640 ms). İnsan/bot eşitliği için sunucuya geri eklenmesi ayrı karar (S-1) |
| F-2 | `WIZ_MOVE` hız alanı sınırı herkes için 90 (bizde W/M/P 67) | ALPHA `GameServer/User.cpp:3401-3424` (`nMaxSpeed = 90;` `:3413`); bizde `User.cpp:3083-3101` | `BotCore/BotMotion.h:12-13` (45/67), `:31-38` `ServerSpeedLimit` (67) | Bot 67'de kalmalı (gerçek istemci sprint 67, docs/03 §13.2). Hız alanı 68-90 gönderen hileli istemci artık atılmaz |
| F-3 | `WIZ_SPEEDHACK_CHECK` toleransı +10 → +15; geri ışınlama ancak **3. ardışık** ihlalde, 10.'da kopma; ihlalde `m_LastX/Z` güncellenmez | ALPHA `GameServer/User.cpp:4146-4182` (`:4159` +15, `:4165-4174`); bizde `User.cpp:3808-3835` | `BotCore/BotMotion.h:78-91` adım sınırı (hız × süre × 1,10 + 0,15 m), `:99-104` `RearmedMoveGateAgeMs`, `:126-129` `SpeedCheckWarpDistance` (74,16/87,75/100 m) | Sunucu iki ihlali sessizce yutar: bot adım guard'ı tek güvencedir. `SpeedCheckWarpDistance` eşikleri AlphaGame'de 77,46/90,55/102,47 m; `ActionExecutor.cpp:4086-4094` ilk iki ihlali "geçti" sayar (telemetri yanlış pozitif vermez ama ihlali kaçırır) |
| F-4 | Skill quest kapısı kodda duruyor ama AlphaGame `MAGIC.Etc` ve `UseStanding` **her satırda 0** | kod ALPHA `GameServer/MagicInstance.cpp:302-308`; veri `[V]` (1886 satır) | `BotCore/BotCombat.h:591-594` `CastQuestAllowed`, `ActionExecutor.cpp:1346-1354` `quest_locked` (`sEtc != 0`) | Sunucu 72-80. seviye skill'leri quest'siz kabul eder; istemci tablosu (`Skill_Magic_Main_us.tbl` kolon 29) hâlâ quest ister (MEC-MAG-14 `[A]`). Bot kuralı `sEtc`'e baktığı için AlphaGame'de **hiçbir skill'i kilitli saymaz**. Bot quest'leri durum 2 yazıldığı için (`db/003`) bugün fark yaratmaz; ama kuralın kaynağı istemci tablosu olmalı (S-4) |
| F-5 | MB-01 kapandı: NPC potları AlphaGame verisinde tüketilir | §4.5 | `BrainDriver.cpp:564`, `RoamPrepFill.h:49` | Adalet açısından iyileşme (insan ve bot aynı); bot stoğu ve pot politikası yeniden ayarlanmalı |
| F-6 | Bilinmeyen skill kimliği oyuncuyu **koparır** ve tüm sunucuya duyurulur | ALPHA `GameServer/MagicProcess.cpp:25-40` | bot yalnız bellekteki `MAGIC`'ten kimlik yollar (`ActionExecutor.cpp:1266-1272`, `:1490-1497`, `:2052`, `:2224`) | Bot için risk yok; elle kodlanmış kimlik listeleri (§3, `WarriorPressure.h:58-68` vb.) AlphaGame `MAGIC`'te var (`[V]`). Yanlış kimlik = kopma, yani test betikleri dikkatle |
| F-7 | Ayakta MP yenilenmesi kaldırıldı (yalnız otururken) | ALPHA `GameServer/User.cpp:3897-3908` (`MSpChange` yorum satırı `:3907`); bizde `User.cpp:3584` (L80'de ~40 MP / 6 sn) | MP modeli yok; tek oturma `BotCore/Survival.h:735-748` | İnsan da aynı kurala tabi: adalet sorunu değil, ayar sorunu. Botun oturması CLI-13 (≥ 1,0 sn duruş aralığı, otururken aksiyon yok) ile zaten sınırlı |
| F-8 | Kuşanmada **sınıf** denetimi eklendi; ırk hâlâ denetlenmiyor | ALPHA `GameServer/ItemHandler.cpp:525-570`, `:613` | bot eşyaları DB betikleriyle doğrudan yazılır (`db/007`, `db/010`) | Sunucu kuşanmayı yalnız `WIZ_ITEM_MOVE`'da denetler; DB'ye yazılan ekipman girişte yeniden denetlenmiyorsa (`[A]`) bot, insanın giyemeyeceği eşyayı (ör. INT 160 mage + ReqIntel 174 robe, §4.6) giyer ⇒ **adalet ihlali**; betikler AlphaGame `ITEM` koşullarına göre yeniden denetlenmeli |
| F-9 | Hız/yavaşlatma/stun sunucuda hâlâ uygulanmıyor (MEC-BUF-06); AlphaGame SPEED2'de `m_bSpeedAmount` 100 (oyuncu) yazar | ALPHA `GameServer/MagicProcess.cpp:586-597` | `BotCore/WarriorPressure.h:161-168` `HasSlowDebuff` yalnız `BuffType 6` | AlphaGame yavaşlatmaları `BuffType 40` (§4.4): bot yavaşlatıldığında **kendini yavaşlatmaz**, 67 ile koşmaya devam eder (yalnız `Survival.h:305-310` 40'ı tanır). Bu, insan istemcisinin yapamadığı bir kaçıştır ⇒ **adalet hatası, bot tarafında düzeltilmeli** |
| F-10 | Debuff direnci: `SuccessRate 100` olan yavaşlatma oyuncuya **her zaman** tutar; diğerlerinde ikinci şans | ALPHA `GameServer/MagicInstance.cpp:2081-2123` | `BotCore/WarriorPressure.h:27`, `MageCombat.h:53` (10 sn tutma) | Sunucu tarafı insan/bot için aynı; ama bot kendi yavaşlatılmasını uygulamazsa (F-9) yeni "her zaman tutar" kuralı botu insandan daha az etkiler |
| F-11 | Eşyalı/skill'li hile önleme ekleri: 300000-399999 kimlikleri oyuncuya kapalı, 490024 yalnız GM, ticaret/pazar açıkken cast yok | ALPHA `GameServer/MagicInstance.cpp:350-366` | bot bu kimlikleri kullanmaz | Etki yok |
| F-12 | Gerçek istemcinin gönderemeyeceği paketler hâlâ kabul: cast süresi (MEC-MAG-01), CASTING→EFFECTING sırası, aynı saniyede aynı potun tekrarı (MEC-MAG-05/MB-02), alan hedef noktasının menzili (MEC-AOE-01), yürünebilirlik (MEC-MOV-03) | ALPHA `GameServer/MagicInstance.cpp:453-463` (recast aynı), `:314-317`; `CharacterMovementHandler.cpp` (yalnız `IsValidPosition`) | CLI-03/04/06/07/08 | Bot guard'ları aynen kalır; AlphaGame bunların hiçbirini eklememiş |
| F-13 | `ExecuteType6` dönüşüm denetimleri iptal etmiyor (`//return false`); canavar dönüşümündeki oyuncu R'ye bağışık | ALPHA `GameServer/MagicInstance.cpp:2355-2376` (`:2375`), `Unit.cpp:875` | bot dönüşüm kullanmaz | Hileli istemci Ronark'ta Type6 kimliği göndererek dönüşebilir ve R'den bağışık olabilir `[I]`; gerçek istemcinin bunu gönderip gönderemediği `[A]`. UA-02 güvenlik listesine aday |
| F-14 | `WIZ_GENIE` (oto-av) hareket/R/skill paketlerini aynı işleyicilere geçirir | ALPHA `GameServer/GenieHandler.cpp:80-99` | — | Sunucu denetimleri aynıdır (aynı işleyiciler); ama 1534 istemcisinde Genie açıksa insan oyuncunun da istemci içi otomasyonu olur. Ronark'ta izinli mi: proje sahibi kararı (§6 S-12) |

## 6. Proje sahibine açık sorular

Her soru tek karar; önce ne olduğu ve neden önemli olduğu, sonra seçenekler.

**S-1. Normal saldırı (R) hızını sunucu denetlesin mi?**
Ne: Bizim sunucu, istemcinin bildirdiği silah gecikmesini ve mesafeyi denetliyordu; AlphaGame bu denetimi kaldırmış. Sunucu artık yalnız "saniyede en çok bir R" kuralını uyguluyor. Neden önemli: Hileli bir istemci ağır silahla bile saniyede bir R atabilir (gerçek istemci ör. 1,64 sn'de bir); botlar kendi kuralıyla yavaş kalır, yani hile yapan insan bottan hızlı vurur. Seçenekler: (a) AlphaGame gibi bırak, botun kendi sınırı yeter; (b) bizim eski denetimi (`delaytime ≥ Delay + 10`, `distance ≤ Range`) AlphaGame koduna geri ekle (`[MECH]` commit + ADR, docs/03 §15 kuralı).

**S-2. Hız ve speedhack eşikleri.**
Ne: AlphaGame hız alanı sınırını herkes için 90 yaptı (bizde 67) ve konum denetiminde geri ışınlamayı ancak üçüncü ardışık ihlalde yapıyor. Neden önemli: Bot zaten 45/67 ile yürüyor; ama sunucu hızlı hareket eden hileciyi artık geç yakalıyor ve bizim "bot ışınlandı mı" telemetrimiz ilk iki ihlali göremiyor. Seçenekler: (a) AlphaGame kuralını kabul et, docs/03'e yaz, botun eşiklerini 77,46/90,55/102,47 m'ye güncelle; (b) bizim 67 sınırını ve tek ihlalde geri ışınlamayı geri getir.

**S-3. AlphaGame skill verisindeki boş `ExpPct` sütunu.**
Ne: AlphaGame `MAGIC_TYPE4` tablosunda `ExpPct` her satırda 0. Sunucu bu sütunu Elysian Web'in büyü hasarı azaltması ve Mana Shield'in mana emmesi için okuyor. Sonuç: Elysian Web altındaki oyuncu büyü hasarı **hiç almaz**, Mana Shield hiçbir şey yapmaz. Neden önemli: Ronark'ta bir priest grubu 20 sn büyüye bağışık olur; mage botların hedef seçimi anlamsızlaşır. İstemci tablosunda bu sütun yoksa AlphaGame verisi içe aktarılırken kaybolmuş olabilir. Seçenekler: (a) UA-05 düzeltme betiğinde bizim değerleri (Elysian Web 70, Mana Shield 15, diğerleri 100) geri yaz; (b) AlphaGame verisini olduğu gibi bırak ve davranışı docs/03'e "kusur" (MB) olarak yaz.

**S-4. Skill quest kilidi neye göre?**
Ne: AlphaGame verisinde hiçbir skill'in quest şartı yok (`Etc` hep 0); istemci tablosu ise 70-80. seviye skill'leri quest'e bağlıyor. Neden önemli: Sunucu quest'siz kullanımı kabul eder; insan istemcisi engeller. Bot kuralı sunucu verisine baktığı için kilidi hiç görmez. Bugün botların quest'leri tamam yazıldığı için fark yok, ama yeni botlarda ve insan rakipte fark olur. Seçenekler: (a) bot kilidi istemci tablosundan (`Skill_Magic_Main_us.tbl` kolon 29) okunsun; (b) sunucu verisine `Etc` değerleri istemciden yazılsın (iki taraf da uygular).

**S-5. Debuff direnci ve leg cutting.**
Ne: AlphaGame'de `SuccessRate 100` olan yavaşlatmalar (leg cutting, Scream, Freeze) oyuncuya her zaman tutuyor; bizde yaklaşık %21 tutuyordu. Buz büyüleri (`SuccessRate 30`) yaklaşık %25 (soğuk direnci < 125 ise), %4 (≥ 126). Neden önemli: Savaşçının kaçanı yakalama gücü çok artar; bot taktikleri (10 sn tutma, KI-021 dönüşümlü ret) eski orana göre. Seçenekler: (a) AlphaGame kuralını kabul et ve botu buna göre ayarla; (b) bizim direnç formülünü geri getir (`[MECH]` + ADR).

**S-6. Ayakta MP yenilenmesi.**
Ne: AlphaGame'de ayakta MP dolmuyor, yalnız otururken doluyor. Bizde 80. seviyede ayakta yaklaşık 40 MP/tick doluyordu. Neden önemli: Botların MP ekonomisi (pot sayısı, savaş arası bekleme) bu yenilenmeye göre ayarlı ve botlar ancak potları bitince oturuyor. Seçenekler: (a) kabul et; bot savaş dışında otursun ve MP potu stoğu artırılsın; (b) ayakta yenilenmeyi geri aç (tek satır, `[MECH]`).

**S-7. AP formülündeki çift bonus ve Berserker +%20.**
Ne: AlphaGame AP hesabında ek AP'yi ve AP yüzdesini iki kez uyguluyor (STR 255 savaşçıda yaklaşık +108 AP) ve Berserker'a +%20 saldırı ekliyor. Neden önemli: Savaşçı hasarı ve öldürme süreleri değişir; bot hedef seçimi (`tgtOwnDps 470`) eski hasara göre. Seçenekler: (a) AlphaGame'deki gibi kabul et ve docs/03/docs/04'e yaz; (b) hata say ve düzelt (`[MECH]`).

**S-8. Botların pot stoğu (MB-01 kapanıyor).**
Ne: AlphaGame verisinde 720 HP ve 1920 MP potları artık tüketiliyor. Botlar bunlardan yalnız birer adet taşıyor (sınırsız sayıldığı için). Neden önemli: İlk kullanımdan sonra botların HP/MP potu biter. Ayrıca AlphaGame taşıma kapasitesini yarıya indirdi. Seçenekler: (a) bot DB betiklerinde stok sayısını gerçek bir insan stoğuna göre belirle (ne kadar?); (b) MB-01'i AlphaGame verisinde yeniden üret (önerilmez: istemciyle tutarsız).

**S-9. Mage zırhının INT şartı.**
Ne: AlphaGame verisinde mage zırhları INT 158-176 istiyor; mage botlarımız INT 160. Neden önemli: İnsan oyuncu bu zırhları INT 160 ile giyemez; bot DB betiğiyle giydirildiği için giyer (adalet). Seçenekler: (a) mage yapılarını (stat dağılımı) şartı karşılayacak biçimde değiştir; (b) şartı karşılayan başka zırh seç.

**S-10. Kule menzili ve Ronark başlangıç noktaları.**
Ne: AlphaGame verisinde kuleler 70 m'den görüyor (bizde 35), iç kuleler 30 m'den vuruyor (bizde 20) ve iki kat hızlı vuruyor; doğuş noktaları birkaç metre kaymış. Neden önemli: Botların kule bölgesi kuralları ve rota verisi eski değerlere göre. Seçenekler: (a) AlphaGame verisini kabul et, bot kule/rota sabitlerini yeniden üret; (b) kule değerlerini bizimkine çevir.

**S-11. PvP öldürme ödülü.**
Ne: AlphaGame Ronark'ta her PvP öldürmesine +10 000 EXP ve "Meat Dumpling" eşyası veriyor; eşya AlphaGame `ITEM` tablosunda yok (verilmez), EXP verilir. Neden önemli: 80. seviyede etkisi çok küçük (yaklaşık 40 000 öldürmede bir seviye), ama seviye 83'e çıkan bot Ronark'tan atılmaz (sınır 35-83). Seçenekler: (a) kabul et; (b) kapat.

**S-12. Genie (oto-av) Ronark'ta açık mı?**
Ne: AlphaGame'de istemcinin oto-av özelliği (Genie) hareket, normal saldırı ve skill paketlerini sunucuya gönderebiliyor; sunucu bunları elle gönderilmiş gibi işliyor. Neden önemli: Açıksa insan oyuncu da kısmen otomatik oynar; botlarla "aynı kurallar" karşılaştırması değişir. Seçenekler: (a) Ronark'ta Genie'yi kapat (sunucu tarafında `WIZ_GENIE` reddi); (b) açık bırak ve insan testlerinde Genie kullanılıp kullanılmadığını kaydet.


---

## Ek: Yöntem ve sınırlar

- Kod: iki ağaç `grep -an`/`sed -n` ile satır satır okundu; üç alt inceleme (büyü hattı, çekirdek kurallar, bot bağımlılıkları) satır numaralı rapor verdi; ilk 10 maddenin ve §2.1'deki `[D]` satırların hepsi bu rapor için yeniden okundu. `[D*]` satırlar yeniden okunmadı.
- Veri: §4'teki tüm sayılar `FDP_kn1534` ve `FDP_alpha1534` `SELECT` sonuçlarıdır (2026-10-08). Kişisel tablolar okunmadı.
- Okunmayanlar: AIServer'ın büyük kısmı (yalnız `MAP.cpp:131-134`, `Npc.cpp:794-860` okundu), AlphaGame Lua betikleri ve DB prosedürleri, Ronark haritasında tuzak olayı olup olmadığı, istemcinin `data[1] = 0` yankısını nasıl gösterdiği. Hiçbir madde çalışma zamanında gözlenmedi; davranış etkileri `[A]`/`[I]` olarak işaretlendi.
- docs/03 güncellemesinde önerilen sıra (docs/21 §5: önce docs/03): §2.1 satırları → §3 yeni kimlikler → docs/05 menzil/bekleme tablosu (§4.2-4.3) → bot sabitleri ve `bots/config/skill_*.txt` yeniden üretimi → T-UPG-04.
