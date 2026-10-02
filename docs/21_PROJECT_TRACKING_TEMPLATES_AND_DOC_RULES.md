# 21 — Proje Takip Şablonları ve Dokümantasyon Kuralları

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Bu doküman, başka bir geliştiricinin veya yapay zekâ kodlama ajanının işi kaldığı yerden devralabilmesi için gereken **durum takibini, kanıt formatını ve doküman değişiklik kurallarını** tanımlar. Şablonların kopyalanabilir hâlleri `templates/` klasöründedir.

---

## 1. Durum tanımları (iş kalemi ve faz için)

Bir iş kalemi veya faz yalnızca aşağıdaki durumlardan birinde olabilir. **Kodun yazılmış olması tek başına "Tamamlandı" anlamına gelmez.**

| Durum | Tanım | Bu duruma geçiş için gereken kanıt |
|---|---|---|
| `PLANLANDI` | Kapsam, kabul kriterleri ve test kimlikleri yazılı | [17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md)'te faz tanımı, [20](20_REQUIREMENTS_TRACEABILITY_MATRIX.md)'de satır |
| `GELIŞTIRILIYOR` | Kod üzerinde çalışılıyor | Branch adı, ilk commit |
| `GELIŞTIRILDI` | Kod ana geliştirme branch'ine birleşti, derleniyor, birim/entegrasyon testleri geçiyor | Commit SHA, derleme kaydı, birim test çıktısı |
| `TEST_EDILDI` | Fazın test senaryoları ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)) çalıştırıldı ve sonuçlar kaydedildi (geçti veya kaldı fark etmez) | Test kanıt kaydı (§4.5), telemetri dosya yolları |
| `KABUL_EDILDI` | Tüm kabul kriterleri ölçülen değerlerle karşılandı, faz sonuç raporu onaylandı | Faz sonuç raporu (§4.2), onay satırı |
| `BLOKE` | Dış bağımlılık veya açık soru nedeniyle ilerlenemiyor | Blokajı açıklayan açık soru kimliği ([18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md)) |
| `ERTELENDI` | Kapsamdan bilinçli olarak çıkarıldı | ADR kimliği |
| `GERI_ALINDI` | Kabul edilmiş bir değişiklik regresyon nedeniyle geri alındı | Bilinen sorun kimliği + geri alma commit'i |

Kurallar:

1. `TEST_EDILDI` → `KABUL_EDILDI` geçişini kodu yazan kişi/ajan tek başına yapamaz. Faz raporunda ölçülen değerlerin kabul kriterleriyle tek tek eşleştirilmesi zorunludur. İnsan onayı tercih edilir; yapay zekâ ajanı onaylıyorsa rapor otomatik metrik çıktısına bağlanmalıdır.
2. Bir sonraki faz, önceki fazın çıkış koşulları ([17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md)) `KABUL_EDILDI` olmadan `GELIŞTIRILIYOR` durumuna geçemez. İstisna: [17]'de "paralel yürütülebilir" olarak işaretlenmiş iş kalemleri.
3. Bir test kaldıysa durum yine `TEST_EDILDI` olur; sonuç raporda "kaldı" olarak yazılır ve faz `KABUL_EDILDI` olmaz.

## 2. Depo içi önerilen yerleşim `[Ö]`

Depodaki klasör yapısı (paket `docs/` altındadır; diğer klasörler ihtiyaç doğdukça oluşturulur):

```
docs/                         # bu doküman paketi (00..21), appendix/, templates/
docs/STATUS.md                # güncel durum dosyası (tek dosya, sürekli güncel)
docs/KNOWN_ISSUES.md          # bilinen sorunlar
docs/adr/ADR-0001-*.md        # teknik karar kayıtları
docs/phase-reports/F<n>-<tarih>.md
docs/test-evidence/<test-id>/<tarih>-<kisa-sha>.md
docs/policies/                # politika dosyaları (L1/L2) ve sürüm işaretçileri
plans/                        # uygulama planları (Claude yazar, DeepSeek uygular) — akış: plans/README.md
tools/build.sh                # WSL'den MSBuild ile derleme
AGENTS.md, opencode.json      # uygulayıcı (opencode/DeepSeek) kuralları ve izinleri
CLAUDE.md, .claude/skills/    # planlayıcı/denetçi (Claude Code) kuralları; /plan-olustur, /plan-dogrula
```

