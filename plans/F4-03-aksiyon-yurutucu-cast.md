# F4-03: `ActionExecutor` cast dilimi — `CastStart`/`CastEffect` (tek hedefli Type1/Type3 skill) ve `BotFairnessGuard` CLI-03 / CLI-04 / CLI-09 kuralları

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-03` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-02 (saldırı dilimi, guard deseni, `m_actionWindow`) — `KAPANDI`; F4-01 (hareket) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-09, CLI-11, MEC-MAG-01..08, MEC-MAG-11; MET-ACT-02, MET-FAIR-01 altyapısı; AC-LRN-03 |
| Tahmini büyüklük | M (8 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun üçüncü gerçek aksiyonu: bir bot **gerçek `WIZ_MAGIC_PROCESS` paketlerini `CUser::HandlePacket()` üzerinden** göndererek tek hedefli bir skill kullanır: cast süreli skill'de `CASTING` → (`CastTime × 100 ms + 80 ms` bekleme) → `EFFECTING`, `CastTime = 0` olan skill'de yalnızca `EFFECTING`. Her paket sunucuya gitmeden önce `BotFairnessGuard` kurallarından geçer: skill menzili (MEC-MAG-11), ayakta skill (CLI-09), MP (MEC-MAG-08), skill başına yeniden kullanım (CLI-04), tip kapısı (MEC-MAG-03), cast boşluğu (CLI-04), cast süresi (CLI-03) ve aksiyon hızı tavanı (CLI-11). Sonuç, sunucunun yayınladığı `WIZ_MAGIC_PROCESS` sonuç paketinden okunur (hedefin HP'sine **bakılmaz**). Karar katmanı yoktur: aksiyonu `/bot cast` komutu tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün üçüncü dilimidir (ADR-0017 Karar 4: hareket → saldırı → **cast** → pot → `Perception`; kararlar ADR-0017 "Ek (F4-03)").

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-03)" (bu planla birlikte yazıldı): kapsam sınırı, sonuç eşlemesi, guard kuralları, iptal yok.
- `plans/F4-02-aksiyon-yurutucu-saldiri.md` §5.2–§5.5 ve Doğrulama Raporu: bu planın kalıpladığı iskelet (saf mantık `BotCore/BotCombat.h` + `ActionExecutor` + `BotSession` durumu + `BotManager` komutu + `echo` atomiği). **F4-02'nin yazılı hâlini değil, birleşmiş kodu esas al.**
- `docs/03` §4.2 **MEC-MAG-01..11** (sunucu skill kapıları; MEC-MAG-11 bu planla birlikte eklendi) ve §14 (CLI ölçümleri): **CLI-03** (CASTING → EFFECTING = `CastTime×100 ms + 70–90 ms`; ölçümler: Ignition 1089–1095 ms, Superior healing 1565–1572 ms; Type1 skill'de istemci CASTING göndermez), **CLI-04** (cast döngüsü `CastTime×100 + ~70 + ~140 ms`; Type1 sunucu tip kapısı ≥ 1 sn), **CLI-09** (`UseStanding`), **CLI-11** (≤ 6/sn), paket düzeni düzeltmesi ("**21 bayt** `u8 opcode, u32 skill, i16 caster, i16 target, i16 data[6]`").
- `docs/13` §5.2 (`Action`), §8 (sonuç eşleme), `docs/16` §3.2 (olaylar), MET-ACT-02.
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `a83ebde` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/User.cpp:391-393` — `case WIZ_MAGIC_PROCESS: CMagicProcess::MagicPacket(pkt, this);`.
  - `GameServer/MagicProcess.cpp:16-53` — `MagicPacket()`: `pkt >> bOpcode >> nSkillID`; tablo yoksa dön; `pkt >> sCasterID >> sTargetID >> sData[0..6]` (7 alan; 21 baytlık paketin 7. alanı `ByteBuffer::read<T>(pos)` taşan okumada `0` döner, `shared/ByteBuffer.h:110-116`); **`sCasterID != pCaster->GetID()` ise sessizce döner** (`:47-49`); sonra `instance.Run()`.
  - `GameServer/MagicInstance.cpp:10-37` — `Run()`: `CheckSkillPrerequisites()` veya `UserCanCast()` başarısızsa `SendSkillFailed()` (kendine `MAGIC_FAIL`) ve dön; `:39-42` `MAGIC_CASTING`/`MAGIC_FAIL` → `SendSkill(bOpcode == MAGIC_CASTING)` (CASTING bölgeye, `MAGIC_FAIL` yalnızca yayıncıya); `:96-` `MAGIC_EFFECTING` → `ExecuteSkill(bType[0])`.
  - `GameServer/MagicInstance.cpp:150-293` — `UserCanCast()`: `canUseSkills()`/ölü (`:165-167`), `sSkill != 0` ise `m_sClass != sSkill/10` veya `GetLevel() < sSkillLevel` → ret (`:184-189`), `GetMana() - sMsp < 0` → ret (`:194-195`), `iUseItem` denetimleri (`:243-260`), `sEtc != 0` ve GM değilse görev denetimi (`:270-275`, `#if !defined(DEBUG)`), `bType[0] < 4` ise `isInAttackRange()` (`:278-281`), `IsAvailable()` (`:283-286`, `bMoral` kuralları: `MORAL_SELF` yalnızca kendine, `MORAL_ENEMY` düşman, `MORAL_FRIEND_WITHME` kendine veya dost).
  - `GameServer/MagicInstance.cpp:296-440` — `CheckSkillPrerequisites()` (yalnızca EFFECTING/FLYING'de tam; CASTING'te yalnızca `UseStanding` menzili `:304-308`): bölge/`sUseStanding == 1 && m_sSpeed != 0` (`:328-330`, MEC-MAG-07), `sRange > 0 && mesafe >= sRange` (`:352-356`, MEC-MAG-11), per-skill soğuma `:360-370` (`0 < (UNIXTIME−son)·1000 < sReCastTime·100`), tip kapısı `:386-422` (`PLAYER_SKILL_REQUEST_INTERVAL = 0.7`, `GameServer/User.h:23`; `UNIXTIME` 1 sn çözünürlüklü), hedef `FREEZE` (`:430-436`).
  - `GameServer/MagicInstance.cpp:705-720` — `SendSkillFailed()`: `sData[3] = (opcode == CASTING ? SKILLMAGIC_FAIL_CASTING(-100) : SKILLMAGIC_FAIL_NOEFFECT(-103))`, `MAGIC_FAIL` paketi yalnızca yayıncıya (`Send`). `:730-760` `BuildSkillPacket`: **yayınlanan paket düzeni `opcode(int8), nSkillID(u32), caster(i16), target(i16), sData[0..6] (i16 × 7)`** (payload 23 bayt; opcode `GetOpcode()`'tan değil, payload'ın ilk baytıdır).
  - `GameServer/MagicInstance.cpp:1061-1115` — `ExecuteType1()`: hasar 0 ise `sData[3] = SKILLMAGIC_FAIL_ATTACKZERO (-104)`, her durumda `SendSkill()` (bölgeye, **opcode 3**); `:1263-1620` `ExecuteType3()`: tek hedefte hedef ölü/yok → `return false` (paket yok); hedef başına `BuildAndSendSkillPacket(pSkillCaster, true, …, bOpcode, …)` (`:1599`, bölgeye), `:1613` `SendSkill()`.
  - `shared/packets.h:379-402` — `MagicOpcode` (`MAGIC_CASTING = 1`, `MAGIC_FLYING = 2`, `MAGIC_EFFECTING = 3`, `MAGIC_FAIL = 4`) ve `e_SkillMagicFailMsg` (`-100 … -104`).
  - `shared/database/structs.h:3-26` — `_MAGIC_TABLE` (`iNum`, `bMoral`, `sSkillLevel`, `sSkill`, `sMsp`, `iUseItem`, `bCastTime`, `sReCastTime`, `bType[2]`, `sRange`, `sUseStanding`, `sEtc`, `bFlyingEffect`); `GameServer/MagicInstance.h:25-37` `MORAL_*` (`MORAL_SELF=1`, `MORAL_FRIEND_WITHME=2`, `MORAL_ENEMY=7`, `MORAL_ALL=8`, `MORAL_AREA_ENEMY=10 … MORAL_SELF_AREA=13`); tablo `g_pMain->m_MagictableArray.GetData(id)` (`MagicProcess.cpp:25`).
  - `GameServer/Unit.h:97-98` `GetMana()`/`GetMaxMana()`; `GameServer/User.h:122` `m_sClass`; `GameServer/Unit.cpp:926-947` `CanCastRHit` (R'ye özgü, cast'e dokunmaz).
  - `GameServer/Bot/ActionExecutor.cpp` (512 satır) — `NextDecisionId`, `FormatFixed`, `EmitFairnessReject(..., type, rule, reason, value, limit)`, `TickAttack()` (`:320-502`) tam deseni; `BotSession.cpp:15-30` `OnPacket()` ve `:32-` `ResetForRespawn()`; `BotManager.cpp:591-633` `ExecuteCommand()`, `:1169-1308` `CommandAttack()`, `:1584-1634` `TickSessions()` saldırı bloğu, `:777-822` `BuildStatusLines()` (`char message[192]`, biçim `:815`), `:1697-1701` `BeginDespawn()`.
  - Yerel `MAGIC` verisi (`SELECT`, yasak tablolar dışında; Claude doğruladı): `110518` Ignition (`Moral 7, Skill 1105, SkillLevel 18, Msp 60, CastTime 10, ReCastTime 1, Type1 3, Range 56, UseStanding 0, UseItem 0, Etc 0, FlyingEffect 0`), `210518` (El Morad, `Range 90`), `106560`/`206560` sword dancing (`Moral 7, Skill 1065/2065, SkillLevel 60, Msp 300, CastTime 0, ReCastTime 5, Type1 1, Range 0`), `112545`/`212545` Superior healing (`Moral 2, Skill 1125/2125, SkillLevel 45, Msp 320, CastTime 15, ReCastTime 1, Type1 3, Range 56`), `110533` Fire burst (`Moral 10, FlyingEffect 191`: **bu planın kapsamı dışı**). Karus sınıf kodları 106/110/112, El Morad 206/210/212 (`db/002`); `Skill/10 == m_sClass`.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** cast süresi, yeniden kullanım, menzil, bekleme hesabı, `CheckCastStart` / `CheckCastEffect` guard kuralları. Birim testleri (`CombatTests.cpp`).
