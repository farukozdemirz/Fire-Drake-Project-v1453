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