Telemetri dosyaları (büyük) depoya konmaz; kanıt kayıtlarında yol, boyut ve SHA-256 özeti verilir.

## 3. Commit ve branch kuralları `[Ö]`

- Branch adı: `bot/F<n>-<kisa-konu>` (ör. `bot/F2-socketless-user`).
- Commit mesajı ilk satırı: `[F<n>] <bileşen>: <özet>`; gövdede ilgili REQ/T/AC kimlikleri.
- Faz sonuç raporu, kabul edilen durumu temsil eden commit SHA'sını içerir. Rapor commit'i ile kod commit'i ayrı olabilir.
- Sunucu mekaniğini değiştiren her commit (bot dışı kod) ayrı işaretlenir: `[MECH]` etiketi ve [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) güncellemesi zorunludur.

## 4. Şablonlar

### 4.1 Güncel durum dosyası (`STATUS.md`)

```markdown
# Bot Projesi — Güncel Durum

Son güncelleme: YYYY-MM-DD HH:MM (UTC+3) · Güncelleyen: <isim/ajan>
Sunucu commit: <sha> · Bot kodu commit: <sha> · Doküman paketi sürümü: <vX.Y>
Veritabanı özeti: MAGIC=<sha256-kısa>, ITEM=<sha256-kısa>, ZONE_INFO=<sha256-kısa>

## Aktif faz
F<n> — <ad> — Durum: <durum> — Başlangıç: <tarih>

## Faz tablosu
| Faz | Durum | Son rapor | Kabul commit |
|---|---|---|---|
| F0 | KABUL_EDILDI | phase-reports/F0-2026-10-15.md | abc1234 |
| F1 | TEST_EDILDI | phase-reports/F1-2026-10-28.md (2 kriter kaldı) | — |

## Son test koşuları (en yeni 10)
| Tarih | Test | Sonuç | Kanıt |
|---|---|---|---|

## Blokajlar
| Açık soru / sorun | Etki | Sahibi | Hedef tarih |
|---|---|---|---|

## Sıradaki 3 adım
1. ...

## Devralan için notlar
- Çalışma ortamı kurulum farkları, geçici çözümler, dikkat edilmesi gereken yerler.
```

### 4.2 Faz sonuç raporu

```markdown
# Faz Sonuç Raporu — F<n> <ad>

Tarih: YYYY-MM-DD · Hazırlayan: <isim/ajan> · Onaylayan: <isim> (onay tarihi)
Değerlendirilen commit: <sha> · Sunucu commit: <sha> · DB özeti: <...>

## 1. Amaç (17'den kopya)
## 2. Teslim edilen kapsam
| İş kalemi | Durum | Commit(ler) | Not |
## 3. Kapsam dışında kalanlar / ertelenenler
## 4. Çalıştırılan testler
| Test kimliği | Tekrar | Sonuç | Kanıt kaydı |
## 5. Kabul kriterleri
| AC kimliği | Kriter | Ölçülen değer | GA (%95) | Karşılandı mı |
## 6. Sapmalar ve açıklamaları
## 7. Yeni bilinen sorunlar (KNOWN_ISSUES kimlikleri)
## 8. Bu fazda alınan kararlar (ADR kimlikleri)
## 9. Doküman güncellemeleri (dosya + bölüm + değişiklik özeti)
## 10. Çıkış kararı
- [ ] Tüm çıkış koşulları karşılandı → KABUL_EDILDI
- [ ] Kısmi → TEST_EDILDI olarak kalır; eksikler: ...
## 11. Geri alma bilgisi
Bu fazın değişikliklerini geri almak için: <commit aralığı / feature flag / config>
```

### 4.3 Teknik karar kaydı (ADR)

