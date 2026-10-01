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
| T-ENV-01 | Temiz kurulum: derleme, DB geri yükleme, ini, sunucuların başlaması | Üç sunucu ayakta; bir insan istemcisi Ronark'a girebiliyor — *kısmen:* derleme/DB/ini/ODBC `tools/check-env.sh` ile 22/22 (F0-01); **sunucuların başlaması** `tools/run-servers.sh start` ile kanıtlandı (F0-02 Doğrulama Tur 2, 2026-10-01: üç sunucu ayakta, GameServer↔AIServer bağlı, her biri 1–5 sn'de hazır, `start` ~13–17 sn) `[V]`; **istemciyle Ronark'a giriş açık** (insan istemcisi gerekir) |
| T-ENV-02 | Release ve Debug derleme farkları (blink, quest kapısı, zaman aşımı) belgelenir | Fark tablosu — **TEST_EDILDI/GEÇTİ 2026-10-01**, kanıt: `plans/F0-01-…md`; tablo `docs/02` §2.1 |
| T-ENV-ARENA-01 | Arena A'da (ve yedek B'de) 30 dk canavar/NPC gözlemi | Arena + 120 m içinde varlık 0 |
| T-ENV-ARENA-02 | Arena koordinatlarının oyun içinde doğrulanması (GM ile yürüme, yükseklik, engeller) | Koordinat ve görüntü kaydı |
| T-ENV-ARENA-03 | Karus kapısı önünde El Morad karakterine tower saldırısı | Saldırı mesafesi ölçümü |
| T-ENV-ARENA-04 | Respawn noktaları ve arena arası yürüme süresi (iki ulus) | Süre kaydı (T-NAV-05 ile) |
| T-DATA-01..05 | [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) §7 | |
| T-DATA-06 | MAGIC.Etc düzeltmesinin kalıcı kaydı (KI-001, ADR-0003) | Script + doğrulama sorgusu |

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

### 4.3 Navigasyon

T-NAV-01..08, T-NAV-LOS-01: [12](12_NAVIGATION_AND_POSITIONING.md) §11.

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
