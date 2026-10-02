#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavSegment.h>
#include <BotCore/NavSmooth.h>
#include <BotCore/Rng.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace
{
	using BotCore::NavCell;
	using BotCore::NavGrid;
	using BotCore::NavSegmentVerdict;

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

	NavGrid MakeNav(int n, float unit, const std::vector<int16_t> & events, const std::vector<float> & heights)
	{
		NavGrid grid;
		if (!grid.Init(n, unit, events, heights))
			CHECK(false);
		grid.Build();
		return grid;
	}

	double CenterX(const NavGrid & grid, int i)
	{
		return (double)grid.CellCenter(i);
	}

	NavSegmentVerdict Check(const NavGrid & grid, double ax, double az, double bx, double bz, bool checkSlope = false)
	{
		return BotCore::NavCheckSegment(grid, ax, az, bx, bz, checkSlope).verdict;
	}

	bool CellBlocked(const NavGrid & grid, int x, int z)
	{
		return !grid.Walk(x, z);
	}

	// Coarse brute force oracle: samples the segment every 0.001 m and looks at the containing
	// cell's closed square (a boundary sample also checks the far side). True when a blocked cell
	// is hit. It is independent of the traversal under test.
	bool OracleBlocked(const NavGrid & grid, double ax, double az, double bx, double bz)
	{
		const double unit = (double)grid.Unit();
		const double dx = bx - ax;
		const double dz = bz - az;
		const double len = std::sqrt(dx * dx + dz * dz);
		int steps = (int)std::ceil(len / 0.001);
		if (steps < 1)
			steps = 1;

		for (int s = 0; s <= steps; ++s)
		{
			const double t = (double)s / (double)steps;
			const double fx = (ax + dx * t) / unit;
			const double fz = (az + dz * t) / unit;
			const int cx = (int)std::floor(fx);
			const int cz = (int)std::floor(fz);
			const double rfx = fx - std::floor(fx);
			const double rfz = fz - std::floor(fz);

			if (CellBlocked(grid, cx, cz))
				return true;
			if (rfx < 1e-9 && CellBlocked(grid, cx - 1, cz))
				return true;
			if (rfx > 1.0 - 1e-9 && CellBlocked(grid, cx + 1, cz))
				return true;
			if (rfz < 1e-9 && CellBlocked(grid, cx, cz - 1))
				return true;
			if (rfz > 1.0 - 1e-9 && CellBlocked(grid, cx, cz + 1))
				return true;
		}
		return false;
	}

	// Liang-Barsky: true when the segment p0->p1 meets the (optionally inflated) rectangle.
	bool SegmentMeetsRect(double ax, double az, double bx, double bz,
		double xmin, double xmax, double zmin, double zmax)
	{
		double t0 = 0.0;
		double t1 = 1.0;
		const double dx = bx - ax;
		const double dz = bz - az;
		const double p[4] = { -dx, dx, -dz, dz };
		const double q[4] = { ax - xmin, xmax - ax, az - zmin, zmax - az };

		for (int i = 0; i < 4; ++i)
		{
			if (p[i] == 0.0)
			{
				if (q[i] < 0.0)
					return false;
			}
			else
			{
				const double r = q[i] / p[i];
				if (p[i] < 0.0)
				{
					if (r > t1)
						return false;
					if (r > t0)
						t0 = r;
				}
				else
				{
					if (r < t0)
						return false;
					if (r < t1)
						t1 = r;
				}
			}
		}
		return t0 <= t1;
	}

	// True when the segment comes within `tol` metres of the cell's closed square.
	bool SegmentNearCell(const NavGrid & grid, double ax, double az, double bx, double bz, int cx, int cz, double tol)
	{
		const double unit = (double)grid.Unit();
		return SegmentMeetsRect(ax, az, bx, bz,
			(double)cx * unit - tol, (double)(cx + 1) * unit + tol,
			(double)cz * unit - tol, (double)(cz + 1) * unit + tol);
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

TEST_CASE("NavSegment_Basic")
{
	const int n = 12;
	const float unit = 4.0f;
	NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));

	// Open interior lines are Ok.
	CHECK(Check(grid, CenterX(grid, 2), CenterX(grid, 2), CenterX(grid, 9), CenterX(grid, 3)) == NavSegmentVerdict::Ok);
	CHECK(Check(grid, CenterX(grid, 3), CenterX(grid, 2), CenterX(grid, 3), CenterX(grid, 9)) == NavSegmentVerdict::Ok);
	CHECK(Check(grid, CenterX(grid, 2), CenterX(grid, 2), CenterX(grid, 8), CenterX(grid, 8)) == NavSegmentVerdict::Ok);

	// A single blocked cell in the middle is reported.
	std::vector<int16_t> events = RingEvents(n);
	events[CellIndex(n, 6, 5)] = 0;
	NavGrid blocked = MakeNav(n, unit, events, HeightZeros(n));
	BotCore::NavSegmentResult r = BotCore::NavCheckSegment(blocked,
		CenterX(blocked, 5), CenterX(blocked, 5), CenterX(blocked, 7), CenterX(blocked, 5));
	CHECK(r.verdict == NavSegmentVerdict::BlockedCell);
	CHECK_EQ(r.cellX, 6);
	CHECK_EQ(r.cellZ, 5);

	// A blocked cell outside the chord does not matter.
	std::vector<int16_t> elsewhere = RingEvents(n);
	elsewhere[CellIndex(n, 6, 3)] = 0;
	NavGrid other = MakeNav(n, unit, elsewhere, HeightZeros(n));
	CHECK(Check(other, CenterX(other, 5), CenterX(other, 5), CenterX(other, 7), CenterX(other, 5)) == NavSegmentVerdict::Ok);

	// Leaving the grid. The outer ring is never Walk (NavGrid::Build drops edge components), so a
	// chord through it stops at the blocked border cell; an endpoint outside the grid is OutOfBounds.
	CHECK(Check(grid, CenterX(grid, 5), CenterX(grid, 5), 60.0, CenterX(grid, 5)) == NavSegmentVerdict::BlockedCell);
	CHECK(Check(grid, -1.0, CenterX(grid, 5), CenterX(grid, 5), CenterX(grid, 5)) == NavSegmentVerdict::OutOfBounds);
	CHECK(Check(grid, 60.0, CenterX(grid, 5), 64.0, CenterX(grid, 5)) == NavSegmentVerdict::OutOfBounds);

	// Zero length: only the start cell matters.
	CHECK(Check(grid, CenterX(grid, 5), CenterX(grid, 5), CenterX(grid, 5), CenterX(grid, 5)) == NavSegmentVerdict::Ok);
	CHECK(Check(blocked, CenterX(blocked, 6), CenterX(blocked, 5), CenterX(blocked, 6), CenterX(blocked, 5)) == NavSegmentVerdict::BlockedCell);

	// Non-finite and absurdly large coordinates are rejected as OutOfBounds before any conversion.
	const double nan = std::numeric_limits<double>::quiet_NaN();
	const double inf = std::numeric_limits<double>::infinity();
	CHECK(Check(grid, nan, CenterX(grid, 5), CenterX(grid, 5), CenterX(grid, 5)) == NavSegmentVerdict::OutOfBounds);
	CHECK(Check(grid, CenterX(grid, 5), inf, CenterX(grid, 5), CenterX(grid, 5)) == NavSegmentVerdict::OutOfBounds);
	CHECK(Check(grid, 1e12, CenterX(grid, 5), CenterX(grid, 5), CenterX(grid, 5)) == NavSegmentVerdict::OutOfBounds);
}

