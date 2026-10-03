# F8 plan paketi: sıra, bağımlılıklar, araç şeması ↔ sunucu eşlemesi ve `db/003` ad çakışması

Hazırlayan: Claude (planlayıcı yardımcısı) · Tarih: 2026-10-03 · Durum: TASLAK (yardımcı belge; plan değil, depoya henüz alınmadı)
Kapsadığı planlar (hepsi `TASLAK`): `F8-03` (16 karakter, `db/005`), `F8-04` (+4 çeşitlilik, `db/006`), `F8-05` (`ScenarioReset` + `SETUP_FAIL`), `F8-06` (sunucu maç olayları), `F8-07` (`win_rule` kuralları).
Okunan kaynak: `gece/2026-10-02` dalı. Kod satırları `7891f74` (dal ucu, 2026-10-03 öğleden sonra) üzerinde yeniden doğrulandı; dal çalışma sırasında hareket etti (`c2c5a08` → `f4daa27` → `7891f74`), `docs/STATUS.md` satır numaraları her birleşmede kayar (aşağıda "≈" ile belirtilmiştir). Doğrulayamadığım noktalar §d'de.

## (a) Sıra ve F6/F7 ile bağımlılık

### a.1 Bağımlılık grafiği

```
F8-06 (olaylar, takım tanımı, sayaçlar) --+--> F8-05 (ScenarioReset, SETUP_FAIL) --+--> F8-07 (win_rule, sayım, respawn sürücüsü)
        |                                                                         |
        +-------------------------------------------------------------------------+   (F8-07 hem F8-06 sayaçlarını hem F8-05 SETUP_FAIL'ini tüketir)

F8-03 (db/005, 16 karakter) .. bağımsız .. F8-04 (db/006, +4)       (DB hattı; F8-05/06/07'den bağımsız; yalnızca BOT_TABLE'a satır ekler)
```

- **F8-06 önce:** takım tanımı (`team_a`/`team_b`), `MatchEvents` sayaçları, `BeginScenarioMatch`/`EndScenarioMatch` onda; F8-05 yerleşim için takım bilgisini, F8-07 kural motoru için sayaçları ondan alır. Betikli 2v2 ile F5-55 beklemeden doğrulanabilir (betikli `attack`/`cast` vardır).
- **F8-05 ikinci:** F8-06'nın takım tanımını kullanır; F8-07 `SETUP_FAIL`'in sayıma katılmamasını bu plana bağlar.
- **F8-07 üçüncü:** F8-06 + F8-05 `KAPANDI` olmadan HAZIR olmaz.
- **F8-03/F8-04:** DB yazma izni (proje sahibi) ve sunucuların kapalı olması gerekir; F8-05/06/07 ile **kod bağımlılığı yok**. Yalnızca ikisi de `BotManager.cpp` `BOT_TABLE`'a kendi bloğunu ekler (en fazla bitişik tek satırlık birleştirme çakışması). F8-04, F8-03'e bağlı değildir: ikiz referansı indeks-1 özgün satırdır; araç sayı tabanlıdır (`tools/bot-composition-check.py` `build_result`), `db/002 + db/006` ile 16 sayar.
- **Üç büyük plan (F8-05/06) 10 dosyayı aşar** (12 ve 15 dosya): HAZIR öncesi bölme kararı plan içinde (S1/S2/S3 commit dilimleri ya da ayrı numara); numara ayırmak gerekirse boş numara kullanılır (F8-08.., kimlikler yeniden kullanılmaz).

### a.2 Hangi ölçüm hangi plana bağlı

