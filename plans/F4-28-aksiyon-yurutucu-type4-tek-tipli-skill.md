# F4-28: `ActionExecutor` Type4 tek tipli skill dilimi — kendine, dosta ve düşmana buff/debuff (Defense, Strength, Malice...)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-28` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-03 (cast dilimi: `BeginCast`/`TickCast`/`SubmitCast`, tip kapısı, `m_castEcho`) — `KAPANDI`; F4-26 (`CastTypesSupported`, `IsGatedType`, `MinGatedSince`, iki tipli kapı) — `KAPANDI` (merge `06a76e8`); F4-27 (`quest_locked`) — `KAPANDI` (merge `b73d31f`) |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-11, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-15 (bu planla eklendi, `[D]`), AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 dilim 4 |
| Tahmini büyüklük | S (3 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.h` yalnızca yorum; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

`BeginCast` yalnızca `bType[0]` 1 veya 3 olan skill'leri (ve `{3, 4}` çiftini) destekliyor: `BotCore::CastTypesSupported(4, 0)` `false` döndürdüğü için **her Type4 tek tipli skill** (`Type1 = 4`, `Type2 = 0`: buff ve debuff) `unsupported_skill` ile reddediliyor (`ActionExecutor.cpp:730`). Warrior'ın `Defense`/`sprint`'i, mage'in `Mana Shield`'i ve dost direnç buff'ları, priest'in `Strength`/`Resist poison` buff'ları ve **Malice/Confusion/Parasite** debuff'ları bu yüzden botla atılamıyor; F6 (warrior/mage) ve F7 (priest) davranışlarının ön koşulu.

Bu plan **yalnızca `bType = {4, 0}`** skill'lerini, `Moral` 1 (kendine), 2 (kendine ya da dosta) ve 7 (düşmana), `UseItem == 0`, uçmayan (`FlyingEffect == 0`) skill'ler için açar. Gereken kod tek satırlık bir destek kuralı değişikliğidir (`CastTypesSupported`); tip kapısı (F4-26 `IsGatedType`/`MinGatedSince`), reuse damgaları, yankı yorumu (`SubmitCast`) ve quest denetimi (F4-27) Type4'ü zaten doğru işler (§2'de satır satır doğrulandı). Planın asıl değeri **sunucu davranışının belgelenmesi ve çalışma zamanında sınanmasıdır**: Type4'ün MP kuralı, "aynı BuffType zaten hedefte" reddi ve debuff yenileme davranışı bot sonuç kodlarına nasıl yansıyor (§5.4, `docs/03` MEC-MAG-15). F4-26'dan kalan "Type4 tip kapısı çalışma zamanında sınanamadı" boşluğu da bununla kapanır.

Kapsam dışı kalan skill'ler §3'tedir. F4'ün yirmi sekizinci planıdır (ADR-0018 sırası: cast iptali ✔ → uçan ✔ → çift tipli ✔ → **Type4 tek tipli (bu plan)** → alan → ...; F4-27 quest planı sıra dışı araya girmişti).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-28)" (bu planla birlikte yazıldı), "Ek (F4-26)" ve "Ek (F4-03)" madde 2. `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (+ Ek 4).
- `docs/03` §4.2 **MEC-MAG-03/-08/-11/-13/-14/-15**, §13.2 CLI-03, CLI-04, CLI-09, CLI-11. MEC-MAG-15 bu planla eklendi (`[D]`).
- `plans/F4-26-aksiyon-yurutucu-cift-tipli-skill.md` (aynı dosyaların aynı bölgeleri; **yazılı planı değil, kodu esas al**).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `b73d31f` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.cpp:22-29` — `Run()`: `UserCanCast() == SkillUseFail` ise `SendSkillFailed()`. `:284-286` `UserCanCast()` CASTING ve EFFECTING'te `IsAvailable()` çağırır; `:705-718` `SendSkillFailed()` çağırana `MAGIC_FAIL` yollar (`sData[3]` = CASTING'te `−100`, diğer opcode'larda `−103`, `shared/packets.h:395-402`; `bSendFail` varsayılan `true`, `MagicInstance.h:81,93`).
  - `GameServer/MagicInstance.cpp:809-823` — `IsAvailable()` moral denetimi: `MORAL_SELF` (1) hedef çağıranın kendisi olmalı; `MORAL_FRIEND_WITHME` (2) hedef kendisi ya da düşman olmayan; `:857-866` `MORAL_ENEMY` (7) hedef düşman olmalı. `:915-917` Type4 için `CheckType4Prerequisites()`.
  - `GameServer/MagicInstance.cpp:540-583` — `CheckType4Prerequisites()`: hedef bir kullanıcıysa (`0 <= sTargetID < MAX_USER = 3000`, `shared/globals.h:9`; bot kimlikleri 2984–2999) ve skill **buff** ise (`_MAGIC_TYPE4::isBuff()`, `shared/database/structs.h:98`; `CMagicProcess::IsBuff`, `GameServer/MagicProcess.cpp:1017`) hedefin `m_buffMap`'inde aynı `BuffType` zaten varsa `false` döner; `UserCanCast` başarısız olur, `Run()` `SendSkillFailed()` gönderir (**CASTING'te `−100`, CastTime 0 skill'de EFFECTING'te `−103`**; yankı yok sayılırsa bot `srv_fail` görür). Debuff skill'inde bu denetim yoktur.
  - `GameServer/MagicInstance.cpp:999-1029` — `IsAvailable()` EFFECTING: `sMsp > GetMana()` ise reddet (`:1011-1012`); `:1028-1029` MP `bType[0] != 4 || sTargetID == -1` iken burada düşer. Tek hedefli Type4 (`sTargetID != -1`) için **burada düşmez**.
  - `GameServer/MagicInstance.cpp:1618-1896` — `ExecuteType4()`: `:1678-1686` tek hedef yoksa/ölüyse/`isBlinking()` ise `false` (yayın yok); `:1695-1696` `sRange > 0` ve mesafe `>= sRange` ise hedef sessizce atlanır (yayın yok); `:1749-1788` aynı `BuffType` hedefte: **buff** ise `bResult = 0` ve `fail_return`, **debuff** ise eski debuff silinir ve süre yenilenir (`:1756-1760`, başarılı sayılır); `:1765-1767` debuff'ın çağırana uygulanması atlanır; `:1800-1802` **MP burada düşer**: `sTargetID != -1 && bType[0] == 4` iken `GrantType4Buff` başarılı olduktan sonra `MSpChange(-sMsp)` (MEC-MAG-08 istisnası); `:1861-1874` `bType[1] == 0 || bType[1] == 4` iken `{sData[0], bResult, sData[2], süre (sn), sData[4], hız, sData[6]}` ile bölge yayını (çağıran dahil); `:1891-1893` `bResult == 0` ise ek `SendSkillFailed` (EFFECTING'te `−103`).
  - `GameServer/MagicInstance.cpp:109-131` — EFFECTING sonrası `bType[0]` için (`:122`) tip damgası; `:387-423` same-type kapısı `bType[0] == 4`'ü içerir (`PLAYER_SKILL_REQUEST_INTERVAL` 0,7 sn, `User.h:23`; bot 1000 ms ile muhafazakâr).
  - `GameServer/Bot/ActionExecutor.cpp` (satırlar `b73d31f`): `BeginCast` destek kuralı `:729-739` (`CastTypesSupported` tek çağrı, `:730`), moral/hedef kuralı `:751-759` (`MORAL_SELF` ⇒ hedefsiz, `MORAL_ENEMY` ⇒ hedefli, `MORAL_FRIEND_WITHME`/`MORAL_ALL` ikisi de), `quest_locked` `:741-749`; `TickCast` tip kapısı `:849-863` (`IsGatedType` iki tip, `m_castTypeHas/Last[0..7]`, tip 4 dizinin içinde); EFFECTING sonrası damgalar `:997-1009`; `SubmitCast` sonuç eşlemesi `:644-652` (`op == MAGIC_EFFECTING` ⇒ `effected`, `code == −104` ⇒ `missed`, `MAGIC_FAIL` ⇒ `srv_fail`) — **değişmez**; Type4 süresi pozitif olduğundan `missed` üretilmez. `GameServer/Bot/BotSession.cpp:72-83` yankı yalnızca `caster == m_castSelfId` ve opcode 1..4'te yazılır (Type4 yayını çağıranı içerir, `MagicInstance.cpp:1864`).
  - `GameServer/Bot/BotManager.cpp:3187-3215` — cast hedef görünümü (self: kendi kimliği/konumu, adlı hedef: o botun oturumundan); **değişmez**.
  - `BotCore/BotCombat.h:317-327` `CastTypesSupported` (bu planın tek mantık değişikliği); `Tests/BotCoreTests/CombatTests.cpp:1019-1041` `Combat_CastTypes_Supported` (`(4, 0)` satırı `false` bekliyor: **bu planda `true` olur**), `:1043` `Combat_TypeGate_MinSince`, `:1085` `Combat_TypeGate_DualCast`. Toplam birim test **103** (F4-27 sonrası).
- **Veri notu (Claude yerel `MAGIC`/`MAGIC_TYPE4` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** `Type1 = 4`, `Type2 = 0`, `Etc = 0`, `UseItem = 0`, bot sınıflarının (Karus `106`/`110`/`112`, El Morad `206`/`210`/`212`) skill'leri; El Morad karşılıkları `2xxxxx`:
  - **Kendine (`Moral` 1):** `106001` sprint (`Msp 5`, `CastTime 0`, `ReCast 60`, `BuffType 6` hız, süre 10 sn), `106007` **Defense** (`Msp 4`, `CastTime 0`, `ReCast 100`, `BuffType 2` AC, süre 15 sn), `110815` **Mana Shield** (`Msp 150`, `CastTime 0`, `ReCast 0`, seviye 12, `BuffType 31`, süre 40 sn), `110820` Instantly Magic (`BuffType 23`: sunucu bu buff aktifken skill/tip damgası yazmaz, `MagicInstance.cpp:117`; bot muhafazakâr kalır ve her zaman damga yazar), warrior `1067` ağacı `Gain`/`Rise`/`Nimble Wind` (`BuffType 7`, 300 sn), `Outrage`/`Frenzy` (`BuffType 5`, 30 sn), `berserk Echo` `106770` (`UseStanding 51`, KI-017), priest `112529`/`112629`/`112729` (`BuffType 7`, 400 sn, seviye 30).
  - **Kendine ya da dosta (`Moral` 2, `Range 56`, `CastTime 15`, `ReCast 1`):** priest `112004` **Strength** (`Msp 10`, `BuffType 7`, 600 sn), `112006` **Resist poison** (`Msp 10`, `BuffType 8`, 600 sn), `1126` ağacı (Insensibility*, `BuffType 2`, 600 sn, seviye 3–76), mage `110506`/`110524`/`110548` (Resist/Endure/Immunity fire, `BuffType 8`, 300 sn) ve buz/yıldırım karşılıkları (`1106`, `1107`; `110612` Frozen armor `BuffType 2`).
  - **Düşmana (`Moral` 7, `Range 56`, `CastTime 15`):** priest `112703` **Malice** (`Msp 40`, `ReCast 74`, seviye 3, `BuffType 2` AC düşürme, 150 sn), `112715` Confusion (`Msp 80`, `BuffType 7`), `112724` Slow (`BuffType 5`), `112727` Reverse life (`BuffType 1`, süre 1 sn), `112745` Parasite, `112760` Massive (`BuffType 4`, `Msp 180`, `ReCast 104`).
  - Bu planda `unsupported_skill` kalanlar: `UseItem != 0` (Fire/Ice/Lightning Armor `110573`/`110673`/`110773`, Absolute power `110802`, Freezing Distance `110674`...), `FlyingEffect != 0`, `Moral` 4 ve 6 (party; ADR-0018 Ek 1 madde 2: ayrı dilim), 10..13 (alan), 5 (NPC), 14/15 (klan) ve diğer her moral; `Etc != 0` skill'ler `quest_locked` kuralından geçer (F4-27: `Wall of Iron` `106675` ve `Berserker` `106775` `Etc 510`, `Superior Parasite` `112771` `Etc 520`; çalışma zamanı sınaması `db/003` uygulanmadığından bu planın dışındadır).
- **Bot karakter notu (`db/002_bot_characters.sql:124-136`, repodaki betik).** Skill ağacı baytları (`strSkill` indeksi = `MAGIC.Skill % 10`): `BotWP`/`BotWG` warrior ağaçları 70/52/20 ya da 60/62/20 (indeks 5/7/8 ya da 5/6/8); **`1060` ağacı (indeks 0) seviye denetimiyle yetinir** (`modulator == 0`, `MagicInstance.cpp:956-963`), bu yüzden `106001`/`106007` iki warrior profiliyle de atılır. Priest `1120` ağacı da indeks 0 (`112004`/`112006` tüm priest botlarıyla); `1127` (debuff, indeks 7) yalnızca **`BotPHD`** (62), `1126` (Insensibility, indeks 6) yalnızca **`BotPHB`** (62): KI-016 ağaç puanı denetimi bot tarafında yok, yetersiz ağaçta `srv_fail` alınır (bu planın kusuru değil). Mage `1108` (Mana Shield, indeks 8) iki mage profilinde de 20 ≥ 12.
- **`UseStanding` notu (KI-017).** Bu planın sınama skill'lerinde `UseStanding` 0'dır; `berserk Echo` (51) gibi master skill'ler için F4-24/F4-26'daki karar aynen geçerlidir (`needsStanding = (UseStanding == 1)`).

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `CastTypesSupported(type0, type1)` kuralı `(4, 0)` için `true` döndürür; diğer çiftler değişmez. Birim testleri (`CombatTests.cpp`: bir mevcut test güncellenir, bir yeni test eklenir; 103 → 104).
2. **`ActionExecutor.cpp` kod değişikliği yoktur** (§5.3): `BeginCast`'in diğer kuralları (`bFlyingEffect`, `iUseItem`, `sEtc`/`quest_locked`, moral, `bad_target`), `TickCast` tip kapısı, reuse damgaları ve `SubmitCast` Type4 için zaten doğru çalışır. Uygulayıcı bunu **kodu okuyarak doğrular** ve `ActionExecutor.h` yorumlarını günceller.
3. **Sonuç yorumu (davranış sözleşmesi, §5.4):** tek hedefli Type4 sonuçları (`effected` + `code` = süre; aynı `BuffType` zaten hedefte ⇒ `srv_fail`; menzil/ölü hedef ⇒ `no_result`; debuff yenileme ⇒ `effected`) belgelenir ve çalışma zamanında sınanır.

**Kapsam dışı (yapılmayacak)**

- Party hedefli (`Moral` 4, 6), alan (`Moral` 10..13), NPC (5), klan (14/15) ve diğer moral'lerin Type4 skill'leri (ADR-0018 dilim 5 ve Ek 1 madde 2); `UseItem` gerektiren (scroll/pot tabanlı) Type4 skill'ler (dilim 6); uçan Type4 (`FlyingEffect != 0`); `bType[0] == 4` ile başlayan çiftler (`{4, x}`); Type4'ün başka tiple karışık çiftleri (`{1, 4}` melee yavaşlatma, `{2, 4}` okçu: ayrı küçük dilimler).
- **Aynı `BuffType` hedefte zaten varken botun kendiliğinden vazgeçmesi** (önkontrol): `BuffType` kaydı sunucunun `m_buffMap`'indedir; hedef başka botsa bot onu okuyamaz (AC-LRN-03, `tools/check-perception-contract.py` R2 `m_buffMap` yalnızca `FillSelfExtras`'ta serbest). Bu kontrolü karar katmanı (F6/F7) yapar: kendi buff'ını `SelfState.buffs`'tan (F4-17), başkasınınkini gözlem tablosundan (F4-53, TASLAK) okuyarak. Bu planda tekrar atım sunucu reddine (`srv_fail`, MP düşmez) bırakılır ve bilinen sınır olarak yazılır (§8-b).
- Skill ağacı puanı denetimi (KI-016) ve `UseStanding` 51..54 anlamı (KI-017): değişmez.
- Type4 etkisinin bot tarafından yorumlanması (hedefin hızı/AC'si/direnci): bot hedefin buff listesini bu planda okumaz; yalnızca doğrulama için `/bot snap <bot>` ile hedef botun **kendi** öz durumu görülür (F4-17).
- Buff/debuff'ın **süre bitişi, yenileme zamanlaması, birden fazla buff'ın yönetimi**: karar katmanı işidir.
- Yeni komut, ini anahtarı, yeni telemetri olayı türü veya alanı, `list`/`snap` çıktısı değişikliği, `BotManager.cpp`/`BotSession.*` değişikliği, `tools/` betik değişikliği, `ActionExecutor.cpp` kod değişikliği (§5.3).
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | `CastTypesSupported` + yorum (`// --- dual-typed cast ...` başlığı) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | `Combat_CastTypes_Supported` güncellenir, `Combat_TypeGate_Type4Single` eklenir (103 → 104) |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca yorumlar (`BeginCast`/`TickCast` açıklaması, `CastOutcome::reason` notu) |

`ActionExecutor.cpp`, `BotSession.*`, `BotManager.cpp` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse (ör. kodu okuyunca Type4 için bir kuralın eksik çıktığını görürsen) **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`:317-327` bölümünü şu hale getir (başlık yorumu ve fonksiyon; ASCII, CRLF, tab, `namespace BotCore` içinde; yeni `#include` yok; **`std::min`/`std::max` kullanma**):

```cpp
	// --- dual-typed cast (ADR-0017 Ek F4-26) and single Type4 cast (ADR-0017 Ek F4-28) ---

	// MAGIC.Type1/Type2 pairs the bot casts (docs/03 MEC-MAG-13, MEC-MAG-15): a single type 1, 3 or 4, or the pair
	// Type3 + Type4 (the server runs Type3 first and Type4 second on the same target). Every other pair stays unsupported.
	inline bool CastTypesSupported(uint8_t type0, uint8_t type1)
	{
		if (type1 == 0)
			return type0 == 1 || type0 == 3 || type0 == 4;

		return type0 == 3 && type1 == 4;
	}
```

Dosyanın kalanı (`IsGatedType`, `CastQuestAllowed`, `TypeStamp`, `MinGatedSince`) **değişmez**. `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

a) **`Combat_CastTypes_Supported` (mevcut test, güncelle):** `CHECK_EQ(BotCore::CastTypesSupported(4, 0), false);` satırı `true` olur ve **`true` bloğuna** taşınır (`(1,0)`, `(3,0)`, `(4,0)`, `(3,4)`); `false` bloğuna şu satırlar eklenir: `(4, 1)`, `(4, 2)`, `(4, 4)`, `(4, 9)`. Diğer satırlar (`(4, 3)`, `(0, 4)`, `(1, 4)`, `(2, 4)` ... `false`) aynen kalır.

b) **`Combat_TypeGate_Type4Single` (yeni, dosyanın sonuna; mevcut `Combat_TypeGate_DualCast` kalıbıyla):** `CastStartCheck c = {}` (alanlar `Combat_TypeGate_DualCast`'tekiyle aynı, ancak `skillRange = 56`, `msp = 10`, `reCastMs = 100`). Sırasıyla:
   - `IsGatedType(4)` true (`CHECK_EQ((int)BotCore::IsGatedType(4), 1)`).
   - Tek tipli Type4 skill'in damgaları: `TypeStamp st[2] = {{4, true, 300}, {0, false, 0}}`; `c.hasTypeLast = MinGatedSince(st, 2, c.sinceTypeLastMs)` ⇒ `1` ve `300`; `CheckCastStart(c) == CAST_REJECT_TYPE_GATE`, `CastWaitMs(c) == 700`.
   - `st[0].sinceMs = 1000` ⇒ `CheckCastStart(c) == CAST_OK`, `CastWaitMs(c) == 0`.
   - Aynı skill daha önce hiç atılmadıysa (`st[0] = {4, false, 0}`): `MinGatedSince` `0` döner ⇒ `c.hasTypeLast = false` ⇒ `CAST_OK` (başka tipten, ör. Type3, bir damga bu skill'in kapısına girmez; damgalar skill'in **kendi** tiplerinden kurulur, bkz. `ActionExecutor.cpp:853-862`).
   - Çift tipli (`{3, 4}`) bir skill'in EFFECTING'inin yazdığı `m_castTypeLast[4]` damgası, sonraki Type4 tek tipli skill'in kapısına **aynı kaynaktan** girer; bu zincir (Ice comet → Mana Shield) birim testle değil çalışma zamanında sınanır (§7 S3, F4-26'nın bıraktığı boşluk).

   Toplam test sayısı **104**.

### 5.3 `ActionExecutor.cpp` / `ActionExecutor.h`

**a) `ActionExecutor.cpp` değişmez.** Uygulayıcı şunları **okuyarak** doğrular ve sonucu Uygulayıcı Raporu'na yazar (ek kod gerekiyorsa §4'e göre durup sorar):
   1. `BeginCast` `:729-739`: `CastTypesSupported` dışında Type4'ü reddeden koşul yok (`grep -n "bType\[" GameServer/Bot/ActionExecutor.cpp` yalnızca `:729,730,809,851,855,1001,1283,1284`'ü gösterir; `:1283-1284` pot sınıflandırmasıdır ve Type3 içindir).
   2. `bad_target` `:751-759`: `MORAL_SELF` hedefsiz, `MORAL_ENEMY` hedefli ister; `MORAL_FRIEND_WITHME` ikisini de kabul eder.
   3. `TickCast` tip kapısı `:849-863` ve damga döngüsü `:999-1007` `bType[0] == 4` için `m_castTypeHas/Last[4]`'ü (dizi 8 elemanlı) okur/yazar.
   4. `SubmitCast` `:644-652` Type4 yankısını (`op == MAGIC_EFFECTING`, `code` = süre) `effected`, `MAGIC_FAIL`'i `srv_fail` yapar.

**b) `ActionExecutor.h` yorumları (yalnızca `:200-216` bölgesi).** `BeginCast` yorumundaki destek cümlesine eklenir: "single Type4 (buff/debuff; Moral 1, 2, 7; ADR-0017 Ek F4-28)". `TickCast`/`CastOutcome::reason` yorumuna eklenir: "single Type4: the EFFECTING echo carries the duration in 'code' (never 'missed'); a buff whose BuffType is already on the target fails with 'srv_fail' (docs/03 MEC-MAG-15); out-of-range or dead target gives 'no_result'". Başka satır değişmez.

### 5.4 Sonuç sözleşmesi (kod değişikliği yok; `docs/03` MEC-MAG-15 ile aynı)

Tek hedefli Type4 (`sTargetID` bir oyuncu kimliği: self ya da adlı hedef) için sunucu davranışı ve bot sonucu:

| Durum | Sunucu | Bot sonucu |
|---|---|---|
| Buff uygulandı (hedefte aynı `BuffType` yok) | EFFECTING yankısı `sData[1] = 1`, `sData[3]` = süre (sn); MP **bir kez** düşer (`ExecuteType4`) | `effected`, `code` = süre |
| Debuff uygulandı ya da yenilendi (hedefte aynı `BuffType` debuff'ı varsa eskisi silinir, süre sıfırlanır) | aynı yayın; MP bir kez düşer | `effected`, `code` = süre |
| Buff'ın `BuffType`'ı hedefte zaten var | `CheckType4Prerequisites` `false` ⇒ `MAGIC_FAIL` (CASTING'te `−100`, `CastTime 0` skill'de EFFECTING'te `−103`); **MP düşmez**, buff değişmez | CASTING'te `srv_fail` (seri `FAILED`), `CastTime 0`'da EFFECTING `srv_fail` |
| Hedef bölge dışı/ölü/blink | `ExecuteType4` `false` ya da hedef atlanır ⇒ yayın yok | `no_result` (seri `FAILED`) |
| `sRange` aşıldı (bot guard `CastInRange` zaten engeller; yarış durumu) | hedef sessizce atlanır, yayın yok | `no_result` |
| Debuff'ı hedef engelledi/yansıttı (`m_bBlockCurses`/`m_bReflectCurses`) | yankı `sData[1] = 0` + ek `MAGIC_FAIL` (`−103`) | `srv_fail` (son paket) |
| `Moral` 7 skill'i dost hedefe, `Moral` 2 skill'i düşmana (`isHostileTo`) | `IsAvailable` `false` ⇒ `MAGIC_FAIL` | `srv_fail` (bot hedef milletini bilmez; bilinen sınır, F4-03 ile aynı) |

Bu tablodaki hiçbir satır için `SubmitCast`/`TickCast`/`OnPacket()` değişmez.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.h` için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h` bir `.cpp` tarafından içerildiğinden `ActionExecutor.cpp`/`BotManager.cpp` de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastTypes_Supported` ve yeni `Combat_TypeGate_Type4Single` adlarını `[ OK ]` ile içerir ve toplam test sayısı **104**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-28 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş.
- [ ] K5: destek kuralı yalnızca `(4, 0)`'ı ekliyor: `git diff gece/2026-10-02...bot/F4-28 -- BotCore/BotCombat.h` yalnızca `CastTypesSupported` bölgesini (başlık yorumu, fonksiyon yorumu, gövde) değiştirir; birim testi `(4, 1)`, `(4, 2)`, `(4, 3)`, `(4, 4)`, `(4, 9)`, `(1, 4)`, `(2, 4)`, `(0, 4)` için `false`, `(4, 0)` için `true` (K3).
- [ ] K6: `ActionExecutor.cpp` değişmedi: `git diff --stat gece/2026-10-02...bot/F4-28` çıktısında `ActionExecutor.cpp`, `BotSession.*`, `BotManager.cpp` yok; Uygulayıcı Raporu §5.3-a'daki dört okuma doğrulamasını satır numaralarıyla yazar.
- [ ] K7: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı/alanı yok (`git diff ... | grep '^+' | grep 'Emit('` boş).
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F4-28` yalnızca §4'teki 3 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş.
- [ ] K9: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp`/`ActionExecutor.h` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K10: gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 103 testin tamamı hâlâ geçiyor.
- [ ] K11: `python3 tools/check-perception-contract.py` `RESULT: PASS` ve sayılar değişmez (`R1 0/0`, `R2 0/28`, `R3 0/18`, `R4 0/0`, `R5 0/0`); yeni kod sunucu nesnesi okumaz.
- [ ] K12 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S5 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-28
git diff gece/2026-10-02...bot/F4-28 -- BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h
git diff gece/2026-10-02...bot/F4-28 -- GameServer/Bot/ActionExecutor.cpp GameServer/Bot/BotManager.cpp GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "bType\[" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h
git diff --check gece/2026-10-02...bot/F4-28
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`); zamanlama için geçici betik `./Scripts/f428_*.txt` (silinir, commit edilmez). Botlar (hepsi zone 71, aynı başlangıç noktasında): Karus warrior **`BotWP_K`** (`1060` ağacı: Defense, sprint), Karus priest **`BotPHD_K`** (Strength, Resist poison, **Malice**: `1127` indeksi 62) ve **`BotPHB_K`** (`1126` indeksi 62: Insensibility Skin), Karus mage **`BotMF_K`** (Mana Shield, Ice comet), El Morad düşman hedef **`BotWP_E`** (zone 71'de Death knight NPC'leri bot hedeflerini öldürebiliyor, F4-26 notu: hedefi gerekirse yeniden doğur). MP/HP `list` çıktısından, buff'lar `snap <bot>` günlük satırından (`cmd snap:   buff skill=<id> type=<BuffType> buff|debuff remain=<s>s`), olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'dan. **Buff süreleri uzun (15–600 sn): aynı `BuffType`'ı taşıyan buff'ın bitmesini beklemek yerine botlar her senaryo ve alt adım arasında `despawn`/`spawn` ile yenilenir (taze oturumda buff yoktur, `docs/15` §6a).**

1. **S1 Kendine buff, CastTime 0 (Defense, sprint, Mana Shield):** `spawn BotWP_K`; MP `list` ile not; `cast BotWP_K 106007 self 1` (Defense: `Msp 4`, `CastTime 0`). Beklenen JSONL: **tek** `ACTION_SUBMIT` `type:"CastEffect"` (`CastStart` **yok**, `since_casting_ms` 0) → `ACTION_RESULT` `ok:true`, **`reason:"effected"`, `op:3`, `code` = 15 (`MAGIC_TYPE4` Duration)**; log `cast finished (effected) after 1 cycle(s), 1 ok, 1 packet(s) sent`; MP düşüşü = **4 (bir kez)**; `snap BotWP_K` `buff skill=106007 type=2 buff remain≈15s`. Aynı sırayı `cast BotWP_K 106001 self 1` (sprint: `Msp 5`, `code` 10, `type=6`) ve mage ile `spawn BotMF_K`, `cast BotMF_K 110815 self 1` (Mana Shield: `Msp 150`, `code` 40, `type=31`) verir.
2. **S2 Aynı BuffType hedefte zaten varken (reddedilen buff, MP düşmez):** (a) `cast BotWP_K 106007 self 2` (Defense ×2; `ReCastTime 100` = 10 sn, buff 15 sn sürer): 1. döngü `effected` (`code` 15, MP −4); 2. döngü guard `recast` beklemesinden sonra (≈ 10 sn) gider ve beklenen `ACTION_RESULT` `ok:false`, **`reason:"srv_fail"`, `op:4`, `code:-103`** (`CastTime 0` ⇒ `MAGIC_FAIL` EFFECTING aşamasında); log `cast finished (srv_fail) after 2 cycle(s), 1 ok, 2 packet(s) sent`; **MP toplam yalnızca 4 düşmüş** (reddedilen Type4'te MP düşmez, MEC-MAG-15); `snap`'te buff tek kayıt, `remain` yenilenmemiş. (b) **Skill değil `BuffType` çakışır:** `spawn BotPHB_K` ile Defense (AC buff'ı, `BuffType 2`) `BotWP_K` üzerindeyken `cast BotPHB_K 112603 BotWP_K 1` (Insensibility Skin: `Moral` 2, `BuffType 2`, `CastTime 15`): CASTING aşamasında reddedilir: `CastStart` `ok:false`, **`srv_fail`, `op:4`, `code:-100`**, log `cast stopped (srv_fail)`, EFFECTING gönderilmez, priest MP değişmez. (c) Cast süreli buff'ın tekrarı: `cast BotPHD_K 112004 BotWP_K 2` (Strength ×2, `ReCast 1`): 1. döngü `effected` (`code` 600, MP −10), 2. döngünün CASTING'i `srv_fail` (`code:-100`), seri durur, toplam MP −10.
3. **S3 Dosta ve kendine buff, cast süreli ve tip kapısı:** `spawn BotPHD_K,BotWP_K`; `cast BotPHD_K 112004 BotWP_K 1` (Strength: `Moral` 2, `Msp 10`, `CastTime 15`, `Range 56`): `CastStart` (`casting`, `cast_ms` 1580) → `CastEffect` (`since_casting_ms` ≥ 1580, `effected`, `code` 600); log `... 2 packet(s) sent`; priest MP −10; `snap BotWP_K` `buff skill=112004 type=7 buff remain≈600s`. `cast BotPHD_K 112006 self 1` (Resist poison, kendine; `code` 600, `type=8`). **Her alt adımdan önce botları `despawn`/`spawn` ile yenile** (taze oturumda buff yoktur; aksi hâlde önceki buff'lar aynı `BuffType`'ta çakışıp `srv_fail` verir). **Type4 tip kapısı (geçici betik `./Scripts/f428_gate4.txt`):** `0 cast BotPHD_K 112004 self 1` ve `2000 cast BotPHD_K 112006 self 1` (ikinci komut 1. `CastEffect`'ten ≈ 0,3 sn sonra silahlanır): ikinci `CastStart` 1. `CastEffect`'ten **≥ 1000 ms** sonra gider (`type_gate` beklemesi, `FAIRNESS_REJECT` yok; `m_castTypeLast[4]`). **Çift tipliden kalan Type4 damgası (F4-26 boşluğu):** `spawn BotMF_K,BotWP_E`, betik `./Scripts/f428_gate34.txt`: `0 cast BotMF_K 110651 BotWP_E 1` (Ice comet `{3, 4}`, `CastTime 15`) ve `2000 cast BotMF_K 110815 self 1` (Mana Shield, Type4 tek tipli): Mana Shield'in `CastEffect`'i Ice comet'in `CastEffect`'inden **≥ 1000 ms** sonra gider (Type4 damgası çift tipli skill'den kalmıştır).
4. **S4 Düşmana debuff ve yenileme (Malice):** `spawn BotPHD_K,BotWP_E`; `cast BotPHD_K 112703 BotWP_E 1` (Malice: `Moral` 7, `Msp 40`, `CastTime 15`, `Range 56`, `ACPct 75` = debuff): `CastStart` → `CastEffect` (`effected`, `code` 150), MP −40; `snap BotWP_E` `buff skill=112703 type=2 debuff remain≈150s`. **Aynı hedefe ikinci Malice** (`cast BotPHD_K 112703 BotWP_E 2`; `ReCast 74` = 7,4 sn): her iki döngü `effected` (debuff yenilenir, **`srv_fail` değil**), toplam MP −80, ikinci `EFFECTING`'ten sonra `snap`'te `remain` yeniden ≈ 150 (MEC-MAG-15 debuff yenileme). `Confusion` `112715` (`Msp 80`, `BuffType 7`) ve `Parasite` `112745` aynı sırayı verir; farklı sonuç (direnç, hız/stun zarı) çıkarsa raporda ayrıca yaz. Hedef bot `BotWP_E` zone 71'de NPC'lerce öldürülürse yeniden doğur (F4-26 notu).
5. **S5 Reddedilen/desteklenmeyen ve gerilemesiz:** `cast BotMF_K 110573 self 1` (Fire Armor, `UseItem`) ⇒ `unsupported_skill`; Type4 party skill'i (`Moral` 4, id'yi `MAGIC`'ten bul) ⇒ `unsupported_skill`; `cast BotWP_K 106007 BotWP_E 1` (`Moral` 1 + hedef adı) ⇒ `bad_target`; `cast BotPHD_K 112703 self` (`Moral` 7 hedefsiz) ⇒ `bad_target`; `cast BotWP_K 112004 self 1` ⇒ `bad_skill` (sınıf); `cast BotPHD_K 112004 BotWP_K 1` hedef **menzil dışı** (≥ 56 m, `move` ile uzaklaş) ⇒ CASTING gitmeden `FAIRNESS_REJECT` (`out_of_range`, MEC-MAG-11). Gerilemesiz: `cast BotMF_K 110518 BotWP_E 3` (Ignition) önceki gibi `effected` ×3, `CastFly` yok; `cast BotMF_K 110615 BotWP_E 1` (Ice arrow, uçan çift tipli) F4-26 gibi 3 paket, MP ≈ 100; `cast BotMF_K 110515 BotWP_E 1` (Fire ball) F4-25 gibi 3 paket; Strength cast'inin CASTING aşamasında (~500 ms) `cast BotPHD_K off` ⇒ `cancelled` (`op:4`, `code:-100`), MP değişmez, hedefte buff yok; `attack`/`move` (F4-01/F4-02); `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama Type4 cast çalışır (log satırları); `ENABLED=0` ⇒ komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde (≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Bölgedeki ikinci bir bot `snap <bot> events` ile Type4 EFFECTING'i görür (`event ... op=3 skill=112004 caster=<priest> target=<warrior>`, F4-52 halkası). Temizlik: ini yedekten geri, geçici betikler silindi, botlar despawn.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `BotCore/` ve `Bot/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **Type4 tek tipli** skill serisinde davranış değiştirir; Type1/Type3/`{3, 4}` skill'ler ve cast etmeyen botlar için davranış değişmez.
- **Thread kuralı (ADR-0005):** `TickCast` yalnızca IOCP thread'inde. `OnPacket()` değişmez. `ActionExecutor` log yazmaz.
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `effected` yalnızca sunucunun bölgeye yayınladığı, çağırana da gelen `MAGIC_EFFECTING` paketinden, `srv_fail` yalnızca çağırana yollanan `MAGIC_FAIL`'den çıkarılır; sunucu nesnesinden (hedefin `m_buffMap`'i dahil) başarı çıkarılmaz.
- **Bilinen sınırlar `[A]`/`[D]`:** (a) Reddedilen buff (aynı `BuffType` hedefte) CASTING aşamasında düşer (`CastTime > 0`) ya da EFFECTING'de (`CastTime 0`); ikisi de `srv_fail`'dir ve MP harcatmaz. (b) Bot hedefteki buff'ı bilmediği için aynı `BuffType`'ı tekrar atmaktan kendiliğinden vazgeçmez; bu bilinçli bir sınırdır (önkontrol §3'te kapsam dışı): sistem düzeyinde MET-ACT-02 (`srv_fail` ≤ %1) karar katmanının tekrar atımı önlemesine bağlıdır (F6/F7: `SelfState.buffs`, F4-53). Telemetri raporunda bu tür `srv_fail`'ler `code:-103`/`-100` ile ayırt edilir. (c) Gerçek istemcinin süreli buff'ı aktifken yeniden atmaya izin verip vermediği bilinmiyor `[A]`; sunucu reddi insan oyuncu için de geçerlidir (aynı kural, K-5). (d) `CastTime 0` skill'lerde CASTING paketinin gönderilmemesi F4-03'ten beri varsayımdır `[A]` (`docs/03` CLI-03: ölçümler `CastTime > 0` skill'leridir); bu plan çalışma zamanında Type4 `CastTime 0` için 1 paketin sunucuca kabul edildiğini (S1) doğrular, gerçek istemcinin gönderim biçimi insan ölçümü (T-MECH-BUF) işidir. (e) Hız/stun türü debuff'larda (`BuffType` 5, 6, 40, 47) sunucu direnç zarı atar ve başarısız olsa bile yayını `bResult = 1` ile yapar (`MagicInstance.cpp:1819-1849`, `docs/03` MB-09): bot bunu `effected` görür, gerçek etki `snap`'ten okunur (karar katmanı işi). (f) Skill ağacı puanı denetlenmez (KI-016): `1126` ağacı için `BotPHD` (6 = 0), `1127` için `BotPHB` (7 = 0) `srv_fail` alır; testler doğru profilleri kullanır. (g) `UseStanding` 51..54 (KI-017) ve `Etc != 0` quest skill'leri (F4-27, `db/003` uygulanmadan sınanmaz) bu planın sınama kapsamı dışındadır.
- Telemetri hacmi değişmez. `tools/bot-telemetry-report.py` değişmez (`srv_fail` zaten MET-ACT-02'ye sayılıyor).
- `list`/`snap` ve `Telemetry.*` değişmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-28` (taban `gece/2026-10-02` @ `8b71514`); `734e64e` `[F4-28] Type4 tek tipli skill destegi: CastTypesSupported (4,0) ve testler` (3 kod dosyası); rapor/`Durum` commit'i bu satırın altında.
- Değişen dosyalar ve nedenleri:
  - `BotCore/BotCombat.h`: `CastTypesSupported` kuralı `type1 == 0` iken artık `type0 == 4`'ü de kabul eder (tek mantık değişikliği); başlık/fonksiyon yorumu `MEC-MAG-15` ve F4-28'i anar. `IsGatedType`/`CastQuestAllowed`/`TypeStamp`/`MinGatedSince` değişmedi.
  - `Tests/BotCoreTests/CombatTests.cpp`: `Combat_CastTypes_Supported` güncellendi (`(4, 0)` artık `true`; `false` bloğuna `(4, 1)`, `(4, 2)`, `(4, 4)`, `(4, 9)` eklendi; diğer satırlar aynen); yeni `Combat_TypeGate_Type4Single` (`(4,0)` tip damgası kapısı, `MinGatedSince`/`CheckCastStart`/`CastWaitMs`; plan §5.2-b). 103 → 104 test.
  - `GameServer/Bot/ActionExecutor.h`: yalnızca yorumlar (`CastOutcome::reason`, `BeginCast` destek cümlesi, `TickCast` tek-tipli Type4 sonuç notu). Kod değişikliği yok.
- Derleme sonucu:
  - `./tools/build.sh Release`: rc=0; son satır `proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe`. Yalnızca önceden var olan `UpgradeHandler.cpp(634/862)` C4789 uyarıları; `BotCombat.h`/`CombatTests.cpp`/`ActionExecutor.h` ve içeren `.cpp`'lerde (`ActionExecutor.cpp`, `BotManager.cpp`, `BotSession.cpp`, `ScenarioRunner.cpp`) uyarı yok.
  - `./tools/build.sh Debug`: rc=0; son satır `proj-GameServer.vcxproj -> ...\build\bin\x86-Debug\Server\GameServer.exe`.
  - `./tools/run-tests.sh Release`: rc=0, `104 tests, 0 failed`; `.[ OK ] Combat_CastTypes_Supported`, `.[ OK ] Combat_TypeGate_Type4Single` (ayrıca `Combat_TypeGate_MinSince`, `Combat_TypeGate_DualCast`). `Debug`: rc=0, `104 tests, 0 failed`.
- §5.3-a kod okuma doğrulamaları (satırlar `bot/F4-28` @ `734e64e`):
  1. `grep -n "bType\["` yalnızca `729, 730, 809, 851, 855, 1001, 1283, 1284` (`:1283-1284` pot sınıflandırması, Type3); `BeginCast`'te `CastTypesSupported` dışında Type4'ü reddeden koşul yok (`:730`).
  2. `bad_target`: `:752-757` `wantedSelf = (bMoral == MORAL_SELF)`, `wantedTarget = (bMoral == MORAL_ENEMY)`; `MORAL_SELF` hedefsiz, `MORAL_ENEMY` hedefli ister, `MORAL_FRIEND_WITHME`/`MORAL_ALL` ikisini de kabul eder (koşul yalnızca bu ikisi).
  3. `TickCast` tip kapısı `:849-859` (`typeGated` iki tip; `typeStamps[i].has = (ty < 8 && s->m_castTypeHas[ty])`); EFFECTING sonrası damga döngüsü `:1001-1007` `bType[0] == 4` için `m_castTypeHas/Last[4]`'ü okur/yazar (dizi 8 elemanlı); `CastStartCheck` `:883`.
  4. `SubmitCast` sonuç eşlemesi `:637-651`: `op == MAGIC_EFFECTING` ⇒ `effected` (`SKILLMAGIC_FAIL_ATTACKZERO` ⇒ `missed`), `MAGIC_FAIL` ⇒ `srv_fail`; tek hedefli Type4 süresi pozitif olduğundan `missed` üretilmez.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (Release rc=0, değişen dosyalarda uyarı yok; yalnızca eski `UpgradeHandler.cpp` C4789).
  - K2 ✔ (Debug rc=0).
  - K3 ✔ (Release ve Debug `104 tests, 0 failed`; iki test adı `[ OK ]`).
  - K4 ✔ (`grep` eşleşme yok; yalnızca `<algorithm>`/`<cstdint>`; eklenen satırlarda `std::min`/`std::max` yok).
  - K5 ✔ (yalnızca `CastTypesSupported` bölgesi değişti; `(4,0)` true, `(4,1)`, `(4,2)`, `(4,3)`, `(4,4)`, `(4,9)`, `(1,4)`, `(2,4)`, `(0,4)` false).
  - K6 ✔ (`ActionExecutor.cpp`, `BotSession.*`, `BotManager.cpp` ve vcxproj dosyaları fark yok; §5.3-a dört okuma yukarıda).
  - K7 ✔ (yeni ini anahtarı/komut/thread/telemetri olayı yok; eklenen satırlarda `Emit(` yok).
  - K8 ✔ (`git diff --stat gece/2026-10-02...bot/F4-28` yalnızca §4'teki 3 dosyayı gösterir; rapor/`Durum` commit'i plan dosyasını ekler).
  - K9 ✔ (`file`: üç dosya da ASCII + CRLF; `git diff --check` boş).
  - K10 ✔ (`CheckMoveStep` 2, `CheckAttack` 1, `CheckCastStart`/`CheckCastEffect`/`CheckCastFly`/`CheckCastLand`/`CheckCastCancel`/`CheckPotion` 1; 103 eski test de geçiyor).
  - K11 ✔ (`python3 tools/check-perception-contract.py` `RESULT: PASS`; `R1 0/0`, `R2 0/28`, `R3 0/18`, `R4 0/0`, `R5 0/0`; 19 dosya).
  - K12 (Claude çalışma zamanı S1–S5) — bu turda çalıştırılmadı (doğrulama işi).
- Plandan sapmalar: Yok. `ActionExecutor.h` yorumlarında plan metnindeki tek tırnaklı alıntılar komşu satırların üslubuna uyularak çift tırnaklı yazıldı (metin anlamı aynı). `BotCore/BotCombat.h` yorumunda plan `MEC-MAG-15`'i anar.
- Açık sorular: Yok. Uygulama sırasında Type4 için eksik bir kural görülmedi (ör. sunucu `MAGIC_FAIL` yolları, moral denetimi, tip kapısı, yankı eşlemesi plan §2/§5.4 ile birebir).

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- **Karar: DOĞRULANDI.**
- İncelenen commit: `bot/F4-28` @ `00cc446` (kod `734e64e`; taban `gece/2026-10-02`). Çalışma ağacı temizdi; birleştirme otonom döngüde döngü betiğine bırakıldı.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `BotCombat.h`/`CombatTests.cpp`/`ActionExecutor.h` `touch` + `tools/build.sh Release` rc=0; `UpgradeHandler.cpp` dışında hiçbir uyarı yok (derleme günlüğü filtrelendi; eski C4789 uyarıları plan dışı) |
| K2 | ✔ | `tools/build.sh Debug` rc=0 |
| K3 | ✔ | Release ve Debug: `104 tests, 0 failed`; `Combat_CastTypes_Supported` ve `Combat_TypeGate_Type4Single` `[ OK ]` |
| K4 | ✔ | `BotCombat.h` içinde `windows.h/stdafx/GameServer/shared/` eşleşmesi yok; eklenen satırlarda `std::min/max` yok (tek eşleşme plan dosyasındaki rapor metni) |
| K5 | ✔ | diff yalnızca `CastTypesSupported` bölgesi (`BotCombat.h:317-327`, +4/−4 satır); testte `(4,0)` true, `(4,1)`, `(4,2)`, `(4,3)`, `(4,4)`, `(4,9)`, `(0,4)`, `(1,4)`, `(2,4)` false |
| K6 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-28`: `ActionExecutor.cpp`, `BotSession.*`, `BotManager.cpp` yok; `ActionExecutor.cpp:729-759` (tek destek çağrısı `:730`, moral `:752-757`), `grep "bType\["` tam olarak plandaki satırları verir; uygulayıcının dört okuma iddiası kodla uyuşuyor |
| K7 | ✔ | eklenen kod satırlarında `Emit(`/ini/komut/thread yok |
| K8 | ✔ | değişen dosyalar: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.h` + plan dosyası; vcxproj değişmedi |
| K9 | ✔ | `file`: üç dosya ASCII + CRLF; `git diff --check` boş |
| K10 | ✔ | `CheckMoveStep` 2, `CheckAttack`/`CheckCastStart`/`CheckCastEffect`/`CheckCastFly`/`CheckCastLand`/`CheckCastCancel`/`CheckPotion` 1; 104 test geçiyor |
| K11 | ✔ | `check-perception-contract.py` `RESULT: PASS`, R1 0/0, R2 0/28, R3 0/18, R4 0/0, R5 0/0 |
| K12 | ✔ | S1–S5 çalışma zamanında geçti (aşağıda; iki alt madde için "Notlar" bölümüne bak) |

**Çalışma zamanı (Release, `ENABLED=1`, `TELEMETRY=decisions`, zone 71; `Logs/bots/2026-10-03/live-021339.jsonl`, `Logs/Bot_3_10_2026.log`):**

- **S1 ✔** Defense `106007`: tek `CastEffect` (`since_casting_ms` 0), `effected`, `op 3`, `code 15`, `snap` `type=2 buff`; sprint `106001`: `code 10`, MP 5370 → 5365 (−5, bir kez), `type=6`; Mana Shield `110815`: `code 40`, MP 6021 → 5871 (−150, bir kez), `type=31`. Log `cast finished (effected) after 1 cycle(s), 1 ok, 1 packet(s) sent`.
- **S2 ✔** (a) Defense ×2: 2. döngü `ok:false`, `srv_fail`, `op 4`, `code -103`, log `(srv_fail) after 2 cycle(s), 1 ok, 2 packet(s)`; `snap` buff yenilenmedi (14 sn → 3 sn kalan). Mana Shield ×2: ikinci `srv_fail -103`, MP yalnızca bir kez −150 (5871'de kaldı: reddedilen Type4 MP düşürmüyor). (b) `BotWP_K` Defense'liyken `112603` Insensibility Skin (`BuffType 2`): `CastStart` `srv_fail`, `op 4`, `code -100`, `cast stopped (srv_fail)`, EFFECTING yok, priest MP 6392 değişmedi. (c) Strength ×2: 1. döngü `effected` `code 600` (MP −10 görüldü), 2. döngü CASTING `srv_fail -100`.
- **S3 ✔** Strength `112004` dosta: `CastStart` (`cast_ms 1580`) → `CastEffect` (`since_casting_ms` 1645, `code 600`), `snap BotWP_K` `type=7 buff remain≈595s`; Resist poison kendine `code 600`, `type=8`. Tip kapısı (`112004` → `112006` kendine): 2. `CastStart` 1. `CastEffect`'ten **1099 ms** sonra (≥1000), `FAIRNESS_REJECT` yok. Ice comet `110651` `{3,4}` → Mana Shield: Ice comet `CastEffect` `t=213163988`, Mana Shield `CastEffect` `t=213165078` (**1090 ms**): Type4 damgası çift tipliden geliyor (F4-26'dan kalan boşluk kapandı).
- **S4 ✔** Malice `112703` düşmana ×2: iki döngü de `effected`, `code 150` (`srv_fail` yok); MP −40 görüldü; `snap BotWP_E` `buff skill=112703 type=2 debuff remain=149s`, ikinci `EFFECTING`'ten ≈3 sn sonra `remain=147s` (süre yenilendi). Confusion/Parasite sınanmadı (plan "aynı sırayı verir" diyor, zorunlu değil).
- **S5 ✔** `110573` (UseItem) ⇒ `unsupported_skill`; Type4 party `112606` (Moral 4; `MAGIC`'ten bulundu) hem `self` hem adlı hedefle ⇒ `unsupported_skill`; `106007 BotWP_E` ⇒ `bad_target`; `112703 self` ⇒ `bad_target`; `BotWP_K 112004` ⇒ `bad_skill`; hedef 70 m uzakta ⇒ `FAIRNESS_REJECT` `MEC-MAG-11` `out_of_range` (`value 70, limit 56`), CASTING gitmedi. Gerilemesiz: Ignition ×3 `effected` ×3 (6 paket, `CastFly` yok); Ice arrow `110615` `CastStart`→`CastFly`→`CastEffect`, 3 paket; Fire ball `110515` aynı, 3 paket; Strength CASTING'inde `cast off` ⇒ `CastCancel` `cancelled`, `op 4`, `code -100`, priest MP 6392 değişmedi, `snap`'te buff yok; ardından yeniden atılan Strength `effected`. İkinci botun `snap BotMF_K events` çıktısı Type4 yayınını gösterdi (`op=3 skill=112004 caster=2986 target=2986`). `tick_p50` 2–4 µs; `GameServer.log`'a yeni hata yazılmadı (son kayıt 2026-10-02). Sunucu 3/3 UP idi; sonunda `stop` ile kapatıldı, `GameServer.ini` değişmedi (yedekle aynı), `Scripts/f428_*` silindi.

**Bulgular (engelleyici yok):**

1. *(not)* Sınama sırasında `PERF_SAMPLE` `tick_p95_us` 72 pencerenin üçünde >1 ms çıktı (1266, 1014, 1348 µs; `tick_p50` 4 µs). Üçü spawn/despawn pencereleridir (`written` yüksek, `sessions` 5'e çıkışı); cast sürelerinde p95 ≈ 100 µs. Bu plan tick yoluna kod eklemediğinden gerileme değil; bilinen spawn maliyetidir.
2. *(not)* MP düşüşü ölçümü sunucu MP yenilemesiyle karışabildiği için Defense/Strength/Malice için "bir kez −N" yalnızca yakın zamanlı `list`'te (sprint −5, Mana Shield −150, Malice −40, Strength −10) ve reddedilen Mana Shield'in MP'sinin değişmemesiyle kanıtlandı; Defense için (−4) yenileme nedeniyle ayrı ölçüm alınmadı.
3. *(not)* K12'deki iki alt madde bu turda çalıştırılmadı: `TELEMETRY=summary` iken Type4 cast'i ve `ENABLED=0` davranışı. Diff `ActionExecutor.cpp`/telemetriye dokunmadığı ve tek değişiklik saf bir koşul olduğu için bu yollar yapısal olarak etkilenmez; yeniden başlatma gerektirdiğinden atlandı.
4. *(not)* Çalışma zamanı sınaması Type4 `CastTime 0` için 1 paketin sunucuca kabul edildiğini doğruladı (§8-d): Defense/sprint/Mana Shield tek `CastEffect` ile `effected`. Gerçek istemci gönderimi hâlâ T-MECH-BUF insan ölçümü işidir.
5. *(not)* Uygulayıcı raporu doğru: commit listesi, dosyalar, derleme ve test sayıları kendi çalıştırmamla örtüşüyor; sapma/soru yok.
