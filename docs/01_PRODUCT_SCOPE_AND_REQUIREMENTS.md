# 01 — Ürün Kapsamı ve Gereksinimler

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Bu doküman gereksinim kimliklerinin (REQ-*) tek kaynağıdır. Her gereksinimin hangi doküman, faz ve testle karşılandığı [20](20_REQUIREMENTS_TRACEABILITY_MATRIX.md)'dedir.
> "Kaynak" sütunu: **K** = kullanıcı gereksinimi (özgün talep), **Ö** = araştırmada keşfedilip eklenen **öneri** gereksinimi.

---

## 1. Ürün hedefi

Ronark Land içinde gerçek oyunculara ve diğer botlara karşı **solo veya party halinde etkili PK yapabilen** botlar. Bu botlar sınıflarını iyi kullanır, doğru zamanda doğru aksiyonu seçer, takım arkadaşlarıyla koordineli hareket eder ve savaşın gidişatına göre karar değiştirir.

**Başarı tanımı:** Başarı, kodun çalışması veya botların birbirini öldürebilmesi değildir. Başarı, [16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md)'daki metriklerle, [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)'teki senaryolarda ve istatistiksel olarak geçerli karşılaştırmalarla ölçülen **taktik karar kalitesi ve mekanik uygulama kalitesidir**. Bu kaliteye **adalet sınırları** içinde ulaşılmalıdır.

## 2. Kapsam

| Kapsam içi (temel sürüm) | Kapsam dışı |
|---|---|
| Warrior, priest, mage (6 rol profili, [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md)) | Rogue, archer (assassin dahil) |
| Level 80, master sınıflar | Diğer seviyeler |
| Karus ve El Morad | — |
| Ronark Land (zone 71), test arenası A | Diğer zone'lar, savaş etkinlikleri, Ardream |
| Solo PK, party PK, kontrollü 8v8 | Klan savaşları, kale kuşatması |
| Referans ekipman setleri S0–S2 | Set item'ları, transform, unique silah çeşitliliği |
| L0 baseline; L1 öğrenme temel sonrası (F9) | L3 RL (araştırma seçeneği) |
| Özel test sunucusu | Canlı/halka açık sunucu operasyonu |
| Pot stoku senaryoda sabit | Zone dışına ikmal yolculuğu |

## 3. Kısıtlar

| Kimlik | Kısıt |
|---|---|
| CON-01 | Botlar oyuncuların tabi olmadığı avantajlara sahip olamaz: saldırı hızı, sınırsız kaynak, duvar arkasını görme, gelecekteki aksiyonları bilme |
| CON-02 | Botlar ortak mekaniği yeniden ve farklı uygulamaz; sunucunun mevcut handler'larını kullanır |
| CON-03 | Normal PK sırasında gizli teleport yok; test kurtarma araçları ayrı ve ölçümlerde işaretli |
| CON-04 | Depo GPLv3; istemci ve veri tescilli olabilir ([18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md) R-11) |
| CON-05 | Kişisel veri içeren DB tabloları kullanılmaz ([18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md) R-12) |

## 4. Gereksinimler

### 4.1 Araştırma ve dokümantasyon

| Kimlik | Gereksinim | Kaynak | Öncelik |
|---|---|---|---|
| REQ-RES-01 | İncelenen branch, commit ve araştırma tarihi belirtilir | K | MVP |
| REQ-RES-02 | Önemli bulgular dosya/fonksiyon/satır veya commit bağlantısıyla verilir | K | MVP |
| REQ-RES-03 | Dış mekanik bilgileri kaynaklandırılır; sürümler karıştırılmaz | K | MVP |
| REQ-RES-04 | Skill/item adı, ID'si ve değerleri uydurulmaz | K | MVP |
| REQ-RES-05 | Depoda olmayan veri ve eksikler açıkça listelenir | K | MVP |
| REQ-RES-06 | Bilgiler 5'li ayrımla etiketlenir (depo/veri, sürüm kaynağı, başka sürüm, öneri, açık) | K | MVP |
| REQ-RES-07 | Çalıştırılmayan test "çalıştı", incelenmeyen kod "incelendi" olarak sunulmaz | K | MVP |
| REQ-DOC-01 | Ayrı Markdown dokümanlar (00–20 ve ekleri), ZIP paketi | K | MVP |
| REQ-DOC-02 | Mekanik kuralların tek ana kaynağı; davranış dokümanları referans verir | K | MVP |
| REQ-DOC-03 | Davranış dokümanlarında amaç, girdiler, öncelikler, durumlar, zamanlama, karar tabloları, pseudocode, fallback, test, kabul, bağımlılık bölümleri | K | MVP |
| REQ-DOC-04 | Doğrulanmış oyun değerleri ile ayarlanabilir parametreler ayrılır | K | MVP |
| REQ-DOC-05 | Gereksinim → doküman → faz → test izlenebilirliği | K | MVP |
| REQ-DOC-06 | Türkçe; kod sembolleri özgün dilinde | K | MVP |
| REQ-DOC-07 | Devralma şablonları: durum dosyası, faz raporu, ADR, bilinen sorunlar, test kanıtı, doküman değişiklik kuralları, durum ayrımı | K | MVP |

