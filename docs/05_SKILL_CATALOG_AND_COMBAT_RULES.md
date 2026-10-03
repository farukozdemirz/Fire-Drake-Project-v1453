# 05 — Skill Kataloğu ve Savaş Kuralları

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Bu doküman **skill verisinin (ID, maliyet, süre, menzil, etki) ve skill bazlı kullanım kurallarının tek kaynağıdır.** Tüm sayısal skill değerleri yerel `FDP_kn_online` veri tabanından otomatik üretilmiş `appendix/A1–A3` tablolarından gelir `[V]`. Sunucunun bu değerleri nasıl uyguladığı [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)'tedir.
> Hiçbir skill adı, ID'si veya değeri elle uydurulmamıştır. Dış kaynaklardaki farklı değerler (ör. 1.298 heal değerleri) `[B]` olarak yalnızca karşılaştırma için anılır.

---

## 1. Veri kaynakları ve doğrulama durumu

| Kaynak | İçerik | Etiket |
|---|---|---|
| `appendix/A1_SKILLS_WARRIOR_106_206.md` | Master warrior (106/206): 58 skill | `[V]` |
| `appendix/A2_SKILLS_PRIEST_112_212.md` | Master priest (112/212): 98 skill | `[V]` |
| `appendix/A3_SKILLS_MAGE_110_210.md` | Master mage (110/210): 92 skill | `[V]` |
| `appendix/data/skills_*.csv` | Tüm sınıf kodları, tüm MAGIC ve tip sütunları | `[V]` |
| `appendix/tools/gen_skill_tables.py` | Tabloları yeniden üreten betik | — |

Doğrulama: veri `[V]`, sunucu uygulaması `[D]`. Skill'lerin gerçek oyunda beklenen etkiyi ürettiği **henüz gözlenmedi** `[A]`. F1'de her profil skill'i için T-MECH-SKILL-* tekil testleri çalıştırılır ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)).

## 2. Kodlama ve birimler `[V]`/`[D]`

