# BotPHD_K (Karus priest, curse tree 62, master 20) casts its weapon-bound master attacks on the El Morad warriors.
# Both targets (BotWP_E, BotWG_E) must be alive, in zone 71 and within 0.9 m of BotPHD_K: Type1 skills with
# MAGIC.Range 0 are bound to the weapon range (priest staff 191110000, ITEM.Range 10 = 1.0 m). The operator places them first.
# Same-skill steps are adjacent, so the recast gap is measured between neighbouring casts.

# Judgment (Type1 fail-safe, UseItem Scroll of Priest + Stone of Priest)
cast BotPHD_K 112802 BotWP_E 2
cast BotPHD_K 112802 BotWG_E 1
# Helis (Type1 fail-safe, ignores defense, Stone of Priest)
cast BotPHD_K 112815 BotWG_E 2
cast BotPHD_K 112815 BotWP_E 1
