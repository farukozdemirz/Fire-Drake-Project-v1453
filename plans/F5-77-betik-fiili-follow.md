# F5-77: Betik fiili `follow` — izinli komut sözlüğü 21 → 22 (`BotCore/ScriptPlan.h`, `ScriptTests.cpp`, `tools/skill-script-gen.py`, örnek betik)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-77 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | `KAPANDI`: F4-19 (`BotCore/ScriptPlan.h`, `ParseScript`), F4-20 (`ScriptRunner`: her adım `BotManager::ExecuteCommand` yolundan), F4-42 (`tools/skill-script-gen.py`), F5-72 (sözlük 20 → 21, `goto`; merge `10dab59`), F5-74 (`/bot follow` sunucu bağlaması; merge `23fe31d`). **F5-64'e bağımlı DEĞİLDİR** (F5-64 şimdi `gece/2026-10-02`'ye birleşmiştir, `79fbf23`) ve onunla dosya paylaşmaz: F5-64 yalnızca `BotCore/NavTrack.h`, `BotCore/NavDrive.h`, `NavTrackTests.cpp`, `NavDriveTests.cpp`; bu plan `ScriptPlan.h`, `ScriptTests.cpp`, `skill-script-gen.py` ve yeni bir `bots/config/*.txt`. F5-64'ün sembolleri (`PlanFollow`, `AssessFollow`, `FollowPlanDue`, ...) kullanılmaz |
| İlgili gereksinim / kabul | ADR-0017 Ek F4-19 madde 2 (izinli komut sözlüğü) ve Ek F5-72 (20 → 21; **bu planın genişletmesi ADR-0017 Ek F5-77 olarak doğrulamada yazılır**); F5-72 plan §3 kapsam dışı notu ("`follow` F5-63 sonrası ayrı dilim"); `docs/17` F4 "betikli test senaryoları" ve F5-66 (çalışma zamanı doğrulama koşusu betikle takip verebilsin) |
| Tahmini büyüklük | S (4 dosya: 3 mevcut + 1 yeni metin dosyası; saf mantık + Python aracı; `GameServer` kaynağı değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-03 (gece modu, ön-plan; referanslar `gece/2026-10-02-preplan` @ `ef759e4` üzerinde doğrulandı; **tazelendi** `gece/2026-10-02` @ `b747aab` üzerinde, bkz. Tazeleme notu) |

---

## 1. Amaç

Test betikleri (`./Scripts/<ad>.txt`, `/bot script run <ad>`) artık `follow <takipçi bot> <hedef bot> [hız]` adımı içerebilir: `BotCore::IsScriptVerb("follow")` doğru döner, `ParseScript` bu adımı kabul eder, betik üretecinin (`tools/skill-script-gen.py`) `raw` satırı ve `--check` doğrulayıcısı aynı sözlüğü kullanır. `ScriptRunner` değişmez: adımı zaten `BotManager::ExecuteCommand`'a verir ve `follow` dağıtımı orada hazırdır (F5-74). Böylece hareketli hedef takibi (F5-66 koşusu, F6 solo/party senaryoları) elle komut yazmadan, zamanlı betikle sınanabilir. Bot sistemi kapalıyken (varsayılan) ve `NAV=0` iken hiçbir sunucu davranışı değişmez.

## 2. Bağlam (okunması zorunlu)

Satır numaraları `gece/2026-10-02-preplan` @ `ef759e4` (= `gece/2026-10-02` ucu) üzerinde doğrulandı. Sapma görürsen **dur** (§5 adım 1).

- `BotCore/ScriptPlan.h` (yalnızca standart kütüphane; ASCII + CRLF `[V: file]`): sözlük yorumu `:59` ("the 21 verbs … "goto" added by F5-72"), `IsScriptVerb` `:60`, `kVerbs[]` `:62-67` (21 eleman; ilk satır `:64` `"move", "goto", "stop", "attack", "cast", "pot", "sit", "stand", "target", "regene",`), küçük harfle karşılaştırma `:69-84`; `ParseScript` yalnızca ilk sözcüğü sözlükle denetler, argüman doğrulaması **yoktur** (argümanı çalışma zamanında `CommandFollow` doğrular). Sınırlar `kScriptMaxBytes 8192`, `kScriptMaxLines 128`, `kScriptMaxSteps 100`, `kScriptMaxLineLen 255` (`:12-15`) değişmez.
- `Tests/BotCoreTests/ScriptTests.cpp` (ASCII + CRLF, 296 satır, 7 `TEST_CASE`): `Script_VerbWhitelist` `:44-74` (`verbs[]` `:46-51`, `for (int i = 0; i < 21; ++i)` `:53`, olumlu büyük-küçük harf örnekleri `:56-59`, olumsuz örnekler `:61-69`), `Script_GotoStep` `:76-136` (bu planın şablonu), `Script_OffsetRules` `:138`. `BotCoreTests.vcxproj` dosyayı zaten içerir.
- `GameServer/Bot/BotManager.cpp:631-632` `follow` → `CommandFollow` dağıtımı (F5-74); `CommandFollow` `:1282-1369`: `follow <bot> <target bot> [speed]` (2-3 sözcük; `speed` `ParseIntStrict`, `-32768..32767`; hatalı kullanımda `usage: follow <bot> <target bot> [speed]` günlüğü; hedef oturum `PHASE_IN_GAME` değilse ya da bilinmiyorsa günlük + dönüş; `ActionExecutor::BeginFollow` ret nedenleri `bad_target`/`target_not_visible`/`dead`/`nav_off`/`nav_zone` günlüğe `cmd follow: <bot> refused (<neden>)`; başarıda `cmd follow: <bot> following <hedef> (sid N) at speed S`). `GameServer/Bot/ScriptRunner.cpp` her adımı `m_mgr.ExecuteCommand(command)` ile çalıştırır ve `SCRIPT_STEP` telemetrisine `verb` alanını yazar (fiil sözlüğüne bağımlı kod yoktur). **`BotManager.cpp`, `ScriptRunner.*`, `ActionExecutor.*` bu planda değişmez.**
- `tools/skill-script-gen.py` (ASCII + LF `[V: file]`): `VERBS` demeti `:74-78` (yorum `:73` "The 21 verbs a script may run", ilk satır `:75`), `VERB_SET` `:79`; `raw` satırı sözlükte olmayan fiili reddeder; `check_bytes` aynı kümeyle `bad verb` der; selftest `# 16.` bloğu `:538-553` (`check_bad_verb` `:548-549` (`0 teleport X`), F5-72'nin `check_ok_goto`/`raw_goto_passthrough` kontrolleri `:550-553`), `# 17.` `:555`; `python3 tools/skill-script-gen.py --selftest` bugün `selftest: 25 checks, 0 failed` basar `[V]`.
- Sözlüğün başka kopyası yoktur: `git grep -n -a -E '"pkick"' -- . ':!docs' ':!plans'` yalnızca `ScriptPlan.h:65`, `ScriptTests.cpp:49`, `skill-script-gen.py:76` (üç kopya) ile `BotManager.cpp:659`/`:2451` (komut dağıtımı ve bir günlük metni; sözlük değildir) verir `[V]`; başka kopya çıkarsa **dur**.
- Örnek betik biçimi: `bots/config/script_smoke_2bot.txt` ve `script_see_symmetry.txt` (başlık `#` yorumları + `<ofset_ms> <komut>` satırları; `python3 tools/skill-script-gen.py --check bots/config/script_smoke_2bot.txt` bugün `ok: 7 steps, 273 bytes, 9 lines, last offset 6000 ms` verir `[V]`).
- Test sayısı tabanda: tazeleme anında `331` (`git grep -h -c '^TEST_CASE' -- Tests/BotCoreTests` toplamı `[V]`; ilk yazımda 315'ti, F5-64 birleşmesiyle +16). Bu plan **+1** test ekler (beklenen `332`): **taban sayıyı başlangıçta yine kendin ölç** ve K2'yi ona göre oku.

### Karar kaydı (otonom döngüde Claude kararı — gözden geçirilmeli; yeni ADR yok: ADR-0017 Ek F4-19 madde 2 ve Ek F5-72'nin uygulaması, **ADR-0017 Ek F5-77 doğrulamada yazılır**)

**D1 — Yalnızca `follow`.** Sözlüğe tek fiil eklenir (21 → 22). Yasak liste (`spawn despawn match scenario script`) aynen kalır.

**D2 — Sözlük sırası.** `"follow"`, `"goto"`'dan hemen sonra eklenir (aynı hareket ailesi; sıra davranışı etkilemez): `"move", "goto", "follow", "stop", ...`.

**D3 — Argüman doğrulaması ayrıştırıcıda yapılmaz.** `ParseScript` sözleşmesini korur (yalnızca ofset + fiil); `follow` argümanları (`<bot> <hedef bot> [hız]`) `CommandFollow`'da denetlenir. Ayrıştırıcıya fiil-bazlı argüman denetimi eklemek ADR-0017 Ek F4-19 madde 5'i değiştirir; bu plan yapmaz. Bu karar ayrıca **F5-75** (sunucu bütçe bağlaması, F5-64'ün ardılı) komutun sözdizimini ya da ret metinlerini değiştirse bile bu planı geçersiz kılmaz.

**D4 — `follow` adımı eşzamansız ve süresizdir.** `ExecuteCommand("follow …")` takibi başlatıp döner; takip `/bot stop`, `move`, `goto`, ölüm ya da hedef kaybı (`target_lost`, ~15 sn) ile biter, **betik bitince kendiliğinden bitmez**. Betik yazarı takibi `stop <bot>` (ya da `stop all`) adımıyla sonlandırmalı ve ölçülen adımları takibin yakınsama süresinden sonraya koymalıdır (F5-74 ölçümü `[V]`: 10 dk takipte yakınsamadan sonra aralık p50 4,5 m / p95 10,8 m). `follow` yalnızca hedef takipçinin **görüşünde** iken başlar (`target_not_visible`); reddedilen adım betiği durdurmaz (ADR-0017 Ek F4-20 madde 4), reddi komutun kendi günlük satırı bildirir. Bunu `docs/` kısmına Claude yazar (kapsam dışı) ve örnek betiğin başlık yorumuna kısa İngilizce not olarak girer (§5 adım 5).

**D5 — Örnek betik.** F5-72 örnek betik eklememişti; takip adımı bir hedef hareketi ve bitiş `stop`'u gerektirdiğinden kullanımı örnekle sabitlemek için `bots/config/script_follow_2bot.txt` eklenir (iki bot: hedef `BotMF_K`, takipçi `BotWP_K`; `script_smoke_2bot.txt` ile aynı biçim). Dosya yalnızca `--check` ile doğrulanır; çalışma zamanı koşusu Claude'un (K7).

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/ScriptPlan.h`: `kVerbs[]` 21 → 22 eleman (`"follow"` eklenir), yorum "21 verbs" → "22 verbs".
2. `Tests/BotCoreTests/ScriptTests.cpp`: `Script_VerbWhitelist` 22 fiile güncellenir; yeni `Script_FollowStep` testi (+1 test).
3. `tools/skill-script-gen.py`: `VERBS` demetine `"follow"`, yorum "22 verbs"; selftest'e iki kontrol (`25 → 27`).
4. `bots/config/script_follow_2bot.txt`: yeni örnek betik (§5 adım 5).

**Kapsam dışı (yapılmayacak)**

- `GameServer/`, `AIServer/`, `shared/`, `*.vcxproj*`, `docs/`: dokunulmaz (ADR-0017 Ek F5-77 ve betik biçimi notunu Claude yazar). `BotManager.cpp`, `ScriptRunner.*`, `ActionExecutor.*`, `BotCore/NavDrive.h`, `BotCore/NavTrack.h`: **değişmez** (son ikisi F5-64'ün).
- `follow` argüman doğrulaması (D3), betik adımının sonucunu bekleme/okuma (ADR-0017 Ek F4-19 madde 5: yok), `skill-script-gen.py` için yeni spec türü (`follow` yalnızca `raw` ile gider; yeni `kind` eklenmez), takibin betik bitince otomatik sonlandırılması (D4: ayrıştırıcı/`ScriptRunner` davranışı değişmez), `follow` için `ScriptRunner` telemetri alanı eklemek.
- Sınırlar (`kScriptMax*`) ve hata sıralaması değişmez; mevcut diğer testler değişmez (`Script_GotoStep` dahil).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/ScriptPlan.h` | değiştir | yalnızca `kVerbs[]` ve sözlük yorumu |
| `Tests/BotCoreTests/ScriptTests.cpp` | değiştir | `Script_VerbWhitelist` güncellenir; `Script_GotoStep`'ten hemen sonra `Script_FollowStep` eklenir |
| `tools/skill-script-gen.py` | değiştir | `VERBS` demeti, yorum, iki selftest kontrolü |
| `bots/config/script_follow_2bot.txt` | yeni | örnek betik; ASCII, satır sonu mevcut `bots/config/script_smoke_2bot.txt` ile aynı (çalışma ağacında `file` bugün "ASCII text, with CRLF line terminators" der `[V]`; `ParseScript` CR/LF'yi kabul eder); yeni dosyada `file` çıktısını bu dosyanınkiyle karşılaştır |

4 dosya; yeni `.cpp`/`.h` yok, `.vcxproj` değişmez. `ScriptPlan.h` ve `ScriptTests.cpp`: ASCII + CRLF (`file` ile doğrula). `skill-script-gen.py`: ASCII + LF (mevcut biçim korunur). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-77 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. `./tools/run-servers.sh status` `[UP]` ise `./tools/run-servers.sh stop`. §2'deki referansları aç ve doğrula (özellikle `ScriptPlan.h:59-67`, `ScriptTests.cpp:44-74` ve `:76-136`, `skill-script-gen.py:73-79` ve `:548-553`, `BotManager.cpp:631-632`); sapma varsa **dur**. Başlangıç ölçümleri: `./tools/run-tests.sh Release 2>&1 | grep -E "tests,|failed"` (taban sayıyı rapora yaz); `python3 tools/skill-script-gen.py --selftest` (`25 checks, 0 failed` beklenir); `git grep -n -a -E '"pkick"' -- . ':!docs' ':!plans'` çıktısını rapora yaz; `git grep -n -a -i 'follow' -- BotCore/ScriptPlan.h Tests/BotCoreTests/ScriptTests.cpp tools/skill-script-gen.py` **boş** olmalı.
2. **`BotCore/ScriptPlan.h`:** `kVerbs[]` ilk satırı `"move", "goto", "follow", "stop", "attack", "cast", "pot", "sit", "stand", "target", "regene",` olur (diğer satırlar aynen); `:59` yorumu `Case-insensitive match against the 22 verbs a script may run (plan section 5.2; "goto" added by F5-72, "follow" by F5-77).` olur. Başka satır değişmez (CRLF korunur).
3. **`Tests/BotCoreTests/ScriptTests.cpp`:**
   - `Script_VerbWhitelist`: `verbs[]` dizisinin ilk satırına `"follow"` (`"goto"`'dan sonra; satır uzarsa yeni satıra bölünebilir), döngü sınırı `22`; olumlu örneğe `CHECK(BotCore::IsScriptVerb("FOLLOW"));`; olumsuz örneklere `CHECK(!BotCore::IsScriptVerb("followx"));` ve `CHECK(!BotCore::IsScriptVerb("follo"));` eklenir. Mevcut diğer satırlar aynen kalır.
   - Yeni `TEST_CASE("Script_FollowStep")` (`Script_GotoStep`'ten hemen sonra, aynı kalıpla):
     - `"0 follow BotWP_K BotMF_K\r\n1500   FOLLOW BotWP_K BotMF_K 45\n 3000\tfollow BotWP_K BotMF_K  \n"` → `error == SCRIPT_OK`, 3 adım; komut metinleri sırasıyla `"follow BotWP_K BotMF_K"`, `"FOLLOW BotWP_K BotMF_K 45"`, `"follow BotWP_K BotMF_K"` (iç boşluk korunur, uç boşluk kırpılır); `offsetMs` 0/1500/3000; `line` 1/2/3.
     - `"0 follow"` → `SCRIPT_OK`, 1 adım, `command == "follow"` (argüman doğrulaması ayrıştırıcıda yok, D3).
     - `"0 followx A B"` → `SCRIPT_ERR_BAD_VERB`, `errorLine == 1`; `"0 move A 1 2\r\n10 follows A B"` → `SCRIPT_ERR_BAD_VERB`, `errorLine == 2` ve `steps` boş (ya-hep-ya-hiç).
     - Karışık dizi: `"0 goto BotMF_K 1274 890\n2000 follow BotWP_K BotMF_K\n60000 stop BotWP_K\n"` → `SCRIPT_OK`, 3 adım, komutlar sırasıyla `"goto BotMF_K 1274 890"`, `"follow BotWP_K BotMF_K"`, `"stop BotWP_K"`, `offsetMs` 0/2000/60000.
     - Bu test dosyasında `kScriptMaxSteps` testi **eklenmez** (`Script_GotoStep` ve `Script_Limits` zaten kapsar).
4. **`tools/skill-script-gen.py`:** `:73` yorumu `# The 22 verbs a script may run (BotCore/ScriptPlan.h: IsScriptVerb).`; `VERBS` demetinin ilk satırı `"move", "goto", "follow", "stop", ...` olur (başka satır değişmez). Selftest'te F5-72'nin `raw_goto_passthrough` kontrolünden hemen sonra, `# 17.`'den önce:
   - `ok, message = check_bytes(b"0 follow BotWP_K BotMF_K\n")` → `check("check_ok_follow", ok and message.startswith("ok: 1 steps"))`;
   - `o = offsets(gen("raw 0 follow BotWP_K BotMF_K\ncast B 1000 self 1\n", magic))` → `check("raw_follow_passthrough", o[0] == (0, "follow BotWP_K BotMF_K"))` (`raw_goto_passthrough` kalıbı).
   - Sonuç: `selftest: 27 checks, 0 failed`.
5. **`bots/config/script_follow_2bot.txt` (yeni):** başlığı İngilizce `#` yorumlarıdır (`script_smoke_2bot.txt` biçimi): ön koşul (`BotMF_K` hedef ve `BotWP_K` takipçi spawn edilmiş, oyunda ve **birbirinin görüşünde**: `/bot spawn BotMF_K,BotWP_K`), kullanım (`Scripts/follow.txt` olarak kopyala, `/bot script run follow`), `follow` adımının eşzamansız ve süresiz olduğu, takibin `stop` ile bitirilmesi gerektiği (D4). Adımlar:
   ```
   0 list
   500 goto BotMF_K 1274 890
   3000 follow BotWP_K BotMF_K
   60000 snap BotWP_K
   60500 snap BotMF_K
   90000 stop BotWP_K
   90500 stop BotMF_K
   91000 list
   ```
   Koordinat `(1274, 890)` F5-72 doğrulamasında (`BotWP_K` (1294; 934)'ten rota 57,1 m) ana dünyada yürünebilir bulunmuş bir noktadır `[V: STATUS F5-72 K7]`; uygulayıcı bu değeri değiştirmez. Dosya ASCII, 128 satır ve 8192 bayt sınırının altında olmalı.
6. **Derle ve sına:** `./tools/build.sh Release` ve `Debug` (GameServer çözümü; `ScriptPlan.h`'yi `ScriptRunner.cpp` dahil eder, yeni uyarı olmamalı); `./tools/run-tests.sh Release` ve `Debug`; `python3 tools/skill-script-gen.py --selftest`; `python3 tools/skill-script-gen.py --check bots/config/script_follow_2bot.txt`.
7. Uygulayıcı Raporu; `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` ve `Debug` rc=0; `BotCore/ScriptPlan.h` `touch` edilip yeniden derlenince **yeni uyarı yok** (`grep -c "warning"` sayısı rapora; eski `GameServerDlg.cpp` C4267/C4834 ve `UpgradeHandler.cpp` C4789 dışında artış yok)
- [ ] K2: `./tools/run-tests.sh Release` ve `Debug`: `0 failed`; sayı = tabandaki sayı **+ 1** (rapora başlangıç ve son sayı); `Script_VerbWhitelist` ve `Script_FollowStep` `[ OK ]`; `Script_GotoStep` ve diğer `Script_*` testleri değişmeden geçer
- [ ] K3: `python3 tools/skill-script-gen.py --selftest` → `selftest: 27 checks, 0 failed`; `check_ok_follow` ve `raw_follow_passthrough` `FAILED` listesinde değil; `python3 tools/skill-script-gen.py --check bots/config/script_follow_2bot.txt` → `ok: 8 steps, ... last offset 91000 ms` rc=0; bir geçici dosyayla (`printf '0 follow BotWP_K BotMF_K\n0 list\n' > /tmp/f.txt`) `ok: 2 steps` rc=0; `printf '0 followx A B\n' > /tmp/fx.txt` `bad verb` ile rc≠0 (rapora komut ve çıktılar)
- [ ] K4: `git diff --stat gece/2026-10-02...bot/F5-77` yalnızca §4'teki 4 dosya + plan dosyası + `plans/README.md` (yalnızca kendi satırının durum sözcüğü); `git diff -U0 gece/2026-10-02...bot/F5-77 -- BotCore/ScriptPlan.h` yalnızca iki satır değişikliği (`-`/`+` çifti: yorum ve `kVerbs[]` ilk satırı); `GameServer/`, `AIServer/`, `shared/`, `docs/`, `*.vcxproj*`, diğer `BotCore/` dosyaları, `BotManager.cpp`, `ScriptRunner.*`, `ActionExecutor.*` farkı **boş**; `git diff --check` boş
- [ ] K5: `file BotCore/ScriptPlan.h Tests/BotCoreTests/ScriptTests.cpp` → ASCII + CRLF; `file tools/skill-script-gen.py` → ASCII, CRLF **yok** (LF); `file bots/config/script_follow_2bot.txt` → ASCII ve `script_smoke_2bot.txt` ile **aynı** satır sonu (bugün CRLF); `python3 tools/check-perception-contract.py` rc=0
- [ ] K6: sözlük üç yerde tutarlı: `git grep -n -a -E '"follow"' -- BotCore tools Tests` çıktısı en az bu üç dosyayı (`ScriptPlan.h`, `ScriptTests.cpp`, `skill-script-gen.py`) gösterir ve `kVerbs[]` ile `VERBS` aynı 22 elemandır (`follow` ikisinde de `goto`'dan sonra); `ScriptRunner.cpp`/`BotManager.cpp` farkı boş
- [ ] K7 (Claude, çalışma zamanı): `GameServer.ini` `[BOT] ENABLED=1, NAV=1, TELEMETRY=decisions`; `BotMF_K` ve `BotWP_K` spawn edilmiş, birbirinin görüşünde (doğuş noktası; F5-74 K11 kurulumu); `Scripts/follow.txt` = `bots/config/script_follow_2bot.txt`: `script run follow` → `Bot_*.log`'da `ScriptRunner: run follow: loaded (8 step(s), last offset 91000 ms)`, `step 2/8 … goto BotMF_K 1274 890`, ardından `cmd goto: BotMF_K planned …`, `step 3/8 … follow BotWP_K BotMF_K`, hemen ardından `cmd follow: BotWP_K following BotMF_K (sid N) at speed 45`, `snap` adımlarında iki botun konumu (takipçi–hedef aralığı rapora, **bilgi**), `step 6/8 … stop BotWP_K` sonrası takipçi durur (`list`: `moving=0`), `finished follow: completed, 8/8 step(s)`; `live-*.jsonl` içinde `SCRIPT_STEP` `verb:"follow"`; `FAIRNESS_REJECT` 0, `VIOLATION` 0
- [ ] K8 (Claude): `Scripts/f577_bad.txt` = `0 followx BotWP_K BotMF_K` → `script run f577_bad` `refused (f577_bad.txt:1: bad verb)`, `SCRIPT_START` yok; `NAV=0` ile `follow` betiği → `cmd follow: BotWP_K refused (nav_off)` (betik `completed`, çökme yok, bot yürümez); hedef görüşte değilken (`BotMF_K` uzak bir noktada) `cmd follow: BotWP_K refused (target_not_visible)` ve betik sürer; `ENABLED=0` iken `Scripts/` okunmaz (davranış değişmedi)
- [ ] K9 (Claude): sunucu `./tools/run-servers.sh stop` ile kapanır (`0/3 hazır`); `GameServer.ini`, `Scripts/` ve `BotCommands.*` başlangıç hâline döner; "T-NAV-04/T-NAV-06 kapandı" ya da "AC-NAV-03 kapandı" **yazılmaz** (kanıt F5-66)

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release 2>&1 | grep -E "Script_|tests,|failed"
./tools/run-tests.sh Debug 2>&1 | grep -E "tests,|failed"
python3 tools/skill-script-gen.py --selftest
python3 tools/skill-script-gen.py --check bots/config/script_follow_2bot.txt
printf '0 follow BotWP_K BotMF_K\n0 list\n' > /tmp/f.txt && python3 tools/skill-script-gen.py --check /tmp/f.txt
printf '0 followx A B\n' > /tmp/fx.txt; python3 tools/skill-script-gen.py --check /tmp/fx.txt; echo rc=$?
python3 tools/check-perception-contract.py
git diff --stat gece/2026-10-02...bot/F5-77
git diff -U0 gece/2026-10-02...bot/F5-77 -- BotCore/ScriptPlan.h
git diff --check gece/2026-10-02...bot/F5-77
file BotCore/ScriptPlan.h Tests/BotCoreTests/ScriptTests.cpp tools/skill-script-gen.py bots/config/script_follow_2bot.txt bots/config/script_smoke_2bot.txt
git grep -n -a -E '"follow"' -- BotCore tools Tests
# Claude (çalışma zamanı): ./tools/run-servers.sh start ; /bot spawn BotMF_K,BotWP_K ; Scripts/follow.txt koy ; echo "script run follow" >> /mnt/c/dev/fdp/server/BotCommands.txt ; grep -a -E "ScriptRunner|cmd goto|cmd follow|follow:" /mnt/c/dev/fdp/server/Logs/Bot_*.log ; ./tools/run-servers.sh stop
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §2-§3: `ScriptPlan.h`/`ScriptTests.cpp` CRLF + tab + Allman, yorumlar İngilizce, ASCII. Python dosyasında mevcut stil (4 boşluk, LF) korunur; örnek betik yorumları İngilizce. Bot sistemi kapalıyken (varsayılan) sunucu davranışı **bayt bayt aynıdır**: bu plan `GameServer` kaynağını değiştirmez; yalnızca `ScriptPlan.h` değiştiği için `ScriptRunner.cpp` yeniden derlenir.
- **Bot avantajı yasağı / adalet:** betik bota yeni yetki vermez; `follow` adımı `ExecuteCommand` → `CommandFollow` → `ActionExecutor::BeginFollow` yolundan geçer (aynı `BotFairnessGuard`, `CheckMoveChord`, `kMovePeriodMs` hızı; hedef konumu yalnızca takipçinin kendi `WIZ_MOVE` gözleminden, F5-74 D1/D7). Betik sözlüğü yaşam döngüsü komutlarını (`spawn despawn match scenario script`) dışarıda tutmaya devam eder.
- **Dürüstlük:** `Script_FollowStep` yalnızca ayrıştırıcı sözleşmesini sınar; `follow` adımının oyunda çalıştığı K7/K8'de Claude'un çalışma zamanı koşusuyla kanıtlanır. Çalıştırmadığın testi "geçti" yazma. Örnek betiğin aralık ölçümü **bilgi**dir: tek koşu, T-NAV-04/T-NAV-06'yı kapatmaz.
- **Eşzamanlı planlarla ilişki:** F5-64 `NavDrive.h`/`NavTrack.h`'yi değiştirmişti ve artık `gece/2026-10-02`'ye birleşmiştir; bu plan o dosyalara hiç dokunmaz, çakışma beklenmez. `plans/README.md` ve `docs/STATUS.md` ortak dosyadır: yalnızca kendi satırına dokun.
- Beklenmedik durumda (sözlüğün dördüncü bir kopyası çıkarsa, `ScriptPlan.h` başka bir yerde sabit 21'e dayanıyorsa, ek dosya gerekiyorsa) **dur** ve `Durum: UYGULANIYOR (BLOKE)` ile raporla.

## Tazeleme (2026-10-03, otonom döngü ön-plan tazeleme)

- Taban: `gece/2026-10-02` @ `b747aab` (F5-64 birleşti; `ef759e4`'ten beri değişen: `BotCore/NavDrive.h`, `BotCore/NavTrack.h`, `NavDriveTests.cpp`, `NavTrackTests.cpp`). Bu planın dokunduğu/okuduğu hiçbir dosya (`ScriptPlan.h`, `ScriptTests.cpp`, `skill-script-gen.py`, `BotManager.cpp`, `ScriptRunner.cpp`) `ef759e4`'ten beri değişmemiştir `[V: git diff]`.
- Yeniden doğrulananlar (hepsi aynen geçerli `[V]`): `ScriptPlan.h` `:59` yorum, `:62-67` `kVerbs[]` (21), sınırlar `:12-15`; `ScriptTests.cpp` 296 satır, `Script_VerbWhitelist` `:44` (`verbs[]` `:46`, döngü `< 21` `:53`, olumlu örnekler `:56-59`), `Script_GotoStep` `:76`, `Script_OffsetRules` `:138`; `skill-script-gen.py` `:73` yorum, `VERBS` `:74-78`, `VERB_SET` `:79`, `# 16.` `:538`, `check_bad_verb` `:549`, `check_ok_goto`/`raw_goto_passthrough` `:551-553`, `# 17.` `:555`, `--selftest` `25 checks, 0 failed`; `BotManager.cpp` `follow` dağıtımı `:631-632`, `CommandFollow` `:1282-1369`; `"pkick"` kopyaları yalnızca `ScriptPlan.h:65`, `ScriptTests.cpp:49`, `skill-script-gen.py:76` (+ `BotManager.cpp:659`/`:2451`); `follow` üç dosyada hâlâ yok; `script_smoke_2bot.txt` `ok: 7 steps, 273 bytes, 9 lines, last offset 6000 ms`, CRLF; `bots/config/script_follow_2bot.txt` henüz yok.
- Değişen tek şey: tabandaki test sayısı `315` → `331` (F5-64 +16); bu plan sonrası beklenen `332`. F5-64'ün getirdiği yeni kodla çelişen adım/kabul kriteri yok (dosya kümeleri ayrık). Durum `HAZIR` kalır.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum:
- Branch / commit'ler:
- Değişen dosyalar ve neden:
- Derleme sonucu (`tools/build.sh Release`, son satırlar):
- Kabul kriterleri öz-değerlendirme:
- Test çıktıları (başlangıç ve son test sayısı, `Script_*` satırları, `skill-script-gen.py --selftest`, `--check` komutları):
- Plandan sapmalar ve gerekçeleri:
- Açık sorular:

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz doğrulanmadı.)
