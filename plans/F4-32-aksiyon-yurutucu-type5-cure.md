# F4-32: `ActionExecutor` Type5 cure dilimi — Cure curse ve Cure disease (`Moral` 2, dost tek hedef)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-32` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-28 (Type4 tek tipli: `Moral` 2 yolu, MEC-MAG-15) — `KAPANDI`; F4-31 (`CastMoralSupported`, `CastHpCostSupported`, `BeginCast` destek koşulu) — `KAPANDI` (merge `78759b5`) |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-11, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-15, MEC-MAG-19 (bu planla eklendi, `[D]`), SK-06, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 Ek 1 madde 1(a) (Type5 cure), `docs/17` §2.1 "Priest cure", T-PRI-05 altyapısı |
| Tahmini büyüklük | S (4 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.cpp`, `ActionExecutor.h`; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

Priest'in debuff temizleme işi botla yapılamıyor ve F7 (healer) bunsuz yazılamaz: **Cure curse** `112525` (tüm Type4 debuff'larını kaldırır: yavaşlatma, direnç düşürme vb.) ve **Cure disease** `112535` (tüm DoT'ları kaldırır: Blaze, Hell fire, Ignition...). Bunlar `MAGIC.Type1 = 5` skill'leridir; `CastTypesSupported(5, 0)` `false` döndürdüğü için `BeginCast` bunları `unsupported_skill` ile reddediyor (`BotCore/BotCombat.h:321-327`). ADR-0018 Ek 1 madde 1(a) Type5 cure'ü dilim 6'nın ilk alt dilimi olarak ister; diriltme (`Moral` 25 + `UseItem`), Bless of God (`Moral` 6) ve Type8 sonraki alt dilimlerdir.

Sunucu tarafında **yeni akış gerekmez**: Type5 `Moral` 2 skill'i, F4-28'in tek hedefli Type4 paketiyle aynı biçimde gider (hedef kimliği = dostun ya da kendinin kimliği, koordinat `self`'te 0). Bu plan:

1. `CastTypesSupported`'a `{5, 0}` çiftini ekler;
2. Type5'i yalnızca `Moral` 2 ile açar (`CastTypeMoralSupported`): Bless of God `112671` (`Moral` 6, grup yolu `UserRegionCheck`'i `sRange` yarıçapıyla kullanır, botlarda erişilemez ve doğrulanamaz) ve diriltmeler (`Moral` 25) kapalı kalır;
3. sunucu davranışını kodla belgeler ve çalışma zamanında doğrular (`docs/03` MEC-MAG-19).

F4'ün otuz ikinci planıdır (ADR-0018 sırası: ... party hedefli ✔ → **Type5 cure (bu plan, dilim 6a)** → diriltme (6b) → Type8 (6c/6d) → `UseItem` (6e) → CLI-12 → envanter doldurma).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-32)" (bu planla birlikte yazıldı), "Ek (F4-28)", "Ek (F4-31)". `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (Ek 1 madde 1, Ek 8).
- `docs/03` §4.2 **MEC-MAG-15** (tek hedefli Type4), **MEC-MAG-19** (bu planla eklendi), `docs/05` priest tablosu (satır 120-121: Cure curse, Cure disease).
- `plans/F4-28-aksiyon-yurutucu-type4-tek-tipli-skill.md`, `plans/F4-31-aksiyon-yurutucu-party-hedefli-skill.md` (aynı fonksiyonlar; **yazılı planı değil, kodu esas al**).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `78759b5` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.cpp:1898-2063` `ExecuteType5()`: oyuncu olmayan çağıran `false`; `sTargetID != -1` ise `pSkillTarget` oyuncu olmalı, **ölü hedef** (diriltme değilse) ⇒ `return false` (`:1943-1947`, yayın yok); kurban başına `switch (pType->bType)`: `REMOVE_TYPE3 = 1` (`:1970-2002`, `m_sHPAmount >= 0` olan HoT'lar korunur, DoT'lar `Reset()` + hedefe `MAGIC_DURATION_EXPIRED` 200), `REMOVE_TYPE4 = 2` (`:2004-2025`, yalnızca `isDebuff()` olan Type4 kayıtları kalkar, buff'lara dokunulmaz). Sonda `bType[1] == 0 || 5` ise çağırana `sData[1] = 1` ile **bölgeye** `BuildAndSendSkillPacket(..., bOpcode, ...)` yayını (`:2054-2059`): hedef kimliği = kurban, `bOpcode` = EFFECTING. Kaldırılacak bir şey **olmasa da** yayın gider (`:2054` koşulsuz), yani cure "boşa" atılsa bile bot sonucu `effected`'dir.
  - `GameServer/MagicInstance.h:55-59`: `REMOVE_TYPE3 1`, `REMOVE_TYPE4 2`, `RESURRECTION 3`, `RESURRECTION_SELF 4`, `REMOVE_BLESS 5`.
  - `GameServer/MagicInstance.cpp:819-823` `MORAL_FRIEND_WITHME` (`IsAvailable()`): hedef çağıranın kendisi değilse ve `isHostileTo(hedef)` ise `MAGIC_FAIL` (CASTING'te `srv_fail`); dost (aynı millet) için party gerekmez. `:1028-1029`: EFFECTING'te `bType[0] != 4 || sTargetID == -1` ⇒ Type5'te MP `Msp` **bir kez** düşer (Type4 tek hedefli gibi "yalnızca başarıda" kuralı Type5'e uygulanmaz). `:166` (`isDead() && bType[0] != 5`): ölü çağıranın Type5 kullanabilmesi diriltme içindir; bot ölüyken `BeginCast` zaten reddeder/uğraşmaz (bu plan değiştirmez).
  - `GameServer/MagicInstance.cpp:389-405`: aynı tip kapısı Type 1..7 için geçerlidir (`PLAYER_SKILL_REQUEST_INTERVAL = 0.7`, `User.h:23`); Type5 de kapıdadır (`BotCore::IsGatedType` `1..7` zaten kapsar, **değişmez**). `:121-125`: başarıda `bType[0]` damgalanır.
  - `GameServer/Bot/BotSession.cpp:72-86` cast yankısı: yalnızca çağıranın kendi paketleri; Type5 yayını çağırana `op 3`, `sData3 = 0` ile gelir ⇒ mevcut `SubmitCast` `effected`, `code 0` üretir (`ActionExecutor.cpp:646-654`). `OnPacket()`/`SubmitCast` **değişmez**.
  - `GameServer/Bot/ActionExecutor.cpp` (`78759b5`): `BeginCast` destek koşulu `:733-738`; `bad_target` kuralı `:755-763` (`MORAL_SELF` ⇒ ad yasak, `MORAL_ENEMY` ⇒ ad zorunlu; `Moral` 2 ikisini de kabul eder); `typeGated` hesabı `:859-861` (her iki tip `IsGatedType` ile), `m_castTypeHas[ty]` damgası `ty < 8` (Type5 dahil).
  - `BotCore/BotCombat.h:317-327` (`CastTypesSupported`), `:357-366` (`CastMoralSupported`); `Tests/BotCoreTests/CombatTests.cpp` `Combat_CastTypes_Supported` (`:1019`, `:1028` `CastTypesSupported(5, 0) == false` satırı değişir), `Combat_PartyCast_Guard` (`:1292`, dosyanın son testi). Toplam birim test **108** (F4-31 sonrası).
- **Veri notu (Claude yerel `MAGIC`/`MAGIC_TYPE5` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** Bot sınıflarında (`106`, `110`, `112`, `2xx` karşılıkları) `Type1 = 5` skill'leri:
  - **`Moral` 2, `UseItem 0` (açılacaklar):** `112525` Cure curse (`SkillLevel 25`, `Skill 1125`, `Msp 60`, `CastTime 15`, `ReCastTime 15`, `Range 56`, `UseStanding 0`, `Etc 0`, MAGIC_TYPE5.Type `2` = `REMOVE_TYPE4`); `112535` Cure disease (`SkillLevel 35`, `Msp 120`, aynı süreler, MAGIC_TYPE5.Type `1` = `REMOVE_TYPE3`). El Morad karşılıkları `212525`, `212535`.
  - **Açılmayacaklar:** `112671` Bless of God (`Moral` 6, `SkillLevel 70`, `UseStanding 54`; botların 1126 ağacı ≤ 62 ⇒ erişilemez; grup yolu kurban seçimini `sRange` = 45 yarıçapıyla yapar, `ExecuteType5` `:1912-1935`); `112733`/`112742`/`112754` Resurrection (`Moral` 25, `UseItem 379006000`, `NeedStone 4/10/30`: dilim 6b); `112805` Absoluteness (`MAGIC.Type1 = 0`: Type5 değil, `CastTypesSupported(0, 0)` `false`).
  - Bot sınıflarında başka `Type1 = 5` skill'i yoktur (warrior `106`, mage `110`, rogue yok: sorgu `MAGIC JOIN MAGIC_TYPE5`, sınıf `106/110/112/206/210/212`).
- **Bot karakter notu (`db/002_bot_characters.sql:128-135`).** `BotPHB_*` ve `BotPHD_*` ikisinde de `1125` (iyileştirme) ağacı **60** ⇒ Cure curse (25) ve Cure disease (35) ikisinde de atılabilir (Karus `BotPHB_K`/`BotPHD_K`, El Morad `BotPHB_E`/`BotPHD_E`). Debuff üretmek için düşman mage'ler: `BotMF_E` (ateş ağacı `1105` = 70: Blaze `210509` `Msp 30`, Hell fire `210539` `Msp 150`, hepsi `Moral` 7 DoT) ve `BotMI_E` (buz ağacı `1106` = 70: Chill `210609` `{3, 4}` `Msp 30`, hasar + yavaşlatma Type4 debuff'ı). Skill numarası El Morad'da `2xxxxx`'tir (Karus `1xxxxx`).

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `CastTypesSupported`'a `{5, 0}` eklenir; yeni `CastTypeMoralSupported(type0, moral)` (Type5 yalnızca `Moral` 2; diğer tipler değişmez). Yorumlar güncellenir (§5.1).
2. **`BeginCast` (`ActionExecutor.cpp`):** destek koşuluna `CastTypeMoralSupported(m->bType[0], m->bMoral)` eklenir; başka koşul değişmez.
3. **Yorumlar (`ActionExecutor.h`):** `BeginCast` açıklaması Type5 cure'ü kapsar (§5.3).
4. **Birim testleri:** `Combat_CastTypes_Supported` güncellenir; yeni `Combat_CureCast_Guard` eklenir (108 → 109).
5. **Sonuç sözleşmesi (§5.4):** Type5 `Moral` 2 akışı belgelenir (`docs/03` MEC-MAG-19) ve çalışma zamanında sınanır.

**Kapsam dışı (yapılmayacak)**

- `Moral` 6 Type5 (Bless of God), `Moral` 25/26 diriltme (`UseItem`, taş tüketimi), `RESURRECTION_SELF`, `REMOVE_BLESS`, `{5, x}` çiftleri, Type8, `Moral` 1 Type5 (veride yok), `UseItem != 0` skill'leri: hepsi ayrı alt dilimler (ADR-0018 Ek 1 madde 1).
- **Karar katmanı:** kimin debuff'lı olduğu, hangi cure'ün atılacağı (kaldırılacak Type4 mi DoT mu), ne zaman atılacağı (F7, `docs/07` T-PRI-05). Bota **hedefte debuff var mı önkontrolü eklenmez** (AC-LRN-03: `SelfState.buffs` ve gözlenen durum algıdan okunur, karar katmanı kullanır; boşa atılan cure sunucuda başarılıdır ve MP düşer).
- MAGIC_TYPE5 tablosunu okumak (alt tip ayrımı): gereksiz; `Moral` 2 + `UseItem 0` + veri bot sınıflarında yalnızca iki cure verir.
- Yeni komut/ini anahtarı/telemetri olayı türü/alan, `BotSession.*`/`BotManager.cpp` değişikliği, `tools/` betik değişikliği.
- Cure'ün etkisini ölçen algı (`F4-53` gözlenen durum tablosu), hedef debuff'ını sorgulamak için paket.
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | `CastTypesSupported` `{5, 0}`; yeni `CastTypeMoralSupported`; yorumlar |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | `Combat_CastTypes_Supported` güncellenir; `Combat_CureCast_Guard` eklenir (108 → 109) |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yalnızca `BeginCast` destek koşulu (`:733-738`) |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca `BeginCast` yorumu |

`BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `ScenarioRunner.*`, `ScriptRunner.*`, `tools/` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

a) `CastTypesSupported` ve üstündeki yorum (`:317-327`) şöyle olur (ASCII, CRLF, tab, `namespace BotCore` içinde; yeni `#include` yok; **`std::min`/`std::max` kullanma**):

