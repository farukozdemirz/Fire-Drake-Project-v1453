#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavSmooth.h>
#include <BotCore/NavTrack.h>
#include <BotCore/NavReach.h>
#include <BotCore/Rng.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
	size_t CellIndex(int n, int x, int z)
	{
		return (size_t)x * (size_t)n + (size_t)z;
	}

	BotCore::NavCell Cell(int x, int z)
	{
		BotCore::NavCell c;
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

	// RingEvents plus a blocked column at x (z = 1..n-2).
	std::vector<int16_t> WallColumn(int n, int x)
	{
		std::vector<int16_t> events = RingEvents(n);
		for (int z = 1; z <= n - 2; ++z)
			events[CellIndex(n, x, z)] = 0;
		return events;
	}

	// RingEvents plus a column at x blocked except for the z gap [gapLo, gapHi].
	std::vector<int16_t> GapWall(int n, int x, int gapLo, int gapHi)
	{
		std::vector<int16_t> events = RingEvents(n);
		for (int z = 1; z <= n - 2; ++z)
		{
			if (z >= gapLo && z <= gapHi)
				continue;
			events[CellIndex(n, x, z)] = 0;
		}
		return events;
	}

	// Flat ground with a 20 m raised block on x in [10, 14], z in [8, 12] (n = 20).
	std::vector<float> BlockHeights(int n)
	{
		std::vector<float> heights((size_t)n * (size_t)n, 0.0f);
		for (int x = 10; x <= 14; ++x)
		{
			for (int z = 8; z <= 12; ++z)
				heights[CellIndex(n, x, z)] = 20.0f;
		}
		return heights;
	}

	BotCore::NavGrid MakeNav(int n, float unit, const std::vector<int16_t> & events, const std::vector<float> & heights)
	{
		BotCore::NavGrid grid;
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

	BotCore::NavReachJudgement UnreachableJudge(BotCore::NavUnreachReason reason)
	{
		BotCore::NavReachJudgement j;
		j.verdict = BotCore::NavReachVerdict::Unreachable;
		j.reason = reason;
		return j;
	}

	BotCore::NavReachJudgement ReachableJudge()
	{
		BotCore::NavReachJudgement j;
		j.verdict = BotCore::NavReachVerdict::Reachable;
		return j;
	}

	BotCore::NavReachJudgement UnknownJudge()
	{
		BotCore::NavReachJudgement j;
		j.verdict = BotCore::NavReachVerdict::Unknown;
		return j;
	}
}

TEST_CASE("NavReach_Components_Small")
{
	// (a) Before Build the table is empty.
	{
		BotCore::NavReach r;
		CHECK_EQ(r.Size(), 0);
		CHECK_EQ(r.ComponentCount(), 0);
		CHECK_EQ(r.ComponentOf(5, 5), -1);
		CHECK_EQ(r.LargestComponent(), -1);
		CHECK_EQ(r.ComponentCells(0), 0);
		CHECK(!r.Connected(Cell(5, 5), Cell(5, 5)));
	}

	const float unit = 4.0f;

	// (b) Flat 40x40: one component, the whole interior.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavReach r;
		r.Build(grid);
		CHECK_EQ(r.Size(), 40);
		CHECK_EQ(r.ComponentCount(), 1);
		CHECK_EQ(r.ComponentCells(0), 1444);
		CHECK_EQ(r.LargestComponent(), 0);
		CHECK_EQ(r.ComponentOf(5, 5), 0);
		CHECK_EQ(r.ComponentOf(0, 0), -1);
		CHECK_EQ(r.ComponentOf(-1, 5), -1);
		CHECK_EQ(r.ComponentOf(40, 5), -1);
		CHECK_EQ(r.ComponentOf(5, 40), -1);
		CHECK(r.Connected(Cell(1, 1), Cell(38, 38)));
		CHECK(r.Connected(Cell(5, 5), Cell(5, 5)));
		CHECK(!r.Connected(Cell(0, 0), Cell(0, 0)));
	}

	// (c) Raised block: outer 299 + block 25.
	{
		BotCore::NavGrid grid = MakeNav(20, unit, RingEvents(20), BlockHeights(20));
		BotCore::NavReach r;
		r.Build(grid);
		CHECK_EQ(r.ComponentCount(), 2);
		CHECK_EQ(r.ComponentOf(5, 5), 0);
		CHECK_EQ(r.ComponentOf(12, 10), 1);
		CHECK_EQ(r.ComponentCells(0), 299);
		CHECK_EQ(r.ComponentCells(1), 25);
		CHECK_EQ(r.ComponentCells(2), 0);
		CHECK_EQ(r.ComponentCells(-1), 0);
		CHECK_EQ(r.LargestComponent(), 0);
		CHECK(!r.Connected(Cell(5, 5), Cell(12, 10)));
		CHECK(!r.Connected(Cell(12, 10), Cell(5, 5)));
		CHECK(r.Connected(Cell(12, 10), Cell(10, 8)));
		CHECK(r.Connected(Cell(5, 5), Cell(18, 18)));
	}

	// (d) The table follows the grid params (slope gate).
	{
		BotCore::NavGrid grid = MakeNav(20, unit, RingEvents(20), BlockHeights(20));
		BotCore::NavReach r;
		r.Build(grid);
		BotCore::NavParams p;
		p.maxSlope = 100.0f;
		grid.Build(p);
		r.Build(grid);
		CHECK_EQ(r.ComponentCount(), 1);
		CHECK_EQ(r.ComponentCells(0), 324);
		grid.Build();
		r.Build(grid);
		CHECK_EQ(r.ComponentCount(), 2);
	}

	// (e) Rebuild with another grid replaces the old table.
	{
		BotCore::NavGrid block = MakeNav(20, unit, RingEvents(20), BlockHeights(20));
		BotCore::NavReach r;
		r.Build(block);
		BotCore::NavGrid flat = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		r.Build(flat);
		CHECK_EQ(r.Size(), 40);
		CHECK_EQ(r.ComponentCount(), 1);
	}

	// (f) Wall column 30x30: only the larger half is Walk, one component.
	{
		BotCore::NavGrid grid = MakeNav(30, unit, WallColumn(30, 15), HeightZeros(30));
		BotCore::NavReach r;
		r.Build(grid);
		CHECK_EQ(r.ComponentCount(), 1);
		CHECK_EQ(r.ComponentCells(0), 392);
		CHECK_EQ(r.ComponentOf(22, 10), -1);
	}
}

