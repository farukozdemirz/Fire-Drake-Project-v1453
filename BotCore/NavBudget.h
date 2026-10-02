#pragma once

// Per-tick path-query budget, fair queue, replan phase and short-lived path cache (F5-53;
// docs/12 s13.5, AC-NAV-07, ADR-0006) on top of the F5-01 grid, F5-02 A* and F5-04 follower.
// A single A* run is fast (near64 p95 ~0.4 ms) but many bots querying in the same tick blow the
// whole BotManager tick budget; this header is the pure-logic half of the fix and never runs a
// search itself. Pure logic: the standard library only, no server header, no global/static mutable
// state and no clock (every time is a caller-supplied one-way millisecond stamp). The caller runs
// Find() only for the bots the scheduler returns and reports the measured cost back.

#include "NavTrack.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BotCore
{
	class NavQueryScheduler
	{
	public:
		// Fixed storage (no allocation): bot ids are dense slot indices 0..kCapacity-1.
		static constexpr int kCapacity = 128;

		// Ask for one navigation query for `botId`. One pending request per bot: a repeated call
		// keeps the original timestamp (it does not refresh the wait).
		void Request(uint16_t botId, int64_t nowMs)
		{
			if (botId >= kCapacity || m_slot[botId].pending)
				return;
			m_slot[botId].pending = true;
			m_slot[botId].requestedAtMs = nowMs;
			++m_pending;
		}

		// Writes the bots to run this tick into `out` (up to `cap`) and returns the count. Selection
		// order: requests waiting at least `maxWaitMs` first (oldest first), then the rest FIFO
		// (oldest first); among equal keys a rotating rank (id - call counter) decides, so tied bots
		// take turns across calls. A query's cost is estimated from the bot's per-bot EWMA, else the
		// global EWMA, else `initialCostMs`; selection stops at the first estimate that would exceed
		// `budgetMs` (a query is never split). NextBatch does not remove the chosen bots from the
		// queue: the caller must Cancel each one after serving it. Progress guarantee: at least one
		// bot is returned whenever requests are pending, whatever the budget.
		int NextBatch(int64_t nowMs, double budgetMs, uint16_t * out, int cap)
		{
			if (out == nullptr || cap <= 0 || m_pending <= 0)
				return 0;

			// Gather pending bots as (priority, age, id); priority 0 = waited >= maxWaitMs.
			int count = 0;
			for (int id = 0; id < kCapacity; ++id)
			{
				if (!m_slot[id].pending)
					continue;
				const int64_t wait = nowMs - m_slot[id].requestedAtMs;
				m_candId[count] = (uint16_t)id;
				m_candWait[count] = wait;
				m_candOver[count] = (int16_t)((wait >= (int64_t)m_maxWaitMs) ? 1 : 0);
				++count;
			}

			// Order: over-waiters first, then by wait descending (oldest first), then the rotating
			// rank (see CandWorse). A simple stable insertion sort keeps < 128 entries cheap.
			for (int i = 1; i < count; ++i)
			{
				const uint16_t id = m_candId[i];
				const int64_t wait = m_candWait[i];
				const int over = m_candOver[i];
				int j = i - 1;
				while (j >= 0 && CandWorse(m_candId[j], m_candWait[j], m_candOver[j], id, wait, over))
				{
					m_candId[j + 1] = m_candId[j];
					m_candWait[j + 1] = m_candWait[j];
					m_candOver[j + 1] = m_candOver[j];
					--j;
				}
				m_candId[j + 1] = id;
				m_candWait[j + 1] = wait;
				m_candOver[j + 1] = (int16_t)over;
			}

			int chosen = 0;
			double spent = 0.0;
			for (int i = 0; i < count && chosen < cap; ++i)
			{
				const uint16_t botId = m_candId[i];
				const double cost = EstimatedCost(botId);
				if (chosen > 0 && spent + cost > budgetMs)
					break;   // select in priority order until the budget is full
				out[chosen++] = botId;
				spent += cost;
			}

			++m_offset;
			return chosen;
		}

		// Report a completed query so the per-bot and global A* cost estimates follow reality.
		void ReportCost(uint16_t botId, double ms, int expanded)
		{
			(void)expanded;
			if (botId >= kCapacity || ms < 0.0)
				return;
			if (m_slot[botId].hasCost)
				m_slot[botId].ewmaMs = 0.7 * m_slot[botId].ewmaMs + 0.3 * ms;
			else
				m_slot[botId].ewmaMs = ms;
			m_slot[botId].hasCost = true;

			if (m_hasGlobalCost)
				m_globalEwmaMs = 0.7 * m_globalEwmaMs + 0.3 * ms;
			else
			{
				m_globalEwmaMs = ms;
				m_hasGlobalCost = true;
			}
		}

		int Pending() const { return m_pending; }

		// Wait of the oldest pending request; -1 when nothing is pending.
		int64_t OldestWaitMs(int64_t nowMs) const
		{
			if (m_pending <= 0)
				return -1;
			int64_t oldest = -1;
			for (int id = 0; id < kCapacity; ++id)
			{
				if (!m_slot[id].pending)
					continue;
				if (oldest < 0 || m_slot[id].requestedAtMs < oldest)
					oldest = m_slot[id].requestedAtMs;
			}
			return oldest < 0 ? -1 : nowMs - oldest;
		}

		// A bot despawned/died while its query was pending.
		void Cancel(uint16_t botId)
		{
			if (botId >= kCapacity || !m_slot[botId].pending)
				return;
			m_slot[botId].pending = false;
			--m_pending;
		}

		void Clear()
		{
			m_pending = 0;
			m_offset = 0;
			m_hasGlobalCost = false;
			m_globalEwmaMs = 0.0;
			for (int i = 0; i < kCapacity; ++i)
			{
				m_slot[i].pending = false;
				m_slot[i].hasCost = false;
				m_slot[i].ewmaMs = 0.0;
				m_slot[i].requestedAtMs = 0;
			}
		}

		void SetMaxWaitMs(int maxWaitMs) { m_maxWaitMs = maxWaitMs > 0 ? maxWaitMs : 0; }

	private:
		struct Slot
		{
			bool pending = false;
			bool hasCost = false;
			double ewmaMs = 0.0;
			int64_t requestedAtMs = 0;
		};

		double EstimatedCost(uint16_t botId) const
		{
			if (m_slot[botId].hasCost)
				return m_slot[botId].ewmaMs;
			if (m_hasGlobalCost)
				return m_globalEwmaMs;
			return m_initialCostMs;
		}

		// True when candidate a must sort after candidate b: over-waiters first, then older wait,
		// then by rotating rank (so equal entries take turns across NextBatch calls).
		bool CandWorse(uint16_t aId, int64_t aWait, int aOver,
			uint16_t bId, int64_t bWait, int bOver) const
		{
			if (aOver != bOver)
				return aOver < bOver;
			if (aWait != bWait)
				return aWait < bWait;
			return RotationRank(aId) > RotationRank(bId);
		}

		// Rank of a bot for equal-key ordering: (id - m_offset) mod kCapacity, ascending. Only the
		// tie-break uses it, so a whole run of equal entries is never rotated as a block.
		int RotationRank(uint16_t id) const
		{
			int r = (int)id - m_offset;
			r %= kCapacity;
			if (r < 0)
				r += kCapacity;
			return r;
		}

		static constexpr double m_initialCostMs = 0.5;   // P-NAV-TICK-BUDGET-MS: cold-start estimate

		Slot m_slot[kCapacity];
		uint16_t m_candId[kCapacity] = {};
		int64_t m_candWait[kCapacity] = {};
		int16_t m_candOver[kCapacity] = {};
		int m_pending = 0;
		int m_offset = 0;
		int m_maxWaitMs = 1000;             // P-NAV-MAX-WAIT [O] docs/12 s13.5
		bool m_hasGlobalCost = false;
		double m_globalEwmaMs = 0.0;
	};

	// Key of one cached route: start/goal cells plus the cost-field version it was computed for.
	struct NavCacheKey
	{
		int startX = 0;
		int startZ = 0;
		int goalX = 0;
		int goalZ = 0;
		int fieldVersion = 0;
	};
	inline bool operator==(const NavCacheKey & a, const NavCacheKey & b)
	{
		return a.startX == b.startX && a.startZ == b.startZ && a.goalX == b.goalX
			&& a.goalZ == b.goalZ && a.fieldVersion == b.fieldVersion;
	}

	class NavPathCache
	{
	public:
		static constexpr int kCapacity = 64;
		static constexpr int kMaxCells = 512;   // longer routes are never stored

		void SetTtlMs(int ttlMs) { m_ttlMs = ttlMs > 0 ? ttlMs : 0; }

		// True and fills `cells`/`cost`/`length` when a fresh entry exists; refreshes LRU age.
		// `cells` is fully overwritten. A version mismatch is a miss (see Invalidate).
		bool Find(const NavCacheKey & key, int64_t nowMs, std::vector<NavCell> & cells,
			float & cost, float & length)
		{
			const int index = IndexOf(key);
			if (index < 0)
				return false;
			Entry & entry = m_entries[index];
			if (nowMs - entry.storedAtMs > (int64_t)m_ttlMs)
			{
				Remove(index);
				return false;
			}

			cells = entry.cells;
			cost = entry.cost;
			length = entry.length;
			entry.lastUsedMs = nowMs;
			return true;
		}

		// Stores (or replaces) a route. Routes longer than kMaxCells are rejected. `cells` is
		// copied, so the caller's buffer may change afterwards.
		void Put(const NavCacheKey & key, const std::vector<NavCell> & cells, float cost, float length, int64_t nowMs)
		{
			if ((int)cells.size() > kMaxCells)
				return;

			const int existing = IndexOf(key);
			int index = existing;
			if (index < 0)
			{
				if (m_count < kCapacity)
				{
					index = m_count++;
				}
				else
				{
					index = 0;
					for (int i = 1; i < m_count; ++i)
					{
						if (m_entries[i].lastUsedMs < m_entries[index].lastUsedMs)
							index = i;
					}
				}
			}

			Entry & entry = m_entries[index];
			entry.key = key;
			entry.cells = cells;
			entry.cost = cost;
			entry.length = length;
			entry.storedAtMs = nowMs;
			entry.lastUsedMs = nowMs;
		}

		// Drop every entry computed for a different cost-field version.
		void Invalidate(int fieldVersion)
		{
			for (int i = m_count - 1; i >= 0; --i)
			{
				if (m_entries[i].key.fieldVersion != fieldVersion)
					Remove(i);
			}
		}

		int Count() const { return m_count; }

		void Clear() { m_count = 0; }

	private:
		struct Entry
		{
			NavCacheKey key;
			std::vector<NavCell> cells;
			float cost = 0.0f;
			float length = 0.0f;
			int64_t storedAtMs = 0;
			int64_t lastUsedMs = 0;
		};

		int IndexOf(const NavCacheKey & key) const
		{
			for (int i = 0; i < m_count; ++i)
			{
				if (m_entries[i].key == key)
					return i;
			}
			return -1;
		}

		void Remove(int index)
		{
			for (int i = index; i + 1 < m_count; ++i)
				m_entries[i] = m_entries[i + 1];
			--m_count;
		}

		int m_ttlMs = 30000;   // P-NAV-CACHE-TTL [O] docs/12 s13.5

		Entry m_entries[kCapacity];
		int m_count = 0;
	};

	// Stagger a bot's replanning so 500 ms intervals do not all land in the same tick: phases
	// spread over `intervalMs` in steps of intervalMs / phases. Negative slots are safe.
	inline int NavReplanPhaseMs(int slot, int intervalMs = 500, int phases = 5)
	{
		if (phases < 1)
			phases = 1;
		int s = slot % phases;
		if (s < 0)
			s += phases;
		return s * (intervalMs / phases);
	}

	// What a bot whose query is deferred (the tick budget is full) should do. The caller applies
	// this contract; the scheduler only hands out the queries.
	enum class NavDeferAction { FollowPlan, Hold };

	struct NavDeferParams
	{
		int   planMaxAgeMs = 5000;   // [A]
		float driftMaxM = 15.0f;     // [A]
	};

	// Fresh plan -> keep following it (no stop, no straight-line step into the target). A stale
	// plan (too old, or the target drifted too far) or no plan at all -> Hold (stop and wait).
	inline NavDeferAction NavWhileDeferred(bool hasPlan, int64_t planAgeMs, float targetDriftM,
		const NavDeferParams & params)
	{
		if (!hasPlan)
			return NavDeferAction::Hold;
		if (planAgeMs > (int64_t)params.planMaxAgeMs)
			return NavDeferAction::Hold;
		if (targetDriftM > params.driftMaxM)
			return NavDeferAction::Hold;
		return NavDeferAction::FollowPlan;
	}
}
