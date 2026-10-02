# F1-10: Hasar logu özet ve model karşılaştırma betiği (`tools/damage-trace-summary.py`)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F1 — Veri ve mekanik doğrulama (`docs/17` §2) |
| Branch | `bot/F1-10` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F1-09 (log biçimi, `DOĞRULANDI`, `gece/2026-10-02`'ye birleşti), F1-06/F1-07 (model çıktıları, `KAPANDI`) |
| İlgili gereksinim / kabul | T-MECH-DMG-01..03 (`docs/15` §4.1: "Model ± %15 içinde veya model güncellendi"), Q-08 (`docs/18` §3), MB-04 (`docs/05`) |
| Tahmini büyüklük | S (1 yeni araç betiği; sunucu ve DB'ye dokunulmaz) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

F1-09'un `FDP_DAMAGE_TRACE` loglarını (`Logs/DamageTrace_*.log`) okuyup (a) saldıran/hedef/bağlam başına özet üreten ve (b) bu ölçümleri `tools/stat-model.py` ve `tools/spell-model.py` çıktılarıyla **± %15** karşılaştıran, Python standart kütüphanesiyle yazılmış bir araç yazmak. Proje sahibi insan oturumunu (T-MECH-DMG) yaptıktan sonra tek komutla "model tuttu / tutmadı" tablosu çıkar. Araç sunucuyu, DB'yi ve modelleri **çalıştırmaz**; model çıktısını dosyadan okur.

## 2. Bağlam (okunması zorunlu)

- `plans/F1-09-sunucu-hasar-kaydi.md` §5.4 (satır biçimi) ve Doğrulama Raporu bulgu 1 — **20 sütunlu, sekme ayraçlı, değişmez** biçim; `ctx=-`/`primary=0` satırları DoT tikleri ve yansıtılan hasardır.
- Satır biçimi (F1-09, `GameServer/DamageTrace.cpp:123-127` içinde doğrulandı; sütun sırası bağlayıcı):
  ```
  wall_ms  t_ms  ctx  primary  a_sid  a_name  a_class  a_nation  a_level  a_hit  t_sid  t_name  t_class  t_nation  t_level  t_ac  requested  applied  hp_before  hp_max
  ```
  `ctx`: `R`, `S<magicNum>` (ör. `S109510`) veya `-`. `requested` = `CUser::HpChange`'e **gelen** istenen değişim (`GameServer/User.cpp:1871` `originalAmount`; `MAX_DAMAGE` sınırı, mirror, mana emilimi ve mastery indirimlerinden **önce**; hasar negatif, heal pozitif). `applied` = `hp_after − hp_before` (indirimler ve ölümcül vuruşta HP tabanı sonrası). Modellerin tahmini `requested`'a karşılık gelir (model `GetDamage`/`GetMagicDamage` çıktısını verir, `User.cpp:1866-1950` içindeki indirimleri içermez).
- `tools/stat-model.py` çıktı biçimi (`write_report`, `:467-533`): satır başı `R`, `K`, `S`, `P`; örnekler:
  ```
  R BotWP_K->BotMI_K hit_prob=0.920 base=225 dmg_avg=224.5 dmg_min=.. dmg_max=.. exp=..
  K BotWP_K->BotMI_K skill=<num> <ad, boşluk içerebilir> hit_pct=0.9200 sHit=.. base=.. dmg_avg=..
  ```
  Yalnızca **Karus** botları (`..._K`) saldırgan ve hedef (`:486` `defenders` = `nation == 1`).
- `tools/spell-model.py` çıktı biçimi (`write_report`, `:479-594`):
  ```
  M BotMF_K->BotWP_K skill=<num> <ad> attr=.. first=.. time=.. dur=.. msp=.. cast_s=.. recast_s=.. dmg_avg=.. dmg_min=.. dmg_max=.. dot_total=.. dot_tick=.. ticks=.. def_hp=.. casts_to_kill=..
  H BotPHD_K skill=<num> <ad> first=.. time=.. dur=.. radius=.. msp=.. cast_s=.. recast_s=.. heal_instant=.. hot_tick=.. ticks=.. hot_total=.. heal_per_msp=..
  ```
  `M` hedef profiline bağlıdır; `H` hedeften bağımsızdır.
- Karakter adları (`db/002_bot_characters.sql:32-37`): `Bot<PROFİL>_<K|E>` — profiller `WP, WG, PHD, PHB, MF, MI`. El Morad botları (`_E`) model çıktısında **yoktur**; eşleme geri düşüşü adım 4'tedir.
- Mevcut betik kalıbı: `tools/packet-trace-summary.py` (`parse_line`, `--selftest`, `USAGE`). Aynı iskeleti ve stili kullan (ASCII, LF, yalnızca standart kütüphane, `main(argv=None)`).
- Çalışma ortamı log dizini: `/mnt/c/dev/fdp/server/Logs/` (`tools/trace-session.sh:16` `LOG_DIR`). Betik yolu argüman olarak alır; sabit yol gömme.

## 3. Kapsam

**Yapılacaklar**

- `tools/damage-trace-summary.py` (yeni): bölüm `L` (okuma özeti), `A` (gruplu özet), `D` (bağlam dışı satırlar), `C` (model karşılaştırması); `--selftest`.

**Kapsam dışı (yapılmayacak)**

- Isabet oranı: ıskalanan vuruşlar loglanmaz (F1-09 kapsam dışı); betik isabet oranı **hesaplamaz** ve `hit_prob`'u karşılaştırmaz. Bunu çıktıdaki not satırı da belirtir.
- DoT tiklerini skill'e atfetmek (`ctx=-` satırlarında skill kimliği yok): yalnızca ayrı bölümde özetlenir, `OK`/`FAIL` hükmü verilmez.
- Model betiklerini çalıştırmak, DB'ye bağlanmak, `GameServer/` veya başka dosyada değişiklik, `docs/**` güncelleme (Claude yapar), grafik/CSV üretimi, log dosyası yazma.
- Ölçüm toplamak (insan oturumu): bu planın işi değil.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/damage-trace-summary.py` | yeni | ASCII, LF, yalnızca standart kütüphane |

`tools/*` düzenlemesi opencode'da `ask` ister; onay verilmezse durup Uygulayıcı Raporu'na yaz. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. **Doğrula.** §2'deki dosya:satır referanslarını depoda aç (`GameServer/DamageTrace.cpp:123-127`, `User.cpp:1871`, `tools/stat-model.py:467-533`, `tools/spell-model.py:479-594`); kayma varsa raporla. Model betiklerini **çalıştırmadan** yalnızca `write_report` fonksiyonlarındaki biçim dizgelerini kaynak al.

2. **Komut satırı:**
   ```
   python3 tools/damage-trace-summary.py LOG [LOG ...] [--stat-model FILE] [--spell-model FILE]
                                         [--attacker NAME] [--target NAME] [--tol PCT] [--min-n N]
   python3 tools/damage-trace-summary.py --selftest
   ```
   `--tol` varsayılan `15`, `--min-n` varsayılan `5`. `--stat-model`/`--spell-model`: önceden dosyaya yönlendirilmiş model çıktıları (`python3 tools/stat-model.py > /tmp/stat.txt`). Biri verilmezse `C` bölümünde yalnızca ilgili satırlar atlanır. `LOG` yoksa `USAGE` yaz, çıkış kodu 2. Log dosyalarını `open(path, "r", encoding="latin-1", newline="")` ile oku (isimler ASCII dışı olabilir; `\r\n` satır sonu `rstrip("\r\n")` ile temizlenir).

3. **Ayrıştırma (`parse_line`).** `line.rstrip("\r\n").split("\t")`; tam **20** sütun değilse veya tamsayı alanları (`wall_ms, t_ms, primary, a_sid, a_class, a_nation, a_level, a_hit, t_sid, t_class, t_nation, t_level, t_ac, requested, applied, hp_before, hp_max`) dönüştürülemezse `None` (bozuk satır sayılır, atlanır, çökme yok). `ctx` değeri `R`, `-` veya `S` + rakamlar dışındaysa da bozuk say. Dönen sözlük: yukarıdaki sütun adları + `skill` (`S` ctx için `int`, aksi halde `None`).

4. **Bölümler** (çıktı `== L ==`, `== A ==`, `== D ==`, `== C ==` başlıklarıyla, sırayla; yalnızca ASCII; deterministik: gruplar anahtara göre sıralı).
   - **`L`:** tek satır `L files=<n> lines=<n> parsed=<n> bad=<n> span_s=<ilk-son wall_ms farkı / 1000, 1 ondalık>`; filtre (`--attacker`/`--target`) **ayrıştırmadan sonra** uygulanır ve `L` satırında `filtered=<n>` olarak görünür. Ardından sabit not satırı: `L note=misses_not_logged hit_rate_not_measured`.
   - **`A` (primary=1 satırlar, `ctx` ve yön başına):** anahtar `(ctx, a_name, t_name, kind)`; `kind` = `dmg` (`requested < 0`) veya `heal` (`requested > 0`); `requested == 0` satırları sayılır ama gruplanmaz (`L` satırına `zero=<n>` ekle). Her grup için bir satır:
     ```
     A ctx=<R|S..> kind=<dmg|heal> a=<ad> t=<ad> n=<n> req_avg=<f.1> req_min=<d> req_max=<d> app_avg=<f.1> app_ratio=<f.3> a_hit=<min>-<max> t_ac=<min>-<max> lethal=<n>
     ```
     `req_avg/req_min/req_max` **büyüklük** (`abs(requested)`) üzerinden; `app_ratio` = `sum(abs(applied)) / sum(abs(requested))` (mastery/mirror/mana emilimi/ölümcül vuruş sınırı etkisi; hasarda <1 beklenir, mastery'li hedefte ~0,85); `lethal` = `hp_before + applied == 0` olan `dmg` olay sayısı (HP'si sıfırlanan; `applied`'ı sınırlanmıştır). `a_hit`/`t_ac` aralık (min-max): AC debuff'ı (Q-08/MB-04) grup içinde `t_ac` değişimiyle görünür.
   - **`D` (primary=0 satırlar, bağlam dışı):** anahtar `(a_name, t_name, kind)`; satır: `D a=<ad> t=<ad> kind=<dmg|heal> n=<n> req_sum=<d> req_avg=<f.1> ctx_seen=<S..,-,R virgüllü>`. Ayrıca, saldıranın bu log içinde `primary=1` ile kullandığı skill'lerin (`S<num>`) model `M` satırlarındaki `dot_tick` değerleri (`--spell-model` verilmişse) `model_dot_tick=<num:değer,...>` olarak eklenir; hüküm **verilmez** (bilgi amaçlı).
   - **`C` (model karşılaştırması)**, yalnızca `A` satırlarındaki `primary=1` gruplar için. Her model satırı biçimi **yalnızca `anahtar=değer` jetonlarını** okuyarak ayrıştırılır (skill adı boşluk içerebilir; `=` içermeyen jetonlar yok sayılır). `meas` = grup `req_avg`, `model` = model değeri, `diff_pct` = `(meas − model) / model × 100`, hüküm: `n < --min-n` → `LOW_N`; model değeri 0 veya yok → `NO_MODEL`; `abs(diff_pct) <= --tol` → `OK`; aksi halde `FAIL`. Satır biçimi: `C kind=<R|K|M|H> a=<ad> t=<ad|-> skill=<num|-> n=<n> meas=<f.1> model=<f.1> diff_pct=<+f.1> verdict=<..> match=<exact|profile> range_viol=<n|->`.
     - `ctx=R`, `kind=dmg` ↔ `stat-model` `R` satırı `dmg_avg`.
     - `ctx=S<N>`, `kind=dmg` ↔ önce `stat-model` `K` satırı (`skill=N`, `dmg_avg`); yoksa `spell-model` `M` satırı (`skill=N`, `dmg_avg` = **anlık** hasar; `dmg_avg == 0` ise `NO_MODEL`).
     - `ctx=S<N>`, `kind=heal` ↔ `spell-model` `H` satırı (`skill=N`, `heal_instant`; hedeften bağımsız, `t=-`... çıktıda gerçek `t` adı yine yazılır).
     - `range_viol`: modelde `dmg_min`/`dmg_max` varsa (`R`, `M`) gruptaki olaylardan `abs(requested)` bu aralığın **dışında** kalanların sayısı (`K`/`H` için `-`). Bunun için grup bazında olay değerlerini bellekte tut.
     - **Eşleme:** önce tam ad (`BotWP_K->BotMI_K`); bulunamazsa ad sonundaki `_K`/`_E`'yi atıp **profil çifti** ile (`WP`->`MI`) Karus satırını kullan ve `match=profile` yaz; `H` için saldıran adı aynı kuralla. İkisi de yoksa `NO_MODEL`, `match=-`. Profil çıkarımı: `re.match(r"^Bot([A-Z]+)_([KE])$", ad)`; eşleşmeyen ad (gerçek oyuncu) için `match=-`, `NO_MODEL`.
   - Her bölüm boşsa yalnızca başlık satırı basılır; çıkış kodu 0 (okunabilir tek bir parse edilmiş satır yoksa yine 0, `L` satırı `parsed=0` gösterir).

5. **`--selftest`.** Bellekte sentetik log satırları ve sentetik model satırları kur (sürüm sabiti, dosya yazma yok; `io.StringIO` kullanılabilir). En az şunları `assert` et ve `selftest OK` yaz:
   - 20 sütunlu geçerli satır ayrıştırılır; 19 sütunlu, tamsayı olmayan alanlı ve geçersiz `ctx`'li satırlar `None` döner.
   - R grubu: 6 olay (`requested` −200, −210, −190, −205, −195, −200) → `req_avg == 200.0`, `n == 6`; model `dmg_avg=205.0` → `diff_pct ≈ -2.4`, `OK`; model `dmg_avg=150.0` → `FAIL`.
   - `n < min_n` → `LOW_N`.
   - Heal: `requested` +300 (3 olay) ↔ `H ... heal_instant=310` → `OK` (`min_n=3` ile).
   - `ctx=-` ve `primary=0` satır `D`'ye düşer, `A`/`C`'ye girmez.
   - `_E` adlı saldıran/hedef için profil geri düşüşü `match=profile` verir; tam ad varsa `match=exact`.
   - `K` satırında boşluklu skill adı (`skill=109510 Hammer Drop hit_pct=...`) doğru ayrıştırılır (`dmg_avg` okunur).
   - `lethal` sayımı: `hp_before=100, applied=-100, requested=-250` olayı `lethal=1`; `app_ratio` hesaplanır.

6. **Elle deneme** (log yoksa sentetik): geçici bir dosya oluştur (`/tmp` altında, işin sonunda sil), 3–4 sentetik satırla betiği çalıştır, çıktısını rapora yapıştır. Gerçek oyuncu adı kullanma; yalnızca `BotWP_K` vb.

7. Raporu bu plan dosyasının "Uygulayıcı Raporu"na yaz; her kriterin çıktısını yapıştır.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/damage-trace-summary.py --selftest` → `selftest OK`, çıkış kodu 0 (çıktıyı yapıştır).
- [ ] K2: Çıktı bölümleri: sentetik 3–4 satırlık log ile çalıştırınca `== L ==`, `== A ==`, `== D ==`, `== C ==` başlıkları bu sırayla basılır; `L` satırında `files= lines= parsed= bad= span_s=` ve `L note=misses_not_logged hit_rate_not_measured` vardır (`grep -c "^== "` = 4; çıktıyı yapıştır).
- [ ] K3: `C` satır biçimi §5 adım 4'teki alanlarla birebir (`C kind= a= t= skill= n= meas= model= diff_pct= verdict= match= range_viol=`); `--tol` ve `--min-n` varsayılanları 15 ve 5 (`python3 tools/damage-trace-summary.py 2>&1 | head` kullanım satırıyla, `grep -n "tol\|min-n\|default" tools/damage-trace-summary.py` çıktısıyla).
- [ ] K4: Dayanıklılık: boş dosya, yalnızca bozuk satırlı dosya ve olmayan dosya yolu ile çalıştırıldığında Python traceback **yok**; boş/bozuk için çıkış 0 (`parsed=0`), olmayan yol için anlamlı hata mesajı ve çıkış kodu 2 (üç çalıştırmanın çıktısını ve `echo $?` değerlerini yapıştır).
- [ ] K5: Model biçimiyle uyum: betiğin ayrıştırdığı `R`, `K`, `M`, `H` biçimleri `tools/stat-model.py:494,512` ve `tools/spell-model.py:532,566` biçim dizgeleriyle aynı alan adlarını kullanır (`grep -n "dmg_avg\|heal_instant" tools/damage-trace-summary.py` ve ilgili model satırları çıktısı).
- [ ] K6: Yalnızca standart kütüphane: `grep -n "^import\|^from" tools/damage-trace-summary.py` çıktısında üçüncü taraf modül yok; dosya ASCII ve LF (`file tools/damage-trace-summary.py` → "ASCII text" / "Python script, ASCII text executable", CRLF yok).
- [ ] K7: Kapsam: `git diff --stat gece/2026-10-02...bot/F1-10` yalnızca `tools/damage-trace-summary.py` ve plan dosyasını gösterir; `git status --short` boş; geçici dosyalar silindi.
- [ ] K8: `./tools/build.sh Release` hatasız biter (sunucu kodu değişmedi; `[UP]` varsa önce `./tools/run-servers.sh stop`); son 5 satırı yapıştır.

## 7. Doğrulama komutları

```bash
python3 tools/damage-trace-summary.py --selftest
python3 tools/damage-trace-summary.py /tmp/f110_sample.log --tol 15 --min-n 3 \
    --stat-model /tmp/f110_stat.txt --spell-model /tmp/f110_spell.txt
python3 tools/damage-trace-summary.py /tmp/olmayan.log; echo "rc=$?"
file tools/damage-trace-summary.py
git diff --stat gece/2026-10-02...bot/F1-10
git status --short
./tools/build.sh Release
```

(`/tmp/f110_*` dosyalarını adım 6'da sentetik olarak sen üretirsin; işin sonunda sil.)

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `tools/*.py` dosyaları ASCII + **LF** (mevcut `tools/*.py` gibi); `AGENTS.md` §3'teki CRLF kuralı depo kaynağı içindir, `tools/` betikleri LF'tir (`.gitattributes`'ı kontrol et; `file tools/stat-model.py` referans).
- Yalnızca standart kütüphane (`argparse`, `collections`, `re`, `sys`, `io`, `statistics` vb.). Yeni üçüncü taraf paket yok.
- **Mekanik:** değişen oyun mekaniği yok; araç salt okur. Sunucu, DB, `docs/**` değişmez.
- **Gizlilik:** Gerçek log oyuncu adları içerebilir; rapora/commit'e **gerçek log satırı kopyalama**. Yalnızca sentetik `Bot*_K/_E` adları. Betik logları değiştirmez ve kopyalamaz.
- **Yorumlama uyarısı (çıktıya not olarak da yaz, `L note=`'tan ayrı olarak betik başlık docstring'inde):** modeller isabet halinde ortalama hasarı verir; ıskalar loglanmadığından ölçüm de "isabet başına" ortalamadır, bu yüzden karşılaştırma uygundur. Küçük örneklemde (`n` az) ± %15 gürültü olabilir; `LOW_N` bu yüzden vardır. `app_ratio` ve `t_ac` aralığı Q-08 (AC debuff'ının çift uygulanması) için yan göstergedir; kesin hüküm vermez.
- Riskler: (1) model çıktısında skill adının boşluk içermesi ayrıştırmayı bozabilir — yalnızca `anahtar=değer` jetonları okunur; (2) `K` ve `M` aynı `skill=` kimliğini paylaşmaz (warrior vs mage/priest) ama yine de önce `K`, sonra `M` aranır; (3) Python `//`/`round` farkları yok, yalnızca raporlama aritmetiği; (4) log CRLF olabilir (Windows `fprintf` metin kipi) — `rstrip("\r\n")` şart.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F1-10` (taban: `gece/2026-10-02`); `79f3b19` "Hasar logu ozet ve model karsilastirma betigi"; rapor + `Durum` commit'i bu satırdan sonra.
- Değişen dosyalar ve neden:
  - `tools/damage-trace-summary.py` (yeni): planın istediği tek araç; `L`/`A`/`D`/`C` bölümleri ve `--selftest`.
  - `plans/F1-10-hasar-logu-ozet-betigi.md`: yalnızca `Durum` satırı ve bu rapor.

- Doğrulama (adım 1): `GameServer/DamageTrace.cpp:123` `fprintf` (20 sütun), `ctx` üretimi `:111-117`; `GameServer/User.cpp:1871` `originalAmount`, kanca `:1953`; `tools/stat-model.py:467` `write_report`, `R` biçimi `:494`, `K` biçimi `:512`; `tools/spell-model.py:479` `write_report`, `M` biçimi `:532-533`, `H` biçimi `:566-567`. Kayma yok.

- Kabul kriterleri öz-değerlendirme (komut çıktıları):
  - **K1 ✔** `python3 tools/damage-trace-summary.py --selftest` → `selftest OK`, `rc=0`.
  - **K2 ✔** Sentetik 4 satırlık log (`/tmp` altında üretildi, sonra silindi) ile:
    ```
    == L ==
    L files=1 lines=5 parsed=5 bad=0 span_s=0.4 filtered=0 zero=0
    L note=misses_not_logged hit_rate_not_measured
    == A ==
    A ctx=R kind=dmg a=BotWP_K t=BotMI_K n=2 req_avg=205.0 req_min=200 req_max=210 app_avg=175.0 app_ratio=0.854 a_hit=50-50 t_ac=100-100 lethal=0
    A ctx=S101006 kind=heal a=BotPHD_K t=BotWP_K n=1 req_avg=300.0 req_min=300 req_max=300 app_avg=300.0 app_ratio=1.000 a_hit=50-50 t_ac=100-100 lethal=0
    A ctx=S109510 kind=dmg a=BotWP_K t=BotMI_K n=1 req_avg=250.0 req_min=250 req_max=250 app_avg=100.0 app_ratio=0.400 a_hit=50-50 t_ac=100-100 lethal=1
    == D ==
    D a=BotWP_K t=BotMI_K kind=dmg n=1 req_sum=-80 req_avg=80.0 ctx_seen=-
    == C ==
    C kind=R a=BotWP_K t=BotMI_K skill=- n=2 meas=205.0 model=205.0 diff_pct=+0.0 verdict=LOW_N match=exact range_viol=0
    C kind=H a=BotPHD_K t=BotWP_K skill=101006 n=1 meas=300.0 model=310.0 diff_pct=-3.2 verdict=LOW_N match=exact range_viol=-
    C kind=K a=BotWP_K t=BotMI_K skill=109510 n=1 meas=250.0 model=210.0 diff_pct=+19.0 verdict=LOW_N match=exact range_viol=-
    ```
    `grep -c "^== "` = **4** (başlık sırası L, A, D, C).
  - **K3 ✔** C satır alanları `kind= a= t= skill= n= meas= model= diff_pct= verdict= match= range_viol=` sırasıyla; `--tol`/`--min-n` varsayılanları 15 ve 5 (kullanım satırı `--tol PCT ... (default 15)`, `--min-n N ... (default 5)`; kod `options = {"tol": 15.0, "min_n": 5, ...}`).
  - **K4 ✔** Boş dosya → `parsed=0 bad=0`, `rc=0`; yalnızca bozuk satırlı dosya → `lines=2 parsed=0 bad=2`, `rc=0`; olmayan yol → `error: cannot read log: [Errno 2] ...`, `rc=2`. Hiçbirinde traceback yok.
  - **K5 ✔** `grep` ile `tools/damage-trace-summary.py:357,359` `heal_instant`/`dmg_avg`; model tarafı `stat-model.py:494,512` ve `spell-model.py:532,566` ile aynı alan adları.
  - **K6 ✔** `file tools/damage-trace-summary.py` → `Python script, ASCII text executable`; CRLF yok (`grep -c $'\r'` = 0); içe aktarımlar yalnızca `io`, `re`, `sys`.
  - **K7 ✔** `git status --short` temiz; `git diff --stat gece/2026-10-02...bot/F1-10` yalnızca `plans/F1-10-...md` ve `tools/damage-trace-summary.py`; `/tmp/f110_*` geçici dosyaları silindi.
  - **K8 ✔** `./tools/build.sh Release` → `rc=0`; son satırlar: `proj-LogInServer.vcxproj -> ...LogInServer.exe`, `proj-GameServer.vcxproj -> ...GameServer.exe`, `proj-AIServer.vcxproj -> ...AIServer.exe`.

- Plandan sapmalar ve gerekçeleri:
  - Yok. Plan dışı dosyaya dokunulmadı; sunucu/DB/`docs/**` değişmedi.
  - Küçük uygulama tercihleri: (a) `L` satırında `filtered` ve `zero` alanları taban satırın sonuna eklendi (plan bu iki alanı "L satırına ekle" dediği için); (b) modele karşılık gelmeyen C satırlarında `model`/`diff_pct` alanları `-` olarak basılır (hüküm `NO_MODEL`).
  - `--selftest` içinde `_E` profil geri düşüşü, `K` boşluklu skill adı, `lethal`/`app_ratio` ve `ctx=- / primary=0` → `D` ayrıca assert edilir; planın istediği maddelerin tamamı kapsanır.

- Açık sorular: Yok. `docs/15` §4.1 T-MECH-DMG ölçümü insan oturumu; F1-10 sonuç yorumu gerçek log geldiğinde yapılır.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz doldurulmadı)
