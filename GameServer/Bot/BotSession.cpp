#include "stdafx.h"
#include "BotSession.h"

BotSession::BotSession(const char * charName, const char * accountName)
	: m_charName(charName), m_accountName(accountName), m_pUser(nullptr),
		m_phase(PHASE_QUEUED), m_selectSeen(false), m_updateCount(0), m_slotId(0),
		m_despawnCount(0), m_moveActive(false), m_moveTargetX(0), m_moveTargetZ(0),
		m_moveSpeed(0), m_actionSeq(0), m_movePackets(0),
		m_attackActive(false), m_attackLeft(0), m_attackHasLast(false),
		m_attackSent(0), m_attackHits(0),
		m_castPhase(CAST_IDLE), m_castSkillId(0), m_castLeft(0), m_castCycle(0),
		m_castDone(0), m_castPackets(0), m_castAnyHas(false),
		m_potActive(false), m_potItemId(0), m_potSkillId(0), m_potKind(0),
		m_potLeft(0), m_potSent(0), m_potOk(0), m_potHasLast(false),
		m_stanceHasLast(false),
		m_hpReqTargetId(-1), m_hpReqHasLast(false), m_deadSeen(false),
		m_partyInviteHasLast(false),
		m_selectResult(SELECT_PENDING), m_packetTotal(0), m_attackEcho(0),
		m_castSelfId(-1), m_castEcho(0), m_stateEcho(0),
		m_targetHpEcho(0), m_targetHpValues(0), m_regeneEcho(0),
		m_partyInviteAtMs(0), m_partyInviteEcho(0), m_partyErrorEcho(0), m_partyJoinEcho(0)
{
	for (int i = 0; i < 256; i++)
		m_opcodeCount[i] = 0;
	for (int i = 0; i < 8; i++)
		m_castTypeHas[i] = false;
}

void BotSession::OnPacket(Packet & pkt)
{
	uint8 opcode = pkt.GetOpcode();
	m_packetTotal++;
	m_opcodeCount[opcode]++;

	// SelectCharacter()'s reply: first payload byte is bResult (0 = failed).
	if (opcode == WIZ_SEL_CHAR)
		m_selectResult = (pkt.read<uint8>(0) != 0) ? SELECT_OK : SELECT_FAILED;

	// Attack result broadcast: u8 bType, u8 bResult, i16 attackerId, i16 tid (AttackHandler.cpp:95-96).
	if (opcode == WIZ_ATTACK && pkt.size() >= 6)
	{
		uint8 result = pkt.read<uint8>(1);
		int16 attackerId = pkt.read<int16>(2);
		int16 tid = pkt.read<int16>(4);
		m_attackEcho = (1ull << 63) | (uint64(attackerId) << 32) | (uint64(tid) << 16) | uint64(result);
	}

	// Skill result broadcast: u8 opcode, u32 skill, i16 caster, i16 target, i16 sData[0..6]
	// (MagicInstance.cpp:730-760). Only the caster's own packets are recorded; m_castSelfId is
	// written on the IOCP thread before HandlePacket() runs on the same thread.
	if (opcode == WIZ_MAGIC_PROCESS && pkt.size() >= 17)
	{
		uint8 op = pkt.read<uint8>(0);
		uint32 skillId = pkt.read<uint32>(1);
		int16 caster = pkt.read<int16>(5);
		int16 sData3 = pkt.read<int16>(15);
		if (op >= 1 && op <= 4 && caster == m_castSelfId.load())
		{
			m_castEcho = (1ull << 63) | (uint64(op & 0xF) << 48)
				| (uint64(uint16(sData3)) << 32) | uint64(skillId);
		}
	}

	// State change broadcast: u16 socket id, u8 bType, u32 nBuff (User.cpp:2817-2819). Only the bot's own packet is recorded;
	// m_castSelfId (own id) is written on the IOCP thread before HandlePacket() runs on the same thread.
	if (opcode == WIZ_STATE_CHANGE && pkt.size() >= 7)
	{
		uint16 sid = pkt.read<uint16>(0);
		uint8 bType = pkt.read<uint8>(2);
		uint32 nBuff = pkt.read<uint32>(3);
		if ((int)sid == m_castSelfId.load())
			m_stateEcho = (1ull << 63) | (uint64(bType) << 32) | uint64(nBuff);
	}

	// Target HP reply: u16 tid, u8 echo, i32 maxHp, i32 hp, u16 damage (User.cpp:2384-2386). Every WIZ_TARGET_HP the bot
	// receives is recorded (request replies and attacker-side damage notices alike); ActionExecutor::RequestTargetHp
	// clears the record before its request and matches tid + echo afterwards, on the same thread.
	// The values word is written first so a reader that sees the valid bit also sees the values.
	if (opcode == WIZ_TARGET_HP && pkt.size() >= 13)
	{
		uint16 tid = pkt.read<uint16>(0);
		uint8 echo = pkt.read<uint8>(2);
		int32 maxHp = pkt.read<int32>(3);
		int32 hp = pkt.read<int32>(7);
		m_targetHpValues = (uint64(uint32(hp)) << 32) | uint64(uint32(maxHp));
		m_targetHpEcho = (1ull << 63) | (uint64(echo) << 16) | uint64(tid);
	}

	// Respawn reply: u16 x*10, u16 z*10, u16 y*10 (AttackHandler.cpp:196-198). ActionExecutor::RequestRegene clears the
	// record before its request and reads it afterwards, on the same thread.
	if (opcode == WIZ_REGENE && pkt.size() >= 6)
	{
		uint16 x = pkt.read<uint16>(0);
		uint16 z = pkt.read<uint16>(2);
		uint16 y = pkt.read<uint16>(4);
		m_regeneEcho = (1ull << 63) | (uint64(x) << 32) | (uint64(z) << 16) | uint64(y);
	}

	// Party packets (PartyHandler.cpp): u8 sub-opcode first. PARTY_PERMIT (2) = an invitation arrived: u16 inviter sid + name.
	// PARTY_INSERT (3) with a 3-byte payload = the bot's own invitation was refused: i16 error code. PARTY_INSERT with
	// a longer payload = a member joined: u16 sid, u8 flag (1 = success, 100 = leader moved), name, ...
	// ActionExecutor::RequestPartyInvite / RequestPartyAccept clear the records before their request and read them
	// afterwards, on the same thread.
	if (opcode == WIZ_PARTY && pkt.size() >= 1)
	{
		uint8 sub = pkt.read<uint8>(0);
		if (sub == PARTY_PERMIT && pkt.size() >= 5)
		{
			uint16 sid = pkt.read<uint16>(1);
			uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
			m_partyInviteAtMs = nowMs;
			m_partyInviteEcho = (1ull << 63) | uint64(sid);
		}
		else if (sub == PARTY_INSERT && pkt.size() == 3)
		{
			int16 code = pkt.read<int16>(1);
			m_partyErrorEcho = (1ull << 63) | uint64(uint16(code));
		}
		else if (sub == PARTY_INSERT && pkt.size() >= 4)
		{
			uint16 sid = pkt.read<uint16>(1);
			uint8 flag = pkt.read<uint8>(3);
			m_partyJoinEcho = (1ull << 63) | (uint64(sid) << 8) | uint64(flag);
		}
	}
}