| Ölçüm / kapı | Gereken planlar | Gereken karakter sayısı | Not |
|---|---|---|---|
| F6 `EVAL-1v1-<A>-<B>` (36 çift, N = 50, `docs/16` §7; `docs/15` §4.6) | **F8-06 + F8-05 + F8-07** (`wipe_first`, 1v1 "ilk ölen kaybeder", `respawn: off`) | 2 (mevcut 12 yeter) | Betikli/elle mekanik testleri (`T-WAR-*`, `T-SOLO-*`, `T-SUR-*`) bunlar olmadan yürür; **puanlanan** EVAL-1v1 yürümez (başlangıç sıfırlaması yok: NP 0 → `Regene` yok KI-013, konum/HP/stok sızıntısı). |
| F7 `EVAL-2v2..5v5` | F8-06 + F8-05 + F8-07 (`killdiff_timed`, `respawn: on`, otomatik respawn sürücüsü) | ≤ 10 (mevcut 12 yeter; C5 = 5/ulus) | `docs/17` F7 Test satırı; `F8-02` aracı `have12_small_ok` |
| F7 `EVAL-WIPE` (8v8), `EVAL-HEALSTALL` | + F8-03 | 16 | `wipe_first` 8v8 |
| `EVAL-8v8-A` (C8-A) | + F8-03 | 16 | `docs/15` §6a |
| `EVAL-8v8-MIX` (C8-B vs C8-C), C8-D | + F8-03 + F8-04 | 20 | 3. W-P ve 3. M-F |
| `EVAL-5v8` | + F8-03 | 13 (bir ulusta 8) | 5 + 8 |
| G8 (`docs/17` §5), T-IGT-EVAL-01 | hepsi + `evalset-v1`/OP-*/taraf değişimi harness'i (henüz numarasız ayrı plan) | 20 | pilot (F8-07 K11) `baseline-v1` ister |

### a.3 `ScenarioReset` (F8-05) F6'daki EVAL-1v1 için **ne zaman** gerekir

- `docs/17` F6 satırı "Test: ... EVAL-1v1; Kabul: L0 politikası B0-NAIVE'i EVAL-1v1'de anlamlı yener" der. Kullanıcı notasyonu **F6-06** (EVAL-1v1 harness'i) olarak geçiyor; **F6 plan dosyaları henüz yok** (`plans/` altında F6-* yok, `docs/STATUS.md` F6 "PLANLANDI"), bu numarayı doğrulayamadım.
- Gerekçe: puanlanan 1v1 koşusu "her tekrar aynı başlangıçtan" olmak zorunda (`docs/15` §6 madde 1-3, §6a). Başlangıç sıfırlaması olmayan koşuda (i) her ölüm NP'yi −50 düşürür, 20 ölümde bot doğamaz (KI-013, `ActionExecutor.cpp:1918-1919`), (ii) HP/MP/NP, konum, çanta stoku ve **ekipman dayanıklılığı** (`ItemWoreOut`, `User.cpp:3270-3345`; `docs/15` §6a'da yok, F8-05 §0 madde 4) maçtan maça taşınır, (iii) ardışık maçlar bağımsız olmaz (ADR-0032-DEG "Bağlam").
- **Öneri (zaman):** F6 *geliştirme* (solo davranış testleri) F8-05'i beklemez. F6 **kabul** ölçümü (EVAL-1v1 matrisi) için F8-06, F8-05, F8-07 F6'nın ilk puanlanan koşusundan **önce** `KAPANDI` olmalı; yani F6 planları yazılırken EVAL-1v1 harness planı (F6-06) bu üçüne `Bağımlı olduğu planlar` olarak bağlanmalı. F8-06 en erken yapılabilir (F5-55 beklemez); F8-05'in yürünebilir başlangıç noktası denetimi F5-55 sonrası eklenir (F8-05 §0 madde 6).
- **Geçici sürücü:** `killdiff_timed` "respawn açık" için bugün karar katmanı yok; F8-07 `ScenarioRunner` içinde geçici respawn sürücüsü önerir (F8-07 §0 madde 4, karar B3). F6 `Brain` gelince sürücü kapatılır.

## (b) `tools/bot-outcome-eval.py` şeması ↔ sunucunun bugün yaydıkları

Aracın okuduğu alanlar `bot-outcome-eval.py` (`parse_file` `:172-209`, `evaluate_match` `:476-583`, `kill_side` `:294-309`) ve `tools/bot-outcome-eval/sample.jsonl` (21 satır); sunucu tarafı `GameServer/Bot/Telemetry.cpp` (`Emit` `:229`, `BeginMatch` `:298`, `EndMatchLocked` `:399`, `AppendLine` `:533`), `BotManager.cpp` (`CommandMatch` `:901-1059`), `ScenarioRunner.cpp`.

**Önemli düzeltme:** `MATCH_START` ve `MATCH_END` sunucuda **vardır** (F3-02; `Telemetry.cpp:376, :444`); eksik olan **içerikleridir**. `DAMAGE`, `DEATH`, `RESPAWN`, `TEST_TELEPORT`, `SETUP_FAIL`, `win_rule` ve `team_a` için kodda `Emit`/alan **yok** (`git grep` boş).

