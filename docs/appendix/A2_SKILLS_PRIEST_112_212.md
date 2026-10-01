# A2_SKILLS_PRIEST_112_212 — Master Priest Skill Kataloğu

> Otomatik üretildi: yerel `FDP_kn_online` veri tabanının salt okunur dökümünden (`MAGIC`, `MAGIC_TYPE1/3/4/5/7/8`), 2026-10-01. Elle değer girilmemiştir. Etiket: `[V]`.
> Birimler: Cast ve Recast **0,1 sn** biriminden saniyeye çevrildi (sunucu recast'i tam saniye çözünürlükle karşılaştırır, bkz. 03 MEC-MAG-02; cast süresini sunucu uygulamaz, MEC-MAG-01). Menzil metre. `Gerek` = ağaç puanı (Ağaç/Master) veya karakter seviyesi (Temel).
> El Morad ID = Karus ID + 100000. `Fark` sütunu El Morad satırının sayısal olarak farklı olduğu alanları gösterir (Karus değeri → El Morad değeri).
> Tip kısaltmaları: T1 yakın dövüş, T3 doğrudan hasar/heal, T4 buff/debuff, T5 cure/diriltme, T7 özel, T8 ışınlanma. `Etc ≠ 0` = quest kapısı (03 §4.3 U11). `Taş` = `BeforeAction` 1–4 olduğunda tüketilen sınıf taşı (03 §4.3 U9); bu durumda `UseItem` yalnızca gereksinimdir.


## Temel (seviye) — temel

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112001 | 212001 | Tiny healing | 1 | FRIEND_WITHME | 5 | 1.9 | 0.1 | 56 | T3 HP 15 |  |  |  |  |  |
| 112002 | 212002 | Light strike | 2 | ENEMY | 5 | 1.5 | 5.4 | 56 | T3 HP -29, büyü |  |  |  |  |  |
| 112004 | 212004 | Strength | 4 | FRIEND_WITHME | 10 | 1.5 | 0.1 | 56 | T4 STATS(7) Str=15, 600sn |  |  |  |  |  |
| 112005 | 212005 | Light healing | 5 | FRIEND_WITHME | 10 | 1.9 | 0.1 | 56 | T3 HP 30 |  |  |  |  |  |
| 112006 | 212006 | Resist poison | 6 | FRIEND_WITHME | 10 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) PoisonR=20, 600sn |  |  |  |  |  |
| 112007 | 212007 | Brightness | 7 | ENEMY | 10 | 1.5 | 5.4 | 56 | T3 HP -77, büyü |  |  |  |  |  |
| 112008 | 212008 | Tiny restore | 8 | FRIEND_WITHME | 15 | 1.5 | 0.1 | 56 | T3 HP, süreli 50/20sn |  |  |  |  |  |
| 112010 | 212010 | Light Attack | 18 | ENEMY | 50 | 1.5 | 2.5 | 33 | T3 MP -105, süreli -10/10sn, büyü |  |  |  |  |  |
| 112011 | 212011 | Light Counter | 24 | ENEMY | 100 | 1.5 | 3.1 | 33 | T3 MP -260, süreli -30/10sn, büyü |  |  |  |  |  |
| 112012 | 212012 | Critical Light | 33 | ENEMY | 150 | 1.5 | 3.7 | 33 | T3 MP -385, süreli -50/10sn, büyü |  |  |  |  |  |