TEST_CASE("NavReach_Matches_AStar")
{
	const int n = 24;
	const float unit = 4.0f;
	const int seeds = 30;
	int multi = 0;
	int apart = 0;
	int same = 0;

	for (int seed = 0; seed < seeds; ++seed)
	{
		BotCore::Rng rng(1000u + (uint32_t)seed);
		std::vector<int16_t> events = RingEvents(n);
		std::vector<float> heights((size_t)n * (size_t)n, 0.0f);
		// Two draws per interior cell in fixed order (event, then height), x ascending then z.
		for (int x = 1; x <= n - 2; ++x)
		{
			for (int z = 1; z <= n - 2; ++z)
			{
				const uint32_t ev = rng.NextBelow(100);
				const uint32_t hh = rng.NextBelow(5);
				if (ev < 8)
					events[CellIndex(n, x, z)] = 0;
				if (hh == 0)
					heights[CellIndex(n, x, z)] = 6.0f;
			}
		}

		BotCore::NavGrid grid = MakeNav(n, unit, events, heights);
		BotCore::NavReach reach;
		reach.Build(grid);

		std::vector<BotCore::NavCell> cells;
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (grid.Walk(x, z))
					cells.push_back(Cell(x, z));
			}
		}

		if (cells.size() < 2)
			continue;

		if (reach.ComponentCount() >= 2)
			++multi;

		// Partition: every Walk cell has a valid id, every non-Walk cell is -1.
		int sum = 0;
		for (int id = 0; id < reach.ComponentCount(); ++id)
			sum += reach.ComponentCells(id);
		CHECK_EQ(sum, (int)cells.size());
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				const int c = reach.ComponentOf(x, z);
				if (grid.Walk(x, z))
					CHECK(c >= 0 && c < reach.ComponentCount());
				else
					CHECK_EQ(c, -1);
			}
		}

		BotCore::NavPathfinder pf;
		BotCore::NavSearchParams sp;
		sp.maxNodes = 100000;
		for (int q = 0; q < 150; ++q)
		{
			const int ia = (int)rng.NextBelow((uint32_t)cells.size());
			const int ib = (int)rng.NextBelow((uint32_t)cells.size());
			BotCore::NavPathResult out;
			pf.Find(grid, cells[(size_t)ia], cells[(size_t)ib], sp, out);
			CHECK(out.status == BotCore::NavPathStatus::Found || out.status == BotCore::NavPathStatus::NoPath);
			const bool found = out.status == BotCore::NavPathStatus::Found;
			const bool connected = reach.Connected(cells[(size_t)ia], cells[(size_t)ib]);
			CHECK(found == connected);
			if (connected)
				++same;
			else
				++apart;
		}
	}

	std::printf("NAVREACH random maps: seeds=%d multi=%d pairs=4500 apart=%d same=%d\n",
		seeds, multi, apart, same);
	REQUIRE(multi >= 15);
	REQUIRE(apart >= 300);
	REQUIRE(same >= 300);
}

