# F4-35: `ActionExecutor` warp dilimi — Gate (`WarpType` 1, `Moral` 1) ve descent (`WarpType` 25, `Moral` 4)

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-35` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-34 (summon istisnası: `BeginCast` Type8 bloğu, `wantedTarget`) — `KAPANDI` (merge `1ac6c55`); F4-28 (`Moral` 1 `self` kuralı) — `KAPANDI`; F4-31 (`Moral` 4 party hedefli) — `KAPANDI`; F4-08 (party kurulumu `/bot pinvite`, `/bot paccept`; yalnızca çalışma zamanı sınaması için) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-11, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-18, MEC-MAG-21, MEC-MAG-22 (bu planla eklendi, `[D]`), MEC-T8-04 (bu planla eklendi), SK-06, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 Ek 1 madde 1(d), Ek 11; `docs/05` (110015 Gate, 106650 descent), `docs/06` §peel, `docs/08` §8 (geri çekilme), T-MECH-T8-02 (Gate'in Ronark'ta çalışıp çalışmadığı), T-IGT-MAG-01 altyapısı |
| Tahmini büyüklük | S (4 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.cpp`, `ActionExecutor.h`; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

İki davranış beceri eksiği botla atılamıyor ve F6/F7 bunlarsız yazılamaz:

- **Gate** `110015` (El Morad `210015`; `MAGIC.Moral` 1 = `MORAL_SELF`, `MAGIC_TYPE8.WarpType` 1): mage'in kendini **diriliş/bind noktasına** ışınladığı skill. `docs/08` §8 geri çekilme alt durumu ve `docs/05` satır 161 (T-MECH-T8-02: "Gate Ronark'ta çalışıyor mu?") buna bağlı. Kod okuması soruyu cevaplar (§2): Ronark'ta (`ZONE_RONARK_LAND = 71`) yalnızca **Escape** (`109035/110035/209035/210035`) engellidir, **Gate engelli değildir** `[D]`. Çalışma zamanında ölçülür.
- **descent** `106650` (El Morad `206650`; `Moral` 4 = `MORAL_PARTY`, `WarpType` 25): W-G'nin "peel" aracı (`docs/05` satır 80, `docs/06` §peel: tehdit altındaki priest/mage'e git): çağıran, **party üyesi hedefin konumuna** ışınlanır (hedef `Radius 30` m içindeyse).

`BeginCast` ikisini de `CastTypesSupported(8, 0) == false` yüzünden `unsupported_skill` ile reddediyor (`ActionExecutor.cpp:760-770`). F4-34'ün summon istisnası yalnızca `WarpType 12`'yi açıyor; bu plan aynı kalıbı **iki warp ailesine daha** genişletir. Plan sunucu davranışını kodla belgeler ve çalışma zamanında doğrular (`docs/03` MEC-MAG-22).

Bu plan:

1. `BotCore/BotCombat.h`'a `CastWarpSupported` ve `CastWarpNeedsOtherTarget` saf mantık fonksiyonlarını ekler;
2. `BeginCast`'in F4-34 summon bloğunu **tek `m_Magictype8Array` okumasıyla** üç bayrağa (`summon`, `warp`, `warpOther`) genişletir, destek koşulunu ve `wantedTarget` satırını buna uydurur;
3. sunucu davranışını belgeler ve çalışma zamanında doğrular.

Kapalı kalanlar (ve neden): **Escape** `110035` (`Moral` 6, hedef `-1` grup yolu; Ronark'ta sunucu zaten reddeder, MP'yi düşürüp başarısız olur), **Blink** `110774` (`SkillLevel 80`: bot ağaçları ≤ 70, `docs/08` CHR-08: standart profilde yok), **Wild advent** `108770` (rogue, `Moral` 7; rogue kapsam dışı, ADR-0018 Ek 7), canavar çağırma `21`, zone'lar arası summon `13`, `WarpType 2/3/5`.

