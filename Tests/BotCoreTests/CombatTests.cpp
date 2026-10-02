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

TEST_CASE("Combat_PartyManageCheck_Order")
{
	BotCore::PartyManageCheck c;
	c.isLeader = false;
	c.targetInParty = false;
	c.hasLast = true;
	c.sinceLastMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_REJECT_LEADER);

	c.isLeader = true;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_REJECT_MEMBER);

	c.targetInParty = true;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_REJECT_GAP);

	c.sinceLastMs = 1000;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_REJECT_RATE);

	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_OK);

	c.hasLast = false;
	c.sinceLastMs = 0;
	c.actionsInWindow = 0;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_OK);

	c.hasLast = false;
	c.sinceLastMs = 0;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_REJECT_RATE);
}

TEST_CASE("Combat_PartyManageCheck_Boundaries")
{
	BotCore::PartyManageCheck c;
	c.isLeader = true;
	c.targetInParty = true;
	c.hasLast = true;
	c.sinceLastMs = 999;
	c.actionsInWindow = 0;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_REJECT_GAP);

	c.sinceLastMs = 1000;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_OK);

	CHECK_EQ((int)BotCore::kPartyManageGapMs, 1000);

	c.sinceLastMs = 1000;
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_OK);

	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_REJECT_RATE);

	c.isLeader = false;
	c.actionsInWindow = 0;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_REJECT_LEADER);

	c.isLeader = true;
	c.targetInParty = false;
	CHECK_EQ((int)BotCore::CheckPartyManage(c), (int)BotCore::PARTYMANAGE_REJECT_MEMBER);
}

TEST_CASE("Combat_ChatText_Validity")
{
	CHECK_EQ((int)BotCore::IsValidChatText("TARGET: BotWP_E (Malice)", sizeof("TARGET: BotWP_E (Malice)") - 1), 1);
	CHECK_EQ((int)BotCore::IsValidChatText("", 0), 0);
	CHECK_EQ((int)BotCore::IsValidChatText(nullptr, 4), 0);

	char text128[128];
	for (int i = 0; i < 128; i++)
		text128[i] = 'a';
	char text129[129];
	for (int i = 0; i < 129; i++)
		text129[i] = 'a';
	CHECK_EQ((int)BotCore::IsValidChatText(text128, 128), 1);
	CHECK_EQ((int)BotCore::IsValidChatText(text129, 129), 0);

	CHECK_EQ((int)BotCore::IsValidChatText("+bot list", 9), 0);
	CHECK_EQ((int)BotCore::IsValidChatText("a+b", 3), 1);
	CHECK_EQ((int)BotCore::IsValidChatText("a\tb", 3), 0);
	CHECK_EQ((int)BotCore::IsValidChatText("a\x7f" "b", 3), 0);
	CHECK_EQ((int)BotCore::IsValidChatText("a\xc4" "b", 3), 0);

	CHECK_EQ((unsigned)BotCore::ChatTextHash("", 0), 0x811C9DC5u);
	CHECK_EQ((unsigned)BotCore::ChatTextHash("a", 1), 0xE40C292Cu);
	CHECK_EQ((unsigned)(BotCore::ChatTextHash("a", 1) != BotCore::ChatTextHash("b", 1)), 1u);
	CHECK_EQ((unsigned)(BotCore::ChatTextHash("same", 4) == BotCore::ChatTextHash("same", 4)), 1u);
}

TEST_CASE("Combat_ChatRateWindow")
{
	BotCore::ChatRateWindow w;
	CHECK_EQ(w.CountInWindow(0), 0);

	w.Record(0);
	w.Record(4000);
	w.Record(8000);
	w.Record(12000);
	w.Record(16000);
	w.Record(20000);

	CHECK_EQ(w.CountInWindow(20000), 6);
	CHECK_EQ(w.CountInWindow(59999), 6);
	CHECK_EQ(w.CountInWindow(60000), 5);
	CHECK_EQ(w.CountInWindow(64000), 4);
	CHECK_EQ(w.CountInWindow(80000), 0);

	w.Record(24000);
	CHECK_EQ(w.CountInWindow(24000), 6);

	w.Clear();
	CHECK_EQ(w.CountInWindow(24000), 0);
}

TEST_CASE("Combat_ChatCheck_Order")
{
	BotCore::ChatCheck c;
	c.textOk = false;
	c.hasLast = true;
	c.sinceLastMs = 0;
	c.sameAsLast = true;
	c.chatsInMinute = 6;
	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_REJECT_TEXT);

	c.textOk = true;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_REJECT_GAP);

	c.sinceLastMs = 4000;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_REJECT_DUP);

	c.sameAsLast = false;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_REJECT_MINUTE);

	c.chatsInMinute = 5;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_REJECT_RATE);

	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_OK);

	c.hasLast = false;
	c.sinceLastMs = 0;
	c.sameAsLast = true;
	c.chatsInMinute = 0;
	c.actionsInWindow = 0;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_OK);
}