TEST_CASE("NavReach_Judge_Basic")
{
	const float unit = 4.0f;

	// (a) No plan yet.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavReach reach;
		reach.Build(grid);
		BotCore::NavFollower f;
		BotCore::NavFollowParams fp;
		BotCore::NavUnreachParams up;
		BotCore::NavReachJudge judge;
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, 22.0f, 22.0f);
		CHECK(j.verdict == BotCore::NavReachVerdict::None);
		CHECK(j.reason == BotCore::NavUnreachReason::None);
		CHECK_EQ(j.ringCells, 0);
		CHECK_EQ(j.ringConnected, 0);
	}

	// (b) Reachable: straight plan, no detour.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavReach reach;
		reach.Build(grid);
		BotCore::NavPathfinder pf;
		BotCore::NavFollower f;
		BotCore::NavFollowParams fp;
		BotCore::NavUnreachParams up;
		BotCore::NavReachJudge judge;
		f.ObserveTarget(0, 82.0f, 22.0f);
		CHECK(f.Update(grid, pf, 0, 22.0f, 22.0f, 0.0f, fp));
		CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
		CHECK(std::fabs(f.Plan().pathCost - 60.0f) <= 1e-3f);
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, 22.0f, 22.0f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Reachable);
		CHECK(j.reason == BotCore::NavUnreachReason::None);
		CHECK_EQ(j.ringCells, 1);
		CHECK_EQ(j.ringConnected, 1);
		CHECK(std::fabs(j.straightM - 60.0f) <= 1e-3f);
		CHECK(std::fabs(j.pathM - 60.0f) <= 1e-3f);
	}

	// (c) NoWalkable: the ring holds no Walk cell (target on the far side of a wall).
	{
		BotCore::NavGrid grid = MakeNav(30, unit, WallColumn(30, 15), HeightZeros(30));
		BotCore::NavReach reach;
		reach.Build(grid);
		BotCore::NavPathfinder pf;
		BotCore::NavFollower f;
		BotCore::NavFollowParams fp;
		BotCore::NavUnreachParams up;
		BotCore::NavReachJudge judge;
		f.ObserveTarget(0, 90.0f, 42.0f);
		CHECK(f.Update(grid, pf, 0, 22.0f, 22.0f, 0.0f, fp));
		REQUIRE(f.Plan().status == BotCore::NavFollowStatus::NoGoal);
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, 22.0f, 22.0f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::NoWalkable);
		CHECK_EQ(j.ringCells, 0);
	}

	// (d) Component: the ring is inside another component.
	{
		BotCore::NavGrid grid = MakeNav(20, unit, RingEvents(20), BlockHeights(20));
		BotCore::NavReach reach;
		reach.Build(grid);
		BotCore::NavPathfinder pf;
		BotCore::NavFollowParams fp;
		fp.ringMinM = 4.0f;
		fp.ringMaxM = 9.0f;
		BotCore::NavUnreachParams up;
		BotCore::NavReachJudge judge;

		BotCore::NavFollower f;
		f.ObserveTarget(0, 50.0f, 42.0f);
		CHECK(f.Update(grid, pf, 0, 22.0f, 22.0f, 0.0f, fp));
		REQUIRE(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
		CHECK(f.Plan().pathStatus == BotCore::NavPathStatus::NoPath);
		BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, 22.0f, 22.0f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::Component);
		CHECK_EQ(j.ringCells, 20);
		CHECK_EQ(j.ringConnected, 0);

		BotCore::NavFollower g;
		BotCore::NavFollowParams f0;
		g.ObserveTarget(0, 50.0f, 42.0f);
		CHECK(g.Update(grid, pf, 0, 22.0f, 22.0f, 0.0f, f0));
		REQUIRE(g.Plan().status == BotCore::NavFollowStatus::PathFailed);
		j = judge.Judge(grid, reach, g.Plan(), f0, up, 22.0f, 22.0f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::Component);
		CHECK_EQ(j.ringCells, 1);
		CHECK_EQ(j.ringConnected, 0);
	}

	// (e) Bot inside the block: its own ring is in the other component.
	{
		BotCore::NavGrid grid = MakeNav(20, unit, RingEvents(20), BlockHeights(20));
		BotCore::NavReach reach;
		reach.Build(grid);
		BotCore::NavPathfinder pf;
		BotCore::NavFollower f;
		BotCore::NavFollowParams fp;
		BotCore::NavUnreachParams up;
		BotCore::NavReachJudge judge;
		f.ObserveTarget(0, 22.0f, 22.0f);
		CHECK(f.Update(grid, pf, 0, 50.0f, 42.0f, 0.0f, fp));
		REQUIRE(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
		CHECK(f.Plan().pathStatus == BotCore::NavPathStatus::NoPath);
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, 50.0f, 42.0f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::Component);
		CHECK_EQ(j.ringCells, 1);
		CHECK_EQ(j.ringConnected, 0);
	}

	// (f) Mixed ring: enough connected candidates, so Planned -> Reachable.
	{
		BotCore::NavGrid grid = MakeNav(20, unit, RingEvents(20), BlockHeights(20));
		BotCore::NavReach reach;
		reach.Build(grid);
		BotCore::NavPathfinder pf;
		BotCore::NavFollower f;
		BotCore::NavFollowParams fp;
		fp.ringMaxM = 13.0f;
		BotCore::NavUnreachParams up;
		BotCore::NavReachJudge judge;
		f.ObserveTarget(0, 50.0f, 42.0f);
		CHECK(f.Update(grid, pf, 0, 22.0f, 22.0f, 0.0f, fp));
		CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
		const BotCore::NavCell goal = f.Plan().goal;
		CHECK(!(10 <= goal.x && goal.x <= 14 && 8 <= goal.z && goal.z <= 12));
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, 22.0f, 22.0f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Reachable);
		CHECK_EQ(j.ringCells, 37);
		CHECK_EQ(j.ringConnected, 12);
	}

	// (g) Unknown: the A* run hit the node limit while a ring cell is connected.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavReach reach;
		reach.Build(grid);
		BotCore::NavPathfinder pf;
		BotCore::NavFollower f;
		BotCore::NavFollowParams fp;
		fp.search.maxNodes = 5;
		BotCore::NavUnreachParams up;
		BotCore::NavReachJudge judge;
		f.ObserveTarget(0, 82.0f, 22.0f);
		CHECK(f.Update(grid, pf, 0, 22.0f, 22.0f, 0.0f, fp));
		REQUIRE(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
		CHECK(f.Plan().pathStatus == BotCore::NavPathStatus::NodeLimit);
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, 22.0f, 22.0f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unknown);
		CHECK(j.reason == BotCore::NavUnreachReason::None);
		CHECK_EQ(j.ringCells, 1);
		CHECK_EQ(j.ringConnected, 1);
	}

	// (h) Unknown: the bot's own cell is not Walk (blocked or off-grid).
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavReach reach;
		reach.Build(grid);
		BotCore::NavPathfinder pf;
		BotCore::NavFollowParams fp;
		BotCore::NavUnreachParams up;
		BotCore::NavReachJudge judge;

		BotCore::NavFollower f;
		f.ObserveTarget(0, 82.0f, 22.0f);
		CHECK(f.Update(grid, pf, 0, 2.0f, 2.0f, 0.0f, fp));
		REQUIRE(f.Plan().status == BotCore::NavFollowStatus::InvalidStart);
		BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, 2.0f, 2.0f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unknown);
		CHECK_EQ(j.ringCells, 0);

		BotCore::NavFollower g;
		g.ObserveTarget(0, 82.0f, 22.0f);
		CHECK(g.Update(grid, pf, 0, -20.0f, 22.0f, 0.0f, fp));
		REQUIRE(g.Plan().status == BotCore::NavFollowStatus::InvalidStart);
		j = judge.Judge(grid, reach, g.Plan(), fp, up, -20.0f, 22.0f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unknown);
		CHECK_EQ(j.ringCells, 0);
	}

	// (i) Parameter defaults.
	{
		BotCore::NavUnreachParams up;
		CHECK_EQ(up.detourFactor, 3.0f);
		CHECK_EQ(up.detourMinM, 120.0f);
		CHECK_EQ(up.holdMs, 1500);
	}
}

