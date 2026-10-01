# A3_SKILLS_MAGE_110_210 — Master Mage Skill Kataloğu

> Otomatik üretildi: yerel `FDP_kn_online` veri tabanının salt okunur dökümünden (`MAGIC`, `MAGIC_TYPE1/3/4/5/7/8`), 2026-10-01. Elle değer girilmemiştir. Etiket: `[V]`.
> Birimler: Cast ve Recast **0,1 sn** biriminden saniyeye çevrildi (sunucu recast'i tam saniye çözünürlükle karşılaştırır, bkz. 03 MEC-MAG-02; cast süresini sunucu uygulamaz, MEC-MAG-01). Menzil metre. `Gerek` = ağaç puanı (Ağaç/Master) veya karakter seviyesi (Temel).
> El Morad ID = Karus ID + 100000. `Fark` sütunu El Morad satırının sayısal olarak farklı olduğu alanları gösterir (Karus değeri → El Morad değeri).
> Tip kısaltmaları: T1 yakın dövüş, T3 doğrudan hasar/heal, T4 buff/debuff, T5 cure/diriltme, T7 özel, T8 ışınlanma. `Etc ≠ 0` = quest kapısı (03 §4.3 U11). `Taş` = `BeforeAction` 1–4 olduğunda tüketilen sınıf taşı (03 §4.3 U9); bu durumda `UseItem` yalnızca gereksinimdir.


## Temel (seviye) — temel

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110001 | 210001 | Flash | 1 | ENEMY | 4 | 1.5 | 4.3 | 56 | T3 HP -24, ateş |  |  |  |  |  |
| 110003 | 210003 | Shiver | 3 | ENEMY | 5 | 1.5 | 4.3 | 56 | T3 HP, süreli -60/20sn, buz |  |  |  |  |  |
| 110004 | 210004 | summon friend | 4 | PARTY | 5 | 1.5 | 0.1 | 22500 | T8 warp=12 r=10000 |  |  |  |  |  |
| 110005 | 210005 | Flame | 5 | ENEMY | 5 | 1.5 | 4.3 | 56 | T3 HP -62, ateş |  |  |  |  |  |
| 110007 | 210007 | Cold wave | 7 | ENEMY | 7 | 1.5 | 4.3 | 56 | T3 HP -68, buz; T4 SPEED(6) Speed=65, 10sn |  |  |  |  |  |
| 110009 | 210009 | Spark | 9 | ENEMY | 15 | 1.5 | 4.3 | 56 | T3 HP -88, yıldırım |  |  |  |  |  |
| 110015 | 210015 | Gate | 15 | SELF | 30 | 1.5 | 10.0 | 56 | T8 warp=1 r=10000 |  |  |  |  |  |
| 110035 | 210035 | Escape | 35 | PARTY_ALL | 400 | 1.5 | 25.0 | 22500 | T8 warp=1 r=10000 |  |  |  |  |  |

## Ağaç 5 — ateş ağacı

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110503 | 210503 | Burn | 3 | ENEMY | 20 | 1.0 | 0.1 | 11 | T3 HP -168, ateş |  |  |  |  |  |
| 110506 | 210506 | Resist fire | 6 | FRIEND_WITHME | 15 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) FireR=20, 300sn |  |  |  |  |  |
| 110509 | 210509 | Blaze | 9 | ENEMY | 30 | 1.5 | 5.3 | 56 | T3 HP, süreli -280/20sn, ateş |  |  |  |  |  |
| 110515 | 210515 | Fire ball | 15 | ENEMY | 50 | 1.5 | 4.3 | 78 | T3 HP -308, ateş |  |  |  |  |  |
| 110518 | 210518 | Ignition | 18 | ENEMY | 60 | 1.0 | 0.1 | 56 | T3 HP -238, süreli -336/20sn, ateş |  |  |  |  | Range 56→90 |
| 110524 | 210524 | Endure fire | 24 | FRIEND_WITHME | 50 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) FireR=50, 300sn |  |  |  |  |  |
| 110527 | 210527 | Fire spear | 27 | ENEMY | 80 | 1.5 | 4.3 | 78 | T3 HP -588, ateş |  |  |  |  |  |
| 110533 | 210533 | Fire burst | 33 | AREA_ENEMY | 150 | 1.5 | 0.1 | 90 | T3 HP -588, ateş, r=8 |  |  |  |  |  |
| 110535 | 210535 | Fire blast | 35 | ENEMY | 150 | 1.5 | 4.3 | 78 | T3 HP -840, ateş | 370001000 |  |  |  |  |
| 110539 | 210539 | Hell fire | 39 | ENEMY | 150 | 1.5 | 4.3 | 56 | T3 HP -480, süreli -1120/20sn, ateş |  |  |  |  |  |
| 110542 | 210542 | fire blade | 42 | ENEMY | 100 | 0.0 | 0.0 | 11 | T1 100%, isabet 100; T3 HP -336, ateş |  |  |  |  |  |
| 110543 | 210543 | specter of fire | 43 | ENEMY | 75 | 1.0 | 0.1 | 11 | T3 HP -615, ateş |  |  |  |  |  |
| 110545 | 210545 | Inferno | 45 | AREA_ENEMY | 200 | 1.5 | 15.3 | 56 | T3 HP -504, ateş, r=15 |  |  |  |  |  |
| 110548 | 210548 | Immunity fire | 48 | FRIEND_WITHME | 80 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) FireR=80, 300sn |  |  |  |  |  |
| 110551 | 210551 | Pillar of fire | 51 | ENEMY | 160 | 1.5 | 5.3 | 56 | T3 HP -1260, ateş |  |  |  |  |  |
| 110554 | 210554 | Fire Thorn | 54 | ENEMY | 220 | 1.5 | 6.0 | 56 | T3 emme -1550, ateş | 379069000 | 379061000 |  |  |  |
| 110556 | 210556 | Manes of fire | 56 | ENEMY | 95 | 1.0 | 0.1 | 11 | T3 HP -1015, ateş |  |  |  |  |  |
| 110557 | 210557 | Fire Impact | 57 | ENEMY | 220 | 1.5 | 20.3 | 56 | T3 HP -1260, süreli -1000/10sn, ateş | 379070000 |  |  |  | Range 56→90 |
| 110560 | 210560 | Supernova | 60 | AREA_ENEMY | 400 | 1.5 | 15.3 | 56 | T3 HP -1800, süreli -600/20sn, ateş, r=15 |  |  |  |  |  |
| 110570 | 210570 | incineration | 70 | ENEMY | 390 | 1.1 | 21.3 | 45 | T3 HP -2500, ateş |  |  |  | evet |  |
| 110571 | 210571 | meteor Fall | 70 | AREA_ENEMY | 600 | 1.3 | 18.3 | 45 | T3 HP -2100, süreli -600/20sn, ateş, r=15 |  |  |  | evet |  |
| 110572 | 210572 | Fire Staff | 72 | ENEMY | 300 | 0.0 | 0.0 | 22 | T1 100% +100, isabet 100; T3 HP -2500, ateş |  |  | 515 |  |  |
| 110573 | 210573 | Fire Armor | 75 | SELF | 250 | 1.5 | 25.0 | 0 | T4 MAGE_ARMOR(25) , 120sn | 379061000 |  | 516 |  |  |
| 110574 | 210574 | Vampiric Fire | 80 | ENEMY | 350 | 1.5 | 0.1 | 56 | T3 MP emme -3000, ateş |  |  | 517 |  |  |
| 110575 | 210575 | Igzination | 80 | ENEMY | 390 | 1.1 | 21.3 | 78 | T3 HP -4500, ateş |  |  | 517 |  |  |

