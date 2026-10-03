# F4-26: `ActionExecutor` çift tipli skill dilimi — tek hedefli Type3 + Type4 büyüler (buz büyüleri, Prismatic)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02, merge `06a76e8`) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-26` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-03 (cast dilimi: `BeginCast`/`TickCast`/`SubmitCast`, tip kapısı, `m_castEcho`) — `KAPANDI`; F4-24 (cast iptali) — `KAPANDI`; F4-25 (uçan Type3, `CAST_FLYING`) — `KAPANDI` (merge `0954929`) |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-11, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-12, MEC-MAG-13 (bu planla eklendi, `[D]`), AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 dilim 3 |
| Tahmini büyüklük | S (4 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

`BeginCast` `bType[1] != 0` olan her skill'i `unsupported_skill` ile reddediyor (`ActionExecutor.cpp:730-732`). Mage'in buz ağacındaki tek hedefli büyülerin hepsi **çift tipli** (`Type1 = 3`, `Type2 = 4`: hasar + yavaşlatma): Freeze, Chill, Solid, Frostbite, Ice comet, **Prismatic** (`110670`) ve uçanlar Ice arrow (`110615`), Ice orb (`110627`). Bu plan **yalnızca `bType = {3, 4}` çiftini**, tek hedefli (`Moral` 7), eşyasız (`UseItem == 0`), görevsiz (`Etc == 0`) skill'ler için açar. Uçan olanlar (Ice arrow/orb) F4-25'in CASTING → FLYING → EFFECTING akışını aynen kullanır.

Sunucu çift tipli skill'i EFFECTING'te iki kez yürütür (`ExecuteSkill(bType[0])` sonra `ExecuteSkill(bType[1])`, `MagicInstance.cpp:109,129`) ve **yanıt paketini yalnızca Type4 bölümü yayınlar** (`:1598` ve `:1862`): bot için üç sonuç doğar ve plan bunları açıkça ele alır (§5.4-d). Ayrıca same-type kapısı (`MEC-MAG-03`) iki tipe de uygulanır; bot bugün yalnızca `bType[0]`'a bakıyor, bu plan ikisini de kapsar. Değer: Prismatic/Ice arrow gibi mage'in ana büyüleri botla atılabilir olur (F6 mage davranışının ön koşulu), tip kapısı Type4 skill'leri (dilim 4) için de doğru zeminde durur.

Kapsam dışı kalan çift tipli ve diğer skill'ler §3'tedir. F4'ün yirmi altıncı planıdır (ADR-0018 sırası: cast iptali ✔ → uçan ✔ (2a) → **çift tipli (bu plan)** → Type4 tek tipli → alan → ...).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-26)" (bu planla birlikte yazıldı), "Ek (F4-25)" ve "Ek (F4-03)" madde 2 (desteklenen skill sınırı; bu plan çift tipli Type3+Type4 için gevşetir). `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (+ Ek 3).
- `docs/03` §4.2 **MEC-MAG-03/-08/-11/-12/-13**, §13.2 CLI-03, CLI-04, CLI-09, CLI-11. MEC-MAG-13 bu planla eklendi (`[D]`).
- `plans/F4-03-aksiyon-yurutucu-cast.md`, `plans/F4-25-aksiyon-yurutucu-ucan-skill.md` ve birleşmiş kod: **yazılı planı değil, kodu esas al.**
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `0954929` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.cpp:109-131` — `Run()` `case MAGIC_EFFECTING`: `bInitialResult = ExecuteSkill(pSkill->bType[0])`; başarılıysa (`:112-126`) `m_CoolDownList`'e skill ve **`bType[0]` ile `bType[1]` için** `m_MagicTypeCooldownList` damgası yazılır, sonra `:129` `ExecuteSkill(pSkill->bType[1])`. Type3 hedefi öldürdü/geçersiz kıldıysa Type4 hedef denetiminde `false` döner.
  - `GameServer/MagicInstance.cpp:1598-1599` — `ExecuteType3()`: skill paketini `bType[1] == 0 || bType[1] == 3` ise yayınlar; `{3, 4}` için **yayınlamaz**.
  - `GameServer/MagicInstance.cpp:1682` — `ExecuteType4()` tek hedef: `pSkillTarget == nullptr || isDead() || isBlinking()` ⇒ `return false` (yayın yok). `:1862-1874` — `bType[1] == 0 || bType[1] == 4` ise `sDataCopy = { sData[0], bResult, sData[2], sDuration, sData[4], pType->bSpeed, sData[6] }` ile `BuildAndSendSkillPacket(pTmp, true, ...)` (bölge yayını, çağıran dahil; `:763-777`). Yani çift tipli EFFECTING yankısında `sData[3]` = **süre (saniye)**, `SKILLMAGIC_FAIL_ATTACKZERO` (−104, `shared/packets.h:402`) **olamaz**: `missed` bu yoldan ayırt edilemez. `:1892-1893` `bResult == 0` ise ek `SendSkillFailed` (`MAGIC_FAIL`, çağırana) — debuff'ı engelleyen hedefte olur, nadir.
  - `GameServer/MagicInstance.cpp:332-346`, `:389-423` — tip kapısı: `bType[0]` ve `bType[1]` (1..7) için ayrı kayıt aranır, ikisinden biri `PLAYER_SKILL_REQUEST_INTERVAL` (0,7 sn, `User.h:23`) içindeyse `SkillUseFail`; `bAttribute == AttributeNone` Type3'te damgalar silinir (buz büyüleri `bAttribute = 2`, silinmez).
  - `GameServer/MagicInstance.cpp:999-1029` — MP: yalnızca `bType[0] == 4 && sTargetID != -1` istisnası; `bType = {3, 4}` için `Msp` bir kez (EFFECTING'te `:1028-1029`), uçansa FLYING'de bir kez daha (MEC-MAG-12). Type4'te ek düşüm `:1800-1802` yalnızca `bType[0] == 4`'te.
  - `GameServer/Bot/ActionExecutor.cpp` (satırlar `0954929`): `BeginCast` destek kuralı `:729-742` (`supportedType`, `m->bType[1] != 0`), `TickCast` tip kapısı girdisi `:842-852` (`type0`, `typeGated`, `hasTypeLast`, `sinceTypeLastMs`), `c.typeGated`/`c.hasTypeLast` atamaları `:871-873`, EFFECTING sonrası reuse zamanlayıcıları `:984-992` (`type0 < 8`), `SubmitCast` sonuç eşlemesi `:585-665` (**değişmez**), `:1445` pot damgası (`m_castTypeLast[3]`, **değişmez**).
  - `GameServer/Bot/BotSession.h:101-102` — `m_castTypeHas[8]`, `m_castTypeLast[8]` (tip 0..7; **değişmez**).
  - `BotCore/BotCombat.h:151-170` `CastStartCheck` (`typeGated`, `hasTypeLast`, `sinceTypeLastMs` alanlarının yorumları zaten "skill'in kapılı tiplerinin en küçüğü" der), `:258-315` uçan cast bölümü, `:317` "cast cancel and standing plan" başlığı. `Tests/BotCoreTests/CombatTests.cpp` 38 `TEST_CASE` (toplam **99**; Claude `./tools/run-tests.sh Release` ile doğruladı).
- **Veri notu (Claude yerel `MAGIC`/`MAGIC_TYPE3`/`MAGIC_TYPE4` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** `Type1 = 3`, `Type2 = 4`, `Moral = 7`, `UseItem = 0`, `Etc = 0` olan master sınıf skill'leri (Karus `110007`, `1106xx`; El Morad karşılıkları `210007`, `2106xx`): `110007` Cold wave (`Skill 1100`, seviye 7, `Msp 7`), `110603` Freeze (`Msp 20`, `CastTime 15`, `ReCast 1`, `Range 11`), `110609` Chill (30, `ReCast 53`, `Range 56`), `110615` Ice arrow (50, `FlyingEffect 291`, `Range 78`), `110618` Solid (60, `ReCast 1`), `110627` Ice orb (80, `FlyingEffect 292`, `Range 78`), `110639` Frostbite (150, `ReCast 43`), `110651` Ice comet (160, `ReCast 53`, seviye 51), `110670` **Prismatic** (`Msp 390`, `CastTime 11`, `ReCast 213`, `Range 45`, seviye 70, **`UseStanding 53`**) ve El Morad `210xxx` karşılıkları. `MAGIC_TYPE4` satırları `BuffType 6` (yavaşlatma), süre 10..19 sn. Bu planda `unsupported_skill` kalanlar: `110633` Ice burst, `110645` Blizzard, `110660` Frost nova, `110671` ice storm (`Moral` 10, alan), `110635` Ice blast, `110657` Ice Impact (`UseItem`), tüm Type1+Type3/4 (ör. Leg cutting `106520`, `Type1 = 1`/`Type2 = 4`), Type2+Type3/4 (okçu), Type1+Type9.
- **Bot karakter notu (`db/002_bot_characters.sql:124-136`, repodaki betik).** Skill ağacı puanı (`strSkill` baytı `[6]`, buz ağacı): `BotMF_K`/`BotMF_E` **52**, `BotMI_K`/`BotMI_E` **70**. Sunucu `m_bstrSkill[sSkill % 10] >= SkillLevel` ister (`MagicInstance.cpp:956-960`, KI-016): Prismatic (`1106`, seviye 70) yalnızca **MI** botlarıyla atılabilir; MF botları ile `srv_fail` alınır (bu planın kusuru değil, KI-016). Ice arrow (15), Ice orb (27), Chill, Freeze, Solid, Frostbite, Ice comet (51) iki bot türüyle de atılır.
- **`UseStanding` notu (KI-017, bu planla açıldı).** Master skill'lerin `UseStanding` değeri bu veritabanında 51..54'tür (Prismatic 53); sunucu yalnızca `== 1` (hız 0 şartı, CASTING menzil denetimi) ve `== 0` (EFFECTING/FLYING menzil denetimi) değerlerini ayırır. Bot `needsStanding = (sUseStanding == 1)` kuralını **aynen korur** (F4-24); Prismatic için "ayakta şart" uygulanmaz, bu bilinen sınırdır (§8).

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** `CastTypesSupported`, `IsGatedType`, `TypeStamp`, `MinGatedSince`. Birim testleri (`CombatTests.cpp`, +3).
2. **`BeginCast` kuralı:** desteklenen tip çifti `BotCore::CastTypesSupported(bType[0], bType[1])` ile belirlenir: tek tip 1 veya 3, ya da çift `{3, 4}`. Diğer her şey (`iUseItem`, `sEtc`, `bMoral`, uçan-ama-Type3-değil kuralı) **değişmez**.
3. **`TickCast` tip kapısı:** skill'in iki tipine de bakar (en küçük "since" kararı belirler); EFFECTING sonrası **her iki tip** için `m_castTypeHas/Last` damgası yazılır.
4. **Sonuç yorumu:** `SubmitCast` değişmez; çift tipli EFFECTING yankısı Type4'ten gelir (`code` = süre, `reason = "effected"`); yankı hiç gelmezse mevcut `"no_result"` (§5.4-d).

**Kapsam dışı (yapılmayacak)**

- Alan çift tipli skill'ler (`bMoral` 10..13: Ice burst, Blizzard, Frost nova, ice storm; hedef noktası CLI-07 — ADR-0018 dilim 5), `UseItem` gerektiren çift tipliler (Ice blast, Ice Impact), görevli skill'ler (`Etc != 0`), Type1+Type3/4 melee çiftleri, Type2 okçu çiftleri, Type1+Type9, `Type4` **tek tipli** skill'ler (dilim 4), `bType[0] == 4` ile başlayan çiftler.
- **Skill ağacı puanı denetimi** (KI-016): `BeginCast` yalnızca sınıf ve seviyeyi denetlemeye devam eder; ağaç puanı kuralı ayrı bir küçük plandır. Bu planın testleri MI botlarını Prismatic için kullanır.
- **`UseStanding` 51..54 anlamı** (KI-017): `== 1` kuralı değişmez; "ayakta şart" genişletmesi ayrı karar/plandır.
- Type4 etkisinin (yavaşlatma) bot tarafından yorumlanması: hedefin `speed`'ini bot okumaz, guard'a katmaz; hedefin buff listesi yalnızca doğrulama için `/bot snap` ile görülür (F4-17 `SelfState`, hedef botun **kendi** öz durumu).
- Çift tipli skill'in "hasar isabet etti mi" bilgisini yankıdan çıkarmak (yankı Type4'ten gelir, hasar bilgisi taşımaz). Hasar/ölüm algısı F4-50/F4-51/F4-52'nin işidir.
- Yeni komut, ini anahtarı, yeni telemetri olayı türü veya alanı, `list`/`snap` çıktısı değişikliği, `BotManager.cpp`/`BotSession.*` değişikliği, `tools/` betik değişikliği.
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | yeni "dual-typed cast" bölümü (`CheckCastLand`'den sonra, `// --- cast cancel and standing plan` başlığından önce) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | 3 yeni test (99 → 102) |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca yorumlar (`BeginCast`/`TickCast` açıklaması, `CastOutcome::reason` notu) |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | `BeginCast` destek kuralı, `TickCast` tip kapısı girdisi ve reuse damgaları |

`BotSession.*`, `BotManager.cpp` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`CheckCastLand`'den sonra, `// --- cast cancel and standing plan (ADR-0017 Ek F4-24) ---` satırından önce ekle (ASCII, CRLF, tab, `namespace BotCore` içinde; yalnızca `<cstdint>`; **`std::min`/`std::max` kullanma**):

```cpp
	// --- dual-typed cast (ADR-0017 Ek F4-26) ---

	// MAGIC.Type1/Type2 pairs the bot casts (docs/03 MEC-MAG-13): a single type 1 or 3, or the pair Type3 + Type4 (the server
	// runs Type3 first and Type4 second on the same target). Every other pair stays unsupported.
	inline bool CastTypesSupported(uint8_t type0, uint8_t type1)
	{
		if (type1 == 0)
			return type0 == 1 || type0 == 3;

		return type0 == 3 && type1 == 4;
	}

	// A skill type that takes part in the same-type gate (MEC-MAG-03: types 1..7).
	inline bool IsGatedType(uint8_t type)
	{
		return type >= 1 && type <= 7;
	}

	// One entry per type of a skill: whether the bot effected that type earlier in this spawn and how long ago.
	struct TypeStamp
	{
		uint8_t type;       // MAGIC.Type1 or MAGIC.Type2
		bool has;
		uint32_t sinceMs;   // meaningful only when 'has'
	};

	// Smallest "since" over the gated types that were effected before; false (sinceMs = 0) when there is none. The server
	// refuses a cast when ANY gated type of the skill is inside the gate (MEC-MAG-13), so the smallest value decides.
	inline bool MinGatedSince(const TypeStamp * stamps, int count, uint32_t & sinceMs)
	{
		bool any = false;
		uint32_t best = 0;
		for (int i = 0; i < count; i++)
		{
			if (!IsGatedType(stamps[i].type) || !stamps[i].has)
				continue;

			if (!any || stamps[i].sinceMs < best)
			{
				best = stamps[i].sinceMs;
				any = true;
			}
		}

		sinceMs = best;
		return any;
	}
```

`BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

Dosyanın sonuna üç `TEST_CASE` (mevcut `CHECK_EQ((int)...)` kalıbıyla):

- **`Combat_CastTypes_Supported`:** `CastTypesSupported` — **true:** `(1,0)`, `(3,0)`, `(3,4)`; **false:** `(0,0)`, `(2,0)`, `(4,0)`, `(5,0)`, `(9,0)`, `(1,3)`, `(1,4)`, `(1,9)`, `(2,3)`, `(2,4)`, `(3,1)`, `(3,2)`, `(3,3)`, `(3,5)`, `(4,3)`, `(0,4)`.
- **`Combat_TypeGate_MinSince`:** `IsGatedType`: `0` ve `8` false, `1` ve `7` true. `MinGatedSince` (dönüş değeri ve `sinceMs`): `{3,true,500},{4,true,200}` ⇒ true, 200; `{3,true,500},{4,false,0}` ⇒ true, 500; `{3,false,0},{4,false,0}` ⇒ false, 0; `{3,true,500},{0,true,10}` ⇒ true, 500 (tip 0 kapıya girmez); `{0,true,10},{4,true,300}` ⇒ true, 300; `count = 1` ile `{3,true,500}` ⇒ true, 500.
- **`Combat_TypeGate_DualCast`:** `CastStartCheck c = {}` (alanlar: `distanceM = 5`, `skillRange = 56`, `distanceField = 50`, `weaponRangeField = 0`, `needsStanding = false`, `standing = true`, `mana = 1000`, `msp = 30`, `reCastMs = 5300`, `typeGated = true`, `hasSkillLast = false`, `hasAnyLast = false`, `actionsInWindow = 0`). `TypeStamp st[2] = {{3,true,5000},{4,true,400}}`; `c.hasTypeLast = MinGatedSince(st, 2, c.sinceTypeLastMs)` ⇒ `CheckCastStart(c) == CAST_REJECT_TYPE_GATE` ve `CastWaitMs(c) == 600`; `st[1].sinceMs = 1000` ⇒ `CheckCastStart(c) == CAST_OK` ve `CastWaitMs(c) == 0`; yalnızca Type4 damgalı (`st[0].has = false`, `st[1] = {4,true,999}`) ⇒ `CAST_REJECT_TYPE_GATE`, `CastWaitMs == 1`.

### 5.3 `ActionExecutor.cpp`

**a) `BeginCast` (`:729-742`) — destek kuralı.** `bool supportedType = (m->bType[0] == 1 || m->bType[0] == 3);` satırı ve koşuldaki `!supportedType` ile `|| m->bType[1] != 0` satırları şu hale gelir (diğer satırlar aynen):

```cpp
	bool flyingCast = BotCore::IsFlyingCast(m->bType[0], m->bFlyingEffect);
	if (!BotCore::CastTypesSupported(m->bType[0], m->bType[1])
		|| (m->bFlyingEffect != 0 && !flyingCast)
		|| m->iUseItem != 0
		|| m->sEtc != 0
		|| (m->bMoral != MORAL_SELF && m->bMoral != MORAL_FRIEND_WITHME
			&& m->bMoral != MORAL_ENEMY && m->bMoral != MORAL_ALL))
```

`bad_skill` (`:709-725`), `bad_target` (`:742-752`) kuralları ve `unsupported_skill` yanıtı değişmez. Not: `{3, 4}` + `Moral` 10..13 (alan) zaten moral koşuluyla reddedilir.

**b) `TickCast` tip kapısı girdisi (`:842-852`).** `type0`/`hasTypeLast`/`sinceTypeLastMs` bloğunu şununla değiştir (`type0` başka yerde kullanılıyorsa — `grep -n "type0" GameServer/Bot/ActionExecutor.cpp` — yalnızca bu blok ve §5.3-c bloğu olmalı; öyleyse değişkeni kaldır):

```cpp
	// MEC-MAG-13: the same-type gate covers every type of the skill (Type3 and Type4 for a dual-typed skill).
	bool typeGated = (m->iNum < 400000
		&& (BotCore::IsGatedType(m->bType[0]) || BotCore::IsGatedType(m->bType[1])));
	BotCore::TypeStamp typeStamps[2];
	for (int i = 0; i < 2; i++)
	{
		uint8 ty = m->bType[i];
		typeStamps[i].type = ty;
		typeStamps[i].has = (ty < 8 && s->m_castTypeHas[ty]);
		typeStamps[i].sinceMs = typeStamps[i].has
			? (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(now - s->m_castTypeLast[ty]).count()
			: 0;
	}
	uint32 sinceTypeLastMs = 0;
	bool hasTypeLast = BotCore::MinGatedSince(typeStamps, 2, sinceTypeLastMs);
```

`c.typeGated = typeGated; c.hasTypeLast = hasTypeLast; c.sinceTypeLastMs = sinceTypeLastMs;` atamaları (`:871-873`) aynen kalır. Tek tipli skill'de `bType[1] == 0` kapıya girmez: davranış önceki ile aynıdır.

**c) EFFECTING sonrası reuse damgaları (`:984-992`).** `if (type0 < 8) { ... }` bloğu:

```cpp
	for (int i = 0; i < 2; i++)
	{
		uint8 ty = m->bType[i];
		if (ty != 0 && ty < 8)
		{
			s->m_castTypeHas[ty] = true;
			s->m_castTypeLast[ty] = now;
		}
	}
```

(Yorum satırı "the bot is conservative (the server only records a timestamp on success)" ve `m_castSkillLast`/`m_castAnyLast` satırları aynen kalır; damga **yalnızca EFFECTING'te** yazılır, FLYING/CASTING'te değil.)

**d) Sonuç yorumu (kod değişikliği yok, yalnızca davranış sözleşmesi).** Çift tipli skill'in EFFECTING `SubmitCast`'i (kod değişmeden) şu sonuçları verir; uygulayıcı bunların **değiştirilmediğini** doğrular ve §5.3-e yorumlarını yazar:
   1. Type4 yankısı geldi (`op == 3`): `ok = true`, `reason = "effected"`, telemetri `code` = Type4 süresi (sn, 10..19) — `SKILLMAGIC_FAIL_ATTACKZERO` olamaz, bu yüzden `"missed"` **hiç üretilmez** `[D]`.
   2. Yankı yok (hedef Type3 kısmında öldü/geçersizleşti ya da Type4 atlandı): `reason = "no_result"` → mevcut dal seriyi `EndCast` ile düşürür (`FAILED "no_result"`); reuse damgaları ve `m_castLeft` önceki gibi güncellenir. Bu, çift tipli skill'de **meşru** bir sonuç olabilir; `tools/bot-telemetry-report.py` MET-ACT-02'de `no_result` zaten ayrı sayılır (değişmez).
   3. `MAGIC_FAIL` (Type4 `bResult == 0` ya da sunucu reddi): `reason = "srv_fail"`.

**e) `ActionExecutor.h` yorumları.** `BeginCast` yorumuna: "single Type3 + Type4 (dual-typed) skills are supported (ADR-0017 Ek F4-26)"; `TickCast`/`CastOutcome::reason` yorumuna: "dual-typed: the EFFECTING echo comes from the Type4 part (code = duration), 'missed' is never reported, no echo = 'no_result'". Başka satır değişmez.

**Yasaklar (AC-LRN-03):** yeni kod sunucu nesnesinden başarı çıkarmaz; yeni sunucu okuması yoktur (yalnızca `m` = `_MAGIC_TABLE` alanları, F4-03'ten beri okunuyor). Başka botun `CUser`'ına yeni erişim yok.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.*`, `CombatTests.cpp` için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı üç yeni test adını (`Combat_CastTypes_Supported`, `Combat_TypeGate_MinSince`, `Combat_TypeGate_DualCast`) içerir ve toplam test sayısı **102**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-26 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş.
- [ ] K5: destek kuralı yalnızca `{3, 4}` çiftini açıyor: `grep -n "CastTypesSupported" GameServer/Bot/ActionExecutor.cpp` `BeginCast`'te tek çağrı gösterir; `bFlyingEffect != 0 && !flyingCast`, `iUseItem != 0`, `sEtc != 0` ve moral koşulları `git diff`'te silinmemiş; `bad_skill`/`bad_target` kuralları değişmemiş; `CastTypesSupported` birim testi `(1,4)`, `(3,3)`, `(3,5)`, `(4,0)` için false (K3).
- [ ] K6: tip kapısı iki tipe bakıyor: `grep -n "MinGatedSince\|IsGatedType" GameServer/Bot/ActionExecutor.cpp` `TickCast`'te kapı girdisini gösterir; `grep -n "type0" GameServer/Bot/ActionExecutor.cpp` boş (değişken kaldırıldı) ya da yalnızca başka bir bağlamda; reuse damgası `for` döngüsü `bType[0]` ve `bType[1]` için `m_castTypeHas`/`m_castTypeLast` yazıyor (kod okumasıyla); `m_castTypeLast[3] = now` pot satırı (`:1445`) değişmemiş.
- [ ] K7: tek tipli yol gerilemesiz: `git diff gece/2026-10-02...bot/F4-26 -- GameServer/Bot/ActionExecutor.cpp | grep '^-' | grep -v '^---'` yalnızca bu planın açıkladığı satırları gösterir (`supportedType` satırı ve koşulu, `|| m->bType[1] != 0`, `type0` kapı bloğu, `type0 < 8` damga bloğu); `PlanStanding`, ARMED bloğunun `CheckCastStart`/CASTING gönderimi, uçan akış (`CheckCastFly`/`CheckCastLand`), `RejectCast`, `SubmitCast`, `CancelCast` satırları silinmemiş.
- [ ] K8: `SubmitCast` ve `OnPacket()` değişmedi: `git diff gece/2026-10-02...bot/F4-26 --stat` içinde `BotSession.cpp`, `BotSession.h`, `BotManager.cpp` yok; `git diff ... -- GameServer/Bot/ActionExecutor.cpp` içinde `SubmitCast` gövdesi (`static CastOutcome SubmitCast` ile `EndCast` arası) değişmemiş.
- [ ] K9: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı/alanı yok; `git diff gece/2026-10-02...bot/F4-26 -- GameServer/Bot/ActionExecutor.cpp | grep '^+' | grep 'Emit('` boş.
- [ ] K10: `git diff --stat gece/2026-10-02...bot/F4-26` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K11: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp`/`ActionExecutor.*` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K12: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K13: gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 99 testin tamamı hâlâ geçiyor.
- [ ] K14: `python3 tools/check-perception-contract.py` `RESULT: PASS` ve sayılar değişmez (`R1 0/0`, `R2 0/28`, `R3 0/18`, `R4 0/0`, `R5 0/0`); yeni kod başka botun `CUser`'ına (`t->m_pUser`) erişmez.
- [ ] K15 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S5 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-26
git diff gece/2026-10-02...bot/F4-26 -- GameServer/Bot/ActionExecutor.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-26 -- GameServer/Bot/BotManager.cpp GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "CastTypesSupported\|MinGatedSince\|IsGatedType\|type0" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.cpp GameServer/Bot/ActionExecutor.h
git diff --check gece/2026-10-02...bot/F4-26
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`); zamanlama için geçici betik `./Scripts/f426_*.txt` (silinir, commit edilmez). Botlar: Karus mage **`BotMI_K`** (buz ağacı 70: Prismatic dahil hepsi) ve `BotMF_K` (buz ağacı 52: Prismatic hariç), hedef El Morad warrior `BotWP_E`; zone 71, botlar aynı başlangıç noktasındadır (menzil içi). MP/HP `list` çıktısından, hedefin yavaşlatma buff'ı `snap BotWP_E` günlük satırından (`cmd snap:   buff skill=<id> type=6 debuff remain=<s>s`), olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'dan. Yavaşlatma 10..19 sn sürer: senaryolar arasında hedefin buff'ının bittiğini `snap` ile gör ya da hedefi yeniden spawn et.

1. **S1 Uçan çift tipli (Ice arrow):** `spawn BotMF_K,BotWP_E`; MP/HP `list` ile not al; `cast BotMF_K 110615 BotWP_E 1`. Beklenen JSONL sırası: `CastStart` (`casting`) → `CastFly` (`since_casting_ms` ≥ 1580; `ok:true`, `reason:"flying"`, `op:2`) → `CastEffect` (`since_flying_ms` ≥ 1000; `ok:true`, **`reason:"effected"`, `op:3`, `code` = 12 (Type4 süresi, `MAGIC_TYPE4` `110615` Duration)**, `missed` değil); log `cast finished (effected) after 1 cycle(s), 1 ok, 3 packet(s) sent`; MP düşüşü ≈ **100 (2 × 50)** (uçan çift tipli de iki kez düşer; yalnızca bir kez ≈ 50 çıkarsa MEC-MAG-13 MP maddesi yanlıştır, bulgu olarak raporla); `snap BotWP_E` hedefte `buff skill=110615 type=6 debuff remain≈12s` ve HP düşüşü (hasar `−216` civarı, direnç/zırhla farklı olabilir). Aynı sırayı `110627` (Ice orb, `Msp 80`, `Duration 14`) verir.
2. **S2 Uçmayan çift tipli (Chill, Ice comet, Prismatic):** `cast BotMF_K 110609 BotWP_E 1` (Chill, `Msp 30`, `Duration 11`): `CastStart` → `CastEffect` (2 paket), `CastFly` **yok**, MP düşüşü ≈ 30, hedefte `buff skill=110609 type=6`. `cast BotMF_K 110651 BotWP_E 1` (Ice comet, `Msp 160`, ağaç 51 ≤ 52): aynı sıra, MP ≈ 160. **Prismatic** `BotMI_K` ile: `spawn BotMI_K,BotWP_E`; `cast BotMI_K 110670 BotWP_E 1`: `CastStart` (`cast_ms` 1180) → `CastEffect` (`since_casting_ms` ≥ 1180, `ok:true`, `reason:"effected"`, `code` = 10), log `... 2 packet(s) sent`, MP düşüşü ≈ 390 (tek düşüm), hedef HP düşüşü büyük (Prismatic `−1750` civarı), `snap BotWP_E` `buff skill=110670 type=6 debuff`. Prismatic `ReCastTime 213`: ardışık ikinci cast ≥ 21 300 ms sonra (3 döngü denemesi gereksiz; `cast ... 2` ile ikinci `CastStart` aralığı ölçülür).
3. **S3 Çok döngü ve tip kapısı:** `cast BotMF_K 110603 BotWP_E 3` (Freeze, `ReCast 1`): `CastStart`/`CastEffect` ×3, her EFFECTING'ten sonraki `CastStart` ≥ 1000 ms (tip kapısı `kTypeGateMs`; bekleme, `FAIRNESS_REJECT` yok), toplam MP ≈ 60; log `cast finished (effected) after 3 cycle(s), 3 ok, 6 packet(s) sent`. Çift tipli bir cast'in hemen ardından tek tipli Fire ball (`110515`) başlatılırsa ikinci seri en az 1000 ms bekler (tip 3 kapısı; Type4 damgası yalnızca birim testle sınanabilir, Type4-only skill dilim 4'te gelir: raporda "sınanmadı (birim testle kapsandı)").
4. **S4 Reddedilen/desteklenmeyen skill'ler:** `cast BotMI_K 110633 BotWP_E 1` (Ice burst, Moral 10) → `unsupported_skill`; `110645` (Blizzard), `110671` (ice storm) aynı; `110635` (Ice blast, UseItem) → `unsupported_skill`; `110657` (Ice Impact) aynı; `cast BotWP_K 106520 BotWP_E 1` (Leg cutting, `Type1 = 1`/`Type2 = 4`) → `unsupported_skill`; `cast BotMI_K 110609 self` → `bad_target`; `cast BotWP_K 110609 BotWP_E 1` → `bad_skill` (sınıf). Menzil dışı hedef (`move` ile ≥ 80 m uzaklaş; Chill `Range 56`) → CASTING gitmeden `FAIRNESS_REJECT` (`MEC-MAG-11`, `out_of_range`). **Bilinen sınır (hata değil, KI-016):** `cast BotMF_K 110670 BotWP_E 1` (MF'nin buz ağacı 52 < 70) → `srv_fail` ile düşer (CASTING `MAGIC_FAIL`); raporda KI-016'nın yeniden üretimi olarak yaz.
5. **S5 İptal ve gerilemesiz:** (a) `cast BotMF_K 110609 BotWP_E 1` ~500 ms sonra `cast BotMF_K off` (CASTING): F4-24 davranışı `cancelled` (`op:4`, `code:-100`), MP değişmez, hedefte buff yok. (b) `cast BotMF_K 110615 BotWP_E 1` ~2,0 sn sonra `cast BotMF_K off` (FLYING aşaması): `dropped`, paket yok, MP ≈ 50 (F4-25 davranışı). (c) `cast BotMF_K 110518 BotWP_E 3` (Ignition, tek tipli, uçmayan) önceki gibi `effected` ×3, `CastFly` yok, 6 paket; `cast BotMF_K 110515 BotWP_E 1` (Fire ball, uçan tek tipli) F4-25 gibi 3 paket, MP ≈ 100; 30 m `move`/`stop` (F4-01); `attack BotWP_K BotWP_E 3` (F4-02); `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama çift tipli cast çalışır (log satırları); `ENABLED=0` → komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde (≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Bölgedeki ikinci bir bot `/bot snap <bot> events` ile çift tipli EFFECTING'i (`event ... op=3 skill=110615 caster=<mage> target=<hedef>`, F4-52 halkası) görür. Temizlik: ini yedekten geri, geçici betik silindi, hedefin buff'ı bitti.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **çift tipli** skill serisinde davranış değiştirir; tek tipli skill'ler ve cast etmeyen botlar için davranış değişmez (§6 K7).
- **Thread kuralı (ADR-0005):** `TickCast` yalnızca IOCP thread'inde. `OnPacket()` değişmez (çift tipli EFFECTING yankısı F4-03'ten beri `m_castEcho`'ya yazılıyor). `ActionExecutor` log yazmaz.
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `effected` yalnızca sunucunun bölgeye yayınladığı, çağırana da gelen `MAGIC_EFFECTING` (Type4 kaynaklı) paketinden çıkarılır; sunucu nesnesinden başarı çıkarılmaz.
- **Bilinen sınırlar `[A]`/`[D]`:** (a) Çift tipli yankı Type4'ten gelir ve hasar bilgisi taşımaz; `missed` ayırt edilemez `[D]`. (b) Hedef Type3 kısmında ölürse yankı gelmez: seri `no_result` ile düşer (öldürme "başarısız" görünür); karar katmanı (F6) ölümü algıdan (`WIZ_DEAD`, F4-52) okur. (c) Hedefin debuff engeli (`m_bBlockCurses`/`m_bReflectCurses`) varsa sunucu `bResult = 0` yankısını ve ardından `MAGIC_FAIL` gönderir; bot son yankıyı (`srv_fail`) görür, nadir. (d) `UseStanding` 51..54 (Prismatic 53) için "ayakta şart" uygulanmaz ve sunucu da menzil denetlemez; bot kendi `CastInRange` denetimini her zaman uygular (daha sıkı). Gerçek istemcinin bu skill'leri hareket halinde atıp atamadığı bilinmiyor `[A]` (KI-017). (e) Skill ağacı puanı denetlenmez (KI-016): Prismatic için MF botları `srv_fail` alır. (f) Yavaşlatmanın hedef botun hareket guard'ına etkisi yok (guard azami hızı sınırlar; yavaş hareket kuralı ihlal etmez).
- Telemetri hacmi değişmez (döngü başına olay sayısı F4-03/F4-25 ile aynı). `tools/bot-telemetry-report.py` değişmez.
- `list`/`snap` ve `Telemetry.*` değişmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-26` (taban: `gece/2026-10-02` @ `f8021c0`) — `c066830 [F4-26] Cift tipli skill dilimi: {3,4} destegi, iki tipli tip kapisi`
- Değişen dosyalar ve nedenleri:
  - `BotCore/BotCombat.h`: §5.1 "dual-typed cast" bölümü eklendi (`CastTypesSupported`, `IsGatedType`, `TypeStamp`, `MinGatedSince`), `CheckCastLand` sonrası ve `// --- cast cancel and standing plan` başlığından önce. Yalnızca `<algorithm>`/`<cstdint>` ve `std::min`/`std::max` yok.
  - `Tests/BotCoreTests/CombatTests.cpp`: §5.2 üç yeni `TEST_CASE` (`Combat_CastTypes_Supported`, `Combat_TypeGate_MinSince`, `Combat_TypeGate_DualCast`), dosya sonuna; 99 → 102.
  - `GameServer/Bot/ActionExecutor.cpp`: §5.3-a `BeginCast` destek kuralı `CastTypesSupported(bType[0], bType[1])`, `supportedType`/`bType[1] != 0` koşulları kaldırıldı; §5.3-b `TickCast` tip kapısı iki tipe bakıyor (`TypeStamp[2]` + `MinGatedSince`), `type0` değişkeni kaldırıldı; §5.3-c EFFECTING sonrası reuse damgaları `bType[0]` ve `bType[1]` için döngüyle yazılıyor.
  - `GameServer/Bot/ActionExecutor.h`: §5.3-e yalnızca yorumlar (tek `CastTypesSupported` çağrısı, `BeginCast`/`TickCast` destek/`reason` notu).
- Derleme sonucu (`tools/build.sh Release` son satırları):
  ```
    BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
    proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  `touch` ile yeniden derlemede `BotCombat|CombatTests|ActionExecutor` için uyarı/hata grep'i boş (rc=1). `Debug` da rc=0.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (Release rc=0, değişen dosyalarda uyarı yok)
  - K2 ✔ (Debug rc=0)
  - K3 ✔ (`Release`/`Debug`: `102 tests, 0 failed`, üç yeni test adı çıktıda)
  - K4 ✔ (grep boş; `#include` yalnızca `<algorithm>`/`<cstdint>`; eklenen satırlarda `std::min`/`std::max` yok)
  - K5 ✔ (`CastTypesSupported` yalnızca `ActionExecutor.cpp:730`; `bFlyingEffect`/`iUseItem`/`sEtc`/moral koşulları diff'te silinmemiş; birim testi `(1,4)`,`(3,3)`,`(3,5)`,`(4,0)` false)
  - K6 ✔ (`IsGatedType`/`MinGatedSince` `TickCast`'te; `grep type0` boş; damga döngüsü iki tip; `m_castTypeLast[3] = now` pot satırı değişmedi — satır `1452`)
  - K7 ✔ (silinen satırlar yalnızca planın saydığı `supportedType`/koşul/`bType[1]`/`type0` blokları; diğer akışlar korunmuş)
  - K8 ✔ (`BotSession.*`, `BotManager.cpp` diff'te yok; `SubmitCast`/`OnPacket()` değişmedi)
  - K9 ✔ (yeni `Emit(` yok; yeni ini/komut/thread/telemetri alanı yok)
  - K10 ✔ (`git diff --stat` yalnızca 4 dosya; vcxproj değişmedi)
  - K11 ✔ (`file` hepsi ASCII + CRLF; `git diff --check` boş)
  - K12 ✔ (yasaklı çağrı grep'i boş)
  - K13 ✔ (tüm `Check*` grep'leri ≥ beklenen; 102/102 test geçiyor)
  - K14 ✔ (`check-perception-contract.py` `RESULT: PASS`, `R1 0/0 R2 0/28 R3 0/18 R4 0/0 R5 0/0`)
  - K15 ⏳ (Claude `/plan-dogrula` çalışma zamanı S1–S5; bu planda DeepSeek yapmaz)
- Plandan sapmalar ve gerekçeleri: Yok. Satır numaraları kaydı (pot damgası `1445` → `1452`); plan "satırlar kayabilir" diyor.
- Açık sorular: Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-26` @ `7eed96b` (kod commit'i `c066830`; `7eed96b` yalnızca rapor ve Durum). Mod: otonom gece döngüsü (`AUTO_LOOP=1`); birleştirme ve push yapılmadı.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | 4 dosya `touch` edilip `build.sh Release` yeniden çalıştırıldı: rc=0; çıktıda yalnızca 2 uyarı, ikisi de `UpgradeHandler.cpp(862)` C4789 (bu plandan önce var); `BotCombat\|CombatTests\|ActionExecutor` için uyarı/hata 0 |
| K2 | ✔ | `build.sh Debug` rc=0, değişen dosyalarda uyarı/hata 0 |
| K3 | ✔ | `run-tests.sh Release` ve `Debug`: `102 tests, 0 failed`; `[ OK ] Combat_CastTypes_Supported`, `Combat_TypeGate_MinSince`, `Combat_TypeGate_DualCast` iki yapılandırmada da var; `CastTypesSupported` testi `(1,4)`, `(3,3)`, `(3,5)`, `(4,0)` false (`CombatTests.cpp` yeni test) |
| K4 | ✔ | `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` boş; `#include` yalnızca `<algorithm>` (`:6`), `<cstdint>` (`:7`); eklenen satırlarda `std::min`/`std::max` yok |
| K5 | ✔ | `grep -n CastTypesSupported ActionExecutor.cpp` tek çağrı `:730`; `bFlyingEffect != 0 && !flyingCast`, `iUseItem`, `sEtc`, moral koşulları diff'te silinmemiş; `bad_skill`/`bad_target` blokları diff'te yok; çalışma zamanı S4: Ice burst/Blizzard/ice storm/Ice blast/Ice Impact/Leg cutting `unsupported_skill` |
| K6 | ✔ | `ActionExecutor.cpp:842` (`IsGatedType` ×2) ve `:854` (`MinGatedSince`) `TickCast` kapı girdisinde; `grep type0` boş; damga döngüsü `bType[0]` ve `bType[1]` için `m_castTypeHas/Last` yazıyor (`:990-998`, yalnızca EFFECTING sonrası); pot satırı `m_castTypeLast[3] = now` `:1452` (içeriği değişmedi) |
| K7 | ✔ | Silinen satırlar yalnızca `supportedType` satırı ve koşulu, `\|\| m->bType[1] != 0`, `type0` kapı bloğu ve `type0 < 8` damga bloğu; diff üç hunk'tan (`:727`, `:839`, `:984`) oluşuyor, `PlanStanding`/ARMED/uçan akış/`RejectCast`/`SubmitCast`/`CancelCast` dokunulmamış |
| K8 | ✔ | `git diff --stat`'ta `BotSession.*`, `BotManager.cpp` yok; `SubmitCast` ve `OnPacket()` hunk'larda yok |
| K9 | ✔ | Eklenen satırlarda `Emit(` yok; yeni ini anahtarı, komut, thread, telemetri olayı/alanı yok |
| K10 | ✔ | `git diff --stat`: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.h`, `ActionExecutor.cpp` + plan dosyası; vcxproj/filters değişmemiş; `GameServer/` altında yalnızca `Bot/` |
| K11 | ✔ | `file`: dört dosya `ASCII text, with CRLF line terminators`; çalışma ağacında CR'siz satır 0 (`grep -vc $'\r$'`); `git diff --check` boş |
| K12 | ✔ | `printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(` `ActionExecutor.*` içinde boş |
| K13 | ✔ | `CheckMoveStep` 2, `CheckAttack` 1, `CheckCastStart` 1, `CheckCastEffect` 1, `CheckCastFly` 1, `CheckCastLand` 1, `CheckCastCancel` 1, `CheckPotion` 1; önceki 99 test geçiyor (102/102) |
| K14 | ✔ | `check-perception-contract.py` `RESULT: PASS`; R1 0/0, R2 0/28, R3 0/18, R4 0/0, R5 0/0; yeni kod başka botun `m_pUser`'ına erişmiyor |
| K15 | ✔ | Çalışma zamanı S1-S5 geçti (aşağıda); `summary`/`ENABLED=0` alt denetimleri yeniden koşulmadı (bkz. not 3) |

- Çalışma zamanı (Release `7eed96b` derlemesi, `[BOT] ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, ini dokunulmadı: md5 `265a8e1c35ea12df46f6d006fe894d9b` öncesi/sonrası; botlar `BotMF_K`, `BotMI_K`, `BotWP_E`, `BotWG_E`, `BotWP_K`, `BotMF_E`, zone 71; gözlem `Logs/Bot_2_10_2026.log` ve `Logs/bots/2026-10-02/live-234303.jsonl`):
  - **S1 ✔ (uçan çift tipli):** Ice arrow `110615` (`BotMF_K` → `BotWP_E`): `CastStart` (`casting`) → `CastFly` (`since_casting_ms` 1660, `flying`, `op:2`) → `CastEffect` (`since_flying_ms` 1000, `ok:true`, **`effected`, `op:3`, `code:12`**, `missed` yok); log `cast finished (effected) after 1 cycle(s), 1 ok, 3 packet(s) sent`; **MP 6021 → 5971 (FLYING) → 5921 (EFFECTING) = 2 × 50**; hedef HP −184..−176. Ice orb `110627` aynı sıra, 3 paket, `snap` hedefte `buff skill=110627 type=6 debuff remain=9s`; MP düşüşü FLYING'de 6021 → 5941 (−80), toplam 6021 → 5901 (−120): ikinci −80, aradaki **+40'lık MP yenilenme adımıyla** (Ice comet sonrası örneklemede 5861 → 5901 adımı görüldü) birlikte −120 verir; 2 × 80 ile uyumlu.
  - **S2 ✔ (uçmayan):** Chill `110609` (`BotMF_K` → `BotWG_E`): `CastStart` → `CastEffect` (`since_casting_ms` 1646, `effected`, `code:11`), `CastFly` yok, log `... 2 packet(s) sent`, `snap` `buff skill=110609 type=6 debuff remain=7s`. Ice comet `110651`: `effected`, 2 paket, **MP 6021 → 5861 = tek düşüm 160**, hedef HP 771 → 414. **Prismatic `110670` `BotMI_K` → `BotWG_E`:** `CastStart` → `CastEffect` (`since_casting_ms` 1214, `effected`, `code:10`), 2 paket, **MP 6021 → 5631 = 390 (tek düşüm)**, `snap` `buff skill=110670 type=6 debuff remain=6s`; hedef HP düşüşü ≈ 527 (planın "−1750 civarı" beklentisi `FirstDamage` değeriydi, gerçek düşüş hedefin direnciyle azalır; Ice comet `FirstDamage −882` → ≈ −357 ile aynı oran; bot kodunu ilgilendirmez).
  - **S3 ✔:** Freeze `110603` ×3 (`BotMF_K` → `BotWG_E`, 1,3 m'e yaklaştıktan sonra): `CastStart`/`CastEffect` ×3, `code:10`, EFFECTING → sonraki `CastStart` aralıkları **1105 ms ve 1101 ms** (tip kapısı ≥ 1000), `FAIRNESS_REJECT` yok, log `finished (effected) after 3 cycle(s), 3 ok, 6 packet(s) sent`. Freeze ardından hemen Fire ball: `CastEffect` (204552417) → Fire ball `CastStart` (204553528) = 1111 ms (tip 3 kapısı beklemesi). Freeze'in ilk denemesi 13,4 m'de (Freeze `Range 11`) `FAIRNESS_REJECT out_of_range` verdi (MEC-MAG-11 çalışıyor). Toplam MP ≈ 60 ölçülemedi (yenilenme adımı 60'ı siler); MP maddesi Ice comet/Prismatic/Ice arrow ile kanıtlandı.
  - **S4 ✔:** `110633`, `110645`, `110671`, `110635`, `110657` (`BotMI_K`) ve `106520` (`BotWP_K`) → `refused (unsupported_skill)`; `cast BotMI_K 110609 self` → `bad_target`; `cast BotWP_K 110609 BotWP_E 1` → `bad_skill`; `cast BotMF_K 110670 BotWP_E 1` → `CastStart` `srv_fail` (`op:4`, `code:-100`), log `cast stopped (srv_fail)`, MP değişmedi (KI-016 yeniden üretildi).
  - **S5 ✔:** (a) Chill CASTING'te `cast off` (+1099 ms): `CastCancel` `cancelled` (`op:4`, `code:-100`), MP 6021 değişmedi, hedefte buff 0. (b) Ice arrow FLYING'de `cast off`: log `stopped after 2 packet(s) sent`, `CastFly` var, o döngüde `CastEffect` yok, MP 6021 → 6011 (−50 + 40 yenilenme), hedefte buff 0. (c) Ignition `210518` ×3 (`BotMF_E` → `BotWP_K`): `effected` ×3, `CastFly` yok, `6 packet(s) sent`; Fire ball `210515`: `CastFly` var, 3 paket, `effected`. F4-52 halkası: `snap BotWP_K events` Chill için `op=1` ve `op=3 skill=210609 caster=<MF_E>` gösterdi. `PERF_SAMPLE`: tipik `tick_p95_us` 82-160; 1,3-2,1 ms sıçramaları 6 pencerede, hepsi `BotWG_E`'nin 640 m yürüyüşü (bölge geçişleri, `NpcInReq`) sırasında, cast pencerelerinde ≤ 522 µs. `Bot_*.log`'da `error/assert/exception` 0; `GameServer.log` değişmedi (mtime 02:17). Sunucular `[UP]` 3/3, sonunda `run-servers.sh stop` ile 0/3.
  - Temizlik: ini md5 aynı, `BotCommands.*` ve geçici betik yok (`Scripts/` boş), sunucular kapalı. Hedef botlar zone 71'deki Death knight NPC'leri tarafından öldürüldü (`BotWG_E`, `BotWP_E` HP 0): bot karakter verisi, plan dışı.
- Bulgular (önem sırasıyla): engelleyici bulgu yok.
  1. **Not:** Hedef Type3 kısmında ölürse çift tipli skill `no_result` verir (planın §5.4-d.2); bu doğrulamada Freeze ve Fire ball, `BotWG_E`'nin Death knight tarafından öldürüldüğü bir anda `no_result` ile düştü (`CastEffect` `ok:false`, `reason:"no_result"`, `op:-1`). Fire ball tek tipli olduğundan bu örnek çift tipliye özgü değil, yalnızca ölü hedefte seri düşürme yolunu doğruluyor; "Type3 öldürürse Type4 yankısı yok" iddiası ayrıca (canlı hedefi tek vuruşta öldüren bir cast ile) ölçülmedi, `docs/03` MEC-MAG-13'te `[D]` kaldı.
  2. **Not:** Plan §7 S1/S2 beklentisindeki hedef HP değerleri (Ice arrow −216, Prismatic −1750) hedefin direncine göre farklı çıktı (−176..−184, ≈ −527); bu hasar mekaniğidir, bot kabul ölçütü değil.
  3. **Not:** `TELEMETRY=summary` ve `ENABLED=0` alt denetimleri bu turda yeniden koşulmadı: değişiklik ini/telemetri/komut yoluna dokunmuyor (K9 ✔, farkta yalnızca `ActionExecutor.*`, `BotCombat.h`, test), F4-25 doğrulamasında bu yollar geçmişti.
  4. **Not:** Plan 99 → 102 ve uyarı/boyut beklentileriyle uyumlu; `ActionExecutor.cpp` `typeGated` koşulu `bType[1] == 0` tek tipli skill'de önceki davranışla aynı (`IsGatedType(0)` false).
- Düzeltme talimatı: gerekmiyor (karar DOĞRULANDI).