TEST_CASE("NavReach_Judge_Detour")
{
	const int n = 60;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, GapWall(n, 30, 57, 58), HeightZeros(n));
	REQUIRE(grid.Walk(30, 57));
	REQUIRE(grid.Walk(28, 5));
	REQUIRE(grid.Walk(32, 5));

	BotCore::NavReach reach;
	reach.Build(grid);
	BotCore::NavPathfinder pf;
	BotCore::NavReachJudge judge;

	auto run = [&](BotCore::NavCell bot, BotCore::NavCell tgt, const BotCore::NavUnreachParams & up,
		const BotCore::NavFollowParams & fp, float & cost, BotCore::NavReachJudgement & j)
	{
		const float bx = grid.CellCenter(bot.x);
		const float bz = grid.CellCenter(bot.z);
		BotCore::NavFollower f;
		f.ObserveTarget(0, grid.CellCenter(tgt.x), grid.CellCenter(tgt.z));
		CHECK(f.Update(grid, pf, 0, bx, bz, 0.0f, fp));
		cost = f.Plan().pathCost;
		j = judge.Judge(grid, reach, f.Plan(), fp, up, bx, bz);
	};

	// (a) A long detour around the gap: 427 m for a straight 16 m.
	{
		BotCore::NavFollowParams fp;
		BotCore::NavUnreachParams up;
		float cost = 0.0f;
		BotCore::NavReachJudgement j;
		run(Cell(28, 5), Cell(32, 5), up, fp, cost, j);
		CHECK(std::fabs(cost - 427.314f) <= 0.05f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::Detour);
		CHECK(std::fabs(j.straightM - 16.0f) <= 1e-3f);
		CHECK(std::fabs(j.pathM - cost) <= 1e-3f);
		CHECK_EQ(j.ringCells, 1);
		CHECK_EQ(j.ringConnected, 1);
	}

	// (b) Middle detour.
	{
		BotCore::NavFollowParams fp;
		BotCore::NavUnreachParams up;
		float cost = 0.0f;
		BotCore::NavReachJudgement j;
		run(Cell(28, 40), Cell(32, 40), up, fp, cost, j);
		CHECK(std::fabs(cost - 147.314f) <= 0.05f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::Detour);
	}

	// (c) Ratio above 3 but below 120 m: still Reachable.
	{
		BotCore::NavFollowParams fp;
		BotCore::NavUnreachParams up;
		float cost = 0.0f;
		BotCore::NavReachJudgement j;
		run(Cell(28, 50), Cell(32, 50), up, fp, cost, j);
		CHECK(std::fabs(cost - 67.314f) <= 0.05f);
		CHECK(j.verdict == BotCore::NavReachVerdict::Reachable);
	}

	// (d) Threshold edges.
	{
		BotCore::NavFollowParams fp;
		float cost = 0.0f;
		BotCore::NavReachJudgement j;

		BotCore::NavUnreachParams u60;
		u60.detourMinM = 60.0f;
		run(Cell(28, 50), Cell(32, 50), u60, fp, cost, j);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::Detour);

		BotCore::NavUnreachParams u30;
		u30.detourFactor = 30.0f;
		run(Cell(28, 5), Cell(32, 5), u30, fp, cost, j);
		CHECK(j.verdict == BotCore::NavReachVerdict::Reachable);

		BotCore::NavUnreachParams u0;
		u0.detourFactor = 0.0f;
		run(Cell(28, 5), Cell(32, 5), u0, fp, cost, j);
		CHECK(j.verdict == BotCore::NavReachVerdict::Reachable);

		BotCore::NavUnreachParams uneg;
		uneg.detourFactor = -1.0f;
		run(Cell(28, 5), Cell(32, 5), uneg, fp, cost, j);
		CHECK(j.verdict == BotCore::NavReachVerdict::Reachable);

		BotCore::NavUnreachParams u500;
		u500.detourMinM = 500.0f;
		run(Cell(28, 5), Cell(32, 5), u500, fp, cost, j);
		CHECK(j.verdict == BotCore::NavReachVerdict::Reachable);
	}
}

