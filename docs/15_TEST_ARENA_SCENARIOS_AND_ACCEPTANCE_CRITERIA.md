# 15 — Test Arenası, Senaryolar ve Kabul Kriterleri

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Bu doküman **test senaryosu kimliklerinin (T-*, EVAL-*) ve test arenası tanımının tek kaynağıdır.** Kabul kriterleri (AC-*) ilgili davranış dokümanlarında tanımlıdır; burada senaryolarla eşlenir. Metrikler [16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md)'da, faz kapıları [17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md)'dedir.

---

## 1. İlkeler

1. **Tek maç kanıt değildir.** Her karşılaştırma tekrar, taraf değiştirme ve eşit ekipmanla yapılır ([16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md) §7).
2. **Kendine karşı oyun yeterli değildir.** Değerlendirme şunları içerir: sabit baseline'lar, farklı taktik profilleri (§5) ve mümkün olduğunda insan oyuncular (§7).
3. **Mekanik doğrulama davranıştan önce gelir.** Davranış testleri, kullandıkları mekaniğin T-MECH testi geçtikten sonra anlamlıdır.
4. **Kurtarma teleportu ölçümü geçersiz kılar** (eval modunda `TEST_TELEPORT` = 0).

## 2. Test arenası

### 2.1 Gereksinim ve araştırma

Gereksinim: Ronark Land içinde, çevresinde canavar veya sonucu etkileyen başka varlık bulunmayan bir alan; Karus kapısı önü aday olarak incelenecek; doğrulanmamış koordinat verilmeyecek.

Yöntem: Yerel harita (`freezone_a_20050718.smd`) ve yerel `K_NPCPOS` verisinden salt okunur analiz `[V]` (`appendix/tools/arena_candidates.py`). Her 8 m'de bir yürünebilir hücre için şunlar hesaplandı:

- Tüm NPC/canavar spawn dikdörtgenlerine (arama menzili eklenmiş) mesafe
- Guard tower'lara (arama menzili eklenmiş) mesafe
- 40 m yarıçap içinde aynı bileşende yürünebilir oran
- Yükseklik farkı
- Engele açıklık
- Respawn noktalarına mesafe

### 2.2 "Karus kapısı önü" değerlendirmesi

| Bulgu | Etiket |
|---|---|
| Karus warp kapısı (1375, 1085); Karus respawn (1380–1390, 1090–1100) | `[V]` |
| Kapının 25–50 m çevresinde 18 + ~10 m'de 4 Karus guard tower'ı; arama menzili 35 m, saldırı 20–30 m; tower'lar oyuncular tarafından saldırılamaz | `[V]` `[D]` |
| Sonuç: kapının **hemen önü** (≤ ~90 m) El Morad botları için tarafsız değildir; tower'lar sonucu belirler | `[I]` (çalışma zamanında T-ENV-ARENA-03 ile doğrulanacak) |

### 2.3 Aday alanlar

