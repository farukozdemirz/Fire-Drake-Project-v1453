# F4-36: `ActionExecutor` eşya tüketen sınıf skill'leri dilimi — sınıf taşı ve scroll gerektiren skill'ler (`UseItem`, ADR-0018 dilim 6e)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-36` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-35 (`BeginCast` Type8 bloğu ve destek koşulu) — `KAPANDI` (merge `9239997`); F4-27 (`quest_locked` kuralı) — `KAPANDI`; F4-28 (Type4 tek tipli), F4-26 (`{3, 4}` çifti), F4-25 (uçan Type3), F4-29 (alan) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-11, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-14, MEC-MAG-15, MEC-MAG-23 (bu planla eklendi, `[D]`), U8/U9 (`docs/03` §4.3), SK-06, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 Ek 1 madde 1(e), Ek 12; `docs/05` satır 150-156 (Impact, Absolute power), `docs/07` (priest), T-MAG-01/02 altyapısı |
| Tahmini büyüklük | S (4 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.cpp`, `ActionExecutor.h`; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

`BeginCast` bugün `MAGIC.UseItem != 0` olan **her** skill'i `unsupported_skill` ile reddediyor (`ActionExecutor.cpp:771`; yalnızca F4-33 diriltmesi ayrı yoldan geçer). Oysa mage/priest/warrior'ın F6/F7 davranışlarının dayandığı skill'lerin bir kısmı sınıf taşı ya da scroll ister (`docs/05`, `docs/17` §2.1 "Warrior kontrol", ADR-0018 Ek 1 madde 1(e)):

- **Mage patlama zinciri** (`docs/05` satır 150-156): Fire Impact `110557`, Ice Impact `110657`, Thunder Impact `110757` (`UseItem 379070000` Spell of impact) ve Absolute power `110802` (`UseItem 379065000` scroll + `BeforeAction 3` ⇒ Stone of Mage tüketir; El Morad `2xxxxx` karşılıkları);
- **Priest** Judgment `112802`/`212802` (`Type1 = 1` tek tipli melee, `UseItem 379066000` + `BeforeAction 4` ⇒ Stone of Priest tüketir);
- aynı kapıyla verisi gereği açılanlar (ölçülmez, §3): Minor Resist `110825` (alan, `Moral` 10), Counter Curse `112676` (`Moral` 6, `Etc` 523), Discountis `112772` (alan `{3, 0}`), Armor ailesi (`Etc` 516/517), Spell of blast/thorn ailesi (botlarda eşya yok ⇒ `no_item`).

Bu skill'lerin tip/Moral biçimleri (`{3, 0}`, `{3, 4}`, `{4, 0}`, `{1, 0}`; `Moral` 1, 6, 7, 10; uçan/uçmayan) **zaten desteklenen** biçimlerdir; bot yalnızca `UseItem != 0` olduğu için reddediliyor. Eksik olan tek şey: (a) eşya gerektiren **sınıf skill'ini** tip kapısından geçirmek, (b) sunucunun eşya kuralını (`docs/03` U8/U9) botun **kendi çantasına** uygulayan bir ön kontrol (`no_item`; boşa paket ve MP/bekleme harcamamak için). Pot yolu aynı kalıbı kullanır (`BeginPotion` sunucunun eşya kurallarını yansıtır, `ActionExecutor.cpp:1383-1388`).

Bu plan:

1. `BotCore/BotCombat.h`'a `CastItemSkillSupported` ve `CastConsumeItem` saf mantık fonksiyonlarını ekler;
2. `BeginCast`'te `m->iUseItem != 0` reddini `!BotCore::CastItemSkillSupported(...)` ile değiştirir ve quest denetiminden sonra çantadaki eşyaları denetleyen `no_item` kuralını ekler;
3. sunucu davranışını belgeler ve çalışma zamanında doğrular.

**Kapalı kalanlar (ve neden):** `MAGIC.Skill == 0` olan eşya-efekt büyüleri (pot, scroll, pirinç keki: `460006`, `490001`...) `/bot cast` ile değil pot yolundan (`UsePotion`, F4-04) kullanılır, `unsupported_skill` kalır; Type5 + `UseItem` (diriltme F4-33'ün ayrı yoludur); Type2 (okçu), Type6 (dönüşüm), Type8 + `UseItem` (`CastSummonSupported`/`CastWarpSupported` `useItem == 0` ister); **Type1 + Type3/4 çiftleri** (Scream `106802` `{1, 4}`, Shock Stun `106820`/Exceed Break `106815` `{1, 3}`: warrior kontrolü, çift tip desteği ayrı dilim, §3 "Kapsam dışı").

F4'ün otuz altıncı planıdır (ADR-0018 sırası: ... Type8 warp ✔ → **eşya tüketen sınıf skill'leri (bu plan, dilim 6e)** → warrior `{1, 3}`/`{1, 4}` çiftleri (6f) → CLI-12 → envanter doldurma → T-MECH-SKILL botla koşusu).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-36)" (bu planla birlikte yazıldı), "Ek (F4-33)" (item'li diriltme). `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (Ek 1 madde 1(e), Ek 12).
- `docs/03` §4.2 **MEC-MAG-23** (bu planla eklendi), MEC-MAG-20 (taş tüketimi farkı); §4.3 U8, U9; `docs/05` satır 74, 150-156; `docs/06` satır 154 (Stone of Warrior); `docs/17` §2.1.
- `plans/F4-35-aksiyon-yurutucu-warp-gate-descent.md` (aynı istisna kalıbı; **yazılı planı değil, kodu esas al**).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `9239997` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.cpp:236-260` `IsAvailable()` eşya kuralı: `pSkill->bType[0]` 2 ve 6 dışında ve `iUseItem != 0` iken **`CanUseItem(iUseItem)`** başarısızsa `SkillUseFail` (`:246-248`; diriltmede hedeften, diğerlerinde çağırandan); ardından **`nConsumeItem`**: `nBeforeAction ∈ [ClassWarrior(1), ClassPriest(4)]` ise `CLASS_STONE_BASE_ID + nBeforeAction * 1000` (= 379059000 Warrior, 379060000 Rogue, 379061000 Mage, 379062000 Priest), değilse `iUseItem` (`:252-255`); `iUseItem != 0` iken **`CanUseItem(nConsumeItem)`** de başarısız olmamalıdır (`:258-260`). **`IsAvailable()` hem CASTING hem EFFECTING'te çalışır** (`:284-286` civarı, F4-35 notu): eşya yoksa CASTING'te `MAGIC_FAIL`.
  - `GameServer/MagicInstance.h:61` `#define CLASS_STONE_BASE_ID 379058000`. `GameServer/User.h:46-49` `ClassWarrior = 1`, `ClassRogue = 2`, `ClassMage = 3`, `ClassPriest = 4`.
  - `GameServer/User.cpp:5121-5145` `CUser::CanUseItem(nItemID, sCount = 1)`: `GetItemPtr` yoksa `false`; dönüşümlü (`isTransformed()`) özel durum; `m_bClass != 0 && !JobGroupCheck(m_bClass)` ya da `GetLevel() < ReqLevel || GetLevel() > ReqLevelMax` ya da `!CheckExistItem(nItemID, sCount)` ⇒ `false`. `GameServer/User.h:984` bildirimi, `public:` bölümünde (`:302`-`:1079`). `GameServer/ItemHandler.cpp:254-266` `CheckExistItem`: çantada tek yığında `sCount >= count`.
  - `GameServer/MagicInstance.cpp:128-133` EFFECTING sonunda `ExecuteSkill(bType[1])` ardından **`if (pSkill->bType[0] != 2) ConsumeItem()`**. `:2983-2998` `ConsumeItem()`: `nConsumeItem` **370001000, 370002000, 370003000, 379069000, 379070000, 379063000, 379064000, 379065000, 379066000** ise `RobItem(0)` (= **hiçbir şey tüketmez**; `ItemHandler.cpp:349-350` `nItemID == 0` ⇒ `false`), aksi halde `RobItem(nConsumeItem)` (1 adet). Yani scroll/spell kâğıtları (379063/64/65/66/69/70, 3700xx) **tükenmez**, yalnızca **sınıf taşları** (379059/61/62) ve diğer `UseItem`'ler tüketilir.
  - `GameServer/MagicInstance.cpp:1014-1028` (`IsAvailable()` Type3/4 oyuncu eşya dalı, EFFECTING): `GetItemPtr(iUseItem) == nullptr` ⇒ `return false` (yayınsız); `m_bClass != 0 && !JobGroupCheck` ya da `ReqLevel` > seviye ⇒ `return false`. `:1034-1035` `MSpChange` Type4'te yalnızca `sTargetID == -1`, aksi halde Type4 MP'si `ExecuteType4`'te düşer (F4-28).
  - `GameServer/MagicInstance.cpp:956-963` skill ağacı: `modulator = sSkill % 10`; `modulator != 0` ise `sSkill / 10` sınıf kimliği olmalı ve `m_bstrSkill[modulator] >= SkillLevel` olmalı. Eşyalı skill'ler aynı denetimden geçer (ağaç yetersizse CASTING'te `srv_fail`).
  - `GameServer/Bot/ActionExecutor.cpp` (`9239997`): `BeginCast` sınıf/seviye denetimi `:726-731`, diriltme bloğu `:736-745` (`m->bType[0] == 5 && m->iUseItem != 0`), Type8 bloğu `:750-764`, **destek koşulu `:767-774` (`m->iUseItem != 0` satırı `:771`)**, `quest_locked` kuralı `:780-789`, `bad_target` `:790-798` (`bool self` `:790`, `wantedTarget` `:792`). `TickCast` (`:814-`) ve `SubmitCast` `iUseItem`'e **bakmaz** (`grep -n "iUseItem" GameServer/Bot/ActionExecutor.cpp`: yalnızca `BeginCast` ve `PotMagicSupported`). `PotMagicSupported` (`:1325-1341`) `sSkill == 0` ister: pot büyüleri ayrı yoldadır.
  - `BotCore/BotCombat.h`: `CastTypesSupported` `:322`, `CastTypeMoralSupported` `:333`, `CastMoralSupported` `:369`, `CastHpCostSupported` (`:381` civarı), `CastQuestAllowed` `:476`, `CastWarpNeedsOtherTarget` `:450`; `Tests/BotCoreTests/CombatTests.cpp` son test `Combat_WarpCast_Guard` (`:1554-`). Toplam birim test **112**.
