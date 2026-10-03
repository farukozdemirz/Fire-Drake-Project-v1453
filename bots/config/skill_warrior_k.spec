# BotWP_K (Karus warrior, attack tree 70, berserk tree 52, master 20) casts its melee and self skills.
# The two targets (BotWP_E, BotWG_E) must be alive, in zone 71 and within 2.0 m of BotWP_K: Type1 skills with
# MAGIC.Range 0 are bound to the weapon range (Raptor, 20 = 2.0 m). The operator places them first.
# Same-skill steps are adjacent, so the recast gap is measured between neighbouring casts.

# Self buffs (Type4 BuffType 6 speed, BuffType 5 attack speed, Type3 HoT)
cast BotWP_K 106001 self 1
cast BotWP_K 106720 self 1
cast BotWP_K 106730 self 1
# Weapon-bound Type1 attacks, two cycles of the cheap skills, one of the expensive ones
cast BotWP_K 106525 BotWP_E 2
cast BotWP_K 106535 BotWG_E 2
cast BotWP_K 106545 BotWP_E 2
cast BotWP_K 106520 BotWP_E 2
cast BotWP_K 106557 BotWG_E 1
cast BotWP_K 106560 BotWP_E 1
cast BotWP_K 106570 BotWG_E 1
# Master skills (Stone of Warrior / Scream Scroll, dual typed)
cast BotWP_K 106802 BotWG_E 1
cast BotWP_K 106815 BotWP_E 1
cast BotWP_K 106820 BotWG_E 1
