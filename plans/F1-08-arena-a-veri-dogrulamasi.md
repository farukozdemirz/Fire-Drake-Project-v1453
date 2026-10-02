# F1-08: Arena A veri doğrulaması (`tools/arena-report.py`)

| Alan | Değer |
|---|---|
| Durum | HAZIR |
| Faz | F1 — Veri ve mekanik doğrulama (`docs/17` §2) |
| Branch | `bot/F1-08` (taban: `main`) |
| Bağımlı olduğu planlar | F0 (KABUL_EDILDI), F1-02 (ölçülen hız: 4,5 m/s yürüyüş, 6,7 m/s sprint) |
| İlgili gereksinim / kabul | ADR-0004 (K-6), T-ENV-ARENA-01..04 (`docs/15` §2, §4.1; **veri tarafı**), REQ-TST-01, Q-11 |
| Tahmini büyüklük | M (1 yeni araç betiği; DB'ye yazılmaz) |
| Hazırlayan / tarih | Claude / 2026-10-02 |

---

## 1. Amaç

`docs/15` §2'deki arena A (1274, 890) analizini **canlı veritabanı ve SMD haritasından yeniden üretilebilir bir araca** dönüştürmek ve şu ana kadar yapılmamış üç şeyi eklemek:

1. **Başlangıç noktası eksenini bulmak.** `docs/15` §2.4 "başlangıç noktaları arena merkezinden ±35 m, iki takım karşılıklı" diyor; ama merkezden **doğuya** +35 m nokta (1309, 890) **yürünemez** ve güneye −35 m nokta (1274, 855) de yürünemez (Claude'un 2026-10-02'de SMD'den doğruladığı). Araç, merkez etrafında açı taraması yapıp **iki ucu da yürünebilir, aralarındaki hat yürünebilir ve yükseklik farkı küçük** bir ekseni bulacak.
2. **Gerçek yürüme mesafesi ve süresi** (T-ENV-ARENA-04): Karus ve El Morad respawn noktalarından arena merkezine olay ızgarasında en kısa yol (düz çizgi değil) ve ölçülen hızlarla süre.
3. **Zone 71 zamanlayıcı etkisi** (ARENA-01): savaş/Bifrost zamanlayıcılarının zone 71'deki oyunculara etkisini **kod okumasıyla** belgelemek.

Çalışma zamanı gözlemi (30 dk canavar gözlemi, tower saldırı ölçümü) bu planın dışındadır; araç yalnızca veri ve harita üzerinden hesap yapar.

## 2. Bağlam (okunması zorunlu)

- `docs/15` §2 (arena kriterleri, §2.2 Karus kapısı, §2.3 aday tablosu, §2.4 karar), `docs/adr/ADR-0004-test-arenasi.md`, `docs/03` §12 (Ronark kuralları), `docs/12` §7 (arena yarıçapı `P-ARENA-R = 60 m`).
- `docs/appendix/tools/smd_parse.py` (349 satır, **değiştirilmez**, `import` edilir) ve `docs/appendix/tools/arena_candidates.py` (referans: aynı mantığın CSV tabanlı hâli, bunu **yeniden yazma**, yalnızca oku). `docs/appendix/data/ronark_zone_data_notes.md` (K_NPCPOS yükleme kuralı), `docs/appendix/data/ronark_npcpos.csv` (eski dışa aktarım: karşılaştırma için).
- `smd_parse.parse(path)` dönüşü: `res["m_nMapSize"]` (513), `res["m_fUnitDist"]` (4,0 m), `res["events"]` (olay ızgarası, uzunluk 513², indeks `x * n + z`, `1` = yürünebilir, diğerleri engelli veya özel), `res["height"]` (aynı indeks, metre). Harita `FDP_MAP_DIR` (varsayılan `/mnt/c/dev/fdp/server/Map`) altında `freezone_a_20050718.smd`. Koordinatlar dünya metresi (x sağa, z yukarı); karo = `int(x // 4)`, `int(z // 4)`.
- **Veritabanı** (`AGENTS.md` §2.7): yerel DB'ye bağlanmak serbest. Bu plan yalnızca `K_NPCPOS`, `K_NPC`, `K_MONSTER`, `START_POSITION` (ve `ZONE_INFO`) tablolarını **okur**. Başka tablo okuma, DB'ye **yazma**. sqlcmd'de `-W` ile `-y` birlikte kullanılamaz.
  - `K_NPCPOS(ZoneID, NpcID, ActType, RegenType, ..., LeftX, TopZ, RightX, BottomZ, ..., NumNPC, RegTime, ...)`: zone 71 satırları. **Yükleme kuralı** (`AIServer/ServerDlg.cpp`, `LoadSpawnCallback`): `ActType < 100` → `K_MONSTER.sSid = NpcID`; `ActType >= 100` → `K_NPC.sSid = NpcID`. Her ikisinde `bySearchRange`, `byAttackRange`, `byTracingRange`, `byType`, `byGroup`, `strName`, `sLevel` var. Sınıflar: `K_NPC.byType = 62` **guard tower**, `155` Bifrost anıtı, `150` kapı; `K_MONSTER` hepsi canavar; `K_NPC.byType = 0` ulus askeri NPC (spawn dikdörtgeni içinde gezer); `101/102` ileri karakol komutanı; `22`, `31` hizmet NPC'si. NumNPC kopya dikdörtgen içinde rastgele noktada doğar.
  - `START_POSITION` zone 71: Karus (1380, 1090), El Morad (630, 920), aralık `rand(0..bRange)` pozitif yönde (`docs/appendix/data/ronark_zone_data_notes.md`); araç tabloyu okuyup doğrulamalı: Karus respawn merkezi **(1385, 1095)**, El Morad **(635, 925)** olarak kullanılır (`bRange=10`'un ortası).
- Ölçülen hızlar (`docs/03` §13.3): yürüyüş **4,5 m/s**, sprint **6,7 m/s**.
- Sunucu kodu (Zone 71 zamanlayıcı etkisi için okunacak): `GameServer/` içinde `ZONE_RONARK_LAND` (`GameServer/Define.h`), savaş/Bifrost zamanlayıcıları (`m_byBattleOpen`, `BattleZoneOpen`, `KickOutZoneUsers`, Bifrost/`m_sBifrost...` benzeri; `grep -n -a` ile bul; `GameServer/GameServerDlg.cpp`, `GameServer/Zone*.cpp`, `GameServer/MapHandler*`, `GameServer/ZoneChange*`), `docs/03` MEC-ZON-03.

## 3. Kapsam

**Yapılacaklar**

- `tools/arena-report.py` (yeni): bölümler SPAWN, GRID, AXIS, PATH, CHECK; `--selftest`.
- Kod okuması: zone 71 zamanlayıcı etkisi (rapora `dosya:satır`).

**Kapsam dışı (yapılmayacak)**

- `docs/appendix/tools/*`, `docs/**`, `GameServer/`, `AIServer/` değişikliği; DB'ye yazma.
- Navmesh, görüş hattı (LOS) hesabı, A* uygulaması (yalnızca olay ızgarasında en kısa yol).
- Çalışma zamanı gözlemi, oyun içi test.
- Arena B'yi **seçmek** (yalnızca karşılaştırma için aynı hesap).

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `tools/arena-report.py` | yeni | ASCII, LF, yalnızca standart kütüphane (`smd_parse`'ı `sys.path` ile içe aktar: `sys.path.insert(0, os.path.join(ROOT, "docs", "appendix", "tools"))`) |

`tools/*` düzenlemesi opencode'da `ask` ister. Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** raporda soru olarak yaz.

## 5. Uygulama adımları

1. **Önce oku:** `smd_parse.py` (özellikle `parse()` dönüşü), `arena_candidates.py` (spawn/tower mesafe formülü: `rect_dist − bySearchRange`), `docs/15` §2.
2. **`tools/arena-report.py`** — komut satırı `python3 tools/arena-report.py [--sqlcmd PATH] [--server ".\SQLEXPRESS"] [--db FDP_kn_online] [--map-dir DIR]` ve `--selftest`. DB erişimi `tools/stat-model.py`'deki kalıpla; harita `--map-dir` (varsayılan `FDP_MAP_DIR` ortam değişkeni, yoksa `/mnt/c/dev/fdp/server/Map`) altındaki `freezone_a_20050718.smd`. Noktalar: **A (1274, 890)**, **B (746, 1106)**; Karus respawn merkezi (1385, 1095), El Morad (635, 925). Yalnızca ASCII ve deterministik çıktı.
   - **Yardımcılar:** `tile(x, z) = (int(x // 4), int(z // 4))`; `walkable(tx, tz)` = sınır içinde ve `events[tx × n + tz] == 1`; `height(x, z)`; `rect_dist(point, rect)` = noktanın dikdörtgene (LeftX..RightX, TopZ..BottomZ; sıralamadan bağımsız min/max) Öklid mesafesi.
   - **Bölüm SPAWN (T-ENV-ARENA-01/03, veri tarafı):** `K_NPCPOS WHERE ZoneID = 71` satırlarını yukarıdaki yükleme kuralıyla `K_MONSTER`/`K_NPC` ile birleştirip her satır için sınıf (`monster`, `monster_boss` (byType 3 ise), `guard_tower` (K_NPC byType 62), `soldier_npc` (K_NPC byType 0), `service`, `monument`, `gate`, `outpost`), `NumNPC`, `bySearchRange`, `byTracingRange`, `byAttackRange` ve dikdörtgen çıkar. Her nokta (A ve B) için `margin = rect_dist − bySearchRange` (docs/15 §2.3'teki "pay") ve `margin_trace = rect_dist − max(bySearchRange, byTracingRange)` hesapla. Satırlar: `SPAWN pt=<A|B> kind=<monster|tower|npc> nearest=<n> name=<ad> sid=<id> class=<sınıf> rect_dist=<m> margin=<m> margin_trace=<m> num=<NumNPC>` her nokta ve her `kind` için **en yakın 3** satır (monster: `monster` ve `monster_boss`; tower: `guard_tower`; npc: diğer hepsi). Ardından özet: `SPAWN_SUMMARY pt=<A|B> within_120_monster=<n> within_120_tower=<n> within_120_other=<n>` (`margin < 120` olan **satır** sayısı) ve `min_margin_monster`, `min_margin_tower`.
   - **Bölüm GRID (T-ENV-ARENA-02):** A ve B için merkez karosunun `walkable`/`height` değeri; `R = 40` ve `R = 60` m için daire içindeki karoların (karo merkezi yarıçap içinde) yürünebilir yüzdesi ve yükseklik min/maks/aralık: `GRID pt=<..> center_walk=<yes|no> center_h=<..> r=<40|60> walk_pct=<..> walk=<n> total=<n> h_min=<..> h_max=<..>`. **Anchor (Claude'un hesabı, ±2 puan):** A, `R=40`: 281/317 = %88,6; `R=60`: 594/709 = %83,8; A merkezi `walk=yes`, `h ≈ 7,80`.
   - **Bölüm AXIS (başlangıç noktaları):** A merkezi için eksen açısı `0, 15, …, 165` derece; her açıda `d = 35 m` ile iki uç `p1 = merkez + d·(cos a, sin a)`, `p2 = merkez − d·(cos a, sin a)`; her uç için: `walkable`, uç etrafında yarıçap 6 m içinde (2 m ızgarayla örneklenen) yürünebilir nokta oranı (`cluster`: yürünebilir/toplam), iki uç arasındaki hat boyunca 71 eşit aralıklı örneğin yürünebilir oranı (`line_walk`), yükseklik farkı `dh`, ve **arena içinde kalan** (tüm yol karoları merkeze ≤ 60 m) 8 komşuluklu en kısa yolun uzunluğu `confined_path_m` (yoksa `none`). Satır: `AXIS pt=A angle=<a> p1=(<x>,<z>) p2=(<x>,<z>) p1_walk=<yes|no> p2_walk=<yes|no> cluster1=<n>/<m> cluster2=<n>/<m> line_walk=<..> dh=<..> confined_path_m=<..|none>`. Sonra `AXIS_BEST pt=A angle=<a> reason=...`: iki uç yürünebilir **ve** `cluster` ≥ 90% **ve** `line_walk ≥ 0,98` **ve** `dh ≤ 2,0` **ve** `confined_path_m` var olan açılar arasında `dh` en küçük, eşitlikte `line_walk` en büyük. **Anchor (Claude'un hesabı):** `angle=0` (doğu ucu (1309, 890)) ve `angle=90` (güney uç taraması) **geçemez**; `angle=15` iki uç yürünebilir, `line_walk=1.0`, `dh ≈ 0,8` (aday); `angle=165` benzer. Aynı taramayı **B için de** yaz (yedek aday).
   - **Bölüm PATH (T-ENV-ARENA-04):** Karus ve El Morad respawn merkezlerinden A ve B merkezlerine olay ızgarasında **8 komşuluklu Dijkstra**: yürünebilir karolar arasında; çapraz geçiş yalnızca iki ortogonal komşu da yürünebilirse; maliyet ortogonal 1, çapraz √2 (× 4 m); başlangıç ve hedef karosu noktayı içeren karo. Satır: `PATH from=<karus|elmorad> to=<A|B> straight_m=<..> path_m=<..> t_walk_s=<path/4.5> t_sprint_s=<path/6.7>` (yol yoksa `path_m=none`). **Anchor (Claude'un hesabı, ±1%):** Karus→A düz 233,1, yol **259,8** m, yürüyüş **57,7 s**, sprint **38,8 s**; El Morad→A düz 640,0, yol **678,1** m, **150,7 s** / **101,2 s**.
   - **Bölüm CHECK (docs/15 ile karşılaştırma):** `docs/15` §2.3'teki beklenen paylar (A: spawn ~144 m, tower ~133 m; B: ~160 m, ~146 m) ile aracın hesapladığı `min_margin_monster`/`min_margin_tower` satır satır karşılaştırılır: `CHECK pt=A min_margin_monster calc=.. doc=144 diff=..`, `... min_margin_tower calc=.. doc=133 ...`, B için 160/146. Fark > 5 m ise bulgudur (nedenini rapora yaz: veri değişmiş mi, mesafe tanımı mı).
   - **`--selftest`** (DB ve harita gerektirmez, sentetik): (a) `rect_dist` (nokta içeride → 0, yanında, köşede); (b) küçük sentetik ızgarada Dijkstra: engelsiz 10 karo ortogonal yol = 10 karo, çapraz 5×5 = 5√2 karo, köşe kesme yasağı (iki ortogonal komşusundan biri engelliyse çapraz geçilemez); (c) daire yürünebilirlik yüzdesi (tamamen açık ızgara %100); (d) `axis_candidate` mantığı: bir ucu engelli sentetik haritada reddedilir. `selftest OK`, çıkış 0.
3. **Çalıştır:** `python3 tools/arena-report.py` çıktısını **kırpmadan** rapora yapıştır (SPAWN: A ve B için 3 kind × 3 satır + özetler; GRID 4 satır; AXIS 12 açı × 2 nokta = 24 satır + `AXIS_BEST` 2; PATH 4 satır; CHECK 4 satır). Anchor'larla karşılaştırma tablosunu da yaz.
4. **Kod okuması (K5):** Zone 71'in savaş/Bifrost zamanlayıcılarından nasıl etkilendiğini oku ve `dosya:satır` ile raporla: (a) `ZONE_RONARK_LAND` kullanımlarının tamamı (`grep -rn -a "ZONE_RONARK_LAND" GameServer shared`); (b) savaş açılınca (`m_byBattleOpen`) veya kapanınca zone 71'deki oyunculara **ne olduğu** (atılıyor mu, ışınlanıyor mu, hiçbir şey olmuyor mu; yoksa yalnızca belirli zone'lar mı), `docs/03` MEC-ZON-03 iddiasının doğrulanması; (c) Bifrost anıtı (`NPC_BIFROST_MONUMENT`) ve Bifrost zamanlayıcısının zone 71'e etkisi (varsa); (d) zone 71'de otomatik ışınlama/yenileme olayı (örn. `ZONE_RONARK_LAND` kontrolleri `isInPKZone`, `MIN_LEVEL_RONARK_LAND`). Bulamadığını "bulunamadı (`grep` çıktısı ...)" olarak yaz; tahmin etme.
5. Raporu yaz; commit mesajı `[F1-08] ...`; yalnızca `tools/arena-report.py` eklenir.

## 6. Kabul kriterleri

- [ ] K1: `python3 tools/arena-report.py --selftest` → `selftest OK`, çıkış 0.
- [ ] K2: Çıktı SPAWN, GRID, AXIS, PATH, CHECK bölümleriyle eksiksiz, çıkış 0, kırpılmadan rapora yapıştırılmış; `AXIS_BEST` hem A hem B için var (A için `angle=0` seçilmemiş).
- [ ] K3: Anchor karşılaştırması: `GRID`/`PATH` değerleri yukarıdaki anchor'lardan sapıyorsa (±%2 / ±%1) fark ve nedeni raporda; `AXIS` taramasında `angle=0` A için reddedilmiş (`p1_walk=no`) ve `angle=15` kabul edilmiş.
- [ ] K4: `CHECK` satırları `docs/15` §2.3 ile karşılaştırılmış; > 5 m fark varsa açıklanmış.
- [ ] K5: Zone 71 zamanlayıcı kod okuması (a–d) dosya:satır ile raporda; `MEC-ZON-03` doğrulandı/yanlışlandı.
- [ ] K6: Araç yalnızca `K_NPCPOS`, `K_NPC`, `K_MONSTER`, `START_POSITION`, `ZONE_INFO` okuyor (`grep -n "FROM\|JOIN" tools/arena-report.py` çıktısı rapora); `INSERT|UPDATE|DELETE|DROP` yok; `docs/appendix/tools/*` değişmedi.
- [ ] K7: Kapsam: `git diff --stat main...bot/F1-08` yalnızca `tools/arena-report.py` ve plan dosyası; `git status --short` boş; `file` ASCII, CR yok.
- [ ] K8 (Claude doğrular): araç yeniden çalıştırılır; AXIS ve PATH anchor'larla ve ayrı bir Dijkstra ile karşılaştırılır; kod satırları açılır.

## 7. Doğrulama komutları

```bash
python3 tools/arena-report.py --selftest
python3 tools/arena-report.py | head -80
grep -n "FROM\|JOIN" tools/arena-report.py
grep -n -i -E "INSERT|UPDATE|DELETE|DROP" tools/arena-report.py
file tools/arena-report.py
git diff --stat main...bot/F1-08
git status --short
```

## 8. Kısıtlar ve uyarılar

- `smd_parse.py` **değiştirilmez**; içe aktarma başarısızsa (yol/ortam) dur ve raporla.
- Harita ve veritabanı yerelde (`/mnt/c/dev/fdp/server/Map`, `FDP_kn_online`); bunlar yoksa aracın hata mesajıyla çıkması (çıkış 1) kabul edilir.
- Izgara örnekleme: dairenin içinde sayma karo **merkezi** yarıçap içinde ise; `cluster` ve `line_walk` örnekleri için koordinatı karoya çevirmek `int(x // 4)`.
- Dijkstra'da yürünebilirlik **yalnızca `events == 1`** demektir (kapı/warp gibi özel olay değerleri engel sayılır); yüksekliğe bağlı eğim sınırı yok (bu planda yok, `docs/12` §5'te ayrıca).
- Arena kararı (A) değişmez; bu plan yalnızca başlangıç **eksenini** önerir ve verileri üretir. Eksen kararı Claude'un `docs/15` güncellemesiyle verilir.
- Yeni karar gerekirse **durup** raporda sor.

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: —
- Branch / commit'ler: —
- Değişen dosyalar ve neden: —
- Kabul kriterleri öz-değerlendirme: —
- Plandan sapmalar ve gerekçeleri: —
- Açık sorular: —

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

(henüz yok)
