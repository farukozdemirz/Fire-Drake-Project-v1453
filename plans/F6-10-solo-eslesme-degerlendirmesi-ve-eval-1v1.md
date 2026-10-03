# F6-10: Solo eşleşme değerlendirmesi (EV), kaçınma/takip sınırı, dolaşma ve EVAL-1v1 koşusu (`BotCore/SoloEval.h`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; **faz kabul testi: "L0 politikası `B0-NAIVE`'i EVAL-1v1'de anlamlı yener"**) |
| Branch | `bot/F6-10 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-01..F6-06** `KAPANDI` ve G6a çalışma zamanı kabulü; rol planları: F6-07/F6-09 (priest) ve F6-08 (mage) `KAPANDI` olmalı (6 profilin tamamı koşulabilsin; warrior-only alt küme için yalnızca F6-06 yeter); F4-51 (rakip HP gözlemi), F4-52 (`SkillEventRing`), F4-06 (`TargetHpReq`); F5-06/F5-07/F5-51 (yasaklı bölge, güvenli nokta, arena sınırı), F5-55 dilimleri (`plans/F5-55-...md` §1A, 2026-10-03): **F5-59** `NavService` (`HAZIR`), **F5-61** kiriş guard'ı, **F5-62** waypoint zinciri + `NavReach`/maliyet katmanı kurulumu, **F5-63** `NavFollower`/takılma, **F5-64** bütçe + `NAV_*` telemetri, **F5-65** nav durumu temizliği, **F5-66** çalışma zamanı doğrulama (F5-61..F5-66 `TASLAK`); F4-55 (karşılıklı görünürlük, `HAZIR`); F8-01 (`tools/bot-outcome-eval.py`, `KAPANDI`) sonuç hesabı için. **EVAL-1v1 resmî koşuları F8-05'e (ScenarioReset + `win_rule`; proje sahibi kuyruğunda F6-01'den **önce**: `docs/STATUS.md:411`; plan dosyası henüz yok) ve KI-DEG-05'e bağlıdır:** bu plan onları **beklemeden** kod/birim kısmını tamamlar; resmî EVAL-1v1 kabulü F8-05 `KAPANDI` olmadan yapılmaz |
| İlgili gereksinim / kabul | `docs/10` (tamamı); T-SOLO-01..05 (T-SOLO-06 L0.5 uyumu kapsam dışı), EVAL-1v1-<A>-<B> (36 ikili, taraf değişimli); **AC-SOLO-01..05**; `docs/17` F6 kabulü ve G6a/G6b/G6c'nin solo koşulları; `docs/15` §5 (`B0-NAIVE`), §6a/§6b (`ScenarioReset`, `win_rule`), ADR-0031-DEG/ADR-0032-DEG; MET-OUT-01/05, MET-SUR-*; AC-NAV-06 (düşman tower halkası girişi 0) |
| Tahmini büyüklük | M (6 dosya: 1 yeni başlık, 1 yeni test, `PolicyL0.h` ve `BrainDriver.cpp` eklemeleri, 2 proje satırı; EVAL koşusu Claude'da) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 yazım turu) |

---

> **Karar (2026-10-03, ADR-0020, proje sahibi):** EVAL-1v1 kabul eşiği: L0 politikası `B0-NAIVE`'e karşı **40 maçta en az %65** (beraberlik yarım galibiyet). Plandaki eşik/"anlamlı" belirsizlikleri bu karara göre okunur; `ScenarioReset` (F8-05) bağımlılığı sürer.

## 1. Amaç

`docs/10`'un asıl iddiasını gerçekleştirmek: solo bot **her eşleşmeyi kazanmaya çalışmaz; kazanma olasılığı ile riski tartar**. Bu plan (a) eşleşme önsel tablosu ve `p_win`/EV formülünü, (b) `ROAM -> EVALUATE -> ENGAGE/AVOID -> DUEL -> CHASE/DISENGAGE` akışını ortak FSM'in alt durumları olarak, (c) arena içinde dolaşma rotasını, takip sınırını ve sayısal dezavantajdan kopmayı, (d) priest'e özgü solo kapılarını, (e) `B0-NAIVE`'e karşı EVAL-1v1 koşusunu kurar. F6-06'da "hedef görünürse saldır" olan basit kapı bu planla EV kararına dönüşür.

## 1a. Neden TASLAK (HAZIR yapmak için)

Ön koşullar:

1. **F6-01..F6-06 + G6a kabulü** ve en az bir caster rol planı (EVAL-1v1'in 6 profili için F6-07/08/09 hepsi); warrior-only eşleşmeler F6-06 ile başlayabilir.
2. **EVAL-1v1 resmî koşusu için F8-05** (`ScenarioReset`: konum/HP/MP/NP/envanter sıfırlama ve doğrulama; `win_rule` türleri; plan dosyası yok; kimlik proje sahibi kuyruğundan, `docs/STATUS.md:411`) `KAPANDI` olmalı (KI-DEG-05: `ScenarioRunner` maç başlangıcında durumu sıfırlamıyor; kuyrukta F6'dan önce geldiğinden bu ön koşulun F6-10 zamanına kadar sağlanmış olması beklenir). Aksi halde yalnızca **duman testi** (elle sıfırlama) yapılır ve kabul sayılmaz.
3. **Proje sahibi kararları (tek tek sorulacak):** (a) "L0 `B0-NAIVE`'i EVAL-1v1'de **anlamlı** yener" ölçütü: önerim: her profil için L0 vs `B0-NAIVE` (aynı profil, taraf değişimli, ≥ 50 tekrar) kazanma oranı Wilson %95 GA **alt sınırı > %50**; 36 ikilinin tamamı için ayrı eşik gerekmez; (b) 1v1'de `win_rule`: `wipe_first` (respawn kapalı, 1v1 = ilk ölen kaybeder, `docs/15` §6b) mi `killdiff_timed` mi; (c) `NO_ENGAGE` (60 sn hasar yok ⇒ `invalid`) ile `AVOID`'in birlikteliği: EVAL'da `soloForceEngage = 1`.
4. **Algı sınırı:** `UnitView` düşman ekipmanını/profilini (W-P/W-G, M-F/M-I) taşımaz: önsel tablo **sınıf ailesi** düzeyine indirgenir (§3); ekipman terimi `a6` sıfır. Ekipman ayrıştırması istenirse ayrı F4 algı planı (bu planda yok).
5. KI-013 (NP 0 ⇒ `Regene` yok): maçlar arası NP sıfırlaması F8-05'in işidir.

Yazım turunda yeniden doğrulanacak referanslar: `docs/10` §2-§8 (özellikle §3.2 tablo, §3.3 katsayılar), `docs/15` §4.6 (EVAL-1v1: 6 profilin tüm ikilileri 36, tekrar L-1: 50), §6a (başlangıç yerleşimi arena merkezinden ±35 m), §6b (`wipe_first`, `engage_timeout_sec` 60), `plans/F8-01-bot-sonuc-degerlendirici.md` (`tools/bot-outcome-eval.py` girdi/çıktı), `tools/bot-outcome-eval/sample.jsonl`, `BotCore/Perception.h:1141-1168` (`UnitView.cls`, `hpKnown`), `BotCore/NavDanger.h` (tower halkası yasaklı), `docs/12` §7-§8, `docs/04` §5 (6 profil), `db/002_bot_characters.sql` (12 sabit karakter: ulus başına W-P, W-G, P-HD, P-HB, M-F, M-I), ADR-0031-DEG/ADR-0032-DEG metinleri.

## 2. Bağlam (okunması zorunlu)

- `docs/10` §1 (ilke: avantajlıyken girmek ve bitirmek, dezavantajlıyken kaçınmak veya zamanında çekilmek), §2 (durum şeması), §3.1 (girdiler), §3.2 (önsel tablo `[Ö]`, EVAL-1v1'de ölçülüp güncellenir), §3.3 (`p_win`, `EV`, `ENGAGE ⇔ EV ≥ P-SOLO-ENGAGE-EV`, AVOID → ENGAGE histerezisi +0,1; katsayılar `a0=0, a1=2,0, a2=1,5, a3=0,8, a4=1,2, a5=0,5, a6=0,6`; `V_kill=1, C_death=1,2, C_time=0,05`; `p_death_if_lose=0,6`), §4 (sınıf bazlı solo taktikleri), §5 (sayısal dezavantaj: `P-SOLO-OUTNUMBER`, geri çekilme yönü, kaçarken yavaşlatma yalnızca takipçiye, `last_stand`), §6 (rakibe uyum: **L0.5, kapsam dışı**), §7 (sözde kod), §8 (T-SOLO-01..06, AC-SOLO-01..05).
- `docs/13` §6 (ortak FSM), `docs/11` §4 (geri çekilme/yeniden giriş: F6-05).
- `docs/15` §4.6 (EVAL-1v1), §5 (`B0-NAIVE`), §6 (değerlendirme protokolü: seed, taraf değişimi, S1 ekipman, STK-01), §6a, §6b (`wipe_first`).
- `docs/16` §7 (geçersiz maç bayrakları), MET-OUT-01 (`draw = 0,5`, Wilson %95 GA), MET-OUT-05.
- `docs/12` §7 (karşı ulus tower halkası **girilmez**; AC-NAV-06), §13.4 (arena modu).
- `docs/14` §4.1-§4.2 (solo parametre aralıkları), §9 (rakip havuzu: `B0-NAIVE` dahil).

## 3. Kapsam

**Yapılacaklar** (`BotCore/SoloEval.h`; yalnızca `BotCore/*.h`; global/static değişken yok; dinamik bellek yok)

1. **Önsel tablo** (`docs/10` §3.2'nin **gözlenebilir** hâli: rakibin W-P/W-G ve M-F/M-I ayrımı algıda yok ⇒ sütunlar ortalama; **`[A]`**):

   | Kendi \ Rakip | warrior | priest | mage |
   |---|---|---|---|
   | W-P | 1,05 | 1,30 | 1,05 |
   | W-G | 0,95 | 1,10 | 0,95 |
   | Priest (P-HD/P-HB) | 0,65 | 1,00 | 0,80 |
   | M-F | 0,95 | 1,20 | 0,95 |
   | M-I | 1,05 | 1,10 | 1,00 |

   `inline float MatchupPrior(BotRole own, int oppFamily)`; rakip ailesi `RoleFamilyOfClass(UnitView.cls)` (rogue/diğer ⇒ 1,0).
2. **Eşleşme değerlendirmesi** `struct EvResult { float pWin; float ev; float terms[7]; }`, `EvResult EstimateEv(const DecisionInput &, const UnitView & opp, int extraEnemies, const BrainParams &)` (`docs/10` §3.3 harfi harfine): `p_win = sigmoid(a0 + a1*ln(prior) + a2*(ownHp% - oppHp%) + a3*own_resource - a4*extra + a5*terrain + a6*gear)`; `oppHp% = hpKnown && !hpStale ? hp/maxHp : 1,0` (bilinmiyorsa **bota avantaj yok**: kötümser varsayım); `own_resource = 0,5 * mp/maxMp + 0,5 * min(1, potStockToplam / soloPotRef)` (`docs/10` "kendi_kaynak_skoru" tanımsızdır: `[A]`); `extra` = rakip dışında 40 m içindeki canlı düşman sayısı; `terrain = 0` (arenada kule yakınlığı yok; tower yakınlığı bayrağı F11), `gear = 0` (ekipman gözlenemez). `EV = p_win*V_kill - (1 - p_win)*p_death_if_lose*C_death - C_time`.
3. **Karar** `SoloDecision DecideEngage(...)`: `ENGAGE ⇔ EV >= P-SOLO-ENGAGE-EV + roleAdj` (`roleAdj`: mage `P-SOLO-ROLE-ADJ-MAGE` +0,15; diğer 0); **AVOID → ENGAGE** yalnızca `EV >= eşik + P-SOLO-EV-HYST` (+0,1); `DUEL` içinde `EV < P-SOLO-DISENGAGE-EV` (−0,15) ya da `extra >= P-SOLO-OUTNUMBER` (1) ⇒ `DISENGAGE`. `soloForceEngage == 1` ise `AVOID` hiç üretilmez (EVAL-1v1 `NO_ENGAGE` koruması); `DISENGAGE` (sayısal dezavantaj) yine geçerlidir.
4. **Priest solo kapıları** (`docs/10` §4.3): P-HD yalnızca rakip HP < %50 **ve** destekçi görünmüyor (rakip çevresi 40 m'de başka düşman yok) ise ENGAGE; P-HB yalnızca **savunma** (son 3 sn'de bize hasar: HP düştü ya da bize cast olayı) ya da rakip HP < %35 ise; aksi ⇒ AVOID (kendi heal'i ve kaçışla). Warrior/mage: EV kararı.
5. **Alt durumlar ve ortak FSM eşlemesi** (docs/13 §6 ile uyum; `SubState`): `Roam`(ROAM) → `Evaluate`(ROAM içinde alt durum) → `ENGAGE` → `Duel`(COMBAT) → `Chase`(ENGAGE/COMBAT alt durumu: rakip kaçıyor) → `Disengage`(= ortak `RETREAT` durumu, `reason = RuleDisengage`: hareket F6-05 `RetreatMove`, HP koşulu aranmaz) → `RECOVER` → `ROAM`; `Avoid`(ROAM alt durumu: rakipten uzaklaş, kendi yarıya). `docs/10` §2 diyagramındaki `AVOID -> ROAM` koşulu "rakip uzaklaştı / görünmez": rakip > 60 m ya da ≥ 3 sn görünmez `[A]`.
6. **Dolaşma rotası** (`docs/10` `P-SOLO-ROAM-ROUTE`, arena modu): `RoamWaypoint(arena, i)` = merkez + `soloRoamRadiusM` (35 m) yarıçaplı çember üzerinde 8 nokta; sıradaki nokta 6 m içine girilince ilerler; yön başlangıçta `rng` ile (belirlenimli); rakip görülürse `EVALUATE`. **Serbest Ronark/bowl dolaşması F11** (canavar yoğun, takılma noktaları; T-NAV-12) — bu planda yok.
7. **Takip sınırı ve yasaklı bölge:** `CHASE` ⇒ `P-SOLO-CHASE-MAX` (60 m kat edildi ya da 10 sn menzile girilemedi) ⇒ `ROAM`/`DISENGAGE`; hedef düşman tower halkasına girdi (`inForbidden`) ⇒ **anında** bırakma (AC-SOLO-04: takip sınırı ihlali 0, halka girişi 0; yol planlayıcı halkaya dışarıdan girmez: F5-06, ek koruma karar katmanında).
8. **`L0Decide` entegrasyonu** (`PolicyL0.h`): F6-06'daki "hedef varsa `Engage`" yerine `Roam → Evaluate → (Engage | Avoid)`; `EV` bileşenleri `DECISION.options[].terms`'e (`docs/16` §4: `terms` ağırlıklarla) yazılır; `BrainDriver.cpp`: `ArenaInfo` (merkez, yarıçap, `ownBase`), `soloForceEngage` ve `soloPotRef` `/bot brain ... param` ile; EV telemetrisi.
9. **EVAL-1v1 koşu protokolü** (Claude; **F8-05 `KAPANDI` sonrası**): senaryo `EVAL-1v1-<A>-<B>`: Karus `A` vs El Morad `B` ve **taraf değişimi** (`A_E` vs `B_K`), seed listesi sabit, `repeat` ≥ 50 (L-1), `win_rule = wipe_first` (`respawn off`) ya da proje sahibinin seçtiği tür, `soloForceEngage = 1`, S1 ekipman, STK-01 envanter, başlangıç ±35 m (`ScenarioReset` doğrulaması %100); politikalar: **L0** (`/bot brain l0`) vs **B0-NAIVE** (`/bot brain naive`), ayrıca L0 vs L0 (ayna: AC-SOLO-01); sonuçlar `tools/bot-outcome-eval.py` (F8-01) ile Wilson %95 GA.
10. Birim testleri (§5.3).

**Kapsam dışı (yapılmayacak)**

- **T-SOLO-06 / L0.5 oturum içi uyum** (`docs/10` §6: kiter mage, pot spam'cı, kaçıp dönen warrior): AC listesinde yok; ayrı plan.
- Serbest Ronark: bowl dolaşması, çatışma arama, tower halkasına çekilme, ≥ 24 sa çalışma: F11 (ADR kapılı).
- Party/takım kararları: F7. `baseline-v1` dondurma, SPRT/Elo (8v8): F8. L1 parametre arama: F9.
- `ScenarioReset`/`win_rule` gerçeklemesi: F8-05.
- Düşman ekipman/profil ayrıştırması (algı), `a6` ve `a5` terimleri.
- `ActionExecutor`/`Telemetry` değişikliği.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/SoloEval.h` | yeni | §3 |
| `Tests/BotCoreTests/SoloTests.cpp` | yeni | §5.3 |
| `BotCore/PolicyL0.h` | değiştir | solo katmanı (ROAM/EVALUATE/ENGAGE/AVOID), `BrainMemory.SoloMemory` |
| `GameServer/Bot/BrainDriver.cpp` | değiştir | `ArenaInfo`, parametre komutları, EV telemetrisi |
| `BotCore/BotCore.vcxproj` | değiştir | bir `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | bir `ClCompile` satırı |

`ActionExecutor.*`, `ScenarioRunner.*`, `Telemetry.*`, `BotSession.h`, `docs/`, `tools/` **değişmez** (`ScenarioRunner` F8-05'in). Listede olmayan dosya gerekirse **durup** Uygulayıcı Raporu'nda soru yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-10 gece/2026-10-02`; F6-01..F6-06 (+ rol planları) birleşmiş mi doğrula; sunucular kapalı; `Durum` → `UYGULANIYOR`.
2. `SoloEval.h` yaz; `PolicyL0.h`/`BrainDriver.cpp` eklemeleri. Katsayılar **yalnızca** `BrainParams`'tan (`docs/10` §3.3 satırları F6-01 tablosunda); kodda sabit yok.
3. `SoloTests.cpp` (adlar **sabit**):
   - `Solo_Prior_Table_Merged`: 5 satır × 3 sütun yukarıdaki değerlerle birebir; rogue/diğer ⇒ 1,0.
   - `Solo_Ev_Formula_Reference_Vectors`: katsayılar varsayılanken üç referans vektör (ör. W-P vs warrior eşit HP/kaynak ⇒ `pWin` ve `EV` önceden hesaplanmış değerler ±1e-4; priest vs warrior ⇒ `EV < 0,1`; M-F vs priest ⇒ yüksek `EV`).
   - `Solo_Ev_NoHiddenInputs`: rakip HP bilinmiyor ⇒ `oppHp% = 1,0`; `gear = 0`, `terrain = 0`; bayat `hpStale` ⇒ 1,0.
   - `Solo_Engage_Threshold_And_Hysteresis`: `EV = 0,0999` ⇒ AVOID; `0,1` ⇒ ENGAGE; AVOID'dan ENGAGE `0,2`'de; mage eşik 0,25.
   - `Solo_Disengage_EvOrOutnumber`: `EV < -0,15` ya da ek düşman ≥ 1 (40 m) ⇒ `Disengage` (`RuleDisengage`, ortak durum `Retreat`).
   - `Solo_Avoid_Behaviour`: `Avoid` ⇒ rakipten uzaklaşma hareketi (arena içinde, kendi yarıya); rakip > 60 m ya da 3 sn görünmez ⇒ `Roam`.
   - `Solo_Roam_Route_Arena`: 8 nokta merkez ± 35 m, 6 m içinde sıradaki; arena dışına çıkmaz; aynı tohum aynı yön.
   - `Solo_Chase_Limit_And_Tower` (AC-SOLO-04): 60 m / 10 sn sınırı; `inForbidden` ⇒ anında bırakma.
   - `Solo_ForceEngage_NoEngageGuard`: `soloForceEngage = 1` ⇒ `Avoid` yok; `Disengage` (sayısal dezavantaj) var.
   - `Solo_Priest_Gate_HD_HB`: P-HD: rakip HP %49 ve destekçi yok ⇒ ENGAGE; %50 ⇒ yok; P-HB: yalnızca savunma ya da rakip HP %34.
   - `Solo_State_Mapping_Common_FSM`: `Evaluate`/`Avoid`/`Duel`/`Chase` alt durumları ortak durumlarla eşlenir; geçersiz geçiş üretilmez.
   - `Solo_Mage_Plus015_And_Roles`: dört profil için rol düzeltmeleri (mage +0,15) ve `B0-NAIVE`'in EV'yi **hiç kullanmadığı** (naive ayrı politika).
   - `Solo_Deterministic_Seeded`: aynı tohum + girdi ⇒ aynı karar dizisi (500 tick).
4. Proje satırlarını ekle; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu.
5. **Çalışma zamanı (Claude, `/plan-dogrula`):**
   - **S1 duman testi (F8-05'ten önce, kabul değil):** elle sıfırlama (bot satırları: HP/MP/NP, konum; `tools/bot-refill.sh apply`) ile L0 vs `B0-NAIVE` aynı profil 10 tekrar; karar/telemetri akışı ve `NO_ENGAGE` yok kontrolü.
   - **T-SOLO-02** (avantajsız eşleşmeden kaçınma: ör. M-F vs W-P yakın mesafede): AC-SOLO-02: dezavantajlı başlangıçlarda ENGAGE oranı ≤ %20 ("dezavantajlı" = önsel `< 1,0` **ve** `EV < eşik`: tanım Claude/proje sahibi netleştirir).
   - **T-SOLO-03** (1v2): ölmeden kopma oranı ≥ baseline (`B0-NAIVE`), ölüm ≤ %60 (AC-SOLO-03).
   - **T-SOLO-04** (kaçan rakip takip sınırı): ihlal 0, rakip tower halkasına giriş 0 (AC-SOLO-04).
   - **T-SOLO-05** (priest solo hayatta kalma): W-P'ye karşı 30 sn hayatta kalma ≥ %70 (kaçış yolu varken; AC-SOLO-05).
   - **T-SOLO-01 / EVAL-1v1** (F8-05 sonrası): 6×6 ikili × 50 tekrar, taraf değişimli; L0 vs `B0-NAIVE`: önerilen ölçüt (alt sınır > %50); ayna eşleşmeler %50 ± 10 (AC-SOLO-01); sonuçlar `bot-outcome-eval.py` ile; MET-OUT-01/05.
   - **İnsan (proje sahibi):** solo eşleşme kararlarının makullüğü formu (kaçınılması gereken savaşa girmeme, zamanında kopma).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; K2: `Debug` rc=0
- [ ] K3: `./tools/run-tests.sh Release|Debug` `0 failed`; on üç yeni test adı `[ OK ]`; mevcut testler (F6-01..F6-09 dahil) değişmeden geçer
- [ ] K4: `SoloEval.h`'te `windows.h|stdafx|GameServer|shared/` yok; `new|malloc|rand(`, global/static değişken yok; ASCII + CRLF
- [ ] K5: EV katsayıları ve eşikler yalnızca `BrainParams`'tan (kodda sabit yok); `ln`/`sigmoid` belirlenimli
- [ ] K6: gizli girdi yok: düşman HP'si yalnızca `hpKnown`; ekipman/MP/cooldown kullanılmaz (`grep` + `tools/check-perception-contract.py` PASS)
- [ ] K7: düşman tower halkasına giriş/takip ihlali **birim düzeyinde 0**; `soloForceEngage` varsayılan `0`
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F6-10` yalnızca §4'teki dosyalar; `ActionExecutor.*`, `ScenarioRunner.*`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K9 (Claude): `docs/10` §3.2 tablosunun gözlenebilirlik uyarlamasını (sınıf ailesi), `own_resource_score` ve `NO_ENGAGE` ↔ `AVOID` kararını docs/ADR'ye işler; **proje sahibine tek tek sorular** (§1a madde 3: "anlamlı yener" ölçütü, 1v1 `win_rule`, `forceEngage`); `docs/15` §5 `B0-NAIVE` belirsizliklerini (F6-01) kapatır
- [ ] K10 (**çalışma zamanı, Claude**): S1 duman testi çalıştı ve `Solo_*` karar telemetrisi görüldü; T-SOLO-02..05 ölçütleri (AC-SOLO-02..05)
- [ ] K11 (**çalışma zamanı, Claude; F8-05 sonrası**): EVAL-1v1 (36 ikili × 50, taraf değişimli): L0, `B0-NAIVE`'i kabul edilen ölçütle yener; AC-SOLO-01 ayna testi; sonuç raporu `docs/reports/`'a
- [ ] K12 (**insan, proje sahibi**): solo karar davranış formu; yokluğunda F6 faz raporu `KABUL_EDILDI` olmaz

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Solo_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F6-10
grep -nE "windows\.h|stdafx|GameServer|shared/" BotCore/SoloEval.h
python3 tools/check-perception-contract.py
git diff --check gece/2026-10-02...bot/F6-10
# EVAL-1v1 (F8-05 sonrası, Claude): senaryo YAML + /bot scenario run; python3 tools/bot-outcome-eval.py <Logs/bots/.../*.jsonl>
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2-§3. **Bota avantaj yok:** `p_win` girdileri yalnızca gözlenebilir bilgidir; bilinmeyen rakip HP'si kötümser (1,0); `B0-NAIVE` ile L0 aynı adalet yolundan geçer, ek bilgi/hız yoktur. EVAL'da kurtarma teleportu **yok** (maçı geçersiz kılar).
- Eşikler/katsayılar `[Ö]`; L1'e açıktır (`docs/14` §4.2). Önsel tablo EVAL-1v1 sonuçlarına göre güncellenir (`docs/10` §3.2): güncelleme docs/`BrainParams` varsayılan değişimi olarak Claude'dadır.
- Çelişkiler/belirsizlikler:
  1. **Önsel tablo gözlenemez ayrım:** `docs/10` §3.2 W-P/W-G ve M-F/M-I sütunlarını ayırır; algı yalnızca sınıf ailesini bilir (`UnitView.cls`, ekipman yok) ⇒ ortalama sütunlar `[A]`.
  2. **`kendi_kaynak_skoru`, `arazi_avantajı`, `ekipman_farkı`** `docs/10` §3.3'te tanımsız: `[A]` (yukarıda).
  3. **`NO_ENGAGE` ↔ `AVOID`:** `docs/15` §6b 60 sn hasar yoksa maç `invalid`; kaçınan iki taraf EVAL'ı geçersiz kılar ⇒ `soloForceEngage`. Bu, "avantajsız eşleşmeden kaçınma" davranışını (T-SOLO-02) EVAL-1v1'de **ölçülemez** kılar: T-SOLO-02 ayrı koşuda (forceEngage kapalı) ölçülür.
  4. **"Anlamlı yener"** (`docs/17` F6) tanımsız; `docs/15` §8 AC-EVAL-01 yalnızca 8v8 SPRT (+100 Elo) için. Öneri §1a madde 3.
  5. `B0-NAIVE` geri çekilmez ve pot %30'da içer; L0 geri çekilir ve potu erken içer: eşit hasar altında L0 avantajı `retreat + pot` politikasından gelir. Eşit melee eşleşmelerde (W-P vs W-P) `p_win` ~%50 beklenir (`AC-SOLO-01`).
  6. **AC-SOLO-02** "dezavantajlı başlangıç" tanımsız.
  7. `docs/10` §5 "kaçarken yavaşlatma yalnızca takipçiye" ve §4.1 "mage'e karşı kısa yönlü yaklaşma (görüş hattı testi olan noktalardan)": ikincisi `NavPickLosCell`'e ve ek konum kararına bağlıdır, bu planda **yok** (F6-03'ün düz yaklaşması kullanılır; `[A]` sınırlama).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
- Kabul kriterleri öz-değerlendirme (K10-K12 DeepSeek'e ait değildir):
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
