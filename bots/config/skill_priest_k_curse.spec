# BotPHD_K (Karus priest, curse tree 62) casts its enemy-targeted curses on the El Morad bots.
# The three targets must be alive, in zone 71 and within 56 m of BotPHD_K (MAGIC.Range 56); the
# operator gathers them first. A debuff of the same BuffType replaces the earlier one on a target and
# still succeeds (MEC-MAG-15), so the targets are shared by all six skills.
# Same-skill steps are adjacent, so the recast gap is measured between neighbouring casts.

# Malice (AC, BuffType 2)
cast BotPHD_K 112703 BotWP_E 1
cast BotPHD_K 112703 BotWG_E 1
cast BotPHD_K 112703 BotMF_E 1
# Torment (area r=10, aimed at the target bot's position; BuffType 2)
cast BotPHD_K 112757 BotWP_E 1
cast BotPHD_K 112757 BotWG_E 1
cast BotPHD_K 112757 BotMF_E 1
# Parasite (max HP, BuffType 1)
cast BotPHD_K 112745 BotWP_E 1
cast BotPHD_K 112745 BotWG_E 1
cast BotPHD_K 112745 BotMF_E 1
# Massive (attack, BuffType 4)
cast BotPHD_K 112760 BotWP_E 1
cast BotPHD_K 112760 BotWG_E 1
cast BotPHD_K 112760 BotMF_E 1
# Slow (speed, BuffType 5; players may resist, MEC-BUF-05)
cast BotPHD_K 112724 BotWP_E 1
cast BotPHD_K 112724 BotWG_E 1
cast BotPHD_K 112724 BotMF_E 1
# Sweep mana (Type3, drains 960 MP)
cast BotPHD_K 112736 BotWP_E 1
cast BotPHD_K 112736 BotWG_E 1
cast BotPHD_K 112736 BotMF_E 1
