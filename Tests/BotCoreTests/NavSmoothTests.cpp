#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavSmooth.h>
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

	BotCore::NavGrid MakeNav(int n, float unit, const std::vector<int16_t> & events, const std::vector<float> & heights)
	{
		BotCore::NavGrid grid;
		if (!grid.Init(n, unit, events, heights))
			CHECK(false);
		grid.Build();
		return grid;
	}

	bool IsSubsequence(const std::vector<BotCore::NavCell> & path, const std::vector<BotCore::NavCell> & sub)
	{
		size_t j = 0;
		for (size_t i = 0; i < path.size() && j < sub.size(); ++i)
		{
			if (path[i] == sub[j])
				++j;
		}
		return j == sub.size();
	}

	bool SegmentsClear(const BotCore::NavGrid & grid, const std::vector<BotCore::NavCell> & waypoints)
	{
		for (size_t i = 1; i < waypoints.size(); ++i)
		{
			if (!BotCore::NavLineClear(grid, waypoints[i - 1], waypoints[i]))
				return false;
		}
		return true;
	}

	float Euclid(const BotCore::NavGrid & grid, BotCore::NavCell a, BotCore::NavCell b)
	{
		const float dx = (float)(b.x - a.x);
		const float dz = (float)(b.z - a.z);
		return grid.Unit() * std::sqrt(dx * dx + dz * dz);
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
}