F4'ün otuz beşinci planıdır (ADR-0018 sırası: ... summon ✔ → **Type8 warp: Gate + descent (bu plan, dilim 6d)** → `UseItem`'li skill'ler (6e) → CLI-12 → envanter doldurma → T-MECH-SKILL botla koşusu).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-35)" (bu planla birlikte yazıldı), "Ek (F4-34)". `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (Ek 1 madde 1, Ek 10, Ek 11).
- `docs/03` §4.2 **MEC-MAG-21** (summon), **MEC-MAG-22** (bu planla eklendi); §5.5 MEC-T8-01..04; `docs/05` satır 80 (descent), 161 (Gate); `docs/06` §peel; `docs/08` §8 ve satır 173.
- `plans/F4-34-aksiyon-yurutucu-summon.md` (aynı istisna kalıbı; **yazılı planı değil, kodu esas al**).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `1ac6c55` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.cpp:2234-2458` `ExecuteType8()`: tek hedefli dal `:2260-2266` (`GetUserPtr(sTargetID)`; `nullptr` ⇒ `false`, yayın yok); `WarpType != 11` iken ölü hedef ya da `!canTeleport()` ⇒ `goto packet_send` (`:2278-2284`); `m_bWarp` ⇒ `goto packet_send` (`:2290`).
    - **`case 1` (Gate) `:2295-2329`:** `ZoneID > ZONE_BIFROST` **ve** `nSkillID` `109035/110035/209035/210035` ise `SendSkillFailed` + `false` (`:2297-2305`); Gate `110015/210015` bu listede **yok** ⇒ engellenmez. Ardından hedefe bölge yayını (`sData[1] = 1`, `:2307-2309`; Gate'te hedef = çağıran ⇒ çağırana ilk EFFECTING yankısı), `GetMap()->GetObjectEvent(m_sBind)` bind noktası varsa oraya `Warp` (`:2314-2317`); yoksa `ZoneID <= ELMORAD`, `isWarZone()` ya da `canAttackOtherNation()` ise `m_StartPositionArray.GetData(zone)` ile ulusun başlangıcına (`:2318-2323`; kayıt yoksa `return false`); aksi halde `m_fInitX/Z`. Ronark: `ZF_ATTACK_OTHER_NATION` (`Unit.cpp:1100-1104`) ⇒ üçüncü yol. Sonra `break`, `bResult = 1` (`:2450`) ve `packet_send` (`:2452-2455`) çağırana **ikinci** EFFECTING yayını.
    - **`case 20` (Blink) `:2387-2407`** `m_oldx/m_oldz` yönüne ışınlar; kapalı kalır.
    - **`case 25` (descent / Wild advent) `:2431-2447`:** hedef başka zone'da ya da (`bMoral < MORAL_ENEMY` ve çağıran hedefe düşman) ya da (`iNum > 500000` ve zone > `ZONE_MORADON`) ise **`return false`** (yayın yok, MP çoktan düşmüştür); aksi halde çağıran oyuncuysa ve `GetDistanceSqrt(hedef) <= sRadius` ise çağıran `Warp(hedefX*10, hedefZ*10)` ile **hedefin konumuna** ışınlanır; değilse `SendSkillFailed()` (çağırana `MAGIC_FAIL`, `sData[3] = SKILLMAGIC_FAIL_NOEFFECT`). `case`'den sonra `bResult = 1` ve `packet_send` her iki durumda da çağırana EFFECTING yayınlar: **mesafe `Radius`'tan büyükse çağırana önce `MAGIC_FAIL`, hemen ardından `sData[1] = 1` EFFECTING gelir** `[D]` (bot yankısı son yazılan olan EFFECTING'i görür, §5.4).
  - `GameServer/MagicInstance.cpp:813-818` `IsAvailable()` `MORAL_SELF`: oyuncu çağıran hedefi kendisi değilse `MAGIC_FAIL` (Gate'te hedef = çağıranın kimliği). `:831-847` `MORAL_PARTY` (descent, summon ile aynı): hedef party'de değilse CASTING'te `MAGIC_FAIL`; çağıran party'de değilse hedef kendisinden başkası olamaz. `:956-963` skill ağacı: `modulator = sSkill % 10`; **`modulator != 0` ise** `sSkill / 10` çağıranın sınıfı olmalı ve `m_bstrSkill[modulator] >= SkillLevel` olmalı (`IsAvailable()` hem CASTING hem EFFECTING'te çalışır, `:284-286`); `modulator == 0` ise yalnızca `SkillLevel <= seviye`.
  - `GameServer/MagicInstance.cpp:352-358` `CheckSkillPrerequisites()` EFFECTING/FLYING: hedef varsa `sRange > 0 && sUseStanding == 0 && GetDistanceSqrt(hedef) >= sRange` (metre) ⇒ fail; çağıran `isInSafetyArea()` ve `nSkillID < 400000` ⇒ fail. `Unit.cpp:1311-1345` `CUser::isInSafetyArea()`: **zone 71 için `case` yoktur ⇒ her zaman `false`** `[D]` (Gate'in vardığı yer Ronark'ta güvenli bölge sayılmaz, cast engellenmez). `:361-371`: skill başına yeniden-kullanım `ReCastTime * 100` ms.
  - `GameServer/MagicInstance.cpp:389-405`: tip kapısı yalnızca `bType[0]` 1..7; **Type8 kapıya girmez**. `:1028-1029`: `bType[0] != 4` iken `Msp` EFFECTING'te `IsAvailable()` içinde **bir kez, `ExecuteSkill`'den önce** düşer: Gate/descent başarısız olsa da MP gider `[D]`.
  - `GameServer/CharacterMovementHandler.cpp:626-654` `CUser::Warp(x, z)`: `m_bWarp` ise **sessizce döner**; `IsValidPosition` başarısızsa **sessizce döner** (ışınlanma yok ama yayın zaten gitmiştir); başarıda `m_LastX/Z` güncellenir, `WIZ_WARP` çağırana (bot alıcısına) gider, `UserInOut(OUT)`, bölge güncellemesi, `UserInOut(WARP)`, `UserInOutForMe`/`NpcInOutForMe`. `m_bWarp` kurmaz.
  - `GameServer/Bot/BotSession.cpp:72-86`: çağırana gelen `WIZ_MAGIC_PROCESS` yanıtı `m_castEcho`'ya (op, `sData[3]`, skill) **son yazılan kazanır** şeklinde yazılır; `OnPacket()` **değişmez**. `m_castEchoVictims++` (`op == EFFECTING && victimId != -1`) Gate'te iki kez artar ama `victims` yalnızca alan skill'inin EFFECTING'inde rapora girer (`TickCast`, F4-29) ⇒ etkisiz. `grep -n WIZ_WARP GameServer/Bot` boş: ışınlanan botun `Perception` tabloları bu plan kapsamında değişmez (§8 (d)).
  - `shared/database/structs.h:164-172` `_MAGIC_TYPE8 { uint32 iNum; uint8 bTarget; uint16 sRadius; uint8 bWarpType; uint16 sExpRecover; uint16 sKickDistance; }`; `g_pMain->m_Magictype8Array.GetData(id)`. `tools/check-perception-contract.py` R1/R2 listelerinde değildir (oyun verisi tablosu).
  - `GameServer/Bot/ActionExecutor.cpp` (`1ac6c55`): `BeginCast` `resurrection` bloğu `:736-745`, summon bloğu **`:749-758`** (`bool summon = false;` `:749`, `m_Magictype8Array` `:752`, `CastSummonSupported` `:754`), destek koşulu `:760-770` (`if (!resurrection && !summon` `:760`, `unsupported_skill` `:769`), `bad_target` kuralı **`:783-790`** (`wantedSelf` `:784`, `wantedTarget` `:785`). `TickCast` (`:815-`): `area = SendsAimPoint(m->bMoral)` (`Moral` 1 ve 4 ⇒ `false`: tek hedefli paket), kendine cast `meters = 0` (`target.isSelf`), tip kapısı Type8'i kapsamaz (`IsGatedType(8) == false`), tip damgaları `ty < 8`; **değişmez**.
  - `BotCore/BotCombat.h:136-141` `CastRecastMs`, `:143-149` `CastInRange` (`skillRange > 0` ise `distanceM < skillRange`), `:381-384` `CastHpCostSupported`, `:398-408` (`CastResurrectionSupported`, `CastNeedsOtherTarget`), `:410-425` (`kMoralPartyMember`, `kType8WarpSummon`, `CastSummonSupported`), `:427-` `CastTargetIdField`/`CastCoordField` (kendine tek hedefli cast koordinat alanına `0` gönderir); `Tests/BotCoreTests/CombatTests.cpp` son test `Combat_SummonCast_Guard` (`:1500-1552`); `Combat_CastTypes_Supported` içinde `CastTypesSupported(8, 0) == false` (`:1031`, `:1314`, `:1518`) **değişmez**. Toplam birim test **111**.
- **Veri notu (Claude yerel `MAGIC`/`MAGIC_TYPE8` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** `MagicNum < 300000`, `Type1 = 8`:
  - **`WarpType 1` Gate (bu planın hedefi):** `110015` ve `210015` (`Skill 1100`/`2100`, `SkillLevel 15`, `Moral 1`, `Msp 30`, `CastTime 15`, `ReCastTime 100` (10 sn), `Range 56`, `Etc 0`, `UseStanding 0`, `FlyingEffect 0`, `HP 0`, `UseItem 0`, `Type2 0`; `MAGIC_TYPE8.Radius 10000`, `Target 1`). Aynı değerlerle `109015`/`209015` novice mage (sınıf 109/209) ⇒ botlarda `bad_skill`. Priest sınıfı için `112700`/`212700` (`Skill 1127`/`2127`, `SkillLevel 0`, `Moral 1`, `Msp 20`, `CastTime 15`, `ReCastTime 74`, `Range 56`, `Etc 0`; `sSkill % 10 = 7`, `SkillLevel 0` ⇒ ağaç denetimi her zaman geçer) ve `111700`/`211700` (`Skill 1117`/`2117`: sınıf 111/211, botlarda yok ⇒ `bad_skill`): bu plan **veri güdümlüdür** (`Moral 1` + `WarpType 1`), yani priest botları da `112700` ile Gate atabilir; docs/05 yalnızca `110015`'i listeler.
  - **`WarpType 25` descent (bu planın ikinci hedefi):** `105650`/`106650` ve `205650`/`206650` (`Skill 1056`/`1066`/`2056`/`2066` ⇒ sınıf 105/106/205/206, `SkillLevel 50`, `Moral 4`, `Msp 50`, `CastTime 0`, `ReCastTime 91` (9,1 sn), `Range 225`, `Etc 0`, `UseStanding 0`, `HP 0`; `MAGIC_TYPE8.Radius 30`). Botlar `106650`/`206650` kullanır (sınıf 106/206). **Menzil kuralları iki farklıdır:** sunucunun `Range 225` denetimi (metre; pratikte sınırsız) ve `case 25`'in `Radius 30` m koşulu (çağıran–hedef ≤ 30 m, aksi `MAGIC_FAIL` + yine de EFFECTING).
  - **Açık kalmayacaklar:** `WarpType 1` Escape `109035`/`110035`/`209035`/`210035` (`Moral 6`, `Msp 400`, `ReCastTime 250`; `Moral 6` ⇒ `CastWarpSupported` `false`); `WarpType 20` Blink `110774`/`210774` (`Moral 1`, `SkillLevel 80`, `Etc 517`; `WarpType 20` ⇒ `false`); `WarpType 25` Wild advent `108770`/`208770` (`Moral 7`; `Moral 7` ⇒ `false`).
- **Bot karakter notu (`db/002_bot_characters.sql:124-137`).** Skill ağacı baytları (`m_bstrSkill[0..9]`, indeks 6 = descent ağacı): `BotWG_K`/`BotWG_E` `0x00000000003C3E001400` ⇒ `[5] = 60`, **`[6] = 62`** ≥ 50 ✔; `BotWP_K`/`BotWP_E` `0x00000000004600341400` ⇒ **`[6] = 0`** ⇒ `106650` CASTING'te `srv_fail` (skill ağacı, KI-016 benzeri; hata değil). `BotMF_*`/`BotMI_*` (sınıf 110/210, seviye 80): `110015` `sSkill % 10 == 0` ⇒ yalnızca `SkillLevel 15 <= 80`. `BotPHD_*`/`BotPHB_*` (sınıf 112/212): `112700` `SkillLevel 0`. Her botun `Hp = Mp = 32000`.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `kMoralSelf`, `kType8WarpGate`, `kType8WarpDescent` sabitleri; `CastWarpSupported(type0, type1, moral, useItem, warpType)` ve `CastWarpNeedsOtherTarget(warpType)` (§5.1).
2. **`BeginCast` (`ActionExecutor.cpp`):** F4-34 summon bloğunu tek `m_Magictype8Array` okumasıyla `summon`/`warp`/`warpOther` bayraklarına genişlet; destek koşulu `!resurrection && !summon && !warp`; `wantedTarget = CastNeedsOtherTarget(m->bMoral) || summon || warpOther` (§5.3).
3. **Yorumlar (`ActionExecutor.h`):** `BeginCast` açıklaması Gate ve descent'i kapsar (§5.3 d).
4. **Birim testleri:** yeni `Combat_WarpCast_Guard` (111 → 112).
5. **Sonuç sözleşmesi (§5.4):** `docs/03` MEC-MAG-22 çalışma zamanında sınanır.

**Kapsam dışı (yapılmayacak)**

- Escape (`Moral 6`), Blink (`WarpType 20`), Wild advent, canavar çağırma (`21`), zone'lar arası summon (`13`), `WarpType 2/3/5/11`.
- **Karar katmanı:** ne zaman Gate atılacağı (geri çekilme), descent'in kime/ne zaman (peel), hedefin mesafesinin `Radius 30` içinde olup olmadığı, party üyeliği, Gate sonrası ne yapılacağı (`docs/06` §peel, `docs/08` §8, F6/F7). Bota **`Radius` mesafe önkontrolü eklenmez**: descent'te mesafe `Radius 30`'dan büyükse sunucu yine EFFECTING yayınlar ve MP düşer (bilinen sınır, §8 (a)); karar katmanı hedef konumunu `Perception`'dan bilir (AC-LRN-03).
- Gate/descent sonrası botun hareket planının (`m_moveActive`) sıfırlanması, `Perception` tablolarının `WIZ_WARP` sonrası yenilenmesi (§8 (d)): ölçüm notu olarak kalır, kod değişikliği yok.
- Yeni komut/ini anahtarı/telemetri olayı türü/alan, `BotSession.*`/`BotManager.cpp` değişikliği, `tools/` betik değişikliği (özellikle `check-perception-contract.py` istisna listeleri).
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | yeni sabitler + `CastWarpSupported` + `CastWarpNeedsOtherTarget` (mevcut fonksiyonlar değişmez; yalnızca ekleme) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | yeni `Combat_WarpCast_Guard` dosyanın sonuna (111 → 112); mevcut testler değişmez |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yalnızca `BeginCast`: Type8 bloğu (summon bloğunun yerine), `if (!resurrection && !summon && !warp` ve `wantedTarget` satırı |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca `BeginCast` yorumu |

`BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `ScenarioRunner.*`, `ScriptRunner.*`, `tools/` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`CastSummonSupported`'tan (`:421-425`) hemen sonra, `CastTargetIdField` öncesine şu bloğu ekle (ASCII, CRLF, tab, `namespace BotCore` içinde; yeni `#include` yok; **`std::min`/`std::max` kullanma**). Önce `grep -n "kMoralSelf\|kType8WarpGate\|kType8WarpDescent\|CastWarpSupported\|CastWarpNeedsOtherTarget" BotCore` ile adların boş olduğunu doğrula (çakışırsa durup raporla):

```cpp
	// --- warp casts: Gate and descent (ADR-0017 Ek F4-35, docs/03 MEC-MAG-22) ---

	// MAGIC.Type1 = 8, MAGIC_TYPE8.WarpType 1 with MAGIC.Moral 1 (MORAL_SELF) = Gate (110015/210015): the caster is warped to
	// its resurrection/start point. WarpType 25 with Moral 4 (MORAL_PARTY) = descent (106650/206650): the caster is warped to
	// a party member within MAGIC_TYPE8.Radius metres, so a descent needs a named target other than the caster. The other
	// warp types (1 with Moral 6 = Escape, 12 = summon (F4-34), 13 cross-zone summon, 20 Blink, 21 monster summon,
	// 25 with Moral 7 = Wild advent) stay closed here.
	constexpr uint8_t kMoralSelf = 1;
	constexpr uint8_t kType8WarpGate = 1;
	constexpr uint8_t kType8WarpDescent = 25;

	// The warps the bot casts: Type8 alone, no item; Gate = Moral 1 + WarpType 1, descent = Moral 4 + WarpType 25. The caller
	// still rejects flying effects and "sacrifice" HP costs.
	inline bool CastWarpSupported(uint8_t type0, uint8_t type1, uint8_t moral, uint32_t useItem, uint8_t warpType)
	{
		if (type0 != 8 || type1 != 0 || useItem != 0)
			return false;

		return (warpType == kType8WarpGate && moral == kMoralSelf)
			|| (warpType == kType8WarpDescent && moral == kMoralPartyMember);
	}

	// A descent warps the caster to ANOTHER party member; naming itself would waste the MP. Gate is a self cast.
	inline bool CastWarpNeedsOtherTarget(uint8_t warpType)
	{
		return warpType == kType8WarpDescent;
	}
```

Dosyanın kalanı **değişmez** (`CastTypesSupported(8, 0)` `false` kalır, `CastSummonSupported`/`CastNeedsOtherTarget` aynen, `IsGatedType(8)` `false`). `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

Yeni **`Combat_WarpCast_Guard`** (dosyanın sonuna, `Combat_SummonCast_Guard`'dan sonra; Gate `110015`: `Msp 30`, `CastTime 15`, `ReCastTime 100`, `Range 56`; descent `106650`: `Msp 50`, `CastTime 0`, `ReCastTime 91`, `Range 225`, `Radius 30`):

- Destek kapısı: `CastWarpSupported(8, 0, 1, 0, 1) == true` (Gate), `CastWarpSupported(8, 0, 4, 0, 25) == true` (descent); `false` olanlar (her biri tek fark): `(5, 0, 1, 0, 1)`, `(3, 0, 1, 0, 1)`, `(8, 4, 1, 0, 1)`, `(8, 0, 1, 379006000, 1)`, `(8, 0, 4, 379006000, 25)`, `(8, 0, 2, 0, 1)`, `(8, 0, 6, 0, 1)` (Escape), `(8, 0, 4, 0, 1)`, `(8, 0, 1, 0, 25)`, `(8, 0, 7, 0, 25)` (Wild advent), `(8, 0, 1, 0, 20)` (Blink), `(8, 0, 4, 0, 20)`, `(8, 0, 4, 0, 12)` (summon başka fonksiyondan geçer), `(8, 0, 1, 0, 12)`, `(8, 0, 4, 0, 13)`, `(8, 0, 4, 0, 21)`, `(8, 0, 1, 0, 0)`.
- F4-34 ile ayrık: `CastSummonSupported(8, 0, 4, 0, 12) == true` ve `CastSummonSupported(8, 0, 4, 0, 25) == false`, `CastSummonSupported(8, 0, 1, 0, 1) == false`.
- Hedef kuralı: `CastWarpNeedsOtherTarget(25) == true`; `false` olanlar: `(1)`, `(12)`, `(20)`, `(0)`.
- Mevcut fonksiyonlar değişmez: `CastTypesSupported(8, 0) == false`, `CastMoralSupported(1) == true`, `CastMoralSupported(4) == true`, `CastNeedsOtherTarget(1) == false`, `CastNeedsOtherTarget(4) == false`, `IsGatedType(8) == false`.
- Paket alanları (tek hedefli yol): `SendsAimPoint(1) == false`, `SendsAimPoint(4) == false`; Gate (kendine): `CastTargetIdField(SendsAimPoint(1), 2986) == 2986`, `CastCoordField(SendsAimPoint(1), true, 123.4f) == 0`; descent (hedefli): `CastCoordField(SendsAimPoint(4), false, 123.4f) == 123`.
- Başlangıç guard'ı, Gate (`CastStartCheck c = {}`; `c.distanceM = 0.0f`, `c.skillRange = 56`, `c.msp = 30`, `c.reCastMs = BotCore::CastRecastMs(100)`, `c.typeGated = false`, `c.mana = 30`, `c.standing = true`, `c.needsStanding = false`, `c.actionsInWindow = 0`): `CastRecastMs(100) == 10000`; `CheckCastStart(c) == CAST_OK`; `c.mana = 29` ⇒ `CAST_REJECT_NO_MANA`; `c.mana = 30; c.hasSkillLast = true; c.sinceSkillLastMs = 9999` ⇒ `CAST_REJECT_RECAST`; `c.sinceSkillLastMs = 10000` ⇒ `CAST_OK`.
- Başlangıç guard'ı, descent (yeni `c = {}`; `c.distanceM = 20.0f`, `c.skillRange = 225`, `c.msp = 50`, `c.reCastMs = BotCore::CastRecastMs(91)`, `c.typeGated = false`, `c.mana = 50`, `c.standing = true`, `c.needsStanding = false`, `c.actionsInWindow = 0`): `CastRecastMs(91) == 9100`; `CheckCastStart(c) == CAST_OK`; `c.distanceM = 224.0f` ⇒ `CAST_OK`; `c.distanceM = 225.0f` ⇒ `CAST_REJECT_OUT_OF_RANGE` (sunucunun `Range` denetimi; `Radius 30` bot guard'ında yoktur); `c.distanceM = 20.0f; c.mana = 49` ⇒ `CAST_REJECT_NO_MANA`.
- Enum sabit adları `BotCombat.h`'dekiyle birebir (`CAST_OK`, `CAST_REJECT_NO_MANA`, `CAST_REJECT_RECAST`, `CAST_REJECT_OUT_OF_RANGE`); farklıysa dosyadaki adı kullan. Test sayısı **112**.

### 5.3 `ActionExecutor.cpp` ve `ActionExecutor.h`

**a) `BeginCast` Type8 bloğu (`ActionExecutor.cpp:749-758`).** F4-34'ün summon bloğunu (yorumu dahil) şu blokla **değiştir** (tek `m_Magictype8Array` okuması; `t8` yalnızca `plain` doğruyken, yani `nullptr` değilken kullanılır):

```cpp
	// ADR-0017 Ek F4-34/F4-35: a Type8 skill is supported only as a summon (Moral 4, MAGIC_TYPE8.WarpType 12: summon friend),
	// a Gate (Moral 1, WarpType 1) or a descent (Moral 4, WarpType 25), although MAGIC.Type1 = 8 is not in CastTypesSupported.
	// Static game data only: the caller's class/level/quest checks above and below still apply; a summon and a descent
	// need a party member (server rule, answered on the wire as srv_fail).
	bool summon = false;
	bool warp = false;
	bool warpOther = false;
	if (m->bType[0] == 8)
	{
		_MAGIC_TYPE8 * t8 = g_pMain->m_Magictype8Array.GetData(skillId);
		bool plain = t8 != nullptr
			&& m->bFlyingEffect == 0
			&& BotCore::CastHpCostSupported(m->sHP);
		summon = plain
			&& BotCore::CastSummonSupported(m->bType[0], m->bType[1], m->bMoral, m->iUseItem, t8->bWarpType);
		warp = plain
			&& BotCore::CastWarpSupported(m->bType[0], m->bType[1], m->bMoral, m->iUseItem, t8->bWarpType);
		warpOther = warp && BotCore::CastWarpNeedsOtherTarget(t8->bWarpType);
	}
```

ve destek koşulunun ilk satırını `if (!resurrection && !summon` yerine `if (!resurrection && !summon && !warp` yap (parantez/girinti düzenini dosyadaki stile uydur: tab, Allman). Koşulun içindeki altı terim ve `{ out.kind = REFUSED; out.reason = "unsupported_skill"; return out; }` gövdesi **aynen** kalır; `resurrection` bloğu ve `flyingCast` satırı değişmez.

**b) `bad_target` (`:785`).** Yalnızca `wantedTarget` satırı:

```cpp
	bool wantedTarget = BotCore::CastNeedsOtherTarget(m->bMoral) || summon || warpOther;
```

Böylece descent'te `self` (boş hedef adı) `bad_target` ile reddedilir (paket gitmez; sunucuda kendine descent anlamsızdır ama MP düşer); Gate `Moral 1` olduğu için `wantedSelf` zaten hedef adı verilirse `bad_target` verir (`:784`, değişmez). Diğer `Moral` 4 skill'leri (party buff'ları) `self` kabul etmeye devam eder; `wantedSelf` ve `if` satırı aynen kalır.

**c)** `TickCast`, `SubmitCast`, `CancelCast`, `RejectCast`, `OnPacket()` ve diğer her şey **değişmez**: Gate tek hedefli kendine yoldur (`area` `false`, hedef kimliği = çağıranın kimliği, koordinat 0, `meters = 0`), descent tek hedefli party yoludur (`area` `false`, hedef kimliği = üyenin kimliği, koordinat hedefin konumu); tip kapısı Type8'i kapsamaz; MP guard'ı `mana >= Msp` (30 / 50) ister; yeniden-kullanım `CastRecastMs(100)` = 10 000 ms (Gate), `CastRecastMs(91)` = 9 100 ms (descent).

**d) `ActionExecutor.h` yorumu.** `BeginCast` yorumundaki summon maddesinin sonundaki "Gate, Escape, Blink, descent and other warp types stay unsupported" ifadesini şu şekilde değiştir: "Escape, Blink, Wild advent and the other warp types stay unsupported"; summon maddesinden sonra şunu ekle: "Gate (Type8, Moral 1, MAGIC_TYPE8.WarpType 1: the caster is warped to its resurrection/start point; self cast only) and descent (Type8, Moral 4, WarpType 25: the caster is warped to a party member within MAGIC_TYPE8.Radius metres; the target must be a party member other than the caster; ADR-0017 Ek F4-35); a Gate or a descent reports 'effected' when the server broadcasts it, which does not prove the caster moved (docs/03 MEC-MAG-22)". `bad_target` açıklamasını "moral does not match the target kind; corpse-friend, summon and descent need a named target" yap. Yalnızca yorum.

### 5.4 Sonuç sözleşmesi (`docs/03` MEC-MAG-22 ile aynı)

| Skill / durum | Sunucu | Bot sonucu |
|---|---|---|
| **Gate** `110015`, `self`, Ronark (zone 71), canlı | CASTING: `IsAvailable()` `MORAL_SELF` ✔; EFFECTING: MP `Msp` (30) bir kez, `ExecuteType8` `case 1`: hedefe (= çağıran) bölge yayını `sData[1] = 1`, bind noktasına ya da ulusun başlangıcına `Warp` (`START_POSITION` 71: Karus (1380, 1090), El Morad (630, 920)), ardından `packet_send` ikinci EFFECTING | `casting` (`op 1`), sonra `effected` (`op 3`, `code 0`); çağıran `list`'te bind/başlangıç noktasında (hangisi olduğu ölçülür), MP −30 |
| Gate, **ölü** çağıran | bot kuralı (`dead`) / sunucu `isDead()` ⇒ `packet_send` `sData[1] = 0` | `dead` (`CastOutcome::FAILED`) |
| Gate, `Warp` başarısız (`IsValidPosition` false ya da `m_bWarp`) | yayınlar gider, ışınlanma yok | `effected`, çağıran yerinde, MP düşer `[D]`, ölçülmedi |
| Gate, hedef adı verildi | bot kuralı | `REFUSED` `bad_target`, paket gitmez |
| Gate yeniden (`ReCastTime 100`) | bot guard (`CastRecastMs`) | ARMED'de ≥ 10 sn beklenir (reddi `FAIRNESS_REJECT` yazılmaz) |
| **descent** `106650`, hedef **party üyesi**, aynı zone, ≤ 30 m, `BotWG_*` (ağaç 62) | CASTING: `IsAvailable()` `MORAL_PARTY` ✔ + ağaç ✔; EFFECTING: MP `Msp` (50) bir kez, `case 25`: çağıran `Warp(hedefX, hedefZ)`, `packet_send` EFFECTING `sData[1] = 1` | `casting` (`op 1`, `CastTime 0` ⇒ kısa), sonra `effected`; çağıran `list`'te hedefin konumunda (≤ ~1 m), MP −50 |
| descent, hedef **30 m'den uzak** (party üyesi, ≤ `Range 225`) | MP düşer; `case 25` `SendSkillFailed()` (çağırana `MAGIC_FAIL`), ardından `packet_send` EFFECTING `sData[1] = 1` | **`effected`** beklenir (`m_castEcho` son yazılanı tutar: EFFECTING), **çağıran yerinde**, MP −50 `[D]` (`srv_fail` çıkarsa bulgudur, §7 S6) |
| descent, hedef **party üyesi değil** | `IsAvailable()` `MORAL_PARTY` ⇒ `fail_return` | CASTING'te `srv_fail` (`op 4`, `code -100`), MP düşmez |
| descent, `BotWP_*` (ağaç `[6] = 0` < 50) | `IsAvailable()` skill ağacı ⇒ `fail_return` | CASTING'te `srv_fail`, MP düşmez (KI-016 benzeri; hata değil) |
| descent, `self` / ad verilmedi | bot kuralı | `REFUSED` `bad_target`, paket gitmez |
| descent, hedef ölü / `canTeleport()` false | `goto packet_send` `sData[1] = 0` | `effected`, çağıran yerinde, MP düşer `[D]` |
| descent, hedef başka zone / `isHostileTo` | `case 25` `return false` (yayın yok) | `no_result`, MP düşer `[D]`, ölçülmedi |
| CASTING'te `cast <bot> off` | iptal paketi | `cancelled` (`op:4`, `code:-100`), MP düşmez, ışınlanma yok |
| Escape `110035`, Blink `110774`, Wild advent (`108770`) | — | `BeginCast`: Escape ve Blink `unsupported_skill` (sınıf 110 ✔ seviye 80 ✔ geçer, destek kapısı reddeder); Wild advent `bad_skill` (sınıf 108 ≠ 106/110); paket gitmez |
| `109015` (novice sınıfı 109) | — | `bad_skill`, paket gitmez |

`reason` metinleri, `CastOutcome::Kind` değerleri ve `TickCast` akışı bu tablo için **değişmez**; yeni çıktı yoktur.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (değişen dört dosya için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h`'yi içeren `.cpp`'ler de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastTypes_Supported`, `Combat_SummonCast_Guard` ve `Combat_WarpCast_Guard` adlarını `[ OK ]` ile içerir ve toplam test sayısı **112**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-35 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş; `git diff ... -- BotCore/BotCombat.h | grep '^-' | grep -v '^---'` boş (yalnızca ekleme).
- [ ] K5: `grep -c "m_Magictype8Array" GameServer/Bot/ActionExecutor.cpp` tam **1**; `grep -n "CastSummonSupported\|CastWarpSupported\|CastWarpNeedsOtherTarget" GameServer/Bot/ActionExecutor.cpp` tam üç satır (her biri bir kez); `grep -n "CastNeedsOtherTarget(m->bMoral)" GameServer/Bot/ActionExecutor.cpp` tek satır ve satır sonunda `|| summon || warpOther;`; `if (!resurrection && !summon && !warp` ifadesi var; `CastTypesSupported(m->bType[0], m->bType[1])`, `CastTypeMoralSupported(m->bType[0], m->bMoral)`, `CastMoralSupported(m->bMoral)`, `CastHpCostSupported(m->sHP)` (iki yerde: Type8 bloğu ve mevcut `if`) ve `(m->bFlyingEffect != 0 && !flyingCast)` mevcut `if` içinde yerinde; `m->iUseItem != 0` koşulu yerinde (`grep -c "m->iUseItem != 0"` ≥ 2, F4-33'ten bu yana değişmedi).
- [ ] K6: gerilemesiz: `git diff gece/2026-10-02...bot/F4-35 -- GameServer/Bot/ActionExecutor.cpp` yalnızca `BeginCast` içinde hunk içerir (Type8 bloğu + destek koşulu satırı bitişik tek hunk, `wantedTarget` satırı ayrı hunk; en çok iki hunk); `TickCast`/`SubmitCast`/`CancelCast`/`RejectCast` gövdesinde hunk yok; `git diff ... --stat` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `tools/` değişmemiş.
- [ ] K7: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı türü ve alanı yok (`git diff ... | grep '^+' | grep 'Emit('` boş).
- [ ] K8: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); dört dosya CRLF kalır (ASCII); `git diff --check` boş.
- [ ] K9: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 111 testin tamamı hâlâ geçiyor.
- [ ] K10: `python3 tools/check-perception-contract.py` `RESULT: PASS` (`R1`..`R5` ihlal sayıları 0; denetlenen dosya sayısı değişmez; araç ve istisna listeleri değişmez).
- [ ] K11 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S8 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-35
git diff gece/2026-10-02...bot/F4-35 -- BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff gece/2026-10-02...bot/F4-35 -- GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotManager.cpp GameServer/Bot/Telemetry.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj tools
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -rn "CastWarpSupported\|CastWarpNeedsOtherTarget\|CastSummonSupported\|CastTypesSupported" BotCore GameServer Tests
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff --check gece/2026-10-02...bot/F4-35
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; komut başına ≥ 1 sn; dosya saniyede bir okunduğu için "~500 ms" iptal elle yakalanamaz: iptal, `casting` yanıtı görülür görülmez gönderilir, F4-33 bulgusu). Botlar (hepsi zone 71; doğuşlar ≥ 3 sn arayla, KI-DEG-01): Karus mage **`BotMF_K`** (Gate), Karus warrior **`BotWG_K`** (descent atan; ağaç `[6] = 62`), **`BotWP_K`** (çağrılan/hedef party üyesi; ağaç `[6] = 0` ⇒ descent `srv_fail`), **`BotPHD_K`** (priest Gate `112700`, S1'e ek), **`BotPHB_K`** (S8 gerilemesi), El Morad **`BotMF_E`** (S2 Gate; öldürücü rolü bu planda gerekmez), `BotWG_E` (descent El Morad, isteğe bağlı). Önce `list` ile konum, HP/MP ve ölü durumunu denetle: önceki oturumlardan ölü kalan botlar (`BotMF_E`, `BotWP_K`) için `db/002` (idempotent) yeniden uygulanır ya da bot `regene` ile diriltilir; **ölü bot başlangıç ölçümüne alınmaz**. Gate'in hedefi (ulusun başlangıç noktası) botların doğduğu noktaya yakınsa Gate'ten önce botu `move` ile ≥ 60 m uzağa götür; bu mesafe ölçümün kabul koşuludur. MP/HP/konum `list`'ten, olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'tan. MP kesin değeri sunucu yenilemesiyle (kümeli +40, ~4-5 sn'de bir) karışır: MP'yi cast'ten hemen önce ve sonra `list` ile al, yenileme payını raporla. Bu dilimde ölçüm DB'ye ek olarak **konum** yazar (despawn kaydı); taş/envanter değişmez.

1. **S1 Gate (`110015`) ışınlar:** `spawn BotMF_K`; `list` ile konumu (x0, z0) al; (x0, z0) Karus başlangıcına (1380, 1090) ≤ 60 m ise `move` ile uzaklaş ve varışı `list` `pos=` ile teyit et. `cast BotMF_K 110015 self 1`: `CastStart` `ACTION_SUBMIT` **`"target":<BotMF_K'nın kendi kimliği>`**, `casting` `op 1`, `cast_ms` ≈ 1580; `CastEffect` → `effected`, `op 3`, `code 0`, `victims` alanı yok; log `cast finished (effected) after 1 cycle(s), 1 ok, 2 packet(s) sent`. Hemen `list`: konum (x0, z0)'dan farklı ve ya `START_POSITION` 71 Karus (1380, 1090) (±1 m) ya da bir bind noktası: **hangisi olduğunu raporla** (T-MECH-T8-02'nin cevabı: Gate Ronark'ta çalışır mı); MP **−30** (bir kez; yenileme payı ≤ ~40). Bot günlüğünde `WIZ_WARP` kaynaklı hata yok; `BotMF_K` `in_game` kalır ve `move` komutuna yanıt verir (`m_bWarp` takılmadı: `move BotMF_K <x> <z>` ile 3 m yürüt, `arrived`). Gözlem (kabul koşulu değil): ışınlanan botun `see`/`npcs`/`snap` çıktısını ışınlanmadan önce ve sonra al; eski bölgenin kayıtları kalıyor mu rapora yaz (§8 (d)). **Ek:** `spawn BotPHD_K`, aynı kontrol `cast BotPHD_K 112700 self 1` (priest Gate; `effected`, konum değişir, MP −20).
2. **S2 Gate El Morad (`210015`):** `spawn BotMF_E` (ölü değil, ≥ 60 m uzakta), `cast BotMF_E 210015 self 1`: `effected`, konum (630, 920) ±1 m ya da bind; MP −30.
3. **S3 Gate yeniden-kullanım:** `cast BotMF_K 110015 self 2`: iki `effected`, ardışık EFFECTING'ler ≥ 10 sn (`ReCastTime 100`), `cast finished (effected) after 2 cycle(s), 2 ok, 4 packet(s) sent`, MP ≈ −60; ikinci Gate de çalışır (zone 71'de `isInSafetyArea()` `false`, Gate'in vardığı yer cast'i engellemez).
4. **S4 Bot kuralları (Gate ve kapalı kalanlar):** `cast BotMF_K 110015 BotWP_K 1` ⇒ `refused (bad_target)`, paket gitmez; `cast BotMF_K 110035 self 1` (Escape) ve `110774 self 1` (Blink) ⇒ `refused (unsupported_skill)`; `cast BotMF_K 109015 self 1` ⇒ `refused (bad_skill)`; `cast BotWG_K 108770 BotWP_K 1` ⇒ `refused (bad_skill)`; dördünde de paket gitmez, MP değişmez.
5. **S5 descent ışınlar:** `spawn BotWG_K,BotWP_K`; party kur: `pinvite BotWG_K BotWP_K` → `paccept BotWP_K`; `snap BotWG_K` `team` satırıyla üyeliği teyit et; `move BotWP_K <x0+20> <z0>` ile hedefi 20 m uzağa yürüt ve varışı `list` ile teyit et (mesafe 15..25 m). `cast BotWG_K 106650 BotWP_K 1`: `ACTION_SUBMIT` **`"target":<BotWP_K kimliği>`**; `casting` (`op 1`) → `effected` (`op 3`, `code 0`); hemen `list`: `BotWG_K` konumu `BotWP_K` konumunun ≤ ~1 m'sinde; `BotWG_K` MP **−50**; `BotWG_K` `move`'a yanıt verir (`m_bWarp` takılmadı).
6. **S6 descent 30 m'den uzakta:** `BotWP_K`'yı `BotWG_K`'dan 40 m uzağa götür (`list` ile teyit); `cast BotWG_K 106650 BotWP_K 1`: beklenen (kod okuması): `casting` → **`effected`** (`code 0`) **ama `BotWG_K` yerinde**, MP −50. Sonuç farklıysa (ör. `srv_fail`, `no_result`, MP düşmedi) bulgudur: doğrulayıcı nedenini çözümleyip raporlar, `docs/03` MEC-MAG-22 ve §5.4 tablosu buna göre düzeltilir.
7. **S7 descent kuralları:** `cast BotWG_K 106650 self 1` ⇒ `refused (bad_target)`, paket gitmez; `cast BotWG_K 106650 BotMF_K 1` (aynı zone, **party dışı**) ⇒ CASTING'te `srv_fail` (`op 4`, `code -100`), MP değişmez, konum değişmez; `cast BotWP_K 106650 BotWG_K 1` (`BotWP_K` ağaç `[6] = 0`) ⇒ CASTING'te `srv_fail`, MP değişmez; CASTING'te iptal: `casting` görülür görülmez `cast BotWG_K off` ⇒ `cancelled` (`op:4`, `code:-100`), konum değişmez (descent `CastTime 0` olduğundan iptal yakalanamazsa bunu raporla ve Gate ile dene: `cast BotMF_K 110015 self 1` + `casting` görülünce `cast BotMF_K off` ⇒ `cancelled`, MP değişmez).
8. **S8 Gerilemesiz ve kapsam:** F4-34 `cast BotMF_K 110004 BotWP_K 1` (summon friend) `effected`, hedef çağıranın konumuna ışınlandı, `cast BotMF_K 110004 self 1` ⇒ `bad_target`; F4-33 `Moral` 25 (`BotWP_K` öldür, `cast BotPHD_K 112733 BotWP_K 1` ⇒ `effected`, hedef dirildi); F4-32 `cast BotPHD_K 112525 self 1` `effected`, `code 0`; **`Moral` 4 `self` hâlâ serbest:** `cast BotPHB_K 112606 self 1` (Grace) `bad_target` **değil**; F4-28 `cast BotPHB_K 112603 self 1` `effected`, `code 600`; F4-31 grup heal `112557 self`, `target -1`, `victims` ≥ 1; F4-29 Inferno `cast BotMF_K 110545 BotWP_E 1` `target -1`, `victims` ≥ 1; Moral 7 `cast BotMF_K 110518 BotWP_E 1` `target` kurban kimliği. `TELEMETRY=summary`: Gate/descent çalışır, JSONL'de `ACTION_*` yok; `ENABLED=0` ⇒ komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde; sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Temizlik: ini yedekten geri, geçici betikler silindi, botlar despawn, sunucular `stop`.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `BotCore/` ve `Bot/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **`Type1 = 8` + `UseItem 0` + (`Moral` 1 + `WarpType` 1) ya da (`Moral` 4 + `WarpType` 25)** skill'i serisinde davranış değiştirir (önceden `unsupported_skill` idi); `wantedTarget` değişikliği yalnızca descent için `self`'i reddeder, diğer tüm skill'lerde davranış aynıdır. F4-34'ün summon davranışı değişmez (aynı koşul, aynı `bad_target`). Diğer tüm skill'ler için `BeginCast` davranışı (paket biçimi ve telemetri satırı dahil) değişmez.
- **Thread kuralı (ADR-0005):** `TickCast` yalnızca IOCP thread'inde; `BeginCast` `m_Magictype8Array`'i (yalnızca açılışta doldurulan tablo) okur, yeni durum/kilit yok. Sunucuda `Warp` çağıranın IOCP thread'inde çalışır (Gate/descent'te çağıran = bot tick'i); bot alıcısına `WIZ_WARP` ve bölge yayınları bu thread'den gider, `BotSession` kilitleri zaten buna göre kurulu (F4-07 `Regene`, F4-34 summon aynı yolu kullanıyor).
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `effected` yalnızca `MAGIC_EFFECTING`'ten, `srv_fail` yalnızca `MAGIC_FAIL`'den. Hedefin ölü olup olmadığı, party üyeliği, zone, mesafe (`Radius 30`), ışınlanabilirlik **bota önkontrol olarak eklenmez**; bunlar `Perception`/`TeamView` ve karar katmanının (F6/F7) işidir. MP önkontrolü bota ait `user->GetMana()` değeridir (mevcut `CheckCastStart`).
- **Bilinen sınırlar `[A]`/`[D]`:** (a) **`effected` ışınlanmanın gerçekleştiğini kanıtlamaz:** descent'te hedef 30 m'den uzaksa, hedef ölüyse/ışınlanamıyorsa, Gate'te `Warp` başarısızsa sunucu yine EFFECTING yayını gönderir ve **MP yine düşer**; gerçek sonuç çağıranın konumudur (`Perception` öz durumu/`list`). Özellikle **descent mesafesi `Radius 30`'dır** (sunucunun `Range 225` denetimi pratikte sınırsızdır, bot guard'ı yalnızca `Range`'i uygular): karar katmanı descent'i yalnızca hedef ≤ 30 m iken atmalıdır. (b) **Gate iki EFFECTING yankısı üretir** (hedef = çağıran olduğu için bölge yayını + `packet_send`); `m_castEcho` son yazılanı tutar, sonuç değişmez; `m_castEchoVictims` iki kez artar ama `victims` yalnızca alan skill'inde rapora girer. (c) **Gate'in vardığı yer** bind noktası ya da ulusun başlangıcıdır (Ronark'ta `START_POSITION` 71: Karus (1380, 1090), El Morad (630, 920); `GetObjectEvent(m_sBind)` bot satırlarında bulunursa o); bu, arena A'dan Karus için ~259,8 m, El Morad için ~678,1 m uzaktır (`docs/15` §2.4.1, F1-08); Gate yani bir **geri çekilme/kaçış** aracıdır ve geri dönüş yürüyüşü karar katmanının işidir. (d) **Işınlanan botun algısı:** `WIZ_WARP` + `UserInOut`/`UserInOutForMe`/`NpcInOutForMe` sunucudan gelir; `BotSession` `WIZ_WARP`'ı işlemez ve bölge değişimi (`WIZ_REGIONCHANGE`) ışınlanmada gelmeyebilir, bu yüzden gözlem tablolarında eski bölgenin kayıtları kalabilir `[A]` (F4-34 (d) ile aynı). Bu plan Perception'ı değiştirmez; S1 gözlemi sonucu rapora yazılır, kalıcı sapma çıkarsa `docs/KNOWN_ISSUES.md`'ye ayrı kayıt ve ayrı plan Claude'un işidir. (e) Gate sırasında `move` planı sürüyorsa plan yeni konumdan devam eder (`TickMove` `user->GetX/Z()` okur); bu plan hareket planını sıfırlamaz. (f) Escape'in Ronark'ta engelli olması (`:2297-2305`) açılmama gerekçelerinden biridir: açılsa bile `srv_fail` + MP kaybı üretirdi. (g) `MAGIC_TYPE8.Target`, `ExpRecover`, `KickDistance` bu iki ailede kullanılmaz; descent `iNum > 500000` kuralı (Wild advent) botlarda geçersizdir.
- Telemetri hacmi değişmez; `tools/bot-telemetry-report.py` değişmez (`no_result`, `srv_fail` ve `effected` zaten ayrı sayılır, MET-ACT-02; mesafe aşımı descent'i `effected` sayılır, bilinen sınırdır).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-35` — `<kısa-sha> [F4-35] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-35` @ `<sha>`
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ / ✘ | dosya:satır / komut çıktısı |

- Bulgular (önem sırasıyla):
