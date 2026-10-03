# F8-06: Sunucu tarafı maç olayları — `MATCH_START` takım alanları, `DAMAGE`, `DEATH` (öldüren), `RESPAWN` ve `tools/bot-outcome-eval.py` şemasının gerçek log ile doğrulanması (docs/16 §3.2/§3.3, ADR-0031-DEG Ek F8-01)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F8 — Değerlendirme ve 8v8 (`docs/17` §2; kapı G8; F7 `EVAL-2v2..5v5` ve F6 `EVAL-1v1` için de ön koşul) |
| Branch | `bot/F8-06 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F3-01/F3-02/F3-03 `KAPANDI` (`Telemetry`, `BeginMatch/EndMatch`, `ScenarioRunner`), F8-01 `KAPANDI` (`tools/bot-outcome-eval.py`, girdi sözleşmesi: ADR-0031-DEG Ek F8-01). Bu plan F8-05 ve F8-07'den **önce** gelir (takım tanımı ve olay/sayaç altyapısı bunlara girdidir). F8-03/F8-04 gerekmez |
| İlgili gereksinim / kabul | `docs/16` §3.1-§3.3 (olay modeli), MET-OUT-01/02/05, `docs/16` §7 (geçersiz maç), ADR-0031-DEG ("Ortak tanımlar": kill, engage; "Ek F8-01" madde 2-4), ADR-0032-DEG, T-IGT-EVAL-01 (geçersiz maç ≤ %10), CON-03 |
| Tahmini büyüklük | L (15 dosya; HAZIR'dan önce bölme: §0 madde 6) |
| Hazırlayan / tarih | Claude / 2026-10-03 (taslak; referanslar `gece/2026-10-02` @ `7891f74`) |

---

## 0. Neden TASLAK (HAZIR yapma ön koşulları)

1. **Plan, F6/F7'den önce değil, F5-55 sonrası yazılabilir.** Sunucu emisyonu gerçek çarpışma gerektirir (bot–bot hasar/ölüm); botlar ancak saldırı ve hareket yürütücüsü (F4) ve hedefe ulaşma (F5-55 nav entegrasyonu veya betikli `attack`) ile çarpışabilir. Betikli `attack`/`cast` komutları bugün vardır (`ScriptRunner`), yani plan F5-55 beklemeden **betikli 2v2** ile doğrulanabilir; HAZIR yapmadan önce proje sahibi "F8 sunucu emisyonu F6/F7'den önce yapılsın mı" kararını verir (F6 `EVAL-1v1` bu planı gerektirir; `docs/17` F6 Test satırı).
2. **Karar A (MATCH_END `result` anlamı) F8-07'de verilir** (bu planda dokunulmaz); bu plan `MATCH_END`'e yalnızca sayaç alanları ekler (§3.2).
3. **Eval aracı `result == "aborted"` ister, ADR "teknik sonlanma ayrı alan" der** (çelişki, `F8-bagimlilik-ve-numaralandirma.md` §b satır "MATCH_END"): kararı F8-07'ye bırakmak için bu planın MATCH_END değişikliği yalnızca ek alanlardır.
4. **DAMAGE/DEATH/RESPAWN seviye eşlemesi (`docs/16` §3.3 satır 69):** bu olaylar `decisions` seviyesindedir; varsayılan `[BOT] TELEMETRY=summary` (`Telemetry.cpp` `Start` `ini.GetString("BOT","TELEMETRY","summary")`) altında **yazılmaz**. Değerlendirme koşuları `TELEMETRY=decisions` ister. Bu plan seviyeyi değiştirmez (docs/16 "kodda sabittir"); karar: seviye uyarısı (§3.3) ve F8-07'deki "skorlu koşu seviye denetimi".
5. **`TEST_TELEPORT` emisyonu bu planda yok:** botların maç içi ışınlama yolu bugün yoktur (`/bot` komutlarında teleport yok; `grep` boş `[D]`); GM'in elle ışınlaması bir sunucu olayıdır ve ayrı karardır (ADR-0032 madde 1: ışınlama maçı geçersiz kılar). Açık soru A2.
6. **Boyut:** 15 dosya (> ~10). HAZIR öncesi Claude şunlardan birini seçer: (a) tek plan, üç commit: **S1** `BotCore` saf sınıflandırma + test (4 dosya: `BotCore/MatchEvents.h`, `Tests/BotCoreTests/MatchEventsTests.cpp`, iki `vcxproj`), **S2** sunucu kancaları + `MatchEvents` sınıfı (6 dosya: `GameServer/Bot/MatchEvents.{h,cpp}`, `proj-GameServer.vcxproj/.filters`, `User.cpp`, `AttackHandler.cpp`), **S3** `ScenarioRunner`/`BotManager` takım anahtarları ve `MATCH_START`/`MATCH_END` alanları (4 dosya); (b) S1+S2 ve S3 iki plan. Taslak (a) varsayar.

## 1. Amaç

`tools/bot-outcome-eval.py` (F8-01) maç sonucunu `MATCH_START` takım listeleri, `DAMAGE` (ilk kayıt = engage), `DEATH` (ölen, öldüren), `RESPAWN`, `MATCH_END` olaylarından hesaplar; **sunucu bugün bunların çoğunu yazmıyor**. Bu plan eksik olayları ve alanları sunucuda üretir, öldürme sayaçlarını sunucu içinde de tutar (F8-07'nin kural motoru için) ve aracın şemasının gerçek bir koşuda üretilen `.jsonl` ile birebir uyumlu olduğunu komutla doğrular.

## 2. Bağlam (okunması zorunlu)

- `docs/16` §3.1 (ortak alanlar), §3.2 (`MATCH_START`/`MATCH_END` satır 43, `DAMAGE` satır 48, `DEATH/RESPAWN/RESURRECT` satır 55), §3.3 (satır 69 seviye eşlemesi; satır 75 `MATCH_START`/`MATCH_END` alanları; **satır 77 değerlendirici girdi sözleşmesi**).
- ADR-0031-DEG: "Ortak tanımlar" (kill = bot–bot PvP öldürme; canavar/kule/intihar/bilinmeyen sayılmaz; `engage` = ilk hasar; `engage_timeout_sec`) ve "Ek F8-01" madde 2-4 (alan adları; `killer` = öldüren birim kimliği, `-1`/yok = bilinmeyen/çevre; öncelik sırası).
- `tools/bot-outcome-eval.py` (okunacak işlevler): `parse_file` `:172-209` (her satır `ev:str`, `match:str` ve `"-"` atlanır, `t:int`), `kill_side` `:294-309` (`DEATH.bot` ölen int; `killer` int ve `-1` değil; takım listelerindeki kimlik eşleşmesi), `evaluate_match` `:476-583` (`MATCH_START` tam 1 adet; `team_a`/`team_b` boş olmayan, kesişmeyen **int listeleri**; ilk `DAMAGE` `t >= start_t` = engage; `MATCH_END.result == "aborted"` ⇒ ABORTED; `RESPAWN` `bot` int; `SETUP_FAIL`/`TEST_TELEPORT` varlığı), `tools/bot-outcome-eval/sample.jsonl` (21 satır: şemanın tek örnek kaynağı; `DAMAGE` örneği `bot` = ölen, `src`, `dst`, `amount`).
- Sunucu bugünkü emisyonu (okundu `[D]`):
  - `GameServer/Bot/Telemetry.cpp:298-380` `BeginMatch`: `MATCH_START` alanları `ts_utc`, `scenario`, `seed`, `run` + çağıranın `extraFields`'ı; `EmitControl` `bot = -1` (`:288`), `AppendLine` `:533-558` her satıra `t`, `match`, `bot`, (`name` yalnızca kontrol dışı), `ev`, `mode:"live"` yazar.
  - `GameServer/Bot/BotManager.cpp:974-990`: `match start` ek alanı yalnızca `composition` (**bot adları**, string listesi) ve `in_game`. `:1016-1050` `match end`: `result` serbest belirteç (`completed` varsayılan; `ScenarioRunner` `bot_lost` ve `aborted` de yollar: `ScenarioRunner.cpp:807, 818, 881`), ek alanlar `in_game`, `perf_samples`, `tick_p95_max_us`, `tick_max_us`; `Telemetry.cpp:399-450` ayrıca `ts_utc`, `duration_ms`, `dropped_soft`, `dropped_hard`.
  - Yayınlanan başka olay: `ACTION_SUBMIT`, `ACTION_RESULT`, `FAIRNESS_REJECT`, `CHAT_SENT`, `SCRIPT_START/STEP/END`, `PERF_SAMPLE`, `SELFTEST`. **`DAMAGE`, `DEATH`, `RESPAWN`, `TEST_TELEPORT`, `SETUP_FAIL` için `Emit` çağrısı yok** (`git grep` `"DAMAGE"|"DEATH"|"SETUP_FAIL"|team_a|win_rule` = 0 `[D]`).
- Kanca noktaları: `GameServer/User.cpp:1924` `CUser::HpChange(int amount, Unit * pAttacker, ...)` (HP hesaplandıktan sonra; mevcut `FDP_DAMAGE_TRACE` kancası `:2010-2012` aynı noktadır ve **IOCP işçi thread'i ya da zamanlayıcı thread'inde** çalışır: `DamageTrace.h:28`); `GameServer/User.cpp:4795` `CUser::OnDeath(Unit * pKiller)` (`m_bResHpType = USER_DEAD` `:4800`); `GameServer/AttackHandler.cpp:100` `CUser::Regene(...)`, doğuş tamamlanınca `RecastSavedMagic()` `:231` civarı. Bot ayırt etme: `m_botSink != nullptr` (`GameServer/User.h:582`, `User.cpp:31-58` kullanımı). `CUser::GetID()` = `GetSocketID()` (`User.h:114`): aracın `bot`/`killer` kimlikleri ile **aynı** uzay.
- `GameServer/Bot/ScenarioRunner.cpp:273-274` bilinen anahtar listesi, `:325-372` `bots` anahtarı (düz liste, **takım ayrımı yok**); `:703` `CommandSpawn`; `:756` `CommandMatch("start ...")` (sunucu `composition` yazar).

## 3. Kapsam

### 3.1 S1 — `BotCore/MatchEvents.h` (yeni, saf mantık) ve testleri

- `enum DeathClass { DEATH_KILL_A, DEATH_KILL_B, DEATH_NOT_KILL }` ve `ClassifyDeath(victimId, killerId, killerKind, teamA, teamB)`: yalnızca iki takımın üyeleri arasındaki öldürme `KILL`; `killer == -1`, canavar/kule/NPC (`killerKind != player`), `killer == victim` (intihar), aynı takım içi, takım listesinde olmayan kimlik `NOT_KILL`. ADR-0031-DEG "Ortak tanımlar" ve `bot-outcome-eval.py` `kill_side` ile **aynı** kural (iki gerçekleme birbirini denetler; ADR Ek F8-01 madde 1).
- `ClassifyDamage(attackerId, victimId, amount, teamA, teamB) -> bool engageEligible`: yalnızca iki farklı takımın botları arasındaki **gerçek hasar** (`amount < 0` yani HP düştü) `DAMAGE` sayılır; heal, kendi hasarı, canavar/kule hasarı, `pAttacker == nullptr` (aynalı/dolaylı hasar) sayılmaz. Gerekçe: araç `DAMAGE`'in **yalnızca `t`'sini** okur ve ilk kaydı engage sayar (`evaluate_match` `:530-534`); üçüncü taraf hasarı yanlış engage üretirdi.
- `Tests/BotCoreTests/MatchEventsTests.cpp` (yeni): her `DeathClass` için olumlu/olumsuz vaka (intihar, canavar öldürücü, kule, aynı takım, bilinmeyen kimlik, `killer = -1`); `ClassifyDamage` heal/aynı takım/NPC/null saldırgan vakaları; ADR-0031-DEG "Ortak tanımlar"dan türetilen örnekler.

### 3.2 S2 — sunucu: `GameServer/Bot/MatchEvents.{h,cpp}` ve kancalar

- `MatchEvents` (tekil, **thread-safe**: kancalar IOCP işçi/zamanlayıcı thread'inden gelir): `Begin(teamA, teamB)` / `End()` (IOCP thread'inden `ScenarioRunner`), `Active()` (atomik bayrak), `OnDamage(Unit * attacker, CUser * victim, int amount, int hpBefore, int hpAfter)`, `OnDeath(CUser * victim, Unit * killer)`, `OnRespawn(CUser * user, bool resurrect)`, `GetCounters()` (kilitli kopya: `killsA`, `killsB`, `deathsA`, `deathsB`, `aliveA`, `aliveB`, `engageMs` (MATCH_START'tan ilk geçerli hasara; yoksa -1), `respawns`). Takım listeleri `uint16` soket kimlikleri (`BotSession::m_pUser->GetSocketID()`).
- Kancalar (her biri ilk satırda `if (!MatchEvents::Active()) return;` ya da eşdeğeri **atomik okuma**: insan oyuncunun sıcak yoluna ek maliyet bir atomik yük olmalı; bot–bot dışı çağrıda hiç iş yapılmaz):
  - `CUser::HpChange`: HP hesaplandıktan sonra (`User.cpp:2010` civarı, mevcut `FDP_DAMAGE_TRACE` kancasının yanında, **onu değiştirmeden**): `MatchEvents::OnDamage(pAttacker, this, amount, oldHP, m_sHp)`.
  - `CUser::OnDeath`: fonksiyonun başında, `m_bResHpType` zaten `USER_DEAD` ise dönen erken çıkıştan **sonra** (çift sayım yok): `MatchEvents::OnDeath(this, pKiller)`.
  - `CUser::Regene`: doğuş tamamlanınca (`AttackHandler.cpp:231` civarı, `RecastSavedMagic()`'ten önce ya da sonra, tek nokta): `MatchEvents::OnRespawn(this, magicid != 0)`.
- Üretilen olaylar (`Telemetry::Emit`, `droppable = false`, seviye `TEL_DECISIONS`; `bot` = ilgili botun soket kimliği; alan adları `tools/bot-outcome-eval/sample.jsonl` ve ADR Ek F8-01 ile birebir):

| `ev` | Alanlar (eklenen) | Yazılma koşulu |
|---|---|---|
| `DAMAGE` | `src` (saldıran kimliği), `dst` (ölen/hasar alan kimliği), `amount` (pozitif tamsayı, düşen HP), `hp_before`, `hp_after`; `bot` = `dst`; `name` = hasar alan bot adı | `ClassifyDamage` doğru |
| `DEATH` | `killer` (öldüren kimliği; öldüren oyuncu değilse `-1`), `killer_kind` (`"player"`/`"npc"`/`"none"`); `bot` = ölen | her bot ölümü (aracın filtresi sayımı yapar; sunucu yalnızca `KILL` sınıfını sayaca yazar) |
| `RESPAWN` | `kind` (`"regene"`/`"resurrect"`); `bot` = doğan | her bot doğuşu |

  `DAMAGE` hacmi: her vuruş bir satırdır; kuyruğun sert sınırında (`Telemetry::HARD_LIMIT` 8192) düşebilir. Bu yüzden **kazanma sayımı telemetriden değil `MatchEvents` sayaçlarından** yapılır (F8-07) ve `MATCH_END`'e yazılan sayaçlar jsonl'den hesaplanan ile çapraz denetlenir (K7).
- `docs/16` §3.2'nin `DAMAGE` için saydığı `skill` alanı **yazılmaz** (skill bağlamı yalnızca `FDP_DAMAGE_TRACE` derlemesinde thread-yerel; `DamageTrace.h:11-23`); eksik alan listesine yazılır (`F8-bagimlilik-ve-numaralandirma.md` §b) ve araç onu okumaz.

### 3.3 S3 — `ScenarioRunner`/`BotManager`: takım tanımı ve `MATCH_START`/`MATCH_END` alanları

- **Senaryo anahtarları** (`ScenarioRunner.cpp:273-274` bilinen anahtar listesi; tekrar anahtarı hatası ve `ParseList` kalıbı korunur): `team_a`, `team_b` (bot adı listeleri; `bots` ile birlikte verilirse `team_a ∪ team_b == bots` ve kesişim boş olmalı, aksi halde `LoadScenario` hatası; yalnızca biri verilirse hata). **Yoksa ulusa göre türetilir `[A]`:** Karus botları `team_a`, El Morad botları `team_b` (çünkü aynı ulus birbirine saldıramaz; ulus bilgisi `BOT_TABLE` adındaki `_K`/`_E` sonekinden okunur, DB'den değil). Bir takım boşsa `LoadScenario` hatası.
- **`MATCH_START` ek alanları** (`match start` yoluna yeni `BotManager::BeginScenarioMatch(scenario, seed, extraFields)` ya da eşdeğeri; `ScenarioRunner` mevcut `CommandMatch` çağrısının yerine bunu çağırır, `m_matchPerfSamples` sıfırlama davranışı aynı kalır): `team_a` (soket kimlikleri, int listesi), `team_b`, `team_a_names`, `team_b_names` (okunabilirlik için), `duration_sec` (etkin değer). `composition` ve `in_game` korunur. `win_rule` ve eşik alanları F8-07'de eklenir; bu planda yazılmazsa araç `win_rule`'ü varsayılan `killdiff_timed` alır `[D]` (`bot-outcome-eval.py:227-235`): şema doğrulama koşusu bu yüzden `--win-rule timed_score` ile değerlendirilir ya da `win_rule` bu planda da `"timed_score"` olarak yazılır (**açık soru A3**; öneri: bu plan `win_rule: "timed_score"` yazar, F8-07 gerçek değeri yazar).
- **`MatchEvents::Begin/End`:** `match start` başarıyla açıldığında `Begin(teamA, teamB)`, `match end`ten önce `End()`. Maç yokken `Active()` yanlıştır: **kancalar hiçbir olay yazmaz**.
- **`MATCH_END` ek alanları:** `k_a`, `k_b` (kill sayıları), `deaths_a`, `deaths_b`, `engage_ms` (MATCH_START'tan ilk geçerli hasara; yoksa -1). `result`/`end_state` anlamı **değişmez** (F8-07).
- **Seviye uyarısı:** `match start` sırasında `Telemetry::IsEnabled(TEL_DECISIONS)` yanlışsa `Bot_*.log`'a bir satır: `match start: DAMAGE/DEATH/RESPAWN events are off at this telemetry level` (maçı reddetmez; reddetme F8-07'de skorlu `win_rule` için).

### 3.4 Şema doğrulaması (kabul kriteri; kalıcı araç **yazılmaz**)

Gerçek bir koşunun `.jsonl` dosyası ile `tools/bot-outcome-eval/sample.jsonl` arasında olay başına alan kümesi ve JSON türü karşılaştırması (aşağıda §7 komutu: geçici `python3 -` betiği, depoya eklenmez) ve aynı dosyanın `tools/bot-outcome-eval.py` ile değerlendirilmesi (`ERROR` satırı yok, rc 0).

**Kapsam dışı (yapılmayacak)**

- `win_rule`/eşik anahtarları ve kural motoru, `MATCH_END.result` hesabı, `RESPAWN_IN_WIPE_FIRST`, respawn sürücüsü (F8-07); `ScenarioReset`, `SETUP_FAIL` emisyonu, `team` tabanlı yerleşim (F8-05); `TEST_TELEPORT` emisyonu, `THIRD_PARTY` (ADR Ek F8-01 madde 4: şema yok, ertelendi), `HEAL`, `BUFF_*`, `TARGET_*`, `STATE_CHANGE`, `RESURRECT` ayrı olayı (`RESPAWN.kind` ile karşılanır).
- Telemetri seviye eşlemesini değiştirmek (`docs/16` §3.3); `Telemetry` sınıfına yeni genel API (mevcut `Emit`/`BeginMatch`/`EndMatch` yeterli); `shared/`, `AIServer/`, `LogInServer/`.
- `tools/bot-outcome-eval.py` değişikliği (araç oracle'dır; bu plan sunucuyu araca uydurur, tersini yapmaz. Çelişki çıkarsa **durup** Uygulayıcı Raporu'nda soru olarak yaz).
- Insan oyuncu hasarı/ölümü: kancalar yalnızca iki tarafı da bot olan olayları yazar.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/MatchEvents.h` | yeni | saf; ASCII, CRLF |
| `Tests/BotCoreTests/MatchEventsTests.cpp` | yeni | ASCII, CRLF |
| `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | birer `ClInclude`/`ClCompile` satırı |
| `GameServer/Bot/MatchEvents.h` / `.cpp` | yeni | ASCII, CRLF |
| `GameServer/proj-GameServer.vcxproj` / `.filters` | değiştir | iki yeni dosya |
| `GameServer/User.cpp` | değiştir | yalnızca iki kanca (`HpChange`, `OnDeath`); mevcut kodlama (BOM/CRLF) korunur |
| `GameServer/AttackHandler.cpp` | değiştir | yalnızca `Regene` kancası; ISO-8859 olabilir: `grep -a`, dönüştürme yok |
| `GameServer/Bot/ScenarioRunner.h` / `.cpp` | değiştir | `team_a`/`team_b`, `BeginScenarioMatch` çağrısı, `Begin/End` |
| `GameServer/Bot/BotManager.h` / `.cpp` | değiştir | `BeginScenarioMatch`/`EndScenarioMatch` (mevcut `CommandMatch` yolu bozulmaz) |

Listede olmayan dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F8-06 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. Sunucular kapalı. `./tools/run-tests.sh` ile taban test sayısını kaydet.
2. S1: `BotCore/MatchEvents.h` + testler + `vcxproj`; testler yeşil; Commit 1.
3. S2: `MatchEvents.{h,cpp}`, üç kanca, `vcxproj/.filters`; kanca ilk satırlarının atomik bayrak olduğunu doğrula (`ENABLED=0` ve maçsız yolda iş yok); Commit 2.
4. S3: `ScenarioRunner` anahtarları, `BotManager` sarmalayıcıları, `MATCH_START/END` alanları, seviye uyarısı; Commit 3.
5. `./tools/build.sh Release` (+ Debug); `./tools/run-tests.sh`.
6. Uygulayıcı Raporu; `Durum` → `UYGULANDI`. Çalışma zamanı kanıtı (§6 K6-K9) Claude'undur: sunucu açmak plan gereği yalnızca Claude'a bırakılmıştır.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` rc=0, yeni uyarı yok (Debug istenirse ayrıca); `./tools/run-tests.sh` `0 failed`; test sayısı = taban + yeni vakalar (ikisi rapora)
- [ ] K2: `git diff --stat gece/2026-10-02...bot/F8-06` yalnızca §4'teki dosyalar (+ plan); `User.cpp`/`AttackHandler.cpp` farkı yalnızca kanca satırları (diff satır sayısı rapora); `git diff --check` boş; `tools/bot-outcome-eval.py` farkı 0
- [ ] K3 (birim): `MatchEventsTests`: `ClassifyDeath` (kill A, kill B, intihar, NPC/kule, `-1`, aynı takım, takımda olmayan kimlik) ve `ClassifyDamage` (düşmana hasar, heal, aynı takım, NPC, saldıran `nullptr` karşılığı); vakalar ADR-0031-DEG "Ortak tanımlar" ve `bot-outcome-eval.py` `kill_side` ile aynı sonucu verir
- [ ] K4 (sıcak yol): kancaların ilk ifadesi atomik `Active()` denetimidir (`dosya:satır` kanıtı); maç yokken ve `[BOT] ENABLED=0` iken kanca hiçbir olay yazmaz ve `Telemetry::Emit` çağırmaz (kod okuma + runtime: `ENABLED=0` sunucuda `Logs/bots/` altında yeni dosya yok)
- [ ] K5 (alan sözleşmesi, **çalışma zamanı**; Claude): 2v2 betikli senaryo (ör. `BotWP_K`+`BotPHD_K` vs `BotWP_E`+`BotPHD_E`, `team_a`/`team_b` açık, `[BOT] TELEMETRY=decisions`, `duration_sec: 120`) çalıştırılır, üretilen `Logs/bots/<tarih>/<match>.jsonl` incelenir: `MATCH_START` `team_a`/`team_b` **int listeleri** ve her kimlik o koşuda `/bot list` ya da `snap` `sid` ile aynı; en az bir `DAMAGE` (alanlar `src`,`dst`,`amount`,`hp_before`,`hp_after`) ve en az bir `DEATH` (`killer` int, `killer_kind`) ve (ölüm olduysa ve doğuş istendiyse) `RESPAWN` satırı
- [ ] K6 (**şema karşılaştırması**, kabul kriteri): §7'deki `python3 -` betiği, gerçek `.jsonl` için `MATCH_START`, `DAMAGE`, `DEATH`, `RESPAWN`, `MATCH_END` olaylarının **her birinin alan kümesinin** `tools/bot-outcome-eval/sample.jsonl`'daki aynı olayın alan kümesini kapsadığını ve her ortak alanın JSON türünün (int/str/list-of-int) aynı olduğunu gösterir (istisna: `MATCH_START`'ta F8-07'nin sahip olduğu `win_rule`, `win_margin`, `early_end_margin`, `engage_timeout_sec` betikte atlanır); betik rc 0 ve çıktı satırı `SCHEMA OK <ev>` beş olay için; fazla alan serbest (araç yok sayar). `python3 tools/bot-outcome-eval.py <aynı .jsonl> --win-rule timed_score` rc 0 ve `OUTCOME match=... ERROR` **yok**. Sayaç eşitliği: aracın `k_a/k_b` değeri `MATCH_END.k_a/k_b` ile **eşit** olmalıdır; bunun için aracı `--duration-sec <(MATCH_END.duration_ms - engage_ms)/1000 - 2>` ile çalıştır (süre penceresi gözlem içine sığsın; aksi halde araç `TRUNCATED` ile `invalid` döndürür ve `k_a` boş kalır: `ScenarioRunner` süreyi maç açılışından sayar, ADR-0031-DEG engage'den; fark F8-07'de kapanır)
- [ ] K7 (sayaç çapraz denetimi): K5 koşusunda jsonl'den sayılan `DEATH` (`kill_side` kuralıyla) = `MATCH_END.k_a + k_b`; `MATCH_END.dropped_hard == 0` (yoksa kayıp olay var: koşu geçersiz, rapora yaz)
- [ ] K8 (olumsuz kontroller): (a) canavar/kule kaynaklı ölüm (varsa, T-ENV-ARENA-03 benzeri) `DEATH.killer = -1`/`killer_kind` `npc`/`none` ve `k_*` sayacına girmez; (b) maç dışında (`match` = `"-"`) bot hasarı `DAMAGE` yazmaz; (c) `TELEMETRY=summary` iken `DAMAGE/DEATH/RESPAWN` satırı yok ve `Bot_*.log` seviye uyarısı var
- [ ] K9 (geriye uyumluluk): `team_a`/`team_b`/`reset` yok bir eski senaryo dosyası (F3-03 örneği) aynı davranışla çalışır (`MATCH_START` yalnızca ek alanlar farklı; araç `team_a` türetilmiş ulus kuralıyla dolu görür); `[BOT] ENABLED=0` davranışı değişmedi
- [ ] K10: Uygulayıcı Raporu dürüst: çalıştırılmayan çalışma zamanı kriterleri "yapılmadı" yazılmış; `ISO-8859` dosyalarda kodlama korunmuş (`file` çıktısı, `git diff --stat` satır sayısı yalnızca kanca)

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/run-tests.sh 2>&1 | tail -3
git diff --stat gece/2026-10-02...bot/F8-06
git diff --check gece/2026-10-02...bot/F8-06
git diff gece/2026-10-02...bot/F8-06 -- GameServer/User.cpp GameServer/AttackHandler.cpp | grep -c '^[+-][^+-]'
# Claude, calisma zamani: sunucu acik, [BOT] ENABLED=1, TELEMETRY=decisions
#   /bot scenario run f8-06-schema-2v2        (team_a/team_b acik, betikli attack/cast)
M=Logs/bots/<tarih>/<match>.jsonl
python3 tools/bot-outcome-eval.py $M --win-rule timed_score; echo rc=$?
python3 tools/bot-outcome-eval.py $M --win-rule timed_score --duration-sec <N> --json | python3 -m json.tool | head -30   # <N>: K6 formulu
python3 - "$M" <<'EOF'
# Gecici (depoya eklenmez): olay basina alan kumesi + JSON turu karsilastirmasi
import json, sys
def load(p): return [json.loads(l) for l in open(p, encoding="utf-8") if l.strip()]
def kind(v):
    if isinstance(v, bool): return "bool"
    if isinstance(v, int): return "int"
    if isinstance(v, str): return "str"
    if isinstance(v, list): return "list-int" if v and all(isinstance(x, int) for x in v) else "list"
    return type(v).__name__
