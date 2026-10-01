# Potions and consumables (FDP_kn_online, read-only extract)

Linkage: an ITEM row is "used" by casting the MAGIC row whose MagicNum = ITEM.Effect1 (client sends WIZ_MAGIC_PROCESS with that skill id). Server side (GameServer/MagicInstance.cpp) checks/consumes MAGIC.UseItem, not ITEM.Effect1: UserCanCast() requires CanUseItem(UseItem) (class/level/stack check) when UseItem<>0, and ConsumeItem() calls RobItem(UseItem) after a successful cast. ReCastTime is in 0.1 s units (server: `diff_ms < sReCastTime*100` -> fail; UNIXTIME has 1 s resolution). Potions are Type1=3 with MAGIC_TYPE3.Attribute=0; for Attribute-0 type-3 skills CheckSkillPrerequisites() erases the per-type cooldown, so potions do not block (and are not blocked by) other Type-3 casts, only by their own ReCastTime. canUsePotions() (debuff BUFF_TYPE_NO_POTIONS 153 / UNDEAD 155 etc.) blocks HP-heal skills whose UseItem item has Class=0 (i.e. real potions). ItemGroup=9 marks potion skills. All potion rows had Etc=1 in MAGIC_BAK_etc (quest-1 gate) and Etc=0 now.

## 1. Exact answers: 720 HP and 1920 MP

**720 HP** restore (MAGIC_TYPE3.DirectType=1, FirstDamage=720):

| ItemID | Item name | Kind | ReqLevel | BuyPrice | Countable | Weight | Effect1 -> MagicNum | Magic name | MAGIC.UseItem | Cast | ReCast (0.1 s) | SellingGroup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 389014000 | Water of favors (+0) | 97 | 1 | 7000 | 1 | 90 | 490014 | Water of favors | 0 (**0 = item is NOT checked nor consumed**) | 5 | 20 | 253 |
| 389063000 | Water of favors(Store) (+0) | 97 | 1 | 7000 | 1 | 90 | 490063 | Water of favors | 389063000 | 5 | 20 | 0 |
| 910004000 | Water of favors(Coloseum Event) (+0) | 97 | 1 | 4000 | 1 | 90 | 500004 | Water of favors | 910004000 | 5 | 20 | 0 |

**1920 MP** restore (MAGIC_TYPE3.DirectType=2, FirstDamage=1920):

| ItemID | Item name | Kind | ReqLevel | BuyPrice | Countable | Weight | Effect1 -> MagicNum | Magic name | MAGIC.UseItem | Cast | ReCast (0.1 s) | SellingGroup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 389020000 | Potion of soul (+0) | 97 | 1 | 15000 | 1 | 30 | 490020 | Potion of soul | 0 (**0 = item is NOT checked nor consumed**) | 5 | 20 | 253 |
| 389082000 | Potion of soul (+0) | 97 | 1 | 15000 | 1 | 30 | 490082 | Potion of soul | 0 (**0 = item is NOT checked nor consumed**) | 5 | 20 | 0 |
| 910010000 | Potion of soul(Coloseum Event) (+0) | 97 | 1 | 500 | 1 | 30 | 500010 | Portion of soul | 910010000 | 5 | 20 | 0 |

Note: the priest skill "Superior healing" (112545/212545, 1920 HP, 320 MP) and the "Great healing" description text "Heal 720 HP" (111527/112527/211527/212527, real Type3 value 960) also contain these numbers, but they are skills, not items. "Hardness" (111633/112633/...) description says 720 HP but MAGIC_TYPE4.MaxHP=480.

**Data quirk (important for a bot):** the "normal" NPC-shop versions 389014000 Water of favors (720 HP) and 389020000 / 389082000 Potion of soul (1920 MP), plus 389013000/389062000 Water of grace (360 HP) and 389019000/389081000 Potion of wisdom (960 MP), point to MAGIC rows whose UseItem = 0 (same in MAGIC_BAK_etc, so this is original data, not caused by the Etc fix). For these the server neither requires the item nor removes it on use: casting skill 490014 / 490020 / 490082 etc. heals without any item (and the stack never decreases). The "(Store)" twins (389063000 -> 490063, UseItem=389063000) and Coloseum-event versions are consumed normally. Side effect: the "No Potion" check in IsAvailable() only fires when UseItem<>0, so these UseItem=0 potions are not blocked by BUFF_TYPE_NO_POTIONS either. A bot should still own and use the real item (client UI requires it) but must not rely on the stack decreasing.

## 2. All HP / MP restoring consumables (Type3 heal/MP, linked through ITEM.Effect1)

DirectType 1 = HP (+heal), 2 = MP (players), 3 = MP (signed). first = instant amount, overtime = total HoT amount over dur seconds (ticks every 2 s).