TEST_CASE("NavSegment_Corner")
{
	const int n = 12;
	const float unit = 4.0f;

	// 45 degrees through a vertex: all four cells are touched, a blocked orthogonal one is caught.
	std::vector<int16_t> events = RingEvents(n);
	events[CellIndex(n, 5, 4)] = 0;   // orthogonal cell at the vertex of the (4,4)->(5,5) chord
	NavGrid corner = MakeNav(n, unit, events, HeightZeros(n));
	BotCore::NavSegmentResult r = BotCore::NavCheckSegment(corner,
		CenterX(corner, 4), CenterX(corner, 4), CenterX(corner, 5), CenterX(corner, 5));
	CHECK(r.verdict == NavSegmentVerdict::BlockedCell);
	CHECK_EQ(r.cellX, 5);
	CHECK_EQ(r.cellZ, 4);

	// Same chord with the four cells open is Ok.
	NavGrid open = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	CHECK(Check(open, CenterX(open, 4), CenterX(open, 4), CenterX(open, 5), CenterX(open, 5)) == NavSegmentVerdict::Ok);

	// A chord lying exactly on the blocked cell's edge touches the closed square: BlockedCell.
	std::vector<int16_t> edge = RingEvents(n);
	edge[CellIndex(n, 5, 5)] = 0;
	NavGrid edgeGrid = MakeNav(n, unit, edge, HeightZeros(n));
	CHECK(Check(edgeGrid, 20.0, 18.0, 20.0, 26.0) == NavSegmentVerdict::BlockedCell);

	// 1e-4 m on the open side of the edge is Ok (the blocked square is not met).
	CHECK(Check(edgeGrid, 20.0 - 1e-4, 18.0, 20.0 - 1e-4, 26.0) == NavSegmentVerdict::Ok);
}