TEST_CASE("NavSmooth_LineClear_Basics")
{
	const int n = 12;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));

	// (a) Flat interior lines and slopes are clear in both directions.
	CHECK(BotCore::NavLineClear(grid, Cell(2, 3), Cell(9, 3)));
	CHECK(BotCore::NavLineClear(grid, Cell(9, 3), Cell(2, 3)));
	CHECK(BotCore::NavLineClear(grid, Cell(3, 2), Cell(3, 9)));
	CHECK(BotCore::NavLineClear(grid, Cell(3, 9), Cell(3, 2)));
	CHECK(BotCore::NavLineClear(grid, Cell(2, 2), Cell(8, 8)));
	CHECK(BotCore::NavLineClear(grid, Cell(8, 8), Cell(2, 2)));
	CHECK(BotCore::NavLineClear(grid, Cell(2, 3), Cell(9, 7)));
	CHECK(BotCore::NavLineClear(grid, Cell(9, 7), Cell(2, 3)));

	// Same blocked/out-of-grid cell cases.
	CHECK(BotCore::NavLineClear(grid, Cell(5, 5), Cell(5, 5)));
	CHECK(!BotCore::NavLineClear(grid, Cell(0, 5), Cell(0, 5)));
	CHECK(!BotCore::NavLineClear(grid, Cell(-1, 3), Cell(5, 3)));
	CHECK(!BotCore::NavLineClear(grid, Cell(5, 3), Cell(-1, 3)));
	CHECK(!BotCore::NavLineClear(grid, Cell(12, 3), Cell(5, 3)));
	CHECK(!BotCore::NavLineClear(grid, Cell(5, 3), Cell(12, 3)));
	CHECK(!BotCore::NavLineClear(grid, Cell(0, 5), Cell(5, 5)));
	CHECK(!BotCore::NavLineClear(grid, Cell(5, 5), Cell(0, 5)));

	// (b) Wall with a single gap at (6,5): only the horizontal crossing is clear.
	std::vector<int16_t> gap = RingEvents(n);
	for (int z = 1; z <= 10; ++z)
	{
		if (z != 5)
			gap[CellIndex(n, 6, z)] = 0;
	}
	BotCore::NavGrid gapGrid = MakeNav(n, unit, gap, HeightZeros(n));
	CHECK(gapGrid.Walk(6, 5));
	CHECK(BotCore::NavLineClear(gapGrid, Cell(5, 5), Cell(7, 5)));
	CHECK(BotCore::NavLineClear(gapGrid, Cell(7, 5), Cell(5, 5)));
	CHECK(!BotCore::NavLineClear(gapGrid, Cell(5, 4), Cell(7, 6)));
	CHECK(!BotCore::NavLineClear(gapGrid, Cell(7, 6), Cell(5, 4)));
	CHECK(!BotCore::NavLineClear(gapGrid, Cell(5, 4), Cell(7, 5)));
	CHECK(!BotCore::NavLineClear(gapGrid, Cell(7, 5), Cell(5, 4)));
	CHECK(!BotCore::NavLineClear(gapGrid, Cell(5, 4), Cell(7, 4)));
	CHECK(!BotCore::NavLineClear(gapGrid, Cell(7, 4), Cell(5, 4)));

	// (c) One blocked cell at (6,5).
	std::vector<int16_t> wall = RingEvents(n);
	wall[CellIndex(n, 6, 5)] = 0;
	BotCore::NavGrid wallGrid = MakeNav(n, unit, wall, HeightZeros(n));
	CHECK(!BotCore::NavLineClear(wallGrid, Cell(5, 4), Cell(7, 6)));
	CHECK(!BotCore::NavLineClear(wallGrid, Cell(7, 6), Cell(5, 4)));
	CHECK(!BotCore::NavLineClear(wallGrid, Cell(5, 5), Cell(7, 5)));
	CHECK(!BotCore::NavLineClear(wallGrid, Cell(7, 5), Cell(5, 5)));
	CHECK(BotCore::NavLineClear(wallGrid, Cell(5, 3), Cell(7, 3)));
	CHECK(BotCore::NavLineClear(wallGrid, Cell(7, 3), Cell(5, 3)));

	// (d) Slope: a 10 m step at x = 9/10 closes every edge across it.
	const int m = 20;
	std::vector<float> heights((size_t)m * (size_t)m, 0.0f);
	for (int x = 0; x < m; ++x)
	{
		for (int z = 0; z < m; ++z)
			heights[CellIndex(m, x, z)] = (x >= 10) ? 10.0f : 0.0f;
	}
	BotCore::NavGrid slopeGrid = MakeNav(m, unit, RingEvents(m), heights);
	CHECK(!BotCore::NavLineClear(slopeGrid, Cell(8, 5), Cell(12, 5)));
	CHECK(!BotCore::NavLineClear(slopeGrid, Cell(12, 5), Cell(8, 5)));
	CHECK(BotCore::NavLineClear(slopeGrid, Cell(2, 2), Cell(8, 9)));
	CHECK(BotCore::NavLineClear(slopeGrid, Cell(8, 9), Cell(2, 2)));
	CHECK(BotCore::NavLineClear(slopeGrid, Cell(11, 2), Cell(17, 9)));
	CHECK(BotCore::NavLineClear(slopeGrid, Cell(17, 9), Cell(11, 2)));
}

TEST_CASE("NavSmooth_OpenField")
{
	const int n = 40;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));

	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams search;
	BotCore::NavPathResult path;
	BotCore::NavSmoothParams smooth;
	BotCore::NavSmoothResult result;

	finder.Find(grid, Cell(5, 5), Cell(25, 17), search, path);
	REQUIRE(path.status == BotCore::NavPathStatus::Found);
	BotCore::NavSmoothPath(grid, path.cells, smooth, result);
	CHECK_EQ((int)result.waypoints.size(), 2);
	CHECK(result.waypoints.front() == Cell(5, 5));
	CHECK(result.waypoints.back() == Cell(25, 17));
	CHECK(std::fabs(result.length - 4.0f * std::sqrt(544.0f)) < 1e-2f);
	CHECK(result.length < path.cost);

	finder.Find(grid, Cell(5, 5), Cell(30, 5), search, path);
	REQUIRE(path.status == BotCore::NavPathStatus::Found);
	BotCore::NavSmoothPath(grid, path.cells, smooth, result);
	CHECK_EQ((int)result.waypoints.size(), 2);
	CHECK(std::fabs(result.length - 100.0f) < 1e-3f);
}

