#include "MiniTest.h"

#include <BotCore/BotCombat.h>

TEST_CASE("Combat_AttackInterval")
{
	CHECK_EQ((int)BotCore::AttackIntervalMs(true, 164), 1640);
	CHECK_EQ((int)BotCore::AttackIntervalMs(true, 80), 1000);
	CHECK_EQ((int)BotCore::AttackIntervalMs(true, 100), 1000);
	CHECK_EQ((int)BotCore::AttackIntervalMs(false, 0), 1000);
	CHECK_EQ((int)BotCore::AttackIntervalMs(true, 300), 3000);
}

TEST_CASE("Combat_DelayAndRangeFields")
{
	CHECK_EQ((int)BotCore::AttackDelayField(true, 164), 174);
	CHECK_EQ((int)BotCore::AttackDelayField(false, 0), 110);
	CHECK_EQ((int)BotCore::AttackRangeField(true, 20), 20);
	CHECK_EQ((int)BotCore::AttackRangeField(true, 35), 35);
	CHECK_EQ((int)BotCore::AttackRangeField(false, 0), 20);
}

TEST_CASE("Combat_DistanceField")
{
	CHECK_EQ((int)BotCore::DistanceField(1.94f), 19);
	CHECK_EQ((int)BotCore::DistanceField(2.06f), 20);
	CHECK_EQ((int)BotCore::DistanceField(0.0f), 0);
	CHECK_EQ((int)BotCore::DistanceField(-3.0f), 0);
	CHECK_EQ((int)BotCore::DistanceField(5000.0f), 32767);
}

TEST_CASE("Combat_Guard_Range")
{
	CHECK_EQ((int)BotCore::CheckAttack(false, 0, 1640, 19, 20, 0), (int)BotCore::ATTACK_OK);
	CHECK_EQ((int)BotCore::CheckAttack(false, 0, 1640, 20, 20, 0), (int)BotCore::ATTACK_OK);
	CHECK_EQ((int)BotCore::CheckAttack(false, 0, 1640, 21, 20, 0), (int)BotCore::ATTACK_REJECT_OUT_OF_RANGE);
	CHECK_EQ((int)BotCore::CheckAttack(false, 0, 1640, -1, 20, 0), (int)BotCore::ATTACK_REJECT_OUT_OF_RANGE);
}

TEST_CASE("Combat_Guard_Interval")
{
	CHECK_EQ((int)BotCore::CheckAttack(true, 1640, 1640, 10, 20, 0), (int)BotCore::ATTACK_OK);
	CHECK_EQ((int)BotCore::CheckAttack(true, 1639, 1640, 10, 20, 0), (int)BotCore::ATTACK_REJECT_TOO_SOON);
	CHECK_EQ((int)BotCore::CheckAttack(false, 0, 1640, 10, 20, 0), (int)BotCore::ATTACK_OK);
	CHECK_EQ((int)BotCore::CheckAttack(true, 900, 500, 10, 20, 0), (int)BotCore::ATTACK_REJECT_TOO_SOON);
	CHECK_EQ((int)BotCore::CheckAttack(true, 1000, 500, 10, 20, 0), (int)BotCore::ATTACK_OK);
}

TEST_CASE("Combat_Guard_Order")
{
	CHECK_EQ((int)BotCore::CheckAttack(true, 0, 1640, 99, 20, 6), (int)BotCore::ATTACK_REJECT_OUT_OF_RANGE);
	CHECK_EQ((int)BotCore::CheckAttack(true, 0, 1640, 10, 20, 6), (int)BotCore::ATTACK_REJECT_TOO_SOON);
	CHECK_EQ((int)BotCore::CheckAttack(true, 5000, 1640, 10, 20, 6), (int)BotCore::ATTACK_REJECT_RATE);
	CHECK_EQ((int)BotCore::CheckAttack(true, 5000, 1640, 10, 20, 5), (int)BotCore::ATTACK_OK);
}

