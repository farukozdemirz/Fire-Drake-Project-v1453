# F3-05: `BotCore` statik kütüphanesi, mini birim test çatısı (`BotCoreTests`) ve belirlenimli `Rng`

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | F3 — Telemetri ve test altyapısı (`docs/17` §2, Görev 5 "Birim test çatısı") |
| Branch | `bot/F3-05` (taban: `gece/2026-10-02`) |
| Bağımlı olduğu planlar | Yok (F3-01..F3-03, F3-06 `KAPANDI`; bu plan sunucu koduna dokunmaz) |
| İlgili gereksinim / kabul | `docs/13` §11 (`BotCore/`, `Tests/BotCoreTests/`), `docs/13` §13 (belirlenim: `seed_bot = hash(seed_episode, bot_slot)`, karar kodunda global `rand()` yok); ADR-0016 |
| Tahmini büyüklük | M (9 dosya, ~500 satır; hepsi yeni, `.sln` dışında mevcut dosya değişmez) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

Sunucudan bağımsız bir `BotCore` statik kütüphanesi ve onu sınayan `BotCoreTests` konsol uygulaması eklemek; testler `tools/run-tests.sh` ile WSL'den koşar, çıkış kodu 0 = hepsi geçti. Çatı, depoya giren tek başlıklı ~100 satırlık bir mini çatıdır (doctest uyumlu makro adlarıyla; ADR-0016). İlk gerçek içerik: bot başına belirlenimli rastgele sayı üreteci (`BotCore::Rng`, SplitMix64 + xoshiro256**, `DeriveBotSeed`), tamamı bilinen/önceden hesaplanmış değerlerle sabitlenmiş testlerle.

`GameServer` bu planda `BotCore`'a **bağlanmaz**; sunucu davranışı ve ikilisi değişmez.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0016-birim-test-catisi.md` — kararın tamamı (neden kendi mini çatımız, hangi dizin/çıktı yolları, `BotCore` saflık kuralı).
- `docs/13_BOT_ARCHITECTURE_AND_DATA_MODEL.md` §11 (dizin yerleşimi) ve §13 (belirlenim, `seed_bot`).
- Örnek proje dosyası: `shared/shared.vcxproj` (StaticLibrary; araç seti `v142` yazılı, `tools/build.sh` `/p:PlatformToolset=v143` ile üzerine yazar; `OutDir`/`IntDir` kalıbı satır 40-52). Uygulama projesi için: `GameServer/proj-GameServer.vcxproj:20` (`Application`), `:47-53` (`OutDir`/`IntDir`), `:81` (`SubSystem`).
- Çözüm dosyası: `KnightOnlineServer.sln` (5 proje; satır 6-25; yapılandırma eşlemeleri `GlobalSection(ProjectConfigurationPlatforms)`, satır 33-52). Yalnızca `Debug|Win32` ve `Release|Win32` vardır.
- `tools/build.sh` (çözümün tamamını derler) ve `tools/run-servers.sh` (betik biçimi: `set -euo pipefail`, `ROOT=...`, kullanım başlığı; `.sh` dosyaları LF).

## 3. Kapsam

**Yapılacaklar**

- `BotCore/BotCore.vcxproj`, `BotCore/Rng.h`, `BotCore/Rng.cpp` (yeni).
- `Tests/BotCoreTests/BotCoreTests.vcxproj`, `MiniTest.h`, `main.cpp`, `RngTests.cpp` (yeni).
- `KnightOnlineServer.sln`: iki proje + yapılandırma eşlemeleri.
- `tools/run-tests.sh` (yeni).

**Kapsam dışı (yapılmayacak)**

