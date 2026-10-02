#pragma once

#include <chrono>
#include <string>
#include "BotSession.h"

// One Move/Stop action's outcome. 'reason' is a constant string, never freed.
struct MoveOutcome
{
	enum Kind { NOTHING, SENT, ARRIVED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // "ok", "not_in_game", "dead", "bad_target", "speed_field",
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
	                       // "not_in_game", "dead", "bad_target", "out_of_range", "too_soon", "rate"
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
};