```markdown
# ADR-<NNNN>: <başlık>

Durum: ÖNERİLDI | KABUL | REDDEDİLDİ | YERİNE_GEÇİLDİ (ADR-xxxx)
Tarih: YYYY-MM-DD · Karar verenler: ...
İlgili: REQ-..., F<n>, açık soru Q-...

## Bağlam
Kanıt etiketleriyle ([D]/[V]/[S]/[B]/[Ö]/[A]) mevcut durum.
## Karar
## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
## Sonuçlar
Olumlu / olumsuz / riskler / geri alma yolu.
## Doğrulama
Bu kararın doğru olduğunu gösterecek test veya metrik.
```

ADR listesi (verilen kararlar `adr/` klasöründe; ayrıntı [18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md) §1):

| ADR | Konu | Durum |
|---|---|---|
| ADR-0001 | Botların temsil katmanı (sunucu tarafı oyuncu varlığı) | KABUL (K-1) |
| ADR-0002 | Level 80 ve master sınıf: DB kurulum betiği; insanlar da aynı | KABUL (K-2, K-3) |
| ADR-0003 | MAGIC.Etc düzeltmesi: depoda geri alınabilir SQL betiği | KABUL (K-4) |
| ADR-0004 | Test arenası: yalnızca arena A, taraf değişimi | KABUL (K-6) |
| ADR-0005 | Bot karar döngüsünün bağlanacağı thread/timer | KABUL (otonom döngüde Claude kararı, 2026-10-02 — gözden geçirilmeli; [adr](adr/ADR-0005-bot-tick-thread-modeli.md)) |
| ADR-0006 | Navigasyon veri kaynağı (SMD ızgarası) ve görüş hattı yaklaşımı | KABUL (otonom döngüde Claude kararı, 2026-10-02 — gözden geçirilmeli; [adr](adr/ADR-0006-navigasyon-izgara-astar.md)) |
| ADR-0007 | Telemetri formatı ve depolama | KABUL (otonom döngüde Claude kararı, 2026-10-02 — gözden geçirilmeli; [adr](adr/ADR-0007-telemetri-formati-ve-depolama.md)) |
| ADR-0008 | Öğrenme katmanı L1 yöntemi | AÇIK |
| ADR-0009 | Tüketilmeyen pot verisi (MB-01): olduğu gibi kalır | KABUL (K-5) |
| ADR-0010 | Görev kapılı master skill'ler ilk sürümde yok | KABUL (K-7) |
| ADR-0011 | Mekanik hatalar şimdilik olduğu gibi | KABUL (K-8) |
| ADR-0012 | Botlar ranking/ödül/duyurulara normal oyuncu gibi dahil | KABUL (K-9) |
| ADR-0013 | Upstream PR #10 alınmaz | KABUL (K-10) |
| ADR-0014 | Bot oturumu hesap doğrulamasını ve `SET_LOGIN_INFO`'yu atlar | KABUL (otonom döngüde Claude kararı, 2026-10-02 — gözden geçirilmeli; [adr](adr/ADR-0014-bot-oturumu-hesap-dogrulamasi.md)) |
| ADR-0015 | Bot çalışma zamanı komut kanalı: konsol `/bot` + komut dosyası | KABUL (otonom döngüde Claude kararı, 2026-10-02 — gözden geçirilmeli; [adr](adr/ADR-0015-bot-calisma-zamani-komut-kanali.md)) |

### 4.4 Bilinen sorunlar (`KNOWN_ISSUES.md`)

```markdown
| Kimlik | Başlık | Önem (K/Y/O/D) | Bileşen | İlk görüldüğü commit | Tekrar üretme | Geçici çözüm | Durum | İlgili test / commit |
|---|---|---|---|---|---|---|---|---|
| KI-001 | MAGIC.Etc=1 satırları quest 1 istiyor | Y | Veri | 0f52027 | start.md §4 | Etc=0 güncellemesi | AÇIK (veri düzeltmesi yerel) | T-DATA-02 |
```

