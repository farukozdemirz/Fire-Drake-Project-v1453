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

	// Observation metadata (ADR-0017 Ek F4-50, docs/13 section 5.2a).
	constexpr int      kObsHistMax            = 4;      // position samples kept per unit (short ring)
	constexpr uint64_t kObsSampleMinGapMs     = 400;    // identical position+speed repeats closer than this are not stored
	constexpr uint32_t kObsSampleMaxAgeMs     = 4000;   // a newest sample older than this yields no velocity
	constexpr uint32_t kObsEstimateMaxAheadMs = 3000;   // dead reckoning is clamped to this many ms
	constexpr uint32_t kPosFreshMs            = 3100;   // moving unit: pos_age <= this is fresh (2 packets at the CLI-05 period) [A]
	constexpr uint32_t kPosLostMs             = 6000;   // moving unit: pos_age > this is a lost candidate [A]

	// One position sample in the short history of a unit. The ring is read oldest..newest with ObsSampleBack.
	struct PosSample
	{
		uint64_t tMs;
		uint16_t x10, z10;
		int16_t  speed;
	};

	// Freshness of the last known position (docs/13 section 5.2a [A]).
	enum PosState
	{
		POS_FRESH = 0,   // fresh, or stationary: a stopped unit never goes stale
		POS_STALE = 1,   // moving, 3100..6000 ms
		POS_LOST  = 2    // moving, > 6000 ms
	};

	// Observation source of a view record (docs/13 section 5.2a). F4-50 uses only the direct one.
	constexpr uint8_t kSrcObserved = 0;   // O: direct observation from the bot's own packets
	constexpr uint8_t kSrcTeam     = 1;   // P: relayed by a team member (later slice)
	constexpr uint8_t kSrcEstimate = 2;   // E: derived by the bot (later slice)

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
		uint64_t lastSeenMs;          // caller's clock (steady_clock ms) of the packet that last touched the unit; "last packet", not "last seen"
		uint64_t lastMoveMs;          // caller's clock of the last WIZ_MOVE (or the registration / respawn packet)
		int16_t  lastSpeed;           // speed field of that WIZ_MOVE; -1 when unknown (the info record carries no speed)
		PosSample hist[kObsHistMax];  // short ring of recent positions; read with ObsSampleBack
		uint8_t  histCount;           // valid samples in hist (<= kObsHistMax)
		uint8_t  histNext;            // ring write index (slot the next sample goes to)
		char     name[kObsNameMax];   // NUL terminated
	};

	// Returns the sample 'back' steps before the newest (0 = newest, 1 = the one before it); nullptr when
	// there is no such sample. Read only.
	inline const PosSample * ObsSampleBack(const UnitObs & u, int back)
	{
		if (back < 0 || back >= (int)u.histCount || u.histCount == 0)
			return nullptr;

		int idx = (int)u.histNext - 1 - back;
		while (idx < 0)
			idx += kObsHistMax;
		return &u.hist[idx];
	}

	// Freshness of the position: a stationary unit (known speed 0 or unknown speed) is always fresh; moving
	// = last WIZ_MOVE speed > 0. A moving unit is fresh up to 3100 ms, stale up to 6000 ms, a lost candidate
	// beyond it.
	inline uint8_t ClassifyPos(bool moving, uint32_t posAgeMs)
	{
		if (!moving)
			return POS_FRESH;
		if (posAgeMs <= kPosFreshMs)
			return POS_FRESH;
		if (posAgeMs <= kPosLostMs)
			return POS_STALE;
		return POS_LOST;
	}

	// Velocity (m/s) from the last two samples. Zero when the last packet said speed 0 (or unknown), when the
	// newest sample is older than 4000 ms, or when the sample interval is outside 400..4000 ms. The magnitude
	// is capped at lastSpeed/10 * 1.1 so a stale estimate never invents speed. Read only.
	inline void EstimateVelocity(const UnitObs & u, uint64_t nowMs, float & vx, float & vz)
	{
		vx = 0.0f;
		vz = 0.0f;

		if (u.lastSpeed <= 0)
			return;

		const PosSample * newest = ObsSampleBack(u, 0);
		const PosSample * older  = ObsSampleBack(u, 1);
		if (newest == nullptr || older == nullptr)
			return;

		if (nowMs > newest->tMs && nowMs - newest->tMs > kObsSampleMaxAgeMs)
			return;

		uint64_t dt = newest->tMs > older->tMs ? newest->tMs - older->tMs : 0;
		if (dt < kObsSampleMinGapMs || dt > kObsSampleMaxAgeMs)
			return;

		float dtSec = dt / 1000.0f;
		vx = (newest->x10 - older->x10) / 10.0f / dtSec;
		vz = (newest->z10 - older->z10) / 10.0f / dtSec;

		float maxV = (u.lastSpeed / 10.0f) * 1.1f;
		float mag = std::sqrt(vx * vx + vz * vz);
		if (mag > maxV && mag > 0.0f)
		{
			float scale = maxV / mag;
			vx *= scale;
			vz *= scale;
		}
	}

	// Dead reckoning: the newest sample position advanced by the velocity for at most 3000 ms. Falls back to
	// the last known position when there is no estimate. Read only.
	inline void EstimatePosition(const UnitObs & u, uint64_t nowMs, float & x, float & z)
	{
		const PosSample * newest = ObsSampleBack(u, 0);
		if (newest == nullptr)
		{
			x = u.x10 / 10.0f;
			z = u.z10 / 10.0f;
			return;
		}

		float vx = 0.0f, vz = 0.0f;
		EstimateVelocity(u, nowMs, vx, vz);

		uint64_t ahead = nowMs > newest->tMs ? nowMs - newest->tMs : 0;
		if (ahead > kObsEstimateMaxAheadMs)
			ahead = kObsEstimateMaxAheadMs;

		x = newest->x10 / 10.0f + vx * (ahead / 1000.0f);
		z = newest->z10 / 10.0f + vz * (ahead / 1000.0f);
	}

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

	// The full WIZ_MOVE payload (ADR-0017 Ek F4-50).
	struct MoveObs
	{
		uint16_t sid;
		uint16_t x10, z10, y10;
		int16_t  speed;
		uint8_t  echo;
	};

	// WIZ_MOVE: u16 sid, u16 x, u16 z, u16 y, i16 speed, u8 echo (x/z/y are x10). Needs 11 bytes; a short
	// packet or a null buffer returns false without touching 'out'. Bounds safe.
	inline bool ParseMoveFull(const uint8_t * data, size_t len, MoveObs & out)
	{
		if (data == nullptr || len < 11)
			return false;

		ByteReader r(data, len);
		out.sid = r.U16();
		out.x10 = r.U16();
		out.z10 = r.U16();
		out.y10 = r.U16();
		out.speed = (int16_t)r.U16();
		out.echo = r.U8();
		return r.ok();
	}

	// --- target HP observations (ADR-0017 Ek F4-51) ---

	constexpr int      kHpMaxEntries = 32;      // table capacity per bot; oldest observation is dropped when full
	constexpr uint32_t kHpStaleMs    = 10000;   // an observation older than this is stale (docs/09 section 4.2 EnemyIntel)

	// One WIZ_TARGET_HP payload (User.cpp:2350-2387 SendTargetHP): u16 tid, u8 echo, i32 maxHp, i32 hp, u16 damage.
	struct TargetHpMsg
	{
		uint16_t tid;
		uint8_t  echo;
		int32_t  maxHp, hp;
		uint16_t damage;
	};

	// WIZ_TARGET_HP: exactly 13 bytes. A shorter packet is not accepted even when hp could be read: the wire
	// layout is fixed and a partial packet could carry a wrong value. maxHp <= 0, hp < 0 or hp > maxHp is
	// rejected as well. Bounds safe; a null buffer returns false without touching 'out'.
	inline bool ParseTargetHp(const uint8_t * data, size_t len, TargetHpMsg & out)
	{
		if (data == nullptr || len < 13)
			return false;

		ByteReader r(data, len);
		out.tid = r.U16();
		out.echo = r.U8();
		out.maxHp = (int32_t)r.U32();
		out.hp = (int32_t)r.U32();
		out.damage = r.U16();
		if (!r.ok())
			return false;

		if (out.maxHp <= 0 || out.hp < 0 || out.hp > out.maxHp)
			return false;

		return true;
	}

	// One stored HP observation of a unit (player or NPC), keyed by the WIZ_TARGET_HP tid.
	struct HpObs
	{
		uint16_t id;
		int32_t  hp, maxHp;
		uint16_t lastDamage;   // damage field of the packet that last touched the record
		uint64_t atMs;         // caller's clock (steady_clock ms) of that packet
		bool     reply;        // true when echo != 0: a reply to the bot's own selection request
	};

	// Copyable table of HP observations, keyed by id. It has no mutex: the caller holds the lock.
	class HpTable
	{
	public:
		HpTable() { Clear(); }

		void Clear() { m_count = 0; }

		int Count() const { return m_count; }

		// Insert or overwrite by id. When a new id arrives and the table is full, the entry with the
		// smallest atMs (oldest) is dropped. Returns true when the record was stored.
		bool Upsert(const HpObs & src)
		{
			int idx = IndexOf(src.id);
			if (idx >= 0)
			{
				m_entries[idx] = src;
				return true;
			}

			if (m_count >= kHpMaxEntries)
			{
				int oldest = 0;
				for (int i = 1; i < m_count; i++)
				{
					if (m_entries[i].atMs < m_entries[oldest].atMs)
						oldest = i;
				}
				m_entries[oldest] = src;
				return true;
			}

			m_entries[m_count] = src;
			m_count++;
			return true;
		}

		// Drops the record of 'id' (death / OUT); the last element closes the gap (order is not preserved).
		void Invalidate(uint16_t id)
		{
			int idx = IndexOf(id);
			if (idx < 0)
				return;
			m_count--;
			if (idx != m_count)
				m_entries[idx] = m_entries[m_count];
		}

		const HpObs * Find(uint16_t id) const
		{
			int idx = IndexOf(id);
			return idx >= 0 ? &m_entries[idx] : nullptr;
		}

	private:
		int IndexOf(uint16_t id) const
		{
			for (int i = 0; i < m_count; i++)
			{
				if (m_entries[i].id == id)
					return i;
			}
			return -1;
		}

		HpObs m_entries[kHpMaxEntries];
		int   m_count;
	};

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
		// The registration / respawn packet seeds the position history with one sample of unknown speed.
		bool Upsert(const UnitObs & src)
		{
			UnitObs u = src;
			u.lastMoveMs = u.lastSeenMs;
			u.lastSpeed = -1;
			memset(u.hist, 0, sizeof(u.hist));
			u.histCount = 1;
			u.histNext = 1 % kObsHistMax;
			u.hist[0].tMs = u.lastSeenMs;
			u.hist[0].x10 = u.x10;
			u.hist[0].z10 = u.z10;
			u.hist[0].speed = -1;

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

		// WIZ_MOVE: updates the position and speed and appends a sample to the history ring. An identical
		// position and speed repeated within kObsSampleMinGapMs of the last sample is not stored again.
		bool UpdateMove(uint16_t sid, uint16_t x10, uint16_t z10, uint16_t y10, int16_t speed, uint64_t nowMs)
		{
			int idx = IndexOf(sid);
			if (idx < 0)
				return false;

			UnitObs & u = m_units[idx];
			u.x10 = x10;
			u.z10 = z10;
			u.y10 = y10;
			u.lastSeenMs = nowMs;
			u.lastMoveMs = nowMs;
			u.lastSpeed = speed;

			const PosSample * last = ObsSampleBack(u, 0);
			if (last != nullptr && last->x10 == x10 && last->z10 == z10 && last->speed == speed
				&& nowMs > last->tMs && nowMs - last->tMs < kObsSampleMinGapMs)
				return true;

			PosSample & s = u.hist[u.histNext];
			s.tMs = nowMs;
			s.x10 = x10;
			s.z10 = z10;
			s.speed = speed;
			u.histNext = (uint8_t)((u.histNext + 1) % kObsHistMax);
			if (u.histCount < kObsHistMax)
				u.histCount++;
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

	// One visible player. The name arrives with WIZ_USER_INOUT (docs/03 section 16). No HP, MP or inventory:
	// the client never learns them (docs/14 5.2). Position metadata is derived from WIZ_MOVE only.
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
		uint32_t ageMs;                // nowMs - lastSeenMs: age of the last packet, NOT position validity (kept as before)
		char     name[kObsNameMax];    // NUL terminated, from the user info record
		uint32_t posAgeMs;             // nowMs - lastMoveMs (0 if the clock is ahead), clamped to 0xFFFFFFFF
		int16_t  speedField;           // speed field of the last WIZ_MOVE; -1 unknown, 0 stationary
		bool     moving;               // speedField > 0; unknown speed (-1, no WIZ_MOVE since registration) counts as stationary: the server sends a WIZ_MOVE for every step of a moving unit
		float    vx, vz;               // estimated velocity in m/s (0 when it cannot be estimated)
		uint8_t  posState;             // POS_FRESH / POS_STALE / POS_LOST
		uint8_t  src;                  // kSrcObserved / kSrcTeam / kSrcEstimate
		bool     hpKnown;              // a WIZ_TARGET_HP observation exists for this id (ADR-0017 Ek F4-51)
		int32_t  hp, maxHp;            // observed target HP; valid only when hpKnown
		uint16_t lastDamage;           // damage field of that packet
		uint32_t hpAgeMs;              // nowMs - atMs of the observation (0 when unknown)
		bool     hpStale;              // hpAgeMs > kHpStaleMs
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
		bool     hpKnown;              // a WIZ_TARGET_HP observation exists for this id (ADR-0017 Ek F4-51)
		int32_t  hp, maxHp;            // observed target HP; valid only when hpKnown
		uint16_t lastDamage;           // damage field of that packet
		uint32_t hpAgeMs;              // nowMs - atMs of the observation (0 when unknown)
		bool     hpStale;              // hpAgeMs > kHpStaleMs
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
			for (uint32_t k = 0; k < kObsNameMax; k++)
				v.name[k] = u.name[k];
			uint64_t age = nowMs > u.lastSeenMs ? nowMs - u.lastSeenMs : 0;
			v.ageMs = age > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)age;
			uint64_t posAge = nowMs > u.lastMoveMs ? nowMs - u.lastMoveMs : 0;
			v.posAgeMs = posAge > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)posAge;
			v.speedField = u.lastSpeed;
			v.moving = (u.lastSpeed > 0);
			EstimateVelocity(u, nowMs, v.vx, v.vz);
			v.posState = ClassifyPos(v.moving, v.posAgeMs);
			v.src = kSrcObserved;

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

	// Attaches the HP observations in 'hp' to every view record already built in 'out'. Player ids are socket
	// ids (< 10000) and NPC ids are >= NPC_BAND (10000), so one table serves both without collisions. A view
	// record with no observation keeps hpKnown = false. BuildSnapshot's signature is unchanged.
	inline void AttachHp(PerceptionSnapshot & out, const HpTable & hp, uint64_t nowMs)
	{
		for (int i = 0; i < out.enemyCount; i++)
		{
			UnitView & v = out.enemies[i];
			const HpObs * o = hp.Find(v.id);
			if (o == nullptr)
				continue;
			v.hpKnown = true;
			v.hp = o->hp;
			v.maxHp = o->maxHp;
			v.lastDamage = o->lastDamage;
			uint64_t age = nowMs > o->atMs ? nowMs - o->atMs : 0;
			v.hpAgeMs = age > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)age;
			v.hpStale = v.hpAgeMs > kHpStaleMs;
		}

		for (int i = 0; i < out.allyCount; i++)
		{
			UnitView & v = out.allies[i];
			const HpObs * o = hp.Find(v.id);
			if (o == nullptr)
				continue;
			v.hpKnown = true;
			v.hp = o->hp;
			v.maxHp = o->maxHp;
			v.lastDamage = o->lastDamage;
			uint64_t age = nowMs > o->atMs ? nowMs - o->atMs : 0;
			v.hpAgeMs = age > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)age;
			v.hpStale = v.hpAgeMs > kHpStaleMs;
		}

		for (int i = 0; i < out.npcCount; i++)
		{
			NpcView & v = out.npcs[i];
			const HpObs * o = hp.Find(v.id);
			if (o == nullptr)
				continue;
			v.hpKnown = true;
			v.hp = o->hp;
			v.maxHp = o->maxHp;
			v.lastDamage = o->lastDamage;
			uint64_t age = nowMs > o->atMs ? nowMs - o->atMs : 0;
			v.hpAgeMs = age > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)age;
			v.hpStale = v.hpAgeMs > kHpStaleMs;
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

	// --- observed skill events (ADR-0017 Ek F4-52) ---

	// WIZ_MAGIC_PROCESS opcodes as the server writes them (shared/packets.h MagicOpcode). BotCore does not include
	// that header (it belongs to the server), so the values are repeated here with the source noted. Only the range
	// matters for the ring: a region broadcast carries CASTING (1), FLYING (2) or EFFECTING (3).
	constexpr uint8_t kMagicCasting   = 1;     // MAGIC_CASTING
	constexpr uint8_t kMagicFlying    = 2;     // MAGIC_FLYING
	constexpr uint8_t kMagicEffecting = 3;     // MAGIC_EFFECTING
	constexpr uint8_t kMagicOpMax     = 13;    // MAGIC_CANCEL2; an op outside 1..13 is rejected
	constexpr uint8_t kSkillOpAny     = 0xFF;  // FindLatest / CountIn wildcard for the opcode
	constexpr int16_t kSkillIdAny     = -1;    // FindLatest / CountIn wildcard for caster / target

	constexpr int kSkillEventRing = 64;        // ring capacity per bot (design limit)

	// One WIZ_MAGIC_PROCESS payload the bot received: u8 opcode, u32 skillId, i16 caster, i16 target, i16 data[7]
	// (MagicInstance.cpp BuildSkillPacket, 23 bytes). data[] is stored raw: its meaning depends on the skill.
	struct SkillEvent
	{
		uint64_t tMs;          // caller's clock (steady_clock ms) of the packet
		uint8_t  op;
		uint32_t skillId;
		int16_t  caster, target;
		int16_t  data[7];
	};

	// WIZ_MAGIC_PROCESS: at least 23 bytes; a shorter packet is rejected (the server broadcast is always 23). Bytes
	// beyond the first 23 are ignored. op must be 1..13. Bounds safe; a null buffer returns false without touching
	// 'out'; nowMs is copied into out.tMs on success.
	inline bool ParseSkillEvent(const uint8_t * data, size_t len, uint64_t nowMs, SkillEvent & out)
	{
		if (data == nullptr || len < 23)
			return false;

		ByteReader r(data, len);
		out.op = r.U8();
		out.skillId = r.U32();
		out.caster = (int16_t)r.U16();
		out.target = (int16_t)r.U16();
		for (int i = 0; i < 7; i++)
			out.data[i] = (int16_t)r.U16();
		if (!r.ok())
			return false;

		if (out.op < 1 || out.op > kMagicOpMax)
			return false;

		out.tMs = nowMs;
		return true;
	}

	// Fixed-size, copyable ring of received skill events. No allocation and no mutex: the caller holds the lock.
	// At(0) is the newest event; Total() counts every Add() including the ones the ring has overwritten.
	class SkillEventRing
	{
	public:
		SkillEventRing() { Clear(); }

		void Clear()
		{
			m_count = 0;
			m_next = 0;
			m_total = 0;
		}

		int Count() const { return m_count; }
		uint32_t Total() const { return m_total; }

		void Add(const SkillEvent & ev)
		{
			m_events[m_next] = ev;
			m_next = (m_next + 1) % kSkillEventRing;
			if (m_count < kSkillEventRing)
				m_count++;
			m_total++;
		}

		// i = 0 is the newest event. The caller must pass 0 <= i < Count().
		const SkillEvent & At(int i) const
		{
			int idx = (int)m_next - 1 - i;
			while (idx < 0)
				idx += kSkillEventRing;
			return m_events[idx];
		}

		// Newest event within the last windowMs that matches op/caster/target (kSkillOpAny / kSkillIdAny are
		// wildcards), or nullptr. Search order: newest first.
		const SkillEvent * FindLatest(uint8_t op, int16_t caster, int16_t target, uint64_t nowMs, uint32_t windowMs) const
		{
			for (int i = 0; i < m_count; i++)
			{
				const SkillEvent & ev = At(i);
				if (op != kSkillOpAny && ev.op != op)
					continue;
				if (caster != kSkillIdAny && ev.caster != caster)
					continue;
				if (target != kSkillIdAny && ev.target != target)
					continue;
				if (!WithinWindow(ev, nowMs, windowMs))
					continue;
				return &ev;
			}
			return nullptr;
		}

		// How many events within the last windowMs match op/target (wildcards as in FindLatest). op has no caster
		// filter here, matching the plan's signature.
		int CountIn(uint8_t op, int16_t target, uint64_t nowMs, uint32_t windowMs) const
		{
			int n = 0;
			for (int i = 0; i < m_count; i++)
			{
				const SkillEvent & ev = At(i);
				if (op != kSkillOpAny && ev.op != op)
					continue;
				if (target != kSkillIdAny && ev.target != target)
					continue;
				if (!WithinWindow(ev, nowMs, windowMs))
					continue;
				n++;
			}
			return n;
		}

	private:
		// An event is inside the window when it is not in the future and its age is at most windowMs.
		static bool WithinWindow(const SkillEvent & ev, uint64_t nowMs, uint32_t windowMs)
		{
			return nowMs >= ev.tMs && nowMs - ev.tMs <= windowMs;
		}

		SkillEvent m_events[kSkillEventRing];
		int        m_count;   // valid events (<= kSkillEventRing)
		int        m_next;    // slot the next Add() writes
		uint32_t   m_total;   // every Add() since Clear()
	};

	// --- observed status and heal observations (ADR-0017 Ek F4-53) ---

	// Skill data value copy. BotCore does not include the server's table structs (structs.h); the caller copies the
	// fields it needs into this POD. The Type3/Type4/Type5 members are meaningful only when the matching type is
	// present in type1/type2.
	constexpr uint8_t kType5RemoveType3 = 1;   // MagicInstance.h REMOVE_TYPE3
	constexpr uint8_t kType5RemoveType4 = 2;   // MagicInstance.h REMOVE_TYPE4
	constexpr int kObsStatusUnits   = 32;      // units tracked per bot (design limit)
	constexpr int kObsStatusPerUnit = 8;       // Type4 records kept per unit
	constexpr int kHealObsRing      = 64;      // heal observations kept per bot

	struct SkillMeta
	{
		uint32_t skillId;
		uint8_t  type1, type2;             // MAGIC.bType[0..1]
		uint8_t  buffType;                 // MAGIC_TYPE4.bBuffType (Type4 part)
		bool     isBuff;                   // MAGIC_TYPE4.bIsBuff (Type4 part)
		uint8_t  directType;               // MAGIC_TYPE3.bDirectType (Type3 part)
		int16_t  firstDamage, timeDamage;  // MAGIC_TYPE3.sFirstDamage / sTimeDamage
		uint8_t  type3DurationSec;         // MAGIC_TYPE3.bDuration
		uint8_t  type5Kind;                // MAGIC_TYPE5.bType (Type5 part)
	};

	// The server broadcasts a Type4 result only when bType[1] == 0 || bType[1] == 4 (MagicInstance.cpp:1862). A
	// {4, 3} skill therefore publishes no Type4 packet; {3, 4} and {1, 4} publish only the Type4 half.
	inline bool SkillSendsType4(const SkillMeta & m)
	{
		return (m.type1 == 4 && m.type2 == 0) || m.type2 == 4;
	}

	// Mirror of the Type3 broadcast condition (MagicInstance.cpp:1598). A {3, 4} skill publishes Type4 only.
	inline bool SkillSendsType3(const SkillMeta & m)
	{
		return (m.type1 == 3 && m.type2 == 0) || m.type2 == 3;
	}

	// Type5 REMOVE_TYPE4 removes the Type4 debuffs of its target (MEC-MAG-19).
	inline bool SkillIsCureDebuff(const SkillMeta & m)
	{
		return m.type1 == 5 && m.type5Kind == kType5RemoveType4;
	}

	// Nominal heal value of a Type3 heal skill: the skill's value, not the effective amount (critical factor, cap
	// and missing HP are unknown). hot is set when the skill also applies a HoT. Only DirectType 1 (HP) heals.
	inline uint32_t SkillHealNominal(const SkillMeta & m, bool & hot)
	{
		hot = false;
		if (!SkillSendsType3(m) || m.directType != 1)
			return 0;

		uint32_t nominal = (m.firstDamage > 0) ? (uint32_t)m.firstDamage : 0;
		hot = (m.type3DurationSec > 0 && m.timeDamage > 0);
		if (hot)
			nominal += (uint32_t)m.timeDamage;
		return nominal;
	}

	// One estimated status record on a unit, derived from an observed Type4 EFFECTING broadcast. src is always
	// kSrcEstimate (E): the server never sends another unit's buff list and it removes a buff at most once per
	// Update(), so the real end can be earlier or later than endMs (design limit, docs/13 section 5.2a).
	struct StatusObs
	{
		uint32_t skillId;
		int16_t  caster;
		uint8_t  buffType;
		bool     isBuff;
		uint8_t  src;
		uint64_t startMs, endMs;
	};

	inline uint32_t StatusRemainingMs(const StatusObs & r, uint64_t nowMs)
	{
		if (r.endMs <= nowMs)
			return 0;
		uint64_t d = r.endMs - nowMs;
		if (d > 0xFFFFFFFFu)
			return 0xFFFFFFFFu;
		return (uint32_t)d;
	}

	enum StatusUpdate
	{
		kStatusIgnored  = 0,
		kStatusRecorded = 1,
		kStatusCured    = 2
	};

	// Copyable table of estimated status records, keyed by target then buffType. No mutex: the caller holds the
	// lock. A pointer returned by Find is valid only until the next mutation.
	class ObservedStatusTable
	{
	private:
		struct Unit
		{
			int16_t   target;
			int       count;
			uint64_t  lastMs;   // latest record/update time of this unit
			StatusObs recs[kObsStatusPerUnit];
		};

	public:
		ObservedStatusTable() { Clear(); }

		void Clear()
		{
			for (int i = 0; i < kObsStatusUnits; i++)
			{
				m_units[i].target = 0;
				m_units[i].count = 0;
				m_units[i].lastMs = 0;
			}
		}

		int Units() const
		{
			int n = 0;
			for (int i = 0; i < kObsStatusUnits; i++)
			{
				if (m_units[i].count > 0)
					n++;
			}
			return n;
		}

		int Records() const
		{
			int n = 0;
			for (int i = 0; i < kObsStatusUnits; i++)
				n += m_units[i].count;
			return n;
		}

		// Feed one received skill event. meta must be the skill's value copy; a null meta ignores the event.
		StatusUpdate Observe(const SkillEvent & ev, const SkillMeta * meta)
		{
			if (meta == nullptr || ev.op != kMagicEffecting || ev.target < 0)
				return kStatusIgnored;

			if (SkillSendsType4(*meta))
			{
				// data[1] = bResult (1 applied, 0 rejected); data[3] = duration seconds. The Type1 half of a pair
				// skill carries 0 / -104 there, so a non-positive duration is not a Type4 record.
				if (ev.data[1] == 0 || ev.data[3] <= 0)
					return kStatusIgnored;

				StatusObs rec;
				rec.skillId  = ev.skillId;
				rec.caster   = ev.caster;
				rec.buffType = meta->buffType;
				rec.isBuff   = meta->isBuff;
				rec.src      = kSrcEstimate;
				rec.startMs  = ev.tMs;
				// The duration comes from the packet, not the table: a scroll buff's stored duration differs.
				rec.endMs    = ev.tMs + (uint64_t)ev.data[3] * 1000;
				Store(ev.target, rec, ev.tMs);
				return kStatusRecorded;
			}

			if (SkillIsCureDebuff(*meta))
			{
				// The cure broadcast is unconditional (data[1] is not read). Only debuffs are removed.
				int u = UnitIndex(ev.target);
				if (u >= 0)
				{
					Unit & unit = m_units[u];
					int w = 0;
					for (int i = 0; i < unit.count; i++)
					{
						if (unit.recs[i].isBuff)
							unit.recs[w++] = unit.recs[i];
					}
					unit.count = w;
					if (w == 0)
					{
						unit.target = 0;
						unit.lastMs = 0;
					}
				}
				return kStatusCured;
			}

			return kStatusIgnored;
		}

		// Live record of (target, buffType), or nullptr. "Live" means endMs > nowMs.
		const StatusObs * Find(int16_t target, uint8_t buffType, uint64_t nowMs) const
		{
			int u = UnitIndex(target);
			if (u < 0)
				return nullptr;
			const Unit & unit = m_units[u];
			for (int i = 0; i < unit.count; i++)
			{
				if (unit.recs[i].buffType != buffType)
					continue;
				if (unit.recs[i].endMs <= nowMs)
					continue;
				return &unit.recs[i];
			}
			return nullptr;
		}

		// Live records of one unit ordered by endMs then buffType ascending; writes at most cap, returns the count.
		int Collect(int16_t target, uint64_t nowMs, StatusObs * out, int cap) const
		{
			if (out == nullptr || cap <= 0)
				return 0;
			int u = UnitIndex(target);
			if (u < 0)
				return 0;
			const Unit & unit = m_units[u];

			StatusObs tmp[kObsStatusPerUnit];
			int n = 0;
			for (int i = 0; i < unit.count; i++)
			{
				if (unit.recs[i].endMs <= nowMs)
					continue;
				tmp[n++] = unit.recs[i];
			}
			for (int i = 1; i < n; i++)
			{
				StatusObs key = tmp[i];
				int j = i - 1;
				while (j >= 0 && (tmp[j].endMs > key.endMs ||
					(tmp[j].endMs == key.endMs && tmp[j].buffType > key.buffType)))
				{
					tmp[j + 1] = tmp[j];
					j--;
				}
				tmp[j + 1] = key;
			}
			int w = 0;
			for (int i = 0; i < n && w < cap; i++)
				out[w++] = tmp[i];
			return w;
		}

		int CountBuffs(int16_t target, uint64_t nowMs) const { return CountBy(target, nowMs, true); }
		int CountDebuffs(int16_t target, uint64_t nowMs) const { return CountBy(target, nowMs, false); }

		// Death / leaving view: MEC-DTH-01 clears all of a unit's records.
		void ClearTarget(int16_t target)
		{
			int u = UnitIndex(target);
			if (u < 0)
				return;
			m_units[u].target = 0;
			m_units[u].count = 0;
			m_units[u].lastMs = 0;
		}

		void Prune(uint64_t nowMs)
		{
			for (int i = 0; i < kObsStatusUnits; i++)
			{
				Unit & unit = m_units[i];
				if (unit.count == 0)
					continue;
				int w = 0;
				for (int j = 0; j < unit.count; j++)
				{
					if (unit.recs[j].endMs > nowMs)
						unit.recs[w++] = unit.recs[j];
				}
				unit.count = w;
				if (w == 0)
				{
					unit.target = 0;
					unit.lastMs = 0;
				}
			}
		}

	private:
		int UnitIndex(int16_t target) const
		{
			for (int i = 0; i < kObsStatusUnits; i++)
			{
				if (m_units[i].count > 0 && m_units[i].target == target)
					return i;
			}
			return -1;
		}

		// Slot for a target: its unit, a free unit, or the unit with the smallest lastMs (tie: low index).
		void Store(int16_t target, const StatusObs & rec, uint64_t tMs)
		{
			int u = UnitIndex(target);
			if (u < 0)
			{
				u = -1;
				for (int i = 0; i < kObsStatusUnits; i++)
				{
					if (m_units[i].count == 0)
					{
						u = i;
						break;
					}
				}
				if (u < 0)
				{
					u = 0;
					for (int i = 1; i < kObsStatusUnits; i++)
					{
						if (m_units[i].lastMs < m_units[u].lastMs)
							u = i;
					}
				}
				m_units[u].target = target;
				m_units[u].count = 0;
				m_units[u].lastMs = 0;
			}

			Unit & unit = m_units[u];

			// Same buffType: overwrite in place. A fresh buff replaces a stale record and a debuff replaces the
			// buff on that buffType (MEC-BUF-02/03).
			for (int i = 0; i < unit.count; i++)
			{
				if (unit.recs[i].buffType == rec.buffType)
				{
					unit.recs[i] = rec;
					if (tMs > unit.lastMs)
						unit.lastMs = tMs;
					return;
				}
			}

			if (unit.count < kObsStatusPerUnit)
			{
				unit.recs[unit.count++] = rec;
			}
			else
			{
				// Evict the record with the smallest endMs (tie: low index).
				int victim = 0;
				for (int i = 1; i < unit.count; i++)
				{
					if (unit.recs[i].endMs < unit.recs[victim].endMs)
						victim = i;
				}
				unit.recs[victim] = rec;
			}

			if (tMs > unit.lastMs)
				unit.lastMs = tMs;
		}

		int CountBy(int16_t target, uint64_t nowMs, bool wantBuff) const
		{
			int u = UnitIndex(target);
			if (u < 0)
				return 0;
			const Unit & unit = m_units[u];
			int n = 0;
			for (int i = 0; i < unit.count; i++)
			{
				if (unit.recs[i].isBuff != wantBuff)
					continue;
				if (unit.recs[i].endMs <= nowMs)
					continue;
				n++;
			}
			return n;
		}

		Unit m_units[kObsStatusUnits];
	};

	// One observed heal event: nominal value only (the effective amount is unknown).
	struct HealObs
	{
		uint64_t tMs;
		uint32_t skillId;
		int16_t  caster, target;
		uint32_t nominal;
		bool     hot;
	};

	// Fixed-size, copyable ring of observed heals. No allocation and no mutex: the caller holds the lock. At(0) is
	// the newest; Total() counts every stored heal.
	class HealObsRing
	{
	public:
		HealObsRing() { Clear(); }

		void Clear()
		{
			m_count = 0;
			m_next = 0;
			m_total = 0;
		}

		int Count() const { return m_count; }
		uint32_t Total() const { return m_total; }

		const HealObs & At(int i) const
		{
			int idx = (int)m_next - 1 - i;
			while (idx < 0)
				idx += kHealObsRing;
			return m_heals[idx];
		}

		// Store a heal event. data[1] is not read (the server leaves it as the caller sent it).
		bool Observe(const SkillEvent & ev, const SkillMeta * meta)
		{
			if (meta == nullptr || ev.op != kMagicEffecting || ev.target < 0)
				return false;

			bool hot = false;
			uint32_t nominal = SkillHealNominal(*meta, hot);
			if (nominal == 0)
				return false;

			HealObs o;
			o.tMs     = ev.tMs;
			o.skillId = ev.skillId;
			o.caster  = ev.caster;
			o.target  = ev.target;
			o.nominal = nominal;
			o.hot     = hot;

			m_heals[m_next] = o;
			m_next = (m_next + 1) % kHealObsRing;
			if (m_count < kHealObsRing)
				m_count++;
			m_total++;
			return true;
		}

		// Sum of nominal heal values for target (kSkillIdAny wildcard) inside the window.
		uint32_t SumNominal(int16_t target, uint64_t nowMs, uint32_t windowMs) const
		{
			uint32_t sum = 0;
			for (int i = 0; i < m_count; i++)
			{
				const HealObs & o = At(i);
				if (target != kSkillIdAny && o.target != target)
					continue;
				if (!WithinWindow(o.tMs, nowMs, windowMs))
					continue;
				sum += o.nominal;
			}
			return sum;
		}

		int CountIn(int16_t target, uint64_t nowMs, uint32_t windowMs) const
		{
			int n = 0;
			for (int i = 0; i < m_count; i++)
			{
				const HealObs & o = At(i);
				if (target != kSkillIdAny && o.target != target)
					continue;
				if (!WithinWindow(o.tMs, nowMs, windowMs))
					continue;
				n++;
			}
			return n;
		}

	private:
		static bool WithinWindow(uint64_t tMs, uint64_t nowMs, uint32_t windowMs)
		{
			return nowMs >= tMs && nowMs - tMs <= windowMs;
		}

		HealObs  m_heals[kHealObsRing];
		int      m_count;
		int      m_next;
		uint32_t m_total;
	};
}
