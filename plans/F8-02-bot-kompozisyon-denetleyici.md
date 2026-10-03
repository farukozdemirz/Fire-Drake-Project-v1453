# F8-02: Kompozisyon ve karakter seti denetleyicisi: `tools/bot-composition-check.py` (C8-A..D, 16/20 karakter ihtiyacı, `db/002` metin ayrıştırma, `--selftest`)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F8 — Değerlendirme ve 8v8 (`docs/17` §2; paralel hat `nav`, değerlendirme/analiz araçları: `docs/17` §1 "analiz araçları her fazla paralel") |
| Branch | `bot/F8-02 (taban: gece/2026-10-02-nav)` |
| Bağımlı olduğu planlar | F8-01 `KAPANDI` (merge `43c4e02`; yalnızca üslup/`--selftest` kalıbı için). Karakter seti kaynağı `db/002` F1-04 `KAPANDI` |
| İlgili gereksinim / kabul | `docs/15` §6a (16/20 karakter dökümü), `docs/09` §2.3/§2.4 (kompozisyonlar, REQ-PTY-02), `docs/04` §3.3, `docs/reports/degerlendirme-takip.md` M6 (`db/003` ön hazırlığı), ADR-0002 Eki F8-02 |
| Tahmini büyüklük | S (2 yeni dosya, hepsi `tools/` altında; `GameServer/`, `BotCore/`, `Tests/`, `db/`, `docs/` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü, hat `nav`, konu sırası dilim 3) |

---

## 1. Amaç

`docs/15` §6a "8v8 için 16, kompozisyon çeşitliliği için 20 karakter gerekir; bugün 12 var" der ama bunu hesaplayan ve **eksik karakterleri adıyla listeleyen** bir şey yoktur. `db/003` (F8 ön koşulu) yazılmadan önce tam olarak hangi karakterlerin eksik olduğu, hangi adlarla ekleneceği ve her EVAL senaryosunun mevcut karakter setiyle kurulup kurulamayacağı tek komutla görülmelidir. Bu plan, `db/002` (ve ileride `db/003`) SQL dosyasındaki `@bots` satırlarını **metin olarak** okuyup kompozisyon ihtiyaçlarını (C2..C5, C8-A..D) hesaplayan, doc ile sürüklenmeyi `--selftest` ile yakalayan kalıcı bir Python aracı yazar.

```
python3 tools/bot-composition-check.py [--sql PATH ...] [--max-bots N] [--json] [--strict] [--target T]
python3 tools/bot-composition-check.py --selftest
```