TEST_CASE("Combat_ChatCheck_Boundaries")
{
	BotCore::ChatCheck c;
	c.textOk = true;
	c.hasLast = true;
	c.sinceLastMs = 3999;
	c.sameAsLast = false;
	c.chatsInMinute = 0;
	c.actionsInWindow = 0;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_REJECT_GAP);

	c.sinceLastMs = 4000;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_OK);

	c.sameAsLast = true;
	c.sinceLastMs = 7999;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_REJECT_DUP);

	c.sinceLastMs = 8000;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_OK);

	c.sameAsLast = false;
	c.sinceLastMs = 4000;
	c.chatsInMinute = 5;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_OK);

	c.chatsInMinute = 6;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_REJECT_MINUTE);

	c.chatsInMinute = 0;
	c.actionsInWindow = 5;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_OK);

	c.actionsInWindow = 6;
	CHECK_EQ((int)BotCore::CheckChat(c), (int)BotCore::CHAT_REJECT_RATE);

	CHECK_EQ((int)BotCore::kChatMaxLen, 128);
	CHECK_EQ((int)BotCore::kChatGapMs, 4000);
	CHECK_EQ((int)BotCore::kChatDupMs, 8000);
	CHECK_EQ((int)BotCore::kChatPerMinute, 6);
	CHECK_EQ((int)BotCore::kChatMinuteMs, 60000);
}

TEST_CASE("Combat_CastCancel_Guard")
{
	CHECK_EQ((int)BotCore::kCastCancelCode, -100);

	CHECK_EQ((int)BotCore::CheckCastCancel(false, 0), (int)BotCore::CANCEL_REJECT_NOT_CASTING);
	CHECK_EQ((int)BotCore::CheckCastCancel(false, 6), (int)BotCore::CANCEL_REJECT_NOT_CASTING);
	CHECK_EQ((int)BotCore::CheckCastCancel(true, 0), (int)BotCore::CANCEL_OK);
	CHECK_EQ((int)BotCore::CheckCastCancel(true, 5), (int)BotCore::CANCEL_OK);
	CHECK_EQ((int)BotCore::CheckCastCancel(true, 6), (int)BotCore::CANCEL_REJECT_RATE);
}

TEST_CASE("Combat_PlanStanding")
{
	CHECK_EQ((int)BotCore::PlanStanding(true, true), (int)BotCore::STAND_STOP_FIRST);
	CHECK_EQ((int)BotCore::PlanStanding(true, false), (int)BotCore::STAND_PROCEED);
	CHECK_EQ((int)BotCore::PlanStanding(false, true), (int)BotCore::STAND_PROCEED);
	CHECK_EQ((int)BotCore::PlanStanding(false, false), (int)BotCore::STAND_PROCEED);
}

TEST_CASE("Combat_FlyingCast_Rules")
{
	CHECK_EQ((int)BotCore::kFlightMinMs, 1000);

	CHECK_EQ(BotCore::IsFlyingCast(3, 191), true);
	CHECK_EQ(BotCore::IsFlyingCast(3, 0), false);
	CHECK_EQ(BotCore::IsFlyingCast(2, 191), false);
	CHECK_EQ(BotCore::IsFlyingCast(1, 191), false);

	CHECK_EQ((int)BotCore::CastManaNeed(50, true), 100);
	CHECK_EQ((int)BotCore::CastManaNeed(50, false), 50);
	CHECK_EQ((int)BotCore::CastManaNeed(350, true), 700);

	BotCore::CastStartCheck c = OkCast();
	c.distanceM = 5.0f;
	c.skillRange = 78;
	c.distanceField = 50;
	c.weaponRangeField = 0;
	c.needsStanding = false;
	c.standing = true;
	c.msp = BotCore::CastManaNeed(50, true);
	c.reCastMs = 4300;
	c.typeGated = true;
	c.hasSkillLast = false;
	c.hasTypeLast = false;
	c.hasAnyLast = false;
	c.actionsInWindow = 0;

	c.mana = 99;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_NO_MANA);
	c.mana = 100;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);

	CHECK_EQ((int)BotCore::CastManaNeed(40000, true), 80000);

	c.msp = BotCore::CastManaNeed(40000, true);
	c.mana = 79999;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_NO_MANA);
	c.mana = 80000;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);
}

