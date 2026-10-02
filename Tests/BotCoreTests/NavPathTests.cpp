#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/Rng.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

namespace
{
	size_t CellIndex(int n, int x, int z)
	{
		return (size_t)x * (size_t)n + (size_t)z;
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

	BotCore::NavGrid MakeNav(int n, float unit, const std::vector<int16_t> & events, const std::vector<float> & heights)
	{
		BotCore::NavGrid grid;
		if (!grid.Init(n, unit, events, heights))
			CHECK(false);
		grid.Build();
		return grid;
	}

	bool SameResult(const BotCore::NavPathResult & a, const BotCore::NavPathResult & b)
	{
		return a.status == b.status && a.cells == b.cells && a.cost == b.cost && a.expanded == b.expanded;
	}

	// Independent validity check: consecutive cells are 8-neighbours joined by an open edge and
	// the step lengths sum to the reported cost.
	bool PathIsValid(const BotCore::NavGrid & grid, const BotCore::NavPathResult & r, BotCore::NavCell start, BotCore::NavCell goal)
	{
		if (r.status != BotCore::NavPathStatus::Found || r.cells.empty())
			return false;
		if (r.cells.front() != start || r.cells.back() != goal)
			return false;

		float sum = 0.0f;
		for (size_t i = 1; i < r.cells.size(); ++i)
		{
			const BotCore::NavCell & a = r.cells[i - 1];
			const BotCore::NavCell & b = r.cells[i];
			const int dx = b.x - a.x;
			const int dz = b.z - a.z;
			if (std::abs(dx) > 1 || std::abs(dz) > 1 || (dx == 0 && dz == 0))
				return false;
			if (!grid.EdgeOpen(a.x, a.z, dx, dz))
				return false;
			sum += (dx != 0 && dz != 0) ? (grid.Unit() * std::sqrt(2.0f)) : grid.Unit();
		}

		return std::fabs(sum - r.cost) <= (1e-3f * r.cost + 1e-3f);
	}

	// Test-local Dijkstra (does not use NavPath.h); negative means unreachable.
	float ReferenceCost(const BotCore::NavGrid & grid, BotCore::NavCell start, BotCore::NavCell goal)
	{
		const int n = grid.Size();
		if (!grid.Walk(start.x, start.z) || !grid.Walk(goal.x, goal.z))
			return -1.0f;

		const float inf = 1e30f;
		std::vector<float> dist((size_t)n * (size_t)n, inf);
		std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int> >, std::greater<std::pair<float, int> > > pq;

		const int s = start.x * n + start.z;
		const int g = goal.x * n + goal.z;
		dist[(size_t)s] = 0.0f;
		pq.push(std::make_pair(0.0f, s));

		const int dxs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		const int dzs[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

		while (!pq.empty())
		{
			const std::pair<float, int> top = pq.top();
			pq.pop();
			const int cur = top.second;
			if (top.first > dist[(size_t)cur])
				continue;

			const int cx = cur / n;
			const int cz = cur % n;
			for (int k = 0; k < 8; ++k)
			{
				const int dx = dxs[k];
				const int dz = dzs[k];
				if (!grid.EdgeOpen(cx, cz, dx, dz))
					continue;
				const int ni = (cx + dx) * n + (cz + dz);
				const float nd = dist[(size_t)cur] + ((dx != 0 && dz != 0) ? (grid.Unit() * std::sqrt(2.0f)) : grid.Unit());
				if (nd < dist[(size_t)ni])
				{
					dist[(size_t)ni] = nd;
					pq.push(std::make_pair(nd, ni));
				}
			}
		}

		if (dist[(size_t)g] >= inf * 0.5f)
			return -1.0f;
		return dist[(size_t)g];
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

	int PercentileInt(const std::vector<int> & sorted, double p)
	{
		if (sorted.empty())
			return 0;
		size_t idx = (size_t)(p * (double)(sorted.size() - 1));
		if (idx >= sorted.size())
			idx = sorted.size() - 1;
		return sorted[idx];
	}
}

TEST_CASE("NavPath_Status_Validation")
{
	const int n = 12;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));

	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams params;
	BotCore::NavPathResult r;

	BotCore::NavCell in;
	in.x = 5;
	in.z = 5;

	// Blocked border cell as start.
	BotCore::NavCell blocked;
	blocked.x = 0;
	blocked.z = 3;
	finder.Find(grid, blocked, in, params, r);
	CHECK(r.status == BotCore::NavPathStatus::InvalidStart);
	CHECK_EQ(r.expanded, 0);
	CHECK(r.cells.empty());

	// Out-of-grid start.
	BotCore::NavCell outside;
	outside.x = -1;
	outside.z = 3;
	finder.Find(grid, outside, in, params, r);
	CHECK(r.status == BotCore::NavPathStatus::InvalidStart);
	CHECK_EQ(r.expanded, 0);
	CHECK(r.cells.empty());

	// Valid start with a blocked goal.
	finder.Find(grid, in, blocked, params, r);
	CHECK(r.status == BotCore::NavPathStatus::InvalidGoal);
	CHECK_EQ(r.expanded, 0);
	CHECK(r.cells.empty());

	// Valid start with an out-of-grid goal.
	finder.Find(grid, in, outside, params, r);
	CHECK(r.status == BotCore::NavPathStatus::InvalidGoal);
	CHECK_EQ(r.expanded, 0);
	CHECK(r.cells.empty());

	// Both invalid: start is checked first.
	finder.Find(grid, blocked, outside, params, r);
	CHECK(r.status == BotCore::NavPathStatus::InvalidStart);
	CHECK_EQ(r.expanded, 0);
	CHECK(r.cells.empty());

	// Before NavGrid::Build every cell reads as not Walk.
	BotCore::NavGrid unbuilt;
	CHECK(unbuilt.Init(n, unit, RingEvents(n), HeightZeros(n)));
	BotCore::NavCell other;
	other.x = 6;
	other.z = 6;
	finder.Find(unbuilt, in, other, params, r);
	CHECK(r.status == BotCore::NavPathStatus::InvalidStart);
	CHECK_EQ(r.expanded, 0);
	CHECK(r.cells.empty());

	// start == goal is Found without a search.
	finder.Find(grid, in, in, params, r);
	CHECK(r.status == BotCore::NavPathStatus::Found);
	CHECK_EQ((int)r.cells.size(), 1);
	CHECK_EQ(r.cost, 0.0f);
	CHECK_EQ(r.expanded, 0);
}

TEST_CASE("NavPath_OpenField_Octile")
{
	const int n = 40;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));

	BotCore::NavCell a;
	a.x = 5;
	a.z = 5;
	BotCore::NavCell b;
	b.x = 25;
	b.z = 17;

	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams params;
	BotCore::NavPathResult r;
	finder.Find(grid, a, b, params, r);

	CHECK(r.status == BotCore::NavPathStatus::Found);
	const float expected = BotCore::NavOctile(20, 12, unit);
	CHECK(std::fabs(r.cost - expected) < 1e-3f);
	CHECK_EQ((int)r.cells.size(), 21);
	CHECK(PathIsValid(grid, r, a, b));

	const float oct = BotCore::NavOctile(3, 4, 4.0f);
	const float octExpected = 4.0f * (3.0f + 4.0f - 2.0f * 3.0f + std::sqrt(2.0f) * 3.0f);
	CHECK(std::fabs(oct - octExpected) < 1e-4f);
}

