# BotMF_K (Karus mage, fire tree 70, ice tree 52, master 20) casts its single-target Type3 skills on the El Morad warriors.
# Both targets (BotWP_E, BotWG_E) must be alive, in zone 71 and closer than 10 m to BotMF_K (Burn has MAGIC.Range 11, the
# nearest limit; the others reach 45..78 m). The operator places them first.
# Same-skill steps are adjacent, so the recast gap is measured between neighbouring casts.
# The targets alternate so that neither warrior (about 5650 HP) takes the whole model damage.

# Not flying (single-typed Type3: Burn, Hell fire with a damage over time, Pillar of fire, incineration)
cast BotMF_K 110503 BotWP_E 1
cast BotMF_K 110539 BotWP_E 1
cast BotMF_K 110551 BotWG_E 1
cast BotMF_K 110570 BotWG_E 1
# Not flying, dual-typed {3, 4} (Ice comet: damage and slow)
cast BotMF_K 110651 BotWP_E 1
# Flying single-typed (Fire ball, Fire spear: MP is charged at FLYING and again at EFFECTING)
cast BotMF_K 110515 BotWP_E 2
cast BotMF_K 110527 BotWP_E 2
# Flying dual-typed {3, 4} (Ice arrow)
cast BotMF_K 110615 BotWG_E 1
