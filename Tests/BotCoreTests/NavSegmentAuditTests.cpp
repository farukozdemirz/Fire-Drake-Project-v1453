#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavSegment.h>
#include <BotCore/NavSmooth.h>
#include <BotCore/Rng.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
	using BotCore::NavCell;
	using BotCore::NavGrid;
	using BotCore::NavSegmentResult;
	using BotCore::NavSegmentVerdict;

	NavCell Cell(int x, int z)
	{
		NavCell c;
		c.x = x;
		c.z = z;
		return c;
	}

	// Loads the exported zone 71 grid; prints a SKIPPED line and returns false when it is missing,
	// so the acceptance run (with tools/nav-export.py run first) exercises the real map.
	bool LoadZone71OrSkip(NavGrid & grid, const char * tag)
	{
		if (!grid.LoadFile("build/nav/zone71.navgrid"))
		{
			std::printf("NAVAUDIT %s: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n", tag);
			return false;
		}
		grid.Build();
		if (grid.MainComponentCells() != 88508)
		{
			std::printf("NAVAUDIT %s: SKIPPED (main component %d != 88508)\n", tag, grid.MainComponentCells());
			return false;
		}
		return true;
	}

	void CollectWalk(const NavGrid & grid, std::vector<NavCell> & cells)
	{
		const int n = grid.Size();
		cells.clear();
		cells.reserve((size_t)grid.MainComponentCells());
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (grid.Walk(x, z))
					cells.push_back(Cell(x, z));
			}
		}
	}

	double CenterX(const NavGrid & grid, int i) { return (double)grid.CellCenter(i); }

	NavSegmentResult CheckCenter(const NavGrid & grid, NavCell a, NavCell b, bool checkSlope = false)
	{
		return BotCore::NavCheckSegment(grid, CenterX(grid, a.x), CenterX(grid, a.z),
			CenterX(grid, b.x), CenterX(grid, b.z), checkSlope);
	}

	NavCell RandomNear(const std::vector<NavCell> & walk, BotCore::Rng & rng, NavCell a, int cheb)
	{
		NavCell best = a;
		for (int tries = 0; tries < 200000; ++tries)
		{
			const NavCell b = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
			if (b == a)
				continue;
			if (std::abs(b.x - a.x) <= cheb && std::abs(b.z - a.z) <= cheb)
			{
				best = b;
				break;
			}
		}
		return best;
	}

	struct AuditPath
	{
		NavCell start;
		NavCell goal;
		BotCore::NavPathResult path;
		BotCore::NavSmoothResult smooth;
	};

	// 3000 mixed-range A* queries with the default smoothing (same seed and Chebyshev mix as the
	// measurement tool); the found paths are the regression pool for both planner and line-clear.
	void BuildAuditPool(const NavGrid & grid, int queries, std::vector<AuditPath> & out)
	{
		std::vector<NavCell> walk;
		CollectWalk(grid, walk);
		out.clear();
		if (walk.empty())
			return;

		BotCore::Rng rng(20261002u);
		BotCore::NavPathfinder finder;
		BotCore::NavSearchParams search;
		const BotCore::NavSmoothParams smooth;

		for (int q = 0; q < queries; ++q)
		{
			const int cheb = (q % 3 == 0) ? 64 : (q % 3 == 1 ? 150 : 40);
			const NavCell a = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
			const NavCell b = RandomNear(walk, rng, a, cheb);
			if (a == b)
				continue;

			AuditPath item;
			item.start = a;
			item.goal = b;
			finder.Find(grid, a, b, search, item.path);
			if (item.path.status != BotCore::NavPathStatus::Found)
				continue;
			BotCore::NavSmoothPath(grid, item.path.cells, smooth, item.smooth);
			out.push_back(item);
		}
	}

	void PrintSegmentExample(const char * kind, double ax, double az, double bx, double bz, const NavSegmentResult & r)
	{
		std::printf("EXAMPLE %s (%.1f,%.1f)->(%.1f,%.1f) world verdict=%d cell=(%d,%d) touched=%d\n",
			kind, ax, az, bx, bz, (int)r.verdict, r.cellX, r.cellZ, r.cellsTouched);
	}
}

