# F4-33: `ActionExecutor` diriltme dilimi — Resurrection of love/grace/favors (`Moral` 25, Stone of Life)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-33` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-32 (Type5 yolu, `BeginCast` destek koşulu, MEC-MAG-19) — `KAPANDI` (merge `8590e33`); F4-07 (`Regene`, ölüm izleme `m_deadSeen`) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-11, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-19, MEC-MAG-20 (bu planla eklendi, `[D]`), SK-06, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 Ek 1 madde 1(b) (diriltme), Ek 8; `docs/17` §2.1 "Priest diriltme", T-PRI-08 altyapısı |
| Tahmini büyüklük | S (4 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.cpp`, `ActionExecutor.h`; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

P-HD priest'inin ölen takım arkadaşını diriltme işi botla yapılamıyor ve F7 (`docs/07` §4 öncelik sırası madde 6 "diriltme", §10 diriltme koşulları, §13 `RESCUE` alt durumu) bunsuz yazılamaz. **Resurrection of love** `112733`, **grace** `112742`, **favors** `112754` (ve El Morad karşılıkları `212733/212742/212754`) `MAGIC.Type1 = 5`, `Moral` 25 (`MORAL_CORPSE_FRIEND`) ve `UseItem = 379006000` (Stone of Life) skill'leridir. `BeginCast` bunları `m->iUseItem != 0` ve `CastMoralSupported(25) == false` yüzünden `unsupported_skill` ile reddediyor (`ActionExecutor.cpp:734-740`).

Sunucu davranışı F4-32'nin `Moral` 2 yolundan **iki noktada farklıdır** ve plan bunu açıkça kapsar:

1. **Hedef ceset olmalıdır:** `Moral` 25 hedefin ölü, çağırandan farklı ve dost millet olmasını ister (`IsAvailable()` `MORAL_CORPSE_FRIEND`). Bot kendine (`self`) diriltme atamaz (`bad_target`).
2. **Taşlar çağıranın değil, ölü HEDEFİN çantasından alınır:** `UserCanCast()` `CanUseItem(iUseItem, sNeedStone)` kontrolünü **hedefe** yapar (`MagicInstance.cpp:247`); `ExecuteType5` `RESURRECTION` dalı taşları hedeften `RobItem` ile alır ve çağırana `(NeedStone / 2) + 1` taş verir (`:2033-2041`). Botların çantasında `db/002` ile 30 adet Stone of Life vardır (`db/002_bot_characters.sql:207`, slot 17). Dolayısıyla bu dilim **envanter doldurma gerektirmez**; ADR-0018 Ek 8'in "taş stoğu ön koşulu" bot satırlarında zaten sağlanmıştır (dilim 8 envanter doldurma yalnızca taşlar tükendikçe gerekir). Not: `docs/17` §2.1'in "kendi taş stoğu" ifadesi yanlıştı, bu planla düzeltilir.

Bu plan:

1. `BotCore/BotCombat.h`'a `CastResurrectionSupported` ve `CastNeedsOtherTarget` saf mantık fonksiyonlarını ekler;
2. `BeginCast`'e diriltme istisnasını ekler (MAGIC_TYPE5 tablosundan `bType == RESURRECTION` doğrulanarak; `Stone of life` öğe-skill'i `480001` ve benzerleri kapalı kalır) ve `bad_target` kuralını `Moral` 25'i kapsayacak şekilde genişletir;
3. sunucu davranışını kodla belgeler ve çalışma zamanında doğrular (`docs/03` MEC-MAG-20).

F4'ün otuz üçüncü planıdır (ADR-0018 sırası: ... Type5 cure ✔ → **diriltme (bu plan, dilim 6b)** → summon + güvenlik kapıları (6c) → Type8 warp/descent/Gate (6d) → `UseItem`'li skill'ler (6e) → CLI-12 → envanter doldurma).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-33)" (bu planla birlikte yazıldı), "Ek (F4-32)". `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (Ek 1 madde 1, Ek 8, Ek 9).
- `docs/03` §4.2 **MEC-MAG-19** (Type5 cure), **MEC-MAG-20** (bu planla eklendi); `docs/05` priest tablosu (satır 128: Resurrection of love/grace/favors, "taş 4/10/30 **hedeften**"); `docs/07` §10 (diriltme koşulları: ceset ≤ 11 m, MP ≥ 800, ölen botun taş stoğu; akış, taş durumuna göre seçim).
- `plans/F4-32-aksiyon-yurutucu-type5-cure.md` (aynı fonksiyonlar; **yazılı planı değil, kodu esas al**).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `8590e33` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.cpp:868-878` `IsAvailable()` `MORAL_CORPSE_FRIEND`: hedef `nullptr`, çağıranla düşman, çağıranın kendisi ya da **hayatta** ise `fail_return` (CASTING ve EFFECTING'te). `:166`: ölü çağıran yalnızca `bType[0] == 5` ise skill atabilir (bot ölüyken `BeginCast` zaten `dead` ile reddeder; değişmez).
  - `GameServer/MagicInstance.cpp:229-262` `UserCanCast()`: `bType[0] == 5` ise `m_Magictype5Array` kaydı okunur (`bType`, `sNeedStone`); `iUseItem != 0` ise `TO_USER(bType == RESURRECTION ? pSkillTarget : pSkillCaster)->CanUseItem(iUseItem, sNeedStone)` başarısızsa `SkillUseFail` ⇒ CASTING'te `MAGIC_FAIL`. İkinci kontrol (`:259`, `CanUseItem(nConsumeItem)` çağıran için) `bType != RESURRECTION` ile atlanır. `CanUseItem` (`User.cpp:5121-5145`) eşyanın sınıf/seviye şartlarına (Stone of Life: sınıf 0, `ReqLevel 1..99`) ve `CheckExistItem(id, adet)`'e bakar.
  - `GameServer/MagicInstance.cpp:1898-2063` `ExecuteType5()`: tek hedef dalı (`:1936-1952`): hedef `nullptr`/oyuncu değilse `false`; **hedef diriltme dışı skill'de ölüyse** `false`; **diriltme skill'inde hedef hayattaysa** `false` (yayın yok). `case RESURRECTION` (`:2033-2041`): `pTUser->CheckExistItem(iUseItem, sNeedStone)` ve `RobItem(iUseItem, sNeedStone)` başarılıysa çağırana `GiveItem(iUseItem, sNeedStone / 2 + 1)` ve `pTUser->Regene(1, nSkillID)`; taş yoksa sessizce hiçbir şey olmaz. Sonda (`bType[1] == 0` ⇒ koşulsuz) çağırana hedef kimlikli EFFECTING yayını (`:2054-2059`): taş eksik olsa bile gider.
  - `GameServer/AttackHandler.cpp:100-` `CUser::Regene(1, magicid)`: büyülü yeniden doğuş dalı (`magicid != 0`): `MSpChange(-m_iMaxMp)` ⇒ **hedefin MP'si 0'a iner**; `m_sWhoKilledMe == -1` ise `ExpChange(m_iLostExp * bExpRecover / 100)`; konum **değişmez** (ölen yerde dirilir); hedefe `WIZ_REGENE` gider, `HpChange(GetMaxHealth())` ile tam HP, `InitType4()` (buff'lar sıfırlanır). Hedef bot `BotSession::OnPacket()` bu `WIZ_REGENE`'yi `m_regeneEcho`'ya yazar (`BotSession.cpp:153-158`); `ActionExecutor::RequestRegene` bu alanı kendi isteğinden önce sıfırladığı için etkilenmez. Ölüm izleme `TickSessions()`'ta (`BotManager.cpp:3026-3039`) hedef dirilince `m_deadSeen = false` yapar.
  - `GameServer/MagicInstance.cpp:1028`: `bType[0] != 4` iken `Msp` EFFECTING'te bir kez düşer (Type5, F4-32 ile aynı). `:2983-3000` `ConsumeItem()`: başarılı EFFECTING'in sonunda çağıranın çantasından **ek** 1 adet `iUseItem` almaya çalışır (`bType[0] != 2`, `:132`); çağıranda taş yoksa sessizce atlanır. Bu davranış `[D]`'dir: planın çalışma zamanı ölçümü çağıranın taş sayısını okuyamaz (bkz. §8).
  - `GameServer/MagicInstance.cpp:389-405`: tip kapısı Type 1..7 için ortaktır (Type5 dahil, F4-32'de doğrulandı; `IsGatedType` değişmez). `:955-960`: skill ağacı denetimi `m_bstrSkill[sSkill % 10] >= SkillLevel` (KI-016).
  - `GameServer/MagicInstance.h:55-59`: `REMOVE_TYPE3 1`, `REMOVE_TYPE4 2`, `RESURRECTION 3`, `RESURRECTION_SELF 4`, `REMOVE_BLESS 5`; `MORAL_CORPSE_FRIEND = 25` (`:47`).
  - `shared/database/structs.h:102-108` `_MAGIC_TYPE5 { uint32 iNum; uint8 bType; uint8 bExpRecover; uint16 sNeedStone; }`; sunucuda `g_pMain->m_Magictype5Array.GetData(id)` (`MagicInstance.cpp:236`). `ActionExecutor.cpp` aynı kalıpla `m_Magictype3Array` okur (`:1366`, `:1408`): oyun verisi tablosu, oyuncu durumu değildir; `tools/check-perception-contract.py` R1/R2 listelerinde değildir.
  - `GameServer/Bot/ActionExecutor.cpp` (`8590e33`): `BeginCast` destek koşulu `:733-740` (`flyingCast` hesabı ve altı `if`); `bad_target` kuralı `:756-763` (`wantedSelf`, `wantedTarget = (m->bMoral == MORAL_ENEMY)`); `TickCast` hedef paketi `CastTargetIdField(area, target.id)` ve `CastCoordField(area, isSelf, ...)` (tek hedefli yol: hedef kimliği ve hedef x/y/z metre) — **değişmez**. `BotManager.cpp:3170-3200` `TickSessions()` cast hedef görünümü hedef botun oturumundan alınır ve **hedefin ölü olmasını sormaz**; ölü hedef konumu normal okunur (`CommandCast` `:1480-1520` de hedef canlılığı denetlemez).
  - `BotCore/BotCombat.h:322-336` (`CastTypesSupported`, `CastTypeMoralSupported`), `:369` (`CastMoralSupported`), `:381` (`CastHpCostSupported`); `Tests/BotCoreTests/CombatTests.cpp` son test `Combat_CureCast_Guard` (`:1363-1431`); `Combat_CastTypes_Supported` ve `Combat_CureCast_Guard` içindeki `CastTypeMoralSupported(5, 25) == false` ve `CastMoralSupported(25) == false` satırları **değişmez** (diriltme bu iki fonksiyonu kullanmaz, `BeginCast`'te ayrı istisnadır). Toplam birim test **109**.
- **Veri notu (Claude yerel `MAGIC`/`MAGIC_TYPE5`/`ITEM` tablolarında doğruladı; oyun verisidir, kişisel veri değil).**
  - `Moral 25`, `Type1 5`, `Type2 0`, `UseItem 379006000`, `FlyingEffect 0`, `HP 0`, `Etc 0`, `BeforeAction 0`, `UseStanding 0`, `Range 11`, `CastTime 15`, `ReCastTime 250` (**25 sn**), `Skill 1127`: `112733` love (`SkillLevel 33`, `Msp 400`, `MAGIC_TYPE5.Type 3`, `ExpRecover 10`, `NeedStone 4`); `112742` grace (`SkillLevel 42`, `Msp 600`, `ExpRecover 10`, `NeedStone 10`); `112754` favors (`SkillLevel 54`, `Msp 800`, `ExpRecover 10`, `NeedStone 30`). El Morad `212733/212742/212754`: aynı değerler (`Skill 2127`; `211733` satırında `ExpRecover 60`, bot sınıfı değil). Stone of Life `379006000`: `Kind 97`, `Class 0`, `ReqLevel 1`, `ReqLevelMax 99`, `Countable 1`.
  - Aynı sorgu `Moral 25/26` ve `MAGIC_TYPE5.Type 3/4/5` ile şunları da döndürür ve **açılmayacaktır**: `111733/111742/111754`, `211733/211742/211754` (sınıf 111/211: botlarda yoktur, `BeginCast` `bad_skill` verir), `480001` "Stone of life" (`Moral 25`, `UseItem 379006000`, **`MAGIC_TYPE5.Type 4` = `RESURRECTION_SELF`**, `sSkill 0`: öğe skill'i; ölü çağıran kendi kendini diriltir, bota kapalı), `480002..480005` (`Moral 1`, scroll'lar), `300133` Deruvish Cancel Magic (`Type5.Type 0`, `Moral 7`, NPC skill'i).
- **Bot karakter notu (`db/002_bot_characters.sql:128-139`; `strSkill` ilk 10 bayt, `m_bstrSkill[sSkill % 10]`).** `BotPHD_K`/`BotPHD_E` (`0x00000000003C003E1400`): `m_bstrSkill[5] = 60` (1125 iyileştirme), `[7] = 62` (**1127 lanet ağacı**) ⇒ üç diriltme de atılabilir (`SkillLevel` 33/42/54 ≤ 62). `BotPHB_K`/`BotPHB_E` (`0x00000000003C3E001400`): `[7] = 0` ⇒ diriltme ağaç yetersizliğiyle **CASTING'te `srv_fail`** (KI-016, `docs/07` "P-HB'de diriltme yoktur"). Warrior/mage botları başka sınıf (`112xxx` skill'i `bad_skill`). Her botun çantasında 30 Stone of Life vardır (`db/002:207`, slot 17); tüm bot satırları aynı çanta şablonunu kullanır.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `kMoralCorpseFriend`, `kType5Resurrection`, `kResurrectionStoneItem` sabitleri; `CastResurrectionSupported(type0, type1, moral, useItem, type5Kind)` ve `CastNeedsOtherTarget(moral)` (§5.1).
2. **`BeginCast` (`ActionExecutor.cpp`):** (a) diriltme istisnası: `bType[0] == 5 && iUseItem != 0` iken `m_Magictype5Array` kaydı okunur, `CastResurrectionSupported(...)` + `bFlyingEffect == 0` + `CastHpCostSupported(sHP)` sağlanırsa destek koşulu atlanır (diğer tüm skill'ler için koşul **aynen** kalır); (b) `wantedTarget = BotCore::CastNeedsOtherTarget(m->bMoral)` (§5.3).
3. **Yorumlar (`ActionExecutor.h`):** `BeginCast` açıklaması diriltmeyi kapsar (§5.3).
4. **Birim testleri:** yeni `Combat_ResurrectionCast_Guard` (109 → 110).
5. **Sonuç sözleşmesi (§5.4):** `docs/03` MEC-MAG-20 çalışma zamanında sınanır.

**Kapsam dışı (yapılmayacak)**

- `Moral` 6 Bless of God, `Moral` 26 (`CORPSE_ENEMY`), `RESURRECTION_SELF` (`480001`), `REMOVE_BLESS`, `{5, x}` çiftleri, Type8, başka `UseItem`'li skill'ler (sınıf taşları `BeforeAction`, scroll'lar): ayrı alt dilimler (ADR-0018 Ek 1 madde 1).
- **Karar katmanı:** kimin öldüğü, hangi diriltmenin (love/grace/favors) seçileceği (hedefin taş stoğuna göre!), ne zaman diriltileceği, güvenli mi (`RESCUE`) ve `ResIntent`/`RESPAWN_HOLD` akışı (F7, `docs/07` §10). Bota **hedefin ölü olup olmadığı, hedefin yeterli taşı olup olmadığı önkontrolü eklenmez** (AC-LRN-03: hedefin çantası ve ölüm durumu başka oyuncunun/botun durumudur; `WIZ_DEAD` gözlemi ve taş stoğu karar katmanının işidir). Bunlar sunucuda CASTING'te `srv_fail` olarak görünür.
- Diriltilen botun sonraki davranışı (buff'sız kalır, MP 0), exp geri kazanımı ölçümü.
- Envanter doldurma / taş yeniden stoklama (dilim 8). `db/002`'yi yeniden çalıştırmak çalışma zamanı ölçümlerinden sonra taşları sıfırlar (§7 notu); plan DB'yi değiştirmez.
- Yeni komut/ini anahtarı/telemetri olayı türü/alan, `BotSession.*`/`BotManager.cpp` değişikliği, `tools/` betik değişikliği (özellikle `check-perception-contract.py` istisna listeleri).
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | yeni sabitler + `CastResurrectionSupported` + `CastNeedsOtherTarget` (mevcut fonksiyonlar değişmez) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | yeni `Combat_ResurrectionCast_Guard` dosyanın sonuna (109 → 110); mevcut testler değişmez |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yalnızca `BeginCast`: diriltme istisnası (destek koşulu çevresi) ve `wantedTarget` satırı |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca `BeginCast` yorumu |

`BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `ScenarioRunner.*`, `ScriptRunner.*`, `tools/` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`CastHpCostSupported`'tan (`:381`) hemen sonra, `CastTargetIdField` öncesine şu bloğu ekle (ASCII, CRLF, tab, `namespace BotCore` içinde; yeni `#include` yok; **`std::min`/`std::max` kullanma**):

```cpp
	// --- resurrection cast (ADR-0017 Ek F4-33, docs/03 MEC-MAG-20) ---

	// MAGIC.Moral 25 = CORPSE_FRIEND (MagicInstance.h): the target must be a dead player of the caster's nation other than
	// the caster. The server takes MAGIC_TYPE5.NeedStone stones of MAGIC.UseItem from the DEAD TARGET (not the caster)
	// and resurrects it in place. MAGIC_TYPE5.Type 3 = RESURRECTION (MagicInstance.h); Type 4 = RESURRECTION_SELF (the
	// item skill 480001) stays closed.
	constexpr uint8_t kMoralCorpseFriend = 25;
	constexpr uint8_t kType5Resurrection = 3;
	constexpr uint32_t kResurrectionStoneItem = 379006000;

	// The resurrections the bot casts: Type5 alone, Moral 25, the Stone of Life, MAGIC_TYPE5.Type RESURRECTION. The caller
	// still rejects flying effects and "sacrifice" HP costs.
	inline bool CastResurrectionSupported(uint8_t type0, uint8_t type1, uint8_t moral, uint32_t useItem, uint8_t type5Kind)
	{
		return type0 == 5 && type1 == 0 && moral == kMoralCorpseFriend
			&& useItem == kResurrectionStoneItem && type5Kind == kType5Resurrection;
	}

	// Morals that need a named target other than the caster: 7 enemy (F4-03) and 25 corpse-friend (F4-33).
	inline bool CastNeedsOtherTarget(uint8_t moral)
	{
		return moral == 7 || moral == kMoralCorpseFriend;
	}
```

Dosyanın kalanı **değişmez** (`CastTypesSupported`, `CastTypeMoralSupported`, `CastMoralSupported` aynen: diriltme bunlardan geçmez, `BeginCast`'te ayrı istisnadır). `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

Yeni **`Combat_ResurrectionCast_Guard`** (dosyanın sonuna, `Combat_CureCast_Guard`'dan sonra; Resurrection of love `112733`: `Msp 400`, `CastTime 15`, `ReCastTime 250`, `Range 11`):

- Destek kapısı: `CastResurrectionSupported(5, 0, 25, 379006000, 3) == true`; `false` olanlar (her biri tek fark): `(3, 0, 25, 379006000, 3)`, `(5, 4, 25, 379006000, 3)`, `(5, 0, 2, 379006000, 3)`, `(5, 0, 6, 379006000, 3)`, `(5, 0, 26, 379006000, 3)`, `(5, 0, 25, 0, 3)`, `(5, 0, 25, 379005000, 3)`, `(5, 0, 25, 379006000, 4)` (RESURRECTION_SELF, `480001`), `(5, 0, 25, 379006000, 2)`, `(5, 0, 25, 379006000, 0)`.
- Mevcut fonksiyonlar kapalı kalır (diriltme onlardan geçmez): `CastTypeMoralSupported(5, 25) == false`, `CastMoralSupported(25) == false`, `CastTypesSupported(5, 0) == true`.
- Hedef kuralı: `CastNeedsOtherTarget(7) == true`, `(25) == true`; `(1)`, `(2)`, `(4)`, `(6)`, `(8)`, `(10)` hepsi `false`.
- Paket alanları (tek hedefli yol): `SendsAimPoint(25) == false`; `CastTargetIdField(SendsAimPoint(25), 2986) == 2986`; `CastCoordField(SendsAimPoint(25), false, 12.7f) == 12`.
- Tip kapısı Type5'i kapsar: `IsGatedType(5) == true` (F4-32 testinde de var; burada tek satır).
- Başlangıç guard'ı (`CastStartCheck c = {}`; alanlar `Combat_CureCast_Guard`'daki gibi, `c.skillRange = 11`, `c.msp = 400`, `c.reCastMs = BotCore::CastRecastMs(250)`, `c.typeGated = true`, `c.mana = 400`, `c.distanceM = 5.0f`, `c.standing = true`, `c.needsStanding = false`, `c.actionsInWindow = 0`): `CastRecastMs(250) == 25000`; `CheckCastStart(c) == CAST_OK`; `c.mana = 399` ⇒ `CAST_REJECT_NO_MANA`; `c.mana = 400; c.distanceM = 10.9f` ⇒ `CAST_OK`; `c.distanceM = 11.0f` ⇒ `CAST_REJECT_OUT_OF_RANGE`; `c.distanceM = 5.0f; c.hasSkillLast = true; c.sinceSkillLastMs = 24999` ⇒ `CAST_REJECT_RECAST`; `c.sinceSkillLastMs = 25000` ⇒ `CAST_OK`.
- Enum sabit adları `BotCombat.h`'dekiyle birebir (`CAST_OK`, `CAST_REJECT_NO_MANA`, `CAST_REJECT_OUT_OF_RANGE`, `CAST_REJECT_RECAST`); farklıysa dosyadaki adı kullan. Test sayısı **110**.

### 5.3 `ActionExecutor.cpp` ve `ActionExecutor.h`

**a) `BeginCast` destek koşulu (`ActionExecutor.cpp:733-740`).** Yalnızca bu bloğu değiştir; `flyingCast` satırından önce diriltme bayrağını hesapla ve mevcut `if`'in başına ekle (mevcut altı koşul **olduğu gibi** kalır):

```cpp
	// ADR-0017 Ek F4-33: a resurrection (Type5 + Moral 25 + the Stone of Life, MAGIC_TYPE5.Type RESURRECTION) is a supported
	// skill although MAGIC.UseItem != 0 and Moral 25 is not in CastMoralSupported. Static game data only: the caller's
	// class/level/quest checks above and below still apply.
	bool resurrection = false;
	if (m->bType[0] == 5 && m->iUseItem != 0)
	{
		_MAGIC_TYPE5 * t5 = g_pMain->m_Magictype5Array.GetData(skillId);
		resurrection = t5 != nullptr
			&& BotCore::CastResurrectionSupported(m->bType[0], m->bType[1], m->bMoral, m->iUseItem, t5->bType)
			&& m->bFlyingEffect == 0
			&& BotCore::CastHpCostSupported(m->sHP);
	}

	bool flyingCast = BotCore::IsFlyingCast(m->bType[0], m->bFlyingEffect);
	if (!resurrection
		&& (!BotCore::CastTypesSupported(m->bType[0], m->bType[1])
			|| !BotCore::CastTypeMoralSupported(m->bType[0], m->bMoral)
			|| (m->bFlyingEffect != 0 && !flyingCast)
			|| m->iUseItem != 0
			|| !BotCore::CastMoralSupported(m->bMoral)
			|| !BotCore::CastHpCostSupported(m->sHP)))
```

`{ out.kind = REFUSED; out.reason = "unsupported_skill"; return out; }` gövdesi aynen kalır. Girinti/parantez biçimini dosyadaki stile uydur (tab, Allman).

**b) `bad_target` (`:756-759`).** Yalnızca `wantedTarget` satırı:

