# F4-53: `Perception` dilim 11 — gözlenen durum tablosu (saf mantık): skill meta değeri, `ObservedStatusTable`, `HealObsRing`

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2, m.10 algı eksikleri; F6/F7 priest ve stall için ön koşul) |
| Branch | `bot/F4-53 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F4-52 (olay halkası, `SkillEvent`) `KAPANDI`; F4-28/F4-31/F4-32 (Type4/party/cure atılabilir, doğrulama için) `KAPANDI` |
| İlgili gereksinim / kabul | `docs/03` §16 (düşman buff/debuff'ı görülen olaylardan takip edilir), MEC-BUF-01..07, MEC-MAG-15, MEC-MAG-19, MEC-MAG-24, MEC-DTH-01, yeni MEC-BUF-10; `docs/09` §6.1 (`heal_rate`), `docs/13` §5.2a (`E` sınıfı: tahmin); DEG-08 (`docs/reports/degerlendirme-2026-10-02.md`) |
| Tahmini büyüklük | S (2 kod dosyası, yalnızca ekleme, sunucu kodu yok) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu; TASLAK'tan HAZIR'a, kapsam daraltıldı) |

---

## 1. Amaç

F4-52 başkalarının `WIZ_MAGIC_PROCESS` yayınlarını ham olay olarak saklar. Karar katmanı ise "hedefte Malice var mı, ne zaman biter", "bu dost kök/yavaşlatma altında mı", "hedefe son 5 sn'de nominal kaç HP heal geldi" sorularına cevap vermelidir. Bu plan, bu soruları cevaplayan **saf mantığı** (`BotCore`, sunucusuz, birim testli) ekler: skill verisinin değer kopyası `SkillMeta`, görülen Type4 olaylarından beslenen `ObservedStatusTable` ve görülen heal olaylarından beslenen `HealObsRing`. Tüm kayıtlar **`E` sınıfıdır** (tahmin; sunucu başkalarının buff listesini göndermez, `docs/13` §5.2a). Sunucuya bağlama (skill tablolarından `SkillMeta` kurma, `BotSession` beslemesi, `WIZ_DEAD` ile temizleme, `/bot snap status`, `UnitView` alanları ve R5 güncellemesi, çalışma zamanı doğrulaması) **bu planda yoktur**: ayrı sıradaki plan (F4-60, aşağıda §3 "Sıradaki"). Bu plan bittiğinde sunucu davranışı ve sunucu ikilisi bit düzeyinde aynıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/03` §16 (`:537-560`, satır `:548`): düşman üzerindeki buff/debuff'lar görülen olaylardan takip edilir `[D]`; düşmanın MP'si/cooldown'ı gönderilmez. `docs/13` §5.2a (`:169`): `E` sınıfı = türetilmiş gözlem, bitiş zamanı örnek olarak sayılır.
- `docs/03` MEC-BUF-01..07 (`:192-198`): her `BuffType` için tek kayıt; aynı `BuffType`'lı yeni **buff** hedefte reddedilir (güçlü zayıfı ezmez), yeni **debuff** eski kaydı (buff olsa bile) silip yerine yazılır ve süresi yenilenir; süre `UNIXTIME + sDuration`, `Update()` çağrısı başına en çok bir kayıt kaldırılır (gerçek kaldırma tahminden geç olabilir). MEC-DTH-01 (`:295`): ölümde tüm DoT/HoT ve buff/debuff'lar temizlenir.
- `GameServer/MagicInstance.cpp:1618-1896` `ExecuteType4` (Type4 yayını): hedef başına `BuildAndSendSkillPacket(..., sCasterID, pTarget->GetID(), bOpcode, nSkillID, sDataCopy)` (`:1861-1874`), `sDataCopy = { sData[0], bResult, sData[2], sDuration, sData[4], bSpeed, sData[6] }` ⇒ **`data[1]` = `bResult`** (1 uygulandı, 0 reddedildi), **`data[3]` = süre (sn)**; yayın yalnızca `bType[1] == 0 || bType[1] == 4` iken (`:1862`). `target` gerçek hedeftir: yansıtılan curse'te çağıran (`:1727-1735`), Krowaz tipi skill'lerde çağıranın kendisi (`:1709-1721`). Buff aynı `BuffType`'ta reddedilirse tek hedefte `bResult = 0` (+ yalnızca çağırana `MAGIC_FAIL`, `:1776-1782`, `:1891-1893`), alan/grup yolunda kurban sessizce atlanır (yayın yok, `:1787`). Debuff aynı `BuffType`'ta eskisini siler (`:1756-1760`). Hız/stun debuff'ında direnç zarı başarısız olsa da kayıt haritada kalır ve yayın `bResult = 1` ile gider (`:1819-1848`, MEC-BUF-05). `DECREASE_RESIST`/`DISABLE_TARGETING` için ek bir `SendSkill()` yayını daha vardır (`:1886-1888`, `data[]` çağıranın gönderdiği ham değerlerdir; bot çağıranıysa `data[3] = 0`).
- `GameServer/MagicInstance.cpp:1109-1112` `ExecuteType1`: Type1 yayınında `sData[3] = 0` (isabet) ya da `-104` (`SKILLMAGIC_FAIL_ATTACKZERO`) yazılır ⇒ `{1, 4}` çiftli skill'de (Scream, leg cutting; MEC-MAG-24) iki yayın gelir ve **`data[3] > 0` yalnızca Type4 yayınını ayırır**.
- `GameServer/MagicInstance.cpp:1263-1616` `ExecuteType3` (heal yayını): yayın `bType[1] == 0 || bType[1] == 3` iken hedef başına gider (`:1598-1599`); anlık yolda `damage = sFirstDamage` (`:1347`, `DirectType 1` ⇒ `HpChangeMagic`, `:1391-1401`), süreli yolda (`bDuration != 0`) başlangıç `sFirstDamage` (`:1527-1528`) ve HoT toplamı `sTimeDamage` (`:1537-1540`, tick başına `sTimeDamage / (bDuration / 2)`, `:1565-1571`). `sData[1]` yalnızca yıldırım-stun başarısızlığında 0 yapılır (`:1593-1594`); heal'de `data[1]`'e **güvenilmez** (insan istemcisinin gönderdiği değer bilinmiyor).
- `GameServer/MagicInstance.cpp:1898` `ExecuteType5` ve `GameServer/MagicInstance.h:55-57` (`REMOVE_TYPE3 1`, `REMOVE_TYPE4 2`, `RESURRECTION 3`); MEC-MAG-19: `REMOVE_TYPE4` yalnızca `isDebuff()` Type4 kayıtlarını kaldırır ve hedef kimlikli `EFFECTING` yayınlar (kaldırılacak bir şey olmasa da).
- `GameServer/MagicProcess.cpp:633-` `RemoveType4Buff`: kaldırma/süre bitişi bildirimi (`MAGIC_DURATION_EXPIRED`, `:989-991`) **yalnızca hedefe** gider (`TO_USER(pTarget)->Send`), bölgeye yayınlanmaz ⇒ başkasının buff'ının bitişi/iptali **gözlenemez**, yalnızca `olay + süre` tahmin edilir.
- `shared/database/structs.h:51-62` `_MAGIC_TYPE3` (`bDirectType`, `sFirstDamage`, `sTimeDamage`, `bDuration`), `:64-100` `_MAGIC_TYPE4` (`bBuffType`, `sDuration`, `bIsBuff`), `:102-107` `_MAGIC_TYPE5` (`bType`). `BotCore` bunları **include etmez**; bu plan yalnızca alanların değer kopyasını (`SkillMeta`) tanımlar, doldurmak sonraki planın işidir.
- Veri doğrulaması (yerel DB, yalnızca skill tabloları; 2026-10-03): `MAGIC_TYPE3` heal'leri `DirectType 1`: Great healing `112527` `FirstDamage 960`, Complete healing `112554` `10000`, Group massive `112557` `960` (`Moral 6`), Superior restore `112548` `TimeDamage 2500` / `Duration 30` (HoT), warrior restoration `106730` `TimeDamage 750` / `Duration 60`; `MAGIC_TYPE4`: Malice `112703` `{4, 0}` `BuffType 2` `Duration 150` (debuff), Insensibility peel `112660` `BuffType 2` `Duration 600`, `112654` `BuffType 1` `Duration 600`, leg cutting `106520` `{1, 4}` `BuffType 6` `Duration 10`, Ice arrow `110615` `{3, 4}` `BuffType 6` `Duration 12`; `MAGIC_TYPE5`: Cure curse `112525` `Type 2`, Cure disease `112535` `Type 1`. Test değerleri bunlardan alınmıştır (`bIsBuff` tabloda değil, yükleme sırasında hesaplanır: testte elle verilir).
- `BotCore/Perception.h:47` `kSrcEstimate = 2` (E sınıfı sabiti, yeniden kullanılır); `:1788-1924` F4-52 bloğu (`SkillEvent`, `ParseSkillEvent`, `SkillEventRing`; `kMagicEffecting = 3`); dosya `namespace BotCore` ile biter (`:1924` kapanış `}`); girinti **sekme**, satır sonu **CRLF**.
- Örnek test düzeni: `Tests/BotCoreTests/PerceptionTests.cpp` (`TEST_CASE("Perception_SkillRing_Basic")` ve yanındakiler, dosya sonunda; `CHECK`, `CHECK_EQ` makroları `MiniTest.h`'den).
- Sözleşme denetimi: `tools/check-perception-contract.py` (R1..R5; yeni satırlar sunucu başlığı/nesnesi içermez, `UnitView`/`NpcView`/`TeamMemberView` alanları **değişmez** ⇒ R5'e dokunulmaz).

## 3. Kapsam

**Yapılacaklar** (`BotCore/Perception.h` dosya sonuna, `namespace BotCore` kapanışından önce; tek bölüm başlığı `// --- observed status and heal observations (ADR-0017 Ek F4-53) ---`)

1. **Sabitler:** `kType5RemoveType3 = 1`, `kType5RemoveType4 = 2` (kaynak yorumu: `MagicInstance.h` `REMOVE_TYPE3/4`); `kObsStatusUnits = 32` (kayıt tutulan birim sayısı, tasarım sınırı), `kObsStatusPerUnit = 8` (birim başına Type4 kaydı), `kHealObsRing = 64`.
2. **`struct SkillMeta`** (POD, sıfırlanabilir; `BotCore` tablo başlığı almaz): `uint32_t skillId; uint8_t type1, type2;` (`MAGIC.bType[0..1]`) `uint8_t buffType; bool isBuff;` (`MAGIC_TYPE4.bBuffType`, `bIsBuff`; `type1 == 4 || type2 == 4` iken anlamlı) `uint8_t directType; int16_t firstDamage, timeDamage; uint8_t type3DurationSec;` (`MAGIC_TYPE3.bDirectType`, `sFirstDamage`, `sTimeDamage`, `bDuration`; Type3 kısmı varken anlamlı) `uint8_t type5Kind;` (`MAGIC_TYPE5.bType`; `type1 == 5` iken).
3. **Sınıflandırma yardımcıları** (inline, saf):
   - `SkillSendsType4(m)` = `(m.type1 == 4 && m.type2 == 0) || m.type2 == 4` (sunucunun Type4 yayın koşulu, `:1862`; `{4, 3}` Type4 yayınlamaz).
   - `SkillSendsType3(m)` = `(m.type1 == 3 && m.type2 == 0) || m.type2 == 3` (`:1598`; `{3, 4}` Type3 yayınlamaz, yalnızca Type4 yayınlar: MEC-MAG-13).
   - `SkillIsCureDebuff(m)` = `m.type1 == 5 && m.type5Kind == kType5RemoveType4`.
   - `SkillHealNominal(m, bool & hot)`: `hot = false`, dönüş 0; `SkillSendsType3(m)` ve `m.directType == 1` değilse 0; aksi halde `nominal = (firstDamage > 0 ? firstDamage : 0)`, `hot = (type3DurationSec > 0 && timeDamage > 0)`, `hot` ise `nominal += timeDamage`; dönüş `uint32_t` (etkili miktar değil, **skill'in nominal değeri**: kritik çarpanı, tavan ve kayıp HP bilinmez).
4. **`struct StatusObs`** (`uint32_t skillId; int16_t caster; uint8_t buffType; bool isBuff; uint8_t src; uint64_t startMs, endMs;`) ve `inline uint32_t StatusRemainingMs(const StatusObs & r, uint64_t nowMs)` (`endMs > nowMs` ise fark, değilse 0; `0xFFFFFFFF` ile sınırlı).
5. **`enum StatusUpdate { kStatusIgnored = 0, kStatusRecorded = 1, kStatusCured = 2 }`** ve **kopyalanabilir `class ObservedStatusTable`** (sabit boyut, ayırma yok, kilit yok: çağıran kilitler; `HpTable`/`SkillEventRing` düzeni):
   - `ObservedStatusTable()` → `Clear()`; `void Clear()`.
   - `StatusUpdate Observe(const SkillEvent & ev, const SkillMeta * meta)` — kurallar (sırayla):
     1. `meta == nullptr`, `ev.op != kMagicEffecting` ya da `ev.target < 0` ⇒ `kStatusIgnored` (CASTING/FLYING ve alan skill'inin hedefsiz `-1` özet olayı kayıt üretmez).
     2. `SkillSendsType4(*meta)`: `ev.data[1] == 0` (reddedildi) ya da `ev.data[3] <= 0` (süre yok; `{1, 4}` skill'in Type1 yayını `0`/`-104` taşır) ⇒ `kStatusIgnored`; aksi halde kayıt: `skillId = ev.skillId`, `caster = ev.caster`, `buffType = meta->buffType`, `isBuff = meta->isBuff`, `src = kSrcEstimate`, `startMs = ev.tMs`, `endMs = ev.tMs + (uint64_t)ev.data[3] * 1000` ⇒ `kStatusRecorded`. **Süre paketten (`data[3]`) alınır, tablodan değil** (scroll buff'ının kayıtlı süresi tablodan farklıdır, `:1791-1797`). Aynı `(target, buffType)` kaydı varsa (buff ya da debuff, MEC-BUF-02/03: debuff eskiyi siler; başarılı bir buff olayı eski kaydın gözlenmeden kalktığını gösterir) **yerinde** üzerine yazılır; yoksa birimin ilk boş yuvasına eklenir; birim doluysa `endMs`'i en küçük kayıt (eşitse düşük indeks; süresi dolmuş kayıt doğal olarak önce gider) ezilir.
     3. Birim tablosu doluysa (32 birim) ve hedef birim yoksa `lastMs`'i (birimin son kayıt/güncelleme zamanı, `max` ile) en küçük birim (eşitse düşük indeks) tümüyle atılır.
     4. `SkillIsCureDebuff(*meta)` (kural 1'i geçmişse; `data[1]`'e bakılmaz, cure yayını koşulsuzdur: MEC-MAG-19): hedefin `isBuff == false` kayıtları silinir (buff'lar kalır; kalan kayıtlar sıralı kayar) ⇒ `kStatusCured` (silinecek kayıt olmasa da). `REMOVE_TYPE3` (`type5Kind == 1`) ve diğer Type5 türleri `kStatusIgnored` (DoT/HoT kaydı tutulmaz, bkz. kapsam dışı).
   - `void ClearTarget(int16_t target)` (ölüm: MEC-DTH-01; birimin tüm kayıtları ve birim yuvası boşalır), `void Prune(uint64_t nowMs)` (`endMs <= nowMs` kayıtlar silinir, boş birimler boşalır).
   - Sorgular (`const`; yalnızca **canlı** kayıtlar: `endMs > nowMs`): `const StatusObs * Find(int16_t target, uint8_t buffType, uint64_t nowMs)` (işaretçi sonraki değişikliğe kadar geçerli), `int Collect(int16_t target, uint64_t nowMs, StatusObs * out, int cap)` (`endMs` artan, eşitse `buffType` artan sırayla ≤ `cap` kayıt; dönüş sayı), `int CountBuffs(...)`, `int CountDebuffs(...)`, `int Units() const` (en az bir kaydı olan birim), `int Records() const` (süresi dolanlar dahil saklanan kayıt).
6. **`struct HealObs`** (`uint64_t tMs; uint32_t skillId; int16_t caster, target; uint32_t nominal; bool hot;`) ve **kopyalanabilir `class HealObsRing`** (`kHealObsRing`; `SkillEventRing` düzeni: `Clear`, `Count`, `Total`, `At(i)` 0 = en yeni): `bool Observe(const SkillEvent & ev, const SkillMeta * meta)` — `meta != nullptr`, `ev.op == kMagicEffecting`, `ev.target >= 0` ve `SkillHealNominal(*meta, hot) > 0` ise ekler ⇒ `true` (**`data[1]`'e bakılmaz**, `:1593-1594`); `uint32_t SumNominal(int16_t target, uint64_t nowMs, uint32_t windowMs) const` ve `int CountIn(int16_t target, uint64_t nowMs, uint32_t windowMs) const` (`target == kSkillIdAny` joker; pencere = `ev.tMs <= nowMs && nowMs - ev.tMs <= windowMs`, `SkillEventRing::WithinWindow` ile aynı).
7. **Birim testleri** (§6 K3): `PerceptionTests.cpp` sonuna 7 yeni `TEST_CASE`.

**Kapsam dışı (yapılmayacak)**

- **Sunucuya bağlama** (sıradaki plan F4-60, bu planda **yok**): `GameServer/Bot/` altında `SkillMeta` kurucusu (`m_MagictableArray` + `m_Magictype3/4/5Array` okuması, dosya-statik tek yer), `BotSession`'a `ObservedStatusTable`/`HealObsRing` üyeleri ve `OnPacket()` beslemesi (`m_skillEvents` ile aynı kilit/yer), `WIZ_DEAD`/birim çıkışında `ClearTarget`, `/bot snap <bot> status` dökümü, `UnitView.statusCount` ve benzeri alanlar (bunlar `check-perception-contract.py` R5'e takılır: `buff`/`skill` sözcükleri; R5 güncellemesi orada), çalışma zamanı doğrulaması (`ACTION_RESULT`/`list` ile çapraz).
- Type3 DoT/HoT **kayıtları** (süreli etki kaydı) ve `REMOVE_TYPE3` (Cure disease) etkisi: yalnızca heal olayının nominal değeri tutulur; DoT/HoT hedef kaydı ayrı bir dilimdir (gerekirse F4-6x).
- `MAGIC_DURATION_EXPIRED`/iptal/`MAGIC_TYPE4_EXTEND` paketlerinin işlenmesi (23 bayttan kısa, `ParseSkillEvent` reddeder; başkasının bitişi zaten gözlenemez); bitiş tahmininin hata payı ölçümü (sonraki plan, çalışma zamanı).
- Karar/telemetri (`decisions` olayları), takım içi paylaşım (`P` sınıfı), hasar/etki ölçümü, stun/yavaşlatmanın hareket cezası (MEC-BUF-06; F5/F6).
- `UnitView`, `NpcView`, `TeamMemberView`, `PerceptionSnapshot`, `BuildSnapshot`, `AttachHp` ve F4-52'nin `SkillEventRing`'i **değişmez**; `tools/check-perception-contract.py` değişmez; `GameServer/`, `shared/`, `AIServer/` değişmez.
- Mevcut hiçbir satırın silinmesi/yeniden biçimlendirilmesi; "iyileştirme" amaçlı yeniden düzenleme.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/Perception.h` | değiştir | yalnızca dosya sonuna ekleme (namespace kapanışından önce) |
| `Tests/BotCoreTests/PerceptionTests.cpp` | değiştir | yalnızca dosya sonuna 7 `TEST_CASE` (+ gerekirse `namespace { ... }` içinde küçük yardımcılar: yeni anonim blok açılabilir) |

Yeni dosya yok ⇒ `.vcxproj`/`.filters` değişmez. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F4-53 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; açık sunucuları durdur (`tools/run-servers.sh stop`). Plan başındaki test sayısını not et: `./tools/run-tests.sh Release 2>&1 | tail -3` (beklenen `252 tests, 0 failed`).
2. `Perception.h` sonuna §3.1-§3.6'yı ekle. İskelet (imzalar bağlayıcıdır, gövdeler sende):

```cpp
	constexpr uint8_t kType5RemoveType3 = 1;   // MagicInstance.h REMOVE_TYPE3
	constexpr uint8_t kType5RemoveType4 = 2;   // MagicInstance.h REMOVE_TYPE4
	constexpr int kObsStatusUnits   = 32;
	constexpr int kObsStatusPerUnit = 8;
	constexpr int kHealObsRing      = 64;

	struct SkillMeta { /* §3.2 */ };
	inline bool SkillSendsType4(const SkillMeta & m);
	inline bool SkillSendsType3(const SkillMeta & m);
	inline bool SkillIsCureDebuff(const SkillMeta & m);
	inline uint32_t SkillHealNominal(const SkillMeta & m, bool & hot);

	struct StatusObs { /* §3.4 */ };
	inline uint32_t StatusRemainingMs(const StatusObs & r, uint64_t nowMs);
	enum StatusUpdate { kStatusIgnored = 0, kStatusRecorded = 1, kStatusCured = 2 };
	class ObservedStatusTable { /* §3.5 */ };
	struct HealObs { /* §3.6 */ };
	class HealObsRing { /* §3.6 */ };
```

3. Testleri yaz (adlar bağlayıcı). Yardımcılar: `MakeEvent(op, skillId, caster, target, tMs, d1, d3)` (tüm `data[]` sıfır, yalnız `data[1]`/`data[3]` verilir) ve elle doldurulmuş `SkillMeta` kurucuları (örnek değerler gerçek skill'lerden esinlidir; tablo okunmaz). BuffType örnekleri: AC `2`, HP/MP `1`, hız `6`.
   1. `Perception_SkillMeta_Classify`: Malice benzeri `{112703, 4, 0, buffType 2, isBuff false}` ⇒ `SkillSendsType4` true, `SkillSendsType3` false; Great healing benzeri `{112527, 3, 0, directType 1, firstDamage 960}` ⇒ `SkillSendsType3` true, `SkillHealNominal` 960 ve `hot` false; Superior restore benzeri `{112548, 3, 0, directType 1, firstDamage 0, timeDamage 2500, duration 30}` ⇒ 2500 ve `hot` true; başlangıçlı HoT (`firstDamage 100`, `timeDamage 200`, `duration 10`) ⇒ 300; hasar büyüsü (`firstDamage -300`) ⇒ 0; `directType 2` (MP) ⇒ 0; `timeDamage > 0` ama `duration 0` ⇒ yalnızca `firstDamage`; çift tip `{3, 4}` ⇒ `SkillSendsType4` true, `SkillSendsType3` false ve `SkillHealNominal` 0; `{1, 4}` ⇒ Type4 true; `{4, 3}` ⇒ Type4 false, Type3 true; Cure `{5, type5Kind 2}` ⇒ `SkillIsCureDebuff` true, `type5Kind 1` ve `3` ⇒ false.
   2. `Perception_Status_Type4`: Malice benzeri debuff, `tMs 1000`, çağıran `2984`, hedef `2985`, `data[1] = 1`, `data[3] = 150` ⇒ `kStatusRecorded`; `Find(2985, 2, 1000)` dolu (`skillId 112703`, `caster 2984`, `isBuff false`, `src == kSrcEstimate`, `startMs 1000`, `endMs 151000`); `StatusRemainingMs` `61000`'de `90000`, `151000`'de `Find == nullptr` (sınır: `endMs > nowMs`); `CountDebuffs` 1 / `CountBuffs` 0 ve süre dolunca 0; her biri **kayıt üretmemeli** (`kStatusIgnored`, `Records()` 0): `data[1] = 0`, `data[3] = 0`, `data[3] = -104`, op CASTING (1) ve FLYING (2), `target = -1`, `meta == nullptr`, Type4 olmayan meta; tablo sıfırdan.
   3. `Perception_Status_Replace`: hedef `2985`'te Insensibility peel benzeri AC buff'ı (`{112660, 4, 0, buffType 2, isBuff true}`, `data[3] = 600`, `t = 0`) ardından Malice debuff'ı (`buffType 2`, `data[3] = 150`, `t = 5000`) ⇒ tek kayıt (`Records() == 1`), `isBuff false`, `skillId 112703`, `endMs 155000`, `CountBuffs 0`; farklı `buffType` buff'ı yan yana yaşar (`{112654, 4, 0, buffType 1, isBuff true}`, `data[3] = 600`, `t = 6000` ⇒ `Collect` iki kayıt, `endMs` artan sırada: önce Malice `155000`, sonra HP buff'ı `606000`); aynı `buffType 1`'de başarılı yeni buff olayı (`t = 7000`, `data[3] = 600`; stale kayıt) kaydı yeniler (`endMs 607000`, `Records()` hâlâ 2); birim kapasitesi (taze tablo, hedef `2990`): `buffType` 1..8, hepsi `t = 0`, `data[3] = 100 + 10 * buffType` (bitiş en erken `buffType 1`), sonra `buffType 9` (`data[3] = 500`) ⇒ `Records() == 8`, `Find(2990, 1, 0) == nullptr`, `Find(2990, 9, 0) != nullptr`, `Find(2990, 2, 0) != nullptr`.
   4. `Perception_Status_CureAndDeath`: hedefte debuff (`buffType 2`) ve buff (`buffType 1`); `Type5` `kType5RemoveType4` cure olayı (`op 3`, aynı hedef, `data[1]` 0 da olsa) ⇒ `kStatusCured`, yalnızca buff kalır; hiç debuff yokken cure da `kStatusCured` döner; başka hedefe cure kaydı etkilemez; `kType5RemoveType3` cure'ü ve `type5Kind 3` ⇒ `kStatusIgnored`, kayıtlar aynı; `ClearTarget(2985)` birimi boşaltır (`Units()` düşer, `Find == nullptr`); `Clear()` hepsini siler.
   5. `Perception_Status_PairSkills`: `{1, 4}` skill (leg cutting `{106520, 1, 4, buffType 6, isBuff false}`): Type1 yayını (`data[3] = 0`, `data[1] = 1`) ve `data[3] = -104` ⇒ `kStatusIgnored`; Type4 yayını (`data[3] = 10`, `data[1] = 1`) ⇒ `kStatusRecorded`; `{3, 4}` skill (Ice arrow `{110615, 3, 4, buffType 6, isBuff false}`): Type4 yayını (`data[3] = 12`) kaydedilir, hedefi `-1` olan özet olayı yok sayılır; aynı hedefte ikisi de `buffType 6` olduğundan tek kayıt kalır ve sonuncunun `skillId`'sini taşır; `endMs` eşitliğinde `Collect` sırası `buffType` artandır (iki farklı `buffType`'lı debuff'ı aynı `endMs`'e getirerek denetle).
   6. `Perception_Status_Capacity`: 32 ayrı hedefe birer kayıt (`tMs = 100 * (i + 1)`) ⇒ `Units() == 32`; 33.cü hedef ⇒ `Units()` hâlâ 32, `lastMs`'i en küçük (ilk) hedef atılmış (`Find` yok), yenisi var; kopya bağımsız (kopyaya ekleme/`Clear` kaynağı bozmaz); `Prune`: taze tabloda hedef `2985`'e `buffType 1` (`data[3] = 10`) ve `buffType 2` (`data[3] = 60`), hedef `2986`'ya `buffType 1` (`data[3] = 10`), hepsi `t = 0` ⇒ `Prune(20000)` sonrası `Records() == 1`, `Units() == 1`, kalan `Find(2985, 2, 20000) != nullptr`.
   7. `Perception_HealRing`: Great healing benzeri (`nominal 960`) `t = 1000` hedef `2985`; HoT benzeri (`2500`, `hot`) `t = 2000` hedef `2985`; hasar büyüsü, Type4 meta, op CASTING, `target = -1`, `meta == nullptr` ⇒ `false` ve halka değişmez; `data[1] = 0` olsa da heal eklenir; `SumNominal(2985, 3000, 5000)` = 3460, `SumNominal(2985, 3000, 1500)` = 2500, başka hedef 0, `kSkillIdAny` joker 3460; `CountIn` aynı pencerelerle 2/1; 70 olay ekle ⇒ `Count() == 64`, `Total() == 70`, `At(0)` en yeni; kopya bağımsız.
4. Derleme ve test (§7). Çalışma zamanı denetimi yok (sunucu kodu değişmez). `Durum` → `UYGULANDI`, Uygulayıcı Raporu.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0; iki dosya `touch` edilip yeniden derlendiğinde değişen dosyalarda yeni uyarı yok (yalnızca eski `UpgradeHandler.cpp` C4789 olabilir)
- [ ] K2: `./tools/build.sh Debug` rc=0, yeni uyarı yok
- [ ] K3: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`, test sayısı plan başındakinden **7 fazla** (`252` → `259`), yedi yeni ad (`Perception_SkillMeta_Classify`, `Perception_Status_Type4`, `Perception_Status_Replace`, `Perception_Status_CureAndDeath`, `Perception_Status_PairSkills`, `Perception_Status_Capacity`, `Perception_HealRing`) her iki yapılandırmada `[ OK ]`
- [ ] K4 (`BotCore` saflığı): `git diff gece/2026-10-02...bot/F4-53 -- BotCore | grep '^+' | grep -E '#include|windows\.h|stdafx|GameServer|shared/'` boş (yorumlarda sunucu dosyası **dizinsiz** anılır: `MagicInstance.cpp:1861`, `GameServer/...` yazılmaz)
- [ ] K5 (sözleşme): `python3 tools/check-perception-contract.py` `RESULT: PASS`, R1 0, R2 0 ihlal (28 izinli), R3 0 ihlal (18 izinli), R4 0, R5 0; `--selftest` rc=0; eklenen satırlarda `g_pMain|GetUserPtr|m_Magictable|_MAGIC_TABLE|m_buffMap|m_pUser->` yok
- [ ] K6 (kapsam): `git diff --stat gece/2026-10-02...bot/F4-53` yalnızca `BotCore/Perception.h`, `Tests/BotCoreTests/PerceptionTests.cpp` ve bu plan dosyasını gösterir (`GameServer/`, `shared/`, `AIServer/`, `tools/`, `docs/` yok)
- [ ] K7 (yalnızca ekleme): `git diff gece/2026-10-02...bot/F4-53 -- BotCore/Perception.h | grep -c '^-[^-]'` = 0; `UnitView`/`NpcView`/`TeamMemberView`/`PerceptionSnapshot` ve F4-52 `SkillEventRing` satırları değişmedi
- [ ] K8 (kurallar): eklenen satırlar ASCII; iki dosya baştan sona CRLF (`file` çıktısı `with CRLF line terminators`; satır sayısı = CRLF sayısı); girinti sekme, Allman; `git diff --check` boş; eklenen satırlarda `printf`/`Sleep`/`CreateThread`/`rand(`/`new `/`malloc`/`std::map`/`std::vector` yok (sabit boyutlu diziler); yeni `#include` yok
- [ ] K9 (davranış): sunucu ikilisi değişmez: `GameServer/`, `shared/` farkı yok (K6) ve `ENABLED=0` varsayılan davranışı aynı (kod yalnızca `BotCore` başlığına eklenir, çağıranı yoktur)
- [ ] K10 (semantik, kodla): `Observe` içinde süre `ev.data[3]`'ten (`meta`'dan değil) okunur; `ev.data[1] == 0` Type4'te kayıt üretmez, heal'de ve cure'de `data[1]` okunmaz (`grep -n 'data\[1\]' BotCore/Perception.h` yalnızca Type4 dalında ve yorumlarda)

## 7. Doğrulama komutları

```bash
./tools/run-tests.sh Release 2>&1 | tail -3     # plan başında sayıyı not et (252)
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release && ./tools/run-tests.sh Debug
python3 tools/check-perception-contract.py | sed -n '1,14p'
python3 tools/check-perception-contract.py --selftest; echo rc=$?
git diff --stat gece/2026-10-02...bot/F4-53
git diff gece/2026-10-02...bot/F4-53 -- BotCore/Perception.h | grep -c '^-[^-]'
file BotCore/Perception.h Tests/BotCoreTests/PerceptionTests.cpp
git diff --check gece/2026-10-02...bot/F4-53
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3 (ASCII, CRLF, sekme, Allman, İngilizce yorum). Dosyalar CRLF'dir: ekleme araçları satır sonunu bozmamalı (`file` ile denetle).
- `BotCore` başlık-yalnızca ve sunucudan bağımsızdır (ADR-0016): `windows.h`, `stdafx.h`, `GameServer`, `shared` yok; `<cstdint>/<cstring>` dışında yeni include yok (zaten dosyada).
- Tüm kayıtlar `E` sınıfıdır ve **tahmindir**: başkasının buff'ı sunucuda `Update()` başına en çok bir kayıt kaldırılabildiği için (MEC-BUF-07) ya da oyuncu iptal ettiği için tahminden **önce ya da sonra** bitebilir; tablo bunu bilemez. Kodda bu bir hata değil; tasarım sınırıdır (yoruma yaz).
- `data[]` anlamı: Type4 yayınında `data[1]` = `bResult`, `data[3]` = süre sn (sunucu yazar); heal/cure'de `data[]`'ya güvenilmez (çağıranın gönderdiği değerler; gerçek insan istemcisinin değerleri ölçülmedi `[Ö]`) ve **okunmaz** (yalnızca `op`, `skillId`, `caster`, `target`). `data[3]` `int16_t`'dir: 32767 sn üstü süreler negatif görünür ve yok sayılır (`[A]`, pratikte en uzun buff 600 sn).
- Fonksiyonlar belirlenimlidir: saat çağrısı yok (`nowMs`/`ev.tMs` parametreyle gelir), dinamik bellek yok, iş parçacığı kilidi yok.
- İşaretçi döndüren sorgular (`Find`) yalnızca sonraki değişikliğe kadar geçerlidir: yoruma yaz.
- Kapsam büyütme: DoT/HoT kaydı, `UnitView` alanı, sunucu bağlaması yazma. Gerekirse Uygulayıcı Raporu'nda "açık soru" olarak yaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-53` (taban `gece/2026-10-02`) — `4f7c3ab [F4-53] Gozlenen durum tablosu: SkillMeta, ObservedStatusTable, HealObsRing (259 test)` + bu rapor commit'i.
- Değişen dosyalar ve neden:
  - `BotCore/Perception.h`: dosya sonuna (namespace kapanışından önce) tek bölüm eklendi; `kType5RemoveType3/4`, `kObsStatusUnits/PerUnit/kHealObsRing`, `SkillMeta`, `SkillSendsType3/4`, `SkillIsCureDebuff`, `SkillHealNominal`, `StatusObs`/`StatusRemainingMs`, `StatusUpdate`, `ObservedStatusTable`, `HealObs`/`HealObsRing`. +475/−0, yalnızca ekleme.
  - `Tests/BotCoreTests/PerceptionTests.cpp`: dosya sonuna anonim yardımcılar (`MakeEvent`, `MakeMeta`) ve 7 `TEST_CASE`. +429/−0.
  - `plans/F4-53-…md`: Durum satırı ve bu rapor.
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
  proj-GameServer.vcxproj -> …\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> …\build\bin\x86-Release\Server\AIServer.exe
  BotCoreTests.vcxproj -> …\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  `build.sh Debug` de rc=0. İki dosya `touch` edilip yeniden derlendi: değişen dosyalarda uyarı/hata yok (`UpgradeHandler.cpp` hariç).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ `build.sh Release` rc=0; `touch` sonrası değişen dosyalarda uyarı yok.
  - K2 ✔ `build.sh Debug` rc=0, uyarı yok.
  - K3 ✔ Release ve Debug `259 tests, 0 failed` (252 → 259, +7); yedi ad her iki yapılandırmada `[ OK ]`.
  - K4 ✔ `git diff -- BotCore | grep '^+' | grep -E '#include|windows\.h|stdafx|GameServer|shared/'` boş (yorumlarda sunucu dosyası dizinsiz anılır).
  - K5 ✔ `check-perception-contract.py` `RESULT: PASS` (R1..R5 0 ihlal), `--selftest` rc=0; eklenen satırlarda yasak simge yok.
  - K6 ✔ `git diff --stat` yalnızca üç dosya (`BotCore/Perception.h`, `Tests/BotCoreTests/PerceptionTests.cpp`, plan).
  - K7 ✔ `Perception.h` ekleme 475, silme 0; `UnitView`/`NpcView`/`TeamMemberView`/`PerceptionSnapshot`/`SkillEventRing` değişmedi.
  - K8 ✔ İki dosya ASCII + CRLF (`file`), girinti sekme/Allman, `git diff --check` boş; yasak yapı (printf/Sleep/CreateThread/rand(/new /malloc/std::map/std::vector) ve yeni `#include` yok.
  - K9 ✔ `GameServer/`, `shared/`, `AIServer/` farkı 0; kodun çağıranı yok.
  - K10 ✔ Süre `ev.data[3]`'ten okunur; `ev.data[1] == 0` yalnızca Type4'te kaydı engeller, heal ve cure yollarında `data[1]` okunmaz (birim testleriyle sabit).
- Plandan sapmalar ve gerekçeleri:
  - `Perception_HealRing` testinde plan metni "data[1]=0 olsa da heal eklenir" ile `SumNominal`/joker `3460` değerlerini aynı olay kümesinde istiyor; ilk iki heal (`data[1]=0`) sum öncesi eklendi, sonra sum'lar 3460/2500/0/3460 olarak sabitlendi. Taşma için ayrı `big` halkası kullanıldı (`Total() 70`) — plan sayıları birebir karşılandı.
  - Yorumlarda `shared/`, `GameServer/` dizinleri kullanılmadı (K4 grepsiz kalması için `structs.h`, `MagicInstance.cpp` biçiminde).
- Açık sorular:
  - Yok. (Bağlama, `UnitView` alanları, `WIZ_DEAD` temizliği ve `/bot snap status` plan gereği F4-60'a bırakıldı.)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-53` @ `<sha>`
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