| ItemID | Item name | Kind | Class | ReqLv | Buy | Countable | Weight | MagicNum | UseItem | Cast | ReCast | Moral | Range | DirectType | first | overtime | dur | radius | Etc(now/bak) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 310310010 | Iron Earring | 91 | 0 | 1 | 640 | 0 | 0 | 460006 | 310310010 | 15 | 250 | 1 | 0 | 1 | 0 | 50 | 60 | 0 | 0/0 |
| 379108000 | Green Pearl (+0) | 97 | 0 | 1 | 400 | 1 | 1 | 490210 | 379108000 | 5 | 25 | 1 | 56 | 1 | 0 | 750 | 30 | 0 | 0/1 |
| 389097000 | Persistent HP Scroll  (+0) | 97 | 0 | 1 | 20000 | 1 | 1 | 490132 | 389097000 | 16 | 16 | 31 | 67 | 1 | 0 | 4500 | 30 | 30 | 0/1 |
| 389059000 | Potion of recovery (+0) | 97 | 0 | 1 | 24000 | 1 | 0 | 490059 | 389059000 | 20 | 150 | 1 | 67 | 1 | 0 | 6000 | 180 | 0 | 0/1 |
| 389010000 | Holy water (+0) | 97 | 0 | 1 | 70 | 1 | 50 | 490010 | 389010000 | 5 | 20 | 1 | 56 | 1 | 45 | 0 | 0 | 0 | 0/1 |
| 910012000 | Holy water (+0) | 97 | 0 | 1 | 70 | 1 | 50 | 500012 | 910012000 | 5 | 20 | 1 | 56 | 1 | 45 | 0 | 0 | 0 | 0/1 |
| 389011000 | Water of life (+0) | 97 | 0 | 1 | 160 | 1 | 60 | 490011 | 389011000 | 5 | 20 | 1 | 56 | 1 | 90 | 0 | 0 | 0 | 0/1 |
| 389060000 | Water of life(Store) (+0) | 97 | 0 | 1 | 160 | 1 | 60 | 490060 | 389060000 | 5 | 25 | 1 | 56 | 1 | 90 | 0 | 0 | 0 | 0/1 |
| 910001000 | Water of life(Coloseum Event) (+0) | 97 | 0 | 1 | 200 | 1 | 60 | 500001 | 910001000 | 5 | 25 | 1 | 56 | 1 | 90 | 0 | 0 | 0 | 0/1 |
| 389012000 | Water of love (+0) | 97 | 0 | 1 | 600 | 1 | 70 | 490012 | 389012000 | 5 | 20 | 1 | 56 | 1 | 180 | 0 | 0 | 0 | 0/1 |
| 389061000 | Water of love(Store) (+0) | 97 | 0 | 1 | 600 | 1 | 70 | 490061 | 389061000 | 5 | 25 | 1 | 56 | 1 | 180 | 0 | 0 | 0 | 0/1 |
| 910002000 | Water of love(Coloseum Event) (+0) | 97 | 0 | 1 | 800 | 1 | 70 | 500002 | 910002000 | 5 | 25 | 1 | 56 | 1 | 180 | 0 | 0 | 0 | 0/1 |
| 389001000 | Pink Rice Cake (small) (+0) | 97 | 0 | 1 | 500 | 1 | 0 | 490001 | 389001000 | 0 | 0 | 1 | 56 | 1 | 200 | 0 | 0 | 0 | 0/1 |
| 389013000 | Water of grace (+0) | 97 | 0 | 1 | 2000 | 1 | 80 | 490013 | 0 | 5 | 20 | 1 | 56 | 1 | 360 | 0 | 0 | 0 | 0/1 |
| 389062000 | Water of grace(Store) (+0) | 97 | 0 | 1 | 2000 | 1 | 80 | 490062 | 0 | 5 | 25 | 1 | 56 | 1 | 360 | 0 | 0 | 0 | 0/1 |
| 910003000 | Water of grace(Coloseum Event) (+0) | 97 | 0 | 1 | 3000 | 1 | 80 | 500003 | 910003000 | 5 | 25 | 1 | 56 | 1 | 360 | 0 | 0 | 0 | 0/1 |
| 379204000 | Tyon Meat (+0) | 97 | 0 | 1 | 3000 | 1 | 10 | 490095 | 379204000 | 5 | 60 | 1 | 56 | 1 | 400 | 0 | 0 | 0 | 0/1 |
| 389002000 | Pink Rice Cake (middle) (+0) | 97 | 0 | 1 | 500 | 1 | 0 | 490002 | 389002000 | 0 | 0 | 1 | 56 | 1 | 400 | 0 | 0 | 0 | 0/1 |
| 800055000 | Cake (+0) | 255 | 0 | 1 | 50 | 0 | 10 | 490094 | 800055000 | 5 | 20 | 1 | 56 | 1 | 500 | 0 | 0 | 0 | 0/1 |
| 389014000 | Water of favors (+0) | 97 | 0 | 1 | 7000 | 1 | 90 | 490014 | 0 | 5 | 20 | 1 | 56 | 1 | 720 | 0 | 0 | 0 | 0/1 |
| 389063000 | Water of favors(Store) (+0) | 97 | 0 | 1 | 7000 | 1 | 90 | 490063 | 389063000 | 5 | 20 | 1 | 56 | 1 | 720 | 0 | 0 | 0 | 0/1 |
| 910004000 | Water of favors(Coloseum Event) (+0) | 97 | 0 | 1 | 4000 | 1 | 90 | 500004 | 910004000 | 5 | 20 | 1 | 56 | 1 | 720 | 0 | 0 | 0 | 0/1 |
| 389003000 | Pink Rice Cake (large) (+0) | 97 | 0 | 1 | 500 | 1 | 0 | 490003 | 389003000 | 0 | 0 | 1 | 56 | 1 | 800 | 0 | 0 | 0 | 0/1 |
| 800082000 | Pink Rice Cake (large) (+0) | 255 | 0 | 1 | 500 | 0 | 0 | 491016 | 800082000 | 0 | 0 | 1 | 56 | 1 | 800 | 0 | 0 | 0 | 0/1 |
| 389210000 | Holy Water of Ancient Grace (+0) | 97 | 0 | 1 | 8500 | 1 | 90 | 490601 | 389210000 | 5 | 25 | 1 | 56 | 1 | 810 | 0 | 0 | 0 | 0/0 |
| 389015000 | Water of bless (+0) | 97 | 0 | 1 | 10000 | 1 | 100 | 490015 | 389015000 | 5 | 20 | 1 | 56 | 1 | 1440 | 0 | 0 | 0 | 0/1 |
| 389064000 | Water of bless(Store) (+0) | 97 | 0 | 1 | 10000 | 1 | 100 | 490064 | 389064000 | 5 | 25 | 1 | 56 | 1 | 1440 | 0 | 0 | 0 | 0/1 |
| 910005000 | Water of bless(Coloseum Event) (+0) | 97 | 0 | 1 | 10000 | 1 | 100 | 500005 | 910005000 | 5 | 25 | 1 | 56 | 1 | 1440 | 0 | 0 | 0 | 0/1 |
| 389093000 | HP Scroll  (+0) | 97 | 0 | 1 | 25000 | 1 | 1 | 490131 | 389093000 | 16 | 16 | 31 | 67 | 1 | 2000 | 0 | 0 | 30 | 0/1 |
| 379104000 | Abyss Blessing (+0) | 97 | 0 | 1 | 950 | 1 | 50 | 490207 | 379104000 | 5 | 25 | 1 | 56 | 1 | 3600 | 3000 | 60 | 0 | 0/1 |
| 389030000 | Prayer of the laying on of hands (+0) | 97 | 4 | 1 | 4000 | 1 | 150 | 490030 | 389030000 | 20 | 55 | 2 | 56 | 1 | 10000 | 0 | 0 | 0 | 0/1 |
| 389016000 | Potion of spirit (+0) | 97 | 0 | 1 | 160 | 1 | 22 | 490016 | 389016000 | 5 | 20 | 1 | 56 | 2 | 120 | 0 | 0 | 0 | 0/1 |
| 389078000 | Potion of spirit (+0) | 97 | 0 | 1 | 160 | 1 | 22 | 490078 | 389078000 | 5 | 20 | 1 | 56 | 2 | 120 | 0 | 0 | 0 | 0/1 |
| 910006000 | Potion of spirit(Coloseum Event) (+0) | 97 | 0 | 1 | 100 | 1 | 22 | 500006 | 910006000 | 5 | 20 | 1 | 56 | 2 | 120 | 0 | 0 | 0 | 0/1 |
| 389004000 | Green  Rice Cake (small) (+0) | 97 | 0 | 1 | 500 | 1 | 0 | 490004 | 389004000 | 0 | 0 | 1 | 56 | 2 | 200 | 0 | 0 | 0 | 0/1 |
| 389017000 | Potion of intelligence (+0) | 97 | 0 | 1 | 600 | 1 | 24 | 490017 | 389017000 | 5 | 20 | 1 | 56 | 2 | 240 | 0 | 0 | 0 | 0/1 |
| 389079000 | Potion of intelligence (+0) | 97 | 0 | 1 | 600 | 1 | 24 | 490079 | 389079000 | 5 | 20 | 1 | 56 | 2 | 240 | 0 | 0 | 0 | 0/1 |
| 910007000 | Potion of intelligence(Coloseum Event) (+0) | 97 | 0 | 1 | 200 | 1 | 24 | 500007 | 910007000 | 5 | 20 | 1 | 56 | 2 | 240 | 0 | 0 | 0 | 0/1 |
| 389005000 | Green  Rice Cake (middle) (+0) | 97 | 0 | 1 | 500 | 1 | 0 | 490005 | 389005000 | 0 | 0 | 1 | 56 | 2 | 400 | 0 | 0 | 0 | 0/1 |
| 389018000 | Potion of sagacity (+0) | 97 | 0 | 1 | 2000 | 1 | 26 | 490018 | 389018000 | 5 | 20 | 1 | 56 | 2 | 480 | 0 | 0 | 0 | 0/1 |
| 389080000 | Potion of sagacity (+0) | 97 | 0 | 1 | 2000 | 1 | 26 | 490080 | 389080000 | 5 | 20 | 1 | 56 | 2 | 480 | 0 | 0 | 0 | 0/1 |
| 910008000 | Potion of sagacity(Coloseum Event) (+0) | 97 | 0 | 1 | 300 | 1 | 26 | 500008 | 910008000 | 5 | 20 | 1 | 56 | 2 | 480 | 0 | 0 | 0 | 0/1 |
| 389006000 | Green  Rice Cake (large) (+0) | 97 | 0 | 1 | 500 | 1 | 0 | 490006 | 389006000 | 0 | 0 | 1 | 56 | 2 | 800 | 0 | 0 | 0 | 0/1 |
| 800081000 | Green  Rice Cake (large) (+0) | 255 | 0 | 1 | 500 | 0 | 0 | 491015 | 800081000 | 0 | 0 | 1 | 56 | 2 | 800 | 0 | 0 | 0 | 0/1 |
| 389019000 | Potion of wisdom (+0) | 97 | 0 | 1 | 7000 | 1 | 28 | 490019 | 0 | 5 | 20 | 1 | 56 | 2 | 960 | 0 | 0 | 0 | 0/1 |
| 389081000 | Potion of wisdom (+0) | 97 | 0 | 1 | 7000 | 1 | 28 | 490081 | 0 | 5 | 20 | 1 | 56 | 2 | 960 | 0 | 0 | 0 | 0/1 |
| 910009000 | Potion of wisdom(Coloseum Event) (+0) | 97 | 0 | 1 | 400 | 1 | 28 | 500009 | 910009000 | 5 | 20 | 1 | 56 | 2 | 960 | 0 | 0 | 0 | 0/1 |
| 389020000 | Potion of soul (+0) | 97 | 0 | 1 | 15000 | 1 | 30 | 490020 | 0 | 5 | 20 | 1 | 56 | 2 | 1920 | 0 | 0 | 0 | 0/1 |
| 389082000 | Potion of soul (+0) | 97 | 0 | 1 | 15000 | 1 | 30 | 490082 | 0 | 5 | 20 | 1 | 56 | 2 | 1920 | 0 | 0 | 0 | 0/1 |
| 910010000 | Potion of soul(Coloseum Event) (+0) | 97 | 0 | 1 | 500 | 1 | 30 | 500010 | 910010000 | 5 | 20 | 1 | 56 | 2 | 1920 | 0 | 0 | 0 | 0/1 |
| 389220000 | Potion of Ancient Spirit (+0) | 97 | 0 | 1 | 17500 | 1 | 30 | 490701 | 389220000 | 5 | 20 | 1 | 56 | 2 | 2160 | 0 | 0 | 0 | 0/0 |
| 800019000 | Clarity potion (+0) | 255 | 0 | 1 | 1000 | 0 | 1 | 500037 | 800019000 | 0 | 0 | 1 | 0 | 2 | 3600 | 3000 | 60 | 0 | 0/1 |
| 389184000 | Super MP Shell [Lunar War] (+0) | 97 | 0 | 1 | 6000 | 1 | 30 | 490113 | 389184000 | 16 | 30 | 7 | 45 | 3 | 10 | 0 | 0 | 10 | 0/0 |
| 389085000 | Super MP Shell (+0) | 97 | 0 | 1 | 20000 | 1 | 30 | 490102 | 389085000 | 16 | 30 | 7 | 45 | 3 | 20 | 0 | 0 | 10 | 0/0 |
| 389183000 | MP Shell [Lunar War] (+0) | 97 | 0 | 1 | 4000 | 1 | 30 | 490112 | 389183000 | 16 | 30 | 7 | 45 | 3 | 20 | 0 | 0 | 10 | 0/0 |
| 389084000 | MP Shell (+0) | 97 | 0 | 1 | 12500 | 1 | 30 | 490101 | 389084000 | 16 | 30 | 7 | 45 | 3 | 50 | 0 | 0 | 5 | 0/0 |
| 389086000 | Hyper MP Shell (+0) | 97 | 0 | 1 | 100000 | 1 | 30 | 490103 | 389086000 | 16 | 30 | 7 | 11 | 3 | 80 | 0 | 0 | 5 | 0/0 |

