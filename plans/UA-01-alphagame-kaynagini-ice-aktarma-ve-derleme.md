# UA-01: AlphaGame kaynağını içe aktarma ve Release|Win32 derleme (bot katmanı devre dışı)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | UA — Sürüm yükseltme tabanı AlphaGame 1534 (ADR-0069) |
| Branch | `bot/UA-01` (taban: `yukseltme/alpha` @ `438f776c` = `yukseltme/1534`) |
| Bağımlı olduğu planlar | — |
| İlgili gereksinim / kabul | ADR-0069 madde 1 ve madde 2 (ilk kural) |
| Tahmini büyüklük | L (dizin değişimi + proje dosyaları) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

Depodaki üst kaynak dizinlerini AlphaGame 1534 kaynağıyla değiştirmek ve çözümü bizim araçlarımızla (`./tools/build.sh`, MSVC v143, Win32) hatasız derlemek. Değişen dizinler: `GameServer` (`GameServer/Bot/` hariç), `AIServer`, `LogInServer`, `shared`, `N3BASE`, `scripting`. Bu planda bot katmanı GameServer'a bağlanmaz (UA-04'ün işi). Sunucu çalıştırılmaz: ADR-0069 madde 2'ye göre güvenlik düzeltmeleri (UA-02) bitmeden AlphaGame kodu çalıştırılmaz.

## 2. Bağlam (okunması zorunlu)

