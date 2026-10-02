# F1-07: Büyü hasarı ve heal modeli (`tools/spell-model.py`)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F1 — Veri ve mekanik doğrulama (`docs/17` §2) |
| Branch | `bot/F1-07` (taban: `main`) |
| Bağımlı olduğu planlar | F1-04, F1-05, F1-06 (KAPANDI; `tools/stat-model.py` biçimi, botlar DB'de) |
| İlgili gereksinim / kabul | T-MECH-DMG-03 (büyü CHA ölçeği, model kısmı), MEC-DMG-03, MEC-DMG-06; `docs/05` |
| Tahmini büyüklük | M (1 yeni araç betiği; DB'ye yazılmaz) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Mage ve priest botlarının **Type3 büyü hasarını** (anlık hasar ve zamanla hasar) ve **heal** miktarını sunucu formülleriyle hesaplayan bir araç yazmak. F1-06'nın `tools/stat-model.py`'si fiziksel hasarı (R ve warrior Type1 skill'leri) kapsıyordu; bu araç büyü yolunu (`MagicInstance::GetMagicDamage`, `ExecuteType3`) kapsar. Sonuç: her mage/priest skill'i için hedef profiline göre hasar (ortalama/en düşük/en yüksek), MP maliyeti, cast/recast süresi, hedefi öldürmek için gereken cast sayısı; priest için heal miktarı ve MP başına heal. Bu değerler bot karar mantığının (utility) ve T-MECH-DMG-03 ölçümünün (model ± %15) girdisidir.

Çalışma zamanı ölçümü yok. Type4 (buff/debuff: stun, yavaşlatma, curse), Type5, Type8 kapsam dışıdır.

## 2. Bağlam (okunması zorunlu)

- `docs/03` §7 (MEC-DMG-03, -06), `docs/04` §4 (INT direnç bonusu, mage CHA ölçeği, "incineration" örneği), `docs/05` (skill kataloğu), `docs/appendix/A2_*`, `A3_*` (skill tabloları).
- `tools/stat-model.py` (F1-06): DB okuma, bot/ITEM/COEFFICIENT yükleme, `derived_stats`, 32 bit float yardımcıları (`f32`, `mul32`), `item_bonuses`. Bunları **bu dosyaya kopyala** (ortak modüle taşıma yok; dosya adı tireli olduğundan `import` edilemez) ve gerekirse uyarla.
- Sunucu kodu (Claude'un 2026-10-02'de okuduğu satırlar; kaymışsa gerçek satırı raporla):
  - `GameServer/MagicInstance.cpp:1258-1600` `MagicInstance::ExecuteType3`: tek hedefli veya alan; **saldırı büyüsü** koşulu: `sFirstDamage < 0` ve `bDirectType == 1` (veya 8) ve `nSkillID < 400000` → `damage = GetMagicDamage(hedef, sFirstDamage, bAttribute)` (`:1337-1340`); aksi halde `damage = sFirstDamage` (**heal** için pozitif değer olduğu gibi uygulanır, stat ölçeği yok). `bDuration == 0` ise anlık (`bDirectType 1`: `HpChangeMagic(damage, ...)`, `:1386-1396`); `bDuration != 0` ise: başlangıç hasarı `damage` (varsa) + **zamanla**: `sTimeDamage < 0 && bAttribute != 4` ise `duration_damage = GetMagicDamage(hedef, sTimeDamage, bAttribute)`, aksi halde `duration_damage = sTimeDamage` (`:1498-1520`); `tickCount = bDuration / 2` (2 sn aralık), tick başına `(int16)(duration_damage / tickCount)`, tick sayısı `(uint8) tickCount` (`:1531-1549`).
  - `GameServer/MagicInstance.cpp:2558-2717` `GetMagicDamage(pTarget, total_hit, attribute)` (oyuncu → oyuncu): `bCha = GetStat(CHA)` (**temel** CHA, item bonusu yok); `bCha > 86` ise `sMagicAmount = bCha − 86`; `+ m_sMagicAttackAmount` (varsayılan 0); `total_hit = total_hit × bCha / 186` (**tamsayı, sıfıra doğru kesme; `total_hit` negatif**); sonuç her zaman SUCCESS (oyuncu büyücü, kaçırma yok). Direnç: `attribute` 1 Ateş (`m_sFireR`), 2 Buz (`m_sColdR`), 3 Yıldırım (`m_sLightningR`), 4 Büyü (`m_sMagicR`), 5 Lanet (`m_sDiseaseR`), 6 Zehir (`m_sPoisonR`) için `total_r = (hedef.m_sXR + hedef.m_bAddXR) × hedef.m_bPctXR / 100` (varsayılan add 0, pct 100); sonra **`total_r += hedef.m_bResistanceBonus`** (`:2639`). `damage = 230 × total_hit / (total_r + 250)` (tamsayı); `random = myrand(0, damage)`: `myrand(min, max)` `min > max` ise **takas eder** (`shared/globals.cpp:16-22`, kapsayıcı), yani negatif `damage` için `random ∈ [damage, 0]`; `damage = (short)(random × 0.3f + damage × 0.85f) − sMagicAmount`. **Asa etkisi:** büyücü sağ elinde asa (`isStaff`) var **ve sol el boş** ise `righthand_damage = asa.Damage + m_bAddWeaponDamage`; `attribute != MAGIC_R (4)` ise `damage −= (short)((righthand_damage × 0.8f) + (righthand_damage × Level) / 60 + (attribute_damage × 0.8f) + (attribute_damage × Level) / 30)` (`attribute_damage = 0`, bu referans setlerde elemental item bonusu yok; `(rh × Level) / 60` **tamsayı** bölme); `hedef.m_bMagicDamageReduction < 100` ise `damage = damage × redüksiyon / 100` (varsayılan 100: değişmez); hava durumu etkisiz (`GetWeatherDamage` sonucu kullanılmıyor, MEC-DMG-04); **hedef oyuncuysa `damage /= 3`**; `MAX_DAMAGE 32000` tavanı.
  - `m_bResistanceBonus` (hedefin, `GameServer/User.cpp:2227-2290`, `SetUserAbility`): warrior pasif direnç `PRO_SKILL2` (strSkill bayt 6) puanına göre 30/60/90 (`10–19` → 30, `20–39` → 60, `40+` → 90), **kalkan yoksa yarısı**; ayrıca **tüm sınıflarda** `bInt = GetStat(INT) (temel)` `> 100` ise `+= (bInt − 100) / 2` (`:2287-2290`). Warrior/priest "Boldness" vb. HP koşullu pasifler uygulanmaz.
  - Hedefin item dirençleri: `GameServer/User.cpp:1352-1357`: `m_sFireR += FireR`, `m_sColdR += ColdR`, `m_sLightningR += LightningR`, `m_sMagicR += MagicR`, `m_sDiseaseR += CurseR`, `m_sPoisonR += PoisonR` (yalnızca ekipman yuvaları 0–13).
  - Heal/HoT için ayrıca `MAGIC_TYPE3` (`FirstDamage > 0` anlık heal, `TimeDamage > 0` ve `Duration` HoT; HoT tick: `(int16)(TimeDamage / (Duration/2))` her 2 sn).
- DB: `MAGIC(MagicNum, EnName, ... Msp, CastTime, ReCastTime, Type1, Type2, Range, Etc, UseStanding, Skill, SkillLevel, ...)`; `MAGIC_TYPE3(iNum, Name, Description, DirectType, FirstDamage, EndDamage, TimeDamage, Duration, Attribute, Radius, Angle)`. `MAGIC.Type1 = 3` Type3 skill demektir (kodda `bType[0]`; **doğrula**: `MAGIC.Type1` sütunu `bType[0]`'dır). `MAGIC.Skill = sınıfKodu × 10 + ağaç` (ör. 1105 mage ateş, 1106 buz, 1107 yıldırım; priest 1125 heal, 1126 buff, 1127 debuff); `SkillLevel` ≤ botun o ağaçtaki puanı olanlar kullanılabilir. Cast/recast `0,1 sn` biriminde (`CastTime=15` → 1,5 sn; `docs/03` MEC-MAG-02).
- **Veritabanı** (`AGENTS.md` §2.7): yerel DB'ye bağlanmak serbest. Bu plan yalnızca `MAGIC`, `MAGIC_TYPE3`, `ITEM`, `COEFFICIENT` ve bot `USERDATA` (`strUserID LIKE 'Bot%'`) satırlarını **okur**; DB'ye **yazılmaz**. Başka oyuncu satırlarını okuma. sqlcmd'de `-W` ile `-y` birlikte kullanılamaz.

## 3. Kapsam

**Yapılacaklar**

- `tools/spell-model.py` (yeni): bölüm M (büyü hasarı), H (heal), P (kontrol); `--selftest`.

**Kapsam dışı (yapılmayacak)**

- Type4/5/8 skill'ler, buff/debuff'ın hasara etkisi, stun/yavaşlatma etkileri, kritik/özel pasifler, `m_bMagicDamageReduction` değişiklikleri (varsayılan 100), mirror/reflect hasarı (`ReflectDamage`), mana absorb (`MEC-DMG-07`).
- NPC hedefler (yalnızca oyuncu → oyuncu).
- DB'ye yazma, `GameServer/`, `docs/**`, `AGENTS.md`, `db/*.sql`, kod değişikliği.
- Çalışma zamanı ölçümü.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/spell-model.py` | yeni | ASCII, LF, yalnızca standart kütüphane |

`tools/*` düzenlemesi opencode'da `ask` ister. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. **Önce oku:** §2'deki kod satırlarını depoda aç ve doğrula (özellikle `GetMagicDamage` `:2558-2717`, `ExecuteType3` `:1258-1600`, `SetUserAbility` direnç bölümü); kaymışsa gerçek satırı raporla. `MAGIC.Type1` ile kodun `bType[0]` eşlemesini (`MagicTableSet.h` veya benzeri) doğrula. `HpChangeMagic` (`Unit.cpp` veya `User.cpp`) içinde büyü hasarını **değiştiren** bir adım var mı oku (varsa raporla; yoksa "değiştirmez" yaz).

2. **`tools/spell-model.py`** — komut satırı `python3 tools/spell-model.py [--sqlcmd PATH] [--server ".\SQLEXPRESS"] [--db FDP_kn_online]` ve `--selftest`. Veri erişimi `tools/stat-model.py`'deki kalıpla. Yalnızca ASCII çıktı, deterministik (rastgele sayı yok: `random` tüm değerleri üzerinden **kesin** ortalama). Karus botları (6 profil) saldırgan/şifacı; hedef olarak 6 profil (WP, WG, PHD, PHB, MF, MI; Karus botları, item'lı değerlerle).
   - **Yardımcı:** `tdiv(a, b)` = C++ tamsayı bölmesi (sıfıra doğru kesme; negatif sayılarda `//` **yanlıştır**, `int(a / b)` float riski taşır: işaret ve mutlak değeri ayır). Ayrıca `short` dönüşümü için `to_short(x)` (sıfıra doğru kesme, 16 bit işaretli aralığa sarma).
   - **Hedef direnci:** her savunmacı için `res[attr]` = ekipman yuvalarındaki (0–13) direnç toplamları (`FireR, ColdR, LightningR, MagicR, CurseR→DiseaseR, PoisonR`) **+** `resistance_bonus` (warrior pasif: `strSkill` bayt 6 puanına göre, kalkan yoksa yarısı; **+** `(INT_temel − 100) / 2` eğer `INT_temel > 100`). `total_r = res[attr] + resistance_bonus` (add=0, pct=100).
   - **Bölüm M (anlık/ zamanla büyü hasarı):** her saldırgan bot × botun kullanabildiği her **Type3 saldırı skill'i** (`MAGIC.Type1 = 3`, `MAGIC_TYPE3.DirectType IN (1, 8)`, `FirstDamage < 0` **veya** `TimeDamage < 0`, `MagicNum < 400000`, `MAGIC.Skill = sınıfKodu×10 + ağaç` ve `SkillLevel ≤ strSkill[ağaç]`) × her savunmacı profil. Satır: `M <atk>-><def> skill=<MagicNum> <ad> attr=<n> first=<sFirst> time=<sTime> dur=<d> msp=<mp> cast_s=<..> recast_s=<..> dmg_avg=<..> dmg_min=<..> dmg_max=<..> dot_total=<..> dot_tick=<..> ticks=<n> def_hp=<maks HP> casts_to_kill=<n>`. Hesap: anlık kısım `first < 0` ise `GetMagicDamage(first)`; `dur != 0` ise ek olarak zamanla kısım: `time < 0 && attr != 4` ise `duration_damage = GetMagicDamage(time)`, değilse `time`; `tickCount = dur / 2.0` (float32), `dot_tick = to_short(int(duration_damage / tickCount))` (C++ `(int16)(float)` kesme), `ticks = int(tickCount)`, `dot_total = dot_tick × ticks`. `dmg_avg/min/max` **pozitif sayı** olarak yazılır (hasar büyüklüğü; kodda negatif). `casts_to_kill = ceil(def_hp / (dmg_avg + dot_total))` (anlık+DoT birlikte; yalnızca anlık hasar tanımsızsa `dot_total` ile). `def_hp` = savunmacının **item'lı maks HP**'si (F1-06 `derived_stats`'ın `max_hp`'si; MAX_PLAYER_HP 14000 tavanı dahil).
   - **Bölüm H (heal):** her priest bot (PHD, PHB) × botun kullanabildiği her Type3 skill'i (`FirstDamage > 0` **veya** `TimeDamage > 0`, `DirectType = 1`, `MagicNum < 400000`, ağaç/seviye koşulu aynı): `H <bot> skill=<MagicNum> <ad> first=<sFirst> time=<sTime> dur=<d> radius=<r> msp=<mp> cast_s=<..> recast_s=<..> heal_instant=<first> hot_tick=<..> ticks=<n> hot_total=<..> heal_per_msp=<(first+hot_total)/msp>`. Heal stat ölçeği taşımaz (`damage = sFirstDamage`). Ayrıca her skill için heal'in hedef profillerin maks HP'sine oranı: `H_PCT <bot> skill=.. def=<profil> pct_of_max_hp=<..>` (yalnızca `first > 0` ve en büyük 3 skill için yeterli).
   - **Bölüm P (kontrol, docs/04 §4):** `docs/04` §4'teki yorum örneğini yeniden üret: "büyü hasarı örneği: incineration −2500, CHA 247 → R 0 ≈ 1071, R 100 ≈ 780, R 200 ≈ 619; CHA 200 → R 0 ≈ 862, R 100 ≈ 626, R 200 ≈ 495" (bu değerler `docs/04`'te **asasız ve ortalama** olarak yazılmıştır; kodda hesabı elle yap: asa terimini **dahil et ve çıkar** olarak iki satır yaz). Satır: `P cha=<n> r=<n> staff=<yes|no> avg=<..> doc=<..>`.
   - Çıktı sırası sabit: `== M ==`, `== H ==`, `== P ==`.

3. **`--selftest`** (DB'siz, sentetik girdi). Beklenen sayılar Claude'un elle hesabıdır; **seninkiler farklı çıkarsa durup raporla** (kodu yeniden oku, bulgun doğru olabilir):
   - Girdi: saldırgan mage (Level 80, temel CHA 247, sağ elde `Damage=111` asa, sol el boş), skill `incineration` (`first = −2500`, `attr = 1` Ateş), hedef W-P (item ateş direnci toplamı 30, `resistance_bonus = 0`).
   - Ara değerler: `total_hit = tdiv(−2500 × 247, 186) = −3319`; `total_r = 30`; `damage = tdiv(230 × (−3319), 30 + 250) = −2726`; `sMagicAmount = 247 − 86 = 161`; asa terimi `to_short(111 × 0.8 + (111 × 80) // 60) = 236` (`88.8 + 148 = 236.8`).
   - Sonuçlar: `random = 0` için hasar `−2714 → /3 = −904` (büyüklük **904**); `random = −2726` için `(short)(−817.8 − 2317.1) = −3134`, `−3134 − 161 − 236 = −3531`, `/3 = −1177` (büyüklük **1177**). `dmg_min = 904`, `dmg_max = 1177`; ortalama bu iki sınır arasında, `random` üzerinden tam ortalama ile (`assert 904 < avg < 1177`; kesin değeri sen hesapla ve raporla).
   - `tdiv(−7, 2) == −3`, `tdiv(7, −2) == −3`, `tdiv(7, 2) == 3` (kesme davranışı).
   - DoT: `dur = 20`, `time = −600`, `duration_damage = −300` verildiğinde `tickCount = 10.0`, `dot_tick = −30`, `ticks = 10`, `dot_total = 300` (yalnızca aritmetik bloğu, `GetMagicDamage` çağırmadan).
   - Heal: `first = 1920` → `heal_instant = 1920`; `time = 800, dur = 30` → `tickCount = 15.0`, `hot_tick = 53` (`int16(800/15.0 = 53.33)`), `ticks = 15`, `hot_total = 795`.
   - Sonunda `selftest OK`, çıkış 0.

4. **Çalıştır:** `python3 tools/spell-model.py` çıktısını **kırpmadan** rapora yapıştır. Çok uzunsa (yüzlerce satır) tam çıktıyı yapıştır ama sonuna her bölüm için satır sayısını yaz.

5. **Kod okuması raporu (K4):** formül adımı → `dosya:satır` tablosu (özellikle `total_hit × bCha / 186`, direnç toplamı + `m_bResistanceBonus`, `230 × total_hit / (total_r + 250)`, `myrand` takası, asa terimi koşulları (sol el boş), `/3`, DoT tick hesabı, `HpChangeMagic` etkisi).

6. Raporu yaz; commit mesajı `[F1-07] ...`; yalnızca `tools/spell-model.py` eklenir.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/spell-model.py --selftest` → `selftest OK`, çıkış 0 (yukarıdaki assert'ler, özellikle 904 / 1177).
- [ ] K2: Çıktı M, H, P bölümleriyle eksiksiz ve çıkış 0; M'de hem mage (MF, MI) hem de saldırı büyüsü olan priest/warrior skill'leri varsa onlar; H'de PHD ve PHB; `P` satırlarında CHA 247/200 × R 0/100/200 × asa yok/var.
- [ ] K3: Bölüm P'de asa **dahil edilmemiş** satırlar `docs/04` §4'teki ~1071 / 780 / 619 ve ~862 / 626 / 495 değerlerine ±%2 içinde yakın veya fark açıklanmış (asasız ortalama, `/3` ve tamsayı kesmeleri); fark nedeni raporda.
- [ ] K4: Formül → kod satırı tablosu raporda (§5.5); `HpChangeMagic`'in büyü hasarını değiştirip değiştirmediği açıkça yazılmış.
- [ ] K5: Betik yalnızca bot satırlarını okuyor (`grep -n "USERDATA" tools/spell-model.py` tek sorgu, `LIKE 'Bot%'`); DB'ye yazan ifade yok (`grep -n -i -E "INSERT|UPDATE|DELETE|DROP" tools/spell-model.py` boş).
- [ ] K6: Kapsam: `git diff --stat main...bot/F1-07` yalnızca `tools/spell-model.py` ve plan dosyası; `git status --short` boş; `file tools/spell-model.py` ASCII, CR yok.
- [ ] K7 (Claude doğrular): çıktı bağımsız yeniden çalıştırılır; iki M satırı ve bir H satırı ayrı bir uygulamayla yeniden hesaplanır; kod satırları açılır.

## 7. Doğrulama komutları

```bash
python3 tools/spell-model.py --selftest
python3 tools/spell-model.py | head -60
grep -n "USERDATA" tools/spell-model.py
grep -n -i -E "INSERT|UPDATE|DELETE|DROP" tools/spell-model.py
file tools/spell-model.py
git diff --stat main...bot/F1-07
git status --short
```

## 8. Kısıtlar ve uyarılar

- **C++ semantiğini birebir uygula:** tamsayı bölmesi sıfıra doğru keser (negatif sayılarda Python `//` aşağı yuvarlar, **hatalıdır**); `short`/`int16` kesmeleri; `0.3f`, `0.85f`, `0.8f` **32 bit float**'tır (`f32` ile yuvarla); `(rh × Level) / 60` tamsayı bölmesidir; `myrand(0, damage)` negatif `damage`'de takas eder.
- `GetStat(CHA)`/`GetStat(INT)` **temel** statlardır (item bonusu girmez); direnç ve `def_hp` ise item'lı değerlerdir. Karıştırma.
- Modelde buff yok; `m_bMagicDamageReduction = 100`, `m_sMagicAttackAmount = 0`, `m_bAddXR = 0`, `m_bPctXR = 100` varsayılan.
- Araç ölçmez, **modeller**; çalışma zamanında doğrulanana kadar `[D]`/`[I]` etiketlidir.
- DB'ye yazma; bot olmayan satırları okuma.
- Yeni karar gerekirse **durup** raporda sor.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1 — 2026-10-02

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F1-07` (taban: `main` @ `41d2576`)
  - `1fa5e89` — `[F1-07] Buyu ve heal modeli araci` (`tools/spell-model.py`)
  - Bu rapor ve `Durum: UYGULANDI` ayrı commit'lenir.
- Değişen dosyalar ve nedenleri:
  - `tools/spell-model.py` (yeni, ASCII + LF): M (Type3 saldırı), H (priest heal/HoT), P (docs/04 §4 örneği) bölümleri + `--selftest`. F1-06'nın veri katmanı (DB okuma, `derived_stats`, `f32`) kopyalanıp uyarlandı; ana sınıf türü `get_magic_damage` benzetimi eklendi.
  - `plans/F1-07-buyu-ve-heal-modeli.md`: yalnızca `Durum` satırı ve bu rapor.
- **Veritabanına yazılmadı**; yalnızca `MAGIC`, `MAGIC_TYPE3`, `ITEM`, `COEFFICIENT` ve bot `USERDATA` (`LIKE 'Bot%'`) okundu. Başka oyuncu satırı okunmadı.

**Adım 1 — kod doğrulaması ve `HpChangeMagic`**
- `MAGIC.Type1` → `bType[0]` eşlemesi doğrulandı (`shared/database/MagicTableSet.h`: `Type1` sütunu `FetchByte(17, pData->bType[0])`).
- **`HpChangeMagic` büyü hasarını değiştirmez:** oyuncu için `Unit.h:195` `virtual void HpChangeMagic(int amount, ...) { HpChange(amount, pAttacker); }` çağrılır (yalnızca `CNpc` kendi sürümünü tanımlar, `Npc.cpp:226`; NPC hedef kapsam dışı). Hasar `GetMagicDamage`'da kesinleşir; `HpChange` sonucu değiştirmez.
- `GetWeatherDamage` çağrısının **dönüş değeri kullanılmıyor** (`MagicInstance.cpp:2707`), yani hava durumu etkisiz (MEC-DMG-04 teyit).
- Varsayılanlar: `m_bAttackAmount = 100` (`Unit.cpp:57`), `m_sMagicAttackAmount = 0` (`:58`), `m_bMagicDamageReduction = 100` (`:66`); buff güncellemeleri `MagicProcess.cpp:363/456/719/810` (modelde buff yok).

**K4 — formül adımı → kod satırı**

| Adım | Kod |
|---|---|
| `bCha = GetStat(CHA)` (**temel**), `sMagicAmount = bCha−86` (bCha>86), `+= m_sMagicAttackAmount` | `MagicInstance.cpp:2589-2593` |
| `total_hit = total_hit × bCha / 186` (int, sıfıra kesme) | `MagicInstance.cpp:2594` |
| Oyuncu büyücüde sonuç daima `SUCCESS` (kaçırma yok) | `:2595` |
| Direnç seçimi (`attribute` 1–6 → `m_sFireR…m_sPoisonR`) | `:2611-2637` |
| `total_r += m_bResistanceBonus` | `:2623` |
| `damage = 230 × total_hit / (total_r + 250)` | `:2693` |
| `random = myrand(0, damage)`; negatifte min/max **takas** (kapsayıcı) | `:2694`; `shared/globals.cpp:16-22` |
| `damage = (short)(random×0.3f + damage×0.85f) − sMagicAmount` | `:2695` |
| Asa koşulu: sağ el `isStaff` **ve** sol el boş; `righthand_damage = Damage` | `:2630-2636` |
| `attribute != MAGIC_R` ise asa terimi çıkarılır (`×0.8f`, `×Level/60` int bölme) | `:2699-2700` |
| `m_bMagicDamageReduction < 100` ise ölçekleme (varsayılan 100 → değişmez) | `:2701-2702` |
| `GetWeatherDamage` sonucu kullanılmıyor (hava etkisiz) | `:2707` |
| Hedef oyuncuysa `damage /= 3`; `MAX_DAMAGE` tavanı | `:2710-2715` |
| İşlem sırası (hasar mı heal mi): `sFirstDamage<0 && DirectType∈{1,8} && MagicNum<400000` | `MagicInstance.cpp:1336-1340` |
| Aksi halde `damage = sFirstDamage` (heal stat ölçeği yok) | `:1342` |
| Anlık uygulama (`DirectType 1`, `HpChangeMagic`) | `:1396` |
| Süreli: başlangıç hasarı | `:1523` |
| DoT: `sTimeDamage<0 && attr!=4` ise `GetMagicDamage`, değilse `TimeDamage` | `:1532-1535` |
| `tickCount = Duration / 2.0f`; `m_sHPAmount = (int16)(duration_damage / tickCount)`; `m_bTickLimit = (uint8)tickCount` | `:1560-1569` |
| Warrior pasif direnç (PRO_SKILL2 bayt 6; kalkan yoksa yarı) | `User.cpp:2254-2267` |
| `INT_temel > 100 → += (INT−100)/2` (tüm sınıflar) | `User.cpp:2307-2308` |
| Item dirençleri (`FireR`, `ColdR`, `LightningR`, `MagicR`, `CurseR→Disease`, `PoisonR`) | `User.cpp:1352-1357` |
| `MAGIC.Type1 → bType[0]` | `MagicTableSet.h` (`Type1` sütunu) |

**K2/K3 — çalıştırma ve bölüm içerikleri**
- `python3 tools/spell-model.py` çıkış 0; **417 satır** (M 336, H 30, H_PCT 36, P 12 + 3 başlık).
- M: 3 Karus saldırgan (WP, MF, MI) × 6 savunmacı; saldırı skill'leri filtreye göre: WP 1 (106725, ağaç 7 puanı 52) ×6 = 6; MF 28 ×6 = 168; MI 27 ×6 = 162. PHD/PHB için uygun Type3 saldırı büyüsü yok (1127 ağacındakiler `DirectType 2`); WG ağaç-7 puanı 0.
- H: PHD ve PHB × ağaç-5 puanı 60'a kadar 15 skill ×2 = 30; HoT'lar (`time>0`) tick = `int(Duration/2)`, `hot_tick = int(time/tickCount)`.
- H_PCT: her priest için `first+hot_total` en büyük 3 skill (112560, 112554, 112548) × 6 savunmacı = 36.
- P: CHA 247/200 × R 0/100/200 × asa yok/var = 12 satır.

**Adım 3/4 — model çıktısı (tam, kırpılmadı; aşağıdaki blok `/tmp/opencode/f1-07-model.txt` dosyasının birebir içeriğidir)**
```text
== M ==
M BotMF_K->BotMF_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=188.5 dmg_min=180.0 dmg_max=197.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=9
M BotMF_K->BotMI_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=188.5 dmg_min=180.0 dmg_max=197.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=12
M BotMF_K->BotPHB_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=185.9 dmg_min=178.0 dmg_max=194.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=19
M BotMF_K->BotPHD_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=185.9 dmg_min=178.0 dmg_max=194.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=19
M BotMF_K->BotWG_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=174.6 dmg_min=168.0 dmg_max=181.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=33
M BotMF_K->BotWP_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=186.8 dmg_min=179.0 dmg_max=195.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=31
M BotMF_K->BotMF_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=220.0 dot_tick=22.0 ticks=10 def_hp=1541 casts_to_kill=8
M BotMF_K->BotMI_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=220.0 dot_tick=22.0 ticks=10 def_hp=2228 casts_to_kill=11
M BotMF_K->BotPHB_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=220.0 dot_tick=22.0 ticks=10 def_hp=3491 casts_to_kill=16
M BotMF_K->BotPHD_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=220.0 dot_tick=22.0 ticks=10 def_hp=3491 casts_to_kill=16
M BotMF_K->BotWG_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=5650 casts_to_kill=29
M BotMF_K->BotWP_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=220.0 dot_tick=22.0 ticks=10 def_hp=5650 casts_to_kill=26
M BotMF_K->BotMF_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=236.2 dmg_min=221.0 dmg_max=252.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=7
M BotMF_K->BotMI_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=236.2 dmg_min=221.0 dmg_max=252.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=10
M BotMF_K->BotPHB_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=231.2 dmg_min=216.0 dmg_max=246.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=16
M BotMF_K->BotPHD_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=231.2 dmg_min=216.0 dmg_max=246.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=16
M BotMF_K->BotWG_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=210.2 dmg_min=198.0 dmg_max=222.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=27
M BotMF_K->BotWP_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=232.8 dmg_min=218.0 dmg_max=248.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=25
M BotMF_K->BotMF_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=212.6 dmg_min=200.0 dmg_max=225.0 dot_total=240.0 dot_tick=24.0 ticks=10 def_hp=1541 casts_to_kill=4
M BotMF_K->BotMI_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=212.6 dmg_min=200.0 dmg_max=225.0 dot_total=240.0 dot_tick=24.0 ticks=10 def_hp=2228 casts_to_kill=5
M BotMF_K->BotPHB_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=208.5 dmg_min=197.0 dmg_max=220.0 dot_total=240.0 dot_tick=24.0 ticks=10 def_hp=3491 casts_to_kill=8
M BotMF_K->BotPHD_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=208.5 dmg_min=197.0 dmg_max=220.0 dot_total=240.0 dot_tick=24.0 ticks=10 def_hp=3491 casts_to_kill=8
M BotMF_K->BotWG_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=192.2 dmg_min=183.0 dmg_max=201.0 dot_total=210.0 dot_tick=21.0 ticks=10 def_hp=5650 casts_to_kill=15
M BotMF_K->BotWP_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=209.8 dmg_min=198.0 dmg_max=222.0 dot_total=240.0 dot_tick=24.0 ticks=10 def_hp=5650 casts_to_kill=13
M BotMF_K->BotMF_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=331.2 dmg_min=301.0 dmg_max=361.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=5
M BotMF_K->BotMI_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=331.2 dmg_min=301.0 dmg_max=361.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=7
M BotMF_K->BotPHB_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=321.5 dmg_min=293.0 dmg_max=350.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMF_K->BotPHD_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=321.5 dmg_min=293.0 dmg_max=350.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMF_K->BotWG_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=281.1 dmg_min=259.0 dmg_max=304.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=21
M BotMF_K->BotWP_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=324.5 dmg_min=296.0 dmg_max=353.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=18
M BotMF_K->BotMF_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=331.2 dmg_min=301.0 dmg_max=361.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=5
M BotMF_K->BotMI_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=331.2 dmg_min=301.0 dmg_max=361.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=7
M BotMF_K->BotPHB_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=321.5 dmg_min=293.0 dmg_max=350.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMF_K->BotPHD_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=321.5 dmg_min=293.0 dmg_max=350.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMF_K->BotWG_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=281.1 dmg_min=259.0 dmg_max=304.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=21
M BotMF_K->BotWP_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=324.5 dmg_min=296.0 dmg_max=353.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=18
M BotMF_K->BotMF_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=416.5 dmg_min=374.0 dmg_max=459.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=4
M BotMF_K->BotMI_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=416.5 dmg_min=374.0 dmg_max=459.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=6
M BotMF_K->BotPHB_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=403.2 dmg_min=362.0 dmg_max=444.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=9
M BotMF_K->BotPHD_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=403.2 dmg_min=362.0 dmg_max=444.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=9
M BotMF_K->BotWG_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=345.5 dmg_min=313.0 dmg_max=378.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=17
M BotMF_K->BotWP_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=407.5 dmg_min=366.0 dmg_max=449.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=14
M BotMF_K->BotMF_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=294.6 dmg_min=270.0 dmg_max=319.0 dot_total=510.0 dot_tick=51.0 ticks=10 def_hp=1541 casts_to_kill=2
M BotMF_K->BotMI_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=294.6 dmg_min=270.0 dmg_max=319.0 dot_total=510.0 dot_tick=51.0 ticks=10 def_hp=2228 casts_to_kill=3
M BotMF_K->BotPHB_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=286.8 dmg_min=264.0 dmg_max=310.0 dot_total=490.0 dot_tick=49.0 ticks=10 def_hp=3491 casts_to_kill=5
M BotMF_K->BotPHD_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=286.8 dmg_min=264.0 dmg_max=310.0 dot_total=490.0 dot_tick=49.0 ticks=10 def_hp=3491 casts_to_kill=5
M BotMF_K->BotWG_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=253.9 dmg_min=236.0 dmg_max=272.0 dot_total=410.0 dot_tick=41.0 ticks=10 def_hp=5650 casts_to_kill=9
M BotMF_K->BotWP_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=289.2 dmg_min=266.0 dmg_max=313.0 dot_total=490.0 dot_tick=49.0 ticks=10 def_hp=5650 casts_to_kill=8
M BotMF_K->BotMF_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=340.2 dmg_min=309.0 dmg_max=371.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=5
M BotMF_K->BotMI_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=340.2 dmg_min=309.0 dmg_max=371.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=7
M BotMF_K->BotPHB_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=330.2 dmg_min=300.0 dmg_max=360.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMF_K->BotPHD_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=330.2 dmg_min=300.0 dmg_max=360.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMF_K->BotWG_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=288.2 dmg_min=265.0 dmg_max=312.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=20
M BotMF_K->BotWP_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=333.5 dmg_min=303.0 dmg_max=364.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=17
M BotMF_K->BotMF_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=302.5 dmg_min=277.0 dmg_max=328.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=6
M BotMF_K->BotMI_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=302.5 dmg_min=277.0 dmg_max=328.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=8
M BotMF_K->BotPHB_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=294.6 dmg_min=270.0 dmg_max=319.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=12
M BotMF_K->BotPHD_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=294.6 dmg_min=270.0 dmg_max=319.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=12
M BotMF_K->BotWG_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=259.9 dmg_min=241.0 dmg_max=279.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=22
M BotMF_K->BotWP_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=297.2 dmg_min=272.0 dmg_max=322.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=20
M BotMF_K->BotMF_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=559.1 dmg_min=495.0 dmg_max=623.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=3
M BotMF_K->BotMI_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=559.1 dmg_min=495.0 dmg_max=623.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=4
M BotMF_K->BotPHB_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=538.8 dmg_min=478.0 dmg_max=600.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=7
M BotMF_K->BotPHD_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=538.8 dmg_min=478.0 dmg_max=600.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=7
M BotMF_K->BotWG_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=452.2 dmg_min=404.0 dmg_max=500.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=13
M BotMF_K->BotWP_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=545.5 dmg_min=483.0 dmg_max=608.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=11
M BotMF_K->BotMF_K skill=110554 Fire Thorn attr=1 first=-1550 time=0 dur=0 msp=220 cast_s=1.5 recast_s=6.0 dmg_avg=657.5 dmg_min=579.0 dmg_max=736.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=3
M BotMF_K->BotMI_K skill=110554 Fire Thorn attr=1 first=-1550 time=0 dur=0 msp=220 cast_s=1.5 recast_s=6.0 dmg_avg=657.5 dmg_min=579.0 dmg_max=736.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=4
M BotMF_K->BotPHB_K skill=110554 Fire Thorn attr=1 first=-1550 time=0 dur=0 msp=220 cast_s=1.5 recast_s=6.0 dmg_avg=632.6 dmg_min=557.0 dmg_max=708.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=6
M BotMF_K->BotPHD_K skill=110554 Fire Thorn attr=1 first=-1550 time=0 dur=0 msp=220 cast_s=1.5 recast_s=6.0 dmg_avg=632.6 dmg_min=557.0 dmg_max=708.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=6
M BotMF_K->BotWG_K skill=110554 Fire Thorn attr=1 first=-1550 time=0 dur=0 msp=220 cast_s=1.5 recast_s=6.0 dmg_avg=526.2 dmg_min=467.0 dmg_max=585.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=11
M BotMF_K->BotWP_K skill=110554 Fire Thorn attr=1 first=-1550 time=0 dur=0 msp=220 cast_s=1.5 recast_s=6.0 dmg_avg=640.6 dmg_min=564.0 dmg_max=717.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=9
M BotMF_K->BotMF_K skill=110556 Manes of fire attr=1 first=-1015 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=475.8 dmg_min=424.0 dmg_max=527.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=4
M BotMF_K->BotMI_K skill=110556 Manes of fire attr=1 first=-1015 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=475.8 dmg_min=424.0 dmg_max=527.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=5
M BotMF_K->BotPHB_K skill=110556 Manes of fire attr=1 first=-1015 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=459.5 dmg_min=410.0 dmg_max=509.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=8
M BotMF_K->BotPHD_K skill=110556 Manes of fire attr=1 first=-1015 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=459.5 dmg_min=410.0 dmg_max=509.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=8
M BotMF_K->BotWG_K skill=110556 Manes of fire attr=1 first=-1015 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=389.8 dmg_min=351.0 dmg_max=429.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=15
M BotMF_K->BotWP_K skill=110556 Manes of fire attr=1 first=-1015 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=464.8 dmg_min=415.0 dmg_max=515.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=13
M BotMF_K->BotMF_K skill=110557 Fire Impact attr=1 first=-1260 time=-1000 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=559.1 dmg_min=495.0 dmg_max=623.0 dot_total=470.0 dot_tick=94.0 ticks=5 def_hp=1541 casts_to_kill=2
M BotMF_K->BotMI_K skill=110557 Fire Impact attr=1 first=-1260 time=-1000 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=559.1 dmg_min=495.0 dmg_max=623.0 dot_total=470.0 dot_tick=94.0 ticks=5 def_hp=2228 casts_to_kill=3
M BotMF_K->BotPHB_K skill=110557 Fire Impact attr=1 first=-1260 time=-1000 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=538.8 dmg_min=478.0 dmg_max=600.0 dot_total=450.0 dot_tick=90.0 ticks=5 def_hp=3491 casts_to_kill=4
M BotMF_K->BotPHD_K skill=110557 Fire Impact attr=1 first=-1260 time=-1000 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=538.8 dmg_min=478.0 dmg_max=600.0 dot_total=450.0 dot_tick=90.0 ticks=5 def_hp=3491 casts_to_kill=4
M BotMF_K->BotWG_K skill=110557 Fire Impact attr=1 first=-1260 time=-1000 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=452.2 dmg_min=404.0 dmg_max=500.0 dot_total=385.0 dot_tick=77.0 ticks=5 def_hp=5650 casts_to_kill=7
M BotMF_K->BotWP_K skill=110557 Fire Impact attr=1 first=-1260 time=-1000 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=545.5 dmg_min=483.0 dmg_max=608.0 dot_total=455.0 dot_tick=91.0 ticks=5 def_hp=5650 casts_to_kill=6
M BotMF_K->BotMF_K skill=110560 Supernova attr=1 first=-1800 time=-600 dur=20 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=742.5 dmg_min=651.0 dmg_max=834.0 dot_total=330.0 dot_tick=33.0 ticks=10 def_hp=1541 casts_to_kill=2
M BotMF_K->BotMI_K skill=110560 Supernova attr=1 first=-1800 time=-600 dur=20 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=742.5 dmg_min=651.0 dmg_max=834.0 dot_total=330.0 dot_tick=33.0 ticks=10 def_hp=2228 casts_to_kill=3
M BotMF_K->BotPHB_K skill=110560 Supernova attr=1 first=-1800 time=-600 dur=20 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=713.5 dmg_min=626.0 dmg_max=801.0 dot_total=320.0 dot_tick=32.0 ticks=10 def_hp=3491 casts_to_kill=4
M BotMF_K->BotPHD_K skill=110560 Supernova attr=1 first=-1800 time=-600 dur=20 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=713.5 dmg_min=626.0 dmg_max=801.0 dot_total=320.0 dot_tick=32.0 ticks=10 def_hp=3491 casts_to_kill=4
M BotMF_K->BotWG_K skill=110560 Supernova attr=1 first=-1800 time=-600 dur=20 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=589.8 dmg_min=521.0 dmg_max=659.0 dot_total=280.0 dot_tick=28.0 ticks=10 def_hp=5650 casts_to_kill=7
M BotMF_K->BotWP_K skill=110560 Supernova attr=1 first=-1800 time=-600 dur=20 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=722.8 dmg_min=634.0 dmg_max=811.0 dot_total=320.0 dot_tick=32.0 ticks=10 def_hp=5650 casts_to_kill=6
M BotMF_K->BotMF_K skill=110570 incineration attr=1 first=-2500 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=979.9 dmg_min=853.0 dmg_max=1107.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=2
M BotMF_K->BotMI_K skill=110570 incineration attr=1 first=-2500 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=979.9 dmg_min=853.0 dmg_max=1107.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=3
M BotMF_K->BotPHB_K skill=110570 incineration attr=1 first=-2500 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=939.5 dmg_min=818.0 dmg_max=1061.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=4
M BotMF_K->BotPHD_K skill=110570 incineration attr=1 first=-2500 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=939.5 dmg_min=818.0 dmg_max=1061.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=4
M BotMF_K->BotWG_K skill=110570 incineration attr=1 first=-2500 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=767.9 dmg_min=672.0 dmg_max=863.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=8
M BotMF_K->BotWP_K skill=110570 incineration attr=1 first=-2500 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=952.6 dmg_min=829.0 dmg_max=1076.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=6
M BotMF_K->BotMF_K skill=110571 meteor Fall attr=1 first=-2100 time=-600 dur=20 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=844.2 dmg_min=737.0 dmg_max=951.0 dot_total=330.0 dot_tick=33.0 ticks=10 def_hp=1541 casts_to_kill=2
M BotMF_K->BotMI_K skill=110571 meteor Fall attr=1 first=-2100 time=-600 dur=20 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=844.2 dmg_min=737.0 dmg_max=951.0 dot_total=330.0 dot_tick=33.0 ticks=10 def_hp=2228 casts_to_kill=2
M BotMF_K->BotPHB_K skill=110571 meteor Fall attr=1 first=-2100 time=-600 dur=20 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=810.2 dmg_min=708.0 dmg_max=912.0 dot_total=320.0 dot_tick=32.0 ticks=10 def_hp=3491 casts_to_kill=4
M BotMF_K->BotPHD_K skill=110571 meteor Fall attr=1 first=-2100 time=-600 dur=20 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=810.2 dmg_min=708.0 dmg_max=912.0 dot_total=320.0 dot_tick=32.0 ticks=10 def_hp=3491 casts_to_kill=4
M BotMF_K->BotWG_K skill=110571 meteor Fall attr=1 first=-2100 time=-600 dur=20 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=666.2 dmg_min=586.0 dmg_max=746.0 dot_total=280.0 dot_tick=28.0 ticks=10 def_hp=5650 casts_to_kill=6
M BotMF_K->BotWP_K skill=110571 meteor Fall attr=1 first=-2100 time=-600 dur=20 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=821.1 dmg_min=718.0 dmg_max=925.0 dot_total=320.0 dot_tick=32.0 ticks=10 def_hp=5650 casts_to_kill=5
M BotMF_K->BotMF_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=166.8 dmg_min=162.0 dmg_max=172.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=10
M BotMF_K->BotMI_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=166.8 dmg_min=162.0 dmg_max=172.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=14
M BotMF_K->BotPHB_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=169.5 dmg_min=164.0 dmg_max=175.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=21
M BotMF_K->BotPHD_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=169.5 dmg_min=164.0 dmg_max=175.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=21
M BotMF_K->BotWG_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=164.8 dmg_min=160.0 dmg_max=170.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=35
M BotMF_K->BotWP_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=175.8 dmg_min=169.0 dmg_max=182.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=33
M BotMF_K->BotMF_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=190.0 dot_tick=19.0 ticks=10 def_hp=1541 casts_to_kill=9
M BotMF_K->BotMI_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=190.0 dot_tick=19.0 ticks=10 def_hp=2228 casts_to_kill=12
M BotMF_K->BotPHB_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=190.0 dot_tick=19.0 ticks=10 def_hp=3491 casts_to_kill=19
M BotMF_K->BotPHD_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=190.0 dot_tick=19.0 ticks=10 def_hp=3491 casts_to_kill=19
M BotMF_K->BotWG_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=180.0 dot_tick=18.0 ticks=10 def_hp=5650 casts_to_kill=32
M BotMF_K->BotWP_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=5650 casts_to_kill=29
M BotMF_K->BotMF_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=196.2 dmg_min=187.0 dmg_max=206.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=8
M BotMF_K->BotMI_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=196.2 dmg_min=187.0 dmg_max=206.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=12
M BotMF_K->BotPHB_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=201.1 dmg_min=191.0 dmg_max=212.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=18
M BotMF_K->BotPHD_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=201.1 dmg_min=191.0 dmg_max=212.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=18
M BotMF_K->BotWG_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=192.6 dmg_min=183.0 dmg_max=202.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=30
M BotMF_K->BotWP_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=212.8 dmg_min=201.0 dmg_max=225.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=27
M BotMF_K->BotMF_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=181.5 dmg_min=174.0 dmg_max=189.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=1541 casts_to_kill=5
M BotMF_K->BotMI_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=181.5 dmg_min=174.0 dmg_max=189.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=2228 casts_to_kill=6
M BotMF_K->BotPHB_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=185.5 dmg_min=177.0 dmg_max=194.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=3491 casts_to_kill=10
M BotMF_K->BotPHD_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=185.5 dmg_min=177.0 dmg_max=194.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=3491 casts_to_kill=10
M BotMF_K->BotWG_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=178.8 dmg_min=172.0 dmg_max=186.0 dot_total=190.0 dot_tick=19.0 ticks=10 def_hp=5650 casts_to_kill=16
M BotMF_K->BotWP_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=194.6 dmg_min=185.0 dmg_max=204.0 dot_total=220.0 dot_tick=22.0 ticks=10 def_hp=5650 casts_to_kill=14
M BotMF_K->BotMF_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=255.2 dmg_min=237.0 dmg_max=274.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=7
M BotMF_K->BotMI_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=255.2 dmg_min=237.0 dmg_max=274.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=9
M BotMF_K->BotPHB_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=264.8 dmg_min=245.0 dmg_max=285.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=14
M BotMF_K->BotPHD_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=264.8 dmg_min=245.0 dmg_max=285.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=14
M BotMF_K->BotWG_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=248.2 dmg_min=231.0 dmg_max=266.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=23
M BotMF_K->BotWP_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=286.8 dmg_min=264.0 dmg_max=310.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=20
M BotMF_K->BotMF_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=255.2 dmg_min=237.0 dmg_max=274.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=7
M BotMF_K->BotMI_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=255.2 dmg_min=237.0 dmg_max=274.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=9
M BotMF_K->BotPHB_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=264.8 dmg_min=245.0 dmg_max=285.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=14
M BotMF_K->BotPHD_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=264.8 dmg_min=245.0 dmg_max=285.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=14
M BotMF_K->BotWG_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=248.2 dmg_min=231.0 dmg_max=266.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=23
M BotMF_K->BotWP_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=286.8 dmg_min=264.0 dmg_max=310.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=20
M BotMF_K->BotMF_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=307.5 dmg_min=281.0 dmg_max=334.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=6
M BotMF_K->BotMI_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=307.5 dmg_min=281.0 dmg_max=334.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=8
M BotMF_K->BotPHB_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=321.5 dmg_min=293.0 dmg_max=350.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMF_K->BotPHD_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=321.5 dmg_min=293.0 dmg_max=350.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMF_K->BotWG_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=297.8 dmg_min=273.0 dmg_max=323.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=19
M BotMF_K->BotWP_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=353.1 dmg_min=320.0 dmg_max=386.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=16
M BotMF_K->BotMF_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=232.2 dmg_min=217.0 dmg_max=247.0 dot_total=360.0 dot_tick=36.0 ticks=10 def_hp=1541 casts_to_kill=3
M BotMF_K->BotMI_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=232.2 dmg_min=217.0 dmg_max=247.0 dot_total=360.0 dot_tick=36.0 ticks=10 def_hp=2228 casts_to_kill=4
M BotMF_K->BotPHB_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=240.2 dmg_min=224.0 dmg_max=256.0 dot_total=380.0 dot_tick=38.0 ticks=10 def_hp=3491 casts_to_kill=6
M BotMF_K->BotPHD_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=240.2 dmg_min=224.0 dmg_max=256.0 dot_total=380.0 dot_tick=38.0 ticks=10 def_hp=3491 casts_to_kill=6
M BotMF_K->BotWG_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=226.6 dmg_min=212.0 dmg_max=241.0 dot_total=350.0 dot_tick=35.0 ticks=10 def_hp=5650 casts_to_kill=10
M BotMF_K->BotWP_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=258.2 dmg_min=239.0 dmg_max=277.0 dot_total=420.0 dot_tick=42.0 ticks=10 def_hp=5650 casts_to_kill=9
M BotMF_K->BotMF_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=260.6 dmg_min=241.0 dmg_max=280.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=6
M BotMF_K->BotMI_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=260.6 dmg_min=241.0 dmg_max=280.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=9
M BotMF_K->BotPHB_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=270.8 dmg_min=250.0 dmg_max=292.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=13
M BotMF_K->BotPHD_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=270.8 dmg_min=250.0 dmg_max=292.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=13
M BotMF_K->BotWG_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=253.5 dmg_min=235.0 dmg_max=272.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=23
M BotMF_K->BotWP_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=294.2 dmg_min=270.0 dmg_max=319.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=20
M BotMF_K->BotMF_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=237.2 dmg_min=221.0 dmg_max=253.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=7
M BotMF_K->BotMI_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=237.2 dmg_min=221.0 dmg_max=253.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=10
M BotMF_K->BotPHB_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=245.5 dmg_min=228.0 dmg_max=263.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=15
M BotMF_K->BotPHD_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=245.5 dmg_min=228.0 dmg_max=263.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=15
M BotMF_K->BotWG_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=231.5 dmg_min=217.0 dmg_max=246.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=25
M BotMF_K->BotWP_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=264.5 dmg_min=245.0 dmg_max=284.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=22
M BotMF_K->BotMF_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=395.8 dmg_min=356.0 dmg_max=435.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=4
M BotMF_K->BotMI_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=395.8 dmg_min=356.0 dmg_max=435.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=6
M BotMF_K->BotPHB_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=416.8 dmg_min=374.0 dmg_max=460.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=9
M BotMF_K->BotPHD_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=416.8 dmg_min=374.0 dmg_max=460.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=9
M BotMF_K->BotWG_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=381.1 dmg_min=344.0 dmg_max=419.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=15
M BotMF_K->BotWP_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=464.2 dmg_min=414.0 dmg_max=514.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=13
M BotMI_K->BotMF_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=162.2 dmg_min=155.0 dmg_max=169.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=10
M BotMI_K->BotMI_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=162.2 dmg_min=155.0 dmg_max=169.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=14
M BotMI_K->BotPHB_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=159.8 dmg_min=153.0 dmg_max=166.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=22
M BotMI_K->BotPHD_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=159.8 dmg_min=153.0 dmg_max=166.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=22
M BotMI_K->BotWG_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=150.5 dmg_min=145.0 dmg_max=156.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=38
M BotMI_K->BotWP_K skill=110503 Burn attr=1 first=-168 time=0 dur=0 msp=20 cast_s=1.0 recast_s=0.1 dmg_avg=160.5 dmg_min=154.0 dmg_max=167.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=36
M BotMI_K->BotMF_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=190.0 dot_tick=19.0 ticks=10 def_hp=1541 casts_to_kill=9
M BotMI_K->BotMI_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=190.0 dot_tick=19.0 ticks=10 def_hp=2228 casts_to_kill=12
M BotMI_K->BotPHB_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=180.0 dot_tick=18.0 ticks=10 def_hp=3491 casts_to_kill=20
M BotMI_K->BotPHD_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=180.0 dot_tick=18.0 ticks=10 def_hp=3491 casts_to_kill=20
M BotMI_K->BotWG_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=170.0 dot_tick=17.0 ticks=10 def_hp=5650 casts_to_kill=34
M BotMI_K->BotWP_K skill=110509 Blaze attr=1 first=0 time=-280 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=190.0 dot_tick=19.0 ticks=10 def_hp=5650 casts_to_kill=30
M BotMI_K->BotMF_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=200.5 dmg_min=188.0 dmg_max=213.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=8
M BotMI_K->BotMI_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=200.5 dmg_min=188.0 dmg_max=213.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=12
M BotMI_K->BotPHB_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=196.5 dmg_min=184.0 dmg_max=209.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=18
M BotMI_K->BotPHD_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=196.5 dmg_min=184.0 dmg_max=209.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=18
M BotMI_K->BotWG_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=179.5 dmg_min=170.0 dmg_max=189.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=32
M BotMI_K->BotWP_K skill=110515 Fire ball attr=1 first=-308 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=197.8 dmg_min=186.0 dmg_max=210.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=29
M BotMI_K->BotMF_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=181.2 dmg_min=171.0 dmg_max=191.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=1541 casts_to_kill=5
M BotMI_K->BotMI_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=181.2 dmg_min=171.0 dmg_max=191.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=2228 casts_to_kill=6
M BotMI_K->BotPHB_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=178.1 dmg_min=169.0 dmg_max=187.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=3491 casts_to_kill=10
M BotMI_K->BotPHD_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=178.1 dmg_min=169.0 dmg_max=187.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=3491 casts_to_kill=10
M BotMI_K->BotWG_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=164.9 dmg_min=158.0 dmg_max=172.0 dot_total=180.0 dot_tick=18.0 ticks=10 def_hp=5650 casts_to_kill=17
M BotMI_K->BotWP_K skill=110518 Ignition attr=1 first=-238 time=-336 dur=20 msp=60 cast_s=1.0 recast_s=0.1 dmg_avg=179.2 dmg_min=170.0 dmg_max=189.0 dot_total=200.0 dot_tick=20.0 ticks=10 def_hp=5650 casts_to_kill=15
M BotMI_K->BotMF_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=277.6 dmg_min=253.0 dmg_max=302.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=6
M BotMI_K->BotMI_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=277.6 dmg_min=253.0 dmg_max=302.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=9
M BotMI_K->BotPHB_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=269.8 dmg_min=247.0 dmg_max=293.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=13
M BotMI_K->BotPHD_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=269.8 dmg_min=247.0 dmg_max=293.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=13
M BotMI_K->BotWG_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=237.2 dmg_min=219.0 dmg_max=255.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=24
M BotMI_K->BotWP_K skill=110527 Fire spear attr=1 first=-588 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=272.1 dmg_min=249.0 dmg_max=296.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=21
M BotMI_K->BotMF_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=277.6 dmg_min=253.0 dmg_max=302.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=6
M BotMI_K->BotMI_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=277.6 dmg_min=253.0 dmg_max=302.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=9
M BotMI_K->BotPHB_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=269.8 dmg_min=247.0 dmg_max=293.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=13
M BotMI_K->BotPHD_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=269.8 dmg_min=247.0 dmg_max=293.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=13
M BotMI_K->BotWG_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=237.2 dmg_min=219.0 dmg_max=255.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=24
M BotMI_K->BotWP_K skill=110533 Fire burst attr=1 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=272.1 dmg_min=249.0 dmg_max=296.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=21
M BotMI_K->BotMF_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=346.8 dmg_min=312.0 dmg_max=381.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=5
M BotMI_K->BotMI_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=346.8 dmg_min=312.0 dmg_max=381.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=7
M BotMI_K->BotPHB_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=335.8 dmg_min=303.0 dmg_max=369.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMI_K->BotPHD_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=335.8 dmg_min=303.0 dmg_max=369.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMI_K->BotWG_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=289.2 dmg_min=263.0 dmg_max=315.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=20
M BotMI_K->BotWP_K skill=110535 Fire blast attr=1 first=-840 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=339.2 dmg_min=306.0 dmg_max=373.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=17
M BotMI_K->BotMF_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=247.8 dmg_min=228.0 dmg_max=268.0 dot_total=420.0 dot_tick=42.0 ticks=10 def_hp=1541 casts_to_kill=3
M BotMI_K->BotMI_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=247.8 dmg_min=228.0 dmg_max=268.0 dot_total=420.0 dot_tick=42.0 ticks=10 def_hp=2228 casts_to_kill=4
M BotMI_K->BotPHB_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=241.5 dmg_min=223.0 dmg_max=260.0 dot_total=400.0 dot_tick=40.0 ticks=10 def_hp=3491 casts_to_kill=6
M BotMI_K->BotPHD_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=241.5 dmg_min=223.0 dmg_max=260.0 dot_total=400.0 dot_tick=40.0 ticks=10 def_hp=3491 casts_to_kill=6
M BotMI_K->BotWG_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=214.8 dmg_min=200.0 dmg_max=230.0 dot_total=340.0 dot_tick=34.0 ticks=10 def_hp=5650 casts_to_kill=11
M BotMI_K->BotWP_K skill=110539 Hell fire attr=1 first=-480 time=-1120 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=243.6 dmg_min=224.0 dmg_max=263.0 dot_total=410.0 dot_tick=41.0 ticks=10 def_hp=5650 casts_to_kill=9
M BotMI_K->BotMF_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=284.9 dmg_min=260.0 dmg_max=310.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=6
M BotMI_K->BotMI_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=284.9 dmg_min=260.0 dmg_max=310.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=8
M BotMI_K->BotPHB_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=276.9 dmg_min=253.0 dmg_max=301.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=13
M BotMI_K->BotPHD_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=276.9 dmg_min=253.0 dmg_max=301.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=13
M BotMI_K->BotWG_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=242.9 dmg_min=224.0 dmg_max=262.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=24
M BotMI_K->BotWP_K skill=110543 specter of fire attr=1 first=-615 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=279.5 dmg_min=255.0 dmg_max=304.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=21
M BotMI_K->BotMF_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=254.2 dmg_min=233.0 dmg_max=275.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=7
M BotMI_K->BotMI_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=254.2 dmg_min=233.0 dmg_max=275.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=9
M BotMI_K->BotPHB_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=247.8 dmg_min=228.0 dmg_max=268.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=15
M BotMI_K->BotPHD_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=247.8 dmg_min=228.0 dmg_max=268.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=15
M BotMI_K->BotWG_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=219.8 dmg_min=204.0 dmg_max=235.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=26
M BotMI_K->BotWP_K skill=110545 Inferno attr=1 first=-504 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=249.8 dmg_min=230.0 dmg_max=270.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=23
M BotMI_K->BotMF_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=462.2 dmg_min=410.0 dmg_max=514.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=4
M BotMI_K->BotMI_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=462.2 dmg_min=410.0 dmg_max=514.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=5
M BotMI_K->BotPHB_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=445.6 dmg_min=396.0 dmg_max=495.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=8
M BotMI_K->BotPHD_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=445.6 dmg_min=396.0 dmg_max=495.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=8
M BotMI_K->BotWG_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=375.5 dmg_min=337.0 dmg_max=414.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=16
M BotMI_K->BotWP_K skill=110551 Pillar of fire attr=1 first=-1260 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=450.9 dmg_min=401.0 dmg_max=501.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=13
M BotMI_K->BotMF_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=144.5 dmg_min=140.0 dmg_max=149.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=11
M BotMI_K->BotMI_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=144.5 dmg_min=140.0 dmg_max=149.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=16
M BotMI_K->BotPHB_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=146.8 dmg_min=142.0 dmg_max=151.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=24
M BotMI_K->BotPHD_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=146.8 dmg_min=142.0 dmg_max=151.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=24
M BotMI_K->BotWG_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=142.9 dmg_min=139.0 dmg_max=147.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=40
M BotMI_K->BotWP_K skill=110603 Freeze attr=2 first=-118 time=0 dur=0 msp=20 cast_s=1.5 recast_s=0.1 dmg_avg=151.8 dmg_min=146.0 dmg_max=157.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=38
M BotMI_K->BotMF_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=160.0 dot_tick=16.0 ticks=10 def_hp=1541 casts_to_kill=10
M BotMI_K->BotMI_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=160.0 dot_tick=16.0 ticks=10 def_hp=2228 casts_to_kill=14
M BotMI_K->BotPHB_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=160.0 dot_tick=16.0 ticks=10 def_hp=3491 casts_to_kill=22
M BotMI_K->BotPHD_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=160.0 dot_tick=16.0 ticks=10 def_hp=3491 casts_to_kill=22
M BotMI_K->BotWG_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=160.0 dot_tick=16.0 ticks=10 def_hp=5650 casts_to_kill=36
M BotMI_K->BotWP_K skill=110609 Chill attr=2 first=0 time=-196 dur=20 msp=30 cast_s=1.5 recast_s=5.3 dmg_avg=0.0 dmg_min=0.0 dmg_max=0.0 dot_total=170.0 dot_tick=17.0 ticks=10 def_hp=5650 casts_to_kill=34
M BotMI_K->BotMF_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=168.2 dmg_min=160.0 dmg_max=176.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=10
M BotMI_K->BotMI_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=168.2 dmg_min=160.0 dmg_max=176.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=14
M BotMI_K->BotPHB_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=172.5 dmg_min=164.0 dmg_max=181.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=21
M BotMI_K->BotPHD_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=172.5 dmg_min=164.0 dmg_max=181.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=21
M BotMI_K->BotWG_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=165.6 dmg_min=158.0 dmg_max=173.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=35
M BotMI_K->BotWP_K skill=110615 Ice arrow attr=2 first=-216 time=0 dur=0 msp=50 cast_s=1.5 recast_s=4.3 dmg_avg=181.8 dmg_min=172.0 dmg_max=192.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=32
M BotMI_K->BotMF_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=156.5 dmg_min=150.0 dmg_max=163.0 dot_total=170.0 dot_tick=17.0 ticks=10 def_hp=1541 casts_to_kill=5
M BotMI_K->BotMI_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=156.5 dmg_min=150.0 dmg_max=163.0 dot_total=170.0 dot_tick=17.0 ticks=10 def_hp=2228 casts_to_kill=7
M BotMI_K->BotPHB_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=159.5 dmg_min=153.0 dmg_max=166.0 dot_total=170.0 dot_tick=17.0 ticks=10 def_hp=3491 casts_to_kill=11
M BotMI_K->BotPHD_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=159.5 dmg_min=153.0 dmg_max=166.0 dot_total=170.0 dot_tick=17.0 ticks=10 def_hp=3491 casts_to_kill=11
M BotMI_K->BotWG_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=154.2 dmg_min=148.0 dmg_max=160.0 dot_total=160.0 dot_tick=16.0 ticks=10 def_hp=5650 casts_to_kill=18
M BotMI_K->BotWP_K skill=110618 Solid attr=2 first=-167 time=-236 dur=20 msp=60 cast_s=1.5 recast_s=0.1 dmg_avg=166.8 dmg_min=159.0 dmg_max=174.0 dot_total=180.0 dot_tick=18.0 ticks=10 def_hp=5650 casts_to_kill=17
M BotMI_K->BotMF_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=215.8 dmg_min=201.0 dmg_max=231.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=8
M BotMI_K->BotMI_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=215.8 dmg_min=201.0 dmg_max=231.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=11
M BotMI_K->BotPHB_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=223.8 dmg_min=208.0 dmg_max=240.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=16
M BotMI_K->BotPHD_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=223.8 dmg_min=208.0 dmg_max=240.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=16
M BotMI_K->BotWG_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=210.5 dmg_min=196.0 dmg_max=225.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=27
M BotMI_K->BotWP_K skill=110627 Ice orb attr=2 first=-412 time=0 dur=0 msp=80 cast_s=1.5 recast_s=4.3 dmg_avg=241.8 dmg_min=223.0 dmg_max=261.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=24
M BotMI_K->BotMF_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=215.8 dmg_min=201.0 dmg_max=231.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=8
M BotMI_K->BotMI_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=215.8 dmg_min=201.0 dmg_max=231.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=11
M BotMI_K->BotPHB_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=223.8 dmg_min=208.0 dmg_max=240.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=16
M BotMI_K->BotPHD_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=223.8 dmg_min=208.0 dmg_max=240.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=16
M BotMI_K->BotWG_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=210.5 dmg_min=196.0 dmg_max=225.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=27
M BotMI_K->BotWP_K skill=110633 Ice burst attr=2 first=-412 time=0 dur=0 msp=150 cast_s=1.5 recast_s=0.1 dmg_avg=241.8 dmg_min=223.0 dmg_max=261.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=24
M BotMI_K->BotMF_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=258.5 dmg_min=237.0 dmg_max=280.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=6
M BotMI_K->BotMI_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=258.5 dmg_min=237.0 dmg_max=280.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=9
M BotMI_K->BotPHB_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=269.8 dmg_min=247.0 dmg_max=293.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=13
M BotMI_K->BotPHD_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=269.8 dmg_min=247.0 dmg_max=293.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=13
M BotMI_K->BotWG_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=250.5 dmg_min=230.0 dmg_max=271.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=23
M BotMI_K->BotWP_K skill=110635 Ice blast attr=2 first=-588 time=0 dur=0 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=295.5 dmg_min=269.0 dmg_max=322.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=20
M BotMI_K->BotMF_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=197.6 dmg_min=185.0 dmg_max=210.0 dot_total=300.0 dot_tick=30.0 ticks=10 def_hp=1541 casts_to_kill=4
M BotMI_K->BotMI_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=197.6 dmg_min=185.0 dmg_max=210.0 dot_total=300.0 dot_tick=30.0 ticks=10 def_hp=2228 casts_to_kill=5
M BotMI_K->BotPHB_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=203.8 dmg_min=191.0 dmg_max=217.0 dot_total=320.0 dot_tick=32.0 ticks=10 def_hp=3491 casts_to_kill=7
M BotMI_K->BotPHD_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=203.8 dmg_min=191.0 dmg_max=217.0 dot_total=320.0 dot_tick=32.0 ticks=10 def_hp=3491 casts_to_kill=7
M BotMI_K->BotWG_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=192.8 dmg_min=181.0 dmg_max=204.0 dot_total=290.0 dot_tick=29.0 ticks=10 def_hp=5650 casts_to_kill=12
M BotMI_K->BotWP_K skill=110639 Frostbite attr=2 first=-336 time=-784 dur=20 msp=150 cast_s=1.5 recast_s=4.3 dmg_avg=218.5 dmg_min=203.0 dmg_max=234.0 dot_total=350.0 dot_tick=35.0 ticks=10 def_hp=5650 casts_to_kill=10
M BotMI_K->BotMF_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=220.5 dmg_min=205.0 dmg_max=236.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=7
M BotMI_K->BotMI_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=220.5 dmg_min=205.0 dmg_max=236.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=11
M BotMI_K->BotPHB_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=228.8 dmg_min=212.0 dmg_max=246.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=16
M BotMI_K->BotPHD_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=228.8 dmg_min=212.0 dmg_max=246.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=16
M BotMI_K->BotWG_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=214.5 dmg_min=200.0 dmg_max=229.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=27
M BotMI_K->BotWP_K skill=110643 specter of Ice attr=2 first=-431 time=0 dur=0 msp=75 cast_s=1.0 recast_s=0.1 dmg_avg=247.5 dmg_min=228.0 dmg_max=267.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=23
M BotMI_K->BotMF_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=201.5 dmg_min=189.0 dmg_max=214.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=8
M BotMI_K->BotMI_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=201.5 dmg_min=189.0 dmg_max=214.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=12
M BotMI_K->BotPHB_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=208.2 dmg_min=194.0 dmg_max=222.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=17
M BotMI_K->BotPHD_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=208.2 dmg_min=194.0 dmg_max=222.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=17
M BotMI_K->BotWG_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=196.9 dmg_min=185.0 dmg_max=209.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=29
M BotMI_K->BotWP_K skill=110645 Blizzard attr=2 first=-353 time=0 dur=0 msp=200 cast_s=1.5 recast_s=15.3 dmg_avg=223.6 dmg_min=207.0 dmg_max=240.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=26
M BotMI_K->BotMF_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=329.8 dmg_min=298.0 dmg_max=362.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=5
M BotMI_K->BotMI_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=329.8 dmg_min=298.0 dmg_max=362.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=7
M BotMI_K->BotPHB_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=346.8 dmg_min=312.0 dmg_max=381.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMI_K->BotPHD_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=346.8 dmg_min=312.0 dmg_max=381.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=11
M BotMI_K->BotWG_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=317.8 dmg_min=288.0 dmg_max=348.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=18
M BotMI_K->BotWP_K skill=110651 Ice comet attr=2 first=-882 time=0 dur=0 msp=160 cast_s=1.5 recast_s=5.3 dmg_avg=385.2 dmg_min=345.0 dmg_max=426.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=15
M BotMI_K->BotMF_K skill=110656 Manes of Ice attr=2 first=-711 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=288.2 dmg_min=262.0 dmg_max=314.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=6
M BotMI_K->BotMI_K skill=110656 Manes of Ice attr=2 first=-711 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=288.2 dmg_min=262.0 dmg_max=314.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=8
M BotMI_K->BotPHB_K skill=110656 Manes of Ice attr=2 first=-711 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=301.8 dmg_min=274.0 dmg_max=330.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=12
M BotMI_K->BotPHD_K skill=110656 Manes of Ice attr=2 first=-711 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=301.8 dmg_min=274.0 dmg_max=330.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=12
M BotMI_K->BotWG_K skill=110656 Manes of Ice attr=2 first=-711 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=278.9 dmg_min=254.0 dmg_max=303.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=21
M BotMI_K->BotWP_K skill=110656 Manes of Ice attr=2 first=-711 time=0 dur=0 msp=95 cast_s=1.0 recast_s=0.1 dmg_avg=332.8 dmg_min=300.0 dmg_max=365.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=17
M BotMI_K->BotMF_K skill=110657 Ice Impact attr=2 first=-882 time=-700 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=329.8 dmg_min=298.0 dmg_max=362.0 dot_total=285.0 dot_tick=57.0 ticks=5 def_hp=1541 casts_to_kill=3
M BotMI_K->BotMI_K skill=110657 Ice Impact attr=2 first=-882 time=-700 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=329.8 dmg_min=298.0 dmg_max=362.0 dot_total=285.0 dot_tick=57.0 ticks=5 def_hp=2228 casts_to_kill=4
M BotMI_K->BotPHB_K skill=110657 Ice Impact attr=2 first=-882 time=-700 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=346.8 dmg_min=312.0 dmg_max=381.0 dot_total=295.0 dot_tick=59.0 ticks=5 def_hp=3491 casts_to_kill=6
M BotMI_K->BotPHD_K skill=110657 Ice Impact attr=2 first=-882 time=-700 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=346.8 dmg_min=312.0 dmg_max=381.0 dot_total=295.0 dot_tick=59.0 ticks=5 def_hp=3491 casts_to_kill=6
M BotMI_K->BotWG_K skill=110657 Ice Impact attr=2 first=-882 time=-700 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=317.8 dmg_min=288.0 dmg_max=348.0 dot_total=275.0 dot_tick=55.0 ticks=5 def_hp=5650 casts_to_kill=10
M BotMI_K->BotWP_K skill=110657 Ice Impact attr=2 first=-882 time=-700 dur=10 msp=220 cast_s=1.5 recast_s=20.3 dmg_avg=385.2 dmg_min=345.0 dmg_max=426.0 dot_total=325.0 dot_tick=65.0 ticks=5 def_hp=5650 casts_to_kill=8
M BotMI_K->BotMF_K skill=110660 Frost nova attr=2 first=-1260 time=-420 dur=0 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=421.2 dmg_min=375.0 dmg_max=467.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=4
M BotMI_K->BotMI_K skill=110660 Frost nova attr=2 first=-1260 time=-420 dur=0 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=421.2 dmg_min=375.0 dmg_max=467.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=6
M BotMI_K->BotPHB_K skill=110660 Frost nova attr=2 first=-1260 time=-420 dur=0 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=445.6 dmg_min=396.0 dmg_max=495.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=8
M BotMI_K->BotPHD_K skill=110660 Frost nova attr=2 first=-1260 time=-420 dur=0 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=445.6 dmg_min=396.0 dmg_max=495.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=8
M BotMI_K->BotWG_K skill=110660 Frost nova attr=2 first=-1260 time=-420 dur=0 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=404.5 dmg_min=361.0 dmg_max=448.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=14
M BotMI_K->BotWP_K skill=110660 Frost nova attr=2 first=-1260 time=-420 dur=0 msp=400 cast_s=1.5 recast_s=15.3 dmg_avg=500.5 dmg_min=443.0 dmg_max=558.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=12
M BotMI_K->BotMF_K skill=110670 Prismatic attr=2 first=-1750 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=540.2 dmg_min=477.0 dmg_max=604.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=3
M BotMI_K->BotMI_K skill=110670 Prismatic attr=2 first=-1750 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=540.2 dmg_min=477.0 dmg_max=604.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=5
M BotMI_K->BotPHB_K skill=110670 Prismatic attr=2 first=-1750 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=573.8 dmg_min=505.0 dmg_max=642.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=7
M BotMI_K->BotPHD_K skill=110670 Prismatic attr=2 first=-1750 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=573.8 dmg_min=505.0 dmg_max=642.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=7
M BotMI_K->BotWG_K skill=110670 Prismatic attr=2 first=-1750 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=516.5 dmg_min=456.0 dmg_max=577.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=11
M BotMI_K->BotWP_K skill=110670 Prismatic attr=2 first=-1750 time=0 dur=0 msp=390 cast_s=1.1 recast_s=21.3 dmg_avg=650.1 dmg_min=570.0 dmg_max=730.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=9
M BotMI_K->BotMF_K skill=110671 ice storm attr=2 first=-1470 time=0 dur=0 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=472.1 dmg_min=419.0 dmg_max=526.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=4
M BotMI_K->BotMI_K skill=110671 ice storm attr=2 first=-1470 time=0 dur=0 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=472.1 dmg_min=419.0 dmg_max=526.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=5
M BotMI_K->BotPHB_K skill=110671 ice storm attr=2 first=-1470 time=0 dur=0 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=500.5 dmg_min=443.0 dmg_max=558.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=7
M BotMI_K->BotPHD_K skill=110671 ice storm attr=2 first=-1470 time=0 dur=0 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=500.5 dmg_min=443.0 dmg_max=558.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=7
M BotMI_K->BotWG_K skill=110671 ice storm attr=2 first=-1470 time=0 dur=0 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=452.5 dmg_min=402.0 dmg_max=503.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=13
M BotMI_K->BotWP_K skill=110671 ice storm attr=2 first=-1470 time=0 dur=0 msp=600 cast_s=1.3 recast_s=18.3 dmg_avg=564.5 dmg_min=497.0 dmg_max=632.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=11
M BotWP_K->BotMF_K skill=106725 Blade of hate attr=4 first=-150 time=0 dur=0 msp=60 cast_s=1.5 recast_s=5.1 dmg_avg=9.5 dmg_min=8.0 dmg_max=11.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=1541 casts_to_kill=162
M BotWP_K->BotMI_K skill=106725 Blade of hate attr=4 first=-150 time=0 dur=0 msp=60 cast_s=1.5 recast_s=5.1 dmg_avg=9.5 dmg_min=8.0 dmg_max=11.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=2228 casts_to_kill=234
M BotWP_K->BotPHB_K skill=106725 Blade of hate attr=4 first=-150 time=0 dur=0 msp=60 cast_s=1.5 recast_s=5.1 dmg_avg=9.2 dmg_min=8.0 dmg_max=11.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=380
M BotWP_K->BotPHD_K skill=106725 Blade of hate attr=4 first=-150 time=0 dur=0 msp=60 cast_s=1.5 recast_s=5.1 dmg_avg=9.2 dmg_min=8.0 dmg_max=11.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=3491 casts_to_kill=380
M BotWP_K->BotWG_K skill=106725 Blade of hate attr=4 first=-150 time=0 dur=0 msp=60 cast_s=1.5 recast_s=5.1 dmg_avg=8.5 dmg_min=7.0 dmg_max=10.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=665
M BotWP_K->BotWP_K skill=106725 Blade of hate attr=4 first=-150 time=0 dur=0 msp=60 cast_s=1.5 recast_s=5.1 dmg_avg=11.5 dmg_min=10.0 dmg_max=13.0 dot_total=0.0 dot_tick=0.0 ticks=0 def_hp=5650 casts_to_kill=490
== H ==
H BotPHB_K skill=112500 minor healing first=60 time=0 dur=0 radius=0 msp=10 cast_s=1.5 recast_s=0.1 heal_instant=60 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=6.00
H BotPHB_K skill=112503 Light restore first=0 time=100 dur=30 radius=0 msp=25 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=6 ticks=15 hot_total=90 heal_per_msp=3.60
H BotPHB_K skill=112509 healing first=120 time=0 dur=0 radius=0 msp=20 cast_s=1.5 recast_s=0.1 heal_instant=120 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=6.00
H BotPHB_K skill=112512 Restore first=0 time=200 dur=30 radius=0 msp=30 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=13 ticks=15 hot_total=195 heal_per_msp=6.50
H BotPHB_K skill=112518 major healing first=240 time=0 dur=0 radius=0 msp=40 cast_s=1.5 recast_s=0.1 heal_instant=240 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=6.00
H BotPHB_K skill=112521 Major restore first=0 time=400 dur=30 radius=0 msp=100 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=26 ticks=15 hot_total=390 heal_per_msp=3.90
H BotPHB_K skill=112527 Great healing first=960 time=0 dur=0 radius=0 msp=80 cast_s=1.5 recast_s=2.0 heal_instant=960 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=12.00
H BotPHB_K skill=112530 Great restore first=0 time=800 dur=30 radius=0 msp=200 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=53 ticks=15 hot_total=795 heal_per_msp=3.98
H BotPHB_K skill=112536 Massive healing first=960 time=0 dur=0 radius=0 msp=160 cast_s=1.5 recast_s=0.1 heal_instant=960 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=6.00
H BotPHB_K skill=112539 Massive restore first=0 time=1500 dur=30 radius=0 msp=375 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=100 ticks=15 hot_total=1500 heal_per_msp=4.00
H BotPHB_K skill=112545 Superior healing first=1920 time=0 dur=0 radius=0 msp=320 cast_s=1.5 recast_s=0.1 heal_instant=1920 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=6.00
H BotPHB_K skill=112548 Superior restore first=0 time=2500 dur=30 radius=0 msp=625 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=166 ticks=15 hot_total=2490 heal_per_msp=3.98
H BotPHB_K skill=112554 Complete healing first=10000 time=0 dur=0 radius=0 msp=960 cast_s=1.5 recast_s=5.4 heal_instant=10000 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=10.42
H BotPHB_K skill=112557 Group massive healing first=960 time=0 dur=0 radius=30 msp=960 cast_s=1.5 recast_s=5.4 heal_instant=960 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=1.00
H BotPHB_K skill=112560 Group complete healing first=10000 time=0 dur=0 radius=30 msp=1920 cast_s=1.5 recast_s=6.4 heal_instant=10000 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=5.21
H BotPHD_K skill=112500 minor healing first=60 time=0 dur=0 radius=0 msp=10 cast_s=1.5 recast_s=0.1 heal_instant=60 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=6.00
H BotPHD_K skill=112503 Light restore first=0 time=100 dur=30 radius=0 msp=25 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=6 ticks=15 hot_total=90 heal_per_msp=3.60
H BotPHD_K skill=112509 healing first=120 time=0 dur=0 radius=0 msp=20 cast_s=1.5 recast_s=0.1 heal_instant=120 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=6.00
H BotPHD_K skill=112512 Restore first=0 time=200 dur=30 radius=0 msp=30 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=13 ticks=15 hot_total=195 heal_per_msp=6.50
H BotPHD_K skill=112518 major healing first=240 time=0 dur=0 radius=0 msp=40 cast_s=1.5 recast_s=0.1 heal_instant=240 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=6.00
H BotPHD_K skill=112521 Major restore first=0 time=400 dur=30 radius=0 msp=100 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=26 ticks=15 hot_total=390 heal_per_msp=3.90
H BotPHD_K skill=112527 Great healing first=960 time=0 dur=0 radius=0 msp=80 cast_s=1.5 recast_s=2.0 heal_instant=960 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=12.00
H BotPHD_K skill=112530 Great restore first=0 time=800 dur=30 radius=0 msp=200 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=53 ticks=15 hot_total=795 heal_per_msp=3.98
H BotPHD_K skill=112536 Massive healing first=960 time=0 dur=0 radius=0 msp=160 cast_s=1.5 recast_s=0.1 heal_instant=960 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=6.00
H BotPHD_K skill=112539 Massive restore first=0 time=1500 dur=30 radius=0 msp=375 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=100 ticks=15 hot_total=1500 heal_per_msp=4.00
H BotPHD_K skill=112545 Superior healing first=1920 time=0 dur=0 radius=0 msp=320 cast_s=1.5 recast_s=0.1 heal_instant=1920 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=6.00
H BotPHD_K skill=112548 Superior restore first=0 time=2500 dur=30 radius=0 msp=625 cast_s=1.5 recast_s=0.1 heal_instant=0 hot_tick=166 ticks=15 hot_total=2490 heal_per_msp=3.98
H BotPHD_K skill=112554 Complete healing first=10000 time=0 dur=0 radius=0 msp=960 cast_s=1.5 recast_s=5.4 heal_instant=10000 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=10.42
H BotPHD_K skill=112557 Group massive healing first=960 time=0 dur=0 radius=30 msp=960 cast_s=1.5 recast_s=5.4 heal_instant=960 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=1.00
H BotPHD_K skill=112560 Group complete healing first=10000 time=0 dur=0 radius=30 msp=1920 cast_s=1.5 recast_s=6.4 heal_instant=10000 hot_tick=0 ticks=0 hot_total=0 heal_per_msp=5.21
H_PCT BotPHB_K skill=112560 Group complete healing def=MF pct_of_max_hp=648.9
H_PCT BotPHB_K skill=112560 Group complete healing def=MI pct_of_max_hp=448.8
H_PCT BotPHB_K skill=112560 Group complete healing def=PHB pct_of_max_hp=286.5
H_PCT BotPHB_K skill=112560 Group complete healing def=PHD pct_of_max_hp=286.5
H_PCT BotPHB_K skill=112560 Group complete healing def=WG pct_of_max_hp=177.0
H_PCT BotPHB_K skill=112560 Group complete healing def=WP pct_of_max_hp=177.0
H_PCT BotPHB_K skill=112554 Complete healing def=MF pct_of_max_hp=648.9
H_PCT BotPHB_K skill=112554 Complete healing def=MI pct_of_max_hp=448.8
H_PCT BotPHB_K skill=112554 Complete healing def=PHB pct_of_max_hp=286.5
H_PCT BotPHB_K skill=112554 Complete healing def=PHD pct_of_max_hp=286.5
H_PCT BotPHB_K skill=112554 Complete healing def=WG pct_of_max_hp=177.0
H_PCT BotPHB_K skill=112554 Complete healing def=WP pct_of_max_hp=177.0
H_PCT BotPHB_K skill=112548 Superior restore def=MF pct_of_max_hp=161.6
H_PCT BotPHB_K skill=112548 Superior restore def=MI pct_of_max_hp=111.8
H_PCT BotPHB_K skill=112548 Superior restore def=PHB pct_of_max_hp=71.3
H_PCT BotPHB_K skill=112548 Superior restore def=PHD pct_of_max_hp=71.3
H_PCT BotPHB_K skill=112548 Superior restore def=WG pct_of_max_hp=44.1
H_PCT BotPHB_K skill=112548 Superior restore def=WP pct_of_max_hp=44.1
H_PCT BotPHD_K skill=112560 Group complete healing def=MF pct_of_max_hp=648.9
H_PCT BotPHD_K skill=112560 Group complete healing def=MI pct_of_max_hp=448.8
H_PCT BotPHD_K skill=112560 Group complete healing def=PHB pct_of_max_hp=286.5
H_PCT BotPHD_K skill=112560 Group complete healing def=PHD pct_of_max_hp=286.5
H_PCT BotPHD_K skill=112560 Group complete healing def=WG pct_of_max_hp=177.0
H_PCT BotPHD_K skill=112560 Group complete healing def=WP pct_of_max_hp=177.0
H_PCT BotPHD_K skill=112554 Complete healing def=MF pct_of_max_hp=648.9
H_PCT BotPHD_K skill=112554 Complete healing def=MI pct_of_max_hp=448.8
H_PCT BotPHD_K skill=112554 Complete healing def=PHB pct_of_max_hp=286.5
H_PCT BotPHD_K skill=112554 Complete healing def=PHD pct_of_max_hp=286.5
H_PCT BotPHD_K skill=112554 Complete healing def=WG pct_of_max_hp=177.0
H_PCT BotPHD_K skill=112554 Complete healing def=WP pct_of_max_hp=177.0
H_PCT BotPHD_K skill=112548 Superior restore def=MF pct_of_max_hp=161.6
H_PCT BotPHD_K skill=112548 Superior restore def=MI pct_of_max_hp=111.8
H_PCT BotPHD_K skill=112548 Superior restore def=PHB pct_of_max_hp=71.3
H_PCT BotPHD_K skill=112548 Superior restore def=PHD pct_of_max_hp=71.3
H_PCT BotPHD_K skill=112548 Superior restore def=WG pct_of_max_hp=44.1
H_PCT BotPHD_K skill=112548 Superior restore def=WP pct_of_max_hp=44.1
== P ==
P cha=247 r=0 staff=no avg=1070.8 doc=1071
P cha=247 r=0 staff=yes avg=1149.5 doc=-
P cha=247 r=100 staff=no avg=780.2 doc=780
P cha=247 r=100 staff=yes avg=858.8 doc=-
P cha=247 r=200 staff=no avg=618.5 doc=619
P cha=247 r=200 staff=yes avg=697.2 doc=-
P cha=200 r=0 staff=no avg=861.5 doc=862
P cha=200 r=0 staff=yes avg=940.2 doc=-
P cha=200 r=100 staff=no avg=626.1 doc=626
P cha=200 r=100 staff=yes avg=704.9 doc=-
P cha=200 r=200 staff=no avg=495.2 doc=495
P cha=200 r=200 staff=yes avg=573.8 doc=-
```

**K5 — P bölümü ve docs/04 karşılaştırması**
- Asa **yokken** ortalama değerler (büyüklük): CHA 247 → R0 `1070.8` (doc 1071), R100 `780.2` (780), R200 `618.5` (619); CHA 200 → R0 `861.5` (862), R100 `626.1` (626), R200 `495.2` (495). Farklar ≤ %0.08'dir; nedeni: docs değerleri tamsayıya yuvarlanmış, model `random` üzerinden **kesin ortalama** (ör. 1070.8) verir; tamsayı kesmeleri ve `/3` adımı nedeniyle ±1 oynar. `±%2` koşulu rahatça sağlanır.
- Asa **varken** (Elixir Staff Damage 111): aynı sırayla `1149.5 / 858.8 / 697.2` ve `940.2 / 704.9 / 573.8`; artış asa terimi +236'nın `/3`'ü kadardır.

**Kabul kriterleri öz-değerlendirmesi**
- K1 ✔ `--selftest` → `selftest OK` (`tdiv` ±, `dmg_min=904`, `dmg_max=1177`, `904<avg<1177`, DoT `-30/10/300`, HoT `53/15/795`).
- K2 ✔ M/H/H_PCT/P eksiksiz, çıkış 0; M'de mage (MF/MI) ve saldırı büyüsü olan warrior (WP, 106725); H'de PHD ve PHB; P'de 12 kombinasyon.
- K3 ✔ P satırlarında asasız değerler docs/04 §4'e ≤ %0.1 yakın; fark nedeni yukarıda.
- K4 ✔ Tablo yukarıda; `HpChangeMagic`'in hasarı değiştirmediği açıkça yazıldı.
- K5 ✔ `grep -n "USERDATA"` tek sorgu (`LIKE 'Bot%'`); `INSERT|UPDATE|DELETE|DROP` boş (grep_exit=1).
- K6 ✔ `git diff --stat main...bot/F1-07` yalnızca `tools/spell-model.py` (+ bu plan dosyası rapor commit'iyle); `file` ASCII, CR yok; rapor commit'inden sonra `git status --short` boş.
- K7 — Claude doğrulayacak (çıktı yeniden çalıştırılır, iki M ve bir H satırı elle).

**Plandan sapmalar ve gerekçeleri**
1. **Direnç toplamı tek kez:** plan §5.2'deki "res[attr] = ekipman + resistance_bonus" ile "total_r = res[attr] + resistance_bonus" ifadeleri çelişiyor (çift sayım olurdu); kod (`:2611-2637` + `:2623`) ve selftest (`item direnci 30, bonus 0 → total_r 30`) gereği `total_r = ekipman_direnci + resistance_bonus` (tek kez) uygulandı.
2. **DoT determinizmi:** `duration_damage = GetMagicDamage(time)` tek bir rastgele örnek olduğundan, model tüm olası `random` değerlerinin **kesin ortalamasını** alır; ardından plan formülü (`int(duration_damage/tickCount)`) uygulanır. Tick miktarının yuvarlama dağılımı ayrıca ortalanmadı (fark ≤ 1 birim/tick).
3. **`to_short` sarması:** C++ `int→int16` dönüşümleri (MSVC) 16 bite sarar; ara değerler aralık dışına çıkarsa model de sarar.
4. **H_PCT kapsamı:** "en büyük 3 skill" ifadesi `first+hot_total` toplamına göre en büyük 3 skill olarak uygulandı (112560, 112554, 112548).

**Açık sorular / bulgular**
1. **Asa koşulu ve `attribute != MAGIC_R`:** Kodda asa terimi `attribute != 4` koşuluyla çıkarılır; yani asa **ateş/buz/yıldırım** büyülerinde hasarı artırır, `MAGIC_R` (attr 4) büyülerinde artırmaz. WP'nin tek saldırı büyüsü 106725 attr 4 olduğundan asa terimi zaten 0'dır (asasız WP'de de aynı).
2. Karus ve El Morad botları stat/ekipman olarak aynı olduğundan M/H/P yalnızca Karus botlarıyla üretildi (plan gereği).
3. Model `[D]`/`[I]` etiketlidir; T-MECH-DMG-03 çalışma zamanı ölçümü (GM ile büyü hasarı) yapılmadan `[V]`'ye yükseltilmez.
4. Mage rakiplerin ateş/buz direnci düşük olduğu için M satırlarında MF/MI hedefleri en yüksek hasarı alır (ör. incineration MF'ye ~980, WP'ye ~953); bu, F4/F8 hedef seçimi için doğrudan girdidir.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
