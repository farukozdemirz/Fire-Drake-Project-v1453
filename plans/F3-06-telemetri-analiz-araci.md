# F3-06: Telemetri analiz aracı (`tools/bot-telemetry-report.py`): JSONL → Markdown rapor (MET-PERF-02, maç özeti, geçersiz maç bayrakları)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F3 — Telemetri ve test altyapısı (`docs/17` §2, Görev 6 "Analiz aracı") |
| Branch | `bot/F3-06` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F3-01 (`KAPANDI`: JSONL biçimi, `PERF_SAMPLE`), F3-02 (`KAPANDI`: `<match>.jsonl`, `summary.json`) |
| İlgili gereksinim / kabul | REQ-MET-02; `docs/16` §6.8 (MET-PERF-02), §7 (geçersiz maç), §8 ("Senaryo raporu üretici"); `docs/17` F3 Kabul: "analiz aracı örnek maçtan MET tablosu üretiyor", "düşürülen olay sayacı çalışıyor" |
| Tahmini büyüklük | S (1 yeni dosya, ~350 satır Python, sunucu kodu yok) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

Numaralandırma notu: `docs/16`, `docs/17` ve ADR-0007/0015 `ScenarioRunner`'a F3-03, GM komutlarına F3-04, birim test çatısına F3-05 adını verdi; kimlikler yeniden kullanılmaz. Bu yüzden analiz aracı (taslakta F3-06 olarak anılıyordu) önce yazılıyor; F3-03..F3-05 numaraları boş kalır ve sırası gelince yazılır.

---

## 1. Amaç

Sunucunun ürettiği telemetri dosyalarını (`Logs/bots/<tarih>/<match>.jsonl`, `live-*.jsonl`, yanındaki `<match>.summary.json`) okuyup tek bir **Markdown rapor** üreten, yalnızca Python standart kütüphanesini kullanan bir araç yazmak: dosya/maç tablosu, geçersiz maç bayrakları, MET-PERF-02 (BotManager tick süresi) tablosu ve bütçe hükmü, olay sayıları, uyarılar. Araç, ileride gelecek olay tiplerini (`DECISION`, `DAMAGE`, ...) bilmeden **sayar ve geçer**; böylece sonraki fazlar araca yeni bölüm ekleyerek ilerler.

Bu plan sunucu koduna dokunmaz. Bot sistemi ve sunucu davranışı hiç değişmez.

## 2. Bağlam (okunması zorunlu)