void BotSession::ResetForRespawn()
{
	m_pUser = nullptr;
	m_phase = PHASE_QUEUED;
	m_selectSeen = false;
	m_updateCount = 0;
	m_moveActive = false;
	m_moveTargetX = 0;
	m_moveTargetZ = 0;
	m_moveSpeed = 0;
	m_actionSeq = 0;
	m_movePackets = 0;
	m_attackActive = false;
	m_attackTargetName.clear();
	m_attackLeft = 0;
	m_attackHasLast = false;
	m_attackSent = 0;
	m_attackHits = 0;
	m_actionWindow.Clear();
	m_castPhase = CAST_IDLE;
	m_castSkillId = 0;
	m_castTargetName.clear();
	m_castLeft = 0;
	m_castCycle = 0;
	m_castDone = 0;
	m_castPackets = 0;
	m_castSkillLast.clear();
	for (int i = 0; i < 8; i++)
		m_castTypeHas[i] = false;
	m_castAnyHas = false;
	m_potActive = false;
	m_potItemId = 0;
	m_potSkillId = 0;
	m_potKind = 0;
	m_potLeft = 0;
	m_potSent = 0;
	m_potOk = 0;
	m_potHasLast = false;
	m_stanceHasLast = false;
	m_hpReqTargetId = -1;
	m_hpReqHasLast = false;
	m_deadSeen = false;
	m_targetHpEcho = 0;
	m_targetHpValues = 0;
	m_regeneEcho = 0;
	m_partyInviteHasLast = false;
	m_partyInviteAtMs = 0;
	m_partyInviteEcho = 0;
	m_partyErrorEcho = 0;
	m_partyJoinEcho = 0;
	m_selectResult = SELECT_PENDING;
	m_packetTotal = 0;
	m_attackEcho = 0;
	m_castSelfId = -1;
	m_castEcho = 0;
	m_stateEcho = 0;
	for (int i = 0; i < 256; i++)
		m_opcodeCount[i] = 0;
}
