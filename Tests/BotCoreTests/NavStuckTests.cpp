#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavDanger.h>
#include <BotCore/NavSmooth.h>
#include <BotCore/NavTrack.h>
#include <BotCore/NavStuck.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace
{
	using BotCore::NavCell;
	using BotCore::NavGrid;
	using BotCore::NavStuckKind;
	using BotCore::NavStuckParams;
	using BotCore::NavStuckDetector;
	using BotCore::NavStuckMonitor;
	using BotCore::NavRecoveryAction;
	using BotCore::NavRecoveryStep;
	using BotCore::NavStuckPenalties;
	using BotCore::NavCostLayer;
	using BotCore::NavCostField;
	using BotCore::NavPathfinder;
	using BotCore::NavPathResult;
	using BotCore::NavPathStatus;
	using BotCore::NavSearchParams;

	using PosFn = std::function<void(int64_t, float &, float &)>;
	using MovingFn = std::function<bool(int64_t)>;

	size_t CellIndex(int n, int x, int z)
	{
		return (size_t)x * (size_t)n + (size_t)z;
	}

	NavCell Cell(int x, int z)
	{
		NavCell c;
		c.x = x;
		c.z = z;
		return c;
	}

	std::vector<float> HeightZeros(int n)
	{
		return std::vector<float>((size_t)n * (size_t)n, 0.0f);
	}

	// Event grid with the border blocked and the interior open.
	std::vector<int16_t> RingEvents(int n)
	{
		std::vector<int16_t> events((size_t)n * (size_t)n, 1);
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (x == 0 || x == n - 1 || z == 0 || z == n - 1)
					events[CellIndex(n, x, z)] = 0;
			}
		}
		return events;
	}

	// Corridor: all blocked, open only for x = 1..n-2 and z = zLo..zHi.
	std::vector<int16_t> CorridorEvents(int n, int zLo, int zHi)
	{
		std::vector<int16_t> events((size_t)n * (size_t)n, 0);
		for (int x = 1; x <= n - 2; ++x)
		{
			for (int z = zLo; z <= zHi; ++z)
				events[CellIndex(n, x, z)] = 1;
		}
		return events;
	}

	// RingEvents plus a blocked column at x = 21 except the single gap z = 5.
	std::vector<int16_t> EastWallEvents(int n)
	{
		std::vector<int16_t> events = RingEvents(n);
		for (int z = 1; z <= n - 2; ++z)
		{
			if (z != 5)
				events[CellIndex(n, 21, z)] = 0;
		}
		return events;
	}

	NavGrid MakeNav(int n, float unit, const std::vector<int16_t> & events, const std::vector<float> & heights)
	{
		NavGrid grid;
		if (!grid.Init(n, unit, events, heights))
			CHECK(false);
		grid.Build();
		return grid;
	}

	double PercentileDouble(const std::vector<double> & sorted, double p)
	{
		if (sorted.empty())
			return 0.0;
		size_t idx = (size_t)(p * (double)(sorted.size() - 1));
		if (idx >= sorted.size())
			idx = sorted.size() - 1;
		return sorted[idx];
	}

	int FirstFire(const NavGrid & grid, int stepMs, int endMs, PosFn pos, NavStuckKind & kind,
		const NavStuckParams & params = NavStuckParams())
	{
		NavStuckDetector detector;
		for (int64_t t = 0; t <= endMs; t += stepMs)
		{
			float x = 0.0f;
			float z = 0.0f;
			pos(t, x, z);
			const NavStuckKind k = detector.Observe(grid, t, x, z, true, params);
			if (k != NavStuckKind::None)
			{
				kind = k;
				return (int)t;
			}
		}
		return -1;
	}

	struct Event
	{
		int64_t t = 0;
		NavRecoveryAction action = NavRecoveryAction::None;
		int stage = 0;
		NavStuckKind kind = NavStuckKind::None;
		int cellX = 0;
		int cellZ = 0;
		bool recovered = false;
		int recoveredStage = 0;
		int recoverMs = 0;
	};

	std::vector<Event> RunLadder(const NavGrid & grid, int endMs, PosFn pos, MovingFn moving,
		const NavStuckParams & params, NavStuckMonitor & monitor)
	{
		std::vector<Event> events;
		for (int64_t t = 0; t <= endMs; t += 100)
		{
			float x = 0.0f;
			float z = 0.0f;
			pos(t, x, z);
			const NavRecoveryStep step = monitor.Update(grid, t, x, z, moving(t), params);
			if (step.action != NavRecoveryAction::None || step.recovered)
			{
				Event e;
				e.t = t;
				e.action = step.action;
				e.stage = step.stage;
				e.kind = step.kind;
				e.cellX = step.cellX;
				e.cellZ = step.cellZ;
				e.recovered = step.recovered;
				e.recoveredStage = step.recoveredStage;
				e.recoverMs = step.recoverMs;
				events.push_back(e);
			}
		}
		return events;
	}

	void ExpectAction(const Event & e, int64_t t, NavRecoveryAction action, int stage, NavStuckKind kind,
		int cellX, int cellZ)
	{
		CHECK_EQ(e.t, t);
		CHECK(e.action == action);
		CHECK_EQ(e.stage, stage);
		CHECK(e.kind == kind);
		CHECK_EQ(e.cellX, cellX);
		CHECK_EQ(e.cellZ, cellZ);
		CHECK(!e.recovered);
	}

	void ExpectRecovered(const Event & e, int64_t t, int stage, int recoverMs)
	{
		CHECK_EQ(e.t, t);
		CHECK(e.action == NavRecoveryAction::None);
		CHECK(e.recovered);
		CHECK_EQ(e.recoveredStage, stage);
		CHECK_EQ(e.recoverMs, recoverMs);
	}

	bool ContainsCell(const std::vector<NavCell> & cells, int x, int z)
	{
		for (size_t i = 0; i < cells.size(); ++i)
		{
			if (cells[i].x == x && cells[i].z == z)
				return true;
		}
		return false;
	}

	struct Pt
	{
		float x = 0.0f;
		float z = 0.0f;
	};

	struct SimOut
	{
		bool reached = false;
		int64_t reachMs = 0;
		std::vector<int> stages;
		int episodes = 0;
		int abandons = 0;
		int recovered = 0;
		int maxRecoverMs = 0;
		int blockedTicks = 0;
	};

	// Test-only simulation: a virtual bot walks the smoothed cell path of the A* planner while the
	// monitor drives the recovery ladder and the penalties feed the cost layer. The real movement
	// lives in ActionExecutor/NavFollower; this only reproduces the plan's scenario.
	SimOut RunStuckSim(const NavGrid & grid, const std::vector<NavCell> & pillars, NavCell start,
		NavCell goal, int64_t maxMs)
	{
		const NavStuckParams params;
		NavStuckMonitor monitor;
		NavStuckPenalties penalties;
		NavCostLayer layer;
		NavPathfinder pathfinder;
		const NavSearchParams search;
		NavPathResult path;
		std::vector<NavCell> scratch;

		std::vector<Pt> wps;
		Pt passed;
		passed.x = grid.CellCenter(start.x);
		passed.z = grid.CellCenter(start.z);
		float x = passed.x;
		float z = passed.z;

		auto isPillar = [&pillars](NavCell c)
		{
			for (size_t i = 0; i < pillars.size(); ++i)
			{
				if (pillars[i] == c)
					return true;
			}
			return false;
		};

		auto plan = [&](float fx, float fz, int64_t now)
		{
			layer.Init(grid);
			penalties.Apply(grid, now, params, layer);
			NavCostField field;
			field.layer = &layer;
			NavCell from;
			from.x = grid.CellOf(fx);
			from.z = grid.CellOf(fz);
			pathfinder.Find(grid, from, goal, search, path, &field);
			wps.clear();
			if (path.status == NavPathStatus::Found)
			{
				for (size_t i = 1; i < path.cells.size(); ++i)
				{
					Pt p;
					p.x = grid.CellCenter(path.cells[i].x);
					p.z = grid.CellCenter(path.cells[i].z);
					wps.push_back(p);
				}
			}
		};

		plan(x, z, 0);

		SimOut out;
		bool abandoned = false;
		for (int64_t t = 0; t <= maxMs; t += 100)
		{
			if (wps.empty())
			{
				out.reached = true;
				out.reachMs = t;
				break;
			}

			const NavRecoveryStep step = monitor.Update(grid, t, x, z, true, params);
			if (step.recovered)
			{
				++out.recovered;
				if (step.recoverMs > out.maxRecoverMs)
					out.maxRecoverMs = step.recoverMs;
			}

			if (step.action != NavRecoveryAction::None)
			{
				out.stages.push_back(step.stage);
				switch (step.action)
				{
				case NavRecoveryAction::Replan:
					plan(x, z, t);
					break;
				case NavRecoveryAction::SideStep:
				{
					NavCell c;
					if (BotCore::NavPickSideStep(grid, x, z, wps[0].x - x, wps[0].z - z, params, scratch, c))
					{
						Pt p;
						p.x = grid.CellCenter(c.x);
						p.z = grid.CellCenter(c.z);
						plan(p.x, p.z, t);
						wps.insert(wps.begin(), p);
					}
					else
					{
						plan(x, z, t);
					}
					break;
				}
				case NavRecoveryAction::StepBack:
				{
					const Pt back = passed;
					plan(back.x, back.z, t);
					wps.insert(wps.begin(), back);
					break;
				}
				case NavRecoveryAction::PenalizeReplan:
					penalties.Add(step.cellX, step.cellZ, t, params);
					penalties.Add(grid.CellOf(wps[0].x), grid.CellOf(wps[0].z), t, params);
					plan(x, z, t);
					break;
				case NavRecoveryAction::Abandon:
					wps.clear();
					abandoned = true;
					break;
				default:
					break;
				}

				if (abandoned)
					break;
				if (wps.empty())
					continue;
			}

			// Movement: 0.6 m per 100 ms step, but never onto a hidden pillar cell.
			const float dx = wps[0].x - x;
			const float dz = wps[0].z - z;
			const float d = std::sqrt(dx * dx + dz * dz);
			float nx = 0.0f;
			float nz = 0.0f;
			if (d <= 0.6f)
			{
				nx = wps[0].x;
				nz = wps[0].z;
			}
			else
			{
				nx = x + dx / d * 0.6f;
				nz = z + dz / d * 0.6f;
			}

			NavCell nextCell;
			nextCell.x = grid.CellOf(nx);
			nextCell.z = grid.CellOf(nz);
			if (isPillar(nextCell))
			{
				++out.blockedTicks;
			}
			else
			{
				x = nx;
				z = nz;
				if (d <= 0.6f)
				{
					passed = wps[0];
					wps.erase(wps.begin());
				}
			}
		}

		out.episodes = monitor.Episodes();
		out.abandons = monitor.Abandons();
		return out;
	}

	std::string StagesStr(const std::vector<int> & stages)
	{
		std::string s;
		for (size_t i = 0; i < stages.size(); ++i)
		{
			if (i != 0)
				s += ",";
			s += std::to_string(stages[i]);
		}
		return s;
	}

	void CheckStages(const SimOut & out, const std::vector<int> & expected)
	{
		REQUIRE(out.stages.size() == expected.size());
		for (size_t i = 0; i < expected.size(); ++i)
			CHECK_EQ(out.stages[i], expected[i]);
	}
}

