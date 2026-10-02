# ADR-0016: `BotCore` statik kütüphanesi ve birim test çatısı: kendi mini çatımız (otonom döngüde Claude kararı — gözden geçirilmeli)

Durum: KABUL (geçici, proje sahibi gözden geçirecek) · Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`; kullanıcıya sorulamadı)
İlgili: `docs/13` §11 ("BotCore/ statik kütüphane", "Tests/BotCoreTests/ doctest veya GoogleTest"), `docs/13` §13 (belirlenim: `seed_bot = hash(seed_episode, bot_slot)`), `docs/17` F3 Görev 5 ("Birim test çatısı"), F3 Test ("Sahte olaylarla yük testi")

## Bağlam
`docs/13` §11, sunucuya bağımlı olmayan saf mantığın (utility, FSM, navigasyon algoritmaları, fairness kuralları, istatistik) `BotCore` adlı bir statik kütüphanede durmasını ve ayrı bir test projesiyle sınanmasını öngörür; test çatısı olarak "doctest veya GoogleTest" yazar, çünkü depoda test çatısı yoktur. F4 (karar/aksiyon), F5 (A*) ve sonrası, sunucuyu açmadan ve milisaniyeler içinde koşan testler olmadan güvenle yazılamaz. Çalışma ortamı: derleme WSL'den `tools/build.sh` ile (MSBuild, v143, Win32); DeepSeek'in ağ erişimi ve çevrimdışı bağımlılık kurma garantisi yoktur; `AGENTS.md` plan izin vermedikçe üçüncü taraf kütüphane eklemeyi yasaklar.

## Karar
- **Yeni proje `BotCore`** (`BotCore/BotCore.vcxproj`, `StaticLibrary`, C++17): `windows.h`, `stdafx.h`, `shared/` ve `GameServer/` başlıklarını **içermez**; yalnızca standart kütüphane. Ad alanı `BotCore`. Çıktı `build\bin\x86-<Cfg>\libs\`.
- **Yeni proje `BotCoreTests`** (`Tests/BotCoreTests/BotCoreTests.vcxproj`, konsol uygulaması, `BotCore`'a `ProjectReference`): çıktı `build\bin\x86-<Cfg>\Tests\BotCoreTests.exe`; çıkış kodu 0 = hepsi geçti.
- **Çatı: depoya giren tek başlıklı mini çatı (`Tests/BotCoreTests/MiniTest.h`)**, doctest'in küçük bir alt kümesiyle **aynı makro adlarıyla** (`TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`): ileride doctest'e geçmek test dosyalarını değiştirmeden `#include` satırını değiştirmek kadar kolay olsun.
- Çözüm dosyasına (`KnightOnlineServer.sln`) iki proje eklenir; `tools/build.sh` zaten tüm çözümü derlediği için Release/Debug derlemesi testleri de derler. Çalıştırma: yeni `tools/run-tests.sh`.
- `GameServer` bu planda `BotCore`'a **bağlanmaz** (henüz tüketicisi yok). İlk tüketici (F4/F5) projeye `ProjectReference` ekler; bu ADR'nin kuralı: `BotCore` hiçbir zaman sunucu başlığı içermez, sunucu `BotCore`'u kullanır, tersi yok.
- İlk içerik: belirlenimli rastgele sayı üreteci (`BotCore/Rng.h/.cpp`: SplitMix64 tohumlama, xoshiro256**, `DeriveBotSeed`), çünkü `docs/13` §13 tüm karar kodunda global `rand()` yasağını ve bot başına tohumu şart koşar; bu, çatıyı gerçek bir gereksinimle sınar.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| doctest (tek başlık, depoya kopyalanmış) | Olgun, `docs/13`'ün ilk önerisi | ~7000 satırlık üçüncü taraf başlık depoya girer; DeepSeek ağdan indiremeyebilir; lisans/sürüm yönetimi; bu aşamada gereksiz özellik | Bu ADR'deki makro uyumuyla sonra eklenebilir |
| GoogleTest | Çok yaygın | Ayrı derleme/NuGet/CMake bağımlılığı; MSBuild + WSL zincirinde kırılgan | Maliyet/risk yüksek |
| Microsoft CppUnitTest (VS ile gelir, `VC\Auxiliary\VS\UnitTest` altında var) | Ek indirme yok | Test koşucusu `vstest.console.exe`/VS'e bağlı, WSL'den çıktı/çıkış kodu zahmetli, DLL tabanlı | Otomasyona uygun değil |
| Çatısız `assert` tabanlı tek `main` | En basit | Başarısızlıkta ilk hatada durur, test adı/özet yok, büyüdükçe dağınık | Bakım maliyeti |
| Kendi mini çatı (seçilen) | Sıfır bağımlılık, ~100 satır, doctest uyumlu makrolar, WSL'den çıkış kodu | Bakımı bize ait; özellikleri (fixture, parametreli test) yok | Gerektiğinde doctest'e geçilir |

## Sonuçlar
- Olumlu: F4+ saf mantığı sunucusuz ve hızlı sınanır; tekrarlanabilir (belirlenimli) davranış baştan test edilir; ek bağımlılık yok.
- Olumsuz: iki yeni proje (çözüm derleme süresi küçük artar); mini çatının özellik eksikleri.
- Geri alma: çözümden iki proje satırı ve `BotCore/`, `Tests/` klasörleri kaldırılır; `GameServer` etkilenmez.

## Doğrulama
F3-05: `tools/run-tests.sh Release` ve `Debug` çıkış kodu 0; bilerek bozulan bir `CHECK` ile çıkış kodunun 0 dışına düştüğü; `grep` ile `BotCore/` altında `windows.h`/`stdafx.h`/`../GameServer` geçmediği.