| # | Araç beklentisi (kaynak) | Sunucu bugün | Durum | Kapatan plan |
|---|---|---|---|---|
| 1 | Her satır JSON nesnesi; `ev:str`, `match:str` (`"-"` atlanır), `t:int` (ms) (`parse_file`) | `t`, `match`, `bot`, (`name`), `ev`, `mode:"live"` (`AppendLine` `:533-558`); `match` maç dışında `"-"` | UYUMLU | — |
| 2 | `MATCH_START` maç başına **tam 1** adet (`evaluate_match` `:479-495`) | `BeginMatch` tek `MATCH_START` yazar (`:376`) | UYUMLU | — |
| 3 | `MATCH_START.team_a`, `team_b`: **boş olmayan, kesişmeyen int listesi**; kimlikler `bot`/`killer` ile aynı uzayda (`:496-499`) | **yok**; yalnızca `composition` = bot **adları** (string listesi) ve `in_game` (`BotManager.cpp:974-990`); senaryo dosyasında takım tanımı yok (`bots` düz liste, `ScenarioRunner.cpp:325-372`) | **EKSİK** | F8-06 (`team_a`/`team_b` anahtarı, ulusa göre türetme; `CUser::GetID()` = `GetSocketID()`, `User.h:114` aynı uzay `[D]`) |
| 4 | `MATCH_START.win_rule` (`killdiff_timed`/`wipe_first`/`timed_score`; yoksa **araç varsayılanı `killdiff_timed`**, `:227-235`) | yok | **EKSİK** + varsayılan çelişkisi (sunucu eski senaryo = `timed_score`) | F8-06 (yer tutucu `timed_score`, açık soru A3) + F8-07 (gerçek değer) |
| 5 | `duration_sec` (1..3600), `win_margin` (1..8), `early_end_margin`, `engage_timeout_sec` (varsayılanlar `:238-283`) | yok; senaryo `duration_sec` var (`ScenarioRunner.cpp:451`) ama `MATCH_START`'a yazılmıyor | **EKSİK** | `duration_sec`: F8-06; eşikler: F8-07 |
| 6 | `DAMAGE`: yalnızca `t`; **ilk kayıt** (`t >= start_t`) = engage (`:530-534`). `sample.jsonl`: `bot` = hasar alan, `src`, `dst`, `amount` | **hiç yazılmıyor** | **EKSİK** | F8-06 (`CUser::HpChange` kancası `User.cpp:1924`/`:2010-2012`; yalnızca bot–bot, düşman takım, gerçek HP düşüşü: üçüncü taraf hasarı yanlış engage üretirdi) |
| 7 | `DEATH`: `bot` = ölen (int), `killer` = öldüren birim kimliği (int, `-1`/yok = bilinmeyen) (`kill_side`) | **hiç yazılmıyor** | **EKSİK** | F8-06 (`CUser::OnDeath` `User.cpp:4795`) |
| 8 | `RESPAWN`: `bot` (int); `wipe_first`'te varlığı ⇒ `RESPAWN_IN_WIPE_FIRST` (`:552-562`) | **hiç yazılmıyor** | **EKSİK** | F8-06 (`CUser::Regene` `AttackHandler.cpp:100`, `:231`) |
| 9 | `TEST_TELEPORT`: varlığı ⇒ geçersiz (`:546`) | emisyon yok; botların maç içi ışınlama komutu da yok (`TEST_TELEPORT`/`testtp` kodda geçmiyor) | EKSİK (kaynak da yok) | açık soru A2 (F8-06 kapsam dışı) |
| 10 | `SETUP_FAIL`: varlığı ⇒ geçersiz; araç bunu **MATCH_START'tan bağımsız** değerlendirir (`:479-488`) | yok | **EKSİK** | F8-05 (`ScenarioRunner` doğrulama adımı) |
| 11 | `MATCH_END`: `t`; `result == "aborted"` ⇒ ABORTED (`:544-545`); `completed` yok sayılır | `result` serbest belirteç: `completed` (`ScenarioRunner.cpp:818`), `aborted` (`:881`), **`bot_lost`** (`:807`) (`BotManager.cpp:1016-1050`) | **ÇELİŞKİ** (aşağıda #C1): `bot_lost` aracın ABORTED'ine düşmez; ADR "result ADR kodu" der | F8-07 (karar B1) |
| 12 | Süre penceresi: `window_end = engage + duration_sec` (`:535`); gözlem `window_end`'den önce bitmişse `TRUNCATED` (`:564-570`) | `ScenarioRunner` süreyi **maç açılışından** sayar (`elapsed >= durationSec`, `ScenarioRunner.cpp:814-815`) ⇒ araç her koşuda TRUNCATED döndürürdü | **ÇELİŞKİ** (#C2) | F8-07 (karar B2: süre engage'den) |
| 13 | `DAMAGE/DEATH/RESPAWN` seviyesi `decisions` (`docs/16` §3.3 satır 69) | varsayılan `[BOT] TELEMETRY=summary` (`Telemetry.cpp:117`): bu olaylar `Emit(TEL_DECISIONS, ...)` ise **yazılmaz** | RİSK | F8-06 (uyarı), F8-07 (skorlu kuralda `TEL_DECISIONS` yoksa reddet; karar B4) |
| 14 | Olay kaybı yok varsayımı | `Emit` sert sınırda (`HARD_LIMIT` 8192, `Telemetry.h:56`) olay düşürür; `DAMAGE` yüksek hacimli | RİSK | F8-06/F8-07: kazanma kararı `MatchEvents` sayaçlarından; `MATCH_END` sayaçları jsonl ile çapraz denetlenir |
| 15 | `docs/16` §3.1 `match` = `<senaryo>-<seed>-<tekrar>-<taraf>`, `mode` ∈ train/eval/live/debug | `<scenario>-<seed>-<run>` (`Telemetry.cpp:338`), `mode` hep `live` (`:549`) | bilgi (araç kullanmıyor) | ayrı plan/taraf değişimi harness'i |
| 16 | `DAMAGE` için `docs/16` §3.2 `skill`, `hp_before/after` | `skill` bağlamı yalnızca `FDP_DAMAGE_TRACE` derlemesinde thread-yerel (`DamageTrace.h:11-23`) | EKSİK (araç okumuyor) | F8-06 `skill` yazmaz, `hp_before/hp_after` yazar |

