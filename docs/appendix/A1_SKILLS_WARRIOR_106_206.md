# A1_SKILLS_WARRIOR_106_206 — Master Warrior Skill Kataloğu

> Otomatik üretildi: yerel `FDP_kn_online` veri tabanının salt okunur dökümünden (`MAGIC`, `MAGIC_TYPE1/3/4/5/7/8`), 2026-10-01. Elle değer girilmemiştir. Etiket: `[V]`.
> Birimler: Cast ve Recast **0,1 sn** biriminden saniyeye çevrildi (sunucu recast'i tam saniye çözünürlükle karşılaştırır, bkz. 03 MEC-MAG-02; cast süresini sunucu uygulamaz, MEC-MAG-01). Menzil metre. `Gerek` = ağaç puanı (Ağaç/Master) veya karakter seviyesi (Temel).
> El Morad ID = Karus ID + 100000. `Fark` sütunu El Morad satırının sayısal olarak farklı olduğu alanları gösterir (Karus değeri → El Morad değeri).
> Tip kısaltmaları: T1 yakın dövüş, T3 doğrudan hasar/heal, T4 buff/debuff, T5 cure/diriltme, T7 özel, T8 ışınlanma. `Etc ≠ 0` = quest kapısı (03 §4.3 U11). `Taş` = `BeforeAction` 1–4 olduğunda tüketilen sınıf taşı (03 §4.3 U9); bu durumda `UseItem` yalnızca gereksinimdir.


## Temel (seviye) — temel

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106001 | 206001 | sprint | 1 | SELF | 5 | 0.0 | 6.0 | 0 | T4 SPEED(6) Speed=150, 10sn |  |  |  |  |  |
| 106003 | 206003 | slash | 3 | ENEMY | 4 | 0.0 | 3.1 | 0 | T1 120%, isabet 100 |  |  |  |  |  |
| 106005 | 206005 | crash | 5 | ENEMY | 4 | 0.0 | 3.1 | 0 | T1 100%, isabet 150 |  |  |  |  |  |
| 106007 | 206007 | Defense | 7 | SELF | 4 | 0.0 | 10.0 | 0 | T4 AC(2) AC=50, 15sn |  |  |  |  |  |
| 106009 | 206009 | piercing | 9 | ENEMY | 4 | 0.0 | 3.1 | 0 | T1 100%, isabet 200 |  |  |  |  |  |

## Ağaç 5 — saldırı ağacı

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106500 | 206500 | Hash | 0 | ENEMY | 10 | 0.0 | 3.1 | 0 | T1 100% +30, isabet 100 |  |  |  |  |  |
| 106505 | 206505 | hoodwink | 5 | ENEMY | 30 | 0.0 | 0.5 | 0 | T1 150%, isabet 100 |  |  |  |  |  |
| 106510 | 206510 | Shear | 10 | ENEMY | 20 | 0.0 | 3.1 | 0 | T1 100% +50, isabet 100 |  |  |  |  |  |
| 106515 | 206515 | pierce | 15 | ENEMY | 60 | 0.0 | 0.5 | 0 | T1 100%, isabet 100 (sabit) |  |  |  |  |  |
| 106520 | 206520 | leg cutting | 20 | ENEMY | 84 | 0.0 | 5.1 | 0 | T1 100%, isabet 100; T4 SPEED(6) Speed=50, 10sn |  |  |  |  |  |
| 106525 | 206525 | Carving | 25 | ENEMY | 90 | 0.0 | 0.5 | 0 | T1 200%, isabet 100 |  |  |  |  |  |
| 106530 | 206530 | Sever | 30 | ENEMY | 40 | 0.0 | 3.1 | 0 | T1 100% +100, isabet 100 |  |  |  |  |  |
| 106535 | 206535 | prick | 35 | ENEMY | 120 | 0.0 | 0.5 | 0 | T1 150%, isabet 100 (sabit) |  |  |  |  |  |
| 106540 | 206540 | multiple shock | 40 | ENEMY | 60 | 0.0 | 3.1 | 0 | T1 150% +50, isabet 100 |  |  |  |  |  |
| 106545 | 206545 | Cleave | 45 | ENEMY | 150 | 0.0 | 0.5 | 0 | T1 150%, isabet 100 |  |  |  |  | T1_Hit 150→125 |
| 106550 | 206550 | mangling | 50 | ENEMY | 60 | 0.0 | 3.1 | 0 | T1 100% +150, isabet 100 |  |  |  |  |  |
| 106555 | 206555 | thrust | 55 | ENEMY | 200 | 0.0 | 0.5 | 0 | T1 100%, isabet 100 (sabit) |  |  |  |  |  |
| 106557 | 206557 | sword aura | 57 | ENEMY | 250 | 0.0 | 0.1 | 0 | T1 100% +250, isabet 100 (sabit) |  |  |  |  |  |
| 106560 | 206560 | sword dancing | 60 | ENEMY | 300 | 0.0 | 0.5 | 0 | T1 150% +150, isabet 100 (sabit) |  |  |  |  |  |
| 106570 | 206570 | Howling Sword | 70 | ENEMY | 400 | 0.0 | 0.8 | 0 | T1 200% +200, isabet 100 (sabit) |  |  |  | evet |  |
| 106575 | 206575 | blooding | 75 | ENEMY | 350 | 0.0 | 21.0 | 0 | T1 350% +400, isabet 100 (sabit); T3 MP, süreli -1000/20sn, büyü |  |  | 510 |  |  |
| 106580 | 206580 | Hell blade | 80 | ENEMY | 400 | 0.0 | 0.8 | 0 | T1 310% +300, isabet 100 (sabit) |  |  | 511 |  |  |

## Ağaç 6 — savunma ağacı

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106605 | 206605 | Hinder | 5 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 106610 | 206610 | resist | 10 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 106615 | 206615 | Arrest | 15 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 106620 | 206620 | endure | 20 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 106630 | 206630 | Binding | 30 | ENEMY | 30 | 0.0 | 6.5 | 67 | T7 Binding |  |  |  |  |  |
| 106635 | 206635 | Bulwark | 35 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 106640 | 206640 | immunity | 40 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 106645 | 206645 | provoke | 45 | AREA_ENEMY | 60 | 1.5 | 15.0 | 22 | T7 provoke |  |  |  |  |  |
| 106650 | 206650 | descent | 50 | PARTY | 50 | 0.0 | 9.1 | 225 | T8 warp=25 r=30 |  |  |  |  |  |
| 106655 | 206655 | evading | 55 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 106660 | 206660 | sacrifice | 60 | PARTY | 180/HP 10001 | 0.0 | 25.0 | 67 | T3 HP 10000 |  |  |  |  |  |
| 106670 | 206670 | Iron Skin | 70 | SELF | 0 | 0.0 | 10.1 | 0 |  |  |  |  | evet |  |
| 106675 | 206675 | wall of Iron | 75 | SELF | 250 | 0.0 | 20.0 | 0 | T4 WALL_OF_IRON(28) Speed=50 ACPct=300, 10sn |  |  | 510 |  |  |
| 106680 | 206680 | iron linker | 80 | SELF | 0 | 0.0 | 10.1 | 0 |  |  |  | 511 |  |  |

## Ağaç 7 — berserk/tutku ağacı

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106705 | 206705 | Gain | 5 | SELF | 10 | 0.0 | 9.1 | 0 | T4 STATS(7) Str=15, 300sn |  |  |  |  |  |
| 106710 | 206710 | pain killer | 10 | SELF | 0/HP 100 | 0.0 | 4.5 | 0 | T3 MP 200 |  |  |  |  |  |
| 106715 | 206715 | Rise | 15 | SELF | 30 | 0.0 | 9.1 | 0 | T4 STATS(7) Sta=10, 300sn |  |  |  |  |  |
| 106720 | 206720 | Outrage | 20 | SELF | 60 | 0.0 | 9.1 | 0 | T4 ATTACK_SPEED(5) AttackSpeed=120, 30sn |  |  |  |  |  |
| 106725 | 206725 | Blade of hate | 25 | ENEMY | 60 | 1.5 | 5.1 | 45 | T3 HP -150, büyü |  |  |  |  |  |
| 106730 | 206730 | restoration | 30 | SELF | 105 | 0.0 | 25.0 | 0 | T3 HP, süreli 750/60sn |  |  |  |  |  |
| 106731 | 206731 | Blaze Killer | 30 | SELF | 0/HP 200 | 0.0 | 6.5 | 0 | T3 MP 400 |  |  |  |  |  |
| 106735 | 206735 | Nimble Wind | 35 | SELF | 90 | 0.0 | 9.1 | 0 | T4 STATS(7) Dex=20, 300sn |  |  |  |  |  |
| 106740 | 206740 | Return to life | 40 | SELF | 500 | 0.0 | 5.1 | 0 | T3 HP 250 |  |  |  |  |  |
| 106750 | 206750 | Regeneration | 50 | SELF | 210 | 0.0 | 25.0 | 0 | T3 HP, süreli 1500/60sn |  |  |  |  |  |
| 106755 | 206755 | Frenzy | 55 | SELF | 150 | 0.0 | 9.1 | 0 | T4 ATTACK_SPEED(5) AttackSpeed=130, 30sn |  |  |  |  |  |
| 106760 | 206760 | Quake | 60 | AREA_ENEMY | 160 | 0.0 | 5.1 | 45 | T3 HP -500, büyü, r=10 |  |  |  |  |  |
| 106770 | 206770 | berserk Echo | 70 | SELF | 250 | 0.0 | 10.1 | 0 | T4 ATTACK_SPEED(5) AttackSpeed=140, 30sn |  |  |  | evet |  |
| 106775 | 206775 | Berserker | 75 | SELF | 300 | 0.0 | 25.5 | 0 | T4 BERSERKER(ATK_SPEED_ARMOR)(18) AC=-300 Attack=120, 30sn |  |  | 510 |  |  |
| 106780 | 206780 | HP Booster | 80 | SELF | 200 | 0.0 | 25.5 | 0 | T3 dt14 105 |  |  | 511 |  |  |

## Master — master

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106800 | 206800 | boldness | 0 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 106802 | 206802 | Scream | 2 | ENEMY | 300 | 0.0 | 10.1 | 0 | T1 250% +200, isabet 100 (sabit); T4 SPEED(6) Speed=1, 7sn | 379063000 | 379059000 |  |  |  |
| 106805 | 206805 | Absoluteness | 5 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 106810 | 206810 | Matchless | 10 | SELF | 0 | 0.0 | 0.0 | 0 |  |  |  |  |  |  |
| 106815 | 206815 | Exceed Break | 15 | ENEMY | 400 | 0.0 | 25.4 | 0 | T1 200%, isabet 100; T3 dt13 -1000 | 379059000 |  |  |  |  |
| 106820 | 206820 | Shock Stun | 20 | ENEMY | 250 | 0.0 | 25.2 | 0 | T1 175% +175, isabet 100; T3 dt0, yıldırım | 379059000 |  |  |  |  |

## (kullanılamaz: 9) — 

| Karus ID | El Morad ID | Ad | Gerek | Hedef | MP | Cast sn | Recast sn | Menzil | Etki | UseItem | Taş | Etc | Ayakta | Fark |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 106010 | 206010 | whipping | 1 | SELF | 5 | 0.0 | 25.5 | 0 | T4 SPEED(6) Speed=200, 1200sn |  |  |  |  |  |