### 4.2 Depo ve mimari

| Kimlik | Gereksinim | Kaynak | Öncelik |
|---|---|---|---|
| REQ-ARC-01 | Botların hangi katmanda temsil edileceği karşılaştırmalı değerlendirilir ve gerekçelendirilir | K | MVP |
| REQ-ARC-02 | Botlar ortak mekaniklerden yararlanır (CON-02) | K | MVP |
| REQ-ARC-03 | Depo alanları incelenir (mimari, döngü, oyuncu/NPC, AI/Game sorumlulukları, yaşam döngüsü, hareket, saldırı, skill, buff, ölüm, party, chat, stat/item/pot, zone kuralları, paketler, DB, thread, build, lisans) | K | MVP |
| REQ-NEW-01 | Bot sistemi derleme ve çalışma bayrağıyla **varsayılan kapalı**; test modu ayrı bayrak | Ö | MVP |
| REQ-NEW-02 | Bot `CUser::Update()` çağrısı ≥ 1 Hz (aksi halde buff/DoT/regen donar) | Ö | MVP |
| REQ-NEW-03 | Gerçek oyuncu bağlantıları bot slotlarını asla almaz | Ö | MVP |

### 4.3 Karakter, skill ve ekipman

| Kimlik | Gereksinim | Kaynak | Öncelik |
|---|---|---|---|
| REQ-CHR-01 | Her sınıf/alt rol için profil: rol, ırk/sınıf, stat ve skill hesabı, ön koşullar, ekipman, beklenen değerler, eşleşmeler, solo/party farkı | K | MVP |
| REQ-CHR-02 | Warrior "255 STR, kalan HP" dağılımı sürüm kurallarıyla doğrulanır | K | MVP |
| REQ-CHR-03 | Eşit bütçeli referans setler; upgrade seviyeleri; başlangıç/standart/ileri kademeler; sürümde olmayan item önerilmez | K | MVP |
| REQ-NEW-04 | Bot karakterleri DB betiğiyle oluşturulur ve stat/skill değişmezleri betikte doğrulanır (oyun içi sınıf yükseltme yok) | Ö | MVP |
| REQ-NEW-05 | Quest kapılı master skill'ler (Etc 510–523) temel profillerde kullanılmaz | Ö | MVP |
| REQ-SKL-01 | Skill kataloğu: ad/ID, ağaç/gereksinim, maliyet, hedef, menzil/alan, cast/cooldown/kilitler, hareket, kesilme, süre/çakışma, öncelik, kaçınma, fallback, kaynak/doğrulama | K | MVP |
| REQ-SKL-02 | Warrior tekrarlanabilir saldırı skill'leri ve baskı döngüsü araştırılır | K | MVP |
| REQ-SKL-03 | Skill + R analizi: ilişki, sunucu zamanlaması, animasyon/kabul farkı, kurala uygun uygulama, hız/menzil/cooldown ihlallerinin önlenmesi | K | MVP |
| REQ-NEW-06 | BuffType çakışma grupları buff/debuff planlamasında zorunlu kural | Ö | MVP |

### 4.4 Adalet

| Kimlik | Gereksinim | Kaynak | Öncelik |
|---|---|---|---|
| REQ-FAIR-01 | CON-01 | K | MVP |
| REQ-FAIR-02 | CON-03 | K | MVP |
| REQ-NEW-07 | Sunucunun uygulamadığı istemci sınırları (cast süresi, silah gecikmesi, hareket cezaları, pot zamanlayıcısı, yürünebilirlik) bot tarafında `BotFairnessGuard` ile uygulanır | Ö | MVP |
| REQ-NEW-08 | Gözlem sözleşmesi: bot yalnızca gerçek istemcinin aldığı bilgiyi kullanır | Ö | MVP |
| REQ-NEW-09 | Tüketilmeyen pot verisi (MB-01) insan ve bot için aynı kuralla kalır (K-5); bot yalnızca envanterinde olan potu kullanır ve cooldown'a uyar | Ö | MVP |

### 4.5 Sınıf davranışları

