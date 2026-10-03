# F6 bağımlılık özeti (TASLAK planlar F6-01..F6-10)

Hazırlayan: Claude (planlayıcı yardımcısı) · 2026-10-03 · Taban dal: `gece/2026-10-02` @ `7891f74` (yazım sırasında döngü ilerledi; planlar `c2c5a08` üzerinde yazıldı ve `7891f74` üzerinde yeniden doğrulandı: F4-53/F4-60 `KAPANDI`, F5-59/F5-60/F5-67/F4-55 planları geldi). BotCore birim test sayısı **260** `[V: git grep -c '^TEST_CASE' @ 7891f74; STATUS.md F4-60 kaydı da 260]`.

Bu dosya `plans/README.md` satırı **değildir**; planlar `HAZIR` olmadan README'ye işlenmez. Hepsi **TASLAK**: nav bağlama dilimleri (F5-61..F5-66), `F8-05` ve `F4-61` henüz kodlanmadı/yazılmadı.

**Proje sahibinin kuyruğu** (`docs/STATUS.md:411`, 2026-10-03): F4-60 → F5-59 → F5-60 → F4-55 → F5-61..F5-65 → F4-61 → F5-66 → **F8-05** (`ScenarioReset`) → **F6-01..F6-08** → F7-01..F7-05 → F8-03/04/06/07. Bu dosyadaki F6-09 ve F6-10 kuyrukta **yoktur** (görev tanımı F6-01..F6-08 dedi): F6-09 priest solo destek (görev tanımındaki F6-07 "heal/buff/cure/debuff" kapsamının bölünen yarısı), F6-10 solo EV + EVAL-1v1 (AC-SOLO-01..05 ve F6 faz testi için gerekli). Kuyruğa eklenmeleri proje sahibinin kararıdır.

**Proje sahibi hedefi (ilk uçtan uca):** *warrior botun hedef seçmesi → navigasyonla yaklaşması → uygun skill + R ve pot kullanması → HP %30'un altında geri çekilmesi → iyileşince yeniden savaşa katılması.* Kanıt: **F6-06 S4** (oyun içi, Claude koşar). Birim düzeyi: `L0_EndToEnd_Sim_Warrior` (F6-06) ve `Surv_Sim_FixedDamage_T_SUR_01` (F6-05).

## 1. Tek tablo: F6 planları, bağımlılıklar ve F4/F5/F8 ile kesişim

