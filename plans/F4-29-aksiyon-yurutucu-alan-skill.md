# F4-29: `ActionExecutor` alan skill dilimi — düşman alanı (`Moral` 10: Inferno, Supernova, Blizzard, Frost nova, Torment...) ve hedef noktası (CLI-07)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02, merge `78ae6b7`) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-29` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-03 (cast dilimi: `BeginCast`/`TickCast`/`SubmitCast`, tip kapısı, `m_castEcho`) — `KAPANDI`; F4-24 (cast iptali, `m_castTargetId`) — `KAPANDI`; F4-26 (`{3, 4}` çifti, iki tipli kapı) — `KAPANDI`; F4-28 (`{4, 0}` tek tipli Type4) — `KAPANDI` (merge `dc61d7d`) |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-07 (bu planla uygulanır), CLI-11, MEC-AOE-01, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-16 (bu planla eklendi, `[D]`), SK-06, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 dilim 5 |
| Tahmini büyüklük | M (6 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.cpp`, `ActionExecutor.h`, `BotSession.h`, `BotSession.cpp`; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

`BeginCast` yalnızca `Moral` 1, 2, 7 ve 8 olan skill'leri kabul ediyor (`ActionExecutor.cpp:733-735`); **her alan skill'i** (`Moral` 10) `unsupported_skill` ile reddediliyor. Mage'in asıl hasar büyüleri (Inferno `110545`, Supernova `110560`, Blizzard `110645`, Frost nova `110660`, Thundercloud, Static nova), priest'in alan debuff'ı Torment `112757` ve warrior'ın Quake `106760`'ı bu yüzden botla atılamıyor; F6 (mage küme hasarı, `docs/08` "hedef noktası kümenin ağırlık merkezi") ve F7 davranışlarının ön koşulu.

Bu plan **yalnızca `Moral` 10 (AREA_ENEMY), uçmayan (`FlyingEffect == 0`), `UseItem == 0` alan skill'lerini** açar: `bType = {3, 0}`, `{3, 4}` ve `{4, 0}` (F4-25/26/28'in desteklediği tip çiftleri). Üç şey değişir:

1. **Paket biçimi (CLI-07):** alan skill'inde sunucu hedef kimliğini `-1` ister (kimlik verilirse `UserCanCast` reddeder, `MagicInstance.cpp:170-180`) ve hedef noktasını `sData[0]`/`sData[2]`'den okur (`MagicProcess.cpp:200-201` `final_test`). Bot `/bot cast <bot> <skill> <hedef bot|self>` komutundaki hedefi **hedef noktası** olarak kullanır (o botun/kendinin konumu); paketin hedef kimliği `-1`, `sData[0..2]` o konum olur. Hedef noktası çağırana `sRange` içinde olmalıdır (CLI-07): mevcut `CastInRange` guard'ı bunu zaten `meters < sRange` ile uygular (`meters` = çağırandan hedef noktasına mesafe), yeni guard kuralı gerekmez.
2. **Destek kuralı:** `Moral` 10 kabul edilir (`BotCore::CastMoralSupported`), uçan alan skill'leri (Fire burst/Ice burst/Thunder burst, `FlyingEffect != 0`) **kapsam dışı kalır** (§3).
3. **Sonuç gözlemi:** alan cast'in yankısı çoğu zaman "isabet var/yok"u ayırt etmez (boş alan da `effected` döner, MP ve bekleme yine harcanır; MEC-MAG-16). Bot, sunucunun çağırana yayınladığı **kurban başına** `MAGIC_EFFECTING` paketlerini sayar ve `ACTION_RESULT`'a `victims` alanı olarak yazar (yalnızca alan skill'inde, yalnızca yayınlanan paketten, AC-LRN-03).

Kapsam dışı kalan skill'ler §3'tedir. F4'ün yirmi dokuzuncu planıdır (ADR-0018 sırası: cast iptali ✔ → uçan ✔ → çift tipli ✔ → Type4 tek tipli ✔ → **alan (bu plan)** → ...).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-29)" (bu planla birlikte yazıldı), "Ek (F4-28)", "Ek (F4-26)", "Ek (F4-25)" ve "Ek (F4-03)". `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (+ Ek 5).
- `docs/03` §4.2 **MEC-MAG-03/-08/-11/-12/-13/-15/-16**, §5 **MEC-AOE-01**, §13.2 **CLI-03, CLI-04, CLI-07, CLI-11**. MEC-MAG-16 bu planla eklendi (`[D]`).
- `docs/05` SK-06 (alan skill'lerinde hedef noktası çağıranın menzili içinde seçilir), `docs/08` §6.3 "Alan hasarı kuralları" (hedef noktası kümenin ağırlık merkezi: karar katmanı işi, bu plan değil).
- `plans/F4-28-aksiyon-yurutucu-type4-tek-tipli-skill.md` (aynı dosyaların aynı bölgeleri; **yazılı planı değil, kodu esas al**).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `dc61d7d` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.cpp:170-180` — `UserCanCast()`: `bMoral` 10..13 (`MORAL_AREA_ENEMY`..`MORAL_SELF_AREA`, `MagicInstance.h:33-36`) ve `sTargetID != -1` ise `SkillUseFail` (item proc hariç). **Bu yüzden CASTING, FLYING-olmayan EFFECTING ve iptal paketinin hedef kimliği `-1` olmalıdır.**
  - `GameServer/MagicInstance.cpp:355` — güvenli bölgede (`isInSafetyArea()`) **her** skill (`nSkillID < 400000`) reddedilir; `:460` ek olarak Type3 alan için aynı denetim (`CheckType3Prerequisites`, `sTargetID == -1`). Bot güvenli bölgede alan skill'i atarsa CASTING'te `srv_fail` (`−100`) alır (bu planın kusuru değil; test botlarını güvenli bölgenin dışına taşı).
  - `GameServer/MagicInstance.cpp:857-866` (`MORAL_ENEMY`) ve `:809-823` — `IsAvailable()` moral `switch`'inde `MORAL_AREA_ENEMY` yoktur: moral denetimi alan için uygulanmaz; sonra `:915-917` döngüsü `CheckType3Prerequisites()`/`CheckType4Prerequisites()` çağırır. `:549-554` `CheckType4Prerequisites()` `sTargetID == -1` iken (AOE) `true` döner (aynı `BuffType` denetimi alanda yok).
  - `GameServer/MagicInstance.cpp:1028-1029` — MP: `bType[0] != 4 || sTargetID == -1` iken EFFECTING'de **`ExecuteSkill`'den önce** düşer; yani alan skill'inde (Type3 de Type4 de) MP, hedef bulunsun bulunmasın bir kez düşer. `ExecuteType4`'ün ek MP düşümü (`:1800-1802`) yalnızca `sTargetID != -1` içindir.
  - `GameServer/MagicInstance.cpp:1276-1307` — `ExecuteType3()` alan dalı (`sTargetID == -1`): çağıranın bölgelerindeki birimler taranır (`GetUnitListFromSurroundingRegions`); GM'ler, çağıranın kendisi, ölü/blink/saldırılamaz olanlar elenir; `CMagicProcess::UserRegionCheck(..., pType->bRadius, sData[0], sData[2])` (`:1294`) hedef noktasına `Radius` içinde ve düşman olanı seçer. **Kurban yoksa** `:1299-1302` `SendSkill(); return true` (boş alan başarılı sayılır: MP düşer, bekleme/tip damgası kaydedilir, yankı gelir). `:1337-1339` her kurban için ayrıca **çağırandan** kurbana mesafe `>= sRange` ise kurban atlanır (hedef noktası menzil içinde olsa bile kümenin uzak kenarı kaçabilir).
  - `GameServer/MagicInstance.cpp:1599` — her kurban için `BuildAndSendSkillPacket(pSkillCaster, true, sCasterID, pTarget->GetID(), bOpcode, ...)`: **bölgeye (çağıran dahil) kurban kimlikli `MAGIC_EFFECTING`**; yalnızca `bType[1] == 0 || 3` iken. `:1611-1613` sonunda `sTargetID == -1 && bType[0] == 3` iken **bir kez daha `SendSkill()`** (hedef `−1`'li alan yayını). Yani Type3 alanın **son** paketi hedef `−1`'li EFFECTING'tir ve `sData[3]` çağıranın gönderdiği değerdir (bot 0 gönderir ⇒ `code` 0, `missed` olamaz: `SKILLMAGIC_FAIL_ATTACKZERO` yalnızca Type1/2'de yazılır, `:1109`, `:1254`).
  - `GameServer/MagicInstance.cpp:1635-1681` — `ExecuteType4()` alan dalı: aynı tarama + `UserRegionCheck(..., pType->bRadius, ...)`; hiç kurban yoksa `:1664-1677` `SendSkill(); return false` (oyuncu çağıranda): yankı gelir ama `bInitialResult == false` ⇒ `Run()` (`:109-131`) **bekleme/tip damgası kaydetmez ve `bType[1]`'i çalıştırmaz** (bot muhafazakâr olarak yine damgalar). `:1695-1696` kurban başına `sRange` denetimi; `:1861-1874` kurban başına `{sData[0], bResult, sData[2], süre, ...}` yayını (çağıran dahil), `:1891-1893` `bResult == 0` ise ek `MAGIC_FAIL` (`−103`).
  - `GameServer/MagicProcess.cpp:113-208` — `UserRegionCheck()`: `MORAL_AREA_ENEMY` (`:147-150`) `isHostileTo` ise `final_test` (`:200-201`): `radius == 0 || kurban.isInRangeSlow(mousex, mousez, radius)` (2D, kare mesafe `<=` yarıçap², `Unit.cpp:133-136`). `radius` = `MAGIC_TYPE3.Radius`/`MAGIC_TYPE4.Radius`.
  - `GameServer/Unit.cpp:805-808` — `SendToRegion` çağırana da gider (`Send_Region(..., nullptr, ...)`): bot kurban paketlerini kendi oturumunda görür.
  - `GameServer/Bot/ActionExecutor.cpp` (satırlar `dc61d7d`): `SubmitCast` `:586-686` (paket `:612-613`: `int16(target.id)`, `sData[0..2]`; `m_castEcho = 0` `:614`; sonuç eşlemesi `:624-653`); `BeginCast` destek kuralı `:729-737` (**moral koşulu `:733-734`**), `bad_target` `:751-759` (alan için değişmez: `MORAL_SELF`/`MORAL_ENEMY` dışında ad ve `self` ikisi de kabul); `TickCast` hedef görünümü ve mesafe `:792-796`, `sData` `:890-893`, CASTING `:909-916` (`m_castTargetId = target.id`, `:915`), FLYING `:951-957`, EFFECTING `:992-994`; iptal paketi `m_castTargetId`'yi hedef alanına yazar (`:1103`, `:1112`); `BotManager.cpp:3187-3215` hedef görünümü (self: kendi konumu, adlı hedef: o botun oturumundan) **değişmez**.
  - `GameServer/Bot/BotSession.cpp:72-83` — yankı yalnızca `caster == m_castSelfId` ve opcode 1..4'te yazılır (`op` ofset 0, `skill` 1, `caster` 5, `target` 7, `sData[0]` 9, `sData[1]` 11, `sData[3]` 15); `BotSession.h:165` `m_castEcho`; ctor `BotSession.cpp:25`.
  - `BotCore/BotCombat.h:143-150` `CastInRange` (`skillRange > 0` ⇒ `distanceM < skillRange`), `:266-270` `IsFlyingCast`, `:317-327` `CastTypesSupported`; `Tests/BotCoreTests/CombatTests.cpp:1019` `Combat_CastTypes_Supported`, `:1130` `Combat_TypeGate_Type4Single` (dosyanın son testi). Toplam birim test **104** (F4-28 sonrası).
- **Veri notu (Claude yerel `MAGIC`/`MAGIC_TYPE3`/`MAGIC_TYPE4` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** `Moral = 10`, `Etc = 0`, `UseItem = 0`, `FlyingEffect = 0`, bot sınıflarının (Karus `106`/`110`/`112`, El Morad `206`/`210`/`212`) skill'leri; El Morad karşılıkları `2xxxxx`. `Radius` 15 m (Quake 10 m):
  - **`{3, 0}`:** warrior `106760` Quake (`Msp 160`, `CastTime 0`, `ReCast 51`, `Range 45`, seviye 60), mage `110545` **Inferno** (`Msp 200`, `CastTime 15`, `ReCast 153`, `Range 56`, seviye 45), `110560` **Supernova** (`Msp 400`, `CastTime 15`, `ReCast 153`, seviye 60), `110571` meteor Fall (`Msp 600`, `CastTime 13`, `ReCast 183`, `Range 45`, **`UseStanding 53`**, seviye 70), `110745` Thundercloud, `110760` Static nova, `110771` Chain lightning (`UseStanding 53`).
  - **`{3, 4}`:** `110645` **Blizzard** (`Msp 200`, seviye 45), `110660` **Frost nova** (`Msp 400`, seviye 60), `110671` ice storm (`Msp 600`, `UseStanding 53`, seviye 70).
  - **`{4, 0}`:** priest `112757` **Torment** (`Msp 150`, `CastTime 15`, `ReCast 94`, `Range 56`, seviye 57), `112770` Subside (`UseStanding 54`, seviye 70), mage `110762` Light Shock (`Msp 400`, `ReCast 1`, seviye 62).
  - Bu planda `unsupported_skill` kalanlar: `FlyingEffect != 0` alan skill'leri (Fire burst `110533` `FlyingEffect 191`, Ice burst `110633` 291, Thunder burst `110733` 391), `Moral` 10 + `UseItem != 0` (Discountis `112772`, Minor Resist `110825`), `Moral` 11 (Elysian Web `112825`, `UseItem`), 12, 13, `{7, 0}` alan skill'leri (provoke `106645`, Sleep Carpet `112751`; Type7 ayrı karar planı, ADR-0018 Ek 1 madde 3) ve party hedefli `Moral` 4/6. **Bot sınıflarında `Moral` 11/12/13 `UseItem`'siz skill yoktur** (sorgu: `Moral BETWEEN 10 AND 13`, sınıf `106/110/112`).
- **Bot karakter notu (`db/002_bot_characters.sql:124-136`).** Skill ağacı baytları (`strSkill` indeksi = `MAGIC.Skill % 10`; sunucu `m_bstrSkill[indeks] >= SkillLevel` ister, `MagicInstance.cpp:956-963`; KI-016: bot tarafında denetim yok, yetersiz ağaçta `srv_fail`): `BotMF` ateş (`1105`, indeks 5) **70**, buz (`1106`, indeks 6) **52**, yıldırım (indeks 7) 0; `BotMI` ateş 52, buz **70**, yıldırım 0; `BotPHD` `1127` (indeks 7) **62**, `BotPHB` 0; `BotWP` `1067` (indeks 7) 52, `BotWG` 0 ⇒ Quake (60) iki warrior profiliyle de **atılamaz** (`srv_fail`), test dışıdır. Sınama için: Inferno/Supernova **BotMF** (ya da BotMI, 52 ≥ 45 ✔, Supernova 60 ✘); Blizzard **BotMF** (52 ≥ 45); Frost nova **BotMI** (70 ≥ 60); Torment **BotPHD** (62 ≥ 57).
- **`UseStanding` notu (KI-017).** `needsStanding = (UseStanding == 1)` kuralı değişmez; `UseStanding 53/54` master skill'leri (meteor Fall, ice storm, Chain lightning, Subside) bu planın sınama kapsamı dışıdır.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `kMoralAreaEnemy = 10`, `IsAreaMoral(moral)`, `CastMoralSupported(moral, flyingEffect)`, `CastTargetIdField(area, id)`, `CastCoordField(area, isSelf, metres)` (§5.1). İki yeni birim test (104 → 106).
2. **`BeginCast` (`ActionExecutor.cpp`):** moral koşulu `CastMoralSupported`'a taşınır (alan kabul, uçan alan ret); `bad_target` kuralı **değişmez**.
3. **`TickCast` + `SubmitCast` (`ActionExecutor.cpp`):** alan skill'inde paketin hedef kimliği `-1`, `sData[0..2]` hedef noktası (adlı hedefin ya da çağıranın konumu; `self` dahil), `m_castTargetId = -1` (iptal paketi de `-1` taşır); `SubmitCast` her paketten önce kurban sayacını sıfırlar ve alan EFFECTING'in `ACTION_RESULT`'ına `victims` yazar.
4. **Kurban sayacı (`BotSession.h/.cpp`):** `std::atomic<uint32> m_castEchoVictims`; `OnPacket()` çağıranın kendi `MAGIC_EFFECTING` paketlerinden hedef kimliği `-1` olmayanları sayar (§5.3).
5. **Sonuç sözleşmesi (§5.4):** boş alan, kurbanlı alan, `{3, 4}` ve `{4, 0}` alan, menzil dışı hedef noktası, güvenli bölge, iptal; belgelenir (`docs/03` MEC-MAG-16) ve çalışma zamanında sınanır.

**Kapsam dışı (yapılmayacak)**

- **Uçan alan skill'leri** (`FlyingEffect != 0`: Fire burst, Ice burst, Thunder burst; ADR-0018 Ek 1 madde 4): CASTING → FLYING → EFFECTING akışında alan paketinin sunucu davranışı (FLYING'de `sTargetID == -1`, MP'nin iki kez düşmesi, kurban seçimi anı) çalışma zamanında ölçülmeden açılmaz; ayrı küçük dilim (F4-30 adayı). `CastMoralSupported` bunları `false` döndürür.
- `Moral` 11, 12, 13 (dost/hepsi/kendi merkezli alan), party hedefli (4, 6), NPC (5), klan (14, 15) ve `UseItem` gerektiren alan skill'leri (Discountis, Minor Resist, Elysian Web: dilim 6), `{7, 0}` alan skill'leri (Type7: ayrı karar planı), `{1, 4}`/`{2, 4}` karışık çiftler, okçu `Type2` alan skill'leri (dilim 2b).
- **Hedef noktası seçimi** (kümenin ağırlık merkezi, en çok düşmanı kapsayan nokta, menzil kenarı payı): karar katmanı işidir (F6, `docs/08`). Bu planda hedef noktası `/bot cast` komutunun verdiği botun/kendinin konumudur; `BotManager.cpp` ve komut sözdizimi **değişmez**. Düz koordinat girişi (`cast <bot> <skill> at <x> <z>`) bu planda yok; ihtiyaç doğarsa ayrı küçük plan.
- **Menzil kenarı payı:** sunucu kurban başına `sRange`'i **çağırandan** denetler (`MagicInstance.cpp:1337-1339`); bot yalnızca hedef noktasını `< sRange` tutar (CLI-07). Kümenin uzak kenarının kaçması bilinen sınırdır (§8-b).
- Skill ağacı puanı denetimi (KI-016) ve `UseStanding` 51..54 anlamı (KI-017), güvenli bölge denetimi (bot yalnızca sunucu reddini görür), kurbanın kim olduğunun/dost-düşman ayrımının bot tarafında yorumlanması, kurban sayısının `list`/`snap` çıktısına eklenmesi, yeni komut/ini anahtarı/telemetri olayı türü, `BotManager.cpp` değişikliği, `tools/` betik değişikliği.
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | yeni bölüm `// --- area cast ...` (`CastTypesSupported` bölümünden sonra, `IsGatedType`'den önce) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | `Combat_CastMoral_Supported`, `Combat_AreaCast_Fields` eklenir (104 → 106) |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | `BeginCast` moral koşulu; `TickCast` alan alanları; `SubmitCast` sayaç sıfırlama + `victims` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `BeginCast`/`TickCast`/`CastTarget` yorumları; `SubmitCast` `static` (yalnızca `.cpp`'de) olduğundan başlıkta imza değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | `m_castEchoVictims` bildirimi |
| `GameServer/Bot/BotSession.cpp` | değiştir | ctor başlatma + `OnPacket()` sayacı |

`BotManager.cpp`, `Telemetry.*`, `ScenarioRunner.*`, `ScriptRunner.*`, `tools/` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`CastTypesSupported` bölümünden hemen sonra şu bölümü ekle (ASCII, CRLF, tab, `namespace BotCore` içinde; yeni `#include` yok; **`std::min`/`std::max` kullanma**):

```cpp
	// --- area cast (ADR-0017 Ek F4-29, docs/03 MEC-MAG-16, CLI-07) ---

	// MAGIC.Moral 10 = AREA_ENEMY (MagicInstance.h). The server wants target id -1 for every area moral (10..13) and reads
	// the aim point from sData[0] (x) and sData[2] (z); the bot only opens Moral 10 so far.
	constexpr uint8_t kMoralAreaEnemy = 10;

	inline bool IsAreaMoral(uint8_t moral)
	{
		return moral == kMoralAreaEnemy;
	}

	// Morals BeginCast accepts: 1 self, 2 friend-with-me, 7 enemy, 8 all (F4-03) and 10 area-enemy (F4-29). A flying area
	// skill (FlyingEffect != 0: Fire burst, Ice burst, Thunder burst) stays unsupported until its own slice.
	inline bool CastMoralSupported(uint8_t moral, uint16_t flyingEffect)
	{
		if (moral == 1 || moral == 2 || moral == 7 || moral == 8)
			return true;

		return IsAreaMoral(moral) && flyingEffect == 0;
	}

	// WIZ_MAGIC_PROCESS 'target' field: an area cast always carries -1, every other cast the target's id.
	inline int16_t CastTargetIdField(bool area, int16_t targetId)
	{
		return area ? int16_t(-1) : targetId;
	}

	// One of sData[0..2]: metres truncated. A single-target self cast sends 0 (F4-03); an area cast always sends the aim
	// point, also for "self" (the caster's own position is the aim point then).
	inline int16_t CastCoordField(bool area, bool isSelf, float metres)
	{
		return (isSelf && !area) ? int16_t(0) : int16_t(metres);
	}
```

Dosyanın kalanı (`IsGatedType`, `CastQuestAllowed`, `TypeStamp`, `MinGatedSince`, `CastTypesSupported`, `CastInRange`) **değişmez**. `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

Dosyanın sonuna iki test ekle (mevcut `TEST_CASE`/`CHECK_EQ` üslubu; yeni testler `Combat_TypeGate_Type4Single`'dan sonra):

a) **`Combat_CastMoral_Supported`:** `CastMoralSupported(moral, flyingEffect)`:
   - `true`: `(1, 0)`, `(2, 0)`, `(7, 0)`, `(8, 0)`, `(10, 0)`; uçan olmayanlar için `flyingEffect` ne olursa olsun `(1, 0)` gibi tek hedefli moral'ler `true` kalır: `(7, 191)` da `true` (uçan tek hedefli F4-25 yolu bu fonksiyona bağlı değildir, `BeginCast`'teki ayrı `bFlyingEffect` koşulu geçerli).
   - `false`: `(10, 191)` (uçan alan), `(0, 0)`, `(3, 0)`, `(4, 0)`, `(5, 0)`, `(6, 0)`, `(9, 0)`, `(11, 0)`, `(12, 0)`, `(13, 0)`, `(14, 0)`, `(15, 0)`, `(25, 0)`.
   - `IsAreaMoral`: `(10)` `true`; `(1)`, `(7)`, `(11)`, `(13)` `false`.

b) **`Combat_AreaCast_Fields`:**
   - `CastTargetIdField(true, 2990) == -1`, `CastTargetIdField(true, -1) == -1`, `CastTargetIdField(false, 2990) == 2990`, `CastTargetIdField(false, 0) == 0`.
   - `CastCoordField(true, true, 123.7f) == 123` (alan + self: hedef noktası gönderilir), `CastCoordField(true, false, 123.7f) == 123`, `CastCoordField(false, true, 123.7f) == 0`, `CastCoordField(false, false, 123.7f) == 123`, `CastCoordField(true, true, 0.0f) == 0`.
   - **CLI-07 hedef noktası menzili (mevcut `CastInRange`/`CheckCastStart` ile):** `CastStartCheck c = {}` (alanlar `Combat_TypeGate_Type4Single`'daki gibi; `skillRange = 56`, `msp = 200`, `reCastMs = 15300`, `typeGated = true`, `hasTypeLast = false`, `hasSkillLast = false`, `mana = 1000`), `c.distanceM = 55.9f` ⇒ `CheckCastStart(c) == CAST_OK`; `c.distanceM = 56.0f` ⇒ `CAST_REJECT_OUT_OF_RANGE`; `c.distanceM = 0.0f` (alan, `self`) ⇒ `CAST_OK`; `c.mana = 199` (Inferno `Msp 200`) ⇒ `CAST_REJECT_NO_MANA` (guard sabit adını `BotCombat.h`'de `enum CastVerdict`'ten doğrula; farklıysa o adı kullan).
   - Toplam test sayısı **106**.

### 5.3 `ActionExecutor.cpp`, `ActionExecutor.h`, `BotSession.*`

**a) `BeginCast` (`ActionExecutor.cpp:729-737`).** Moral koşulunu değiştir; kalan koşullar aynen kalır:

```cpp
	bool flyingCast = BotCore::IsFlyingCast(m->bType[0], m->bFlyingEffect);
	if (!BotCore::CastTypesSupported(m->bType[0], m->bType[1])
		|| (m->bFlyingEffect != 0 && !flyingCast)
		|| m->iUseItem != 0
		|| !BotCore::CastMoralSupported(m->bMoral, m->bFlyingEffect))
```

`bad_target` (`:751-759`) **değişmez**: `Moral` 10 ne `MORAL_SELF` ne `MORAL_ENEMY` olduğundan ad da `self` de kabul edilir (hedef noktası = o bot ya da çağıran).

**b) `TickCast` (`:792-1010`).** Hedef görünümü ve mesafe hesabı (`:792-796`) aynen kalır (`meters` = çağırandan hedef noktasına). `m` çözüldükten sonra (`flying` bildiriminin yanına):

```cpp
	bool area = BotCore::IsAreaMoral(m->bMoral);
	CastTarget sent = target;
	sent.id = BotCore::CastTargetIdField(area, target.id);
```

- `sData[0..2]` (`:890-893`): `BotCore::CastCoordField(area, target.isSelf, target.x)` / `target.y` / `target.z`.
- `SubmitCast` çağrıları (CASTING `:909`, FLYING `:951`, EFFECTING `:992`) `target` yerine **`sent`** ve ek `bool area` argümanı alır; `s->m_castTargetId = sent.id` (`:915`, `:957`). Böylece iptal paketi (`:1103`, `:1112`) alan skill'inde `-1` taşır (hedef kimlikli iptal paketi `UserCanCast`'te `:170-180` yüzünden `−103` ile reddedilirdi).
- `RejectCast`, tip kapısı, damgalar, `CheckCastStart/Effect/Land` çağrıları **değişmez** (mesafe zaten hedef noktasınadır).

**c) `SubmitCast` (`:586-686`).** İmza: `static CastOutcome SubmitCast(BotSession * s, CUser * user, uint8 opcode, uint32 skillId, const CastTarget & target, bool area, const int16 sData[3], ...)` (diğer parametreler aynı sırada). `s->m_castEcho = 0;` satırının yanına `s->m_castEchoVictims = 0;`. `HandlePacket` döndükten sonra `uint32 victims = s->m_castEchoVictims.load();`. `ACTION_RESULT` alanlarına, **yalnızca `area && opcode == MAGIC_EFFECTING`** iken `+ ",\"victims\":" + std::to_string(victims)` eklenir (alan sırası: `...,"code":..,"victims":N,"latency_us":..`; alan dışı skill'lerde satır **bayt bayt aynı** kalır). `reason` eşlemesi **değişmez**.

**d) `BotSession.h` / `BotSession.cpp`.** `BotSession.h:165` yanına `std::atomic<uint32> m_castEchoVictims;   // written by OnPacket(): own EFFECTING packets with a real target id since the last reset by SubmitCast()`. Ctor (`BotSession.cpp:25`) `m_castEcho(0)` yanına `m_castEchoVictims(0)`. `OnPacket()` `:72-83` bloğunda, `op`/`caster` zaten okunmuşken: `int16 victimId = pkt.read<int16>(7);` ve `op == MAGIC_EFFECTING && caster == m_castSelfId.load() && victimId != -1` iken `m_castEchoVictims++` (yankı yazan `if`'in içinde; `MAGIC_EFFECTING` (`MagicInstance.h`) `stdafx.h` üzerinden görünür değilse `BotCore::kMagicEffecting` (`BotCore/Perception.h:1795`, değeri 3) kullan). Başka satır değişmez.

**e) `ActionExecutor.h` yorumları.** `CastTarget` açıklamasına: "for an area skill (MAGIC.Moral 10) 'x/y/z' is the aim point (the target bot's position or the caster's own for 'self') and the packet's target id is -1 (ADR-0017 Ek F4-29)". `BeginCast` yorumundaki destek cümlesine: "area Moral 10 (non-flying; aim point = the target's position, within MAGIC.Range of the caster, CLI-07) is supported, flying area skills are not". `TickCast` yorumuna: "area: the EFFECTING echo is the last packet the server sent (Type3: target -1 broadcast, code 0; {3, 4}/{4, 0}: last victim's Type4 packet, code = duration); an empty area still gives 'effected' and costs MP; 'victims' in ACTION_RESULT counts the per-victim EFFECTING packets (docs/03 MEC-MAG-16)".

### 5.4 Sonuç sözleşmesi (`docs/03` MEC-MAG-16 ile aynı)

Alan skill'i (`Moral` 10, hedef kimliği `-1`, hedef noktası `sData[0]`/`sData[2]`):

| Durum | Sunucu | Bot sonucu |
|---|---|---|
| CASTING (`CastTime > 0`) | `SendSkill(true)`: CASTING yankısı bölgeye | `casting` (`CastTime 0`: CASTING gitmez, tek EFFECTING) |
| EFFECTING, kurban(lar) var, Type3 `{3, 0}` | MP **bir kez** (`IsAvailable`, `:1028-1029`); her kurban için hedef kimlikli EFFECTING, sonra hedef `−1`'li alan EFFECTING | `effected`, `code` = 0 (son paket), `victims` = kurban sayısı (NPC dahil, çağıranın bölgesindeki, hedef noktası `Radius` içinde, çağırana `< sRange`, düşman, canlı) |
| EFFECTING, kurban yok (Type3) | MP düşer, `SendSkill()` tek EFFECTING, bekleme/tip damgası yazılır | `effected`, `code` 0, **`victims` 0** |
| EFFECTING, `{3, 4}` | MP bir kez; `SendSkill()` + her kurban için Type4 paketi (`code` = süre) | `effected`, `code` = son kurbanın süresi (kurban yoksa 0), `victims` |
| EFFECTING, `{4, 0}` (Torment) | MP bir kez (`:1028`, `sTargetID == -1`); her kurban için Type4 paketi; kurban yoksa `SendSkill(); return false` (sunucu **bekleme damgası yazmaz**) | `effected`, `code` = süre (yoksa 0), `victims` |
| Type4 kurbanda direnç/engel (`bResult = 0`) | kurban paketinden sonra ek `MAGIC_FAIL` (`−103`) (`:1891-1893`) | son paket `MAGIC_FAIL` ise `srv_fail`; `victims` yine kurban paketlerini sayar (bilinen sınır, §8-c) |
| Hedef noktası `>= sRange` | bot guard `CastInRange` CASTING/EFFECTING'ten önce reddeder (`FAIRNESS_REJECT` `out_of_range`, paket gitmez) | `REFUSED` `out_of_range` (CLI-07) |
| Güvenli bölge (`:355`, `:460`) | `UserCanCast` `false` ⇒ `MAGIC_FAIL` (CASTING'te `−100`) | `srv_fail` (CASTING'te seri `FAILED`) |
| Beceri ağacı yetersiz (KI-016) | `IsAvailable` `:956-963` `false` | `srv_fail` |
| Cast sırasında `cast <bot> off` | iptal paketi hedef `−1`, `sData[3] = −100` | `cancelled` (`op:4`, `code:-100`), MP düşmez (F4-24) |

`reason` metinleri, `CastOutcome::Kind` değerleri ve `TickCast` akışı bu tablo için **değişmez**; tek yeni çıktı `ACTION_RESULT.victims`'tir.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (değişen altı dosya için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h`/`BotSession.h` içeren `.cpp`'ler de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastMoral_Supported` ve `Combat_AreaCast_Fields` adlarını `[ OK ]` ile içerir ve toplam test sayısı **106**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-29 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş.
- [ ] K5: `BeginCast` destek kuralı: `grep -n "CastMoralSupported" GameServer/Bot/ActionExecutor.cpp` tam bir eşleşme (`BeginCast`) verir; eski `m->bMoral != MORAL_SELF && ...` dört koşullu ifade kalmamıştır (`grep -n "m->bMoral != MORAL_SELF" GameServer/Bot/ActionExecutor.cpp` boş); `bad_target` bloğu (`wantedSelf`/`wantedTarget`) değişmemiştir.
- [ ] K6: alan paketi: `TickCast`'te `SubmitCast`'in üç çağrısı da `sent` ve `area` ile yapılır (`grep -n "SubmitCast(s, user" GameServer/Bot/ActionExecutor.cpp` üç satır, hepsinde `sent`); `m_castTargetId = sent.id` iki yerde; `sData` üç satırı `CastCoordField` kullanır; alan dışı skill'lerde paketin hedef kimliği ve `sData` değeri değişmez (birim test `CastTargetIdField(false, id) == id`, `CastCoordField(false, ...)`).
- [ ] K7: sayaç: `grep -n "m_castEchoVictims" GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp GameServer/Bot/ActionExecutor.cpp` başlıkta bir, `BotSession.cpp`'de iki (ctor, `OnPacket`), `ActionExecutor.cpp`'de iki (sıfırlama, okuma) satır verir; `victims` alanı yalnızca `area && opcode == MAGIC_EFFECTING` koşulunda yazılır (`grep -n '"victims"' GameServer/Bot/ActionExecutor.cpp` tek eşleşme ve koşul görünür).
- [ ] K8: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread yok; yeni telemetri olayı türü yok (`git diff ... | grep '^+' | grep 'Emit('` boş; `victims` mevcut `ACTION_RESULT` satırına koşullu alandır); `BotManager.cpp`, `Telemetry.*`, `tools/` değişmemiştir.
- [ ] K9: `git diff --stat gece/2026-10-02...bot/F4-29` yalnızca §4'teki 6 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş.
- [ ] K10: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); değişen altı dosya CRLF kalır (`BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.h`, `BotSession.h` ASCII); `git diff --check` boş.
- [ ] K11: gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 104 testin tamamı hâlâ geçiyor.
- [ ] K12: `python3 tools/check-perception-contract.py` `RESULT: PASS` (`R1`..`R5` ihlal sayıları 0; denetlenen dosya sayısı değişmez); yeni kod sunucu nesnesi okumaz (yalnızca kendi alınan paketinden sayar).
- [ ] K13 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S6 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-29
git diff gece/2026-10-02...bot/F4-29 -- BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp
git diff gece/2026-10-02...bot/F4-29 -- GameServer/Bot/BotManager.cpp GameServer/Bot/Telemetry.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "CastMoralSupported\|SubmitCast(s, user\|m_castTargetId = \|m_castEchoVictims\|\"victims\"" GameServer/Bot/ActionExecutor.cpp GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotSession.cpp
git diff --check gece/2026-10-02...bot/F4-29
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`); zamanlama için geçici betik `./Scripts/f429_*.txt` (silinir, commit edilmez). Botlar (hepsi zone 71, aynı başlangıç noktasında): Karus mage **`BotMF_K`** (Inferno, Supernova, Blizzard), Karus mage **`BotMI_K`** (Frost nova), Karus priest **`BotPHD_K`** (Torment), El Morad kurbanlar **`BotWP_E`**, **`BotWG_E`**, **`BotPHD_E`** (HP 32000; zone 71'de NPC'ler bot kurbanları öldürebilir, gerekirse yeniden doğur, F4-26 notu). Güvenli bölge: botlar güvenli bölgede başlıyorsa her cast `srv_fail` verir (önceki planlarda botla cast edilebildiği için başlangıç noktası dışarıdadır; ilk S1 denemesi `srv_fail −100` verirse güvenli bölgeyi kontrol et ve `move` ile taşı). MP/HP `list` çıktısından, buff/debuff `snap <bot>`'tan, olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'tan. Skill bekleme süreleri uzun (Inferno 15,3 sn): her senaryo arasında botlar `despawn`/`spawn` ile yenilenir (taze oturumda bekleme/buff yoktur).

