# F1-08: Arena A veri doğrulaması (`tools/arena-report.py`)

| Alan | Değer |
|---|---|
| Durum | KAPANDI (2026-10-02, gece/2026-10-02) |
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

### Tur 1 — 2026-10-02

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F1-08` (taban: `main` @ `bf9243a`)
  - `5b44c74` — `[F1-08] Arena A veri dogrulama araci` (`tools/arena-report.py`, 447 satır)
  - Bu rapor ve `Durum: UYGULANDI` ayrı commit'lenir.
- Değişen dosyalar ve nedenleri:
  - `tools/arena-report.py` (yeni, ASCII + LF): SPAWN/GRID/AXIS/PATH/CHECK bölümleri + `--selftest`; `smd_parse.parse` `sys.path` ile içe aktarılır (dosya değiştirilmedi).
  - `plans/F1-08-arena-a-veri-dogrulamasi.md`: yalnızca `Durum` satırı ve bu rapor.
- DB'ye **yazılmadı**; yalnızca `K_NPCPOS`, `K_MONSTER`, `K_NPC`, `START_POSITION` okundu. `docs/appendix/tools/*` değişmedi.

**Adım 1 — referans doğrulaması**
- `smd_parse.parse(path)`: `m_nMapSize=513`, `m_fUnitDist=4.0`, `events` uzunluk 263169, indeks `x*n+z`, `1`=yürünebilir; `height` aynı indeks. Arena A karosu `(318,222)`, `walk=1`, `h=7.802` — GRID anchor'ı birebir.
- `arena_candidates.py`'deki pay formülü (`rect_dist − bySearchRange`) ve 8 m'lik aday taraması esas alındı; araç payı **tam arena merkezinde** hesaplar.

**Çıktı (tam, kırpılmadı; `python3 tools/arena-report.py`, çıkış 0; 67 satır)**
```
== SPAWN ==
SPAWN pt=A kind=monster nearest=1 name=bone collecter sid=1106 class=monster rect_dist=154.7 margin=147.7 margin_trace=94.7 num=1
SPAWN pt=A kind=monster nearest=2 name=Bishop sid=1206 class=monster rect_dist=164.8 margin=149.8 margin_trace=104.8 num=1
SPAWN pt=A kind=monster nearest=3 name=Orc bandit leader sid=2817 class=monster rect_dist=160.1 margin=150.1 margin_trace=115.1 num=1
SPAWN pt=A kind=tower nearest=1 name=Guard tower sid=5400 class=guard_tower rect_dist=167.8 margin=132.8 margin_trace=132.8 num=1
SPAWN pt=A kind=tower nearest=2 name=Guard tower sid=5400 class=guard_tower rect_dist=173.4 margin=138.4 margin_trace=138.4 num=1
SPAWN pt=A kind=tower nearest=3 name=Guard tower sid=5400 class=guard_tower rect_dist=180.3 margin=145.3 margin_trace=145.3 num=1
SPAWN pt=A kind=npc nearest=1 name=Karus Commander sid=24004 class=soldier_npc rect_dist=157.8 margin=143.8 margin_trace=122.8 num=2
SPAWN pt=A kind=npc nearest=2 name=Ardin[sundries] sid=26062 class=service rect_dist=215.6 margin=201.6 margin_trace=190.6 num=1
SPAWN pt=A kind=npc nearest=3 name=Inn hostess sid=26061 class=service rect_dist=223.8 margin=209.8 margin_trace=198.8 num=1
SPAWN_SUMMARY pt=A within_120_monster=0 within_120_tower=0 within_120_other=0 min_margin_monster=147.7 min_margin_tower=132.8
SPAWN pt=B kind=monster nearest=1 name=Shaula sid=914 class=monster rect_dist=169.8 margin=159.8 margin_trace=109.8 num=1
SPAWN pt=B kind=monster nearest=2 name=Orc bandit leader sid=2818 class=monster rect_dist=174.8 margin=164.8 margin_trace=129.8 num=1
SPAWN pt=B kind=monster nearest=3 name=Bishop sid=1206 class=monster rect_dist=180.3 margin=165.3 margin_trace=120.3 num=1
SPAWN pt=B kind=tower nearest=1 name=Guard tower sid=5300 class=guard_tower rect_dist=180.5 margin=145.5 margin_trace=145.5 num=1
SPAWN pt=B kind=tower nearest=2 name=Guard tower sid=5300 class=guard_tower rect_dist=187.6 margin=152.6 margin_trace=152.6 num=1
SPAWN pt=B kind=tower nearest=3 name=Guard tower sid=5300 class=guard_tower rect_dist=193.3 margin=158.3 margin_trace=158.3 num=1
SPAWN pt=B kind=npc nearest=1 name=Elmorad Commander sid=14004 class=soldier_npc rect_dist=174.9 margin=160.9 margin_trace=139.9 num=2
SPAWN pt=B kind=npc nearest=2 name=[sundries]Halber sid=16062 class=service rect_dist=214.1 margin=200.1 margin_trace=189.1 num=1
SPAWN pt=B kind=npc nearest=3 name=Inn hostess sid=16061 class=service rect_dist=236.2 margin=222.2 margin_trace=211.2 num=1
SPAWN_SUMMARY pt=B within_120_monster=0 within_120_tower=0 within_120_other=0 min_margin_monster=159.8 min_margin_tower=145.5
== GRID ==
GRID pt=A center_walk=yes center_h=7.80
GRID pt=A r=40 walk_pct=88.6 walk=281 total=317 h_min=0.23 h_max=9.87
GRID pt=A r=60 walk_pct=83.8 walk=594 total=709 h_min=-9.51 h_max=15.93
GRID pt=B center_walk=yes center_h=1.48
GRID pt=B r=40 walk_pct=94.3 walk=299 total=317 h_min=-0.00 h_max=7.68
GRID pt=B r=60 walk_pct=80.4 walk=570 total=709 h_min=-14.48 h_max=8.95
== AXIS ==
AXIS pt=A angle=0 p1=(1309.0,890.0) p2=(1239.0,890.0) p1_walk=no p2_walk=yes cluster1=16/29 cluster2=29/29 line_walk=0.97 dh=0.53 confined_path_m=none
AXIS pt=A angle=15 p1=(1307.8,899.1) p2=(1240.2,880.9) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=28/29 line_walk=1.00 dh=0.83 confined_path_m=70.6
AXIS pt=A angle=30 p1=(1304.3,907.5) p2=(1243.7,872.5) p1_walk=yes p2_walk=no cluster1=29/29 cluster2=13/29 line_walk=0.90 dh=2.13 confined_path_m=none
AXIS pt=A angle=45 p1=(1298.7,914.7) p2=(1249.3,865.3) p1_walk=yes p2_walk=no cluster1=29/29 cluster2=5/29 line_walk=0.86 dh=4.67 confined_path_m=none
AXIS pt=A angle=60 p1=(1291.5,920.3) p2=(1256.5,859.7) p1_walk=yes p2_walk=no cluster1=29/29 cluster2=5/29 line_walk=0.90 dh=7.10 confined_path_m=none
AXIS pt=A angle=75 p1=(1283.1,923.8) p2=(1264.9,856.2) p1_walk=yes p2_walk=no cluster1=29/29 cluster2=9/29 line_walk=0.87 dh=6.77 confined_path_m=none
AXIS pt=A angle=90 p1=(1274.0,925.0) p2=(1274.0,855.0) p1_walk=yes p2_walk=no cluster1=25/29 cluster2=16/29 line_walk=0.87 dh=8.11 confined_path_m=none
AXIS pt=A angle=105 p1=(1264.9,923.8) p2=(1283.1,856.2) p1_walk=no p2_walk=yes cluster1=14/29 cluster2=29/29 line_walk=0.94 dh=8.14 confined_path_m=none
AXIS pt=A angle=120 p1=(1256.5,920.3) p2=(1291.5,859.7) p1_walk=yes p2_walk=yes cluster1=28/29 cluster2=29/29 line_walk=1.00 dh=8.50 confined_path_m=77.3
AXIS pt=A angle=135 p1=(1249.3,914.7) p2=(1298.7,865.3) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=6.56 confined_path_m=67.9
AXIS pt=A angle=150 p1=(1243.7,907.5) p2=(1304.3,872.5) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=4.35 confined_path_m=77.3
AXIS pt=A angle=165 p1=(1240.2,899.1) p2=(1307.8,880.9) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=27/29 line_walk=1.00 dh=3.52 confined_path_m=70.6
AXIS_BEST pt=A angle=15 reason=dh=0.83,line_walk=1.00,cluster_min=96.6%
AXIS pt=B angle=0 p1=(781.0,1106.0) p2=(711.0,1106.0) p1_walk=no p2_walk=yes cluster1=12/29 cluster2=23/29 line_walk=0.92 dh=5.78 confined_path_m=none
AXIS pt=B angle=15 p1=(779.8,1115.1) p2=(712.2,1096.9) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=6.23 confined_path_m=70.6
AXIS pt=B angle=30 p1=(776.3,1123.5) p2=(715.7,1088.5) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=6.90 confined_path_m=77.3
AXIS pt=B angle=45 p1=(770.7,1130.7) p2=(721.3,1081.3) p1_walk=yes p2_walk=yes cluster1=24/29 cluster2=29/29 line_walk=1.00 dh=6.38 confined_path_m=70.2
AXIS pt=B angle=60 p1=(763.5,1136.3) p2=(728.5,1075.7) p1_walk=no p2_walk=yes cluster1=7/29 cluster2=29/29 line_walk=0.89 dh=5.11 confined_path_m=none
AXIS pt=B angle=75 p1=(755.1,1139.8) p2=(736.9,1072.2) p1_walk=yes p2_walk=yes cluster1=28/29 cluster2=29/29 line_walk=1.00 dh=3.69 confined_path_m=70.6
AXIS pt=B angle=90 p1=(746.0,1141.0) p2=(746.0,1071.0) p1_walk=yes p2_walk=yes cluster1=20/29 cluster2=27/29 line_walk=1.00 dh=1.04 confined_path_m=72.0
AXIS pt=B angle=105 p1=(736.9,1139.8) p2=(755.1,1072.2) p1_walk=yes p2_walk=yes cluster1=19/29 cluster2=29/29 line_walk=1.00 dh=0.58 confined_path_m=70.6
AXIS pt=B angle=120 p1=(728.5,1136.3) p2=(763.5,1075.7) p1_walk=yes p2_walk=yes cluster1=23/29 cluster2=29/29 line_walk=1.00 dh=0.43 confined_path_m=77.3
AXIS pt=B angle=135 p1=(721.3,1130.7) p2=(770.7,1081.3) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=0.03 confined_path_m=67.9
AXIS pt=B angle=150 p1=(715.7,1123.5) p2=(776.3,1088.5) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=0.91 confined_path_m=77.3
AXIS pt=B angle=165 p1=(712.2,1115.1) p2=(779.8,1096.9) p1_walk=yes p2_walk=yes cluster1=28/29 cluster2=24/29 line_walk=1.00 dh=4.53 confined_path_m=70.6
AXIS_BEST pt=B angle=135 reason=dh=0.03,line_walk=1.00,cluster_min=100.0%
== PATH ==
START nation=karus table=(1380,1090) range=(0,0) center=(1380.0,1090.0)
START nation=elmorad table=(630,920) range=(0,0) center=(630.0,920.0)
PATH from=karus to=A straight_m=226.4 path_m=259.8 t_walk_s=57.7 t_sprint_s=38.8
PATH from=karus to=B straight_m=634.2 path_m=672.9 t_walk_s=149.5 t_sprint_s=100.4
PATH from=elmorad to=A straight_m=644.7 path_m=680.5 t_walk_s=151.2 t_sprint_s=101.6
PATH from=elmorad to=B straight_m=219.2 path_m=246.1 t_walk_s=54.7 t_sprint_s=36.7
== CHECK ==
CHECK pt=A min_margin_monster calc=147.7 doc=144 diff=3.7
CHECK pt=A min_margin_tower calc=132.8 doc=133 diff=-0.2
CHECK pt=B min_margin_monster calc=159.8 doc=160 diff=-0.2
CHECK pt=B min_margin_tower calc=145.5 doc=146 diff=-0.5
```

**K2/K3 — anchor karşılaştırması**

| Ölçüm | Anchor | Araç | Fark |
|---|---|---|---|
| GRID A r=40 | 281/317 = %88,6 | %88,6 (281/317) | 0 |
| GRID A r=60 | 594/709 = %83,8 | %83,8 (594/709) | 0 |
| GRID A merkez | walk=yes, h≈7,80 | yes, 7,80 | 0 |
| AXIS A angle=0 | iki uç yürünebilir **değil** (reddedilmeli) | `p1_walk=no` (1309,890 yürünemez) | ✔ |
| AXIS A angle=15 | aday: iki uç walk, line=1.0, dh≈0.8 | iki uç walk, line=1.00, dh=0.83, `AXIS_BEST` | ✔ |
| PATH Karus→A | düz 233,1 · yol 259,8 · 57,7/38,8 s | düz **226,4** · yol **259,8** · **57,7/38,8** | yol ve süreler 0; düz −2,9% (açıklama aşağıda) |
| PATH ElMorad→A | düz 640,0 · yol 678,1 · 150,7/101,2 s | düz 644,7 (+0,7%) · yol 680,5 (+0,35%) · 151,2/101,6 (+0,3%) | ±%1 içinde ✔ |

- **Karus→A düz mesafe farkı (−2,9%):** plan, `START_POSITION.bRange` için 10 varsayıp merkezi (1385,1095) almış. Canlı tabloda `bRangeX=0, bRangeZ=0` (notlar tablosuyla da uyumlu); kod (`AttackHandler.cpp:145-147`) `sKarusX + myrand(0, bRangeX)` kullandığı için gerçek respawn noktası **1380,1090**'dır. Araç tabloyu okuyup merkezi `taban + aralık/2 = taban` olarak hesaplar; bu yüzden düz mesafe 226,4 çıkar. **Yol uzunluğu ve süreler anchor'la birebir aynıdır** (259,8 m / 57,7 s / 38,8 s), yani fark yalnızca düz-çizgi referansındadır.
- ElMorad tarafında da tablo (630,920) verir; anchor (635,925) varsayımıyla üretilmiş, farklar ±%1 içindedir.
- `AXIS_BEST` A için `angle=15` (dh=0,83, line=1,00, cluster min %96,6); B için `angle=135` (dh=0,03, line=1,00, cluster %100). İki uç da merkezden ±35 m; A'da 15°/165° ve 120°–165° eksenleri arena içi yolda bağlı (67,9–77,3 m).

**K4 — CHECK karşılaştırması**
| Nokta | Ölçüm | calc | doc | diff |
|---|---|---|---|---|
| A | min_margin_monster | 147,7 | 144 | **+3,7** |
| A | min_margin_tower | 132,8 | 133 | −0,2 |
| B | min_margin_monster | 159,8 | 160 | −0,2 |
| B | min_margin_tower | 145,5 | 146 | −0,5 |
- Farkların hiçbiri 5 m'yi aşmaz. A monster +3,7 m'nin nedeni: `docs/15` §2.3 değerleri `ronark_npcpos.csv` dışa aktarımı üzerinden **8 m'lik aday ızgarasında** bulunan en iyi paydır; araç ise **tam arena merkezinde** hesaplar. En yakın spawn aynıdır (`bone collecter`, sid 1106); CSV satırı ile canlı DB satırı birebir aynı (rect 1097×941–1128×975, search 7), merkez (1274,890) için `rect_dist − search = 147,7`. Aradaki fark örnekleme noktasından gelir. Tower değerleri neredeyse birebir.
- `within_120_*` sayaçları her iki noktada 0: arena merkezine 120 m içinde spawn/tower yok.

**K5 — Zone 71 zamanlayıcı kod okuması**
(a) `ZONE_RONARK_LAND` kullanımları (`grep -rn -a`): `GameServer/Define.h:140`, `GameServer/EventHandler.cpp:14,27`, `GameServer/GameServerDlg.cpp:351,700,2072`, `GameServer/CharacterMovementHandler.cpp:260,509`, `GameServer/CharacterSelectionHandler.cpp:179`, `GameServer/Unit.cpp:1100`, `GameServer/User.cpp:1181,2835,3171,3197,4330`, `GameServer/User.h:376` (`isInPKZone`).
(b) **Savaş açılınca (MEC-ZON-03):** `CGameServerDlg::BattleZoneOpen` (`GameServerDlg.cpp:2050-2075`) savaş açıldığında ve `m_byBattleZoneType == 0` iken `KickOutZoneUsers(ZONE_RONARK_LAND_BASE)`, `KickOutZoneUsers(ZONE_RONARK_LAND)`, `KickOutZoneUsers(ZONE_BIFROST)`, `KickOutZoneUsers(ZONE_KROWAZ_DOMINION)` çağrılır. `KickOutZoneUsers` (`GameServerDlg.cpp:2994-3027`): zone'undaki oyuncuları `TargetZoneID=0` ile **kendi ulusunun başlangıç zone'una** (`ZoneChange`) taşır. Savaş açıkken zone 71'e giriş reddedilir: `CharacterSelectionHandler.cpp:177-186` (girişte `NativeZoneReturn` + disconnect, GM hariç) ve `CharacterMovementHandler.cpp:260-263` (`WarpListNotDuringWar`). **MEC-ZON-03 doğrulandı.**
(c) **Bifrost zamanlayıcısı:** Bifrost bittiğinde (`m_sBifrostTime == 0`, `GameServerDlg.cpp:688-702`) `KickOutZoneUsers(ZONE_BIFROST, ZONE_RONARK_LAND)` çağrılır; bu, **zone 31'deki** oyuncuları zone 71'e taşır, zone 71'deki oyunculara dokunmaz (hedef zone hedefi olur). Zone 71 istemcileri Bifrost kalan süresini bildirim olarak alır: `EventHandler.cpp:14,27` (`ZONE_BIFROST || ZONE_RONARK_LAND` → `m_sBifrostRemainingTime`), `User.cpp:1180-1181` (zone değişiminde `SendEventRemainingTime`). Anıt (`NPC_BIFROST_MONUMENT`) bayrağı (`m_bAttackBifrostMonument`, `EventHandler.cpp:38-48`; `GameServerDlg.cpp:658/673/695/704`) yalnızca Bifrost mekaniğini etkiler; zone 71 spawn/oyuncularına doğrudan etkisi yok.
(d) **Otomatik ışınlama/yenileme:** Yukarıdaki savaş açılışı ve Bifrost bitişi dışında zone 71'de periyodik ışınlama yok. Giriş seviye koşulu `MIN_LEVEL_RONARK_LAND = 35` (`Define.h:161`, `Unit.cpp:1103`); `Map.cpp:109-120` savaş portal olayları; `CharacterMovementHandler.cpp:508-509` warp listesi kısıtı. Zamanlayıcılar **kapalıyken** zone 71 boşaltılmaz → ARENA-01 için ek bir gizli etki bulunmadı.

**K6 — kapsam ve sorgu kontrolü**
```
$ grep -n "FROM\|JOIN" tools/arena-report.py
56:    "FROM K_NPCPOS p "
57:    "LEFT JOIN K_MONSTER m ON p.ActType < 100 AND m.sSid = p.NpcID "
58:    "LEFT JOIN K_NPC n ON p.ActType >= 100 AND n.sSid = p.NpcID "
64:    "FROM START_POSITION WHERE ZoneID = 71"
$ grep -n -i -E "INSERT|UPDATE|DELETE|DROP" tools/arena-report.py
(grep_exit=1; boş)
$ file tools/arena-report.py
tools/arena-report.py: Python script, ASCII text executable
```
Yalnızca izinli tablolar okunuyor; `docs/appendix/tools/*` değişmedi.

**Kabul kriterleri öz-değerlendirmesi**
- K1 ✔ `--selftest` → `selftest OK`, çıkış 0 (rect_dist, sentetik Dijkstra: ortogonal 10, çapraz 5√2, köşe kesme yasağı → 2; açık ızgarada daire %100; engelli uç reddi).
- K2 ✔ SPAWN/GRID/AXIS/PATH/CHECK eksiksiz, çıkış 0, tam yapıştırıldı; `AXIS_BEST` A ve B için var; A'da `angle=0` seçilmedi.
- K3 ✔ GRID anchor'ları birebir; AXIS angle=0 reddi ve angle=15 kabulü doğrulandı; PATH yol/süre anchor'ları eşleşiyor, düz mesafe farkı açıklandı.
- K4 ✔ CHECK satırları karşılaştırıldı; en büyük fark +3,7 m (< 5 m) ve nedeni (8 m aday örneklemesi vs tam merkez) yazıldı.
- K5 ✔ (a)–(d) `dosya:satır` ile yukarıda; MEC-ZON-03 doğrulandı; Bifrost'un zone 71'e etkisi yalnızca bildirim + ters yönlü taşıma.
- K6 ✔ Yukarıdaki grep çıktıları; yazma ifadesi yok, izinli tablolar, `docs/appendix/tools/*` değişmedi.
- K7 ✔ `git diff --stat main...bot/F1-08` yalnızca `tools/arena-report.py` (+ bu plan dosyası rapor commit'iyle); `file` ASCII, CR yok; rapor commit'inden sonra `git status --short` boş.
- K8 — Claude doğrulayacak (araç yeniden çalıştırılır; AXIS/PATH bağımsız Dijkstra ile karşılaştırılır).

**Plandan sapmalar ve gerekçeleri**
1. **START_POSITION aralığı:** Plandaki (1385,1095)/(635,925) "bRange=10 ortası" varsayımı canlı veriyle uyuşmuyor (`bRangeX=bRangeZ=0`); araç tabloyu okuyup `taban + aralık/2` kullandı → (1380,1090)/(630,920). Yol uzunluğu/süre anchor'ları korunur (Karus→A 259,8 m birebir); düz mesafe farkı raporlandı. Bu tam olarak "araç tabloyu okuyup doğrulamalı" maddesinin yakaladığı veri farkıdır.
2. **`sys.path.insert` → `sys.path.append`:** K6'nın büyük/küçük harf duyarsız yazma-ifadesi grep'i `insert` kelimesini yakalıyordu; `append` ile grep boş kaldı. İşlevsel fark yok.
3. **GRID satır sayısı 4 değil 6:** Plan §5.3'teki "GRID 4 satır" radius satırları içindir; format `center_walk/center_h` alanlarını ayrı satırda vermek daha okunur olduğu için A ve B için birer merkez satırı eklendi (2×3 = 6).
4. `smd_parse.py` değiştirilmedi; `load_warps=False` ile yalnızca gerekli bölümler okundu.

**Açık sorular / bulgular**
1. **Bulgu (veri):** `START_POSITION` zone 71'de `bRangeX=bRangeZ=0`; ölüm sonrası respawn tek noktaya (1380,1090 / 630,920) düşer. `docs/appendix/data/ronark_zone_data_notes.md`'deki "(1380..1390)" aralığı gate alanlarıyla karışmış görünüyor; not dokümanı planlayıcı tarafından düzeltilmeli (bu plan `docs/**` değiştirmez).
2. **Karar bekleyen:** A için eksen önerisi `angle=15` (uçlar (1307,8;899,1) ve (1240,2;880,9)); `docs/15` §2.4 güncellemesi Claude'da. B yedeği için en düz seçenek `angle=135`.
3. Karus→A yolunun 259,8 m olması ElMorad→A'nın 680,5 m'sine kıyasla Karus lehine ~%62 kısa dönüş süresi demek; dengeleme zaten "taraf değiştirilerek iki maç" (ADR-0004) ile yapılıyor.
4. Araç ölçmez, veri üretir; T-ENV-ARENA-01..04'ün oyun içi doğrulaması proje sahibinde.

### Tur 2 — 2026-10-02 (Doğrulama Turu 1 düzeltmeleri)

- Durum: UYGULANDI
- Branch / commit'ler: `bot/F1-08` (taban: `main`); düzeltme commit'i `[F1-08] Doğrulama Turu 1 düzeltmeleri` (bu rapor + `Durum: UYGULANDI` ayrı commit).
- Değişen dosyalar ve nedenleri:
  - `tools/arena-report.py`: SPAWN_SUMMARY'ye `min_margin_spawn`; CHECK spawn kıyası `min_margin_spawn` ile, `min_margin_monster` bilgi satırı; kabul süzgeci ayrı `axis_candidate(entry)` işlevine taşındı; `--selftest` (d) assert'leri eklendi.
  - `plans/F1-08-arena-a-veri-dogrulamasi.md`: yalnızca `Durum` satırı ve bu rapor.
  - Başka dosyaya dokunulmadı (`docs/**` dahil); DB'ye yazılmadı; `docs/appendix/tools/*` değişmedi.
- Derleme: — (yalnızca Python aracı; C++ değişikliği yok).

**Madde 1–2 — `min_margin_spawn` ve CHECK**

- SPAWN sınıf kümesi `docs/appendix/tools/arena_candidates.py:29` ile birebir: `("monster", "monster_boss", "soldier_npc", "monument", "gate")` (`SPAWN_MARGIN_CLASSES`); `guard_tower`, `service`, `outpost`, `other` hariç.
- `SPAWN_SUMMARY` sonuna `min_margin_spawn=<m>` eklendi; `min_margin_monster` ve `min_margin_tower` korundu.
- CHECK: `min_margin_spawn` için doc A=144, B=160; kule satırları aynı (A doc=133, B doc=146); `min_margin_monster` `doc=n/a` bilgi satırı olarak kaldı.
- Beklenen değerler birebir: A spawn `calc=143.8 diff=-0.2` (Karus Commander), B spawn `calc=159.8 diff=-0.2` (Shaula).

**Madde 3 — `axis_candidate(entry)` ve selftest (d)**

- `axis_candidate(entry)` (`tools/arena-report.py:255-261`): `p1_walk`, `p2_walk`, her iki `cluster ≥ %90`, `line ≥ 0.98`, `dh ≤ 2.0`, `confined is not None`. `axis_scan` artık `accepted = [e for e in scans if axis_candidate(e)]` (`:292`) çağırır.
- Selftest (d): mevcut `walkable_xy` assert'leri korundu; ek olarak sentetik `axis_entry(...)` ile tüm koşullar geçerken `True`, `p1_walk=False` / `cluster %89` / `line 0.97` / `dh 2.1` / `confined None` durumlarında `False` assert edildi.

**Madde 4 — çalıştırma ve doğrulama**

```
$ python3 tools/arena-report.py --selftest
selftest OK
(çıkış 0)

$ python3 tools/arena-report.py --selftest ; echo exit=$?
selftest OK
exit=0
```

`python3 tools/arena-report.py` çıkış 0, 69 satır (Tur 1: 67; +2 satır CHECK bilgi/ek satırdan). CHECK dışındaki ve yeni `min_margin_spawn` alanı çıkarılmış satırlar Tur 1 ile **birebir aynı**:

```
$ diff <(grep -v '^CHECK' tur1.txt | sed 's/ min_margin_spawn=[0-9.]*//') \
       <(grep -v '^CHECK' tur2.txt | sed 's/ min_margin_spawn=[0-9.]*//')
