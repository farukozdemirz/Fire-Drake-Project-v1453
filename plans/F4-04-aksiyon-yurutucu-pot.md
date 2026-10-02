# F4-04: `ActionExecutor` pot dilimi — `UsePotion` (kendine HP/MP potu) ve `BotFairnessGuard` CLI-06 kuralları

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-04` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-03 (cast dilimi, `m_castEcho`/`m_castSelfId`, `m_cast*` zamanlayıcıları) — `KAPANDI`; F4-02 (`m_actionWindow`) — `KAPANDI`; F4-01 — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-06, CLI-11, MEC-POT-01..04, MEC-MAG-05, MB-01, K-5 (ADR-0009), REQ-NEW-09, AC-SUR-04 (envanterde olmayan pot = 0, 2 sn grup bekleme ihlali = 0), MET-ACT-02, MET-FAIR-01 altyapısı, AC-LRN-03 |
| Tahmini büyüklük | M (8 dosya; yeni dosya yok, `proj-GameServer.vcxproj` ve `BotCore*.vcxproj` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

Botun dördüncü gerçek aksiyonu: bir bot, **kendi çantasında bulunan** bir HP veya MP potunu **gerçek `WIZ_MAGIC_PROCESS` (`MAGIC_EFFECTING`) paketi** ile `CUser::HandlePacket()` üzerinden içer. Her paket sunucuya gitmeden önce `BotFairnessGuard` kurallarından geçer: pot çantada ≥ 1 adet (CLI-06; MB-01 potlarında sunucu bunu kontrol etmez, **bot eder**), HP ve MP için ortak 2500 ms bekleme (CLI-06), aksiyon hızı tavanı (CLI-11). Sonuç, sunucunun yayınladığı sonuç paketinden okunur (HP/MP değişimine **bakılmaz**). Karar katmanı yoktur: aksiyonu `/bot pot` komutu tetikler. Bot sistemi kapalıyken (varsayılan) hiçbir şey değişmez.

F4'ün dördüncü dilimidir (ADR-0017 Karar 4: hareket → saldırı → cast → **pot** → `Perception`; kararlar ADR-0017 "Ek (F4-04)").

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-04)" (bu planla birlikte yazıldı): kapsam sınırı, çanta kuralı, ortak 2,5 sn bekleme, cast zamanlayıcılarıyla ilişki, sonuç eşlemesi.
- `docs/adr/ADR-0009-tuketilmeyen-potlar.md` ve `docs/11` §2: **K-5** — veri olduğu gibi kalır; bot yalnızca çantasında ≥ 1 adet olan potu kullanır.
- `plans/F4-03-aksiyon-yurutucu-cast.md` §5.2–§5.5 ve Doğrulama Raporu: bu planın kalıpladığı iskelet (saf mantık `BotCore/BotCombat.h` + `ActionExecutor` + `BotSession` durumu + `BotManager` komutu). **F4-03'ün yazılı hâlini değil, birleşmiş kodu esas al** (`ActionExecutor.cpp` `RejectCast` `:520`, `SubmitCast` `:571`, `BeginCast` `:653`, `TickCast` `:738`, `EndCast` `:930`).
- `docs/03` §6 **MEC-POT-01..05**, §4.2 **MEC-MAG-05**, §14 **CLI-06**, **CLI-11** ve §13.2 ölçümleri (pot paketinde yalnızca `opcode 3`; HP/MP pot aralığı 2504–2665 ms; HP→MP geçişi 2540 ms), `docs/11` §3.1.
- `docs/13` §5.2 (`ActionType::UsePotion`), §8 (sonuç eşleme), `docs/16` §3.2 (olaylar), MET-ACT-02.
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `3bb40c7` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicProcess.cpp:16-53` — `MagicPacket()`: paket okuma sırası `opcode, skill, caster, target, sData[0..6]`; **`sCasterID != pCaster->GetID()` ise sessizce döner** (`:47-49`) → bot `caster` olarak kendi kimliğini yazar (F4-03 `SubmitCast` ile aynı).
  - `GameServer/MagicInstance.cpp:108-126` — `MAGIC_EFFECTING`: `ExecuteSkill(bType[0])` başarılıysa **pot için de** `m_CoolDownList[skill]` ve `m_MagicTypeCooldownList[bType[0]]` (= tip 3) kaydı yazılır (`:119-125`).
  - `GameServer/MagicInstance.cpp:150-293` — `UserCanCast()`: `iUseItem != 0` ise `CanUseItem(iUseItem, 1)` (`:243-260`); **`iUseItem == 0` ise (MB-01) envanter hiç sorulmaz**. `:278-286` `IsAvailable()` (Moral 1 = yalnızca kendine).
  - `GameServer/MagicInstance.cpp:296-440` — `CheckSkillPrerequisites()`: skill başına soğuma `:360-370`; tip kapısı `:386-422` yalnızca `nSkillID < 400000` için (**pot ID'leri 490xxx/500xxx → kapıya girmez**, MEC-MAG-05).
  - `GameServer/MagicInstance.cpp:936-960` — `NO_POTIONS` debuff'ı (`canUsePotions()`), HP iyileştiren `UseItem != 0` skill'leri engeller (`goto fail_return`); bot bunu **algılamaz**, sonuçtan (`MAGIC_FAIL`) öğrenir.
  - `GameServer/MagicInstance.cpp:1263-1620` — `ExecuteType3()`: tek hedefte `bDirectType` 1 (HP) / 2 (MP), hedef başına `BuildAndSendSkillPacket(... bOpcode ...)` (`:1599`, bölgeye, **opcode 3**; HP/MP dolu olsa da gönderilir `[A]`, doğrulamada Claude teyit eder); pot tüketimi `RobItem(nConsumeItem)` `:2985-2997` (`nConsumeItem = pSkill->iUseItem`, MB-01'de 0 → hiçbir şey düşmez).
  - `GameServer/MagicInstance.cpp:730-760` — `BuildSkillPacket`: yayınlanan paket düzeni `opcode(i8), skill(u32), caster(i16), target(i16), sData[0..6]`; `BotSession::OnPacket()` (`BotSession.cpp:40-53`) bunu zaten kaydeder (**değişmeyecek**).
  - `GameServer/GameDefine.h:315` `_ITEM_TABLE::m_iEffect1` (potun skill kimliği), `:305-` `m_bClass`, `m_bReqLevel`; `g_pMain->GetItemPtr(id)` (`GameServerDlg.h:338`); `g_pMain->m_MagictableArray.GetData(id)` ve `m_Magictype3Array.GetData(id)` (`_MAGIC_TYPE3`: `bDirectType`, `sFirstDamage`, `sTimeDamage`, `bDuration`; `shared/database/structs.h:51-62`).
  - `GameServer/User.h:557-561` `GetItem(uint8 pos)`; `shared/globals.h:286-` `_ITEM_DATA` (`nNum`, `sCount`); `shared/globals.h:226-236` `SLOT_MAX = 14`, `HAVE_MAX = 28`, `INVENTORY_INVENT = SLOT_MAX` (çanta = 14..41); `GameServer/User.h:598` `JobGroupCheck(short)` (`User.cpp:4649`); `GameServer/ItemHandler.cpp:254-266` `CheckExistItem` (sunucu **ekipman yuvalarına da** bakar; bot yalnızca çantaya bakar).
  - `GameServer/Bot/ActionExecutor.cpp:36-69` `NextDecisionId`, `FormatFixed`, `EmitFairnessReject`; `:891-901` pot ile aynı biçimde güncellenecek cast zamanlayıcıları (`m_castTypeHas`/`m_castTypeLast`/`m_castAnyHas`/`m_castAnyLast`); `BotSession.cpp:3-18` (başlatıcı listesi) ve `:56-95` `ResetForRespawn()`; `BotManager.cpp:627-628` fiil dağıtımı, `:1342-` `CommandCast()` (kalıp), `:1815-1880` `TickSessions()` cast bloğu, `:823-826` `BuildStatusLines()` biçimi, `:1953-1957` `BeginDespawn()`.
  - Yerel `MAGIC`/`MAGIC_TYPE3` verisi (`SELECT`, yasak tablolar dışında; Claude doğruladı): `490014` (720 HP, `UseItem 0`, MB-01), `490015` (1440 HP, `UseItem 389015000`), `490019` (960 MP, `UseItem 0`), `490020` (1920 MP, `UseItem 0`), `490701` (2160 MP, `UseItem 389220000`); hepsi `Moral 1, Skill 0, SkillLevel 0, Msp 0, CastTime 5, ReCastTime 20, Type1 3, Type2 0, Range 56, Etc 0, FlyingEffect 0, UseStanding 0`; `DirectType` 1 (HP) / 2 (MP), `TimeDamage 0`. Bot çantası (`db/002_bot_characters.sql:207`): slot 14 = `389014000` ×1 (Effect1 490014), slot 15 = `389015000` ×100 (490015), slot 16 = `389020000` ×1 (490020), slot 17 = `379006000` ×30 (pot değil). **Çantada `389019000` (960 MP) yoktur** (envanter dışı pot testi için kullanılır).

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h` içine ekleme, yalnızca standart kütüphane):** pot bekleme sabiti, `PotSupported`, `PotionCheck`, `PotionVerdict`, `PotionWaitMs`, `CheckPotion`. Birim testleri (`CombatTests.cpp`).
2. **`ActionExecutor` (`BeginPotion` / `TickPotion` / `EndPotion`):** `WIZ_MAGIC_PROCESS` (`MAGIC_EFFECTING`) paketini oluşturur, guard'dan geçirir, `CUser::HandlePacket()` ile işletir, sunucunun yayınladığı sonuç paketinden `effected` / `srv_fail` / `no_result` eşler, telemetri olaylarını üretir; pot sonrası cast zamanlayıcılarını (tip 3, genel boşluk) günceller.
3. **Oturum durumu (`BotSession`):** pot serisi durumu (item, skill, tür, kalan, gönderilen, başarılı) ve spawn boyunca geçerli ortak pot zamanı.
4. **Komutlar (`BotManager`):** `pot <bot> <item id> [adet]` ve `pot <bot>|all off` (konsol, `BotCommands.txt` ve `+bot` aynı çekirdekten); her `Tick()`'te seriyi ilerletme; `list` satırına `pot=`.
5. **Telemetri:** `decisions` seviyesinde `ACTION_SUBMIT` / `ACTION_RESULT` (`"type":"UsePotion"`), `FAIRNESS_REJECT` (`"type":"Potion"`).

**Kapsam dışı (yapılmayacak)**

- **Pot seçimi/kararı** (hangi kademe, ne zaman, `docs/11` §3.2 formülleri), HP/MP eksikliğine bakma, "gereksiz pot" önleme: karar katmanının işi. `pot` komutu verilen potu, bekleme kurallarına uyarak, verilen adet kadar içer; botun HP/MP'si dolu olsa da içer (sonuç yalnızca paketten okunur).
- **Zamanla iyileştiren / buff veren / sınıfa özgü / uzun bekleme süreli** potlar, yiyecekler, scroll'lar, pot dışı tüketilebilirler: `unsupported_item` ile reddedilir (ADR-0017 Ek F4-04 madde 2).
- **Envanter doldurma / yeniden stoklama**, çanta dışı (magic bag, cospre, depo) yuvalar: yok; yalnızca çanta yuvaları (14..41) okunur, eşya yazılmaz, `RobItem`/`GiveItem` çağrılmaz.
- **`NO_POTIONS` / `SILENCE` / `KAUL` algısı** (MEC-POT-04): yok; sunucu reddederse `srv_fail` ile öğrenilir.
- **CLI-06'nın "HP/MP gerçekten ayrı mı" araştırması** (Q-06, T-MECH-POT-03): ortak zamanlayıcı muhafazakâr varsayımdır; ölçümü bu plan yapmaz.
- Pot iptali (`MAGIC_FAIL`), pot sırasında hareketin durdurulması (Q-06, bilinmiyor): yok; `pot` sırasında `move` sürer.
- `ChatHandler.cpp` `+bot` yardım metni (KI-012; Claude doğrulamada günceller). Yeni `GameServer.ini` anahtarı yok. `ENABLED=0` iken davranış değişmez. Yeni dosya yok; **`GameServer` projesine dosya eklenmez**.
- Dokümanları (`docs/03`, `docs/11`, `docs/13`, `docs/16`) güncellemek: Claude'un işi, DeepSeek dokunmaz.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | Yalnızca ekleme (§5.2); mevcut içerik ve `#include`'lar değişmez |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | Yalnızca ekleme: iki yeni `TEST_CASE` |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `PotionOutcome`, üç yeni statik fonksiyon |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | Pot yolu (§5.4); mevcut hareket/saldırı/cast kodu değişmez |
| `GameServer/Bot/BotSession.h` | değiştir | Yalnızca pot durumu üyeleri |
| `GameServer/Bot/BotSession.cpp` | değiştir | Başlatıcı listesi ve `ResetForRespawn()`; **`OnPacket()` değişmez** |
| `GameServer/Bot/BotManager.h` | değiştir | `CommandPot` bildirimi |
| `GameServer/Bot/BotManager.cpp` | değiştir | Fiil dağıtımı, komut, `TickSessions()`, `BeginDespawn()`, `BuildStatusLines()` |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. (Yeni dosya açılmaz: guard `BotCombat.h`'ye eklenir, böylece `BotCore*.vcxproj` değişmez.)

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-04 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `BotCore/BotCombat.h` (saf mantık, mevcut `namespace BotCore` içine ekleme)

Mevcut kodun biçimini (tab, Allman, `inline`, İngilizce kısa yorum, ölçülmemiş değer `[A]`) koru. Yeni `#include` gerekmez. `windows.h`, `stdafx.h`, `../GameServer`, `../shared` **geçmez**. `CheckCastEffect`'ten **sonra**, `namespace`'in kapanışından önce ekle:

```cpp
	// --- potion slice (ADR-0017 Ek F4-04) ---

	constexpr uint32_t kPotCooldownMs = 2500;   // docs/03 CLI-06: HP and MP pots share ~2.5 s (measured 2504..2665 ms; HP->MP 2540 ms) [A: shared timer]

	// Supported pots: the server's per-skill recast (MAGIC.ReCastTime, 0.1 s units) never exceeds the shared timer,
	// so no per-skill bookkeeping is needed. Longer-recast items are out of scope (unsupported_item).
	inline bool PotSupported(uint16_t reCastTime);   // CastRecastMs(reCastTime) <= kPotCooldownMs

	struct PotionCheck
	{
		uint32_t stock;           // count of the pot item in the bot's own bag
		bool hasLast;             // any pot packet was sent earlier in this spawn
		uint32_t sinceLastMs;     // since that packet
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum PotionVerdict
	{
		POT_OK = 0,
		POT_REJECT_NO_STOCK = 1,   // CLI-06 (no item in the bag; MB-01 pots included)
		POT_REJECT_COOLDOWN = 2,   // CLI-06 (shared 2.5 s timer)
		POT_REJECT_RATE = 3        // CLI-11
	};

	// Milliseconds until the shared pot timer allows the next pot; 0 = now. Stock and rate are not timing waits.
	inline uint32_t PotionWaitMs(const PotionCheck & c);

	// Guard rule for a pot packet. Order: no stock, cooldown, rate.
	inline PotionVerdict CheckPotion(const PotionCheck & c);
```

`CastRecastMs` aynı başlıkta daha yukarıda tanımlıdır. Gövdeleri mevcut kalıbı izleyerek yaz (`PotSupported`: `CastRecastMs(reCastTime) <= kPotCooldownMs`; `PotionWaitMs`: `hasLast && sinceLastMs < kPotCooldownMs ? kPotCooldownMs - sinceLastMs : 0`; `CheckPotion`: sırayla `stock < 1`, `hasLast && sinceLastMs < kPotCooldownMs`, `actionsInWindow >= kMaxActionsPerWindow`).

**`Tests/BotCoreTests/CombatTests.cpp` (ekleme, mevcut makro stili):** iki yeni `TEST_CASE`:

- `Combat_PotCheck_Order`: `stock = 0` → `POT_REJECT_NO_STOCK` (cooldown ve oran da ihlal edilse bile, sıra); `stock = 1, hasLast, sinceLastMs = 2499` → `POT_REJECT_COOLDOWN`; `sinceLastMs = 2500` → `POT_OK`; `hasLast = false` → `POT_OK`; `actionsInWindow = 6` (diğerleri geçerli) → `POT_REJECT_RATE`; `actionsInWindow = 5` → `POT_OK`.
- `Combat_PotWait`: `PotionWaitMs` (`hasLast = false` → 0; `sinceLastMs = 0` → 2500; `1000` → 1500; `2500` → 0; `9000` → 0) ve `PotSupported` (`20` → true; `25` → true; `26` → false; `150` → false; `0` → true).

### 5.3 `GameServer/Bot/BotSession.h/.cpp`

`BotSession.h`'de cast üyelerinin altına (aynı yorum/hizalama biçimi), hepsi **IOCP thread only**:

```cpp
	bool m_potActive;                                      // IOCP thread only: a pot series is in progress
	uint32 m_potItemId;                                    // IOCP thread only: ITEM.Num of the pot
	uint32 m_potSkillId;                                   // IOCP thread only: its ITEM.Effect1 skill
	uint8 m_potKind;                                       // IOCP thread only: 1 = HP (DirectType 1), 2 = MP (DirectType 2)
	uint32 m_potLeft;                                      // IOCP thread only: pots still to drink in this series
	uint32 m_potSent;                                      // IOCP thread only: pot packets sent in this series
	uint32 m_potOk;                                        // IOCP thread only: of those, result "effected"
	bool m_potHasLast;                                     // IOCP thread only: m_potLast is valid for this spawn (shared timer)
	std::chrono::steady_clock::time_point m_potLast;       // IOCP thread only: when the last pot packet went out (any pot)
```

`BotSession.cpp`: başlatıcı listesine `m_potActive(false), m_potItemId(0), m_potSkillId(0), m_potKind(0), m_potLeft(0), m_potSent(0), m_potOk(0), m_potHasLast(false)` ekle (mevcut sıraya uy, üye bildirim sırasıyla aynı sırada: derleyici sıra uyarısı vermemeli). `ResetForRespawn()` içine aynı alanları sıfırla (`m_potHasLast = false` dahil: ortak zaman spawn başına). `OnPacket()` **değişmez**: pot sonucu F4-03'ün `m_castEcho` / `m_castSelfId` mekanizmasından okunur.

### 5.4 `GameServer/Bot/ActionExecutor.h/.cpp`

**Başlık (`ActionExecutor.h`):** `CastOutcome`'dan sonra:

```cpp
struct PotionOutcome
{
	enum Kind { NOTHING, SENT, FINISHED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "ok", "effected", "srv_fail", "no_result",
	                       // "not_in_game", "dead", "bad_item", "unsupported_item", "no_stock", "pot_cooldown", "rate"
};
```

`class ActionExecutor` içine (cast bildirimlerinin altına, aynı yorum kalıbıyla):

```cpp
	// Validates and arms a series of 'count' pots of ITEM 'itemId'; sends nothing yet (the same Tick()'s TickPotion()
	// does). REFUSED (nothing armed): "not_in_game", "dead", "bad_item" (count < 1, unknown item, no Effect1, level or
	// class does not match, unknown skill), "unsupported_item" (see 5.4 rules). The bag stock is NOT checked here:
	// the guard checks it before every packet, so the refusal is visible as FAIRNESS_REJECT (CLI-06).
	static PotionOutcome BeginPotion(BotSession * s, uint32 itemId, uint32 count,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session; NOTHING unless s->m_potActive and the shared pot timer allows
	// the next pot. Sends at most one WIZ_MAGIC_PROCESS (MAGIC_EFFECTING). SENT: pot sent, series continues.
	// FINISHED: last pot sent. REFUSED: guard rejected, series dropped. FAILED: server answered MAGIC_FAIL or
	// published nothing, series dropped.
	static PotionOutcome TickPotion(BotSession * s, std::chrono::steady_clock::time_point now);

	// Clears the pot series without sending anything (stop, despawn). Keeps the shared pot timer.
	static void EndPotion(BotSession * s);
```

**`ActionExecutor.cpp`**, cast kodunun (`EndCast` sonrası) altına; mevcut yardımcıları (`NextDecisionId`, `FormatFixed`, `EmitFairnessReject`) **olduğu gibi** kullan:

1. **`static uint32 CountInBag(CUser * user, uint32 itemId)`:** `i = INVENTORY_INVENT .. INVENTORY_INVENT + HAVE_MAX - 1` için `user->GetItem((uint8)i)`; `nNum == itemId` olan yuvaların `sCount` toplamı. (Yalnızca çanta; ekipman/cospre/magic bag **sayılmaz**.)
2. **`BeginPotion`:**
   - `s->m_pUser == nullptr || !isInGame()` → `REFUSED "not_in_game"`; `isDead()` → `REFUSED "dead"` (MEC-POT-04); `count < 1` → `REFUSED "bad_item"`.
   - `_ITEM_TABLE * it = g_pMain->GetItemPtr(itemId)`; `it == nullptr || it->m_iEffect1 == 0` → `bad_item`. `it->m_bReqLevel != 0 && user->GetLevel() < it->m_bReqLevel` veya `it->m_bClass != 0 && !user->JobGroupCheck(it->m_bClass)` → `bad_item` (sunucunun `:1018-1028` aynası). `JobGroupCheck`'in `public` olduğunu doğrula (`User.h:598`, `public:` bölümü `:302`); değilse **durup** soru olarak raporla.
   - `_MAGIC_TABLE * m = g_pMain->m_MagictableArray.GetData(it->m_iEffect1)`; `m == nullptr` → `bad_item`. `_MAGIC_TYPE3 * t3 = g_pMain->m_Magictype3Array.GetData(it->m_iEffect1)`.
   - **Destek kuralı** (hepsi sağlanmazsa `unsupported_item`): `m->bType[0] == 3 && m->bType[1] == 0 && m->bMoral == MORAL_SELF && m->sSkill == 0 && m->sMsp == 0 && m->sUseStanding == 0 && m->sEtc == 0 && m->bFlyingEffect == 0 && (m->iUseItem == 0 || m->iUseItem == itemId) && BotCore::PotSupported(m->sReCastTime) && t3 != nullptr && (t3->bDirectType == 1 || t3->bDirectType == 2) && t3->sFirstDamage > 0 && t3->sTimeDamage == 0`. `m_potKind = t3->bDirectType`.
   - Hazırla: `m_potActive = true; m_potItemId = itemId; m_potSkillId = it->m_iEffect1; m_potKind; m_potLeft = count; m_potSent = 0; m_potOk = 0; s->m_castSelfId = user->GetID();` → `SENT "ok"` (F4-03 `BeginCast` kalıbı: arma başarılı = `SENT`/`ok`, komut bunu "using" olarak yazar). Zaten aktif seri varsa **yenisi onu değiştirir** (zamanlayıcılar korunur).
3. **`TickPotion`:** `!s->m_potActive` → `NOTHING`. Oturum/kullanıcı/`isDead()` sorunu → `EndPotion` + `REFUSED "not_in_game"`/`"dead"`.
   - `nowMs` ve `inWindow` (`s->m_actionWindow`) F4-03 `TickCast` ile aynı biçimde hesaplanır (aynı `steady_clock` → ms dönüşümü; kodu kopyalama yerine mevcut satırlara bak).
   - `PotionCheck c = { CountInBag(...), s->m_potHasLast, sinceLastMs (hasLast ise now - m_potLast, ms), inWindow }`.
   - **Önce çanta:** `c.stock < 1` → `RejectPotion(..., POT_REJECT_NO_STOCK, c)`.
   - **Bekleme:** `BotCore::PotionWaitMs(c) > 0` → `NOTHING` (normal akış; `FAIRNESS_REJECT` **yazılmaz**).
   - `BotCore::CheckPotion(c) != POT_OK` → `RejectPotion` (bekleme sıfırken yalnızca `RATE` kalabilir; yine de her `HandlePacket`'tan önce çağrı vardır).
   - `SubmitPotion(...)` ile gönder; ardından zamanlayıcıları güncelle: `m_potHasLast = true; m_potLast = now;` ve **cast zamanlayıcıları (ADR-0017 Ek F4-04 madde 5):** `m_castTypeHas[3] = true; m_castTypeLast[3] = now; m_castAnyHas = true; m_castAnyLast = now;`. Pot bu zamanlayıcıları **okumaz**.
   - `m_potSent++`; sonuç `effected` ise `m_potOk++`, `m_potLeft--`. `effected` değilse (`srv_fail`/`no_result`) `EndPotion` + `FAILED` (reason = sonuç). `m_potLeft == 0` → `EndPotion` (zamanlayıcılar korunur) + `FINISHED "effected"`; aksi halde `SENT "effected"`.
4. **`static PotionOutcome RejectPotion(BotSession * s, CUser * user, BotCore::PotionVerdict v, const BotCore::PotionCheck & c)`:** `NO_STOCK` → `rule "CLI-06", reason "no_stock", value = stock, limit = 1`; `COOLDOWN` → `rule "CLI-06", reason "pot_cooldown", value = sinceLastMs, limit = kPotCooldownMs`; `RATE` → `rule "CLI-11", reason "rate", value = inWindow, limit = kMaxActionsPerWindow`. `decisionId = NextDecisionId(s)`; `EmitFairnessReject(s, user, decisionId, "Potion", rule, reason, value, limit)`; `EndPotion(s)`; `REFUSED` + reason. Sunucuya paket **gitmez**.
5. **`static PotionOutcome SubmitPotion(...)`:** `SubmitCast` kalıbı:
   - `ACTION_SUBMIT` (`decisions`): `"decision_id","type":"UsePotion","item":<id>,"skill":<id>,"kind":"hp"|"mp","stock":<paket öncesi adet>,"use":<m_potSent+1>`.
   - Paket: `Packet pkt(WIZ_MAGIC_PROCESS); pkt << uint8(MAGIC_EFFECTING) << uint32(skill) << int16(user->GetID()) << int16(user->GetID()) << int16(0) × 6;` (21 bayt: `u8 + u32 + i16 + i16 + 6 × i16`, F4-03 ile aynı). `s->m_castEcho = 0;` → `user->HandlePacket(pkt)` (gecikme `steady_clock` ile) → `s->m_actionWindow.Record(nowMs)`.
   - Sonuç: `echo` geçerli (`1ull << 63` biti) ve skill eşleşiyorsa `op = (echo >> 48) & 0xF`, `code = (int16)((echo >> 32) & 0xFFFF)`; `op == MAGIC_EFFECTING` → `effected`; `op == MAGIC_FAIL` → `srv_fail`; aksi → `no_result`.
   - `ACTION_RESULT`: `"decision_id","type":"UsePotion","ok","reason","op","code","latency_us","stock_after":<CountInBag, paketten sonra>`. `stock_after` **yalnızca** operatör/doğrulama içindir; sonuç eşlemesi buna **bakmaz**.
6. **`EndPotion`:** `m_potActive = false; m_potLeft = 0;` (`m_potSent`/`m_potOk`/`m_potHasLast`/`m_potLast` korunur; `m_potSent`/`m_potOk` bir sonraki `BeginPotion`'da sıfırlanır, `ResetForRespawn`'da da).

`HandlePacket(pkt)` çağrısı pot için **tek yerde** (`SubmitPotion`). Hiçbir yerde `MagicPacket`/`MagicInstance`, `RobItem`/`GiveItem`, `MSpChange`/`HpChange`, `m_CoolDownList` çağrılmaz.

### 5.5 `GameServer/Bot/BotManager.h/.cpp`

`BotManager.h`'ye özel bildirim: `void CommandPot(const std::string & args);` (IOCP thread only).

1. **`ExecuteCommand()` (`:627`):** `cast` dalının yanına `pot` fiilini ekle; "unknown command" listesini `(spawn, despawn, list, match, scenario, move, stop, attack, cast, pot)` yap. `RESPAWN_CYCLES != 0` reddine dokunma.
2. **`CommandPot(args)`:** `CommandCast` kalıbını izle (`SplitWords`, `FindSession`, `IsKnownBotName`, `ParseIntStrict`, `PhaseName`, `WriteBotLog`). Kullanım satırı (hata durumlarında **tek** günlük satırı ve dön): `BotManager: cmd pot: usage: pot <bot> <item id> [count] | pot <bot|all> off`.
   - **`off`:** `words.size() == 2 && _stricmp(words[1], "off") == 0`. `all` ise her `PHASE_IN_GAME` oturum için, değilse adı verilen oturum için (aynı `unknown or not spawned bot '<ad|?>'` / `not in game (phase X)` iletileri, ön ek `cmd pot:`): seri aktifse (`m_potActive`) `EndPotion` ve `cmd pot: <bot> stopped after <m_potSent> use(s)`, değilse `cmd pot: <bot> not potting`. `all` için özet: `cmd pot all: N stopped, M not potting`.
   - **Başlatma:** `words.size() == 2 || 3`. Bot: `FindSession(words[0])` (aynı iletiler). Item: `ParseIntStrict(words[1], v)`, `v < 1 || v > 2147483647` → kullanım satırı. Adet: `words.size() == 3` ise `ParseIntStrict`, `1..20` dışı → kullanım satırı; verilmezse 1. Sonra `ActionExecutor::BeginPotion(s, (uint32)v, (uint32)count, now)`: `REFUSED` → `cmd pot: <bot> refused (<reason>)`; aksi halde `cmd pot: <bot> using <item> (<n> use(s))`.
3. **`TickSessions()` `PHASE_IN_GAME` dalı (cast bloğundan sonra, aynı `if (s->m_phase == PHASE_IN_GAME)` içinde):** `if (s->m_potActive)`:
   - `s->m_pUser->isDead()` → `EndPotion(s)` + `bot <ad> pot stopped (dead)` günlüğü.
   - Aksi halde `PotionOutcome o = ActionExecutor::TickPotion(s, now)`: `FINISHED` → `bot <ad> pot finished (<reason>) after <m_potSent> use(s), <m_potOk> ok`; `REFUSED`/`FAILED` → `bot <ad> pot stopped (<reason>)`; `SENT`/`NOTHING` → günlük yok.
4. **`BeginDespawn()` (`:1953`):** `ActionExecutor::EndCast(s)` satırının yanına `ActionExecutor::EndPotion(s)`.
5. **`BuildStatusLines()` (`:823`):** oturum satırının sonuna `pot=<0|1>` ekle (`m_potActive`); mevcut alanların sırası ve adları **değişmez** (`mp=` sonrası). `char message[192]` kesilmeye başlarsa 256'ya çıkar (`list` çıktısının kesilmediğini gör).
6. Başka hiçbir yere dokunma (`ScenarioRunner`, `Telemetry`, `ChatHandler.cpp` dahil).

### 5.6 Proje dosyaları

`GameServer/proj-GameServer.vcxproj`, `.filters`, `BotCore/BotCore.vcxproj` ve `Tests/BotCoreTests/BotCoreTests.vcxproj` **değişmez** (yeni dosya yok). Mevcut dosyaların kodlama/satır sonu/BOM durumu korunur.

### 5.7 Derle ve sına

`./tools/build.sh Release`, `./tools/build.sh Debug`, `./tools/run-tests.sh Release` (ve `Debug`). Hepsi hatasız/yeşil. **Sunucuyu çalıştırma** (çalışma zamanı doğrulamasını Claude yapar, bkz. §7).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (`BotCombat.h`, `ActionExecutor.cpp`, `BotSession.cpp`, `BotManager.cpp`, `CombatTests.cpp` için uyarı çıktısı boş).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı iki yeni test adını (`Combat_PotCheck_Order`, `Combat_PotWait`) içerir ve toplam test sayısı **26**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; dosyada `#include` yalnızca `<algorithm>` ve `<cstdint>`.
- [ ] K5: pot paketi yalnızca `ActionExecutor.cpp`'de oluşturuluyor ve `HandlePacket` ile işletiliyor: `grep -n "WIZ_MAGIC_PROCESS" GameServer/Bot/*.cpp` yalnızca `ActionExecutor.cpp` (paket oluşturma: `SubmitCast`, `SubmitPotion`) ve `BotSession.cpp` (`OnPacket` sonuç okuma) satırlarını gösterir; `grep -n "MagicPacket\|MagicInstance\|m_CoolDownList\|m_MagicTypeCooldownList\|MSpChange\|HpChange\|RobItem\|GiveItem" GameServer/Bot/*.cpp GameServer/Bot/*.h` çağrı göstermez (yalnızca `#include "../MagicInstance.h"` olabilir).
- [ ] K6: `ActionExecutor`'da guard atlanmıyor: `TickPotion`'da `HandlePacket`'tan önce `CheckPotion` çağrısı ve `POT_OK` dışında erken dönüş vardır; çanta kontrolü (`stock < 1` → `RejectPotion`) `HandlePacket`'tan önce çalışır (kod okumasıyla; Claude çalışma zamanında da sınar). Pot için `HandlePacket(pkt)` çağrısı tek yerde (`SubmitPotion`).
- [ ] K7: çanta okuması yalnızca çanta yuvalarına bakar ve yazmaz: `CountInBag` `INVENTORY_INVENT`..`INVENTORY_INVENT + HAVE_MAX - 1` arasında döner, `GetItem` dışında `m_sItemArray` yazımı yoktur (`grep -n "m_sItemArray" GameServer/Bot/*.cpp` boş veya yalnızca okuma).
- [ ] K8: `ENABLED=0` ile davranış değişmez: yeni kod yalnızca `PHASE_IN_GAME` oturumları üzerinde ve komutlarla çalışır; `git diff gece/2026-10-02...bot/F4-04 -- GameServer/Bot/BotManager.cpp | grep '^-'` yalnızca bilinçli değiştirilen satırları gösterir (`unknown command` mesajı, `BuildStatusLines` biçim satırı ve olası tampon boyutu); `Startup()`/`Tick()` akışı ve `ini` okuma değişmedi; `BotSession::OnPacket()` değişmedi.
- [ ] K9: `git diff --stat gece/2026-10-02...bot/F4-04` yalnızca §4'teki 8 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmemiş.
- [ ] K10: değiştirilen dosyaların satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); `BotCombat.h`/`CombatTests.cpp` ASCII + CRLF kalır; `git diff --check` boş.
- [ ] K11: `GameServer/Bot/ActionExecutor.*` içinde `printf`, `Sleep`, `lock_guard`, `mutex`, `CreateThread`, `rand(` yok.
- [ ] K12: F4-01..F4-03 gerilemesiz: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack" GameServer/Bot/ActionExecutor.cpp` ≥ 1, `grep -c "CheckCastStart" GameServer/Bot/ActionExecutor.cpp` ≥ 1; önceki 24 testin tamamı hâlâ geçiyor; `EmitFairnessReject`'in hareket/saldırı/cast çağrıları `"Move"`/`"Attack"`/`"Cast"` geçiyor.
- [ ] K13 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları 1–7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-04
git diff gece/2026-10-02...bot/F4-04 -- GameServer/Bot/BotManager.cpp | grep '^-'
git diff gece/2026-10-02...bot/F4-04 -- GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -n "WIZ_MAGIC_PROCESS" GameServer/Bot/*.cpp
grep -n "MagicPacket\|MagicInstance\|m_CoolDownList\|m_MagicTypeCooldownList\|MSpChange\|HpChange\|RobItem\|GiveItem" GameServer/Bot/*.cpp GameServer/Bot/*.h
grep -n "m_sItemArray" GameServer/Bot/*.cpp
grep -n "CheckPotion\|HandlePacket" GameServer/Bot/ActionExecutor.cpp
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp
git diff --check gece/2026-10-02...bot/F4-04
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`). Botlar: Karus warrior `BotWP_K`, Karus mage `BotMF_K`, El Morad warrior `BotWP_E` (cast etkileşim testinde hedef); zone 71. Çanta durumu **DB okunmadan** (USERDATA yasak) `ACTION_RESULT`'taki `stock_after` ve `db/002` şablonundan teyit edilir; `MAGIC`/`ITEM` satırları `SELECT` ile teyit edilir. Gözlem `Logs/bots/<tarih>/live-*.jsonl`'den, telemetri `decisions` ile yapılır. Beklenmeyen `srv_fail`/`no_result` bu planın hatası değil, **sonuç olarak raporlanır** (özellikle HP/MP dolu botta pot paketinin yayınlanıp yayınlanmadığı: `ExecuteType3` doğrulaması).

1. **Tüketilen HP potu (mutlu yol):** `spawn BotWP_K,BotMF_K,BotWP_E`; `pot BotWP_K 389015000 3` → log `using 389015000 (3 use(s))`; JSONL: üç kez `ACTION_SUBMIT` (`type:"UsePotion"`, `kind:"hp"`, `skill:490015`, `stock` 100, 99, 98) → `ACTION_RESULT` (`reason:"effected"`, `op:3`, `stock_after` 99, 98, 97); ardışık `ACTION_SUBMIT`'ler arası `t` farkı ≥ 2500 ms ve ≤ 2500 + 250 ms (tick ≈ 110 ms); `FAIRNESS_REJECT` yok; `pot finished (effected) after 3 use(s), 3 ok`; `latency_us` < 5000; `list` yalnızca sonuçta `pot=0`.
2. **MB-01 pot (tüketilmez, çantada var):** `pot BotWP_K 389014000 3` → üç `effected`, her `stock`/`stock_after` **1** (sayı azalmaz, K-5); aralar ≥ 2500 ms. `pot BotWP_K 389020000 2` (1920 MP, `kind:"mp"`, `skill:490020`) → iki `effected`, `stock_after` 1. HP pot → MP pot arası (komutlar peş peşe verilirse) ≥ 2500 ms (**ortak zamanlayıcı**), `FAIRNESS_REJECT` yok.
3. **Çantada olmayan pot reddi (CLI-06, AC-SUR-04):** `pot BotWP_K 389019000` (960 MP, `UseItem 0`: sunucu bu potu çantasız da kabul ederdi; çantada yok) → komut `using` der (arma başarılı), ilk tick'te `pot stopped (no_stock)`; JSONL: yalnızca `FAIRNESS_REJECT` (`type:"Potion"`, `rule:"CLI-06"`, `reason:"no_stock"`, `value` 0, `limit` 1), **`ACTION_SUBMIT` yok**, oturum kopmaz.
4. **Bekleme ve cast etkileşimi:** (a) iki `pot` komutunu 1 sn içinde arka arkaya ver (`pot BotWP_K 389015000 1`, hemen ardından aynı komut) → ikinci pot ilkten ≥ 2500 ms sonra gider, `FAIRNESS_REJECT` **yazılmaz**. (b) `pot BotMF_K 389015000 1` hemen ardından `cast BotMF_K 110518 BotWP_E 1` → `CastStart` potun `ACTION_SUBMIT`'inden ≥ 1000 ms sonra gider (pot tip-3 zamanlayıcısını besler), cast `effected`, `srv_fail` **yok**. (c) `pot` sonrası hemen `cast` yerine `attack BotWP_K BotWP_E 3` birlikte: CLI-11 penceresi ortak, `rate` reddi yok.
5. **Reddedilen komutlar:** `pot BotWP_K 1` (ITEM yok) → `refused (bad_item)`; `ITEM`'de var ama `Effect1 = 0` olan bir eşya veya pot olmayan bir eşya (doğrulayıcı `SELECT` ile seçer, ör. çantadaki `379006000` ya da bir silah kimliği) → `refused (bad_item)` veya `refused (unsupported_item)` (hangisi olduğu raporda yazılır; ikisi de kabul); `ReCastTime > 25` olan bir pot (ör. `389059000`, `ReCastTime 150`) → `refused (unsupported_item)`; `Moral` ≠ 1 veya zamanla iyileştiren bir yiyecek/pot (SELECT ile bulunursa) → `refused (unsupported_item)`; `pot Ghost 389015000` → tek satır `unknown or not spawned bot '?'`; `pot BotWP_K 389015000 0`, `... 21`, `... abc`, argümansız → kullanım satırı; `despawn` edilmiş botla `not in game (phase despawned)`; `pot BotWP_K off` ortada (2. pottan önce) → `stopped after 1 use(s)` ve **sonra yeni paket yok**; `pot all off` boşta botlarda `not potting`, özet `0 stopped, N not potting`; `RESPAWN_CYCLES=2` iken `pot` → `cmd rejected`.
6. **Ömür döngüsü:** yürütülen pot serisi sırasında bot `despawn` → temiz despawn (`despawn complete`, `pool free` tam), despawn sonrası yeni paket yok; yeniden `spawn` → `pot=0` ve ortak pot zamanı sıfır (ilk pot hemen gider; `stock` bu sefer DB'deki kayıtlı adet: tüketilen 1440 HP potu için azalmış olabilir, doğrulama sonunda `db/002` ile bot satırları geri yüklenir ve çanta 100'e döner).
7. **Gerilemesiz:** `attack BotWP_K BotWP_E 3` aynı çıktıyı verir (F4-02); `cast BotMF_K 110518 BotWP_E 2` iki çevrim `effected` (F4-03); `move` 30 m yürüyüşü `5 packets`; `TELEMETRY=summary` iken `ACTION_*` yazılmaz ama pot çalışır; `ENABLED=0` → komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` F4-03 düzeyinde (≤ 1 ms); sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. İnsan istemcisi gerekmez (görsel doğrulama `T-ARCH-09`, `docs/STATUS.md` "Proje sahibi testleri").

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `Bot/` ve `BotCore/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). `pot` yalnızca `ENABLED=1` iken, üretim dışı test komutudur.
- **Thread kuralı (ADR-0005):** `ActionExecutor` yalnızca IOCP thread'inde (`Tick()` içinden) çalışır; `HandlePacket` zaten bu thread'de koşar. Konsol/`+bot` işleyicisi `BotSession`/`ActionExecutor`'a **dokunmaz**; komutlar `EnqueueCommand` kuyruğundan gelir. `OnPacket()` her thread'den çağrılabilir ve bu planda **değişmez**.
- **Sonuç yalnızca yayınlanan sonuç paketinden okunur** (AC-LRN-03 / `docs/13` §8). Botun HP/MP değişimi, `stock_after` veya hedef durumu aksiyon sonucunu belirlemek için **kullanılmaz**. Çanta adedi guard girdisidir (botun **kendi** durumu; istemci de çantasını görür); yalnızca adet okunur, çanta içeriği başka amaçla toplanmaz/loglanmaz (`stock`/`stock_after` yalnızca seçilen potun adedidir).
- **K-5 / ADR-0009'u çiğneme:** MB-01 potları için sunucuya "çantasız" gönderim yolu **ekleme**; bot kuralı çantada ≥ 1 adet olmasıdır. Sunucuya mevcut MB-01 davranışını (`UseItem 0` → tüketim yok) **değiştirme**; `GameServer/` içinde bot dışı hiçbir dosyaya dokunulmaz.
- **`UNIXTIME` 1 sn çözünürlüklüdür** (MEC-MAG-10): bot ≥ 2500 ms bekler, bu sunucunun pot başına 2000 ms kuralını daima geçer. Pot beklemesini 2500 ms'nin altına indirecek bir yol ekleme.
- **Bilinen sınırlar `[A]`:** HP/MP ortak zamanlayıcı varsayımı (Q-06, T-MECH-POT-03); pot paketinde `caster`/`target` alanları (F1 izinde 0 görüldü, sunucu `caster == GetID()` ister; bu planda kendi kimliği yazılır, F4-03 self heal ile aynı); tick (≈ 110 ms) nedeniyle aralık alt sınırdan 0–110 ms geç olabilir; HP/MP dolu botta `ExecuteType3`'ün paketi yine yayınlayacağı varsayımı doğrulamada teyit edilir (yayınlamıyorsa `no_result` raporlanır ve plan düzeltmesi gerekir: test botunun HP/MP'sini düşürmek için kapsam dışı bir yol gerekirse **durup** raporla).
- Telemetri hacmi küçüktür (pot başına ≤ 2 olay); `droppable = false`, `IsEnabled` denetimi önceki aksiyonlarla aynı.
- `list` satırı biçimi (`mp=` sonrası yeni alan) F3-04 yanıtını da etkiler; mevcut alan adlarını ve sırasını **değiştirme**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Branch / son commit: `bot/F4-04` (taban: `gece/2026-10-02`); kod commit'i `b728b6b` (+ bu rapor/Durum commit'i).
- Yapılanlar:
  - `BotCore/BotCombat.h`: pot dilimi saf mantığı eklendi (`kPotCooldownMs = 2500`, `PotSupported`, `PotionCheck`, `PotionVerdict`, `PotionWaitMs`, `CheckPotion`). Mevcut içerik/`#include`'lar değişmedi (yalnızca `<algorithm>`, `<cstdint>`).
  - `Tests/BotCoreTests/CombatTests.cpp`: `Combat_PotCheck_Order` ve `Combat_PotWait` eklendi (mevcut makro stili). Toplam test 24 → 26.
  - `GameServer/Bot/ActionExecutor.h/.cpp`: `PotionOutcome`; `CountInBag` (yalnızca çanta 14..41), `SubmitPotion` (tek `WIZ_MAGIC_PROCESS`/`MAGIC_EFFECTING`, caster = target = kendi kimliği, `m_castEcho`'dan sonuç), `RejectPotion` (`CLI-06 no_stock`/`pot_cooldown`, `CLI-11 rate`), `BeginPotion`, `TickPotion`, `EndPotion`. Pot, tip-3 ve genel cast zamanlayıcılarını besler; onlardan beklemez.
  - `GameServer/Bot/BotSession.h/.cpp`: pot serisi üyeleri + ortak zaman (`m_potHasLast`/`m_potLast`); başlatıcı listesi ve `ResetForRespawn()`. `OnPacket()` **değişmedi**.
  - `GameServer/Bot/BotManager.h/.cpp`: `CommandPot` bildirimi ve gövdesi; `ExecuteCommand` `pot` fiili + unknown listesi; `TickSessions()` `PHASE_IN_GAME` pot bloğu (cast bloğunun ardından, aynı seviyede); `BeginDespawn()` `EndPotion`; `BuildStatusLines()` `pot=`.
  - `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` değişmedi (yeni dosya yok).
- Çalıştırılan komutlar ve çıktıları:
  - `./tools/build.sh Release`: rc=0; `proj-GameServer.vcxproj -> ...\GameServer.exe`; yeni dosyalarda uyarı yok (yalnızca eski `GameServerDlg.cpp` C4834/C4267 ve `UpgradeHandler.cpp` C4789).
  - `./tools/build.sh Debug`: rc=0; `proj-GameServer.vcxproj -> ...\GameServer.exe`.
  - `./tools/run-tests.sh Release`: `26 tests, 0 failed` (`[ OK ] Combat_PotCheck_Order`, `[ OK ] Combat_PotWait`).
  - `./tools/run-tests.sh Debug`: `26 tests, 0 failed`.
  - K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` → eşleşme yok; `#include` yalnızca `<algorithm>`, `<cstdint>`.
  - K5: `WIZ_MAGIC_PROCESS` yalnızca `ActionExecutor.cpp` (SubmitCast `:592`, SubmitPotion `:1005`) ve `BotSession.cpp` (`OnPacket` `:46`). Yasak çağrılar (`MagicPacket`/`MagicInstance`/`m_CoolDownList`/`m_MagicTypeCooldownList`/`MSpChange`/`HpChange`/`RobItem`/`GiveItem`) yalnızca `#include "../MagicInstance.h"` ve açıklama yorumlarında; **çağrı yok**.
  - K6: `TickPotion` önce `stock < 1` (`:1191`), sonra `PotionWaitMs`, sonra `CheckPotion` (`:1197`) sonra `SubmitPotion`/`HandlePacket` (`:1013`); pot `HandlePacket(pkt)` tek yerde (`SubmitPotion`).
  - K7: `CountInBag` `INVENTORY_INVENT..INVENTORY_INVENT + HAVE_MAX - 1` (14..41); `grep m_sItemArray GameServer/Bot/*.cpp` boş.
  - K8: `git diff gece/2026-10-02...bot/F4-04 -- BotManager.cpp | grep '^-'` yalnızca 3 bilinçli satır (unknown command mesajı, `BuildStatusLines` biçimi/argümanı); `OnPacket()` değişmedi.
  - K9: `git diff --stat` yalnızca §4'teki 8 dosya; vcxproj diff boş.
  - K10: `file` CRLF; `BotCombat.h`/`CombatTests.cpp` ASCII+CRLF; `git diff --check` temiz.
  - K11: `grep printf|Sleep|lock_guard|mutex|CreateThread|rand(` `ActionExecutor.*` boş.
  - K12: `CheckMoveStep` 2, `CheckAttack` 1, `CheckCastStart` 1; önceki 24 test geçiyor; `EmitFairnessReject` `"Move"`/`"Attack"`/`"Cast"` çağrıları yerinde (yeni `"Potion"` eklendi).
- Sapmalar / sorular:
  - Plandaki "sonraki cast zamanlayıcılarını güncelle" adımında paket `WIZ_MAGIC_PROCESS` yalnızca `SubmitPotion`'da oluşturulur (planla uyumlu); sapma yok.
  - Çalışma zamanı doğrulaması (§7, K13) Claude'a aittir; yapılmadı.
- Kabul kriterleri K1–K12 durumu: K1 ✔, K2 ✔, K3 ✔ (26 test, iki yeni ad), K4 ✔, K5 ✔, K6 ✔, K7 ✔, K8 ✔, K9 ✔, K10 ✔, K11 ✔, K12 ✔. (K13 Claude doğrulamasında.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- **Karar: DOĞRULANDI.** İncelenen commit: `76053ef` (`bot/F4-04`, taban `gece/2026-10-02` @ `dbb8f18`; kod commit'i `b728b6b`). Otonom gece modu (`AUTO_LOOP=1`): birleştirme ve push yapılmadı, birleştirmeyi döngü betiği yapar. Çalışma ağacı temizdi.
- Özet: 13/13 kriter ✔. Kod plan §5 ile birebir uyumlu; Release/Debug rc=0, değişen dört `.cpp` touch'lanıp yeniden derlendi (Release), derleyici uyarısı 0; `26 tests, 0 failed` (Release + Debug). Çalışma zamanı senaryoları S1–S7 gerçek sunucuda geçti; uygulayıcı raporundaki iddialar doğru çıktı.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 Release derleme | ✔ | `build.sh Release` rc=0; `ActionExecutor.cpp`, `BotManager.cpp`, `BotSession.cpp`, `CombatTests.cpp` touch'lanıp yeniden derlendi, çıktıda `warning`/`error` satırı yok |
| K2 Debug derleme | ✔ | `build.sh Debug` rc=0, `BotCoreTests.exe` üretildi |
| K3 Testler | ✔ | `run-tests.sh Release`/`Debug`: `26 tests, 0 failed`; `Combat_PotCheck_Order`, `Combat_PotWait` mevcut ve geçiyor (24 eski test dahil) |
| K4 `BotCombat.h` bağımsız | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/"` boş; `#include` yalnızca `<algorithm>`, `<cstdint>`; fark yalnızca ekleme (`BotCombat.h` +48) |
| K5 Paket yolu | ✔ | `WIZ_MAGIC_PROCESS` paket oluşturma yalnızca `ActionExecutor.cpp:592` (`SubmitCast`) ve `:1005` (`SubmitPotion`); `BotSession.cpp:46` `OnPacket` okuma. Yasak çağrı grep'i: yalnızca `#include "../MagicInstance.h"` ve iki açıklama yorumu, **çağrı yok** |
| K6 Guard atlanmıyor | ✔ | `TickPotion`: `stock < 1` → `RejectPotion` (`:1190`), `PotionWaitMs > 0` → `NOTHING`, `CheckPotion != POT_OK` → `RejectPotion` (`:1197`), sonra `SubmitPotion`; pot `HandlePacket` tek yerde (`:1013`). Çalışma zamanında da sınandı (S3: `ACTION_SUBMIT` yok) |
| K7 Çanta okuması | ✔ | `CountInBag` `INVENTORY_INVENT`..`INVENTORY_INVENT + HAVE_MAX - 1` (14..41), yalnızca `GetItem`; `grep m_sItemArray GameServer/Bot/*.cpp` boş |
| K8 `ENABLED=0` değişmez | ✔ | `BotManager.cpp` `grep '^-'`: yalnızca 3 bilinçli satır (`unknown command` metni, `BuildStatusLines` biçim ve argüman satırları); `Startup()`/`Tick()`/ini okuma ve `OnPacket()` değişmedi. Çalışma zamanında: `ENABLED=0` ile komut dosyası verildi → `Bot_*.log` +0 satır, `Logs/bots` oluşmadı, `GameServer.log` 32→32 |
| K9 Dosya kapsamı | ✔ | `git diff --stat`: 8 dosya + plan; `proj-GameServer.vcxproj*`, `BotCore.vcxproj`, `BotCoreTests.vcxproj` farkı boş; `docs/`, `CLAUDE.md`, `AGENTS.md`, başka plan değişmedi |
| K10 Kodlama / satır sonu | ✔ | 8 dosya `ASCII text, with CRLF` (depoda LF, çalışma ağacında CRLF; `git ls-files --eol` diğer dosyalarla aynı); `git diff --check` rc=0, boş |
| K11 Yasaklı kalıplar | ✔ | `grep printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(` `ActionExecutor.*` boş |
| K12 Gerilemesiz | ✔ | `CheckMoveStep` 2, `CheckAttack` 1, `CheckCastStart` 1; `EmitFairnessReject` `"Move"` (`:97`, `:190`), `"Attack"` (`:402`), `"Cast"` (`:560`), yeni `"Potion"` (`:977`); 24 eski test geçiyor; çalışma zamanı S7 (aşağıda) |
| K13 Çalışma zamanı | ✔ | S1–S7, aşağıda |

- Çalışma zamanı sınaması (Release, `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `BotWP_K`/`BotMF_K`/`BotWP_E`, zone 71; ini'ye geçici `[BOT]` eklendi, eski `Logs/bots` `bots_old_f404`'e taşındı):
  - **S1 tüketilen HP potu:** `pot BotWP_K 389015000 3` → `using 389015000 (3 use(s))`; üç `ACTION_SUBMIT` (`kind:"hp"`, `skill:490015`, `stock` 100/99/98) → `ACTION_RESULT` (`effected`, `op:3`, `stock_after` 99/98/97); aralıklar 2528 ve 2508 ms (≥ 2500, ≤ 2750); `FAIRNESS_REJECT` yok; `pot finished (effected) after 3 use(s), 3 ok`; `latency_us` 113–134; bot HP'si zaten doluydu (5650/5650) ve sunucu yine de `op:3` yayınladı: **`ExecuteType3` HP/MP dolu botta da paket yayınlıyor** (plan §8 `[A]` varsayımı doğrulandı); `list` sonuçta `pot=0`.
  - **S2 MB-01 potları:** `389014000 ×3` → üç `effected`, `stock`/`stock_after` hep **1** (K-5: sayı azalmıyor), aralıklar 2507/2518 ms. `389020000 ×2` (`kind:"mp"`, `skill:490020`) → iki `effected`, `stock_after` 1, aralık 2515 ms. HP→MP geçişi (1,2 sn arayla verilen iki komut): 2508 ms (**ortak zamanlayıcı**), `FAIRNESS_REJECT` yok.
  - **S3 çantada olmayan pot (CLI-06, AC-SUR-04):** `pot BotWP_K 389019000` (`UseItem 0`, çantada yok) → `using` (arma), ilk tick'te `pot stopped (no_stock)`; JSONL'de yalnızca `FAIRNESS_REJECT` (`"type":"Potion","rule":"CLI-06","reason":"no_stock","value":0.00,"limit":1.00`), **`ACTION_SUBMIT` yok**, oturum kopmadı.
  - **S4 bekleme/etkileşim:** (a) iki `pot` komutu 1 sn arayla → ikinci pot ilkten 2512 ms sonra gitti, `FAIRNESS_REJECT` yok. (b) `pot BotMF_K 389015000 1` ardından `cast BotMF_K 110518 BotWP_E 1` → `CastStart` potun `ACTION_SUBMIT`'inden 1102 ms sonra (≥ 1000), cast `effected`, `srv_fail` yok. (c) `pot` + `attack BotWP_K BotWP_E 3` aynı dosyada → pot `effected`, saldırı `3 hit(s) sent, 3 ok`, `rate` reddi yok.
  - **S5 reddedilen komutlar:** `pot BotWP_K 1` → `refused (bad_item)`; `379006000` (`Effect1 0`) → `refused (bad_item)`; `389059000` (`ReCastTime 150`), `310310010` (`ReCastTime 250`), `379105000` (`Moral 5`) → `refused (unsupported_item)`; `pot Ghost 389015000` → `unknown or not spawned bot '?'`; `... 0`, `... 21`, `... abc`, `pot`, `pot BotWP_K`, `pot BotWP_K 0` → kullanım satırı; `pot all off` → `not potting` ×3, `0 stopped, 3 not potting`; `pot BotWP_K off` boşta → `not potting`; **`off` ortada** (`... 5`, ~3,3 sn sonra `off`) → `stopped after 2 use(s)`, sonraki 7 sn'de yeni paket yok (JSONL satır sayısı 80, değişmedi). Despawn edilmiş botla → `not in game (phase despawned)`. **`RESPAWN_CYCLES=2` iken `cmd rejected` yeniden sınanmadı** (kod yolu bu planda değişmedi, `ExecuteCommand` reddi fiil dağıtımından önce; F4-03 ile aynı kapsam).
  - **S6 ömür döngüsü:** seri sürerken (`... 5`, 2 pot sonrası) `despawn BotWP_K` → `despawned (... names cleared yes)`, `pool free 14/16`, sonrası yeni `ACTION_SUBMIT` yok; yeniden `spawn BotWP_K` → `pot=0`, ilk pot hemen gitti (ortak pot zamanı sıfırlandı), `stock` **90** (3+2+1+2+2 = 10 pot düştü, kayıt logout'ta DB'ye yazıldı) → `stock_after` 89.
  - **S7 gerilemesiz:** `attack ... 3` → `3 hit(s) sent, 3 ok`; `cast BotMF_K 110518 BotWP_E 2` → `cast finished (effected) after 2 cycle(s), 2 ok, 4 packet(s) sent`; 30 m `move` → `arrived ... after 5 packets`; `PERF_SAMPLE` `tick_p95_us` 108/112/112 (≤ 1 ms), `skipped_ticks` 0; `TELEMETRY=summary` ile yeniden başlatıp `pot BotWP_K 389015000 2` → `pot finished (effected) after 2 use(s), 2 ok`, JSONL'de `ACTION_*` **0**; sunucu 3/3 UP, `GameServer.log` 32→32 (yeni hata yok).
  - **Temizlik:** sunucular kapatıldı (`run-servers.sh stop`); `GameServer.ini` yedekten geri yüklendi (md5 `d16463283c0d41074a2d8b6ec4aee203`, öncekiyle aynı; **not:** sunucu `[BOT]` yoksa kapanışta kendisi `[BOT] ENABLED=0` ekliyor, mevcut davranış, bu planın işi değil); `BotCommands.*` kalmadı; 12 bot satırı `sqlcmd -v Upgrade=7 -i db/002_bot_characters.sql` ile geri yüklendi (rc=0, `BotWP_K` çantası 100'e döndü; kişisel veri tablosu okunmadı); test artıkları depo dışında (`Logs\bots_old_f404\`, `Logs\bots_f404_run2\`).

- Bulgular (önem sırasına göre; hiçbiri engel değil):
  1. **[Not]** `GameServer/Bot/ActionExecutor.cpp:1063` `BeginPotion` içinde kullanılmayan `now` parametresi `(void)now;` ile bastırılmış (F4-02/F4-03 kalıbı; plan imzayı bu biçimde istiyor).
  2. **[Not]** `GameServer/ChatHandler.cpp:1197-1198` `+bot` yardım metni `cast` ve `pot` fiillerini de listelemiyor (kapsam dışı, KI-012 kapsamı genişletildi).
  3. **[Not]** Plan §7 S4(b) için komut dosyası yoklaması saniyelik: iki komutu 1 sn'den kısa arayla vermek ilkini ezer (ilk deneme 0,4 sn aralıkla ezildi, 1,05 sn aralıkla geçti). Bir ürün hatası değil; test notu olarak not düşüldü.
  4. **[Not]** Hâlâ `[A]`: HP/MP ortak zamanlayıcı varsayımı (Q-06, T-MECH-POT-03). Bu plan ayrı zamanlayıcıyı ölçmez; ortak 2500 ms muhafazakâr taraftadır.
- Uygulayıcı raporu doğrulandı: commit listesi, 8 dosya, derleme/test çıktıları ve grep sonuçları gerçekle uyuşuyor. Sorulan soru/sapma yok.
- Kalan (insan testi, kriteri engellemez): T-ARCH-09 (`docs/STATUS.md` "Proje sahibi testleri"): gerçek istemcide pot efektleri.

