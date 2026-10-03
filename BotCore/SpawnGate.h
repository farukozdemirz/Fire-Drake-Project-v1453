#pragma once
#include <cstdint>

namespace BotCore
{
	// A spawning session may send GameStart(1) only while no other session sits between its own
	// GameStart(1) and GameStart(2) (plan F4-55). Pure rule; BotManager counts the sessions.
	inline bool HandshakeGateOpen(int sessionsInHandshake)
	{
		return sessionsInHandshake <= 0;
	}

	// Server model of the two-step login (CharacterSelectionHandler.cpp GameStart(1)/(2)):
	// unit X does NOT learn about unit Y iff Y registered after X took its region snapshot and
	// before X was in game (X then misses Y's broadcast): op1(X) < op2(Y) < op2(X).
	inline bool HandshakeMissesUnit(uint64_t op1X, uint64_t op2X, uint64_t op2Y)
	{
		return op1X < op2Y && op2Y < op2X;
	}
}