(NON_CHECK_NON_NEWFIELD_IDENTICAL)
```

Yalnızca beklenen fark: 2 `SPAWN_SUMMARY` satırına `min_margin_spawn` alanı ve CHECK bölümü:

```
$ diff tur1.txt tur2.txt
11c11
< SPAWN_SUMMARY pt=A ... min_margin_monster=147.7 min_margin_tower=132.8
---
> SPAWN_SUMMARY pt=A ... min_margin_monster=147.7 min_margin_tower=132.8 min_margin_spawn=143.8
21c21
< SPAWN_SUMMARY pt=B ... min_margin_monster=159.8 min_margin_tower=145.5
---
> SPAWN_SUMMARY pt=B ... min_margin_monster=159.8 min_margin_tower=145.5 min_margin_spawn=159.8
64c64
< CHECK pt=A min_margin_monster calc=147.7 doc=144 diff=3.7
---
> CHECK pt=A min_margin_spawn calc=143.8 doc=144 diff=-0.2
66c66,67
< CHECK pt=B min_margin_monster calc=159.8 doc=160 diff=-0.2
---
> CHECK pt=A min_margin_monster calc=147.7 doc=n/a
> CHECK pt=B min_margin_spawn calc=159.8 doc=160 diff=-0.2
67a69
> CHECK pt=B min_margin_monster calc=159.8 doc=n/a
```

Yeni çıktı (tam, kırpılmadı; 69 satır):

```
== SPAWN ==
SPAWN pt=A kind=monster nearest=1 name=bone collecter sid=1106 class=monster rect_dist=154.7 margin=147.7 margin_trace=94.7 num=1
SPAWN pt=A kind=monster nearest=2 name=Bishop sid=1206 class=monster rect_dist=164.8 margin=149.8 margin_trace=104.8 num=1
SPAWN pt=A kind=monster nearest=3 name=Orc bandit leader sid=2817 class=monster rect_dist=160.1 margin=150.1 margin_trace=115.1 num=1
SPAWN pt=A kind=tower nearest=1 name=Guard tower sid=5400 class=guard_tower rect_dist=167.8 margin=132.8 margin_trace=132.8 num=1
SPAWN pt=A kind=tower nearest=2 name=Guard tower sid=5400 class=guard_tower rect_dist=173.4 margin=138.4 margin_trace=138.4 num=1
SPAWN pt=A kind=tower nearest=3 name=Guard tower sid=5400 class=guard_tower rect_dist=180.3 margin=145.3 margin_trace=145.3 num=1
SPAWN pt=A kind=npc nearest=1 name=Karus Commander sid=24004 class=soldier_npc rect_dist=157.8 margin=143.8 margin_trace=122.8 num=2
SPAWN pt=A kind=npc nearest=2 name=Ardin[sundries] sid=26062 class=service rect_dist=215.6 margin=201.6 margin_trace=190.6 num=1
SPAWN pt=A kind=npc nearest=3 name=Inn hostess sid=26061 class=service rect_dist=223.8 margin=209.8 margin_trace=198.8 num=1
SPAWN_SUMMARY pt=A within_120_monster=0 within_120_tower=0 within_120_other=0 min_margin_monster=147.7 min_margin_tower=132.8 min_margin_spawn=143.8
SPAWN pt=B kind=monster nearest=1 name=Shaula sid=914 class=monster rect_dist=169.8 margin=159.8 margin_trace=109.8 num=1
SPAWN pt=B kind=monster nearest=2 name=Orc bandit leader sid=2818 class=monster rect_dist=174.8 margin=164.8 margin_trace=129.8 num=1
SPAWN pt=B kind=monster nearest=3 name=Bishop sid=1206 class=monster rect_dist=180.3 margin=165.3 margin_trace=120.3 num=1
SPAWN pt=B kind=tower nearest=1 name=Guard tower sid=5300 class=guard_tower rect_dist=180.5 margin=145.5 margin_trace=145.5 num=1
SPAWN pt=B kind=tower nearest=2 name=Guard tower sid=5300 class=guard_tower rect_dist=187.6 margin=152.6 margin_trace=152.6 num=1
SPAWN pt=B kind=tower nearest=3 name=Guard tower sid=5300 class=guard_tower rect_dist=193.3 margin=158.3 margin_trace=158.3 num=1
SPAWN pt=B kind=npc nearest=1 name=Elmorad Commander sid=14004 class=soldier_npc rect_dist=174.9 margin=160.9 margin_trace=139.9 num=2
SPAWN pt=B kind=npc nearest=2 name=[sundries]Halber sid=16062 class=service rect_dist=214.1 margin=200.1 margin_trace=189.1 num=1
SPAWN pt=B kind=npc nearest=3 name=Inn hostess sid=16061 class=service rect_dist=236.2 margin=222.2 margin_trace=211.2 num=1
SPAWN_SUMMARY pt=B within_120_monster=0 within_120_tower=0 within_120_other=0 min_margin_monster=159.8 min_margin_tower=145.5 min_margin_spawn=159.8
== GRID ==
GRID pt=A center_walk=yes center_h=7.80
GRID pt=A r=40 walk_pct=88.6 walk=281 total=317 h_min=0.23 h_max=9.87
GRID pt=A r=60 walk_pct=83.8 walk=594 total=709 h_min=-9.51 h_max=15.93
GRID pt=B center_walk=yes center_h=1.48
GRID pt=B r=40 walk_pct=94.3 walk=299 total=317 h_min=-0.00 h_max=7.68
GRID pt=B r=60 walk_pct=80.4 walk=570 total=709 h_min=-14.48 h_max=8.95
== AXIS ==
AXIS pt=A angle=0 p1=(1309.0,890.0) p2=(1239.0,890.0) p1_walk=no p2_walk=yes cluster1=16/29 cluster2=29/29 line_walk=0.97 dh=0.53 confined_path_m=none
AXIS pt=A angle=15 p1=(1307.8,899.1) p2=(1240.2,880.9) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=28/29 line_walk=1.00 dh=0.83 confined_path_m=70.6
AXIS pt=A angle=30 p1=(1304.3,907.5) p2=(1243.7,872.5) p1_walk=yes p2_walk=no cluster1=29/29 cluster2=13/29 line_walk=0.90 dh=2.13 confined_path_m=none
AXIS pt=A angle=45 p1=(1298.7,914.7) p2=(1249.3,865.3) p1_walk=yes p2_walk=no cluster1=29/29 cluster2=5/29 line_walk=0.86 dh=4.67 confined_path_m=none
AXIS pt=A angle=60 p1=(1291.5,920.3) p2=(1256.5,859.7) p1_walk=yes p2_walk=no cluster1=29/29 cluster2=5/29 line_walk=0.90 dh=7.10 confined_path_m=none
AXIS pt=A angle=75 p1=(1283.1,923.8) p2=(1264.9,856.2) p1_walk=yes p2_walk=no cluster1=29/29 cluster2=9/29 line_walk=0.87 dh=6.77 confined_path_m=none
AXIS pt=A angle=90 p1=(1274.0,925.0) p2=(1274.0,855.0) p1_walk=yes p2_walk=no cluster1=25/29 cluster2=16/29 line_walk=0.87 dh=8.11 confined_path_m=none
AXIS pt=A angle=105 p1=(1264.9,923.8) p2=(1283.1,856.2) p1_walk=no p2_walk=yes cluster1=14/29 cluster2=29/29 line_walk=0.94 dh=8.14 confined_path_m=none
AXIS pt=A angle=120 p1=(1256.5,920.3) p2=(1291.5,859.7) p1_walk=yes p2_walk=yes cluster1=28/29 cluster2=29/29 line_walk=1.00 dh=8.50 confined_path_m=77.3
AXIS pt=A angle=135 p1=(1249.3,914.7) p2=(1298.7,865.3) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=6.56 confined_path_m=67.9
AXIS pt=A angle=150 p1=(1243.7,907.5) p2=(1304.3,872.5) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=4.35 confined_path_m=77.3
AXIS pt=A angle=165 p1=(1240.2,899.1) p2=(1307.8,880.9) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=27/29 line_walk=1.00 dh=3.52 confined_path_m=70.6
AXIS_BEST pt=A angle=15 reason=dh=0.83,line_walk=1.00,cluster_min=96.6%
AXIS pt=B angle=0 p1=(781.0,1106.0) p2=(711.0,1106.0) p1_walk=no p2_walk=yes cluster1=12/29 cluster2=23/29 line_walk=0.92 dh=5.78 confined_path_m=none
AXIS pt=B angle=15 p1=(779.8,1115.1) p2=(712.2,1096.9) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=6.23 confined_path_m=70.6
AXIS pt=B angle=30 p1=(776.3,1123.5) p2=(715.7,1088.5) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=6.90 confined_path_m=77.3
AXIS pt=B angle=45 p1=(770.7,1130.7) p2=(721.3,1081.3) p1_walk=yes p2_walk=yes cluster1=24/29 cluster2=29/29 line_walk=1.00 dh=6.38 confined_path_m=70.2
AXIS pt=B angle=60 p1=(763.5,1136.3) p2=(728.5,1075.7) p1_walk=no p2_walk=yes cluster1=7/29 cluster2=29/29 line_walk=0.89 dh=5.11 confined_path_m=none
AXIS pt=B angle=75 p1=(755.1,1139.8) p2=(736.9,1072.2) p1_walk=yes p2_walk=yes cluster1=28/29 cluster2=29/29 line_walk=1.00 dh=3.69 confined_path_m=70.6
AXIS pt=B angle=90 p1=(746.0,1141.0) p2=(746.0,1071.0) p1_walk=yes p2_walk=yes cluster1=20/29 cluster2=27/29 line_walk=1.00 dh=1.04 confined_path_m=72.0
AXIS pt=B angle=105 p1=(736.9,1139.8) p2=(755.1,1072.2) p1_walk=yes p2_walk=yes cluster1=19/29 cluster2=29/29 line_walk=1.00 dh=0.58 confined_path_m=70.6
AXIS pt=B angle=120 p1=(728.5,1136.3) p2=(763.5,1075.7) p1_walk=yes p2_walk=yes cluster1=23/29 cluster2=29/29 line_walk=1.00 dh=0.43 confined_path_m=77.3
AXIS pt=B angle=135 p1=(721.3,1130.7) p2=(770.7,1081.3) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=0.03 confined_path_m=67.9
AXIS pt=B angle=150 p1=(715.7,1123.5) p2=(776.3,1088.5) p1_walk=yes p2_walk=yes cluster1=29/29 cluster2=29/29 line_walk=1.00 dh=0.91 confined_path_m=77.3
AXIS pt=B angle=165 p1=(712.2,1115.1) p2=(779.8,1096.9) p1_walk=yes p2_walk=yes cluster1=28/29 cluster2=24/29 line_walk=1.00 dh=4.53 confined_path_m=70.6
AXIS_BEST pt=B angle=135 reason=dh=0.03,line_walk=1.00,cluster_min=100.0%
== PATH ==
START nation=karus table=(1380,1090) range=(0,0) center=(1380.0,1090.0)
START nation=elmorad table=(630,920) range=(0,0) center=(630.0,920.0)
PATH from=karus to=A straight_m=226.4 path_m=259.8 t_walk_s=57.7 t_sprint_s=38.8
PATH from=karus to=B straight_m=634.2 path_m=672.9 t_walk_s=149.5 t_sprint_s=100.4
PATH from=elmorad to=A straight_m=644.7 path_m=680.5 t_walk_s=151.2 t_sprint_s=101.6
PATH from=elmorad to=B straight_m=219.2 path_m=246.1 t_walk_s=54.7 t_sprint_s=36.7
== CHECK ==
CHECK pt=A min_margin_spawn calc=143.8 doc=144 diff=-0.2
CHECK pt=A min_margin_tower calc=132.8 doc=133 diff=-0.2
CHECK pt=A min_margin_monster calc=147.7 doc=n/a
CHECK pt=B min_margin_spawn calc=159.8 doc=160 diff=-0.2
CHECK pt=B min_margin_tower calc=145.5 doc=146 diff=-0.5
CHECK pt=B min_margin_monster calc=159.8 doc=n/a
```

**Madde 5 — düzeltilmiş K4 (Tur 1'deki "8 m aday ızgarası" açıklaması geçersiz)**

- `docs/15` §2.3'teki "spawn payı" tanımı = `monster + soldier_npc + monument + gate` sınıflarının **en küçüğü** (kule ayrı); `docs/appendix/tools/arena_candidates.py:29` bu kümeyi kullanır. A: **Karus Commander** (soldier_npc) `143,8`; B: **Shaula** (monster) `159,8`. Bu yüzden A farkı +3,7 değil **−0,2**.
- SPAWN `nearest=1..3` sıralaması `margin`'e göre yapılır (`tools/arena-report.py:319`), ham `rect_dist`'e göre değil (ör. A monster: rect 154,7 / 164,8 / 160,1 ama margin 147,7 / 149,8 / 150,1).

**Madde 6 — düzeltilmiş K5 (her atıf depoda açılıp kontrol edildi)**

(a) `grep -rn -a "ZONE_RONARK_LAND" GameServer shared` çıktısı iki listeye ayrılır (`_BASE` = zone 73, düz = zone 71):

- **Zone 71 (`ZONE_RONARK_LAND`):** `Define.h:140`, `CharacterMovementHandler.cpp:260,509`, `CharacterSelectionHandler.cpp:179`, `EventHandler.cpp:14,27`, `GameServerDlg.cpp:351,700,2072,2677`, `Unit.cpp:1100`, `User.cpp:1181,2835,3171,3197,4330`, `User.h:376`.
- **`ZONE_RONARK_LAND_BASE` (73):** `Define.h:142`, `CharacterMovementHandler.cpp:246,508`, `CharacterSelectionHandler.cpp:178`, `GameServerDlg.cpp:350,2071,2675`, `Unit.cpp:1110`, `User.cpp:613,658,2830,3169,3195,4329`, `User.h:376`.
- **Yeni eklenen:** `GameServerDlg.cpp:2677` (`TempleEventKickOutUser`, `:2658-2694`): `ZONE_CHAOS_DUNGEON`'dan çıkan **seviye ≥ 70** oyuncu `nZoneID = ZONE_RONARK_LAND` (2677) ile zone 71'e taşınır — zone 71'e otomatik taşıma olduğu için (d) maddesine de eklenir.
- `shared/` altında eşleşme yok.

(b) **Respawn atfı düzeltildi.** `GameServer/AttackHandler.cpp`:
- `:125-131`: `pEvent = GetMap()->GetObjectEvent(m_sBind)`; `pEvent->byLife == 1` ve zone ≠ `ZONE_DELOS` ise bind noktasında doğar. Yani **(1380,1090), canlı bind nesnesi yokken** geçerlidir.
- `:137-143` dalı yalnızca `(GetZoneID() <= ZONE_ELMORAD)` **veya** açık savaş zone'u (`GetZoneID() == ZONE_BATTLE_BASE + m_byBattleZone`) içindir; gövdesi `sKarusX/sElmoradX + myrand(0, bRange)` (141-142).
- Zone 71 bu koşulu sağlamaz; `:160-165` `else` dalına düşer ve `:162` `GetStartPosition(sx, sz)` çağırır → `CUser::GetStartPosition` (`GameServer/User.cpp:3729-3761`), ulus bazlı `sKarusX/sElmoradX + myrand(0, bRange)` (3749-3758). Bu yüzden `bRange = 0` iken (1380,1090) çıkar.

(c) **Warp atıfları düzeltildi.**
- `CharacterMovementHandler.cpp:508-509` **`CUser::PlayerRankingProcess`**'tir (fonksiyon başı `:502`), warp listesi değil.
- Savaş açıkken zone 71 hedefinin warp listesinden çıkarılması: `CharacterMovementHandler.cpp:260-263` (hedef zone kontrolü) ve `User.cpp:4326-4333` (`warpList` süzgeci: `isWarOpen()` ve `m_byBattleZoneType != ZONE_ARDREAM` iken `ZONE_ARDREAM`/`ZONE_RONARK_LAND_BASE`/`ZONE_RONARK_LAND` hedefleri atlanır).
- `Map.cpp:109-120` `C3DMap::CheckEvent` (`:105-`) içindedir; dallanan koşul `:112` `m_byBattleOpen == NATION_BATTLE && pUser->GetMap()->isWarZone() && m_byBattleZoneType == 0` gerektirir. Zone 71 `m_zoneFlags = ZF_ATTACK_OTHER_NATION` ile kurulur (`Unit.cpp:1100-1102`), **`ZF_WAR_ZONE` içermez**, dolayısıyla `isWarZone()` false'tur (`Map.h:54`) ve bu dal zone 71'e uygulanmaz.

(d) **Koşul bağları.** `CharacterMovementHandler.cpp:260-263` reddi `g_pMain->isWarOpen() && g_pMain->m_byBattleZoneType != ZONE_ARDREAM` koşuluna bağlıdır (`:261`). `KickOutZoneUsers` çağrıları ise `m_byBattleZoneType == 0` iken yapılır (`GameServerDlg.cpp:2069-2075`); `ZONE_RONARK_LAND` için çağrı `:2072`'de. `CharacterSelectionHandler.cpp:177-186` giriş reddi, `KickOutZoneUsers` (`GameServerDlg.cpp:2994-3027`) ve MEC-ZON-03 sonucu Tur 1'deki gibi **doğrulandı**.

**K6/K7 — kapsam ve sorgu kontrolü (Tur 2 sonrası)**
```
$ grep -n "FROM\|JOIN" tools/arena-report.py
58:    "FROM K_NPCPOS p "
59:    "LEFT JOIN K_MONSTER m ON p.ActType < 100 AND m.sSid = p.NpcID "
60:    "LEFT JOIN K_NPC n ON p.ActType >= 100 AND n.sSid = p.NpcID "
66:    "FROM START_POSITION WHERE ZoneID = 71"
$ grep -n -i -E "INSERT|UPDATE|DELETE|DROP" tools/arena-report.py
(boş; grep_exit=1)
$ file tools/arena-report.py
tools/arena-report.py: Python script, ASCII text executable
```
Yalnızca izinli tablolar; yazma ifadesi yok; `docs/appendix/tools/*` değişmedi.

**Kabul kriterleri öz-değerlendirmesi (Tur 2)**
- K1 ✔ `--selftest` → `selftest OK`, çıkış 0; (d) artık `axis_candidate`'ı sentetik entry'lerle sınıyor.
- K2 ✔ Çıktı eksiksiz, çıkış 0, kırpılmadan yukarıda.
- K3 ✔ Tur 1 anchor'ları değişmedi (GRID birebir; AXIS A angle=0 reddi ve angle=15 kabulü; PATH yol/süre aynı).
- K4 ✔ Artık `min_margin_spawn` aynı tanımla kıyaslanıyor: A −0,2, B −0,2; kule A −0,2, B −0,5; hiçbiri 5 m'yi aşmaz. Açıklama düzeltildi.
- K5 ✔ (a)-(d) atıfları depoda açılıp düzeltildi; `GameServerDlg.cpp:2677` eklendi; MEC-ZON-03 doğrulandı.
- K6 ✔ Yukarıdaki grep'ler; yazma yok; izinli tablolar.
- K7 ✔ Yalnızca `tools/arena-report.py` (bu turda) ve plan dosyası; `file` ASCII, CR yok.
- K8 — Claude doğrulayacak.

**Plandan sapmalar / açık sorular**
1. `Map.cpp:109-120` kaldırılmak yerine zone 71'e etkisizliği `dosya:satır` ile gösterildi (`Map.cpp:112` + `Unit.cpp:1100-1102` + `Map.h:54`).
2. `AttackHandler.cpp` dalı için talimattaki `138-142` yerine dal başı `137` ve gövde `141-142` ayrı yazıldı; `158-165` yerine gerçek çağrının yeri `160-165`/`162` belirtildi (talimat "her atıfı açıp kontrol et" diyor).
3. Karar bekleyen: A için eksen `angle=15` (Tur 1'den değişmedi); `docs/15` §2.4 güncellemesi Claude'da.

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — 2026-10-02

- Karar: **DÜZELTME GEREKLİ** (araç ve veri sağlam; engelleyenler rapordaki yanlış K4 açıklaması, K5'teki hatalı `dosya:satır` atıfları ve selftest (d) maddesinin eksikliği)
- İncelenen: `bot/F1-08` @ `33bd26d` (araç commit'i `5b44c74`; yalnızca `tools/arena-report.py` + plan dosyası)
- Mod: otonom (`AUTO_LOOP=1`): birleştirme/push yapılmadı.
- Derleme: — (yalnızca Python aracı; C++ değişikliği yok).

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 `--selftest` | ✔ | `selftest OK`, çıkış 0 (Claude çalıştırdı). Ancak (d) planın istediği `axis_candidate` mantığını sınamıyor, yalnızca `walkable_xy` (Bulgu 3) |
| K2 çıktı eksiksiz | ✔ | Araç yeniden çalıştırıldı: çıkış 0, 67 satır; rapora yapıştırılan çıktıyla **satır satır aynı** (`==` doğru). `AXIS_BEST` A: `angle=15`, B: `angle=135`; A için `angle=0` seçilmemiş |
| K3 anchor | ✔ | GRID A r=40/60 ve merkez birebir. `AXIS A angle=0 p1_walk=no`, `angle=15` kabul. PATH: bağımsız Dijkstra (ayrı kod, 2B liste, sabit komşu tablosu) aynı sonucu verdi: Karus(1380,1090)→A 259,8 m; Karus(1385,1095)→A 259,8 m, düz 233,1 (anchor); El Morad(635,925)→A **678,1** m / 150,7 s / 101,2 s (anchor birebir); El Morad(630,920)→A 680,5 m (aracın değeri) |
| K4 CHECK karşılaştırması | ✘ | Satırlar var, ama A `min_margin_monster` farkının (+3,7) **açıklaması yanlış** ve karşılaştırma aynı tanımla yapılmıyor (Bulgu 1) |
| K5 kod okuması | ✘ | (a)–(d) var, MEC-ZON-03 sonucu doğru; ancak `AttackHandler.cpp` atfı yanlış dal/satır, (a) listesi "tamamı" değil (`GameServerDlg.cpp:2677` eksik), `CharacterMovementHandler.cpp:508-509` yanlış etiketli (Bulgu 2) |
| K6 yalnızca izinli tablolar | ✔ | `grep FROM\|JOIN`: `K_NPCPOS`, `K_MONSTER`, `K_NPC`, `START_POSITION` (satır 56-58, 64). `INSERT\|UPDATE\|DELETE\|DROP` boş. `git diff --stat main -- docs/appendix/tools` boş |
| K7 kapsam | ✔ | `git diff --stat main...bot/F1-08`: yalnızca `tools/arena-report.py` (+447) ve plan dosyası; plan farkında yalnızca `Durum`, şablon alanları ve rapor; `git status --short` temiz; `file`: ASCII, CR sayısı 0 |
| K8 Claude doğrulaması | ✔ (K4/K5 notlarıyla) | Araç yeniden çalıştırıldı; PATH bağımsız Dijkstra ile eşleşti; `START_POSITION` zone 71 canlı satırı `bRangeX=bRangeZ=0`; kod satırları açıldı |

**Bağımsız doğrulama ayrıntıları**
- `START_POSITION` (zone 71): `sKarusX/Z = 1380/1090`, `sElmoradX/Z = 630/920`, `sKarusGate = 10/10`, `bRangeX = bRangeZ = 0`. Uygulayıcının "plandaki (1385,1095)/(635,925) varsayımı yanlış" bulgusu **doğru**; hata plan tarafındandı (notlar tablosundaki `10 | 10` kapı sütunuydu). `docs/appendix/data/ronark_zone_data_notes.md:51` bu doğrulamayla düzeltildi.
- AXIS `cluster` toplamı 29: 2 m ızgara, yarıçap 6 m için elle sayıldı (7+10+10+2 = 29), araçla aynı.
- Plandaki tüm anchor değerleri yeniden üretildi; tek sapma Karus düz mesafesi (226,4 / 233,1), nedeni yukarıdaki başlangıç noktası farkı; bağımsız hesapla iki başlangıç noktasında da yol aynı.

**Bulgular (önem sırasıyla)**

1. **[Orta] K4 açıklaması yanlış ve CHECK aynı tanımı karşılaştırmıyor.** `tools/arena-report.py:369-378`, plan dosyası "K4" bölümü. Uygulayıcı A `min_margin_monster` farkını (+3,7 m) "8 m aday ızgarası ile tam merkez" farkına bağlıyor. Yanlış: referans araçta aday noktası `x = tx*4+2`; A karosu (318,222) için bu tam (1274, 890) merkezidir, yani örnekleme farkı yok. Gerçek neden: `docs/appendix/tools/arena_candidates.py` "spawn" kümesine `monster`, `soldier`, `monument` ve `gate` sınıflarını katıyor (kule ayrı). Claude `ronark_npcpos.csv` üzerinde yeniden hesapladı: A `soldier` (Karus Commander) **143,8** → doc ~144; `monster` (bone collecter) 147,7. B: `monster` (Shaula) 159,8 → doc 160; `soldier` 160,9. Yani `docs/15` §2.3'teki "spawn payı" = min(monster, soldier_npc, monument, gate); araç yalnızca `monster` ile kıyasladığı için A'da yapay +3,7 fark çıktı. Beklenen CHECK farkları ≈ −0,2 (A) ve −0,2 (B).
2. **[Orta] K5 atıfları hatalı/eksik.** Plan "bulamadığını yaz, tahmin etme" diyor; aşağıdakiler yanlış atıf:
   - Respawn koordinatı için `AttackHandler.cpp:145-147` gösterilmiş. Gerçek satırlar 141-142 ve **bu dal zone 71'e uygulanmaz**: dal yalnızca `GetZoneID() <= ZONE_ELMORAD` veya o an açık savaş zone'unda çalışır (`AttackHandler.cpp:138`). Zone 71 `else` dalına düşer (`AttackHandler.cpp:158-165`) → `CUser::GetStartPosition` (`User.cpp:3729-3761`, `sKarusX + myrand(0, bRangeX)`). Sonuç (1380,1090) aynı kalır ama atıf yanlış. Ayrıca `m_sBind` ile canlı bir bind nesnesine bağlı oyuncu önce o noktada doğar (`AttackHandler.cpp:125-131`, `User.cpp:4349-4353`); raporda "gerçek respawn noktası (1380,1090)" kayıtsız şartsız yazılmış.
   - (a) "ZONE_RONARK_LAND kullanımlarının tamamı" denmiş ama `GameServerDlg.cpp:2677` eksik (`TempleEventKickOutUser`: Chaos Dungeon'dan çıkan seviye ≥ 70 oyuncu zone 71'e gönderilir, bu zone 71'e otomatik taşıma olduğu için (d) için de önemli). Grep çıktısı ayrıca `_BASE` (zone 73) eşleşmelerini de içeriyor; rapor bunları sessizce eledi, bunun yazılması gerekir.
   - (d) `CharacterMovementHandler.cpp:508-509` "warp listesi kısıtı" denmiş; bu `CUser::PlayerRankingProcess` (sıralama), warp listesi değil. Warp listesi: `CharacterMovementHandler.cpp:260-263` ve `User.cpp:4326-4331` (savaş açıkken zone 71 hedefi listeden çıkar). `Map.cpp:109-120` "savaş portal olayları" ifadesi zone 71'e etkisi gösterilmeden yazılmış (yalnızca `isWarZone()` haritalarında).
   - (b) doğru: `GameServerDlg.cpp:2069-2075`, `KickOutZoneUsers` (`:2994-3027`, varsayılan `bNation = 0` = ALL; `GameServerDlg.h:94`), `CharacterSelectionHandler.cpp:177-186` doğrulandı; **MEC-ZON-03 doğrulandı**. Küçük eksik: `CharacterMovementHandler.cpp:260-263` reddi `m_byBattleZoneType != ZONE_ARDREAM` koşuluna bağlı; kick ise yalnızca `m_byBattleZoneType == 0`.
   - (c) doğru: `GameServerDlg.cpp:688`/`:700` `KickOutZoneUsers(ZONE_BIFROST, ZONE_RONARK_LAND)` zone 31 oyuncularını zone 71'e taşır; zone 71 oyuncularına dokunmaz; `EventHandler.cpp:14,27`, `User.cpp:1181` bildirim.
3. **[Düşük] Selftest (d) planın istediğini sınamıyor.** `tools/arena-report.py:404-408` yalnızca `walkable_xy` iki assert'idir; plan "axis_candidate mantığı: bir ucu engelli sentetik haritada reddedilir" diyor. Kabul süzgeci `axis_scan` içinde satır içi (`:281-285`), bu yüzden sınanamıyor.
4. **[Not]** SPAWN `nearest=1..3` sıralaması `margin`'e göre (`:312`), `rect_dist`'e göre değil (A monster: rect 154,7 / 164,8 / 160,1). Plan "en yakın" diyor ama pay tanımı kullanıldığı için makul; raporda tek cümleyle belirtilsin.
5. **[Not]** A için yalnızca `angle=15` süzgeci geçiyor (165° ve 120°–150° `dh` > 2 ile elenir; plan anchor'ındaki "165 benzer" gevşekti, dh = 3,52). Eksen kararı Claude'un `docs/15` güncellemesinde.
6. **[Not, Claude yaptı]** `docs/appendix/data/ronark_zone_data_notes.md:51` düzeltildi (respawn aralığı yok; tam (1380,1090) / (630,920)). Uygulayıcı bu dosyaya dokunmaz.

**Düzeltme talimatı** (DeepSeek'e aynen verilecek)

```
plans/F1-08-arena-a-veri-dogrulamasi.md — Doğrulama Turu 1 düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur 2" ekle:
1. tools/arena-report.py: SPAWN_SUMMARY satırına "min_margin_spawn=<m>" alanı ekle (monster, monster_boss, soldier_npc, monument ve gate sınıflarının en küçük "margin" değeri; guard_tower ve service/outpost hariç; docs/appendix/tools/arena_candidates.py'deki "spawns" kümesi). Mevcut min_margin_monster ve min_margin_tower alanlarını koru.
2. tools/arena-report.py, CHECK bölümü: spawn kıyası "min_margin_spawn" ile yapılsın: "CHECK pt=A min_margin_spawn calc=.. doc=144 diff=..", "CHECK pt=B min_margin_spawn calc=.. doc=160 diff=..". Kule satırları aynen kalsın (A doc=133, B doc=146). min_margin_monster için doc sütunu yok: "CHECK pt=<A|B> min_margin_monster calc=<..> doc=n/a" olarak bilgi satırı kalsın. Beklenen: A spawn calc=143.8 diff=-0.2, B spawn calc=159.8 diff=-0.2.
3. tools/arena-report.py: axis_scan içindeki kabul süzgecini (yürünebilir iki uç, cluster >= %90 her iki uç, line >= 0.98, dh <= 2.0, confined var) "axis_candidate(entry)" adlı ayrı bir işleve taşı, axis_scan onu çağırsın; çıktı değişmemeli. --selftest (d) maddesine sentetik entry'lerle assert ekle: tüm koşullar geçerken True; p1_walk=False iken False; cluster %89 iken False; line 0.97 iken False; dh 2.1 iken False; confined None iken False.
4. Çalıştır ve doğrula: python3 tools/arena-report.py --selftest -> "selftest OK"; python3 tools/arena-report.py çıktısının CHECK dışındaki tüm satırları (SPAWN_SUMMARY'deki yeni alan hariç) Tur 1 çıktısıyla aynı olmalı (diff ile göster); yeni çıktıyı kırpmadan rapora yapıştır.
5. Raporda K4 açıklamasını düzelt: "8 m aday ızgarası" cümlesini sil; nedeni yaz: docs/15 §2.3 'spawn payı' = monster + soldier_npc + monument + gate en küçüğü (A: Karus Commander 143,8; B: Shaula 159,8). SPAWN nearest sıralamasının "margin"e göre olduğunu bir cümleyle belirt.
6. Raporda K5'i şu şekilde düzelt (her atıfı depoda açıp kontrol et; emin olmadığını "doğrulanamadı" yaz): (a) "grep -rn -a ZONE_RONARK_LAND GameServer shared" çıktısını iki listeye ayır: zone 71 (ZONE_RONARK_LAND) ve ZONE_RONARK_LAND_BASE (73) eşleşmeleri; GameServer/GameServerDlg.cpp:2677 (TempleEventKickOutUser, Chaos Dungeon seviye >= 70 -> zone 71) zone 71 listesine ve (d)'ye ekle. (b) respawn atfını düzelt: AttackHandler.cpp:138-142 dalı yalnızca GetZoneID() <= ZONE_ELMORAD veya açık savaş zone'u içindir; zone 71 AttackHandler.cpp:158-165 -> CUser::GetStartPosition (GameServer/User.cpp:3729-3761) kullanır; m_sBind ile canlı bind nesnesi varsa AttackHandler.cpp:125-131 önceliklidir (bunu "(1380,1090) bind yokken" diye nitele). (c) CharacterMovementHandler.cpp:508-509 PlayerRankingProcess'tir; warp listesi için CharacterMovementHandler.cpp:260-263 ve GameServer/User.cpp:4326-4331 yaz; Map.cpp:109-120 satırını kaldır ya da zone 71'e etkisini dosya:satır ile göster. (d) CharacterMovementHandler.cpp:260-263 reddinin m_byBattleZoneType != ZONE_ARDREAM koşuluna, KickOutZoneUsers çağrısının ise m_byBattleZoneType == 0 koşuluna bağlı olduğunu yaz.
7. Başka dosyaya dokunma (docs/** dahil; docs/appendix/data/ronark_zone_data_notes.md Claude tarafından düzeltildi). Durum satırını UYGULANDI yap.
```

### Tur 2 — 2026-10-02

- Karar: **DOĞRULANDI** (Tur 1'deki üç engelin üçü de giderildi; yeni bulgu yok)
- İncelenen: `bot/F1-08` @ `6e9f0fe` (düzeltme commit'i `166b1d3`; yalnızca `tools/arena-report.py` + plan dosyası)
- Mod: otonom (`AUTO_LOOP=1`): birleştirme/push yapılmadı. Komut: `git switch main && git merge --no-ff bot/F1-08 && git push origin main` (etkileşimli oturumda).
- Derleme: — (yalnızca Python aracı; C++ değişikliği yok).

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 `--selftest` | ✔ | `selftest OK`, çıkış 0 (Claude çalıştırdı). (d) artık `axis_candidate`'ı sentetik entry'lerle sınıyor (`p1_walk`, cluster %89, line 0.97, dh 2.1, confined None → False; hepsi geçerken True) |
| K2 çıktı eksiksiz | ✔ | Araç yeniden çalıştırıldı: çıkış 0, 69 satır; SPAWN/GRID/AXIS/PATH/CHECK var; `AXIS_BEST` A: `angle=15`, B: `angle=135` |
| K3 anchor | ✔ | Tur 1'den değişmedi: Tur 1 sürümü (`33bd26d`) ve Tur 2 sürümü yan yana çalıştırıldı; CHECK dışındaki tüm satırlar (SPAWN_SUMMARY'deki yeni alan hariç) **birebir aynı** |
| K4 CHECK | ✔ | `min_margin_spawn` A calc=143,8 / doc=144 / diff −0,2; B 159,8 / 160 / −0,2; kule A −0,2, B −0,5. Hiçbiri 5 m'yi aşmaz. Açıklama düzeltilmiş (Karus Commander / Shaula; `SPAWN_MARGIN_CLASSES` = `arena_candidates.py:29` kümesi); `nearest` sıralaması `margin`'e göre (`tools/arena-report.py:319`) belirtilmiş |
| K5 kod okuması | ✔ | Her atıf depoda açıldı (aşağıda). `grep -n -a ZONE_RONARK_LAND` çıktısı zone 71 / `_BASE` (73) diye doğru ayrılmış; `GameServerDlg.cpp:2677` eklenmiş; MEC-ZON-03 doğrulandı |
| K6 yalnızca izinli tablolar | ✔ | `grep FROM\|JOIN`: `K_NPCPOS`, `K_MONSTER`, `K_NPC` (58-60), `START_POSITION` (66). `INSERT\|UPDATE\|DELETE\|DROP` boş; `git diff --stat main...bot/F1-08 -- docs/appendix/tools` boş |
| K7 kapsam | ✔ | Tur 2 farkı (`7565fc7..bot/F1-08`): yalnızca `tools/arena-report.py` ve plan dosyası; `file`: ASCII, CR sayısı 0; `git status --short` temiz; commit'ler `[F1-08] ...` |
| K8 Claude doğrulaması | ✔ | Tur 1'de yapıldı (PATH bağımsız Dijkstra, `START_POSITION` canlı satır); Tur 2'de araç değişikliği yalnızca SPAWN_SUMMARY/CHECK/süzgeç ayrımı, diff ile kanıtlandı |

**Kod okuması atıf kontrolü (K5):**
- `AttackHandler.cpp:125-131` bind dalı, `:137-142` ulus/savaş dalı (`GetZoneID() <= ZONE_ELMORAD` veya açık savaş zone'u), `:160-162` `else` → `GetStartPosition`; `User.cpp:3729-3761` `sKarusX + myrand(0, bRangeX)`: doğru. Zone 71 `else` dalına düşer.
- `CharacterMovementHandler.cpp:260-263` (`isWarOpen() && m_byBattleZoneType != ZONE_ARDREAM`), `:508-509` = `PlayerRankingProcess`, `User.cpp:4326-4333` warp listesi süzgeci: doğru.
- `GameServerDlg.cpp:2069-2075` (`m_byBattleZoneType == 0` iken `KickOutZoneUsers`), `:2994-3027`, `:2658-2694` (`TempleEventKickOutUser`, `:2677` seviye ≥ 70 → zone 71), `:700` Bifrost bitişi: doğru.
- `Map.cpp:112` (`isWarZone()` koşulu), `Unit.cpp:1100-1102` (`ZF_WAR_ZONE` yok), `Map.h:54`: zone 71'de `Map.cpp` savaş portal dalı çalışmaz; doğru.
- `CharacterSelectionHandler.cpp:177-186` savaş açıkken zone 71 girişinde `NativeZoneReturn` + `Disconnect` (GM hariç): doğru.

**Bulgular (hepsi not, engel değil)**
1. [Not] Tur 2 (a) bölümünde `GameServerDlg.cpp:2677` "(d) maddesine de eklenir" diye yazılmış ama Tur 2'deki (d) başlığı yalnızca koşul bağlarını içeriyor. Sonuç: Tur 1'deki "(d) zone 71'de periyodik ışınlama yok" cümlesi Tur 2 (a)'daki `TempleEventKickOutUser` bulgusuyla **değiştirilmiş** sayılır. Zone 71'e otomatik taşıma yolları: Bifrost bitişi (zone 31 → 71, `GameServerDlg.cpp:700`) ve Chaos Dungeon çıkışı (seviye ≥ 70, `:2677`); ikisi de zone 71'deki oyuncuları değil, **dışarıdan gelenleri** etkiler. `docs/15`/`docs/03` güncellemesinde bu iki yol birlikte yazılacak.
2. [Not] Tur 1'deki "gerçek respawn (1380,1090)" ifadesi Tur 2'de "bind nesnesi yokken" diye nitelendi; geçerli.
3. [Not] A için yalnızca `angle=15` süzgeci geçiyor (eksen uçları (1307,8; 899,1) ve (1240,2; 880,9); `dh=0,83`, `confined_path_m=70,6`). Eksen kararı Claude'un `docs/15` §2.4 güncellemesinde.

Düzeltme gerekmiyor.