| Aday | Merkez (x, z) | Konum | Spawn alanlarına pay* | Tower menziline pay* | R=40'ta yürünebilir | Yükseklik farkı (R=40) | Karus respawn'a | El Morad respawn'a |
|---|---|---|---|---|---|---|---|---|
| **A** | (1274, 890) | Karus kapısının ~215 m güneybatısı ("Karus kapısı açıklığı") | ~144 m | ~133 m | %85 | ~8,8 m | ~233 m | ~640 m |
| **B** | (746, 1106) | El Morad kapısının ~230 m kuzeydoğusu (A'nın yaklaşık ayna eşi) | ~160 m | ~146 m | %93 | ~7,7 m | ~639 m | ~212 m |

\* Pay = spawn dikdörtgeni veya tower kenarından **arama menzili düşüldükten sonra** kalan düz mesafe. Bütün değerler `[V]` veriden hesaplanmış `[I]` aritmetiktir. Koordinatlar oyun içinde henüz doğrulanmadı `[A]` (T-ENV-ARENA-01..04).

Görsel: `appendix/maps/zone71_arena_overlay.png` (açık mavi: ana alan; kırmızı/pembe: spawn alanı ve arama menzili; sarı: tower menzilleri; yeşil daire: aday A; mor: respawn noktaları; 1 piksel = 4 m; x sağa, z yukarı).

Simetrik ve izole bir merkez nokta bulunamadı. İki respawn noktasına eşit uzaklıktaki alanlar merkez çanağın (Bifrost Monument ve yoğun canavar) etrafında kalıyor `[I]`.

### 2.4 Karar: yalnızca arena A (K-6, ADR-0004)

- Kontrollü testler ve değerlendirme maçları **arena A**'da (1274, 890) oynanır. B (746, 1106) yedek aday olarak kayıtta kalır.
- Bilinen sonuç: A, Karus respawn noktasına ~233 m, El Morad'a ~640 m uzaktadır. Ölen Karus üyeleri savaşa daha çabuk döner.
- Dengeleme: her seed iki kez, **taraf değiştirilerek** oynanır. Böylece her bot ve politika iki ulusta da eşit sayıda maç oynar. Ölüm sonrası dönüş (MET-PTY-01, MET-SUM-01) ve kazanma oranı **ulus bazında ayrı** raporlanır. Ulus etkisi istatistik raporunda ayrı bir değişken olarak gösterilir.
- Arena yarıçapı `P-ARENA-R` = 60 m; botlar arena dışına yol planlamaz ([12](12_NAVIGATION_AND_POSITIONING.md) §7).
- Başlangıç noktaları: arena merkezinden ±35 m, iki takım karşılıklı.

### 2.4.1 Çalışma zamanı ölçümleri (2026-10-03, proje sahibi oturumu) `[V]`

Paket izleyicili sunucu (`FDP_PACKET_TRACE`), insan istemcisi, `WIZ_MOVE` kayıtları (`speed` alanı 0,1 m/s birimi; hareket paketi ~1,5 sn'de bir):

| Test | Sonuç |
|---|---|
| T-ENV-ARENA-04 (Karus) | Karus W-G, `/town` → respawn (1369,9; 1090,3) → arena A (1274,5; 892,6): **69,8 sn**, izlenen yol **323 m** (düz mesafe 220 m, dolambaç oranı 1,47), hız 4,54 m/s. Karşılaştırma: F1-08 ızgara A* uzunluğu 259,8 m: insan rotası ~%24 uzun |
| T-ENV-ARENA-04 (El Morad) | El Morad W-G, `/town` → respawn (630,0; 920,0) → arena A (1276,3; 889,0): **165,1 sn (2,75 dk)**, izlenen yol **731 m** (düz 647 m, oran 1,13), hız 4,53 m/s, tek 5 sn duraklama. F1-08 A* uzunluğu 678,1 m: insan rotası ~%8 uzun. Summon/geri dönüş süre hesabı bu değerlere dayanır. Not: T-NAV-10'daki bot beklentisi (Karus ~52 sn, El Morad ~142 sn) A* yolu ÷ 4,5 m/s'dir; insan rotaları daha uzundur (yukarıdaki satırlar) |
| T-ENV-ARENA-03 | El Morad W-G (5650 HP) Karus kapısının (1375, 1085) güneybatısından yaklaştı: (1307,1; 1072,3) en yakın kuleye (1335, 1061) **30,1 m**, (1314,0; 1073,0) 24,2 m'de hâlâ yürüyordu; **26,0 m'de durduğunda öldü** (`DeathUser`: `Killer=Guard tower`, SID 5400, X=1311 Z=1072). Beklenen 20–30 m saldırı aralığıyla uyumlu, kule birkaç saniyede 5650 HP'li savaşçıyı öldürür: ≥ 35 m yasak halka (F5-06 `forbiddenPenalty`) zorunlu |
| T-ENV-ARENA-01 (otomatik) | Arena A'da `BotWP_K`'nın NPC tablosu 60 sn aralıkla `npcs` ile taranır (`Scripts/arena-npcs.txt`, 10 dk × 3 tur): üç turun **33 taramasının hepsinde 0 NPC/canavar** (~30 dk, `Logs/Bot_3_10_2026.log`): GEÇTİ. Not: tarama botun görüş alanını (bölge tabanlı, ~144 m) kapsar; insan gözlemi isteğe bağlı |

### 2.5 Test modu ve izolasyon

| Kimlik | Önlem | Tür |
|---|---|---|
| ARENA-01 | Savaş zamanlayıcıları kapalı (savaşta zone 71 boşaltılır, MEC-ZON-03); Bifrost zamanlayıcısının zone 71'e etkisi kontrol edilir `[A]` | Yapılandırma |
| ARENA-02 | Test sırasında zone 71'de bot ve onaylı test katılımcıları dışında oyuncu yok (özel sunucu) | Operasyon |
| ARENA-03 | Arena içinde ve 120 m çevresinde canavar yok: T-ENV-ARENA-01 ile 30 dk gözlem. Gezinen canavar görülürse test modu "NPC bastırma" seçeneği: AIServer, arena merkezine yakın spawn satırlarını test modunda yüklemez `[Ö]` (ADR-0004) | Doğrulama / öneri |
| ARENA-04 | ~~Nötr respawn test modu~~ — K-6 ile seçilmedi; respawn noktaları değiştirilmez | Kullanılmıyor |
| ARENA-05 | Test kurtarma teleportu yalnızca `train`/`debug` modunda | Kural |
| ARENA-06 | ~~Bot–bot ölüm duyurularını kısma~~ — K-9 ile seçilmedi; duyurular normal çalışır | Kullanılmıyor |

## 3. Test seviyeleri

| Seviye | Kapsam | Ortam |
|---|---|---|
| L-U | `BotCore` birim testleri (utility, FSM, A*, fairness kuralları, istatistik) | Sunucusuz |
| L-M | Mekanik doğrulama (tekil skill, pot, buff çakışması, istemci zamanlaması) | Sunucu + bot/insan |
| L-1 | Sınıf/rol bazlı 1v1 | Arena |
| L-S | Küçük takım (2v2–5v5) | Arena |
| L-8 | Dengeli 8v8 | Arena A |
| L-P | Performans ve dayanıklılık | Arena + yük |
| L-H | İnsan değerlendirmesi | Arena |

## 4. Senaryo kataloğu

Her senaryo: kurulum, adımlar, ölçülen metrikler, tekrar sayısı ve ilgili AC. Tekrar sayısı aksi belirtilmedikçe: L-M 10, L-1 50, L-S 30, L-8 20 (iki tarafta eşit dağıtılmış).

### 4.1 Ortam ve veri

| Kimlik | Senaryo | Ölçüm / Geçer |
|---|---|---|
| T-ENV-01 | Temiz kurulum: derleme, DB geri yükleme, ini, sunucuların başlaması | Üç sunucu ayakta; bir insan istemcisi Ronark'a girebiliyor — *kısmen:* derleme/DB/ini/ODBC `tools/check-env.sh` ile 22/22 (F0-01); **sunucuların başlaması** `tools/run-servers.sh start` ile kanıtlandı (F0-02 Doğrulama Tur 2, 2026-10-01: üç sunucu ayakta, GameServer↔AIServer bağlı, her biri 1–5 sn'de hazır, `start` ~13–17 sn) `[V]`; **istemciyle Ronark Land'e giriş yapıldı, sorun yok** (proje sahibi, 2026-10-02; ekran görüntüsü/saat notu yok, beyan) `[Ö]` → T-ENV-01 **tamam** |
| T-ENV-02 | Release ve Debug derleme farkları (blink, quest kapısı, zaman aşımı) belgelenir | Fark tablosu — **TEST_EDILDI/GEÇTİ 2026-10-01**, kanıt: `plans/F0-01-…md`; tablo `docs/02` §2.1 |
| T-ENV-ARENA-01 | Arena A'da (ve yedek B'de) 30 dk canavar/NPC gözlemi | Arena + 120 m içinde varlık 0 |
| T-ENV-ARENA-02 | Arena koordinatlarının oyun içinde doğrulanması (GM ile yürüme, yükseklik, engeller) | Koordinat ve görüntü kaydı |
| T-ENV-ARENA-03 | Karus kapısı önünde El Morad karakterine tower saldırısı | Saldırı mesafesi ölçümü |
| T-ENV-ARENA-04 | Respawn noktaları ve arena arası yürüme süresi (iki ulus) | Süre kaydı (T-NAV-05 ile) |
| T-DATA-01..05 | [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) §7 | |
| T-DATA-06 | MAGIC.Etc düzeltmesinin kalıcı kaydı (KI-001, ADR-0003) | Script + doğrulama sorgusu — **TEST_EDILDI/GEÇTİ 2026-10-02**, kanıt: `plans/F1-03-magic-etc-sql-betigi.md` Doğrulama Tur 1 (geçici kopyada: 1306 satır düzeltildi, 510–523 korundu, tekrar uygulama ve geri alma doğru) |

