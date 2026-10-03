# F5-60: Su ve engel verisi denetimi (çevrimdışı): harita verisi su geçişini gerçekten engelliyor mu? (`nav_measure water` bölümü + `tools/nav-water-audit.py`)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-03, gece/2026-10-02, merge `2e85af8`) |
| Faz | F5 — Navigasyon (`docs/17` §2; kapı G5) |
| Branch | `bot/F5-60 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | F5-01 (`NavGrid`), F5-02 (`NavPathfinder`), F5-03 (`NavSmoothPath`), F5-11 (`tools/nav-measure*`, `nav-regress*`), F5-50 (`NavSegment`), F5-58 — hepsi `KAPANDI`. Şemsiye: F5-55 (dilim 2). F5-59'dan **bağımsızdır** (sunucusuz) |
| İlgili gereksinim / kabul | `docs/12` §13.1 ("Su" maddesi), Q-26 (`docs/18` satır 75), T-NAV-09 (`docs/15` satır 168), DEG-18 (`docs/reports/degerlendirme-2026-10-02.md`), CLI-08; proje sahibi talebi (2026-10-03): "Su katmanının eksikliğini açıkça ele al. Harita verisinin su geçişlerini gerçekten engellediğini doğrulamadan 'suya takılmıyor' kabulü verme." |
| Tahmini büyüklük | S–M (2 dosya; çevrimdışı; ~1 gün, adım 5 zaman kutulu) |
| Hazırlayan / tarih | Claude / 2026-10-03 |

---

## 1. Amaç

Bugünkü navigasyon **ayrı bir su katmanı içermez** (`docs/12` §13.1 "Su"): ızgara yalnızca SMD olay ızgarasına (0 = engelli, 1 = açık) ve ana-bileşen kuralına dayanır. Bu planın sonunda, **sunucu çalıştırmadan** (a) zone 71 verisinde su/geçilemez alanın nasıl kodlandığı ve nasıl **kodlanmadığı**, (b) `NavGrid`'in neyi engel saydığı, (c) planlayıcı yollarının ve paket kirişlerinin "su adayı" hücrelere girip girmediği ölçülmüş, (d) bir **doğrulanmış su zemin gerçeği** (ground truth) bulunup bulunamadığı kayda geçmiş ve (e) şu karar yazılmış olur: **ayrı bir `water` katmanı gerekli mi?** (`GEREKLİ` / `GEREKMEZ` / `BELİRSİZ: T-NAV-09 insan testi bekliyor`). Zemin gerçeği olmadan `GEREKMEZ` ve "suya takılmıyor" yazılamaz (§3 karar kuralı). İnsan istemcisi gerektiren T-NAV-09 bu planın kabulünün **dışında**, "proje sahibi testi" olarak §9'da tanımlıdır.

## 2. Bağlam (okunması zorunlu)

Satır numaraları `gece/2026-10-02` @ `c2c5a08` üzerinde Claude tarafından yeniden doğrulandı; uygulayıcı kendi çalışma ağacında teyit etsin, kayma varsa **dur** (`AGENTS.md` §7).

- `docs/12_NAVIGATION_AND_POSITIONING.md` §1 (tablo: olay ızgarası 0/1, "Göller: iç kısımlar ana alandan kopuk yürünebilir cepler, kıyılar engelli `[V]`/`[I]`", ana bileşen 88 508 hücre, dış bant 115 363), §2 (`walk` = olay 1 ve ana bileşen; ayrı su/`slope` katmanı yok), §13.1 son madde (**"Su: ayrı bir su katmanı yoktur … istemcinin suya girip girmediği ve suda yavaşlayıp yavaşlamadığı ölçülmedi `[A]` → T-NAV-09"**), `docs/18` Q-26, `docs/15` satır 168 (T-NAV-09).
- **Haritada su verisi (kanıt araması, Claude, 2026-10-03, `git grep -n -a -i -E "water|swim|liquid|\bpond\b|\briver\b|\blake\b" gece/2026-10-02 -- GameServer shared AIServer N3BASE LogInServer`):** anlamlı eşleşme **yok** (yalnızca `GameServer/GameServerDlg.cpp:3188` `"Lake of Life"` anıt adı). Yani **sunucu ve ortak kodda su, yüzme veya sığ/derin su kavramı yoktur**; sunucu hareketi yalnızca harita sınırıyla doğrular (`shared/SMDFile.cpp:194-198` `IsValidPosition`; `docs/12` §1, MEC-MOV-03). SMD biçimi: `m_nMapSize`, `m_fUnitDist`, yükseklik `float[n²]`, çarpışma verisi, nesne olayları, olay ızgarası `short[n²]` (`shared/SMDFile.cpp:75-102, 104-134`; `docs/appendix/tools/smd_parse.py` başlığı); su için ayrı bir alan **yoktur** `[V: kod]`. Olay ızgarası değerleri: zone 71'de yalnızca `0` (29 522) ve `1` (233 647) `[V ön ölçüm, Claude, Python, 2026-10-03; uygulayıcı yeniden üretsin]`.
- **Çarpışma geometrisi** (34 871 yüz) yüklenir ama sunucuda sorgulanmaz (`docs/12` §1; `N3BASE/N3ShapeMgr.cpp:52-114`). İstemci kaynak kodu depoda **yoktur** (`N3BASE/` yalnızca `My_3DStruct.h`, `N3ShapeMgr.*`, `stdafx.h` içerir): istemcinin suya girip giremediği, suda yavaşlayıp yavaşlamadığı koddan **doğrulanamaz**.
- **İstemci verisi ipuçları (kod değil, dosya adları; yorum yapılmamıştır):** `/mnt/c/dev/fdp/Client/Zones/freezone_a.gtd` (679 959 bayt; biçimi depoda belgelenmemiştir, düz `strings` çıktısında yalnızca `ka_water.dxt` adı 8 kez geçer), `freezone_a.opd` içinde nesne adları `object\obj_el_lakefog01.n3pmesh` ve `object\obj_el_transmark00_water.n3pmesh`; `Client/Object/me_water00xx.dxt`, `Client/Misc/river/*water*.dxt` doku dosyaları. Bunlar suyun arazi doku indeksiyle ve/veya ayrı göl/nehir nesneleriyle çizildiğine **işaret edebilir `[A]`**; olay ızgarasıyla ilişkisi **doğrulanmamıştır**. Adım 5 (keşif) bunu zaman kutulu araştırır.
- `BotCore/NavGrid.h:30` `Init`, `:88-103` `Load`, `:136` `Build`: `Walk` = olay 1 **ve** haritanın kenarına değmeyen en büyük 4-bağlantılı bileşen (`:150` bileşen etiketleme, `:200` ana bileşen seçimi); `EdgeOpen` eğim (`maxSlope = 0,625` `[A]`) ve köşe kesme kuralı; **yükseklik yalnızca eğim kuralında kullanılır**, düşük/yüksek zemin engel değildir. Yani su, **ancak olay ızgarası onu 0 yapmışsa** (veya yalnızca ana bileşene bağlı değilse) engeldir.
- Araçlar: `tools/nav-export.py:4-12, 52` (SMD → `build/nav/zone71.navgrid`; biçim), `tools/nav-measure.sh` (`g++ -O2`, `build/nav/zone71.navgrid` yoksa `nav-export.py` ile üretir; `Ctx` `tools/nav-measure/nav_measure.cpp:160-166` {grid, walk, seed, n}; `NearestWalk :168`, `RandomNear :184`, `TouchedCells :58` (muhafazakâr süpercover), `Smoothing :196` (yol/düzleştirme/kiriş örnekleme kalıbı), `main :1070`, kullanım metni `:1074`, `--n` ayrıştırma `:1086`, bölüm yönlendirmesi ve `all` listesi `:1101-1119`), `tools/nav-segment-check.py:1-80` (bağımsız Python süpercover/`Walk` maskesi; `load_navgrid` yalnızca olayları okur: yükseklik için yeni okuyucu gerekir), `tools/nav-regress.sh/.py` (`all` çıktısını izler: **yeni bölüm `all`'a eklenmez**, regresyon çıktısı değişmez).
- Bu worktree'de `build/nav/zone71.navgrid` **yoktur** (`build*/` `.gitignore`'da): uygulayıcı önce `python3 tools/nav-export.py` çalıştırır.

### Planlayıcının ön ölçümü `[V ön ölçüm: Claude, Python, WSL, 2026-10-03; uygulayıcı bağımsız yeniden üretmeli, farklıysa nedenini raporlamalı]`

`/mnt/c/dev/fdp/server/Map/freezone_a_20050718.smd` (aynı dosyadan `smd_parse.py` ile; olay ızgarası, 4-bağlantılı bileşenler):

- Olay: `1` = 233 647 hücre, `0` = 29 522. Ana bileşen (`Walk`) 88 508 hücre (`docs/12` §1 ile aynı). Ana bileşen **dışında**, kenara değmeyen 633 "cep" (toplam 28 501 hücre); kenara değen bileşenler (dış bant) toplam 116 638 hücre. Cep hücrelerinden yalnızca 37'si bir `Walk` hücreye 8-komşu: cepler ana alandan neredeyse tamamen kopuk (docs/12 §1'in "iç kısımlar ana alandan kopuk" gözlemiyle uyumlu). Birçok cebin en düşük yüksekliği tam `−30,63`: **sentinel/boş zemin olabilir `[A]`**.
- `Walk` hücrelerinin yükseklik dağılımı: min −7,38; p1 −6,08; p5 −0,56; medyan 2,58; p95 15,91; maks 37,77. **`Walk` hücrelerinden 3 830'u < −1,0 m**, 3 227'si < −2,0 m, 934'ü < −6,0 m.
- `Walk` ∩ (yükseklik < −1,0 m) hücrelerinin 4-bağlantılı bileşenlerinden ≥ 200 hücrelik **5 "havza"** (toplam 3 739 hücre): 1 820 hücre (dünya x 1268-1636, z 1164-1376, yükseklik −7,38…−1,0), 1 225 (x 404-720, z 692-820, −7,07…−1,01), 247 (x 812-856, z 884-1064), 228 (x 1164-1236, z 940-1112), 219 (x 424-512, z 1096-1220). Havza hücrelerinin yalnızca 38-167'si bir olay-0 hücresine değiyor (yani havza kıyısı çoğunlukla **engelli değil**). El Morad doğuşu (≈ (630, 920)) ve Karus doğuşu (≈ (1380, 1090)) bu havzaların yakınındadır.
- **Yorum sınırı:** bu sayılar **suyun varlığını göstermez**; yalnızca "yürünebilir ama çevresine göre çukur" bölgeleri gösterir. Bunların su mu, vadi mi olduğu bilinmiyor. Bu plan tam olarak bunu ölçülebilir hale getirir; ön sayılar bir **varsayım** olarak okunur `[A]`.

## 3. Kapsam

**Karar kuralı (planın çıktısını sınırlar; sonuç bu kuraldan sapamaz):**

| Çevrimdışı kanıt | Karar (`docs/12` §13.1'e yazılır) | F5-67 |
|---|---|---|
| **Doğrulanmış su zemin gerçeği var** (adım 5 sonucu `KABUL`) ve `Walk` ∩ su > 0 **veya** planlayıcı yolu/kirişi su maskesine değiyor > 0 | **`GEREKLİ`:** ayrı `water` katmanı şart | F5-67 `HAZIR` yazılır |
| Doğrulanmış su zemin gerçeği var ve `Walk` ∩ su = 0 **ve** yol/kiriş teması = 0 (≥ 5000 çift) | **`GEREKMEZ` (yalnızca çevrimdışı veri düzeyinde):** veri su geçişini engelliyor; istemci davranışı yine T-NAV-09 ile doğrulanır | F5-67 `İPTAL` (Claude) |
| Zemin gerçeği **yok / doğrulanamadı** | **`BELİRSİZ`:** "suya takılmıyor" kabulü **verilmez**; çevrimdışı denetim yalnızca "su adayı" maruziyetini ölçer. Karar T-NAV-09 (proje sahibi testi, §9) sonucuna bağlıdır | F5-67 `TASLAK` kalır |

"Doğrulanmış zemin gerçeği" yalnızca adım 5'in `KABUL` ölçütlerini sağlayan bir su maskesidir; yükseklik tabanlı vekil (havza) zemin gerçeği **değildir**.

**Yapılacaklar**

1. `tools/nav-measure/nav_measure.cpp`: yeni `water` bölümü (§5 adım 2): olay/`Walk` istatistiği, cep ve havza sayımı, su-adayı kümesi `W`, ≥ 5000 çift için yol/düzleştirilmiş segment/6,75 m kiriş temas sayımı, iki doğuş → arena rotası, isteğe bağlı doğrulanmış maske (`--mask`).
2. `tools/nav-water-audit.py` (yeni, yalnızca standart kütüphane): (a) `basins`: C++ bölümünden **bağımsız** Python uygulaması (olay/cep/havza sayıları, aynı çıktı anahtarları; çapraz kontrol), (b) `probe-client`: istemci veri dosyalarında su ile ilgili adları **olduğu gibi** listeleyen keşif komutu.
3. Zaman kutulu keşif (adım 5): istemci verisinden bir **su maskesi** çıkarılabiliyor mu; ya doğrulanmış maske (`KABUL`) ya da "doğrulanamadı" sonucu.
4. Ölçüm çıktısı ve karar Uygulayıcı Raporu'na; Claude `docs/12` §13.1, Q-26 ve `docs/15` T-NAV-09 satırına işler (uygulayıcı `docs/`'a dokunmaz).

**Kapsam dışı (yapılmayacak)**

- `BotCore/*` değişikliği (su katmanı **eklenmez**: o F5-67'dir, koşulludur), `GameServer/`, `shared/`, `Tests/` değişikliği, yeni birim testi (bu bir ölçüm/denetim aracı; kalıcı regresyon kararı `GEREKLİ`/`GEREKMEZ` sonrası Claude'dadır).
- `water` bölümünü `nav_measure all`'a eklemek (F5-11 regresyon çıktısı değişmesin).
- Sunucu çalıştırmak, istemciye girmek, DB'ye bağlanmak, istemci dosyalarını **değiştirmek** (yalnızca okuma).
- İstemcinin suda yavaşlayıp yavaşlamadığı ve suya girip giremediği (bu bir **insan istemcisi ölçümüdür**: T-NAV-09, proje sahibi testi §9; kabul kriteri değildir).
- `docs/` değişikliği.

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/nav-measure/nav_measure.cpp` | değiştir | yalnızca yeni `water` bölümü, yeni `Ctx` alanları, `main` içinde yeni bayraklar ve bir `if (section == "water")` dalı; **mevcut bölümlerin çıktısı bayt düzeyinde değişmez** (K6) |
| `tools/nav-water-audit.py` | yeni | yalnızca standart kütüphane; ASCII; `--selftest` içerir |

`BotCore/`, `Tests/`, `GameServer/`, `shared/`, `.vcxproj`, `docs/`, `tools/nav-measure.sh`, `tools/nav-regress.*` dosyalarına **dokunulmaz**. Listede olmayan bir dosya gerekirse **durup** soru olarak yaz.

## 5. Uygulama adımları

1. `git switch -c bot/F5-60 gece/2026-10-02`; `Durum` → `UYGULANIYOR`. `python3 tools/nav-export.py` (çıktı satırını rapora yaz). Sunucuya hiç dokunma (paralel hat olabilir). Değişiklik **öncesi** taban çıktısını kaydet: `mkdir -p build/nav-measure && ./tools/nav-regress.sh --skip-timing > build/nav-measure/regress-before.txt; echo rc=$?` (rc'yi rapora yaz).
2. **`nav_measure.cpp` `water` bölümü** (`Smoothing`'in örnekleme kalıbını yeniden kullan; mevcut işlevlere dokunma). Yeni bayraklar (mevcut `i += 2` ayrıştırıcısına eklenir): `--water-t <float>` (varsayılan `-1.0`), `--water-min <int>` (varsayılan `200`), `--mask <yol>` (isteğe bağlı doğrulanmış su maskesi: `n*n` bayt, `x*n + z` sırası, `0/1`). Bölüm şunları yazar (her sonuç tek `KEY değer ...` satırı):
   - `WATER_GRID n=… unit=… events0=… events1=… walk=… event1_not_walk=… pockets=… pocket_cells=… pocket_cells_adjacent_to_walk=…` (cep = olay 1, `Walk` değil, kenara değmeyen 4-bağlantılı bileşen; `walk` = `MainComponentCells()`).
   - `WATER_HEIGHT walk_hmin=… walk_p1=… walk_p5=… walk_p50=… walk_p95=… walk_hmax=… walk_below_0=… walk_below_m1=… walk_below_m2=… walk_below_m4=… walk_below_m6=…` (`Walk` hücreleri; `NavGrid::Height`).
   - **Havzalar (vekil su adayı):** `Walk(x,z) && Height(x,z) < waterT` hücrelerinin 4-bağlantılı bileşenleri; `cells >= waterMin` olanlar "havza". Her havza için `WATER_BASIN id=… cells=… hmin=… hmax=… x=[min,max] z=[min,max] edge0_touch=…` (dünya metresi; `edge0_touch` = 8-komşusunda olay-0 hücresi olan havza hücresi sayısı; bileşenler `Walk` tarama sırası x-dış/z-iç ile numaralanır, ilk bulunan küçük id) ve özet `WATER_BASIN_SUMMARY t=… min_cells=… count=… cells=…`.
   - **Su-adayı kümesi `W`:** `--mask` verilmemişse `W` = havza hücreleri (`WATER_SOURCE proxy_basins`); verilmişse `W` = `Walk ∩ maske` ve ayrıca `WATER_TRUTH mask_cells=… mask_and_walk=… mask_not_walk=…` yazılır (`WATER_SOURCE mask:<yol>`). Maske yokken **hiçbir satırda "su engelli", "suya takılmıyor" veya benzeri bir yargı basılmaz**; bunun yerine `WATER_TRUTH none`.
   - **Kıyı bandı (bilgi):** `W` dışındaki, `W` hücresine 8-komşu `Walk` hücre sayısı: `WATER_SHORE cells=…`.
   - **Planlayıcı denetimi:** `c.n` çift (varsayılan 1000; **K için `--n 5000` veya üstü**), karışım `Smoothing` ile aynı: yaklaşık %40 `RandomNear(cheb 64)`, %30 `cheb 150`, %20 `cheb 40`, %10 tüm haritadan rastgele iki `Walk` hücre; `NavPathfinder::Find` (`NavSearchParams` varsayılanı, maliyet alanı yok). Her `Found` yol için: (i) ham A* hücreleri içinde `W` hücre sayısı ve **`Walk` olmayan hücre sayısı** (sağlama: 0 olmalı), (ii) `NavSmoothPath` segmentlerinin `TouchedCells` süpercover'ında `W` teması, (iii) 6,75 m paket kirişleri (`Smoothing`'deki döngü kalıbı) `W` teması. Çıktı: `WATER_PATHS pairs=… found=… nodelimit=… nopath=… non_walk_cells_on_path=… paths_with_w_cell=… raw_cells_in_w=… smooth_segments=… smooth_segments_touching_w=… chords=… chords_touching_w=…` ve (varsa) ilk 5 örnek `EXAMPLE water path (x,z)->(x,z) cell (cx,cz) in W`.
   - **Doğuş → arena rotaları:** Karus (1385, 1095) → arena (1274, 890) ve El Morad (635, 925) → arena (1274, 890) (`NearestWalk`, maliyet alanı yok; `NavArenaTests.cpp:320-345` kalıbı): `WATER_ROUTE name=karus|elmorad length_m=… cells=… cells_in_w=… segments_touching_w=…`.
   - `all` ve diğer bölümler **değişmez**. Kullanım satırına (`:1074`) `water` eklenebilir (yalnızca metin).
3. **`tools/nav-water-audit.py`** (iskelet; ASCII, İngilizce yorum):

   ```python
   #!/usr/bin/env python3
   """Water / blocker audit helpers for the zone 71 navigation grid (F5-60).
   basins:       independent re-implementation of the nav_measure `water` grid/basin counts (cross-check oracle).
   probe-client: list water-related names found in the client data files (facts only, no interpretation).
   Usage:
       python3 tools/nav-water-audit.py basins [--navgrid build/nav/zone71.navgrid] [--t -1.0] [--min 200]
       python3 tools/nav-water-audit.py probe-client [--client-dir /mnt/c/dev/fdp/Client]
       python3 tools/nav-water-audit.py --selftest
   """
   # load_navgrid(path) -> n, unit, events(array 'h'), heights(array 'f')   # FDPNAV01 layout, see tools/nav-export.py
   # walk_mask(n, events)  -> same rule as BotCore::NavGrid::Build (largest 4-connected event==1 component not touching the edge)
   # basins(n, unit, events, heights, walk, t, min_cells) -> prints WATER_GRID / WATER_HEIGHT / WATER_BASIN / WATER_BASIN_SUMMARY
   ```

   `basins` çıktısı C++ bölümüyle **aynı anahtarları ve aynı sayıları** üretmelidir (`WATER_GRID`, `WATER_HEIGHT` sayıları, `WATER_BASIN` ve `WATER_BASIN_SUMMARY`; `walk_p*` yüzdelikleri aynı `Walk` hücre listesinin sıralı değerinde `int(len*q)` indeksiyle tanımlanır; iki uygulamada **aynı tanım** kullanılmalı, farklıysa hangisinin neden değiştiğini raporla). `--selftest`: küçük sentetik ızgarada (ör. 12×12: bir çukur havza, bir cep, kenar bileşeni) beklenen sayıları sınar; `SELFTEST OK` basar. `probe-client`: `Zones/freezone_a.*` dosyalarının baytlarında ASCII dizeleri (≥ 5 karakter) tarar ve `water|lake|river|pond` içerenleri (dosya, bayt konumu, dize, sayı) listeler; `Misc/river`, `Object` içindeki ad kalıplarını listeler. **Yorum yapmaz, maske üretmez** (maske üretimi adım 5'tir).
4. **Çalıştır ve doğrula:** `python3 tools/nav-water-audit.py --selftest`; `./tools/nav-measure.sh water --n 5000 > build/nav-measure/water-audit.txt` (çıktıyı rapora yapıştır; dosya commit edilmez); `python3 tools/nav-water-audit.py basins` çıktısını C++ ile satır satır karşılaştır (sayıları ve anahtarları). Planlayıcı ön ölçümüyle (§2) karşılaştır: farkları raporla.
5. **Zaman kutulu keşif (en çok ~2 saat; sonuç ya `KABUL` ya "doğrulanamadı"):** `python3 tools/nav-water-audit.py probe-client` çıktısını rapora yapıştır. Ardından `Client/Zones/freezone_a.gtd` (ve gerekirse `.opd`/`.opdext`) biçiminin çözülüp çözülemediğini araştır: arazi doku tablosunda `ka_water.dxt`'nin doku indeksini bulup hücre başına doku indeksi alanı var mı; alan 513×513 köşe veya 512×512 karo ızgarasına eşleniyor mu. **Hiçbir dosya değiştirilmez.** İki olası sonuç:
   - **`KABUL`:** bir su maskesi (`build/nav/zone71.water`, `n*n` bayt, `x*n + z`, `0/1`; commit edilmez) üretildi ve **şunların hepsi** raporlandı: (i) çözülen biçim (alan konumları, boyutlar, doğrulama: dosya boyutu aritmetiği tam tutuyor), (ii) maskenin ızgaraya eşlenme kanıtı (ör. maske hücreleri ile havza/cep kümelerinin kesişim oranları; rastgele eşlemeye göre anlamlı fark), (iii) mask yüzdesi ve `WATER_TRUTH` satırı. Ardından `./tools/nav-measure.sh water --n 5000 --mask build/nav/zone71.water` yeniden koşulur.
   - **"Doğrulanamadı":** biçim çözülemedi veya eşleme kanıtlanamadı. Hangi adımda durduğunu ve neden güvenilir olmadığını yaz. **Zayıf kanıtla maske üretip `KABUL` yazma.** Karar tablosundaki üçüncü satır geçerlidir.
6. Karar önerisini (`GEREKLİ`/`GEREKMEZ`/`BELİRSİZ`) §3 tablosuna **mekanik olarak** uygulayarak Uygulayıcı Raporu'nun başına yaz; Claude `docs/`'a işler. `Durum` → `UYGULANDI`.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/nav-water-audit.py --selftest` → `SELFTEST OK`, rc=0
- [ ] K2: `./tools/nav-measure.sh water --n 5000` rc=0; çıktıda `WATER_GRID`, `WATER_HEIGHT`, `WATER_BASIN_SUMMARY`, `WATER_SOURCE`, `WATER_TRUTH`, `WATER_PATHS` (`pairs=5000` veya daha fazla, `found > 0`), iki `WATER_ROUTE` satırı var; `WATER_PATHS` satırında **`non_walk_cells_on_path=0`**
- [ ] K3: `python3 tools/nav-water-audit.py basins` ile C++ `water` bölümünün `WATER_GRID`, `WATER_HEIGHT` ve `WATER_BASIN`/`WATER_BASIN_SUMMARY` sayıları **birebir aynı** (iki çıktı rapora yan yana); farklıysa fark nedenle açıklanmış ve **hangisinin doğru olduğu bağımsız bir hesapla** gösterilmiş
- [ ] K4: Planlayıcı ön ölçümüyle (§2: `main=88508`, 633 cep/28 501 hücre, `Walk` < −1,0 m = 3 830, 5 havza/3 739 hücre) uyum; sapma varsa nedeni raporda (K başarısızlığı değil, bulgu)
- [ ] K5: **Zemin gerçeği durumu açık:** rapor, adım 5 sonucunu `KABUL` veya `doğrulanamadı` olarak, kanıtıyla yazar. Maske yokken çıktıda ve raporda "su engelli/suya takılmıyor" yargısı **yok**; `WATER_TRUTH none`
- [ ] K6: **Mevcut çıktı değişmez:** `git diff gece/2026-10-02...bot/F5-60 -- tools/nav-measure/nav_measure.cpp | grep -c "^-[^-]"` ≤ 1 (yalnızca kullanım metni satırı değişebilir; mevcut bölüm kodu silinmedi/değişmedi); `./tools/nav-regress.sh --skip-timing` rc'si adım 1'deki taban rc'siyle **aynı** ve çıktısı `regress-before.txt` ile aynı (zamanlama hariç); `water` bölümü `all` listesine eklenmedi (`grep -n '"water"' tools/nav-measure/nav_measure.cpp` yalnızca `if (section == "water")` ve kullanım metni)
- [ ] K7: Karar tablosu (§3) mekanik uygulanmış; sonuç (`GEREKLİ`/`GEREKMEZ`/`BELİRSİZ`) ve gerekçesi (hangi satır) raporun ilk bölümünde; F5-67 için sonuç ("HAZIR yazılır" / "İPTAL" / "TASLAK kalır") açık
- [ ] K8: `git diff --stat gece/2026-10-02...bot/F5-60` yalnızca §4'teki 2 dosya (+ plan dosyası); `BotCore/`, `Tests/`, `GameServer/`, `shared/`, `docs/`, `.vcxproj` farkı 0; `git diff --check` boş; yeni dosya ASCII
- [ ] K9: `./tools/build.sh Release` rc=0 (araçlar derlemeyi etkilememeli) ve `./tools/run-tests.sh` `0 failed`
- [ ] K10 (Claude): bağımsız doğrulama: `water` bölümü yeniden koşulur, havza ve yol sayıları `nav-water-audit.py basins` ile ve (maske varsa) maskenin ızgaraya eşleme kanıtı (`KABUL` ise) ayrıca incelenir; karar `docs/12` §13.1'e işlenir

## 7. Doğrulama komutları

```bash
python3 tools/nav-export.py
python3 tools/nav-water-audit.py --selftest
mkdir -p build/nav-measure
./tools/nav-measure.sh water --n 5000 | tee build/nav-measure/water-audit.txt
python3 tools/nav-water-audit.py basins
python3 tools/nav-water-audit.py probe-client
./tools/nav-regress.sh --skip-timing                 # taban (adım 1) ile aynı rc ve çıktı
./tools/build.sh Release && ./tools/run-tests.sh Release
git diff --stat gece/2026-10-02...bot/F5-60
```

## 8. Kısıtlar ve uyarılar

- `AGENTS.md` §3: kod girintisi tab, Allman, yorumlar İngilizce, yeni dosya ASCII (+ CRLF); `nav_measure.cpp`'nin mevcut kodlaması/satır sonu korunur. Konsol çıktısı araç doğasında (bu bir araç).
- **Yalnızca okuma:** istemci dosyaları (`/mnt/c/dev/fdp/Client/...`) ve SMD dosyaları değiştirilmez; DB'ye bağlanılmaz; sunucu çalıştırılmaz.
- **Dürüstlük:** yükseklik tabanlı "havza" bir **vekildir** (su olduğu kanıtlanmamıştır); çıktı etiketleri `proxy` ve `mask` ayrımını açık tutar. Süpercover ve `Walk` tanımı `NavGrid::Build` ile aynıdır (`tools/nav-segment-check.py` oracle'ı ile tutarlı). Host `g++ -O2` zamanlamaları MSVC Release değildir.
- Bu plan **su katmanı eklemez** ve `NavGrid`/planlayıcı davranışını değiştirmez: yalnızca ölçer ve karar üretir. Kalıcı regresyon (`nav-regress`'e `water` satırı) kararı sonucu görmüş Claude'dadır.
- İstemci veri biçimi ters-mühendislik **zaman kutulu** bir keşiftir; çözülemezse bu **başarısızlık değil**, doğru sonuçtur (§3 üçüncü satır). 2 saati aşan ters-mühendislik yapma; `Durum: UYGULANIYOR (BLOKE)` yazmak da gerekmez: "doğrulanamadı" ile ilerle.

## 9. Proje sahibi testi: T-NAV-09 (bu planın kabulünün **dışında**; insan istemcisi gerekir)

Çevrimdışı denetim istemcinin davranışını kanıtlayamaz. Aşağıdaki test **proje sahibine** aittir, `docs/reports/degerlendirme-takip.md`'de ayrı satır olarak izlenir; sonucu Claude `docs/12` §13.1, `docs/18` Q-26 ve `docs/15` T-NAV-09'a işler ve F5-67 kararını verir. F5-60 `DOĞRULANDI` olması T-NAV-09'un yapıldığı anlamına **gelmez**.

- **Girdi:** `water` bölümünün 5 havza satırı (merkez ve sınır dünya koordinatları) ve iki doğuş → arena rotası (`WATER_ROUTE`); varsa `W` ∩ yol temas örnekleri (`EXAMPLE water path`).
- **Yöntem (insan istemcisi, zone 71):** her havzanın (ve rotadaki temas noktalarının) merkezine ve kıyısına yürü/ışınlan (yöntem proje sahibinin: bu planda önerilen bir GM komutu **uydurulmamıştır**). Her nokta için kaydet: (1) görsel olarak su var mı (kıyı/göl/nehir), (2) istemci yürümene izin veriyor mu (suya girilebiliyor mu), (3) suya girince hız değişiyor mu (yavaşlama: evet/hayır), (4) kıyıda duvara takılma var mı (görünmez engel), (5) sunucu konumu kabul ediyor mu (sunucu günlüğünde `WIZ_MOVE` reddi/geri çekme yok).
- **Çıktı:** nokta tablosu (havza id, koordinat, 1-5 sonuç, not/ekran görüntüsü). Yorum: istemci **suya giremiyorsa ama `Walk` o hücreyi yürünebilir sayıyorsa** → `GEREKLİ` (F5-67); istemci rahatça yürüyorsa → `GEREKMEZ` (ızgara doğru; havzalar vadi); hız değişiyorsa → maliyet katmanı değerlendirilir (F5-67 kapsamı).

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Karar özeti (§3 tablosu, mekanik): **`BELİRSİZ`** — dayanak satırı: **§3 üçüncü satır** ("Zemin gerçeği yok / doğrulanamadı"). Adım 5 `KABUL` ölçütünü sağlayan doğrulanmış bir su maskesi üretilemedi (§2/§5.5: istemci `.gtd` biçimi çözülemedi, hücre-başına doku indeksi ızgarası kanıtlanamadı), bu yüzden `Walk` ∩ su ve yol/kiriş-suması sayıları "su" olarak yorumlanamaz. **"Suya takılmıyor" kabulü verilmedi**; `WATER_TRUTH none`. **F5-67 `TASLAK` kalır** (Claude yazacak). Karar T-NAV-09 insan testine bağlı (§9).
- Branch / commit'ler: `bot/F5-60` — `aad29d4 [F5-60] Nav olcumu: su ve engel verisi denetimi (nav_measure water + nav-water-audit.py)`; bu rapor güncellemesi ek commit. Taban `gece/2026-10-02` (= `78550f0`).
- Değişen dosyalar ve neden:
  - `tools/nav-measure/nav_measure.cpp` — yalnızca yeni `water` bölümü (`Water`, `HeightPct`, `LabelComponents`, `SegmentTouchesSet`), `Ctx`'e `waterT`/`waterMin`/`maskPath` alanları, `main`'e `--water-t`/`--water-min`/`--mask` bayrakları ve `if (section == "water")` dalı, kullanım satırı. `water` `all` listesine **eklenmedi**.
  - `tools/nav-water-audit.py` (yeni) — `basins` bağımsız Python çapraz kontrolü (aynı anahtarlar/sayılar), `probe-client` keşif komutu, `--selftest` (12×12 sentetik: çukur havza, cep, kenar bileşeni).
- `nav-export.py` çıktı satırı:
  `NAVGRID file=…/build/nav/zone71.navgrid bytes=1579030 n=513 unit=4.0 events0=29522 events1=233647 hmin=-30.633 hmax=82.122 main_component=88508 crc32=4fd154bc`
  Ayrıca değişiklik öncesi taban: `./tools/nav-regress.sh --skip-timing` → `rc=0`, `NAV-REGRESS PASS checks=27 pass=27 fail=0 warn=0 info=8 …` (`build/nav-measure/regress-before.txt`).
- `water` bölümü çıktısı (tamamı, `./tools/nav-measure.sh water --n 5000`, `build/nav-measure/water-audit.txt`; commit edilmedi):

```
GRID n=513 unit=4.0 main_cells=88508 seed=20261002 pairs=5000
WATER_GRID n=513 unit=4.0 events0=29522 events1=233647 walk=88508 event1_not_walk=145139 pockets=633 pocket_cells=28501 pocket_cells_adjacent_to_walk=37
WATER_HEIGHT walk_hmin=-7.38 walk_p1=-6.08 walk_p5=-0.56 walk_p50=2.58 walk_p95=15.91 walk_hmax=37.77 walk_below_0=14521 walk_below_m1=3830 walk_below_m2=3227 walk_below_m4=2155 walk_below_m6=934
WATER_BASIN id=0 cells=1225 hmin=-7.07 hmax=-1.01 x=[404.0,720.0] z=[692.0,820.0] edge0_touch=135
WATER_BASIN id=1 cells=219 hmin=-3.43 hmax=-1.00 x=[424.0,512.0] z=[1096.0,1220.0] edge0_touch=75
WATER_BASIN id=2 cells=247 hmin=-5.89 hmax=-1.04 x=[812.0,856.0] z=[884.0,1064.0] edge0_touch=53
WATER_BASIN id=3 cells=228 hmin=-2.45 hmax=-1.01 x=[1164.0,1236.0] z=[940.0,1112.0] edge0_touch=38
WATER_BASIN id=4 cells=1820 hmin=-7.38 hmax=-1.00 x=[1268.0,1636.0] z=[1164.0,1376.0] edge0_touch=167
WATER_BASIN_SUMMARY t=-1.00 min_cells=200 count=5 cells=3739
WATER_SOURCE proxy_basins
WATER_TRUTH none
WATER_SHORE cells=733
EXAMPLE water path (498.0,698.0)->(394.0,958.0) cell (124,179) in W
EXAMPLE water path (758.0,1062.0)->(402.0,506.0) cell (164,189) in W
EXAMPLE water path (346.0,694.0)->(850.0,726.0) cell (121,180) in W
EXAMPLE water path (1034.0,938.0)->(582.0,902.0) cell (212,260) in W
EXAMPLE water path (442.0,1350.0)->(334.0,1094.0) cell (119,305) in W
WATER_PATHS pairs=5000 found=4980 nodelimit=11 nopath=9 non_walk_cells_on_path=0 paths_with_w_cell=1377 raw_cells_in_w=22339 smooth_segments=27786 smooth_segments_touching_w=2605 chords=208214 chords_touching_w=16033
WATER_ROUTE name=karus length_m=271.5 cells=61 cells_in_w=0 segments_touching_w=0
WATER_ROUTE name=elmorad length_m=727.8 cells=161 cells_in_w=0 segments_touching_w=0
```

  **`basins` çapraz kontrolü:** `python3 tools/nav-water-audit.py basins` çıktısı C++ `water` bölümünün `WATER_GRID`/`WATER_HEIGHT`/`WATER_BASIN`/`WATER_BASIN_SUMMARY` satırlarıyla `diff` boş (birebir aynı; bağımsız Python uygulaması). `nav-water-audit.py --selftest` → `SELFTEST OK`, rc=0.
  **§2 ön ölçümüyle karşılaştırma (K4):** birebir uyum — `main=88508`, `events1=233647`, cep 633/28 501 hücre, cep→Walk 8-komşu 37, `Walk` yükseklik min −7,38 / p1 −6,08 / p5 −0,56 / medyan 2,58 / p95 15,91 / maks 37,77, `Walk` < −1,0 m = 3 830, 5 havza / 3 739 hücre (1820/1225/247/228/219; x/z sınırları ve `edge0_touch` 38–167 dahil). Sapma yok.
- Adım 5 (keşif) sonucu: **doğrulanamadı** (maske üretilmedi, hiçbir dosya değiştirilmedi).
  - `probe-client` (`build/nav-measure/probe-client.txt`, 118 satır): `Zones/freezone_a.gtd` içinde `ka_water.dxt` 8 kez (bayt 639397, 641182, 641229, 645618, 650051, 659764, 666397, 670786); `Zones/freezone_a.opd` içinde `object\obj_el_lakefog01.n3pmesh`, `object\obj_el_transmark00_water.n3pmesh`; `Misc/river/` (`el_lake.dxt`, `ka_water.dxt`, `bat_water.dxt`, `dun_pond.dxt`, …) ve `Object/` (`me_water00xx.dxt`, `me_waterdrop/smog…`) ad kalıpları. Yalnızca gerçekler; yorum yok.
  - **Durma noktası / neden güvenilmez:** `freezone_a.gtd` 679 959 bayt. Çözülebildiği kadarıyla: başta `int32 = 10`; `dtex\*.gtt` arazi doku adlarının **260 bayt adımlı 127 kayıtlık tablosu** bayt 602 923–635 943 arasında (`dtex\map_co_nsnow01_5.gtt` … `dtex\map_ka_nground05_7.gtt`); sonrası ikili kayıtlar. `ka_water.dxt` adları bu tabloda değil, gövdede düzensiz aralıklarla (+1785, +47, +4389, …) gömülü. Header'da 512/513 boyutu yok; hücre-başına doku/arazi indeksi ızgarasının (513×513 köşe veya 512×512 karo) varlığı ve bu ızgaraya eşlenmesi **kanıtlanamadı**. Depoda istemci kaynak kodu yok (`N3BASE/` yalnızca `N3ShapeMgr.*`), biçim belgelenmemiş ⇒ güvenilir maske çıkarılamadı. Zayıf kanıtla maske üretilmedi (§8 dürüstlük kuralı).
- Derleme / test sonucu: `./tools/build.sh Release` → `rc=0` (`proj-GameServer.vcxproj -> …/GameServer.exe`); `./tools/run-tests.sh Release` → son satır `264 tests, 0 failed`. Mevcut çıktı değişmezliği: `./tools/nav-regress.sh --skip-timing` `rc=0` ve çıktısı `regress-before.txt` ile `diff` boş.
- Kabul kriterleri öz-değerlendirme: K1 ✔ (`SELFTEST OK`, rc=0); K2 ✔ (`pairs=5000`, `found=4980`, `non_walk_cells_on_path=0`, tüm gerekli satırlar var); K3 ✔ (Python `basins` = C++ `water`, `diff` boş); K4 ✔ (§2 ile birebir, sapma yok); K5 ✔ (`WATER_TRUTH none`, raporda "su engelli/suya takılmıyor" yargısı yok, adım 5 kanıtı ve durma noktası yazılı); K6 ✔ (`nav_measure.cpp` silinen satır `grep -c "^-[^-]"` = 1, yalnızca kullanım metni; regress aynı; `"water"` yalnızca `if (section == "water")`); K7 ✔ (`BELİRSİZ` / §3 üçüncü satır; F5-67 `TASLAK`); K8 ✔ (yalnızca §4'teki 2 dosya + plan; `git diff --check` boş; yeni dosya ASCII); K9 ✔ (build rc=0, `264 tests, 0 failed`); K10 Claude'un.
- Plandan sapmalar ve gerekçeleri:
  - `nav-water-audit.py` satır sonu **LF** (plan §8 "CRLF" diyor) — `.gitattributes` `*.py text eol=lf` zorunlu kılıyor ve depodaki tüm Python araçları LF; kardeş araç konvansiyonu izlendi.
  - `probe-client` biçimi planda serbest bırakılmış ("listeler"); `PROBE file=… offset=… name=…` ve `PROBE_NAME dir=… name=…` satırları üretildi. `WATER_BASIN` havza sırası tarama sırasıdır (id 0..4); §2 listesi boyuta göreydi, sayılar aynı.
- Açık sorular:
  - İstemci `.gtd`/`.opd` biçimi bu zaman kutusunda çözülemedi; `ka_water.dxt`'nin hangi hücrelere denk geldiği belirlenemedi. Proje sahibi isterse istemci kaynağı/araç olmadan bu keşif sürdürülemez; T-NAV-09 (insan istemcisi) §9 kararın tek dayanağıdır.
  - Proxy havzaların (özellikle El Morad doğuşu (635,925) yakınındaki 1225 ve 219 hücrelik havzalar) gerçekten su mu yoksa vadi/eğim çukuru mu olduğu bilinmiyor; T-NAV-09 bunu ayırt eder.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-03

- Karar: **DOĞRULANDI** (gece modu, `AUTO_LOOP=1`; birleştirmeyi döngü betiği yapar, bu oturumda birleştirme/push yok)
- İncelenen: `gece/2026-10-02...bot/F5-60` @ `196e060` (`aad29d4` kod + `196e060` rapor; plan dosyası dışında yalnızca 2 dosya)
- **Karar özeti (K7, §3 mekanik):** `BELİRSİZ`, dayanak **§3 üçüncü satır** (doğrulanmış su zemin gerçeği yok). **"Suya takılmıyor" kabulü verilmez**; `WATER_TRUTH none`. **F5-67 `TASLAK` kalır** (iptal/HAZIR yazılmaz). Karar T-NAV-09 (proje sahibi insan testi, §9) sonucuna bağlıdır; F5-60'ın `DOĞRULANDI` olması T-NAV-09'un yapıldığı anlamına gelmez.
- Kriter sonuçları (hepsi denetçi tarafından yeniden koşuldu; sunucular `[DOWN]`, `AUTO_LOOP=1`):

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ | `python3 tools/nav-water-audit.py --selftest` → `SELFTEST OK`, rc=0 |
| K2 | ✔ | `./tools/nav-measure.sh water --n 5000` rc=0; `WATER_GRID/HEIGHT/BASIN_SUMMARY/SOURCE/TRUTH` var; `WATER_PATHS pairs=5000 found=4980 ... non_walk_cells_on_path=0`; iki `WATER_ROUTE` (karus 271,5 m / 61 hücre, elmorad 727,8 m / 161 hücre, ikisinde `cells_in_w=0 segments_touching_w=0`). Çıktı uygulayıcının raporundaki ile **satır satır aynı** (seed sabit) |
| K3 | ✔ | `grep -E '^WATER_(GRID\|HEIGHT\|BASIN)'` (C++) ile `nav-water-audit.py basins` çıktısı `diff` boş (`BASINS_IDENTICAL`). Python tarafı kendi `walk_mask`/`label_components` kodunu kullanır (`tools/nav-water-audit.py:36-115`), C++'a bağlı değil |
| K4 | ✔ | `main=88508`, `events1=233647`, 633 cep / 28 501 hücre, cep→Walk 37, `walk_below_m1=3830`, 5 havza / 3 739 hücre (1820/1225/247/228/219) = §2 ön ölçümü ile birebir; sapma yok |
| K5 | ✔ | `WATER_TRUTH none`, `WATER_SOURCE proxy_basins`; raporda "su engelli/suya takılmıyor" yargısı yok (yalnızca "kabulü verilmedi" olumsuzlaması); adım 5 = "doğrulanamadı", durma noktası yazılı. Bağımsız teyit (`.gtd` gerçekleri): başta `int32=10`; `dtex\*.gtt` adlarının 127 kayıtlık 260 bayt adımlı tablosu (ilk bayt 602 923, son ad 635 683, tablo sonu 635 943); `ka_water.dxt` 8 kez tablo dışında gövdede; `probe-client` çıktısı 118 satır, uygulayıcınınkiyle aynı |
| K6 | ✔ | `git diff gece/2026-10-02...bot/F5-60 -- nav_measure.cpp \| grep -c "^-[^-]"` = 1 (yalnızca kullanım metni); eklemeler yeni işlevler + `Ctx` alanları + bayrak ayrıştırma + `if (section == "water")` (`nav_measure.cpp:1525`); `grep -n '"water"'` tek eşleşme (`all` listesi değişmedi); `./tools/nav-regress.sh --skip-timing` rc=0, `NAV-REGRESS PASS checks=27 pass=27 fail=0`, çıktısı `regress-before.txt` ile `diff` boş |
| K7 | ✔ | Üçüncü satır mekanik uygulanmış; F5-67 `TASLAK` kalır (yukarıda) |
| K8 | ✔ | `git diff --name-only`: `plans/F5-60-...md`, `tools/nav-measure/nav_measure.cpp`, `tools/nav-water-audit.py`; `BotCore/`, `Tests/`, `GameServer/`, `shared/`, `docs/`, `.vcxproj` farkı 0; `git diff --check` boş (rc=0); `nav-water-audit.py` ASCII (`LC_ALL=C grep -P '[^\x00-\x7F]'` boş). Not: bulgu 1 |
| K9 | ✔ | `./tools/build.sh Release` rc=0 (çıktıda `warning` 0); `./tools/run-tests.sh Release` → `264 tests, 0 failed` |
| K10 (Claude) | ✔ | `water` yeniden koşuldu, `basins` ile eşleşti (K3); maske olmadığı için eşleme kanıtı uygulanmaz; karar `docs/12` §13.1, `docs/18` Q-26, `docs/15` T-NAV-09 ve `degerlendirme-takip.md`'ye işlendi |

- Bulgular (önem sırasıyla; hiçbiri engel değil):
  1. **Not (kodlama):** `tools/nav-measure/nav_measure.cpp:1279` yorumundaki `∩` (U+2229), taban blob'u **ASCII** olan dosyayı UTF-8'e çevirdi (BOM yok; derleme etkilenmedi). Sonraki dokunuşta `and`/`intersect` ile değiştirilsin; ayrı düzeltme turu açılmadı.
  2. **Not (örnekleme):** `Water` içindeki çift karışımı `q % 3` ile 64/150/40 (üçte bir), `Smoothing` ile birebir aynı. Plan §5.2'deki "%40/%30/%20/%10 (tüm harita)" tarifi yaklaşıktı; planın "`Smoothing` ile aynı" ifadesi sağlandı, sonuçların yorumunu etkilemez.
  3. **Not (satır sonu):** `nav-water-audit.py` LF (plan "CRLF" diyordu); `.gitattributes` `*.py text eol=lf` zorunlu kılıyor, uygulayıcının sapma gerekçesi doğru.
  4. **Not (keşif derinliği):** adım 5 sığ ama §3/§8 "doğrulanamadı"yı geçerli sonuç sayıyor ve zayıf kanıtla maske üretilmemesi doğru. Denetçi gözlemi `[A]`: `.gtd` tablo sonrası gövde yalnızca 44 016 bayt (679 959 − 635 943); 512×512 hücre başına en az 1 bayt gerektiren bir ızgaraya yetmez, yani hücre-başına doku/su indeksi büyük olasılıkla bu dosyada değil. Sonraki keşif (gerekirse) başka istemci dosyalarına bakmalı; kaynak kodu olmadan maliyetlidir.
  5. **Ölçüm bulgusu (yorum değil, `[V]` sayılar):** bulunan yolların %27,7'si (1 377/4 980) vekil havzalardan en az bir hücreden geçiyor; düzleştirilmiş segmentlerin %9,4'ü (2 605/27 786), paket kirişlerinin %7,7'si (16 033/208 214) temas ediyor; iki doğuş→arena rotası temas etmiyor. Havza hücrelerinin yalnızca %9-34'ü bir olay-0 hücresine komşu (135/1225, 75/219, 53/247, 38/228, 167/1820): **havzalar gerçekten su ise veri onları engellemiyor**, `docs/12`'deki "kıyılar engelli" ifadesi bu havzalar için desteklenmiyor (`[A]`'ya indirildi). Havzaların su olup olmadığı bilinmiyor; ayırt eden tek ölçüm T-NAV-09.
- Düzeltme talimatı: yok (`DOĞRULANDI`).