TEST_CASE("NavStuck_Detect_NoProgress")
{
	const NavGrid grid = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));
	const NavStuckParams params;
	NavStuckKind kind = NavStuckKind::None;

	// a: stationary.
	{
		NavStuckDetector detector;
		NavStuckKind at1400 = NavStuckKind::None;
		NavStuckKind at1600 = NavStuckKind::None;
		NavStuckKind at1700 = NavStuckKind::None;
		int first = -1;
		NavStuckKind firstKind = NavStuckKind::None;
		for (int64_t t = 0; t <= 1700; t += 100)
		{
			const NavStuckKind k = detector.Observe(grid, t, 82.0f, 82.0f, true, params);
			if (t == 1400)
				at1400 = k;
			if (t == 1600)
				at1600 = k;
			if (t == 1700)
				at1700 = k;
			if (first < 0 && k != NavStuckKind::None)
			{
				first = (int)t;
				firstKind = k;
			}
		}
		CHECK_EQ(first, 1500);
		CHECK(firstKind == NavStuckKind::NoProgress);
		CHECK(at1400 == NavStuckKind::None);
		CHECK(at1600 == NavStuckKind::NoProgress);
		CHECK(at1700 == NavStuckKind::NoProgress);
	}

	// b: 0.5 m/s.
	CHECK_EQ(FirstFire(grid, 100, 6000,
		[](int64_t t, float & x, float & z) { x = 82.0f + 0.05f * (float)(t / 100); z = 82.0f; }, kind), 1500);
	CHECK(kind == NavStuckKind::NoProgress);

	// c: 0.625 m/s (0.9375 m in 1.5 s).
	CHECK_EQ(FirstFire(grid, 100, 6000,
		[](int64_t t, float & x, float & z) { x = 82.0f + 0.0625f * (float)(t / 100); z = 82.0f; }, kind), 1500);
	CHECK(kind == NavStuckKind::NoProgress);

	// d: 1.25 m/s: never fires.
	CHECK_EQ(FirstFire(grid, 100, 6000,
		[](int64_t t, float & x, float & z) { x = 82.0f + 0.125f * (float)(t / 100); z = 82.0f; }, kind), -1);

	// e: exactly 1.0 m in 1.5 s: equality does not fire, first fire at 3000.
	{
		NavStuckDetector detector;
		int first = -1;
		NavStuckKind firstKind = NavStuckKind::None;
		for (int64_t t = 0; t <= 6000; t += 100)
		{
			const float x = (t < 1500) ? 82.0f : 83.0f;
			const NavStuckKind k = detector.Observe(grid, t, x, 82.0f, true, params);
			if (t >= 1500 && t <= 2900)
				CHECK(k == NavStuckKind::None);
			if (first < 0 && k != NavStuckKind::None)
			{
				first = (int)t;
				firstKind = k;
			}
		}
		CHECK_EQ(first, 3000);
		CHECK(firstKind == NavStuckKind::NoProgress);
	}

	// f: 0.999 m in 1.5 s.
	CHECK_EQ(FirstFire(grid, 100, 6000,
		[](int64_t t, float & x, float & z) { x = (t < 1500) ? 82.0f : 82.999f; z = 82.0f; }, kind), 1500);
	CHECK(kind == NavStuckKind::NoProgress);

	// g: sparse samples.
	{
		const int64_t times[4] = { 0, 700, 1480, 1500 };
		NavStuckDetector detector;
		NavStuckKind k[4];
		for (int i = 0; i < 4; ++i)
			k[i] = detector.Observe(grid, times[i], 82.0f, 82.0f, true, params);
		CHECK(k[0] == NavStuckKind::None);
		CHECK(k[1] == NavStuckKind::None);
		CHECK(k[2] == NavStuckKind::None);
		CHECK(k[3] == NavStuckKind::NoProgress);

		const int64_t times2[4] = { 0, 700, 1499, 1500 };
		NavStuckDetector detector2;
		for (int i = 0; i < 4; ++i)
			CHECK(detector2.Observe(grid, times2[i], 82.0f, 82.0f, true, params) == NavStuckKind::None);
	}

	// h: moving = false only at t = 1100 resets the window; first fire at 2700.
	{
		NavStuckDetector detector;
		bool sawReset = false;
		int first = -1;
		NavStuckKind firstKind = NavStuckKind::None;
		for (int64_t t = 0; t <= 4000; t += 100)
		{
			const bool moving = (t != 1100);
			const NavStuckKind k = detector.Observe(grid, t, 82.0f, 82.0f, moving, params);
			if (t == 1100)
			{
				CHECK_EQ(detector.Count(), 0);
				sawReset = true;
			}
			else if (t > 1100 && first < 0 && k != NavStuckKind::None)
			{
				first = (int)t;
				firstKind = k;
			}
		}
		CHECK(sawReset);
		CHECK_EQ(first, 2700);
		CHECK(firstKind == NavStuckKind::NoProgress);
	}

	// i: minimum spacing and capacity.
	{
		NavStuckDetector detector;
		for (int64_t t = 0; t <= 1400; t += 100)
			detector.Observe(grid, t, 82.0f, 82.0f, true, params);
		CHECK_EQ(detector.Count(), 15);
		CHECK(detector.Observe(grid, 1410, 82.0f, 82.0f, true, params) == NavStuckKind::None);
		CHECK_EQ(detector.Count(), 15);
		CHECK(detector.Observe(grid, 1400, 82.0f, 82.0f, true, params) == NavStuckKind::None);
		CHECK_EQ(detector.Count(), 15);
		CHECK(detector.Observe(grid, 1419, 82.0f, 82.0f, true, params) == NavStuckKind::None);
		CHECK_EQ(detector.Count(), 15);
		CHECK(detector.Observe(grid, 1420, 82.0f, 82.0f, true, params) == NavStuckKind::None);
		CHECK_EQ(detector.Count(), 16);
	}

	// j: 25 ms and 20 ms spacing.
	CHECK_EQ(FirstFire(grid, 25, 6000,
		[](int64_t, float & x, float & z) { x = 82.0f; z = 82.0f; }, kind), 1500);
	CHECK(kind == NavStuckKind::NoProgress);
	{
		NavStuckDetector detector;
		int first = -1;
		NavStuckKind firstKind = NavStuckKind::None;
		NavStuckKind atEnd = NavStuckKind::None;
		for (int64_t t = 0; t <= 6000; t += 20)
		{
			const NavStuckKind k = detector.Observe(grid, t, 82.0f, 82.0f, true, params);
			if (t == 6000)
				atEnd = k;
			if (first < 0 && k != NavStuckKind::None)
			{
				first = (int)t;
				firstKind = k;
			}
		}
		CHECK_EQ(first, 1500);
		CHECK(firstKind == NavStuckKind::NoProgress);
		CHECK(atEnd == NavStuckKind::NoProgress);
		CHECK_EQ(detector.Count(), NavStuckDetector::kCapacity);
	}

	// k: disabled rules.
	{
		NavStuckParams p = params;
		p.noProgressMs = 800;
		CHECK_EQ(FirstFire(grid, 100, 6000,
			[](int64_t, float & x, float & z) { x = 82.0f; z = 82.0f; }, kind, p), 800);
		CHECK(kind == NavStuckKind::NoProgress);

		p = params;
		p.noProgressMs = 0;
		CHECK_EQ(FirstFire(grid, 100, 6000,
			[](int64_t, float & x, float & z) { x = 82.0f; z = 82.0f; }, kind, p), -1);

		p = params;
		p.minProgressM = 0.0f;
		CHECK_EQ(FirstFire(grid, 100, 6000,
			[](int64_t, float & x, float & z) { x = 82.0f; z = 82.0f; }, kind, p), -1);
	}
}

