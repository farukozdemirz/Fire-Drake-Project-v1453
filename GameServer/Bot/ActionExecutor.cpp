#include "stdafx.h"
#include "ActionExecutor.h"
#include "BotSession.h"
#include "Telemetry.h"
#include "../Map.h"
#include "../../BotCore/BotMotion.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

// --- file-local helpers ---

// Mirrors the guard's step bound so FAIRNESS_REJECT can report the limit in metres.
static float MoveStepLimit(int16 movingSpeedField, uint32 elapsedMs)
{
	uint32 periodMs = elapsedMs < BotCore::kMovePeriodMs ? BotCore::kMovePeriodMs : elapsedMs;
	return BotCore::MaxStepMeters(movingSpeedField, periodMs) * 1.10f + 0.15f;
}

// Server-side speed field limit the bot's class is subject to; same classification as
// CUser::SpeedHackUser() (GameServer/User.cpp:2903-2906).
static int16 ServerLimitFor(CUser * user)
{
	return BotCore::ServerSpeedLimit(
		user->GetFame() == COMMAND_CAPTAIN || user->isRogue(),
		user->isWarrior() || user->isMage() || user->isPriest());
}

// Per-session action counter, used as decision_id. Only advanced when the event is emitted.
static uint32 NextDecisionId(BotSession * s)
{
	if (!Telemetry::Instance().IsEnabled(TEL_DECISIONS))
		return 0;

	return ++s->m_actionSeq;
}

static std::string FormatFixed(double value, int precision)
{
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(precision) << value;
	return oss.str();
}

// 'rule' / 'reason' / 'value' / 'limit': CLI-05 checks the speed field, CLI-08 the step in metres.
static void EmitFairnessReject(BotSession * s, CUser * user, uint32 decisionId,
	const char * rule, const char * reason, float value, float limit)
{
	if (!Telemetry::Instance().IsEnabled(TEL_DECISIONS))
		return;

	std::string fields = "\"decision_id\":" + std::to_string(decisionId)
		+ ",\"type\":\"Move\",\"rule\":\"" + rule
		+ "\",\"reason\":\"" + reason
		+ "\",\"value\":" + FormatFixed(value, 2)
		+ ",\"limit\":" + FormatFixed(limit, 2);

	Telemetry::Instance().Emit(TEL_DECISIONS, "FAIRNESS_REJECT", user->GetSocketID(),
		s->m_charName.c_str(), fields, false);
}

// Builds one WIZ_MOVE packet at (nx,nz), runs it past the fairness guard and CUser::HandlePacket(),
// then checks that the handler really moved the bot. 'arrived' marks the final stop packet.
static MoveOutcome SubmitMove(BotSession * s, CUser * user, float nx, float nz,
	int16 speed, uint8 echo, bool arrived, uint32 elapsedMs,
	std::chrono::steady_clock::time_point now)
{
	MoveOutcome out;
	out.kind = MoveOutcome::NOTHING;
	out.reason = "ok";

	uint16 will_x = uint16(nx * 10.0f + 0.5f);
	uint16 will_z = uint16(nz * 10.0f + 0.5f);
	uint16 will_y = user->GetSPosY();

	float packetX = will_x / 10.0f;
	float packetZ = will_z / 10.0f;
	float dx = packetX - user->GetX();
	float dz = packetZ - user->GetZ();
	float stepMeters = std::sqrt(dx * dx + dz * dz);

	int16 serverLimit = ServerLimitFor(user);
	BotCore::MoveVerdict verdict = BotCore::CheckMoveStep(speed, s->m_moveSpeed,
		serverLimit, stepMeters, elapsedMs);
	if (verdict != BotCore::MOVE_OK)
	{
		bool speedField = verdict == BotCore::MOVE_REJECT_SPEED_FIELD;
		const char * reason = speedField ? "speed_field" : "step_too_long";
		uint32 decisionId = NextDecisionId(s);
		EmitFairnessReject(s, user, decisionId, speedField ? "CLI-05" : "CLI-08", reason,
			speedField ? (float)speed : stepMeters,
			speedField ? (float)serverLimit : MoveStepLimit(s->m_moveSpeed, elapsedMs));

		s->m_moveActive = false;
		out.kind = MoveOutcome::REFUSED;
		out.reason = reason;
		return out;
	}

	uint32 decisionId = NextDecisionId(s);
	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"Move\",\"x\":" + FormatFixed(packetX, 1)
			+ ",\"z\":" + FormatFixed(packetZ, 1)
			+ ",\"speed\":" + std::to_string((int)speed)
			+ ",\"echo\":" + std::to_string((unsigned)echo);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_SUBMIT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	Packet pkt(WIZ_MOVE);
	pkt << uint16(will_x) << uint16(will_z) << uint16(will_y) << int16(speed) << uint8(echo);

	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	user->HandlePacket(pkt);
	std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	long long latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

	bool ok = user->isInGame()
		&& std::fabs(user->GetX() - packetX) < BotCore::kStopSlackMeters
		&& std::fabs(user->GetZ() - packetZ) < BotCore::kStopSlackMeters;

	if (!ok)
	{
		s->m_moveActive = false;
		out.kind = MoveOutcome::FAILED;
		out.reason = "handler_noop";
	}
	else
	{
		s->m_moveLastSent = now;
		s->m_movePackets++;
		if (arrived)
			s->m_moveActive = false;
		out.kind = arrived ? MoveOutcome::ARRIVED : MoveOutcome::SENT;
		out.reason = "ok";
	}

	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"Move\",\"ok\":" + (ok ? "true" : "false")
			+ ",\"reason\":\"" + out.reason + "\""
			+ ",\"latency_us\":" + std::to_string(latencyUs);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_RESULT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	return out;
}

