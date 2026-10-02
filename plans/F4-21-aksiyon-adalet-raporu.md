# F4-21: Betikli test dizisi, dilim 3 — `tools/bot-telemetry-report.py` MET-ACT-02 / MET-FAIR-01 / betik raporu

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-02, gece/2026-10-02, merge `d803438`) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-21` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F3-06 (`tools/bot-telemetry-report.py`) — `KAPANDI`; F4-20 (`SCRIPT_*` telemetrisi) — `KAPANDI` (merge `e36d9d1`); F4-01..F4-18 (`ACTION_*`/`FAIRNESS_REJECT` yazan kod) — `KAPANDI` |
| İlgili gereksinim / kabul | `docs/17` F4 "Test: MET-ACT-02; MET-FAIR-01" ve "Kabul: betikli dizilerde sunucuya giden geçersiz aksiyon ≤ %1"; MET-ACT-02, MET-FAIR-01 (`docs/16` §6.2); ADR-0017 Eki F4-21 |
| Tahmini büyüklük | S (tek Python dosyası; sunucu koduna dokunmaz) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

`tools/bot-telemetry-report.py` şu an yalnızca maç sınırlarını, olay sayaçlarını ve `PERF_SAMPLE` bütçesini raporluyor. Bu plan bittiğinde aynı araç, F4 kabulünün iki ölçütünü telemetri dosyalarından hesaplar: **MET-ACT-02** (geçersiz aksiyon oranı + sebep dağılımı, PASS/WARN/FAIL hükmü) ve **MET-FAIR-01** (guard reddi sayısı, bot-saat başına oran, kural/sebep dağılımı); ayrıca her `/bot script run` koşusunu (`SCRIPT_START/STEP/END`) tek satırda özetler ve o koşunun penceresindeki aksiyon/ret sayılarını yanına koyar. Sunucu koduna dokunulmaz; bot sistemi kapalıyken ve açıkken sunucu bu araçtan etkilenmez.

## 2. Bağlam (okunması zorunlu)

- `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md` §3.2 (olay tablosu, `ACTION_SUBMIT/RESULT`, `FAIRNESS_REJECT`, `SCRIPT_*`), §5.1 (sonuç kodları), §6.2 (MET-ACT-02 satırı: `ACTION_RESULT` içinde `SRV_FAIL_*` / tüm `ACTION_SUBMIT`, ≤ %2, sebep dağılımı raporlanır; MET-FAIR-01: `FAIRNESS_REJECT` / bot-saat, bilgi amaçlı).
- `docs/17` F4 kabul satırı: betikli dizilerde sunucuya giden geçersiz aksiyon **≤ %1** (MET-ACT-02'nin 2 % sınırından daha sıkı; ikisi de raporlanır).
- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` Eki F4-21 (bu planın sınıflandırma kararı) ve Eki F4-20 madde 5 (`SCRIPT_*` alanları).
- `tools/bot-telemetry-report.py` (839 satır; satırlar commit `e36d9d1` başına): `load_file` satır 98 (satır satır okuyucu, `info` sözlüğü), `gather` 401, `render_markdown` 479, `render_json` 568, `has_violation` 572, `make_record` 582, `run_selftest` 614, `main` 769. Araç yalnızca standart kütüphane, ASCII, satır satır okur.
- Telemetri biçimi (doğrulandı, `GameServer/Bot/Telemetry.cpp:536`): her satır `{"t":<ms>,"match":"..","bot":<int>,"name":"..", ...alanlar}`; `ev` alanı olay adıdır. `ActionExecutor.cpp` `ACTION_SUBMIT` alanları: `decision_id`, `type`, (tipe özgü ek alanlar); `ACTION_RESULT`: `decision_id`, `type`, `ok` (**JSON bool**), `reason` (string), `latency_us`, tipe özgü ek alanlar; `FAIRNESS_REJECT`: `decision_id`, `type`, `rule`, `reason`, `value`, `limit` (`ActionExecutor.cpp:55-68`). `SCRIPT_*` olayları `bot:-1` ve `name` yok (`ScriptRunner.cpp:173,211,281`).
- `ACTION_RESULT.reason` değerleri koddan sayıldı (başarısızlıkta `ok:false` olanlar): `srv_fail` (sunucu handler'ı reddetti: saldırı sonuç kodu ≠ başarı, `MAGIC_FAIL`; `ActionExecutor.cpp` ~470, 633-648, 1058), `handler_noop` (`Move`: handler botu oynatmadı, ~line 130), `refused_target`/`refused_level`/`refused_zone`/`refused_other` (party daveti reddi, ~1895-1902), `no_result` (sunucudan beklenen yayın gelmedi; birçok tip). Başarı: `ok`, `hit`, `killed`, `casting`, `effected`, `missed`, `applied`, `observed`, `respawned`, `joined`, `created`, `sent`, `left`, `disbanded`, `kicked`, `promoted`, `declined`, `received`.

## 3. Kapsam

**Yapılacaklar**

- Telemetri okuyucusuna aksiyon, ret ve betik toplayıcıları ekle (`load_file`).
- Üç yeni rapor bölümü: MET-ACT-02, MET-FAIR-01, Scripts (+ iki sebep tablosu); Markdown ve JSON çıktıda.
- `--strict` yalnızca MET-ACT-02 **FAIL** hükmünde (> %2) çıkış kodunu 1 yapar (WARN yapmaz).
- Birim (selftest) vakaları ve tek bir sentetik örnek dosya üzerinden elle doğrulama.

**Kapsam dışı (yapılmayacak)**

- `GameServer/`, `BotCore/`, `AIServer/`, `shared/`, `Tests/`, `docs/`, `plans/` dosyalarına dokunmak. (Doküman ve ADR güncellemesini Claude yapar.)
- MET-ACT-01/-03 (fırsat tespiti ve gecikmesi; karar katmanı yok), MET-PERF-xx değişiklikleri, `PERF_SAMPLE` tablosunu yeniden düzenlemek.
- "Sunucuya ulaşan ihlal = 0" ölçümü: telemetriden hesaplanamaz (guard reddi sunucuya gitmez; ihlalli paket ancak guard atlanırsa olur). Rapor bunu **"ölçülmedi, kod incelemesiyle doğrulanır"** diye yazar; sayı uydurulmaz.
- Betik adımı başına ayrıntılı ilişkilendirme (yalnızca koşu penceresi), saat/duvar zamanı dönüşümü, grafik, yeni komut satırı seçeneği (yalnızca mevcut `--json`, `--out`, `--strict`, `--selftest`).
- Sunucuyu açmak, `.ini` düzenlemek, log silmek.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/bot-telemetry-report.py` | değiştir | tek dosya; yeni dosya yok |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

Sabitler (dosya başı, `VERDICT_NOTE` yanına):

```python
ACT_GATE_PCT = 1      # docs/17 F4 acceptance: invalid actions <= 1 % on scripted runs
ACT_LIMIT_PCT = 2     # docs/16 MET-ACT-02: <= 2 %
SRV_INVALID_REASONS = ("srv_fail", "handler_noop")   # plus every reason starting with "refused_"
```

1. **Sınıflandırma (saf fonksiyon).** `classify_action_result(ok, reason)` döner:
   - `ok is True` → `"ok"`;
   - `ok is False` ve (`reason in SRV_INVALID_REASONS` veya `reason` `"refused_"` ile başlıyor) → `"invalid"` (sunucu reddetti / handler etkisiz kaldı);
   - `ok is False` ve `reason == "no_result"` → `"no_result"` (sunucudan onay yayını gelmedi; **MET-ACT-02 payına girmez**, ayrı sayılır);
   - diğer her şey (`ok` bool değil, `reason` string değil, bilinmeyen sebep) → `"other"` (uyarıya yazılır).
   `act_verdict(submit, invalid)` döner (tam sayı karşılaştırması, kayan nokta yok): `submit == 0` → `"NO_DATA"`; `invalid * 100 <= submit * ACT_GATE_PCT` → `"PASS"`; `invalid * 100 <= submit * ACT_LIMIT_PCT` → `"WARN"`; aksi `"FAIL"`.

2. **Toplayıcılar (`load_file` içinde, mevcut `event` dallanmasına ekle; `SELFTEST` satırları zaten önce çıkıyor).** `info`'ya şu alanları ekle:
   - `act_submit`: `{type: adet}` (`ACTION_SUBMIT`; `type` string değilse `"?"`).
   - `act_result`: `{type: {"ok":n,"invalid":n,"no_result":n,"other":n}}` ve `act_reasons`: `{(type, reason, sınıf): n}` yalnızca sınıf ≠ `ok` için (`reason` string değilse `"?"`).
   - `fair`: `{"count": n, "by": {(type, rule, reason): n}}` (`FAIRNESS_REJECT`; eksik alan `"?"`).
   - `bots`: `bot` alanı int ve ≥ 0 olan `ACTION_SUBMIT`/`ACTION_RESULT`/`FAIRNESS_REJECT` satırlarının küme olarak kimlikleri.
   - `t_first`, `t_last`: `t` alanı int olan tüm (SELFTEST olmayan) satırların en küçük/en büyük `t` değeri.
   - `scripts`: koşu listesi. `SCRIPT_START` yeni koşu açar: `{"script","steps","duration_ms","start_t","end_t":None,"result":None,"steps_run":None,"steps_total":None,"elapsed_ms":None,"max_late_ms":None,"steps_seen":0,"submit":0,"invalid":0,"no_result":0,"rejects":0}`; açık bir koşu varken yeni `SCRIPT_START` gelirse eski koşunun `result`'ı `"NO_END"` yapılıp kapatılır. `SCRIPT_STEP` açık koşunun `steps_seen`'ini artırır. `SCRIPT_END` açık koşuyu `completed`/`stopped` (kayıttaki `result`) ve `steps_run/steps_total/elapsed_ms/max_late_ms/end_t` ile kapatır. Açık koşu varken gelen `ACTION_SUBMIT` → `submit++`; `ACTION_RESULT` sınıfına göre `invalid++`/`no_result++`; `FAIRNESS_REJECT` → `rejects++` (bu **dosya sırası** penceresidir; zamana bakılmaz). Dosya bitince hâlâ açık koşunun `result`'ı `"NO_END"` (sunucu betik sürerken kapandı: `ScriptRunner` o durumda `SCRIPT_END` yazmaz, ADR-0017 Eki F4-20 madde 6), `steps_run` = `steps_seen`.
   - Dosya sonunda uyarılar (`info["warnings"]`'a, dosya adı = `os.path.basename(path)` ön ekli): tip başına `act_submit[type] != toplam act_result[type]` ise `"<dosya>: ACTION_SUBMIT/ACTION_RESULT differ for <type> (<s> vs <r>)"`; `other` sınıfı sayısı > 0 ise `"<dosya>: <n> ACTION_RESULT with unknown ok/reason"`; kapanmış koşuda `SCRIPT_END.steps_run != steps_seen` ise `"<dosya>: script <ad> steps_run <a> != SCRIPT_STEP <b>"`.

3. **Satır üreticileri (saf fonksiyonlar, `gather` çağırır).**
   - `build_action_row(name, info)` → yalnızca `submit` toplamı > 0 veya `result` toplamı > 0 ise sözlük, aksi `None`: `file`, `submit`, `result`, `ok`, `invalid`, `no_result`, `other`, `invalid_pct` (`round(invalid * 100.0 / submit, 2)`, `submit == 0` ise `None`), `verdict` (`act_verdict`).
   - Birden çok satır varsa sonuna `file == "(total)"` toplam satırı ekle (aynı alanlar, sayılar toplanır, `verdict` toplamdan yeniden hesaplanır). Tek satır varsa toplam satırı eklenmez.
   - `build_fair_row(name, info)` → `fair["count"] > 0` ise: `file`, `rejects`, `bots` (`max(len(info["bots"]), in_game_max)`; `in_game_max` = `perf_windows`'taki `in_game` en büyüğü, pencere yoksa 0), `span_s` (`(t_last - t_first) / 1000.0`, `t` yoksa `None`), `bot_hours_est` (`bots * span_s / 3600.0`; `bots == 0` veya `span_s` `None`/0 ise `None`), `rejects_per_bot_hour_est` (`rejects / bot_hours_est`, `bot_hours_est` yok ise `None`; oran **yuvarlanmamış** bot-saatten hesaplanır, yuvarlama yalnızca sonuçta: `round(.., 2)`, `bot_hours_est` 4 hane). Etiket: "est" (tahmin).
   - `reason` tabloları: `action_reasons` = tüm dosyalardan toplanan `(type, reason, sınıf, count)` satırları; `fairness_reasons` = `(type, rule, reason, count)`; ikisi de `(-count, ...)` sıralı (belirlenimli).
   - `build_script_rows(name, info)` → her koşu için: `file`, `script`, `steps_total` (`SCRIPT_END`'den; yoksa `SCRIPT_START.steps`), `steps_run`, `result`, `elapsed_ms` (yoksa `None`), `max_late_ms` (yoksa `None`), `submit`, `invalid`, `no_result`, `rejects`.

4. **`gather` ve rapor sözlüğü.** `report`'a şu anahtarları ekle (mevcut anahtarlar ve değerleri **değişmez**): `actions`, `action_reasons`, `fairness`, `fairness_reasons`, `scripts` (hepsi liste, boşsa `[]`).

5. **`render_markdown`.** `## MET-PERF-02` bölümünden sonra, `## Events`'ten önce şu bölümler (her biri boşsa `(none)`):
   - `## MET-ACT-02 (invalid actions)`: `| file | submit | result | ok | invalid | no_result | other | invalid_pct | verdict |` tablosu, ardından not: `Invalid = ACTION_RESULT ok=false with reason srv_fail, handler_noop or refused_*; no_result (no server confirmation) and guard rejections are not counted. PASS <= 1 % (F4 gate), WARN <= 2 % (MET-ACT-02), FAIL above. All action types are included.`
   - `## Action failures by reason`: `| type | reason | class | count |`.
   - `## MET-FAIR-01 (fairness rejects)`: `| file | rejects | bots | span_s | bot_hours_est | rejects_per_bot_hour_est |`; not: `Informational. Rejects never reach the server. "Violations that reached the server = 0" is not measurable from telemetry (verify by code review).`
   - `## Fairness rejects by rule`: `| type | rule | reason | count |`.
   - `## Scripts`: `| file | script | steps_total | steps_run | result | elapsed_ms | max_late_ms | submit | invalid | no_result | rejects |`; not: `Counts are attributed by file order between SCRIPT_START and SCRIPT_END (approximate: follow-up results of the last step may land after SCRIPT_END).`
   Mevcut bölüm başlıkları ve sırası aynı kalır.

6. **`has_violation`.** `report["actions"]` içinde `verdict == "FAIL"` olan satır varsa `True` (diğer koşullar aynen).

7. **Selftest (`run_selftest`) — mevcut 6 vakaya dokunma; sonuna ekle** (`print("selftest OK")` satırından önce). Hepsi geçici dizinde:
   - **Vaka 7 (sentetik betik koşusu).** Tek dosya, şu kayıtlar sırayla (`make_record`; `bot=-1` olanlar `SCRIPT_*`):
     `SCRIPT_START` t=1000 `script="s1"` `steps=2` `duration_ms=5000` bot -1; `SCRIPT_STEP` t=1010 `script="s1"` `step=1` `line=3` `offset_ms=0` `late_ms=10` `verb="move"`; `ACTION_SUBMIT` t=1011 bot 0 `type="Move"`; `ACTION_RESULT` t=1012 bot 0 `type="Move"` `ok=True` `reason="ok"`; `ACTION_SUBMIT` t=1013 bot 1 `type="Attack"`; `ACTION_RESULT` t=1014 bot 1 `type="Attack"` `ok=True` `reason="hit"`; `SCRIPT_STEP` t=2010 `step=2` `line=4` `offset_ms=1000` `late_ms=10` `verb="cast"`; `ACTION_SUBMIT` t=2011 bot 0 `type="CastEffect"`; `ACTION_RESULT` t=2012 bot 0 `type="CastEffect"` `ok=False` `reason="srv_fail"`; `ACTION_SUBMIT` t=2013 bot 0 `type="UsePotion"`; `ACTION_RESULT` t=2014 bot 0 `type="UsePotion"` `ok=False` `reason="no_result"`; `FAIRNESS_REJECT` t=2015 bot 1 `type="Attack"` `rule="CLI-01"` `reason="too_soon"` `value=100.0` `limit=1000.0`; ikinci aynı `FAIRNESS_REJECT` t=2016; `SCRIPT_END` t=3000 `script="s1"` `result="completed"` `steps_run=2` `steps_total=2` `elapsed_ms=2000` `max_late_ms=10` bot -1.
     Beklenenler: `report["actions"]` tek satır; `submit == 4`, `result == 4`, `ok == 2`, `invalid == 1`, `no_result == 1`, `other == 0`, `invalid_pct == 25.0`, `verdict == "FAIL"` (tek satır → `(total)` yok); `has_violation(report)` `True`; `report["fairness"]` tek satır: `rejects == 2`, `bots == 2`, `span_s == 2.0`, `bot_hours_est == 0.0011`, `rejects_per_bot_hour_est == 1800.0` (±0,5 tolerans); `report["fairness_reasons"] == [{"type": "Attack", "rule": "CLI-01", "reason": "too_soon", "count": 2}]`; `report["action_reasons"]` iki satır (`CastEffect/srv_fail/invalid/1`, `UsePotion/no_result/no_result/1`); `report["scripts"]` tek satır: `script == "s1"`, `steps_total == 2`, `steps_run == 2`, `result == "completed"`, `submit == 4`, `invalid == 1`, `no_result == 1`, `rejects == 2`, `max_late_ms == 10`; `report["warnings"]` boş (bu vakada fazladan uyarı **olmamalı**); Markdown çıktıda `## MET-ACT-02`, `## MET-FAIR-01`, `## Scripts` başlıkları ve `FAIL` geçer.
   - **Vaka 8 (sınıflandırma ve hüküm).** `classify_action_result(True, "hit") == "ok"`, `(False, "srv_fail") == "invalid"`, `(False, "handler_noop") == "invalid"`, `(False, "refused_level") == "invalid"`, `(False, "no_result") == "no_result"`, `(False, "weird") == "other"`, `("yes", "ok") == "other"`, `(None, "ok") == "other"`. `act_verdict(0, 0) == "NO_DATA"`, `(200, 2) == "PASS"`, `(200, 3) == "WARN"`, `(200, 4) == "WARN"`, `(200, 5) == "FAIL"`, `(100, 1) == "PASS"`, `(100, 2) == "WARN"`, `(100, 3) == "FAIL"`.
   - **Vaka 9 (eksik betik sonu, uyarı, boş bölümler, belirlenim).** İkinci dosya: `SCRIPT_START` (steps=3), bir `SCRIPT_STEP`, `ACTION_SUBMIT` (bot 2, `type="Move"`), `ACTION_RESULT` (`ok=True`, `reason="ok"`), `ACTION_SUBMIT` (bot 2, `type="Move"`) ve `SCRIPT_END` yok: `scripts[0]["result"] == "NO_END"`, `steps_run == 1`, uyarılarda `ACTION_SUBMIT/ACTION_RESULT differ for Move (2 vs 1)` geçer. Üçüncü dosya yalnızca `PERF_SAMPLE`+`MATCH_START/END` (aksiyon yok): üçünü birlikte `gather` ile ver → `actions` içinde `(total)` satırı vardır, aksiyonsuz dosya satır üretmez; yalnızca üçüncü dosyayla `gather` → `actions == []` ve Markdown'da `## MET-ACT-02` altında `(none)` görünür. Aynı girdiyle iki `gather` → `render_markdown` ve `render_json` çıktıları birebir aynıdır; `json.loads(render_json(...))` içinde `actions`, `fairness`, `scripts` anahtarları var.

8. **Çalıştır ve doğrula (aşağıdaki §7).** Ayrıca sentetik örnek dosyayı (Vaka 7 kayıtları) `/tmp/f4-21-sample.jsonl` olarak yazıp araçla Markdown ve `--json --strict` çalıştır; çıktıyı Uygulayıcı Raporu'na ekle (dosyayı depoya **koyma**).

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/bot-telemetry-report.py --selftest` `selftest OK` basar, çıkış kodu 0 (Vaka 1–9; ilk 6 vaka değiştirilmemiş).
- [ ] K2: `classify_action_result` ve `act_verdict` §5 adım 7 Vaka 8'deki tüm eşleşmeleri verir (selftest içinde assert; Uygulayıcı Raporu'nda sayısı belirtilir).
- [ ] K3: Sentetik koşu (`/tmp/f4-21-sample.jsonl`) için Markdown çıktısında satır `| f4-21-sample.jsonl | 4 | 4 | 2 | 1 | 1 | 0 | 25.0 | FAIL |` görünür; `## MET-FAIR-01` altında `rejects` 2 ve `rejects_per_bot_hour_est` 1800.0; `## Scripts` altında `s1`, `completed`, `submit` 4, `invalid` 1.
- [ ] K4: `python3 tools/bot-telemetry-report.py /tmp/f4-21-sample.jsonl --strict` çıkış kodu 1; aynı komut yalnızca `PERF_SAMPLE` içeren (aksiyonsuz) bir dosyayla çıkış kodu 0. (`echo $?` ile kanıtla.)
- [ ] K5: Eski rapor bölümleri değişmedi: `## Files`, `## Matches`, `## MET-PERF-02`, `## Events`, `## Warnings` başlıkları aynı sırada; selftest Vaka 1–6'nın `assert`'leri **aynen** duruyor (`git diff` yalnızca ekleme gösterir, `-` ile başlayan satır yalnızca `has_violation`, `gather` rapor sözlüğü, `render_markdown` bölüm ekleme noktalarında olabilir).
- [ ] K6: `report["actions"]`, `action_reasons`, `fairness`, `fairness_reasons`, `scripts` anahtarları JSON çıktıda var (`--json` çıktısını `python3 -c 'import json,sys; d=json.load(sys.stdin); print(sorted(d))'` ile göster).
- [ ] K7: `file tools/bot-telemetry-report.py` "ASCII text" (CRLF yok, Türkçe karakter yok); `python3 -m py_compile tools/bot-telemetry-report.py` hatasız; yalnızca standart kütüphane `import`'ları.
- [ ] K8: `git diff --stat main...bot/F4-21` (taban `gece/2026-10-02`: `git diff --stat gece/2026-10-02...bot/F4-21`) yalnızca `tools/bot-telemetry-report.py` ve (uygulayıcı raporu için) bu plan dosyasını gösterir.
- [ ] K9: `tools/build.sh Release` hatasız biter (yeni uyarı yok; sunucu kodu değişmediği için beklenen sonuç değişmez) ve `./tools/run-tests.sh` `82 tests, 0 failed` basar.

## 7. Doğrulama komutları

```bash
python3 -m py_compile tools/bot-telemetry-report.py
python3 tools/bot-telemetry-report.py --selftest; echo "rc=$?"
file tools/bot-telemetry-report.py
python3 tools/bot-telemetry-report.py /tmp/f4-21-sample.jsonl
python3 tools/bot-telemetry-report.py /tmp/f4-21-sample.jsonl --strict >/dev/null; echo "rc=$?"
python3 tools/bot-telemetry-report.py /tmp/f4-21-sample.jsonl --json | python3 -c 'import json,sys; d=json.load(sys.stdin); print(sorted(d))'
./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-21
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `tools/*.py` dosyaları ASCII, LF; betik içi metinlerde Türkçe karakter yok (`file` çıktısı "ASCII text").
- Yalnızca Python standart kütüphanesi; Python 3.8+ uyumlu (`match` ifadesi, `X | Y` tip yazımı, `dict | dict` yok).
- Dosyayı satır satır oku (zaten öyle); tüm satırları belleğe yükleme. Toplayıcılar yalnızca sayaç/küme tutar, ham satır saklamaz.
- JSON çıktı belirlenimli olmalı: sözlük anahtarları `sort_keys=True` ile yazılıyor; liste sıraları `(-count, ...)` gibi tam sıralamayla verilir; kümeleri çıktıya koymadan önce sayıya çevir (`len`).
- JSON'a demet anahtarlı sözlük koyma (`json.dumps` hata verir): sebep tablolarını satır listesi (sözlük) olarak üret.
- `ok` alanını `is True` / `is False` ile denetle: `json` bool'u `True/False` yapar, ama `1`/`"true"` gibi değerler `other` sınıfına gider (sessizce `ok` sayılmasın).
- `submit == 0` iken yüzde hesaplama (sıfıra bölme yok); `bot_hours_est` 0 iken oran yok.
- Kişisel veri: telemetri dosyalarında yalnızca bot karakter adları vardır; rapor `name` alanını yazmaz (yalnızca sayar). Gerçek oyuncu adı görürsen rapora kopyalama.
- Sunucuyu açma, `.ini` düzenleme, log silme **yok**. Gerçek telemetri dosyası yoksa K3/K4'ü yalnızca sentetik örnekle yap ve raporda "gerçek koşu verisi yoktu" yaz; gerçek koşu doğrulaması Claude'dadır.
- Sınıflandırma kararı (hangi sebep "geçersiz" sayılır) ADR-0017 Eki F4-21'dedir; sebep listesini kendin genişletme: yeni bir sebep görürsen `other`'a düşer ve uyarı yazar, Uygulayıcı Raporu'nda soru olarak bildir.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-21` (taban `gece/2026-10-02`) — `ad760d4 [F4-21] MET-ACT-02/MET-FAIR-01 raporu ve betik kosu ozeti (bot-telemetry-report.py)`
- Değişen dosyalar ve neden:
  - `tools/bot-telemetry-report.py`: `ACT_GATE_PCT`/`ACT_LIMIT_PCT`/`SRV_INVALID_REASONS` sabitleri ve üç not metni; `classify_action_result`/`act_verdict`/`add_bot` saf fonksiyonları; `load_file` toplayıcıları (`act_submit`, `act_result`, `act_reasons`, `fair`, `bots`, `t_first`/`t_last`, `scripts`, dosya sonu uyarıları + `NO_END`); `build_action_row`/`build_action_total`/`build_fair_row`/`build_script_rows`; `gather` rapor sözlüğüne `actions`/`action_reasons`/`fairness`/`fairness_reasons`/`scripts`; `render_markdown` beş yeni bölüm (MET-ACT-02, Action failures by reason, MET-FAIR-01, Fairness rejects by rule, Scripts); `has_violation` `actions` FAIL denetimi; `run_selftest` Vaka 7-9.
  - `plans/F4-21-aksiyon-adalet-raporu.md`: yalnızca `Durum` satırı (`HAZIR` → `UYGULANIYOR` → `UYGULANDI`) ve bu rapor.
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  BotCore.vcxproj -> ...\build\bin\x86-Release\libs\BotCore.lib
  Lua.vcxproj -> ...\build\bin\x86-Release\libs\Lua.lib
  shared.vcxproj -> ...\build\bin\x86-Release\libs\shared.lib
  proj-LogInServer.vcxproj -> ...\build\bin\x86-Release\Server\LogInServer.exe
  proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  proj-AIServer.vcxproj -> ...\build\bin\x86-Release\Server\AIServer.exe
  BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  rc=0. `./tools/run-tests.sh` → `82 tests, 0 failed`, rc=0.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ `python3 tools/bot-telemetry-report.py --selftest` → `selftest OK`, rc=0; Vaka 1-6 `assert`'leri değişmedi (git diff'te bu satırlarda `-` yok).
  - K2 ✔ Vaka 8: 8 sınıflandırma + 8 hüküm karşılaştırması (16 assert) selftest içinde.
  - K3 ✔ `/tmp/f4-21-sample.jsonl` (14 kayıt): `| f4-21-sample.jsonl | 4 | 4 | 2 | 1 | 1 | 0 | 25.0 | FAIL |`; `## MET-FAIR-01` satırı `rejects 2`, `rejects_per_bot_hour_est 1800.0`; `## Scripts` satırı `s1`, `completed`, `submit 4`, `invalid 1`.
  - K4 ✔ `--strict` sentetik örnekle rc=1; yalnızca `PERF_SAMPLE` içeren `/tmp/f4-21-plain.jsonl` ile rc=0.
  - K5 ✔ `## Files`, `## Matches`, `## MET-PERF-02`, `## Events`, `## Warnings` sırası aynı; git diff yalnızca ekleme (`-` satırı yok; sadece plan `Durum` satırı değişti).
  - K6 ✔ `--json` anahtarları: `['action_reasons','actions','events','fairness','fairness_reasons','files','ignored_selftest','matches','perf','scripts','warnings']`.
  - K7 ✔ `file` → "Python script, ASCII text executable"; `py_compile` hatasız; yalnızca `json/os/statistics/sys/tempfile` (standart).
  - K8 ✔ `git diff --stat gece/2026-10-02...bot/F4-21` yalnızca `tools/bot-telemetry-report.py` (+545) ve plan dosyası (+1/-1) gösterir (commit sonrası).
  - K9 ✔ `tools/build.sh Release` rc=0 (yeni uyarı yok, sunucu kodu değişmedi); `./tools/run-tests.sh` → `82 tests, 0 failed`.
- Plandan sapmalar ve gerekçeleri:
  - Vaka 7'de `bot_hours_est == 0.0011` karşılaştırması kayan nokta güvenliği için `abs(...) < 1e-9` toleransıyla yazıldı (plan "== 0.0011" diyor; değer aynı).
  - `build_action_total(rows)` dosya adı argümanı almadan `"(total)"` sabitini kullanır (plan §5 adım 3 "file == \"(total)\"" der; davranış aynı).
  - Diğer her şey plan metnine göre.
- Açık sorular:
  - Gerçek telemetri dosyası bu çalışma ağacında yoktu (plan §8: "gerçek koşu verisi yoktu" yaz); K3/K4 yalnızca sentetik örnekle yapıldı. Gerçek `script_smoke_2bot` koşusunun çapraz denetimi Claude'dadır.
  - `ACTION_SUBMIT`/`ACTION_RESULT` farkı, betik dışı normal komut kullanımında da (dosya genelinde) uyarı üretir; plan bu uyarıyı dosya geneli istiyor (§5 adım 2), bu yüzden davranış plandaki gibidir.

  Örnek çıktı (`/tmp/f4-21-sample.jsonl`, yalnızca yeni bölümler):
  ```
  ## MET-ACT-02 (invalid actions)
  | file | submit | result | ok | invalid | no_result | other | invalid_pct | verdict |
  |---|---|---|---|---|---|---|---|---|
  | f4-21-sample.jsonl | 4 | 4 | 2 | 1 | 1 | 0 | 25.0 | FAIL |

  ## Action failures by reason
  | type | reason | class | count |
  |---|---|---|---|
  | CastEffect | srv_fail | invalid | 1 |
  | UsePotion | no_result | no_result | 1 |

  ## MET-FAIR-01 (fairness rejects)
  | file | rejects | bots | span_s | bot_hours_est | rejects_per_bot_hour_est |
  |---|---|---|---|---|---|
  | f4-21-sample.jsonl | 2 | 2 | 2.0 | 0.0011 | 1800.0 |

  ## Fairness rejects by rule
  | type | rule | reason | count |
  |---|---|---|---|
  | Attack | CLI-01 | too_soon | 2 |

  ## Scripts
  | file | script | steps_total | steps_run | result | elapsed_ms | max_late_ms | submit | invalid | no_result | rejects |
  |---|---|---|---|---|---|---|---|---|---|---|
  | f4-21-sample.jsonl | s1 | 2 | 2 | completed | 2000 | 10 | 4 | 1 | 1 | 2 |
  ```

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-21` @ `c468b3a` (2 commit: `ad760d4` kod, `c468b3a` rapor; ikisi de `[F4-21]` önekli, merge/force izi yok). Gece modu (`AUTO_LOOP=1`): birleştirme ve push döngü betiğinde, bu turda yapılmadı. Sunucular kapalıydı (`run-servers.sh status` 0/3).
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `python3 tools/bot-telemetry-report.py --selftest` → `selftest OK`, rc=0 (kendi çalıştırmam). Diff'te Vaka 7-9 `run_selftest` sonuna, `print("selftest OK")` öncesine eklenmiş; Vaka 1-6'ya dokunulmamış (`git diff ... \| grep -c '^-[^-]'` = 0). |
| K2 | ✔ | Vaka 8: 8 `classify_action_result` + 8 `act_verdict` assert'i (`bot-telemetry-report.py` Vaka 8 bloğu), planın tüm eşleşmeleri; selftest geçti. Kod: `classify_action_result` `tools/bot-telemetry-report.py:99` (`ok is True/False` denetimi, `refused_` öneki), `act_verdict` `:113` (tam sayı karşılaştırması). |
| K3 | ✔ | `/tmp/f4-21-sample.jsonl` (Vaka 7 kayıtları, kendim yazdım): `\| f4-21-sample.jsonl \| 4 \| 4 \| 2 \| 1 \| 1 \| 0 \| 25.0 \| FAIL \|`; MET-FAIR-01 `2 / 2 / 2.0 / 0.0011 / 1800.0`; Scripts `s1 / 2 / 2 / completed / 2000 / 10 / 4 / 1 / 1 / 2`; Warnings `(none)`. |
| K4 | ✔ | `--strict` örnekle `rc=1`; aksiyonsuz (`PERF_SAMPLE`) dosyayla `rc=0`; gerçek koşu (`live-184432.jsonl`, PASS) ile de `rc=0`. |
| K5 | ✔ | Bölüm sırası: `## Files`, `## Matches`, `## MET-PERF-02`, (yeni beş bölüm), `## Events`, `## Warnings`. Araç dosyası diff'inde silinen satır 0; `gather`/`has_violation`/`render_markdown` yalnızca ekleme. |
| K6 | ✔ | `--json` anahtarları: `['action_reasons','actions','events','fairness','fairness_reasons','files','ignored_selftest','matches','perf','scripts','warnings']`. |
| K7 | ✔ | `file` → "Python script, ASCII text executable"; CR sayısı 0; ASCII dışı karakter yok; `py_compile` hatasız; importlar `json/os/statistics/sys/tempfile`. |
| K8 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-21` yalnızca `tools/bot-telemetry-report.py` (+545) ve plan dosyası; plan diff'inde yalnızca `Durum` satırı ve Uygulayıcı Raporu şablonu dolduruldu. |
| K9 | ✔ | `./tools/build.sh Release` rc=0, `warning C`/`error C` sayısı 0 (çıktı `/tmp/f4-21-build.log`); `./tools/run-tests.sh` → `82 tests, 0 failed`, rc=0. |

- Ek doğrulama (plan §8: gerçek koşu Claude'da): F4-20 doğrulamasından kalan gerçek dosya `/mnt/c/dev/fdp/server/Logs/bots/2026-10-02/live-184432.jsonl` araçla çalıştırıldı. Ham sayım (`grep -c`): SUBMIT 3, RESULT 3, FAIRNESS_REJECT 1, SCRIPT_START 3, SCRIPT_STEP 18, SCRIPT_END 3 = araç çıktısı (submit 3, result 3, ok 3, invalid 0, `PASS`; rejects 1, `State/CLI-13/toggle`, 2 bot, 166.165 sn, bot-saat 0.0923, 10.83/bot-saat). Betik satırları F4-20 çalışma zamanı kaydıyla uyuşuyor: `smoke` 7/7 `completed` (max_late 99), `longrun` 7/10 `stopped`, `fair` 4/4 `completed` ve `rejects` 1. Bu veride `srv_fail`/`no_result` yok; `invalid` yolu yalnızca sentetik veriyle sınandı.
- Proje kuralları: sunucu/`BotCore`/`docs` dosyasına dokunulmamış, mekanik ve bot avantajı etkilenmez; bot sistemi varsayılanı değişmez (yalnızca Python aracı); araç yalnızca sayaç/küme tutuyor, ham satır saklamıyor, `name` alanı rapora yazılmıyor (gerçek koşu çıktısında bot adı yok); JSON'da demet anahtarlı sözlük yok (sebep tabloları satır listesi), sıralar `(-count, ...)` tam sıralı, iki `gather` çıktısı birebir aynı (Vaka 9).
- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. Not: `ACTION_SUBMIT`/`ACTION_RESULT` fark uyarısı dosya geneli ve gece/canlı bir dosyada, sonuç henüz gelmemiş son aksiyon varken (ör. sunucu kapanışı) yanlış alarm üretebilir; plan §5 adım 2 bunu böyle istiyor, uygulayıcı da raporunda belirtti. Kabul edilebilir.
  2. Not: betik penceresi dosya sırasıyla atanıyor; `SCRIPT_END` sonrası gelen son-adım sonuçları sayılmaz (rapor notu bunu söylüyor). Planla uyumlu.
  3. Not: üst üste binen `SCRIPT_START` ile kapanan koşuda `steps_run` `-` görünür (yalnızca `result=NO_END`); plan yalnızca `result`'ı istiyor.
  4. Sapmalar kabul: `bot_hours_est` kontrolü `abs(..) < 1e-9` ile (değer aynı), `build_action_total` dosya adı argümansız `"(total)"` sabitiyle.
- Düzeltme talimatı: yok (karar DOĞRULANDI).
