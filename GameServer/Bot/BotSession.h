#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include "IBotSink.h"
#include "../../BotCore/BotCombat.h"
#include "../../BotCore/Perception.h"

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

	enum CastPhase { CAST_IDLE = 0, CAST_ARMED = 1, CAST_CASTING = 2, CAST_FLYING = 3 };

	BotSession(const char * charName, const char * accountName);

	// Any thread. Counts the packet; WIZ_SEL_CHAR also records the select result.
	virtual void OnPacket(Packet & pkt);

	// IOCP thread only. Call after the slot was returned to the pool (m_pUser already null):
	// puts the session back into PHASE_QUEUED with fresh per-spawn counters.
	void ResetForRespawn();

	// IOCP thread only. Copies up to 'cap' ids the table still does not know (selfSid and ids learned meanwhile are
	// dropped) without removing them.
	int PeekUserInBatch(uint16 selfSid, uint16 * out, int cap);

	// IOCP thread only. Removes the ids from the pending list (call after the request went out).
	void DropUserInBatch(const uint16 * ids, int n);

	// IOCP thread only. Copies up to 'cap' NPC ids the NPC table still does not know (ids learned meanwhile are dropped) without removing them.
	int PeekNpcInBatch(uint16 * out, int cap);

	// IOCP thread only. Removes the ids from the NPC pending list (call after the request went out).
	void DropNpcInBatch(const uint16 * ids, int n);

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
	int16 m_castTargetId;                                  // IOCP thread only: target id sent with CASTING (the cancel packet carries it)
	std::chrono::steady_clock::time_point m_castCastingAt; // IOCP thread only: when CASTING went out (phase CAST_CASTING)
	std::chrono::steady_clock::time_point m_castFlyingAt;   // IOCP thread only: when FLYING went out (phase CAST_FLYING)
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

	bool m_partyEnteredHasAt;                              // IOCP thread only: m_partyEnteredAt is valid (the bot created or joined a party in this spawn)
	std::chrono::steady_clock::time_point m_partyEnteredAt;    // IOCP thread only: when the bot last created or joined a party

	bool m_partyManageHasLast;                             // IOCP thread only: m_partyManageLast is valid for this spawn
	std::chrono::steady_clock::time_point m_partyManageLast;   // IOCP thread only: when the last party promote / kick went out

	bool m_chatHasLast;                                    // IOCP thread only: m_chatLast and m_chatLastHash are valid for this spawn
	std::chrono::steady_clock::time_point m_chatLast;      // IOCP thread only: when the last party chat message went out
	uint32 m_chatLastHash;                                 // IOCP thread only: BotCore::ChatTextHash of that message
	BotCore::ChatRateWindow m_chatWindow;                  // IOCP thread only: CLI-18 per-minute window

	bool m_userInHasLast;                                  // IOCP thread only: m_userInLast is valid for this spawn
	std::chrono::steady_clock::time_point m_userInLast;    // IOCP thread only: when the last WIZ_REQ_USERIN request went out
	uint32 m_userInRequests;                               // IOCP thread only: WIZ_REQ_USERIN requests sent in this spawn
	uint32 m_userInUnits;                                  // IOCP thread only: total units carried by the replies in this spawn

	bool m_npcInHasLast;                                   // IOCP thread only: m_npcInLast is valid for this spawn
	std::chrono::steady_clock::time_point m_npcInLast;     // IOCP thread only: when the last WIZ_REQ_NPCIN request went out
	uint32 m_npcInRequests;                                // IOCP thread only: WIZ_REQ_NPCIN requests sent in this spawn
	uint32 m_npcInUnits;                                   // IOCP thread only: total NPCs carried by the replies in this spawn

	std::mutex m_obsLock;                                  // guards m_obs and m_obsPending: OnPacket() may run on any thread
	BotCore::ObsTable m_obs;                               // guarded by m_obsLock: players in view, from received packets only (Perception, ADR-0017 Ek F4-12)
	BotCore::PendingIds m_obsPending;                      // guarded by m_obsLock: ids of the last WIZ_REGIONCHANGE the table did not know (Perception, ADR-0017 Ek F4-13)
	BotCore::NpcTable m_npcs;                              // guarded by m_obsLock (the same mutex as m_obs): NPCs in view, from received packets only (Perception, ADR-0017 Ek F4-14)
	BotCore::PendingIds m_npcPending;                      // guarded by m_obsLock: ids of the last WIZ_NPC_REGION the NPC table did not know (Perception, ADR-0017 Ek F4-15)
	BotCore::TeamTable m_team;                             // guarded by m_obsLock (the same mutex as m_obs): the bot's party members, from received WIZ_PARTY packets only (Perception, ADR-0017 Ek F4-18)
	BotCore::HpTable m_hp;                                 // guarded by m_obsLock (the same mutex as m_obs): HP observations from received WIZ_TARGET_HP packets only (Perception, ADR-0017 Ek F4-51)
	BotCore::SkillEventRing m_skillEvents;                 // guarded by m_obsLock (the same mutex as m_obs): received WIZ_MAGIC_PROCESS broadcasts of any caster (Perception, ADR-0017 Ek F4-52)

	std::atomic<int> m_selectResult;                       // SelectResult, set by OnPacket
	std::atomic<uint32> m_packetTotal;
	std::atomic<uint32> m_opcodeCount[256];
	std::atomic<uint64> m_attackEcho;                      // written by OnPacket() (same thread as HandlePacket for own hits)
	std::atomic<int> m_selfSid;                            // written by BotManager::TickSessions() (IOCP thread) when the bot enters the game, read by OnPacket(): own socket id, -1 = none
	std::atomic<int> m_castSelfId;                         // set by ActionExecutor (IOCP thread), read by OnPacket(): own caster id, -1 = none
	std::atomic<uint64> m_castEcho;                        // written by OnPacket(): skill result packet, see BotSession.cpp
	std::atomic<uint32> m_castEchoVictims;                 // written by OnPacket(): own EFFECTING packets with a real target id since the last reset by SubmitCast()
	std::atomic<uint64> m_stateEcho;                       // written by OnPacket(): own WIZ_STATE_CHANGE broadcast, see BotSession.cpp
	std::atomic<uint64> m_targetHpEcho;                    // written by OnPacket(): valid bit | echo << 16 | tid of the last WIZ_TARGET_HP reply
	std::atomic<uint64> m_targetHpValues;                  // written by OnPacket() BEFORE m_targetHpEcho: hp << 32 | maxHp
	std::atomic<uint64> m_regeneEcho;                      // written by OnPacket(): valid bit | x << 32 | z << 16 | y (all x10) of the last WIZ_REGENE reply
	std::atomic<uint64> m_partyInviteAtMs;                 // written by OnPacket() BEFORE m_partyInviteEcho: steady_clock ms when the invitation arrived
	std::atomic<uint64> m_partyInviteEcho;                 // written by OnPacket(): valid bit | inviter sid of the last PARTY_PERMIT; cleared by PartyAccept
	std::atomic<uint64> m_partyErrorEcho;                  // written by OnPacket(): valid bit | uint16(error code) of the last PARTY_INSERT refusal (payload of 3 bytes)
	std::atomic<uint64> m_partyJoinEcho;                   // written by OnPacket(): valid bit | sid << 8 | flag of the last PARTY_INSERT member packet
	std::atomic<uint64> m_partyLeaveEcho;                  // written by OnPacket(): valid bit | kind << 16 | sid of the last PARTY_REMOVE (kind 1, sid = the removed member) or PARTY_DELETE (kind 2, sid 0)
	std::atomic<uint32> m_chatEchoHash;                    // written by OnPacket() BEFORE m_chatEcho: BotCore::ChatTextHash of the last WIZ_CHAT message received (0 when longer than kChatMaxLen)
	std::atomic<uint64> m_chatEcho;                        // written by OnPacket(): valid bit | chat type << 32 | uint16 sender sid of the last WIZ_CHAT received
	std::atomic<uint32> m_obsUnresolved;                   // written by OnPacket(): ids of the last WIZ_REGIONCHANGE that were not in m_obs, the bot itself included
	std::atomic<uint64> m_userInEcho;                      // written by OnPacket(): valid bit (63) | number of units parsed from the last WIZ_REQ_USERIN reply
	std::atomic<uint32> m_npcUnresolved;                   // written by OnPacket(): ids of the last WIZ_NPC_REGION that were not in m_npcs
	std::atomic<uint64> m_npcInEcho;                       // written by OnPacket(): valid bit (63) | number of NPCs parsed from the last WIZ_REQ_NPCIN reply

	// Diagnostic counters for the one-way view investigation (plan F4-54, KI-DEG-01). They are written by
	// OnPacket() only (any thread) and read by CommandSee on the IOCP thread; no behavior depends on them.
	std::atomic<uint32> m_inoutIn;                         // WIZ_USER_INOUT entries parsed as present (in)
	std::atomic<uint32> m_inoutOut;                        // WIZ_USER_INOUT entries parsed as gone (out)
	std::atomic<uint32> m_inoutParseFail;                  // WIZ_USER_INOUT packets the parser rejected
	std::atomic<uint32> m_reqUserInRecv;                   // WIZ_REQ_USERIN replies received
	std::atomic<uint32> m_reqUserInUnits;                  // units parsed from all WIZ_REQ_USERIN replies
	std::atomic<uint32> m_reqUserInParseStop;              // WIZ_REQ_USERIN replies whose declared count exceeded the parsed units
	std::atomic<uint32> m_regionRecv;                      // WIZ_REGIONCHANGE packets received
	std::atomic<uint32> m_regionIdsLast;                   // ids in the last WIZ_REGIONCHANGE list
	std::atomic<uint32> m_regionDroppedTotal;              // units dropped by Retain across all WIZ_REGIONCHANGE packets
	std::atomic<uint32> m_moveUnknown;                     // WIZ_MOVE for an id the table did not know
	uint16 m_regionDroppedLastIds[8];                      // guarded by m_obsLock: ids dropped by the last Retain, at most 8
	int m_regionDroppedLastCount;                          // guarded by m_obsLock: valid entries in m_regionDroppedLastIds
};
