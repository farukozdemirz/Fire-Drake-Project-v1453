# 09 — Party Koordinasyonu ve Hedef Seçimi

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Bu doküman takım düzeyindeki kararların, `TeamBlackboard` sözleşmesinin, hedef skorunun ve takım chat protokolünün tek kaynağıdır. Sınıf davranışları 06–08'de, mekanik 03'te (party: MEC-PTY-*, chat: MEC-CHT-*) tanımlıdır.
> Eşikler ayarlanabilir tasarım parametresidir `[Ö]`.

---

## 1. Amaç

Botların yalnızca aynı hedef kimliğini paylaşması yetmez. Takım:

1. Rolleri dağıtır ve kompozisyon kurallarına uyar.
2. Ortak hedefi **ulaşılabilirlik, bitirilebilirlik, tehdit ve risk** üzerinden seçer, gereksiz yere değiştirmez.
3. Debuff ile saldırıyı zamanlar.
4. Priest ve mage'leri korur.
5. Dağılınca toplanır, gerektiğinde birlikte geri çekilir ve yeniden girer.
6. Ölüm, respawn, diriltme ve summon akışını yönetir.
7. Lider kaybı, tam yenilgi ve üçüncü taraf müdahalesinde toparlanır.

## 2. Party kurulumu

### 2.1 Mekanik kısıtlar `[D]`

- En fazla 8 üye (MEC-PTY-01).
- Davet için aynı ulus, aynı zone ve seviye bandı gerekir (MEC-PTY-02). Tüm botlar level 80 olduğundan seviye bandı sorun değildir.
- Zone değişimi party'den çıkarır (MEC-PTY-05). Party **Ronark Land'e girdikten sonra** kurulur.
- Lider ayrılırsa veya koparsa party silinir (MEC-PTY-03). **Ölüm ayrılma değildir.**

### 2.2 Kurulum akışı `[Ö]`

Bot yöneticisi party'yi gerçek paketlerle kurar: lider `PARTY_CREATE` / davet, üye `PARTY_PERMIT 1` ([03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §14). İnsan oyuncu içeren karma party'lerde davet insana normal istemci akışıyla gider.

### 2.3 Kompozisyon kuralları

| Kimlik | Kural | Tür |
|---|---|---|
| REQ-PTY-02 | Bir party'de **en fazla iki priest** | Kullanıcı gereksinimi |
| CMP-01 | Her party'de en az bir priest (yoksa party "desteksiz" modda çalışır, daha erken geri çekilir) | Öneri |
| CMP-02 | İki priest varsa biri P-HD, diğeri P-HB | Öneri |
| CMP-03 | Mage varsa summon görevi bir mage'e atanır (`summoner`) | Öneri |
| CMP-04 | W-G sayısı ≥ 1 (priest korumak için), 8'lik party'de önerilen | Öneri |

### 2.4 Örnek 8'lik kompozisyonlar

| Kimlik | Warrior | Priest | Mage | Kullanım |
|---|---|---|---|---|
| C8-A Standart | 2 W-P + 1 W-G | P-HD + P-HB | 2 M-F + 1 M-I | Varsayılan değerlendirme kompozisyonu |
| C8-B Melee ağırlıklı | 3 W-P + 1 W-G | P-HD + P-HB | 1 M-F + 1 M-I | Kompozisyon çeşitliliği |
| C8-C Büyü ağırlıklı | 1 W-P + 1 W-G | P-HD + P-HB | 3 M-F + 1 M-I | Kompozisyon çeşitliliği |
| C8-D Tek priest | 3 W-P + 1 W-G | P-HD | 2 M-F + 1 M-I | Stres testi |

Küçük takımlar: C2 (W-P + P-HD), C3 (W-P + P-HD + M-F), C4 (W-P + W-G + P-HD + M-F), C5 (C4 + P-HB).

## 3. Roller ve liderlik