## Ağaç 5 — heal ağacı

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112500 | 212500 | minor healing | 0 | FRIEND_WITHME | 10 | 1.5 | 0.1 | 56 | T3 HP 60 |  |  |  |  |  |
| 112503 | 212503 | Light restore | 3 | FRIEND_WITHME | 25 | 1.5 | 0.1 | 56 | T3 HP, süreli 100/30sn |  |  |  |  |  |
| 112509 | 212509 | healing | 9 | FRIEND_WITHME | 20 | 1.5 | 0.1 | 56 | T3 HP 120 |  |  |  |  |  |
| 112511 | 212511 | collision | 12 | ENEMY | 30 | 0.0 | 0.1 | 0 | T1 200%, isabet 100 (sabit) |  |  |  |  |  |
| 112512 | 212512 | Restore | 12 | FRIEND_WITHME | 30 | 1.5 | 0.1 | 56 | T3 HP, süreli 200/30sn |  |  |  |  |  |
| 112518 | 212518 | major healing | 18 | FRIEND_WITHME | 40 | 1.5 | 0.1 | 56 | T3 HP 240 |  |  |  |  |  |
| 112520 | 212520 | shuddering | 21 | ENEMY | 40 | 0.0 | 0.2 | 0 | T1 250%, isabet 100 (sabit) |  |  |  |  |  |
| 112521 | 212521 | Major restore | 21 | FRIEND_WITHME | 100 | 1.5 | 0.1 | 56 | T3 HP, süreli 400/30sn |  |  |  |  |  |
| 112525 | 212525 | Cure curse | 25 | FRIEND_WITHME | 60 | 1.5 | 1.5 | 56 | T5 debuff temizle (REMOVE_TYPE4) |  |  |  |  |  |
| 112527 | 212527 | Great healing | 27 | FRIEND_WITHME | 80 | 1.5 | 2.0 | 56 | T3 HP 960 |  |  |  |  |  |
| 112529 | 212529 | blasting | 30 | SELF | 80 | 0.0 | 1.0 | 0 | T4 STATS(7) Str=30, 400sn |  |  |  |  |  |
| 112530 | 212530 | Great restore | 30 | FRIEND_WITHME | 200 | 1.5 | 0.1 | 56 | T3 HP, süreli 800/30sn |  |  |  |  |  |
| 112535 | 212535 | Cure disease | 35 | FRIEND_WITHME | 120 | 1.5 | 1.5 | 56 | T5 DoT temizle (REMOVE_TYPE3) |  |  |  |  |  |
| 112536 | 212536 | Massive healing | 36 | FRIEND_WITHME | 160 | 1.5 | 0.1 | 56 | T3 HP 960 |  |  |  |  |  |
| 112539 | 212539 | Massive restore | 39 | FRIEND_WITHME | 375 | 1.5 | 0.1 | 56 | T3 HP, süreli 1500/30sn |  |  |  |  |  |
| 112542 | 212542 | ruin | 42 | ENEMY | 100 | 0.0 | 2.0 | 0 | T1 300% +100, isabet 100 (sabit) |  |  |  |  |  |
| 112545 | 212545 | Superior healing | 45 | FRIEND_WITHME | 320 | 1.5 | 0.1 | 56 | T3 HP 1920 |  |  |  |  |  |
| 112548 | 212548 | Superior restore | 48 | FRIEND_WITHME | 625 | 1.5 | 0.1 | 56 | T3 HP, süreli 2500/30sn |  |  |  |  |  |
| 112551 | 212551 | Hellish | 51 | ENEMY | 120 | 0.0 | 3.0 | 0 | T1 350% +50, isabet 100 (sabit) |  |  |  |  |  |
| 112554 | 212554 | Complete healing | 54 | FRIEND_WITHME | 960 | 1.5 | 5.4 | 56 | T3 HP 10000 |  |  |  |  |  |
| 112557 | 212557 | Group massive healing | 57 | PARTY_ALL | 960 | 1.5 | 5.4 | 56 | T3 HP 960, r=30 |  |  |  |  |  |
| 112560 | 212560 | Group complete healing | 60 | PARTY_ALL | 1920 | 1.5 | 6.4 | 56 | T3 HP 10000, r=30 |  |  |  |  |  |
| 112570 | 212570 | critical restore | 70 | PARTY_ALL | 1000 | 1.5 | 0.1 | 56 | T3 HP, süreli 3000/20sn, r=20 |  |  |  | evet |  |
| 112575 | 212575 | Past Recovery | 75 | PARTY_ALL | 1200 | 1.5 | 0.1 | 67 | T3 HP 2500, süreli 3000/20sn, r=30 |  |  | 520 |  |  |
| 112580 | 212580 | Past Restore | 80 | PARTY_ALL | 1400 | 1.5 | 25.5 | 67 | T3 HP, süreli 6000/20sn, r=30 |  |  | 523 |  |  |