TEST_CASE("NavStuck_Detect_Oscillation")
{
	const NavGrid grid = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));
	const NavStuckParams params;
	NavStuckKind kind = NavStuckKind::None;

	// a: A <-> B, fires at 1500.
	{
		NavStuckDetector detector;
		NavStuckKind at1400 = NavStuckKind::None;
		int first = -1;
		NavStuckKind firstKind = NavStuckKind::None;
		for (int64_t t = 0; t <= 1700; t += 100)
		{
			const float x = ((t / 500) % 2 != 0) ? 86.0f : 82.0f;
			const NavStuckKind k = detector.Observe(grid, t, x, 82.0f, true, params);
			if (t == 1400)
				at1400 = k;
			if (first < 0 && k != NavStuckKind::None)
			{
				first = (int)t;
				firstKind = k;
			}
		}
		CHECK_EQ(first, 1500);
		CHECK(firstKind == NavStuckKind::Oscillation);
		CHECK(at1400 == NavStuckKind::None);
	}

	CHECK_EQ(FirstFire(grid, 100, 6000,
		[](int64_t t, float & x, float & z) { x = ((t / 500) % 2 != 0) ? 86.0f : 82.0f; z = 82.0f; }, kind),
		1500);
	CHECK(kind == NavStuckKind::Oscillation);

	// a disabled: oscSwings = 0 and oscWindowMs = 0.
	{
		NavStuckParams p = params;
		p.oscSwings = 0;
		CHECK_EQ(FirstFire(grid, 100, 6000,
			[](int64_t t, float & x, float & z) { x = ((t / 500) % 2 != 0) ? 86.0f : 82.0f; z = 82.0f; }, kind, p),
			-1);
		p = params;
		p.oscWindowMs = 0;
		CHECK_EQ(FirstFire(grid, 100, 6000,
			[](int64_t t, float & x, float & z) { x = ((t / 500) % 2 != 0) ? 86.0f : 82.0f; z = 82.0f; }, kind, p),
			-1);
	}

	// b: oscSwings = 4.
	{
		NavStuckParams p = params;
		p.oscSwings = 4;
		CHECK_EQ(FirstFire(grid, 100, 6000,
			[](int64_t t, float & x, float & z) { x = ((t / 500) % 2 != 0) ? 86.0f : 82.0f; z = 82.0f; }, kind, p),
			2000);
		CHECK(kind == NavStuckKind::Oscillation);
	}

	// c: oscWindowMs = 1000.
	{
		NavStuckParams p = params;
		p.oscWindowMs = 1000;
		CHECK_EQ(FirstFire(grid, 100, 6000,
			[](int64_t t, float & x, float & z) { x = ((t / 500) % 2 != 0) ? 86.0f : 82.0f; z = 82.0f; }, kind, p),
			-1);
	}

	// d: slow swing, fires at 4500.
	CHECK_EQ(FirstFire(grid, 100, 12000,
		[](int64_t t, float & x, float & z) { x = ((t / 1500) % 2 != 0) ? 86.0f : 82.0f; z = 82.0f; }, kind),
		4500);
	CHECK(kind == NavStuckKind::Oscillation);

	// e: vertical swing.
	CHECK_EQ(FirstFire(grid, 100, 6000,
		[](int64_t t, float & x, float & z) { x = 82.0f; z = ((t / 500) % 2 != 0) ? 86.0f : 82.0f; }, kind),
		1500);
	CHECK(kind == NavStuckKind::Oscillation);

	// f: straight walk, never fires.
	CHECK_EQ(FirstFire(grid, 100, 20000,
		[](int64_t t, float & x, float & z) { x = 10.0f + 0.6f * (float)(t / 100); z = 82.0f; }, kind), -1);

	// g: three-cell patrol, never fires.
	CHECK_EQ(FirstFire(grid, 100, 12000,
		[](int64_t t, float & x, float & z)
		{
			const int idx = (int)((t / 1000) % 3);
			x = (idx == 0) ? 82.0f : (idx == 1 ? 86.0f : 90.0f);
			z = 82.0f;
		},
		kind), -1);

	// h: tremor across two cells, NoProgress wins.
	CHECK_EQ(FirstFire(grid, 100, 6000,
		[](int64_t t, float & x, float & z) { x = ((t / 500) % 2 != 0) ? 84.1f : 83.9f; z = 82.0f; }, kind),
		1500);
	CHECK(kind == NavStuckKind::NoProgress);

	// i: return to the 1.5 s old position: NoProgress.
	CHECK_EQ(FirstFire(grid, 100, 6000,
		[](int64_t t, float & x, float & z) { x = ((t / 1000) % 2 != 0) ? 86.0f : 82.0f; z = 82.0f; }, kind),
		2000);
	CHECK(kind == NavStuckKind::NoProgress);
}

