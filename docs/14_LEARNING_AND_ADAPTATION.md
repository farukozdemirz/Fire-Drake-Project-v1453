# 14 — Öğrenme ve Adaptasyon

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027` (ko4life-net/Fire-Drake-Project-v1453 `main`)
> Bu doküman **tasarım önerisidir** (etiket `[Ö]`). Mekanik kurallara dair her ifade için tek kaynak [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)'tür. Mimari bileşen adları [13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md)'te, metrik kimlikleri [16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md)'da, senaryo kimlikleri [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)'te tanımlıdır.

---

## 1. Amaç ve "öğrenme" kelimesinin bu projedeki anlamı

Bu projede "bot öğreniyor" ifadesi **yalnızca** şu anlamda kullanılır:

> Botun karar politikası, sınırlı ve önceden tanımlanmış bir **parametre/seçenek kümesi** üzerinde, kayıtlı savaş deneyiminden üretilen kanıta göre güncellenir. Güncellenmiş politika, öğrenmede kullanılmamış **sabit bir değerlendirme setinde** sabit baseline'dan istatistiksel olarak daha iyi sonuç verirse kabul edilir.

Bu tanımın dışında kalanlar:

- Oyun mekaniğini, cooldown'ları, menzilleri, hasar formüllerini, saldırı hızını değiştiren her şey (bunlar [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)'teki sunucu kurallarıdır ve botun öğrenme alanına girmez).
- Oyuncuların erişemediği bilgiyi kullanan her şey (bkz. §5.2 gözlem sözleşmesi).
- Canlı ortamda denetimsiz politika değişikliği (bkz. §11).
- Belirli bir rakip karakteri veya oyuncuyu ezberleyen "kişiye özel" modeller (bkz. §13).

## 2. Yaklaşımların karşılaştırması

Değerlendirme ölçütleri: veri ihtiyacı, açıklanabilirlik, güvenlik (kural dışına çıkma riski), C++ GameServer içine uygulama maliyeti, deterministik test edilebilirlik ve tek maçın uzun sürdüğü bir ortamda örnek verimliliği.

| Yaklaşım | Ne öğrenir / nasıl karar verir | Veri ihtiyacı | Açıklanabilirlik | Güvenlik | Uygulama maliyeti | Bu projede rolü |
|---|---|---|---|---|---|---|
| Kural tabanlı (if/else, öncelik listesi) | Hiç öğrenmez; sabit öncelikler | Yok | Çok yüksek | Çok yüksek | Düşük | **L0 baseline**'ın acil durum katmanı (ör. kritik heal, ölüm önleme) |
| Davranış ağacı (BT) | Öğrenmez; görev yapısı ve fallback düzeni | Yok | Yüksek | Yüksek | Orta | Durum/görev akışını yapılandırmak için kullanılabilir; zorunlu değil |
| Utility scoring | Öğrenmez ama **ağırlık ve eğri parametreleri** dışarıdan ayarlanabilir | Yok (ayar için telemetri) | Yüksek (her seçeneğin skoru loglanır) | Yüksek | Orta | **L0 karar çekirdeği**; L1/L2'nin üzerinde çalıştığı parametre yüzeyi |
| İstatistiksel adaptasyon (oturum içi) | Rakibin gözlenen davranışından kısa ömürlü tahmin (ör. düşman healer'ın son 10 sn heal hızı) | Oturum içi | Yüksek | Yüksek | Düşük | L0'ın parçası; "öğrenme" değil, **durum kestirimi** sayılır |
| Offline parametre optimizasyonu (random search, CMA-ES, Bayesian opt.) | Utility ağırlıkları, eşikler, histerezis aralıkları | Orta (yüzlerce senaryo tekrarı) | Yüksek (parametre farkı okunabilir) | Yüksek (aralık sınırlı) | Orta | **L1**: ilk gerçek öğrenme katmanı |
| Contextual bandit (ör. LinUCB, Thompson sampling) | Az sayıda ayrık karar için bağlama göre seçenek | Orta | Orta-yüksek | Orta (keşif kontrol edilmeli) | Orta | **L2**: hedef değiştir/kal, geri çekilme bandı, solo savaşa gir/girme |
| Reinforcement learning (PPO, self-play) | Uçtan uca politika | Çok yüksek (milyonlarca adım) | Düşük | Düşük (ödül sömürüsü riski) | Çok yüksek (simülatör + eğitim altyapısı) | **Temel kapsam dışı**; L3 araştırma seçeneği |
| Taklit öğrenme (insan oyuncu kayıtlarından) | İnsan aksiyon dağılımı | Yüksek (etiketli insan verisi) | Düşük-orta | Orta | Yüksek | Kapsam dışı; insan değerlendirme verisi ileride ek girdi olabilir |

Neden RL ile başlanmıyor:

1. Sunucu gerçek zamanlı çalışıyor; hızlandırılmış ya da paralel simülasyon için ayrı bir simülatör gerekir ve bu simülatör [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)'teki kuralların ikinci bir kopyası olur. Kullanıcı gereksinimi botların **ortak mekaniği yeniden uygulamamasıdır**.
2. 8 vs 8 maçlar dakikalar sürer; RL'in ihtiyaç duyduğu örnek sayısına gerçek sunucu üzerinde ulaşmak pratik değildir.
3. RL politikası açıklanamaz; "neden bu hedefi seçti" sorusuna [16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md)'daki karar logu ile cevap verilemez.
4. Ödül sömürüsü (reward hacking) riski yüksektir: kaçarak ölmemek, maçı uzatmak, rakibin bir hatasını tekrar tekrar kullanmak.

## 3. Önerilen katmanlı yol

| Katman | İçerik | Ön koşul | Faz ([17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md)) |
|---|---|---|---|
| **L0 — Deterministik baseline** | Kural + utility scoring, sabit parametre dosyası `policy/baseline-v1.json` | Mekanik entegrasyon fazları (F4–F5) kabul edilmiş | F6–F7 (geliştirme), F8 (dondurma) |
| **L0.5 — Oturum içi kestirim** | Kısa pencereli istatistikler: hedefe gelen heal hızı, düşman burst tahmini, cure gecikmesi | Telemetri (F3) | F6–F7 |
| **L1 — Offline parametre optimizasyonu** | Rol profili başına sınırlı parametre vektörü; aday politikaların sabit değerlendirme setinde baseline'a karşı test edilmesi | Değerlendirme harness'i (F8) | F9 |
| **L2 — Kısıtlı contextual bandit** | En fazla 3–4 ayrık karar noktasında bağlama göre seçim; keşif yalnızca eğitim arenasında | L1 kabul edilmiş ve kararlı | F10 (opsiyonel) |
| **L3 — RL / taklit öğrenme** | Araştırma | Ayrı simülatör kararı (ADR) | Temel kapsam dışı |

**Kural:** Bir katman, altındaki katmanın kabul kriterleri karşılanmadan başlatılmaz. Kod yazılmış olması katmanın kabul edildiği anlamına gelmez (bkz. [17](17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md) durum tanımları).

## 4. Ne öğreniliyor? (değişebilir parametre yüzeyi)

Aşağıdaki liste **kapalı listedir**. Listeye ekleme bir teknik karar kaydı (ADR) gerektirir. Her parametrenin varsayılanı ilgili davranış dokümanında tanımlanır; burada yalnızca öğrenmeye açık olup olmadığı ve izin verilen aralık belirtilir. Aralıklar başlangıç hipotezidir.

### 4.1 Ortak (tüm roller)

| Parametre kimliği | Tanım yeri | Öğrenmeye açık mı | İzinli aralık (başlangıç) | Not |
|---|---|---|---|---|
| `P-SUR-RETREAT-HP` | [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md) | Evet (L1) | %20–%40 | Kullanıcı gereksinimi: temel davranış %30 altında geri çekilme. Öğrenme yalnızca bağlama göre bu bant içinde kaydırabilir. |
| `P-SUR-REENTER-HP` | [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md) | Evet (L1) | `RETREAT-HP + 25` ile %90 arası | Histerezis farkı en az 25 puan korunur. |
| `P-SUR-THREAT-WEIGHT` | [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md) | Evet (L1) | 0.0–2.0 | Yaklaşan hasar tahmininin geri çekilme skoruna etkisi |
| `P-POT-HP-DEFICIT-MIN` | [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md) | Evet (L1) | Pot değerinin %60–%110'u | "Eksik miktar pot değerini ne kadar karşılıyorsa iç" eşiği |
| `P-POT-MP-DEFICIT-MIN` | [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md) | Evet (L1) | Pot değerinin %60–%110'u | |
| `P-WAR-CHASE-MAX-DIST/TIME`, `P-SOLO-CHASE-MAX` | [06](06_WARRIOR_BEHAVIOR.md), [10](10_SOLO_PK_BEHAVIOR.md) | Evet (L1) | 06/10 tablolarında | Takip mesafesi/süresi sınırı |
| `P-TGT-*` ağırlıkları | [09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) | Evet (L1) | Ağırlık başına 0.0–3.0 | Hedef skoru bileşenleri |
| `P-TGT-COMMIT-MIN` | [09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) | Evet (L1) | 2–8 sn | Hedefe bağlı kalma süresi |
| `P-TGT-SWITCH-MARGIN` | [09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) | Evet (L1) | +%10 – +%60 | Yeni hedefin skoru mevcut hedefi bu kadar geçmeli |

### 4.2 Rol bazlı

| Rol | Öğrenmeye açık parametreler (tanım yeri) |
|---|---|
| Warrior | Skill rotasyon önceliği içinde **eşit geçerli** seçenekler arasındaki sıra; peel (koruma) tetik eşiği; healer'a geçiş eğilimi ([06](06_WARRIOR_BEHAVIOR.md)) |
| Priest (healer+debuffer / healer+buffer) | Heal tetik eşikleri (acil/normal), tahmini gelen hasar için ön-heal eğilimi, debuff hedef seçim ağırlıkları, MP rezerv oranı ([07](07_PRIEST_BEHAVIOR.md)) |
| Mage | Tercih edilen menzil bandı, alan hasarı için minimum hedef sayısı, summon zamanlama eşikleri ([08](08_MAGE_BEHAVIOR.md)) |
| Solo profilleri | Savaşa girme skoru eşiği, takip sınırı, sayısal dezavantaj toleransı ([10](10_SOLO_PK_BEHAVIOR.md)) |

### 4.3 Kesinlikle değiştirilemeyenler (değişmez kurallar)

Aşağıdakiler kod düzeyinde **sabit** tutulur; politika dosyasında bulunmaz ve öğrenme kodunun erişebileceği bir yüzeyde yer almaz:

1. **Sunucu mekaniği:** saldırı/skill kabul kuralları, cooldown, cast süresi, menzil, MP/HP/item maliyeti, pot cooldown'u ([03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)). Bot aksiyonları gerçek oyuncu paketlerinin geçtiği aynı handler'lardan geçer ([13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md)).
2. **Hız tavanı:** Bot, sunucu bir kontrolü uygulamıyor olsa bile [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)'teki "insan oyuncunun istemcisiyle ulaşabileceği" zamanlama tavanını aşamaz (ör. normal saldırı aralığı, skill animasyon kilidi). Bu tavanlar `BotFairnessGuard` tarafından uygulanır.
3. **Bilgi sınırı:** §5.2'deki gözlem sözleşmesi.
4. **Hareket bütünlüğü:** PK sırasında gizli teleport yok ([12](12_NAVIGATION_AND_POSITIONING.md)). Test kurtarma teleportları yalnızca test modunda ve telemetride işaretli.
5. **Chat sınırı:** Party chat mesaj oranı limiti ve "başarısız debuff sonrası çağrı yok" kuralı ([09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md)).
6. **Kompozisyon kuralı:** Bir party'de en fazla iki priest ([01](01_PRODUCT_SCOPE_AND_REQUIREMENTS.md)).
7. **Güvenlik kuralları:** Ölüm sonrası summon'un güvenli konum koşulları ([08](08_MAGE_BEHAVIOR.md)), acil heal önceliği ([07](07_PRIEST_BEHAVIOR.md)) gibi "asla devre dışı bırakılamaz" kurallar.

## 5. Hangi gözlemler kullanılıyor?

### 5.1 Özellik (feature) grupları

| Grup | Örnek özellikler | Kaynak |
|---|---|---|
| Öz durum | HP/MP oranı ve mutlak eksik, aktif buff/debuff listesi ve kalan süreleri, cooldown'lar, pot stoku, pot cooldown'u | Kendi `CUser` durumu |
| Hedef durumu | Mesafe, görünür sınıf/ırk/ekipman, (gözlemlenebilirse) HP oranı, son N saniyede aldığı hasar ve heal tahmini | Gözlem sözleşmesi |
| Takım durumu | Party üyelerinin HP/MP oranları, konumları, ölü/canlı, ortak hedef, rol | Party paketi ve `TeamBlackboard` |
| Düşman takımı | Görünen düşman sayısı, sınıf dağılımı, healer sayısı, düşmanların ortak hedefe yönelimi | Bölgedeki görünür birimler |
| Arazi | Hedefe yürünebilir yol uzunluğu, görüş hattı (varsa), kaçış yönü güvenliği | [12](12_NAVIGATION_AND_POSITIONING.md) |
| Zaman | Savaş süresi, ölüm/respawn zamanları | Sunucu zamanı |

### 5.2 Gözlem sözleşmesi (bilgi adaleti)

Bot, bir insan oyuncunun aynı konumda **istemcisinin aldığı paketlerden** elde edebileceğinden fazlasını kullanmaz. Uygulama kuralı: `Perception` bileşeni sunucu nesnelerine doğrudan erişse bile ([13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md)), dışarı verdiği gözlem kaydı yalnızca sözleşmedeki alanları içerir. Sözleşmenin alan alan doğrulanması (hangi bilginin hangi paketle istemciye gittiği) [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §"Gözlemlenebilirlik" bölümündedir. Doğrulanmamış alanlar (ör. düşman HP'sinin istemciye ne zaman gönderildiği) doğrulanana kadar **kullanılmaz** veya yalnızca botun kendi verdiği hasarla tahmin edilir. *(Güncelleme 2026-10-02: düşman/hedef HP'si `docs/03` §16'da `[D]` olarak doğrulandı: yalnızca hasar verilen veya seçili hedef için `WIZ_TARGET_HP` paketiyle gelir ve yaşıyla birlikte kullanılır; oyuncu adı `WIZ_USER_INOUT` kaydıyla gelir. Düşman MP'si, cooldown'ı, envanteri ve pot stoku yasak kalır. Kaynak sınıfları ve tazelik: `docs/13` §5.2a.)*

Yasak örnekleri: düşmanın tam MP'si, cooldown durumu, envanteri, pot stoku, görüş alanı dışındaki konumu, düşman party'sinin iç planı, gelecekteki aksiyonu.

## 6. Deneyim hangi düzeyde tutuluyor?

| Düzey | Tutulan | Gerekçe |
|---|---|---|
| **Rol profili** (`warrior.pressure`, `warrior.guard`, `priest.heal_debuff`, `priest.heal_buff`, `mage.fire_burst`, `mage.ice_control`; [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) §5) | Politika parametre vektörü (L1), bandit modeli (L2) | Ana öğrenme birimi. Aynı rol profilini oynayan tüm botlar aynı politika sürümünü kullanır. Bu, veri miktarını artırır ve karakter adına ezberi önler. |
| **Takım/kompozisyon** | Koordinasyon parametreleri (hedef değişim marjı, regroup eşiği) | Takım düzeyindeki kararlar tek bir botun deneyimiyle açıklanamaz. |
| **Global** | Ortak eşikler (pot bandı gibi), arazi istatistikleri (takılma noktaları ısı haritası) | Rol bağımsız. |
| **Karakter** | Yalnızca telemetri ve teşhis | Politika karakter düzeyinde **tutulmaz**. Karakter düzeyi öğrenme belirli rakiplere aşırı uyuma yol açar. |

Ekipman ve stat dağılımı farklı karakterler aynı rol profilini paylaşıyorsa politika girdisine ekipman sınıfı (ör. `gear_tier`) özellik olarak eklenir; ayrı politika açılmaz.

### 6.1 Hedefle uyum: "oynadıkça gelişen bot" ne demektir (ADR-0030-DEG, değerlendirme 2026-10-02)

Proje hedefi "deneyimlerinden gelişen botlar"dır. Bu projede bu ifade **üç ayrı düzeyde** ve ayrı ölçütlerle karşılanır; hangisinin hedeflendiği açıktır:

| Düzey | Ne gelişir | Nasıl | Kalıcılık | Faz | Ölçüt |
|---|---|---|---|---|---|
| Oturum içi (L0.5) | Tek botun/takımın rakip kestirimi (rakip heal hızı, burst, hedefin heal'i) | `EnemyIntel`, kısa pencere istatistikleri | Maç bitince silinir | F6–F7 | Öğrenme **değil**, durum kestirimi; MET-STALL-01 |
| Rol profili (L1/L2) | Aynı rolü oynayan **tüm** botların ortak politikası | Çevrim dışı arama (L1), kısıtlı bandit (L2); `PolicyStore` sürümü | Sürümlü politika dosyası | F9/F10 | AC-LRN-01, AC-LRN-08 |
| Karakter | — | **Planlı değil**: karakter başına kalıcı politika yok; yalnızca telemetri/teşhis | — | F10 sonrası, ayrı ADR | — |

Sonuç: "her bot oynadıkça ustalaşır" beklentisi **bireysel maç geçmişinden değil, filonun toplam deneyiminden** (aynı rolü oynayan tüm botlar aynı politikayı paylaşır) karşılanır. Gerekçe: veri miktarı, rakibe aşırı uyum riski (§6), tekrarlanabilirlik ve açıklanabilirlik. Karakter bazında kalıcı öğrenme istenirse ADR-0030-DEG'deki seçenek B (rol politikasına çekilmiş, sınırlı karakter sapması) ayrı onayla açılır.

## 7. Ödül fonksiyonu

### 7.1 Yapı

Ödül iki parçalıdır ve parçalar ayrı loglanır:

```
R_episode = w_team * R_team + w_role * R_role - P_violation
```

- `R_team` (takım sonucu): öldürülen düşman sayısı − kaybedilen üye sayısı (ağırlıklı), senaryo hedefi (ör. alan kontrolü) varsa onun sonucu. Tüm takım üyelerine **aynı** değer verilir. Bu, "kill çalma" teşvikini ortadan kaldırır.
- `R_role` (rol katkısı): rol bazlı, normalize edilmiş ve **üst sınırlı** katkı ölçüleri (§7.2).
- `P_violation`: geçersiz aksiyon denemeleri, chat spam limiti aşımı, takip sınırı ihlali, pozisyon kuralı ihlali gibi kural ihlali cezaları.

Başlangıç ağırlıkları: `w_team = 0.7`, `w_role = 0.3`. Rol katkısının takım sonucunu geçememesi bilinçli bir tercihtir.

### 7.2 Rol katkısı (destek rolleri dahil)

| Rol | Katkı ölçüsü | Tanım ([16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md) metrikleri) |
|---|---|---|
| Warrior | Ortak hedefe verilen etkili hasar oranı, hedefe temas süresi, düşmanın kendi destek karakterine temas süresinin azaltılması | MET-DMG-01, MET-TGT-02, MET-PEEL-01 |
| Priest (heal) | **Etkili heal** (overheal hariç), **kurtarılan kritik durum** (müttefik HP < %25 → 5 sn içinde ölmeden %50 üstüne çıkması), overheal oranı (ceza) | MET-HEAL-01..03 |
| Priest (buff) | Takımın buff kapsama oranı (savaş süresince gerekli buff'ların aktif olduğu süre / toplam süre), buff yenileme gecikmesi | MET-BUFF-01..02 |
| Priest (debuff) | Başarılı debuff sonrası T saniye içinde hedefe takım hasarı/kill (debuff "dönüşüm oranı"), başarısız debuff sayısı (ceza değil, verim payında) | MET-DEBUFF-01..03 |
| Cure | Kritik debuff'ın cure gecikmesi | MET-CURE-01 |
| Mage | Ortak hedefe hasar, alan hasarı verimi (vurulan hedef sayısı / alan skill sayısı), summon sonrası savaşa dönüş süresi | MET-DMG-01, MET-AOE-01, MET-SUM-01 |

Destek katkısının ölçümü ancak **karşı-olgusal bir yaklaşım** ile adil olur: "Bu heal olmasaydı müttefik ölür müydü?" sorusuna yaklaşık cevap için, heal anındaki müttefik HP'si ve sonraki 3 sn içindeki gelen hasar toplamı kaydedilir. `heal_saved = min(heal_amount, max(0, incoming_3s - hp_before + 1))`. Bu ölçü [16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md)'da MET-HEAL-03 olarak tanımlanır.

### 7.3 Yanlış öğrenmeyi önleme

| Risk | Belirti | Önlem |
|---|---|---|
| Gereksiz kaçış (ölmemek için savaştan uzak durma) | Savaş dışı süre artarken takım sonucu kötüleşiyor | `R_team` baskın; "savaş dışında geçen süre" metriği guard metrik olarak izlenir (MET-SUR-04); geri çekilme yalnızca §4.1 aralığında öğrenilebilir |
| Yalnızca kill kovalamak | Bireysel kill artarken ortak hedefe katılım düşüyor | Ödülde bireysel kill **yoktur**; takım sonucu ortak |
| Maçı uzatmak | Beraberlik/zaman aşımı oranı artıyor | Zaman aşımı yenilgi gibi sayılır (`R_team` negatif sabit); maç süresi guard metrik |
| Rakibin açığını sömürmek | Tek bir rakip profiline karşı ani iyileşme, diğerlerinde düşüş | Rakip havuzu çeşitliliği (§13), değerlendirme setinde görülmemiş profiller, profil bazlı sonuçların ayrı raporlanması |
| Mekanik hatasını sömürmek | Sunucudaki bir doğrulama eksikliğinden yararlanan aksiyon dizisi | Değişmez kurallar (§4.3) aksiyon yüzeyini sınırlar; telemetride "fairness guard reddi" sayacı (MET-FAIR-01) sıfır olmalı |
| Overheal ile heal metriği şişirme | Heal miktarı yüksek, kurtarılan kritik durum düşük | Ödül etkili heal ve `heal_saved` üzerinden; overheal ceza |
| Debuff spam | Debuff sayısı yüksek, dönüşüm oranı düşük | Ödül dönüşüm oranına bağlı; chat çağrısı yalnızca başarılı debuff'ta |

## 8. Yeni politikanın baseline karşısında değerlendirilmesi

1. **Sabit baseline:** `baseline-v1` dondurulur; hiçbir zaman yerinde güncellenmez. Yeni baseline ancak ADR ile ve eski baseline'a karşı kazanarak ilan edilir.
2. **Eşleştirme protokolü:** Her değerlendirme maçı ikiz olarak oynanır: aday A tarafı/baseline B tarafı, ardından **taraf değiştirilerek** (Karus ↔ El Morad ve başlangıç noktası) aynı seed ile tekrar. Ekipman setleri [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md)'teki eşit bütçeli referans setlerdir.
3. **İstatistik:** Kazanma oranı için Wilson güven aralığı ve ardışık test (SPRT; satranç motoru testlerinde kullanılan yöntem, bkz. [19](19_SOURCES_AND_EVIDENCE.md)), sürekli metrikler (etkili heal, hedefe katılım oranı vb.) için bootstrap güven aralığı. Başlangıç kabul ölçütü: SPRT ile H0: Elo farkı ≤ 0, H1: Elo farkı ≥ 10, α = β = 0.05.
4. **Guard metrikler:** Aday, kazanma oranı iyileşse bile aşağıdakilerden birinde baseline'ın güven aralığının altına düşerse reddedilir: geçersiz aksiyon oranı, takılma oranı, chat ihlali, fairness guard reddi, ortalama tick maliyeti.
5. **Profil bazlı rapor:** Sonuçlar rakip profili ve senaryo bazında ayrı raporlanır. Tek bir senaryoda büyük kazanç ve diğerlerinde kayıp kabul edilmez (her senaryoda "anlamlı kötüleşme yok" şartı).
6. **İnsan değerlendirmesi:** Faz kapısında en az bir oturumda insan oyuncuların botlara karşı/yanında oynayıp [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)'teki puanlama formunu doldurması.

## 9. Öğrenme verisi ile değerlendirme senaryolarının ayrılması

| Küme | İçerik | Kullanım |
|---|---|---|
| Eğitim havuzu | Senaryo şablonları × rastgele başlangıç ofsetleri × rakip profilleri (A grubu) | L1 optimizasyonu, L2 keşfi |
| Doğrulama havuzu | Eğitimle aynı dağılımdan ama ayrı seed'ler | Hiperparametre ve erken durdurma |
| **Kilitli değerlendirme seti** | [15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)'teki `EVAL-*` senaryoları, B grubu rakip profilleri (eğitimde hiç kullanılmamış), sabit seed listesi | Yalnızca kabul kararı. Bu sette elde edilen sonuç parametre ayarı için kullanılmaz. |

Kilitli setin içeriği sürüm numaralıdır (`evalset-v1`). Sette değişiklik ADR gerektirir ve önceki sonuçlarla karşılaştırma yapılmaz.

**Değerlendirme seti erişim denetimi (2026-10-02):** kilitli `evalset-v1` sonuçları ayar için okunmaz; her açılış `EVAL_ACCESS` olayıyla (kim, ne için, hangi aday) kaydedilir; bir aday sürümü için en çok bir kabul koşusu yapılır (ek koşu = yeni aday sürümü + yeni seed listesi); hiperparametre ayarı yalnızca doğrulama havuzunda yapılır. Rakip profilleri ayrıdır: A grubu (OP-AGGRO, OP-KITE, OP-RANDOM) eğitimde, B grubu (OP-TURTLE, OP-HEALER-FIRST, OP-SPREAD) yalnızca kilitli değerlendirmede; politika koşucusu eğitim modunda B kimliklerini yüklemeyi reddeder.

## 10. Deneyim kaydı ve yeniden üretilebilirlik

### 10.1 Politika dosyası

```json
{
  "policy_id": "priest.heal_debuff",
  "version": "1.3.0",
  "parent": "1.2.1",
  "layer": "L1",
  "created_utc": "2026-11-20T10:00:00Z",
  "server_commit": "<git sha>",
  "bot_commit": "<git sha>",
  "trained_on": {"pool": "trainpool-v2", "episodes": 640, "data_hash": "sha256:..."},
  "params": {"P-PRI-HEAL-EMERG": 0.32, "P-PRI-PREHEAL-K": 0.8, "P-TGT-COMMIT-MIN": 4.0},
  "evaluation": {"evalset": "evalset-v1", "vs": "baseline-v1", "report": "reports/eval-2026-11-21.md"}
}
```

Kurallar: Parametre adları [13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md)'teki parametre kayıt defterinde tanımlı olmalı; aralık dışı değer yüklemede reddedilir ve bot baseline'a döner.

### 10.2 Deneyim kaydı (episode log)

Her karar noktası [16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md)'daki karar logu biçimindedir (gözlem özeti, değerlendirilen seçenekler ve skorları, seçilen aksiyon, sonuç). Öğrenme için ek olarak episode başına: senaryo kimliği, seed, taraf, kompozisyonlar, ekipman seti kimliği, politika sürümleri, sonuç ve ödül bileşenleri.

### 10.3 Yeniden üretilebilirlik

- Her bot kendi RNG'sini senaryo seed'inden türetir (`seed_bot = hash(seed_episode, bot_slot)`); karar kodunda global `rand()` kullanılmaz.
- Sunucunun kendi rastgeleliği (isabet, hasar varyansı) ve thread zamanlaması tam deterministik değildir ([03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md), [18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md)). Bu nedenle "aynı seed → aynı sonuç" beklenmez; "aynı seed → aynı karar dağılımı ve istatistiksel olarak aynı sonuç" beklenir. Tek bir maçın tekrarı kanıt sayılmaz; değerlendirme her zaman çoklu tekrarla yapılır (§8).
- Politika + bot commit + sunucu commit + veri tabanı sürüm özeti (MAGIC/ITEM tablosu hash'i) her raporda yazılır.

## 11. Bozulan davranışın tespiti ve geri alma

1. **Canary:** Yeni politika önce eğitim arenasında 1 party'de, sonra değerlendirme setinde, en son geniş testlerde kullanılır.
2. **Otomatik guard:** Çalışma sırasında rol profili başına kayan pencere guard metrikleri izlenir (ör. 10 dakikalık pencerede geçersiz aksiyon oranı, takılma oranı, ölüm/dakika). Eşik aşımında `PolicyStore` profili bir önceki kabul edilmiş sürüme döndürür ve olay telemetriye `POLICY_ROLLBACK` olarak yazılır.
3. **Manuel geri alma:** GM/konsol komutu ile profil sürümü sabitlenebilir ([13](13_BOT_ARCHITECTURE_AND_DATA_MODEL.md)).
4. Politika dosyaları değişmez (immutable) saklanır; geri alma bir işaretçi değişikliğidir.

## 12. L2 contextual bandit tasarım taslağı (opsiyonel faz)

Yalnızca aşağıdaki karar noktaları adaydır. Her biri az sayıda koldan oluşur ve her kol L0'da zaten geçerli bir davranıştır. Bandit yalnızca **güvenli seçenekler arasında** seçim yapar.

| Karar noktası | Kollar | Bağlam özellikleri | Ödül (kısa vadeli) |
|---|---|---|---|
| Ortak hedefte kal / healer'a geç / ikinci hedefe geç | 3 | Hedefin net hasar-heal dengesi, healer'a erişim, takım HP durumu | Sonraki 15 sn içinde takım kill farkı ve hasar baskısı |
| Geri çekilme bandı | 3 (erken / normal / geç) | Gelen hasar tahmini, heal desteği, kaçış yolu | Hayatta kalma + savaşa dönüş süresi |
| Solo: savaşa gir / bekle / kaçın | 3 | Eşleşme skoru, kaynak durumu, sayısal durum | Sonraki 60 sn sonuç |
| Mage summon zamanı | 2 (şimdi / bekle) | Hedef üyenin konumu, takımın savaş durumu | Summon edilen üyenin 20 sn içinde ölmeden katkı vermesi |

Keşif yalnızca eğitim arenasında açıktır (ε-greedy için ε ≤ 0.1 veya Thompson sampling). Değerlendirme ve canlı test modlarında keşif kapalıdır.

## 13. Ezber yerine genelleme

- **Rakip havuzu:** Kural tabanlı taktik profilleri (OP-AGGRO, OP-KITE, OP-RANDOM eğitimde; OP-TURTLE, OP-HEALER-FIRST, OP-SPREAD yalnızca değerlendirmede) + B0-NAIVE + baseline + önceki politika sürümleri ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md) §5).
- **Kompozisyon çeşitliliği:** 8 vs 8 dışında 2v2, 3v3, 4v4, 5v8 gibi dengesiz durumlar.
- **Konum çeşitliliği:** Arena içinde farklı başlangıç noktaları, eğim ve dar geçit içeren alt bölgeler.
- **Değerlendirmede görülmemiş profiller** ve ayna olmayan kompozisyonlar.
- **İnsan oyuncu oturumları:** İnsanlarla yapılan maçlar politika eğitiminde kullanılmaz (veri az, dağılım farklı); kalite göstergesi olarak raporlanır.

## 14. Kabul kriterleri (öğrenme özellikleri)

| Kimlik | Kriter |
|---|---|
| AC-LRN-01 | L1 aday politikası, `evalset-v1` üzerinde baseline'a karşı SPRT ile kabul (H1: +10 Elo) ve tüm guard metriklerde anlamlı kötüleşme yok |
| AC-LRN-02 | Aynı politika ve seed listesi ile değerlendirme iki ayrı günde tekrarlandığında kazanma oranları birbirinin %95 güven aralığı içinde |
| AC-LRN-03 | Yasaklı gözlem alanlarına erişim denemesi 0 (statik kontrol + çalışma zamanı assert) |
| AC-LRN-04 | Aralık dışı parametre içeren politika dosyası yüklenemiyor ve bot baseline'a dönüyor (birim testi) |
| AC-LRN-05 | Otomatik geri alma: yapay olarak **geçerli ama kötü** politika (tüm parametreler izinli aralıkta, ör. `P-SUR-RETREAT-HP` 0,40 + `P-SUR-REENTER-HP` 0,90 + `P-SUR-THREAT-WEIGHT` 2,0) canary aşamasında 10 dk içinde geri alınıyor; geri çekilme eşiği %95 gibi **aralık dışı** değerler AC-LRN-04 ile yüklemede reddedilir ve bu teste girmez |
| AC-LRN-06 | Destek rolü katkısı: priest politikasının iyileşmesi heal metriği şişmesiyle değil, `heal_saved` ve takım sonucu ile gösteriliyor |
| AC-LRN-07 | Yükleme reddi (AC-LRN-04, birim testi: aralık dışı değer hiç yüklenmez) ile çalışma zamanı geri alma (AC-LRN-05, oyun içi: yüklenebilir ama guard metriklerini bozan politika canary'de geri alınır) **ayrı** testlerdir; biri diğerinin yerine sayılmaz |
| AC-LRN-08 | Öğrenme başarısı ayrı ölçülür: F9 çıkışı "baseline'ı SPRT ile geçen politika" **veya** "iyileşme yok" kanıtıdır (ikisi de geçerli). İkinci durumda rapor aday sayısı, arama bütçesi, aranan parametre uzayı ve güven aralığını içerir; kanıtsız "iyileşme yok" kabul edilmez. F9 tamamlanması "daha güçlü politika bulundu" anlamına gelmez |

## 15. Açık konular

Tamamı [18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md)'de izlenir. Bu dokümanı doğrudan etkileyenler:

- Düşman HP'sinin istemciye hangi koşulda gönderildiği (gözlem sözleşmesi).
- Sunucu rastgeleliğinin test modunda seed'lenip seed'lenemeyeceği.
- Hızlandırılmış zaman (time-scale) olmadan yeterli tekrar sayısına ulaşma süresi: 8 vs 8 maç başına tahmini süre ve günlük kapasite ölçülmelidir.

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
| 2026-10-02 | v1.1 | Değerlendirme: §6.1 öğrenme düzeyi netleştirildi (ADR-0030-DEG), AC-LRN-05 örneği AC-LRN-04 ile uyumlu hale getirildi, AC-LRN-07/08, §9 değerlendirme seti erişim denetimi |
