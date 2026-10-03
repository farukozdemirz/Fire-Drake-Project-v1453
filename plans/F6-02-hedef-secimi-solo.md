# F6-02: Hedef seçimi (solo): aday süzme, skor, bağlılık ve acil değiştirme (`BotCore/TargetSelect.h`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; kapı G6a) |
| Branch | `bot/F6-02 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-01** (`Intent`/`DecisionInput`/`ReasonCode`/`BrainParams`) `KAPANDI`; F4-50 (`UnitView` konum yaşı/hız/ad) `KAPANDI`; F4-51 (`UnitView.hp*`, `HpTable`) `KAPANDI`; F4-52 (`SkillEventRing`) `KAPANDI`; F4-06 (`TargetHpReq`, CLI-10) `KAPANDI`; F5-05 (`NavReachVerdict`/`NavUnreachTracker`) ve F5-06 (yasaklı bölge) `KAPANDI` (yalnızca `NavView` alan anlamı için; bağlama F6-06'da) |
| İlgili gereksinim / kabul | `docs/09` §5 (hedef seçimi: yalnızca skor/override/bağlılık; takım kısımları F7), `docs/10` §3.1 (solo girdileri), `docs/13` §5.2a (tazelik/görünürlük), `docs/16` §5.2 (`TARGET_SET` gerekçeleri), MET-TGT-04 (≤ 4/dk), AC-WAR-02 (baskı sürekliliğine ön koşul), AC-SOLO-04 (rakip tower halkasına giriş 0) altyapısı |
| Tahmini büyüklük | S–M (4 dosya: 1 yeni başlık, 1 yeni test dosyası, 2 proje satırı) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 yazım turu) |

---

## 1. Amaç

Solo bir bot (önce warrior, rol-bağımsız) görünür düşmanlar arasından **tek bir hedef seçer, ona bağlı kalır ve gerektiğinde değiştirir**: hard filtreler (ölü/görünmez/kayıp/ulaşılamaz/yasaklı bölge) → `docs/09` §5.2 skoru → bağlılık + değiştirme marjı → acil override'lar → `TARGET_SET` gerekçe kodu. Saf mantık (BotCore, belirlenimli, sunucusuz); girdi yalnızca `PerceptionSnapshot` (+ `NavView` ipuçları), düşman MP/cooldown/envanteri **okunmaz**.

## 1a. Neden TASLAK (HAZIR yapmak için)

Ön koşullar:

1. **F6-01 `KAPANDI`** (`DecisionInput`, `ReasonCode`, `BrainParams` satırları buraya dayanır).
2. `NavView.targets[]` alan anlamı F5-55 dilimleri (`plans/F5-55-...md` §1A, 2026-10-03): **F5-59** `NavService` yaşam döngüsü ve harita yükleme (`HAZIR`; `bot/F5-59` dalı açık), **F5-61** kiriş guard'ı, **F5-62** `/bot goto` + waypoint zinciri + `NavPathfinder`/`NavReach`/maliyet katmanı kurulumu, **F5-63** `NavFollower`/takılma/F5-57 sözleşmesi, **F5-64** bütçe + `NAV_*` telemetri, **F5-65** nav durumu temizliği, **F5-66** çalışma zamanı doğrulama (F5-61..F5-66 `TASLAK`; F5-60/F5-67 yalnızca su denetimi/koşullu düzeltme) ile onaylanmalı (**Dilim boşluğu (2026-10-03 kontrolü):** `NavRetreatPlanner` (güvenli nokta) çağrısı, `NavReachJudge` sonucunun karar katmanına iletilmesi ve `directClear` (düz kiriş) sorgusu bu dilimlerin satırlarında **yazılı değildir** (`plans/F5-55` §1A; F5-59 §3 "kapsam dışı" yalnızca `NavReach` kurulumunu F5-62'ye bırakır). İlgili dilim planı genişletilmeli ya da ek bir F5 dilimi (öneri, kimlik atanmadı) yazılmalıdır.): `verdict` (`NavReachVerdict`), `pathM`, `inForbidden`. Bu plan alanları **tüketir**, üretmez; üretici yoksa `NavView.serviceUp == false` ve süzme "nav'sız" dalında çalışır (`Unreachable`/`inForbidden` bilinmez sayılır; testle sabit).
3. F4-53 ve F4-60 **`KAPANDI`** (merge `0001d04`, `7891f74`): `D` (kendi debuff'ımız) ve `Risk` içindeki "Mage Armor gözlendi" terimi `ObservedStatusTable::Find(target, buffType, nowMs)` ile (`BotCore/Perception.h:2014`, `E` sınıfı kayıt) beslenir; sunucu tarafında tablo `BotSession::m_status` (`m_obsLock` altında). `UnitView`'a gözlenen durum alanları **F4-61**'dedir (planı yok); bu plan tabloyu `TargetExtras` ile doğrudan okur. Tablo boşsa `D = 0`, `armorFlag = nullptr` (testle sabit).

Yazım turunda yeniden doğrulanacak referanslar: `BotCore/Perception.h:1141-1168` (`UnitView` alanları: `posState`, `invisibility`, `hpKnown`, `hpStale`, `hpAgeMs`, `vx/vz`), `:26` (`kPosLostMs = 6000`), `:383` (`kHpStaleMs = 10000`), `:1840` (`SkillEventRing`, `CountIn`/`FindLatest` imzaları), `BotCore/BotCombat.h:685` (`kTargetHpPollMs = 2000`), `BotCore/NavReach.h:48-97`; `docs/04` §4 HP öncülleri (W 5650, P 3491, M-F 1541, M-I 2228 `[D]`/`[I]`); `docs/05` §5.1 madde 4 (≈ 470–550 hasar/sn, "yalnızca göreli karar için").

## 2. Bağlam (okunması zorunlu)

- `docs/09` §5.1 (gözlemlenebilir girdiler; HP gözlemi **seyrek**: yalnızca kendi hasarından sonraki `WIZ_TARGET_HP` ve seçili hedef için ≤ 2 sn'de bir yoklama), §5.2 (skor formülü ve ağırlıklar `P-TGT-*`), §5.3 (bağlılık/değiştirme), §5.4 (override listesi).
- `docs/10` §3.1 (solo girdileri: rakip sınıfı, HP oranı, 40 m'deki ek düşman sayısı), §5 (≥ `P-SOLO-OUTNUMBER` ek düşman → çıkış: F6-10).
- `docs/13` §5.2a: kaynak sınıfları (`O`/`P`/`E`/`G`), geçerlilik süreleri (hareketli: ≤ 3,1 sn taze, 3,1–6 sn bayat = yalnızca `E` kestirimi, > 6 sn kayıp aday; durağan sınırsız; `in_region = false` → hedef olamaz); `TARGET_LOST_VIS` = bölge dışı **veya** "kayıp aday" ≥ 3 sn; gizli/görünmez oyuncu hedeflenmez.
- `docs/16` §5.2 gerekçe kodları; MET-TGT-04 (≤ 4/dk; `SCORE_MARGIN` gerekçeli ≤ 1,5/dk).
- `BotCore/Perception.h`: yukarıdaki satırlar. `BotSession.cpp:109-116`: başkalarının `WIZ_ATTACK` yayını **saklanmaz** (yalnızca kendi `m_attackEcho`): melee saldırganı ayrıştırılamaz (aşağıda "Kapsam dışı").

## 3. Kapsam

**Yapılacaklar** (`BotCore/TargetSelect.h`, yalnızca `BotCore/*.h`; global/static değişken yok; tick yolunda dinamik bellek yok)

1. Veri tipleri:
   ```cpp
   enum class RejectReason : uint8_t { None, Dead, Invisible, PosLost, TooFar, Forbidden, Unreachable, Abandoned };
   struct TargetEvent  { uint64_t tMs; uint16_t oldId; uint16_t newId; ReasonCode reason; };   // TARGET_SET
   struct TargetMemory {
       bool has; uint16_t id; uint64_t setAtMs; float score; uint64_t lostSinceMs; uint64_t lastHpPollMs;
       struct { uint16_t id; uint64_t untilMs; } abandoned[8]; int abandonedCount;
       TargetEvent events[8]; int eventHead, eventCount;       // fixed ring, PopEvent() drains
       void Clear(); bool PopEvent(TargetEvent &);
   };
   struct TargetExtras { const uint8_t * ourDebuffs; /* per enemy index, F4-53; may be null */ const uint8_t * armorFlag; /* Mage Armor, F4-53; null = unknown */ };
   struct TargetChoice { bool has; uint16_t id; ReasonCode reason; float score; bool changed; bool wantHpPoll; int considered; int rejected; };
   ```
2. `TargetChoice SelectTarget(const DecisionInput & in, TargetMemory & mem, Rng & rng, const TargetExtras * extras = nullptr)`.
3. **Hard filtreler** (sıra sabit; her red `considered`/`rejected` sayacına ve test için `RejectReason` çıktısına yazılır; aday listesi `snap->enemies[0..enemyCount)`):
   1. `dead` → `Dead`; 2. `invisibility != 0` → `Invisible` (`docs/13` §5.2a); 3. `posState == POS_LOST` ve `lostSince` ≥ 3000 ms (`TARGET_LOST_VIS`; bayat 3,1–6 sn aralığı aday kalır, konumu `EstimatePosition` kestirimiyle `E` etiketli) → `PosLost`; 4. `dist > tgtMaxDistM` (80 m `[A]`) → `TooFar`; 5. `nav.targets[].inForbidden` → `Forbidden` (karşı ulus tower halkası: AC-SOLO-04, AC-NAV-06; **sert**: skorlanmaz); 6. `verdict == Unreachable` (F5-05'in `holdMs` bekleyişinden sonra) → `Unreachable`; 7. `abandoned[]` içinde ve `untilMs > nowMs` → `Abandoned` (`P-TGT-ABANDON-HOLD-MS` 10 sn: bırakılan hedef hemen yeniden seçilmez).
4. **Skor** (`docs/09` §5.2; ağırlıklar `BrainParams.tgtW*`):
   - `K = 1 / (1 + TTK / 10 sn)`, `TTK = hpEst / max(1, tgtOwnDps)`; `hpEst` = `hpKnown && !hpStale ? hp : ClassHpPrior(cls)`; `ClassHpPrior`: warrior 5650, priest 3491, mage 1900 (M-F 1541 / M-I 2228 ayrımı gözlenemez, ortalama `[A]`), diğer 4000. `tgtOwnDps` 470 `[A]` (`docs/05` §5.1; **göreli** karar).
   - `R` (**solo için sürekli**, takım payı yok): `pathTimeMs = (nav.pathM > 0 ? pathM : dist * 1.2) / hız * 1000` (hız: yürüme 4,5 m/s; kendi hız buff'ı (`BuffType 6`, `isBuff`) varsa 6,7), `R = clamp(1 - pathTimeMs / tgtReachHorizonMs, 0, 1)` (varsayılan 20 sn). `docs/09`'daki "≤ 4 sn içinde ulaşabilen üye oranı" 70 m'lik arena başlangıcında (≈ 15 sn yürüme) her zaman 0 verirdi: bkz. "Çelişkiler" 1.
   - `T`: sınıf ağırlığı priest 1,0 / mage 0,8 / warrior 0,6 / diğer 0,4 + `0,4 * min(1, k/3)`, `k` = son 5 sn'de bu adayın **bize** yönelik `WIZ_MAGIC_PROCESS` olay sayısı (`SkillEventRing`, `CountIn`); üst sınır 1,5. Melee darbesi atfı yok (kapsam dışı).
   - `D` = `extras->ourDebuffs[i]` × 0,3 (en çok 0,6; `extras == nullptr` → 0). `Risk` = (adayın 10 m çevresinde ≥ 3 başka düşman: +0,3) + (`nav.targets[].crossesCluster`: +0,3) + (`armorFlag`: +0,4; `nullptr` → 0). `Forbidden` zaten elenmiştir (+1,0 terimi hiç uygulanmaz).
   - `score = wKill*K + wReach*R + wThreat*T + wDebuff*D - wRisk*Risk + commitBonus`.
5. **Bağlılık ve değiştirme** (`docs/09` §5.3): mevcut hedef hard filtreden geçiyorsa `commitBonus` = ilk `tgtCommitMinMs` (4 sn) boyunca `+1e6` (override hariç), sonra yeni aday ancak `score_new > score_cur + tgtSwitchMargin * max(|score_cur|, 0,1)` ise geçer (`ScoreMargin`).
6. **Acil override'lar** (bağlılığı yok sayar; solo ile ilgili olanlar): `TargetDead` (hedef `dead` ya da tablodan düştü → aynı tick'te yeni hedef: `docs/06` §8 "bir sonraki karar ≤ 300 ms"), `TargetLostVis` (bölge dışı ya da kayıp aday ≥ 3 sn), `TargetUnreachable` (nav `Unreachable` → hedef `abandoned[]`'e `tgtAbandonHoldMs` ile yazılır), `Finishable` (başka bir **HP'si bilinen** düşman < %15, ulaşılabilir ve mevcut hedef HP > %50), `SelfDefense` (mevcut hedef dışında bir düşman son 3 sn'de bize cast etti ve mevcut hedef etmedi; yalnızca mesafe ≤ 25 m). `HealerSwitch`, `PeelThreat`, `TeamCall`, `DebuffCall`, `LeaderOrder` **F7** (üretilmez).
7. **HP yoklaması** (`TargetChoice.wantHpPoll`, CLI-10): seçili hedef için `hpKnown == false` ya da `hpAgeMs >= kTargetHpPollMs` (2000 ms) ise `true` (ilk seçimde `echo = 1`: yürütücü işi); aynı hedef için `lastHpPollMs` penceresi 2000 ms'den önce yeniden `true` olmaz.
8. `TARGET_SET` olayı: hedef değiştiğinde `TargetEvent` (eski/yeni kimlik, gerekçe) halkaya yazılır (`PopEvent`); ilk seçim `Init`.
9. Birim testleri (§5.3).

**Kapsam dışı (yapılmayacak)**

- `TeamBlackboard`, ortak hedef, healer geçişi (`HEALER_SWITCH`), debuff çağrısı, `PEEL_THREAT`, `LEADER_ORDER`: F7.
- NPC/canavar hedefleri (arena A'da NPC yok, `docs/15` ARENA-03; bowl F11).
- Eşleşme değerlendirmesi/EV (`ENGAGE`/`AVOID`): F6-10 (bu plan yalnızca "kim?" sorusuna cevap verir, "girilsin mi?" sorusuna değil).
- **Melee saldırganı atfı**: başkalarının `WIZ_ATTACK` yayını algıda saklanmıyor (`BotSession.cpp:109-116`); `T` yalnızca cast olaylarıyla beslenir. Bir `AttackEventRing` algı dilimi gerekirse ayrı F4 planı (öneri; bu planda yok).
- Düşman ekipmanı/profili (W-P/W-G, M-F/M-I) ayrımı: `UnitView` ekipman taşımaz (`Perception.h` `ParseUserInfo` madde kayıtlarını atlar); sınıf ailesiyle yetinilir.
- Sunucu bağlama, telemetri yazımı (`TARGET_SET` satırı F6-06'da), `docs/` değişikliği (Claude).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/TargetSelect.h` | yeni | §3 |
| `Tests/BotCoreTests/TargetSelectTests.cpp` | yeni | §5.3 |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca bir `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca bir `ClCompile` satırı |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-02 gece/2026-10-02`; F6-01'in birleşmiş olduğunu doğrula (`git log --oneline | grep F6-01`); `Durum` → `UYGULANIYOR`.
2. `TargetSelect.h` yaz (§3). Sıralama/seçimde belirlenimli kıran: eşit skorda `Rng::NextBelow` (yalnızca tam eşitlikte); `std::sort` yok (sabit dizi üzerinde seçim; ≤ 32 aday).
3. `TargetSelectTests.cpp` (adlar **sabit**; sentetik `PerceptionSnapshot`, sabit tohum, sentetik zaman):
   - `Target_Filters_Reject_Reasons`: yedi `RejectReason`'ın her biri için bir aday; `considered`/`rejected` sayaçları; nav'sız (`serviceUp == false`) dalda `Forbidden`/`Unreachable` uygulanmaz.
   - `Target_Score_Terms_Monotonic`: HP düştükçe `K` artar; yol uzadıkça `R` azalır; priest > mage > warrior `T`; ≥ 3 yakın düşman `Risk` ekler.
   - `Target_HpPrior_Vs_Observed`: `hpKnown && !hpStale` gözlenen HP'yi; bayat/yok sınıf öncülünü kullanır.
   - `Target_Commit_NoSwitchWithinMin`: 4 sn içinde daha iyi aday olsa da değişmez; 4 sn sonra marjı (%30) geçmezse değişmez, geçerse `ScoreMargin` ile değişir.
   - `Target_Override_TargetDead_Immediate`: hedef ölünce aynı çağrıda yeni hedef (`TargetDead`), bağlılık yok sayılır.
   - `Target_Override_LostVis_3s`: bölge dışı/kayıp aday 2999 ms'de tutulur, 3000 ms'de `TargetLostVis`.
   - `Target_Override_Unreachable_AbandonHold`: `Unreachable` → `TargetUnreachable`, aynı kimlik 10 sn yeniden seçilmez (9999 ms'de `Abandoned`, 10000 ms'de uygun).
   - `Target_Override_SelfDefense_And_Finishable`: koşullar ve sınırlar (mesafe 25 m, HP %15/%50).
   - `Target_HpPoll_Cadence_2s`: `wantHpPoll` 2000 ms'den önce ikinci kez `true` olmaz; ilk seçimde `true`.
   - `Target_Switch_Rate_Sim_60s`: 60 sn sentetik oyun (jitterli konum/HP, iki yakın skorlu düşman) → hedef değişimi ≤ 4 (MET-TGT-04).
   - `Target_Tie_Rng_Deterministic`: tam eşit skorda aynı tohum aynı seçim; farklı tohum iki adayı da üretebilir (64 deneme).
   - `Target_NoEnemy_Cleared`: düşman yok → `has == false`, `RuleNoTarget`; bellek temizlenir, eski hedef `abandoned`'a yazılmaz.