TEST_CASE("NavStuck_Recovery_Ladder")
{
	const NavGrid grid = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));
	const NavStuckParams params;

	// L1: stationary ladder, abandon, then a fresh episode.
	{
		NavStuckMonitor monitor;
		const std::vector<Event> e = RunLadder(grid, 8000,
			[](int64_t, float & x, float & z) { x = 82.0f; z = 82.0f; },
			[](int64_t) { return true; }, params, monitor);
		REQUIRE(e.size() == 7);
		ExpectAction(e[0], 1500, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[1], 2000, NavRecoveryAction::SideStep, 2, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[2], 3000, NavRecoveryAction::StepBack, 3, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[3], 4500, NavRecoveryAction::PenalizeReplan, 4, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[4], 5500, NavRecoveryAction::Abandon, 5, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[5], 7100, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[6], 7600, NavRecoveryAction::SideStep, 2, NavStuckKind::NoProgress, 20, 20);
		CHECK_EQ(monitor.Episodes(), 2);
		CHECK_EQ(monitor.Abandons(), 1);
		CHECK_EQ(monitor.Stage(), 2);
	}

	// L2: recovery at exactly 1.0 m.
	{
		NavStuckMonitor monitor;
		const std::vector<Event> e = RunLadder(grid, 1700,
			[](int64_t t, float & x, float & z) { x = (t < 1600) ? 82.0f : 83.0f; z = 82.0f; },
			[](int64_t) { return true; }, params, monitor);
		REQUIRE(e.size() == 2);
		ExpectAction(e[0], 1500, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		ExpectRecovered(e[1], 1600, 1, 100);
		CHECK_EQ(monitor.Stage(), 0);
		CHECK_EQ(monitor.Episodes(), 1);
	}

	// L2b: 0.999 m is not enough.
	{
		NavStuckMonitor monitor;
		const std::vector<Event> e = RunLadder(grid, 2200,
			[](int64_t t, float & x, float & z) { x = (t < 1600) ? 82.0f : 82.999f; z = 82.0f; },
			[](int64_t) { return true; }, params, monitor);
		REQUIRE(e.size() == 2);
		ExpectAction(e[0], 1500, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[1], 2000, NavRecoveryAction::SideStep, 2, NavStuckKind::NoProgress, 20, 20);
	}

	// L3: recovery, escalation, abandon.
	{
		NavStuckMonitor monitor;
		const std::vector<Event> e = RunLadder(grid, 8000,
			[](int64_t t, float & x, float & z) { x = (t < 2100) ? 82.0f : 86.0f; z = 82.0f; },
			[](int64_t) { return true; }, params, monitor);
		REQUIRE(e.size() == 7);
		ExpectAction(e[0], 1500, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[1], 2000, NavRecoveryAction::SideStep, 2, NavStuckKind::NoProgress, 20, 20);
		ExpectRecovered(e[2], 2100, 2, 600);
		ExpectAction(e[3], 3600, NavRecoveryAction::StepBack, 3, NavStuckKind::NoProgress, 21, 20);
		ExpectAction(e[4], 5100, NavRecoveryAction::PenalizeReplan, 4, NavStuckKind::NoProgress, 21, 20);
		ExpectAction(e[5], 6100, NavRecoveryAction::Abandon, 5, NavStuckKind::NoProgress, 21, 20);
		ExpectAction(e[6], 7700, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 21, 20);
		CHECK_EQ(monitor.Episodes(), 3);
		CHECK_EQ(monitor.Abandons(), 1);
	}

	// L3b: escalation window boundary (equality climbs).
	{
		NavStuckParams p = params;
		p.escalateWindowMs = 1500;
		NavStuckMonitor monitor;
		const std::vector<Event> e = RunLadder(grid, 3300,
			[](int64_t t, float & x, float & z) { x = (t < 1700) ? 82.0f : 83.2f; z = 82.0f; },
			[](int64_t) { return true; }, p, monitor);
		REQUIRE(e.size() == 3);
		ExpectAction(e[0], 1500, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		ExpectRecovered(e[1], 1700, 1, 200);
		ExpectAction(e[2], 3200, NavRecoveryAction::SideStep, 2, NavStuckKind::NoProgress, 20, 20);
		CHECK_EQ(monitor.Episodes(), 2);
	}
	{
		NavStuckParams p = params;
		p.escalateWindowMs = 1499;
		NavStuckMonitor monitor;
		const std::vector<Event> e = RunLadder(grid, 3300,
			[](int64_t t, float & x, float & z) { x = (t < 1700) ? 82.0f : 83.2f; z = 82.0f; },
			[](int64_t) { return true; }, p, monitor);
		REQUIRE(e.size() == 3);
		ExpectAction(e[0], 1500, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		ExpectRecovered(e[1], 1700, 1, 200);
		ExpectAction(e[2], 3200, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		CHECK_EQ(monitor.Episodes(), 2);
	}

	// L4: recovery at stage 4, then a detection abandons directly.
	{
		NavStuckMonitor monitor;
		const std::vector<Event> e = RunLadder(grid, 7000,
			[](int64_t t, float & x, float & z) { x = (t < 4600) ? 82.0f : 83.5f; z = 82.0f; },
			[](int64_t) { return true; }, params, monitor);
		REQUIRE(e.size() == 6);
		ExpectAction(e[0], 1500, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[1], 2000, NavRecoveryAction::SideStep, 2, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[2], 3000, NavRecoveryAction::StepBack, 3, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(e[3], 4500, NavRecoveryAction::PenalizeReplan, 4, NavStuckKind::NoProgress, 20, 20);
		ExpectRecovered(e[4], 4600, 4, 3100);
		ExpectAction(e[5], 6100, NavRecoveryAction::Abandon, 5, NavStuckKind::NoProgress, 20, 20);
		CHECK_EQ(monitor.Episodes(), 2);
		CHECK_EQ(monitor.Abandons(), 1);
		CHECK_EQ(monitor.Stage(), 0);
	}

	// L5: moving = false cancels the running recovery.
	{
		NavStuckMonitor monitor;
		std::vector<Event> events;
		for (int64_t t = 0; t <= 4200; t += 100)
		{
			const bool moving = (t != 2100);
			const NavRecoveryStep step = monitor.Update(grid, t, 82.0f, 82.0f, moving, params);
			if (step.action != NavRecoveryAction::None || step.recovered)
			{
				Event e;
				e.t = t;
				e.action = step.action;
				e.stage = step.stage;
				e.kind = step.kind;
				e.cellX = step.cellX;
				e.cellZ = step.cellZ;
				e.recovered = step.recovered;
				e.recoveredStage = step.recoveredStage;
				e.recoverMs = step.recoverMs;
				events.push_back(e);
			}
			if (t == 2100)
				CHECK_EQ(monitor.Stage(), 0);
		}
		REQUIRE(events.size() == 4);
		ExpectAction(events[0], 1500, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(events[1], 2000, NavRecoveryAction::SideStep, 2, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(events[2], 3700, NavRecoveryAction::Replan, 1, NavStuckKind::NoProgress, 20, 20);
		ExpectAction(events[3], 4200, NavRecoveryAction::SideStep, 2, NavStuckKind::NoProgress, 20, 20);
	}

	// L6: the time gate.
	{
		NavStuckMonitor monitor;
		for (int64_t t = 0; t <= 1500; t += 100)
		{
			const NavRecoveryStep step = monitor.Update(grid, t, 82.0f, 82.0f, true, params);
			if (t == 1500)
			{
				CHECK(step.action == NavRecoveryAction::Replan);
				CHECK_EQ(step.stage, 1);
			}
		}
		CHECK_EQ(monitor.Stage(), 1);

		NavRecoveryStep s = monitor.Update(grid, 1500, 200.0f, 82.0f, true, params);
		CHECK(s.action == NavRecoveryAction::None);
		CHECK(!s.recovered);
		CHECK_EQ(monitor.Stage(), 1);
		CHECK_EQ(monitor.Episodes(), 1);

		s = monitor.Update(grid, 1400, 200.0f, 82.0f, true, params);
		CHECK(s.action == NavRecoveryAction::None);
		CHECK(!s.recovered);
		CHECK_EQ(monitor.Stage(), 1);
		CHECK_EQ(monitor.Episodes(), 1);

		s = monitor.Update(grid, 1600, 200.0f, 82.0f, true, params);
		CHECK(s.recovered);
		CHECK_EQ(s.recoveredStage, 1);
		CHECK_EQ(s.recoverMs, 100);
		CHECK_EQ(monitor.Stage(), 0);
	}

	// L7: oscillation climbs and ends in abandon.
	{
		NavStuckMonitor monitor;
		const std::vector<Event> e = RunLadder(grid, 9500,
			[](int64_t t, float & x, float & z) { x = ((t / 500) % 2 != 0) ? 86.0f : 82.0f; z = 82.0f; },
			[](int64_t) { return true; }, params, monitor);
		REQUIRE(e.size() == 9);
		ExpectAction(e[0], 1500, NavRecoveryAction::Replan, 1, NavStuckKind::Oscillation, 21, 20);
		ExpectRecovered(e[1], 2000, 1, 500);
		ExpectAction(e[2], 3500, NavRecoveryAction::SideStep, 2, NavStuckKind::Oscillation, 21, 20);
		ExpectRecovered(e[3], 4000, 2, 500);
		ExpectAction(e[4], 5500, NavRecoveryAction::StepBack, 3, NavStuckKind::Oscillation, 21, 20);
		ExpectRecovered(e[5], 6000, 3, 500);
		ExpectAction(e[6], 7500, NavRecoveryAction::PenalizeReplan, 4, NavStuckKind::Oscillation, 21, 20);
		ExpectRecovered(e[7], 8000, 4, 500);
		ExpectAction(e[8], 9500, NavRecoveryAction::Abandon, 5, NavStuckKind::Oscillation, 21, 20);
		CHECK_EQ(monitor.Episodes(), 5);
		CHECK_EQ(monitor.Abandons(), 1);
	}

	// L8: Reset.
	{
		NavStuckMonitor monitor;
		for (int64_t t = 0; t <= 2000; t += 100)
			monitor.Update(grid, t, 82.0f, 82.0f, true, params);
		CHECK_EQ(monitor.Stage(), 2);
		CHECK_EQ(monitor.Episodes(), 1);

		monitor.Reset();
		CHECK_EQ(monitor.Stage(), 0);
		CHECK_EQ(monitor.Episodes(), 0);
		CHECK_EQ(monitor.Abandons(), 0);

		const NavRecoveryStep s = monitor.Update(grid, 0, 82.0f, 82.0f, true, params);
		CHECK(s.action == NavRecoveryAction::None);
		CHECK(!s.recovered);
		CHECK_EQ(monitor.Stage(), 0);
	}

	// L9: first call with moving = false.
	{
		NavStuckMonitor monitor;
		const NavRecoveryStep s = monitor.Update(grid, 0, 82.0f, 82.0f, false, params);
		CHECK(s.action == NavRecoveryAction::None);
		CHECK(!s.recovered);
		CHECK_EQ(monitor.Stage(), 0);
	}
}

TEST_CASE("NavStuck_SideStep")
{
	const float unit = 4.0f;
	const NavGrid flat = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
	const NavGrid corridor1 = MakeNav(40, unit, CorridorEvents(40, 20, 20), HeightZeros(40));
	const NavGrid corridor3 = MakeNav(40, unit, CorridorEvents(40, 19, 21), HeightZeros(40));
	const NavGrid corridor5 = MakeNav(40, unit, CorridorEvents(40, 18, 22), HeightZeros(40));
	const NavGrid eastWall = MakeNav(40, unit, EastWallEvents(40), HeightZeros(40));
	const NavStuckParams params;
	std::vector<NavCell> scratch;

	auto expect = [&](const NavGrid & grid, float x, float z, float hx, float hz,
		const NavStuckParams & p, bool found, int ex, int ez)
	{
		NavCell out;
		out.x = -7;
		out.z = -7;
		const bool ok = BotCore::NavPickSideStep(grid, x, z, hx, hz, p, scratch, out);
		CHECK_EQ((int)ok, (int)found);
		if (found)
		{
			CHECK_EQ(out.x, ex);
			CHECK_EQ(out.z, ez);
		}
		else
		{
			CHECK_EQ(out.x, -7);
			CHECK_EQ(out.z, -7);
		}
	};

	// A
	expect(flat, 6.0f, 82.0f, 1.0f, 0.0f, params, true, 2, 19);
	// B
	expect(flat, 6.0f, 82.0f, 0.0f, 0.0f, params, true, 2, 20);
	// C
	expect(flat, 6.0f, 82.0f, 0.0f, 1.0f, params, true, 2, 20);
	// D
	expect(flat, 6.0f, 82.0f, -1.0f, 0.0f, params, true, 2, 19);
	// E
	expect(flat, 82.0f, 82.0f, 1.0f, 0.0f, params, true, 20, 19);
	// F
	expect(flat, 82.0f, 82.0f, 0.0f, 0.0f, params, true, 19, 20);
	// G
	expect(flat, 82.0f, 82.0f, 0.0f, -1.0f, params, true, 19, 20);
	// H
	expect(corridor1, 42.0f, 82.0f, 1.0f, 0.0f, params, false, 0, 0);
	expect(corridor1, 42.0f, 82.0f, 0.0f, 0.0f, params, false, 0, 0);
	// I
	expect(corridor3, 42.0f, 78.0f, 1.0f, 0.0f, params, true, 10, 20);
	// J
	expect(corridor3, 42.0f, 82.0f, 1.0f, 0.0f, params, false, 0, 0);
	// K
	expect(corridor3, 42.0f, 82.0f, 0.0f, 0.0f, params, true, 9, 20);
	// L
	expect(corridor5, 42.0f, 78.0f, 1.0f, 0.0f, params, true, 10, 20);
	// M
	expect(corridor5, 42.0f, 74.0f, 1.0f, 0.0f, params, true, 10, 19);

	// N
	{
		NavStuckParams p = params;
		p.sideStepMaxM = 2.0f;
		expect(flat, 6.0f, 82.0f, 1.0f, 0.0f, p, false, 0, 0);
		p.sideStepMaxM = 4.0f;
		expect(flat, 6.0f, 82.0f, 1.0f, 0.0f, p, false, 0, 0);
		p.sideStepMaxM = 8.0f;
		expect(flat, 6.0f, 82.0f, 1.0f, 0.0f, p, true, 2, 19);
	}

	// O
	{
		NavStuckParams p = params;
		p.sideStepClearance = 1;
		expect(flat, 6.0f, 82.0f, 1.0f, 0.0f, p, true, 1, 19);
		p.sideStepClearance = 3;
		expect(flat, 6.0f, 82.0f, 1.0f, 0.0f, p, true, 3, 18);
	}

	// P
	expect(eastWall, 82.0f, 82.0f, 1.0f, 0.0f, params, true, 19, 19);
	// Q
	expect(eastWall, 82.0f, 82.0f, 0.0f, 0.0f, params, true, 19, 20);
	// R
	expect(flat, 2.0f, 2.0f, 1.0f, 0.0f, params, false, 0, 0);
	expect(flat, -5.0f, 82.0f, 1.0f, 0.0f, params, false, 0, 0);
}

TEST_CASE("NavStuck_Penalties")
{
	const float unit = 4.0f;
	const NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
	const NavStuckParams params;
	const NavSearchParams search;

	// Basic lifetime.
	{
		NavStuckPenalties penalties;
		penalties.Add(20, 20, 1000, params);
		CHECK_EQ(penalties.Count(1000), 1);
		CHECK(penalties.Active(20, 20, 60999));
		CHECK(!penalties.Active(20, 20, 61000));
		CHECK_EQ(penalties.Count(60999), 1);
		CHECK_EQ(penalties.Count(61000), 0);

		// Renewal extends, never shortens.
		penalties.Add(20, 20, 2000, params);
		CHECK(penalties.Active(20, 20, 61999));
		CHECK(!penalties.Active(20, 20, 62000));
		CHECK_EQ(penalties.Count(2000), 1);
		penalties.Add(20, 20, 1500, params);
		CHECK(penalties.Active(20, 20, 61999));
		CHECK_EQ(penalties.Count(1500), 1);
	}

	// penaltyMs = 0 is a no-op.
	{
		NavStuckParams p = params;
		p.penaltyMs = 0;
		NavStuckPenalties penalties;
		penalties.Add(20, 20, 0, p);
		CHECK_EQ(penalties.Count(0), 0);
	}

	// Capacity: the earliest expiring entry is replaced.
	{
		NavStuckPenalties penalties;
		for (int i = 0; i < 32; ++i)
		{
			NavStuckParams p = params;
			p.penaltyMs = 1000 + 10 * i;
			penalties.Add(i, 1, 0, p);
		}
		CHECK_EQ(penalties.Count(0), 32);
		penalties.Add(100, 1, 0, params);
		CHECK(!penalties.Active(0, 1, 0));
		CHECK(penalties.Active(1, 1, 0));
		CHECK(penalties.Active(100, 1, 0));
		CHECK_EQ(penalties.Count(0), 32);
		penalties.Clear();
		CHECK_EQ(penalties.Count(0), 0);
	}

	// Apply.
	{
		NavStuckPenalties penalties;
		penalties.Add(20, 20, 1000, params);
		penalties.Add(20, 20, 2000, params);
		NavCostLayer layer;
		layer.Init(grid);
		penalties.Apply(grid, 2000, params, layer);
		CHECK_EQ((int)layer.Danger(20, 20), 255);
		CHECK_EQ((int)layer.Danger(19, 20), 0);
		CHECK_EQ((int)layer.Danger(21, 20), 0);
		CHECK_EQ((int)layer.Danger(20, 21), 0);

		NavCostLayer expired;
		expired.Init(grid);
		penalties.Apply(grid, 62000, params, expired);
		CHECK_EQ((int)expired.Danger(20, 20), 0);

		NavStuckParams half = params;
		half.penaltyWeight = 0.5f;
		NavCostLayer layerHalf;
		layerHalf.Init(grid);
		penalties.Apply(grid, 2000, half, layerHalf);
		CHECK_EQ((int)layerHalf.Danger(20, 20), 128);

		NavStuckParams zero = params;
		zero.penaltyWeight = 0.0f;
		NavCostLayer layerZero;
		layerZero.Init(grid);
		penalties.Apply(grid, 2000, zero, layerZero);
		CHECK_EQ((int)layerZero.Danger(20, 20), 0);

		// Uninitialized / mismatched layer: nothing changes.
		NavCostLayer uninit;
		penalties.Apply(grid, 2000, params, uninit);
		CHECK_EQ((int)uninit.Danger(20, 20), 0);
		NavGrid small = MakeNav(20, unit, RingEvents(20), HeightZeros(20));
		NavCostLayer smallLayer;
		smallLayer.Init(small);
		penalties.Apply(grid, 2000, params, smallLayer);
		CHECK_EQ((int)smallLayer.Danger(15, 15), 0);

		// Forbidden flag is preserved.
		NavCostLayer forbid;
		forbid.Init(grid);
		forbid.AddForbidDisc(82.0f, 82.0f, 3.0f);
		penalties.Apply(grid, 2000, params, forbid);
		CHECK_EQ((int)forbid.Danger(20, 20), 255);
		CHECK(forbid.Forbidden(20, 20));
	}

	// A*: flat grid, detour around the penalized cell.
	{
		NavPathfinder pathfinder;
		NavPathResult path;

		pathfinder.Find(grid, Cell(5, 20), Cell(35, 20), search, path, nullptr);
		CHECK(path.status == NavPathStatus::Found);
		CHECK(std::fabs(path.length - 120.0f) <= 1e-3f);
		CHECK_EQ((int)path.cells.size(), 31);
		CHECK(ContainsCell(path.cells, 20, 20));

		NavStuckPenalties penalties;
		penalties.Add(20, 20, 1000, params);
		penalties.Add(20, 20, 2000, params);
		NavCostLayer layer;
		layer.Init(grid);
		penalties.Apply(grid, 2000, params, layer);
		NavCostField field;
		field.layer = &layer;

		pathfinder.Find(grid, Cell(5, 20), Cell(35, 20), search, path, &field);
		CHECK(path.status == NavPathStatus::Found);
		CHECK(std::fabs(path.length - 123.3137f) <= 1e-3f);
		CHECK(std::fabs(path.cost - 123.3137f) <= 1e-3f);
		CHECK_EQ((int)path.cells.size(), 31);
		CHECK(!ContainsCell(path.cells, 20, 20));
		CHECK(ContainsCell(path.cells, 20, 19));

		NavCostLayer expiredLayer;
		expiredLayer.Init(grid);
		penalties.Apply(grid, 62000, params, expiredLayer);
		NavCostField expiredField;
		expiredField.layer = &expiredLayer;
		pathfinder.Find(grid, Cell(5, 20), Cell(35, 20), search, path, &expiredField);
		CHECK(path.status == NavPathStatus::Found);
		CHECK(std::fabs(path.length - 120.0f) <= 1e-3f);
		CHECK(ContainsCell(path.cells, 20, 20));
	}

	// A single-cell corridor: the penalty does not block the route.
	{
		const NavGrid corridor = MakeNav(40, unit, CorridorEvents(40, 20, 20), HeightZeros(40));
		NavPathfinder pathfinder;
		NavPathResult path;

		NavCostLayer empty;
		empty.Init(corridor);
		NavCostField emptyField;
		emptyField.layer = &empty;
		pathfinder.Find(corridor, Cell(5, 20), Cell(35, 20), search, path, &emptyField);
		CHECK(path.status == NavPathStatus::Found);
		CHECK(std::fabs(path.cost - 180.0f) <= 1e-3f);

		NavStuckPenalties penalties;
		penalties.Add(20, 20, 1000, params);
		NavCostLayer layer;
		layer.Init(corridor);
		penalties.Apply(corridor, 2000, params, layer);
		NavCostField field;
		field.layer = &layer;
		pathfinder.Find(corridor, Cell(5, 20), Cell(35, 20), search, path, &field);
		CHECK(path.status == NavPathStatus::Found);
		CHECK(std::fabs(path.length - 120.0f) <= 1e-3f);
		CHECK(std::fabs(path.cost - 196.0f) <= 1e-3f);
	}
}

TEST_CASE("NavStuck_Sim")
{
	const NavGrid grid = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));
	const NavCell start = Cell(5, 20);
	const NavCell goal = Cell(35, 20);

	// Control: no hidden obstacle.
	{
		std::vector<NavCell> none;
		const SimOut out = RunStuckSim(grid, none, start, goal, 60000);
		CHECK(out.reached);
		CHECK(out.reachMs >= 20000 && out.reachMs <= 22000);
		CHECK_EQ(out.episodes, 0);
		CHECK_EQ(out.blockedTicks, 0);
		std::printf("NAVSTUCK sim control: reached=%d reach_ms=%lld episodes=%d blocked_ticks=%d\n",
			out.reached ? 1 : 0, (long long)out.reachMs, out.episodes, out.blockedTicks);
	}

	// Pillar: one hidden cell.
	{
		std::vector<NavCell> pillars;
		pillars.push_back(Cell(20, 20));
		const SimOut out = RunStuckSim(grid, pillars, start, goal, 60000);
		CHECK(out.reached);
		CHECK(out.reachMs >= 24000 && out.reachMs <= 31000);
		CheckStages(out, { 1, 2, 3, 4 });
		CHECK_EQ(out.episodes, 3);
		CHECK_EQ(out.abandons, 0);
		CHECK_EQ(out.recovered, 3);
		CHECK(out.maxRecoverMs <= 5000);
		CHECK(out.maxRecoverMs == 700);
		CHECK(out.blockedTicks > 0);
		std::printf("NAVSTUCK sim pillar: reached=%d reach_ms=%lld episodes=%d abandons=%d recovered=%d max_recover_ms=%d blocked_ticks=%d stages=%s\n",
			out.reached ? 1 : 0, (long long)out.reachMs, out.episodes, out.abandons, out.recovered,
			out.maxRecoverMs, out.blockedTicks, StagesStr(out.stages).c_str());
	}

	// Wall of three hidden cells: the bot gives up.
	{
		std::vector<NavCell> pillars;
		pillars.push_back(Cell(20, 19));
		pillars.push_back(Cell(20, 20));
		pillars.push_back(Cell(20, 21));
		const SimOut out = RunStuckSim(grid, pillars, start, goal, 60000);
		CHECK(!out.reached);
		CheckStages(out, { 1, 2, 3, 4, 5 });
		CHECK_EQ(out.episodes, 3);
		CHECK_EQ(out.abandons, 1);
		CHECK_EQ(out.recovered, 2);
		CHECK(out.maxRecoverMs == 700);
		CHECK(out.blockedTicks > 0);
		std::printf("NAVSTUCK sim wall3: reached=%d reach_ms=%lld episodes=%d abandons=%d recovered=%d max_recover_ms=%d blocked_ticks=%d stages=%s\n",
			out.reached ? 1 : 0, (long long)out.reachMs, out.episodes, out.abandons, out.recovered,
			out.maxRecoverMs, out.blockedTicks, StagesStr(out.stages).c_str());
	}
}

TEST_CASE("NavStuck_SideStep_RealMap")
{
	NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVSTUCK real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	const NavStuckParams params;
	std::vector<NavCell> scratch;

	int walk = 0;
	int sidestepH = 0;
	int sidestepN = 0;
	int violations = 0;
	int clr2Cells = 0;
	int clr2Found = 0;
	std::vector<double> durations;

	NavCell out;
	for (int x = 0; x < grid.Size(); ++x)
	{
		for (int z = 0; z < grid.Size(); ++z)
		{
			if (!grid.Walk(x, z))
				continue;
			++walk;

			const float cx = grid.CellCenter(x);
			const float cz = grid.CellCenter(z);
			const bool clr2 = (int)grid.Clearance(x, z) >= 2;
			if (clr2)
				++clr2Cells;

			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			const bool foundH = BotCore::NavPickSideStep(grid, cx, cz, 1.0f, 0.0f, params, scratch, out);
			const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
			durations.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

			if (foundH)
			{
				++sidestepH;
				if (clr2)
					++clr2Found;

				const NavCell from = Cell(x, z);
				const float dx = (float)(out.x - from.x);
				const float dz = (float)(out.z - from.z);
				const float dot = dx * 1.0f + dz * 0.0f;
				const float centerDx = grid.CellCenter(out.x) - cx;
				const float centerDz = grid.CellCenter(out.z) - cz;
				const float centerD2 = centerDx * centerDx + centerDz * centerDz;

				bool ok = grid.Walk(out.x, out.z);
				ok = ok && (int)grid.Clearance(out.x, out.z) >= 2;
				ok = ok && BotCore::NavLineClear(grid, from, out);
				ok = ok && (out.x != from.x || out.z != from.z);
				ok = ok && centerD2 <= (12.0f + 0.01f) * (12.0f + 0.01f);
				ok = ok && dot * dot <= 0.5f * (dx * dx + dz * dz) * 1.0f;
				if (!ok)
					++violations;
			}

			if (BotCore::NavPickSideStep(grid, cx, cz, 0.0f, 0.0f, params, scratch, out))
				++sidestepN;
		}
	}

	std::sort(durations.begin(), durations.end());
	const double msP95 = PercentileDouble(durations, 0.95);

	CHECK_EQ(walk, 88508);
	CHECK_EQ(sidestepH, 86017);
	CHECK_EQ(sidestepN, 86968);
	CHECK_EQ(violations, 0);
	CHECK_EQ(clr2Cells, 66265);
	CHECK_EQ(clr2Found, 66003);
#ifndef _DEBUG
	CHECK(msP95 <= 0.2);
#endif

	std::printf("NAVSTUCK real: walk=%d sidestep_h=%d sidestep_n=%d violations=%d clr2_cells=%d clr2_found=%d ms_p95=%.4f\n",
		walk, sidestepH, sidestepN, violations, clr2Cells, clr2Found, msP95);
}
