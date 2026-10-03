# Bekleyen Kararlar: Teknik Etkisi Açıklanmış Seçenekler (2026-10-03)

Hazırlayan: Claude (planlayıcı/denetçi yardımcısı) · Durum: TASLAK, hiçbir karar alınmış sayılmaz · Kaynak: `gece/2026-10-02` dalı, **commit `f4daa27`** (dal ben okurken ilerledi; tüm `dosya:satır` kanıtları bu commit'e göre, kod dosyaları iki commit arasında değişmedi).

Okuma notu: Her karar için önce "bu ne, neden önemli", sonra seçenekler, sonra benim önerim ayrı yazılır. Öneri karar değildir; seçim sende. `[D]` kodla doğrulandı, `[V]` ölçüldü, `[A]` varsayım, `[Ö]` benim yargım. Doğrulayamadığım noktalar belgenin sonunda ve ilgili yerde "doğrulanamadı" diye işaretli.

---

## (C) Bu kararlar neyi engelliyor?

Önce bir not: otonom gece döngüsü hiçbir kararda teknik olarak durmaz (`plans/OTONOM_DONGU.md` §2: tasarım kararlarını Claude verir ve ADR'ye "gözden geçirilmeli" yazar; döngü yalnızca **faz sınırında** durur). Aşağıdaki "bloke" sütunu, **faz kabul kapısı** (`docs/17` §4, §5) demektir; kabul zaten yalnızca senin imzandır.

| Karar | Hangi plan / faz kapısını bloke eder? | Bloke etmiyorsa: beklerken iş devam eder (yetkili işler) |
|---|---|---|
| **A1. Bot quest kurulumu** (`db/003`) | Hiçbirini. | F4-60/F4-61 (algı bağlaması), kalan skill koşuları, `db/005`/`db/006` yazımı, F5-55 sürer. Betik depoda KAPANDI ve geri alınabilir. 72-80. seviye skill ölçümü gerekirse betik geçici uygulanıp geri alınır (F4-27 K8 gibi). |
| **A2. 51–54 quest kimliklerini sunucuda `Etc`'e taşımak** (Q-28) | Hiçbirini. G4 içindeki "T-MECH-SKILL botla koşusu" 70. seviye 32 skill için, karar "taşı" çıkarsa **yeniden koşulur**; kapıyı kilitlemez. | Mevcut hâlle ölçümler sürer (warrior/priest/mage koşuları `docs/05` §9). KI-017 "AÇIK (karar gerekli)" kalır; bot kuralı (`UseStanding == 1`) değişmeden çalışır. |
| **ADR-0017 + ADR-0018 (F4 paketi; kapsam, Ek 7 okçu/rogue)** | **F4 faz kabulünü (G4) bloke eder**; F4 kabulü olmadan F6'ya (F5 + ADR-0018 m.4 ön koşul, `docs/17` §5 G6a) formal geçiş yok. **BLOKE (1/2)** `[Ö]` | F4-60/61 gibi küçük algı dilimleri, skill koşuları, KI-DEG-01 planı. Kapsam daraltılırsa (okçu/rogue yok) F4 skill koşusu mage ile biter (`docs/reports/gece-2026-10-03.md` §7 madde 4, 8). |
| **ADR-0006 (+9 Ek): navigasyon A\*, sorgu kapısı ≤ 64 hücre, LoS advisory** | **F5 kabulünü (G5) ve F5-55 (sunucu entegrasyonu) kabulünü bloke eder**; F6 ön koşulu F5. **BLOKE (2/2)** `[Ö]` | `BotCore` içindeki saf mantık ve araçlar (`nav-regress`, `nav-measure`) sürer; ADR kendisi "F5 başlamadan onaylanmalıydı" der (ADR-0006 Bağlam). |
| ADR-0005 (IOCP thread modeli) | F2 kabulü (`docs/phase-reports/F2-taslak.md:58`). Her sunucu bot kodunun temeli. | Tüm F4/F5 işleri sürer. Reddedilirse büyük yeniden yazım (aşağıda). |
| ADR-0014 (hesap doğrulaması atlanır) | F2 kabulü (aynı rapor). | Hepsi sürer. |
| ADR-0015 (komut kanalı: `/bot`, `BotCommands.txt`, `+bot`) | F2/F3 kabulü (`F3-taslak.md:56`). | Hepsi sürer. |
| ADR-0007, ADR-0016 (telemetri, test çatısı) | F3 kabulü (`F3-taslak.md:56`). | Hepsi sürer; sırf kayıt. |
| ADR-0002 Ek F8-02, ADR-0031-DEG Ek F8-01 | Hiçbirini (araçlar, DB'ye yazmaz). | F8-03/F8-04 yazımı sürer. |
| ADR-0030..0033-DEG teyidi | Hiçbirini (zaten KABUL; yalnızca kayıt tutarsızlığı). | F8/F11/F12 planları sürer. |

**Gerçekten ilerlemeyi kilitleyen iki karar `[Ö]`:** (1) ADR-0017/0018 F4 paketi (özellikle ADR-0018 kapsamı ve Ek 7), (2) ADR-0006 navigasyon. İkisi de F6'ya giden yolun (F4 kabulü, F5 kabulü) üzerindedir. Q-28'in iki yüzü (A1, A2) kapı kilitlemez.

---

## (A) Quest kararı (Q-28): iki ayrı karar

### Önce ortak zemin: "quest ile açılan skill" nedir ve bugün ne oluyor?

Oyunda bazı güçlü skill'ler, bir NPC'nin görevi bitince açılır. Bu projede iki ayrı kural var ve ikisi aynı şeyi söylemiyor:

- **Sunucu kuralı `[D]`:** Skill'in `MAGIC.Etc` sütunu 0'dan farklıysa, oyuncu GM değilse ve sunucu **Release** derlemesindeyse, o quest'in durumu tam **2** olmak zorundadır (`GameServer/MagicInstance.cpp:268-276`; `CheckExistEvent` tam eşitliktir: `GameServer/QuestHandler.cpp:137-148`). Debug derlemesinde bu kapı hiç yoktur.
- **İstemci kuralı `[A]`:** Oyun istemcisi quest bitmeden skill'i "?" ikonuyla gösterip kullandırmıyor (veri örüntüsü + senin gözlemin; istemci exe'sinden doğrulanmadı: `docs/03` MEC-MAG-14).
- **Bugünkü veri `[V]` (`docs/03:134`, `tools/client-tbl-quests.py --server`):** 72 skill'de quest sütunu `Etc` (kimlik 510–523): iki taraf da kilitler. 32 skill'de (70. seviye, kimlik 51–54: Howling Sword, Iron Skin, berserk Echo, critical restore, imposingness, Bless of God, Subside, incineration, meteor Fall, Prismatic, ice storm, Stun Cloud, Chain lightning, Dark pursuer, Bloody Beast…) kimlik **yanlış sütunda**, `UseStanding`'de duruyor; `Etc` 0. Yani sunucu bunları kilitlemiyor, yalnızca istemci kilitliyor.
- **Geçmiş:** Yedek tablo `MAGIC_BAK_etc` bu 32 satırın düzeltme öncesi `Etc` değerinin 1 olduğunu gösteriyor (`docs/03:134`). `db/001` (K-4/ADR-0003) yalnızca `Etc = 1` satırlarını 0'a çekti ve 510–523'e dokunmadı (`db/001_magic_etc_fix.sql:1-5`). Bu, **bu veritabanında `Etc` sütununun bir kez güvenilmez çıktığını** gösteriyor (1306 satır "görev 1 şart" diyordu): A2'de dikkat edilecek bir geçmiş.

Not: İstediğin "docs/13 §3" `docs/13`'te **thread modeli** (ADR-0005) bölümüdür; adalet ve quest ile ilgili asıl yer `docs/03` §13.4 ("üç katman") ve MEC-MAG-14/MB-15'tir. İkisini de kullandım.

Önemli bir bulgu (iki karara da girdi): **72–80. seviye skill'lerin (510–523) hepsi ağaç puanı ister** (`Skill % 10 != 0`; kontrol `MagicInstance.cpp:960`: `m_bstrSkill[ağaç] >= SkillLevel`). Referans bot profillerinde hiçbir ağaç 70'i geçmiyor (`db/002_bot_characters.sql:126-127`: WP `0x…46 00 34 14` = ağaçlar 70/0/52, usta 20; WG `0x…3C 3E 00 14` = 60/62/0, usta 20). Dolayısıyla **bugünkü botlar 72–80. seviye skill'leri puan yüzünden zaten atamaz**; quest yazılsa da yazılmasa da. Bu analizi `docs/appendix/data/skills_*.csv`'den doğruladım (50 satır `Etc` 510–523, hepsi `req_kind = skill points in category`).

---

### A1. Bot quest kurulumu (`db/003_bot_quests.sql`)

**Ne ve neden önemli (sade):** Botların karakter satırında quest listesi boş. `db/003` betiği, 12 botun listesine kendi sınıfının quest'lerini "tamamlandı (2)" olarak yazar: savaşçı {51, 510, 511}, mage {53, 515, 516, 517}, priest {54, 518–523} (`db/003_bot_quests.sql:96-101`). Amaç: bot, "o quest'leri bitirmiş bir insan oyuncu" ile aynı kurala tabi olsun. **Yalnızca bot karakterleri etkilenir** (açık 12 ad listesi, `LIKE 'Bot%'` yok; betik gerçek oyuncu satırına dokunmaz; rogue botu yok). Durum: plan F4-27 `KAPANDI` (merge `b73d31f`), iki doğrulama turu (Tur 1 big-endian hatası bulundu, Tur 2 `DOĞRULANDI`, geri alma sağlama toplamı birebir: `plans/F4-27-…md:203-283`). **DB'de şu an UYGULANMAMIŞ** (yedek tablo `USERDATA_BOT_QUEST_BACKUP` 0 satır; bunu ben DB'ye bağlanmadığım için doğrulayamadım, kaynak: plan Doğrulama Tur 2 "Test sonu durum" ve `docs/KNOWN_ISSUES.md` KI-018, bu beyanını sen teyit ediyorsun).

**Kodun tarafı zaten karar verilmiş durumda:** `BeginCast` artık quest'siz botu `quest_locked` ile (sunucuya paket göndermeden) reddediyor, quest'li botu geçiriyor (`GameServer/Bot/ActionExecutor.cpp:787-796`). Bu kod, betik uygulansa da uygulanmasa da çalışır ve zararsızdır.

**Karar kimde? (belirsizlik):** ADR-0018 Ek 3 (quest) başlığında "gözden geçirilmeli" etiketi yok ve plan "proje sahibinin bildirimi" diyor (`plans/F4-27…md:11`), ama kararı yazan Claude. Açıkça senin onayın kayıtta yok; bu yüzden **açık karar olarak** sunuyorum. STATUS.md:6, senin 2026-10-03'te `testing` ve `testmage` hesaplarına 18 quest'in (51–54, 510–523) elle işlendiğini bildirdiğini yazıyor (doğrulanmadı, USERDATA okunmadı): yani insan test hesaplarında tüm quest'ler zaten var.

#### Gerçekte ne değişir (somut etki)

| Soru | Cevap |
|---|---|
| Bot skill erişimi | **Bugünkü 12 botta pratikte değişmez.** 72+ skill'ler puan yüzünden erişilemez (yukarıda). 51–54 skill'lerini sunucu zaten kilitlemiyor, yani bot bunları quest olmadan da atıyor (`Etc = 0`). `db/003` yalnızca (a) ileride ağaç ≥ 72'ye çıkarılırsa, (b) 72–80. seviye skill ölçümü (`-v QuestTestPoints=1`, yalnız `BotWP_K`/`BotMF_K`, geri alınabilir) yapılırsa anlam kazanır. F4-27 K8 koşusu bunu gösterdi: quest + puan varken Hell blade (`106580`) ve Igzination (`110575`) `effected`; geri alma sonrası `refused (quest_locked)` (plan Doğrulama Tur 2 çalışma zamanı). |
| Adalet (bot–insan) | Sunucu tarafında kural **aynı**: ikisi de `Etc ≠ 0` ise quest durumu 2 ister (bot GM değil, `Authority = 1`). Asıl fark istemci: insan "?" kilidine takılır `[A]`, bot istemciyi hiç kullanmaz. 51–54 skill'lerinde bu fark kalıcıdır: bot quest yazılsa da yazılmasa da kilitsiz atar. `db/003` o skill'ler için kâğıt üstü eşitlik sağlar (bot "quest'i bitirmiş insan gibi" ilan edilir), kuralı gerçekten uygulatmaz. Bu `docs/03` §13.4'ün "ihtiyatlı bot sınırı" ilkesiyle uyumlu ama MB-15 satırında "bot ve insan için kural aynı" ifadesi bu nüansı söylemiyor `[Ö]`. |
| Adaletsizlik var mı? | **Bugün yok:** botlar insan test hesaplarından (hepsi quest'li) daha fazlasını yapamıyor. **Gelecekte olabilir:** Q-04 ("bu kurulumda 510–523 quest'leri oyunda tamamlanabilir mi") **cevapsız** (`docs/18:53`). Gerçek oyuncu quest'i oyunda bitiremiyorsa ve botun ağacı ≥ 72 olursa, bot oyuncunun hiç ulaşamayacağı skill'i kullanmış olur. Önlem: Q-04 cevaplanana kadar bot ağaçlarını 70'in üzerine çıkarma (zaten ADR-0010/K-7). |
| Yan etki (stat) | Savaşçıda quest 51 durum 2 ise ve savunma ağacı ≥ 70 ise savunma bonusu 50 → 60 olur (`GameServer/User.cpp:2310`, `docs/03:134`; belge `:2276` diyor, yeni numara 2310). Bugünkü WG'nin savunma ağacı 62 olduğu için **etki yok**; ağaç 70'e çıkarılırsa AC değişir ve `docs/04` hasar/AC modeli yeniden hesaplanmalı. |
| **K-7/ADR-0010 ile çelişki** | ADR-0010 (senin kararın): "ilk sürümde 510–523 skill'leri kullanılmaz". Reddedilen alternatif tam olarak "betikle görevleri tamamlanmış saymak" idi (`ADR-0010:15`, "Doğrulanmamış görev durumu"). `db/003` bunu yapıyor. Bugün sonuç aynı (kullanılmıyor, puan yok) ama **bu karar ADR-0010'u fiilen yumuşatıyor**; açıkça yazılmalı. |
| Geri alınabilirlik | Kolay: `db/003_bot_quests_rollback.sql`, yedek tablo, sağlama toplamı eşitliği Tur 2'de kanıtlandı. Betik sunucular kapalıyken çalışır (çıkışta bellekteki liste ezer: `DBAgent.cpp:930-938`). Bot oyundan çıkınca sunucu listeyi kendi sırasıyla geri yazar (idempotans buna dayanıklı, Tur 2'de test edildi). |
| Risk | Düşük: yalnız 12 bot satırı, açık ad listesi, `RAISERROR` ile 200 kayıt sınırı. Bilinen sınır: kimlik ≥ 32768 olan bir kayıt `smallint` taşması verip betiği durdurur (zarar yok, işlem geri alınır; Tur 2 Not 1). |

#### Seçenekler

1. **ONAYLA ve kalıcı uygula** (`-v QuestTestPoints=0`). Bot satırları quest'li; insan test hesaplarıyla aynı durum. Maliyet: ADR-0010'a "bot quest'li, ama puan yok" notu. Test sonrası yeniden kurulumda `db/002` sonrası `db/003` uygulanır.
2. **ONAYLA ama "gerektiğinde uygula":** betik depoda kalır, DB'ye yalnızca (a) bot ağaçları ≥ 72'ye çıkarılırken veya (b) 72–80. seviye ölçümü yapılırken uygulanır, sonra geri alınır. DB `db/002` durumunda kalır.
3. **REDDET:** `db/003` uygulanmaz (istersen depodan çıkarılır), `quest_locked` kuralı kalır. Botlar 72+ skill'i hiç kullanmaz; KI-018 bot tarafında "kabul edilmiş sınır" olarak kapanır. ADR-0018 Ek 3 geri çekilir. En muhafazakâr ve adalet açısından en temiz; ölçüm gerekirse betik bir kez daha yazılır (maliyet düşük çünkü kod hazır).
4. **DEĞİŞTİR (kapsam):** (a) yalnızca sunucunun gerçekten denetlediği 510–523'ü yaz, 51–54'ü yazma (quest 51 savunma yan etkisini ve "kâğıt üstü eşitlik" sorununu kaldırır; ama insan test hesaplarındaki 18 quest'le uyumsuz kalır), ya da (b) 18 quest'in tamamını yaz (insan test hesaplarıyla birebir; rogue botları yok, 52/512–514 gereksiz).

**Önerim `[Ö]`: seçenek 1 (ONAYLA, kalıcı uygula, `QuestTestPoints=0`), şu iki notla:** ADR-0010'a tek satırlık işaret eklenir ("bot quest'li olsa da 72+ skill ağaç puanı yüzünden kullanılmaz; ağaç ≥ 72 yapmak ayrı karar ve Q-04'e bağlı"), ve bot ağaç puanları 70'in üstüne çıkarılmaz. Gerekçe: bugün sıfır davranış etkisi, kolayca geri alınır, bot ve insan test hesapları aynı duruma gelir, ileride ölçüm/profil değişiminde ek adım gerekmez. Seçenek 2 de makul (DB'yi en temiz bırakır); fark yalnızca "bir sonraki ölçümde bir komut daha". Seçenek 3'ü ancak "botlar quest skill'lerinden kalıcı olarak uzak dursun" dersen öneririm.

**Senin yapman gereken:** Bir cümleyle 1/2/3/4'ten birini söyle; uygulama (sunucular kapalıyken `db/003`) bende.

---

### A2. TÜM oyuncuları etkileyen mekanik değişiklik: 51–54'ü sunucuda `UseStanding`'den `Etc`'e taşımak (Q-28, KI-017)

**Ne ve neden önemli (sade):** 32 skill'in "hangi quest" bilgisi veritabanında yanlış sütunda. İstemci bu quest'i uygular, sunucu uygulamaz. Soru: sunucu da uygulasın mı? Bu **bot kararı değil, sunucu oyun verisi değişikliğidir** ve değişirse **sunucuya bağlanan herkes** etkilenir (`docs/03` §15: sunucu mekanik düzeltmeleri `[MECH]` etiketli ayrı commit + ADR ister ve hem bota hem oyuncuya uygulanır; K-8: "her düzeltme ayrı kararla"). Ayrıca bu 32 satırın `UseStanding` değeri (51..54) yüzünden sunucu bu skill'lerde iki denetimi **hiç yapmıyor** (KI-017).

**Kodda bu iki sütunun etkisi `[D]` (`GameServer/MagicInstance.cpp`):**
- `:268-276` quest kapısı: `sEtc != 0` ⇒ quest durumu 2 şart.
- `:306` ve `:328-329`: `UseStanding == 1` ⇒ "oyuncu hareket etmiyor olmalı" (`m_sSpeed != 0` ise reddedilir) ve CASTING'te menzil denetimi.
- `:353`: EFFECTING/FLYING'de menzil denetimi **yalnızca `UseStanding == 0`** iken (`sRange > 0` ve mesafe ≥ `sRange` ise reddedilir).
- Bugün bu 32 skill'in `UseStanding`'i 51..54 olduğu için `:306`, `:328`, `:353` hiçbiri tetiklenmiyor: **ayakta şartı yok, menzil denetimi yok.**
- Menziller (appendix verisi): mage 70. seviye skill'lerinde `Range` 45, priest'te 45–56, warrior'da 0 (`docs/appendix/data/skills_*.csv`).

**Bilmediğimiz kritik şey (doğrulanamadı):** Bu 32 satırın *orijinal* `UseStanding` değeri ne olmalıydı (0 mı, 1 mi)? `docs/05:29` bu skill'leri "UseStanding = 1" diye listeliyordu ama veri 51..54. İstemci tablosunun (`Skill_Magic_Main_us.tbl`) `UseStanding` kolonu henüz karşılaştırılmadı (`docs/18:102`: "diğer kolonlar karşılaştırılmadı"; araç yalnızca kolon 29 ve 15'i okuyor: `tools/client-tbl-quests.py:8-10`). Bu yüzden "Etc'e taşırken UseStanding'i 0 mı 1 mı yapalım" sorusunun cevabı bugün elimizde yok.

#### Seçenekler ve her birinin oyuncu etkisi

| | S0. Mevcut hâl (taşıma) | S1. `Etc`'e taşı, `UseStanding` = 0 (docs/18 Q-28'in ikinci seçeneği) | S2. `Etc`'e taşı, `UseStanding` = 1 | S3. Önce ölç, sonra karar (ek seçenek) | S4. Sunucu koduyla çöz (başka seçenek) |
|---|---|---|---|---|---|
| **Ne değişir** | Hiçbir şey. | `MAGIC`'te 32 satır: `Etc = 51..54`, `UseStanding = 0`. | Aynı, `UseStanding = 1`. | Veri değişmez; önce kanıt toplanır (aşağıda). | `MagicInstance.cpp` quest kapısı `Etc` yerine `UseStanding ∈ 51..54`'e de bakar. Sunucu kodu değişir (DeepSeek işi, ayrı plan). |
| **Oyuncuya etkisi** | Yok. İstemci kilidi sürer `[A]`; değiştirilmiş istemci/bot kilidi aşabilir. | (1) Sunucu artık quest'siz oyuncuyu reddeder (Release): "?" kilidini aşan istemci hilesi kapanır. (2) **Quest'i durum 2'ye getirmemiş gerçek oyuncu** (oyun akışı 3/4 gibi başka durumda bırakıyorsa) bu skill'leri **kaybeder**: bkz. `QuestHandler.cpp:60-90` durum 1/2/3/4 kullanıyor; hangi akışın 2 verdiği quest betiğine bağlı, **doğrulanamadı** (Q-04'ün 51–54 karşılığı açık). (3) 32 skill'de EFFECTING/FLYING menzil denetimi **ilk kez devreye girer**: mage 45 m dışı hedef reddedilir. (4) Ayakta şartı yine yok. | (1) ve (2) aynı. (3) **Ayakta şartı ilk kez gelir**: bu skill'ler hareket halinde atılamaz (`:328-329`), menzil denetimi CASTING'te. Oyuncuya en görünür değişiklik. | Hiçbir şey değişmez; kanıtla karar. | S1 ile aynı quest etkisi; `UseStanding` aynen kalır (menzil/ayakta denetimleri bugünkü gibi yok). Veri bozulmaz. |
| **Bota etkisi** | Yok. | Bot `sEtc != 0` olan 32 skill için `quest_locked` kuralına girer (`ActionExecutor.cpp:790`): **`db/003` uygulanmış olmalı** (51/53/54 yazıyor); yoksa botlar 70. seviye skill'lerini kaybeder (F4-45/46/48 ölçümleri çalışmaz). `needsStanding = (UseStanding == 1)` kuralı değişmez (KI-017 kapanır). | Bot, bu skill'lerden önce durur ve bir tick bekler (`needsStanding`, F4-24); `db/003` yine şart. | — | Bot kapısı zaten sunucuyla aynı mantığı uygular; sunucu koduna paralel güncelleme gerekir. |
| **Geri alınabilirlik** | — | Kolay: `db/001` gibi yedek tablolu SQL + geri alma betiği; sunucu yeniden başlatma ya da konsol `reload_magics` (`GameServer/ChatHandler.cpp:39`, `:1097-1110`) yeter. Ama **değişiklik oyuncular bağlıyken yapılırsa** kayıp skill hemen görünür. | Aynı. | — | Kod geri alması derleme gerektirir; veri geri alması gerekmez. |
| **Test edilmesi gerekenler** | — | (a) `MAGIC` 32 satırın geçici kopya tabloda (F1-03'ün yöntemi) dene-geri al; (b) gerçek istemcide quest durumu 2 olan hesap (`testing`/`testmage`: 18 quest işli, STATUS:6) 32 skill'i atar; quest'siz hesap reddedilir; (c) mage skill'lerini 45 m içi/dışı hedefe at (yeni menzil denetimi); (d) priest `Range 56/45`; (e) quest'i oyun içinde bitiren yeni karakter durum 2 alıyor mu (Q-04 benzeri); (f) bot T-MECH-SKILL'i 32 skill için yeniden koş; (g) Release ve Debug farkı (Debug'da sunucu kapısı yok); (h) rollback sonrası eski davranış. | S1'in (a)-(h)'sine ek: hareket halinde atış reddi, hareketsiz atış kabulü; istemcinin bu skill'leri gerçekte hareket halinde atıp atmadığı (KI-017 `[A]`). | İstemci tablosunun `UseStanding` kolonunu salt-okunur çöz ve 32 satırı karşılaştır; test hesabıyla (quest'siz) istemcide 70. seviye skill'i dene (gerçekten "?" mi?); quest NPC'lerini (Skaki 51, Clarence 52, Drake 53, Minerva 54) oyunda bitir, `strQuest` durumunu izle (yalnızca kendi test hesabı). | Birim test + çalışma zamanı; ayrıca ADR-0003'ün "Etc'e dokunma" ilkesiyle çelişmediğini kontrol et. |
| **Riskler** | İstemci hilesi kapısı açık; adalet argümanı "bot da insan da aynı eksik kuralı görür". | Veri geçmişi (KI-001): `Etc` sütunu bir kez güvenilmez çıktı; yanlış `UseStanding` değeri (0 yerine 1 olmalıysa) menzil/ayakta davranışını sessizce değiştirir. Quest durumu eşitliği (`== 2`) bazı oyuncuları kapsam dışı bırakabilir. Her temiz kurulumda ek bir betik uygulanması gerekir (`db/README` listesine eklenmeli). | S1 + "ayakta şartı" oyuncuya yeni kısıt. | Zaman maliyeti; kararı geciktirir. | Sunucu kodu değişir; "DeepSeek işi" kuralı (`CLAUDE.md`), `[MECH]` ADR gerekir. |

**Önerim `[Ö]`: şimdilik S0 (mevcut hâl), ama kararı kanıtlı vermek için S3'ü ucuz bir ön adım olarak yap.** Gerekçe, kısaca:
1. Kazanç dar: yalnızca değiştirilmiş istemciyle kilidi aşmayı kapatır; normal oyuncuyu bugün zaten istemci engelliyor `[A]`.
2. Zarar riski geniş ve test edilmemiş: quest durumunun gerçek oyun akışında 2'ye ulaşıp ulaşmadığı bilinmiyor (Q-04 cevapsız); yanlışsa **tüm oyuncular** 70. seviye skill'lerini kaybeder.
3. `UseStanding`'in doğru değeri bilinmiyor (0, 1 ya da başka); yanlış seçim sessiz davranış değişikliği doğurur ve S1 ile S2 oyuncuya farklı kısıt getirir.
4. Veritabanı geçmişi (KI-001) bu sütunların bir kez bozuk geldiğini gösteriyor; yeni bir düzeltmeyi ölçmeden yapmak aynı hatayı tekrarlama riski taşır. K-8 de "mekanik düzeltmeler ayrı kararla" der.
5. Maliyet düşük: S3 salt-okunur araçla ve senin test hesabınla yapılır; sonra S1/S2'ye geçmek kolaydır, geri dönmek de.

S3 sonucu "istemcinin `UseStanding`'i X, quest durum 2 akışı doğrulanmış" çıkarsa S1/S2 (hangisiyse) mantıklı hâle gelir.

**Bot tarafı için (A2'den bağımsız) bir not:** `docs/18` Q-28 öneri sütunu "mevcut hâl, bot satırlarına yine de quest yazılır" diyor: yani A1 ile uyumlu.

**Senin yapman gereken:** S0/S1/S2/S3/S4'ten birini seç; S3 seçersen "test hesabıyla istemci denemesini yapabilirim/yapamam" bilgisini ver.

---

## (B) Gözden geçirilmeyi bekleyen ADR'ler

Bulma yöntemi: `git grep -l 'gözden geçirilmeli' f4daa27 -- docs/adr` ⇒ ADR-0002 (Ek), 0005, 0006, 0007, 0014, 0015, 0016, 0017, 0018, 0031-DEG (Ek). ADR-0030/0032/0033-DEG listede yok (başlıkta etiket yok). ADR-0002 ve ADR-0031 yalnızca **Ek** düzeyinde etiketli; ana kararları sizin.

Önce sahibin kararını gerçekten isteyenler (üç grup + teyit), sonra sırf kayıtlar.

### B1. ADR-0005: Bot tick'i ve aksiyonları IOCP thread'inde

- **Sade:** Sunucu, oyuncu paketlerini tek bir "ağ işçisi" thread'de sırayla işliyor (`shared/SocketMgr.cpp:48-54`: tek `m_thread`). Botların da aynı sırada çalışması için ayrı bir zamanlayıcı thread yalnızca "tik" olayı bırakır, botun kendisi ağ işçisinde yürür. Neden önemli: `CUser` üzerinde kilit yok; bot ayrı thread'den dokunursa gerçek oyuncu paketleriyle yarışır ve nadiren görülen çökme/bozulma riski doğar. Bedel: bot yavaşlarsa oyuncu paketleri de bekler (bu yüzden tik süresi ölçülüyor; ölçülen p95 ≤ 408 µs, 4 botla, bütçe 5000 µs).
- **Alternatif:** (a) ayrı bot thread'i, `CUser`'a doğrudan dokunur: basit ama kilitsiz yarış; (b) mevcut 30 sn zamanlayıcıya bağlanmak: çok kaba; (c) tik'i biriktirmek: yavaş tikte kuyruk büyür. Kararı değiştiren gerçek alternatif: **botları gruplara bölüp ikinci bir işlem thread'ine taşımak** (ADR "Sonuçlar" bunu bütçe aşılırsa öneriyor). 
- **Yanlışsa ne olur / geri alma:** Yanlışsa (ör. 16+ botla gerçek oyuncu gecikmesi) bot tik'i bir bütçe aşımında gruplara bölünür; asıl maliyet büyük: tüm `GameServer/Bot/*.cpp` bu varsayıma göre kilitsiz yazıldı (bot durumu yalnızca IOCP thread'inde; ek kilitler gerekir). `ENABLED=0` ile tüm mekanizma kapanır (anında geri alma).
- **Koda ne kadarı dayanıyor:** Çok: `shared/SocketMgr.cpp:99-113` (`SetBotTickHandler`, `PostBotTick`), `shared/SocketDefines.h:8` (`SOCKET_IO_EVENT_BOT_TICK`), `GameServer/Bot/BotManager.cpp:342,379`; `BotManager.cpp` 3493 satır, `ActionExecutor.cpp` 3267 satır hep bu modele göre. `shared/` değişimi `LogInServer`/`AIServer` derlemesini etkiler ama onlar kancayı kurmaz (ADR). Ölçüm: `PERF_SAMPLE` (`BotManager.cpp:500-512`).
- **Önerim:** **ONAYLA.** Gerekçe: tek IOCP thread zaten sistemi seri çalıştırıyor; alternatif yarış riski getiriyor; ölçülen maliyet bütçenin çok altında. Yalnızca hedef yükte (12-16 bot yürürken) ölçüm eksik (`F4-taslak §5`): onayla ama "16 bot tik ölçümü açık" notunu koru.
- **Senin yapman gereken:** ONAYLA/DEĞİŞTİR cevabı ver; ONAYLA ise F4 faz kabulünde 16 bot tik ölçümünün F5 başına bırakılıp bırakılmadığını da söyle.

### B2. ADR-0006: Navigasyon: ızgara A\*, LoS `advisory` (+ Ek F5-03..F5-11)

- **Sade:** Botlar, haritanın 4 m'lik kareli bir ızgarası üzerinde A\* ile yol buluyor (navmesh değil). Üç sonuç ayrı: bulundu / yol yok / "bilinmiyor" (20 000 düğüm sınırı aşıldı). Görüş hattı (engel arkasından vurma) şimdilik yalnız tavsiye (`advisory`): botu engellemiyor. Neden önemli: botların takılmadan yürümesi F5'in ve sonraki tüm davranışların (F6+) temeli.
- **Kararı değiştiren alternatif:** navmesh (Recast/Detour: düzgün yollar, ama bağımlılık/üretim hattı, sunucu çarpışma verisi doğrulanmadı), waypoint grafiği (el bakımı). ADR tetikleyici koyuyor: takılma > 2/bot-saat ise yeni ADR (`docs/18` R-09).
- **Yanlışsa ne olur:** Izgara 4 m dar geçitlerde kaba kalabilir; geçişte botlar takılır. Geri alma: `BotCore/Nav*.h` ve testleri silinir, sunucu etkilenmez (ADR bunu söylüyor). Maliyet: ~4400 satırlık `BotCore/Nav*.h` + 135 birim test (14 nav test dosyası) yeniden yazılır.
- **Koda ne kadarı dayanıyor:** 4401 satır `BotCore/Nav*.h` (`NavGrid`, `NavPath`, `NavSmooth`, `NavStuck`…) ve 14 nav test dosyasında 135 `TEST_CASE` (tüm `BotCoreTests` toplamı 259). **GameServer bu dosyaları henüz `#include` etmiyor** (F5-55 sunucu entegrasyonu `TASLAK`): yani karar şimdilik yalnızca saf mantıkta duruyor, geri alma ucuz.
- **Dikkat edilecek bir "kapı gevşetmesi" `[Ö]`:** ADR madde 4, `docs/12` T-NAV-03'ün "1000 rastgele sorgu" kapısını **tüm harita yerine ≤ 64 hücre (256 m)** sorgularıyla ölçülecek şekilde yeniden tanımlıyor (gerekçe: tüm harita sorgularında p95 ≈ 19 000 düğüm, 2 ms tutmaz). Bu, AC-NAV-02'yi (p95 ≤ 2 ms) anlamlı ama daha kolay hâle getirir; `docs/12:168` buna not düşülmüş. Uzun yürüyüşler (respawn → arena, 233–640 m) kapı dışında raporlanıyor ve şu an hiyerarşik aramaya ertelenmiş (`NodeLimit` ≈ %4).
- **9 Ek (F5-03 düzleştirme, -04 hareketli hedef, -05 ulaşılamaz hedef, -06 tehlike maliyeti, -07 geri çekilme noktası, -08 formasyon, -09 takılma, -10 LoS, -11 regresyon aracı):** davranış ayrıntıları, saf mantık, geri alması kolay: sırf kayıt, tek tek onay gerekmez. Bunların tamamını başlık düzeyinde taradım, satır satır okumadım (doğrulanamadı).
- **Önerim:** **ONAYLA** (ana karar + kapı tanımı), "uzun mesafe için hiyerarşik arama ayrı karar" notuyla. Navmesh'e geçiş tetikleyicisi R-09 kalsın.
- **Senin yapman gereken:** "ızgara A\* kalsın ve T-NAV-03 kapısı arena ölçeği (≤ 64 hücre) olsun" cümlesini onayla ya da kapıyı tüm haritaya geri çevir (o durumda hiyerarşik arama planı öne gelir).

### B3. ADR-0017 + ADR-0018: F4 paketi (aksiyon yürütücü, adalet koruması, kapsam genişletmesi)

Bu iki ADR toplam ~180 KB: ADR-0017 ana karar + 38 Ek, ADR-0018 ana karar + 29 Ek (Ek numaraları yinelenmiş: "Ek 2" ve "Ek 3" iki kez geçiyor, `ADR-0018:37,40,43,46`). Hepsini tek tek okumadım (doğrulanamadı); ana kararları, F4-23/F4-07/F4-34/F4-35/F4-38 Eklerini ve Ek 3/Ek 7'yi okudum.

**B3a. ADR-0017 ana karar: "aksiyon = gerçek paket + gerçek handler; adalet koruması saf mantık"**
- **Sade:** Bot, istemcinin yollayacağı paketin aynısını `CUser::HandlePacket()`'a verir; durumu doğrudan değiştirmez. Adalet koruması ("insanın yapamayacağını yapma": hız tavanı, 1 sn skill kapısı, pot aralığı, toplam 6 aksiyon/sn…) sunucuya gitmeden önce `BotCore`'da saf fonksiyon olarak kontrol eder. Neden önemli: bot "hile yapmıyor" iddiasının dayanağı bu.
- **Alternatif:** doğrudan `CUser` alanlarına yazmak (daha basit, ama bölge/olay yan etkileri kaçar, adalet iddiası zayıflar); sunucu başlığına bağımlı guard (birim test edilemez).
- **Yanlışsa:** Adalet sınırları `[A]` (ölçülmemiş) değerlere dayanıyor (ör. `kRegeneMinDeadMs 3000`, CLI-11 6/sn); yanlışsa değerler değişir, yapı değişmez. Geri alma: tek tek guard kuralları ayarlanır (düşük maliyet).
- **Koda dayanan:** Tüm `GameServer/Bot/ActionExecutor.cpp` (3267 satır), `BotCore/BotCombat.h` (1074 satır), `BotCore/Perception.h` (2399 satır); `check-perception-contract.py` R1 0/0, R2 0/28, R3 0/18 (F4-taslak §5).
- **Önerim:** **ONAYLA.**

**B3b. ADR-0018 ana karar: "F4 kapsamı genişletildi; aksiyon desteği tamamlanmadan F4 'bitti' denmez"**
- **Sade:** Ana hat F4'ü "bitti" diye durdurmuştu; ama uçan/çift tipli/buff/alan skill'leri, cast iptali, speedhack paketi, envanter doldurma gibi şeyler olmadan F6+ davranışları yazılamıyor. Bu yüzden Claude, senin "eksiksiz ama hızlı" sözüne dayanarak 10+ dilimi F4'e kattı. Neden önemli: F4 kabulünün **ne zaman** "tamam" sayılacağını belirliyor ve işin büyüklüğünü (F4-24'ten F4-60'a kadar yaklaşık 35 plan).
- **Alternatif:** F4'ü dar tutup (yalnız ana hat F4-01..F4-23) skill desteğini F6/F7 içinde, ihtiyaç doğdukça eklemek (daha az "önden iş", ama karar motoru yazarken tek tek geri dönüş).
- **Yanlışsa:** Kapsam gereğinden büyükse zaman kaybı; ama kodun çoğu `ENABLED=0` ile kapalı ve bayraklı; geri alma maliyeti "yazılan işi kullanmamak". ADR zaten "kapsam daraltılabilir, genişletilmez" der.
- **Koda dayanan:** Büyük: F4-24..F4-53 KAPANDI (merge'ler), 259 birim test (260'a gidiyor), skill koşuları `docs/05` §9.
- **Önerim:** **ONAYLA** (iş büyük kısmı bitti, geri dönmek kâr getirmez) **ve** kapsamı şu sınırla dondur: okçu (Ek 7), rogue ve Type7 kapsam dışı kalsın; yeni dilim önerileri bundan sonra sana sorulsun.
- **Bu karar F4 kabulünü bloke eder (bkz. C).**

**B3c. ADR-0018 Ek 7: okçu/rogue botları kapsamda mı?**
- **Sade:** Bot kadrosu 6 profil (2 warrior, 2 priest, 2 mage; 12 karakter). Rogue/okçu bot yok. ADR-0018 Ek 7 okçu skill'lerini (Type2) erteliyor. Soru: ileride okçu/rogue botu da olacak mı?
- **Seçenekler:** "Hayır": F4 skill koşusu mage ile biter (F4-48), kadro 6 profil kalır. "Evet": ayrı ADR + bot profili + Type2 dilimi (ok tüketimi, yay denetimi).
- **Önerim:** **"Hayır, şimdilik"** (`docs/01` §2 zaten rogue/archer'ı temel sürümden hariç tutuyor; kadro `db/002`'de 6 profil). Daha sonra istenirse ayrı ADR. **Senin yapman gereken:** evet/hayır.

**B3d. ADR-0017/0018 içinde sahibin bilmesi gereken, ama onay beklemeyen Ek kararları (kısaca)**
- **F4-38 (hız kontrol paketi, CLI-12):** Bot her 10 sn'de gerçek istemci gibi `WIZ_SPEEDHACK_CHECK` yollar (ADR-0017 Ek F4-38'e göre sunucunun tek mesafe denetimi buna bağlı: ≥ 87,75 m sıçrayan oyuncu geri ışınlanır, `User.cpp:3615-3641`; bu satırı ben ayrıca kodla doğrulamadım). Kapatma anahtarı var: `[BOT] SPEEDHACK_CHECK` (`BotManager.cpp:200`, varsayılan 1). ONAYLA.
- **F4-34/F4-35 (summon, Gate/descent):** bot summon/warp skill'lerini atabiliyor; "hedef güvenli mi" önkontrolü bilerek yok (karar katmanında). ONAYLA; F7'de güvenlik kapıları şart.
- **F4-23 (algı sözleşmesi, R3 istisnası):** test sürücüleri hedef **bot** oturumunun konumunu doğrudan okuyabiliyor (18 isabet): geçici, F6'da karar motoru gelince sıfırlanacak. ONAYLA, F6 öncesi kapanışı kayda bağlı.
- **F4-07 (`no_np`):** NP = 0 olan bot yeniden doğmaz (KI-013); 20 ölümden sonra bot ölü kalır. Bilinçli sınır; NP yenileme ayrı iş.
- Kalan ~30 Ek: tek yetenek dilimleri (cast/pot/party/algı/betik/skill türleri). **Sırf kayıt**: tek tek onay gerekmez, kod `ENABLED=0` iken etkisiz ve bayrakla kapanır.
- **Önerim:** toplu **ONAYLA**; yalnızca B3b/B3c senin kararını ister.

### B4. ADR-0014: Bot oturumu hesap doğrulamasını ve `SET_LOGIN_INFO`'yu atlar

- **Sade:** Gerçek oyuncu giriş yaparken parola kontrolü ve `CURRENTUSER` tablosuna "çevrimiçi" kaydı yapılır. Botların parolası/IP'si yok; bu yüzden bot bu iki adımı **atlar** ve `CURRENTUSER`/`TB_USER` kişisel veri tablolarına hiç dokunmaz (gizlilik kuralıyla uyumlu). Çıkışta `AccountLogout` da atlanır. Yalnızca 12 sabit bot adı spawn edilebilir (rastgele ad kabul edilmez).
- **Alternatif:** sabit IP ile `SET_LOGIN_INFO` çalıştırmak (artık satır riski, kişisel veri tablosuna yazma), gerçek parola ile giriş taklidi, sahte LoginServer oturumu: üçü de risk/yüzey artırıyor.
- **Yanlışsa ne olur:** Bot hesapları `CURRENTUSER`'da görünmez; bu listeyi okuyan harici araç (web paneli) botları göstermez; LoginServer aynı hesapla gerçek girişe "çevrimiçi değil" der: bot hesapları insana verilmemeli. Geri alma: `CharacterSelectionHandler.cpp:190` ve `DatabaseThread.cpp:458`'deki iki koşul ve `BotManager::StartSession` kaldırılır (küçük).
- **Koda dayanan:** İki tek satırlık koşul (`m_botSink == nullptr`, `CharacterSelectionHandler.cpp:190`, `DatabaseThread.cpp:458`), `BotManager` sabit 12'lik tablo. Çalışma zamanında insan testiyle doğrulandı (T-ARCH-01, 02, 03, 04 GEÇTİ).
- **Önerim:** **ONAYLA.** (Hem gizlilik kuralına hem sade tasarıma uyuyor.)
- **Senin yapman gereken:** ONAYLA; yalnızca "bot hesapları web panelinde görünmüyor" yan etkisini bil.

### B5. ADR-0015: Komut kanalı (konsol `/bot`, `BotCommands.txt`, oyun içi `+bot`)

- **Sade:** Botları çalışma anında yönetmenin üç yolu: konsol komutu, sunucu klasöründeki `BotCommands.txt` (saniyede bir okunur, okununca silinir) ve GM yetkili oyuncunun `+bot` sohbet komutu. Komutlar yalnızca bot tik'inde (IOCP thread) çalışır. Neden önemli: otomasyon (gece döngüsü, senaryo koşucusu) konsola yazamadığı için dosya yolu gerekti; ama bu bir **güvenlik yüzeyidir**.
- **Alternatif:** yalnız konsol (otomasyon çalışmaz), TCP/pipe/HTTP portu (yeni ağ yüzeyi, kimlik doğrulama gerekir), konsola tuş enjekte etmek (kırılgan).
- **Yanlışsa ne olur:** Sunucu çalışma dizinine yazabilen herkes bot yönetebilir; yalnız `[BOT] ENABLED=1` iken etkin (üretim dışı test sunucusu). `+bot` yalnız GM. Geri alma: tablo satırları ve komut bölümü silinir (`ChatHandler.cpp:45,84,1174,1227`).
- **Koda dayanan:** `BotManager` komut çekirdeği; her F2-F4 doğrulaması bu kanalı kullandı (`BotCommands.txt`).
- **Önerim:** **ONAYLA**, tek şartla: üretim sunucusunda `[BOT] ENABLED=0` (varsayılan) kalsın. İstersen (DEĞİŞTİR seçeneği) dosya kanalını derleme bayrağına bağlayıp üretim ikilisinde hiç derletmeyiz; ek iş küçük ama zorunlu değil.
- **Senin yapman gereken:** ONAYLA ve üretimde `ENABLED=0` olduğunu teyit et.

### B6. ADR-0030..0033-DEG teyidi

**Durum çelişkisi:** ADR dosyalarının hepsinde "Durum: KABUL (proje sahibi, 2026-10-02: …)" ve alıntı var; commit `84bfb65` "Proje sahibi kararları: ADR-0030..0033-DEG kesinleştirildi" der; `docs/reports/degerlendirme-2026-10-02-ek.md:22-40` KABUL yazar. Ama `docs/STATUS.md:567` hâlâ "arka plan ajanında Claude kararı, gözden geçirilmeli" diyor. Yani ADR'ler sahibin kararı, **STATUS satırı eski**. Teyit = bunu tek cümleyle onaylamak (ve STATUS satırını düzeltmek).

Her biri için sahibin kararı ne, koda ne kadar dayanıyor ve teyit istenen ayrıntı:

| ADR | Sahibin kararı (ADR'den) | Kodda ne dayanıyor | Teyit istenen ayrıntı (varsa) | Önerim |
|---|---|---|---|---|
| 0030-DEG | Öğrenme iki aşama: önce rol profili + oturum içi kestirim, karakter bazlı kalıcı öğrenme F12'ye (kapsamdan çıkmadı) | Kod yok; yalnız `docs/14`, `docs/17` F12 taslağı | Yok | ONAYLA (kayıt) |
| 0031-DEG | `killdiff_timed` (`win_margin` 2: fark −1,0,+1 beraberlik) ve ayrı `wipe_first`; eşikler yapılandırılabilir | `tools/bot-outcome-eval.py` (F8-01, 70 öz-test); `ScenarioRunner` `win_rule` henüz yok | Sen "±2/±1 örneklerle kesinleşsin" demiştin: ADR'deki 8 satırlık örnek tablosunu **onayladığın açıkça kayıtlı değil**; ayrıca pilot beraberlik hedefi %15–35 ve varsayılan süreler (300/120 sn) Claude'un seçimi `[A]` | ONAYLA, örnek tablosunu da "evet bu doğru" diye teyit et |
| 0032-DEG | Maç öncesi kurulum konumu serbest, maç başladıktan sonra yalnız normal mekanik; kurulum bot **çevrimdışıyken DB satırına yazılarak** | `db/004_bot_inventory.sql` (F4-40) bu yöntemi kullanıyor; `ScenarioReset` henüz yok | Yok (gerekçe ve canlı `CUser` tutarlılığı ADR'de yazılı) | ONAYLA |
| 0033-DEG | Arena modunda geri çekilme arena içinde; serbest Ronark için güvenli konuma çekilme, yeniden gruplanma, savaşa dönüş ayrıca (F11) | `BotCore` nav arena mantığı (`NavArenaTests`, 4 test); `docs/12` §13.4 | Yok | ONAYLA |

**Ek F8-01 (ADR-0031 altında, "gözden geçirilmeli") ve ADR-0002 Ek F8-02:** ikisi de yalnızca **salt-okunur Python araçları** (`tools/bot-outcome-eval.py`, `tools/bot-composition-check.py`); DB'ye yazmaz, sunucuya bağlanmaz. F8-01: girdi sözleşmesi ve ADR'nin boş bıraktığı noktalar `[A]` (sınırlar dahil, 1..3600 üst sınır, ek geçersizlik nedenleri). F8-02: kompozisyon kuralı yalnız "en çok 2 priest" (REQ-PTY-02), ulus başına 10 karakter ihtiyacı `[A]`, ek karakter adlandırması (`BotWP2_K`…). **Önerim:** ONAYLA (toplu); ikisi de kodu/mekaniği değiştirmez, yanlışsa araç düzeltilir.
**Senin yapman gereken (B6 toplu):** "ADR-0030..0033-DEG benim kararımdır, ADR-0031'deki örnek tablosu doğrudur" de; STATUS satırı bende düzelir.

### B7. Sırf kaydı düşülen, ayrıca onay gerektirmeyenler (gruplanmış)

- **ADR-0007 (telemetri JSONL, sınırlı kuyruk, ayrı yazıcı thread) + Ek F3-02 (`<match>.jsonl`, `summary.json`):** Biçim ve depolama seçimi; kapalıyken (`ENABLED=0` ya da `TELEMETRY=off`) hiçbir şey çalışmaz, dosya oluşmaz. Alternatifler (kilitsiz halka, DB'ye yazım, binary) daha karmaşık ya da oyun DB'sine yük. Yanlışsa: `[BOT] TELEMETRY=off` ya da `Telemetry.*` silinir. Dayanak: `GameServer/Bot/Telemetry.cpp` (752 satır), `tools/bot-telemetry-report.py`; 16 botta `decisions` seviyesi hacim tahmini 20–80 MB/10 dk (`docs/16:265`) `[Ö]` (diskte dikkat). **ONAYLA.**
- **ADR-0016 (kendi mini test çatımız, `BotCore` + `BotCoreTests`):** Üçüncü taraf kütüphane eklemeden doctest uyumlu makrolarla birim test; çatı ileride doctest'e geçilebilir. Dayanak: 259 test, `tools/run-tests.sh`. Yanlışsa: `#include` değişimiyle doctest. **ONAYLA.**
- **ADR-0015 Ek F3-02 (`match`), F3-03 (`scenario`), F3-04 (`+bot`):** komut ailesi ayrıntıları; B5 ile birlikte.
- **ADR-0006 Ek F5-03..F5-11, ADR-0017 Ek F4-02..F4-53 (B3d dışındakiler), ADR-0018 Ek 2..27 (Ek 3 quest ve Ek 7 hariç):** dilim ayrıntıları; B2/B3 ile birlikte.
- **ADR-0014 F2-04 eki (çıkışta `AccountLogout` atlanır):** B4 ile birlikte.

---

## Özet tablo: sunulan kararlar

| # | Karar | Türü | Öneri |
|---|---|---|---|
| A1 | Bot quest kurulumu (`db/003`) | **Sahibin kararı** | Onayla, kalıcı uygula (alternatif: gerektiğinde) |
| A2 | 51–54'ü `Etc`'e taşı (Q-28; tüm oyuncular) | **Sahibin kararı** | Şimdilik mevcut hâl; önce S3 ölçümü |
| B1 | ADR-0005 thread modeli | Sahibin kararı (temel) | Onayla |
| B2 | ADR-0006 navigasyon + T-NAV-03 kapısı | **Sahibin kararı (kapı)** | Onayla |
| B3b | ADR-0018 F4 kapsamı | **Sahibin kararı (kapı)** | Onayla ve dondur |
| B3c | Ek 7 okçu/rogue kadroda mı | **Sahibin kararı** | Hayır, şimdilik |
| B3a | ADR-0017 ana karar | Kayıt + onay | Onayla |
| B4 | ADR-0014 hesap doğrulaması | Onay | Onayla |
| B5 | ADR-0015 komut kanalı | Onay | Onayla (üretimde `ENABLED=0`) |
| B6 | ADR-0030..0033-DEG teyidi | Teyit | Teyit et |
| B7 | ADR-0007, 0016, Ekler | Kayıt | Toplu onay |

---

## Doğrulayamadığım noktalar ve tutarsızlıklar

1. **DB durumu:** `USERDATA_BOT_QUEST_BACKUP` 0 satır ve `testing`/`testmage` quest'leri: DB'ye bağlanmadım (kural); yalnızca plan raporu, STATUS ve KI-018 yazısına dayandım.
2. **İstemci "?" davranışı** `[A]` ve **quest tamamlanabilirliği (Q-04)**: doğrulanmadı; A1 adaletinin ve A2 riskinin merkezinde.
3. **32 satırın orijinal `UseStanding` değeri:** bilinmiyor; istemci tablosunun bu kolonu karşılaştırılmadı.
4. **ADR-0017 (38 Ek) ve ADR-0018 (29 Ek), ADR-0006 (9 Ek):** hepsini tek tek okumadım; örnekleme ve başlık taraması.
5. **Eski satır numaraları:** `docs/03` MEC-MAG-14 "`User.cpp:2276`" diyor, gerçek konum `User.cpp:2310`; "`MagicInstance.cpp:269-275`" bugün `:268-276` (`#if` dahil); ADR-0005 "`SocketMgr.cpp:51`" bugün `:48-54`. Dokümanın sahibi olarak düzeltme önerilir (ben salt-okunurum).
6. **ADR-0018 numaralandırması:** "Ek 2" ve "Ek 3" ikişer kez var (`:37,40,43,46`); atıf yaparken tarih ekle.
7. **STATUS.md:567** ADR-0030..0033-DEG'i hâlâ Claude kararı sayıyor (ADR dosyalarıyla çelişir).
8. **Dal hareketi:** `gece/2026-10-02` bu çalışma sırasında ilerledi (`0001d04` → `f4daa27`: Q-27→Q-28, `db/003`→`db/005`/`db/006` yeniden adlandırma, F4-60 planı); kanıtları `f4daa27`'ye sabitledim.
9. `docs/13` §3 quest ile ilgili değil (thread modeli); isteğin ilgili yeri `docs/03` §13.4 ve MEC-MAG-14/MB-15 olarak okundu.
