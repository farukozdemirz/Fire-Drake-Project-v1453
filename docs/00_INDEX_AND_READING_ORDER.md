# 00 — Dizin ve Okuma Sırası

> **Fire Drake Project v1453 — PK Bot Teknik Dokümantasyon Paketi**
> Paket sürümü: v1.0 (Taslak) · Hazırlanma tarihi: 2026-10-01
> İncelenen depo: `ko4life-net/Fire-Drake-Project-v1453`, branch `main`, commit [`0f520272ae1f11472623d62bff76fff98562e7b3`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/commit/0f520272ae1f11472623d62bff76fff98562e7b3) (2020-10-12; upstream ile aynı, 2026-10-01'de kontrol edildi)
> Yerel veri: `FDP_kn_online` (SQL Server Express, salt okunur sorgular), `Map/freezone_a_20050718.smd` (zone 71)
> **Önemli:** Bu araştırmada sunucu **çalıştırılmadı**, depoda **değişiklik yapılmadı**. Tüm davranış iddiaları kod okumasına, yerel veriye veya kaynaklandırılmış dış bilgiye dayanır. Çalışma zamanı doğrulaması F1 fazının işidir.

---

## 1. Paket içeriği

| # | Dosya | İçerik | Sahibi olduğu bilgi |
|---|---|---|---|
| 00 | `00_INDEX_AND_READING_ORDER.md` | Bu dosya | Kimlik aileleri, sözlük |
| 01 | `01_PRODUCT_SCOPE_AND_REQUIREMENTS.md` | Hedef, kapsam, kısıtlar, gereksinimler | REQ-*, CON-* |
| 02 | `02_REPOSITORY_ANALYSIS_AND_INTEGRATION_MAP.md` | Depo mimarisi, thread modeli, yaşam döngüsü, bot temsil seçenekleri, entegrasyon noktaları | S1–S12, R-CODE-* |
| 03 | `03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md` | Sürüm kimliği; doğrulanmış sunucu mekaniği; istemci tarafı adalet sınırları; mekanik hatalar; gözlemlenebilirlik | MEC-*, CLI-*, MB-* |
| 04 | `04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md` | Stat/skill bütçeleri, 6 rol profili, referans ekipman setleri | CHR-*, profiller, S0–S2 |
| 05 | `05_SKILL_CATALOG_AND_COMBAT_RULES.md` | Skill kodlaması, çakışma grupları, sınıf çekirdek skill setleri, skill + R | SK-* |
| 06 | `06_WARRIOR_BEHAVIOR.md` | Warrior davranışı | P-WAR-*, AC-WAR-* |
| 07 | `07_PRIEST_BEHAVIOR.md` | Priest davranışı (heal/buff/debuff/cure/diriltme, iki priest) | P-PRI-*, AC-PRI-* |
| 08 | `08_MAGE_BEHAVIOR.md` | Mage davranışı (hasar, kiting, summon akışı) | P-MAG-*, SUM-*, AC-MAG-* |
| 09 | `09_PARTY_COORDINATION_AND_TARGET_SELECTION.md` | Takım rolleri, TeamBlackboard, hedef skoru, heal-stall, regroup, chat | P-TGT-*, P-PTY-*, P-TEAM-*, AC-PTY-* |
| 10 | `10_SOLO_PK_BEHAVIOR.md` | Solo PK | P-SOLO-*, AC-SOLO-* |
| 11 | `11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md` | Pot politikası, geri çekilme/dönüş, stok | P-POT-*, P-SUR-*, STK-*, AC-SUR-* |
| 12 | `12_NAVIGATION_AND_POSITIONING.md` | Harita verisi, yol bulma, LoS, takılma, güvenli nokta | P-NAV-*, AC-NAV-* |
| 13 | `13_BOT_ARCHITECTURE_AND_DATA_MODEL.md` | Bileşenler, thread modeli, bot oturumu, FSM, veri modeli, parametre kayıt defteri, komutlar | AC-ARCH-* |
| 14 | `14_LEARNING_AND_ADAPTATION.md` | Öğrenme yaklaşımı (L0–L3), ödül, değerlendirme, sürümleme | AC-LRN-* |
| 15 | `15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md` | Test arenası (arena A), senaryo kataloğu, rakip profilleri, insan değerlendirmesi | T-*, EVAL-*, OP-*, AC-EVAL-* |
| 16 | `16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md` | Olay modeli, karar logu, metrik kataloğu, istatistik kuralları | MET-* |
| 17 | `17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md` | F0–F10 fazları ve kapıları | F* |
| 18 | `18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md` | Kararlar (K-*), varsayımlar, açık sorular, riskler | K-*, A-*, Q-*, R-* |
| 19 | `19_SOURCES_AND_EVIDENCE.md` | Kaynak dizini, etiketler, çelişkiler | W-*, L-*, R-REPO |
| 20 | `20_REQUIREMENTS_TRACEABILITY_MATRIX.md` | Gereksinim → doküman → faz → test | — |
| 21 | `21_PROJECT_TRACKING_TEMPLATES_AND_DOC_RULES.md` | Durum tanımları, şablonlar, doküman kuralları, bilinen sorunlar başlangıcı | KI-*, ADR listesi |
| — | `templates/` | `STATUS.md`, `PHASE_REPORT.md`, `ADR.md`, `KNOWN_ISSUES.md`, `TEST_EVIDENCE.md` | — |
| — | `appendix/A1–A3_*.md` | Master warrior/priest/mage tam skill kataloğu (DB'den otomatik) | Skill verisi |
| — | `appendix/data/` | DB dökümleri (CSV): skill'ler, potlar, ekipman, katsayılar, seviye, Ronark NPC'leri | — |
| — | `appendix/maps/` | Zone 71 yükseklik, olay ızgarası, bileşen ve arena görselleri | — |
| — | `appendix/research/` | Kod araştırma notları (satır referanslı) ve web araştırma notları | — |
| — | `appendix/tools/` | Tabloları ve görselleri yeniden üreten salt okunur betikler | — |

## 2. Okuma sırası

| Okuyucu | Sıra |
|---|---|
| Proje sahibi | 00 → 18 §1 (verilen kararlar) → 01 → 17 → 15 §2 (arena) → 03 §15 (mekanik hatalar) |
| Sunucu geliştiricisi | 00 → 02 → 03 → 13 → 12 → 17 → 21 |
| Davranış/AI geliştiricisi | 00 → 03 → 04 → 05 → 09 → 06/07/08 → 10 → 11 → 14 → 16 |
| Test sorumlusu | 00 → 15 → 16 → 03 §13 → 17 |
| Devralan yapay zekâ ajanı | 00 → 21 §6 (devralma listesi) → `STATUS.md` → aktif faz (17) → ilgili dokümanlar |

## 3. Kanıt etiketleri

| Etiket | Anlam |
|---|---|
| `[D]` | Depodaki kodla doğrulandı (kod okuması; çalışma zamanı değil) |
| `[V]` | Yerel sürüm verisiyle (DB, harita dosyası) doğrulandı |
| `[S]` | İlgili sürüm/dönem için dış kaynakla doğrulandı |
| `[B]` | Başka sürümden (ör. resmî 1.298, modern KO) veya doğrulanmamış |
| `[Ö]` | Tasarım önerisi |
| `[A]` | Açık konu / çalışma zamanı testi gerekli |
| `[I]` | Çıkarım veya aritmetik |

Satır referansları `dosya:satır` biçimindedir ve commit `0f52027`'e ait GitHub bağlantılarıdır.

## 4. Kimlik aileleri

| Önek | Anlam | Tanım yeri |
|---|---|---|
| REQ-, CON- | Gereksinim, kısıt | 01 |
| S1–S12, R-CODE- | Entegrasyon noktası, kod riski | 02 |
| MEC-, CLI-, MB- | Sunucu kuralı, istemci tarafı adalet sınırı, mekanik hata | 03 |
| CHR- | Karakter kuralı | 04 |
| SK- | Skill kullanım kuralı | 05 |
| P- | Ayarlanabilir parametre (sahibi ilgili davranış dokümanı) | 06–12, 16 |
| SUM-, STK-, CMP-, ARENA- | Summon, stok, kompozisyon, arena kuralları | 08, 11, 09, 15 |
| T-, EVAL-, OP-, B0 | Test senaryosu, değerlendirme maçı, rakip profili, naif baseline | 15 |
| AC- | Kabul kriteri | 06–15 |
| MET- | Metrik | 16 |
| F0–F10 | Faz | 17 |
| K-, A-, Q-, R- | Karar, varsayım, açık soru, risk | 18 |
| W-, L- | Dış kaynak, yerel kaynak | 19 |
| KI-, ADR- | Bilinen sorun, teknik karar kaydı | 21 |

## 5. En önemli bulgular (özet)

1. **Sürüm:** Depo, 2006 dönemi USKO "Reign of the Fire Drake" mekaniklerine yakın, topluluk tarafından değiştirilmiş bir snoxd türevidir. Seviye sınırı 80'dir; master sınıflar vardır; Ronark Land = zone 71 `[D]`/`[S]` (03 §1).
2. **Bot katmanı:** NPC tabanlı botlar oyuncu gibi görünemez, party'ye katılamaz ve PvP formüllerini kullanamaz. Önerilen yol, ayrılmış oturum slotlarında **soketsiz `CUser`**'dır. Aksiyonlar gerçek paketler olarak IOCP thread'inde mevcut handler'lardan geçer (02 §10, 13).
3. **Sunucunun uygulamadıkları:** cast süresi, silah gecikmesi (yalnızca istemcinin bildirdiği değer), hareket cezaları (yavaşlatma/stun), yürünebilirlik/yükseklik/görüş hattı, aynı saniye içindeki recast. Bunların hepsi bot tarafında adalet kuralı olarak uygulanmalıdır (03 §13).
4. **Sunucunun uyguladıkları:** saniyede 1 başarılı R; ID < 400000 skill'lerde aynı tipten saniyede 1 kullanım; skill recast'leri (1 sn çözünürlük); sınıf koduna birebir bağlı skill'ler; BuffType başına tek buff (03 §3–5).
5. **Level 80 bütçesi:** 577 stat, 142 skill puanı (ağaç ≤ 80, master ≤ 20). "255 STR, kalan HP" warrior'ı geçerlidir. Priest'in iki hibrit rolü de bütçeye sığar. Mage'de hasar–HP ödünleşimi serttir (taban HP ~900–1600) (04).
6. **Sınıf yükseltme yok:** Bu kurulumda oyun içi master'a yükseltme akışı yok. Botlar (ve adil karşılaştırma için test insanları) DB betiğiyle master sınıf ve level 80 olarak oluşturulmalıdır (K-2, K-3).
7. **Potlar:** 720 HP = Water of favors, 1920 MP = Potion of soul. Sunucu recast'i 2 sn (HP ve MP cooldown'ları sunucuda ayrı); ölçülen istemci ve botun ortak aralığı ~2,5 sn (03 CLI-06). **Bu potların NPC sürümleri sunucuda tüketilmiyor (MB-01)**; karar K-5 gereği veri olduğu gibi kalır, kural insan ve bot için aynıdır (03 §6, 11 §2).
8. **Ronark Land'de respawn sonrası dokunulmazlık (blink) yok;** güvenli alan yok; respawn noktaları kendi guard tower halkasının içinde (03 §8, §12).
9. **Gözlemlenebilirlik:** Saldıran, vurduğu hedefin kesin HP'sini alır. Party üyelerinin HP/MP'si yayınlanır. Düşman MP'si ve cooldown'ları görünmez. Bu, botların karar girdilerini sınırlar (03 §16).
10. **Takım:** En fazla 8 üye. Lider ayrılırsa party silinir, ölürse silinmez. Chat'te sunucu tarafı oran sınırı yok; sınır bot tarafında (03 §10–11).
11. **Navigasyon:** Olay ızgarası 0 = engelli, 1 = açık; ana oynanabilir alan ~1,42 km². AIServer'ın A*'ı yürünebilirliği ters yorumluyor (MB-11). Botlar için yeni bir ızgara A* öneriliyor (12).
12. **Test arenası:** Karus kapısının hemen önü guard tower menzilinde, tarafsız değil. Karar K-6 gereği testler canavar ve tower menzilinden ≥ 120 m uzaktaki arena A'da (1274, 890) taraf değiştirilerek oynanır; koordinatlar F1'de oyun içinde doğrulanacak (15 §2).
13. **Asimetri:** Bazı El Morad skill'lerinin menzili Karus eşlerinden uzun (ör. Malice 90 vs 56). Taraf değiştirmeli değerlendirme zorunlu (05 §3).
14. **Öğrenme:** RL ile başlanmaz. L0 deterministik utility → L1 offline parametre optimizasyonu → (opsiyonel) L2 kısıtlı bandit; değişmez kurallar ve gözlem sözleşmesi korunur (14).
15. **Kararlar:** K-1…K-10 2026-10-01'de verildi (18 §1, `adr/`).

## 6. Sözlük

| Terim | Anlam |
|---|---|
| R / R vuruşu | Normal saldırı (istemcide R tuşu) |
| Skill + R | Bir skill ile normal saldırının aynı zaman penceresinde kullanılması (03 §13.1) |
| Tip kapısı | Aynı magic tipinden saniyede bir kullanım sınırı (MEC-MAG-03) |
| BuffType | Sunucunun buff/debuff'ları sınıflandırdığı tip; hedef başına tip başına tek kayıt |
| Blink | Respawn sonrası geçici dokunulmazlık (Ronark'ta yok) |
| Summon | Canlı party üyesini çağıranın yanına ışınlama (mage summon friend) |
| Diriltme | Ölü üyeyi ceset konumunda geri getirme (priest) |
| Respawn | Ölü oyuncunun kendi başlangıç noktasında yeniden doğması |
| Peel | Destek karakterine saldıran düşmanı ondan uzaklaştırma/bağlama |
| Heal-stall | Ortak hedefin heal ile ayakta tutulması durumu (09 §6) |
| TeamBlackboard | Botların takım bilgisini paylaştığı yapı (chat'ten bağımsız) |
| Gözlem sözleşmesi | Botun yalnızca istemcinin aldığı bilgiyi kullanması kuralı |
| BotFairnessGuard | İstemci tarafı sınırları (CLI-*) uygulayan bileşen |
| Arena A | Karus kapısı açıklığındaki test alanı (1274, 890); maçlar taraf değiştirilerek oynanır |
| SPRT | Ardışık olasılık oranı testi (satranç motoru testlerinde kullanılır) |
| L0/L1/L2/L3 | Öğrenme katmanları (14 §3) |

## 7. Bu paketin kendisi hakkında

- Dokümanlar birbirine bağlantılıdır. Bir bilgi türü tek dokümana aittir ([21](21_PROJECT_TRACKING_TEMPLATES_AND_DOC_RULES.md) §5).
- `appendix/` altındaki tablolar ve görseller betiklerle üretilmiştir. Veri değiştiğinde `appendix/tools/` ile yeniden üretilebilir. Betikler salt okunurdur ve yerel yolları içerir.
- Paketin depo içindeki konumu `docs/`'tur. Durum dosyası, ADR'ler, faz raporları ve test kanıtları da bu klasörde tutulur ([21](21_PROJECT_TRACKING_TEMPLATES_AND_DOC_RULES.md) §2).

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
