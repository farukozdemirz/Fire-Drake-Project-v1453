#pragma once

// Perception slice 1 (ADR-0017 Ek F4-12): the observation table a bot keeps for the players in
// view. It is fed only from the packets the server sends to the bot's own session; no other
// session, region array or map structure is ever touched (docs/14 section 5.2). Pure logic: the
// standard library only, and the parser never reads past the end of its buffer.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>

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

	// Copyable, lock-free list of ids a WIZ_REGIONCHANGE or WIZ_NPC_REGION listed and the table did not know. The caller holds the
	// lock; insertion order is kept.
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
		// kObsPendingMax kept.
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
		// WITHOUT removing them. Returns how many were copied.
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

	// --- perception snapshot (ADR-0017 Ek F4-16) ---

	constexpr int     kSnapMaxUnits = 32;   // enemies / allies kept per list (nearest first), design limit
	constexpr int     kSnapMaxNpcs  = 32;   // npcs kept (nearest first), design limit
	constexpr uint8_t kSnapUserSit  = 2;    // USER_SITDOWN in the res/hp type byte of the user info
	constexpr int     kSnapMaxBuffs     = 16;   // type 4 buffs/debuffs kept in the self state (design limit)
	constexpr int     kSnapMaxCooldowns = 16;   // skills with a running reuse timer kept in the self state

	// One type 4 buff or debuff on the bot itself (the client shows both as status icons).
	struct BuffView
	{
		uint32_t skillId;
		uint8_t  buffType;       // BUFF_TYPE_* (key of the server's buff map)
		bool     isBuff;         // false = debuff
		uint32_t remainingSec;   // > 0 (expired entries are never stored)
	};

	// One skill whose reuse timer is still running.
	struct CooldownView
	{
		uint32_t skillId;
		uint32_t remainingMs;    // > 0
	};

	// The bot's own state (read from its own session by the caller; the contract allows it).
	// own state only: the contract (docs/14 5.2) forbids the same fields for OTHER players
	struct SelfState
	{
		uint16_t sid;
		uint8_t  nation;
		uint16_t cls;
		uint8_t  level;
		float    x, z;                 // world position
		int32_t  hp, maxHp, mp, maxMp;
		bool     dead;
		bool     sitting;
		uint32_t     hpPotStock;        // pots in the own bag that BeginPotion would accept: HP kind
		uint32_t     mpPotStock;        // same, MP kind
		uint32_t     potWaitMs;         // shared pot timer: ms until the next pot may go out; 0 = now
		uint32_t     castGapWaitMs;     // gap after the last EFFECTING (kCastGapMs): ms until the next cast; 0 = now
		BuffView     buffs[kSnapMaxBuffs];
		int          buffCount;         // entries stored (<= kSnapMaxBuffs)
		int          buffTotal;         // buffs/debuffs with remainingSec > 0 offered to SelfAddBuff
		CooldownView cooldowns[kSnapMaxCooldowns];
		int          cooldownCount;
		int          cooldownTotal;
		bool         inParty;           // the bot's own CUser::isInParty()
		bool         partyLeader;       // the bot's own CUser::isPartyLeader()
	};

	// One visible player. No HP, MP, name or inventory: the client never learns them (docs/14 5.2).
	struct UnitView
	{
		uint16_t id;                   // socket id of the player (NOT an npc id)
		uint8_t  nation;
		uint8_t  race;
		uint16_t cls;
		uint8_t  level;
		float    x, z;                 // x10 / 10 as sent on the wire
		float    dist;                 // to SelfState x, z
		bool     dead;                 // resHpType == kObsUserDead
		bool     sitting;              // resHpType == kSnapUserSit
		bool     partyLeader;
		uint8_t  invisibility;         // raw byte, NOT filtered
		uint32_t ageMs;                // nowMs - lastSeenMs (0 if the packet clock is ahead), clamped to 0xFFFFFFFF
	};

	// One visible NPC (monster, guard tower, gate, ...). No classification yet.
	struct NpcView
	{
		uint16_t id;                   // npc id (NOT a socket id)
		uint16_t protoId;
		uint8_t  type;
		uint8_t  nation;
		uint8_t  level;
		float    x, z;
		float    dist;
		bool     dead;
		bool     gateOpen;
		uint32_t ageMs;
	};

	// --- team view (ADR-0017 Ek F4-18) ---

	constexpr int      kTeamMaxMembers = 8;     // MAX_PARTY_USERS (MEC-PTY-01)
	constexpr uint16_t kTeamNone       = 0xFFFF;
	constexpr uint8_t  kPartyInsert    = 3;     // packets.h PARTY_INSERT
	constexpr uint8_t  kPartyRemove    = 4;     // PARTY_REMOVE
	constexpr uint8_t  kPartyDelete    = 5;     // PARTY_DELETE
	constexpr uint8_t  kPartyHpChange  = 6;     // PARTY_HPCHANGE
	constexpr uint8_t  kPartyFlagJoin  = 1;     // member record flag: joined
	constexpr uint8_t  kPartyFlagLeader = 100;  // member record flag: leader moved (PartyPromote)

	struct TeamObs
	{
		uint16_t sid;
		uint8_t  nation;
		uint8_t  level;
		uint16_t cls;
		int32_t  hp, maxHp, mp, maxMp;
		uint64_t lastSeenMs;         // caller's clock of the packet that last touched the member (record or HP change)
		char     name[kObsNameMax];  // NUL terminated
	};

	enum PartyEventKind
	{
		PARTY_EV_NONE   = 0,
		PARTY_EV_MEMBER = 1,   // PARTY_INSERT member record (flag 1 or 100)
		PARTY_EV_HP     = 2,   // PARTY_HPCHANGE
		PARTY_EV_REMOVE = 3,   // PARTY_REMOVE
		PARTY_EV_DELETE = 4    // PARTY_DELETE
	};

	struct PartyEvent
	{
		PartyEventKind kind;
		uint8_t  flag;               // MEMBER only
		uint16_t sid;                // HP / REMOVE (for MEMBER the sid is member.sid)
		int32_t  hp, maxHp, mp, maxMp;   // HP only
		uint64_t nowMs;              // caller's clock carried with the event (the HP branch stamps the member's lastSeenMs)
		TeamObs  member;             // MEMBER only
	};

	// Parses one WIZ_PARTY payload (starts at the sub-opcode byte). Returns true when 'out' holds a MEMBER / HP / REMOVE /
	// DELETE event. Everything else returns false and leaves out.kind == PARTY_EV_NONE: other sub-opcodes, the 3-byte
	// PARTY_INSERT refusal (i16 code), an unknown member flag (not 1 / 100), a truncated or over-long field (name > kObsNameMax - 1).
	// Member name = u16 length + bytes (ByteBuffer default, no SByte()).
	// Layouts: PartyHandler.cpp:233-260, :315-326, :393-395, :433-434, User.cpp:2057-2065. Never reads out of bounds;
	// data may be null with len 0.
	inline bool ParsePartyEvent(const uint8_t * data, size_t len, uint64_t nowMs, PartyEvent & out)
	{
		memset(&out, 0, sizeof(out));
		out.kind = PARTY_EV_NONE;
		out.nowMs = nowMs;

		ByteReader r(data, len);
		uint8_t sub = r.U8();
		if (!r.ok())
			return false;

		if (sub == kPartyInsert)
		{
			if (len == 3)
				return false;         // the bot's own invitation was refused: i16 code, not a member record

			uint16_t sid = r.U16();
			uint8_t flag = r.U8();
			if (flag != kPartyFlagJoin && flag != kPartyFlagLeader)
				return false;

			TeamObs m;
			memset(&m, 0, sizeof(m));
			uint16_t nameLen = r.U16();
			if (!r.ok() || nameLen > kObsNameMax - 1)
				return false;
			for (uint16_t i = 0; i < nameLen; i++)
				m.name[i] = (char)r.U8();
			if (!r.ok())
				return false;
			m.name[nameLen] = '\0';

			m.maxHp = (int32_t)(int16_t)r.U16();
			m.hp = (int32_t)(int16_t)r.U16();
			m.level = r.U8();
			m.cls = r.U16();
			m.maxMp = (int32_t)(int16_t)r.U16();
			m.mp = (int32_t)(int16_t)r.U16();
			m.nation = r.U8();
			if (!r.ok())
				return false;

			m.sid = sid;
			m.lastSeenMs = nowMs;
			out.kind = PARTY_EV_MEMBER;
			out.flag = flag;
			out.member = m;
			return true;
		}

		if (sub == kPartyHpChange)
		{
			out.sid = r.U16();
			out.maxHp = (int32_t)(int16_t)r.U16();
			out.hp = (int32_t)(int16_t)r.U16();
			out.maxMp = (int32_t)(int16_t)r.U16();
			out.mp = (int32_t)(int16_t)r.U16();
			if (!r.ok())
				return false;
			out.kind = PARTY_EV_HP;
			return true;
		}

		if (sub == kPartyRemove)
		{
			out.sid = r.U16();
			if (!r.ok())
				return false;
			out.kind = PARTY_EV_REMOVE;
			return true;
		}

		if (sub == kPartyDelete)
		{
			out.kind = PARTY_EV_DELETE;
			return true;
		}

		return false;
	}

	// Copyable table of the party members the bot has been told about, keyed by sid. No mutex: the caller holds the lock.
	// The bot's own record is stored too when the server sent one (members get their own record from the join broadcast;
	// a leader never receives its own record) - BuildTeam skips it.
	class TeamTable
	{
	public:
		TeamTable() { Clear(); }

		void Clear()
		{
			m_count = 0;
			m_overflow = 0;
			m_unknownHp = 0;
			m_promotedSid = kTeamNone;
			m_firstSid = kTeamNone;
		}

		int Count() const { return m_count; }
		uint32_t Overflow() const { return m_overflow; }
		uint32_t UnknownHp() const { return m_unknownHp; }

		const TeamObs * Find(uint16_t sid) const
		{
			int idx = IndexOf(sid);
			return idx >= 0 ? &m_members[idx] : nullptr;
		}

		const TeamObs & At(int i) const { return m_members[i]; }

		// Applies one parsed event; returns true when the table changed. selfSid = the bot's own socket id (kTeamNone = unknown).
		bool Apply(const PartyEvent & ev, uint16_t selfSid)
		{
			if (ev.kind == PARTY_EV_MEMBER)
			{
				bool empty = (m_count == 0);
				int idx = IndexOf(ev.member.sid);
				if (idx >= 0)
				{
					m_members[idx] = ev.member;
				}
				else
				{
					if (m_count >= kTeamMaxMembers)
					{
						m_overflow++;
						return false;
					}
					m_members[m_count] = ev.member;
					m_count++;
				}
				if (empty)
					m_firstSid = ev.member.sid;
				if (ev.flag == kPartyFlagLeader)
					m_promotedSid = ev.member.sid;
				return true;
			}

			if (ev.kind == PARTY_EV_HP)
			{
				int idx = IndexOf(ev.sid);
				if (idx < 0)
				{
					m_unknownHp++;
					return false;
				}
				m_members[idx].hp = ev.hp;
				m_members[idx].maxHp = ev.maxHp;
				m_members[idx].mp = ev.mp;
				m_members[idx].maxMp = ev.maxMp;
				m_members[idx].lastSeenMs = ev.nowMs;
				return true;
			}

			if (ev.kind == PARTY_EV_REMOVE)
			{
				if (ev.sid == selfSid)
				{
					Clear();
					return true;
				}
				int idx = IndexOf(ev.sid);
				if (idx < 0)
					return false;
				m_count--;
				if (idx != m_count)
					m_members[idx] = m_members[m_count];
				if (m_promotedSid == ev.sid)
					m_promotedSid = kTeamNone;
				if (m_firstSid == ev.sid)
					m_firstSid = kTeamNone;
				return true;
			}

			if (ev.kind == PARTY_EV_DELETE)
			{
				Clear();
				return true;
			}

			return false;
		}

		// Leader resolution, first match wins: (1) the sid of the last flag-100 record; (2) selfSid when the bot itself is the
		// leader (selfIsLeader: a leader never receives its own record); (3) the sid that was first inserted into an empty
		// table (the joiner is sent the existing members in slot order, leader first) [A]; otherwise kTeamNone.
		uint16_t LeaderSid(uint16_t selfSid, bool selfIsLeader) const
		{
			if (m_promotedSid != kTeamNone)
				return m_promotedSid;
			if (selfIsLeader && selfSid != kTeamNone)
				return selfSid;
			if (m_firstSid != kTeamNone)
				return m_firstSid;
			return kTeamNone;
		}

	private:
		int IndexOf(uint16_t sid) const
		{
			for (int i = 0; i < m_count; i++)
			{
				if (m_members[i].sid == sid)
					return i;
			}
			return -1;
		}

		TeamObs m_members[kTeamMaxMembers];
		int m_count;
		uint32_t m_overflow;
		uint32_t m_unknownHp;
		uint16_t m_promotedSid;
		uint16_t m_firstSid;
	};

	// One other member of the bot's party. Fields the party packet gives (hp, mp, class, level, name) plus what the bot
	// already sees on its own (position, only when the member is in view). No cooldowns, buffs, stock or inventory.
	struct TeamMemberView
	{
		uint16_t id;
		uint8_t  nation;
		uint8_t  level;
		uint16_t cls;
		int32_t  hp, maxHp, mp, maxMp;
		bool     leader;
		bool     dead;               // hp <= 0, or the observation table marks the member dead
		bool     inView;             // the member is in the observation table (x, z, dist valid)
		float    x, z, dist;         // 0 unless inView; dist to SelfState x, z
		uint32_t ageMs;              // nowMs - lastSeenMs of the last party packet that touched the member (clamped, 0 if the clock is ahead)
		char     name[kObsNameMax];
	};

	struct TeamView
	{
		bool     inParty;            // SelfState.inParty; false -> everything below is empty
		bool     selfLeader;         // SelfState.partyLeader
		uint16_t leaderId;           // kTeamNone = unknown (also when not in a party)
		int      memberCount;        // entries filled (the table holds at most kTeamMaxMembers; the bot itself is never listed)
		int      memberTotal;        // other members in the table (== memberCount unless the capacity were exceeded)
		TeamMemberView members[kTeamMaxMembers];   // ascending id
	};

	// Zeroes 'out' (leaderId = kTeamNone), then, when self.inParty: leaderId = team.LeaderSid(self.sid, self.partyLeader);
	// every table entry except self.sid becomes a TeamMemberView sorted by id ascending (insertion sort, no allocation);
	// inView/x/z/dist come from obs.Find(id) (x10/10, z10/10, dist = sqrt(dx^2 + dz^2) in float), dead = hp <= 0 ||
	// (inView && obs resHpType == kObsUserDead). When !self.inParty the table is ignored (it may still hold entries of an
	// earlier party). Deterministic, no clock besides nowMs.
	inline void BuildTeam(const SelfState & self, const TeamTable & team, const ObsTable & obs, uint64_t nowMs, TeamView & out)
	{
		memset(&out, 0, sizeof(out));
		out.leaderId = kTeamNone;
		out.inParty = self.inParty;
		out.selfLeader = self.partyLeader;

		if (!self.inParty)
			return;

		out.leaderId = team.LeaderSid(self.sid, self.partyLeader);

		int other = 0;
		for (int i = 0; i < team.Count(); i++)
		{
			const TeamObs & m = team.At(i);
			if (m.sid == self.sid)
				continue;

			TeamMemberView v;
			memset(&v, 0, sizeof(v));
			v.id = m.sid;
			v.nation = m.nation;
			v.level = m.level;
			v.cls = m.cls;
			v.hp = m.hp;
			v.maxHp = m.maxHp;
			v.mp = m.mp;
			v.maxMp = m.maxMp;
			v.leader = (m.sid == out.leaderId);
			for (uint32_t k = 0; k < kObsNameMax; k++)
				v.name[k] = m.name[k];

			const UnitObs * u = obs.Find(m.sid);
			if (u != nullptr)
			{
				v.inView = true;
				v.x = u->x10 / 10.0f;
				v.z = u->z10 / 10.0f;
				float dx = v.x - self.x;
				float dz = v.z - self.z;
				v.dist = std::sqrt(dx * dx + dz * dz);
				v.dead = (u->resHpType == kObsUserDead);
			}
			if (v.hp <= 0)
				v.dead = true;

			uint64_t age = nowMs > m.lastSeenMs ? nowMs - m.lastSeenMs : 0;
			v.ageMs = age > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)age;

			if (other < kTeamMaxMembers)
				out.members[other] = v;
			other++;
		}

		out.memberTotal = other;
		out.memberCount = other < kTeamMaxMembers ? other : kTeamMaxMembers;

		for (int i = 1; i < out.memberCount; i++)
		{
			TeamMemberView key = out.members[i];
			int j = i - 1;
			while (j >= 0 && out.members[j].id > key.id)
			{
				out.members[j + 1] = out.members[j];
				j--;
			}
			out.members[j + 1] = key;
		}
	}

	struct PerceptionSnapshot
	{
		uint64_t tMs;                          // nowMs the snapshot was built for
		SelfState self;
		UnitView enemies[kSnapMaxUnits];       // nation != self.nation, nearest first
		int      enemyCount;                   // entries filled (<= kSnapMaxUnits)
		int      enemyTotal;                   // all enemies in the table (>= enemyCount)
		UnitView allies[kSnapMaxUnits];        // nation == self.nation, self excluded, nearest first
		int      allyCount;
		int      allyTotal;
		NpcView  npcs[kSnapMaxNpcs];           // nearest first
		int      npcCount;
		int      npcTotal;                     // all npcs in the table
		TeamView team;                         // filled by BuildTeam, not by BuildSnapshot
	};

	// Inserts 'v' into arr[0..count) keeping (dist, id) ascending; at capacity the farthest entry is dropped
	// (v itself when it sorts last). Both view types have 'dist' and 'id'.
	template <class V, int CAP>
	inline void SnapInsertNearest(V * arr, int & count, const V & v)
	{
		int pos = count;
		for (int i = 0; i < count; i++)
		{
			if (v.dist < arr[i].dist || (v.dist == arr[i].dist && v.id < arr[i].id))
			{
				pos = i;
				break;
			}
		}

		if (pos >= CAP)
			return;

		int limit = count < CAP ? count : CAP - 1;
		for (int i = limit; i > pos; i--)
			arr[i] = arr[i - 1];
		arr[pos] = v;
		if (count < CAP)
			count++;
	}

	// Zeroes 'out', copies 'self', then fills the lists from the two tables (read only). Players: the entry whose sid
	// equals self.sid is skipped; nation != self.nation goes to enemies, the rest to allies; dead and sitting players are
	// kept (flags). Npcs: every table entry, dead ones included. Each list keeps the kSnap* nearest by (dist, id)
	// ascending (ties: lower id first); *Total counts everything seen, so Total > Count means entries were dropped.
	// dist = sqrt((x - self.x)^2 + (z - self.z)^2) in float. Fully deterministic, no allocation, no clock.
	inline void BuildSnapshot(const SelfState & self, const ObsTable & obs, const NpcTable & npcs,
		uint64_t nowMs, PerceptionSnapshot & out)
	{
		memset(&out, 0, sizeof(out));
		out.tMs = nowMs;
		out.self = self;

		for (int i = 0; i < obs.Count(); i++)
		{
			const UnitObs & u = obs.At(i);
			if (u.sid == self.sid)
				continue;

			UnitView v;
			memset(&v, 0, sizeof(v));
			v.id = u.sid;
			v.nation = u.nation;
			v.race = u.race;
			v.cls = u.cls;
			v.level = u.level;
			v.x = u.x10 / 10.0f;
			v.z = u.z10 / 10.0f;
			float dx = v.x - self.x;
			float dz = v.z - self.z;
			v.dist = std::sqrt(dx * dx + dz * dz);
			v.dead = (u.resHpType == kObsUserDead);
			v.sitting = (u.resHpType == kSnapUserSit);
			v.partyLeader = u.partyLeader;
			v.invisibility = u.invisibility;
			uint64_t age = nowMs > u.lastSeenMs ? nowMs - u.lastSeenMs : 0;
			v.ageMs = age > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)age;

			if (u.nation != self.nation)
			{
				out.enemyTotal++;
				SnapInsertNearest<UnitView, kSnapMaxUnits>(out.enemies, out.enemyCount, v);
			}
			else
			{
				out.allyTotal++;
				SnapInsertNearest<UnitView, kSnapMaxUnits>(out.allies, out.allyCount, v);
			}
		}

		for (int i = 0; i < npcs.Count(); i++)
		{
			const NpcObs & n = npcs.At(i);

			NpcView v;
			memset(&v, 0, sizeof(v));
			v.id = n.id;
			v.protoId = n.protoId;
			v.type = n.type;
			v.nation = n.nation;
			v.level = n.level;
			v.x = n.x10 / 10.0f;
			v.z = n.z10 / 10.0f;
			float dx = v.x - self.x;
			float dz = v.z - self.z;
			v.dist = std::sqrt(dx * dx + dz * dz);
			v.dead = n.dead;
			v.gateOpen = n.gateOpen;
			uint64_t age = nowMs > n.lastSeenMs ? nowMs - n.lastSeenMs : 0;
			v.ageMs = age > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)age;

			out.npcTotal++;
			SnapInsertNearest<NpcView, kSnapMaxNpcs>(out.npcs, out.npcCount, v);
		}
	}

	// --- self state extras (ADR-0017 Ek F4-17) ---

	// Whole seconds left until 'endSec' (a time_t value); 0 when it is not in the future. Clamped to 0xFFFFFFFF.
	inline uint32_t SnapRemainingSec(int64_t endSec, int64_t nowSec)
	{
		if (endSec <= nowSec)
			return 0;

		uint64_t left = (uint64_t)(endSec - nowSec);
		return left > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)left;
	}

	// Milliseconds left of a 'spanMs' timer that started 'sinceMs' ago; 0 when it has run out.
	inline uint32_t SnapRemainingMs(uint32_t spanMs, uint64_t sinceMs)
	{
		if (sinceMs >= spanMs)
			return 0;

		return spanMs - (uint32_t)sinceMs;
	}

	// Appends a buff. remainingSec == 0 -> nothing happens (returns false, not counted). Otherwise buffTotal++ and the
	// entry is stored while buffCount < kSnapMaxBuffs (returns true when stored).
	inline bool SelfAddBuff(SelfState & s, uint32_t skillId, uint8_t buffType, bool isBuff, uint32_t remainingSec)
	{
		if (remainingSec == 0)
			return false;

		s.buffTotal++;
		if (s.buffCount >= kSnapMaxBuffs)
			return false;

		BuffView & b = s.buffs[s.buffCount++];
		b.skillId = skillId;
		b.buffType = buffType;
		b.isBuff = isBuff;
		b.remainingSec = remainingSec;
		return true;
	}

	// Same rules for cooldowns (remainingMs == 0 -> ignored).
	inline bool SelfAddCooldown(SelfState & s, uint32_t skillId, uint32_t remainingMs)
	{
		if (remainingMs == 0)
			return false;

		s.cooldownTotal++;
		if (s.cooldownCount >= kSnapMaxCooldowns)
			return false;

		CooldownView & c = s.cooldowns[s.cooldownCount++];
		c.skillId = skillId;
		c.remainingMs = remainingMs;
		return true;
	}
}
