# 04 — Karakter Profilleri, Stat Dağılımı ve Ekipman

> Durum: Taslak v1.0 · Araştırma tarihi: 2026-10-01 · Referans commit: `0f52027`
> Bu doküman **karakter bütçelerinin, rol profillerinin ve referans ekipman setlerinin tek kaynağıdır.** Mekanik kurallar [03](03_VERSION_COMPATIBILITY_AND_VERIFIED_MECHANICS.md)'ten (MEC-CHR-*), skill verisi [05](05_SKILL_CATALOG_AND_COMBAT_RULES.md) ve `appendix/A1–A3`'ten gelir.
> Hesaplanan HP/MP/saldırı değerleri koddaki formüllerle yapılmış **yaklaşık** değerlerdir (`[D]` formül + aritmetik). Item ve buff katkısı ayrıca belirtilir. Çalışma zamanında T-DATA-03 ile doğrulanır.

---

## 1. Genel kurallar

| Kimlik | Kural | Dayanak |
|---|---|---|
| CHR-01 | Tüm botlar level 80 ve **master sınıf kodu** ile oluşturulur: warrior 106/206, mage 110/210, priest 112/212. | MEC-CHR-01, MEC-CHR-03 |
| CHR-02 | Botlar ve test rakipleri Authority 1 olur (GM değil). | MEC-CHR-05 |
| CHR-03 | Stat toplamı 577; her stat sınıfın sıfırlama ön ayarının altına inemez, 255'i geçemez. | MEC-CHR-06/07/09 |
| CHR-04 | Skill puanı toplamı 142; ağaç başına ≤ 80; master ≤ 20. | MEC-CHR-08 |
| CHR-05 | Botlar yalnızca gerçek istemcinin o sınıfa kuşattığı ekipmanı kullanır. Sunucu sınıf kontrolü yapmasa da (MEC-CHR-04) istemcinin sınıf kısıtı uygulandığı varsayılır `[A]`. Kural: `ITEM.Class` ve `Kind` sınıfla uyumlu olmalı (warrior zırhı Kind 210, priest 240, mage 230). | `[Ö]` |
| CHR-06 | Ekipman gereksinimleri **temel** statlara uygulanır; item ve buff statları gereksinimi karşılamaz. | MEC-CHR-04, [`GameServer/ItemHandler.cpp:527-539`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/ItemHandler.cpp#L527-L539) `[D]` |
| CHR-07 | Değerlendirme maçlarında iki taraf aynı referans ekipman setini kullanır (§6). | [16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md) §7 |
| CHR-08 | Quest kapılı skill'ler (`Etc` 510–523) **standart profillerde kullanılmaz**; ilgili questlerin bu kurulumda tamamlanabilirliği doğrulanınca "ileri profil" olarak açılır (ADR-0002, Q-04). | [05](05_SKILL_CATALOG_AND_COMBAT_RULES.md) §2 |

## 2. Irk ve sınıf uygunluğu

Sunucu ırk–sınıf kombinasyonunu doğrulamaz (MEC-CHR-04). Aşağıdaki kombinasyonlar kod tabanındaki ulus/meslek değişimi eşlemelerinden çıkarılmıştır `[D]`/`[I]` (`GameServer/CharacterHandler.cpp:49-95,171-432`):

| Sınıf | Karus ırkları | El Morad ırkları | Bot için seçim |
|---|---|---|---|
| Warrior | 1 Arch Tuarek | 11 Barbarian, 12 El Morad erkek, 13 El Morad kadın | Karus 1, El Morad 11 |
| Priest | 2 Tuarek, 4 Puri Tuarek | 12, 13 | Karus **4** (bkz. not), El Morad 12 |
| Mage | 3 Wrinkle Tuarek (4 ulus/meslek değişimiyle) | 12, 13 | Karus 3, El Morad 12 |

Not: Stat sıfırlama kodunda Karus ırk 2 (Tuarek) priest'e rogue ön ayarı uygulanıyor ([`GameServer/User.cpp:3962-3978`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L3962-L3978)) `[D]`. Bu tuhaflıktan kaçınmak için Karus priest botları ırk 4 ile oluşturulur.

Irkın bu sunucuda savaş değerlerine doğrudan etkisi bulunamadı. Statlar sınıf ön ayarı ve dağıtımla belirlenir; set item kimliği ise `Race ≥ 100` alanını kullanır `[D]`.

## 3. Bütçeler

### 3.1 Stat bütçesi `[D]`

- Toplam: `T(80) = 300 + 3·79 + 2·20 = 577`.
- Sıfırlama sonrası sınıf ön ayarı (toplam 290) + **287 serbest puan**.

| Sınıf | Ön ayar STR/STA(HP)/DEX/INT/CHA(MP) |
|---|---|
| Warrior | 65 / 65 / 60 / 50 / 50 |
| Priest | 50 / 50 / 70 / 70 / 50 |
| Mage | 50 / 60 / 60 / 70 / 50 |

Kaynak: [`GameServer/User.cpp:3908-4158`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L3908-L4158). İsimlendirme: istemcide "HP" statı = kodda STA, "MP" (Magic Power) statı = kodda CHA. INT, MP havuzunu belirler.

### 3.2 Skill bütçesi `[D]`

- Toplam 142 = `2·(80−9)`.
- Ağaç 5/6/7 her biri ≤ 80; master (8) ≤ 20.
- İki ağacı birden 80'e çıkarmak mümkün değildir (160 > 142). Tipik dağılım: **ana ağaç + ikinci ağaç + master 20**.

### 3.3 Bot karakterlerinin oluşturulması `[D]`/`[Ö]`

İstemciyle oluşturulan karakter başlangıç sınıfında kalır (MEC-CHR-03). `CREATE_NEW_CHAR` varsayılanları seviye 1, Loyalty 500, Authority 1, zone 21'dir `[V]`. `start.md`'deki "oluşturma sonrası seviye 51" gözlemi DB, prosedür veya kodla açıklanamadı (Q-03).

Öneri (ADR-0002): Bot karakterleri **sunucu kapalıyken** bir kurulum betiğiyle USERDATA'ya yazılır. Betik `Level=80`, master `Class`, statlar, `Points=0`, `strSkill` (bayt 0 = serbest, 5–8 = ağaçlar), `strItem` (§6), `Zone=71`, `Loyalty>0` alanlarını doldurur ve WAREHOUSE satırını oluşturur. Betik, yazdığı her satır için CHR-03/CHR-04 değişmezlerini doğrular (T-DATA-01). Girişte sunucu bu alanları yeniden doğrulamaz (MEC-CHR-09); bu yüzden doğrulama betiğin sorumluluğundadır.

### 3.4 Kurulum betiği ve veri biçimi (F1-04) `[V]`

Betik: `db/002_bot_characters.sql` (+ `002_bot_characters_rollback.sql`, kullanım `db/README.md`); 12 karakter (6 profil × 2 ulus), `Upgrade` kademesi 0/7/8 (S0/S1/S2). Gerçek veritabanında doğrulandı: stat 577, skill 142, `strItem` ve `strSkill` baytları çözülüp planla karşılaştırıldı, `LOAD_USER_DATA` bot satırını döndürüyor, betik tekrar çalıştırılabilir.

- **`ACCOUNT_CHAR` şart:** `LOAD_USER_DATA`, karakterin hesabın `strCharID1..3` alanında listelenmesini ister; bot başına bir `BotAcc<PROFIL><K|E>` (örn. `BotAccWPK`) hesap satırı + `WAREHOUSE` satırı yazılır. **Hesap adı yalnızca harf ve rakam içerebilir** (`LoginServer` `WordGuardSystem`, `LogInServer/LoginSession.cpp:144-158`; alt çizgi reddedilir: "No such registered ID"). `TB_USER` satırını yazmaya gerek yoktur: `ACCOUNT_LOGIN` ilk girişte hesabı otomatik açar (verilen şifre kaydedilir).
- **`strSkill` (varchar(10), ham bayt):** bayt 0 = serbest puan, 5–7 = ağaçlar, 8 = master. Ağaç anlamları (MAGIC verisi): warrior 106: 5 saldırı, 6 savunma, 7 berserk; priest 112: 5 heal, 6 buff/koruma, 7 debuff/lanet; mage 110: 5 ateş, 6 buz, 7 yıldırım. Profiller: WP [70,0,52,20], WG [60,62,0,20], PHD [60,0,62,20], PHB [60,62,0,20], MF [70,52,0,20], MI [52,70,0,20].
- **`strItem` (binary(584)):** 73 yuva × 8 bayt, `<int32 itemID, int16 dayanıklılık, int16 adet>` little-endian; yuvalar `shared/globals.h:193-206` (0 RIGHTEAR, 1 HEAD, 2 LEFTEAR, 3 NECK, 4 BREAST, 5 SHOULDER, 6 RIGHTHAND, 7 WAIST, 8 LEFTHAND, 9 RIGHTRING, 10 LEG, 11 LEFTRING, 12 GLOVE, 13 FOOT), çanta 14–41. Zırh parçası son 3 hane öncesi: `…001`=BREAST, `…002`=LEG, `…003`=HEAD, `…004`=GLOVE, `…005`=FOOT. `strSerial`/`strItemTime` sıfır bırakılır (sunucu seri üretir).
- **Seviye 80 `Exp`** `LEVEL_UP`'tan okunur (1898706631). Girişte `LOAD_USER_DATA` başlangıç sınıfı kodlarını (105/107/109/111, 205/…) seviye > 59 ise master'a çevirir; betik zaten master kodu yazar.
- **Kuşanılabilirlik ve ağırlık (F1-05, `tools/bot-gear-report.py`) `[V]`:** 12 botun 150 ekipman parçasının hepsi sunucu kuşanma koşulunu (`ItemEquipAvailable`, temel stat) karşılıyor (`fail_count=0`). Hiçbir ekipmanda `Race` kısıtı yok; sınıf kısıtlı 15 parça `Class` 6 (warrior), 10 (mage), 12 (priest). Ekipmandan gelen bonuslar (S1): warrior STR+29/STA+34/HP+260 (AC 715 WP, 871 WG), priest STR+5/STA+34/DEX+14/INT+20/HP+250/MP+180 (AC 796), mage STR+5/STA+34/INT+20/CHA+14/HP+150/MP+180 (AC 525). Ağırlık: çanta dahil toplam 10470 (mage), 10680 (priest), 10867–10877 (warrior); maks ağırlık tabanı (çarpansız) 6750 / 10250 / 18200; envanter şablonundaki `389015000 ×100` (1440 HP pot, ağırlık 100) tek başına 10000 ağırlık ekliyor. `m_bMaxWeightAmount` başlatılmadığı için gerçek maks ağırlık 0, tabanın 1 veya 2 katı olabilir (MB-12, çalışma zamanında ölçülecek).
- **Başlangıç HP/MP (F1-06) `[D]`:** Sunucu girişte (`SelectCharacter` → `SetUserAbility`) `SetMaxHp`/`SetMaxMp` ile geçerli değeri yalnızca **maksa indirir**, yükseltmez (`User.cpp:1073-1076`, `:1108-1111`). Bu yüzden betik `Hp`/`Mp` değerlerini bilerek `32000` yazar; giriş kırpması botları tam can/manayla başlatır. Çalışma zamanında doğrulanacak `[A]`.
- **Hâlâ ölçülecek:** ulus kısıtlı item'ların istemci etkisi ve istemcide kuşanılabilirlik (T-DATA-02, Q-05).

## 4. Türetilmiş değerler (level 80, item ve buff hariç)

Formüller `[D]`:

- Maks HP = `HPkatsayı·L²·STA + 0,1·L·STA + STA/5 + 20` ([`GameServer/User.cpp:1036-1071`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1036-L1071)).
- Maks MP (caster) = `MPkatsayı·L²·(INT+30) + 0,2·L·(INT+30) + (INT+30)/5 + 20`.
- Warrior MP'si `SP` katsayısıyla STA'dan hesaplanır ([`GameServer/User.cpp:1076-1103`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L1076-L1103)).
- Saldırı (warrior/priest) = `0,010·D·(STR+40) + silahKatsayı·D·L·STR + 3 + (temelSTR−150)` ([`GameServer/User.cpp:2166-2205`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/User.cpp#L2166-L2205)).
- Büyü hasarı ölçeği = `temelCHA/186` ve düz ek `max(0, CHA−86)` (MEC-CHR-11).

Katsayılar yerel COEFFICIENT tablosundan alınmıştır `[V]`: master warrior HP 0,003 / SP 0,003 / silah 0,00032; master priest HP 0,0015 / MP 0,0015 / topuz 0,00025; master mage HP 0,001 / MP 0,0018 / asa 0,00015.

| Profil | STR/STA/DEX/INT/CHA | Maks HP | Maks MP | Saldırı (silahla) | INT direnç bonusu | STA AC bonusu |
|---|---|---|---|---|---|---|
| W-P / W-G | 255/162/60/50/50 | ~4458 | ~4438 (SP) | Raptor +7: ~1766 · Graham +7: ~1036 | 0 | +62 |
| P-HD / P-HB | 120/147/70/190/50 | ~2636 | ~5696 | Priest Impact +7: (düşük, melee hedeflenmez) | +45 | +47 |
| M-F | 50/60/60/160/247 | **~896** | ~5286 | – | +30 | 0 |
| M-I | 50/107/60/160/200 | ~1582 | ~5286 | – | +30 | +7 |

**Ekipmanlı değerler ve fiziksel hasar modeli (F1-06, `tools/stat-model.py`) `[D]`/`[I]`:** Sunucu formülleri bire bir uygulanır (tam sayı ve 32 bit float semantiği), yukarıdaki ekipmansız tablo 13/14 satırda **aynen** yeniden üretildi (tek fark M-I Maks HP 1581: `(short)` kırpması, tablo yuvarlanmış). Girişteki gerçek değerler (S1 ekipman, buff yok):

| Profil | Maks HP | Maks MP | Toplam saldırı | Toplam AC |
|---|---|---|---|---|
| W-P | 5650 | 5370 | 1947 | 857 |
| W-G | 5650 | 5370 | 1138 | 1488 |
| P-HD / P-HB | 3491 | 6392 | 418 | 923 |
| M-F | 1541 | 6021 | 57 | 605 |
| M-I | 2228 | 6021 | 57 | 612 |

R vuruşu, hedef profil başına isabette ortalama hasar (oyuncuya, `/2` ve silah direnci sonrası): W-P → W-G 73, → W-P 159, → P 108, → M-F 207; W-G → W-P 93, → W-G 42, → P 63, → M-F 121; priest → W-P 34, → M 44; mage (asa, fiziksel) ≈ 3–6. Warrior Type1 skill'leri R'ye göre ×1,0–2,0 (sHit %100–200; ör. W-P `Carving` → M-F 476). Tam tablolar: `python3 tools/stat-model.py`. Referans ekipmanda elemental/drain sütunları ve `ITEM_OP` proc kaydı yoktur; model bunları atlar. Çalışma zamanı ölçümü (T-MECH-DMG-01, ± %15) yapılmadan etiketler `[V]` olmaz.

**Büyü hasarı ve heal modeli (F1-07, `tools/spell-model.py`) `[D]`/`[I]`:** `MagicInstance::GetMagicDamage` ve `ExecuteType3` formülleri tam sayı/float semantiğiyle uygulandı; `docs/04` yukarıdaki "incineration" örneği asasız satırlarda ±0,1 içinde yeniden üretildi. Asa (sağ el asa, sol el boş, ateş/buz/yıldırım büyüsü) hasara `(0,8×Damage + Damage×Level/60)` ekler (Elixir Staff +7: +236, oyuncuya `/3` sonrası ≈ +79). Örnekler (M-F, CHA 247, S1 ekipman, ortalama / en düşük–en yüksek, bir cast):

| Büyü (MP, cast / recast) | → W-P | → W-G | → P-HD | → M-F |
|---|---|---|---|---|
| incineration (390, 1,1 / 21,3 sn) | 953 (829–1076) | 768 | 940 | 980 (853–1107) |
| meteor Fall (600, 1,3 / 18,3 sn) | 821 | 666 | 810 | 844 |
| Supernova (400, 1,5 / 15,3 sn; ek DoT 10 × 32) | 723 + 320 | 590 | 714 | 743 + 330 |

M-I (CHA 200, Prismatic buz, 390 MP): → W-P 650, → M-F 540. Maks HP'ye göre (S1): M-F ~1541 → incineration ile 2 cast, W-P 5650 → 6 cast. Heal (stat ölçeği yok, `sFirstDamage` olduğu gibi): Complete healing 10 000 (960 MP, recast 5,4 sn, MP başına 10,4), Group complete healing 10 000 (r=30, 1920 MP), Superior restore HoT 2490 (15 tick × 166). Priest P-HD/P-HB'nin Type3 saldırı büyüsü yoktur; hasarları R iledir. Tam tablolar: `python3 tools/spell-model.py`. Çalışma zamanı ölçümü (T-MECH-DMG-03, ± %15) yapılana kadar etiketler `[V]` olmaz.

Yorum:

- Master mage'in STA başına HP'si 14,6'dır (warrior 27,4). Mage canlılığı büyük ölçüde item HP bonuslarına ve priest buff'larına bağlıdır (ör. massiveness +1500, Undying %160) [05](05_SKILL_CATALOG_AND_COMBAT_RULES.md). Bu, mage'in **solo PK'da kırılgan** olacağını ve party'de priest buff'ı olmadan savaşa girmemesi gerektiğini gösterir ([08](08_MAGE_BEHAVIOR.md), [10](10_SOLO_PK_BEHAVIOR.md)).
- Büyü hasarı örneği (incineration, −2500, kod formülü, ortalama, PvP /3 sonrası; hedef direnci R): CHA 247 → R 0: ~1071, R 100: ~780, R 200: ~619. CHA 200 → R 0: ~862, R 100: ~626, R 200: ~495 `[D]` formül + `[I]` aritmetik. M-F ile M-I arasındaki hasar farkı yaklaşık %25'tir; HP farkı ise yaklaşık %77'dir.

## 5. Rol profilleri

Her profil bir **rol profili kimliği** taşır ve öğrenme ([14](14_LEARNING_AND_ADAPTATION.md)) ile telemetri ([16](16_TELEMETRY_DEBUGGING_AND_PERFORMANCE.md)) bu kimliği kullanır.

### 5.1 W-P — `warrior.pressure` (baskı warrior'u)

| Alan | Değer |
|---|---|
| Savaş amacı | Ortak hedefe sürekli yakın dövüş baskısı, kaçanı yavaşlatma, düşman healer'ını zorlama |
| Irk | Karus 1 / El Morad 11 |
| Stat | STR 255 (+190), STA 162 (+97), DEX 60, INT 50, CHA 50 → 577 ✓ ("255 STR, kalan HP" bu sunucuda **geçerli**: Chitin Shell +7/+8 ve Raptor +7/+8 gereksinimleri karşılanıyor) |
| Skill | Saldırı ağacı 70 + Berserk 52 + Master 20 = 142 ✓ |
| Saldırı 70 ile açılanlar | hoodwink 5, pierce 15, leg cutting 20, Carving 25, prick 35, Cleave 45, thrust 55, sword aura 57, sword dancing 60, Howling Sword 70 |
| Berserk 52 ile açılanlar | Gain 5, Outrage 20 (saldırı hızı %120), restoration 30, Return to life 40, Regeneration 50 |
| Master 20 | Scream 2, Absoluteness 5, Matchless 10 (alınan hasar ×0,85, MEC-CHR-16), Exceed Break 15, Shock Stun 20 |
| Ön koşullar | Sınıf 106/206; Scream için Scream Scroll `379063000` (gereksinim, tüketilmez) ve Stone of Warrior `379059000` (tüketilir); Exceed Break ve Shock Stun için Stone of Warrior |
| Silah | Raptor (2H mızrak, Kind 52) — `[S]` dönem rehberi "warrior için en iyi silah"; sunucu verisi: +7 hasar 175, gecikme 164, menzil 20, STR ≥ 224 |
| Güçlü | Yüksek hasar, tüm Type1 skill'lerin AP ile ölçeklenmesi, 1 m'den 20 m'ye kadar uzanan menzil (15 + silah menzili, MEC-R-05) |
| Zayıf | Kalkansız: savunma ağacı pasifleri yok; mage'lere karşı direnç düşük; yavaşlatma ile kite edilebilir |
| Güçlü olduğu eşleşmeler | Priest (hedef zırhı düşük), yakalanmış mage |
| Zayıf olduğu eşleşmeler | Buz mage'i kiting, W-G peel'i, yoğun AC buff'lı hedef |
| Solo/party farkı | Solo'da Regeneration/restoration ve pot ile sürdürülebilirlik; party'de ortak hedefe tam bağlılık ve debuff sonrası patlama |

Seçenek (ileri profil, CHR-08 sonrası): Saldırı 80 (Hell blade, Etc 511) + Berserk 42 + Master 20.

### 5.2 W-G — `warrior.guard` (koruma/peel warrior'u)

| Alan | Değer |
|---|---|
| Savaş amacı | Kendi priest ve mage'lerini korumak, düşman melee'sini bağlamak, ikincil baskı |
| Stat | W-P ile aynı (255/162/60/50/50) |
| Skill | Saldırı 60 + Savunma 62 + Master 20 = 142 ✓ |
| Savunma 62 ile açılanlar | Hinder/Arrest/Bulwark/evading pasifleri (AC +%20/30/40/50, kalkansız yarıya iner), resist/endure/immunity pasifleri (dirençler +30/60/90), Binding 30, provoke 45, descent 50 (party üyesine ışınlanma, r=30), sacrifice 60 |
| Silah | Graham (1H kılıç, +7 hasar 98, gecikme 114) + Chitin Shield (AC 156, silah tiplerine +50 direnç) |
| Güçlü | Yüksek AC ve direnç; descent ile tehdit altındaki müttefike anında ulaşma; daha hızlı R |
| Zayıf | Saldırı gücü W-P'nin ~%59'u (1036 / 1766); kill potansiyeli düşük |
| Solo/party farkı | Solo'da uzun dayanıklılık ama düşük öldürme; party'de peel ve hedef bağlama |

### 5.3 P-HD — `priest.heal_debuff` (heal + debuff)

| Alan | Değer |
|---|---|
| Savaş amacı | Takımı hayatta tutmak; başarılı debuff ile ortak hedef çağırmak; ölen üyeyi diriltmek |
| Irk | Karus 4 / El Morad 12 |
| Stat | STR 120 (+70), STA 147 (+97), DEX 70, INT 190 (+120), CHA 50 → 577 ✓ (Priest Chitin Shell +8: INT 190, STR 108; Priest Impact +8: STR 120, INT 190) |
| Skill | Heal 60 + Curse 62 + Master 20 = 142 ✓ — **kullanıcının "debuffer + healer" rolü level 80 bütçesine sığıyor** |
| Heal 60 | healing 9, major 18, Cure curse 25, Great healing 27 (960), Cure disease 35, Massive healing 36 (960), Superior healing 45 (1920), restore'lar, Complete healing 54 (10000), Group massive healing 57, Group complete healing 60 |
| Curse 62 | Malice 3 (AC %75), Clear mana 9, Confusion 15, Slow 24 (saldırı hızı %70), Resurrection of love 33, Sweep mana 36, Resurrection of grace 42, Parasite 45 (maks HP %80), Resurrection of favors 54, Torment 57 (alan AC %70), Massive 60 (saldırı %80) |
| Master 20 | Judgment 2, Absoluteness 5, Matchless 10, Helis 12, Curse Refraction 15, Elysian Web 20 (alan, büyü hasarı azaltma) |
| Erişilemeyen | Buff ağacı (AC/HP buff'ları), Superior Parasite (75, Etc 520), Discountis (80, Etc 523) |
| Güçlü | Takım heal'i, debuff ile hedef çağırma, diriltme |
| Zayıf | AC/HP buff'ı veremez; buff'lar P-HB'ye bağımlı |
| Solo/party farkı | Solo'da kendi kendini heal ile uzun dayanır ama öldürme gücü çok düşük ([10](10_SOLO_PK_BEHAVIOR.md)) |

### 5.4 P-HB — `priest.heal_buff` (heal + buff)

| Alan | Değer |
|---|---|
| Savaş amacı | Takım buff kapsamasını (AC, maks HP, direnç) sürekli tutmak; ikincil healer; cure |
| Stat | P-HD ile aynı |
| Skill | Heal 60 + Buff 62 + Master 20 = 142 ✓ — **"buffer + healer" rolü bütçeye sığıyor** |
| Buff 62 | Insensibility serisi (AC +20…+300; peel 60 = +300), Grace…massiveness (maks HP +60…+1500), Undying 54 (maks HP %160), Heapness 54, Greatness 57 (alan party +1200), dirençler (Fresh mind 45: +80), blasting/wildness (STR) |
| Erişilemeyen | imposingness 70 (+2000), Bless of God 70 (party debuff temizleme), Etc 518–523 skill'leri, curse ağacı ve **diriltme** |
| Varyant P-HB2 | Heal 52 + Buff 70 + Master 20 = 142: imposingness ve **Bless of God** (party çapında debuff temizleme) kazanılır; Complete healing 54 ve grup heal'leri kaybedilir. Deney için ([14](14_LEARNING_AND_ADAPTATION.md) L1 dışı, ADR ile). |
| Kısıt | HP buff'ları aynı BuffType (HP_MP=1) kullanır. Bir hedefte HP buff'ı varken daha güçlüsü uygulanamaz (MEC-BUF-02); buff sırası [07](07_PRIEST_BEHAVIOR.md)'de tanımlıdır. |

**İki priest kuralı:** Party'de en fazla iki priest olur ([01](01_PRODUCT_SCOPE_AND_REQUIREMENTS.md) REQ-PTY-02). Önerilen eşleşme **P-HD + P-HB**: biri debuff/diriltme/heal, diğeri buff/heal/cure. İkisi de heal ve cure skill'lerine sahiptir.

### 5.5 M-F — `mage.fire_burst` (ateş hasarı)

| Alan | Değer |
|---|---|
| Savaş amacı | Ortak hedefe yüksek tek hedef hasarı, kümelenmiş düşmana alan hasarı, ölen üyeleri summon ile geri getirme |
| Irk | Karus 3 / El Morad 12 |
| Stat | STR 50, STA 60, DEX 60, INT 160 (+90), CHA 247 (+197) → 577 ✓. Elixir Staff +7/+8 (INT 158, CHA 128–132) ve Complete +7/+8 (INT 160) karşılanır. CHA 255 + INT 158 mümkün değil (293 > 287). |
| Skill | Ateş 70 + Buz 52 + Master 20 = 142 ✓ |
| Ateş 70 | Fire ball, Fire spear, Fire burst 33 (alan r=8), Fire blast 35, Hell fire 39, Inferno 45, Pillar of fire 51 (−1260), Fire Thorn 54, Fire Impact 57, Supernova 60 (alan), **incineration 70** (−2500), **meteor Fall 70** (alan −2100) |
| Buz 52 | Frozen armor, Ice arrow, Ice orb, Ice burst 33, Frostbite, Blizzard 45 (alan yavaşlatma), Ice comet 51 (yavaşlatma %32) |
| Master 20 | Absolute power 2 (büyü gücü %130, 30 sn; scroll `379065000` + Stone of Mage `379061000`), Mana Shield 12, Instantly Magic 15, Minor Resist 20 |
| Temel | **summon friend** (seviye 4, party üyesini yanına çeker), Gate (bind'e; Ronark'ta warp tipi 1 skill'lerin bir kısmı engelli, MEC-T8), Escape (Ronark'ta çalışmaz) |
| Güçlü | En yüksek patlama hasarı |
| Zayıf | Çok düşük HP (~896 + item/buff); melee baskısında hızlı ölüm |

### 5.6 M-I — `mage.ice_control` (buz kontrol)

| Alan | Değer |
|---|---|
| Savaş amacı | Düşmanı yavaşlatarak kendi melee'sine yakalatmak, kaçışı engellemek, alan kontrolü |
| Stat | STR 50, STA 107 (+47), DEX 60, INT 160 (+90), CHA 200 (+150) → 577 ✓ |
| Skill | Buz 70 + Ateş 52 + Master 20 = 142 ✓ |
| Buz 70 | Ice comet 51, Ice Impact 57, Frost nova 60 (alan, yavaşlatma %30, 20 sn), Prismatic 70, ice storm 70 |
| Ateş 52 | Inferno 45, Pillar of fire 51 |
| Not | Yavaşlatma etkisini sunucu hareket için uygulamaz (MEC-BUF-06). Etki gerçek istemcide geçerlidir; botlar da kendi üzerlerindeki yavaşlatmayı uygular (CLI-05). Direnç ≥ 125 olan oyuncu hedefe karşı %100 direnilir (MEC-BUF-05). Bu nedenle direnç takılı rakiplere karşı değeri düşer. |

Lightning (stun/Light Shock) mage profili ilk kapsamda **yoktur**; ikinci faz adayıdır ([01](01_PRODUCT_SCOPE_AND_REQUIREMENTS.md)).

## 6. Referans ekipman setleri

### 6.1 İlkeler

- Aynı kademe, aynı upgrade seviyesi, aynı takı seti: **ekipman farkı AI kalitesini maskelemesin** (CHR-07).
- Set item'ları (Race 111–114: Dragon/Ron/Trial) ve `ItemClass 4` ("Exceptional"/rebirth varyantları) referans setlerde **yoktur**. Set bonusları ve sınıf bazlı AP/AC yüzdeleri adil karşılaştırmayı bozar.
- MB-06/MB-07 nedeniyle HP-drain ve mirror-damage item'ları kullanılmaz.
- Upgrade gösterimi: item ID'sinin son hanesi `+N`'dir (ör. `206001007` = Chitin Shell Pauldron +7) `[V]`. Aynı isimli farklı ID'ler (`x1xx`, `x5xx` gibi) seçenek varyantlarıdır; referans setler yalnızca aşağıdaki ID'leri kullanır.

### 6.2 Kademeler

| Kademe | Kullanım | Zırh/silah upgrade | Takılar |
|---|---|---|---|
| **S0 Başlangıç** | Mekanik doğrulama, ilk 1v1 | +0 | Temel takılar |
| **S1 Standart** | Tüm değerlendirme ve öğrenme maçları (varsayılan) | +7 | Temel takılar |
| **S2 İleri** | Dayanıklılık ve insan karşılaştırması | +8 | Temel takılar |

### 6.3 Zırh ve silah (S1 = `…007`; S0 için son hane 0, S2 için 8)

| Sınıf | Parça | ID (S1) | Değer (S1) | Gereksinim (S1, temel stat) |
|---|---|---|---|---|
| Warrior | Chitin Shell Pauldron / Pads / Helmet / Gauntlet / Boots | 206001007 / 206002007 / 206003007 / 206004007 / 206005007 | AC 175 / 145 / 109 / 73 / 73 | STR 188/184/180/172/176, STA 94/92/90/86/88 |
| W-P | Raptor (2H mızrak) | 156210007 | Hasar 175, gecikme 164, menzil 20 | STR 224 |
| W-G | Graham (1H kılıç) + Chitin Shield | 121310007 + 170250256 | Hasar 98, gecikme 114 / AC 156, silah dirençleri +50 | STR 202 / STR 51 |
| Priest | Priest Chitin Shell Pauldron / Pads / Helmet / Gauntlet / Boots | 286001007 / 286002007 / 286003007 / 286004007 / 286005007 | AC 145 / 121 / 92 / 61 / 61 | INT 188/184/180/172/176, STR 106/104/102/98/100 |
| Priest | Priest Impact (1H topuz) + Chitin Shield | 191110007 + 170250256 | Hasar 100, gecikme 119 | STR 119, INT 188 |
| Mage | Complete Robe / Pants / Helmet / Glove / Boots | 266001007 / 266002007 / 266003007 / 266004007 / 266005007 | AC 115 / 98 / 74 / 49 / 49 | INT 160/156/152/144/148, CHA 118/116/114/110/112 |
| Mage | Elixir Staff | 181110007 | Hasar 111, gecikme 200, menzil 10 | INT 158, CHA 128 |

Kaynak: yerel ITEM tablosu `[V]`; tam satırlar `appendix/data/ref_gear_candidates.psv`. S2 (+8) örnekleri: Complete Robe +8 AC 124 (INT 160, CHA 120), Elixir Staff +8 hasar 120 (INT 158, CHA 132), Raptor +8 hasar 187 (STR 228).

### 6.4 Takılar (tüm kademelerde aynı, upgrade'siz)

| Sınıf | Küpe ×2 | Kolye | Yüzük ×2 | Kemer |
|---|---|---|---|---|
| Warrior | Warrior Earring `310310005` (AC 15, STR +5, HP +80, ateş/buz/yıldırım R +10, zehir R +30) | Iron Necklace `320310126` (STR +5, STA +20, silah tiplerine +10) | Ring of Courage `330110255` (AC 30, STR/STA +7, HP +50, ateş R +20, lanet R +30) | Iron Belt `340610107` (AC 50, silah tiplerine +10) |
| Priest | Cleric Earring `310310007` (AC 15, INT +5, HP +50, MP +40, tüm R +10) | Iron Necklace `320310126` | Ring of Life `330150257` (AC 50, STA/DEX +7, HP +50, MP +50, yıldırım R +20, lanet R +30) | Glass Belt `340410109` (AC 30, INT +10, HP +50, silah tiplerine +10) |
| Mage | Cleric Earring `310310007` | Iron Necklace `320310126` | Ring of Magic `330150256` (AC 40, STA/CHA +7, MP +50, buz R +20, lanet R +30) | Glass Belt `340410109` |

Seçim dönem rehberlerindeki öncelik listeleriyle uyumludur `[S]` (Kalais priest/warrior/mage rehberleri, [19](19_SOURCES_AND_EVIDENCE.md)). Değerler yerel DB'dendir `[V]`. Takıların istemcide sınıfa göre kuşanılabilirliği `[A]` (T-DATA-02).

Not: Item stat bonusları (ör. Iron Necklace STA +20) **maks HP hesabına girer** (STA toplamı) ama **ekipman gereksinimine girmez** (CHR-06).

### 6.5 Taşınan sarf malzemeleri (envanter şablonu)

| Item | ID | Adet | Kullanan |
|---|---|---|---|
| HP potu 720 | Water of favors `389014000` (tüketilmez, K-5) | 1 | Tümü ([11](11_RESOURCE_POTION_AND_SURVIVAL_MANAGEMENT.md)) |
| HP potu 1440 | Water of bless `389015000` (tüketilir) | Senaryo stoğu (STK-01) | Tümü |
| MP potu 1920 | Potion of soul `389020000` (tüketilmez, K-5) | 1 | Tümü |
| Stone of Warrior | `379059000` | 50 | W-P, W-G |
| Stone of Mage | `379061000` | 50 | M-F, M-I |
| Stone of Priest | `379062000` | 50 | P-HD, P-HB |
| Scream Scroll | `379063000` | 1 (tüketilmez) | Warrior |
| Absolute power scroll | `379065000` | 1 | Mage |
| Judgment scroll | `379066000` | 1 | Priest |
| Fire Thorn/Static Thorn scroll | `379069000` | 1 | Mage (opsiyonel) |
| Impact scroll | `379070000` | 1 | Mage (Fire/Ice Impact) |
| Mage blast scroll'ları | `370001000`–`370003000` | 1'er | Mage |
| Stone of Life | `379006000` | 30 | **Tümü** (diriltilmek için taş **hedeften** alınır, 03 §5.4) |

ID'ler skill verisinden (`UseItem`, `BeforeAction`) türetilmiştir `[V]`. Bazı scroll'ların sunucuda tüketilmediği kodda doğrulanmıştır ([`GameServer/MagicInstance.cpp:2978-2996`](https://github.com/ko4life-net/Fire-Drake-Project-v1453/blob/0f520272ae1f11472623d62bff76fff98562e7b3/GameServer/MagicInstance.cpp#L2978-L2996)) `[D]`.

## 7. Doğrulama listesi (F1)

| Test | İçerik |
|---|---|
| T-DATA-01 | Bot karakter kurulum betiği: 6 profil × 2 ulus; stat/skill değişmezleri; giriş sonrası `SetUserAbility` değerlerinin §4 ile karşılaştırılması |
| T-DATA-02 | Referans setlerin istemcide kuşanılabilirliği ve item satırlarının tam dökümü |
| T-DATA-03 | Maks HP/MP/saldırı değerlerinin oyunda ölçülmesi (item ve buff dahil) |
| T-DATA-04 | Quest kapılı skill'lerin (Etc 510–523) bu kurulumda açılabilirliği (Q-04) |
| T-DATA-05 | Maksimum ağırlık başlatma sorunu (MB-12) |

## 8. Açık sorular (bu dokümana özel)

Q-03 (seviye 51), Q-04 (master questleri), Q-05 (istemci sınıf/ekipman kısıtı) — ayrıntı [18](18_RISKS_ASSUMPTIONS_AND_OPEN_QUESTIONS.md).

## Değişiklik günlüğü

| Tarih | Sürüm | Değişiklik |
|---|---|---|
| 2026-10-01 | v1.0 | İlk sürüm |