- **Veri notu (Claude yerel `MAGIC`, `MAGIC_TYPE1/3/4`, `ITEM` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** `MagicNum < 300000`, `Skill != 0`, `UseItem != 0`, bot sınıfları (105/106/110/112/205/206/210/212), `Type1 ∈ {1, 3, 4}`:
  - **Fire Impact** `110557` (`Skill 1105` ⇒ ağaç indeksi 5, `SkillLevel 57`, `Moral 7`, `Msp 220`, `CastTime 15`, `ReCastTime 203` (20,3 sn), `Range 56`, `Etc 0`, `FlyingEffect 0`, `{3, 0}`, `UseItem 379070000`, `BeforeAction 0`; `MAGIC_TYPE3` `DirectType 1`, `FirstDamage −1260`, `TimeDamage −1000`); El Morad `210557` (`Range 90`). **Ice Impact** `110657`/`210657` (`Skill 1106` ⇒ indeks 6, `lv 57`, `{3, 4}`, `FlyingEffect 293`: uçan, F4-25/F4-26 akışı) ve **Thunder Impact** `110757`/`210757` (`Skill 1107` ⇒ indeks 7, `{3, 0}`, `FlyingEffect 392`). Üçü de `UseItem 379070000`, `BeforeAction 0` ⇒ `nConsumeItem` = `379070000` ⇒ **tüketilmez**.
  - **Absolute power** `110802`/`210802` (`Skill 1108` ⇒ indeks 8, `SkillLevel 2`, `Moral 1` (kendine), `Msp 240`, `CastTime 0`, `ReCastTime 250` (25 sn), `Range 56`, `Etc 0`, `{4, 0}`, `UseItem 379065000` scroll, **`BeforeAction 3`** ⇒ `nConsumeItem` = `379061000` Stone of Mage ⇒ **her atışta 1 taş tüketilir**; `MAGIC_TYPE4` `BuffType 10`, `Duration 30`).
  - **Judgment** `112802`/`212802` (`Skill 1128`/`2128` ⇒ indeks 8, `SkillLevel 2`, `Moral 7`, `Msp 200`, `CastTime 0`, `ReCastTime 5` (0,5 sn), `Range 0`, `Etc 0`, `{1, 0}`, `UseItem 379066000` Judgment Scroll, **`BeforeAction 4`** ⇒ `nConsumeItem` = `379062000` Stone of Priest ⇒ **her atışta 1 taş tüketilir**; `MAGIC_TYPE1` `Hit 500`, `AddDamage 150`).
  - **`ITEM` tablosu:** `379059000` Stone of Warrior, `379061000` Stone of Mage, `379062000` Stone of Priest (`Class 0`, `ReqLevel 1`, `ReqLevelMax 99`, `Countable 1`); `379063000` Scream Scroll, `379065000` Absolute Power Scroll, `379066000` Judgment Scroll (`Class 0`, `Countable 0`); `379070000` Spell of impact, `379069000` Spell of thorn (`Class 3` = mage grubu, `ReqLevel 1..99`); `370001000/2/3` Spell of Fire/Glacier/Thunder Blast (`Class 0`).
  - **Veri güdümlü olarak açılıp bu planda ölçülmeyenler:** Minor Resist `110825`/`210825` (`Moral 10` alan, `{4, 0}`, `UseItem 379061000`, `Skill 1108` lv 20), Counter Curse `112676`/`212676` (`Moral 6`, `{4, 0}`, `Etc 523`, `SkillLevel 80`), Discountis `112772`/`212772` (`Moral 10`, `{3, 0}`, `Etc 523`, lv 80), Fire/Ice/Lightning Armor `110573`/`110673`/`110773` ve El Morad (`Moral 1`, `{4, 0}`, `Etc 516`, lv 75), Freezing Distance `110674` (`Etc 517`, lv 80), Spell of blast `110535`/`110635`/`110735` (`UseItem 37000x000`), Spell of thorn `110554`/`110754` (`UseItem 379069000`), Fire Thorn `210554`... Hiçbiri ek kod gerektirmez; ağaç/quest/eşya kuralları sunucuda ya da botta (`quest_locked`, `no_item`) uygulanır.
  - **Hâlâ kapalı:** Scream `106802`/`206802` (`{1, 4}`), Exceed Break `106815`/`206815` ve Shock Stun `106820`/`206820` (`{1, 3}`): `CastTypesSupported(1, 4)`/`(1, 3)` `false` (çift tip, 6f); Elysian Web `112825` (`Moral 11`: `CastMoralSupported(11)` `false`); `Skill == 0` eşya büyüleri.
- **Bot karakter notu (`db/002_bot_characters.sql:124-137`, `:205-216`).** Skill ağacı baytları (`m_bstrSkill[0..9]`): `BotMF_*` `0x00000000004634001400` ⇒ `[5] = 70`, `[6] = 52`, `[7] = 0`, `[8] = 20`; `BotMI_*` `0x00000000003446001400` ⇒ `[5] = 52`, `[6] = 70`, `[7] = 0`, `[8] = 20`; `BotPHD_*` `0x00000000003C003E1400` ve `BotPHB_*` `0x00000000003C3E001400` ⇒ `[8] = 20`. Dolayısıyla: Fire Impact yalnızca `BotMF_*`'te (52 < 57 ⇒ `BotMI_*` CASTING'te `srv_fail`), Ice Impact yalnızca `BotMI_*`'te (`BotMF_*` 52 < 57), Thunder Impact **hiçbirinde** (`[7] = 0` ⇒ `srv_fail`), Absolute power ve Judgment tüm ilgili botlarda (`[8] = 20 >= 2`). Çanta (slot 18-20): warrior `379059000` ×50 + `379063000` ×1; priest `379062000` ×50 + `379066000` ×1; mage `379061000` ×50 + `379065000` ×1 + `379070000` ×1 (**`379069000` yok**: Thorn skill'leri `no_item`). Her botun `Hp = Mp = 32000` (gerçek MP ≈ 6000 civarı, F4-35 ölçümü), seviye 80.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `kClassStoneBase`, `kClassWarriorId`, `kClassPriestId` sabitleri; `CastItemSkillSupported(type0, skill, useItem)` ve `CastConsumeItem(beforeAction, useItem)` (§5.1).
2. **`BeginCast` (`ActionExecutor.cpp`):** destek koşulunda `|| m->iUseItem != 0` → `|| !BotCore::CastItemSkillSupported(m->bType[0], m->sSkill, m->iUseItem)`; `quest_locked` kuralından sonra, `bad_target` kuralından önce **`no_item`** kuralı (§5.3).
3. **Yorumlar (`ActionExecutor.h`):** `reason` listesine `"no_item"`, `BeginCast` açıklamasına eşyalı sınıf skill'leri (§5.3 d).
4. **Birim testleri:** yeni `Combat_ItemSkill_Guard` (112 → 113).
5. **Sonuç sözleşmesi (§5.4):** `docs/03` MEC-MAG-23 çalışma zamanında sınanır.