ref, live = load("tools/bot-outcome-eval/sample.jsonl"), load(sys.argv[1])
bad = 0
skip = {"win_rule", "win_margin", "early_end_margin", "engage_timeout_sec"}   # owned by F8-07
for ev in ("MATCH_START", "DAMAGE", "DEATH", "RESPAWN", "MATCH_END"):
    r = next((x for x in ref if x["ev"] == ev), None); l = next((x for x in live if x["ev"] == ev), None)
    if r is None or l is None: print("MISSING", ev); bad += 1; continue
    r = {k: v for k, v in r.items() if not (ev == "MATCH_START" and k in skip)}
    miss = [k for k in r if k not in l]; typ = [k for k in r if k in l and kind(r[k]) != kind(l[k])]
    print("SCHEMA OK" if not miss and not typ else "SCHEMA FAIL", ev, miss, typ); bad += bool(miss or typ)
sys.exit(1 if bad else 0)
EOF
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları (yeni dosyalar ASCII + CRLF). `User.cpp` UTF-8 BOM'lu olabilir, `AttackHandler.cpp` ISO-8859: dönüştürme/BOM değişikliği **yasak**, düzenlemeden önce `file` çıktısını kaydet; `grep -a` kullan.
- **Thread kuralı (`AGENTS.md` §2.6):** `HpChange`/`OnDeath` IOCP işçi ve zamanlayıcı thread'lerinden gelir: `MatchEvents` iç durumu mutex ile korunur; kancalar `CUser` durumunu **değiştirmez** (yalnızca okur); `Telemetry::Emit` zaten thread-safe. IOCP tick bütçesi (MET-PERF-02): kancaların toplam maliyeti ölçülür (`PERF_SAMPLE` `tick_p95_us` karşılaştırması rapora).
- **Oyun mekaniği değişmez** (`[MECH]` yok): kancalar gözlemcidir; hasar/ölüm/doğuş sonuçları aynı kalır.
- **Kişisel veri:** olaylara yalnızca bot adı yazılır; `name` insan oyuncuya ait olamaz (kanca iki tarafı da `m_botSink != nullptr` olan olaya sınırlıdır).
- **Telemetri düşmesi:** `DAMAGE` yüksek hacimlidir; sert sınırda düşebilir. Kazanma kararı sayaçlardan alınır (F8-07), jsonl yalnızca kanıt/oracle içindir.
- `[BOT] ENABLED=0` davranışı ve mevcut olay biçimleri (`ACTION_*`, `PERF_SAMPLE` vb.) değişmez.
- Açık sorular: **A2** `TEST_TELEPORT`'u GM ışınlama komutundan emit etmek (sunucu kodu: `ChatHandler`/`CUser::Warp` yolu) bu plana mı, ayrı plana mı? **A3** `win_rule` MATCH_START alanını F8-06 mı yazsın (`timed_score` yer tutucu) yoksa yalnızca F8-07 mi? (öneri: yer tutucu yazılsın, araç varsayılan `killdiff_timed`'a düşmesin).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F8-06` — `<kısa-sha> [F8-06] …`
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
- İncelenen: `gece/2026-10-02...bot/F8-06` @ `<sha>`
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
