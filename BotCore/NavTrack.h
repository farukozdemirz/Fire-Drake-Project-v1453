#pragma once

// Moving-target tracking, lead prediction, range-ring goal selection and unified replanning
// (F5-04; docs/12 s4.2, ADR-0006 Ek F5-04) on top of the F5-01 grid, F5-02 A* and F5-03
// smoothing. Pure logic: the standard library only, no server header, no global/static state and
// no clock (every time is a caller-supplied one-way millisecond stamp). The follower never
// decides that a target is unreachable: NavFollowPlan::pathStatus only carries the last A* status
// (F5-05 owns that decision). World units are metres; cell (x, z) maps to world (x, z) through
// NavGrid::CellOf/CellCenter.

#include "NavSmooth.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BotCore
{
	// Timestamped target observations -> velocity. Fixed storage, no allocation.
	class NavTargetTracker
	{
	public:
		static constexpr int kCapacity = 16;

		void Clear();
		// False (sample dropped) when tMs <= the newest stored time; otherwise stored. When all
		// kCapacity slots are used the oldest sample is overwritten. speedField is the WIZ_MOVE
		// speed field (m/s * 10, docs/03 CLI-05): 0 marks a stopped packet, -1 (default) means the
		// caller has no speed information and the velocity is derived from the positions only.
		bool Observe(int64_t tMs, float x, float z, int16_t speedField = -1);
		int  Count() const;
		// False when empty. Otherwise the newest sample.
		bool Latest(int64_t & tMs, float & x, float & z) const;
		// vx = vz = 0 unless: >= 2 samples, the newest sample's speed field is not 0, and
		// nowMs - newest.t <= windowMs. Then the newest sample is differenced against the nearest
		// older sample whose span is >= minSpanMs (> 0) and <= windowMs (docs/12 s13.2: last two
		// observations at packet cadence, or going back to a long-enough span). A positive newest
		// speed field clamps the magnitude to speedField / 10 * 1.1 m/s (direction preserved).
		void Velocity(int64_t nowMs, int windowMs, int minSpanMs, float & vx, float & vz) const;

	private:
		struct Sample
		{
			int64_t t = 0;
			float x = 0.0f;
			float z = 0.0f;
			int16_t speed = -1;
		};

		const Sample & At(int backFromNewest) const;

		Sample m_samples[kCapacity];
		int m_count = 0;
		int m_next = 0;
	};

	// Lead time in seconds: min(maxLeadSec, dist / ownSpeedMps); 0 when ownSpeedMps <= 0,
	// maxLeadSec <= 0 or dist <= 0.
	inline float NavPredictLead(float dist, float ownSpeedMps, float maxLeadSec)
	{
		if (ownSpeedMps <= 0.0f || maxLeadSec <= 0.0f || dist <= 0.0f)
			return 0.0f;
		const float lead = dist / ownSpeedMps;
		return lead < maxLeadSec ? lead : maxLeadSec;
	}

	// Fills `out` (fully overwritten) with the Walk cells whose centre is within [lo, hi] metres of
	// the world point (cx, cz), nearest to `from` first. Effective radii: hi = max(ringMaxM,
	// 0.5 * unit * sqrt(2)); lo = min(max(ringMinM, 0), hi). Order: NavOctile(cell - from)
	// ascending, ties by x then z. The scan window is the square of cells within
	// ceil(hi / unit) + 1 of the cell containing (cx, cz), clipped by Walk (which is false off-grid).
	inline void NavRingCells(const NavGrid & grid, float cx, float cz, float ringMinM, float ringMaxM,
		NavCell from, std::vector<NavCell> & out)
	{
		out.clear();

		const float unit = grid.Unit();
		if (unit <= 0.0f)
			return;

		const float halfDiagonal = 0.5f * unit * std::sqrt(2.0f);
		float hi = ringMaxM > halfDiagonal ? ringMaxM : halfDiagonal;
		float lo = ringMinM > 0.0f ? ringMinM : 0.0f;
		if (lo > hi)
			lo = hi;

		const int centerX = grid.CellOf(cx);
		const int centerZ = grid.CellOf(cz);
		const int r = static_cast<int>(std::ceil(hi / unit)) + 1;
		const float lo2 = lo * lo;
		const float hi2 = hi * hi;

		for (int x = centerX - r; x <= centerX + r; ++x)
		{
			for (int z = centerZ - r; z <= centerZ + r; ++z)
			{
				if (!grid.Walk(x, z))
					continue;
				const float dx = grid.CellCenter(x) - cx;
				const float dz = grid.CellCenter(z) - cz;
				const float d2 = dx * dx + dz * dz;
				if (d2 < lo2 || d2 > hi2)
					continue;
				NavCell cell;
				cell.x = x;
				cell.z = z;
				out.push_back(cell);
			}
		}

		std::sort(out.begin(), out.end(), [&](const NavCell & a, const NavCell & b)
		{
			const float oa = NavOctile(a.x - from.x, a.z - from.z, unit);
			const float ob = NavOctile(b.x - from.x, b.z - from.z, unit);
			if (oa != ob)
				return oa < ob;
			if (a.x != b.x)
				return a.x < b.x;
			return a.z < b.z;
		});
	}

	struct NavFollowParams
	{
		float maxLeadSec = 1.5f;        // docs/12 s4.2 [O]
		int   velocityWindowMs = 4000;  // docs/12 s13.2 [A] P-NAV-VEL-WINDOW
		int   minVelocitySpanMs = 400;  // docs/12 s13.2 [A] P-NAV-VEL-MIN-SPAN
		float replanDistM = 6.0f;       // docs/12 s4.2 [O]; <= 0 disables the Moved trigger
		int   replanIntervalMs = 500;   // docs/12 s4.2 [O]
		float ringMinM = 0.0f;          // role ring around the predicted point (metres)
		float ringMaxM = 0.0f;
		int   ringMaxTries = 3;         // [A] ADR-0006 Ek F5-04; values < 1 act as 1
		NavSearchParams search;         // P-NAV-MAX-NODES
		NavSmoothParams smooth;         // P-NAV-SMOOTH-LOOKAHEAD
	};

	enum class NavFollowStatus
	{
		NoTarget,       // no observation yet (or after Reset)
		Planned,        // path found and smoothed
		NoGoal,         // the ring holds no Walk cell
		InvalidStart,   // the bot's own cell is not Walk / off-grid
		PathFailed      // every tried ring cell failed (see pathStatus: NoPath / NodeLimit)
	};

	enum class NavReplanReason { None, First, Moved, Interval };

	struct NavFollowPlan
	{
		NavFollowStatus status = NavFollowStatus::NoTarget;
		NavPathStatus pathStatus = NavPathStatus::NoPath;  // status of the LAST A* run; meaningful for Planned, InvalidStart, PathFailed
		NavCell goal;                 // chosen ring cell; meaningful when Planned
		float targetX = 0.0f;         // latest observed target position used for this plan
		float targetZ = 0.0f;
		float predX = 0.0f;           // ring centre (predicted point after back-off)
		float predZ = 0.0f;
		float leadSec = 0.0f;         // lead time actually applied (0: no velocity or no walkable prediction)
		int   tries = 0;              // A* runs of this plan (0 for NoGoal)
		int   expanded = 0;           // closed nodes summed over those runs
		float pathCost = 0.0f;         // A* cost (metres) of the chosen route; meaningful when Planned
		int64_t plannedAtMs = 0;
		NavSmoothResult smooth;       // start..goal waypoints and length; empty unless Planned
	};

	class NavFollower
	{
	public:
		void Reset();   // forget observations and plan: status NoTarget, reason None, Replans() 0
		// Feed a target position when the perception layer reports one (tMs = observation time).
		// speedField = -1 when unknown (see NavTargetTracker::Observe). False when dropped
		// (tMs <= newest stored time).
		bool ObserveTarget(int64_t tMs, float x, float z, int16_t speedField = -1);
		// Call once per bot tick. Returns true when a plan was (re)computed during this call
		// (Plan() then holds the result, also for NoGoal/InvalidStart/PathFailed). Returns false when
		// nothing was due or no target was observed yet (Plan() unchanged).
		bool Update(const NavGrid & grid, NavPathfinder & pathfinder, int64_t nowMs,
			float botX, float botZ, float botSpeedMps, const NavFollowParams & params);

		const NavFollowPlan & Plan() const { return m_plan; }
		NavReplanReason LastReason() const { return m_reason; }  // reason of the last replan; None before the first
		int Replans() const { return m_replans; }                // replans since Reset
		const NavTargetTracker & Tracker() const { return m_tracker; }

	private:
		NavTargetTracker m_tracker;
		NavFollowPlan m_plan;
		NavPathResult m_path;
		std::vector<NavCell> m_candidates;
		NavReplanReason m_reason = NavReplanReason::None;
		int m_replans = 0;
	};

	inline void NavTargetTracker::Clear()
	{
		m_count = 0;
		m_next = 0;
	}

	inline const NavTargetTracker::Sample & NavTargetTracker::At(int backFromNewest) const
	{
		const int idx = (m_next - 1 - backFromNewest + 2 * kCapacity) % kCapacity;
		return m_samples[idx];
	}

	inline bool NavTargetTracker::Observe(int64_t tMs, float x, float z, int16_t speedField)
	{
		if (m_count > 0)
		{
			int64_t newestT = 0;
			float nx = 0.0f;
			float nz = 0.0f;
			Latest(newestT, nx, nz);
			if (tMs <= newestT)
				return false;
		}

		m_samples[m_next].t = tMs;
		m_samples[m_next].x = x;
		m_samples[m_next].z = z;
		m_samples[m_next].speed = speedField;
		m_next = (m_next + 1) % kCapacity;
		if (m_count < kCapacity)
			++m_count;
		return true;
	}

	inline int NavTargetTracker::Count() const
	{
		return m_count;
	}

	inline bool NavTargetTracker::Latest(int64_t & tMs, float & x, float & z) const
	{
		if (m_count == 0)
			return false;
		const Sample & newest = At(0);
		tMs = newest.t;
		x = newest.x;
		z = newest.z;
		return true;
	}

	inline void NavTargetTracker::Velocity(int64_t nowMs, int windowMs, int minSpanMs, float & vx, float & vz) const
	{
		vx = 0.0f;
		vz = 0.0f;
		if (m_count < 2 || windowMs < 0)
			return;

		const Sample & newest = At(0);
		if (nowMs - newest.t > static_cast<int64_t>(windowMs))
			return;

		// A packet with speed field 0 means the target stopped (docs/12 s13.2).
		if (newest.speed == 0)
			return;

		// Teleport jump guard (F5-56): when two consecutive observations that are at least a
		// normal span apart imply an impossible speed (> 30 m/s; respawn, summon, blink), the
		// newest sample no longer pairs with the past. Report no velocity until a fresh
		// observation re-establishes a plausible pair. The span gate keeps packet bunching
		// (sub-minSpan gaps) out of this branch; it is handled by the normal selection below.
		if (m_count >= 2)
		{
			const int64_t jspan = newest.t - At(1).t;
			if (jspan >= static_cast<int64_t>(minSpanMs) && jspan > 0)
			{
				const float jdx = newest.x - At(1).x;
				const float jdz = newest.z - At(1).z;
				const float implied = std::sqrt(jdx * jdx + jdz * jdz) * 1000.0f / static_cast<float>(jspan);
				if (implied > 30.0f)
					return;
			}
		}

		// Nearest older sample whose span from the newest is at least minSpanMs, staying within
		// windowMs of the newest observation. At packet cadence (~1.5 s) the newest-to-previous
		// span is already long enough; a short recent pair falls back to a longer span.
		const int64_t threshold = newest.t - static_cast<int64_t>(windowMs);
		int64_t oldT = 0;
		float oldX = 0.0f;
		float oldZ = 0.0f;
		bool haveOld = false;
		for (int k = 1; k < m_count; ++k)
		{
			const Sample & s = At(k);
			if (s.t < threshold)
				break;   // samples only get older from here
			if (newest.t - s.t >= static_cast<int64_t>(minSpanMs))
			{
				oldT = s.t;
				oldX = s.x;
				oldZ = s.z;
				haveOld = true;
				break;
			}
		}
		if (!haveOld)
			return;

		const int64_t span = newest.t - oldT;
		if (span <= 0)
			return;

		const float inv = 1000.0f / static_cast<float>(span);
		vx = (newest.x - oldX) * inv;
		vz = (newest.z - oldZ) * inv;

		// A positive speed field is a magnitude hint (WIZ_MOVE speed = m/s * 10): clamp a noisy
		// position jump to speedField / 10 * 1.1 m/s, direction preserved.
		if (newest.speed > 0)
		{
			const float maxV = (static_cast<float>(newest.speed) / 10.0f) * 1.1f;
			const float mag = std::sqrt(vx * vx + vz * vz);
			if (mag > maxV && mag > 0.0f)
			{
				const float scale = maxV / mag;
				vx *= scale;
				vz *= scale;
			}
		}
	}

	inline void NavFollower::Reset()
	{
		m_tracker.Clear();
		m_plan = NavFollowPlan();
		m_reason = NavReplanReason::None;
		m_replans = 0;
	}

	inline bool NavFollower::ObserveTarget(int64_t tMs, float x, float z, int16_t speedField)
	{
		return m_tracker.Observe(tMs, x, z, speedField);
	}

	inline bool NavFollower::Update(const NavGrid & grid, NavPathfinder & pathfinder, int64_t nowMs,
		float botX, float botZ, float botSpeedMps, const NavFollowParams & params)
	{
		int64_t targetT = 0;
		float tx = 0.0f;
		float tz = 0.0f;
		if (!m_tracker.Latest(targetT, tx, tz))
			return false;

		// When is a (re)plan due? First plan, then a moved target, then the interval.
		NavReplanReason reason = NavReplanReason::None;
		if (m_plan.status == NavFollowStatus::NoTarget)
		{
			reason = NavReplanReason::First;
		}
		else if (params.replanDistM > 0.0f)
		{
			const float mdx = tx - m_plan.targetX;
			const float mdz = tz - m_plan.targetZ;
			if (mdx * mdx + mdz * mdz >= params.replanDistM * params.replanDistM)
				reason = NavReplanReason::Moved;
		}
		if (reason == NavReplanReason::None
			&& nowMs - m_plan.plannedAtMs >= static_cast<int64_t>(params.replanIntervalMs))
		{
			reason = NavReplanReason::Interval;
		}
		if (reason == NavReplanReason::None)
			return false;

		// Velocity and lead prediction with back-off onto a walkable cell.
		float vx = 0.0f;
		float vz = 0.0f;
		m_tracker.Velocity(nowMs, params.velocityWindowMs, params.minVelocitySpanMs, vx, vz);

		const float distX = tx - botX;
		const float distZ = tz - botZ;
		const float dist = std::sqrt(distX * distX + distZ * distZ);
		const float lead0 = NavPredictLead(dist, botSpeedMps, params.maxLeadSec);

		float predX = tx;
		float predZ = tz;
		float leadSec = 0.0f;
		if ((vx != 0.0f || vz != 0.0f) && lead0 > 0.0f)
		{
			// Observation age is added to the lead: the older the newest observation, the further
			// the target has moved since (docs/12 s13.2). Still capped at maxLeadSec.
			float lead = lead0 + static_cast<float>(nowMs - targetT) / 1000.0f;
			if (lead > params.maxLeadSec)
				lead = params.maxLeadSec;
			for (int attempt = 0; attempt < 4; ++attempt)
			{
				const float qx = tx + vx * lead;
				const float qz = tz + vz * lead;
				if (grid.Walk(grid.CellOf(qx), grid.CellOf(qz)))
				{
					predX = qx;
					predZ = qz;
					leadSec = lead;
					break;
				}
				lead *= 0.5f;
			}
		}

		NavCell botCell;
		botCell.x = grid.CellOf(botX);
		botCell.z = grid.CellOf(botZ);

		NavRingCells(grid, predX, predZ, params.ringMinM, params.ringMaxM, botCell, m_candidates);

		m_plan.status = NavFollowStatus::NoTarget;
		m_plan.pathStatus = NavPathStatus::NoPath;
		m_plan.goal.x = 0;
		m_plan.goal.z = 0;
		m_plan.tries = 0;
		m_plan.expanded = 0;
		m_plan.pathCost = 0.0f;
		m_plan.smooth.waypoints.clear();
		m_plan.smooth.length = 0.0f;

		if (m_candidates.empty())
		{
			m_plan.status = NavFollowStatus::NoGoal;
		}
		else
		{
			int maxTries = params.ringMaxTries;
			if (maxTries < 1)
				maxTries = 1;

			const int limit = maxTries < static_cast<int>(m_candidates.size())
				? maxTries : static_cast<int>(m_candidates.size());
			bool finished = false;
			for (int i = 0; i < limit; ++i)
			{
				pathfinder.Find(grid, botCell, m_candidates[static_cast<size_t>(i)], params.search, m_path);
				++m_plan.tries;
				m_plan.expanded += m_path.expanded;
				m_plan.pathStatus = m_path.status;

				if (m_path.status == NavPathStatus::Found)
				{
					m_plan.goal = m_candidates[static_cast<size_t>(i)];
					m_plan.pathCost = m_path.cost;
					NavSmoothPath(grid, m_path.cells, params.smooth, m_plan.smooth);
					m_plan.status = NavFollowStatus::Planned;
					finished = true;
					break;
				}
				if (m_path.status == NavPathStatus::InvalidStart)
				{
					m_plan.status = NavFollowStatus::InvalidStart;
					finished = true;
					break;
				}
			}

			if (!finished)
			{
				m_plan.status = NavFollowStatus::PathFailed;
				m_plan.smooth.waypoints.clear();
				m_plan.smooth.length = 0.0f;
			}
		}

		m_plan.targetX = tx;
		m_plan.targetZ = tz;
		m_plan.predX = predX;
		m_plan.predZ = predZ;
		m_plan.leadSec = leadSec;
		m_plan.plannedAtMs = nowMs;
		m_reason = reason;
		++m_replans;
		return true;
	}
}
