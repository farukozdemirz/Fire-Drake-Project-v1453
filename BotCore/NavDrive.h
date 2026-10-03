#pragma once

// Path-following drive for the real move packets (F5-62; docs/12 s13.1, s13.4). Pure logic:
// the standard library and sibling headers only, no global state, no clock. One drive per bot.
// F5-62 carries the Goto mode only; F5-63 adds Follow and the stuck members.
#include "NavChordGuard.h"
#include "NavPath.h"
#include "NavSmooth.h"
#include "NavStuck.h"      // NavRoutePoint, NavRouteProgressM (F5-57)

#include <cmath>
#include <cstdint>
#include <vector>

namespace BotCore
{
	constexpr int kMaxGotoReplans = 1;
	constexpr float kMinTruncStepM = 0.15f;   // a truncated step must advance at least this far

	// Mirrors the executor packet quantisation (ActionExecutor.cpp: uint16(w * 10.0f + 0.5f) / 10.0f).
	// Precondition 0 <= w <= 6553.5 (callers reject others before quantising).
	inline float NavQuantiseM(float w)
	{
		return (float)(uint16_t)(w * 10.0f + 0.5f) / 10.0f;
	}

	enum class NavDriveMode { Off, Goto };
	enum class NavPlanStatus { None, Planned, InvalidStart, InvalidGoal, NoPath, NodeLimit, ReplanLimit };

	struct NavDriveParams
	{
		NavSearchParams search;   // P-NAV-MAX-NODES 20000
		NavSmoothParams smooth;   // P-NAV-SMOOTH-LOOKAHEAD 64
	};

	struct NavDriveStep
	{
		enum Kind { None, Step, Arrived, Blocked } kind = None;
		float x = 0.0f, z = 0.0f;      // packet position, already quantised (the executor re-quantises idempotently)
		float routeProgressM = 0.0f;   // route progress of (x, z); F5-63 feeds NavProgressAssessor
		float distToGoalM = 0.0f;      // Euclid distance from (x, z) to the goal point
		bool  truncated = false;       // the full step was cut at a route vertex because its chord was blocked
	};

	class NavDrive
	{
	public:
		void Reset()
		{
			m_mode = NavDriveMode::Off;
			m_route.clear();
			m_cum.clear();
			m_routeLength = 0.0f;
			m_goalX = 0.0f;
			m_goalZ = 0.0f;
			m_replans = 0;
			m_planExpanded = 0;
			m_planWaypoints = 0;
		}

		bool Active() const { return m_mode != NavDriveMode::Off; }
		NavDriveMode Mode() const { return m_mode; }

		// Plans bot -> goal with the caller's pathfinder (caller guarantees single-thread use) and arms the
		// walk. Goal is quantised first (design decision 2); route per design decision 3. On any status
		// other than Planned the drive is Reset (inactive). Replans() = 0 after Planned.
		NavPlanStatus BeginGoto(const NavGrid & grid, NavPathfinder & finder, float botX, float botZ,
			float goalX, float goalZ, const NavDriveParams & params)
		{
			Reset();
			const NavPlanStatus status = PlanGoto(grid, finder, botX, botZ, goalX, goalZ, params);
			if (status != NavPlanStatus::Planned)
			{
				Reset();
				return status;
			}
			m_mode = NavDriveMode::Goto;
			return status;   // m_replans stays 0
		}

