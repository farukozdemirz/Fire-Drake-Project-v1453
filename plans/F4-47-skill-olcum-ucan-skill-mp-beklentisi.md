# F4-47: `skill-check.py` uçan Type3 skill MP beklentisi (MEC-MAG-12, cast başına 2 × `Msp`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; ADR-0018 m.9 "T-MECH-SKILL'in botla yeniden koşusu", Ek 23) |
| Branch | `bot/F4-47` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-41 (`tools/skill-check.py`), F4-43 (`--mp-regen` hükmü) — `KAPANDI`; F4-46 — `KAPANDI` (merge `f444da0`) |
| İlgili gereksinim / kabul | `docs/03` MEC-MAG-12 (uçan Type3 skill'de MP iki kez düşer), MEC-MAG-13, MEC-MAG-17; `docs/05` §7 (mage tablosu), §8 SK-01, §9 (T-MECH-SKILL-M-*) |
| Tahmini büyüklük | S (tek araç dosyası: `tools/skill-check.py`; C++ yok) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

Mage'in çekirdek skill'lerinin büyük kısmı **uçan Type3**'tür (Fire ball, Fire spear, Ice arrow, Fire burst, Ice burst ...). Sunucu bu skill'lerde MP'yi iki kez düşer: `MAGIC_FLYING`'de bir, `MAGIC_EFFECTING`'te bir kez (MEC-MAG-12, çalışma zamanında `[V]`: Fire ball `Msp 50`, MP 6021 → 5971 → 5921). `tools/skill-check.py` ise MP hükmünü her zaman tek `MAGIC.Msp`'ye göre verir (`judge_mp`, `skill-check.py:405-415`); bir mage koşusunda uçan skill'lerin tamamı yanlış `FAIL` alırdı. Bu plan aracı düzeltir: bir cast'te `CastFly` görüldüyse ve skill `Type1 == 3` ise beklenen MP düşümü `2 × Msp` olur; uçmayan cast'ler eskisi gibi `Msp` ile yargılanır. Sonraki plan (F4-48) mage skill betiğini üretip botla koşar; bu düzeltme onun önkoşuludur (ADR-0018 Ek 22 "dilim sırası").

## 2. Bağlam (okunması zorunlu)

- `docs/03` MEC-MAG-12 (satır 132): uçan Type3 skill cast başına **2 × `Msp`**; "FLYING'siz gönderilen EFFECTING bir kez düşer". MEC-MAG-13 (çift tipli `{3, 4}`: MP yine bir kez, uçansa iki kez), MEC-MAG-17 (uçan alan: 150 + 150).
- `tools/skill-check.py` (okuyup anla, tamamı 872 satır):
  - `new_stat()` `:156-169` (`"deltas": []` `:166`).
  - `analyze()` `:176`: `CastFly` submit'i açık kaydı işaretler (`record["flying"] = True`, `:243-247`); kayıt kapanınca `stat["deltas"].append(mp_before - mp_after)` (`:304-306`, yalnızca `effected`/`missed` ve iki MP alanı da varsa).
  - `build_report()` `:321-402`: `mp_exp = magic_row["msp"]` (`:328`), `judge_mp(mp_exp, deltas, ...)` (`:337`), satır sözlüğü `:372-396`.
  - `judge_mp()` `:405-415`: `regen = min(mp_regen, mp_exp // 2)`, `floor = mp_exp - regen`; `top > mp_exp + tol` veya `top < floor` ⇒ `FAIL`; `top < mp_exp - tol` veya `min < floor` ⇒ `WARN`; yoksa `PASS`; `mp_exp is None` ya da örnek `< min_n` ⇒ `NO_DATA`.
  - `render_markdown()` `:456-505` (tablo başlığı `:462-470`, boş satır `:471`, hücreler `:472-499`), `run_selftest()` `:527-815` (`flying_chain` durumu `:699-710`), şu an `selftest: 32 checks, 0 failed`.
- MAGIC satırı (`parse_magic_lines`, `:87-109`) `type1`, `type2` alanlarını taşır; bu plan `type1`'i kullanır.
- Telemetri `type` değerleri: `CastStart`, `CastFly`, `CastEffect` (`GameServer/Bot/ActionExecutor.cpp:594-595`); `ACTION_SUBMIT` `CastStart`'ta `mp` (cast öncesi), `ACTION_RESULT` `CastEffect`'te `mp_after` taşır (F4-41). Uçan akışta tek kayıt = `CastStart → CastFly → CastEffect` (`flying_chain` durumu bunu zaten sınar); `mp_before` ilk `CastStart`'tan, `mp_after` son `CastEffect` sonucundan gelir, yani fark **iki düşümün toplamıdır**.

**Karar (Claude):**

1. Uçan sayılan kayıt: kayıtta `flying == True` (yani `CastFly` submit'i görüldü). `CastTime == 0` olan uçan skill'de açık kayıt olmayabilir ve `CastFly` yok sayılır (`record is None`); mage'in uçan skill'lerinin hepsinin `CastTime`'ı > 0'dır (`docs/05` §7), bu durum kapsam dışıdır.
2. Çift sayım yalnızca `MAGIC.Type1 == 3` skill'lerde uygulanır (`{3, 0}` ve `{3, 4}`). `Type1 != 3` bir skill'de (örn. okçu `Type1 2`: sunucu MP'yi EFFECTING'te hiç düşmez, MEC-MAG-12) uçan kayıt **eskisi gibi** `Msp` ile yargılanır; bu davranış değişmez.
3. Uçan örneklerde yenilenme payı: `regen = min(mp_regen, Msp // 2)` (**tek** maliyetin yarısı), `floor = 2 × Msp − regen`. Gerekçe: iki düşümden birinin eksik olması (yani tek `Msp` düşümü) her zaman `floor`'un altında kalmalı (Fire ball `Msp 50`: `floor = 100 − 25 = 75`, tek düşüm 50 < 75 ⇒ `FAIL`). `mp_exp // 2` (= `Msp`) kullanılsaydı `floor = 2×Msp − 50 = 50`, tek düşüm 50 ⇒ `WARN`'a düşerdi.
4. Aynı skill'in uçan ve uçmayan örnekleri karışık olabilir: iki grup ayrı yargılanır, sonuç **en kötüsüdür** (`FAIL` > `WARN` > `PASS`); bir grup `NO_DATA` ise yok sayılır; ikisi de `NO_DATA` ise `NO_DATA`. `--min-n` her grup için ayrı uygulanır.
5. Rapor satırı **eklemeli** değişir (mevcut anahtarlar ve anlamları korunur): `mp_delta_min/med/max` tüm örnekler üzerindedir (uçan + uçmayan, ham değerler); yeni alanlar `flying_n` (MP verisi olan uçan örnek sayısı) ve `mp_exp_flying` (`Type1 == 3` ise `2 × Msp`, değilse `None`).

## 3. Kapsam

**Yapılacaklar**

1. `tools/skill-check.py`: `new_stat()` yeni `"fly_deltas": []`; `analyze()` kayıt kapanırken uçan kaydın MP farkını `fly_deltas`'a, diğerlerini `deltas`'a yazar; `judge_mp()` isteğe bağlı `regen_base` parametresi; yeni `judge_mp_split()`; `build_report()` yeni alanlar ve hüküm; `render_markdown()` iki yeni sütun; modül docstring'i ve `USAGE` metni (uçan kural); `run_selftest()` §5.3'teki 16 yeni denetim.

**Kapsam dışı (yapılmayacak)**

- C++ (`GameServer/`, `BotCore/`, `Tests/`), `.vcxproj`, `tools/skill-script-gen.py`, başka `tools/*`, `bots/config/*`, `db/*`, `docs/`, ADR: **değişmez** (docs/ADR'yi Claude yazar).
- Telemetri biçimi, `MAGIC_QUERY` (yeni sütun eklenmez; yalnızca mevcut `type1` kullanılır), `--mp-tol`/`--ms-tol`/`--min-n`/`--mp-regen` varsayılanları ve anlamları, recast ve etki hükümleri (`judge_recast`, `effect_verdict`) **değişmez**.
- Mage skill betiği ve gerçek koşu (F4-48).
- `CastTime == 0` uçan skill, okçu `Type2` uçan skill ölçümü, uçuş süresi/mesafe modeli.
- Mevcut selftest denetimlerini silme/gevşetme; yalnızca ekleme.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/skill-check.py` | değiştir | §5 |

Plan dosyası dahil 2 dosya. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

1. `git switch -c bot/F4-47 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (`AGENTS.md` §4; bu plan sunucu açmaz).
2. Başlangıç ölçümü: `python3 tools/skill-check.py --selftest` ⇒ `selftest: 32 checks, 0 failed` (değilse dur ve raporla).

### 5.2 Araç değişikliği

`tools/skill-check.py` (Python 3, yalnızca standart kütüphane; yorumlar İngilizce, ASCII):

1. `new_stat()`: `"fly_deltas": []` ekle (`"deltas"` yanına).
2. `analyze()`, kayıt tamamlama döngüsünde (`:304-306`): MP farkı hesaplanınca `record.get("flying")` doğruysa `stat["fly_deltas"]`'e, değilse `stat["deltas"]`'e ekle (koşullar aynı: `outcome in ("effected", "missed")` ve iki MP alanı da `None` değil). `CastFly` işaretleme mantığına dokunma.
3. `judge_mp()`: imzayı `judge_mp(mp_exp, deltas, mp_tol, mp_regen, min_n, regen_base=None)` yap; `regen = min(mp_regen, (mp_exp if regen_base is None else regen_base) // 2)`. `regen_base` verilmediğinde davranış **birebir aynıdır** (mevcut selftest durumları bunu korur).
4. Yeni işlev (isim sabit değil, davranış sabit):

```python
def judge_mp_split(magic_row, deltas, fly_deltas, mp_tol, mp_regen, min_n):
    """Judges plain casts against Msp and flying Type3 casts against 2 * Msp (MEC-MAG-12)."""
    msp = magic_row["msp"] if magic_row else None
    if magic_row is None or magic_row.get("type1") != 3:
        # no doubling: every sample is judged against Msp (flying or not)
        return judge_mp(msp, deltas + fly_deltas, mp_tol, mp_regen, min_n)
    plain = judge_mp(msp, deltas, mp_tol, mp_regen, min_n)
    flying = judge_mp(None if msp is None else 2 * msp, fly_deltas, mp_tol, mp_regen, min_n,
                      regen_base=msp)
    parts = [verdict for verdict in (plain, flying) if verdict != "NO_DATA"]
    if not parts:
        return "NO_DATA"
    for verdict in ("FAIL", "WARN"):
        if verdict in parts:
            return verdict
    return "PASS"
```

5. `build_report()`: `all_deltas = stat["deltas"] + stat["fly_deltas"]`; `mp_min/med/max` `all_deltas` üzerinden (ham, eski anahtarlarla aynı alanlar); `mp_verdict = judge_mp_split(magic_row, stat["deltas"], stat["fly_deltas"], mp_tol, mp_regen, min_n)`; `mp_n` = `len(all_deltas)`; satır sözlüğüne `"flying_n": len(stat["fly_deltas"])` ve `"mp_exp_flying": 2 * mp_exp if (magic_row and magic_row.get("type1") == 3 and mp_exp is not None) else None`. Mevcut anahtarlar kalır.
6. `render_markdown()`: başlık satırında `mp_exp` sütunundan hemen sonra `mp_exp_fly | fly_n` ekle; ayırıcı satır ve `(none)` satırı ve hücre listesi aynı sayıda sütunla güncellenir (21 → 23 sütun); hücreler `format_number(row["mp_exp_flying"])`, `str(row["flying_n"])`.
7. Docstring/`USAGE`: bir paragraf: "A cast that went through CastFly on a Type1 == 3 skill is judged against 2 * Msp (MEC-MAG-12); regeneration allowance is min(--mp-regen, Msp // 2); other casts keep the single Msp expectation."

### 5.3 Birim denetimleri (`run_selftest()` içine, mevcut denetimlerden sonra, `if failures:` öncesi)

Yerel yardımcılar (mevcut `submit`/`result`/`run_case` kullanılır): bir uçan cast = `submit("CastStart", d, t=T, skill=S, mp=B)`, `result("CastStart", d, t=T+10, reason="casting")`, `submit("CastFly", d+1, t=T+100, skill=S)`, `result("CastFly", d+1, t=T+110, reason="flying")`, `submit("CastEffect", d+2, t=T+1100, skill=S)`, `result("CastEffect", d+2, t=T+1110, reason="effected", code=0, mp_after=B-drop)`; her cast için `T` 10000 ms artar, `d` benzersizdir, `B = 6000`. Bir uçmayan cast yalnızca `submit("CastEffect", ..., mp=B)` + `result("CastEffect", ..., mp_after=B-drop)` çiftidir. Yerel MAGIC sözlüğü (`fly_magic`): `110515` `{"name": "Fire ball", "msp": 50, "cast_time": 1500, "recast_time": 43, "range": 78, "type1": 3, "type2": 0}`; `110099` `{"name": "Arrow", "msp": 50, "cast_time": 1500, "recast_time": 0, "range": 78, "type1": 2, "type2": 1}` (diğer anahtarlar eksiksiz yazılır).

Eklenecek **16** denetim (adlar sabit; hepsi `check(name, condition)`):

| # | Ad | Durum | Beklenen |
|---|---|---|---|
| 1 | `flying_mp_pass` | `110515`, 3 uçan cast, düşümler `[100, 100, 100]` | `mp_verdict == "PASS"` |
| 2 | `flying_mp_fields` | aynı rapor | `row["mp_exp"] == 50`, `row["mp_exp_flying"] == 100`, `row["flying_n"] == 3` |
| 3 | `flying_single_drop_fail` | 3 uçan cast, `[50, 50, 50]` | `mp_verdict == "FAIL"` (`floor` 75) |
| 4 | `flying_regen_pass` | 3 uçan cast, `[100, 80, 100]` | `PASS` (`80 >= floor 75`) |
| 5 | `flying_below_floor_warn` | 3 uçan cast, `[100, 70, 100]` | `WARN` |
| 6 | `flying_over_fail` | 2 uçan cast, `[100, 160]`, `min_n=2` | `FAIL` |
| 7 | `flying_no_data` | 2 uçan cast, `[100, 100]`, `min_n=3` | `NO_DATA` |
| 8 | `flying_regen_zero_warn` | 3 uçan cast `[100, 80, 100]`, `mp_regen=0` | `WARN` |
| 9 | `mixed_pass` | aynı skill: 3 uçan `[100]×3` + 3 uçmayan `[50]×3` | `mp_verdict == "PASS"` ve `flying_n == 3` |
| 10 | `mixed_fields` | aynı rapor | `mp_delta_min == 50` ve `mp_delta_max == 100` |
| 11 | `mixed_plain_wrong_fail` | 3 uçan `[100]×3` + 3 uçmayan `[100]×3` | `FAIL` (uçmayan grup `Msp 50` için 100 düşmüş) |
| 12 | `plain_unchanged` | `110515`, 3 uçmayan cast `[50, 50, 50]` | `PASS` ve `flying_n == 0` |
| 13 | `flying_type2_single` | `110099` (`type1 2`), 3 uçan cast `[50, 50, 50]` | `PASS`, `mp_exp_flying is None`, `flying_n == 3` |
| 14 | `flying_unknown_skill` | MAGIC'te olmayan skill `999998`, 3 uçan cast `[100]×3` | `mp_exp is None`, `mp_exp_flying is None`, `mp_verdict == "NO_DATA"` (tek `check`, `and` ile) |
| 15 | `render_columns` | `render_markdown(rapor)`: başlık, ayırıcı ve ilk veri satırı | üçünde de `\|` sayısı eşit; başlıkta `mp_exp_fly` ve `fly_n` geçer |
| 16 | `render_json_fields` | `json.loads(render_json(rapor))["skills"][0]` | `"flying_n"` ve `"mp_exp_flying"` anahtarları var |

Mevcut 32 denetim aynen geçmeye devam eder (özellikle `mp_*`, `flying_chain_*`, `json_out`); toplam **48**.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/skill-check.py --selftest` çıkış 0 ve son satır **`selftest: 48 checks, 0 failed`** (32 eski + 16 yeni).
- [ ] K2: §5.3'teki 16 denetimin adı `tools/skill-check.py` içinde geçer: `for n in flying_mp_pass flying_mp_fields flying_single_drop_fail flying_regen_pass flying_below_floor_warn flying_over_fail flying_no_data flying_regen_zero_warn mixed_pass mixed_fields mixed_plain_wrong_fail plain_unchanged flying_type2_single flying_unknown_skill render_columns render_json_fields; do grep -c "\"$n\"" tools/skill-check.py; done` her satırda ≥ 1.
- [ ] K3: mevcut denetim adları (`mp_ok`, `mp_off_fail`, `mp_off_warn`, `mp_no_data`, `mp_regen_pass`, `mp_regen_single_warn`, `mp_below_floor_fail`, `mp_over_fail`, `mp_regen_min_warn`, `mp_regen_zero_strict`, `flying_chain_count`, `flying_chain_effect`) hâlâ `tools/skill-check.py`'de var ve `git diff gece/2026-10-02...bot/F4-47 -- tools/skill-check.py` bu satırlardan **hiçbirini silmez/değiştirmez** (`git diff ... | grep '^-' | grep -c 'check("'` = 0).
- [ ] K4: CLI uçtan uca (selftest dışında): `python3 tools/skill-check.py <geçici.jsonl> --magic <geçici-magic.txt> --min-n 3 --json` çıktısı geçerli JSON'dur; §5.3 durum 1'in kaydı (3 uçan Fire ball, düşüm 100, `--magic` dosyası `110515|Fire ball|50|15|43|78|3|0`) için satırda `"mp_verdict": "PASS"`, `"flying_n": 3`, `"mp_exp_flying": 100`, `"mp_exp": 50`; aynı kayıt **`git stash` ile eski araçla** `"mp_verdict": "FAIL"` verirdi (uygulayıcı yalnızca yeni araçla koşar, Uygulayıcı Raporu'na JSON satırını yapıştırır; eski araç davranışı `judge_mp(50, [100]*3, ...)` `top 100 > 50 + 10` ⇒ `FAIL`).
- [ ] K5: `python3 tools/skill-script-gen.py --selftest` hâlâ `selftest: 23 checks, 0 failed`.
- [ ] K6: `./tools/build.sh Release` hatasız biter (C++ değişmedi; yeni uyarı yok) ve `./tools/run-tests.sh` son satırı `251 tests, 0 failed` (ya da fazlası).
- [ ] K7: `git diff --stat gece/2026-10-02...bot/F4-47` yalnızca `tools/skill-check.py` ve plan dosyasını gösterir; plan dosyasında yalnızca `Durum` ve Uygulayıcı Raporu değişir.
- [ ] K8: araç ASCII ve LF kalır: `file tools/skill-check.py` "ASCII text" (CRLF içermez); `git ls-files --eol tools/skill-check.py` `i/lf`.
- Derleme sonucu (`tools/build.sh Release` son satırları) Uygulayıcı Raporu'na yapıştırılır.

## 7. Doğrulama komutları

```bash
python3 tools/skill-check.py --selftest                    # selftest: 48 checks, 0 failed
python3 tools/skill-script-gen.py --selftest               # selftest: 23 checks, 0 failed
for n in flying_mp_pass flying_mp_fields flying_single_drop_fail flying_regen_pass flying_below_floor_warn flying_over_fail flying_no_data flying_regen_zero_warn mixed_pass mixed_fields mixed_plain_wrong_fail plain_unchanged flying_type2_single flying_unknown_skill render_columns render_json_fields; do printf '%s ' "$n"; grep -c "\"$n\"" tools/skill-check.py; done

# K4: build a throwaway telemetry file (3 flying casts of Fire ball, MP 6000 -> 5900) and a one-line MAGIC file
python3 - <<'EOF'
import json
rows = []
for i in range(3):
    t, d = 10000 * i, 10 * i
    rows += [
        {"ev": "ACTION_SUBMIT", "t": t, "bot": 1, "type": "CastStart", "skill": 110515, "decision_id": d, "mp": 6000},
        {"ev": "ACTION_RESULT", "t": t + 10, "bot": 1, "type": "CastStart", "decision_id": d, "ok": True, "reason": "casting", "code": 0},
        {"ev": "ACTION_SUBMIT", "t": t + 100, "bot": 1, "type": "CastFly", "skill": 110515, "decision_id": d + 1},
        {"ev": "ACTION_RESULT", "t": t + 110, "bot": 1, "type": "CastFly", "decision_id": d + 1, "ok": True, "reason": "flying", "code": 0},
        {"ev": "ACTION_SUBMIT", "t": t + 1100, "bot": 1, "type": "CastEffect", "skill": 110515, "decision_id": d + 2},
        {"ev": "ACTION_RESULT", "t": t + 1110, "bot": 1, "type": "CastEffect", "decision_id": d + 2, "ok": True, "reason": "effected", "code": 0, "mp_after": 5900},
    ]
open("/tmp/fly_k4.jsonl", "w").write("\n".join(json.dumps(r) for r in rows) + "\n")
open("/tmp/fly_k4_magic.txt", "w").write("110515|Fire ball|50|15|43|78|3|0\n")
EOF
python3 tools/skill-check.py /tmp/fly_k4.jsonl --magic /tmp/fly_k4_magic.txt --min-n 3 --json | python3 -c "import json,sys; r=json.load(sys.stdin)['skills'][0]; print({k: r[k] for k in ('mp_exp','mp_exp_flying','flying_n','mp_delta_med','mp_verdict')})"
# expected: {'mp_exp': 50, 'mp_exp_flying': 100, 'flying_n': 3, 'mp_delta_med': 100, 'mp_verdict': 'PASS'}

file tools/skill-check.py
git ls-files --eol tools/skill-check.py
./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-47
```

(Uygulayıcı çıktıları Uygulayıcı Raporu'na gerçek hâliyle yapıştırır.)

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. Araç ASCII, LF; yorumlar İngilizce.
- Kişisel veri: araç yalnızca `MAGIC`'i okur (değişmez); bu planda gerçek DB/sunucu/telemetri dosyası kullanılmaz (geçici sentetik dosyalar `/tmp`'de).
- Geriye uyumluluk: mevcut JSON anahtarları ve Markdown sütun **adları** korunur; yalnızca iki yeni sütun/anahtar eklenir. `--json` tüketicisi yoksa da anahtarları kaldırma/yeniden adlandırma.
- Uçan MP kuralı yalnızca `Type1 == 3`'e uygulanır; `CastFly` işaretini üreten telemetri mantığını değiştirme (`:243-247`).
- Uygulayıcı "iyileştirme" olarak `MAGIC_QUERY`'ye `FlyingEffect` ekleyip sunucu tablosundan uçuşu çıkarmaya **çalışmaz**: uçuş telemetrideki `CastFly` kaydından anlaşılır (bot gerçekten uçuş paketi gönderdi mi).
- Sunucu çalıştırma ve mage koşusu bu planın işi değildir (F4-48, Claude K8/K9).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-47` — `<kısa-sha> [F4-47] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır) ve `./tools/run-tests.sh` son satırı:
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …
- `git diff --stat gece/2026-10-02...bot/F4-47` (gerçek çıktı):
  ```
  …
  ```

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-47` @ `<sha>`
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
