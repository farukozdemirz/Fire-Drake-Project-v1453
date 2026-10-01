# 07 — Priest Davranışı

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Profiller: `priest.heal_debuff` (P-HD) ve `priest.heal_buff` (P-HB) — [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) §5.3–5.4. Skill verisi: [05](05_SKILL_CATALOG_AND_COMBAT_RULES.md) §6 ve `appendix/A2`. BuffType çakışmaları: [05](05_SKILL_CATALOG_AND_COMBAT_RULES.md) §4. Takım koordinasyonu ve hedef çağrısı protokolü: [09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md). Kaynak ve geri çekilme: [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md).
> Eşikler ayarlanabilir tasarım parametresidir `[Ö]`.

---

## 1. Amaç ve kapsam

Priest'in görevi takımın **hayatta kalma kapasitesini** ve **buff/debuff durumunu** yönetmektir:

1. Ölümü önleyen acil heal, ardından overheal'i düşük tutan normal heal.
2. Takım buff'larının (AC, maks HP, direnç) savaş boyunca kapsanması (P-HB).
3. Kritik debuff ve DoT'ların temizlenmesi.
4. Ortak hedefe taktiksel debuff ve **başarılı** debuff sonrası hedef çağrısı (P-HD).
5. Ölen üyeyi koşullar uygunsa diriltme (P-HD).
6. Kendini hayatta tutma ve konum yönetimi.

**Doğrulanmış rol fizibilitesi:** Her iki hibrit rol level 80'de 142 skill puanına sığar (Heal 60 + Curse 62 + Master 20 ve Heal 60 + Buff 62 + Master 20; [04](04_CHARACTER_BUILDS_STATS_AND_EQUIPMENT.md) §5.3–5.4) `[D]`/`[V]`. Üst seviye skill'lerin tamamı aynı anda kullanılamaz. P-HD'de AC/HP buff'ı yoktur, P-HB'de debuff ve diriltme yoktur.

## 2. Girdiler

| Girdi | Kaynak | Not |
|---|---|---|
| Party üyelerinin kesin HP/MP'si ve maks değerleri | `PARTY_HPCHANGE` (MEC-PTY-04) | Gözlem sözleşmesine uygun |
| Üye başına gelen hasar hızı (son 2 sn ve 5 sn) | HP değişim akışından türetilir | `incoming_2s`, `incoming_5s` |
| Üyelere yakın düşmanlar (sınıf, mesafe) | Bölge paketleri | Tehdit tahmini |
| Üyeler üzerindeki buff/debuff'lar ve tahmini bitiş zamanı | Kendi cast'leri + gözlenen skill olayları (bölge yayını) | Süre = olay zamanı + skill süresi |
| Kendi MP, recast'ler, sunucu saniyesi, envanter (taş, pot) | Kendi durumu | |
| Ortak hedef, debuff çağrıları, heal/cure/res rezervasyonları | `TeamBlackboard` | |

## 3. Parametreler (bu doküman sahibidir)

| Kimlik | Varsayılan | Açıklama | Öğrenmeye açık |
|---|---|---|---|
| P-PRI-HEAL-EMERG | 0,32 | Tahmini HP oranı bunun altındaysa **acil** heal | Evet (0,20–0,45) |
| P-PRI-HEAL-NORMAL | 0,75 | Bunun altında normal heal adayı | Evet (0,6–0,85) |
| P-PRI-PREHEAL-K | 0,8 | Tahmin ufkunda gelen hasar ağırlığı | Evet (0–1,5) |
| P-PRI-HORIZON | cast süresi + 0,4 sn | HP tahmin ufku | Hayır |
| P-PRI-OVERHEAL-MAX | 0,25 | Normal heal'de kabul edilen overheal oranı | Evet |
| P-PRI-MP-RESERVE | 1100 MP | Complete healing (960) + Cure curse (60) + pay | Evet (800–2000) |
| P-PRI-GROUP-MIN | 3 üye | Grup heal için r=30 içindeki "heal gerektiren" üye sayısı | Evet |
| P-PRI-BUFF-REFRESH | 20 sn | Buff bitimine bu kadar kala hedefin yakınında olmayı planla | Evet |
| P-PRI-CURE-DOT-MIN | 800 HP | Kalan DoT toplamı bunu aşarsa Cure disease | Evet |
| P-PRI-RES-SAFE-RADIUS | 15 m | Diriltme için bu yarıçapta düşman melee olmamalı | Evet |
| P-PRI-POS-BACK | 20–35 m | Ön hattın arkasında tercih edilen mesafe | Evet |
| P-PRI-ENEMY-MELEE-MIN | 22 m | En yakın düşman warrior'a minimum mesafe tercihi (peel yoksa) | Evet |
| P-PRI-CALL-DEDUP | 8 sn | Aynı hedef için yeni çağrı aralığı | Hayır |
| P-PRI-CHAT-RATE | 1 mesaj / 4 sn, 6 mesaj / dk | Party chat sınırı (MEC-CHT-02 sunucuda sınır yok) | Hayır |