| Plan | Başlık (kısa) | Boy / dosya | F6 ön koşulu | F4 (`KAPANDI` gerekenler) | F5 (`KAPANDI` gerekenler) | F8 / diğer dış bağ | Yeni dosyalar | Oyun içi kapı / test | Kabul (AC) |
|---|---|---|---|---|---|---|---|---|---|
| **F6-01** | Karar katmanı iskeleti: ortak FSM, `Intent`, parametre kayıt defteri, **B0-NAIVE** | M / 7 | — | F3-05, F4-03/04/05/24, F4-16/17/18, F4-50/51/52, **F4-53/F4-60 (artık `KAPANDI`: `SkillSpec` `SkillMeta`'yı içerir)** | — (saf mantık) | — | `Brain.h`, `BrainParams.h`, `SkillReady.h`, `PolicyNaive.h`, `BrainTests.cpp` | yok (birim) | AC-LRN-04, MET-SUR-06 altyapısı |
| **F6-02** | Hedef seçimi (solo): süzme, skor, bağlılık, override | S–M / 4 | F6-01 | F4-06, F4-50, F4-51, F4-52, F4-53/F4-60 (`ObservedStatusTable`) | F5-05, F5-06 (alan anlamı) | — | `TargetSelect.h`, `TargetSelectTests.cpp` | yok (birim; F6-06'da `TARGET_SET`) | MET-TGT-04, AC-SOLO-04 (birim) |
| **F6-03** | Warrior yaklaşma + R/Type1 döngüsü, sprint, kaçan hedef | M / 4 | F6-01, F6-02 | F4-02, F4-03, F4-24, F4-26, F4-28, F4-36, F4-37, F4-45 | F5-04, F5-50, F5-57; çalışma zamanı için **F5-61/F5-62/F5-63** | `directClear` sorgusu hiçbir F5 diliminde yazılı değil (boşluk) | `WarriorPressure.h`, `WarriorTests.cpp` | T-WAR-01..03 (F6-06'da) | AC-WAR-01/02/03/05 (F6-06'da) |
| **F6-04** | Pot kararı (HP/MP, EWMA, ortak 2,5 sn, stok) | S–M / 4 | F6-01 | F4-04, F4-17, F4-40 | — | T-MECH-POT-03 (insan ölçümü) bekliyor | `PotPolicy.h`, `PotPolicyTests.cpp` | T-POT-01..03 (F6-06'da) | AC-SUR-03/04 |
| **F6-05** | Geri çekilme / iyileşme / yeniden giriş | M / 4 | F6-01, F6-04, (F6-03 eylem kısmı) | F4-05, F4-07 | F5-06, F5-07, F5-09, F5-51, F5-54, F5-57; çalışma zamanı için **F5-62/F5-63/F5-65** | ADR kararı: eşik %30 mu formül mü (proje sahibi); `NavRetreatPlanner` çağrısı hiçbir F5 diliminde yazılı değil (boşluk) | `Survival.h`, `SurvivalTests.cpp` | T-SUR-01/03 (F6-06'da), T-SUR-02 (F6-07'de) | AC-SUR-01/02 |
| **F6-06** | `L0Policy` birleşimi + `BrainDriver` sunucu bağlaması + **warrior solo uçtan uca** | L / 10-11 (a/b bölünebilir) | F6-01..05 | F4-01..07, F4-20, F4-24, F4-28, F4-37, F4-38, F4-40, F4-45, F4-50/51/52, F4-53/F4-60; **F4-55** (karşılıklı görünürlük) | **F5-59** (`NavService`), **F5-61** (kiriş guard'ı), **F5-62** (`/bot goto`, waypoint), **F5-63** (`NavFollower`/takılma/F5-57), **F5-64** (bütçe, `NAV_*`), **F5-65** (nav durumu temizliği: ölüm/respawn), **F5-66** (oyun içi doğrulama) | **F8-05** (kuyrukta F6'dan önce; KI-DEG-05/KI-013 elle) | `PolicyL0.h`, `PolicyL0Tests.cpp`, `BrainDriver.h/.cpp` | **T-IGT-WAR-01 (G6a)**, uçtan uca S4, T-WAR-01..03, T-POT-01..03, T-SUR-01/03, AC-ARCH-02 | AC-WAR-01/02/03/05, AC-SUR-01/03/04, AC-ARCH-06 |
| **F6-07** | Priest heal döngüsü (kendine + tek müttefik) | M / 6-7 | F6-01, F6-04, F6-05, F6-06 (+G6a) | F4-03, F4-18, F4-24, F4-31, F4-42, F4-43, F4-46, F4-53/F4-60 (`HealObsRing`, isteğe bağlı) | F5-04, F5-07, F5-10; F5-59..F5-66 | — | `PriestHeal.h`, `PriestHealTests.cpp` | **G6b**: T-PRI-01/02/07, T-SUR-02 | AC-PRI-01/02/09 |
| **F6-08** | Mage saldırı: tek hedef/alan, kiting, Mana Shield, element tahmini (**summon F7**) | M / 6 | F6-01, F6-02, F6-04, F6-05, F6-06 (+G6a) | F4-24, F4-25, F4-26, F4-29, F4-30, F4-36, F4-47, F4-48, F4-51, F4-60 (`FillSkillMeta`) | F5-04, F5-07, F5-10; F5-59..F5-66 | B0-NAIVE koşusu (F6-06 `naive` modu) | `MageCombat.h`, `MageTests.cpp` | **G6c**: T-MAG-01..04 | AC-MAG-01/02(tek başına)/03 |
| **F6-09** | Priest solo destek: self buff, cure, tek hedef debuff, Judgment/Helis | M / 6 | F6-07 | F4-28, F4-32, F4-44, F4-46, F4-52, **F4-53/F4-60 (`KAPANDI`)**; F4-61 (planı yok) isteğe bağlı | — | T-MECH-BUF-03 (BUF-HP-01 kararı) | `PriestSupport.h`, `PriestSupportTests.cpp` | G6b tamamlayıcı; self buff/cure/debuff ölçümü | (AC-PRI-04/05/06 F7'de) |
| **F6-10** | Solo EV, kaçınma/takip sınırı, dolaşma, **EVAL-1v1** | M / 6 | F6-01..06 + (07/08/09 profil için) | F4-06, F4-51, F4-52, F4-55 | F5-06, F5-07, F5-51; F5-59..F5-66 | **F8-05 (ScenarioReset/win_rule)**, F8-01 (`bot-outcome-eval.py`, `KAPANDI`) | `SoloEval.h`, `SoloTests.cpp` | T-SOLO-02..05; **EVAL-1v1** (F8-05 sonrası); F6 faz testi | AC-SOLO-01..05, F6 "B0-NAIVE'i yener" |

**F5-55 dilim eşlemesi (doğrulandı, `plans/F5-55-...md` §1A, 2026-10-03):** F5-59 `NavService` yaşam döngüsü/harita (`HAZIR`) · F5-60 su/engel denetimi (`HAZIR`, çevrimdışı) · F5-61 kiriş guard'ı (`TASLAK`) · F5-62 `/bot goto` waypoint zinciri + `NavPathfinder`/`NavReach`/maliyet katmanı kurulumu (`TASLAK`) · F5-63 `NavFollower`, takılma, F5-57 sözleşmesi (`TASLAK`) · F5-64 bütçe + `NAV_*` telemetri (`TASLAK`) · F5-65 nav durumu temizliği (`TASLAK`) · F5-66 çalışma zamanı doğrulama (`TASLAK`) · F5-67 su katmanı düzeltmesi (koşullu). **F6-01..F6-05 bu dilimlere bağımlı değildir; F6-06 ve ardılları F5-59..F5-66'nın tamamına bağımlıdır.** `NavService` API'si bugün (F5-59) yalnızca `Startup/Shutdown/Ready/Grid/Info`; yol/takip API'si F5-62/F5-63'te yazılacağından F6-06'daki nav çağrı adları **geçicidir**.

## 2. Önerilen uygulama sırası

```
                 (kuyruk: ... F5-66 -> F8-05 ->)
F6-01 ──┬── F6-02 ───┐
        └── F6-04 ───┼── F6-03 ── F6-05 ── F6-06 (G6a) ──┬── F6-07 ── F6-09
                     │                                      ├── F6-08          (G6b/G6c paralel)
                     └────────────────────────────────────  └── F6-10 (+F8-05 ile EVAL-1v1)
```

- F6-01 sonrası **F6-02 ve F6-04 paralel** yazılıp uygulanabilir (ortak dosya yok; ikisi de yalnızca `BotCore.vcxproj`/`BotCoreTests.vcxproj` satırı ekler: birleştirmede satır çakışması beklenir, el ile çözülür).
- F6-03, F6-02'ye bağlıdır (`TargetChoice`); F6-05 eylem kısmı F6-03'ün `WarSkillId`'ine bağlıdır (gerekirse F6-05 eylem kısmı ayrı plana bölünür).
- **F6-06 kritik yoldur ve dış blokaja sahiptir:** F5-59..F5-66 `KAPANDI` olmadan başlamaz (kuyrukta zaten önünde). F6-01..F6-05 bu dilimleri **beklemeden** `bot/F6-0N` olarak uygulanabilir (saf mantık); proje sahibinin kuyruğu F6'yı F8-05'ten sonraya koyduğundan sıra onayı gerekir.
- F6-07 ve F6-08, G6a oyun içi kabulü geçtikten sonra (aynı yürütme hattı doğrulanmış) paralel koşulur; ikisi de `PolicyL0.h` ve `BrainDriver.cpp`'ye **ekleme** yaptığından birleştirme çakışması beklenir: sıralı birleştirme önerilir.
- F6-10, resmî EVAL-1v1 için **F8-05**'i bekler (kuyrukta önde); kod/birim ve duman testi beklemez.

## 3. Dış plan/kimlik blokajları (2026-10-03 `7891f74` üzerinde doğrulandı)

| Dış plan | Durum | Kim bloklanır | Not |
|---|---|---|---|
| F4-53 gözlenen durum tablosu (saf mantık) | **`KAPANDI`** (merge `0001d04`) | — (F6-01 `SkillSpec` `SkillMeta`'yı içerir; F6-09/F6-02 `ObservedStatusTable`'ı okur) | Type3 DoT/HoT kaydı kapsam dışı: F6-09 Cure disease için kendi `DotRemaining` kestirimi |
| F4-60 gözlenen durum sunucu bağlaması | **`KAPANDI`** (`DOĞRULANDI`, merge `7891f74`) | — | `BotSession::m_status`/`m_healObs`, `FillSkillMeta` (`BotSession.cpp:8`), `/bot snap <bot> status` |
| **F4-61** (`UnitView` gözlenen durum alanları, görünürlük kaybı/yeniden giriş) | **plan dosyası yok**; kuyrukta F5-66'dan önce | F6-09 (isteğe bağlı: `statusKnown`), F6-02 (`TargetExtras`) | karar katmanı tabloyu doğrudan okur, F4-61 zorunlu değil |
| F4-55 giriş el sıkışması (KI-DEG-01 kalıcı düzeltme) | `HAZIR` (`bot/F4-55`) | F6-06/F6-10 çalışma zamanı (botlar ≥ 3 sn arayla doğurma geçici çözümü kalkar) | |
| F5-59 `NavService` | `HAZIR`; `bot/F5-59` dalı açık (`da21d40`) | F6-06+ | yalnızca yaşam döngüsü + `Grid()` |
| F5-61..F5-66 | `TASLAK` (plan dosyası yok, yalnızca `plans/F5-55` §1A tablosu) | F6-06+ | **dilim boşluğu:** `NavRetreatPlanner`, `NavReachJudge` iletimi, `directClear` sorgusu hiçbir satırda yok |
| **F8-05** (`ScenarioReset`/KI-DEG-05) | **plan dosyası yok**; proje sahibi kuyruğunda F6-01'den önce | F6-10 (resmî EVAL-1v1); F6-06 elle sıfırlama ile ilerler | NP/konum/envanter sıfırlama + `win_rule`; `db/005`/`db/006` (F8-03/F8-04) ayrı |
| F7 summon planı (SUM-01..07, `READY_FOR_SUMMON`) | yok (kapsam dışı) | — | F6-08 summon'u dışarıda bırakır |
| T-MECH-POT-03/04/05, T-MECH-BUF-03, T-MECH-CLIENT-03 | insan ölçümü bekliyor | F6-04, F6-09, F6-07/F6-08 | proje sahibi oturumu |

## 4. docs ile docs/kod arası bulunan çelişkiler (özet; ayrıntı her planın §8'inde)

1. **Summon F6/F7:** `docs/17` F6 "kapsam dışı: summon; mage summon hariç", F7 "summon akışı (08 §8)", G7b; F4-34/ADR-0018 Ek 10 "güvenlik kapıları F7". Görev tanımı F6-08'de "güvenli summon" der. Karar: **F7** (F6-08).
2. **Geri çekilme eşiği:** `docs/11` §4.1 "HP %30" (kullanıcı gereksinimi) ↔ §4.2 formülü (`0,30 + role_adj + 0,05·threat − 0,08·support + 0,1·[kök]`; 1v1 melee yakınken 0,35); `P-POT-HP-EMERG` 0,35; `docs/14` aralığı 0,20–0,40 yalnızca tabanı sınırlar; `P-SUR-THREAT-WEIGHT`'in mage ağırlığına uygulanıp uygulanmadığı belirsiz. **Proje sahibine soru.** Ek: `docs/11` REENTER 0,65 ↔ `docs/10` `P-SOLO-RECOVER-HP` 0,85; solo hedef "kendi tower halkası" ↔ ADR-0033-DEG "arena içi kendi ulus yarısı" (nokta tanımsız, `[A]` 40 m).
3. **B0-NAIVE:** `docs/15` §5 "pot %30'da" (HP/MP?), priest/mage için "en yakın düşmana saldırır/rastgele hazır skill" tanımsız; `docs/17` "EVAL-1v1'de **anlamlı** yener" tanımsız (AC-EVAL-01 SPRT +100 Elo yalnızca 8v8); `baseline-v1` dondurma F8'dir ama `B0-NAIVE` F6 kod teslimatıdır (F6-01 tanımladı).
4. **FSM:** `docs/13` §6 ↔ `docs/10` §2 (EVALUATE/AVOID/DUEL/CHASE) ↔ `docs/06` §7 (APPROACH/PRESSURE/PURSUE/PEEL); diyagramda `ENGAGE→ROAM`, `COMBAT→ROAM`, `ENGAGE→RETREAT`, `COMBAT→ENGAGE` yok.
5. **Melee menzil:** `docs/06` §3 `15 + silah.Range − marj` ↔ bot guard'ı `distance ≤ silah.Range` (Raptor 2,0 m, F4-45); nav ızgarası 4 m (halka ≥ 2,83 m) ⇒ son yaklaşma doğrudan adım.
6. **MP rezervi (`docs/06`):** §3 "rezervin altında yalnızca Carving ve R" ↔ §6.2 "Carving MP ≥ 90 + rezerv" ↔ "Scream MP ≥ 300 + rezerv" (çift sayım). F6-03 rezervi **korunan MP** okur. Ayrıca `docs/05` §5 "Scream Stone of Warrior tüketir" ↔ F4-45 ölçümü: Scream `UseItem 379063000` Scream Scroll (tüketilmez), Stone of Warrior Exceed Break'te.
7. **MET-TGT-01 ↔ arena:** p50 ≤ 4 sn, başlangıç 70 m iken fiziksel olarak imkânsız (~15,5 sn yürüme); T-IGT-WAR-01 başlangıcı ≤ 15 m olmalı ya da metrik yeniden tanımlanmalı.
8. **`docs/09` `R(t)`** takım payı tanımı tek bot/arena başlangıcında hep 0: solo için sürekli ufuk (F6-02 `[A]`).
9. **`docs/10` önsel tablo** W-P/W-G, M-F/M-I ayrımı gözlenemez (`UnitView` ekipman/profil taşımaz): sınıf ailesi ortalaması (F6-10 `[A]`); `kendi_kaynak_skoru`/`arazi`/`ekipman` terimleri tanımsız.
10. **`NO_ENGAGE` (`docs/15` §6b) ↔ AVOID:** EVAL-1v1'de `soloForceEngage`; T-SOLO-02 ayrı koşu.
11. **`docs/11` §3.4** "ayrı grup cooldown'ları" ↔ §3.1 tek ortak 2,5 sn; **docs/07**: tam canlı hedefe heal sunucuda `-100` (F4-42/F4-43) belgelenmemiş; **BUF-HP-01 ↔ buff matrisi** (priest için Undying vs massiveness).
12. **Faz sınırı kayması:** `docs/17` F6 kabul listesi AC-WAR-04 (peel), AC-SUR-05/T-SUR-04 (takım yönüne çekilme), AC-MAG-02 "W-G desteğiyle", T-PRI-07 "peel isteği" F7 öğeleridir (aynı AC-WAR-04 hem F6 hem F7 satırında); priest buff/cure/debuff AC-PRI-04..06 F7 kapısı G7a'dadır.
13. **Kod ↔ docs:** `BotManager`/`ActionExecutor` sürücüsü ad tabanlı ve hedef konumunu hedef botun `CUser`'ından (G sınıfı) okur (`BotManager.cpp:3199`, `:3259`); karar katmanı perception'dan beslenmelidir (F6-06 çözer). Algıda başkalarının `WIZ_ATTACK` yayını saklanmıyor (`BotSession.cpp:109-116`: yalnızca kendi yankısı; melee saldırgan atfı yok), düşman ekipmanı ayrıştırılmıyor, Type3 DoT/HoT gözlem tablosu yok.
14. **F5 dilim boşluğu:** güvenli nokta (`NavRetreatPlanner`), ulaşılamaz yargısının iletimi (`NavReachJudge`) ve `directClear` F5-59..F5-66 satırlarında yok (F5-62 yalnızca `NavReach` kurulumunu anar).

## 5. Claude'un docs güncelleme işleri (plan `HAZIR`a geçmeden)

`docs/16` §5.2 (yeni `ReasonCode` adları) · `docs/13` §6 (eksik dört geçiş, alt durum alanı) · `docs/06` (`P-WAR-MELEE-RANGE`, MP rezervi, Howling "ayakta"/KI-017, Scream eşyası) · `docs/09` (solo `R`) · `docs/10` (gözlenebilir önsel tablo, `NO_ENGAGE`) · `docs/11` (§3.4, acil kademe fallback, `RECOVER` çıkışı, arena anchor) · `docs/07` (tam canlı hedefe heal yok, BUF-HP-01) · `docs/17` (summon F7, AC-WAR-04/AC-SUR-05/AC-MAG-02 faz ayrımı, MET-TGT-01 başlangıç mesafesi) · `docs/21` §5 sahiplik tablosu (yeni `P-*` kimlikleri: `P-WAR-STANDOFF-M`, `P-WAR-NAV-HANDOFF-M`, `P-WAR-SPRINT-MIN-DIST`, `P-TGT-REACH-HORIZON`, `P-TGT-OWN-DPS`, `P-TGT-MAX-DIST`, `P-TGT-ABANDON-HOLD-MS`, `P-SUR-RECOVER-MP-*`, `P-SOLO-ROAM-R-M`, `P-SOLO-FORCE-ENGAGE`, `P-SOLO-POT-REF`).

## 6. Proje sahibine tek tek sorulacak kararlar (sade dille; cevaplar ADR'ye)

1. Geri çekilme eşiği **tam %30** mu, yoksa tehdit/destek/kök ile **kayan** (0,22–0,45) mi? (F6-05)
2. Geri çekilen bot **%65**'te mi dönsün, yoksa güvendeyse **%85**'e dek dinlensin mi? (F6-05)
3. `B0-NAIVE` "pot %30'da": yalnızca HP mi, MP de mi? priest/mage naive hangi skill'leri kullanır? (F6-01)
4. "L0, `B0-NAIVE`'i anlamlı yener" ölçütü: önerim "Wilson %95 GA alt sınırı > %50, ≥ 50 tekrar, taraf değişimli"; ve 1v1 kazanma kuralı `wipe_first` mi? (F6-10)
5. Summon F7'de kalsın mı? (F6-08; önerim: evet)
6. MET-TGT-01 (≤ 4 sn) için T-IGT-WAR-01 başlangıç mesafesi ≤ 15 m mi olsun? (F6-06)
7. F6-09 ve F6-10 kuyruğa (F6-01..F6-08'in yanına) eklensin mi? (kuyruk sahibi)

## 7. Doğrulanamayan noktalar

- F5-61..F5-66 ve F8-05 plan dosyaları yok; kapsamları yalnızca `plans/F5-55` §1A tablosuna ve `docs/STATUS.md:411` kuyruğuna dayanır. F4-61 planı yok.
- `NavService` yol/takip API adları (F6-06'daki geçici adlar) F5-62/F5-63 yazılınca eşlenecek.
- Raptor `ITEM.Range 20` = 2,0 m ve master skill `UseStanding 51..54` değerleri F4-45/KI-017 kayıtlarına dayanır; bu turda DB'ye bağlanılmadı, kod ve plan metinleriyle yetinildi.
- Pot değerleri 1440 HP / 2160 MP kademeleri için öğe kimlikleri/değerleri `docs/11`'de yalnızca ad olarak geçer.
- `MAGIC_TYPE3.bAttribute` (element) alan adı yazım turunda sunucu kaynağından doğrulanmalı (`SkillMeta` `bDirectType`/`sFirstDamage` mevcut, element yok).
- 260 test sayısı `git grep` sayımı ve STATUS kaydıdır, çalıştırılmadı. Hiçbir plan oyun içinde doğrulanmış değildir; "çalışma zamanı" maddeleri **planlanmıştır**, yapılmamıştır.