TEST_CASE("NavSegment_Slope")
{
	const int n = 20;
	const float unit = 4.0f;

	// Edge step exactly at the slope limit: maxSlope * unit = 2.5 m.
	std::vector<float> atLimit((size_t)n * (size_t)n, 0.0f);
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
			atLimit[CellIndex(n, x, z)] = (x >= 10) ? 2.5f : 0.0f;
	}
	NavGrid limitGrid = MakeNav(n, unit, RingEvents(n), atLimit);
	CHECK(Check(limitGrid, CenterX(limitGrid, 8), CenterX(limitGrid, 5), CenterX(limitGrid, 12), CenterX(limitGrid, 5), true) == NavSegmentVerdict::Ok);

	// Just above the limit: SlopeTooSteep, reported at the entered cell.
	std::vector<float> above((size_t)n * (size_t)n, 0.0f);
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
			above[CellIndex(n, x, z)] = (x >= 10) ? 2.51f : 0.0f;
	}
	NavGrid aboveGrid = MakeNav(n, unit, RingEvents(n), above);
	BotCore::NavSegmentResult r = BotCore::NavCheckSegment(aboveGrid,
		CenterX(aboveGrid, 8), CenterX(aboveGrid, 5), CenterX(aboveGrid, 12), CenterX(aboveGrid, 5), true);
	CHECK(r.verdict == NavSegmentVerdict::SlopeTooSteep);
	CHECK_EQ(r.cellX, 10);
	CHECK_EQ(r.cellZ, 5);

	// The slope layer is optional and defaults off: the same steep chord only checks Walk.
	CHECK(Check(aboveGrid, CenterX(aboveGrid, 8), CenterX(aboveGrid, 5), CenterX(aboveGrid, 12), CenterX(aboveGrid, 5)) == NavSegmentVerdict::Ok);

	// Vertex (diagonal) step uses the unit*sqrt(2) scale: limit = 0.625 * 4 * sqrt(2) = 3.5355 m.
	const int m = 12;
	std::vector<float> diag((size_t)m * (size_t)m, 0.0f);
	diag[CellIndex(m, 6, 6)] = 3.5f;
	NavGrid diagOk = MakeNav(m, unit, RingEvents(m), diag);
	CHECK(Check(diagOk, CenterX(diagOk, 5), CenterX(diagOk, 5), CenterX(diagOk, 6), CenterX(diagOk, 6), true) == NavSegmentVerdict::Ok);

	std::vector<float> diagBad((size_t)m * (size_t)m, 0.0f);
	diagBad[CellIndex(m, 6, 6)] = 3.6f;
	NavGrid diagSteep = MakeNav(m, unit, RingEvents(m), diagBad);
	BotCore::NavSegmentResult d = BotCore::NavCheckSegment(diagSteep,
		CenterX(diagSteep, 5), CenterX(diagSteep, 5), CenterX(diagSteep, 6), CenterX(diagSteep, 6), true);
	CHECK(d.verdict == NavSegmentVerdict::SlopeTooSteep);
	CHECK_EQ(d.cellX, 6);
	CHECK_EQ(d.cellZ, 6);
}

