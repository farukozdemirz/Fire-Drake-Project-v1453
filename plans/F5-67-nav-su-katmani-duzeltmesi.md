# F5-67: Su katmanı düzeltmesi (KOŞULLU): `water` işaretli hücreler için `NavGrid`/planlayıcı/kiriş denetimi

| Alan | Değer |
|---|---|
| Durum | İPTAL (2026-10-03: T-NAV-09 sonucu: istemci suya girip çıkıyor, suda hız düşmüyor, çukurlar `Walk` ile uyumlu; ayrı su katmanı gerekmez) |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-67 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F5-60** `KAPANDI` ve sonucu **`GEREKLİ`** olmalı (rapor + bu planın §2'sini dolduracak veri); F5-59 (`NavService`, ızgara kaynağı) `KAPANDI`; F5-61 (kiriş guard'ı) ile sıra: bu plan `water` işaretini kiriş denetimine de taşır (F5-61 önce veya birlikte). Şemsiye: F5-55 (dilim 9, koşullu) |
| İlgili gereksinim / kabul | `docs/12` §13.1 "Su", Q-26, T-NAV-09, CLI-08, AC-NAV-03; proje sahibi talebi: "suya takılmıyor" kabulü doğrulanmış veri olmadan verilmez |
| Tahmini büyüklük | M (kesin boyut F5-60 sonucuna bağlı; ≤ ~10 dosya hedefi) |
| Hazırlayan / tarih | Claude / 2026-10-03 (koşullu taslak) |

---

## Neden TASLAK

Bu plan **yalnızca** F5-60 "ayrı bir `water` katmanı gerekli mi?" sorusuna `GEREKLİ` yanıtı verirse yapılır. F5-60 `GEREKMEZ` verirse plan `İPTAL` olur; `BELİRSİZ` verirse (zemin gerçeği yok) T-NAV-09 insan testi sonucu beklenir. HAZIR yapmak için gereken ön koşullar:

1. F5-60 Uygulayıcı/Doğrulama Raporu: su-adayı kümesi `W` (maske veya T-NAV-09 nokta tablosu), `Walk ∩ W` hücre sayısı, planlayıcı yolu/kirişi temas sayıları (`WATER_PATHS`, `WATER_ROUTE`).
2. **Maske kaynağı kararı** (proje sahibi, ADR): F5-59 sunucuyu SMD belleğinden kurar ve dosya okumaz; su SMD'de **kodlu değildir**, yani maske dışarıdan gelmek zorundadır: (a) istemci verisinden üretilip `./Nav/zone71.water` olarak dağıtılan küçük dosya (F5-59'un "dosya yok" ilkesinden bilinçli sapma: dosya bayatlama riski, parmak izi gerekir), veya (b) koordinat dikdörtgenleri/çokgenleri olarak koda/ini'ye gömülen el yapımı liste (T-NAV-09 nokta tablosundan), veya (c) yükseklik kuralı (yalnızca T-NAV-09 "istemci düşük zemine giremiyor" derse ve eşik doğrulanırsa).
3. **Sert engel mi, maliyet mi** kararı (T-NAV-09 sonucuna göre): istemci suya **giremiyorsa** `water` = sert engel (`Walk` değil; CLI-08 kirişi de reddeder); girebiliyor ama **yavaşlıyorsa** maliyet katmanı (`NavCostLayer` benzeri) ve hız modeli (`docs/12` §6, CLI-05) ayrıca ele alınır.
4. Bağımlı planların `KAPANDI` olması ve §2'deki satırların bugünkü kodda yeniden doğrulanması.

## 1. Amaç

`water` olarak işaretlenen hücrelerin planlayıcı yollarına, düzleştirmeye ve **paket kirişlerine** (CLI-08) girmediğini garanti etmek; mevcut `NavGrid` yapısını (olay + ana bileşen + eğim) bozmadan, F5-60'ın ölçtüğü temas sayılarını **0**'a indirmek.

## 2. Bağlam (okunması zorunlu; HAZIR yapılırken doğrulanacak)

- `docs/12` §13.1 "Su" (F5-60 sonucu bu maddeyi günceller), §2 veri katmanları tablosuna eklenecek `water` satırı (Claude yazar).
- F5-60 raporu ve `tools/nav-measure` `water` bölümü çıktısı (temas sayıları: bu planın kabul tabanı).
- `BotCore/NavGrid.h:136` `Build` (ana bileşen: olay 1 ve kenara değmeyen en büyük 4-bağlantılı bileşen) ve `:30` `Init`; `EdgeOpen` (`Walk` kapısı); **gerçek-harita testleri `MainComponentCells() == 88508` sabitini kullanır** (örn. `Tests/BotCoreTests/NavArenaTests.cpp:329`, `:409`; `NavGridTests.cpp`, `NavPathTests.cpp` vb.): sert engel `Build`'in bileşen hesabından **önce** uygulanırsa ana bileşen büyüklüğü değişir ve bu sabitler (ve `nav-regress` tabanı `tools/nav-regress/good.txt`) birlikte güncellenmelidir. Bu yüzden önerilen yaklaşım: `water` işaretini ana bileşen hesabından **sonra** bir `walk` maskesi daraltması olarak uygulamak ve bileşen sayısının değişimini ayrıca raporlamak (kesin seçim HAZIR yapılırken ölçümle).
- `BotCore/NavDanger.h` (`NavCostLayer`, `Forbidden`: yasaklı hücreye dışarıdan girilemez, içeriden çıkış serbest, F5-06): sert-engel tasarımı için emsal; `BotCore/NavSegment.h` `NavCheckSegment` (F5-50) kiriş denetimi `Walk` süpercover'ıdır: `water` daraltılmış `Walk`'ta kalıyorsa kiriş denetimi **değişmeden** korur.
- F5-59 `NavService` (maske kaynağı ve kurulum noktası), F5-61 (guard bağlama).

