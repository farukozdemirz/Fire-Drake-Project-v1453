# F8-01: Maç sonuç değerlendiricisi: `tools/bot-outcome-eval.py` (ADR-0031-DEG kazanma kuralları, telemetriden, `--selftest`)

| Alan | Değer |
|---|---|
| Durum | DÜZELTME GEREKLİ |
| Faz | F8 — Değerlendirme ve 8v8 (`docs/17` §2; paralel hat `nav`, değerlendirme/analiz araçları: `docs/17` §1 "analiz araçları her fazla paralel") |
| Branch | `bot/F8-01 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F3-02, F3-06 `KAPANDI` (`MATCH_START`/`MATCH_END`, `tools/bot-telemetry-report.py`); F5-11 `KAPANDI` (`gece/2026-10-02-nav`, merge `e211ae3`). Sunucu tarafı `DEATH`/`DAMAGE` emisyonu **yoktur** (§2); araç belgelenmiş girdi sözleşmesine göre yazılır, gerçek log gelince aynen çalışır |
| İlgili gereksinim / kabul | ADR-0031-DEG (+ Ek F8-01), `docs/15` §6b, `docs/16` MET-OUT-01/05, §3.2/§3.3, T-IGT-EVAL-01 (altyapı) |
| Tahmini büyüklük | S (2 yeni dosya, hepsi `tools/` altında; `GameServer/`, `BotCore/`, `Tests/`, `docs/15` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü, hat `nav`, konu sırası dilim 2) |

---

## 1. Amaç

`docs/15` §6b ve ADR-0031-DEG, bir maçın sonucunu (`win_a`/`win_b`/`draw`/`invalid`/`no_result`) üç `win_rule` türüyle tanımlar ama bunu hesaplayan hiçbir şey yoktur. Bu plan, bir maçın telemetri olaylarından sonucu **ADR kurallarıyla** hesaplayan, eşikleri komut satırı parametresi olan kalıcı bir Python aracı yazar. ADR ve `docs/15` §6b'deki tüm örnek tablolar `--selftest` vakalarıdır; böylece kural yanlış okunursa araç kendini yakalar. Aynı araç, sunucu tarafı (ScenarioRunner `win_rule`, `DEATH`/`DAMAGE` emisyonu) yazıldığında bağımsız bir **oracle** olacaktır.

```
python3 tools/bot-outcome-eval.py PATH [PATH ...] [--win-rule R] [--duration-sec N] [--win-margin N]
        [--early-end-margin N] [--engage-timeout-sec N] [--tick-ms N] [--json] [--strict]
python3 tools/bot-outcome-eval.py --selftest
```

Araç **yalnızca okur** (CLI modunda hiçbir dosya yazmaz), sunucuya/DB'ye bağlanmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0031-DEG-senaryo-kazanma-kurali.md` (tamamı): ortak tanımlar (kill, `fark`, `engage`, `engage_timeout_sec`), üç tür, örnek tablolar. **Ek F8-01** (bu planla birlikte yazıldı): girdi sözleşmesi ve ADR'nin tanımlamadığı noktalar için `[A]` kararları; bu planın §5'i onu uygular, çelişirse **ADR Ek F8-01** kazanır ve durup soru yazılır.
- `docs/15` §6b (satır ~282–304): aynı tablolar ve `wipe_first` örnekleri. `docs/16` §3.2 (olay tipleri), **§3.3 "Değerlendirici girdi sözleşmesi (F8-01)"** maddesi (alan adları), MET-OUT-01/05 (satır ~210–214).
- `GameServer/Bot/Telemetry.cpp:336-380` (`BeginMatch`: `MATCH_START` alanları `ts_utc`, `scenario`, `seed`, `run` + `extraFields`) ve `:395-450` (`EndMatch`: `ts_utc`, `duration_ms`, `result`, `dropped_*` + `extraFields`); `:536` her kayıt `{"t":<ms>,"match":"..","bot":<int>,...`. `extraFields` ileride `win_rule`/`team_a`/`team_b` taşıyabilir; **bugün sunucu `DEATH`, `DAMAGE`, `TEST_TELEPORT`, `SETUP_FAIL` yazmıyor** (`grep` ile doğrulandı: `GameServer/Bot/` içinde yok). Bu planın işi sunucuyu değiştirmek **değildir** (§3).
- Üslup örneği: `tools/bot-telemetry-report.py` (JSONL okuma, `--selftest`, `--strict`, `--json`), `tools/nav-regress.py` (PASS/FAIL, çıkış kodları). Aynı dil ve üslup: yalnızca standart kütüphane, ASCII, LF.

## 3. Kapsam

**Yapılacaklar**

1. `tools/bot-outcome-eval.py` (yeni): §5'teki sözleşmeye göre maçları gruplar, sonucu hesaplar, `OUTCOME`/`SUMMARY` satırlarını (ya da `--json`) yazar.
2. `tools/bot-outcome-eval/sample.jsonl` (yeni): §5.7'deki 4 maçlık örnek girdi (CLI'nin gerçek dosyayla koşusu ve belgeleme için; `--selftest` bu dosyaya **bağlı değildir**).
3. `--selftest`: §5.6'daki vakalar (≥ 50), her biri `PASS <ad>` / `FAIL <ad>: ...`; son satır `SELFTEST PASS n=<N>` ya da `SELFTEST FAIL failed=<M> of <N>` (çıkış 0/1).

**Kapsam dışı (yapılmayacak)**

- `GameServer/*`, `AIServer/*`, `shared/*`, `BotCore/*`, `Tests/*`, `docs/*`, `plans/README.md`, `tools/bot-telemetry-report.py` değişmez. `ScenarioRunner`'a `win_rule` eklemek, sunucunun `DEATH`/`DAMAGE`/`MATCH_START.team_*` yazması, "respawn kapalı" modu: **ayrı sunucu planı** (F8 sunucu tarafı).
- Toplulaştırma: Wilson/bootstrap/SPRT, kazanma oranı (MET-OUT-01), `fark` ortalaması/standart sapması, pilot kalibrasyon raporu (`docs/15` §6b), taraf değişimi eşleştirme, Elo. Bunlar sonraki dilimdir; bu araç yalnızca **maç başına sonuç** ve sonuç sayaçlarını verir.
- `THIRD_PARTY` geçersizlik nedeni (üçüncü taraf müdahalesini tanıyan olay şeması yok).
- `--out`, dosya yazma, Markdown raporu, CI/`run-tests.sh` bağlantısı.
- Sunucu çalıştırmak, `tools/run-servers.sh`, DB bağlantısı (paralel hat).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/bot-outcome-eval.py` | yeni | LF (`*.py text eol=lf`), yalnızca ASCII, standart kütüphane, Python 3.8 uyumlu (`match` deyimi yok), `#!/usr/bin/env python3` |
| `tools/bot-outcome-eval/sample.jsonl` | yeni | §5.7'deki satırlar **aynen**; LF; ASCII |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. `.vcxproj` değişmez (derlemeye girmez).