TEST_CASE("Combat_RateWindow")
{
	BotCore::ActionRateWindow w;
	CHECK_EQ(w.CountInWindow(0), 0);

	w.Record(0);
	w.Record(100);
	w.Record(200);
	w.Record(300);
	w.Record(400);
	w.Record(500);

	CHECK_EQ(w.CountInWindow(500), 6);
	CHECK_EQ(w.CountInWindow(999), 6);
	CHECK_EQ(w.CountInWindow(1000), 5);
	CHECK_EQ(w.CountInWindow(1499), 1);
	CHECK_EQ(w.CountInWindow(1500), 0);
	CHECK_EQ(w.CountInWindow(1600), 0);

	w.Record(1000);
	CHECK_EQ(w.CountInWindow(1000), 6);

	w.Clear();
	CHECK_EQ(w.CountInWindow(1000), 0);

	w.Record(5000);
	CHECK_EQ(w.CountInWindow(4000), 0);
	CHECK_EQ(w.CountInWindow(5000), 1);
}

static BotCore::CastStartCheck OkCast()
{
	BotCore::CastStartCheck c;
	c.distanceM = 3.0f;
	c.skillRange = 56;
	c.distanceField = 30;
	c.weaponRangeField = 20;
	c.needsStanding = false;
	c.standing = true;
	c.mana = 1000;
	c.msp = 60;
	c.reCastMs = 100;
	c.hasSkillLast = false;
	c.sinceSkillLastMs = 0;
	c.typeGated = true;
	c.hasTypeLast = false;
	c.sinceTypeLastMs = 0;
	c.hasAnyLast = false;
	c.sinceAnyLastMs = 0;
	c.actionsInWindow = 0;
	return c;
}

TEST_CASE("Combat_CastDuration")
{
	CHECK_EQ((int)BotCore::CastDurationMs(0), 0);
	CHECK_EQ((int)BotCore::CastDurationMs(10), 1080);
	CHECK_EQ((int)BotCore::CastDurationMs(15), 1580);
	CHECK_EQ((int)BotCore::CastRecastMs(5), 500);
	CHECK_EQ((int)BotCore::CastRecastMs(1), 100);
	CHECK_EQ((int)BotCore::CastRecastMs(0), 0);
}

TEST_CASE("Combat_CastInRange")
{
	CHECK_EQ(BotCore::CastInRange(55.9f, 56, 0, 20), true);
	CHECK_EQ(BotCore::CastInRange(56.0f, 56, 0, 20), false);
	CHECK_EQ(BotCore::CastInRange(0.0f, 56, 0, 20), true);
	CHECK_EQ(BotCore::CastInRange(99.0f, 0, 20, 20), true);
	CHECK_EQ(BotCore::CastInRange(0.0f, 0, 21, 20), false);
	CHECK_EQ(BotCore::CastInRange(0.0f, 0, -1, 20), false);
	CHECK_EQ(BotCore::CastInRange(1.0f, 56, 32767, 20), true);
}

TEST_CASE("Combat_CastStart_Order")
{
	CHECK_EQ((int)BotCore::CheckCastStart(OkCast()), (int)BotCore::CAST_OK);

	BotCore::CastStartCheck c = OkCast();
	c.distanceM = 56.0f;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_OUT_OF_RANGE);

	c = OkCast();
	c.needsStanding = true;
	c.standing = false;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_NOT_STANDING);

	c = OkCast();
	c.mana = 59;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_NO_MANA);
	c = OkCast();
	c.mana = 60;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);

	c = OkCast();
	c.hasSkillLast = true;
	c.sinceSkillLastMs = 99;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_RECAST);
	c = OkCast();
	c.hasSkillLast = true;
	c.sinceSkillLastMs = 100;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);

	c = OkCast();
	c.hasTypeLast = true;
	c.sinceTypeLastMs = 999;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_TYPE_GATE);
	c = OkCast();
	c.hasTypeLast = true;
	c.sinceTypeLastMs = 1000;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);
	c = OkCast();
	c.typeGated = false;
	c.hasTypeLast = true;
	c.sinceTypeLastMs = 0;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);

	c = OkCast();
	c.hasAnyLast = true;
	c.sinceAnyLastMs = 139;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_GAP);
	c = OkCast();
	c.hasAnyLast = true;
	c.sinceAnyLastMs = 140;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);

	c = OkCast();
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_RATE);
	c = OkCast();
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);

	// Ordering: the earlier rule wins.
	c = OkCast();
	c.distanceM = 56.0f;
	c.mana = 59;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_OUT_OF_RANGE);

	c = OkCast();
	c.mana = 59;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_NO_MANA);

	c = OkCast();
	c.hasSkillLast = true;
	c.sinceSkillLastMs = 99;
	c.hasTypeLast = true;
	c.sinceTypeLastMs = 999;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_RECAST);

	c = OkCast();
	c.hasTypeLast = true;
	c.sinceTypeLastMs = 999;
	c.hasAnyLast = true;
	c.sinceAnyLastMs = 139;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_TYPE_GATE);

	c = OkCast();
	c.hasAnyLast = true;
	c.sinceAnyLastMs = 139;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_GAP);
}