## 4. Karar öncelikleri

1. **Kendi ölümünü önleme:** [11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md) geri çekilme kararı. Geri çekilirken de kendine heal atılabilir (acil heal kendine).
2. **Acil heal** (kendisi dahil): Tahmini HP oranı < P-PRI-HEAL-EMERG olan, menzildeki üye. İki priest varsa rezervasyon kuralı (§6).
3. **Grup heal:** Menzil ve r=30 içinde ≥ P-PRI-GROUP-MIN üye normal heal eşiğinin altında.
4. **Kritik cure:** §8 tablosundaki "kritik" debuff'lar.
5. **Normal heal:** Overheal kuralına uyan en verimli heal.
6. **Diriltme** (P-HD): §10 koşulları.
7. **Debuff** (P-HD): Ortak hedef açıcı veya fırsat debuff'ı (§9).
8. **Buff** (P-HB; P-HD yalnızca kendi master buff'ları): §7 matrisi.
9. **Pozisyon:** Ön hattın arkasında, heal menzilinde, düşman melee'sinden uzak (§11).
10. **Melee** (yalnızca solo veya takımda heal ihtiyacı yokken): Judgment/Helis.

## 5. Heal seçimi

### 5.1 Tahmin

```
hp_pred(u) = u.hp − P_PRI_PREHEAL_K · incoming_rate(u) · P_PRI_HORIZON − pending_heals(u)
deficit(u) = u.maxhp − hp_pred(u)
ratio(u)   = hp_pred(u) / u.maxhp
```

`pending_heals(u)`, diğer priest'in rezervasyonlarından gelen beklenen heal toplamıdır (§6).

### 5.2 Skill seçimi (tek hedef)

| Koşul | Seçim | Gerekçe |
|---|---|---|
| ratio < EMERG ve deficit ≥ 2500 ve Complete healing hazır, MP ≥ 960 | **Complete healing** (tam) | Tek cast ile kurtarma |
| ratio < EMERG ve (Complete recast'te veya deficit < 2500) | Superior healing (1920) | Hızlı, yüksek |
| deficit ≥ 1920·(1 − OVERHEAL-MAX) | Superior healing | |
| deficit ≥ 960·(1 − OVERHEAL-MAX) ve Great healing hazır | **Great healing** (960, en iyi HP/MP) | Verim |
| deficit ≥ 720 ve Great healing recast'te | Massive healing (960) | |
| deficit ≥ 400, hedefte HoT yok, gelen hasar sürekli | Superior restore (HoT 2500/30 sn) | Önceden heal; MEC-T3-01 |
| aksi | Heal yok | Overheal önleme |

Grup: r=30 içinde ≥ 3 üyenin deficit'i ≥ 700 → Group massive healing (960 MP). ≥ 3 üye ratio < 0,45 ve Group complete hazır, MP ≥ 1920 + rezerv → Group complete healing.

Heal'ler sabit değerlidir (MEC-CHR-12); stat ile değişmez. Kritik nokta (Critical Point) buff'ı heal'i 2 katına çıkarabilir; bu durumda overheal tahmini 1,5 kat ile hesaplanır.

### 5.3 Cast ve kesilme

- Cast 1,5 sn (CLI-03): CASTING gönderilir, 1,5 sn sonra EFFECTING. Bekleme sırasında hedef ölür, menzilden çıkar veya bot hareket etmek zorunda kalırsa iptal edilir (SK-02). Sunucu MP'yi EFFECTING'de düştüğü için iptal MP harcatmaz (MEC-MAG-08).
- Dönem kaynaklarına göre düşmanın R vuruşları büyü cast'ini keser `[S]`/`[B]`. Bu sunucuda cast kesilmesinin sunucu tarafı karşılığı yoktur (MEC-MAG-01). İstemci davranışı `[A]` (T-MECH-CLIENT-03). Bot için kural: düşman warrior R menzilindeyken (≤ 15 + silah menzili) cast başlatmadan önce 3–4 adım geri çekilme seçeneği değerlendirilir. Dönem rehberi de bunu öneriyor `[S]`.

## 6. İki priest koordinasyonu

Party'de en fazla iki priest vardır. Önerilen eşleşme P-HD + P-HB.

| Konu | Kural |
|---|---|
| Heal rezervasyonu | Priest heal'e karar verdiğinde `TeamBlackboard`'a `{hedef, skill, beklenen miktar, bitiş = şimdi + cast + 0,3 sn}` yazar. Diğer priest aynı hedefe yalnızca `ratio_after_pending < EMERG` ise heal atar. |
| Çift heal metriği | MET-HEAL-04 ≤ %5 hedefi |
| Görev paylaşımı | P-HB: buff kapsaması birincil, heal ikincil. P-HD: debuff/çağrı ve diriltme birincil, heal ikincil. **Acil heal her ikisinin de birinci önceliğidir.** |
| Hedef dağıtımı | Her üye için "birincil healer" atanır: üyeye en yakın ve MP'si yüksek priest. Birincil healer'ın bir rezervasyonu varken diğeri ikinci acil durumu alır. |
| Tek priest kaldığında | Kalan priest diğerinin birincil görevlerini devralır; öncelik: acil heal > cure > grup heal > diriltme > buff > debuff. |
| Cure rezervasyonu | Cure de rezerve edilir; iki priest aynı üyeye aynı anda cure atmaz. |
| Buff paylaşımı | Buff'ları yalnızca P-HB atar. P-HB ölürse P-HD buff atamaz (ağacı yok). Bu durumda buff kapsaması düşer; telemetri `BUFF_COVERAGE_LOST` olayı. |

## 7. Buff yönetimi (P-HB)

### 7.1 Buff matrisi

| Üye rolü | AC (BuffType 2) | Maks HP (BuffType 1) | Direnç (BuffType 8) | Diğer |
|---|---|---|---|---|
| Warrior (W-P/W-G) | Insensibility peel (+300) | **Undying** (maks HP %160) — taban HP yüksek olduğundan %60 artış > +1500 `[I]` | Fresh mind veya element direnci (§7.3) | Warrior Defense kullanmaz (06 §5) |
| Priest | Insensibility peel | massiveness (+1500) | Fresh mind | |
| Mage | Insensibility peel | massiveness (+1500) — taban HP düşük olduğundan +1500 > %60 `[I]` | §7.3 | Mage AC buff'ları (Frozen armor vb.) priest AC buff'ıyla çakışır; mage kendine AC buff'ı atmaz |

Kural `BUF-HP-01`: Maks HP buff'ı seçimi `argmax(maksHP_buffsuz · 0,6, 1500)` (yüzde buff'ın maks HP'nin hangi bileşenine uygulandığı `[A]`; T-MECH-BUF-03 ile doğrulanır). Party'de ≥ 4 üye r=30 içindeyse ve tek tek buff'lanmamışlarsa Greatness (+1200, 570 MP) ile toplu başlatma, sonra rol bazlı değiştirme **yapılamaz** (aynı tip, MEC-BUF-02). Bu nedenle ilk kurulumda tek tek atama tercih edilir.

### 7.2 Yenileme ve takip

- Buff süresi 600 sn. Sunucu süresi dolmadan aynı tipten yenilemeye izin vermez (MEC-BUF-02). Bu yüzden "yenileme", **süre dolduktan hemen sonra yeniden uygulama**dır. P-HB, bitişe P-PRI-BUFF-REFRESH kala hedefe menzil içinde olacak şekilde konum planlar.
- Süre takibi: kendi cast olayından (`t_cast + 600`) hesaplanır. Buff, düşman debuff'ıyla silinmiş olabilir (Malice/Torment AC'yi, Parasite HP'yi siler; MEC-BUF-03). Silinme gözlenen debuff olayından çıkarılır ve buff "eksik" olarak işaretlenir. Debuff süresi (150 sn) bitmeden buff yeniden atılamaz (debuff aynı tipte kayıt tutar) → cure curse ile debuff kaldırılır, ardından buff yeniden atılır.
- Ölüm tüm buff'ları siler (MEC-DTH-01). Respawn/diriltme sonrası üye "buff eksik" kuyruğuna girer.
- Savaş içinde buff yalnızca acil heal ihtiyacı yokken ve hedef menzildeyken atılır.

### 7.3 Direnç buff'ı kararı

Hedef başına tek direnç buff'ı (BuffType 8) olabilir. Takım düzeyinde karar ([09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) `TeamPlan.resist`):

| Düşman kompozisyonu | Karar |
|---|---|
| ≥ 2 düşman mage aynı elementte (gözlenen skill olaylarından) | Kendi mage'imizin ilgili element direnci (Immunity fire/cold/lightning, +80). M-F ateş, M-I buz direnci atabilir. |
| Karışık / bilinmiyor | Fresh mind (büyü/hastalık/zehir +80) |

### 7.4 Buff ve debuff tekrarı önleme

- Aynı tip buff hedefte varken buff gönderilmez (SK-03, MET-BUFF-03 = 0).
- Aynı debuff hedefte aktifken (gözlenen olay, kalan süre > 5 sn) tekrar atılmaz.

## 8. Cure

| Durum (müttefikte) | Skill | Öncelik |
|---|---|---|
| Kök/yavaşlatma (Scream, leg cutting, buz yavaşlatmaları) altında ve geri çekilmede veya kaçmaya çalışıyor | Cure curse | **Kritik** |
| Parasite/Superior Parasite (maks HP düşük) ve HP oranı < 0,6 | Cure curse | Kritik |
| Malice/Torment (AC) ve düşman melee teması var | Cure curse | Yüksek |
| Massive/Slow kendi warrior'ımızda ve ortak hedefe baskı sürüyor | Cure curse | Orta |
| DoT kalan toplamı ≥ P-PRI-CURE-DOT-MIN | Cure disease | Yüksek |
| Mage üzerinde herhangi bir kritik debuff | Cure curse | Rol ağırlığı ×1,3 (dönem rehberi: "önce mage'i cure et" `[S]`) |

Cure curse tüm debuff'ları kaldırır (REMOVE_TYPE4, [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §5.4). Bu yüzden birden fazla debuff taşıyan üye daha değerlidir: skor = Σ debuff ağırlıkları × rol ağırlığı × aciliyet.

Cure gecikmesi hedefi: MET-CURE-01 p95 ≤ 3 sn.

## 9. Debuff (P-HD) ve hedef çağrısı

### 9.1 Debuff seçimi

| Koşul | Debuff | Gerekçe |
|---|---|---|
| Takımın ortak hedefi var, hedefte AC debuff'ı yok | **Malice** (40 MP, AC %75, AC buff'ını siler) | Ucuz açıcı. AC iki kez uygulanıyor olabilir (MB-04). |
| ≥ 3 düşman 10 m içinde kümelenmiş ve hedef noktası menzilde | **Torment** (alan AC %70) | Malice ile aynı tip; hedefte Malice varsa Torment onu ezer (gereksiz değil, alan değeri) |
| Hedefte gözlenen HP buff'ı (massiveness/Undying) var veya hedef yüksek HP'li warrior | **Parasite** (HP buff'ını siler, maks HP %80) | Bitirilebilirliği artırır |
| Düşman warrior kendi priest'imize/mage'imize baskı yapıyor | Massive (saldırı %80) veya Slow | Hasar azaltma (Slow yalnızca istemcide etkili) |
| Düşman priest'in MP'si düşük görünüyor (gözlenen pot/heal yoğunluğu) | Sweep mana (−960) | Fırsat; düşük öncelik |
| Hedefte Counter Curse veya Curse Refraction (gözlenen) | Debuff yok | Boşa MP, yansıma riski |

### 9.2 Hedef çağrısı protokolü

Kullanıcı gereksinimi: başarılı ve taktiksel olarak anlamlı debuff sonrası party chat'te hedefin adıyla çağrı yapılır; botlar bilgiyi chat ayrıştırmasına bağımlı olmadan paylaşır.

1. Debuff `ACTION_RESULT = OK` ve hedef üzerinde debuff olayı gözlendi (SK-04).
2. "Taktiksel anlam" kontrolü: hedef ortak hedef adayıdır ([09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) skor ≥ eşik) **ve** ulaşılabilir üye sayısı ≥ 2.
3. `TeamBlackboard.post(TargetCall{hedef_id, hedef_adı, sebep=DEBUFF_SUCCESS, debuff, zaman})` — botlar bunu doğrudan okur.
4. Party chat: `"HEDEF: <karakter_adı> (Malice)"`. Sınırlar: P-PRI-CALL-DEDUP (aynı hedef 8 sn içinde tekrar yok), P-PRI-CHAT-RATE. Chat yalnızca **insanlar** için görünürlük sağlar; botların kararı chat'e bağlı değildir.
5. Başarısız debuff → çağrı yok, chat yok (MET-DEBUFF-04 = 0).
6. Debuff cure edildi (gözlenen) → `TargetCall` "zayıfladı" olarak işaretlenir. Takım hedefi bırakmaz ama skor yeniden hesaplanır ([09]). P-HD debuff'ı yeniden uygulamayı değerlendirir (recast 7,4 sn).

Party'deki insan oyuncuların chat'te verdiği hedef çağrıları: botlar `+`/komut önekli olmayan, `HEDEF:` veya `TARGET:` önekli mesajları **ek ipucu** olarak ayrıştırabilir (opsiyonel, `P-TEAM-HUMAN-CALLS`, varsayılan kapalı). Bu ayrıştırma başarısız olursa davranış bozulmaz.

## 10. Diriltme (P-HD)

Diriltme, ölen üyeyi ceset konumunda geri getirir. Diriltilen MP 0 ile başlar, blink almaz ve PvP ölümünde EXP iadesi yoktur (03 §5.4). Respawn + mage summon ile **karıştırılmaz** ([08](08_MAGE_BEHAVIOR.md), [09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md) §9).

| Koşul | Zorunlu |
|---|---|
| Ceset ≤ 11 m (skill menzili) | Evet |
| P-PRI-RES-SAFE-RADIUS içinde düşman melee yok, son 3 sn'de cesedin 20 m çevresinde düşman alan skill'i yok | Evet |
| Kendi HP oranı ≥ 0,6, MP ≥ 800 + rezerv | Evet |
| Ölen bot ise envanterinde yeterli Stone of Life var (`TeamBlackboard` stok bilgisi); insan oyuncuysa bilinmez → deneme yapılır, fail olursa tekrar denenmez | Evet |
| Takım "yerel kazanç" durumunda (görünür düşman sayısı ≤ kendi canlı sayımızın yarısı) veya savaş durdu | Evet |

Akış: ölüm anında P-HD `ResIntent{ölen, tahmini süre}` yazar → ölen bot `RESPAWN_HOLD` durumunda `P-PTY-RES-WAIT` (varsayılan 12 sn) bekler → diriltme olmazsa respawn → mage summon akışı ([08]). Diriltme Resurrection of favors (30 taş) yerine taş durumuna göre seçilir (love 4 / grace 10 / favors 30; EXP oranları PvP'de önemsiz).

## 11. Konum yönetimi

- Party merkezine ≤ 40 m, heal menzili (56; El Morad bazı skill'lerde 90) içinde en çok üyeyi kapsayan nokta.
- Ön hattın P-PRI-POS-BACK kadar arkasında.
- En yakın düşman warrior'a ≥ P-PRI-ENEMY-MELEE-MIN; ihlal edilirse W-G'ye peel isteği + geri adım.
- Engel arkasında kalmama: [12](12_NAVIGATION_AND_POSITIONING.md) yaklaşık görüş hattı testi; heal hedefiyle "görüş yok" ise yer değiştirme.
- İki priest birbirinden ≥ 8 m ayrı durur (alan debuff'larına toplu yakalanmama).

## 12. MP yönetimi

- Rezerv: P-PRI-MP-RESERVE (acil heal + cure). Rezervin altında yalnızca acil heal, cure ve pot.
- MP potu [11]'deki kurallarla; priest için MP önceliği HP'den yüksektir. Ancak HP oranı < 0,35 ise HP önceliklidir.
- Uzun savaşta MP harcama oranı izlenir. Tahmini tükenme süresi < 20 sn ise debuff/buff dondurulur.

## 13. Durumlar

`COMBAT` alt durumları: `SUSTAIN` (heal/cure odaklı), `SUPPORT` (buff/debuff), `RESCUE` (diriltme), `REPOSITION`. Geçişler §4 öncelik sırasının sonucudur; her tick yeniden değerlendirilir. Salınımı önlemek için `REPOSITION` en az 1 sn sürer.

## 14. Pseudocode

```
on_tick(p):
  s = p.perception.snapshot()
  if survival.retreat_needed(s): return RETREAT(heal_self_allowed=True)
  emerg = [u for u in s.party_alive if in_range(p,u) and ratio(u) < P_PRI_HEAL_EMERG]
  emerg = blackboard.filter_unreserved(emerg)
  if emerg: return heal_single(p, most_urgent(emerg))      # Complete/Superior
  if group_heal_needed(s): return heal_group(p)
  c = best_cure_candidate(s)
  if c and c.priority >= HIGH: return cure(p, c)
  n = best_normal_heal(s)                                   # overheal kuralı
  if n: return heal_single(p, n)
  if p.role == HD:
     if res_ok(s): return resurrect(p, s.best_corpse)
     d = choose_debuff(s)
     if d: return cast_debuff_and_maybe_call(p, d)          # §9.2
  if p.role == HB:
     b = next_missing_buff(s)                               # matris + çakışma
     if b: return cast_buff(p, b)
  return position.support_spot(p, s)
```

## 15. Hata ve fallback

| Durum | Davranış |
|---|---|
| Heal hedefi menzil dışı | Menzile yaklaş; yolda düşman melee varsa diğer priest'e devret (rezervasyonu bırak) |
| MP tükendi, pot cooldown'da | Melee'den uzaklaş, kendine en ucuz heal (Great healing 80 MP); W-G'den peel iste |
| Debuff fail (Counter Curse vb.) | Hedefi "debuff bağışık" olarak 10 sn işaretle; çağrı yok |
| Buff reddedildi (aynı tip var) | Hedefte bilinmeyen bir buff var demektir (ör. warrior Defense). Matris güncellenir ve 30 sn tekrar denenmez. |
| Diriltme fail (taş yok) | Ölen üyeye "respawn" sinyali; mage summon akışı |
| Silence/No-Potion altında | Silence: skill yok → geri çekil, cure iste (diğer priest). No-Potion: HP potu yok, heal skill'i serbest. |

## 16. Test senaryoları ve kabul kriterleri

| Test | Amaç |
|---|---|
| T-PRI-01 | Sabit hasar alan tek üyeyi hayatta tutma (overheal, verim) |
| T-PRI-02 | Ani patlama (burst): acil heal tepki süresi |
| T-PRI-03 | İki priest: çift heal ve cure çakışması |
| T-PRI-04 | Buff kapsaması 10 dk; debuff ile silinen buff'ın yeniden kurulması |
| T-PRI-05 | Cure önceliği (kök altındaki mage vs Malice'li warrior) |
| T-PRI-06 | Debuff + çağrı: başarılı debuff'ta çağrı, başarısızda çağrı yok; chat oranı |
| T-PRI-07 | Priest'e ani baskı: geri çekilme, peel isteği, kendine heal |
| T-PRI-08 | Diriltme güvenlik koşulları |

| Kimlik | Kriter |
|---|---|
| AC-PRI-01 | T-PRI-02'de acil heal kararı p95 ≤ 300 ms (tahmini HP eşiği aşıldıktan sonra), EFFECTING ≤ cast + 400 ms |
| AC-PRI-02 | MET-HEAL-02 overheal ≤ %25 (acil heal hariç) |
| AC-PRI-03 | MET-HEAL-04 çift heal ≤ %5 |
| AC-PRI-04 | MET-BUFF-01 kapsama ≥ %90 (P-HB canlıyken), MET-BUFF-03 = 0 |
| AC-PRI-05 | MET-CURE-01 p95 ≤ 3 sn (kritik debuff'lar) |
| AC-PRI-06 | MET-DEBUFF-04 yanlış çağrı = 0; MET-CHAT-01 limit ihlali = 0 |
| AC-PRI-07 | MET-HEAL-03 kurtarılan kritik durum sayısı baseline'dan düşük değil (öğrenme sonrası) |
| AC-PRI-08 | Güvensiz diriltme (diriltme sonrası 5 sn içinde ölüm) ≤ %10 |

## 17. Bağımlılıklar ve açık sorular

- Yüzde HP buff'ının (Undying) item HP'sine uygulanıp uygulanmadığı (T-MECH-BUF-03).
- Düşman R vuruşunun istemcide cast kesip kesmediği (T-MECH-CLIENT-03).
- İnsan müttefiklerin taş stoğu bilinemez; diriltme denemesi fail olabilir.

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
