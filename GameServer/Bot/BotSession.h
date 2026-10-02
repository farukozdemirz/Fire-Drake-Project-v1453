#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include "IBotSink.h"
#include "../../BotCore/BotCombat.h"

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

	enum CastPhase { CAST_IDLE = 0, CAST_ARMED = 1, CAST_CASTING = 2 };

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

	bool m_moveActive;                                     // IOCP thread only: a walk is in progress (ActionExecutor)
	float m_moveTargetX;                                   // IOCP thread only
	float m_moveTargetZ;                                   // IOCP thread only
	int16 m_moveSpeed;                                     // IOCP thread only: speed field of the walk (packet speed while walking)
	std::chrono::steady_clock::time_point m_moveLastSent;  // IOCP thread only: when the last WIZ_MOVE went out
	uint32 m_actionSeq;                                    // IOCP thread only: per-spawn counter used as decision_id
	uint32 m_movePackets;                                  // IOCP thread only: WIZ_MOVE packets sent in the current walk

	bool m_attackActive;                                   // IOCP thread only: an attack series is in progress
	std::string m_attackTargetName;                        // IOCP thread only: character name of the target bot
	uint32 m_attackLeft;                                   // IOCP thread only: hits still to send in this series
	bool m_attackHasLast;                                  // IOCP thread only: m_attackLastSent is valid for this series
	std::chrono::steady_clock::time_point m_attackLastSent;// IOCP thread only: when the last WIZ_ATTACK went out
	uint32 m_attackSent;                                   // IOCP thread only: WIZ_ATTACK packets sent in this series
	uint32 m_attackHits;                                   // IOCP thread only: of those, results hit/killed
	BotCore::ActionRateWindow m_actionWindow;              // IOCP thread only: CLI-11 window (non-move actions)

	uint8 m_castPhase;                                     // IOCP thread only: CastPhase
	uint32 m_castSkillId;                                  // IOCP thread only
	std::string m_castTargetName;                          // IOCP thread only: target bot's character name; empty = self
	uint32 m_castLeft;                                     // IOCP thread only: cycles still to complete
	uint32 m_castCycle;                                    // IOCP thread only: cycles started in this series (1-based in telemetry)
	uint32 m_castDone;                                     // IOCP thread only: cycles whose EFFECTING result was effected/missed
	uint32 m_castPackets;                                  // IOCP thread only: WIZ_MAGIC_PROCESS packets sent in this series
	std::chrono::steady_clock::time_point m_castCastingAt; // IOCP thread only: when CASTING went out (phase CAST_CASTING)
	std::map<uint32, std::chrono::steady_clock::time_point> m_castSkillLast;   // IOCP thread only: skill id -> last EFFECTING sent
	bool m_castTypeHas[8];                                 // IOCP thread only: per skill type 0..7
	std::chrono::steady_clock::time_point m_castTypeLast[8];   // IOCP thread only
	bool m_castAnyHas;                                     // IOCP thread only
	std::chrono::steady_clock::time_point m_castAnyLast;   // IOCP thread only: last EFFECTING of any skill

	std::atomic<int> m_selectResult;                       // SelectResult, set by OnPacket
	std::atomic<uint32> m_packetTotal;
	std::atomic<uint32> m_opcodeCount[256];
	std::atomic<uint64> m_attackEcho;                      // written by OnPacket() (same thread as HandlePacket for own hits)
	std::atomic<int> m_castSelfId;                         // set by ActionExecutor (IOCP thread), read by OnPacket(): own caster id, -1 = none
	std::atomic<uint64> m_castEcho;                        // written by OnPacket(): skill result packet, see BotSession.cpp
};