		// Next move packet from the bot's CURRENT position. Not Active, non-finite input or
		// maxStepM <= 0 -> kind None. The drive does not move the bot: the caller feeds the next position.
		NavDriveStep NextStep(const NavGrid & grid, float botX, float botZ, float maxStepM)
		{
			NavDriveStep step;
			if (m_mode == NavDriveMode::Off || m_route.size() < 2)
				return step;
			if (!std::isfinite(botX) || !std::isfinite(botZ) || !(maxStepM > 0.0f))
				return step;

			const float total = m_routeLength;
			const float p = NavRouteProgressM(m_route.data(), (int)m_route.size(), botX, botZ);
			const bool isGoal = (total - p) <= maxStepM;

			NavRoutePoint candidate;
			if (isGoal)
				candidate = m_route.back();
			else
				candidate = PointAt(p + maxStepM);

			const float qx = NavQuantiseM(candidate.x);
			const float qz = NavQuantiseM(candidate.z);

			if (CheckMoveChord(&grid, botX, botZ, qx, qz).verdict == ChordVerdict::Ok)
			{
				step.kind = isGoal ? NavDriveStep::Arrived : NavDriveStep::Step;
				step.x = qx;
				step.z = qz;
			}
			else
			{
				// Design decision 4: cut the step at the first intermediate route vertex between the
				// minimum advance and the intended step distance, when that vertex is itself clear.
				const float lo = p + kMinTruncStepM;
				const float hi = isGoal ? total : p + maxStepM;
				for (size_t i = 1; i + 1 < m_route.size(); ++i)
				{
					const float cum = m_cum[i];
					if (cum < lo)
						continue;
					if (cum >= hi)
						break;

					const float tx = NavQuantiseM(m_route[i].x);
					const float tz = NavQuantiseM(m_route[i].z);
					if (CheckMoveChord(&grid, botX, botZ, tx, tz).verdict == ChordVerdict::Ok)
					{
						step.kind = NavDriveStep::Step;
						step.x = tx;
						step.z = tz;
						step.truncated = true;
					}
					break;   // only the first qualifying vertex is tried
				}

				if (step.kind != NavDriveStep::Step)
				{
					step.kind = NavDriveStep::Blocked;
					step.x = botX;
					step.z = botZ;
				}
			}

			step.routeProgressM = NavRouteProgressM(m_route.data(), (int)m_route.size(), step.x, step.z);
			const float gdx = step.x - m_goalX;
			const float gdz = step.z - m_goalZ;
			step.distToGoalM = std::sqrt(gdx * gdx + gdz * gdz);
			return step;
		}

		// After a Blocked step: re-plans from the bot's current position to the SAME (quantised) goal.
		// Not Active -> None; Replans() >= kMaxGotoReplans -> Reset + ReplanLimit; otherwise as BeginGoto
		// with Replans() incremented on Planned (and Reset on failure).
		NavPlanStatus Replan(const NavGrid & grid, NavPathfinder & finder, float botX, float botZ,
			const NavDriveParams & params)
		{
			if (m_mode == NavDriveMode::Off)
				return NavPlanStatus::None;
			if (m_replans >= kMaxGotoReplans)
			{
				Reset();
				return NavPlanStatus::ReplanLimit;
			}

			const NavPlanStatus status = PlanGoto(grid, finder, botX, botZ, m_goalX, m_goalZ, params);
			if (status != NavPlanStatus::Planned)
			{
				Reset();
				return status;
			}
			++m_replans;
			m_mode = NavDriveMode::Goto;
			return status;
		}

		const std::vector<NavRoutePoint> & Route() const { return m_route; }
		float RouteLengthM() const { return m_mode != NavDriveMode::Off ? m_routeLength : 0.0f; }
		float GoalX() const { return m_goalX; }   // quantised goal
		float GoalZ() const { return m_goalZ; }
		int   Replans() const { return m_replans; }
		int   PlanExpanded() const { return m_planExpanded; }
		int   PlanWaypoints() const { return m_planWaypoints; }

	private:
		// 0 <= w <= 6553.5, finite. Callers reject anything else (design decision 2).
		bool ValidCoord(float w) const
		{
			return std::isfinite(w) && w >= 0.0f && w * 10.0f <= 65535.0f;
		}