**Kapsam dışı (yapılmayacak)**

- **Çift tipli Type1 + Type3/4 skill'ler** (Scream, Exceed Break, Shock Stun): `CastTypesSupported` **değişmez**; 6f (warrior `{1, 3}`/`{1, 4}` çiftleri) ayrı plandır ve bu planın `no_item` kuralını kullanacaktır.
- `Skill == 0` eşya büyüleri (pot, scroll, yemek) ve `/bot pot` yolu; Type2/6/8 + `UseItem`; Type5 + `UseItem` (diriltme F4-33'te); `Moral` 11/12/13 (Elysian Web).
- **Karar katmanı:** hangi taşın/scroll'un ne zaman kullanılacağı, stok izleme, taş bitince davranış (`docs/06` satır 154), Absolute power'ın patlama penceresiyle eşleşmesi (`docs/05` satır 156) F6/F7'nin işidir. Bota **taş sayısı/stok önkontrolü eklenmez** (yalnızca "≥ 1 adet var" `no_item` kuralı; gerçek stok izleme `Perception` öz durumunun işidir, AC-LRN-03: yalnızca botun kendi çantası).
- Envanter doldurma/yeniden stoklama (taş bitince `db/002` yeniden uygulanır; ayrı dilim m.8), `BotSession` öz durumuna taş sayısı ekleme (`FillSelfExtras`), `/bot snap` çıktısı.
- Yeni komut/ini anahtarı/telemetri olayı türü/alan, `BotSession.*`/`BotManager.cpp` değişikliği, `tools/` betik değişikliği (özellikle `check-perception-contract.py` istisna listeleri; `no_item` yalnızca `CUser::CanUseItem` çağıran bir `BeginCast` satırıdır, yeni sunucu nesnesi okuması değildir: çağıranın kendi çantası, R2 istisnası gerektirmez, uygulayıcı `K10` ile doğrular).
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | yeni sabitler + `CastItemSkillSupported` + `CastConsumeItem` (mevcut fonksiyonlar değişmez; yalnızca ekleme) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | yeni `Combat_ItemSkill_Guard` dosyanın sonuna (112 → 113); mevcut testler değişmez |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yalnızca `BeginCast`: destek koşulundaki `m->iUseItem != 0` satırı ve yeni `no_item` bloğu |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca `CastOutcome::reason` listesi ve `BeginCast` yorumu |

`BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `ScenarioRunner.*`, `ScriptRunner.*`, `tools/` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`CastWarpNeedsOtherTarget`'tan (`:450`) hemen sonra, `CastTargetIdField` öncesine şu bloğu ekle (ASCII, CRLF, tab, `namespace BotCore` içinde; yeni `#include` yok; **`std::min`/`std::max` kullanma**). Önce `grep -n "kClassStoneBase\|kClassWarriorId\|kClassPriestId\|CastItemSkillSupported\|CastConsumeItem" BotCore` ile adların boş olduğunu doğrula (çakışırsa durup raporla):

```cpp
	// --- item-requiring class skills (ADR-0017 Ek F4-36, docs/03 MEC-MAG-23) ---

	// MagicInstance.h CLASS_STONE_BASE_ID: with MAGIC.BeforeAction 1..4 (ClassWarrior..ClassPriest, User.h) the server takes
	// ONE class stone 379058000 + BeforeAction * 1000 (379059000 Warrior, 379061000 Mage, 379062000 Priest) and treats
	// MAGIC.UseItem as a required, not consumed item; otherwise it takes MAGIC.UseItem (a few scrolls are never taken:
	// MagicInstance::ConsumeItem).
	constexpr uint32_t kClassStoneBase = 379058000;
	constexpr uint32_t kClassStoneStep = 1000;
	constexpr uint32_t kClassWarriorId = 1;
	constexpr uint32_t kClassPriestId = 4;

	// A skill with MAGIC.UseItem != 0 is opened for the bot when it is a CLASS skill (MAGIC.Skill != 0) of Type1, 3 or 4;
	// the type/Moral/flying shape is still judged by the other Cast*Supported functions. Item-effect magics (Skill == 0:
	// potions, scrolls, food) go through the potion path, Type5 + item (resurrection, F4-33) has its own path and
	// Type2/6/8 + item stay closed. A skill without an item always passes (this function judges the item only).
	inline bool CastItemSkillSupported(uint8_t type0, uint16_t skill, uint32_t useItem)
	{
		if (useItem == 0)
			return true;

		return skill != 0 && (type0 == 1 || type0 == 3 || type0 == 4);
	}

	// The item the server takes for a cast (MagicInstance.cpp:252-255): the class stone for BeforeAction 1..4, else
	// MAGIC.UseItem. The server checks BOTH MAGIC.UseItem and this item (CanUseItem) when UseItem != 0.
	inline uint32_t CastConsumeItem(uint32_t beforeAction, uint32_t useItem)
	{
		if (beforeAction >= kClassWarriorId && beforeAction <= kClassPriestId)
			return kClassStoneBase + beforeAction * kClassStoneStep;

		return useItem;
	}
```

Dosyanın kalanı **değişmez** (`CastTypesSupported`, `CastTypeMoralSupported`, `CastMoralSupported`, `CastSummonSupported`, `CastWarpSupported` aynen). `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

Yeni **`Combat_ItemSkill_Guard`** (dosyanın sonuna, `Combat_WarpCast_Guard`'dan sonra):

- `CastItemSkillSupported`, eşyasız her biçim geçer (`useItem == 0`): `(3, 1105, 0) == true`, `(1, 0, 0) == true`, `(8, 1100, 0) == true`, `(0, 0, 0) == true`.
- Eşyalı **sınıf skill'i**, Type 1/3/4 geçer: `(3, 1105, 379070000) == true` (Fire Impact), `(3, 1106, 379070000) == true` (Ice Impact), `(4, 1108, 379065000) == true` (Absolute power), `(1, 1128, 379066000) == true` (Judgment), `(1, 1068, 379063000) == true` (Scream: tip çifti başka fonksiyonda reddedilir).
- Eşyalı **ama kapalı**: `(5, 1127, 379006000) == false` (diriltme kendi yolunda), `(8, 1100, 379070000) == false`, `(2, 1105, 379070000) == false`, `(6, 1105, 379070000) == false`, `(0, 1105, 379070000) == false`; `Skill == 0` eşya büyüleri: `(3, 0, 389001000) == false` (Rice Cake 490001), `(3, 0, 310310010) == false` (Hyper Healing 460006), `(4, 0, 379070000) == false`.
- Mevcut fonksiyonlar değişmez: `CastTypesSupported(1, 4) == false`, `CastTypesSupported(1, 3) == false` (Scream/Shock Stun hâlâ çift tip), `CastTypesSupported(3, 4) == true`, `CastTypesSupported(4, 0) == true`, `CastTypesSupported(8, 0) == false`, `CastMoralSupported(11) == false`, `CastMoralSupported(7) == true`, `CastMoralSupported(10) == true`, `CastMoralSupported(6) == true`.
- `CastConsumeItem`: `(3, 379065000) == 379061000` (Absolute power: Stone of Mage), `(4, 379066000) == 379062000` (Judgment: Stone of Priest), `(1, 379063000) == 379059000` (Scream: Stone of Warrior), `(2, 379000000) == 379060000` (Rogue), `(0, 379070000) == 379070000` (Impact: scroll), `(5, 379070000) == 379070000` (sınır dışı), `(0, 0) == 0`, `(1, 0) == 379059000` (sunucu `BeforeAction 1..4` ise `UseItem` boşken de taşı seçer; botta yalnızca `UseItem != 0` iken kullanılır). Sabitler: `kClassStoneBase == 379058000`, `kClassStoneStep == 1000`, `kClassWarriorId == 1`, `kClassPriestId == 4`.
- Test sayısı **113**. Test adı ve çerçeve makroları (`TEST_CASE`, `CHECK_EQ`) dosyadaki mevcut testlerle aynı (örnek `Combat_WarpCast_Guard` `:1554`); işaretli/işaretsiz karşılaştırma uyarısı (C4389) çıkarsa F4-35'teki gibi `(int)`/`uint32_t` dönüşümü ekle.

### 5.3 `ActionExecutor.cpp` ve `ActionExecutor.h`

**a) Destek koşulu (`ActionExecutor.cpp:771`).** Yalnızca şu satırı değiştir (koşulun diğer beş terimi, `if (!resurrection && !summon && !warp` ilk satırı ve gövdesi aynen kalır):

```cpp
			|| !BotCore::CastItemSkillSupported(m->bType[0], m->sSkill, m->iUseItem)
```

(eski: `|| m->iUseItem != 0`). Girinti/satır sırası dosyadaki stile uyar (tab, Allman). Diriltme ve Type8 blokları `iUseItem`'i kendi fonksiyonlarına (`CastResurrectionSupported`, `CastSummonSupported`, `CastWarpSupported`) verir; onlar **değişmez**: bu satır yalnızca o üç bayrağın doğru olmadığı skill'lerde devreye girer. Not: Type8/Type5 + `UseItem` skill'i bayrak yanlışsa `CastItemSkillSupported(8|5, ...)` `false` ⇒ `unsupported_skill` (önceki davranışla aynı).

**b) `no_item` kuralı.** `quest_locked` bloğunun (`:780-789`) hemen **sonrasına**, `bool self = targetName.empty();` satırından önce ekle:

```cpp
	// ADR-0017 Ek F4-36 (docs/03 MEC-MAG-23, U8/U9): a skill with MAGIC.UseItem needs that item AND, for BeforeAction 1..4,
	// the class stone in the caster's own bag (the server asks for both: MagicInstance.cpp:246-260). The bot reads only its
	// own CUser (CanUseItem checks class, level range and existence), so no foreign state is touched; the server still
	// decides (a missing item would end as srv_fail with a wasted CASTING packet).
	if (m->iUseItem != 0
		&& (!user->CanUseItem(m->iUseItem)
			|| !user->CanUseItem(BotCore::CastConsumeItem(m->nBeforeAction, m->iUseItem))))
	{
		out.kind = CastOutcome::REFUSED;
		out.reason = "no_item";
		return out;
	}
```

Diriltme (`Moral` 25) bu satırdan **muaf olmalıdır**: sunucu taşları **ölü hedeften** alır ve çağıranın çantasında olmaları gerekmez (`MagicInstance.cpp:247`); bu yüzden koşulun başına `!resurrection &&` ekle: `if (!resurrection && m->iUseItem != 0 && (...))`. Aksi halde F4-33 diriltmesi `no_item` ile bozulur (çağıranın çantasında zaten 30 taş vardır ama kural yine de muaf tutulur: sunucu kuralıyla birebir). `bad_target` kuralı ve sonrası **değişmez**.

**c)** `TickCast`, `SubmitCast`, `CancelCast`, `RejectCast`, `OnPacket()` ve diğer her şey **değişmez**: eşyalı skill'ler mevcut paket biçimlerini kullanır (tek hedefli; alan `Moral` 10/6 `-1` + hedef noktası; uçan: CASTING → FLYING → EFFECTING; tip kapısı ve damgalar skill'in tiplerine göre). MP guard'ı `mana >= Msp` (uçanda `2 × Msp`) ister.

**d) `ActionExecutor.h` yorumu.** `CastOutcome::reason` yorum listesine `"no_item"` ekle (`"quest_locked"`'ten sonra). `BeginCast` yorumunda `"quest_locked" (...)` maddesinden sonra şunu ekle: `"no_item" (the skill's MAGIC.UseItem, or for MAGIC.BeforeAction 1..4 the class stone 379058000 + n * 1000, is not in the caster's own bag or not usable by its class/level; ADR-0017 Ek F4-36, docs/03 MEC-MAG-23)`; ve `"unsupported_skill"` açıklamasının sonuna şunu ekle: `class skills with MAGIC.UseItem (Type1/3/4, MAGIC.Skill != 0: Impact scrolls, Absolute power, Judgment; ADR-0017 Ek F4-36) are supported when the other rules pass; item-effect magics (MAGIC.Skill == 0) and the dual-typed Type1 + Type3/4 skills (Scream, Shock Stun) stay unsupported`. Yalnızca yorum.

### 5.4 Sonuç sözleşmesi (`docs/03` MEC-MAG-23 ile aynı)

| Skill / durum | Sunucu | Bot sonucu |
|---|---|---|
| **Absolute power** `110802`, `self`, `BotMF_K`, scroll + Stone of Mage çantada | CASTING yok (`CastTime 0`): EFFECTING: `IsAvailable()` eşya (`379065000` + `379061000`) ✔, ağaç `[8] = 20 >= 2` ✔; `ExecuteType4` MP `Msp 240`; `ExecuteSkill` sonrası `ConsumeItem()`: **`RobItem(379061000)` 1 taş** | `effected` (`op 3`), `code` = Type4 süresi (Duration 30 sn ⇒ beklenen `300` `[D]`, ölçülür); MP −240; Stone of Mage −1 (DB okunmaz, `[D]`) |
| Absolute power aynı `BuffType` hedefte zaten varken | `ExecuteType4` aynı `BuffType`'ı reddeder (MEC-MAG-15) | `srv_fail`, MP düşmez; **taş tüketimi `ConsumeItem()`'a bağlı** (ölçülür, §7 S1) |
| **Fire Impact** `110557`, `BotMF_K` → düşman bot (≤ 56 m), canlı | CASTING (`CastTime 15`) → EFFECTING: eşya `379070000` ✔ (`nConsumeItem` = aynı ⇒ `RobItem(0)` **tüketilmez**), ağaç `[5] = 70 >= 57` ✔, MP −220, hasar + DoT | `casting` (`op 1`, `cast_ms` ≈ 1580) → `effected` ya da `missed`; MP −220; scroll çantada kalır |
| **Ice Impact** `110657`, `BotMI_K` → düşman bot | uçan `{3, 4}` (F4-25/26): CASTING → FLYING → EFFECTING; MP 2 × 220 (MEC-MAG-12); ağaç `[6] = 70` ✔ | `casting` → `flying` → `effected` (`code` = Type4 süresi, F4-26) |
| Fire Impact `BotMI_K` (`[5] = 52 < 57`) ya da Thunder Impact (`[7] = 0`) | `IsAvailable()` ağaç ⇒ `fail_return` | CASTING'te `srv_fail` (`op 4`, `code -100`/`-103`: ölçülür), MP düşmez |
| **Judgment** `112802`, `BotPHD_K` → düşman bot (melee menzili) | `CastTime 0`: tek EFFECTING; eşya (`379066000` + `379062000`) ✔, `Range 0` ⇒ silah menzili (F4-03 Type1 yolu), MP −200; `ConsumeItem()` **1 Stone of Priest** | `effected`/`missed`/`srv_fail` (menzil dışı `FAIRNESS_REJECT` `out_of_range`, MEC-MAG-11); MP −200 |
| Çantada gerekli eşya yok (`BotMF_K` Thorn `110554`: `379069000` yok) | — | `REFUSED` `no_item`, paket gitmez, MP değişmez |
| `Skill == 0` eşya büyüsü (`490001` Rice Cake (S)) | — | `REFUSED` `unsupported_skill`, paket gitmez |
| Scream `106802` / Shock Stun `106820` (`{1, 4}`/`{1, 3}`) | — | `REFUSED` `unsupported_skill` (çift tip kapalı) |
| Diriltme `112733` (F4-33, `Moral` 25) | taş ölü hedeften | davranış **değişmez** (`no_item` kuralından muaf) |
| `ENABLED=0` / eşyasız skill'ler | — | davranış değişmez |

`reason` metinleri, `CastOutcome::Kind` değerleri ve `TickCast` akışı bu tablo için **değişmez**; **tek yeni çıktı** `REFUSED` `no_item` metnidir.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (değişen dört dosya için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h`'yi içeren `.cpp`'ler de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastTypes_Supported`, `Combat_WarpCast_Guard` ve `Combat_ItemSkill_Guard` adlarını `[ OK ]` ile içerir ve toplam test sayısı **113**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-36 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş; `git diff ... -- BotCore/BotCombat.h | grep '^-' | grep -v '^---'` boş (yalnızca ekleme).
- [ ] K5: `grep -n "m->iUseItem != 0" GameServer/Bot/ActionExecutor.cpp` tam **iki** satır (diriltme bloğu `if (m->bType[0] == 5 && m->iUseItem != 0)` ve yeni `no_item` koşulu; destek koşulundaki eski satır **yok**); `grep -n "CastItemSkillSupported" GameServer/Bot/ActionExecutor.cpp` tek satır ve destek koşulu içinde; `grep -n "CastConsumeItem" GameServer/Bot/ActionExecutor.cpp` tek satır; `grep -n "\"no_item\"" GameServer/Bot/ActionExecutor.cpp` tek satır; `no_item` koşulunun başında `!resurrection &&` var; `no_item` bloğu `quest_locked` bloğundan sonra, `bool self = targetName.empty();` satırından önce; `CastTypesSupported(m->bType[0], m->bType[1])`, `CastTypeMoralSupported(m->bType[0], m->bMoral)`, `CastMoralSupported(m->bMoral)`, `CastHpCostSupported(m->sHP)` (iki yerde) ve `(m->bFlyingEffect != 0 && !flyingCast)` mevcut `if` içinde yerinde; `grep -c "m_Magictype8Array"` tam 1 (değişmedi).
- [ ] K6: gerilemesiz: `git diff gece/2026-10-02...bot/F4-36 -- GameServer/Bot/ActionExecutor.cpp` yalnızca `BeginCast` içinde hunk içerir (destek koşulu satırı ve `no_item` bloğu; en çok iki hunk); `TickCast`/`SubmitCast`/`CancelCast`/`RejectCast` gövdesinde hunk yok; `git diff ... --stat` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `tools/` değişmemiş.
- [ ] K7: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı türü ve alanı yok (`git diff ... | grep '^+' | grep 'Emit('` boş).
- [ ] K8: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); dört dosya CRLF kalır (ASCII); `git diff --check` boş.
- [ ] K9: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 112 testin tamamı hâlâ geçiyor.
- [ ] K10: `python3 tools/check-perception-contract.py` `RESULT: PASS` (`R1`..`R5` ihlal sayıları 0; denetlenen dosya sayısı değişmez; araç ve istisna listeleri değişmez).
- [ ] K11 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-36
git diff gece/2026-10-02...bot/F4-36 -- BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff gece/2026-10-02...bot/F4-36 -- GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotManager.cpp GameServer/Bot/Telemetry.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj tools
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -rn "CastItemSkillSupported\|CastConsumeItem\|CastTypesSupported" BotCore GameServer Tests
grep -n "m->iUseItem != 0\|\"no_item\"\|CanUseItem" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff --check gece/2026-10-02...bot/F4-36
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; komut başına ≥ 1 sn; dosya saniyede bir okunduğu için "~500 ms" iptal elle yakalanamaz). Botlar (hepsi zone 71; doğuşlar ≥ 3 sn arayla, KI-DEG-01): Karus mage **`BotMF_K`** (Fire Impact, Absolute power, Thorn `no_item`), Karus mage **`BotMI_K`** (Ice Impact; Fire Impact `srv_fail`), Karus priest **`BotPHD_K`** (Judgment), El Morad hedefleri **`BotWP_E`/`BotWG_E`** (düşman; yakında yoksa `move` ile 20-40 m'ye yaklaştır, menzil: Impact ≤ 56 m, Judgment silah menzili), `BotPHB_K` (F4-33 gerilemesi için ölü hedef gerekirse). Önce `list` ile konum, HP/MP ve ölü durumunu denetle: **ölü bot başlangıç ölçümüne alınmaz** (`db/002` idempotent yeniden uygulanır ya da bot `regene` ile diriltilir). MP/HP/konum `list`'ten, olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'tan; MP'yi cast'ten hemen önce ve sonra `list` ile al, kümeli +40 yenileme payını raporla. **Taş tüketimi:** `USERDATA` satırları okunmaz (`CLAUDE.md` DB kuralı); Stone of Mage/Priest tüketimi koddan `[D]` kalır, ancak çalışma zamanında **dolaylı** olarak şöyle sınanabilir: koşu sırasında sunucu günlüğünde ya da `Bot_*.log`'da `RobItem` hatası olmaması, `effected`'ın `ConsumeItem()` yolundan geçtiğini kanıtlar (`ConsumeItem` yalnızca başarılı `ExecuteSkill` sonrası çalışır). Bu ölçüm botların taşlarını azaltır (50 → ~45): rapora yaz, `db/002` yeniden uygulaması gerekmez.

1. **S1 Absolute power (`110802`, tek tipli Type4 + eşya):** `spawn BotMF_K`; `cast BotMF_K 110802 self 1` ⇒ `ACTION_SUBMIT` `CastEffect` (`CastTime 0` olduğundan `casting` aşaması yok), `effected` `op 3`, `code` ≈ süre (Duration 30 ⇒ `300` beklenir, farklıysa raporla), MP **−240**, log `cast finished (effected) after 1 cycle(s)`. Hemen tekrar `cast BotMF_K 110802 self 1` (aynı `BuffType` etkin, `ReCastTime 250` ⇒ guard `recast` ile ≥ 25 sn bekler): bekleme sonrası sonuç (`effected` ya da `srv_fail`) ve MP'yi raporla (MEC-MAG-15: aynı `BuffType` ⇒ `srv_fail`, MP düşmez; taşın tüketilip tüketilmediği `[D]`).
2. **S2 Impact (scroll tüketilmez):** `spawn BotMF_K,BotWP_E`; hedef ≤ 56 m: `cast BotMF_K 110557 BotWP_E 1` ⇒ `casting` (`op 1`, `cast_ms` ≈ 1580) → `effected` ya da `missed` (hasar `DAMAGE` olayı), MP **−220**; hemen ardından (20,3 sn `ReCastTime` bitince) ikinci atış `ReCastTime`'tan sonra yine `casting` → `effected`: **scroll tüketilmiyor** (ikinci atış `no_item` değil; çantada 1 adet olan `379070000` tüketilseydi ikincisi `no_item` olurdu). `spawn BotMI_K`: `cast BotMI_K 110657 BotWP_E 1` ⇒ `casting` → `flying` → `effected` (`code` = Type4 süresi); MP −440.
3. **S3 Judgment (`112802`, Type1 + eşya):** `spawn BotPHD_K`; düşman bot silah menzilinde: `cast BotPHD_K 112802 BotWP_E 1` ⇒ `effected` ya da `missed`, MP **−200**; menzil dışıysa `FAIRNESS_REJECT` `out_of_range` (MEC-MAG-11; doğru red, bulgu değil; yaklaştırıp yeniden dene). Hasar `Hit 500` büyüklüğünde olmalı (`DAMAGE` olayı, fizik hasar modeli ±15 %, `docs/04`).
4. **S4 Sunucu ağaç/eşya reddi:** `cast BotMI_K 110557 BotWP_E 1` (Fire Impact, `[5] = 52 < 57`) ⇒ CASTING'te `srv_fail`, MP değişmez; `cast BotMF_K 110757 BotWP_E 1` (Thunder Impact, `[7] = 0`) ⇒ CASTING'te `srv_fail`, MP değişmez. (Bu ikisi **bot kuralını geçer**: eşya çantada, `no_item` değil.)
5. **S5 Bot kuralları:** `cast BotMF_K 110554 BotWP_E 1` (Fire Thorn, `379069000` çantada yok) ⇒ `refused (no_item)`, paket gitmez (`ACTION_SUBMIT` 0), MP değişmez; `cast BotMF_K 110535 BotWP_E 1` (Spell of blast, `370001000` yok) ⇒ `refused (no_item)`; `cast BotMF_K 490001 self 1` (Rice Cake, `Skill 0`) ⇒ `refused (unsupported_skill)`; `cast BotWG_K 106802 BotWP_E 1` (Scream, `{1, 4}`) ⇒ `refused (unsupported_skill)`; `cast BotWG_K 106820 BotWP_E 1` (Shock Stun, `{1, 3}`) ⇒ `refused (unsupported_skill)`; hepsinde paket gitmez.
6. **S6 Gerilemesiz:** F4-33 `Moral` 25 `no_item`'den muaf: `BotWP_K` öldür (ya da ölü bot), `cast BotPHD_K 112733 BotWP_K 1` ⇒ `BeginCast`'ten geçer (`casting`, `refused` değil; sonuç F4-33 notuna göre); F4-35 `cast BotMF_K 110015 self 1` `effected`; F4-34 summon; F4-32 `cast BotPHD_K 112525 self 1`; F4-28 `cast BotPHB_K 112603 self 1`; F4-29 Inferno `cast BotMF_K 110545 BotWP_E 1` `target -1`. Eşyasız skill'lerde davranış aynı.
7. **S7 Ayarlar ve temizlik:** `TELEMETRY=summary`: bir Impact atışı çalışır, JSONL'de `ACTION_*` yok; `ENABLED=0` ⇒ komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde; sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Temizlik: ini yedekten geri, geçici betikler silindi, botlar despawn, sunucular `stop`.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `BotCore/` ve `Bot/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **`MAGIC.UseItem != 0` + `Skill != 0` + Type1/3/4** skill'i serisinde davranış değiştirir (önceden `unsupported_skill` idi); eşyasız skill'lerde `BeginCast` davranışı (paket biçimi, telemetri satırı, `reason` değerleri) değişmez. Diriltme (F4-33), summon/warp (F4-34/35) ve cure yolları değişmez.
- **Thread kuralı (ADR-0005):** `BeginCast` bot tick'inde (IOCP thread'i) çalışır; `CUser::CanUseItem` çağıranın kendi `m_sItemArray`'ini okur (DB yükleme girişte bir kez; çanta değişimi `RobItem` ile aynı thread'de). Yeni durum/kilit yok.
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `effected` yalnızca `MAGIC_EFFECTING`'ten, `srv_fail` yalnızca `MAGIC_FAIL`'den. Hedefin durumu, aynı `BuffType` etkisi, ağaç puanı (KI-016), taş sayısı **bota önkontrol olarak eklenmez**. `no_item` yalnızca botun **kendi çantası** (kendi `CUser`'ı) için "≥ 1 adet" kuralıdır; sunucunun kendi eşya kuralını yansıtır (pot yolundaki `BeginPotion` gibi), bu yüzden `FAIRNESS_REJECT` değil ön kontrol (`REFUSED`) olarak yazılır ve `ACTION_*` olayı üretmez.
- **Bilinen sınırlar `[A]`/`[D]`:** (a) Çantada tek yığında ≥ 1 adet yeterlidir; taşlar 50'lik yığındır ve her Absolute power/Judgment atışında 1 azalır, bitince `no_item` döner (stok izleme ve yeniden stoklama karar/envanter dilimlerinin işidir; ölçüm botları 50 → ~45'e düşürür). (b) Scroll'lar (379063/64/65/66/69/70) `ConsumeItem()` listesinde olduğu için **tüketilmez**: çantada 1 adet olması süresiz yeter. (c) `no_item` kuralı sunucunun dönüşüm (`isTransformed()`) istisnasını da `CanUseItem` ile birebir uygular. (d) Ağaç yetersizliği (KI-016: `BotMI_*` Fire Impact, hiçbir botta Thunder Impact) CASTING'te `srv_fail` verir ve MP düşmez; bu hata değildir. (e) Warrior'ın Scream/Shock Stun/Exceed Break skill'leri bu planla **açılmaz** (çift tip, 6f); `docs/06` satır 154 (Stone of Warrior bitti) buna bağlıdır. (f) Item'li alan/party skill'leri (Minor Resist `Moral` 10, Counter Curse `Moral` 6) veri gereği açılır ama ölçülmez; çalışma zamanında ilk kullanıldıklarında kendi dilim notlarındaki (F4-29, F4-31) sonuç sözleşmesine uyarlar.
- Telemetri hacmi değişmez; `tools/bot-telemetry-report.py` değişmez (`REFUSED` sonuçları ayrı sayılır, MET-ACT-02).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-36` (taban `gece/2026-10-02` @ `8b31e52`); `09449aa` — "[F4-36] Eşya tüketen sınıf skill'leri: CastItemSkillSupported + no_item ön kontrolü"
- Değişen dosyalar ve neden:
  - `BotCore/BotCombat.h`: §5.1 bloğu `CastWarpNeedsOtherTarget`'tan sonra, `CastTargetIdField` öncesine eklendi (sabitler `kClassStoneBase`/`kClassStoneStep`/`kClassWarriorId`/`kClassPriestId`, `CastItemSkillSupported`, `CastConsumeItem`). Yalnızca ekleme (33 satır), BOM/CRLF korundu.
  - `Tests/BotCoreTests/CombatTests.cpp`: dosya sonuna `Combat_ItemSkill_Guard` (46 satır). Mevcut testler değişmedi.
  - `GameServer/Bot/ActionExecutor.cpp`: `BeginCast` destek koşulundaki `|| m->iUseItem != 0` yerine `|| !BotCore::CastItemSkillSupported(m->bType[0], m->sSkill, m->iUseItem)` (satır 771); `quest_locked` bloğundan sonra `no_item` ön kontrolü (satır 792-802). İki hunk, ikisi de `BeginCast` içinde.
  - `GameServer/Bot/ActionExecutor.h`: `CastOutcome::reason` listesine `"no_item"`; `BeginCast` yorumuna eşyalı sınıf skill'leri ve `no_item` açıklaması. Yalnızca yorum.
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  - `proj-GameServer.vcxproj -> ...\x86-Release\Server\GameServer.exe`
  - `BotCoreTests.vcxproj -> ...\x86-Release\Tests\BotCoreTests.exe`
  - Debug de hatasız (`BotCoreTests.vcxproj -> ...\x86-Debug\Tests\BotCoreTests.exe`). Değişen dört dosyada yeni uyarı yok (iki C4789 `UpgradeHandler.cpp`'de, önceden var).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ Release rc=0; değişen dosyalarda uyarı yok.
  - K2 ✔ Debug rc=0.
  - K3 ✔ `tools/run-tests.sh Release` ve `Debug`: `248 tests, 0 failed`; `Combat_CastTypes_Supported`, `Combat_WarpCast_Guard`, `Combat_ItemSkill_Guard` üçü de `[ OK ]`. **Not:** plan 113 test bekliyordu; gerçek toplam **248** (bu çalışma ağacında nav/F8 planlarının testleri de mevcut; F4-36 öncesi 247 → sonrası 248, +1). Sayı plan metnindeki 112→113 tabanına göre değil, güncel ağaca göre doğru.
  - K4 ✔ `grep` K4 desenleri eşleşme vermedi; `std::min`/`std::max` yok; `BotCombat.h` diff'inde silinen satır yok.
  - K5 ✔ `m->iUseItem != 0` tam iki satır (737 diriltme, 794 `no_item`); `CastItemSkillSupported` tek satır (771, destek koşulu içinde); `CastConsumeItem` tek satır (796); `"no_item"` tek satır (799); `no_item` başında `!resurrection &&`; blok `quest_locked`'ten sonra `bool self = ...`'dan önce; diğer koşul terimleri yerinde; `m_Magictype8Array` 1.
  - K6 ✔ `ActionExecutor.cpp` diff'i yalnızca `BeginCast` içinde iki hunk; `git diff --stat` yalnızca §4'teki 4 dosya + plan; proje dosyaları/`BotSession`/`BotManager`/`Telemetry`/`tools` değişmedi.
  - K7 ✔ yeni ini/komut/thread/telemetri yok; diff'te `Emit(` eklenmedi.
  - K8 ✔ dört dosya CRLF + ASCII (`file` durumu aynı); `git diff --check` boş.
  - K9 ✔ guard grep'leri beklenen sayılarda; 112 önceki testin tamamı geçiyor.
  - K10 ✔ `check-perception-contract.py` `RESULT: PASS` (`files scanned: 31`, R1/R4/R5 = 0), `--selftest` `selftest OK`.
  - K11 (çalışma zamanı) Claude'a bırakıldı.
- Plandan sapmalar ve gerekçeleri:
  - Test sayısı plandaki "112 → 113" değil, güncel ağaçta "247 → 248". Bunun nedeni tabanın (veya bu çalışma ağacının) nav/F8 testlerini de içermesidir; F4-36 kendi testinde +1 ekler. Yeni test adı ve içeriği plan §5.2 ile birebir.
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — —

- Karar: —