```cpp
	bool wantedTarget = BotCore::CastNeedsOtherTarget(m->bMoral);
```

(Önceki hâl: `bool wantedTarget = (m->bMoral == MORAL_ENEMY);`, `Moral` 7 için davranış aynıdır.) Böylece `Moral` 25'te `self` (boş hedef adı) `bad_target` ile reddedilir; paket gitmez. `wantedSelf` ve `if` satırı aynen kalır.

**c)** `TickCast`, `SubmitCast`, `CancelCast`, `RejectCast`, `OnPacket()` ve diğer her şey **değişmez**: diriltme tek hedefli yoldur (`area` `false`, hedef kimliği = ölü botun kimliği, koordinat hedefin konumu metre), tip kapısı (`typeGated`, `typeStamps`) Type5'i zaten kapsar, MP guard'ı `mana >= Msp` ister (400/600/800).

**d) `ActionExecutor.h` yorumu.** `BeginCast` yorumundaki destek listesine ekle: "resurrection (Type5, Moral 25, the Stone of Life, MAGIC_TYPE5.Type RESURRECTION: Resurrection of love/grace/favors; the target must be a dead friendly bot other than the caster, the server takes the stones from the dead target; Bless of God, RESURRECTION_SELF and other item skills stay unsupported; ADR-0017 Ek F4-33); a resurrection reports 'effected' when the server broadcasts it, which does not prove the target is alive (docs/03 MEC-MAG-20)", ve `bad_target` açıklamasını "moral does not match the target kind; corpse-friend needs a named target" yap. Yalnızca yorum.