TEST_CASE("Combat_CastWait")
{
	CHECK_EQ((int)BotCore::CastWaitMs(OkCast()), 0);

	BotCore::CastStartCheck c = OkCast();
	c.hasSkillLast = true;
	c.reCastMs = 500;
	c.sinceSkillLastMs = 200;
	CHECK_EQ((int)BotCore::CastWaitMs(c), 300);

	c = OkCast();
	c.hasTypeLast = true;
	c.sinceTypeLastMs = 400;
	CHECK_EQ((int)BotCore::CastWaitMs(c), 600);

	c = OkCast();
	c.typeGated = false;
	c.hasTypeLast = true;
	c.sinceTypeLastMs = 400;
	CHECK_EQ((int)BotCore::CastWaitMs(c), 0);

	c = OkCast();
	c.hasAnyLast = true;
	c.sinceAnyLastMs = 100;
	CHECK_EQ((int)BotCore::CastWaitMs(c), 40);

	c = OkCast();
	c.hasSkillLast = true;
	c.reCastMs = 500;
	c.sinceSkillLastMs = 200;
	c.hasTypeLast = true;
	c.sinceTypeLastMs = 400;
	c.hasAnyLast = true;
	c.sinceAnyLastMs = 100;
	CHECK_EQ((int)BotCore::CastWaitMs(c), 600);

	c = OkCast();
	c.hasSkillLast = true;
	c.reCastMs = 500;
	c.sinceSkillLastMs = 500;
	CHECK_EQ((int)BotCore::CastWaitMs(c), 0);
}

TEST_CASE("Combat_CastEffect")
{
	CHECK_EQ((int)BotCore::CheckCastEffect(true, 1079, 10, 0), (int)BotCore::CAST_REJECT_TOO_EARLY);
	CHECK_EQ((int)BotCore::CheckCastEffect(true, 1080, 10, 0), (int)BotCore::CAST_OK);
	CHECK_EQ((int)BotCore::CheckCastEffect(false, 1080, 10, 0), (int)BotCore::CAST_REJECT_OUT_OF_RANGE);
	CHECK_EQ((int)BotCore::CheckCastEffect(true, 1080, 10, 6), (int)BotCore::CAST_REJECT_RATE);
	CHECK_EQ((int)BotCore::CheckCastEffect(true, 0, 0, 0), (int)BotCore::CAST_OK);
	CHECK_EQ((int)BotCore::CheckCastEffect(false, 0, 10, 6), (int)BotCore::CAST_REJECT_TOO_EARLY);
	CHECK_EQ((int)BotCore::CheckCastEffect(false, 1080, 10, 6), (int)BotCore::CAST_REJECT_OUT_OF_RANGE);
}

static BotCore::PotionCheck OkPot()
{
	BotCore::PotionCheck c;
	c.stock = 1;
	c.hasLast = false;
	c.sinceLastMs = 0;
	c.actionsInWindow = 0;
	return c;
}

