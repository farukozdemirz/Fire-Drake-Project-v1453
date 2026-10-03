# BotMF_K (Karus mage, fire tree 70, ice tree 52, master 20) casts its two big area skills (Moral 10) at El Morad warriors.
# Supernova and meteor Fall deal 1800 / 2100 plus a damage over time to every victim, so they run in their own script and
# the operator refreshes the warriors' HP first. Both targets (BotWP_E, BotWG_E) must be alive, in zone 71, closer than 10 m
# to BotMF_K and within 6 m of each other (both inside the radius, r = 15). The operator places them first.

# Not flying area, single-typed with a damage over time
cast BotMF_K 110560 BotWP_E 1
cast BotMF_K 110571 BotWG_E 1
