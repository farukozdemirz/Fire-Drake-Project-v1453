# F6-05: Geri çekilme, iyileşme ve yeniden savaşa katılma: `RETREAT`/`RECOVER`/`REENTER` kararı (`BotCore/Survival.h`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; kapı G6a, T-IGT-SUR-01'in solo kısmı) |
| Branch | `bot/F6-05 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-01** (`StateMachine`, `Intent`, `ReasonCode`, `BrainParams`), **F6-04** (`IncomingEwma`, `PotChoice.outOfPots`) `KAPANDI`; F4-05 (`SetStance`, CLI-13), F4-07 (`Regene`, CLI-14) `KAPANDI`; F5-07 (`NavRetreatPlanner`: `NavRetreatStatus::NoCandidate` = `last_stand`), F5-06 (güvenlik/yasaklı bölge, `Safe` bayrağı), F5-51 (arena sınırı ve doğuş yolu), F5-09/F5-54/F5-57 (takılma tespiti/kurtarma) `KAPANDI` (saf mantık; sunucu bağlaması F5-55 dilimlerinde, F6-06'yı bloklar) |
| İlgili gereksinim / kabul | `docs/11` §4 (geri çekilme ve yeniden giriş), §5 (ölüm sonrası kaynak); T-SUR-01, T-SUR-03, (T-SUR-02 destek terimi); AC-SUR-01 (MET-SUR-06 = 0), AC-SUR-02 (geri çekilme başarısı ≥ %70, kaçış yolu varken); MET-SUR-01..07; ADR-0033-DEG (arena modu: güvenli nokta arenanın içinde); `docs/12` §8, §13.4; CLI-13 |
| Tahmini büyüklük | M (4 dosya: 1 yeni başlık, 1 yeni test dosyası, 2 proje satırı) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 yazım turu) |

---

> **Karar (2026-10-03, ADR-0019, proje sahibi):** geri çekilme eşiği **sabit %30** (`threatWeight = 0`), yeniden savaşa dönüş eşiği **%85** (`REENTER = 0,85`). Bu plandaki "proje sahibine sorulacak" / `[A]` ifadeleri bu karara göre okunur; `docs/11` §4.2 formülü ayrı bir kararla sonraya bırakıldı.

## 1. Amaç

Proje sahibi hedefinin kritik parçası: **HP %30'un altına inen bot güvenle geri çekilir; yeterince toparlanınca savaşa döner; salınım olmaz** (`docs/11` §4.1). Bu plan, `docs/11` §4.2 karar modelini (`retreat_hp`, `ttd`, `support`, `threat`, `last_stand`, `REENTER`) saf mantık olarak yazar; geri çekilme hedefi (güvenli nokta) nav'dan (`NavRetreatPlanner` sonucu `NavView`), eylemler (pot, sprint, restoration, oturma) mevcut aksiyonlardan gelir. Çıktı: istenen durum (`Retreat`/`Recover`/önceki) ve `Intent` parçaları.

## 1a. Neden TASLAK (HAZIR yapmak için)

Ön koşullar:

1. **F6-01 ve F6-04 `KAPANDI`**; F6-03 (warrior eylemleri: sprint, restoration, leg cutting kimlikleri) ile aynı `WarSkillId` yardımcısı kullanılır: F6-03 `KAPANDI` olmadan bu planın warrior eylem kısmı (§3 madde 6) yazılmaz; öncelik gerekirse eylem kısmı ayrı plana bölünür.
2. **Proje sahibi kararı gerekir (tek soru, sade dille):** geri çekilme eşiği *tam %30 mu*, yoksa `docs/11` formülündeki bağlama duyarlı eşik mi? Formül varsayılanlarında (`P-SUR-THREAT-WEIGHT 1,0`) 8 m içinde tek düşman melee varken eşik %35'e çıkar; `P-POT-HP-EMERG` de 0,35'tir (aşağıda "Çelişkiler" 1). Karar gelene dek plan **ikisini de** destekler: `threatWeight = 0` ⇒ tam `retreatHp` (0,30); T-SUR-01 ve uçtan uca hedef testi `threatWeight = 0` ile koşulur.
3. F5-55 dilimleri (`plans/F5-55-...md` §1A, 2026-10-03): **F5-59** `NavService` yaşam döngüsü ve harita yükleme (`HAZIR`; `bot/F5-59` dalı açık), **F5-61** kiriş guard'ı, **F5-62** `/bot goto` + waypoint zinciri + `NavPathfinder`/`NavReach`/maliyet katmanı kurulumu, **F5-63** `NavFollower`/takılma/F5-57 sözleşmesi, **F5-64** bütçe + `NAV_*` telemetri, **F5-65** nav durumu temizliği, **F5-66** çalışma zamanı doğrulama (F5-61..F5-66 `TASLAK`; F5-60/F5-67 yalnızca su denetimi/koşullu düzeltme): `NavRetreatPlanner` sonucu + arena anchor bağlamasının `NavView.safePointFound/safeX/safeZ/safePathM/lastStand`'i doldurması gerekir. Bu plan alanları **tüketir**. **Dilim boşluğu (2026-10-03 kontrolü):** `NavRetreatPlanner` (güvenli nokta) çağrısı, `NavReachJudge` sonucunun karar katmanına iletilmesi ve `directClear` (düz kiriş) sorgusu bu dilimlerin satırlarında **yazılı değildir** (`plans/F5-55` §1A; F5-59 §3 "kapsam dışı" yalnızca `NavReach` kurulumunu F5-62'ye bırakır). İlgili dilim planı genişletilmeli ya da ek bir F5 dilimi (öneri, kimlik atanmadı) yazılmalıdır.
4. Arena kenarı tanımı (solo "kendi ulus yarısı", ADR-0033-DEG) için dayanak noktası belirsiz: bu plan `[A]` olarak "arena merkezinden kendi doğuş noktasına doğru 40 m" tanımlar; proje sahibi/Claude onaylar.

Yazım turunda yeniden doğrulanacak referanslar: `BotCore/NavRetreat.h:28-108` (`NavRetreatMode`, `NavRetreatStatus`, `NavRetreatQuery.hasAnchor/anchorX/anchorZ/threats`, `NavRetreatPlanner::Find`), `BotCore/Perception.h:1098-1140` (`BuffView.buffType/isBuff`, `SelfState.sitting`, `hpPotStock`), `:1454-1500` (`TeamMemberView.hp/mp/cls/inView/dist`), `BotCore/BotCombat.h:648-683` (`CheckStance`: yalnızca durmuşken ve yürüme/saldırı/cast serisi yokken oturma, ardışık duruş ≥ 1,0 sn), `docs/03` §13.2 CLI-13 ölçümü (oturunca +296 HP ≈ %5,2 / 5–6 sn), `docs/03` Ek A (BuffType 6 SPEED, 28 Wall of Iron, 40 SPEED2, 47 STUN, 152 SILENCE, 153 NO_POTIONS), `docs/15` §2.4 (arena A (1274, 890), r = 60 m, başlangıç ±35 m; Karus doğuş (1380-1390, 1090-1100), El Morad (630-640, 920-930)).

## 2. Bağlam (okunması zorunlu)

- `docs/11` §4.1 (temel kural: kullanıcı gereksinimi; "iki sabit eşik değil, histerezisli ve bağlama duyarlı bir karar"), §4.2 (modeli: `ttd`, `support`, `threat`, `retreat_hp`, `RETREAT`, `REENTER`; parametreler `P-SUR-RETREAT-HP 0,30`, `role_adj` (mage +0,10, priest +0,05, W-G −0,05, W-P 0), `P-SUR-REENTER-HP 0,65 (≥ RETREAT-HP + 0,25)`, `P-SUR-THREAT-WEIGHT`, `P-SUR-MIN-RETREAT-TIME 3 sn`), §4.3 (hareket + destek aksiyonları; **mod ayrımı ADR-0033-DEG**: arena modunda güvenli nokta arenanın içinde), §4.4 (`last_stand`), §5 (respawn MP'yi doldurmaz; `RECOVER`'da MP rol eşiğine potla tamamlanır: warrior %50, priest %70, mage %70).
- `docs/10` §2 (solo durum şeması: `DUEL -> RETREAT -> RECOVER -> ROAM`; `RECOVER`: güvenli noktada pot ve heal; **oturmak yalnızca 60 m içinde düşman yokken**, `MEC-DTH` +296 HP / 6 sn), §3.3 (`P-SOLO-RECOVER-HP/MP` %85 / %70), §5 (kaçarken yavaşlatma yalnızca **takipçiye**).
- `docs/13` §6 (FSM: `COMBAT -> RETREAT -> RECOVER -> ROAM`), §13 (belirlenim). `docs/12` §8 (güvenli nokta: arena modunda arenanın içinde; party: arka hat; solo: kendi ulus yarısı), §13.4 (arena sınırı, geri çekilme).
- `docs/16` MET-SUR-01 (≥ %70; başlangıçtan `P-SUR-REENTER-HP`'ye ölmeden ulaşma), MET-SUR-02/03/04/05, **MET-SUR-06** (10 sn içinde ≥ 3 `COMBAT <-> RETREAT` = 0), MET-SUR-07 (başarısız savaşa dönüş ≤ %20); `STATE_CHANGE` olayı.
- `docs/06` §4 (karar önceliği: 1. ölüm önleme/geri çekilme, 2. kritik pot — **aynı tick'te ikisi birlikte olabilir**), `docs/07` §4 madde 1 (geri çekilirken kendine heal; F6-07).
- `ADR-0033-DEG` (arena/serbest mod ayrımı: serbest Ronark'ta tower halkası/düşmansız bölge F11'dedir ve bu planın kapsamı **dışındadır**).

## 3. Kapsam

**Yapılacaklar** (`BotCore/Survival.h`; yalnızca `BotCore/*.h`; global/static değişken yok; dinamik bellek yok)

1. **Tipler:** `struct SurvivalCtx { float support; bool rooted; bool slowed; bool cureSoon; bool enemyWithin60m; }`; `struct RetreatEval { float ratio; float retreatHpEff; float ttdSec; float threat; bool retreat; bool lastStand; ReasonCode reason; }`; `struct SurvivalMemory { uint64_t retreatSinceMs; uint64_t recoverSinceMs; bool lastStandSticky; uint64_t lastEnemyWithin60Ms; }`.
2. **Girdi türetmeleri (saf):**
   - `ComputeThreat(snap, nowMs)` = `melee + 0,5 * mage`: `melee` = 8 m içindeki canlı düşman warrior/rogue sayısı (`RoleFamilyOfClass`); `mage` = 45 m içinde, son 3 sn'de **bize** yönelik `WIZ_MAGIC_PROCESS` olayı (`SkillEventRing`) olan düşman mage sayısı (`docs/11` §4.2: "bize yönelik olay").
   - `ComputeSupport(self, team, nowMs)` = 1 eğer party'de **canlı priest** üye (`cls % 100` 4/11/12), `inView`, `dist <= 56` (heal menzili `[A]`) ve `mp >= 960`; rezervasyon **bize ya da serbest** koşulu F7'ye kadar hep sağlanmış sayılır `[A]`; aksi 0. Solo'da 0.
   - `ComputeRootSlow(self)` = `SelfState.buffs` içinde `isBuff == false` ve `buffType` ∈ {6, 40, 47} (yavaşlatma/stun/kök); `silenced` (152) ve `noPotions` (153) ayrı bayraklar (pot/skill kapıları F6-04/F6-03).
   - `ComputeEnemyWithin(snap, 60 m)` (`RECOVER` oturma kapısı).
3. **Karar** `RetreatEval EvaluateRetreat(const DecisionInput & in, const SurvivalCtx & ctx, float incomingRateHpPerSec, SurvivalMemory & mem)` (`docs/11` §4.2):
   - `ratio = hp / maxHp`; `ttd = hp / max(0,001, incomingRate)` (sn); `retreatHpEff = retreatHp + roleAdj(role) + 0,05 * threatWeight * threat - 0,08 * support + 0,1 * (rooted || slowed)`; üst sınır **uygulanmaz** (formül çıktısı; belirsizlik "Çelişkiler" 1 ve 2).
   - `escapeOk = nav.safePointFound && !ctx.rooted` ( `docs/11` `escape_ok`; stun/kök altında kaçış yok).
   - `lastStand = nav.lastStand || (ctx.rooted && !ctx.cureSoon) || (ttd < 2,0 s && !escapeOk)`; **sticky**: `lastStand` bir kez true olursa HP `reenterHp`'ye çıkana dek true kalır (salınım önleme) `[A]`.
   - `retreat = (ratio < retreatHpEff && !lastStand) || (ttd < 2,5 && support == 0 && escapeOk)`; gerekçe `RuleRetreatHp` ya da `RuleRetreatTtd`; `lastStand` ise `RuleLastStand` (geri çekilme **yok**; orkestratör pot + savunma skill'i + en düşük HP'li ulaşılabilir hedefe tam saldırı).
   - `role_adj` ve `ttd` eşikleri kodda sabit (`docs/14` §4.3 ruhu).
4. **Durum geçişi** `BotState SurvivalStep(const DecisionInput &, BotState current, const RetreatEval &, SurvivalMemory &, const SurvivalCtx &, bool outOfPots)` (istenen durumu döndürür; geçişi `StateMachine::Request` yapar, minimum bekleme orada):
   - `Engage`/`Combat` + `retreat` ⇒ `Retreat`.
   - `Retreat`: güvenli noktaya varıldı (`dist <= 3 m` ya da yol bitti) **ve** `DwellMs >= minRetreatMs` (3 sn) ⇒ `Recover`; yolda ölüm ⇒ `Dead` (kendi kendine geçiş).
   - `Recover` ⇒ `Roam` yalnızca **çıkış kapısı** sağlanınca: `ratio >= reenterHp` (0,65) **ve** `mp/maxMp >= recoverMp(role)` (`P-SUR-RECOVER-MP-WAR/PRI/MAG`: 0,50 / 0,70 / 0,70) **ve** (`enemyWithin60m` **ya da** (`ratio >= P-SOLO-RECOVER-HP` (0,85) ve `mp/maxMp >= P-SOLO-RECOVER-MP` (0,70))): yani düşman yakında baskı varken erken yeniden giriş (0,65), güvendeyken tam dinlenme (0,85) `[Ö]` ("Çelişkiler" 2).
   - **Yeniden giriş** `Roam -> Engage -> Combat` sonraki hedef seçimi (F6-02) ve (F6-10) EV ile; bu plan `REENTER` kapısını (`RuleReenter`) ve `MET-SUR-07` için "yeniden girişten sonra 10 sn" sayacını (`mem`) sağlar.
5. **Hareket niyeti** `RetreatMove(in, mem)`: `Intent.move = { MoveKind::Retreat, nav.safeX, nav.safeZ, speedField (sprint etkinse 67 yoksa 45) }`; güvenli nokta yoksa (`!safePointFound`) `lastStand`. **Arena modu** (`nav.arena.active`): `RetreatAnchor(arena)` = `arena merkezi + 40 m * birim(arena merkezinden ownBase'e)` (`NavRetreatQuery.hasAnchor/anchorX/anchorZ` için; "kendi ulus yarısı", dayanak ADR-0033-DEG'de tanımsız `[A]`); serbest mod (kendi tower halkası) **uygulanmaz** (F11).
6. **Eylemler** (yuva dağılımı): geri çekilirken `pot` (F6-04, aynı tick, ayrı yuva), `cast`: warrior için sprint (yoksa ve `dist(takipçi) > 4 m`), `restoration` (`HP < %70` ve etkin değil), takipçiye leg cutting (yalnızca ≤ 2,0 m'deki **en yakın takipçiye**, hazırsa ve son 10 sn'de uygulanmadıysa; kaçış hareketini bozmaz: CastTime 0); mage/priest eylemleri F6-08/F6-07'de (`Survival.h` yalnızca "izinli eylem" bayraklarını verir: `allowSelfHeal`, `allowManaShield`).
7. **`RECOVER` eylemleri:** HP < `P-SOLO-RECOVER-HP` ve pot yoksa (`outOfPots`) ve `!enemyWithin60m` ve duruyor (`!moving`, hareket/saldırı/cast serisi yok) ve `!sitting` ⇒ `Intent.stance = { active, sit = true }` (CLI-13: ≥ 1 sn aralıklı, yürüme/saldırı/cast yokken); düşman 60 m'ye girerse ya da çıkış kapısı sağlanırsa `sit = false` (kalk). Pot önceliklidir (F6-04: sit yalnızca pot yokken; oturma yenilenmesi +%5,2/5–6 sn pota göre çok yavaş). Respawn sonrası MP doldurma (`docs/11` §5): `Respawned` durumunda `mp/maxMp < recoverMp(role)` ⇒ MP potu (F6-04 `mpNeedSoon` ipucu = eşiğe kadar eksik).
8. **MET-SUR-06 koruması:** `StateMachine` min süreler + histerezis (`reenterHp - retreatHp >= 0,25`, `BrainParams` doğrular) + `lastStand` sticky; `Oscillations()` 0 beklenir.
9. Birim testleri (§5.3).

**Kapsam dışı (yapılmayacak)**

- **Serbest Ronark geri çekilmesi** (kendi tower halkası, dost konumu, düşmansız bölge; yeniden gruplanma; savaşa dönüş arama): F11-a/b/c (`docs/17` F11), ADR-0033-DEG.
- Party geri çekilmesi (takım arka hattı, `Takımın tersine kaçmaz`, T-SUR-04, AC-SUR-05, `TeamPlan` modu): F7 (`TeamBlackboard`). Bu planın `ComputeSupport` terimi yalnızca `TeamView`'dan (priest üye HP/MP/mesafe) beslenir; F7 yoksa rezervasyon "serbest" varsayılır.
- Priest/mage'e özgü geri çekilme eylemleri (kendine heal, Mana Shield, kiting): F6-07/F6-08.
- `ActionExecutor`, `BotSession`, nav bağlaması, telemetri yazımı: F6-06 ve F5-55 dilimleri. Doğuş sonrası yürüyerek dönüş/summon: F6-06 (yürüme) ve F7 (summon).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Survival.h` | yeni | §3 |
| `Tests/BotCoreTests/SurvivalTests.cpp` | yeni | §5.3 |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca bir `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca bir `ClCompile` satırı |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-05 gece/2026-10-02`; F6-01/F6-03/F6-04 birleşmiş mi doğrula; `Durum` → `UYGULANIYOR`.
2. `Survival.h` yaz (§3). Zamanlama bilgisini `StateMachine` (F6-01) tutar; bu başlık ikinci bir durum zamanlayıcısı kurmaz.
3. `SurvivalTests.cpp` (adlar **sabit**; sentetik `DecisionInput`):
   - `Surv_Retreat_Threshold_Exact30`: `threatWeight = 0`, destek 0, kök yok: `ratio = 0,2999` ⇒ `retreat`; `0,3001` ⇒ yok (W-P, `maxHp 5650`).
   - `Surv_Retreat_RoleAdj`: mage 0,40; priest 0,35; W-G 0,25; W-P 0,30.
   - `Surv_Retreat_ThreatAndSupportTerms`: tek melee 8 m içinde ⇒ +0,05; mage olayı +0,025; destek 1 ⇒ −0,08 (`retreatHpEff` birebir); `threatWeight` 0 ⇒ terim 0.
   - `Surv_Retreat_RootSlowTerm`: `BuffType 6` debuff ⇒ +0,10; buff olan 6 ⇒ etkisiz.
   - `Surv_Retreat_TtdRule`: `ratio` yüksek ama `ttd = 2,4 s`, destek 0, `escapeOk` ⇒ `RuleRetreatTtd`; destek 1 ⇒ geri çekilme yok; `escapeOk == false` ⇒ `lastStand`.
   - `Surv_LastStand_NoCandidate_Sticky`: `nav.lastStand` ⇒ `RuleLastStand`, `retreat == false`; HP `reenterHp`'ye çıkana dek sticky.
   - `Surv_Reenter_Hysteresis_065`: `Recover`'dan çıkış `ratio 0,649` ⇒ yok; `0,65` ve düşman 60 m içinde ⇒ `Roam`; düşman yok iken `0,85`'e dek `Recover`.
   - `Surv_MinRetreatTime_3s`: `Retreat`'te 2999 ms'de `Recover` istenmez; 3000 ms'de varış varsa geçer.
   - `Surv_Sim_FixedDamage_T_SUR_01`: 180 sn sentetik (sabit 120 HP/sn hasar kaynağı 25 sn açık/25 sn kapalı, `threatWeight 0`, pot kapalı/açık iki varyant): geri çekilme `ratio < 0,30`'da başlar (≤ 1 tick sonra), yeniden giriş `ratio >= 0,65`'te, `Oscillations(10 s) == 0` (AC-SUR-01), ölüm yok.
   - `Surv_Recover_SitOnlyWhenSafe`: pot yok + 60 m içinde düşman yok + duruyor ⇒ `stance.sit`; 60 m içinde düşman ⇒ oturma yok/kalk; hareket serisi varken oturma yok (CLI-13).
   - `Surv_Support_FromTeamView`: canlı, `inView`, ≤ 56 m, `mp >= 960` priest ⇒ 1; MP 959 ⇒ 0; ölü/uzak ⇒ 0; solo ⇒ 0 (T-SUR-02 terimi).
   - `Surv_RetreatMove_And_Anchor`: güvenli nokta ⇒ `MoveKind::Retreat` ve sprint/pot/restoration yuvaları; `!safePointFound` ⇒ `lastStand`; arena modunda anchor = merkez + 40 m (kendi doğuş yönü).
   - `Surv_Deterministic_NoRng`: aynı girdi dizisi ⇒ aynı çıktı; `Rng` çağrılmaz.
4. Proje satırlarını ekle; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; K2: `Debug` rc=0
- [ ] K3: `./tools/run-tests.sh Release|Debug` `0 failed`; on üç yeni test adı `[ OK ]`; mevcut testler değişmeden geçer
- [ ] K4: `Survival.h`'te `windows.h|stdafx|GameServer|shared/` yok; `new|malloc|rand(`, global/static değişken yok; ASCII + CRLF
- [ ] K5: formül `docs/11` §4.2 ile birebir (katsayılar 0,05 / 0,08 / 0,1; `ttd` 2,5 sn; `role_adj` tablosu); `threatWeight = 0` iken eşik tam `retreatHp`
- [ ] K6: `Surv_Sim_FixedDamage_T_SUR_01` salınım 0 (MET-SUR-06), ölüm 0, geri çekilme/yeniden giriş eşikleri 0,30/0,65
- [ ] K7: `Survival.h` serbest Ronark (tower halkası) kodu **içermez** (arena modu tek mod; `grep -n tower BotCore/Survival.h` yalnızca yorum)
- [ ] K8: oturma yalnızca CLI-13 koşullarında (testle); pot oturmadan önceliklidir
- [ ] K9: `git diff --stat gece/2026-10-02...bot/F6-05` yalnızca §4'teki dosyalar; `GameServer/`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K10 (Claude): **proje sahibine tek soru** (geri çekilme eşiği: tam %30 mu, bağlama duyarlı formül mü) ve **ikinci soru** (`RECOVER` çıkışı: tek 0,65 mi, iki kademeli mi); cevaplar ADR'ye yazılır; `docs/11` çelişkileri düzeltilir; arena "kendi ulus yarısı" dayanağı tanımlanır
- [ ] K11 (çalışma zamanı, bu planda yok): T-SUR-01 (`MET-SUR-01 ≥ %70`, `MET-SUR-06 = 0`), T-SUR-03 (kök altında son direniş) ve uçtan uca "HP < %30 geri çekil → iyileş → yeniden katıl" **F6-06**'da Claude koşar; T-SUR-02 (priest desteğiyle erteleme) **F6-07** koşusunda; T-SUR-04/AC-SUR-05 F7

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Surv_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F6-05
grep -nE "windows\.h|stdafx|GameServer|shared/" BotCore/Survival.h
grep -n "tower" BotCore/Survival.h
git diff --check gece/2026-10-02...bot/F6-05
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. **Bota avantaj yok:** geri çekilme ışınlanma/teleport kullanmaz (`TEST_TELEPORT` eval modunda maçı geçersiz kılar, AC-NAV-05); yalnızca yürüme + sprint + normal pot.
- Eşikler `[Ö]`; L1 aralıkları `docs/14` §4.1 (`P-SUR-RETREAT-HP` %20–%40, `REENTER` `RETREAT+25`–%90).
- Çelişkiler/belirsizlikler (yazım turunda Claude çözer):
  1. **%30 ↔ formül:** `docs/11` §4.1 "HP %30'un altına inen bot geri çekilir" (kullanıcı gereksinimi) ↔ §4.2 `retreat_hp = 0,30 + role_adj + 0,05·threat − 0,08·support + 0,1·[kök]`: 1v1'de melee bir düşman 8 m içindeyken eşik 0,35'tir (W-P). `P-POT-HP-EMERG` (0,35) ile aynı değerdir. `docs/14` §4.1 `P-SUR-RETREAT-HP` aralığı 0,20–0,40 yalnızca **tabanı** sınırlar; formül çıktısı 0,40'ı geçebilir. `P-SUR-THREAT-WEIGHT`'in "0,05 ve 0,5 katsayılarının çarpanı" ifadesi mage ağırlığına da uygulanırsa çift sayım olur: bu plan `0,05 * W * (melee + 0,5 * mage)` okur `[A]`.
  2. **İki yeniden giriş eşiği:** `docs/11` `P-SUR-REENTER-HP` 0,65 ↔ `docs/10` `P-SOLO-RECOVER-HP` 0,85 (`RECOVER -> ROAM`). Bu plan ikisini birleştirir (§3 madde 4); T-SUR-01 "%65'te dönüş" ile uyumlu.
  3. **Solo geri çekilme hedefi:** `docs/11` §4.3 solo "kendi tower halkasına doğru" (halka arenadan 233 m / 640 m dışta) ↔ ADR-0033-DEG arena içi "kendi ulus yarısı" (noktası tanımsız: `[A]` 40 m).
  4. **Başarı ölçütü:** AC-SUR-02 (`MET-SUR-01 ≥ %70`, "kaçış yolu varken") eşit melee rakibi (W-P ~470 hasar/sn, `docs/05` §5.1) yürüyen/sprint eden botu 6,75/10 m paket adımlarıyla kovalarken **ulaşılamaz olabilir**; geçerli ölçüm "sabit hasar" (T-SUR-01) senaryosudur. EVAL-1v1'de geri çekilme başarısı rakibin kovalamasına bağlıdır.
  5. `docs/17` F6 kabul listesi AC-SUR-05 / T-SUR-04'ü (party üyesinin takım yönüne çekilmesi) içerir; bunlar `TeamPlan`/arka hat tanımı gerektirdiğinden F7'ye bırakıldı.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
- Kabul kriterleri öz-değerlendirme:
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar:
- İncelenen:
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | | |

- Bulgular (önem sırasıyla):
- Düzeltme talimatı (DeepSeek'e aynen verilecek):
