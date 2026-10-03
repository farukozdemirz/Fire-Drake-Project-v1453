# F4-43: T-MECH-SKILL botla koşusu, dilim 2b — MP yenilenmesine dayanıklı hüküm (`skill-check.py`) ve düzeltilmiş Karus priest spec'i

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02, merge `5318a90`) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; ADR-0018 m.9 "T-MECH-SKILL'in botla yeniden koşusu", Ek 19) |
| Branch | `bot/F4-43` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-42 (`tools/skill-script-gen.py`, `bots/config/skill_priest_k.{spec,txt}`) — `KAPANDI` (merge `28cc1d6`); F4-41 (`tools/skill-check.py`) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/05` §9 / §9.1 (T-MECH-SKILL-P-*: MP düşümü, recast, etki, fail sebebi), `docs/03` MEC-MAG-18, CLI-15; ADR-0018 m.9, Ek 17, Ek 18, Ek 19 |
| Tahmini büyüklük | S (1 araç düzeltmesi + 1 spec + üretilmiş betik; C++ yok) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

F4-42'nin gerçek koşusu (`docs/05` §9.1) iki sınıf sorun gösterdi: (a) **ölçüm aracı** MP yenilenmesini hesaba katmıyor, bu yüzden 5 skill yanlışlıkla `mp_verdict FAIL` aldı (hepsinde en büyük düşüm `Msp`'ye eşitti); (b) **spec** üç yerde yanlıştı: party kabul aralığı 1000 ms (CLI-15 `kPartyAcceptMinMs = 1000` sınırında reddedildi), `112548` aynı hedefe art arda atılıyor (sunucu reddediyor) ve `112656` Greatness'tan önce aynı `BuffType`'lı `112657` atılıyor (sunucu sessizce atlıyor). Bu plan aracı `mp_delta_max` ve sınırlı bir yenilenme payıyla hüküm verir hale getirir, spec'i düzeltir ve betiği yeniden üretir. Gerçek yeniden koşu Claude'un çalışma zamanı doğrulamasıdır (K7).

## 2. Bağlam (okunması zorunlu)

- `docs/05` §9.1 (F4-42 koşu bulguları 1-5) ve `docs/03` MEC-MAG-18 (`Moral` 4 / 6 kuralları).
- `tools/skill-check.py` (tamamı; özellikle aşağıdaki işlevler) ve `tools/skill-script-gen.py` (`--selftest` değişmez; bu planda **dokunulmaz**).
- İlgili kod (hepsini açıp doğrula; satırlar `28cc1d6` itibarıyladır):
  - `tools/skill-check.py:168` `analyze(paths, magic, mp_tol, ms_tol, min_n)`; `:310` `build_report(...)` çağrısı; `:313` `build_report`; `:328` `judge_mp(mp_exp, deltas, mp_tol, min_n)` çağrısı; `:396-407` `judge_mp` (şimdi: tüm örnekler `Msp ± mp_tol` içindeyse `PASS`, medyan içindeyse `WARN`, aksi `FAIL`); `:565-569` selftest `run_case` yardımcısı; `:575-615` `mp_ok`, `mp_off_fail`, `mp_off_warn`, `mp_no_data` kontrolleri; `:706` `analyze([path], magic, 10, 50, 3)` konumsal çağrısı; `:785-796` argparse seçenekleri; `:805` `analyze(...)` çağrısı.
  - `BotCore/BotCombat.h:767` `kPartyAcceptMinMs = 1000`; `BotCore/BotCombat.h:822` `sinceInviteMs < kPartyAcceptMinMs` ⇒ `accept_wait` (F4-42 koşusunda `value` 996/989).
  - `GameServer/MagicInstance.cpp:515-522`: `pType->sTimeDamage > 0` (HoT/restoration) iken hedefte `m_durationalSkills[i].m_sHPAmount > 0` varsa `CheckType3Prerequisites()` `false` döner. Doğrulanan `MAGIC_TYPE3`: `112548` `TimeDamage 2500`, `Duration 30` (30 sn); `112527`/`112536`/`112545`/`112554`/`112557`/`112560` `TimeDamage 0` (etkilenmez). **Sonuç `[D]`:** F4-42'deki `112548` `cycles 3` aralığı (1720 ms) 30 sn'lik HoT'un içinde kaldığı için 2. atış `srv_fail -100` aldı; "tam canlı hedef" nedeni **değildir**.
  - `GameServer/MagicInstance.cpp:1770-1782` ve `docs/03` MEC-MAG-18: grup (`hedef -1`) Type4'te hedefte aynı `BuffType` varsa üye **sessizce atlanır** (hata ve yayın yok); hiçbir üye buff almazsa yayın yoktur ⇒ bot `no_result`. Doğrulanan `MAGIC_TYPE4.BuffType`: `112654` = 1, `112656` = 1 (`Radius 30`), `112657` = 1, `112660` = 2, `112645` = 8, `112820` = 30. **Sonuç `[D]`:** F4-42 spec'inde `BotPHB_K` önce `112657 self` (BuffType 1) attı, sonra `112656 self`; `BotWP_K`/`BotWG_K`/`BotPHD_K` `112654` ile zaten BuffType 1 taşıyordu ⇒ kurbanların hepsi atlandı ⇒ `no_result`. Greatness'ın çalışması için **çağıranın BuffType 1'i boş olmalı**.
- `bots/config/skill_priest_k.spec` ve `.txt` (F4-42 çıktısı; yeni sürüm aracın aynı komutuyla yeniden üretilir).
- Gerçek `MAGIC` satırları değişmedi (F4-42 K2): `112527` `80/15/20/56/3/2`, `112536` `160/15/1/56/3/2`, `112545` `320/15/1/56/3/2`, `112548` `625/15/1/56/3/2`, `112554` `960/15/54/56/3/2`, `112557` `960/15/54/56/3/6`, `112560` `1920/15/64/56/3/6`, `112525` `60/15/15/56/5/2`, `112535` `120/15/15/56/5/2`, `112660` `150/15/1/56/4/2`, `112645` `60/15/1/56/4/2`, `112654` `240/15/1/56/4/4`, `112657` `360/15/1/56/4/4`, `112656` `570/15/1/101/4/6`, `112820` `320/15/1/45/4/1` (`Msp/CastTime/ReCastTime/Range/Type1/Moral`). Uygulayıcı önce `skill-script-gen.py`'nin gerçek `MAGIC` ile üretimini çalıştırıp spec'in hâlâ geçerli olduğunu doğrulasın.

## 3. Kapsam

**Yapılacaklar**

1. `tools/skill-check.py`: `judge_mp` hükmünü **en büyük düşüm + sınırlı yenilenme payı** kuralına çevir (§5.2), `--mp-regen N` seçeneği (varsayılan 60), selftest'e yeni kontroller.
2. `bots/config/skill_priest_k.spec`: §5.3'teki düzeltilmiş içerik (party aralığı 1500 ms; `112548` hedef başına tek atış; BuffType 1 üçlüsünün hedef dağılımı).
3. `bots/config/skill_priest_k.txt`: 2'den aracın yeniden ürettiği betik (elle düzenlenmez).

**Kapsam dışı (yapılmayacak)**

- C++ (`GameServer/`, `BotCore/`, `Tests/`), `.vcxproj`: **yok**; davranış değişmez.
- `tools/skill-script-gen.py` (değişmez; `--selftest` sayısı değişmez), `tools/bot-refill.sh`, `db/*`, `docs/`, ADR, senaryo YAML'ı: **değişmez** (docs/ADR'yi Claude yazar).
- Bot konumlandırma: betik konumlandırmaz; botları 56 m içine toplamak K7'de Claude'un işidir (`/bot move`). Spec'e `move` **eklenmez** (bot konumu DB'deki son konumdur, mesafe bilinmez).
- Uçan skill'de MP'nin iki kez düşmesi (MEC-MAG-12) beklenti düzeltmesi, düşman hedefli priest, warrior, mage betikleri: **F4-44+**.
- `--mp-tol`/`--ms-tol` varsayılanlarını, recast ve etki hükümlerini, çıktı sütunlarını/JSON anahtarlarını **değiştirme** (yalnızca `mp_verdict` hesabı değişir).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/skill-check.py` | değiştir | `judge_mp`, `build_report`/`analyze` imzası (anahtar sözcüklü yeni parametre), argparse, USAGE, docstring, selftest |
| `bots/config/skill_priest_k.spec` | değiştir | §5.3 |
| `bots/config/skill_priest_k.txt` | değiştir | araç çıktısı (üretilmiş) |

Plan dosyası dahil 4 dosya. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-43 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (`AGENTS.md` §4; bu plan sunucu açmaz).

### 5.2 `tools/skill-check.py`: yenilenmeye dayanıklı MP hükmü

Kök neden: telemetri `mp_before − mp_after` verir; bot MP'si cast sırasında doğal yenilenir (+20/+40 kümeli, `docs/05` §9.1 bulgu 1), bu yüzden tek tek düşüm `Msp`'den küçük çıkabilir, **ama hiçbir düşüm `Msp`'yi aşamaz** ve en az bir örnekte tam `Msp` görülür. Yeni kural (`judge_mp(mp_exp, deltas, mp_tol, mp_regen, min_n)`):

```python
DEFAULT_MP_REGEN = 60   # max MP the bot may regenerate during one cast window [A] (observed +20/+40)

def judge_mp(mp_exp, deltas, mp_tol, mp_regen, min_n):
    if mp_exp is None or len(deltas) < min_n:
        return "NO_DATA"
    regen = min(mp_regen, mp_exp // 2)          # regeneration cannot refund more than half the cost
    floor = mp_exp - regen
    top = max(deltas)
    if top > mp_exp + mp_tol or top < floor:
        return "FAIL"
    if top < mp_exp - mp_tol or min(deltas) < floor:
        return "WARN"
    return "PASS"
```

- `analyze(paths, magic, mp_tol, ms_tol, min_n, mp_regen=DEFAULT_MP_REGEN)` ve `build_report(stats, totals, magic, mp_tol, ms_tol, min_n, mp_regen=DEFAULT_MP_REGEN)`: yeni parametre **sona ve varsayılanlı**, böylece mevcut konumsal çağrılar (`:569`, `:706`) bozulmaz; `build_report` `judge_mp`'ye geçirir.
- argparse: `--mp-regen` (`type=int`, varsayılan `DEFAULT_MP_REGEN`; negatifse `parser.error`), `main` `analyze`'a verir. USAGE'e ve modül docstring'ine bir satır ekle: `--mp-regen N  MP the bot may regenerate during one cast, capped at half the skill cost (default 60; 0 = strict)`.
- Çıktı biçimi, sütunlar ve JSON anahtarları **aynı** kalır.

Selftest (mevcut 26 kontrol aynen kalır: `mp_ok`, `mp_off_fail`, `mp_off_warn`, `mp_no_data` yeni kural altında da aynı sonucu verir; uygulayıcı bunu çalıştırıp doğrulasın). **Yeni** adlandırılmış kontroller (bellek içi `MAGIC` girdileri ekle: Msp 80, 160, 625):

1. `mp_regen_pass`: Msp 80; düşümler 80, 40, 80 ⇒ `PASS` (en büyük 80 bandın içinde, en küçük 40 ≥ taban 40).
2. `mp_regen_single_warn`: Msp 625; tek düşüm 585, `min_n=1` ⇒ `WARN` (taban 565 ≤ 585 < 615).
3. `mp_below_floor_fail`: Msp 160; düşümler 40, 40 ⇒ `FAIL` (en büyük 40 < taban 100).
4. `mp_over_fail`: Msp 80; düşümler 80, 100 ⇒ `FAIL` (en büyük 100 > 90).
5. `mp_regen_min_warn`: Msp 160; düşümler 160, 60 ⇒ `WARN` (en büyük tamam, en küçük 60 < taban 100).
6. `mp_regen_zero_strict`: Msp 80; düşümler 80, 40, 80, `mp_regen=0` ⇒ `WARN` (taban 80; en küçük 40 < 80). `run_case`'e `mp_regen` anahtar sözcüklü parametresi ekle.

Toplam ≥ 32 kontrol (26 + 6); son satır `selftest: N checks, 0 failed`.

### 5.3 Düzeltilmiş spec (`bots/config/skill_priest_k.spec`)

Dosyayı aşağıdaki içerikle **değiştir** (yorumlar İngilizce; `skill-script-gen.py` söz dizimi F4-42 ile aynı):

```
# party: BotPHD_K leads, the other three join (Moral 4 / 6 skills need a party).
# Every paccept is 1500 ms after its pinvite (CLI-15 rejects an accept earlier than 1000 ms).
# The four bots must start within 56 m of each other; the operator gathers them first.
raw 0    pinvite BotPHD_K BotPHB_K
raw 1500 paccept BotPHB_K
raw 3000 pinvite BotPHD_K BotWP_K
raw 4500 paccept BotWP_K
raw 6000 pinvite BotPHD_K BotWG_K
raw 7500 paccept BotWG_K

# BotPHD_K: heals and cures on BotWP_K (Heal 60); MP pots between the expensive ones
cast BotPHD_K 112527 BotWP_K 3
cast BotPHD_K 112536 BotWP_K 3
cast BotPHD_K 112545 BotWP_K 3
cast BotPHD_K 112525 BotWP_K 3
cast BotPHD_K 112535 BotWP_K 3
# 112548 is a 30 s restoration (HoT): an active restoration on the target rejects the next cast
# (MagicInstance.cpp CheckType3Prerequisites), so one cast per target
cast BotPHD_K 112548 BotWP_K 1
cast BotPHD_K 112548 BotWG_K 1
cast BotPHD_K 112548 self 1
pot  BotPHD_K 389220000 3
cast BotPHD_K 112554 BotWP_K 3
cast BotPHD_K 112557 BotWP_K 3
pot  BotPHD_K 389220000 3
cast BotPHD_K 112560 BotWP_K 2

# BotPHB_K: buffs; one target per sample because an active same-BuffType buff is rejected (MEC-BUF-02)
cast BotPHB_K 112660 BotWP_K 1
cast BotPHB_K 112660 BotPHD_K 1
cast BotPHB_K 112660 BotWG_K 1
cast BotPHB_K 112645 BotWP_K 1
cast BotPHB_K 112645 BotPHD_K 1
cast BotPHB_K 112645 BotWG_K 1
# 112654, 112657 and 112656 share BuffType 1 (HP_MP): one of them per target. The caster itself must stay
# clear of BuffType 1 until 112656, whose group path skips any member that already holds it (MEC-MAG-18)
cast BotPHB_K 112654 BotWP_K 1
cast BotPHB_K 112654 BotWG_K 1
cast BotPHB_K 112657 BotPHD_K 1
cast BotPHB_K 112656 self 1
cast BotPHB_K 112820 self 1
```

Üretim ve yerleştirme: `python3 tools/skill-script-gen.py bots/config/skill_priest_k.spec --out bots/config/skill_priest_k.txt` (gerçek `MAGIC`, `--magic` verilmeden). Beklenen: `t0 = 7500 + 1500 = 9000`; 31 adım (6 `raw` + 11 `BotPHD_K` `cast` + 11 `BotPHB_K` `cast` + 2 `pot` + 1 `list`). Yeniden üretim `skill-script-gen.py`'nin 31 adımı `--check` ile sınırlar içinde bulması demektir; sınır aşılırsa **spec'i kısaltma, dur** ve rapora yaz.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/skill-check.py --selftest` çıkış 0, son satır `selftest: N checks, 0 failed`, N ≥ 32; çıktıda/kodda `mp_ok`, `mp_off_fail`, `mp_off_warn`, `mp_no_data` ve §5.2'deki 6 yeni ad vardır (`grep -c` ile gösterilir).
- [ ] K2: Sabit örnek (komutlar §7): tek örneklik `112548` (Msp 625, düşüm 585) `--min-n 1` ile varsayılan `--mp-regen`'de `mp_verdict == "WARN"`, `--mp-regen 0`'da `"FAIL"`, `--mp-regen 80`'de `"WARN"`; **değişiklikten önceki** davranışta bu örnek `FAIL` idi (Uygulayıcı önce `git stash`/ana dalda gösterir).
- [ ] K3: `judge_mp` dışında hiçbir hüküm (`recast_verdict`, `effect_verdict`) ve çıktı sütunu değişmedi: `git diff gece/2026-10-02...bot/F4-43 -- tools/skill-check.py` yalnızca §5.2'deki işlevleri/imzaları, argparse/USAGE/docstring satırlarını ve selftest kontrollerini gösterir (Uygulayıcı Raporu'nda `git diff --stat` ve hunk başlıkları listelenir).
- [ ] K4: `python3 tools/skill-script-gen.py bots/config/skill_priest_k.spec --out /tmp/skill_priest_k.txt` (gerçek `MAGIC`) çıkış 0 ve `diff /tmp/skill_priest_k.txt bots/config/skill_priest_k.txt` boş.
- [ ] K5: `python3 tools/skill-script-gen.py --check bots/config/skill_priest_k.txt` çıkış 0 ve `ok: 31 steps, ...`; `grep -c '^[0-9]* cast BotPHD_K'` = 11, `grep -c '^[0-9]* cast BotPHB_K'` = 11 (`112654` ×2 ve `112657` ×1: bkz. §5.3), `grep -c '^[0-9]* \(pinvite\|paccept\)'` = 6, `grep -c '^[0-9]* pot'` = 2; `grep '^[0-9]* paccept'` satırlarının her birinin ofseti kendi `pinvite`'ınınkinden tam 1500 ms büyüktür (`grep -n '^1500 paccept BotPHB_K'`, `4500 ... BotWP_K`, `7500 ... BotWG_K`); `112548` üç satırdır (`BotWP_K`, `BotWG_K`, `self`), `112656` `112657`'den **sonradır** ve `112657` hedefi `BotPHD_K`'dır; `grep -c 112703` = 0.
- [ ] K6: `python3 tools/skill-script-gen.py --selftest` hâlâ `selftest: 23 checks, 0 failed` (araç değişmedi).
- [ ] K7: `./tools/build.sh Release` hatasız biter (C++ değişmedi; yeni uyarı yok) ve `./tools/run-tests.sh` `251 tests, 0 failed` (ya da fazlası) ile geçer.
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F4-43` yalnızca `tools/skill-check.py`, `bots/config/skill_priest_k.spec`, `bots/config/skill_priest_k.txt` ve plan dosyasını gösterir.
- [ ] K9 (Claude, çalışma zamanı): `bot-refill.sh apply --mp-pots 20` stoğu duruyorsa atla; sunucular açık, `[BOT] ENABLED=1 MAX_BOTS=16 TELEMETRY=decisions`, 4 bot spawn, **botlar 56 m içine toplanır** (`/bot move`), betik `Scripts/skill_priest_k.txt` olarak koşulur; `skill-check.py <jsonl> --min-n 1` raporu: tüm 3 party üyesi girer (`CLI-15 accept_wait` yok), `112548` 3/3 `effected`, `112656` `effected` (sonuç paketi gelir, `no_result` yok), `mp_verdict FAIL` yok (yenilenmeden kaynaklanan), kalan her `FAIL`/`WARN` bir bulgu olarak `docs/05` §9.1'e işlenir. Sunucular `stop` ile kapatılır, `GameServer.ini` yedekten geri döner.
- Derleme sonucu (`tools/build.sh Release` son satırları) Uygulayıcı Raporu'na yapıştırılır.

## 7. Doğrulama komutları

```bash
python3 tools/skill-check.py --selftest
grep -c 'mp_regen_pass\|mp_regen_single_warn\|mp_below_floor_fail\|mp_over_fail\|mp_regen_min_warn\|mp_regen_zero_strict' tools/skill-check.py

# K2 fixture (not committed): Msp 625, one cast, 1000 -> 415 (drop 585)
printf '112548|Superior restore|625|15|1|56|3|0\n' > /tmp/f443.magic
printf '%s\n' \
  '{"ev":"ACTION_SUBMIT","t":0,"bot":1,"type":"CastEffect","skill":112548,"decision_id":1,"mp":1000}' \
  '{"ev":"ACTION_RESULT","t":10,"bot":1,"type":"CastEffect","decision_id":1,"ok":true,"reason":"effected","code":0,"mp_after":415}' \
  > /tmp/f443.jsonl
for r in "" "--mp-regen 0" "--mp-regen 80"; do
  python3 tools/skill-check.py /tmp/f443.jsonl --magic /tmp/f443.magic --min-n 1 --json $r \
    | python3 -c "import json,sys; print(json.load(sys.stdin)['skills'][0]['mp_verdict'])"
done   # expected: WARN, FAIL, WARN

python3 tools/skill-script-gen.py bots/config/skill_priest_k.spec --out /tmp/skill_priest_k.txt
diff /tmp/skill_priest_k.txt bots/config/skill_priest_k.txt
python3 tools/skill-script-gen.py --check bots/config/skill_priest_k.txt
python3 tools/skill-script-gen.py --selftest
./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-43
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `tools/skill-check.py` ASCII/UTF-8, LF, yalnızca standart kütüphane; mevcut dosyanın üslubunu ve yorum yoğunluğunu koru. Spec ve üretilmiş betik ASCII, LF. Kod yorumları ve değişken adları İngilizce.
- Kişisel veri: yalnızca `MAGIC` okunur (araçlar zaten böyle); başka tablo sorgulanmaz.
- `mp_regen` payı `[A]`'dır (gözlem: +20/+40; üst sınır 60 ve `Msp/2` tavanı seçimdir). Bu bir **hüküm gevşetmesidir**: gerçek MP düşümü hatası (hiç düşmeyen/yarıdan az düşen skill) yine `FAIL`/`WARN` verir (`mp_below_floor_fail`, `mp_regen_min_warn`).
- Derleme/test hedefleri C++'a dokunmadığı için değişmez; yine de K7'yi çalıştır.
- Uygulayıcı §5.3'teki spec'i **aynen** yazar; `margin_ms`, `--start-gap-ms` ve üretici varsayılanlarını değiştirmez. Betik sınırı aşılırsa dur ve sor.
- Uçan skill ve düşman hedefli skill bu planda yok; spec'e ekleme.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-43` (taban `gece/2026-10-02`) — `8fd65a8 [F4-43] skill-check MP hükmü yenilenmeye dayanıklı + priest spec v2`
- Değişen dosyalar ve neden:
  - `tools/skill-check.py`: `judge_mp` "en büyük düşüm + sınırlı yenilenme payı" kuralına çevrildi; `DEFAULT_MP_REGEN = 60`; `analyze`/`build_report` sona varsayılanlı `mp_regen` parametresi; `--mp-regen` argparse (negatifse `parser.error`); USAGE/docstring satırı; selftest `run_case(mp_regen=...)` + 6 yeni kontrol (bellek içi MAGIC 110601/110602/110603, Msp 80/160/625).
  - `bots/config/skill_priest_k.spec`: §5.3 içeriği aynen (party aralığı 1500 ms; `112548` hedef başına tek atış ×3; BuffType 1 üçlüsü `112654`×2 hedef + `112657`→`BotPHD_K`; `112656` self en sonda).
  - `bots/config/skill_priest_k.txt`: `skill-script-gen.py` ile gerçek `MAGIC`'ten yeniden üretildi (31 adım, `t0=9000`).
  - `plans/F4-43-...md`: yalnızca `Durum` ve bu rapor.
- Derleme sonucu (`tools/build.sh Release`, rc=0; son satırlar):
  ```
  BotCore.vcxproj -> ...\build\bin\x86-Release\libs\BotCore.lib
  Lua.vcxproj -> ...\libs\Lua.lib
  shared.vcxproj -> ...\libs\shared.lib
  proj-LogInServer.vcxproj -> ...\Server\LogInServer.exe
  proj-GameServer.vcxproj -> ...\Server\GameServer.exe
  proj-AIServer.vcxproj -> ...\Server\AIServer.exe
  BotCoreTests.vcxproj -> ...\Tests\BotCoreTests.exe
  ```
  `./tools/run-tests.sh`: `251 tests, 0 failed`.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ `selftest: 32 checks, 0 failed` (rc=0); `grep -c 'mp_regen_pass\|...\|mp_regen_zero_strict' tools/skill-check.py` = 6; `mp_ok`/`mp_off_fail`/`mp_off_warn`/`mp_no_data` korundu.
  - K2 ✔ fixture (`Msp 625`, tek düşüm `585`, `--min-n 1`): yeni sürüm boş → `WARN`, `--mp-regen 0` → `FAIL`, `--mp-regen 80` → `WARN`; **eski** sürüm (`git show gece/2026-10-02:tools/skill-check.py`) varsayılanda `FAIL` (kanıtlandı).
  - K3 ✔ `git diff gece/2026-10-02...bot/F4-43 -- tools/skill-check.py` yalnızca docstring/USAGE, `DEFAULT_MP_REGEN`, `analyze`/`build_report` imzaları, `judge_mp`, selftest, argparse/main hunk'ları (11 hunk; `recast_verdict`/`effect_verdict`/sütun/JSON değişmedi).
  - K4 ✔ `--out /tmp/skill_priest_k.txt` (gerçek MAGIC) rc=0, `diff` boş (`DIFF_EMPTY`).
  - K5 ✔ `--check` → `ok: 31 steps, 1147 bytes, 33 lines, last offset 121000 ms`; PHD cast 11, PHB cast 11, party 6, pot 2, `112703` 0; `paccept` ofsetleri 1500/4500/7500 (her biri kendi `pinvite`+1500); `112548` 3 satır (`BotWP_K`, `BotWG_K`, `self`); `112656` (satır 23) `112657` (satır 21, hedef `BotPHD_K`) sonrasında.
  - K6 ✔ `selftest: 23 checks, 0 failed` (araç değişmedi).
  - K7 ✔ `build.sh Release` rc=0 hatasız; `run-tests.sh` `251 tests, 0 failed`.
  - K8 ✔ `git diff --stat gece/2026-10-02...bot/F4-43`: yalnızca `tools/skill-check.py`, `bots/config/skill_priest_k.spec`, `bots/config/skill_priest_k.txt`, plan dosyası.
  - K9 — Claude'un çalışma zamanı turu (sunucu açma/koşu); bu turda yapılmadı.
- Plandan sapmalar ve gerekçeleri: yok. Spec §5.3 aynen yazıldı; üretici seçenekleri/varsayılanları değiştirilmedi; betik 31 adım (sınırlar içinde).
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-43` @ `a89ba48` (kod/spec commit'i `8fd65a8`; `a89ba48` yalnızca Uygulayıcı Raporu). Gece modu (`AUTO_LOOP=1`): birleştirme/push yapılmadı, döngü betiği yapar.
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `python3 tools/skill-check.py --selftest` → `selftest: 32 checks, 0 failed`, rc=0; `grep -c` 6 yeni ad = 6; `mp_ok`/`mp_off_fail`/`mp_off_warn`/`mp_no_data` kodda birer kez |
| K2 | ✔ | §7 fixture (Msp 625, düşüm 585, `--min-n 1`): varsayılan `WARN`, `--mp-regen 0` `FAIL`, `--mp-regen 80` `WARN`; eski sürüm (`git show gece/2026-10-02:tools/skill-check.py`) aynı fixture'da `FAIL`. Ek: `--mp-regen -1` → rc=2, `--mp-regen must not be negative` |
| K3 | ✔ | `git diff gece/2026-10-02...bot/F4-43 -- tools/skill-check.py` okundu: yalnızca docstring/USAGE, `DEFAULT_MP_REGEN`, `analyze`/`build_report` imzaları (sona varsayılanlı parametre), `judge_mp` (`skill-check.py:405-415`), selftest (`run_case`, 6 yeni kontrol), argparse/`main`; `judge_recast`/`judge_effect`, sütunlar, JSON anahtarları değişmedi |
| K4 | ✔ | `skill-script-gen.py bots/config/skill_priest_k.spec --out /tmp/skill_priest_k.txt` rc=0, `diff` boş |
| K5 | ✔ | `--check` → `ok: 31 steps, 1147 bytes, 33 lines, last offset 121000 ms`, rc=0; `cast BotPHD_K` 11, `cast BotPHB_K` 11, `pinvite`/`paccept` 6, `pot` 2, `112703` 0; `paccept` ofsetleri 1500/4500/7500 (her biri `pinvite` + 1500); `112548` satır 25-27 (`BotWP_K`, `BotWG_K`, `self`); `112657` (satır 21, hedef `BotPHD_K`) `112656`'dan (satır 23) önce |
| K6 | ✔ | `skill-script-gen.py --selftest` → `selftest: 23 checks, 0 failed` |
| K7 | ✔ | `./tools/build.sh Release` rc=0, çıktıda `warning`/`error` 0; `./tools/run-tests.sh` → `251 tests, 0 failed` |
| K8 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-43`: yalnızca `tools/skill-check.py`, `bots/config/skill_priest_k.spec`, `bots/config/skill_priest_k.txt`, plan dosyası |
| K9 | ✔ | Çalışma zamanı (aşağıda): party 3/3 girdi (`accept_wait` 0), `112548` 3/3 `effected`, `112656` `effected`, `mp_verdict FAIL` 0 |

- Biçim: üç dosya ASCII, LF (`file`, `grep -c $'\r'` = 0); C++/`.vcxproj`/docs/ADR değişmedi; `skill-script-gen.py` değişmedi.

**K9 çalışma zamanı ayrıntısı.** Stok önceki koşudan duruyordu (`bot-refill` atlandı). `GameServer.ini`'ye geçici `[BOT] ENABLED=1 MAX_BOTS=16 TELEMETRY=decisions`, Release sunucu (`build/bin/x86-Release/Server`), 4 bot (`BotPHD_K`, `BotPHB_K`, `BotWP_K`, `BotWG_K`) 4 sn arayla spawn; `list`: dördü zone 71'de (1270-1276, 928-944), birbirine ≤ 16 m (56 m içinde; `move` gerekmedi). `Bot_3_10_2026.log`: `loaded (31 step(s), last offset 121000 ms)`, `finished skill_priest_k: completed, 31/31 step(s) in 121095 ms (max late 109 ms)`. `live-090405.jsonl` üstünde `tools/skill-check.py --min-n 1`: 15 skill, PASS 13, WARN 2, FAIL 0, NO_DATA 0 (önceki koşu: PASS 11, WARN 2, FAIL 1, NO_DATA 1 aynı araçla). `PartyInvite` 3 (`created` 1, `sent` 2), `PartyAccept` 3 (`joined`), `accept_wait` 0. Kalan WARN'lar: `112645` (Msp 60, düşümler 20/60/60: en küçük 20 < taban 30) ve `112657` (Msp 360, tek örnek 320 < 350 bandı; taban 300 içinde). İkisi de yenilenme payı sınırı içinde kalan tek örnek sapmasıdır, bulgu değil `[Ö]`. Sunucular `stop` ile kapatıldı, botlar önce `despawn all`, `GameServer.ini` yedekten geri yüklendi (`GameServer.ini.bak-before-bot-test-20261002` ile bayt bayt aynı), `Scripts/skill_priest_k.txt` kopyası silindi. Sonuçlar `docs/05` §9.1'e işlendi.

- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. `tools/skill-check.py:405-415` `judge_mp`: `Msp` küçükken (ör. 60) `Msp/2` tavanı payı 30'a indirir; `112645`'in tek 20'lik düşümü WARN verdi. Yenilenme gözlemi +20/+40 olduğundan 60 MP'lik skill'de 40'lık yenilenme taban 30'u aşar. Hüküm WARN'dır (FAIL değil) ve `--mp-regen` ile ayarlanır; not olarak bırakıldı.
  2. `112657` ve `112820` gibi tek örnekli skill'lerde kanıt zayıf (`--min-n 1` gerekir); spec her skill için ≥ 2 örnek vermiyor (BuffType 1 kısıtı nedeniyle bilinçli). Sonraki dilimlerde ayrı oturumla tamamlanabilir.
  3. Üslup: `judge_mp` satır içi yorumu kod satırının yanında (`# regeneration cannot refund ...`); çevredeki kodla uyumlu, engel değil.
- Düzeltme talimatı: yok (`DOĞRULANDI`).