| Kimlik | Gereksinim | Kaynak |
|---|---|---|
| REQ-WAR-01 | Hedefe erişme ve menzili koruma | K |
| REQ-WAR-02 | Sürekli ve geçerli saldırı döngüsü; uygun skill + normal saldırı | K |
| REQ-WAR-03 | Ortak hedefe baskı; düşman healer'a geçiş | K |
| REQ-WAR-04 | Kendi destek karakterlerini koruma | K |
| REQ-WAR-05 | Kaçan hedefi takip etme/bırakma; aşırı ilerlememe | K |
| REQ-WAR-06 | Düşük canda geri çekilme ve dönüş | K |
| REQ-PRI-01 | Debuffer+healer ve buffer+healer rollerinin level 80 bütçesine uygunluğu doğrulanır | K |
| REQ-PRI-02 | Acil/normal heal; tek hedef/party heal kararı | K |
| REQ-PRI-03 | Buff ve AC uygulama, yenileme, eksik buff takibi | K |
| REQ-PRI-04 | Cure öncelikleri | K |
| REQ-PRI-05 | Kendini hayatta tutma; menzil, görüş, konum | K |
| REQ-PRI-06 | Diğer priest ile görev paylaşımı; çift heal, overheal, tekrarlı buff/debuff önleme | K |
| REQ-PRI-07 | MP tasarrufu ve kritik rezerv | K |
| REQ-PRI-08 | Torment, Parasite, Malice ve diğer debuff'ların taktiksel kullanımı; başarısızlık/cure sonrası karar | K |
| REQ-PRI-09 | Başarılı ve anlamlı debuff sonrası party chat'te hedef nick'iyle çağrı; botlar chat ayrıştırmasına bağımlı değil; yanlış çağrı ve spam yok | K |
| REQ-MAG-01 | Element ve skill dağılımına uygun saldırı | K |
| REQ-MAG-02 | Menzil/konum yönetimi; yakın dövüş baskısından kaçınma | K |
| REQ-MAG-03 | Alan hasarı ve kontrol etkilerinin doğru kullanımı; party'ye destek | K |
| REQ-MAG-04 | Respawn sonrası üyeyi summon; gelen üyenin toparlanıp katılması | K |
| REQ-MAG-05 | Diriltme, respawn ve summon'un karıştırılmaması; güvensiz summon önleme koşulları | K |

### 4.6 Takım

| Kimlik | Gereksinim | Kaynak |
|---|---|---|
| REQ-PTY-01 | Party oluşturma ve rol dağılımı | K |
| REQ-PTY-02 | Party'de **en fazla iki priest** | K |
| REQ-PTY-03 | Sekiz kişilik örnek kompozisyonlar | K |
| REQ-PTY-04 | Lider, hedef çağırıcı, lider değişimi | K |
| REQ-PTY-05 | Ortak hedef seçimi; erişilebilirlik ve fiilen katılabilen üye sayısı | K |
| REQ-PTY-06 | Debuff ile saldırının zamanlanması | K |
| REQ-PTY-07 | Priest ve mage koruması | K |
| REQ-PTY-08 | Ayrılan üyeleri toplama; geri çekilme, regroup, yeniden giriş | K |
| REQ-PTY-09 | Ölüm, respawn, summon ve takıma dönüş; tam yenilgi sonrası toparlanma | K |
| REQ-PTY-10 | Birden fazla düşman party ve üçüncü taraf | K |
| REQ-PTY-11 | Heal ile ayakta tutulan hedef problemi (hasar/heal analizi, healer baskısı, sürekli hedef değiştirmeme) | K |
| REQ-PTY-12 | Hedef skoru, bağlılık süresi, değişim maliyeti, acil override; gözlemlenebilir bilgi | K |
| REQ-NEW-10 | Takım bilgi paylaşımı (`TeamBlackboard`) yalnızca en az bir üyenin gözlediği bilgiyle ve gecikmeli | Ö |

### 4.7 Solo, hayatta kalma, pot

| Kimlik | Gereksinim | Kaynak |
|---|---|---|
| REQ-SOLO-01 | Solo mantık party'den ayrı: girme/kaçınma, eşleşme değerlendirmesi, mesafe/görüş, skill/pot/cooldown, rakibe uyum, takip sınırı, sayısal dezavantaj, geri çekilme/dönüş, destek build sınırları | K |
| REQ-SOLO-02 | Her build'in her eşleşmeyi kazanması hedeflenmez; doğru karar hedeflenir | K |
| REQ-SUR-01 | HP %30 altında güvenli geri çekilme; yeterince toparlanınca dönüş | K |
| REQ-SUR-02 | Histerezis; düşman baskısı, gelen hasar, heal desteği, debuff, kaçış yolu, rol dikkate alınır | K |
| REQ-SUR-03 | Sıkışma fallback'i; geri çekilirken pot/skill/hareket ilişkisi; party'nin dağılmaması | K |
| REQ-POT-01 | 720 HP ve 1920 MP potlarının sürüm karşılıkları doğrulanır | K |
| REQ-POT-02 | Eksik miktar, verim, tüketim hızı ve yaklaşan ihtiyaca göre pot; MP için %40 beklenmez; küçük eksikte pot harcanmaz | K |
| REQ-POT-03 | HP/MP ortak/ayrı cooldown doğrulanır; öncelik kuralı; skill–pot–MP rezervi ilişkisi | K |
| REQ-POT-04 | Envanter, pot bitmesi ve ikmal politikası | K |
| REQ-POT-05 | Eşikler başlangıç hipotezi; telemetriyle ayar yöntemi | K |