## Ağaç 6 — buff ağacı (Aura/Ecstacy)

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112603 | 212603 | Insensibility Skin | 3 | FRIEND_WITHME | 10 | 1.5 | 0.1 | 56 | T4 AC(2) AC=20, 600sn |  |  |  |  |  |
| 112606 | 212606 | Grace | 6 | PARTY | 15 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHP=60, 600sn |  |  |  |  |  |
| 112609 | 212609 | Resist all | 9 | FRIEND_WITHME | 15 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) MagicR=20 DiseaseR=20 PoisonR=20, 600sn |  |  |  |  |  |
| 112611 | 212611 | Wrath | 12 | ENEMY | 30 | 0.0 | 0.1 | 0 | T1 150%, isabet 100 (sabit) |  |  |  |  |  |
| 112612 | 212612 | Insensibility shell | 12 | FRIEND_WITHME | 20 | 1.5 | 0.1 | 56 | T4 AC(2) AC=40, 600sn |  |  |  |  |  |
| 112615 | 212615 | Brave | 15 | PARTY | 30 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHP=120, 600sn |  |  |  |  |  |
| 112620 | 212620 | wield | 21 | ENEMY | 40 | 0.0 | 0.2 | 0 | T1 200%, isabet 100 (sabit) |  |  |  |  |  |
| 112621 | 212621 | Insensibility armor | 21 | FRIEND_WITHME | 40 | 1.5 | 0.1 | 56 | T4 AC(2) AC=80, 600sn |  |  |  |  |  |
| 112624 | 212624 | Strong | 24 | PARTY | 60 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHP=240, 600sn |  |  |  |  |  |
| 112627 | 212627 | Bright mind | 27 | FRIEND_WITHME | 30 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) MagicR=40 DiseaseR=40 PoisonR=40, 600sn |  |  |  |  |  |
| 112629 | 212629 | wildness | 30 | SELF | 80 | 0.0 | 1.0 | 0 | T4 STATS(7) Str=30, 400sn |  |  |  |  |  |
| 112630 | 212630 | Insensibility shield | 30 | FRIEND_WITHME | 80 | 1.5 | 0.1 | 56 | T4 AC(2) AC=120, 600sn |  |  |  |  |  |
| 112633 | 212633 | Hardness | 33 | PARTY | 120 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHP=480, 600sn |  |  |  |  |  |
| 112636 | 212636 | Calm mind | 36 | FRIEND_WITHME | 45 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) MagicR=60 DiseaseR=60 PoisonR=60, 600sn |  |  |  |  |  |
| 112639 | 212639 | Insensibility barrier | 39 | FRIEND_WITHME | 80 | 1.5 | 0.1 | 56 | T4 AC(2) AC=160, 600sn |  |  |  |  |  |
| 112641 | 212641 | Harsh | 42 | ENEMY | 100 | 0.0 | 2.0 | 0 | T1 200% +100, isabet 100 (sabit) |  |  |  |  |  |
| 112642 | 212642 | Mightness | 42 | PARTY | 240 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHP=960, 600sn |  |  |  |  |  |
| 112645 | 212645 | Fresh mind | 45 | FRIEND_WITHME | 60 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) MagicR=80 DiseaseR=80 PoisonR=80, 600sn |  |  |  |  |  |
| 112650 | 212650 | collapse | 51 | ENEMY | 120 | 0.0 | 3.0 | 0 | T1 350% +50, isabet 100 (sabit) |  |  |  |  |  |
| 112651 | 212651 | Insensibility protector | 51 | FRIEND_WITHME | 100 | 1.5 | 0.1 | 56 | T4 AC(2) AC=200, 600sn |  |  |  |  |  |
| 112654 | 212654 | Undying | 54 | PARTY | 240 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHPPct=160, 600sn |  |  |  |  |  |
| 112655 | 212655 | Heapness | 54 | PARTY | 300 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHP=1200, 600sn |  |  |  |  |  |
| 112656 | 212656 | Greatness | 57 | PARTY_ALL | 570 | 1.5 | 0.1 | 101 | T4 HP_MP(1) MaxHP=1200, 600sn, r=30 |  |  |  |  |  |
| 112657 | 212657 | massiveness | 57 | PARTY | 360 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHP=1500, 600sn |  |  |  |  |  |
| 112660 | 212660 | Insensibility peel | 60 | FRIEND_WITHME | 150 | 1.5 | 0.1 | 56 | T4 AC(2) AC=300, 600sn |  |  |  |  |  |
| 112670 | 212670 | imposingness | 70 | PARTY | 460 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHP=2000, 600sn |  |  |  | evet |  |
| 112671 | 212671 | Bless of God | 70 | PARTY_ALL | 230 | 1.5 | 0.1 | 45 | T5 debuff temizle (REMOVE_TYPE4) |  |  |  | evet |  |
| 112672 | 212672 | Massive Binder | 72 | PARTY_ALL | 960 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHP=2000, 600sn, r=30 |  |  | 518 |  |  |
| 112673 | 212673 | Round Insensibility | 74 | PARTY_ALL | 750 | 1.5 | 0.1 | 56 | T4 AC(2) AC=300, 600sn, r=30 |  |  | 519 |  |  |
| 112674 | 212674 | Insensibility Guard | 76 | FRIEND_WITHME | 300 | 1.5 | 0.1 | 56 | T4 AC(2) AC=350, 600sn |  |  | 521 |  |  |
| 112675 | 212675 | Superioris | 78 | PARTY | 690 | 1.5 | 0.1 | 56 | T4 HP_MP(1) MaxHP=2500, 600sn |  |  | 522 |  |  |
| 112676 | 212676 | Counter Curse | 80 | PARTY_ALL | 1200 | 1.5 | 0.1 | 56 | T4 BLOCK_CURSE(29) , 10sn, r=20 | 379062000 |  | 523 |  |  |