**Çelişkiler (karar ister):**
- **#C1 `MATCH_END.result`:** ADR-0031-DEG ("result ∈ win_a|win_b|draw|invalid|no_result; teknik sonlanma ayrı alan") ↔ `docs/16` §3.3 satır 77 ve araç (`result == "aborted"` = geçersiz). Öneri: iki alan birlikte yazılır (F8-07 §0 madde 2); araç değişmez.
- **#C2 Zaman tabanı:** ADR "süre engage'den" ↔ `ScenarioRunner` "maç açılışından". Öneri: ADR'ye uy (F8-07).
- **#C3 `SETUP_FAIL`/MATCH_START:** `docs/15` §6a (satır 281) ve ADR-0032-DEG "`MATCH_START` yerine `SETUP_FAIL`" ↔ `Telemetry` olayları yalnızca açık bir maç bağlamında kaydeder (`match = "-"` satırları araç tarafından atlanır) ve araç `SETUP_FAIL`'i `MATCH_START`'tan bağımsız `invalid` sayar. F8-05 §3.2: başarısız kurulumda da bir maç kaydı açılıp `invalid` ile kapatılır (MATCH_START + SETUP_FAIL + MATCH_END); metin ile uyuşmuyor, ADR/`docs/15` düzeltmesi ya da seçenek değişikliği gerekir (F8-05 açık soru A1).
- **#C4 Varsayılan `win_rule`:** araç `killdiff_timed`, eski senaryo koşucusu kural yok (`completed`). Öneri: sunucu `win_rule`'ü her `MATCH_START`'a açıkça yazar; anahtarsız eski senaryo `timed_score`.

## (c) `db/003` ad çakışması: düzeltilecek yerler ve yeni numaralar