TEST_CASE("NavSmooth_WallGap_NoCornerCut")
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

	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams search;
	BotCore::NavPathResult path;
	BotCore::NavSmoothParams smooth;
	BotCore::NavSmoothResult result;

	finder.Find(grid, Cell(5, 4), Cell(7, 6), search, path);
	REQUIRE(path.status == BotCore::NavPathStatus::Found);
	REQUIRE((int)path.cells.size() == 5);
	CHECK(path.cells[0] == Cell(5, 4));
	CHECK(path.cells[1] == Cell(5, 5));
	CHECK(path.cells[2] == Cell(6, 5));
	CHECK(path.cells[3] == Cell(7, 5));
	CHECK(path.cells[4] == Cell(7, 6));

	BotCore::NavSmoothPath(grid, path.cells, smooth, result);
	REQUIRE((int)result.waypoints.size() == 4);
	CHECK(result.waypoints[0] == Cell(5, 4));
	CHECK(result.waypoints[1] == Cell(5, 5));
	CHECK(result.waypoints[2] == Cell(7, 5));
	CHECK(result.waypoints[3] == Cell(7, 6));
	CHECK(std::fabs(result.length - 16.0f) < 1e-3f);

	std::vector<BotCore::NavCell> expected;
	expected.push_back(Cell(5, 4));
	expected.push_back(Cell(5, 5));
	expected.push_back(Cell(7, 5));
	expected.push_back(Cell(7, 6));
	CHECK(result.waypoints == expected);

	BotCore::NavSmoothParams one;
	one.maxLookahead = 1;
	BotCore::NavSmoothPath(grid, path.cells, one, result);
	CHECK_EQ((int)result.waypoints.size(), 5);
	CHECK(result.waypoints == path.cells);

	finder.Find(grid, Cell(7, 6), Cell(5, 4), search, path);
	REQUIRE(path.status == BotCore::NavPathStatus::Found);
	BotCore::NavSmoothPath(grid, path.cells, smooth, result);
	CHECK_EQ((int)result.waypoints.size(), 4);
	CHECK(std::fabs(result.length - 16.0f) < 1e-3f);
}

TEST_CASE("NavSmooth_WallDetour")
{
	const int n = 20;
	const float unit = 4.0f;

	std::vector<int16_t> events = RingEvents(n);
	for (int z = 1; z <= 13; ++z)
		events[CellIndex(n, 10, z)] = 0;
	BotCore::NavGrid grid = MakeNav(n, unit, events, HeightZeros(n));

	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams search;
	BotCore::NavPathResult path;
	BotCore::NavSmoothParams smooth;
	BotCore::NavSmoothResult result;

	finder.Find(grid, Cell(5, 5), Cell(15, 5), search, path);
	REQUIRE(path.status == BotCore::NavPathStatus::Found);
	REQUIRE((int)path.cells.size() == 21);
	CHECK(std::fabs(path.cost - 93.255f) < 1e-2f);

	BotCore::NavSmoothPath(grid, path.cells, smooth, result);
	CHECK((int)result.waypoints.size() >= 3);
	CHECK((int)result.waypoints.size() <= 5);
	CHECK(result.waypoints.front() == Cell(5, 5));
	CHECK(result.waypoints.back() == Cell(15, 5));
	CHECK(IsSubsequence(path.cells, result.waypoints));
	CHECK(SegmentsClear(grid, result.waypoints));
	CHECK(result.length < path.cost - 1.0f);
	CHECK(result.length >= 40.0f - 1e-3f);
	CHECK(result.length <= path.cost + 1e-3f);

	for (size_t i = 0; i < result.waypoints.size(); ++i)
	{
		const bool inWallColumn = result.waypoints[i].x == 10
			&& result.waypoints[i].z >= 1 && result.waypoints[i].z <= 13;
		CHECK(!inWallColumn);
	}
}

