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

	bool m_potActive;                                      // IOCP thread only: a pot series is in progress
	uint32 m_potItemId;                                    // IOCP thread only: ITEM.Num of the pot
	uint32 m_potSkillId;                                   // IOCP thread only: its ITEM.Effect1 skill
	uint8 m_potKind;                                       // IOCP thread only: 1 = HP (DirectType 1), 2 = MP (DirectType 2)
	uint32 m_potLeft;                                      // IOCP thread only: pots still to drink in this series
	uint32 m_potSent;                                      // IOCP thread only: pot packets sent in this series
	uint32 m_potOk;                                        // IOCP thread only: of those, result "effected"
	bool m_potHasLast;                                     // IOCP thread only: m_potLast is valid for this spawn (shared timer)
	std::chrono::steady_clock::time_point m_potLast;       // IOCP thread only: when the last pot packet went out (any pot)

	bool m_stanceHasLast;                                  // IOCP thread only: m_stanceLast is valid for this spawn
	std::chrono::steady_clock::time_point m_stanceLast;    // IOCP thread only: when the last stance packet went out

	int m_hpReqTargetId;                                   // IOCP thread only: id of the selected target (last WIZ_TARGET_HP request), -1 = none
	bool m_hpReqHasLast;                                   // IOCP thread only: m_hpReqLast is valid for this spawn
	std::chrono::steady_clock::time_point m_hpReqLast;     // IOCP thread only: when the last WIZ_TARGET_HP request went out

	bool m_deadSeen;                                       // IOCP thread only: the bot was seen dead and m_deadSince is valid
	std::chrono::steady_clock::time_point m_deadSince;     // IOCP thread only: when its death was first noticed (TickSessions or the first regene request)

	bool m_partyInviteHasLast;                             // IOCP thread only: m_partyInviteLast is valid for this spawn
	std::chrono::steady_clock::time_point m_partyInviteLast;   // IOCP thread only: when the last party invitation went out

	std::atomic<int> m_selectResult;                       // SelectResult, set by OnPacket
	std::atomic<uint32> m_packetTotal;
	std::atomic<uint32> m_opcodeCount[256];
	std::atomic<uint64> m_attackEcho;                      // written by OnPacket() (same thread as HandlePacket for own hits)
	std::atomic<int> m_castSelfId;                         // set by ActionExecutor (IOCP thread), read by OnPacket(): own caster id, -1 = none
	std::atomic<uint64> m_castEcho;                        // written by OnPacket(): skill result packet, see BotSession.cpp
	std::atomic<uint64> m_stateEcho;                       // written by OnPacket(): own WIZ_STATE_CHANGE broadcast, see BotSession.cpp
	std::atomic<uint64> m_targetHpEcho;                    // written by OnPacket(): valid bit | echo << 16 | tid of the last WIZ_TARGET_HP reply
	std::atomic<uint64> m_targetHpValues;                  // written by OnPacket() BEFORE m_targetHpEcho: hp << 32 | maxHp
	std::atomic<uint64> m_regeneEcho;                      // written by OnPacket(): valid bit | x << 32 | z << 16 | y (all x10) of the last WIZ_REGENE reply
	std::atomic<uint64> m_partyInviteAtMs;                 // written by OnPacket() BEFORE m_partyInviteEcho: steady_clock ms when the invitation arrived
	std::atomic<uint64> m_partyInviteEcho;                 // written by OnPacket(): valid bit | inviter sid of the last PARTY_PERMIT; cleared by PartyAccept
	std::atomic<uint64> m_partyErrorEcho;                  // written by OnPacket(): valid bit | uint16(error code) of the last PARTY_INSERT refusal (payload of 3 bytes)
	std::atomic<uint64> m_partyJoinEcho;                   // written by OnPacket(): valid bit | sid << 8 | flag of the last PARTY_INSERT member packet
};