HP/MP Type3 MAGIC rows (>=400000) that no ITEM references via Effect1: 500036 PowerUP potion(test) (UseItem 800016000, dt1 first=3600 overtime=3000)

## 3. Other relevant consumables (full list in consumables_other.csv)

### Speed / attack-speed potions (Type4)

| ItemID | Item name | Kind | ReqLv | Buy | Countable | MagicNum | UseItem | ReCast | Moral | Type details |
|---|---|---|---|---|---|---|---|---|---|---|
| 389007000 | White Rice Cake (Increase Running Speed) (+0) | 97 | 1 | 500 | 1 | 490007 | 389007000 | 0 | 1 | T4 SPEED(6) dur=300s Speed=150 |
| 389052000 | Speed up liquid medicine (+0) | 97 | 1 | 3600 | 1 | 490052 | 389052000 | 150 | 1 | T4 ATTACK_SPEED(5) dur=60s AttackSpeed=80 |
| 389054000 | Alacrity potion (+0) | 97 | 1 | 2400 | 1 | 490054 | 389054000 | 150 | 1 | T4 ACCURACY(9) dur=300s HitRate=255 AvoidRate=255 |
| 389096000 | Attack Speed Scroll  (+0) | 97 | 1 | 20000 | 1 | 490135 | 389096000 | 16 | 31 | T4 ATTACK_SPEED(5) dur=45s radius=30 AttackSpeed=130 |
| 800015000 | Speed+ Potion (+0) | 255 | 1 | 2000 | 0 | 500035 | 800015000 | 255 | 1 | T4 SPEED(6) dur=1800s Speed=150 |
| 800035000 | Speed Up Rice Cake (+0) | 255 | 1 | 2000 | 0 | 501003 | 800035000 | 0 | 1 | T4 SPEED(6) dur=28800s Speed=150 |
| 810077000 | Speed-Up Potion (+0) | 255 | 1 | 2000 | 0 | 500064 | 810077000 | 0 | 1 | T4 SPEED(6) dur=1800s Speed=150 |
| 910114000 | Haste Potion (+0) | 255 | 1 | 2000 | 0 | 500057 | 910114000 | 0 | 1 | T4 SPEED(6) dur=1800s Speed=150 |
| 910142000 | Haste Potion (+0) | 255 | 1 | 2000 | 0 | 500063 | 910142000 | 0 | 1 | T4 SPEED(6) dur=10800s Speed=150 |

### Buff scrolls / prayers / resistance potions (Type4)

