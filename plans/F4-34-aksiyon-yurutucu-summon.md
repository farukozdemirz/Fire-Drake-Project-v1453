# F4-34: `ActionExecutor` summon dilimi — summon friend (Type8, `Moral` 4, `WarpType` 12)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-34` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-33 (diriltme istisnası kalıbı: `BeginCast` ayrı istisna + `wantedTarget`) — `KAPANDI` (merge `184beba`); F4-31 (`Moral` 4 party hedefli, MEC-MAG-18) — `KAPANDI`; F4-08 (party kurulumu `/bot pinvite`, `/bot paccept`; yalnızca çalışma zamanı sınaması için) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-11, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-18, MEC-MAG-21 (bu planla eklendi, `[D]`), MEC-T8-01/-03, SK-06, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 Ek 1 madde 1(c) (summon), Ek 10; `docs/08` §8 (summon akışı), REQ-MAG-04/-05, T-IGT-MAG-01 altyapısı |
| Tahmini büyüklük | S (4 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.cpp`, `ActionExecutor.h`; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

Mage'in "respawn → summon" akışı (`docs/08` §8: üye ölür, kendi üssünde dirilir, toparlanır, mage onu **summon friend** ile yanına çeker) botla yapılamıyor. **summon friend** `110004` (ve El Morad karşılığı `210004`) `MAGIC.Type1 = 8` (warp/summon ailesi), `Moral` 4 (`MORAL_PARTY`), `MAGIC_TYPE8.WarpType` 12 ("aynı zone'daki hedefi çağıranın yanına ışınla") skill'idir. `BeginCast` bunu `CastTypesSupported(8, 0) == false` yüzünden `unsupported_skill` ile reddediyor (`ActionExecutor.cpp:748`). F7 (party koordinasyonu, mage `SUMMON` alt durumu, `docs/08` §13) bunsuz yazılamaz.

Sunucu yolu F4-31'in `Moral` 4 yolundan ve F4-33'ün diriltme yolundan **üç noktada farklıdır** ve plan bunu açıkça kapsar:

1. **Hedef (ışınlanan) çağrılan üyedir, çağıran değil:** `ExecuteType8` tek hedefli dalda hedefi `GetUserPtr(sTargetID)` ile alır; `WarpType 12` hedefi **çağıranın konumuna** ışınlar (`pTUser->Warp(pSkillCaster->GetSPosX(), GetSPosZ())`, `MagicInstance.cpp:2355-2373`). Çağıran kendini hedef alırsa (`pSkillCaster == pTUser`) sunucu sessizce `bResult = 0` ile döner ama **MP yine düşer**; bot kendine summon atamaz (`bad_target`, `ActionExecutor.cpp:772`'deki kural genişler).
2. **Sonuç yayını başarıyı kanıtlamaz:** hedef ölü, başka zone'da, ışınlanması engelli (`canTeleport()`) ya da zaten ışınlanıyorsa (`m_bWarp`) sunucu `goto packet_send` ile **yine** çağırana hedef kimlikli EFFECTING yayını gönderir, yalnızca `sData[1] = 0` olur (başarıda 1). Botun `m_castEcho` yalnızca `op` + `sData[3]` + skill kimliğini saklar (`BotSession.cpp:79-85`); yani **`effected` summon'un gerçekleştiğini göstermez** `[D]`. Gerçek sonuç hedefin konumudur (`list`); karar katmanı sonucu `PerceptionSnapshot`/olay halkasından çıkarır (`SkillEvent.data[1]`, F4-52).
3. **Güvenlik kapıları bota eklenmez:** `docs/08` §8.1 SUM-01..07 (hedef yaşıyor mu, çevrede düşman var mı, mage HP, takım yenilgisi...) karar katmanının (F7) işidir. AC-LRN-03: hedefin ölü olup olmadığı başka botun durumudur, yürütücü önkontrol yapmaz; `Perception`/`TeamView` bu bilgiyi karar katmanına zaten verir. Bu plan yalnızca **atılabilirliği** sağlar (ADR-0018 Ek 1 madde 1(c)'nin aksiyon desteği kısmı; ADR-0018 Ek 10 kapıların F7'ye ait olduğunu kayda geçirir).

Bu plan:

1. `BotCore/BotCombat.h`'a `CastSummonSupported` saf mantık fonksiyonunu ekler;
2. `BeginCast`'e summon istisnasını ekler (`m_Magictype8Array` ile `WarpType == 12` doğrulanır: Gate/Escape/Blink/descent/Wild advent kapalı kalır) ve `bad_target` kuralını summon'u kapsayacak şekilde genişletir;
3. sunucu davranışını kodla belgeler ve çalışma zamanında doğrular (`docs/03` MEC-MAG-21).

F4'ün otuz dördüncü planıdır (ADR-0018 sırası: ... diriltme ✔ → **summon (bu plan, dilim 6c)** → Type8 warp/descent/Gate (6d) → `UseItem`'li skill'ler (6e) → CLI-12 → envanter doldurma).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-34)" (bu planla birlikte yazıldı), "Ek (F4-33)". `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (Ek 1 madde 1, Ek 9, Ek 10).
- `docs/03` §4.2 **MEC-MAG-18** (`Moral` 4), **MEC-MAG-20**, **MEC-MAG-21** (bu planla eklendi); §5.5 MEC-T8-01..03; `docs/05` mage tablosu (satır 155: `110004` summon friend); `docs/08` §2, §8 (summon akışı ve güvenlik koşulları).
- `plans/F4-33-aksiyon-yurutucu-diriltme.md` (aynı istisna kalıbı; **yazılı planı değil, kodu esas al**).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `184beba` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.cpp:831-847` `IsAvailable()` `MORAL_PARTY` (CASTING ve EFFECTING): NPC çağıran/hedef ⇒ `false`; çağıran party'de değilse hedef kendisinden başkası olamaz; hedef varsa `GetPartyID()` çağıranınkiyle aynı olmalı; aksi `MAGIC_FAIL`. `:284-286`: `IsAvailable()` hem CASTING hem EFFECTING'te çalışır, yani party dışı hedef **CASTING'te** `srv_fail` olur.
  - `GameServer/MagicInstance.cpp:352-358` `CheckSkillPrerequisites()`: hedef varsa `sRange > 0 && sUseStanding == 0 && mesafe >= sRange` ⇒ fail (summon friend `Range 22500` ⇒ pratikte hiç tetiklenmez); **çağıran güvenli bölgedeyse** (`isInSafetyArea()`, `nSkillID < 400000`) fail (botlar güvenli bölgede başlıyorsa her cast `srv_fail` verir: F4-29/F4-33 bulgusu).
  - `GameServer/MagicInstance.cpp:389-405`: tip kapısı yalnızca `bType[0]` 1..7 için (`PLAYER_SKILL_REQUEST_INTERVAL`); **Type8 kapıya girmez**. `:361-371`: skill başına yeniden-kullanım süresi `ReCastTime * 100` ms (summon friend `ReCastTime 1` ⇒ 100 ms).
  - `GameServer/MagicInstance.cpp:1028-1029`: `bType[0] != 4` iken `Msp` EFFECTING'te `IsAvailable()` içinde **bir kez, `ExecuteSkill`'den önce** düşer: summon başarısız olsa da (hedef ölü, kendisi...) MP gider `[D]`.
  - `GameServer/MagicInstance.cpp:2234-2458` `ExecuteType8()`: tek hedefli dal `:2260-2266` (`GetUserPtr(sTargetID)`, `nullptr` ⇒ `false`, yayın yok); `WarpType != 11` iken ölü hedef ya da `!canTeleport()` ⇒ `goto packet_send` (`:2278-2284`); `m_bWarp` ⇒ `goto packet_send` (`:2290`); `case 12` `:2355-2373`: Forgotten Temple'da ya da (`ZoneID > ZONE_BIFROST` ve `490042/490050`) `SendSkillFailed` + `false`; hedef çağıranla **aynı zone'da değilse ya da çağıranın kendisiyse** `goto packet_send`; aksi halde hedefe bölge yayını (`BuildAndSendSkillPacket(*itr, true, ..., sData[1] = 1)`), sonra `pTUser->Warp(...)`. `packet_send` `:2452-2455`: çağırana (bölge yayını) hedef kimlikli EFFECTING, `sData[1] = bResult`. `return true` ⇒ `Process()` yeniden-kullanım damgalarını ekler (`:111-126`).
  - `GameServer/CharacterMovementHandler.cpp:626-654` `CUser::Warp(x, z)`: **`m_bWarp` bayrağını kurmaz** (yalnızca `ZoneChange` kurar); `WIZ_WARP` hedefe gider (bot alıcısına düşer), `UserInOut(INOUT_OUT)`, bölge güncellemesi, `UserInOut(INOUT_WARP)`, `UserInOutForMe`/`NpcInOutForMe`. Bot için ek bir istemci onayı (`WIZ_ZONE_CHANGE`) **gerekmez** (bu nedenle bot `m_bWarp = true` ile takılı kalmaz). F4-07 `Regene` aynı `Warp`'ı bot oturumunda zaten kullanıyor.
  - `GameServer/Bot/BotSession.cpp:72-86`: çağırana gelen `WIZ_MAGIC_PROCESS` yanıtı `m_castEcho`'ya (op, `sData[3]`, skill) yazılır; `OnPacket()` **değişmez**. Hedef bota giden yayın `caster != m_castSelfId` olduğundan echo'yu bozmaz. Kodda `WIZ_WARP` için bot tarafı işleyici yoktur (`grep -n WIZ_WARP GameServer/Bot` boş): ışınlanan botun `Perception` tabloları bu plan kapsamında değişmez (bkz. §8 (d)).
  - `shared/database/structs.h:164-172` `_MAGIC_TYPE8 { uint32 iNum; uint8 bTarget; uint16 sRadius; uint8 bWarpType; uint16 sExpRecover; uint16 sKickDistance; }`; sunucuda `g_pMain->m_Magictype8Array.GetData(id)` (`MagicInstance.cpp:2239`, `GameServerDlg.h:364`). `ActionExecutor.cpp` aynı kalıpla `m_Magictype3Array`/`m_Magictype5Array` okur: oyun verisi tablosu, oyuncu durumu değildir; `tools/check-perception-contract.py` R1/R2 listelerinde değildir.
  - `GameServer/Bot/ActionExecutor.cpp` (`184beba`): `BeginCast` destek koşulu `:733-758` (`resurrection` bayrağı `:736-744`, `if (!resurrection && (...))` `:747`); `bad_target` kuralı `:770-778` (`wantedSelf`, `wantedTarget = BotCore::CastNeedsOtherTarget(m->bMoral)`); `TickCast` (`:794-`): `area = SendsAimPoint(m->bMoral)` (`Moral` 4 ⇒ `false`, tek hedefli paket), `typeGated` yalnızca `IsGatedType` (1..7) için (`:875-876`, Type8 ⇒ `false`), tip damgaları `ty < 8` (`:882`, `:1027`: Type8 damga yazmaz); **değişmez**.
  - `BotCore/BotCombat.h:322-336` (`CastTypesSupported`, `CastTypeMoralSupported`), `:369-375` (`CastMoralSupported`: `Moral` 4 zaten açık), `:381` (`CastHpCostSupported`), `:398-408` (`CastResurrectionSupported`, `CastNeedsOtherTarget`), `:424` (`IsGatedType`); `Tests/BotCoreTests/CombatTests.cpp` son test `Combat_ResurrectionCast_Guard` (`:1433-1498`); `Combat_CastTypes_Supported` içindeki `CastTypesSupported(8, 0) == false` satırı (`CombatTests.cpp:1031`) **değişmez** (summon bu fonksiyondan geçmez, `BeginCast`'te ayrı istisnadır). Toplam birim test **110**.
- **Veri notu (Claude yerel `MAGIC`/`MAGIC_TYPE8` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** `MAGIC.Type1 = 8` ve `MAGIC_TYPE8` kaydı olan, `MagicNum < 300000` skill'ler ve `WarpType`'ları:
  - **`WarpType 12` (summon, bu planın tek hedefi):** `110004` ve `210004` summon friend (`Skill 1100`/`2100` ⇒ sınıf 110/210, `SkillLevel 4`, `Moral 4`, `Msp 5`, `CastTime 15`, `ReCastTime 1`, `Range 22500`, `UseItem 0`, `Etc 0`, `BeforeAction 0`, `UseStanding 0`, `FlyingEffect 0`, `HP 0`, `MAGIC_TYPE8.Radius 10000`, `Target 1`); `109004` ve `209004` aynı değerlerle novice mage sınıfı 109/209 (`Skill 1090`/`2090`). Botlar master sınıftadır (110/210): `109004`/`209004` `bad_skill` alır (`ActionExecutor.cpp:725-731`).
  - Açılmayacaklar: `WarpType 1` Gate `110015`/`109015`/`111700`/`112700` (`Moral 1`) ve Escape `110035`/`109035` (`Moral 6`), `WarpType 20` Blink `110774` (`Moral 1`, `SkillLevel 80`), `WarpType 25` descent `105650`/`106650` (`Moral 4`, `Msp 50`) ve Wild advent `108770` (`Moral 7`, `Etc` yok, `UseStanding 52`); El Morad karşılıkları. Hepsi `unsupported_skill` kalır.
- **Bot karakter notu (`db/002_bot_characters.sql:124-137`).** `BotMF_K`/`BotMI_K` (sınıf 110), `BotMF_E`/`BotMI_E` (sınıf 210), seviye 80: `110004`/`210004` summon friend `SkillLevel 4 <= 80`; `Skill 1100` için `sSkill % 10 == 0` olduğundan skill ağacı denetimi **yoktur** (`MagicInstance.cpp:956-963` yalnızca `modulator != 0`; `m_bstrSkill[0]` kullanılmaz, KI-016 burada etkisiz). Her botun `Hp = Mp = 32000` (`db/002`), Msp 5 sorun değildir.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `kMoralPartyMember`, `kType8WarpSummon` sabitleri ve `CastSummonSupported(type0, type1, moral, useItem, warpType)` (§5.1).
2. **`BeginCast` (`ActionExecutor.cpp`):** (a) summon istisnası: `bType[0] == 8` iken `m_Magictype8Array` kaydı okunur, `CastSummonSupported(...)` + `bFlyingEffect == 0` + `CastHpCostSupported(sHP)` sağlanırsa destek koşulu atlanır (diğer tüm skill'ler için koşul **aynen** kalır); (b) `wantedTarget = BotCore::CastNeedsOtherTarget(m->bMoral) || summon` (§5.3).
3. **Yorumlar (`ActionExecutor.h`):** `BeginCast` açıklaması summon'u kapsar (§5.3).
4. **Birim testleri:** yeni `Combat_SummonCast_Guard` (110 → 111).
5. **Sonuç sözleşmesi (§5.4):** `docs/03` MEC-MAG-21 çalışma zamanında sınanır.

**Kapsam dışı (yapılmayacak)**

- Type8'in diğer aileleri: Gate/Escape (`WarpType 1`), Blink (`20`), descent/Wild advent (`25`), canavar çağırma (`21`), zone'lar arası summon (`13`), `WarpType 2/3/5`, hedef `-1` grup yolu (`Moral` 6 Escape: `UserRegionCheck` + respawn sonrası 180 sn kuralı) — ADR-0018 Ek 1 madde 1(d), dilim 6d.
- **Karar katmanı ve güvenlik kapıları:** kimin summon edileceği, hedefin canlı/toparlanmış/party'de/ışınlanabilir olup olmadığı, çevrede düşman, mage HP, "yürüyerek katıl" geri dönüşü (`docs/08` §8.1 SUM-01..07, F7). Bota **hedefin ölü, party üyesi, aynı zone'da ya da ışınlanabilir olup olmadığı önkontrolü eklenmez** (AC-LRN-03); bunlar sunucuda CASTING'te `srv_fail` (party dışı) ya da `sData[1] = 0` ile `effected` (diğerleri) olarak görünür.
- **`sData[1]` (summon sonucu) okuma:** `BotSession::OnPacket()`/`m_castEcho` düzeni değişmez; sonucu karar katmanı olay halkasından çıkarır (F7 planı gerekirse genişletir).
- Işınlanan botun `Perception` tablolarının `WIZ_WARP` sonrası yenilenmesi (§8 (d)): ölçüm notu olarak kalır, kod değişikliği yok.
- Yeni komut/ini anahtarı/telemetri olayı türü/alan, `BotSession.*`/`BotManager.cpp` değişikliği, `tools/` betik değişikliği (özellikle `check-perception-contract.py` istisna listeleri).
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | yeni sabitler + `CastSummonSupported` (mevcut fonksiyonlar değişmez) |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | yeni `Combat_SummonCast_Guard` dosyanın sonuna (110 → 111); mevcut testler değişmez |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yalnızca `BeginCast`: summon istisnası (destek koşulu çevresi) ve `wantedTarget` satırı |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca `BeginCast` yorumu |

`BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `ScenarioRunner.*`, `ScriptRunner.*`, `tools/` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`CastNeedsOtherTarget`'tan (`:405-408`) hemen sonra, `CastTargetIdField` öncesine şu bloğu ekle (ASCII, CRLF, tab, `namespace BotCore` içinde; yeni `#include` yok; **`std::min`/`std::max` kullanma**). Önce `grep -n "kMoralPartyMember\|kType8WarpSummon" BotCore` ile adların boş olduğunu doğrula (çakışırsa durup raporla):

```cpp
	// --- summon cast (ADR-0017 Ek F4-34, docs/03 MEC-MAG-21) ---

	// MAGIC.Type1 = 8 is the warp/summon family. MAGIC_TYPE8.WarpType 12 = "summon a target within the zone": summon friend
	// (110004/210004), MAGIC.Moral 4 = PARTY, so the target must be a member of the caster's party. The target is teleported
	// to the CASTER; the caster may not name itself. The other warp types (1 Gate/Escape, 13 cross-zone summon, 20 Blink,
	// 21 monster summon, 25 descent/Wild advent) stay closed (ADR-0018 Ek 1 madde 1d).
	constexpr uint8_t kMoralPartyMember = 4;
	constexpr uint8_t kType8WarpSummon = 12;

	// The summons the bot casts: Type8 alone, Moral 4, no item, MAGIC_TYPE8.WarpType 12. The caller still rejects flying
	// effects and "sacrifice" HP costs.
	inline bool CastSummonSupported(uint8_t type0, uint8_t type1, uint8_t moral, uint32_t useItem, uint8_t warpType)
	{
		return type0 == 8 && type1 == 0 && moral == kMoralPartyMember
			&& useItem == 0 && warpType == kType8WarpSummon;
	}
```

Dosyanın kalanı **değişmez** (`CastTypesSupported(8, 0)` `false` kalır, `CastNeedsOtherTarget` aynen, `IsGatedType(8)` `false`). `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

Yeni **`Combat_SummonCast_Guard`** (dosyanın sonuna, `Combat_ResurrectionCast_Guard`'dan sonra; summon friend `110004`: `Msp 5`, `CastTime 15`, `ReCastTime 1`, `Range 22500`):

- Destek kapısı: `CastSummonSupported(8, 0, 4, 0, 12) == true`; `false` olanlar (her biri tek fark): `(5, 0, 4, 0, 12)`, `(3, 0, 4, 0, 12)`, `(8, 4, 4, 0, 12)`, `(8, 0, 1, 0, 12)`, `(8, 0, 2, 0, 12)`, `(8, 0, 6, 0, 12)`, `(8, 0, 7, 0, 12)`, `(8, 0, 4, 379006000, 12)`, `(8, 0, 4, 0, 1)` (Gate/Escape), `(8, 0, 4, 0, 13)`, `(8, 0, 4, 0, 20)`, `(8, 0, 4, 0, 21)`, `(8, 0, 4, 0, 25)` (descent), `(8, 0, 4, 0, 0)`.
- Mevcut fonksiyonlar değişmez: `CastTypesSupported(8, 0) == false` (summon bu fonksiyondan geçmez), `CastMoralSupported(4) == true`, `CastNeedsOtherTarget(4) == false` (Moral 4 party buff'ları `self` kabul etmeye devam eder; summon'un hedef kuralı `BeginCast`'tedir), `IsGatedType(8) == false`.
- Paket alanları (tek hedefli yol): `SendsAimPoint(4) == false`; `CastTargetIdField(SendsAimPoint(4), 2986) == 2986`; `CastCoordField(SendsAimPoint(4), false, 12.7f) == 12`.
- Başlangıç guard'ı (`CastStartCheck c = {}`; alanlar `Combat_ResurrectionCast_Guard`'daki gibi, `c.skillRange = 22500`, `c.msp = 5`, `c.reCastMs = BotCore::CastRecastMs(1)`, `c.typeGated = false`, `c.mana = 5`, `c.distanceM = 300.0f`, `c.standing = true`, `c.needsStanding = false`, `c.actionsInWindow = 0`): `CastRecastMs(1) == 100`; `CheckCastStart(c) == CAST_OK` (menzil fiilen sınırsız: 300 m kabul); `c.mana = 4` ⇒ `CAST_REJECT_NO_MANA`; `c.mana = 5; c.hasSkillLast = true; c.sinceSkillLastMs = 99` ⇒ `CAST_REJECT_RECAST`; `c.sinceSkillLastMs = 100` ⇒ `CAST_OK`.
- Enum sabit adları `BotCombat.h`'dekiyle birebir (`CAST_OK`, `CAST_REJECT_NO_MANA`, `CAST_REJECT_RECAST`); farklıysa dosyadaki adı kullan. Test sayısı **111**.

### 5.3 `ActionExecutor.cpp` ve `ActionExecutor.h`

**a) `BeginCast` destek koşulu (`ActionExecutor.cpp:733-758`).** Yalnızca bu bloğu değiştir; `resurrection` bloğundan sonra, `flyingCast` satırından önce summon bayrağını hesapla ve mevcut `if`'in `!resurrection` ön koşulunu `!resurrection && !summon` yap (mevcut altı koşul **olduğu gibi** kalır):

```cpp
	// ADR-0017 Ek F4-34: a summon (Type8, Moral 4, MAGIC_TYPE8.WarpType 12: summon friend) is a supported skill although
	// MAGIC.Type1 = 8 is not in CastTypesSupported. Static game data only: the caller's class/level/quest checks above and
	// below still apply; the target must be a party member (server rule, answered on the wire as srv_fail).
	bool summon = false;
	if (m->bType[0] == 8)
	{
		_MAGIC_TYPE8 * t8 = g_pMain->m_Magictype8Array.GetData(skillId);
		summon = t8 != nullptr
			&& BotCore::CastSummonSupported(m->bType[0], m->bType[1], m->bMoral, m->iUseItem, t8->bWarpType)
			&& m->bFlyingEffect == 0
			&& BotCore::CastHpCostSupported(m->sHP);
	}
```

ve `if (!resurrection` satırını `if (!resurrection && !summon` olarak değiştir (parantez/girinti düzenini dosyadaki stile uydur: tab, Allman). `{ out.kind = REFUSED; out.reason = "unsupported_skill"; return out; }` gövdesi aynen kalır.

**b) `bad_target` (`:772`).** Yalnızca `wantedTarget` satırı:

```cpp
	bool wantedTarget = BotCore::CastNeedsOtherTarget(m->bMoral) || summon;
```

Böylece summon'da `self` (boş hedef adı) `bad_target` ile reddedilir; paket gitmez (sunucu aynı durumda MP'yi düşer ve `bResult = 0` döner). Diğer `Moral` 4 skill'leri (party buff'ları) `self` kabul etmeye devam eder; `wantedSelf` ve `if` satırı aynen kalır.

**c)** `TickCast`, `SubmitCast`, `CancelCast`, `RejectCast`, `OnPacket()` ve diğer her şey **değişmez**: summon tek hedefli yoldur (`area` `false`, hedef kimliği = çağrılan botun kimliği, koordinat hedefin konumu), tip kapısı Type8'i kapsamaz (`IsGatedType(8) == false`), MP guard'ı `mana >= Msp` (5) ister, yeniden-kullanım `CastRecastMs(1)` = 100 ms.

**d) `ActionExecutor.h` yorumu.** `BeginCast` yorumundaki destek listesine ekle: "summon (Type8, Moral 4, MAGIC_TYPE8.WarpType 12: summon friend; the target must be a party member other than the caster, the server teleports it to the caster; Gate, Escape, Blink, descent and other warp types stay unsupported; ADR-0017 Ek F4-34); a summon reports 'effected' when the server broadcasts it, which does not prove the target moved (docs/03 MEC-MAG-21)", ve `bad_target` açıklamasını "moral does not match the target kind; corpse-friend and summon need a named target" yap. Yalnızca yorum.

### 5.4 Sonuç sözleşmesi (`docs/03` MEC-MAG-21 ile aynı)

| Skill / durum | Sunucu | Bot sonucu |
|---|---|---|
| **summon friend** `110004`, hedef **canlı party üyesi**, aynı zone, çağıranla aynı party | CASTING: `IsAvailable()` `MORAL_PARTY` ✔ (`SendSkill(true)` bölgeye); EFFECTING: MP `Msp` (5) bir kez (`IsAvailable()` içinde), `ExecuteType8` `case 12`: hedefe bölge yayını (`sData[1] = 1`), hedef çağıranın konumuna `Warp`, çağırana hedef kimlikli EFFECTING (`sData[1] = 1`) | `casting` (`op 1`), sonra `effected` (`op 3`, `code 0`, `victims` alanı yok); hedef `list`'te çağıranın konumunda (≤ ~1 m), çağıran MP −5 |
| Hedef **ölü** party üyesi | CASTING geçer; EFFECTING: MP düşer, `ExecuteType8` ölü hedefte `goto packet_send` (`sData[1] = 0`), ışınlanma yok | `effected` (`code 0`) **ama hedef yerinde ve ölü**; MP −5 `[D]` |
| Hedef **party üyesi değil** (party'siz ya da başka party, düşman dahil) | `IsAvailable()` `MORAL_PARTY` ⇒ `fail_return` | CASTING'te `srv_fail` (`op 4`, `code -100`), MP düşmez |
| Hedef `self` / ad verilmedi | bot kuralı | `REFUSED` `bad_target`, paket gitmez |
| Hedef başka zone'da / `canTeleport()` false / zaten `m_bWarp` | `goto packet_send` (`sData[1] = 0`) | `effected`, hedef yerinde; MP düşer `[D]`, ölçülmedi |
| Forgotten Temple | `SendSkillFailed` + `false` | CASTING değil EFFECTING sonrası `srv_fail`; ölçülmedi, `[D]` |
| Çağıranda recast (`ReCastTime 1` = 100 ms) | bot guard (`CastRecastMs`) | ARMED'de beklenir (reddi `FAIRNESS_REJECT` yazılmaz); tip kapısı yok |
| CASTING'te `cast <bot> off` | iptal paketi hedef kimliği | `cancelled` (`op:4`, `code:-100`), MP düşmez, hedef ışınlanmaz |
| Gate `110015`, Escape `110035`, Blink `110774`, descent `106650`, Wild advent `108770` | — | `BeginCast` `unsupported_skill`, paket gitmez |
| `109004` (novice sınıfı 109), `209004` | — | `bad_skill`, paket gitmez |

`reason` metinleri, `CastOutcome::Kind` değerleri ve `TickCast` akışı bu tablo için **değişmez**; yeni çıktı yoktur.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (değişen dört dosya için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h`'yi içeren `.cpp`'ler de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastTypes_Supported`, `Combat_ResurrectionCast_Guard` ve `Combat_SummonCast_Guard` adlarını `[ OK ]` ile içerir ve toplam test sayısı **111**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-34 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş; `git diff ... -- BotCore/BotCombat.h | grep '^-' | grep -v '^---'` boş (yalnızca ekleme).
- [ ] K5: `grep -n "CastSummonSupported\|m_Magictype8Array" GameServer/Bot/ActionExecutor.cpp` tam iki satır verir (her biri bir kez); `grep -n "CastNeedsOtherTarget" GameServer/Bot/ActionExecutor.cpp` tek satır ve satır sonunda `|| summon`; `CastTypesSupported(m->bType[0], m->bType[1])`, `CastTypeMoralSupported(m->bType[0], m->bMoral)`, `CastMoralSupported(m->bMoral)`, `CastHpCostSupported(m->sHP)` ve `(m->bFlyingEffect != 0 && !flyingCast)` mevcut `if` içinde yerinde; `if (!resurrection && !summon` ifadesi var; `m->iUseItem != 0` koşulu yerinde (`grep -c "m->iUseItem != 0"` ≥ 2, F4-33'ten bu yana değişmedi).
- [ ] K6: gerilemesiz: `git diff gece/2026-10-02...bot/F4-34 -- GameServer/Bot/ActionExecutor.cpp` yalnızca `BeginCast` içinde hunk içerir (destek koşulu çevresi ve `wantedTarget` satırı; iki hunk, ya da bitişik kalırlarsa tek hunk); `TickCast`/`SubmitCast`/`CancelCast`/`RejectCast` gövdesinde hunk yok; `git diff ... --stat` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `tools/` değişmemiş.
- [ ] K7: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı türü ve alanı yok (`git diff ... | grep '^+' | grep 'Emit('` boş).
- [ ] K8: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); dört dosya CRLF kalır (ASCII); `git diff --check` boş.
- [ ] K9: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 110 testin tamamı hâlâ geçiyor.
- [ ] K10: `python3 tools/check-perception-contract.py` `RESULT: PASS` (`R1`..`R5` ihlal sayıları 0; denetlenen dosya sayısı değişmez; araç ve istisna listeleri değişmez).
- [ ] K11 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-34
git diff gece/2026-10-02...bot/F4-34 -- BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff gece/2026-10-02...bot/F4-34 -- GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotManager.cpp GameServer/Bot/Telemetry.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj tools
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -rn "CastSummonSupported\|CastNeedsOtherTarget\|CastTypesSupported" BotCore GameServer Tests
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff --check gece/2026-10-02...bot/F4-34
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; komut başına ≥ 1 sn; dosya saniyede bir okunduğu için "~500 ms" iptal elle yakalanamaz: iptal, `casting` yanıtı görülür görülmez gönderilir, F4-33 bulgusu). Botlar (hepsi zone 71, aynı noktada doğar; doğuşlar ≥ 3 sn arayla, KI-DEG-01): Karus mage **`BotMF_K`** (summon eden), **`BotWP_K`** (çağrılan party üyesi), **`BotWG_K`** (party dışı + descent denemesi), **`BotPHB_K`** (S7 gerilemesi), El Morad **`BotMF_E`** (öldürücü: `210518`, ~16 cast ≈ −320/cast; F4-32/F4-33 bulgusu; S2'de `BotWP_K`'yı öldürür). Önce `list` ile konum, HP/MP ve ölü durumunu denetle; botlar güvenli bölgede başlıyorsa her cast `srv_fail` verir (F4-29/F4-30/F4-32/F4-33 bulgusu: ölü bot `regene` ile doğuş noktasına taşınır, konumu DB'de kalır). MP/HP/konum `list`'ten, olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'tan. MP kesin değeri sunucu yenilemesiyle (kümeli +40, ~4-5 sn'de bir) karışır: MP'yi cast'ten hemen önce ve sonra `list` ile al, yenileme payını raporla. Bu dilimde ölçüm DB'ye ek olarak **konum** yazar (despawn kaydı); taş/envanter değişmez.

1. **S1 Summon friend (`110004`) ışınlar:** `spawn BotMF_K,BotWP_K,BotWG_K`; party kur: `pinvite BotMF_K BotWP_K` → `paccept BotWP_K`; `snap BotMF_K` `team` satırıyla üyeliği teyit et. `list` ile ortak başlangıç konumunu (x0, z0) al; `move BotWP_K <x0+30> <z0>` ile çağrılacak botu 30 m uzağa yürüt ve varışını `list` `pos=` ile teyit et (mesafe ≥ 25 m). `cast BotMF_K 110004 BotWP_K 1`: `CastStart` `ACTION_SUBMIT` **`"target":<BotWP_K kimliği>`** (`-1` değil) → `casting` `op 1`, `cast_ms` ≈ 1580; `CastEffect` → `effected`, `op 3`, `code 0`, **`victims` alanı yok**; log `cast finished (effected) after 1 cycle(s), 1 ok, 2 packet(s) sent`. Hemen `list`: `BotWP_K` konumu `BotMF_K` konumunun ≤ ~1 m'sinde; `BotMF_K` MP **−5** (bir kez; yenileme payı ≤ ~40). Gözlem (kabul koşulu değil): ışınlanan `BotWP_K`'nın `see`/`npcs`/`snap` çıktısını ışınlanmadan önce ve sonra al; eski bölgenin kayıtları kalıyor mu rapora yaz (§8 (d)). Bot günlüğünde `WIZ_WARP` kaynaklı hata yok; `BotWP_K` `in_game` kalır ve `move` komutuna yanıt verir (`m_bWarp` takılmadı: `move BotWP_K <x> <z>` ile 3 m yürüt, `arrived`).
2. **S2 Ölü hedef: `effected` ama ışınlanma yok:** `BotMF_K`'yı `move` ile `BotWP_K`'dan ≥ 25 m uzağa götür (`list` ile teyit); `cast BotMF_E 210518 BotWP_K 16` ile `BotWP_K`'yı öldür (`list` `hp=0`), ölü konumu kaydet. `cast BotMF_K 110004 BotWP_K 1`: `casting` → `effected`, `op 3`, `code 0` (CASTING `IsAvailable()` ölü hedefte geçer; EFFECTING'te `ExecuteType8` `goto packet_send`); `list`: `BotWP_K` **hâlâ ölü ve aynı konumda**; `BotMF_K` MP **−5** (`[D]`: MP summon başarısız olsa da düşer). Bu, plan §1 madde 2'nin (`effected` ≠ ışınlandı) ölçümüdür. Farklı sonuç (ör. `srv_fail`, `no_result`, MP düşmedi) bulgudur: doğrulayıcı nedenini çözümleyip raporlar, `docs/03` MEC-MAG-21 buna göre düzeltilir.
3. **S3 Party dışı hedef:** `BotWP_K` diriltilmiş ya da yenilenmiş olsun (taze `despawn`/`spawn`). `cast BotMF_K 110004 BotWG_K 1` (`BotWG_K` aynı noktada ama **party dışı**): CASTING'te `srv_fail` (`op 4`, `code -100`), MP değişmez (yalnızca yenileme), hedef yerinde; aynı sonuç El Morad party dışı hedef için (`cast BotMF_K 110004 BotMF_E 1`) ve party'siz çağıran için (taze oturumda `spawn BotMF_K,BotWP_K`, party yok: `cast BotMF_K 110004 BotWP_K 1` ⇒ `srv_fail`).
4. **S4 Bot kuralları:** `cast BotMF_K 110004 self 1` ⇒ `refused (bad_target)`, **paket gitmez** (`ACTION_SUBMIT` yok), MP değişmez. Menzil denetimi fiilen yok: hedef 30 m'de bile `FAIRNESS_REJECT` yazılmaz (S1 zaten 30 m'de geçti; `out_of_range` **beklenmez**).
5. **S5 İptal:** `BotWP_K` 30 m uzakta ve canlıyken `cast BotMF_K 110004 BotWP_K 1`, `casting` yanıtı görülür görülmez `cast BotMF_K off`: `cancelled` (`op:4`, `code:-100`), iptal paketi hedef kimliği `BotWP_K`, MP değişmez, `BotWP_K` ışınlanmaz.
6. **S6 Kapalı kalanlar ve recast:** `cast BotMF_K 110015 self 1` (Gate, `Moral 1`, `WarpType 1`), `110035 self 1` (Escape), `110774 self 1` (Blink, `SkillLevel 80`; sınıf/seviye geçerse `unsupported_skill`, geçmezse `bad_skill`: hangisi olduğunu raporla), `cast BotWG_K 106650 BotWP_K 1` (descent, `WarpType 25`) ⇒ `refused (unsupported_skill)`; `cast BotMF_K 109004 BotWP_K 1` ⇒ `refused (bad_skill)`; beşinde de paket gitmez. Recast: `cast BotMF_K 110004 BotWP_K 3` (üç ardışık cast): her biri `casting` + `effected`, ardışık EFFECTING'ler ≥ 1,5 sn arayla (cast süresi + boşluk; recast 100 ms bağlayıcı değil), `cast finished (effected) after 3 cycle(s), 3 ok, 6 packet(s) sent`, MP ≈ −15.
7. **S7 Gerilemesiz ve kapsam:** F4-33 `Moral` 25: `BotWP_K` öldür, `cast BotPHD_K 112733 BotWP_K 1` ⇒ `effected`, hedef dirildi (`CastNeedsOtherTarget(25)` değişmedi; `|| summon` yalnızca summon'da doğru); F4-32 `cast BotPHD_K 112525 self 1` `effected`, `code 0`; **`Moral` 4 `self` hâlâ serbest:** `cast BotPHB_K 112606 self 1` (Grace, `Moral` 4, `{4, 0}`, `BotPHB_*` `strSkill[6] = 0x3E`: 1126 ağacı) `bad_target` **değil** (CASTING + `effected`, `code` = süre; party içi/dışı farkı raporla); F4-28 `cast BotPHB_K 112603 self 1` `effected`, `code 600` (**`BotPHD_*` değil `BotPHB_*`**: `112603` `Skill 1126` ağacı, F4-33 bulgusu); F4-31 grup heal `112557 self`, `target -1`, `victims` ≥ 1; F4-29 Inferno `cast BotMF_K 110545 BotWP_E 1` `target -1`, `victims` ≥ 1; Moral 7 `cast BotMF_K 110518 BotWP_E 1` `target` kurban kimliği. `TELEMETRY=summary`: summon çalışır, JSONL'de `ACTION_*` yok; `ENABLED=0` ⇒ komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde; sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Temizlik: ini yedekten geri, geçici betikler silindi, botlar despawn, sunucular `stop`.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `BotCore/` ve `Bot/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **`Type1 = 8` + `Moral` 4 + `UseItem 0` + `MAGIC_TYPE8.WarpType 12`** skill'i serisinde davranış değiştirir (önceden `unsupported_skill` idi); `wantedTarget` değişikliği yalnızca summon için `self`'i reddeder, diğer tüm skill'lerde davranış aynıdır. Diğer tüm skill'ler için `BeginCast` davranışı (paket biçimi ve telemetri satırı dahil) değişmez.
- **Thread kuralı (ADR-0005):** `TickCast` yalnızca IOCP thread'inde; `BeginCast` `m_Magictype8Array`'i (yalnızca açılışta doldurulan tablo) okur, yeni durum/kilit yok. Sunucuda `Warp` çağıranın IOCP thread'inde çalışır (çağıran bot tick'i); çağrılan bot oturumuna `Send` (bot alıcısı) bu thread'den yapılır, `BotSession` kilitleri zaten buna göre kurulu (F4-07 `Regene` aynı yolu kullanıyor).
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `effected` yalnızca `MAGIC_EFFECTING`'ten, `srv_fail` yalnızca `MAGIC_FAIL`'den. Hedefin ölü olup olmadığı, party üyeliği, zone, ışınlanabilirlik **bota önkontrol olarak eklenmez**; bunlar `WIZ_DEAD` gözlemi (`Perception`), `TeamView` ve karar katmanının (F7) işidir. MP önkontrolü bota ait `user->GetMana()` değeridir (mevcut `CheckCastStart`).
- **Bilinen sınırlar `[A]`/`[D]`:** (a) **`effected` summon'un gerçekleştiğini kanıtlamaz:** hedef ölü/başka zone'da/ışınlanamıyor/ışınlanıyorsa sunucu yine EFFECTING yayını gönderir (`sData[1] = 0`) ve **MP yine düşer**; gerçek sonuç hedefin konumudur (`list`/`Perception`) ya da olay halkasındaki `SkillEvent.data[1]` (başarıda 1, başarısızlıkta 0). F7 karar katmanı bu ikisinden başarıyı çıkarmalıdır (`docs/08` §13 "Summon fail" satırı). (b) **Menzil fiilen yoktur:** `Range 22500` ⇒ `CastInRange` ve sunucu menzil denetimi sınırsız sayılır; `Radius 10000` tek hedefli yolda kullanılmaz. (c) **"Respawn'dan sonra 180 sn summon edilemez" kuralı** (`CLAN_SUMMON_TIME`, `MagicProcess.cpp:136`, `docs/03` MEC-T8-02) yalnızca `UserRegionCheck` yolunda (hedef `-1`, `Moral` 6 grup/Escape) vardır; tek hedefli summon friend (`Moral` 4) bu yoldan geçmez, yani son diriltilen/dirilmiş üye summon'a **hemen** uygundur `[D]` (ölçülmedi: Ronark'ta `isWarZone()` değeri doğrulanmadı). (d) **Işınlanan botun algısı:** sunucu hedefe `WIZ_WARP` + `UserInOut`/`UserInOutForMe`/`NpcInOutForMe` gönderir; `BotSession` `WIZ_WARP`'ı işlemez ve bölge değişimi (`WIZ_REGIONCHANGE`) ışınlanmada gelmeyebilir, bu yüzden hedefin gözlem tablolarında eski bölgenin kayıtları kalabilir `[A]`. Bu plan Perception'ı değiştirmez; S1 gözlemi sonucu (`see`/`npcs`/`snap` ışınlanma öncesi/sonrası) rapora yazılır, kalıcı sapma çıkarsa `docs/KNOWN_ISSUES.md`'ye ayrı kayıt ve ayrı plan Claude'un işidir. (e) Skill adı sunucuda "Forgotten Temple" ve `490042/490050` istisnaları Ronark'ta geçersizdir. (f) `MAGIC_TYPE8.Target` ve `ExpRecover` summon'da kullanılmaz.
- Telemetri hacmi değişmez; `tools/bot-telemetry-report.py` değişmez (`no_result`, `srv_fail` ve `effected` zaten ayrı sayılır, MET-ACT-02; başarısız summon `effected` sayılır, bu bilinen sınırdır).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- **Durum:** UYGULANDI (tüm uygulama adımları yapıldı; derleme ve birim testleri geçti). Planda istenen çalışma zamanı doğrulaması (S1–S7) Claude'a aittir; bu turda yapılmadı.
- **Branch:** `bot/F4-34` (taban: `gece/2026-10-02` @ `0b77e34`). Commit'ler: `[F4-34] ...` (kod) ve `[F4-34] Uygulandı ...` (Durum + rapor) — commit hash'leri commit sonrası `git log`'ta görülür.
- **Değişen dosyalar ve nedenleri:**
  - `BotCore/BotCombat.h`: `kMoralPartyMember` (4) ve `kType8WarpSummon` (12) sabitleri ve `CastSummonSupported(type0, type1, moral, useItem, warpType)` eklendi (plan §5.1). Yalnızca ekleme (+17 satır); mevcut fonksiyonlar değişmedi.
  - `GameServer/Bot/ActionExecutor.cpp`: `BeginCast` içine summon istisnası (`m->bType[0] == 8` için `m_Magictype8Array` okuması + `CastSummonSupported` + `bFlyingEffect == 0` + `CastHpCostSupported`) ve `if (!resurrection && !summon` değişikliği; `wantedTarget = ... || summon` (plan §5.3 a/b). İki hunk, ikisi de `BeginCast` içinde.
  - `GameServer/Bot/ActionExecutor.h`: yalnızca `BeginCast` yorumu (destek listesine summon, `bad_target` açıklaması; plan §5.3 d).
  - `Tests/BotCoreTests/CombatTests.cpp`: yeni `Combat_SummonCast_Guard` (110 → 111; plan §5.2).
- **Derleme çıktısının son satırları:**
  - `./tools/build.sh Release`: `All 14057 functions were compiled... proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe` — hata yok; yalnızca mevcut `UpgradeHandler.cpp` C4789 uyarıları.
  - `./tools/build.sh Debug`: `proj-GameServer.vcxproj -> ...\build\bin\x86-Debug\Server\GameServer.exe`, `BotCoreTests.vcxproj -> ...\build\bin\x86-Debug\Tests\BotCoreTests.exe` — hata yok.
- **Testler:** `./tools/run-tests.sh Release` ve `Debug`: `111 tests, 0 failed`; `Combat_CastTypes_Supported`, `Combat_ResurrectionCast_Guard`, `Combat_SummonCast_Guard` `[ OK ]`.
- **Kriter öz-değerlendirmesi:**
  - K1 ✔ (Release hatasız; değişen dört dosyada yeni uyarı yok), K2 ✔ (Debug hatasız).
  - K3 ✔ (`111 tests, 0 failed` Release + Debug; üç test adı `[ OK ]`).
  - K4 ✔ (BotCore saflık grep'i boş; `git diff ... | grep '^+' | grep std::min/std::max` boş; `grep '^-'` boş = yalnızca ekleme).
  - K5 ✔ (`CastSummonSupported`/`m_Magictype8Array` tam iki satır; `CastNeedsOtherTarget` tek satır ve `|| summon`; `if (!resurrection && !summon`; `m->iUseItem != 0` sayısı 2).
  - K6 ✔ (ActionExecutor.cpp iki hunk, ikisi de `BeginCast` içinde; `git diff --stat` yalnızca dört dosyayı gösterir; korunan dosyaların diff'i boş; proje dosyaları değişmedi).
  - K7 ✔ (diff'te yeni `Emit(` yok; yeni ini anahtarı/komut/thread/telemetri türü/alanı yok).
  - K8 ✔ (dört dosya `file` çıktısı: ASCII + CRLF; `git diff --check` boş).
  - K9 ✔ (CheckMoveStep 2, CheckAttack/CheckCastStart/CheckCastEffect/CheckCastFly/CheckCastLand/CheckCastCancel/CheckPotion ≥ 1; önceki 110 test geçiyor).
  - K10 ✔ (`python3 tools/check-perception-contract.py`: `files scanned: 19`, R1 0/0, R2 0/28, R4 0/0, R5 0/0, `RESULT: PASS`; araç/istisna listeleri değişmedi).
  - K11 — Claude'a ait (çalışma zamanı S1–S7); bu turda yapılmadı.
- **Plandan sapmalar:** Yok. Plan §5.1/§5.2/§5.3 metinleri birebir uygulandı (yorum metinleri dahil). `docs/`, `plans/README.md`, ADR dosyaları değiştirilmedi.
- **Açık sorular:** Yok.


---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

_Henüz doğrulanmadı._