2. **`ActionExecutor` (`BeginCast` / `TickCast` / `EndCast`):** `WIZ_MAGIC_PROCESS` paketlerini oluşturur, guard'dan geçirir, `CUser::HandlePacket()` ile işletir, sunucunun yayınladığı sonuç paketinden `casting` / `effected` / `missed` / `srv_fail` / `no_result` eşler, telemetri olaylarını üretir.
3. **Oturum durumu (`BotSession`):** cast serisi durumu (skill, hedef adı, kalan çevrim, faz, CASTING zamanı), skill/tip/genel son-EFFECTING zamanları (CLI-04, MEC-MAG-03), atomik `m_castSelfId` ve `m_castEcho`; `OnPacket()` kendi `WIZ_MAGIC_PROCESS` paketini kaydeder.
4. **Komutlar (`BotManager`):** `cast <bot> <skill id> <hedefbot|self> [çevrim]` ve `cast <bot>|all off` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten); her `Tick()`'te seriyi ilerletme; `list` satırına `casting=` ve `mp=`.
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT` / `ACTION_RESULT` (`"type":"CastStart"` ve `"CastEffect"`), `FAIRNESS_REJECT` (`"type":"Cast"`).

**Kapsam dışı (yapılmayacak)**

- **Uçan skill'ler** (`bType[0] == 2` veya `bFlyingEffect != 0`: `MAGIC_FLYING` fazı, uçuş süresi, ok/mermi tüketimi), **alan skill'leri** (`bMoral` 10..13: hedef noktası `sData`, `sTargetID = -1`), **Type4–Type9** (buff, dönüşüm, diriltme, gizlenme…), **eşya tüketen** (`iUseItem != 0`) ve **göreve bağlı** (`sEtc != 0`) skill'ler: `unsupported_skill` ile reddedilir. Pot (CLI-06) ve envanter doldurma ayrı dilim.
- **Cast iptali:** hareketle iptal, `cast … off` veya hedef kaybı CASTING sonrası seriyi **sessizce** düşürür; istemcinin gönderdiği `MAGIC_FAIL (-100)` iptal paketi **gönderilmez** (ADR-0017 Ek F4-03 madde 6). `cast` sırasında `move` verilirse cast sürer (test sürücüsü; çakışmayı karar katmanı yönetir).
- `UseStanding = 1` skill için **otomatik durma** (CLI-09'un "durma hareketi + bir tick bekleme" kısmı): bu planda yürüyen bot `not_standing` ile **reddedilir** (otomatik durdurma karar katmanının işi). Yerel veride `UseStanding = 1` skill yoksa kural yalnızca birim testiyle sınanır.
- MEC-MAG-04 istisnası (element-0 Type3 heal'inin tip kapısını silmesi): uygulanmaz, tip kapısı her Type1/Type3 skill için muhafazakâr uygulanır.
- Buff/algı: `BUFF_TYPE_INSTANT_MAGIC` (anlık cast), cast hızı etkileri, `Sit`/`Regene`, ölü botu diriltme yok.
- **Hedef seçimi, menzile yaklaşma, kovalama:** `cast` yalnızca test komutudur; bot menzil dışındaysa cast etmez (guard reddi), yürümez. Hedef olarak NPC/canavar/gerçek oyuncu yok; yalnızca spawn edilmiş **bot** veya `self`. Hedef konumu/kimliği hedef botun oturumundan okunur (`Perception` dilimi değiştirecek).
- **CLI-02:** R ile skill arasında kilit **yoktur** (F1 ölçümü); `attack` ve `cast` bağımsız zamanlayıcılardır, aralarında kilit eklenmez. Aynı bot aynı anda hem `attack` hem `cast` serisi yürütebilir; yalnızca CLI-11 penceresi ortaktır.
- `ChatHandler.cpp` `+bot` yardım metni (KI-012; Claude doğrulamada günceller). Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya yok; **`GameServer` projesine dosya eklenmez**.
- Dokümanları (`docs/13`, `docs/16`, `docs/03`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | Yalnızca ekleme (§5.2); mevcut içerik ve `#include`'lar değişmez |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | Yalnızca ekleme: beş yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `CastTarget`, `CastOutcome`, üç yeni statik fonksiyon |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | Cast yolu (§5.4); mevcut hareket/saldırı kodu değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca cast durumu üyeleri ve `#include <map>` |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi, `ResetForRespawn()`, `OnPacket()` içinde `WIZ_MAGIC_PROCESS` |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandCast` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, komut, `TickSessions()`, `BeginDespawn()`, `BuildStatusLines()` |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (Yeni dosya açılmaz: guard `BotCombat.h`'ye eklenir, böylece `BotCore*.vcxproj` değişmez.)

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-03 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotCombat.h` (saf mantık, mevcut `namespace BotCore` içine ekleme)

Mevcut kodun biçimini (tab, Allman, `inline`, İngilizce kısa yorum, ölçülmemiş değer `[A]`) koru. Yeni `#include` gerekmez (`<algorithm>`, `<cstdint>` zaten var). `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez**. `ActionRateWindow`'dan **sonra**, `namespace`'in kapanışından önce ekle:

```cpp
	constexpr uint32_t kCastExtraMs = 80;    // docs/03 CLI-03: CASTING -> EFFECTING = CastTime*100 + 70..90 ms (measured)
	constexpr uint32_t kCastGapMs   = 140;   // docs/03 CLI-04: 135..140 ms between EFFECTING and the next CASTING (measured)
	constexpr uint32_t kTypeGateMs  = 1000;  // docs/03 MEC-MAG-03 / MEC-MAG-10: same type, skill id < 400000

	// Time the bot waits between CASTING and EFFECTING; 0 when the skill has no cast time (no CASTING packet then).
	inline uint32_t CastDurationMs(uint8_t castTime);   // castTime == 0 ? 0 : castTime * 100 + kCastExtraMs
	// Per-skill reuse time (MEC-MAG-02, CLI-04): ReCastTime is in 0.1 s units, real time, no whole-second rounding.
	inline uint32_t CastRecastMs(uint16_t reCastTime);  // reCastTime * 100

	// Skill range (MEC-MAG-11): skillRange (metres, MAGIC.Range) > 0 -> distanceM < skillRange (the server rejects >=);
	// skillRange == 0 (weapon-bound Type1) -> 0 <= distanceField <= weaponRangeField (same 0.1 m field as R).
	inline bool CastInRange(float distanceM, uint16_t skillRange, int16_t distanceField, int16_t weaponRangeField);

	struct CastStartCheck
	{
		float distanceM;            // caster -> target, metres (0 for a self cast)
		uint16_t skillRange;        // MAGIC.Range
		int16_t distanceField;      // DistanceField(distanceM)
		int16_t weaponRangeField;   // AttackRangeField(...)
		bool needsStanding;         // MAGIC.UseStanding == 1
		bool standing;              // the bot has no walk in progress
		int32_t mana;               // caster's current MP
		uint16_t msp;               // MAGIC.Msp
		uint32_t reCastMs;          // CastRecastMs(MAGIC.ReCastTime)
		bool hasSkillLast;          // this skill was effected earlier in this spawn
		uint32_t sinceSkillLastMs;
		bool typeGated;             // MAGIC.Type1/Type2 in 1..7 and skill id < 400000 (MEC-MAG-03)
		bool hasTypeLast;           // one of the skill's gated types was effected earlier
		uint32_t sinceTypeLastMs;   // the smallest "since" over the skill's gated types
		bool hasAnyLast;            // any skill was effected earlier in this spawn
		uint32_t sinceAnyLastMs;
		int actionsInWindow;        // ActionRateWindow::CountInWindow(now)
	};

	enum CastVerdict
	{
		CAST_OK = 0,
		CAST_REJECT_OUT_OF_RANGE = 1,   // MEC-MAG-11
		CAST_REJECT_NOT_STANDING = 2,   // CLI-09 / MEC-MAG-07
		CAST_REJECT_NO_MANA = 3,        // MEC-MAG-08
		CAST_REJECT_RECAST = 4,         // CLI-04 (per skill)
		CAST_REJECT_TYPE_GATE = 5,      // MEC-MAG-03
		CAST_REJECT_GAP = 6,            // CLI-04 (gap after the previous EFFECTING)
		CAST_REJECT_RATE = 7,           // CLI-11
		CAST_REJECT_TOO_EARLY = 8       // CLI-03 (EFFECTING before the cast time ran out)
	};

	// Milliseconds until the timing rules (recast, type gate, gap) allow the next cast to start; 0 = now.
	// Range, standing, mana and rate are NOT timing waits and are not part of this value.
	inline uint32_t CastWaitMs(const CastStartCheck & c);

	// Guard rule for the first packet of a cast (CASTING, or EFFECTING when there is no cast time).
	// Order: range, standing, mana, recast, type gate, gap, rate.
	inline CastVerdict CheckCastStart(const CastStartCheck & c);

	// Guard rule for the EFFECTING packet. Order: too early, range, rate.
	inline CastVerdict CheckCastEffect(bool inRange, uint32_t sinceCastingMs, uint8_t castTime, int actionsInWindow);
```

Kurallar:

- `CastDurationMs`: `castTime == 0 ? 0 : uint32_t(castTime) * 100 + kCastExtraMs`. `CastRecastMs`: `uint32_t(reCastTime) * 100`.
- `CastInRange`: `skillRange > 0 ? distanceM < float(skillRange) : (distanceField >= 0 && distanceField <= weaponRangeField)`.
- `CastWaitMs`: üç alanın en büyüğü, her biri 0 tabanlı: `hasSkillLast && sinceSkillLastMs < reCastMs` → `reCastMs - sinceSkillLastMs`; `typeGated && hasTypeLast && sinceTypeLastMs < kTypeGateMs` → `kTypeGateMs - sinceTypeLastMs`; `hasAnyLast && sinceAnyLastMs < kCastGapMs` → `kCastGapMs - sinceAnyLastMs`.
- `CheckCastStart` sırasıyla: (1) `!CastInRange(c.distanceM, c.skillRange, c.distanceField, c.weaponRangeField)` → `OUT_OF_RANGE`; (2) `needsStanding && !standing` → `NOT_STANDING`; (3) `mana < int32_t(msp)` → `NO_MANA`; (4) `hasSkillLast && sinceSkillLastMs < reCastMs` → `RECAST`; (5) `typeGated && hasTypeLast && sinceTypeLastMs < kTypeGateMs` → `TYPE_GATE`; (6) `hasAnyLast && sinceAnyLastMs < kCastGapMs` → `GAP`; (7) `actionsInWindow >= kMaxActionsPerWindow` → `RATE`; aksi halde `CAST_OK`.
- `CheckCastEffect` sırasıyla: (1) `sinceCastingMs < CastDurationMs(castTime)` → `TOO_EARLY`; (2) `!inRange` → `OUT_OF_RANGE`; (3) `actionsInWindow >= kMaxActionsPerWindow` → `RATE`; aksi halde `OK`. (`castTime == 0` ve `sinceCastingMs = 0` → `OK`.)

**Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`'ye ekleme)**, mevcut stille (`TEST_CASE`, `CHECK_EQ((int)…, …)`); enum karşılaştırmalarını `int`'e çevir; kayan nokta sınırında kalan değer kullanma. Yardımcı: dosyanın içinde `static BotCore::CastStartCheck OkCast()` (her şey geçerli: `distanceM = 3.0f`, `skillRange = 56`, `distanceField = 30`, `weaponRangeField = 20`, `needsStanding = false`, `standing = true`, `mana = 1000`, `msp = 60`, `reCastMs = 100`, üç `has* = false`, `sinceX = 0`, `typeGated = true`, `actionsInWindow = 0`). En az şu test durumları:

- `Combat_CastDuration`: `CastDurationMs(0)` = 0; `(10)` = 1080; `(15)` = 1580; `CastRecastMs(5)` = 500; `(1)` = 100; `(0)` = 0.
- `Combat_CastInRange`: `(55.9f, 56, 0, 20)` → doğru; `(56.0f, 56, 0, 20)` → yanlış (`>=` reddedilir); `(0.0f, 56, 0, 20)` → doğru; `skillRange = 0` iken `(99.0f, 0, 20, 20)` → doğru, `(0.0f, 0, 21, 20)` → yanlış, `(0.0f, 0, -1, 20)` → yanlış; `skillRange > 0` iken alan değeri dikkate alınmaz: `(1.0f, 56, 32767, 20)` → doğru.
- `Combat_CastStart_Order`: `OkCast()` → `OK`; tek tek bozulunca: `distanceM = 56.0f` → `OUT_OF_RANGE`; `needsStanding = true, standing = false` → `NOT_STANDING`; `mana = 59` → `NO_MANA` (`mana = 60` → `OK`); `hasSkillLast = true, sinceSkillLastMs = 99` → `RECAST` (`100` → `OK`); `hasTypeLast = true, sinceTypeLastMs = 999` → `TYPE_GATE` (`1000` → `OK`; `typeGated = false` iken `0` → `OK`); `hasAnyLast = true, sinceAnyLastMs = 139` → `GAP` (`140` → `OK`); `actionsInWindow = 6` → `RATE` (`5` → `OK`). Sıra: menzil dışı + mana yok → `OUT_OF_RANGE`; mana yok + pencere dolu → `NO_MANA`; recast + tip kapısı birlikte → `RECAST`; tip kapısı + boşluk birlikte → `TYPE_GATE`; boşluk + pencere dolu → `GAP`.
- `Combat_CastWait`: hiçbir `has*` yokken 0; `hasSkillLast, reCastMs = 500, sinceSkillLastMs = 200` → 300; `hasTypeLast, typeGated, sinceTypeLastMs = 400` → 600; `typeGated = false` iken aynısı → 0; `hasAnyLast, sinceAnyLastMs = 100` → 40; üçü birlikte (300, 600, 40) → 600; `sinceSkillLastMs >= reCastMs` → 0.
- `Combat_CastEffect`: `CheckCastEffect(true, 1079, 10, 0)` → `TOO_EARLY`; `(true, 1080, 10, 0)` → `OK`; `(false, 1080, 10, 0)` → `OUT_OF_RANGE`; `(true, 1080, 10, 6)` → `RATE`; `(true, 0, 0, 0)` → `OK` (cast süresiz); sıra: `(false, 0, 10, 6)` → `TOO_EARLY`; `(false, 1080, 10, 6)` → `OUT_OF_RANGE`.

Beklenen toplam test sayısı: 19 + 5 = **24**.

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'ye `#include <map>` ve (hepsi "IOCP thread only", mevcut üyelerin biçiminde yorumla; saldırı üyelerinin **altına**, atomiklerin üstüne) ekle:

```cpp
	enum CastPhase { CAST_IDLE = 0, CAST_ARMED = 1, CAST_CASTING = 2 };

	uint8 m_castPhase;                                     // IOCP thread only: CastPhase
	uint32 m_castSkillId;                                  // IOCP thread only
	std::string m_castTargetName;                          // IOCP thread only: target bot's character name; empty = self
	uint32 m_castLeft;                                     // IOCP thread only: cycles still to complete
	uint32 m_castCycle;                                    // IOCP thread only: cycles started in this series (1-based in telemetry)
	uint32 m_castDone;                                     // IOCP thread only: cycles whose EFFECTING result was effected/missed
	uint32 m_castPackets;                                  // IOCP thread only: WIZ_MAGIC_PROCESS packets sent in this series
	std::chrono::steady_clock::time_point m_castCastingAt; // IOCP thread only: when CASTING went out (phase CAST_CASTING)
	std::map<uint32, std::chrono::steady_clock::time_point> m_castSkillLast;   // IOCP thread only: skill id -> last EFFECTING sent
	bool m_castTypeHas[8];                                 // IOCP thread only: per skill type 0..7
	std::chrono::steady_clock::time_point m_castTypeLast[8];   // IOCP thread only
	bool m_castAnyHas;                                     // IOCP thread only
	std::chrono::steady_clock::time_point m_castAnyLast;   // IOCP thread only: last EFFECTING of any skill

	std::atomic<int> m_castSelfId;                         // set by ActionExecutor (IOCP thread), read by OnPacket(): own caster id, -1 = none
	std::atomic<uint64> m_castEcho;                        // written by OnPacket(), see below
```

(`m_castSelfId`/`m_castEcho` atomik bloğa, `m_attackEcho`'nun yanına; `CastPhase` enum'ı `Phase`/`SelectResult` yanına.)

`m_castEcho` kodlaması (`OnPacket` yazar, `ActionExecutor` okur): `0` = yok; aksi halde `(1ull << 63) | (uint64(opcode & 0xF) << 48) | (uint64(uint16(sData3)) << 32) | uint64(skillId)`. Her paketten **önce** `ActionExecutor` `0` yazar.

`BotSession.cpp`:

- Kurucu başlatıcı listesine (başlıktaki bildirim sırasına uygun; `-Wreorder`): `m_castPhase(CAST_IDLE), m_castSkillId(0), m_castLeft(0), m_castCycle(0), m_castDone(0), m_castPackets(0), m_castAnyHas(false), m_castSelfId(-1), m_castEcho(0)`; gövdede `for (int i = 0; i < 8; i++) m_castTypeHas[i] = false;`. (`m_castTargetName`, `m_castSkillLast` varsayılan kurucu; `time_point`'ler başlatılmaz, `m_castTypeHas`/`m_castAnyHas` önce kontrol edilir.)
- `ResetForRespawn()`: tüm cast üyelerini sıfırla (`m_castPhase = CAST_IDLE`, `m_castTargetName.clear()`, `m_castSkillLast.clear()`, `m_castTypeHas[*] = false`, `m_castAnyHas = false`, sayaçlar 0, `m_castSelfId = -1`, `m_castEcho = 0`).
- `OnPacket()` (hâlâ **yalnızca atomik** dokunur): `opcode == WIZ_MAGIC_PROCESS && pkt.size() >= 17` iken yük düzeni `u8 opcode@0, u32 skill@1, i16 caster@5, i16 target@7, i16 sData[0]@9, [1]@11, [2]@13, [3]@15` (`MagicInstance.cpp:730-760`); `caster == m_castSelfId.load()` ve `1 <= op <= 4` ise `m_castEcho`'ya yukarıdaki kodlamayla yaz. `read<T>(pos)` taşan okumada 0 döner (`shared/ByteBuffer.h:110-116`); yine de `size()` denetimini koru.

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

`ActionExecutor.h`'ye `AttackOutcome`'un yanına:

```cpp
// Caller-supplied view of the cast target (ADR-0017 Ek F4-03). Temporary, like AttackTarget: the /bot cast test
// driver fills it from the target bot's session (or from the caster itself for "self").
struct CastTarget
{
	int16 id;      // target's id (WIZ_MAGIC_PROCESS 'target')
	float x;       // metres
	float y;
	float z;
	bool isSelf;
};

struct CastOutcome
{
	enum Kind { NOTHING, SENT, FINISHED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "ok", "casting", "effected", "missed", "srv_fail", "no_result",
	                       // "not_in_game", "dead", "bad_skill", "unsupported_skill", "bad_target",
	                       // "out_of_range", "not_standing", "no_mana", "recast", "type_gate", "gap", "rate", "too_early"
};
```

ve `ActionExecutor` sınıfına:

```cpp
	// Validates and arms a cast series of 'count' cycles of skill 'skillId'; sends nothing yet (the same Tick()'s
	// TickCast() does). 'targetName' empty = self. REFUSED (nothing armed): "not_in_game", "dead",
	// "bad_skill" (unknown id, other class, level too low, count < 1), "unsupported_skill" (see 5.4 rules),
	// "bad_target" (moral does not match the target kind).
	static CastOutcome BeginCast(BotSession * s, uint32 skillId, const std::string & targetName, uint32 count,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session; NOTHING unless s->m_castPhase != CAST_IDLE and the timing
	// rules (CastWaitMs / CastDurationMs) allow the next packet. Sends at most one WIZ_MAGIC_PROCESS.
	// SENT: a packet went out and the series continues ("casting" = CASTING onayı, "effected"/"missed"/"srv_fail" = bir
	// çevrim bitti). FINISHED: last cycle done. REFUSED: guard rejected, series dropped. FAILED: handler produced no
	// result / dead caster / unknown skill, series dropped.
	static CastOutcome TickCast(BotSession * s, const CastTarget & target,
		std::chrono::steady_clock::time_point now);

	// Clears the cast state without sending anything (stop, despawn, target lost). Keeps the reuse timers.
	static void EndCast(BotSession * s);
```

(`#include <string>` zaten var.) Kod yorumları İngilizce olacak (yukarıdaki Türkçe parantez yalnızca plan içindir).

`ActionExecutor.cpp` ek `#include`'ları (derleyici isterse): `"../GameServerDlg.h"`, `"../MagicInstance.h"` (`MORAL_*`, `m_MagictableArray`); yalnızca gerekeni ekle.

**`BeginCast` doğrulamaları** (sırayla): oturum/`isInGame` → `not_in_game`; `isDead()` → `dead`; `count < 1` → `bad_skill`; `_MAGIC_TABLE * m = g_pMain->m_MagictableArray.GetData(skillId)`, `m == nullptr` → `bad_skill`; `m->sSkill != 0 && (user->m_sClass != m->sSkill / 10 || user->GetLevel() < m->sSkillLevel)` → `bad_skill`; **desteklenmeyen**: `m->bType[0]` 1 veya 3 değilse, `m->bType[1] != 0`, `m->bFlyingEffect != 0`, `m->iUseItem != 0`, `m->sEtc != 0`, ya da `bMoral` 1, 2, 7, 8 dışında → `unsupported_skill`; **hedef uyumu**: `m->bMoral == MORAL_SELF` iken `targetName` boş değilse, `MORAL_ENEMY` iken boşsa → `bad_target` (`MORAL_FRIEND_WITHME`, `MORAL_ALL` ikisini de kabul eder). Başarılıysa: `m_castPhase = CAST_ARMED`, `m_castSkillId`, `m_castTargetName`, `m_castLeft = count`, `m_castCycle = 0`, `m_castDone = 0`, `m_castPackets = 0`, `m_castSelfId = user->GetID()`; zaten bir seri varsa yenisi eskisinin yerini alır (sessizce). Dönüş `SENT`, `"ok"`. Yeniden kullanım zamanlayıcılarına (`m_castSkillLast`, `m_castTypeLast`, `m_castAnyLast`) **dokunma** (seriler arası da geçerli).

**`TickCast` akışı:**

1. **Ön koşullar:** `m_castPhase == CAST_IDLE` → `NOTHING`. `user == nullptr || !user->isInGame()` → `EndCast`, `NOTHING`. `user->isDead()` → `EndCast`, `FAILED`, `"dead"`. Tablo satırı yine alınır; `nullptr` ise `EndCast`, `FAILED`, `"bad_skill"`.
2. **Girdiler:** `dx/dz` ile `meters = sqrt(dx*dx + dz*dz)` (self için 0), `distanceField = DistanceField(meters)`, silah menzili `AttackRangeField(weapon != nullptr, weapon ? weapon->m_sRange : 0)` (F4-02'deki `GetItemPrototype(RIGHTHAND)` yolu), `nowMs` (F4-02 ile aynı), `inWindow = m_actionWindow.CountInWindow(nowMs)`. `typeGated`: `m->iNum < 400000` ve `bType[0]` 1..7; `sinceTypeLast`: `m_castTypeHas[bType[0]]` ise `now - m_castTypeLast[bType[0]]` ms (tek tip yeterli, `bType[1] == 0` kuralı sayesinde).
3. **Faz `CAST_ARMED`** (yeni bir çevrim başlıyor): `CastStartCheck c` doldur (`needsStanding = m->sUseStanding == 1`, `standing = !s->m_moveActive`, `mana = user->GetMana()`, `msp = m->sMsp`, `reCastMs = CastRecastMs(m->sReCastTime)`, `hasSkillLast/sinceSkillLastMs` `m_castSkillLast`'tan, `hasAnyLast/sinceAnyLastMs` `m_castAnyHas/m_castAnyLast`'tan). **Bekleme (guard'dan önce, `NOTHING`):** `CastWaitMs(c) > 0` ise bu tick'in sırası değil (guard reddi **değildir**). Sonra `CheckCastStart(c)`; `CAST_OK` değilse paketi **gönderme**, `FAIRNESS_REJECT` (`"type":"Cast"`; bkz. kural tablosu), `EndCast`, `REFUSED` + `reason`. `CAST_OK` ise `m_castCycle++` ve:
   - `m->bCastTime > 0` → **CASTING paketi** gönder (aşağıdaki gönderim yordamı, `opcode = MAGIC_CASTING`, `type` metni `"CastStart"`, `sData` hepsi 0). Sonuç `casting` ise `m_castPhase = CAST_CASTING; m_castCastingAt = now;` → `SENT`, `"casting"`. Sonuç `srv_fail` veya `no_result` ise `EndCast` ve `FAILED` (reason aynı). **Aynı tick'te EFFECTING gönderilmez.**
   - `m->bCastTime == 0` → doğrudan EFFECTING (aşağıda, `sinceCastingMs = 0`).
4. **Faz `CAST_CASTING`:** `now - m_castCastingAt < CastDurationMs(m->bCastTime)` ise `NOTHING`; değilse EFFECTING (aşağıda, `sinceCastingMs = now - m_castCastingAt`).
5. **EFFECTING gönderimi:** `inRange = CastInRange(meters, m->sRange, distanceField, weaponRangeField)`; `CheckCastEffect(inRange, sinceCastingMs, m->bCastTime, inWindow)`; `CAST_OK` değilse `FAIRNESS_REJECT`, `EndCast`, `REFUSED`. Paket: `opcode = MAGIC_EFFECTING`, `type` metni `"CastEffect"`; `sData[0..2]`: hedef `isSelf` değilse `int16(target.x)`, `int16(target.y)`, `int16(target.z)` (metre, kesme; birimi `[A]`: tek hedefli Type1/Type3'te sunucu `sData[0..2]`'yi okumaz, `docs/03` §14 gözlemi), `isSelf` ise 0; `sData[3..5]` 0. Sonuç eşleme (aşağıda): `effected`/`missed` → `m_castDone++`. Sonra **her sonuçta** (`no_result` dahil) yeniden kullanım zamanlayıcıları güncellenir: `m_castSkillLast[skillId] = now`, `m_castTypeHas[bType[0]] = true; m_castTypeLast[bType[0]] = now`, `m_castAnyHas = true; m_castAnyLast = now` (sunucu yalnızca başarıda zaman damgası yazar; bot muhafazakâr davranır). `m_castLeft--`. `no_result` → `EndCast`, `FAILED`, `"no_result"`. `m_castLeft == 0` → `EndCast`, `FINISHED`, reason = bu çevrimin sonucu. Aksi halde `m_castPhase = CAST_ARMED`, `SENT`, reason = sonuç (`srv_fail`/`missed` seriyi **düşürmez**; çevrim sayısı sonlandırır).

**Gönderim yordamı** (CASTING ve EFFECTING için ortak, dosya-yerel yardımcı): `decisionId = NextDecisionId(s)`; `ACTION_SUBMIT` (yalnızca `IsEnabled(TEL_DECISIONS)`; paketten hemen önce) — CastStart: `"decision_id":N,"type":"CastStart","skill":<id>,"target":<tid>,"cycle":<m_castCycle>,"cast_ms":<CastDurationMs>`; CastEffect: `"decision_id":N,"type":"CastEffect","skill":<id>,"target":<tid>,"cycle":<m_castCycle>,"since_casting_ms":<sinceCastingMs>`. Paket:

```cpp
	Packet pkt(WIZ_MAGIC_PROCESS);
	pkt << uint8(opcode) << uint32(skillId) << int16(user->GetID()) << int16(target.id)
		<< int16(sData[0]) << int16(sData[1]) << int16(sData[2]) << int16(sData[3]) << int16(sData[4]) << int16(sData[5]);
```

(21 bayt: gerçek istemci düzeni; sunucu 7. alanı `0` okur. `MagicPacket` `sCasterID != pCaster->GetID()` ise sessizce döndüğünden **caster alanı `user->GetID()` olmalıdır**; `GetID`/`GetSocketID` farklı olabilir, uygulayıcı önce `Unit.h`'de doğrular ve F4-02'nin `GetSocketID()` kullanımıyla çelişirse raporlar.) Sonra: `s->m_castEcho = 0; t0; user->HandlePacket(pkt); t1; s->m_castPackets++; m_actionWindow.Record(nowMs);` (CASTING ve EFFECTING ayrı aksiyon). `uint64 echo = s->m_castEcho.load()`. **Sonuç eşleme** (yalnızca yayınlanan paket; hedefin HP'sine/`GetHealth()`'ine/`GetMana()` farkına bakılmaz):

| Gönderilen | `echo` (bit 63 set ve `skillId` eşit) | `ok` | `reason` |
|---|---|---|---|
| CASTING | opcode 1 | true | `casting` |
| CASTING | opcode 4 (`MAGIC_FAIL`) | false | `srv_fail` |
| EFFECTING | opcode 3, `sData[3] == 0` | true | `effected` |
| EFFECTING | opcode 3, `sData[3] == -104` (`SKILLMAGIC_FAIL_ATTACKZERO`, Type1 hasarı 0) | true | `missed` |
| EFFECTING | opcode 4 | false | `srv_fail` |
| herhangi | `echo == 0`, beklenmeyen opcode (CASTING'e 3, EFFECTING'e 1) veya başka `skillId` | false | `no_result` |

(`ok` yalnızca "sunucu bu aksiyonu kabul edip yayınladı" demektir; `missed` hasarın 0 çıktığını söyler, MET-ACT-02 paydasında `srv_fail` sayılmaz.) `ACTION_RESULT`: `"decision_id":N,"type":"CastStart"|"CastEffect","ok":true|false,"reason":"<reason>","op":<echo opcode veya -1>,"code":<sData[3] veya 0>,"latency_us":%lld`. Her `TickCast`'ta **en çok bir** paket gider.

**`FAIRNESS_REJECT` kural tablosu** (`EmitFairnessReject(..., "Cast", rule, reason, value, limit)`; `reason` metni `CastOutcome.reason` ile aynı):

| Verdict | rule | reason | value | limit |
|---|---|---|---|---|
| `OUT_OF_RANGE` | `MEC-MAG-11` | `out_of_range` | `skillRange > 0` ise `meters`, değilse `distanceField` | `skillRange > 0` ise `skillRange`, değilse `weaponRangeField` |
| `NOT_STANDING` | `CLI-09` | `not_standing` | 1 | 0 |
| `NO_MANA` | `MEC-MAG-08` | `no_mana` | `mana` | `msp` |
| `RECAST` | `CLI-04` | `recast` | `sinceSkillLastMs` | `reCastMs` |
| `TYPE_GATE` | `MEC-MAG-03` | `type_gate` | `sinceTypeLastMs` | `kTypeGateMs` |
| `GAP` | `CLI-04` | `gap` | `sinceAnyLastMs` | `kCastGapMs` |
| `RATE` | `CLI-11` | `rate` | `inWindow` | `kMaxActionsPerWindow` |
| `TOO_EARLY` | `CLI-03` | `too_early` | `sinceCastingMs` | `CastDurationMs(bCastTime)` |

**`EndCast`:** `m_castPhase = CAST_IDLE; m_castLeft = 0;` (`m_castCycle`, `m_castDone`, `m_castPackets` ve yeniden kullanım zamanlayıcıları korunur: log satırı sayıları okur, bir sonraki `BeginCast` sayıları sıfırlar). `m_castSelfId` korunur.

Thread: yalnızca IOCP thread'inde; kilit, `Sleep`, `printf`, `rand` yok; `std::ostringstream`/`FormatFixed` deseni. Konsola/log dosyasına yazma yok (günlüğü `BotManager.cpp` yazar). `ActionExecutor`, `Unit`/`CUser` durumunu **doğrudan değiştirmez** (MP, HP, `m_CoolDownList`, `m_MagicTypeCooldownList`, `m_sSpeed` yazılmaz; sunucu kendi kayıtlarını `HandlePacket` içinde tutar).

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirim: `void CommandCast(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()` (`:591`):** `attack` dalının yanına `cast` fiilini ekle; "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **`CommandCast(args)`:** `SplitWords` ile ayrıştır; `CommandAttack` kalıbını izle (`FindSession`, `IsKnownBotName`, `ParseIntStrict`, `PhaseName`). Kullanım satırı (hata durumlarında **tek** günlük satırı ve dön): `BotManager: cmd cast: usage: cast <bot> <skill id> <target bot|self> [cycles] | cast <bot|all> off`.
   - **`off`:** `words.size() == 2 && _stricmp(words[1], "off") == 0`. `all` ise her `PHASE_IN_GAME` oturum için, değilse adı verilen oturum için (aynı `unknown or not spawned bot '<ad|?>'` / `not in game (phase X)` iletileri, ön ek `cmd cast:`): seri aktifse (`m_castPhase != CAST_IDLE`) `EndCast` ve `cmd cast: <bot> stopped after <m_castPackets> packet(s) sent`, değilse `cmd cast: <bot> not casting`. `all` için özet satırı: `cmd cast all: N stopped, M not casting`.
   - **Başlatma:** `words.size() == 3 || 4`. Saldıran: `FindSession(words[0])`; yok → `cmd cast: unknown or not spawned bot '<ad|?>'`; `PHASE_IN_GAME` değil → `cmd cast: <bot> not in game (phase X)`. Skill: `ParseIntStrict(words[1], v)`, `v < 1 || v > 2147483647` → kullanım satırı. Hedef: `words[2]` `self` (büyük/küçük harf fark etmez) ise `targetName` boş; değilse `FindSession(words[2])`: yok → `unknown or not spawned bot '<ad|?>'`; `PHASE_IN_GAME` değil → `cmd cast: target <bot> not in game (phase X)`; hedef == kendisi → `cmd cast: <bot> refused (bad_target)`; aksi halde `targetName = hedef->m_charName`. Çevrim: `words.size() == 4` ise `ParseIntStrict`, `1..20` dışı → kullanım satırı; verilmezse 1. Sonra `ActionExecutor::BeginCast(s, (uint32)v, targetName, (uint32)cycles, now)`: `REFUSED` → `cmd cast: <bot> refused (<reason>)`; aksi halde `cmd cast: <bot> casting <skill> on <hedef|self> (<n> cycle(s))`.
3. **`TickSessions()` `PHASE_IN_GAME` dalı (`:1584-1634` saldırı bloğundan sonra, aynı `if (s->m_phase == PHASE_IN_GAME)` içinde):** `if (s->m_castPhase != BotSession::CAST_IDLE)`:
   - `s->m_pUser->isDead()` → `EndCast(s)` + `bot <ad> cast stopped (dead)` günlüğü.
   - Aksi halde hedef görünümü (**test sürücüsü**, yorumda belirt): `m_castTargetName` boşsa `CastTarget tv = { (int16)s->m_pUser->GetID(), x, y, z, true }` (kendi konumu); doluysa `BotSession * t = FindSession(...)`; `t == nullptr || t->m_phase != PHASE_IN_GAME || t->m_pUser == nullptr` → `EndCast(s)` + `bot <ad> cast stopped (target_lost)`; aksi halde `tv = { (int16)t->m_pUser->GetID(), t->m_pUser->GetX(), t->m_pUser->GetY(), t->m_pUser->GetZ(), false }`. Sonra `CastOutcome o = ActionExecutor::TickCast(s, tv, now)`:
     - `FINISHED` → `bot <ad> cast finished (<reason>) after <m_castCycle> cycle(s), <m_castDone> ok, <m_castPackets> packet(s) sent`;
     - `REFUSED` / `FAILED` → `bot <ad> cast stopped (<reason>)`;
     - `SENT` / `NOTHING` → günlük yok (spam yok; ayrıntı telemetride).
4. **`BeginDespawn()` (`:1697`):** `ActionExecutor::EndAttack(s)` satırının yanına `ActionExecutor::EndCast(s)`.
5. **`BuildStatusLines()` (`:777-822`):** oturum satırının sonuna `casting=<0|1> mp=<cur>/<max>`: `casting = m_castPhase != CAST_IDLE`; `mp` yalnızca `s->m_pUser != nullptr` iken `%d/%d` (`GetMana()`/`GetMaxMana()`), değilse `-`. Mevcut alanların sırası ve biçimi değişmez; yeni alanlar `attacking=` sonrasına eklenir. `char message[192]` kesilmeye başlarsa 256'ya çıkar (`list` ile çıktının kesilmediğini gör).
6. Başka hiçbir yere dokunma (`ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj`, `.filters`, `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` **değişmez** (yeni dosya yok). Mevcut dosyaların kodlama/satır sonu/BOM durumu korunur.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı beş yeni `Combat_Cast*` testini içerir ve toplam test sayısı **24**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; dosyada `#include` yalnızca `<algorithm>` ve `<cstdint>`.
- [ ] K5: `WIZ_MAGIC_PROCESS` yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_MAGIC_PROCESS" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma) ve `BotSession.cpp` (`OnPacket` sonuç okuma) satırlarını gösterir; `grep -n "MagicPacket\|MagicInstance\|m_CoolDownList\|m_MagicTypeCooldownList\|MSpChange\|HpChange" GameServer/Bot/*.cpp GameServer/Bot/*.h` bot kodunda `MagicPacket`/`MagicInstance` çağrısı, soğuma listesi veya MP/HP yazımı göstermez (yalnızca `#include "../MagicInstance.h"` olabilir).
- [ ] K6: `ActionExecutor`'da guard atlanmıyor: `TickCast`'ta her `HandlePacket` çağrısından önce `CheckCastStart` (CASTING ve süresiz EFFECTING için) veya `CheckCastEffect` (EFFECTING için) çağrısı ve `CAST_OK` dışında erken dönüş vardır (kod okumasıyla; Claude çalışma zamanında da sınar). `HandlePacket(pkt)` çağrısı `WIZ_MAGIC_PROCESS` için tek yerde (ortak gönderim yordamı).
- [ ] K7: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutlarla çalışır; `git diff gece/2026-10-02...bot/F4-03 -- GameServer/Bot/BotManager.cpp | grep '^-'` yalnızca bilinçli değiştirilen satırları gösterir (`unknown command` mesajı, `BuildStatusLines` biçim satırı ve tampon boyutu); `Startup()`/`Tick()` akışı ve `ini` okuma değişmedi; `OnPacket()` dışındaki `BotSession` mantığı değişmedi.
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F4-03` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş.
- [ ] K9: değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K10: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K11: F4-01/F4-02 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack" GameServer/Bot/ActionExecutor.cpp` ≥ 1; `Motion_*` ve `Combat_Attack*`/`Combat_Guard*`/`Combat_RateWindow` testleri hâlâ geçiyor; `EmitFairnessReject`'in hareket ve saldırı çağrıları `"Move"`/`"Attack"` geçiyor.
- [ ] K12 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-03
git diff gece/2026-10-02...bot/F4-03 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-03 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "WIZ_MAGIC_PROCESS" GameServer/Bot/*.cpp
grep -n "MagicPacket\|MagicInstance\|m_CoolDownList\|m_MagicTypeCooldownList\|MSpChange\|HpChange" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "CheckCastStart\|CheckCastEffect\|HandlePacket" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-03
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`). Botlar: Karus mage `BotMF_K` (110518 Ignition), Karus warrior `BotWP_K` (106560 sword dancing), El Morad warrior `BotWP_E` (düşman hedef), El Morad priest `BotPHD_E` (212545 Superior healing, `self`); zone 71, farklı ulus + PVP bölgesi → düşman. Menzil 56 m (Ignition) / silah menzili (sword dancing) olduğundan Type1 testinde botlar `move` ile yan yana getirilir (`list` `pos=` ile teyit; mesafe ≤ silah menzili). `MAGIC` satırları `SELECT` ile teyit edilir (yasak tablolar **okunmaz**). `Cast` gözlemi ölçümü `Logs/bots/<tarih>/live-*.jsonl`'den, telemetri `decisions` ile yapılır. Beklenmeyen `srv_fail`/`no_result` bu planın hatası değil **bulgudur**: doğrulayıcı nedeni (menzil, sınıf/seviye, düşmanlık, `UNIXTIME` saniye kapısı) çözümleyip raporlar.

1. **Cast süreli Type3 (mutlu yol):** `spawn BotMF_K,BotWP_E`; `cast BotMF_K 110518 BotWP_E 2` → log `casting 110518 on BotWP_E (2 cycle(s))`; JSONL: her çevrimde sırayla `ACTION_SUBMIT CastStart` (`cast_ms` = 1080) → `ACTION_RESULT` (`reason:"casting"`, `op:1`) → `ACTION_SUBMIT CastEffect` → `ACTION_RESULT` (`reason:"effected"`, `op:3`, `code:0`); `CastStart` → `CastEffect` `t` farkı ≥ 1080 ms ve ≤ 1080 + 250 ms (tick ≈ 110 ms); ikinci çevrimin `CastStart`'ı birinci `CastEffect`'ten ≥ 1000 ms sonra (tip kapısı) ve ≥ 140 ms; `list`: `BotWP_E` `hp` düştü, `BotMF_K` `mp` azaldı (2 × 60), `casting=0`; `cast finished (effected) after 2 cycle(s), 2 ok, 4 packet(s) sent`; `FAIRNESS_REJECT` yok; `latency_us` < 5000.
2. **Type1 (cast süresiz):** `BotWP_K` ile `cast BotWP_K 106560 BotWP_E 2` (botlar yan yana) → yalnızca `CastEffect` paketleri (`CastStart` yok); iki `EFFECTING` arası ≥ 1000 ms (tip kapısı; `ReCastTime = 5` 500 ms'dir, 1000 ms baskın); `reason` `effected` veya `missed` (`code:-104`); `list`: `mp` 2 × 300 azaldı (MEC-MAG-08), `BotWP_E` `hp` düştü ya da `missed`.
3. **Kendine heal:** `cast BotPHD_E 212545 self` → `CastStart` (`target` = botun kendi kimliği, `cast_ms` 1580) → `CastEffect` (`reason:"effected"`), `BotPHD_E` `hp` ≤ maks (değişmeyebilir, sonuç yalnızca paketten okunur); `cast BotPHD_E 212545 BotWP_E` → `refused`/`srv_fail` davranışı Moral 2 kuralına göre (dost olmayana heal: `IsAvailable` ret; doğrulayıcı sonucu raporlar).
4. **Guard reddi (sunucuya paket gitmez):** botlar > 56 m uzakta iken `cast BotMF_K 110518 BotWP_E` → `cast stopped (out_of_range)`; JSONL'de yalnızca `FAIRNESS_REJECT` (`"type":"Cast"`, `"rule":"MEC-MAG-11"`, `value` ≥ `limit` 56), **`ACTION_SUBMIT` yok**, `BotMF_K` oturumu kopmaz (`GameServer.log`'da yeni satır yok). Ardışık iki komut: ilk `cast` biter bitmez 200 ms içinde ikinci `cast BotWP_K 106560 …` verilince (aynı skill, tip kapısı) çevrim `CastWaitMs` ile **bekletilir**, `FAIRNESS_REJECT` **yazılmaz**, ikinci `EFFECTING` ≥ 1000 ms sonra gider. `UseStanding = 1` skill yoksa (SELECT ile arama) `not_standing` yalnızca birim testindedir; raporda yaz.
5. **Reddedilen komutlar:** `cast BotMF_K 999999 BotWP_E` → `refused (bad_skill)`; `cast BotWP_K 110518 BotWP_E` (başka sınıf) → `refused (bad_skill)`; `cast BotMF_K 110533 BotWP_E` (uçan alan) → `refused (unsupported_skill)`; `cast BotMF_K 110518 self` (Moral 7, kendine) → `refused (bad_target)`; `cast BotMF_K 110518 BotMF_K` → `refused (bad_target)`; `cast Ghost 110518 BotWP_E` ve `cast BotMF_K 110518 Nobody` → tek satır `unknown or not spawned bot '?'`; `cast BotMF_K 110518 BotWP_E 0`, `... 21`, `... abc`, argümansız → kullanım satırı; `despawn` edilmiş botla `not in game (phase despawned)`; hedef despawn edilmiş → `target ... not in game`; `cast BotMF_K off` ortada (CASTING sonrası) → `stopped after N packet(s) sent` ve **sonra yeni paket yok** (EFFECTING gitmez); `cast all off` hareketsiz botlarda `not casting`, özet `0 stopped, N not casting`; `RESPAWN_CYCLES=2` iken `cast` → `cmd rejected`.
6. **Ömür döngüsü:** yürütülen cast sırasında hedef `despawn` → `cast stopped (target_lost)`; caster `despawn` → temiz despawn (`despawn complete`, `pool free` tam), despawn sonrası yeni paket yok; yeniden `spawn` → `casting=0`, `mp` DB'deki son değer (bot satırları logout ile kaydedilir: doğrulama sonunda `db/002` ile `Hp=Mp=32000` geri yüklenir).
7. **Gerilemesiz:** `attack BotWP_K BotWP_E 3` aynı çıktıyı verir (F4-02: `3 ok`, aralık ≈ silah `Delay×10`); `move` 30 m yürüyüşü `5 packets`; `cast` ve `attack` birlikte (aynı bot, CLI-11 penceresi ortak) çalışır ve `FAIRNESS_REJECT rate` **yazılmaz** (≤ 6/sn); `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama cast çalışır; `ENABLED=0` → komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` F4-02 düzeyinde (≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. İnsan istemcisi gerekmez (görsel doğrulama `T-ARCH-08`, `docs/STATUS.md` "Proje sahibi testleri").

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). `cast` yalnızca `ENABLED=1` iken, üretim dışı test komutudur.
- **Thread kuralı (ADR-0005):** `ActionExecutor` yalnızca IOCP thread'inde (`Tick()` içinden) çalışır; `HandlePacket` zaten bu thread'de koşar. Konsol/`+bot` işleyicisi `BotSession`/`ActionExecutor`'a **dokunmaz**; komutlar `EnqueueCommand` kuyruğundan gelir. `OnPacket()` her thread'den çağrılabilir: orada yalnızca atomik alanlara yaz (`m_castEcho`; `m_castSelfId`'i yalnızca oku); `std::string`/`std::map`/`std::chrono` üyelere dokunma.
- **Sonuç yalnızca yayınlanan sonuç paketinden okunur** (AC-LRN-03 / `docs/13` §8). Hedefin `GetHealth()`'i, hasar, kendi MP'sinin değişimi aksiyon sonucunu belirlemek için **kullanılmaz**. Guard girdisi olan `GetMana()` botun **kendi** durumudur (istemci de MP'sini görür); `BuildStatusLines`'taki `hp=`/`mp=` yalnızca operatör çıktısıdır.
- **Test sürücüsü sınırı:** `TickSessions()` hedef konumunu hedef botun `CUser`'ından okur; bu üretim algısı değildir, `Perception` dilimi gelince (ADR-0017) yerini alır. Bu okumayı `ActionExecutor` içine **taşıma** (`CastTarget` yapısı arayüzdür).
- **`UNIXTIME` 1 sn çözünürlüklüdür** (MEC-MAG-10): sunucu soğumaları tam saniye farkıyla karşılaştırır; bot gerçek zamanla ≥ `ReCastTime × 100 ms` ve ≥ 1000 ms (tip kapısı) bekler, bu sunucu kapısını daima geçer. Tip kapısını 1000 ms'nin altına indirecek bir yol ekleme.
- **Skill verisi** (`m_MagictableArray`) istemcinin de bildiği statik veridir; hedefin buff/HP durumu okunmaz. Sınıf/seviye kontrolü sunucunun `UserCanCast` kuralının aynasıdır (bot hiçbir zaman başka sınıfın skill'ini göndermez).
- **Bilinen sınırlar `[A]`:** `sData[0..2]` birimi/dizilimi (tek hedefli skill'de sunucu okumaz); `CastTime × 100 + 80 ms` tek sınıf/skill ölçümüne (Ignition, Superior healing) dayanır; iptal paketi yok; `UseStanding` otomatik durdurma yok; MEC-MAG-04 istisnası uygulanmaz (heal tekrarında bot gereğinden yavaş kalabilir, adil yönde hata); tick (≈ 110 ms) nedeniyle `CASTING`→`EFFECTING` aralığı alt sınırdan 0–110 ms geç olabilir.
- Telemetri hacmi küçüktür (cast başına ≤ 4 olay); `droppable = false`, `IsEnabled` denetimi hareket/saldırıyla aynı.
- `list` satırı biçimi (`attacking=` sonrası yeni alanlar) F3-04 yanıtını da etkiler; mevcut alan adlarını ve sırasını **değiştirme**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI (doğrulamaya hazır)
- Branch / commit'ler: `bot/F4-03` (taban: `gece/2026-10-02`), tek commit `c4c8c49` — `[F4-03] cast dilimi: CastStart/CastEffect (Type1/Type3) + CLI-03/04/09/11 guard`
- Değişen dosyalar ve neden:
  - `BotCore/BotCombat.h`: §5.2 eklemeleri (sabitler, `CastDurationMs`, `CastRecastMs`, `CastInRange`, `CastStartCheck`, `CastVerdict`, `CastWaitMs`, `CheckCastStart`, `CheckCastEffect`).
  - `Tests/BotCoreTests/CombatTests.cpp`: beş yeni `TEST_CASE` + `OkCast()` yardımcısı.
  - `GameServer/Bot/ActionExecutor.h`: `CastTarget`, `CastOutcome`, `BeginCast`/`TickCast`/`EndCast` bildirimleri.
  - `GameServer/Bot/ActionExecutor.cpp`: cast yolu (`SubmitCast`, `RejectCast`, `BeginCast`, `TickCast`, `EndCast`) + `GameServerDlg.h`/`MagicInstance.h`/`<cstring>` include'ları.
  - `GameServer/Bot/BotSession.h/.cpp`: cast durumu üyeleri, `#include <map>`, kurucu/`ResetForRespawn` sıfırlamaları, `OnPacket()` içinde `WIZ_MAGIC_PROCESS` sonuç okuma.
  - `GameServer/Bot/BotManager.h/.cpp`: `CommandCast`, fiil dağıtımı, `TickSessions()` cast bloğu, `BeginDespawn()` `EndCast`, `BuildStatusLines()` `casting=`/`mp=`.
- Derleme sonucu:
  - `./tools/build.sh Release`: rc=0; `proj-GameServer.vcxproj -> ...GameServer.exe`; yeni dosyalarda uyarı yok (kalan uyarılar eski satırlar: `GameServerDlg.cpp:816/1143/1802`, `UpgradeHandler.cpp:634/862`).
  - `./tools/build.sh Debug`: rc=0; `GameServer.exe` üretildi; aynı eski uyarılar.
  - `./tools/run-tests.sh Release`: `24 tests, 0 failed`; beş yeni `Combat_Cast*` testi görünüyor.
  - `./tools/run-tests.sh Debug`: `24 tests, 0 failed`.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ (Release hatasız; değişen dosyalarda yeni uyarı yok).
  - K2 ✔ (Debug hatasız).
  - K3 ✔ (Release ve Debug `24 tests, 0 failed`).
  - K4 ✔ (`BotCombat.h`'de sunucu başlığı yok; include yalnızca `<algorithm>`, `<cstdint>`).
  - K5 ✔ (`WIZ_MAGIC_PROCESS` yalnızca `ActionExecutor.cpp` üretimi + `BotSession.cpp` okuma; `MagicPacket`/`MagicInstance` çağrısı, soğuma listesi, MP/HP yazımı yok; yalnızca `#include "../MagicInstance.h"`).
  - K6 ✔ (`HandlePacket` cast çağrısı tek yerde (`SubmitCast`); `CheckCastStart` CAST_ARMED'da, `CheckCastEffect` EFFECTING'de, `CAST_OK` dışında erken dönüş).
  - K7 ✔ (`-` satırları yalnızca bilinçli: unknown command metni, `char message[192]`→`[256]`, `BuildStatusLines` biçim satırı; `Startup`/`Tick` akışı ve ini okuma değişmedi).
  - K8 ✔ (`git diff --stat`: §4'teki 8 dosya + plan; `vcxproj*` değişmemesi doğrulandı).
  - K9 ✔ (`file` çıktıları tümü ASCII + CRLF; `git diff --check` temiz).
  - K10 ✔ (`ActionExecutor.*` içinde `printf`/`Sleep`/`lock_guard`/`mutex`/`CreateThread`/`rand(` yok).
  - K11 ✔ (`CheckMoveStep` ×2, `CheckAttack` ×1; `Move`/`Attack`/`Cast` `EmitFairnessReject` çağrıları; Motion/Combat testleri geçiyor).
  - K12 (çalışma zamanı) — DeepSeek yapmaz, Claude yapar.
- Plandan sapmalar ve gerekçeleri:
  - `CommandCast`'te hedef adı çözümlendikten sonra kendine-cast denetimi için `target == s` erken reddi korundu (planla uyumlu).
  - `TickCast`'ta `FINISHED`/`FAILED` dönüşlerinde `reason` olarak `CastOutcome.reason` kullanıldı (`SubmitCast` zaten `const char*` sabit döndürür); plandaki tabloyla birebir.
  - Yok: `recast`/`type_gate`/`gap` durumları `CastWaitMs > 0` iken `NOTHING` döner (plan §5.4 madde 3 ile uyumlu), guard reddi yalnızca gerçek ihlalde yazılır.
- Açık sorular: yok.

### Tur 2

- Durum: UYGULANDI (doğrulamaya hazır)
- Branch / commit'ler: `bot/F4-03` (taban: `gece/2026-10-02`); önceki turlar korundu, düzeltme commit'i Tur 2 raporuyla birlikte atılır.
- Düzeltme talimatına uyum: Doğrulama Turu 1 bulgusu 1 (Orta) giderildi. `GameServer/Bot/ActionExecutor.cpp` `RejectCast()` içindeki `switch (verdict)`'e `CAST_REJECT_OUT_OF_RANGE` için `default`'tan önce `case` eklendi: `rule = "MEC-MAG-11"`, `reason = "out_of_range"`, `value = (c.skillRange > 0) ? c.distanceM : (float)c.distanceField`, `limit = (c.skillRange > 0) ? (float)c.skillRange : (float)c.weaponRangeField`. Başka verdict'e ve başka fonksiyona dokunulmadı; başlangıç kural/sebep metinleri değişmedi. Tek satırlık İngilizce yorum eklendi.
- Değişen dosyalar ve neden:
  - `GameServer/Bot/ActionExecutor.cpp` (+4 satır, yalnızca ekleme): `CAST_REJECT_OUT_OF_RANGE` case'i + İngilizce yorum. Başka dosya değişmedi (`docs/**`, `BotCombat.h`, testler, `GameServer/**` dahil).
- Derleme ve test sonuçları:
  - `./tools/build.sh Release`: rc=0; `ActionExecutor.cpp` yeniden derlendi, bu dosya için uyarı/hata yok (`GameServer.exe` üretildi).
  - `./tools/build.sh Debug`: rc=0; `ActionExecutor.cpp` yeniden derlendi, bu dosya için uyarı/hata yok (`GameServer.exe` üretildi).
  - `./tools/run-tests.sh Release`: rc=0; `24 tests, 0 failed`.
  - `./tools/run-tests.sh Debug`: rc=0; `24 tests, 0 failed`.
  - `git diff --stat`: yalnızca `GameServer/Bot/ActionExecutor.cpp` (+4) ve bu plan dosyası (`Durum` satırı); `file GameServer/Bot/ActionExecutor.cpp` → `ASCII text, with CRLF`; `git diff --check` boş.
- Kabul kriterleri öz-değerlendirme (düzeltme kapsamı): bulgu 1 `value`/`limit` artık plan §5.4 tablosu ve §7 S4 beklentisini karşılar (`skillRange > 0` ise metre, aksi halde `distanceField` / `weaponRangeField`). K1/K2/K3 yeniden karşılandı (rc=0, uyarı yok, 24/24). Diğer kriterler (K4–K11) bu değişiklikten etkilenmez; K12 çalışma zamanı doğrulaması Claude'a aittir.
- Plandan sapmalar: yok (talimat birebir uygulandı; talimattaki "tek satır" yorum koda eklendi).
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: DÜZELTME GEREKLİ
- İncelenen: `gece/2026-10-02...bot/F4-03` @ `28011b1` (3 commit: `8f6a6ec` plan, `c4c8c49` uygulama, `28011b1` rapor; hepsi `[F4-03] ...` biçiminde, merge/force izi yok). Otonom gece modu (`AUTO_LOOP=1`): birleştirme/push yapılmadı.
- Özet: Kod planla birebir uyumlu ve 11 kriter + çalışma zamanı senaryolarının çoğu geçti; tek somut hata: guard'ın `out_of_range` reddi `FAIRNESS_REJECT` olayına `value`/`limit` değerlerini **0,00 / 0,00** yazıyor (plan §5.4 kural tablosu ve §7 S4 `value ≥ limit 56` bekliyor). Rapor kanıtı: K12 S4 ✘.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release derleme | ✔ | `./tools/build.sh Release` rc=0; `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` touch'lanıp yeniden derlendi (log'da dördü de görünüyor): `warning`/`error` satırı 0 |
| K2 Debug derleme | ✔ | `./tools/build.sh Debug` rc=0, uyarı/hata yok |
| K3 Birim testleri | ✔ | `tools/run-tests.sh Release` ve `Debug`: `24 tests, 0 failed`; `Combat_CastDuration`, `Combat_CastInRange`, `Combat_CastStart_Order`, `Combat_CastWait`, `Combat_CastEffect` görünüyor |
| K4 `BotCombat.h` saflığı | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/"` boş; `#include` yalnızca `<algorithm>`, `<cstdint>` (`BotCombat.h:6-7`) |
| K5 Paket yolu | ✔ | `WIZ_MAGIC_PROCESS`: `ActionExecutor.cpp:588` (`Packet pkt(WIZ_MAGIC_PROCESS)`) ve `BotSession.cpp:44` (`OnPacket` sonuç okuma); `MagicPacket`/`m_CoolDownList`/`m_MagicTypeCooldownList`/`MSpChange`/`HpChange` bot kodunda yok, yalnızca `ActionExecutor.cpp:7` `#include "../MagicInstance.h"` |
| K6 Guard atlanmıyor | ✔ | `HandlePacket` cast için tek yerde (`ActionExecutor.cpp:596`, `SubmitCast`); `CheckCastStart` `:841` (CAST_ARMED, CASTING ve süresiz EFFECTING için) ve `CAST_OK` değilse `RejectCast` ile erken dönüş; `CheckCastEffect` `:881` ve aynı erken dönüş; çalışma zamanında da sınandı (S4: `ACTION_SUBMIT` yok) |
| K7 `ENABLED=0` değişmez | ✔ | `git diff ... BotManager.cpp \| grep '^-'`: yalnızca `unknown command` metni, `char message[192]`→`[256]` ve `BuildStatusLines` biçim satırı (4 `-` satırı); `Startup()`/`Tick()`/ini okuma değişmedi; `BotSession` değişikliği yalnızca yeni üyeler, `ResetForRespawn()` ve `OnPacket()` içi atomik yazım. Çalışma zamanında `ENABLED=0` yeniden sınanmadı (kod yolu `IsEnabled`/`m_phase` kapılarında, bu planda değişmedi) |
| K8 Kapsam | ✔ | `git diff --stat`: §4'teki 8 dosya + plan; `proj-GameServer.vcxproj*`, `BotCore.vcxproj`, `BotCoreTests.vcxproj` farkı 0 |
| K9 Kodlama | ✔ | `file`: tüm değiştirilen dosyalar `ASCII text, with CRLF`; `git diff --check` boş |
| K10 Yasaklı çağrılar | ✔ | `grep "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand("` `ActionExecutor.*` içinde boş |
| K11 F4-01/F4-02 gerilemesiz | ✔ | `CheckMoveStep` ×2, `CheckAttack` ×1; `EmitFairnessReject` hareket (`:97`, `:190`) `"Move"`, saldırı (`:402`) `"Attack"`; `Motion_*`/`Combat_*` testleri geçiyor; çalışma zamanı: `attack ... 3` → `3 hit(s) sent, 3 ok`, aralık 1640–1650 ms, 30 m `move` → `after 5 packets` |
| K12 Çalışma zamanı | ✘ | S1–S3, S5–S7 geçti; **S4'te `FAIRNESS_REJECT out_of_range` `value`/`limit` 0,00** (bulgu 1). Ayrıntı aşağıda |

- Çalışma zamanı sınaması (Release, `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`; `BotMF_K` Karus mage, `BotWP_K` Karus warrior, `BotWP_E` El Morad warrior, `BotPHD_E` El Morad priest; zone 71; eski `Logs/bots/` `bots_old_f403`'e taşındı):
  - **S1 Type3, cast süreli (110518 Ignition, `BotMF_K` → `BotWP_E`, 2 çevrim):** `casting 110518 on BotWP_E (2 cycle(s))`, `cast finished (effected) after 2 cycle(s), 2 ok, 4 packet(s) sent`. JSONL sırası `CastStart` (`cast_ms:1080`) → `casting` (`op:1`) → `CastEffect` → `effected` (`op:3, code:0`) ×2; `CastStart`→`CastEffect` farkı 1101 / 1097 ms (aralık [1080, 1330] içinde); `CastEffect`→sonraki `CastStart` 1098 ms (tip kapısı ≥ 1000, boşluk ≥ 140; bot bekledi, `FAIRNESS_REJECT` yazılmadı); `latency_us` 9–138; `BotWP_E` `hp` 5650 → 5157, `BotMF_K` `mp` 6021 → 5941 (MP yenilenmesi nedeniyle tam 120 değil).
  - **S2 Type1, cast süresiz (106560 sword dancing, `BotWP_K` → `BotWP_E`, 2 çevrim):** yalnızca `CastEffect` (`CastStart` yok, `since_casting_ms:0`), iki `EFFECTING` arası 1094 ms (≥ 1000), `reason:"effected"`; `BotWP_K` `mp` 5370 → 4930 (2 × 300 − yenilenme).
  - **S3 heal (212545 Superior healing):** `self` → `CastStart` (`target:2987` = kendi kimliği, `cast_ms:1580`) → `CastEffect` 1641 ms sonra → `effected`; dost `BotWP_E`'ye → `effected` (aynı millet, Moral 2 kuralı sunucuda geçti), `BotWP_E` `hp` 5157 → 5650 (iyileşme).
  - **S4 guard reddi:** `BotWP_E` 70 m uzağa yürütüldü (`move ... after 11 packets`); `cast BotMF_K 110518 BotWP_E` → `cast stopped (out_of_range)`; `cast BotWP_K 106560 BotWP_E` → `cast stopped (out_of_range)`. JSONL'de yalnızca `FAIRNESS_REJECT` (`"type":"Cast"`, `"rule":"MEC-MAG-11"`, `"reason":"out_of_range"`), **`ACTION_SUBMIT` yok** ✔, oturum kopmadı, `GameServer.log` 32 → 32 satır ✔; ancak **`"value":0.00,"limit":0.00`** ✘ (beklenen: mage için ≈ 70 / 56; Type1 için `distanceField` / `weaponRangeField`). `UseStanding = 1` skill: yerel `MAGIC`'te 8 satır var (hepsi `3010xx`, `Skill = 1010` = sınıf 101); botların sınıf kodlarıyla (106/110/112/206/210/212) eşleşmediği için `bad_skill` ile reddedilir. `not_standing` yalnızca birim testindedir (planın öngördüğü durum). Tip kapısı beklemesi S1/S2'de (ikinci çevrim) `FAIRNESS_REJECT` yazmadan gözlendi.
  - **S5 reddedilen komutlar:** `999999` → `refused (bad_skill)`; `cast BotWP_K 110518 ...` (başka sınıf) → `refused (bad_skill)`; `110533` → `refused (unsupported_skill)`; `110518 self` ve `110518 BotMF_K` → `refused (bad_target)`; `Ghost` ve `Nobody` → `unknown or not spawned bot '?'`; `... 0`, `... 21`, `... abc`, argümansız → kullanım satırı; `cast BotMF_K off` (hareketsiz) → `not casting`; `cast all off` → `0 stopped, 4 not casting`. **`off` ortada:** `cast ... 5` sonra ~3,4 sn'de `off` → `stopped after 3 packet(s) sent`; sonraki 6 sn'de JSONL satır sayısı değişmedi (EFFECTING gitmedi). `despawn` edilmiş bot / hedef ve `RESPAWN_CYCLES=2` reddi bu turda yeniden sınanmadı (kod F2-05/F4-02'deki kalıpla aynı).
  - **S6 yaşam döngüsü:** seri sürerken hedef `despawn` → `cast stopped (target_lost)`; caster `despawn` → `despawned (... names cleared yes)`, sonra JSONL satır sayısı değişmedi (129 → 129), `pool free` 13/16 (3 oturum); yeniden `spawn` → `casting=0 mp=6021/6021`.
  - **S7 gerilemesiz:** `attack BotWP_K BotWP_E 3` → `3 hit(s) sent, 3 ok`; aynı bot `cast 106560 ... 3` + `attack ... 3` birlikte → `cast finished (effected) after 3 cycle(s), 3 ok`, `attack finished (hit) ... 3 ok`, yeni `FAIRNESS_REJECT` yok (CLI-11 ortak pencere ≤ 6/sn); 30 m `move` → `after 5 packets`; `PERF_SAMPLE` `tick_p95_us` 104–367 µs (≤ 1 ms), `skipped_ticks` 0; sunucu 3/3 UP, `GameServer.log` +0 satır. `TELEMETRY=summary` ve `ENABLED=0` bu turda **yeniden sınanmadı** (cast kodunun `IsEnabled(TEL_DECISIONS)` kapıları F4-02 ile aynı kalıp; `ENABLED=0` yolu diff'te değişmedi).
  - Temizlik: ini yedekten geri yüklendi (md5 `d16463283c0d41074a2d8b6ec4aee203`, önce/sonra aynı), `BotCommands.*` kalmadı, sunucular kapalı; dört bot satırı (`BotMF_K`, `BotWP_E`, `BotWP_K`, `BotPHD_E`) için hedefli `UPDATE` ile `Hp=Mp=32000`, `PX=127400`, `PZ=89000` geri yazıldı (yalnızca bu dört satır; kişisel veri tablosu okunmadı). Test artıkları depo dışında `C:\dev\fdp\server\Logs\bots_old_f403\` ve `Logs\bots\`.
- Bulgular (önem sırasına göre):
  1. **[Orta] `FAIRNESS_REJECT out_of_range` olayında `value`/`limit` hep 0.** `GameServer/Bot/ActionExecutor.cpp:525-526` `value`/`limit` 0 ile başlatılıyor; `switch (verdict)` (`:528-553`) `CAST_REJECT_OUT_OF_RANGE` için `case` içermiyor, `default: break;` ile düşüyor. Plan §5.4 kural tablosu `OUT_OF_RANGE` için `value` = `skillRange > 0` ise `meters` değilse `distanceField`, `limit` = `skillRange > 0` ise `skillRange` değilse `weaponRangeField` diyor; §7 S4 de `value ≥ limit 56` bekliyor. Sonuç: MET-FAIR-01 ve telemetri analizinde menzil reddi ölçülemez (F4-02'de aynı olay `value:148.00, limit:20.00` veriyordu). Birim testle yakalanmaz (`RejectCast` `BotCombat.h` dışında), uygulayıcı özdeğerlendirmesi bunu görmedi.
  2. **[Not]** `SubmitCast` (`:645` `(void)now;`) ve `BeginCast` (`:656` `(void)now;`) `now` parametresi de kullanılmıyor; F4-02'deki `(void)now` kalıbı gibi kabul edildi, engel değil.
  3. **[Not]** Plan §7 S1'deki "`mp` azaldı (2 × 60)" beklentisi tam değil: bot MP yenilenmesi (`list` ~8 sn sonra) farkı küçültüyor (6021 → 5941); kod etkilenmez, sonuç yalnızca paketten okunuyor.
  4. **[Not]** `UseStanding = 1` skill'leri yerel `MAGIC`'te yalnızca sınıf 101 (`Skill = 1010`) için var; bot sınıflarıyla eşleşmediğinden `not_standing` çalışma zamanında yalnızca birim testiyle sınanabilir (planda öngörülüydü).
- **Düzeltme talimatı** (DeepSeek'e aynen verilecek)

```
plans/F4-03-aksiyon-yurutucu-cast.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
1. GameServer/Bot/ActionExecutor.cpp, RejectCast() (:520-553): switch (verdict) içine CAST_REJECT_OUT_OF_RANGE için case ekle (default'tan önce): rule = "MEC-MAG-11"; reason = "out_of_range"; value = (c.skillRange > 0) ? c.distanceM : (float)c.distanceField; limit = (c.skillRange > 0) ? (float)c.skillRange : (float)c.weaponRangeField. Yorum İngilizce, tek satır. Başka verdict'e ve başka fonksiyona dokunma; kural/sebep metinleri aynı kalsın (başlangıç değerleri zaten "MEC-MAG-11"/"out_of_range").
2. ./tools/build.sh Release ve ./tools/build.sh Debug hatasız, ActionExecutor.cpp için uyarı yok. ./tools/run-tests.sh Release ve Debug: 24 tests, 0 failed. Çıktıları raporuna yaz.
3. Başka dosyaya dokunma (docs/**, BotCombat.h, testler, GameServer/** içindeki diğer dosyalar dahil). Sunucuyu çalıştırma. Durum satırını UYGULANDI yap.
```