- `GameServer`, `AIServer`, `LogInServer`, `shared`, `Scripting` projelerine ve kaynaklarına dokunmak; `GameServer`'a `BotCore` referansı eklemek (ilk tüketici F4/F5'in işi).
- doctest/GoogleTest veya başka üçüncü taraf kütüphane indirmek ya da kopyalamak.
- `Rng`'den başka `BotCore` modülü (utility, FSM, A*, istatistik) yazmak; yük testi; `.vcxproj.filters` dosyası eklemek (gerekmez).
- Mini çatıya fixture, parametreli test, ölçüm, renk, çok iş parçacığı vb. özellik eklemek; belirtilenden fazla makro tanımlamak.
- Sunucuyu çalıştırmak (yalnızca derleme + birim test; sunucu çalışırken derleme `GameServer.exe`'yi kilitleyebilir: `./tools/run-servers.sh status` `[UP]` gösteriyorsa önce `./tools/run-servers.sh stop`).
- `AGENTS.md`, `docs/` değişikliği (belgeleri Claude günceller).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/BotCore.vcxproj` | yeni | StaticLibrary, v142 yazılı, C++17, W4 |
| `BotCore/Rng.h` | yeni | ad alanı `BotCore`; yalnızca `<cstdint>` |
| `BotCore/Rng.cpp` | yeni | |
| `Tests/BotCoreTests/BotCoreTests.vcxproj` | yeni | Application/Console, `BotCore`'a `ProjectReference` |
| `Tests/BotCoreTests/MiniTest.h` | yeni | tek başlıklı mini çatı |
| `Tests/BotCoreTests/main.cpp` | yeni | `int main(int, char**)` -> `minitest::RunAll` |
| `Tests/BotCoreTests/RngTests.cpp` | yeni | test durumları |
| `KnightOnlineServer.sln` | değiştir | yalnızca iki `Project` bloğu + 8 eşleme satırı eklenir |
| `tools/run-tests.sh` | yeni | derle (opsiyonel) + `BotCoreTests.exe` çalıştır |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

Önce dalı aç: `git switch -c bot/F3-05 gece/2026-10-02`.

### 5.1 `BotCore/Rng.h` ve `Rng.cpp`

Başlık `#pragma once`, yalnızca `<cstdint>` içerir. Arayüz (kesin):

```cpp
namespace BotCore
{
	// Reference SplitMix64: advances `state` and returns the next output.
	uint64_t SplitMix64(uint64_t & state);

	// Per-bot seed (docs/13 s13): one SplitMix64 step over (episodeSeed << 32) | botSlot.
	uint64_t DeriveBotSeed(uint32_t episodeSeed, uint32_t botSlot);

	// xoshiro256**; state is filled by four SplitMix64 steps starting from `seed`.
	class Rng
	{
	public:
		explicit Rng(uint64_t seed);

		uint64_t NextU64();
		uint32_t NextU32();                         // upper 32 bits of NextU64()
		uint32_t NextBelow(uint32_t bound);         // uniform in [0, bound); bound == 0 returns 0
		int32_t  NextRange(int32_t lo, int32_t hi); // uniform in [lo, hi] inclusive; requires lo <= hi
		double   NextDouble();                      // uniform in [0, 1)

	private:
		uint64_t m_s[4];
	};
}
```

Algoritmalar (hepsi bu sırayla ve sabitlerle; "uydurma" yok):

- `SplitMix64`: `state += 0x9E3779B97F4A7C15; z = state; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9; z = (z ^ (z >> 27)) * 0x94D049BB133111EB; return z ^ (z >> 31);` (tümü `uint64_t`, sarmalı).
- `DeriveBotSeed`: `uint64_t st = (uint64_t(episodeSeed) << 32) | botSlot; return SplitMix64(st);`
- `Rng(seed)`: `uint64_t st = seed; for i in 0..3: m_s[i] = SplitMix64(st);`
- `NextU64` (xoshiro256**): `result = rotl(m_s[1] * 5, 7) * 9; t = m_s[1] << 17; m_s[2] ^= m_s[0]; m_s[3] ^= m_s[1]; m_s[1] ^= m_s[2]; m_s[0] ^= m_s[3]; m_s[2] ^= t; m_s[3] = rotl(m_s[3], 45); return result;` (`rotl(x,k) = (x << k) | (x >> (64 - k))`).
- `NextU32`: `uint32_t(NextU64() >> 32)`.
- `NextBelow(bound)`: `bound == 0` -> `0`. Aksi halde reddetmeli örnekleme: `uint32_t threshold = (0u - bound) % bound; for (;;) { uint32_t r = NextU32(); if (r >= threshold) return r % bound; }`.
- `NextRange(lo, hi)`: `uint32_t span = uint32_t(hi) - uint32_t(lo) + 1u;` `span == 0` (tam aralık) ise `int32_t(NextU32())` döndür; aksi halde `int32_t(uint32_t(lo) + NextBelow(span))`. (İşaretli taşma tanımsız davranış olmasın diye `uint32_t` aritmetiği kullan.)
- `NextDouble`: `double(NextU64() >> 11) * (1.0 / 9007199254740992.0)` (2^-53).

`Rng.cpp` yalnızca `#include "Rng.h"` içerir (`stdafx.h` **yok**).

### 5.2 `BotCore/BotCore.vcxproj`

`shared/shared.vcxproj`'u şablon al (BOM'lu UTF-8, CRLF). Farklar:

- `ProjectGuid` `{923A84D1-26FF-47B6-A051-DD7D261BD29C}`, `RootNamespace` `BotCore`.
- `ConfigurationType` `StaticLibrary`, `PlatformToolset` `v142` (diğer projelerle aynı), `CharacterSet` `MultiByte`; `WholeProgramOptimization` **yok**.
- `OutDir` `$(SolutionDir)build\bin\$(PlatformTarget)-$(Configuration)\libs\`, `IntDir` `$(SolutionDir)build\obj\$(PlatformTarget)-$(Configuration)\libs\$(ProjectName)\` (iki yapılandırma için; `shared.vcxproj`'daki yinelenen/eski `OutDir` satırını kopyalama).
- `ClCompile`: `WarningLevel` `Level4`, `PrecompiledHeader` `NotUsing`, `LanguageStandard` `stdcpp17`, `PreprocessorDefinitions` `WIN32;_WIN32;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)`, `RuntimeLibrary` Debug'ta `MultiThreadedDebug`, Release'te `MultiThreaded` (sunucu ve testle aynı CRT), Release'te `Optimization` `MaxSpeed`.
- Öğeler: `<ClInclude Include="Rng.h" />`, `<ClCompile Include="Rng.cpp" />`.

### 5.3 `Tests/BotCoreTests/MiniTest.h`

Tek başlık, yalnızca standart kütüphane (`<cstdio>`, `<sstream>`, `<string>`, `<vector>`). Gereken yüzey:

```cpp
namespace minitest
{
	struct RequireFailure {};                                    // thrown by REQUIRE
	struct Registrar { Registrar(const char * name, void (*fn)(), const char * file, int line); };
	void ReportFailure(const char * file, int line, const std::string & text);   // prints + counts
	int  RunAll(int argc, char ** argv);   // 0 = all passed, 1 = any failure, 2 = usage error
}
```

- Kayıt defteri `inline` işlev içindeki `static std::vector<...>` (başlık-only, tek tanım).
- Makrolar: `TEST_CASE(name)` (iki aşamalı birleştirme ve **`__COUNTER__`** ile benzersiz işlev/kayıtçı adı üretir — `__LINE__` değil; MSVC `/ZI` altında sabit olmaz — `static void fn(); static minitest::Registrar reg(name, &fn, __FILE__, __LINE__); static void fn()` kalıbı), `CHECK(expr)`, `CHECK_EQ(a, b)` (başarısızlıkta iki değeri `std::ostringstream` ile yazar; `uint64_t` ondalık basılır), `REQUIRE(expr)` (başarısızlıkta raporlar ve `RequireFailure` fırlatır; `RunAll` yakalayıp o testi bitirir). Başarısızlık satırı `dosya(satır): CHECK(expr) basarisiz` biçimindedir (MSVC tarzı, `file(line):`).
- `RunAll`: tüm testleri kayıt sırasıyla çalıştırır; her test için `[ OK ] ad` veya `[FAIL] ad` yazar; sonda `N test, M basarisiz` (tam bu biçim: `<N> tests, <M> failed`, İngilizce sabit, betikler ve Claude bunu grep'ler). Argümanlar: `--list` (yalnızca test adlarını satır satır yazar, 0 döner), tek bir konumsal argüman = ad alt dizgisi süzgeci (eşleşen test yoksa `no tests matched` yazıp 2 döner). Başlık `#include`'dan sonra başka çatı kodu yok.

### 5.4 `Tests/BotCoreTests/main.cpp`

`#include "MiniTest.h"` ve `int main(int argc, char ** argv) { return minitest::RunAll(argc, argv); }`.

### 5.5 `Tests/BotCoreTests/RngTests.cpp`

`#include "MiniTest.h"` ve `#include <BotCore/Rng.h>` (proje `AdditionalIncludeDirectories` olarak `$(SolutionDir)` verir). En az şu 6 `TEST_CASE` (ad önerileri `Rng_...`); beklenen değerler **bağımsız bir Python gerçeklemesiyle hesaplanmıştır**, aynen kullan:

