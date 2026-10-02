#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include "IBotSink.h"

class CUser;

// One bot character's login state plus the receiver for the packets its CUser would send
// to a client. OnPacket() may run on the DB thread, the IOCP thread or the 30 s timer
// thread, so it touches atomics only. Every other member is IOCP thread only.
class BotSession : public IBotSink
{
public:
	enum Phase
	{
		PHASE_QUEUED,       // waiting for its turn (one spawn per tick)
		PHASE_WAIT_SELECT,  // WIZ_SEL_CHAR request queued, waiting for the DB thread's reply
		PHASE_WAIT_LOADED,  // GameStart(1) done, waiting to send GameStart(2)
		PHASE_IN_GAME,
		PHASE_FAILED,
		PHASE_DESPAWN_WAIT,   // OnDisconnect() called, waiting for the DB thread to finish the logout save
		PHASE_DESPAWNED,      // slot returned to the pool, m_pUser == nullptr
		PHASE_DESPAWN_STUCK   // logout save not confirmed in time; slot deliberately kept
	};

	enum SelectResult { SELECT_PENDING = 0, SELECT_OK = 1, SELECT_FAILED = 2 };

	BotSession(const char * charName, const char * accountName);

	// Any thread. Counts the packet; WIZ_SEL_CHAR also records the select result.
	virtual void OnPacket(Packet & pkt);

	// IOCP thread only. Call after the slot was returned to the pool (m_pUser already null):
	// puts the session back into PHASE_QUEUED with fresh per-spawn counters.
	void ResetForRespawn();

	const std::string m_charName;
	const std::string m_accountName;

	CUser * m_pUser;                                       // IOCP thread only
	Phase m_phase;                                         // IOCP thread only
	std::chrono::steady_clock::time_point m_phaseStart;    // IOCP thread only
	bool m_selectSeen;                                     // IOCP thread only
	std::chrono::steady_clock::time_point m_selectSeenAt;  // IOCP thread only
	std::chrono::steady_clock::time_point m_inGameSince;   // IOCP thread only
	std::chrono::steady_clock::time_point m_lastUpdate;    // IOCP thread only
	std::chrono::steady_clock::time_point m_despawnStart;  // IOCP thread only
	uint32 m_updateCount;                                  // IOCP thread only
	uint16 m_slotId;                                       // IOCP thread only, kept for log lines after m_pUser is cleared
	uint32 m_despawnCount;                                 // IOCP thread only, completed despawns (slot returned)

	std::atomic<int> m_selectResult;                       // SelectResult, set by OnPacket
	std::atomic<uint32> m_packetTotal;
	std::atomic<uint32> m_opcodeCount[256];
};