TEST_CASE("Combat_CastFly_Guard")
{
	CHECK_EQ((int)BotCore::CastDurationMs(15), 1580);

	CHECK_EQ((int)BotCore::CheckCastFly(true, 1579, 15, 100, 100, 0), (int)BotCore::CAST_REJECT_TOO_EARLY);
	CHECK_EQ((int)BotCore::CheckCastFly(true, 1580, 15, 100, 100, 0), (int)BotCore::CAST_OK);
	CHECK_EQ((int)BotCore::CheckCastFly(false, 1580, 15, 100, 100, 0), (int)BotCore::CAST_REJECT_OUT_OF_RANGE);
	CHECK_EQ((int)BotCore::CheckCastFly(true, 1580, 15, 99, 100, 0), (int)BotCore::CAST_REJECT_NO_MANA);
	CHECK_EQ((int)BotCore::CheckCastFly(true, 1580, 15, 100, 100, 6), (int)BotCore::CAST_REJECT_RATE);

	CHECK_EQ((int)BotCore::CheckCastFly(false, 0, 15, 0, 100, 6), (int)BotCore::CAST_REJECT_TOO_EARLY);
	CHECK_EQ((int)BotCore::CheckCastFly(false, 1580, 15, 0, 100, 6), (int)BotCore::CAST_REJECT_OUT_OF_RANGE);
	CHECK_EQ((int)BotCore::CheckCastFly(true, 1580, 15, 0, 100, 6), (int)BotCore::CAST_REJECT_NO_MANA);

	CHECK_EQ((int)BotCore::CheckCastFly(true, 0, 0, 100, 100, 0), (int)BotCore::CAST_OK);
}

TEST_CASE("Combat_CastLand_Guard")
{
	CHECK_EQ((int)BotCore::CheckCastLand(true, 999, 50, 50, 0), (int)BotCore::CAST_REJECT_TOO_EARLY);
	CHECK_EQ((int)BotCore::CheckCastLand(true, 1000, 50, 50, 0), (int)BotCore::CAST_OK);
	CHECK_EQ((int)BotCore::CheckCastLand(false, 1000, 50, 50, 0), (int)BotCore::CAST_REJECT_OUT_OF_RANGE);
	CHECK_EQ((int)BotCore::CheckCastLand(true, 1000, 49, 50, 0), (int)BotCore::CAST_REJECT_NO_MANA);
	CHECK_EQ((int)BotCore::CheckCastLand(true, 1000, 50, 50, 6), (int)BotCore::CAST_REJECT_RATE);

	CHECK_EQ((int)BotCore::CheckCastLand(false, 0, 0, 50, 6), (int)BotCore::CAST_REJECT_TOO_EARLY);
	CHECK_EQ((int)BotCore::CheckCastLand(false, 1000, 0, 50, 6), (int)BotCore::CAST_REJECT_OUT_OF_RANGE);
	CHECK_EQ((int)BotCore::CheckCastLand(true, 1000, 0, 50, 6), (int)BotCore::CAST_REJECT_NO_MANA);
}

TEST_CASE("Combat_CastTypes_Supported")
{
	CHECK_EQ(BotCore::CastTypesSupported(1, 0), true);
	CHECK_EQ(BotCore::CastTypesSupported(3, 0), true);
	CHECK_EQ(BotCore::CastTypesSupported(4, 0), true);
	CHECK_EQ(BotCore::CastTypesSupported(3, 4), true);

	CHECK_EQ(BotCore::CastTypesSupported(0, 0), false);
	CHECK_EQ(BotCore::CastTypesSupported(2, 0), false);
	CHECK_EQ(BotCore::CastTypesSupported(5, 0), false);
	CHECK_EQ(BotCore::CastTypesSupported(9, 0), false);
	CHECK_EQ(BotCore::CastTypesSupported(1, 3), false);
	CHECK_EQ(BotCore::CastTypesSupported(1, 4), false);
	CHECK_EQ(BotCore::CastTypesSupported(1, 9), false);
	CHECK_EQ(BotCore::CastTypesSupported(2, 3), false);
	CHECK_EQ(BotCore::CastTypesSupported(2, 4), false);
	CHECK_EQ(BotCore::CastTypesSupported(3, 1), false);
	CHECK_EQ(BotCore::CastTypesSupported(3, 2), false);
	CHECK_EQ(BotCore::CastTypesSupported(3, 3), false);
	CHECK_EQ(BotCore::CastTypesSupported(3, 5), false);
	CHECK_EQ(BotCore::CastTypesSupported(4, 1), false);
	CHECK_EQ(BotCore::CastTypesSupported(4, 2), false);
	CHECK_EQ(BotCore::CastTypesSupported(4, 3), false);
	CHECK_EQ(BotCore::CastTypesSupported(4, 4), false);
	CHECK_EQ(BotCore::CastTypesSupported(4, 9), false);
	CHECK_EQ(BotCore::CastTypesSupported(0, 4), false);
}

