# F5-72: Betik fiili `goto` — izinli komut sözlüğü 20 → 21 (`BotCore/ScriptPlan.h`, `ScriptTests.cpp`, `tools/skill-script-gen.py`)

| Alan | Değer |
|---|---|
| Durum | DOĞRULANDI |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-72 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `KAPANDI`: F4-19 (`BotCore/ScriptPlan.h`, `ParseScript`), F4-20 (`ScriptRunner`: her adım `BotManager::ExecuteCommand` yolundan), F4-42 (`tools/skill-script-gen.py`), F5-70 (`/bot goto`, merge `99d7170`). **F5-73'e (`NavDrive` Follow kipi; artık `gece/2026-10-02` dalında birleşti, merge `bc3170d`) bağımlı DEĞİLDİR** ve onunla dosya paylaşmaz: F5-73 yalnızca `BotCore/NavDrive.h`, `BotCore/NavTrack.h`, `NavDriveTests.cpp`, `NavTrackTests.cpp`; bu plan `ScriptPlan.h`, `ScriptTests.cpp`, `tools/skill-script-gen.py` |
| İlgili gereksinim / kabul | ADR-0017 Ek F4-19 madde 2 (izinli komut sözlüğü, 20 verb; **bu planın genişletmesi ADR-0017 Ek F5-72 olarak doğrulamada yazılır**); F5-70 plan §0 (betik fiili bu plana ayrıldı); `docs/17` F4 "betikli test senaryoları" ve F5-66 (çalışma zamanı doğrulama koşusu betikle `goto` verebilsin) |
| Tahmini büyüklük | S (3 dosya; saf mantık + Python aracı; `GameServer` kaynağı değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu, ön-plan; referanslar `gece/2026-10-02-preplan` @ `fa24182` üzerinde doğrulandı; `f636c17` üzerinde yeniden doğrulandı, bkz. Tazeleme notu) |

---

## 1. Amaç

Test betikleri (`./Scripts/<ad>.txt`, `/bot script run <ad>`) artık `goto <bot> <x> <z> [hız]` adımı içerebilir: `BotCore::IsScriptVerb("goto")` doğru döner, `ParseScript` bu adımı kabul eder, betik üretecinin (`tools/skill-script-gen.py`) `raw` satırı ve `--check` doğrulayıcısı aynı sözlüğü kullanır. `ScriptRunner` değişmez: adımı zaten `BotManager::ExecuteCommand`'a verir ve `goto` dağıtımı orada hazırdır (F5-70). Bot sistemi kapalıyken (varsayılan) ve `NAV=0` iken hiçbir sunucu davranışı değişmez.

## 2. Bağlam (okunması zorunlu)

Satır numaraları `gece/2026-10-02` @ `f636c17` üzerinde yeniden doğrulandı (ilk yazım `fa24182`). Sapma görürsen **dur** (§5 adım 1).