**Karar (proje sahibi kuralı):** uygulanmış migration yeniden numaralandırılmaz; yeni işler boş numara kullanır. `db/003` = bot quest (F4-27), `db/004` = bot envanter (F4-40). Önerilen yeni numaralar: **`db/005` = F8-03 (16 karakter, indeks 2)**, **`db/006` = F8-04 (+4 çeşitlilik, indeks 3)**. 007+ gerekmez (yeni karakterlerin quest ve çanta düzeni `db/005`/`db/006` içinde yazılır; ayrı genişletme gerekirse 007 ayrılır).

**Durum notu (önemli):** bu görev yazılırken (`c2c5a08`) dört canlı doküman yeri hâlâ `db/003` diyordu. Çalışma sırasında dal ucuna `f4daa27` ("[belge] Q-27 kimlik çakışması giderildi ... db/003 ad çakışması (16/20 karakter için db/005, db/006)", 2026-10-03 11:36) geldi ve bunları düzeltti. Aşağıdaki tablo `7891f74` üzerindeki **güncel** durumdur.

### c.1 Zaten düzeltilmiş (`f4daa27`; yeniden dokunulmaz)

| Yer | Şimdiki metin |
|---|---|
| `docs/15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md:291` | "F8 ön koşulu: `db/005` (16 karakter, F8-03; `db/003` zaten bot quest betiğidir) ve `BOT_TABLE`'ın ..." |
| `docs/17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md:309` (G8) | "`db/005` (16 karakter, F8-03) ve `db/006` (+4 çeşitlilik, F8-04; ... `db/003` zaten bot quest betiğidir)" |
| `docs/04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md:67` | "`db/005` (16 karakter, F8-03) ve `db/006` (+4 çeşitlilik, F8-04) ön koşulu" |
| `docs/adr/ADR-0002-karakter-kurulum-betigi.md:27, :29, :31` (Ek F8-02 madde 1, 3, 5) | `db/005`/`db/006` |
| `tools/bot-composition-check.py:4` (docstring) | "future db/005, db/006" |
| `db/README.md` "Numara ayırma notu (2026-10-03)" | `db/001..004` yeniden numaralandırılmaz; 005 = 16 karakter (F8-03), 006 = +4 (F8-04); eski kayıtlardaki 16/20 anlamlı "`db/003`" bu betikleri kasteder |

### c.2 Hâlâ eski anlamda `db/003` diyen canlı yerler (düzeltme önerisi)

| Dosya:satır (`7891f74`) | Mevcut ifade (kısa) | Öneri |
|---|---|---|
| `docs/STATUS.md:98` (≈; F8-02 satırı) | "`db/002` (ve ileride `db/003`) `@bots` satırlarını ..." | `db/002` (ve ileride `db/005`, `db/006`) |
| `docs/STATUS.md:324` (≈; ADR-0002 Ek F8-02 özeti) | "`db/003` yazımı ertelendi" | `db/005`/`db/006` yazımı ertelendi |
| `docs/STATUS.md:492` (≈; nav kapanış notu) | "F8-02 ek karakter adlandırması (`db/003`'ü bağlar)"; "`db/003` (DB yazma izni sizde)" | `db/005`/`db/006`; F8-03/F8-04 planları yazıldığında plan numaralarıyla değiştir |
| `docs/STATUS.md:493` (≈; "Elle/başka plan") | "`db/003` (ek 4–8 karakter ...)"; "Araç çıktısı `db/003` planına girdi olacak" | `db/005`/`db/006` (F8-03/F8-04) |
| `plans/README.md:102` (F8-02 satırı, iki kez) | "`db/002` (ve ileride `db/003`)"; "`db/003` yazımı ... kapsam dışı" | `db/005`/`db/006` |

STATUS satır numaraları her gece birleşmesinde kayar: yapıştırmadan önce `git grep -n "db/003" docs/STATUS.md` ile yeniden bul.

### c.3 Tarihsel kayıtlar (düzeltilmesi **önerilmez**; `db/README.md` notu bunları kapsar)

- Kapanmış plan: `plans/F8-02-bot-kompozisyon-denetleyici.md:9, :17 (iki kez), :40, :45, :221` ("ileride `db/003`", "`db/003` (F8 ön koşulu)", `sample-20.sql` "`db/003` sonrası 20 karakter").
- `docs/reports/degerlendirme-2026-10-02.md:41, :109`; `docs/reports/degerlendirme-2026-10-02-ek.md:57, :134`; `docs/reports/degerlendirme-takip.md:49` (M6: "`db/003` + 16/20 karakterle 8v8"); `docs/reports/gece-2026-10-03-nav.md:73, :103, :128 (üç kez), :227, :229`.
- Öneri: `docs/reports/degerlendirme-takip.md` M6 satırı yaşayan bir izleme tablosudur: yalnızca bu satırı `db/005`/`db/006` olarak güncelle (rapor dosyalarının geri kalanı tarihsel kalır).

