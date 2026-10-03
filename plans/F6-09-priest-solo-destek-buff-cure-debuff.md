# F6-09: Priest solo destek: kendine buff bakımı, cure, tek hedef debuff ve solo savunma (Judgment/Helis) (`BotCore/PriestSupport.h`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; kapı G6b'nin tamamlayıcısı; G7a (F7) bileşenlerini hazırlar) |
| Branch | `bot/F6-09 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-07** `KAPANDI` (`PriestDecide`, `HealTarget`, `KnownHealLog`); F4-28 (Type4 tek hedef/self), F4-32 (Type5 cure `Moral` 2), F4-44 (curse debuff'ları, düşman hedefli), F4-46 (usta skill: Judgment/Helis, Stone of Priest), F4-52 (`SkillEventRing`) `KAPANDI`; **F4-53 (`SkillMeta`, `ObservedStatusTable`, `HealObsRing`) ve F4-60 (sunucu bağlaması: `BotSession::m_status`/`m_healObs`, `FillSkillMeta`) `KAPANDI` ve `gece/2026-10-02`'de (merge `0001d04`, `7891f74`; 2026-10-03 doğrulandı)**: dost/düşman üzerindeki Type4 debuff/buff gözlemi bunlarla bilinir (**Type3 DoT/HoT kaydı F4-53'te yoktur**, ADR-0017 Ek F4-53); **kendi** debuff/buff'ı `SelfState.buffs`'ten bilindiğinden kendine cure/buff gözlem tablosundan bağımsızdır; `UnitView` alanları ve R5 güncellemesi F4-61 (planı yok) |
| İlgili gereksinim / kabul | `docs/07` §7 (buff), §8 (cure), §9.1 (debuff seçimi; çağrı/chat **F7**), §15; `docs/10` §4.3 (priest solo sınırları); MET-BUFF-01/03, MET-CURE-01/02, MET-DEBUFF-*; AC-PRI-04/05/06 (**F7 kabulü**; bu plan yalnızca altyapı ve solo/self ölçümü), AC-SOLO-05'e girdi; CLI-03/04/11; MEC-BUF-02/03 |
| Tahmini büyüklük | M (6 dosya: 1 yeni başlık, 1 yeni test, `PolicyL0.h` ve `BrainDriver.cpp` eklemeleri, 2 proje satırı) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 yazım turu) |

---

## 1. Amaç

Priest'in **kendine** (ve ek olarak tek bir düşman hedefe) uygulayacağı destek eylemlerinin saf mantığı: P-HB için kendi buff'larının bakımı (AC, maks HP, direnç; çakışma kuralları), kritik debuff'ların cure edilmesi (kök/yavaşlatma, Parasite/Malice, DoT), P-HD için solo debuff sırası (Malice → Parasite) ve priest'in kendini savunması (Judgment/Helis, silaha bağlı menzil). Bu davranışlar `docs/07` §4 öncelik listesinin 4 (kritik cure), 7 (debuff), 8 (buff), 10 (melee) basamaklarıdır; F6-07 heal/konum basamaklarını verdi. **Takım düzeyi** (dost cure rezervasyonu, hedef çağrısı + chat, iki priest buff paylaşımı, diriltme, Elysian Web) F7'dedir.

## 1a. Neden TASLAK (HAZIR yapmak için)

Ön koşullar:

1. **F6-07 `KAPANDI`** ve G6b'nin heal kısmı oyun içinde doğrulanmış olmalı.
2. **Gözlenen durum (F4-53/F4-60, `KAPANDI`):** dost/düşman üzerindeki Type4 buff/debuff `ObservedStatusTable::Find/Collect` ile okunur (`E` sınıfı tahmin; başkasının buff'ının gerçek kalkışı yayınlanmaz: kabul edilen sınır). Tablo boş/bayatsa (`statusKnown == false`, ör. görünürlük kaybı sonrası: F4-61) dost cure ve düşman "zaten debuff'lı mı" gözlemi devre dışı kalır, yalnızca **kendi** durumu ve kendi atışlarının sonucu kullanılır (test: `statusKnown == false` dalı). **DoT kalanı tabloda yoktur** (Type3 kaydı kapsam dışı): Cure disease için bu plan **kendine uygulanan** DoT'u `SkillEventRing` olayları + `SkillMeta` (`timeDamage`, `type3DurationSec`) ile kestirir (`DotRemaining`, `E`, `[A]`); kestirim yoksa Cure disease devre dışı.
3. **Çelişki kararı (`docs/07` §7.1 ↔ BUF-HP-01):** priest satırı "massiveness (+1500)" der, ama BUF-HP-01 `argmax(maksHP_buffsuz·0,6, 1500)` priest için (3491·0,6 = 2095) **Undying**'i verir. Yüzde buff'ın hangi HP bileşenine uygulandığı da `[A]` (T-MECH-BUF-03). Bu plan BUF-HP-01'i **harfi harfine** uygular ve sonucu parametre değil **sabit tablo + test** yapar; T-MECH-BUF-03 sonucu gelince Claude docs/kodu birlikte düzeltir.
4. Judgment `UseItem 379066000` (Scroll) ve Stone of Priest tüketimi ölçülmedi (`docs/05` §9.5 "ölçülemeyenler"): `SkillSpec.itemOk` kapısı kullanılır, tüketim sayacı yok.
5. Bu planın **solo ENGAGE kararı** (P-HD/P-HB ne zaman savaşa girer, `docs/10` §4.3) **F6-10**'dadır.

Yazım turunda yeniden doğrulanacak referanslar: `BotCore/Perception.h:1098-1140` (`BuffView{skillId, buffType, isBuff, remainingSec}`, `SelfState.buffs`; buff haritası anahtarı `BuffType`: bir debuff aynı anahtarı **ezer**: `docs/03` MEC-BUF-03, Ek A), `docs/03` Ek A (BuffType 1 HP_MP, 2 AC, 6 SPEED, 8 RESISTANCES, 29 BLOCK_CURSE, 30 BLOCK_CURSE_REFLECT, 40 SPEED2, 47 STUN, 152 SILENCE, 153 NO_POTIONS), `docs/05` §6 (Insensibility peel `112660` 150 MP AC +300 600 sn; massiveness `112657` 360 MP HP +1500; Undying `112654` 240 MP HP %160; Fresh mind `112645` 60 MP; Cure curse `112525` 60 MP; Cure disease `112535` 120 MP; Malice `112703` 40 MP recast 7,4 sn; Parasite `112745` 100 MP; Judgment `112802` 200 MP; Helis `112815` 350 MP; Curse Refraction `112820` 320 MP), `plans/F4-53-algi-gozlenen-durum-tablosu.md` §3 (API adları: `SkillMeta`, `ObservedStatusTable`, `HealObsRing` imzaları), `docs/05` §9.1-§9.5 (F4-42..F4-46 ölçümleri; Moral 4 buff'ları `self` kabul eder: F4-34 notu), `plans/F4-32-aksiyon-yurutucu-type5-cure.md` (cure her zaman `effected` raporlar, hiçbir şey silinmese de: MEC-MAG-19).

## 2. Bağlam (okunması zorunlu)

- `docs/07` §6 (buff paylaşımı: yalnızca P-HB buff atar; P-HD'de AC/HP buff yok), §7.1 (matris: priest satırı AC `Insensibility peel`, HP `massiveness`/`Undying`, direnç `Fresh mind`; **BUF-HP-01**), §7.2 (süre 600 sn; sunucu süresi dolmadan **aynı tipten yenilemeye izin vermez** MEC-BUF-02; "yenileme" = süre dolduktan hemen sonra yeniden uygulama; Malice/Torment AC'yi, Parasite HP'yi siler MEC-BUF-03; debuff süresi (150 sn) dolmadan buff atılamaz ⇒ önce cure curse; ölüm tüm buff'ları siler; savaş içinde buff yalnızca acil heal ihtiyacı yokken), §7.4 (aynı tip buff varken buff gönderilmez; aynı debuff aktifken tekrar atılmaz, kalan süre > 5 sn), §8 (cure tablosu ve öncelikler; `MET-CURE-01 p95 ≤ 3 sn`), §9.1 (debuff seçimi: Malice ucuz açıcı; hedefte Counter Curse/Curse Refraction gözlenmişse debuff yok), §9.2 (hedef çağrısı **F7**), §15 (buff reddedilirse 30 sn tekrar yok; debuff fail ⇒ 10 sn "bağışık" işareti).
- `docs/10` §4.3 (P-HD solo: Malice + Parasite + Judgment/Helis, yalnızca zayıflamış/heal'siz rakibe; P-HB solo: kendine AC/HP buff'ı, ENGAGE yalnızca savunma ya da rakip HP < %35).
- `docs/03` MEC-BUF-02/03/09, MEC-MAG-15 (aynı BuffType tekrar `srv_fail`), MEC-MAG-19 (cure), `docs/16` MET-BUFF-01 (kapsama ≥ %90), MET-BUFF-03 (gereksiz/yinelenen buff = 0), MET-CURE-01/02, MET-DEBUFF-04 (başarısız debuff sonrası çağrı = 0).
- `docs/11` §3.5 (priest cast sırasında pot yok), `docs/07` §12 (buff/debuff **dondurma**: tahmini MP tükenme süresi < 20 sn).

## 3. Kapsam

**Yapılacaklar** (`BotCore/PriestSupport.h`; yalnızca `BotCore/*.h`; global/static değişken yok; dinamik bellek yok)

1. **Kendi buff bakımı (yalnızca P-HB; P-HD'de yalnızca master self buff'ları)** `bool ChooseSelfBuff(const DecisionInput &, PriestMemory &, CastIntent & out)`:
   - **Matris (priest satırı):** AC = `Insensibility peel` (BuffType 2); HP = **BUF-HP-01**: `argmax(maksHP_buffsuz * 0,6, 1500)` ⇒ Undying (`maxHp_buffsiz` 3491 ⇒ 2095 > 1500) ya da massiveness; direnç (BuffType 8) = `Fresh mind` (solo'da elementi bilinmiyorsa); mage satırları F7.
   - **Eksiklik:** buff, `SelfState.buffs`'te **yoksa** (ölüm sonrası, süre doldu); bir **debuff** varsa (`isBuff == false` aynı `BuffType`) ⇒ önce cure (aşağıda), buff **atılmaz** (MEC-BUF-02/03). Kalan süre `remainingSec` bilinirse yenileme **yalnızca süre bittikten sonra**; `P-PRI-BUFF-REFRESH` yalnızca takım planında (F7) anlamlıdır.
   - **Kapı:** savaş dışı veya acil heal ihtiyacı yok; `mp - msp >= priMpReserve`; tahmini MP tükenmesi < 20 sn ise dondur (`docs/07` §12); buff reddi (`srv_fail`) ⇒ o buff **30 sn** denenmez (`PriestMemory.buffRetryAtMs[]`); aynı tip varken gönderim **yok** (MET-BUFF-03 = 0).
   - Sıra: AC → HP → direnç. Tick başına en çok bir cast; `Moral 4` buff'ları `self` kabul eder (F4-34 notu): `CastIntent.self = true`.
2. **Cure** `bool ChooseCure(const DecisionInput &, PriestMemory &, CastIntent & out)` (`docs/07` §8; **kendine** ve — `statusKnown` ise — `TeamView` üyesine):
   - **Self (kesin bilgi, `SelfState.buffs`):** kök/yavaşlatma (`BuffType` 6/40/47 `isBuff == false`) **ve** geri çekiliyor ya da kaçmaya çalışıyor ⇒ **kritik**; Parasite/Malice (`BuffType 1`/`2` debuff) ve `ratio < 0,6` (HP) ⇒ kritik; Malice/Torment (AC debuff) ve düşman melee teması (8 m) ⇒ yüksek; Silence (`152`) altında **cure edilemez** (skill yok: geri çekil).
   - **Dost (`statusKnown`):** aynı tablo `ObservedStatusTable` kayıtlarından; çift cure yok (kendi cast'imiz sürerken aynı hedefe ikinci cure yok).
   - **Cure disease** (`112535`): kalan DoT toplamı `>= P-PRI-CURE-DOT-MIN` (800 HP; `DotRemaining` = kendine uygulanan Type3 DoT'lar için `|SkillMeta.timeDamage| * kalan_tik`, olaydan + `type3DurationSec`; kestirim yoksa **devre dışı**).
   - Skor = Σ debuff ağırlıkları × rol ağırlığı × aciliyet (`docs/07` §8: birden çok debuff taşıyan daha değerli; mage ×1,3 yalnızca F7 üyeleri için); **tek** Cure curse tüm Type4 debuff'ları kaldırır (REMOVE_TYPE4).
   - Cure gecikme hedefi p95 ≤ 3 sn (MET-CURE-01): kritik cure heal'den **sonra** (acil heal birinci öncelik) ama normal heal'den önce.
3. **Debuff (yalnızca P-HD, solo hedef; çağrı/chat yok)** `bool ChooseDebuff(const DecisionInput &, const TargetChoice &, PriestMemory &, CastIntent & out)` (`docs/07` §9.1, solo kısmı):
   - **Malice** (`112703`, 40 MP, recast 7,4 sn): hedefte AC debuff'ı **yoksa** (kendi atış belleği: son başarılı Malice < 150 sn önce ⇒ "var"; `statusKnown` ise gözlenen kayıt) ve `mp - 40 >= priMpReserve`;
   - **Parasite** (`112745`, 100 MP): hedefte gözlenen HP buff'ı (BuffType 1) var ya da hedef warrior (yüksek HP) ve Malice zaten uygulandı;
   - **Counter Curse (29)/Curse Refraction (30)** gözlenmişse **debuff yok**; debuff `srv_fail`/direnildi ⇒ hedef 10 sn "debuff bağışık" (`PriestMemory.immuneUntilMs`); aynı debuff aktifken (kalan > 5 sn) tekrar yok.
   - **Torment/Massive/Slow/Sweep mana**: bu planda **seçilmez** (alan/takım/ek hedef F7); P-HB'de debuff ağacı yoktur.
   - `TargetCall`/`HEDEF:` chat **üretilmez** (F7; `MET-DEBUFF-04` = 0 bu planda zaten sağlanır).
4. **Solo savunma/melee** `bool ChooseSelfDefenseAttack(...)` (`docs/07` §4 madde 10; `docs/10` §4.3): Judgment (`112802`, 200 MP, `UseItem` scroll, `Type1 %500 +150`) → Helis (`112815`, 350 MP, `%400 +400`), **silaha bağlı menzil** (`Range 0`, priest sopası `ITEM.Range 10` ⇒ `weaponRangeField 10` = **1,0 m**; F4-46 hedefleri 0,3-0,5 m'de): yalnızca hedef ≤ 1,0 m ve heal ihtiyacı yokken; `SkillReady` + `itemOk`; ENGAGE kararı F6-10.
5. **Orkestrasyon:** `PriestDecide` (F6-07) sırası `docs/07` §4'e tamamlanır: 1 geri çekilme > 2 acil heal > **4 kritik cure** > 5 normal heal > (P-HD) **7 debuff** > (P-HB) **8 buff** > 9 konum > **10 melee** (yalnızca solo ve heal ihtiyacı yokken); her basamakta "tek cast, tek yuva" (`m_castPhase`). `PolicyL0.h` priest dalı bu fonksiyonları çağırır; `BrainDriver.cpp` priest `BuildSkills`'e buff/cure/debuff/melee kimliklerini ekler (El Morad `+100000`) ve **düşman hedefli** `BeginCast` için hedef adını `UnitView.name`'den verir.
6. **Sonuç geri bildirimi:** `PriestMemory::OnCastResult(skillId, targetId, effected, srvFail, nowMs)`: buff başarısı ⇒ yeniden deneme yok; `srv_fail` ⇒ 30 sn; debuff `effected` ⇒ `debuffUntilMs[hedef][tip] = now + 150 sn`; debuff `srv_fail`/`missed` ⇒ `immuneUntilMs = now + 10 sn`; cure `effected` ⇒ cure sayacı (cure her zaman `effected` raporlar: yararlılık `SelfState.buffs`'te debuff'ın kalkmasından doğrulanır).
7. **Telemetri girdisi (F6-06 altyapısı):** `BUFF_APPLY`/`BUFF_REMOVED` ve `DECISION.reason` (`EMERGENCY_HEAL_RULE`, vb.) yazımı `BrainDriver`'dadır; bu planda yalnızca gerekçe kodu üretilir.
8. Birim testleri (§5.3).

**Kapsam dışı (yapılmayacak)**

- **Takım düzeyi:** buff matrisinin warrior/mage satırları, `Greatness`/grup buff, iki priest buff paylaşımı (`BUFF_COVERAGE_LOST`), dost cure rezervasyonu ve çakışması, hedef çağrısı + party chat, Elysian Web, diriltme (`ResIntent`), Torment/Massive/Slow/Sweep mana: **F7** (G7a: T-PRI-03..06/08, AC-PRI-03..08).
- Heal kararları (F6-07); solo ENGAGE/AVOID (F6-10).
- `ActionExecutor`/guard/`Telemetry` değişikliği; `UnitView` gözlenen durum alanları (F4-61: planı yok); Type3 DoT/HoT gözlem tablosu (F4-53 kapsamı dışı; ayrı algı dilimi önerisi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/PriestSupport.h` | yeni | §3 |
| `Tests/BotCoreTests/PriestSupportTests.cpp` | yeni | §5.3 |
| `BotCore/PolicyL0.h` | değiştir | yalnızca priest dalının sırası ve `PriestMemory` alanları |
| `GameServer/Bot/BrainDriver.cpp` | değiştir | priest `BuildSkills` ek kimlikler, düşman hedefli cast adı, `OnCastResult` beslemesi |
| `BotCore/BotCore.vcxproj` | değiştir | bir `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | bir `ClCompile` satırı |

`ActionExecutor.*`, `BotSession.h`, `Telemetry.*`, `docs/`, `tools/` **değişmez**. Listede olmayan dosya gerekirse **durup** Uygulayıcı Raporu'nda soru yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-09 gece/2026-10-02`; F6-07, F4-53 ve F4-60 birleşmiş mi doğrula (`git log --oneline | grep -E "F4-53|F4-60"`); sunucular kapalı; `Durum` → `UYGULANIYOR`.
2. `PriestSupport.h` yaz; `PolicyL0.h`/`BrainDriver.cpp` eklemeleri. Skill maliyetleri/süreleri `SkillSpec`'ten; yalnızca BUF-HP-01 çarpanları ve 800/150/30/10 sn sabitleri `docs/07`'den.
3. `PriestSupportTests.cpp` (adlar **sabit**):
   - `PriBuff_Matrix_HB_Self`: P-HB: AC → HP → direnç sırası; P-HD'de self buff yok.
   - `PriBuff_HpBuff_Rule_BUF_HP_01`: `maxHp_buffsiz 3491` ⇒ Undying; `1500 > 0,6 * maxHp` olan senaryoda (ör. 2000) ⇒ massiveness.
   - `PriBuff_NoRecastWhilePresent_And_30sRetry`: buff varken gönderim yok (MET-BUFF-03 = 0); `srv_fail` sonrası 29999 ms'de yok, 30000 ms'de var.
   - `PriBuff_NotInCombatOrEmergency`: acil heal ihtiyacı/savaş içi ⇒ buff yok; `mp - msp < 1100` ⇒ yok; tahmini MP tükenmesi < 20 sn ⇒ dondurulur.
   - `PriBuff_Debuffed_NeedsCureFirst`: kendinde Malice (BuffType 2 debuff) ⇒ buff yok, cure seçilir; cure sonrası buff.
   - `PriCure_Self_Critical_SlowRoot`: kök/yavaşlatma + geri çekiliyor ⇒ kritik cure; geri çekilmiyor ⇒ düşük öncelik; Silence ⇒ cure yok.
   - `PriCure_Priority_And_NoDouble`: acil heal > kritik cure > normal heal; aynı hedefe ikinci cure yok.
   - `PriCure_Disease_DotThreshold_800`: DoT kalanı 799 ⇒ yok, 800 ⇒ Cure disease; kestirim yok ⇒ devre dışı.
   - `PriDebuff_Malice_Then_Parasite_Solo`: Malice yok ⇒ Malice; Malice < 150 sn ⇒ Parasite (HP buff'lı/warrior); aynı debuff kalan > 5 sn ⇒ yok.
   - `PriDebuff_Skip_CounterCurse_And_ImmuneWindow`: BuffType 29/30 gözlenmiş ⇒ debuff yok; `srv_fail` ⇒ 10 sn bağışık (9999/10000 ms).
   - `PriDebuff_NoCall_NoChat`: `TargetCall`/chat yuvası **yok** (F7); P-HB'de debuff seçilmez.
   - `PriMelee_Judgment_Helis_Range_And_Mp`: hedef 1,0 m içinde ve heal ihtiyacı yok ⇒ Judgment; 1,1 m ⇒ yok; `itemOk == false` ⇒ Helis'e düşer; MP rezervi.
   - `PriSupport_Priority_Order`: `docs/07` §4 sırası (1 > 2 > 4 > 5 > 7 > 8 > 9 > 10) tek tick'te çakışan adaylarla.
   - `PriSupport_Deterministic_NoRng`: aynı girdi + tohum ⇒ aynı `Intent`; `Rng` çağrılmaz.
4. Proje satırlarını ekle; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu.
5. **Çalışma zamanı (Claude, `/plan-dogrula`):** `BotPHB_K` ve `BotPHD_K` brain; düşman bot(lar) yavaşlatma/Malice/Parasite atar (F4-44/F4-45 betik üslubu).
   - **Self buff bakımı** (P-HB, 10 dk): MET-BUFF-01 (self) ≥ %90, MET-BUFF-03 = 0 (yinelenen gönderim), debuff'la silinen buff'ın cure sonrası yeniden kurulması.
   - **Cure gecikmesi** (kök/yavaşlatma/Malice altında self): MET-CURE-01 p95 ≤ 3 sn.
   - **Solo debuff** (P-HD): Malice → Parasite sırası, aynı debuff yeniden atılmaz, `srv_fail` sonrası bağışık penceresi telemetride.
   - **Solo savunma:** Judgment/Helis 1,0 m'de; hedefe yaklaşma F6-10 kararına bağlı (bu planda elle yerleştirme).
   - **İnsan (proje sahibi):** priest'in buff/cure/debuff sezgisi (ayrı form maddesi).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; K2: `Debug` rc=0
- [ ] K3: `./tools/run-tests.sh Release|Debug` `0 failed`; on dört yeni test adı `[ OK ]`; mevcut testler (F6-01..F6-08 dahil) değişmeden geçer
- [ ] K4: `PriestSupport.h`'te `windows.h|stdafx|GameServer|shared/` yok; `new|malloc|rand(`, global/static değişken yok; ASCII + CRLF
- [ ] K5: aynı `BuffType` buff'ı varken gönderim yok; debuff'lı `BuffType` için buff yok; buff reddi 30 sn; debuff bağışıklığı 10 sn (testler)
- [ ] K6: `TargetCall`/chat/diriltme/Elysian Web/grup buff **kodda yok** (`grep -nE "TargetCall|CHAT|Resurrect|112825|Greatness" BotCore/PriestSupport.h` yalnızca yorum)
- [ ] K7: düşman/dost durumu yalnızca `SelfState`, kendi cast sonuçları ve (varsa) `ObservedStatusTable` gözleminden; düşman MP/cooldown/envanteri okunmaz
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F6-09` yalnızca §4'teki dosyalar; `ActionExecutor.*`, `BotSession.h`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K9 (Claude): BUF-HP-01 ↔ matris çelişkisini ve `T-MECH-BUF-03` bekleyen ölçümünü docs'a işler; `docs/07` §3 `P-PRI-CURE-DOT-MIN`/buff sabitleri `[A]`; F4-61 (gözlenen durum: görünürlük kaybı ve yeniden giriş, `UnitView` alanları) ve Type3 DoT gözlem tablosu ihtiyacını planlar
- [ ] K10 (**çalışma zamanı, Claude**): §5 madde 5 ölçütleri: MET-BUFF-01 (self) ≥ %90, MET-BUFF-03 = 0, MET-CURE-01 p95 ≤ 3 sn; AC-PRI-04/05/06 **F7'de** ve takım düzeyinde kabul edilir
- [ ] K11 (**insan, proje sahibi**): priest destek davranışı formu; yokluğunda G6b tamamlanmış sayılmaz

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Pri(Buff|Cure|Debuff|Melee|Support)_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F6-09
grep -nE "TargetCall|Resurrect|112825|Greatness" BotCore/PriestSupport.h
grep -nE "windows\.h|stdafx|GameServer|shared/" BotCore/PriestSupport.h
git diff --check gece/2026-10-02...bot/F6-09
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2-§3. **Bota avantaj yok:** düşman üzerindeki debuff/buff durumu yalnızca **görülen** `WIZ_MAGIC_PROCESS` olaylarından ve kendi atışlarının sonucundan türetilir (tahmin `E` sınıfı, hata payı kaydedilir; `docs/13` §5.2a); bitiş = olay + süre, başkasının buff'ının gerçek kalkışı yayınlanmaz (ADR-0017 Ek F4-53 §5 "kabul edilen sınırlar").
- Çelişkiler/belirsizlikler:
  1. **Faz sınırı:** `docs/17` §5 G7a "Buff, cure, debuff, diriltme, iki priest" ve AC-PRI-04..06 F7'dedir; görev tanımı "F6-07 priest heal/buff/cure/debuff döngüleri (kendine/tek müttefik)" der. Bu plan **solo/self** kısmı F6'da, **takım düzeyini** F7'de bırakır.
  2. **BUF-HP-01 ↔ matris** (yukarıda §1a madde 3).
  3. **Buff takibi:** `SelfState.buffs` Type4 listesidir (`kSnapMaxBuffs 16`); Type3 HoT (Superior restore) ve DoT'lar orada **görünmez**: HoT takibi kendi atışından (F6-07 `restoreUntilMs`).
  4. `docs/07` §7.2 "buff **yenileme** = süre dolduktan hemen sonra" ↔ `P-PRI-BUFF-REFRESH` (20 sn kala yanında ol) yalnızca konum planı içindir; bu planda `remainingSec` kalan süre sıfır olunca yeniden atış.
  5. Cure `effected` hiçbir şeyin silindiğini kanıtlamaz (MEC-MAG-19): başarı **kendi** debuff'ının kalkmasından doğrulanır; dost için gözlenen durum yoksa (`statusKnown == false`) doğrulama yok `[A]`.

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
