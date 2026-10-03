# F4-46: T-MECH-SKILL botla koşusu, dilim 5 — Karus priest silaha bağlı usta skill'leri (`skill_priest_k_master`: Judgment, Helis)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02, merge `f444da0`) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; ADR-0018 m.9 "T-MECH-SKILL'in botla yeniden koşusu", Ek 22) |
| Branch | `bot/F4-46` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-45 (`bots/config/skill_warrior_k.{spec,txt}`, silaha bağlı Type1 üslup örneği) — `KAPANDI` (merge `5d2ce2e`); F4-42 (`tools/skill-script-gen.py`), F4-43 (`tools/skill-check.py` MP hükmü) — `KAPANDI`; F4-36 (eşya/sınıf taşı skill'leri, `no_item`), F4-40 (envanter doldurma) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/05` §6 (priest tablosu, son satır Judgment/Helis), §8 SK-01, SK-04, SK-09 / §9 (T-MECH-SKILL-P-*), `docs/03` MEC-MAG-03, MEC-MAG-11, MEC-MAG-23; ADR-0018 m.9, Ek 17..Ek 22 |
| Tahmini büyüklük | S (1 spec + üretilmiş betik; Python/C++ yok) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

F4-42..F4-44 priest'in heal/cure/buff ve curse skill'lerini, F4-45 warrior'ı botla ölçtü (`docs/05` §9.1..§9.4). `docs/05` §6'daki priest tablosunun **silaha bağlı usta saldırıları** (Judgment `112802`, Helis `112815`) hiç botla ölçülmedi (F4-36 yalnızca Judgment'ın `no_item` kapısını koda ekledi). Bu plan `BotPHD_K`'nın tek başına atacağı, ofsetleri `skill-script-gen.py` ile hesaplanmış bir betik (`bots/config/skill_priest_k_master.spec` + üretilmiş `.txt`) ekler. Araç ve C++ **değişmez**; gerçek koşu ve `docs/05` §9.5 işlemesi Claude'un çalışma zamanı doğrulamasıdır (K8/K9).

## 2. Bağlam (okunması zorunlu)

- `docs/05` §6 (son satır: Judgment/Helis `Master 2/12`, 200/350 MP, "priest'in kendini savunması"), §8 SK-01/SK-09, §9.4 (F4-45 koşusunun biçimi).
- `docs/03` MEC-MAG-11 (menzil), MEC-MAG-23 (taş/scroll tüketimi).
- `tools/skill-script-gen.py` (`cast <bot> <skill_id> <target bot|self> <cycles 1..20>`) ve `bots/config/skill_warrior_k.spec` (üslup örneği: silaha bağlı skill'ler, hedef mesafesi uyarısı).
- **Menzil kuralı (kod, `BotCore/BotCombat.h:143-150` `[D]`):** `MAGIC.Range == 0` (silaha bağlı Type1) ise guard `0 <= distanceField <= weaponRangeField` ister. `weaponRangeField` botun sağ elindeki silahın `ITEM.Range`'idir (`GameServer/Bot/ActionExecutor.cpp:894-897`, `BotCore/BotCombat.h:34-37`). Priest profili sağ el `191110000 + Upgrade` (`db/002_bot_characters.sql:200-201`); gerçek `ITEM`'da `191110000`, `191110007`, `191110008` için `Range = 10` ⇒ `weaponRangeField = 10` (**1,0 m**; warrior'ın Raptor'u 2,0 m'ydi). `distanceField` metrenin ×10 kesilmiş değeridir (`DistanceField`), yani hedef `BotPHD_K`'dan **≤ 0,9 m** olmalıdır (K8'de Claude yerleştirir; betik konumlandırmaz).
- Doğrulanmış veri (Claude, 2026-10-03, yerel DB; `MAGIC`: `Msp/CastTime/ReCastTime/Range/Type1/Type2/Moral/Skill/SkillLevel/UseItem/BeforeAction/Etc/UseStanding`):

| Skill | Ad | Veri | Ek tablo |
|---|---|---|---|
| `112802` | Judgment | `200/0/5/0/1/0/7`, `Skill 1128/2`, `UseItem 379066000`, `BeforeAction 4`, `Etc 0`, `UseStanding 0` | `MAGIC_TYPE1` `Type 1` (kesin isabet), `HitRate 100`, `Hit 500`, `AddDamage 150` |
| `112815` | Helis | `350/0/5/0/1/0/7`, `Skill 1128/12`, `UseItem 0`, `BeforeAction 4`, `Etc 0`, `UseStanding 0` | `MAGIC_TYPE1` `Type 1`, `HitRate 100`, `Hit 400`, `AddDamage 400` (açıklama: savunmayı yok sayar) |

  `BotPHD_K` `strSkill = 00 00 00 00 00 3C 00 3E 14 00` (`db/002:128`): `[8] = 20` (master) ⇒ iki skill de ağaçta açıktır (`SkillLevel` 2 ve 12 ≤ 20). Çantada (`db/002:213`, `db/004` ile korunur): Scroll of Priest `379066000` ×1 (yuva 19, tüketilmez), Stone of Priest `379062000` ×50 (yuva 18).
- **Eşya davranışı `[D]` (`GameServer/MagicInstance.cpp:246-260`, `:2985-3000`):** iki skill de `BeforeAction 4` ⇒ sunucu tüketilecek eşyayı **Stone of Priest** (`379058000 + 4×1000 = 379062000`) yapar; Judgment'ta ayrıca `UseItem 379066000` (scroll) çantada bulunmalı ama tüketilmez (`ConsumeItem` listesi). Helis'te `UseItem 0` olduğundan sunucu eşya **denetlemez** (`pSkill->iUseItem != 0` koşulu), yine de `ConsumeItem()` taşı düşürmeye çalışır; botun `no_item` ön kontrolü de yalnızca `UseItem != 0` skill'lerde çalışır (`ActionExecutor.cpp:801-808`) ⇒ Helis'te taşın gerçekten düşüp düşmediği `[Ö]` (ölçülemez, aşağıda). Uygulayıcı bu tabloyu yeniden **doğrulamak zorunda değildir**; §5.2'deki `--check` adımı `MAGIC` ile uyumsuzlukta zaten patlar.
- Hedefler: El Morad warrior'ları `BotWP_E`, `BotWG_E` (azami HP ~5650, `docs/05` §9.3 `[V]`). Priest'in ham gücü düşüktür (STR 120) ve toplam 6 vuruşla hedeflerin ölmesi beklenmez (K8'de `list` ile denetlenir). `BotMF_E`/`BotMI_E` (azami HP ~1541) **kullanılmaz**.
- Sunucu davranışı `[D]`: `Type 1` (kesin isabet) ⇒ zar yok, `missed` beklenmez; yine de `missed`/`srv_fail` çıkarsa araç hatası değil bulgudur (SK-04).

## 3. Kapsam

**Yapılacaklar**

1. `bots/config/skill_priest_k_master.spec`: §5.1'deki içerik.
2. `bots/config/skill_priest_k_master.txt`: 1'den `skill-script-gen.py`'nin ürettiği betik (elle düzenlenmez).

**Kapsam dışı (yapılmayacak)**

- C++ (`GameServer/`, `BotCore/`, `Tests/`), `.vcxproj`, `tools/*`, `db/*`, `docs/`, ADR, senaryo YAML'ı, mevcut `bots/config/*` dosyaları: **değişmez** (docs/ADR'yi Claude yazar).
- **Elysian Web `112825`:** `MAGIC.Moral 11` = `MORAL_AREA_FRIEND` (`GameServer/MagicInstance.h:34`); `BotCore::CastMoralSupported` yalnızca 1, 2, 4, 6, 7, 8, 10'u açar (`BotCombat.h:373-379`) ⇒ bot bu skill'i **atamaz**. Ayrı bir C++ dilimi (alan-dost `Moral` 11, `Type4` `BuffType 27`, `UseItem 379062000`) gerekir; bu planın konusu değildir. Spec'e **eklenmez** (eklenirse üretici geçer ama bot `unsupported_skill` ile reddeder (`ActionExecutor.cpp:777-785`)).
- Buff silme etkileşimleri (Malice AC buff'ını, Parasite HP buff'ını siler): hedefteki buff durumunu gözleyen bir algı tablosu yok (F4-53 `TASLAK`); bot yalnızca sonuç paketini görür ⇒ ölçülemez.
- Diriltmeler (`112733/742/754`, ölü hedef gerektirir), `BotPHB_K`, El Morad priest betiği, mage/rogue/archer, uçan skill MP beklentisi (MEC-MAG-12): sonraki dilimler.
- Bot konumlandırma, El Morad botların spawn'ı/HP'sini toplama, hedefe ≤ 0,9 m yaklaşma: betik yapmaz, K8/K9'da Claude'un işidir. Spec'e `move`/`regene`/`raw`/`pot` **eklenmez**.
- Hasar miktarı ölçümü (telemetride hedef HP'si yok; T-MECH-DMG ayrı iştir), taş tüketimi ölçümü (telemetri/`snap` eşya sayacı vermiyor, F4-45 Bulgu 3).
- `skill-check.py` hükümlerini değiştirme.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `bots/config/skill_priest_k_master.spec` | yeni | §5.1 |
| `bots/config/skill_priest_k_master.txt` | yeni | araç çıktısı (üretilmiş) |

Plan dosyası dahil 3 dosya. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch ve spec

1. `git switch -c bot/F4-46 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (`AGENTS.md` §4; bu plan sunucu açmaz).
2. `bots/config/skill_priest_k_master.spec` dosyasını aşağıdaki içerikle **aynen** yaz (ASCII, LF; yorumlar İngilizce). Söz dizimi `skill_warrior_k.spec` ile aynıdır; `raw` adımı yoktur, bu yüzden `t0 = 0`:

```
# BotPHD_K (Karus priest, curse tree 62, master 20) casts its weapon-bound master attacks on the El Morad warriors.
# Both targets (BotWP_E, BotWG_E) must be alive, in zone 71 and within 0.9 m of BotPHD_K: Type1 skills with
# MAGIC.Range 0 are bound to the weapon range (priest staff 191110000, ITEM.Range 10 = 1.0 m). The operator places them first.
# Same-skill steps are adjacent, so the recast gap is measured between neighbouring casts.

# Judgment (Type1 fail-safe, UseItem Scroll of Priest + Stone of Priest)
cast BotPHD_K 112802 BotWP_E 2
cast BotPHD_K 112802 BotWG_E 1
# Helis (Type1 fail-safe, ignores defense, Stone of Priest)
cast BotPHD_K 112815 BotWG_E 2
cast BotPHD_K 112815 BotWP_E 1
```

### 5.2 Betiği üret

`python3 tools/skill-script-gen.py bots/config/skill_priest_k_master.spec --out bots/config/skill_priest_k_master.txt` (gerçek `MAGIC`, `--magic` verilmeden; sqlcmd yalnızca `MAGIC`'i okur). Beklenen: 4 `cast` + 1 `list` = **5 adım** (`ok: 5 steps, 264 bytes, 7 lines, last offset 12500 ms`); ofsetler (adım ilerlemesi `cycles × max(recast, cast+140, 1000) + 1500`, son adımdan `list`e +500):

```
0 3500 6000 9500 | list 12500
```

(Claude bu çıktıyı 2026-10-03'te gerçek `MAGIC` ile aynı dosya adıyla `/tmp`'de ürettiği için yazdı; üretilen dosyanın ilk satır yorumu spec dosya adını içerir, bayt sayısı bu yüzden spec adına bağlıdır.) Çıktı farklıysa (örn. bir skill'in `MAGIC` verisi değişmişse) **dur** ve Uygulayıcı Raporu'na farkı yaz; spec'i kısaltma, ofsetleri elle düzeltme.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/skill-script-gen.py bots/config/skill_priest_k_master.spec --out /tmp/skill_priest_k_master.txt` çıkış 0 ve `diff /tmp/skill_priest_k_master.txt bots/config/skill_priest_k_master.txt` boş.
- [ ] K2: `python3 tools/skill-script-gen.py --check bots/config/skill_priest_k_master.txt` çıkış 0; çıktı `ok: 5 steps,` ile başlar ve `last offset 12500 ms` ile biter.
- [ ] K3: `grep -c '^[0-9]* cast BotPHD_K' bots/config/skill_priest_k_master.txt` = 4; `grep -c '^[0-9]* cast'` = 4 (başka bot yok); `grep -c '^[0-9]* list'` = 1; `grep -c '^[0-9]* \(raw\|pinvite\|paccept\|pot\|move\)'` = 0; ilk satır ofseti `0` (`112802`); satırlar sırasıyla `112802 BotWP_E 2`, `112802 BotWG_E 1`, `112815 BotWG_E 2`, `112815 BotWP_E 1`; hedef olarak yalnızca `BotWP_E`/`BotWG_E` geçer (`self`, `_K`, `BotMF_E`, `BotMI_E` yok); `112825` hiçbir yerde geçmez.
- [ ] K4: ardışık ofset farkları skill'in kendi periyodundan (`cycles × max(recast, 1000) + 1500`) küçük değildir: `awk` ile 3500, 2500, 3500, 3000 ms (son fark `112815`'ten `list`e, +500 ile).
- [ ] K5: `python3 tools/skill-script-gen.py --selftest` hâlâ `selftest: 23 checks, 0 failed` ve `python3 tools/skill-check.py --selftest` son satırı `selftest: N checks, 0 failed` (N ≥ 32); iki araç **değişmedi** (K7).
- [ ] K6: `./tools/build.sh Release` hatasız biter (C++ değişmedi; yeni uyarı yok) ve `./tools/run-tests.sh` `251 tests, 0 failed` (ya da fazlası) ile geçer.
- [ ] K7: `git diff --stat gece/2026-10-02...bot/F4-46` yalnızca `bots/config/skill_priest_k_master.spec`, `bots/config/skill_priest_k_master.txt` ve plan dosyasını gösterir.
- [ ] K8 (Claude, çalışma zamanı): sunucular açık (`[BOT] ENABLED=1 MAX_BOTS=16 TELEMETRY=decisions`), `BotPHD_K`, `BotWP_E`, `BotWG_E` spawn; hepsi canlı (`list`: `hp > 0`) ve zone 71'de; hedefler `BotPHD_K`'dan `/bot move` ile ≤ 0,9 m'ye getirilir ve `list` konumlarından mesafe doğrulanır (≤ 0,9 m kurulamazsa guard `out_of_range` verir: kurulum hatasıdır, bulgu değildir); Scroll of Priest ≥ 1 ve Stone of Priest ≥ 6 çantada (`bot-refill.sh` sonrası); toplam MP maliyeti `3×200 + 3×350` = **1650** (priest azami MP'si ölçülmedi `[A]`; MP yetmezse `no_mana`/`mp_before` telemetrisinden görülür ve Claude MP pot adımı ekleyen yeni plan yazar); betik `Scripts/skill_priest_k_master.txt` olarak koşulur.
- [ ] K9 (Claude, çalışma zamanı): `skill-check.py <jsonl> --min-n 1` raporu: 2 skill görünür, ikisinde de 3 çevrim başlar; 5/5 adım zamanında koşar (en geç gecikme < 500 ms); `srv_fail` = 0 ve `missed` = 0 beklenir (`Type 1` kesin isabet); değilse bulgu olarak `docs/05` §9.5'e işlenir. Helis'te sunucunun `UseItem 0` ile taşı gerçekten tüketip tüketmediği ve Judgment'ın scroll'u tüketmediği çalışma zamanı sonrası ölçülemezse `[Ö]` kalır. Sunucular `stop` ile kapatılır, `GameServer.ini` yedekten geri döner.
- Derleme sonucu (`tools/build.sh Release` son satırları) Uygulayıcı Raporu'na yapıştırılır.

## 7. Doğrulama komutları

```bash
python3 tools/skill-script-gen.py bots/config/skill_priest_k_master.spec --out /tmp/skill_priest_k_master.txt
diff /tmp/skill_priest_k_master.txt bots/config/skill_priest_k_master.txt
python3 tools/skill-script-gen.py --check bots/config/skill_priest_k_master.txt

grep -c '^[0-9]* cast BotPHD_K' bots/config/skill_priest_k_master.txt     # 4
grep -c '^[0-9]* cast' bots/config/skill_priest_k_master.txt              # 4
grep -c '^[0-9]* list' bots/config/skill_priest_k_master.txt              # 1
grep -c '^[0-9]* \(raw\|pinvite\|paccept\|pot\|move\)' bots/config/skill_priest_k_master.txt  # 0
grep '^[0-9]* cast' bots/config/skill_priest_k_master.txt | awk '$5!="BotWP_E" && $5!="BotWG_E"'   # no output
grep -c 112825 bots/config/skill_priest_k_master.txt                      # 0
grep '^[0-9]* \(cast\|list\)' bots/config/skill_priest_k_master.txt | awk '{ if (NR>1) print $1-last; last=$1 }'
# K4: prints 3500, 2500, 3500, 3000 (differences of consecutive offsets)

python3 tools/skill-script-gen.py --selftest
python3 tools/skill-check.py --selftest
./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-46
```

(Uygulayıcı çıktıyı Uygulayıcı Raporu'na gerçek hâliyle yapıştırır. `awk` satırı ardışık ofset farklarını basar; çıktı 4 sayıdır.)

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. Spec ve üretilmiş betik ASCII, LF; yorumlar İngilizce. Üretilmiş dosya elle düzenlenmez.
- Kişisel veri: yalnızca `MAGIC` okunur (araç zaten böyle); başka tablo sorgulanmaz.
- Betik sınırları (`BotCore/ScriptPlan.h`): ≤ 100 adım, ≤ 8192 bayt, ≤ 128 satır, ≤ 600000 ms; bu betik 5 adım / 264 bayt / 12,5 sn ile içindedir.
- Uygulayıcı §5.1'deki spec'i **aynen** yazar; `--margin-ms`, `--start-gap-ms` ve üretici varsayılanlarını değiştirmez. Çıktı §5.2'deki ofsetlerden farklıysa dur ve sor.
- Menzil: `Range 0` skill'lerde hedef ≤ 0,9 m olmalıdır (priest sopası 1,0 m); hedef uzaksa guard `out_of_range` (`MEC-MAG-11`, `value` = mesafe alanı, `limit` 10) ile reddeder. Bu K9'da bulgu değil **kurulum hatasıdır**: Claude hedefleri yaklaştırıp koşuyu yeniler.
- Sunucu çalıştırma ve gerçek koşu bu plan kapsamında DeepSeek'in işi **değildir**.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-46` — `402d15f [F4-46] Karus priest usta skill betiği (Judgment, Helis) spec + üretilmiş betik` (uygulama), ardından Durum/rapor commit'i.
- Değişen dosyalar ve neden:
  - `bots/config/skill_priest_k_master.spec` (yeni): plan §5.1 içeriği aynen; `BotPHD_K` Judgment (`112802`) ve Helis (`112815`) 6 çevrim, hedefler `BotWP_E`/`BotWG_E`.
  - `bots/config/skill_priest_k_master.txt` (yeni): `skill-script-gen.py` çıktısı (elle düzenlenmedi); 4 `cast` + 1 `list`.
  - `plans/F4-46-skill-betik-priest-master-judgment-helis.md`: `Durum` satırı ve bu rapor.
- Derleme sonucu (`tools/build.sh Release` son satırlar) ve `./tools/run-tests.sh`:
  ```
  proj-GameServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\AIServer.exe
  BotCoreTests.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  `./tools/run-tests.sh`: son satır `251 tests, 0 failed` (C++ değişmedi).
- Kabul kriterleri öz-değerlendirme (K1–K7; K8/K9 Claude'un işi):
  - K1 ✔: yeniden üretim `/tmp/skill_priest_k_master.txt` ile commit'li dosya arasında `diff` boş, çıkış 0.
  - K2 ✔: `--check` çıkış 0; `ok: 5 steps, 264 bytes, 7 lines, last offset 12500 ms`.
  - K3 ✔: `cast BotPHD_K` = 4, toplam `cast` = 4, `list` = 1, `raw|pinvite|paccept|pot|move` = 0; ilk ofset `0` (`112802 BotWP_E 2`), sıra `112802 BotWP_E 2`, `112802 BotWG_E 1`, `112815 BotWG_E 2`, `112815 BotWP_E 1`; hedefler yalnızca `BotWP_E`/`BotWG_E`; `112825` = 0.
  - K4 ✔: ardışık ofset farkları `3500, 2500, 3500, 3000` ms.
  - K5 ✔: `skill-script-gen.py --selftest` → `selftest: 23 checks, 0 failed`; `skill-check.py --selftest` → `selftest: 32 checks, 0 failed`; iki araç değişmedi.
  - K6 ✔: `./tools/build.sh Release` hatasız bitti (C++ değişmedi); `./tools/run-tests.sh` → `251 tests, 0 failed`.
  - K7 ✔: `git diff --stat gece/2026-10-02...bot/F4-46` yalnızca spec, üretilmiş betik ve plan dosyasını gösterir (aşağıda).
- Plandan sapmalar ve gerekçeleri: Yok. Üretilen çıktı §5.2'deki beklenen ofsetler/bayt sayısıyla birebir.
- Açık sorular: Yok. K8/K9 (gerçek koşu, sunucu açma, bot konumlandırma) plan gereği Claude'un işidir; DeepSeek sunucu çalıştırmadı.
- `git diff --stat gece/2026-10-02...bot/F4-46` (gerçek çıktı):
  ```
   bots/config/skill_priest_k_master.spec             | 11 +++++++++++
   bots/config/skill_priest_k_master.txt              |  7 +++++++
   plans/F4-46-skill-betik-priest-master-judgment-helis.md | 32 ++++++++++++++++------
   3 files changed, 41 insertions(+), 9 deletions(-)
  ```

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-46` @ `c78c990` (kod/spec commit'i `402d15f`; `c9b3a3e` ve `c78c990` yalnızca `Durum` ve Uygulayıcı Raporu). Gece modu (`AUTO_LOOP=1`): birleştirme/push yapılmadı, döngü betiği yapar.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `skill-script-gen.py bots/config/skill_priest_k_master.spec --out /tmp/skill_priest_k_master.txt` rc=0; `diff` boş |
| K2 | ✔ | `--check` → `ok: 5 steps, 264 bytes, 7 lines, last offset 12500 ms`, rc=0 |
| K3 | ✔ | `cast BotPHD_K` 4, `cast` 4, `list` 1, `raw\|pinvite\|paccept\|pot\|move` 0; ilk ofset `0` (`112802 BotWP_E 2`), sıra `112802 BotWP_E 2`, `112802 BotWG_E 1`, `112815 BotWG_E 2`, `112815 BotWP_E 1`; `awk $5!=...` çıktısı boş (hedefler yalnızca `BotWP_E`/`BotWG_E`); `112825` 0 |
| K4 | ✔ | `awk` farkları `3500 2500 3500 3000` |
| K5 | ✔ | `skill-script-gen.py --selftest` `23 checks, 0 failed`; `skill-check.py --selftest` `32 checks, 0 failed`; `git diff --stat ... -- tools BotCore GameServer shared AIServer Tests docs db` boş |
| K6 | ✔ | `./tools/build.sh Release` rc=0, 7 proje bağlandı, `warning`/`error` satırı 0; `./tools/run-tests.sh` `251 tests, 0 failed` |
| K7 | ✔ | `git diff --stat`: yalnızca `skill_priest_k_master.spec`, `skill_priest_k_master.txt` ve plan dosyası; plan dosyasında yalnızca `Durum` ve Uygulayıcı Raporu değişti |
| K8 | ✔ | Çalışma zamanı (aşağıda): `[BOT] ENABLED=1 MAX_BOTS=16 TELEMETRY=decisions`; üç bot canlı zone 71; hedefler `BotPHD_K`'ya 0,5 m ve 0,3 m; Scroll/Stone çantada (`bot-refill.sh apply --mp-pots 20`: `ok=12 fail=0`); MP 4782/6392 (≥ 1650); betik `Scripts/skill_priest_k_master.txt` |
| K9 | ✔ | 5/5 adım, en geç gecikme 71 ms (< 500); 2 skill görünür, ikisinde 3 çevrim başladı ve etkili; `srv_fail` 0, `missed` 0; PASS 2, WARN 0, FAIL 0; MP düşümü 200 ve 350 (beklenenle aynı); taş tüketimi ve Judgment scroll'unun tüketilmemesi ölçülemedi `[Ö]` (plan "yoksa `[Ö]`" der) |

- Biçim: iki yeni dosya ASCII (`file`), depoda LF (`git ls-files --eol`: `i/lf`; çalışma ağacındaki CRLF yalnızca eski dosyalarda `autocrlf` sonucu, `ScriptPlan.h:151` sondaki `\r`'yi kırpar); spec, plan §5.1 bloğuyla aynı; C++/`.vcxproj`/docs/ADR/araçlar/mevcut `bots/config/*` değişmedi.
- Plan dayanakları yeniden kontrol edildi: `BotCombat.h:143-150` (`CastInRange`, `skillRange == 0` ⇒ `distanceField <= weaponRangeField`), `ActionExecutor.cpp:894-897` ve `:801-808` (`no_item` yalnızca `UseItem != 0`); yerel `MAGIC`: `112802` `200/0/5/0/1/0/7`, `Skill 1128/2`, `UseItem 379066000`, `BeforeAction 4`; `112815` `350/0/5/0/1/0/7`, `Skill 1128/12`, `UseItem 0`, `BeforeAction 4`; `112825` `Moral 11`, `Type1 4`; `ITEM.Range` 191110000/007/008 = 10.

**K8/K9 çalışma zamanı ayrıntısı.** `bot-refill.sh apply --mp-pots 20` (sunucu kapalıyken), `GameServer.ini`'ye geçici `[BOT] ENABLED=1 MAX_BOTS=16 TELEMETRY=decisions`, Release sunucu; `BotPHD_K`, `BotWP_E`, `BotWG_E` 6 sn arayla spawn (zone 71; (1274, 928), (1273, 934,5), (1271, 933,5)). `pot BotWP_E 389015000 8` ile `BotWP_E` 3053 → 5650; `move BotWP_E 1274.0 928.5 45`, `move BotWG_E 1274.3 928.0 45`; `list`: `hp > 0`, konumlar 0,5 m / 0,3 m, `BotPHD_K` MP 4782/6392. `script run skill_priest_k_master`: `loaded (5 step(s), last offset 12500 ms)`, `finished skill_priest_k_master: completed, 5/5 step(s) in 12534 ms (max late 71 ms)`. `live-100119.jsonl` üstünde `skill-check.py --min-n 1`: 2 skill, PASS 2, WARN 0, FAIL 0, NO_DATA 0. Betik sonu `list`: `BotPHD_K` MP 3252, `BotWP_E` 4973/5650, `BotWG_E` 4233/5650 (ikisi de hayatta). Botlar `despawn all`, sunucular `stop`, `GameServer.ini` yedekten geri yüklendi (`cmp` aynı), `Scripts/skill_priest_k_master.txt` kopyası ve `BotCommands.txt` silindi. Sonuçlar `docs/05` §9.5'e işlendi.

- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. `docs/05` §9.5: Judgment ve Helis botla beklendiği gibi çalışıyor (kesin isabet, MP maliyeti tam, recast aralığı ~1,1 sn); bulgu/hata yok.
  2. Stone of Priest tüketimi ve Judgment scroll'unun korunması ölçülemedi `[Ö]` (telemetri/`snap` eşya sayacı yok; §9.4 Bulgu 3 ile aynı; ayrı iş).
  3. Hasar miktarı ölçülmedi (kapsam dışı); `list` HP farkları yenilenme içerdiği için yalnızca gözlem `[Ö]`.
  4. Üslup: Uygulayıcı raporundaki derleme çıktısı kısaltılmış, plan dosyası satır sayısı (`32`) rapordan sonraki commit'le 33 oldu (önemsiz); rc ve `251 tests` doğrulamada yeniden üretildi; başka sapma yok.
- Düzeltme talimatı: yok (`DOĞRULANDI`).