| ItemID | Item name | Kind | ReqLv | Buy | Countable | MagicNum | UseItem | ReCast | Moral | Type details |
|---|---|---|---|---|---|---|---|---|---|---|
| 379098000 | Sweeping Potion (+0) | 97 | 1 | 2500 | 1 | 490201 | 379098000 | 250 | 1 | T4 MAGIC_POWER(10) dur=30s MagicAttack=130 |
| 389008000 | White Rice Cake (Increase Defense) (+0) | 97 | 1 | 500 | 1 | 490008 | 389008000 | 0 | 1 | T4 AC(2) dur=300s ACPct=200 |
| 389009000 | White Rice Cake (Increase HP) (+0) | 97 | 1 | 500 | 1 | 490009 | 389009000 | 0 | 1 | T4 HP_MP(1) dur=300s MaxHP=240 |
| 389023000 | Spell of life (+0) | 97 | 1 | 3600 | 1 | 490023 | 389023000 | 55 | 1 | T4 HP_MP(1) dur=300s MaxHP=480 |
| 389025000 | Net (+0) | 95 | 1 | 360 | 1 | 490025 | 389025000 | 55 | 7 | T4 SPEED(6) dur=10s Speed=50 |
| 389026000 | Prayer of god`s power (+0) | 97 | 1 | 2400 | 1 | 490026 | 0 | 55 | 1 | T4 DAMAGE(4) dur=100s Attack=150 |
| 389027000 | Prayer of wind (+0) | 97 | 1 | 2500 | 1 | 490027 | 389027000 | 55 | 1 | T4 SPEED(6) dur=300s Speed=140 |
| 389029000 | Prayer of earth (+0) | 97 | 1 | 3500 | 1 | 490029 | 389029000 | 55 | 7 | T4 SPEED(6) dur=10s Speed=1 |
| 389031000 | Prayer of holy Armor (+0) | 97 | 1 | 4500 | 1 | 490031 | 389031000 | 55 | 1 | T4 AC(2) dur=30s ACPct=1000 Attack=1 |
| 389034000 | Bezoar  (+0) | 97 | 1 | 1000 | 1 | 490034 | 389034000 | 55 | 1 | T4 SIZE(3) dur=60s |
| 389035000 | glutinous Rice Cake (+0) | 97 | 1 | 1000 | 1 | 490035 | 389035000 | 55 | 1 | T4 SIZE(3) dur=60s |
| 389037000 | Talisman (+0) | 97 | 1 | 1000 | 1 | 490037 | 389037000 | 55 | 1 | T4:(no row) |
| 389038000 | Flame Resistance Potion (+0) | 97 | 1 | 2500 | 1 | 490038 | 389038000 | 55 | 1 | T4 RESISTANCES(8) dur=60s FireR=80 |
| 389039000 | Glacier Resistance Potion (+0) | 97 | 1 | 2500 | 1 | 490039 | 389039000 | 55 | 1 | T4 RESISTANCES(8) dur=60s ColdR=80 |
| 389040000 | Lightning Resistnace Potion (+0) | 97 | 1 | 2500 | 1 | 490040 | 389040000 | 55 | 1 | T4 RESISTANCES(8) dur=60s LightningR=80 |
| 389041000 | Spell of Lion`s Strength (+0) | 97 | 1 | 2500 | 1 | 490041 | 389041000 | 55 | 1 | T4 STATS(7) dur=300s Str=15 Sta=15 Dex=15 Intel=15 Cha=15 FireR=50 ColdR=50 LightningR=50 MagicR=50 DiseaseR=50 PoisonR=50 |
| 389045000 | Reserve Life (+0) | 97 | 1 | 5400 | 1 | 490045 | 389045000 | 150 | 7 | T4 HP_MP(1) dur=1s MaxHPPct=99 |
| 389047000 | Shrinking Potion (+0) | 97 | 1 | 1000 | 1 | 490035 | 389035000 | 55 | 1 | T4 SIZE(3) dur=60s |
| 389047500 | Enlarging Potion (+0) | 97 | 1 | 100 | 1 | 490034 | 389034000 | 55 | 1 | T4 SIZE(3) dur=60s |
| 389053000 | Attack up liquid medicine (+0) | 97 | 1 | 3600 | 1 | 490053 | 389053000 | 150 | 1 | T4 DAMAGE(4) dur=300s Attack=150 |
| 389055000 | Scroll of shell (+0) | 97 | 1 | 4200 | 1 | 490055 | 389055000 | 150 | 1 | T4 AC(2) dur=300s ACPct=600 |
| 389056000 | Potion of intelligence (+0) | 97 | 1 | 5400 | 1 | 490056 | 389056000 | 150 | 1 | T4 STATS(7) dur=300s Intel=20 |
| 389094000 | Defense Scroll  (+0) | 97 | 1 | 20000 | 1 | 490133 | 389094000 | 16 | 31 | T4 AC(2) dur=45s radius=30 AC=500 |
| 389095000 | Resistance Scroll  (+0) | 97 | 1 | 20000 | 1 | 490134 | 389095000 | 16 | 31 | T4 RESISTANCES(8) dur=45s radius=30 FireR=250 ColdR=250 LightningR=250 MagicR=250 DiseaseR=250 PoisonR=250 |
| 389098000 | Magic Attack Scroll  (+0) | 97 | 1 | 20000 | 1 | 490136 | 389098000 | 16 | 31 | T4 MAGIC_POWER(10) dur=45s radius=30 MagicAttack=130 |
| 389099000 | Physical Attack Scroll  (+0) | 97 | 1 | 20000 | 1 | 490137 | 389099000 | 16 | 31 | T4 DAMAGE(4) dur=45s radius=30 Attack=130 |
| 389100000 | Defense Scroll for Ladder Truck (+0) | 97 | 1 | 20000 | 1 | 490121 | 389100000 | 16 | 31 | T4:(no row) |
| 389155000 | Weapon Enchant Scroll (+0) | 97 | 1 | 700000 | 1 | 500049 | 0 | 0 | 1 | T4 WEAPON_DAMAGE(13) dur=1800s Attack=5 |
| 389156000 | Armor Enchant Scroll (+0) | 97 | 1 | 300000 | 1 | 500050 | 0 | 0 | 1 | T4 WEAPON_AC(14) dur=1800s AC=30 |
| 800003000 | STR+ Scroll(Stat)(L) (+0) | 255 | 1 | 2000 | 0 | 500023 | 800003000 | 0 | 1 | T4 STATS(7) dur=1800s Str=15 |
| 800004000 | HP+ Scroll(Stat)(L) (+0) | 255 | 1 | 2000 | 0 | 500024 | 800004000 | 0 | 1 | T4 STATS(7) dur=1800s Sta=15 |
| 800005000 | DEX+ Scroll(Stat)(L) (+0) | 255 | 1 | 2000 | 0 | 500025 | 800005000 | 0 | 1 | T4 STATS(7) dur=1800s Dex=15 |
| 800006000 | INT+ Scroll(Stat)(L) (+0) | 255 | 1 | 2000 | 0 | 500026 | 800006000 | 0 | 1 | T4 STATS(7) dur=1800s Intel=15 |
| 800007000 | MP+ Scroll(Stat)(L) (+0) | 255 | 1 | 2000 | 0 | 500027 | 800007000 | 0 | 1 | T4 STATS(7) dur=1800s Cha=15 |
| 800008000 | Power of Lion Scroll(Stat)(L) (+0) | 255 | 1 | 2000 | 0 | 500028 | 800008000 | 0 | 1 | T4 STATS(7) dur=1800s Str=10 Sta=10 Dex=10 Intel=10 Cha=10 |
| 800009000 | 150 Defense+ Scroll(L) (+0) | 255 | 1 | 2000 | 0 | 500029 | 800009000 | 0 | 1 | T4 AC(2) dur=1800s AC=150 |
| 800010000 | 300 Defense+ Scroll(L) (+0) | 255 | 1 | 2000 | 0 | 500030 | 800010000 | 0 | 1 | T4 AC(2) dur=1800s AC=300 |
| 800011000 | 500 HP+ Scroll(L) (+0) | 255 | 1 | 2000 | 0 | 500031 | 800011000 | 0 | 1 | T4 HP_MP(1) dur=1800s MaxHP=500 |
| 800012000 | 1000 HP+ Scroll(L) (+0) | 255 | 1 | 2000 | 0 | 500032 | 800012000 | 0 | 1 | T4 HP_MP(1) dur=1800s MaxHP=1000 |
| 800013000 | 1500 HP+ Scroll(L) (+0) | 255 | 1 | 2000 | 0 | 500033 | 800013000 | 255 | 1 | T4 HP_MP(1) dur=1800s MaxHP=1500 |
| 800014000 | Attack+ Scroll (+0) | 255 | 1 | 2000 | 0 | 500034 | 800014000 | 255 | 1 | T4 DAMAGE(4) dur=1800s Attack=120 |
| 800023000 | STR+ Scroll(Stat)(S) (+0) | 255 | 1 | 2000 | 0 | 500040 | 800023000 | 0 | 1 | T4 STATS(7) dur=1800s Str=15 |
| 800024000 | HP+ Scroll(Stat)(S) (+0) | 255 | 1 | 2000 | 0 | 500041 | 800024000 | 0 | 1 | T4 STATS(7) dur=1800s Sta=15 |
| 800025000 | DEX+ Scroll(Stat)(S) (+0) | 255 | 1 | 2000 | 0 | 500042 | 800025000 | 0 | 1 | T4 STATS(7) dur=1800s Dex=15 |
| 800026000 | INT+ Scroll(Stat)(S) (+0) | 255 | 1 | 2000 | 0 | 500043 | 800026000 | 0 | 1 | T4 STATS(7) dur=1800s Intel=15 |
| 800027000 | MP+ Scroll(Stat)(S) (+0) | 255 | 1 | 2000 | 0 | 500044 | 800027000 | 0 | 1 | T4 STATS(7) dur=1800s Cha=15 |
| 800028000 | Power of Lion Scroll(Stat)(S) (+0) | 255 | 1 | 2000 | 0 | 500045 | 800028000 | 0 | 1 | T4 STATS(7) dur=1800s Str=10 Sta=10 Dex=10 Intel=10 Cha=10 |
| 800029000 | 150 Defense+ Scroll(S) (+0) | 255 | 1 | 2000 | 0 | 500046 | 800029000 | 0 | 1 | T4 AC(2) dur=1800s AC=150 |
| 800030000 | 500 HP+ Scroll(S) (+0) | 255 | 1 | 2000 | 0 | 500047 | 800030000 | 0 | 1 | T4 HP_MP(1) dur=1800s MaxHP=500 |
| 800031000 | 1000 HP+ Scroll(S) (+0) | 255 | 1 | 2000 | 0 | 500048 | 800031000 | 0 | 1 | T4 HP_MP(1) dur=1800s MaxHP=1000 |
| 800033000 | HP Rice Cake (+0) | 255 | 1 | 2000 | 0 | 501001 | 800033000 | 0 | 1 | T4 HP_MP(1) dur=28800s MaxHP=1500 |
| 800034000 | MP Rice Cake (+0) | 255 | 1 | 2000 | 0 | 501002 | 800034000 | 0 | 1 | T4 HP_MP(1) dur=28800s MaxMP=1500 |
| 800050000 | Mount Scroll (+0) | 255 | 1 | 1500 | 0 | 501005 | 800050000 | 0 | 1 | T4 EXPERIENCE(11) dur=3600s ExpPct=150 |
| 800051000 | Ascent Scroll (+0) | 255 | 1 | 1500 | 0 | 501006 | 800051000 | 0 | 1 | T4 EXPERIENCE(11) dur=3600s ExpPct=120 |
| 800060000 | Weight Scroll  (+0) | 255 | 1 | 2000 | 0 | 501010 | 800060000 | 0 | 1 | T4 WEIGHT(12) dur=3600s ExpPct=120 |
| 800061000 | Weapon Enchant Scroll (+0) | 255 | 1 | 2000 | 0 | 500051 | 800061000 | 0 | 1 | T4 WEAPON_DAMAGE(13) dur=1800s Attack=5 |
| 800062000 | Armor Enchant Scroll (+0) | 255 | 1 | 2000 | 0 | 500052 | 800062000 | 0 | 1 | T4 WEAPON_AC(14) dur=1800s AC=30 |
| 800074000 | NP Increase Scroll(+0) | 255 | 1 | 2000 | 0 | 504001 | 800074000 | 0 | 1 | T4 LOYALTY(15) dur=3600s ExpPct=150 |
| 800076000 | Scroll of Armor 350 (+0) | 255 | 1 | 2000 | 0 | 500055 | 800076000 | 0 | 1 | T4 AC(2) dur=1800s AC=350 |
| 800077000 | Strengthen Defense Power 350 Scroll (+0) | 255 | 1 | 2000 | 0 | 500056 | 800077000 | 0 | 1 | T4 AC(2) dur=1800s AC=400 |
| 800078000 | HP Scroll 2000 (+0) | 255 | 1 | 2000 | 0 | 500053 | 800078000 | 0 | 1 | T4 HP_MP(1) dur=1800s MaxHP=2000 |
| 800079000 | Increase HP 60% Scroll  (+0) | 255 | 1 | 2000 | 0 | 500054 | 800079000 | 0 | 4 | T4 HP_MP(1) dur=1800s MaxHPPct=160 |
| 800091000 | Scroll of  Advanced Strength (+0) | 255 | 1 | 2000 | 0 | 500501 | 810008000 | 0 | 1 | T4 STATS(7) dur=1800s Str=15 Sta=15 Dex=15 Intel=15 Cha=15 |
| 800092000 | Scroll of  Advanced Health (+0) | 255 | 1 | 2000 | 0 | 500502 | 800023000 | 0 | 1 | T4 STATS(7) dur=1800s Str=25 |
| 800093000 | Scroll of  Advanced Dexterity (+0) | 255 | 1 | 2000 | 0 | 500503 | 800024000 | 0 | 1 | T4 STATS(7) dur=1800s Sta=25 |
| 800094000 | Scroll of Advanced Lion Strength (+0) | 255 | 1 | 2000 | 0 | 500504 | 800025000 | 0 | 1 | T4 STATS(7) dur=1800s Dex=25 |
| 810060000 | Symbol of Gladiator  (+0) | 255 | 1 | 2000 | 0 | 500111 | 810060000 | 0 | 1 | T4 EXPERIENCE(11) dur=3600s ExpPct=170 |
| 810061000 | Symbol of Warrior (+0) | 255 | 1 | 2500 | 0 | 500112 | 810061000 | 0 | 1 | T4 EXPERIENCE(11) dur=3600s ExpPct=140 |
| 810073000 | Symbol of Gladiator [5 ea] (+0) | 255 | 1 | 2000 | 0 | 500113 | 810073000 | 0 | 1 | T4 EXPERIENCE(11) dur=3600s ExpPct=170 |
| 810074000 | Symbol of Warrior[5 ea] (+0) | 255 | 1 | 2500 | 0 | 500114 | 810074000 | 0 | 1 | T4 EXPERIENCE(11) dur=3600s ExpPct=140 |
| 810075000 | Symbol of Gladiator[10 ea] (+0) | 255 | 1 | 2000 | 0 | 500115 | 810075000 | 0 | 1 | T4 EXPERIENCE(11) dur=3600s ExpPct=170 |
| 810076000 | Symbol of Warrior[10 ea] (+0) | 255 | 1 | 2500 | 0 | 500116 | 810076000 | 0 | 1 | T4 EXPERIENCE(11) dur=3600s ExpPct=140 |
| 810078000 | Scroll of Armor 300 (+0) | 255 | 1 | 2000 | 0 | 500065 | 810078000 | 0 | 1 | T4 AC(2) dur=1800s AC=300 |
| 810079000 | HP Scroll 1500 (+0) | 255 | 1 | 2000 | 0 | 500066 | 810079000 | 0 | 1 | T4 HP_MP(1) dur=1800s MaxHP=1500 |
| 810080000 | Scroll of Armor 350 (+0) | 255 | 1 | 2000 | 0 | 500067 | 810080000 | 1 | 1 | T4 AC(2) dur=1800s AC=350 |
| 810081000 | HP Scroll 2000 (+0) | 255 | 1 | 2000 | 0 | 500068 | 810081000 | 80 | 1 | T4 HP_MP(1) dur=1800s MaxHP=2000 |
| 910115000 | Maximum HP Increase Potion (for quests) (+0) | 255 | 1 | 2000 | 0 | 500058 | 910115000 | 0 | 1 | T4 HP_MP(1) dur=1800s MaxHP=100 |
| 910116000 | Maximum MP Increase Potion (for quests) (+0) | 255 | 1 | 2000 | 0 | 500059 | 910116000 | 0 | 1 | T4 HP_MP(1) dur=1800s MaxMP=100 |
| 910139000 | Scroll of Armor 300 (+0) | 255 | 1 | 2000 | 0 | 500060 | 910139000 | 0 | 1 | T4 AC(2) dur=10800s AC=300 |
| 910140000 | HP Scroll 1500 (+0) | 255 | 1 | 2000 | 0 | 500061 | 910140000 | 0 | 1 | T4 HP_MP(1) dur=10800s MaxHP=1500 |
| 910141000 | Scroll of Attack (+0) | 255 | 1 | 2000 | 0 | 500062 | 910141000 | 0 | 1 | T4 DAMAGE(4) dur=10800s Attack=120 |