TEST_CASE("NavSegmentAudit_Planner")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "planner"))
		return;

	std::vector<AuditPath> pool;
	BuildAuditPool(grid, 3000, pool);

	const double step = 6.75;
	long paths = 0;
	long rawEdges = 0;
	long rawBad = 0;
	long smoothSegments = 0;
	long smoothBad = 0;
	long chords = 0;
	long chordBad = 0;
	int shown = 0;

	for (size_t p = 0; p < pool.size(); ++p)
	{
		const std::vector<NavCell> & cells = pool[p].path.cells;
		const std::vector<NavCell> & way = pool[p].smooth.waypoints;
		if (cells.size() < 2 || way.size() < 2)
			continue;
		++paths;

		// (a) raw A* path: every cell-centre to cell-centre edge must stay walkable.
		for (size_t i = 1; i < cells.size(); ++i)
		{
			++rawEdges;
			const NavSegmentResult r = CheckCenter(grid, cells[i - 1], cells[i]);
			if (r.verdict != NavSegmentVerdict::Ok)
			{
				++rawBad;
				if (shown < 3)
				{
					PrintSegmentExample("planner raw edge",
						CenterX(grid, cells[i - 1].x), CenterX(grid, cells[i - 1].z),
						CenterX(grid, cells[i].x), CenterX(grid, cells[i].z), r);
					++shown;
				}
			}
		}

		// (b) smoothed polyline segments (NavSmoothPath with default parameters).
		for (size_t i = 1; i < way.size(); ++i)
		{
			++smoothSegments;
			const NavSegmentResult r = CheckCenter(grid, way[i - 1], way[i]);
			if (r.verdict != NavSegmentVerdict::Ok)
			{
				++smoothBad;
				if (shown < 3)
				{
					PrintSegmentExample("planner smoothed segment",
						CenterX(grid, way[i - 1].x), CenterX(grid, way[i - 1].z),
						CenterX(grid, way[i].x), CenterX(grid, way[i].z), r);
					++shown;
				}
			}
		}

		// (c) 6.75 m packet chords along the smoothed polyline: the mandatory Walk-only supercover
		// must accept each one, i.e. no packet step cuts a corner.
		double curx = CenterX(grid, way[0].x);
		double curz = CenterX(grid, way[0].z);
		for (size_t i = 1; i < way.size(); ++i)
		{
			const double tx = CenterX(grid, way[i].x);
			const double tz = CenterX(grid, way[i].z);
			for (;;)
			{
				const double sdx = tx - curx;
				const double sdz = tz - curz;
				const double remain = std::sqrt(sdx * sdx + sdz * sdz);
				const bool last = remain <= step + 1e-9;
				double nx = tx;
				double nz = tz;
				if (!last)
				{
					nx = curx + sdx / remain * step;
					nz = curz + sdz / remain * step;
				}
				++chords;
				const NavSegmentResult r = BotCore::NavCheckSegment(grid, curx, curz, nx, nz);
				if (r.verdict != NavSegmentVerdict::Ok)
				{
					++chordBad;
					if (shown < 3)
					{
						PrintSegmentExample("planner packet chord", curx, curz, nx, nz, r);
						++shown;
					}
				}
				curx = nx;
				curz = nz;
				if (last)
					break;
			}
		}
	}

	std::printf("NAVAUDIT planner paths=%ld raw_edges=%ld raw_bad=%ld smooth_segments=%ld smooth_bad=%ld chords=%ld chord_bad=%ld\n",
		paths, rawEdges, rawBad, smoothSegments, smoothBad, chords, chordBad);
	CHECK(paths > 0);
	CHECK_EQ(rawBad, 0);
	CHECK_EQ(smoothBad, 0);
	CHECK_EQ(chordBad, 0);
}

TEST_CASE("NavSegmentAudit_LineClear")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "lineclear"))
		return;

	long pairs = 0;
	long clear = 0;
	long falsePositive = 0;
	int shown = 0;

	// NavLineClear is the smoothing/planner predicate; whenever it accepts a pair, the mandatory
	// supercover NavCheckSegment must also accept it (no false positive).
	auto auditPair = [&](const NavGrid & g, NavCell a, NavCell b)
	{
		if (a == b || !g.Walk(a.x, a.z) || !g.Walk(b.x, b.z))
			return;
		++pairs;
		if (!BotCore::NavLineClear(g, a, b))
			return;
		++clear;
		const NavSegmentResult r = CheckCenter(g, a, b);
		if (r.verdict != NavSegmentVerdict::Ok)
		{
			++falsePositive;
			if (shown < 3)
			{
				std::printf("EXAMPLE lineclear false positive cells (%d,%d)->(%d,%d) world (%.1f,%.1f)->(%.1f,%.1f) verdict=%d cell=(%d,%d)\n",
					a.x, a.z, b.x, b.z,
					CenterX(g, a.x), CenterX(g, a.z), CenterX(g, b.x), CenterX(g, b.z),
					(int)r.verdict, r.cellX, r.cellZ);
				++shown;
			}
		}
	};

	// (a) Cell pairs from the planner pool (same stride iteration as the measurement tool),
	// capped at 20000 pairs.
	{
		std::vector<AuditPath> pool;
		BuildAuditPool(grid, 3000, pool);
		bool capped = false;
		for (size_t p = 0; p < pool.size() && !capped; ++p)
		{
			const std::vector<NavCell> & cells = pool[p].path.cells;
			for (size_t i = 0; i + 2 < cells.size() && !capped; i += 3)
			{
				for (size_t j = i + 2; j < cells.size() && j <= i + 64; j += 5)
				{
					auditPair(grid, cells[i], cells[j]);
					if (pairs >= 20000)
					{
						capped = true;
						break;
					}
				}
			}
		}
	}

	// (b) Synthetic 13x13 grids: blocked border, open 9x9 interior, one extra blocked cell at
	// every interior position; all walkable cell pairs are checked under that mutation.
	{
		const int n = 13;
		for (int bxCell = 3; bxCell <= 9; ++bxCell)
		{
			for (int bzCell = 3; bzCell <= 9; ++bzCell)
			{
				std::vector<int16_t> ev((size_t)n * n, 0);
				for (int x = 2; x <= 10; ++x)
				{
					for (int z = 2; z <= 10; ++z)
						ev[(size_t)x * n + z] = 1;
				}
				ev[(size_t)bxCell * n + bzCell] = 0;
				NavGrid g;
				if (!g.Init(n, 4.0f, ev, std::vector<float>((size_t)n * n, 0.0f)))
					continue;
				g.Build();
				for (int ax = 2; ax <= 10; ++ax)
				{
					for (int az = 2; az <= 10; ++az)
					{
						for (int bx = 2; bx <= 10; ++bx)
						{
							for (int bz = 2; bz <= 10; ++bz)
								auditPair(g, Cell(ax, az), Cell(bx, bz));
						}
					}
				}
			}
		}
	}

	// (c) Random clutter grids: 13x13, blocked border, open 9x9 interior with 22% blocked, 40
	// probes each (a dense random layout is where a non-super-cover line check can slip).
	{
		const int n = 13;
		BotCore::Rng rng(99u);
		for (int round = 0; round < 3000; ++round)
		{
			std::vector<int16_t> ev((size_t)n * n, 0);
			for (int x = 2; x <= 10; ++x)
			{
				for (int z = 2; z <= 10; ++z)
					ev[(size_t)x * n + z] = (rng.NextBelow(100) < 22) ? 0 : 1;
			}
			NavGrid g;
			if (!g.Init(n, 4.0f, ev, std::vector<float>((size_t)n * n, 0.0f)))
				continue;
			g.Build();
			for (int k = 0; k < 40; ++k)
			{
				const int ax = 2 + (int)rng.NextBelow(9);
				const int az = 2 + (int)rng.NextBelow(9);
				const int bx = 2 + (int)rng.NextBelow(9);
				const int bz = 2 + (int)rng.NextBelow(9);
				auditPair(g, Cell(ax, az), Cell(bx, bz));
			}
		}
	}

	std::printf("NAVAUDIT lineclear pairs=%ld clear=%ld false_positive=%ld\n", pairs, clear, falsePositive);
	CHECK(pairs > 0);
	CHECK_EQ(falsePositive, 0);
}

