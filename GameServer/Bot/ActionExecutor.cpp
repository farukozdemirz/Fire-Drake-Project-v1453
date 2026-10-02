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