TEST_CASE("NavReach_Tracker")
{
	BotCore::NavUnreachTracker t;
	CHECK(!t.Active());
	CHECK_EQ(t.SinceMs(), (int64_t)0);
	CHECK(t.Reason() == BotCore::NavUnreachReason::None);
	CHECK(!t.Due(100000, 0));

	t.Feed(1000, UnreachableJudge(BotCore::NavUnreachReason::Component));
	CHECK(t.Active());
	CHECK_EQ(t.SinceMs(), (int64_t)1000);
	CHECK(t.Reason() == BotCore::NavUnreachReason::Component);
	CHECK(!t.Due(2499, 1500));
	CHECK(t.Due(2500, 1500));
	CHECK(t.Due(1000, 0));
	CHECK(!t.Due(999, 1500));

	// A later Unreachable keeps the streak start but updates the reason.
	t.Feed(1200, UnreachableJudge(BotCore::NavUnreachReason::NoWalkable));
	CHECK_EQ(t.SinceMs(), (int64_t)1000);
	CHECK(t.Reason() == BotCore::NavUnreachReason::NoWalkable);

	// Any other verdict clears the streak.
	t.Feed(1300, UnknownJudge());
	CHECK(!t.Active());
	CHECK_EQ(t.SinceMs(), (int64_t)0);
	CHECK(t.Reason() == BotCore::NavUnreachReason::None);
	CHECK(!t.Due(100000, 0));

	t.Feed(1400, UnreachableJudge(BotCore::NavUnreachReason::Detour));
	CHECK_EQ(t.SinceMs(), (int64_t)1400);
	t.Feed(1500, ReachableJudge());
	CHECK(!t.Active());
	t.Feed(1600, BotCore::NavReachJudgement());
	CHECK(!t.Active());
	t.Feed(1700, UnreachableJudge(BotCore::NavUnreachReason::Component));
	CHECK(t.Active());
	t.Reset();
	CHECK(!t.Active());
	CHECK_EQ(t.SinceMs(), (int64_t)0);
	CHECK(t.Reason() == BotCore::NavUnreachReason::None);
}