1. `Rng_SplitMix64_ReferenceVector`: `state = 0` ile üç ardışık `SplitMix64(state)` = `0xE220A8397B1DCDAF`, `0x6E789E6AA1B965F4`, `0x06C45D188009454F` (yayımlanmış referans vektör).
2. `Rng_DeriveBotSeed_Pinned`: `DeriveBotSeed(7,0) == 0xBCDA4680438A5951`, `DeriveBotSeed(7,1) == 0x1A3EAA3C25C3A340`, `DeriveBotSeed(8,0) == 0xFEAB185D957C5F22`; ayrıca `episodeSeed = 7`, `botSlot = 0..63` için 64 değerin hepsi birbirinden farklı (`std::set`).
3. `Rng_Xoshiro_Pinned`: `Rng(12345)` ilk üç `NextU64()` = `0xBE6A36374160D49B`, `0x214AAA0637A688C6`, `0xF69D16DE9954D388`; yeni `Rng(12345)` ile ilk üç `NextU32()` = `3194631735`, `558541318`, `4137490142`; `Rng(DeriveBotSeed(7,0))` ilk iki `NextU64()` = `0x6D1C3405675A6FB1`, `0xF2EA51CE434A4177`.
4. `Rng_Determinism`: aynı tohumla iki `Rng` 1000 çıktı boyunca özdeş; `Rng(1)` ile `Rng(2)` ilk 8 çıktıda farklı (en az bir fark).
5. `Rng_NextBelow`: `Rng(12345)` ile ilk 8 `NextBelow(10)` = `5, 8, 2, 2, 6, 6, 7, 3`; `NextBelow(1) == 0` (100 çağrı); `NextBelow(0) == 0`; `Rng(99)` ile 100000 `NextBelow(10)` çağrısında her kutu 9500..10500 aralığında (olasılıksal değil, tohum sabit olduğu için belirlenimli; başarısız olursa **eşiği gevşetme**, raporla).
6. `Rng_NextDouble_And_NextRange`: `Rng(12345)` ile ilk iki `NextDouble()` tam eşitlikle `0.7438081631565894` ve `0.13004553462783452`; `Rng(5)` ile 100000 `NextDouble()` hepsi `[0,1)` içinde ve ortalama `0.49..0.51`; `NextRange(-5, 5)` 10000 çağrıda hiç aralık dışı değil ve hem `-5` hem `5` en az bir kez görülür; `NextRange(3, 3) == 3`; `NextRange(INT32_MIN, INT32_MAX)` 1000 çağrıda çökmeden döner.

`NextU64` pinleri tutmazsa: sabitleri ve kaydırmaları §5.1'e karşı bir kez daha kontrol et; ancak yine tutmazsa testi değiştirme, **dur** ve raporla.

### 5.6 `Tests/BotCoreTests/BotCoreTests.vcxproj`

§5.2 şablonundan, farklar:

- `ProjectGuid` `{A45478D2-7A55-43D6-B6CB-F473CA514A2A}`, `RootNamespace` `BotCoreTests`, `ConfigurationType` `Application`, `Link` içinde `SubSystem` `Console`.
- `OutDir` `$(SolutionDir)build\bin\$(PlatformTarget)-$(Configuration)\Tests\`, `IntDir` `$(SolutionDir)build\obj\$(PlatformTarget)-$(Configuration)\Tests\$(ProjectName)\`; `TargetName` `BotCoreTests` (çıktı `BotCoreTests.exe`).
- `ClCompile` içinde `AdditionalIncludeDirectories` `$(SolutionDir);%(AdditionalIncludeDirectories)`.
- `ProjectReference`: `<ProjectReference Include="..\..\BotCore\BotCore.vcxproj"><Project>{923A84D1-26FF-47B6-A051-DD7D261BD29C}</Project></ProjectReference>`.
- Öğeler: `MiniTest.h` (ClInclude), `main.cpp`, `RngTests.cpp` (ClCompile).

### 5.7 `KnightOnlineServer.sln`

BOM + CRLF korunur. `shared` projesinin `EndProject` satırından sonra (`Global`'dan önce) iki `Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}")` bloğu ekle: `BotCore` (`"BotCore\BotCore.vcxproj"`, `{923A84D1-...}`) ve `BotCoreTests` (`"Tests\BotCoreTests\BotCoreTests.vcxproj"`, `{A45478D2-...}`; mevcut projelerdeki gibi `ProjectSection(ProjectDependencies)` ile `{923A84D1-...}`'e bağımlı). `GlobalSection(ProjectConfigurationPlatforms)` içine her iki GUID için `Debug|Win32` ve `Release|Win32` için `.ActiveCfg` ve `.Build.0` satırları (toplam 8 satır; `Deploy.0` yok). Başka hiçbir satırı (SolutionGuid dâhil) değiştirme.

