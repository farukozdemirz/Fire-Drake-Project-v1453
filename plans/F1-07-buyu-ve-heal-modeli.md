# F1-07: Büyü hasarı ve heal modeli (`tools/spell-model.py`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
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

### Tur 1

- Durum: —
- Branch / commit'ler: —
- Değişen dosyalar ve neden: —
- Kabul kriterleri öz-değerlendirme: —
- Plandan sapmalar ve gerekçeleri: —
- Açık sorular: —

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