TEST_CASE("NavReach_Drop_Sim")
{
	const int n = 20;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), BlockHeights(n));
	BotCore::NavReach reach;
	reach.Build(grid);
	BotCore::NavReachJudge judge;
	BotCore::NavUnreachParams up;

	// Scenario A: the target stays unreachable from the first tick.
	{
		BotCore::NavFollower f;
		BotCore::NavPathfinder pf;
		BotCore::NavFollowParams fp;
		fp.ringMinM = 4.0f;
		fp.ringMaxM = 9.0f;
		BotCore::NavUnreachTracker tracker;
		f.ObserveTarget(0, 50.0f, 42.0f);

		long long dropMs = -1;
		long long detectMs = -1;
		int feeds = 0;
		for (int64_t t = 0; t <= 4000; t += 100)
		{
			if (f.Update(grid, pf, t, 22.0f, 22.0f, 0.0f, fp))
			{
				const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, 22.0f, 22.0f);
				tracker.Feed(t, j);
				++feeds;
			}
			if (tracker.Due(t, up.holdMs))
			{
				dropMs = t;
				detectMs = tracker.SinceMs();
				break;
			}
		}
		std::printf("NAVREACH drop A: detect_ms=%lld drop_ms=%lld feeds=%d\n", detectMs, dropMs, feeds);
		CHECK_EQ(detectMs, (long long)0);
		CHECK_EQ(dropMs, (long long)1500);
		CHECK_EQ(feeds, 4);
	}

	// Scenario B: a walkable detour in between resets the streak.
	{
		BotCore::NavFollower f;
		BotCore::NavPathfinder pf;
		BotCore::NavFollowParams fp;
		fp.ringMinM = 4.0f;
		fp.ringMaxM = 9.0f;
		BotCore::NavUnreachTracker tracker;

		long long dropMs = -1;
		long long detectMs = -1;
		int feeds = 0;
		for (int64_t t = 0; t <= 4000; t += 100)
		{
			if (t == 0)
				f.ObserveTarget(t, 50.0f, 42.0f);
			else if (t == 700)
				f.ObserveTarget(t, 30.0f, 22.0f);
			else if (t == 1500)
				f.ObserveTarget(t, 50.0f, 42.0f);

			if (f.Update(grid, pf, t, 22.0f, 22.0f, 0.0f, fp))
			{
				const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, 22.0f, 22.0f);
				tracker.Feed(t, j);
				++feeds;
			}
			if (tracker.Due(t, up.holdMs))
			{
				dropMs = t;
				detectMs = tracker.SinceMs();
				break;
			}
		}
		std::printf("NAVREACH drop B: detect_ms=%lld drop_ms=%lld feeds=%d\n", detectMs, dropMs, feeds);
		CHECK_EQ(detectMs, (long long)1500);
		CHECK_EQ(dropMs, (long long)3000);
		CHECK_EQ(feeds, 8);
	}
}