TEST_CASE("Combat_PotCheck_Order")
{
	BotCore::PotionCheck c = OkPot();
	c.stock = 0;
	c.hasLast = true;
	c.sinceLastMs = 2499;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPotion(c), (int)BotCore::POT_REJECT_NO_STOCK);

	c = OkPot();
	c.hasLast = true;
	c.sinceLastMs = 2499;
	CHECK_EQ((int)BotCore::CheckPotion(c), (int)BotCore::POT_REJECT_COOLDOWN);

	c = OkPot();
	c.hasLast = true;
	c.sinceLastMs = 2500;
	CHECK_EQ((int)BotCore::CheckPotion(c), (int)BotCore::POT_OK);

	c = OkPot();
	c.hasLast = false;
	CHECK_EQ((int)BotCore::CheckPotion(c), (int)BotCore::POT_OK);

	c = OkPot();
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPotion(c), (int)BotCore::POT_REJECT_RATE);

	c = OkPot();
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckPotion(c), (int)BotCore::POT_OK);
}

TEST_CASE("Combat_PotWait")
{
	BotCore::PotionCheck c = OkPot();
	c.hasLast = false;
	CHECK_EQ((int)BotCore::PotionWaitMs(c), 0);

	c = OkPot();
	c.hasLast = true;
	c.sinceLastMs = 0;
	CHECK_EQ((int)BotCore::PotionWaitMs(c), 2500);

	c.sinceLastMs = 1000;
	CHECK_EQ((int)BotCore::PotionWaitMs(c), 1500);

	c.sinceLastMs = 2500;
	CHECK_EQ((int)BotCore::PotionWaitMs(c), 0);

	c.sinceLastMs = 9000;
	CHECK_EQ((int)BotCore::PotionWaitMs(c), 0);

	CHECK_EQ(BotCore::PotSupported(20), true);
	CHECK_EQ(BotCore::PotSupported(25), true);
	CHECK_EQ(BotCore::PotSupported(26), false);
	CHECK_EQ(BotCore::PotSupported(150), false);
	CHECK_EQ(BotCore::PotSupported(0), true);
}

static BotCore::StanceCheck OkStance()
{
	BotCore::StanceCheck c;
	c.toSit = true;
	c.busy = false;
	c.hasLast = false;
	c.sinceLastMs = 0;
	c.actionsInWindow = 0;
	return c;
}

TEST_CASE("Combat_StanceCheck_Order")
{
	BotCore::StanceCheck c = OkStance();
	c.busy = true;
	c.hasLast = true;
	c.sinceLastMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckStance(c), (int)BotCore::STANCE_REJECT_BUSY);

	c = OkStance();
	c.hasLast = true;
	c.sinceLastMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckStance(c), (int)BotCore::STANCE_REJECT_TOGGLE);

	c = OkStance();
	c.hasLast = true;
	c.sinceLastMs = 999;
	CHECK_EQ((int)BotCore::CheckStance(c), (int)BotCore::STANCE_REJECT_TOGGLE);

	c = OkStance();
	c.hasLast = true;
	c.sinceLastMs = 1000;
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckStance(c), (int)BotCore::STANCE_OK);

	c = OkStance();
	c.hasLast = false;
	CHECK_EQ((int)BotCore::CheckStance(c), (int)BotCore::STANCE_OK);

	c = OkStance();
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckStance(c), (int)BotCore::STANCE_REJECT_RATE);

	c = OkStance();
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckStance(c), (int)BotCore::STANCE_OK);
}

TEST_CASE("Combat_StanceCheck_StandIgnoresBusy")
{
	BotCore::StanceCheck c = OkStance();
	c.toSit = false;
	c.busy = true;
	CHECK_EQ((int)BotCore::CheckStance(c), (int)BotCore::STANCE_OK);

	c = OkStance();
	c.toSit = false;
	c.hasLast = true;
	c.sinceLastMs = 500;
	CHECK_EQ((int)BotCore::CheckStance(c), (int)BotCore::STANCE_REJECT_TOGGLE);

	c = OkStance();
	c.toSit = false;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckStance(c), (int)BotCore::STANCE_REJECT_RATE);

	CHECK_EQ((int)BotCore::kStanceToggleMinMs, 1000);
}