```cpp
	// MAGIC.Type1/Type2 pairs the bot casts (docs/03 MEC-MAG-13, MEC-MAG-15, MEC-MAG-19): a single type 1, 3, 4 or 5 (5 =
	// cure, see CastTypeMoralSupported), or the pair Type3 + Type4 (the server runs Type3 first and Type4 second on the
	// same target). Every other pair stays unsupported.
	inline bool CastTypesSupported(uint8_t type0, uint8_t type1)
	{
		if (type1 == 0)
			return type0 == 1 || type0 == 3 || type0 == 4 || type0 == 5;

		return type0 == 3 && type1 == 4;
	}

	// Type5 (cure, resurrection) is opened only for MAGIC.Moral 2 (friend-with-me, single target: Cure curse, Cure disease;
	// F4-32). The party-all cure (Bless of God, Moral 6) and the resurrections (Moral 25, MAGIC.UseItem) stay closed.
	// Every other type passes through; its Moral is checked by CastMoralSupported.
	inline bool CastTypeMoralSupported(uint8_t type0, uint8_t moral)
	{
		return type0 != 5 || moral == 2;
	}
```

b) Dosyanın kalanı **değişmez** (`CastMoralSupported`, `IsGatedType` (1..7 zaten Type5'i içerir), `CastHpCostSupported` aynen). `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

a) **`Combat_CastTypes_Supported` (var olan test, `:1019`) güncellenir:** `CHECK_EQ(BotCore::CastTypesSupported(5, 0), false)` satırı `true` olur ve `true` grubuna taşınır (yanındaki `(1, 0)`, `(3, 0)`, `(4, 0)` satırlarının altına). Aşağıdaki `false` satırları aynen kalır; **şunlar eklenir** (yoksa): `CastTypesSupported(5, 4) == false`, `CastTypesSupported(5, 3) == false`, `CastTypesSupported(5, 5) == false`, `CastTypesSupported(4, 5) == false`, `CastTypesSupported(3, 5) == false` (var olan satır), `CastTypesSupported(6, 0) == false`, `CastTypesSupported(7, 0) == false`, `CastTypesSupported(8, 0) == false`.

b) **Yeni `Combat_CureCast_Guard`** (dosyanın sonuna, `Combat_PartyCast_Guard`'dan sonra; Cure curse `112525`: `Msp 60`, `CastTime 15`, `ReCastTime 15`, `Range 56`):
   - Tip/moral kapısı: `CastTypeMoralSupported(5, 2) == true`; `CastTypeMoralSupported(5, 1) == false`, `(5, 4) == false`, `(5, 6) == false`, `(5, 7) == false`, `(5, 25) == false`, `(5, 0) == false`; diğer tiplerde moral kapıya girmez: `CastTypeMoralSupported(3, 6) == true`, `(3, 10) == true`, `(4, 4) == true`, `(1, 7) == true`, `(3, 25) == true`, `(4, 0) == true` (bu fonksiyon yalnızca Type5'i kısıtlar; `CastMoralSupported` ayrıca denetler).
   - Birleşik karar (BeginCast'in iki fonksiyonu): Cure curse/disease (`type0 5`, `type1 0`, `moral 2`): `CastTypesSupported(5, 0) && CastTypeMoralSupported(5, 2) && CastMoralSupported(2) == true`; Bless of God (`5, 0`, `moral 6`): `CastTypesSupported(5, 0) == true` ama `CastTypeMoralSupported(5, 6) == false`; Resurrection (`5, 0`, `moral 25`): `CastTypeMoralSupported(5, 25) == false` ve `CastMoralSupported(25) == false`.
   - Paket alanları (mevcut yardımcılar; `Moral` 2 tek hedefli): `SendsAimPoint(2) == false`; `CastTargetIdField(SendsAimPoint(2), 2990) == 2990`; `CastCoordField(SendsAimPoint(2), true, 80.9f) == 0` (kendine cure: koordinat 0); `CastCoordField(SendsAimPoint(2), false, 80.9f) == 80` (dosta cure).
   - Tip kapısı Type5'i kapsar: `IsGatedType(5) == true`; `BotCore::TypeStamp st[2]` ile `st[0] = { 5, true, 600 }`, `st[1] = { 0, false, 0 }` için `MinGatedSince(st, 2, since) == true` ve `since == 600`.
   - Başlangıç guard'ı (`CastStartCheck c = {}`; alanlar `Combat_PartyCast_Guard`'daki gibi, `c.skillRange = 56`, `c.msp = 60`, `c.reCastMs = BotCore::CastRecastMs(15)`, `c.typeGated = true`, `c.mana = 60`, `c.distanceM = 0.0f` (`self`), `c.standing = true`, `c.needsStanding = false`, `c.actionsInWindow = 0`): `CastRecastMs(15) == 1500`; `CheckCastStart(c) == CAST_OK`; `c.mana = 59` ⇒ `CAST_REJECT_NO_MANA`; `c.mana = 60; c.distanceM = 55.9f` ⇒ `CAST_OK`; `c.distanceM = 56.0f` ⇒ `CAST_REJECT_OUT_OF_RANGE`; `c.distanceM = 0.0f; c.hasSkillLast = true; c.sinceSkillLastMs = 1499` ⇒ `CAST_REJECT_RECAST`; `c.sinceSkillLastMs = 1500` ⇒ `CAST_OK`.
   - Test adı ve enum sabit adları `BotCombat.h`'dekiyle birebir kullanılır (`CAST_OK`, `CAST_REJECT_NO_MANA`, `CAST_REJECT_OUT_OF_RANGE`, `CAST_REJECT_RECAST`); farklıysa dosyadaki adı kullan. `TypeStamp` alan sırası `{ type, has, sinceMs }` (`BotCombat.h:404-409`).
   - Toplam test sayısı **109**.

### 5.3 `ActionExecutor.cpp` ve `ActionExecutor.h`

**a) `BeginCast` (`ActionExecutor.cpp:733-738`).** Destek koşuluna Type5 moral kapısını ekle (yalnızca bu satır):

```cpp
	bool flyingCast = BotCore::IsFlyingCast(m->bType[0], m->bFlyingEffect);
	if (!BotCore::CastTypesSupported(m->bType[0], m->bType[1])
		|| !BotCore::CastTypeMoralSupported(m->bType[0], m->bMoral)
		|| (m->bFlyingEffect != 0 && !flyingCast)
		|| m->iUseItem != 0
		|| !BotCore::CastMoralSupported(m->bMoral)
		|| !BotCore::CastHpCostSupported(m->sHP))
```

`bad_target` bloğu (`:755-763`), `TickCast`, `SubmitCast`, `CancelCast`, `RejectCast` ve diğer her şey **değişmez**: Type5 `Moral` 2 tek hedefli yolda `area` `false`, hedef kimliği `CastTargetIdField(false, id)`, koordinat `CastCoordField(false, isSelf, ...)`, `typeGated`/`typeStamps` Type5'i zaten kapsar.

**b) `ActionExecutor.h` yorumu.** `BeginCast` yorumundaki destek listesine ekle: "Type5 cure (Moral 2 only: Cure curse, Cure disease; the party-all cure and resurrections stay unsupported; ADR-0017 Ek F4-32); a cure always reports 'effected' when the server broadcasts it, even if nothing was removed (docs/03 MEC-MAG-19)". Yalnızca yorum; başlıkta kod değişikliği yok.

