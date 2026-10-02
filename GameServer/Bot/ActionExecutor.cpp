#include "stdafx.h"
#include "ActionExecutor.h"
#include "BotSession.h"
#include "Telemetry.h"
#include "../Map.h"
#include "../GameServerDlg.h"
#include "../MagicInstance.h"
#include "../../BotCore/BotMotion.h"
#include "../../BotCore/BotCombat.h"

#include <cmath>
#include <cstring>
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

	if (user->m_bResHpType == USER_SITDOWN)
	{
		out.kind = MoveOutcome::REFUSED;
		out.reason = "sitting";
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

	if (user->m_bResHpType == USER_SITDOWN)
	{
		out.kind = AttackOutcome::REFUSED;
		out.reason = "sitting";
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

// --- cast slice (ADR-0017 Ek F4-03) ---

// Maps a guard verdict to the FAIRNESS_REJECT rule/reason and the measured value/limit.
static CastOutcome RejectCast(BotSession * s, CUser * user, BotCore::CastVerdict verdict,
	const BotCore::CastStartCheck & c, uint32 sinceCastingMs, uint8 castTime, int inWindow)
{
	const char * rule = "MEC-MAG-11";
	const char * reason = "out_of_range";
	float value = 0.0f;
	float limit = 0.0f;

	switch (verdict)
	{
	case BotCore::CAST_REJECT_NOT_STANDING:
		rule = "CLI-09"; reason = "not_standing"; value = 1; limit = 0;
		break;
	case BotCore::CAST_REJECT_NO_MANA:
		rule = "MEC-MAG-08"; reason = "no_mana"; value = (float)c.mana; limit = (float)c.msp;
		break;
	case BotCore::CAST_REJECT_RECAST:
		rule = "CLI-04"; reason = "recast"; value = (float)c.sinceSkillLastMs; limit = (float)c.reCastMs;
		break;
	case BotCore::CAST_REJECT_TYPE_GATE:
		rule = "MEC-MAG-03"; reason = "type_gate"; value = (float)c.sinceTypeLastMs; limit = (float)BotCore::kTypeGateMs;
		break;
	case BotCore::CAST_REJECT_GAP:
		rule = "CLI-04"; reason = "gap"; value = (float)c.sinceAnyLastMs; limit = (float)BotCore::kCastGapMs;
		break;
	case BotCore::CAST_REJECT_RATE:
		rule = "CLI-11"; reason = "rate"; value = (float)inWindow; limit = (float)BotCore::kMaxActionsPerWindow;
		break;
	case BotCore::CAST_REJECT_TOO_EARLY:
		rule = "CLI-03"; reason = "too_early"; value = (float)sinceCastingMs; limit = (float)BotCore::CastDurationMs(castTime);
		break;
	// MEC-MAG-11: skill range in metres, or the 0.1 m attack field for a weapon-bound Type1.
	case BotCore::CAST_REJECT_OUT_OF_RANGE:
		rule = "MEC-MAG-11"; reason = "out_of_range"; value = (c.skillRange > 0) ? c.distanceM : (float)c.distanceField; limit = (c.skillRange > 0) ? (float)c.skillRange : (float)c.weaponRangeField;
		break;
	default:
		break;
	}

	uint32 decisionId = NextDecisionId(s);
	EmitFairnessReject(s, user, decisionId, "Cast", rule, reason, value, limit);

	ActionExecutor::EndCast(s);
	CastOutcome out;
	out.kind = CastOutcome::REFUSED;
	out.reason = reason;
	return out;
}

// Builds one WIZ_MAGIC_PROCESS packet, runs it through CUser::HandlePacket() and maps the result the server
// published back (via BotSession::m_castEcho). 'type' is the telemetry action name.
static CastOutcome SubmitCast(BotSession * s, CUser * user, uint8 opcode, uint32 skillId,
	const CastTarget & target, const int16 sData[3], uint32 cycle, uint32 sinceCastingMs,
	uint32 castMs, uint64 nowMs, std::chrono::steady_clock::time_point now)
{
	const char * type = (opcode == MAGIC_CASTING) ? "CastStart" : "CastEffect";
	uint32 decisionId = NextDecisionId(s);
	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"" + type + "\""
			+ ",\"skill\":" + std::to_string(skillId)
			+ ",\"target\":" + std::to_string((int)target.id)
			+ ",\"cycle\":" + std::to_string(cycle);
		if (opcode == MAGIC_CASTING)
			fields += ",\"cast_ms\":" + std::to_string(castMs);
		else
			fields += ",\"since_casting_ms\":" + std::to_string(sinceCastingMs);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_SUBMIT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	Packet pkt(WIZ_MAGIC_PROCESS);
	pkt << uint8(opcode) << uint32(skillId) << int16(user->GetID()) << int16(target.id)
		<< int16(sData[0]) << int16(sData[1]) << int16(sData[2])
		<< int16(0) << int16(0) << int16(0);

	s->m_castEcho = 0;

	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	user->HandlePacket(pkt);
	std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	long long latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

	s->m_castPackets++;
	s->m_actionWindow.Record(nowMs);

	uint64 echo = s->m_castEcho.load();
	int op = -1;
	int code = 0;
	bool ok = false;
	const char * reason = "no_result";
	if ((echo & (1ull << 63)) != 0
		&& (uint32)(echo & 0xFFFFFFFF) == skillId)
	{
		op = (int)((echo >> 48) & 0xF);
		code = (int)(int16)((echo >> 32) & 0xFFFF);
		if (opcode == MAGIC_CASTING)
		{
			if (op == MAGIC_CASTING) { ok = true; reason = "casting"; }
			else if (op == MAGIC_FAIL) { reason = "srv_fail"; }
		}
		else
		{
			if (op == MAGIC_EFFECTING)
			{
				ok = true;
				reason = (code == SKILLMAGIC_FAIL_ATTACKZERO) ? "missed" : "effected";
			}
			else if (op == MAGIC_FAIL) { reason = "srv_fail"; }
		}
	}

	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"" + type + "\""
			+ ",\"ok\":" + (ok ? "true" : "false")
			+ ",\"reason\":\"" + reason + "\""
			+ ",\"op\":" + std::to_string(op)
			+ ",\"code\":" + std::to_string(code)
			+ ",\"latency_us\":" + std::to_string(latencyUs);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_RESULT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	CastOutcome out;
	out.kind = CastOutcome::SENT;
	out.reason = reason;
	(void)now;
	return out;
}

CastOutcome ActionExecutor::BeginCast(BotSession * s, uint32 skillId, const std::string & targetName, uint32 count,
	std::chrono::steady_clock::time_point now)
{
	CastOutcome out;
	out.kind = CastOutcome::NOTHING;
	out.reason = "ok";

	(void)now;

	CUser * user = s != nullptr ? s->m_pUser : nullptr;
	if (user == nullptr || !user->isInGame())
	{
		out.kind = CastOutcome::REFUSED;
		out.reason = "not_in_game";
		return out;
	}

	if (user->isDead())
	{
		out.kind = CastOutcome::REFUSED;
		out.reason = "dead";
		return out;
	}

	if (user->m_bResHpType == USER_SITDOWN)
	{
		out.kind = CastOutcome::REFUSED;
		out.reason = "sitting";
		return out;
	}

	if (count < 1)
	{
		out.kind = CastOutcome::REFUSED;
		out.reason = "bad_skill";
		return out;
	}

	_MAGIC_TABLE * m = g_pMain->m_MagictableArray.GetData(skillId);
	if (m == nullptr)
	{
		out.kind = CastOutcome::REFUSED;
		out.reason = "bad_skill";
		return out;
	}

	if (m->sSkill != 0
		&& (user->m_sClass != m->sSkill / 10 || user->GetLevel() < m->sSkillLevel))
	{
		out.kind = CastOutcome::REFUSED;
		out.reason = "bad_skill";
		return out;
	}

	bool supportedType = (m->bType[0] == 1 || m->bType[0] == 3);
	if (!supportedType
		|| m->bType[1] != 0
		|| m->bFlyingEffect != 0
		|| m->iUseItem != 0
		|| m->sEtc != 0
		|| (m->bMoral != MORAL_SELF && m->bMoral != MORAL_FRIEND_WITHME
			&& m->bMoral != MORAL_ENEMY && m->bMoral != MORAL_ALL))
	{
		out.kind = CastOutcome::REFUSED;
		out.reason = "unsupported_skill";
		return out;
	}

	bool self = targetName.empty();
	bool wantedSelf = (m->bMoral == MORAL_SELF);
	bool wantedTarget = (m->bMoral == MORAL_ENEMY);
	if ((wantedSelf && !self) || (wantedTarget && self))
	{
		out.kind = CastOutcome::REFUSED;
		out.reason = "bad_target";
		return out;
	}

	s->m_castPhase = BotSession::CAST_ARMED;
	s->m_castSkillId = skillId;
	s->m_castTargetName = targetName;
	s->m_castLeft = count;
	s->m_castCycle = 0;
	s->m_castDone = 0;
	s->m_castPackets = 0;
	s->m_castSelfId = user->GetID();

	out.kind = CastOutcome::SENT;
	out.reason = "ok";
	return out;
}

CastOutcome ActionExecutor::TickCast(BotSession * s, const CastTarget & target,
	std::chrono::steady_clock::time_point now)
{
	CastOutcome out;
	out.kind = CastOutcome::NOTHING;
	out.reason = "ok";

	if (s == nullptr || s->m_castPhase == BotSession::CAST_IDLE)
		return out;

	CUser * user = s->m_pUser;
	if (user == nullptr || !user->isInGame())
	{
		EndCast(s);
		return out;
	}

	if (user->isDead())
	{
		EndCast(s);
		out.kind = CastOutcome::FAILED;
		out.reason = "dead";
		return out;
	}

	_MAGIC_TABLE * m = g_pMain->m_MagictableArray.GetData(s->m_castSkillId);
	if (m == nullptr)
	{
		EndCast(s);
		out.kind = CastOutcome::FAILED;
		out.reason = "bad_skill";
		return out;
	}

	// Target view (metres).
	float dx = target.x - user->GetX();
	float dz = target.z - user->GetZ();
	float meters = target.isSelf ? 0.0f : std::sqrt(dx * dx + dz * dz);

	_ITEM_TABLE * weapon = user->GetItemPrototype(RIGHTHAND);
	bool hasWeapon = weapon != nullptr;
	uint16 weaponRange = hasWeapon ? weapon->m_sRange : 0;

	int16 distanceField = BotCore::DistanceField(meters);
	int16 weaponRangeField = BotCore::AttackRangeField(hasWeapon, weaponRange);

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()).count();
	int inWindow = s->m_actionWindow.CountInWindow(nowMs);

	// Timing inputs from the reuse timers.
	uint32 sinceSkillLastMs = 0;
	bool hasSkillLast = false;
	std::map<uint32, std::chrono::steady_clock::time_point>::iterator skillIt = s->m_castSkillLast.find(s->m_castSkillId);
	if (skillIt != s->m_castSkillLast.end())
	{
		hasSkillLast = true;
		sinceSkillLastMs = (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(
			now - skillIt->second).count();
	}

	uint8 type0 = m->bType[0];
	bool typeGated = (m->iNum < 400000 && type0 >= 1 && type0 <= 7);
	bool hasTypeLast = false;
	uint32 sinceTypeLastMs = 0;
	if (type0 < 8 && s->m_castTypeHas[type0])
	{
		hasTypeLast = true;
		sinceTypeLastMs = (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(
			now - s->m_castTypeLast[type0]).count();
	}

	uint32 sinceAnyLastMs = 0;
	if (s->m_castAnyHas)
		sinceAnyLastMs = (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(
			now - s->m_castAnyLast).count();

	BotCore::CastStartCheck c;
	c.distanceM = meters;
	c.skillRange = m->sRange;
	c.distanceField = distanceField;
	c.weaponRangeField = weaponRangeField;
	c.needsStanding = (m->sUseStanding == 1);
	c.standing = !s->m_moveActive;
	c.mana = user->GetMana();
	c.msp = m->sMsp;
	c.reCastMs = BotCore::CastRecastMs(m->sReCastTime);
	c.hasSkillLast = hasSkillLast;
	c.sinceSkillLastMs = sinceSkillLastMs;
	c.typeGated = typeGated;
	c.hasTypeLast = hasTypeLast;
	c.sinceTypeLastMs = sinceTypeLastMs;
	c.hasAnyLast = s->m_castAnyHas;
	c.sinceAnyLastMs = sinceAnyLastMs;
	c.actionsInWindow = inWindow;

	int16 sData[3];
	sData[0] = target.isSelf ? 0 : int16(target.x);
	sData[1] = target.isSelf ? 0 : int16(target.y);
	sData[2] = target.isSelf ? 0 : int16(target.z);

	if (s->m_castPhase == BotSession::CAST_ARMED)
	{
		// Not this tick's turn yet: not a guard rejection, so no FAIRNESS_REJECT is written.
		if (BotCore::CastWaitMs(c) > 0)
			return out;

		BotCore::CastVerdict verdict = BotCore::CheckCastStart(c);
		if (verdict != BotCore::CAST_OK)
			return RejectCast(s, user, verdict, c, 0, m->bCastTime, inWindow);

		s->m_castCycle++;

		if (m->bCastTime > 0)
		{
			CastOutcome cast = SubmitCast(s, user, MAGIC_CASTING, s->m_castSkillId, target, sData,
				s->m_castCycle, 0, BotCore::CastDurationMs(m->bCastTime), nowMs, now);
			if (cast.reason != nullptr && std::strcmp(cast.reason, "casting") == 0)
			{
				s->m_castPhase = BotSession::CAST_CASTING;
				s->m_castCastingAt = now;
				return cast;
			}

			// CASTING was not accepted (srv_fail / no_result): drop the series.
			const char * reason = cast.reason;
			EndCast(s);
			out.kind = CastOutcome::FAILED;
			out.reason = reason;
			return out;
		}
		// bCastTime == 0: fall through to EFFECTING now (sinceCastingMs = 0).
	}

	uint32 sinceCastingMs = 0;
	if (s->m_castPhase == BotSession::CAST_CASTING)
	{
		uint32 wait = BotCore::CastDurationMs(m->bCastTime);
		long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			now - s->m_castCastingAt).count();
		if (elapsed < (long long)wait)
			return out;

		sinceCastingMs = (uint32)elapsed;
	}

	bool inRange = BotCore::CastInRange(meters, m->sRange, distanceField, weaponRangeField);
	BotCore::CastVerdict effectVerdict = BotCore::CheckCastEffect(inRange, sinceCastingMs, m->bCastTime, inWindow);
	if (effectVerdict != BotCore::CAST_OK)
		return RejectCast(s, user, effectVerdict, c, sinceCastingMs, m->bCastTime, inWindow);

	CastOutcome effect = SubmitCast(s, user, MAGIC_EFFECTING, s->m_castSkillId, target, sData,
		s->m_castCycle, sinceCastingMs, BotCore::CastDurationMs(m->bCastTime), nowMs, now);
	const char * reason = effect.reason;

	// Reuse timers: the bot is conservative (the server only records a timestamp on success).
	s->m_castSkillLast[s->m_castSkillId] = now;
	if (type0 < 8)
	{
		s->m_castTypeHas[type0] = true;
		s->m_castTypeLast[type0] = now;
	}
	s->m_castAnyHas = true;
	s->m_castAnyLast = now;

	if (std::strcmp(reason, "effected") == 0 || std::strcmp(reason, "missed") == 0)
		s->m_castDone++;

	s->m_castLeft--;

	if (std::strcmp(reason, "no_result") == 0)
	{
		EndCast(s);
		out.kind = CastOutcome::FAILED;
		out.reason = "no_result";
		return out;
	}

	if (s->m_castLeft == 0)
	{
		EndCast(s);
		out.kind = CastOutcome::FINISHED;
		out.reason = reason;
		return out;
	}

	s->m_castPhase = BotSession::CAST_ARMED;
	out.kind = CastOutcome::SENT;
	out.reason = reason;
	return out;
}

void ActionExecutor::EndCast(BotSession * s)
{
	if (s == nullptr)
		return;

	s->m_castPhase = BotSession::CAST_IDLE;
	s->m_castLeft = 0;
}

// --- potion slice (ADR-0017 Ek F4-04) ---

// Counts 'itemId' in the bot's own bag only (slots 14..41). Equipment, cospre and magic bag are not counted.
static uint32 CountInBag(CUser * user, uint32 itemId)
{
	uint32 count = 0;
	for (uint8 i = INVENTORY_INVENT; i < INVENTORY_INVENT + HAVE_MAX; i++)
	{
		_ITEM_DATA * item = user->GetItem(i);
		if (item != nullptr && item->nNum == itemId)
			count += item->sCount;
	}

	return count;
}

// Maps a pot guard verdict to the FAIRNESS_REJECT rule/reason and the measured value/limit.
static PotionOutcome RejectPotion(BotSession * s, CUser * user, BotCore::PotionVerdict verdict,
	const BotCore::PotionCheck & c)
{
	const char * rule = "CLI-06";
	const char * reason = "no_stock";
	float value = (float)c.stock;
	float limit = 1.0f;

	switch (verdict)
	{
	case BotCore::POT_REJECT_COOLDOWN:
		rule = "CLI-06"; reason = "pot_cooldown"; value = (float)c.sinceLastMs; limit = (float)BotCore::kPotCooldownMs;
		break;
	case BotCore::POT_REJECT_RATE:
		rule = "CLI-11"; reason = "rate"; value = (float)c.actionsInWindow; limit = (float)BotCore::kMaxActionsPerWindow;
		break;
	default:
		break;
	}

	uint32 decisionId = NextDecisionId(s);
	EmitFairnessReject(s, user, decisionId, "Potion", rule, reason, value, limit);

	ActionExecutor::EndPotion(s);
	PotionOutcome out;
	out.kind = PotionOutcome::REFUSED;
	out.reason = reason;
	return out;
}

// Builds one WIZ_MAGIC_PROCESS (MAGIC_EFFECTING) for the bot's own pot, runs it through CUser::HandlePacket() and
// maps the result the server published back (via BotSession::m_castEcho). 'stockBefore' is for telemetry only.
static PotionOutcome SubmitPotion(BotSession * s, CUser * user, uint32 stockBefore, uint64 nowMs)
{
	uint32 decisionId = NextDecisionId(s);
	const char * kind = (s->m_potKind == 2) ? "mp" : "hp";
	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"UsePotion\""
			+ ",\"item\":" + std::to_string(s->m_potItemId)
			+ ",\"skill\":" + std::to_string(s->m_potSkillId)
			+ ",\"kind\":\"" + kind + "\""
			+ ",\"stock\":" + std::to_string(stockBefore)
			+ ",\"use\":" + std::to_string(s->m_potSent + 1);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_SUBMIT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	Packet pkt(WIZ_MAGIC_PROCESS);
	pkt << uint8(MAGIC_EFFECTING) << uint32(s->m_potSkillId)
		<< int16(user->GetID()) << int16(user->GetID())
		<< int16(0) << int16(0) << int16(0) << int16(0) << int16(0) << int16(0);

	s->m_castEcho = 0;

	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	user->HandlePacket(pkt);
	std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	long long latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

	s->m_actionWindow.Record(nowMs);

	uint64 echo = s->m_castEcho.load();
	int op = -1;
	int code = 0;
	bool ok = false;
	const char * reason = "no_result";
	if ((echo & (1ull << 63)) != 0
		&& (uint32)(echo & 0xFFFFFFFF) == s->m_potSkillId)
	{
		op = (int)((echo >> 48) & 0xF);
		code = (int)(int16)((echo >> 32) & 0xFFFF);
		if (op == MAGIC_EFFECTING) { ok = true; reason = "effected"; }
		else if (op == MAGIC_FAIL) { reason = "srv_fail"; }
	}

	// 'stock_after' is for operators/verification only; the outcome never reads it (MB-01 pots do not drop).
	uint32 stockAfter = CountInBag(user, s->m_potItemId);

	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"UsePotion\""
			+ ",\"ok\":" + (ok ? "true" : "false")
			+ ",\"reason\":\"" + reason + "\""
			+ ",\"op\":" + std::to_string(op)
			+ ",\"code\":" + std::to_string(code)
			+ ",\"latency_us\":" + std::to_string(latencyUs)
			+ ",\"stock_after\":" + std::to_string(stockAfter);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_RESULT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	PotionOutcome out;
	out.kind = PotionOutcome::SENT;
	out.reason = reason;
	return out;
}

PotionOutcome ActionExecutor::BeginPotion(BotSession * s, uint32 itemId, uint32 count,
	std::chrono::steady_clock::time_point now)
{
	PotionOutcome out;
	out.kind = PotionOutcome::NOTHING;
	out.reason = "ok";

	(void)now;

	CUser * user = s != nullptr ? s->m_pUser : nullptr;
	if (user == nullptr || !user->isInGame())
	{
		out.kind = PotionOutcome::REFUSED;
		out.reason = "not_in_game";
		return out;
	}

	if (user->isDead())
	{
		out.kind = PotionOutcome::REFUSED;
		out.reason = "dead";
		return out;
	}

	if (count < 1)
	{
		out.kind = PotionOutcome::REFUSED;
		out.reason = "bad_item";
		return out;
	}

	_ITEM_TABLE * it = g_pMain->GetItemPtr(itemId);
	if (it == nullptr || it->m_iEffect1 == 0)
	{
		out.kind = PotionOutcome::REFUSED;
		out.reason = "bad_item";
		return out;
	}

	// Mirrors the server's own item checks (MagicInstance.cpp:1018-1028).
	if ((it->m_bClass != 0 && !user->JobGroupCheck(it->m_bClass))
		|| (it->m_bReqLevel != 0 && user->GetLevel() < it->m_bReqLevel))
	{
		out.kind = PotionOutcome::REFUSED;
		out.reason = "bad_item";
		return out;
	}

	_MAGIC_TABLE * m = g_pMain->m_MagictableArray.GetData(it->m_iEffect1);
	if (m == nullptr)
	{
		out.kind = PotionOutcome::REFUSED;
		out.reason = "bad_item";
		return out;
	}

	_MAGIC_TYPE3 * t3 = g_pMain->m_Magictype3Array.GetData(it->m_iEffect1);

	bool supported =
		m->bType[0] == 3
		&& m->bType[1] == 0
		&& m->bMoral == MORAL_SELF
		&& m->sSkill == 0
		&& m->sMsp == 0
		&& m->sUseStanding == 0
		&& m->sEtc == 0
		&& m->bFlyingEffect == 0
		&& (m->iUseItem == 0 || m->iUseItem == itemId)
		&& BotCore::PotSupported(m->sReCastTime)
		&& t3 != nullptr
		&& (t3->bDirectType == 1 || t3->bDirectType == 2)
		&& t3->sFirstDamage > 0
		&& t3->sTimeDamage == 0;
	if (!supported)
	{
		out.kind = PotionOutcome::REFUSED;
		out.reason = "unsupported_item";
		return out;
	}

	s->m_potActive = true;
	s->m_potItemId = itemId;
	s->m_potSkillId = it->m_iEffect1;
	s->m_potKind = t3->bDirectType;
	s->m_potLeft = count;
	s->m_potSent = 0;
	s->m_potOk = 0;
	s->m_castSelfId = user->GetID();

	out.kind = PotionOutcome::SENT;
	out.reason = "ok";
	return out;
}

PotionOutcome ActionExecutor::TickPotion(BotSession * s, std::chrono::steady_clock::time_point now)
{
	PotionOutcome out;
	out.kind = PotionOutcome::NOTHING;
	out.reason = "ok";

	if (s == nullptr || !s->m_potActive)
		return out;

	CUser * user = s->m_pUser;
	if (user == nullptr || !user->isInGame())
	{
		EndPotion(s);
		out.kind = PotionOutcome::REFUSED;
		out.reason = "not_in_game";
		return out;
	}

	if (user->isDead())
	{
		EndPotion(s);
		out.kind = PotionOutcome::REFUSED;
		out.reason = "dead";
		return out;
	}

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()).count();
	int inWindow = s->m_actionWindow.CountInWindow(nowMs);

	BotCore::PotionCheck c;
	c.stock = CountInBag(user, s->m_potItemId);
	c.hasLast = s->m_potHasLast;
	c.sinceLastMs = 0;
	if (s->m_potHasLast)
		c.sinceLastMs = (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(
			now - s->m_potLast).count();
	c.actionsInWindow = inWindow;

	// The bag rule is checked before every packet (CLI-06; the server does not check MB-01 pots).
	if (c.stock < 1)
		return RejectPotion(s, user, BotCore::POT_REJECT_NO_STOCK, c);

	// Shared timer still running: normal flow, not a guard rejection, so no FAIRNESS_REJECT is written.
	if (BotCore::PotionWaitMs(c) > 0)
		return out;

	BotCore::PotionVerdict verdict = BotCore::CheckPotion(c);
	if (verdict != BotCore::POT_OK)
		return RejectPotion(s, user, verdict, c);

	PotionOutcome sent = SubmitPotion(s, user, c.stock, nowMs);

	// The pot feeds the cast timers as if it were a type-3 effect (ADR-0017 Ek F4-04 item 5).
	s->m_potHasLast = true;
	s->m_potLast = now;
	s->m_castTypeHas[3] = true;
	s->m_castTypeLast[3] = now;
	s->m_castAnyHas = true;
	s->m_castAnyLast = now;

	s->m_potSent++;

	const char * reason = sent.reason;
	if (std::strcmp(reason, "effected") != 0)
	{
		EndPotion(s);
		out.kind = PotionOutcome::FAILED;
		out.reason = reason;
		return out;
	}

	s->m_potOk++;
	s->m_potLeft--;

	if (s->m_potLeft == 0)
	{
		EndPotion(s);
		out.kind = PotionOutcome::FINISHED;
		out.reason = "effected";
		return out;
	}

	out.kind = PotionOutcome::SENT;
	out.reason = "effected";
	return out;
}

void ActionExecutor::EndPotion(BotSession * s)
{
	if (s == nullptr)
		return;

	s->m_potActive = false;
	s->m_potLeft = 0;
}

// --- stance slice (ADR-0017 Ek F4-05) ---

// Maps a stance guard verdict to the FAIRNESS_REJECT rule/reason and the measured value/limit.
static StanceOutcome RejectStance(BotSession * s, CUser * user, BotCore::StanceVerdict verdict,
	const BotCore::StanceCheck & c)
{
	const char * rule = "CLI-13";
	const char * reason = "busy";
	float value = 1.0f;
	float limit = 0.0f;

	switch (verdict)
	{
	case BotCore::STANCE_REJECT_TOGGLE:
		rule = "CLI-13"; reason = "toggle"; value = (float)c.sinceLastMs; limit = (float)BotCore::kStanceToggleMinMs;
		break;
	case BotCore::STANCE_REJECT_RATE:
		rule = "CLI-11"; reason = "rate"; value = (float)c.actionsInWindow; limit = (float)BotCore::kMaxActionsPerWindow;
		break;
	default:
		break;
	}

	uint32 decisionId = NextDecisionId(s);
	EmitFairnessReject(s, user, decisionId, "State", rule, reason, value, limit);

	StanceOutcome out;
	out.kind = StanceOutcome::REFUSED;
	out.reason = reason;
	return out;
}

StanceOutcome ActionExecutor::SetStance(BotSession * s, bool sit, std::chrono::steady_clock::time_point now)
{
	StanceOutcome out;
	out.kind = StanceOutcome::NOTHING;
	out.reason = "ok";

	CUser * user = s != nullptr ? s->m_pUser : nullptr;
	if (user == nullptr || !user->isInGame())
	{
		out.kind = StanceOutcome::REFUSED;
		out.reason = "not_in_game";
		return out;
	}

	if (user->isDead())
	{
		out.kind = StanceOutcome::REFUSED;
		out.reason = "dead";
		return out;
	}

	// Already in the requested stance: refuse without an event (the client sends no packet either).
	bool sittingNow = (user->m_bResHpType == USER_SITDOWN);
	if (sit == sittingNow)
	{
		out.kind = StanceOutcome::REFUSED;
		out.reason = "no_change";
		return out;
	}

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()).count();
	int inWindow = s->m_actionWindow.CountInWindow(nowMs);

	uint32 sinceLastMs = 0;
	if (s->m_stanceHasLast)
		sinceLastMs = (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(
			now - s->m_stanceLast).count();

	BotCore::StanceCheck c;
	c.toSit = sit;
	c.busy = s->m_moveActive || s->m_attackActive || s->m_castPhase != BotSession::CAST_IDLE;
	c.hasLast = s->m_stanceHasLast;
	c.sinceLastMs = sinceLastMs;
	c.actionsInWindow = inWindow;

	BotCore::StanceVerdict verdict = BotCore::CheckStance(c);
	if (verdict != BotCore::STANCE_OK)
		return RejectStance(s, user, verdict, c);

	uint32 decisionId = NextDecisionId(s);
	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"StateSit\",\"to\":\"" + (sit ? "sit" : "stand") + "\"";
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_SUBMIT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	// Server reads u8 bType + u16 nBuff (User.cpp:2715-2716).
	Packet pkt(WIZ_STATE_CHANGE);
	pkt << uint8(1) << uint16(sit ? USER_SITDOWN : USER_STANDING);

	s->m_castSelfId = user->GetID();
	s->m_stateEcho = 0;

	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	user->HandlePacket(pkt);
	std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	long long latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

	s->m_actionWindow.Record(nowMs);
	s->m_stanceHasLast = true;
	s->m_stanceLast = now;

	// Result only from the broadcast the server published (m_stateEcho); m_bResHpType is telemetry only.
	uint64 echo = s->m_stateEcho.load();
	uint32 wanted = sit ? USER_SITDOWN : USER_STANDING;
	bool ok = (echo & (1ull << 63)) != 0
		&& ((echo >> 32) & 0xFF) == 1
		&& (uint32)(echo & 0xFFFFFFFF) == wanted;

	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"StateSit\""
			+ ",\"ok\":" + (ok ? "true" : "false")
			+ ",\"reason\":\"" + (ok ? "applied" : "no_result") + "\""
			+ ",\"latency_us\":" + std::to_string(latencyUs)
			+ ",\"state_after\":" + std::to_string((int)user->m_bResHpType);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_RESULT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	if (ok)
	{
		out.kind = StanceOutcome::SENT;
		out.reason = "applied";
	}
	else
	{
		out.kind = StanceOutcome::FAILED;
		out.reason = "no_result";
	}
	return out;
}

// --- target HP slice (ADR-0017 Ek F4-06) ---

// Maps a target HP guard verdict to the FAIRNESS_REJECT rule/reason and the measured value/limit.
static TargetHpOutcome RejectTargetHp(BotSession * s, CUser * user, BotCore::TargetHpVerdict verdict,
	const BotCore::TargetHpCheck & c)
{
	const char * rule = "CLI-10";
	const char * reason = "out_of_view";
	float value = (float)c.regionDelta;
	float limit = (float)BotCore::kViewRegionRadius;

	switch (verdict)
	{
	case BotCore::TARGETHP_REJECT_POLL:
		rule = "CLI-10"; reason = "poll"; value = (float)c.sinceLastMs; limit = (float)BotCore::kTargetHpPollMs;
		break;
	case BotCore::TARGETHP_REJECT_RATE:
		rule = "CLI-11"; reason = "rate"; value = (float)c.actionsInWindow; limit = (float)BotCore::kMaxActionsPerWindow;
		break;
	default:
		break;
	}

	uint32 decisionId = NextDecisionId(s);
	EmitFairnessReject(s, user, decisionId, "TargetHp", rule, reason, value, limit);

	TargetHpOutcome out;
	out.kind = TargetHpOutcome::REFUSED;
	out.reason = reason;
	out.hp = 0;
	out.maxHp = 0;
	return out;
}

TargetHpOutcome ActionExecutor::RequestTargetHp(BotSession * s, const TargetHpTarget & target,
	std::chrono::steady_clock::time_point now)
{
	TargetHpOutcome out;
	out.kind = TargetHpOutcome::NOTHING;
	out.reason = "ok";
	out.hp = 0;
	out.maxHp = 0;

	CUser * user = s != nullptr ? s->m_pUser : nullptr;
	if (user == nullptr || !user->isInGame())
	{
		out.kind = TargetHpOutcome::REFUSED;
		out.reason = "not_in_game";
		return out;
	}

	if (user->isDead())
	{
		out.kind = TargetHpOutcome::REFUSED;
		out.reason = "dead";
		return out;
	}

	if (target.id < 0 || target.id == (int16)user->GetSocketID())
	{
		out.kind = TargetHpOutcome::REFUSED;
		out.reason = "bad_target";
		return out;
	}

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()).count();
	int inWindow = s->m_actionWindow.CountInWindow(nowMs);

	uint32 sinceLastMs = 0;
	if (s->m_hpReqHasLast)
		sinceLastMs = (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(
			now - s->m_hpReqLast).count();

	BotCore::TargetHpCheck c;
	c.regionDelta = BotCore::RegionDelta(user->GetX(), user->GetZ(), target.x, target.z);
	c.sameTarget = (s->m_hpReqTargetId == (int)target.id);
	c.hasLast = s->m_hpReqHasLast;
	c.sinceLastMs = sinceLastMs;
	c.actionsInWindow = inWindow;

	BotCore::TargetHpVerdict verdict = BotCore::CheckTargetHp(c);
	if (verdict != BotCore::TARGETHP_OK)
		return RejectTargetHp(s, user, verdict, c);

	// New/different target = selection (echo=1); re-polling the selected target = echo=0 (ADR-0017 Ek F4-06 item 3).
	uint8 echo = c.sameTarget ? 0 : 1;

	uint32 decisionId = NextDecisionId(s);
	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"TargetHpReq\",\"target\":" + std::to_string((int)target.id)
			+ ",\"echo\":" + std::to_string((unsigned)echo);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_SUBMIT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	// Server reads u16 uid + u8 echo (User.cpp:358-359).
	Packet pkt(WIZ_TARGET_HP);
	pkt << uint16(target.id) << uint8(echo);

	s->m_targetHpEcho = 0;
	s->m_targetHpValues = 0;

	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	user->HandlePacket(pkt);
	std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	long long latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

	s->m_actionWindow.Record(nowMs);
	s->m_hpReqTargetId = target.id;
	s->m_hpReqHasLast = true;
	s->m_hpReqLast = now;

	// Result only from the reply the server published (m_targetHpEcho / m_targetHpValues); never from the server object.
	uint64 e = s->m_targetHpEcho.load();
	bool observed = (e & (1ull << 63)) != 0
		&& (uint16)(e & 0xFFFF) == (uint16)target.id
		&& (uint8)((e >> 16) & 0xFF) == echo;

	int32 hp = 0, maxHp = 0;
	if (observed)
	{
		uint64 v = s->m_targetHpValues.load();
		maxHp = (int32)(uint32)(v & 0xFFFFFFFF);
		hp = (int32)(uint32)(v >> 32);
	}

	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"TargetHpReq\""
			+ ",\"ok\":" + (observed ? "true" : "false")
			+ ",\"reason\":\"" + (observed ? "observed" : "no_result") + "\""
			+ ",\"latency_us\":" + std::to_string(latencyUs);
		if (observed)
			fields += ",\"hp\":" + std::to_string((int)hp)
				+ ",\"max_hp\":" + std::to_string((int)maxHp);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_RESULT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	if (observed)
	{
		out.kind = TargetHpOutcome::SENT;
		out.reason = "observed";
		out.hp = hp;
		out.maxHp = maxHp;
	}
	else
	{
		out.kind = TargetHpOutcome::FAILED;
		out.reason = "no_result";
	}
	return out;
}

// --- respawn slice (ADR-0017 Ek F4-07) ---

// Maps a regene guard verdict to the FAIRNESS_REJECT rule/reason and the measured value/limit.
static RegeneOutcome RejectRegene(BotSession * s, CUser * user, BotCore::RegeneVerdict verdict,
	const BotCore::RegeneCheck & c)
{
	const char * rule = "CLI-14";
	const char * reason = "dead_wait";
	float value = (float)c.sinceDeadMs;
	float limit = (float)BotCore::kRegeneMinDeadMs;

	switch (verdict)
	{
	case BotCore::REGENE_REJECT_RATE:
		rule = "CLI-11"; reason = "rate"; value = (float)c.actionsInWindow; limit = (float)BotCore::kMaxActionsPerWindow;
		break;
	default:
		break;
	}

	uint32 decisionId = NextDecisionId(s);
	EmitFairnessReject(s, user, decisionId, "Regene", rule, reason, value, limit);

	RegeneOutcome out;
	out.kind = RegeneOutcome::REFUSED;
	out.reason = reason;
	out.x = 0.0f;
	out.z = 0.0f;
	return out;
}

RegeneOutcome ActionExecutor::RequestRegene(BotSession * s, std::chrono::steady_clock::time_point now)
{
	RegeneOutcome out;
	out.kind = RegeneOutcome::NOTHING;
	out.reason = "ok";
	out.x = 0.0f;
	out.z = 0.0f;

	CUser * user = s != nullptr ? s->m_pUser : nullptr;
	if (user == nullptr || !user->isInGame())
	{
		out.kind = RegeneOutcome::REFUSED;
		out.reason = "not_in_game";
		return out;
	}

	if (!user->isDead())
	{
		out.kind = RegeneOutcome::REFUSED;
		out.reason = "not_dead";
		return out;
	}

	// Server would kick a loyalty-0 bot out of the PK zone after the respawn (AttackHandler.cpp:240-243, KI-013).
	if (user->GetLoyalty() == 0)
	{
		out.kind = RegeneOutcome::REFUSED;
		out.reason = "no_np";
		return out;
	}

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()).count();

	// First time the death is noticed here: remember it (TickSessions() usually got there first).
	if (!s->m_deadSeen)
	{
		s->m_deadSeen = true;
		s->m_deadSince = now;
	}

	uint32 sinceDeadMs = 0;
	if (now > s->m_deadSince)
		sinceDeadMs = (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(
			now - s->m_deadSince).count();

	int inWindow = s->m_actionWindow.CountInWindow(nowMs);

	BotCore::RegeneCheck c;
	c.sinceDeadMs = sinceDeadMs;
	c.actionsInWindow = inWindow;

	BotCore::RegeneVerdict verdict = BotCore::CheckRegene(c);
	if (verdict != BotCore::REGENE_OK)
		return RejectRegene(s, user, verdict, c);

	uint32 decisionId = NextDecisionId(s);
	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"Regene\",\"regene_type\":1";
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_SUBMIT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	// Server reads u8 type (User.cpp:334-336).
	Packet pkt(WIZ_REGENE);
	pkt << uint8(1);

	s->m_regeneEcho = 0;

	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	user->HandlePacket(pkt);
	std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	long long latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

	s->m_actionWindow.Record(nowMs);

	// Result only from the reply the server published (m_regeneEcho); never from the server object.
	uint64 e = s->m_regeneEcho.load();
	bool respawned = (e & (1ull << 63)) != 0;

	float x = 0.0f, z = 0.0f;
	if (respawned)
	{
		x = (float)((e >> 32) & 0xFFFF) / 10.0f;
		z = (float)((e >> 16) & 0xFFFF) / 10.0f;
		s->m_deadSeen = false;
	}

	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"Regene\""
			+ ",\"ok\":" + (respawned ? "true" : "false")
			+ ",\"reason\":\"" + (respawned ? "respawned" : "no_result") + "\""
			+ ",\"latency_us\":" + std::to_string(latencyUs)
			+ ",\"alive\":" + (user->isDead() ? "false" : "true");
		if (respawned)
			fields += ",\"x\":" + FormatFixed(x, 1) + ",\"z\":" + FormatFixed(z, 1);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_RESULT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	if (respawned)
	{
		out.kind = RegeneOutcome::SENT;
		out.reason = "respawned";
		out.x = x;
		out.z = z;
	}
	else
	{
		out.kind = RegeneOutcome::FAILED;
		out.reason = "no_result";
	}
	return out;
}

// --- party slice (ADR-0017 Ek F4-08) ---

// Maps a party invitation guard verdict to the FAIRNESS_REJECT rule/reason and the measured value/limit.
static PartyOutcome RejectPartyInvite(BotSession * s, CUser * user, BotCore::PartyInviteVerdict verdict,
	const BotCore::PartyInviteCheck & c, int peerId)
{
	const char * rule = "CLI-15";
	const char * reason = "not_leader";
	float value = 0.0f;
	float limit = 0.0f;

	switch (verdict)
	{
	case BotCore::PARTYINVITE_REJECT_VIEW:
		rule = "CLI-15"; reason = "out_of_view"; value = (float)c.regionDelta; limit = (float)BotCore::kViewRegionRadius;
		break;
	case BotCore::PARTYINVITE_REJECT_GAP:
		rule = "CLI-15"; reason = "invite_gap"; value = (float)c.sinceLastMs; limit = (float)BotCore::kPartyInviteGapMs;
		break;
	case BotCore::PARTYINVITE_REJECT_RATE:
		rule = "CLI-11"; reason = "rate"; value = (float)c.actionsInWindow; limit = (float)BotCore::kMaxActionsPerWindow;
		break;
	default:
		break;
	}

	uint32 decisionId = NextDecisionId(s);
	EmitFairnessReject(s, user, decisionId, "PartyInvite", rule, reason, value, limit);

	PartyOutcome out;
	out.kind = PartyOutcome::REFUSED;
	out.reason = reason;
	out.peerId = peerId;
	return out;
}

// Maps a party acceptance guard verdict to the FAIRNESS_REJECT rule/reason and the measured value/limit.
static PartyOutcome RejectPartyAccept(BotSession * s, CUser * user, BotCore::PartyAcceptVerdict verdict,
	const BotCore::PartyAcceptCheck & c, int peerId)
{
	const char * rule = "CLI-15";
	const char * reason = "accept_wait";
	float value = (float)c.sinceInviteMs;
	float limit = (float)BotCore::kPartyAcceptMinMs;

	if (verdict == BotCore::PARTYACCEPT_REJECT_RATE)
	{
		rule = "CLI-11"; reason = "rate"; value = (float)c.actionsInWindow; limit = (float)BotCore::kMaxActionsPerWindow;
	}

	uint32 decisionId = NextDecisionId(s);
	EmitFairnessReject(s, user, decisionId, "PartyAccept", rule, reason, value, limit);

	PartyOutcome out;
	out.kind = PartyOutcome::REFUSED;
	out.reason = reason;
	out.peerId = peerId;
	return out;
}

PartyOutcome ActionExecutor::RequestPartyInvite(BotSession * s, const PartyInviteTarget & target,
	std::chrono::steady_clock::time_point now)
{
	PartyOutcome out;
	out.kind = PartyOutcome::NOTHING;
	out.reason = "ok";
	out.peerId = -1;

	CUser * user = s != nullptr ? s->m_pUser : nullptr;
	if (user == nullptr || !user->isInGame())
	{
		out.kind = PartyOutcome::REFUSED;
		out.reason = "not_in_game";
		return out;
	}

	if (user->isDead())
	{
		out.kind = PartyOutcome::REFUSED;
		out.reason = "dead";
		return out;
	}

	// 20 = MAX_ID_SIZE, the same bound the server applies to the invited name.
	if (target.id < 0 || target.id == (int16)user->GetID() || target.name.empty() || target.name.size() > 20)
	{
		out.kind = PartyOutcome::REFUSED;
		out.reason = "bad_target";
		return out;
	}

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()).count();
	int inWindow = s->m_actionWindow.CountInWindow(nowMs);

	uint32 sinceLastMs = 0;
	if (s->m_partyInviteHasLast)
		sinceLastMs = (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(
			now - s->m_partyInviteLast).count();

	BotCore::PartyInviteCheck c;
	c.inParty = user->isInParty();
	c.isLeader = user->isPartyLeader();
	c.regionDelta = BotCore::RegionDelta(user->GetX(), user->GetZ(), target.x, target.z);
	c.hasLast = s->m_partyInviteHasLast;
	c.sinceLastMs = sinceLastMs;
	c.actionsInWindow = inWindow;

	BotCore::PartyInviteVerdict verdict = BotCore::CheckPartyInvite(c);
	if (verdict != BotCore::PARTYINVITE_OK)
		return RejectPartyInvite(s, user, verdict, c, target.id);

	bool create = !c.inParty;

	uint32 decisionId = NextDecisionId(s);
	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"PartyInvite\",\"target\":" + std::to_string((int)target.id)
			+ ",\"invite_mode\":\"" + (create ? "create" : "insert") + "\"";
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_SUBMIT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	// Server reads u8 sub-opcode + a KO string (PartyHandler.cpp:11-16); the default double-byte length is what it reads.
	Packet pkt(WIZ_PARTY, uint8(create ? PARTY_CREATE : PARTY_INSERT));
	pkt << target.name;

	s->m_castSelfId = user->GetID();
	s->m_partyErrorEcho = 0;
	s->m_stateEcho = 0;

	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	user->HandlePacket(pkt);
	std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	long long latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

	s->m_actionWindow.Record(nowMs);
	s->m_partyInviteHasLast = true;
	s->m_partyInviteLast = now;

	// Result only from published replies; never from the server object.
	uint64 e = s->m_partyErrorEcho.load();
	int code = 0;
	const char * reason = nullptr;
	bool failed = false;
	if ((e & (1ull << 63)) != 0)
	{
		code = (int)(int16)(e & 0xFFFF);
		if (code == -1)
			reason = "refused_target";
		else if (code == -2)
			reason = "refused_level";
		else if (code == -3)
			reason = "refused_zone";
		else
			reason = "refused_other";
		failed = true;
	}
	else if (create)
	{
		// PARTY_CREATE is confirmed by the bot's own leader state broadcast (WIZ_STATE_CHANGE type 6, nBuff 1).
		uint64 st = s->m_stateEcho.load();
		bool created = (st & (1ull << 63)) != 0
			&& ((st >> 32) & 0xFF) == 6
			&& (uint32)(st & 0xFFFFFFFF) == 1;
		if (created)
			reason = "created";
		else
		{
			reason = "no_result";
			failed = true;
		}
	}
	else
	{
		// PARTY_INSERT has no positive reply for the inviter; "sent" = no refusal reply ([A]).
		reason = "sent";
	}

	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"PartyInvite\""
			+ ",\"ok\":" + (failed ? "false" : "true")
			+ ",\"reason\":\"" + reason + "\""
			+ ",\"latency_us\":" + std::to_string(latencyUs);
		if ((e & (1ull << 63)) != 0)
			fields += ",\"code\":" + std::to_string(code);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_RESULT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	if (failed)
	{
		out.kind = PartyOutcome::FAILED;
		out.reason = reason;
	}
	else
	{
		out.kind = PartyOutcome::SENT;
		out.reason = reason;
	}
	out.peerId = target.id;
	return out;
}

PartyOutcome ActionExecutor::RequestPartyAccept(BotSession * s, std::chrono::steady_clock::time_point now)
{
	PartyOutcome out;
	out.kind = PartyOutcome::NOTHING;
	out.reason = "ok";
	out.peerId = -1;

	CUser * user = s != nullptr ? s->m_pUser : nullptr;
	if (user == nullptr || !user->isInGame())
	{
		out.kind = PartyOutcome::REFUSED;
		out.reason = "not_in_game";
		return out;
	}

	if (user->isDead())
	{
		out.kind = PartyOutcome::REFUSED;
		out.reason = "dead";
		return out;
	}

	// The pending invitation is the one OnPacket() recorded; none -> refuse without an event.
	uint64 inv = s->m_partyInviteEcho.load();
	if ((inv & (1ull << 63)) == 0)
	{
		out.kind = PartyOutcome::REFUSED;
		out.reason = "no_invite";
		return out;
	}
	int inviter = (int)(inv & 0xFFFF);

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()).count();
	uint64 invMs = s->m_partyInviteAtMs.load();
	uint32 sinceInviteMs = nowMs >= invMs ? (uint32)(nowMs - invMs) : 0;
	int inWindow = s->m_actionWindow.CountInWindow(nowMs);

	BotCore::PartyAcceptCheck c;
	c.sinceInviteMs = sinceInviteMs;
	c.actionsInWindow = inWindow;

	BotCore::PartyAcceptVerdict verdict = BotCore::CheckPartyAccept(c);
	if (verdict != BotCore::PARTYACCEPT_OK)
		return RejectPartyAccept(s, user, verdict, c, inviter);

	uint32 decisionId = NextDecisionId(s);
	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"PartyAccept\",\"inviter\":" + std::to_string(inviter);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_SUBMIT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	// Server reads u8 sub-opcode + u8 permit (PartyHandler.cpp:28-31).
	Packet pkt(WIZ_PARTY, uint8(PARTY_PERMIT));
	pkt << uint8(1);

	s->m_castSelfId = user->GetID();
	s->m_partyJoinEcho = 0;
	s->m_partyInviteEcho = 0;   // the invitation is consumed

	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	user->HandlePacket(pkt);
	std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	long long latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

	s->m_actionWindow.Record(nowMs);

	// The server sends the existing members first and the accepter's own member packet last, so the record is the
	// accepter's own packet (sid == its id, flag 1).
	uint64 j = s->m_partyJoinEcho.load();
	bool joined = (j & (1ull << 63)) != 0
		&& (int)((j >> 8) & 0xFFFF) == (int)user->GetID()
		&& (uint8)(j & 0xFF) == 1;

	if (Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		std::string fields = "\"decision_id\":" + std::to_string(decisionId)
			+ ",\"type\":\"PartyAccept\""
			+ ",\"ok\":" + (joined ? "true" : "false")
			+ ",\"reason\":\"" + (joined ? "joined" : "no_result") + "\""
			+ ",\"latency_us\":" + std::to_string(latencyUs)
			+ ",\"inviter\":" + std::to_string(inviter);
		Telemetry::Instance().Emit(TEL_DECISIONS, "ACTION_RESULT", user->GetSocketID(),
			s->m_charName.c_str(), fields, false);
	}

	if (joined)
	{
		out.kind = PartyOutcome::SENT;
		out.reason = "joined";
	}
	else
	{
		out.kind = PartyOutcome::FAILED;
		out.reason = "no_result";
	}
	out.peerId = inviter;
	return out;
}