### Transformation items (Type6 / Type1=0 with Type2)

| ItemID | Item name | Kind | ReqLv | Buy | Countable | MagicNum | UseItem | ReCast | Moral | Type details |
|---|---|---|---|---|---|---|---|---|---|---|
| 379090000 | Transformation Totem (+0) | 93 | 1 | 20000 | 0 | 470001 | 379090000 | 0 | 1 | T1:(no row) |
| 379093000 | Transformation Totem (+0) | 93 | 1 | 20000 | 0 | 471001 | 379093000 | 0 | 1 | T1:(no row) |
| 381001000 | Transformation Scroll  (+0) | 97 | 1 | 400000 | 1 | 472001 | 381001000 | 0 | 1 | T1:(no row) |
| 800022000 | Duration Item (+0) | 255 | 1 | 2000 | 0 | 500039 | 800022000 | 0 | 240 | (type 0) |
| 800075000 | Logos's Shout (+0) | 255 | 1 | 2000 | 0 | 504002 | 800075000 | 0 | 1 | (type 0) |
| 800101000 | Imir's Prayer (+0) | 255 | 1 | 2000 | 0 | 500122 | 800101000 | 0 | 1 | (type 0) |
| 910143000 | Logos's Shout (+0) | 255 | 1 | 2000 | 0 | 504007 | 910143000 | 0 | 1 | (type 0) |
| 379134000 | Snowman(El Morad) (+0) | 97 | 1 | 120000 | 1 | 478001 | 379134000 | 0 | 1 | T6 transform=2701 dur=1200 hp=0 mp=0 spd=0; T4:(no row) |
| 379135000 | Snowman(Karus) (+0) | 97 | 1 | 120000 | 1 | 478002 | 379135000 | 0 | 1 | T6 transform=2702 dur=1200 hp=0 mp=0 spd=0; T4:(no row) |
| 379174000 | El Morad groom (+0) | 255 | 1 | 10000 | 1 | 478003 | 379174000 | 0 | 1 | T6 transform=14101 dur=7200 hp=0 mp=0 spd=0; T4 MANA_ABSORB(31) dur=1800s |
| 379175000 | El Morad bride (+0) | 255 | 1 | 10000 | 1 | 478004 | 379175000 | 0 | 1 | T6 transform=14102 dur=7200 hp=0 mp=0 spd=0; T4 MANA_ABSORB(31) dur=1800s |
| 379176000 | Karus groom (+0) | 255 | 1 | 10000 | 1 | 478005 | 379176000 | 0 | 1 | T6 transform=24101 dur=7200 hp=0 mp=0 spd=0; T4 MANA_ABSORB(31) dur=1800s |
| 379177000 | Karus bride (+0) | 255 | 1 | 10000 | 1 | 478006 | 379177000 | 0 | 1 | T6 transform=24102 dur=7200 hp=0 mp=0 spd=0; T4 MANA_ABSORB(31) dur=1800s |
| 379194000 | Snowman Poroom~ (+0) | 97 | 1 | 2400 | 1 | 478007 | 379194000 | 0 | 1 | T6 transform=2703 dur=1200 hp=0 mp=0 spd=0; T4:(no row) |
| 379195000 | Snowman Teeti (+0) | 97 | 1 | 2400 | 1 | 478008 | 379195000 | 0 | 1 | T6 transform=2704 dur=1200 hp=0 mp=0 spd=0; T4:(no row) |
| 379196000 | Snowman Theo (+0) | 97 | 1 | 2400 | 1 | 478009 | 379196000 | 0 | 1 | T6 transform=2705 dur=1200 hp=0 mp=0 spd=0; T4:(no row) |
| 379197000 | Snowman Nunu (+0) | 97 | 1 | 2400 | 1 | 478010 | 379197000 | 0 | 1 | T6 transform=2706 dur=1200 hp=0 mp=0 spd=0; T4:(no row) |
| 389101000 | Catapult Key (+0) | 97 | 1 | 1250000 | 1 | 450001 | 389101000 | 0 | 1 | T6 transform=30010 dur=9000 hp=15000 mp=5000 spd=1 |
| 389102000 | Ram Key (+0) | 97 | 1 | 1250000 | 1 | 450002 | 389102000 | 0 | 1 | T6 transform=30040 dur=9000 hp=20000 mp=3000 spd=3 |
| 389103000 | Ladder Truck Key (+0) | 97 | 1 | 2500000 | 1 | 450003 | 389103000 | 0 | 1 | T6 transform=30060 dur=9000 hp=30000 mp=2000 spd=3 |
| 389104000 | Leader Car Key (+0) | 97 | 1 | 1250000 | 1 | 450004 | 389104000 | 0 | 1 | T6 transform=30050 dur=9000 hp=30000 mp=5000 spd=2 |
| 389151000 | Catapult Key (+0) | 97 | 1 | 0 | 1 | 450011 | 389151000 | 0 | 1 | T6 transform=30010 dur=9000 hp=15000 mp=5000 spd=1 |
| 389152000 | Ram Key (+0) | 97 | 1 | 0 | 1 | 450012 | 389152000 | 0 | 1 | T6 transform=30040 dur=9000 hp=20000 mp=3000 spd=3 |
| 389153000 | Ladder Truck Key (+0) | 97 | 1 | 0 | 1 | 450013 | 389153000 | 0 | 1 | T6 transform=30060 dur=9000 hp=30000 mp=2000 spd=3 |
| 389154000 | Leader Car Key (+0) | 97 | 1 | 0 | 1 | 450014 | 389154000 | 0 | 1 | T6 transform=30050 dur=9000 hp=30000 mp=5000 spd=2 |
| 389182000 | Catapult Key [Lunar War] (+0) | 97 | 1 | 500000 | 1 | 450005 | 389182000 | 0 | 1 | T6 transform=30010 dur=9000 hp=15000 mp=5000 spd=0 |
| 800100000 | Patrick Transform Scroll(El Morad) (+0) | 255 | 1 | 2000 | 0 | 500505 | 800100000 | 0 | 1 | T6 transform=26008 dur=3600 hp=0 mp=0 spd=0; T4:(no row) |
| 800110000 | Menissiah Transform Scroll(El Morad) (+0) | 255 | 1 | 2000 | 0 | 500506 | 800110000 | 0 | 1 | T6 transform=26009 dur=3600 hp=0 mp=0 spd=0; T4:(no row) |
| 800120000 | Cougar Transform Scroll(El Morad) (+0) | 255 | 1 | 2000 | 0 | 500507 | 800120000 | 0 | 1 | T6 transform=26010 dur=3600 hp=0 mp=0 spd=0; T4:(no row) |
| 800130000 | Hera Transform Scroll(El Morad) (+0) | 255 | 1 | 2000 | 0 | 500508 | 800130000 | 0 | 1 | T6 transform=26011 dur=3600 hp=0 mp=0 spd=0; T4:(no row) |
| 800190000 | Patrick Transform Scroll(Karus) (+0) | 255 | 1 | 2000 | 0 | 500509 | 800190000 | 0 | 1 | T6 transform=26012 dur=3600 hp=0 mp=0 spd=0; T4:(no row) |
| 800200000 | Menissiah Transform Scroll(Karus) (+0) | 255 | 1 | 2000 | 0 | 500510 | 800200000 | 0 | 1 | T6 transform=26013 dur=3600 hp=0 mp=0 spd=0; T4:(no row) |
| 800210000 | Cougar Transform Scroll(Karus) (+0) | 255 | 1 | 2000 | 0 | 500511 | 800210000 | 0 | 1 | T6 transform=26014 dur=3600 hp=0 mp=0 spd=0; T4:(no row) |
| 800220000 | Hera Transform Scroll(Karus) (+0) | 255 | 1 | 2000 | 0 | 500512 | 800220000 | 0 | 1 | T6 transform=26015 dur=3600 hp=0 mp=0 spd=0; T4:(no row) |