Önem: K = Kritik (test yapılamıyor), Y = Yüksek (sonuçları bozuyor), O = Orta (geçici çözüm var), D = Düşük.

Başlangıç listesi (depo/sürümden bilinen; ayrıntı [18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md)):

| Kimlik | Başlık | Kaynak |
|---|---|---|
| KI-001 | `MAGIC.Etc = 1` satırları quest 1 tamamlanmış olmasını istiyor; yeni karakterde skill/pot çalışmıyor | `start.md` §4, yerel DB'de `MAGIC_BAK_etc` mevcut |
| KI-002 | `WIZ_WARP` istemcinin verdiği koordinata yalnızca harita sınırı kontrolüyle ışınlıyor; paket yalnızca GM'lerden kabul ediliyor ([`GameServer/User.cpp:313-316`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L313-L316), [`GameServer/User.cpp:626-659`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L626-L659)) | `start.md` §9; kod [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) MEC-MOV-06 |
| KI-003 | Zones.tbl (istemci) ve sunucu SMD dosya adları zone 71/72 için ters | `start.md` §9; [12](12_NAVIGATION_AND_POSITIONING.md) |
| KI-004 | Konsol çıktısı dosyaya yönlendirildiğinde boş kalıyor | `start.md` §9 |
| KI-005 | Moradon plaza merdiveninde karakter sıkışması | `start.md` §9 |
| KI-006 | 360/720 HP ve 960/1920 MP potlarının NPC sürümleri sunucuda tüketilmiyor (MB-01) — **kabul edildi** (K-5) | Yerel MAGIC verisi, [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §6.2 |

### 4.5 Test kanıt kaydı

```markdown
# Test Kanıtı — <T-ID> — YYYY-MM-DD

Mod: eval | train | debug · Çalıştıran: <isim/ajan>
Sunucu commit: <sha> · Bot commit: <sha> · Politika: <id@sürüm> · DB özeti: <...>
Ortam: <makine, CPU, OS>, sunucu derleme tipi (Release/Debug)
Senaryo parametreleri: <kompozisyon, ekipman seti kimliği, seed listesi, tekrar sayısı, taraflar>

## Sonuçlar
| Metrik | Değer | GA (%95) | Eşik | Sonuç |

## Geçersiz sayılan koşular ve nedenleri
## Telemetri dosyaları
| Dosya | Boyut | SHA-256 |
## Gözlemler (insan değerlendirmesi varsa)
## Karar: GEÇTİ / KALDI / GEÇERSİZ
```

## 5. Dokümantasyon değişiklik kuralları

1. **Tek kaynak ilkesi.** Her bilgi türünün tek bir sahibi dokümanı vardır:

   | Bilgi türü | Sahip doküman |
   |---|---|
   | Sunucu mekaniği, doğrulanmış oyun kuralları, gözlemlenebilirlik | [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) |
   | Skill verisi (ID, maliyet, süre, menzil) | [05](05_SKILL_CATALOG_AND_COMBAT_RULES.md) |
   | Stat/skill puanı bütçesi, ekipman setleri | [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) |
   | Depo yapısı ve entegrasyon noktaları | [02](02_REPOSITORY_ANALYSIS_AND_INTEGRATION_MAP.md) |
   | Bileşenler, veri modeli, parametre kayıt defteri biçimi | [13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md) |
   | Davranış parametrelerinin varsayılanları | İlgili davranış dokümanı (06–12) |
   | Metrik tanımları, log şeması | [16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md) |
   | Test senaryoları ve kabul kriterleri | [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md) (faz kapıları [17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md)) |
   | Açık sorular, riskler, varsayımlar | [18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md) |
   | Kaynaklar | [19](19_SOURCES_AND_EVIDENCE.md) |
   | Gereksinim → doküman/faz/test eşlemesi | [20](20_REQUIREMENTS_TRACEABILITY_MATRIX.md) |

   Diğer dokümanlar bu bilgileri **kopyalamaz, kimlik/bağlantı ile referans verir.** Okunabilirlik için kısa alıntı yapılırsa yanına "(kaynak: 05 §3.2)" yazılır.

2. **Kanıt etiketi zorunluluğu.** Mekanik veya veri içeren her yeni ifade bir etiket taşır: `[D]` depo kodu, `[V]` yerel sürüm verisi (DB/harita), `[S]` sürüm için dış kaynak, `[B]` başka sürüm/doğrulanmamış, `[Ö]` tasarım önerisi, `[A]` açık/çalışma zamanı testi gerekli. Etiketsiz mekanik iddia içeren değişiklik kabul edilmez.
3. **Etiket yükseltme.** Bir `[A]` veya `[B]` ifadesi çalışma zamanı testi ile doğrulandığında `[D]`/`[V]` olarak güncellenir ve yanına test kanıt kaydının yolu eklenir (ör. `[V: T-MECH-BUF-03 2026-11-02]`).
4. **Sıralı güncelleme.** Mekanik bir bulgu değişirse sıra: (a) 03 güncellenir → (b) etkilenen davranış dokümanları → (c) 15'teki testler → (d) 20'deki eşleme → (e) 18'de ilgili açık soru kapatılır.
5. **Sürüm ve değişiklik günlüğü.** Her dokümanın başlığında `Durum: <Taslak|Gözden geçirildi|Onaylı> vX.Y` bulunur. Her dokümanın sonuna değişiklik günlüğü tablosu eklenir (tarih, sürüm, değişiklik, ilgili commit/ADR).
6. **Kimlikler değişmez.** REQ, MEC, T, AC, MET, P, Q, KI, ADR kimlikleri silinmez ve yeniden kullanılmaz; kaldırılan öğe "KALDIRILDI (gerekçe)" olarak işaretlenir.
7. **İzlenebilirlik.** Yeni gereksinim, test veya kabul kriteri eklendiğinde [20](20_REQUIREMENTS_TRACEABILITY_MATRIX.md) aynı değişiklikte güncellenir.
8. **Yapay zekâ ajanları için ek kural.** Ajan, çalıştırmadığı testi "geçti" olarak yazamaz; doğrulamadığı mekanik için `[D]` etiketi kullanamaz; yalnızca kod yazdığı bir iş kalemini `GELIŞTIRILDI`'den ileri taşıyamaz.

## 6. Devralma kontrol listesi

Yeni bir geliştirici veya ajan işe başlarken:

1. `STATUS.md`'yi oku: aktif faz, blokajlar, sıradaki adımlar.
2. Aktif fazın [17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md) tanımını ve kabul kriterlerini oku.
3. [18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md)'de aktif fazı bloke eden açık soruları kontrol et.
4. `KNOWN_ISSUES.md`'de aktif bileşenleri etkileyen kayıtları kontrol et.
5. Son faz raporundaki commit'i derle, F0 duman testlerini ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md) T-ENV-*) çalıştır; ortamın beklenen durumda olduğunu doğrula.
6. Çalışmaya başlamadan önce `STATUS.md`'ye "devralındı" satırı ekle.

## 7. Planlayıcı–uygulayıcı iş akışı

Proje sahibinin kararıyla (2026-10-01) geliştirme şu düzende yapılır:

- **Claude (Claude Code):** planları `/plan-olustur` ile `plans/` altına yazar. Uygulanan işi `/plan-dogrula` ile denetler ve plan dosyasına Doğrulama Raporu yazar.
- **DeepSeek (opencode):** planı `AGENTS.md` kurallarıyla, `bot/<FAZ>-<NN>` branch'inde uygular ve Uygulayıcı Raporu yazar.

Akışın ve plan durumlarının tek kaynağı `plans/README.md`'dir. Plan durumları (`HAZIR … DOĞRULANDI … KAPANDI`) iş kalemi düzeyindedir. Faz durumu (§1) yine faz sonuç raporuyla belirlenir.

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
| 2026-10-01 | v1.1 | Depo yerleşimi `docs/`; §7 planlayıcı–uygulayıcı iş akışı; ADR durumları |