TEST_CASE("NavSegmentAudit_StraightSteps")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "straight"))
		return;

	// Fixed regression vectors: both endpoints are in walkable cells, yet the straight step touches
	// a blocked cell. This is the evidence that a planner-less /bot move packet has no walkability
	// guarantee (CLI-08; the executor guard is F5-55).
	const double fixed[3][4] = {
		{ 1566.0, 878.0, 1578.0, 866.0 },
		{ 926.0, 834.0, 934.0, 826.0 },
		{ 1102.0, 1034.0, 1106.0, 1046.0 }
	};
	for (int k = 0; k < 3; ++k)
	{
		const int axCell = grid.CellOf((float)fixed[k][0]);
		const int azCell = grid.CellOf((float)fixed[k][1]);
		const int bxCell = grid.CellOf((float)fixed[k][2]);
		const int bzCell = grid.CellOf((float)fixed[k][3]);
		REQUIRE(grid.Walk(axCell, azCell));
		REQUIRE(grid.Walk(bxCell, bzCell));

		const NavSegmentResult r = BotCore::NavCheckSegment(grid,
			fixed[k][0], fixed[k][1], fixed[k][2], fixed[k][3]);
		if (r.verdict != NavSegmentVerdict::BlockedCell)
		{
			std::printf("NAVAUDIT straight fixed[%d] (%.1f,%.1f)->(%.1f,%.1f) verdict=%d cell=(%d,%d)\n",
				k, fixed[k][0], fixed[k][1], fixed[k][2], fixed[k][3], (int)r.verdict, r.cellX, r.cellZ);
		}
		CHECK(r.verdict == NavSegmentVerdict::BlockedCell);
		CHECK(!grid.Walk(r.cellX, r.cellZ));   // the reported cell is the blocked one
	}

	// Control: a straight step inside the open arena is accepted.
	CHECK(BotCore::NavCheckSegment(grid, 1274.0, 890.0, 1280.0, 890.0).verdict == NavSegmentVerdict::Ok);

	// Random 2-3 cell straight steps: the blocked ratio is the standing evidence; acceptance is
	// only blocked > 0 (the ratio is printed for the report).
	std::vector<NavCell> walk;
	CollectWalk(grid, walk);
	REQUIRE(!walk.empty());

	BotCore::Rng rng(20261002u);
	const int total = 6000;
	long pairs = 0;
	long blocked = 0;
	for (int q = 0; q < total; ++q)
	{
		const NavCell a = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		const NavCell b = RandomNear(walk, rng, a, 3);
		if (a == b)
			continue;
		++pairs;
		const NavSegmentResult r = CheckCenter(grid, a, b);
		if (r.verdict == NavSegmentVerdict::BlockedCell)
			++blocked;
	}

	std::printf("NAVAUDIT straight pairs=%ld blocked=%ld\n", pairs, blocked);
	CHECK(pairs > 0);
	CHECK(blocked > 0);
}