### 5.8 `tools/run-tests.sh`

Biçim `tools/build.sh`/`run-servers.sh` gibi (LF, `#!/usr/bin/env bash`, `set -euo pipefail`, üstte kullanım yorumu, tab girinti):

```
Usage: tools/run-tests.sh [Release|Debug] [--no-build] [test-args...]
```

- Varsayılan `Release`. `--no-build` yoksa `"$ROOT/tools/build.sh" "$CONFIG"` çağrılır (derleme hatası betiği durdurur).
- `EXE="$ROOT/build/bin/x86-$CONFIG/Tests/BotCoreTests.exe"`; yoksa `BotCoreTests.exe not found: ... (build first)` ile `exit 2`.
- Kalan argümanları (ör. `--list`, ad süzgeci) `BotCoreTests.exe`'ye geçir; çıkış kodunu olduğu gibi döndür (`exit "$rc"`; `set -e` testin başarısızlığında betiği erken kesmesin, `rc=0; "$EXE" "$@" || rc=$?`).
- Bilinmeyen ilk argüman `Release|Debug` değilse kullanım yazıp `exit 2`.

## 6. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release` hatasız biter; çıktıda `BotCore` veya `BotCoreTests` yolunu içeren `warning` satırı yok (`./tools/build.sh Release 2>&1 | grep -i 'warning' | grep -i 'botcore'` boş).
- [ ] K2: `./tools/build.sh Debug` hatasız biter, aynı uyarı denetimi boş.
- [ ] K3: `./tools/run-tests.sh Release --no-build` çıkış kodu 0; son satır `N tests, 0 failed` ve `N >= 6`; `build/bin/x86-Release/Tests/BotCoreTests.exe` ve `build/bin/x86-Release/libs/BotCore.lib` vardır.
- [ ] K4: `./tools/run-tests.sh Debug --no-build` çıkış kodu 0, `N tests, 0 failed`, `N >= 6`.
- [ ] K5: Sabitlenmiş değerler dosyada vardır: `grep -ci -e 'E220A8397B1DCDAF' -e 'BCDA4680438A5951' -e 'BE6A36374160D49B' -e '0.7438081631565894' Tests/BotCoreTests/RngTests.cpp` en az 4 (büyük/küçük harf duyarsız).
- [ ] K6: Başarısızlık gerçekten bildiriliyor: `RngTests.cpp`'de bir pin literalini geçici olarak bozup (ör. `0xE220A8397B1DCDAF` -> `0xE220A8397B1DCDAE`) yeniden derle ve `./tools/run-tests.sh Release` çalıştır: çıkış kodu **0 değil**, çıktıda `RngTests.cpp(` ile başlayan satır ve `1 failed` benzeri sayı var. Sonra **geri al** (`git diff Tests/BotCoreTests/RngTests.cpp` bozulmadan sonraki commit'le aynı) ve K3'ü tekrar çalıştır. Bu denemeyi Uygulayıcı Raporu'nda çıktıyla göster; deneme commit edilmez.
- [ ] K7: `./tools/run-tests.sh Release --no-build --list` en az 6 satır test adı yazar, çıkış 0; `./tools/run-tests.sh Release --no-build Rng_NextBelow` yalnızca o testi çalıştırır (`1 tests, 0 failed`); `... NoSuchTest` çıkış kodu 2.
- [ ] K8: `BotCore` saf: `grep -rniE 'windows\.h|stdafx|GameServer|shared/|winsock' BotCore/*.h BotCore/*.cpp` boş; `grep -n 'rand(' BotCore/*.cpp BotCore/*.h` boş.
- [ ] K9: Kapsam: `git diff --stat gece/2026-10-02...bot/F3-05` yalnızca §4 tablosundaki dosyaları + plan dosyasını gösterir; `GameServer/`, `AIServer/`, `LogInServer/`, `shared/`, `Scripting/` altında değişiklik yok; `KnightOnlineServer.sln` farkı yalnızca **ekleme** satırlarıdır (`git diff gece/2026-10-02...bot/F3-05 -- KnightOnlineServer.sln | grep '^-' | grep -v '^---'` boş).
- [ ] K10: Kodlama: yeni `.h/.cpp` dosyalar ASCII + CRLF; `.vcxproj` ve `.sln` UTF-8 BOM + CRLF; `tools/run-tests.sh` ASCII + LF (`file` çıktısıyla).
- [ ] K11: Sunucu davranışı değişmedi: `GameServer.exe` bu planla yeniden bağlanabilir ama kaynak/proje farkı yok (K9); `[BOT] ENABLED` varsayılanı vb. dosyalara dokunulmadı.
- [ ] K12: Tüm testler < 2 sn içinde biter (`time ./tools/run-tests.sh Release --no-build`).