## 5. Uygulama adımları

1. `git switch -c bot/F8-01 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`. `tools/bot-telemetry-report.py` içindeki JSONL okuma ve `--selftest` iskeletine bak (kopyalama yok; aynı üslup).

2. **Girdi sözleşmesi** (ADR Ek F8-01 madde 1, `docs/16` §3.3). Her satır tek JSON nesnesidir; `t` (int, ms), `match` (str), `ev` (str) zorunludur. Kullanılan olaylar:

   | `ev` | Kullanılan alanlar |
   |---|---|
   | `MATCH_START` | `t`; `team_a`, `team_b` (boş olmayan, ayrık `int` listeleri = `bot` kimlikleri); isteğe bağlı `win_rule`, `duration_sec`, `win_margin`, `early_end_margin`, `engage_timeout_sec` |
   | `DAMAGE` | yalnızca `t` (maçtaki **ilk** `DAMAGE` kaydı = `engage`; `t >= MATCH_START.t`) |
   | `DEATH` | `bot` (ölen, `int`), `killer` (öldüren birim kimliği, `int`; yok ya da `-1` = bilinmeyen/çevre) |
   | `RESPAWN` | `bot` |
   | `TEST_TELEPORT`, `SETUP_FAIL` | yalnızca varlığı |
   | `MATCH_END` | `t`, `result` (`aborted` ise geçersiz; başka değer yok sayılır) |

   Başka tüm `ev` değerleri ve ek alanlar yok sayılır. `match == "-"` olan satırlar ve `ev == "SELFTEST"` satırları atlanır. Satırlar **maç kimliğine göre** gruplanır (birden çok dosya ve dosya başına birden çok maç olabilir; çıktı sırası = maç kimliğinin ilk görüldüğü sıra) ve her grup içinde `t`'ye göre **kararlı** sıralanır (eşit `t`'de dosya sırası korunur). Bozuk JSON satırı, `t`/`match`/`ev` eksikliği ya da yanlış türü: `ERROR` satırı `<dosya>:<satır>: <mesaj>` ve çıkış 2 (o dosyanın diğer satırları okunmaya devam eder).

3. **Parametreler** (öncelik: komut satırı > `MATCH_START` alanı > varsayılan). Doğrulama (ADR "Yapılandırılabilirlik"): `win_rule` ∈ {`killdiff_timed`, `wipe_first`, `timed_score`} (varsayılan `killdiff_timed`); `win_margin` tamsayı 1..8 (2); `early_end_margin` tamsayı, 0 ya da ≥ `win_margin` (0 = kapalı); `engage_timeout_sec` tamsayı 1..3600 (60) `[A]` üst sınır; `duration_sec` tamsayı 1..3600 `[A]` üst sınır, varsayılan: `killdiff_timed` için takım boyutu (`max(len(team_a), len(team_b))`) ≥ 6 ise 300, değilse 120 (ADR: EVAL-8v8 300, 2v2..5v5 120; 6–7 oyuncu ADR'de tanımsız, `[A]`), `wipe_first` ve `timed_score` için 300 (`timed_score` varsayılanı `[A]`); `--tick-ms` 1..1000 (100, ADR "aynı tick ≤ 100 ms"). `bool` değerler tamsayı sayılmaz. Komut satırı değeri geçersizse kullanım hatası (çıkış 2, hiçbir maç işlenmez); `MATCH_START` değeri ya da birleşik sonuç geçersizse o maç için `ERROR` satırı (çıkış 2'ye katkı). `wipe_first`'te `win_margin`/`early_end_margin` kullanılmaz (doğrulanır ama etkisizdir).

4. **Hesap** (her maç için; zaman değerleri maç başlangıcına göre `*_ms = t − MATCH_START.t`):

   a. `SETUP_FAIL` olayı varsa: sonuç `invalid`, neden `SETUP_FAIL`; `MATCH_START` yoksa `rule=-`. (`docs/15` §6a: kurulum başarısızsa `MATCH_START` yerine yazılır.) Aksi halde `MATCH_START` yoksa `ERROR` (`no MATCH_START`); birden çok varsa `ERROR` (`multiple MATCH_START`); takım listeleri eksik/boş/kesişiyorsa `ERROR`.

   b. `obs_end` = son `MATCH_END.t`, yoksa grubun en büyük `t`'si. `engage_t` = `t >= start_t` olan ilk `DAMAGE`'in `t`'si (yoksa yok). `window_end` = `engage_t + duration_sec*1000`.

   c. **Geçersizlik nedenleri** (hepsi toplanır, bu öncelik sırasıyla yazılır): `SETUP_FAIL` > `ABORTED` (`MATCH_END.result == "aborted"` `[A]`) > `TEST_TELEPORT` (maçta, `t >= start_t`, herhangi biri; docs/16 §3.2) > `NO_ENGAGE` (`engage_t` yok **ve** `obs_end − start_t >= engage_timeout_sec*1000`; ya da `engage_t − start_t > engage_timeout_sec*1000`; sınır dahil = geçerli) > `RESPAWN_IN_WIPE_FIRST` (`wipe_first` iken `[engage_t, bitiş]` aralığında bir takım üyesinin `RESPAWN`'ı `[A]`; ADR "respawn on olursa doğrulama reddeder") > `TRUNCATED` (**yalnızca başka hiçbir neden yokken**; e/f adımlarındaki sonlanma koşullarından hiçbiri gerçekleşmedi — erken bitiş/wipe yok — **ve** (`engage_t` yok ya da `obs_end + tick_ms < window_end`) `[A]`: günlük maç bitmeden kesilmiş; `NO_ENGAGE` zaten varsa `TRUNCATED` eklenmez). Neden listesi boş değilse sonuç `invalid`; sayısal alanlar `-`, `engage_ms`/`end_ms`/`end` `-`.

   d. **Kill:** `engage_t <= t <= bitiş` aralığında bir `DEATH` ki `bot` bir takımın üyesi ve `killer` **karşı** takımın üyesi. `killer` kendisi, aynı takım, takımlarda olmayan kimlik (canavar/kule), `-1` ya da yok ise kill **sayılmaz**. `RESPAWN` olayı `killdiff_timed`/`timed_score`'da yok sayılır.

   e. **`killdiff_timed`:** olayları sırayla işle; `k_a`, `k_b` artar, `fark = k_a − k_b`. `early_end_margin > 0` ve `|fark| >= early_end_margin` olduğu **ilk** kill'de maç biter (`end=early_end`, `bitiş` = o kill'in `t`'si; sonraki kill'ler sayılmaz). Aksi halde `bitiş = window_end` (`end=duration`). Sonuç `judge_killdiff(k_a, k_b, win_margin)`: `fark >= win_margin` → `win_a`; `fark <= −win_margin` → `win_b`; aksi `draw`. Aralık dışı (`t > window_end`) olaylar yok sayılır; `t == window_end` olan sayılır.

   f. **`wipe_first`:** canlı = takım üyesi ve henüz `DEATH` almamış (takımlarda olmayan `bot` yok sayılır). `DEATH`'leri sırayla işle (`window_end`'e kadar). Bir takımın canlısı **ilk kez** 0 olduğunda `t0` = o `DEATH`'in `t`'si: `t0 + tick_ms` ve `window_end`'e kadar gelen sonraki `DEATH`'ler de uygulanır; ikisi de 0 canlıdaysa `draw`, değilse 0 canlıdaki takım kaybeder (`win_a`/`win_b`), `end=wipe`, `bitiş = t0`. **1v1** (`len(team_a) == len(team_b) == 1`) özel durumu: **ilk ölen kaybeder**; iki ölüm aynı `t`'de ise `draw` (`tick_ms` penceresi uygulanmaz) `[A]`. Wipe olmadan `window_end` gelirse: canlısı fazla takım galip, eşitse `draw`, `end=alive_count`, `bitiş = window_end`. `alive_a`/`alive_b` = `bitiş` anındaki canlı sayıları. Kill sayaçları (d) yine hesaplanıp yazılır (`bitiş`'e kadar).

   g. **`timed_score`:** kill sayaçları `killdiff_timed` gibi (erken bitiş yok, `bitiş = window_end`), sonuç `no_result`, `end=duration`.

