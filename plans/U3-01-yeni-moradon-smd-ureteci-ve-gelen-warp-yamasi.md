# U3-01: Yeni Moradon SMD üreteci (`tools/u3-moradon-smd.py`) ve Moradon'a giren warp yaması

| Alan | Değer |
|---|---|
| Durum | UYGULANDI |
| Faz | U3 — Sürüm yükseltme 1534, yeni Moradon (`docs/17` §2 U, ADR-0068 Ek 2) |
| Branch | `bot/U3-01` (taban: `main`) |
| Bağımlı olduğu planlar | — |
| İlgili gereksinim / kabul | T-UPG-02 hazırlığı; `docs/reports/u0-1534/G-yeni-moradon-smd.md` §7 (V1–V10) |
| Tahmini büyüklük | M (1 araç + `docs/appendix/tools` kullanımı) |
| Hazırlayan / tarih | Claude / 2026-10-08 |

---

## 1. Amaç

Yeni Moradon (zone 21) için sunucu harita dosyasını (SMD) **1534 istemcisinin kendi dosyalarından** üreten, Moradon'a giren warp'ları diğer haritalarda yamalayan ve çıktıyı çevrimdışı doğrulayan deterministik bir araç. Yöntem G raporunda kanıtlı bir prototiple doğrulandı (`/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/k1534/g/u3_moradon_smd.py`; prototip referanstır, depo kurallarına göre yeniden yaz). Üretilen SMD'ler **depoya girmez** (istemciden türetilmiş tescilli veri); dağıtım Claude'un işidir.

## 2. Bağlam (okunması zorunlu)