TEST_CASE("NavSmooth_Invariants_Random")
{
	const float unit = 4.0f;
	int totalPaths = 0;
	int totalCells = 0;
	int totalWaypoints = 0;
	int clearPairs = 0;

	for (int k = 0; k < 6; ++k)
	{
		const int n = 28;
		BotCore::Rng gridRng((uint64_t)(2000 + k));
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
				if (grid.Walk(x, z))
					walkable.push_back(Cell(x, z));
			}
		}
		REQUIRE(!walkable.empty());

		BotCore::Rng pairRng((uint64_t)(6000 + k));
		BotCore::NavPathfinder finder;
		BotCore::NavSearchParams search;
		search.maxNodes = 1000000;
		BotCore::NavPathResult path;
		BotCore::NavSmoothParams smooth;
		BotCore::NavSmoothResult result;
		const int looks[3] = { 1, 3, 64 };

		for (int q = 0; q < 40; ++q)
		{
			const BotCore::NavCell s = walkable[(size_t)pairRng.NextBelow((uint32_t)walkable.size())];
			const BotCore::NavCell g = walkable[(size_t)pairRng.NextBelow((uint32_t)walkable.size())];
			finder.Find(grid, s, g, search, path);
			if (path.status != BotCore::NavPathStatus::Found)
				continue;

			++totalPaths;
			totalCells += (int)path.cells.size();

			for (int li = 0; li < 3; ++li)
			{
				smooth.maxLookahead = looks[li];
				BotCore::NavSmoothPath(grid, path.cells, smooth, result);
				CHECK(!result.waypoints.empty());
				CHECK(result.waypoints.front() == s);
				CHECK(result.waypoints.back() == g);
				CHECK(IsSubsequence(path.cells, result.waypoints));
				CHECK(SegmentsClear(grid, result.waypoints));
				CHECK(result.length <= path.cost + 1e-3f * path.cost + 1e-3f);
				CHECK(result.length >= Euclid(grid, s, g) - 1e-3f);
				if (looks[li] == 1)
					CHECK(result.waypoints == path.cells);
				if (looks[li] == 64)
					totalWaypoints += (int)result.waypoints.size();
			}
		}

		// NavLineClear is symmetric; when it is clear, A* must find a path of octile length.
		BotCore::Rng propRng((uint64_t)(7000 + k));
		for (int q = 0; q < 200; ++q)
		{
			const BotCore::NavCell a = walkable[(size_t)propRng.NextBelow((uint32_t)walkable.size())];
			const BotCore::NavCell b = walkable[(size_t)propRng.NextBelow((uint32_t)walkable.size())];
			const bool ab = BotCore::NavLineClear(grid, a, b);
			const bool ba = BotCore::NavLineClear(grid, b, a);
			CHECK(ab == ba);
			if (!ab)
				continue;

			++clearPairs;
			finder.Find(grid, a, b, search, path);
			CHECK(path.status == BotCore::NavPathStatus::Found);
			if (path.status == BotCore::NavPathStatus::Found)
			{
				const int ddx = b.x - a.x;
				const int ddz = b.z - a.z;
				const int adx = ddx < 0 ? -ddx : ddx;
				const int adz = ddz < 0 ? -ddz : ddz;
				const float oct = BotCore::NavOctile(adx, adz, unit);
				CHECK(std::fabs(path.cost - oct) <= 1e-3f * oct + 1e-3f);
			}
		}
	}

	std::printf("NAVSMOOTH random: paths=%d cells=%d waypoints=%d clear_pairs=%d\n",
		totalPaths, totalCells, totalWaypoints, clearPairs);
	CHECK(totalPaths > 0);
	CHECK(clearPairs > 0);
}