### 5.4 Sonuç sözleşmesi (`docs/03` MEC-MAG-19 ile aynı)

| Skill / durum | Sunucu | Bot sonucu |
|---|---|---|
| **Cure curse `112525`** (`{5, 0}`, `Moral` 2) dosta ya da kendine, hedefte debuff var | CASTING: `IsAvailable()` `MORAL_FRIEND_WITHME` (dost ya da kendisi); EFFECTING: MP `Msp` bir kez (`:1028-1029`), `ExecuteType5` `REMOVE_TYPE4`: hedefin tüm Type4 **debuff**'ları kalkar (buff'lar kalır), çağırana hedef kimlikli EFFECTING yayını | `effected`, `code 0`, `victims` alanı yok (alan olmayan yol); hedefin debuff'ları `snap`'te gider |
| **Cure disease `112535`** (`Moral` 2), hedefte DoT var | `REMOVE_TYPE3`: DoT'lar (`m_sHPAmount < 0`) `Reset()`, HoT'lar korunur, hedefe `MAGIC_DURATION_EXPIRED` 200; aynı yayın | `effected`, `code 0`; HP düşüşü durur (`list`) |
| Cure, hedefte **kaldırılacak bir şey yok** | `ExecuteType5` yine `true`, yayın gider (`:2054`), MP düşer | `effected` (cure "boşa" atılmış olur; karar katmanı önceden bilir, F7) |
| Cure, **düşman** millet bota | `MORAL_FRIEND_WITHME` ⇒ `MAGIC_FAIL` | CASTING'te `srv_fail`, MP düşmez |
| Cure, **ölü** dosta | `ExecuteType5` `:1943-1947` `false`, yayın yok | `no_result` (MP düşmüş olabilir: `:1028-1029` `ExecuteType5`'ten önce) `[D]` |
| Cure, hedef `>= sRange` (56) | bot guard CASTING öncesi reddeder | `REFUSED`/`out_of_range`, paket gitmez |
| Cure, ağaç/seviye yetersiz | `IsAvailable()` ağaç denetimi ⇒ CASTING'te `MAGIC_FAIL` | `srv_fail` (KI-016; referans priest ağaçlarında 1125 = 60, ikisi de yeterli) |
| Bless of God `112671` (`Moral` 6), Resurrection `1127xx` (`Moral` 25, `UseItem`) | — | `BeginCast` `unsupported_skill`, paket gitmez |
| CASTING'te `cast <bot> off` | iptal paketi hedef kimliği (`m_castTargetId`) | `cancelled` (`op:4`, `code:-100`), MP düşmez |

`reason` metinleri, `CastOutcome::Kind` değerleri ve `TickCast` akışı bu tablo için **değişmez**; yeni çıktı yoktur.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (değişen dört dosya için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h`'yi içeren `.cpp`'ler de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastTypes_Supported` ve `Combat_CureCast_Guard` adlarını `[ OK ]` ile içerir ve toplam test sayısı **109**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-32 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş.
- [ ] K5: `grep -n "CastTypeMoralSupported" GameServer/Bot/ActionExecutor.cpp` tam bir eşleşme verir ve `!BotCore::CastTypeMoralSupported(m->bType[0], m->bMoral)` biçimindedir; `CastTypesSupported(m->bType[0], m->bType[1])`, `CastMoralSupported(m->bMoral)`, `CastHpCostSupported(m->sHP)` ve `(m->bFlyingEffect != 0 && !flyingCast)` koşulları yerinde (her biri tek eşleşme).
- [ ] K6: gerilemesiz: `git diff gece/2026-10-02...bot/F4-32 -- GameServer/Bot/ActionExecutor.cpp` yalnızca `BeginCast` destek koşulu hunk'ını içerir (tek hunk, tek eklenen satır); `TickCast`/`SubmitCast`/`CancelCast`/`RejectCast` gövdesinde hunk yok; `git diff ... --stat` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*` değişmemiş.
- [ ] K7: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı türü ve alanı yok (`git diff ... | grep '^+' | grep 'Emit('` boş).
- [ ] K8: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); dört dosya CRLF kalır (ASCII); `git diff --check` boş.
- [ ] K9: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 108 testin tamamı (güncellenen biri dahil) hâlâ geçiyor.
- [ ] K10: `python3 tools/check-perception-contract.py` `RESULT: PASS` (`R1`..`R5` ihlal sayıları 0; denetlenen dosya sayısı değişmez).
- [ ] K11 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S6 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-32
git diff gece/2026-10-02...bot/F4-32 -- BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff gece/2026-10-02...bot/F4-32 -- GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotManager.cpp GameServer/Bot/Telemetry.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -rn "CastTypesSupported\|CastTypeMoralSupported\|CastMoralSupported" BotCore GameServer Tests
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff --check gece/2026-10-02...bot/F4-32
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; komut başına ≥ 1 sn). Botlar (hepsi zone 71, aynı noktada doğar): Karus priest **`BotPHB_K`** (cure atan), Karus **`BotPHD_K`**, **`BotWP_K`** (dost hedef), El Morad **`BotMF_E`** (DoT: Blaze `210509`, Hell fire `210539`) ve **`BotMI_E`** (Chill `210609` yavaşlatma). Önce `list` ile konum ve HP/ölü durumunu denetle (botlar güvenli bölgede başlıyorsa her cast `srv_fail` verir; ölü bot `regene` ile doğuş noktasına taşınır: F4-29/F4-30 bulgusu). MP/HP `list`'ten, buff/debuff `snap <bot>`'tan (debuff `buff` satırında `isBuff` bayrağı ile ayrılır), olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'tan. MP kesin değeri sunucu yenilemesiyle (~5-6 MP/sn) karışır: MP'yi cast'ten hemen önce ve sonra `list` ile al, yenileme payını raporla. Her senaryo arasında botlar `despawn`/`spawn` ile yenilenir (eski debuff kalmasın).

