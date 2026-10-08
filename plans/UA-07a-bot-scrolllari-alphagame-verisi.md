# UA-07a: Bot scroll'ları AlphaGame verisinde (Attack+/Speed+ tanınmıyor) ve yedinci tür: Scroll of Advanced Strength

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | UA — AlphaGame tabanı, bot yeniden doğrulama (UA-07 dilimi) |
| Branch | `bot/UA-07a` (taban: `yukseltme/alpha`) |
| Bağımlı olduğu planlar | UA-04, UA-05b (DOĞRULANDI) |
| İlgili gereksinim / kabul | Proje sahibi 2026-10-09: "benim üzerimdeki bütün sc'lerden onlara da"; "speed+ potion kullanırken sprint kullanamamaları lazım" |
| Tahmini büyüklük | M |
| Hazırlayan / tarih | Claude / 2026-10-09 |

---

## 1. Amaç

AlphaGame tabanındaki ilk bot koşusunda (2026-10-09) botlar Attack+ Scroll (800014000) ve Speed+ Potion'ı (800015000) hiç kullanmadı; telemetri `SCROLL_STOCK counts [2,2,2,0,2,0] unusable 40`. Sebep `[V]`: `BotCore/ScrollUse.h` `ScrollShapeSupported` `reCastTime * 100 == info.reCastMs` ister; bizim veride 500034/500035 `ReCastTime` 255 (25,5 sn), AlphaGame `FDP_alpha_game`'de 0. Sonuç: warrior botlar hızı Sprint'le sağlıyor; Speed+ açık olsaydı `HasSprintBuff` (`WarriorPressure.h:151`) Sprint'i zaten durdururdu. Ayrıca proje sahibinin taşıdığı Scroll of Advanced Strength (800091000, skill 500501, BuffType 7, 1800 sn) botlarda tanımlı değil.

## 2. Bağlam (okunması zorunlu)

- `BotCore/ScrollUse.h` (F7-45; `ScrollInfoOf`, `ScrollShapeSupported`, `kScrollKinds`, `kScrollAllMask`), `BotCore/ScrollDrive.h`, `BotCore/ScrollReady.h`, `BotCore/RoamPrep*.h` (`critScrollMask`), `GameServer/Bot/BrainDriver.cpp` (stok sayımı ve maske kullanımı ~2640–2760, 3093), `GameServer/Bot/ActionExecutor.cpp` (`ScrollItemCheck`), telemetri adları (`ScrollDrive.h` `ScrollKindName`), ilgili `Tests/BotCoreTests/*Scroll*`.
- Veri (AlphaGame, `FDP_alpha_game`): 500034/500035/500053/500055 `Type1 4, Moral 1, CastTime 0, ReCastTime 0, UseItem = item`; 500501 `BuffType 7, Duration 1800, ReCastTime 0`; 500501'in `UseItem`'ı DB'de 810008000 (ITEM'de yok) — Claude `db/031` ile 800091000'e çeviriyor (bu planın dışında).
- `docs/11` §6.1 SCR-01..08, `docs/03` MEC-BUF-11 (scroll kuralları).

## 3. Kapsam

1. Attack ve Speed türlerinin beklenen `reCastMs`'i AlphaGame verisine (0) uyar; tercihen beklenen yeniden kullanım süresi sabit yerine canlı `MAGIC.ReCastTime`'dan okunur ve shape denetimi bu alanı karşılaştırmaz (diğer shape alanları aynen denetlenir). Seçimini raporda gerekçelendir.
2. Yedinci tür `Strength`: item 800091000, skill 500501, BuffType 7, 1800 sn, lockable (diğer stat scroll'ları gibi davranıp davranmadığını sunucu kodundan doğrula), öncelik sırası (D3) ve telemetri adı `strength`; `kScrollKinds`/`kScrollAllMask` ve bunları kullanan her yer (maske genişliği, diziler, `RoamPrep`, `BrainDriver`) tutarlı.
3. Testler: mevcut scroll testleri yeni değerlere; yeni tür için birim testleri (eksik → kullan, aktif → kullanma, stok yok → kısa stok raporu).

**Yok:** Sprint mantığı (zaten herhangi bir hız buff'ında durur); diğer bot davranışları; DB.

## 4. Dokunulabilecek dosyalar

`BotCore/Scroll*.h`, `BotCore/RoamPrep*.h` (yalnız maske/tür sayısı), `GameServer/Bot/BrainDriver.cpp`, `GameServer/Bot/ActionExecutor.cpp` (yalnız scroll kısmı), `Tests/BotCoreTests/*`, gerekirse `tools/*` öz-test dizgeleri (telemetri adı), `docs/` dokunulmaz (Claude günceller).

## 5. Kabul kriterleri

- [ ] K1: `./tools/build.sh Release --packet-trace --damage-trace` ve `./tools/build.sh Debug` 0 hata, yeni uyarı yok.
- [ ] K2: `./tools/run-tests.sh Release` 0 başarısız (yeni testler dahil sayı raporda).
- [ ] K3: AlphaGame satırlarıyla (500034, 500035, 500053, 500055, 500501 + 500051/500052) shape denetiminin her yedi tür için "usable" verdiği bir birim testi.
- [ ] K4: `git diff --stat` yalnız §4; `git status --short` temiz; ADR-0069 Ek 1 (altı içe aktarılmış dizinde bayt kuralı) gerekirse uygulanır.

## 6. Kısıtlar

- Sunucu başlatılmaz/durdurulmaz; DB'ye yazılmaz.
- Git: `AGENTS.md` §2.8; commit `[UA-07a] ...`; push yok.

---

## Uygulayıcı Raporu

(Uygulayıcı doldurur.)

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(Henüz yok.)
