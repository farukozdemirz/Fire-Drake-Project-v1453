# F4-30: `ActionExecutor` uçan alan skill dilimi — Fire burst, Ice burst, Thunder burst (`Moral` 10 + `FlyingEffect != 0`)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02, merge `c88dd95`) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-30` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-25 (uçan Type3: CASTING → FLYING → EFFECTING, `CastManaNeed`, `CheckCastFly/Land`) — `KAPANDI`; F4-26 (`{3, 4}` çifti) — `KAPANDI`; F4-29 (alan skill: hedef kimliği `-1`, hedef noktası, `victims`) — `KAPANDI` (merge `78ae6b7`) |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-07, CLI-11, MEC-AOE-01, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-12, MEC-MAG-13, MEC-MAG-16, MEC-MAG-17 (bu planla eklendi, `[D]`), SK-06, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 dilim 5 devamı (Ek 1 madde 4) |
| Tahmini büyüklük | S (4 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.cpp`, `ActionExecutor.h`; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

F4-29 `Moral` 10 alan skill'lerini açtı ama **uçan** alan skill'lerini bilerek dışarıda bıraktı: `CastMoralSupported(moral, flyingEffect)` `(10, FlyingEffect != 0)` için `false` döndürüyor (`BotCore/BotCombat.h:342-348`), bu yüzden mage'in üç "burst" büyüsü botla atılamıyor: **Fire burst `110533`** (`{3, 0}`, `FlyingEffect 191`), **Ice burst `110633`** (`{3, 4}`, 291), **Thunder burst `110733`** (`{3, 0}`, 391) ve El Morad karşılıkları (`210533`, `210633`, `210733`). `docs/08` §6.3 bunları yakın küme için (r = 8 içinde ≥ 2 düşman) mage'in ucuz alan seçeneği olarak ister; F6 mage davranışının ön koşuludur.

F4-25 (uçan akış: CASTING → FLYING → EFFECTING, MP'nin iki kez düşmesi) ve F4-29 (alan paketi: hedef kimliği `-1`, hedef noktası `sData[0..2]`, `victims`) ayrı ayrı çalışma zamanında doğrulandı. `TickCast` bu iki özelliği **birbirinden bağımsız** `flying` ve `area` bayraklarıyla işliyor (`ActionExecutor.cpp:812`, `:815`); `SubmitCast` üç opcode'un hepsinde `sent`/`area` ile çalışıyor. Yani eksik olan tek şey, `BeginCast`'in moral denetimindeki `flyingEffect == 0` koşuludur.

Bu plan:

1. `CastMoralSupported`'tan `flyingEffect` koşulunu kaldırır (uçan alan açılır; imza `CastMoralSupported(uint8_t moral)` olur);
2. uçan alan akışının sunucu davranışını kodla ve çalışma zamanında doğrular ve belgeler (`docs/03` MEC-MAG-17);
3. skill'e özgü sayılarla (Fire burst: `Msp 150`, `CastTime 15`, `Range 90`) bir birim test ekler.

Başka mantık değişikliği yoktur. F4'ün otuzuncu planıdır (ADR-0018 sırası: ... alan ✔ → **uçan alan (bu plan)** → 2b okçu → dilim 6 → CLI-12 → envanter doldurma).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-30)" (bu planla birlikte yazıldı), "Ek (F4-29)", "Ek (F4-26)", "Ek (F4-25)". `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (Ek 1 madde 4, Ek 5, Ek 6).
- `docs/03` §4.2 **MEC-MAG-12** (uçan Type3'te MP iki kez), **MEC-MAG-13** (`{3, 4}` yankısı Type4'ten), **MEC-MAG-16** (alan), **MEC-MAG-17** (bu planla eklendi), §13.2 **CLI-03** (satır 425: gerçek istemci Fire burst `110533` ölçümü: CASTING → FLYING +1539 ms → EFFECTING +2576 ms, `target = -1`, `sData[0..2]` hedef noktası, n = 3), **CLI-07**.
- `plans/F4-25-aksiyon-yurutucu-ucan-skill.md` ve `plans/F4-29-aksiyon-yurutucu-alan-skill.md` (aynı fonksiyonlar; **yazılı planı değil, kodu esas al**).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `78ae6b7` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.cpp:45-94` — `MAGIC_FLYING` dalı: `pSkillTarget` kullanılmaz; skill `MAGIC_TYPE2` tablosunda yoksa (Type3 burst'ler yok) `sMsp > GetMana()` ise `SendSkillFailed`, aksi hâlde `MSpChange(-sMsp)` ve `SendSkill(true)` (bölgeye FLYING yayını). **Hedef kimliği `-1` bu dalı engellemez:** `Run()` `:20` `pSkillTarget`'ı yalnızca `sTargetID != -1` iken çözer; `UserCanCast()` `:170-180` alan moral'inde `-1`'i *ister*; `CheckSkillPrerequisites()` `:350` `pSkillTarget != nullptr` bloğu atlanır (menzil denetimi yok, bot guard'ı yapar).
  - `GameServer/MagicInstance.cpp:96-134` — EFFECTING: `ExecuteSkill(bType[0])`; başarılıysa bekleme/tip damgası ve `ExecuteSkill(bType[1])`. `IsAvailable()` içindeki MP düşümü (`:999-1029`) `bType[0] == 2 && FlyingEffect != 0` dışında **her** uçan skill'de çalışır (Type3'te `sTargetID == -1` de dahil: `:1028`): bir burst cast'i **2 × `Msp`** harcar (FLYING'de `:90`, EFFECTING'te `:1029`). `UserCanCast()` `IsAvailable()`'ı **yalnızca CASTING ve EFFECTING** opcode'larında çağırır (`:284-286`); FLYING'de çağırmaz. Skill ağacı/seviye denetimi `IsAvailable()` içindedir (`:956-963`), yani `CastTime > 0` olan burst'lerde ağacı yetersiz bir bot **CASTING'te** `MAGIC_FAIL` alır ve MP harcamaz (KI-016'nın uçan skill sonucu; F4-25'te Static orb `srv_fail` ile aynı yol). Yalnızca `CastTime == 0` olan bir uçan skill'de ilk paket FLYING olur ve MP'yi ağaç denetiminden önce düşer (bot sınıflarında böyle bir uçan alan skill'i yok).
  - `GameServer/MagicInstance.cpp:1276-1307`, `:1337-1339`, `:1599`, `:1611-1613`, `:1635-1681` — alan hedef seçimi ve yayını: F4-29 ile aynı (`bFlyingEffect` bu aralıkta okunmaz; `grep bFlyingEffect GameServer/MagicInstance.cpp` yalnızca `:1003`'te). Yani uçan alan EFFECTING'i uçmayan alanla **bayt bayt aynı** sunucu yolundan geçer.
  - `GameServer/Bot/ActionExecutor.cpp` (`78ae6b7`): `BeginCast` destek kuralı `:733-737` (**`CastMoralSupported(m->bMoral, m->bFlyingEffect)` `:737`**); `TickCast` `flying` `:812`, `area`/`sent` `:815-817`, `c.msp = CastManaNeed(Msp, true)` (FLYING öncesi) `:886-887`, `sData` `:898-901`, CASTING `:915-925`, FLYING `:949-975`, uçuş bekleme `:977-989`, EFFECTING `:991-1002`; `SubmitCast` `:585-677`.
  - `BotCore/BotCombat.h:258-315` (uçan akış: `kFlightMinMs`, `IsFlyingCast`, `CastManaNeed`, `CheckCastFly`, `CheckCastLand`), `:329-361` (alan: `IsAreaMoral`, `CastMoralSupported`, `CastTargetIdField`, `CastCoordField`); `Tests/BotCoreTests/CombatTests.cpp` `Combat_CastMoral_Supported` (`:1171`), `Combat_AreaCast_Fields` (`:1203`, dosyanın son testi). Toplam birim test **106** (F4-29 sonrası).
- **Veri notu (Claude yerel `MAGIC`/`MAGIC_TYPE3`/`MAGIC_TYPE4` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** Bot sınıflarında `Moral 10` + `FlyingEffect != 0` skill'leri **yalnızca şu üç** (El Morad karşılıkları `2xxxxx`; hepsi `UseItem 0`, `Etc 0`, `UseStanding 0`, `ReCastTime 1`, `CastTime 15`, `Msp 150`, `Range 90`, `SkillLevel 33`):
  - `110533` **Fire burst** (`{3, 0}`, `FlyingEffect 191`, `Skill 1105`, `Radius 8`, `FirstDamage −588`, ateş),
  - `110633` **Ice burst** (`{3, 4}`, `FlyingEffect 291`, `Skill 1106`, Type3 `Radius 8`, `FirstDamage −412`; Type4 `BuffType 6`, `Radius 5`, `Duration 15`),
  - `110733` **Thunder burst** (`{3, 0}`, `FlyingEffect 391`, `Skill 1107`, `Radius 8`, `FirstDamage −412`).
  Başka `Moral` 4–13 + `FlyingEffect != 0` skill'i bot sınıflarında yoktur (sorgu: `FlyingEffect <> 0 AND Moral BETWEEN 4 AND 13`, sınıf `106/110/112/206/210/212`; Moral 7'ler tek hedefli F4-25/26 skill'leridir).
- **Bot karakter notu (`db/002_bot_characters.sql:124-136`, F4-29 planı §2'de doğrulandı).** Skill ağacı baytları: `BotMF` ateş (`1105`) **70**, buz (`1106`) 52, yıldırım (`1107`) **0**; `BotMI` ateş 52, buz **70**, yıldırım **0**. Fire burst/Ice burst seviye 33 ⇒ her iki mage ile atılabilir (52 ≥ 33); **Thunder burst iki profille de atılamaz** (ağaç 0, KI-016): bot `BeginCast`'te kabul eder, sunucu EFFECTING'te reddeder (sınama §7 S5-b).
- **Menzil notu.** `Range 90` m: hedef noktası bu mesafe içinde seçilebilir (gerçek test alanı bunun çok altındadır). `Radius 8` (Ice burst Type4 `Radius 5`): kurbanlar hedef noktasının 8 m (Type4 5 m) içinde olmalıdır; sınama botları aynı noktada doğar (F4-29 S1 gibi).

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `CastMoralSupported(uint8_t moral)` (yalnızca `moral` parametresi; `flyingEffect` kaldırılır), `Moral` 10 uçan/uçmayan fark etmeksizin kabul. Yorumlar güncellenir (§5.1). Mevcut test güncellenir, bir yeni test eklenir (106 → 107).
2. **`BeginCast` (`ActionExecutor.cpp`):** çağrı `CastMoralSupported(m->bMoral)` olur. Başka koşul değişmez: `(m->bFlyingEffect != 0 && !flyingCast)` Type3 olmayan uçan skill'leri hâlâ reddeder (`{4, 0}` uçan, Type2 okçu).
3. **Yorumlar (`ActionExecutor.h`):** `BeginCast`/`TickCast` açıklamaları uçan alanı da kapsar (§5.3).
4. **Sonuç sözleşmesi (§5.4):** uçan alan akışı (CASTING → FLYING → EFFECTING, hedef kimliği `-1`, MP 2 ×, `victims`, iptal, ağaç yetersizliği) belgelenir (`docs/03` MEC-MAG-17) ve çalışma zamanında sınanır.

**Kapsam dışı (yapılmayacak)**

- `Moral` 11, 12, 13 (dost/hepsi/kendi merkezli alan), party hedefli (4, 6), NPC (5), klan (14, 15), `UseItem` gerektiren alan skill'leri (Discountis, Minor Resist, Elysian Web: dilim 6), `{7, 0}` alan (Type7: Ek 1 madde 3), `{1, 4}`/`{2, 4}` karışık çiftler, **okçu Type2 skill'leri** (dilim 2b: ok tüketimi, yay denetimi; Type2 uçan skill'de MP düşmez, `:1003`), uçan **Type4** skill'ler (`IsFlyingCast` yalnızca `bType[0] == 3`; `BeginCast` bunları `unsupported_skill` ile reddetmeye devam eder).
- **Uçuş süresinin mesafeye bağlılığı:** bot FLYING'den ≥ `kFlightMinMs` (1000 ms) sonra EFFECTING gönderir (CLI-03, tek ölçüm `[A]`). Sunucu uçuş süresini denetlemez; gerçek istemci mesafeye göre farklı bekleyebilir. Bu planda `kFlightMinMs` **değişmez**; mesafeye bağlı uçuş modeli gerekiyorsa ayrı plan.
- **İstemcinin EFFECTING ile aynı anda gönderdiği `opcode 4`, `sData[3] = -101` paketi** (CLI-03 satır 425): sunucu `MAGIC_FAIL`'i yalnızca iletir (`MagicInstance.cpp:40-43`); bot bunu uçmayan skill'lerde de göndermiyor, bu planda da eklenmez.
- Hedef noktası seçimi (kümenin ağırlık merkezi, r = 8 içinde ≥ 2 düşman kuralı, kenar payı): karar katmanı işidir (F6, `docs/08` §6.3). Düz koordinat girişi (`cast <bot> <skill> at <x> <z>`) yok; `BotManager.cpp` ve komut sözdizimi **değişmez**.
- Skill ağacı puanı denetimi (KI-016) ve `UseStanding` anlamı (KI-017), güvenli bölge denetimi (bot yalnızca sunucu reddini görür), yeni komut/ini anahtarı/telemetri olayı türü/alan, `BotSession.*` değişikliği, `tools/` betik değişikliği.
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | `CastMoralSupported` imzası ve yorumu (`// --- area cast ...` bölümü) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | `Combat_CastMoral_Supported` yeni imzaya uyarlanır; `Combat_FlyingArea_Guard` eklenir (106 → 107) |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yalnızca `BeginCast` çağrısı (`:737`) ve çevresindeki yorum |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `BeginCast`/`TickCast` yorumları |

`BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `ScenarioRunner.*`, `ScriptRunner.*`, `tools/` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`// --- area cast ... ---` bölümünde `CastMoralSupported`'ı ve üstündeki yorumu şöyle değiştir (ASCII, CRLF, tab, `namespace BotCore` içinde; yeni `#include` yok; **`std::min`/`std::max` kullanma**):

```cpp
	// Morals BeginCast accepts: 1 self, 2 friend-with-me, 7 enemy, 8 all (F4-03) and 10 area-enemy (F4-29, flying or not:
	// the flying area skills Fire/Ice/Thunder burst run the same packet shape through CASTING -> FLYING -> EFFECTING,
	// F4-30). Whether the skill may fly at all is decided by the caller (IsFlyingCast: Type3 only).
	inline bool CastMoralSupported(uint8_t moral)
	{
		if (moral == 1 || moral == 2 || moral == 7 || moral == 8)
			return true;

		return IsAreaMoral(moral);
	}
```

Bölümün başındaki `kMoralAreaEnemy`/`IsAreaMoral` yorumundaki "the bot only opens Moral 10 so far" cümlesi aynen kalabilir. Dosyanın kalanı (uçan akış bölümü, `CastTypesSupported`, `IsGatedType`, ...) **değişmez**. `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

a) **`Combat_CastMoral_Supported` (var olan test) yeni imzaya uyarlanır:** her `CastMoralSupported(x, y)` çağrısı `CastMoralSupported(x)` olur; `true` kümesi `1, 2, 7, 8, 10`; `false` kümesi `0, 3, 4, 5, 6, 9, 11, 12, 13, 14, 15, 25`. İki satır **kaldırılır** (artık anlamsız): `(7, 191)` `true` ve `(10, 191)` `false`. `IsAreaMoral` denetimleri aynen kalır.

b) **Yeni `Combat_FlyingArea_Guard`** (dosyanın sonuna, `Combat_AreaCast_Fields`'ten sonra; Fire burst `110533`: `Msp 150`, `CastTime 15`, `Range 90`, `FlyingEffect 191`, `Moral 10`):
   - Sınıflandırma: `IsFlyingCast(3, 191) == true`, `IsFlyingCast(3, 291) == true`, `IsFlyingCast(3, 391) == true`; `IsAreaMoral(10) == true`; `CastMoralSupported(10) == true`; `CastTypesSupported(3, 0) == true`, `CastTypesSupported(3, 4) == true` (Fire/Thunder burst `{3, 0}`, Ice burst `{3, 4}`); `CastTypesSupported(4, 0) == true` ama `IsFlyingCast(4, 291) == false` (uçan Type4 `BeginCast`'te reddedilmeye devam eder; mantık tek başına `IsFlyingCast` ile korunur).
   - Paket alanları (uçan alanda da): `CastTargetIdField(true, 2990) == -1`; `CastCoordField(true, true, 80.9f) == 80`.
   - MP: `CastManaNeed(150, true) == 300`; `CastManaNeed(150, false) == 150`.
   - **Başlangıç guard'ı** (`CastStartCheck c = {}`; alanlar `Combat_AreaCast_Fields`'taki gibi, `c.skillRange = 90`, `c.msp = CastManaNeed(150, true)`, `c.reCastMs = 100`, `c.typeGated = true`, `c.mana = 300`, `c.distanceM = 0.0f` (alan `self`), `c.standing = true`, `c.needsStanding = false`, `c.actionsInWindow = 0`): `CheckCastStart(c) == CAST_OK`; `c.mana = 299` ⇒ `CAST_REJECT_NO_MANA`; `c.mana = 300; c.distanceM = 89.9f` ⇒ `CAST_OK`; `c.distanceM = 90.0f` ⇒ `CAST_REJECT_OUT_OF_RANGE`.
   - **FLYING/iniş guard'ı:** `CheckCastFly(true, 1580, 15, 300, 300, 0) == CAST_OK`; `CheckCastFly(true, 1579, 15, 300, 300, 0) == CAST_REJECT_TOO_EARLY`; `CheckCastFly(true, 1580, 15, 299, 300, 0) == CAST_REJECT_NO_MANA`; `CheckCastFly(false, 1580, 15, 300, 300, 0) == CAST_REJECT_OUT_OF_RANGE`; `CheckCastLand(true, 1000, 150, 150, 0) == CAST_OK` (FLYING ilk yarıyı düştükten sonra `manaNeed = Msp`); `CheckCastLand(true, 999, 150, 150, 0) == CAST_REJECT_TOO_EARLY`; `CheckCastLand(true, 1000, 149, 150, 0) == CAST_REJECT_NO_MANA`.
   - Test adı ve enum sabit adları `BotCombat.h`'dekiyle birebir kullanılır (`CAST_OK`, `CAST_REJECT_TOO_EARLY`, `CAST_REJECT_OUT_OF_RANGE`, `CAST_REJECT_NO_MANA`); farklıysa dosyadaki adı kullan.
   - Toplam test sayısı **107**.

### 5.3 `ActionExecutor.cpp` ve `ActionExecutor.h`

**a) `BeginCast` (`ActionExecutor.cpp:729-737`).** Yalnızca son satırı değiştir:

```cpp
	bool flyingCast = BotCore::IsFlyingCast(m->bType[0], m->bFlyingEffect);
	if (!BotCore::CastTypesSupported(m->bType[0], m->bType[1])
		|| (m->bFlyingEffect != 0 && !flyingCast)
		|| m->iUseItem != 0
		|| !BotCore::CastMoralSupported(m->bMoral))
```

`TickCast`, `SubmitCast`, `CancelCast`, `RejectCast` ve diğer her şey **değişmez** (uçan alan `flying` ve `area` bayraklarının birleşimidir). `bad_target` bloğu (`:754-762`) değişmez.

**b) `ActionExecutor.h` yorumları.** `BeginCast` yorumundaki destek cümlesinden "flying area skills are not" ibaresini kaldır ve "flying area skills (Fire/Ice/Thunder burst: Moral 10 + Type3 FlyingEffect, run as CASTING -> FLYING -> EFFECTING with target id -1; ADR-0017 Ek F4-30) are supported" ekle. `TickCast` yorumuna: "flying area: FLYING and EFFECTING carry the same target id -1 and aim point; MP is charged at FLYING and again at EFFECTING (2 x Msp, docs/03 MEC-MAG-12/-17); 'victims' counts the per-victim EFFECTING packets of the EFFECTING step only". Yorum dışında başlıkta değişiklik yok.

### 5.4 Sonuç sözleşmesi (`docs/03` MEC-MAG-17 ile aynı)

Uçan alan skill'i (`Moral` 10, `FlyingEffect != 0`, hedef kimliği `-1`, hedef noktası `sData[0]`/`sData[2]`):

| Aşama | Sunucu | Bot sonucu |
|---|---|---|
| CASTING (`CastTime 15`) | `UserCanCast` alan moral'inde `-1`'i kabul eder; `SendSkill(true)` bölgeye | `casting` (`op:1`) |
| ≥ 1580 ms sonra FLYING | MP yeterliyse `MSpChange(-Msp)` (**ilk yarı**), `SendSkill(true)` (bölgeye FLYING yayını, hedef `-1`); Type2 tablosunda olmadığı için ok/silah denetimi yok | `flying` (`op:2`), `CastFly` `ACTION_SUBMIT` `"target":-1` |
| ≥ 1000 ms sonra EFFECTING, kurban(lar) var | `IsAvailable()` (ağaç/seviye/silah denetimi burada), MP ikinci kez (`:1028-1029`), kurban başına hedef kimlikli EFFECTING, sonra hedef `−1`'li son EFFECTING (`{3, 0}`); `{3, 4}`'te kurban paketlerini Type4 yayınlar (`code` = süre) | `effected`, `code` 0 (`{3, 4}`: Type4 süresi), `victims` = kurban kimlikli EFFECTING sayısı (yalnızca bu adımın paketleri; FLYING yayını sayılmaz: `OnPacket` her `SubmitCast`'te sıfırlanır) |
| EFFECTING, kurban yok | MP yine düşer, `SendSkill()`; bekleme/tip damgası yazılır (Type3) | `effected`, `code` 0, `victims` 0 |
| **Toplam MP** | FLYING'de `Msp`, EFFECTING'te `Msp` | **2 × `Msp`** (Fire burst: 300); başlangıç guard'ı `2 × Msp` ister (`CastManaNeed`) |
| Skill ağacı yetersiz (KI-016; Thunder burst `BotMF`/`BotMI`) | CASTING'te `IsAvailable()` `false` ⇒ `MAGIC_FAIL` (`CastTime 15`); FLYING'e hiç gidilmez | CASTING'te `srv_fail`, seri `FAILED`; MP düşmez (`CastTime == 0` olan bir uçan skill'de FLYING'de MP kaybı olurdu, bot sınıflarında yok; §8-d) |
| Hedef noktası `>= sRange` | bot guard CASTING/FLYING/EFFECTING öncesi reddeder (`FAIRNESS_REJECT` `out_of_range`) | `REFUSED`/`out_of_range`, paket gitmez |
| CASTING'te `cast <bot> off` | iptal paketi hedef `-1`, `sData[3] = -100` | `cancelled` (`op:4`, `code:-100`), MP düşmez |
| FLYING'de `cast <bot> off` | paket gitmez (F4-25: mermi havada, sunucuda iptal edilecek durum yok) | seri sessiz düşer; **FLYING'in `Msp`'si geri gelmez** (§8-c) |
| Güvenli bölge (`:355`, `:460`) | CASTING/FLYING'te `UserCanCast`/`CheckSkillPrerequisites` `false` ⇒ `MAGIC_FAIL` | `srv_fail` (CASTING'te seri `FAILED`) |

`reason` metinleri, `CastOutcome::Kind` değerleri ve `TickCast` akışı bu tablo için **değişmez**; yeni çıktı yoktur.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (değişen dört dosya için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h`'yi içeren `.cpp`'ler de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastMoral_Supported` ve `Combat_FlyingArea_Guard` adlarını `[ OK ]` ile içerir ve toplam test sayısı **107**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-30 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş.
- [ ] K5: `BeginCast`: `grep -n "CastMoralSupported" GameServer/Bot/ActionExecutor.cpp` tam bir eşleşme verir ve `CastMoralSupported(m->bMoral)` biçimindedir (ikinci argüman yok); `grep -rn "CastMoralSupported(.*,.*)" BotCore GameServer Tests` boş; `(m->bFlyingEffect != 0 && !flyingCast)` koşulu yerinde.
- [ ] K6: gerilemesiz: `git diff gece/2026-10-02...bot/F4-30 -- GameServer/Bot/ActionExecutor.cpp` yalnızca `BeginCast` çağrı satırını ve (varsa) çevresindeki yorumu içerir; `TickCast`/`SubmitCast` hunk'ı yok; `git diff ... --stat` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*` değişmemiş.
- [ ] K7: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı türü ve alanı yok (`git diff ... | grep '^+' | grep 'Emit('` boş).
- [ ] K8: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); dört dosya CRLF kalır (ASCII); `git diff --check` boş.
- [ ] K9: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 106 testin tamamı (güncellenen biri dahil) hâlâ geçiyor.
- [ ] K10: `python3 tools/check-perception-contract.py` `RESULT: PASS` (`R1`..`R5` ihlal sayıları 0; denetlenen dosya sayısı değişmez).
- [ ] K11 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S6 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-30
git diff gece/2026-10-02...bot/F4-30 -- BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff gece/2026-10-02...bot/F4-30 -- GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotManager.cpp GameServer/Bot/Telemetry.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -rn "CastMoralSupported" BotCore GameServer Tests
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff --check gece/2026-10-02...bot/F4-30
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; komut başına ≥ 1 sn). Botlar (hepsi zone 71): Karus mage **`BotMF_K`** (Fire burst, Ice burst, Thunder burst), Karus mage **`BotMI_K`** (Ice burst), El Morad kurbanlar **`BotWP_E`**, **`BotWG_E`**, **`BotPHD_E`** (HP 32000). Önce `list` ile konumları denetle (bir bot despawn'da son konumunu saklar; F4-29 bulgu 3); botlar güvenli bölgede başlıyorsa her cast `srv_fail` verir. MP/HP `list`'ten, debuff `snap <bot>`'tan, olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'tan. Skill bekleme süreleri kısa (`ReCastTime 1`) ama tip kapısı 1 sn; her senaryo arasında botlar `despawn`/`spawn` ile yenilenir. MP kesin değeri sunucu yenilemesiyle (~5-6 MP/sn) karışır: MP'yi cast'ten hemen önce ve FLYING/EFFECTING sonrası `list` ile al, yenileme payını raporla.

1. **S1 Kurbanlı uçan alan, `{3, 0}` (Fire burst):** `spawn BotMF_K,BotWP_E,BotWG_E,BotPHD_E`; MP ve düşman HP `list` ile not; `cast BotMF_K 110533 BotWP_E 1` (hedef noktası = `BotWP_E`'nin konumu). Beklenen JSONL: `CastStart` `ACTION_SUBMIT` **`"target":-1`** → `ACTION_RESULT` `ok:true`, `reason:"casting"`, `op:1`; ≥ 1580 ms sonra `CastFly` `ACTION_SUBMIT` `"target":-1`, `since_casting_ms` ≥ 1580 → `ok:true`, `reason:"flying"`, `op:2`; ≥ 1000 ms sonra `CastEffect` `ACTION_SUBMIT` `"target":-1`, **`since_flying_ms` ≥ 1000** → `ok:true`, **`reason:"effected"`, `op:3`, `code:0`, `victims` ≥ 3** (üç El Morad bot + varsa yakın NPC'ler); log `cast finished (effected) after 1 cycle(s), 1 ok, 3 packet(s) sent`; **MP düşüşü ≈ 300 (150 FLYING sonrası + 150 EFFECTING sonrası)** (FLYING sonrası ara MP `list`'ten; yenileme payı ≤ ~12); üç kurbanın HP'si düşmüş; `snap BotMF_K events` kurban kimlikli `op=3 skill=110533 ... target=<kurban>` olaylarını, hedef `−1`'li son olayı ve `op=2` FLYING olayını gösterir.
2. **S2 Boş alan ve `self`:** `spawn BotMF_K` (düşman yok): `cast BotMF_K 110533 self 1`: üç pakette `"target":-1`; `CastEffect` `effected`, `code:0`, **`victims:0`**; MP yine ≈ −300 (boş alan başarılı, MP düşer). Aynı oturumda ikinci `cast BotMF_K 110533 self 1`: tip kapısı/bekleme guard'ı yoksa gider (`ReCastTime 1`), ≥ 1 sn sonraya kadar `FAIRNESS_REJECT` (`MEC-MAG-03`) görülebilir. `spawn BotWP_E,BotWG_E` sonra (düşman çağıranla aynı noktada): `cast BotMF_K 110533 self 1` (taze oturum) `victims` ≥ 2.
3. **S3 Çift tipli uçan alan `{3, 4}` (Ice burst):** `spawn BotMI_K,BotWP_E,BotWG_E` (BotMI buz ağacı 70): `cast BotMI_K 110633 BotWP_E 1`: üç paket (`casting` → `flying` → `effected`), **`code` = Type4 süresi (~15)**, `victims` ≥ 2, MP ≈ −300, `snap BotWP_E`/`BotWG_E` `buff skill=110633 type=6 debuff` (yavaşlatma, `Radius 5`: kurbanlar hedef noktasında olmalı). `BotMF_K` ile aynı skill (buz 52 ≥ 33) de atılır.
4. **S4 CLI-07 hedef noktası menzili:** `spawn BotMF_K,BotWP_E`; `BotWP_E`'yi `move` ile çağırandan ≥ 95 m uzağa taşı ve `cast BotMF_K 110533 BotWP_E 1` ⇒ **paket gitmeden** `FAIRNESS_REJECT` (`out_of_range`, `limit 90`; `ACTION_SUBMIT` yok), log `cast stopped (out_of_range)`, MP değişmez. (Test alanında 95 m'ye ulaşılamıyorsa bu senaryo yerine hedef kimliğinin menzil dışı bir nokta olduğu birim test kapsamında bırakılır ve raporda yazılır; `Radius 8` yüzünden 85-89 m'de kurbansız `victims 0` ile geçmesi de kabul edilir.)
5. **S5 İptal ve ağaç yetersizliği:** (a) `cast BotMF_K 110533 BotWP_E 1` verip CASTING aşamasında (~500 ms) `cast BotMF_K off`: `cancelled` (`op:4`, `code:-100`), iptal paketi **hedef `−1`**, MP değişmez. (b) FLYING aşamasında (CASTING bittikten ~300 ms sonra) `cast BotMF_K off`: paket gitmez, seri sessiz düşer; **MP FLYING'in `Msp`'si kadar eksik kalır** (−150) ve EFFECTING gitmez, hedefte hasar yok (F4-25 davranışı, §8-c). (c) **Thunder burst** `cast BotMF_K 110733 BotWP_E 1` (yıldırım ağacı 0, KI-016): bot kabul eder (`BeginCast` ağaç denetlemez); `CastStart` `ACTION_SUBMIT` → `ACTION_RESULT` `ok:false`, `reason:"srv_fail"`, `op:4`, **seri CASTING'te biter** (`CastFly` yok), MP değişmez, hedefte hasar yok. Gözlenen farklar raporlanır (sonuç beklenenden farklıysa MEC-MAG-17 düzeltilir).
6. **S6 Gerilemesiz ve hâlâ desteklenmeyenler:** F4-29 alan: `cast BotMF_K 110545 BotWP_E 1` (Inferno) üç paket değil **iki** paket (CASTING + EFFECTING), `effected`, `victims` ≥ 1, MP −200 (bir kez; uçmayan alan MP'si tek). F4-25/26: `cast BotMF_K 110515 BotWP_E 1` (Fire ball) 3 paket, MP ≈ −100, `victims` alanı **yok**; `cast BotMI_K 110615 BotWP_E 1` (Ice arrow) 3 paket, `code` ≈ 12. Ignition `110518` `effected`, `target` kurban kimliği. Hâlâ reddedilenler `unsupported_skill`: uçan Type4 `110674` (Freezing Distance, `UseItem`/`Etc`; `{4, 0}` uçan), Sleep Carpet `112751`, Discountis `112772`, Minor Resist `110825`. `TELEMETRY=summary`: uçan alan cast çalışır, JSONL'de `ACTION_*` yok; `ENABLED=0` ⇒ komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde; sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Temizlik: ini yedekten geri, geçici betikler silindi, botlar despawn, sunucular `stop`.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `BotCore/` ve `Bot/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **`Moral` 10 + Type3 `FlyingEffect != 0`** skill'i serisinde davranış değiştirir (önceden `unsupported_skill` idi); diğer skill'ler ve cast etmeyen botlar için davranış (paket biçimi ve telemetri satırı dahil) değişmez.
- **Thread kuralı (ADR-0005):** `TickCast` yalnızca IOCP thread'inde; bu planda yeni durum/kilit yok.
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `flying` yalnızca bölgeye yayınlanan çağırana da gelen `MAGIC_FLYING` paketinden, `effected` yalnızca `MAGIC_EFFECTING`'ten, `srv_fail` yalnızca `MAGIC_FAIL`'den; `victims` yalnızca çağıranın kendi oturumuna gelen kurban kimlikli `MAGIC_EFFECTING` paketlerinden. Sunucu nesnelerinden (kurban listesi, HP, MP) hiçbir şey okunmaz; MP önkontrolü bota ait `user->GetMana()` değeridir (mevcut `CheckCastStart`).
- **Bilinen sınırlar `[A]`/`[D]`:** (a) **Uçuş süresi `[A]`:** `kFlightMinMs = 1000` tek ölçümden (CLI-03: ~1037 ms) geliyor; gerçek istemci uzak hedefte daha uzun bekleyebilir. Sunucu uçuş süresini denetlemediği için bu adalet kuralı bota *daha kısıtlayıcı değil, istemciyle aynı* düzeydedir; mesafeye bağlı model ayrı plan. (b) **CLI-07 `[Ö]`:** gerçek istemcinin CASTING'te hedef noktası gönderip göndermediği ölçülmedi; bot üç pakette aynı noktayı yazar. (c) **FLYING sonrası iptal MP kaybı:** FLYING'den sonra seri iptal edilirse `Msp` bir kez düşmüş kalır (sunucu geri vermez; F4-25 ile aynı); karar katmanı uçuş başladıktan sonra iptal etmemelidir. (d) **Ağaç yetersizliği (KI-016):** `CastTime 15` olan burst'lerde sunucu `IsAvailable()`'ı CASTING'te çağırır ve reddeder; MP kaybı yoktur. Bot önkontrolü KI-016 gereği yok (Thunder burst iki mage profiliyle de `srv_fail` verir, test dışı); karar katmanı (F6) yalnızca profilinin ağacı yeten skill'leri seçer. (e) **Kenar payı** (kurban başına çağırandan `< sRange`, `:1337-1339`): `Range 90` büyük olduğundan pratikte belirleyici değildir; `Radius 8` (Ice burst Type4 5) belirleyicidir ve hedef noktası seçimi karar katmanı işidir. (f) **`victims` ≠ isabet** (NPC dahil; F4-29 §8-d). (g) **`{3, 4}` uçan alanda** son paket kurbanlardan birinde `bResult = 0` ise `MAGIC_FAIL` olabilir (F4-29 §8-c).
- Telemetri hacmi değişmez: FLYING için ek bir `ACTION_SUBMIT`/`ACTION_RESULT` çifti uçan skill'lerde zaten vardır (F4-25); `tools/bot-telemetry-report.py` değişmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-30` (taban: `gece/2026-10-02`) — kod `9d0c86b [F4-30] Ucan alan skill dilimi: CastMoralSupported flyingEffect kosulunu kaldirir (Fire/Ice/Thunder burst)`; bu rapor ayrı commit.
- Değişen dosyalar ve neden:
  - `BotCore/BotCombat.h` — `CastMoralSupported(uint8_t moral, uint16_t flyingEffect)` → `CastMoralSupported(uint8_t moral)`; `Moral` 10 uçan/uçmayan ayrımı olmadan `IsAreaMoral` ile kabul edilir. Üstteki yorum uçan alanı kapsayacak şekilde güncellendi.
  - `Tests/BotCoreTests/CombatTests.cpp` — `Combat_CastMoral_Supported` tek argümanlı imzaya uyarlandı (`(7, 191)` ve `(10, 191)` satırları kaldırıldı); yeni `Combat_FlyingArea_Guard` eklendi (sınıflandırma, paket alanları, `CastManaNeed`, `CheckCastStart`/`CheckCastFly`/`CheckCastLand`), 106 → 107.
  - `GameServer/Bot/ActionExecutor.cpp` — yalnızca `BeginCast` destek koşulunda çağrı `CastMoralSupported(m->bMoral, m->bFlyingEffect)` → `CastMoralSupported(m->bMoral)`. Başka değişiklik yok.
  - `GameServer/Bot/ActionExecutor.h` — `BeginCast` yorumundan "flying area skills are not" kaldırıldı, uçan alan desteği eklendi; `TickCast` yorumuna uçan alan cümlesi eklendi.
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
    CombatTests.cpp
    BotManager.cpp
    BotSession.cpp
    ScenarioRunner.cpp
    Kod üretiliyor
    BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
    proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  Dört değişen dosya `touch` edilip yeniden derlendi; `grep -iE "warning|error"` çıktısı boş.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ — Release hatasız; dört dosya `touch` ile yeniden derlendi, değişen dosyalarda uyarı/hata yok.
  - K2 ✔ — Debug hatasız bitti.
  - K3 ✔ — `Release` ve `Debug` `107 tests, 0 failed`; çıktıda `[ OK ] Combat_CastMoral_Supported` ve `[ OK ] Combat_FlyingArea_Guard`.
  - K4 ✔ — `grep windows.h|stdafx|GameServer|shared/` boş; `std::min`/`std::max` eklenmedi; include'lar değişmedi.
  - K5 ✔ (literal grep hariç, bkz. sapmalar) — `ActionExecutor.cpp`'de `CastMoralSupported(m->bMoral)`; `(m->bFlyingEffect != 0 && !flyingCast)` yerinde.
  - K6 ✔ — diff yalnızca §4'teki 4 dosya; `ActionExecutor.cpp` tek hunk (`BeginCast` çağrısı), `TickCast`/`SubmitCast` hunk'ı yok; `BotSession.*`, `BotManager.cpp`, `Telemetry.*`, proje dosyaları farkı boş.
  - K7 ✔ — yeni `Emit(` yok; yeni ini anahtarı/komut/thread/olay türü/alan yok.
  - K8 ✔ — dört dosya ASCII + CRLF; `git diff --check` boş.
  - K9 ✔ — guard çağrı sayaçları `CheckMoveStep=2`, `CheckAttack=1`, `CheckCastStart=1`, `CheckCastEffect=1`, `CheckCastFly=1`, `CheckCastLand=1`, `CheckCastCancel=1`, `CheckPotion=1`; 107 testin tamamı geçiyor.
  - K10 ✔ — `tools/check-perception-contract.py` `RESULT: PASS` (R1 0/0, R2 0/28, R3 0/18, R4 0/0, R5 0/0; 19 dosya).
  - K11 — Claude `/plan-dogrula` çalışma zamanı (S1–S6); DeepSeek yapmaz.
- Plandan sapmalar ve gerekçeleri:
  - K5'teki ikinci grep (`grep -rn "CastMoralSupported(.*,.*)" BotCore GameServer Tests` boş olmalı) plan metnindeki düzenli ifade nedeniyle **test satırlarını da yakalar**: `CHECK_EQ(BotCore::CastMoralSupported(1), true)` satırındaki virgül dıştaki `CHECK_EQ`'ye aittir, fonksiyona ikinci argüman değildir. Daha kesin `grep -rnE "CastMoralSupported\([^)]*,[^)]*\)"` **boş** döner; yani gerçek iki argümanlı çağrı yoktur, kriter amaç olarak karşılanır.
  - Plandaki test sınıflandırma maddesinin bir kısmı (`IsFlyingCast(4, 291) == false`) tek başına `IsFlyingCast` ile doğrulandı; uçan Type4'ün `BeginCast`'te reddi `(m->bFlyingEffect != 0 && !flyingCast)` koşuluna dayanır ve bu koşul değişmedi (K5).
- Açık sorular: yok. Çalışma zamanı doğrulaması ve `docs/03` MEC-MAG-17 etiketi Claude'a aittir (K11, §5.4/§7).

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-30` @ `497d59d` (kod `9d0c86b`). Çalışma ağacı temizdi; otonom gece döngüsünde (`AUTO_LOOP=1`) birleştirme/push yapılmadı, birleştirmeyi döngü betiği `gece/2026-10-02`'ye yapar.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | dört dosya `touch` + `tools/build.sh Release` rc=0; yeniden derlenenler `ActionExecutor.cpp`, `CombatTests.cpp`; tek uyarı eski `UpgradeHandler.cpp` C4789 (bu plan dokunmadı) |
| K2 | ✔ | `tools/build.sh Debug` rc=0, uyarı yok |
| K3 | ✔ | `run-tests.sh Release` ve `Debug` ikisinde `107 tests, 0 failed`; `[ OK ] Combat_CastMoral_Supported`, `[ OK ] Combat_FlyingArea_Guard` |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` boş; eklenen kod satırlarında `std::min/std::max/Emit(` 0; include değişmedi |
| K5 | ✔ | `grep CastMoralSupported` (CHECK_EQ hariç) yalnızca tanım `BotCombat.h:343` ve çağrı `ActionExecutor.cpp:737` `CastMoralSupported(m->bMoral)`; `grep -rnE "CastMoralSupported\([^)]*,[^)]*\)"` boş (plandaki geniş regex test satırlarını da yakalar: uygulayıcının sapma notu doğru); `(m->bFlyingEffect != 0 && !flyingCast)` yerinde (`:735`) |
| K6 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-30`: yalnızca §4'teki 4 dosya + plan dosyası; `ActionExecutor.cpp` tek hunk (`:737`), `TickCast`/`SubmitCast` hunk'ı yok; `BotSession.*`, `BotManager.cpp`, `Telemetry.*`, vcxproj farkı yok |
| K7 | ✔ | eklenen kod satırlarında `Emit(` yok; yeni ini anahtarı/komut/thread/olay türü yok; `ENABLED=0` çalışma zamanında sınandı |
| K8 | ✔ | `file`: dört dosya ASCII + CRLF; `git diff --check` boş (`CHECK_OK`) |
| K9 | ✔ | `CheckMoveStep` 2; `CheckAttack`, `CheckCastStart`, `CheckCastEffect`, `CheckCastFly`, `CheckCastLand`, `CheckCastCancel`, `CheckPotion` 1'er; 107 testin tamamı geçiyor |
| K10 | ✔ | `check-perception-contract.py` `RESULT: PASS` (R1 0/0, R2 0/28, R3 0/18, R4 0/0, R5 0/0) |
| K11 | ✔ | S1–S6 çalışma zamanında geçti (aşağıda; S1'de `victims` 2, plandaki ≥ 3 değil: bulgu 2) |

**Çalışma zamanı** (Release, `ENABLED=1`, `MAX_BOTS=16`, `TELEMETRY=decisions`, zone 71; `Logs/bots/2026-10-03/live-030643.jsonl`, `Logs/Bot_3_10_2026.log`; ini yedekten geri yüklendi, sunucular durduruldu):

- **S1 ✔** Fire burst `110533` BotMF_K → BotWG_E (hedef noktası = kurbanın konumu, BotPHD_E aynı noktada): `CastStart` `ACTION_SUBMIT` `"target":-1`, `casting` `op 1`; `CastFly` `"target":-1`, `since_casting_ms 1650`, `flying` `op 2`; `CastEffect` `"target":-1`, `since_flying_ms 1099`, `effected`, `op 3`, `code 0`, **`victims 2`**; log `cast finished (effected) after 1 cycle(s), 1 ok, 3 packet(s) sent`; **MP 6021 → 5871 (FLYING sonrası) → 5721 (EFFECTING sonrası) = −150 − 150**; iki kurbanın HP'si düştü (2891/5650, 2096/3491 öncesi 3134/5650, 2370/3491); `snap BotMF_K events`: kurban kimlikli iki `op=3 skill=110533 target=2986/2987`, hedef `-1`'li son `op=3`, `op=2` ve `op=1` olayları (üçünde `d0=1274 d2=925` hedef noktası).
- **S2 ✔** Boş alan (`self`, düşman yok): üç pakette `"target":-1`, `effected`, `code 0`, **`victims 0`**; MP 5691 → 5541 (FLYING sonrası −150, EFFECTING sonrası −150; boş alanda MP yine düşer). İki döngülü `cast BotMF_K 110533 self 2`: ikinci `CastStart` ilk `CastEffect`'ten 1,1 sn sonra (tip kapısı), iki döngü `effected`, `victims 1` (BotPHD_E; BotWG_E 110 m uzakta, yarıçap dışı).
- **S3 ✔** Ice burst `110633` `{3, 4}` BotMI_K: `casting` → `flying` → `effected`, **`code 15`** (Type4 süresi), `victims 2`; MP 5861 → 5751 → 5601 (−260, ~8 sn'lik sunucu MP yenilemesi dahil); `snap BotWG_E`/`BotPHD_E` `buff skill=110633 type=6 debuff remain=10s/9s`. BotMF_K aynı skill'i (buz ağacı 52 ≥ 33) attı: `code 15`, `victims 2`.
- **S4 ✔** hedef 110 m uzakta: `FAIRNESS_REJECT` `MEC-MAG-11` `out_of_range` `value 110.00`, `limit 90.00`; `ACTION_SUBMIT` yok; log `cast stopped (out_of_range)`; MP değişmedi.
- **S5 ✔** (a) CASTING'te (1102 ms) `cast off`: `CastCancel` `"target":-1`, `cause:"cmd"`, `cancelled`, `op 4`, `code -100`; MP 5901 → 5901. (b) FLYING aşamasında `cast off`: `CastStart` + `CastFly` gitti, EFFECTING gitmedi, log `cast stopped after 2 packet(s) sent`; **MP 6021 → 5871 (FLYING'in 150'si geri gelmedi)**, kurban HP'si değişmedi (2716). (c) Thunder burst `110733` (yıldırım ağacı 0, KI-016): `CastStart` `ACTION_RESULT` `ok:false`, `reason:"srv_fail"`, `op 4`, `code -100`; `CastFly` yok (seri CASTING'te bitti), MP değişmedi (yalnızca yenileme), hedef HP değişmedi.
- **S6 ✔** Inferno `110545`: **iki** paket (`casting`, `effected`), `victims 2`, MP 5991 → 5821 (−200 + yenileme, bir kez). Fire ball `110515`: üç paket, `victims` alanı yok. Ice arrow `110615` (BotMI_K): üç paket, `code 12`, `victims` yok. Ignition `110518`: `CastEffect` `"target":2985` (kurban kimliği), `effected`, `victims` yok. Reddedilenler (`refused unsupported_skill`): uçan Type4 `110674`, `110825`; Sleep Carpet `112751` ve Discountis `112772` BotPHD_K ile `unsupported_skill` (BotMF_K ile `bad_skill`: sınıfın skill'i değil, plan senaryosunun kurulum hatası, bulgu 3). `TELEMETRY=summary`: Fire burst `self` `cast finished (effected) ... 3 packet(s)`, JSONL'de `ACTION_*` 0 (yalnızca `PERF_SAMPLE`); `ENABLED=0`: `BotCommands.txt` işlenmedi (dosya kaldı), `Bot_*.log`'a satır ve yeni JSONL yok; `PERF_SAMPLE` `tick_p50_us` 4–6, `tick_p95_us` 98–106; `GameServer.log`'da yeni hata yok.

- Bulgular (önem sırasıyla; engelleyici yok):
  1. *(not)* **Sonuç sözleşmesi doğrulandı, `docs/03` MEC-MAG-17 `[D]` → `[V]`:** uçan alan akışı (üç paket, hedef `-1`, MP FLYING + EFFECTING = 2 × `Msp`, `victims` yalnızca EFFECTING, `{3, 4}` `code` = Type4 süresi, ağaç yetersizliği CASTING'te `srv_fail` ve MP kaybı yok, FLYING sonrası iptalde `Msp` geri gelmez) çalışma zamanında ölçüldü. Ağaç yetersizliği yanıtında `code` `-100` görüldü (F4-29'daki Quake EFFECTING'te `-103` idi; kod CASTING/EFFECTING'e göre değişiyor, bot yalnızca `srv_fail` yazar).
  2. *(not)* S1'de `victims` 3 değil 2: `BotWP_E` DB'den ölü (`hp 0/5650`) geldi, `regene` onu El Morad doğuş noktasına (685, 920) taşıdı (~590 m, çağırandan uzak), bu yüzden S1/S3/S5 BotWG_E + BotPHD_E ile yapıldı. `victims` kuralı (yalnızca yarıçap içindeki kurban kimlikli paketler) 2 kurbanla da sınandı; üçüncü kurban ek bilgi taşımaz. Plan S1'in "≥ 3" beklentisi bu test ortamı için ≥ 2 olarak okundu.
  3. *(not)* Plan §7 S6'daki Sleep Carpet `112751`/Discountis `112772` BotMF_K ile değil BotPHD_K ile denenmelidir (`bad_skill` sınıf uyuşmazlığı); BotPHD_K ile `unsupported_skill` görüldü. Kod doğru.
  4. *(not)* MP düşüşü ~5–6 MP/sn sunucu yenilemesiyle karışır; tam −300 yalnızca MP'si dolu başlayan S1'de görüldü (6021 → 5721). S3'te −260 yenileme payıdır.
  5. *(not)* Operasyonel: bot despawn'da son konumunu saklar ve `regene` ölü botu doğuş noktasına taşır; çalışma zamanı sınamasından önce `list` ile konum/HP denetimi gerekir (F4-29 bulgu 3 ile aynı).
  6. *(not)* Uygulayıcı raporu doğru: commit listesi, dosyalar, derleme ve test sayıları kendi çalıştırmamla örtüşüyor; iki sapma notu da yerinde.
  7. Sınanmayanlar (plan kapsamı dışı/bilinen sınır): uçuş süresinin mesafe bağımlılığı `[A]`, gerçek istemcinin CASTING'te hedef noktası göndermesi (CLI-07 `[Ö]`), `{3, 4}` uçan alanda kurbanda direnç/engelli son paket `MAGIC_FAIL`, El Morad burst karşılıkları (`2xxxxx`; aynı kod yolu), güvenli bölge `srv_fail`.
- Düzeltme talimatı: yok (karar DOĞRULANDI).
