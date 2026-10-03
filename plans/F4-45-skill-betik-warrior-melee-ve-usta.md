# F4-45: T-MECH-SKILL botla koşusu, dilim 4 — Karus warrior melee, self ve usta skill betiği (`skill_warrior_k`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; ADR-0018 m.9 "T-MECH-SKILL'in botla yeniden koşusu", Ek 21) |
| Branch | `bot/F4-45` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-44 (`bots/config/skill_priest_k_curse.{spec,txt}`, üslup örneği) — `KAPANDI` (merge `0673515`); F4-42 (`tools/skill-script-gen.py`), F4-43 (`tools/skill-check.py` MP hükmü) — `KAPANDI`; F4-37 (`{1, 3}`/`{1, 4}` melee çiftleri), F4-36 (eşya skill'leri), F4-40 (envanter doldurma) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/05` §5 (warrior satırları), §5.1, §8 SK-01, SK-04, SK-05, SK-09 / §9 (T-MECH-SKILL-W-*), `docs/03` MEC-MAG-03, MEC-MAG-11, MEC-MAG-13, MEC-MAG-23, MEC-MAG-24; ADR-0018 m.9, Ek 17..Ek 21 |
| Tahmini büyüklük | S (1 spec + üretilmiş betik; Python/C++ yok) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

F4-42..F4-44 priest skill'lerini botla ölçtü (`docs/05` §9.1..§9.3). `docs/05` §5'teki **warrior çekirdek skill seti** (Type1 melee, self buff'lar, usta skill'ler) hiç botla ölçülmedi; F4-37 yalnızca çift tipli skill'lerin bir kez çalıştığını gösterdi. Bu plan `BotWP_K`'nın tek başına atacağı, ofsetleri `skill-script-gen.py` ile hesaplanmış bir betik (`bots/config/skill_warrior_k.spec` + üretilmiş `.txt`) ekler. Araç ve C++ **değişmez**; gerçek koşu ve `docs/05` §9.4 işlemesi Claude'un çalışma zamanı doğrulamasıdır (K8/K9).

## 2. Bağlam (okunması zorunlu)

- `docs/05` §5 (warrior tablosu), §5.1 (MP: Carving 90, Howling 400; `BotWP_K` azami MP'si modelde ~4438 `[D]`, ölçülmedi), §8 SK-01/SK-09, §9.3 (F4-44 koşusunun biçimi).
- `docs/03` MEC-MAG-24 (çift tipli melee skill: Scream `{1, 4}` sonucu `effected` ve `code` = Type4 süresi; Shock Stun/Exceed Break `{1, 3}` sonucu `effected` ya da `missed` `-104`), MEC-MAG-23 (taş tüketimi), MEC-MAG-11 (menzil).
- `tools/skill-script-gen.py` (`cast <bot> <skill_id> <target bot|self> <cycles 1..20>`; `self` yalnızca `Moral 1` skill'lerde kullanılır, `ActionExecutor.cpp:810-813`) ve `bots/config/skill_priest_k_curse.spec` (üslup örneği).
- **Menzil kuralı (kod, `BotCore/BotCombat.h:141-148` `[D]`):** `MAGIC.Range == 0` (silaha bağlı Type1) ise guard `0 <= distanceField <= weaponRangeField` ister; `BotWP_K`'nın silahı Raptor (`156210000 + Upgrade`, `ITEM.Range 20`) ⇒ `weaponRangeField = 20` (**2,0 m**, `ActionExecutor.cpp:894-897`). Hedefler `BotWP_K`'dan ≤ 2,0 m olmalıdır (K8'de Claude yerleştirir; betik konumlandırmaz).
- Doğrulanmış veri (Claude, 2026-10-03, yerel DB; `MAGIC`: `Msp/CastTime/ReCastTime/Range/Type1/Type2/Moral`; hepsinde `CastTime 0`):

| Skill | Ad | Veri | Hedef / ek tablo |
|---|---|---|---|
| `106001` | sprint | `5/0/60/0/4/0/1` | self; `MAGIC_TYPE4` `BuffType 6`, `Speed 150`, 10 sn |
| `106720` | Outrage | `60/0/91/0/4/0/1` | self; `BuffType 5`, `AttackSpeed 120`, 30 sn |
| `106730` | restoration | `105/0/250/0/3/0/1` | self; `MAGIC_TYPE3` `DirectType 1`, `TimeDamage 750`, 60 sn (HoT) |
| `106525` | Carving | `90/0/5/0/1/0/7` | düşman; `MAGIC_TYPE1` `Type 0` (zar), `Hit 200` |
| `106535` | prick | `120/0/5/0/1/0/7` | düşman; `Type 1` (kesin isabet), `Hit 150` |
| `106545` | Cleave | `150/0/5/0/1/0/7` | düşman; `Type 0` (zar), `Hit 150` |
| `106520` | leg cutting | `84/0/51/0/1/4/7` | düşman, `{1, 4}`; `MAGIC_TYPE4` `BuffType 6`, `Speed 50`, 10 sn |
| `106557` | sword aura | `250/0/1/0/1/0/7` | düşman; `Type 1`, `Hit 100`, `AddDamage 250` |
| `106560` | sword dancing | `300/0/5/0/1/0/7` | düşman; `Type 1`, `Hit 150`, `AddDamage 150` |
| `106570` | Howling Sword | `400/0/8/0/1/0/7` | düşman; `Type 1`, `Hit 200`, `AddDamage 200` |
| `106802` | Scream | `300/0/101/0/1/4/7` | düşman, `{1, 4}`; `UseItem 379063000` (Scream Scroll, tüketilmez) |
| `106815` | Exceed Break | `400/0/254/0/1/3/7` | düşman, `{1, 3}`; `UseItem 379059000` (Stone of Warrior) |
| `106820` | Shock Stun | `250/0/252/0/1/3/7` | düşman, `{1, 3}`; `UseItem 379059000` |

  `MAGIC.Skill`/`SkillLevel`: 1060/1, 1067/20, 1067/30, 1065/25, 35, 45, 20, 57, 60, 70, 1068/2, 15, 20; hepsinde `Etc 0` (quest kilidi yok). `BotWP_K` `strSkill = 00 00 00 00 00 46 00 34 14 00` (`db/002_bot_characters.sql:126`): `[5] = 70` (attack), `[7] = 52` (berserk), `[8] = 20` (master) ⇒ on üç skill de ağaçta açıktır. Çantada (`db/002:208-210`, `db/004` ile korunur): Stone of Warrior `379059000` ×50, Scream Scroll `379063000` ×1 (yuva 18/19). Uygulayıcı bu tabloyu yeniden **doğrulamak zorunda değildir**; §5.2'deki `--check` adımı `MAGIC` ile uyumsuzlukta zaten patlar.
- Hedefler: El Morad warrior'ları `BotWP_E`, `BotWG_E` (azami HP ~5650, `docs/05` §9.3 `[V]`). Betiğin toplam hasarı modelde hedef başına ~2000-2500'dür (`docs/05` §5 model hasarları `[Ö]`); iki hedef de ölmemelidir (K8'de `list` ile denetlenir). `BotMF_E`/`BotMI_E` (azami HP ~1541) **kullanılmaz**: tek vuruşta ölür.
- Sunucu davranışı `[D]` (MEC-MAG-24): `MP` bir kez düşer; `CheckType4Prerequisites` hız debuff'ında (`BuffType 6`) engel koymaz; Type1 zar (`Hit`) başarısızsa sonuç `missed` (`-104`) olabilir ve bu **araç hatası değil bulgudur**; `restoration` hedefte etkin HoT varken yeniden atılamaz (`MagicInstance.cpp:515-522` `[D]`) ⇒ betikte tek atış; Regeneration `106750` aynı HoT olduğundan **konmaz**.

## 3. Kapsam

**Yapılacaklar**

1. `bots/config/skill_warrior_k.spec`: §5.1'deki içerik.
2. `bots/config/skill_warrior_k.txt`: 1'den `skill-script-gen.py`'nin ürettiği betik (elle düzenlenmez).

**Kapsam dışı (yapılmayacak)**

- C++ (`GameServer/`, `BotCore/`, `Tests/`), `.vcxproj`, `tools/skill-check.py`, `tools/skill-script-gen.py`, `db/*`, `docs/`, ADR, senaryo YAML'ı, mevcut `bots/config/*` dosyaları: **değişmez** (docs/ADR'yi Claude yazar).
- Bot konumlandırma, El Morad botların spawn'ı/diriltilmesi, hedefe ≤ 2,0 m yaklaşma: betik yapmaz, K8/K9'da Claude'un işidir. Spec'e `move`/`regene`/`raw` **eklenmez**.
- `BotWG_K` ve diğer sınıflar (rogue, mage, priest): bu dilimde yok. `BotWG_K` kalkan/savunma ağacı (Binding `106630`, provoke `106645`: Type7 kapalı), sacrifice `106660` (`HP ≥ 10000` kuralı kapalı), descent `106650` (Type8 warp; F4-35 ölçüldü).
- Quest kilitli skill'ler (Blooding `106575`, Hell blade `106580`, wall of Iron `106675`, Berserker `106775`, HP Booster `106780`: `Etc 510/511`), `Gain`/`Rise`/`Nimble Wind` (BuffType 7 çakışması), Frenzy `106755` (ağaç 52 < 55), Regeneration `106750`, düşük seviyeli temel skill'ler (`106003..106010`, `106500..106555`'in kalanı): kapsam dışı.
- El Morad warrior betiği, mage/rogue/archer, uçan skill MP beklentisi (MEC-MAG-12), priest kalanı (Judgment, Helis, Elysian Web, buff silme): sonraki dilimler.
- Hasar miktarı ve zar isabet oranı ölçümü (telemetride hedef HP'si yok; yalnızca sonuç paketi `effected`/`missed`/`srv_fail` sayılır): T-MECH-DMG ayrı iştir.
- `skill-check.py` hükümlerini değiştirme.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `bots/config/skill_warrior_k.spec` | yeni | §5.1 |
| `bots/config/skill_warrior_k.txt` | yeni | araç çıktısı (üretilmiş) |

Plan dosyası dahil 3 dosya. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch ve spec

1. `git switch -c bot/F4-45 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (`AGENTS.md` §4; bu plan sunucu açmaz).
2. `bots/config/skill_warrior_k.spec` dosyasını aşağıdaki içerikle **aynen** yaz (ASCII, LF; yorumlar İngilizce). Söz dizimi `skill_priest_k_curse.spec` ile aynıdır; `raw` adımı yoktur, bu yüzden `t0 = 0`:

```
# BotWP_K (Karus warrior, attack tree 70, berserk tree 52, master 20) casts its melee and self skills.
# The two targets (BotWP_E, BotWG_E) must be alive, in zone 71 and within 2.0 m of BotWP_K: Type1 skills with
# MAGIC.Range 0 are bound to the weapon range (Raptor, 20 = 2.0 m). The operator places them first.
# Same-skill steps are adjacent, so the recast gap is measured between neighbouring casts.

# Self buffs (Type4 BuffType 6 speed, BuffType 5 attack speed, Type3 HoT)
cast BotWP_K 106001 self 1
cast BotWP_K 106720 self 1
cast BotWP_K 106730 self 1
# Weapon-bound Type1 attacks, two cycles of the cheap skills, one of the expensive ones
cast BotWP_K 106525 BotWP_E 2
cast BotWP_K 106535 BotWG_E 2
cast BotWP_K 106545 BotWP_E 2
cast BotWP_K 106520 BotWP_E 2
cast BotWP_K 106557 BotWG_E 1
cast BotWP_K 106560 BotWP_E 1
cast BotWP_K 106570 BotWG_E 1
# Master skills (Stone of Warrior / Scream Scroll, dual typed)
cast BotWP_K 106802 BotWG_E 1
cast BotWP_K 106815 BotWP_E 1
cast BotWP_K 106820 BotWG_E 1
```

### 5.2 Betiği üret

`python3 tools/skill-script-gen.py bots/config/skill_warrior_k.spec --out bots/config/skill_warrior_k.txt` (gerçek `MAGIC`, `--magic` verilmeden; sqlcmd yalnızca `MAGIC`'i okur). Beklenen: 13 `cast` + 1 `list` = **14 adım** (`ok: 14 steps, 572 bytes, 16 lines, last offset 140000 ms`); ofsetler (adım ilerlemesi `cycles × max(recast, cast+140, 1000) + 1500`):

```
0 7500 18100 44600 48100 51600 55100 66800 69300 71800 74300 85900 112800 | list 140000
```

(Claude bu çıktıyı 2026-10-03'te gerçek `MAGIC` ile `/tmp`'de ürettiği için yazdı.) Çıktı farklıysa (örn. bir skill'in `MAGIC` verisi değişmişse) **dur** ve Uygulayıcı Raporu'na farkı yaz; spec'i kısaltma, ofsetleri elle düzeltme.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/skill-script-gen.py bots/config/skill_warrior_k.spec --out /tmp/skill_warrior_k.txt` çıkış 0 ve `diff /tmp/skill_warrior_k.txt bots/config/skill_warrior_k.txt` boş.
- [ ] K2: `python3 tools/skill-script-gen.py --check bots/config/skill_warrior_k.txt` çıkış 0; çıktı `ok: 14 steps,` ile başlar ve `last offset 140000 ms` ile biter.
- [ ] K3: `grep -c '^[0-9]* cast BotWP_K' bots/config/skill_warrior_k.txt` = 13; `grep -c '^[0-9]* cast'` = 13 (başka bot yok); `grep -c '^[0-9]* list'` = 1; `grep -c '^[0-9]* \(raw\|pinvite\|paccept\|pot\|move\)'` = 0; ilk satır ofseti `0` (`106001`); `106001`/`106720`/`106730` satırlarının hedefi `self`; `106525`, `106535`, `106545`, `106520` satırlarında çevrim sayısı `2`, `106557`, `106560`, `106570`, `106802`, `106815`, `106820` ve üç buff'ta `1`; hedefler `BotWP_E` ya da `BotWG_E` (`self` olmayan 10 satır; `BotMF_E`/`BotMI_E`/`_K` hedefi yok).
- [ ] K4: ardışık ofset farkları skill'in kendi periyodundan (`cycles × max(recast, 1000) + 1500`) küçük değildir: `awk` ile `106001` 7500, `106720` 10600, `106730` 26500, `106525`/`106535`/`106545` 3500, `106520` 11700, `106557`/`106560`/`106570` 2500, `106802` 11600, `106815` 26900 ms (son adımdan `list`e 27200 ms).
- [ ] K5: `python3 tools/skill-script-gen.py --selftest` hâlâ `selftest: 23 checks, 0 failed` ve `python3 tools/skill-check.py --selftest` son satırı `selftest: N checks, 0 failed` (N ≥ 32); iki araç **değişmedi** (K7).
- [ ] K6: `./tools/build.sh Release` hatasız biter (C++ değişmedi; yeni uyarı yok) ve `./tools/run-tests.sh` `251 tests, 0 failed` (ya da fazlası) ile geçer.
- [ ] K7: `git diff --stat gece/2026-10-02...bot/F4-45` yalnızca `bots/config/skill_warrior_k.spec`, `bots/config/skill_warrior_k.txt` ve plan dosyasını gösterir.
- [ ] K8 (Claude, çalışma zamanı): sunucular açık (`[BOT] ENABLED=1 MAX_BOTS=16 TELEMETRY=decisions`), `BotWP_K`, `BotWP_E`, `BotWG_E` spawn; hepsi canlı (`list`: `hp > 0`) ve zone 71'de; iki El Morad botu `/bot move` ile `BotWP_K`'ya ≤ 1,5 m getirilir ve `list` konumlarından mesafe ≤ 2,0 m doğrulanır; Stone of Warrior ≥ 3 ve Scream Scroll ≥ 1 çantada (`list` ya da `bot-refill.sh` sonrası `stock`); toplam MP maliyeti `5+60+105 + 2×(90+120+150+84) + 250+300+400 + 300+400+250` = **2958** (azami MP ölçülmedi `[A]`; MP yetmezse `no_mana`/`mp_before` telemetrisinden görülür ve Claude MP pot adımı ekleyen yeni plan yazar); betik `Scripts/skill_warrior_k.txt` olarak koşulur.
- [ ] K9 (Claude, çalışma zamanı): `skill-check.py <jsonl> --min-n 1` raporu: 13 skill görünür (başlayan: `106525`/`106535`/`106545`/`106520` 2, diğerleri 1); 14/14 adım zamanında koşar (en geç gecikme < 500 ms); `srv_fail` = 0 beklenir. **Zar skill'lerinde (`106525`, `106545`, `106520`, `106815`, `106820`) `missed` (`-104`) olabilir**: bulgu olarak `docs/05` §9.4'e işlenir (SK-04), araç hatası sayılmaz. Scream `code 7`, leg cutting `code 10` (MEC-MAG-24) beklenir; değilse bulgudur. Stone of Warrior tüketimi (50 → 47 beklenir: Scream + Exceed Break + Shock Stun) çalışma zamanı sonrası `list`/`stock_after` ile bakılır, yoksa `[Ö]`. Sunucular `stop` ile kapatılır, `GameServer.ini` yedekten geri döner.
- Derleme sonucu (`tools/build.sh Release` son satırları) Uygulayıcı Raporu'na yapıştırılır.

## 7. Doğrulama komutları

```bash
python3 tools/skill-script-gen.py bots/config/skill_warrior_k.spec --out /tmp/skill_warrior_k.txt
diff /tmp/skill_warrior_k.txt bots/config/skill_warrior_k.txt
python3 tools/skill-script-gen.py --check bots/config/skill_warrior_k.txt

grep -c '^[0-9]* cast BotWP_K' bots/config/skill_warrior_k.txt            # 13
grep -c '^[0-9]* cast' bots/config/skill_warrior_k.txt                    # 13
grep -c '^[0-9]* list' bots/config/skill_warrior_k.txt                    # 1
grep -c '^[0-9]* \(raw\|pinvite\|paccept\|pot\|move\)' bots/config/skill_warrior_k.txt  # 0
grep '^[0-9]* cast' bots/config/skill_warrior_k.txt | awk '$5!="self" && $5!="BotWP_E" && $5!="BotWG_E"'   # no output
grep '^[0-9]* \(cast\|list\)' bots/config/skill_warrior_k.txt | awk '{ if (NR>1) print prev, $1-last; prev=$4; last=$1 }'
# K4: each printed difference >= the skill's own period (see K4); the last line is Shock Stun -> list (27200)

python3 tools/skill-script-gen.py --selftest
python3 tools/skill-check.py --selftest
./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-45
```

(`list` satırında `$4` boştur; son çıktı satırı `106820`'nin periyodudur. Uygulayıcı çıktıyı Uygulayıcı Raporu'na gerçek hâliyle yapıştırır.)

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. Spec ve üretilmiş betik ASCII, LF; yorumlar İngilizce. Üretilmiş dosya elle düzenlenmez.
- Kişisel veri: yalnızca `MAGIC` okunur (araç zaten böyle); başka tablo sorgulanmaz.
- Betik sınırları (`BotCore/ScriptPlan.h`): ≤ 100 adım, ≤ 8192 bayt, ≤ 128 satır, ≤ 600000 ms; bu betik 14 adım / 572 bayt / 140,0 sn ile içindedir.
- Uygulayıcı §5.1'deki spec'i **aynen** yazar; `--margin-ms`, `--start-gap-ms` ve üretici varsayılanlarını değiştirmez. Çıktı §5.2'deki ofsetlerden farklıysa dur ve sor.
- Menzil: `Range 0` skill'lerde hedef ≤ 2,0 m olmalıdır; hedef uzaksa guard `out_of_range` (`MEC-MAG-11`, `value` = mesafe alanı, `limit` 20) ile reddeder. Bu K9'da bulgu değil **kurulum hatasıdır**: Claude hedefleri yaklaştırıp koşuyu yeniler.
- Sunucu çalıştırma ve gerçek koşu bu plan kapsamında DeepSeek'in işi **değildir**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release` son 10 satır) ve `./tools/run-tests.sh` çıktısı:
- Kabul kriterleri öz-değerlendirme (K1–K7; K8/K9 Claude'un işi):
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:
- `git diff --stat gece/2026-10-02...bot/F4-45` (gerçek çıktı, elle yazma):

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)