### 5.4 Sonuç sözleşmesi (`docs/03` MEC-MAG-20 ile aynı)

| Skill / durum | Sunucu | Bot sonucu |
|---|---|---|
| **Resurrection of love/grace/favors** (`112733/742/754`), hedef **ölü dost bot** (`BotPHD_K` → ölü `BotWP_K`), hedefte ≥ `NeedStone` taş | CASTING: `IsAvailable()` `MORAL_CORPSE_FRIEND` + `UserCanCast` hedef taş denetimi + ağaç denetimi ✔; EFFECTING: MP `Msp` bir kez (`:1028`), `RESURRECTION` dalı hedeften `NeedStone` taşı alır, çağırana `NeedStone/2 + 1` verir, hedefi `Regene(1, id)` ile **ölen yerinde** diriltir (tam HP, **MP 0**), çağırana hedef kimlikli EFFECTING yayını | `casting` (`op 1`), sonra `effected` (`op 3`, `code 0`, `victims` alanı yok); hedef `list`'te canlı, HP = `MaxHP`, MP ≈ 0, konum aynı |
| Hedef **hayatta** | `IsAvailable()` `isAlive()` ⇒ `fail_return` | CASTING'te `srv_fail` (`op 4`, `code -100`), MP düşmez |
| Hedefte **yeterli taş yok** (`CanUseItem(id, NeedStone)`) | `UserCanCast()` `SkillUseFail` ⇒ `MAGIC_FAIL` | CASTING'te `srv_fail`, MP düşmez |
| **Düşman** millet ceset | `IsAvailable()` `isHostileTo` ⇒ `fail_return` | CASTING'te `srv_fail` |
| Hedef `self` / ad verilmedi | bot kuralı | `REFUSED` `bad_target`, paket gitmez |
| Ağaç yetersiz (`BotPHB_*`: 1127 = 0) | `IsAvailable()` ağaç denetimi (KI-016) | CASTING'te `srv_fail` |
| Hedef `>= sRange` (11 m) | bot guard CASTING öncesi reddeder | `REFUSED`/`out_of_range`, `limit 11.00`, paket gitmez |
| Çağıranda recast (`ReCastTime 250` = 25 sn) | bot guard (`CastRecastMs`) aynı skill'in tekrarında bekletir | ARMED'de beklenir (reddi `FAIRNESS_REJECT` yazılmaz); farklı diriltme skill'i tip kapısı (0,7 sn) ve CLI-11 ile |
| Taş eksik ama EFFECTING'e ulaşıldı (yarış) | `RESURRECTION` dalı sessizce atlar, **yayın yine gider** `[D]` | `effected` (diriltme olmadı); sonuç `list` ile doğrulanmalı |
| CASTING'te `cast <bot> off` | iptal paketi hedef kimliği | `cancelled` (`op:4`, `code:-100`), MP düşmez |
| `Stone of life` `480001` (`RESURRECTION_SELF`), Bless of God `112671` (`Moral` 6), `111733` (sınıf 111) | — | `BeginCast` `unsupported_skill` (480001, 112671) / `bad_skill` (111733), paket gitmez |