### 4.8 Navigasyon

| Kimlik | Gereksinim | Kaynak |
|---|---|---|
| REQ-NAV-01 | Duvar, su, tümsek, dar geçit, yükseklik farkında sürekli takılmama | K |
| REQ-NAV-02 | Pathfinding/collision olanakları, yürünebilir alan verisi, navmesh/waypoint/hibrit değerlendirmesi | K |
| REQ-NAV-03 | Su/yükseklik kuralları; görüş hattı ile yürünebilirliğin ayrılması | K |
| REQ-NAV-04 | Hareketli hedefe yaklaşma, yeniden rota; yığılmanın önlenmesi | K |
| REQ-NAV-05 | Takılma tespiti ve aşamalı kurtarma; ulaşılamayan hedefi bırakma; güvenli geri çekilme konumu | K |
| REQ-NEW-11 | Botlar yürünebilirliği kendileri uygular (sunucu kontrol etmiyor) | Ö |

### 4.9 Öğrenme

| Kimlik | Gereksinim | Kaynak |
|---|---|---|
| REQ-LRN-01 | Yaklaşımlar karşılaştırılır; uygulanabilir başlangıç yaklaşımı önerilir | K |
| REQ-LRN-02 | Ne öğreniliyor, gözlemler, değişebilir parametreler, değişmez kurallar, deneyim düzeyi, ödül, destek katkısı | K |
| REQ-LRN-03 | Yanlış öğrenmenin önlenmesi; baseline karşısında değerlendirme; veri ayrımı; geri alma; sürüm ve yeniden üretilebilirlik; genelleme | K |

### 4.10 Test ve ölçüm

| Kimlik | Gereksinim | Kaynak |
|---|---|---|
| REQ-TST-01 | Ronark Land içinde canavarsız test bölgesi; Karus kapısı önü incelenir; gate/guard/güvenli bölge/respawn/arazi araştırılır; doğrulanmamış koordinat verilmez; gerekirse izole test modu | K |
| REQ-TST-02 | Test matrisi (kullanıcı listesi, [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md) §4.8) | K |
| REQ-TST-03 | Kendine karşı oyun tek kanıt değil; baseline'lar, taktik profilleri, insan değerlendirmesi | K |
| REQ-MET-01 | Ölçülebilir metrikler | K |
| REQ-MET-02 | Açıklanabilir karar logu | K |
| REQ-MET-03 | Seed, tekrar, taraf değiştirme, ekipman dengesi, istatistiksel belirsizlik | K |
| REQ-NEW-12 | Tek arena (A) üzerinde taraf değişimi ve ulus bazlı raporlamayla ulus asimetrilerinin dengelenmesi (K-6) | Ö |
| REQ-NEW-13 | F1'de gerçek istemci zamanlamasını ölçmek için debug paket izleyicisi | Ö |

### 4.11 Yol haritası

| Kimlik | Gereksinim | Kaynak |
|---|---|---|
| REQ-PLN-01 | Bağımlılık sırasına göre küçük, doğrulanabilir fazlar; her faz için amaç…kanıt alanları | K |
| REQ-PLN-02 | Deterministik ve ölçülebilir temel önce; koordinasyon ve öğrenme sonra | K |
| REQ-PLN-03 | Kod yazılmış olması fazın tamamlanması sayılmaz | K |
| REQ-PLN-04 | Temel sürüm ve sonraki geliştirmeler ayrılır; kapsam büyümesi sınırlanır | K |
| REQ-NEW-14 | ~~Botların ranking/ödül/duyurulardan hariç tutulması~~ — KALDIRILDI (K-9: botlar normal oyuncu gibi dahil) | Ö |
| REQ-NEW-15 | Test DB'si paylaşılmadan önce kişisel veri tabloları temizlenir | Ö |

## 5. Önceliklendirme

- **MVP** (F0–F8): REQ-RES, REQ-DOC, REQ-ARC, REQ-CHR, REQ-SKL, REQ-FAIR, REQ-WAR, REQ-PRI, REQ-MAG, REQ-PTY, REQ-SOLO, REQ-SUR, REQ-POT, REQ-NAV, REQ-TST, REQ-MET, REQ-PLN ve REQ-NEW-01..15.
- **Sonraki** (F9+): REQ-LRN'nin uygulaması. Tasarımı MVP dokümanlarında yapılmıştır ([14](14_LEARNING_AND_ADAPTATION.md)).

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
