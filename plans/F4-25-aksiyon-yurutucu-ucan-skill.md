# F4-25: `ActionExecutor` uçan skill dilimi — tek hedefli, tek tipli Type3 uçan büyüler (CASTING → FLYING → EFFECTING)

| Alan | Değer |
|---|---|
| Durum | UYGULANIYOR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-25` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-03 (cast dilimi: `BeginCast`/`TickCast`/`SubmitCast`, `m_castEcho`) — `KAPANDI`; F4-24 (cast iptali, `CancelCast`, `CAST_CASTING` hareketle iptal) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-03 (uçan skill: CASTING → FLYING → EFFECTING), CLI-04, CLI-11, MEC-MAG-08, MEC-MAG-11, MEC-MAG-12 (bu planla eklendi, `[D]`), AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 dilim 2 (bölünmüş: 2a = bu plan) |
| Tahmini büyüklük | M (5 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

F4-03 uçan skill'leri bilerek dışarıda bıraktı (`ActionExecutor.cpp:721-733`: `m->bFlyingEffect != 0` ⇒ `unsupported_skill`). Mage'in ana tek hedefli büyüleri (Fire ball `110515`, Fire spear `110527`, Static orb `110751`; El Morad karşılıkları `2105xx`/`2107xx`) uçan Type3 skill'dir ve bot bugün bunları atamıyor. Bu plan, **tek hedefli, tek tipli (`bType[1] == 0`), eşyasız, görevsiz (`Etc == 0`) Type3 uçan skill'ler** için gerçek istemcinin ölçülen paket düzenini ekler:

1. **CASTING** (`CastTime > 0` ise; F4-03 ile aynı) → bot `CastTime × 100 + 80` ms bekler →
2. **FLYING** (`WIZ_MAGIC_PROCESS` opcode 2, `CUser::HandlePacket()` ile) → sunucu bölgeye yayınlar, **MP'nin ilk yarısını düşer** (MEC-MAG-12) →
3. bot en az `kFlightMinMs = 1000` ms bekler (mermi uçuşu; `docs/03` CLI-03: FLYING → EFFECTING ≈ 1037 ms, tek örnek) →
4. **EFFECTING** → hasar ve MP'nin ikinci yarısı.

Sunucu FLYING ile CASTING/EFFECTING arasında sıra tutmaz (MEC-MAG-01); değer **protokol sadakati** (gerçek istemci uçan skill'de FLYING gönderir; FLYING'in bölge yayını diğer oyunculara/botlara mermi görünümünü ve F4-52 olay halkasındaki `MAGIC_FLYING` olayını üretir) ve **doğru MP muhasebesidir** (aşağıda: sunucu uçan Type3'te MP'yi iki kez düşer, guard bunu bilmeli).

Kapsam dışı kalan uçan skill türleri (alan, çift tipli, okçu Type2, eşya tüketen, görevli) sonraki dilimlerdedir (§3). F4'ün yirmi beşinci planıdır (ADR-0018 sırası: 1. cast iptali ✔ → **2. uçan skill'ler (2a: bu plan)** → çift tipli → Type4 → alan → ...).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-25)" (bu planla birlikte yazıldı) ve "Ek (F4-03)" madde 2 (desteklenen skill sınırı; bu plan uçan Type3 için gevşetir) ve "Ek (F4-24)". `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (+ Ek 1 madde 4: uçan ve alan tutarlı kurulur).
- `docs/03` §4.1 (opcode'lar, paket düzeni), §4.2 **MEC-MAG-01/-02/-03/-08/-11/-12**, §13.2 ve CLI-03 uçan alan büyüsü satırı (`110533`: CASTING → FLYING +1539 ms → EFFECTING +2576 ms, n=3, tek örnek `[V]`), CLI-04, CLI-11.
- `plans/F4-03-aksiyon-yurutucu-cast.md`, `plans/F4-24-aksiyon-yurutucu-cast-iptali.md` ve birleşmiş kod: **yazılı planı değil, kodu esas al.**
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `cf22667` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.cpp:45-94` — `Run()` `case MAGIC_FLYING`: oyuncu için `m_Magictype2Array.GetData(nSkillID)` (Type3 skill'de `nullptr`); `pType == nullptr` ise `:83-87` `sMsp > GetMana()` → `SendSkillFailed`; `:90` **`pCaster->MSpChange(-(pSkill->sMsp))`**; `:93` `SendSkill(true)` (bölge yayını, çağıran dahil: CASTING yankısıyla aynı yol).
  - `GameServer/MagicInstance.cpp:284-286` — `UserCanCast()`: `IsAvailable()` yalnızca `MAGIC_EFFECTING`/`MAGIC_CASTING` için çağrılır (FLYING için **çağrılmaz**); `:194` `GetMana() - sMsp < 0` her opcode'da (FAIL/CANCEL hariç) `SkillUseFail`.
  - `GameServer/MagicInstance.cpp:999-1029` — `IsAvailable()` içi, `bOpcode == MAGIC_EFFECTING`: `:1003` yalnızca `bType[0] == 2 && bFlyingEffect != 0` için MP düşmeden `return true`; Type3 için `:1011` yetersiz MP ⇒ `fail_return`, `:1028-1029` **`MSpChange(-sMsp)`** ⇒ uçan Type3 skill toplamda **2 × `sMsp`** harcar `[D]` (FLYING + EFFECTING). Çalışma zamanı doğrulaması bu planın Claude adımıdır (§7 S1).
  - `GameServer/MagicInstance.cpp:296-311` — `CheckSkillPrerequisites()`: FLYING ve EFFECTING tam denetimden geçer; CASTING yalnızca `UseStanding == 1` menzil denetimi. `:350-358` menzil (`sUseStanding == 0` ve mesafe `>= sRange` ⇒ fail), `:361-371` skill recast (`m_CoolDownList`, yalnızca EFFECTING başarısında yazılır `:117-126`), `:388-423` tip kapısı.
  - `GameServer/MagicInstance.cpp:740-747`, `:763-777` — yanıt paketi `u8 opcode, u32 skill, i16 caster, i16 target, i16 sData[0..6]`; FLYING `bSendToRegion = true`.
  - `shared/packets.h:382-384` — `MAGIC_CASTING = 1`, `MAGIC_FLYING = 2`, `MAGIC_EFFECTING = 3` (ISO-8859 dosya: `grep -a`). `shared/database/structs.h:9` — `uint16 bFlyingEffect`.
  - `GameServer/Bot/ActionExecutor.cpp` (satırlar `cf22667`): `RejectCast` `:534-581`, `SubmitCast` `:585-665` (echo çözümü `:621-645`), `BeginCast` `:667-757` (destek kuralı `:721-733`), `TickCast` `:759-961` (ARMED bloğu `:871-903`, CASTING bekleme `:905-915`, EFFECTING `:917-960`), `EndCast` `:963-970`, `CancelCast` `:978-1070` (ARMED dalı `:988-1000`).
  - `GameServer/Bot/BotSession.h:34` — `enum CastPhase { CAST_IDLE = 0, CAST_ARMED = 1, CAST_CASTING = 2 }`; `:94-98` cast alanları. `GameServer/Bot/BotSession.cpp:52-83` — `OnPacket()` `WIZ_MAGIC_PROCESS` bloğu: `op` 1..4 ve `caster == m_castSelfId` ise `m_castEcho = valid | op<<48 | uint16(sData3)<<32 | skill`; **FLYING (op 2) yankısı zaten buradan geçer, blok değişmez.**
  - `GameServer/Bot/BotManager.cpp:3043` — hareketle iptal yalnızca `CAST_CASTING`'te; `:3172`, `:3217` `!= CAST_IDLE` denetimleri yeni fazı da kapsar. **BotManager.cpp değişmez.**
  - `BotCore/BotCombat.h:123-256` — cast dilimi (`CastDurationMs :130`, `CastStartCheck :151-170` — `msp` alanı `uint16_t`, `CheckCastStart :217`, `CheckCastEffect :244`); `Tests/BotCoreTests/CombatTests.cpp` 35 test (toplam **96**, Claude `./tools/run-tests.sh Release` ile doğruladı).
- **Veri notu (Claude yerel `MAGIC` tablosunda doğruladı; `MAGIC` oyun verisidir):** `Type1 = 3`, `Type2 = 0`, `FlyingEffect != 0`, `UseItem = 0`, `Moral ∈ {1,2,7,8}` olan master sınıf skill'leri yalnızca şunlardır: `110515` Fire ball (`Msp 50`, `CastTime 15`, `ReCastTime 43`, `Range 78`, `Etc 0`, seviye 15), `110527` Fire spear (`Msp 80`, aynı süreler, seviye 27), `110574` Vampiric Fire (`Etc 517` ⇒ **görevli, bu plan dışında**), `110751` Static orb (`Msp 160`, `ReCastTime 53`, `Range 56`, seviye 51) ve El Morad karşılıkları `210515`/`210527`/`210574`/`210751`. `Moral` hepsinde 7 (düşman). Mage botları (seviye 80, sınıf 110/210) `110515`/`110527`/`110751` ve `2105xx`/`2107xx` karşılıklarını `sSkill/10 == m_sClass` kuralıyla geçer. `110533` Fire burst (Moral 10, alan), `110535` Fire blast (`UseItem`), `110615` Ice arrow (çift tipli, `Type2 = 4`) ve okçu Type2 skill'leri bu planda `unsupported_skill` kalır.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** `kFlightMinMs`, `IsFlyingCast`, `CastManaNeed`, `CheckCastFly`, `CheckCastLand`; `CastStartCheck::msp` alanı `uint32_t`'e genişler. Birim testleri (`CombatTests.cpp`, +3).
2. **`BotSession::CAST_FLYING = 3`** fazı ve `m_castFlyingAt` zaman damgası.
3. **`BeginCast` kuralı:** `bType[0] == 3`, `bType[1] == 0` ve `bFlyingEffect != 0` skill artık desteklenir; Type1 (veya başka tip) + `bFlyingEffect != 0` hâlâ `unsupported_skill`. `iUseItem`, `sEtc`, `bMoral` kuralları **değişmez**.
4. **`TickCast` uçan akış:** CASTING → (bekleme) → FLYING gönderimi (guard `CheckCastFly`) → `CAST_FLYING` fazı → (en az `kFlightMinMs` bekleme) → EFFECTING (guard `CheckCastLand`) → döngü sonu. Uçan skill'lerde başlangıç mana gereksinimi `2 × Msp`.
5. **`SubmitCast`** FLYING opcode'unu da taşır (`type:"CastFly"`); EFFECTING telemetrisine `since_flying_ms` eklenir.
6. **`CancelCast`:** `CAST_FLYING` fazında paket gönderilmez, seri paketsiz düşer (mermi çıktı; gerçek istemci uçan mermiyi iptal edemez `[A]`).

**Kapsam dışı (yapılmayacak)**

- Alan uçan skill'ler (`bMoral` 10..13, `Fire burst` `110533`; hedef noktası CLI-07 — ADR-0018 dilim 5), çift tipli uçan skill'ler (`bType[1] != 0`: Ice arrow/orb/blast, Prismatic — dilim 3), okçu **Type2** skill'leri (`bType[0] == 2`: ok tüketimi, yay denetimi; envanter doldurma dilim 8'e bağlı, ayrı küçük dilim "2b"), `UseItem` gerektiren skill'ler (Fire blast, Thunder blast, Impact serisi), görevli skill'ler (`Etc != 0`: Vampiric Fire, Static Thorn).
- **MP düşme sayısının sunucuda değiştirilmesi.** Uçan Type3'te çift MP düşümü sunucu davranışıdır; bot buna uyar (guard `2 × Msp` ister), sunucu koduna dokunulmaz.
- FLYING'den sonra EFFECTING göndermeden seriyi bırakmak için paket (hedef kaybı, ölüm, despawn mevcut sessiz `EndCast` olarak kalır).
- Hedefin uçuş sırasında hareket etmesine göre "ıska" tahmini, uçuş süresinin mesafeye bağlı modeli (`kFlightMinMs` sabit alt sınırdır; ölçülmedi `[A]`), `FLYING` paketinde `sData[0..2]` gerçek istemci değeri (bilinmiyor `[A]`).
- Yeni komut, ini anahtarı, yeni telemetri olayı türü (yalnızca `ACTION_*` altında yeni `type:"CastFly"`), `list`/`snap` çıktısı değişikliği, `BotManager.cpp` değişikliği.
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | `CastStartCheck::msp` genişletme + yeni "flying cast" bölümü (`CheckCastEffect`'ten sonra, "cast cancel" başlığından önce) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | 3 yeni test (96 → 99) |
| `GameServer/Bot/BotSession.h` | değiştir | `CAST_FLYING = 3`, `m_castFlyingAt` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca yorumlar (`CastOutcome` sebep listesine `"flying"`; `BeginCast`/`TickCast`/`CancelCast` açıklamaları) |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | `RejectCast`, `SubmitCast`, `BeginCast`, `TickCast`, `CancelCast` |

`BotSession.cpp` (başlatıcı listesi/`ResetForRespawn()` zaten `m_castPhase = CAST_IDLE` yapar; `m_castFlyingAt` varsayılan değerle kalır) ve `BotManager.cpp` **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

1. `CastStartCheck` içinde `uint16_t msp;` satırını `uint32_t msp;               // MAGIC.Msp, or CastManaNeed(...) for a flying cast` yap (`CheckCastStart` `:225` karşılaştırması `int32_t(c.msp)` ile aynen çalışır; mevcut testlerdeki atamalar değişmez).
2. `CheckCastEffect`'ten sonra, `// --- cast cancel and standing plan` satırından önce ekle (ASCII, CRLF, `namespace BotCore` içinde; yalnızca `<cstdint>`/`<algorithm>`):

```cpp
	// --- flying cast (ADR-0017 Ek F4-25) ---

	// docs/03 CLI-03 [V]: a flying skill goes CASTING -> FLYING -> EFFECTING; the client sent EFFECTING 1037 ms after FLYING
	// (one area skill, n = 3). [A] the flight of a single-target skill may depend on distance: the bot waits at least this long.
	constexpr uint32_t kFlightMinMs = 1000;

	// A Type3 skill with a flying effect (single-typed is checked by the caller). The server charges its MP at FLYING and
	// again at EFFECTING (docs/03 MEC-MAG-12 [D]).
	inline bool IsFlyingCast(uint8_t type0, uint16_t flyingEffect)
	{
		return type0 == 3 && flyingEffect != 0;
	}

	// MP the bot must hold before the first packet of a series: a flying cast pays MAGIC.Msp twice.
	inline uint32_t CastManaNeed(uint16_t msp, bool flying)
	{
		return flying ? uint32_t(msp) * 2 : uint32_t(msp);
	}

	// Guard rule for the FLYING packet. Order: too early (same wait as the EFFECTING of a non-flying cast), range, mana
	// (manaNeed = CastManaNeed(msp, true): FLYING has not charged anything yet), rate.
	inline CastVerdict CheckCastFly(bool inRange, uint32_t sinceCastingMs, uint8_t castTime, int32_t mana,
		uint32_t manaNeed, int actionsInWindow)
	{
		if (sinceCastingMs < CastDurationMs(castTime))
			return CAST_REJECT_TOO_EARLY;

		if (!inRange)
			return CAST_REJECT_OUT_OF_RANGE;

		if (mana < int32_t(manaNeed))
			return CAST_REJECT_NO_MANA;

		if (actionsInWindow >= kMaxActionsPerWindow)
			return CAST_REJECT_RATE;

		return CAST_OK;
	}

	// Guard rule for the EFFECTING packet of a flying cast. Order: flight time, range, mana (manaNeed = MAGIC.Msp: FLYING
	// already took the first half), rate.
	inline CastVerdict CheckCastLand(bool inRange, uint32_t sinceFlyingMs, int32_t mana, uint32_t manaNeed,
		int actionsInWindow)
	{
		if (sinceFlyingMs < kFlightMinMs)
			return CAST_REJECT_TOO_EARLY;

		if (!inRange)
			return CAST_REJECT_OUT_OF_RANGE;

		if (mana < int32_t(manaNeed))
			return CAST_REJECT_NO_MANA;

		if (actionsInWindow >= kMaxActionsPerWindow)
			return CAST_REJECT_RATE;

		return CAST_OK;
	}
```

`BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

Dosyanın sonuna üç `TEST_CASE` (mevcut `CHECK_EQ((int)...)` kalıbıyla):

- **`Combat_FlyingCast_Rules`:** `kFlightMinMs == 1000`; `IsFlyingCast(3, 191) == true`, `IsFlyingCast(3, 0) == false`, `IsFlyingCast(2, 191) == false`, `IsFlyingCast(1, 191) == false`; `CastManaNeed(50, true) == 100`, `CastManaNeed(50, false) == 50`, `CastManaNeed(350, true) == 700`; `CheckCastStart` ile: `CastStartCheck c = {}` (alanlar: `distanceM = 5`, `skillRange = 78`, `distanceField = 50`, `weaponRangeField = 0`, `needsStanding = false`, `standing = true`, `msp = CastManaNeed(50, true)`, `reCastMs = 4300`, `typeGated = true`, diğer bayraklar false, `actionsInWindow = 0`): `mana = 99` ⇒ `CAST_REJECT_NO_MANA`, `mana = 100` ⇒ `CAST_OK`.
- **`Combat_CastFly_Guard`:** `castTime = 15` (`CastDurationMs(15) == 1580`): `CheckCastFly(true, 1579, 15, 100, 100, 0) == CAST_REJECT_TOO_EARLY`; `(true, 1580, 15, 100, 100, 0) == CAST_OK`; `(false, 1580, 15, 100, 100, 0) == CAST_REJECT_OUT_OF_RANGE`; `(true, 1580, 15, 99, 100, 0) == CAST_REJECT_NO_MANA`; `(true, 1580, 15, 100, 100, 6) == CAST_REJECT_RATE`; sıra: `(false, 0, 15, 0, 100, 6) == CAST_REJECT_TOO_EARLY`, `(false, 1580, 15, 0, 100, 6) == CAST_REJECT_OUT_OF_RANGE`, `(true, 1580, 15, 0, 100, 6) == CAST_REJECT_NO_MANA`; `castTime = 0` (CASTING gönderilmedi): `(true, 0, 0, 100, 100, 0) == CAST_OK`.
- **`Combat_CastLand_Guard`:** `CheckCastLand(true, 999, 50, 50, 0) == CAST_REJECT_TOO_EARLY`; `(true, 1000, 50, 50, 0) == CAST_OK`; `(false, 1000, 50, 50, 0) == CAST_REJECT_OUT_OF_RANGE`; `(true, 1000, 49, 50, 0) == CAST_REJECT_NO_MANA`; `(true, 1000, 50, 50, 6) == CAST_REJECT_RATE`; sıra: `(false, 0, 0, 50, 6) == CAST_REJECT_TOO_EARLY`, `(false, 1000, 0, 50, 6) == CAST_REJECT_OUT_OF_RANGE`, `(true, 1000, 0, 50, 6) == CAST_REJECT_NO_MANA`.

### 5.3 `BotSession` (durum)

- `BotSession.h:34`: `enum CastPhase { CAST_IDLE = 0, CAST_ARMED = 1, CAST_CASTING = 2, CAST_FLYING = 3 };` (FLYING gönderildi, EFFECTING için uçuş süresi bekleniyor).
- Cast alanlarının yanına: `std::chrono::steady_clock::time_point m_castFlyingAt;   // IOCP thread only: when FLYING went out (phase CAST_FLYING)`. `BotSession.cpp` değişmez (`time_point` varsayılan kurucu yeterli; yalnızca `CAST_FLYING` fazında okunur).

### 5.4 `ActionExecutor.cpp`

**a) `RejectCast` (`:534`)** — yeni parametre `uint32 earlyLimitMs` (en sona): `CAST_REJECT_TOO_EARLY` dalı `limit = (float)earlyLimitMs` kullanır (`value` = `sinceCastingMs` parametresi, yani "ilgili fazdan beri ms"). Mevcut iki çağrı (`:879`, `:920`) `BotCore::CastDurationMs(m->bCastTime)` geçirir; uçan akışın çağrıları §5.4-d'de. Diğer dallar değişmez.