// --- ActionExecutor ---

MoveOutcome ActionExecutor::BeginMove(BotSession * s, float tx, float tz, int16 speedField,
	std::chrono::steady_clock::time_point now)
{
	MoveOutcome out;
	out.kind = MoveOutcome::NOTHING;
	out.reason = "ok";

	CUser * user = s != nullptr ? s->m_pUser : nullptr;
	if (user == nullptr || !user->isInGame())
	{
		out.kind = MoveOutcome::REFUSED;
		out.reason = "not_in_game";
		return out;
	}

	if (user->isDead())
	{
		out.kind = MoveOutcome::REFUSED;
		out.reason = "dead";
		return out;
	}

	int16 serverLimit = ServerLimitFor(user);
	BotCore::MoveVerdict speedVerdict = BotCore::CheckMoveStep(speedField, speedField,
		serverLimit, 0.0f, BotCore::kMovePeriodMs);
	if (speedField < 1 || speedVerdict != BotCore::MOVE_OK)
	{
		uint32 decisionId = NextDecisionId(s);
		EmitFairnessReject(s, user, decisionId, "CLI-05", "speed_field",
			(float)speedField, (float)serverLimit);
		out.kind = MoveOutcome::REFUSED;
		out.reason = "speed_field";
		return out;
	}

	if (!(tx >= 0.0f && tz >= 0.0f && tx * 10.0f <= 65535.0f && tz * 10.0f <= 65535.0f)
		|| user->GetMap() == nullptr
		|| !user->GetMap()->IsValidPosition(tx, tz, user->GetY()))
	{
		out.kind = MoveOutcome::REFUSED;
		out.reason = "bad_target";
		return out;
	}

	s->m_moveActive = true;
	s->m_moveTargetX = tx;
	s->m_moveTargetZ = tz;
	s->m_moveSpeed = speedField;
	s->m_movePackets = 0;
	s->m_moveLastSent = now - std::chrono::milliseconds(BotCore::kMovePeriodMs);

	out.kind = MoveOutcome::SENT;
	out.reason = "ok";
	return out;
}

MoveOutcome ActionExecutor::TickMove(BotSession * s, std::chrono::steady_clock::time_point now)
{
	MoveOutcome out;
	out.kind = MoveOutcome::NOTHING;
	out.reason = "ok";

	if (s == nullptr || !s->m_moveActive)
		return out;

	CUser * user = s->m_pUser;
	if (user == nullptr || !user->isInGame())
	{
		s->m_moveActive = false;
		return out;
	}

	long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		now - s->m_moveLastSent).count();
	if (elapsed < (long long)BotCore::kMovePeriodMs)
		return out;

	// A stalled walk must not turn into one huge step.
	uint32 elapsedMs = (uint32)elapsed;
	if (elapsedMs > 2 * BotCore::kMovePeriodMs)
		elapsedMs = 2 * BotCore::kMovePeriodMs;

	float maxStep = BotCore::MaxStepMeters(s->m_moveSpeed, elapsedMs);
	BotCore::StepResult step = BotCore::StepToward(user->GetX(), user->GetZ(),
		s->m_moveTargetX, s->m_moveTargetZ, maxStep);

	int16 speed = step.arrived ? 0 : s->m_moveSpeed;
	uint8 echo = step.arrived ? 0 : 3;

	return SubmitMove(s, user, step.x, step.z, speed, echo, step.arrived, elapsedMs, now);
}

MoveOutcome ActionExecutor::StopMove(BotSession * s, std::chrono::steady_clock::time_point now)
{
	MoveOutcome out;
	out.kind = MoveOutcome::NOTHING;
	out.reason = "ok";

	if (s == nullptr || !s->m_moveActive)
		return out;

	CUser * user = s->m_pUser;
	if (user == nullptr || !user->isInGame())
	{
		s->m_moveActive = false;
		return out;
	}

	return SubmitMove(s, user, user->GetX(), user->GetZ(), 0, 0, true,
		BotCore::kMovePeriodMs, now);
}

void ActionExecutor::AbandonMove(BotSession * s)
{
	if (s != nullptr)
		s->m_moveActive = false;
}