TEST_CASE("NavReach_RealMap")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVREACH real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	BotCore::NavReach reach;
	const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	reach.Build(grid);
	const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	const double buildMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

	// ADR-0024 (maxSlope 0.45): the steeper edge rule splits the previous main component into
	// many more small pockets (229 -> 995 cells); re-measured on the real grid, 2026-10-03.
	CHECK_EQ(reach.ComponentCount(), 401);
	CHECK_EQ(reach.ComponentCells(reach.LargestComponent()), 87513);

	int total = 0;
	int pockets = 0;
	std::vector<int> sizes;
	for (int id = 0; id < reach.ComponentCount(); ++id)
	{
		const int c = reach.ComponentCells(id);
		total += c;
		sizes.push_back(c);
	}
	CHECK_EQ(total, 88508);
	CHECK_EQ(total - reach.ComponentCells(reach.LargestComponent()), 995);
	pockets = 995;

	std::sort(sizes.begin(), sizes.end(), std::greater<int>());
	const int expected[12] = { 87513, 93, 79, 31, 23, 15, 15, 13, 13, 13, 12, 11 };
	REQUIRE((int)sizes.size() >= 12);
	for (int k = 0; k < 12; ++k)
		CHECK_EQ(sizes[(size_t)k], expected[k]);

	const BotCore::NavCell a = Cell(318, 222);
	const BotCore::NavCell b = Cell(186, 276);
	const BotCore::NavCell p = Cell(173, 211);
	// Detour pair re-measured for ADR-0024 (the old 218,206 -> 221,211 pair is no longer in the
	// same component). s and g are both in the main component and connected, but the direct route
	// is blocked: the A* path (~276 m) is far longer than the 40 m straight line.
	const BotCore::NavCell s = Cell(211, 212);
	const BotCore::NavCell g = Cell(221, 212);
	REQUIRE(grid.Walk(a.x, a.z));
	REQUIRE(grid.Walk(b.x, b.z));
	REQUIRE(grid.Walk(p.x, p.z));
	REQUIRE(grid.Walk(s.x, s.z));
	REQUIRE(grid.Walk(g.x, g.z));
	CHECK_EQ(reach.ComponentOf(a.x, a.z), reach.LargestComponent());
	CHECK_EQ(reach.ComponentOf(a.x, a.z), 0);
	CHECK(reach.Connected(a, b));
	CHECK(reach.Connected(s, g));
	CHECK(!reach.Connected(a, p));
	CHECK_EQ(reach.ComponentCells(reach.ComponentOf(p.x, p.z)), 5);
	CHECK_EQ(reach.ComponentOf(p.x, p.z), 137);

	const float ax = grid.CellCenter(a.x);
	const float az = grid.CellCenter(a.z);
	const float bx = grid.CellCenter(b.x);
	const float bz = grid.CellCenter(b.z);
	const float px = grid.CellCenter(p.x);
	const float pz = grid.CellCenter(p.z);
	const float sx = grid.CellCenter(s.x);
	const float sz = grid.CellCenter(s.z);
	const float gx = grid.CellCenter(g.x);
	const float gz = grid.CellCenter(g.z);

	BotCore::NavPathfinder pf;
	BotCore::NavReachJudge judge;
	BotCore::NavUnreachParams up;
	BotCore::NavFollowParams fp;

	int mageRing = 0;
	int mageConnected = 0;
	float detourPath = 0.0f;
	float detourStraight = 0.0f;

	// Pocket target, ring [0, 0]: A* hits the node limit, the component is decisive.
	{
		BotCore::NavFollower f;
		f.ObserveTarget(0, px, pz);
		CHECK(f.Update(grid, pf, 0, ax, az, 0.0f, fp));
		REQUIRE(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
		CHECK(f.Plan().pathStatus == BotCore::NavPathStatus::NodeLimit);
		CHECK_EQ(f.Plan().tries, 1);
		CHECK_EQ(f.Plan().expanded, 20000);
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, ax, az);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::Component);
		CHECK_EQ(j.ringCells, 1);
		CHECK_EQ(j.ringConnected, 0);
	}

	// Bot in the pocket: A* exhausts the 5-cell pocket.
	{
		BotCore::NavFollower f;
		f.ObserveTarget(0, ax, az);
		CHECK(f.Update(grid, pf, 0, px, pz, 0.0f, fp));
		REQUIRE(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
		CHECK(f.Plan().pathStatus == BotCore::NavPathStatus::NoPath);
		CHECK_EQ(f.Plan().expanded, 5);
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, px, pz);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::Component);
		CHECK_EQ(j.ringCells, 1);
		CHECK_EQ(j.ringConnected, 0);
	}

	// Mage ring [30, 45]: the target is reachable at range.
	{
		BotCore::NavFollower f;
		BotCore::NavFollowParams pm;
		pm.ringMinM = 30.0f;
		pm.ringMaxM = 45.0f;
		f.ObserveTarget(0, px, pz);
		CHECK(f.Update(grid, pf, 0, ax, az, 0.0f, pm));
		CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), pm, up, ax, az);
		CHECK(j.verdict == BotCore::NavReachVerdict::Reachable);
		CHECK_EQ(j.ringCells, 168);
		CHECK_EQ(j.ringConnected, 146);
		mageRing = j.ringCells;
		mageConnected = j.ringConnected;
	}

	// Detour pair: both cells are in the main component but the path is long.
	{
		BotCore::NavFollower f;
		f.ObserveTarget(0, gx, gz);
		CHECK(f.Update(grid, pf, 0, sx, sz, 0.0f, fp));
		REQUIRE(f.Plan().status == BotCore::NavFollowStatus::Planned);
		CHECK(std::fabs(f.Plan().pathCost - 276.451f) <= 0.1f);
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, sx, sz);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::Detour);
		CHECK(std::fabs(j.straightM - 40.000f) <= 0.01f);
		CHECK(j.pathM > 3.0f * j.straightM);
		CHECK(j.pathM > 120.0f);
		detourPath = j.pathM;
		detourStraight = j.straightM;
	}

	// Control A -> B: planned and no detour.
	{
		BotCore::NavFollower f;
		f.ObserveTarget(0, bx, bz);
		CHECK(f.Update(grid, pf, 0, ax, az, 0.0f, fp));
		CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), fp, up, ax, az);
		CHECK(j.verdict == BotCore::NavReachVerdict::Reachable);
		CHECK(j.pathM / j.straightM < 1.5f);
	}

	std::printf("NAVREACH real: components=%d largest=%d pockets=%d build_ms=%.2f; pocket NodeLimit->Component, in-pocket NoPath->Component; mage ring=%d connected=%d; detour path=%.3f straight=%.2f\n",
		reach.ComponentCount(), reach.ComponentCells(reach.LargestComponent()), pockets, buildMs,
		mageRing, mageConnected, detourPath, detourStraight);

#ifndef _DEBUG
	CHECK(buildMs <= 100.0);
#endif
}

TEST_CASE("NavReach_Perf")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVREACH real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	BotCore::NavReach reach;
	reach.Build(grid);

	const int n = grid.Size();
	std::vector<BotCore::NavCell> cells;
	cells.reserve((size_t)grid.MainComponentCells());
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			if (grid.Walk(x, z))
				cells.push_back(Cell(x, z));
		}
	}
	REQUIRE((int)cells.size() == 88508);

#if defined(_DEBUG)
	const int queries = 100;
#else
	const int queries = 1000;