| Rol | Görev | Varsayılan atama |
|---|---|---|
| **Lider** (`leader`) | Takım modu: ENGAGE / HOLD / RETREAT / REGROUP; regroup noktası; yeniden giriş kararı | W-G (yoksa en yüksek HP'li warrior) |
| **Hedef çağırıcı** (`caller`) | Ortak hedef önerisi ve çağrısı | P-HD (debuff ile); P-HD yoksa veya ölüyse lider |
| **Summoner** | Respawn olan üyeleri çağırma | Bir mage (M-I tercih; daha dayanıklı) |
| **Anchor** (`peel`) | Priest'leri korumak | W-G |

**Lider değişimi:** Halef listesi `[W-G, W-P1, W-P2, P-HB, …]`. Lider ölür veya geri çekilme durumuna girerse `P-TEAM-LEADER-TIMEOUT` (1,5 sn) içinde halef "vekil lider" olur (MET-PTY-03 ≤ 2 sn). Oyun içi party liderliği yalnızca lider oturumu düşecekse (bot despawn, test sonu) `PartyPromote` ile devredilir. Ölüm party'yi silmez; oyundaki lider bayrağının değişmesi gerekmez. Bot katmanındaki liderlik, oyun içi party liderliğinden bağımsızdır.

## 4. `TeamBlackboard` sözleşmesi

Botlar takım bilgisini chat ayrıştırmadan, paylaşılan bir veri yapısı üzerinden alır. Chat yalnızca insanlara görünürlük içindir.

### 4.1 Adalet kuralı

- Blackboard'a yalnızca **en az bir takım üyesinin gözlem sözleşmesine uygun olarak gördüğü** bilgi yazılır ([14](14_LEARNING_AND_ADAPTATION.md) §5.2). Bu, insan takımların sesli iletişimine karşılık gelir.
- Takım içi paylaşım gecikmesi `P-TEAM-COMMS-DELAY` = 300 ms ile modellenir: bir üyenin gözlemi diğerlerine bu gecikmeyle görünür.

### 4.2 Kayıt tipleri

| Kayıt | Alanlar | Yazan | Ömür |
|---|---|---|---|
| `TeamPlan` | mod, regroup noktası, direnç planı, odak listesi | Lider | Sürekli |
| `TargetCall` | hedef_id, hedef_adı, sebep (`DEBUFF_SUCCESS`, `LOW_HP`, `HEALER_PRESSURE`, `LEADER`), debuff, zaman, durum (`aktif`, `zayıfladı`, `geçersiz`) | Caller, lider | Hedef ölene / 20 sn |
| `Reservation` | tür (`heal`, `cure`, `res`, `summon`, `peel`), hedef, sahip, bitiş | Herkes | Bitişe kadar |
| `MemberStatus` | rol, durum (COMBAT/RETREAT/DEAD/RESPAWN_HOLD/READY_FOR_SUMMON/REINTEGRATE), HP/MP, konum, stok (taş/pot) | Her bot kendisi | Sürekli |
| `EnemyIntel` | düşman id, sınıf, son konum, son 5 sn'de aldığı hasar/heal, gözlenen buff/debuff, "healer" etiketi | Gözlemleyen | 10 sn sonra bayatlar |
| `PeelRequest` | destekçi, saldırgan | Priest/mage | Tehdit bitene kadar |

## 5. Hedef seçimi

### 5.1 Gözlemlenebilir girdiler

Hedef skoru yalnızca şunları kullanır: görünür düşmanların konumu, sınıfı, ekipmanı; vurulan veya seçili düşmanın kesin HP'si; gözlenen hasar/heal olayları; gözlenen buff/debuff olayları. Düşman MP'si, cooldown'u, envanteri **kullanılmaz** ([03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §16).

### 5.2 Skor

```
score(t) = w_kill·K(t) + w_reach·R(t) + w_threat·T(t) + w_debuff·D(t)
           − w_risk·Risk(t) + commit_bonus(t)
```

| Bileşen | Tanım | Aralık |
|---|---|---|
| `K` bitirilebilirlik | `1 / (1 + TTK_est / 10 sn)`. `TTK_est = HP_est / max(ε, takım_hasar_hızı_tahmini − hedef_heal_hızı)`. HP_est bilinmiyorsa sınıf/ekipmana göre ön tahmin. | 0–1 |
| `R` ulaşılabilirlik | Rol menzil bandına ≤ `P-TGT-REACH-TIME` (4 sn) içinde ulaşabilecek canlı üye sayısı / canlı üye sayısı | 0–1 |
| `T` tehdit | Sınıf ve rol ağırlığı: healer 1,0, mage 0,8, warrior 0,6; son 5 sn'de bize verdiği hasarla artar | 0–1,5 |
| `D` debuff durumu | Bizim debuff'ımız aktif (Malice/Torment/Parasite): +0,3 her biri, en fazla 0,6; Counter Curse/Curse Refraction gözlenmiş: −0,5 | −0,5–0,6 |
| `Risk` | Hedef kendi guard tower halkasına ≤ 90 m: +1,0; Mage Armor gözlenmiş: +0,4; hedefin 10 m çevresindeki düşman sayısı ≥ 3: +0,3; hedefe giden yol düşman kümesinin içinden geçiyor: +0,3 | 0–2 |
| `commit_bonus` | Mevcut ortak hedefse, atandıktan sonraki ilk `P-TGT-COMMIT-MIN` sn içinde +∞ (override hariç), sonra `+P-TGT-SWITCH-MARGIN·score` | — |

Başlangıç ağırlıkları (`P-TGT-*`): w_kill 1,2 · w_reach 1,0 · w_threat 0,6 · w_debuff 0,5 · w_risk 1,0. P-TGT-COMMIT-MIN 4 sn · P-TGT-SWITCH-MARGIN %30 · P-TGT-REACH-SLACK 3 m.

### 5.3 Hedefe bağlılık ve değiştirme

- Ortak hedef atandıktan sonra en az `P-TGT-COMMIT-MIN` sn değişmez (override hariç).
- Sonrasında yeni aday, mevcut hedefin skorunu `P-TGT-SWITCH-MARGIN` kadar geçmelidir.
- Değiştirme maliyeti: warrior'ların yeniden yaklaşma süresi ve debuff'ın kaybı skora yansır (yeni hedefin `D` = 0, `R` yol süresine bağlı).

### 5.4 Acil override'lar (bağlılık süresini yok sayar)

| Override | Koşul |
|---|---|
| `TARGET_DEAD` / `TARGET_LOST_VIS` | Hedef öldü veya 3 sn görünmüyor |
| `TARGET_UNREACHABLE` | `R(t) < 0,25` ve 3 sn sürüyor |
| `FINISHABLE` | Başka bir düşman HP < %15, ulaşılabilir (`R ≥ 0,5`) ve mevcut hedef HP > %50 |
| `PEEL_THREAT` | Kendi priest'imiz HP < %40 ve düşman melee teması var (yalnızca anchor ve en yakın warrior için bireysel override) |
| `HEALER_SWITCH` | §6 kararı |

## 6. Problem: "Ortak hedef heal ile ayakta tutuluyor"

### 6.1 Gözlem

`EnemyIntel`'den hedef için kayan pencereler (5 sn ve 10 sn):

- `dmg_rate(t)`: takımın hedefe verdiği hasar (vuran botlar hedefin kesin HP'sini alır).
- `heal_rate(t)`: hedefin HP artışları + hedefe yönelik gözlenen heal olayları.
- `net(t) = dmg_rate − heal_rate`.
- `stall(t)`: `net(t) ≤ 0,1·dmg_rate` ve HP oranı son 6 sn'de %5'ten az düştü.
- Healer kimliği: hedefe heal atan düşman priest(ler), gözlenen skill olaylarından.

### 6.2 Karar tablosu

| Durum | Karar |
|---|---|
| `stall` yok | Mevcut hedefe devam |
| `stall` var, tek düşman healer, healer'a `R ≥ 0,5`, healer'ın 8 m içinde düşman melee yok, healer TTK_est < mevcut hedef TTK_est | **HEALER_SWITCH:** ortak hedef healer olur; W-P'ler Scream/Shock Stun ile kilitler; P-HD Malice; mage'ler patlama |
| `stall` var, healer korunuyor (yakınında ≥ 1 düşman melee) veya `R < 0,5` | **SPLIT:** bir W-P (en yakın) healer'a baskı yapar (cast kesme fırsatı, kaçmaya zorlama), takımın geri kalanı hedefte kalır ve **senkron patlama** bekler |
| `stall` var, iki düşman healer | Priest'lerden birine HEALER_SWITCH yalnızca `R ≥ 0,6` ise; değilse senkron patlama + Parasite + Sweep mana (P-HD) |
| Senkron patlama hazır (P-HD debuff + ≥ 2 mage'in patlama skill'i hazır + W-P MP ≥ BURST) | Lider `BURST_NOW` sinyali: 2 sn içinde tüm patlama skill'leri, hedefin heal kapasitesini aşmayı amaçlar |
| Kendi takımımız kayıpta (canlı < düşman, toplam HP oranı < 0,4) | Hedef değişimi yok; [11]/[12] geri çekilme kararı öncelikli |

**Heal kapasitesi tahmini `[I]`:** Bir priest tek hedefe yaklaşık 1920 HP / ~1,6 sn (Superior healing) ≈ 1200 HP/sn sürdürebilir. Complete healing 5,4 sn'de bir tam heal ekler ([05](05_SKILL_CATALOG_AND_COMBAT_RULES.md) §6). Senkron patlamanın hedefi, 2 sn'lik pencerede hedefin "HP + 2 sn heal kapasitesi"ni aşmaktır. Bu nedenle Parasite (maks HP ve HP buff'ını düşürür) ve AC debuff'ı patlamadan önce uygulanır.

**Histerezis:** HEALER_SWITCH sonrası en az 6 sn yeni healer kararı verilmez. Healer kaçar ve ulaşılamaz olursa (`R < 0,25`, 3 sn) eski hedefe dönülür.

## 7. Debuff ile saldırının zamanlanması

1. Caller (P-HD) ortak hedefe Malice/Parasite atar → başarı → `TargetCall(DEBUFF_SUCCESS)` ([07](07_PRIEST_BEHAVIOR.md) §9.2).
2. W-P'ler: hedef menzildeyse sonraki Type1 slotunda en yüksek hasarlı skill; menzil dışındaysa sprint + yaklaşma.
3. Mage'ler: `P-MAG-BURST-WINDOW` içinde Absolute power + incineration/Prismatic.
4. Ölçüm: MET-DEBUFF-02 (debuff → ilk takım hasarı) p50 ≤ 2 sn.

Debuff yoksa (P-HD ölü/uzakta) lider hedefi `LEADER` sebebiyle çağırır. Bu durumda patlama penceresi kısa tutulur.

## 8. Formasyon, koruma ve dağılmanın önlenmesi

| Kural | Değer |
|---|---|
| Party merkezi | Canlı ve RETREAT dışındaki üyelerin konum ortalaması |
| P-PTY-SPREAD-MAX | 45 m; aşılırsa lider REGROUP sinyali |
| Ön hat | Warrior'lar; ortak hedefe doğru |
| Orta hat | Mage'ler (P-MAG-PREF-RANGE) |
| Arka hat | Priest'ler (P-PRI-POS-BACK) |
| Anchor | W-G, priest'lerden ≤ 12 m, düşman melee ile priest arasında |
| Üst üste yığılma | Aynı party'den iki üye < 1,5 m ise ayrışma vektörü ([12](12_NAVIGATION_AND_POSITIONING.md)) |
| P-PTY-REGROUP-R | 15 m: regroup noktasına bu mesafe "toplandı" sayılır |

## 9. Ölüm, diriltme, respawn ve yeniden katılım (takım akışı)

```
DEATH(u):
  if u.role is not priest and P-HD alive and res_conditions_likely(u):   # 07 §10
      P-HD posts ResIntent(u); u enters RESPAWN_HOLD for P-PTY-RES-WAIT (12 s)
  else:
      u respawns immediately (WIZ_REGENE type 1)
AFTER RESPAWN(u):
  u.RECOVER: MP pot to role threshold [11]; stay inside own guard-tower ring
  if team has alive summoner and team mode != RETREAT: u posts READY_FOR_SUMMON
  else: u walks to regroup point with path avoiding enemy clusters [12]
AFTER SUMMON(u): u.REINTEGRATE (≤ P-PTY-REINTEGRATE-MAX = 6 s) then COMBAT
```

Ölçüm: MET-PTY-01 (respawn → takıma katılım) summon'lu ve summon'suz ayrı raporlanır.

## 10. Geri çekilme, regroup ve yeniden giriş (takım düzeyi)

| Tetik | Takım kararı |
|---|---|
| Canlı sayımız ≤ görünür düşman sayısının %60'ı ve takım HP toplamı oranı < 0,5 | **RETREAT:** regroup noktası = kendi tarafımıza doğru, düşman kümesinden uzak güvenli nokta ([12](12_NAVIGATION_AND_POSITIONING.md) §8). Priest'ler ve mage'ler önden, warrior'lar arkadan; yavaşlatma ile takip kesme. |
| P-PTY-SPREAD-MAX aşıldı, savaş sürüyor | **REGROUP:** odak korunur, üyeler merkeze yaklaşır |
| Regroup tamam (≥ %75 üye P-PTY-REGROUP-R içinde) ve HP/MP ortalaması ≥ %70 ve buff kapsaması ≥ %80 | **RE-ENGAGE** |
| Tam yenilgi (canlı üye 0) | Herkes respawn → üs içinde toplan → buff/MP tamamla → **birlikte yürüyerek** arena'ya; summoner varsa önce summoner + anchor + bir priest yürür, kalanları güvenli noktada summon eder |

Bireysel geri çekilme (HP < %30) [11]'dedir. Bireysel geri çekilen üye takımdan kopmamak için önce **takım merkezine/arka hatta** çekilir; kaçış yönü takımın tersine değildir ([11] §5).

## 11. Birden fazla düşman party ve üçüncü taraf

- Perception görünür düşmanları 15 m bağlantı mesafesiyle kümeler. Ana küme "mevcut savaş", diğerleri "yaklaşan tehdit" olur.
- Yaklaşan küme 40 m içine girer ve yönü bize doğruysa lider durumu yeniden değerlendirir: sayısal üstünlük kaybedildiyse RETREAT. Kazanılıyorsa mevcut hedefi bitirmek için `P-TEAM-FINISH-WINDOW` (5 sn) tanınır, sonra HOLD/RETREAT.
- Üçüncü taraf: düşman olmayan oyuncular (kendi ulusumuz) dikkate alınmaz. Canavarlar yalnızca bize saldırıyorsa savunma amaçlı hedef olur. Test arenasında canavar bulunmaması hedeflenir ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)); beklenmeyen canavar müdahalesi telemetride `THIRD_PARTY` olarak işaretlenir ve maçı geçersiz kılabilir.
- Guard tower: botlar karşı ulusun tower halkasına (kapıdan ~85 m) girmez ([12](12_NAVIGATION_AND_POSITIONING.md) §7).

## 12. Party chat protokolü

| Mesaj | Biçim | Gönderen | Koşul |
|---|---|---|---|
| Hedef çağrısı | `HEDEF: <ad> (<debuff>)` | Caller | Başarılı debuff (07 §9.2) veya lider kararı |
| Healer geçişi | `HEALER: <ad>` | Lider | §6 HEALER_SWITCH |
| Patlama | `PATLAT: <ad>` | Lider | §6 BURST_NOW |
| Toplan | `TOPLAN` | Lider | REGROUP |
| Geri çekil | `GERİ` | Lider | RETREAT |
| Summon bekliyor | `TP` | Respawn olan üye | READY_FOR_SUMMON (dönem kullanımı "TP" `[S]`) |

Sınırlar: bot başına 1 mesaj / 4 sn, 6 mesaj / dk; aynı içerik 8 sn içinde tekrar yok (MET-CHAT-01 ihlal = 0). Sunucuda chat sınırı yoktur (MEC-CHT-02); sınır bot tarafındadır. Mesajlar ASCII dışı karakter içerebilir; `GERİ` için istemci karakter seti testi T-PTY-09 `[A]` (gerekirse `GERI`).

## 13. Pseudocode (lider ve hedef çağırıcı)

```
leader_tick(team):
  s = team.shared_snapshot()                       # sözleşmeli, 300 ms gecikmeli
  if team.mode in (ENGAGE, HOLD) and retreat_condition(s): set_mode(RETREAT, regroup_point(s))
  elif spread(s) > P_PTY_SPREAD_MAX: set_mode(REGROUP, center(s))
  elif team.mode in (RETREAT, REGROUP) and reengage_ready(s): set_mode(ENGAGE)
  if team.mode == ENGAGE:
     cur = team.target
     if override(cur, s): cur = None
     best = argmax(score(t) for t in s.visible_enemies)
     if cur is None or (age(cur) >= COMMIT_MIN and score(best) > score(cur)*(1+SWITCH_MARGIN)):
         assign_target(best, reason)
     handle_stall(cur, s)                          # §6
```

## 14. Test senaryoları ve kabul kriterleri

| Test | Amaç |
|---|---|
| T-PTY-01 | Party kurulumu ve kompozisyon kuralı (3. priest reddi) |
| T-PTY-02 | Ortak hedef: katılım oranı ve çağrı → baskı süresi |
| T-PTY-03 | Heal ile ayakta tutulan hedef: HEALER_SWITCH/SPLIT kararları |
| T-PTY-04 | Hedef değiştirme sıklığı ve gerekçeleri (thrash yok) |
| T-PTY-05 | Lider ölümü ve vekil lider |
| T-PTY-06 | Dağılma ve regroup |
| T-PTY-07 | Tam yenilgi ve toparlanma |
| T-PTY-08 | İkinci düşman party'nin gelişi |
| T-PTY-09 | Chat mesajlarının istemcide görünmesi ve oran sınırı |

| Kimlik | Kriter |
|---|---|
| AC-PTY-01 | MET-TGT-03 ortak hedefe katılım ≥ %75 (fiilen katılabilir üyeler üzerinden) |
| AC-PTY-02 | MET-TGT-04 bot başına hedef değişimi ≤ 4/dk; `SCORE_MARGIN` gerekçeli ≤ 1,5/dk |
| AC-PTY-03 | T-PTY-03'te stall tespitinden karara p50 ≤ 3 sn; HEALER_SWITCH sonrası 20 sn içinde healer ölümü veya geri çekilmesi oranı baseline'a göre raporlanır |
| AC-PTY-04 | MET-PTY-03 vekil lider ≤ 2 sn |
| AC-PTY-05 | MET-PTY-04 regroup ≤ 15 sn (ulaşılabilir alanda) |
| AC-PTY-06 | Party kompozisyonunda 3 priest durumu 0 (statik kontrol) |
| AC-PTY-07 | MET-CHAT-01 ihlal 0; MET-DEBUFF-04 yanlış çağrı 0 |

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