### 4.2 Mekanik (L-M)

| Kimlik | Senaryo | Geçer |
|---|---|---|
| T-MECH-CLIENT-01 | Gerçek 1453 istemcisinde warrior R ve Type1 skill zamanlaması paket kaydıyla (sunucu tarafı log) | CLI-01/02 değerleri tablolaştırıldı |
| T-MECH-CLIENT-02 | Paket alan değerleri (delaytime, distance ölçeği, echo, hız alanı) | [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §14 güncellendi |
| T-MECH-CLIENT-03 | Cast süresi, hareketle iptal, R ile kesilme (istemci) | CLI-03 güncellendi |
| T-MECH-CLIENT-04 | `WIZ_MOVE` ve `WIZ_SPEEDHACK_CHECK` sıklığı | [12](12_NAVIGATION_AND_POSITIONING.md) §6 güncellendi |
| T-MECH-SKILL-W/P/M-* | Her çekirdek skill: MP, recast, menzil, etki, fail sebebi | [05](05_SKILL_CATALOG_AND_COMBAT_RULES.md) tablosuyla uyum |
| T-MECH-BUF-01..08 | BuffType çakışmaları, debuff'ın buff'ı silmesi, yüzde HP buff'ı, Confusion | 05 §4 doğrulandı/düzeltildi |
| T-MECH-DMG-01..03 | Warrior hasar dağılımı, Malice tek/çift uygulama, büyü CHA ölçeği | Model ± %15 içinde veya model güncellendi |
| T-MECH-T8-01..02 | summon friend koşulları (ölü, No-Recall, mesafe, zone); Gate Ronark'ta | MEC-T8 doğrulandı |
| T-MECH-12 | Direnilen yavaşlatmanın hız buff'ını engellemesi (MB-09) | Sonuç kaydı |
| T-POT-01..03, T-MECH-POT-03/04/05 | [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md) §8 | |

### 4.2.1 İnsan zamanlama oturumu protokolü (T-MECH-CLIENT-01..04)

Amaç: gerçek 1453 istemcisinin zamanlamasını sunucu tarafında kaydetmek ([03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §13'teki CLI-01..06, CLI-11, CLI-12 ve Q-01, Q-02, Q-18 için). Kaydı **insan oyuncu** yapar (proje sahibi); araçlar F1-01 (paket izleyici) ve F1-02 (`tools/trace-session.sh`, `tools/packet-trace-summary.py --cli`). Ölçüm yerel ağda sunucuya **varış** zamanıdır (istemcinin gönderme zamanına çok yakın, yerel bağlantıda gecikme ihmal edilebilir `[Ö]`).

Ön koşullar: F1-02 `DOĞRULANDI` ve `main`'de; bir hedef canavar (çok dayanıklı veya sürekli yeniden doğan; arena A'da veya yakınında); level 80'e yakın warrior, priest ve mage karakterleri (karakter kurulum betiği ADR-0002 taslağı henüz yoksa mevcut karakterler yeterli; ayrıntı notu rapora yazılır).

Komutlar (WSL'de, depo kökünde):

```
tools/trace-session.sh prepare          # sunucuyu izleyicili derler ve açar (oyunda kimse yokken)
# ... istemciyle girin, bir senaryoyu oynayın ...
tools/trace-session.sh collect <etiket> # o senaryonun kaydını ayırır ve özet basar
tools/trace-session.sh finish           # (oyundan çıktıktan sonra) sunucuyu kapatır, normal derlemeye döner
```

Her senaryo arasında `collect` çalıştırın: etiketler sınıf-senaryo biçiminde (ör. `war-r`). Her senaryo **tek bir hedefe**, kesintisiz oynanır; senaryo başlamadan önce 3 sn bekleyin, bitince 3 sn bekleyip `collect` yapın.

| Etiket | Ne yapılır | Ölçülen |
|---|---|---|
| `war-r` | Warrior: hedefe yalnızca normal saldırı (R), 30 sn, sürekli | CLI-01 (R aralığı, `delaytime`/`distance` alanları, T-MECH-CLIENT-02) |
| `war-skill` | Warrior: tek bir Type1 skill'i bekleme süresi dolar dolmaz arka arda, 30 sn | CLI-04 (skill tekrar aralığı) |
| `war-combo` | Warrior: skill + R'yi her zamanki oynayış gibi, 60 sn | CLI-02 (skill-R boşlukları), CLI-11 (aksiyon hızı) |
| `war-move` | Warrior: düz koşu 30 sn, sonra 5 kez dur-kalk | CLI-05, Q-02 (hareket sıklığı, hız alanı, koşu hızı) |
| `idle` | Herhangi bir sınıf: hiçbir şey yapmadan 5 dk bekleyin (hedef seçmeyin) | CLI-12 (`WIZ_SPEEDHACK_CHECK` sıklığı) |
| `target` | Hedef seçip bırakın (20 kez), sonra bir hedefe 20 sn saldırın | Q-18 (hedef HP isteği sıklığı) |
| `pri-cast` | Priest: bir cast süreli skill'i (ör. heal) her seferinde tamamlanmasını bekleyerek 20 kez | CLI-03 (CASTING → EFFECTING süresi) |
| `pri-cancel` | Priest: aynı skill'i başlatıp cast sırasında 10 kez yürüyerek iptal edin | CLI-03 (iptal: `opcode 6` sayısı ve zamanlama) |
| `mag-cast` | Mage: bir cast süreli saldırı skill'i, her seferinde tamamlanmasını bekleyerek 20 kez | CLI-03 |
| `mag-cancel` | Mage: aynı skill'i cast sırasında 10 kez yürüyerek iptal edin | CLI-03 |
| `pot` | Herhangi bir sınıf: HP potunu 20 sn en hızlı basışla, sonra MP potunu 20 sn | CLI-06 (pot aralığı) |

Kurallar:
- Oynayış **normal insan hızında** olmalı; amaç sınırı değil, insanın gerçek tempolarını ölçmek (CLI-01'de R'yi "en hızlı basabildiğiniz" tempoda ayrıca bir `war-r-fast` kaydı da alabilirsiniz).
- Her senaryoda oyuncu adı log'a yazılır. `plans/_logs/trace/` içindeki dosyaları **paylaşmayın ve git'e eklemeyin**; yalnızca `<etiket>.summary.txt` özetleri (adsız sayılar) rapora girer.
- `collect` "yeni kayit yok" derse senaryoda izlenen opcode'lar oynanmamış demektir; senaryoyu tekrarlayın.
- Oturum bitince oyundan çıkın, sonra `finish` çalıştırın (istemci bağlıyken `stop` reddeder).

Çıktı: Claude, `<etiket>.summary.txt` sayılarını `docs/03` §13 (CLI-01..06, CLI-11, CLI-12 ölçülmüş değerleri), §14 (paket alanı doğrulamaları) ve [12](12_NAVIGATION_AND_POSITIONING.md) §6'ya işler; etiketler `[A]` → `[V]` yükseltilir ([21](21_PROJECT_TRACKING_TEMPLATES_AND_DOC_RULES.md) §5).

### 4.3 Navigasyon

T-NAV-01..08, T-NAV-LOS-01: [12](12_NAVIGATION_AND_POSITIONING.md) §11.

Değerlendirme ekleri (2026-10-02, `docs/12` §13): **T-NAV-09** su ve göl kıyısı (insan istemcisi suya girebiliyor mu; olay ızgarasıyla uyum; Q-26); **T-NAV-10** ölüm → respawn → arenaya dönüş zinciri, **Karus ve El Morad ayrı ayrı** oyun içinde (her ulus ≥ 10 tekrar; Karus ~52 sn, El Morad ~142 sn ± %20; arena sınırı içinde kalma; takılma/`NodeLimit`/`InvalidGoal` 0); **T-NAV-11** 16 botta oyun içi tick ve yol bulma bütçesi (gerçek `BotManager` tick'i, MSVC Release, 30 dk; ertelenen sorguda bekleme/takip/mevcut yolu kullanma davranışı). Üçü de birim testi veya doküman güncellemesiyle kapanmaz: oyun içi kanıt `docs/reports/degerlendirme-takip.md`'de ayrı izlenir. **T-NAV-12** (proje sahibi bilgisi, 2026-10-03, `docs/12` §1 "bowl") bowl turu: harita merkezindeki canavar yoğun halkada (≈ (1024, 1024), r ~150 m) 10 dk dolaşma; canavar saldırısı/dk, tehlike katmanı (`danger_static`) ile kaçınma, takılma/bot-saat ve engel yoğun noktalarda kurtarma (F5-54/F5-57); sonuç F11 (çatışma arama) planlamasının girdisidir.

### 4.4 Sınıf ve rol

| Kimlik | Kaynak |
|---|---|
| T-WAR-01..07 | [06](06_WARRIOR_BEHAVIOR.md) §10 |
| T-PRI-01..08 | [07](07_PRIEST_BEHAVIOR.md) §16 |
| T-MAG-01..06 | [08](08_MAGE_BEHAVIOR.md) §12 |
| T-SUR-01..04 | [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md) §8 |
| T-SOLO-01..06 | [10](10_SOLO_PK_BEHAVIOR.md) §8 |

### 4.5 Takım

T-PTY-01..09: [09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) §14.

### 4.6 Değerlendirme maçları (kilitli set `evalset-v1`)

| Kimlik | Kompozisyon | Rakip | Birincil metrik |
|---|---|---|---|
| EVAL-1v1-<A>-<B> | 6 profilin tüm ikilileri (36) | Aynı politika veya baseline | MET-OUT-01 |
| EVAL-2v2 | C2 vs C2 | baseline-v1, OP-AGGRO, OP-KITE | MET-OUT-01, MET-TGT-03 |
| EVAL-3v3 | C3 vs C3 | aynı | aynı |
| EVAL-4v4 | C4 vs C4 | aynı | aynı |
| EVAL-5v5 | C5 vs C5 | aynı | aynı |
| EVAL-8v8-A | C8-A vs C8-A | baseline-v1 ve taktik profilleri | MET-OUT-01, MET-OUT-02 |
| EVAL-8v8-MIX | C8-B vs C8-C | aynı | Kompozisyon genellemesi |
| EVAL-5v8 | 5 vs 8 | OP-AGGRO | Sayısal dezavantajda geri çekilme ve kayıp oranı |
| EVAL-HEALSTALL | 8v8, rakipte iki priest + OP-TURTLE | — | AC-PTY-03 |
| EVAL-WIPE | 8v8, kendi takımın tam yenilgisi tetiklenir | — | AC-PTY-05, MET-PTY-01 |
| EVAL-THIRD | 8v8 + 4 kişilik ikinci düşman grubu 90. sn'de | — | T-PTY-08 |

### 4.7 Performans ve dayanıklılık

| Kimlik | Senaryo | Geçer |
|---|---|---|
| T-PERF-01 | 16 bot, 30 dk | MET-PERF-01/02 bütçe içinde |
| T-PERF-02 | 32 bot, 30 dk | Raporlanır |
| T-PERF-03 | 64 bot, 30 dk | MET-PERF-02 ≤ 15 ms |
| T-PERF-04 | 64 bot, 4 saat dayanıklılık | AC-ARCH-03 |
| T-PERF-05 | Bot sistemi kapalı, regresyon | AC-ARCH-02 |
| T-PERF-06 | Spawn/despawn döngüsü (1000 kez) | Çökme 0, slot sızıntısı 0 (R-CODE-01) |

### 4.8 Kullanıcı test listesiyle eşleme

| Kullanıcının istediği test | Senaryo |
|---|---|
| Tekil skill ve potion doğrulaması | T-MECH-SKILL-*, T-POT-*, T-MECH-POT-* |
| Sınıf ve rol bazlı 1 vs 1 | EVAL-1v1-*, T-SOLO-01 |
| Küçük takım savaşları | EVAL-2v2 … EVAL-5v5 |
| Dengeli 8 vs 8 | EVAL-8v8-A, EVAL-8v8-MIX |
| Sürekli heal alan ortak hedef | T-PTY-03, EVAL-HEALSTALL |
| Debuff ve cure etkileşimi | T-PRI-05, T-PRI-06, T-MECH-BUF-* |
| Priest'e ani baskı | T-PRI-07 |
| Düşük HP'de geri çekilme | T-SUR-01..04 |
| MP azlığı ve potion çakışmaları | T-POT-02, T-POT-03, AC-WAR-05 |
| Ulaşılamayan veya görüş hattı dışındaki hedef | T-NAV-07, T-NAV-LOS-01 |
| Navigasyonda sıkışma | T-NAV-04 |
| Üye ölümü, respawn, summon ve tekrar katılım | T-MAG-05, T-MAG-06, EVAL-WIPE |
| Lider ölümü | T-PTY-05 |
| Party'nin dağılması ve toparlanması | T-PTY-06, T-PTY-07 |
| Uzun süreli dayanıklılık ve çok botlu performans | T-PERF-01..06 |

### 4.9 Oyun içi kabul testleri (G-IGT, değerlendirme 2026-10-02)

Derleme ve birim testi bir davranışın oyunda çalıştığını göstermez. Aşağıdaki testler ilgili faz alt kapısının (`docs/17` §5) **zorunlu** kanıtıdır; kanıt türü = telemetri raporu (`tools/bot-telemetry-report.py`) + insan gözlemi/ekran kaydı.

| Kimlik | Davranış | Senaryo (mevcut kimlikler) | Geçer ölçütü | Kanıt |
|---|---|---|---|---|
| T-IGT-WAR-01 | Warrior hedefe ulaşır ve sürdürülebilir baskı kurar | T-WAR-01..03 (hareketli hedef, 60 sn × 10 tekrar, solo) | MET-TGT-01 p50 ≤ 4 sn; MET-TGT-02 ≥ %70; MET-ACT-02 ≤ %1; MET-NAV-01 ≤ 2/bot-saat | telemetri + insan gözlemi |
| T-IGT-PRI-01 | Priest heal, buff, cure, debuff önceliklerini doğru yönetir; iki priest çift heal yapmaz | T-PRI-01..07 birleşik (sabit hasar alan warrior, kök/Malice'li mage, düşman) | AC-PRI-01..06, AC-PRI-09; MET-HEAL-02/04/05, MET-BUFF-01/03, MET-CURE-01/02 | karar logu (`override=true` örnekleri) + insan |
| T-IGT-MAG-01 | Mage, respawn olmuş **yaşayan** takım arkadaşını güvenle çeker (güvensiz summon yok) | T-MAG-05/06, EVAL-WIPE kısmı (20 tekrar) | MET-SUM-02 ≤ %10; SUM-01..03 koşulları sağlanmadan summon 0; AC-MAG-04/05 | insan + telemetri |
| T-IGT-PTY-01 | Party ortak hedefi uygular; heal ile öldürülemeyen hedefte taktik değiştirir | T-PTY-02/03, EVAL-HEALSTALL | MET-TGT-03 ≥ %75, MET-TGT-04, AC-PTY-03, MET-STALL-01 | karar logu + insan |
| T-IGT-SUR-01 | Düşük HP'de geri çekilir, uygun koşulda savaşa döner | T-SUR-01..04 | MET-SUR-01 ≥ %70, MET-SUR-03, MET-SUR-07, AC-SUR-01 | insan + telemetri |
| T-IGT-EVAL-01 | Tekrarlanabilir 8v8 ve güçlü rakiplere karşı ölçülebilir PK kalitesi | EVAL-8v8-A/MIX (B grubu rakipler dahil) | AC-EVAL-01..03; başlangıç doğrulaması (§6a) %100; geçersiz maç ≤ %10 | otomatik rapor + insan formu |
## 5. Rakip profilleri ve baseline'lar

| Kimlik | Tanım | Kullanım |
|---|---|---|
| B0-NAIVE | En yakın düşmana saldırır, rastgele hazır skill, geri çekilme yok, pot %30'da | Alt sınır (sanity); baseline-v1 bunu açıkça yenmeli |
| baseline-v1 | L0 deterministik politika ([14](14_LEARNING_AND_ADAPTATION.md) §3), dondurulmuş | Ana karşılaştırma |
| OP-AGGRO | En düşük HP'li düşmana odak, takip sınırı yok | Eğitim (A grubu) |
| OP-KITE | Mage'ler azami mesafe ve sürekli yavaşlatma | Eğitim (A) |
| OP-TURTLE | İki priest, sık regroup, düşük risk | Değerlendirme (B) |
| OP-HEALER-FIRST | Daima düşman priest'e odak | Değerlendirme (B) |
| OP-SPREAD | Dağınık konumlanma, alan hasarından kaçınma | Değerlendirme (B) |
| OP-RANDOM | Rastgele hedef ve rastgele geçerli skill | Eğitim (A) |

A grubu profiller eğitimde, B grubu yalnızca kilitli değerlendirmede kullanılır ([14](14_LEARNING_AND_ADAPTATION.md) §9).

## 6. Değerlendirme protokolü (özet)

1. Seed listesi ve tekrar sayısı senaryo dosyasında sabittir.
2. Her seed için iki maç: taraflar değiştirilir (arena A).
3. Ekipman S1, envanter STK-01.
4. Geçersiz maçlar ([16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md) §7) ayrı sayılır.
5. Sonuç raporu `ScenarioRunner` tarafından üretilir ve [21](21_PROJECT_TRACKING_TEMPLATES_AND_DOC_RULES.md) §4.5 test kanıt kaydına eklenir.

### 6a. Senaryo başlangıç sıfırlama sözleşmesi (`ScenarioReset`, ADR-0032-DEG)

Bot durumunun bir kısmı DB'de kalıcıdır (bot çıkışında kayıt, AC-ARCH-05): konum, HP/MP/NP, envanter. **Maç başlamadan önce** botların konumu ve durumu test kurulumu kapsamında belirlenir (ADR-0032-DEG: bot çevrimdışıyken bot satırına DB yazımı; neden DB ve canlı `CUser` ile tutarlılık ADR'de). **Maç başladıktan sonra** hareket, ölüm, respawn ve savaşa dönüş yalnızca normal oyun mekanikleriyle olur; maç içi ışınlama kurtarma teleportudur ve maçı geçersiz kılar. Her maçın (tekrarın) başında aşağıdakiler sıfırlanır ve **doğrulanır**:

| Durum | Sıfırlama | Kim | Doğrulama |
|---|---|---|---|
| Konum | MATCH_START **öncesi kurulum yerleşimi** (başlangıç noktası arena merkezinden ±35 m): bot `DESPAWNED` iken satıra yazılır, bot normal giriş yoluyla açılır | `ScenarioRunner` Prepare | tüm botlar başlangıç noktasından ≤ 3 m |
| HP / MP / NP | HP = MaxHP, MP = MaxMP, NP ≥ 1000 (KI-013: NP 0 → `Regene` yok; her ölüm −50) | `ScenarioRunner` (yalnız `BOT_TABLE` bot satırlarına `UPDATE`, kişisel veri tablosu okunmadan) | `snap` ile `SelfState` |
| Buff/debuff, DoT, cooldown, cast durumu | Çıkışta bellek içi durum biter; taze oturumda boş olduğu doğrulanır | Prepare doğrulaması | `buffTotal = 0`, `cooldownTotal = 0` |
| Pot ve tüketilebilir eşyalar | STK-01: tüketilen potlar senaryo stokuna **doldurulur**, tüketilmeyen potlar 1 adet, taş/scroll senaryo listesi (envanter doldurma ADR-0018 m.8 ile ortak) | `ScenarioRunner` (yalnız bot envanteri) | `hpPotStock`/`mpPotStock` = senaryo |
| Party üyelikleri ve roller | Maç sonunda tüm botlar party'den çıkar; maç başında party **gerçek paketlerle** kurulur (betik/senaryo) ve doğrulanır | `ScenarioRunner` + betik | `TeamView` üye sayısı/lider = senaryo |
| Hedef, rezervasyon, karar hafızası, `EnemyIntel`, durum makinesi | Her maçta yeni `BotAgent`; `TeamBlackboard.Clear()`; önceki maçtan hiçbir şey taşınmaz | Brain/`TeamBlackboard` | `MATCH_START` sonrası durum `PREPARE`, blackboard boş (assert) |
| Politika ve rastgelelik | Politika sürümleri senaryo dosyasında sabit; `seed_bot = hash(seed_episode, bot_slot)` `MATCH_START` olayında yazılır | `ScenarioRunner` | `MATCH_START.policy`, `.seed` |
| Çevre | Arena + 120 m çevresinde canavar/NPC yok (ARENA-03); tüm botlar despawn → spawn (taze oturum) | `ScenarioRunner` | `npcs` = 0 |

**Başlangıç doğrulaması:** Prepare sonunda yukarıdaki koşullar denetlenir; biri sağlanmazsa maç **başlamaz**, `MATCH_START` yerine `SETUP_FAIL` (neden listesiyle) yazılır ve maç geçersiz sayılır (`docs/16` §7).

**Karakter seti ve kapasite (kesin döküm).** Eşzamanlı ihtiyaç 8v8 için **16 karakter** (ulus başına 8); `MAX_BOTS = 16` yalnızca eşzamanlı slot kapasitesidir, karakter varlığını karşılamaz. Bugün DB'de 12 karakter vardır (ulus başına 6: W-P, W-G, P-HD, P-HB, M-F, M-I).

| Hesap | Ulus başına | Toplam | Neden |
|---|---|---|---|
| Bugünkü set | 6 | 12 | 6 rol profili × 2 ulus (ADR-0002) |
| **EVAL-8v8-A için asgari (C8-A: 2 W-P, 1 W-G, P-HD, P-HB, 2 M-F, 1 M-I)** | 8 (+1 W-P, +1 M-F) | **16** | aynı anda 16 karakter |
| Kompozisyon çeşitliliği (C8-B 3 W-P + 1 M-F; C8-C 1 W-P + 3 M-F; C8-D 3 W-P + 2 M-F) | 10 (+1 W-P, +1 M-F daha) | **20** | 3. W-P ve 3. M-F aynı sabit karakter kümesinden seçilebilsin |

**20 karakter 16'nın üstüne 4 ek karakterdir (ulus başına 3. W-P ve 3. M-F); yedek ya da test profili değildir.** Kaynağı kompozisyon çeşitliliğidir: EVAL-8v8-MIX (C8-B vs C8-C) ve `docs/14` §13 kompozisyon genellemesi. Aynı anda en fazla 16 karakter girişlidir; 4'ü her maçta boştadır. OP-* rakip profilleri, B0-NAIVE ve baseline aynı sınıf karakterlerini farklı politikayla oynatır: ek karakter gerektirmez. Seçenek: yalnızca 16 karakterle başlayıp (EVAL-8v8-A) 20'ye kompozisyon çeşitliliği eklenirken çıkmak; ya da 16 karakter + kompozisyon başına DB'de sınıf/skill/ekipman yeniden yazımı (yavaş ve hata riskli; seçilmedi). F8 ön koşulu: `db/003` ve `BOT_TABLE`'ın sabit 12 girişten DB/ini kaynaklı tabloya çevrilmesi. F7 küçük takım testleri (≤ C5) mevcut 12 karakterle çalışır. 32/64 bot performans testleri (T-PERF-02..04) için ayrı karakter kümesi veya aynı karakterlerin ardışık yeniden doğuşu tanımlanmalıdır.

### 6b. Kazanma kuralları (ADR-0031-DEG)

Süre dolması **kendiliğinden kazanma değildir.** `MATCH_END.result` ∈ `win_a` | `win_b` | `draw` | `invalid` | `no_result` (teknik sonlanma `completed`/`aborted` ayrı alandır). Kill = bot–bot PvP öldürme (canavar/kule/intihar/bilinmeyen kaynak sayılmaz); `fark = K_A − K_B`; başlangıç = ilk hasar (`engage`); `engage_timeout_sec` (60) içinde hasar yoksa maç `invalid` (`NO_ENGAGE`). Senaryo `win_rule` ile **tür** seçer; tüm eşikler senaryo anahtarıdır (kodda sabit değil) ve `MATCH_START`'a yazılır:

| `win_rule` | Kural | Anahtarlar (varsayılan) |
|---|---|---|
| `killdiff_timed` (respawn **açık**; EVAL varsayılanı) | Süre sonunda `fark ≥ win_margin` → `win_a`; `fark ≤ −win_margin` → `win_b`; `\|fark\| < win_margin` → `draw` | `duration_sec` (EVAL-8v8 300, 2v2..5v5 120), `win_margin` (2), `early_end_margin` (0 = kapalı) |
| `wipe_first` (respawn **kapalı**, ayrı tür) | Bir takımın tüm üyeleri ölü → rakip galip (anında biter); iki takım aynı tick'te 0 canlı → `draw`; süre dolunca canlı sayısı fazla olan galip, eşitse `draw`; 1v1 = ilk ölen kaybeder | `duration_sec` (300), `respawn` (off), `resurrection` (off) |
| `timed_score` | Yalnızca ölçüm, `no_result` | `duration_sec` |

**`killdiff_timed` örnekleri (8v8, 300 sn, `win_margin = 2`):**

| K_A | K_B | fark | Sonuç |
|---|---|---|---|
| 14 | 11 | +3 | `win_a` (galibiyet) |
| 10 | 8 | +2 | `win_a` (sınır dahil) |
| 9 | 8 | +1 | `draw` |
| 8 | 8 | 0 | `draw` |
| 8 | 9 | −1 | `draw` |
| 7 | 9 | −2 | `win_b` (A mağlup) |
| 0 | 0 | 0 | `draw` (hasar vardı) veya `invalid` `NO_ENGAGE` (hiç hasar yoktu) |

`win_margin = 3` ile (14, 11) `win_a`, (10, 8) ve (7, 9) `draw` olur. **`wipe_first` örnekleri (8v8):** B'nin son botu ölür, A'da 3 canlı → `win_a`; süre dolar, A 4 canlı B 2 canlı → `win_a`; A 3 / B 3 → `draw`; son botlar aynı tick'te ölür → `draw`. **Pilot kalibrasyonu:** `baseline-v1` aynı-aynıya 20 maç (taraf değişimli); `fark` ortalaması/standart sapması ve beraberlik oranı raporlanır; hedef beraberlik %15–35; dışındaysa `win_margin` değiştirilir (ADR-0031-DEG eki, kod değişmez). Tekrar sayıları ve taraf değişimi `docs/16` §7'dedir.

## 7. İnsan değerlendirmesi

| Unsur | Tanım |
|---|---|
| Katılımcı | En az 3 insan oyuncu (KO PvP deneyimli); test hesapları botlarla aynı kurulum betiğiyle level 80 master ve S1 setiyle hazırlanır (K-3) |
| Oturum | 2 × 30 dk: botlarla aynı takımda ve botlara karşı |
| Kör test | İnsan, karşı takımdaki karakterlerin bot mu insan mı olduğunu tahmin eder (karma takımlarda) |
| Form (1–5 ölçek) | Hedef seçimi mantıklı mı; healer davranışı; geri çekilme zamanlaması; ortak hedefe katılım; chat çağrılarının yararı; "bot gibi" davranış (tekrarlı, insanüstü tepki); adalet algısı (bot hile yapıyor gibi mi) |
| Serbest not | En iyi ve en kötü 3 davranış |
| Kullanım | Politika eğitiminde kullanılmaz ([14](14_LEARNING_AND_ADAPTATION.md) §13); faz kapısında kalite göstergesi |

## 8. Kabul kriteri özeti

| Alan | AC kimlikleri | Tanım yeri |
|---|---|---|
| Warrior | AC-WAR-01..06 | 06 §11 |
| Priest | AC-PRI-01..08 | 07 §16 |
| Mage | AC-MAG-01..05 | 08 §12 |
| Takım | AC-PTY-01..07 | 09 §14 |
| Solo | AC-SOLO-01..05 | 10 §8 |
| Hayatta kalma/pot | AC-SUR-01..05 | 11 §8 |
| Navigasyon | AC-NAV-01..06 | 12 §11 |
| Mimari | AC-ARCH-01..06 | 13 §14 |
| Öğrenme | AC-LRN-01..06 | 14 §14 |
| Takım düzeyi kalite (8v8) | AC-EVAL-01: baseline-v1, B0-NAIVE'i EVAL-8v8-A'da SPRT ile yener (H1: +100 Elo). AC-EVAL-02: baseline-v1, B grubu profillere karşı her birinde kazanma oranı %95 GA alt sınırı ≥ %35 (ezilmeme). AC-EVAL-03: İnsan formu ortalaması "hedef seçimi" ve "healer davranışı" ≥ 3,0 | Bu doküman |

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
| 2026-10-02 | v1.1 | Değerlendirme: §4.9 oyun içi kabul testleri (T-IGT-*), §6a senaryo başlangıç sıfırlama sözleşmesi ve karakter seti kapasitesi, §6b kazanma kuralı (ADR-0031-DEG) |
| 2026-10-02 | v1.2 | Değerlendirme eki (proje sahibi kararları): §6a konum kurulumu ve DB gerekçesi, 16/20 karakter dökümü; §6b `killdiff_timed`/`wipe_first`/`timed_score` türleri, örnekler, yapılandırılabilir eşikler, pilot kalibrasyonu |
