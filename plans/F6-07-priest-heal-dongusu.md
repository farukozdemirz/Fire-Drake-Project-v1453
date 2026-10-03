# F6-07: Priest heal döngüsü: kendine ve tek müttefike acil/normal heal, tahmini HP, overheal ve cast iptali (`BotCore/PriestHeal.h`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; kapı G6b: priest tek) |
| Branch | `bot/F6-07 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-01, F6-04, F6-05, F6-06** `KAPANDI` (`PolicyL0.h` ve `BrainDriver` bu planda **genişletilir**); F4-03/F4-24 (cast, iptal), F4-18 (`TeamView`: üye HP/MP/mesafe), F4-31 (party hedefli skill: Moral 4/6; bu planda yalnızca Moral 2 tek hedef kullanılır), F4-42/F4-43 (priest skill ölçümü: `docs/05` §9.1-§9.2), F4-46 (usta skill'ler) `KAPANDI`; F5-04/F5-10 (takip ve `NavPickLosCell` advisory), F5-07 (`NavRetreatPlanner`) `KAPANDI`; **F5-55 dilimleri (`plans/F5-55-...md` §1A, 2026-10-03): **F5-59** `NavService` yaşam döngüsü ve harita yükleme (`HAZIR`; `bot/F5-59` dalı açık), **F5-61** kiriş guard'ı, **F5-62** `/bot goto` + waypoint zinciri + `NavPathfinder`/`NavReach`/maliyet katmanı kurulumu, **F5-63** `NavFollower`/takılma/F5-57 sözleşmesi, **F5-64** bütçe + `NAV_*` telemetri, **F5-65** nav durumu temizliği, **F5-66** çalışma zamanı doğrulama (F5-61..F5-66 `TASLAK`; F5-60/F5-67 yalnızca su denetimi/koşullu düzeltme)**. İsteğe bağlı: F4-53/F4-60 (`HealObsRing`/`m_healObs`: başkalarının heal'i; `KAPANDI`) |
| İlgili gereksinim / kabul | `docs/07` §3-§5, §11-§12, §15-§16; T-PRI-01, T-PRI-02, T-PRI-07 (peel isteği hariç), T-SUR-02; **AC-PRI-01** (acil heal kararı p95 ≤ 300 ms; EFFECTING ≤ cast + 400 ms), **AC-PRI-02** (overheal ≤ %25, acil hariç), AC-PRI-09 (kendi bekleyen heal'i), MET-HEAL-01..03/05/06; CLI-03/04/11; G6b (`docs/17` §5) |
| Tahmini büyüklük | M (7 dosya: 1 yeni başlık, 1 yeni test, `PolicyL0.h` ve `BrainDriver.cpp` eklemeleri, 2 proje satırı; **ek** `BotSession.h` gerekirse 8) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 yazım turu) |

---

## 1. Amaç

Priest'in **kendini ve tek bir müttefiki** (party'deki bir üye) hayatta tutan heal döngüsünün saf mantığı ve bağlaması: tahmini HP (`hp_pred`, kendi bekleyen heal'i dahil), acil/normal heal eşikleri, skill seçimi (`docs/07` §5.2 tablosu), overheal ve MP rezervi kuralları, cast iptali (hedef öldü/menzil dışı/hareket), konum (heal menzilinde kalma, düşman melee'sinden uzak durma) ve geri çekilirken kendine heal. Kapsam solo priest + 1 müttefik'tir (G6b); takım koordinasyonu (iki priest, rezervasyon, grup heal) F7'dir.

## 1a. Neden TASLAK (HAZIR yapmak için)

Ön koşullar:

1. **F6-06 `KAPANDI`** (`BrainDriver` ve `PolicyL0` yoksa bu plan bağlanamaz) ve G6a oyun içi kabulü geçmiş olmalı (aynı yürütme hattı doğrulanmış olmalı).
2. **Skill verisi:** nominal heal `SkillHealNominal(SkillSpec.meta, hot)` (F4-53, `BotCore/Perception.h`; nominal heal: Great/Massive 960, Superior 1920, Complete 10000; `docs/05` §6, `docs/04` §4 "heal sabit değerli, stat ile değişmez MEC-CHR-12") `BuildSkills(role)` tarafından **mevcut** `FillSkillMeta` (`GameServer/Bot/BotSession.cpp:8`, F4-60) ile `SkillMeta`'ya kopyalanır (alan adları yazım turunda yeniden doğrulanır; F4-42/F4-43 MP/recast ölçümleri `docs/05` §9.1-§9.2'de, nominal heal ölçülmedi).
3. **Bilinen sınır `[A]` (doğrulanmadı):** `incoming_est` yalnızca **kendi** bildiği heal'i çıkarır (kendi atışları + kendi pot'u); başkasının heal'i/HoT'u (insan müttefik, ikinci priest) HP artışı olarak görünür ve gelen hasarı hafife alır (`docs/07` §5.1 madde 3). F4-53/F4-60 birleştiğinden `HealObsRing` (`BotSession::m_healObs`, `SumNominal(target, nowMs, windowMs)`) başkalarının heal'ini de çıkarmak için **bağlanabilir**: bu plan önce kendi heal'lerini çıkarır, `HealObsRing` entegrasyonu `[A]` olarak aynı planda test edilir (`PriHeal_IncomingEst_NetOfKnownHeals`).
4. İnsan müttefik taş stoğu/diriltme (`docs/07` §10), buff/cure/debuff: F6-09 ve F7.
5. Party kurulumu botlar arasında gerçek paketlerle (`pinvite`/`paccept`) yapılır; F4-42 bulgusu: `pinvite` → `paccept` aralığı ≥ 1500 ms olmalı (CLI-15 `accept_wait`).

Yazım turunda yeniden doğrulanacak referanslar: `BotCore/Perception.h:1454-1500` (`TeamMemberView`: `hp`, `maxHp`, `mp`, `inView`, `dist`, `ageMs`, `name`), `BotCore/BotCombat.h:151-236` (`CheckCastStart`), `:563-583` (`CheckCastCancel`, `kCastCancelCode = -100`), `docs/05` §6 (priest tablosu: Great healing `112527` 80 MP cast 1,5 / recast 2,0; Massive `112536` 160; Superior healing `112545` 320 (+1920); Complete healing `112554` 960 (+10000, recast 5,4); Superior restore `112548` 625 (HoT 2500/30 sn); hepsi `Moral 2`, `Range 56`), `docs/05` §9.1-§9.2 (priest ölçümü: tam canlı hedefe heal `srv_fail -100`; kendi `Moral 2` self atış), `GameServer/Bot/ActionExecutor.cpp:810-830` (`BeginCast`: `Moral 2` hedef adı ile ya da `self`), `GameServer/Bot/BrainDriver.cpp` (F6-06: `BuildSkills`, `Execute`).

## 2. Bağlam (okunması zorunlu)

- `docs/07` §2 (girdiler), §3 (parametreler: `P-PRI-HEAL-EMERG 0,32`, `P-PRI-HEAL-NORMAL 0,75`, `P-PRI-PREHEAL-K 0,8`, `P-PRI-HORIZON = cast + 0,4 sn`, `P-PRI-OVERHEAL-MAX 0,25`, `P-PRI-MP-RESERVE 1100`, `P-PRI-ENEMY-MELEE-MIN 22 m`, `P-PRI-POS-BACK 20-35 m`), §4 (karar öncelikleri 1-10), §5.1 (`hp_pred`, `pending_heals` **eklenir**, `incoming_est` net HP farkından, 2 sn ve 5 sn pencere), §5.2 (tek hedef skill seçimi tablosu), §5.3 (cast 1,5 sn; hedef öldü/menzil dışı/hareket ⇒ iptal; düşman warrior menzilindeyken 3-4 adım geri çekilme), §11 (konum), §12 (MP yönetimi: rezerv; MP önceliği HP'den yüksek, HP < 0,35 ise HP), §13-§15 (durumlar, sözde kod, fallback: "MP tükendi, pot cooldown'da ⇒ kendine en ucuz heal Great healing 80 MP"), §16 (T-PRI-01/02/07, AC-PRI-01/02/09).
- `docs/11` §3.5 (priest cast sırasında pot **içmez**), §4.3 (priest: kendine heal; takipçi uzaktaysa dur-cast), `docs/03` CLI-03 (CASTING → EFFECTING = cast×100 + 70-90 ms; hareket cast'i iptal eder, **önce `MAGIC_FAIL -100` iptal paketi**), CLI-04 (cast döngüsü `CastTime×100 + ~70 + ~140 ms`; Great healing ölçülen en kısa ardışık aralık 3746 ms).
- `docs/16` MET-HEAL-01/02/03 (`heal_saved`)/05 (kaçırılan kritik heal ≤ %5)/06, `HEAL` olayı (`miktar`, `etkili miktar`, `overheal`, `hedef HP önce/sonra`).
- `docs/04` §4 (P-HD/P-HB HP 3491, MP 6392; S1 değerleri), `docs/05` §6.

## 3. Kapsam

**Yapılacaklar** (`BotCore/PriestHeal.h`; yalnızca `BotCore/*.h`; global/static değişken yok; dinamik bellek yok)

1. **Takip edilen birimler:** `struct HealTarget { uint16_t id; bool self; int32_t hp, maxHp; uint32_t ageMs; float dist; bool inView; const char * name; }`; küme = **kendisi + `TeamView.members` (en çok 3 üye; testler ≤ 1)**; ölü (`dead`) ve görünmez üye heal adayı değil (diriltme F7).
2. **HP geçmişi ve gelen hasar** `class HpHistory` (birim başına 12 örnek halkası): `Add(tMs, hp, knownHealNominal)`, `IncomingEst(windowMs)` = `max(0, -dHP_pencere + uygulanmış_bilinen_heal_pencere) / pencere` (`docs/07` §5.1 madde 3; pencereler 2000 ve 5000 ms ayrı); `PartyHpChange` paketi **net HP** taşır: kendi heal'ini çıkarmak için `KnownHealLog` (kendi `effected` heal'leri: `tMs`, `targetId`, `nominal`).
3. **Tahmin:** `hp_pred(u) = clamp(u.hp - preHealK * incoming_est(u) * horizon + pending_heals(u), 0, u.maxHp)`; `horizon = castMs/1000 + 0,4`; `pending_heals(u)` = **yalnızca kendi** devam eden cast'imin nominal heal'i (CASTING gönderildi, EFFECTING henüz yok, hedef aynı, sahibi canlı ve menzilde); iptal/hedef ölümü/menzil dışı/sahip ölümü/tamamlanma ⇒ 0 (`docs/09` §4.3 yaşam döngüsünün tek-sahipli alt kümesi; AC-PRI-09 birim düzeyi: `pending` `hp_pred`'i **artırır**, `maxHp`'yi aşmaz).
4. **Skill seçimi** `HealPick PickHeal(const HealTarget &, hpPred, incoming, const DecisionInput &, PriestMemory &)` (`docs/07` §5.2, ilk eşleşen kazanır):
   - aday kapısı: `deficit = maxHp - hp_pred > 0` **ve** (`ratio < healEmerg` ya da `ratio < healNormal`); **tam canlı hedefe heal atılmaz** (sunucu `-100` verir: F4-42/F4-43 bulgusu; belgede yok, "Çelişkiler" 5).
   - `ratio < healEmerg` ve `deficit >= 2500` ve Complete hazır ve `mp >= 960` ⇒ **Complete healing** (`112554`);
   - `ratio < healEmerg` ve (Complete recast'te veya `deficit < 2500`) ⇒ **Superior healing** (`112545`);
   - `deficit >= 1920 * (1 - overhealMax)` (1440) ⇒ Superior healing;
   - `deficit >= 960 * (1 - overhealMax)` (720) ve Great healing hazır ⇒ **Great healing** (`112527`, en iyi HP/MP);
   - `deficit >= 720` ve Great healing recast'te ⇒ Massive healing (`112536`);
   - `deficit >= 400` ve hedefte HoT yok (**kendi** `restoreUntilMs`'ten) ve `incoming_5s > 0` (sürekli hasar) ⇒ Superior restore (`112548`, HoT 2500/30 sn);
   - aksi ⇒ heal yok (overheal önleme). Kimlikler El Morad için `+100000`.
   - **MP rezervi:** emergency değilken `mp - msp >= priMpReserve` (1100); rezervin altında yalnızca acil heal (ve cure/pot; F6-09/F6-04). MP yetmiyorsa acil durumda en ucuz heal (Great healing 80 MP, `docs/07` §15).
5. **Hedef sıralama:** acil (`ratio < emerg`) aday > normal aday; aynı sınıfta **en düşük `ratio`**; eşitlikte kendisi, sonra küçük `id`. Tick başına **en çok bir** cast (`Intent.cast`); aktif cast varken yenisi başlamaz (`m_castPhase != CAST_IDLE`).
6. **Cast iptali (SK-02):** CASTING iken hedef ölürse/`inView == false`/mesafe `>= range` olursa ya da karar katmanı hareket (geri çekilme) istiyorsa `cast.cancel = true` (yürütücü `CancelCast`: önce `MAGIC_FAIL -100`, sonra hareket; CLI-03). Sunucu MP'yi EFFECTING'de düştüğünden iptal MP harcatmaz.
7. **Konum** `PriestPosition(in, mem)`: (a) heal hedefi menzilde kalsın: `dist >= range - 4 m` ⇒ `FollowTarget` halkası `[0,5 * range, range - 4]`; (b) düşman warrior/rogue `P-PRI-ENEMY-MELEE-MIN` (22 m) içinde ve cast başlamadan önce: `CastWindowSafe` (`dist - hız * 1,5 sn > saldırı menzili + 2` ise önce cast, değilse önce 3-4 adım geri: `docs/07` §5.3); (c) görüş: `NavPickLosCell` **advisory** (`P-NAV-LOS-MODE` varsayılan `advisory`: aksiyonu engellemez); (d) iki priest ≥ 8 m ve arka hat 20-35 m **F7** (bu planda yok).
8. **Geri çekilme ve acil durumlar:** `Survival` (F6-05) `role_adj +0,05` ile geri çekilmeyi **önce** verir; geri çekilirken `allowSelfHeal`: takipçi `CastWindowSafe` ise **dur-cast** (kendine acil heal), değilse hareket sürer (cast, hareketle iptal olacağından başlatılmaz). Priest solo ve melee hedef peşindeyse kaçışta kendine heal zaman kazandırır (`docs/10` §4.3).
9. **MP/pot:** priest cast sırasında pot yok (F6-04 `ctx.casting`); MP potu önceliği HP'den yüksek, HP < %35 ise HP (F6-04 `allyEmergency`).
10. **Orkestrasyon:** `Intent PriestDecide(const DecisionInput &, BrainMemory &)` (öncelik `docs/07` §4: 1 geri çekilme/ölüm önleme, 2 acil heal, 5 normal heal, 9 konum; 3/4/6/7/8/10 F6-09 ve F7); `PolicyL0.h` priest dalı bunu çağırır; `BrainDriver.cpp` priest için `BuildSkills` (Great/Massive/Superior/Complete healing, Superior restore, kimlikler ve El Morad +100000) ve **hedef adı** ile `BeginCast` (müttefik adı `TeamMemberView.name`; kendine `""`), `OnCastResult` ⇒ `KnownHealLog`/`restoreUntilMs`/`HEAL` telemetri (`miktar`, `etkili = min(nominal, eksik)`, `overheal`, hedef HP önce/sonra: PARTY_HPCHANGE gecikmesi nedeniyle **tahmini**, `[A]`).
11. Birim testleri (§5.3).

**Kapsam dışı (yapılmayacak)**

- Buff (AC/HP/direnç), cure, debuff/hedef çağrısı + chat, diriltme, Elysian Web, Judgment/Helis (solo savunma): **F6-09** (solo/self) ve **F7** (takım).
- Grup heal (`112557`/`112560`, `Moral 6` r = 30), iki priest koordinasyonu ve **rezervasyon** (`TeamBlackboard`), birincil healer, `peel isteği` (T-PRI-07'nin W-G'ye çağrısı), heal-stall: **F7**.
- `ActionExecutor`/guard değişikliği, nav bağlama, telemetri altyapısı.
- İnsan müttefik taş stoğu, MP/heal tahmini öğrenme (`docs/14`).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/PriestHeal.h` | yeni | §3 |
| `Tests/BotCoreTests/PriestHealTests.cpp` | yeni | §5.3 |
| `BotCore/PolicyL0.h` | değiştir | yalnızca priest dalı ve `BrainMemory.PriestMemory` |
| `GameServer/Bot/BrainDriver.cpp` | değiştir | priest `BuildSkills`, hedef adı ile cast, `HEAL` telemetri |
| `BotCore/BotCore.vcxproj` | değiştir | bir `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | bir `ClCompile` satırı |

`ActionExecutor.*`, `BotSession.h` (gerekmezse), `Telemetry.*`, `docs/`, `tools/` **değişmez**. Listede olmayan dosya gerekirse **durup** Uygulayıcı Raporu'nda soru yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-07 gece/2026-10-02`; F6-06 birleşmiş ve G6a çalışma zamanı kabulü `docs/STATUS.md`'de kayıtlı mı doğrula; sunucular kapalı; `Durum` → `UYGULANIYOR`.
2. `PriestHeal.h` yaz; `PolicyL0.h`/`BrainDriver.cpp` eklemeleri. Heal değerleri ve MP maliyetleri **sunucu verisinden** (`SkillSpec`) okunur, kodda sabit değildir (yalnızca eşik 2500/720/1440/400 `docs/07` §5.2'den).
3. `PriestHealTests.cpp` (adlar **sabit**; sentetik `DecisionInput` ve `TeamView`):
   - `PriHeal_HpPred_PendingAdds` (AC-PRI-09): kendi bekleyen heal'i `hp_pred`'i artırır, `maxHp`'yi aşmaz; iptal/hedef ölümü/menzil dışı/sahip ölümü/tamamlanma sonrası `pending == 0`.
   - `PriHeal_IncomingEst_NetOfKnownHeals`: kendi heal'i HP artışından çıkarılır; bilinmeyen heal'de gelen hasar hafife alınır (belgelenmiş sınır `[A]`); iki pencere (2/5 sn) ayrı.
   - `PriHeal_SkillChoice_Table`: `docs/07` §5.2 satırlarının her biri için vektör (Complete / Superior (acil) / Superior (≥ 1440) / Great (≥ 720) / Massive (Great recast'te) / Superior restore / yok).
   - `PriHeal_NoHealAtFullHp`: `deficit <= 0` ⇒ heal yok; `ratio >= healNormal` ⇒ normal heal yok.
   - `PriHeal_Emergency_Self_And_Ally`: ikisi acil ⇒ en düşük `ratio`; eşitte kendisi.
   - `PriHeal_Overheal_Limit`: normal heal'de beklenen overheal ≤ %25 (AC-PRI-02 birim düzeyi: 200 sentetik durum, `overheal/nominal <= 0,25` acil hariç).
   - `PriHeal_MpReserve_Gate`: `mp - msp < 1100` ⇒ normal heal yok, acil heal var; MP yetmez + acil ⇒ Great healing.
   - `PriHeal_Cast_Cancel_Conditions`: hedef ölü/`!inView`/mesafe `>= 56`/hareket isteği ⇒ `cancel`; aksi sürer.
   - `PriHeal_NoPotDuringCast`: `casting` ⇒ pot yok (F6-04 sözleşmesi).
   - `PriHeal_Reposition_RangeAndMelee`: `dist >= range - 4` ⇒ `FollowTarget`; melee 22 m içinde ⇒ `CastWindowSafe` ile önce cast/önce adım kararları.
   - `PriHeal_Priority_Order`: geri çekilme > acil heal > normal heal > konum; ölü ⇒ hiçbir şey.
   - `PriHeal_Latency_Budget` (AC-PRI-01 birim düzeyi): `hp_pred` eşiği aştığı tick'te `cast.active == true` (≤ 1 tick = 100 ms).
   - `PriHeal_ElMorad_IdOffset_Deterministic`: Karus/El Morad kimlik ofseti; aynı girdi + tohum ⇒ aynı `Intent`; `Rng` yok.
4. Proje satırlarını ekle; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu.
5. **Çalışma zamanı (Claude, `/plan-dogrula`; G6b):** hazırlık: `BotPHD_K` (brain priest) + `BotWP_K` (hedef/müttefik) party'de (`pinvite` → ≥ 1500 ms → `paccept`), düşman `BotWP_E` betikle sabit hasar verir; `TELEMETRY=decisions`.
   - **T-PRI-01** (sabit hasar alan tek üyeyi hayatta tutma): 10 × 60 sn; müttefik ölüm 0; MET-HEAL-02 ≤ %25, MET-HEAL-05 ≤ %5, MET-HEAL-06 ≤ %10.
   - **T-PRI-02** (ani patlama): rakip burst; AC-PRI-01 (karar ≤ 300 ms, EFFECTING ≤ cast + 400 ms) telemetriden.
   - **T-PRI-07** (priest'e baskı; peel isteği hariç): düşman W-P priest'e saldırır; geri çekilme + kendine heal; priest ölümü ve süre raporlanır.
   - **T-SUR-02** (priest desteğiyle warrior geri çekilmesinin ertelenmesi): `ComputeSupport` etkin iken warrior eşiği −0,08.
   - **İnsan (proje sahibi):** priest'in heal zamanlaması ve kendini koruması form maddesi (ayrı).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; K2: `Debug` rc=0
- [ ] K3: `./tools/run-tests.sh Release|Debug` `0 failed`; on üç yeni test adı `[ OK ]`; mevcut testler (F6-01..F6-06 dahil) değişmeden geçer
- [ ] K4: `PriestHeal.h`'te `windows.h|stdafx|GameServer|shared/` yok; `new|malloc|rand(`, global/static değişken yok; ASCII + CRLF
- [ ] K5: skill seçimi `docs/07` §5.2 ile birebir; tam canlı hedefe heal **hiç** seçilmez (test); heal değerleri/MP kodda sabit değil
- [ ] K6: `BrainDriver.cpp` yalnızca `ActionExecutor` üzerinden cast gönderir (`grep -n HandlePacket` boş); başka oturumun `CUser`'ına erişim yok
- [ ] K7: `git diff --stat gece/2026-10-02...bot/F6-07` yalnızca §4'teki dosyalar; `ActionExecutor.*`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K8 (Claude): `docs/07` §5.2'ye "tam canlı hedefe heal atılmaz" kuralını ve `incoming_est` bilinen-heal sınırını işler; `P-PRI-*` `[A]` etiketleri
- [ ] K9 (**çalışma zamanı, Claude**): T-PRI-01/T-PRI-02/T-PRI-07/T-SUR-02 ölçütleri (§5 madde 5): AC-PRI-01 p95 ≤ 300 ms, AC-PRI-02 overheal ≤ %25, müttefik ölümü 0, MET-HEAL-05 ≤ %5
- [ ] K10 (**insan, proje sahibi**): priest davranış formu; yokluğunda G6b `KABUL_EDILDI` olmaz

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "PriHeal_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F6-07
grep -n "HandlePacket" GameServer/Bot/BrainDriver.cpp
grep -nE "windows\.h|stdafx|GameServer|shared/" BotCore/PriestHeal.h
git diff --check gece/2026-10-02...bot/F6-07
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2-§3. **Bota avantaj yok:** müttefik HP'si yalnızca `PARTY_HPCHANGE` paketinden (`docs/03` MEC-PTY-04; gözlem sözleşmesine uygun); düşman MP/cooldown'u okunmaz; heal menzili `CastInRange` ile guard'da zorlanır.
- Çelişkiler/belirsizlikler:
  1. `docs/17` F6 test listesi T-PRI-01/02/07 ve AC-PRI-01/02'yi içerir; buff/cure/debuff/diriltme (AC-PRI-03..08, T-PRI-03..06/08) G7a/F7'dir. Görev tanımındaki "buff/cure/debuff döngüleri (kendine/tek müttefik)" F6'nın solo kısmıdır: **F6-09**.
  2. T-PRI-07 "peel isteği" (`docs/07` §16) `TeamBlackboard.PeelRequest` ister (F7): bu planda ölçülmez.
  3. `docs/07` §14 sözde kodu `s.party_alive`, `blackboard.filter_unreserved` kullanır (F7); tek priest/tek müttefikte `reserved` filtresi boş kabul edilir.
  4. `docs/07` §5.1 `incoming_est` insan müttefik heal'ini bilemez `[A]`; bu plan kendi heal'lerini çıkarır.
  5. **Tam canlı hedefe heal sunucuda reddedilir** (`112548` ikinci tur `srv_fail -100`, F4-42 §9.1 bulgu 4, F4-43): `docs/07` bunu söylemez; bu plan `deficit > 0` kapısıyla önler.
  6. `docs/07` §3 `P-PRI-HORIZON = cast + 0,4 sn` ve `docs/03` CLI-03 ölçülen cast döngüsü (`CastTime×100 + ~70 + ~140 ms`): ufuk 1,5 sn cast için 1,9 sn kalır, gerçek EFFECTING ~1,58 sn; fark `P-PRI-PREHEAL-K` ile telafi edilir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
- Kabul kriterleri öz-değerlendirme (K9-K10 DeepSeek'e ait değildir):
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