TEST_CASE("NavPath_NoCornerCutting")
{
	const int n = 12;
	const float unit = 4.0f;

	std::vector<int16_t> events = RingEvents(n);
	for (int z = 1; z <= 10; ++z)
	{
		if (z != 5)
			events[CellIndex(n, 6, z)] = 0;
	}

	BotCore::NavGrid grid = MakeNav(n, unit, events, HeightZeros(n));
	CHECK(grid.Walk(6, 5));

	BotCore::NavCell a;
	a.x = 5;
	a.z = 4;
	BotCore::NavCell b;
	b.x = 7;
	b.z = 6;

	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams params;
	BotCore::NavPathResult r;
	finder.Find(grid, a, b, params, r);

	CHECK(r.status == BotCore::NavPathStatus::Found);
	CHECK(std::fabs(r.cost - 16.0f) < 1e-3f);
	CHECK_EQ((int)r.cells.size(), 5);
	CHECK(PathIsValid(grid, r, a, b));

	bool throughGap = false;
	for (size_t i = 0; i < r.cells.size(); ++i)
	{
		if (r.cells[i].x == 6 && r.cells[i].z == 5)
			throughGap = true;
	}
	CHECK(throughGap);

	finder.Find(grid, b, a, params, r);
	CHECK(r.status == BotCore::NavPathStatus::Found);
	CHECK(std::fabs(r.cost - 16.0f) < 1e-3f);
}