- `BotCore/ScriptPlan.h` (yalnızca standart kütüphane; son değişiklik `59662a7` F4-19): sözlük yorumu `:59` ("the 20 verbs"), `IsScriptVerb` `:60`, `kVerbs[]` dizisi `:62-67` (`"move", "stop", "attack", "cast", "pot", "sit", "stand", "target", "regene", "pinvite", "paccept", "pdecline", "pleave", "ppromote", "pkick", "pchat", "see", "npcs", "snap", "list"`), karşılaştırma küçük harfle yapılır (`:69-84`), `ParseScript` yalnızca ilk sözcüğü (`verb`) sözlükle denetler `:224-230` — argüman doğrulaması **yoktur** (argümanı çalışma zamanında `CommandGoto` doğrular).
- `Tests/BotCoreTests/ScriptTests.cpp`: `Script_VerbWhitelist` `:44-73` (`verbs[]` dizisi `:46-51`, `for (int i = 0; i < 20; ++i)` `:53`, olumsuz örnekler `:60-69`). Dosya CRLF + ASCII. Dosyada altı `TEST_CASE` var (`Script_ParseValid` `:7` … `Script_ErrorLineNumbers` `:208`). `BotCoreTests.vcxproj:101` dosyayı zaten içerir.
- `GameServer/Bot/BotManager.cpp:629-630` `goto` → `CommandGoto` dağıtımı (F5-70); `GameServer/Bot/ScriptRunner.cpp:206-214` her adımı `m_mgr.ExecuteCommand(command)` ile çalıştırır ve `SCRIPT_STEP` telemetrisine `verb` alanını yazar (fiil sözlüğüne bağımlı kod yok). **Bu iki dosyaya dokunulmaz.**
- `tools/skill-script-gen.py` (LF + ASCII): `VERBS` demeti `:73-79` (yorum `:73` "The 20 verbs a script may run"), `VERB_SET` `:79`; `raw` satırı sözlükte olmayan fiili reddeder `:214-222`; `check_bytes` aynı kümeyle `bad verb` der `:379-380`; selftest `# 16.` bloğu `:538-549`, `check("check_bad_verb", …)` `:548-549` (`0 teleport X`); `python3 tools/skill-script-gen.py --selftest` bugün `selftest: 23 checks, 0 failed` basar `[V]`.
- Sözlüğün başka kopyası yoktur: `git grep -n -a -E '"pkick"' -- . ':!docs' ':!plans'` yalnızca `ScriptPlan.h:65`, `ScriptTests.cpp:49`, `skill-script-gen.py:76` (üç kopya) ile `BotManager.cpp:657`/`:2221` (komut dağıtımı ve bir günlük metni; sözlük değildir) verir `[V]`; başka kopya çıkarsa **dur**.
- Test sayısı tabanda: F5-73 birleştiği için şu an `314` (`git grep -h -c '^TEST_CASE' -- Tests/BotCoreTests` toplamı `[V]`, `f636c17`); bu plan sonrası `315` beklenir. **Taban sayıyı başlangıçta kendin ölç** ve K2'yi ona göre oku (bu plan **+1** test ekler).

### Karar kaydı (otonom döngüde Claude kararı — gözden geçirilmeli; yeni ADR yok: ADR-0017 Ek F4-19 madde 2'nin uygulaması, **ADR-0017 Ek F5-72 doğrulamada yazılır**)

**D1 — Yalnızca `goto`.** Sözlüğe tek fiil eklenir; `follow` **eklenmez** (F5-63 sunucu bağlaması olmadan çalışmaz; ayrı küçük dilim, F5-63 sonrası). Yasak liste (`spawn despawn match scenario script`) aynen kalır.

**D2 — Sözlük sırası.** `"goto"`, `"move"`'dan hemen sonra eklenir (aynı komut ailesi; sıra davranışı etkilemez).

**D3 — Argüman doğrulaması ayrıştırıcıda yapılmaz.** `ParseScript` mevcut sözleşmesini korur (yalnızca ofset + fiil); `goto` argümanları (`<bot> <x> <z> [hız]`) `CommandGoto`'da denetlenir. Ayrıştırıcıya fiil-bazlı argüman denetimi eklemek ADR-0017 Ek F4-19 madde 5'i değiştirir; bu plan yapmaz.