TEST_CASE("Combat_RegionIndex_RegionDelta")
{
	CHECK_EQ(BotCore::RegionIndex(0.0f), 0);
	CHECK_EQ(BotCore::RegionIndex(47.9f), 0);
	CHECK_EQ(BotCore::RegionIndex(48.0f), 1);
	CHECK_EQ(BotCore::RegionIndex(95.9f), 1);
	CHECK_EQ(BotCore::RegionIndex(96.0f), 2);
	CHECK_EQ(BotCore::RegionIndex(-5.0f), 0);
	CHECK_EQ(BotCore::RegionIndex(70000.0f), 65535 / 48);

	CHECK_EQ(BotCore::RegionDelta(10.0f, 10.0f, 10.0f, 10.0f), 0);
	CHECK_EQ(BotCore::RegionDelta(47.9f, 0.0f, 48.0f, 0.0f), 1);
	CHECK_EQ(BotCore::RegionDelta(0.0f, 0.0f, 48.0f, 48.0f), 1);
	CHECK_EQ(BotCore::RegionDelta(0.0f, 0.0f, 96.0f, 0.0f), 2);
	CHECK_EQ(BotCore::RegionDelta(0.0f, 0.0f, 48.0f, 96.0f), 2);
	CHECK_EQ(BotCore::RegionDelta(100.0f, 100.0f, 100.0f, 148.0f), 1);

	CHECK_EQ(BotCore::RegionDelta(10.0f, 20.0f, 130.0f, 80.0f),
		BotCore::RegionDelta(130.0f, 80.0f, 10.0f, 20.0f));
}

static BotCore::TargetHpCheck OkTargetHp()
{
	BotCore::TargetHpCheck c;
	c.regionDelta = 0;
	c.sameTarget = true;
	c.hasLast = true;
	c.sinceLastMs = 5000;
	c.actionsInWindow = 0;
	return c;
}

TEST_CASE("Combat_TargetHpCheck_Order")
{
	BotCore::TargetHpCheck c = OkTargetHp();
	c.regionDelta = 2;
	c.sinceLastMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckTargetHp(c), (int)BotCore::TARGETHP_REJECT_VIEW);

	c = OkTargetHp();
	c.regionDelta = 1;
	c.sinceLastMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckTargetHp(c), (int)BotCore::TARGETHP_REJECT_POLL);

	c = OkTargetHp();
	c.sinceLastMs = 1999;
	CHECK_EQ((int)BotCore::CheckTargetHp(c), (int)BotCore::TARGETHP_REJECT_POLL);

	c = OkTargetHp();
	c.sinceLastMs = 2000;
	CHECK_EQ((int)BotCore::CheckTargetHp(c), (int)BotCore::TARGETHP_OK);

	c = OkTargetHp();
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckTargetHp(c), (int)BotCore::TARGETHP_REJECT_RATE);

	c = OkTargetHp();
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckTargetHp(c), (int)BotCore::TARGETHP_OK);
}

TEST_CASE("Combat_TargetHpCheck_SwitchAndFirst")
{
	BotCore::TargetHpCheck c = OkTargetHp();
	c.sameTarget = false;
	c.hasLast = true;
	c.sinceLastMs = 0;
	CHECK_EQ((int)BotCore::CheckTargetHp(c), (int)BotCore::TARGETHP_OK);

	c = OkTargetHp();
	c.sameTarget = true;
	c.hasLast = false;
	c.sinceLastMs = 0;
	CHECK_EQ((int)BotCore::CheckTargetHp(c), (int)BotCore::TARGETHP_OK);

	c = OkTargetHp();
	c.sameTarget = false;
	c.regionDelta = 2;
	CHECK_EQ((int)BotCore::CheckTargetHp(c), (int)BotCore::TARGETHP_REJECT_VIEW);

	CHECK_EQ((int)BotCore::kTargetHpPollMs, 2000);
	CHECK_EQ((int)BotCore::kViewDistance, 48);
	CHECK_EQ((int)BotCore::kViewRegionRadius, 1);
}