TEST_CASE("NavPath_NoPath_SlopeCut")
{
	const int n = 20;
	const float unit = 4.0f;

	std::vector<float> heights((size_t)n * (size_t)n, 0.0f);
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
			heights[CellIndex(n, x, z)] = (x >= 10) ? 10.0f : 0.0f;
	}

	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), heights);

	BotCore::NavCell a;
	a.x = 2;
	a.z = 2;
	BotCore::NavCell b;
	b.x = 15;
	b.z = 10;
	BotCore::NavCell sameHalf;
	sameHalf.x = 8;
	sameHalf.z = 15;

	// Both halves belong to the same 4-connected event component (Walk), but the slope closes
	// every edge across x = 9/10.
	CHECK(grid.Walk(a.x, a.z));
	CHECK(grid.Walk(b.x, b.z));
	CHECK(grid.Walk(sameHalf.x, sameHalf.z));

	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams params;
	BotCore::NavPathResult r;

	finder.Find(grid, a, b, params, r);
	CHECK(r.status == BotCore::NavPathStatus::NoPath);
	CHECK_EQ(r.expanded, 162);
	CHECK(r.cells.empty());
	CHECK_EQ(r.cost, 0.0f);

	finder.Find(grid, a, sameHalf, params, r);
	CHECK(r.status == BotCore::NavPathStatus::Found);
	CHECK(PathIsValid(grid, r, a, sameHalf));
}

TEST_CASE("NavPath_NodeLimit_Semantics")
{
	const int n = 64;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));

	BotCore::NavCell a;
	a.x = 2;
	a.z = 2;
	BotCore::NavCell b;
	b.x = 60;
	b.z = 60;

	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams params;
	BotCore::NavPathResult r;

	params.maxNodes = 1000000;
	finder.Find(grid, a, b, params, r);
	CHECK(r.status == BotCore::NavPathStatus::Found);
	const int e = r.expanded;
	CHECK(e > 0);
	const BotCore::NavPathResult full = r;

	params.maxNodes = e;
	finder.Find(grid, a, b, params, r);
	CHECK(r.status == BotCore::NavPathStatus::Found);
	CHECK_EQ(r.cost, full.cost);
	CHECK(r.cells == full.cells);
	CHECK_EQ(r.expanded, e);

	params.maxNodes = e - 1;
	finder.Find(grid, a, b, params, r);
	CHECK(r.status == BotCore::NavPathStatus::NodeLimit);
	CHECK_EQ(r.expanded, e - 1);
	CHECK(r.cells.empty());
	CHECK_EQ(r.cost, 0.0f);

	params.maxNodes = 10;
	finder.Find(grid, a, b, params, r);
	CHECK(r.status == BotCore::NavPathStatus::NodeLimit);
	CHECK_EQ(r.expanded, 10);

	params.maxNodes = 0;
	finder.Find(grid, a, b, params, r);
	CHECK(r.status == BotCore::NavPathStatus::NodeLimit);
	CHECK_EQ(r.expanded, 0);

	params.maxNodes = -5;
	finder.Find(grid, a, b, params, r);
	CHECK(r.status == BotCore::NavPathStatus::NodeLimit);
	CHECK_EQ(r.expanded, 0);

	params.maxNodes = 0;
	finder.Find(grid, a, a, params, r);
	CHECK(r.status == BotCore::NavPathStatus::Found);
	CHECK_EQ((int)r.cells.size(), 1);
}

TEST_CASE("NavPath_MatchesDijkstra")
{
	int totalFound = 0;
	int totalPairs = 0;

	for (int k = 0; k < 6; ++k)
	{
		const int n = 28;
		const float unit = 4.0f;
		BotCore::Rng gridRng((uint64_t)(1000 + k));

		std::vector<int16_t> events = RingEvents(n);
		std::vector<float> heights((size_t)n * (size_t)n, 0.0f);
		for (int x = 1; x < n - 1; ++x)
		{
			for (int z = 1; z < n - 1; ++z)
			{
				const size_t i = CellIndex(n, x, z);
				if (gridRng.NextBelow(100) < 18)
					events[i] = 0;
				heights[i] = (float)gridRng.NextDouble() * 3.0f;
			}
		}

		BotCore::NavGrid grid = MakeNav(n, unit, events, heights);

		std::vector<BotCore::NavCell> walkable;
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (!grid.Walk(x, z))
					continue;
				BotCore::NavCell c;
				c.x = x;
				c.z = z;
				walkable.push_back(c);
			}
		}
		REQUIRE(!walkable.empty());

		BotCore::Rng pairRng((uint64_t)(5000 + k));
		BotCore::NavPathfinder finder;
		BotCore::NavSearchParams params;
		params.maxNodes = 1000000;
		BotCore::NavPathResult r;

		int found = 0;
		int nopath = 0;
		for (int q = 0; q < 40; ++q)
		{
			const BotCore::NavCell s = walkable[(size_t)pairRng.NextBelow((uint32_t)walkable.size())];
			const BotCore::NavCell g = walkable[(size_t)pairRng.NextBelow((uint32_t)walkable.size())];
			const float ref = ReferenceCost(grid, s, g);
			finder.Find(grid, s, g, params, r);

			if (ref >= 0.0f)
			{
				CHECK(r.status == BotCore::NavPathStatus::Found);
				CHECK(std::fabs(r.cost - ref) <= (1e-3f * ref + 1e-3f));
				CHECK(PathIsValid(grid, r, s, g));
				++found;
			}
			else
			{
				CHECK(r.status == BotCore::NavPathStatus::NoPath);
				++nopath;
			}
			++totalPairs;
		}

		totalFound += found;
		std::printf("NAVPATH dijkstra: pairs=%d found=%d nopath=%d\n", 40, found, nopath);
	}

	CHECK(totalFound > 0);
	CHECK_EQ(totalPairs, 240);
}

