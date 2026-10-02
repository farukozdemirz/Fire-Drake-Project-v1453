#include "stdafx.h"
#include "BotSession.h"

BotSession::BotSession(const char * charName, const char * accountName)
	: m_charName(charName), m_accountName(accountName), m_pUser(nullptr),
		m_phase(PHASE_QUEUED), m_selectSeen(false), m_updateCount(0), m_slotId(0),
		m_despawnCount(0), m_moveActive(false), m_moveTargetX(0), m_moveTargetZ(0),
		m_moveSpeed(0), m_actionSeq(0), m_movePackets(0),
		m_attackActive(false), m_attackLeft(0), m_attackHasLast(false),
		m_attackSent(0), m_attackHits(0),
		m_selectResult(SELECT_PENDING), m_packetTotal(0), m_attackEcho(0)
{
	for (int i = 0; i < 256; i++)
		m_opcodeCount[i] = 0;
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
	m_selectResult = SELECT_PENDING;
	m_packetTotal = 0;
	m_attackEcho = 0;
	for (int i = 0; i < 256; i++)
		m_opcodeCount[i] = 0;
}
