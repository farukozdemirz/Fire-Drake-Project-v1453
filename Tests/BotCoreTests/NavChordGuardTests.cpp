#include "MiniTest.h"

#include <BotCore/NavChordGuard.h>
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
	using BotCore::ChordResult;
	using BotCore::ChordVerdict;
	using BotCore::NavCell;
	using BotCore::NavGrid;

	// 9x9 grid, unit 1 m: the interior (1..7) is open, one wall column (x = 4) blocks z 2..6
	// with a gap at z = 1 and z = 7, so both sides stay connected (a single main component).
	std::vector<int16_t> SyntheticEvents()
	{
		const int n = 9;
		std::vector<int16_t> ev((size_t)n * n, 0);
		for (int x = 1; x <= 7; ++x)
		{
			for (int z = 1; z <= 7; ++z)
				ev[(size_t)x * n + z] = 1;
		}
		for (int z = 2; z <= 6; ++z)
			ev[(size_t)4 * n + z] = 0;
		return ev;
	}

	NavGrid MakeSynthetic()
	{
		NavGrid grid;
		if (!grid.Init(9, 1.0f, SyntheticEvents(), std::vector<float>(81, 0.0f)))
			CHECK(false);
		grid.Build();
		return grid;
	}

	// Loads the exported zone 71 grid; prints a SKIPPED line and returns false when it is missing,
	// so the acceptance run (with tools/nav-export.py run first) exercises the real map.
	bool LoadZone71OrSkip(NavGrid & grid, const char * tag)
	{
		if (!grid.LoadFile("build/nav/zone71.navgrid"))
		{
			std::printf("NAVCHORD %s: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n", tag);
			return false;
		}
		grid.Build();
		if (grid.MainComponentCells() != 88508)
		{
			std::printf("NAVCHORD %s: SKIPPED (main component %d != 88508)\n", tag, grid.MainComponentCells());
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
				{
					NavCell c;
					c.x = x;
					c.z = z;
					cells.push_back(c);
				}
			}
		}
	}

	NavCell RandomNear(const std::vector<NavCell> & walk, BotCore::Rng & rng, NavCell a, int cheb)
	{
		NavCell best = a;
		for (int tries = 0; tries < 200000; ++tries)
		{
			const NavCell b = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
			if (b.x == a.x && b.z == a.z)
				continue;
			if (std::abs(b.x - a.x) <= cheb && std::abs(b.z - a.z) <= cheb)
			{
				best = b;
				break;
			}
		}
		return best;
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

	// Mirrors the executor packet quantisation (uint16(w * 10 + 0.5) / 10).
	float Quantise(double w)
	{
		return (float)(uint16_t)(w * 10.0 + 0.5) / 10.0f;
	}
}

TEST_CASE("NavChord_Skipped_NoGrid")
{
	// No grid (NAV off / not ready): every chord is Skipped, never BlockedCell.
	CHECK(BotCore::CheckMoveChord(nullptr, 0.0f, 0.0f, 100.0f, 100.0f).verdict == ChordVerdict::Skipped);
	CHECK(BotCore::CheckMoveChord(nullptr, 10.0f, 20.0f, 10.0f, 20.0f).verdict == ChordVerdict::Skipped);
}

TEST_CASE("NavChord_Synthetic")
{
	NavGrid grid = MakeSynthetic();
	REQUIRE(grid.MainComponentCells() > 0);
	REQUIRE(!grid.Walk(4, 3));   // wall cell
	REQUIRE(grid.Walk(3, 3));    // left side
	REQUIRE(grid.Walk(5, 3));    // right side

	// A chord crossing the wall column hits the blocked cell.
	ChordResult r = BotCore::CheckMoveChord(&grid, 3.5f, 3.5f, 5.5f, 3.5f);
	CHECK(r.verdict == ChordVerdict::BlockedCell);
	CHECK_EQ(r.cellX, 4);
	CHECK_EQ(r.cellZ, 3);

	// The open row (z = 1) is accepted.
	r = BotCore::CheckMoveChord(&grid, 1.5f, 1.5f, 6.5f, 1.5f);
	CHECK(r.verdict == ChordVerdict::Ok);

	// A chord through the wall corner vertex (4.5,1.5)->(5.5,2.5) touches the blocked cell (4,2)
	// only at that corner: the conservative supercover still rejects it.
	r = BotCore::CheckMoveChord(&grid, 4.5f, 1.5f, 5.5f, 2.5f);
	CHECK(r.verdict == ChordVerdict::BlockedCell);
	CHECK_EQ(r.cellX, 4);
	CHECK_EQ(r.cellZ, 2);

	// An endpoint outside the grid is OutOfBounds (NavSegment visits the out-of-bounds start cell).
	r = BotCore::CheckMoveChord(&grid, -1.0f, 3.5f, 3.5f, 3.5f);
	CHECK(r.verdict == ChordVerdict::OutOfBounds);
	CHECK_EQ(r.cellX, -1);

	// A chord from inside toward the map edge crosses the blocked border cell first, so it is
	// BlockedCell rather than OutOfBounds (the reported cell is the blocked border, not -1).
	r = BotCore::CheckMoveChord(&grid, 1.5f, 1.5f, 20.0f, 20.0f);
	CHECK(r.verdict == ChordVerdict::BlockedCell);
	CHECK(!grid.Walk(r.cellX, r.cellZ));

	// Non-finite input is fail-closed.
	r = BotCore::CheckMoveChord(&grid, std::nanf(""), 1.5f, 2.0f, 2.0f);
	CHECK(r.verdict == ChordVerdict::OutOfBounds);

	// The verdict and the reported cell do not depend on the chord direction.
	const ChordResult fwd = BotCore::CheckMoveChord(&grid, 3.5f, 3.5f, 5.5f, 3.5f);
	const ChordResult rev = BotCore::CheckMoveChord(&grid, 5.5f, 3.5f, 3.5f, 3.5f);
	CHECK(fwd.verdict == rev.verdict);
	CHECK_EQ(fwd.cellX, rev.cellX);
	CHECK_EQ(fwd.cellZ, rev.cellZ);
}