**D4 — `goto` adımı eşzamansızdır.** `ExecuteCommand("goto …")` rotayı planlar ve yürüyüşü başlatıp döner; varış sonradan olur. Betik yazarı sonraki adımlarını (ör. `snap`, `attack`) yürüme süresinden sonraya koymalıdır (F5-70 doğrulaması `[V]`: Karus doğuş → arena A rota 275,0 m ≈ 60 sn, El Morad → arena A 700,2 m ≈ 152 sn, 4,5 m/s). Bunu `docs/` kısmına Claude yazar (kapsam dışı).

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/ScriptPlan.h`: `kVerbs[]` 20 → 21 eleman (`"goto"` eklenir), yorum "20 verbs" → "21 verbs".
2. `Tests/BotCoreTests/ScriptTests.cpp`: `Script_VerbWhitelist` 21 fiile güncellenir; yeni `Script_GotoStep` testi (+1 test).
3. `tools/skill-script-gen.py`: `VERBS` demetine `"goto"`, yorum "21 verbs"; selftest'e iki kontrol.

**Kapsam dışı (yapılmayacak)**

- `GameServer/`, `AIServer/`, `shared/`, `*.vcxproj*`, `docs/`: dokunulmaz (ADR-0017 Ek F5-72 ve betik biçimi notunu Claude yazar). `BotManager.cpp`, `ScriptRunner.*`, `ActionExecutor.*`: **değişmez**.
- Fiil `follow` (F5-63 sonrası ayrı dilim), `goto` argüman doğrulaması (D3), betik adımının sonucunu bekleme/okuma (ADR-0017 Ek F4-19 madde 5: yok), örnek betik dosyası (`bots/config/script_*.txt`), `skill-script-gen.py` için yeni spec türü (`goto` yalnızca `raw` ile gider; yeni `kind` eklenmez).
- Sınırlar (`kScriptMax*`) ve hata sıralaması değişmez; mevcut diğer testler değişmez.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/ScriptPlan.h` | değiştir | yalnızca `kVerbs[]` ve sözlük yorumu |
| `Tests/BotCoreTests/ScriptTests.cpp` | değiştir | `Script_VerbWhitelist` güncellenir; sona/yakına `Script_GotoStep` |
| `tools/skill-script-gen.py` | değiştir | `VERBS` demeti, yorum, iki selftest kontrolü |

