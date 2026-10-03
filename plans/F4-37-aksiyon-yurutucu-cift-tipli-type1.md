# F4-37: `ActionExecutor` çift tipli Type1 skill dilimi — `{1, 3}` ve `{1, 4}` çiftleri (warrior Scream, Shock Stun, Exceed Break, leg cutting; ADR-0018 dilim 6f)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; kapsam ADR-0018 ile genişletildi) |
| Branch | `bot/F4-37` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-36 (`CastItemSkillSupported` + `no_item` kuralı) — `KAPANDI` (merge `bc15a0b`); F4-26 (`{3, 4}` çifti, tip damgaları iki tip için) — `KAPANDI`; F4-28 (Type4 tek tipli), F4-27 (`quest_locked`) — `KAPANDI` |
| İlgili gereksinim / kabul | CLI-03, CLI-04, CLI-11, MEC-MAG-03, MEC-MAG-08, MEC-MAG-11, MEC-MAG-13, MEC-MAG-14, MEC-MAG-15, MEC-MAG-23, MEC-MAG-24 (bu planla eklendi, `[D]`), SK-06, AC-LRN-03, MET-ACT-02, MET-FAIR-01 altyapısı; ADR-0018 Ek 1 madde 1(e) (warrior parçası), Ek 3, Ek 4, Ek 12, Ek 13; `docs/05` satır 74-76 (Scream, Exceed Break, Shock Stun), `docs/06` §3-§6 (warrior kontrol), `docs/17` §2.1 "Warrior kontrol" |
| Tahmini büyüklük | S (3 dosya: `BotCombat.h`, `CombatTests.cpp`, `ActionExecutor.h`; **`ActionExecutor.cpp` değişmez**; yeni dosya yok, proje dosyaları değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

`BotCore::CastTypesSupported` bugün yalnızca tek tipli (`type1 == 0`: Type1, 3, 4, 5) ve `{3, 4}` çiftini kabul eder (`BotCore/BotCombat.h:322-328`); `{1, 3}` ve `{1, 4}` çiftleri `unsupported_skill` ile reddedilir. Oysa warrior'ın kontrol/baskı skill'lerinin hepsi bu çiftlerdir (`docs/05` satır 74-76, `docs/06` §6 "Kaçışı engelleme", `docs/17` §2.1 "Warrior kontrol"):

- **Scream** `106802`/`206802` — `{1, 4}` (Type1 %250 hasar + Type4 hız debuff'ı `BuffType 6`, süre 7 sn), `UseItem 379063000` (Scream Scroll) + `BeforeAction 1` ⇒ 1 Stone of Warrior tüketir;
- **Exceed Break** `106815`/`206815` ve **Shock Stun** `106820`/`206820` — `{1, 3}` (Type1 hasarı + Type3), `UseItem 379059000` (Stone of Warrior; `BeforeAction 0` ⇒ `UseItem`'in kendisi tüketilir);
- **Leg cutting** `106520`/`206520` (novice/master 105/205 karşılığı `105520`/`205520` başka sınıf) — `{1, 4}`, eşyasız, hız debuff'ı süre 10 sn;
- aynı kapıyla verisi gereği açılanlar (ölçümü §7'de kısmen yapılır): Blooding `106575`/`206575` (`{1, 3}`, `Etc 510` quest), mage asa skill'leri `110542`/`110642`/`110742` ve `110572`/`110672`/`110772` ve El Morad karşılıkları (`{1, 3}`, `Range 11/22`), rogue Blinding `108675`/`208675` (`{1, 4}`, `UseItem 379060000`, `Etc 512`).

Bu skill'lerin geri kalan tüm kuralları **zaten desteklenen** kurallardır: `Moral 7` (düşman), uçmayan (`FlyingEffect 0`), `CastTime 0` (tek EFFECTING, `casting` aşaması yok), eşya kapısı F4-36'dan (`CastItemSkillSupported(1, ...)` `true` döner), tip damgaları ve same-type kapısı iki tip için F4-26'dan (`ActionExecutor.cpp:907-921`, `:1057-1065`). Eksik olan tek şey **tip çifti kapısıdır**.

Bu plan:

1. `CastTypesSupported`'a `{1, 3}` ve `{1, 4}` çiftlerini ekler (başka hiçbir çift açılmaz);
2. birim testlerini günceller (iki mevcut test `false` → `true`, bir yeni test);
3. `ActionExecutor.h` yorumunu günceller;
4. sunucu davranışını (çift yankı, MP, eşya, menzil) belgeler ve çalışma zamanında doğrular.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` "Ek (F4-37)" (bu planla birlikte yazıldı), "Ek (F4-26)" (çift tipli Type3+Type4), "Ek (F4-36)" (`no_item`). `docs/adr/ADR-0018-f4-kapsam-genisletme-aksiyon-destegi.md` Ek 3 ("Type1+Type4 melee çiftleri: warrior davranışı için ayrı küçük dilim"), Ek 4, Ek 12 (6f'nin ertelendiği yer), Ek 13.
- `docs/03` §4.2 **MEC-MAG-13** (çift tipli yankı kuralı), **MEC-MAG-15** (Type4 aynı `BuffType` reddi), **MEC-MAG-23** (eşya kuralı), **MEC-MAG-24** (bu planla eklendi); `docs/05` satır 74-76; `docs/06` satır 36-39, 55, 154.
- `plans/F4-36-aksiyon-yurutucu-esya-skilleri.md` (eşya kapısı), `plans/F4-26-aksiyon-yurutucu-cift-tipli-skill.md` (çift tip kalıbı); **yazılı planları değil, kodu esas al**.
- İlgili kod (Claude tarafından `gece/2026-10-02` @ `bc15a0b` üzerinde doğrulandı; satırlar kayabilir, uygulayıcı önce yeniden bakar):
  - `BotCore/BotCombat.h:317-328` `CastTypesSupported(type0, type1)`: `type1 == 0` ise `type0 ∈ {1, 3, 4, 5}`, değilse yalnızca `type0 == 3 && type1 == 4`. `:333-336` `CastTypeMoralSupported(type0, moral)` (Type5 dışında her şey geçer), `:369-375` `CastMoralSupported` (7 geçer), `:466-476` `CastItemSkillSupported` (`type0 == 1` geçer), `:478-486` `CastConsumeItem`, `:502-505` `IsGatedType` (1..7), `:524-540` `MinGatedSince`.
  - `GameServer/Bot/ActionExecutor.cpp:766-778` `BeginCast` destek koşulu (`CastTypesSupported(m->bType[0], m->bType[1])` ilk terim; **bu satıra dokunulmaz**), `:794-801` `no_item` (diriltme dışında `UseItem != 0` ise `CanUseItem` iki eşya), `:803-811` `bad_target` (`Moral 7` ⇒ `wantedTarget`: `self` ⇒ `bad_target`), `:907-921` `TickCast` tip damgaları iki tip için, `:1055-1065` damga yazımı iki tip için, `:625-655` `SubmitCast` yankıyı **son yazılan** `m_castEcho`'dan okur (`code == SKILLMAGIC_FAIL_ATTACKZERO` ⇒ `missed`, aksi `effected`).
  - `GameServer/Bot/BotSession.cpp:72-86` `OnPacket()`: yalnızca **çağıranın kendi** `WIZ_MAGIC_PROCESS` paketlerini (`op` 1..4) `m_castEcho`'ya yazar; her yeni paket öncekini ezer.
  - `GameServer/MagicInstance.cpp:109-133` `Run()` EFFECTING: `ExecuteSkill(bType[0])` başarılıysa tip damgaları (`bType[0]` ve `bType[1]` için `m_MagicTypeCooldownList`) yazılır, `ExecuteSkill(bType[1])` çalışır, `bType[0] != 2` ise `ConsumeItem()`. **`ExecuteSkill(bType[1])`, `bType[0]` başarısız (hedef ölü) olsa bile çalışmaz** (`if (bInitialResult)` bloğu içinde).
  - `GameServer/MagicInstance.cpp:906-930` `IsAvailable()` ön koşulları **iki tip için** (`for i < 2`): `case 3` `CheckType3Prerequisites()` (`:446-533`; `Moral 7 > MORAL_PARTY` ⇒ düşman hedefte `true`), `case 4` `CheckType4Prerequisites()` (`:540-`; **buff'lar için aynı `BuffType` hedefte varsa `false`**, debuff'lar (`isDebuff`) için engel yoktur).
  - `GameServer/MagicInstance.cpp:278-281` ve `GameServer/User.cpp:5047-5110` `isInAttackRange(hedef, skill)`: `bType[0] < 4` ise çağıranın silahına bağlı menzil denetimi; Type1'de `15 + (sRange == 0 ? silah menzili : sRange) + silah menzili` metre (sunucu çok cömert; bot kendi `CastInRange` kuralını kullanır: `sRange > 0` ⇒ `mesafe < sRange`, `sRange == 0` ⇒ silah menzili alanı).
  - `GameServer/MagicInstance.cpp:965-996` Type1 + `sSkill ∈ {1055, 2055}` (çift silah) ve `{1056, 2056}` (iki elli) silah doğrulaması: `return false`, **yayınsız**. Bu `sSkill` değerleri 105/205 sınıfına aittir (`Skill / 10 != GetClass()` ⇒ bot `bad_skill`); 106/206/110/210/112/212 botları için geçerli değildir.
  - `GameServer/MagicInstance.cpp:1011-1012`, `:1028-1029` `IsAvailable()` EFFECTING: `Msp > mana` ⇒ `fail_return`, ardından **`bType[0] != 4` ise `MSpChange(-Msp)`**: `{1, x}` çiftlerinde MP `IsAvailable()`'da **bir kez** düşer. `:1800-1802` `ExecuteType4` yalnızca `bType[0] == 4` iken ayrıca MP düşer ⇒ `{1, 4}`'te **çift ödeme yoktur**.
  - `GameServer/MagicInstance.cpp:1061-1115` `ExecuteType1`: hedef ölü değilse `GetDamage` + `HpChange`; `sData[3] = (damage == 0 ? SKILLMAGIC_FAIL_ATTACKZERO : 0)` (`:1109`); **`SendSkill()` ile bölgeye hedef kimlikli EFFECTING yayınlar** (`:1112`); hedef ölüyse hasarsız yayın, `false` döner. `GameServer/MagicInstance.cpp:1263-1616` `ExecuteType3`: hedef ölü/yok ⇒ `false` (yayın yok); `sRange > 0` ve mesafe `>= sRange` ⇒ o hedef atlanır (`:1337-1339`); `bType[1] == 0 || bType[1] == 3` iken hedef kimlikli yayın (`:1598-1599`, **ortak `sData` kullanır: `sData[3]` hâlâ Type1'in `0`/`-104` değeridir**, `sData[1]` Type3 tarafından `1` yapılır). `GameServer/MagicInstance.cpp:1618-1896` `ExecuteType4`: `bType[1] == 0 || bType[1] == 4` iken `{sData[0], bResult, sData[2], süre(sn), sData[4], hız, sData[6]}` yayını (`:1862-1874`); hız/stun debuff'ında (`BuffType` 6/`SPEED2`/`STUN`, `Moral 7`) direnç zarı (`:1819-1848`) başarısızsa yine yayın gider.
  - `GameServer/MagicInstance.cpp:389-423` same-type kapısı iki tip için ayrı (`m_MagicTypeCooldownList`, `PLAYER_SKILL_REQUEST_INTERVAL`); `{1, 3}`/`{1, 4}` bu yüzden hem Type1 hem Type3/4 damgasını yazar. `:2983-3001` `ConsumeItem()` (F4-36): `nConsumeItem` listede olmayan 379059000 (Stone of Warrior) her atışta 1 azalır; 379063000 (Scream Scroll) tüketilmez.
  - `Tests/BotCoreTests/CombatTests.cpp`: `Combat_CastTypes_Supported` `:1019-` içinde `:1033-1034` `CastTypesSupported(1, 3)`/`(1, 4)` **`false`** bekliyor; `Combat_ItemSkill_Guard` `:1650-` içinde `:1672-1673` aynı iki satır **`false`** bekliyor. Başka yerde `CastTypesSupported(1, ...)` yok. Toplam birim test **248**.
- **Veri notu (Claude yerel `MAGIC`, `MAGIC_TYPE1/3/4` tablolarında doğruladı; oyun verisidir, kişisel veri değil).** `Type1 = 1` ve `Type2 ∈ {3, 4}` olan, bot sınıflarına (106/206/110/210/112/212; 105/205/108/208/109/209 başka sınıf) ait `MagicNum < 300000` satırları:

  | MagicNum (K / E) | Ad | Çift | Sınıf / ağaç | `Msp` | `ReCastTime` (0,1 sn) | `Range` | `UseItem` / `BeforeAction` | `Etc` | Diğer |
  |---|---|---|---|---|---|---|---|---|---|
  | `106520` / `206520` | leg cutting | `{1, 4}` | 106/206, `Skill 1065` ⇒ indeks 5, `SkillLevel 20` | 84 | 51 | 0 | yok | 0 | Type1 `Hit 100`; Type4 `BuffType 6`, `Duration 10`, `Speed 50` |
  | `106575` / `206575` | blooding | `{1, 3}` | 106/206, indeks 5, `SkillLevel 75` | 350 | 210 | 0 | yok | **510** | Type1 `Hit 350`, `AddDamage 400`; Type3 DoT `TimeDamage −1000`, `Duration 20` |
  | `106802` / `206802` | Scream | `{1, 4}` | 106/206, `Skill 1068` ⇒ indeks 8, `SkillLevel 2` | 300 | 101 | 0 | `379063000` / **1** (Stone of Warrior tüketir) | 0 | Type1 `Hit 250`, `AddDamage 200`; Type4 `BuffType 6`, `Duration 7`, `Speed 1` |
  | `106815` / `206815` | Exceed Break | `{1, 3}` | indeks 8, `SkillLevel 15` | 400 | 254 | 0 | `379059000` / 0 (taşın kendisi tüketilir) | 0 | Type1 `Hit 200`; Type3 `DirectType 13`, `FirstDamage −1000` (dayanıklılık) |
  | `106820` / `206820` | Shock Stun | `{1, 3}` | indeks 8, `SkillLevel 20` | 250 | 252 | 0 | `379059000` / 0 | 0 | Type1 `Hit 175`, `AddDamage 175`; Type3 `DirectType 0`, `Attribute 3` |
  | `108675` / `208675` | Blinding | `{1, 4}` | 108/208 (rogue; bot yok) | 200 | 254 | 0 | `379060000` / 0 | **512** | ölçülmez |
  | `110542` / `210542` | fire blade | `{1, 3}` | 110/210, `Skill 1105` ⇒ indeks 5, `SkillLevel 42` | 100 | 0 | 11 | yok | 0 | Type1 `Hit 100`; Type3 `DirectType 1`, `FirstDamage −336`, ateş |
  | `110642` / `210642` | frozen blade | `{1, 3}` | indeks 6, `SkillLevel 42` | 100 | 0 | 11 | yok | 0 | buz |
  | `110742` / `210742` | charged blade | `{1, 3}` | indeks 7, `SkillLevel 42` | 100 | 0 | 11 | yok | 0 | yıldırım |
  | `110572`/`110672`/`110772` ve El Morad | Fire/Ice/Light Staff | `{1, 3}` | indeks 5/6/7, `SkillLevel 72` | 300 | 0 | 22 | yok | **515** | ölçülmez (quest) |

  Hepsi `Moral 7`, `CastTime 0`, `FlyingEffect 0`, `HP 0`, `UseStanding 0`. Çıkarım `[D]`: yukarıdaki tablo `SELECT` ile alınmıştır; uygulayıcı çalışma zamanında DB'ye bağlanmaz.
  - **`ITEM`:** `379059000` Stone of Warrior (`Class 0`, `ReqLevel 1..99`, `Countable 1`), `379063000` Scream Scroll (`Countable 0`), `379060000` Stone of Rogue (ölçülmez).
- **Bot karakter notu (`db/002_bot_characters.sql:124-137`, `:205-216`).** Skill ağacı baytları: `BotWP_*` `0x00000000004600341400` ⇒ `[5] = 70`, `[6] = 0`, `[7] = 52`, `[8] = 20`; `BotWG_*` `0x00000000003C3E001400` ⇒ `[5] = 60`, `[6] = 62`, `[7] = 0`, `[8] = 20`; `BotMF_*` `[5] = 70, [6] = 52, [7] = 0, [8] = 20`; `BotMI_*` `[5] = 52, [6] = 70, [7] = 0, [8] = 20`. Sonuç: leg cutting (indeks 5, lv 20), Scream/Exceed Break/Shock Stun (indeks 8, lv ≤ 20) tüm warrior botlarında ağaçtan geçer; Blooding (indeks 5, lv 75) hiçbirinde geçmez (70/60 < 75) ve `Etc 510` quest'i de kilitlidir; mage `110542` yalnızca `BotMF_*` (`[5] = 70 ≥ 42`) ve `BotMI_*` (`[5] = 52 ≥ 42`), `110642` iki mage'de (`[6]` 52/70 ≥ 42), `110742` hiçbirinde (`[7] = 0`). Çanta (slot 18-19): warrior `379059000` ×50 + `379063000` ×1 (`no_item` kuralı için yeterli); mage asa skill'leri eşyasızdır. Her botun seviyesi 80.

## 3. Kapsam

**Yapılacaklar**

1. **Saf mantık (`BotCore/BotCombat.h`):** `CastTypesSupported` yalnızca `type1 != 0` dalında genişler: `(type0 == 3 && type1 == 4) || (type0 == 1 && (type1 == 3 || type1 == 4))`; yorum güncellenir (§5.1).
2. **Birim testleri (`CombatTests.cpp`):** `Combat_CastTypes_Supported` `:1033-1034` ve `Combat_ItemSkill_Guard` `:1672-1673` satırları `true` olur; yeni `Combat_Type1Pair_Guard` (248 → 249) (§5.2).
3. **Yorum (`ActionExecutor.h`):** `BeginCast` yorumundaki "dual-typed Type1 + Type3/4 skills (Scream, Shock Stun) stay unsupported" ifadesi ve `TickCast` "dual-typed" yorumu güncellenir (§5.3). **`ActionExecutor.cpp` değişmez.**
4. **Sonuç sözleşmesi (§5.4):** `docs/03` MEC-MAG-24 çalışma zamanında sınanır (S1–S8).

**Kapsam dışı (yapılmayacak)**

- Başka hiçbir tip çifti: `{1, 9}` (stealth/lupine, `108680`), `{1, 1}`, `{1, 2}`, `{1, 5..8}`, `{2, x}` (okçu), `{3, 1}`, `{3, 3}`, `{4, 3}`, `{4, 4}`, `{5, x}`, `{0, x}` (dönüşüm/disguise scroll) ve Type6/7/8/9 çiftleri **kapalı kalır** (birim testiyle sabitlenir).
- Alan/party/uçan `{1, x}` çiftleri: bu çiftlerin hiçbirinin verisi yoktur (hepsi `Moral 7`, `FlyingEffect 0`); `BeginCast` mevcut `Moral`/uçan kuralları aynen geçerlidir.
- **Karar katmanı:** Scream/leg cutting'in ne zaman kullanılacağı (P-WAR-SLOW-TRIGGER), Stone of Warrior bitince devre dışı bırakma (`docs/06` satır 154), MP rezervi (P-WAR-MP-RESERVE), hedef hızı/kaçış ölçümü, aynı `BuffType` hedefte var mı önkontrolü F6'nın işidir. Bota **taş sayısı/stok önkontrolü** (yalnızca F4-36'nın "≥ 1 adet" `no_item` kuralı), **debuff etkisi önkontrolü** ve **silah doğrulaması** (`sSkill 1055/1056`) eklenmez (AC-LRN-03).
- Stun/yavaşlama **etkisinin** hedefte gerçekleştiğinin ölçümü (hedefin `BuffType 6` durumu, hız): algı tarafında `E` sınıfı gözlem (F4-53 TASLAK) ve istemci tarafı hareket etkisi kapsam dışıdır; yalnızca yayın sonucu (`effected`/`missed`/`srv_fail`/`no_result`) ölçülür.
- Yeni komut/ini anahtarı/telemetri olayı türü/alan, `BotSession.*`/`BotManager.cpp` değişikliği, `tools/` betik değişikliği, `ActionExecutor.cpp` değişikliği, `TickCast`/`SubmitCast`/`OnPacket()` değişikliği.
- Envanter doldurma/yeniden stoklama (taş 50 → azalır; `db/002` yeniden uygulaması ayrı dilim m.8), CLI-12, T-MECH-SKILL botla koşusu.
- `docs/`, `plans/README.md`, ADR dosyaları (Claude'un işi).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCombat.h` | değiştir | yalnızca `CastTypesSupported` gövdesi ve üstündeki yorum (`:317-328`); başka fonksiyon değişmez |
| `Tests/BotCoreTests/CombatTests.cpp` | değiştir | iki mevcut satır çifti (`:1033-1034`, `:1672-1673`) `false` → `true`; yeni `Combat_Type1Pair_Guard` dosyanın sonuna (248 → 249); başka test değişmez |
| `GameServer/Bot/ActionExecutor.h` | değiştir | yalnızca iki yorum (`BeginCast` "dual-typed Type1 + Type3/4" ifadesi `:230-231`, `TickCast` "dual-typed" yorumu `:244-245`) |

`ActionExecutor.cpp`, `BotSession.*`, `BotManager.cpp`, `Telemetry.*`, `ScenarioRunner.*`, `ScriptRunner.*`, `tools/` ve proje dosyaları **değişmez**. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz; plan dışı düzeltme yapma.

## 5. Uygulama adımları

### 5.1 Saf mantık (`BotCore/BotCombat.h`)

Önce `grep -rn "CastTypesSupported" BotCore GameServer Tests` ile tüm kullanım yerlerini listele (beklenen: tanım, `ActionExecutor.cpp:768`, `CombatTests.cpp` iki test). `CastTypesSupported`'ı (`:317-328`, yorumuyla birlikte) şu hâle getir (ASCII, CRLF, tab, Allman; yeni `#include` yok; **`std::min`/`std::max` kullanma**):

```cpp
	// --- dual-typed cast (ADR-0017 Ek F4-26), single Type4 cast (Ek F4-28) and Type1 pairs (Ek F4-37) ---

	// MAGIC.Type1/Type2 pairs the bot casts (docs/03 MEC-MAG-13, MEC-MAG-15, MEC-MAG-19, MEC-MAG-24): a single type 1, 3, 4
	// or 5 (5 = cure, see CastTypeMoralSupported), the pair Type3 + Type4 (the server runs Type3 first and Type4 second on
	// the same target), or a melee pair Type1 + Type3 / Type1 + Type4 (warrior Scream, Shock Stun, Exceed Break, leg
	// cutting; the server runs the Type1 hit first, then the Type3 / Type4 part). Every other pair stays unsupported.
	inline bool CastTypesSupported(uint8_t type0, uint8_t type1)
	{
		if (type1 == 0)
			return type0 == 1 || type0 == 3 || type0 == 4 || type0 == 5;

		if (type0 == 1)
			return type1 == 3 || type1 == 4;

		return type0 == 3 && type1 == 4;
	}
```

Dosyanın kalanı **değişmez** (`CastTypeMoralSupported`, `CastMoralSupported`, `CastItemSkillSupported`, `CastConsumeItem`, `IsGatedType`, `MinGatedSince` aynen). Mevcut bölüm başlık yorumu (`// --- dual-typed cast (ADR-0017 Ek F4-26) and single Type4 cast (ADR-0017 Ek F4-28) ---`) yerine yukarıdaki satır gelir; başka silme/taşıma yok. `BotCore/` sunucu başlığı içermez (K4).

### 5.2 Birim testleri (`Tests/BotCoreTests/CombatTests.cpp`)

**a) Mevcut testlerin düzeltilmesi (zorunlu; yapılmazsa K3 kırılır).**
- `Combat_CastTypes_Supported` içinde `:1033-1034`: `CHECK_EQ(BotCore::CastTypesSupported(1, 3), false);` ve `(1, 4)` satırları **`true`** olur. `:1035` `(1, 9)` `false` kalır.
- `Combat_ItemSkill_Guard` içinde `:1672-1673`: `CastTypesSupported(1, 4)` ve `(1, 3)` **`true`** olur (Scream/Shock Stun artık tip kapısından geçer; eşya kapısı zaten `true`).
- Başka mevcut satır değişmez. Her iki testteki ilgili satırların üstüne tek satır İngilizce yorum ekle (`// F4-37: melee pairs are supported`).

**b) Yeni `Combat_Type1Pair_Guard`** (dosyanın sonuna, `Combat_ItemSkill_Guard`'dan sonra; çerçeve makroları dosyadaki mevcut testlerle aynı: `TEST_CASE`, `CHECK_EQ`):
- Açılan: `CastTypesSupported(1, 3) == true`, `(1, 4) == true`.
- Değişmeyenler (tek tipli ve eski çift): `(1, 0)`, `(3, 0)`, `(4, 0)`, `(5, 0)` `true`; `(3, 4)` `true`; `(0, 0)` `false`, `(2, 0)` `false`, `(6, 0)` `false`, `(7, 0)` `false`, `(8, 0)` `false`, `(9, 0)` `false`.
- **Kapalı kalan çiftler** (hepsi `false`): `(1, 1)`, `(1, 2)`, `(1, 5)`, `(1, 6)`, `(1, 7)`, `(1, 8)`, `(1, 9)`; `(2, 3)`, `(2, 4)`; `(3, 1)`, `(3, 2)`, `(3, 3)`; `(4, 1)`, `(4, 3)`, `(4, 4)`; `(5, 3)`, `(5, 4)`; `(0, 3)`, `(0, 4)` (dönüşüm/disguise: `bType[0] == 0`); `(6, 4)`, `(8, 4)`, `(9, 4)`.
- Eşya kapısı değişmedi ve Scream ile birlikte çalışır: `CastItemSkillSupported(1, 1068, 379063000) == true` (Scream), `CastItemSkillSupported(1, 1068, 379059000) == true` (Shock Stun / Exceed Break), `CastItemSkillSupported(1, 1065, 0) == true` (leg cutting, eşyasız).
- Tüketilen eşya: `(int)BotCore::CastConsumeItem(1, 379063000) == 379059000` (Scream: sınıf taşı), `(int)BotCore::CastConsumeItem(0, 379059000) == 379059000` (Shock Stun / Exceed Break: `UseItem`'in kendisi), `(int)BotCore::CastConsumeItem(0, 0) == 0` (leg cutting).
- Tip kapısı iki tip için (mevcut `TypeStamp`/`MinGatedSince`): `TypeStamp s13[2] = { {1, true, 900}, {3, true, 400} };` ⇒ `MinGatedSince(s13, 2, since)` `true` ve `since == 400`; `TypeStamp s14[2] = { {1, false, 0}, {4, true, 1500} };` ⇒ `true` ve `since == 1500`; `TypeStamp none[2] = { {1, false, 0}, {3, false, 0} };` ⇒ `false` ve `since == 0`. (Struct alan sırası `type, has, sinceMs`, `BotCombat.h:515-520`.)
- `CastMoralSupported(7) == true`, `CastTypeMoralSupported(1, 7) == true`, `CastHpCostSupported(0) == true`, `IsGatedType(1) == true`.
- Test sayısı **249** (248 + 1). İşaretli/işaretsiz karşılaştırma uyarısı (C4389) çıkarsa F4-35/F4-36'daki gibi `(int)` dönüşümü kullan.

### 5.3 `ActionExecutor.h` yorumları

Yalnızca yorum; kod yok. İki değişiklik:

1. `BeginCast` yorumunda (`:230-231`) `"... item-effect magics (MAGIC.Skill == 0) and the dual-typed Type1 + Type3/4 skills (Scream, Shock Stun) stay unsupported),"` ifadesini şuna çevir: `"... item-effect magics (MAGIC.Skill == 0) stay unsupported; the melee pairs Type1 + Type3 and Type1 + Type4 (Scream, Shock Stun, Exceed Break, leg cutting; ADR-0017 Ek F4-37) are supported, every other type pair stays unsupported),"` (parantez/virgül yapısını koru).
2. `TickCast` yorumunda (`:244-245`) `// dual-typed: the EFFECTING echo comes from the Type4 part (code = duration), "missed" is never reported,` ve devamı satırından hemen sonra şu satırları ekle: `// Type1 + Type3 / Type1 + Type4 (ADR-0017 Ek F4-37, docs/03 MEC-MAG-24): the server broadcasts twice (the Type1 hit, then the Type3 / Type4 part);` ve `// the LAST packet is the echo: {1, 4} reports the Type4 duration in "code" (never "missed"), {1, 3} keeps the Type1 code (0 = "effected", -104 = "missed").`

**`ActionExecutor.cpp` değişmez** (K5). `BeginCast`'in mevcut kuralları (sınıf/ağaç, `quest_locked`, `no_item`, `bad_target`, MP/menzil/tip kapısı guard'ları) bu çiftler için olduğu gibi çalışır.

### 5.4 Sonuç sözleşmesi (`docs/03` MEC-MAG-24 ile aynı)

| Skill / durum | Sunucu | Bot sonucu |
|---|---|---|
| **Shock Stun** `106820`, `BotWG_K` → düşman bot (silah menzili), canlı | `CastTime 0`: tek EFFECTING; `IsAvailable()`: iki tip ön koşulu (Type3 düşman hedefte `true`), eşya `379059000` ×2 ✔, ağaç `[8] = 20 >= 20` ✔, MP **−250 bir kez**; `ExecuteType1` hasar + hedef kimlikli yayın (`sData[3]` 0 / −104); `ExecuteType3` ikinci yayın (aynı `sData[3]`); `ConsumeItem()` **1 Stone of Warrior** | `effected` (`op 3`, `code 0`) ya da `missed` (`code −104`: Type1 hasarı 0); MP −250; hedef HP düşer (`DAMAGE`); iki tip damgası (1 ve 3) |
| **Exceed Break** `106815` | aynı, `Msp 400`, Type3 `DirectType 13` dayanıklılık | `effected`/`missed`; MP −400 |
| **Scream** `106802` | `IsAvailable()`: eşya `379063000` ✔ ve `BeforeAction 1` ⇒ `379059000` ✔, MP **−300 bir kez** (`{1, 4}`'te `bType[0] != 4`; `ExecuteType4:1800` MP düşmez); Type1 hasarı + yayın, sonra Type4 debuff + yayın `{…, süre 7, hız 1, …}`; `ConsumeItem()` **1 Stone of Warrior**, Scream Scroll tüketilmez | `effected`, `code` = **7** (Type4 süresi; son yankı Type4'ten); `missed` raporlanmaz; MP −300 |
| **Leg cutting** `106520` | eşyasız; MP −84; Type4 `BuffType 6`, `Duration 10` | `effected`, `code` = **10**; MP −84 |
| Aynı `BuffType` (hız) hedefte zaten varken Scream / leg cutting | `BuffType 6` bir **debuff** (`isDebuff`): `CheckType4Prerequisites()` engellemez; mevcut debuff silinip yenisi eklenir (süre sıfırlanır, `:1756-1760`) | `effected`, `code` süre (MEC-MAG-15'in buff reddi burada uygulanmaz); direnç zarı başarısızsa yine yayın |
| Hedef Type1 sırasında ölü / ölürse | `ExecuteType1` `false` ⇒ `ExecuteSkill(bType[1])` çalışmaz; yalnızca Type1 yayını | `missed` (`code −104` hasarsız) ya da `effected`; Type3/4 yayını yok |
| `Etc != 0` quest (`106575` Blooding, `110572` Fire Staff) | — | `REFUSED` `quest_locked` (`db/003` uygulanmadıysa); uygulanmışsa ağaç (`[5] = 70 < 75`) ⇒ `srv_fail` |
| Ağaç yetersiz (`110742`, `BotMF_K` `[7] = 0`; `BotMI_K` `110542`: `[5] = 52 ≥ 42` ✔ geçer) | `IsAvailable()` `fail_return` | `srv_fail` (`op 4`), MP düşmez |
| Başka sınıf skill'i (`105520`, `108675`, `208675` K botuyla) | — | `REFUSED` `bad_skill` (sınıf) |
| `self` hedef (`cast BotWG_K 106820 self 1`) | — | `REFUSED` `bad_target` (`Moral 7`) |
| Mage asa skill'i `110542`, `BotMF_K` → düşman bot (`< 11 m`) | `CastTime 0`; MP −100; Type1 hasarı + Type3 ateş hasarı; `Range 11`: `GetDistanceSqrt >= 11` ise Type3 hedefi atlanır (`:1337`) | `effected`/`missed`; mesafe `>= 11 m` ise bot guard'ı `FAIRNESS_REJECT` `out_of_range` (MEC-MAG-11) |
| Kapalı çiftler (`{1, 9}`, `{2, x}`, `{0, x}`...) | — | `REFUSED` `unsupported_skill` (birim testiyle sabit; bot sınıflarında oyun içi örneği yok) |
| `ENABLED=0` / tek tipli / `{3, 4}` / eşyasız skill'ler | — | davranış değişmez |

`reason` metinleri, `CastOutcome::Kind` değerleri ve `TickCast` akışı bu tablo için **değişmez**; yeni çıktı yoktur.

## 6. Kabul kriterleri

- [ ] K1: `tools/build.sh Release` hatasız biter (değişen üç dosya için uyarı çıktısı boş; bu dosyalar `touch` edilip yeniden derlenir; `ActionExecutor.h`'yi içeren `.cpp`'ler de yeniden derlenir).
- [ ] K2: `tools/build.sh Debug` hatasız biter.
- [ ] K3: `tools/run-tests.sh Release` çıkış kodu 0; çıktı `Combat_CastTypes_Supported`, `Combat_ItemSkill_Guard` ve `Combat_Type1Pair_Guard` adlarını `[ OK ]` ile içerir ve toplam test sayısı **249**; `Debug` aynı.
- [ ] K4: `grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h` eşleşme vermez; `#include` yalnızca `<algorithm>` ve `<cstdint>` (mevcut); `git diff gece/2026-10-02...bot/F4-37 -- BotCore/BotCombat.h | grep '^+' | grep "std::min\|std::max"` boş; `git diff ... -- BotCore/BotCombat.h` yalnızca `CastTypesSupported` ve üstündeki yorum satırlarını değiştirir (tek hunk; `CastTypeMoralSupported`, `CastMoralSupported`, `CastItemSkillSupported`, `CastConsumeItem` gövdeleri aynı).
- [ ] K5: `git diff gece/2026-10-02...bot/F4-37 --stat` **tam üç dosya** gösterir (`BotCore/BotCombat.h`, `Tests/BotCoreTests/CombatTests.cpp`, `GameServer/Bot/ActionExecutor.h`) artı plan dosyası; `git diff ... -- GameServer/Bot/ActionExecutor.cpp` **boş**; `grep -c "CastTypesSupported(m->bType\[0\], m->bType\[1\])" GameServer/Bot/ActionExecutor.cpp` tam 1; `grep -n "{1, 3}\|(1, 3)\|(1, 4)" Tests/BotCoreTests/CombatTests.cpp` içinde `false` bekleyen `CastTypesSupported(1, 3|4)` satırı kalmamıştır (`(1, 9)` hariç).
- [ ] K6: `Combat_Type1Pair_Guard` yukarıdaki §5.2 b listesini içerir: `grep -c "CastTypesSupported" Tests/BotCoreTests/CombatTests.cpp` en az eski sayıdan 30 fazla; kapalı çift satırları (`(1, 9)`, `(0, 4)`, `(4, 3)`, `(5, 4)`) `false` ile mevcuttur.
- [ ] K7: `ENABLED=0` davranışı değişmez: yeni ini anahtarı, komut, thread, telemetri olayı türü ve alanı yok (`git diff ... | grep '^+' | grep 'Emit('` boş).
- [ ] K8: satır sonu/BOM durumu bozulmadı (`file` çıktısı değişiklik öncesiyle aynı); üç dosya CRLF kalır (ASCII); `git diff --check` boş.
- [ ] K9: `grep -c "CheckMoveStep" GameServer/Bot/ActionExecutor.cpp` ≥ 2, `grep -c "CheckAttack"` ≥ 1, `grep -c "CheckCastStart"` ≥ 1, `grep -c "CheckCastEffect"` ≥ 1, `grep -c "CheckCastFly"` ≥ 1, `grep -c "CheckCastLand"` ≥ 1, `grep -c "CheckCastCancel"` ≥ 1, `grep -c "CheckPotion"` ≥ 1; önceki 248 testin tamamı (yalnızca iki satırı `true`'ya çevrilenler dahil) geçiyor.
- [ ] K10: `python3 tools/check-perception-contract.py` `RESULT: PASS` (`R1`..`R5` ihlal sayıları 0; denetlenen dosya sayısı değişmez; araç ve istisna listeleri değişmez).
- [ ] K11 (Claude, `/plan-dogrula` çalışma zamanı): bkz. §7 senaryoları S1–S8 geçer.

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status        # [UP] varsa: ./tools/run-servers.sh stop  (exe kilidi)
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release
./tools/run-tests.sh Debug
git diff --stat gece/2026-10-02...bot/F4-37
git diff gece/2026-10-02...bot/F4-37 -- BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h
git diff gece/2026-10-02...bot/F4-37 -- GameServer/Bot/ActionExecutor.cpp GameServer/Bot/BotSession.cpp GameServer/Bot/BotSession.h GameServer/Bot/BotManager.cpp GameServer/Bot/Telemetry.cpp GameServer/proj-GameServer.vcxproj GameServer/proj-GameServer.vcxproj.filters BotCore/BotCore.vcxproj Tests/BotCoreTests/BotCoreTests.vcxproj tools
grep -n "windows.h\|stdafx\|GameServer\|shared/" BotCore/BotCombat.h
grep -rn "CastTypesSupported" BotCore GameServer Tests
grep -n "printf\|Sleep\|lock_guard\|mutex\|CreateThread\|rand(" GameServer/Bot/ActionExecutor.h
python3 tools/check-perception-contract.py
file BotCore/BotCombat.h Tests/BotCoreTests/CombatTests.cpp GameServer/Bot/ActionExecutor.h
git diff --check gece/2026-10-02...bot/F4-37
```

**Çalışma zamanı doğrulaması (DeepSeek yapmaz, Claude `/plan-dogrula`'da yapar):** `GameServer.ini` `[BOT]` ile `ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, `SPAWN_ON_START` boş; ini her senaryodan sonra yedekten geri yüklenir; eski `Logs/bots/` test dosyaları önceden temizlenir; kapanış `CTRL_BREAK` ile (KI-010); iş bitince `./tools/run-servers.sh stop`. Komutlar `BotCommands.txt` ile verilir (önce geçici ada yaz, sonra `mv`; komut başına ≥ 1 sn; dosya saniyede bir okunduğu için "~500 ms" iptal elle yakalanamaz). Botlar (hepsi zone 71; doğuşlar ≥ 3 sn arayla, KI-DEG-01): Karus warrior **`BotWG_K`** (sword+shield) ve **`BotWP_K`**, Karus mage **`BotMF_K`** / **`BotMI_K`**, El Morad hedefleri **`BotWP_E`/`BotWG_E`** (düşman; silah menzili için hedef bota `move` ile ≤ **1 m** yaklaş: F4-36 Judgment ölçümünde silah menzili alanı ≤ 1 m idi; mage asa skill'i için ≤ 10 m). Önce `list` ile konum, HP/MP ve ölü durumunu denetle: **ölü bot başlangıç ölçümüne alınmaz** (`db/002` idempotent yeniden uygulanır ya da bot `regene` ile diriltilir). MP/HP/konum `list`'ten, olaylar `Logs/bots/<tarih>/live-*.jsonl` ve `Logs/Bot_*.log`'tan; MP'yi cast'ten hemen önce ve sonra `list` ile al, kümeli +40 yenileme payını raporla. **Taş tüketimi:** `USERDATA` satırları okunmaz (`CLAUDE.md` DB kuralı); Stone of Warrior tüketimi koddan `[D]` kalır (F4-36 notu); bu ölçüm taşları azaltır (50 → ~45): rapora yaz. `ReCastTime` süreleri: Scream 10,1 sn, Shock Stun 25,2 sn, Exceed Break 25,4 sn, leg cutting 5,1 sn.

1. **S1 Shock Stun (`106820`, `{1, 3}` + Stone of Warrior):** `spawn BotWG_K` ve `BotWP_E` (≥ 3 sn arayla), `BotWG_K`'yı hedefin ≤ 1 m'sine getir; MP'yi `list` ile al; `cast BotWG_K 106820 BotWP_E 1` ⇒ `ACTION_SUBMIT` `CastEffect` (`casting` aşaması yok), `ACTION_RESULT` `effected` (`op 3`, `code 0`) ya da `missed` (`code −104`); log `cast finished (effected) after 1 cycle(s)`; MP **−250**; hedef HP düşer (`DAMAGE`). Hedefin `/bot snap BotWP_E events` çıktısında aynı skill için **iki** EFFECTING olayı (Type1 yayını + Type3 yayını) görünmeli; görünmüyorsa raporla (olay halkası boyutu/süre). `since_casting_ms` alanı yok.
2. **S2 Exceed Break (`106815`, `{1, 3}`):** aynı koşullar; ReCastTime farklı skill olduğundan S1'den ≥ 1 sn sonra (same-type kapısı: Type1 damgası; bot ≥ 1000 ms bekler) `cast BotWG_K 106815 BotWP_E 1` ⇒ `effected`/`missed`, MP **−400**. Kapıyı doğrula: S1'in `ACTION_RESULT`'ı ile S2'nin `ACTION_SUBMIT`'i arası ≥ 1000 ms (tip kapısı iki tip için, MEC-MAG-03/13).
3. **S3 Scream (`106802`, `{1, 4}` + `BeforeAction 1`):** `cast BotWG_K 106802 BotWP_E 1` ⇒ `effected`, `code` **7** (Type4 süresi; farklıysa raporla), MP **−300** (çift ödeme yok: `list` farkı 300, 600 değil); `missed` görülmez. Ardından ReCastTime (10,1 sn) bitince aynı hedefe ikinci Scream (aynı `BuffType 6` debuff'ı hedefte): `effected` (debuff yenilenir, `srv_fail` **değil**); sonuç farklıysa MEC-MAG-24'ü düzelt. `ENABLED` açıkken Scream Scroll tüketilmediği: çantada 1 adet `379063000` ile ikinci atışın `no_item` olmaması kanıtlar.
4. **S4 Leg cutting (`106520`, `{1, 4}`, eşyasız):** `cast BotWG_K 106520 BotWP_E 1` ⇒ `effected`, `code` **10**, MP **−84**. `BotWP_K` ile de bir atış (`[5] = 70 ≥ 20`).
5. **S5 Mage asa skill'i (`110542`, `{1, 3}`, `Range 11`):** `spawn BotMF_K` ve `BotWP_E`; hedef ≤ 10 m: `cast BotMF_K 110542 BotWP_E 1` ⇒ `effected`/`missed`, MP **−100**, hedef HP düşer; mesafe ≥ 11 m iken (ör. 15 m) `FAIRNESS_REJECT` `out_of_range`, paket gitmez. `cast BotMF_K 110742 BotWP_E 1` (`[7] = 0`) ⇒ `srv_fail`, MP değişmez. (İsteğe bağlı: `BotMI_K` ile `110642`.)
6. **S6 Bot kuralları:** `cast BotWG_K 106820 self 1` ⇒ `refused (bad_target)`; `cast BotWP_K 106575 BotWP_E 1` (Blooding, `Etc 510`) ⇒ `refused (quest_locked)` (`db/003` uygulanmadıysa; uygulanmışsa `srv_fail` ve ağaç nedeniyle) — hangisi olduğunu raporla; `cast BotWG_K 105520 BotWP_E 1` (novice sınıfı) ⇒ `refused (bad_skill)`; hepsinde paket gitmez (`ACTION_SUBMIT` 0), MP değişmez.
7. **S7 Gerilemesiz:** F4-36 `cast BotPHD_K 112802 BotWP_E 1` (Judgment, tek tipli Type1 + eşya) `effected`; F4-26 `{3, 4}` `cast BotMI_K 110657 BotWP_E 1` ya da Ice comet; F4-35 `cast BotWG_K 106650 BotWP_K 1` (descent) davranışı; F4-28 Type4 `cast BotPHB_K 112603 self 1`; tek tipli Type1 `cast BotWG_K 106525 BotWP_E 1` (Carving, `Msp 90`; ya da `106500` Hash `Msp 10`) `effected`/`missed`, MP düşüşü `Msp` kadar. Eşyasız tek tipli skill'lerde davranış aynı.
8. **S8 Ayarlar ve temizlik:** `TELEMETRY=summary`: bir Shock Stun atışı çalışır, JSONL'de `ACTION_*` yok; `ENABLED=0` ⇒ komut/log/dosya yok; `PERF_SAMPLE` `tick_p95_us` önceki düzeyde; sunucu 3/3 UP, `GameServer.log`'a yeni hata yok. Temizlik: ini yedekten geri, geçici betikler silindi, botlar despawn, sunucular `stop`.

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman). `BotCore/` ve `Bot/` ASCII; kod yorumları İngilizce. Mevcut dosyaların kodlama/BOM durumu korunur (`file` ile önce/sonra karşılaştır).
- **Bot sistemi varsayılan kapalı** (`docs/13` §1, ADR-0015). Yeni kod yalnızca `PHASE_IN_GAME` oturumlarında ve **`{1, 3}`/`{1, 4}` çiftli** skill'i serisinde davranış değiştirir (önceden `unsupported_skill` idi); tek tipli ve `{3, 4}` skill'lerde `BeginCast`/`TickCast` davranışı (paket biçimi, telemetri satırı, `reason` değerleri) değişmez.
- **Thread kuralı (ADR-0005):** değişen kod saf mantıktır (durumsuz); `BeginCast`/`TickCast` bot tick'inde çalışır, yeni durum/kilit yok.
- **Sonuç yalnızca yayınlanan yanıttan** (AC-LRN-03 / `docs/13` §8): `effected` yalnızca son `MAGIC_EFFECTING`'ten, `srv_fail` yalnızca `MAGIC_FAIL`'den. Hedefin stun/yavaşlama durumu, debuff'ın tutup tutmadığı, direnç zarı sonucu, Type3 kısmının uygulanıp uygulanmadığı **bota önkontrol veya sonuç olarak eklenmez**.
- **Bilinen sınırlar `[A]`/`[D]`:**
  - (a) **Çift yankı:** sunucu Type1 yayınından sonra Type3/Type4 yayını da gönderir; bot yalnızca **son** paketi görür (`m_castEcho` her paketle ezilir). `{1, 4}` sonucu Type4'ten (`code` = süre), `{1, 3}` sonucu Type1'in `sData[3]`'ünü taşır (`0` ya da `−104` ⇒ `missed` mümkündür, `{3, 4}`'ten farklı). Type3/Type4 kısmının hedefte uygulandığı yayından **ayırt edilemez**.
  - (b) Hedef Type1'de ölürse Type3/4 hiç çalışmaz (yalnızca Type1 yayını). Hedef ölü doğmuşsa `ExecuteType1` hasarsız yayın yapar ve `false` döner.
  - (c) Stone of Warrior her Scream/Shock Stun/Exceed Break atışında 1 azalır (50'lik yığın); bitince `no_item`. Yeniden stoklama envanter doldurma diliminin işidir (m.8); `docs/06` satır 154 (devre dışı bırakma) F6'nın işidir.
  - (d) Çift silah / iki elli silah doğrulaması (`sSkill 1055/2055/1056/2056`, `MagicInstance.cpp:968-995`) yalnızca 105/205 sınıfı skill'lerine uygulanır; 106/206 botları için geçerli değildir. Gelecekte 105/205 sınıfı bot eklenirse bu doğrulama yayınsız `false` verir (`no_result`).
  - (e) Aynı `BuffType`'ın hedefte bulunması Scream/leg cutting'i reddettirmez (debuff yenilenir); bu buff'lar için MEC-MAG-15 kuralı geçerlidir. Blinding'in `BuffType`'ı ölçülmedi.
  - (f) Mage asa skill'lerinin (`Range 11/22`) Type3 kısmı `GetDistanceSqrt >= Range` ise hedefi atlar; bot guard'ı `mesafe < Range` ister (aynı kural), bu yüzden sınırda tutarsızlık beklenmez.
  - (g) Blinding, Blooding, Fire/Ice/Light Staff veri gereği açılır ama çoğu ölçülmez (quest/ağaç/sınıf); ölçüldüğünde ilgili dilim notuna eklenir.
- Telemetri hacmi değişmez; `tools/bot-telemetry-report.py` değişmez (`REFUSED` sonuçları ayrı sayılır, MET-ACT-02).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

(henüz doldurulmadı)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz doldurulmadı)
