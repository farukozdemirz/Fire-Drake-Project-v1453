# F4-23: Algı sözleşmesi statik denetim aracı — `tools/check-perception-contract.py` (AC-LRN-03 / AC-ARCH-06, statik kısım)

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2) |
| Branch | `bot/F4-23` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | F4-12, F4-14, F4-16, F4-17, F4-18 (`Perception` dilimleri) — `KAPANDI`; F4-22 — `KAPANDI` (merge `1a42d6a`) |
| İlgili gereksinim / kabul | `docs/17` F4 "Test: AC-LRN-03 statik/çalışma zamanı denetimi" ve "Kabul: sözleşme dışı algı erişimi 0"; AC-LRN-03 (`docs/14` §14), AC-ARCH-06 (`docs/13` §14), REQ-FAIR-01 / REQ-NEW-08 (`docs/20`); ADR-0017 Eki F4-23 |
| Tahmini büyüklük | S (tek Python dosyası; sunucu koduna ve `BotCore`'a dokunmaz) |
| Hazırlayan / tarih | Claude / 2026-10-02 (otonom gece döngüsü) |

---

## 1. Amaç

F4'ün kabul koşulu "sözleşme dışı algı erişimi 0" şu an her `Perception` planında elle yazılmış `grep` satırlarıyla sınanıyor; tek bir komutla tüm bot kodunu denetleyen bir araç yok. Bu plan bittiğinde `python3 tools/check-perception-contract.py` bot kaynaklarını (`GameServer/Bot/`, `BotCore/`) tarar ve beş kuralı denetler: (R1) sunucunun NPC/bölge/party/oturum kayıt defterlerine hiç erişilmez, (R2) başka sunucu nesnesi okumaları yalnızca **adlandırılmış ve sayısı sabitlenmiş** istisnalarda olur, (R3) bir botun başka bir botun `CUser` nesnesini okuması yalnızca bilinen test sürücülerinde olur, (R4) `BotCore` sunucu başlığı içermez, (R5) `UnitView`/`NpcView`/`TeamMemberView` yapılarında yasaklı alan yoktur. Yeni, istisna listesinde olmayan bir erişim aracı **başarısız** yapar. Çalışma zamanı assert'i (AC-LRN-03'ün ikinci yarısı) bu planın dışındadır. Sunucu koduna dokunulmaz, bot sistemi ve sunucu davranışı değişmez.

## 2. Bağlam (okunması zorunlu)

- `docs/14_LEARNING_AND_ADAPTATION.md` §5.2 (gözlem sözleşmesi: bot, insan istemcisinin paketlerden öğreneceğinden fazlasını kullanmaz; yasak örnekleri: düşmanın tam MP'si, cooldown'u, envanteri, pot stoku, görüş alanı dışı konum) ve §14 AC-LRN-03 ("yasaklı gözlem alanlarına erişim denemesi 0, statik kontrol + çalışma zamanı assert").
- `docs/13_BOT_ARCHITECTURE_AND_DATA_MODEL.md` §2 (`Perception`), AC-ARCH-06.
- `docs/adr/ADR-0017-aksiyon-yurutucu-ve-adalet-korumasi.md` Eki F4-12/F4-14/F4-16/F4-18 (algının tek kaynağı alınan paketler, sunucu dizilerine dokunulmaz), Ek (F4-02) madde 4 ("hedef girdisi geçicidir": `/bot attack` vb. test komutları hedef botun konumunu/kimliğini kendi oturumundan okur), Ek F4-23 (bu planın kararı). `docs/adr/ADR-0016` (`BotCore` saf, sunucu başlığı yok).
- Önceki planlardaki elle sözleşme grep'leri: `plans/F4-16-algi-anlik-goruntu.md` K5, `plans/F4-18-algi-takim-gorunumu.md` K5 (bu araç onların yerini alır, onları değiştirmez).
- **Kod gerçekleri (Claude'un geçici bir prototipiyle `/tmp`'de ölçtü, commit `1a42d6a`; satırlar kayabilir, sayılar sizin aracınızın çıktısıyla aynı olmalıdır):**
  - Tarama kümesi: `GameServer/Bot/*.cpp|*.h` (13 dosya: `ActionExecutor`, `BotManager`, `BotSession`, `ScenarioRunner`, `ScriptRunner`, `Telemetry` × `.cpp/.h`, `IBotSink.h`) + `BotCore/*.cpp|*.h` (6 dosya: `BotCombat.h`, `BotMotion.h`, `Perception.h`, `Rng.cpp`, `Rng.h`, `ScriptPlan.h`) = **19 dosya**. Dosyalar CRLF + tab; bazıları ISO-8859 olabilir.
  - R1 (sıfır hoşgörü) sembolleri depodaki gerçek adlardır (`GameServer/GameServerDlg.h:306,352,368,335`, `GameServer/Region.h:20-21`, `GameServer/Map.h:27`, `GameServer/Npc.h:6`): `GetNpcPtr`, `m_arNpcArray`, `m_RegionUserArray`, `m_RegionNpcArray`, `GetRegion`, `FindNpcInZone`, `CNpc`, `m_PartyArray`, `GetPartyPtr`. Bot kodunda şu an **0 isabet**.
  - R2 sembolleri ve gerçek isabetler (28 isabet, 15 istisna girdisi; işlev adı aracın bulduğu addır):
    - `GetMap`: `ActionExecutor::BeginMove` ×2 (`ActionExecutor.cpp:205-206`, kendi haritası, adım geçerlilik denetimi).
    - `GetItem`: `CountInBag` ×1 (`ActionExecutor.cpp:968`, kendi çantası, CLI-06); `FillSelfExtras` ×1 (`BotManager.cpp:2426`).
    - `isInParty`: `ActionExecutor::RequestPartyInvite` ×1 (`:1847`), `ActionExecutor::RequestPartyLeave` ×1 (`:2231`), `RequestPartyManage` ×1 (`:2390`, dosya-statik işlev, sınıf öneki yok), `ActionExecutor::RequestChatParty` ×1 (`:2588`), `FillSelfExtras` ×1 (`BotManager.cpp:2469`), `IsSamePartyMember` ×2 (`BotManager.cpp:2098`).
    - `GetPartyID`: `IsSamePartyMember` ×2 (`BotManager.cpp:2099`).
    - `m_buffMap`: `FillSelfExtras` ×1 (`BotManager.cpp:2462`).
    - `GetUserPtr`: `BotManager::StartSession` ×2 (`BotManager.cpp:3244,3250`), `BotManager::PollDespawn` ×2 (`:3203-3204`); yalnızca ad kayıtlı mı diye `nullptr` karşılaştırması, veri okunmaz.
    - `GetActiveSessionMap` ×6 ve `GetIdleSessionMap` ×4: `BotManager::Startup` (`BotManager.cpp:221-222,255,288-289`; havuz öz-sınaması, yalnızca rezerve edilmiş kimliklerin üyeliği).
    - `m_CoolDownList`, `m_sItemArray`: 0 isabet (listede kalır; yeni kullanım istisna ister).
  - R3 (başka bot oturumunun `CUser`'ı): 18 isabet, hepsi `BotManager.cpp`: `BotManager::CommandPartyInvite` ×3, `BotManager::CommandPartyManage` ×1, `BotManager::CommandTarget` ×3, `BotManager::TickSessions` ×9 (`/bot attack`/`cast` hedef girdisi, `:2958-3031`), `IsSamePartyMember` ×2. Hepsi test sürücüsüdür (ADR-0017 Ek F4-02 madde 4).
  - R4: `BotCore/` dosyalarının `#include`'ları şu an yalnızca standart başlıklar (`<cstdint>`, `<cmath>`, `<vector>`…) ve `"Rng.h"`; ihlal 0.
  - R5: `BotCore/Perception.h` içinde `struct UnitView` (13 alan: `id nation race cls level x z dist dead sitting partyLeader invisibility ageMs`), `struct NpcView` (11 alan: `id protoId type nation level x z dist dead gateOpen ageMs`), `struct TeamMemberView` (16 alan: `id nation level cls hp maxHp mp maxMp leader dead inView x z dist ageMs name`); yapılar `\t};` ile biter. İhlal 0.
  - Beklenen gerçek-ağaç çıktısı (K2): `R1 0/0`, `R2 0/28`, `R3 0/18`, `R4 0/0`, `R5 0/0`, `RESULT: PASS`.

## 3. Kapsam

**Yapılacaklar**

- Tek dosya: `tools/check-perception-contract.py` (yalnızca standart kütüphane, ASCII, LF).
- Beş kural (R1-R5), istisna tabloları kod içinde sabit veri olarak, bir `--selftest`, bir `--json` çıktısı.
- Çıkış kodu: 0 = PASS, 1 = en az bir ihlal, 2 = kullanım/giriş hatası.

**Kapsam dışı (yapılmayacak)**

- `GameServer/`, `BotCore/`, `AIServer/`, `shared/`, `Tests/`, `docs/`, `tools/` altındaki **başka** dosyalara dokunmak; bot kodunda bulunan bir "sorunu" düzeltmek; istisna tablosunu, beklenen sayıları ya da kuralları gerçek ağaçta sonuç çıksın diye değiştirmek (sonuç planla uyuşmazsa **dur**, §8'e bak).
- Çalışma zamanı assert'i (AC-LRN-03'ün ikinci yarısı); paket/telemetri incelemesi; gözlem alanlarının anlamsal doğruluğu (hangi bilgi hangi paketle gelir: `docs/03` §16).
- Sunucuyu açmak, `.ini` düzenlemek, DB'ye bağlanmak, derleme sistemine (`.vcxproj`) ya da `tools/run-tests.sh`'a bu aracı bağlamak.
- C++ ayrıştırıcısı yazmak: araç satır tabanlıdır (yorum/dizge ayıklama + işlev başlığı sezgisi); bunun ötesine geçme.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/check-perception-contract.py` | yeni | tek kod dosyası; ASCII, LF |
| `plans/F4-23-algi-sozlesme-denetim-araci.md` | değiştir | yalnızca `Durum` satırı ve Uygulayıcı Raporu |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

**Sabitler (dosya başı):**

```python
SCAN_DIRS = ("GameServer/Bot", "BotCore")
SCAN_EXT = (".cpp", ".h")
VIEW_FILE = "BotCore/Perception.h"
R1_SYMBOLS = ("GetNpcPtr", "m_arNpcArray", "m_RegionUserArray", "m_RegionNpcArray", "GetRegion",
              "FindNpcInZone", "CNpc", "m_PartyArray", "GetPartyPtr")
R2_SYMBOLS = ("GetUserPtr", "GetMap", "GetItem", "m_buffMap", "isInParty", "GetPartyID",
              "m_CoolDownList", "m_sItemArray", "GetActiveSessionMap", "GetIdleSessionMap")
R3_RE = re.compile(r"\b(?!s\b)\w+(?:->|\.)m_pUser\b")     # any base name except the bot's own session "s"
FUNC_RE = re.compile(r"^(?:[A-Za-z_~][\w\s\*&:<>,~]*?\s)?((?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*)\s*\(")
NOT_FUNC = ("if", "for", "while", "switch", "return", "else", "case", "do", "catch", "sizeof")
INC_RE = re.compile(r"^\s*#\s*include\s*([<\"])([^>\"]+)[>\"]")
FIELD_RE = re.compile(r"(\w+)\s*(?:\[[^\]]*\])?\s*(?:=[^;,]*)?\s*[;,]")
WORD_RE = re.compile(r"[A-Z]+(?![a-z])|[A-Z]?[a-z]+|\d+")
```

İstisna tabloları (bu veriyi **aynen** yaz; `(dosya, işlev, simge, azami adet, gerekçe)`):

```python
ACT = "GameServer/Bot/ActionExecutor.cpp"
BOT = "GameServer/Bot/BotManager.cpp"
OWN_PARTY = "own party flag, guard input"
NAME_REG = "name still registered? null compare only, no data read"
SELF = "own state (SelfState), read in the IOCP thread"
POOL = "pool self-test: membership of the reserved slot ids only (F2-01)"
ALLOW_R2 = [
    (ACT, "ActionExecutor::BeginMove", "GetMap", 2, "own map: IsValidPosition check of the next step (static terrain)"),
    (ACT, "CountInBag", "GetItem", 1, "own bag stock for CLI-06"),
    (ACT, "ActionExecutor::RequestPartyInvite", "isInParty", 1, OWN_PARTY),
    (ACT, "ActionExecutor::RequestPartyLeave", "isInParty", 1, OWN_PARTY),
    (ACT, "RequestPartyManage", "isInParty", 1, OWN_PARTY),
    (ACT, "ActionExecutor::RequestChatParty", "isInParty", 1, OWN_PARTY),
    (BOT, "BotManager::PollDespawn", "GetUserPtr", 2, NAME_REG),
    (BOT, "BotManager::StartSession", "GetUserPtr", 2, NAME_REG),
    (BOT, "FillSelfExtras", "GetItem", 1, SELF),
    (BOT, "FillSelfExtras", "isInParty", 1, SELF),
    (BOT, "FillSelfExtras", "m_buffMap", 1, SELF),
    (BOT, "IsSamePartyMember", "isInParty", 2, "guard input from two bot sessions (ADR-0017 Eki F4-08/F4-10)"),
    (BOT, "IsSamePartyMember", "GetPartyID", 2, "guard input from two bot sessions (ADR-0017 Eki F4-08/F4-10)"),
    (BOT, "BotManager::Startup", "GetActiveSessionMap", 6, POOL),
    (BOT, "BotManager::Startup", "GetIdleSessionMap", 4, POOL),
]
TEST_DRIVER = "test driver: reads another bot's id/position from its session (ADR-0017 Ek F4-02 item 4); replaced when Perception feeds targets"
ALLOW_R3 = [
    (BOT, "BotManager::CommandPartyInvite", "m_pUser", 3, TEST_DRIVER),
    (BOT, "BotManager::CommandPartyManage", "m_pUser", 1, TEST_DRIVER),
    (BOT, "BotManager::CommandTarget", "m_pUser", 3, TEST_DRIVER),
    (BOT, "BotManager::TickSessions", "m_pUser", 9, TEST_DRIVER),
    (BOT, "IsSamePartyMember", "m_pUser", 2, TEST_DRIVER),
]
VIEW_FORBIDDEN = {   # struct name -> forbidden words (lower case) in field names
    "UnitView": ("hp", "mp", "name", "cooldown", "stock", "inventory", "invent", "buff", "skill", "item", "potion"),
    "NpcView": ("hp", "mp", "name", "cooldown", "stock", "inventory", "invent", "buff", "skill", "item", "potion"),
    "TeamMemberView": ("cooldown", "stock", "inventory", "invent", "buff", "skill", "item", "potion"),
}
```

1. **Okuma ve ayıklama.** Dosyayı `open(path, "rb").read().decode("latin-1")` ile oku, `"\n"` ile böl, her satırdan `"\r"` sonunu at (ISO-8859 dosyalar çökmesin). `strip_code(line, in_block)` → `(code, in_block)`: dizge (`"..."`) ve karakter (`'...'`) sabitlerini tek boşlukla değiştirir (içinde `\` kaçışı atlanır), `//` satır sonuna kadar yorumu atar, `/* ... */` yorumunu satırlar arasında `in_block` bayrağıyla atar. Bu fonksiyonun gövdesi şöyledir (aynen kullanabilirsin):

   ```python
   def strip_code(line, in_block):
       out = []
       i = 0
       n = len(line)
       while i < n:
           c = line[i]
           if in_block:
               j = line.find("*/", i)
               if j < 0:
                   return "".join(out), True
               i = j + 2
               in_block = False
               continue
           two = line[i:i + 2]
           if two == "//":
               break
           if two == "/*":
               in_block = True
               i += 2
               continue
           if c == '"' or c == "'":
               i += 1
               while i < n and line[i] != c:
                   i += 2 if line[i] == "\\" else 1
               i += 1
               out.append(" ")
               continue
           out.append(c)
           i += 1
       return "".join(out), in_block
   ```

2. **İşlev ataması (`scan_file(path, rel)`).** Satır satır dolaş, `cur = "(file)"` ile başla. Ayıklanmış `code` boş değilse, **ilk karakteri boşluk değilse** ve harf/`_`/`~` ile başlıyorsa ve `;` ile bitmiyorsa `FUNC_RE.match(code)` dene; eşleşirse ve `group(1)` `NOT_FUNC` içinde değilse `cur = group(1)`. (Girintili satırlar işlev başlığı sayılmaz; sınıf içi satır içi tanımlar bu yüzden son sütun-0 işlevine yazılır, `.h` dosyalarında `(file)` olur.) Aynı `code` üzerinde: her `R1_SYMBOLS`/`R2_SYMBOLS` elemanı için `re.finditer(r"\b" + re.escape(sym) + r"\b", code)` her eşleşmede bir isabet `(satır_no, kural, simge, cur)` ekle (`R1`/`R2`); `R3_RE.finditer(code)` her eşleşmede `(satır_no, "R3", "m_pUser", cur)`. Dönüş: isabet listesi.

3. **Denetim (`audit(root, allow2, allow3)`; tablolar parametre olsun ki selftest kendi tablosunu verebilsin).**
   - `SCAN_DIRS` altındaki dosyaları `sorted(os.listdir(...))` ile topla (`SCAN_EXT` ile biten); iki dizin de yoksa `exit 2` (`error: no scan directories under <root>`); `files` = taranan dosya sayısı.
   - **R1:** her isabet ihlaldir (`why="forbidden"`).
   - **R2/R3 eşleme:** isabetleri `(dosya, işlev, simge)` ile grupla (R3 için simge `m_pUser`). Grup için tabloda girdi yoksa tüm isabetler ihlal (`why="not allowlisted"`). Girdi varsa satır sırasına göre ilk `azami` isabet izinli sayılır (izinli sayacı artar), kalanlar ihlal (`why="exceeds allowlist (<adet> > <azami>)"`).
   - **Bayat girdi:** girdinin gerçek isabeti azami adetten azsa `stale` listesine `(girdi, isabet)` ekle. Bilgi amaçlıdır, **ihlal sayılmaz**, çıkış kodunu etkilemez.
   - **R4:** yalnızca `BotCore/` dosyalarında, **ham** satırlarda `INC_RE.match`. `<ad>` biçimi `^[a-z_0-9]+$` ile eşleşmiyorsa (ör. `<windows.h>`, `<stdint.h>`) ihlal; `"ad"` biçimi `^[A-Za-z0-9_]+\.h$` ile eşleşmiyorsa (ör. içinde `/` ya da `\`) ya da `stdafx.h` (büyük/küçük harf fark etmez) ise ihlal. İhlal metni: `R4 include <kind><name>`.
   - **R5:** `VIEW_FILE` yoksa tek ihlal `BotCore/Perception.h:0: R5 file not found`. Varsa her `VIEW_FORBIDDEN` yapısı için `struct_fields(lines, ad)`: `^\s*struct\s+AD\b(?!\s*;)` satırını bul (ayıklanmış kodda), ondan sonra `{`/`}` sayarak derinliği izle (yapı satırının kendisindeki `{` sayılır; açılış çoğu zaman sonraki satırdadır: ilk `{` görülene kadar derinlik 0'dır ve **durulmaz**, durma yalnızca bir `}` derinliği 0'a indirdiğinde olur), derinlik **tam 1** iken her ayıklanmış satırda `FIELD_RE.finditer` ile alan adlarını topla (`(satır_no, ad)`), derinlik 0'a inince dur. Yapı bulunamazsa `BotCore/Perception.h:0: R5 <Struct> struct not found` ihlali. Her alan adını `WORD_RE.findall` ile kelimelere böl (`maxHp` → `max`, `hp`; `playerName` → `player`, `name`), küçük harfe çevir; yasak kümeyle kesişim varsa ihlal: `<file>:<satır>: R5 <Struct>.<alan> (forbidden word: <kelimeler, virgülle, sıralı>)`.
   - Sonuç: ihlal listesi `(dosya, satır, kural, simge, işlev, why)` ile `(dosya, satır, kural, simge)` sırasına dizilir; kural başına `violations` ve `allowlisted` sayıları; izinli girdiler `(kural, dosya, işlev, simge, isabet, azami, gerekçe)`.

4. **Çıktı.** Varsayılan (metin, ASCII):

   ```
   Perception contract audit (AC-LRN-03 / AC-ARCH-06, static part)
   files scanned: 19

   | rule | description | violations | allowlisted |
   |---|---|---|---|
   | R1 | forbidden registry symbols | 0 | 0 |
   | R2 | restricted symbols (own state, static data, pool) | 0 | 28 |
   | R3 | cross-session CUser reads (test drivers) | 0 | 18 |
   | R4 | BotCore purity (includes) | 0 | 0 |
   | R5 | view struct fields | 0 | 0 |

   RESULT: PASS
   ```

   Ardından (her zaman): `## Allowlisted` altında `| rule | file | function | token | hits | max | reason |` tablosu (sıra: kural, dosya, işlev, simge); ihlal varsa `## Violations` altında satır başına `<dosya>:<satır>: <kural> <simge> in <işlev> (<why>)` (R4/R5 için yukarıdaki biçim); bayat girdi varsa `## Stale allowlist entries` altında satır başına `<dosya> <işlev> <simge> hits <n> < max <m>`; son satır: `Not covered: runtime assert (AC-LRN-03 second half) and the semantic check of observation fields (docs/03 sec. 16).` `RESULT: PASS` ihlal 0 iken, `RESULT: FAIL` aksi halde. `--json`: `json.dumps(..., sort_keys=True, indent=2)` ile `{"allowlisted": [...], "files": n, "result": "PASS|FAIL", "rules": {"R1": {"allowlisted": a, "violations": v}, ...}, "stale": [...], "violations": [...]}` (kök yolu yazılmaz, çıktı belirlenimli). Kullanım: `check-perception-contract.py [--root DIR] [--json] [--selftest]`; `--root` varsayılanı `os.path.dirname(os.path.dirname(os.path.abspath(__file__)))`. Bilinmeyen seçenek/eksik değer: kullanım satırı ve çıkış kodu 2.

5. **Selftest (`--selftest`).** Geçici dizinde (`tempfile.mkdtemp`, sonunda `shutil.rmtree`) küçük sahte ağaçlar kur (dosyaları **CRLF** ile yaz), `audit`'i kendi küçük tablolarıyla çağır; sonda `selftest OK` yaz, çıkış kodu 0. Her vakada beklenen sayıları `assert` et:
   - **V1 (temiz):** `GameServer/Bot/T.cpp` = `void Foo::Bar(int a)` / `{` / `\treturn;` / `}`; `BotCore/Perception.h` üç yapıyı (`UnitView` {`uint16_t id; float x, z;`}, `NpcView` {`uint16_t id;`}, `TeamMemberView` {`int32_t hp, maxHp; char name[24];`}, her biri ayrı satırda `\tstruct X` / `\t{` / alanlar / `\t};`) temiz içerir; `BotCore/X.h` = `#include <cstdint>` ve `#include "Rng.h"` → ihlal 0, izinli 0, `files == 3`.
   - **V2 (R1, yorum/dizge/blok yorum yok sayılır):** `Foo::Bar` içine `g_pMain->GetNpcPtr(1);` → tam 1 ihlal, kural `R1`, işlev `Foo::Bar`. Sonra aynı yere yalnızca `// GetNpcPtr(1)`, `const char * t = "CNpc";` ve `/* m_PartyArray` / `GetPartyPtr */` (iki satırlık blok yorum) koy → ihlal 0.
   - **V3 (R2 istisna mantığı):** `allow2 = [("GameServer/Bot/T.cpp", "Foo::Bar", "GetUserPtr", 1, "x")]`. Bir `GetUserPtr(1);` → ihlal 0, izinli 1. İki çağrı → ihlal 1 (`exceeds allowlist (2 > 1)`), izinli 1. Çağrı `Foo::Baz` adlı başka bir işlevde → ihlal 1 (`not allowlisted`). Hiç çağrı yok → ihlal 0, `stale` 1 girdi, `RESULT: PASS`. Dosya-statik işlev: `static int Helper(CUser * u)` başlığı altındaki çağrının işlevi `Helper` olarak atanır; iki satırlık başlıkta (`static int Helper(CUser * u,` + girintili ikinci satır) da aynı.
   - **V4 (R3):** `Foo::Bar` içinde `s->m_pUser->GetX();` → ihlal 0 (kendi oturumu); `t->m_pUser->GetX();` → 1 ihlal (`R3`); `allow3 = [("GameServer/Bot/T.cpp", "Foo::Bar", "m_pUser", 1, "x")]` ile → ihlal 0, izinli 1; iki `t->m_pUser` ile → ihlal 1.
   - **V5 (R4):** `BotCore/X.h` içine sırayla `#include <windows.h>`, `#include "stdafx.h"`, `#include "../GameServer/Bot/Foo.h"`, `#include <stdint.h>` → her biri ayrı ihlal (toplam 4); `// #include <windows.h>` (yorum satırı) ihlal sayılmaz.
   - **V6 (R5):** `UnitView`'e `int32_t hp;` → 1 ihlal (kelime `hp`); `NpcView`'e `char playerName[24];` → 1 ihlal (`name`); `TeamMemberView`'e `uint8_t buffCount;` → 1 ihlal (`buff`), `name`/`hp`/`maxHp` ise ihlal değil; `Perception.h`'den `NpcView` yapısını sil → `struct not found` ihlali; `Perception.h` hiç yok → `file not found` ihlali.
   - **V7 (çıktı ve belirlenim):** V2'nin ihlalli ağacında `--json` çıktısı (`render_json(...)` ya da eşdeğer saf işlev) iki kez üretilince birebir aynı, `json.loads` ile açılınca `result == "FAIL"` ve `violations` bir liste; metin çıktısında `RESULT: FAIL` ve `GameServer/Bot/T.cpp:<satır>: R1 GetNpcPtr in Foo::Bar` satırı geçer.

6. **Gerçek ağaçta çalıştır** (§7) ve çıktıyı Uygulayıcı Raporu'na ekle. Sonuç §2'deki beklenen çıktıyla (`R2 0/28`, `R3 0/18`, `RESULT: PASS`, `files scanned: 19`) **aynı değilse** istisna tablosunu ya da kuralları değiştirme: farkı (hangi dosya:satır, hangi işlev adı) Uygulayıcı Raporu'nun "Açık sorular"ına yaz ve dur.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/check-perception-contract.py --selftest` `selftest OK` basar, çıkış kodu 0 (V1-V7).
- [ ] K2: `python3 tools/check-perception-contract.py` gerçek ağaçta çıkış kodu 0 ve çıktıda şu satırlar tam olarak geçer: `files scanned: 19`, `| R1 | forbidden registry symbols | 0 | 0 |`, `| R2 | restricted symbols (own state, static data, pool) | 0 | 28 |`, `| R3 | cross-session CUser reads (test drivers) | 0 | 18 |`, `| R4 | BotCore purity (includes) | 0 | 0 |`, `| R5 | view struct fields | 0 | 0 |`, `RESULT: PASS`; `## Stale allowlist entries` bölümü yok.
- [ ] K3: Enjeksiyon (§7'deki komutlar): bot kaynaklarının geçici kopyasına eklenen `g_pMain->GetNpcPtr(1);` satırı için araç çıkış kodu 1 verir ve çıktıda `GameServer/Bot/BotManager.cpp:<satır>: R1 GetNpcPtr in Injected::Test (forbidden)` geçer; aynı kopyaya eklenen ikinci enjeksiyon (`Injected::Test` içinde `g_pMain->GetUserPtr(1);`) için `R2 GetUserPtr in Injected::Test (not allowlisted)` geçer; üçüncü enjeksiyon (`t->m_pUser->GetX();`) için `R3 m_pUser in Injected::Test (not allowlisted)`.
- [ ] K4: `python3 tools/check-perception-contract.py --json` geçerli JSON (`python3 -c 'import json,sys; d=json.load(sys.stdin); print(d["result"], d["files"], sorted(d))'` → `PASS 19 ['allowlisted', 'files', 'result', 'rules', 'stale', 'violations']`); iki ardışık `--json` çıktısı `cmp` ile aynı.
- [ ] K5: `## Allowlisted` tablosunda tam 20 satır (15 R2 + 5 R3) ve her satırda `hits` = `max` (bayat girdi yok); R2 satırlarının `hits` toplamı 28, R3'ünki 18.
- [ ] K6: `file tools/check-perception-contract.py` "ASCII text" (CRLF yok, ASCII dışı karakter yok); `python3 -m py_compile` hatasız; yalnızca standart kütüphane `import`'ları (`json`, `os`, `re`, `shutil`, `sys`, `tempfile`).
- [ ] K7: `git diff --stat gece/2026-10-02...bot/F4-23` yalnızca `tools/check-perception-contract.py` ve bu plan dosyasını gösterir; `git diff --stat gece/2026-10-02...bot/F4-23 -- GameServer BotCore Tests shared AIServer docs` boş.
- [ ] K8: `tools/build.sh Release` hatasız biter (yeni uyarı yok; sunucu/BotCore değişmediği için sonuç aynı) ve `./tools/run-tests.sh` `82 tests, 0 failed` basar.

## 7. Doğrulama komutları

```bash
python3 -m py_compile tools/check-perception-contract.py
python3 tools/check-perception-contract.py --selftest; echo "rc=$?"
python3 tools/check-perception-contract.py; echo "rc=$?"
python3 tools/check-perception-contract.py --json | python3 -c 'import json,sys; d=json.load(sys.stdin); print(d["result"], d["files"], sorted(d))'
python3 tools/check-perception-contract.py --json > /tmp/f4-23-a.json; python3 tools/check-perception-contract.py --json > /tmp/f4-23-b.json; cmp /tmp/f4-23-a.json /tmp/f4-23-b.json && echo same
file tools/check-perception-contract.py

# Enjeksiyon denemesi (kopya /tmp'de, depoya yazılmaz)
rm -rf /tmp/f4-23-tree && mkdir -p /tmp/f4-23-tree/GameServer /tmp/f4-23-tree/BotCore
cp -r GameServer/Bot /tmp/f4-23-tree/GameServer/Bot && cp BotCore/*.h BotCore/*.cpp /tmp/f4-23-tree/BotCore/
printf '\nvoid Injected::Test()\n{\n\tg_pMain->GetNpcPtr(1);\n\tg_pMain->GetUserPtr(1);\n\tt->m_pUser->GetX();\n}\n' >> /tmp/f4-23-tree/GameServer/Bot/BotManager.cpp
python3 tools/check-perception-contract.py --root /tmp/f4-23-tree; echo "rc=$?"
rm -rf /tmp/f4-23-tree

./tools/build.sh Release
./tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F4-23
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `tools/*.py` dosyaları ASCII, LF; kod içindeki metinlerde Türkçe karakter yok.
- Yalnızca Python standart kütüphanesi; Python 3.8+ uyumlu (`match` ifadesi, `X | Y` tip yazımı, `dict | dict` yok).
- Kaynak dosyaları **yalnızca oku** ve `latin-1` ile çöz; hiçbir dosyayı değiştirme. Araç depoda yalnızca `tools/` altına yazılır, geçici dosyalar `/tmp`'de.
- İstisna tabloları ADR-0017 Eki F4-23'ün parçasıdır: bir istisnayı **sen** eklemez, silmez, azami adedini değiştirmezsin. Gerçek ağaçta plandaki sayılardan farklı bir sonuç çıkarsa (ör. işlev adı sezgisi başka bir ad bulur) bu bir plan/sezgi uyuşmazlığıdır: kuralı/tabloyu uydurma, farkı rapora yaz ve dur.
- İşlev adı sezgisi kasıtlı olarak basittir (sütun-0 başlık satırı). Daha akıllı bir ayrıştırıcı (parantez dengeleme, şablon, makro açma) **yazma**.
- Kişisel veri: araç yalnızca kaynak kodu okur; DB, log ya da telemetri dosyası açmaz.
- Çalışan sunucu gerekmez; `tools/run-servers.sh` çağırma.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F4-23` (taban: `gece/2026-10-02`) — `157b631 [F4-23] Algi sozlesmesi statik denetim araci: bes kural (R1-R5), selftest ve JSON ciktisi`
- Değişen dosyalar ve neden:
  - `tools/check-perception-contract.py` (yeni): tek Python dosyası; `GameServer/Bot/` + `BotCore/` taraması, R1-R5 kuralları, `--selftest` (V1-V7), `--json`, çıkış kodu 0/1/2. Yalnızca standart kütüphane, ASCII, LF.
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\GameServer\UpgradeHandler.cpp(634): warning C4789: ... [C:\...\proj-GameServer.vcxproj]
  C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\GameServer\UpgradeHandler.cpp(862): warning C4789: ... [C:\...\proj-GameServer.vcxproj]
    All 14017 functions were compiled because no usable IPDB/IOBJ from previous compilation was found.
    Kodun üretilmesi tamamlandı
    proj-GameServer.vcxproj -> C:\Users\frkoz\OneDrive\Desktop\Fire-Drake-Project-v1453\build\bin\x86-Release\Server\GameServer.exe
  ```
  `rc=0`; yeni uyarı yok (iki C4789 mesajı eski `UpgradeHandler.cpp` satırlarından, `GameServerDlg.cpp(1802)` C4267 da eski). Yalnızca Python eklenip C++ kaynağı değişmedi.
- `./tools/run-tests.sh` son satırı: `82 tests, 0 failed` (`rc=0`).
- Kabul kriterleri öz-değerlendirme:
  - K1 ✔: `--selftest` → `selftest OK`, `rc=0` (V1-V7; temiz ağaç, yorum/dizge/blok yorum yok sayma, R2 istisna/taşma/başka işlev/bayat/dosya-statik, R3 kendi oturumu/başkası/izin/taşma, R4 dört biçim + yorum, R5 alan/yapı yok/dosya yok, metin+JSON belirlenim).
  - K2 ✔: `rc=0`; `files scanned: 19`, `| R1 | ... | 0 | 0 |`, `| R2 | ... | 0 | 28 |`, `| R3 | ... | 0 | 18 |`, `| R4 | ... | 0 | 0 |`, `| R5 | ... | 0 | 0 |`, `RESULT: PASS`; `## Stale allowlist entries` yok.
  - K3 ✔: enjeksiyon kopyasında `rc=1`; `GameServer/Bot/BotManager.cpp:3301: R1 GetNpcPtr in Injected::Test (forbidden)`, `:3302: R2 GetUserPtr in Injected::Test (not allowlisted)`, `:3303: R3 m_pUser in Injected::Test (not allowlisted)`.
  - K4 ✔: `PASS 19 ['allowlisted', 'files', 'result', 'rules', 'stale', 'violations']`; iki ardışık `--json` çıktısı `cmp` ile aynı.
  - K5 ✔: `## Allowlisted` tam 20 satır (15 R2 + 5 R3), her satırda `hits` = `max`; R2 toplamı 28, R3 toplamı 18.
  - K6 ✔: `file` → `Python script, ASCII text executable` (CRLF yok); `python3 -m py_compile` hatasız; import'lar yalnızca `json, os, re, shutil, sys, tempfile`.
  - K7 ✔: commit sonrası fark yalnızca `tools/check-perception-contract.py` ve bu plan dosyası; `-- GameServer BotCore Tests shared AIServer docs` boş.
  - K8 ✔: `tools/build.sh Release` `rc=0`, yeni uyarı yok; `run-tests.sh` `82 tests, 0 failed`.
- Plandan sapmalar ve gerekçeleri:
  - Yok. Sabitler, istisna tabloları ve çıktı biçimi planla birebir; gerçek ağaç çıktısı §2'deki beklenen değerlerle aynı (`R1 0/0`, `R2 0/28`, `R3 0/18`, `R4 0/0`, `R5 0/0`, `files 19`, `RESULT: PASS`).
  - İki küçük uygulama ayrıntısı (plandaki ifadeyi birebir izler, karar değil): işlev başlığı kapısında `;` denetimi `code.rstrip()` üzerinden yapıldı; `--root` hem boşluklu hem `--root=DIR` biçimini kabul eder.
- Açık sorular: Yok. Çalışma zamanı assert'i (AC-LRN-03'ün ikinci yarısı) ve gözlem alanlarının anlamsal denetimi (`docs/03` §16) plan gereği kapsam dışıdır.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F4-23` @ `<sha>`
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
