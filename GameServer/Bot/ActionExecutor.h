#pragma once

#include <chrono>
#include "BotSession.h"

// One Move/Stop action's outcome. 'reason' is a constant string, never freed.
struct MoveOutcome
{
	enum Kind { NOTHING, SENT, ARRIVED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // "ok", "not_in_game", "dead", "bad_target", "speed_field",
	                       // "step_too_long", "handler_noop"
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
};