TEST_CASE("NavPath_Deterministic_Reuse")
{
	const float unit = 4.0f;
	BotCore::NavGrid g16 = MakeNav(16, unit, RingEvents(16), HeightZeros(16));
	BotCore::NavGrid g40 = MakeNav(40, unit, RingEvents(40), HeightZeros(40));

	BotCore::NavCell a;
	a.x = 2;
	a.z = 2;
	BotCore::NavCell b;
	b.x = 13;
	b.z = 12;
	BotCore::NavCell c;
	c.x = 3;
	c.z = 3;
	BotCore::NavCell d;
	d.x = 5;
	d.z = 12;

	BotCore::NavSearchParams params;
	BotCore::NavPathfinder finder;
	BotCore::NavPathResult first;
	BotCore::NavPathResult other;
	BotCore::NavPathResult again;
	BotCore::NavPathResult fresh;

	finder.Find(g16, a, b, params, first);
	finder.Find(g16, c, d, params, other);
	finder.Find(g16, a, b, params, again);
	CHECK(SameResult(first, again));

	BotCore::NavPathfinder finder2;
	finder2.Find(g16, a, b, params, fresh);
	CHECK(SameResult(first, fresh));
	CHECK(first.status == BotCore::NavPathStatus::Found);

	BotCore::NavCell a2;
	a2.x = 2;
	a2.z = 2;
	BotCore::NavCell b2;
	b2.x = 30;
	b2.z = 25;

	finder.Find(g40, a2, b2, params, again);
	CHECK(again.status == BotCore::NavPathStatus::Found);
	CHECK(PathIsValid(g40, again, a2, b2));

	finder.Find(g16, a, b, params, again);
	CHECK(again.status == BotCore::NavPathStatus::Found);
	CHECK(PathIsValid(g16, again, a, b));

	finder.Find(g40, a2, b2, params, again);
	CHECK(again.status == BotCore::NavPathStatus::Found);
	CHECK(PathIsValid(g40, again, a2, b2));
}

TEST_CASE("NavPath_RealMap_Queries")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVPATH real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	BotCore::NavCell a;
	a.x = grid.CellOf(1274.0f);
	a.z = grid.CellOf(890.0f);
	BotCore::NavCell b;
	b.x = grid.CellOf(746.0f);
	b.z = grid.CellOf(1106.0f);
	REQUIRE(grid.Walk(a.x, a.z));
	REQUIRE(grid.Walk(b.x, b.z));

	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams params;
	BotCore::NavPathResult r;

	finder.Find(grid, a, b, params, r);
	CHECK(r.status == BotCore::NavPathStatus::Found);
	CHECK(std::fabs(r.cost - 660.617f) < 0.5f);
	CHECK(r.expanded <= 20000);
	CHECK(PathIsValid(grid, r, a, b));
	std::printf("NAVPATH arena A->B: cost=%.3f expanded=%d cells=%d\n", r.cost, r.expanded, (int)r.cells.size());

	BotCore::NavPathResult rev;
	finder.Find(grid, b, a, params, rev);
	CHECK(rev.status == BotCore::NavPathStatus::Found);
	CHECK(std::fabs(rev.cost - r.cost) < 1e-2f);

	BotCore::NavCell pocket;
	pocket.x = 174;
	pocket.z = 215;
	REQUIRE(grid.Walk(pocket.x, pocket.z));

	BotCore::NavSearchParams big;
	big.maxNodes = 200000;
	finder.Find(grid, a, pocket, big, r);
	CHECK(r.status == BotCore::NavPathStatus::NoPath);
	CHECK(r.expanded > 88000);
	CHECK(r.expanded <= grid.MainComponentCells());

	finder.Find(grid, a, pocket, params, r);
	CHECK(r.status == BotCore::NavPathStatus::NodeLimit);
	CHECK_EQ(r.expanded, 20000);
}