`reason` metinleri, `CastOutcome::Kind` değerleri ve `TickCast` akışı bu tablo için **değişmez**; yeni çıktı yoktur.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (değişen dört dosya için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h`'yi içeren `.cpp`'ler de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastTypes_Supported`, `Combat_CureCast_Guard` ve `Combat_ResurrectionCast_Guard` adlarını `[ OK ]` ile içerir ve toplam test sayısı **110**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-33 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş; `git diff ... -- BotCore/BotCombat.h | grep '^-' | grep -v '^---'` boş (yalnızca ekleme).
- [ ] K5: `grep -n "CastResurrectionSupported\|CastNeedsOtherTarget\|m_Magictype5Array" GameServer/Bot/ActionExecutor.cpp` tam üç satır verir (her biri bir kez); `CastTypesSupported(m->bType[0], m->bType[1])`, `CastTypeMoralSupported(m->bType[0], m->bMoral)`, `CastMoralSupported(m->bMoral)`, `CastHpCostSupported(m->sHP)` ve `(m->bFlyingEffect != 0 && !flyingCast)` mevcut `if` içinde yerinde; `m->iUseItem != 0` koşulu yerinde (`grep -c "m->iUseItem != 0"` ≥ 2: biri diriltme ön koşulu, biri destek koşulu).
- [ ] K6: gerilemesiz: `git diff gece/2026-10-02...bot/F4-33 -- GameServer/Bot/ActionExecutor.cpp` yalnızca `BeginCast` içinde hunk içerir (destek koşulu çevresi ve `wantedTarget` satırı; iki hunk, ya da bitişik kalırlarsa tek hunk); `TickCast`/`SubmitCast`/`CancelCast`/`RejectCast` gövdesinde hunk yok; `git diff ... --stat` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `tools/` değişmemiş.
- [ ] K7: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı türü ve alanı yok (`git diff ... | grep '^+' | grep 'Emit('` boş).
- [ ] K8: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); dört dosya CRLF kalır (ASCII); `git diff --check` boş.
- [ ] K9: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 109 testin tamamı hâlâ geçiyor.
- [ ] K10: `python3 tools/check-perception-contract.py` `RESULT: PASS` (`R1`..`R5` ihlal sayıları 0; denetlenen dosya sayısı değişmez; araç ve istisna listeleri değişmez).
- [ ] K11 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-33
git diff gece/2026-10-02...bot/F4-33 -- BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff gece/2026-10-02...bot/F4-33 -- GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotManager.cpp GameServer/Bot/Telemetry.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj tools
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -rn "CastResurrectionSupported\|CastNeedsOtherTarget\|CastTypeMoralSupported\|CastMoralSupported" BotCore GameServer Tests
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff --check gece/2026-10-02...bot/F4-33
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; komut başına ≥ 1 sn). Botlar (hepsi zone 71, aynı noktada doğar; doğuşlar ≥ 3 sn arayla, KI-DEG-01): Karus **`BotPHD_K`** (diriltici, 1127 = 62), **`BotPHB_K`** (diriltemez, 1127 = 0), **`BotWP_K`** (diriltilecek ceset), El Morad **`BotMF_E`** (öldürücü: `210518`, ~16 cast ≈ −320/cast; F4-32 S5 bulgusu) ve ceset olarak düşman tarafı için **`BotWP_E`**. Önce `list` ile konum, HP/MP ve ölü durumunu denetle; botlar güvenli bölgede başlıyorsa her cast `srv_fail` verir (ölü bot `regene` ile doğuş noktasına taşınır, konumu DB'de kalır: F4-29/F4-30/F4-32 bulgusu). MP/HP/konum `list`'ten, olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'tan. MP kesin değeri sunucu yenilemesiyle (kümeli +40, ~4-5 sn'de bir) karışır: MP'yi cast'ten hemen önce ve sonra `list` ile al, yenileme payını raporla. **Ölçüm DB'ye yazar:** botlar `despawn`'da kaydedilir; taşlar tüketilir ve çağırana taş eklenir. Doğrulama sonunda taş durumunu eski hâline getirmek için `db/002_bot_characters.sql` (idempotent) yeniden uygulanabilir; uygulanmazsa rapora "bot taşları tüketildi" yaz. Her senaryo arasında gerekirse botlar `despawn`/`spawn` ile yenilenir.