`ScriptPlan.h` ve `ScriptTests.cpp`: ASCII + CRLF (`file` ile doğrula). `skill-script-gen.py`: ASCII + LF (mevcut biçim korunur). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-72 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. `./tools/run-servers.sh status` `[UP]` ise `./tools/run-servers.sh stop`. §2'deki referansları aç ve doğrula (özellikle `ScriptPlan.h:62-67`, `ScriptTests.cpp:44-73`, `skill-script-gen.py:73-79`); sapma varsa **dur**. Başlangıç ölçümleri: `./tools/run-tests.sh Release 2>&1 | grep -E "tests,|failed"` (taban sayıyı rapora yaz); `python3 tools/skill-script-gen.py --selftest` (`23 checks, 0 failed` beklenir); `git grep -n -a -E '"pkick"' -- . ':!docs' ':!plans'` çıktısını rapora yaz.
2. **`BotCore/ScriptPlan.h`:** `kVerbs[]` ilk satırı `"move", "goto", "stop", "attack", "cast", "pot", "sit", "stand", "target", "regene",` olur (diğer satırlar aynen); `:59` yorumu `Case-insensitive match against the 21 verbs a script may run (plan section 5.2; "goto" added by F5-72).` olur. Başka satır değişmez (CRLF korunur).
3. **`Tests/BotCoreTests/ScriptTests.cpp`:**
   - `Script_VerbWhitelist`: `verbs[]` dizisine `"goto"` (`"move"`'dan sonra), döngü sınırı `21`; olumsuz örneklere `CHECK(!BotCore::IsScriptVerb("gotox"));`, `CHECK(!BotCore::IsScriptVerb("go"));`, `CHECK(!BotCore::IsScriptVerb("follow"));` **eklenmez** (D1: `follow` sonraki dilimin testi olur; bu test onu kırmamalı); olumlu örneğe `CHECK(BotCore::IsScriptVerb("GOTO"));` eklenir.
   - Yeni `TEST_CASE("Script_GotoStep")` (`Script_VerbWhitelist`'ten hemen sonra):
     - `"0 goto BotWP_K 1274 890\r\n1500   GOTO BotWP_K 1294 934 45\n 3000\tgoto BotWP_K -5 10  \n"` → `error == SCRIPT_OK`, 3 adım; komut metinleri sırasıyla `"goto BotWP_K 1274 890"`, `"GOTO BotWP_K 1294 934 45"`, `"goto BotWP_K -5 10"` (iç boşluk korunur, uç boşluk kırpılır; D3: negatif sayı ayrıştırıcıda reddedilmez); `offsetMs` 0/1500/3000; `line` 1/2/3.
     - `"0 goto"` → `SCRIPT_OK`, 1 adım, `command == "goto"` (argüman doğrulaması ayrıştırıcıda yok, D3).
     - `"0 gotoo BotWP_K 1 2"` → `SCRIPT_ERR_BAD_VERB`, `errorLine == 1`; `"0 move BotWP_K 1 2\r\n10 goto2 BotWP_K 1 2"` → `SCRIPT_ERR_BAD_VERB`, `errorLine == 2` (ya-hep-ya-hiç: `steps` boş).
     - `kScriptMaxSteps` etkileşimi: 100 `goto` satırı → `SCRIPT_OK`, 100 adım; 101. satır `SCRIPT_ERR_TOO_MANY_STEPS` (mevcut `Script_Limits` kalıbıyla; yeni sınır yok).
4. **`tools/skill-script-gen.py`:** `:73` yorumu `# The 21 verbs a script may run (BotCore/ScriptPlan.h: IsScriptVerb).`; `VERBS` demetinin ilk satırı `"move", "goto", "stop", ...` olur (başka satır değişmez). Selftest'te `# 16.` bloğunun sonuna (`:548-549` `check_bad_verb` satırından hemen sonra, `# 17.`'den (`:551`) önce):
   - `ok, message = check_bytes(b"0 goto BotWP_K 1274 890\n")` → `check("check_ok_goto", ok and message.startswith("ok: 1 steps"))`;
   - `o = offsets(gen("raw 0 goto BotWP_K 1274 890\ncast B 1000 self 1\n", magic))` → `check("raw_goto_passthrough", o[0] == (0, "goto BotWP_K 1274 890"))` (ham adım `t0` öncesi sırada kalır; `raw_sets_t0` kalıbı).
   - Sonuç: `selftest: 25 checks, 0 failed`.
5. **Derle ve sına:** `./tools/build.sh Release` ve `Debug` (GameServer çözümü; `ScriptPlan.h`'yi `ScriptRunner.cpp` dahil eder, yeni uyarı olmamalı); `./tools/run-tests.sh Release` ve `Debug`; `python3 tools/skill-script-gen.py --selftest`.
6. Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; `BotCore/ScriptPlan.h` `touch` edilip yeniden derlenince **yeni uyarı yok** (`grep -c "warning"` sayısı rapora; eski `UpgradeHandler.cpp` C4789 dışında artış yok)
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; sayı = tabandaki sayı **+ 1** (F5-73 birleşti: `314 + 1 = 315`; rapora başlangıç ve son sayı); `Script_VerbWhitelist` ve `Script_GotoStep` `[ OK ]`; diğer `Script_*` testleri değişmeden geçer
- [ ] K3: `python3 tools/skill-script-gen.py --selftest` → `selftest: 25 checks, 0 failed`; `check_ok_goto` ve `raw_goto_passthrough` çıktıda/`FAILED` listesinde değil; `python3 tools/skill-script-gen.py --check` bir geçici dosyayla (`printf '0 goto BotWP_K 1274 890\n0 list\n' > /tmp/g.txt`) `ok: 2 steps` verir, `0 follow X` içeren dosya `bad verb` ile rc≠0 verir (rapora komut ve çıktı)
- [ ] K4: `git diff --stat gece/2026-10-02...bot/F5-72` yalnızca §4'teki 3 dosya + plan dosyası + `plans/README.md` (yalnızca kendi satırının durum sözcüğü); `git diff -U0 gece/2026-10-02...bot/F5-72 -- BotCore/ScriptPlan.h` yalnızca iki satır değişikliği (`-`/`+` çifti: yorum ve `kVerbs[]` ilk satırı); `GameServer/`, `AIServer/`, `shared/`, `docs/`, `*.vcxproj*`, diğer `BotCore/` dosyaları farkı **boş**; `git diff --check` boş
- [ ] K5: `file BotCore/ScriptPlan.h Tests/BotCoreTests/ScriptTests.cpp` → ASCII + CRLF; `file tools/skill-script-gen.py` → ASCII, CRLF **yok** (LF korunur); `python3 tools/check-perception-contract.py` rc=0
- [ ] K6: sözlük üç yerde tutarlı: `git grep -n -a -E '"goto"' -- BotCore tools Tests` çıktısı en az bu üç dosyayı (`ScriptPlan.h`, `ScriptTests.cpp`, `skill-script-gen.py`) gösterir ve `kVerbs[]` ile `VERBS` aynı 21 elemandır (`goto` ikisinde de `move`'dan sonra); `ScriptRunner.cpp`/`BotManager.cpp` farkı boş
- [ ] K7 (Claude, çalışma zamanı): `GameServer.ini` `[BOT] ENABLED=1, NAV=1, TELEMETRY=decisions`; `Scripts/f572.txt` = `0 goto BotWP_K 1274 890` + `70000 list` (bot önceden spawn, doğuş noktasında; F5-70 K11 kurulumu): `script run f572` → `Bot_*.log`'da `ScriptRunner: loaded (2 step(s)…`, `step 1/2 … goto BotWP_K 1274 890`, hemen ardından `cmd goto: BotWP_K planned <w> waypoints …`, sonra `arrived at (1274.0, 890.0) after N packets`; `live-*.jsonl` içinde `SCRIPT_STEP` `verb:"goto"`; `FAIRNESS_REJECT` 0
- [ ] K8 (Claude): `Scripts/f572_bad.txt` = `0 gotoo BotWP_K 1 2` → `script run f572_bad` `refused (f572_bad.txt:1: bad verb)`, `SCRIPT_START` yok; `NAV=0` ile `f572` → `cmd goto: … refused (nav_off)` (betik `completed`, çökme yok, bot yürümez); `ENABLED=0` iken `Scripts/` okunmaz (davranış değişmedi)
- [ ] K9 (Claude): sunucu `./tools/run-servers.sh stop` ile kapanır (`0/3 hazır`); `GameServer.ini` ve `Scripts/` başlangıç hâline döner; "AC-NAV-03 kapandı" **yazılmaz** (kanıt F5-66)

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Script_|tests,|failed"
./tools/run-tests.sh Debug 2>&1 | grep -E "tests,|failed"
python3 tools/skill-script-gen.py --selftest
printf '0 goto BotWP_K 1274 890\n0 list\n' > /tmp/g.txt && python3 tools/skill-script-gen.py --check /tmp/g.txt
printf '0 follow X\n' > /tmp/f.txt; python3 tools/skill-script-gen.py --check /tmp/f.txt; echo rc=$?
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-72
git diff -U0 gece/2026-10-02...bot/F5-72 -- BotCore/ScriptPlan.h
git diff --check gece/2026-10-02...bot/F5-72
file BotCore/ScriptPlan.h Tests/BotCoreTests/ScriptTests.cpp tools/skill-script-gen.py
git grep -n -a -E '"goto"' -- BotCore tools Tests
# Claude (çalışma zamanı): ./tools/run-servers.sh start ; Scripts/f572.txt koy ; echo "script run f572" >> /mnt/c/dev/fdp/server/BotCommands.txt ; grep -a -E "ScriptRunner|cmd goto|arrived" /mnt/c/dev/fdp/server/Logs/Bot_*.log ; ./tools/run-servers.sh stop
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2-§3: `ScriptPlan.h`/`ScriptTests.cpp` CRLF + tab + Allman, yorumlar İngilizce, ASCII. Python dosyasında mevcut stil (4 boşluk, LF) korunur. Bot sistemi kapalıyken (varsayılan) sunucu davranışı **bayt bayt aynıdır**: bu plan `GameServer` kaynağını değiştirmez; yalnızca `ScriptPlan.h` değiştiği için `ScriptRunner.cpp` yeniden derlenir.
- **Bot avantajı yasağı / adalet:** betik bota yeni yetki vermez; `goto` adımı `ExecuteCommand` → `CommandGoto` → `ActionExecutor::BeginGoto` yolundan geçer (aynı `BotFairnessGuard`, `CheckMoveChord`, `kMovePeriodMs` hızı). Betik sözlüğü yaşam döngüsü komutlarını (`spawn despawn match scenario script`) dışarıda tutmaya devam eder.
- **Dürüstlük:** `Script_GotoStep` yalnızca ayrıştırıcı sözleşmesini sınar; `goto` adımının oyunda çalıştığı K7/K8'de Claude'un çalışma zamanı koşusuyla kanıtlanır. Çalıştırmadığın testi "geçti" yazma.
- Beklenmedik durumda (sözlüğün dördüncü bir kopyası çıkarsa, `ScriptPlan.h` başka bir yerde sabit 20'ye dayanıyorsa, ek dosya gerekiyorsa) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

### Tazeleme (2026-10-03, otonom ön-plan tazeleme, `gece/2026-10-02` @ `f636c17`)

- Önceki planın (F5-73, merge `bc3170d`) değiştirdiği dosyalar `BotCore/NavDrive.h`, `BotCore/NavTrack.h`, `NavDriveTests.cpp`, `NavTrackTests.cpp`: bu planın dosyalarıyla **kesişmez**; `git diff fa24182 HEAD -- BotCore Tests` yalnızca bu dört dosyayı gösterir `[V]`. `ScriptPlan.h` son değişikliği hâlâ `59662a7`.
- Doğrulananlar `[V]`: `ScriptPlan.h` yorum `:59`, `IsScriptVerb` `:60`, `kVerbs[]` `:62-67` (20 eleman), `ParseScript` fiil denetimi `:224-230`, `kScriptMaxSteps = 100`, `kScriptMaxLines = 128` (`:13-14`); `ScriptTests.cpp` `Script_VerbWhitelist` `:44-73`, 6 `TEST_CASE`, `Script_Limits` `:143`, `vcxproj:101`; `skill-script-gen.py` `VERBS` `:73-79`, `raw` `:214-222`, `check_bytes` `:379-380`; `"pkick"` yalnızca 3 sözlük kopyası + `BotManager.cpp:657`/`:2221`; `BotManager.cpp:629` `goto` dağıtımı, `ScriptRunner.cpp:214` `ExecuteCommand`; `--selftest` bugün `23 checks, 0 failed`.
- Düzeltilenler: selftest satır numaraları (`# 16.` `:538`, `check_bad_verb` `:548-549`, `# 17.` `:551`); taban test sayısı `293` → `314` (F5-73 birleşti), K2 beklentisi `315`; F5-73 notu "birleşti" olarak güncellendi.
- Adım/kabul çelişkisi yok; plan geçerli, Durum `HAZIR` kalır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-72` (taban `gece/2026-10-02`) — `81650f2 [F5-72] Betik fiili goto: izinli komut sozlugu 20 -> 21`
- Değişen dosyalar ve neden:
  - `BotCore/ScriptPlan.h`: `kVerbs[]` 20 → 21 (`"goto"`, `"move"`'dan sonra), sözlük yorumu "20 verbs" → "21 verbs" (Kapsam §3.1).
  - `Tests/BotCoreTests/ScriptTests.cpp`: `Script_VerbWhitelist` 21 fiile güncellendi (+`IsScriptVerb("GOTO")`); sonuna yeni `Script_GotoStep` (+1 test) eklendi (Kapsam §3.2).
  - `tools/skill-script-gen.py`: `VERBS` demetine `"goto"`, yorum "21 verbs", selftest'e `check_ok_goto` + `raw_goto_passthrough` (23 → 25) (Kapsam §3.3).
- Derleme sonucu (`tools/build.sh Release`, son satırlar):
  ```
  proj-GameServer.vcxproj -> ...\build\bin\x86-Release\Server\GameServer.exe
  BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe
  ```
  `./tools/build.sh Debug` de rc=0 (`GameServer.exe`, `BotCoreTests.exe` üretildi). `touch BotCore/ScriptPlan.h` sonrası iki yapılandırmada yalnızca eski `GameServerDlg.cpp` C4267/C4834 uyarıları (artış yok).
- Kabul kriterleri öz-değerlendirme: K1 ✔, K2 ✔, K3 ✔, K4 ✔, K5 ✔, K6 ✔. K7/K8/K9 Claude çalışma zamanı (sunucusuz ortamda yapılmadı).
- Test çıktıları (başlangıç ve son test sayısı, `Script_*` satırları, `skill-script-gen.py --selftest`, `--check` komutları):
  - Başlangıç: `314 tests, 0 failed`; son: Release `315 tests, 0 failed`, Debug `315 tests, 0 failed` (taban + 1).
  - `Script_` satırları: `Script_ParseValid`, `Script_VerbWhitelist`, `Script_GotoStep`, `Script_OffsetRules`, `Script_LineRules`, `Script_Limits`, `Script_ErrorLineNumbers` hepsi `[ OK ]`.
  - `skill-script-gen.py --selftest`: başlangıç `23 checks, 0 failed` → son `25 checks, 0 failed`.
  - `--check /tmp/g.txt` (`0 goto …` + `0 list`): `ok: 2 steps, 31 bytes, 2 lines, last offset 0 ms`, rc=0; `--check /tmp/f.txt` (`0 follow X`): `1: bad verb`, rc=1.
  - Sözlük üç kopyada tutarlı (`git grep -n -a -E '"goto"' -- BotCore tools Tests` → `ScriptPlan.h:64`, `ScriptTests.cpp:48`, `skill-script-gen.py:75`).
- Plandan sapmalar ve gerekçeleri: `plans/README.md` F5-72 satırındaki durum sözcüğü **güncellenmedi**; `AGENTS.md` §2 `plans/README.md`'yi dokunulmayacaklar listesinde sayıyor ve düzenleme izni kapalı. K4'ün izin verdiği kümeye `plans/README.md` dahil, ancak değişikliği zorunlu kılmıyor; durum sözcüğünü Claude/gece döngüsü günceller.
- Açık sorular: yok.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: DOĞRULANDI
- İncelenen: `gece/2026-10-02...bot/F5-72` @ `e22d498` (kod commit'i `81650f2`; taban `169cd9f`; gece modu, `AUTO_LOOP=1`: birleştirmeyi döngü betiği yapar, bu oturum birleştirme/push yapmadı)
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `touch BotCore/ScriptPlan.h` sonrası `./tools/build.sh Release` rc=0 (`grep -c warning` = 5: `GameServerDlg.cpp` 1147/1806 C4267, 820 C4834, `UpgradeHandler.cpp` 634/862 C4789; hepsi eski, `BotCore/` ve `Bot\` altında uyarı yok); `Debug` rc=0 (yalnızca `GameServerDlg.cpp` C4267 iki satır) |
| K2 | ✔ | Release ve Debug: `315 tests, 0 failed` (taban 314 + 1, uygulayıcının ölçümüyle aynı); `Script_ParseValid`, `Script_VerbWhitelist`, `Script_GotoStep`, `Script_OffsetRules`, `Script_LineRules`, `Script_Limits`, `Script_ErrorLineNumbers` hepsi `[ OK ]` (iki yapılandırmada) |
| K3 | ✔ | `--selftest` -> `selftest: 25 checks, 0 failed`; `--check /tmp/g.txt` -> `ok: 2 steps, 31 bytes, 2 lines, last offset 0 ms` rc=0; `--check /tmp/f.txt` (`0 follow X`) -> `1: bad verb` rc=1 |
| K4 | ✔ | `git diff --stat gece/2026-10-02...bot/F5-72`: `ScriptPlan.h` (4 satır), `ScriptTests.cpp`, `skill-script-gen.py`, plan dosyası; `plans/README.md` değişmedi (izin verilen küme, zorunlu değil); `ScriptPlan.h` farkı tam bir `-`/`+` çifti x 2 (yorum `:59`, `kVerbs[]` ilk satırı `:64`); `GameServer/ AIServer/ shared/ docs/ *.vcxproj*` farkı yok; `git diff --check` boş (rc=0) |
| K5 | ✔ | `file`: `ScriptPlan.h` ve `ScriptTests.cpp` "ASCII text, with CRLF line terminators" (tabanda blob LF: `core.autocrlf=true`, diff'te `^M` değişimi yok); `skill-script-gen.py` "ASCII text" (CRLF yok, `.gitattributes` `*.py eol=lf`); `check-perception-contract.py` RESULT: PASS rc=0 |
| K6 | ✔ | `git grep -n -a '"goto"' -- BotCore tools Tests`: `ScriptPlan.h:64`, `ScriptTests.cpp:48`, `skill-script-gen.py:75`; üç dizi de `"move", "goto", "stop", ...` ile başlar, kalan satırlar aynı; `"pkick"` kopyaları yalnızca bu üç dosya + `BotManager.cpp:657/2221` (dağıtım ve günlük metni); `ScriptRunner.cpp`/`BotManager.cpp` farkı yok |
| K7 | ✔ | **Çalışma zamanı `[V]`** (Release, `bot/F5-72` ucu; `[BOT] ENABLED=1, NAV=1, TELEMETRY=decisions`; `NavService: nav ready ... crc32=4fd154bc`; `BotWP_K` (1294; 934)'te): `script run f572` -> `ScriptRunner: run f572: loaded (2 step(s), last offset 70000 ms)`, `step 1/2 (line 1, +0 ms, late 0 ms): goto BotWP_K 1274 890`, hemen ardından `cmd goto: BotWP_K planned 4 waypoints, route 57.1 m, expanded 54, 0.63 ms, walking to (1274.0, 890.0) at speed 45`, `arrived at (1274.0, 890.0) after 9 packets`, `step 2/2 (+70000 ms, late 6 ms): list`, `finished f572: completed, 2/2 step(s) in 70007 ms`. `live-183458.jsonl`: `SCRIPT_START`, `SCRIPT_STEP step:1 verb:"goto"`, `SCRIPT_STEP step:2 verb:"list"`, `SCRIPT_END completed`; `FAIRNESS_REJECT` 0 |
| K8 | ✔ (`ENABLED=0` hariç) | `f572_bad.txt` (`0 gotoo BotWP_K 1 2`) -> `ScriptRunner: run f572_bad: refused (f572_bad.txt:1: bad verb)`, telemetride `SCRIPT_START` `f572_bad` yok (0). `NAV=0` ile yeniden başlatılıp `f572` -> `cmd goto: BotWP_K refused (nav_off)`, `finished f572: completed, 2/2`, çökme yok, `list`: `pos=1294.0,934.0 moving=0` (bot yürümedi). `ENABLED=0` ayrıca koşulmadı: `ScriptRunner` yalnızca `BotManager` etkinken kurulur ve bu planda `GameServer` kaynağı değişmedi (F5-70 K15 ile aynı gerekçe) |
| K9 | ✔ | `run-servers.sh stop` iki kez nazik kapanış, `status` `0/3 hazır`; `GameServer.ini` yedekten geri alındı (md5 öncesi = sonrası = `791b71379a7173f877b0dcfc04595a66`); `Scripts/f572*.txt` silindi, `BotCommands.*` kalmadı. "AC-NAV-03 kapandı" yazılmadı (kanıt F5-66) |

- Bulgular (önem sırasıyla):
  1. Engelleyici bulgu yok. Not: uygulayıcı `plans/README.md` durum sözcüğünü güncellemedi (`AGENTS.md` §2); plana uygun (K4 zorunlu kılmaz), kayıt bu doğrulamada güncellendi.
  2. Not: `Script_GotoStep` ayrıştırıcı sözleşmesini sınar (D3: argüman denetimi yok, `"0 goto"` kabul); `goto` argümanlarının geçerliliği çalışma zamanında `CommandGoto`'dadır, K7/K8 koşusu bunu kanıtladı.
  3. Not: `goto` adımı eşzamansızdır (D4). K7'de 57,1 m rota 9 `Move` paketinde bitti, betik 70 sn bekledi; gerçek betiklerde sonraki adım yürüme süresinden sonraya konmalı (Karus doğuş -> arena A ~60 sn, El Morad -> arena A ~152 sn); ADR-0017 Ek F5-72 madde 3'e yazıldı.
- Düzeltme talimatı: gerekmiyor (`DOĞRULANDI`).
