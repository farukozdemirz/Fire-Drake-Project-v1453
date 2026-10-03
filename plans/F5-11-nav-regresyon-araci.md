# F5-11: Kalıcı navigasyon regresyonu: `tools/nav-regress.sh` (PASS/FAIL, eşik ihlali, `--selftest`)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02-nav) |
| Faz | F5 — Navigasyon (`docs/17` §2; paralel hat `nav`, değerlendirme/analiz araçları: `docs/17` §1 "analiz araçları her fazla paralel") |
| Branch | `bot/F5-11 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F5-01..F5-10 ve F5-50..F5-58 `KAPANDI` (hepsi `gece/2026-10-02-nav` içinde; `tools/nav-measure/nav_measure.cpp` bölümleri `smoothing`, `synthetic`, `velocity`, `velocity-robust`, `arena`, `budget`, `budget-scheduled`, `stuck`, `progress` hazır) |
| İlgili gereksinim / kabul | `docs/12` §11 (AC-NAV-01..07) ve §13.1–§13.5, MET-NAV-01, MET-PERF-02/03; proje sahibi kararı 2026-10-02 madde 7 (geçici betiklere dayanan ölçümleri kalıcı araçlara çevir) |
| Tahmini büyüklük | S (3 yeni dosya, hepsi `tools/` altında; `BotCore/`, `GameServer/`, `Tests/`, `docs/` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü, hat `nav`, konu sırası dilim 1) |

---

## 1. Amaç

Bugün navigasyon ölçümleri `tools/nav-measure.sh <bölüm>` ile ve `tools/nav-segment-check.py` ile **elle** koşulup çıktı gözle okunuyor; hangi satırın hangi eşiğe karşı okunacağı plan dosyalarına ve `docs/12` §13'e dağılmış durumda. Bu plan, bu ölçümleri eşikleriyle birlikte **tek komutta** koşan, çıktıyı otomatik değerlendiren kalıcı bir regresyon aracı yazar:

```
tools/nav-regress.sh [--n N] [--seed S] [--skip-timing] [--timing-retries K] [--from-file F] [--save F] [--list] [--selftest]
```

Çıktı: her denetim için `PASS`/`FAIL`/`WARN`/`INFO` satırı (gözlenen değer, eşik, kaynak) ve sonda `NAV-REGRESS PASS|FAIL ...` özeti; `FAIL` satırı **hangi eşiğin ne kadar aşıldığını** yazar. Çıkış kodu: `0` hepsi geçti, `1` en az bir `FAIL`, `2` ortam/kullanım hatası. `--selftest` sunucusuz, `g++`'sız, haritasız çalışır: gömülü sahte çıktılarla her denetim sınıfının `FAIL` verebildiğini kanıtlar.

Araç `nav_measure.cpp`'yi **değiştirmez**; onun `KEY k=v ...` çıktısını okur. Yeni ölçüm yazmak, eşik **uydurmak** değildir (§3).

## 2. Bağlam (okunması zorunlu)

- `tools/nav-measure.sh` (35 satır; `NAV_SRC_ROOT`, `NAV_MEASURE_OUT`, ızgara yoksa `tools/nav-export.py` ile dışa aktarır, `g++ -std=c++17 -O2` ile derler, `exec` ile bölümü koşar; kullanım `tools/nav-measure.sh [bölüm|all] [--navgrid P] [--seed N] [--n COUNT]`) ve `tools/nav-measure/nav_measure.cpp` (çıktı satırları: `GRID`, `SMOOTHING`, `LINECLEAR`, `STRAIGHT`, `SYNTHETIC`, `VELOCITY`, `VELOCITYR`, `ARENA`, `BUDGET`, `BUDGET_SCHED`, `STUCK`, `STUCK_TRUE`, `PROGRESS`, `PROGRESS_TRUE`, serbest metinli `EXAMPLE`; hata: `ERROR ...`; `main` satır ~1070–1121). Biçimlerin gerçek örnekleri §5.2'de.
- `tools/nav-segment-check.py` (bağımsız süpercover oracle: stdin'den `x0 z0 x1 z1` satırları, çıktı `OK` ya da `BLOCKED cell=(cx,cz)`; `--selftest`; `--navgrid`).
- `docs/12` §13.1 (duvar bulgusu: ham yol/düzleştirme/kiriş 0 ihlal, düz adım ~%7), §13.2–§13.3 (hız kestirimi, takılma), §13.4 (arena), §13.5 (AC-NAV-07), §11 (AC-NAV-01..07); `docs/16` MET-NAV-01, MET-PERF-02/03.
- Eşiklerin kaynak planları: `plans/F5-56-nav-hiz-kestirimi-dayaniklilik.md` §5.3 (hız eşikleri), `plans/F5-51-nav-arena-siniri-ve-dogus-yolu.md` K5 (arena düğüm sınırları), `plans/F5-53-nav-sorgu-butcesi-ve-onbellek.md` (AC-NAV-07), `plans/F5-54-nav-takilma-tespiti.md`/`F5-57` (takılma), `plans/F5-58-nav-duvar-denetimi-kalici-regresyon.md` (sabit duvar vektörleri).
- Çıktı biçimi örnekleri için `tools/bot-telemetry-report.py` (PASS/WARN/FAIL kalıbı, `--strict`) ve `tools/check-perception-contract.py` (`--selftest`, çıkış kodu 1) — aynı dil ve üslup.

## 3. Kapsam

**Yapılacaklar**

1. `tools/nav-regress.py` (yeni): `nav-measure.sh all` çıktısını ayrıştırır, §5.3'teki denetim tablosunu uygular, `nav-segment-check.py` ile çapraz kontrol yapar, raporlar.
2. `tools/nav-regress.sh` (yeni): ince sarmalayıcı (`python3 tools/nav-regress.py "$@"`; `python3` yoksa çıkış 2).
3. `tools/nav-regress/good.txt` (yeni): `--selftest` için "iyi" `nav-measure` çıktısı örneği (gerçek bir `tools/nav-measure.sh all --n 2000` çıktısı, `ERROR` yok).
4. Eşik tablosu **koddaki veri** olarak tutulur (`CHECKS` listesi); `--list` onu yazdırır (kimlik, koşul, kaynak). Her eşiğin kaynağı (`docs/12` §…, plan, AC/MET) tabloda yazılıdır. **Kaynağı olmayan bir sayı gerekiyorsa** `[A]` etiketiyle yazılır ve raporda `[A]` olarak görünür; yeni eşik uydurulmaz, §5.3'te verilenlerin dışına çıkılmaz.

**Kapsam dışı (yapılmayacak)**

- `tools/nav-measure/nav_measure.cpp`, `tools/nav-measure.sh`, `tools/nav-segment-check.py`, `BotCore/*`, `Tests/*`, `GameServer/*`, `shared/*`, `AIServer/*`, `docs/*` değişmez. Bir denetim için gereken satır `nav_measure`'da yoksa **durup** raporda soru olarak yaz (ölçüm ekleme ayrı plandır).
- MSVC Release ölçümü: araç host `g++ -O2` ölçümünü okur (`docs/12` §13 başlığındaki uyarı geçerli: bu MSVC Release değildir). Zamanlama denetimleri bu yüzden gevşek sınırlıdır ve yeniden denenir (§5.4).
- CI/zamanlanmış görev, `build.sh`/`run-tests.sh` içine bağlama, JSON/HTML raporu, başka bölge/zone (yalnızca `zone71.navgrid`), gerçek `WIZ_MOVE` ölçümü (T-NAV-06, F5-55), T-NAV-04/05/09, AC-NAV-03'ün oyun içi kanıtı.
- Sunucu çalıştırmak ya da `tools/run-servers.sh` çağırmak (paralel hat; sunuculara dokunulmaz).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/nav-regress.py` | yeni | LF (depoda `*.py text eol=lf`), yalnızca ASCII, standart kütüphane |
| `tools/nav-regress.sh` | yeni | LF (`*.sh text eol=lf`), ASCII |
| `tools/nav-regress/good.txt` | yeni | selftest girdisi; LF veya CRLF okunabilmeli (`splitlines()`) |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. Proje dosyası (`.vcxproj`) değişmez: bunlar derlemeye girmez.

## 5. Uygulama adımları

1. `git switch -c bot/F5-11 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`; `python3 tools/nav-export.py` (ızgara yoksa); `tools/nav-measure.sh all --n 2000 --seed 20261002 > /tmp/nm.txt` ile mevcut çıktıyı gör (≈ 8 sn).

2. **Çıktı biçimi (doğrulanmış gerçek örnekler, `gece/2026-10-02-nav` @ `db33291`, `--n 2000`):** her satır `KEY` + boşlukla ayrılmış `k=v` jetonlarıdır (ilk jeton bazen `anahtar=değer` olabilir: `VELOCITY jitter=1300..1900ms ...`); `EXAMPLE` satırları serbest metindir; sayılar `int`/`float` ya da `-1`. Ayrıştırıcı: `KEY` → `[{k: v}]` (aynı `KEY` birden çok satırda gelir; denetimler `where` süzgeciyle satır seçer). Değer sayıya çevrilebiliyorsa `float` olur, değilse `str` kalır.
   ```
   GRID n=513 unit=4.0 main_cells=88508 seed=20261002 pairs=2000
   SMOOTHING paths=1990 raw_edges=127018 raw_bad=0 smooth_segments=11140 smooth_bad=0 paths_with_smooth_bad=0 chords=83667 chord_bad=0
   LINECLEAR pairs=337869 clear=122269 false_positive=0
   EXAMPLE straight step (what a bare /bot move packet does) (654.0,818.0)->(646.0,806.0) both cells walkable, touches non-walk cell (163,203)
   STRAIGHT pairs=2000 blocked=138
   SYNTHETIC single_block trials=309680 nav_line_clear_true=270320 false_positive=0
   SYNTHETIC random_clutter trials=68335 nav_line_clear_true=20784 false_positive=0
   VELOCITY cadence_ms=1500 window_ms=1000 ticks=285 zero=285 zero_pct=100.0
   VELOCITY jitter=1300..1900ms window_ms=4000 ticks=800 zero=0 mean_rel_err=0.000 max_rel_err=0.000
   VELOCITYR scenario=arrival_bunching seeds=20 seed_p95=19 seed_max=18 zero_pct=0.000 err_p50=0.0711 err_p95=0.1360 err_max=0.5217
   ARENA case=elmorad_respawn_to_arena forbidden_penalty=10(default) status=Found expanded=2924 length_m=740.5 ms=0.621
   BUDGET set=near64 bots_per_tick=16 ticks=300 query_p50=0.048 query_p95=0.395 query_p99=0.680 tick_p50=1.477 tick_p95=2.745 tick_p99=5.290 tick_max=7.497
   BUDGET_SCHED mode=B bots=16 ticks=600 budget_ms=1.5 served=1920 tick_p50=0.240 tick_p95=0.791 tick_p99=1.276 tick_max=3.389 longest_wait_ms=200 pending=0
   STUCK model=tick110.8+-20+3%late250 feed=every_tick params=F5-09_default ticks=5065 alarm_ticks=6 false_episodes=6
   STUCK_TRUE params=cadence_3200 detected_after_ms=3200
   PROGRESS model=tick110.8+-20+3%late250 evaluator=assessor ticks=5065 stalled=0 awaiting=0 progressing=5065 blocked=0 false_episodes=0 first_ms=-1
   PROGRESS_TRUE evaluator=assessor detected_after_ms=3200
   ```
   Ortak değerler: `STUCK`/`PROGRESS` `model` üç değer alır (`tick100+-0`, `tick100+-10`, `tick110.8+-20+3%late250`); `STUCK` `feed` ∈ {`every_tick`, `packets_only`}, `params` ∈ {`F5-09_default`, `cadence_3200`}; `PROGRESS` `evaluator` ∈ {`F5-09_default`, `cadence_3200`, `assessor`}; `ARENA` `case` ∈ {`karus_respawn_to_arena`, `elmorad_respawn_to_arena`, `arena_to_karus_respawn`}, `forbidden_penalty` ∈ {`nofield`, `10(default)`, `0`}; `BUDGET` `set` ∈ {`near64`, `mid150`, `whole`} (near64 iki kez: `bots_per_tick` 16 ve 64); `BUDGET_SCHED` `mode` ∈ {`A`, `B`}.

3. **Denetim tablosu.** Aşağıdaki her satır bir (ya da aynı süzgeçle birden çok) denetimdir; sütun "Sınıf": **D** = belirlenimci (tohumla aynı sonuç; yeniden denenmez), **Z** = zamanlama (host ölçümü; §5.4), **K** = kontrol (negatif kontrol: ölçümün gücünü sınar), **I** = yalnızca bilgi (`INFO`, hiçbir zaman `FAIL` vermez). "Satır" boşsa denetim `FAIL` verir: `missing line` (bölüm sessizce düşmüş demektir).

   | Kimlik | Satır / süzgeç | Koşul | Sınıf | Kaynak |
   |---|---|---|---|---|
   | `grid.main_cells` | `GRID` | `n == 513` ve `main_cells == 88508` | D | F5-01, F5-58 (zone 71 ana bileşeni) |
   | `smoothing.raw_bad` | `SMOOTHING` | `raw_bad == 0` ve `raw_edges > 0` | D | `docs/12` §13.1, AC-NAV-03 |
   | `smoothing.smooth_bad` | `SMOOTHING` | `smooth_bad == 0`, `paths_with_smooth_bad == 0` ve `smooth_segments > 0` | D | `docs/12` §13.1 |
   | `smoothing.chord_bad` | `SMOOTHING` | `chord_bad == 0` ve `chords > 0` | D | `docs/12` §13.1 (6,75 m paket kirişleri) |
   | `smoothing.coverage` | `SMOOTHING` | `paths >= 0.9 * n` (`n` = `--n`) | D | ölçümün örneklem gücü `[A]` |
   | `lineclear.false_positive` | `LINECLEAR` | `false_positive == 0` ve `clear > 0` | D | `docs/12` §13.1 |
   | `straight.control` | `STRAIGHT` | `blocked > 0` (planlayıcısız düz adım hâlâ engel kesiyor: ölçüm kör değil) | K | `docs/12` §13.1, F5-58 |
   | `synthetic.single_block` | `SYNTHETIC single_block` | `false_positive == 0`, `trials > 0`, `nav_line_clear_true > 0` | D | `docs/12` §13.1 |
   | `synthetic.random_clutter` | `SYNTHETIC random_clutter` | aynı | D | `docs/12` §13.1 |
   | `velocity.jitter` | `VELOCITY` (`jitter` anahtarlı satır) | `zero == 0` ve `max_rel_err <= 0.30` | D | `docs/12` §13.2, F5-56 (jitter eşiği) |
   | `velocity.robust.arrival_jitter` | `VELOCITYR scenario=arrival_jitter` | `zero_pct == 0`, `err_p95 <= 0.20`, `err_max <= 0.30` | D | F5-56 §5.3 |
   | `velocity.robust.arrival_bunching` | `scenario=arrival_bunching` | `zero_pct <= 1`, `err_p95 <= 0.20`, `err_max <= 0.60` | D | F5-56 §5.3 |
   | `velocity.robust.variable_interval` | `scenario=variable_interval` | `zero_pct == 0`, `err_p95 <= 0.10` | D | F5-56 §5.3 |
   | `velocity.robust.packet_loss` | `scenario=packet_loss` | `zero_pct == 0`, `err_p95 <= 0.10` | D | F5-56 §5.3 |
   | `velocity.legacy_window` | `VELOCITY` (`cadence_ms` anahtarlı, `window_ms=1000`) | yalnızca `INFO` (eski 1000 ms pencerenin 1500+ ms'de %100 sıfır hız gösterdiği bilinen model) | I | `docs/12` §13.2 |
   | `arena.respawn.found` | `ARENA`, `case` ∈ {`karus_respawn_to_arena`, `elmorad_respawn_to_arena`}, her üç `forbidden_penalty` | `status == Found` | D | F5-51, `docs/12` §13.4 |
   | `arena.respawn.nodes` | aynı, `forbidden_penalty` ∈ {`10(default)`, `0`} | Karus `expanded <= 2000`; El Morad `expanded <= 6000` | D | F5-51 K5 |
   | `arena.inside_out.design` | `ARENA case=arena_to_karus_respawn`, `forbidden_penalty` ∈ {`10(default)`, `0`} | `status == InvalidGoal` (arena içindeki bot dışarıya hedef planlamaz: kural gevşetilmedi) | D | F5-51 (madde "yasaklıya girilemez gevşetilmez"), AC-NAV-06 |
   | `arena.inside_out.nofield` | aynı case, `forbidden_penalty=nofield` | `status == Found` (alan yokken yol vardır: kontrol) | K | F5-51 |
   | `budget.near64.query_p95` | `BUDGET set=near64` (iki satır: 16 ve 64 bot) | `query_p95 <= 2.0` (ms) | Z | AC-NAV-02, MET-PERF-03 |
   | `budget.unscheduled` | `BUDGET set=mid150` ve `set=whole` | yalnızca `INFO` (tick toplamı p95 bütçeyi aşar; zamanlayıcı gerekçesi) | I | `docs/12` §13.5 |
   | `sched.B.tick_p95` | `BUDGET_SCHED mode=B` | `tick_p95 <= 1.5` (ms) | Z | AC-NAV-07, `P-NAV-TICK-BUDGET-MS` |
   | `sched.B.longest_wait` | `mode=B` | `longest_wait_ms <= 1000` | Z | AC-NAV-07, `P-NAV-MAX-WAIT` (1 sn) |
   | `sched.B.served` | `mode=A` ve `mode=B` | `B.pending == 0` ve `B.served >= 0.9 * A.served` | D | F5-53 (bot başına servis, K5) |
   | `sched.A.info` | `mode=A` | yalnızca `INFO` (zamanlayıcısız taban çizgisi) | I | F5-53 |
   | `stuck.cadence_3200.false` | `STUCK params=cadence_3200` (3 model × 2 `feed` = 6 satır) | `false_episodes == 0` ve `ticks > 0` | D | AC-NAV-01, MET-NAV-01 (≤ 2/bot-saat; 600 sn'lik modelde 0 şartı `[A]`), `docs/12` §13.3, F5-54 |
   | `stuck.default.control` | `STUCK model=tick110.8+-20+3%late250 feed=every_tick params=F5-09_default` | `false_episodes > 0` beklenir (F5-09 varsayılanının bilinen kusuru; yoksa `WARN` "bilinen kusur değişti, beklentiyi güncelle", `FAIL` değil) | K | `docs/12` §13.3 |
   | `stuck.true_positive` | `STUCK_TRUE params=cadence_3200` | `3100 <= detected_after_ms <= 3300` | D | `docs/12` §13.3 ("≥ 3,1 sn"), F5-57 (3200 ms) |
   | `progress.assessor.false` | `PROGRESS evaluator=assessor` (3 model) | `false_episodes == 0`, `stalled == 0`, `first_ms == -1`, `ticks > 0` | D | F5-57, `docs/12` §13.3 |
   | `progress.assessor.true_positive` | `PROGRESS_TRUE evaluator=assessor` | `3100 <= detected_after_ms <= 3300` | D | F5-57 |
   | `progress.old.info` | `PROGRESS` `evaluator` ∈ {`F5-09_default`, `cadence_3200`} ve `STUCK_TRUE params=F5-09_default`, `PROGRESS_TRUE` bu değerlendiriciler | yalnızca `INFO` | I | F5-57 |
   | `oracle.straight_examples` | tüm `EXAMPLE straight step ... (x0,z0)->(x1,z1) ...` satırları (≤ 3 basılır) | her biri `nav-segment-check.py` ile `BLOCKED` (bağımsız oracle aynı fikirde); `STRAIGHT.blocked > 0` iken hiç `EXAMPLE straight` satırı yoksa `FAIL` | D | F5-58 K7 |
   | `oracle.fixed_vectors` | sabit vektörler (aşağıda) `nav-segment-check.py --navgrid <ızgara>` | ilk üçü `BLOCKED`, dördüncü `OK` | D | F5-58 K5 |
   | `oracle.selftest` | `nav-segment-check.py --selftest` | çıkış 0 ve çıktıda `selftest PASS` | D | F5-50 |
   | `examples.unexpected` | `EXAMPLE` satırlarından `straight step` olmayanlar (smoothing/lineclear/synthetic ihlal örnekleri) | sayı `== 0` (varsa ilk 3'ü `FAIL` satırının altında yazdırılır) | D | `docs/12` §13.1 |

   Sabit vektörler (`x0 z0 x1 z1`, dünya metresi, F5-58): `1566.0 878.0 1578.0 866.0` → `BLOCKED`; `926.0 834.0 934.0 826.0` → `BLOCKED`; `1102.0 1034.0 1106.0 1046.0` → `BLOCKED`; `1274.0 890.0 1280.0 890.0` → `OK`. Sınıf **D** olan her denetimde `==`/`<=` karşılaştırması tam ölçüm değerine uygulanır; `float` karşılaştırmasında 1e-9 hoşgörü yeter.

   Bu tablo yeterlidir: `nav_measure` çıktısında olup tabloda olmayan satır türleri (ör. ileride eklenen yeni bölüm) `INFO` olarak sayılır ve `unknown_lines=<n>` olarak özetlenir; `FAIL` vermez.

4. **Zamanlama yeniden denemesi.** Sınıf **Z** denetimleri host zamanlamasına bağlıdır (gürültü). Davranış: tüm ölçüm bir kez koşulur; Z denetimlerinden biri `FAIL` ise yalnızca `nav-measure.sh budget` ve `nav-measure.sh budget-scheduled` (aynı `--n`/`--seed`) en çok `--timing-retries K` (varsayılan 2) kez yeniden koşulur ve Z denetimleri yeni satırlarla yeniden değerlendirilir; **en az bir denemede geçerse** denetim `PASS` olur ve satırda `(attempt 2/3)` yazar (hepsi başarısızsa son denemenin değeri ve `(attempts 3/3)` ile `FAIL`). Sınıf **D** denetimleri yeniden denenmez. `--skip-timing` Z denetimlerini `INFO "skipped"` yapar (ve `budget*` bölümleri koşulmaz, yalnızca diğer bölümler: `smoothing`, `synthetic`, `velocity`, `velocity-robust`, `arena`, `stuck`, `progress`). `--from-file F` hiç ölçüm koşmaz, `F` dosyasını `nav-measure` çıktısı olarak okur (yeniden deneme yok); `--save F` ham `nav-measure` çıktısını yazar.

5. **Orkestrasyon (`nav-regress.py`).** Varsayılan: `subprocess` ile `tools/nav-measure.sh all --n <N> --seed <S>` (varsayılan `--n 3000`, `--seed 20261002`; aynı ortam değişkenleri `NAV_SRC_ROOT`/`NAV_MEASURE_OUT` geçer); çıkış kodu 0 değilse ya da `ERROR` satırı varsa çıkış **2** ("environment": `g++`/ızgara/kaynak yok) ve `nav-measure`'ın son 10 stderr satırı yazılır. Izgara yolu `build/nav/zone71.navgrid` (yoksa `oracle.*` ızgaralı denetimler ve `nav-measure` zaten `nav-export.py` ile üretir; `--from-file` iken ızgara yoksa `oracle.fixed_vectors` ve `oracle.straight_examples` `INFO "skipped: no navgrid"` olur, `FAIL` değil). Python 3 standart kütüphane; `nav-segment-check.py`'yi `subprocess` ile çağırır (içe aktarmaz; dosya adında tire var).

6. **Çıktı biçimi.** Denetim başına bir satır, kimliğe göre hizalı:
   ```
   PASS  smoothing.raw_bad            raw_bad=0 raw_edges=127018  (== 0)                      [docs/12 s13.1, AC-NAV-03]
   FAIL  sched.B.tick_p95             tick_p95=1.62 (<= 1.5)  EXCEEDED by 0.12 ms (attempts 3/3)  [AC-NAV-07]
   WARN  stuck.default.control        false_episodes=0 (expected > 0: known defect changed)    [docs/12 s13.3]
   INFO  budget.unscheduled           set=whole bots=16 tick_p95=28.462                        [docs/12 s13.5]
   NAV-REGRESS FAIL checks=44 pass=43 fail=1 warn=0 info=12 unknown_lines=0 (n=3000 seed=20261002 attempts=3)
   ```
   `FAIL`'de "hangi eşik, gözlenen ne, ne kadar aştı" zorunludur. Son satırdan önce `FAIL` varsa kısa bir `FAILED:` listesi (kimlikler). `--list` her denetim için `id | sınıf | koşul | kaynak` satırı yazar ve 0 döner (ölçüm koşmaz).

7. **`--selftest`** (ölçüm koşmaz, `g++`/ızgara gerekmez): (a) `tools/nav-regress/good.txt` (`n=2000` ile değerlendirilir: `smoothing.coverage` için) → değerlendirici `PASS` (çıkış 0 mantığı) ve `FAIL` sayısı 0; (b) §5.8'deki mutasyon tablosunun her satırı için `good.txt`'nin ilgili satırı değiştirilir (regex ile), değerlendirici çalıştırılır ve **beklenen kimlik** `FAIL` verir (diğerlerinin sayısı değişmeyebilir; yalnızca beklenen kimliğin `FAIL` olması aranır); (c) bir satırın silinmesi `missing line` `FAIL`'ı verir; (d) `nav-segment-check.py --selftest` çalışır (yalnızca `python3` gerekir). Sonunda `selftest PASS` (çıkış 0) ya da başarısız vaka listesi ve `selftest FAIL` (çıkış 1).

8. **Mutasyon vakaları (selftest, en az bunlar; kimlik → değişiklik):**
   - `smoothing.raw_bad` → `raw_bad=0` yerine `raw_bad=1`
   - `smoothing.smooth_bad` → `smooth_bad=3`
   - `smoothing.chord_bad` → `chord_bad=2`
   - `lineclear.false_positive` → `false_positive=1` (`LINECLEAR` satırında)
   - `straight.control` → `STRAIGHT` `blocked=0`
   - `synthetic.single_block` → `SYNTHETIC single_block` `false_positive=1`
   - `velocity.jitter` → `zero=5`
   - `velocity.robust.arrival_bunching` → `err_max=0.61`
   - `velocity.robust.variable_interval` → `err_p95=0.11`
   - `arena.respawn.found` → `elmorad_respawn_to_arena` `10(default)` `status=NodeLimit`
   - `arena.respawn.nodes` → `elmorad_respawn_to_arena` `10(default)` `expanded=6001`
   - `arena.inside_out.design` → `arena_to_karus_respawn` `10(default)` `status=Found`
   - `budget.near64.query_p95` → `set=near64 bots_per_tick=16` `query_p95=2.5`
   - `sched.B.tick_p95` → `mode=B` `tick_p95=1.6`
   - `sched.B.longest_wait` → `longest_wait_ms=1500`
   - `sched.B.served` → `mode=B` `served=1000` (A'nın 0,9 katının altı)
   - `stuck.cadence_3200.false` → `params=cadence_3200` satırlarından birinde `false_episodes=1`
   - `stuck.true_positive` → `STUCK_TRUE params=cadence_3200` `detected_after_ms=5000`
   - `progress.assessor.false` → `evaluator=assessor` satırında `false_episodes=1`
   - `progress.assessor.true_positive` → `PROGRESS_TRUE evaluator=assessor` `detected_after_ms=2900`
   - `examples.unexpected` → fazladan `EXAMPLE smoothing segment (1.0,1.0)->(5.0,5.0) cells (0,0)->(1,1) touches non-walk cell (0,0)` satırı
   - **Aşırı eşik dengesi:** `stuck.default.control` → `STUCK ... F5-09_default` satırında `false_episodes=0` verilince **`WARN`** üretir (FAIL değil), selftest bunu ayrıca sınar.
   - **Süreç testleri:** boş girdi → `FAIL` (`missing line`) ve çıkış kodu 1; `ERROR cannot load ...` satırı içeren girdi → çıkış kodu 2.

9. **Gerçek koşu** ve kayıt: `bash tools/nav-regress.sh --selftest`; `bash tools/nav-regress.sh --list`; `bash tools/nav-regress.sh` (rc=0 beklenir; çıktının tamamı rapora); `bash tools/nav-regress.sh --save /tmp/nm-regress.txt` ve `bash tools/nav-regress.sh --from-file /tmp/nm-regress.txt` (aynı `D` sonuçları). Bir **D** denetimi gerçek koşuda `FAIL` verirse: eşiği gevşetme; kimlik, gözlenen değer, komut ve **bulgu** olarak rapora yaz (§8), planı `UYGULANDI` yapma, soru olarak bırak.

10. `tools/nav-regress/good.txt`: gerçek çıktıdan üretilir (`tools/nav-measure.sh all --n 2000 --seed 20261002 > tools/nav-regress/good.txt`; yalnızca stdout, `ERROR` ve stderr yok). Dosya 160 satırın altında kalır (`EXAMPLE` satırları dahil, ≈ 70 satır beklenir).

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok (bu plan derlenen dosyaya dokunmaz; GameServer çözümünün hâlâ derlendiğini gösterir)
- [ ] K2: `bash tools/nav-regress.sh --selftest` çıkış 0 ve son satır `selftest PASS`; §5.8'deki **her** mutasyon vakası çıktıda `ok` (vaka sayısı ≥ 22 yazılır)
- [ ] K3: `bash tools/nav-regress.sh --list` çıkış 0; çıktıda §5.3 tablosundaki **her kimlik** bulunur (`grep -c` ile ≥ 35 satır; tabloda 35 kimlik var) ve her satırda kaynak sütunu boş değildir
- [ ] K4: `bash tools/nav-regress.sh` (gerçek koşu, sunucusuz) çıkış 0, son satır `NAV-REGRESS PASS`, `fail=0`; toplam süre ≤ 120 sn; çıktının tamamı Uygulayıcı Raporu'nda
- [ ] K5: sınıf **D** satırlarının tamamı iki ardışık gerçek koşuda (`--save` ile iki dosya) aynı değerleri verir: `diff <(grep -vE '^(BUDGET|ARENA)' a.txt) <(grep -vE '^(BUDGET|ARENA)' b.txt)` boş (ARENA'daki `ms=` alanı zamanlamadır; `ARENA` satırlarında `status`/`expanded` karşılaştırması ayrıca aynı olmalı)
- [ ] K6: `--from-file` ile K4'ün `--save` dosyası değerlendirilince aynı `PASS/FAIL` sayıları çıkar (Z yeniden denemesi olmadan); `--from-file` boş dosya → çıkış 1, `ERROR` içeren dosya → çıkış 2
- [ ] K7: `--skip-timing` ile `Z` denetimleri `INFO "skipped"`, `budget*` bölümleri koşulmamış (süre belirgin kısalır), çıkış 0
- [ ] K8: `git diff --stat gece/2026-10-02-nav...bot/F5-11` yalnızca §4'teki üç dosya (+ plan); `BotCore/`, `Tests/`, `GameServer/`, `shared/`, `AIServer/`, `docs/`, `tools/nav-measure*`, `tools/nav-segment-check.py` farkı 0; ASCII; `git diff --check` boş
- [ ] K9 (Claude): kasıtlı ihlal enjeksiyonu: `NAV_SRC_ROOT` ile geçici bir kopyada `BotCore/NavSmooth.h` ya da eşik sayısı bozulur (depoya yazılmaz) → gerçek koşuda ilgili `FAIL` ve çıkış 1; ayrıca `tools/nav-regress.sh --from-file` ile elle bozulmuş çıktı `FAIL` verir
- [ ] K10 (Claude): eşik kaynakları: `--list` çıktısındaki her sayı `docs/12` §11/§13 ve F5-51/53/54/56/57/58 planlarındaki değerle eşleşir; `[A]` etiketli eşikler (`smoothing.coverage`, `stuck.cadence_3200.false` 600 sn şartı, `velocity.jitter` `max_rel_err`) raporda açıkça işaretlidir

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
bash tools/nav-regress.sh --selftest
bash tools/nav-regress.sh --list
time bash tools/nav-regress.sh --save /tmp/nm-a.txt
bash tools/nav-regress.sh --save /tmp/nm-b.txt
diff <(grep -vE '^(BUDGET|ARENA)' /tmp/nm-a.txt) <(grep -vE '^(BUDGET|ARENA)' /tmp/nm-b.txt)
bash tools/nav-regress.sh --from-file /tmp/nm-a.txt
bash tools/nav-regress.sh --skip-timing
./tools/build.sh Release
git diff --stat gece/2026-10-02-nav...bot/F5-11
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3. Yeni dosyalar yalnızca ASCII. **Bu üç dosya `tools/` altında olduğu için LF** satır sonlu olmalıdır (`.gitattributes`: `*.sh`, `*.py` `eol=lf`); "yeni dosya CRLF" kuralı yalnızca C++ kaynakları içindir. `good.txt` için LF yeterli. Dosya modu `100644` (diğer araçlarla aynı); doğrulama komutları `bash tools/...` biçimindedir.
- Sunuculara dokunma, `tools/run-servers.sh` çağırma (paralel hat). `nav-measure.sh` yalnızca host `g++` ve `build/nav/` altına yazar; `build/` git'e girmez.
- **Eşik uydurma yasak:** §5.3 dışında sayı koyma. Gerçek koşuda bir **D** denetimi başarısız olursa bu bir bulgudur (kod gerileme ya da yanlış eşik): eşiği gevşetme, raporla.
- **Z denetimi eşikleri** (`query_p95 <= 2.0`, `tick_p95 <= 1.5`, `longest_wait_ms <= 1000`) AC-NAV-02/AC-NAV-07'den gelir ve MSVC Release'te kanıtlanır (`docs/12` §13.5); host `g++` ölçümü yalnızca bir göstergedir. Gürültü için yeniden deneme vardır; yine de sürekli başarısız olan Z denetimi bulgudur.
- Araç `nav_measure` satır biçimine bağlıdır: `nav_measure.cpp` değişirse bu aracın tablosu da güncellenmelidir (sessiz bozulmayı `missing line` ve `unknown_lines` yakalar). Bu yüzden ayrıştırıcı bilinmeyen jetonlara dayanıklı olmalı (eksik alan → o denetim `FAIL missing field`, çökme değil).
- `AUTO_LOOP`/otonom koşuda Python 3 ve `g++` vardır (`tools/nav-measure.sh` bunlarla çalışır); yoksa çıkış 2 ve rapora yaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-11` (taban `gece/2026-10-02-nav`); uygulama `dbf5cc2` (üç araç dosyası), plan/durum `1679cd6` (bu rapor). Plan metnindeki fark yalnızca `Durum` satırı ve "Uygulayıcı Raporu" (doğrulama `git diff` ile teyit edildi).
- Değişen dosyalar ve neden:
  - `tools/nav-regress.py` (yeni, 1164 satır): `nav-measure` çıktısının `KEY k=v ...` ayrıştırıcısı; `CHECK_META`/`CHECK_FUNCS` (35 denetim, §5.3 tablosunun birebir karşılığı, sınıf D/Z/K/I + kaynak); `PASS/FAIL/WARN/INFO` satırları ve `NAV-REGRESS` özeti; Z denetimleri için `--timing-retries` ile yeniden deneme ve `(attempt a/b)` notu; `--skip-timing` (Z denetimleri `INFO "skipped"`, `budget*` koşulmaz); `--from-file`/`--save`; `--list` (35 satır, kaynak dolu); `--selftest` (27 vaka, gömülü mutasyonlar); `oracle.*` için `tools/nav-segment-check.py` alt süreç çapraz kontrolü (sabit vektörler + `EXAMPLE straight` segmentleri + oracle `--selftest`).
  - `tools/nav-regress.sh` (yeni, 19 satır): `python3 tools/nav-regress.py "$@"` sarmalayıcı; `python3` yoksa çıkış 2.
  - `tools/nav-regress/good.txt` (yeni, 60 satır): `tools/nav-measure.sh all --n 2000 --seed 20261002` gerçek çıktısı (ASCII, LF, `ERROR` yok, `< 160` satır).
  - Kod/proje/doküman dosyalarına dokunulmadı (`BotCore/`, `GameServer/`, `Tests/`, `shared/`, `AIServer/`, `docs/`, `tools/nav-measure*`, `tools/nav-segment-check.py` farkı 0).
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
  Lua.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\Lua.lib
  shared.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\libs\shared.lib
  proj-LogInServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\LogInServer.exe
  proj-GameServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Server\AIServer.exe
  NavLosTests.cpp
  BotCoreTests.vcxproj -> C:\dev\fdp-nav\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  `rc=0`. (Sunuculara/`tools/run-servers.sh`'e dokunulmadı; paralel hat.)
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ `./tools/build.sh Release` rc=0; bu plan derlenen dosyaya dokunmaz.
  - K2 ✔ `bash tools/nav-regress.sh --selftest` çıkış 0, son satır `selftest PASS`, 27 vaka `ok` (≥ 22); her mutasyon beklenen kimliği `FAIL` (ve `stuck.default.control` `WARN`) yaptı.
  - K3 ✔ `bash tools/nav-regress.sh --list` çıkış 0, 35 satır (`wc -l` = 35), her satırda kaynak dolu.
  - K4 ✔ `bash tools/nav-regress.sh` (varsayılan `--n 3000 --seed 20261002`) çıkış 0, son satır `NAV-REGRESS PASS ... fail=0`, süre ≈ 8 sn; çıktının tamamı aşağıda.
  - K5 ✔ iki ardışık `--save` koşusunda D satırları aynı: `diff <(grep -vE '^(BUDGET|ARENA)' /tmp/nm-a.txt) <(grep -vE '^(BUDGET|ARENA)' /tmp/nm-b.txt)` boş; tüm `ARENA` satırlarında `status`/`expanded` iki dosyada birebir (`ms=` dışında).
  - K6 ✔ `--from-file /tmp/nm-a.txt` ve `/tmp/nm-b.txt`: `checks=31 pass=31 fail=0 ... attempts=1` (Z yeniden denemesi yok); boş dosya → çıkış 1 (`FAILED` listesi, 27 FAIL); `ERROR` içeren dosya → çıkış 2; olmayan dosya → çıkış 2.
  - K7 ✔ `bash tools/nav-regress.sh --skip-timing` çıkış 0; `budget.near64.query_p95`/`sched.B.tick_p95`/`sched.B.longest_wait`/`sched.B.served` `INFO "skipped (--skip-timing)"`; `budget*` bölümleri koşulmadı; süre ≈ 2,7 sn (tam koşu ≈ 8 sn).
  - K8 (kısmi, commit sonrası Claude doğrular) fark yalnızca §4'teki üç dosya + plan; `git diff --check` boş; üç dosya da ASCII + LF.
  - K9, K10: Claude'un işi (kasıtlı ihlal enjeksiyonu, eşik kaynak denetimi).
- Çalışma zamanı çıktısı (K4, tamamı):
  ```
  PASS  grid.main_cells                  n=513 main_cells=88508  [F5-01, F5-58]
  PASS  smoothing.raw_bad                raw_bad=0 raw_edges=190074  [docs/12 s13.1, AC-NAV-03]
  PASS  smoothing.smooth_bad             smooth_bad=0 paths_with_smooth_bad=0 smooth_segments=16574  [docs/12 s13.1]
  PASS  smoothing.chord_bad              chord_bad=0 chords=125429  [docs/12 s13.1]
  PASS  smoothing.coverage               paths=2986 (>= 0.9*n=2700)  [sample power [A]]
  PASS  lineclear.false_positive         false_positive=0 clear=182394  [docs/12 s13.1]
  PASS  straight.control                 blocked=197 (> 0)  [docs/12 s13.1, F5-58]
  PASS  synthetic.single_block           false_positive=0 trials=309680 nav_line_clear_true=270320  [docs/12 s13.1]
  PASS  synthetic.random_clutter         false_positive=0 trials=68335 nav_line_clear_true=20784  [docs/12 s13.1]
  PASS  velocity.jitter                  zero=0 max_rel_err=0 (<= 0.30)  [docs/12 s13.2, F5-56 [A]]
  PASS  velocity.robust.arrival_jitter   zero_pct=0 (== 0) err_p95=0.1359 (<= 0.20) err_max=0.1691 (<= 0.30)  [F5-56 s5.3]
  PASS  velocity.robust.arrival_bunching zero_pct=0 (<= 1) err_p95=0.136 (<= 0.20) err_max=0.5217 (<= 0.60)  [F5-56 s5.3]
  PASS  velocity.robust.variable_interval zero_pct=0 (== 0) err_p95=0.0156 (<= 0.10)  [F5-56 s5.3]
  PASS  velocity.robust.packet_loss      zero_pct=0 (== 0) err_p95=0.0074 (<= 0.10)  [F5-56 s5.3]
  INFO  velocity.legacy_window           legacy 1000 ms window: cadence_ms=500 zero_pct=0.0 cadence_ms=1000 zero_pct=0.0 cadence_ms=1500 zero_pct=100.0 cadence_ms=1540 zero_pct=100.0 cadence_ms=2000 zero_pct=100.0  [docs/12 s13.2]
  PASS  arena.respawn.found              all 6 respawn queries Found  [F5-51, docs/12 s13.4]
  PASS  arena.respawn.nodes              karus_respawn_to_arena/10(default) expanded=602 (<= 2000) karus_respawn_to_arena/0 expanded=602 (<= 2000) elmorad_respawn_to_arena/10(default) expanded=2924 (<= 6000) elmorad_respawn_to_arena/0 expanded=2924 (<= 6000)  [F5-51 K5]
  PASS  arena.inside_out.design          inside-out goal InvalidGoal (policy not relaxed)  [F5-51, AC-NAV-06]
  PASS  arena.inside_out.nofield         no-field control Found  [F5-51]
  PASS  budget.near64.query_p95          bots=16 query_p95=0.358 (<= 2.0) bots=64 query_p95=0.409 (<= 2.0) (attempt 1/1)  [AC-NAV-02, MET-PERF-03]
  INFO  budget.unscheduled               scheduler rationale: mid150 tick_p95=8.348 whole tick_p95=30.907  [docs/12 s13.5]
  PASS  sched.B.tick_p95                 tick_p95=0.858 (<= 1.5) (attempt 1/1)  [AC-NAV-07, P-NAV-TICK-BUDGET-MS]
  PASS  sched.B.longest_wait             longest_wait_ms=200 (<= 1000) (attempt 1/1)  [AC-NAV-07, P-NAV-MAX-WAIT]
  PASS  sched.B.served                   A.served=1920 B.served=1920 (>= 0.9*A=1728) B.pending=0  [F5-53 K5]
  INFO  sched.A.info                     unscheduled baseline tick_p95=0.789  [F5-53]
  PASS  stuck.cadence_3200.false         6 model/feed lines false_episodes=0  [AC-NAV-01, MET-NAV-01 [A], docs/12 s13.3, F5-54]
  PASS  stuck.default.control            false_episodes=6 (> 0: known F5-09 defect present)  [docs/12 s13.3]
  PASS  stuck.true_positive              detected_after_ms=3200 (3100..3300)  [docs/12 s13.3, F5-57]
  PASS  progress.assessor.false          3 models false_episodes=0 stalled=0 first_ms=-1  [F5-57, docs/12 s13.3]
  PASS  progress.assessor.true_positive  detected_after_ms=3200 (3100..3300)  [F5-57]
  INFO  progress.old.info                old evaluators: tick100+-0/F5-09_default false=0 tick100+-0/cadence_3200 false=0 tick100+-10/F5-09_default false=0 tick100+-10/cadence_3200 false=0 tick110.8+-20+3%late250/F5-09_default false=6 tick110.8+-20+3%late250/cadence_3200 false=0 STUCK_TRUE/F5-09_default=1500 PROGRESS_TRUE/F5-09_default=1500 PROGRESS_TRUE/cadence_3200=3200  [F5-57]
  PASS  oracle.straight_examples         3 straight examples BLOCKED by oracle  [F5-58 K7]
  PASS  oracle.fixed_vectors             3 BLOCKED + 1 OK  [F5-58 K5]
  PASS  oracle.selftest                  nav-segment-check.py selftest PASS  [F5-50]
  PASS  examples.unexpected              0 unexpected EXAMPLE lines  [docs/12 s13.1]
  NAV-REGRESS PASS checks=31 pass=31 fail=0 warn=0 info=4 unknown_lines=0 (n=3000 seed=20261002 attempts=1)
  ```
- `--selftest` çıktısı:
  ```
  ok   good.txt -> 0 FAIL
  ok   smoothing.raw_bad -> FAIL
  ok   smoothing.smooth_bad -> FAIL
  ok   smoothing.chord_bad -> FAIL
  ok   lineclear.false_positive -> FAIL
  ok   straight.control -> FAIL
  ok   synthetic.single_block -> FAIL
  ok   velocity.jitter -> FAIL
  ok   velocity.robust.arrival_bunching -> FAIL
  ok   velocity.robust.variable_interval -> FAIL
  ok   arena.respawn.found -> FAIL
  ok   arena.respawn.nodes -> FAIL
  ok   arena.inside_out.design -> FAIL
  ok   budget.near64.query_p95 -> FAIL
  ok   sched.B.tick_p95 -> FAIL
  ok   sched.B.longest_wait -> FAIL
  ok   sched.B.served -> FAIL
  ok   stuck.cadence_3200.false -> FAIL
  ok   stuck.true_positive -> FAIL
  ok   progress.assessor.false -> FAIL
  ok   progress.assessor.true_positive -> FAIL
  ok   examples.unexpected -> FAIL
  ok   stuck.default.control -> WARN
  ok   delete GRID -> FAIL missing line
  ok   nav-segment-check.py --selftest
  ok   empty input -> FAIL missing line
  ok   ERROR input -> exit 2 path
  selftest: 27 cases, 27 ok
  selftest PASS
  ```
- Plandan sapmalar ve gerekçeleri:
  1. `--skip-timing` ve Z yeniden denemelerinde `nav-measure.sh` bölüm başına yeniden derlediği için (§5.4 "süre belirgin kısalır" şartı), bu yollarda önce `nav-measure.sh` bir kez `__build__` bölümüyle derlenir (ölçüm yapmaz), sonra derlenmiş `nav_measure` ikilisi doğrudan koşulur; varsayılan tam koşu yine `nav-measure.sh all` ile yapılır. `NAV_MEASURE_OUT`/`NAV_SRC_ROOT` ortam değişkenleri geçer. Bu, `nav-measure.sh`'in `OUT_DIR`/`BIN` yol mantığına bağlıdır; `nav-measure.sh` değişirse araç da güncellenmelidir (plan §8'deki "satır biçimine bağlı" uyarısının kardeşi). `--skip-timing` süresi 2,7 sn (tam koşu 8 sn).
  2. `--from-file` modunda da `oracle.*` denetimleri ızgara varsa koşar (K6 aynı PASS/FAIL sayılarını istediği için); ızgara yoksa `INFO "skipped: no navgrid"`.
  3. Özet sayaçları: her kimlik tek satır (`checks` = PASS+FAIL+WARN, `info` ayrı); §5.6 örneğindeki `checks=44` alt-denetim açılımına karşılık gelmez, `--list` sayısı (35) korunur. `unknown_lines` satır türleri tabloda olmayan `KEY` satırlarıdır (bu koşuda 0).
- Açık sorular:
  - Yok. (K9 kasıtlı ihlal enjeksiyonu ve K10 eşik-kaynak denetimi Claude'a aittir.)

---

## Doğrulama Raporu (Claude doldurur)

### Tur 1 — 2026-10-03

- **Karar: DOĞRULANDI**
- İncelenen commit: `4183e8e` (`bot/F5-11`, taban `gece/2026-10-02-nav`; paralel hat `nav`, `AUTO_LOOP=1`, sunuculara dokunulmadı, birleştirmeyi döngü betiği yapar). Commit'ler `dbf5cc2` (üç araç dosyası), `1679cd6`, `4183e8e` (plan/rapor); mesajlar `[F5-11] ...` biçiminde.
- Kapsam: `git diff --stat gece/2026-10-02-nav...bot/F5-11` = `tools/nav-regress.py` (+1164), `tools/nav-regress.sh` (+19), `tools/nav-regress/good.txt` (+60) ve kendi planı (yalnızca `Durum` satırı ve Uygulayıcı Raporu). `BotCore/`, `Tests/`, `GameServer/`, `shared/`, `AIServer/`, `docs/`, `tools/nav-measure*`, `tools/nav-segment-check.py` farkı 0. `git diff --check` boş; üç dosya ASCII, LF (CR 0), mod `100644`; `build/` commit'e girmemiş; çalışma ağacı temiz.

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 `build.sh Release` rc=0, yeni uyarı yok | ✔ | `./tools/build.sh Release` `build rc=0`, çıktıda `warning` 0; `./tools/run-tests.sh` `223 tests, 0 failed` (plan derlenen dosyaya dokunmaz) |
| K2 `--selftest` çıkış 0, `selftest PASS`, ≥ 22 vaka | ✔ | Kendi koşum: rc=0, `selftest: 27 cases, 27 ok`, `selftest PASS`; her `MUTATIONS` kimliği `ok`, `stuck.default.control` → `WARN`, GRID silme → `missing line`, boş girdi, `ERROR` yolu |
| K3 `--list` 35 satır, kaynak dolu | ✔ | rc=0, `wc -l` = 35; kimlik kümesi plan §5.3 tablosuyla `diff` boş (35 = 35); her satırda 4. sütun dolu |
| K4 gerçek koşu rc=0, `NAV-REGRESS PASS`, `fail=0`, ≤ 120 sn | ✔ | `time bash tools/nav-regress.sh --save /tmp/v-a.txt`: rc=0, 8,06 sn, `NAV-REGRESS PASS checks=31 pass=31 fail=0 warn=0 info=4 unknown_lines=0`; çıktı uygulayıcının raporuyla aynı denetim/değerler (yalnızca zamanlama sayıları farklı) |
| K5 D satırları iki koşuda aynı | ✔ | `diff <(grep -vE '^(BUDGET\|ARENA)' a) <(... b)` boş; `ARENA` `status`/`expanded` (ms hariç) aynı |
| K6 `--from-file` aynı sayılar; boş → 1, `ERROR` → 2 | ✔ | `--from-file /tmp/v-a.txt`: `checks=31 pass=31 fail=0 attempts=1`; boş dosya rc=1 (27 FAIL); `ERROR cannot load foo` rc=2; olmayan dosya rc=2 |
| K7 `--skip-timing` | ✔ | rc=0, 2,5 sn (tam koşu 8 sn); dört Z/sched denetimi ve `budget.unscheduled`/`sched.A.info` `INFO skipped (--skip-timing)`; `checks=27 pass=27` |
| K8 kapsam/biçim | ✔ | yukarıdaki kapsam satırı |
| K9 (Claude) kasıtlı ihlal enjeksiyonu | ✔ | Geçici kopyada (`NAV_SRC_ROOT`, depoya yazılmadı): (a) `NavLineClear` her çifte `true` → `smoothing.smooth_bad` (3444), `chord_bad` (35305), `lineclear.false_positive` (283780), `synthetic.single_block`/`random_clutter`, `examples.unexpected` `FAIL`, rc=1; (b) `NavStuck.h` algılayıcı penceresi ×2 → `stuck.true_positive detected_after_ms=6400 (expected 3100..3300)` `FAIL` ve `stuck.default.control` `WARN`, rc=1; (c) A* girişine yapay gecikme → `sched.B.tick_p95 1.636 (<= 1.5) EXCEEDED by 0.136 (attempts 2/2)`, `sched.B.longest_wait`, `sched.B.served` `FAIL`, rc=1: yeniden deneme yolu çalışıyor; (d) elle bozulmuş `--from-file` (`raw_bad=4`, `query_p95=3.5`) → ilgili iki `FAIL`, rc=1 |
| K10 (Claude) eşik kaynakları | ✔ | `velocity.robust.*`: F5-56 §5.3 (0,20/0,30/0; ≤ 1/0,20/0,60; 0,10; 0,10); arena 2000/6000: F5-51 K5; 1,5 ms / 1 sn: `docs/12` §13.5 ve AC-NAV-07; `query_p95 <= 2.0`: AC-NAV-02/MET-PERF-03; 3100..3300: `docs/12` §13.3 ("≥ 3,1 sn") ve F5-57; sabit vektörler: F5-58 §5.3; 88508: F5-01. `[A]` etiketli üç eşik (`smoothing.coverage`, `stuck.cadence_3200.false` MET-NAV-01 600 sn şartı, `velocity.jitter` `max_rel_err`) `--list`te ve raporda `[A]` işaretli |

Ek denetimler: `tools/nav-regress/good.txt` gerçekten `tools/nav-measure.sh all --n 2000 --seed 20261002` çıktısı (D satırları ve `ARENA` `status`/`expanded` yeniden üretimle birebir aynı, 60 satır, `ERROR` yok). Uygulayıcı Raporu'ndaki sayılar (31 denetim, 27 selftest vakası, 8 sn / 2,7 sn, derleme rc=0) doğru; K4 çıktısı yeniden üretildi.

**Bulgular (hiçbiri engel değil; önem sırasıyla):**

1. (düşük) `tools/nav-regress.py:630-635` `oracle.straight_examples`: `EXAMPLE straight step` satırları var ama `VECTOR_RE` hiçbirini ayrıştıramazsa (biçim kayması) `check([])` boş döner ve denetim `PASS 0 straight examples BLOCKED by oracle` verir (boş girdiyle denendi). Sessiz bozulma yolu; `vectors` boşken `FAIL` verilmeli.
2. (düşük) Kısmi satır kaybı yakalanmıyor: `tools/nav-regress.py:519-532` (`stuck.cadence_3200.false`, plan "6 satır"), `:559-574` (`progress.assessor.false`, 3 model) ve `:443-459` (`budget.near64`, 2 satır) yalnızca "hiç yok" durumunda `FAIL` veriyor; 6 satırdan yalnız 1'i kalınca `PASS 1 model/feed lines` çıkıyor (denendi). Plan §5.3 yalnızca "satır boşsa" demiştir, bu yüzden ihlal sayılmadı; beklenen satır sayısı (6/3/2) denetlenirse sessiz düşme yakalanır.
3. (düşük) `tools/nav-regress.py:337-354` `_robust`: `err_p95` ve `zero_pct` ihlallerinde `FAIL` satırı "ne kadar aştı" yazmıyor (`err_p95=0.11 (<= 0.10) err_p95 EXCEEDED`; `err_max` için miktar var); plan §5.6 "hangi eşik, gözlenen, ne kadar aştı zorunlu" der. Koşul zinciri (`:348`) ayrıca gereğinden karmaşık.
4. (not) `tools/nav-regress.py:835-843` `Measure.build`: `--skip-timing`/yeniden deneme yolu `nav-measure.sh __build__` ile derler; bu `nav_measure`'ın bilinmeyen bölüm adında yalnızca ızgarayı yükleyip 0 dönmesine dayanır (`nav_measure.cpp:1101-1120`). Çalışıyor (uygulayıcı sapma olarak bildirdi, onaylanıyor) ama kırılgan: `nav_measure` bilinmeyen bölümde hata verecek hale gelirse `--skip-timing` ortam hatasıyla (2) düşer.
5. (not, araç dışı) `nav_measure.cpp:889-893` `STUCK` bölümü `noProgressMs = 3200`'ü kendi içinde sabitliyor, `NavPacketCadenceParams()` kullanmıyor; bu yüzden başlıktaki `NavStuck.h:484` değeri değişse `stuck.*` denetimleri bunu görmez (denendi: 3200 → 5000 yakalanmadı; algılayıcı mantığı bozulunca yakalanıyor). Plan `nav_measure.cpp`'yi değiştirmeyi yasakladığı için bu planın kusuru değil; ayrı bir ölçüm planı konusu (`KI-016`).
6. (not) Z denetimleri ilk denemede geçse de satırda `(attempt 1/1)` yazıyor (plan yalnızca yeniden deneme geçişinde not ister) ve `float` için 1e-9 hoşgörü uygulanmıyor (ölçüm 4 hane yazdığından pratikte etkisiz).

Kayıtlar: `plans/README.md`, `docs/STATUS.md`, `docs/KNOWN_ISSUES.md` (`KI-016`) güncellendi. Birleştirme/push yapılmadı (`AUTO_LOOP=1`; döngü betiği `gece/2026-10-02-nav`'a birleştirir).