### c.4 `db/003` ifadesi doğru (quest) olan yerler, **dokunma**

`docs/STATUS.md:34, :111, :169, :170, :471-473, :496 (≈)`; `docs/04:71`; `docs/adr/ADR-0018:47`; `docs/reports/gece-2026-10-03.md:22, :48, :86, :105, :125, :204`; `plans/README.md:115`; `plans/F4-27-*`, `plans/F4-37-*`, `plans/F4-40-*` (quest betiği olarak).

### c.5 Plan-sonrası doküman işleri (Claude; planlar `HAZIR`'a geçerken)

- ADR-0002 Ek F8-03: `BOT_TABLE` yöntemi (sabit tablo + ad sınırı) ve `docs/15:291` "DB/ini kaynaklı tabloya çevrilmesi" cümlesinin düzeltilmesi (F8-03 §0 madde 2).
- ADR-0032-DEG Ek F8-05: DB yazım yolu (senkron IOCP vs `DatabaseThread` kuyruğu), `USER_SAVED_MAGIC` düzeltmesi, ekipman dayanıklılığı, `SETUP_FAIL` kayıt biçimi; `docs/15` §6a satır 274 ("çıkışta bellek içi durum biter") ve eksik "ekipman dayanıklılığı" satırı.
- ADR-0031-DEG Ek F8-07: B1 (`result`/`end_state`), B2 (süre engage'den), B3 (geçici respawn sürücüsü), B4 (seviye denetimi); `docs/16` §3.2/§3.3 satır 75-77 (sunucu artık yazıyor).
- `docs/KNOWN_ISSUES.md:33` KI-DEG-05: kapanış kriteri F8-05 §9'dadır; "KISMEN KAPANDI (a, c: F8-05)" ara durumu.

## (d) Doğrulayamadığım / varsayımla yazdığım noktalar

1. **F6-06 numarası:** F6 plan dosyası yok; EVAL-1v1 harness'ine "F6-06" denmesi kullanıcı notasyonudur, depoda doğrulanamadı.
2. **`OdbcConnection` paylaşımının IOCP thread'inden eşzamanlı güvenliği** ve senkron `UPDATE`'in IOCP bloklama süresi (F8-05 §0 madde 2): kodda `recursive_mutex` ve IOCP'den doğrudan `g_DBAgent` çağrıları var (`User.cpp:3599, 5888, 5912`) ama ölçülmedi.
3. **Botun Type9 skill'i kullanıp `USER_SAVED_MAGIC`'e buff yazıp yazmadığı** (`MagicInstance.cpp:2156` koşulsuz `InsertSavedMagic`): doğrulanmadı; `USER_*` tablosu okunmadı.
4. **Başlangıç noktası yürünebilirliği ve ızgara yeri:** ADR "merkezden ±35 m"; tam nokta/yön/boşluk `[V]` değil.
5. **`db/003`/`db/004`'ün gerçek DB'deki anlık durumu:** kayıtlara göre geri alındı; DB'ye bağlanmadım (kural), doğrulamadım.
6. **`Upgrade` kademesi** (S1 = 7 beklenir): DB'de doğrulanmadı.
7. **Oyun içi hiçbir şey çalıştırılmadı** (salt-okunur görev): tüm çalışma zamanı kabul kriterleri `BEKLİYOR` olarak taslaktadır; birim/derleme doğrulaması oyun içi doğrulamanın yerine geçmez.
8. **`TEST_TELEPORT` kaynağı** (A2) ve **`bot-telemetry-report.py`'nin yeni olaylarla davranışı:** araç bilinmeyen olayları genel sayar (`:10`) ve `TEST_TELEPORT`'u tanır (`:318`); yeni olaylarla çalıştırılmadı.
9. **`docs/STATUS.md` satır numaraları** her gece kayar; (c.2) yaklaşıktır (`7891f74`).