### Resurrection / sleep / teleport / stealth items (Type5/7/8/9)

| ItemID | Item name | Kind | ReqLv | Buy | Countable | MagicNum | UseItem | ReCast | Moral | Type details |
|---|---|---|---|---|---|---|---|---|---|---|
| 800036000 | 60% re-spawn scroll (+0) | 255 | 1 | 2500 | 0 | 480003 | 800036000 | 2 | 1 | T5 type=4 exprecover=60 needstone=0 |
| 800041000 | Resurrection Scroll(50) (+0) | 97 | 1 | 50 | 1 | 480005 | 800041000 | 2 | 1 | T5 type=4 exprecover=50 needstone=0 |
| 910022000 | Resurrection Scroll(50) (+0) | 97 | 1 | 50 | 1 | 480002 | 910022000 | 2 | 1 | T5 type=4 exprecover=50 needstone=0 |
| 379114000 | Abyss Lullaby (+0) | 97 | 1 | 1500 | 1 | 490213 | 379114000 | 90 | 7 | T7 group=0 monster=0 target=2 state=0 radius=0 hitrate=100 dur=20 dmg=0 |
| 389042000 | Retrieving Clan Member (+0) | 97 | 1 | 120 | 1 | 490042 | 389042000 | 250 | 15 | T8 target=1 radius=10000 warp=12(summon target (same zone)) exprecover=0 kick=0 |
| 389050000 | Calling Friend Scroll  (+0) | 97 | 1 | 12000 | 1 | 490050 | 389050000 | 250 | 6 | T8 target=1 radius=10000 warp=12(summon target (same zone)) exprecover=0 kick=0 |
| 389175000 | Chamber of Arrogance Transport Scroll (+0) | 97 | 1 | 10000 | 1 | 490301 | 389175000 | 30 | 1 | T8:(no row) |
| 389176000 | Chamber of Gluttony Transport Scroll (+0) | 97 | 1 | 20000 | 1 | 490302 | 389176000 | 30 | 1 | T8:(no row) |
| 389177000 | Chamber of Rage Transport Scroll (+0) | 97 | 1 | 30000 | 1 | 490303 | 389177000 | 30 | 1 | T8:(no row) |
| 389178000 | Chamber of Sloth Transport Scroll (+0) | 97 | 1 | 40000 | 1 | 490304 | 389178000 | 30 | 1 | T8:(no row) |
| 389179000 | Chamber of Lechery Transport Scroll (+0) | 97 | 1 | 50000 | 1 | 490305 | 389179000 | 30 | 1 | T8:(no row) |
| 389180000 | Chamber of Jealousy Transport Scroll (+0) | 97 | 1 | 60000 | 1 | 490306 | 389180000 | 30 | 1 | T8:(no row) |
| 389181000 | Chamber of Avarice Transport Scroll (+0) | 97 | 1 | 70000 | 1 | 490307 | 389181000 | 30 | 1 | T8:(no row) |
| 700003000 | Monster Summon Staff (+0) | 255 | 1 | 2500 | 0 | 490088 | 700003000 | 250 | 1 | T8 target=1 radius=10000 warp=21(summon monster) exprecover=0 kick=0 |
| 700004000 | Monster Summon Staff (+0) | 255 | 1 | 2500 | 0 | 490093 | 700004000 | 250 | 1 | T8 target=1 radius=10000 warp=21(summon monster) exprecover=2 kick=0 |
| 700005000 | Monster Summon Staff (+0) | 255 | 1 | 2500 | 0 | 490096 | 700005000 | 250 | 1 | T8 target=1 radius=10000 warp=21(summon monster) exprecover=1 kick=0 |
| 700006000 | Monster Summon Staff(Event) (+0) | 255 | 1 | 2500 | 0 | 490097 | 700006000 | 250 | 1 | T8 target=1 radius=10000 warp=21(summon monster) exprecover=0 kick=0 |
| 800002000 | Re-Spawn Teleport Scroll (+0) | 255 | 1 | 2000 | 0 | 500022 | 800002000 | 0 | 1 | T8 target=1 radius=10000 warp=1(to resurrection point (town)) exprecover=0 kick=0 |
| 800021000 | Scroll of teleport friend (+0) | 255 | 1 | 50 | 0 | 500038 | 800021000 | 15 | 3 | T8 target=1 radius=10000 warp=25(teleport to target (Descent/Wild Advent)) exprecover=0 kick=0 |
| 379112000 | Abyss Eye (+0) | 97 | 1 | 2500 | 1 | 490211 | 379112000 | 50 | 1 | T9 group=0 state=3 radius=25 hitrate=100 dur=30 dmg=0 vision=100 needitem=0 |
| 800056000 | Leader's Guardian [Karus] (+0) | 255 | 1 | 3000000 | 0 | 502001 | 800056000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800057000 | Leader's Guardian [El Morad] (+0) | 255 | 1 | 3000000 | 0 | 502003 | 800057000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800058000 | Co-Leader's Guardian [Karus] (+0) | 255 | 1 | 1200000 | 0 | 502002 | 800058000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800059000 | Co-Leader's Guardian [El Morad] (+0) | 255 | 1 | 1200000 | 0 | 502004 | 800059000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800064000 | Leader's Guardian [Karus 5] (+0) | 255 | 1 | 15000000 | 0 | 502005 | 800064000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800065000 | Leader's Guardian [El Morad 5] (+0) | 255 | 1 | 15000000 | 0 | 502007 | 800065000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800066000 | Co-Leader's Guardian [Karus 5] (+0) | 255 | 1 | 6000000 | 0 | 502006 | 800066000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800067000 | Co-Leader's Guardian [El Morad 5] (+0) | 255 | 1 | 6000000 | 0 | 502008 | 800067000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800068000 | Leader's Guardian [Karus 5] (+0) | 255 | 1 | 30000000 | 0 | 502009 | 800068000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800069000 | Leader's Guardian [El Morad 5] (+0) | 255 | 1 | 30000000 | 0 | 502011 | 800069000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800070000 | Co-Leader's Guardian [Karus 5] (+0) | 255 | 1 | 12000000 | 0 | 502010 | 800070000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |
| 800071000 | Co-Leader's Guardian [El Morad 5] (+0) | 255 | 1 | 12000000 | 0 | 502012 | 800071000 | 40 | 1 | T9 group=0 state=7 radius=0 hitrate=100 dur=3600 dmg=0 vision=100 needitem=0 |