TEST_CASE("NavChord_StopPacket_NeverBlocked")
{
	NavGrid grid = MakeSynthetic();

	// Shorter than kChordIgnoreMeters: always Ok, on a blocked start cell...
	CHECK(BotCore::CheckMoveChord(&grid, 4.5f, 3.5f, 4.5f, 3.5f).verdict == ChordVerdict::Ok);      // 0 m
	CHECK(BotCore::CheckMoveChord(&grid, 4.5f, 3.5f, 4.55f, 3.55f).verdict == ChordVerdict::Ok);    // 0.0707 m
	CHECK(BotCore::CheckMoveChord(&grid, 4.5f, 3.5f, 4.579f, 3.5f).verdict == ChordVerdict::Ok);    // 0.079 m

	// ...and on a cell boundary.
	CHECK(BotCore::CheckMoveChord(&grid, 4.0f, 3.5f, 4.0f, 3.5f).verdict == ChordVerdict::Ok);      // 0 m
	CHECK(BotCore::CheckMoveChord(&grid, 4.0f, 3.5f, 4.05f, 3.55f).verdict == ChordVerdict::Ok);    // 0.0707 m

	// 0.2 m is a real step: entering a blocked cell is rejected (the threshold hides nothing).
	const ChordResult r = BotCore::CheckMoveChord(&grid, 3.9f, 3.5f, 4.1f, 3.5f);
	CHECK(r.verdict == ChordVerdict::BlockedCell);
	CHECK_EQ(r.cellX, 4);
	CHECK_EQ(r.cellZ, 3);
}

TEST_CASE("NavChord_StartNotWalk")
{
	NavGrid grid = MakeSynthetic();

	// Start on the blocked wall cell (4,3); the chord leaves into the walkable left side.
	ChordResult r = BotCore::CheckMoveChord(&grid, 4.5f, 3.5f, 2.5f, 3.5f);
	CHECK(r.verdict == ChordVerdict::Ok);
	CHECK(r.startExempt);

	// Same, into the walkable right side.
	r = BotCore::CheckMoveChord(&grid, 4.5f, 3.5f, 6.5f, 3.5f);
	CHECK(r.verdict == ChordVerdict::Ok);
	CHECK(r.startExempt);

	// The chord leaves the blocked start cell into the blocked column: rejected.
	r = BotCore::CheckMoveChord(&grid, 4.5f, 3.5f, 4.5f, 5.5f);
	CHECK(r.verdict == ChordVerdict::BlockedCell);
	CHECK_EQ(r.cellX, 4);
	CHECK_EQ(r.cellZ, 4);

	// A chord that stays inside the blocked start cell is Ok (nothing to leave).
	r = BotCore::CheckMoveChord(&grid, 4.2f, 3.5f, 4.7f, 3.5f);
	CHECK(r.verdict == ChordVerdict::Ok);

	// A start position outside the grid is OutOfBounds.
	r = BotCore::CheckMoveChord(&grid, 100.0f, 3.5f, 90.0f, 3.5f);
	CHECK(r.verdict == ChordVerdict::OutOfBounds);
}

