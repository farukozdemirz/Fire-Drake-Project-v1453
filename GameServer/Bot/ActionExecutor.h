#pragma once

#include <chrono>
#include <string>
#include "BotSession.h"

// One Move/Stop action's outcome. 'reason' is a constant string, never freed.
struct MoveOutcome
{
	enum Kind { NOTHING, SENT, ARRIVED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // "ok", "not_in_game", "dead", "sitting", "bad_target", "speed_field",
	                       // "step_too_long", "handler_noop"
};

// Caller-supplied view of the target (ADR-0017 Ek F4-02). Temporary: the /bot attack test driver fills it
// from the target bot's session; the Perception slice replaces the source, not this struct.
struct AttackTarget
{
	int16 id;      // target's socket id (WIZ_ATTACK 'tid')
	float x;       // target position, metres
	float z;
};

struct AttackOutcome
{
	enum Kind { NOTHING, SENT, FINISHED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "ok", "hit", "killed", "srv_fail", "no_result",
	                       // "not_in_game", "dead", "sitting", "bad_target", "out_of_range", "too_soon", "rate"
};

// Caller-supplied view of the cast target (ADR-0017 Ek F4-03). Temporary, like AttackTarget: the /bot cast test
// driver fills it from the target bot's session (or from the caster itself for "self").
struct CastTarget
{
	int16 id;      // target's id (WIZ_MAGIC_PROCESS 'target')
	float x;       // metres
	float y;
	float z;
	bool isSelf;
};

struct CastOutcome
{
	enum Kind { NOTHING, SENT, FINISHED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "ok", "casting", "effected", "missed", "srv_fail", "no_result",
	                       // "not_in_game", "dead", "sitting", "bad_skill", "unsupported_skill", "bad_target",
	                       // "out_of_range", "not_standing", "no_mana", "recast", "type_gate", "gap", "rate", "too_early"
};

struct PotionOutcome
{
	enum Kind { NOTHING, SENT, FINISHED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "ok", "effected", "srv_fail", "no_result",
	                       // "not_in_game", "dead", "bad_item", "unsupported_item", "no_stock", "pot_cooldown", "rate"
};

struct StanceOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "applied" (SENT), "no_result" (FAILED),
	                       // REFUSED: "not_in_game", "dead", "no_change", "busy", "toggle", "rate"
};

// Caller-supplied view of the HP request target (ADR-0017 Ek F4-06). Temporary, like AttackTarget: the /bot target
// test driver fills it from the target bot's session; the Perception slice replaces the source, not this struct.
struct TargetHpTarget
{
	int16 id;      // target's socket id (WIZ_TARGET_HP 'uid')
	float x;       // metres
	float z;
};

struct TargetHpOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "observed" (SENT), "no_result" (FAILED),
	                       // REFUSED: "not_in_game", "dead", "bad_target", "out_of_view", "poll", "rate"
	int32 hp;              // from the server's reply; valid only when kind == SENT
	int32 maxHp;
};

// Turns Move/Stop intents into real WIZ_MOVE packets and runs them through CUser::HandlePacket()
// (ADR-0017). IOCP thread only. No logging, no locking, no console output.
class ActionExecutor
{
public:
	// Validates and arms a walk to (tx,tz) at 'speedField'; sends nothing yet (the same Tick()'s TickMove() does).
	static MoveOutcome BeginMove(BotSession * s, float tx, float tz, int16 speedField,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session.
	static MoveOutcome TickMove(BotSession * s, std::chrono::steady_clock::time_point now);

	// Ends an active walk: sends a stop packet at the current position.
	static MoveOutcome StopMove(BotSession * s, std::chrono::steady_clock::time_point now);

	// Clears the walk state without sending anything (despawn).
	static void AbandonMove(BotSession * s);

	// Validates and arms an attack series of 'count' R hits on the target bot 'targetName'; sends nothing yet
	// (the same Tick()'s TickAttack() does). REFUSED (nothing armed) when the session is not in game / dead
	// ("not_in_game", "dead") or count < 1 / targetName empty ("bad_target").
	static AttackOutcome BeginAttack(BotSession * s, const std::string & targetName, uint32 count,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session; NOTHING unless s->m_attackActive and the CLI-01 interval
	// (or the series start) allows a hit. Sends at most one WIZ_ATTACK. 'target' is the caller's current view.
	// SENT: hit sent, series continues. FINISHED: last hit sent or target killed (reason "ok"/"hit"/"killed").
	// REFUSED: guard rejected, series dropped. FAILED: handler produced no result / dead attacker, series dropped.
	static AttackOutcome TickAttack(BotSession * s, const AttackTarget & target,
		std::chrono::steady_clock::time_point now);

	// Clears the attack state without sending anything (stop, despawn, target lost).
	static void EndAttack(BotSession * s);

	// Validates and arms a cast series of 'count' cycles of skill 'skillId'; sends nothing yet (the same Tick()'s
	// TickCast() does). 'targetName' empty = self. REFUSED (nothing armed): "not_in_game", "dead",
	// "bad_skill" (unknown id, other class, level too low, count < 1), "unsupported_skill" (see 5.4 rules),
	// "bad_target" (moral does not match the target kind).
	static CastOutcome BeginCast(BotSession * s, uint32 skillId, const std::string & targetName, uint32 count,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session; NOTHING unless s->m_castPhase != CAST_IDLE and the timing
	// rules (CastWaitMs / CastDurationMs) allow the next packet. Sends at most one WIZ_MAGIC_PROCESS.
	// SENT: a packet went out and the series continues ("casting" = CASTING accepted, "effected"/"missed"/"srv_fail" =
	// one cycle finished). FINISHED: last cycle done. REFUSED: guard rejected, series dropped. FAILED: handler produced
	// no result / dead caster / unknown skill, series dropped.
	static CastOutcome TickCast(BotSession * s, const CastTarget & target,
		std::chrono::steady_clock::time_point now);

	// Clears the cast state without sending anything (stop, despawn, target lost). Keeps the reuse timers.
	static void EndCast(BotSession * s);

	// Validates and arms a series of 'count' pots of ITEM 'itemId'; sends nothing yet (the same Tick()'s TickPotion()
	// does). REFUSED (nothing armed): "not_in_game", "dead", "bad_item" (count < 1, unknown item, no Effect1, level or
	// class does not match, unknown skill), "unsupported_item" (see 5.4 rules). The bag stock is NOT checked here:
	// the guard checks it before every packet, so the refusal is visible as FAIRNESS_REJECT (CLI-06).
	static PotionOutcome BeginPotion(BotSession * s, uint32 itemId, uint32 count,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session; NOTHING unless s->m_potActive and the shared pot timer allows
	// the next pot. Sends at most one WIZ_MAGIC_PROCESS (MAGIC_EFFECTING). SENT: pot sent, series continues.
	// FINISHED: last pot sent. REFUSED: guard rejected, series dropped. FAILED: server answered MAGIC_FAIL or
	// published nothing, series dropped.
	static PotionOutcome TickPotion(BotSession * s, std::chrono::steady_clock::time_point now);

	// Clears the pot series without sending anything (stop, despawn). Keeps the shared pot timer.
	static void EndPotion(BotSession * s);

	// One-shot stance change: sit down (sit = true) or stand up. Validates, runs the guard, sends one WIZ_STATE_CHANGE
	// (type 1) through CUser::HandlePacket() and maps the result from the broadcast the server published (m_stateEcho).
	// SENT "applied": the broadcast carried the requested stance. FAILED "no_result": no/other broadcast.
	// REFUSED: "not_in_game", "dead", "no_change" (already in that stance; no event) or a guard verdict ("busy", "toggle",
	// "rate"; FAIRNESS_REJECT written).
	static StanceOutcome SetStance(BotSession * s, bool sit, std::chrono::steady_clock::time_point now);

	// One-shot target selection + HP request: sends one WIZ_TARGET_HP through CUser::HandlePacket() after the guard
	// (CLI-10: target inside the bot's 3x3 regions, same target re-polled at most every 2 s; CLI-11) and maps the result
	// from the reply the server published (m_targetHpEcho / m_targetHpValues). SENT "observed": a reply for that target
	// arrived (hp / maxHp filled). FAILED "no_result": no reply (target dead or unknown to the server).
	// REFUSED: "not_in_game", "dead", "bad_target" (id < 0 or the bot itself) or a guard verdict ("out_of_view", "poll",
	// "rate"; FAIRNESS_REJECT written).
	static TargetHpOutcome RequestTargetHp(BotSession * s, const TargetHpTarget & target,
		std::chrono::steady_clock::time_point now);
};