- `docs/reports/u0-1534/G-yeni-moradon-smd.md` tamamı (özellikle §1 düzenler, §2 yükseklik, §3 çarpışma, §4 yürünebilirlik kuralı R ve doğrulamalar, §5 warp'lar, §7 araç tasarımı ve V1–V10, §8 çalışma zamanı denetimleri).
- `docs/adr/ADR-0068-*.md` Ek 2.
- Sunucu okuyucu: `shared/SMDFile.cpp:16-180`, `N3BASE/N3ShapeMgr.cpp:52-118`, `shared/database/structs.h:212-240` (`_WARP_INFO`).
- Depodaki ayrıştırıcılar: `docs/appendix/tools/smd_parse.py`, `smd_analyze.py`, `tools/nav-export.py` (zone 71 parmak izi/crc32).
- Girdiler (salt okunur): istemci `/mnt/c/dev/fdp1534/client/Knight Online/Zones/moradon.gtd`, `.opd`, `.opdext`; bağışçı `/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/Server-Files/Map/moradon_0826.smd`; bizim haritalar `/mnt/c/dev/fdp/server/Map/*.smd`; ALPHA DB `.\SQL2019` → `FDP_alpha1534` (yalnız `SELECT`, `K_NPCPOS`/`START_POSITION` doğrulama noktaları için).

## 3. Kapsam

**Var:** `tools/u3-moradon-smd.py` (yalnız standart kütüphane; `python3 -I` ile çalışır):
- `build --client-zones DIR --donor SMD [--warps SPEC.json] --out SMD`: G §7 adımları 1–8 (yükseklik `.gtd`'den transpozsuz; çarpışma `.opd` bloğu bayt bayt; kural R ile olay ızgarası; ALPHA nesne olayı bloğu; regene 0; warp'lar ALPHA'dan zone 73 hariç; yan JSON: girdi/çıktı md5 ve sayılar). Warp ücretleri: **bizim** değerler (ADR-0068 Ek 2 madde 3) — bizim eski Moradon SMD'sindeki aynı kimlikli warp'ların ücreti kullanılır; karşılığı yoksa ALPHA ücreti ve raporda listelenir.
- `patch-inbound --map-dir DIR --files LIST --x 817 --z 530 --out-dir DIR`: `sZone == 21` warp kayıtlarında yalnız `fX/fZ` (8 bayt) değişir; diğer her bayt aynı.
- `verify --smd SMD [--points JSON] [--spawns FILE]`: V1–V10 (G §7 tablosu); başarısızlıkta çıkış ≠ 0.
- `--selftest`: kural R'yi depodaki/yerel referans SMD'lerde (eski Moradon, 71, 72) %100; istemci→ızgara uçtan uca ≥ %99,99 (71, 72); dosyalar yoksa ilgili alt test `SKIP` yazar ama sentetik birim testleri (başlık ayrıştırma, kural R, yama baytları) her zaman koşar.

**Yok:** DB değişiklikleri (U3-02), Lua (U3-03), dağıtım, sunucu kodu, depoya SMD eklemek.

## 4. Dokunulabilecek dosyalar

| Dosya | Değişiklik |
|---|---|
| `tools/u3-moradon-smd.py` | YENİ |
| `docs/appendix/tools/smd_parse.py` | yalnız hata düzeltmesi gerekirse (raporla) |

## 5. Uygulama adımları

1. Aracı yaz; `--selftest`.
2. `build` → çıktıyı depo **dışına**, `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad/u3/out/moradon_1534.smd` yaz.
3. `patch-inbound` → 6 dosya (`karus_051221.smd`, `elmo_051221.smd`, `siege_0722.smd`, `freezone_a_20050718.smd`, `freezone_b_20050718.smd`, `In_dungeon_20050718.smd`; adları `/mnt/c/dev/fdp/server/Map`'te doğrula) → aynı çıktı dizini. Kaynak harita dizinine **yazma**.
4. `verify` → V1–V10 tablosu (beklenen değerler G §7).
5. Zone 71 parmak izi: yamalı `freezone_a_20050718.smd` için `tools/nav-export.py` crc32'si yamasız ile aynı.

## 6. Kabul kriterleri

- [ ] K1: `python3 -I tools/u3-moradon-smd.py --selftest` → OK.
- [ ] K2: `build` çıktısı 2.708.222 bayt ± (320 × warp sayısı farkı); yan JSON üretildi; aynı girdilerle ikinci `build` bayt bayt aynı (determinizm).
- [ ] K3: V1–V10 hepsi geçer (çıktı tablosu raporda, G §7 beklenenleriyle yan yana).
- [ ] K4: `patch-inbound` farkı yalnız `sZone==21` kayıtlarında 8 bayt (bayt düzeyinde fark dökümü); zone 71 crc32 aynı.
- [ ] K5: Depoya SMD/ikili eklenmedi; `git diff --stat main...bot/U3-01` yalnız §4.
- [ ] K6: Python LF, ASCII; `git status --short` temiz.

## 7. Doğrulama komutları

```bash
python3 -I tools/u3-moradon-smd.py --selftest
python3 -I tools/u3-moradon-smd.py build --client-zones "/mnt/c/dev/fdp1534/client/Knight Online/Zones" --donor "/mnt/c/dev/fdp1534/alpha/ALPHA KO 1534 PROJE/Server-Files/Map/moradon_0826.smd" --out <scratchpad>/u3/out/moradon_1534.smd
python3 -I tools/u3-moradon-smd.py verify --smd <scratchpad>/u3/out/moradon_1534.smd
git diff --stat main...bot/U3-01
git status --short
```

## 8. Kısıtlar ve uyarılar

- İndirilen paketteki exe/dll çalıştırılmaz; istemci ve ALPHA dosyaları yalnız okunur.
- `/mnt/c/dev/fdp/server/Map` ve `/mnt/c/dev/fdp1534*/server/Map` dizinlerine **yazma** (dağıtım Claude'un).
- DB yalnız `SELECT`; kişisel veri tabloları okunmaz.
- Git: `AGENTS.md` §2.8; commit `[U3-01] ...`.

---

## Uygulayıcı Raporu

### Tur 1

**Durum:** UYGULANDI (K1–K6 ✔). Uygulayıcı: Claude alt ajanı (proje sahibinin bu yükseltme fazı için verdiği talimatla, DeepSeek yerine), önceden açılmış çalışma ağacı `/mnt/c/dev/fdp-u3-01`. Aşağıda `<sp>` = `/tmp/claude-1000/-mnt-c-Users-frkoz-OneDrive-Desktop-Fire-Drake-Project-v1453/fbfeef86-7e71-4d83-9948-3d869babf2db/scratchpad`.

**Branch ve commit'ler:** `bot/U3-01` (taban `main` @ `435cf68e`)
- `d91aed95` [U3-01] Durum UYGULANIYOR
- `6a8b50b1` [U3-01] tools/u3-moradon-smd.py: yeni Moradon SMD ureteci, gelen warp yamasi, V1-V10 dogrulama
- bu rapor + `Durum: UYGULANDI` commit'i

**Değişen dosyalar**
- `tools/u3-moradon-smd.py` (YENİ, 1.588 satır, ASCII, LF, yalnız standart kütüphane, `python3 -I` ile çalışır). Prototip (`<sp>/k1534/g/u3_moradon_smd.py` ve yardımcıları) baştan yazıldı:
  - `build`: G §7 adım 1–8. İstemci başlığı iki biçimde denenir (1534: `int L, L bayt, int bayrak` → @15; eski: `int sürüm, int L, ad` → @8+L) ve yükü doğrulanan seçilir; `.gtd` n=257 zorunlu; yükseklikler 8 baytlık MAPDATA'dan transpozsuz, float bitleri aynen; `.opd` çarpışma bloğu bayt bayt (1024×1024 zorunlu); kural R (eşik 10,0 m, kenar satır/sütun 1); ALPHA nesne olayı bloğu aynen; regene 0; warp'lar ALPHA'dan zone 73 hariç, yalnız `dwPay` değişir. Çıktı kendi ayrıştırıcısıyla geri okunur, boyut formülle denetlenir. Yan JSON (`<çıktı>.json`): girdi/çıktı yol-boyut-md5, sayılar, blok ofsetleri, warp başına ücret kaynağı; zaman damgası yok, anahtarlar sıralı.
  - `patch-inbound`: her dosyada `sZone==21` kayıtlarının yalnız `fX`(+300)/`fZ`(+308) alanları yazılır; tüm dosya bayt farkı alınıp bu 8 baytın dışında fark varsa durur; `inbound_patch.json` raporu. Çıktı dizini `.../server/Map` veya girdi dizini ise reddeder.
  - `verify`: V1–V10 (aşağıda), herhangi bir FAIL'de çıkış 1, girdi hatasında 2. V1 depodaki `docs/appendix/tools/smd_parse.py` ile; V10'daki crc32 `tools/nav-export.py`'nin kendi `parse/write_navgrid/read_navgrid` işlevleriyle hesaplanır.
  - `--selftest`: sentetik testler her zaman (başlık iki biçim, çarpışma alt-hücre sırası, kural R: tam 10,0 m kapalı / 9,999 m açık / çarpışma / kenarlar, build gidiş-dönüş + `smd_parse` aynası + ücret eşlemesi + zone 73 düşürme + determinizm + spec + yinelenen kimlik hatası, yama baytları + idempotentlik, örnekleme, x–z üçgen/kare kesişimi, çıktı dizini koruması); dosya tabanlı testler (kural R: eski Moradon, 71, 72; uçtan uca: 71, 72) dosya yoksa `SKIP`.
- `docs/appendix/tools/smd_parse.py`: değişmedi (hata bulunmadı).

**Derleme:** C++ değişikliği yok; AGENTS.md §4 gereği `./tools/build.sh Release` çalıştırıldı (28 sn, çıkış 0, `error` 0; yalnız önceden var olan C4834 uyarıları). Bu ağacın sunucuları `[DOWN]` (çalışanlar `C:\dev\fdp-edit`'e ait, dokunulmadı). Son satırlar:
```
  proj-GameServer.vcxproj -> C:\dev\fdp-u3-01\build\bin\x86-Release\Server\GameServer.exe
  BotCoreTests.vcxproj -> C:\dev\fdp-u3-01\build\bin\x86-Release\Tests\BotCoreTests.exe
```

**Çıktılar (depo dışı, `<sp>/u3/out/`; depoya girmedi)**

| dosya | bayt | md5 |
|---|---|---|
| `moradon_1534.smd` | 2.708.222 | `cfbdc4051edc042d04049ad2a3bee6c1` |
| `moradon_1534.json` (yan JSON) | 5.459 | `df2971318b63a846b20365eb3334be82` |
| `karus_051221.smd` (yamalı) | 4.271.912 | `c85cad285ef665b4bad63d81c15c01bf` |
| `elmo_051221.smd` (yamalı) | 3.955.450 | `8f8f229c347ed17b545fe244f8ef66c0` |
| `siege_0722.smd` (yamalı) | 1.028.528 | `1447b1e401751b15b6ad1745cb92c69d` |
| `freezone_a_20050718.smd` (yamalı) | 4.679.624 | `afc4faee2cec65b09f7b54d031a7a77c` |
| `freezone_b_20050718.smd` (yamalı) | 1.522.560 | `e82289c915ff5e36279239258569c93d` |
| `In_dungeon_20050718.smd` (yamalı) | 211.466 | `77ac0f0447db72fe965a0d21fc86b372` |
| `inbound_patch.json` | 8.681 | `f38cdffbf134afb0b060ddce5cdd3c70` |

Girdiler değişmedi (koşu sonrası md5): bizim 6 harita + `moradon_20060124.smd` başlangıçtakiyle aynı (`83556c23…`, `cc811945…`, `082e99b6…`, `4baa01dd…`, `37d9abd2…`, `e473044d…`, `68b1f6c3…`); ALPHA `2cfce818…`; istemci `.gtd` `1867f157…`, `.opd` `b844a8a7…`, `.opdext` `81ce8dcf…` (G §1 ile aynı).

**Kabul kriterleri**

- ✔ **K1** `python3 -I tools/u3-moradon-smd.py --selftest` → çıkış 0:
```
selftest synthetic: OK (header, collision, rule R, build, patch bytes, sampling)
selftest OK   rule R old Moradon (zone 21)    moradon_20060124.smd n=129 agree 16641/16641 (100.0000%)
selftest OK   rule R Ronark Land (zone 71)    freezone_a_20050718.smd n=513 agree 263169/263169 (100.0000%)
selftest OK   rule R Ardream (zone 72)        freezone_b_20050718.smd n=257 agree 66049/66049 (100.0000%)
selftest OK   e2e Ronark Land (zone 71)    freezone_b.gtd/.opd -> freezone_a_20050718.smd n=513 heights equal 263169/263169, grid agree 263156/263169 (99.9951%) official0/gen1 6 official1/gen0 7
selftest OK   e2e Ardream (zone 72)        freezone.gtd/.opd -> freezone_b_20050718.smd n=257 heights equal 66049/66049, grid agree 66044/66049 (99.9924%) official0/gen1 4 official1/gen0 1
SELFTEST OK
```
  `--map-dir /nonexistent --client-zones /nonexistent` ile: sentetik testler OK, 5 dosya testi `SKIP`, `SELFTEST OK (5 reference tests skipped)`, çıkış 0. (Not: 1534 istemcisinde zone 71 = `freezone_b.*` (n=513), zone 72 = `freezone.*`; G §3.2/§4.2 ile aynı.)
- ✔ **K2** `build` (plan §7 komutu) → `bytes=2708222` (W=14, fark 0), çıkış 0; yan JSON üretildi. Aynı girdilerle ikinci `build` başka dizine: `cmp` aynı, md5 ikisi de `cfbdc405…`; yan JSON'lar yalnız `"path"` satırlarında farklı. Prototip çıktısıyla (`03429b74…`) `cmp -l`: yalnız 6 bayt, kayıt 3/10/12'nin `dwPay` alanı (2114, 2124, 2126; aşağıdaki ücret kuralı) — başka her bayt aynı.
- ✔ **K3** V1–V10 (tam koşu: `verify --smd <sp>/u3/out/moradon_1534.smd --spawns <sp>/u3/work/k_npcpos21_alpha.txt --zone-info <sp>/u3/work/zone_info_ours.txt --patched-dir <sp>/u3/out` → `VERIFY OK (10 PASS, 0 FAIL, 0 SKIP; 4 WARN)`, çıkış 0):

| id | sonuç | ölçülen | G §7 beklenen |
|---|---|---|---|
| V1 | PASS | `smd_parse.parse`: size check ok, trailing 0, n 257, unit 4,0, faces 26.839, obj 29, regene 0, warps 14, boyut 2.708.222 | ok / 0 / faces 26.839, obj 29, regene 0, warps 14 |
| V2 | PASS | yükseklik 66.049/66.049 (float bitleri aynı); ALPHA^T 58.738 (%88,9), ALPHA düz 4.528 (%6,9) | 66.049/66.049; ALPHA^T %88,9 |
| V3 | PASS | çıktı bloğu = `.opd` bloğu (@15, 2.306.732 B), md5 `4c83ae95211e18353cd208d504b367bb` ikisinde de | eşit |
| V4 | PASS | kendi verisi R'ye göre 66.049/66.049; eski Moradon / 71 / 72 %100; uçtan uca 71 %99,9951, 72 %99,9924 | %100; ≥ %99,99 |
| V5 | PASS | START 121/121; gelen r5 81/81; gelen `SelectWarpList` kutusu 121/121 (ek satır); Folk 80/81; Tale 73/81; MINI_ARENA 81/81; arena A 127/169, B 127/169 | 121/121, 81/81, 80/81, 73/81, 81/81, 127/169 |
| V6 | PASS | sunucu kuralı (ActType < 100, `AIServer/ServerDlg.cpp:268`): canavar merkezi 70/75, NPC 20/48. Prototip dışa aktarımıyla (kind sütunu): **64/69**, NPC 26/54. Kapalı karodaki 5 canavar her ikisinde aynı: 351, 554, 652, 1056, 1058 | ≥ 64/69 |
| V7 | PASS | %72,3 (47.353/65.536) vs %72,1 (11.814/16.384) | %72,3 vs %72,1 (±3) |
| V8 | PASS | yürünebilir 0 (416 yüz, dokunulan karo 3.160) | 0 |
| V9 | PASS* | 14/14: grup ∈ {211,212}, hedef zone ZONE_INFO'da (taze `FDP_kn_online` dökümü), merkez açık ve r5 ≥ %85 (diğer haritalarda 81/81, Folk 80/81, Tale 73/81). *4 WARN, bkz. Açık soru 3 | hepsi |
| V10 | PASS | 12 kayıt, 69 farklı bayt, hepsi fX/fZ içinde; zone 71 crc32 `4fd154bc` == `4fd154bc` | evet |

  Plan §7'deki yalın `verify --smd …` (varsayılanlar): `VERIFY OK (9 PASS, 0 FAIL, 1 SKIP; 4 WARN)` — V6 `--spawns` verilmediği için SKIP (dosya, `.\SQL2019`/`FDP_alpha1534`'ten `SELECT NpcID, ActType, LeftX, TopZ, RightX, BottomZ, NumNPC FROM K_NPCPOS WHERE ZoneID = 21`). Negatif denemeler: START karosu 0 yapılmış kopya → V4+V5 FAIL, çıkış 1; yamalı `siege_0722.smd` kopyasında `dwPay`'den 1 bayt değişik → V10 FAIL (`stray [1028180]`), çıkış 1; olmayan girdi → `ERROR: cannot read client heights …`, çıkış 2; `patch-inbound --out-dir /mnt/c/dev/fdp/server/Map` → `ERROR: refusing to write into a server Map directory`, çıkış 2, dosya değişmedi.
- ✔ **K4** `patch-inbound` (plan §5.3 dosyaları; adlar `/mnt/c/dev/fdp/server/Map`'te doğrulandı, hepsi 0 artık bayt): 6 dosya, 12 kayıt, çıkış 0. Bağımsız `cmp -l` (1 tabanlı ofsetler; her biri bir `sZone==21` kaydının +300..+303 / +308..+311 aralığında; boyutlar aynı):
```
karus_051221.smd: 4264214 4264215 4264216 4264223 4264224
elmo_051221.smd: 3949032 3949033 3949034 3949040 3949041 3949042
siege_0722.smd: 1027230 1027231 1027232 1027238 1027239 1027240 1027550 1027551 1027552 1027558 1027559 1027560 1028190 1028191 1028192 1028198 1028199 1028200 1028510 1028511 1028512 1028518 1028519 1028520
freezone_a_20050718.smd: 4678966 4678967 4678968 4678974 4678975 4678976 4679286 4679287 4679288 4679295 4679296
freezone_b_20050718.smd: 1522222 1522223 1522224 1522230 1522231 1522232 1522542 1522543 1522544 1522551 1522552
In_dungeon_20050718.smd: 211128 211129 211130 211136 211137 211138 211448 211449 211450 211456 211457 211458
```
  Kayıtlar (0 tabanlı kayıt ofseti): 114 @4263912 (288,369); 214 @3948730 (337,318); 3024 @1026928, 3044 @1027248 (337,318); 3014 @1027888, 3034 @1028208 (290,370); 7114 @4678664, 7214 @1521920 (295,368); 7124 @4678984, 7224 @1522240 (352,311); 8111 @210826 (285,372); 8121 @211146 (337,318) → hepsi (817,530); kayıt başına 5–6 bayt (float'ların bazı baytları aynı kaldığı için 8'den az). `fR`, `dwPay`, ad, ulus değişmedi.
  Zone 71 parmak izi, `tools/nav-export.py` ile (çıktılar `<sp>/u3/work/nav/`): yamasız `crc32=4fd154bc`, yamalı `crc32=4fd154bc` (n=513, events0=29522, events1=233647, main_component=88508 ikisinde de); iki `.navgrid` dosyası bayt bayt aynı.
- ✔ **K5** `git diff --stat main...bot/U3-01`: yalnız plan dosyası (Durum + bu rapor) ve `tools/u3-moradon-smd.py`; SMD/ikili yok.
- ✔ **K6** `file tools/u3-moradon-smd.py` → `Python script, ASCII text executable`, CR 0 (LF); `git status --short` boş. (`tools/nav-export.py` koşusunun `docs/appendix/tools/__pycache__/` altında bıraktığı yok sayılan önbelleği sildim.)

**Warp ücretleri (14 kayıt)**

| id | hedef | ALPHA | yazılan | kaynak |
|---|---|---|---|---|
| 2111 / 2121 | Folk Village (21) | 3000 | 3000 | **karşılığı yok → ALPHA** |
| 2112 / 2122 | Tale Village (21) | 3000 | 3000 | **karşılığı yok → ALPHA** |
| 2113 | Luferson Castle (1) | 5000 | 5000 | bizim 2111 |
| 2114 | Lunar Valley (1) | 10000 | 5000 | bizim 2114 |
| 2115 / 2125 | Delos (30) | 17000 | 17000 | bizim 2115 / 2125 |
| 2116 | Ardream (72) | 3000 | 3000 | bizim 2116 |
| 2118 | Ronark Land (71) | 17000 | 17000 | bizim 2113 |
| 2123 | El Morad Castle (2) | 5000 | 5000 | bizim 2121 |
| 2124 | Lunar Valley (2) | 10000 | 5000 | bizim 2124 |
| 2126 | Ardream (72) | 17000 | 3000 | bizim 2126 |
| 2128 | Ronark Land (71) | 17000 | 17000 | bizim 2123 |

Düşürülen: 2117, 2127 (Ronark Land Base, zone 73; ZONE_INFO'da yok).

**Plandan sapmalar**
1. **Ücret eşlemesi kimliğe göre değil, varış yerine göre.** Plan "aynı kimlikli warp'ın ücreti" diyor; ama bizim eski Moradon'da kimlikler başka yerlere gidiyor: salt kimlikle Folk Village (2111) bizim 2111 Luferson'un 5000'ini, Luferson (2113) bizim 2113 Ronark'ın 17000'ini, Folk (2121) bizim 2121 El Morad Castle'ın 5000'ini, El Morad Castle (2123) bizim 2123 Ronark'ın 17000'ini alırdı. ADR-0068 Ek 2 m.3'ün niyeti ("ücretler bizimki") için karşılık = aynı grup (`sWarpID/10`), aynı hedef zone ve aynı ad; birden çok eşleşmede aynı kimlik tercih edilir. Kimliği ve varış yeri aynı olan 2114/2115/2116/2124/2125/2126'da iki kural aynı sonucu verir. Soru 1.
2. **Ek seçenekler** (plan imzasının üstüne, varsayılanlarla plan komutları aynen çalışır): `build --fees-from SMD|alpha` (varsayılan `<map-dir>/moradon_20060124.smd`), `--drop-zones` (73), `--map-dir`; `verify --client-zones --donor --map-dir --zone-info --warps --drop-zones --patched-dir --inbound-x/z`. `--patched-dir` verilmezse `--smd`'nin dizini, 6 yamalı dosya oradaysa kullanılır. `--zone-info` verilmezse araçtaki `FDP_kn_online` ZONE_INFO anlık kopyası (2026-10-08) kullanılır; K3'te taze döküm verildi.
3. **V9 ölçütü:** G §7'nin tanımı (merkez + r5 diski, eşik %85) geçme koşuludur. Ben ayrıca sunucunun gerçek varış kümesini (`CUser::SelectWarpList`: `myrand(0, 2R)`, `< R` ise eksi → x,z + {−4..0, 5..10}, 121 nokta) ölçtüm; bunu ilk sürümde geçme koşulu yapmıştım ve 4 warp %85'in altında kaldı. G'nin tanımına döndüm, bu ölçüyü `WARN` olarak raporda bıraktım (gizlenmedi; Soru 3).
4. **V6 sınıflaması:** sunucu kuralı ActType < 100 → 75 canavar (70/75). G'nin 64/69'u prototip dışa aktarımına göre; aradaki 6 satır 551–553 (ActType 1; K_MONSTER'da Lycan/Loup-garou/Shadow seeker, K_NPC'de "Safety Zone Gate"), merkezlerinin hepsi açık. Dosyada `kind` sütunu varsa o kullanılır (G ile karşılaştırma için), yoksa ActType.
5. **V8:** dokunulan karo sayısı 3.160 (G 3.124). Ayırıcı eksen testiyle x–z kesişimi; yalnız kenar/köşe teması sayılmaz. Sonuç (yürünebilir 0) aynı.

**Açık sorular**
1. Ücret karşılığının kimlik yerine varış yeriyle (grup + hedef zone + ad) bulunması onaylanıyor mu? (Salt kimlik isteniyorsa yukarıdaki dört kayıt yanlış ücret alır.)
2. Folk Village (2111/2121) ve Tale Village (2112/2122) bizim eski Moradon'da yok; ALPHA ücreti 3000 kullanıldı. Proje sahibi başka ücret isterse `--warps` spec'iyle verilebilir.
3. Sunucunun gerçek varış kutusu (x,z + {−4..0, 5..10}) bazı hedeflerde kısmen kapalı karoya düşüyor: Tale Village (81,919) 67/121, Ardream 2126 (190,897) 93/121, Ronark 2128 (622,898) 99/121 (r5 diskinde hepsi ≥ 73/81). Karşılaştırma: bizim bugünkü hedeflerimiz de benzer (Ronark 2123 (630,920) 97/121, Ardream 2126 (193,898) 86/121) yani 71/72 için bu bugünkü durum. Yeni olan yalnız Tale Village; hedef birkaç metre kaydırılabilir veya kabul edilebilir (sunucu hareketi ızgarayla kısıtlamıyor). Karar Claude / proje sahibinde; `docs/KNOWN_ISSUES.md` adayı.
4. `FDP_kn1534` kurulduğunda (ZONE_INFO 21 → `moradon_1534.smd`) V9 o DB'nin `ZONE_INFO` dökümüyle `--zone-info` vererek yeniden koşulmalı; araçtaki anlık kopya `FDP_kn_online`'dandır.

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
