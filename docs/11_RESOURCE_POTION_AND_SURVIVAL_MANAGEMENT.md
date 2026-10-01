# 11 — Kaynak, Pot ve Hayatta Kalma Yönetimi

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Pot mekaniği: [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §6 (MEC-POT-01..05, MB-01, CLI-06). Bu doküman geri çekilme/yeniden giriş kararının, pot politikasının ve stok politikasının **tek kaynağıdır.**
> Eşikler başlangıç hipotezidir `[Ö]`; telemetri ile ayarlanır (§9).

---

## 1. Doğrulanmış temel (özet)

| Bulgu | Etiket |
|---|---|
| Potlar `WIZ_MAGIC_PROCESS` ile MAGIC satırı üzerinden kullanılır; cooldown pot skill ID'sine özgüdür → **HP ve MP potları sunucuda ayrı cooldown'dadır**; farklı kademeler de ayrıdır | `[D]` `[V]` |
| Tüm başlıca HP/MP potlarının recast'i 2,0 sn ("(Store)" HP sürümleri 2,5 sn), cast alanı 5 (0,5 sn) | `[V]` |
| **720 HP** = Water of favors (`389014000`, MAGIC 490014); **1920 MP** = Potion of soul (`389020000`/`389082000`, MAGIC 490020/490082); ikisi de seviye şartsız | `[V]` |
| Bu iki potun normal sürümleri (ve 360 HP, 960 MP) **tüketilmiyor** (`UseItem = 0`, MB-01) | `[V]` |
| Aynı saniyede aynı potun tekrarını sunucu reddetmiyor (MEC-MAG-05, MB-02) | `[D]` |
| Potlar tip kapısına girmez; skill kullanımını engellemez (ID ≥ 400000) | `[D]` |
| Pot satıcıları (SellingGroup 253) Ronark Land'de yok | `[V]` |
| Ronark'ta respawn HP'yi doldurur, **MP'yi doldurmaz**; blink yok | `[D]` |
| Dönem oyuncu ifadesi: "720'lik potu 2 sn'de çekiyoruz" | `[S]` |
| HP ve MP potlarının **istemcide** ortak zamanlayıcı kullanıp kullanmadığı | `[A]` (T-MECH-POT-03) |

## 2. Karar: tüketilmeyen potlar (K-5, ADR-0009)

**Verilen karar (2026-10-01):** Veri olduğu gibi kalır. 360/720 HP ve 960/1920 MP potlarının NPC sürümleri hem insanlar hem botlar için tüketilmez.

| Sonuç | Açıklama |
|---|---|
| Adalet | Kural insan ve bot için aynıdır; bota özel avantaj yoktur. "Sınırsız kaynak yok" gereksinimi bu potlar için **sunucu kuralı olarak kabul edilmiş** sayılır (01 REQ-NEW-09). |
| Bot kuralı (CLI-06) | Bot, envanterinde o pottan en az bir adet varsa kullanır. Gerçek istemcinin envanterde olmayan potu kullandırmadığı varsayılır `[A]` (T-MECH-POT-05). |
| Sınırlayıcı | Pratikteki tek sınır pot cooldown'udur (§3.1) ve botlar bunu uygular. |
| Tüketilen potlar | 1440 HP (Water of bless), 2160 MP (Ancient Spirit), "(Store)" sürümleri gibi tüketilen potlar normal azalır; stok politikası (§6) yalnızca bunlar için anlamlıdır. |
| Telemetri | Pot olayları yine sayılır (MET-POT-01..03). Pot verimi ve tüketim hızı bu karar altında da raporlanır. |

## 3. Pot politikası

### 3.1 Bot tarafı cooldown politikası (CLI-06)

- **HP grubu:** Tüm HP pot kademeleri için ortak 2,0 sn (Store sürümünde 2,5 sn).
- **MP grubu:** Tüm MP pot kademeleri için ortak 2,0 sn.
- HP ve MP grupları birbirinden bağımsızdır (sunucu kuralı). İstemci ortak zamanlayıcı uyguluyorsa (T-MECH-POT-03), politika "HP ve MP arası en az 2,0 sn" olarak güncellenir.
- Aynı tick'te pot ve skill gönderimi arasında en az 200 ms (insan girişi temposu, CLI-11).

### 3.2 HP potu kararı

```
hp_pred      = hp − incoming_rate_ewma · 1,0 sn − 0  (pot anlık)
deficit      = maxhp − hp_pred
heal_soon    = beklenen priest heal'i (rezervasyonlar, 07 §6) ≤ 1,5 sn içinde
best_tier    = en büyük kademe s.t. değer ≤ deficit / P-POT-HP-DEFICIT-MIN    (stokta olan)
drink_hp ⇔ hp_ready ∧ best_tier ∃ ∧ ( hp_pred/maxhp < P-POT-HP-EMERG  ∨  (deficit ≥ değer(best_tier)·P-POT-HP-DEFICIT-MIN ∧ ¬heal_soon) )
```

| Parametre | Varsayılan | Not |
|---|---|---|
| P-POT-HP-DEFICIT-MIN | 0,9 | Pot değerinin %90'ı kadar eksik yoksa içme (israf önleme) |
| P-POT-HP-EMERG | 0,35 | Bunun altında, priest heal'i beklense bile iç |
| Kademe sırası | 1440 → 720 | Büyük eksikte büyük pot |

Party'de priest destekliyken HP potu daha geç içilir (heal_soon). Solo'da daha erken içilir: `P-POT-HP-DEFICIT-MIN` solo'da 0,7.

### 3.3 MP potu kararı

Kullanıcı gereksinimi: HP güvenliyken MP'yi gereksiz yere %40'a kadar beklememek, ama küçük eksikler için de sürekli pot harcamamak.

```
mp_need_soon = sonraki 3 sn'de planlanan skill'lerin MP maliyeti + rol rezervi
drink_mp ⇔ mp_ready ∧ ( mp < mp_need_soon  ∨  (maxmp − mp) ≥ 1920·P-POT-MP-DEFICIT-MIN )
           ∧ ¬(drink_hp bu tick)
```

| Parametre | Varsayılan |
|---|---|
| P-POT-MP-DEFICIT-MIN | 0,95 (1920'lik pot ~1824 eksikte içilir) |
| Rol MP rezervi | Warrior 700 (P-WAR-MP-RESERVE), Priest 1100 (P-PRI-MP-RESERVE), Mage 500 (P-MAG-MP-RESERVE) |

Örnek: Priest maks MP ~5696. Eksik 1824'e ulaştığında (MP ≈ %68) pot içer. Yoğun heal döngüsünde 2 sn'de bir pot ile ~960 MP/sn yenilenir. Warrior Howling döngüsünde 400 MP/sn harcar; Carving'e geçiş MP'yi korur ([06](06_WARRIOR_BEHAVIOR.md) §6.2).

### 3.4 HP ve MP aynı anda gerekirse

| Durum | Öncelik |
|---|---|
| `hp_pred/maxhp < P-POT-HP-EMERG` | HP |
| Priest ve MP < acil heal maliyeti (960) ve bir müttefik acil durumda | MP (priest'in heal'i takım için daha değerli) |
| Diğer | Eksik oranı (eksik / pot değeri) büyük olan; diğeri ≥ 0,5 sn sonra (grup cooldown'ları ayrıysa) |

### 3.5 Pot ve skill ilişkisi

- Pot, skill tip kapısını tüketmez. Warrior aynı saniyede bir Type1 skill + R + pot gönderebilir (sunucu kabul eder). Bot bunu CLI-11 tavanı içinde yapar.
- Priest heal cast'i (1,5 sn) sırasında pot içmek: istemcide cast'i bozup bozmadığı `[A]`. Varsayılan: priest cast sırasında pot **içmez**.
- Pot kullanımı hareketi durdurmaz varsayımı `[A]` (T-MECH-POT-04); doğrulanana kadar bot potu durmadan içer.

## 4. Geri çekilme ve yeniden giriş

### 4.1 Temel kural (kullanıcı gereksinimi)

HP %30'un altına inen bot güvenli biçimde geri çekilir; yeterince toparlanınca savaşa döner. Bu kural iki sabit eşik değil, **histerezisli ve bağlama duyarlı** bir karardır.

### 4.2 Karar modeli

```
ttd        = hp / max(ε, incoming_rate_ewma(1,5 sn))              # ölüme kalan süre tahmini
support    = 1 if (priest canlı ∧ menzilde ∧ rezervasyonu bize ya da serbest ∧ MP ≥ 960) else 0
threat     = görünür düşman melee sayısı (≤ 8 m) + 0,5·düşman mage sayısı (≤ 45 m, bize yönelik olay)
retreat_hp = P-SUR-RETREAT-HP + role_adj + 0,05·threat − 0,08·support + 0,1·[kök/yavaşlatma var]
RETREAT ⇔ (hp/maxhp < retreat_hp ∧ ¬last_stand)  ∨  (ttd < 2,5 sn ∧ ¬support ∧ escape_ok)
REENTER ⇔ hp/maxhp ≥ P-SUR-REENTER-HP ∧ mp ≥ rol_min ∧ (party modu ENGAGE ∨ solo EV ≥ eşik)
```

| Parametre | Varsayılan | Not |
|---|---|---|
| P-SUR-RETREAT-HP | 0,30 | Kullanıcı gereksinimi |
| role_adj | Mage +0,10; Priest +0,05; W-G −0,05; W-P 0 | Düşük HP'li roller erken çekilir |
| P-SUR-REENTER-HP | 0,65 (≥ RETREAT-HP + 0,25) | Histerezis |
| P-SUR-THREAT-WEIGHT | 1,0 (formüldeki 0,05 ve 0,5 katsayılarının çarpanı) | Öğrenmeye açık |
| P-SUR-MIN-RETREAT-TIME | 3 sn | Geri çekilmeye girince en az bu kadar kalır (salınım önleme, MET-SUR-06 = 0) |

### 4.3 Geri çekilme hareketi

| Rol | Hareket | Destek aksiyonları |
|---|---|---|
| Party üyesi | Takım arka hattına / priest'e doğru, düşman kümesinden uzaklaşan yol ([12](12_NAVIGATION_AND_POSITIONING.md) §8). **Takımın tersine kaçmaz.** | HP potu; warrior sprint, restoration; mage Mana Shield, takipçiye yavaşlatma; priest kendine heal (takipçi uzaktaysa dur-cast) |
| Solo | Kendi tower halkasına doğru veya düşmansız bölgeye | Aynı |

Geri çekilme mesafesi party modunda ön hattan en fazla 40 m'dir. Daha uzağa çekilme yalnızca takım RETREAT modundaysa yapılır ([09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) §10).

### 4.4 Sıkışma fallback'i (son direniş)

`last_stand` şu durumlarda doğrudur: kaçış yolu yok (navigasyon güvenli nokta bulamadı), kök/stun altında ve cure 2 sn içinde beklenmiyor, ya da `ttd` < kaçış süresi tahmini. Bu durumda bot geri çekilmez:

- Pot ve savunma skill'leri (Mana Shield, restoration, Elysian Web) kullanılır.
- En düşük HP'li ulaşılabilir düşmana tam saldırı yapılır.
- `TeamBlackboard`'a acil durum bildirilir (priest acil heal, W-G peel).

## 5. Ölüm sonrası kaynak

- Respawn MP'yi doldurmaz. Bot `RECOVER` durumunda MP'yi rol eşiğine (warrior %50, priest %70, mage %70) potla tamamlar, sonra `READY_FOR_SUMMON` der ([08](08_MAGE_BEHAVIOR.md) §8).
- Diriltilen üyenin MP'si 0'dır: ilk aksiyonu MP potu olur. Priest diriltilmişse diğer priest/heal'ler onu korur.

## 6. Stok ve ikmal politikası

| Kimlik | Kural |
|---|---|
| STK-01 | Maç başı envanter senaryo tanımında sabittir ve iki taraf için eşittir: tüketilmeyen potlardan (720 HP, 1920 MP) birer adet (K-5), tüketilen potlardan senaryo stoğu (ör. 8v8 S1: HP 1440 ×40). ScenarioRunner maç öncesi envanteri doldurur ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)). |
| STK-02 | (Tüketilen potlar için) stok %25'in altına düşünce "tasarruf modu": P-POT-*-DEFICIT-MIN +0,1, P-POT-HP-EMERG değişmez. |
| STK-03 | Tüketilen potun stoğu 0: o kademe kullanılmaz, tüketilmeyen kademeye düşülür; hiç pot yoksa HP için priest/sitting, MP için sitting (yalnızca 60 m içinde düşman yoksa). |
| STK-04 | Ronark Land'de pot satıcısı yoktur `[V]`. Canlı (test dışı) modda ikmal için zone'dan çıkış **ilk kapsamda yoktur**; bot stok bitince üs içinde bekler veya test operatörüne bildirir. |
| STK-05 | Ağırlık: pot ağırlıkları (ör. 720 HP potu 90) stoğu sınırlar. Kuşanma ve savaşta ağırlık kontrolü yoktur (yalnızca alımda), ama MB-12 nedeniyle ağırlık sınırı test edilir. |

## 7. Telemetri ve ayar

Metrikler: MET-POT-01..04, MET-SUR-01..06 ([16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md)). Ayar yöntemi:

1. F6'da 1v1 ve küçük takım senaryolarında başlangıç eşikleriyle ölçüm.
2. Geri çekilme başarı oranı (MET-SUR-01), geri çekilmede ölüm (MET-SUR-03) ve savaş dışı süre (MET-SUR-04) arasındaki ödünleşim grafiği.
3. Eşikler L1 optimizasyonunda [14](14_LEARNING_AND_ADAPTATION.md) §4.1 aralıklarında aranır; kabul yalnızca değerlendirme setinde (evalset).

## 8. Test senaryoları ve kabul kriterleri

| Test | Amaç |
|---|---|
| T-SUR-01 | Sabit hasar altında %30 geri çekilme ve %65'te dönüş; salınım yok |
| T-SUR-02 | Priest desteği varken geri çekilmenin ertelenmesi |
| T-SUR-03 | Kök altında sıkışma → son direniş |
| T-SUR-04 | Party üyesinin takım yönüne çekilmesi (kopma yok) |
| T-POT-01 | HP/MP pot cooldown'ları (sunucu) ve bot politikası |
| T-POT-02 | MP potunun %40'ı beklemeden, eksik 1824'te içilmesi; küçük eksikte içilmemesi |
| T-POT-03 | HP ve MP potunun aynı anda gerekmesi |
| T-MECH-POT-03/04/05 | İstemci: HP/MP ortak zamanlayıcı var mı, pot hareketi durduruyor mu, envanterde olmayan pot kullanılabiliyor mu |

| Kimlik | Kriter |
|---|---|
| AC-SUR-01 | MET-SUR-06 durum salınımı = 0 |
| AC-SUR-02 | T-SUR-01: geri çekilme başarı oranı ≥ %70 (kaçış yolu varken) |
| AC-SUR-03 | MET-POT-01 pot verimi ≥ %80 |
| AC-SUR-04 | Envanterde bulunmayan pot kullanımı = 0 (CLI-06); 2 sn grup cooldown ihlali = 0 |
| AC-SUR-05 | T-SUR-04: geri çekilen party üyesinin takım merkezinden uzaklaşma oranı ≤ %10 |

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
