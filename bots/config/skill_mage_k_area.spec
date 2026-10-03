# BotMF_K (Karus mage, fire tree 70, ice tree 52, master 20) casts its cheap area skills (Moral 10) at El Morad warriors.
# The aim point of an area skill is the position of the named target bot (a single-target step names a bot, not a point).
# Both targets (BotWP_E, BotWG_E) must be alive, in zone 71, closer than 10 m to BotMF_K and within 6 m of each other so that
# both stand inside the smallest radius (Fire burst r = 8). The operator places them first.
# Same-skill steps are adjacent, so the recast gap is measured between neighbouring casts.

# Flying area, single-typed (Fire burst, r = 8: MP is charged at FLYING and again at EFFECTING)
cast BotMF_K 110533 BotWP_E 2
# Flying area, dual-typed {3, 4} (Ice burst, r = 8)
cast BotMF_K 110633 BotWG_E 1
# Not flying area (Inferno r = 15; Blizzard r = 15, dual-typed {3, 4})
cast BotMF_K 110545 BotWP_E 1
cast BotMF_K 110645 BotWG_E 1