5. **Çıktı.** Her maç için tek satır (değerler boşluksuz; `-` = tanımsız):

   ```
   OUTCOME match=<id> rule=<rule|-> result=<win_a|win_b|draw|invalid|no_result> k_a=<n|-> k_b=<n|-> diff=<+n|0|-n|-> alive_a=<n|-> alive_b=<n|-> engage_ms=<n|-> end_ms=<n|-> end=<duration|early_end|wipe|alive_count|-> reasons=<NEDEN[,NEDEN...]|->
   ```

   `alive_*` yalnızca `wipe_first`'te sayıdır, diğerlerinde `-`. `ERROR` olan maç: `OUTCOME match=<id> ERROR <mesaj>`. En sonda `SUMMARY n=<N> win_a=.. win_b=.. draw=.. invalid=.. no_result=..` (`N` = `ERROR` olmayan maç sayısı; `ERROR` varsa satır sonuna ` error=<E>` eklenir). `--json`: tek JSON nesnesi `{"matches":[{match, rule, result, k_a, k_b, diff, alive_a, alive_b, engage_ms, end_ms, end, reasons:[...], error:null|str}], "summary":{n, win_a, win_b, draw, invalid, no_result, error}}` (`-` yerine `null`). **Çıkış kodu:** `0` tamam; `1` yalnızca `--strict` ve en az bir `invalid`; `2` kullanım hatası, okunamayan yol/dosya, bozuk satır ya da `ERROR` maç (2, 1'e baskındır). Yol olarak klasör verilirse içinde özyinelemeli `*.jsonl` taranır (sıralı).