## Ağaç 7 — curse ağacı (Holy Spirit/Talisman)

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112700 | 212700 | Gate | 0 | SELF | 20 | 1.5 | 7.4 | 56 | T8 warp=1 r=10000 |  |  |  |  |  |
| 112703 | 212703 | Malice | 3 | ENEMY | 40 | 1.5 | 7.4 | 56 | T4 AC(2) ACPct=75, 150sn |  |  |  |  | Range 56→90 |
| 112709 | 212709 | Clear mana | 9 | ENEMY | 80 | 1.5 | 7.4 | 56 | T3 MP -480 |  |  |  |  |  |
| 112712 | 212712 | tilt | 12 | ENEMY | 30 | 0.0 | 0.1 | 0 | T1 150%, isabet 100 (sabit) |  |  |  |  |  |
| 112715 | 212715 | Confusion | 15 | ENEMY | 80 | 1.5 | 7.4 | 56 | T4 STATS(7) Cha=-30, 150sn |  |  |  |  |  |
| 112721 | 212721 | Bloody | 21 | ENEMY | 40 | 0.0 | 0.2 | 0 | T1 300%, isabet 100 (sabit) |  |  |  |  |  |
| 112724 | 212724 | Slow | 24 | ENEMY | 120 | 1.5 | 7.4 | 56 | T4 ATTACK_SPEED(5) AttackSpeed=70, 150sn |  |  |  |  |  |
| 112727 | 212727 | Reverse life | 27 | ENEMY | 50 | 1.5 | 9.4 | 56 | T4 HP_MP(1) MaxHPPct=99, 1sn |  |  |  |  |  |
| 112729 | 212729 | eruption | 30 | SELF | 80 | 0.0 | 1.0 | 0 | T4 STATS(7) Str=30, 400sn |  |  |  |  |  |
| 112730 | 212730 | Sleep Wing | 30 | ENEMY | 120 | 1.5 | 7.4 | 56 | T7 Sleep Wing |  |  |  |  |  |
| 112733 | 212733 | Resurrection of love | 33 | CORPSE_FRIEND | 400 | 1.5 | 25.0 | 11 | T5 diriltme, taş 4 | 379006000 |  |  |  |  |
| 112736 | 212736 | Sweep mana | 36 | ENEMY | 160 | 1.5 | 7.4 | 56 | T3 MP -960 |  |  |  |  |  |
| 112739 | 212739 | raving edge | 39 | ENEMY | 100 | 0.0 | 2.0 | 0 | T1 250% +100, isabet 100 (sabit) |  |  |  |  |  |
| 112742 | 212742 | Resurrection of grace | 42 | CORPSE_FRIEND | 600 | 1.5 | 25.0 | 11 | T5 diriltme, taş 10 | 379006000 |  |  |  |  |
| 112745 | 212745 | Parasite | 45 | ENEMY | 100 | 1.5 | 7.4 | 56 | T4 HP_MP(1) MaxHPPct=80, 150sn |  |  |  |  | Range 56→90 |
| 112750 | 212750 | Hades | 51 | ENEMY | 120 | 0.0 | 3.0 | 0 | T1 350% +50, isabet 100 (sabit) |  |  |  |  |  |
| 112751 | 212751 | Sleep Carpet | 51 | AREA_ENEMY | 240 | 1.5 | 9.4 | 56 | T7 Sleep Carpet |  |  |  |  |  |
| 112754 | 212754 | Resurrection of favors | 54 | CORPSE_FRIEND | 800 | 1.5 | 25.0 | 11 | T5 diriltme, taş 30 | 379006000 |  |  |  |  |
| 112757 | 212757 | Torment | 57 | AREA_ENEMY | 150 | 1.5 | 9.4 | 56 | T4 AC(2) ACPct=70, 150sn, r=10 |  |  |  |  |  |
| 112760 | 212760 | Massive | 60 | ENEMY | 180 | 1.5 | 10.4 | 56 | T4 DAMAGE(4) Attack=80, 150sn |  |  |  |  | Range 56→90 |
| 112770 | 212770 | Subside | 70 | AREA_ENEMY | 260 | 1.5 | 15.4 | 45 | T4 DAMAGE(4) Attack=80, 150sn, r=20 |  |  |  | evet |  |
| 112771 | 212771 | Superior Parasite | 75 | ENEMY | 350 | 1.5 | 25.4 | 112 | T4 HP_MP(1) MaxHPPct=70, 150sn |  |  | 520 |  |  |
| 112772 | 212772 | Discountis | 80 | AREA_ENEMY | 960 | 1.5 | 25.4 | 112 | T3 MP -3840, r=15 | 379062000 |  | 523 |  |  |

## Master — master

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112800 | 212800 | Daring | 0 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 112802 | 212802 | Judgment | 2 | ENEMY | 200 | 0.0 | 0.5 | 0 | T1 500% +150, isabet 100 (sabit) | 379066000 | 379062000 |  |  |  |
| 112805 | 212805 | Absoluteness | 5 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 112810 | 212810 | Matchless | 10 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 112815 | 212815 | Helis | 12 | ENEMY | 350 | 0.0 | 0.5 | 0 | T1 400% +400, isabet 100 (sabit) |  | 379062000 |  |  |  |
| 112820 | 212820 | Curse Refraction | 15 | SELF | 320 | 1.5 | 0.1 | 45 | T4 BLOCK_CURSE_REFLECT(30) , 10sn |  |  |  |  |  |
| 112825 | 212825 | Elysian Web | 20 | AREA_FRIEND | 640 | 1.5 | 0.1 | 56 | T4 ELYSIAN_WEB(27) ExpPct=70, 20sn, r=15 | 379062000 |  |  |  |  |

## (kullanılamaz: 9) — 

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 112009 | 212009 | whipping | 1 | SELF | 5 | 0.0 | 25.5 | 0 | T4 SPEED(6) Speed=200, 1200sn |  |  |  |  |  |
