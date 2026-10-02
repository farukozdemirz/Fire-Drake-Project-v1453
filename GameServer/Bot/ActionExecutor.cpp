#include "stdafx.h"
#include "ActionExecutor.h"
#include "BotSession.h"
#include "Telemetry.h"
#include "../Map.h"
#include "../../BotCore/BotMotion.h"
#include "../../BotCore/BotCombat.h"

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

// 'type' / 'rule' / 'reason' / 'value' / 'limit': CLI-05 checks the speed field, CLI-08 the step in metres,
// CLI-01/CLI-11 the attack interval / action rate.
static void EmitFairnessReject(BotSession * s, CUser * user, uint32 decisionId,
	const char * type, const char * rule, const char * reason, float value, float limit)
{
	if (!Telemetry::Instance().IsEnabled(TEL_DECISIONS))
		return;

	std::string fields = "\"decision_id\":" + std::to_string(decisionId)
		+ ",\"type\":\"" + type + "\",\"rule\":\"" + rule
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
		EmitFairnessReject(s, user, decisionId, "Move", speedField ? "CLI-05" : "CLI-08", reason,
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
		EmitFairnessReject(s, user, decisionId, "Move", "CLI-05", "speed_field",
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

AttackOutcome ActionExecutor::BeginAttack(BotSession * s, const std::string & targetName, uint32 count,
	std::chrono::steady_clock::time_point now)
{
	AttackOutcome out;
	out.kind = AttackOutcome::NOTHING;
	out.reason = "ok";

	(void)now;

	CUser * user = s != nullptr ? s->m_pUser : nullptr;
	if (user == nullptr || !user->isInGame())
	{
		out.kind = AttackOutcome::REFUSED;
		out.reason = "not_in_game";
		return out;
	}

	if (user->isDead())
	{
		out.kind = AttackOutcome::REFUSED;
		out.reason = "dead";
		return out;
	}

	if (count < 1 || targetName.empty())
	{
		out.kind = AttackOutcome::REFUSED;
		out.reason = "bad_target";
		return out;
	}

	s->m_attackActive = true;
	s->m_attackTargetName = targetName;
	s->m_attackLeft = count;
	s->m_attackHasLast = false;
	s->m_attackSent = 0;
	s->m_attackHits = 0;

	out.kind = AttackOutcome::SENT;
	out.reason = "ok";
	return out;
}

AttackOutcome ActionExecutor::TickAttack(BotSession * s, const AttackTarget & target,
	std::chrono::steady_clock::time_point now)
{
	AttackOutcome out;
	out.kind = AttackOutcome::NOTHING;
	out.reason = "ok";

	if (s == nullptr || !s->m_attackActive)
		return out;

	CUser * user = s->m_pUser;
	if (user == nullptr || !user->isInGame())
	{
		EndAttack(s);
		return out;
	}

	if (user->isDead())
	{
		EndAttack(s);
		out.kind = AttackOutcome::FAILED;
		out.reason = "dead";
		return out;
	}

	_ITEM_TABLE * weapon = user->GetItemPrototype(RIGHTHAND);
	bool hasWeapon = weapon != nullptr;
	uint16 weaponDelay = hasWeapon ? weapon->m_sDelay : 0;
	uint16 weaponRange = hasWeapon ? weapon->m_sRange : 0;
	uint32 intervalMs = BotCore::AttackIntervalMs(hasWeapon, weaponDelay);

	// Not this tick's turn yet: not a guard rejection, so no FAIRNESS_REJECT is written.
	if (s->m_attackHasLast)
	{
		long long sinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(
			now - s->m_attackLastSent).count();
		if (sinceLast < (long long)intervalMs)
			return out;
	}

	float dx = target.x - user->GetX();
	float dz = target.z - user->GetZ();
	float meters = std::sqrt(dx * dx + dz * dz);
	int16 distanceField = BotCore::DistanceField(meters);
	int16 rangeField = BotCore::AttackRangeField(hasWeapon, weaponRange);

	uint32 sinceLastMs = 0;
	if (s->m_attackHasLast)
		sinceLastMs = (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(
			now - s->m_attackLastSent).count();

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()).count();
	int inWindow = s->m_actionWindow.CountInWindow(nowMs);

	BotCore::AttackVerdict verdict = BotCore::CheckAttack(s->m_attackHasLast, sinceLastMs,
		intervalMs, distanceField, rangeField, inWindow);
	if (verdict != BotCore::ATTACK_OK)
	{
		const char * rule = "MEC-R-04";
		const char * reason = "out_of_range";
		float value = (float)distanceField;
		float limit = (float)rangeField;
		if (verdict == BotCore::ATTACK_REJECT_TOO_SOON)
		{
			rule = "CLI-01";
			reason = "too_soon";
			value = (float)sinceLastMs;
			limit = (float)(intervalMs < BotCore::kRMinIntervalMs ? BotCore::kRMinIntervalMs : intervalMs);
		}
		else if (verdict == BotCore::ATTACK_REJECT_RATE)
		{
			rule = "CLI-11";
			reason = "rate";
			value = (float)inWindow;
			limit = (float)BotCore::kMaxActionsPerWindow;
		}

		uint32 rejectId = NextDecisionId(s);
		EmitFairnessReject(s, user, rejectId, "Attack", rule, reason, value, limit);

		EndAttack(s);
		out.kind = AttackOutcome::REFUSED;
		out.reason = reason;
		return out;
	}

	int16 delayField = BotCore::AttackDelayField(hasWeapon, weaponDelay);

	uint32 decisionId = NextDecisionId(s);
	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"Attack\",\"target\":" + std::to_string((int)target.id)
			+ ",\"distance\":" + std::to_string((int)distanceField)
			+ ",\"delay\":" + std::to_string((int)delayField);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_SUBMIT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	Packet pkt(WIZ_ATTACK);
	pkt << uint8(1) << uint8(1) << int16(target.id) << delayField << distanceField;

	s->m_attackEcho = 0;

	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	user->HandlePacket(pkt);
	std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	long long latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

	uint64 echo = s->m_attackEcho.load();
	int result = -1;
	bool ok = false;
	bool killed = false;
	const char * reason = "no_result";
	if ((echo & (1ull << 63)) != 0
		&& (uint16)((echo >> 32) & 0xFFFF) == user->GetSocketID())
	{
		result = (int)(echo & 0xFF);
		if (result == ATTACK_SUCCESS)
		{
			ok = true;
			reason = "hit";
		}
		else if (result == ATTACK_TARGET_DEAD || result == ATTACK_TARGET_DEAD_OK)
		{
			ok = true;
			killed = true;
			reason = "killed";
		}
		else
		{
			reason = "srv_fail";
		}
	}

	s->m_attackLastSent = now;
	s->m_attackHasLast = true;
	s->m_attackSent++;
	s->m_actionWindow.Record(nowMs);
	s->m_attackLeft--;
	if (ok)
		s->m_attackHits++;

	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"Attack\",\"ok\":" + (ok ? "true" : "false")
			+ ",\"reason\":\"" + reason + "\""
			+ ",\"result\":" + std::to_string(result)
			+ ",\"latency_us\":" + std::to_string(latencyUs);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_RESULT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	if (result < 0)
	{
		EndAttack(s);
		out.kind = AttackOutcome::FAILED;
		out.reason = "no_result";
		return out;
	}

	if (killed)
	{
		EndAttack(s);
		out.kind = AttackOutcome::FINISHED;
		out.reason = "killed";
		return out;
	}

	if (s->m_attackLeft == 0)
	{
		EndAttack(s);
		out.kind = AttackOutcome::FINISHED;
		out.reason = reason;
		return out;
	}

	out.kind = AttackOutcome::SENT;
	out.reason = reason;
	return out;
}

void ActionExecutor::EndAttack(BotSession * s)
{
	if (s == nullptr)
		return;

	s->m_attackActive = false;
	s->m_attackLeft = 0;
	s->m_attackHasLast = false;
}