1. **S1 Resurrection of love (`112733`):** `spawn BotPHD_K,BotWP_K,BotMF_E`; `cast BotMF_E 210518 BotWP_K 16` ile `BotWP_K`'yı öldür (`list` `hp=0`); ölü konumu kaydet. `cast BotPHD_K 112733 BotWP_K 1`: `CastStart` `ACTION_SUBMIT` **`"target":<BotWP_K kimliği>`** (`-1` değil) → `casting` `op 1`, `cast_ms` ≈ 1580; `CastEffect` → `effected`, `op 3`, `code 0`, **`victims` alanı yok**; log `cast finished (effected) after 1 cycle(s), 1 ok, 2 packet(s) sent`. Hemen `list`: `BotWP_K` **canlı**, `hp` = `MaxHP`, `mp` ≈ 0 (`Regene` MP'yi sıfırlar), konum ölü konumuyla **aynı** (ölen yerde dirildi); `BotPHD_K` MP **−400** (bir kez; yenileme payı ≤ ~40). `snap BotWP_K`: `buffs 0`. Birkaç sn sonra `BotWP_K` `m_deadSeen` temizlenmiş olmalıdır (aşağıdaki S2 doğrular). Bot günlüğünde `WIZ_REGENE` kaynaklı hata yok.
2. **S2 Hedef hayatta + sonraki ölüm:** `BotWP_K` canlıyken `cast BotPHD_K 112742 BotWP_K 1` (grace; farklı skill, 25 sn recast beklemesi yok): CASTING'te `srv_fail` (`op:4`, `code -100`), MP değişmez (yalnızca yenileme). Sonra `BotWP_K`'yı yeniden öldür (`210518`) ve ≥ 26 sn bekleyip `regene` KULLANMADAN `cast BotPHD_K 112742 BotWP_K 1`: `effected`, hedef canlı; MP **−600**. (Bu, `m_deadSeen` temizliğini ve ikinci ölümde tekrar diriltmeyi doğrular.)
3. **S3 Taşlar hedeften tüketilir (dolaylı kanıt):** S1 (love, 4 taş) ve S2 (grace, 10 taş) sonrası `BotWP_K`'da 30 − 14 = 16 taş kalmıştır. `BotWP_K`'yı yeniden öldür; `cast BotPHD_K 112754 BotWP_K 1` (favors, `NeedStone 30`): CASTING'te **`srv_fail`** (hedefte 16 < 30), MP değişmez; ardından aynı ceset için `cast BotPHD_K 112742 BotWP_K 1` (grace, 10 ≤ 16, ≥ 26 sn sonra gerekirse): `effected`, hedef canlı (6 taş kalır). Bu sıra, taşların ölü hedeften alındığını ve favors'un yetersiz taşla reddedildiğini birlikte gösterir. Çağıranın taş değişimi (`+NeedStone/2+1`, ek −1 `ConsumeItem`) okunamaz: `[D]`, rapora yazılır.
4. **S4 Bot kuralları:** `cast BotPHD_K 112733 self 1` ⇒ `refused (bad_target)`, **paket gitmez** (`ACTION_SUBMIT` yok). Ceset ≥ 11 m uzaktaysa (taşınabiliyorsa; `BotWP_K`'yı `move` ile uzaklaştırma ölü iken mümkün değil: canlı hedeften önce uzaklaştır, sonra öldür) `FAIRNESS_REJECT` `MEC-MAG-11` `out_of_range`, `limit 11.00`, paket gitmez; taşınamıyorsa birim testle kapsanır ve raporda yazılır. Menzil içinde (≤ 10 m) olduğu doğrulanmadan diğer senaryolara geçme.
5. **S5 Ağaç yetersiz ve düşman ceset:** `BotWP_K` ölüyken `cast BotPHB_K 112733 BotWP_K 1` (1127 = 0): CASTING'te `srv_fail` (KI-016), MP değişmez. Düşman ceset: `BotWP_E`'yi (El Morad) `BotMF_K` ile öldür ve `cast BotPHD_K 112733 BotWP_E 1` ⇒ CASTING'te `srv_fail` (`isHostileTo`), MP değişmez; düşman cesedi kurmak çok pahalıysa raporda "ölçülmedi, birim test + kod okuması" yaz.
6. **S6 İptal:** `BotWP_K` ölüyken `cast BotPHD_K 112733 BotWP_K 1` ardından CASTING aşamasında (~500 ms) `cast BotPHD_K off`: `cancelled` (`op:4`, `code:-100`), iptal paketi hedef kimliği `BotWP_K`, MP değişmez, `BotWP_K` ölü kalır.
7. **S7 Kapalı kalanlar, gerilemesiz ve kapsam:** `cast BotPHD_K 480001 BotWP_K 1` (`RESURRECTION_SELF` öğe skill'i) ⇒ `refused (unsupported_skill)`; `cast BotPHD_K 112671 self 1` (Bless of God) ⇒ `unsupported_skill`; `cast BotPHD_K 111733 BotWP_K 1` ⇒ `bad_skill`; üçünde de paket gitmez. Gerilemesiz: F4-32 `cast BotPHD_K 112525 self 1` `effected`, `code 0` (Cure curse); F4-28 `112603 self` `effected`, `code 600`; F4-31 grup heal `112557 self` `target -1`, `victims` ≥ 1; F4-29 Inferno `cast BotMF_K 110545 BotWP_E 1` `target -1`, `victims` ≥ 1; Moral 7 `cast BotMF_K 110518 BotWP_E 1` `target` kurban kimliği (`CastNeedsOtherTarget(7)` değişmedi). `TELEMETRY=summary`: diriltme çalışır, JSONL'de `ACTION_*` yok; `ENABLED=0` ⇒ komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde; sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Temizlik: ini yedekten geri, geçici betikler silindi, botlar despawn, sunucular `stop`.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `BotCore/` ve `Bot/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **`Type1 = 5` + `Moral` 25 + `UseItem 379006000` + `MAGIC_TYPE5.Type 3`** skill'i serisinde davranış değiştirir (önceden `unsupported_skill` idi); `wantedTarget` değişikliği `Moral` 7 için davranışı korur ve `Moral` 25'i ekler. Diğer tüm skill'ler için `BeginCast` davranışı (paket biçimi ve telemetri satırı dahil) değişmez.
- **Thread kuralı (ADR-0005):** `TickCast` yalnızca IOCP thread'inde; `BeginCast` `m_Magictype5Array`'i (yalnızca açılışta doldurulan tablo) okur, yeni durum/kilit yok.
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `effected` yalnızca `MAGIC_EFFECTING`'ten, `srv_fail` yalnızca `MAGIC_FAIL`'den. Hedefin ölü olup olmadığı, yeterli taşı olup olmadığı, millet ve ağaç yeterliliği **bota önkontrol olarak eklenmez** (ağaç için KI-016, hedef durumu için AC-LRN-03); bu bilgiler `WIZ_DEAD` gözlemi (`Perception`), `TeamView` ve karar katmanının (F7) işidir. MP önkontrolü bota ait `user->GetMana()` değeridir (mevcut `CheckCastStart`).
- **Bilinen sınırlar `[A]`/`[D]`:** (a) **`effected` diriltmenin gerçekleştiğini kanıtlamaz:** `RESURRECTION` dalı taş eksik/hedef değişmiş olsa da sessizce atlanır ve EFFECTING yayını yine gider (`:2054`); gerçek sonuç hedefin canlı olmasıdır (`list`/`WIZ_REGENE`/`WIZ_USER_INOUT` gözlemi; karar katmanı hedefin dirildiğini algıdan çıkarır). (b) **Çağıranın taş değişimi** (`+NeedStone/2+1` ve `ConsumeItem`'ın ek −1'i) okunamaz, `[D]` kod okuması; çalışma zamanı ölçümü yalnızca hedefin taş tükenişini dolaylı gösterir (S3). (c) **Diriltilen bot** buff'sız ve MP 0 ile kalır (`Regene` büyülü dal); exp geri kazanımı `m_sWhoKilledMe == -1` şartına bağlıdır (bot ölümleri oyuncu öldürmesiyse geri kazanım yoktur) ve ölçülmez. (d) **Stone of life tükenir:** tek bot 30 taşla en çok bir favors, ya da 7 love diriltilebilir; envanter yeniden stoklaması dilim 8'dedir (karar katmanı F7 hedefin stoğunu bilmeden favors seçmemelidir, aksi halde `srv_fail`). (e) **Recast 25 sn:** aynı diriltme skill'i 25 sn içinde tekrar atılamaz; farklı diriltme skill'leri tip kapısı (0,7 sn) ile ayrılır, ama sunucu `m_CoolDownList` skill başına tutar. (f) Çağıran ölüyse `BeginCast` `dead` ile reddeder (sunucu ölü Type5 çağırana izin verir, `:166`; bot bunu kullanmaz: self-diriltme kapsam dışı).
- Telemetri hacmi değişmez; `tools/bot-telemetry-report.py` değişmez (`no_result` ve `srv_fail` zaten ayrı sayılır, MET-ACT-02).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI (statik kriterler); K11 çalışma zamanı Claude'a bağlı.
- Branch / commit'ler: `bot/F4-33` (taban `gece/2026-10-02` @ `1d9e80f`); kod commit `31b4dd6` ("[F4-33] Diriltme dilimi: Moral 25 Type5 destegi ve hedef kurali"); plan durum/rapor commit'i bu satırla birlikte.
- Değişen dosyalar ve neden:
  - `BotCore/BotCombat.h`: `CastHpCostSupported` ile `CastTargetIdField` arasına `kMoralCorpseFriend`/`kType5Resurrection`/`kResurrectionStoneItem` sabitleri, `CastResurrectionSupported` ve `CastNeedsOtherTarget` eklendi (yalnızca ekleme, 24 satır).
  - `Tests/BotCoreTests/CombatTests.cpp`: dosyanın sonuna `Combat_ResurrectionCast_Guard` eklendi (109 → 110); mevcut testler değişmedi.
  - `GameServer/Bot/ActionExecutor.cpp`: yalnızca `BeginCast`; diriltme bayrağı (`m_Magictype5Array` okuması) ve `resurrection` istisnası destek koşuluna eklendi, `wantedTarget` satırı `BotCore::CastNeedsOtherTarget` oldu. İki hunk, ikisi de `BeginCast` içinde.
  - `GameServer/Bot/ActionExecutor.h`: yalnızca `BeginCast` yorumu (diriltme desteği + `bad_target` açıklaması).
- Derleme sonucu: `./tools/build.sh Release` ve `./tools/build.sh Debug` hatasız (`PIPESTATUS=0`); son satırlar `proj-GameServer.vcxproj -> ...\Server\GameServer.exe` ve `BotCoreTests.vcxproj -> ...\Tests\BotCoreTests.exe`. Tam yeniden derlemede yalnızca önceden var olan `UpgradeHandler.cpp` C4789 uyarıları görüldü; dört değişen dosyada uyarı yok.
- Kabul kriterleri öz-değerlendirme (K11 hariç, Claude'a bağlı):
  - K1 ✔ Release rc=0; K2 ✔ Debug rc=0; K3 ✔ `110 tests, 0 failed` (Release+Debug), `Combat_CastTypes_Supported`/`Combat_CureCast_Guard`/`Combat_ResurrectionCast_Guard` `[ OK ]`.
  - K4 ✔ `grep` eşleşmesiz; `BotCombat.h` yalnızca ekleme (silinen satır yok), `std::min/max` yok.
  - K5 ✔ üç satır (`CastResurrectionSupported` :741, `m_Magictype5Array` :739, `CastNeedsOtherTarget` :772), mevcut `if` koşulları yerinde, `m->iUseItem != 0` iki kez.
  - K6 ✔ `--stat` yalnızca dört dosya; `ActionExecutor.cpp` iki hunk, ikisi de `BeginCast`; yasaklı dosyalar (vcxproj, `BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `tools/`) değişmedi.
  - K7 ✔ yeni `Emit(` yok; K8 ✔ `file` dört dosyada da "ASCII text, with CRLF line terminators", `git diff --check` boş; K9 ✔ sekiz guard grep'i eşikleri karşılıyor, 109 eski test geçiyor; K10 ✔ `RESULT: PASS`.
- Plandan sapmalar ve gerekçeleri: `ActionExecutor.h` yorumunda F4-32'nin "the party-all cure and resurrections stay unsupported" ifadesi "the party-all cure stays unsupported" yapıldı (diriltme artık destekli; aksi hâlde yorum planla çelişirdi). Davranış değişikliği yok.
- Açık sorular: Yok. K11 (S1–S7 çalışma zamanı) Claude'un `/plan-dogrula` adımında yapılır; çalışma zamanı ölçümü botların Stone of Life taşlarını tüketir (plana göre `db/002` ile geri alınabilir).

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-33` @ `b770f02` (kod `31b4dd6`). Çalışma ağacı temizdi; otonom gece döngüsünde (`AUTO_LOOP=1`, `AUTO_INTEGRATION_BRANCH=gece/2026-10-02`) birleştirme/push yapılmadı, birleştirmeyi döngü betiği `gece/2026-10-02`'ye yapar.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | dört dosya `touch` + `tools/build.sh Release` rc=0, `warning` 0, `error` 0 |
| K2 | ✔ | `tools/build.sh Debug` rc=0, değişen dosyalarda uyarı/hata 0 |
| K3 | ✔ | `run-tests.sh Release` ve `Debug` ikisinde rc=0, `110 tests, 0 failed`; `[ OK ] Combat_CastTypes_Supported`, `Combat_CureCast_Guard`, `Combat_ResurrectionCast_Guard` |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` boş; eklenen satırlarda `std::min`/`std::max` 0; `-` satırı 0 (`BotCombat.h` yalnızca ekleme, +24) |
| K5 | ✔ | `ActionExecutor.cpp` `:739` `m_Magictype5Array`, `:741` `CastResurrectionSupported`, `:772` `CastNeedsOtherTarget` (her biri bir kez); `CastTypesSupported(m->bType[0], m->bType[1])`, `CastTypeMoralSupported`, `(m->bFlyingEffect != 0 && !flyingCast)`, `CastMoralSupported`, `CastHpCostSupported(m->sHP)` mevcut `if` içinde yerinde (`:748-755`); `grep -c "m->iUseItem != 0"` = 2 |
| K6 | ✔ | `git diff --stat`: yalnızca §4'teki 4 dosya + plan dosyası; `ActionExecutor.cpp` iki hunk (destek koşulu `:733`, `wantedTarget` `:769`), ikisi de `BeginCast` içinde; vcxproj, `BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `tools/` farkı boş |
| K7 | ✔ | kod farkında eklenen `Emit(` yok (tek eşleşme plan dosyasındaki rapor metni); yeni ini anahtarı/komut/thread/olay türü yok; `TELEMETRY=summary` ve `ENABLED=0` çalışma zamanında sınandı (aşağıda) |
| K8 | ✔ | `file`: dört dosya `ASCII text, with CRLF line terminators`; `git diff --check` rc=0, çıktı boş |
| K9 | ✔ | `CheckMoveStep` 2; `CheckAttack`, `CheckCastStart`, `CheckCastEffect`, `CheckCastFly`, `CheckCastLand`, `CheckCastCancel`, `CheckPotion` 1'er; önceki 109 test dahil 110 test geçiyor |
| K10 | ✔ | `check-perception-contract.py` `RESULT: PASS` |
| K11 | ✔ | S1–S7 çalışma zamanında geçti (aşağıda; iki plan-kurulum notu bulgularda) |

**Çalışma zamanı** (Release derlemesi `GameServer.exe`, `ENABLED=1`, `MAX_BOTS=16`, `TELEMETRY=decisions`, zone 71; `Logs/bots/2026-10-03/live-041901.jsonl`, `Logs/Bot_3_10_2026.log`; `summary` ve `ENABLED=0` için ayrı yeniden başlatma. Botlar: `BotPHD_K` (2984, diriltici), `BotWP_K` (ceset), `BotMF_E` (öldürücü, `210518` ×16), `BotPHB_K`, `BotMF_K`, `BotWP_E`; hepsi ≤ 12 m içinde). MP ölçümünde sunucu yenilemesi kümeli +40'tır.

- **S1 ✔ Resurrection of love (`112733`).** `BotMF_E` `210518` ×16 ile `BotWP_K` öldürüldü (`hp=0`, konum 1274,955). `cast BotPHD_K 112733 BotWP_K 1`: `CastStart` `ACTION_SUBMIT` **`"target":2985`** (`-1` değil), `cast_ms 1580` → `casting` `op 1`; `CastEffect` → `effected`, `op 3`, `code 0`, **`victims` alanı yok**; log `cast finished (effected) after 1 cycle(s), 1 ok, 2 packet(s) sent`. Hemen `list`: `BotWP_K` **canlı**, `hp` 5650/5650, `mp` **0**/5370, konum 1274,955 (**ölen yerde**); `BotPHD_K` MP **6392 → 5992 (−400 tam, yenileme araya girmedi)**. `snap BotWP_K`: `alive`, `buffs 0`. Aynı yolla `BotPHB_K` (priest) cesedi de dirildi: HP 3491/3491, MP 0 (+40), `BotPHD_K` MP 5672 → 5312 (−400 + 40).
- **S2 ✔ Hedef hayatta + sonraki ölüm.** `BotWP_K` canlıyken `cast BotPHD_K 112742 BotWP_K 1`: CASTING'te `ok:false`, `srv_fail`, `op 4`, `code -100`; MP 6072 → 6112 (yalnızca yenileme). `BotWP_K` yeniden öldürüldü (`hp=0`, `regene` kullanılmadı), ≥ 26 sn sonra aynı grace: `effected`, `code 0`, hedef canlı (HP tam), MP **6392 → 5792 (−600)**. İkinci ölümde tekrar diriltme çalıştı (`m_deadSeen` temizliği dolaylı doğrulandı), `Bot_*.log`'da `WIZ_REGENE` kaynaklı hata yok.
- **S3 ✔ Taşlar hedeften tüketilir.** Love (4) + grace (10) sonrası `BotWP_K`'da 16 taş kalmıştı. Yeniden öldürüldü; `cast BotPHD_K 112754 BotWP_K 1` (favors, 30): CASTING'te **`srv_fail`**, MP 6152 → 6192 (yalnızca yenileme). Hemen ardından aynı cesede grace (10 ≤ 16): `effected`, hedef canlı, MP 6192 → 5632 (−600 + 40). Çağıranda 30 taş vardı ve favors reddedildi; denetimin ölü hedefe yapıldığı dolaylı kanıtlandı. Çağıranın taş değişimi okunamaz `[D]`.
- **S4 ✔ Bot kuralları.** `cast BotPHD_K 112733 self 1` ⇒ `refused (bad_target)`, `ACTION_SUBMIT` yok. `BotWP_K` canlıyken 1274,940'a taşındı (menzilden 22 m uzak), öldürüldü: `cast BotPHD_K 112733 BotWP_K 1` ⇒ **paket gitmeden** `FAIRNESS_REJECT` `MEC-MAG-11` `out_of_range`, `value 22.00`, `limit 11.00`, log `cast stopped (out_of_range)`, MP yalnızca yenilendi. Sonraki senaryolarda mesafe ≤ 7 m doğrulandı (`list`).
- **S5 ✔ Ağaç yetersiz ve düşman ceset.** `BotWP_K` ölüyken (6 taşı vardı, yani taş yetersizliği değil) `cast BotPHB_K 112733 BotWP_K 1`: `CastStart` `srv_fail`, `op 4`, `code -100`, MP 4822 → 4862 (yalnızca yenileme). Düşman ceset: `BotMF_K` `110518` ile `BotMF_E` öldürüldü; `cast BotPHD_K 112733 BotMF_E 1` ⇒ `CastStart` `srv_fail` (`isHostileTo`). Not: bu denemede `BotPHD_K` MP'si tavandaydı (6392/6392), MP'nin değişmediği doğrudan görülemedi; CASTING'te reddedilen cast'te MP düşmez (S2/S3/S5a'da ölçüldü).
- **S6 ✔ İptal.** `BotPHB_K` ölüyken (30 taş) `cast BotPHD_K 112733 BotPHB_K 1` ve `casting` yanıtı görülür görülmez `cast BotPHD_K off`: `CastCancel` **`"target":2987`**, `cause cmd`, `since_casting_ms 1101`, `cancelled`, `op 4`, `code -100`; `BotPHD_K` MP 6352 → 6392 (yenileme, tavan), `BotPHB_K` ölü kaldı (`hp=0`). (İlk deneme: iptal komutu 1,5 sn sonra gittiği için cast tamamlandı ve `BotWP_K` dirildi; bu bir hata değil, komut dosyası okuma gecikmesi, aşağıdaki not 2.)
- **S7 ✔ Kapalı kalanlar, gerilemesiz, kapsam.** `cast BotPHD_K 480001 BotWP_K 1` ve `112671 self 1` ⇒ `refused (unsupported_skill)`; `111733 BotWP_K 1` ⇒ `refused (bad_skill)`; üçünde de jsonl'de `ACTION_SUBMIT` yok. Gerilemesiz: F4-32 `112525 self` `effected`, `code 0`, `target 2984`; F4-28 `cast BotPHB_K 112603 self 1` `effected`, **`code 600`**; F4-31 grup heal `112557 self`: `CastStart`/`CastEffect` `"target":-1`, `effected`, `victims 1`; F4-29 Inferno `cast BotMF_K 110545 BotWP_E 1`: `target -1`, `effected`, `victims 1`; Moral 7 `cast BotMF_K 110518 BotWP_E 1`: `"target":2989` (kurban kimliği), `effected`, `victims` yok. `TELEMETRY=summary` (yeniden başlatma): `BotPHD_K` `BotPHB_K` cesedini diriltti (`effected`, hedef HP 3491/3491), `live-043038.jsonl`'de **`ACTION_` satırı 0**, yalnızca `PERF_SAMPLE`. `ENABLED=0` (ini yedeğinden, `[BOT]` yok): `BotCommands.txt` işlenmedi (dosya yerinde kaldı), `Bot_*.log` satır sayısı değişmedi, yeni jsonl yok. `PERF_SAMPLE` `tick_p95_us` normalde 70–528; spawn penceresinde en çok 1222 (önceki planlarla aynı düzey). Sunucu 3/3 UP; `GameServer.log` değişmedi (son yazım 2026-10-02).
- Temizlik: botlar despawn, sunucular `stop` (nazik), `GameServer.ini` ve `GameServer.exe` orijinallerine döndü (`diff` ini == `GameServer.ini.bak-before-bot-test-20261002`; exe botsuz ikili geri kondu), `BotCommands.txt` silindi, yedekler silindi, çalışma ağacı temiz. **DB durumu:** ölçüm DB'ye yazdı ve `db/002` yeniden uygulanmadı: botların Stone of Life taşları tüketildi (`BotWP_K` ≈ 2, `BotPHB_K` ≈ 22, `BotPHD_K` fazla taş aldı), `BotMF_E` DB'de ölü, konumlar 1274,940–950 civarı. Sonraki diriltme ölçümünden önce `db/002` yeniden uygulanmalıdır.

- Bulgular (önem sırasıyla; engelleyici yok):
  1. *(not)* **Plan S7 kurulum hatası (planlayıcı eksiği):** F4-28 `112603 self` `BotPHD_K` ile `srv_fail` verdi, çünkü `112603` `Skill 1126` ağacındadır ve `BotPHD_*` `strSkill[6] = 0` (yalnızca `BotPHB_*` `[6] = 62`); KI-016 ağaç denetimi, gerileme değil. `BotPHB_K` ile `effected`, `code 600`. Kod değişikliği gerekmez.
  2. *(not)* **Plan S6 kurulum notu:** komut dosyası saniyede bir okunduğu için "~500 ms" iptal elle yakalanamaz; iptal `casting` yanıtı izlenerek gönderilince `since_casting_ms 1101`'de çalıştı. İptal yolu ölçüldü, 500 ms hedefi planın yanlış beklentisiydi.
  3. *(not)* **Çalışma zamanı kurulum notları:** (a) `regene` ölü `BotMF_E`'yi El Morad doğuş noktasına (630,920) taşır ve oradan dönüş ~190 m yürüyüşte Death knight/Undying NPC'leri botu öldürdü; (b) zone 71 NPC'leri botları kendiliğinden öldürür (`BotPHB_K` ~2 dk içinde öldü), bu `TELEMETRY=summary` denemesinde ceset kaynağı oldu.
  4. *(not)* **Sonuç sözleşmesi doğrulandı:** `docs/03` MEC-MAG-20 `[D]` → `[V]`, `v1.15` değişiklik satırı eklendi. `[D]` kalanlar: çağıranın taş değişimi (`+NeedStone/2+1`, ek −1), exp iadesi, "taş EFFECTING'te eksik, yayın yine gider" yarışı, favors `112754` başarılı atımı (hedefte 30 taş bırakılmadı), El Morad `2127xx`.
  5. *(not)* Uygulayıcı raporu doğru: commit listesi, dosyalar, derleme ve test sayıları kendi çalıştırmamla örtüşüyor; `ActionExecutor.h` yorum sapması (F4-32 ifadesinden "resurrections" çıkarıldı) kabul.
- Düzeltme talimatı: yok (karar DOĞRULANDI).
