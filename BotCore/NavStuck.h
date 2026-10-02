#pragma once

// Stuck detection and staged recovery (F5-09; docs/12 s10, ADR-0006 Ek F5-09) on top of the F5-01
// grid, F5-02 A*, F5-03 smoothing, F5-04 ring cells and F5-06 cost layers. Four independent
// tools: NavStuckDetector (no-progress / oscillation), NavStuckMonitor (recovery ladder state
// machine), NavPickSideStep (stage-2 target) and NavStuckPenalties (stage-4 cell penalty). Pure
// logic: the standard library only, no server header, no global/static mutable state and no clock
// (every time is a caller-supplied one-way millisecond stamp). The tools never execute an action:
// the monitor returns "do this now" and the caller moves the bot, keeps the previous waypoint and
// abandons the target. Out of scope here (the caller owns them): telemetry, heat map, team
// notification, TEST_TELEPORT, the NavFollower/ActionExecutor binding and the route-progress
// measure.

#include "NavTrack.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BotCore
{
	enum class NavStuckKind { None, NoProgress, Oscillation };

	struct NavStuckParams
	{
		int   noProgressMs = 1500;        // [O] docs/12 s10; <= 0 disables the no-progress rule
		float minProgressM = 1.0f;        // [O] docs/12 s10; <= 0 disables the no-progress rule
		int   oscWindowMs = 4000;         // [O] docs/12 s10; <= 0 disables the oscillation rule
		int   oscSwings = 3;              // [O] docs/12 s10; <= 0 disables the oscillation rule
		float resumeM = 1.0f;             // [A] recovery succeeded: displacement from the stage-entry position
		int   escalateWindowMs = 10000;   // [A] a new detection this soon after a recovery climbs the ladder
		int   stageMs[4] = { 500, 1000, 1500, 1000 };   // [O] docs/12 s10 stages 1..4
		float sideStepMaxM = 12.0f;       // [A]
		int   sideStepClearance = 2;      // [O] docs/12 s10 stage 2
		int   penaltyMs = 60000;          // [O] docs/12 s10 stage 4; <= 0: NavStuckPenalties::Add is a no-op
		float penaltyWeight = 1.0f;       // [A] danger weight 0..1 (1.0 = 255)
	};

	class NavStuckDetector
	{
	public:
		static constexpr int kCapacity = 256;     // samples; covers 4 s at the 20 ms minimum spacing
		static constexpr int kMinSpacingMs = 20;

		void Reset();                             // forget all samples
		int  Count() const;                       // stored samples
		// Feeds one sample and returns the verdict for the window ending at tMs.
		NavStuckKind Observe(const NavGrid & grid, int64_t tMs, float x, float z, bool moving,
			const NavStuckParams & params);

	private:
		struct Sample
		{
			int64_t t = 0;
			float x = 0.0f;
			float z = 0.0f;
			int16_t cellX = 0;
			int16_t cellZ = 0;
		};

		struct Transition
		{
			int16_t ax = 0;
			int16_t az = 0;
			int16_t bx = 0;
			int16_t bz = 0;
		};

		static bool SameTransition(const Transition & a, const Transition & b);
		const Sample & At(int backFromNewest) const;

		Sample m_samples[kCapacity];
		int m_count = 0;
		int m_next = 0;                           // next write slot
	};

	enum class NavRecoveryAction { None, Replan, SideStep, StepBack, PenalizeReplan, Abandon };

	struct NavRecoveryStep
	{
		NavRecoveryAction action = NavRecoveryAction::None;  // what the caller must do NOW (once per stage entry)
		int stage = 0;                       // 1..5 when action != None, else 0 (5 = Abandon)
		NavStuckKind kind = NavStuckKind::None;   // reason of the episode; None when action == None
		int cellX = 0;                       // bot cell at the moment of the action (the stuck cell); 0 when action == None
		int cellZ = 0;
		bool recovered = false;              // movement resumed during this call (never together with an action)
		int  recoveredStage = 0;             // 1..4 when recovered
		int  recoverMs = 0;                  // detection -> recovery (MET-NAV-02), when recovered
	};

	class NavStuckMonitor
	{
	public:
		void Reset();                        // everything: detector, stage 0, time gate, memory, counters
		NavRecoveryStep Update(const NavGrid & grid, int64_t tMs, float x, float z, bool moving,
			const NavStuckParams & params);
		int Stage() const { return m_stage; }          // 0 = not recovering, 1..4 = ladder stage in progress
		int Episodes() const { return m_episodes; }    // detections since Reset (MET-NAV-01 raw count)
		int Abandons() const { return m_abandons; }    // stage-5 results since Reset

		// Caller contract (comment only): an action is returned once at stage entry; the caller keeps
		// passing moving = true while it executes the action; after Abandon the caller drops the target.

	private:
		NavRecoveryStep Enter(const NavGrid & grid, int64_t tMs, float x, float z, int stage);

		NavStuckDetector m_detector;
		int m_stage = 0;
		bool m_hasLastTime = false;
		int64_t m_lastTime = 0;
		int m_episodes = 0;
		int m_abandons = 0;
		int64_t m_stuckAt = 0;
		NavStuckKind m_episodeKind = NavStuckKind::None;
		int64_t m_stageEntryMs = 0;
		float m_entryX = 0.0f;
		float m_entryZ = 0.0f;
		int m_lastRecoveryStage = 0;
		int64_t m_lastRecoveryMs = 0;
	};

	// Stage-2 target: nearest cell (see rules) to step aside to.
	inline bool NavPickSideStep(const NavGrid & grid, float x, float z, float hx, float hz,
		const NavStuckParams & params, std::vector<NavCell> & scratch, NavCell & out)
	{
		NavCell from;
		from.x = grid.CellOf(x);
		from.z = grid.CellOf(z);
		if (!grid.Walk(from.x, from.z))
			return false;

		NavRingCells(grid, x, z, 0.0f, params.sideStepMaxM, from, scratch);

		const float h2 = hx * hx + hz * hz;
		for (size_t i = 0; i < scratch.size(); ++i)
		{
			const NavCell & candidate = scratch[i];
			if (candidate == from)
				continue;
			if ((int)grid.Clearance(candidate.x, candidate.z) < params.sideStepClearance)
				continue;

			if (h2 > 0.0f)
			{
				const float dx = (float)(candidate.x - from.x);
				const float dz = (float)(candidate.z - from.z);
				const float dot = dx * hx + dz * hz;
				if (dot * dot > 0.5f * (dx * dx + dz * dz) * h2)
					continue;   // inside +-45 degrees of the walking direction
			}

			if (!NavLineClear(grid, from, candidate))
				continue;

			out = candidate;
			return true;
		}
		return false;
	}

	class NavStuckPenalties
	{
	public:
		static constexpr int kCapacity = 32;

		void Clear();
		void Add(int x, int z, int64_t nowMs, const NavStuckParams & params);
		int  Count(int64_t nowMs) const;                       // active entries
		bool Active(int x, int z, int64_t nowMs) const;
		void Apply(const NavGrid & grid, int64_t nowMs, const NavStuckParams & params, NavCostLayer & layer) const;

	private:
		struct Entry
		{
			int x = 0;
			int z = 0;
			int64_t expiryMs = 0;
		};

		Entry m_entries[kCapacity];
		int m_count = 0;
	};

	inline void NavStuckDetector::Reset()
	{
		m_count = 0;
		m_next = 0;
	}

	inline int NavStuckDetector::Count() const
	{
		return m_count;
	}

	inline const NavStuckDetector::Sample & NavStuckDetector::At(int backFromNewest) const
	{
		const int idx = (m_next - 1 - backFromNewest + 2 * kCapacity) % kCapacity;
		return m_samples[idx];
	}

	inline bool NavStuckDetector::SameTransition(const Transition & a, const Transition & b)
	{
		const bool forward = a.ax == b.ax && a.az == b.az && a.bx == b.bx && a.bz == b.bz;
		const bool reverse = a.ax == b.bx && a.az == b.bz && a.bx == b.ax && a.bz == b.az;
		return forward || reverse;
	}

	inline NavStuckKind NavStuckDetector::Observe(const NavGrid & grid, int64_t tMs, float x, float z,
		bool moving, const NavStuckParams & params)
	{
		if (!moving)
		{
			Reset();
			return NavStuckKind::None;
		}

		if (m_count > 0 && tMs < At(0).t + (int64_t)kMinSpacingMs)
			return NavStuckKind::None;

		Sample & slot = m_samples[m_next];
		slot.t = tMs;
		slot.x = x;
		slot.z = z;
		slot.cellX = (int16_t)grid.CellOf(x);
		slot.cellZ = (int16_t)grid.CellOf(z);
		m_next = (m_next + 1) % kCapacity;
		if (m_count < kCapacity)
			++m_count;

		NavStuckKind kind = NavStuckKind::None;

		if (params.noProgressMs > 0 && params.minProgressM > 0.0f)
		{
			const int64_t threshold = tMs - (int64_t)params.noProgressMs;
			for (int back = 0; back < m_count; ++back)
			{
				const Sample & anchor = At(back);
				if (anchor.t <= threshold)
				{
					const float dx = x - anchor.x;
					const float dz = z - anchor.z;
					if (dx * dx + dz * dz < params.minProgressM * params.minProgressM)
						kind = NavStuckKind::NoProgress;
					break;   // the newest sample at or before the threshold
				}
			}
		}

		if (kind == NavStuckKind::None && params.oscSwings > 0 && params.oscWindowMs > 0)
		{
			const int64_t windowLo = tMs - (int64_t)params.oscWindowMs;
			Transition transitions[kCapacity];
			int count = 0;
			for (int back = m_count - 1; back >= 1; --back)
			{
				const Sample & older = At(back);
				if (older.t < windowLo)
					continue;
				const Sample & newer = At(back - 1);
				if (older.cellX != newer.cellX || older.cellZ != newer.cellZ)
				{
					transitions[count].ax = older.cellX;
					transitions[count].az = older.cellZ;
					transitions[count].bx = newer.cellX;
					transitions[count].bz = newer.cellZ;
					++count;
				}
			}

			for (int i = 0; i < count && kind == NavStuckKind::None; ++i)
			{
				int matches = 0;
				for (int j = 0; j < count; ++j)
				{
					if (SameTransition(transitions[i], transitions[j]))
						++matches;
				}
				if (matches >= params.oscSwings)
					kind = NavStuckKind::Oscillation;
			}
		}

		return kind;
	}

	inline void NavStuckMonitor::Reset()
	{
		m_detector.Reset();
		m_stage = 0;
		m_hasLastTime = false;
		m_lastTime = 0;
		m_episodes = 0;
		m_abandons = 0;
		m_stuckAt = 0;
		m_episodeKind = NavStuckKind::None;
		m_stageEntryMs = 0;
		m_entryX = 0.0f;
		m_entryZ = 0.0f;
		m_lastRecoveryStage = 0;
		m_lastRecoveryMs = 0;
	}

	inline NavRecoveryStep NavStuckMonitor::Update(const NavGrid & grid, int64_t tMs, float x, float z,
		bool moving, const NavStuckParams & params)
	{
		NavRecoveryStep step;

		// Time gate: the monitor only moves forward.
		if (m_hasLastTime && tMs <= m_lastTime)
			return step;
		m_lastTime = tMs;
		m_hasLastTime = true;

		// Stopped (cast, wait): not stuck. The running recovery is cancelled; the recovery memory
		// is intentionally kept.
		if (!moving)
		{
			m_detector.Reset();
			m_stage = 0;
			return step;
		}

		if (m_stage == 0)
		{
			const NavStuckKind kind = m_detector.Observe(grid, tMs, x, z, true, params);
			if (kind == NavStuckKind::None)
				return step;

			++m_episodes;
			m_stuckAt = tMs;
			m_episodeKind = kind;

			int first = 1;
			if (m_lastRecoveryStage > 0
				&& tMs - m_lastRecoveryMs <= (int64_t)params.escalateWindowMs)
			{
				first = m_lastRecoveryStage + 1;
			}
			return Enter(grid, tMs, x, z, first);
		}

		// Recovery succeeded: displacement from the stage-entry position.
		const float dx = x - m_entryX;
		const float dz = z - m_entryZ;
		if (dx * dx + dz * dz >= params.resumeM * params.resumeM)
		{
			step.recovered = true;
			step.recoveredStage = m_stage;
			step.recoverMs = (int)(tMs - m_stuckAt);
			m_lastRecoveryStage = m_stage;
			m_lastRecoveryMs = tMs;
			m_stage = 0;
			m_detector.Reset();
			m_detector.Observe(grid, tMs, x, z, true, params);   // seed a fresh window
			return step;
		}

		if (tMs - m_stageEntryMs >= (int64_t)params.stageMs[m_stage - 1])
			return Enter(grid, tMs, x, z, m_stage + 1);

		return step;
	}

	inline NavRecoveryStep NavStuckMonitor::Enter(const NavGrid & grid, int64_t tMs, float x, float z,
		int stage)
	{
		NavRecoveryStep step;
		step.kind = m_episodeKind;
		step.cellX = grid.CellOf(x);
		step.cellZ = grid.CellOf(z);
		step.stage = stage;

		if (stage <= 4)
		{
			static constexpr NavRecoveryAction kAction[4] = {
				NavRecoveryAction::Replan, NavRecoveryAction::SideStep,
				NavRecoveryAction::StepBack, NavRecoveryAction::PenalizeReplan
			};
			step.action = kAction[stage - 1];
			m_stage = stage;
			m_stageEntryMs = tMs;
			m_entryX = x;
			m_entryZ = z;
		}
		else
		{
			step.action = NavRecoveryAction::Abandon;
			step.stage = 5;
			++m_abandons;
			m_stage = 0;
			m_detector.Reset();
			m_lastRecoveryStage = 0;
			m_lastRecoveryMs = 0;
		}
		return step;
	}

	inline void NavStuckPenalties::Clear()
	{
		m_count = 0;
	}

	inline void NavStuckPenalties::Add(int x, int z, int64_t nowMs, const NavStuckParams & params)
	{
		if (params.penaltyMs <= 0)
			return;

		const int64_t expiry = nowMs + (int64_t)params.penaltyMs;

		for (int i = 0; i < m_count; ++i)
		{
			if (m_entries[i].x == x && m_entries[i].z == z)
			{
				if (expiry > m_entries[i].expiryMs)
					m_entries[i].expiryMs = expiry;
				return;
			}
		}

		if (m_count < kCapacity)
		{
			m_entries[m_count].x = x;
			m_entries[m_count].z = z;
			m_entries[m_count].expiryMs = expiry;
			++m_count;
			return;
		}

		int earliest = 0;
		for (int i = 1; i < kCapacity; ++i)
		{
			if (m_entries[i].expiryMs < m_entries[earliest].expiryMs)
				earliest = i;
		}
		m_entries[earliest].x = x;
		m_entries[earliest].z = z;
		m_entries[earliest].expiryMs = expiry;
	}

	inline int NavStuckPenalties::Count(int64_t nowMs) const
	{
		int count = 0;
		for (int i = 0; i < m_count; ++i)
		{
			if (nowMs < m_entries[i].expiryMs)
				++count;
		}
		return count;
	}

	inline bool NavStuckPenalties::Active(int x, int z, int64_t nowMs) const
	{
		for (int i = 0; i < m_count; ++i)
		{
			if (m_entries[i].x == x && m_entries[i].z == z && nowMs < m_entries[i].expiryMs)
				return true;
		}
		return false;
	}

	inline void NavStuckPenalties::Apply(const NavGrid & grid, int64_t nowMs, const NavStuckParams & params,
		NavCostLayer & layer) const
	{
		if (layer.Size() != grid.Size())
			return;

		for (int i = 0; i < m_count; ++i)
		{
			if (nowMs >= m_entries[i].expiryMs)
				continue;
			layer.AddDangerBand(grid.CellCenter(m_entries[i].x), grid.CellCenter(m_entries[i].z),
				0.0f, 0.5f, 0.0f, params.penaltyWeight);
		}
	}

	// Packet-cadence preset for the real binding (F5-54; docs/12 s13.3). The server position only
	// changes per move packet (~1500 ms, BotMotion.h kMovePeriodMs), so the default 1500 ms
	// no-progress window has zero margin and the 4 s / 3-swing oscillation rule is unreachable.
	// The thresholds are >= 2 packet periods plus tolerance (docs/12 s13.3, [A]; T-NAV-04 revises).
	inline NavStuckParams NavPacketCadenceParams()
	{
		NavStuckParams params;
		params.noProgressMs = 3200;   // >= 2 packet periods + 100 ms tolerance
		params.minProgressM = 1.0f;
		params.oscWindowMs = 8000;
		params.oscSwings = 3;
		return params;
	}

	// Guard-blocked detector (F5-54): a rejected move packet with no sent packet inside the window
	// is BLOCKED_BY_GUARD, not a stuck episode (docs/12 s13.3). The caller keeps this out of the
	// NavStuckMonitor ladder. Fixed storage, no allocation, no clock: one-way caller timestamps.
	class NavGuardBlockDetector
	{
	public:
		enum { kCapacity = 16 };   // >= 16 events

		void Reset();
		void OnPacketSent(int64_t tMs);
		void OnPacketRejected(int64_t tMs);
		// True when at least one packet was rejected and no packet was sent within the last
		// windowMs (inclusive lower bound). False when there is no event or time went backwards.
		bool Blocked(int64_t nowMs, int windowMs = 3200) const;

	private:
		struct Entry
		{
			int64_t t = 0;
			bool rejected = false;
		};

		Entry m_entries[kCapacity];
		int m_count = 0;      // valid entries (<= kCapacity)
		int m_next = 0;       // next write slot
		int64_t m_lastMs = 0; // last recorded timestamp (call order)
		bool m_hasLast = false;
	};

	inline void NavGuardBlockDetector::Reset()
	{
		for (int i = 0; i < kCapacity; ++i)
		{
			m_entries[i].t = 0;
			m_entries[i].rejected = false;
		}
		m_count = 0;
		m_next = 0;
		m_lastMs = 0;
		m_hasLast = false;
	}

	inline void NavGuardBlockDetector::OnPacketSent(int64_t tMs)
	{
		m_entries[m_next].t = tMs;
		m_entries[m_next].rejected = false;
		m_next = (m_next + 1) % kCapacity;
		if (m_count < kCapacity)
			++m_count;
		m_lastMs = tMs;
		m_hasLast = true;
	}

	inline void NavGuardBlockDetector::OnPacketRejected(int64_t tMs)
	{
		m_entries[m_next].t = tMs;
		m_entries[m_next].rejected = true;
		m_next = (m_next + 1) % kCapacity;
		if (m_count < kCapacity)
			++m_count;
		m_lastMs = tMs;
		m_hasLast = true;
	}

	inline bool NavGuardBlockDetector::Blocked(int64_t nowMs, int windowMs) const
	{
		if (!m_hasLast || nowMs < m_lastMs)
			return false;

		const int64_t lo = nowMs - (int64_t)windowMs;
		bool rejected = false;
		bool sent = false;
		for (int i = 0; i < m_count; ++i)
		{
			if (m_entries[i].t < lo || m_entries[i].t > nowMs)
				continue;
			if (m_entries[i].rejected)
				rejected = true;
			else
				sent = true;
		}
		return rejected && !sent;
	}
}