## 7. Doğrulama komutları

```bash
./tools/run-servers.sh status          # [UP] varsa: ./tools/run-servers.sh stop
./tools/build.sh Release
./tools/build.sh Debug
./tools/run-tests.sh Release --no-build
./tools/run-tests.sh Debug --no-build
./tools/run-tests.sh Release --no-build --list
./tools/run-tests.sh Release --no-build Rng_NextBelow
file BotCore/* Tests/BotCoreTests/* KnightOnlineServer.sln tools/run-tests.sh
git diff --stat gece/2026-10-02...bot/F3-05
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3 (CRLF, tab, Allman, yorumlar İngilizce, yeni kaynaklar ASCII). **İstisna:** `AGENTS.md` "yeni bot kodu `GameServer/Bot/` altına" der; bu planda `BotCore/` ve `Tests/` **bilerek** kök dizinde (`docs/13` §11, ADR-0016) ve `GameServer/proj-GameServer.vcxproj`'a **eklenmez**.
- `BotCore` sunucu başlığı içermez ve içermeyecek (ADR-0016 kuralı); `rand()`/`srand()`/`std::rand`/`std::random_device` yok.
- Üçüncü taraf kod yok. `MiniTest.h` senin yazdığın ~100 satırdır.
- Gece modu: bot sistemi ve sunucu davranışı bu planla değişmez; plan sonunda sunucu açmadın, kapatmaya gerek yok.
- Bu planın K6 denemesi dışında hiçbir testi gevşetme; pinlerin tutmaması bir hata göstergesidir, çözüm testi değiştirmek değildir.
- Tahmin gerektiren bir nokta kalırsa (ör. `__COUNTER__` MSVC'de beklenmedik davranırsa) dur ve raporla.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- **Durum:** UYGULANDI
- **Branch / commit'ler:** `bot/F3-05` (taban: `gece/2026-10-02`); `bac4e21` (BotCore + Rng), `0d0b7b5` (BotCoreTests + sln + `tools/run-tests.sh`), bu raporu ve `Durum` satirini iceren son commit.
- **Degisen dosyalar ve nedenleri (yalnizca §4):**
  - `BotCore/Rng.h`, `BotCore/Rng.cpp` (yeni): `SplitMix64`, `DeriveBotSeed`, `Rng` (xoshiro256**; `NextU64/NextU32/NextBelow/NextRange/NextDouble`) §5.1'deki algoritma ve sabitlerle birebir. `Rng.cpp` yalnizca `#include "Rng.h"`.
  - `BotCore/BotCore.vcxproj` (yeni): StaticLibrary, v142, C++17, W4, `NotUsing` PCH, iki yapilandirmada `OutDir`/`IntDir` `libs\`; `shared.vcxproj` sablonundan, `WholeProgramOptimization` yok.
  - `Tests/BotCoreTests/MiniTest.h`, `main.cpp`, `RngTests.cpp` (yeni): tek baslikli mini cati (`TEST_CASE`/`CHECK`/`CHECK_EQ`/`REQUIRE`, `__COUNTER__` ile iki asamali benzersiz ad; `RunAll` `--list` ve ad suzgeci) ve 6 test.
  - `Tests/BotCoreTests/BotCoreTests.vcxproj` (yeni): Application/Console, `BotCore` `ProjectReference`, `AdditionalIncludeDirectories=$(SolutionDir);...`.
  - `KnightOnlineServer.sln` (degisti): yalnizca 2 `Project` blogu + 8 yapilandirma esleme satiri eklendi (silme yok; `SolutionGuid` ve diger satirlar aynen).
  - `tools/run-tests.sh` (yeni): `[Release|Debug] [--no-build] [test-args...]`; bilinmeyen ilk arguman `exit 2`; EXE yoksa `exit 2`; testin cikis kodunu oldugu gibi dondurur.
- **Bagimsiz on dogrulama:** §5.5'teki tum sabitlenmis degerler yazmadan once bagimsiz bir Python gerceklemesiyle yeniden hesaplandi; hepsi birebir tuttu (SplitMix64 referans vektoru; `DeriveBotSeed(7,0)=0xBCDA4680438A5951`, `(7,1)=0x1A3EAA3C25C3A340`, `(8,0)=0xFEAB185D957C5F22`; `Rng(12345)` u64/u32; `DeriveBotSeed(7,0)` u64; `NextBelow(10)` dizisi `5,8,2,2,6,6,7,3`; iki `NextDouble`; seed 99 `NextBelow(10)` kutu sayilari 9872..10141; seed 5 `NextDouble` ortalama 0,5003; seed 7 `NextRange(-5,5)` tam kapsama).
- **Derleme ciktisi (son satirlar):**
  - Release: `BotCoreTests.vcxproj -> ...\build\bin\x86-Release\Tests\BotCoreTests.exe` / `proj-GameServer.vcxproj -> ...\x86-Release\Server\GameServer.exe`; `rc=0`.
  - Debug: `BotCoreTests.vcxproj -> ...\build\bin\x86-Debug\Tests\BotCoreTests.exe` / `proj-GameServer.vcxproj -> ...\x86-Debug\Server\GameServer.exe`; `rc=0`.
  - Kalan uyarilar yalnizca eski `GameServerDlg.cpp:816/1143/1802`; `botcore` iceren uyari 0.
- **Kabul kriterleri oz-degerlendirmesi:**
  - K1 ✔ Release rc=0; `grep -i warning | grep -i botcore` bos.
  - K2 ✔ Debug rc=0; ayni uyari denetimi bos.
  - K3 ✔ `./tools/run-tests.sh Release --no-build` rc=0, son satir `6 tests, 0 failed`; `BotCoreTests.exe` ve `libs/BotCore.lib` mevcut.
  - K4 ✔ Debug ayni: `6 tests, 0 failed`, rc=0.
  - K5 ✔ `grep -ci ...` = 4.
  - K6 ✔ `0xE220A8397B1DCDAF` -> `...DAE` yapilip yeniden derlendi: rc=1, `...\Tests\BotCoreTests\RngTests.cpp(10): CHECK_EQ(...) failed` satiri ve `6 tests, 1 failed`; pin geri alindi, K3 tekrar rc=0. Deneme commit edilmedi.
  - K7 ✔ `--list` 6 ad rc=0; `Rng_NextBelow` -> `1 tests, 0 failed`; `NoSuchTest` -> rc=2.
  - K8 ✔ `windows.h|stdafx|GameServer|shared/|winsock` ve `rand(` grep'leri bos.
  - K9 ✔ `git diff --stat gece/2026-10-02...bot/F3-05` yalnizca §4'teki 9 dosya; `GameServer/`, `AIServer/`, `LogInServer/`, `shared/`, `Scripting/` farki yok; sln farki salt ekleme (`^-` satiri yok).
  - K10 ✔ `.h/.cpp` ASCII+CRLF; `.vcxproj`/`.sln` UTF-8 BOM+CRLF; `tools/run-tests.sh` ASCII+LF (`file` ile).
  - K11 ✔ GameServer kaynak/proje farki yok; bot sistemi/ini dosyalarina dokunulmadi.
  - K12 ✔ `time ./tools/run-tests.sh Release --no-build` ≈ 0,04 sn.
- **Plandan sapmalar:** Yok. K6 denemesi disinda hicbir test gevsetilmedi.
- **Notlar / acik sorular:** Yok. (Bilgi: MSVC `__FILE__`'i tam yol olarak basar; K6'nin istedigi `RngTests.cpp(` ifadesi satirin icinde `...\Tests\BotCoreTests\RngTests.cpp(10):` seklinde gecer.)

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

*(henüz doğrulanmadı)*
