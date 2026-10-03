# F4-44: T-MECH-SKILL botla koşusu, dilim 3 — Karus priest düşman hedefli curse betiği (`skill_priest_k_curse`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; ADR-0018 m.9 "T-MECH-SKILL'in botla yeniden koşusu", Ek 20) |
| Branch | `bot/F4-44` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-43 (`tools/skill-check.py` yenilenmeye dayanıklı MP hükmü, `bots/config/skill_priest_k.{spec,txt}`) — `KAPANDI` (merge `5318a90`); F4-42 (`tools/skill-script-gen.py`) — `KAPANDI`; F4-28 (Type4 tek tipli), F4-29 (alan skill), F4-26 (çift tipli) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/05` §6 (priest curse satırları) / §8 SK-04, SK-05 / §9 (T-MECH-SKILL-P-*), `docs/03` MEC-MAG-15, MEC-MAG-16, MEC-BUF-05; ADR-0018 m.9, Ek 17, Ek 18, Ek 19, Ek 20 |
| Tahmini büyüklük | S (1 spec + üretilmiş betik; Python/C++ yok) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

F4-42/F4-43 priest'in **dost hedefli / kendine / party** skill'lerini botla ölçtü (`docs/05` §9.1, §9.2). Priest'in kalan çekirdek skill'leri **düşman hedeflidir**: Malice, Torment, Parasite, Massive, Slow, Sweep mana (`docs/05` §6). Bu plan o altı skill'i `BotPHD_K`'nın (curse ağacı 62) üç El Morad bota atacağı, ofsetleri `skill-script-gen.py` ile hesaplanmış bir betik (`bots/config/skill_priest_k_curse.spec` + üretilmiş `.txt`) ekler. Araç ve C++ **değişmez**; gerçek koşu ve `docs/05` §9.3 işlemesi Claude'un çalışma zamanı doğrulamasıdır (K9).

## 2. Bağlam (okunması zorunlu)

- `docs/05` §6 (curse satırları), §8 SK-04/SK-05 (debuff başarısı yalnızca sonuç paketinden; yavaşlatma direnci), §9.2 (F4-43 koşusu) ve `docs/03` MEC-MAG-15 (debuff kuralları), MEC-BUF-05 (hız debuff'ında direnç zarı).
- `tools/skill-script-gen.py` (komut satırı ve `cast` söz dizimi: `cast <bot> <skill_id> <target bot|self> <cycles 1..20>`) ve `bots/config/skill_priest_k.spec` (üslup örneği).
- Doğrulanmış veri (Claude, 2026-10-03, yerel DB; `Msp/CastTime/ReCastTime/Range/Type1/Type2/Moral`):

| Skill | Ad | Veri | `MAGIC_TYPE4`/`MAGIC_TYPE3` |
|---|---|---|---|
| `112703` | Malice | `40/15/74/56/4/0/7` | `BuffType 2`, `ACPct 75`, `Duration 150` |
| `112757` | Torment | `150/15/94/56/4/0/10` (**alan**, `Moral` 10) | `BuffType 2`, `Radius 10`, `ACPct 70` |
| `112745` | Parasite | `100/15/74/56/4/0/7` | `BuffType 1`, `MaxHPPct 80` (maks HP %80) |
| `112760` | Massive | `180/15/104/56/4/0/7` | `BuffType 4`, `Attack 80` (saldırı %80) |
| `112724` | Slow | `120/15/74/56/4/0/7` | `BuffType 5` (hız; direnç zarı, MEC-BUF-05), `AttackSpeed 70` |
| `112736` | Sweep mana | `160/15/74/56/3/0/7` | Type3 `DirectType 2`, `FirstDamage -960` (MP düşürür; `MAGIC_TYPE3.Name` sütunu 'Drawbridge Destruction', `MAGIC.EnName` 'Sweep mana': veri satırı `iNum` ile eşleşir) |

  Hepsinin `UseItem 0`, `Etc 0` (quest kilidi yok), `Skill 1127`, `SkillLevel` 3..60; `BotPHD_K` `strSkill = 00 00 00 00 00 3C 00 3E 14 00` (`db/002_bot_characters.sql:128`): 6. bayt Heal 60, **8. bayt Curse 62**, 9. bayt Master 20 ⇒ altısı da ağaçta açıktır; `BotPHB_K`'da curse 0'dır (kullanma). Uygulayıcı bu tabloyu yeniden **doğrulamak zorunda değildir**, ama §5.2'deki `diff`/`--check` adımı `MAGIC` ile uyumsuzlukta zaten patlar.
- Debuff kuralı (kod, `GameServer/MagicInstance.cpp:1752-1757`, `:1770-1789` `[D]`): hedefte aynı `BuffType` varsa debuff eskiyi silip süreyi yeniler ve **başarılı sayılır**; debuff `bBlockingDebuffs` hâli dışında hata üretmez (başarısız debuff hedef listesinden sessizce atlanır). Bu yüzden F4-43'teki gibi "buff başına ayrı hedef" dağıtımı **gerekmez**; Malice ve Torment (ikisi `BuffType 2`) aynı hedefe art arda atılabilir.
- Menzil: `MAGIC.Range 56` ⇒ `CastInRange` `distanceM < 56` (`BotCore/BotCombat.h:143-148`); botlar bu mesafe içinde olmalıdır (betik konumlandırmaz, F4-43 Ek 19 c).

## 3. Kapsam

**Yapılacaklar**

1. `bots/config/skill_priest_k_curse.spec`: §5.1'deki içerik.
2. `bots/config/skill_priest_k_curse.txt`: 1'den `skill-script-gen.py`'nin ürettiği betik (elle düzenlenmez).

**Kapsam dışı (yapılmayacak)**

- C++ (`GameServer/`, `BotCore/`, `Tests/`), `.vcxproj`, `tools/skill-check.py`, `tools/skill-script-gen.py`, `db/*`, `docs/`, ADR, senaryo YAML'ı: **değişmez** (docs/ADR'yi Claude yazar). `skill_priest_k.{spec,txt}` **değişmez**.
- Bot konumlandırma, El Morad botların spawn'ı ve diriltilmesi, ulus/zone kontrolü: betik yapmaz, K9'da Claude'un işidir. Spec'e `move`/`regene` **eklenmez**.
- Judgment `112802` (`UseItem`, silah menzili ≤ 1 m), Helis `112815`, Elysian Web `112825` (dost alan + Stone of Priest), diriltmeler, Counter Curse/Curse Refraction etkileşimi (hedef debuff'ı bloklarsa `bBlockingDebuffs`), Malice'in AC buff'ını silmesi ve Parasite'in HP buff'ını silmesi testleri (buff'lı hedef gerektirir): **F4-45+** (`docs/05` §6'da kalır).
- El Morad priest (`BotPHD_E`) curse betiği, warrior ve mage betikleri, uçan skill MP beklenti düzeltmesi (MEC-MAG-12): sonraki dilimler.
- `skill-check.py` hükümlerini değiştirme (hız debuff'ının direnç sonucu `K9`'da yorumlanır, araç değişmez).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `bots/config/skill_priest_k_curse.spec` | yeni | §5.1 |
| `bots/config/skill_priest_k_curse.txt` | yeni | araç çıktısı (üretilmiş) |

Plan dosyası dahil 3 dosya. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch ve spec

1. `git switch -c bot/F4-44 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (`AGENTS.md` §4; bu plan sunucu açmaz).
2. `bots/config/skill_priest_k_curse.spec` dosyasını aşağıdaki içerikle **aynen** yaz (ASCII, LF; yorumlar İngilizce). `skill-script-gen.py` söz dizimi `skill_priest_k.spec` ile aynıdır; `raw` adımı yoktur (party gerekmez), bu yüzden `t0 = 0`:

```
# BotPHD_K (Karus priest, curse tree 62) casts its enemy-targeted curses on the El Morad bots.
# The three targets must be alive, in zone 71 and within 56 m of BotPHD_K (MAGIC.Range 56); the
# operator gathers them first. A debuff of the same BuffType replaces the earlier one on a target and
# still succeeds (MEC-MAG-15), so the targets are shared by all six skills.
# Same-skill steps are adjacent, so the recast gap is measured between neighbouring casts.

# Malice (AC, BuffType 2)
cast BotPHD_K 112703 BotWP_E 1
cast BotPHD_K 112703 BotWG_E 1
cast BotPHD_K 112703 BotMF_E 1
# Torment (area r=10, aimed at the target bot's position; BuffType 2)
cast BotPHD_K 112757 BotWP_E 1
cast BotPHD_K 112757 BotWG_E 1
cast BotPHD_K 112757 BotMF_E 1
# Parasite (max HP, BuffType 1)
cast BotPHD_K 112745 BotWP_E 1
cast BotPHD_K 112745 BotWG_E 1
cast BotPHD_K 112745 BotMF_E 1
# Massive (attack, BuffType 4)
cast BotPHD_K 112760 BotWP_E 1
cast BotPHD_K 112760 BotWG_E 1
cast BotPHD_K 112760 BotMF_E 1
# Slow (speed, BuffType 5; players may resist, MEC-BUF-05)
cast BotPHD_K 112724 BotWP_E 1
cast BotPHD_K 112724 BotWG_E 1
cast BotPHD_K 112724 BotMF_E 1
# Sweep mana (Type3, drains 960 MP)
cast BotPHD_K 112736 BotWP_E 1
cast BotPHD_K 112736 BotWG_E 1
cast BotPHD_K 112736 BotMF_E 1
```

### 5.2 Betiği üret

`python3 tools/skill-script-gen.py bots/config/skill_priest_k_curse.spec --out bots/config/skill_priest_k_curse.txt` (gerçek `MAGIC`, `--magic` verilmeden; sqlcmd yalnızca `MAGIC`'i okur). Beklenen: 18 `cast` + 1 `list` = **19 adım**; ofsetler (adım ilerlemesi `max(recast, cast+140, 1000) + 1500`):

```
0 8900 17800 26700 37600 48500 59400 68300 77200 86100 98000 109900 121800 130700 139600 148500 157400 166300 | list 175700
```

Çıktı farklıysa (örn. bir skill'in `MAGIC` verisi değişmişse) **dur** ve Uygulayıcı Raporu'na farkı yaz; spec'i kısaltma, ofsetleri elle düzeltme.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/skill-script-gen.py bots/config/skill_priest_k_curse.spec --out /tmp/skill_priest_k_curse.txt` çıkış 0 ve `diff /tmp/skill_priest_k_curse.txt bots/config/skill_priest_k_curse.txt` boş.
- [ ] K2: `python3 tools/skill-script-gen.py --check bots/config/skill_priest_k_curse.txt` çıkış 0; çıktı `ok: 19 steps,` ile başlar ve `last offset 175700 ms` ile biter.
- [ ] K3: `grep -c '^[0-9]* cast BotPHD_K' bots/config/skill_priest_k_curse.txt` = 18; `grep -c '^[0-9]* cast' ...` = 18 (başka bot yok); `grep -c '^[0-9]* list' ...` = 1; her bir skill (`112703`, `112757`, `112745`, `112760`, `112724`, `112736`) için `grep -c " $id Bot"` = 3 ve üç satırın hedefleri sırayla `BotWP_E`, `BotWG_E`, `BotMF_E`; `grep -c '^[0-9]* \(raw\|pinvite\|paccept\|pot\|move\)'` = 0; `112703` satırlarının ilki ofset `0`.
- [ ] K4: Aynı skill'in ardışık satırları arasındaki ofset farkı skill'in recast'inden küçük değildir: `112703`/`112745`/`112724`/`112736` için 8900 ms, `112757` için 10900 ms, `112760` için 11900 ms (`awk` ile gösterilir; ilk skill'in ilk satırı dışında hiçbir fark < `ReCastTime*100`).
- [ ] K5: `python3 tools/skill-script-gen.py --selftest` hâlâ `selftest: 23 checks, 0 failed` ve `python3 tools/skill-check.py --selftest` son satırı `selftest: N checks, 0 failed` (N ≥ 32); iki araç **değişmedi** (`git diff --stat` K7).
- [ ] K6: `./tools/build.sh Release` hatasız biter (C++ değişmedi; yeni uyarı yok) ve `./tools/run-tests.sh` `251 tests, 0 failed` (ya da fazlası) ile geçer.
- [ ] K7: `git diff --stat gece/2026-10-02...bot/F4-44` yalnızca `bots/config/skill_priest_k_curse.spec`, `bots/config/skill_priest_k_curse.txt` ve plan dosyasını gösterir.
- [ ] K8 (Claude, çalışma zamanı): sunucular açık (`[BOT] ENABLED=1 MAX_BOTS=16 TELEMETRY=decisions`), `BotPHD_K` + `BotWP_E`, `BotWG_E`, `BotMF_E` spawn; hepsi canlı ve zone 71'de; üç El Morad botu `BotPHD_K`'nın 56 m içine `/bot move` ile toplanır (hedefler birbirinden ≥ 11 m ayrı olursa Torment tek hedefe vurur, ≤ 10 m olursa üçüne; ikisi de geçerli, K9'a not düşülür); `bot-refill.sh` gerekmez (curse'ler MP dışında tüketim yapmaz; toplam maliyet `3 × (40+150+100+180+120+160)` = 2250 MP, priest azami MP'si ölçülmedi `[A]`, MP yetmezse `mp_before` telemetrisinden görülür ve Claude spec'e MP pot adımı ekleyen yeni bir plan yazar); betik `Scripts/skill_priest_k_curse.txt` olarak koşulur.
- [ ] K9 (Claude, çalışma zamanı): `skill-check.py <jsonl> --min-n 1` raporu: 6 skill görünür; 19/19 adım zamanında koşar (en geç gecikme < 500 ms); her skill için `started` = 3, `srv_fail` = 0 beklenir; **bir debuff `missed`/`no_result` ya da `srv_fail` verirse** (özellikle `112724` Slow'da direnç zarı, MEC-BUF-05) bu bir **bulgu** olarak `docs/05` §9.3'e işlenir (SK-04/SK-05: debuff başarısı yalnızca sonuç paketinden); Sweep mana (`112736`) için hedefin MP düşüşü `docs/05` §9.3'e (telemetride yoksa `[Ö]`) yazılır. Sunucular `stop` ile kapatılır, `GameServer.ini` yedekten geri döner.
- Derleme sonucu (`tools/build.sh Release` son satırları) Uygulayıcı Raporu'na yapıştırılır.

## 7. Doğrulama komutları

```bash
python3 tools/skill-script-gen.py bots/config/skill_priest_k_curse.spec --out /tmp/skill_priest_k_curse.txt
diff /tmp/skill_priest_k_curse.txt bots/config/skill_priest_k_curse.txt
python3 tools/skill-script-gen.py --check bots/config/skill_priest_k_curse.txt

grep -c '^[0-9]* cast BotPHD_K' bots/config/skill_priest_k_curse.txt      # 18
grep -c '^[0-9]* list' bots/config/skill_priest_k_curse.txt               # 1
grep -c '^[0-9]* \(raw\|pinvite\|paccept\|pot\|move\)' bots/config/skill_priest_k_curse.txt  # 0
for id in 112703 112757 112745 112760 112724 112736; do
  printf '%s ' "$id"; grep -c " $id Bot" bots/config/skill_priest_k_curse.txt            # 3 each
done
awk '/ cast /{ if ($4==prev) print $4, $1-last; prev=$4; last=$1 }' bots/config/skill_priest_k_curse.txt
# K4: each printed gap >= 8900 (10900 for 112757, 11900 for 112760)

python3 tools/skill-script-gen.py --selftest
python3 tools/skill-check.py --selftest
./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-44
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. Spec ve üretilmiş betik ASCII, LF; yorumlar İngilizce. Üretilmiş dosya elle düzenlenmez.
- Kişisel veri: yalnızca `MAGIC` okunur (araç zaten böyle); başka tablo sorgulanmaz.
- Betik sınırları (`BotCore/ScriptPlan.h`): ≤ 100 adım, ≤ 8192 bayt, ≤ 128 satır, ≤ 600000 ms; bu betik 19 adım / ~0,8 KB / 175,7 sn ile içindedir.
- Uygulayıcı §5.1'deki spec'i **aynen** yazar; `--margin-ms`, `--start-gap-ms` ve üretici varsayılanlarını değiştirmez. Çıktı §5.2'deki ofsetlerden farklıysa dur ve sor.
- Torment alan skill'idir (`Moral` 10): hedef noktası `cast` hedefindeki botun konumudur (F4-29); hedef botun konumu bilinmiyorsa bot `bad_target`/`out_of_view` ile reddedebilir. Bu bir bulgudur (K9), spec'ten çıkarma nedeni değildir.
- Sunucu çalıştırma ve gerçek koşu bu plan kapsamında DeepSeek'in işi **değildir**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-44` — `<kısa-sha> [F4-44] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-44` @ `<sha>`
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