1. **S1 Cure curse (`112525`, `Moral` 2, Type4 debuff kaldırma):** `spawn BotPHB_K,BotMI_E,BotWP_K`; `cast BotMI_E 210609 BotPHB_K 1` (Chill, `Msp 30`; isabet olasılığı < 1: gerekirse tekrarla) ile priest'e debuff koy; `snap BotPHB_K`: en az bir `isBuff = false` kaydı (hız düşürücü `210609` `BuffType` ...). Sonra `cast BotPHB_K 112525 self 1`: `CastStart` `ACTION_SUBMIT` **`"target":<priest kimliği>`** (`-1` değil) → `casting`, `op 1`; ≥ 1,5 sn sonra `CastEffect` → `effected`, `op 3`, `code 0`, **`victims` alanı yok**; log `cast finished (effected) after 1 cycle(s), 1 ok, 2 packet(s) sent`; **MP −60** (bir kez, yenileme payı ≤ ~10); `snap BotPHB_K`: debuff kaydı **gitmiş**, varsa buff'lar yerinde. Aynısını dosta: `cast BotMI_E 210609 BotWP_K 1` sonra `cast BotPHB_K 112525 BotWP_K 1` ⇒ `CastEffect` `"target":<BotWP_K kimliği>`, `effected`, `snap BotWP_K` debuff'sız.
2. **S2 Cure disease (`112535`, DoT kaldırma):** taze oturum; `cast BotMF_E 210539 BotWP_K 1` (Hell fire DoT, `Msp 150`, `TimeDamage -1120` ⇒ canı saniyelik düşürür): `list`'te `BotWP_K` HP'sinin birkaç saniye üst üste düştüğünü kaydet; `cast BotPHB_K 112535 BotWP_K 1` ⇒ `effected`, `code 0`, **MP −120**; sonraki 3 `list` örneğinde (1 sn arayla) `BotWP_K` HP'si düşmez (DoT kalktı; yalnızca doğal yenilenme). Karşılaştırma: aynı DoT'ta cure atılmayan taze botta HP düşmeye devam eder (süre 20 sn).
3. **S3 Kaldırılacak bir şey yok:** hiç debuff'ı olmayan `BotWP_K`'ya `cast BotPHB_K 112525 BotWP_K 1` ve `cast BotPHB_K 112535 BotWP_K 1`: ikisi de `effected`, `code 0`; **MP yine düşer** (−60, −120; MEC-MAG-19 "boşa cure"). İkinci cure'ü ilkinden ≥ 0,7 sn + guard aralığı sonra ver (tip kapısı Type5'i kapsar: ilk cure'den hemen sonraki ikincisi **bot guard'ında** `type_gate` ya da `gap` reddi alabilir; görüleni raporla).
4. **S4 Düşman ve menzil:** `cast BotPHB_K 112525 BotMF_E 1` (düşman millet) ⇒ CASTING'te `srv_fail` (`op:4`, `code -100`), MP değişmez. Hedefi ≥ 60 m uzağa taşıyabiliyorsan `cast BotPHB_K 112525 <uzak dost> 1` ⇒ **paket gitmeden** `FAIRNESS_REJECT` `out_of_range`, `limit 56.00`; taşınamıyorsa birim testle kapsanır ve raporda yazılır.
5. **S5 İptal ve ölü hedef:** `cast BotPHB_K 112525 BotWP_K 1` ardından CASTING aşamasında (~500 ms) `cast BotPHB_K off`: `cancelled` (`op:4`, `code:-100`), iptal paketi hedef kimliği `BotWP_K`, MP değişmez. Ölü dosta cure (`BotWP_K`'yı `BotMF_E` ile öldürmek mümkünse): `no_result` ve MP'nin düşüp düşmediği ölçülür; kurulamıyorsa raporda "ölçülmedi, `[D]`" yazılır ve MEC-MAG-19 buna göre `[D]` kalır.
6. **S6 Kapalı kalanlar, gerilemesiz ve kapsam:** `cast BotPHB_K 112671 self 1` (Bless of God, `Moral` 6) ⇒ `refused (unsupported_skill)`, paket gitmez; `cast BotPHB_K 112733 BotWP_K 1` (Resurrection, `Moral` 25 + `UseItem`) ⇒ `unsupported_skill` (ya da `quest_locked`/`bad_skill` olabilir: görüleni raporla, paket gitmemeli); `112805` Absoluteness ⇒ `unsupported_skill`. Gerilemesiz: F4-28 `cast BotPHB_K 112603 self 1` (Moral 2 Type4) `effected` `code 600`; F4-31 grup heal `cast BotPHB_K 112557 self 1` `target -1`, `effected`, `victims` ≥ 1; F4-31 buff `cast BotPHB_K 112642 BotWP_K 1` `effected`; F4-29 Inferno `cast BotMF_K 110545 BotWP_E 1` `victims` ≥ 1; Moral 7 `cast BotMF_K 110518 BotWP_E 1` `target` kurban kimliği. `TELEMETRY=summary`: cure çalışır, JSONL'de `ACTION_*` yok; `ENABLED=0` ⇒ komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde; sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Temizlik: ini yedekten geri, geçici betikler silindi, botlar despawn, sunucular `stop`.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `BotCore/` ve `Bot/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **`Type1 = 5` + `Moral` 2 + `UseItem 0`** skill'i serisinde davranış değiştirir (önceden `unsupported_skill` idi); diğer tüm tipler/moraller için davranış (paket biçimi ve telemetri satırı dahil) değişmez: `CastTypeMoralSupported` Type5 dışında her zaman `true` döner.
- **Thread kuralı (ADR-0005):** `TickCast` yalnızca IOCP thread'inde; bu planda yeni durum/kilit yok.
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `effected` yalnızca `MAGIC_EFFECTING`'ten, `srv_fail` yalnızca `MAGIC_FAIL`'den. Hedefte debuff var mı, cure'ün bir şey kaldırıp kaldırmadığı, hedefin düşman mı dost mu olduğu **bota önkontrol olarak eklenmez**: bunlar `SelfState.buffs`, gözlenen durum (F4-53) ve karar katmanının (F7) işidir; MP önkontrolü bota ait `user->GetMana()` değeridir (mevcut `CheckCastStart`).
- **Bilinen sınırlar `[A]`/`[D]`:** (a) **"Boşa cure":** `effected` cure'ün etkili olduğunu **göstermez** (sunucu kaldırılacak şey olmasa da yayınlar); etki `snap`/`list` ile ölçülür, karar katmanı etkiyi `SelfState`/`TeamView`'dan çıkarır. (b) **Type5 MP kuralı** Type4 tek hedefli kuralından farklıdır (başarıya bağlı değil, EFFECTING'te bir kez; `:1028-1029`): guard `mana >= Msp`'yi ister. (c) **Ölü hedef** `no_result` olur, MP'nin düşmesi `[D]` (sunucu kodu okuması; ölçülemezse raporda belirtilir). (d) **KI-016:** ağaç yetersizliği bota önkontrol edilmez; referans priest ağaçları cure için yeterlidir. (e) Bless of God (`Moral` 6) ve diriltmeler bilinçli olarak kapalıdır: Bless of God'un kurban seçimi `sRange` yarıçaplı farklı bir yol kullanır ve botlarda erişilemez (doğrulanamaz), diriltme taş tüketimi ister (dilim 6b/envanter).
- Telemetri hacmi değişmez; `tools/bot-telemetry-report.py` değişmez (`no_result` zaten ayrı sayılır, MET-ACT-02).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-32` (taban: `gece/2026-10-02`) — `1ca8af6 [F4-32] Type5 cure dilimi: {5,0} tip destegi ve Moral 2 kapisi` (kod/plan Durum)
- Değişen dosyalar ve neden:
  - `BotCore/BotCombat.h`: `CastTypesSupported(5, 0)` açıldı; yeni `CastTypeMoralSupported(type0, moral)` (Type5 yalnızca `Moral` 2, diğer tipler pass-through); yorum güncellendi.
  - `GameServer/Bot/ActionExecutor.cpp`: `BeginCast` destek koşuluna tek satır `!BotCore::CastTypeMoralSupported(m->bType[0], m->bMoral)`.
  - `GameServer/Bot/ActionExecutor.h`: `BeginCast` yorumuna Type5 cure açıklaması eklendi.
  - `Tests/BotCoreTests/CombatTests.cpp`: `Combat_CastTypes_Supported` güncellendi (`(5,0)` `true`; yeni `false` satırları); yeni `Combat_CureCast_Guard` eklendi; `Combat_PartyCast_Guard` içindeki mevcut `CastTypesSupported(5, 0) == false` satırı zorunlu olarak `true` yapıldı (açıklama: aşağıda, plandan sapma).
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
    CombatTests.cpp
    BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
    proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  Release rc=0, Debug rc=0; değişen dört dosyada uyarı gözlenmedi.
- Kabul kriterleri öz-değerlendirme: K1 ✔ (Release rc=0), K2 ✔ (Debug rc=0), K3 ✔ (`109 tests, 0 failed`, iki yapılandırmada; `Combat_CastTypes_Supported`, `Combat_CureCast_Guard`, `Combat_PartyCast_Guard` `[ OK ]`), K4 ✔ (include denetimi boş, `std::min/max` yok), K5 ✔ (tek eşleşme, biçim doğru), K6 ✔ (tek hunk, tek eklenen satır; `--stat` yalnız §4'teki 4 dosya; yasak dosyalarda fark boş; commit sonrası ölçüldü), K7 ✔ (`Emit(` boş; yeni ini/komut/thread yok), K8 ✔ (dört dosya CRLF+ASCII, `git diff --check` boş), K9 ✔ (guard grep sayıları; önceki testler geçiyor), K10 ✔ (`RESULT: PASS`, `files scanned: 19`, R1..R5 ihlal 0/0). K11 Claude'un çalışma zamanı turunda.
- Plandan sapmalar ve gerekçeleri:
  1. **`Combat_PartyCast_Guard` içindeki `CASTTypesSupported(5, 0) == false` satırı `true` yapıldı.** Plan §5.2 bunu açıkça anmıyor, ancak `(5,0)` artık desteklendiği için mevcut assert zorunlu olarak geçersizdi (ilk test koşusunda `Combat_PartyCast_Guard` `[FAIL]`). Aynı dosya ve aynı amaç (tip kapısı) kapsamındadır; K3'ün "önceki 108 test hâlâ geçiyor" şartı bunu gerektirir. Davranış/mekanik değişikliği değildir.
- Açık sorular: Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-32` @ `<sha>`
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ / ✘ | dosya:satır / komut çıktısı |

- Bulgular (önem sırasıyla):
  1. …
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
…
```
