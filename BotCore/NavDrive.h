#pragma once

// Path-following drive for the real move packets (F5-62; docs/12 s13.1, s13.4). Pure logic:
// the standard library and sibling headers only, no global state, no clock. One drive per bot.
// F5-62 carries the Goto mode only; F5-63 adds Follow and the stuck members.
#include "NavBudget.h"     // NavQueryScheduler, NavPathCache, NavWhileDeferred (F5-64)
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

	enum class NavDriveMode { Off, Goto, Follow };
	enum class NavFollowEnd { None, TargetLost, StuckAbandon, PlanFailed, PathBlocked };
	enum class NavPlanStatus { None, Planned, InvalidStart, InvalidGoal, NoPath, NodeLimit, ReplanLimit };
	// F5-64 D4: which part of one Follow tick runs. All = the F5-73 TickFollow behaviour
	// (plan + assess); PlanOnly = the plan block only; AssessOnly = the loss policy, defer
	// decision, verdict and recovery ladder, never a plan.
	enum class NavPlanPhase { All, PlanOnly, AssessOnly };

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

	// Follow-mode parameters (F5-73; docs/12 s4.2, s10, s13.3). The ring is [3.0, 6.4] m: on a 4 m
	// grid a 6.0 m maximum can leave the ring empty near a cell corner (D2).
	struct NavFollowDriveParams
	{
		NavFollowParams   follow;             // ringMinM = 3.0f, ringMaxM = 6.4f in the constructor
		NavStuckParams    stuck;              // NavPacketCadenceParams() in the constructor (D9)
		NavProgressParams progress;           // defaults
		int   lostGraceMs = 1000;             // D3: <= this the target counts as visible
		int   lostHoldMs = 6000;              // D3: beyond this the route is dropped and holdStop fires once
		int   lostAbandonMs = 15000;          // D3: beyond this the drive ends with TargetLost
		int   planFailAbandon = 10;           // D8: consecutive failed plans that end the drive
		int   blockedAbandonMs = 5000;        // D10: continuous Blocked step time that ends the drive
		int   awaitingLogMs = 5000;           // diagnostics only (event), not a stuck rule
		float stepBackM = 3.0f;               // stage-3 step-back distance
		NavDeferParams defer;                 // F5-64 D5: plan-age / target-drift hold thresholds
		NavFollowDriveParams();
	};

	// One TickFollow call worth of output; the executor (F5-63) turns these into log/telemetry lines.
	struct NavDriveEvents
	{
		bool planned = false;                                   // NavFollower recomputed a plan this call
		NavFollowStatus planStatus = NavFollowStatus::NoTarget; // valid when planned
		NavReplanReason planReason = NavReplanReason::None;     // valid when planned
		int  planExpanded = 0;                                  // A* closed nodes of that plan
		bool routeAdopted = false;                              // planned && Planned: a fresh route replaced the old one
		NavProgressVerdict verdict = NavProgressVerdict::Idle;
		NavRecoveryStep recovery;                               // action != None once per stage entry; .recovered when recovered
		bool holdStop = false;                                  // D3: once, when the target is lost for > lostHoldMs
		bool awaitingLong = false;                              // intent active, no packet for >= awaitingLogMs (once per gap)
		NavFollowEnd ended = NavFollowEnd::None;                // != None => the drive is Off (Reset) after this call
		bool deferHold = false;                                 // F5-64 D5: once, when a due plan is not served on a stale route
	};

	inline NavFollowDriveParams::NavFollowDriveParams()
	{
		follow.ringMinM = 3.0f;
		follow.ringMaxM = 6.4f;
		stuck = NavPacketCadenceParams();
	}

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

			// Follow state (F5-73).
			m_follower.Reset();
			m_assess.Reset();
			m_monitor.Reset();
			m_penalties.Clear();
			m_sideScratch.clear();
			m_followPlans = 0;
			m_planFailCount = 0;
			m_lastSeenMs = 0;
			m_arrived = false;
			m_holding = false;
			m_awaitingLogged = false;
			m_activityMs = 0;
			m_pending = NavRecoveryAction::None;
			m_resetMonitorNext = false;
			m_hasPktVec = false;
			m_pktVx = 0.0f;
			m_pktVz = 0.0f;
			m_stepFromX = 0.0f;
			m_stepFromZ = 0.0f;
			m_hasBlockedSince = false;
			m_blockedSince = 0;
			m_blockedAbandon = false;

			// Plan-queue state (F5-64).
			m_planPending = false;
			m_planIsReplan = false;
			m_cacheHit = false;
			m_deferHeld = false;
			m_routeAtMs = INT64_MIN;
			m_routeTargetX = 0.0f;
			m_routeTargetZ = 0.0f;
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
			if (m_mode != NavDriveMode::Goto)
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

		// F5-64 D7: arm a Goto walk cheaply (endpoint validation only, no A*) so the caller can
		// queue the A* run. Returns None once armed ("plan pending"); invalid endpoints return
		// InvalidStart/InvalidGoal with the drive Reset. Mirrors PlanGoto's validation order.
		NavPlanStatus ArmGoto(const NavGrid & grid, float botX, float botZ, float goalX, float goalZ)
		{
			Reset();
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

			m_mode = NavDriveMode::Goto;
			m_goalX = qgx;
			m_goalZ = qgz;
			m_planPending = true;
			m_planIsReplan = false;
			return NavPlanStatus::None;
		}

		bool PlanPending() const { return m_planPending; }

		// Runs the armed Goto plan through the caller's pathfinder (optionally the F5-53 cache).
		// Same statuses as PlanGoto; None when nothing is pending. A fresh plan clears the pending
		// flag; a replan increments Replans() exactly like Replan().
		NavPlanStatus RunGotoPlan(const NavGrid & grid, NavPathfinder & finder, NavPathCache * cache,
			int64_t nowMs, float botX, float botZ, const NavDriveParams & params)
		{
			if (!m_planPending)
			{
				m_cacheHit = false;
				return NavPlanStatus::None;
			}

			const bool wasReplan = m_planIsReplan;
			const NavPlanStatus status = PlanGotoImpl(grid, finder, cache, nowMs, botX, botZ,
				m_goalX, m_goalZ, params);
			if (status != NavPlanStatus::Planned)
			{
				Reset();
				return status;
			}
			m_planPending = false;
			m_planIsReplan = false;
			if (wasReplan)
				++m_replans;
			m_mode = NavDriveMode::Goto;
			return status;
		}

		// Marks a blocked Goto route for the next RunGotoPlan (F5-64 D7). Idempotent while a plan
		// is already pending; ReplanLimit (and Reset) when the replan budget is spent.
		NavPlanStatus RequestReplan()
		{
			if (m_mode != NavDriveMode::Goto)
				return NavPlanStatus::None;
			if (m_planPending)
				return NavPlanStatus::None;
			if (m_replans >= kMaxGotoReplans)
			{
				Reset();
				return NavPlanStatus::ReplanLimit;
			}
			m_planPending = true;
			m_planIsReplan = true;
			return NavPlanStatus::None;
		}

		bool LastPlanCacheHit() const { return m_cacheHit; }

		const std::vector<NavRoutePoint> & Route() const { return m_route; }
		float RouteLengthM() const { return m_mode != NavDriveMode::Off ? m_routeLength : 0.0f; }
		float GoalX() const { return m_goalX; }   // quantised goal
		float GoalZ() const { return m_goalZ; }
		int   Replans() const { return m_replans; }
		int   PlanExpanded() const { return m_planExpanded; }
		int   PlanWaypoints() const { return m_planWaypoints; }

		// Follow mode (F5-73): moving-target chase on top of the F5-04 follower, the F5-09 stuck
		// ladder and the F5-57 progress assessor. Pure logic; the caller moves the bot and feeds
		// each NextFollowStep result back through OnPacketSent/OnPacketRejected.
		void BeginFollow(int64_t nowMs);
		void ObserveTarget(int64_t tMs, float x, float z, int16_t speedField, int64_t nowMs);
		// Call once per bot tick BEFORE asking for a step. `reach` is NavReach (or any type with
		// ComponentOf); `scratch` is an optional cost layer for the stage-4 penalty field.
		template <class Reach>
		NavDriveEvents TickFollow(const NavGrid & grid, NavPathfinder & finder, int64_t nowMs,
			float botX, float botZ, float botSpeedMps, const NavFollowDriveParams & params,
			NavCostLayer * scratch, const Reach & reach)
		{
			return TickFollowImpl<Reach>(grid, &finder, nowMs, botX, botZ, botSpeedMps, params, scratch, &reach, NavPlanPhase::All);
		}
		NavDriveEvents TickFollow(const NavGrid & grid, NavPathfinder & finder, int64_t nowMs,
			float botX, float botZ, float botSpeedMps, const NavFollowDriveParams & params,
			NavCostLayer * scratch)
		{
			return TickFollowImpl<NavNoReach>(grid, &finder, nowMs, botX, botZ, botSpeedMps, params, scratch, nullptr, NavPlanPhase::All);
		}

		// F5-64 D3: "this bot wants a plan right now". The scheduler uses it to enqueue a query;
		// FollowPlanDue never runs A*.
		bool FollowPlanDue(int64_t nowMs, const NavFollowDriveParams & params) const
		{
			if (m_mode != NavDriveMode::Follow)
				return false;
			if (m_blockedAbandon)
				return false;
			if (nowMs - m_lastSeenMs > (int64_t)params.lostGraceMs)
				return false;
			return m_follower.DueReason(nowMs, params.follow) != NavReplanReason::None;
		}

		// F5-64 D4: the plan half of a Follow tick. Runs A* only when FollowPlanDue; otherwise an
		// empty event set. `reach` is NavReach (or any type with ComponentOf).
		template <class Reach>
		NavDriveEvents PlanFollow(const NavGrid & grid, NavPathfinder & finder, int64_t nowMs,
			float botX, float botZ, float botSpeedMps, const NavFollowDriveParams & params,
			NavCostLayer * scratch, const Reach & reach)
		{
			return TickFollowImpl<Reach>(grid, &finder, nowMs, botX, botZ, botSpeedMps, params, scratch, &reach, NavPlanPhase::PlanOnly);
		}

		// F5-64 D4/D5: the assessment half (loss policy, defer hold, verdict, recovery). Never
		// runs A*. Call after PlanFollow for the tick.
		NavDriveEvents AssessFollow(const NavGrid & grid, int64_t nowMs, float botX, float botZ,
			const NavFollowDriveParams & params)
		{
			return TickFollowImpl<NavNoReach>(grid, nullptr, nowMs, botX, botZ, 0.0f, params, nullptr, nullptr, NavPlanPhase::AssessOnly);
		}

		bool DeferHeld() const { return m_deferHeld; }
		// Route age in ms; INT64_MAX when no route was adopted yet (D6).
		int64_t RouteAgeMs(int64_t nowMs) const
		{
			return m_routeAtMs == INT64_MIN ? INT64_MAX : nowMs - m_routeAtMs;
		}
		// True when a Follow route is held by the defer contract (D6): F5-76 counts such packets
		// as nav_stale_steps. Goto (and no route) is always false.
		bool RouteStale(int64_t nowMs, const NavDeferParams & deferParams) const
		{
			if (m_mode != NavDriveMode::Follow || m_route.size() < 2)
				return false;
			float driftM = 0.0f;
			int64_t lt = 0;
			float lx = 0.0f;
			float lz = 0.0f;
			if (m_follower.Tracker().Latest(lt, lx, lz))
			{
				const float dx = lx - m_routeTargetX;
				const float dz = lz - m_routeTargetZ;
				driftM = std::sqrt(dx * dx + dz * dz);
			}
			return NavWhileDeferred(true, RouteAgeMs(nowMs), driftM, deferParams) == NavDeferAction::Hold;
		}

		NavDriveStep NextFollowStep(const NavGrid & grid, int64_t nowMs, float botX, float botZ,
			float maxStepM, const NavFollowDriveParams & params);
		void OnPacketSent(int64_t tMs, const NavDriveStep & step);
		void OnPacketRejected(int64_t tMs);
		const NavFollower & Follower() const { return m_follower; }
		int  RecoveryStage() const { return m_monitor.Stage(); }
		int  StuckEpisodes() const { return m_monitor.Episodes(); }
		int  FollowPlans() const { return m_followPlans; }
		bool Arrived() const { return m_arrived; }

	private:
		// 0 <= w <= 6553.5, finite. Callers reject anything else (design decision 2).
		bool ValidCoord(float w) const
		{
			return std::isfinite(w) && w >= 0.0f && w * 10.0f <= 65535.0f;
		}

		// F5-70/F5-72 keep the synchronous path; the cache-aware core lives in PlanGotoImpl so
		// RunGotoPlan (F5-64 D7) can reuse it without touching BeginGoto/Replan.
		NavPlanStatus PlanGoto(const NavGrid & grid, NavPathfinder & finder, float botX, float botZ,
			float goalX, float goalZ, const NavDriveParams & params)
		{
			return PlanGotoImpl(grid, finder, nullptr, 0, botX, botZ, goalX, goalZ, params);
		}

		NavPlanStatus PlanGotoImpl(const NavGrid & grid, NavPathfinder & finder, NavPathCache * cache,
			int64_t nowMs, float botX, float botZ, float goalX, float goalZ, const NavDriveParams & params);

		// Builds and adopts the route [bot] + smoothed waypoint cell centres + [goal] with the start
		// and end shortcuts (F5-62 design decisions 3/4); Follow (F5-73) reuses it on the follower's
		// smoothed waypoints. Goto behaviour is unchanged.
		void AdoptRoute(const NavGrid & grid, float botX, float botZ,
			const std::vector<NavCell> & waypoints, float goalX, float goalZ)
		{
			NavRoutePoint botPt;
			botPt.x = botX;
			botPt.z = botZ;
			NavRoutePoint goalPt;
			goalPt.x = goalX;
			goalPt.z = goalZ;

			std::vector<NavRoutePoint> route;
			route.reserve(waypoints.size() + 2);
			route.push_back(botPt);
			for (size_t i = 0; i < waypoints.size(); ++i)
			{
				NavRoutePoint pt;
				pt.x = grid.CellCenter(waypoints[i].x);
				pt.z = grid.CellCenter(waypoints[i].z);
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
			if (waypoints.size() >= 2 && route.size() >= 4)
			{
				const NavRoutePoint & ckm1 = route[route.size() - 3];
				const NavRoutePoint & goal = route.back();
				if (CheckMoveChord(&grid, ckm1.x, ckm1.z, goal.x, goal.z).verdict == ChordVerdict::Ok)
					route.erase(route.end() - 2);
			}

			m_route.swap(route);
			m_goalX = goalX;
			m_goalZ = goalZ;
			RecomputeRoute();
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

		// The plan block of a Follow tick (F5-64 D4) shared by the All and PlanOnly phases; see the
		// former TickFollowImpl body for the exact sequence. Sets ev.routeAdopted and the route
		// stamps (D6), and ev.ended = PlanFailed (with Reset) when the fail budget is spent.
		template <class Reach>
		void PlanBlock(const NavGrid & grid, NavPathfinder & finder, int64_t nowMs,
			float botX, float botZ, float botSpeedMps, const NavFollowDriveParams & params,
			NavCostLayer * scratch, const Reach * reach, NavDriveEvents & ev);

		template <class Reach>
		NavDriveEvents TickFollowImpl(const NavGrid & grid, NavPathfinder * finder, int64_t nowMs,
			float botX, float botZ, float botSpeedMps, const NavFollowDriveParams & params,
			NavCostLayer * scratch, const Reach * reach, NavPlanPhase phase);

		NavDriveMode m_mode = NavDriveMode::Off;
		std::vector<NavRoutePoint> m_route;
		std::vector<float> m_cum;
		float m_routeLength = 0.0f;
		float m_goalX = 0.0f;
		float m_goalZ = 0.0f;
		int m_replans = 0;
		int m_planExpanded = 0;
		int m_planWaypoints = 0;

		// Follow state (F5-73).
		NavFollower m_follower;
		NavProgressAssessor m_assess;
		NavStuckMonitor m_monitor;
		NavStuckPenalties m_penalties;
		std::vector<NavCell> m_sideScratch;
		int m_followPlans = 0;
		int m_planFailCount = 0;
		int64_t m_lastSeenMs = 0;
		bool m_arrived = false;
		bool m_holding = false;
		bool m_awaitingLogged = false;
		int64_t m_activityMs = 0;
		NavRecoveryAction m_pending = NavRecoveryAction::None;
		bool m_resetMonitorNext = false;
		bool m_hasPktVec = false;
		float m_pktVx = 0.0f;
		float m_pktVz = 0.0f;
		float m_stepFromX = 0.0f;
		float m_stepFromZ = 0.0f;
		bool m_hasBlockedSince = false;
		int64_t m_blockedSince = 0;
		bool m_blockedAbandon = false;

		// Plan-queue state (F5-64): Goto arming/replan, cache hit and the Follow defer hold.
		bool m_planPending = false;
		bool m_planIsReplan = false;
		bool m_cacheHit = false;
		bool m_deferHeld = false;
		int64_t m_routeAtMs = INT64_MIN;
		float m_routeTargetX = 0.0f;
		float m_routeTargetZ = 0.0f;
	};

	inline void NavDrive::BeginFollow(int64_t nowMs)
	{
		Reset();
		m_mode = NavDriveMode::Follow;
		m_lastSeenMs = nowMs;
		m_activityMs = nowMs;
	}

	inline void NavDrive::ObserveTarget(int64_t tMs, float x, float z, int16_t speedField, int64_t nowMs)
	{
		if (m_mode != NavDriveMode::Follow)
			return;
		m_follower.ObserveTarget(tMs, x, z, speedField);
		m_lastSeenMs = nowMs;
	}

	inline NavPlanStatus NavDrive::PlanGotoImpl(const NavGrid & grid, NavPathfinder & finder, NavPathCache * cache,
		int64_t nowMs, float botX, float botZ, float goalX, float goalZ, const NavDriveParams & params)
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
		m_cacheHit = false;
		if (cache != nullptr)
		{
			NavCacheKey key;
			key.startX = startCell.x;
			key.startZ = startCell.z;
			key.goalX = goalCell.x;
			key.goalZ = goalCell.z;
			key.fieldVersion = 0;
			std::vector<NavCell> cells;
			float cost = 0.0f;
			float length = 0.0f;
			if (cache->Find(key, nowMs, cells, cost, length))
			{
				m_cacheHit = true;
				path.status = NavPathStatus::Found;
				path.cells = cells;
				path.cost = cost;
				path.length = length;
				path.expanded = 0;   // the A* closed-node count is not cached
			}
			else
			{
				finder.Find(grid, startCell, goalCell, params.search, path);
				if (path.status == NavPathStatus::Found)
					cache->Put(key, path.cells, path.cost, path.length, nowMs);
			}
		}
		else
		{
			finder.Find(grid, startCell, goalCell, params.search, path);
		}

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

		AdoptRoute(grid, botX, botZ, smooth.waypoints, qgx, qgz);
		m_planExpanded = path.expanded;
		m_planWaypoints = (int)smooth.waypoints.size();
		return NavPlanStatus::Planned;
	}

	template <class Reach>
	void NavDrive::PlanBlock(const NavGrid & grid, NavPathfinder & finder, int64_t nowMs,
		float botX, float botZ, float botSpeedMps, const NavFollowDriveParams & params,
		NavCostLayer * scratch, const Reach * reach, NavDriveEvents & ev)
	{
		// (4) plan block: the penalty field is applied only while a penalty is active (D7).
		const NavCostField * fieldPtr = nullptr;
		NavCostField field;
		NavFollowParams follow = params.follow;
		if (scratch != nullptr && m_penalties.Count(nowMs) > 0)
		{
			if (scratch->Size() != grid.Size())
				scratch->Init(grid);
			else
				scratch->Clear();
			m_penalties.Apply(grid, nowMs, params.stuck, *scratch);
			field.params = NavCostParams();
			field.layer = scratch;
			fieldPtr = &field;
			follow.smooth.maxLookahead = 1;   // D7: penalties must not be cut by a smooth chord
		}

		if (m_follower.UpdateReachable(grid, finder, nowMs, botX, botZ, botSpeedMps, follow, *reach, fieldPtr))
		{
			const NavFollowPlan & plan = m_follower.Plan();
			ev.planned = true;
			ev.planStatus = plan.status;
			ev.planReason = m_follower.LastReason();
			ev.planExpanded = plan.expanded;
			++m_followPlans;

			if (plan.status == NavFollowStatus::Planned)
			{
				m_planFailCount = 0;
				const float gx = NavQuantiseM(grid.CellCenter(plan.goal.x));
				const float gz = NavQuantiseM(grid.CellCenter(plan.goal.z));
				AdoptRoute(grid, botX, botZ, plan.smooth.waypoints, gx, gz);
				ev.routeAdopted = true;
				m_routeAtMs = nowMs;
				m_routeTargetX = plan.targetX;
				m_routeTargetZ = plan.targetZ;
				m_deferHeld = false;

				const float p = NavRouteProgressM(m_route.data(), (int)m_route.size(), botX, botZ);
				if (m_routeLength - p > params.progress.arriveM)
				{
					m_arrived = false;
					m_assess.SetIntent(true, nowMs);   // D4: before NotifyReplan (SetIntent resets the base)
				}
				m_assess.NotifyReplan(nowMs, 0.0f);
				m_activityMs = nowMs;
				m_awaitingLogged = false;
			}
			else
			{
				++m_planFailCount;
				if (m_planFailCount >= params.planFailAbandon)
				{
					ev.ended = NavFollowEnd::PlanFailed;
					Reset();
					return;
				}
			}
		}
	}

	template <class Reach>
	NavDriveEvents NavDrive::TickFollowImpl(const NavGrid & grid, NavPathfinder * finder, int64_t nowMs,
		float botX, float botZ, float botSpeedMps, const NavFollowDriveParams & params,
		NavCostLayer * scratch, const Reach * reach, NavPlanPhase phase)
	{
		NavDriveEvents ev;

		if (m_mode != NavDriveMode::Follow)
			return ev;
		if (!ValidCoord(botX) || !ValidCoord(botZ))
			return ev;

		// PlanOnly (F5-64 D4): never runs the loss policy, the verdict or the recovery ladder.
		if (phase == NavPlanPhase::PlanOnly)
		{
			if (m_blockedAbandon || finder == nullptr)
				return ev;
			if (!FollowPlanDue(nowMs, params))
				return ev;
			PlanBlock(grid, *finder, nowMs, botX, botZ, botSpeedMps, params, scratch, reach, ev);
			return ev;
		}

		// D10: a continuous Blocked step ended the drive.
		if (m_blockedAbandon)
		{
			ev.ended = NavFollowEnd::PathBlocked;
			Reset();
			return ev;
		}

		// D3: target-loss policy.
		const int64_t lostMs = nowMs - m_lastSeenMs;
		if (lostMs >= (int64_t)params.lostAbandonMs)
		{
			ev.ended = NavFollowEnd::TargetLost;
			Reset();
			return ev;
		}
		if (lostMs > (int64_t)params.lostHoldMs)
		{
			if (!m_holding)
			{
				m_holding = true;
				ev.holdStop = true;
				m_route.clear();
				m_cum.clear();
				m_routeLength = 0.0f;
				m_pending = NavRecoveryAction::None;
				m_arrived = false;
				m_follower.InvalidatePlan();
				m_assess.SetIntent(false, nowMs);
			}
		}
		else if (lostMs <= (int64_t)params.lostGraceMs)
		{
			m_holding = false;

			if (phase == NavPlanPhase::All)
			{
				PlanBlock(grid, *finder, nowMs, botX, botZ, botSpeedMps, params, scratch, reach, ev);
				if (ev.ended != NavFollowEnd::None)
					return ev;
			}
			else
			{
				// D5: the query is due but was not served this tick. A fresh route is still
				// followed (steps come from NextFollowStep); a stale route holds once.
				if (m_route.size() >= 2 && !m_holding && !m_arrived && FollowPlanDue(nowMs, params))
				{
					float driftM = 0.0f;
					int64_t lt = 0;
					float lx = 0.0f;
					float lz = 0.0f;
					if (m_follower.Tracker().Latest(lt, lx, lz))
					{
						const float ddx = lx - m_routeTargetX;
						const float ddz = lz - m_routeTargetZ;
						driftM = std::sqrt(ddx * ddx + ddz * ddz);
					}
					if (!m_deferHeld
						&& NavWhileDeferred(true, RouteAgeMs(nowMs), driftM, params.defer) == NavDeferAction::Hold)
					{
						m_deferHeld = true;
						ev.deferHold = true;
						m_assess.SetIntent(false, nowMs);
					}
				}
			}
		}

		// (6) verdict and the awaitingLong diagnostic.
		ev.verdict = m_assess.Assess(nowMs, params.progress);
		if (ev.verdict == NavProgressVerdict::AwaitingPacket && !m_awaitingLogged
			&& nowMs - m_activityMs >= (int64_t)params.awaitingLogMs)
		{
			ev.awaitingLong = true;
			m_awaitingLogged = true;
		}

		// (7) moving (D5) with the D6 one-tick window reset.
		bool moving = (ev.verdict == NavProgressVerdict::Progressing || ev.verdict == NavProgressVerdict::Stalled)
			|| (m_monitor.Stage() > 0 && ev.verdict == NavProgressVerdict::AwaitingPacket);
		if (m_resetMonitorNext)
		{
			moving = false;
			m_resetMonitorNext = false;
		}

		const NavRecoveryStep rec = m_monitor.Update(grid, nowMs, botX, botZ, moving, params.stuck);
		ev.recovery = rec;

		switch (rec.action)
		{
		case NavRecoveryAction::Replan:
			m_follower.InvalidatePlan();
			break;
		case NavRecoveryAction::SideStep:
		case NavRecoveryAction::StepBack:
			m_pending = rec.action;   // the newest action wins
			break;
		case NavRecoveryAction::PenalizeReplan:
			m_penalties.Add(rec.cellX, rec.cellZ, nowMs, params.stuck);
			if (m_route.size() >= 2)
			{
				const float p = NavRouteProgressM(m_route.data(), (int)m_route.size(), botX, botZ);
				const NavRoutePoint ahead = PointAt(p + grid.Unit());
				m_penalties.Add(grid.CellOf(ahead.x), grid.CellOf(ahead.z), nowMs, params.stuck);
			}
			m_follower.InvalidatePlan();
			break;
		case NavRecoveryAction::Abandon:
			ev.ended = NavFollowEnd::StuckAbandon;
			Reset();
			return ev;
		default:
			break;
		}

		return ev;
	}

	inline NavDriveStep NavDrive::NextFollowStep(const NavGrid & grid, int64_t nowMs, float botX, float botZ,
		float maxStepM, const NavFollowDriveParams & params)
	{
		NavDriveStep step;
		if (m_mode != NavDriveMode::Follow)
			return step;
		// F5-64 D5: a held (deferred + stale) route produces no step at all, pending side/back
		// recovery included; the caller may send a stop packet once on the deferHold event.
		if (m_deferHeld)
			return step;
		if (!ValidCoord(botX) || !ValidCoord(botZ) || !(maxStepM > 0.0f))
			return step;

		// A pending stage-2/3 recovery action is consumed once.
		if (m_pending == NavRecoveryAction::SideStep || m_pending == NavRecoveryAction::StepBack)
		{
			const NavRecoveryAction action = m_pending;
			m_pending = NavRecoveryAction::None;

			float hx = 0.0f;
			float hz = 0.0f;
			if (m_route.size() >= 2 && m_routeLength > 0.0f)
			{
				const float p = NavRouteProgressM(m_route.data(), (int)m_route.size(), botX, botZ);
				const NavRoutePoint a = PointAt(p);
				const NavRoutePoint b = PointAt(p + 2.0f);
				const float hdx = b.x - a.x;
				const float hdz = b.z - a.z;
				const float hlen = std::sqrt(hdx * hdx + hdz * hdz);
				if (hlen > 1e-6f)
				{
					hx = hdx / hlen;
					hz = hdz / hlen;
				}
			}

			bool hasTarget = false;
			float tx = 0.0f;
			float tz = 0.0f;
			if (action == NavRecoveryAction::SideStep)
			{
				NavCell cell;
				if (NavPickSideStep(grid, botX, botZ, hx, hz, params.stuck, m_sideScratch, cell))
				{
					tx = grid.CellCenter(cell.x);
					tz = grid.CellCenter(cell.z);
					hasTarget = true;
				}
			}
			else if (hx != 0.0f || hz != 0.0f)
			{
				const float back = params.stepBackM < maxStepM ? params.stepBackM : maxStepM;
				tx = botX - hx * back;
				tz = botZ - hz * back;
				hasTarget = true;
			}

			if (hasTarget)
			{
				const float dx = tx - botX;
				const float dz = tz - botZ;
				const float d = std::sqrt(dx * dx + dz * dz);
				if (d > maxStepM)
				{
					const float s = maxStepM / d;
					tx = botX + dx * s;
					tz = botZ + dz * s;
				}

				const float qx = NavQuantiseM(tx);
				const float qz = NavQuantiseM(tz);
				const float rdx = qx - botX;
				const float rdz = qz - botZ;
				const float rd = std::sqrt(rdx * rdx + rdz * rdz);
				if (ValidCoord(qx) && ValidCoord(qz) && rd >= kMinTruncStepM
					&& CheckMoveChord(&grid, botX, botZ, qx, qz).verdict == ChordVerdict::Ok)
				{
					step.kind = NavDriveStep::Step;
					step.x = qx;
					step.z = qz;
					step.routeProgressM = (m_route.size() >= 2)
						? NavRouteProgressM(m_route.data(), (int)m_route.size(), qx, qz) : 0.0f;
					const float gdx = qx - m_goalX;
					const float gdz = qz - m_goalZ;
					step.distToGoalM = std::sqrt(gdx * gdx + gdz * gdz);
					m_stepFromX = botX;
					m_stepFromZ = botZ;
					return step;
				}
			}
		}

		// Arrival latch (D4), target hold (D3) or no route: no ordinary step.
		if (m_arrived || m_holding || m_route.size() < 2)
			return step;

		step = NextStep(grid, botX, botZ, maxStepM);
		if (step.kind == NavDriveStep::Step || step.kind == NavDriveStep::Arrived)
		{
			m_stepFromX = botX;
			m_stepFromZ = botZ;
			m_hasBlockedSince = false;
		}
		else if (step.kind == NavDriveStep::Blocked)
		{
			m_follower.InvalidatePlan();
			if (!m_hasBlockedSince)
			{
				m_hasBlockedSince = true;
				m_blockedSince = nowMs;
			}
			else if (nowMs - m_blockedSince >= (int64_t)params.blockedAbandonMs)
			{
				m_blockedAbandon = true;
			}
		}
		return step;
	}

	inline void NavDrive::OnPacketSent(int64_t tMs, const NavDriveStep & step)
	{
		if (m_mode != NavDriveMode::Follow)
			return;

		// D6: a packet whose displacement reverses the previous packet's direction starts a one-tick
		// monitor window reset (a U-turning target is not a stuck bot).
		const float vx = step.x - m_stepFromX;
		const float vz = step.z - m_stepFromZ;
		if (m_hasPktVec && m_monitor.Stage() == 0 && vx * m_pktVx + vz * m_pktVz < 0.0f)
			m_resetMonitorNext = true;
		m_pktVx = vx;
		m_pktVz = vz;
		m_hasPktVec = true;

		m_assess.OnPacketSent(tMs, step.x, step.z, step.routeProgressM, step.distToGoalM);
		m_activityMs = tMs;
		m_awaitingLogged = false;

		if (step.kind == NavDriveStep::Arrived)
		{
			m_arrived = true;
			m_assess.SetIntent(false, tMs);
		}
	}

	inline void NavDrive::OnPacketRejected(int64_t tMs)
	{
		if (m_mode != NavDriveMode::Follow)
			return;
		m_assess.OnPacketRejected(tMs);
	}
}
