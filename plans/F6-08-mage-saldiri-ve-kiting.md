# F6-08: Mage saldırı davranışı: tek hedef/alan skill seçimi, menzil bandı, kiting, Mana Shield ve element tahmini (`BotCore/MageCombat.h`); **summon F7'dedir**

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; kapı G6c: mage saldırı, **summon yok**) |
| Branch | `bot/F6-08 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-01, F6-02, F6-04, F6-05, F6-06** `KAPANDI`; F4-25 (uçan tek tipli Type3), F4-26 (çift tipli `{3,4}`), F4-29/F4-30 (alan ve uçan alan, `victims`), F4-24 (cast iptali/ayakta), F4-36 (eşyalı sınıf skill kapısı: Impact scroll'ları), F4-47/F4-48 (mage uçan/tek hedef/alan skill ölçümü, `docs/05` §9.6 işlenmeli) `KAPANDI`; F4-51 (`HpObs.lastDamage`: element tahmini girdisi); F5-04 (menzil halkası), F5-07 (`NavRetreatPlanner`), F5-10 (`NavPickLosCell` advisory) `KAPANDI`; **F5-55 dilimleri (`plans/F5-55-...md` §1A, 2026-10-03): **F5-59** `NavService` yaşam döngüsü ve harita yükleme (`HAZIR`; `bot/F5-59` dalı açık), **F5-61** kiriş guard'ı, **F5-62** `/bot goto` + waypoint zinciri + `NavPathfinder`/`NavReach`/maliyet katmanı kurulumu, **F5-63** `NavFollower`/takılma/F5-57 sözleşmesi, **F5-64** bütçe + `NAV_*` telemetri, **F5-65** nav durumu temizliği, **F5-66** çalışma zamanı doğrulama (F5-61..F5-66 `TASLAK`; F5-60/F5-67 yalnızca su denetimi/koşullu düzeltme)**. B0-NAIVE koşusu için F6-06'nın `/bot brain naive` modu (AC-MAG-02 karşılaştırması) |
| İlgili gereksinim / kabul | `docs/08` §3-§7, §9-§12; T-MAG-01, T-MAG-02 (yalnızca "tek başına" kısmı), T-MAG-03, T-MAG-04; **AC-MAG-01** (geçersiz cast ≤ %2; CLI-03 ihlali 0), **AC-MAG-02** (W-P'ye karşı tek başına hayatta kalma ≥ baseline; W-G desteği kısmı F7), **AC-MAG-03** (MET-AOE-01 ≥ `P-MAG-AOE-MIN − 0,5`); CLI-03/04/07/11; MEC-MAG-03; G6c (`docs/17` §5) |
| Tahmini büyüklük | M (6 dosya: 1 yeni başlık, 1 yeni test, `PolicyL0.h` ve `BrainDriver.cpp` eklemeleri, 2 proje satırı) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 yazım turu) |

---

## 1. Amaç

Solo bir mage botun (M-F `mage.fire_burst`, M-I `mage.ice_control`) **menzilden yüksek hasar, kümeye alan hasarı ve melee tehdidinden kaçınma** davranışının saf mantığı ve bağlaması: hedef (F6-02) için `docs/08` §6.2 skill tablosu, tercih edilen menzil bandı (30–45 m), kiting (`docs/08` §7), alan skill'inde küme ve hedef noktası kararı (`P-MAG-AOE-MIN`, CLI-07), Mana Shield ve yavaşlatma ile yakın tehdit savuşturma, MP rezervi, element direnci tahmini (oturum içi L0.5). Çıktı `Intent` (F6-01), `PolicyL0` mage dalı.

**Summon bu planın kapsamında DEĞİLDİR** (aşağıdaki "Summon kararı").

## 1a. Neden TASLAK (HAZIR yapmak için)

Ön koşullar:

1. **F6-06 `KAPANDI` ve G6a çalışma zamanı kabulü kayıtlı** (aynı yürütme hattı); F6-02 hedef seçimi mage için de kullanılır.
2. **Skill verisi:** `SkillDamageNominal(SkillSpec.meta)`, `SkillSpec.element/radius` (F6-01; `meta` `FillSkillMeta` ile, F4-60) `BuildSkills(role)` tarafından `_MAGIC_TABLE`/`MAGIC_TYPE3`/`MAGIC_TYPE4`'ten doldurulur; `element` kaynağı (`MAGIC_TYPE3.bAttribute` varsayımı) **yazım turunda yeniden doğrulanmalı** (`docs/05` §7 yalnızca "Ateş/Buz" ağacı adı verir). M-I (`BotMI_K`) skill ağacı baytları `db/002`'den okunup hangi skill'lerin açık olduğu doğrulanmalı (M-F için `strSkill = 00 00 00 00 00 46 34 00 14 00`: ateş 70, buz 52, usta 20 `[V]` F4-48 planı); KI-016 (ağaç puanı denetimi) ve KI-018 (`Etc != 0` quest kilidi) bu plana girdidir.
3. **Alan skill'i hedef noktası:** mevcut `ActionExecutor` alan atışında hedef noktası **hedef botun konumu**dur (ad tabanlı sürücü); kümenin **ağırlık merkezi** için `CastTarget{id = -1, x, y, z, isSelf = false}` ile **hedef görünümü doğrudan** verilmesi gerekir. `TickCast` zaten `CastTarget` alır (`ActionExecutor.h:37-44` ve `ActionExecutor.cpp:955-959` `CastCoordField(area, isSelf, ...)`); `BrainDriver` bunu kullanır (`isSelf = false`: aksi halde nokta çağıranın konumu olur). Davranış yazım turunda yeniden doğrulanmalı.
4. `docs/08` `P-MAG-PREF-RANGE` ve çoğu skill menzili (56/45/78/90) ile tercih edilen bant tutarlılığı ve "ayakta skill" (incineration `UseStanding 53`: KI-017, ayakta şartı **uygulanmaz**) ölçümle teyit edilmeli.
5. AC-MAG-02'nin "W-G desteğiyle" kısmı ve T-MAG-02'nin peel'li varyantı F7'dedir.

Yazım turunda yeniden doğrulanacak referanslar: `BotCore/BotCombat.h:266-340` (`IsFlyingCast`, `CastTypesSupported`, `CastTypeMoralSupported`), `:346-384` (`IsAreaMoral`, `SendsAimPoint`, `CastMoralSupported`), `:505-518` (`CastTargetIdField`, `CastCoordField`), `GameServer/Bot/ActionExecutor.cpp:870-875`, `:955-959` (`area`, `sData`), `BotCore/Perception.h:418-430` (`HpObs.lastDamage`), `plans/F4-48-skill-betik-mage-karus-tek-hedef-ve-alan.md` §2 (mage skill veri tablosu: Pillar of fire `110551` 160 MP cast 1,5 / recast 5,3 menzil 56; incineration `110570` 390 MP 1,1 / 21,3 menzil 45; Ice comet `110651` `{3,4}`; Fire ball/spear `110515`/`110527` uçan menzil 78; Fire burst `110533` Moral 10 r = 8 menzil 90; Inferno `110545`/Blizzard `110645`/Supernova `110560`/meteor Fall `110571` Moral 10 r = 15), `docs/05` §7 (mage tablosu; Mana Shield `110815` master 12, 150 MP; Frozen armor/shell/Ice barrier priest AC buff'ıyla çakışır), `KNOWN_ISSUES.md` KI-016/017/018.

## 2. Bağlam (okunması zorunlu)

- `docs/08` §3 (girdiler), §4 (parametreler: `P-MAG-PREF-RANGE 30–45 m`, `P-MAG-MELEE-DANGER 18 m`, `P-MAG-AOE-MIN 3 (r15) / 2 (r8)`, `P-MAG-MP-RESERVE 500`, `P-MAG-ELEM-SAMPLES 3`), §5 (karar öncelikleri 1-8), §6 (hasar seçimi: §6.1 element tahmini, §6.2 tek hedef tablo M-F/M-I, §6.3 alan kuralları), §7 (kiting sözde kodu), §9 (kendini koruma), §10-§12.
- `docs/10` §4.2 (solo mage: hazırlık Mana Shield + (rakip elementi biliniyorsa) direnç buff'ı + Frozen armor/shell; açılış azami menzilden yavaşlatma (Ice comet) → patlama (incineration/Prismatic) → Pillar of fire/Ice Impact; melee'ye karşı kiting, mesafe kapanırsa ve HP < %60 ise `DISENGAGE`; M-F ENGAGE eşiği +0,15: F6-10).
- `docs/03` CLI-03 (cast: CASTING → EFFECTING = `CastTime×100 + 70–90 ms`; uçan alan: CASTING → FLYING → EFFECTING, ~1 sn uçuş), CLI-04, CLI-07 (alan hedef noktası çağırana `sRange` içinde), MEC-MAG-03 (elementli Type3 saniyede bir), MEC-MAG-12/16/17 (uçan Type3 MP iki kez; alan skill `victims`; uçan alan).
- `docs/04` §4 (M-F HP 1541 / M-I 2228 S1; MP 6021), `docs/05` §7, `docs/16` MET-AOE-01, MET-ACT-01 (mage ≥ %75), MET-SUR-*.
- `docs/11` §4.2 (`role_adj`: mage **+0,10**, geri çekilme daha erken), §3.3 (mage MP rezervi 500).

## 3. Kapsam

**Yapılacaklar** (`BotCore/MageCombat.h`; yalnızca `BotCore/*.h`; global/static değişken yok; dinamik bellek yok)

1. **Skill adları ve kimlikler:** `enum class MageSkill { PillarOfFire, Incineration, MeteorFall, Supernova, FireBurst, FireBall, FireSpear, FireImpact, IceComet, FrostNova, Prismatic, Blizzard, IceBurst, IceOrb, IceImpact, ManaShield, FrozenArmor, FrozenShell, IceBarrier }` ve `MageSkillId(skill, nation)` (Karus `110xxx`, El Morad `+100000`; `docs/05` §7). Yalnızca `SkillSpec` listesinde **bulunan ve** `SkillReady` olanlar adaydır.
2. **Tek hedef seçimi** `MagePick PickSingle(const DecisionInput &, const TargetChoice &, MageMemory &)` (`docs/08` §6.2; ilk eşleşen kazanır; tip kapısı: tick başına en çok bir Type3; EFFECTING ile sonraki CASTING arası ≥ 1 farklı sunucu saniyesi: `SkillReady` + `typeGateWaitMs[3]`):
   - **M-F:** hedef kaçıyor (F6-03'teki `IsFleeing`) ⇒ Ice comet (yavaşlatma); patlama penceresi (**solo:** hedefe yavaşlatma uygulandı ≤ 6 sn önce ya da hedef HP bilinen < %50) ve `dist <= 44` ve `mp - 390 >= rezerv` ⇒ incineration; standart ⇒ Pillar of fire → Fire Impact (`itemOk`) → Fire spear/Fire ball (uçan, menzil 78); MP < rezerv ⇒ yalnızca MP potu ve pozisyon (acil durumda Fire burst **değil**).
   - **M-I:** patlama penceresi ⇒ Prismatic; kaçıyor ⇒ Ice comet/Blizzard; standart ⇒ Ice comet → Ice Impact → Ice orb.
   - **Element direnci** (§6): hedefte "dirençli" işaretli element **düşük puan** alır; M-F için buz yedekleri, M-I için ateş yedekleri (Pillar of fire).
   - **Menzil/ayakta:** `CastInRange` ile `SkillReady` yeterli; `UseStanding == 1` skill'de durma (`move = Hold`) önce; KI-017 (`53`) skill'leri için ek durma yok.
3. **Alan kararı** `AoePick PickArea(...)` (`docs/08` §6.3, §5 madde 5; CLI-07): düşman adayları `snap->enemies` (canlı, görünür, `invisibility == 0`); her aday merkez için yarıçap `r` içindeki sayıyı hesapla (`r = SkillSpec.radius`: 15 ya da 8); **en çok isabetli** merkezi seç; hedef noktası = o kümenin **ağırlık merkezi** (aynı kümedeki düşmanların ortalaması); koşul: `count >= P-MAG-AOE-MIN` (r15: 3, r8: 2), hedef noktası çağırana `Range` içinde (guard ile aynı `meters < range`), `mp - msp >= rezerv`. Çıktı `CastIntent{aim = true, aimX, aimZ}` (paket hedef kimliği `-1`; kurbanı sunucu seçer). `victims` ölçümü MET-AOE-01'dir.
4. **Menzil bandı ve konum** `MageMove(in, mem)`: hedef `dist` ∈ `[prefMin, prefMax]` (30–45 m) ise `Hold`; `dist < prefMin` ⇒ geri aç (`kite`); `dist > prefMax` ⇒ yaklaş (halka `[prefMin, prefMax - 2]`, `FollowTarget`); mage takımdan koparsa/`P-PTY-SPREAD-MAX` F7. Engel arkasında kalmama: `NavPickLosCell` **advisory**.
5. **Kiting** `KiteStep(...)` (`docs/08` §7, birebir): `threat` = en yakın düşman melee (`RoleFamilyOfClass`: warrior/rogue); `threat.dist > meleeDanger (18 m)` ⇒ yok; `dir` = `NavView` güvenli nokta yönü (nav yok ⇒ tehditten doğrudan uzaklaşma vektörü; arena modunda arena içinde kal); `t_cast = 1,5 sn`; `threat.dist - hız_tahmini * t_cast > threat.saldırı_menzili + 2` ⇒ `CAST_THEN_MOVE` (önce cast, sonra adım), aksi ⇒ `MOVE_THEN_CAST` (önce mesafe aç); `hız_tahmini` = `UnitView.vx/vz` büyüklüğü (`E`), `saldırı_menzili` = 2,0 m `[A]` (melee). Kendi yavaşlatmamız hareket hızına uygulanır (CLI-05: 45; sprint yok).
6. **Kendini koruma** (`docs/08` §9): melee teması ≤ 5 m ve HP > %50 ⇒ **Mana Shield** (`110815`, `SelfState.buffs`'te `BuffType 31` yoksa) + takipçiye yavaşlatma (Ice comet/Frost nova) + adım; melee teması ve HP ≤ %50 ⇒ geri çekilme (F6-05 `role_adj +0,10`) + pot; yavaşlatma/kök altında cure isteği (F7; bu planda yok) ve menzildeki hedefe cast sürer; Silence ⇒ geri çekil.
7. **Hazırlık** (savaş dışı): Mana Shield, solo'da Frozen armor/shell (priest yok: çakışma yok; `docs/10` §4.2); `P-MAG-MP-RESERVE` altında hazırlık yok.
8. **Element tahmini (L0.5, oturum içi)** `class ElemEstimator` (`docs/08` §6.1): hedef kimliği × element (ateş/buz/yıldırım) için örnek halkası; örnek = kendi **isabet eden** cast'ten sonra `HpObs.lastDamage` (WIZ_TARGET_HP `damage` alanı: **gözlem sözleşmesine uygun**) / `SkillDamageNominal(SkillSpec.meta)`; `P-MAG-ELEM-SAMPLES` (3) örnekten sonra oranı **en iyi elementin %70'inin altında** olan element "dirençli" (`[A]` eşik) ve skor çarpanı 0,5; sıfırlama: hedef değişince. Düşman ekipmanı/direnç değeri **okunmaz**.
9. **MP:** rezerv 500 (summon 5 + Mana Shield 150 + Blizzard 200 + pay); rezervin altında yalnızca MP potu (F6-04) ve pozisyon; savaşta oturma **yok** (`docs/08` §11).
10. **Orkestrasyon:** `Intent MageDecide(const DecisionInput &, BrainMemory &)` (öncelik `docs/08` §5: 1 ölüm önleme (F6-05), 2 yakın tehdit (Mana Shield/yavaşlat/kite), 4 patlama (solo penceresi), 5 alan fırsatı, 6 sürekli hasar, 7 destek (**F7**: direnç buff'ı), 8 pozisyon; 3 **summon: yok**); `PolicyL0.h` mage dalı; `BrainDriver.cpp` mage `BuildSkills` (kimlikler, `radius`, `element`, `SkillDamageNominal(meta)`), alan atışında `aim` ile `CastTarget{-1, aimX, y, aimZ, isSelf=false}`, `OnCastResult` ⇒ `ElemEstimator`.
11. Birim testleri (§5.3).

### Summon kararı (docs/17'ye göre; çelişki notu)

**Summon F6'da değildir, F7'dedir.** Dayanak: `docs/17` F6 satırı "Kapsam dışı: TeamBlackboard, çağrılar, **summon**"; F6 "mage (**summon hariç**)"; F7 kapsamı "... **summon akışı (08 §8)**"; G7b "Mage summon: güvenli summon akışı" (T-IGT-MAG-01, T-MAG-05/06, AC-MAG-04/05); `docs/09` §9 ve `docs/08` §8 (SUM-01..07; SUM-04 "takımın en az bir priest'i ve bir başka canlı üyesi mage'den ≤ 40 m" ve SUM-05/06 `TeamBlackboard.READY_FOR_SUMMON` ve takım yenilgi durumu F7 verisidir); F4-34 planı ve ADR-0018 Ek 10 ("güvenlik kapıları bota eklenmez; karar katmanının (F7) işidir"). Bu plan summon skill'ini (`110004`/`210004`, `Type1 8`) **hiçbir koşulda seçmez** (test: `MagNoSummon_InF6`). F7'de: SUM-01..07 kapı fonksiyonu, `READY_FOR_SUMMON` akışı ve `RESPAWNED -> READY_FOR_SUMMON` durum geçişi yazılacaktır (ayrı plan; `docs/08` §12 AC-MAG-04/05).

**Çelişki:** görev tanımındaki "F6-08 mage saldırı **ve güvenli summon**" ifadesi `docs/17` ile çelişir; `docs/08` §1 madde 4-5 summon'u mage davranışına yazar ama faz atamaz. Kod: summon atılabilir (F4-34 `KAPANDI`), güvenlik kapıları **yok** (F4-34 §Kapsam). Karar: F7. Proje sahibi aksini isterse (summon'un tek başına mage için solo anlamı yok: hedef party üyesi gerekir) ayrı karar.

**Kapsam dışı (yapılmayacak)**

- **Summon**, Gate (`110015`) ve Escape/Blink; `READY_FOR_SUMMON`/`REINTEGRATE`; `TeamBlackboard`, ortak hedef, debuff çağrısı ve **patlama penceresi (takım çağrısı)**, direnç buff planı (`TeamPlan.resist`), peel isteği (W-G): F7.
- Absolute power (`110802`, scroll + Stone of Mage), Instantly Magic, Minor Resist (kümelenmiş ortak hedef): bu planda yok (patlama penceresi takım çağrısına bağlı, F7).
- Cure isteği (kök/yavaşlatma altında priest'e): F7.
- `ActionExecutor`/guard/`Telemetry` değişikliği; element tahmininin öğrenmeye (L1) bağlanması.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/MageCombat.h` | yeni | §3 |
| `Tests/BotCoreTests/MageTests.cpp` | yeni | §5.3 |
| `BotCore/PolicyL0.h` | değiştir | yalnızca mage dalı ve `BrainMemory.MageMemory` |
| `GameServer/Bot/BrainDriver.cpp` | değiştir | mage `BuildSkills`, alan atışı hedef noktası, `ElemEstimator` beslemesi |
| `BotCore/BotCore.vcxproj` | değiştir | bir `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | bir `ClCompile` satırı |

`ActionExecutor.*`, `BotSession.h` (gerekmezse), `Telemetry.*`, `docs/`, `tools/` **değişmez**. Listede olmayan dosya gerekirse **durup** Uygulayıcı Raporu'nda soru yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-08 gece/2026-10-02`; F6-06 birleşmiş mi doğrula; sunucular kapalı; `Durum` → `UYGULANIYOR`.
2. `MageCombat.h` yaz; `PolicyL0.h`/`BrainDriver.cpp` eklemeleri. Hasar/heal/MP değerleri `SkillSpec`'ten okunur, kodda sabit değildir.
3. `MageTests.cpp` (adlar **sabit**; sentetik `DecisionInput`):
   - `MagSkill_SingleTarget_MF`: kaçan hedef ⇒ Ice comet; patlama penceresi ve `dist <= 44` ⇒ incineration; standart ⇒ Pillar of fire; MP < rezerv ⇒ cast yok.
   - `MagSkill_SingleTarget_MI`: Prismatic/Ice comet/Ice orb sırası ve yedekler.
   - `MagRange_Band_Hold`: 30–45 m ⇒ `Hold`; 25 m ⇒ geri aç; 50 m ⇒ yaklaş (halka); 46 m'de menzil 45 skill (incineration) seçilmez (guard menzili).
   - `MagKite_Step_Rules`: `docs/08` §7 sözde kodunun iki dalı (`CAST_THEN_MOVE`/`MOVE_THEN_CAST`); `threat.dist > 18` ⇒ yok; yön yoksa ⇒ yok (sıkışma → `last_stand`).
   - `MagAoe_Cluster_Aim_And_Min` (T-MAG-03/AC-MAG-03): 4 düşman 8 m içinde ⇒ ağırlık merkezi hedef noktası, `count >= 3` r15 skill; 2 düşman 8 m ⇒ Fire burst; 2 düşman r15 ⇒ alan yok (tek hedef); küme `Range` dışı ⇒ alan yok (CLI-07).
   - `MagAoe_Packet_Shape`: alan `CastIntent` `aim == true`, `targetId` yok; `isSelf == false` sözleşmesi (BrainDriver testi değil, `Intent` biçimi).
   - `MagElem_Estimate_SwitchAfter3Samples` (T-MAG-04): 3 örnekten önce element değişmez; 3 örnekte ateş oranı en iyinin %70'inin altına düşerse ateş skorları 0,5 ile çarpılır ve buz yedeği seçilir; hedef değişince sıfırlanır.
   - `MagMana_Reserve_And_Pot`: MP < 500 ⇒ yalnızca pot/pozisyon; rezerv üstü ⇒ cast; savaşta `stance.sit` **yok**.
   - `MagManaShield_OnContact`: melee ≤ 5 m ve HP > %50 ve buff yok ⇒ Mana Shield (+ yavaşlatma); buff varsa tekrar yok; HP ≤ %50 ⇒ geri çekilme.
   - `MagCast_TypeGate_And_Gap`: tip kapısı bekliyorsa cast yok; `waitMs` ölçüsü `SkillReady` ile aynı.
   - `MagRetreat_RoleAdj010`: mage için `retreatHpEff = 0,40` (F6-05 sözleşmesi).
   - `MagNoSummon_InF6`: `SkillSpec{type0 = 8}` (summon/Gate/descent) hiçbir durumda seçilmez; `SUM-*` kodu yok.
   - `MagOpening_Slow_Then_Burst`: solo açılış sırası (Ice comet → incineration → Pillar of fire).
   - `Mag_Deterministic_NoRng`: aynı girdi dizisi ⇒ aynı `Intent`; `Rng` çağrılmaz.
4. Proje satırlarını ekle; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu.
5. **Çalışma zamanı (Claude, `/plan-dogrula`; G6c):** `BotMF_K` brain mage, düşman bot(lar) El Morad.
   - **T-MAG-01** (sabit hedefe hasar tempo ve cast/EFFECTING zamanlaması): 10 × 60 sn; AC-MAG-01: geçersiz cast ≤ %2, CLI-03 ihlali 0; MET-ACT-01 ≥ %75.
   - **T-MAG-02** (W-P baskısı altında kiting; **yalnızca tek başına**): 20 tekrar; hayatta kalma süresi `B0-NAIVE` mage'inden (aynı koşul, `/bot brain naive`) kötü değil (AC-MAG-02 birinci yarısı); W-G destekli ölüm oranı F7.
   - **T-MAG-03** (kümelenmiş 4 hedef): 4 El Morad botu ≤ 8 m içinde durur; `ACTION_RESULT.victims` ortalaması ≥ `P-MAG-AOE-MIN − 0,5` (AC-MAG-03).
   - **T-MAG-04** (element direnci giyen hedef): hedefe element direnç buff'ı (`docs/05` priest direnç buff'ları: kimlik yazım turunda belirlenir) ya da ekipman; 3 örnek sonra element değişimi telemetride.
   - **İnsan (proje sahibi):** mage'in kiting ve alan kararı davranış formu (ayrı madde).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; K2: `Debug` rc=0
- [ ] K3: `./tools/run-tests.sh Release|Debug` `0 failed`; on dört yeni test adı `[ OK ]`; mevcut testler (F6-01..F6-07 dahil) değişmeden geçer
- [ ] K4: `MageCombat.h`'te `windows.h|stdafx|GameServer|shared/` yok; `new|malloc|rand(`, global/static değişken yok; ASCII + CRLF
- [ ] K5: summon/Gate/descent/Escape skill'i **hiçbir koşulda** seçilmez (`MagNoSummon_InF6`; `grep -nE "SUM-0|summon|READY_FOR_SUMMON" BotCore/MageCombat.h` yalnızca yorum)
- [ ] K6: alan atışı CLI-07 (hedef noktası `Range` içinde) ve `P-MAG-AOE-MIN` ile birebir; paket hedef kimliği `-1` (yürütücü sözleşmesi)
- [ ] K7: element tahmini yalnızca `HpObs.lastDamage` (kendi isabeti) ve `SkillDamageNominal(SkillSpec.meta)`'dan; düşman direnç/ekipman/MP alanı okunmaz
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F6-08` yalnızca §4'teki dosyalar; `ActionExecutor.*`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K9 (Claude): `docs/17`/`docs/08`'e summon'un F7'ye ait olduğunu ve AC-MAG-02'nin tek başına/W-G ayrımını işler; `P-MAG-*` `[A]` etiketleri; F7 summon planı için not
- [ ] K10 (**çalışma zamanı, Claude**): T-MAG-01..04 ölçütleri (§5 madde 5): AC-MAG-01 ≤ %2 ve CLI-03 ihlali 0, AC-MAG-02 (tek başına ≥ baseline), AC-MAG-03 (`victims` ≥ AOE-MIN − 0,5)
- [ ] K11 (**insan, proje sahibi**): mage davranış formu; yokluğunda G6c `KABUL_EDILDI` olmaz

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Mag(Skill|Range|Kite|Aoe|Elem|Mana|Cast|Retreat|NoSummon|Opening)_|Mag_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F6-08
grep -nE "SUM-0|summon|READY_FOR_SUMMON" BotCore/MageCombat.h
grep -nE "windows\.h|stdafx|GameServer|shared/" BotCore/MageCombat.h
git diff --check gece/2026-10-02...bot/F6-08
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2-§3. **Bota avantaj yok:** düşmanın direnç değeri/MP'si okunmaz; element tahmini yalnızca kendi isabetinden gözlenen hasarla (`docs/08` §6.1: "oyuncunun da yapabileceği bir çıkarım"). Alan atışı hedef noktası kümenin görünür düşmanlarından hesaplanır; görüş alanı dışı yok.
- Eşikler `[Ö]`/`[A]` (özellikle %70 direnç eşiği, solo patlama penceresi, melee saldırı menzili 2,0 m).
- Çelişkiler/belirsizlikler:
  1. **Summon F6 mı F7 mi:** yukarıda; `docs/17` F7, `docs/08` fazsız, görev tanımı F6.
  2. **AC-MAG-02** "W-G desteğiyle ölüm oranı ≤ %20": W-G desteği peel/`TeamBlackboard` ister (F7); F6'da yalnızca "tek başına ≥ baseline" ölçülür. "Baseline" = `B0-NAIVE` mi `baseline-v1` mi tanımsız (`docs/08` §12).
  3. **Patlama penceresi:** `P-MAG-BURST-WINDOW` "takım debuff çağrısından sonraki 6 sn" takım kavramıdır; solo karşılığı bu planın `[A]` kararıdır (yavaşlatma uygulandıktan 6 sn ya da hedef HP < %50).
  4. `docs/08` §7 sözde kodundaki `nav.safe_retreat_direction(m, away_from=threat, toward=team_center)` `NavRetreatPlanner` ile birebir değildir (tek sel/anchor); solo'da anchor = arena kendi yarısı (F6-05).
  5. İncineration/meteor Fall `UseStanding 53` (KI-017): docs "ayakta" der, bot ve sunucu ayakta şartı uygulamaz; menzilde durmak zaten tercih bandının gereğidir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
- Kabul kriterleri öz-değerlendirme (K10-K11 DeepSeek'e ait değildir):
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
