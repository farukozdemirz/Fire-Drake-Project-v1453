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