#endif

	BotCore::Rng rng(20261002u);
	std::vector<BotCore::NavCell> starts;
	std::vector<BotCore::NavCell> goals;
	while ((int)starts.size() < queries)
	{
		const int si = (int)rng.NextBelow((uint32_t)cells.size());
		const int gi = (int)rng.NextBelow((uint32_t)cells.size());
		if (si == gi)
			continue;
		const int dx = std::abs(cells[(size_t)si].x - cells[(size_t)gi].x);
		const int dz = std::abs(cells[(size_t)si].z - cells[(size_t)gi].z);
		if (std::max(dx, dz) > 64)
			continue;
		// ADR-0024 (maxSlope 0.45) leaves ~1% of the Walk cells in edge-isolated pockets; the
		// judge quality gate is about the main component, so keep the sample there.
		if (reach.ComponentOf(cells[(size_t)si].x, cells[(size_t)si].z) != reach.LargestComponent())
			continue;
		if (reach.ComponentOf(cells[(size_t)gi].x, cells[(size_t)gi].z) != reach.LargestComponent())
			continue;
		starts.push_back(cells[(size_t)si]);
		goals.push_back(cells[(size_t)gi]);
	}

	BotCore::NavPathfinder pf;
	BotCore::NavReachJudge judge;
	BotCore::NavUnreachParams up;

	auto runSet = [&](float ringMin, float ringMax, bool exact, const char * name)
	{
		BotCore::NavFollower follower;
		BotCore::NavFollowParams params;
		params.ringMinM = ringMin;
		params.ringMaxM = ringMax;

		for (int q = 0; q < 20; ++q)
		{
			const float sx = grid.CellCenter(starts[(size_t)q].x);
			const float sz = grid.CellCenter(starts[(size_t)q].z);
			follower.Reset();
			follower.ObserveTarget(0, grid.CellCenter(goals[(size_t)q].x), grid.CellCenter(goals[(size_t)q].z));
			follower.Update(grid, pf, 0, sx, sz, 0.0f, params);
			judge.Judge(grid, reach, follower.Plan(), params, up, sx, sz);
		}

		std::vector<double> ms;
		ms.reserve((size_t)queries);
		int judged = 0;
		int unreachable = 0;
		int detour = 0;
		bool invariants = true;

		for (int q = 0; q < queries; ++q)
		{
			const float sx = grid.CellCenter(starts[(size_t)q].x);
			const float sz = grid.CellCenter(starts[(size_t)q].z);
			follower.Reset();
			follower.ObserveTarget(0, grid.CellCenter(goals[(size_t)q].x), grid.CellCenter(goals[(size_t)q].z));
			follower.Update(grid, pf, 0, sx, sz, 0.0f, params);

			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			const BotCore::NavReachJudgement j = judge.Judge(grid, reach, follower.Plan(), params, up, sx, sz);
			const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
			ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
			++judged;

			if (j.verdict == BotCore::NavReachVerdict::Unreachable)
				++unreachable;
			if (j.reason == BotCore::NavUnreachReason::Detour)
				++detour;

			const BotCore::NavFollowPlan & plan = follower.Plan();
			if (plan.status == BotCore::NavFollowStatus::Planned)
			{
				const bool ok = j.verdict == BotCore::NavReachVerdict::Reachable
					|| (j.verdict == BotCore::NavReachVerdict::Unreachable
						&& j.reason == BotCore::NavUnreachReason::Detour);
				if (!ok || j.ringConnected < 1)
					invariants = false;
			}
			else if (plan.status == BotCore::NavFollowStatus::PathFailed)
			{
				const bool component = j.verdict == BotCore::NavReachVerdict::Unreachable
					&& j.reason == BotCore::NavUnreachReason::Component;
				const bool unknown = j.verdict == BotCore::NavReachVerdict::Unknown;
				if (j.ringConnected == 0 && !component)
					invariants = false;
				if (j.ringConnected >= 1 && !unknown)
					invariants = false;
				if (exact && plan.pathStatus == BotCore::NavPathStatus::NoPath && !component)
					invariants = false;
			}
		}

		std::sort(ms.begin(), ms.end());
		const double p50 = PercentileDouble(ms, 0.50);
		const double p95 = PercentileDouble(ms, 0.95);
		const double p99 = PercentileDouble(ms, 0.99);

		std::printf("NAVREACH perf set=%s judged=%d unreachable=%d detour=%d ms_p50=%.3f ms_p95=%.3f ms_p99=%.3f\n",
			name, judged, unreachable, detour, p50, p95, p99);

		CHECK_EQ(judged, queries);
		CHECK(unreachable * 100 <= queries * 5);
		CHECK(detour * 100 <= queries * 5);
		CHECK(invariants);
#ifndef _DEBUG
		CHECK(p95 <= 0.5);
#endif
	};

	runSet(0.0f, 0.0f, true, "exact");
	runSet(30.0f, 45.0f, false, "mage");
}