- **MagicNum = N CC T LL**: N ulus (1 Karus, 2 El Morad), CC sınıf tipi (MagicNum/1000 = tam sınıf kodu), T kategori (0 temel / 5, 6, 7 ağaçlar / 8 master), LL gereksinim. Örnek: `106560` = Karus master warrior, ağaç 5, 60 puan (sword dancing).
- `MAGIC.Skill = sınıfKodu·10 + kategori`; `SkillLevel` = gereksinim (kategori 0'da karakter seviyesi, diğerlerinde ağaç puanı) (MEC-CHR-01).
- **El Morad ID = Karus ID + 100000.** Satırların büyük çoğunluğu sayısal olarak aynıdır; farklar §3'te.
- Birimler: `CastTime` ve `ReCastTime` 0,1 sn; menzil ve yarıçap metre; Type4 süresi saniye; Type3 süreli hasar `TimeDamage` toplamı, 2 sn'de bir tick.
- `Etc ≠ 0` = quest kapısı (yalnızca Release derleme). Standart profillerde `Etc` 510–523 skill'leri kullanılmaz (CHR-08).
- `BeforeAction` 1–4 ise sınıf taşı (`379058000 + n·1000`) tüketilir ve `UseItem` yalnızca gereksinim olur (03 §4.3 U9).
- `UseStanding = 1` skill'ler hareket halinde kullanılamaz (CLI-09). Örnekler: Howling Sword, Iron Skin, berserk Echo, critical restore, imposingness, Bless of God, Subside, incineration, meteor Fall, Prismatic, ice storm, Stun Cloud, Chain lightning. **Veri notu (2026-10-02, KI-017):** master skill'lerde (incineration, meteor Fall, Prismatic, ice storm, Stun Cloud, Chain lightning, Howling Sword, Bloody Beast, critical restore, imposingness, Bless of God, Subside, Dark pursuer) bu veritabanındaki `UseStanding` değeri 1 değil **51..54**'tür; sunucu yalnızca `== 1`'i hız denetimiyle uygular, bot da öyle (CLI-09 yalnızca `== 1`). Gerçek istemcinin davranışı bilinmiyor `[A]`.

## 3. Karus – El Morad asimetrileri `[V]`

| Skill | Karus | El Morad | Etki |
|---|---|---|---|
| Cleave 106545 / 206545 | %150 | %125 | Karus warrior'ı lehine |
| Malice 112703 / 212703 | menzil 56 | menzil 90 | El Morad priest'i lehine |
| Parasite 112745 / 212745 | menzil 56 | menzil 90 | El Morad lehine |
| Massive 112760 / 212760 | menzil 56 | menzil 90 | El Morad lehine |
| Mage: Ignition, Solid, Static hemisphere, Fire Impact (110xxx) | menzil 56 | menzil 90 | El Morad lehine |

**Sonuç:** Taraflar simetrik değildir. Değerlendirme protokolü her maçı taraf değiştirerek tekrarlar ([16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md) §7). Davranış parametreleri menzili **skill verisinden okur**, sabit kodlamaz.

## 4. BuffType çakışma grupları (buff planlaması için kritik)

Sunucu her hedefte BuffType başına tek kayıt tutar (MEC-BUF-01). Aynı tipten bir kayıt varken yeni **buff** reddedilir (MEC-BUF-02). Yeni **debuff** ise aynı tipteki **buff dahil** her kaydı siler (MEC-BUF-03).

| BuffType | Buff'lar (kaynak) | Debuff'lar | Pratik sonuç |
|---|---|---|---|
| AC (2) | Priest Insensibility serisi, Round Insensibility; mage Frozen armor/shell, Ice barrier; warrior **Defense** (106007) | Malice, Torment | Warrior kendi Defense'ini açarsa priest'in AC buff'ı reddedilir. **Malice/Torment hedefin AC buff'ını siler.** Malice ile Torment aynı tiptir; ikisini birden atmak anlamsızdır. |
| HP_MP (1) | Grace…massiveness, Heapness, Undying, Greatness, imposingness, Superioris, Massive Binder | **Parasite**, Superior Parasite, Reverse life | **Parasite hedefin maks HP buff'ını siler** ve %80'e indirir. Party'de tek HP buff'ı seçilmeli (öncelik [07](07_PRIEST_BEHAVIOR.md)). |
| STATS (7) | Priest Strength; blasting/wildness/eruption; warrior Gain/Rise/Nimble Wind; stat scroll'ları | Confusion | Warrior Gain ile priest Strength çakışır. Confusion stat buff'ını siler. |
| RESISTANCES (8) | Priest Resist all…Fresh mind; mage Resist/Endure/Immunity fire/cold/lightning; direnç potları | — | **Hedef başına tek direnç buff'ı.** Fresh mind (büyü/hastalık/zehir +80) ile Immunity fire (+80 ateş) birlikte olamaz. |
| ATTACK_SPEED (5) | Outrage/Frenzy/berserk Echo | Slow | Slow, warrior'ın saldırı hızı buff'ını siler. Saldırı hızının etkisi yalnızca istemcidedir (MEC-R-04). |
| SPEED (6) | sprint, whipping, hız potları | Cold wave, ice yavaşlatmaları, leg cutting, Scream (Speed=1) | Yavaşlatma sprint'i siler. Hareket etkisi yalnızca istemcide (MEC-BUF-06, CLI-05). |
| DAMAGE (4) | Prayer of god's power, saldırı potları | Massive, Subside | Massive saldırı buff'ını siler. |

Doğrulama: T-MECH-BUF-01..07 ([15](15_TEST_ARENA_SCENARIOS_AND_ACCEPTANCE_CRITERIA.md)).

**Confusion uyarısı `[I]`:** Confusion CHA'yı stat buff'ı olarak −30 düşürür. Büyü hasarı yalnızca **temel** CHA'yı kullanır (MEC-CHR-11). Bu nedenle Confusion'ın mage hasarına etkisi büyük olasılıkla yoktur. Davranışlarda Confusion "hasar düşürücü" olarak puanlanmaz; T-MECH-BUF-08 ile doğrulanana kadar düşük öncelikli kalır.

## 5. Warrior (106/206) — PvP çekirdek skill seti

Tam tablo: `appendix/A1`. Aşağıdaki tablo davranış dokümanının ([06](06_WARRIOR_BEHAVIOR.md)) kullandığı alt kümedir. Hasar sütunu kod formülüyle hesaplanmış **model tahminidir** (W-P, S1, Raptor +7, buff'sız, hedef W-P S1 AC ≈ 857, PvP sonrası ortalama). Ölçülmüş değer değildir; T-MECH-DMG-01 ile doğrulanır.

| ID (Karus) | Ad | Gerek | MP | Recast | Tip/etki | İsabet | Model hasarı | Hasar/MP | Kullanım |
|---|---|---|---|---|---|---|---|---|---|
| 106525 | Carving | Saldırı 25 | 90 | 0,5 sn | T1 %200 | Zar (isabet/kaçınma) | ~370 | **4,1** | Varsayılan sürekli baskı |
| 106535 | prick | Saldırı 35 | 120 | 0,5 sn | T1 %150 | Sabit %100 | ~278 | 2,3 | Kaçınması yüksek hedefe |
| 106545 | Cleave | Saldırı 45 | 150 | 0,5 sn | T1 %150 (El Morad %125) | Zar | ~278 | 1,9 | Yedek |
| 106557 | sword aura | Saldırı 57 | 250 | 0,1 sn | T1 %100 +250 | Sabit | ~268 | 1,1 | Düşük öncelik |
| 106560 | sword dancing | Saldırı 60 | 300 | 0,5 sn | T1 %150 +150 | Sabit | ~328 | 1,1 | Howling kullanılamazken patlama |
| 106570 | Howling Sword | Saldırı 70 | 400 | 0,8 sn | T1 %200 +200, **ayakta** | Sabit | **~437** | 1,1 | Patlama/bitirme penceresi |
| 106520 | leg cutting | Saldırı 20 | 84 | 5,1 sn | T1 %100 + hız %50 (10 sn) | Zar | — | — | Kaçan/kite eden hedef |
| 106802 | Scream | Master 2 | 300 | 10,1 sn | T1 %250 +200 + hız %1 (7 sn, kök) | Sabit | ~529 | 1,8 | Kaçışı kesme, healer'a geçişte kilitleme; Stone of Warrior tüketir |
| 106815 | Exceed Break | Master 15 | 400 | 25,4 sn | T1 %200 + dayanıklılık hasarı | Zar | — | — | Düşük öncelik |
| 106820 | Shock Stun | Master 20 | 250 | 25,2 sn | T1 %175 +175 + yıldırım stun (direnç zarı) | Zar | — | — | Kaçışı/heal'i kesme fırsatı |
| 106001 | sprint | Seviye 1 | 5 | 6,0 sn | T4 hız %150, 10 sn | — | — | — | Yaklaşma, kovalama, geri çekilme |
| 106720/106755 | Outrage / Frenzy | Berserk 20/55 | 60/150 | 9,1 sn | T4 saldırı hızı %120/%130, 30 sn | — | — | — | R aralığını kısaltır (CLI-01) |
| 106730/106750 | restoration / Regeneration | Berserk 30/50 | 105/210 | 25 sn | T3 HoT 750/1500 (60 sn) | — | — | — | Geri çekilme ve solo |
| 106650 | descent | Savunma 50 (W-G) | 50 | 9,1 sn | T8 warp 25: party üyesine ışınlan, r=30 | — | — | — | Peel: tehdit altındaki priest/mage'e git |
| 106630/106645 | Binding / provoke | Savunma 30/45 (W-G) | 30/60 | 6,5/15 sn | T7 (bağlama/kışkırtma) | — | — | — | T7 dönüş hatası (MB-10); sunucu etkisi `[A]` |

Debuff etkisi (aynı model): Malice altındaki hedefe Carving ~460 (tek uygulama modeli) ile ~563 (çift uygulama modeli, MB-04) arasında. T-MECH-DMG-02 (2026-10-02) çift uygulama modelini doğruladı `[V]` (`docs/03` MB-04): Malice altındaki hedefin etkili AC'si ~%56'ya (0,75²) iner; bu yüzden ~563 değeri kullanılır, ~460 değil.

### 5.1 Warrior baskı döngüsünün mekanik sınırları

1. Sunucu **saniyede bir Type1 skill** kabul eder (MEC-MAG-03). Skill recast'leri 0,1–0,8 sn olduğundan pratik sınır tip kapısıdır.
2. Sunucu **saniyede bir başarılı R** kabul eder (MEC-R-07). İstemci tarafında R aralığı silah gecikmesidir: Raptor +7 için 1,64 sn; Outrage ile 1,64/1,2 = 1,37 sn; Frenzy ile 1,26 sn (CLI-01).
3. R ile skill arasında sunucu bağı yoktur (MEC-MAG-06). Bot ikisini aynı pencerede gönderebilir; R ile skill arasında **kilit yoktur** (CLI-02, ölçüldü: komşu aksiyon ≥ ~61 ms, `docs/03` §13.2); bot yalnızca kendi zamanlayıcılarını ve CLI-11 toplam sınırını uygular.
4. Sonuç: W-P için sunucunun izin verdiği teorik tavan saniyede **1 Type1 + en fazla 1 R**'dir. Model tahminiyle hedef başına saniyede ~370–437 (skill) + ~98–117 (R, gecikmeye göre) ≈ **470–550 PvP hasarı/sn** (buff'sız, debuff'sız). Bu sayı davranış tasarımında yalnızca **göreli** karar için kullanılır.
5. MP tüketimi: Carving döngüsü 90 MP/sn, Howling döngüsü 400 MP/sn. W-P'nin ~4438 MP'si Howling ile ~11 sn, Carving ile ~49 sn yeter. Sunucu pot başına 2 sn recast verir (MEC-POT-02) ama botun (ve ölçülen istemcinin) pot aralığı ortak **~2,5 sn**'dir (CLI-06): 1920 MP'lik pottan en çok ~768 MP/sn. **MP sınırını pot stoku belirler** ([11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md)).

### 5.2 "Skill + R" uygulaması

Kural [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md) §13.1'dedir. Warrior için zamanlama şablonu `[Ö]`:

```
her bot tick'inde (100 ms):
  if R_hazir (CLI-01 gecikmesi doldu ve son R farklı sunucu saniyesinde; skill ile R arasında kilit yok)
       and hedef R menzilinde: R gönder
  if Type1_hazir (farklı sunucu saniyesi ve skill recast doldu ve ayakta-şartı sağlandı)
       and hedef skill menzilinde: seçilen Type1 skill'i gönder (R kilidi başlatılmaz)
```

"Aynı saniye" kontrolü sunucu saatinin (`UNIXTIME`) bot tarafından okunmasıyla yapılır. Böylece sunucunun reddedeceği aksiyon hiç gönderilmez (MET-ACT-02 hedefi ≤ %2).

## 6. Priest (112/212) — PvP çekirdek skill seti

Tam tablo: `appendix/A2`. Tüm heal'ler sabit değerlidir (MEC-CHR-12). Element niteliği 0 olduğu için tip kapısına girmezler (MEC-MAG-04); kısıt yalnızca kendi recast'leri ve istemci cast süresidir (1,5 sn, CLI-03).

| ID (Karus) | Ad | Gerek | MP | Cast/Recast | Hedef | Etki | HP/MP | Kullanım |
|---|---|---|---|---|---|---|---|---|
| 112527 | Great healing | Heal 27 | 80 | 1,5 / 2,0 | Dost tek | +960 HP | **12,0** | Varsayılan verimli heal |
| 112536 | Massive healing | Heal 36 | 160 | 1,5 / 0,1 | Dost tek | +960 HP | 6,0 | Great healing recast'teyken |
| 112545 | Superior healing | Heal 45 | 320 | 1,5 / 0,1 | Dost tek | +1920 HP | 6,0 | Büyük eksik |
| 112554 | Complete healing | Heal 54 | 960 | 1,5 / 5,4 | Dost tek | +10000 HP (tam doldurur) | eksik/960 | Acil kurtarma (eksik ≥ ~3000) |
| 112557 | Group massive healing | Heal 57 | 960 | 1,5 / 5,4 | Party, r=30 | +960 her üyeye | — | ≥ 3 üye eksik |
| 112560 | Group complete healing | Heal 60 | 1920 | 1,5 / 6,4 | Party, r=30 | tam | — | Toplu acil durum |
| 112548 | Superior restore | Heal 48 | 625 | 1,5 / 0,1 | Dost tek | HoT 2500 / 30 sn | — | Önceden heal (hedefte HoT yoksa, MEC-T3-01) |
| 112525 | Cure curse | Heal 25 | 60 | 1,5 / 1,5 | Dost tek | Tüm debuff'ları kaldırır (REMOVE_TYPE4) | — | Kritik debuff |
| 112535 | Cure disease | Heal 35 | 120 | 1,5 / 1,5 | Dost tek | DoT'ları kaldırır | — | Ağır DoT (blooding, Hell fire) |
| 112703 | Malice | Curse 3 | 40 | 1,5 / 7,4 | Düşman | AC %75, 150 sn; **AC buff'ını siler** | — | Ortak hedef açıcı (P-HD) |
| 112757 | Torment | Curse 57 | 150 | 1,5 / 9,4 | Alan r=10 | AC %70, 150 sn | — | Kümelenmiş düşman |
| 112745 | Parasite | Curse 45 | 100 | 1,5 / 7,4 | Düşman | Maks HP %80; **HP buff'ını siler** | — | Buff'lı hedef |
| 112760 | Massive | Curse 60 | 180 | 1,5 / 10,4 | Düşman | Saldırı %80 | — | Düşman warrior baskısı |
| 112724 | Slow | Curse 24 | 120 | 1,5 / 7,4 | Düşman | Saldırı hızı %70 (istemci) | — | Düşman warrior |
| 112736 | Sweep mana | Curse 36 | 160 | 1,5 / 7,4 | Düşman | −960 MP | — | Düşman priest/mage MP'si |
| 112733/112742/112754 | Resurrection of love/grace/favors | Curse 33/42/54 | 400/600/800 | 1,5 / 25 | Ceset, menzil 11 | Diriltme; taş 4/10/30 **hedeften** | — | Ölü üye (bkz. [07](07_PRIEST_BEHAVIOR.md)) |
| 112660 | Insensibility peel | Buff 60 | 150 | 1,5 / 0,1 | Dost tek | AC +300, 600 sn | — | P-HB |
| 112657 | massiveness | Buff 57 | 360 | 1,5 / 0,1 | Party üyesi | Maks HP +1500, 600 sn | — | P-HB |
| 112654 | Undying | Buff 54 | 240 | 1,5 / 0,1 | Party üyesi | Maks HP %160 | — | Alternatif HP buff'ı (çakışır) |
| 112656 | Greatness | Buff 57 | 570 | 1,5 / 0,1 | Party alan r=30 | Maks HP +1200 | — | Toplu yenileme |
| 112645 | Fresh mind | Buff 45 | 60 | 1,5 / 0,1 | Dost tek | Büyü/hastalık/zehir R +80 | — | Mage'e karşı (tek direnç buff'ı kuralı) |
| 112820 | Curse Refraction | Master 15 | 320 | 1,5 / 0,1 | Kendisi | Debuff'ları %25 yansıt, aksi engelle, 10 sn | — | Debuff baskısı altında |
| 112825 | Elysian Web | Master 20 | 640 | 1,5 / 0,1 | Dost alan r=15 | Büyü hasarı azaltma, 20 sn; Stone of Priest gerektirir | — | Düşman mage patlaması |
| 112802/112815 | Judgment / Helis | Master 2/12 | 200/350 | 0 / 0,5 | Düşman | T1 %500 +150 / %400 +400 | — | Priest'in kendini savunması, solo |

## 7. Mage (110/210) — PvP çekirdek skill seti

Tam tablo: `appendix/A3`. Elementli Type3 skill'ler tip kapısına tabidir: saniyede bir Type3 (MEC-MAG-03). Cast süreleri genellikle 1,5 sn'dir (CLI-03). Pratik tempo, cast süresine göre **~0,67 büyü/sn**'dir.

| ID (Karus) | Ad | Gerek | MP | Cast/Recast | Menzil | Etki | Kullanım |
|---|---|---|---|---|---|---|---|
| 110551 | Pillar of fire | Ateş 51 | 160 | 1,5 / 5,3 | 56 | −1260 ateş | Ana tek hedef |
| 110570 | incineration | Ateş 70 | 390 | 1,1 / 21,3 | 45 | −2500 ateş, **ayakta** | Açılış/bitirme patlaması |
| 110571 | meteor Fall | Ateş 70 | 600 | 1,3 / 18,3 | 45 | Alan r=15 −2100 + DoT, ayakta | ≥ 3 düşman kümesi |
| 110560 | Supernova | Ateş 60 | 400 | 1,5 / 15,3 | 56 | Alan r=15 −1800 + DoT | Küme |
| 110533 | Fire burst | Ateş 33 | 150 | 1,5 / 0,1 | 90 | Alan r=8 −588 | Sürekli alan baskısı |
| 110515/110527 | Fire ball / Fire spear | Ateş 15/27 | 50/80 | 1,5 / 4,3 | 78 | −308 / −588 | Uzun menzil dolgu |
| 110557 | Fire Impact | Ateş 57 | 220 | 1,5 / 20,3 | 56 | −1260 + DoT; scroll 379070000 | Patlama |
| 110651 | Ice comet | Buz 51 | 160 | 1,5 / 5,3 | 56 | −882 + hız %32 (19 sn) | Kaçan hedef (M-F de alır) |
| 110660 | Frost nova | Buz 60 | 400 | 1,5 / 15,3 | 56 | Alan r=15 −1260 + hız %30 (20 sn) | M-I: alan kontrol |
| 110670 | Prismatic | Buz 70 | 390 | 1,1 / 21,3 | 45 | −1750 + yavaşlatma, ayakta | M-I patlaması |
| 110645 | Blizzard | Buz 45 | 200 | 1,5 / 15,3 | 56 | Alan r=15 −353 + hız %34 | Kaçışı kesme |
| 110004 | **summon friend** | Seviye 4 | 5 | 1,5 / 0,1 | 22500 | T8 warp 12: party üyesini çağıranın yanına | Ölüp respawn olan üyeyi geri getirme ([08](08_MAGE_BEHAVIOR.md)) |
| 110802 | Absolute power | Master 2 | 240 | 0 / 25 | — | Büyü gücü %130, 30 sn; scroll + Stone of Mage | Patlama penceresi |
| 110815 | Mana Shield | Master 12 | 150 | 0 / 0 | — | Hasarın bir kısmı MP'den, 40 sn | Melee baskısında |
| 110820 | Instantly Magic | Master 15 | 100 | 0 / 25,5 | — | Sonraki cast cooldown kaydı yok | Patlama kombosu |
| 110825 | Minor Resist | Master 20 | 450 | 1,3 / 0,1 | 56, r=10 | Düşman dirençleri −20, 10 sn | Ortak hedef kümesi öncesi |
| 110612/110630/110654 | Frozen armor/shell/Ice barrier | Buz 12/30/54 | — | 1,5 / 0,1 | 56 | AC +60/+120/+180 | **Priest AC buff'ıyla çakışır** (§4) |
| 110015 | Gate | Seviye 15 | 30 | 1,5 / 10 | — | Bind/başlangıca ışınlanma | Ronark'ta yalnızca Escape (`110035`) engelli, Gate `110015` engelli değil (kod, MEC-T8-04); **ölçüldü (F4-35, 2026-10-03): Gate Ronark'ta çalışır, tam ulus başlangıcına (Karus (1380, 1090), El Morad (630, 920)) ışınlar** `[V]` |

## 8. Skill kullanım kuralları (tüm sınıflar)

| Kimlik | Kural |
|---|---|
| SK-01 | Bot bir skill'i göndermeden önce şunları kontrol eder: sınıf/ağaç gereksinimi (sabit), MP ≥ maliyet + rol rezervi, recast (gerçek ms, CLI-04), tip kapısı (sunucu saniyesi), menzil (skill verisi), hedefin canlı/görünür/düşman ya da dost olması (moral), ayakta şartı, gereken item/taş stoğu. |
| SK-02 | Cast süresi olan skill'lerde CASTING → bekle → EFFECTING. Bekleme sırasında hedef menzilden çıkar veya ölürse EFFECTING gönderilmez. Bu "kesilme"dir: MP harcanmaz çünkü sunucu MP'yi EFFECTING'de düşer (MEC-MAG-08). Bot cast sırasında hareket etmez. Gerçek istemcide hareketin cast'i iptal ettiği varsayılır `[A]`. |
| SK-03 | Buff uygulanmadan önce hedefte aynı BuffType'ın olup olmadığı **gözlenen olaylardan** kontrol edilir (§4). Varsa buff gönderilmez. İstisna: kalan süre `P-PRI-BUFF-REFRESH`'ten azsa, süre dolumunu bekleme planı yapılır. Sunucu, buff süresi dolmadan aynı tipten yeniden uygulamaya izin vermez (MEC-BUF-02). |
| SK-04 | Debuff başarısı sunucunun gönderdiği sonuç paketiyle doğrulanır. Fail paketi veya hedefin Counter Curse/Curse Refraction durumu "başarısız" sayılır. Takım çağrısı yalnızca başarılı sonuçta yapılır ([09](09_PARTY_COORDINATION_AND_TARGET_SELECTION.md)). |
| SK-05 | Yavaşlatma/stun (SPEED/STUN) debuff'larının oyuncu hedefte direnilme şansı yüksektir (MEC-BUF-05). Bot, hedefin hareket hızını yalnızca **gözlenen** hareketinden çıkarır; debuff'ı "kesin etkili" saymaz. |
| SK-06 | Alan skill'lerinde hedef noktası çağıranın menzili içinde seçilir (CLI-07). Alan heal'leri yalnızca party üyelerini etkiler (PARTY_ALL). |
| SK-07 | Freeze altındaki hedefe hiçbir hedefli skill gönderilmez (03 §4.3 C8). |
| SK-08 | Quest kapılı (`Etc` 510–523) skill'ler standart profilde **yoktur** (CHR-08). |
| SK-09 | Skill verisindeki menzil sunucu tarafında katıdır (`mesafe < sRange`). Bot `P-SK-RANGE-MARGIN` (varsayılan 1,5 m) kadar içeriden kullanır. |
| SK-10 | Bir aksiyon `ACTION_RESULT` ile doğrulanmadan aynı skill tekrar gönderilmez. Zaman aşımı 1500 ms'dir; sonra `TIMEOUT` loglanır. |

## 9. Doğrulama planı

| Test | İçerik |
|---|---|
| T-MECH-SKILL-W/P/M-* | Her çekirdek skill: MP düşümü, recast, menzil sınırı, etki, fail sebebi |
| T-MECH-BUF-01..08 | §4 çakışma grupları, debuff'ın buff'ı silmesi, Confusion etkisi |
| T-MECH-DMG-01..03 | Warrior skill hasar dağılımı (model ile karşılaştırma), Malice tek/çift uygulama, büyü hasarı CHA ölçeği |
| T-MECH-T8-01..02 | summon friend koşulları; Gate'in Ronark'ta çalışıp çalışmadığı |
| T-MECH-CLIENT-01..03 | Gerçek istemcinin R/skill/cast zamanlaması (CLI-01..03) |

### 9.1 Bot koşusu: Karus priest dost hedefli/kendine/party skill'leri (F4-42, 2026-10-03)

Bot koşusu (`tools/skill-script-gen.py` → `bots/config/skill_priest_k.txt`, 30 adım; `BotPHD_K`, `BotPHB_K`, hedef `BotWP_K`/`BotWG_K`; `[BOT] TELEMETRY=decisions`; `tools/skill-check.py --min-n 2`, gerçek `MAGIC`). Betik hatasız yüklendi, 30/30 adım zamanında çalıştı (en geç gecikme 107 ms) `[V]`. Örnek sayısı skill başına 1-3'tür; sonuçlar **ilk ölçüm**dür.

| Skill | Başlayan / etkili | `Msp` ↔ ölçülen MP düşümü (en büyük) | Recast (`ReCastTime` ↔ en küçük ardışık aralık) | Not |
|---|---|---|---|---|
| 112527 Great healing | 2 / 2 | 80 ↔ 80 | 2000 ↔ 3746 ms | MP bandı 40-80 (aşağıdaki MP yenilenmesi notu) |
| 112536 / 112545 Massive / Superior healing | 2 / 2 | 160 ↔ 160; 320 ↔ 320 | 100 ↔ 2744 / 2734 ms | |
| 112548 Superior restore | 2 / 1 | 625 ↔ 585 (tek etkili örnek) | - | 2. tur `srv_fail` (`-100`); hedef o sırada tam canlıydı `[Ö]` |
| 112554 Complete healing | 3 / 3 | 960 ↔ 960 | 5400 ↔ 7111 ms | |
| 112557 / 112560 Group massive / complete healing | 3 / 3; 2 / 2 | 960 ↔ 960; 1920 ↔ 1920 | 5400 ↔ 7117; 6400 ↔ 8084 ms | party-all; party tam kurulmadı (aşağıda) |
| 112525 / 112535 Cure curse / disease | 2 / 2; 2 / 2 | 60 ↔ 60; 120 ↔ 120 | 1500 ↔ 3193 / 3185 ms | |
| 112660 / 112645 / 112654 | 3 / 3 her biri | 150 ↔ 150; 60 ↔ 60; 240 ↔ 240 | 100 ↔ 3181-3186 ms | buff, hedef başına tek atış |
| 112657 massiveness / 112820 Curse Refraction | 1 / 1 | 360 ↔ 360; 320 ↔ 280 (tek örnek) | - | |
| 112656 Greatness (`Moral` 6, self) | 1 / 0 | - | - | `CastEffect` gönderildi, **sonuç paketi gelmedi** (`no_result`) |

Bulgular (her biri bir sonraki planın girdisidir):

1. **MP yenilenmesi ölçümü bozuyor `[V]`:** bot MP'si cast sırasında kümeli +20/+40 yenilenir; `skill-check.py` `mp_verdict` FAIL'leri (112527, 112535, 112536, 112560, 112645) tek örnekte `Msp` − yenilenme farkıdır, **en büyük düşüm hepsinde `Msp`'ye eşittir**. Araç `mp_delta_max` ile hüküm vermeli ya da yenilenme payı tanımlanmalı.
2. **Party kurulumu tamamlanmadı `[V]`:** `raw` adımlarında `pinvite`→`paccept` arası 1000 ms yetmiyor; `BotWP_K` ve `BotWG_K` `paccept` `CLI-15 accept_wait` (`value` 996/989, `limit` 1000) ile reddedildi (yalnızca `BotPHB_K` girdi). Moral 4/6 skill'lerin party üyesi hedefli ölçümü bu yüzden geçerli sayılmaz; `raw` aralığı ≥ 1500 ms olmalı.
3. **Bot konumu betikten bağımsız `[V]`:** botlar farklı yerlerde doğdu (`BotWP_K`/`BotWG_K` priestlerden ~400 m uzakta; DB'deki son konum nedeniyle `[Ö]`); betikten önce `move` ile 56 m içine getirilmesi gerekti. Betik konumlandırma yapmaz (F4-42 kapsam dışı).
4. **Hedef canı:** `112548` 2. turu `srv_fail`; tam canlı hedefe heal'in sunucuda reddedildiği düşünülüyor `[Ö]` (heal örneklerinden önce hedefin yaralanması gerekir).
5. **112656 Greatness** sonuç paketi alınmadı `[V]`; nedeni araştırılmadı `[Ö]`.

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
