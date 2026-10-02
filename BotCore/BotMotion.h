#pragma once

// Pure movement logic for the bot fairness guard (ADR-0016 / ADR-0017).
// No server headers: only the standard library (docs/13 section 2).

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace BotCore
{
	constexpr int16_t  kWalkSpeedField   = 45;    // docs/03 CLI-05: measured walking speed field
	constexpr int16_t  kSprintSpeedField = 67;    // docs/03 CLI-05: sprint field (warrior/mage/priest server limit)
	constexpr uint32_t kMovePeriodMs     = 1500;  // docs/03 CLI-05: client sends WIZ_MOVE about every 1.5 s while moving
	constexpr float    kStopSlackMeters  = 0.05f; // quantisation slack: packet positions are multiples of 0.1 m

	// Speed field unit is 0.1 m/s (docs/03 CLI-05).
	inline float SpeedFieldToMps(int16_t speedField)
	{
		return speedField / 10.0f;
	}

	// Distance a bot may cover in 'periodMs' at 'speedField'.
	inline float MaxStepMeters(int16_t speedField, uint32_t periodMs)
	{
		return SpeedFieldToMps(speedField) * periodMs / 1000.0f;
	}

	// Server-side speed field limit, mirrors CUser::SpeedHackUser() (User.cpp:2896).
	// captainOrRogue -> 90; else warriorMagePriest -> 67; else 45.
	inline int16_t ServerSpeedLimit(bool captainOrRogue, bool warriorMagePriest)
	{
		if (captainOrRogue)
			return 90;
		if (warriorMagePriest)
			return 67;
		return 45;
	}

	struct StepResult { float x; float z; bool arrived; };

	// Moves (x,z) toward (tx,tz) by at most maxStep metres along the straight line.
	// distance <= maxStep (or maxStep <= 0 with distance == 0) -> exactly (tx,tz), arrived = true.
	inline StepResult StepToward(float x, float z, float tx, float tz, float maxStep)
	{
		float dx = tx - x;
		float dz = tz - z;
		float distance = std::sqrt(dx * dx + dz * dz);

		StepResult result;
		if (distance <= maxStep || distance == 0.0f)
		{
			result.x = tx;
			result.z = tz;
			result.arrived = true;
			return result;
		}

		float scale = maxStep / distance;
		result.x = x + dx * scale;
		result.z = z + dz * scale;
		result.arrived = false;
		return result;
	}

	// BotFairnessGuard movement rule (docs/13 section 2, docs/03 CLI-05 / CLI-08 "no teleport").
	enum MoveVerdict
	{
		MOVE_OK = 0,
		MOVE_REJECT_SPEED_FIELD = 1,   // CLI-05: speed field negative or above the server limit
		MOVE_REJECT_STEP_TOO_LONG = 2  // CLI-08: step longer than the bot could have walked
	};

	// packetSpeedField : value that goes into the packet (0 for a stop packet).
	// movingSpeedField : speed the bot is walking at (45 or 67...); used for the step bound.
	// stepMeters       : distance between the current position and the packet position (>= 0).
	// elapsedMs        : time since the previous move packet; values below kMovePeriodMs count as kMovePeriodMs.
	inline MoveVerdict CheckMoveStep(int16_t packetSpeedField, int16_t movingSpeedField,
		int16_t serverLimit, float stepMeters, uint32_t elapsedMs)
	{
		if (packetSpeedField < 0 || packetSpeedField > serverLimit
			|| movingSpeedField < 0 || movingSpeedField > serverLimit)
			return MOVE_REJECT_SPEED_FIELD;

		uint32_t periodMs = elapsedMs < kMovePeriodMs ? kMovePeriodMs : elapsedMs;
		float limit = MaxStepMeters(movingSpeedField, periodMs) * 1.10f + 0.15f;
		if (stepMeters > limit)
			return MOVE_REJECT_STEP_TOO_LONG;

		return MOVE_OK;
	}
}