TEST_CASE("Combat_TypeGate_MinSince")
{
	CHECK_EQ(BotCore::IsGatedType(0), false);
	CHECK_EQ(BotCore::IsGatedType(1), true);
	CHECK_EQ(BotCore::IsGatedType(7), true);
	CHECK_EQ(BotCore::IsGatedType(8), false);

	uint32_t since = 12345;
	BotCore::TypeStamp a[2] = { { 3, true, 500 }, { 4, true, 200 } };
	CHECK_EQ((int)BotCore::MinGatedSince(a, 2, since), 1);
	CHECK_EQ((int)since, 200);

	BotCore::TypeStamp b[2] = { { 3, true, 500 }, { 4, false, 0 } };
	CHECK_EQ((int)BotCore::MinGatedSince(b, 2, since), 1);
	CHECK_EQ((int)since, 500);

	BotCore::TypeStamp c[2] = { { 3, false, 0 }, { 4, false, 0 } };
	CHECK_EQ((int)BotCore::MinGatedSince(c, 2, since), 0);
	CHECK_EQ((int)since, 0);

	BotCore::TypeStamp d[2] = { { 3, true, 500 }, { 0, true, 10 } };
	CHECK_EQ((int)BotCore::MinGatedSince(d, 2, since), 1);
	CHECK_EQ((int)since, 500);

	BotCore::TypeStamp e[2] = { { 0, true, 10 }, { 4, true, 300 } };
	CHECK_EQ((int)BotCore::MinGatedSince(e, 2, since), 1);
	CHECK_EQ((int)since, 300);

	BotCore::TypeStamp f[1] = { { 3, true, 500 } };
	CHECK_EQ((int)BotCore::MinGatedSince(f, 1, since), 1);
	CHECK_EQ((int)since, 500);
}

TEST_CASE("Combat_CastQuestAllowed")
{
	CHECK_EQ(BotCore::CastQuestAllowed(0, false, false), true);
	CHECK_EQ(BotCore::CastQuestAllowed(511, false, false), false);
	CHECK_EQ(BotCore::CastQuestAllowed(511, false, true), true);
	CHECK_EQ(BotCore::CastQuestAllowed(511, true, false), true);
	CHECK_EQ(BotCore::CastQuestAllowed(0, false, true), true);
}

TEST_CASE("Combat_TypeGate_DualCast")
{
	BotCore::CastStartCheck c = {};
	c.distanceM = 5.0f;
	c.skillRange = 56;
	c.distanceField = 50;
	c.weaponRangeField = 0;
	c.needsStanding = false;
	c.standing = true;
	c.mana = 1000;
	c.msp = 30;
	c.reCastMs = 5300;
	c.typeGated = true;
	c.hasSkillLast = false;
	c.hasAnyLast = false;
	c.actionsInWindow = 0;

	BotCore::TypeStamp st[2] = { { 3, true, 5000 }, { 4, true, 400 } };
	c.hasTypeLast = BotCore::MinGatedSince(st, 2, c.sinceTypeLastMs);
	CHECK_EQ((int)c.hasTypeLast, 1);
	CHECK_EQ((int)c.sinceTypeLastMs, 400);
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_TYPE_GATE);
	CHECK_EQ((int)BotCore::CastWaitMs(c), 600);

	st[1].sinceMs = 1000;
	c.hasTypeLast = BotCore::MinGatedSince(st, 2, c.sinceTypeLastMs);
	CHECK_EQ((int)c.sinceTypeLastMs, 1000);
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);
	CHECK_EQ((int)BotCore::CastWaitMs(c), 0);

	st[0].has = false;
	st[1].type = 4;
	st[1].has = true;
	st[1].sinceMs = 999;
	c.hasTypeLast = BotCore::MinGatedSince(st, 2, c.sinceTypeLastMs);
	CHECK_EQ((int)c.hasTypeLast, 1);
	CHECK_EQ((int)c.sinceTypeLastMs, 999);
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_TYPE_GATE);
	CHECK_EQ((int)BotCore::CastWaitMs(c), 1);
}