6. **`--selftest` vakaları** (≥ 50; her biri sözleşmeyi **olay düzeyinde** sınar: yardımcı bir `build(...)` işlevi `MATCH_START` `t=0`, ilk `DAMAGE` `t=1000`, kill'leri 1000 ms aralıkla `t=2000`'den itibaren, `MATCH_END` `t=1000+duration*1000` üretir; çıkış kodu/dosya vakaları `tempfile` ile gerçek dosya yazar). Vaka adları aşağıdadır ve **aynen** kullanılır (K2 bunları `grep` eder):

   | Grup | Vaka adı → beklenen sonuç |
   |---|---|
   | ADR `killdiff_timed` (8v8, 300 sn, `win_margin=2`): saf `judge_killdiff` **ve** olay düzeyi | `kd_14_11` `win_a`; `kd_10_08` `win_a`; `kd_09_08` `draw`; `kd_08_08` `draw`; `kd_08_09` `draw`; `kd_07_09` `win_b`; `kd_03_12` `win_b`; `kd_00_00_damage` `draw` (hasar var); `kd_00_00_nodamage` `invalid` `NO_ENGAGE` (obs_end ≥ 60000) |
   | `win_margin=3` (`--win-margin` / `MATCH_START` alanı) | `kdm3_14_11` `win_a`; `kdm3_10_08` `draw`; `kdm3_07_09` `draw`; `kdm1_09_08` `win_a` (`win_margin=1`) |
   | Küçük takım (2v2, 120 sn varsayılan süre) | `kd2v2_03_01` `win_a`; `kd2v2_02_01` `draw`; `kd_default_duration_2v2` (`duration_sec` yok → 120 kullanıldı: `end_ms` = 121000) ve `kd_default_duration_8v8` (→ 300: `end_ms` = 301000) |
   | Pencere ve sayma | `win_kill_at_window_end` (kill `t = window_end` sayılır); `win_kill_after_window_end` (`+1 ms` sayılmaz); `win_kill_before_engage` (engage öncesi `DEATH` sayılmaz); `win_killer_unknown` (`killer=-1`); `win_killer_missing` (`killer` alanı yok); `win_killer_ally` (aynı takım); `win_killer_monster` (takımlarda olmayan kimlik); `win_killer_self`; `win_respawn_ignored_killdiff` |
   | Erken bitiş | `early_end_4` (`early_end_margin=4`, `win_margin=2`: A'nın 4. kill'inde biter, sonraki B kill'leri sayılmaz, `end=early_end`, `end_ms` o kill'in `t`'si); `early_end_off` (`early_end_margin=0`: aynı akış süre sonunu bekler); `early_end_negative` (`B` önde: `win_b`); `early_end_lt_win_margin_error` (`early_end_margin=1`, `win_margin=2` → `ERROR`) |
   | Engage | `engage_at_timeout_ok` (hasar `t=60000`, geçerli); `engage_after_timeout_invalid` (`t=60001` → `NO_ENGAGE`); `engage_timeout_param` (`--engage-timeout-sec 10`, hasar `t=10001` → `NO_ENGAGE`); `engage_none_short_log_truncated` (hasar yok, `obs_end=5000` → `invalid` `TRUNCATED`, `NO_ENGAGE` değil) |
   | `wipe_first` (8v8, ADR/`docs/15` örnekleri) | `wipe_b_last_dies_a3` (B'nin son botu ölür, A'da 3 canlı → `win_a`, `end=wipe`, `alive_a=3`, `alive_b=0`); `wipe_timeout_a4_b2` (süre dolar, A 4/B 2 → `win_a`, `end=alive_count`); `wipe_timeout_3_3` `draw`; `wipe_same_tick_draw` (son iki ölüm 100 ms arayla → `draw`); `wipe_101ms_win` (101 ms arayla → ilk 0'a inen takım kaybeder); `wipe_a_wiped_b_wins` (`win_b`); `wipe_unknown_bot_ignored` (takımlarda olmayan `bot` ölümü canlıyı değiştirmez); `wipe_1v1_first_dies_loses`; `wipe_1v1_same_t_draw`; `wipe_respawn_invalid` (`RESPAWN` → `invalid` `RESPAWN_IN_WIPE_FIRST`); `wipe_tick_ms_param` (`--tick-ms 50` ile 60 ms arayla → ilk 0'a inen kaybeder) |
   | `timed_score` | `ts_no_result_with_counts` (`no_result`, `k_a`/`k_b` dolu); `ts_no_engage_invalid` (`invalid`) |
   | Geçersizlik | `inv_setup_fail_no_start` (`SETUP_FAIL`, `MATCH_START` yok → `invalid`, `rule=-`); `inv_test_teleport`; `inv_aborted_end`; `inv_truncated_killdiff` (`MATCH_END.t` pencereden 5 sn önce, wipe/early yok → `TRUNCATED`); `inv_not_truncated_within_tick` (`obs_end + tick_ms == window_end` → geçerli); `inv_reason_order` (`TEST_TELEPORT` + `NO_ENGAGE` → `reasons=TEST_TELEPORT,NO_ENGAGE`) |
   | Dosya/CLI | `cli_multi_match_order_and_summary` (bir dosyada 2 maç + `match:"-"` satırı + `SELFTEST` satırı → 2 `OUTCOME` + `SUMMARY n=2`); `cli_sort_by_t` (satırlar ters sırada, sonuç aynı); `cli_bad_json_line_rc2` (satır numaralı `ERROR`, çıkış 2); `cli_missing_teams_error_rc2`; `cli_overlap_teams_error`; `cli_strict_rc1`; `cli_strict_rc0_when_valid`; `cli_json_keys` (`json.loads` ile; `summary.n`, `matches[0].result`); `cli_param_priority` (CLI `--win-margin 3` > `MATCH_START` `win_margin:2`); `cli_win_margin_9_rc2`; `cli_missing_path_rc2`; `cli_directory_scan` |
   | Örnek dosya | `sample_file_expected` (§5.7'deki 4 `OUTCOME` satırı ve `SUMMARY`, dosya `tools/bot-outcome-eval/sample.jsonl`'den okunur; yoksa vaka `FAIL`) |

   Her vaka bağımsızdır (bir vaka düşerse diğerleri yine koşar). Çıkış 0 ancak hepsi geçerse.

7. **`tools/bot-outcome-eval/sample.jsonl`** (aşağıdaki 21 satır **aynen**, sırasıyla; CRLF değil LF; `sample_file_expected` vakası dosyayı `os.path.dirname(os.path.abspath(__file__))` altından açar):

   ```
   {"t":0,"match":"SMP-win-1-1","bot":-1,"ev":"MATCH_START","ts_utc":"2026-10-03T00:00:00Z","scenario":"SMP-win","seed":1,"run":1,"win_rule":"killdiff_timed","duration_sec":120,"win_margin":2,"team_a":[1,2],"team_b":[3,4]}
   {"t":1000,"match":"SMP-win-1-1","bot":3,"ev":"DAMAGE","src":1,"dst":3,"amount":120}
   {"t":5000,"match":"SMP-win-1-1","bot":3,"ev":"DEATH","killer":1}
   {"t":8000,"match":"SMP-win-1-1","bot":3,"ev":"RESPAWN"}
   {"t":9000,"match":"SMP-win-1-1","bot":4,"ev":"DEATH","killer":2}
   {"t":30000,"match":"SMP-win-1-1","bot":1,"ev":"DEATH","killer":3}
   {"t":60000,"match":"SMP-win-1-1","bot":3,"ev":"DEATH","killer":2}
   {"t":121000,"match":"SMP-win-1-1","bot":-1,"ev":"MATCH_END","ts_utc":"2026-10-03T00:02:01Z","duration_ms":121000,"result":"completed"}
   {"t":0,"match":"SMP-draw-1-1","bot":-1,"ev":"MATCH_START","ts_utc":"2026-10-03T00:03:00Z","scenario":"SMP-draw","seed":1,"run":1,"win_rule":"killdiff_timed","duration_sec":120,"win_margin":2,"team_a":[1,2],"team_b":[3,4]}
   {"t":1000,"match":"SMP-draw-1-1","bot":3,"ev":"DAMAGE","src":1,"dst":3,"amount":120}
   {"t":5000,"match":"SMP-draw-1-1","bot":3,"ev":"DEATH","killer":1}
   {"t":9000,"match":"SMP-draw-1-1","bot":4,"ev":"DEATH","killer":2}
   {"t":30000,"match":"SMP-draw-1-1","bot":1,"ev":"DEATH","killer":3}
   {"t":121000,"match":"SMP-draw-1-1","bot":-1,"ev":"MATCH_END","ts_utc":"2026-10-03T00:05:01Z","duration_ms":121000,"result":"completed"}
   {"t":0,"match":"SMP-wipe-1-1","bot":-1,"ev":"MATCH_START","ts_utc":"2026-10-03T00:06:00Z","scenario":"SMP-wipe","seed":1,"run":1,"win_rule":"wipe_first","duration_sec":300,"team_a":[1,2],"team_b":[3,4]}
   {"t":500,"match":"SMP-wipe-1-1","bot":3,"ev":"DAMAGE","src":1,"dst":3,"amount":120}
   {"t":4000,"match":"SMP-wipe-1-1","bot":3,"ev":"DEATH","killer":1}
   {"t":7000,"match":"SMP-wipe-1-1","bot":4,"ev":"DEATH","killer":2}
   {"t":7100,"match":"SMP-wipe-1-1","bot":-1,"ev":"MATCH_END","ts_utc":"2026-10-03T00:06:07Z","duration_ms":7100,"result":"completed"}
   {"t":0,"match":"SMP-noeng-1-1","bot":-1,"ev":"MATCH_START","ts_utc":"2026-10-03T00:07:00Z","scenario":"SMP-noeng","seed":1,"run":1,"team_a":[1,2],"team_b":[3,4]}
   {"t":130000,"match":"SMP-noeng-1-1","bot":-1,"ev":"MATCH_END","ts_utc":"2026-10-03T00:09:10Z","duration_ms":130000,"result":"completed"}
   ```

   Beklenen çıktı (`python3 tools/bot-outcome-eval.py tools/bot-outcome-eval/sample.jsonl`):

   ```
   OUTCOME match=SMP-win-1-1 rule=killdiff_timed result=win_a k_a=3 k_b=1 diff=+2 alive_a=- alive_b=- engage_ms=1000 end_ms=121000 end=duration reasons=-
   OUTCOME match=SMP-draw-1-1 rule=killdiff_timed result=draw k_a=2 k_b=1 diff=+1 alive_a=- alive_b=- engage_ms=1000 end_ms=121000 end=duration reasons=-
   OUTCOME match=SMP-wipe-1-1 rule=wipe_first result=win_a k_a=2 k_b=0 diff=+2 alive_a=2 alive_b=0 engage_ms=500 end_ms=7000 end=wipe reasons=-
   OUTCOME match=SMP-noeng-1-1 rule=killdiff_timed result=invalid k_a=- k_b=- diff=- alive_a=- alive_b=- engage_ms=- end_ms=- end=- reasons=NO_ENGAGE
   SUMMARY n=4 win_a=2 win_b=0 draw=1 invalid=1 no_result=0
   ```

8. Tüm vakalar geçince `python3 tools/bot-outcome-eval.py --selftest | tail -1`, sample çıktısı ve `--win-margin 3` çıktısını Uygulayıcı Raporu'na yapıştır. `tools/build.sh Release` ve `tools/run-tests.sh` koş (değişmediklerini göstermek için). Commit: `[F8-01] ...`; planın `Durum`'u `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/bot-outcome-eval.py --selftest` çıkış 0; son satır `SELFTEST PASS n=<N>` ve `N >= 50`; çıktıda `FAIL ` ile başlayan satır yok.
- [ ] K2: §5.6 tablosundaki **tüm** vaka adları `--selftest` çıktısında `PASS <ad>` olarak geçer (`python3 tools/bot-outcome-eval.py --selftest > /tmp/st.txt; for n in kd_14_11 kd_10_08 kd_09_08 kd_08_08 kd_08_09 kd_07_09 kd_03_12 kd_00_00_damage kd_00_00_nodamage kdm3_14_11 kdm3_10_08 kdm3_07_09 kdm1_09_08 kd2v2_03_01 kd2v2_02_01 wipe_b_last_dies_a3 wipe_timeout_a4_b2 wipe_timeout_3_3 wipe_same_tick_draw wipe_101ms_win early_end_4 engage_after_timeout_invalid sample_file_expected; do grep -c "^PASS $n\$" /tmp/st.txt; done` — her satır `1`).
- [ ] K3: `python3 tools/bot-outcome-eval.py tools/bot-outcome-eval/sample.jsonl` çıktısı §5.7'deki beklenen 5 satırla **birebir** aynı (`diff` boş), çıkış 0.
- [ ] K4: `... sample.jsonl --win-margin 3` → `SMP-win-1-1` `result=draw` (fark +2 < 3), `SMP-wipe-1-1` değişmez `win_a`; `SUMMARY n=4 win_a=1 win_b=0 draw=2 invalid=1 no_result=0`.
- [ ] K5: çıkış kodları: `--win-margin 9` → 2; var olmayan yol → 2; sample + `--strict` → 1 (bir `invalid` var); sample (strict'siz) → 0; `--json` çıktısı `python3 -c "import json,sys; d=json.load(sys.stdin); assert d['summary']['n']==4 and d['matches'][0]['result']=='win_a'"` ile geçer.
- [ ] K6 (mutasyon): aracın bir kopyasında (`/tmp` altında) `judge_killdiff` içindeki `win_margin` karşılaştırmasının sınır dahil `>=`'ı `>` yapıldığında kopyanın `--selftest`'i çıkış 1 verir ve **en az 2** vaka `FAIL` olur (`kd_10_08`, `kdm3_14_11` ya da benzeri). Aynı şekilde `wipe_first` aynı-tick penceresinde `<=` → `<` mutasyonu `wipe_same_tick_draw`'u düşürür. Hangi satırı değiştirdiğini ve çıktıyı rapora yaz.
- [ ] K7: `tools/bot-outcome-eval.py` yalnızca standart kütüphaneyi içe aktarır (`grep -nE "^(import|from) " tools/bot-outcome-eval.py` çıktısında her modül stdlib'dir; **`socket`, `urllib`, `http`, `requests`, `subprocess` yok**); CLI modunda dosya yazmaz (`grep -n "open(" ` çıktısındaki her yazma `--selftest` yolundadır); ASCII (`LC_ALL=C grep -nP '[^\x00-\x7F]' tools/bot-outcome-eval.py tools/bot-outcome-eval/sample.jsonl` boş); CRLF yok (`grep -c $'\r'` 0).
- [ ] K8: `git diff --stat gece/2026-10-02-nav...bot/F8-01` yalnızca `tools/bot-outcome-eval.py`, `tools/bot-outcome-eval/sample.jsonl` ve bu plan dosyasını gösterir; `GameServer/`, `BotCore/`, `Tests/`, `shared/`, `AIServer/`, `docs/` değişmemiştir.
- [ ] K9: `./tools/build.sh Release` hatasız biter (yeni uyarı yok) ve `./tools/run-tests.sh` `223 tests, 0 failed` (değişmedi) yazar.
- [ ] K10: Kural bağımsızlığı: Uygulayıcı Raporu'nda ADR-0031-DEG ve `docs/15` §6b örnek tablolarındaki **her satırın** hangi vaka adıyla sınandığı bir tabloyla eşlenmiştir (`killdiff_timed` 7 satır + 0-0 iki durum, `win_margin=3` üç satır, 2v2 iki satır, `wipe_first` dört örnek).

## 7. Doğrulama komutları

```bash
python3 tools/bot-outcome-eval.py --selftest | tail -3
python3 tools/bot-outcome-eval.py tools/bot-outcome-eval/sample.jsonl
python3 tools/bot-outcome-eval.py tools/bot-outcome-eval/sample.jsonl --win-margin 3 | tail -1
python3 tools/bot-outcome-eval.py tools/bot-outcome-eval/sample.jsonl --json | python3 -m json.tool | head -20
python3 tools/bot-outcome-eval.py tools/bot-outcome-eval/sample.jsonl --strict; echo rc=$?
./tools/build.sh Release
./tools/run-tests.sh | tail -2
git diff --stat gece/2026-10-02-nav...bot/F8-01
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları; `*.py text eol=lf`; dosyalar yalnızca ASCII (Türkçe karakter yok, yorumlar İngilizce).
- **Eşik uydurma:** ADR'de olmayan her sayı/kural §5'te `[A]` ile işaretlidir (üst sınırlar, 6–7 oyunculu varsayılan süre, `ABORTED`/`TRUNCATED`/`RESPAWN_IN_WIPE_FIRST` nedenleri, 1v1 aynı-`t` beraberliği). Bunların dışında yeni kural eklenmez; belirsiz bir durumda **durup** soru yaz.
- Sunucu logu şu an `DEATH`/`DAMAGE` içermez: aracı gerçek log üzerinde koşmak bu planın kabulü **değildir**; `sample.jsonl` ve `--selftest` yeterlidir.
- Python 3.8 uyumu: `match` deyimi, `X | Y` tür birleşimi, `list[int]` genel tip notasyonu kullanma.
- `bool` (`true`/`false`) JSON değeri tamsayı olarak kabul edilmez (Python'da `bool` `int` alt sınıfıdır; `isinstance(v, bool)` önce denetlenmeli).
- Sınır koşulları ADR'de **dahildir**: `fark == win_margin` galibiyet; `t == window_end` sayılır; `engage_t − start_t == engage_timeout_sec*1000` geçerli. Bu üçü için ayrı vaka vardır.
- Bu plan sunucu davranışını değiştirmez; bot sistemi kapalıyken (varsayılan) hiçbir şey etkilenmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F8-01` (taban `gece/2026-10-02-nav`)
  - `8895e8e [F8-01] Maç sonuç değerlendiricisi: tools/bot-outcome-eval.py + örnek + --selftest (67 vaka)`
  - plan kaydı commit'i (bu rapor + `Durum`)
- Değişen dosyalar ve neden:
  - `tools/bot-outcome-eval.py` (yeni, 1407 satır): §5 sözleşmesine göre maçları gruplar,
    kural parametrelerini çözer, `killdiff_timed`/`wipe_first`/`timed_score` sonucunu
    hesaplar, `OUTCOME`/`SUMMARY` (ya da `--json`) yazar; `--selftest` 67 vaka.
  - `tools/bot-outcome-eval/sample.jsonl` (yeni, 21 satır): §5.7'deki örnek girdi.
  - `plans/F8-01-bot-sonuc-degerlendirici.md`: yalnızca `Durum` satırı ve bu rapor.
- Derleme sonucu (`tools/build.sh Release` son 7 satır):
  ```
    BotCore.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\BotCore.lib
    Lua.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\Lua.lib
    shared.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\shared.lib
    proj-LogInServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\LogInServer.exe
    proj-GameServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\GameServer.exe
    proj-AIServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\AIServer.exe
    BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  Yeni uyarı/hata yok. `./tools/run-tests.sh` son satırı: `223 tests, 0 failed`
  (değişmedi). Sunuculara dokunulmadı (paralel hat `nav`; `run-servers.sh status`
  ana hattın sunucularını gösteriyor, kapatılmadı).

### Doğrulama çıktıları (istenen)

`python3 tools/bot-outcome-eval.py --selftest | tail -1`:
```
SELFTEST PASS n=67
```

`python3 tools/bot-outcome-eval.py tools/bot-outcome-eval/sample.jsonl`:
```
OUTCOME match=SMP-win-1-1 rule=killdiff_timed result=win_a k_a=3 k_b=1 diff=+2 alive_a=- alive_b=- engage_ms=1000 end_ms=121000 end=duration reasons=-
OUTCOME match=SMP-draw-1-1 rule=killdiff_timed result=draw k_a=2 k_b=1 diff=+1 alive_a=- alive_b=- engage_ms=1000 end_ms=121000 end=duration reasons=-
OUTCOME match=SMP-wipe-1-1 rule=wipe_first result=win_a k_a=2 k_b=0 diff=+2 alive_a=2 alive_b=0 engage_ms=500 end_ms=7000 end=wipe reasons=-
OUTCOME match=SMP-noeng-1-1 rule=killdiff_timed result=invalid k_a=- k_b=- diff=- alive_a=- alive_b=- engage_ms=- end_ms=- end=- reasons=NO_ENGAGE
SUMMARY n=4 win_a=2 win_b=0 draw=1 invalid=1 no_result=0
```

`... sample.jsonl --win-margin 3`:
```
OUTCOME match=SMP-win-1-1 rule=killdiff_timed result=draw k_a=3 k_b=1 diff=+2 alive_a=- alive_b=- engage_ms=1000 end_ms=121000 end=duration reasons=-
OUTCOME match=SMP-draw-1-1 rule=killdiff_timed result=draw k_a=2 k_b=1 diff=+1 alive_a=- alive_b=- engage_ms=1000 end_ms=121000 end=duration reasons=-
OUTCOME match=SMP-wipe-1-1 rule=wipe_first result=win_a k_a=2 k_b=0 diff=+2 alive_a=2 alive_b=0 engage_ms=500 end_ms=7000 end=wipe reasons=-
OUTCOME match=SMP-noeng-1-1 rule=killdiff_timed result=invalid k_a=- k_b=- diff=- alive_a=- alive_b=- engage_ms=- end_ms=- end=- reasons=NO_ENGAGE
SUMMARY n=4 win_a=1 win_b=0 draw=2 invalid=1 no_result=0
```

### K10 — ADR-0031-DEG / `docs/15` §6b örnek eşlemesi

| Örnek | Vaka adı | Sonuç |
|---|---|---|
| killdiff (14,11,+3) | `kd_14_11` | win_a |
| killdiff (10,8,+2, sınır) | `kd_10_08` | win_a |
| killdiff (9,8,+1) | `kd_09_08` | draw |
| killdiff (8,8,0) | `kd_08_08` | draw |
| killdiff (8,9,-1) | `kd_08_09` | draw |
| killdiff (7,9,-2) | `kd_07_09` | win_b |
| killdiff (3,12,-9) | `kd_03_12` | win_b |
| killdiff 0-0 (hasar var) | `kd_00_00_damage` | draw |
| killdiff 0-0 (hasar yok) | `kd_00_00_nodamage` | invalid NO_ENGAGE |
| margin 3 (14,11) | `kdm3_14_11` | win_a |
| margin 3 (10,8) | `kdm3_10_08` | draw |
| margin 3 (7,9) | `kdm3_07_09` | draw |
| 2v2 (3,1) | `kd2v2_03_01` | win_a |
| 2v2 (2,1) | `kd2v2_02_01` | draw |
| wipe: B'nin son botu ölür, A 3 canlı | `wipe_b_last_dies_a3` | win_a |
| wipe: süre dolar, A 4 / B 2 | `wipe_timeout_a4_b2` | win_a |
| wipe: süre dolar, 3 / 3 | `wipe_timeout_3_3` | draw |
| wipe: son botlar aynı tick'te ölür | `wipe_same_tick_draw` | draw |

(Saf `judge_killdiff` sınırları ayrıca `kd_pure_judge_boundaries` vakasında sınanır.)

### Kabul kriterleri öz-değerlendirme

- K1 ✔ `--selftest` çıkış 0, son satır `SELFTEST PASS n=67` (≥ 50), `FAIL ` ile başlayan satır yok.
- K2 ✔ §5.6 tablosundaki tüm adlar `^PASS <ad>$` olarak bir kez geçer (listeli grep: `missing=0`).
- K3 ✔ sample çıktısı §5.7'deki 5 satırla birebir aynı (`sample_file_expected` vakası + elle `diff`), çıkış 0.
- K4 ✔ `--win-margin 3`: `SMP-win` draw, `SMP-wipe` win_a, `SUMMARY n=4 win_a=1 win_b=0 draw=2 invalid=1 no_result=0`.
- K5 ✔ `--win-margin 9` → 2; var olmayan yol → 2; sample `--strict` → 1; sample → 0; `--json` assert geçti.
- K6 ✔ (`/tmp/f801-mut`): `judge_killdiff` `>=` → `>` mutasyonu rc=1, 8 `FAIL` (dahil
  `kd_10_08`, `kdm3_14_11`); aynı-tick sınırı `t_value > t0 + tick_ms` → `>=` mutasyonu
  rc=1, `wipe_same_tick_draw` `draw` yerine `win_b` (FAIL).
- K7 ✔ içe aktarımlar yalnız `json/os/sys/tempfile`; yazan `open()` yalnız `--selftest`
  yollarında (`write_records`, `cli_bad_json_line_rc2`); ASCII; CRLF yok.
- K8 ✔ (kayıt commit'i sonrası) `git diff --stat gece/2026-10-02-nav...bot/F8-01` yalnız
  iki yeni dosya + plan; `GameServer/`, `BotCore/`, `Tests/`, `shared/`, `AIServer/`, `docs/` değişmedi.
- K9 ✔ `tools/build.sh Release` hatasız; `223 tests, 0 failed`.
- K10 ✔ yukarıdaki eşleme tablosu (killdiff 7 + 0-0 iki durum, margin 3 üç, 2v2 iki, wipe dört).

- Plandan sapmalar ve gerekçeleri:
  - `--selftest` vaka sayısı 67'dir (istenen ≥ 50); §5.6 tablosundaki adların tamamı aynen var.
  - `win_killer_*` için önce tek bir toplayıcı vaka yazılmıştı; ad yinelemesini önlemek için
    her alt vaka ayrı `case()` yapıldı (K2 `grep -c` = 1 şartı).
  - `sample.jsonl` için `git` `core.autocrlf=true` uyarısı verdi ("LF will be replaced by
    CRLF"); depodaki blob LF (CR sayısı 0). Çalışma ağacında da LF. `.gitattributes`'a
    `*.jsonl` satırı eklemek bu planın izinli dosya listesinde değildir, eklenmedi; taze bir
    CRLF checkout'unda K7 `grep -c $'\r'` şartı etkilenebilir (aşağıdaki açık soru).
- Açık sorular:
  - `sample.jsonl` (ve olası başka `.jsonl` araç dosyaları) için `.gitattributes`'a
    `*.jsonl text eol=lf` eklenmeli mi? Bu plan yalnız iki yeni dosyaya dokunulmasına izin
    veriyor; gerekirse küçük bir bakım planı gerekir.
- Not: Bu araç sunucu davranışını değiştirmez; `ENABLED=0` (varsayılan) hiç etkilenmez.
  Gerçek logda `DEATH`/`DAMAGE` henüz yoktur; kabul `sample.jsonl` + `--selftest` iledir.


---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DÜZELTME GEREKLİ
- İncelenen: `gece/2026-10-02-nav...bot/F8-01` @ `64dfd01` (iki commit: `8895e8e`, `64dfd01`; paralel hat `nav`, gece modu, sunuculara dokunulmadı, birleştirme/push yapılmadı)
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `--selftest > /tmp/st.txt` rc=0; son satır `SELFTEST PASS n=67`; `grep -c '^FAIL '` = 0 |
| K2 | ✔ | Planın 23 adlı `grep -c "^PASS <ad>$"` döngüsü: hepsi `1`; ayrıca §5.6 tablosundaki tüm vaka adları (69 aday, sonuç sözcükleri hariç) `PASS` satırı olarak var |
| K3 | ✔ | sample çıktısı, plandaki beklenen 5 satırdan çıkarılan dosyayla `diff` boş; rc=0 |
| K4 | ✔ | `--win-margin 3`: `SMP-win` `draw`, `SMP-wipe` `win_a`, `SUMMARY n=4 win_a=1 win_b=0 draw=2 invalid=1 no_result=0` |
| K5 | ✔ | `--win-margin 9` rc=2; `/nonexistent` rc=2; sample `--strict` rc=1; sample rc=0; `--json` assert geçti |
| K6 | ✔ | Kopyada `:284` `fark >= win_margin` → `fark > win_margin`: rc=1, 8 `FAIL` (`kd_pure_judge_boundaries`, `kd_10_08`, `kdm3_14_11`, `kdm1_09_08`, ...), `SELFTEST FAIL failed=8 of 67`. Kopyada `:391` `t_value > t0 + tick_ms` → `>=`: rc=1, `FAIL wipe_same_tick_draw: result: 'win_b' != 'draw'`, `failed=1 of 67` |
| K7 | ✔ | içe aktarımlar `json/os/sys/tempfile` (`:28-31`); `open(` yazımları yalnızca `:708` (`write_records`) ve `:1261` (selftest); okuma `:172`; ASCII (`grep -P` boş); CR sayısı 0 (iki dosya); shebang `:1` |
| K8 | ✔ | `git diff --stat`: yalnızca `tools/bot-outcome-eval.py`, `tools/bot-outcome-eval/sample.jsonl`, plan; plan diff'inde silinen satırlar yalnızca şablon satırları ve `Durum` |
| K9 | ✔ | `./tools/build.sh Release` rc=0, `warning`/`error C` sayısı 0; `./tools/run-tests.sh`: `223 tests, 0 failed` (diff yalnızca `tools/` ve plan olduğundan beklenen) |
| K10 | ✔ | Uygulayıcı raporundaki eşleme tablosu: killdiff 7 + 0-0 iki durum, margin 3 üç, 2v2 iki, wipe dört; her vaka adı `--selftest` çıktısında `PASS` |

- Bulgular (önem sırasıyla):
  1. **`--win-rule` komut satırı seçeneği hiç çalışmıyor** (`tools/bot-outcome-eval.py:119-126`, ilgili kullanım `:18`, `:35`, `:40`, plan §1/§5.3). `PARAM_OPTIONS` içindeki tüm seçenekler `parse_cli_int` ile tamsayıya çevriliyor; `--win-rule` değeri bir sözcük (`wipe_first`) olduğundan her zaman `error: option --win-rule needs an integer` ve çıkış 2 veriyor. Kanıt: `python3 tools/bot-outcome-eval.py tools/bot-outcome-eval/sample.jsonl --win-rule wipe_first; echo rc=$?` → `error: option --win-rule needs an integer`, `rc=2`. Sonraki doğrulama (`:134-135`) ve çözümleme (`:225-226`) kodu doğru yazılmış ama ulaşılamaz. `--selftest` bu yolu hiç sınamadığı için 67/67 geçiyor; plan §5.6 tablosu da bu CLI seçeneği için vaka istemiyordu (plan boşluğu), ama §1'deki kullanım sözleşmesi ve §5.3 ("komut satırı > `MATCH_START` alanı > varsayılan") açıkça `--win-rule`'ü tanımlıyor. Düzeltme küçük: kapsam dışına çıkmaz.
  2. Not (engel değil): `parse_cli_int` `int(raw, 10)` kullandığı için `1_0` gibi alt çizgili değer `10` kabul edilir (`:101`); önemsiz.
  3. Not (engel değil): uygulayıcının açık sorusu (`sample.jsonl` için `.gitattributes`): depodaki blob LF, çalışma ağacında CR yok; ancak `*.jsonl` satırı yok ve `core.autocrlf=true` ile CRLF checkout riski var (KI-009'un aynısı, `.jsonl` için). Bu planın kapsamı dışı olduğundan kapsamı genişletilmedi; `docs/KNOWN_ISSUES.md` KI-017 olarak kaydedildi (küçük bakım planı, `.gitattributes`'a `*.jsonl text eol=lf`).
  4. Not: kural mantığı (`judge_killdiff`, `eval_killdiff`, `eval_wipe`, `evaluate_match`) planın §5.4'üne satır satır uyuyor: sınırlar dahil (`:284`, `:319`, `:548`), 1v1 özel durumu (`:361-387`), tick penceresi (`:391`), `TRUNCATED` yalnızca başka neden yokken ve sonlanma yokken (`:561-567`), neden sırası `REASON_ORDER`. Ek elle sınama (1v1 wipe, `win_margin: true` → `ERROR`, `timed_score` sayaçları) beklenen çıktıyı verdi.
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
plans/F8-01-bot-sonuc-degerlendirici.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
1. tools/bot-outcome-eval.py parse_args (satır ~119-126): `--win-rule` değeri tamsayıya çevrilmemeli. `--win-rule` için ham dizgeyi `opts["win_rule"]`'a ata (değer eksikse mevcut "option --win-rule needs a value" hatası kalsın); diğer beş seçenek (`--duration-sec`, `--win-margin`, `--early-end-margin`, `--engage-timeout-sec`, `--tick-ms`) `parse_cli_int` ile kalsın. Geçersiz kural adı mevcut satır 134-135 denetimiyle çıkış 2 vermeli (değiştirme).
2. --selftest'e üç yeni vaka ekle (mevcut 67 vakanın adlarını değiştirme; ad yinelemesi yok; her biri bağımsız `case()`):
   - `cli_win_rule_option`: `MATCH_START`'ında `win_rule` alanı olmayan 2v2 maç (A'nın iki kill'i, süre dolar), `--win-rule wipe_first` ile koş: `rule=wipe_first` ve `end=alive_count` olmalı.
   - `cli_win_rule_priority`: `MATCH_START` `win_rule:"killdiff_timed"` iken `--win-rule timed_score` komut satırı değeri kazanır: `result=no_result`, `k_a`/`k_b` dolu.
   - `cli_win_rule_invalid_rc2`: `--win-rule bogus` -> çıkış 2 ve stdout'ta hiç `OUTCOME` satırı yok.
3. Doğrula ve raporla: `python3 tools/bot-outcome-eval.py tools/bot-outcome-eval/sample.jsonl --win-rule timed_score` çıktısı (dört maç da kuralı komut satırından almalı: `rule=timed_score`; beklenen: `SMP-win` ve `SMP-draw` `no_result`, `SMP-wipe` `invalid` `TRUNCATED` (7100 ms'de biten günlük, 300 sn'lik pencere dolmadan), `SMP-noeng` `invalid` `NO_ENGAGE`; `SUMMARY n=4 win_a=0 win_b=0 draw=0 invalid=2 no_result=2`), `--selftest | tail -1` (`SELFTEST PASS n=70`), K2'nin 23 adlı `grep -c` döngüsü, K7'nin `grep` komutları (ASCII, CR=0, yalnızca stdlib). Başka dosyaya ve başka mantığa dokunma; `.gitattributes` dahil (KI-017 ayrı).
```

