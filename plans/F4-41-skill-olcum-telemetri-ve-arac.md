# F4-41: T-MECH-SKILL botla koşusu, dilim 1 — cast telemetrisine MP/skill alanları ve `tools/skill-check.py` hüküm aracı

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02, merge 0901ae0) |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; ADR-0018 m.9 "T-MECH-SKILL'in botla yeniden koşusu") |
| Branch | `bot/F4-41` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-03/F4-24..F4-37 (cast dilimleri, `SubmitCast`) — `KAPANDI`; F4-21 (`tools/bot-telemetry-report.py` kalıbı) — `KAPANDI`; F4-40 (envanter doldurma, koşu öncesi stok) — `KAPANDI` (merge `7cfef9c`) |
| İlgili gereksinim / kabul | `docs/05` §9 (T-MECH-SKILL-W/P/M-*: MP düşümü, recast, menzil sınırı, etki, fail sebebi), `docs/03` MEC-MAG-02/-08/-11, CLI-04; `docs/17` F4 "Teslimatlar: T-MECH-SKILL-* bot kanıtları"; ADR-0018 m.9 |
| Tahmini büyüklük | S–M (1 C++ dosyası küçük ekleme + 1 yeni Python aracı; birim testi aracın `--selftest`'indedir) |
| Hazırlayan / tarih | Claude / 2026-10-03 (otonom gece döngüsü) |

---

## 1. Amaç

ADR-0018 m.9, T-MECH-SKILL'in (her çekirdek skill için MP düşümü, recast, menzil, etki, fail sebebi) botla koşulmasını ister. Bugün cast telemetrisi bunu ölçmeye yetmez: `ACTION_SUBMIT`/`ACTION_RESULT` botun MP'sini taşımaz ve `FAIRNESS_REJECT` hangi skill için reddedildiğini söylemez. Bu plan (a) cast telemetrisine botun **kendi** MP'sini ve ret kaydına skill kimliğini ekler, (b) telemetri JSONL'ını `MAGIC` tablosuyla (skill verisi, kişisel veri değil) karşılaştırıp **skill başına hüküm** veren `tools/skill-check.py` aracını ekler. Koşu betikleri ve gerçek ölçüm turları sonraki plandır (F4-42); bu plan o koşunun **ölçüm altyapısını** verir. Bot sistemi kapalıyken (`[BOT] ENABLED=0`, varsayılan) ve telemetri kapalıyken sunucu davranışı değişmez.

## 2. Bağlam (okunması zorunlu)

- `docs/05` §9 ve `docs/03` MEC-MAG-02 (ReCastTime 0,1 sn birimi, gerçek zaman), MEC-MAG-08 (MP yetersiz), MEC-MAG-11 (menzil): aracın beklenen değerleri buradan.
- `docs/16` §3.2 satırları `ACTION_SUBMIT`/`ACTION_RESULT`/`FAIRNESS_REJECT` (`docs/` değişikliğini Claude yapar, bu plan `docs/`'a dokunmaz).
- `tools/bot-telemetry-report.py` (başlık/kullanım/`--selftest`/çıkış kodu kalıbı; yalnızca standart kütüphane) ve `tools/spell-model.py:120-136` (`run_query`: sqlcmd çağrısı ve `|` ayrımlı ayrıştırma; **kopyala**, `import` etme: araçlar bağımsız betiklerdir).
- İlgili kod (hepsini açıp doğrula; satırlar `7cfef9c` itibarıyla):
  - `GameServer/Bot/ActionExecutor.cpp:53-67` `EmitFairnessReject(s, user, decisionId, type, rule, reason, value, limit)`: ortak ret yayıncısı (Move/Attack/Cast/...). Çağrı yerleri: `grep -n EmitFairnessReject`.
  - `GameServer/Bot/ActionExecutor.cpp:534-577` `RejectCast`: `EmitFairnessReject(s, user, decisionId, "Cast", rule, reason, value, limit)` çağrısı (`:574`, dosyadaki tek `"Cast"` tipli çağrı), hemen ardından `ActionExecutor::EndCast(s)`; `s->m_castSkillId` o anda reddedilen skill'dir (`BeginCast`'te `:814` `s->m_castSkillId = skillId;`, `RejectCast` çağrılarından önce; uygulayıcı bunu açıp doğrulasın, değilse Açık sorular'a yazsın).
  - `GameServer/Bot/ActionExecutor.cpp:585-682` `SubmitCast`: `ACTION_SUBMIT` alan parçası `:593-601` (`decision_id`, `type` ∈ `CastStart`/`CastFly`/`CastEffect`, `skill`, `target`, `cycle`, …), `ACTION_RESULT` alan parçası `:660-669` (`decision_id`, `type`, `ok`, `reason` ∈ `casting`/`flying`/`effected`/`missed`/`srv_fail`/`no_result`, `op`, `code`, `[victims]`, `latency_us`). `user->HandlePacket(pkt)` çağrısı `:618-620`.
  - `GameServer/Bot/ActionExecutor.cpp:930` / `:936-938`: `BeginCast` zaten `user->GetMana()` (botun kendi MP'si) ve `m->sReCastTime`, `m->sMsp` okuyor; `GetMana` `tools/check-perception-contract.py:32-33` `R2_SYMBOLS`'ta **yoktur** (kısıtlı sembol değil).
  - `GameServer/MagicInstance.cpp:90,1029,1802`: sunucu MP'yi **bir kez** `pSkill->sMsp` kadar düşürür (non-Type4 ya da hedefsiz Type4: `:1029`; hedefli Type4: `:1802`).
  - `BotCore/BotCombat.h:125-139`: `kCastExtraMs = 80`, `CastDurationMs`, `CastRecastMs(reCastTime) = reCastTime * 100` ms.
  - `shared/packets.h:398-402`: `SKILLMAGIC_FAIL_CASTING -100`, `_KILLFLYING -101`, `_ENDCOMBO -102`, `_NOEFFECT -103`, `_ATTACKZERO -104` (`ACTION_RESULT.code` bu değerleri taşır).
  - Telemetri satırı biçimi: `GameServer/Bot/Telemetry.cpp:533-557` (`{"t":<ms>,"match":"...","bot":<id>,"name":"...","ev":"...","mode":"live",<alanlar>}`).
- `MAGIC` tablosu sütunları (sqlcmd ile doğrulandı): `MagicNum, EnName, Msp, CastTime, ReCastTime, Range, Type1, Type2, UseItem, Moral, ...`. **Yalnızca `MAGIC` okunur**; yasak tablolar (CLAUDE.md "Kişisel veri") okunmaz.

## 3. Kapsam

**Yapılacaklar**

1. `ActionExecutor.cpp`: yalnızca telemetri alanı ekle (davranış değişmez):
   - `ACTION_SUBMIT` (cast, `SubmitCast`): `"mp":<user->GetMana()>` (paket gönderilmeden **önceki** MP).
   - `ACTION_RESULT` (cast, `SubmitCast`): `"mp_after":<user->GetMana()>` (`HandlePacket` döndükten sonra; `latency_us`'tan önce).
   - `FAIRNESS_REJECT`: `EmitFairnessReject`'e sondan isteğe bağlı parametre `uint32 skillId = 0`; sıfırdan farklıysa `,"skill":<id>` eklenir. Yalnızca `RejectCast` çağrısı `s->m_castSkillId` geçirir; diğer çağıranlar **değişmez** (çıktıları bayt bayt aynı kalır).
2. `tools/skill-check.py` (yeni): telemetri JSONL + `MAGIC` verisi → skill başına tablo ve hüküm (§5.2). `--selftest` ile.

**Kapsam dışı (yapılmayacak)**

- Koşu betikleri (`bots/config/skill_*.txt`), senaryo YAML'ı, bot yerleştirme/konum sıfırlama, gerçek ölçüm turu ve sonuçlarını `docs/05`'e yazmak: **F4-42** (Claude/sonraki plan). Bu planın doğrulaması sentetik veriyle (`--selftest`) ve Claude'un çalışma zamanı sınamasıyla yapılır.
- `BotCore/*`, `Tests/BotCoreTests/*`, `ScenarioRunner.*`, `ScriptRunner.*`, `BotSession.*`, `BotManager.*`, `Telemetry.*`, `tools/bot-telemetry-report.py`, `tools/check-perception-contract.py` **değişmez**.
- Cast **kararına** veya guard kurallarına dokunmak, `mp`/`mp_after` değerini sonuç eşlemesinde okumak (yalnızca operatör/analiz içindir; `stock_after` ile aynı kural, `ActionExecutor.cpp:1314`): **yok**.
- Pot/Move/Attack ve diğer aksiyon tiplerinin telemetri alanları: **değişmez** (yalnızca cast `SubmitCast` ve `RejectCast`).
- DB'ye yazma, `GameServer.ini` anahtarı, yeni `docs/` dosyası, ADR (Claude yazar).
- Menzil sınırının **sunucu tarafında** ölçülmesi (guard'ın izin verdiğinden uzağa deneme gönderme): F4-42'nin betik/senaryo işidir; bu araç yalnızca guard `out_of_range` retlerini ve sunucu fail kodlarını sayar.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/ActionExecutor.cpp` | değiştir | yalnızca `EmitFairnessReject` isteğe bağlı parametresi, `RejectCast` çağrısı, `SubmitCast` içinde iki alan |
| `tools/skill-check.py` | yeni | araç + `--selftest` |

Dosya sayısı 2 (plan dosyası dahil 3). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz. `.vcxproj` değişmez (yeni C++ dosyası yok).

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-41 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 `ActionExecutor.cpp` ekleri

1. `EmitFairnessReject` imzasını `..., float value, float limit, uint32 skillId = 0)` yap; alan parçasının sonuna (`limit`'ten sonra) `if (skillId != 0) fields += ",\"skill\":" + std::to_string(skillId);`. Mevcut çağrılar (Move/Attack/Pot/Sit/...) argüman eklemeden derlenir ve çıktıları değişmez.
2. `RejectCast` içindeki `EmitFairnessReject(s, user, decisionId, "Cast", rule, reason, value, limit)` çağrısına son argüman olarak `s->m_castSkillId` ekle. Başka `"Cast"` tipli çağrı varsa (`grep -n '"Cast"'`) onu da aynı biçimde güncelle.
3. `SubmitCast` içinde `ACTION_SUBMIT` alan parçasına, `cycle` alanından sonra ve `cast_ms`/`since_*` bloğundan **önce**: `+ ",\"mp\":" + std::to_string((int)user->GetMana())`. `ACTION_RESULT` alan parçasına `code`'dan sonra, `victims`/`latency_us`'tan **önce**: `+ ",\"mp_after\":" + std::to_string((int)user->GetMana())`. (`GetMana()` `int32` döndürür; `mp_after` `HandlePacket` çağrısından **sonra** okunur, yani `:618-620` satırlarının altında; `ACTION_SUBMIT` içindeki `mp` ise paket gönderilmeden önce, mevcut `Emit` bloğundadır.)
4. Alan sırası, JSON anahtar adları ve sayı biçimi **bağlayıcıdır** (araç bunları okur): `mp`, `mp_after`, `skill` (hepsi tam sayı).

### 5.3 `tools/skill-check.py`

Başlık (docstring) ve kullanım metni `bot-telemetry-report.py` kalıbında. Yalnızca standart kütüphane (`argparse`, `json`, `os`, `statistics`, `subprocess`, `sys`, `tempfile`). Girdi UTF-8, çıktı UTF-8, satır sonu LF. Python kodu için sayısal sabitler bu belgedeki gibidir.

**Komut satırı**

```
python3 tools/skill-check.py PATH [PATH ...] [--magic FILE | --sqlcmd P --server S --db D]
    [--mp-tol N] [--ms-tol N] [--min-n N] [--json] [--out FILE] [--strict]
python3 tools/skill-check.py --selftest
```

- `PATH`: `.jsonl` dosyası ya da klasör (özyinelemeli `*.jsonl`). Bozuk satır (JSON değil / `ev` yok) atlanır ve sayılır (`skipped_lines`, raporda yazılır).
- `--magic FILE`: `MAGIC` verisini sqlcmd çıktısıyla aynı biçimde okur: satır başına `MagicNum|EnName|Msp|CastTime|ReCastTime|Range|Type1|Type2` (`|` ayrımlı, başlık satırı yok). Verilmezse sqlcmd çalıştırılır (`--sqlcmd` varsayılanı `spell-model.py:26` ile aynı, `--server` `.\SQLEXPRESS`, `--db` `FDP_kn_online`): `SELECT MagicNum, RTRIM(EnName), Msp, CastTime, ReCastTime, Range, Type1, Type2 FROM MAGIC` (yalnızca bu tablo). sqlcmd hatasında stderr'e yaz, çıkış kodu 2.
- `--mp-tol` (varsayılan `10`): MP düşüm bandı ± (öz MP yenilenmesi/oyalanması için `[A]`). `--ms-tol` (varsayılan `50`): recast alt sınır toleransı. `--min-n` (varsayılan `3`): hüküm için en az örnek.
- `--strict`: herhangi bir skill'de `FAIL` varsa çıkış kodu 1. Çıkış kodları: 0 başarılı, 1 `--strict` ve FAIL, 2 kullanım/girdi hatası. `--json`: tek JSON nesnesi, aksi hâlde Markdown. `--out FILE`: raporu dosyaya yaz.

**Cast kaydı birleştirme** (bot başına, dosya sırasıyla; bot anahtarı `bot` alanı, görünen ad `name`):

- Yalnızca `ev` ∈ {`ACTION_SUBMIT`, `ACTION_RESULT`, `FAIRNESS_REJECT`} ve `type` ∈ {`CastStart`, `CastFly`, `CastEffect`, `Cast`} olanlar işlenir; diğerleri sayılmadan atlanır. `SELFTEST` satırları yok sayılır.
- `ACTION_SUBMIT` `CastStart`: bot için açık kayıt yoksa yeni kayıt aç (`skill`, `t_start = t`, `mp_before = mp`); açık kayıt varsa onu `abandoned` olarak kapat ve yenisini aç. `CastFly`: kayda `flying = true`. `CastEffect`: açık kayıt yoksa (CastTime 0 olan skill'ler) yeni kayıt aç (`mp_before = mp`, `t_start = t`); her durumda `t_effect = t`, `effect_decision = decision_id`.
- `ACTION_RESULT`: `decision_id` ile `type` aynı bota ait son SUBMIT'e eşlenir. `CastStart`/`CastFly` sonucu `ok == false` ise kaydı `srv_fail` + `code` ile kapat. `CastEffect` sonucu kaydı kapatır: `outcome = reason` (`effected`/`missed`/`srv_fail`/`no_result`), `code`, `mp_after`.
- `FAIRNESS_REJECT` `type == "Cast"`: `skill` alanı varsa `(skill, reason)` ret sayacı artar (`reason` ∈ `no_mana`, `recast`, `type_gate`, `gap`, `rate`, `too_early`, `out_of_range`, `not_standing`); bota ait açık kayıt varsa `guard_reject` ile kapatılır. `skill` alanı yoksa (eski kayıt) `skill = 0` altında ("unknown") sayılır.
- Dosya sonunda açık kalan kayıt `open` sayılır (tabloya girmez, toplamda `open_records` yazılır).

**Skill başına tablo** (satır: `skill` kimliği; `name` = `MAGIC.EnName`, `MAGIC`'te yoksa `?`):

| Sütun | Anlam |
|---|---|
| `started` | kapanan kayıt sayısı (abandoned/open hariç) |
| `effected` / `missed` / `srv_fail` / `no_result` / `guard_reject` | sonuç dağılımı (`missed` = `code -104`; `srv_fail` kodları `code_hist` içinde, ör. `-103:2`) |
| `rejects` | `FAIRNESS_REJECT` sebep dağılımı (ör. `out_of_range:3 recast:1`) |
| `mp_exp` | `MAGIC.Msp` |
| `mp_delta_min/med/max` | `mp_before − mp_after`, yalnızca `effected` ve `missed` kayıtlarından ve iki alan da varsa; `statistics.median` |
| `mp_verdict` | `PASS` tüm delta'lar `[mp_exp − mp_tol, mp_exp + mp_tol]` içinde; `WARN` medyan bantta ama bazı delta'lar dışında; `FAIL` medyan bant dışında; `NO_DATA` örnek < `--min-n` |
| `recast_exp_ms` | `MAGIC.ReCastTime * 100` |
| `recast_min_gap_ms` | aynı bot + aynı skill için ardışık `effected` kayıtların `t_effect` farkının en küçüğü (ms) |
| `recast_verdict` | `PASS` en küçük fark ≥ `recast_exp_ms − ms-tol`; `FAIL` aksi; `NO_DATA` ardışık çift yok (veya `recast_exp_ms == 0` ise `n/a`) |
| `cast_ms_med` | `t_effect − t_start` medyanı (yalnızca `CastTime > 0` ve `effected`/`missed`; bilgi amaçlı, hükümsüz) |
| `effect_verdict` | `PASS` `effected ≥ 1` ve `srv_fail == 0`; `FAIL` `srv_fail ≥ 1` (kodlar listelenir); `NO_DATA` `effected + missed + srv_fail == 0` |

Genel hüküm: herhangi bir `*_verdict` `FAIL` ise skill `FAIL`; yoksa herhangi bir `WARN` ise `WARN`; hepsi `NO_DATA`/`n/a` ise `NO_DATA`; yoksa `PASS`. Rapor sonunda skill sayıları (`PASS/WARN/FAIL/NO_DATA`) ve `skipped_lines`, `open_records`, `abandoned_records`, `files` yazılır. Satırlar skill kimliğine göre sıralı (belirlenimli çıktı).

**`--selftest`** (geçici klasörde sentetik JSONL + bellek içi `MAGIC` satırları; DB/sunucu yok; hepsi geçerse `selftest: N checks, 0 failed`, çıkış 0). En az şu durumlar, her biri adlandırılmış kontrol:

1. `mp_ok`: Msp 40, delta 40 (3 kayıt) → `mp_verdict PASS`.
2. `mp_off`: Msp 40, delta 10 (3 kayıt) → `FAIL`; delta'lar 40,40,40,10 → `WARN`.
3. `mp_no_data`: 2 kayıt → `NO_DATA`.
4. `recast_ok` / `recast_fail`: ReCastTime 30 (3000 ms), farklar 3100 → PASS; 2900 (tol 50 dışında) → FAIL; tek kayıt → `NO_DATA`.
5. `chain_start_effect`: `CastStart` SUBMIT → `CastStart` RESULT `ok` → `CastEffect` SUBMIT → RESULT `effected` tek kayıt, `cast_ms_med` doğru.
6. `chain_no_cast_time`: yalnızca `CastEffect` SUBMIT/RESULT (CastTime 0) tek kayıt.
7. `flying_chain`: `CastStart` → `CastFly` → `CastEffect` tek kayıt.
8. `srv_fail_code`: `CastEffect` RESULT `srv_fail`, `code -103` → `effect_verdict FAIL`, `code_hist` `-103:1`.
9. `guard_reject_attribution`: `FAIRNESS_REJECT` `Cast` + `skill` → `rejects` doğru skill'e yazılır; `skill` alanı yok → "unknown" (`skill 0`).
10. `abandoned`: ikinci `CastStart` önceki açık kaydı `abandoned` yapar (sayaç).
11. `bad_lines`: bozuk JSON ve `ev`'siz satır `skipped_lines`'a sayılır, çökmez.
12. `unknown_skill`: `MAGIC`'te olmayan skill → `name "?"`, hükümler `NO_DATA`/MP beklenen `n/a`, çökmez.
13. `strict_exit`: `--strict` ile FAIL → 1, FAIL yok → 0 (alt çağrı ya da `main()` dönüş değeri).
14. `json_out`: `--json` çıktısı geçerli JSON ve skill satırlarını içerir.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/skill-check.py --selftest` çıkış 0 ve son satır `selftest: N checks, 0 failed` (N ≥ 14; §5.3'teki on dört durumun hepsi adlandırılmış kontrol).
- [ ] K2: `./tools/build.sh Release` hatasız biter (yeni uyarı yok); `./tools/build.sh Debug` da (varsa `AGENTS.md` yönergesi) hatasız.
- [ ] K3: `git diff --stat main...bot/F4-41` (taban `gece/2026-10-02`) yalnızca `GameServer/Bot/ActionExecutor.cpp`, `tools/skill-check.py` ve plan dosyasını gösterir; `ActionExecutor.cpp` farkı yalnızca §5.2'deki ekleri içerir (cast karar/guard/sonuç eşleme satırlarında değişiklik yok: `git diff` ile gözle doğrulanır, Uygulayıcı Raporu'nda belirtilir).
- [ ] K4: `grep -n 'mp_after\|"mp"' GameServer/Bot/ActionExecutor.cpp` iki ekleme yerini (SUBMIT `\"mp\"`, RESULT `\"mp_after\"`) gösterir; `grep -n 'm_castSkillId' GameServer/Bot/ActionExecutor.cpp | grep -i reject` ya da `RejectCast` içindeki `EmitFairnessReject` çağrısı `s->m_castSkillId` geçirir; `EmitFairnessReject`'in diğer çağrıları argüman almadan kalır.
- [ ] K5: `python3 tools/check-perception-contract.py` çıkış 0 (`GetMana` R2 sembolü değildir; yeni ihlal yok).
- [ ] K6: `./tools/run-tests.sh` mevcut tüm birim testleri geçer (BotCore değişmedi; sayıda düşüş yok, mevcut sayı `251 tests, 0 failed` ya da daha fazla).
- [ ] K7 (Claude, çalışma zamanı): bot sistemi açık, `[BOT] TELEMETRY = decisions`; mage botuyla bir tek hedefli Type3 skill'i en az 3 kez atınca `Logs/bots/` JSONL'ında `ACTION_SUBMIT` `mp` ve `ACTION_RESULT` `mp_after` görünür; `tools/skill-check.py` o dosya ve gerçek `MAGIC` ile çalışır, o skill için `mp_exp` `MAGIC.Msp` ve `mp_delta_med` (düşüm) tutarlı çıkar. Bot kapalıyken/telemetri kapalıyken çıktıda değişiklik yok.
- [ ] K8: Telemetri alanı eklemesi dışında davranış aynı: bot sistemi açık, aynı betikle (`bots/config/script_smoke_2bot.txt` ya da eşdeğer) `ACTION_RESULT.ok`/`reason` dağılımı F4-41 öncesiyle aynıdır (Claude, çalışma zamanı).

## 7. Doğrulama komutları

```bash
python3 tools/skill-check.py --selftest
./tools/build.sh Release
python3 tools/check-perception-contract.py
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-41
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları. `ActionExecutor.cpp`'in mevcut kodlaması ve satır sonu korunur (CRLF/ISO-8859 ise `grep -a`, tek satırlık düzenleme araçlarıyla bozma); `tools/skill-check.py` UTF-8, LF.
- Kod yorumları ve değişken adları İngilizce. Python aracında üçüncü taraf bağımlılık yok.
- `mp`/`mp_after` **yalnızca operatör ve analiz içindir**: botun karar yolu bu değerleri okumaz (algı sözleşmesi: botun kendi MP'si zaten `SelfState`'tedir, yeni bilgi kaynağı değildir). Başka bir oyuncunun MP'sine/durumuna erişen satır eklenmez.
- Kişisel veri: araç yalnızca `MAGIC` tablosunu okur. Telemetri dosyalarında kimlik doğrulama belirteci bulunmaz; rapora ham satır kopyalanmaz.
- `ACTION_RESULT`'ın `mp_after` değeri bot kendi MP yenilenmesi nedeniyle `Msp`'den birkaç puan sapabilir: bu yüzden bant (`--mp-tol`) vardır; toleransı kendi başına değiştirme, varsayılan `10` `[A]`.
- Bu planın çalışma zamanı ölçümü Claude'dadır (K7, K8); DeepSeek yalnızca kod, `--selftest`, derleme ve statik denetimi koşar.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-41` — `1b72056 [F4-41] Cast telemetrisine mp/mp_after/skill alanları ve skill-check.py hüküm aracı`; `Durum`/rapor commit'i bu maddenin ardından aynı branch'te.
- Değişen dosyalar ve neden:
  - `GameServer/Bot/ActionExecutor.cpp`: `EmitFairnessReject` imzasına sondan `uint32 skillId = 0` eklendi; sıfırdan farklıysa `,"skill":<id>` yazılır (diğer tüm çağrılar argüman eklemediği için çıktıları bayt bayt aynı). `RejectCast` çağrısı `s->m_castSkillId` geçirir. `SubmitCast` `ACTION_SUBMIT`'e `cycle`'dan sonra `,"mp":<user->GetMana()>`, `ACTION_RESULT`'a `code`'dan sonra `,"mp_after":<user->GetMana()>` ekler (ikincisi `HandlePacket` döndükten sonra okunur). Davranış değişmez.
  - `tools/skill-check.py`: yeni araç; telemetri JSONL + `MAGIC` (`--magic` dosyası ya da sqlcmd) → skill başına MP/recast/etki/ret hükmü; `--selftest` (25 kontrol) ve `--strict`/`--json`/`--out`. Yalnızca standart kütüphane, ASCII/LF.
  - `plans/F4-41-skill-olcum-telemetri-ve-arac.md`: `Durum` satırı ve bu rapor.
- Derleme sonucu (`tools/build.sh Release` son satırlar):
  ```
    All 14064 functions were compiled because no usable IPDB/IOBJ from previous compilation was found.
    Kodun üretilmesi tamamlandı
    proj-GameServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\GameServer.exe
  rc=0
  ```
  (Tek uyarı, önceden var olan `UpgradeHandler.cpp(634,862)` C4789; yeni uyarı yok.)
  `./tools/build.sh Debug` de `rc=0` bitti (`BotCoreTests.exe` + `GameServer.exe`), yeni uyarı yok.
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔ — `python3 tools/skill-check.py --selftest` çıkış 0, son satır `selftest: 25 checks, 0 failed` (14 adlı durumun tamamı + alt kontroller).
  - K2 ✔ — Release ve Debug `rc=0`, yeni uyarı yok.
  - K3 ✔ — `git diff --stat gece/2026-10-02..HEAD` yalnızca `GameServer/Bot/ActionExecutor.cpp` (+12/−4) ve `tools/skill-check.py` (yeni, 807 satır) gösterir; plan dosyası da aynı branch'te. `ActionExecutor.cpp` farkı yalnızca §5.2 ekleridir; cast karar/guard/eşleme mantığında satır değişmedi (`git diff` ile gözle doğrulandı).
  - K4 ✔ (grep notu aşağıda) — `mp_after` satır 671, `\"mp\"` satır 604, `RejectCast`'in `EmitFairnessReject(..., s->m_castSkillId)` çağrısı satır 579; diğer 16 çağrı argümansız (grep ile listelendi, değişmedi).
  - K5 ✔ — `python3 tools/check-perception-contract.py` `RESULT: PASS` (R1..R5 ihlal 0; `GetMana` R2 sembolü değil).
  - K6 ✔ — `./tools/run-tests.sh` → `251 tests, 0 failed`.
  - K7/K8 — Claude'un çalışma zamanı sınaması (bu planın kapsamı dışı; DeepSeek sunucu açmadı).
- Plandan sapmalar ve gerekçeleri:
  - K4 grep metni `grep -n 'mp_after\|"mp"'` `\"mp\"` alanını yakalamaz: C++'ta alan `+ ",\"mp\":"` biçiminde yazıldığından kaynak metinde `mp\"` geçer, düz `"mp"` dizisi geçmez (grep yalnızca 671 `mp_after` ve ilgisiz 1279 `"mp" : "hp"` satırını döndürür). Eklemenin kendisi doğru ve plandaki §5.2-3 ile birebir; sapma değil, K4 grep kalıbının kaba olmasıdır.
- Açık sorular:
  - Uçan skill'lerde MP FLYING + EFFECTING'te iki kez düştüğü için (MEC-MAG-12) `mp_delta` `MAGIC.Msp`'nin iki katı olur ve `mp_verdict` `FAIL` verir. Plan §5.3 formülü bilinçli olarak uçuşu özel-durumlamadı; F4-42 koşusunda uçan skill için `--mp-tol`/beklenti ayarı gerekirse karar Claude'a bırakıldı.
  - `mp`/`mp_after` yalnızca operatör/analiz içindir; bot karar yolu bu alanları okumaz (algı sözleşmesi korunur).

### Tur 2 — 2026-10-03 (Doğrulama Turu 1 düzeltmeleri)

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-41` (aynı branch); kod düzeltmesi commit'i ve `Durum`/rapor commit'i bu maddeden sonra.
- Değişen dosyalar ve neden:
  - `tools/skill-check.py`:
    - `load_magic_sql`: `subprocess.run(..., capture_output=True, text=True)` yerine `capture_output=True` (bayt) ve `stdout`/`stderr` için `decode("utf-8", errors="replace")`; hata mesajı da çözülen `stderr`'i kullanır. Ayrıştırma (`replace("\r", "").split("\n")` → `parse_magic_lines`) ve `parse_magic_lines`'in bozuk satır `InputError` davranışı aynı kaldı (Bulgu 1: gerçek `MAGIC` çıktısındaki UTF-8 olmayan bayt `UnicodeDecodeError` ile çökertiyordu).
    - `analyze` `FAIRNESS_REJECT` dalından ölü `if skill is None: skill = 0` iki satırı silindi (`as_int(..., 0)` zaten `None` döndürmez) (Bulgu 2).
    - `--selftest`'e `sqlcmd_non_utf8` adlı kontrol eklendi (15. durum): geçici klasörde `#!/bin/sh` + `printf '105660|sacrifice\250|180|0|250|67|3|0\n'` (bayt `0xA8`) yazdıran sahte sqlcmd betiği `chmod +x` ile çalıştırılır; `load_magic_sql(sahte, "s", "d")` sonucu `{105660: {"msp": 180, ...}}` içerir ve çökmez. Posix değilse (Windows) `sqlcmd_non_utf8_skipped` adıyla sayılan kontrol olarak atlanır. Toplam kontrol 25 → 26; son satır biçimi `selftest: N checks, 0 failed`.
  - `plans/F4-41-skill-olcum-telemetri-ve-arac.md`: `Durum` satırı (`UYGULANDI`) ve bu rapor.
- Derleme: Bu turda yalnızca Python aracı değişti (`ActionExecutor.cpp`'e dokunulmadı); C++ derlemesi gerekmedi/koşulmadı. Tur 1'in `./tools/build.sh Release`/`Debug` `rc=0` sonuçları geçerlidir.
- Doğrulama sonuçları (istenen 5. madde):
  - `python3 tools/skill-check.py --selftest` → `selftest: 26 checks, 0 failed`, çıkış 0.
  - `python3 tools/skill-check.py /mnt/c/dev/fdp/server/Logs/bots/2026-10-02/f422_noscript-7-4.jsonl --out /tmp/skillcheck_out.txt` (`--magic` **vermeden**) → çıkış 0, çökme yok, rapor yazıldı.
  - `load_magic_sql` doğrudan çağrıldığında gerçek `MAGIC`'ten **1839 satır** okundu; **137** `EnName` değerinde yer değiştirme karakteri (`U+FFFD`) var (UTF-8 olmayan baytlar çözüldü; ör. `105660` = `sacrifice`, `msp 180`). Yalnızca `MAGIC` tablosu okunur.
  - `python3 tools/skill-check.py` (PATH yok) → çıkış 2.
  - `python3 tools/check-perception-contract.py` → `RESULT: PASS`; `git status` yalnızca `tools/skill-check.py` değişti; `ActionExecutor.cpp` önceki halinde.
- Kriter öz-değerlendirme: K1 ✔ (26 kontrol); engelleyici Bulgu 1 giderildi; Bulgu 2 (ölü kod) giderildi; Bulgu 3/4 not (davranış/kapsam değişmedi). K7 sqlcmd yolu artık gerçek `MAGIC` ile çalışır (çalışma zamanı ölçümü Claude'da). K2/K6 C++ değişmediği için Tur 1'deki gibi geçerlidir.
- Plandan sapmalar: Sahte sqlcmd betiğinde örnekteki `\xa8` yerine POSIX octal `\250` kullanıldı; `#!/bin/sh` (dash) `\x` kaçışını yorumlamadığı için bayt `0xA8` bu yolla güvenilir üretilir (amaç aynı: UTF-8 olmayan bayt). Kapsam ve davranış değişmedi.
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DÜZELTME GEREKLİ
- İncelenen: `gece/2026-10-02...bot/F4-41` @ `3da0d20` (kod commit'i `1b72056`)
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ (araç kendi selftest'inde) | `python3 tools/skill-check.py --selftest` → `selftest: 25 checks, 0 failed`, çıkış 0; 14 adlı durumun hepsi var (`tools/skill-check.py:578-746`). Ancak sqlcmd yolu selftest'te hiç sınanmıyor (bkz. Bulgu 1). |
| K2 | ✔ | `./tools/build.sh Release` rc=0, `warning C` sayısı 0; `./tools/build.sh Debug` rc=0, uyarı 0. |
| K3 | ✔ | `git diff --stat gece/2026-10-02...bot/F4-41`: `ActionExecutor.cpp` (+11/−4), `tools/skill-check.py` (yeni, 807), plan dosyası. `ActionExecutor.cpp` farkı yalnızca §5.2 ekleri: imza `:54-55`, `skill` alanı `:66-68`, `RejectCast` çağrısı `:579`, `"mp"` `:604`, `"mp_after"` `:671`. Karar/guard/eşleme satırı değişmedi. |
| K4 | ✔ | `"mp"` eklemesi `:604` (`\"mp\"` biçiminde olduğundan düz `"mp"` grep'i yakalamaz; uygulayıcının notu doğru), `mp_after` `:671`, `RejectCast` `s->m_castSkillId` geçiriyor (`:579`; `m_castSkillId` `BeginCast`'te `:821`'de, `RejectCast` çağrılarından `:968/:1013/:1054-1055` önce atanıyor). Dosyadaki tek `"Cast"` tipli çağrı bu; diğer 15 `EmitFairnessReject` çağrısı argümansız (`:102,:202,:421,:1151,:1265,:1590,:1729,:1884,:2040,:2064,:2348,:2372,:2614,:2815,:2994,:3103`). |
| K5 | ✔ | `python3 tools/check-perception-contract.py` çıkış 0, `RESULT: PASS`. |
| K6 | ✔ | `./tools/run-tests.sh` → `251 tests, 0 failed`. |
| K7 | ertelendi | Çalışma zamanı + gerçek `MAGIC` ile araç çalıştırması: araç düzeltilene kadar yapılamaz (Bulgu 1). Düzeltme sonrası Tur 2'de koşulur. |
| K8 | ertelendi | Çalışma zamanı sınaması Tur 2'de (sunucu, bu turda açılmadı). |

Ek sınamalar (denetçi): `MAGIC` tablosundan alınan gerçek bir skill satırı (`--magic` dosyası) + gerçek telemetri biçiminde sentetik JSONL ile araç doğru hüküm üretti (`mp_exp 180`, `mp_delta_med 180`, `recast_min_gap_ms 25100 ≥ 25000-50`, PASS, `--strict` çıkış 0); var olmayan yol çıkış 2; PATH'siz çağrı çıkış 2. **Varsayılan sqlcmd yolu gerçek veride çöktü** (aşağıda).

- Bulgular (önem sırasıyla):
  1. **[Engelleyici] `tools/skill-check.py:119` — varsayılan (sqlcmd) yolu gerçek `MAGIC` verisinde `UnicodeDecodeError` ile çöküyor.** `subprocess.run(command, capture_output=True, text=True)` çıktıyı UTF-8 olarak çözer; `MAGIC` tablosunun bazı `EnName` değerlerinde UTF-8 olmayan bayt var (örn. bayt `0xa8`, çıktı konumu 41926). Sonuç: `--magic` verilmeden çalıştırılan her çağrı traceback ile çıkar, çıkış kodu 1 (planın "sqlcmd hatasında stderr'e yaz, çıkış kodu 2" kuralını ve `--strict` ile "FAIL" çıkış kodunu karıştırma riskini ihlal eder). K7 ("`tools/skill-check.py` o dosya ve gerçek `MAGIC` ile çalışır") tam bu yola dayanır. `--selftest` yalnızca `--magic` dosyasını denediği için hatayı yakalamadı. Denetçi doğruladı: baytları alıp `decode("utf-8", errors="replace")` ile çözünce 1839 satır sorunsuz ayrışıyor.
  2. [Not, engelleyici değil] `tools/skill-check.py:273-274` — `as_int(..., 0)` hiçbir zaman `None` döndürmez, `if skill is None` dalı ölü kod.
  3. [Not] `tools/skill-check.py:221-232` — `abandoned` kapatılan kaydın `pending` girdisi silinmiyor; ona ait geç gelen bir `ACTION_RESULT` kaydı `completed`'e `abandoned` yerine sonuçla ekleyebilir (nadir; şu an sayaçları yanıltmaz çünkü `abandoned` kayıt hiç `completed`'e girmez, ama geç sonuç gelirse girer). Düzeltme zorunlu değil.
  4. [Not] Uygulayıcının açık sorusu (uçan skill'lerde MP iki kez düşer, MEC-MAG-12, `mp_verdict` yanlış `FAIL` verebilir): plan §5.3 formülü bilerek özel-durumlamadı; karar F4-42'de (koşu betiği uçan skill seçtiğinde) verilecek. Bu planı engellemez; STATUS'a not düşüldü.
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
plans/F4-41-skill-olcum-telemetri-ve-arac.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
1. tools/skill-check.py `load_magic_sql` (satır ~112-125): `subprocess.run(..., capture_output=True, text=True)` yerine baytları al (`text=True` kaldır) ve `result.stdout.decode("utf-8", errors="replace")` ile çöz; `result.stderr` mesajını da `decode("utf-8", errors="replace")` ile çöz (hata mesajında kullanılıyor). Ayrıştırma (`replace("\r", "").split("\n")` -> `parse_magic_lines`) aynı kalır. Gerçek MAGIC çıktısında UTF-8 olmayan baytlar bulunur; araç çökmemeli, ilgili `EnName` değerinde yer değiştirme karakteri olabilir.
2. Aynı fonksiyonda `parse_magic_lines` içinde bozuk bir satır `InputError` atıyor; sqlcmd çıktısında beklenmeyen satır (ör. uyarı/boş) varsa araç çökmesin, main() `InputError`'ı zaten çıkış 2 ile yakalıyor: bu davranışı bozma.
3. `--selftest`'e yeni adlı kontrol ekle `sqlcmd_non_utf8`: `load_magic_sql`'in çağırdığı alt süreci taklit eden (sahte sqlcmd yürütülebilir betiği, ör. geçici klasörde `#!/bin/sh` ile `printf '105660|sacrifice\xa8|180|0|250|67|3|0\n'` yazdıran dosya; chmod +x) ile `load_magic_sql(sahte_yol, "s", "d")` çağır ve sonucun `{105660: ...}` içerdiğini ve çökmediğini doğrula. Sahte betik yürütülemiyorsa (Windows) kontrolü atla ama yine de sayılan kontrol olarak "skipped" adıyla say. Selftest toplam kontrol sayısı artar; son satır `selftest: N checks, 0 failed` biçimi aynı kalır.
4. Ölü kodu temizle: `analyze` içinde `FAIRNESS_REJECT` dalındaki `if skill is None: skill = 0` iki satırını sil (`as_int(..., 0)` zaten None döndürmez).
5. Doğrulama: `python3 tools/skill-check.py --selftest` (çıkış 0), `python3 tools/skill-check.py <herhangi bir .jsonl> ` (--magic VERMEDEN) gerçek sqlcmd ile çökmeden çalışsın ve MAGIC satırlarını okusun (yalnızca MAGIC tablosu), `python3 tools/skill-check.py` çıkışları: PATH yok -> 2. Yalnızca `tools/skill-check.py` ve bu plan dosyası değişir; `ActionExecutor.cpp`'e dokunma.
```

### Tur 2 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F4-41` @ `260d0d9` (Tur 2 kod commit'i `350dbc8`; `GameServer/` farkı `3da0d20`'den beri boş, yani Tur 1'de derlenen C++ değişmedi)
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `python3 tools/skill-check.py --selftest` → `selftest: 26 checks, 0 failed`, çıkış 0; yeni `sqlcmd_non_utf8` kontrolü sahte sqlcmd ile bayt `0xA8` üretiyor (`tools/skill-check.py:748-765`); 14 adlı durumun hepsi duruyor. |
| K2 | ✔ | Tur 1: Release/Debug rc=0, uyarı 0. Tur 2'de C++ değişmedi (`git diff 3da0d20..bot/F4-41 -- GameServer/` boş); Release `GameServer.exe` (`build/bin/x86-Release`, 08:24) kod commit'inden sonra derlenmiş ve çalışma zamanı sınamasında kullanıldı. |
| K3 | ✔ | Taban farkı: `ActionExecutor.cpp` (+11/−4), `tools/skill-check.py` (yeni), plan dosyası; ayrıca `docs/STATUS.md` ve `plans/README.md` (Claude'un Tur 1 doğrulama kayıtları). Tur 1'de `ActionExecutor.cpp` farkının yalnızca §5.2 ekleri olduğu doğrulandı; Tur 2'de değişmedi. |
| K4 | ✔ | Tur 1 kanıtı geçerli (`:579` `s->m_castSkillId`, `:604` `\"mp\"`, `:671` `mp_after`, diğer çağrılar argümansız). Çalışma zamanında üç alan da görüldü (K7). |
| K5 | ✔ | `python3 tools/check-perception-contract.py` → `RESULT: PASS`, çıkış 0. |
| K6 | ✔ | `./tools/run-tests.sh` → `251 tests, 0 failed`. |
| K7 | ✔ | Gece modu çalışma zamanı: `[BOT] ENABLED=1, MAX_BOTS=16, TELEMETRY=decisions`, Release sunucu, `spawn BotMF_K`, `spawn BotWP_E`, `cast BotMF_K 110518 BotWP_E 3` (Ignition, Type3 tek hedef, `MAGIC.Msp 60`, `ReCastTime 1`). `live-082610.jsonl`: `ACTION_SUBMIT` `CastStart`/`CastEffect` `"mp":6021` → `ACTION_RESULT` `CastEffect` `"mp_after":5961`, sonraki çevrimler 5961→5901→5841 (her atışta −60); alan sırası plandaki gibi; `SpeedCheck` satırlarında yeni alan yok. Reddedilen atış: `FAIRNESS_REJECT` `Cast` `MEC-MAG-11` `out_of_range` `value 60.90` `limit 56.00` `"skill":110518`. Araç **sqlcmd ile (`--magic` verilmeden)** bu dosyada: `110518 Ignition started 4, effected 3, guard_reject 1, rejects out_of_range:1, mp_exp 60, mp_delta_min/med/max 60, mp_verdict PASS, recast_exp_ms 100, recast_min_gap_ms 2185 PASS, cast_ms_med 1091, effect_verdict PASS`, genel `PASS`, `--strict` çıkış 0. |
| K8 | ✔ (sınırlı) | `script_smoke_2bot.txt` ile öncesi/sonrası karşılaştırması yapılmadı; bunun yerine kod farkı yalnızca telemetri alanı eklemesi ve çalışma zamanında cast zinciri F4-34 kayıtlarıyla aynı davrandı (`casting` → `effected`, `ok:true`, `op 1/3`, `code 0`; log `cast finished (effected) after 3 cycle(s), 3 ok, 6 packet(s) sent`; menzil dışında `cast stopped (out_of_range)`). Davranış değişikliği izi yok. |

Ek sınamalar (denetçi): sentetik JSONL + sqlcmd'den gerçek `MAGIC` (skill `105660`, `Msp 180`, `ReCastTime 250`): `mp_delta_med 180`, `recast_min_gap_ms 25100 ≥ 24950`, hepsi `PASS`, `--strict` çıkış 0; PATH'siz çağrı çıkış 2; olmayan yol çıkış 2; dosya ASCII/LF (`file`: ASCII text, CR sayısı 0). Çalışma ortamı geri alındı: sunucular kapatıldı (`status` 0/3), `GameServer.ini` yedekten bayt bayt geri yüklendi (`cmp` sıfır fark), botlar `despawn all` ile kaldırıldı; yeni dosya yalnızca `Logs/bots/2026-10-03/live-082610.jsonl` ve `Logs/Bot_3_10_2026.log` satırları.

- Tur 1 bulgularının durumu: Bulgu 1 (engelleyici, `UnicodeDecodeError`) giderildi: bayt çıktısı `decode("utf-8", errors="replace")` ile çözülüyor (`tools/skill-check.py:119-127`), varsayılan sqlcmd yolu gerçek `MAGIC` ile çalışıyor (1839 satır). Bulgu 2 (ölü kod) giderildi. Bulgu 3 (abandoned kaydın `pending` girdisi) not olarak açık, düzeltme zorunlu değil. Bulgu 4 (uçan skill) F4-42'ye devredildi.
- Bulgular (önem sırasıyla, hiçbiri engelleyici değil):
  1. [Not] `tools/skill-check.py:221-232` — `abandoned` kapatılan kaydın `pending` girdisi silinmiyor; geç gelen bir `ACTION_RESULT` kaydı yine de `completed`'e ekleyebilir (nadir; F4-42 koşusunda ikinci `CastStart` öncesi sonuç beklenmediği için beklenmiyor).
  2. [Not] Uçan skill'lerde MP FLYING + EFFECTING'te iki kez düşer (MEC-MAG-12): `mp_delta` `Msp`'nin iki katı olur, `mp_verdict` yanlış `FAIL` verir. F4-42'de uçan skill seçilirse `--mp-tol`/beklenti kararı verilecek (uygulayıcının açık sorusu; plan §5.3 bilerek özel-durumlamadı).
  3. [Not] K8 tam öncesi/sonrası betik karşılaştırması değil (yukarıda); telemetri-yalnız fark ve çalışma zamanı zinciri davranışı kanıt olarak yeterli sayıldı.
- Birleştirme: otonom gece modu (`AUTO_LOOP=1`, `AUTO_INTEGRATION_BRANCH=gece/2026-10-02`); birleştirmeyi ve push'u döngü betiği yapar, bu oturumda yapılmadı.
