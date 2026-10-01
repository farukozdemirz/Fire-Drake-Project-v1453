# COEFFICIENT and LEVEL_UP (full tables in coefficient.csv / level_up.csv)

## COEFFICIENT (24 rows: sClass 101-112 and 201-212)

* Loaded by `CCoefficientSet` (shared/database/CoefficientSet.h) into `m_CoefficientArray`, keyed by the **full class code** (`m_sClass`, e.g. 106), so a beginner (101), novice (105) and master (106) warrior use different rows. Karus and El Morad rows are identical (101 == 201, ..., 112 == 212).
* Columns: weapon hit coefficients ShortSword (dagger), Sword, Axe, Club (mace, also Kind 18x), Spear (Spear and Pole share `Spear` in code; `Pole` is unused), Staff, Bow; Hp, Mp, Sp (warriors/rogues use Sp as their MP pool), Ac, Hitrate, Evasionrate.
* Formulas (GameServer/User.cpp, `SetUserAbility`, `SetMaxHp`, `SetMaxMp`):
  * Attack (warrior/priest): `TotalHit = 0.010*WeaponDmg*(STR+40) + coefWeapon*WeaponDmg*Level*STR` (+ base AP, then * (100+APbonus)/100). Mage: `0.005*WeaponDmg*(STR+40) + coefStaff*WeaponDmg*Level`. The weapon coefficient comes from the right-hand weapon's Kind/10.
  * `TotalAc = Ac * (Level + itemAC)` (Ac = 1.0 for all classes), then * ACPercent/100 (buffs such as Malice/Torment/Wall of Iron change ACPercent).
  * `Hitrate = (1 + coefHitrate*Level*DEX) * itemHitrate/100 * buffHitRate/100`; Evasion analogous.
  * `MaxHP = Hp*L^2*STA + 0.1*L*STA + STA/5 + buffHP + itemHP + 20`.
  * `MaxMP = Mp*L^2*INT + 0.2*L*INT + INT/5 + buffMP + itemMP + 20` when Mp<>0; otherwise (warrior/rogue) `Sp*L^2*STA + 0.1*L*STA + STA/5 + ...` (no +20).
* At level 80 (L^2 = 6400) this gives, per stat point (before items/buffs):

| Class (code) | HP per STA | MP per INT (or per STA for warriors) | Best weapon coef (Kind) | Hitrate coef |
|---|---|---|---|---|
| Master warrior (106/206) | 0.003*6400+8+0.2 = 27.4 | Sp: 27.4 per STA | Sword/Axe/Club/Spear 0.00032 | 0.05 |
| Novice warrior (105/205) | 27.4 | 27.4 per STA | 0.00025 | 0.025 |
| Master mage (110/210) | 0.001*6400+8+0.2 = 14.6 | 0.0018*6400+16+0.2 = 27.72 | Staff 0.00015 (ShortSword 0.00015) | 0.015 |
| Novice mage (109/209) | 0.0008 -> 13.32 | 0.0015 -> 25.8 | Staff 0.00015 | 0.015 |
| Master priest (112/212) | 0.0015*6400+8+0.2 = 17.8 | 25.8 | Sword 0.00025, Club 0.00025 | 0.015 |
| Novice priest (111/211) | 0.0012 -> 15.88 | 25.8 | Club 0.0002 | 0.015 |
| Beginner warrior/mage/priest (101/103/104) | 17.8 / 10.76 / 14.6 | 17.8 (Sp) / 25.8 / 25.8 | 0.00013 / 0.0001 staff / 0.00005 club | 0.01 |

  Example: a level-80 master warrior with 150 STA has about 27.4*150+20 = 4130 HP before items/buffs; a master priest with 100 STA has about 1800 HP.
* Stat budget: `LevelChange()` sets total stats = 300 + (L-1)*3 + 2*(L-60) for L>60, i.e. **577 at level 80**; free skill points = (L-9)*2 = **142 at level 80** (max 80 per tree category 5/6/7, master category 8 max L-60 = 20, only for mastered classes).

## LEVEL_UP (79 rows)

* Columns `level` (tinyint), `Exp` (bigint) = experience needed to go from that level to the next (`m_iMaxExp = GetExpByLevel(level)`).
* Loaded into `std::map<int,int64>`: **levels 57 and 58 appear twice** (identical values, second insert ignored) and **levels 24, 74 and 77 are missing**, so `GetExpByLevel()` returns 0 for them (a character at 24/74/77 levels up on the next experience gain). MAX_LEVEL is 80 (Define.h); the row for 80 (1,898,706,631) is the cap.
* Selected values: L50 15,218,948; L60 73,402,110; L70 311,540,999; L75 812,028,367; L79 1,763,786,231; L80 1,898,706,631.
