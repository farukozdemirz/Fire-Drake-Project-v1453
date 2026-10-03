# F4-48: T-MECH-SKILL botla koşusu, dilim 6 — Karus mage (`BotMF_K`) tek hedefli, uçan ve alan Type3 skill betikleri

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; ADR-0018 m.9 "T-MECH-SKILL'in botla yeniden koşusu", Ek 24) |
| Branch | `bot/F4-48` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-47 (`tools/skill-check.py` uçan Type3 MP beklentisi `2 × Msp`, selftest 48) — `KAPANDI` (merge `2135a0c`); F4-42 (`tools/skill-script-gen.py`), F4-25/F4-30 (uçan ve uçan alan cast), F4-29 (alan cast), F4-26 (çift tipli `{3, 4}`), F4-27 (quest kilitleri), F4-40 (envanter doldurma) — `KAPANDI`; F4-45/F4-46 (`bots/config/skill_*.spec` üslup örneği) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/05` §7 (mage tablosu), §8 SK-01, SK-04, SK-06, SK-09 / §9 (T-MECH-SKILL-M-*), `docs/03` MEC-MAG-03, MEC-MAG-11, MEC-MAG-12, MEC-MAG-13, MEC-MAG-16, MEC-MAG-17; ADR-0018 m.9, Ek 17..Ek 24 |
| Tahmini büyüklük | S (3 spec + 3 üretilmiş betik; Python/C++ yok) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

F4-42..F4-46 priest ve warrior skill'lerini botla ölçtü (`docs/05` §9.1..§9.5); F4-47 mage'in uçan skill'leri için ölçüm aracını hazırladı. `docs/05` §7'deki **mage skill'leri hiç botla ölçülmedi**. Bu plan `BotMF_K`'nın atacağı, ofsetleri `skill-script-gen.py` ile hesaplanmış **üç** betik ekler (`bots/config/skill_mage_k_single.{spec,txt}`, `skill_mage_k_area.{spec,txt}`, `skill_mage_k_area_heavy.{spec,txt}`). Üç betik, çünkü hedefler yalnızca ~5650 HP'lidir (warrior) ve alan skill'leri her ikisine birden vurur: tek betikte toplam model hasarı hedefi öldürürdü (§2). Araç ve C++ **değişmez**; gerçek koşu ve `docs/05` §9.6 işlemesi Claude'un çalışma zamanı doğrulamasıdır (K8/K9).

## 2. Bağlam (okunması zorunlu)

- `docs/05` §7 (mage tablosu), §8 SK-01/SK-04/SK-06/SK-09, §9.5 (F4-46 koşusunun biçimi); `docs/03` MEC-MAG-12 (uçan Type3: MP iki kez), MEC-MAG-13 (çift tipli `{3, 4}`), MEC-MAG-16/17 (alan skill, uçan alan).
- `tools/skill-script-gen.py` (`cast <bot> <skill_id> <target bot|self> <cycles 1..20>`) ve `bots/config/skill_warrior_k.spec` (üslup örneği).
- **Uçma kararı `[D]`** (`BotCore/BotCombat.h:266-268` `IsFlyingCast`, `GameServer/Bot/ActionExecutor.cpp:773`/`:868`): bir skill, `Type1 == 3` ve `MAGIC.FlyingEffect != 0` ise uçandır. `CastTypesSupported` (`BotCombat.h:323-331`) `{3, 0}` ve `{3, 4}` çiftlerini açar; `CastMoralSupported` (`:373-379`) `Moral` 7 (düşman, tek hedef) ve 10 (alan düşman) için doğrudur. Uçan skill'in MP'si `MAGIC_FLYING`'de bir, `MAGIC_EFFECTING`'te bir kez düşer (`CastManaNeed` `:272-275`; `skill-check.py` F4-47'den beri `2 × Msp` bekler).
- **Alan skill'inde hedef noktası `[D]`** (`plans/F4-29-aksiyon-yurutucu-alan-skill.md` §5): `cast <bot> <skill> <hedef bot>` komutundaki hedef botun konumu hedef noktasıdır; paketin hedef kimliği `-1`'dir. Hedef noktası çağırana `MAGIC.Range` içinde olmalıdır (guard `meters < sRange`); kurban başına sunucu ayrıca çağıran–kurban mesafesini `sRange` ile sınar (`GameServer/MagicInstance.cpp:1337-1339`). Aynı yarıçap içinde duran her düşman vurulur.
- `UseStanding` sütununda `110570`, `110571` için **53** değeri görünür (yerel DB; veri tuhaflığı). Bot yalnızca `UseStanding == 1`'i ayakta-şart sayar (`ActionExecutor.cpp:940`) ve sunucu da yalnızca `== 1` ile ayakta ister (`MagicInstance.cpp:306`, `:328`); `53` hiçbirini tetiklemez, ek olarak sunucu bu skill'lerde menzil denetimini (`:353`, `sUseStanding == 0`) **atlar** ama bot guard'ı menzili yine uygular. Bu bir bulgu değil bilgi notudur; uygulayıcı bu satırı yeniden doğrulamak zorunda değildir.
- Doğrulanmış veri (Claude, 2026-10-03, yerel DB; `MAGIC`: `Msp/CastTime/ReCastTime/Range/Moral/FlyingEffect/Etc`, `Type1/Type2`; `MAGIC_TYPE3` `FirstDamage`, `TimeDamage`, `Radius`):

| Skill | Ad | Msp | Cast/Recast (0,1 sn) | Menzil (m) | Tür | Uçan | Moral | Hasar (model) |
|---|---|---|---|---|---|---|---|---|
| `110503` | Burn | 20 | 10 / 1 | **11** | `{3, 0}` | hayır | 7 | −168 |
| `110539` | Hell fire | 150 | 15 / 43 | 56 | `{3, 0}` | hayır | 7 | −480, DoT −1120 (20 sn) |
| `110551` | Pillar of fire | 160 | 15 / 53 | 56 | `{3, 0}` | hayır | 7 | −1260 |
| `110570` | incineration | 390 | 11 / 213 | 45 | `{3, 0}` | hayır | 7 | −2500 |
| `110651` | Ice comet | 160 | 15 / 53 | 56 | `{3, 4}` | hayır | 7 | −882 + hız (`MAGIC_TYPE4` Speed 32, 19 sn) |
| `110515` | Fire ball | 50 | 15 / 43 | 78 | `{3, 0}` | **evet** (191) | 7 | −308 |
| `110527` | Fire spear | 80 | 15 / 43 | 78 | `{3, 0}` | **evet** (192) | 7 | −588 |
| `110615` | Ice arrow | 50 | 15 / 43 | 78 | `{3, 4}` | **evet** (291) | 7 | −216 + hız (Speed 46, 12 sn) |
| `110533` | Fire burst | 150 | 15 / 1 | 90 | `{3, 0}` | **evet** (191) | **10** (r = 8) | −588 |
| `110633` | Ice burst | 150 | 15 / 1 | 90 | `{3, 4}` | **evet** (291) | **10** (r = 8) | −412 + hız (`MAGIC_TYPE4` r 5, Speed 40, 15 sn) |
| `110545` | Inferno | 200 | 15 / 153 | 56 | `{3, 0}` | hayır | **10** (r = 15) | −504 |
| `110645` | Blizzard | 200 | 15 / 153 | 56 | `{3, 4}` | hayır | **10** (r = 15) | −353 + hız (r 15, Speed 34, 18 sn) |
| `110560` | Supernova | 400 | 15 / 153 | 56 | `{3, 0}` | hayır | **10** (r = 15) | −1800, DoT −600 (20 sn) |
| `110571` | meteor Fall | 600 | 13 / 183 | 45 | `{3, 0}` | hayır | **10** (r = 15) | −2100, DoT −600 (20 sn) |

  Hepsinde `Etc 0` (quest kilidi yok), `UseItem 0` (eşya gerekmez). `BotMF_K` `strSkill = 00 00 00 00 00 46 34 00 14 00` (`db/002_bot_characters.sql:130`): `[5] = 70` ateş, `[6] = 52` buz, `[8] = 20` usta ⇒ 14 skill'in hepsi ağaçta açıktır (en yüksek gereksinimler: ateş 70, buz 51 (`110651`) ve 45 (`110645`); Ice comet `110651` buz 51 ≤ 52). Uygulayıcı bu tabloyu yeniden **doğrulamak zorunda değildir**; §5.2'deki `--check` adımı `MAGIC` ile uyumsuzlukta zaten patlar.
- DoT toplamı `TimeDamage` kadardır (`MagicInstance.cpp:1536-1565`, tüm süreye yayılır). Model toplam hasar, ikisi de alan içindeyken **her** warrior başına: `single` betiği WP ≈ 4442 / WG ≈ 3976 (hedefler dönüşümlü); `area` ≈ 2445; `area_heavy` ≈ 5100 (1800 + 600 + 2100 + 600). Warrior azami HP'si ~5650 (`docs/05` §9.3 `[V]`); bu yüzden üç ayrı betik ve her betikten önce HP tazelemesi (K8). `BotMF_E`/`BotMI_E` (azami HP ~1541) **hedef olmaz**.
- Menzil: tek hedefli skill'lerin en kısası Burn (11 m). Hedefler `BotMF_K`'dan **< 10 m** olur; alan betiklerinde iki warrior birbirinden **≤ 6 m** (Fire burst r = 8 en küçük yarıçap). Yerleştirme K8'de Claude'un işidir (betik konumlandırmaz).
- Sunucu davranışı `[D]`: `Type3` zar kullanır (`missed` olabilir; `MAGIC_TYPE3` isabeti sunucu hesabıdır). `missed`/`srv_fail` araç hatası değil bulgudur (SK-04).

## 3. Kapsam

**Yapılacaklar**

1. `bots/config/skill_mage_k_single.spec` + üretilmiş `.txt`: tek hedefli, uçmayan ve uçan, tek ve çift tipli skill'ler (§5.1).
2. `bots/config/skill_mage_k_area.spec` + üretilmiş `.txt`: hafif alan skill'leri (§5.2).
3. `bots/config/skill_mage_k_area_heavy.spec` + üretilmiş `.txt`: Supernova ve meteor Fall (§5.3).

**Kapsam dışı (yapılmayacak)**

- C++ (`GameServer/`, `BotCore/`, `Tests/`), `.vcxproj`, `tools/*`, `db/*`, `docs/`, ADR, senaryo YAML'ı, mevcut `bots/config/*` dosyaları: **değişmez** (docs/ADR'yi Claude yazar).
- Eşyalı ve quest kilitli skill'ler: Fire Impact `110557` (`UseItem 379070000`), Fire Thorn `110554` (`BeforeAction 3`, `UseItem 379069000`), Fire blast `110535`, Absolute power `110802` (`UseItem 379065000`), Minor Resist `110825` (`UseItem 379061000`), Igzination `110575`/Vampiric Fire `110574` (`Etc 517`), Fire Staff `110572` ve silaha bağlı `Type1` skill'leri: sonraki dilim.
- Self buff'lar (Resist/Endure/Immunity, Frozen armor/shell/Ice barrier `110612/630/654`, Mana Shield `110815`, Instantly Magic `110820`), `summon friend`, Gate: sonraki dilim; ölçülecek hedef/etkileşim kurulumu ister (buff çakışmaları, `docs/05` §4).
- Ice tree 70 skill'leri (Frost nova `110660` buz 60, Prismatic `110670`, ice storm `110671`): `BotMF_K` buzu 52'dir; `BotMI_K` betiği ayrı dilim.
- Mage'in lightning ağacı (`BotMF_K` `[7] = 0`): kapalı.
- El Morad mage betiği, rogue, archer, Elysian Web `Moral 11`: sonraki dilimler.
- Bot konumlandırma, hedefleri toplama/HP tazeleme, hedefe yaklaşma: betik yapmaz, K8'de Claude'un işidir. Spec'e `move`/`regene`/`raw`/`pot` **eklenmez**.
- Hasar miktarı ölçümü (telemetride hedef HP'si yok; T-MECH-DMG ayrı iştir), hız düşüşünün (Type4 Speed) gözlemlenmesi (algı tablosu yok, F4-53 `TASLAK`), alan kurban sayısının hükme bağlanması (`ACTION_RESULT.victims` alanı gözlemdir).
- `skill-check.py` ve `skill-script-gen.py` hükümlerini/varsayılanlarını değiştirme.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `bots/config/skill_mage_k_single.spec` | yeni | §5.1 |
| `bots/config/skill_mage_k_single.txt` | yeni | araç çıktısı (üretilmiş) |
| `bots/config/skill_mage_k_area.spec` | yeni | §5.2 |
| `bots/config/skill_mage_k_area.txt` | yeni | araç çıktısı (üretilmiş) |
| `bots/config/skill_mage_k_area_heavy.spec` | yeni | §5.3 |
| `bots/config/skill_mage_k_area_heavy.txt` | yeni | araç çıktısı (üretilmiş) |

Plan dosyası dahil 7 dosya (hepsi veri; araç/kod yok). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.0 Branch

1. `git switch -c bot/F4-48 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (`AGENTS.md` §4; bu plan sunucu açmaz).

### 5.1 `skill_mage_k_single`

2. `bots/config/skill_mage_k_single.spec` dosyasını aşağıdaki içerikle **aynen** yaz (ASCII, LF; yorumlar İngilizce). Söz dizimi `skill_warrior_k.spec` ile aynıdır; `raw` adımı yoktur, bu yüzden `t0 = 0`:

```
# BotMF_K (Karus mage, fire tree 70, ice tree 52, master 20) casts its single-target Type3 skills on the El Morad warriors.
# Both targets (BotWP_E, BotWG_E) must be alive, in zone 71 and closer than 10 m to BotMF_K (Burn has MAGIC.Range 11, the
# nearest limit; the others reach 45..78 m). The operator places them first.
# Same-skill steps are adjacent, so the recast gap is measured between neighbouring casts.
# The targets alternate so that neither warrior (about 5650 HP) takes the whole model damage.

# Not flying (single-typed Type3: Burn, Hell fire with a damage over time, Pillar of fire, incineration)
cast BotMF_K 110503 BotWP_E 1
cast BotMF_K 110539 BotWP_E 1
cast BotMF_K 110551 BotWG_E 1
cast BotMF_K 110570 BotWG_E 1
# Not flying, dual-typed {3, 4} (Ice comet: damage and slow)
cast BotMF_K 110651 BotWP_E 1
# Flying single-typed (Fire ball, Fire spear: MP is charged at FLYING and again at EFFECTING)
cast BotMF_K 110515 BotWP_E 2
cast BotMF_K 110527 BotWP_E 2
# Flying dual-typed {3, 4} (Ice arrow)
cast BotMF_K 110615 BotWG_E 1
```

3. `python3 tools/skill-script-gen.py bots/config/skill_mage_k_single.spec --out bots/config/skill_mage_k_single.txt` (gerçek `MAGIC`, `--magic` verilmeden; sqlcmd yalnızca `MAGIC`'i okur). Beklenen: 8 `cast` + 1 `list` = **9 adım** (`ok: 9 steps, 402 bytes, 11 lines, last offset 71420 ms`); ofsetler (adım ilerlemesi `cycles × max(recast, cast+140, 1000) + 1500`, son adımdan `list`e +500):

```
0 2720 8520 15320 38120 44920 55020 65120 | list 71420
```

### 5.2 `skill_mage_k_area`

4. `bots/config/skill_mage_k_area.spec` dosyasını **aynen** yaz:

```
# BotMF_K (Karus mage, fire tree 70, ice tree 52, master 20) casts its cheap area skills (Moral 10) at El Morad warriors.
# The aim point of an area skill is the position of the named target bot (a single-target step names a bot, not a point).
# Both targets (BotWP_E, BotWG_E) must be alive, in zone 71, closer than 10 m to BotMF_K and within 6 m of each other so that
# both stand inside the smallest radius (Fire burst r = 8). The operator places them first.
# Same-skill steps are adjacent, so the recast gap is measured between neighbouring casts.

# Flying area, single-typed (Fire burst, r = 8: MP is charged at FLYING and again at EFFECTING)
cast BotMF_K 110533 BotWP_E 2
# Flying area, dual-typed {3, 4} (Ice burst, r = 8)
cast BotMF_K 110633 BotWG_E 1
# Not flying area (Inferno r = 15; Blizzard r = 15, dual-typed {3, 4})
cast BotMF_K 110545 BotWP_E 1
cast BotMF_K 110645 BotWG_E 1
```

5. `python3 tools/skill-script-gen.py bots/config/skill_mage_k_area.spec --out bots/config/skill_mage_k_area.txt`. Beklenen: 4 `cast` + 1 `list` = **5 adım** (`ok: 5 steps, 256 bytes, 7 lines, last offset 42260 ms`); ofsetler:

```
0 4940 8160 24960 | list 42260
```

### 5.3 `skill_mage_k_area_heavy`

6. `bots/config/skill_mage_k_area_heavy.spec` dosyasını **aynen** yaz:

```
# BotMF_K (Karus mage, fire tree 70, ice tree 52, master 20) casts its two big area skills (Moral 10) at El Morad warriors.
# Supernova and meteor Fall deal 1800 / 2100 plus a damage over time to every victim, so they run in their own script and
# the operator refreshes the warriors' HP first. Both targets (BotWP_E, BotWG_E) must be alive, in zone 71, closer than 10 m
# to BotMF_K and within 6 m of each other (both inside the radius, r = 15). The operator places them first.

# Not flying area, single-typed with a damage over time
cast BotMF_K 110560 BotWP_E 1
cast BotMF_K 110571 BotWG_E 1
```

7. `python3 tools/skill-script-gen.py bots/config/skill_mage_k_area_heavy.spec --out bots/config/skill_mage_k_area_heavy.txt`. Beklenen: 2 `cast` + 1 `list` = **3 adım** (`ok: 3 steps, 192 bytes, 5 lines, last offset 37100 ms`); ofsetler:

```
0 16800 | list 37100
```

(Claude bu çıktıları 2026-10-03'te gerçek `MAGIC` ile aynı dosya adlarıyla `/tmp`'de ürettiği için yazdı; üretilen dosyanın ilk satır yorumu spec dosya adını içerir, bayt sayısı bu yüzden spec adına bağlıdır.) Bir çıktı farklıysa (örn. bir skill'in `MAGIC` verisi değişmişse) **dur** ve Uygulayıcı Raporu'na farkı yaz; spec'leri kısaltma, ofsetleri elle düzeltme.

## 6. Kabul kriterleri

- [ ] K1: üç spec için `python3 tools/skill-script-gen.py bots/config/<ad>.spec --out /tmp/<ad>.txt` çıkış 0 ve `diff /tmp/<ad>.txt bots/config/<ad>.txt` boş (`<ad>` = `skill_mage_k_single`, `skill_mage_k_area`, `skill_mage_k_area_heavy`).
- [ ] K2: `python3 tools/skill-script-gen.py --check bots/config/<ad>.txt` üçü için çıkış 0 ve çıktılar sırasıyla `ok: 9 steps, 402 bytes, 11 lines, last offset 71420 ms`, `ok: 5 steps, 256 bytes, 7 lines, last offset 42260 ms`, `ok: 3 steps, 192 bytes, 5 lines, last offset 37100 ms`.
- [ ] K3: `grep -c '^[0-9]* cast BotMF_K' bots/config/<ad>.txt` = 8, 4, 2 (sırasıyla); `grep -c '^[0-9]* cast'` aynı sayılar (başka bot yok); `grep -c '^[0-9]* list'` = 1; `grep -c '^[0-9]* \(raw\|pinvite\|paccept\|pot\|move\)'` = 0; her betikte ilk satır ofseti `0`; hedef olarak yalnızca `BotWP_E`/`BotWG_E` geçer (`self`, `_K`, `BotMF_E`, `BotMI_E` yok).
- [ ] K4: skill kimlikleri ve sırası: `single` = `110503 110539 110551 110570 110651 110515 110527 110615`, `area` = `110533 110633 110545 110645`, `area_heavy` = `110560 110571`; yasak kimlikler (`110557 110554 110535 110802 110825 110575 110574 110572 110660 110670 110671`) üç betikte de **geçmez** (`grep -c` = 0).
- [ ] K5: ardışık ofset farkları (`awk`, §7) `single` için `2720 5800 6800 22800 6800 10100 10100 6300`, `area` için `4940 3220 16800 17300`, `area_heavy` için `16800 20300`.
- [ ] K6: `python3 tools/skill-script-gen.py --selftest` hâlâ `selftest: 23 checks, 0 failed` ve `python3 tools/skill-check.py --selftest` `selftest: 48 checks, 0 failed`; iki araç **değişmedi** (K8).
- [ ] K7: `./tools/build.sh Release` hatasız biter (C++ değişmedi; yeni uyarı yok) ve `./tools/run-tests.sh` `251 tests, 0 failed` (ya da fazlası) ile geçer.
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F4-48` yalnızca §4'teki 6 dosyayı ve plan dosyasını gösterir (`tools`, `BotCore`, `GameServer`, `shared`, `AIServer`, `Tests`, `docs`, `db` yok).
- [ ] K9 (Claude, çalışma zamanı): sunucular açık (`[BOT] ENABLED=1 MAX_BOTS=16 TELEMETRY=decisions`), `BotMF_K`, `BotWP_E`, `BotWG_E` spawn; hepsi canlı (`list`: `hp > 0`) ve zone 71'de; hedefler `BotMF_K`'dan < 10 m ve (alan betikleri için) birbirinden ≤ 6 m'ye `/bot move` ile getirilir, mesafe `list` konumlarından doğrulanır (kurulamazsa guard `out_of_range` verir: kurulum hatasıdır, bulgu değildir); her betikten önce iki warrior'ın HP'si `pot <bot> 389015000 8` ile tazelenir (ölürlerse `regene`), `BotMF_K` MP'si `list` ile denetlenir (toplam MP maliyeti `single` 1500, `area` 1300, `area_heavy` 1000; mage azami MP'si ölçülmedi `[A]`, MP yetmezse `no_mana` telemetrisinden görülür ve Claude MP bekleme/pot ekleyen yeni plan yazar); betikler `Scripts/<ad>.txt` olarak sırayla koşulur.
- [ ] K10 (Claude, çalışma zamanı): her betik için `skill-check.py <jsonl> --min-n 1` raporu: tüm skill'ler görünür; adımların tamamı zamanında koşar (en geç gecikme < 500 ms); uçan skill'lerde (`110515`, `110527`, `110615`, `110533`, `110633`) `fly_n` > 0 ve MP beklentisi `2 × Msp` (F4-47), uçmayanlarda tek `Msp`; `srv_fail`/`missed` bulgu olarak `docs/05` §9.6'ya işlenir (alan skill'lerinde `ACTION_RESULT.victims` yalnızca gözlemdir). `Ice burst` (uçan alan `{3, 4}`), bu kombinasyon daha önce hiç koşulmadığı için bir `unsupported_skill`/`bad_*` çıkarsa **bulgu** (kod diliminin girdisi), hata değil. Sunucular `stop` ile kapatılır, `GameServer.ini` yedekten geri döner.
- Derleme sonucu (`tools/build.sh Release` son satırları) Uygulayıcı Raporu'na yapıştırılır.

## 7. Doğrulama komutları

```bash
for n in skill_mage_k_single skill_mage_k_area skill_mage_k_area_heavy; do
  python3 tools/skill-script-gen.py bots/config/$n.spec --out /tmp/$n.txt
  diff /tmp/$n.txt bots/config/$n.txt
  python3 tools/skill-script-gen.py --check bots/config/$n.txt
done

grep -c '^[0-9]* cast BotMF_K' bots/config/skill_mage_k_single.txt        # 8
grep -c '^[0-9]* cast BotMF_K' bots/config/skill_mage_k_area.txt          # 4
grep -c '^[0-9]* cast BotMF_K' bots/config/skill_mage_k_area_heavy.txt    # 2
grep -c '^[0-9]* \(raw\|pinvite\|paccept\|pot\|move\)' bots/config/skill_mage_k_*.txt   # 0 for each file
grep -h '^[0-9]* cast' bots/config/skill_mage_k_*.txt | awk '$5!="BotWP_E" && $5!="BotWG_E"'   # no output
grep -c '11055[47]\|110535\|110802\|110825\|11057[245]\|11066[0]\|110670\|110671' bots/config/skill_mage_k_*.txt   # 0 for each file
for n in skill_mage_k_single skill_mage_k_area skill_mage_k_area_heavy; do
  grep '^[0-9]* \(cast\|list\)' bots/config/$n.txt | awk '{ if (NR>1) printf "%d ", $1-last; last=$1 } END { print "" }'
done
# K5: 2720 5800 6800 22800 6800 10100 10100 6300 / 4940 3220 16800 17300 / 16800 20300

python3 tools/skill-script-gen.py --selftest
python3 tools/skill-check.py --selftest
./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-48
```

(Uygulayıcı çıktıyı Uygulayıcı Raporu'na gerçek hâliyle yapıştırır. `grep -c` çok dosyada dosya başına sayı yazar; hepsi 0 olmalıdır.)

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. Spec ve üretilmiş betikler ASCII, LF; yorumlar İngilizce. Üretilmiş dosyalar elle düzenlenmez.
- Kişisel veri: yalnızca `MAGIC` okunur (araç zaten böyle); başka tablo sorgulanmaz.
- Betik sınırları (`BotCore/ScriptPlan.h`): ≤ 100 adım, ≤ 8192 bayt, ≤ 128 satır, ≤ 600000 ms; en büyük betik 9 adım / 402 bayt / 71,4 sn ile içindedir.
- Uygulayıcı §5'teki spec'leri **aynen** yazar; `--margin-ms`, `--start-gap-ms` ve üretici varsayılanlarını değiştirmez. Çıktı beklenen ofsetlerden farklıysa dur ve sor.
- Menzil: Burn `Range 11`: hedef ≥ 11 m ise guard `out_of_range` ile reddeder (MEC-MAG-11); bu K10'da bulgu değil **kurulum hatasıdır**, Claude hedefleri yaklaştırıp koşuyu yeniler.
- Hasar: bir hedef betik sırasında ölürse sonraki alan cast'leri onu kurban saymaz (ve hedef-nokta bot ölüyse reddedilebilir); bu bulgu değil kurulum notudur, Claude HP tazeleyip koşuyu yeniler. Betiği kısaltma, cycle sayısını düşürme.
- Sunucu çalıştırma ve gerçek koşu bu plan kapsamında DeepSeek'in işi **değildir**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-48` — `994d148 [F4-48] Karus mage tek hedefli/uçan/alan skill betikleri`
- Değişen dosyalar ve neden:
  - `bots/config/skill_mage_k_single.spec` + `.txt` (yeni): §5.1 tek hedefli, uçmayan/uçan, tek/çift tipli 8 cast.
  - `bots/config/skill_mage_k_area.spec` + `.txt` (yeni): §5.2 hafif alan 4 cast.
  - `bots/config/skill_mage_k_area_heavy.spec` + `.txt` (yeni): §5.3 Supernova + meteor Fall 2 cast.
  - `plans/F4-48-…md`: `Durum` satırı `UYGULANIYOR` + bu Uygulayıcı Raporu.
  - Üretilmiş `.txt` dosyaları `tools/skill-script-gen.py` ile spec'lerden üretildi, elle düzenlenmedi.
- Derleme sonucu (`tools/build.sh Release` son satırlar) ve `./tools/run-tests.sh` son satırı:
  ```
  BotCore.vcxproj -> ...\build\bin\x86-Release\libs\BotCore.lib
  Lua.vcxproj -> ...\build\bin\x86-Release\libs\Lua.lib
  shared.vcxproj -> ...\build\bin\x86-Release\libs\shared.lib
  proj-LogInServer.vcxproj -> ...\build\bin\x86-Release\Server\LogInServer.exe
  proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> ...\build\bin\x86-Release\Server\AIServer.exe
  BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  251 tests, 0 failed
  ```
- Kabul kriterleri öz-değerlendirme (K1–K8; K9/K10 Claude'un işi):
  - K1 ✔: üç spec için `--out /tmp/<ad>.txt` çıkış 0 ve `diff` boş (üçü de EMPTY).
  - K2 ✔: `--check` üçü için sırasıyla `ok: 9 steps, 402 bytes, 11 lines, last offset 71420 ms`, `ok: 5 steps, 256 bytes, 7 lines, last offset 42260 ms`, `ok: 3 steps, 192 bytes, 5 lines, last offset 37100 ms`.
  - K3 ✔: `cast BotMF_K` = 8/4/2, `cast` toplamı aynı, `list` = 1, yasak verb = 0, ilk ofset = 0; hedef alanı yalnızca `BotWP_E`/`BotWG_E` (awk süzgeci boş çıktı).
  - K4 ✔: kimlikler ve sıra planla birebir (`110503 110539 110551 110570 110651 110515 110527 110615` / `110533 110633 110545 110645` / `110560 110571`); yasak kimlikler 0/0/0.
  - K5 ✔: ofset farkları `2720 5800 6800 22800 6800 10100 10100 6300` / `4940 3220 16800 17300` / `16800 20300`.
  - K6 ✔: `skill-script-gen.py --selftest` 23/0, `skill-check.py --selftest` 48/0; iki araç değişmedi.
  - K7 ✔: `build.sh Release` hatasız (yeni uyarı yok), `run-tests.sh` `251 tests, 0 failed`.
  - K8 ✔: `git diff --stat gece/2026-10-02...bot/F4-48` yalnızca §4'teki 6 veri dosyası + plan dosyası; `tools`/`BotCore`/`GameServer`/`shared`/`AIServer`/`Tests`/`docs`/`db` yok.
- Plandan sapmalar ve gerekçeleri: Yok. §5'teki spec'ler aynen yazıldı; `--margin-ms`/`--start-gap-ms` değiştirilmedi; üretilen çıktılar plan bekleyişiyle birebir.
- Açık sorular: Yok.
- `git diff --stat gece/2026-10-02...bot/F4-48` (gerçek çıktı):
  ```
   bots/config/skill_mage_k_area.spec                     | 13 +++++++++++++
   bots/config/skill_mage_k_area.txt                      |  7 +++++++
   bots/config/skill_mage_k_area_heavy.spec               |  8 ++++++++
   bots/config/skill_mage_k_area_heavy.txt                |  5 +++++
   bots/config/skill_mage_k_single.spec                   | 18 ++++++++++++++++++
   bots/config/skill_mage_k_single.txt                    | 11 +++++++++++
   .../F4-48-skill-betik-mage-karus-tek-hedef-ve-alan.md  |  2 +-
   7 files changed, 63 insertions(+), 1 deletion(-)
  ```

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-48` @ `<sha>`
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ / ✘ | komut çıktısı |

- Bulgular (önem sırasıyla):
  1. …
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
…
```
