# party: BotPHD_K leads, the other three join (Moral 4 / 6 skills need a party)
raw 0    pinvite BotPHD_K BotPHB_K
raw 1000 paccept BotPHB_K
raw 2000 pinvite BotPHD_K BotWP_K
raw 3000 paccept BotWP_K
raw 4000 pinvite BotPHD_K BotWG_K
raw 5000 paccept BotWG_K

# BotPHD_K: heals and cures on BotWP_K (Heal 60); MP pots between the expensive ones
cast BotPHD_K 112527 BotWP_K 3
cast BotPHD_K 112536 BotWP_K 3
cast BotPHD_K 112545 BotWP_K 3
cast BotPHD_K 112525 BotWP_K 3
cast BotPHD_K 112535 BotWP_K 3
cast BotPHD_K 112548 BotWP_K 3
pot  BotPHD_K 389220000 3
cast BotPHD_K 112554 BotWP_K 3
cast BotPHD_K 112557 BotWP_K 3
pot  BotPHD_K 389220000 3
cast BotPHD_K 112560 BotWP_K 2

# BotPHB_K: buffs; one target per sample because an active same-BuffType buff is rejected (MEC-BUF-02)
cast BotPHB_K 112660 BotWP_K 1
cast BotPHB_K 112660 BotPHD_K 1
cast BotPHB_K 112660 BotWG_K 1
cast BotPHB_K 112645 BotWP_K 1
cast BotPHB_K 112645 BotPHD_K 1
cast BotPHB_K 112645 BotWG_K 1
cast BotPHB_K 112654 BotWP_K 1
cast BotPHB_K 112654 BotWG_K 1
cast BotPHB_K 112654 BotPHD_K 1
cast BotPHB_K 112657 self 1
cast BotPHB_K 112656 self 1
cast BotPHB_K 112820 self 1
