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
		m_selectResult(SELECT_PENDING), m_packetTotal(0), m_attackEcho(0),
		m_castSelfId(-1), m_castEcho(0)
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
	m_selectResult = SELECT_PENDING;
	m_packetTotal = 0;
	m_attackEcho = 0;
	m_castSelfId = -1;
	m_castEcho = 0;
	for (int i = 0; i < 256; i++)
		m_opcodeCount[i] = 0;
}