- `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md` §3.1 (ortak alanlar), §3.2 (olay tipleri), **§3.3 (F3-01/F3-02 uygulama notları: `PERF_SAMPLE` alanları, `MATCH_START`/`MATCH_END` alanları, `summary.json` biçimi, `SELFTEST` yok sayılır)**, §6.8 (MET-PERF-02 bütçesi: 16 bot için p95 ≤ 5 ms, 64 bot için ≤ 15 ms), §7 ("Geçersiz maç" listesi), §8 ("Senaryo raporu üretici").
- `docs/adr/ADR-0007-telemetri-formati-ve-depolama.md` (biçim kararları; "Ek (F3-02)").
- `tools/damage-trace-summary.py`: aynı klasördeki mevcut araç. Komut satırı kalıbını (`USAGE` metni, `--selftest`, `main(argv)`, çıkış kodları 0/2), yalnızca stdlib ve ASCII-only kuralını **aynen izle** (bak `:19-35`, `:470` `run_selftest`, `:602` `main`).
- Gerçek örnek veriler (depo dışı, `/mnt/c/dev/fdp/server/Logs/bots/2026-10-02/`, 2026-10-02'de var olduğu doğrulandı):
  - `t1-7-1.jsonl` (5 satır: `MATCH_START`, 3 `PERF_SAMPLE`, `MATCH_END`) ve `t1-7-1.summary.json`; `t1-7-2`, `t2-5-4`, `t3-4294967295-3` aynı biçimde.
  - `live-063701.jsonl` (~620 KB, 8192 `SELFTEST` satırı + birkaç `PERF_SAMPLE`; yüklü dosya sınaması), `live-070724.jsonl` (maçsız, `"match":"-"`).
- Gerçek satır biçimi (örnek, `t1-7-1.jsonl`):

  ```
  {"t":144025455,"match":"t1-7-1","bot":-1,"ev":"MATCH_START","mode":"live","ts_utc":"2026-10-02T04:04:41Z","scenario":"t1","seed":7,"run":1,"composition":["BotWP_K","BotMF_K","BotWP_E","BotPHD_E"],"in_game":4}
  {"t":144030080,"match":"t1-7-1","bot":-1,"ev":"PERF_SAMPLE","mode":"live","window_ms":5071,"tick_n":46,"tick_p50_us":1,"tick_p95_us":1270,"tick_p99_us":1638,"tick_max_us":1638,"sessions":4,"in_game":4,"pool_free":12,"skipped_ticks":0,"queue_len":0,"written":3,"dropped_soft":0,"dropped_hard":0}
  {"t":144040819,"match":"t1-7-1","bot":-1,"ev":"MATCH_END","mode":"live","ts_utc":"2026-10-02T04:04:56Z","duration_ms":15364,"result":"ok","dropped_soft":0,"dropped_hard":0,"in_game":4,"perf_samples":3,"tick_p95_max_us":1270,"tick_max_us":1638}
  ```

  `summary.json` (tek satır): `{"match","mode","file","start":{...MATCH_START alanları},"end":{...MATCH_END alanları},"lines":<int>,"events":{"<ev>":<sayı>,...}}`.
- Sunucu tarafı gerçekleri (2026-10-02'de depoda doğrulandı; kod aracı etkilemez, yalnızca sayıların anlamı için):
  - `PERF_SAMPLE`'daki `written`, `dropped_soft`, `dropped_hard` **süreç boyunca birikimlidir** (`GameServer/Bot/BotManager.cpp:447-455`, `Telemetry::GetStats()`); `MATCH_END`'deki `dropped_soft`/`dropped_hard` ise **yalnızca o maç içindir** (`GameServer/Bot/Telemetry.cpp:418-422`, maç başı sayaç çıkarılır).
  - `PERF_SAMPLE` pencere yüzdelikleri (`tick_p50/p95/p99_us`) pencere başınadır; pencereler birleştirilince tam dağılım yeniden kurulamaz → araç yaklaşık ve konservatif tahmin verir (§5.4).
  - `SELFTEST` olayları yalnızca `[BOT] TELEMETRY_SELFTEST=1` iken yazılır, analizde yok sayılır (`docs/16` §3.3).
- Uygulayıcı önce şunu kontrol etsin: yukarıdaki örnek dosyalar yoksa (log temizlenmiş olabilir) K6/K7'yi çalıştırma, raporda "örnek veri yok" yaz; selftest (K3) tek başına yeterli kanıt değildir ama zorunludur.

## 3. Kapsam

**Yapılacaklar**

- Yeni `tools/bot-telemetry-report.py`: komut satırından dosya ve/veya klasör alır, Markdown (varsayılan) veya `--json` çıktı verir, `--selftest` ile yerleşik sentetik testini çalıştırır.
- Bölümler: dosyalar, maçlar (geçerlilik hükmüyle), MET-PERF-02, olay sayıları, uyarılar (§5).
- Yerleşik `--selftest` (sentetik JSONL'lerle, geçici dizinde).

**Kapsam dışı (yapılmayacak)**

- Sunucu kodu, `GameServer/**`, `shared/**`, `docs/**`, ini dosyaları, SQL; `tools/` altında başka bir dosya.
- Grafik/PNG/HTML üretimi, harita izi, bootstrap/Wilson/SPRT istatistikleri, Elo; MET-PERF-01 (bot başına tick), MET-PERF-04/05, kazanma oranı gibi **henüz olay üretilmeyen** metrikler.
- Telemetri açık/kapalı karşılaştırması (`docs/17` F3 Kabul'deki "ek maliyet ≤ %10" ölçümü); iki koşunun karşılaştırılması bu aracın işi değil (ayrı plan).
- Yeni üçüncü taraf paket (numpy, pandas, matplotlib, yaml, ...), `pip install`.
- Sunucuyu çalıştırmak, `GameServer.ini` düzenlemek, DB'ye bağlanmak, dosya silmek/yeniden adlandırmak. Araç **yalnızca okur** (`--out` ile verilen çıktı dosyası dışında hiçbir şey yazmaz).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/bot-telemetry-report.py` | yeni | ASCII, LF, `#!/usr/bin/env python3`, yalnızca stdlib |

Başka dosya yok (vcxproj/filters değişmez; `tools/*.py` derlemeye girmez). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Komut satırı

```
python3 tools/bot-telemetry-report.py PATH [PATH ...] [--json] [--out FILE] [--strict]
python3 tools/bot-telemetry-report.py --selftest
```

- `PATH`: `.jsonl` dosyası veya klasör (klasör **özyinelemeli** taranır, yalnızca adı `.jsonl` ile biten dosyalar alınır; `*.summary.json` giriş sayılmaz, yalnızca §5.3'teki çapraz denetimde kullanılır). Dosya sırası: yola göre sıralı (deterministik çıktı).
- `--json`: raporu Markdown yerine tek JSON nesnesi olarak yaz (anahtarlar §5.5).
- `--out FILE`: çıktıyı stdout yerine dosyaya yaz (UTF-8, LF).
- `--strict`: geçersiz maç veya MET-PERF-02 `FAIL` varsa çıkış kodu 1 (varsayılan: rapor üretildiyse 0).
- Çıkış kodları: 0 başarı; 1 yalnızca `--strict` ihlali; 2 kullanım/G/Ç hatası (`PATH` yok, okunamayan argüman, hiç `.jsonl` bulunamadı). `damage-trace-summary.py`'deki gibi `USAGE` sabiti, `unknown argument` hata metni.
- Hiç `PATH` verilmediyse `USAGE` yazıp 2 döndür (varsayılan klasör **yok**).

### 5.2 Okuma ve ayrıştırma

- Dosyayı `open(path, "r", encoding="utf-8", errors="replace")` ile satır satır oku; boş satırları atla. Her satırı `json.loads` ile çöz; sonuç `dict` değilse veya çözülemezse satırı **kötü satır** say (dosya başına `bad_lines`, ilk kötü satırın numarası `first_bad_line`), çökme yok.
- Geçerli satırda `ev` (str) yoksa kötü satır say.
- `ev == "SELFTEST"` satırları `ignored_selftest` sayacına gider, hiçbir tablodaki olay sayısına/pencere hesabına girmez.
- Beklenmeyen veya yeni `ev` değerleri (`DECISION`, `DAMAGE`, ...) hata değildir: yalnızca olay sayısı tablosuna girer.
- Eksik/beklenmeyen tipli alanlarda (ör. `tick_p95_us` yok veya sayı değil) o `PERF_SAMPLE` pencereye **katılmaz** ve dosya uyarısına `perf sample N skipped (missing field)` eklenir.
- `t` (ms, monoton) süreç içi karşılaştırma içindir: aynı dosyada `t` değerleri azalıyorsa uyarı `t not monotonic at line N` (bir kez, ilk ihlal); hata değil.

### 5.3 Maç birleştirme ve geçerlilik

Her dosyada `MATCH_START`/`MATCH_END` olayları aranır. Bir dosya (maç dosyaları için) **en fazla bir** maç içerir; `live-*.jsonl` dosyaları maçsızdır (`"match":"-"`) ve `MATCH_START` içermez. Dosya başına `match` kimliği: `MATCH_START` varsa onun `match` alanı, yoksa `-`. Dosyada birden çok `MATCH_START` varsa ilkini kullan ve uyarı `multiple MATCH_START (using first)` ekle.

Maç satırı alanları: `match`, `scenario`, `seed`, `run`, `mode`, `ts_utc` (başlangıç), `duration_ms` (`MATCH_END`'den; yoksa `-`), `result`, `composition` (liste uzunluğu `n_comp`), `in_game_start` (`MATCH_START.in_game`), `perf_samples` (dosyadaki geçerli `PERF_SAMPLE` sayısı), `dropped_soft`, `dropped_hard` (`MATCH_END`'den; yoksa `-`), `status`, `reasons`.

Geçerlilik (`docs/16` §7'ye eşleme). `reasons` aşağıdaki kodların boş olmayan listesidir; liste boşsa `status = VALID`, değilse `INVALID`:

| Kod | Koşul | Dayanak |
|---|---|---|
| `NO_END` | `MATCH_START` var, `MATCH_END` yok (sunucu çökmesi/kesilmiş dosya) | §7 "sunucu çökmesi" |
| `ABORTED` | `MATCH_END.result == "aborted"` (sunucu kapanışı) | F3-02: `Stop()` açık maçı `aborted` kapatır |
| `SPAWN_SHORT` | `MATCH_START.in_game < n_comp` | §7 "bot spawn hatası" |
| `DROPPED_HARD` | `MATCH_END.dropped_hard > 0` | veri kaybı, ölçüm eksik |
| `TELEPORT_IN_EVAL` | `mode == "eval"` ve dosyada `TEST_TELEPORT` olayı var | §7 (şimdilik hep `live`, ileriye dönük) |
| `BAD_LINES` | dosyada kötü satır var | bozuk/kesik JSONL |

`result` serbest metindir (`/bot match end [sonuç]`): `ok`, `completed` gibi değerler geçerli sayılır; yalnızca tam olarak `aborted` geçersiz kılar.

**summary.json çapraz denetimi:** `<dosya adı>` `.jsonl` uzantısı yerine `.summary.json` olan dosya aynı klasörde varsa oku; bulduğunda şunları karşılaştır ve fark varsa uyarı yaz (hata değil): `lines` (= dosyadaki **boş olmayan** satır sayısı, kötü satırlar dahil), `events` sözlüğü (= `ev` başına satır sayısı, `SELFTEST` dahil: sunucu onu da sayar mı bilinmez → karşılaştırmada `SELFTEST` anahtarını iki taraftan da at), `start.scenario`/`start.seed`/`start.run` ve `end.duration_ms` değerleri. Uyarı metni: `summary mismatch: lines 5 != 6` biçiminde. Yoksa uyarı yazma (maçsız `live-*` dosyalarında `summary.json` zaten yok).

### 5.4 MET-PERF-02 tablosu

Kaynak başına (her `.jsonl` dosyası ayrı satır; tek tek geçerli `PERF_SAMPLE` pencereleri):

- `windows`: pencere sayısı; `ticks`: `tick_n` toplamı.
- `p50_med_us`: pencere `tick_p50_us` değerlerinin medyanı.
- `p95_est_us`: pencere `tick_p95_us` değerlerinin `tick_n` ile ağırlıklı ortalaması (tam yüzdelik değil, **tahmin**; başlıkta `est` yazar). `tick_n` toplamı 0 ise `-`.
- `p95_worst_us`: pencere `tick_p95_us` en büyüğü; `p99_worst_us`: pencere `tick_p99_us` en büyüğü; `max_us`: pencere `tick_max_us` en büyüğü.
- `in_game_max`: pencere `in_game` en büyüğü.
- `skipped_ticks_max`: `skipped_ticks` en büyüğü (süreç birikimlidir, son değer ≈ en büyük).
- `dropped_soft_max`, `dropped_hard_max`: pencerelerdeki `dropped_soft`/`dropped_hard` en büyüğü (**süreç birikimli sayaç**; tablo başlığında `(cum.)` yazar). Maç dosyasında maça özgü sayı `MATCH_END`'den, maçlar tablosundadır.
- `budget_us` ve `verdict`: bütçe `in_game_max <= 16` ise 5000, `<= 64` ise 15000 (`docs/16` §6.8), `> 64` ise bütçe yok (`-`, hüküm `NO_BUDGET`). Hüküm `p95_worst_us <= budget_us` ise `OK`, değilse `FAIL`; `windows == 0` ise `NO_DATA`. (Konservatif ölçüt: en kötü pencere kullanılır; `p95_est_us` yalnızca bilgi içindir. Hükmün gerekçesi bu satırın altına bir cümleyle yazılır.)

`windows == 0` dosyalar (örn. çok kısa maç) satırda `NO_DATA` ile yer alır.

### 5.5 Çıktı biçimi

Markdown, bölüm başlıkları sabit sırada (İngilizce başlık, ASCII):

```
# Bot telemetry report

## Files
| file | match | mode | lines | bad_lines | events |
## Matches
| match | scenario | seed | run | mode | duration_s | result | composition | in_game | perf_samples | dropped_soft | dropped_hard | status | reasons |
## MET-PERF-02 (BotManager tick)
| file | windows | ticks | p50_med_us | p95_est_us | p95_worst_us | p99_worst_us | max_us | in_game_max | skipped_max | dropped_soft (cum.) | dropped_hard (cum.) | budget_us | verdict |
## Events
| ev | count |
## Warnings
- ...
```

- `file`: yalnızca dosya adı (tam yol değil; aynı ad iki klasörde varsa üst klasörle `2026-10-02/t1-7-1.jsonl`). `duration_s = duration_ms / 1000` ondalık 1 hane. Sayılar tamsayı; `-` yoksa.
- `Matches` bölümü maç içermeyen girişte `(none)` satırı yazar; `Warnings` boşsa `(none)`.
- `Events` tablosu tüm girişlerin toplamıdır, sayıya göre azalan, eşitlikte `ev` adına göre artan; bölümün altına `ignored SELFTEST lines: N` satırı.
- `--json` çıktısı: `{"files":[...],"matches":[...],"perf":[...],"events":{...},"ignored_selftest":N,"warnings":[...]}`; alan adları tablo sütunlarıyla aynı (`dropped_soft_cum` gibi boşluksuz ASCII), `json.dumps(..., sort_keys=True, indent=2)`.
- Çıktı deterministik olmalı (aynı girdi, aynı bayt); araç sistem saatini kullanmaz.

### 5.6 `--selftest`

`tempfile.TemporaryDirectory` içinde sentetik dosyalar üretip rapor fonksiyonlarını doğrudan çağıran (alt süreç açmayan) bir sınama yaz; `assert` + `print("selftest OK")`, `return 0` (bak `damage-trace-summary.py:470-598`). En az şu durumlar:

1. Geçerli maç (`MATCH_START`, 3 `PERF_SAMPLE`, `MATCH_END`, yanında doğru `summary.json`) → `status VALID`, MET-PERF-02 değerleri elle hesaplanmış sayılarla birebir (medyan, `tick_n` ağırlıklı p95 ortalaması, en büyükler), `verdict OK`, uyarı yok.
2. `MATCH_END` eksik → `NO_END`; `result:"aborted"` → `ABORTED`; `in_game < n_comp` → `SPAWN_SHORT`; `dropped_hard:1` → `DROPPED_HARD`; `mode:"eval"` + `TEST_TELEPORT` → `TELEPORT_IN_EVAL`; bozuk satır → `BAD_LINES` ve `first_bad_line`. Birden çok kod aynı anda listelenir.
3. Bütçe: `in_game_max = 16`, `tick_p95_us = 5000` → `OK`; `5001` → `FAIL`; `in_game_max = 17` aynı değerlerle 15000 bütçesi; `in_game_max = 65` → `NO_BUDGET`; pencere yok → `NO_DATA`.
4. `SELFTEST` satırları yok sayılır ve `ignored SELFTEST lines` doğru; bilinmeyen olay (`DECISION`) olay tablosunda sayılır ve hata vermez.
5. `summary.json` çapraz denetimi: bilerek `lines` yanlış → `summary mismatch` uyarısı; doğru → uyarı yok.
6. Klasör taraması özyinelemeli ve `.summary.json` giriş sayılmıyor; aynı girdi iki kez çalıştırılınca çıktı baytları aynı; `--json` çıktısı `json.loads` ile çözülüyor.

## 6. Kabul kriterleri

- [ ] K1: `tools/bot-telemetry-report.py` var; `file tools/bot-telemetry-report.py` "ASCII text" (CRLF/BOM yok); yalnızca stdlib import'ları (`grep -nE '^(import|from) ' tools/bot-telemetry-report.py` çıktısında üçüncü taraf modül yok).
- [ ] K2: `python3 tools/bot-telemetry-report.py` (argümansız) `USAGE` yazar, çıkış kodu 2; `python3 tools/bot-telemetry-report.py --bogus x.jsonl` `unknown argument` yazar, çıkış 2; var olmayan yol çıkış 2.
- [ ] K3: `python3 tools/bot-telemetry-report.py --selftest` çıkış 0 ve son satır `selftest OK`; §5.6'nın altı maddesinin hepsi kodda mevcut (Uygulayıcı Raporu'nda madde madde hangi `assert`'in neyi sınadığı yazılır).
- [ ] K4: Bütçe sınırları (`5000`→OK, `5001`→FAIL, `in_game_max` 17→15000, 65→`NO_BUDGET`, pencere yok→`NO_DATA`) selftest'te sınanıyor (Claude değerleri kodda okuyup elle bozarak yeniden dener).
- [ ] K5: Çıktı deterministik: aynı komut iki kez çalıştırılınca `md5sum` aynı.
- [ ] K6: Gerçek veride (varsa) `python3 tools/bot-telemetry-report.py /mnt/c/dev/fdp/server/Logs/bots/2026-10-02` çıkış 0; `Matches` tablosunda `t1-7-1`, `t1-7-2`, `t2-5-4`, `t3-4294967295-3` dört satırı bulunur, her biri `summary.json` ile uyuşur (`summary mismatch` uyarısı yok); `t1-7-1` için `duration_s 15.4`, `perf_samples 3`, `p95_worst_us 1270`, `max_us 1638`, `in_game_max 4`, `verdict OK`; `t3-4294967295-3` `duration_s 8.8`, `p95_worst_us 95`, `max_us 1244`.
- [ ] K7: Aynı dizinde `live-063701.jsonl` (8192 `SELFTEST` satırlı dosya) hata vermeden işlenir; `Events` tablosunda `SELFTEST` **yok**, `ignored SELFTEST lines: 8192` (veya dosyadaki gerçek sayı: Uygulayıcı `grep -c SELFTEST` ile doğrular) yazar; tüm komut < 5 sn.
- [ ] K8: `--json` çıktısı `python3 -c 'import json,sys; json.load(sys.stdin)'` ile çözülüyor; `--out /tmp/x.md` aynı Markdown'u dosyaya yazıyor, stdout boş; `--strict` ile, içinde `INVALID` maç olan bir girdiyle (selftest'ten bağımsız elle hazırlanmış geçici dosya) çıkış 1.
- [ ] K9: Araç hiçbir dosyayı değiştirmez/silmez (`--out` hariç): çalıştırma öncesi ve sonrası `Logs/bots/2026-10-02` altındaki `md5sum`'lar aynı.
- [ ] K10: `tools/build.sh Release` hatasız biter (yeni uyarı yok; derleme sonucu etkilenmemeli).
- [ ] K11: `git diff --stat gece/2026-10-02...bot/F3-06` yalnızca `tools/bot-telemetry-report.py` ve plan dosyasını gösterir (artı `Durum`/rapor satırları); `GameServer/`, `shared/`, `docs/`, ini dosyalarında fark yok.

## 7. Doğrulama komutları

```bash
python3 tools/bot-telemetry-report.py --selftest
python3 tools/bot-telemetry-report.py /mnt/c/dev/fdp/server/Logs/bots/2026-10-02
python3 tools/bot-telemetry-report.py /mnt/c/dev/fdp/server/Logs/bots/2026-10-02/t1-7-1.jsonl --json | python3 -m json.tool | head -40
python3 tools/bot-telemetry-report.py /mnt/c/dev/fdp/server/Logs/bots/2026-10-02 | md5sum
python3 tools/bot-telemetry-report.py /mnt/c/dev/fdp/server/Logs/bots/2026-10-02 | md5sum
(cd /mnt/c/dev/fdp/server/Logs/bots/2026-10-02 && md5sum *) > /tmp/before.md5   # K9: araç çalıştırıldıktan sonra tekrar, fark yok
./tools/build.sh Release
git diff --stat gece/2026-10-02...bot/F3-06
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `tools/*.py` dosyaları ASCII; yeni dosya da ASCII (`file` ile doğrula), betik içi metinlerde Türkçe karakter yok. Mevcut `tools/*.py` dosyalarının satır sonu LF'tir (`.gitattributes` `.sh` için LF zorlar; `.py` için `file` çıktısı "ASCII text" döner); yeni dosyada CRLF olmasın.
- Yalnızca Python standart kütüphanesi (`json`, `os`, `sys`, `tempfile`, `statistics` vb.). Python 3.8+ ile çalışmalı (WSL'de 3.12; yeni sözdizimi gerekmiyor; `match` ifadesi ve `X | Y` tip yazımı kullanma).
- Büyük dosyalar: dosyayı satır satır oku, tamamını belleğe `readlines()` ile yükleme (yalnızca olay sayaçlarını ve `PERF_SAMPLE` pencerelerini tut).
- Kişisel veri: telemetri dosyalarında yalnızca bot karakter adları vardır (`BotWP_K` ...). Rapor bu adları (composition listesi) yazabilir; gerçek oyuncu adı görürsen rapora kopyalama.
- Bu plan sunucu davranışını değiştirmez: bot sistemi kapalıyken ve açıkken sunucu bu araçtan etkilenmez.
- Sunucuyu açma, ini düzenleme, log silme **yok**; örnek veri yoksa K6/K7'yi raporda "çalıştırılmadı (örnek veri yok)" olarak işaretle, uydurma.
- `PERF_SAMPLE` yüzdelik birleştirmesi bir **tahmin**dir; raporda "est" olarak etiketlendiğinden emin ol, kesin yüzdelik gibi sunma.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F3-06` (taban: `gece/2026-10-02`) — `f4d0163 [F3-06] Telemetri analiz aracı (JSONL -> Markdown/JSON rapor)`; bu rapor ayrı bir commit'te (`[F3-06] Uygulayıcı raporu: UYGULANDI`).
- Değişen dosyalar ve neden:
  - `tools/bot-telemetry-report.py` (yeni, 839 satır): JSONL telemetri dosyalarını/klasörlerini okuyup Markdown (varsayılan) veya `--json` rapor üreten; `--selftest` yerleşik sınamalı; yalnızca Python stdlib. Sunucu koduna dokunulmadı.
  - `plans/F3-06-telemetri-analiz-araci.md`: `Durum` satırı (`HAZIR` → `UYGULANIYOR` → `UYGULANDI`) ve bu rapor.
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
  proj-LogInServer.vcxproj -> ...\build\bin\x86-Release\Server\LogInServer.exe
  proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  ```
  Yeni uyarı yok; görülen uyarılar yalnızca eski satırlarda (`GameServerDlg.cpp` 816/1143/1802, `UpgradeHandler.cpp` 634/862). Değişen dosya Python olduğu için derlemeyi etkilemez.
- Kabul kriterleri öz-değerlendirme:
  - **K1 ✔** `file`: `Python script, ASCII text executable`; import'lar yalnızca `json, os, statistics, sys, tempfile`; CRLF/BOM yok (LF).
  - **K2 ✔** argümansız → `USAGE`, çıkış 2; `--bogus x.jsonl` → `unknown argument: --bogus` + `USAGE`, çıkış 2; `/nonexistent/path` → `error: cannot read path: no such path`, çıkış 2.
  - **K3 ✔** `--selftest` çıkış 0, son satır `selftest OK`. §5.6 madde eşlemesi:
    1. *Geçerli maç:* `assert match["status"] == "VALID"`, `duration_s == 15.364`, `perf_samples == 3`, `warnings == []`; MET pencereleri için `windows==3, ticks==40, p50_med_us==2, p95_est_us==225` (ağırlıklı: `(10·100+10·200+20·300)/40`), `p95_worst_us==300, p99_worst_us==400, max_us==450, in_game_max==4, verdict=="OK"`; ayrıca Markdown'da `| 15.4 |`.
    2. *Geçersizlik kodları:* `bad.jsonl` (mode `eval`, `in_game=4 < n_comp=5`, `result=aborted`, `dropped_hard=1`, `TEST_TELEPORT`, ilk satır bozuk) → `reasons == ["ABORTED","SPAWN_SHORT","DROPPED_HARD","TELEPORT_IN_EVAL","BAD_LINES"]`, `status=="INVALID"`, `first_bad_line==1`; ayrıca `noend.jsonl` → `reasons == ["NO_END"]`.
    3. *Bütçe:* `build_perf_row` doğrudan: `5000→OK` (bütçe 5000), `5001→FAIL`, `in_game=17` + `5001→15000/OK`, `in_game=65→NO_BUDGET`, pencere yok → `NO_DATA`.
    4. *SELFTEST/bilinmeyen olay:* 3 `SELFTEST` + 2 `DECISION` + 1 `PERF_SAMPLE` → `ignored_selftest==3`, `events == {"DECISION":2,"PERF_SAMPLE":1}` (SELFTEST yok), Markdown'da `| SELFTEST |` yok ve `ignored SELFTEST lines: 3`.
    5. *summary.json çapraz denetimi:* doğru özet (K1 senaryosu) → uyarı yok; `lines=99` yapılmış özet → `warnings` içinde `summary mismatch`.
    6. *Klasör/determinizm/JSON:* `collect_paths` özyinelemeli, `t1.summary.json` giriş değil, sıralı; aynı basename iki klasörde → `a/t1.jsonl`/`b/t1.jsonl`; `gather` iki kez → `render_markdown` ve `render_json` bayt-eşit; `json.loads(render_json(...))` sözlük döndürür.
  - **K4 ✔** Bütçe sınır değerleri K3/3'te assert ediliyor (yukarıda); değerler kodda açıkça okunabilir.
  - **K5 ✔** Gerçek klasör iki kez çalıştırıldı, `md5sum` aynı: `f43b25869d44fc30d869956cd2475397`.
  - **K6 ✔** Gerçek veride çıkış 0; `Matches` tablosunda dört maç var; `summary mismatch` uyarısı yok (`Warnings (none)`). `t1-7-1`: `duration_s 15.4`, `perf_samples 3`, `p95_worst_us 1270`, `max_us 1638`, `in_game_max 4`, `verdict OK`. `t3-4294967295-3`: `duration_s 8.8`, `p95_worst_us 95`, `max_us 1244`. (`t2-5-4` beklenen şekilde `INVALID/ABORTED`, özetiyle uyumlu.)
  - **K7 ✔** `live-063701.jsonl` (8197 satır; `grep -c SELFTEST` = 8192) hatasız işlendi; `Events` tablosunda SELFTEST yok, `ignored SELFTEST lines: 8192` yazıyor; tüm komut ~0,15 sn (< 5 sn).
  - **K8 ✔** `--json` çıktısı `json.load` ile çözülüyor; `--out /tmp/f306-x.md` dosyaya doğrudan çıktıyla bit-eşit Markdown yazıyor, stdout/stderr 0 bayt; `INVALID` maç içeren geçici girdiyle `--strict` çıkış 1, `--strict` olmadan çıkış 0.
  - **K9 ✔** Çalıştırma öncesi/sonrası `Logs/bots/2026-10-02` altındaki 14 dosyanın tamamı `md5sum -c` ile OK.
  - **K10 ✔** `tools/build.sh Release` rc=0; yeni uyarı yok.
  - **K11 ✔** `git status` yalnızca `tools/bot-telemetry-report.py` (yeni) ve plan dosyasını (Durum + rapor) gösteriyor; `GameServer/`, `shared/`, `docs/`, ini dosyalarında fark yok.
- Plandan sapmalar ve gerekçeleri:
  - `p95_est_us` ve `p50_med_us` tamsayıya yuvarlandı (`int(round(...))`); §5.5 "Sayılar tamsayı" kuralına uymak için. Selftest değerleri tam sayı olacak şekilde seçildi.
  - `Matches` satırı `MATCH_END` veya `MATCH_START`'dan en az biri varsa üretilir (planda yalnızca "en fazla bir maç" deniyor); yalnızca `MATCH_END` içeren kuramsal dosya da raporlanır.
  - Bir dosyada ikiden çok `MATCH_START` olsa da `multiple MATCH_START (using first)` uyarısı yalnızca bir kez eklenir (tekrarı önlemek için).
  - `--selftest` dışında `--out` yazımı UTF-8/LF; planla uyumlu.
- Açık sorular:
  - `summary.json` çapraz denetim uyarıları dosya adı içermiyor (plan metni `summary mismatch: lines 5 != 6` biçimini istiyor). Aynı uyarı birden çok dosyada oluşursa ayırt etmek zor olabilir; ileride `<dosya>: ...` öneki eklenmesi istenirse küçük bir değişikliktir.
  - `Events` bölümü boşsa `(none)` yazılıyor; plan yalnızca `Matches`/`Warnings` için `(none)` belirtiyordu (zararsız ek).

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F3-06` @ `<sha>`
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ / ✘ | dosya:satır / komut çıktısı |

- Bulgular (önem sırasıyla):
  1. …
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
…
```