## 3. Kapsam (taslak)

**Neyin değişeceği (F5-60 `GEREKLİ` ise):**

- **NavGrid katmanı:** `water` işareti (`std::vector<uint8_t>`, n·n) ve `NavGrid::ApplyWater(const uint8_t * mask)` (veya `NavParams`) ile `Walk`/`clearance` yeniden hesabı; `water` hücreler `Walk` olmaz (`EdgeOpen` otomatik reddeder, `NavSegment`/`NavLos` değişmez). Maske boyutu ≠ n·n → reddedilir.
- **Planlayıcı:** sert engel için `NavPathfinder` değişmez (yalnızca `Walk` kapısı); maliyet seçeneği seçilirse `NavCostLayer`'a "ıslak" hücre cezası eklenir (maliyet yönü `docs/12` §4.1 formülüne uygun, ceza ≥ 0 kalır, sezgisel tutarlı).
- **Kiriş denetimi:** `NavCheckSegment` `Walk` temelli olduğundan `water` kirişlerini de reddeder; F5-61 guard testlerine bir "su kirişi" vakası eklenir.
- **Testler:** sentetik göl (kıyı + ortada ada): yol göle girmez, kiriş reddedilir; `water` yokken eski çıktı bayt düzeyinde aynı; gerçek harita: F5-60 `WATER_PATHS`/`WATER_ROUTE` temas sayıları **0**; `nav-regress` tabanı güncellenir.
- **Sunucu:** `NavService::Startup()` maskeyi (kaynak kararına göre) yükleyip `ApplyWater` çağırır; parmak izine maske katılır.

**Kapsam dışı:** istemcinin suda yavaşlama modeli (`docs/12` §6 hız) — yalnızca T-NAV-09 "yavaşlıyor" derse ayrı plan; yüzme/su içi hareket; zone 71 dışı haritalar.

## 4. Dokunulabilecek dosyalar (tahmini; HAZIR yapılırken kesinleşir)

| Dosya | İşlem | Not |
|---|---|---|
| `BotCore/NavGrid.h` | değiştir | `ApplyWater` ve maske alanı |
| `Tests/BotCoreTests/NavGridTests.cpp` (veya yeni `NavWaterTests.cpp` + `.vcxproj`) | değiştir/yeni | sentetik göl, eski çıktı aynı |
| `Tests/BotCoreTests/NavSegmentTests.cpp` | değiştir | su kirişi reddi |
| `GameServer/Bot/NavService.{h,cpp}` | değiştir | maskeyi yükle ve uygula |
| `BotCore/NavFingerprint.h` | değiştir (gerekirse) | maske parmak izi |
| `tools/nav-regress/good.txt` | değiştir | taban güncelle (ana bileşen sayısı değişirse) |

## 5. Uygulama adımları (taslak)

1. F5-60 raporundaki `W` kümesini ve karar satırını doğrula; maske kaynağı ve sert/maliyet kararı ADR'de.
2. `NavGrid::ApplyWater` + birim testleri (sentetik); mevcut testlerin hepsi geçmeli (maske yokken davranış değişmez).
3. `NavService` maske yükleme; parmak izi.
4. Gerçek harita: `./tools/nav-measure.sh water --n 5000` temas sayıları **0**; `nav-regress` güncellemesi.
5. F5-61 guard testine su kirişi vakası.

## 6. Kabul kriterleri (taslak)

- [ ] K1: `./tools/build.sh Release|Debug` rc=0; `./tools/run-tests.sh` `0 failed`; `water` maskesi yokken tüm mevcut çıktılar ve testler değişmez
- [ ] K2: `./tools/nav-measure.sh water --n 5000 --mask <maske>`: `paths_with_w_cell=0`, `smooth_segments_touching_w=0`, `chords_touching_w=0`, iki doğuş → arena rotasında `cells_in_w=0`
- [ ] K3: sentetik göl testlerinde yol/kiriş su hücresine girmez; `NavCheckSegment` su kirişini reddeder
- [ ] K4 (proje sahibi, ayrı): T-NAV-09 sonucu (§ F5-60 §9) ile maske/sert-engel kararı uyumlu; oyun içi doğrulama F5-66'dadır

## 7. Doğrulama komutları

```bash
./tools/build.sh Release && ./tools/run-tests.sh Release
./tools/nav-measure.sh water --n 5000 --mask build/nav/zone71.water
./tools/nav-regress.sh --skip-timing
git diff --stat gece/2026-10-02...bot/F5-67
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §3. `[BOT] NAV=0` ve `ENABLED=0` davranışı değişmez.
- **Dürüstlük:** su katmanı ancak F5-60 `GEREKLİ` ve maske kaynağı/sert-maliyet kararı verilmişse yazılır; vekil (yükseklik) kuralıyla veri uydurulmaz. Birim testi geçmesi "suya takılmıyor" demek değildir: oyun içi kanıt T-NAV-09 + F5-66.
- Bu plan HAZIR olmadan uygulanmaz.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F5-67` — `<kısa-sha> [F5-67] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `gece/2026-10-02...bot/F5-67` @ `<sha>`
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
