#pragma once

// Perception slice 1 (ADR-0017 Ek F4-12): the observation table a bot keeps for the players in
// view. It is fed only from the packets the server sends to the bot's own session; no other
// session, region array or map structure is ever touched (docs/14 section 5.2). Pure logic: the
// standard library only, and the parser never reads past the end of its buffer.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace BotCore
{
	constexpr int      kObsMaxUnits = 64;   // table capacity per bot (design limit; one 3x3 region group)
	constexpr uint32_t kObsNameMax  = 24;   // stored name buffer, NUL included; MAX_ID_SIZE is 20, longer names are rejected
	constexpr uint16_t kObsInOutOut = 2;    // InOutType: INOUT_OUT (1 in, 3 respawn, 4 warp, 5 summon = present)
	constexpr uint8_t  kObsUserDead = 3;    // USER_DEAD in the m_bResHpType byte of the user info

	struct UnitObs
	{
		uint16_t sid;
		uint8_t  nation;
		uint8_t  race;
		uint16_t cls;
		uint8_t  level;
		uint16_t x10, z10, y10;       // position x10 as sent on the wire
		uint8_t  resHpType;           // 1 standing, 2 sitting, 3 dead (as in the user info)
		bool     partyLeader;
		uint8_t  invisibility;
		uint64_t lastSeenMs;          // caller's clock (steady_clock ms) of the packet that last touched the unit
		char     name[kObsNameMax];   // NUL terminated
	};

	// Small-endian reader over a packet payload. A read past the end zeroes the result, clears
	// ok() and leaves the cursor at the end: it never reads out of bounds.
	class ByteReader
	{
	public:
		ByteReader(const uint8_t * data, size_t len)
		{
			m_data = data;
			m_len = (data != nullptr) ? len : 0;
			m_pos = 0;
			m_ok = (data != nullptr) && (len > 0);
		}

		uint8_t U8()
		{
			if (!m_ok || m_pos + 1 > m_len)
			{
				Fail();
				return 0;
			}
			return m_data[m_pos++];
		}

		uint16_t U16()
		{
			uint16_t value = (uint16_t)U8();
			value |= (uint16_t)U8() << 8;
			return value;
		}

		uint32_t U32()
		{
			uint32_t value = (uint32_t)U16();
			value |= (uint32_t)U16() << 16;
			return value;
		}

		void Skip(size_t n)
		{
			if (!m_ok || m_pos + n > m_len)
			{
				Fail();
				return;
			}
			m_pos += n;
		}

		// u8 length + that many bytes. Fails (and clears ok()) when the value does not fit in
		// cap - 1 or when the buffer is too short; on success 'out' is NUL terminated.
		bool Str(char * out, uint32_t cap)
		{
			if (!m_ok || cap == 0)
			{
				Fail();
				return false;
			}
			uint8_t n = U8();
			if (!m_ok)
				return false;
			if ((uint32_t)n > cap - 1 || m_pos + n > m_len)
			{
				if (cap > 0)
					out[0] = '\0';
				Fail();
				return false;
			}
			for (uint8_t i = 0; i < n; i++)
				out[i] = (char)m_data[m_pos + i];
			out[n] = '\0';
			m_pos += n;
			return true;
		}

		bool ok() const { return m_ok; }
		size_t pos() const { return m_pos; }
		size_t remaining() const { return m_pos <= m_len ? m_len - m_pos : 0; }

	private:
		void Fail()
		{
			m_ok = false;
			m_pos = m_len;
		}

		const uint8_t * m_data;
		size_t m_len;
		size_t m_pos;
		bool m_ok;
	};

	// Reads the user info that follows a WIZ_USER_INOUT / WIZ_REQ_USERIN entry (the sid has already
	// been consumed) and advances 'r' to the end of the record. See the field-order table in the
	// plan: name, nation, clan block, level/race/class, position, face/hair, hp type, abnormal,
	// party/invisibility flags, direction, ranks, 10 equipment records, zone.
	inline bool ParseUserInfo(ByteReader & r, uint16_t sid, uint64_t nowMs, UnitObs & out)
	{
		char name[kObsNameMax];
		if (!r.Str(name, kObsNameMax))
			return false;

		uint8_t nation = r.U8();
		r.Skip(2);                    // clan id
		r.Skip(1);                    // fame

		// clan block: alliance id, clan name, grade, ranking, mark version, cape. The no-clan form
		// written by the server is byte-identical in size (alliance 0, empty name, grade 0,
		// ranking 0, mark 0, cape 0xFFFF).
		r.Skip(2);                    // alliance id
		{
			uint8_t clanNameLen = r.U8();
			r.Skip(clanNameLen);
		}
		r.Skip(1);                    // grade
		r.Skip(1);                    // ranking
		r.Skip(2);                    // mark version
		r.Skip(2);                    // cape id

		out.level = r.U8();
		out.race = r.U8();
		out.cls = r.U16();
		out.x10 = r.U16();
		out.z10 = r.U16();
		out.y10 = r.U16();
		r.Skip(2);                    // face, hair
		out.resHpType = r.U8();
		r.Skip(4);                    // abnormal status
		r.Skip(2);                    // party search, authority
		out.partyLeader = (r.U8() != 0);
		out.invisibility = r.U8();
		r.Skip(1);                    // helmet hidden
		r.Skip(2);                    // direction
		r.Skip(4);                    // chicken flag, king flag, two NP ranks
		r.Skip(70);                   // 10 equipment records x (u32 num, i16 durability, u8 flag)
		r.Skip(1);                    // zone id

		for (int i = 0; i < kObsNameMax; i++)
			out.name[i] = name[i];
		out.sid = sid;
		out.nation = nation;
		out.lastSeenMs = nowMs;
		return r.ok();
	}

	// WIZ_USER_INOUT: u16 type, u16 sid, then the user info unless type == kObsInOutOut. For OUT
	// nothing but out.sid is touched. Returns false on a malformed packet.
	inline bool ParseUserInOut(const uint8_t * data, size_t len, uint64_t nowMs, uint16_t & type, UnitObs & out)
	{
		ByteReader r(data, len);
		type = r.U16();
		out.sid = r.U16();
		if (!r.ok())
			return false;
		if (type == kObsInOutOut)
			return true;
		return ParseUserInfo(r, out.sid, nowMs, out);
	}

	// WIZ_REQ_USERIN: u16 count, then count x (u8 0, u16 sid, user info). Parses at most 'cap'
	// entries and stops at the first malformed or missing one; returns the number written to
	// 'out'. A count larger than the data is not an error.
	inline int ParseUserList(const uint8_t * data, size_t len, uint64_t nowMs, UnitObs * out, int cap)
	{
		ByteReader r(data, len);
		uint16_t count = r.U16();
		if (!r.ok())
			return 0;

		int written = 0;
		for (uint16_t i = 0; i < count && written < cap; i++)
		{
			r.Skip(1);                // entry marker
			uint16_t sid = r.U16();
			if (!r.ok())
				break;
			if (!ParseUserInfo(r, sid, nowMs, out[written]))
				break;
			written++;
		}
		return written;
	}

	// WIZ_MOVE: u16 sid, u16 x, u16 z, u16 y, i16 speed, u8 echo (x/z/y are x10). Needs 11 bytes.
	inline bool ParseMove(const uint8_t * data, size_t len, uint16_t & sid, uint16_t & x10,
		uint16_t & z10, uint16_t & y10)
	{
		if (data == nullptr || len < 11)
			return false;

		ByteReader r(data, len);
		sid = r.U16();
		x10 = r.U16();
		z10 = r.U16();
		y10 = r.U16();
		r.Skip(2);                    // speed
		r.Skip(1);                    // echo
		return r.ok();
	}

	// WIZ_REGIONCHANGE: u16 count, then count x u16 sid. Writes at most 'cap' ids and returns how
	// many were read (0 when malformed; a short trailing list just stops).
	inline int ParseRegionList(const uint8_t * data, size_t len, uint16_t * ids, int cap)
	{
		ByteReader r(data, len);
		uint16_t count = r.U16();
		if (!r.ok())
			return 0;

		int written = 0;
		for (uint16_t i = 0; i < count && written < cap; i++)
		{
			uint16_t id = r.U16();
			if (!r.ok())
				break;
			ids[written++] = id;
		}
		return written;
	}

	// Copyable table of observed units, keyed by sid. It has no mutex: the caller holds the lock.
	class ObsTable
	{
	public:
		ObsTable() { Clear(); }

		void Clear()
		{
			m_count = 0;
			m_overflow = 0;
		}

		int Count() const { return m_count; }
		uint32_t Overflow() const { return m_overflow; }

		// Insert or refresh by sid; false (and Overflow()++ ) when a fresh unit is added and the table is full.
		bool Upsert(const UnitObs & u)
		{
			int idx = IndexOf(u.sid);
			if (idx >= 0)
			{
				m_units[idx] = u;
				return true;
			}
			if (m_count >= kObsMaxUnits)
			{
				m_overflow++;
				return false;
			}
			m_units[m_count] = u;
			m_count++;
			return true;
		}

		// WIZ_USER_INOUT OUT: closes the gap with the last element (order is not preserved).
		void Remove(uint16_t sid)
		{
			int idx = IndexOf(sid);
			if (idx < 0)
				return;
			m_count--;
			if (idx != m_count)
				m_units[idx] = m_units[m_count];
		}

		bool UpdatePosition(uint16_t sid, uint16_t x10, uint16_t z10, uint16_t y10, uint64_t nowMs)
		{
			int idx = IndexOf(sid);
			if (idx < 0)
				return false;
			m_units[idx].x10 = x10;
			m_units[idx].z10 = z10;
			m_units[idx].y10 = y10;
			m_units[idx].lastSeenMs = nowMs;
			return true;
		}

		bool MarkDead(uint16_t sid, uint64_t nowMs)
		{
			int idx = IndexOf(sid);
			if (idx < 0)
				return false;
			m_units[idx].resHpType = kObsUserDead;
			m_units[idx].lastSeenMs = nowMs;
			return true;
		}

		// Drops every unit not in 'ids' (selfSid is always dropped) and returns how many listed ids
		// other than selfSid are unknown to the table; repeats are counted separately.
		int Retain(const uint16_t * ids, int n, uint16_t selfSid)
		{
			int unresolved = 0;
			for (int i = 0; i < n; i++)
			{
				if (ids[i] != selfSid && IndexOf(ids[i]) < 0)
					unresolved++;
			}

			int keep = 0;
			for (int i = 0; i < m_count; i++)
			{
				bool listed = false;
				if (m_units[i].sid != selfSid)
				{
					for (int j = 0; j < n; j++)
					{
						if (ids[j] == m_units[i].sid)
						{
							listed = true;
							break;
						}
					}
				}
				if (listed)
				{
					if (keep != i)
						m_units[keep] = m_units[i];
					keep++;
				}
			}
			m_count = keep;
			return unresolved;
		}

		const UnitObs * Find(uint16_t sid) const
		{
			int idx = IndexOf(sid);
			return idx >= 0 ? &m_units[idx] : nullptr;
		}

		const UnitObs & At(int i) const
		{
			return m_units[i];
		}

	private:
		int IndexOf(uint16_t sid) const
		{
			for (int i = 0; i < m_count; i++)
			{
				if (m_units[i].sid == sid)
					return i;
			}
			return -1;
		}

		UnitObs m_units[kObsMaxUnits];
		int m_count;
		uint32_t m_overflow;
	};

	// --- region-change user request (ADR-0017 Ek F4-13) ---

	constexpr int      kObsPendingMax  = 128;   // ids kept from one WIZ_REGIONCHANGE list (design limit; the table holds 64)
	constexpr int      kUserInMaxIds   = 32;    // ids per WIZ_REQ_USERIN request (CLI-19, design limit) [A]
	constexpr uint32_t kUserInMinGapMs = 1000;  // min time between two requests (CLI-19, design limit) [A]

	// Copyable, lock-free list of ids a WIZ_REGIONCHANGE or WIZ_NPC_REGION listed and the table did not know. The
	// caller holds the lock; insertion order is kept.
	class PendingIds
	{
	public:
		PendingIds() { Clear(); }

		void Clear()
		{
			m_count = 0;
		}

		int Count() const { return m_count; }

		// Replaces the content with the ids of 'ids' that 'obs' does not know yet. Repeats are skipped; at most
		// kObsPendingMax kept. 'obs' is an ObsTable or an NpcTable (both offer Find(uint16_t)).
		template <class TableT>
		void Set(const uint16_t * ids, int n, const TableT & obs)
		{
			m_count = 0;
			for (int i = 0; i < n && m_count < kObsPendingMax; i++)
			{
				if (obs.Find(ids[i]) != nullptr)
					continue;
				if (Contains(ids[i]))
					continue;
				m_ids[m_count++] = ids[i];
			}
		}

		// Drops the ids that 'obs' knows by now and 'selfSid'; copies up to 'cap' of the rest to 'out' in order
		// WITHOUT removing them. Returns how many were copied. 'obs' is an ObsTable or an NpcTable.
		template <class TableT>
		int Peek(const TableT & obs, uint16_t selfSid, uint16_t * out, int cap)
		{
			int keep = 0;
			for (int i = 0; i < m_count; i++)
			{
				if (m_ids[i] == selfSid || obs.Find(m_ids[i]) != nullptr)
					continue;
				m_ids[keep++] = m_ids[i];
			}
			m_count = keep;

			int written = keep < cap ? keep : cap;
			for (int i = 0; i < written; i++)
				out[i] = m_ids[i];
			return written;
		}

		// Removes the listed ids (order of the others is kept).
		void Remove(const uint16_t * ids, int n)
		{
			int keep = 0;
			for (int i = 0; i < m_count; i++)
			{
				bool listed = false;
				for (int j = 0; j < n; j++)
				{
					if (ids[j] == m_ids[i])
					{
						listed = true;
						break;
					}
				}
				if (!listed)
					m_ids[keep++] = m_ids[i];
			}
			m_count = keep;
		}

	private:
		bool Contains(uint16_t id) const
		{
			for (int i = 0; i < m_count; i++)
			{
				if (m_ids[i] == id)
					return true;
			}
			return false;
		}

		uint16_t m_ids[kObsPendingMax];
		int m_count;
	};

	// Guard input for a WIZ_REQ_USERIN request (CLI-19).
	struct UserInCheck
	{
		int count;               // ids the request would carry
		bool hasLast;            // a request was sent earlier in this spawn
		uint32_t sinceLastMs;    // since that request
	};

	enum UserInVerdict
	{
		USERIN_OK = 0,
		USERIN_REJECT_COUNT = 1,   // CLI-19: count < 1 or > kUserInMaxIds (defensive; the caller already clamps)
		USERIN_REJECT_GAP = 2      // CLI-19: previous request < kUserInMinGapMs ago
	};

	// Order: count, gap. Not rate limited by CLI-11 (automatic client traffic, see ADR-0017 Ek F4-13).
	inline UserInVerdict CheckUserIn(const UserInCheck & c)
	{
		if (c.count < 1 || c.count > kUserInMaxIds)
			return USERIN_REJECT_COUNT;

		if (c.hasLast && c.sinceLastMs < kUserInMinGapMs)
			return USERIN_REJECT_GAP;

		return USERIN_OK;
	}

	// --- NPC observation (ADR-0017 Ek F4-14) ---

	constexpr int      kNpcMaxUnits = 128;   // table capacity per bot (design limit; a 3x3 region group of the arena holds far fewer)
	constexpr uint32_t kNpcNameMax  = 32;    // stored name buffer, NUL included; the longest NPC name in the tables is 30
	constexpr uint8_t  kNpcInOutOut = 2;     // InOutType: INOUT_OUT, ONE byte in WIZ_NPC_INOUT (1 in, 3 respawn, 4 warp, 5 summon = present)

	struct NpcObs
	{
		uint16_t id;
		uint16_t protoId;             // prototype id as sent (the server's m_sSid)
		uint8_t  type;                // NPC type byte as sent
		uint8_t  nation;              // 0 for monsters (the server sends 0)
		uint8_t  level;
		uint16_t x10, z10, y10;       // position x10 as sent on the wire
		bool     gateOpen;            // gate flag of the last info packet (not updated by object events)
		uint8_t  objectType;
		bool     dead;                // set by WIZ_DEAD, cleared by a fresh info record
		uint64_t lastSeenMs;          // caller's clock (steady_clock ms) of the packet that last touched the NPC
		char     name[kNpcNameMax];   // NUL terminated, every byte after the NUL is zero
	};

	// Reads the NPC info record (the id has already been consumed) that follows every WIZ_NPC_INOUT
	// entry except OUT and every WIZ_REQ_NPCIN entry, and advances 'r' to the end of the record.
	// Field order as the server writes it: proto id, picture id, type, selling group, size, weapon 1
	// and 2, name (u8 length + bytes), nation, level, position, gate flag, object type, two unknown
	// u16 and direction.
	inline bool ParseNpcInfo(ByteReader & r, uint16_t id, uint64_t nowMs, NpcObs & out)
	{
		char name[kNpcNameMax];
		out.protoId = r.U16();
		r.Skip(2);                    // picture id
		out.type = r.U8();
		r.Skip(4);                    // selling group
		r.Skip(2);                    // size
		r.Skip(4);                    // weapon 1
		r.Skip(4);                    // weapon 2
		if (!r.Str(name, kNpcNameMax))
			return false;
		out.nation = r.U8();
		out.level = r.U8();
		out.x10 = r.U16();
		out.z10 = r.U16();
		out.y10 = r.U16();
		out.gateOpen = (r.U32() != 0);
		out.objectType = r.U8();
		r.Skip(2);                    // unknown
		r.Skip(2);                    // unknown
		r.Skip(1);                    // direction

		for (uint32_t i = 0; i < kNpcNameMax; i++)
			out.name[i] = '\0';
		for (uint32_t i = 0; i < kNpcNameMax && name[i] != '\0'; i++)
			out.name[i] = name[i];

		out.id = id;
		out.dead = false;
		out.lastSeenMs = nowMs;
		return r.ok();
	}

	// WIZ_NPC_INOUT: u8 type, u16 id, then the NPC info unless type == kNpcInOutOut. For OUT only
	// out.id is written. Returns false on a malformed packet (needs 3 bytes).
	inline bool ParseNpcInOut(const uint8_t * data, size_t len, uint64_t nowMs, uint8_t & type, NpcObs & out)
	{
		ByteReader r(data, len);
		type = r.U8();
		out.id = r.U16();
		if (!r.ok())
			return false;
		if (type == kNpcInOutOut)
			return true;
		return ParseNpcInfo(r, out.id, nowMs, out);
	}

	// WIZ_REQ_NPCIN: u16 count, then count x (u16 id, NPC info) with NO marker byte. Parses at most
	// 'cap' entries and stops at the first malformed or missing one; returns the number written to
	// 'out'. 'declared' receives the count the packet states (0 when the count itself is unreadable).
	// A count larger than the data is not an error.
	inline int ParseNpcList(const uint8_t * data, size_t len, uint64_t nowMs, NpcObs * out, int cap, uint16_t & declared)
	{
		declared = 0;
		ByteReader r(data, len);
		uint16_t count = r.U16();
		if (!r.ok())
			return 0;
		declared = count;

		int written = 0;
		for (uint16_t i = 0; i < count && written < cap; i++)
		{
			uint16_t id = r.U16();
			if (!r.ok())
				break;
			if (!ParseNpcInfo(r, id, nowMs, out[written]))
				break;
			written++;
		}
		return written;
	}

	// WIZ_NPC_MOVE: u16 id, u16 x, u16 z, u16 y, u16 speed (x, z, y are x10). Needs 10 bytes.
	inline bool ParseNpcMove(const uint8_t * data, size_t len, uint16_t & id, uint16_t & x10, uint16_t & z10, uint16_t & y10)
	{
		if (data == nullptr || len < 10)
			return false;

		ByteReader r(data, len);
		id = r.U16();
		x10 = r.U16();
		z10 = r.U16();
		y10 = r.U16();
		r.Skip(2);                    // speed
		return r.ok();
	}

	// Copyable table of observed NPCs, keyed by id. It has no mutex: the caller holds the lock.
	class NpcTable
	{
	public:
		NpcTable() { Clear(); }

		void Clear()
		{
			m_count = 0;
			m_overflow = 0;
		}

		int Count() const { return m_count; }
		uint32_t Overflow() const { return m_overflow; }

		// Adds n to Overflow() (list entries beyond the parse cap).
		void NoteDropped(uint32_t n) { m_overflow += n; }

		// Insert or refresh by id; false (and Overflow()++ ) when a fresh NPC is added and the table is full.
		bool Upsert(const NpcObs & n)
		{
			int idx = IndexOf(n.id);
			if (idx >= 0)
			{
				m_npcs[idx] = n;
				return true;
			}
			if (m_count >= kNpcMaxUnits)
			{
				m_overflow++;
				return false;
			}
			m_npcs[m_count] = n;
			m_count++;
			return true;
		}

		// WIZ_NPC_INOUT OUT: closes the gap with the last element (order is not preserved).
		void Remove(uint16_t id)
		{
			int idx = IndexOf(id);
			if (idx < 0)
				return;
			m_count--;
			if (idx != m_count)
				m_npcs[idx] = m_npcs[m_count];
		}

		bool UpdatePosition(uint16_t id, uint16_t x10, uint16_t z10, uint16_t y10, uint64_t nowMs)
		{
			int idx = IndexOf(id);
			if (idx < 0)
				return false;
			m_npcs[idx].x10 = x10;
			m_npcs[idx].z10 = z10;
			m_npcs[idx].y10 = y10;
			m_npcs[idx].lastSeenMs = nowMs;
			return true;
		}

		bool MarkDead(uint16_t id, uint64_t nowMs)
		{
			int idx = IndexOf(id);
			if (idx < 0)
				return false;
			m_npcs[idx].dead = true;
			m_npcs[idx].lastSeenMs = nowMs;
			return true;
		}

		// Drops every NPC not in 'ids' and returns how many listed ids are unknown; repeats are
		// counted separately.
		int Retain(const uint16_t * ids, int n)
		{
			int unresolved = 0;
			for (int i = 0; i < n; i++)
			{
				if (IndexOf(ids[i]) < 0)
					unresolved++;
			}

			int keep = 0;
			for (int i = 0; i < m_count; i++)
			{
				bool listed = false;
				for (int j = 0; j < n; j++)
				{
					if (ids[j] == m_npcs[i].id)
					{
						listed = true;
						break;
					}
				}
				if (listed)
				{
					if (keep != i)
						m_npcs[keep] = m_npcs[i];
					keep++;
				}
			}
			m_count = keep;
			return unresolved;
		}

		const NpcObs * Find(uint16_t id) const
		{
			int idx = IndexOf(id);
			return idx >= 0 ? &m_npcs[idx] : nullptr;
		}

		const NpcObs & At(int i) const
		{
			return m_npcs[i];
		}

	private:
		int IndexOf(uint16_t id) const
		{
			for (int i = 0; i < m_count; i++)
			{
				if (m_npcs[i].id == id)
					return i;
			}
			return -1;
		}

		NpcObs m_npcs[kNpcMaxUnits];
		int m_count;
		uint32_t m_overflow;
	};

	// --- region-change NPC request (ADR-0017 Ek F4-15) ---

	constexpr int      kNpcInMaxIds   = 32;    // ids per WIZ_REQ_NPCIN request (CLI-20, design limit) [A]
	constexpr uint32_t kNpcInMinGapMs = 1000;  // min time between two requests (CLI-20, design limit) [A]

	// Guard input for a WIZ_REQ_NPCIN request (CLI-20).
	struct NpcInCheck
	{
		int count;               // ids the request would carry
		bool hasLast;            // a request was sent earlier in this spawn
		uint32_t sinceLastMs;    // since that request
	};

	enum NpcInVerdict
	{
		NPCIN_OK = 0,
		NPCIN_REJECT_COUNT = 1,   // CLI-20: count < 1 or > kNpcInMaxIds (defensive; the caller already clamps)
		NPCIN_REJECT_GAP = 2      // CLI-20: previous request < kNpcInMinGapMs ago
	};

	// Order: count, gap. Not rate limited by CLI-11 (automatic client traffic, see ADR-0017 Ek F4-15).
	inline NpcInVerdict CheckNpcIn(const NpcInCheck & c)
	{
		if (c.count < 1 || c.count > kNpcInMaxIds)
			return NPCIN_REJECT_COUNT;

		if (c.hasLast && c.sinceLastMs < kNpcInMinGapMs)
			return NPCIN_REJECT_GAP;

		return NPCIN_OK;
	}
}
