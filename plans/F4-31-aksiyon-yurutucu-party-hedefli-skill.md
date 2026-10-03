# F4-31: `ActionExecutor` party hedefli skill dilimi — grup heal/grup buff (`Moral` 6) ve party üyesine buff (`Moral` 4)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02, merge `78759b5`) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-31` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-28 (Type4 tek tipli: `Moral` 1/2/7, MEC-MAG-15) — `KAPANDI`; F4-29 (alan: hedef kimliği `-1`, hedef noktası, `victims`) — `KAPANDI`; F4-30 (`CastMoralSupported(moral)` tek parametre) — `KAPANDI` (merge `c88dd95`); F4-08 (party kurulumu: `/bot pinvite`, `/bot paccept`) — `KAPANDI` (yalnızca çalışma zamanı sınaması için) |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-07, CLI-11, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-15, MEC-MAG-16, MEC-MAG-18 (bu planla eklendi, `[D]`), SK-06, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 dilim 5/m.5 (Ek 1 madde 2: party hedefli skill'ler), `docs/17` §2.1 "Priest grup heal", "Priest buff" |
| Tahmini büyüklük | S (4 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.cpp`, `ActionExecutor.h`; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

Priest'in iki iş kalemi botla atılamıyor ve F7 (party koordinasyonu, healer) bunlarsız yazılamaz: **grup heal** (Group massive healing `112557`, Group complete healing `112560`: `Moral` 6, Type3, party r = 30) ve **party buff'ları** (Grace/Brave/Strong/Hardness/Mightness/massiveness `1126xx`: `Moral` 4, Type4, party üyesine; Greatness `112656`: `Moral` 6, Type4, tüm party). `BeginCast`, `CastMoralSupported`'ta `Moral` 4 ve 6'yı bilmediği için bunları `unsupported_skill` ile reddediyor (`BotCore/BotCombat.h:343-349`). ADR-0018 Ek 1 madde 2 bu işi alan skill'lerinden ayrı, "party hedefi çözümü açıkça yazılmış" bir dilim olarak ister; 2b (okçu Type2) bu dilimden önce **ertelendi** (ADR-0018 Ek 7: `docs/01` §2 rogue/archer'ı kapsam dışı bırakır ve bot profillerinde okçu yoktur).

Sunucu tarafında **yeni akış gerekmez**: `Moral` 4 tek hedefli (Moral 2 gibi, hedef kimliği = party üyesinin kimliği ya da kendisi), `Moral` 6 ise alan skill'i gibi hedef kimliği `-1` + hedef noktası ister ve kurban listesi "çağıranın party'si" olur (`MagicProcess.cpp:121-141`). F4-29'un `area` bayrağı (paket alanları + `victims`) `Moral` 6 için aynen işe yarar. Bu plan:

1. `CastMoralSupported`'a `Moral` 4 ve 6'yı ekler; `Moral` 6'yı hedef-kimliği-`-1`/hedef-noktası/`victims` yoluna bağlar (`SendsAimPoint`, `IsAreaMoral` yerine `TickCast`'ta);
2. kendini öldürebilen skill'i (Sacrifice `106660`, `MAGIC.HP >= 10000`) bu yeni `Moral` 4 kapısından geçmekten **korur** (`CastHpCostSupported`);
3. sunucu davranışını kodla belgeler ve çalışma zamanında doğrular (`docs/03` MEC-MAG-18).

F4'ün otuz birinci planıdır (ADR-0018 sırası: ... uçan alan ✔ → **party hedefli (bu plan)** → dilim 6 (Type5+/`UseItem`) → CLI-12 → envanter doldurma).

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-31)" (bu planla birlikte yazıldı), "Ek (F4-29)", "Ek (F4-28)". `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` (Ek 1 madde 2, Ek 7).
- `docs/03` §4.2 **MEC-MAG-15** (tek hedefli Type4: aynı `BuffType` reddi, MP yalnızca başarıda), **MEC-MAG-16** (alan: hedef `-1`, kurban başına yayın, `victims`), **MEC-MAG-18** (bu planla eklendi), §13.2 CLI-07, `docs/05` SK-06 ("Alan heal'leri yalnızca party üyelerini etkiler (PARTY_ALL)"), §5 priest tablosu (satır 117-118).
- `plans/F4-28-aksiyon-yurutucu-type4-tek-tipli-skill.md`, `plans/F4-29-aksiyon-yurutucu-alan-skill.md`, `plans/F4-30-aksiyon-yurutucu-ucan-alan-skill.md` (aynı fonksiyonlar; **yazılı planı değil, kodu esas al**).
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `c88dd95` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `GameServer/MagicInstance.h:23-34` — `MORAL_PARTY = 4`, `MORAL_PARTY_ALL = 6`. `:170-180` (`UserCanCast`): hedef kimliği `-1` zorunluluğu yalnızca `Moral 10..13` içindir (`MORAL_AREA_ENEMY..MORAL_SELF_AREA`); `Moral` 6 için kimlik zorunlu değildir ama `-1` grup yoludur (aşağıda). `:20` `Run()` hedefi yalnızca `sTargetID != -1` iken çözer.
  - **`Moral` 4** (`IsAvailable()` `:831-847`): NPC çağıran/hedef ⇒ `false`; çağıran party'de değilse hedef kendisinden başkası olamaz; hedef varsa `GetPartyID()` çağıranınkiyle aynı olmalı (kendisi geçer). Aksi `MAGIC_FAIL`. `IsAvailable()` CASTING ve EFFECTING'te çalışır (`:284-286`), yani reddedilen bir buff **CASTING'te** `srv_fail` olur. Skill ağacı/seviye denetimi de oradadır (`:956-963`).
  - **`Moral` 6** (`IsAvailable()`'ta `case` yok): hedef kimliği `-1` ise `ExecuteType3` (`:1276-1307`) / `ExecuteType4` (`:1635-1677`) grup yoluna girer. Kurban seçimi `UserRegionCheck` (`MagicProcess.cpp:113-141`): `MORAL_PARTY_ALL` ⇒ NPC olamaz; hedef party'de değilse yalnızca çağıranın kendisi geçer; aynı `PartyID` ise (Type8 hariç) `final_test`: `radius == 0 || kurban.isInRangeSlow(sData[0], sData[2], radius)` (2D, hedef noktasından). Ayrıca **çağırandan** kurbana mesafe `>= sRange` ise kurban atlanır (`:1337-1339`, `:1695`). **Type3 grup heal:** çağıran (`sFirstDamage > 0 || sTimeDamage > 0` ise) her zaman listeye girer (`:1280-1281`), diğer üyeler `UserRegionCheck`'ten geçerse; **hiç kurban yoksa** `SendSkill(); return true` (`:1299-1302`). **Type4 grup buff:** `UserRegionCheck` boş çıkarsa ve skill `Moral` 6 ise çağıran tek kurban olur (`:1664-1669`). Hedef kimliği `-1` yerine bir üye kimliği verilirse `Moral` 6 yine **tek hedefli** çalışır (`pSkillTarget` yolu; `IsAvailable()`'ta `Moral` 6 denetimi yok); botun grup skill'ini bu yolla atması istenmez: gerçek istemci grup skill'inde `-1` yollar ve bu plan alan paketi biçimini (F4-29) kullanır.
  - **`CheckType3Prerequisites()`** (`:446-478`): hedef `-1` ve çağıran oyuncuysa güvenli bölgede `false`; `Moral` 6 + `sTimeDamage > 0` (HoT; Past Restore `112570`) ise çağırandaki etkin restoration varken `false`. `112557`/`112560` `TimeDamage 0` (anlık heal) olduğundan bu kural onlara uygulanmaz. **`CheckType4Prerequisites()`** (`:540-583`): hedef kimliği `-1` ise `true` (alan buff'larında aynı `BuffType` denetimi yok).
  - **MP:** `IsAvailable()` EFFECTING'te `bType[0] != 4 || sTargetID == -1` iken `Msp`'yi **bir kez** düşer (`:1028-1029`); tek hedefli Type4'te (Moral 4, hedef kimliği `!= -1`) düşmez, `ExecuteType4` başarıda bir kez düşer (`:1800-1802`). Yani `Moral` 6: EFFECTING'te bir kez (kurban yoksa da); `Moral` 4: yalnızca başarılı buff'ta bir kez.
  - **Grup Type4'te aynı `BuffType` hedefteyse:** `ExecuteType4` `:1770-1790`: alan buff'ında (`sTargetID == -1`) hata verilmez, o üye **sessizce atlanır** (`continue`, o üyeye yayın yok). Tüm üyelerde buff varsa EFFECTING hiç yayınlanmaz ⇒ bot sonucu `no_result` (MEC-MAG-15'in tek hedefli `srv_fail`'inden farklı).
  - **Kendini öldüren skill (`:1041-1048`):** `MAGIC.HP >= 10000` ise sunucu çağırandan **10 000 HP düşer** (can denetimi yok; `HpChange(-10000)`), kendine atılamaz. Bot sınıflarında yalnızca **Sacrifice `106660`** (`Moral` 4, `{3, 0}`, `HP 10001`, `Msp 180`, `CastTime 0`, `Range 67`, `SkillLevel 60`, `Skill 1066`) böyledir; yani `Moral` 4 açılınca bu skill otomatik açılırdı (warrior profilinde WG'nin 1066 ağacı 62 ≥ 60). Referans bot canı (≈ 5650, `docs/04`) 10 000'den azdır: cast botu öldürürdü. Bu plan o skill'i `unsupported_skill` ile dışarıda tutar (karar katmanı `docs/06` ile ayrıca ele alır).
  - `GameServer/Bot/ActionExecutor.cpp` (`c88dd95`): `BeginCast` destek kuralı `:733-737` (`CastMoralSupported(m->bMoral)` `:737`); `bad_target` kuralı `:754-762` (`MORAL_SELF` ⇒ hedef adı yasak, `MORAL_ENEMY` ⇒ hedef adı zorunlu; `Moral` 4/6 ikisini de kabul eder); `TickCast`: `area` `:815`, `sent.id` `:816-817`, `sData` `:898-901`, `SubmitCast(..., area, ...)` `:917`, `:959`, `:1000`; `SubmitCast` içinde `victims` `area && opcode == MAGIC_EFFECTING` `:665`.
  - `GameServer/Bot/BotManager.cpp:3186-3215` (test sürücüsü): kendine atışta hedef görünümü `id` = çağıranın kimliği, `x/y/z` çağıranın konumu, `isSelf = true`; başka bota atışta hedef botun konumu. Bu plan **değiştirmez**.
  - `BotCore/BotCombat.h:333-349` (`kMoralAreaEnemy`, `IsAreaMoral`, `CastMoralSupported`), `:352-363` (`CastTargetIdField`, `CastCoordField`); `Tests/BotCoreTests/CombatTests.cpp` `Combat_CastMoral_Supported` (`:1171`), `Combat_AreaCast_Fields` (`:1199`), `Combat_FlyingArea_Guard` (`:1241`, dosyanın son testi). Toplam birim test **107** (F4-30 sonrası).
- **Veri notu (Claude yerel `MAGIC`/`MAGIC_TYPE3`/`MAGIC_TYPE4` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** Bot sınıflarında (`106`, `110`, `112` ve `2xx` karşılıkları) `Moral` 4/6 + `UseItem 0` + Type3/Type4 skill'leri **yalnızca priest'te** (112) ve Sacrifice'ta:
  - **`Moral` 6, Type3 (`{3, 0}`):** `112557` Group massive healing (`SkillLevel 57`, `Skill 1125`, `Msp 960`, `CastTime 15`, `ReCastTime 54`, `Range 56`, Type3 `FirstDamage +960`, `Radius 30`); `112560` Group complete healing (`SkillLevel 60`, `Msp 1920`, `ReCastTime 64`, `FirstDamage +10000`, `Radius 30`); `112570` critical restore (`SkillLevel 70`, `UseStanding 54`, HoT `TimeDamage 3000`, `Radius 20`; botların 1125 ağacı 60 ⇒ erişilemez); `112575`/`112580` (`SkillLevel 75/80`, `Etc 520/523`: erişilemez).
  - **`Moral` 4, Type4 (`{4, 0}`):** Grace `112606` (6), Brave `112615` (15), Strong `112624` (24), Hardness `112633` (33), Mightness `112642` (42), Undying `112654`/Heapness `112655` (54), massiveness `112657` (57), imposingness `112670` (70, `UseStanding 54`), Superioris `112675` (78, `Etc 522`); hepsi `CastTime 15`, `ReCastTime 1`, `Range 56`, `Radius 0`, `BuffType 1` (MaxHP buff'ları: aynı hedefte yalnızca biri), `Duration 600`; `Msp` 15–690.
  - **`Moral` 6, Type4 (`{4, 0}`):** Greatness `112656` (`SkillLevel 57`, `Msp 570`, `Radius 30`, `BuffType 1`); `112672` Massive Binder (72, `Etc 518`), `112673` Round Insensibility (74, `Etc 519`, `BuffType 2`): erişilemez.
  - Reddedilmeye devam edenler: `Moral` 6 + **Type5** Bless of God `112671`, `Moral` 2 + Type5 Cure curse `112525`/`112535`, `Moral` 6 + Type4 + **`UseItem`** Counter Curse `112676`, `Moral` 4 + **Type8** descent `106650`, summon friend `110004`, `Moral` 6 + Type8 Escape `110035` (hepsi `CastTypesSupported`/`UseItem` ile; dilim 6'ya kalır).
  - Bu sınıflarda başka `Moral` 4/6 skill'i yoktur (sorgu: `Moral IN (4, 6)`, sınıf `106/110/112`).
- **Bot karakter notu (`db/002_bot_characters.sql:124-136`).** Skill ağacı baytları (`m_bstrSkill[Skill % 10]`): `BotPHB` `1125` (iyileştirme) **60**, `1126` (buff) **62**, `1127` (debuff) 0 ⇒ `112557` (57), `112560` (60), Grace…massiveness/Greatness (≤ 57) atılabilir; `BotPHD` `1125` **60**, `1126` **0**, `1127` 62 ⇒ grup heal'ler atılabilir, **buff'lar atılamaz** (ağaç 0, KI-016: bot kabul eder, sunucu CASTING'te `MAGIC_FAIL` verir, MP düşmez). Sınama için `BotPHB_K` kullanılır (Karus; El Morad karşılığı `BotPHB_E`, skill'ler `2125xx`/`2126xx`).
- **Menzil notu.** `Range 56` m: `Moral` 4'te hedef üye, `Moral` 6'da hedef noktası çağırandan `< 56` m olmalı (bot guard'ı `CastInRange`, `MEC-MAG-11`); `Moral` 6'da ayrıca üyeler hedef noktasının `Radius 30` m içinde ve çağırandan `< 56` m olmalı (sunucu seçer). Sınama botları aynı noktada doğar.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `kMoralPartyAll`, `IsPartyAllMoral`, `SendsAimPoint(moral)` (`Moral` 10 ya da 6), `CastMoralSupported(moral)` `1, 2, 4, 7, 8` + `SendsAimPoint`, `CastHpCostSupported(hp)` (`hp < kSacrificeHpCost`). Yorumlar güncellenir (§5.1).
2. **`BeginCast` (`ActionExecutor.cpp`):** destek koşuluna `CastHpCostSupported(m->sHP)` eklenir; başka koşul değişmez (`bad_target` kuralı `Moral` 4/6 için zaten serbest).
3. **`TickCast` (`ActionExecutor.cpp`):** `bool area = BotCore::SendsAimPoint(m->bMoral);` (yalnızca bu satır ve yorumu); `SubmitCast`/`CancelCast`/`RejectCast` **değişmez** (`area` bayrağı "hedef kimliği `-1` + hedef noktası + `victims`" anlamını taşımaya devam eder).
4. **Yorumlar (`ActionExecutor.h`):** `CastTarget`, `BeginCast`, `TickCast` açıklamaları party hedefli skill'leri kapsar (§5.3).
5. **Birim testleri:** `Combat_CastMoral_Supported` güncellenir; yeni `Combat_PartyCast_Guard` eklenir (107 → 108).
6. **Sonuç sözleşmesi (§5.4):** `Moral` 4 ve 6 akışı belgelenir (`docs/03` MEC-MAG-18) ve çalışma zamanında sınanır.

**Kapsam dışı (yapılmayacak)**

- `Moral` 3 (dost, kendisi hariç), 5 (NPC), 9, 11, 12, 13, 14, 15, 16+; **Type5** (cure, Bless of God, diriltme), **Type8** (descent, summon friend, Gate, Escape), `UseItem != 0` skill'leri (Counter Curse, Stone of Life, sınıf taşları), `{3, 4}`/`{1, 4}`/`{2, 4}` party çiftleri (bot sınıflarında yok), `{7, 0}`; hepsi dilim 6'ya (Type5+/`UseItem`) kalır.
- **Hedef çözümü / karar katmanı:** hangi party üyesine hangi buff'ın atılacağı, kimin eksik canlı olduğu, grup skill'inin ne zaman atılacağı (`≥ 3 üye eksik`, `docs/07`) F7/karar katmanı işidir. Bu planın "hedef çözümü" yalnızca paket sözleşmesidir: **`Moral` 4: komuttaki ad (ya da `self`) = hedef kimliği; `Moral` 6: ad/`self` = hedef noktası, paket hedef kimliği `-1`**. Bota **party üyeliği/hedefte buff var mı önkontrolü eklenmez** (AC-LRN-03: `TeamView` ve `SelfState.buffs` zaten algıdan okunur, karar katmanı kullanır; üyelik dışı hedef sunucuda `srv_fail` olur).
- Yeni komut/ini anahtarı/telemetri olayı türü/alan, `BotSession.*`/`BotManager.cpp` değişikliği, `tools/` betik değişikliği.
- Sacrifice'ı (`106660`) açmak (`HP >= 10000` skill'leri): yalnızca karar katmanı (F6 warrior) ve güvenli can eşiği ile, ayrı plan.
- Skill ağacı puanı denetimi (KI-016) ve `UseStanding` anlamı (KI-017: `112570`/`112670` `UseStanding 54`; bu skill'ler botlarda zaten erişilemez), güvenli bölge denetimi (bot yalnızca sunucu reddini görür).
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | `// --- area cast ...` bölümü: `kMoralPartyAll`, `IsPartyAllMoral`, `SendsAimPoint`, `CastMoralSupported`; `CastHpCostSupported` |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | `Combat_CastMoral_Supported` güncellenir; `Combat_PartyCast_Guard` eklenir (107 → 108) |
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | `BeginCast` destek koşulu (`:733-738`), `TickCast` `area` satırı (`:814-815`) |
| `GameServer/Bot/ActionExecutor.h` | değiştir | `CastTarget`/`BeginCast`/`TickCast` yorumları |

`BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `ScenarioRunner.*`, `ScriptRunner.*`, `tools/` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

`// --- area cast ... ---` bölümünü şöyle genişlet (ASCII, CRLF, tab, `namespace BotCore` içinde; yeni `#include` yok; **`std::min`/`std::max` kullanma**). `kMoralAreaEnemy`/`IsAreaMoral` aynen kalır; `CastMoralSupported`'ı ve üstündeki yorumu değiştir:

```cpp
	// MAGIC.Moral 6 = PARTY_ALL (MagicInstance.h): the whole party within MAGIC_TYPE3/4.Radius of the aim point. Like an area
	// skill it is cast with target id -1 and the aim point in sData[0] (x) / sData[2] (z); the server picks the victims
	// (the caster's party members, UserRegionCheck). Moral 4 = PARTY is single-target (a party member or self) and uses the
	// ordinary packet shape (F4-31, docs/03 MEC-MAG-18).
	constexpr uint8_t kMoralPartyAll = 6;

	inline bool IsPartyAllMoral(uint8_t moral)
	{
		return moral == kMoralPartyAll;
	}

	// Skills cast with target id -1 and an aim point: area enemy (10, F4-29) and party-all (6, F4-31).
	inline bool SendsAimPoint(uint8_t moral)
	{
		return IsAreaMoral(moral) || IsPartyAllMoral(moral);
	}

	// Morals BeginCast accepts: 1 self, 2 friend-with-me, 4 party member, 7 enemy, 8 all (F4-03, F4-31), 10 area-enemy
	// (F4-29, flying or not: F4-30) and 6 party-all (F4-31). Whether the skill may fly at all is decided by the caller
	// (IsFlyingCast: Type3 only).
	inline bool CastMoralSupported(uint8_t moral)
	{
		if (moral == 1 || moral == 2 || moral == 4 || moral == 7 || moral == 8)
			return true;

		return SendsAimPoint(moral);
	}

	// MAGIC.HP >= 10000 is the server's "sacrifice" convention (MagicInstance.cpp:1041-1048): the caster loses 10000 HP
	// without a health check, which would kill a bot. The bot never opens such a skill (F4-31).
	constexpr uint16_t kSacrificeHpCost = 10000;

	inline bool CastHpCostSupported(uint16_t hp)
	{
		return hp < kSacrificeHpCost;
	}
```

`CastTargetIdField(bool area, ...)` ve `CastCoordField(bool area, ...)` **değişmez** (parametre adı `area` kalır; çağıran `SendsAimPoint` sonucunu verir). Üstlerindeki yorumlar "area" yanında "party-all"ı da anar. Dosyanın kalanı **değişmez**. `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

a) **`Combat_CastMoral_Supported` (var olan test) güncellenir:** `true` kümesi `1, 2, 4, 6, 7, 8, 10`; `false` kümesi `0, 3, 5, 9, 11, 12, 13, 14, 15, 25` (yani `CastMoralSupported(4)` ve `(6)` satırları `false`'tan `true`'ya çevrilir; `(3)`, `(5)` `false` kalır). `IsAreaMoral` denetimleri aynen kalır; `IsAreaMoral(6) == false` eklenir.

b) **Yeni `Combat_PartyCast_Guard`** (dosyanın sonuna, `Combat_FlyingArea_Guard`'dan sonra; Group massive healing `112557`: `Msp 960`, `CastTime 15`, `ReCastTime 54`, `Range 56`):
   - Sınıflandırma: `IsPartyAllMoral(6) == true`; `IsPartyAllMoral(4) == false`; `IsPartyAllMoral(10) == false`; `SendsAimPoint(6) == true`, `SendsAimPoint(10) == true`, `SendsAimPoint(4) == false`, `SendsAimPoint(7) == false`, `SendsAimPoint(1) == false`.
   - Tipler: `CastTypesSupported(3, 0) == true`, `CastTypesSupported(4, 0) == true` (grup heal `{3, 0}`, party buff `{4, 0}`); `CastTypesSupported(5, 0) == false` (Bless of God/Cure curse reddedilmeye devam eder), `CastTypesSupported(8, 0) == false` (descent/summon friend/Escape).
   - Paket alanları: `Moral` 6: `CastTargetIdField(SendsAimPoint(6), 2990) == -1`; `CastCoordField(SendsAimPoint(6), true, 80.9f) == 80` (kendine grup heal: hedef noktası = çağıranın konumu). `Moral` 4: `CastTargetIdField(SendsAimPoint(4), 2990) == 2990`; `CastCoordField(SendsAimPoint(4), true, 80.9f) == 0` (kendine buff: koordinat 0); `CastCoordField(SendsAimPoint(4), false, 80.9f) == 80` (party üyesine buff).
   - Sacrifice kapısı: `CastHpCostSupported(0) == true`, `(100) == true` (Pain killer `100`, Blaze Killer `200`: Moral 1 `HP < 10000`, zaten açık), `(9999) == true`, `(10000) == false`, `(10001) == false` (Sacrifice), `(65535) == false`.
   - Başlangıç guard'ı (`CastStartCheck c = {}`; alanlar `Combat_AreaCast_Fields`'taki gibi, `c.skillRange = 56`, `c.msp = 960`, `c.reCastMs = BotCore::CastRecastMs(54)`, `c.typeGated = true`, `c.mana = 960`, `c.distanceM = 0.0f` (`self`), `c.standing = true`, `c.needsStanding = false`, `c.actionsInWindow = 0`): `CastRecastMs(54) == 5400`; `CheckCastStart(c) == CAST_OK`; `c.mana = 959` ⇒ `CAST_REJECT_NO_MANA`; `c.mana = 960; c.distanceM = 55.9f` ⇒ `CAST_OK`; `c.distanceM = 56.0f` ⇒ `CAST_REJECT_OUT_OF_RANGE`; `c.distanceM = 0.0f; c.hasSkillLast = true; c.sinceSkillLastMs = 5399` ⇒ `CAST_REJECT_RECAST`; `c.sinceSkillLastMs = 5400` ⇒ `CAST_OK`.
   - Test adı ve enum sabit adları `BotCombat.h`'dekiyle birebir kullanılır (`CAST_OK`, `CAST_REJECT_NO_MANA`, `CAST_REJECT_OUT_OF_RANGE`, `CAST_REJECT_RECAST`); farklıysa dosyadaki adı kullan.
   - Toplam test sayısı **108**.

### 5.3 `ActionExecutor.cpp` ve `ActionExecutor.h`

**a) `BeginCast` (`ActionExecutor.cpp:733-738`).** Destek koşuluna sunucunun kendini öldüren skill kuralını ekle (yalnızca bu satır):

```cpp
	bool flyingCast = BotCore::IsFlyingCast(m->bType[0], m->bFlyingEffect);
	if (!BotCore::CastTypesSupported(m->bType[0], m->bType[1])
		|| (m->bFlyingEffect != 0 && !flyingCast)
		|| m->iUseItem != 0
		|| !BotCore::CastMoralSupported(m->bMoral)
		|| !BotCore::CastHpCostSupported(m->sHP))
```

`bad_target` bloğu (`:754-762`) **değişmez** (`wantedSelf` yalnızca `MORAL_SELF`, `wantedTarget` yalnızca `MORAL_ENEMY`; `Moral` 4 ve 6 hem `self` hem hedef adı ile kabul edilir). `m->sHP` `uint16`'dir (`shared/database/MagicTableSet.h:26`); `MAGIC.HP` sütunu.

**b) `TickCast` (`ActionExecutor.cpp:814-815`).** Yalnızca şu satır ve üstündeki yorum değişir:

```cpp
	// ADR-0017 Ek F4-29/F4-31: an area skill (MAGIC.Moral 10) and a party-all skill (Moral 6) send target id -1 and the aim
	// point in sData[0..2]; a Moral 4 (single party member) skill sends the ordinary single-target packet.
	bool area = BotCore::SendsAimPoint(m->bMoral);
```

`SubmitCast`, `CancelCast`, `RejectCast`, `EndCast` ve diğer her şey **değişmez**.

**c) `ActionExecutor.h` yorumları.** `CastTarget` açıklamasındaki "For an area skill (MAGIC.Moral 10) 'x/y/z' is the aim point ..." cümlesine "or party-all (Moral 6)" ekle. `BeginCast` yorumundaki destek listesine "party-targeted skills (Moral 4 single party member or self; Moral 6 whole party with target id -1 and an aim point; MAGIC.HP >= 10000 'sacrifice' skills stay unsupported; ADR-0017 Ek F4-31)" ekle; `TickCast` yorumuna: "party-all: like area (target id -1, aim point, 'victims' = per-member EFFECTING packets of the EFFECTING step, docs/03 MEC-MAG-18); an empty party still gives 'effected' for a heal (the caster is always healed); a group buff on members that already hold the BuffType gives 'no_result'; Moral 4: like single Type4 (MEC-MAG-15)". Yorum dışında başlıkta değişiklik yok.

### 5.4 Sonuç sözleşmesi (`docs/03` MEC-MAG-18 ile aynı)

| Skill | Sunucu | Bot sonucu |
|---|---|---|
| **`Moral` 4 `{4, 0}`, party üyesine ya da kendine** (hedef kimliği = üye/kendi, koordinat 0 kendine, üye konumu başkasına) | CASTING: `IsAvailable()` `MORAL_PARTY` denetimi (aynı `PartyID` ya da kendisi) + `CheckType4Prerequisites()` aynı `BuffType` denetimi (MEC-MAG-15); EFFECTING: `ExecuteType4` tek hedef, MP **yalnızca başarıda bir kez** (`:1800-1802`), süreli yayın | buff uygulandı ⇒ `effected`, `code` = süre (600), `victims` yok; aynı `BuffType` hedefte ⇒ CASTING'te `srv_fail` (`op:4`, `code -100`), MP düşmez |
| `Moral` 4, çağıran party'de **değilken** başka bota | `IsAvailable()` `MORAL_PARTY` ⇒ `MAGIC_FAIL` | CASTING'te `srv_fail`, MP düşmez |
| `Moral` 4, çağıran party'de değilken **kendine** | geçer (`pSkillCaster == pSkillTarget`) | `effected` (party gerekmez) |
| `Moral` 4, farklı party'deki / düşman bota | `GetPartyID()` farkı ⇒ `MAGIC_FAIL` | CASTING'te `srv_fail` |
| **`Moral` 6 `{3, 0}` grup heal** (hedef `-1`, hedef noktası `sData[0]`/`sData[2]`) | `CheckType3Prerequisites()` (güvenli bölgede `false`); EFFECTING'te MP bir kez (`:1028-1029`, kurban yoksa da); çağıran her zaman listede + hedef noktasının `Radius 30` m ve çağırandan `< 56` m içindeki **aynı party** üyeleri; kurban başına hedef kimlikli EFFECTING, sonra hedef `−1`'li son `SendSkill()` | `effected`, `code 0`, **`victims`** = çağırana gelen kurban kimlikli EFFECTING sayısı (çağıran dahil: kendi heal'ı da bir paket); party'siz çağıran yalnız kendini iyileştirir (`victims 1`) |
| **`Moral` 6 `{4, 0}` grup buff** (Greatness `112656`) | `CheckType4Prerequisites()` `true`; MP bir kez (EFFECTING); `UserRegionCheck` boşsa çağıran tek kurban; `BuffType` zaten hedefteyse o üye **sessizce atlanır**; her kalan üye için `{sData[0], 1, sData[2], süre, ...}` yayını | en az bir üye buff aldıysa `effected`, `code` = süre, `victims` = alan paket sayısı; **tüm üyelerde buff varsa yayın yok ⇒ `no_result`** (MP yine düşmüş olur) |
| `Moral` 6, hedef noktası `>= sRange` (56) | bot guard EFFECTING/CASTING öncesi reddeder | `REFUSED`/`out_of_range`, paket gitmez |
| `Moral` 6, party üyesi çağırandan `>= 56` m ya da hedef noktasından `> Radius` | sunucu üyeyi atlar | `victims` o üyeyi saymaz |
| `BotPHD` ile buff (`1126` ağacı 0) | `IsAvailable()` ağaç denetimi ⇒ CASTING'te `MAGIC_FAIL` | `srv_fail`, MP düşmez (KI-016) |
| Sacrifice `106660` (`HP 10001`) | — | `BeginCast` `unsupported_skill`, paket gitmez |
| Güvenli bölge (`:355`, `:460`) | `MAGIC_FAIL` | `srv_fail` |
| CASTING'te `cast <bot> off` | iptal paketi: `Moral` 6'da hedef `-1` (`m_castTargetId` = `sent.id`), `Moral` 4'te hedef kimliği | `cancelled` (`op:4`, `code:-100`), MP düşmez |

`reason` metinleri, `CastOutcome::Kind` değerleri ve `TickCast` akışı bu tablo için **değişmez**; yeni çıktı yoktur. **İptal paketinin hedef kimliği (tablonun son satırı):** `CancelCast` hedef kimliğini `s->m_castTargetId`'den yazar (`ActionExecutor.cpp:1111`, `:1120`); o alan `TickCast`'ta `sent.id` ile (`:923`, `:965`), yani `CastTargetIdField(area, ...)` sonucuyla doldurulur. `Moral` 6'da `area` artık `true` olduğundan iptal paketi de otomatik `-1` taşır; `CancelCast` **değişmez** (`grep -n "IsAreaMoral" GameServer/Bot/ActionExecutor.cpp` bu planın sonunda boştur).

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (değişen dört dosya için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h`'yi içeren `.cpp`'ler de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastMoral_Supported` ve `Combat_PartyCast_Guard` adlarını `[ OK ]` ile içerir ve toplam test sayısı **108**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-31 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş.
- [ ] K5: `BeginCast`: `grep -n "CastHpCostSupported" GameServer/Bot/ActionExecutor.cpp` tam bir eşleşme verir ve `!BotCore::CastHpCostSupported(m->sHP)` biçimindedir; `grep -n "CastMoralSupported" GameServer/Bot/ActionExecutor.cpp` tam bir eşleşme (`CastMoralSupported(m->bMoral)`); `grep -n "bool area" GameServer/Bot/ActionExecutor.cpp` `BotCore::SendsAimPoint(m->bMoral)` gösterir; `grep -n "IsAreaMoral" GameServer/Bot/ActionExecutor.cpp` boş; `(m->bFlyingEffect != 0 && !flyingCast)` koşulu yerinde.
- [ ] K6: gerilemesiz: `git diff gece/2026-10-02...bot/F4-31 -- GameServer/Bot/ActionExecutor.cpp` yalnızca `BeginCast` destek koşulu ile `TickCast`'ın `area` satırı ve yorumu hunk'larını içerir; `SubmitCast`/`CancelCast`/`RejectCast` gövdesinde hunk yok; `git diff ... --stat` yalnızca §4'teki 4 dosyayı (ve plan dosyasını) gösterir; `GameServer/proj-GameServer.vcxproj*`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*` değişmemiş.
- [ ] K7: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı türü ve alanı yok (`git diff ... | grep '^+' | grep 'Emit('` boş).
- [ ] K8: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); dört dosya CRLF kalır (ASCII); `git diff --check` boş.
- [ ] K9: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 107 testin tamamı (güncellenen biri dahil) hâlâ geçiyor.
- [ ] K10: `python3 tools/check-perception-contract.py` `RESULT: PASS` (`R1`..`R5` ihlal sayıları 0; denetlenen dosya sayısı değişmez).
- [ ] K11 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S7 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-31
git diff gece/2026-10-02...bot/F4-31 -- BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff gece/2026-10-02...bot/F4-31 -- GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotManager.cpp GameServer/Bot/Telemetry.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -rn "CastMoralSupported\|CastHpCostSupported\|SendsAimPoint\|IsAreaMoral" BotCore GameServer Tests
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h GameServer/Bot/ActionExecutor.cpp
git diff --check gece/2026-10-02...bot/F4-31
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; komut başına ≥ 1 sn). Botlar (hepsi zone 71, aynı noktada doğar): Karus priest **`BotPHB_K`** (heal + buff ağaçları 60/62), Karus priest **`BotPHD_K`** (heal 60, buff 0), Karus **`BotWP_K`**, **`BotWG_K`**, **`BotMF_K`** (party üyeleri; HP 5650 civarı), El Morad **`BotWP_E`** (düşman, saldırı ile can düşürmek için). Önce `list` ile konum ve HP/ölü durumunu denetle (bir bot despawn'da son konumunu saklar ve ölü bot `regene` ile doğuş noktasına taşınır: F4-29/F4-30 bulgusu; botlar güvenli bölgede başlıyorsa her cast `srv_fail` verir). Party kurma: `pinvite BotPHB_K BotWP_K` → `paccept BotWP_K`, aynı şekilde `BotWG_K`; `snap BotPHB_K` `team` satırlarıyla üyeliği teyit et. MP/HP `list`'ten, buff `snap <bot>`'tan, olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'tan. MP kesin değeri sunucu yenilemesiyle (~5-6 MP/sn) karışır: MP'yi cast'ten hemen önce ve sonra `list` ile al, yenileme payını raporla. Her senaryo arasında botlar `despawn`/`spawn` ile yenilenir (aynı `BuffType`'ın eskisini taşımasın).

1. **S1 Grup heal (`112557`, `{3, 0}`, `Moral` 6):** `spawn BotPHB_K,BotWP_K,BotWG_K,BotMF_K,BotWP_E`; party kur (3 üye + priest); `BotWP_E` ile `BotWP_K` ve `BotWG_K` üstüne `attack` ver (R; canlarını bir miktar düşür, `list` ile HP not et), sonra `cast BotPHB_K 112557 self 1` (hedef noktası = priest'in konumu). Beklenen JSONL: `CastStart` `ACTION_SUBMIT` **`"target":-1`** → `ok:true`, `reason:"casting"`, `op:1`; ≥ 1,5 sn sonra `CastEffect` `ACTION_SUBMIT` `"target":-1` → `ok:true`, **`reason:"effected"`, `op:3`, `code:0`, `victims` ≥ 4** (priest + üç üye: kurban kimlikli EFFECTING sayısı); log `cast finished (effected) after 1 cycle(s), 1 ok, 2 packet(s) sent`; **MP düşüşü ≈ 960** (bir kez; yenileme payı ≤ ~10); hasar almış üyelerin HP'si `list`'te ≈ +960 artmış (en çok kendi `MaxHP`'sine kadar); `snap BotPHB_K events` kurban kimlikli `op=3 skill=112557 ... target=<üye>` olaylarını ve hedef `−1`'li son olayı gösterir. `BotMF_K`'yi çağırandan > 56 m uzağa taşımak mümkünse `victims` o üyeyi saymaz (mümkün değilse raporda belirtilir).
2. **S2 Party'siz çağıran ve boş grup:** taze oturumda `spawn BotPHB_K,BotWP_K` (party yok): `cast BotPHB_K 112557 self 1`: `effected`, `code:0`, **`victims:1`** (yalnızca kendisi: `UserRegionCheck` party'siz hedefte yalnız çağıranı geçirir; `BotWP_K` aynı noktada ama party dışı olduğundan atlanır); MP ≈ −960. `Group complete healing 112560` (`Msp 1920`, `ReCastTime 64` ⇒ aynı seride ikinci cast ≥ 6,4 sn sonra): `cast BotPHB_K 112560 self 1` `effected`, MP ≈ −1920.
3. **S3 Grup buff (`112656`, `{4, 0}`, `Moral` 6):** taze oturum, party (priest + `BotWP_K` + `BotWG_K`): `cast BotPHB_K 112656 self 1`: `casting` → `effected`, **`code` = süre (600)**, `victims` ≥ 3, MP ≈ −570 (bir kez); `snap BotWP_K` `snap BotWG_K` `snap BotPHB_K`: `buff skill=112656 type=1` (MaxHP buff'ı) ve `list`'te MaxHP artmış. **Aynı skill'i hemen yeniden** (`ReCastTime 1`, tip kapısı ≥ 1 sn sonra): tüm üyelerde `BuffType 1` olduğu için sunucu üyeleri atlar ⇒ yayın yok ⇒ **`no_result`** (`cast stopped (no_result)`/seri `FAILED`, MP yine ≈ −570 düşer: §5.4 son not; gözlenen fark raporlanır ve MEC-MAG-18 düzeltilir).
4. **S4 Party üyesine buff (`Moral` 4, `{4, 0}`):** taze oturum, party (priest + `BotWP_K`): `cast BotPHB_K 112642 BotWP_K 1` (Mightness, `Msp 240`): `CastStart` `ACTION_SUBMIT` `"target":<BotWP_K kimliği>` (**`-1` değil**) → `casting`; `CastEffect` `"target":<BotWP_K kimliği>` → `effected`, `code` = süre (600), `victims` alanı **yok**; MP ≈ −240 (**bir kez, yalnızca başarıda**); `snap BotWP_K` `buff skill=112642 type=1`. Aynı `BuffType` hedefteyken ikinci buff (`cast BotPHB_K 112615 BotWP_K 1`, Brave, aynı `BuffType 1`): **CASTING'te `srv_fail`** (`op:4`, `code -100`), MP değişmez (MEC-MAG-15 tek hedefli kural). Kendine: `cast BotPHB_K 112624 self 1` (Strong): `effected`, hedef kimliği = priest'in kimliği, koordinat 0.
5. **S5 Party dışı / başka party hedefi:** taze oturum, **party yok**, `spawn BotPHB_K,BotWP_K`: `cast BotPHB_K 112615 BotWP_K 1` ⇒ CASTING'te `srv_fail` (`IsAvailable()` `MORAL_PARTY`: çağıran party'de değil, hedef kendisi değil), MP değişmez; `cast BotPHB_K 112615 self 1` ⇒ `effected` (party gerekmez). Düşman millet bota (`BotWP_E`, aynı party değil): `cast BotPHB_K 112615 BotWP_E 1` ⇒ `srv_fail`.
6. **S6 Ağaç yetersizliği ve hedef noktası menzili:** (a) `BotPHD_K` (buff ağacı 0, KI-016): `cast BotPHD_K 112615 self 1` ⇒ bot kabul eder (`BeginCast` ağaç denetlemez), `CastStart` `ACTION_RESULT` `ok:false`, `reason:"srv_fail"`, `op:4`, seri CASTING'te biter, MP değişmez; `cast BotPHD_K 112557 self 1` ⇒ `effected` (heal ağacı 60 ≥ 57). (b) CLI-07: hedef botu çağırandan ≥ 60 m uzağa taşıyabiliyorsan `cast BotPHB_K 112557 <uzak bot> 1` ⇒ **paket gitmeden** `FAIRNESS_REJECT` `out_of_range`, `limit 56.00`, log `cast stopped (out_of_range)`; taşınamıyorsa bu alt senaryo birim testlerle (`c.distanceM = 56.0f` ⇒ `CAST_REJECT_OUT_OF_RANGE`) kapsanır ve raporda yazılır. (c) iptal: `cast BotPHB_K 112557 self 1` ardından CASTING aşamasında (~500 ms) `cast BotPHB_K off`: `cancelled` (`op:4`, `code:-100`), iptal paketi **hedef `-1`**, MP değişmez; aynı şeyi `Moral` 4 buff için (`112642 BotWP_K`) yap: iptal paketi hedef kimliği = `BotWP_K` kimliği (alan olmayan yol).
7. **S7 Sacrifice, gerilemesiz ve hâlâ desteklenmeyenler:** `BotWG_K` ile `cast BotWG_K 106660 BotWP_K 1` (Sacrifice, `HP 10001`) ⇒ `refused unsupported_skill`, **paket gitmez**, `BotWG_K`'nın canı değişmez. Hâlâ reddedilenler `unsupported_skill`: Bless of God `112671` (Type5), Cure curse `112525` (`Moral` 2 Type5), Counter Curse `112676` (`UseItem`), descent `106650`/summon friend `110004`/Escape `110035` (Type8), Group heal `112570`, Past Recovery `112575` (`Etc`/`SkillLevel`; sınıf/seviye `bad_skill` ya da `quest_locked` olabilir: reddi raporla). Gerilemesiz: `cast BotPHB_K 112606 self 1` hariç F4-28 Moral 2: `cast BotPHB_K 112603 self 1` (Insensibility Skin `Moral` 2 `{4, 0}`) `effected`; F4-29 alan: `cast BotMF_K 110545 BotWP_E 1` (Inferno) iki paket, `victims` ≥ 1; Moral 7 tek hedef `cast BotMF_K 110518 BotWP_E 1` `target` kurban kimliği; F4-30 `cast BotMF_K 110533 BotWP_E 1` üç paket, MP 2 × 150. `TELEMETRY=summary`: grup heal çalışır, JSONL'de `ACTION_*` yok; `ENABLED=0` ⇒ komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde; sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Temizlik: ini yedekten geri, geçici betikler silindi, botlar despawn, sunucular `stop`.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `BotCore/` ve `Bot/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **`Moral` 4/6 + `{3, 0}`/`{4, 0}` + `UseItem 0` + `MAGIC.HP < 10000`** skill'i serisinde davranış değiştirir (önceden `unsupported_skill` idi); `Moral` 1/2/7/8/10 ve cast etmeyen botlar için davranış (paket biçimi ve telemetri satırı dahil) değişmez. **Tek istisna `MAGIC.HP >= 10000` kapısıdır:** bot sınıflarında bu yalnızca Sacrifice `106660`'dır ve o skill zaten `unsupported_skill` (`Moral` 4) idi; yani gözlenen davranış değişmez.
- **Thread kuralı (ADR-0005):** `TickCast` yalnızca IOCP thread'inde; bu planda yeni durum/kilit yok.
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `effected` yalnızca `MAGIC_EFFECTING`'ten, `srv_fail` yalnızca `MAGIC_FAIL`'den; `victims` yalnızca çağıranın kendi oturumuna gelen kurban kimlikli `MAGIC_EFFECTING` paketlerinden (üyelik/`PartyID`/HP/MP hiçbir sunucu nesnesinden okunmaz). Party üyeliği, hedefte buff var mı, kimin eksik canlı olduğu **bota önkontrol olarak eklenmez**: bunlar `TeamView` ve `SelfState.buffs` ile karar katmanının (F7) işidir; MP önkontrolü bota ait `user->GetMana()` değeridir (mevcut `CheckCastStart`).
- **Bilinen sınırlar `[A]`/`[D]`:** (a) **CLI-07 `[Ö]`:** gerçek istemcinin grup heal paketinde (`Moral` 6) hedef kimliği/hedef noktası biçimi ölçülmedi; bot alan skill'iyle (F4-29) aynı paket biçimini yazar (`target -1`, `sData[0]`/`sData[2]` = çağıran ya da hedefin konumu). Sunucu `Moral` 6'da tek kimlikli paketi de kabul eder; ölçüm gelirse `docs/03` CLI-07 ve MEC-MAG-18 güncellenir. (b) **`Moral` 6 grup buff'ı `no_result`:** tüm üyeler zaten aynı `BuffType`'a sahipse yayın yoktur; bot bunu `FAILED`/`no_result` yazar ve MP düşmüş olur. Karar katmanı bunu `SelfState.buffs` + `TeamView` ile önceden bilir (F7). (c) **`Moral` 4'te MP yalnızca başarıda** (MEC-MAG-15) ama `Moral` 6'da (EFFECTING'te) **her zaman**; guard `mana >= Msp`'yi her ikisinde de ister. (d) **Ağaç yetersizliği (KI-016):** `BotPHD` buff'ları sunucuca reddedilir; bot önkontrolü yok, karar katmanı profil ağacına göre skill seçer. (e) **`victims` ≠ isabet**: çağıran kendi heal'ı için de bir paket alır, party'siz çağıranda `victims 1`'dir. (f) **Heal'in HP etkisi** doğrudan ölçülür (`list`), `victims` ile birlikte bir kanıt değil iki kanıttır. (g) Sacrifice'ın bota kapatılması `[Ö]`-bağımsız bir güvenlik kararıdır; gerçek oyuncu için de aynı skill sunucuda açıktır (`HP` yetersizse ölür) ve bu plan sunucu davranışını değiştirmez.
- Telemetri hacmi değişmez; `tools/bot-telemetry-report.py` değişmez (`no_result` zaten ayrı sayılır, MET-ACT-02).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-31` (taban `gece/2026-10-02` @ `0e22858`); `126e86e` `[F4-31] Party hedefli skill: Moral 4/6 destegi ve Sacrifice kapisi`
- Değişen dosyalar ve neden:
  - `BotCore/BotCombat.h`: `kMoralPartyAll` / `IsPartyAllMoral` / `SendsAimPoint` eklendi; `CastMoralSupported` kümesine `Moral 4` eklendi ve son koşul `SendsAimPoint`'e bağlandı; `kSacrificeHpCost` / `CastHpCostSupported` eklendi; `CastTargetIdField`/`CastCoordField` yorumları party-all'ı anar (değerler değişmedi).
  - `Tests/BotCoreTests/CombatTests.cpp`: `Combat_CastMoral_Supported` güncellendi (4 ve 6 `true`; `IsAreaMoral(6) == false`); yeni `Combat_PartyCast_Guard` eklendi → toplam 108 test.
  - `GameServer/Bot/ActionExecutor.cpp`: `BeginCast` destek koşuluna `!BotCore::CastHpCostSupported(m->sHP)` eklendi; `TickCast` `bool area = BotCore::SendsAimPoint(m->bMoral);` (tek satır + yorum). `SubmitCast`/`CancelCast`/`RejectCast` değişmedi.
  - `GameServer/Bot/ActionExecutor.h`: `CastTarget`/`BeginCast`/`TickCast` yorumları party hedefli skill'leri kapsar (kod değişikliği yok).
- Derleme sonucu (`tools/build.sh Release`, değişen dört dosya `touch` edilip yeniden derlendi; son satırlar):
  - `proj-AIServer.vcxproj -> ...\AIServer.exe`
  - `BotCoreTests.vcxproj -> ...\BotCoreTests.exe`
  - `proj-GameServer.vcxproj -> ...\GameServer.exe` (rc=0, değişen dosyalarda uyarı/hata 0; Release toplam `warning C` 0, `error C` 0)
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ Release rc=0, değişen dört dosyada uyarı 0 (touch + yeniden derleme).
  - K2 ✔ Debug rc=0, değişen dosyalarda uyarı 0.
  - K3 ✔ Release ve Debug `108 tests, 0 failed`; `Combat_CastMoral_Supported` ve `Combat_PartyCast_Guard` `[ OK ]`.
  - K4 ✔ `grep` yasaklı sembol boş; `#include` yalnızca `<algorithm>`, `<cstdint>`; eklenen satırlarda `std::min`/`std::max` yok.
  - K5 ✔ `CastHpCostSupported` mevcut ve `!BotCore::CastHpCostSupported(m->sHP)` biçiminde; `CastMoralSupported(m->bMoral)`; `bool area = BotCore::SendsAimPoint(m->bMoral)`; `IsAreaMoral` ActionExecutor.cpp'de yok; `(m->bFlyingEffect != 0 && !flyingCast)` yerinde.
  - K6 ✔ `git diff --stat` yalnızca §4'teki 4 dosya; ActionExecutor.cpp yalnızca iki planlı hunk; `SubmitCast`/`CancelCast`/`RejectCast` gövdesinde hunk yok; yasaklı dosyalar ve proje dosyaları değişmedi.
  - K7 ✔ `git diff | grep '^+' | grep 'Emit('` boş; yeni ini/komut/thread/telemetri türü yok.
  - K8 ✔ `file` dört dosya `ASCII text, with CRLF`; `git diff --check` boş.
  - K9 ✔ Guard çağrı sayıları: `CheckMoveStep=2, CheckAttack=1, CheckCastStart=1, CheckCastEffect=1, CheckCastFly=1, CheckCastLand=1, CheckCastCancel=1, CheckPotion=1`; önceki 107 test (güncellenen biri dahil) geçiyor.
  - K10 ✔ `python3 tools/check-perception-contract.py` `RESULT: PASS` (R1 0/0, R2 0/28, R3 0/18, R4 0/0, R5 0/0); denetlenen dosya sayısı değişmedi.
  - K11 (Claude, `/plan-dogrula` çalışma zamanı): uygulayıcı kapsamı dışında; statik kanıt yukarıda.
- Plandan sapmalar: Yok. Planın §5.2b başlangıç guard'ı örneğinde anılmayan `standing`/`needsStanding` alanları testte açıkça atandı (`standing=true`, `needsStanding=false`) ki `CheckCastStart` sırası range→standing→mana olsun; sonuç değişmez.
- Açık sorular: Yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-31` @ `6e287e8` (kod `126e86e`). Çalışma ağacı temizdi; otonom gece döngüsünde (`AUTO_LOOP=1`) birleştirme/push yapılmadı, birleştirmeyi döngü betiği `gece/2026-10-02`'ye yapar.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | dört dosya `touch` + `tools/build.sh Release` rc=0, `error C` 0; tek iki uyarı eski `UpgradeHandler.cpp` C4789 (`BifrostPieceProcess`/`SpecialItemExchange`; bu plan dokunmadı); değişen dört dosyada uyarı yok |
| K2 | ✔ | `tools/build.sh Debug` rc=0, `error C` 0, değişen dosyalarda uyarı yok |
| K3 | ✔ | `run-tests.sh Release` ve `Debug` ikisinde `108 tests, 0 failed`; `[ OK ] Combat_CastMoral_Supported`, `[ OK ] Combat_PartyCast_Guard` |
| K4 | ✔ | `grep "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` boş; eklenen satırlarda `#include`, `std::min`/`std::max`, `Emit(` 0 |
| K5 | ✔ | `ActionExecutor.cpp:738` `!BotCore::CastHpCostSupported(m->sHP)` (tek eşleşme), `:737` `CastMoralSupported(m->bMoral)` (tek), `:817` `bool area = BotCore::SendsAimPoint(m->bMoral)`; `IsAreaMoral` bu dosyada yok; `:735` `(m->bFlyingEffect != 0 && !flyingCast)` yerinde |
| K6 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-31`: yalnızca §4'teki 4 dosya + plan dosyası; `ActionExecutor.cpp` iki hunk (`BeginCast` destek koşulu `:733-738`, `TickCast` `area` satırı ve yorumu `:812-817`); `SubmitCast`/`CancelCast`/`RejectCast` hunk'ı yok; vcxproj, `BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `docs/`, `tools/` farkı yok |
| K7 | ✔ | eklenen satırlarda `Emit(` yok; yeni ini anahtarı/komut/thread/olay türü yok; `ENABLED=0` ve `TELEMETRY=summary` çalışma zamanında sınandı (aşağıda) |
| K8 | ✔ | `file`: dört dosya ASCII + CRLF (taban ile aynı); `git diff --check` boş |
| K9 | ✔ | `CheckMoveStep` 2; `CheckAttack`, `CheckCastStart`, `CheckCastEffect`, `CheckCastFly`, `CheckCastLand`, `CheckCastCancel`, `CheckPotion` 1'er; 108 testin tamamı geçiyor (önceki 107 dahil) |
| K10 | ✔ | `check-perception-contract.py` `RESULT: PASS` (`files scanned: 19`, değişmedi) |
| K11 | ✔ | S1–S7 çalışma zamanında geçti (aşağıda; S1'de `victims` 3, plandaki ≥ 4 değil: bulgu 1; S7'de Group heal `112570` `srv_fail`, plandaki `unsupported_skill` değil: bulgu 2) |

**Çalışma zamanı** (Release, `ENABLED=1`, `MAX_BOTS=16`, `TELEMETRY=decisions`, zone 71; `Logs/bots/2026-10-03/live-032934.jsonl`, `Logs/Bot_3_10_2026.log`; ini yedekten geri yüklendi (taban ile aynı), `BotCommands.txt` silindi, sunucular durduruldu, `GameServer.log` bu oturumda değişmedi):

- **S1 ✔** Party: `BotPHB_K` + `BotWP_K` + `BotWG_K` + `BotMF_K` (`snap`: `members 3`). `BotWP_E` ile `BotWP_K`'ya saldırtıldı (HP 5288/6610). `cast BotPHB_K 112557 self 1`: `CastStart` `ACTION_SUBMIT` **`"target":-1`** → `casting` `op 1`; `CastEffect` `"target":-1` → `effected`, `op 3`, `code 0`, **`victims 3`**; log `cast finished (effected) after 1 cycle(s), 1 ok, 2 packet(s) sent`; **`BotWP_K` HP 5288 → 6248 (+960)**; MP 4132 → 3212 (−960 + ~40 yenileme); `snap BotPHB_K events`: kurban kimlikli `op=3 skill=112557 target=2987/2986/2984` + hedef `-1`'li son `op=3` (d0=1274, d2=890) + `op=1`. **Yarıçap kanıtı:** `BotMF_K` çağırandan 35 m (< 56) ama hedef noktasından (priest) 35 m > `Radius 30` olduğu için sayılmadı (bulgu 1). Ek: `cast BotPHB_K 112557 BotMF_K 1` (hedef noktası = `BotMF_K` konumu, z=925) ⇒ `victims 2` (`BotMF_K` + çağıran; olaylar `d2=925`), yani hedef noktası paketten okunuyor.
- **S2 ✔** Party'siz taze oturum, `cast BotPHB_K 112557 self 1`: `"target":-1`, `effected`, `code 0`, **`victims 1`** (aynı noktadaki party dışı `BotWG_K` sayılmadı), MP 6392 → 5432 (−960); `112560`: `effected`, MP 5432 → 3592 (−1920 + ~80 yenileme).
- **S3 ✔** Taze oturum, party (priest + `BotWP_K` + `BotWG_K`): `cast BotPHB_K 112656 self 1`: `casting` → **`effected`, `code 600`, `victims 3`**, MP 3372 → 2882 (−570 + yenileme); `snap BotWP_K`: `buff skill=112656 type=1 remain=588s`; MaxHP üçünde de +1200 (3491 → 4691, 5650 → 6850). **Hemen yeniden** `cast BotPHB_K 112656 self 1`: `CastEffect` `ok:false`, **`reason:"no_result"`**, `op -1`, `victims 0`; log `cast stopped (no_result)`; **MP 2922 → 2392 (−570 yine düştü, yenileme dahil −530)**: MEC-MAG-18 tahmini aynen çıktı.
- **S4 ✔** Party (priest + `BotWP_K`): `cast BotPHB_K 112642 BotWP_K 1`: `CastStart` **`"target":2986`** (≠ −1) → `casting`; `CastEffect` `"target":2986` → `effected`, `code 600`, `victims` alanı yok; MP 3872 → 3672 (−240 + ~40 yenileme); `snap BotWP_K`: `buff skill=112642 type=1 remain=594s`. Aynı `BuffType` hedefteyken `cast BotPHB_K 112615 BotWP_K 1`: `ok:false`, **`srv_fail`**, `op 4`, `code -100`, MP 3712 → 3752 (yalnızca yenileme). Kendine `cast BotPHB_K 112624 self 1`: `"target":2984` (priest kimliği), `effected`, `code 600`; `snap`: `buff skill=112624 type=1`.
- **S5 ✔** Party yok (taze oturum): `cast BotPHB_K 112615 BotWP_K 1` ⇒ `srv_fail` (`op 4`, `code -100`); `cast BotPHB_K 112615 BotWP_E 1` (düşman) ⇒ `srv_fail`; MP yalnızca yenilendi (2412 → 2492); `cast BotPHB_K 112615 self 1` ⇒ `effected`, `code 600` (party gerekmedi), MP 2492 → 2502 (`Msp 30`, `MAGIC` tablosundan: −30 + ~40 yenileme).
- **S6 ✔** (a) `BotPHD_K` (buff ağacı 0, KI-016): `cast BotPHD_K 112615 self 1` ⇒ bot kabul etti, `CastStart` `ACTION_RESULT` `ok:false`, `srv_fail`, `op 4`, `code -100`, MP 6392 → 6392; `cast BotPHD_K 112557 self 1` ⇒ `effected`, `victims 1`, MP −920 (+ yenileme). (b) `BotPHD_K` 72 m uzağa taşındı: `cast BotPHB_K 112560 BotPHD_K 1` ⇒ **paket gitmeden** `FAIRNESS_REJECT` `MEC-MAG-11` `out_of_range`, `value 72.00`, `limit 56.00`, log `cast stopped (out_of_range)`. (c) CASTING'te `cast ... off` (since_casting 1094 ms): `112557` ⇒ `CastCancel` **`"target":-1`**, `cancelled`, `op 4`, `code -100`, MP 3752 → 3792 (yalnızca yenileme); `112642 BotWP_K` ⇒ `CastCancel` **`"target":2986`**, `cancelled`, MP değişmedi.
- **S7 ✔** Sacrifice `cast BotWG_K 106660 BotWP_K 1` ⇒ `refused (unsupported_skill)`, jsonl'de `106660`/`106650` için `ACTION_SUBMIT` 0, `BotWG_K` HP 5650/5650 değişmedi (`MAGIC` tablosu: `106660` `Moral 4`, `HP 10001`, `Msp 180`). `unsupported_skill`: Bless of God `112671`, Cure curse `112525`, Counter Curse `112676` (`BotPHB_K`), descent `106650` (`BotWG_K`), summon friend `110004`, Escape `110035` (`BotMF_K`). `112575` ⇒ `quest_locked`; `112570` ⇒ bot kabul etti, sunucu `srv_fail` (bulgu 2). Gerilemesiz: `cast BotPHB_K 112603 self 1` (Moral 2) `effected` `code 600`; F4-29 Inferno `110545` `BotMF_K` → `BotWP_E`: `target -1`, `effected`, `victims 1`, MP −200; Moral 7 `110518`: `CastEffect` `"target":2989` (kurban kimliği), `victims` yok; F4-30 `110533`: `CastFly` + `CastEffect` `target -1`, `victims 1`, MP 5881 → 5621 (2 × 150 − yenileme). `TELEMETRY=summary`: grup heal `effected`, jsonl'de `ACTION_` satırı 0, `PERF_SAMPLE` `tick_p95_us` 82–96 (spawn penceresinde 1350, önceki planlarla aynı); `ENABLED=0`: `BotCommands.txt` işlenmedi (dosya kaldı), `Bot_*.log`'a satır ve yeni jsonl yok; sunucu 3/3 UP, `GameServer.log`'a yeni hata yok.

- Bulgular (önem sırasıyla; engelleyici yok):
  1. *(not)* **S1 beklentisi düzeltildi:** plan `victims ≥ 4` (priest + üç üye) diyordu; ölçülen 3. `BotMF_K` doğuş noktasında priest'ten 35 m uzakta (z=925) olduğundan `Radius 30` dışında kaldı. Bu beklenen sunucu kuralıdır ve yarıçap kuralını kanıtlar (`MagicProcess.cpp:113-141`); `BotMF_K` hedef alınınca (`cast BotPHB_K 112557 BotMF_K 1`) `victims 2` ve olaylarda `d2=925`.
  2. *(not)* **Group heal `112570` (Moral 6, `SkillLevel 70`, ağaç 60) `unsupported_skill` değil `srv_fail`:** `Moral 6` + `{3, 0}` + `UseItem 0` artık desteklenir (plan §2 "erişilemez" demişti); bot kabul eder, sunucu `IsAvailable()` seviye/ağaç denetimiyle CASTING'te reddeder, MP düşmez (KI-016 ile aynı sınıf). Plan §7 S7'deki beklenti bu skill için yanlıştı; kod doğru.
  3. *(not)* **Kurulum notları (plan §7):** (a) `110004`/`110035` `BotPHB_K` ile `bad_skill` (sınıf uyuşmazlığı); `BotMF_K` ile `unsupported_skill` görüldü. (b) `Greatness 112656` `Range 101`, `112671` `Range 45` (plan §2 menzil notu yalnızca 56'yı anar; guard `MAGIC.Range`'i kullanır). (c) Brave `Msp 30`, Strong `60` (plan "15–690" aralığında). (d) Sunucu açılışında `GameServer.ini`'ye `[BOT]` anahtarlarının tamamını yazıyor (çalışma zamanı ini'si yedekten geri yüklendi). (e) MP/HP botlar arasında DB'de kalıcı (despawn sonrası `BotPHB_K` MP ≈ 3000'den başlar); `Msp` toplamları ~5-6 MP/sn yenilemeyle okunur.
  4. *(not)* **Sonuç sözleşmesi doğrulandı, `docs/03` MEC-MAG-18 `[D]` → `[V]`:** `Moral` 4 (hedef kimliği, MP yalnızca başarıda, aynı `BuffType` ⇒ CASTING'te `srv_fail`, party dışı/düşman hedef ⇒ `srv_fail`, kendine party'siz `effected`), `Moral` 6 grup heal (`-1` + hedef noktası, `victims` = çağıran + yarıçap/menzil içi party üyesi, MP bir kez, party'siz `victims 1`), `Moral` 6 grup buff (`code` = süre, tümünde varsa `no_result` ve MP düşer), Sacrifice kapısı çalışma zamanında ölçüldü.
  5. *(not)* Uygulayıcı raporu doğru: commit listesi, dosyalar, derleme ve test sayıları kendi çalıştırmamla örtüşüyor; tek sapma notu (`standing`/`needsStanding` açık atama) yerinde ve zararsız.
  6. Sınanmayanlar (plan kapsamı dışı/bilinen sınır): gerçek istemcinin grup skill paketi biçimi (CLI-07 `[Ö]`), güvenli bölgede grup heal `srv_fail` (botlar güvenli bölgede değildi), El Morad karşılıkları (`2xxxxx`; aynı kod yolu), Group heal'in çağırandan > 56 m üyeyi atlaması (üye taşınmadı; çağırandan mesafe kuralı `UserRegionCheck` + `:1337-1339`, kod okumasıyla `[D]`).
- Düzeltme talimatı: yok (karar DOĞRULANDI).
