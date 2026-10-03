# F8-04: Kompozisyon çeşitliliği için ek 4 bot karakteri: `db/006_bot_characters_diversity.sql` (+ geri alma) ve `BOT_TABLE` ek girişleri (ulus başına 3. W-P ve 3. M-F, toplam 20)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F8 — Değerlendirme ve 8v8 (`docs/17` §2; kapı G8) |
| Branch | `bot/F8-04 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `db/002` F1-04, F4-27, F4-40, F8-02 `KAPANDI`. **F8-03'e bağımlı değildir** (aşağıda §0): ikisi birbirinin satırını okumaz; önerilen sıra F8-03 → F8-04, ama ters sıra da geçerlidir. F6/F7'ye bağımlı değildir |
| İlgili gereksinim / kabul | `docs/15` §6a (satır 283-291: "20 karakter = 16 + 4"), `docs/09` §2.4 (C8-B/C/D), `docs/04` §3.3 (satır 67), `docs/17` G8, ADR-0002 Ek F8-02 madde 2-3, EVAL-8v8-MIX (`docs/15` §4.6), `docs/14` §13 kompozisyon genellemesi |
| Tahmini büyüklük | S (F8-03 ile aynı kalıp; ≤ 4 dosya) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak; referanslar `gece/2026-10-02` @ `7891f74`) |

---

## 0. Neden TASLAK (HAZIR yapma ön koşulları) ve F8-03'ten bağımsızlık

1. **Proje sahibi izni (DB'ye yazma)** ve **`BOT_TABLE` yöntemi kararı**: F8-03 §0 madde 1-3 ile aynı kararlar geçerlidir; F8-03 HAZIR'a geçerken verilen karar bu plana da uygulanır (farklı çıkarsa bu plan da değişir).
2. **Neden ayrı plan:** `docs/15` §6a "20 karakter 16'nın üstüne 4 ek karakterdir; yedek ya da test profili değildir"; kaynağı yalnızca EVAL-8v8-MIX (C8-B vs C8-C) ve C8-D'dir. EVAL-8v8-A 16 karakterle koşulur; çeşitlilik karakterleri ilk 8v8 ölçümünü beklemeden ayrı onayla eklenir. Proje sahibi "16 mı 20 mi" kararını `docs/reports/degerlendirme-2026-10-02-ek.md` satır 134'te soru olarak bırakmıştı; doküman 16 + 4 olarak kesinleşti, ama ikinci adımı (20) ayrı izin ve ayrı doğrulamayla almak bu planın amacıdır.
3. **Bağımsızlığın tanımı `[A]`:** F8-04 yalnızca indeks **3** adlarını (`BotWP3_*`, `BotMF3_*`) ekler; F8-03 yalnızca indeks **2** adlarını. İkiz referansı her zaman **indeks 1** (özgün) satırdır (`BotWP_K` vb.), F8-03 satırı değil. Sonuç: yalnızca `db/002` + `db/006` uygulanmış bir DB'de (indeks 2 yok) araç 16 karakter sayar (`have=16`: WP ve MF ulus başına 2) ve `min16_missing = 0` bildirir (araç sayı tabanlıdır, indeks boşluğu kabul eder), `full20_missing = 4` kalır. Ad boşluğu (2 yokken 3 var) bilinçli kabul edilir; tek kısıt, kompozisyon seçimini ad değil sınıf profiline göre yapmaktır (senaryo dosyaları karakter adlarını ayrı ayrı yazar).
4. **Numara:** `db/006` (proje sahibi kuralı; `db/README.md` "Numara ayırma notu" `db/005`/`db/006`'yı bu iş için ayırmıştır).
5. F8-03 §0 madde 4 (`Upgrade` kademesi doğrulaması) ve madde 6 (`db/003`/`db/004` anlık durumu belirsiz) aynen geçerlidir.

## 1. Amaç

Ulus başına 3. W-P ve 3. M-F karakterini (`BotWP3_K`, `BotMF3_K`, `BotWP3_E`, `BotMF3_E`) `db/002` ile aynı stat/skill/ekipman/quest/stok düzeniyle ekleyen geri alınabilir bir migration (`db/006`) ve `BOT_TABLE` girişleri. F8-03 ile birlikte DB'de ulus başına 10, toplam **20** karakter olur (C8-A..D her biri aynı kümeden kurulabilir). Kabul: `tools/bot-composition-check.py` 20 karakterde `full20_missing = 0` bildirir ve karakter kümesi `tools/bot-composition-check/sample-20.sql`'in (hedef durum fixtürü) 20 adıyla aynıdır.

## 2. Bağlam (okunması zorunlu)

- F8-03 §2 (aynı kaynaklar: `db/002:124-137` satır biçimi, `:139-277` üretim döngüsü, `:282-328` değişmezler; `db/002_bot_characters_rollback.sql:16-62`; `db/003_bot_quests.sql:96-101`; `db/004_bot_inventory.sql:205`).
- `docs/09` §2.4: C8-B `3 W-P + 1 W-G + P-HD + P-HB + 1 M-F + 1 M-I`; C8-C `1 W-P + 1 W-G + P-HD + P-HB + 3 M-F + 1 M-I`; C8-D `3 W-P + 1 W-G + P-HD + 2 M-F + 1 M-I`. Profil başına en büyük sayı: W-P 3, M-F 3 (ADR-0002 Ek F8-02 madde 2).
- `tools/bot-composition-check/sample-20.sql` (F8-02 fixtürü, yürütülebilir SQL değil): hedef 20 ad. `tools/bot-composition-check.py:537-` `sample20_rows()` ile aynı küme (indeks 2 ve 3).
- Kod: `GameServer/Bot/BotManager.cpp:58-66` `BOT_TABLE` (sabit boyuttan bağımsız tarama `:68-76`, `:2877`); `ScenarioRunner.cpp:16` `SCENARIO_MAX_BOTS = 16` (aynı anda en fazla 16 karakter; 4'ü her maçta boşta, `docs/15` §6a).

## 3. Kapsam

### 3.1 `db/006_bot_characters_diversity.sql` (yeni; ASCII, LF)

F8-03 §3.1 ile aynı yapı (başlık yorumu, zorunlu `-v Upgrade`, sahiplik/ikiz/`ITEM` ön denetimleri, tek işlem, değişmez denetimleri, öz denetim). Farklar:

- **`@bots` (yalnızca 4 satır):**

| profile | charName | account | nation | race | class | diğer sütunlar |
|---|---|---|---|---|---|---|
| `WP` | `BotWP3_K` | `BotAccWP3K` | 1 | 1 | 106 | `db/002:126` satırıyla aynen |
| `MF` | `BotMF3_K` | `BotAccMF3K` | 1 | 3 | 110 | `db/002:130` satırıyla aynen |
| `WP` | `BotWP3_E` | `BotAccWP3E` | 2 | 11 | 206 | `db/002:132` satırıyla aynen |
| `MF` | `BotMF3_E` | `BotAccMF3E` | 2 | 12 | 210 | `db/002:136` satırıyla aynen |

- İkiz kontrolü: `BotWP_K`, `BotMF_K`, `BotWP_E`, `BotMF_E` (indeks 1). `BotWP2_*`/`BotMF2_*` **okunmaz ve beklenmez**.
- Quest ve çanta: F8-03 §3.1.5 ile aynı kayıtlar/yerleşim.
- Öz denetim çıktısı: `BOTCHARS20: rows=4 ok=4 fail=0 twin_mismatch=0 orig_unchanged=1 other_rows_unchanged=1 upgrade=<n>`. "Orijinal satırlar" denetimi bu betikte **12 özgün satırı** kapsar (F8-03'ün 4 satırı bu betiğin dokunmadığı satırlardır ve sağlama toplamına dahil edilmez; aksi halde iki betik birbirine bağımlı olurdu).
- Yeniden çalıştırma: yalnızca bu betiğin 4 satırını siler/yeniden ekler.

### 3.2 `db/006_bot_characters_diversity_rollback.sql` (yeni)

F8-03 §3.3 kalıbı; yalnızca `BotWP3_*`/`BotMF3_*` + `BotAccWP3*`/`BotAccMF3*`; çıktı `BOTCHARS20_ROLLBACK: removed=<N>`.

### 3.3 `BOT_TABLE` (`GameServer/Bot/BotManager.cpp`)

Ayrı yorumlu blok (`F8-04: diversity set (db/006)`): `{ "BotWP3_K", "BotAccWP3K" }, { "BotMF3_K", "BotAccMF3K" }, { "BotWP3_E", "BotAccWP3E" }, { "BotMF3_E", "BotAccMF3E" }`. `:58` yorumunda `db/006`'yı da say. Yöntem kararı F8-03 §3.2'dedir; o karar değişirse bu bölüm de değişir. F8-03 ile aynı tabloya eklemeden doğan birleştirme çakışması en fazla bitişik tek satırdır; iki bloğu da koruyarak çöz.

### 3.4 `db/README.md`

`006` bölümü (yalnızca ekleme).

**Kapsam dışı (yapılmayacak)**

- İndeks 2 karakterleri (F8-03), 4'ten fazla ek karakter, 32/64 bot performans kümesi (`docs/15` §6a son cümle; ayrı karar).
- `db/002`..`db/005`, `tools/bot-refill.sh`, `tools/bot-composition-check.py` (`sample-20.sql` dahil) değişikliği; `ScenarioRunner` limiti; kompozisyon senaryo dosyaları (F8 harness planı).
- Kişisel veri tabloları (`TB_USER`, `CURRENTUSER`, `KNIGHTS*`...); yalnızca `USERDATA`/`ACCOUNT_CHAR`/`WAREHOUSE` bot satırları.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `db/006_bot_characters_diversity.sql` | yeni | ASCII, LF |
| `db/006_bot_characters_diversity_rollback.sql` | yeni | ASCII, LF |
| `db/README.md` | değiştir | yalnızca `006` bölümü eklenir |
| `GameServer/Bot/BotManager.cpp` | değiştir | yalnızca `BOT_TABLE` (4 giriş) ve `:58` yorumu |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. Yeni `.cpp`/`.h` yok: `vcxproj`/`.filters` değişmez.

## 5. Uygulama adımları

1. `git switch -c bot/F8-04 gece/2026-10-02`; `Durum` → `UYGULANIYOR`; sunucular kapalı.
2. İkiz ön kontrolü (yalnızca sayaç). `db/006` ve rollback (§3.1-3.2); `db/README.md`.
3. Araç komutları (§7), DB'ye yazmadan.
4. SQL denemeleri: uygula → tekrar uygula → geri al (`removed=4`) → tekrar geri al (`removed=0`) → uygula; **F8-03 uygulanmamış** bir DB'de de çalıştığını göster (bağımsızlık kanıtı: `db/005` yok, `db/006` uygulanır).
5. `BOT_TABLE` (§3.3); build ve test.
6. Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok; `./tools/run-tests.sh` `0 failed`, test sayısı değişmedi (sayıyı rapora yaz)
- [ ] K2: `git diff --stat gece/2026-10-02...bot/F8-04` yalnızca §4'teki dosyalar (+ plan); `git diff --check` boş
- [ ] K3 (bağımsızlık): `python3 tools/bot-composition-check.py --sql db/002_bot_characters.sql --sql db/006_bot_characters_diversity.sql --strict --target min16` rc=0 (`have == 16`, `min16_missing == 0`, `full20_missing == 4`); F8-03 yokken de hata yok
- [ ] K4 (20 karakter): `python3 tools/bot-composition-check.py --sql db/002_bot_characters.sql --sql db/005_bot_characters_16.sql --sql db/006_bot_characters_diversity.sql --strict` rc=0, `--json` `summary.have == 20`, `full20_missing == 0`, `errors == []`; adlar `sample20_rows()` ile aynı küme: `python3 - <<'EOF'` ile `tools/bot-composition-check.py` içe aktarılıp `sample20_rows()` adları, üç SQL dosyasından ayrıştırılan adlarla kümece karşılaştırılır (fark boş) (F8-03 `bot/F8-03` birleşmiş olmalı; birleşmemişse bu madde `BEKLİYOR` yazılır, K3 yeterlidir)
- [ ] K5: `db/006` ilk çalıştırma `BOTCHARS20: rows=4 ok=4 fail=0 twin_mismatch=0 orig_unchanged=1 other_rows_unchanged=1 upgrade=<n>`; ikinci çalıştırma aynı; rollback `BOTCHARS20_ROLLBACK: removed=4` sonra `removed=0`; `grep -c "LIKE 'Bot" db/006_bot_characters_diversity.sql` = 0
- [ ] K6: `BOT_TABLE` adları ile SQL adları eşit (F8-03 K6 komutu `db/006` dosyası eklenerek; F8-03 birleşmişse 20 ad, değilse 16 ad)
- [ ] K7: kodlama: yeni dosyalar ASCII ve LF; `BotManager.cpp` kodlaması değişmedi; `db/README.md` yalnızca ekleme
- [ ] K8 (Claude, çalışma zamanı, oyun içi doğrulama ayrı kayıtlı): `db/005`+`db/006` uygulanmış, 20 karakter DB'de; (a) 4 yeni bot `/bot spawn` ile `in_game`, `snap` sınıf/seviye/konum/hp doğru, ikiz (`BotWP_K`) ile `maxHp`/`maxMp`/`stock` aynı; (b) C8-B kompozisyonu (3 W-P) ve C8-C kompozisyonu (3 M-F) bir ulusta aynı anda 8 bot olarak `in_game` olur (`/bot list` 8 + karşı ulus 8 = 16, `ScenarioRunner` senaryo `loaded`); (c) 20 adın hepsi `spawn` komutunca bilinen ad (bilinmeyen ad uyarısı yok); (d) iş bitince `despawn all`, sunucular kapatılır, DB istenen duruma
- [ ] K9: Uygulayıcı Raporu dürüst; yapılmayan/doğrulanamayan açıkça yazılmış; satır içeriği rapora girmemiş

## 7. Doğrulama komutları

```bash
python3 tools/bot-composition-check.py --sql db/002_bot_characters.sql --sql db/006_bot_characters_diversity.sql --strict --target min16; echo rc=$?
python3 tools/bot-composition-check.py --sql db/002_bot_characters.sql --sql db/005_bot_characters_16.sql --sql db/006_bot_characters_diversity.sql --strict; echo rc=$?
python3 tools/bot-composition-check.py --sql db/002_bot_characters.sql --sql db/005_bot_characters_16.sql --sql db/006_bot_characters_diversity.sql --json | python3 -c "import json,sys; d=json.load(sys.stdin); s=d['summary']; print(s['have'], s['full20_missing'], d['errors'])"
SQLCMD="/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_kn_online -b -v Upgrade=7 -i db/006_bot_characters_diversity.sql
"$SQLCMD" -S '.\SQLEXPRESS' -E -d FDP_kn_online -b -i db/006_bot_characters_diversity_rollback.sql
./tools/build.sh Release && ./tools/run-tests.sh 2>&1 | tail -3
git diff --stat gece/2026-10-02...bot/F8-04
```

## 8. Kısıtlar ve uyarılar

- F8-03 §8 maddeleri aynen geçerlidir (kodlama, kişisel veri, sunucular kapalı, `-v` zorunlu, `db/002` yeniden çalıştırma etkisi, `Upgrade` uyuşmazlığı, `ENABLED=0` değişmez).
- **Uygulama sırası tuzağı:** `db/002` yeniden çalıştırılırsa 12 özgün satır sıfırlanır; `db/005`/`db/006` satırları kendi `Upgrade` kademesiyle yazılmış kalır. Kademe farkı varsa öz denetim (ikiz karşılaştırması) bir sonraki çalıştırmada uyarır; `db/002` yeniden uygulandıysa `db/005` ve `db/006` da aynı `-v Upgrade` ile yeniden uygulanmalıdır (README'ye not).
- Aynı anda en fazla 16 karakter girişlidir (`MAX_BOTS` 16, `SCENARIO_MAX_BOTS` 16); bu plan bu sınırı değiştirmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F8-04` — `<kısa-sha> [F8-04] …`
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
- İncelenen: `gece/2026-10-02...bot/F8-04` @ `<sha>`
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