## Ağaç 6 — buz ağacı

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110603 | 210603 | Freeze | 3 | ENEMY | 20 | 1.5 | 0.1 | 11 | T3 HP -118, buz; T4 SPEED(6) Speed=50, 10sn |  |  |  |  |  |
| 110606 | 210606 | Resist cold | 6 | FRIEND_WITHME | 15 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) ColdR=20, 300sn |  |  |  |  |  |
| 110609 | 210609 | Chill | 9 | ENEMY | 30 | 1.5 | 5.3 | 56 | T3 HP, süreli -196/20sn, buz; T4 SPEED(6) Speed=48, 11sn |  |  |  |  |  |
| 110612 | 210612 | Frozen armor | 12 | FRIEND_WITHME | 40 | 1.5 | 0.1 | 56 | T4 AC(2) AC=60, 300sn |  |  |  |  |  |
| 110615 | 210615 | Ice arrow | 15 | ENEMY | 50 | 1.5 | 4.3 | 78 | T3 HP -216, buz; T4 SPEED(6) Speed=46, 12sn |  |  |  |  |  |
| 110618 | 210618 | Solid | 18 | ENEMY | 60 | 1.5 | 0.1 | 56 | T3 HP -167, süreli -236/20sn, buz; T4 SPEED(6) Speed=44, 13sn |  |  |  |  | Range 56→90 |
| 110624 | 210624 | Endure cold | 24 | FRIEND_WITHME | 50 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) ColdR=50, 300sn |  |  |  |  |  |
| 110627 | 210627 | Ice orb | 27 | ENEMY | 80 | 1.5 | 4.3 | 78 | T3 HP -412, buz; T4 SPEED(6) Speed=42, 14sn |  |  |  |  |  |
| 110630 | 210630 | Frozen shell | 30 | FRIEND_WITHME | 80 | 1.5 | 0.1 | 56 | T4 AC(2) AC=120, 300sn |  |  |  |  |  |
| 110633 | 210633 | Ice burst | 33 | AREA_ENEMY | 150 | 1.5 | 0.1 | 90 | T3 HP -412, buz, r=8; T4 SPEED(6) Speed=40, 15sn, r=5 |  |  |  |  |  |
| 110635 | 210635 | Ice blast | 35 | ENEMY | 150 | 1.5 | 4.3 | 78 | T3 HP -588, buz; T4 SPEED(6) Speed=38, 16sn | 370002000 |  |  |  |  |
| 110639 | 210639 | Frostbite | 39 | ENEMY | 150 | 1.5 | 4.3 | 56 | T3 HP -336, süreli -784/20sn, buz; T4 SPEED(6) Speed=36, 17sn, r=5 |  |  |  |  |  |
| 110642 | 210642 | frozen blade | 42 | ENEMY | 100 | 0.0 | 0.0 | 11 | T1 100%, isabet 100; T3 HP -236, buz |  |  |  |  |  |
| 110643 | 210643 | specter of Ice | 43 | ENEMY | 75 | 1.0 | 0.1 | 11 | T3 HP -431, buz |  |  |  |  |  |
| 110645 | 210645 | Blizzard | 45 | AREA_ENEMY | 200 | 1.5 | 15.3 | 56 | T3 HP -353, buz, r=15; T4 SPEED(6) Speed=34, 18sn, r=15 |  |  |  |  |  |
| 110648 | 210648 | Immunity cold | 48 | FRIEND_WITHME | 80 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) ColdR=80, 300sn |  |  |  |  |  |
| 110651 | 210651 | Ice comet | 51 | ENEMY | 160 | 1.5 | 5.3 | 56 | T3 HP -882, buz; T4 SPEED(6) Speed=32, 19sn |  |  |  |  |  |
| 110654 | 210654 | Ice barrier | 54 | FRIEND_WITHME | 120 | 1.5 | 0.1 | 56 | T4 AC(2) AC=180, 300sn |  |  |  |  |  |
| 110656 | 210656 | Manes of Ice | 56 | ENEMY | 95 | 1.0 | 0.1 | 11 | T3 HP -711, buz |  |  |  |  |  |
| 110657 | 210657 | Ice Impact | 57 | ENEMY | 220 | 1.5 | 20.3 | 56 | T3 HP -882, süreli -700/10sn, buz; T4 SPEED(6) Speed=50, 10sn | 379070000 |  |  |  |  |
| 110660 | 210660 | Frost nova | 60 | AREA_ENEMY | 400 | 1.5 | 15.3 | 56 | T3 HP -1260, süreli -420/0sn, buz, r=15; T4 SPEED(6) Speed=30, 20sn, r=15 |  |  |  |  |  |
| 110670 | 210670 | Prismatic | 70 | ENEMY | 390 | 1.1 | 21.3 | 45 | T3 HP -1750, buz; T4 SPEED(6) Speed=50, 10sn |  |  |  | evet |  |
| 110671 | 210671 | ice storm | 70 | AREA_ENEMY | 600 | 1.3 | 18.3 | 45 | T3 HP -1470, buz, r=15; T4 SPEED(6) Speed=30, 20sn, r=15 |  |  |  | evet |  |
| 110672 | 210672 | Ice Staff | 72 | ENEMY | 300 | 0.0 | 0.0 | 22 | T1 100% +100, isabet 100; T3 HP -2500, buz |  |  | 515 |  |  |
| 110673 | 210673 | Ice Armor | 75 | SELF | 250 | 1.5 | 25.0 | 0 | T4 MAGE_ARMOR(25) , 120sn | 379061000 |  | 516 |  |  |
| 110674 | 210674 | Freezing Distance | 80 | ENEMY | 350 | 1.5 | 25.0 | 56 | T4 FREEZE(22) Speed=1, 15sn | 379061000 |  | 517 |  |  |