- `docs/adr/ADR-0069-surum-yukseltme-tabani-alphagame-1534.md`.
- `docs/reports/u0-1534/A-kaynak-kod-karsilastirmasi.md` §1 (dosya sayıları), §5 (kanca dosyaları; bu planda taşınmaz), §7 (derleme: toolset karışık v142/v143, `LanguageStandard` yok, `Server-Files/*.lib` ve `C:\KO\...` mutlak yolları yalnız Debug|x64/Template'te).
- Kaynak ağaç (salt okunur, indirilmiş dosya): `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/1-Game Source/`. Bu dizinde betik ya da derleme **çalıştırılmaz**; betik bu yolu argüman olarak alır ve `python3 -I` ile çalışır.
- Çözüm `KnightOnlineServer.sln`: proje adları ve GUID'leri AlphaGame `ALPHAGAME v1534.sln` ile aynı (`AIServer`, `GameServer`, `LogInServer`, `Lua`, `shared`). `BotCore` ve `BotCoreTests` projeleri değişmeden kalır.
- Bizim vcxproj ayarları (çıktı yolları, toolset, C++17) bugünkü `GameServer/proj-GameServer.vcxproj` vb. dosyalarda; `tools/run-servers.sh` exe'leri `build/bin/x86-<Config>/Server/` altında bekler.

## 3. Kapsam

**Var:**
1. **İçe aktarma aracı** `tools/ua-import-alpha.py` (standart kütüphane, `python3 -I`, kaynak ve hedef yolu argümanla): hedefteki altı dizinin git'te izlenen dosyalarını siler (`GameServer/Bot/` korunur), AlphaGame dosyalarını bayt bayt kopyalar ve sayım raporu basar. Kopyalanmayanlar: derleme çıktıları ve artıklar (`*.obj *.pdb *.idb *.ilk *.lib *.exe *.dll *.exp *.log *.tlog *.recipe *.ipch *.aps *.user *.sdf *.db *.opendb`) ve `Debug*/`, `Release*/`, `x64/`, `.vs/` dizinleri. Bayt eşitliği `--check` ile doğrulanabilir olmalı.
2. **Proje dosyaları:** AlphaGame vcxproj dosya listeleri kullanılır; derleme ayarları bizim düzene uyarlanır:
   - `Debug|Win32` ve `Release|Win32` çalışır; diğer yapılandırmalar `.sln`'de kullanılmaz.
   - `OutDir`/`IntDir` bizim bugünkü vcxproj değerleriyle aynı (`$(SolutionDir)build\bin\$(PlatformTarget)-$(Configuration)\Server\` vb.).
   - GameServer `LanguageStandard=stdcpp17` (bot katmanı için gerekli); diğer projelerde gerekmiyorsa değişmez.
   - Kütüphaneler çözüm içi derleme çıktılarından (`shared.lib`, `Lua.lib`); `Server-Files` ve `C:\KO\` referansı **hiç** kalmaz (Win32 yapılandırmalarında ve diğerlerinde).
3. **C++17 kırılmaları** (ör. `shared/tstring.cpp` `std::ptr_fun`) en küçük değişiklikle düzeltilir; her biri raporda dosya:satır ve gerekçeyle.
4. `GameServer/Bot/*` diskte kalır, GameServer projesine eklenmez.

**Yok:** bot kancaları (UA-04), güvenlik düzeltmeleri (UA-02), paket düzeni (UA-03), DB, Map/Lua, ini dosyaları, sunucu başlatma.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `AIServer/**`, `LogInServer/**`, `shared/**`, `N3BASE/**`, `scripting/**` | AlphaGame ile değişim + §3.2–3.3 |
| `GameServer/**` (`GameServer/Bot/**` hariç) | aynı |
| `tools/ua-import-alpha.py` | yeni |
| `KnightOnlineServer.sln`, `.gitattributes` | yalnız gerekirse (raporda gerekçe) |

## 5. Uygulama adımları

1. Aracı yaz; `--dry-run` sayımlarını rapora al; çalıştır; `git status --short | wc -l` ve eklenen/silinen/değişen dosya sayıları rapora.
2. vcxproj ayarlarını uyarla; `./tools/build.sh Release` ve `./tools/build.sh Debug`.
3. `./tools/run-tests.sh Release`.
4. §6 K4 ve K5 kanıtlarını topla.

## 6. Kabul kriterleri

- [ ] K1: Release ve Debug (Win32) 0 hata; uyarı sayısı raporda (AlphaGame kaynaklı uyarılar kabul, yeni eklediğimiz kodda uyarı yok).
- [ ] K2: `build/bin/x86-Release/Server/` altında `GameServer.exe`, `AIServer.exe`, `LogInServer.exe` oluşur.
- [ ] K3: `./tools/run-tests.sh Release` taban (3484) ile aynı sayıda test, 0 başarısız.
- [ ] K4: `grep -a -rn -i -E "Server-Files|C:\\\\KO" --include=*.vcxproj .` çıktısı boş.
- [ ] K5: `python3 -I tools/ua-import-alpha.py --check <kaynak> .` içe aktarılan her dosyanın AlphaGame ile bayt eşit olduğunu ya da §3.2–3.3 değişikliği olduğunu listeler; liste dışı fark 0.
- [ ] K6: `git diff --stat yukseltme/alpha...bot/UA-01` yalnız §4; `GameServer/Bot/` değişmedi; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/build.sh Debug
./tools/run-tests.sh Release
grep -a -rn -i -E "Server-Files|C:\\\\KO" --include=*.vcxproj .
python3 -I tools/ua-import-alpha.py --check "/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/1-Game Source" .
git diff --stat yukseltme/alpha...bot/UA-01 -- GameServer/Bot
git status --short
```

## 8. Kısıtlar ve uyarılar

- Sunucu başlatılmaz. İndirilen paketteki hiçbir `exe/lib/pdb` çalıştırılmaz, kopyalanmaz, bağlanmaz.
- Kaynak dizinde komut çalıştırılmaz; araç yalnız okur.
- Paketteki `Server-Files/Logs` ve `DB/` bu planın kapsamında değildir; kopyalanmaz.
- Kodlama: kopyalanan dosyalar bayt bayt korunur; elle düzeltilen dosyalarda özgün kodlama ve satır sonu korunur (`AGENTS.md` §3).
- Git: `AGENTS.md` §2.8; commit `[UA-01] ...`; push yok.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