static BotCore::RegeneCheck OkRegene()
{
	BotCore::RegeneCheck c;
	c.sinceDeadMs = 5000;
	c.actionsInWindow = 0;
	return c;
}

TEST_CASE("Combat_RegeneCheck_Order")
{
	BotCore::RegeneCheck c = OkRegene();
	c.sinceDeadMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckRegene(c), (int)BotCore::REGENE_REJECT_WAIT);

	c = OkRegene();
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckRegene(c), (int)BotCore::REGENE_REJECT_RATE);

	c = OkRegene();
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckRegene(c), (int)BotCore::REGENE_OK);

	c = OkRegene();
	CHECK_EQ((int)BotCore::CheckRegene(c), (int)BotCore::REGENE_OK);
}

TEST_CASE("Combat_RegeneCheck_Boundaries")
{
	BotCore::RegeneCheck c = OkRegene();
	c.sinceDeadMs = 2999;
	CHECK_EQ((int)BotCore::CheckRegene(c), (int)BotCore::REGENE_REJECT_WAIT);

	c = OkRegene();
	c.sinceDeadMs = 3000;
	CHECK_EQ((int)BotCore::CheckRegene(c), (int)BotCore::REGENE_OK);

	c = OkRegene();
	c.sinceDeadMs = 0;
	CHECK_EQ((int)BotCore::CheckRegene(c), (int)BotCore::REGENE_REJECT_WAIT);

	CHECK_EQ((int)BotCore::kRegeneMinDeadMs, 3000);
}

static BotCore::PartyInviteCheck OkInvite()
{
	BotCore::PartyInviteCheck c;
	c.inParty = false;
	c.isLeader = false;
	c.regionDelta = 0;
	c.hasLast = false;
	c.sinceLastMs = 0;
	c.actionsInWindow = 0;
	return c;
}

TEST_CASE("Combat_PartyInviteCheck_Order")
{
	BotCore::PartyInviteCheck c = OkInvite();
	c.inParty = true;
	c.isLeader = false;
	c.regionDelta = 2;
	c.hasLast = true;
	c.sinceLastMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_REJECT_LEADER);

	c = OkInvite();
	c.inParty = true;
	c.isLeader = true;
	c.regionDelta = 2;
	c.hasLast = true;
	c.sinceLastMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_REJECT_VIEW);

	c = OkInvite();
	c.hasLast = true;
	c.sinceLastMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_REJECT_GAP);

	c = OkInvite();
	c.hasLast = true;
	c.sinceLastMs = 1000;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_REJECT_RATE);

	c = OkInvite();
	c.hasLast = true;
	c.sinceLastMs = 1000;
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_OK);

	c = OkInvite();
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_OK);

	c = OkInvite();
	c.inParty = true;
	c.isLeader = true;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_OK);
}

TEST_CASE("Combat_PartyInviteCheck_Boundaries")
{
	BotCore::PartyInviteCheck c = OkInvite();
	c.hasLast = true;
	c.sinceLastMs = 999;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_REJECT_GAP);

	c = OkInvite();
	c.hasLast = true;
	c.sinceLastMs = 1000;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_OK);

	c = OkInvite();
	c.hasLast = false;
	c.sinceLastMs = 0;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_OK);

	c = OkInvite();
	c.regionDelta = 1;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_OK);

	c = OkInvite();
	c.regionDelta = 2;
	CHECK_EQ((int)BotCore::CheckPartyInvite(c), (int)BotCore::PARTYINVITE_REJECT_VIEW);

	CHECK_EQ((int)BotCore::kPartyInviteGapMs, 1000);
}