### Other Type3 items (damage throwables, shells, repair)

| ItemID | Item name | Kind | ReqLv | Buy | Countable | MagicNum | UseItem | ReCast | Moral | Type details |
|---|---|---|---|---|---|---|---|---|---|---|
| 379099000 | Magic Hammer (+0) | 97 | 1 | 1000 | 1 | 490202 | 379099000 | 25 | 1 | T3 durability first=2000 |
| 379101000 | Abyss Holy Water(S) (+0) | 97 | 1 | 300 | 1 | 490204 | 379101000 | 25 | 1 | T3 %HP first=120 |
| 379102000 | Abyss Holy Water(M) (+0) | 97 | 1 | 450 | 1 | 490205 | 379102000 | 25 | 1 | T3 %HP first=135 |
| 379103000 | Abyss Holy Water(L) (+0) | 97 | 1 | 800 | 1 | 490206 | 379103000 | 25 | 1 | T3 %HP first=150 |
| 379105000 | Abyss Fire (+0) | 97 | 1 | 1500 | 1 | 490214 | 379105000 | 50 | 5 | T3 %HP first=10 |
| 379115000 | Practice Cannon Ball (+0) | 97 | 1 | 1000 | 1 | 490111 | 379115000 | 5 | 7 | T3 %HP first=0 radius=5 |
| 389021000 | Spell of flame (+0) | 97 | 1 | 60 | 1 | 490021 | 389021000 | 55 | 7 | T3 HP first=-500 attr=fire |
| 389022000 | Spell of fire ball (+0) | 97 | 1 | 60 | 1 | 490022 | 389022000 | 55 | 7 | T3 HP first=-300 attr=fire |
| 389024000 | Explosive arrow (+0) | 97 | 1 | 60 | 1 | 490024 | 389024000 | 55 | 10 | T3 HP first=-30000 end=-30000 attr=fire radius=25 |
| 389028000 | Prayer of hell fire (+0) | 97 | 1 | 3000 | 1 | 490028 | 389028000 | 55 | 7 | T3 HP first=-900 attr=fire |
| 389032000 | Prayer of anger (+0) | 97 | 1 | 5000 | 1 | 490032 | 389032000 | 55 | 13 | T3 HP first=-800 end=-800 attr=fire radius=20 |
| 389033000 | Styx (+0) | 97 | 1 | 8000 | 1 | 490033 | 389033000 | 55 | 7 | T3 MP first=-3000 |
| 389043000 | Rock (+0) | 97 | 1 | 100 | 1 | 490043 | 389043000 | 20 | 7 | T3 HP first=-10 |
| 389048000 | Suicide Bbomb (+0) | 97 | 1 | 100 | 1 | 490044 | 389048000 | 150 | 27 | T3 HP first=-5000 end=-1000 attr=fire radius=15 |
| 389057000 | Poison arrow of harpy (+0) | 97 | 1 | 3600 | 1 | 490057 | 389057000 | 150 | 7 | T3 HP first=-250 overtime=-1800 dur=60s attr=poison |
| 389058000 | Prayer of rage (+0) | 97 | 1 | 2400 | 1 | 490058 | 389058000 | 150 | 7 | T3 HP first=-480 |
| 389077000 | Snow ball (+0) | 97 | 1 | 10 | 1 | 490077 | 389077000 | 20 | 7 | T3 HP first=-10 |
| 389083000 | Acid potion (+0) | 95 | 1 | 120000 | 1 | 490083 | 389083000 | 20 | 7 | T3 durability first=-2000 |
| 389087000 | durability Shell (+0) | 97 | 1 | 12500 | 1 | 490104 | 389087000 | 30 | 7 | T3 durability first=-1000 radius=5 |
| 389088000 | Super durability Shell (+0) | 97 | 1 | 20000 | 1 | 490105 | 389088000 | 30 | 7 | T3 durability first=-2000 radius=10 |
| 389089000 | Hyper durability Shell (+0) | 97 | 1 | 100000 | 1 | 490106 | 389089000 | 30 | 7 | T3 durability first=-4000 radius=5 |
| 389090000 | HP Shell (+0) | 97 | 1 | 12500 | 1 | 490107 | 389090000 | 30 | 7 | T3 %HP first=50 radius=5 |
| 389091000 | Super HP Shell (+0) | 97 | 1 | 20000 | 1 | 490108 | 389091000 | 30 | 7 | T3 %HP first=20 radius=10 |
| 389092000 | Hyper HP Shell (+0) | 97 | 1 | 100000 | 1 | 490109 | 389092000 | 30 | 7 | T3 %HP first=80 radius=5 |
| 389105000 | Nova Shell (+0) | 97 | 1 | 25000 | 1 | 490110 | 389105000 | 5 | 7 | T3 HP first=-1800 end=-900 attr=fire radius=10 |
| 389111000 | Stage1 Guard Tower (+0) | 97 | 1 | 1000000 | 1 | 490202 | 379099000 | 25 | 1 | T3 durability first=2000 |
| 389112000 | Stage2 Guard Tower (+0) | 97 | 1 | 1000000 | 1 | 490202 | 379099000 | 25 | 1 | T3 durability first=2000 |
| 389113000 | Stage3 Guard Tower (+0) | 97 | 1 | 1000000 | 1 | 490202 | 379099000 | 25 | 1 | T3 durability first=2000 |
| 389114000 | Stage1 Sentry (+0) | 98 | 1 | 1000000 | 1 | 490202 | 379099000 | 25 | 1 | T3 durability first=2000 |
| 389115000 | Stage2 Sentry (+0) | 98 | 1 | 1000000 | 1 | 490202 | 379099000 | 25 | 1 | T3 durability first=2000 |
| 389116000 | Stage3 Sentry (+0) | 98 | 1 | 1000000 | 1 | 490202 | 379099000 | 25 | 1 | T3 durability first=2000 |
| 389185000 | Durability Shell (+0) | 97 | 1 | 4000 | 1 | 490114 | 389185000 | 30 | 7 | T3 durability first=-500 end=10 radius=10 |
| 389186000 | Super durability Shell (+0) | 97 | 1 | 6000 | 1 | 490115 | 389186000 | 30 | 7 | T3 durability first=-1000 end=5 radius=10 |
| 389187000 | HP Shell [Lunar War] (+0) | 97 | 1 | 4000 | 1 | 490116 | 389187000 | 30 | 7 | T3 %HP first=20 end=5 radius=10 |
| 389188000 | Super HP Shell [Lunar War] (+0) | 97 | 1 | 6000 | 1 | 490117 | 389188000 | 30 | 7 | T3 %HP first=10 end=10 radius=10 |
| 389189000 | Nova Shell (+0) | 97 | 1 | 7000 | 1 | 490118 | 389189000 | 5 | 7 | T3 HP first=-1200 end=10 attr=fire radius=10 |
| 800063000 | Stat Scroll (+0) | 255 | 1 | 2000 | 0 | 501011 | 800063000 | 0 | 7 | T3 dt255 first=0 |
| 800140000 | Hammer of Recovery(S) (+0) | 255 | 1 | 2000 | 0 | 490215 | 800140000 | 25 | 1 | T3 durability first=25000 |
| 800150000 | Hammer of Recovery(M) (+0) | 255 | 1 | 2000 | 0 | 490216 | 800150000 | 25 | 1 | T3 durability first=25000 |
| 800160000 | Hammer of Recovery(L) (+0) | 255 | 1 | 2000 | 0 | 490217 | 800160000 | 25 | 1 | T3 durability first=25000 |

