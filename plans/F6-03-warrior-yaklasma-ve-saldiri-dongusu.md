# F6-03: Warrior yaklaşma ve saldırı döngüsü: nav sözleşmesi, sprint, normal saldırı (R) + Type1 skill seçimi (`BotCore/WarriorPressure.h`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; kapı G6a) |
| Branch | `bot/F6-03 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-01**, **F6-02** `KAPANDI`; F4-02 (R, CLI-01), F4-03/F4-24 (cast, iptal, `UseStanding`), F4-26/F4-37 (çift tipli `{1,4}`: leg cutting, Scream), F4-28 (Type4 self: sprint, Outrage), F4-36 (eşyalı sınıf skill kapısı), F4-45 (warrior skill ölçümü, `docs/05` §9.4) `KAPANDI`; F5-04 (`NavFollower` halka sözleşmesi), F5-50 (kiriş denetimi), F5-57 (`NavProgressVerdict`) `KAPANDI` |
| İlgili gereksinim / kabul | `docs/06` §3-§8, §10 (T-WAR-01..03, T-WAR-05), AC-WAR-01/02/03/05; CLI-01/02/03/04/05/09/11; MET-ACT-01/02, MET-TGT-01/02 (ölçüm F6-06); `docs/03` §13.1 (skill + R kombosu) |
| Tahmini büyüklük | M (4 dosya: 1 yeni başlık, 1 yeni test dosyası, 2 proje satırı; ~600 satır mantık) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 yazım turu) |

---

## 1. Amaç

Seçilmiş hedefe (F6-02 çıktısı) bir warrior botun **yaklaşması, menzili koruması ve her sunucu saniyesinde izinli en yüksek tempoda saldırması**nın saf mantığı: nav ile yaklaşma ve son metrede doğrudan adım, sprint kullanımı, hedef kaçıyorsa leg cutting → Scream → takip, takip sınırında bırakma, menzilde R + tek Type1 skill (`docs/06` §6.2 tablosu), MP rezervinin korunması. Çıktı `Intent` (F6-01): hareket, `attack`, `cast` yuvaları. Oyun içi çalışma F6-06'dadır.

## 1a. Neden TASLAK (HAZIR yapmak için)

Ön koşullar:

1. **F6-01 ve F6-02 `KAPANDI`**.
2. **Nav sözleşmesi**: F5-55 dilimleri (`plans/F5-55-...md` §1A, 2026-10-03): **F5-59** `NavService` yaşam döngüsü ve harita yükleme (`HAZIR`; `bot/F5-59` dalı açık), **F5-61** kiriş guard'ı, **F5-62** `/bot goto` + waypoint zinciri + `NavPathfinder`/`NavReach`/maliyet katmanı kurulumu, **F5-63** `NavFollower`/takılma/F5-57 sözleşmesi, **F5-64** bütçe + `NAV_*` telemetri, **F5-65** nav durumu temizliği, **F5-66** çalışma zamanı doğrulama (F5-61..F5-66 `TASLAK`; F5-60/F5-67 yalnızca su denetimi/koşullu düzeltme): `NavService`/`NavFollower` bağlamasını, kiriş guard'ını (CLI-08) ve `NavView.targets[].directClear/pathM` üretimini sağlamalı. **Dilim boşluğu (2026-10-03 kontrolü):** `NavRetreatPlanner` (güvenli nokta) çağrısı, `NavReachJudge` sonucunun karar katmanına iletilmesi ve `directClear` (düz kiriş) sorgusu bu dilimlerin satırlarında **yazılı değildir** (`plans/F5-55` §1A; F5-59 §3 "kapsam dışı" yalnızca `NavReach` kurulumunu F5-62'ye bırakır). İlgili dilim planı genişletilmeli ya da ek bir F5 dilimi (öneri, kimlik atanmadı) yazılmalıdır. Bu plan nav'ı **çağırmaz**; yalnızca `Intent.move` ile niyet bildirir ve `NavView`'den okur. `NavView.serviceUp == false` iken davranış: hedefe `MoveKind::Hold` (hareket yok) ve `RuleStuck` yok; testle sabit.
3. `docs/06` §3/§6.2 MP rezervi anlamının netleşmesi (aşağıda "Çelişkiler" 2) ve melee menzil formülü (1): Claude `docs/06`'yı bu planın kararına göre düzeltir ya da proje sahibi farklı seçerse parametre değişir.
4. `SkillSpec` listesinin bağlama tarafından doldurulması (F6-06): bu plan skill kimliklerini sabit adlarla bilir ama **sunucu verisini kopyalamaz**.

Yazım turunda yeniden doğrulanacak referanslar: `BotCore/BotCombat.h:11-80` (`AttackIntervalMs`, `AttackRangeField`, `DistanceField`, `CheckAttack`), `:151-236` (`CheckCastStart`), `BotCore/BotMotion.h:13-20` (`kWalkSpeedField 45`, `kSprintSpeedField 67`, `kMovePeriodMs 1500`), `BotCore/NavTrack.h:127-167` (`NavFollowParams.ringMinM/ringMaxM`; 4 m ızgarada etkin üst sınır ≥ 2,83 m), `plans/F4-45-...md` §2 (warrior skill veri tablosu: `Msp/CastTime/ReCastTime/Range/Type1/Type2/Moral`, hepsinde `CastTime 0`; `Range 0` ⇒ silaha bağlı menzil, Raptor `ITEM.Range 20` ⇒ `weaponRangeField 20` = **2,0 m**), `docs/05` §9.4 (ölçülmüş warrior skill sonuçları; KI-021 leg cutting dönüşümlü `srv_fail`), `KNOWN_ISSUES.md` KI-017 (master skill `UseStanding` 51..54 ⇒ ayakta şartı **uygulanmaz**).

## 2. Bağlam (okunması zorunlu)

- `docs/06` §3 (parametreler), §4 (karar öncelikleri 1-9), §5 (çakışma/bilgi kuralları), §6.1 (zamanlama: Type1 sunucu saniyesi başına ≤ 1; R `max(gecikme, 1 sn)` ve farklı sunucu saniyesi; skill ile R arası **kilit yok** CLI-02), §6.2 (Type1 seçim tablosu), §6.3 (sözde kod), §7 (durumlar ve geçişler, histerezis), §8 (hata/fallback), §9 (solo farkları).
- `docs/10` §4.1 (solo warrior: hazırlık Gain/Outrage, sprint ile yaklaşma, kaçarsa leg cutting sonra Scream), §6 (sprint'i mesafe ≤ 25 m'de kullan: kiter mage uyumu), P-SOLO-CHASE-MAX (60 m veya 10 sn).
- `docs/03` §13.1 ("skill + R" **ayrı ayrı geçerli** iki aksiyonun aynı pencerede gönderilmesi), CLI-01 (`delaytime = Delay + 10`), CLI-04 (Type1 tip kapısı ≥ 1 sn), CLI-05 (yürüme 45 / sprint 67, paket ~1,5 sn), CLI-09, CLI-11 (≤ 6 aksiyon/sn).
- `docs/05` §5 (warrior skill tablosu; Carving `106525` 90 MP, prick `106535` 120, leg cutting `106520` 84, Scream `106802` 300, sword dancing `106560` 300, Howling `106570` 400, sprint `106001` 5, Outrage `106720` 60, restoration `106730` 105; El Morad = Karus + 100000), §5.1 (mekanik sınırlar).
- `docs/12` §4.2 (menzil halkası: warrior melee halkası; **4 m ızgarada etkin ring üst sınırı ≥ 2,83 m**, yani nav son birkaç metreyi çözemez), §6 (hareket paketi ~1,5 sn; adım ~6,75 m yürüme / ~10 m sprint).
- `docs/13` §7.1 (acil kurallar → utility → geçerlilik filtresi → eşitlikte seed'li rastgele).

## 3. Kapsam

**Yapılacaklar** (`BotCore/WarriorPressure.h`; yalnızca `BotCore/*.h`; global/static değişken yok; dinamik bellek yok)

1. **Skill adları ve kimlikler:** `enum class WarSkill { Carving, Prick, LegCutting, Scream, SwordDancing, Howling, Sprint, Outrage, Restoration }`, `inline uint32_t WarSkillId(WarSkill, uint8_t nation)` (Karus `106525`, `106535`, `106520`, `106802`, `106560`, `106570`, `106001`, `106720`, `106730`; El Morad `+100000`; `docs/13` §5.3 kuralı) ve `inline const SkillSpec * FindSkill(const DecisionInput &, uint32_t id)`.
2. **Geometri:** `MeleeRangeM(timers)` = `weaponRangeField / 10` (silahsız `kEmptyHandRangeField`); `StandOffM(params, rangeM)` = `clamp(warStandOffM, 0.3, rangeM - 0.3)` (varsayılan 1,0 m; `P-SK-RANGE-MARGIN` 1,5 m **menzilli** skill'ler içindir, 2,0 m'lik melee için 0,5 m'ye düşerdi: "Çelişkiler" 1); `StandOffPoint(self, target, standOff)` = hedef konumundan bota doğru `standOff` m; `InMeleeRange(distM, timers)` = `DistanceField(distM) <= weaponRangeField` (guard ile **aynı** ölçüt); hedef konumu `UnitView.x/z` (son gözlem), yaklaşma hedefi `x + vx * min(posAgeMs, 3000)/1000` (`E` etiketli kestirim).
3. **Kaçma tahmini:** `struct WarriorMemory` içinde 8'lik mesafe örnek halkası; `IsFleeing(mem, targetSpeedMps, ownSpeedMps, params)` = hedef hızı ≥ `warSlowTrigger` × kendi hızı **ve** son 2 sn'de mesafe > 0,5 m arttı (`docs/06` §3 `P-WAR-SLOW-TRIGGER`); `PRESSURE -> PURSUE` için 0,5 sn histerezis (menzile dönerse geçiş yok, `docs/06` §7).
4. **Durum/niyet** `Intent DecideWarriorPressure(const DecisionInput & in, const TargetChoice & tgt, WarriorMemory & mem, Rng & rng)`:
   - Hedef yoksa: `sub = None`, `move = Hold`, `attack/cast` yok (`RuleNoTarget`).
   - `Approach` (menzil dışı): `dist > warNavHandoffM` (8 m `[A]`) ise `MoveKind::FollowTarget` (halka `[0, 4]` m: nav ızgarası sınırı); `dist <= warNavHandoffM` ve `nav.targets[i].directClear` ise `MoveKind::DirectStep` → `StandOffPoint`; değilse `FollowTarget`. `speedField` = sprint buff'ı varken 67 (`SelfState.buffs` içinde `BuffType 6` ve `isBuff`), değilse 45; kendi üzerinde `BuffType 6` **debuff** varsa 45 ve sprint istenmez (yavaşlatma yüzdesi `[A]`, CLI-05).
   - **Sprint** (`docs/06` §6.3 `far(...) and sprint_ready`): `dist > warSprintMinDist` (15 m `[A]`) ve sprint buff'ı yok ve `SkillReady(Sprint)` ve `mp >= 5` ise `cast = {Sprint, self}` (tip kapısı Type4 `typeGateWaitMs[4]`).
   - `Pressure` (menzilde): `move = Hold` (hareket yoksa yürütücü paket üretmez; hareket varsa durma paketi, CLI-09); `attack.active = true` ise `rWaitMs == 0` ve `InMeleeRange`; R ile skill **aynı tick'te** olabilir (CLI-02: kilit yok).
   - `Pursue` (hedef kaçıyor ve menzil dışı): `FollowTarget`/`DirectStep` + sprint; takip sınırı **solo**: başlangıçtan `P-SOLO-CHASE-MAX` (60 m) kat edildi veya 10 sn menzile girilemedi → `sub = Disengage`, `reason = RuleDisengage` (orkestratör F6-06 hedefi `TargetMemory`'de bırakır: `TargetUnreachable` benzeri `abandoned[]`); hedef kendi tower halkasına girdi (`inForbidden`) → hemen `Disengage`.
   - `Disengage -> ` hedef bırakılır; sonraki tick'te F6-02 yeni hedef seçer.
5. **Type1 seçimi** `PickType1(...)` (`docs/06` §6.2; **ilk eşleşen kazanır**; yalnızca `typeGateWaitMs[1] == 0` ve `InMeleeRange` iken; hepsi `SkillReady(...).ready` ister; MP kuralı §3.1):
   1. hedef kaçıyor ve leg cutting hazır ve **son 10 sn'de leg cutting uygulanmadı** (`mem.legCuttingAppliedMs`; KI-021: debuff süresi dolmadan yeniden atış dönüşümlü `-103` verir) → **leg cutting**;
   2. kaçıyor, leg cutting yok/yasak, Scream hazır ve `itemOk` ve `mp >= 300` → **Scream** (rezervin harcanacağı skill: §3.1);
   3. hedef HP **biliniyor** ve `hp/maxHp < warFinishHp` (0,25), `mp - 400 >= rezerv` → **Howling Sword**; değilse `mp - 300 >= rezerv` → sword dancing;
   4. hedefin kaçınması yüksek (son 10 Type1 denemesinde ıska ≥ %30 ve ≥ 5 örnek; `OnCastResult("missed")`) ve `mp - 120 >= rezerv` → **prick**;
   5. varsayılan: `mp - 90 >= rezerv` → **Carving**;
   6. hiçbiri → Type1 yok (yalnızca R ve MP potu, F6-04).
   "Eşit geçerli seçenekler arasındaki sıra" L1'e açıktır (`docs/06` §6.2 son cümle); bu planda sabit ve belirlenimli (rastgelelik yok; `Rng` yalnızca ileride eşitlik için ayrılmış, kullanılmazsa test bunu doğrular).
   - 3.1 **MP rezervi anlamı (karar, "Çelişkiler" 2):** `warMpReserve` (700) **korunan** MP'dir: leg cutting (84), Scream (300), sprint (5) rezervi harcayabilen skill'lerdir (`mp >= maliyet`); Carving/prick/sword dancing/Howling rezerve **dokunmaz** (`mp - maliyet >= rezerv`). Rezervin altında MP varsa yalnızca R, leg cutting, Scream, sprint ve MP potu kalır.
6. **Hazırlık/ek buff** `PrepareBuffs(in, mem)`: savaş dışında (`hedef yok` ya da `dist > 25 m`): Outrage (`106720`, 60 MP) etkin değilse (`SelfState.buffs`'te `BuffType 5` yok, `mp - 60 >= rezerv`) `cast = {Outrage, self}`. **Restoration** (`106730`, HoT 60 sn, recast 25 sn; Type3 HoT `SelfState.buffs`'te görünmez: `mem.restorationUntilMs` kendi atışından izlenir) yalnızca HP < %70 ve etkin değil iken (tam canlı hedefe heal sunucuda `-100` verir: F4-43 bulgusu; HoT'un tam canda atılması engellenir). Gain/Defense (`docs/06` §5 çakışma kuralları; kimlikler `docs/05` tablosunda yok) **bu planda yok**.
7. **Takılma/nav:** `nav.stalled` veya `nav.blockedByGuard` iken karar katmanı `move`'u üretmez (`MoveKind::None`, `reason = RuleStuck`): kurtarma merdiveni nav bağlamasındadır (F5-09/F5-57); `nav.abandon` → `Disengage`.
8. **Sonuç geri bildirimi:** `WarriorMemory::OnCastResult(uint32_t skillId, bool effected, bool missed, uint64_t nowMs)` (`ActionExecutor` `CastOutcome` sonucu F6-06'da eşlenir): leg cutting `effected` → `legCuttingAppliedMs`; Type1 ıska halkası; restoration `effected` → `restorationUntilMs = now + 60 s`.
9. Birim testleri (§5.3).

**Kapsam dışı (yapılmayacak)**

- `PEEL` (priest/mage'e temas eden düşman): F7 (`TeamBlackboard`); `Peel` alt durumu üretilmez. W-G ana görevi (koruma) F7.
- Healer'a geçiş, takım çağrısı/debuff penceresi (`P-WAR-BURST` "takım çağrısı < 10 sn": F7); Shock Stun, Exceed Break, Binding/provoke (Type7, MB-10), `descent` (Type8 warp): bu planda seçilmez.
- Geri çekilme/pot kararı: F6-05/F6-04 (acil kurallar F6-06 orkestratöründe önce çalışır).
- Kalkan/silah değiştirme, transform, rogue/archer.
- `ActionExecutor`/`BotSession`/`BotManager` değişikliği, telemetri yazımı, nav bağlama: F6-06 ve F5-55 dilimleri.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/WarriorPressure.h` | yeni | §3 |
| `Tests/BotCoreTests/WarriorTests.cpp` | yeni | §5.3 |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca bir `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca bir `ClCompile` satırı |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-03 gece/2026-10-02`; F6-01/F6-02 birleşmiş mi doğrula; `Durum` → `UYGULANIYOR`.
2. `WarriorPressure.h` yaz. Menzil/maliyet/hazırlık kararları `SkillReady` ve `BotCombat.h` guard fonksiyonlarından türetilir; **kendi zamanlama mantığı yazılmaz**. Sunucu mekaniği (hasar, recast değerleri) kopyalanmaz: `SkillSpec`'ten okunur.
3. `WarriorTests.cpp` (adlar **sabit**; sentetik `DecisionInput`, `Rng` sabit tohum; Raptor `weaponRangeField = 20`, `weaponDelay = 164`):
   - `WarApproach_Handoff_NavVsDirect`: 30 m → `FollowTarget`; 7 m ve `directClear` → `DirectStep`; 7 m ve `!directClear` → `FollowTarget`; `serviceUp == false` → `Hold`.
   - `WarApproach_StandOffPoint`: standoff 1,0 m ve menzil 2,0 m; hedef menzilin içindeyken `Pressure`.
   - `WarApproach_SprintRules`: 16 m ve sprint yok ve hazır → `cast = Sprint`; sprint zaten etkin → tekrar yok, `speedField 67`; yavaşlatma debuff'ı → 45 ve sprint yok; `dist <= 15` → sprint yok.
   - `WarAttack_R_ReadyAndRange`: `rWaitMs == 0` ve `DistanceField <= 20` → `attack.active`; `rWaitMs > 0` veya `distanceField 21` → yok; R ve Type1 aynı tick'te birlikte (CLI-02).
   - `WarSkill_Default_Carving`: MP yeterli, hedef kaçmıyor, HP bilinmiyor → Carving.
   - `WarSkill_Fleeing_LegCutting_Then_Scream`: kaçan hedef → leg cutting; leg cutting yasak penceredeyse Scream; Scream `itemOk == false` → Carving'e düşmez, yalnızca R (kaçış durumunda Carving seçilmez: tablo sırası).
   - `WarSkill_LegCutting_NoRecastWithinDebuff` (KI-021): leg cutting `effected` sonrası 9999 ms'de yeniden seçilmez, 10000 ms'de seçilir.
   - `WarSkill_Finish_Howling`: hedef HP %24 (bilinen) ve `mp - 400 >= 700` → Howling; HP bilinmiyor → Howling seçilmez; `mp - 400 < 700` → sword dancing/Carving yolu.
   - `WarSkill_MpReserve_Keeps_Scream`: rezerv 700 iken `mp = 790` → Carving seçilir (790 - 90 = 700); `mp = 789` → Carving **seçilmez** (yalnızca R); aynı `mp = 789` ve kaçan hedefte Scream (`mp >= 300`) ve leg cutting (`mp >= 84`) seçilebilir; `mp = 250` → Scream seçilemez, leg cutting seçilebilir; AC-WAR-05 birim düzeyi.
   - `WarSkill_Prick_OnHighMiss`: 10 denemede 3 ıska ve ≥ 5 örnek → prick; 2 ıska → Carving.
   - `WarState_Pressure_Pursue_Hysteresis`: mesafe 2 sn artar → `Pursue`; 0,5 sn içinde menzile dönerse `Pursue` yok.
   - `WarChase_Limits_Disengage`: 60 m kat edildi veya 10 sn menzile girilemedi → `Disengage` + `RuleDisengage`; hedef `inForbidden` → anında `Disengage`.
   - `WarBuffs_Outrage_Restoration_Rules`: savaş dışı Outrage; HP %95'te restoration yok, %65'te var; 60 sn içinde yeniden yok.
   - `War_NoTeamFeatures_Deterministic`: `Peel` hiç üretilmez; aynı girdi + tohum aynı `Intent`; `Rng` çağrılmadığı doğrulanır (çekirdek sayacı).
4. Proje satırlarını ekle; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; K2: `Debug` rc=0
- [ ] K3: `./tools/run-tests.sh Release|Debug` `0 failed`; on dört yeni test adı `[ OK ]`; mevcut testler değişmeden geçer
- [ ] K4: `WarriorPressure.h`'te `windows.h|stdafx|GameServer|shared/` yok; `new|malloc|rand(`, global/static değişken yok; ASCII + CRLF
- [ ] K5: R için menzil ölçütü `CheckAttack` ile **aynı** (`DistanceField <= weaponRangeField`); Type1 yalnızca `typeGateWaitMs[1] == 0` iken; tick başına en çok bir `cast`
- [ ] K6: MP rezervi kuralı (§3.1) testle sabit: rezerve dokunan skill yok, rezervi harcayan skill'ler (leg cutting/Scream/sprint) `mp >= maliyet` ile seçilir
- [ ] K7: KI-021 önlemi: leg cutting 10 sn içinde yeniden atılmaz (test)
- [ ] K8: düşman MP/cooldown/envanteri okunmaz; yalnızca `SelfState`, `UnitView`, `NavView`, `SelfTimers`, `SkillSpec`
- [ ] K9: `git diff --stat gece/2026-10-02...bot/F6-03` yalnızca §4'teki dosyalar; `GameServer/`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K10 (Claude): `docs/06` §3 `P-WAR-MELEE-RANGE`, §3/§6.2 MP rezervi, §6.1 Howling "ayakta" notu ve `docs/05` §5 Scream/Stone of Warrior satırını (F4-45: Scream `UseItem 379063000` Scream Scroll, **tüketilmez**; Stone of Warrior Exceed Break'te) düzeltir; `P-WAR-STANDOFF-M`, `P-WAR-NAV-HANDOFF-M`, `P-WAR-SPRINT-MIN-DIST` `[A]` satırlarını ekler
- [ ] K11 (çalışma zamanı, bu planda yok): T-WAR-01 (hareketsiz hedefe tempo: MET-ACT-01 ≥ %85, MET-ACT-02 ≤ %1), T-WAR-02 (kaçan hedef), T-WAR-03 (engelli arazide erişim) **F6-06**'da Claude koşar; AC-WAR-01/02/03/05 bu planda kapanmaz

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "War(Approach|Attack|Skill|State|Chase|Buffs)_|War_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F6-03
grep -nE "windows\.h|stdafx|GameServer|shared/" BotCore/WarriorPressure.h
git diff --check gece/2026-10-02...bot/F6-03
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3; mekanik ve guard kuralları **değişmez**. **Bota avantaj yok:** hedefin MP'si/cooldown'u kullanılmaz; "hedefte leg cutting var mı" yalnızca **kendi** atış sonucundan (`OnCastResult`) izlenir (F4-53 gözlenen durum tablosu bağlanınca doğrulama kaynağı eklenir, bu planda yok).
- Bu planın `Intent`'i yürütülmeden "çalışıyor" sayılmaz (`docs/17` §4: oyun içi kanıt): birim testleri `GELIŞTIRILDI` kanıtı değildir.
- Çelişkiler/belirsizlikler (yazım turunda Claude çözer; proje sahibine tek tek sorulacaklar işaretli):
  1. **Melee menzil:** `docs/06` §3 `P-WAR-MELEE-RANGE = min(skill menzili, 15 + silah.Range) - P-SK-RANGE-MARGIN`; sunucunun R mesafe kontrolü `15 + silah menzili` (`docs/03` MEC-R-05) iken **bot guard'ı** istemcinin bildirdiği `distance <= silah.Range` ölçütünü uygular (`BotCombat.h` `CheckAttack`; Raptor ⇒ 2,0 m, F4-45). Etkin sınır guard'dır; formül buna göre yeniden tanımlanır. (Birim ayrımı `weapon->m_sRange` = 0,1 m `[A]`: F4-45 raporuna dayanır, bu yazım turunda yeniden ölçülmedi.)
  2. **MP rezervi:** `docs/06` §3 "bu rezervin altında yalnızca Carving ve R" ↔ §6.2 son satır "Carving yalnızca MP ≥ 90 + rezerv ise" (rezervin altında hiç Carving yok) ↔ §6.2 "Scream: MP ≥ 300 + rezerv" (rezerv Scream için ayrılmış olduğundan çift sayım). Bu plan rezervi **korunan** MP olarak okur (§3.1). **Proje sahibine sorulacak.**
  3. **Howling "ayakta":** `docs/06` §6.1 "önce durma hareketi" ↔ KI-017 (`UseStanding` 51 ⇒ bot guard'ı ayakta şartı uygulamaz). Menzilde `Hold` zaten durmuştur; ek davranış yok.
  4. **Takip sınırı:** `docs/06` `P-WAR-CHASE-MAX-DIST` 35 m (takım merkezinden) ↔ `docs/10` `P-SOLO-CHASE-MAX` 60 m / 10 sn: solo'da `docs/10` kullanılır; takım merkezi F7.
  5. **`far()` ve sprint eşiği** `docs/06` §6.3'te sayısız: `warSprintMinDist` 15 m `[A]`; `docs/10` §6 "≤ 25 m'de kullan" yalnızca kiter mage uyumudur.

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