TEST_CASE("NavSmooth_Params_Edge")
{
	const int n = 40;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));

	BotCore::NavSmoothParams smooth;
	BotCore::NavSmoothResult result;

	const std::vector<BotCore::NavCell> empty;
	BotCore::NavSmoothPath(grid, empty, smooth, result);
	CHECK(result.waypoints.empty());
	CHECK_EQ(result.length, 0.0f);

	std::vector<BotCore::NavCell> one;
	one.push_back(Cell(5, 5));
	BotCore::NavSmoothPath(grid, one, smooth, result);
	CHECK_EQ((int)result.waypoints.size(), 1);
	CHECK_EQ(result.length, 0.0f);

	std::vector<BotCore::NavCell> two;
	two.push_back(Cell(5, 5));
	two.push_back(Cell(6, 5));
	BotCore::NavSmoothPath(grid, two, smooth, result);
	CHECK_EQ((int)result.waypoints.size(), 2);
	CHECK(std::fabs(result.length - 4.0f) < 1e-3f);

	// Reuse a filled result: the output is fully overwritten.
	result.waypoints.clear();
	result.waypoints.push_back(Cell(1, 1));
	result.waypoints.push_back(Cell(2, 2));
	result.length = 999.0f;
	BotCore::NavSmoothPath(grid, empty, smooth, result);
	CHECK(result.waypoints.empty());
	CHECK_EQ(result.length, 0.0f);

	std::vector<BotCore::NavCell> line;
	for (int x = 5; x <= 14; ++x)
		line.push_back(Cell(x, 5));

	smooth.maxLookahead = 1;
	BotCore::NavSmoothPath(grid, line, smooth, result);
	CHECK_EQ((int)result.waypoints.size(), 10);
	CHECK(result.waypoints == line);

	smooth.maxLookahead = 0;
	BotCore::NavSmoothPath(grid, line, smooth, result);
	CHECK_EQ((int)result.waypoints.size(), 10);

	smooth.maxLookahead = -5;
	BotCore::NavSmoothPath(grid, line, smooth, result);
	CHECK_EQ((int)result.waypoints.size(), 10);

	smooth.maxLookahead = 2;
	BotCore::NavSmoothPath(grid, line, smooth, result);
	std::vector<BotCore::NavCell> expected;
	expected.push_back(Cell(5, 5));
	expected.push_back(Cell(7, 5));
	expected.push_back(Cell(9, 5));
	expected.push_back(Cell(11, 5));
	expected.push_back(Cell(13, 5));
	expected.push_back(Cell(14, 5));
	CHECK(result.waypoints == expected);
	CHECK(std::fabs(result.length - 36.0f) < 1e-3f);

	smooth.maxLookahead = 64;
	BotCore::NavSmoothPath(grid, line, smooth, result);
	CHECK_EQ((int)result.waypoints.size(), 2);
	CHECK(result.waypoints.front() == Cell(5, 5));
	CHECK(result.waypoints.back() == Cell(14, 5));
	CHECK(std::fabs(result.length - 36.0f) < 1e-3f);

	smooth.maxLookahead = 64;
	BotCore::NavSmoothResult again;
	BotCore::NavSmoothPath(grid, line, smooth, result);
	BotCore::NavSmoothPath(grid, line, smooth, again);
	CHECK(result.waypoints == again.waypoints);
	CHECK_EQ(result.length, again.length);
}

TEST_CASE("NavSmooth_RealMap")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVSMOOTH real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
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
	BotCore::NavSearchParams search;
	BotCore::NavPathResult path;
	BotCore::NavSmoothParams smooth;
	BotCore::NavSmoothResult result;

	finder.Find(grid, a, b, search, path);
	REQUIRE(path.status == BotCore::NavPathStatus::Found);
	BotCore::NavSmoothPath(grid, path.cells, smooth, result);

	CHECK((int)path.cells.size() <= 200);
	CHECK((int)result.waypoints.size() <= 25);
	CHECK((int)result.waypoints.size() >= 2);
	CHECK(result.waypoints.front() == a);
	CHECK(result.waypoints.back() == b);
	CHECK(IsSubsequence(path.cells, result.waypoints));
	CHECK(SegmentsClear(grid, result.waypoints));
	CHECK(result.length >= 570.4f);
	CHECK(result.length <= 661.1f);
	CHECK(result.length <= path.cost + 1e-3f * path.cost + 1e-3f);
	std::printf("NAVSMOOTH arena A->B: cells=%d waypoints=%d length=%.3f cost=%.3f\n",
		(int)path.cells.size(), (int)result.waypoints.size(), result.length, path.cost);

	BotCore::NavPathResult reversePath;
	BotCore::NavSmoothResult reverse;
	finder.Find(grid, b, a, search, reversePath);
	REQUIRE(reversePath.status == BotCore::NavPathStatus::Found);
	BotCore::NavSmoothPath(grid, reversePath.cells, smooth, reverse);
	CHECK((int)reverse.waypoints.size() <= 25);
	CHECK(reverse.waypoints.front() == b);
	CHECK(reverse.waypoints.back() == a);
	CHECK(SegmentsClear(grid, reverse.waypoints));
	CHECK(std::fabs(result.length - reverse.length) < 1.0f);
}