## Ağaç 7 — yıldırım ağacı

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110703 | 210703 | Charge | 3 | ENEMY | 20 | 1.5 | 0.1 | 11 | T3 HP -118, yıldırım |  |  |  |  |  |
| 110706 | 210706 | Resist lightning | 6 | FRIEND_WITHME | 15 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) LightningR=20, 300sn |  |  |  |  |  |
| 110709 | 210709 | Counter spell | 9 | ENEMY | 30 | 1.5 | 5.3 | 56 | T3 HP, süreli -196/20sn, yıldırım |  |  |  |  |  |
| 110715 | 210715 | Lightning | 15 | ENEMY | 50 | 1.5 | 4.3 | 78 | T3 HP -216, yıldırım |  |  |  |  |  |
| 110718 | 210718 | Static hemisphere | 18 | ENEMY | 60 | 1.5 | 0.1 | 56 | T3 HP -167, süreli -236/20sn, yıldırım |  |  |  |  | Range 56→90 |
| 110724 | 210724 | Endure lightning | 24 | FRIEND_WITHME | 50 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) LightningR=50, 300sn |  |  |  |  |  |
| 110727 | 210727 | Thunder | 27 | ENEMY | 80 | 1.5 | 4.3 | 78 | T3 HP -412, yıldırım |  |  |  |  |  |
| 110733 | 210733 | Thunder burst | 33 | AREA_ENEMY | 150 | 1.5 | 0.1 | 90 | T3 HP -412, yıldırım, r=8 |  |  |  |  |  |
| 110735 | 210735 | Thunder blast | 35 | ENEMY | 150 | 1.5 | 4.3 | 78 | T3 HP -588, yıldırım | 370003000 |  |  |  |  |
| 110739 | 210739 | Discharge | 39 | ENEMY | 150 | 1.5 | 4.3 | 56 | T3 HP -336, süreli -784/20sn, yıldırım |  |  |  |  |  |
| 110742 | 210742 | charged blade | 42 | ENEMY | 100 | 0.0 | 0.0 | 11 | T1 100%, isabet 100; T3 HP -236, yıldırım |  |  |  |  |  |
| 110743 | 210743 | specter of thunder | 43 | ENEMY | 75 | 1.0 | 0.1 | 11 | T3 HP -431, yıldırım |  |  |  |  |  |
| 110745 | 210745 | Thundercloud | 45 | AREA_ENEMY | 200 | 1.5 | 15.3 | 56 | T3 HP -353, yıldırım, r=15 |  |  |  |  |  |
| 110748 | 210748 | Immunity lightning | 48 | FRIEND_WITHME | 80 | 1.5 | 0.1 | 56 | T4 RESISTANCES(8) LightningR=80, 300sn |  |  |  |  |  |
| 110751 | 210751 | Static orb | 51 | ENEMY | 160 | 1.5 | 5.3 | 56 | T3 HP -882, yıldırım |  |  |  |  |  |
| 110754 | 210754 | Static Thorn | 54 | ENEMY | 220 | 1.5 | 6.0 | 56 | T3 emme -1100, yıldırım | 379069000 | 379061000 |  |  |  |
| 110756 | 210756 | Manes of thunder | 56 | ENEMY | 95 | 1.0 | 0.1 | 11 | T3 HP -711, yıldırım |  |  |  |  |  |
| 110757 | 210757 | Thunder Impact | 57 | ENEMY | 220 | 1.5 | 20.3 | 56 | T3 HP -882, süreli -700/10sn, yıldırım | 379070000 |  |  |  |  |
| 110760 | 210760 | Static nova | 60 | AREA_ENEMY | 400 | 1.5 | 15.3 | 56 | T3 HP -1260, süreli -420/0sn, yıldırım, r=15 |  |  |  |  |  |
| 110762 | 210762 | Light Shock | 62 | AREA_ENEMY | 400 | 1.5 | 0.1 | 56 | T4 DISABLE_TARGETING(20) , 5sn, r=10 |  |  |  |  |  |
| 110770 | 210770 | Stun Cloud | 70 | ENEMY | 390 | 1.1 | 21.3 | 45 | T3 HP -1750, yıldırım |  |  |  | evet |  |
| 110771 | 210771 | Chain lightning | 70 | AREA_ENEMY | 600 | 1.3 | 18.3 | 45 | T3 HP -1470, süreli -420/0sn, yıldırım, r=15 |  |  |  | evet |  |
| 110772 | 210772 | Light Staff | 72 | ENEMY | 300 | 0.0 | 0.0 | 22 | T1 100% +100, isabet 100; T3 HP -2500, yıldırım |  |  | 515 |  |  |
| 110773 | 210773 | Lightning Armor | 75 | SELF | 250 | 1.5 | 25.0 | 0 | T4 MAGE_ARMOR(25) , 120sn | 379061000 |  | 516 |  |  |
| 110774 | 210774 | Blink | 80 | SELF | 100 | 0.0 | 21.3 | 0 | T8 warp=20 r=20 |  |  | 517 |  |  |

