# F6-04: Pot kararı: HP/MP pot kuralı, gelen hasar EWMA'sı, ortak 2,5 sn zamanlayıcı ve stok politikası (`BotCore/PotPolicy.h`)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F6 — Sınıf davranışları, hayatta kalma ve solo (`docs/17` §2; kapılar G6a/G6b) |
| Branch | `bot/F6-04 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F6-01** `KAPANDI`; F4-04 (`UsePotion`, CLI-06 ortak 2,5 sn, `kPotCooldownMs`) `KAPANDI`; F4-17 (`SelfState.hpPotStock/mpPotStock/potWaitMs`) `KAPANDI`; F4-40 (envanter doldurma `db/004`, `tools/bot-refill.sh`) `KAPANDI`. F6-03/F6-05 ile paralel yazılabilir (yalnızca `Intent.pot` yuvasını ve `PotContext`'i paylaşır) |
| İlgili gereksinim / kabul | `docs/11` §2 (K-5/ADR-0009), §3 (pot politikası), §6 (stok STK-01..05); T-POT-01/02/03, AC-SUR-03 (MET-POT-01 ≥ %80), AC-SUR-04 (envanterde olmayan pot 0; ortak 2,5 sn; sunucu 2,0 sn recast ihlali 0); CLI-06, CLI-11; MET-POT-01..04 |
| Tahmini büyüklük | S–M (4 dosya: 1 yeni başlık, 1 yeni test dosyası, 2 proje satırı) |
| Hazırlayan / tarih | Claude / 2026-10-03 (TASLAK; F6 yazım turu) |

---

## 1. Amaç

Botun HP ve MP potunu **ne zaman, hangi kademeden** içeceğinin saf mantığı (`docs/11` §3): tahmini HP (`hp_pred`) ve eksik, acil eşik, israf önleme (`P-POT-*-DEFICIT-MIN`), HP/MP öncelik çatışması, ortak 2,5 sn zamanlayıcı ve stok/tasarruf kuralları. Çıktı `Intent.pot` (F6-01). Sunucuya giden aksiyon mevcut `ActionExecutor::BeginPotion` ile (F6-06 bağlar).

## 1a. Neden TASLAK (HAZIR yapmak için)

Ön koşullar:

1. **F6-01 `KAPANDI`** (`PotView`, `BrainParams` pot satırları, `Intent.pot`).
2. **Pot değeri kaynağı:** `PotView.value` (720/1440 HP; 1920/2160 MP) sunucu bağlamasında (F6-06) `ITEM.Effect1 -> MAGIC -> MAGIC_TYPE3` zincirinden **bir kez** hesaplanıp değer kopyası olarak verilir; zincir ve alan adları bu yazım turunda yeniden doğrulanmalı (F4-04 çalışma zamanı ölçümü: 720 HP / 1920 MP `[V]`; 1440 HP Water of bless ve 2160 MP Ancient Spirit için kimlik ve değer **doğrulanamadı**, `docs/11` §2 yalnızca adı geçirir).
3. `heal_soon` girdisi (beklenen priest heal'i ≤ 1,5 sn) F7 rezervasyonlarına bağlıdır: bu planda **parametre** olarak alınır (`PotContext.healSoon`, solo/F6'da hep `false`); F7 doldurur.
4. Ortak/ayrı HP-MP zamanlayıcı kanıtı (T-MECH-POT-03) gelirse politika değişir (`docs/11` §3.1: "yeni karar gerektirir").

Yazım turunda yeniden doğrulanacak referanslar: `BotCore/BotCombat.h:600-640` (`kPotCooldownMs = 2500`, `PotionCheck`, `CheckPotion`), `BotCore/Perception.h:1115-1140` (`SelfState`: `hpPotStock`, `mpPotStock`, `potWaitMs`), `GameServer/Bot/BotManager.cpp:2500-2550` (`FillSelfExtras`: çanta taraması, `ActionExecutor::PotKindOf`), `GameServer/Bot/ActionExecutor.h:293-309` (`PotKindOf`, `BeginPotion`, `TickPotion`), `docs/11` §3.2-§3.4 formülleri, `tools/bot-refill.sh` ve `db/004` stok değerleri.

## 2. Bağlam (okunması zorunlu)

- `docs/11` §1 (doğrulanmış temel: tüketilmeyen 720 HP `389014000` (MAGIC 490014), 1920 MP `389020000`/`389082000` (490020/490082); ikisi de seviye şartsız), §2 (K-5: tüketilmeyen potlar bot ve insan için aynı; bota özel avantaj yok), §3.1 (**uygulanan:** HP ve MP için **tek ortak 2,5 sn**, pot yalnızca çantada ≥ 1 adet iken), §3.2 (HP formülü), §3.3 (MP formülü), §3.4 (öncelik), §3.5 (pot-skill ilişkisi: tip kapısını tüketmez; priest cast sırasında pot içmez), §5 (ölüm sonrası MP rezervi), §6 STK-01..05.
- `docs/03` CLI-06 (muhafazakâr ortak 2,5 sn `[A]`: T-MECH-POT-03), CLI-11; MB-01 (tüketilmeyen potlar, `UseItem 0`).
- `docs/16` MET-POT-01 (etkili kazanım / nominal pot değeri ≥ %80), MET-POT-02/03/04, `POTION` olayı (`item`, `eksik miktar`, `beklenen/etkili kazanım`, `cooldown durumu`).
- `docs/06` §4 madde 2 ("Kritik pot": HP eksiği ve tehdit `docs/11` kritik bandaysa; aksiyon kilidini engellemez) ve `docs/07` §12 (priest için MP önceliği HP'den yüksek, HP < %35 ise HP).
- `BotCore/BotCombat.h`: `CheckPotion` guard sırası: stok yok → cooldown → rate (bu plan guard'ı **tekrarlamaz**; karar katmanı önkontrol yapar, guard yine reddedebilir ve `FAIRNESS_REJECT` yazar).

## 3. Kapsam

**Yapılacaklar** (`BotCore/PotPolicy.h`; yalnızca `BotCore/*.h`; global/static değişken yok; dinamik bellek yok)

1. **Tipler:** `struct PotView` (F6-01 iskeleti): `itemId`, `kind` (1 HP, 2 MP), `value` (nominal), `stock`, `consumed` (tüketilen potta sayaç düşer; MB-01 pot'ta `false`); `struct PotContext { bool healSoon; float mpNeedSoon; bool casting; bool allyEmergency; }`; `struct PotChoice { bool drink; uint8_t kind; uint32_t itemId; ReasonCode reason; bool override_; float hpPred; float deficit; bool outOfPots; }`.
2. **Gelen hasar EWMA'sı** `class IncomingEwma`: `Reset()`, `Update(uint64_t nowMs, int32_t hp, int32_t knownHealSinceLast)`, `float RatePerSec() const`. `tau = 1500 ms` (`docs/11` §4.2 "incoming_rate_ewma(1,5 sn)"); `dt` 20–2000 ms'ye kırpılır; hasar = `max(0, lastHp + knownHeal - hp)`; HP artışı (pot/regen) hasar sayılmaz, oran sönümlenir. `knownHealSinceLast` botun **kendi** pot etkisi (`PotMemory` bildirir).
3. **HP kuralı** (`docs/11` §3.2, harfi harfine):
   `hp_pred = hp - ewma * 1,0 sn`; `deficit = maxHp - hp_pred`; `ratio = hp_pred / maxHp`; **kademe**: stoğu olan HP potları arasında `deficit >= value * dMin` sağlayan **en büyük** `value` (`dMin` = party `potHpDeficitMin` 0,9; **solo** `potHpDeficitMinSolo` 0,7; tasarruf modunda tüketilen kademe için `+0,1`); `drink_hp = potReady && (anyTier && (ratio < potHpEmerg || (bestTier && !healSoon)))`. Acil durumda (`ratio < potHpEmerg`) uygun kademe yoksa (`deficit` en küçük kademenin `dMin` payından küçük) **en küçük stoklu kademe** seçilir `[A]` (belgede yok; küçük `maxHp`'li mage için gerekli). `override_ = ratio < potHpEmerg` (acil kural, `docs/06` §4 madde 2).
4. **MP kuralı** (`docs/11` §3.3): `drink_mp = potReady && (mp < ctx.mpNeedSoon || bestMpTier) && !drink_hp`; `bestMpTier` = `(maxMp - mp) >= value * potMpDeficitMin` sağlayan en büyük stoklu MP kademesi (1920 için 1824: T-POT-02). `mpNeedSoon` rol modülünden gelir (sonraki 3 sn'de planlanan skill MP'si + rol rezervi; warrior 700, priest 1100, mage 500: `BrainParams`). MP potu hiçbir zaman HP potuyla **aynı tick**'te seçilmez.
5. **Öncelik (§3.4) ve ortak zamanlayıcı:** tek ortak 2,5 sn zamanlayıcı nedeniyle tick başına **en çok bir** pot: `ratio < potHpEmerg` → HP; priest ve `mp < 960` ve `allyEmergency` → MP (`docs/11` §3.4 ikinci satır); aksi halde **eksik / pot değeri** oranı büyük olan; `docs/11` §3.4 "diğeri ≥ 0,5 sn sonra (grup cooldown'ları ayrıysa)" ifadesi uygulanmaz (ortak sayaç, "Çelişkiler" 1). `potWaitMs > 0` ise `drink = false` (guard reddi beklenmez: önkontrol).
6. **Pot ve cast** (§3.5; `docs/03` Ek A debuff kapıları: `SelfState.buffs` içinde `BuffType 153` (NO_POTIONS, `isBuff == false`) ⇒ **HP potu seçilmez**, `BuffType 152` (SILENCE) ⇒ **pot ve skill seçilmez**; `docs/07` §15): `ctx.casting && role == Priest*` iken pot **içilmez** `[A]`; warrior/mage cast sırasında içebilir (pot tip kapısını tüketmez; CLI-11 ≤ 6/sn korunur).
7. **Stok** (`PotMemory`): ilk görülen stok `startStock`; `consumed` kademe için `stock * 4 < startStock` ⇒ **tasarruf modu** (STK-02: `dMin += 0,1`; `potHpEmerg` değişmez); tüketilen kademe stoğu 0 ⇒ o kademe seçilmez, tüketilmeyen kademeye düşülür (STK-03); hiç stoklu pot yoksa `outOfPots = true` (F6-05 `RECOVER` oturma/priest-heal fallback'ini kullanır: STK-03 "HP için priest/sitting, MP için sitting yalnızca 60 m içinde düşman yoksa"). Pot **ikmali yoktur** (STK-04, `Ronark'ta pot satıcısı yok`).
8. `PotMemory::OnPotResult(bool effected, int32_t hpBefore, int32_t hpAfter, int32_t mpBefore, int32_t mpAfter, uint64_t nowMs)`: etkili kazanım (`min(value, eksik)`) ve MET-POT-01 payı için iç sayaçlar (`NominalSum`, `EffectiveSum`); telemetriyi yazmaz (F6-06).
9. Birim testleri (§5.3).

**Kapsam dışı (yapılmayacak)**

- Heal beklentisi/rezervasyon (`heal_soon` üretimi): F7. Sit-down ile yenilenme kararı ve `RECOVER` davranışı: F6-05.
- `ActionExecutor`/guard değişikliği, çanta tarayıcı, pot değeri hesabı, telemetri (`POTION`) yazımı: F6-06.
- Ayrı HP/MP zamanlayıcısı varsayımı (T-MECH-POT-03 insan ölçümü bekliyor).
- Pot satın alma/ikmal yolculuğu (STK-04: F11).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/PotPolicy.h` | yeni | §3 |
| `Tests/BotCoreTests/PotPolicyTests.cpp` | yeni | §5.3 |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca bir `ClInclude` satırı |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca bir `ClCompile` satırı |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F6-04 gece/2026-10-02`; F6-01 birleşmiş mi doğrula; `Durum` → `UYGULANIYOR`.
2. `PotPolicy.h` yaz (§3). `exp()` gerekiyorsa `std::exp` (`<cmath>`); küçük dt için yaklaşık yok. Pot değerleri **kodda sabit değildir**: yalnızca `PotView.value`.
3. `PotPolicyTests.cpp` (adlar **sabit**; sentetik zaman; `PotView`: HP 1440 (tüketilen), HP 720, MP 1920, MP 2160 (tüketilen)):
   - `Pot_Ewma_IncomingRate`: sabit 500 HP/sn hasar → ~1,5 sn sonra oran %63'e yakınsar; HP artışı (pot) hasar sayılmaz; `dt` kırpma sınırları.
   - `Pot_Hp_EmergencyBelow35`: `hp_pred/max < 0,35` ve pot hazır → HP potu, `override_ = true`; 0,36 ve eksik küçük → pot yok.
   - `Pot_Hp_DeficitRule_Solo07`: solo `dMin 0,7`: 720 potu için eksik ≥ 504'te içilir, 503'te içilmez; party `0,9` ⇒ 648 / 647.
   - `Pot_Hp_TierChoice_1440_vs_720`: eksik 1400 ⇒ 1440 potu (solo: `1440*0,7 = 1008 <= 1400`); eksik 900 ⇒ 720; yalnızca 720 stoklu ⇒ 720.
   - `Pot_Hp_NoWaste_SmallDeficit`: eksik 300 (en küçük kademenin payından küçük) ve ratio ≥ 0,35 ⇒ pot yok (MET-POT-01 savunması).
   - `Pot_Hp_Emergency_SmallestTierFallback`: `maxHp 1541`, ratio 0,30, uygun kademe yok ⇒ en küçük stoklu kademe.
   - `Pot_Mp_Rule_1824Deficit` (T-POT-02): eksik 1824 ⇒ MP potu; 1823 ve `mp >= mpNeedSoon` ⇒ içilmez; `mp < mpNeedSoon` ⇒ içilir (eksik küçük olsa da).
   - `Pot_Hp_And_Mp_Same_Time_Priority` (T-POT-03): ikisi gerekli ⇒ HP önce; HP ratio ≥ acil ve MP oranı eksik/pot daha büyük ⇒ MP; priest ve `mp < 960` ve `allyEmergency` ⇒ MP; aynı tick'te iki pot **asla**.
   - `Pot_SharedTimer_2500`: `potWaitMs > 0` ⇒ `drink == false`; 0 ⇒ karar; HP içtikten 2499 ms sonra MP potu da **yok** (ortak sayaç), 2500 ms'de var.
   - `Pot_NoStock_NoPot_OutOfPots` (AC-SUR-04): stok 0 ⇒ `drink == false`; hiç stoklu pot yoksa `outOfPots == true`; tüketilmeyen 720 stoğu 1 iken pot atılabilir (K-5).
   - `Pot_SavingMode_STK02`: tüketilen 1440 kademesinde `stock*4 < startStock` ⇒ `dMin` +0,1; tüketilmeyen kademe etkilenmez; stok 0 ⇒ kademe düşer (STK-03).
   - `Pot_Priest_NoPotWhileCasting`: priest ve `casting` ⇒ pot yok; warrior ve `casting` ⇒ pot kararı normal.
   - `Pot_Efficiency_Sim_80pct` (AC-SUR-03 birim düzeyi): 120 sn sentetik savaş (değişken hasar, 2,5 sn zamanlayıcı) ⇒ `EffectiveSum / NominalSum >= 0,80`.
   - `Pot_Silence_NoPotions_Debuffs`: `BuffType 153` debuff ⇒ HP potu yok (MP potu serbest); `152` ⇒ hiç pot yok; aynı `BuffType` bir **buff** ise etkisiz.
   - `Pot_Deterministic`: aynı girdi dizisi ⇒ aynı `PotChoice` dizisi; `Rng` kullanılmaz.
4. Proje satırlarını ekle; derleme ve test (§7); `Durum` → `UYGULANDI`; Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; K2: `Debug` rc=0
- [ ] K3: `./tools/run-tests.sh Release|Debug` `0 failed`; on beş yeni test adı `[ OK ]`; mevcut testler değişmeden geçer
- [ ] K4: `PotPolicy.h`'te `windows.h|stdafx|GameServer|shared/` yok; `new|malloc|rand(`, global/static değişken yok; ASCII + CRLF
- [ ] K5: kural sabitleri `docs/11` §3.2/§3.3 ile birebir (0,9/0,7/0,95/0,35; 1824; tau 1,5 sn); pot kademe değerleri kodda **yok**
- [ ] K6: tick başına en çok bir pot; ortak 2,5 sn (`kPotCooldownMs`) ihlali 0 (test)
- [ ] K7: stok yokken pot kararı 0 (AC-SUR-04)
- [ ] K8: MET-POT-01 birim simülasyonu ≥ %80
- [ ] K9: `git diff --stat gece/2026-10-02...bot/F6-04` yalnızca §4'teki dosyalar; `GameServer/`, `shared/`, `docs/`, `tools/` farkı 0; `git diff --check` boş
- [ ] K10 (Claude): `docs/11` §3.4 ("ayrı grup cooldown'ları") ve §3.2 acil kademe fallback'ini (belgede yok) docs'a işler; solo/party `dMin` ayrımını `docs/11` tablosuna açık yazar
- [ ] K11 (çalışma zamanı, bu planda yok): T-POT-01 (sunucu 2,0 sn / bot 2,5 sn), T-POT-02 (MP eksik 1824), T-POT-03 (HP+MP birlikte) F6-06 koşusunda (`POTION` olayı, MET-POT-01..04) Claude'da; insan testleri T-MECH-POT-03/04/05 ayrı (proje sahibi)

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Pot_|tests,"
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F6-04
grep -nE "windows\.h|stdafx|GameServer|shared/" BotCore/PotPolicy.h
git diff --check gece/2026-10-02...bot/F6-04
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. K-5/ADR-0009: tüketilmeyen potlar olduğu gibi kalır; kural insan ve bot için aynıdır. **Bota avantaj yok:** pot yalnızca **kendi çantasında** bulunuyorsa; guard (`CheckPotion`) bunu ayrıca zorlar.
- Eşikler `[Ö]`; L1 aralıkları `docs/14` §4.1 (%60–%110).
- Çelişkiler/belirsizlikler:
  1. `docs/11` §3.4 "diğeri ≥ 0,5 sn sonra (grup cooldown'ları ayrıysa)" ↔ §3.1 uygulanan **tek ortak 2,5 sn**: bu plan ortak sayacı uygular.
  2. `docs/11` §3.2 "best_tier" tanımı acil durumda (`ratio < EMERG`) hiçbir kademenin payı sağlanmazsa ne yapılacağını söylemez: bu plan en küçük stoklu kademeyi seçer `[A]`.
  3. `P-POT-HP-EMERG` 0,35 ile `P-SUR-RETREAT-HP` 0,30 (F6-05): pot geri çekilmeden **önce** devreye girer; sıra `docs/06` §4 ile uyumlu (1: geri çekilme, 2: kritik pot, ikisi aynı tick'te olabilir: pot ayrı slot, MEC-POT-02).

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