Araç **yalnızca okur** (CLI modunda hiçbir dosya yazmaz), SQL çalıştırmaz, DB'ye/sunucuya bağlanmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0002-karakter-kurulum-betigi.md` **Ek F8-02** (bu planla birlikte yazıldı): ulus başına ihtiyaç kuralı, ek karakter adlandırması, bilinçli ertelenenler. Bu planın §5'i onu uygular; çelişirse **ADR Ek F8-02** kazanır ve durup soru yazılır.
- `docs/09` §2.3 (REQ-PTY-02: bir party'de en fazla iki priest), §2.4 (satır 46–53): C8-A..D tablosu ve küçük takımlar `C2`..`C5`. **Kompozisyon verisinin tek kaynağı bu tablodur**; araç içindeki tablo onun kopyasıdır ve `--selftest` iki yönlü karşılaştırır (§5.6 `doc09_*`).
- `docs/15` §6a (satır 272–280): 12 → 16 → 20 dökümü; "20 karakter 16'nın üstüne 4 ek karakterdir". Senaryo kimlikleri ve kompozisyon eşleşmeleri `docs/15` satır 178–183 (EVAL-2v2..5v5: C2..C5 vs kendisi; EVAL-8v8-A: C8-A vs C8-A; EVAL-8v8-MIX: C8-B vs C8-C).
- `db/002_bot_characters.sql:124-137`: `@bots` tablo değişkenine `INSERT ... VALUES` satırları: `('WP', 'BotWP_K', 'BotAccWPK', 1, 1, 106, 255, ...)` = profil, karakter adı, hesap adı, ulus (1 Karus / 2 El Morad), ırk, sınıf kodu, stat... Sınıf kodları: WP/WG 106/206, PHD/PHB 112/212, MF/MI 110/210. Satır 29–62'deki iki sahiplik önkontrolü `('BotWP_K','BotAccWPK')` biçiminde **iki** sütunlu demetlerdir: ayrıştırıcı bunları saymamalıdır. Dosya ASCII, LF (`file` ile doğrulandı).
- `docs/04` §3.3/§3.4: hesap adı yalnızca harf/rakam; `shared/globals.h:12` `MAX_ID_SIZE 20` (karakter adı üst sınırı). Eşzamanlı bot sınırı: `GameServer/Bot/BotManager.cpp:168` `MAX_BOTS` varsayılan 16, `GameServer/Bot/ScenarioRunner.cpp:16` `SCENARIO_MAX_BOTS = 16`.
- Üslup örneği: `tools/bot-outcome-eval.py` (başlık docstring'i, `USAGE`, `--selftest` → `PASS <ad>`/`FAIL <ad>: ...` ve son satır `SELFTEST PASS n=<N>`), `tools/nav-regress.py`. Aynı dil: yalnızca standart kütüphane, ASCII, LF, Python 3.8 uyumlu.

## 3. Kapsam

**Yapılacaklar**

1. `tools/bot-composition-check.py` (yeni): SQL dosyalarından karakter satırlarını ayrıştırır, doğrular, kompozisyon/küme/çift ihtiyaçlarını hesaplar, eksik karakterleri listeler (§5).
2. `tools/bot-composition-check/sample-20.sql` (yeni): §5.7'deki 20 satırlık örnek (hedef durum: `db/003` sonrası 20 karakter). Yürütülebilir SQL **değildir**; CLI ve `--selftest` için ayrıştırıcı girdisidir.
3. `--selftest`: §5.6'daki vakalar (≥ 58).

**Kapsam dışı (yapılmayacak)**

- `db/003` yazmak, DB'ye bağlanmak, `sqlcmd` çalıştırmak, `BOT_TABLE`'ı SQL/ini kaynaklı yapmak, `GameServer/*`, `AIServer/*`, `shared/*`, `BotCore/*`, `Tests/*`, `db/*`, `docs/*`, `plans/README.md`, `tools/bot-outcome-eval.py` değişmez.
- Irk (`Race`), stat, skill, ekipman denetimi (`tools/bot-gear-report.py`, `db/002` T-DATA-01 sorumluluğu).
- Kompozisyonu tanımsız senaryolar: EVAL-1v1 (36 ikili), EVAL-5v8, EVAL-HEALSTALL, EVAL-WIPE. CMP-01..04 önerileri (yalnızca REQ-PTY-02 denetlenir).
- 32/64 bot performans karakter kümesi (`docs/15` §6a son cümle), politika/taktik profilleri, takım kurma akışı.
- Sunucu çalıştırmak, `tools/run-servers.sh` (paralel hat).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/bot-composition-check.py` | yeni | LF (`*.py text eol=lf`), yalnızca ASCII, standart kütüphane, Python 3.8 uyumlu (`match` deyimi, `X \| Y` tür birleşimi, `list[int]` yok), `#!/usr/bin/env python3` |
| `tools/bot-composition-check/sample-20.sql` | yeni | §5.7'deki 22 satır **aynen**; LF; ASCII |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. `.vcxproj` değişmez (derlemeye girmez).

## 5. Uygulama adımları

1. `git switch -c bot/F8-02 gece/2026-10-02-nav`; `Durum` → `UYGULANIYOR`. `tools/bot-outcome-eval.py` başlığına ve `run_selftest` kalıbına bak (kopyalama yok; aynı üslup).

2. **Sabit tablolar (araç içinde).** Profil sırası her yerde `WP WG PHD PHB MF MI` (profil kodları: W-P→`WP`, W-G→`WG`, P-HD→`PHD`, P-HB→`PHB`, M-F→`MF`, M-I→`MI`). Ulus kodları: `K` = 1 (Karus), `E` = 2 (El Morad). Sınıf kodu tablosu (`db/002`'den): `WP`/`WG` → K 106, E 206; `PHD`/`PHB` → K 112, E 212; `MF`/`MI` → K 110, E 210. Priest = `PHD`, `PHB`.

   | Kimlik | WP | WG | PHD | PHB | MF | MI | Boyut |
   |---|---|---|---|---|---|---|---|
   | C8-A | 2 | 1 | 1 | 1 | 2 | 1 | 8 |
   | C8-B | 3 | 1 | 1 | 1 | 1 | 1 | 8 |
   | C8-C | 1 | 1 | 1 | 1 | 3 | 1 | 8 |
   | C8-D | 3 | 1 | 1 | 0 | 2 | 1 | 8 |
   | C2 | 1 | 0 | 1 | 0 | 0 | 0 | 2 |
   | C3 | 1 | 0 | 1 | 0 | 1 | 0 | 3 |
   | C4 | 1 | 1 | 1 | 0 | 1 | 0 | 4 |
   | C5 | 1 | 1 | 1 | 1 | 1 | 0 | 5 |

   Bu tablo `docs/09` §2.4/satır 53'ten alınmıştır (C5 = C4 + P-HB). Çıktı sırası: C8-A, C8-B, C8-C, C8-D, C2, C3, C4, C5. `rule=ok` ⇔ priest sayısı ≤ 2 (REQ-PTY-02), değilse `rule=too_many_priests`.

   **Kümeler** (ulus başına ihtiyaç; ADR Ek F8-02 madde 2): `small` = C2..C5'in profil başına en büyüğü (WP1 WG1 PHD1 PHB1 MF1 MI0 → 5/ulus, 10); `min16` = C8-A (8/ulus, 16); `full20` = C8-A..D'nin profil başına en büyüğü (WP3 WG1 PHD1 PHB1 MF3 MI1 → 10/ulus, 20). **Çiftler** (`docs/15` satır 178–183): `EVAL-2v2` (C2/C2), `EVAL-3v3` (C3/C3), `EVAL-4v4` (C4/C4), `EVAL-5v5` (C5/C5), `EVAL-8v8-A` (C8-A/C8-A), `EVAL-8v8-MIX` (C8-B/C8-C; **iki atama** ayrı satır: karus=C8-B el_morad=C8-C, sonra karus=C8-C el_morad=C8-B). Taraflar farklıysa iki atama da yazılır, aynıysa tek satır.

3. **SQL ayrıştırma** (`--sql PATH` tekrarlanabilir, sırayla okunur; verilmezse `<araç klasörü>/../db/002_bot_characters.sql`, yani çalışma dizininden bağımsız). Dosya `utf-8-sig` olarak (BOM toleransı, CRLF toleransı) okunur; `/* ... */` blokları kaldırılır (satır sayısı korunur), `--` ile başlayan satırlar (baştaki boşluk sonrası) atlanır. Satır, şu **ön ek** desenini sağlıyorsa bir karakter satırıdır: `(` , tırnaklı büyük harf dizisi (profil), tırnaklı ad, tırnaklı hesap, tamsayı ulus, tamsayı ırk, tamsayı sınıf, ardından `,` (alanlar arası boşluk serbest; ör. `^\s*\(\s*'([A-Z]+)'\s*,\s*'([^']*)'\s*,\s*'([^']*)'\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,`). İki sütunlu önkontrol demetleri bu desene uymaz, sayılmaz. Satırın geri kalanı yok sayılır. Hiç satır bulunamayan dosya `ERROR`.

   **Satır doğrulaması** (her ihlal `ERROR <dosya>:<satır>: <mesaj>`): profil ∈ altı profil; ulus ∈ {1, 2}; ad deseni `^Bot(WP|WG|PHD|PHB|MF|MI)([2-9]|[1-9][0-9]+)?_(K|E)$` ve adın profili satır profiliyle, `_K`/`_E` soneki ulusla aynı; hesap tam olarak `BotAcc` + profil + (indeks varsa indeks) + `K|E` ve yalnızca harf/rakam; `len(ad) <= 20` (`MAX_ID_SIZE`); sınıf kodu §5.2 tablosundaki değer; ad ve hesap tüm dosyalar boyunca tekil (yineleme `ERROR`). **İndeks** = addaki sayı, yoksa 1 (`BotWP_K` → 1, `BotWP3_K` → 3; adda `1` yazılamaz, desen reddeder). Irk ve diğer sütunlar denetlenmez.

   Herhangi bir `ERROR` varsa: yalnızca `ERROR` satırları basılır (rapor basılmaz) ve çıkış 2 (`--json` ise `{"errors":[...]}`).

4. **Hesap.** `have[ulus][profil]` = satır sayısı. Her kompozisyon için profil sayıları ve `size`/`priests`. Her küme için: `per_nation` = ihtiyaç toplamı, `total` = 2 × `per_nation`, `usable` = Σ_ulus Σ_profil `min(have, ihtiyaç)`, `missing` = `total − usable` (fazlalık eksiği azaltmaz, negatif olmaz), `status` = `OK` (missing 0) ya da `SHORT`. Çift için: karus için kompozisyon `a`, el_morad için `b`; `missing` = Σ_ulus Σ_profil `max(0, ihtiyaç − have)`; `online` = `size(a) + size(b)`; `status` = `SHORT` (missing > 0), aksi halde `OVER_MAX_BOTS` (`online > max_bots`), aksi halde `OK`. `--max-bots N` tamsayı ≥ 1 (varsayılan 16; `bool` kabul edilmez).

   **Eksik karakter listesi** yalnızca `full20` kümesine göre: her ulus (K, sonra E), her profil (sıralı), `full20` ihtiyacı − `have` kadar yuva. Yuvanın indeksi: kullanılmış indeksler atlanarak 1'den başlayan en küçük boş indeksler (`have` {1,3} ise ilk yuva indeks 2). Yuva adı ADR Ek F8-02 madde 3: indeks ≥ 2 için `Bot<PROFİL><n>_<K|E>`, hesap `BotAcc<PROFİL><n><K|E>`, sınıf kodu §5.2 tablosundan. `needed_by` = yuvanın profil içindeki sırası (`have` + o yuvanın eksik listesindeki 1 tabanlı sırası) `min16` ihtiyacı (C8-A sayısı) içinde kalıyorsa `min16`, değilse `full20`.

5. **Çıktı.** Aşağıdaki sırayla, her kayıt tek satır, alanlar tek boşlukla ayrılır:

   ```
   SQL files=<n> rows=<n>
   HAVE nation=<K|E> WP=<n> WG=<n> PHD=<n> PHB=<n> MF=<n> MI=<n> total=<n>          (K, sonra E)
   COMP id=<id> size=<n> WP=<n> WG=<n> PHD=<n> PHB=<n> MF=<n> MI=<n> priests=<n> rule=<ok|too_many_priests>
   SET id=<small|min16|full20> per_nation=<n> total=<n> usable=<n> missing=<n> status=<OK|SHORT>
   MISSING nation=<K|E> profile=<P> index=<n> char=<ad> account=<hesap> class=<kod> needed_by=<min16|full20>
   PAIR id=<id> karus=<comp> el_morad=<comp> online=<n> max_bots=<n> missing=<n> status=<OK|SHORT|OVER_MAX_BOTS>
   SUMMARY have=<n> small_missing=<n> min16_missing=<n> full20_missing=<n> pairs_ok=<n> pairs_not_ok=<n> errors=0
   ```

   `--json`: tek JSON nesnesi, anahtarlar: `files` (liste), `rows`, `have` (`{"K":{"WP":..,"WG":..,"PHD":..,"PHB":..,"MF":..,"MI":..,"total":..},"E":{...}}`), `compositions` (`[{id,size,counts:{WP..MI},priests,rule}]`), `sets` (`[{id,per_nation,total,usable,missing,status}]`), `missing` (`[{nation,profile,index,char,account,class,needed_by}]`), `pairs` (`[{id,karus,el_morad,online,max_bots,missing,status}]`), `summary` (`{have,small_missing,min16_missing,full20_missing,pairs_ok,pairs_not_ok,errors}`), `errors` (liste). **Çıkış kodu:** `0` tamam; `1` yalnızca `--strict` ve `--target T` (`small`|`min16`|`full20`, varsayılan `full20`) kümesinin `missing > 0` olması; `2` kullanım hatası (bilinmeyen seçenek, geçersiz `--target`/`--max-bots`, `--sql` değeri eksik), okunamayan/bulunamayan dosya ya da herhangi bir `ERROR` (2, 1'e baskındır). `--selftest` diğer seçeneklerle birlikte verilirse yalnızca selftest koşar.

6. **`--selftest` vakaları** (≥ 58; her biri bağımsızdır, bir vaka düşerse diğerleri koşar; çıktı `PASS <ad>` / `FAIL <ad>: <neden>`, son satır `SELFTEST PASS n=<N>` ya da `SELFTEST FAIL failed=<M> of <N>`, çıkış 0/1). Yardımcı `make_sql(rows)` db/002 biçiminde (başlık yorumu, iki sütunlu önkontrol demeti, ardından `@bots` satırları, bir `/* ... */` yorumunda gizlenmiş satır) metin üretir; dosya vakaları `tempfile` ile gerçek dosya yazar. Vaka adları **aynen** kullanılır (K2 bunları `grep` eder):

   | Grup | Vaka adı → beklenen |
   |---|---|
   | Ayrıştırma | `parse_real_db002_rows` (gerçek `db/002` → 12 satır); `parse_real_db002_have` (her ulus × her profil = 1, total 6); `parse_ignores_comments_and_precheck_rows` (önkontrol demetleri, `--` satırı, `/* */` içi satır sayılmaz); `parse_crlf_and_bom` (CRLF + BOM'lu dosya aynı sonuç); `parse_multiple_files_sum` (iki `--sql` dosyası toplanır); `parse_line_numbers` (hata satır numarası dosyadaki gerçek satır, yorum bloğundan sonra da doğru) |
   | Satır doğrulaması | `err_unknown_profile`; `err_bad_nation` (3); `err_name_profile_mismatch` (profil `WP`, ad `BotWG_K`); `err_name_nation_suffix_mismatch` (ulus 1, ad `..._E`); `err_class_mismatch` (WP/K sınıf 112); `err_dup_name`; `err_dup_account`; `err_dup_across_files`; `err_account_not_alnum` (hesapta `_`); `err_account_mismatch` (hesap `BotAccWGK` ama profil `WP`); `err_name_too_long` (21+ karakter); `err_index_one_rejected` (`BotWP1_K`); `err_no_rows` (satırsız dosya); `err_missing_file_rc2` |
   | Kompozisyonlar | `comp_sizes` (8,8,8,8,2,3,4,5); `comp_c8a_counts`; `comp_c8d_one_priest`; `comp_c5_is_c4_plus_phb`; `comp_req_pty_02_all_ok`; `comp_rule_violation_detected` (3 priest'li kompozisyon `too_many_priests`); `doc09_table_matches_builtin` (`docs/09` §2.4 tablosu C8-A..D satırlarını `(\d+ )?(W-P\|W-G\|P-HD\|P-HB\|M-F\|M-I)` jetonlarıyla ayrıştırır, araç tablosuyla birebir; dosya yoksa vaka `FAIL`); `doc09_small_teams_match_builtin` (`docs/09`'daki küçük takımlar satırı, `C2 (`, `C3 (`, `C4 (`, `C5 (` demetleriyle ayrıştırılır; araç kaynağı ASCII kalsın diye satır Türkçe başlığına değil `C2 \(W-P` desenine bağlanır, dosya `utf-8` okunur; `C5 (C4 + P-HB)` gibi başka kompozisyona gönderme genişletilir; dosya yoksa `FAIL`) |
   | Kümeler | `set_small_need` (5/ulus, WP1 WG1 PHD1 PHB1 MF1 MI0); `set_min16_need` (8/ulus, toplam 16); `set_full20_need` (WP3 WG1 PHD1 PHB1 MF3 MI1, 10/ulus, toplam 20); `have12_small_ok` (missing 0); `have12_min16_missing_4`; `have12_full20_missing_8`; `have16_min16_ok_full20_missing_4` (WP ve MF ulus başına 2: `docs/15` "20 = 16 + 4"); `have20_all_ok`; `have_surplus_not_negative` (fazladan WG: usable ihtiyacı aşmaz, missing değişmez) |
   | Eksik listesi | `missing_real_db002_8_lines` (gerçek `db/002` için tam 8 `MISSING` satırı, §5.8 bloğuyla aynı); `missing_index_skips_used` (WP indeksleri {1,3} → tek eksik yuva indeks 2); `missing_needed_by` (WP indeks 2 `min16`, indeks 3 `full20`); `missing_class_codes` (K WP 106, E MF 210); `missing_none_when_20` |
   | Çiftler | `pair_small_ok` (EVAL-2v2 `online=4`, EVAL-5v5 `online=10`, 12 karakterle `OK`); `pair_8v8_a_short_4` (`online=16`, missing 4, `SHORT`); `pair_mix_both_assignments` (iki satır, ikisi de missing 4); `pair_same_comp_single_line`; `pair_over_max_bots` (20 karakter + `--max-bots 12` → `EVAL-8v8-A` `OVER_MAX_BOTS`; missing > 0 ise `SHORT` öncelikli) |
   | CLI | `cli_real_expected_output` (varsayılan `db/002` ile çıktı §5.8 bloğuyla satır satır aynı, çıkış 0); `cli_json_keys` (`json.loads`; `summary.full20_missing == 8`, `len(missing) == 8`, `have.K.WP == 1`); `cli_strict_rc1_default_full20`; `cli_strict_target_small_rc0`; `cli_strict_rc0_with_sample20`; `cli_bad_target_rc2`; `cli_max_bots_zero_rc2`; `cli_unknown_option_rc2`; `cli_default_path_independent_of_cwd` (geçici dizine `chdir` edip varsayılan yolla koş, sonuç aynı); `cli_errors_only_output` (hatalı satırda rapor satırı yok, yalnızca `ERROR`, rc 2) |
   | Örnek dosya | `sample_file_expected` (`tools/bot-composition-check/sample-20.sql` §5.9'daki çıktıyı verir; dosya yoksa `FAIL`) |

7. **`tools/bot-composition-check/sample-20.sql`** (aşağıdaki 22 satır **aynen**, sırasıyla; LF; `sample_file_expected` vakası dosyayı `os.path.dirname(os.path.abspath(__file__))` altından açar). İlk iki satır yorumdur (araç bunları atlar), ardından 20 satır:

   ```
   -- Fixture for tools/bot-composition-check.py: NOT executable SQL.
   -- 20 character rows in the db/002 @bots row format (first six columns only).
   ('WP', 'BotWP_K', 'BotAccWPK', 1, 1, 106, 0),
   ('WG', 'BotWG_K', 'BotAccWGK', 1, 1, 106, 0),
   ('PHD', 'BotPHD_K', 'BotAccPHDK', 1, 4, 112, 0),
   ('PHB', 'BotPHB_K', 'BotAccPHBK', 1, 4, 112, 0),
   ('MF', 'BotMF_K', 'BotAccMFK', 1, 3, 110, 0),
   ('MI', 'BotMI_K', 'BotAccMIK', 1, 3, 110, 0),
   ('WP', 'BotWP2_K', 'BotAccWP2K', 1, 1, 106, 0),
   ('WP', 'BotWP3_K', 'BotAccWP3K', 1, 1, 106, 0),
   ('MF', 'BotMF2_K', 'BotAccMF2K', 1, 3, 110, 0),
   ('MF', 'BotMF3_K', 'BotAccMF3K', 1, 3, 110, 0),
   ('WP', 'BotWP_E', 'BotAccWPE', 2, 11, 206, 0),
   ('WG', 'BotWG_E', 'BotAccWGE', 2, 11, 206, 0),
   ('PHD', 'BotPHD_E', 'BotAccPHDE', 2, 12, 212, 0),
   ('PHB', 'BotPHB_E', 'BotAccPHBE', 2, 12, 212, 0),
   ('MF', 'BotMF_E', 'BotAccMFE', 2, 12, 210, 0),
   ('MI', 'BotMI_E', 'BotAccMIE', 2, 12, 210, 0),
   ('WP', 'BotWP2_E', 'BotAccWP2E', 2, 11, 206, 0),
   ('WP', 'BotWP3_E', 'BotAccWP3E', 2, 11, 206, 0),
   ('MF', 'BotMF2_E', 'BotAccMF2E', 2, 12, 210, 0),
   ('MF', 'BotMF3_E', 'BotAccMF3E', 2, 12, 210, 0);
   ```

8. **Beklenen çıktı: gerçek `db/002` ile** (`python3 tools/bot-composition-check.py`; `diff` boş olmalı):

   ```
   SQL files=1 rows=12
   HAVE nation=K WP=1 WG=1 PHD=1 PHB=1 MF=1 MI=1 total=6
   HAVE nation=E WP=1 WG=1 PHD=1 PHB=1 MF=1 MI=1 total=6
   COMP id=C8-A size=8 WP=2 WG=1 PHD=1 PHB=1 MF=2 MI=1 priests=2 rule=ok
   COMP id=C8-B size=8 WP=3 WG=1 PHD=1 PHB=1 MF=1 MI=1 priests=2 rule=ok
   COMP id=C8-C size=8 WP=1 WG=1 PHD=1 PHB=1 MF=3 MI=1 priests=2 rule=ok
   COMP id=C8-D size=8 WP=3 WG=1 PHD=1 PHB=0 MF=2 MI=1 priests=1 rule=ok
   COMP id=C2 size=2 WP=1 WG=0 PHD=1 PHB=0 MF=0 MI=0 priests=1 rule=ok
   COMP id=C3 size=3 WP=1 WG=0 PHD=1 PHB=0 MF=1 MI=0 priests=1 rule=ok
   COMP id=C4 size=4 WP=1 WG=1 PHD=1 PHB=0 MF=1 MI=0 priests=1 rule=ok
   COMP id=C5 size=5 WP=1 WG=1 PHD=1 PHB=1 MF=1 MI=0 priests=2 rule=ok
   SET id=small per_nation=5 total=10 usable=10 missing=0 status=OK
   SET id=min16 per_nation=8 total=16 usable=12 missing=4 status=SHORT
   SET id=full20 per_nation=10 total=20 usable=12 missing=8 status=SHORT
   MISSING nation=K profile=WP index=2 char=BotWP2_K account=BotAccWP2K class=106 needed_by=min16
   MISSING nation=K profile=WP index=3 char=BotWP3_K account=BotAccWP3K class=106 needed_by=full20
   MISSING nation=K profile=MF index=2 char=BotMF2_K account=BotAccMF2K class=110 needed_by=min16
   MISSING nation=K profile=MF index=3 char=BotMF3_K account=BotAccMF3K class=110 needed_by=full20
   MISSING nation=E profile=WP index=2 char=BotWP2_E account=BotAccWP2E class=206 needed_by=min16
   MISSING nation=E profile=WP index=3 char=BotWP3_E account=BotAccWP3E class=206 needed_by=full20
   MISSING nation=E profile=MF index=2 char=BotMF2_E account=BotAccMF2E class=210 needed_by=min16
   MISSING nation=E profile=MF index=3 char=BotMF3_E account=BotAccMF3E class=210 needed_by=full20
   PAIR id=EVAL-2v2 karus=C2 el_morad=C2 online=4 max_bots=16 missing=0 status=OK
   PAIR id=EVAL-3v3 karus=C3 el_morad=C3 online=6 max_bots=16 missing=0 status=OK
   PAIR id=EVAL-4v4 karus=C4 el_morad=C4 online=8 max_bots=16 missing=0 status=OK
   PAIR id=EVAL-5v5 karus=C5 el_morad=C5 online=10 max_bots=16 missing=0 status=OK
   PAIR id=EVAL-8v8-A karus=C8-A el_morad=C8-A online=16 max_bots=16 missing=4 status=SHORT
   PAIR id=EVAL-8v8-MIX karus=C8-B el_morad=C8-C online=16 max_bots=16 missing=4 status=SHORT
   PAIR id=EVAL-8v8-MIX karus=C8-C el_morad=C8-B online=16 max_bots=16 missing=4 status=SHORT
   SUMMARY have=12 small_missing=0 min16_missing=4 full20_missing=8 pairs_ok=4 pairs_not_ok=3 errors=0
   ```

9. **Beklenen çıktı: `sample-20.sql` ile** (`python3 tools/bot-composition-check.py --sql tools/bot-composition-check/sample-20.sql`): `SQL files=1 rows=20`; `HAVE nation=K WP=3 WG=1 PHD=1 PHB=1 MF=3 MI=1 total=10` ve aynı `nation=E`; `COMP` satırları §5.8 ile **aynı**; `SET id=small per_nation=5 total=10 usable=10 missing=0 status=OK`, `SET id=min16 per_nation=8 total=16 usable=16 missing=0 status=OK`, `SET id=full20 per_nation=10 total=20 usable=20 missing=0 status=OK`; **hiç `MISSING` satırı yok**; yedi `PAIR` satırı §5.8'deki gibi ama hepsi `missing=0 status=OK`; son satır `SUMMARY have=20 small_missing=0 min16_missing=0 full20_missing=0 pairs_ok=7 pairs_not_ok=0 errors=0`.

10. Tüm vakalar geçince `python3 tools/bot-composition-check.py --selftest | tail -1`, §5.8 çıktısı ve §5.9 çıktısını Uygulayıcı Raporu'na yapıştır. `tools/build.sh Release` ve `tools/run-tests.sh` koş (değişmediklerini göstermek için). Commit: `[F8-02] ...`; planın `Durum`'u `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/bot-composition-check.py --selftest` çıkış 0; son satır `SELFTEST PASS n=<N>` ve `N >= 58`; çıktıda `FAIL ` ile başlayan satır yok.
- [ ] K2: §5.6 tablosundaki **tüm** vaka adları `--selftest` çıktısında `PASS <ad>` olarak bir kez geçer (§7'deki "K2 komutu"; her satırın sayısı `1`, en az 58 satır; yalnızca §5.6 vaka adları bu önekleri taşır).
- [ ] K3: `python3 tools/bot-composition-check.py` çıktısı §5.8'deki 30 satırla **birebir** aynı (`diff` boş), çıkış 0; `--sql tools/bot-composition-check/sample-20.sql` çıktısı §5.9 ile aynı (`MISSING` satırı yok: `grep -c '^MISSING'` → 0), çıkış 0.
- [ ] K4: `--json` çıktısı `python3 -c "import json,sys; d=json.load(sys.stdin); assert d['summary']['full20_missing']==8 and len(d['missing'])==8 and d['have']['K']['WP']==1 and d['pairs'][4]['id']=='EVAL-8v8-A' and d['errors']==[]"` ile geçer; sample-20 ile `full20_missing==0 and d['missing']==[]`.
- [ ] K5: çıkış kodları: varsayılan → 0; `--strict` → 1 (full20 eksik 8); `--strict --target small` → 0; `--strict --target min16` → 1; `--strict --sql tools/bot-composition-check/sample-20.sql` → 0; `--target bogus` → 2; `--max-bots 0` → 2; `--sql /yok.sql` → 2; `--nonsense` → 2. Hatalı bir geçici SQL (profil `XX`) → yalnızca `ERROR <dosya>:<satır>: ...` satırı, `SQL`/`HAVE`/`SUMMARY` satırı yok, çıkış 2.
- [ ] K6 (mutasyon): aracın bir kopyasında (`/tmp` altında) (a) araç içi C8-A tablosunda WP sayısı 2 → 1 yapıldığında kopyanın `--selftest`'i çıkış 1 verir ve **en az 3** vaka `FAIL` olur (`comp_c8a_counts`, `doc09_table_matches_builtin`, `set_min16_need` ya da benzeri); (b) `missing` hesabındaki `max(0, ihtiyaç - have)` (ya da `min(have, ihtiyaç)`) ifadesi sınırlandırmasız `ihtiyaç - have` yapıldığında `have_surplus_not_negative` düşer; (c) eksik listesinde kullanılmış indeksleri atlama kaldırıldığında `missing_index_skips_used` düşer. Hangi satırı değiştirdiğini ve çıktıyı rapora yaz.
- [ ] K7: `tools/bot-composition-check.py` yalnızca standart kütüphaneyi içe aktarır (`grep -nE "^(import|from) " tools/bot-composition-check.py` çıktısında her modül stdlib'dir; **`socket`, `urllib`, `http`, `requests`, `subprocess`, `sqlite3`, `pyodbc` yok**); CLI modunda dosya yazmaz (`grep -n "open(" ` çıktısındaki her yazma `--selftest` yolundadır); ASCII (`LC_ALL=C grep -nP '[^\x00-\x7F]' tools/bot-composition-check.py tools/bot-composition-check/sample-20.sql` boş); CRLF yok (`grep -c $'\r'` her iki dosyada 0).
- [ ] K8: `git diff --stat gece/2026-10-02-nav...bot/F8-02` yalnızca `tools/bot-composition-check.py`, `tools/bot-composition-check/sample-20.sql` ve bu plan dosyasını gösterir; `GameServer/`, `BotCore/`, `Tests/`, `shared/`, `AIServer/`, `db/`, `docs/` değişmemiştir.
- [ ] K9: `./tools/build.sh Release` hatasız biter (yeni uyarı yok) ve `./tools/run-tests.sh` `223 tests, 0 failed` (değişmedi) yazar.
- [ ] K10: Doc eşlemesi: Uygulayıcı Raporu'nda `docs/15` §6a sayılarının (12 bugünkü, 16 asgari, 20 çeşitlilik, "16'ya +4", ulus başına 8/10, +1 W-P/+1 M-F, "mevcut 12 ile ≤ C5 çalışır") hangi vaka/çıktı satırıyla doğrulandığı bir tabloyla eşlenmiştir.

## 7. Doğrulama komutları

```bash
python3 tools/bot-composition-check.py --selftest | tail -3
# K2 komutu: her vaka adi icin PASS satiri sayisi (hepsi 1 olmali)
python3 tools/bot-composition-check.py --selftest > /tmp/st.txt
for n in $(grep -oE '`[a-z0-9_]+`' plans/F8-02-bot-kompozisyon-denetleyici.md | tr -d '`' | grep -E '^(parse|err|comp|doc09|set|have|missing|pair|cli|sample)_' | sort -u); do
  printf '%s %s\n' "$n" "$(grep -c "^PASS $n\$" /tmp/st.txt)"
done
python3 tools/bot-composition-check.py
python3 tools/bot-composition-check.py --sql tools/bot-composition-check/sample-20.sql | tail -4
python3 tools/bot-composition-check.py --json | python3 -m json.tool | head -20
python3 tools/bot-composition-check.py --strict; echo rc=$?
python3 tools/bot-composition-check.py --strict --target small; echo rc=$?
./tools/build.sh Release
./tools/run-tests.sh | tail -2
git diff --stat gece/2026-10-02-nav...bot/F8-02
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları; `*.py text eol=lf`; dosyalar yalnızca ASCII (Türkçe karakter yok, yorumlar İngilizce).
- **Kurallar uydurulmaz:** ADR Ek F8-02'de `[A]` işaretli olanlar (ulus başına en büyük küme, ek karakter adlandırması, `needed_by`, `OVER_MAX_BOTS`) dışında yeni kural eklenmez. Belirsiz bir durumda **durup** soru yaz.
- **Doc ile sürüklenme:** kompozisyon tablosu `docs/09`'dan kopyadır. `docs/09` değişirse `doc09_*` vakaları düşer; bu **istenen** davranıştır, vakaları gevşetme. `docs/09` bu planda değiştirilmez.
- `db/002` dosyasının biçimi (satır 124–137) sabit kabul edilir; dosyayı değiştirme. Satır deseni yalnızca ilk altı sütuna bağlıdır, böylece `db/003` aynı biçimde yazılırsa araç aynen çalışır.
- Kişisel veri: bu araç hiçbir DB tablosunu okumaz; yalnızca depodaki SQL metnini okur.
- Python 3.8 uyumu: `match` deyimi, `X | Y` tür birleşimi, `list[int]` genel tip notasyonu kullanma. `bool` değeri tamsayı olarak kabul edilmez (`--max-bots` zaten komut satırından geldiği için dizgedir; `isinstance(v, bool)` yalnızca JSON/iç API'de gerekirse).
- Bu plan sunucu davranışını değiştirmez; bot sistemi kapalıyken (varsayılan) hiçbir şey etkilenmez. `tools/build.sh` ve `run-tests.sh` bu aracı çalıştırmaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F8-02` — `6b2ea79 [F8-02] Kompozisyon denetleyicisi: tools/bot-composition-check.py + sample-20.sql (58 selftest)`
- Değişen dosyalar ve neden:
  - `tools/bot-composition-check.py` (yeni): SQL `@bots` satırlarını metin olarak ayrıştırır, ADR-0002 ad/sınıf kurallarını doğrular, kompozisyon/küme/çift ihtiyaçlarını ve eksik karakter listesini hesaplar; `--selftest` (58 vaka), `--json`, `--strict`, `--target`, `--max-bots`, `--sql`.
  - `tools/bot-composition-check/sample-20.sql` (yeni): §5.7'deki 22 satır aynen; 20 karakterlik hedef durum fixtürü (yürütülebilir SQL değil).
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
  `./tools/run-tests.sh` → `223 tests, 0 failed` (değişmedi). Sunucular kapalıydı (`run-servers.sh status` 3/3 `[DOWN]`), paralel hat olduğu için dokunulmadı.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ `SELFTEST PASS n=58` (58 ≥ 58), `FAIL` satırı yok, rc 0.
  - K2 ✔ §5.6'daki 58 vaka adının tamamı `PASS` olarak tam bir kez; `grep -c '^PASS'` = 58.
  - K3 ✔ Gerçek `db/002` çıktısı §5.8'in 30 satırıyla birebir (`diff` boş, yalnızca plan bloğundaki 3 boşluk girinti farkı), rc 0; `sample-20.sql` çıktısı §5.9 ile aynı, `grep -c '^MISSING'` = 0.
  - K4 ✔ `--json` (gerçek ve sample) `python3 -c` doğrulaması geçti.
  - K5 ✔ rc: varsayılan 0, `--strict` 1, `--strict --target small` 0, `--strict --target min16` 1, `--strict --sql sample-20` 0, `--target bogus` 2, `--max-bots 0` 2, `--sql /yok.sql` 2, `--nonsense` 2; hatalı SQL → yalnızca `ERROR` satırı, rapor yok, rc 2.
  - K6 ✔ Üç mutasyon `/tmp` kopyasında: (a) C8-A WP 2→1 → rc 1 ve 24 `FAIL` (`comp_c8a_counts`, `set_min16_need`, …); (b) `min(...)` sınırı kaldırılınca `have_surplus_not_negative` (`usable=16 missing=-6`); (c) kullanılmış indeks atlama kaldırılınca `missing_index_skips_used` (`index 1`). Her üçünde rc 1.
  - K7 ✔ Yalnızca stdlib (`json os re sys tempfile`); `socket/urllib/http/requests/subprocess/sqlite3/pyodbc` yok; `open()` ile yazan tek yol `--selftest` içindeki `write_tmp`; iki dosya ASCII, `\r` 0.
  - K8 ✔ `git diff --stat gece/2026-10-02-nav...bot/F8-02` yalnızca iki yeni `tools/` dosyası + plan dosyası (commit sonrası).
  - K9 ✔ `./tools/build.sh Release` hatasız; `./tools/run-tests.sh` `223 tests, 0 failed`.
  - K10 ✔ Aşağıdaki eşleme tablosu.
- Plandan sapmalar ve gerekçeleri:
  - `err_unknown_profile` ve `cli_errors_only_output` vakalarının fixtür satırı, `row()` yardımcısının bilinmeyen profilde sınıf kodu bulamaması nedeniyle elle `("XX", ...)` demetiyle yazıldı; davranış ve beklenen çıktı değişmedi.
  - `have_surplus_not_negative` testinde kullanılmayan bir ara liste ifadesi temizlendi (üretilen satır kümesi aynı).
- Açık sorular: yok.

#### K10 — `docs/15` §6a eşlemesi

| `docs/15` §6a ifadesi | Doğrulayan vaka / çıktı |
|---|---|
| "Bugün DB'de 12 karakter" | `parse_real_db002_rows` (`SQL files=1 rows=12`), `SUMMARY have=12` |
| "ulus başına 6 (W-P, W-G, P-HD, P-HB, M-F, M-I)" | `parse_real_db002_have` (`HAVE ... total=6`) |
| "EVAL-8v8-A için asgari 8 (+1 W-P, +1 M-F) = 16" | `set_min16_need` (`min16` = C8-A), `have12_min16_missing_4` (`SET id=min16 ... total=16 missing=4`), `missing_needed_by` (WP/MF ikinci yuva `needed_by=min16`) |
| "Kompozisyon çeşitliliği 10 (+1 W-P, +1 M-F daha) = 20" | `set_full20_need` (WP3 MF3, 10/ulus), `have12_full20_missing_8`, `have20_all_ok` |
| "20 karakter 16'nın üstüne 4 ek karakterdir (ulus başına 3. W-P ve 3. M-F)" | `have16_min16_ok_full20_missing_4` (min16 0, full20 4), eksik listede tam 8 yuva (WP2/WP3/MF2/MF3 × 2 ulus) |
| "ulus başına 8 / 10" | `SET id=min16 per_nation=8`, `SET id=full20 per_nation=10` |
| "F7 küçük takım testleri (≤ C5) mevcut 12 karakterle çalışır" | `have12_small_ok` (`SET id=small ... missing=0`), `pair_small_ok` (2v2/5v5 `OK`) |
| "3. W-P ve 3. M-F aynı sabit karakter kümesinden seçilebilsin" | `missing_class_codes`, `missing_index_skips_used`, `MISSING` satırlarının ad/sınıf kodları |

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02-nav...bot/F8-02` @ `<sha>`
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
