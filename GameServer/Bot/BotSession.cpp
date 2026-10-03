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
		m_castDone(0), m_castPackets(0), m_castTargetId(-1), m_castAnyHas(false),
		m_potActive(false), m_potItemId(0), m_potSkillId(0), m_potKind(0),
		m_potLeft(0), m_potSent(0), m_potOk(0), m_potHasLast(false),
		m_stanceHasLast(false),
		m_hpReqTargetId(-1), m_hpReqHasLast(false), m_deadSeen(false),
		m_partyInviteHasLast(false),
		m_partyEnteredHasAt(false),
		m_partyManageHasLast(false),
		m_chatHasLast(false), m_chatLastHash(0),
		m_userInHasLast(false), m_userInRequests(0), m_userInUnits(0),
		m_npcInHasLast(false), m_npcInRequests(0), m_npcInUnits(0),
		m_speedHasLast(false), m_speedChecks(0), m_speedWarps(0),
		m_selectResult(SELECT_PENDING), m_packetTotal(0), m_attackEcho(0),
		m_selfSid(-1),
		m_castSelfId(-1), m_castEcho(0), m_castEchoVictims(0), m_stateEcho(0),
		m_targetHpEcho(0), m_targetHpValues(0), m_regeneEcho(0),
		m_partyInviteAtMs(0), m_partyInviteEcho(0), m_partyErrorEcho(0), m_partyJoinEcho(0),
		m_partyLeaveEcho(0), m_chatEchoHash(0), m_chatEcho(0), m_obsUnresolved(0), m_userInEcho(0), m_npcUnresolved(0), m_npcInEcho(0), m_warpEcho(0)
{
	for (int i = 0; i < 256; i++)
		m_opcodeCount[i] = 0;
	for (int i = 0; i < 8; i++)
		m_castTypeHas[i] = false;
	for (int i = 0; i < 8; i++)
		m_regionDroppedLastIds[i] = 0;

	m_inoutIn = 0;
	m_inoutOut = 0;
	m_inoutParseFail = 0;
	m_reqUserInRecv = 0;
	m_reqUserInUnits = 0;
	m_reqUserInParseStop = 0;
	m_regionRecv = 0;
	m_regionIdsLast = 0;
	m_regionDroppedTotal = 0;
	m_moveUnknown = 0;
	m_regionDroppedLastCount = 0;
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
		int16 victimId = pkt.read<int16>(7);
		int16 sData3 = pkt.read<int16>(15);
		if (op >= 1 && op <= 4 && caster == m_castSelfId.load())
		{
			m_castEcho = (1ull << 63) | (uint64(op & 0xF) << 48)
				| (uint64(uint16(sData3)) << 32) | uint64(skillId);
			if (op == MAGIC_EFFECTING && victimId != -1)
				m_castEchoVictims++;
		}
	}

	// Perception skill events (ADR-0017 Ek F4-52): every WIZ_MAGIC_PROCESS broadcast the server sends to this session
	// is stored in the fixed-size ring, for any caster (players and NPCs alike). Parsed before the lock; the ring
	// lives under m_obsLock. The own-cast echo block above is unaffected.
	if (opcode == WIZ_MAGIC_PROCESS && pkt.size() >= 23)
	{
		uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
		BotCore::SkillEvent ev;
		if (BotCore::ParseSkillEvent(pkt.contents(), pkt.size(), nowMs, ev))
		{
			std::lock_guard<std::mutex> lock(m_obsLock);
			m_skillEvents.Add(ev);
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

	// Perception target HP (ADR-0017 Ek F4-51): every WIZ_TARGET_HP the bot receives is stored per id, beside the
	// action record above. Parsed before the lock; the table lives under m_obsLock. Nothing is read from any CUser.
	if (opcode == WIZ_TARGET_HP)
	{
		const uint8 * data = pkt.size() > 0 ? pkt.contents() : nullptr;
		size_t len = pkt.size();
		uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
		BotCore::TargetHpMsg msg;
		if (BotCore::ParseTargetHp(data, len, msg))
		{
			BotCore::HpObs obs;
			obs.id = msg.tid;
			obs.hp = msg.hp;
			obs.maxHp = msg.maxHp;
			obs.lastDamage = msg.damage;
			obs.atMs = nowMs;
			obs.reply = msg.echo != 0;
			std::lock_guard<std::mutex> lock(m_obsLock);
			m_hp.Upsert(obs);
		}
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
	// PARTY_REMOVE (4) = u16 sid of the member who left; PARTY_DELETE (5) = the party was disbanded (1 byte).
	// ActionExecutor::RequestPartyLeave clears m_partyLeaveEcho before its request and reads it afterwards, on the same thread.
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
		else if (sub == PARTY_REMOVE && pkt.size() >= 3)
		{
			uint16 sid = pkt.read<uint16>(1);
			m_partyLeaveEcho = (1ull << 63) | (1ull << 16) | uint64(sid);
		}
		else if (sub == PARTY_DELETE)
		{
			m_partyLeaveEcho = (1ull << 63) | (2ull << 16);
		}
	}

	// Party team table (ADR-0017 Ek F4-18): member records, HP/MP changes, removals and disbands the server sends to this
	// session. Layouts: PartyHandler.cpp:233-260, :315-326, :393-395, :433-434, User.cpp:2057-2065. Parsed before the lock;
	// a REMOVE of the bot's own id (m_selfSid) empties the table. Nothing is read from the server's party arrays.
	if (opcode == WIZ_PARTY)
	{
		uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
		BotCore::PartyEvent ev;
		if (BotCore::ParsePartyEvent(pkt.size() > 0 ? pkt.contents() : nullptr, pkt.size(), nowMs, ev))
		{
			int selfSid = m_selfSid.load();
			std::lock_guard<std::mutex> lock(m_obsLock);
			m_team.Apply(ev, selfSid >= 0 ? (uint16_t)selfSid : BotCore::kTeamNone);
		}
	}

	// Chat broadcast (ChatHandler.cpp:161): u8 type, u8 nation, i16 sender sid, u8-length name,
	// u16-length message. Every chat packet the bot receives is recorded (own party chat echo and other players' chat
	// alike); ActionExecutor::RequestChatParty clears the record before its request and matches type, sender and the
	// message hash afterwards, on the same thread. The hash word is written first so a reader that sees the valid bit
	// also sees the hash.
	if (opcode == WIZ_CHAT && pkt.size() >= 7)
	{
		uint8 type = pkt.read<uint8>(0);
		uint16 sid = pkt.read<uint16>(2);
		uint8 nameLen = pkt.read<uint8>(4);
		size_t msgLenPos = 5 + (size_t)nameLen;
		uint32 hash = 0;
		if (pkt.size() >= msgLenPos + 2)
		{
			uint16 msgLen = pkt.read<uint16>(msgLenPos);
			if (msgLen <= BotCore::kChatMaxLen && pkt.size() >= msgLenPos + 2 + (size_t)msgLen)
			{
				char text[BotCore::kChatMaxLen];
				for (uint16 i = 0; i < msgLen; i++)
					text[i] = (char)pkt.read<uint8>(msgLenPos + 2 + i);
				hash = BotCore::ChatTextHash(text, msgLen);
			}
		}
		m_chatEchoHash = hash;
		m_chatEcho = (1ull << 63) | (uint64(type) << 32) | uint64(sid);
	}

	// Perception (ADR-0017 Ek F4-12): what a client would learn about the other players in view. Only the packets the
	// server sends to this session are read; nothing is fetched from other sessions, regions or the map. The payload
	// starts at contents(); the opcode is stored separately. Parsing happens before the lock is taken.
	if (opcode == WIZ_USER_INOUT || opcode == WIZ_REQ_USERIN || opcode == WIZ_REGIONCHANGE
		|| opcode == WIZ_MOVE || opcode == WIZ_DEAD)
	{
		const uint8 * data = pkt.size() > 0 ? pkt.contents() : nullptr;
		size_t len = pkt.size();
		uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();

		if (opcode == WIZ_USER_INOUT)
		{
			uint16 type = 0;
			BotCore::UnitObs unit;
			if (BotCore::ParseUserInOut(data, len, nowMs, type, unit))
			{
				std::lock_guard<std::mutex> lock(m_obsLock);
				if (type == BotCore::kObsInOutOut)
				{
					m_inoutOut++;
					m_obs.Remove(unit.sid);
					m_hp.Invalidate(unit.sid);
				}
				else
				{
					m_inoutIn++;
					m_obs.Upsert(unit);
				}
			}
			else
			{
				m_inoutParseFail++;
			}
		}
		else if (opcode == WIZ_REQ_USERIN)
		{
			BotCore::UnitObs list[BotCore::kObsMaxUnits];
			int n = BotCore::ParseUserList(data, len, nowMs, list, BotCore::kObsMaxUnits);
			uint16 declared = (data != nullptr && len >= 2)
				? (uint16)((uint16)data[0] | ((uint16)data[1] << 8)) : 0;
			std::lock_guard<std::mutex> lock(m_obsLock);
			for (int i = 0; i < n; i++)
				m_obs.Upsert(list[i]);
			m_reqUserInRecv++;
			m_reqUserInUnits += (uint32)n;
			if (declared > (uint16)n)
				m_reqUserInParseStop++;
			m_userInEcho = (1ull << 63) | (uint64)n;
		}
		else if (opcode == WIZ_REGIONCHANGE)
		{
			uint16 ids[BotCore::kObsMaxUnits * 4];
			int n = BotCore::ParseRegionList(data, len, ids, BotCore::kObsMaxUnits * 4);
			std::lock_guard<std::mutex> lock(m_obsLock);

			// Diagnostic (plan F4-54): remember the table size and ids before Retain so the drop count and the
			// last dropped ids can be reported. No behavior depends on these values.
			uint16 before[BotCore::kObsMaxUnits];
			int beforeCount = m_obs.Count();
			for (int i = 0; i < beforeCount; i++)
				before[i] = m_obs.At(i).sid;

			m_obsUnresolved = (uint32)m_obs.Retain(ids, n, 0xFFFF);
			int dropped = beforeCount - m_obs.Count();
			int written = 0;
			for (int i = beforeCount - 1; i >= 0 && written < 8; i--)
			{
				if (m_obs.Find(before[i]) == nullptr)
					m_regionDroppedLastIds[written++] = before[i];
			}
			m_regionDroppedLastCount = written;
			m_regionRecv++;
			m_regionIdsLast = (uint32)n;
			m_regionDroppedTotal += (uint32)(dropped < 0 ? 0 : dropped);
			m_obsPending.Set(ids, n, m_obs);
		}
		else if (opcode == WIZ_MOVE)
		{
			BotCore::MoveObs move;
			if (BotCore::ParseMoveFull(data, len, move))
			{
				std::lock_guard<std::mutex> lock(m_obsLock);
				if (m_obs.Find(move.sid) == nullptr)
					m_moveUnknown++;
				m_obs.UpdateMove(move.sid, move.x10, move.z10, move.y10, move.speed, nowMs);
			}
		}
		else if (opcode == WIZ_DEAD && len >= 2)
		{
			uint16 sid = (uint16)data[0] | ((uint16)data[1] << 8);
			std::lock_guard<std::mutex> lock(m_obsLock);
			m_obs.MarkDead(sid, nowMs);
			m_hp.Invalidate(sid);
		}
	}

	// NPC perception (ADR-0017 Ek F4-14): what a client would learn about the NPCs in view. Same rules as above: only the
	// packets the server sends to this session are read; parsing happens before the lock is taken.
	if (opcode == WIZ_NPC_INOUT || opcode == WIZ_REQ_NPCIN || opcode == WIZ_NPC_REGION
		|| opcode == WIZ_NPC_MOVE || opcode == WIZ_DEAD)
	{
		const uint8 * data = pkt.size() > 0 ? pkt.contents() : nullptr;
		size_t len = pkt.size();
		uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();

		if (opcode == WIZ_NPC_INOUT)
		{
			uint8 type = 0;
			BotCore::NpcObs npc;
			if (BotCore::ParseNpcInOut(data, len, nowMs, type, npc))
			{
				std::lock_guard<std::mutex> lock(m_obsLock);
				if (type == BotCore::kNpcInOutOut)
				{
					m_npcs.Remove(npc.id);
					m_hp.Invalidate(npc.id);
				}
				else
				{
					m_npcs.Upsert(npc);
				}
			}
		}
		else if (opcode == WIZ_REQ_NPCIN)
		{
			BotCore::NpcObs list[BotCore::kNpcMaxUnits];
			uint16_t declared = 0;
			int n = BotCore::ParseNpcList(data, len, nowMs, list, BotCore::kNpcMaxUnits, declared);
			std::lock_guard<std::mutex> lock(m_obsLock);
			for (int i = 0; i < n; i++)
				m_npcs.Upsert(list[i]);
			if (n == BotCore::kNpcMaxUnits && declared > n)
				m_npcs.NoteDropped((uint32_t)(declared - n));
			m_npcInEcho = (1ull << 63) | (uint64)n;
		}
		else if (opcode == WIZ_NPC_REGION)
		{
			uint16 ids[BotCore::kNpcMaxUnits * 4];
			int n = BotCore::ParseRegionList(data, len, ids, BotCore::kNpcMaxUnits * 4);
			std::lock_guard<std::mutex> lock(m_obsLock);
			m_npcUnresolved = (uint32)m_npcs.Retain(ids, n);
			m_npcPending.Set(ids, n, m_npcs);
		}
		else if (opcode == WIZ_NPC_MOVE)
		{
			uint16 id = 0, x10 = 0, z10 = 0, y10 = 0;
			if (BotCore::ParseNpcMove(data, len, id, x10, z10, y10))
			{
				std::lock_guard<std::mutex> lock(m_obsLock);
				m_npcs.UpdatePosition(id, x10, z10, y10, nowMs);
			}
		}
		else if (opcode == WIZ_DEAD && len >= 2)
		{
			uint16 id = (uint16)data[0] | ((uint16)data[1] << 8);
			std::lock_guard<std::mutex> lock(m_obsLock);
			m_npcs.MarkDead(id, nowMs);
			m_hp.Invalidate(id);
		}
	}

	// Server-side warp: u16 x, u16 z (both x10; CharacterMovementHandler.cpp:642-644). CUser::Send() delivers it on the same
	// thread, inside HandlePacket(), so TickSpeedCheck() can read it right after the call.
	if (opcode == WIZ_WARP && pkt.size() >= 4)
	{
		uint16 x = pkt.read<uint16>(0);
		uint16 z = pkt.read<uint16>(2);
		m_warpEcho = (1ull << 63) | (uint64(x) << 16) | uint64(z);
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
	m_castTargetId = -1;
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
	m_partyEnteredHasAt = false;
	m_partyManageHasLast = false;
	m_chatHasLast = false;
	m_chatLastHash = 0;
	m_chatWindow.Clear();
	m_partyLeaveEcho = 0;
	m_chatEchoHash = 0;
	m_chatEcho = 0;
	{
		std::lock_guard<std::mutex> lock(m_obsLock);
		m_obs.Clear();
		m_obsPending.Clear();
		m_npcs.Clear();
		m_npcPending.Clear();
		m_team.Clear();
		m_hp.Clear();
		m_skillEvents.Clear();
		m_regionDroppedLastCount = 0;
		for (int i = 0; i < 8; i++)
			m_regionDroppedLastIds[i] = 0;
	}
	m_obsUnresolved = 0;
	m_npcUnresolved = 0;
	m_inoutIn = 0;
	m_inoutOut = 0;
	m_inoutParseFail = 0;
	m_reqUserInRecv = 0;
	m_reqUserInUnits = 0;
	m_reqUserInParseStop = 0;
	m_regionRecv = 0;
	m_regionIdsLast = 0;
	m_regionDroppedTotal = 0;
	m_moveUnknown = 0;
	m_userInHasLast = false;
	m_userInRequests = 0;
	m_userInUnits = 0;
	m_userInEcho = 0;
	m_npcInHasLast = false;
	m_npcInRequests = 0;
	m_npcInUnits = 0;
	m_npcInEcho = 0;
	m_speedHasLast = false;
	m_speedChecks = 0;
	m_speedWarps = 0;
	m_warpEcho = 0;
	m_selectResult = SELECT_PENDING;
	m_packetTotal = 0;
	m_attackEcho = 0;
	m_castSelfId = -1;
	m_selfSid = -1;
	m_castEcho = 0;
	m_stateEcho = 0;
	for (int i = 0; i < 256; i++)
		m_opcodeCount[i] = 0;
}

int BotSession::PeekUserInBatch(uint16 selfSid, uint16 * out, int cap)
{
	std::lock_guard<std::mutex> lock(m_obsLock);
	return m_obsPending.Peek(m_obs, (uint16_t)selfSid, out, cap);
}

void BotSession::DropUserInBatch(const uint16 * ids, int n)
{
	std::lock_guard<std::mutex> lock(m_obsLock);
	m_obsPending.Remove(ids, n);
}

int BotSession::PeekNpcInBatch(uint16 * out, int cap)
{
	std::lock_guard<std::mutex> lock(m_obsLock);
	return m_npcPending.Peek(m_npcs, (uint16_t)0xFFFF, out, cap);
}

void BotSession::DropNpcInBatch(const uint16 * ids, int n)
{
	std::lock_guard<std::mutex> lock(m_obsLock);
	m_npcPending.Remove(ids, n);
}