TEST_CASE("NavPath_Perf_T_NAV_03")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVPATH real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	const int n = grid.Size();
	std::vector<BotCore::NavCell> cells;
	cells.reserve((size_t)grid.MainComponentCells());
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			if (!grid.Walk(x, z))
				continue;
			BotCore::NavCell c;
			c.x = x;
			c.z = z;
			cells.push_back(c);
		}
	}
	REQUIRE((int)cells.size() == 88508);

#if defined(_DEBUG)
	const int queries = 100;
#else
	const int queries = 1000;
#endif

	BotCore::Rng rng(20261002u);
	const int bounds[3] = { 64, 150, -1 };
	const char * names[3] = { "near64", "far150", "global" };

	for (int set = 0; set < 3; ++set)
	{
		const int bound = bounds[set];

		std::vector<BotCore::NavCell> starts((size_t)queries);
		std::vector<BotCore::NavCell> goals((size_t)queries);
		for (int q = 0; q < queries; ++q)
		{
			int si = 0;
			int gi = 0;
			for (;;)
			{
				si = (int)rng.NextBelow((uint32_t)cells.size());
				gi = (int)rng.NextBelow((uint32_t)cells.size());
				if (si == gi)
					continue;
				if (bound >= 0)
				{
					const int dx = std::abs(cells[(size_t)si].x - cells[(size_t)gi].x);
					const int dz = std::abs(cells[(size_t)si].z - cells[(size_t)gi].z);
					if (std::max(dx, dz) > bound)
						continue;
				}
				break;
			}
			starts[(size_t)q] = cells[(size_t)si];
			goals[(size_t)q] = cells[(size_t)gi];
		}

		BotCore::NavPathfinder finder;
		BotCore::NavSearchParams params;
		BotCore::NavPathResult r;

		// Warm-up queries from the same set, not measured.
		for (int q = 0; q < 20 && q < queries; ++q)
			finder.Find(grid, starts[(size_t)q], goals[(size_t)q], params, r);

		std::vector<double> ms((size_t)queries, 0.0);
		std::vector<int> expanded((size_t)queries, 0);
		int found = 0;
		int nopath = 0;
		int nodelimit = 0;
		bool validPaths = true;

		for (int q = 0; q < queries; ++q)
		{
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			finder.Find(grid, starts[(size_t)q], goals[(size_t)q], params, r);
			const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

			ms[(size_t)q] = std::chrono::duration<double, std::milli>(t1 - t0).count();
			expanded[(size_t)q] = r.expanded;

			if (r.status == BotCore::NavPathStatus::Found)
				++found;
			else if (r.status == BotCore::NavPathStatus::NoPath)
				++nopath;
			else if (r.status == BotCore::NavPathStatus::NodeLimit)
				++nodelimit;

			CHECK(r.expanded <= 20000);
			if (r.status == BotCore::NavPathStatus::Found)
			{
				if (r.cells.empty() || r.cells.front() != starts[(size_t)q] || r.cells.back() != goals[(size_t)q])
					validPaths = false;
				if (q < 50 && !PathIsValid(grid, r, starts[(size_t)q], goals[(size_t)q]))
					validPaths = false;
			}
		}

		CHECK(found + nopath + nodelimit == queries);
		CHECK(validPaths);

		std::vector<double> sortedMs = ms;
		std::sort(sortedMs.begin(), sortedMs.end());
		std::vector<int> sortedExpanded = expanded;
		std::sort(sortedExpanded.begin(), sortedExpanded.end());

		const double ms50 = PercentileDouble(sortedMs, 0.50);
		const double ms95 = PercentileDouble(sortedMs, 0.95);
		const double ms99 = PercentileDouble(sortedMs, 0.99);
		const int exp50 = PercentileInt(sortedExpanded, 0.50);
		const int exp95 = PercentileInt(sortedExpanded, 0.95);

		std::printf("NAVPATH T-NAV-03 set=%s queries=%d found=%d nopath=%d nodelimit=%d expanded_p50=%d expanded_p95=%d ms_p50=%.3f ms_p95=%.3f ms_p99=%.3f\n",
			names[set], queries, found, nopath, nodelimit, exp50, exp95, ms50, ms95, ms99);

		if (set == 0)
		{
			CHECK(found * 100 >= queries * 95);
#ifndef _DEBUG
			CHECK(ms95 <= 2.0);
#endif
		}
	}
}