TEST_CASE("NavSegment_Symmetry_Oracle")
{
	const int n = 64;
	const float unit = 1.0f;

	BotCore::Rng gridRng(20261002u);
	std::vector<int16_t> events((size_t)n * (size_t)n, 1);
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			if (x == 0 || x == n - 1 || z == 0 || z == n - 1)
				events[CellIndex(n, x, z)] = 0;
			else if (gridRng.NextBelow(100) < 20)
				events[CellIndex(n, x, z)] = 0;
		}
	}
	NavGrid grid = MakeNav(n, unit, events, HeightZeros(n));

	BotCore::Rng rng(20261005u);
	const int total = 3000;
	int symViolations = 0;
	int safetyViolations = 0;
	int thinGraze = 0;
	int excess = 0;
	int blockedCount = 0;

	for (int q = 0; q < total; ++q)
	{
		const double ax = 1.0 + rng.NextDouble() * (double)(n - 2);
		const double az = 1.0 + rng.NextDouble() * (double)(n - 2);
		const double bx = 1.0 + rng.NextDouble() * (double)(n - 2);
		const double bz = 1.0 + rng.NextDouble() * (double)(n - 2);

		BotCore::NavSegmentResult ab = BotCore::NavCheckSegment(grid, ax, az, bx, bz);
		BotCore::NavSegmentResult ba = BotCore::NavCheckSegment(grid, bx, bz, ax, az);
		if (ab.verdict != ba.verdict)
			++symViolations;

		const bool oracle = OracleBlocked(grid, ax, az, bx, bz);
		if (ab.verdict == NavSegmentVerdict::Ok && oracle)
			++safetyViolations;
		if (ab.verdict == NavSegmentVerdict::BlockedCell)
		{
			++blockedCount;
			if (!oracle)
			{
				if (SegmentNearCell(grid, ax, az, bx, bz, ab.cellX, ab.cellZ, 0.002))
					++thinGraze;
				else
					++excess;
			}
		}
	}

	std::printf("NAVSEG oracle: chords=%d blocked=%d sym=%d safety=%d graze=%d excess=%d\n",
		total, blockedCount, symViolations, safetyViolations, thinGraze, excess);
	CHECK_EQ(symViolations, 0);
	CHECK_EQ(safetyViolations, 0);
	CHECK_EQ(excess, 0);
	CHECK((thinGraze * 1000) <= total);   // conservative decisions stay at most 0.1%
}

TEST_CASE("NavSegment_RealMap_Planner")
{
	NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVSEG planner: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	const int n = grid.Size();
	std::vector<NavCell> cells;
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

	const int queries = 1000;
	BotCore::Rng rng(20261002u);
	std::vector<NavCell> starts((size_t)queries);
	std::vector<NavCell> goals((size_t)queries);
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

	const double step = 6.75;
	int paths = 0;
	int segments = 0;
	int segmentBad = 0;
	int chords = 0;
	int chordViolations = 0;
	int chordSlopeOpt = 0;

	for (int q = 0; q < queries; ++q)
	{
		finder.Find(grid, starts[(size_t)q], goals[(size_t)q], search, path);
		if (path.status != BotCore::NavPathStatus::Found)
			continue;
		BotCore::NavSmoothPath(grid, path.cells, smooth, result);
		if (result.waypoints.size() < 2)
			continue;
		++paths;

		// Planner waypoint segments: these are exactly the chords NavLineClear validated, so with
		// the optional slope layer on (checkSlope = true) the guard must return Ok for every one.
		for (size_t i = 1; i < result.waypoints.size(); ++i)
		{
			const double ax = CenterX(grid, result.waypoints[i - 1].x);
			const double az = CenterX(grid, result.waypoints[i - 1].z);
			const double bx = CenterX(grid, result.waypoints[i].x);
			const double bz = CenterX(grid, result.waypoints[i].z);
			++segments;
			if (Check(grid, ax, az, bx, bz, true) != NavSegmentVerdict::Ok)
				++segmentBad;
		}

		// 6.75 m packet chords along the smoothed polyline, checked with the mandatory Walk-only
		// supercover (slope off): the hard guarantee is that no chord meets a blocked cell, so
		// every default verdict must be Ok. Separately count how many of the same chords the
		// optional slope layer would refuse (information only; a sub-chord has no planner context).
		double curx = CenterX(grid, result.waypoints[0].x);
		double curz = CenterX(grid, result.waypoints[0].z);
		for (size_t i = 1; i < result.waypoints.size(); ++i)
		{
			const double tx = CenterX(grid, result.waypoints[i].x);
			const double tz = CenterX(grid, result.waypoints[i].z);
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
				if (Check(grid, curx, curz, nx, nz) != NavSegmentVerdict::Ok)
					++chordViolations;
				if (Check(grid, curx, curz, nx, nz, true) == NavSegmentVerdict::SlopeTooSteep)
					++chordSlopeOpt;
				curx = nx;
				curz = nz;
				if (last)
					break;
			}
		}
	}

	std::printf("NAVSEG planner: paths=%d segments=%d segment_bad=%d chords=%d chord_violations=%d chord_slope_opt=%d\n",
		paths, segments, segmentBad, chords, chordViolations, chordSlopeOpt);
	CHECK(paths > 0);
	CHECK_EQ(segmentBad, 0);
	CHECK_EQ(chordViolations, 0);
}