1. **S1 Kurbanlı alan, `{3, 0}` (Inferno, Supernova):** `spawn BotMF_K,BotWP_E,BotWG_E,BotPHD_E`; MP ve düşman HP `list` ile not; `cast BotMF_K 110545 BotWP_E 1` (Inferno: `Msp 200`, `CastTime 15`, `Range 56`, `Radius 15`; hedef noktası = `BotWP_E`'nin konumu). Beklenen JSONL: `CastStart` `ACTION_SUBMIT` **`"target":-1`** → `ACTION_RESULT` `ok:true`, `reason:"casting"`, `op:1`; ≥ 1580 ms sonra `CastEffect` `ACTION_SUBMIT` `"target":-1`, `since_casting_ms` ≥ 1580 → `ACTION_RESULT` `ok:true`, **`reason:"effected"`, `op:3`, `code:0`, `victims` ≥ 3** (üç El Morad bot + varsa yakındaki NPC'ler; `victims` ≥ bot sayısı); log `cast finished (effected) after 1 cycle(s), 1 ok, 2 packet(s) sent`; **MP düşüşü = 200 (bir kez)**; üç El Morad botun HP'si düşmüş (hedef botlar `list`'te); `snap BotMF_K events` kurban kimlikli `op=3 skill=110545 caster=<mage> target=<kurban>` olaylarını ve hedef `−1`'li son olayı gösterir (F4-52 halkası; kurban olayı sayısı `victims` ile uyumlu, halka 10 olay tutar). Aynı sırayı `cast BotMF_K 110560 BotWP_E 1` (Supernova: `Msp 400`) verir (MP −400; botlar yenilenmiş taze oturumda).
2. **S2 Hedef noktası = çağıran (`self`) ve boş alan:** `spawn BotMF_K` (düşman yok, **El Morad bot yok**): `cast BotMF_K 110545 self 1`: `CastStart` ve `CastEffect`'te `"target":-1`; `CastEffect` `ok:true`, `reason:"effected"`, `code:0`, **`victims:0`** (boş alan başarılı sayılır, MEC-MAG-16); **MP yine −200**. Aynı oturumda ikinci `cast BotMF_K 110545 self 1`: guard `recast` (`FAIRNESS_REJECT`/bekleme) ile ≥ 15,3 sn sonra gider (boş alan sunucuda da bekleme damgası yazar: `ExecuteType3` boş alanda `true` döner). `spawn BotWP_E,BotWG_E` sonra (düşman çağıranla aynı noktada): `cast BotMF_K 110545 self 1` (taze oturum) `victims` ≥ 2 (hedef noktası çağıranın kendi konumu; sData self için de gönderilir).
3. **S3 Çift tipli alan `{3, 4}` ve Type4 alan `{4, 0}`:** (a) `spawn BotMF_K,BotWP_E,BotWG_E`; `cast BotMF_K 110645 BotWP_E 1` (Blizzard: `Msp 200`, `{3, 4}`, buz ağacı 52 ≥ 45): `CastEffect` `effected`, **`code` > 0 (Type4 süresi)**, `victims` ≥ 2, MP −200; `snap BotWP_E` ve `snap BotWG_E`'de yavaşlatma debuff'ı (`debuff`, `type=` yavaşlatma `BuffType`). `BotMI_K` ile `cast BotMI_K 110660 BotWP_E 1` (Frost nova: `Msp 400`) aynı biçimde. (b) `spawn BotPHD_K,BotWP_E,BotWG_E`; `cast BotPHD_K 112757 BotWP_E 1` (Torment: `{4, 0}`, `Msp 150`, `CastTime 15`): `effected`, `code` = Torment süresi (MAGIC_TYPE4 `Duration`), `victims` ≥ 2, MP −150, kurbanlarda `debuff` (`snap`). (c) Torment **boş alanda** (`spawn BotPHD_K` tek başına, `cast BotPHD_K 112757 self 1`): `effected`, `code:0`, `victims:0`, MP −150 (sunucu bekleme damgası yazmaz; bot muhafazakâr, bu fark raporda yazılır).
4. **S4 CLI-07 hedef noktası menzili:** `spawn BotMF_K,BotWP_E`; `BotWP_E`'yi `move` ile çağırandan ≥ 60 m uzağa taşı (`move BotWP_E ...`, yürüme bitince): `cast BotMF_K 110545 BotWP_E 1` ⇒ paket gitmeden `FAIRNESS_REJECT` (`out_of_range`; `ACTION_SUBMIT` yok), log `cast stopped (out_of_range)`; MP değişmez. `BotWP_E`'yi ≈ 50 m'ye yaklaştırıp tekrar: `casting` → `effected` (guard geçer; `victims` 0 ya da 1, sunucu kurban başına çağırandan `< 56` ister, kurban 50 m'de ⇒ hedef alınır). Sınırda (≈ 55,5 m) ve kurbanın çağırana mesafesi ≥ 56 durumları ölçülmez (kenar payı karar katmanı işi, §8-b).
5. **S5 İptal ve reddedilen/desteklenmeyen:** (a) `cast BotMF_K 110545 BotWP_E 1` verip CASTING aşamasında (~500 ms) `cast BotMF_K off`: `ACTION_RESULT` `cancelled` (`op:4`, `code:-100`), iptal paketi **hedef `−1`** taşır (`Logs`/JSONL ve `PacketTrace`; hedef kimlikli iptalin `−103` vereceği engellenmiştir), MP değişmez, hedefte hasar yok. (b) `cast BotMF_K 110533 BotWP_E 1` (Fire burst, uçan alan) ⇒ `unsupported_skill`; `cast BotPHD_K 112751 BotWP_E 1` (Sleep Carpet `{7, 0}`) ⇒ `unsupported_skill`; `cast BotPHD_K 112772 BotWP_E 1` (Discountis, `UseItem`) ⇒ `unsupported_skill`; `cast BotMF_K 110825 self` (Minor Resist, `UseItem`) ⇒ `unsupported_skill`; `cast BotWP_K 106760 BotWP_E 1` (Quake, ağaç 52 < 60) ⇒ kabul edilir (bot tarafında ağaç denetimi yok) ve CASTING'te/EFFECTING'te `srv_fail` (KI-016; `CastTime 0`, tek `CastEffect` `srv_fail`, `op:4`, `code:-103`), MP düşmez. (c) `cast BotMF_K 110545 BotWP_E 3` serisi: her döngü guard `recast` ile ≥ 15,3 sn arayla gider, her döngüde `victims` ≥ 1, MP üç kez −200.
6. **S6 Gerilemesiz:** Tek hedefli alan dışı skill'ler önceki planlardaki gibi çalışır: `cast BotMF_K 110518 BotWP_E 3` (Ignition) `effected` ×3, `target` alanı **kurban kimliği** (−1 değil) ve ACTION_RESULT'ta **`victims` alanı yok** (alan değil); `cast BotMF_K 110615 BotWP_E 1` (Ice arrow, uçan çift tipli) F4-26 gibi 3 paket, MP ≈ 100; `cast BotMF_K 110515 BotWP_E 1` (Fire ball) F4-25 gibi 3 paket; `cast BotPHD_K 112703 BotWP_E 1` (Malice, tek hedefli Type4) F4-28 gibi `effected`, `code` 150, `victims` yok; `cast BotWP_K 106007 self 1` (Defense) `effected`; `attack`/`move` (F4-01/F4-02); `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama alan cast çalışır (log satırları); `ENABLED=0` ⇒ komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde (≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Temizlik: ini yedekten geri, geçici betikler silindi, botlar despawn.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `BotCore/` ve `Bot/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **`Moral` 10 alan skill'i** serisinde davranış değiştirir; alan dışı skill'ler (paket biçimi ve telemetri satırı dahil) ve cast etmeyen botlar için davranış değişmez.
- **Thread kuralı (ADR-0005):** `TickCast` yalnızca IOCP thread'inde. `OnPacket()` yalnızca bir `std::atomic` sayacı artırır (kilit yok); sayaç `SubmitCast` içinde, `HandlePacket()` ile aynı thread'de sıfırlanır/okunur (kendi `m_castEcho`'sunun düzeni). `ActionExecutor` log yazmaz.
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `effected` yalnızca sunucunun bölgeye yayınladığı, çağırana da gelen `MAGIC_EFFECTING` paketinden, `srv_fail` yalnızca çağırana yollanan `MAGIC_FAIL`'den; `victims` yalnızca çağıranın kendi oturumuna gelen kurban kimlikli `MAGIC_EFFECTING` paketlerinden çıkarılır. Sunucu nesnelerinden (kurban listesi, HP) hiçbir şey okunmaz.
- **Bilinen sınırlar `[A]`/`[D]`:** (a) **CLI-07 `[Ö]`:** gerçek istemcinin CASTING paketinde de hedef noktası gönderip göndermediği ve `sData[1]` için ne yazdığı ölçülmedi; bot CASTING ve EFFECTING'e aynı hedef noktasını yazar (sunucu yalnızca EFFECTING'te okur) ve hedef kimliği `-1`'dir (sunucu zorunlu kılar). Gerçek istemci biçimi insan ölçümüdür (T-MECH-AOE adayı, `docs/15`). (b) **Menzil kenarı:** bot hedef noktasını çağırana `< sRange` tutar; sunucu kurban başına çağırandan `< sRange` ister (`MagicInstance.cpp:1337-1339`), bu yüzden hedef noktası menzilin kenarındaysa kümenin uzak yarısı kaçar. Payı karar katmanı seçer (F6). (c) **Yankının son paketi:** `{4, 0}`/`{3, 4}` alanda kurbanlardan birinde direnç/engel (`bResult = 0`) olursa son paket `MAGIC_FAIL` olabilir ⇒ `srv_fail` görünür, oysa diğer kurbanlar etkilenmiştir; gerçek durum `victims` ve `snap`'tedir. (d) **`victims` ≠ isabet:** sayaç sunucunun kurban kimlikli yayınlarını sayar (NPC dahil; Type4'te direnç/engelli kurban da sayılır, hasar sıfırsa da); kimin etkilendiği karar katmanının algısından (F4-52 halkası, F4-53) okunur. (e) **Güvenli bölge/ağaç puanı** sunucu reddine bırakılır (`srv_fail`). (f) **Bekleme damgası:** Type4 alan skill'i hedefsiz kalırsa sunucu bekleme/tip damgasını yazmaz, bot muhafazakâr olarak yazar; fark yalnızca botun bir sonraki cast'ini geciktirir. (g) **Aynı anda çok sayıda paket:** bir alan cast'i kurban başına paket üretir; `m_skillEvents` halkası (F4-52, 10 olay) taşabilir, bu planın konusu değildir. (h) **`UseStanding` 53/54 master skill'leri** (meteor Fall, ice storm, Chain lightning, Subside) KI-017 gereği test dışıdır; `BeginCast` bunları reddetmez, sunucunun cevabı geçerlidir.
- Telemetri hacmi: alan cast başına en fazla bir ek tamsayı alan; `tools/bot-telemetry-report.py` değişmez (bilinmeyen alanı yok sayar, `srv_fail` zaten MET-ACT-02'ye sayılır). `list`/`snap` ve `Telemetry.*` değişmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI (Release/Debug derleme rc=0, iki birim testi `[ OK ]`, toplam 106 test, 0 failed).
- Branch / commit'ler: `bot/F4-29` (taban `gece/2026-10-02`). Kod + `Durum` commit'i: `c6d686b` "`[F4-29]` Alan skill dilimi: Moral 10, hedef noktasi -1, victims sayaci" (6 kod dosyası + plan `Durum` satırı). Bu rapor ayrı commit'lenir.
- Değişen dosyalar ve nedenleri (yalnızca §4'teki 6 dosya):
  - `BotCore/BotCombat.h`: `CastTypesSupported` bölümünden sonra `--- area cast ---` bölümü: `kMoralAreaEnemy = 10`, `IsAreaMoral`, `CastMoralSupported` (1/2/7/8 + uçmayan 10), `CastTargetIdField`, `CastCoordField`. Yeni include yok (`<algorithm>`, `<cstdint>` mevcut).
  - `Tests/BotCoreTests/CombatTests.cpp`: dosya sonuna `Combat_CastMoral_Supported` ve `Combat_AreaCast_Fields` (104 → 106). İkincisi CLI-07 menzil (`55.9` OK / `56.0` OUT_OF_RANGE), alan-self (`0.0` OK) ve `mana` 199 → `NO_MANA` (`Msp` 200) sınırlarını da kapsar.
  - `GameServer/Bot/ActionExecutor.cpp`: `BeginCast` moral koşulu `CastMoralSupported`'a taşındı; `TickCast`'te `area`/`sent` (hedef kimliği alanda `-1`), `sData[0..2]` üç satırı `CastCoordField`, üç `SubmitCast` çağrısı `sent, area`, `m_castTargetId = sent.id`; `SubmitCast` imzasına `bool area`, sayaç sıfırlama/okuma ve `ACTION_RESULT`'a koşullu `victims` (yalnızca `area && opcode == MAGIC_EFFECTING`, `code` ile `latency_us` arasında).
  - `GameServer/Bot/ActionExecutor.h`: `CastTarget`, `BeginCast`, `TickCast` yorumları (alan davranışı, CLI-07, MEC-MAG-16, `victims`).
  - `GameServer/Bot/BotSession.h`: `std::atomic<uint32> m_castEchoVictims` bildirimi (`m_castEcho` yanına).
  - `GameServer/Bot/BotSession.cpp`: ctor `m_castEchoVictims(0)`; `OnPacket()` `WIZ_MAGIC_PROCESS` bloğunda `victimId = read<int16>(7)` ve `op == MAGIC_EFFECTING && victimId != -1` iken `m_castEchoVictims++` (yankı `if`'inin içinde, `caster == m_castSelfId` zaten koşulda).
- Derleme sonucu:
  - `./tools/build.sh Release`: rc=0; son satırlar `All 14050 functions were compiled...`, `Kodun üretilmesi tamamlandı`, `proj-GameServer.vcxproj -> ...\x86-Release\Server\GameServer.exe`. Değişen altı dosyada uyarı yok; yalnızca önceden var olan iki `UpgradeHandler.cpp` C4789 uyarısı.
  - `./tools/build.sh Debug`: rc=0; `proj-GameServer.vcxproj -> ...\x86-Debug\Server\GameServer.exe`, `BotCoreTests.vcxproj -> ...\x86-Debug\Tests\BotCoreTests.exe`.
  - `./tools/run-tests.sh Release` ve `Debug`: `106 tests, 0 failed`; `Combat_CastMoral_Supported` ve `Combat_AreaCast_Fields` `[ OK ]`.
- §5.3 kod okuma doğrulamaları (satırlarla, commit sonrası):
  - `BeginCast`: `ActionExecutor.cpp:730-737`; eski `m->bMoral != MORAL_SELF ...` ifadesi kalmadı (`grep` boş), `bad_target` bloğu (`wantedSelf`/`wantedTarget`, `:751-759`) değişmedi.
  - `TickCast`: `:811-817` `area`/`sent`; `:896-899` `sData[0..2]` `CastCoordField`; `:917` CASTING, `:959` FLYING, `:1000` EFFECTING `SubmitCast(s, user, ..., sent, area, sData, ...)`; `m_castTargetId = sent.id` `:923` ve `:965`.
  - `SubmitCast`: `:585-587` imza `..., const CastTarget & target, bool area, const int16 sData[3], ...`; `:615` `m_castEchoVictims = 0`; `:626` `victims` okuma; `:666` koşullu `victims` alanı.
  - `BotSession.cpp`: `:25` ctor; `:76` `victimId`; `:84` artırma. `BotSession.h:166` bildirim.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (Release rc=0, değişen dosyalarda uyarı yok), K2 ✔ (Debug rc=0).
  - K3 ✔ (iki yapılandırmada da 106 test, 0 failed; iki yeni test `[ OK ]`).
  - K4 ✔ (`grep` boş; include yalnızca `<algorithm>`/`<cstdint>`; `std::min/max` eklenmedi).
  - K5 ✔ (`CastMoralSupported` `BeginCast`'te; eski ifade yok; `bad_target` değişmedi).
  - K6 ✔ (üç `SubmitCast` çağrısı `sent`+`area`; `m_castTargetId = sent.id` iki yerde; `sData` üç satırı `CastCoordField`; alan dışı birim testleri geçiyor).
  - K7 ✔ (`m_castEchoVictims` başlıkta 1, `BotSession.cpp` 2, `ActionExecutor.cpp` 2; `victims` yalnızca `area && opcode == MAGIC_EFFECTING` koşulunda).
  - K8 ✔ (yeni ini/komut/thread yok; `Emit(` eklenmedi; `BotManager.cpp`/`Telemetry.*`/`tools/` değişmedi; `victims` mevcut `ACTION_RESULT` satırına koşullu alan).
  - K9 ✔ (`git diff --stat gece/2026-10-02...bot/F4-29` yalnızca 6 dosya + plan; proje dosyaları değişmedi).
  - K10 ✔ (altı dosya ASCII + CRLF; `git diff --check` boş).
  - K11 ✔ (`CheckMoveStep` 2, `CheckAttack`/`CheckCastStart`/`CheckCastEffect`/`CheckCastFly`/`CheckCastLand`/`CheckCastCancel`/`CheckPotion` ≥ 1; önceki 104 test geçiyor).
  - K12 ✔ (`tools/check-perception-contract.py` `RESULT: PASS`, ihlal 0; yeni kod yalnızca kendi alınan paketinden sayıyor).
  - K13: Claude'un çalışma zamanı doğrulaması (S1–S6).
- Plandan sapmalar: yok. (Küçük not: `ActionExecutor.h` `CastTarget` yorumuna alan cümlesi eklendi; `int16 id` alan yorumu plan gereği değiştirilmedi. `RESPAWN` davranışı için sayaç sıfırlama eklenmedi; plan gereği `SubmitCast` her paketten önce sıfırlar.)
- Açık sorular: yok; plan ile kod çelişmedi. Doğrulamada dikkat: alan yayını `m_skillEvents` halkasında (F4-52) hızlı dolar ve `victims` direnç/engelli kurbanı da sayar (§8-c/d, bilinen sınır, plan gereği).

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- **Karar: DOĞRULANDI.**
- İncelenen commit: `bot/F4-29` @ `30f1a86` (kod `c6d686b`; taban `gece/2026-10-02`). Çalışma ağacı temizdi; otonom gece döngüsünde (`AUTO_LOOP=1`) birleştirme/push yapılmadı, birleştirmeyi döngü betiği `gece/2026-10-02`'ye yapar.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | altı dosya `touch` + `tools/build.sh Release` rc=0; günlükte `ActionExecutor.cpp`, `CombatTests.cpp`, `BotSession.cpp` yeniden derlendi, uyarı yok |
| K2 | ✔ | `tools/build.sh Debug` rc=0, uyarı yok |
| K3 | ✔ | `run-tests.sh Release` ve `Debug` rc=0, ikisinde `106 tests, 0 failed`; `Combat_CastMoral_Supported` ve `Combat_AreaCast_Fields` `[ OK ]` |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/" BotCombat.h` boş; `#include` yalnızca `<algorithm>`, `<cstdint>`; eklenen satırlarda `std::min/max` yok |
| K5 | ✔ | `CastMoralSupported` `ActionExecutor.cpp:737` tek eşleşme; `m->bMoral != MORAL_SELF` yok; diff'te `wantedSelf`/`wantedTarget` satırı yok (`bad_target` bloğu `:751-759` değişmedi) |
| K6 | ✔ | `SubmitCast(s, user` üç satır (`:917`, `:959`, `:1000`), üçünde `sent, area`; `m_castTargetId = sent.id` `:923`, `:965`; `sData` üç satırı `CastCoordField` (`:896-899`); alan dışı `CastTargetIdField(false, id) == id` ve `CastCoordField(false, ...)` birim testte; çalışma zamanında tek hedefli skill'lerde `target` kurban kimliği (S6) |
| K7 | ✔ | `m_castEchoVictims`: `BotSession.h:166` 1, `BotSession.cpp:25` ctor + `:84` `OnPacket` (2), `ActionExecutor.cpp:615` sıfırlama + `:626` okuma (2); `"victims"` tek eşleşme, `if (area && opcode == MAGIC_EFFECTING)` içinde (`:664-666`) |
| K8 | ✔ | eklenen satırlarda `Emit(` yok (tek eşleşme plan dosyasındaki rapor metni); `BotManager.cpp`, `Telemetry.*`, `tools/`, vcxproj değişmedi; yeni ini/komut/thread yok; `ENABLED=0` çalışma zamanında sınandı (aşağıda) |
| K9 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-29`: yalnızca §4'teki 6 dosya + plan dosyası |
| K10 | ✔ | `file`: altı dosya ASCII + CRLF; `git diff --check` boş (rc=0); altı dosyada LF'li (CR'siz) satır sayısı 0 (`grep -vc $'\r$'`; `core.autocrlf=true` olduğundan `git diff` çıktısı CR göstermez) |
| K11 | ✔ | `CheckMoveStep` 2; `CheckAttack`, `CheckCastStart`, `CheckCastEffect`, `CheckCastFly`, `CheckCastLand`, `CheckCastCancel`, `CheckPotion` 1'er; önceki 104 test geçiyor |
| K12 | ✔ | `check-perception-contract.py` `RESULT: PASS` (19 dosya); yeni kod yalnızca kendi alınan paketinden sayar (`BotSession::OnPacket`) |
| K13 | ✔ | S1–S6 çalışma zamanında geçti (aşağıda) |

**Çalışma zamanı** (Release, `ENABLED=1`, `MAX_BOTS=16`, `TELEMETRY=decisions`, zone 71; `Logs/bots/2026-10-03/live-023933.jsonl`, `Logs/Bot_3_10_2026.log`):

- **S1 ✔** Inferno `110545` BotMF_K → BotWP_E (hedef noktası = kurbanın konumu): `CastStart` `"target":-1`, `casting` `op 1`; 1662 ms sonra `CastEffect` `"target":-1`, `effected`, `op 3`, `code 0`, **`victims 3`** (üç El Morad bot); log `cast finished (effected) after 1 cycle(s), 1 ok, 2 packet(s) sent`; MP 6021 → 5821 (−200, bir kez); üç kurbanın HP'si düştü; `snap BotMF_K events`: kurban kimlikli üç `op=3 ... target=2985/2986/2987` olayı + hedef `−1`'li son olay + `op=1`. Supernova `110560`: `victims 3`, MP −400 (+ yenileme, 5941 → 5581).
- **S2 ✔** Boş alan (`self`, düşman yok): `target -1`, `effected`, `code 0`, **`victims 0`**; iki döngüde ikinci `CastStart` ilk `CastEffect`'ten 15,37 sn sonra (guard `recast`, ≥ 15,3 sn); `self` + iki düşman aynı noktada: `victims 2`.
- **S3 ✔** (a) Blizzard `110645` `{3, 4}`: `effected`, `code 18`, `victims 2`; `snap BotWP_E`/`BotWG_E` `buff skill=110645 type=6 debuff`. Frost nova `110660` (BotMI_K): `code 20`, `victims 2`, MP 6021 → 5661. (b) Torment `112757` (BotPHD_K) `{4, 0}`: `code 150`, `victims 2`, `snap` ikisinde `skill=112757 type=2 debuff remain=146s/145s`. (c) Torment boş alan (`self`): `effected`, `code 0`, `victims 0`, MP 6322 → 6212.
- **S4 ✔** hedef 68 m: `FAIRNESS_REJECT` `MEC-MAG-11` `out_of_range` (`value 68, limit 56`), `ACTION_SUBMIT` yok, log `cast stopped (out_of_range)`; 50 m: `casting` → `effected`, `victims 1`.
- **S5 ✔** (a) CASTING'te (1100 ms) `cast off`: `CastCancel` `"target":-1`, `cancelled`, `op 4`, `code -100`; MP değişmedi, hedef HP'si değişmedi. (b) Fire burst `110533`, Sleep Carpet `112751`, Discountis `112772`, Minor Resist `110825` ⇒ `unsupported_skill`; Quake `106760` (BotWP_K, ağaç 52 < 60) ⇒ tek `CastEffect` `srv_fail`, `op 4`, `code -103`, MP değişmedi (KI-016). (c) Inferno ×3: döngü arası 15,3 sn (`recast`), her döngüde `victims 1` (hedef noktası BotWG_E; WP_E 60 m uzakta, `Radius` dışı), üç döngü `effected`, MP −600 + yenileme (6021 → 5701 / 50 sn).
- **S6 ✔** Ignition ×3 `effected`, `target` kurban kimliği (2985), `victims` alanı **yok**; Ice arrow `110615` CastStart → CastFly → CastEffect (3 paket, `code 12`); Fire ball `110515` 3 paket; Malice `112703` (canlı hedef) `effected`, `code 150`, `victims` yok; Defense `106007` `effected`, `code 15`, `victims` yok; `move` (`ok`) ve `attack` (2 vuruş, `hit`) çalışıyor; `TELEMETRY=summary`: alan cast `cast finished (effected)`, JSONL'de `ACTION_*` yok (yalnızca `PERF_SAMPLE`); `ENABLED=0`: `BotCommands.txt` işlenmedi, `Bot_*.log`'a satır ve yeni JSONL yok; `tick_p50` 1–5 µs, `tick_p95` ortancası 132 µs, 146 pencerenin ikisi > 1 ms (2611 µs, 1226 µs; ikisi de spawn pencereleri: `in_game` 3/6 ve 4/7 oturum, bilinen spawn maliyeti, F4-28 raporu bulgu 1); `Logs/GameServer.log` değişmedi (son yazım 2026-10-02 02:17). Temizlik: botlar despawn, sunucular `stop`, ini yedekten geri yüklendi (`[BOT]` bölümü yok), `BotCommands.txt` silindi, repo temiz.

**Bulgular (engelleyici yok):**

1. *(not)* Quake `srv_fail` yanıtında da `victims 0` yazılır (alan EFFECTING'i; plan §5.3-c gereği). Reddedilmiş alan cast'inde `victims` anlamsızdır; raporlama/analiz araçları `ok:false` satırında `victims`'i yok saymalıdır. Planın bilinen sınırı, kod doğru.
2. *(not)* MP düşüşü sunucu MP yenilemesiyle (≈ 5–6 MP/sn) karışır; kesin −N yalnızca S1'de (Inferno −200, Frost nova −360 ≈ −400 + yenileme) görüldü. Bu, F4-28 raporunun bulgu 2'siyle aynı ölçüm sınırıdır.
3. *(not)* Operasyonel: bir bot despawn'da son konumunu saklar; sonraki spawn'da botlar farklı noktalarda doğabilir (BotWP_E 1324, BotWP_K 960 gibi) ve `cast`/`attack` `out_of_range` verir; çalışma zamanı sınamasından önce `list` ile konumlar kontrol edilmelidir. Ayrıca bir cast'in CASTING aşamasında aynı botun `move` komutu cast'i iptal eder (F4-24 davranışı, `CastCancel` `cause:"move"`); `BotCommands.txt`'e art arda iki komut yazılırsa ilki işlenmeden ezilir (komut başına ≥ 1 sn bekle). Hiçbiri bu planın kusuru değil.
4. *(not)* Sınanmayanlar (plan kapsamı dışı/bilinen sınır): Type4 alanda (Torment) boş alanın sunucu bekleme damgası yazmaması (§8-f, bot muhafazakâr), kurbanın çağırana `>= sRange` kenar kaybı (§8-b), `{4, 0}`/`{3, 4}` alanda kurbanda direnç/engelli son paket `MAGIC_FAIL` (§8-c), güvenli bölge `srv_fail`, gerçek istemcinin alan paketi biçimi (CLI-07 `[Ö]`, §8-a; insan ölçümü T-MECH-AOE adayı).
5. *(not)* Uygulayıcı raporu doğru: commit listesi, dosyalar, derleme (rc=0, değişen dosyalarda uyarı yok) ve test sayıları (106, 0 failed) kendi çalıştırmamla örtüşüyor; sapma/soru yok.