TEST_CASE("Combat_PartyAcceptCheck")
{
	BotCore::PartyAcceptCheck c;
	c.sinceInviteMs = 999;
	c.actionsInWindow = 0;
	CHECK_EQ((int)BotCore::CheckPartyAccept(c), (int)BotCore::PARTYACCEPT_REJECT_WAIT);

	c.sinceInviteMs = 1000;
	CHECK_EQ((int)BotCore::CheckPartyAccept(c), (int)BotCore::PARTYACCEPT_OK);

	c.sinceInviteMs = 0;
	CHECK_EQ((int)BotCore::CheckPartyAccept(c), (int)BotCore::PARTYACCEPT_REJECT_WAIT);

	c.sinceInviteMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyAccept(c), (int)BotCore::PARTYACCEPT_REJECT_WAIT);

	c.sinceInviteMs = 1000;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyAccept(c), (int)BotCore::PARTYACCEPT_REJECT_RATE);

	c.sinceInviteMs = 1000;
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckPartyAccept(c), (int)BotCore::PARTYACCEPT_OK);

	CHECK_EQ((int)BotCore::kPartyAcceptMinMs, 1000);
}

TEST_CASE("Combat_PartyDeclineCheck")
{
	BotCore::PartyDeclineCheck c;
	c.sinceInviteMs = 999;
	c.actionsInWindow = 0;
	CHECK_EQ((int)BotCore::CheckPartyDecline(c), (int)BotCore::PARTYDECLINE_REJECT_WAIT);

	c.sinceInviteMs = 1000;
	CHECK_EQ((int)BotCore::CheckPartyDecline(c), (int)BotCore::PARTYDECLINE_OK);

	c.sinceInviteMs = 0;
	CHECK_EQ((int)BotCore::CheckPartyDecline(c), (int)BotCore::PARTYDECLINE_REJECT_WAIT);

	c.sinceInviteMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyDecline(c), (int)BotCore::PARTYDECLINE_REJECT_WAIT);

	c.sinceInviteMs = 1000;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyDecline(c), (int)BotCore::PARTYDECLINE_REJECT_RATE);

	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckPartyDecline(c), (int)BotCore::PARTYDECLINE_OK);

	CHECK_EQ((int)BotCore::kPartyDeclineMinMs, 1000);
}

TEST_CASE("Combat_PartyLeaveCheck_Order")
{
	BotCore::PartyLeaveCheck c;
	c.hasEntered = true;
	c.sinceEnteredMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyLeave(c), (int)BotCore::PARTYLEAVE_REJECT_WAIT);

	c.sinceEnteredMs = 1000;
	CHECK_EQ((int)BotCore::CheckPartyLeave(c), (int)BotCore::PARTYLEAVE_REJECT_RATE);

	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckPartyLeave(c), (int)BotCore::PARTYLEAVE_OK);

	c.hasEntered = false;
	c.sinceEnteredMs = 0;
	c.actionsInWindow = 0;
	CHECK_EQ((int)BotCore::CheckPartyLeave(c), (int)BotCore::PARTYLEAVE_OK);

	c.hasEntered = false;
	c.sinceEnteredMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyLeave(c), (int)BotCore::PARTYLEAVE_REJECT_RATE);
}

TEST_CASE("Combat_PartyLeaveCheck_Boundaries")
{
	BotCore::PartyLeaveCheck c;
	c.hasEntered = true;
	c.sinceEnteredMs = 999;
	c.actionsInWindow = 0;
	CHECK_EQ((int)BotCore::CheckPartyLeave(c), (int)BotCore::PARTYLEAVE_REJECT_WAIT);

	c.sinceEnteredMs = 1000;
	CHECK_EQ((int)BotCore::CheckPartyLeave(c), (int)BotCore::PARTYLEAVE_OK);

	CHECK_EQ((int)BotCore::kPartyLeaveMinMs, 1000);

	c.sinceEnteredMs = 1000;
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckPartyLeave(c), (int)BotCore::PARTYLEAVE_OK);

	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyLeave(c), (int)BotCore::PARTYLEAVE_REJECT_RATE);
}