**b) `SubmitCast` (`:585`)** — iki değişiklik:
1. Yeni parametre `int32 sinceFlyingMs` (`sinceCastingMs`'ten sonra; `-1` = uçan seri değil). `type` adı: `MAGIC_CASTING` → `"CastStart"`, `MAGIC_FLYING` → `"CastFly"`, aksi `"CastEffect"`. `ACTION_SUBMIT` alanları: `CastStart` için mevcut (`cast_ms`); `CastFly` için `"since_casting_ms"`; `CastEffect` için mevcut `"since_casting_ms"` ve `sinceFlyingMs >= 0` ise ek `"since_flying_ms"`.
2. Sonuç eşleme: `opcode == MAGIC_FLYING` iken yankı `op == MAGIC_FLYING` ⇒ `ok = true`, `reason = "flying"`; `op == MAGIC_FAIL` ⇒ `reason = "srv_fail"`; başka ⇒ `"no_result"`. `CASTING` ve `EFFECTING` eşlemeleri aynen kalır. Paket düzeni (`:606-609`) aynı: `u8 opcode, u32 skill, i16 caster, i16 target, i16 sData[0..2]` + üç sıfır `i16` (FLYING'de `sData[0..2]` = EFFECTING ile aynı hedef konumu; gerçek istemci değeri bilinmiyor `[A]`).

**c) `BeginCast` (`:721-733`)** — destek kuralı:

```cpp
	bool flyingCast = BotCore::IsFlyingCast(m->bType[0], m->bFlyingEffect);
	bool supportedType = (m->bType[0] == 1 || m->bType[0] == 3);
	if (!supportedType
		|| m->bType[1] != 0
		|| (m->bFlyingEffect != 0 && !flyingCast)
		|| m->iUseItem != 0
		|| m->sEtc != 0
		|| (m->bMoral != MORAL_SELF && ... /* mevcut moral koşulu aynen */))
```

Yani yalnızca `m->bFlyingEffect != 0` koşulu `(m->bFlyingEffect != 0 && !flyingCast)` olur. Diğer satırlar ve `bad_skill`/`bad_target` kuralları değişmez.

**d) `TickCast` (`:759`)** — akış:

1. `m` alındıktan sonra: `bool flying = BotCore::IsFlyingCast(m->bType[0], m->bFlyingEffect);`
2. `c.msp` ataması (`:855`): `c.msp = (flying && s->m_castPhase != BotSession::CAST_FLYING) ? BotCore::CastManaNeed(m->sMsp, true) : (uint32)m->sMsp;` (ARMED/CASTING'te `2 × Msp`, FLYING fazında `Msp`; `no_mana` reddinin `limit`'i böylece gerçek eşiği gösterir).
3. ARMED bloğu (`:871-903`) **aynen kalır** (`CheckCastStart`, CASTING gönderimi, `m_castTargetId`). `bCastTime == 0` ise bloktan düşer (uçan skill'de sıradaki adım FLYING).
4. CASTING bekleme (`:905-915`) **aynen kalır** (`sinceCastingMs`, `elapsed < wait` ⇒ `return`). `m_castPhase == CAST_FLYING` iken bu blok çalışmaz.
5. **Yeni FLYING gönderimi** (CASTING beklemesinden sonra, EFFECTING'ten önce):

```cpp
	// ADR-0017 Ek F4-25: a flying skill sends FLYING after the cast time and EFFECTING after the flight time.
	if (flying && s->m_castPhase != BotSession::CAST_FLYING)
	{
		bool flyInRange = BotCore::CastInRange(meters, m->sRange, distanceField, weaponRangeField);
		BotCore::CastVerdict flyVerdict = BotCore::CheckCastFly(flyInRange, sinceCastingMs, m->bCastTime,
			c.mana, c.msp, inWindow);
		if (flyVerdict != BotCore::CAST_OK)
			return RejectCast(s, user, flyVerdict, c, sinceCastingMs, m->bCastTime, inWindow,
				BotCore::CastDurationMs(m->bCastTime));

		CastOutcome fly = SubmitCast(s, user, MAGIC_FLYING, s->m_castSkillId, target, sData,
			s->m_castCycle, sinceCastingMs, -1, BotCore::CastDurationMs(m->bCastTime), nowMs, now);
		if (fly.reason != nullptr && std::strcmp(fly.reason, "flying") == 0)
		{
			s->m_castPhase = BotSession::CAST_FLYING;
			s->m_castFlyingAt = now;
			s->m_castTargetId = target.id;   // also for CastTime == 0 (no CASTING went out)
			return fly;                      // SENT "flying"
		}

		// FLYING was not accepted (srv_fail / no_result): drop the series. MP was not charged by a failed FLYING.
		const char * flyReason = fly.reason;
		EndCast(s);
		out.kind = CastOutcome::FAILED;
		out.reason = flyReason;
		return out;
	}
```

   (`SubmitCast` imzasındaki parametre sırası uygulayıcıya bırakılmıştır; yukarıdaki yalnızca niyeti gösterir.) `c.mana` bu çağrıda hâlâ `user->GetMana()` değeridir (FLYING henüz MP düşmedi); mana yetersizliğinde reddedilen seri düşer (`RejectCast` `EndCast` çağırır; FLYING gitmediği için MP harcanmamıştır).
6. **Uçuş bekleme ve EFFECTING:** EFFECTING bloğunun başında (`:917`'den önce):

```cpp
	uint32 sinceFlyingMs = 0;
	if (s->m_castPhase == BotSession::CAST_FLYING)
	{
		long long flown = std::chrono::duration_cast<std::chrono::milliseconds>(now - s->m_castFlyingAt).count();
		if (flown < (long long)BotCore::kFlightMinMs)
			return out;   // NOTHING: the missile is still in the air

		sinceFlyingMs = (uint32)flown;
		sinceCastingMs = (m->bCastTime > 0)
			? (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(now - s->m_castCastingAt).count()
			: 0;
	}
```

   `effectVerdict` (`:918`): `flying` ise `BotCore::CheckCastLand(inRange, sinceFlyingMs, c.mana, c.msp, inWindow)` (`c.msp` bu fazda `Msp`, madde 2), değilse mevcut `CheckCastEffect(...)`. Reddedilirse `RejectCast(..., sinceFlyingMs, ..., BotCore::kFlightMinMs)` (uçan) / mevcut çağrı (uçmayan). `SubmitCast(MAGIC_EFFECTING, ..., sinceFlyingMs = flying ? (int32)sinceFlyingMs : -1, ...)`. Geri kalan (reuse zamanlayıcıları `m_castSkillLast`/`m_castTypeLast`/`m_castAnyLast`, `m_castDone`, `m_castLeft`, `FINISHED`/`CAST_ARMED`'e dönüş) **aynen**: zamanlayıcılar yalnızca EFFECTING'te güncellenir (sunucu da `m_CoolDownList`'i yalnızca EFFECTING başarısında yazar). FLYING anında zamanlayıcı güncellenmez.
7. Uçan olmayan skill'ler için hiçbir davranış değişmez: `flying == false` ⇒ yeni bloklar atlanır, `c.msp` eskisi gibi `Msp`, `RejectCast` çağrıları yalnızca ek parametre taşır.

**e) `CancelCast` (`:988`)** — ARMED dalının koşulu `s->m_castPhase == BotSession::CAST_ARMED || s->m_castPhase == BotSession::CAST_FLYING` olur: `EndCast(s)`, `NOTHING "dropped"`, paket/telemetri yok (mermi çıktı, MP'nin ilk yarısı harcandı; bu bilinen sınırdır). Yorum bildirimi (`.h`) buna göre güncellenir. Hareketle iptal (`BotManager.cpp:3043`) yalnızca `CAST_CASTING`'te kalır: FLYING fazında yürümek seriyi bozmaz.

**f) `ActionExecutor.h`** — yalnızca yorumlar: `CastOutcome::reason` listesine `"flying"`; `BeginCast` yorumuna "flying Type3 single-typed skills are supported (ADR-0017 Ek F4-25)"; `TickCast` yorumuna "SENT 'flying' = FLYING accepted".

**Yasaklar (AC-LRN-03):** yeni kod sunucu nesnesinden başarı çıkarmaz: FLYING sonucu yalnızca `m_castEcho`'dan (op 2), MP/HP/konum okuması yalnızca botun kendi `CUser`'ından guard girdisi olarak (`user->GetMana()`, zaten mevcut). Başka botun `CUser`'ına yeni erişim yok.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.*`, `BotSession.h`, `CombatTests.cpp` için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı üç yeni test adını (`Combat_FlyingCast_Rules`, `Combat_CastFly_Guard`, `Combat_CastLand_Guard`) içerir ve toplam test sayısı **99**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>`; `git diff gece/2026-10-02...bot/F4-25 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş.
- [ ] K5: FLYING paketi yalnızca `ActionExecutor.cpp`'de oluşuyor ve `HandlePacket` ile işleniyor: `grep -n "MAGIC_FLYING" GameServer/Bot/*.cpp GameServer/Bot/*.h` yeni satırları yalnızca `ActionExecutor.cpp`/`.h` içinde gösterir (`BotManager.cpp`, `BotSession.*`, `ScenarioRunner`/`ScriptRunner` yeni satır yok); `BotManager.cpp` ve `BotSession.cpp` `git diff`'te yok.
- [ ] K6: guard atlanmıyor: `SubmitCast(... MAGIC_FLYING ...)` çağrısından önce `CheckCastFly` çağrısı, `CAST_OK` dışında `RejectCast` ile `SubmitCast`'sız dönüş; EFFECTING için uçan seride `CheckCastLand` (kod okumasıyla); FLYING için `SubmitCast`/`HandlePacket` çağrısı **tek yerde**.
- [ ] K7: destek kuralı yalnızca uçan Type3'ü açıyor: `grep -n "bFlyingEffect" GameServer/Bot/ActionExecutor.cpp` `BeginCast`'te `(m->bFlyingEffect != 0 && !flyingCast)` biçimini gösterir; `bType[1] != 0`, `iUseItem != 0`, `sEtc != 0` ve moral koşulları `git diff`'te silinmemiş; `IsFlyingCast` `bType[0] == 3` ister (birim test).
- [ ] K8: uçan olmayan yol gerilemesiz: `git diff gece/2026-10-02...bot/F4-25 -- GameServer/Bot/ActionExecutor.cpp | grep '^-' | grep -v '^---'` yalnızca bu planın açıkladığı satırları gösterir (`RejectCast`/`SubmitCast` imzaları ve TOO_EARLY `limit` satırı, `BeginCast` bayrak koşulu, `c.msp` ataması, `CancelCast` ARMED koşulu, mevcut `RejectCast`/`SubmitCast` çağrı satırlarındaki ek parametre, EFFECTING doğrulama seçimi); `PlanStanding` bloğu, ARMED bloğunun `CheckCastStart`/CASTING gönderimi ve reuse zamanlayıcı satırları silinmemiş.
- [ ] K9: `CancelCast`'te `CAST_FLYING` paketsiz düşer: `grep -n "CAST_FLYING" GameServer/Bot/ActionExecutor.cpp` `CancelCast` içinde `EndCast` + `"dropped"` dalını gösterir; `CancelCast`'teki `kCastCancelCode` paket yolu (CASTING) değişmedi.
- [ ] K10: `ENABLED=0` davranışı değişmez: `GameServer/Bot/BotManager.cpp` ve `BotSession.cpp` `git diff --stat`'te yok; yeni ini anahtarı, komut, thread yok.
- [ ] K11: `git diff --stat gece/2026-10-02...bot/F4-25` yalnızca §4'teki 5 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş; `GameServer/` içinde `Bot/` dışında dosya değişmemiş.
- [ ] K12: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp`/`ActionExecutor.*`/`BotSession.h` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K13: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K14: gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 96 testin tamamı hâlâ geçiyor; `EmitFairnessReject` çağrıları `"Move"`/`"Attack"`/`"Cast"`/`"CastCancel"`/`"Potion"` geçiyor.
- [ ] K15: `python3 tools/check-perception-contract.py` `RESULT: PASS` ve sayılar değişmez (`R1 0/0`, `R2 0/28`, `R3 0/18`, `R4 0/0`, `R5 0/0`); yeni kod başka botun `CUser`'ına (`t->m_pUser`) erişmez, yalnızca `s->m_pUser`.
- [ ] K16 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S5 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-25
git diff gece/2026-10-02...bot/F4-25 -- GameServer/Bot/ActionExecutor.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-25 -- GameServer/Bot/BotManager.cpp GameServer/Bot/BotSession.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "MAGIC_FLYING\|CAST_FLYING\|CheckCastFly\|CheckCastLand\|bFlyingEffect" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/BotSession.h
git diff --check gece/2026-10-02...bot/F4-25
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; bir dosyadaki satırlar aynı `Tick()`'te sırayla çalışır); zamanlama için geçici betik `./Scripts/f425_*.txt` (silinir, commit edilmez). Botlar: Karus mage `BotMF_K` (Fire ball `110515`: `Msp 50`, `CastTime 15` ⇒ CASTING→FLYING ≈ 1580 ms, FLYING→EFFECTING ≥ 1000 ms, `ReCastTime 43` ⇒ ardışık cast ≥ 4300 ms, `Range 78`), El Morad warrior `BotWP_E` hedef; zone 71, botlar aynı başlangıç noktasındadır. MP ve HP `list` çıktısından, olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'dan.

1. **S1 Tek uçan cast + MP muhasebesi (MEC-MAG-12 `[V]`):** `spawn BotMF_K,BotWP_E`; MP/HP `list` ile not al; `cast BotMF_K 110515 BotWP_E 1`. Beklenen JSONL sırası: `CastStart` (`ACTION_SUBMIT`→`ACTION_RESULT` `casting`) → `CastFly` (`since_casting_ms` ≥ 1580; `ok:true`, `reason:"flying"`, `op:2`) → `CastEffect` (`since_flying_ms` ≥ 1000; `ok:true`, `effected` ya da `missed`, `op:3`); log `cast finished (effected) after 1 cycle(s), 1 ok, 3 packet(s) sent`; cast sonrası MP düşüşü **≈ 100 (2 × 50)** (doğal MP yenilenmesi için birkaç puan tolerans; FLYING sonrası ~1 sn'de ara `list` ile ilk yarının (≈ 50) FLYING'de düştüğü de görülür); hedef `BotWP_E` HP düşer (`effected` ise). **Düşüş ≈ 50 çıkarsa** `[D]` iddiası (çift düşüm) yanlıştır: `docs/03` MEC-MAG-12 ve guard'ın `2 × Msp` kuralı düzeltme planına gider; bu doğrulama bulgusu olarak raporlanır.
2. **S2 Üç döngü + tempo:** `cast BotMF_K 110515 BotWP_E 3`: `CastStart`/`CastFly`/`CastEffect` ×3; ardışık EFFECTING→sonraki `CastStart` ≥ 4300 ms (`ReCastTime 43`); toplam MP düşüşü ≈ 300; log `cast finished (effected) after 3 cycle(s), 3 ok, 9 packet(s) sent`; `FAIRNESS_REJECT` yok. `cast BotMF_K 110527 BotWP_E 1` (Fire spear, `Msp 80`, seviye 27) ve `110751` (Static orb, `Msp 160`, `ReCastTime 53`) aynı sırayı verir; MP düşüşü ≈ 160 / ≈ 320.
3. **S3 İptal ve FLYING fazı:** (a) `cast BotMF_K 110515 BotWP_E 1` ~500 ms sonra `cast BotMF_K off` (CASTING aşaması): F4-24 davranışı `cancelled` (`op:4`, `code:-100`), `CastFly` yok, MP değişmez. (b) `cast ... 110515 ...` ~2,0 sn sonra `cast BotMF_K off` (FLYING aşaması, FLYING +~400 ms): log `stopped after 2 packet(s) sent`; JSONL'de iptal paketi (`CastCancel`) **yok**, `CastEffect` **yok**; MP ≈ 50 düşmüş (yalnızca FLYING'in yarısı). (c) CASTING aşamasında `move` ile iptal (F4-24 S2) ve ardından yeniden uçan cast: seri/zamanlayıcı bozulmamış, tam akış çalışır. (d) FLYING aşamasında `move BotMF_K ...` seriyi **bozmaz**: `CastEffect` yine gönderilir (menzil içindeyse).
4. **S4 Reddedilen/desteklenmeyen skill'ler ve guard:** `cast BotMF_K 110533 BotWP_E 1` (Fire burst, Moral 10) → `unsupported_skill`; `110535` (UseItem) → `unsupported_skill`; `110574` (Etc 517) → `unsupported_skill`; `110615` (Ice arrow, çift tipli) → `unsupported_skill`; `cast BotWP_K 110515 BotWP_E 1` → `bad_skill` (sınıf); `cast BotMF_K 110515 self` → `bad_target`; menzil dışı hedef (`move` ile ≥ 80 m uzaklaş) → CASTING gitmeden `FAIRNESS_REJECT` (`MEC-MAG-11`, `out_of_range`). `no_mana` (`2 × Msp` eşiği) gerçek sunucuda üretilemezse raporda "sınanmadı (birim testle kapsandı)" yazılır.
5. **S5 Gerilemesiz:** `cast BotMF_K 110518 BotWP_E 3` (Ignition, uçmayan) önceki gibi `effected` ×3 ve **`CastFly` olayı yok**, 6 paket; `cast BotMF_K 110518 self` → `bad_target`; 30 m `move`/`stop` (F4-01); `attack BotWP_K BotWP_E 3` (F4-02); `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama uçan cast çalışır (log satırları); `ENABLED=0` → komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde (≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Bölgedeki ikinci bir bot `/bot snap <bot> events` ile **FLYING** olayını (`event ... op=2 skill=110515 caster=<mage> target=<hedef>`, F4-52 halkası) görür. İnsan istemcisi gerekmez (gerçek istemcide mermi görünümü `T-CAST-FLY-01`). Temizlik: ini yedekten geri, geçici betik silindi, bot satırlarının durumu önceki gibi.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **uçan** skill serisinde çalışır; uçmayan skill'ler ve cast etmeyen botlar için davranış değişmez.
- **Thread kuralı (ADR-0005):** `TickCast`/`CancelCast` yalnızca IOCP thread'inde (`Tick()` içinden; komutlar `EnqueueCommand` kuyruğundan). `OnPacket()` her thread'den çağrılabilir; bu plan onu **değiştirmez** (FLYING yankısı F4-03'ten beri `m_castEcho`'ya yazılıyor). `ActionExecutor` log yazmaz; log satırları `BotManager`'dadır (değişmez: `SENT "flying"` için log yok, yalnızca `FINISHED`/`REFUSED`/`FAILED` yazılır).
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): FLYING "başarılı" yalnızca sunucunun bölgeye yayınladığı, çağırana da gelen `MAGIC_FLYING` paketinden (aynı skill kimliği) çıkarılır; sunucu nesnesinden başarı çıkarılmaz.
- **Bilinen sınırlar `[A]`:** (a) `kFlightMinMs = 1000` tek ölçüme (alan büyüsü, n = 3: 1037 ms) dayanır; tek hedefli büyüde uçuş mesafeye bağlı olabilir, bot sabit alt sınırı bekler, ölçülmedi; (b) FLYING paketinin `sData[0..2]` alanının gerçek istemci değeri bilinmiyor (bot EFFECTING ile aynı hedef konumunu gönderir); (c) CASTING → FLYING bekleme süresi `CastTime × 100 + 80` ms'dir (ölçülen `+39` ms; muhafazakâr), (d) mermi uçarken hedef ölürse/kaybolursa bot EFFECTING göndermez, seri sessiz düşer (mevcut `target_lost` yolu `TickCast`'a girmeden `EndCast` çağırır); gerçek istemci EFFECTING'i yine gönderirdi, davranış karar katmanı konusudur; (e) MP'nin FLYING'de düşmesi (MEC-MAG-12) kod okumasından `[D]`, çalışma zamanı ölçümü S1'de; (f) FLYING'den sonra seri düşerse (iptal, hedef kaybı, ölüm) MP'nin ilk yarısı geri gelmez.
- Telemetri hacmi küçüktür (döngü başına +2 olay: `CastFly` SUBMIT/RESULT); `droppable = false`, `IsEnabled` denetimi önceki aksiyonlarla aynı. `tools/bot-telemetry-report.py` değişmez (MET-ACT-02 `type`'tan bağımsız `reason` sınıflandırır; `flying` `ok:true`).
- `list`/`snap` ve `Telemetry.*` değişmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-25` — `<kısa-sha> [F4-25] …`
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
- İncelenen: `gece/2026-10-02...bot/F4-25` @ `<sha>`
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