## 4. Items required or consumed by warrior / priest / mage skills (MAGIC.UseItem / BeforeAction)

Rules from MagicInstance.cpp: (a) if UseItem<>0 the caster must own it (CanUseItem). (b) If BeforeAction is 1..4 the consumed item is the class stone CLASS_STONE_BASE_ID(379058000)+BeforeAction*1000 (379059000 Warrior, 379060000 Rogue, 379061000 Mage, 379062000 Priest) and UseItem is only required. (c) ConsumeItem() never consumes the spell scrolls 370001000/370002000/370003000, 379063000, 379064000, 379065000, 379066000, 379069000, 379070000 (they are permanent "keys"). (d) Resurrection skills (Type5 type 3) take MAGIC_TYPE5.NeedStone x UseItem from the dead target and give the caster (NeedStone/2)+1 back.

| UseItem | Item name | Buy | Countable | BeforeAction -> consumed | Skills |
|---|---|---|---|---|---|
| 0 | - |  |  | stone 379062000 Stone of Priest (+0) | 112815 Helis, 212815 Helis |
| 370001000 | Spell of Fire Blast (+0) | 75000 | 0 | not consumed (scroll key) | 109535 Fire blast, 110535 Fire blast, 209535 Fire blast, 210535 Fire blast |
| 370002000 | Spell of Glacier Blast (+0) | 75000 | 0 | not consumed (scroll key) | 109635 Ice blast, 110635 Ice blast, 209635 Ice blast, 210635 Ice blast |
| 370003000 | Spell of Thunder Blast (+0) | 75000 | 0 | not consumed (scroll key) | 109735 Thunder blast, 110735 Thunder blast, 209735 Thunder blast, 210735 Thunder blast |
| 379006000 | Stone of life (+0) | 20000 | 1 | UseItem consumed | 111733 Resurrection of love, 111742 Resurrection of grace, 111754 Resurrection of favors, 112733 Resurrection of love, 112742 Resurrection of grace, 112754 Resurrection of favors, 211733 Resurrection of love, 211742 Resurrection of grace, 211754 Resurrection of favors, 212733 Resurrection of love, 212742 Resurrection of grace, 212754 Resurrection of favors |
| 379059000 | Stone of Warrior (+0) | 520 | 1 | UseItem consumed | 106815 Exceed Break, 106820 Shock Stun, 206815 Exceed Break, 206820 Shock Stun |
| 379061000 | Stone of Mage (+0) | 520 | 1 | UseItem consumed | 110573 Fire Armor, 110673 Ice Armor, 110674 Freezing Distance, 110773 Lightning Armor, 110825 Minor Resist, 210573 Fire Armor, 210673 Ice Armor, 210674 Freezing Distance, 210773 Lightning Armor, 210825 Minor Resist |
| 379062000 | Stone of Priest (+0) | 520 | 1 | UseItem consumed | 112676 Counter Curse, 112772 Discountis, 112825 Elysian Web, 212676 Counter Curse, 212772 Discountis, 212825 Elysian Web |
| 379063000 | Scream Scroll (+0) | 75000 | 0 | stone 379059000 Stone of Warrior (+0) | 106802 Scream, 206802 Scream |
| 379065000 | Absolute Power Scroll (+0) | 75000 | 0 | stone 379061000 Stone of Mage (+0) | 110802 Absolute power, 210802 Absolute power |
| 379066000 | Judgment Scroll (+0) | 75000 | 0 | stone 379062000 Stone of Priest (+0) | 112802 Judgment, 212802 Judgment |
| 379069000 | Spell of thorn (+0) | 75000 | 0 | stone 379061000 Stone of Mage (+0) | 109554 Fire Thorn, 109754 Static Thorn, 110554 Fire Thorn, 110754 Static Thorn, 209554 Fire Thorn, 209754 Static Thorn, 210554 Fire Thorn, 210754 Static Thorn |
| 379070000 | Spell of impact (+0) | 75000 | 0 | not consumed (scroll key) | 109557 Fire Impact, 109657 Ice Impact, 109757 Thunder Impact, 110557 Fire Impact, 110657 Ice Impact, 110757 Thunder Impact, 209557 Fire Impact, 209657 Ice Impact, 209757 Thunder Impact, 210557 Fire Impact, 210657 Ice Impact, 210757 Thunder Impact |

## 5. Where these are sold (ITEM.SellingGroup x 1000 = K_NPC.iSellingGroup)

* SellingGroup **253** (potion merchants, K_NPC iSellingGroup 253000: [Potions]Uronia 506, [Potions]Nion 528, [Healing Potion]Karpis 13005, [Healing Potions]Shama 22202 (Karus); [Healing Potions]Ruber 12117, [Healing potion]Raina 12118, [Potion Merchant]Clepio 13003, [Healing potion]Rona 16089, [Portion] Kawani 17005 (El Morad)) sells exactly: Holy water 389010000 (45 HP), Water of life 389011000 (90), Water of love 389012000 (180), Water of grace 389013000 (360), **Water of favors 389014000 (720 HP, 7000 noah)**, Potion of spirit 389016000 (120 MP), intelligence 389017000 (240), sagacity 389018000 (480), wisdom 389019000 (960), **Potion of soul 389020000 (1920 MP, 15000 noah)**. These merchants are spawned (K_NPCPOS) in zones 1, 2, 21, 30, 31, 32, 33, 48, 64 - **none in Ronark Land (71)**.
* SellingGroup **255** (sundries, 255000) - present in Ronark as [sundries]Halber 16062 (El Morad base) and Ardin[sundries] 26062 (Karus base): class stones 379059000 Warrior / 379060000 Rogue / 379061000 Mage / 379062000 Priest, Stone of life 379006000, Prayers of life/love/grace/favors, Transformation Scroll 381001000, Prayer of god's power 389026000, resistance potions 389038000-389040000, Calling Friend Scroll 389050000, Acid potion, Arrow, etc. (no HP/MP potions).
* SellingGroup 254 = [Scrolls]Charon (upgrade/enchant scrolls), 251/250 = siege/Lunar-war quartermasters (HP/MP/durability shells, Defense/Resistance/Attack-speed/Magic-attack/Physical-attack scrolls 389094000-389099000).
* Note: Kind 255 "premium" items (800xxx/810xxx/910xxx scrolls, speed potions) have SellingGroup 0 (item mall / events).