## Master — master

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110800 | 210800 | Bright Dew | 0 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 110802 | 210802 | Absolute power | 2 | SELF | 240 | 0.0 | 25.0 | 56 | T4 MAGIC_POWER(10) MagicAttack=130, 30sn | 379065000 | 379061000 |  |  |  |
| 110805 | 210805 | Absoluteness | 5 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 110810 | 210810 | Matchless | 10 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 110815 | 210815 | Mana Shield | 12 | SELF | 150 | 0.0 | 0.0 | 0 | T4 MANA_ABSORB(31) ExpPct=15, 40sn |  |  |  |  |  |
| 110820 | 210820 | Instantly Magic | 15 | SELF | 100 | 0.0 | 25.5 | 0 | T4 INSTANT_MAGIC(23) , 180sn |  |  |  |  |  |
| 110825 | 210825 | Minor Resist | 20 | AREA_ENEMY | 450 | 1.3 | 0.1 | 56 | T4 DECREASE_RESIST(24) FireR=20 ColdR=20 LightningR=20 MagicR=20 DiseaseR=20 PoisonR=20, 10sn, r=10 | 379061000 |  |  |  |  |

## (kullanılamaz: 9) — 

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 110036 | 210036 | whipping | 1 | SELF | 5 | 0.0 | 25.5 | 0 | T4 SPEED(6) Speed=200, 1200sn |  |  |  |  |  |
