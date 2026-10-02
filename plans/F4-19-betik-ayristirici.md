# F4-19: Betikli test dizisi, dilim 1 — `BotCore/ScriptPlan.h`: betik dosyası ayrıştırıcı ve doğrulayıcı (saf mantık)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-19` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F3-05 (`BotCore`, birim test çatısı) — `KAPANDI`; F4-01..F4-11 (betiğin sürebileceği komutlar) — `KAPANDI`; F4-12..F4-18 (`see`/`npcs`/`snap` gözlem komutları) — `KAPANDI` (F4-18 merge `a7349a1`) |
| İlgili gereksinim / kabul | `docs/17` F4 "Görevler 6) Betikli test senaryoları" ve "Kabul: betikli dizilerde sunucuya giden geçersiz aksiyon ≤ %1"; MET-ACT-02, MET-FAIR-01 (`docs/16` §3.4); `docs/13` §2 (`ScenarioRunner`), ADR-0016 (saf mantık `BotCore`'da) |
| Tahmini büyüklük | S (1 yeni başlık + 1 yeni test dosyası + 2 `vcxproj` satırı; `GameServer/` değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

F4 kabulü "betikli dizilerde" ölçülür (`docs/17` F4): önceden yazılmış bir aksiyon sırası, zaman damgalarıyla, mevcut `/bot` komutlarını (`move`, `attack`, `cast`, `pot`, `pchat`, `snap`, …) sırayla çalıştırır ve sunucuya giden aksiyonların geçerlilik oranı (MET-ACT-02) ve fairness reddi (MET-FAIR-01) ölçülür. Bugün böyle bir sıra yalnızca elle `BotCommands.txt`'e satır yazarak verilebiliyor; zamanlama yok, tekrarlanabilirlik yok.

Bu plan, **betik dosyasının biçimini ve doğrulamasını** saf mantık olarak yazar: `BotCore/ScriptPlan.h` bir metni (`<ofset_ms> <komut satırı>` satırları) ayrıştırır, sınırları ve izinli komut sözlüğünü denetler, `ScriptStep` listesi döndürür. Her şey birim testlidir. **Sunucu koduna dokunulmaz**; çalıştırıcı (`/bot script run <ad>`, `GameServer/Bot/ScriptRunner.*`) bir sonraki planın (F4-20) işidir ve bu başlığı kullanır.

## 2. Bağlam (okunması zorunlu)

- `docs/17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md` F4 bölümü (satır 115-135): kapsam "betikli test botu", test "Betikli testler; MET-ACT-02; MET-FAIR-01", kabul "geçersiz aksiyon ≤ %1; fairness ihlali 0".
- `docs/16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md` §3.4 (satır 131-133): MET-ACT-02, MET-FAIR-01 tanımları (bu plan ölçmez; yalnızca girdiyi, betiği tanımlar).
- `docs/adr/ADR-0016-birim-test-catisi.md`: `BotCore` yalnızca standart kütüphane; başlık-yalnızca.
- `docs/adr/ADR-0015-bot-calisma-zamani-komut-kanali.md`: komut kanalı (`/bot <komut>`, `BotCommands.txt`); betik satırları aynı komut sözlüğünü kullanır.
- İlgili kod (hepsini açıp doğrula; satırlar `a7349a1` itibarıyla):
  - `GameServer/Bot/BotManager.cpp:591-680` `BotManager::ExecuteCommand`: komut sözlüğü (verb'ler `:610-657`: `spawn despawn list match scenario move stop attack cast pot sit stand target regene pinvite paccept pdecline pleave ppromote pkick pchat see npcs snap`), `:603` `RESPAWN_CYCLES` reddi. Betik komutları bu sözlüğün alt kümesidir.
  - `GameServer/Bot/BotManager.cpp:566-570` dosya komut kanalı: satır tamponu 256 bayt, `#` ile başlayan satırlar atlanır (betik biçimiyle aynı yorum kuralı).
  - `GameServer/Bot/ScenarioRunner.cpp:13-19` ve `:84-98` `IsSafeFileStem`: senaryo dosyası sınırları (4096 bayt, 64 satır, satır ≤ 255) — betik sınırları bu kalıba göre seçildi.
  - `BotCore/BotMotion.h:1-12` ve `BotCore/Perception.h:1-40`: dosya başlığı/biçim örneği (`#pragma once`, `namespace BotCore`, `inline`, tab, Allman, CRLF, ASCII).
  - `Tests/BotCoreTests/MotionTests.cpp:1-12` test dosyası biçimi (`#include "MiniTest.h"`, `#include <BotCore/...>`, `TEST_CASE`, `CHECK`, `CHECK_EQ(int(..), int(..))`); `Tests/BotCoreTests/MiniTest.h:129-175` makrolar (`CHECK_EQ` değerleri `<<` ile yazdırır: enum'ları `int(...)` ile sar).
  - `Tests/BotCoreTests/BotCoreTests.vcxproj:79-83` `ClCompile` listesi; `BotCore/BotCore.vcxproj:72-75` `ClInclude` listesi.

## 3. Kapsam

**Yapılacaklar**

1. `BotCore/ScriptPlan.h` (yeni, başlık-yalnızca): sınır sabitleri, `ScriptError`, `ScriptStep`, `ScriptParseResult`, `ScriptErrorText`, `IsScriptVerb`, `ParseScript`.
2. `Tests/BotCoreTests/ScriptTests.cpp` (yeni): altı birim testi (76 → 82).
3. İki proje dosyasına birer satır: `BotCore.vcxproj` (`ClInclude`) ve `BotCoreTests.vcxproj` (`ClCompile`).

**Kapsam dışı (yapılmayacak)**

- **`GameServer/` altında hiçbir dosya değişmez** (ne `Bot/` ne başka): çalıştırıcı, `script` komutu, dosya okuma, zamanlayıcı F4-20'dir. `ScriptPlan.h` bu planda **hiçbir yerden `#include` edilmez** (yalnızca test dosyası dahil eder).
- Dosya okuma (`fopen`), süre/zaman ölçümü, log yazma, komut çalıştırma: yok; `ParseScript` saf bir metin→liste fonksiyonudur (G/Ç yok, global durum yok).
- Koşul, döngü, değişken, bekleme-şartı ("hedef ölene kadar"), olay tetikleyicisi, rastgelelik: yok. Betik yalnızca **sabit zaman damgalı komut listesidir**.
- Betik içinden `spawn`, `despawn`, `match`, `scenario`, `script` çağrısı: **yasak** (yaşam döngüsünü senaryo yönetir; yinelemeli betik yok).
- Komut argümanlarının içeriğini doğrulamak (bot adı bilinir mi, sayılar geçerli mi): yok; bunu her komutun kendi işleyicisi zaten yapar. `ParseScript` yalnızca ilk sözcüğün (verb) izinli olup olmadığına bakar.
- Telemetri olayı, `docs/` değişikliği (Claude yapar), örnek betik dosyaları, `tools/` betiği.
- `BotCore/Perception.h`, `BotCombat.h`, `BotMotion.h`, `Rng.*` ve mevcut test dosyaları **değişmez**.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/ScriptPlan.h` | yeni | tüm saf mantık |
| `Tests/BotCoreTests/ScriptTests.cpp` | yeni | altı `TEST_CASE` |
| `BotCore/BotCore.vcxproj` | değiştir | yalnızca bir `<ClInclude Include="ScriptPlan.h" />` satırı (`Rng.h`'den önce, alfabetik sıra: `Perception.h` ile `Rng.h` arasına) |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | değiştir | yalnızca bir `<ClCompile Include="ScriptTests.cpp" />` satırı (`RngTests.cpp`'den sonra) |

Dosya sayısı 4 (plan dosyası dahil 5). `GameServer/proj-GameServer.vcxproj*` **değişmez** (bu başlık sunucu projesine girmiyor). Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

### 5.1 Branch

`git switch -c bot/F4-19 gece/2026-10-02`. Planın `Durum` satırını `UYGULANIYOR` yap. `./tools/run-servers.sh status`'ta `[UP]` varsa `./tools/run-servers.sh stop` (exe kilidi, `AGENTS.md` §4).

### 5.2 Betik biçimi (sözleşme; ADR-0017 Eki F4-19)

Düz metin, satır tabanlı. Her **fiziksel satır** üçünden biridir:

- boş satır (yalnızca boşluk/sekme) — atlanır;
- yorum: ilk boşluk-olmayan karakter `#` — atlanır (satır **ortasındaki** `#` yorum **değildir**: `pchat` metni `#` içerebilir);
- adım: `<ofset_ms> <komut satırı>` — ör. `0 move BotWP_K 120 340`, `1500 attack BotWP_K BotWP_E 10`, `4000 snap BotMF_K`.

`<ofset_ms>`: betik başlangıcından itibaren milisaniye, yalnızca ondalık rakamlar (işaret, nokta, boşluk içi yok). `<komut satırı>`: ofsetten sonraki metin, baştaki/sondaki boşluk kırpılmış, **iç boşluklar aynen korunur**; ilk sözcüğü (verb) izinli listede olmalı.

İzinli verb'ler (küçük/büyük harf duyarsız, 20 adet): `move stop attack cast pot sit stand target regene pinvite paccept pdecline pleave ppromote pkick pchat see npcs snap list`. Yasak/bilinmeyen: `spawn despawn match scenario script` ve listede olmayan her şey.

Sınırlar (hepsi `constexpr`): toplam ≤ 8192 bayt, ≤ 128 fiziksel satır (yorum/boş dahil), ≤ 100 adım, satır ≤ 255 karakter (satır sonu hariç; CR/LF sayılmaz), ofset ≤ 600000 ms (10 dk), ofsetler **azalmayan** sırada (eşit olabilir; eşitlerde dosya sırası korunur). En az bir adım olmalı. Satırda `\t` dışında kontrol karakteri (0x00-0x1F, 0x7F) yok; ≥ 0x80 baytlar serbesttir (`pchat` metni). Satır sonu `\n` veya `\r\n`; `\r` yalnızca satır sonunda kabul edilir (satır ortasında kontrol karakteri sayılır). Dosya sonundaki satır `\n` ile bitmek zorunda değildir.

### 5.3 `BotCore/ScriptPlan.h`

Biçim: `BotMotion.h` gibi (`#pragma once`, `namespace BotCore`, tab, Allman, `inline`, İngilizce kısa yorum, ASCII + CRLF). Include'lar: `<cstddef>`, `<cstdint>`, `<string>`, `<vector>`. (Yükleme anında bir kez çalışan kod olduğundan `std::string`/`std::vector` serbesttir; `Perception.h`'in "sıcak yol" kısıtı burada geçerli değil.) Sunucu başlığı yok.

İskelet (adlar ve imzalar **bağlayıcıdır**; gövdeleri sen yaz):

```cpp
#pragma once

// Pure parser/validator for bot test scripts (ADR-0017 Ek F4-19). No I/O, no server headers.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BotCore
{
	constexpr size_t   kScriptMaxBytes    = 8192;
	constexpr size_t   kScriptMaxLines    = 128;     // physical lines, comments and blanks included
	constexpr size_t   kScriptMaxSteps    = 100;
	constexpr size_t   kScriptMaxLineLen  = 255;     // without the line terminator
	constexpr uint32_t kScriptMaxOffsetMs = 600000;

	enum ScriptError
	{
		SCRIPT_OK = 0,
		SCRIPT_ERR_TOO_LARGE,         // text longer than kScriptMaxBytes (line 0)
		SCRIPT_ERR_TOO_MANY_LINES,    // more than kScriptMaxLines physical lines (line 0)
		SCRIPT_ERR_EMPTY,             // no step at all (line 0)
		SCRIPT_ERR_LINE_TOO_LONG,
		SCRIPT_ERR_CONTROL_CHAR,
		SCRIPT_ERR_BAD_OFFSET,        // not 1..9 decimal digits
		SCRIPT_ERR_OFFSET_RANGE,      // digits ok but > kScriptMaxOffsetMs
		SCRIPT_ERR_OFFSET_ORDER,      // smaller than the previous step's offset
		SCRIPT_ERR_MISSING_COMMAND,   // offset only
		SCRIPT_ERR_BAD_VERB,          // verb not in the allowed list
		SCRIPT_ERR_TOO_MANY_STEPS     // the (kScriptMaxSteps + 1)th step
	};

	struct ScriptStep
	{
		uint32_t    offsetMs;
		std::string command;          // trimmed "<verb> [args]", inner spacing kept
		uint32_t    line;             // 1-based physical line the step came from
	};

	struct ScriptParseResult
	{
		ScriptError             error;      // SCRIPT_OK on success
		uint32_t                errorLine;  // 1-based; 0 for file-level errors
		std::vector<ScriptStep> steps;      // empty on any error
	};

	// Short English text for log lines, e.g. "bad verb". Never null.
	inline const char * ScriptErrorText(ScriptError error);

	// Case-insensitive match against the 20 allowed verbs (section 5.2). The argument is the verb alone.
	inline bool IsScriptVerb(const std::string & verb);

	// Parses a whole script text; the checks and their order are fixed by the plan (section 5.3).
	inline ScriptParseResult ParseScript(const std::string & text);
}
```

Ayrıntılı davranış (hepsi `ParseScript` içinde; hata durumunda `steps` **boşaltılır**, ilk hatada durulur):

1. `text.size() > kScriptMaxBytes` → `SCRIPT_ERR_TOO_LARGE`, `errorLine = 0`.
2. Metni `\n`'de böl. Son parça yalnızca boş değilse satırdır (sondaki `\n`'den sonraki boş parça satır **sayılmaz**). Satır sayısı > `kScriptMaxLines` → `SCRIPT_ERR_TOO_MANY_LINES`, `errorLine = 0` (satır başına denetimlerden **önce**). Bir satırın sonundaki tek `\r` silinir.
3. Satır `i` (1'den): uzunluk > 255 → `SCRIPT_ERR_LINE_TOO_LONG`; `\t` dışındaki bayt `< 0x20` veya `== 0x7F` (işaretsiz bayt olarak karşılaştır) → `SCRIPT_ERR_CONTROL_CHAR`. Her ikisinde `errorLine = i`. (Bu denetimler **yorum ve boş satırlar dahil** her satıra uygulanır.)
4. Satırı baş/son boşluk-sekme kırp; boş veya `#` ile başlıyorsa atla.
5. İlk boşluk/sekmeye kadar olan parça ofset belirtecidir: 1..9 karakter ve hepsi `0`-`9` değilse `SCRIPT_ERR_BAD_OFFSET`; değer (uint64 olarak hesapla) > `kScriptMaxOffsetMs` ise `SCRIPT_ERR_OFFSET_RANGE`; önceki adımın ofsetinden küçükse `SCRIPT_ERR_OFFSET_ORDER` (ilk adımda önceki = 0).
6. Belirteçten sonraki kısım kırpılır; boşsa `SCRIPT_ERR_MISSING_COMMAND`; ilk sözcüğü (boşluk/sekmeye kadar) `IsScriptVerb` değilse `SCRIPT_ERR_BAD_VERB`.
7. Adım sayısı zaten 100 iken yeni bir adım eklenecekse `SCRIPT_ERR_TOO_MANY_STEPS` (hata satırı = 101. adımın satırı). Aksi halde `ScriptStep{offset, command, i}` ekle.
8. Döngü sonunda adım yoksa `SCRIPT_ERR_EMPTY`, `errorLine = 0`.

`ScriptErrorText` her enum değeri için kısa İngilizce metin döner (`"ok"`, `"file too large"`, `"too many lines"`, `"no steps"`, `"line too long"`, `"control character"`, `"bad offset"`, `"offset out of range"`, `"offsets must not decrease"`, `"missing command"`, `"bad verb"`, `"too many steps"`); bilinmeyen değer için `"?"`.

`IsScriptVerb`: sabit 20 verb'lik küçük harfli dizi; karşılaştırmada ASCII küçük-harfe çevirme (locale'e bağımlı `tolower` kullanma; `c >= 'A' && c <= 'Z'` ise `+ 32`).

### 5.4 `Tests/BotCoreTests/ScriptTests.cpp` (altı test; adlar bağlayıcı)

Dosya başı: `#include "MiniTest.h"`, `#include <BotCore/ScriptPlan.h>`, `<string>`. Metinleri `std::string` olarak kur (satır sonları için `"\n"`/`"\r\n"` birleştir). Enum karşılaştırması `CHECK_EQ(int(r.error), int(BotCore::SCRIPT_...))`.

1. `Script_ParseValid`: `"# header\r\n\r\n0 move BotWP_K 120 340\r\n1500   ATTACK BotWP_K BotWP_E 10\n1500 pchat BotMF_K hello #1 world\n  4000\tsnap BotMF_K  \n"` → `error == OK`, **4 adım**. Adım 0: ofset 0, komut `"move BotWP_K 120 340"`, satır 3. Adım 1: ofset 1500, komut `"ATTACK BotWP_K BotWP_E 10"` (verb büyük harfle kalır), satır 4. Adım 2: ofset 1500, komut `"pchat BotMF_K hello #1 world"` (`#` korunur), satır 5. Adım 3: ofset 4000, komut `"snap BotMF_K"` (baştaki boşluklar ve sondaki boşluklar kırpılır, ofset belirteci sekmeyle de biter), satır 6. Ek olarak son satırı `\n`'siz olan `"0 list"` 1 adım döner.
2. `Script_VerbWhitelist`: 20 verb'in tamamı `IsScriptVerb` true (küçük harf), üçü (`"MOVE"`, `"Snap"`, `"PCHAT"`) büyük/karışık harfle true; `spawn`, `despawn`, `match`, `scenario`, `script`, `""`, `"move2"`, `"mov"`, `"list "` (sonda boşluk) false. Ek: `ParseScript("0 spawn BotWP_K")` → `SCRIPT_ERR_BAD_VERB`, `errorLine == 1`.
3. `Script_OffsetRules`: Aşağıdakiler `SCRIPT_ERR_BAD_OFFSET`: `"x move a 1 1"`, `"-5 stop all"`, `"5x stop all"`, `"1.5 stop all"`, `"1234567890 stop all"` (10 hane); `"600001 stop all"` → `OFFSET_RANGE`; `"600000 stop all"` → OK; `"100 stop all\n99 stop all"` → `OFFSET_ORDER`, `errorLine == 2`; `"100 stop all\n100 list"` → OK, 2 adım (eşit ofset serbest); `"0 stop all"` → OK (ofset 0 serbest).
4. `Script_LineRules`: `"100"` → `MISSING_COMMAND`; `"100   "` → `MISSING_COMMAND`; satırda `\x01` (ör. `"0 pchat a b\001c"`) → `CONTROL_CHAR`; **yorum satırında** `\001` (`"# a\001b\n0 list"`) → `CONTROL_CHAR`, `errorLine == 1`; (C++ dizgesinde onaltılık `\x01c` açgözlü okunur: **sekizlik `\001` kullan**.) Satır içinde `\t` serbest (`"0\tlist"` OK); satır ortasında `\r` (`"0 list\rx"`) → `CONTROL_CHAR`; uzunluk: `"0 pchat a " + std::string(245, 'x')` tam 255 karakter → OK, 256 karakter → `LINE_TOO_LONG` (satır sonu `\r\n` iken de 255 geçerli).
5. `Script_Limits`: `std::string(8193, '#')` → `TOO_LARGE`, `errorLine == 0`; tam 8192 baytlık geçerli metin → OK, 1 adım (127 yorum satırı, her biri `"#"` + 62 × `'-'` + `"\n"` = 64 bayt → 8128 bayt; ardından `\n`'siz son satır `"0 list"` + 58 boşluk = 64 bayt; toplam 8192 bayt ve 128 satır; **önce `CHECK_EQ(text.size(), size_t(8192))`**), aynı metne 1 bayt eklenince (`+ "x"`) `TOO_LARGE`; 129 yorum satırı (`"#\n"` × 129) → `TOO_MANY_LINES`, `errorLine == 0`; 128 yorum satırı → `EMPTY` (satır sayısı serbest, adım yok); yalnızca boş string → `EMPTY`; 100 adım (`"0 list\n"` × 100) → OK, 100 adım; 101 adım → `TOO_MANY_STEPS`, `errorLine == 101`, `steps.empty()`.
6. `Script_ErrorLineNumbers`: boş ve yorum satırları numaralamaya dahildir: `"# c\n\n0 list\n5 bogus x\n"` → `BAD_VERB`, `errorLine == 4`, `steps.empty()`; `"0 list\n\n\n7 move"` ... (`move` argümansız **geçerlidir**: argüman doğrulaması yok) → OK, 2 adım, ikinci adımın `line == 4`; `ScriptErrorText`'in 12 değerin hepsi için boş olmayan metin döndüğünü (ve `SCRIPT_OK` için `"ok"`) denetle.

Her test kendi içinde bağımsız; global durum yok.

### 5.5 `vcxproj` satırları

- `BotCore/BotCore.vcxproj`: `ClInclude` grubunda `Perception.h` ile `Rng.h` arasına `    <ClInclude Include="ScriptPlan.h" />`. Dosyanın mevcut girinti/CRLF düzenini koru (sadece bu satır eklenir; başka hiçbir satır değişmez).
- `Tests/BotCoreTests/BotCoreTests.vcxproj`: `ClCompile` grubunda `RngTests.cpp` satırından sonra `    <ClCompile Include="ScriptTests.cpp" />`.

### 5.6 Derleme ve test

```bash
./tools/run-tests.sh                  # derler + 82 test koşar
./tools/build.sh Release              # GameServer çözümü hâlâ derleniyor (bu plan sunucuya dokunmaz)
```

## 6. Kabul kriterleri

- [ ] K1: `./tools/run-tests.sh` çıktısının son satırı `82 tests, 0 failed`; dönüş kodu 0.
- [ ] K2: `grep -c '^TEST_CASE' Tests/BotCoreTests/ScriptTests.cpp` çıktısı `6`; `grep -o 'TEST_CASE("[A-Za-z_]*")' Tests/BotCoreTests/ScriptTests.cpp` altı satırı §5.4'tekilerle aynı: `Script_ParseValid`, `Script_VerbWhitelist`, `Script_OffsetRules`, `Script_LineRules`, `Script_Limits`, `Script_ErrorLineNumbers`.
- [ ] K3: `./tools/build.sh Release` hatasız biter (yeni uyarı yok).
- [ ] K4: `git diff --stat gece/2026-10-02...bot/F4-19` yalnızca şu dosyaları listeler: plan dosyası, `BotCore/ScriptPlan.h`, `BotCore/BotCore.vcxproj`, `Tests/BotCoreTests/ScriptTests.cpp`, `Tests/BotCoreTests/BotCoreTests.vcxproj` (5 dosya); `GameServer/` altında **hiçbir** dosya yok.
- [ ] K5: `grep -rl 'ScriptPlan' GameServer` boş (başlık yalnızca testten dahil ediliyor; `GameServer` bağlanmadı).
- [ ] K6: `BotCore/ScriptPlan.h` saf: `grep -nE '#include *[<"](windows|stdafx|winsock|\.\./)|fopen|FILE|printf|chrono|thread|mutex|malloc' BotCore/ScriptPlan.h` boş (yorumlarda da bu sözcükleri kullanma).
- [ ] K7: 20 verb: `ScriptPlan.h` içindeki verb dizisi tam olarak `move stop attack cast pot sit stand target regene pinvite paccept pdecline pleave ppromote pkick pchat see npcs snap list` içerir; `spawn`, `despawn`, `match`, `scenario`, `script` dizide yok: `grep -nE '"(spawn|despawn|match|scenario|script)"' BotCore/ScriptPlan.h` boş.
- [ ] K8: sınır sabitleri tam değerleriyle bulunur: `grep -nE 'kScriptMaxBytes += 8192|kScriptMaxLines += 128|kScriptMaxSteps += 100|kScriptMaxLineLen += 255|kScriptMaxOffsetMs += 600000' BotCore/ScriptPlan.h` beş satır.
- [ ] K9: yeni dosyalar ASCII + CRLF: `file BotCore/ScriptPlan.h Tests/BotCoreTests/ScriptTests.cpp` çıktısı `ASCII text, with CRLF line terminators`; `git diff` iki `vcxproj` için yalnızca birer eklenen satır gösterir.
- [ ] K10: bot sistemi davranışı: `GameServer/` farkı boş olduğundan `ENABLED=0` iken sunucu davranışı değişmez (K4 ile aynı kanıt).

## 7. Doğrulama komutları

```bash
./tools/run-tests.sh
./tools/build.sh Release
git diff --stat gece/2026-10-02...bot/F4-19
grep -c '^TEST_CASE' Tests/BotCoreTests/ScriptTests.cpp
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (yeni dosyalar ASCII + CRLF, tab, Allman). `vcxproj` dosyalarında mevcut CRLF/girinti korunur.
- `BotCore` kuralı (ADR-0016): `windows.h`, `stdafx.h`, `GameServer`/`shared` başlığı yok; yalnızca standart kütüphane. Yorumlar İngilizce.
- **Doğrulama uyarısı (F4-18 dersi):** biçim sözleşmesi gövde-düzeyinde birim testle sınanır; testlerdeki girdileri koddan türetme, §5.4'te yazılı sabit metinleri kullan.
- Satır/bayt sınırı testinde (§5.4 madde 5) tam 8192 bayt hedefi bir aritmetik tuzaktır: önce metni kur, `text.size()`'ı `CHECK_EQ` ile doğrula, sonra `ParseScript` sonucunu denetle. Sayı tutmuyorsa metni ayarla; **sabitleri değiştirme**.
- `ParseScript` hatada `steps`'i boşaltır; kısmi sonuç döndürme.
- Bu plan sonunda sunucu davranışı değişmez; sunucu çalıştırma gerekmez.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-19` (taban: `gece/2026-10-02`). Kod commit'i `59662a7` (`[F4-19] Betik ayrıştırıcı: BotCore/ScriptPlan.h ve 6 birim testi`); bu rapor ve `Durum: UYGULANDI` ayrı bir commit'tedir.
- Değişen dosyalar ve neden:
  - `BotCore/ScriptPlan.h` (yeni, 256 satır): sınır sabitleri, `ScriptError`, `ScriptStep`, `ScriptParseResult`, `ScriptTrim`, `IsScriptVerb`, `ScriptErrorText`, `ParseScript`; saf başlık, yalnızca standart kütüphane (`<cstddef>`, `<cstdint>`, `<string>`, `<vector>`).
  - `Tests/BotCoreTests/ScriptTests.cpp` (yeni, 233 satır): altı `TEST_CASE` (`Script_ParseValid`, `Script_VerbWhitelist`, `Script_OffsetRules`, `Script_LineRules`, `Script_Limits`, `Script_ErrorLineNumbers`).
  - `BotCore/BotCore.vcxproj` (+1): `Perception.h` ile `Rng.h` arasına `<ClInclude Include="ScriptPlan.h" />`.
  - `Tests/BotCoreTests/BotCoreTests.vcxproj` (+1): `RngTests.cpp`'den sonra `<ClCompile Include="ScriptTests.cpp" />`.
  - `plans/F4-19-betik-ayristirici.md`: `Durum` satırı ve bu rapor.
- Derleme/test sonucu:
  - `./tools/run-tests.sh` (Release): son satır `82 tests, 0 failed`, rc=0 (altı yeni test adı çıktıda).
  - `./tools/run-tests.sh Debug`: son satır `82 tests, 0 failed`, rc=0 (plan istemedi; ek güvence).
  - `./tools/build.sh Release`: rc=0; `BotCore.vcxproj`, `proj-GameServer.vcxproj`, `proj-AIServer.vcxproj`, `proj-LogInServer.vcxproj`, `BotCoreTests.vcxproj` üretildi. `ScriptPlan.h`/`ScriptTests.cpp` `touch` edilip yeniden derlendi, derleme günlüğünde `warning`/`error` 0 (K3).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔: `82 tests, 0 failed`, rc=0.
  - K2 ✔: `grep -c '^TEST_CASE'` = 6; altı ad §5.4 ile birebir.
  - K3 ✔: `build.sh Release` hatasız, yeni uyarı 0.
  - K4 ✔: `git diff --stat gece/2026-10-02...bot/F4-19` tam 5 dosya; `GameServer/` farkı boş.
  - K5 ✔: `grep -rl 'ScriptPlan' GameServer` boş.
  - K6 ✔: saflık grep'i boş (`windows|stdafx|winsock|../`, `fopen|FILE|printf|chrono|thread|mutex|malloc`).
  - K7 ✔: dizi tam 20 verb; `spawn|despawn|match|scenario|script` dizide yok, grep boş (Python ile tam eşleşme doğrulandı).
  - K8 ✔: beş sabit grep'i beş satır döndürdü (tek boşluklu `= değer` biçimi; aşağıdaki sapmaya bakınız).
  - K9 ✔: `file` iki yeni dosya için `ASCII text, with CRLF line terminators`; her `vcxproj` farkı tam bir eklenen satır (BOM/CRLF korundu).
  - K10 ✔: `GameServer/` farkı boş (K4 ile aynı kanıt); `ENABLED=0` davranışı değişmez.
- Plandan sapmalar ve gerekçeleri:
  - §5.3 iskeletindeki sabit satırlarında `=` işareti hizalıydı (ör. `kScriptMaxBytes    = 8192`), ancak K8 grep'i `kScriptMaxBytes += 8192` (tek boşluk + `=+`) biçiminde verildiğinden çok boşluklu hizalamayla eşleşmez. K8 kabul kriterini sağlamak için sabitlerde `=` öncesi tek boşluk kullanıldı (`kScriptMaxBytes = 8192`); adlar/imzalar ve değerler iskeletle aynı, yalnızca boşluk farkı. Başka sapma yok.
  - `ScriptTrim` ve `IsScriptVerb` yardımcıları için iskelette gövde yoktu; §5.3 davranış tanımına göre yazıldı (`ScriptTrim` ek genel yardımcı, `IsScriptVerb` imzası iskeletle aynı).
- Açık sorular: Yok. `BotCoreTests.vcxproj.filters` (varsa) güncellenmedi; plan §4 yalnızca iki `vcxproj`'u izinli listeye koyuyor ve filtre dosyası derlemeyi etkilemez.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1