TEST_CASE("NavSmooth_Perf")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVSMOOTH real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
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
			const int dx = std::abs(cells[(size_t)si].x - cells[(size_t)gi].x);
			const int dz = std::abs(cells[(size_t)si].z - cells[(size_t)gi].z);
			if (std::max(dx, dz) > 64)
				continue;
			break;
		}
		starts[(size_t)q] = cells[(size_t)si];
		goals[(size_t)q] = cells[(size_t)gi];
	}

	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams search;
	BotCore::NavPathResult path;
	BotCore::NavSmoothParams smooth;
	BotCore::NavSmoothResult result;

	// Warm-up queries from the same set, not measured.
	for (int q = 0; q < 20 && q < queries; ++q)
	{
		finder.Find(grid, starts[(size_t)q], goals[(size_t)q], search, path);
		if (path.status == BotCore::NavPathStatus::Found)
			BotCore::NavSmoothPath(grid, path.cells, smooth, result);
	}

	std::vector<double> ms;
	ms.reserve((size_t)queries);
	int found = 0;
	long long cellsSum = 0;
	long long waypointsSum = 0;
	double ratioSum = 0.0;
	int ratioCount = 0;
	bool invariants = true;
	int checked = 0;

	for (int q = 0; q < queries; ++q)
	{
		finder.Find(grid, starts[(size_t)q], goals[(size_t)q], search, path);
		if (path.status != BotCore::NavPathStatus::Found)
			continue;

		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		BotCore::NavSmoothPath(grid, path.cells, smooth, result);
		const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
		ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

		++found;
		cellsSum += (long long)path.cells.size();
		waypointsSum += (long long)result.waypoints.size();
		if (path.cost > 0.0f)
		{
			ratioSum += (double)(result.length / path.cost);
			++ratioCount;
		}
		if (checked < 50)
		{
			if (!IsSubsequence(path.cells, result.waypoints) || !SegmentsClear(grid, result.waypoints))
				invariants = false;
			++checked;
		}
	}

	std::sort(ms.begin(), ms.end());
	const double ms50 = PercentileDouble(ms, 0.50);
	const double ms95 = PercentileDouble(ms, 0.95);
	const double ms99 = PercentileDouble(ms, 0.99);
	const double cellsMean = found > 0 ? (double)cellsSum / (double)found : 0.0;
	const double waypointsMean = found > 0 ? (double)waypointsSum / (double)found : 0.0;
	const double ratioMean = ratioCount > 0 ? ratioSum / (double)ratioCount : 0.0;

	std::printf("NAVSMOOTH perf set=near64 paths=%d cells_mean=%.1f waypoints_mean=%.1f length_ratio_mean=%.3f ms_p50=%.3f ms_p95=%.3f ms_p99=%.3f\n",
		found, cellsMean, waypointsMean, ratioMean, ms50, ms95, ms99);

	CHECK(found * 100 >= queries * 95);
	CHECK(waypointsSum * 100 <= cellsSum * 30);
	CHECK(ratioMean <= 0.99);
	CHECK(ratioMean >= 0.85);
	CHECK(invariants);
#ifndef _DEBUG
	CHECK(ms95 <= 0.5);
#endif
}