4. Proje satırlarını ekle; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; K2: `Debug` rc=0
- [ ] K3: `./tools/run-tests.sh Release|Debug` `0 failed`; on iki yeni test adı `[ OK ]`; mevcut testler (F6-01 dahil) değişmeden geçer
- [ ] K4: `TargetSelect.h`'te `windows.h|stdafx|GameServer|shared/` yok; `new|malloc|rand(`, global/static değişken yok; ASCII + CRLF
- [ ] K5: hard filtreler `RejectReason` ile tek tek sınanmış; `Forbidden` aday asla seçilmez (AC-SOLO-04 birim düzeyi)
- [ ] K6: MET-TGT-04: `Target_Switch_Rate_Sim_60s` ≤ 4 değişim/dk
- [ ] K7: belirlenim (aynı girdi + tohum → aynı `TargetChoice`); düşman MP/cooldown/envanteri alanı **hiçbir yerde** okunmaz (`grep -nE "mp|cooldown|inventory"` yalnızca `SelfState` kendi alanlarında; `tools/check-perception-contract.py` PASS)
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F6-02` yalnızca §4'teki dosyalar (+ plan); `GameServer/`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K9 (Claude): `docs/09` §5.2 `R` tanımı ile solo `R` farkını (aşağıdaki "Çelişkiler" 1) docs'a işler; `P-TGT-REACH-HORIZON`, `P-TGT-OWN-DPS`, `P-TGT-MAX-DIST`, `P-TGT-ABANDON-HOLD-MS` `[A]` etiketlerini `docs/09`'a ekler
- [ ] K10 (çalışma zamanı, bu planda yok): hedef seçiminin sunucuda doğrulanması F6-06 (`TARGET_SET` olayı, `MET-TGT-04`); bu plan `GELIŞTIRILDI` sayılmaz

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Target_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F6-02
grep -nE "windows\.h|stdafx|GameServer|shared/" BotCore/TargetSelect.h
python3 tools/check-perception-contract.py
git diff --check gece/2026-10-02...bot/F6-02
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; **bota avantaj yok** (`docs/03` §13, `docs/14` §5.2): düşman MP'si, cooldown'u, envanteri, görüş alanı dışındaki birimler girdi değildir. `hpKnown` yalnızca kendi `WIZ_TARGET_HP` gözleminden gelir.
- Skor ağırlıkları ve eşikler `[Ö]`/`[A]`; L1 aralıkları `docs/14` §4.1.
- Çelişkiler/belirsizlikler:
  1. `docs/09` §5.2 `R` = "≤ `P-TGT-REACH-TIME` (4 sn) içinde menzile ulaşabilecek **canlı üye oranı**" takım tanımıdır; test arenasında başlangıç 70 m (`docs/15` §2.4) iken tek bot için hep 0 çıkar. Solo için sürekli bir ufuk (20 sn `[A]`) kullanılır; docs'ta açık tanım yok.
  2. `docs/09` `T` "son 5 sn'de bize verdiği hasarla artar": melee atfı algıda yok (kapsam dışı notu).
  3. `docs/13` §5.2a "bayat 3,1–6 sn: yalnızca `E` kestirimiyle kullan": hedef seçiminde bayat adayın skoru konum kestirimiyle hesaplanır, ama menzil kararı F6-03'te yeniden doğrulanır.

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
