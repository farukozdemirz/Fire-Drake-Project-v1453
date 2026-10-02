#pragma once

// Reachability judgement for navigation targets (F5-05; docs/12 s4.3, ADR-0006 Ek F5-05) on top of
// the F5-01 grid, the F5-02 A* and the F5-04 follower. Pure logic: the standard library only, no
// server header, no global/static state and no clock (every time is a caller-supplied one-way
// millisecond stamp). NavReach labels the EdgeOpen-connected components of the Walk cells, so that
// "different component" is known without running A*. NavReachJudge turns a NavFollowPlan into
// Reachable / Unreachable / Unknown / None, and NavUnreachTracker holds the uninterrupted
// Unreachable streak. Dropping a target, the TARGET_UNREACHABLE event, team notification and a new
// target choice belong to the decision layer (F6/F7), never to BotCore.

#include "NavTrack.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BotCore
{
	// Edge-connected components of Walk cells (NavGrid::EdgeOpen, 8 neighbours). Two cells are in
	// different components exactly when NavPathfinder reports NoPath between them.
	class NavReach
	{
	public:
		// Labels the components of `grid` (iterative, uses an explicit stack). Ids are assigned in
		// scan order (x ascending, then z ascending) of each component's first cell. Replaces any
		// previous result. A grid that was not Init'ed/Built yields Size() == 0 / no components.
		// Must be called again after NavGrid::Build with other params.
		void Build(const NavGrid & grid);

		int Size() const;                        // grid side at Build time (0 before)
		int ComponentCount() const;              // 0 when there is no Walk cell
		int ComponentCells(int id) const;        // 0 for an invalid id
		int LargestComponent() const;            // id of the largest (first on ties); -1 if none
		// -1 for off-grid, non-Walk cells and before Build.
		int ComponentOf(int x, int z) const;
		// True when both cells are Walk and in the same component.
		bool Connected(NavCell a, NavCell b) const;

	private:
		int m_n = 0;
		std::vector<int32_t> m_labels;   // n*n; -1 = off-grid / not Walk / unlabelled
		std::vector<int> m_sizes;
		std::vector<int32_t> m_stack;    // reused Build scratch
	};

	enum class NavReachVerdict
	{
		None,           // no plan yet (NavFollowStatus::NoTarget)
		Reachable,      // planned, no detour
		Unknown,        // A* node limit / first tries failed while a ring cell is connected, or the bot cell is not Walk
		Unreachable     // provable (see NavUnreachReason)
	};

	enum class NavUnreachReason
	{
		None,           // verdict != Unreachable
		NoWalkable,     // the ring holds no Walk cell (NavFollowStatus::NoGoal)
		Component,      // no ring cell shares the bot's component
		Detour          // planned, but A* cost > detourFactor * straight and > detourMinM
	};

	struct NavUnreachParams
	{
		float detourFactor = 3.0f;   // docs/12 s4.3 [O]; <= 0 disables the Detour rule
		float detourMinM = 120.0f;   // docs/12 s4.3 [O]
		int   holdMs = 1500;         // [A] ADR-0006 Ek F5-05; MET-NAV-04 budget is 3000 ms
	};

	struct NavReachJudgement
	{
		NavReachVerdict verdict = NavReachVerdict::None;
		NavUnreachReason reason = NavUnreachReason::None;
		int   ringCells = 0;       // Walk cells in the plan's ring (0 when not computed)
		int   ringConnected = 0;   // of those, in the bot's component
		float straightM = 0.0f;    // Planned only: bot cell centre -> goal cell centre
		float pathM = 0.0f;        // Planned only: NavFollowPlan::pathCost
	};

	class NavReachJudge
	{
	public:
		// `plan` and `follow` must be the plan just returned by NavFollower::Update and the
		// params it was called with; botX/botZ the bot position of that call (metres).
		NavReachJudgement Judge(const NavGrid & grid, const NavReach & reach, const NavFollowPlan & plan,
			const NavFollowParams & follow, const NavUnreachParams & params, float botX, float botZ);

	private:
		std::vector<NavCell> m_ring;   // reused, no per-call allocation after warm-up
	};

	class NavUnreachTracker
	{
	public:
		void Reset();
		// Feed one judgement (call whenever NavFollower::Update returned true). Unreachable starts
		// (or continues) the streak; every other verdict (Reachable, Unknown, None) clears it.
		void Feed(int64_t nowMs, const NavReachJudgement & judgement);
		bool Active() const;                 // an Unreachable streak is running
		int64_t SinceMs() const;             // time of the first judgement of the streak; 0 when !Active()
		NavUnreachReason Reason() const;     // reason of the latest Unreachable judgement; None when !Active()
		// Active() && nowMs - SinceMs() >= holdMs (holdMs <= 0: due as soon as Active()).
		bool Due(int64_t nowMs, int holdMs) const;

	private:
		bool m_active = false;
		int64_t m_since = 0;
		NavUnreachReason m_reason = NavUnreachReason::None;
	};

	inline void NavReach::Build(const NavGrid & grid)
	{
		const int n = grid.Size();
		if (n < 2)
		{
			m_n = 0;
			m_labels.clear();
			m_sizes.clear();
			return;
		}

		m_n = n;
		const size_t cells = (size_t)n * (size_t)n;
		m_labels.assign(cells, -1);
		m_sizes.clear();

		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				const int start = x * n + z;
				if (!grid.Walk(x, z) || m_labels[(size_t)start] >= 0)
					continue;

				const int label = (int)m_sizes.size();
				m_sizes.push_back(0);

				m_stack.clear();
				m_stack.push_back(start);
				m_labels[(size_t)start] = label;

				while (!m_stack.empty())
				{
					const int cur = m_stack.back();
					m_stack.pop_back();
					const int cx = cur / n;
					const int cz = cur % n;
					m_sizes[(size_t)label] += 1;

					for (int dx = -1; dx <= 1; ++dx)
					{
						for (int dz = -1; dz <= 1; ++dz)
						{
							if (dx == 0 && dz == 0)
								continue;
							if (!grid.EdgeOpen(cx, cz, dx, dz))
								continue;
							const int ni = (cx + dx) * n + (cz + dz);
							if (m_labels[(size_t)ni] < 0)
							{
								m_labels[(size_t)ni] = label;
								m_stack.push_back(ni);
							}
						}
					}
				}
			}
		}
	}

	inline int NavReach::Size() const
	{
		return m_n;
	}

	inline int NavReach::ComponentCount() const
	{
		return (int)m_sizes.size();
	}

	inline int NavReach::ComponentCells(int id) const
	{
		if (id < 0 || id >= (int)m_sizes.size())
			return 0;
		return m_sizes[(size_t)id];
	}

	inline int NavReach::LargestComponent() const
	{
		int best = -1;
		int bestSize = 0;
		for (size_t i = 0; i < m_sizes.size(); ++i)
		{
			if (m_sizes[i] > bestSize)
			{
				bestSize = m_sizes[i];
				best = (int)i;
			}
		}
		return best;
	}

	inline int NavReach::ComponentOf(int x, int z) const
	{
		if (m_n <= 0 || x < 0 || x >= m_n || z < 0 || z >= m_n)
			return -1;
		return m_labels[(size_t)(x * m_n + z)];
	}

	inline bool NavReach::Connected(NavCell a, NavCell b) const
	{
		const int ca = ComponentOf(a.x, a.z);
		const int cb = ComponentOf(b.x, b.z);
		return ca >= 0 && ca == cb;
	}

	inline NavReachJudgement NavReachJudge::Judge(const NavGrid & grid, const NavReach & reach,
		const NavFollowPlan & plan, const NavFollowParams & follow, const NavUnreachParams & params,
		float botX, float botZ)
	{
		NavReachJudgement judgement;

		if (plan.status == NavFollowStatus::NoTarget)
			return judgement;   // None

		NavCell botCell;
		botCell.x = grid.CellOf(botX);
		botCell.z = grid.CellOf(botZ);
		const int botComp = reach.ComponentOf(botCell.x, botCell.z);

		if (plan.status == NavFollowStatus::InvalidStart || botComp < 0)
		{
			judgement.verdict = NavReachVerdict::Unknown;
			return judgement;
		}

		if (plan.status == NavFollowStatus::NoGoal)
		{
			judgement.verdict = NavReachVerdict::Unreachable;
			judgement.reason = NavUnreachReason::NoWalkable;
			return judgement;
		}

		NavRingCells(grid, plan.predX, plan.predZ, follow.ringMinM, follow.ringMaxM, botCell, m_ring);
		judgement.ringCells = (int)m_ring.size();
		for (size_t i = 0; i < m_ring.size(); ++i)
		{
			if (reach.ComponentOf(m_ring[i].x, m_ring[i].z) == botComp)
				++judgement.ringConnected;
		}

		if (judgement.ringCells == 0)
		{
			judgement.verdict = NavReachVerdict::Unreachable;
			judgement.reason = NavUnreachReason::NoWalkable;
			return judgement;
		}

		if (judgement.ringConnected == 0)
		{
			judgement.verdict = NavReachVerdict::Unreachable;
			judgement.reason = NavUnreachReason::Component;
			return judgement;
		}

		if (plan.status == NavFollowStatus::Planned)
		{
			const float bx = grid.CellCenter(botCell.x);
			const float bz = grid.CellCenter(botCell.z);
			const float gx = grid.CellCenter(plan.goal.x);
			const float gz = grid.CellCenter(plan.goal.z);
			const float dx = gx - bx;
			const float dz = gz - bz;
			judgement.straightM = std::sqrt(dx * dx + dz * dz);
			judgement.pathM = plan.pathCost;

			if (params.detourFactor > 0.0f && judgement.pathM > params.detourFactor * judgement.straightM
				&& judgement.pathM > params.detourMinM)
			{
				judgement.verdict = NavReachVerdict::Unreachable;
				judgement.reason = NavUnreachReason::Detour;
			}
			else
			{
				judgement.verdict = NavReachVerdict::Reachable;
			}
			return judgement;
		}

		// Remaining case: PathFailed with a connected ring candidate (limit / first tries failed).
		judgement.verdict = NavReachVerdict::Unknown;
		return judgement;
	}

	inline void NavUnreachTracker::Reset()
	{
		m_active = false;
		m_since = 0;
		m_reason = NavUnreachReason::None;
	}

	inline void NavUnreachTracker::Feed(int64_t nowMs, const NavReachJudgement & judgement)
	{
		if (judgement.verdict == NavReachVerdict::Unreachable)
		{
			if (!m_active)
			{
				m_active = true;
				m_since = nowMs;
			}
			m_reason = judgement.reason;
		}
		else
		{
			m_active = false;
			m_since = 0;
			m_reason = NavUnreachReason::None;
		}
	}

	inline bool NavUnreachTracker::Active() const
	{
		return m_active;
	}

	inline int64_t NavUnreachTracker::SinceMs() const
	{
		return m_since;
	}

	inline NavUnreachReason NavUnreachTracker::Reason() const
	{
		return m_reason;
	}

	inline bool NavUnreachTracker::Due(int64_t nowMs, int holdMs) const
	{
		return m_active && nowMs - m_since >= (int64_t)holdMs;
	}
}
