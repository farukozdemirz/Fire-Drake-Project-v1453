#include "MiniTest.h"

#include <BotCore/BotMotion.h>

#include <cmath>

TEST_CASE("Motion_SpeedUnits")
{
	CHECK(std::fabs(BotCore::SpeedFieldToMps(45) - 4.5f) < 1e-4f);
	CHECK(std::fabs(BotCore::SpeedFieldToMps(67) - 6.7f) < 1e-4f);
	CHECK(std::fabs(BotCore::MaxStepMeters(45, 1500) - 6.75f) < 1e-4f);
}

TEST_CASE("Motion_ServerSpeedLimit")
{
	CHECK_EQ(int(BotCore::ServerSpeedLimit(false, false)), int(45));
	CHECK_EQ(int(BotCore::ServerSpeedLimit(false, true)), int(67));
	CHECK_EQ(int(BotCore::ServerSpeedLimit(true, false)), int(90));
	CHECK_EQ(int(BotCore::ServerSpeedLimit(true, true)), int(90));
}

TEST_CASE("Motion_StepToward")
{
	{
		BotCore::StepResult r = BotCore::StepToward(0.0f, 0.0f, 3.0f, 4.0f, 2.5f);
		CHECK(!r.arrived);
		CHECK(std::fabs(r.x - 1.5f) < 1e-4f);
		CHECK(std::fabs(r.z - 2.0f) < 1e-4f);
	}
	{
		BotCore::StepResult r = BotCore::StepToward(0.0f, 0.0f, 3.0f, 4.0f, 5.0f);
		CHECK(r.arrived);
		CHECK(std::fabs(r.x - 3.0f) < 1e-4f);
		CHECK(std::fabs(r.z - 4.0f) < 1e-4f);
	}
	{
		BotCore::StepResult r = BotCore::StepToward(7.0f, 9.0f, 7.0f, 9.0f, 0.0f);
		CHECK(r.arrived);
		CHECK(std::fabs(r.x - 7.0f) < 1e-4f);
		CHECK(std::fabs(r.z - 9.0f) < 1e-4f);
	}
}

TEST_CASE("Motion_Guard_SpeedField")
{
	CHECK_EQ(int(BotCore::CheckMoveStep(100, 45, 67, 0.0f, 1500)), int(BotCore::MOVE_REJECT_SPEED_FIELD));
	CHECK_EQ(int(BotCore::CheckMoveStep(-1, 45, 67, 0.0f, 1500)), int(BotCore::MOVE_REJECT_SPEED_FIELD));
	CHECK_EQ(int(BotCore::CheckMoveStep(67, 67, 67, 6.0f, 1500)), int(BotCore::MOVE_OK));
	CHECK_EQ(int(BotCore::CheckMoveStep(90, 90, 67, 0.0f, 1500)), int(BotCore::MOVE_REJECT_SPEED_FIELD));
	CHECK_EQ(int(BotCore::CheckMoveStep(90, 90, 90, 0.0f, 1500)), int(BotCore::MOVE_OK));
}

TEST_CASE("Motion_Guard_StepBound")
{
	CHECK_EQ(int(BotCore::CheckMoveStep(45, 45, 67, 6.75f, 1500)), int(BotCore::MOVE_OK));
	CHECK_EQ(int(BotCore::CheckMoveStep(45, 45, 67, 30.0f, 1500)), int(BotCore::MOVE_REJECT_STEP_TOO_LONG));
	CHECK_EQ(int(BotCore::CheckMoveStep(45, 45, 67, 7.5f, 1500)), int(BotCore::MOVE_OK));
	CHECK_EQ(int(BotCore::CheckMoveStep(45, 45, 67, 7.7f, 1500)), int(BotCore::MOVE_REJECT_STEP_TOO_LONG));
	CHECK_EQ(int(BotCore::CheckMoveStep(45, 45, 67, 7.5f, 100)), int(BotCore::MOVE_OK));
	CHECK_EQ(int(BotCore::CheckMoveStep(45, 45, 67, 14.9f, 3000)), int(BotCore::MOVE_OK));
}

TEST_CASE("Motion_Guard_StopPacket")
{
	CHECK_EQ(int(BotCore::CheckMoveStep(0, 45, 67, 4.0f, 1500)), int(BotCore::MOVE_OK));
	CHECK_EQ(int(BotCore::CheckMoveStep(0, 45, 67, 30.0f, 1500)), int(BotCore::MOVE_REJECT_STEP_TOO_LONG));
}