		NavPlanStatus PlanGoto(const NavGrid & grid, NavPathfinder & finder, float botX, float botZ,
			float goalX, float goalZ, const NavDriveParams & params)
		{
			if (!ValidCoord(botX) || !ValidCoord(botZ))
				return NavPlanStatus::InvalidStart;
			if (!ValidCoord(goalX) || !ValidCoord(goalZ))
				return NavPlanStatus::InvalidGoal;

			const float qgx = NavQuantiseM(goalX);
			const float qgz = NavQuantiseM(goalZ);

			NavCell startCell;
			startCell.x = grid.CellOf(botX);
			startCell.z = grid.CellOf(botZ);
			NavCell goalCell;
			goalCell.x = grid.CellOf(qgx);
			goalCell.z = grid.CellOf(qgz);

			if (!grid.Walk(startCell.x, startCell.z))
				return NavPlanStatus::InvalidStart;
			if (!grid.Walk(goalCell.x, goalCell.z))
				return NavPlanStatus::InvalidGoal;

			NavPathResult path;
			finder.Find(grid, startCell, goalCell, params.search, path);
			switch (path.status)
			{
			case NavPathStatus::Found:
				break;
			case NavPathStatus::NoPath:
				return NavPlanStatus::NoPath;
			case NavPathStatus::NodeLimit:
				return NavPlanStatus::NodeLimit;
			case NavPathStatus::InvalidStart:
				return NavPlanStatus::InvalidStart;
			case NavPathStatus::InvalidGoal:
				return NavPlanStatus::InvalidGoal;
			}

			NavSmoothResult smooth;
			NavSmoothPath(grid, path.cells, params.smooth, smooth);

			NavRoutePoint botPt;
			botPt.x = botX;
			botPt.z = botZ;
			NavRoutePoint goalPt;
			goalPt.x = qgx;
			goalPt.z = qgz;

			// Route per design decision 3: [bot] + smoothed cell centres + [goal].
			std::vector<NavRoutePoint> route;
			route.reserve(smooth.waypoints.size() + 2);
			route.push_back(botPt);
			for (size_t i = 0; i < smooth.waypoints.size(); ++i)
			{
				NavRoutePoint pt;
				pt.x = grid.CellCenter(smooth.waypoints[i].x);
				pt.z = grid.CellCenter(smooth.waypoints[i].z);
				route.push_back(pt);
			}
			route.push_back(goalPt);

			// Start shortcut: drop c0 when bot -> c1 (or, when c1 does not exist, bot -> goal) is clear.
			if (route.size() >= 3)
			{
				const NavRoutePoint target = (route.size() >= 4) ? route[2] : route.back();
				if (CheckMoveChord(&grid, route[0].x, route[0].z, target.x, target.z).verdict == ChordVerdict::Ok)
					route.erase(route.begin() + 1);
			}

			// End shortcut: drop ck when c(k-1) -> goal is clear. After the start shortcut the route keeps
			// [bot, c1..ck, goal] (or [bot, c0..ck, goal]), so route[n-3] is still c(k-1) for >= 4 points.
			if (smooth.waypoints.size() >= 2 && route.size() >= 4)
			{
				const NavRoutePoint & ckm1 = route[route.size() - 3];
				const NavRoutePoint & goal = route.back();
				if (CheckMoveChord(&grid, ckm1.x, ckm1.z, goal.x, goal.z).verdict == ChordVerdict::Ok)
					route.erase(route.end() - 2);
			}

			m_route.swap(route);
			m_goalX = qgx;
			m_goalZ = qgz;
			m_planExpanded = path.expanded;
			m_planWaypoints = (int)smooth.waypoints.size();
			RecomputeRoute();
			return NavPlanStatus::Planned;
		}

		void RecomputeRoute()
		{
			m_cum.clear();
			m_cum.reserve(m_route.size());
			float total = 0.0f;
			for (size_t i = 0; i < m_route.size(); ++i)
			{
				if (i > 0)
				{
					const float dx = m_route[i].x - m_route[i - 1].x;
					const float dz = m_route[i].z - m_route[i - 1].z;
					total += std::sqrt(dx * dx + dz * dz);
				}
				m_cum.push_back(total);
			}
			m_routeLength = total;
		}

		NavRoutePoint PointAt(float d) const
		{
			if (m_route.empty())
				return NavRoutePoint();
			if (d <= 0.0f)
				return m_route.front();
			if (d >= m_routeLength)
				return m_route.back();

			for (size_t i = 0; i + 1 < m_route.size(); ++i)
			{
				const float c0 = m_cum[i];
				const float c1 = m_cum[i + 1];
				if (d <= c1)
				{
					const float seg = c1 - c0;
					const float t = seg > 0.0f ? (d - c0) / seg : 0.0f;
					NavRoutePoint pt;
					pt.x = m_route[i].x + (m_route[i + 1].x - m_route[i].x) * t;
					pt.z = m_route[i].z + (m_route[i + 1].z - m_route[i].z) * t;
					return pt;
				}
			}
			return m_route.back();
		}

		NavDriveMode m_mode = NavDriveMode::Off;
		std::vector<NavRoutePoint> m_route;
		std::vector<float> m_cum;
		float m_routeLength = 0.0f;
		float m_goalX = 0.0f;
		float m_goalZ = 0.0f;
		int m_replans = 0;
		int m_planExpanded = 0;
		int m_planWaypoints = 0;
	};
}