TEST_CASE("NavChord_RealMap_StraightVectors")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "real map"))
		return;

	// Fixed vectors from tools/nav-regress/good.txt: both endpoints are walkable, yet the straight
	// step touches a blocked cell. The reported cell is NavCheckSegment's first offending cell (the
	// good.txt EXAMPLE cell comes from nav_measure's own touched-cell order and may differ).
	const double v[3][4] = {
		{ 654.0, 818.0, 646.0, 806.0 },
		{ 958.0, 946.0, 946.0, 958.0 },
		{ 994.0, 1006.0, 990.0, 994.0 }
	};

	long blocked = 0;
	for (int k = 0; k < 3; ++k)
	{
		const ChordResult r = BotCore::CheckMoveChord(&grid,
			(float)v[k][0], (float)v[k][1], (float)v[k][2], (float)v[k][3]);
		CHECK(r.verdict == ChordVerdict::BlockedCell);
		CHECK(!grid.Walk(r.cellX, r.cellZ));   // the reported cell is a blocked one
		std::printf("NAVCHORD vector[%d] cell=(%d,%d)\n", k, r.cellX, r.cellZ);
		if (r.verdict == ChordVerdict::BlockedCell)
			++blocked;
	}

	std::printf("NAVCHORD real map: vectors=3 blocked=%ld\n", blocked);
	CHECK_EQ(blocked, 3);
}

TEST_CASE("NavChord_RealMap_PlannerPaths_Clean")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "planner"))
		return;

	std::vector<NavCell> walk;
	CollectWalk(grid, walk);
	REQUIRE(!walk.empty());

	BotCore::Rng rng(20261002u);
	BotCore::NavPathfinder finder;
	BotCore::NavSearchParams search;
	const BotCore::NavSmoothParams smooth;

	const double step = 6.75;
	long paths = 0;
	long chords = 0;
	long blocked = 0;
	long qChords = 0;
	long qBlocked = 0;

	for (int q = 0; q < 3000 && paths < 200; ++q)
	{
		const int cheb = (q % 3 == 0) ? 64 : (q % 3 == 1 ? 150 : 40);
		const NavCell a = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		const NavCell b = RandomNear(walk, rng, a, cheb);
		if (a.x == b.x && a.z == b.z)
			continue;

		BotCore::NavPathResult path;
		finder.Find(grid, a, b, search, path);
		if (path.status != BotCore::NavPathStatus::Found)
			continue;
		BotCore::NavSmoothResult out;
		BotCore::NavSmoothPath(grid, path.cells, smooth, out);
		const std::vector<NavCell> & way = out.waypoints;
		if (way.size() < 2)
			continue;
		++paths;

		// Split the smoothed polyline into 6.75 m packet chords (unquantised, as the audit does).
		double curx = grid.CellCenter(way[0].x);
		double curz = grid.CellCenter(way[0].z);
		for (size_t i = 1; i < way.size(); ++i)
		{
			const double tx = grid.CellCenter(way[i].x);
			const double tz = grid.CellCenter(way[i].z);
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
				const ChordResult r = BotCore::CheckMoveChord(&grid,
					(float)curx, (float)curz, (float)nx, (float)nz);
				if (r.verdict != ChordVerdict::Ok)
					++blocked;

				// Informational only (no CHECK): the executor sends quantised positions.
				++qChords;
				const ChordResult qr = BotCore::CheckMoveChord(&grid,
					Quantise(curx), Quantise(curz), Quantise(nx), Quantise(nz));
				if (qr.verdict != ChordVerdict::Ok)
					++qBlocked;

				curx = nx;
				curz = nz;
				if (last)
					break;
			}
		}
	}

	std::printf("NAVCHORD planner chords=%ld blocked=%ld\n", chords, blocked);
	std::printf("NAVCHORD planner quantised chords=%ld blocked=%ld\n", qChords, qBlocked);
	CHECK(paths >= 200);
	CHECK(chords >= 1000);
	CHECK_EQ(blocked, 0);
}

TEST_CASE("NavChord_Perf")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "perf"))
		return;

	std::vector<NavCell> walk;
	CollectWalk(grid, walk);
	REQUIRE(!walk.empty());

	BotCore::Rng rng(20261002u);
	const int total = 20000;
	std::vector<double> ms;
	ms.reserve((size_t)total);

	for (int q = 0; q < total; ++q)
	{
		const NavCell a = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		const NavCell b = RandomNear(walk, rng, a, 3);
		const float ax = grid.CellCenter(a.x);
		const float az = grid.CellCenter(a.z);
		const float bx = grid.CellCenter(b.x);
		const float bz = grid.CellCenter(b.z);

		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		const ChordResult r = BotCore::CheckMoveChord(&grid, ax, az, bx, bz);
		const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
		(void)r;
		ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
	}

	std::sort(ms.begin(), ms.end());
	const double p95 = PercentileDouble(ms, 0.95);
	std::printf("NAVCHORD perf: chords=%d ms_p95=%.6f\n", (int)ms.size(), p95);

#ifndef _DEBUG
	CHECK(p95 <= 0.02);
#else
	(void)p95;
#endif
}
