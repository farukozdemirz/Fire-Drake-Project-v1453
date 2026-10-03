# F4-49: `ActionExecutor` alan-dost skill dilimi — Elysian Web (`Moral` 11 = AREA_FRIEND)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-49 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F4-29 (alan: hedef kimliği `-1`, hedef noktası, `victims`) — `KAPANDI`; F4-31 (`SendsAimPoint`, `Moral` 6 party-all) — `KAPANDI`; F4-36 (eşyalı sınıf skill'i kapısı `CastItemSkillSupported`, `no_item`) — `KAPANDI`; F4-46 (priest usta skill'leri, Stone of Priest tüketimi, `docs/05` §9.5) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-07, CLI-11, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-15, MEC-MAG-16, MEC-MAG-18, MEC-MAG-23, MEC-MAG-25 (bu planla eklendi, `[D]`), AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 Ek 1 madde 2 (party/alan-dost hedef çözümü), Ek 22, Ek 25 (bu planla eklendi); `docs/05` §6 (Elysian Web `112825`), SK-06 |
| Tahmini büyüklük | S (4 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.h`, `ActionExecutor.cpp`; son ikisinde yalnızca yorum; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

Priest'in usta ağacındaki **Elysian Web** (`112825`/`212825`: Type4, `MAGIC.Moral` 11 = `MORAL_AREA_FRIEND`, hedef çevresinde `Radius 15` m içindeki dostlara büyü hasarı azaltma, 20 sn) botla atılamıyor: `BeginCast` bunu `unsupported_skill` ile reddediyor, çünkü `CastMoralSupported(11)` `false` (`BotCore/BotCombat.h:373-379`). F7 (party koordinasyonu: "Düşman mage patlaması" karşısında buff, `docs/05:135`) bu skill'i bekler; F4-46 ve `docs/05` §9.5 bunu açıkça "ayrı C++ dilimi" diye ertelemişti.

Sunucu tarafında **yeni akış gerekmez**: `Moral` 11 de `Moral` 10 ve 6 gibi hedef kimliği `-1` + hedef noktası (`sData[0]` x, `sData[2]` z) ister ve kurban listesini sunucu seçer (aşağıda §2). F4-29'un `area` bayrağı (paket alanları + `victims`) aynen işe yarar. Bu plan:

1. `Moral` 11'i "aim point gönderen" skill'ler kümesine (`SendsAimPoint`) ve dolayısıyla `CastMoralSupported`'a ekler;
2. Elysian Web'in bütün kapılardan geçtiğini (Type4 `{4, 0}`, eşya `379062000` Stone of Priest, `Skill 1128` ağacı) birim testle sabitler;
3. sunucu davranışını kodla belgeler (`docs/03` MEC-MAG-25) ve çalışma zamanında Claude doğrular.

F4'ün kırk dokuzuncu planıdır (ADR-0018 sırası: ... eşyalı skill'ler ✔ → çift tipli ✔ → CLI-12 ✔ → envanter doldurma ✔ → T-MECH-SKILL koşuları ✔ → **alan-dost (bu plan)** → algı eksikleri (F4-53 `TASLAK`)).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-49)" (bu planla birlikte yazıldı), "Ek (F4-31)", "Ek (F4-29)". `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` Ek 25 (bu planla yazıldı), Ek 22.
- `docs/03` §4.2 **MEC-MAG-16** (alan: hedef `-1`, kurban başına yayın, `victims`), **MEC-MAG-18** (party hedefli skill, grup buff'ta aynı `BuffType` sessizce atlanır), **MEC-MAG-23** (eşyalı sınıf skill'i), **MEC-MAG-25** (bu planla eklendi), §13.2 CLI-07; `docs/05` §6 satır 135 (Elysian Web), §9.5 son paragraf ("Elysian Web botla atılamaz").
- `plans/F4-31-aksiyon-yurutucu-party-hedefli-skill.md` (aynı fonksiyonlar, `Moral` 6; **yazılı planı değil, kodu esas al**).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `de64af0` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.h:23-37` — `MORAL_AREA_ENEMY = 10`, **`MORAL_AREA_FRIEND = 11`**, `MORAL_AREA_ALL = 12`, `MORAL_SELF_AREA = 13`.
  - `GameServer/MagicInstance.cpp:170-180` (`UserCanCast`): `Moral 10..13` iken hedef kimliği `-1` olmak zorunda (`sTargetID != -1` ⇒ `SkillUseFail`); bu yüzden `Moral` 11 skill'i **mutlaka** `-1` ile gider. `:806-900` (`IsAvailable()` `bMoral` switch'i): `MORAL_AREA_FRIEND` için `case` yoktur ⇒ ek denetim yok. `:446-478` `CheckType3Prerequisites` (Type3'e özgü); `:540-583` `CheckType4Prerequisites`: hedef `-1` ise `true`.
  - **MP:** `IsAvailable()` EFFECTING'te `bType[0] != 4 || sTargetID == -1` iken `Msp`'yi **bir kez** düşer (`:1028-1029`) ⇒ Elysian Web (`Type1 = 4`, hedef `-1`) EFFECTING'te `Msp 640` bir kez öder, kurban yoksa da; `ExecuteType4`'ün ek düşümü yalnızca `sTargetID != -1` (`:1800-1802`).
  - **Eşya:** `IsAvailable()` `:240-262`: `iUseItem != 0` ⇒ `CanUseItem(iUseItem)`; `nBeforeAction` 1..4 değilse `nConsumeItem = iUseItem`. Elysian Web `UseItem 379062000` (Stone of Priest (+0), `ITEM` tablosunda doğrulandı), `BeforeAction 0` ⇒ tüketilen eşya **379062000'in kendisi**; `ConsumeItem()` (`:2983-2997`) yalnızca `370001000..3`, `379069000/70/63/64/65/66`'yı atlar, 379062000 atlanmaz ⇒ her başarılı atışta 1 azalır. `ConsumeItem()` `MAGIC_EFFECTING`'te `ExecuteSkill(bType[0])` `true` dönünce çağrılır (`:117-133`).
  - **Kurban seçimi** `ExecuteType4` `:1631-1677`: `sTargetID == -1` ⇒ `GetUnitListFromSurroundingRegions(pSkillCaster)` (çağıranın 3×3 bölgesindeki bütün NPC ve oyuncular, çağıran dahil, `GameServerDlg.cpp:1560-1589`); her biri için `!isDead() && !isBlinking() && isAttackable() && UserRegionCheck(...)`; arena'da çağıranın kendisi atlanır (`:1647`, Ronark'ta `isInArena()` `false`). `CMagicProcess::UserRegionCheck` `MORAL_AREA_FRIEND` (`MagicProcess.cpp:172-175`): **`!pSkillCaster->isHostileTo(pSkillTarget)`** ⇒ `final_test`: `radius == 0 || pSkillTarget->isInRangeSlow(sData[0], sData[2], radius)` (2D, hedef noktasından; `radius = MAGIC_TYPE4.Radius = 15`). `CUser::isHostileTo` (`Unit.cpp:1219-1259`): farklı ulus + `isInPVPZone()` ⇒ düşman (Ronark zone 71'de El Morad botları Karus için düşman), aynı ulus ⇒ dost (**party üyeliği aranmaz**), NPC'ler için `CNpc::isHostileTo` (dost NPC'ler de kurban olabilir). Ayrıca çağırandan kurbana mesafe `>= sRange` (56) ise kurban atlanır (`:1695`).
  - **Boş kurban listesi** (`:1664-1675`): çağıran oyuncu ve `Moral` 6 değilse `SendSkill(); return false` ⇒ **`ConsumeItem()` çalışmaz**, taş harcanmaz (MP yine düşmüştür). Çağıranın kendisi blinking ise (yeni doğmuş/yeniden doğmuş bot) listeye girmez; bu bir kurulum notudur (§7).
  - **Buff uygulaması** `:1700-1810`: Elysian Web `BuffType 27` (`BUFF_TYPE_RESIS_AND_MAGIC_DMG`, `GameDefine.h:774`; `CMagicProcess::IsBuff` `MagicProcess.cpp:1100` ⇒ **buff**). Hedefte aynı `BuffType` varsa (`bSkillTypeAlreadyOnTarget && isBuff`) alan yolunda (`sTargetID == -1`) hata verilmez, o kurban **sessizce atlanır** (`continue`, o kurbana yayın yok); **atlanan kurbanlar olsa bile `ExecuteType4` `true` döner** ⇒ `ConsumeItem()` çalışır (taş harcanır). Her uygulanan kurban için `{sData[0], 1, sData[2], süre, ...}` ile hedef kimlikli EFFECTING yayını (`BuildAndSendSkillPacket`, `:1874`); `sDuration = MAGIC_TYPE4.Duration = 20`.
  - `GameServer/Bot/ActionExecutor.cpp` (`de64af0`): `BeginCast` destek koşulu `:772-782` (`CastMoralSupported(m->bMoral)` `:779`), `bad_target` kuralı `:811-817` (`wantedSelf` yalnızca `MORAL_SELF`, `wantedTarget` yalnızca `CastNeedsOtherTarget` / summon / warpOther ⇒ `Moral` 11 hem `self` hem hedef adı ile kabul edilir), `no_item` ön kontrolü `:796-806`, `TickCast` `area` satırı `:870-875` (`bool area = BotCore::SendsAimPoint(m->bMoral);`), `sData` `:955-959` (`CastCoordField(area, target.isSelf, ...)`).
  - `BotCore/BotCombat.h:342-379` (`kMoralAreaEnemy`, `IsAreaMoral`, `kMoralPartyAll`, `IsPartyAllMoral`, `SendsAimPoint`, `CastMoralSupported`), `:474-490` (`CastItemSkillSupported`, `CastConsumeItem`), `:493-508` (`CastTargetIdField`, `CastCoordField`). `tests/BotCoreTests/CombatTests.cpp`: `Combat_CastMoral_Supported` `:1179-1207` (`CastMoralSupported(11) == false` `:1197` ve `IsAreaMoral(11) == false` `:1204` var), `Combat_PartyCast_Guard` `:1300`, `Combat_ItemSkill_Guard` `:1651`, dosyanın son testi `Combat_Type1Pair_Guard` `:1698`. Toplam birim test **251**.
- **Veri notu (Claude yerel `MAGIC`/`MAGIC_TYPE4`/`ITEM` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** `MAGIC.Moral = 11` satırları yalnızca dörttür: `112825`, `212825` (Elysian Web, bot sınıfları) ve `300106`, `300208` (`Type1 = 3`, `Skill 0`, `UseItem 0`: canavar/NPC skill'leri). Elysian Web: `Type1 4`, `Type2 0`, `Moral 11`, `Skill 1128`/`2128`, `SkillLevel 20`, `Msp 640`, `HP 0`, `CastTime 15` (1,5 sn), `ReCastTime 1` (100 ms), `Range 56`, `UseStanding 0`, `UseItem 379062000`, `BeforeAction 0`, `FlyingEffect 0`, `Etc 0` (quest yok), `SuccessRate 50` (**kullanılmaz**: `bSuccessRate` yalnızca Type3 yıldırım stun kolunda okunur, `MagicInstance.cpp:1366`); `MAGIC_TYPE4`: `BuffType 27`, `Radius 15`, `Duration 20`.
- **Bot karakter notu (`db/002_bot_characters.sql:128-135`).** Skill ağacı baytları: `BotPHD_K` `0x00000000003C003E1400` ve `BotPHB_K` `0x00000000003C3E001400` ⇒ `m_bstrSkill[8]` (`Skill 1128 % 10 = 8`, usta ağacı) `0x14 = 20 >= SkillLevel 20` ⇒ **her iki priest de Elysian Web'i atabilir** (Judgment `SkillLevel 2`, Helis `12` aynı ağaçtan, F4-46'da ölçüldü). Çantada Stone of Priest: sınıf taşı yuvası `ClassStones=50` (`db/002`, `db/004`); `CanUseItem(379062000)` F4-46'da Judgment/Helis (`BeforeAction 4` ⇒ aynı eşya) ile geçti `[V]`. El Morad karşılıkları `BotPHD_E`/`BotPHB_E` `212825`.
- **Menzil notu.** `Range 56` m: hedef noktası çağırandan `< 56` m olmalı (bot guard'ı `CastInRange`, MEC-MAG-11) ve kurbanlar çağırandan `< 56` m **ve** hedef noktasının `Radius 15` m içinde olmalı (sunucu seçer). `self` atışta hedef noktası çağıranın kendi konumudur, bu yüzden çağıran her zaman yarıçap içindedir.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `kMoralAreaFriend = 11`, `IsAreaFriendMoral(moral)`; `SendsAimPoint(moral)` `Moral` 10, 6 **ya da 11** için `true`; `CastMoralSupported` `SendsAimPoint`'e dayandığı için 11'i otomatik kabul eder. `IsAreaMoral` **değişmez** (yalnızca `Moral` 10; `IsAreaMoral(11) == false` testi kalır). Yorumlar güncellenir (§5.1).
2. **`ActionExecutor.cpp` / `ActionExecutor.h`:** **yalnızca yorumlar** (`TickCast`'ın `area` satırı üstündeki yorum, `CastTarget`/`BeginCast`/`TickCast` açıklamaları); `bool area = BotCore::SendsAimPoint(m->bMoral);` satırı ve diğer her kod aynen kalır (§5.3).
3. **Birim testleri:** `Combat_CastMoral_Supported` güncellenir (`CastMoralSupported(11)` `false` → `true`); yeni `Combat_AreaFriendCast_Guard` eklenir (251 → 252).
4. **Sonuç sözleşmesi (§5.4):** `Moral` 11 akışı belgelenir (`docs/03` MEC-MAG-25 Claude'da yazılı) ve çalışma zamanında sınanır.

**Kapsam dışı (yapılmayacak)**

- `Moral` 3 (dost, kendisi hariç), 5 (NPC), 9, 12 (`AREA_ALL`), 13 (`SELF_AREA`), 14, 15, 16+; Type5 party-all cure (Bless of God `Moral` 6), Type8 (Escape, Blink, Wild advent), `{4, x}` çiftleri; hepsi kapalı kalır ve birim testiyle sabitlenir.
- **Canavar/NPC skill'leri `300106`/`300208`** (`Moral` 11, `Skill 0`): bu plan onlara özel kapı eklemez. Bot yalnızca komut/betik/karar katmanının istediği skill'i atar; sınıf skill'i dışı (`Skill == 0`) skill'lerin genel politikası F4-24 öncesi bir karardır (ADR-0017). Bir bot `300106`'yı komutla denerse sunucu karar verir.
- **Karar katmanı:** Elysian Web'in ne zaman atılacağı ("düşman mage patlaması" öncesi, kaç dost yarıçapta, tekrar atma aralığı) F6/F7'nin işidir. Bota **yarıçapta kaç dost var / hedefte buff var mı önkontrolü eklenmez** (AC-LRN-03: `TeamView` ve `SelfState.buffs` algıdan zaten okunur, karar katmanı kullanır).
- Stone of Priest stok izleme, `snap` eşya sayacı, envanter doldurma (m.8 `db/004` mevcut), skill ağacı puanı denetimi (KI-016), blinking durumunun botta önkontrolü (bot yalnızca sunucu sonucunu görür).
- Yeni komut/ini anahtarı/telemetri olayı türü/alan, `BotSession.*`/`BotManager.cpp` değişikliği, `tools/` betik değişikliği, yeni `skill_priest_k_*.spec` betiği (Elysian Web'in botla ölçümü Claude'un çalışma zamanı doğrulamasıdır; kalıcı betik gerekirse ayrı plan).
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | `// --- area cast ...` bölümü: `kMoralAreaFriend`, `IsAreaFriendMoral`, `SendsAimPoint`, `CastMoralSupported` yorumu |
| `tests/BotCoreTests/CombatTests.cpp` | değiştir | `Combat_CastMoral_Supported` güncellenir; `Combat_AreaFriendCast_Guard` eklenir (251 → 252) |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | **yalnızca yorum** (`:870-871`) |
| `GameServer/Bot/ActionExecutor.h` | değiştir | **yalnızca yorum** (`CastTarget` `:34-36`, `BeginCast` `:218-230`, `TickCast` `:260-270` açıklamaları) |

`BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `ScenarioRunner.*`, `ScriptRunner.*`, `tools/` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`// --- area cast ... ---` bölümünü şöyle genişlet (ASCII, CRLF, tab, `namespace BotCore` içinde; yeni `#include` yok; **`std::min`/`std::max` kullanma**). `kMoralAreaEnemy`/`IsAreaMoral`/`kMoralPartyAll`/`IsPartyAllMoral` aynen kalır; `IsPartyAllMoral`'dan sonraki `SendsAimPoint` ve `CastMoralSupported` bloklarını (ve üstlerindeki yorumları) değiştir, araya `kMoralAreaFriend` bloğunu ekle:

```cpp
	// MAGIC.Moral 11 = AREA_FRIEND (MagicInstance.h): every non-hostile unit within MAGIC_TYPE4.Radius of the aim point
	// (CMagicProcess::UserRegionCheck: !isHostileTo, so the caster's whole nation in the zone, party membership is not
	// asked). Like area enemy (10) and party-all (6) it is cast with target id -1 (UserCanCast demands it for Moral
	// 10..13) and the aim point in sData[0] (x) / sData[2] (z); the server picks the victims (F4-49, docs/03 MEC-MAG-25).
	constexpr uint8_t kMoralAreaFriend = 11;

	inline bool IsAreaFriendMoral(uint8_t moral)
	{
		return moral == kMoralAreaFriend;
	}

	// Skills cast with target id -1 and an aim point: area enemy (10, F4-29), party-all (6, F4-31) and area friend (11, F4-49).
	inline bool SendsAimPoint(uint8_t moral)
	{
		return IsAreaMoral(moral) || IsPartyAllMoral(moral) || IsAreaFriendMoral(moral);
	}

	// Morals BeginCast accepts: 1 self, 2 friend-with-me, 4 party member, 7 enemy, 8 all (F4-03, F4-31), 10 area-enemy
	// (F4-29, flying or not: F4-30), 6 party-all (F4-31) and 11 area-friend (F4-49). Whether the skill may fly at all is
	// decided by the caller (IsFlyingCast: Type3 only).
	inline bool CastMoralSupported(uint8_t moral)
	{
		if (moral == 1 || moral == 2 || moral == 4 || moral == 7 || moral == 8)
			return true;

		return SendsAimPoint(moral);
	}
```

`CastTargetIdField(bool area, ...)` ve `CastCoordField(bool area, ...)` **değişmez** (çağıran `SendsAimPoint` sonucunu verir); üstlerindeki yorumlarda "area and party-all" ifadesi "area, party-all and area-friend" olur. Dosyanın kalanı **değişmez**. `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`tests/BotCoreTests/CombatTests.cpp`)

a) **`Combat_CastMoral_Supported` (var olan test) güncellenir:** `true` kümesi `1, 2, 4, 6, 7, 8, 10, 11`; `false` kümesi `0, 3, 5, 9, 12, 13, 14, 15, 25` (yani `CastMoralSupported(11)` satırı `false` kümesinden `true` kümesine taşınır; **`IsAreaMoral(11) == false` satırı aynen kalır**; `IsAreaMoral` denetimlerine dokunma).

b) **Yeni `Combat_AreaFriendCast_Guard`** (dosyanın sonuna, `Combat_Type1Pair_Guard`'dan sonra; Elysian Web `112825`: `Type1 4`, `Moral 11`, `Skill 1128`, `Msp 640`, `CastTime 15`, `ReCastTime 1`, `Range 56`, `UseItem 379062000`, `BeforeAction 0`):
   - Sınıflandırma: `IsAreaFriendMoral(11) == true`; `IsAreaFriendMoral(10) == false`, `(6) == false`, `(12) == false`, `(13) == false`; `SendsAimPoint(11) == true`, `SendsAimPoint(10) == true`, `SendsAimPoint(6) == true`, `SendsAimPoint(4) == false`, `SendsAimPoint(7) == false`, `SendsAimPoint(12) == false`, `SendsAimPoint(13) == false`; `CastMoralSupported(12) == false`, `CastMoralSupported(13) == false`.
   - Skill'in bütün kapıları (Elysian Web açılır): `CastTypesSupported(4, 0) == true`; `CastTypeMoralSupported(4, 11) == true`; `CastItemSkillSupported(4, 1128, 379062000) == true`; `CastConsumeItem(0, 379062000) == 379062000` (`BeforeAction 0` ⇒ `UseItem`'ın kendisi tüketilir); `CastHpCostSupported(0) == true`; `IsFlyingCast(4, 0) == false` (uçmaz). Sınıf skill'i olmayan eşyalı Type4 kapalı kalır: `CastItemSkillSupported(4, 0, 379062000) == false`.
   - Paket alanları: `CastTargetIdField(SendsAimPoint(11), 2990) == -1`; `CastTargetIdField(SendsAimPoint(11), -1) == -1`; `CastCoordField(SendsAimPoint(11), true, 80.9f) == 80` (kendine atış: hedef noktası = çağıranın konumu, `0` değil); `CastCoordField(SendsAimPoint(11), false, 123.7f) == 123` (başka bota atış: hedefin konumu).
   - Başlangıç guard'ı (`CastStartCheck c = {}`; alanlar `Combat_PartyCast_Guard`'daki gibi, `c.skillRange = 56`, `c.msp = 640`, `c.reCastMs = BotCore::CastRecastMs(1)`, `c.typeGated = true`, `c.mana = 640`, `c.distanceM = 0.0f`, `c.standing = true`, `c.needsStanding = false`, `c.actionsInWindow = 0`): `CastRecastMs(1) == 100`; `CheckCastStart(c) == CAST_OK`; `c.mana = 639` ⇒ `CAST_REJECT_NO_MANA`; `c.mana = 640; c.distanceM = 55.9f` ⇒ `CAST_OK`; `c.distanceM = 56.0f` ⇒ `CAST_REJECT_OUT_OF_RANGE`; `c.distanceM = 0.0f; c.hasSkillLast = true; c.sinceSkillLastMs = 99` ⇒ `CAST_REJECT_RECAST`; `c.sinceSkillLastMs = 100` ⇒ `CAST_OK`.
   - Test adı ve enum sabit adları `BotCombat.h`/`Combat_PartyCast_Guard`'daki ile birebir kullanılır (`CAST_OK`, `CAST_REJECT_NO_MANA`, `CAST_REJECT_OUT_OF_RANGE`, `CAST_REJECT_RECAST`); farklıysa dosyadaki adı kullan.
   - Toplam test sayısı **252** (var olan 251 değişmez; yalnızca bir test eklenir). Başlamadan önce `tools/run-tests.sh Release` ile mevcut sayının 251 olduğunu doğrula; farklıysa raporla ve beklenen sayıyı "mevcut + 1" say.

### 5.3 `ActionExecutor.cpp` ve `ActionExecutor.h` (yalnızca yorum)

**a) `TickCast` (`ActionExecutor.cpp:870-872`).** Yalnızca yorum cümlesi değişir; `bool area = ...` satırı aynen kalır:

```cpp
	// ADR-0017 Ek F4-29/F4-31/F4-49: an area-enemy skill (MAGIC.Moral 10), a party-all skill (Moral 6) and an area-friend
	// skill (Moral 11) send target id -1 and the aim point in sData[0..2]; a Moral 4 (single party member) skill sends the
	// ordinary single-target packet.
	bool area = BotCore::SendsAimPoint(m->bMoral);
```

`BeginCast` (`:772-782`, `:811-817`), `SubmitCast`, `CancelCast`, `RejectCast`, `EndCast` ve diğer her şey **değişmez** (`CancelCast` hedef kimliğini `s->m_castTargetId`'den yazar ve o alan `sent.id` ile dolar; `area` artık `Moral` 11 için de `true` olduğundan iptal paketi otomatik `-1` taşır).

**b) `ActionExecutor.h` yorumları.** `CastTarget` açıklamasındaki "For an area skill (MAGIC.Moral 10) or party-all (Moral 6) 'x/y/z' is the aim point" cümlesine "or area-friend (Moral 11)" ekle. `BeginCast` yorumundaki destek listesine "area-friend (Moral 11: Elysian Web; the server buffs every non-hostile unit within the type's Radius of the aim point, ADR-0017 Ek F4-49)" ekle; `TickCast` yorumuna şunu ekle: "area-friend (Moral 11): like area (target id -1, aim point, 'victims' = per-victim EFFECTING packets, docs/03 MEC-MAG-25); a victim that already holds the BuffType is skipped silently, so a repeat cast on the same victims gives 'no_result'." Kod değişmez.

### 5.4 Sonuç sözleşmesi (`docs/03` MEC-MAG-25 ile aynı)

| Durum | Sunucu | Bot sonucu |
|---|---|---|
| **Elysian Web `self`** (hedef `-1`, hedef noktası = çağıranın konumu), çağıranın 15 m çevresinde dost(lar) var | CASTING: `IsAvailable()` ağaç/seviye/eşya denetimi; EFFECTING: MP `Msp 640` **bir kez** (`:1028-1029`); kurban = çağıranın 3×3 bölgesinde `!isHostileTo` ve hedef noktasının 15 m içinde ve çağırandan `< 56` m; her kurban için hedef kimlikli EFFECTING (`BuffType 27`, süre 20); `ConsumeItem()` Stone of Priest −1 | `effected`, `code` = 20 (son paketin süresi), **`victims`** = çağırana gelen kurban kimlikli EFFECTING sayısı (çağıran dahil; aynı ulustan botlar **party'de olmasalar da** sayılır, El Morad botları sayılmaz) |
| Hedef adıyla (`cast <bot> <skill> <hedef>`): hedef noktası = hedef botun konumu | aynı; kurban hedefin 15 m çevresindeki dostlar, **çağıran hedeften > 15 m ise kendisi kurban olmaz** | `effected`, `victims` hedef çevresindekiler; çağıran dışarıda kalabilir |
| Aynı kurbanlarda `BuffType 27` zaten varken (≤ 20 sn) tekrar atış | kurbanlar **sessizce atlanır** (`bSkillTypeAlreadyOnTarget && isBuff` ⇒ `continue`), `ExecuteType4` yine `true`: MP yine düşer, **Stone yine azalır** `[D]` | `no_result` (EFFECTING'te hedef kimlikli paket yok) |
| Bazı kurbanlar buff'lı, bazıları değil | buff'sızlar buff alır, buff'lılar atlanır | `effected`, `victims` yalnızca buff alanlar |
| Yarıçapta çağıran dahil kimse uygun değil (çağıran blinking, arena, çağıran dışarıda ve kimse yok) | `casted_member` boş, çağıran oyuncu, `Moral != 6` ⇒ `SendSkill(); return false` ⇒ `ConsumeItem()` **çalışmaz**; MP yine düşmüştür | `effected`/`no_result` yayın biçimine göre (`SendSkill` hedef `-1` EFFECTING yayını `effected`, `victims 0` verebilir `[Ö]`; ölçülürse `docs/05`'e işlenir) |
| Hedef noktası `>= Range` (56 m) | bot guard EFFECTING/CASTING öncesi reddeder | `REFUSED`/`out_of_range`, paket gitmez |
| `BotMF_K` (mage) Elysian Web | `BeginCast`: `m_sClass != sSkill / 10` | `REFUSED`/`bad_skill`, paket gitmez |
| Çantada Stone of Priest yok | `BeginCast` `no_item` ön kontrolü (`CanUseItem`) | `REFUSED`/`no_item`, paket gitmez |
| CASTING'te `cast <bot> off` | iptal paketi hedef `-1` (`m_castTargetId` = `sent.id`) | `cancelled` (`op:4`, `code:-100`), MP düşmez |
| Ağaç yetersiz (usta ağacı < 20) | `IsAvailable()` ağaç denetimi ⇒ CASTING'te `MAGIC_FAIL` | `srv_fail`, MP düşmez (KI-016; bot priest'lerinde ağaç 20, tetiklenmez) |

`reason` metinleri, `CastOutcome::Kind` değerleri ve `TickCast` akışı bu tablo için **değişmez**; yeni çıktı yoktur.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (değişen dört dosya için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h`'yi içeren `.cpp`'ler de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastMoral_Supported` ve `Combat_AreaFriendCast_Guard` adlarını `[ OK ]` ile içerir ve toplam test sayısı **252**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-49 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş.
- [ ] K5: `ActionExecutor.cpp` **kod olarak değişmez**: `git diff gece/2026-10-02...bot/F4-49 -- GameServer/Bot/ActionExecutor.cpp | grep '^[+-]' | grep -v '^+++\|^---' | grep -v '^[+-][[:space:]]*//'` boş (yalnızca yorum satırları değişir); `grep -n "bool area" GameServer/Bot/ActionExecutor.cpp` `BotCore::SendsAimPoint(m->bMoral)` gösterir; `grep -n "IsAreaMoral\|IsAreaFriendMoral" GameServer/Bot/ActionExecutor.cpp` boş. `ActionExecutor.h` için aynı denetim (yalnızca yorum satırları).
- [ ] K6: gerilemesiz: `git diff gece/2026-10-02...bot/F4-49 --stat` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `tests/BotCoreTests/BotCoreTests.vcxproj`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*` değişmemiş; önceki 251 testin tamamı (güncellenen biri dahil) hâlâ geçiyor.
- [ ] K7: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı türü ve alanı yok (`git diff ... | grep '^+' | grep 'Emit('` boş).
- [ ] K8: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); dört dosya CRLF kalır (ASCII); `git diff --check` boş.
- [ ] K9: `python3 tools/check-perception-contract.py` `RESULT: PASS` (`R1`..`R5` ihlal sayıları 0; denetlenen dosya sayısı değişmez).
- [ ] K10 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-49
git diff gece/2026-10-02...bot/F4-49 -- BotCore/BotCombat.h tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff gece/2026-10-02...bot/F4-49 -- GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotManager.cpp GameServer/Bot/Telemetry.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -rn "CastMoralSupported\|SendsAimPoint\|IsAreaMoral\|IsAreaFriendMoral" BotCore GameServer tests
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff --check gece/2026-10-02...bot/F4-49
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; komut başına ≥ 1 sn). Botlar (zone 71): Karus priest **`BotPHD_K`** (çağıran; usta ağacı 20), Karus priest **`BotPHB_K`**, Karus **`BotWP_K`** (aynı ulustan dost; **party'ye alınmaz**: Moral 11 party aramaz), Karus **`BotMF_K`** (`bad_skill` için), El Morad **`BotWP_E`** (düşman; kurban olmamalı). Önce `list` ile konum, HP/MP ve ölü durumunu denetle; botlar **spawn'dan en az 15 sn sonra** denenir (blinking kurban/çağıranı listeden düşürür `[A]`); botlar güvenli bölgede başlıyorsa her cast `srv_fail` verir (F4-29/F4-30 bulgusu): hepsini yarıçap (≤ 10 m) içine `/bot move` ile topla ve mesafeyi `list` konumlarından doğrula; ayrıca bir botu çağıranın 25–30 m ötesine yerleştir (hedef noktası sınaması). MP `list`'ten (cast'ten hemen önce ve sonra; yenileme payını raporla), buff `snap <bot>`'tan (her kurbanın kendi `self` buff listesi), olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'tan. Stone of Priest tüketimi `snap`'te sayaç olmadığı için **ölçülemez `[Ö]`** (F4-46 ile aynı sınır; yasak DB tablolarına bakılmaz). Senaryolar:

- **S1:** `cast BotPHD_K 112825 self`: paket hedef kimliği `-1`, `sData[0]`/`sData[2]` çağıranın koordinatı; `effected`, `code 20`; `victims` = çağıran + 15 m içindeki Karus botları (party'de olmasalar da); El Morad botu `victims`te yok ve buff listesinde `BuffType 27` yok; MP bir kez −640; her kurbanın `snap`'inde Elysian Web (BuffType 27) görünür.
- **S2:** S1'in hemen ardından (≤ 20 sn) aynı atış: `no_result` (kurbanların hepsi buff'lı), MP yine −640.
- **S3:** hedef adıyla atış: çağıranın ≈ 25 m ötesindeki `BotWP_K`'ya (`cast BotPHD_K 112825 BotWP_K`, `BotWP_K` çağırandan `< 56` m): paket hedef noktası `BotWP_K`'nın konumu; çağıran hedeften > 15 m olduğu için **kurban değil** (`snap BotPHD_K`'da yeni buff yok), `BotWP_K` ve onun 15 m çevresindekiler kurban; `effected`.
- **S4:** `>= 56` m uzaktaki hedef noktası: `REFUSED`/`out_of_range`, paket yok.
- **S5:** `cast BotMF_K 112825 self`: `REFUSED`/`bad_skill`. `cast BotPHB_E 212825 self` (El Morad simetrisi, aynı kurulumla): `effected`.
- **S6:** CASTING'te (1,5 sn pencere) `cast BotPHD_K off`: iptal paketi hedef `-1`, `cancelled`, MP düşmez.
- **S7 (gerilemesizlik):** `Moral` 6 grup heal `112557` (BotPHB_K, party'li) ve `Moral` 10 alan `110545` hâlâ F4-31/F4-29 sonuçlarını verir.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları; dört dosya ASCII + CRLF + tab; kod yorumları İngilizce.
- **Davranış tek yerde değişir:** `SendsAimPoint(11)` `true` olur. `ActionExecutor.cpp` içinde mantık değişikliği **yoktur**; yalnızca yorum. Başka bir değişiklik gerekiyorsa dur ve raporla.
- `IsAreaMoral`'ı `11`'i kapsayacak biçimde **genişletme**: `IsAreaMoral` yalnızca `Moral` 10'dur (`CombatTests.cpp:1204` `IsAreaMoral(11) == false` ve `Combat_AreaCast_Fields` testleri buna dayanır); `SendsAimPoint` ayrı bir kavramdır.
- **Thread kuralı:** yeni thread/kilit yok; `BotCore/` saf mantıktır (ADR-0016): `windows.h`/`stdafx.h`/`GameServer`/`shared` içermez.
- **Sunucu davranışı, bot kapalıyken değişmez:** `[BOT] ENABLED=0` varsayılan; bu plan yalnızca bot cast kapısını açar.
- MEC-MAG-25 yorumu: `no_result`/`victims` yalnızca gözlemdir; hüküm vermek için kullanılmaz (hasar azaltma etkisi ölçülmez: algı tarafı F4-53, etki ölçümü T-MECH-DMG'dir).
- Bu plana özgü riskler: (a) `Moral` 11 party aramaz; botun "yalnız party'ye buff" beklentisi yanlıştır, karar katmanı bunu bilmelidir (alan-dost = ulusun tümü). (b) Tekrar atışta taş yine harcanır (MEC-MAG-25 `[D]`); F7 karar katmanı 20 sn içinde tekrarı önlemelidir. (c) Çağıran blinking iken atış kurbansız kalabilir (`ConsumeItem` çalışmaz, MP gider).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-49` (taban: `gece/2026-10-02` @ `5d2c444`) — `04871fc` `[F4-49] Alan-dost skill (Moral 11) acildi; Elysian Web kapilari ve guard testleri`, `31a4669` `[F4-49] Birim testleri: CastMoralSupported(11) ve Combat_AreaFriendCast_Guard (252 test)`, ardından bu rapor/kayıt commit'i.
- Değişen dosyalar ve neden:
  - `BotCore/BotCombat.h`: `kMoralAreaFriend = 11` + `IsAreaFriendMoral`; `SendsAimPoint` 11'i kapsayacak biçimde genişletildi; `CastMoralSupported` yorumu ve `CastTargetIdField`/`CastCoordField` yorumları güncellendi (`IsAreaMoral` değişmedi).
  - `tests/BotCoreTests/CombatTests.cpp`: `Combat_CastMoral_Supported` (11 → `true`); yeni `Combat_AreaFriendCast_Guard`; `Combat_ItemSkill_Guard` içindeki eski `CastMoralSupported(11) == false` satırı `true` yapıldı (bkz. sapmalar).
  - `GameServer/Bot/ActionExecutor.cpp`: **yalnızca** `TickCast` `bool area` üstündeki yorum (`F4-29/F4-31/F4-49`).
  - `GameServer/Bot/ActionExecutor.h`: **yalnızca** `CastTarget`, `BeginCast` ve `TickCast` yorumları (kod yok).
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
  BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  Değişen dört dosya için uyarı/hata yok. `tools/build.sh Debug` de hatasız.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (Release rc=0, değişen dosyalarda uyarı yok; `ActionExecutor.h`'yi içeren `.cpp`'ler yeniden derlendi)
  - K2 ✔ (Debug rc=0)
  - K3 ✔ (`run-tests.sh Release` ve `Debug`: `252 tests, 0 failed`; `Combat_CastMoral_Supported` ve `Combat_AreaFriendCast_Guard` `[ OK ]`)
  - K4 ✔ (yasak include yok; yalnızca `<algorithm>`, `<cstdint>`; eklenen `std::min`/`std::max` yok)
  - K5 ✔ (iki dosyada da yalnızca yorum satırları değişti; `bool area = BotCore::SendsAimPoint(m->bMoral);` aynen; `ActionExecutor.cpp`'de `IsAreaMoral`/`IsAreaFriendMoral` yok)
  - K6 ✔ (`git diff --stat gece/2026-10-02...bot/F4-49` yalnızca dört dosya; `.vcxproj*`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*` değişmedi; önceki testler geçiyor)
  - K7 ✔ (yeni ini/komut/thread/telemetri olayı yok; eklenen `Emit(` yok)
  - K8 ✔ (`file`: dört dosya ASCII + CRLF, değişmedi; `git diff --check` boş)
  - K9 ✔ (`python3 tools/check-perception-contract.py` → `RESULT: PASS`, R1..R5 ihlal 0, taranan dosya 31)
  - K10 (çalışma zamanı S1–S7) — `/plan-dogrula`'da Claude'da.
- Plandan sapmalar ve gerekçeleri:
  1. Plan §5.2(a) yalnızca `Combat_CastMoral_Supported`'taki `CastMoralSupported(11) == false` satırını (planın dediği `:1197`) güncellemeyi söylüyordu; ancak `Combat_ItemSkill_Guard` içinde (`:1679`) ikinci bir `CastMoralSupported(11) == false` satırı vardı ve bu, planın kendi amacıyla (`Moral 11` artık destekli) doğrudan çelişiyordu. K6'nın "önceki testler geçmeli" koşulu bu satır güncellenmeden sağlanamayacağından satır `true` yapıldı. Değişiklik test beklentisindedir, oyun mekaniğinde / paket biçiminde değil; yalnızca izinli dosyada.
- Açık sorular: yok. (Çalışma zamanı doğrulaması ve `docs/03` MEC-MAG-25 işlemesi Claude'dadır.)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-49` @ `1cb22bb` (taban `5d2c444`; üç commit: `04871fc`, `31a4669`, `1cb22bb`; hepsi `[F4-49] ...` biçiminde, merge/rebase/force izi yok; `build/` commit'li değil)
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `./tools/build.sh Release` rc=0; dört dosya `touch` edilip yeniden derlendi (`/tmp` günlüğü), `warning` satırı 0 |
| K2 | ✔ | `./tools/build.sh Debug` rc=0, `warning` satırı 0 |
| K3 | ✔ | `run-tests.sh Release`: `252 tests, 0 failed`; `Debug`: `252 tests, 0 failed`; iki yapılandırmada `[ OK ] Combat_CastMoral_Supported` ve `[ OK ] Combat_AreaFriendCast_Guard` |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` boş; `#include` yalnızca `<algorithm>`, `<cstdint>` (`BotCombat.h:6-7`); eklenen satırlarda `std::min`/`std::max` yok |
| K5 | ✔ | `ActionExecutor.cpp` ve `.h` farkında yorum olmayan `+`/`-` satırı yok (boş çıktı); `ActionExecutor.cpp:873` `bool area = BotCore::SendsAimPoint(m->bMoral);` aynen; `ActionExecutor.cpp`'de `IsAreaMoral`/`IsAreaFriendMoral` yok (yalnızca `:741` yorumunda `CastMoralSupported`, `:779` ve `:873` mevcut çağrılar) |
| K6 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-49`: `BotCombat.h`, `ActionExecutor.cpp`, `ActionExecutor.h`, `Tests/BotCoreTests/CombatTests.cpp` + plan dosyası; `.vcxproj*`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*` değişmedi; plan dosyasında yalnızca `Durum` ve Uygulayıcı Raporu (`@@ -2` ve `@@ -212`); önceki 251 test (güncellenen biri dahil) geçiyor |
| K7 | ✔ | koda eklenen `Emit(`/ini anahtarı/komut/thread yok; kod farkı `kMoralAreaFriend`, `IsAreaFriendMoral`, `SendsAimPoint`/yorumlar |
| K8 | ✔ | `file`: dört dosya `ASCII text, with CRLF line terminators`; satır sayısı = CRLF sayısı (1074/1841/404/3267), ASCII dışı bayt 0; `git diff --check` boş |
| K9 | ✔ | `python3 tools/check-perception-contract.py` `RESULT: PASS`, `files scanned: 31` |
| K10 | ✔ | Çalışma zamanı S1–S7 aşağıda; hepsi beklenenle uyumlu |

- **Çalışma zamanı (K10)** (Release, `[BOT] ENABLED=1 MAX_BOTS=16 TELEMETRY=decisions`, zone 71; `Logs/bots/2026-10-03/live-110215.jsonl`, `Logs/Bot_3_10_2026.log`; çantalar sunucu kapalıyken `bot-refill.sh apply --mp-pots 20` (`ok=12 fail=0`); ini yedekten geri yüklendi (`cmp` aynı), `BotCommands.txt` silindi, botlar `despawn all`, sunucular `stop`). Botlar: `BotPHD_K` (1274,0 / 928,0; çağıran), `BotPHB_K` (1270,0 / 944,0 → 936,0), `BotWP_K` (1272,0 / 934,0 → 1274,0 / 953,0), `BotWP_E` (1274,0 / 928,5; düşman), `BotMF_K` (908,0 / 1150,0 → 1274,0 / 905,0), `BotPHB_E` (1274,0 / 890,0 → 922,0). Hepsi ilgili atıştan ≥ 25 sn önce spawn edildi (blinking süresi geçmiş: kurban/çağıran listeye girdi); `list` mesafeleri `snap`'ten teyit edildi. Hiçbir bot party'de değildi (S7 hariç).
  - **S1 ✔** `cast BotPHD_K 112825 self 1`: `CastStart` `ACTION_SUBMIT` **`"target":-1`**, `casting` `op 1`; `CastEffect` `"target":-1` → `effected`, `op 3`, **`code 20`**, **`victims 2`**, `mp_after` 3692 → 3052 (**−640 tam**); `snap BotPHD_K events`: `op=1 target=-1 d0=1274 d2=928` (hedef noktası = çağıranın konumu), `op=3 target=2986` (BotWP_K, 6,3 m) ve `target=2984` (çağıran) `d1=1`; **`snap`**: `BotPHD_K` ve `BotWP_K`'da `buff skill=112825 type=27 remain=14s/12s`; `BotPHB_K` (16,5 m, yarıçap dışı) ve düşman `BotWP_E` (0,5 m, hostile) buff'sız `buffs 0`. `BotPHB_K` 8,9 m'ye alınıp tekrar atışta (buff'lar sönmüşken): `victims 3` (çağıran + `BotWP_K` + `BotPHB_K`; party'siz), MP −600 (−640 + ~40 yenileme), üçünde de `type=27`; `BotWP_E` yine buff'sız. **Party aranmadığı kanıtlandı** (hiçbiri party'de değil).
  - **S2 ✔** S1 tekrarının 4 sn sonrası aynı atış: `ok:false`, `reason:"no_result"`, `op -1`, `victims 0`, log `cast stopped (no_result)`; **MP 2732 → 2132 (−600 = −640 + ~40 yenileme)**: MP yine düşer (MEC-MAG-25 `[D]` ⇒ `[V]`).
  - **S3 ✔** `BotWP_K` çağırandan 25,0 m (`snap`), `BotPHB_K` `BotWP_K`'dan 17,5 m: `cast BotPHD_K 112825 BotWP_K 1`: `target -1`, `op=1 d2=953` (hedef botun konumu), `effected`, **`op=3` yalnızca `target=2986`**; `snap`: `BotWP_K` `type=27 remain=15s`, `BotPHD_K` (çağıran, hedeften 25 m) ve `BotPHB_K` buff'sız ⇒ **çağıran hedeften > 15 m iken kurban değil** (plan §5.4 satır 2).
  - **S4 ✔** `cast BotPHD_K 112825 BotMF_K 1` (428 m): paket gitmeden `FAIRNESS_REJECT` `rule MEC-MAG-11`, `out_of_range`, `value 428.07`, `limit 56.00`, log `cast stopped (out_of_range)`; MP değişmedi (yalnızca yenileme).
  - **S5 ✔** `cast BotMF_K 112825 self 1` ⇒ `refused (bad_skill)`, jsonl'de `BotMF_K` için `CastStart` yok. El Morad: `cast BotPHB_E 212825 self 1` ⇒ `CastStart` `target -1`, `effected`, `op=3` `target=2989` (çağıran) ve `2988` (`BotWP_E`, 6,5 m), `snap`: ikisinde `skill=212825 type=27`; Karus botundan `BotPHD_K` (6,0 m, yarıçap içinde ama düşman) `snap`'te `buffs 0` (`BotPHB_K` bu adımda `snap` edilmedi).
  - **S6 ✔** `cast BotPHD_K 112825 self 1`, ~1,1 sn sonra ayrı dosyada `cast BotPHD_K off`: `CastCancel` `"target":-1`, `cause "cmd"`, `since_casting_ms 1096`, `cancelled`, `op 4`, `code -100`; log `cast cancelled after 2 packet(s) sent`; MP 2612 → 2692 (yalnızca yenileme, düşüm yok).
  - **S7 ✔** `BotPHB_K` + `BotPHD_K` party (`pinvite`/`paccept`): `cast BotPHB_K 112557 self 1` (Moral 6 grup heal) ⇒ `CastStart`/`CastEffect` `target -1`, `effected`, `code 0`, `victims 2`, MP −960 (+ ~40). Moral 10 alan `cast BotMF_K 110545 BotWP_E 1` ⇒ `target -1`, `effected`, `victims 2` (El Morad `BotWP_E` 4973 → 4732 ve `BotPHB_E` 3491 → 3242 HP düştü), MP 6021 → 5821 (−200).
  - Ölçülemeyenler `[Ö]`: Stone of Priest tüketimi (`snap` stok satırı yalnızca `hp_pot`/`mp_pot`), boş kurban listesinde `ConsumeItem()`'ın çalışmaması (§5.4 satır 5; blinking/kimsesiz atış denenmedi), `no_item` ve `srv_fail` (ağaç yetersiz) kolları (bot priest'lerinde tetiklenmez).
- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. *(not)* Uygulayıcı sapması yerinde: `Combat_ItemSkill_Guard` içindeki ikinci `CastMoralSupported(11) == false` satırı (`CombatTests.cpp:1679`) `true` yapıldı; planın §5.2(a) yalnızca ilk satırı anıyordu, ancak K6 ("önceki testler geçmeli") ve planın amacı bunu gerektiriyordu; yalnızca izinli test dosyasında, mekanik/paket değişikliği yok.
  2. *(not)* Plan yolu `tests/BotCoreTests/...` yazıyor, depodaki yol `Tests/BotCoreTests/...` (büyük `T`); içerik ve kapsam etkilenmedi.
  3. *(not, kurulum)* `BotCommands.txt` sunucuda saniyede bir okunuyor; aynı dosyaya art arda yazılan komutlar bir önceki alınmadan üzerine yazılırsa kaybolur. `cast ... off` aynı dosyada (aynı tick) `cast`'ten hemen sonra verilirse CastStart paketi gitmeden iptal olur (`stopped after 0 packet(s)`, paket yok); CASTING'te iptal sınaması için iki komut ~1,1 sn arayla **ayrı dosyalarla** verilmeli (F4-31 `since_casting 1094` ile aynı).
  4. *(not)* `snap <bot>` olaylarındaki `op=1 d0/d2` hedef noktasını doğrudan gösteriyor; telemetri (`ACTION_SUBMIT`) hedef noktası alanı taşımıyor, bu plan kapsamında yeni alan gerekmiyor (CLI-07 gerçek istemci paketi `[Ö]` insan testi, `docs/STATUS.md` "isteğe bağlı gözlemler").
  5. *(not)* Uygulayıcı raporundaki tüm iddialar (commit listesi, dosyalar, `252 tests`, K1–K9) kendi çalıştırmamla örtüşüyor.
- Düzeltme talimatı: yok (`DOĞRULANDI`).