TEST_CASE("NavSegment_RealMap_Straight")
{
	NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVSEG straight: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	const int n = grid.Size();
	std::vector<NavCell> cells;
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

	BotCore::Rng rng(20261006u);
	const int total = 2000;
	int blockedCount = 0;
	int symViolations = 0;
	int safetyViolations = 0;
	int thinGraze = 0;
	int excess = 0;

	for (int q = 0; q < total; ++q)
	{
		NavCell a = cells[(size_t)rng.NextBelow((uint32_t)cells.size())];
		NavCell b = a;
		for (int tries = 0; tries < 64; ++tries)
		{
			b = cells[(size_t)rng.NextBelow((uint32_t)cells.size())];
			if (a == b)
				continue;
			const int dx = std::abs(a.x - b.x);
			const int dz = std::abs(a.z - b.z);
			if (std::max(dx, dz) <= 8)
				break;
		}

		const double ax = CenterX(grid, a.x);
		const double az = CenterX(grid, a.z);
		const double bx = CenterX(grid, b.x);
		const double bz = CenterX(grid, b.z);

		BotCore::NavSegmentResult ab = BotCore::NavCheckSegment(grid, ax, az, bx, bz);
		BotCore::NavSegmentResult ba = BotCore::NavCheckSegment(grid, bx, bz, ax, az);
		if (ab.verdict != ba.verdict)
			++symViolations;

		const bool oracle = OracleBlocked(grid, ax, az, bx, bz);
		if (ab.verdict == NavSegmentVerdict::Ok && oracle)
			++safetyViolations;
		if (ab.verdict == NavSegmentVerdict::BlockedCell)
		{
			++blockedCount;
			if (!oracle)
			{
				if (SegmentNearCell(grid, ax, az, bx, bz, ab.cellX, ab.cellZ, 0.002))
					++thinGraze;
				else
					++excess;
			}
		}
	}

	std::printf("NAVSEG straight: chords=%d blocked=%d sym=%d safety=%d graze=%d excess=%d\n",
		total, blockedCount, symViolations, safetyViolations, thinGraze, excess);
	CHECK_EQ(symViolations, 0);
	CHECK_EQ(safetyViolations, 0);
	CHECK_EQ(excess, 0);
	CHECK((thinGraze * 1000) <= total);
}

TEST_CASE("NavSegment_Perf")
{
	NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVSEG perf: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	const double unit = (double)grid.Unit();
	const double lo = unit;
	const double hi = (double)grid.Size() * unit - unit;

	BotCore::Rng rng(20261007u);
	const int queries = 20000;
	std::vector<double> ms;
	ms.reserve((size_t)queries);
	int outOfBounds = 0;

	// Warm-up, not measured.
	for (int q = 0; q < 100; ++q)
	{
		const double ax = lo + rng.NextDouble() * (hi - lo);
		const double az = lo + rng.NextDouble() * (hi - lo);
		const double ang = rng.NextDouble() * 6.283185307179586;
		BotCore::NavCheckSegment(grid, ax, az, ax + 10.0 * std::cos(ang), az + 10.0 * std::sin(ang));
	}

	for (int q = 0; q < queries; ++q)
	{
		const double ax = lo + rng.NextDouble() * (hi - lo);
		const double az = lo + rng.NextDouble() * (hi - lo);
		const double ang = rng.NextDouble() * 6.283185307179586;
		const double bx = ax + 10.0 * std::cos(ang);
		const double bz = az + 10.0 * std::sin(ang);

		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		BotCore::NavSegmentResult r = BotCore::NavCheckSegment(grid, ax, az, bx, bz);
		const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
		ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
		if (r.verdict == NavSegmentVerdict::OutOfBounds)
			++outOfBounds;
	}

	std::sort(ms.begin(), ms.end());
	const double ms50 = PercentileDouble(ms, 0.50);
	const double ms95 = PercentileDouble(ms, 0.95);
	const double ms99 = PercentileDouble(ms, 0.99);
	std::printf("NAVSEG perf: chords=%d out_of_bounds=%d ms_p50=%.6f ms_p95=%.6f ms_p99=%.6f\n",
		queries, outOfBounds, ms50, ms95, ms99);

#ifndef _DEBUG
	CHECK(ms95 <= 0.02);
#endif
}