TEST_CASE("Combat_TypeGate_Type4Single")
{
	BotCore::CastStartCheck c = {};
	c.distanceM = 5.0f;
	c.skillRange = 56;
	c.distanceField = 50;
	c.weaponRangeField = 0;
	c.needsStanding = false;
	c.standing = true;
	c.mana = 1000;
	c.msp = 10;
	c.reCastMs = 100;
	c.typeGated = true;
	c.hasSkillLast = false;
	c.hasAnyLast = false;
	c.actionsInWindow = 0;

	CHECK_EQ((int)BotCore::IsGatedType(4), 1);

	BotCore::TypeStamp st[2] = { { 4, true, 300 }, { 0, false, 0 } };
	c.hasTypeLast = BotCore::MinGatedSince(st, 2, c.sinceTypeLastMs);
	CHECK_EQ((int)c.hasTypeLast, 1);
	CHECK_EQ((int)c.sinceTypeLastMs, 300);
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_TYPE_GATE);
	CHECK_EQ((int)BotCore::CastWaitMs(c), 700);

	st[0].sinceMs = 1000;
	c.hasTypeLast = BotCore::MinGatedSince(st, 2, c.sinceTypeLastMs);
	CHECK_EQ((int)c.hasTypeLast, 1);
	CHECK_EQ((int)c.sinceTypeLastMs, 1000);
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);
	CHECK_EQ((int)BotCore::CastWaitMs(c), 0);

	st[0] = { 4, false, 0 };
	c.hasTypeLast = BotCore::MinGatedSince(st, 2, c.sinceTypeLastMs);
	CHECK_EQ((int)c.hasTypeLast, 0);
	CHECK_EQ((int)c.sinceTypeLastMs, 0);
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);
	CHECK_EQ((int)BotCore::CastWaitMs(c), 0);
}

TEST_CASE("Combat_CastMoral_Supported")
{
	CHECK_EQ(BotCore::CastMoralSupported(1, 0), true);
	CHECK_EQ(BotCore::CastMoralSupported(2, 0), true);
	CHECK_EQ(BotCore::CastMoralSupported(7, 0), true);
	CHECK_EQ(BotCore::CastMoralSupported(8, 0), true);
	CHECK_EQ(BotCore::CastMoralSupported(10, 0), true);
	CHECK_EQ(BotCore::CastMoralSupported(7, 191), true);

	CHECK_EQ(BotCore::CastMoralSupported(10, 191), false);
	CHECK_EQ(BotCore::CastMoralSupported(0, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(3, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(4, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(5, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(6, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(9, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(11, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(12, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(13, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(14, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(15, 0), false);
	CHECK_EQ(BotCore::CastMoralSupported(25, 0), false);

	CHECK_EQ(BotCore::IsAreaMoral(10), true);
	CHECK_EQ(BotCore::IsAreaMoral(1), false);
	CHECK_EQ(BotCore::IsAreaMoral(7), false);
	CHECK_EQ(BotCore::IsAreaMoral(11), false);
	CHECK_EQ(BotCore::IsAreaMoral(13), false);
}

TEST_CASE("Combat_AreaCast_Fields")
{
	CHECK_EQ((int)BotCore::CastTargetIdField(true, 2990), -1);
	CHECK_EQ((int)BotCore::CastTargetIdField(true, -1), -1);
	CHECK_EQ((int)BotCore::CastTargetIdField(false, 2990), 2990);
	CHECK_EQ((int)BotCore::CastTargetIdField(false, 0), 0);

	CHECK_EQ((int)BotCore::CastCoordField(true, true, 123.7f), 123);
	CHECK_EQ((int)BotCore::CastCoordField(true, false, 123.7f), 123);
	CHECK_EQ((int)BotCore::CastCoordField(false, true, 123.7f), 0);
	CHECK_EQ((int)BotCore::CastCoordField(false, false, 123.7f), 123);
	CHECK_EQ((int)BotCore::CastCoordField(true, true, 0.0f), 0);

	BotCore::CastStartCheck c = {};
	c.distanceM = 5.0f;
	c.skillRange = 56;
	c.distanceField = 50;
	c.weaponRangeField = 0;
	c.needsStanding = false;
	c.standing = true;
	c.mana = 1000;
	c.msp = 200;
	c.reCastMs = 15300;
	c.typeGated = true;
	c.hasTypeLast = false;
	c.hasSkillLast = false;
	c.hasAnyLast = false;
	c.actionsInWindow = 0;

	c.distanceM = 55.9f;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);

	c.distanceM = 56.0f;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_OUT_OF_RANGE);

	c.distanceM = 0.0f;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_OK);

	c.mana = 199;
	CHECK_EQ((int)BotCore::CheckCastStart(c), (int)BotCore::CAST_REJECT_NO_MANA);
}
