#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavSmooth.h>
#include <BotCore/NavTrack.h>
#include <BotCore/NavLos.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace
{
	using BotCore::NavCell;
	using BotCore::NavGrid;
	using BotCore::NavLosMode;
	using BotCore::NavLosParams;

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

	double PercentileDouble(const std::vector<double> & sorted, double p)
	{
		if (sorted.empty())
			return 0.0;
		size_t idx = (size_t)(p * (double)(sorted.size() - 1));
		if (idx >= sorted.size())
			idx = sorted.size() - 1;
		return sorted[idx];
	}

	// RingEvents plus the given cells blocked.
	std::vector<int16_t> BlockedEvents(const std::vector<NavCell> & blocked)
	{
		std::vector<int16_t> events = RingEvents(40);
		for (size_t i = 0; i < blocked.size(); ++i)
			events[CellIndex(40, blocked[i].x, blocked[i].z)] = 0;
		return events;
	}

	// RingEvents plus the column x = 20 blocked for z = zLo..zHi.
	std::vector<int16_t> WallEvents(int zLo, int zHi)
	{
		std::vector<int16_t> events = RingEvents(40);
		for (int z = zLo; z <= zHi; ++z)
			events[CellIndex(40, 20, z)] = 0;
		return events;
	}

	// 40x40 vertex heights: the columns x = 20 and x = 21 (every z) are h, the rest 0. The
	// plateau is the world band x in [80, 84], with ramps on [76, 80] and [84, 88].
	std::vector<float> HillHeights(float h)
	{
		std::vector<float> heights((size_t)40 * 40, 0.0f);
		for (int x = 20; x <= 21; ++x)
		{
			for (int z = 0; z < 40; ++z)
				heights[CellIndex(40, x, z)] = h;
		}
		return heights;
	}

	void GridBothWays(const NavGrid & grid, float ax, float az, float bx, float bz, bool expected)
	{
		CHECK_EQ(BotCore::NavLosGridClear(grid, ax, az, bx, bz), expected);
		CHECK_EQ(BotCore::NavLosGridClear(grid, bx, bz, ax, az), expected);
	}

	void TerrainBothWays(const NavGrid & grid, float ax, float az, float bx, float bz,
		const NavLosParams & params, bool expected)
	{
		CHECK_EQ(BotCore::NavLosTerrainClear(grid, ax, az, bx, bz, params), expected);
		CHECK_EQ(BotCore::NavLosTerrainClear(grid, bx, bz, ax, az, params), expected);
	}

	void ClearBothWays(const NavGrid & grid, float ax, float az, float bx, float bz,
		const NavLosParams & params, bool expected)
	{
		CHECK_EQ(BotCore::NavLosClear(grid, ax, az, bx, bz, params), expected);
		CHECK_EQ(BotCore::NavLosClear(grid, bx, bz, ax, az, params), expected);
	}

	// Independent slab test: true when the segment does not cross the OPEN interior of any blocked
	// cell other than its own start/end cell. Used as the cross-check for NavLosGridClear.
	bool RefClear(const NavGrid & grid, double ax, double az, double bx, double bz)
	{
		const double unit = (double)grid.Unit();
		const int n = grid.Size();
		const int startX = grid.CellOf((float)ax);
		const int startZ = grid.CellOf((float)az);
		const int endX = grid.CellOf((float)bx);
		const int endZ = grid.CellOf((float)bz);
		const double dx = bx - ax;
		const double dz = bz - az;

		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (grid.Event(x, z) == 1)
					continue;
				if ((x == startX && z == startZ) || (x == endX && z == endZ))
					continue;

				const double loX = (double)x * unit;
				const double hiX = (double)(x + 1) * unit;
				const double loZ = (double)z * unit;
				const double hiZ = (double)(z + 1) * unit;

				double t0 = 0.0;
				double t1 = 1.0;

				if (dx == 0.0)
				{
					if (!(ax > loX && ax < hiX))
						continue;
				}
				else
				{
					double a = (loX - ax) / dx;
					double b = (hiX - ax) / dx;
					if (a > b)
					{
						const double tmp = a;
						a = b;
						b = tmp;
					}
					if (a > t0)
						t0 = a;
					if (b < t1)
						t1 = b;
				}

				if (dz == 0.0)
				{
					if (!(az > loZ && az < hiZ))
						continue;
				}
				else
				{
					double a = (loZ - az) / dz;
					double b = (hiZ - az) / dz;
					if (a > b)
					{
						const double tmp = a;
						a = b;
						b = tmp;
					}
					if (a > t0)
						t0 = a;
					if (b < t1)
						t1 = b;
				}

				if (t0 < t1 - 1e-9)
					return false;
			}
		}
		return true;
	}
}

TEST_CASE("NavLos_Grid")
{
	const NavGrid flat = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));
	const NavGrid one = MakeNav(40, 4.0f, BlockedEvents({ Cell(20, 20) }), HeightZeros(40));
	const NavGrid two = MakeNav(40, 4.0f, BlockedEvents({ Cell(20, 21), Cell(21, 20) }), HeightZeros(40));

	// flat
	GridBothWays(flat, 50, 82, 114, 82, true);
	GridBothWays(flat, 82, 50, 82, 114, true);
	GridBothWays(flat, 82, 82, 98, 98, true);
	GridBothWays(flat, 50, 50, 114, 114, true);
	GridBothWays(flat, 82, 82, 82, 82, true);
	GridBothWays(flat, 82, 82, 83, 82.5f, true);
	GridBothWays(flat, 82, 82, -10, 82, false);
	GridBothWays(flat, 82, 82, 82, 700, false);

	// one
	GridBothWays(one, 50, 82, 114, 82, false);
	GridBothWays(one, 50, 86, 114, 86, true);
	GridBothWays(one, 50, 78, 114, 78, true);
	GridBothWays(one, 82, 50, 82, 114, false);
	GridBothWays(one, 50, 50, 114, 114, false);
	GridBothWays(one, 66, 66, 98, 98, false);
	GridBothWays(one, 78, 78, 90, 90, false);
	GridBothWays(one, 76, 76, 84, 84, false);
	GridBothWays(one, 50, 82, 82, 82, true);
	GridBothWays(one, 82, 82, 114, 82, true);
	GridBothWays(one, 82, 82, 98, 98, true);
	GridBothWays(one, 81, 81, 83, 83, true);
	GridBothWays(one, 70, 82, 78, 82, true);
	GridBothWays(one, 76, 92, 92, 76, true);
	GridBothWays(one, 76, 91.99f, 92, 75.99f, false);
	GridBothWays(one, 76, 92.01f, 92, 76.01f, true);
	GridBothWays(one, 50, 80, 114, 80, false);
	GridBothWays(one, 50, 79.99f, 114, 79.99f, true);
	GridBothWays(one, 50, 84, 114, 84, true);
	GridBothWays(one, 50, 83.99f, 114, 83.99f, false);
	GridBothWays(one, 80, 50, 80, 114, false);
	GridBothWays(one, 79.99f, 50, 79.99f, 114, true);
	GridBothWays(one, 84, 50, 84, 114, true);
	GridBothWays(one, 83.99f, 50, 83.99f, 114, false);

	// two
	GridBothWays(two, 76, 92, 92, 76, false);
	GridBothWays(two, 70, 98, 98, 70, false);
	GridBothWays(two, 76, 90, 92, 74, false);
}

TEST_CASE("NavLos_Terrain")
{
	const NavLosParams params;

	const NavGrid hill4 = MakeNav(40, 4.0f, RingEvents(40), HillHeights(4.0f));
	const NavGrid hill19 = MakeNav(40, 4.0f, RingEvents(40), HillHeights(1.9f));
	const NavGrid hill18 = MakeNav(40, 4.0f, RingEvents(40), HillHeights(1.8f));
	const NavGrid hill15 = MakeNav(40, 4.0f, RingEvents(40), HillHeights(1.5f));

	// Table 2 rows; the cell rule is clear here, so terrain and clear agree.
	ClearBothWays(hill4, 50, 82, 114, 82, params, false);
	ClearBothWays(hill19, 50, 82, 114, 82, params, false);
	ClearBothWays(hill18, 50, 82, 114, 82, params, true);
	ClearBothWays(hill15, 50, 82, 114, 82, params, true);

	ClearBothWays(hill4, 82, 82, 114, 82, params, true);
	ClearBothWays(hill19, 82, 82, 114, 82, params, true);
	ClearBothWays(hill18, 82, 82, 114, 82, params, true);
	ClearBothWays(hill15, 82, 82, 114, 82, params, true);

	ClearBothWays(hill4, 50, 82, 82, 82, params, true);
	ClearBothWays(hill19, 50, 82, 82, 82, params, true);
	ClearBothWays(hill18, 50, 82, 82, 82, params, true);
	ClearBothWays(hill15, 50, 82, 82, 82, params, true);

	ClearBothWays(hill4, 76, 82, 88, 82, params, false);
	ClearBothWays(hill19, 76, 82, 88, 82, params, false);
	ClearBothWays(hill18, 76, 82, 88, 82, params, true);
	ClearBothWays(hill15, 76, 82, 88, 82, params, true);

	ClearBothWays(hill4, 70, 82, 94, 82, params, false);
	ClearBothWays(hill19, 70, 82, 94, 82, params, false);
	ClearBothWays(hill18, 70, 82, 94, 82, params, true);
	ClearBothWays(hill15, 70, 82, 94, 82, params, true);

	ClearBothWays(hill4, 84, 82, 100, 82, params, true);
	ClearBothWays(hill19, 84, 82, 100, 82, params, true);
	ClearBothWays(hill18, 84, 82, 100, 82, params, true);
	ClearBothWays(hill15, 84, 82, 100, 82, params, true);

	// Every row must also agree with the composition of the two rules.
	TerrainBothWays(hill4, 50, 82, 114, 82, params, false);
	TerrainBothWays(hill18, 50, 82, 114, 82, params, true);

	// Parameter changes on hill(1.9), (50, 82) -> (114, 82): only the terrain rule.
	{
		NavLosParams p;
		CHECK_EQ(BotCore::NavLosTerrainClear(hill19, 50, 82, 114, 82, p), false);
		CHECK_EQ(BotCore::NavLosClear(hill19, 50, 82, 114, 82, p), false);
		CHECK_EQ(BotCore::NavLosGridClear(hill19, 50, 82, 114, 82), true);
	}
	{
		NavLosParams p;
		p.eyeM = 3.0f;
		CHECK_EQ(BotCore::NavLosTerrainClear(hill19, 50, 82, 114, 82, p), true);
	}
	{
		NavLosParams p;
		p.terrainSlackM = 0.5f;
		CHECK_EQ(BotCore::NavLosTerrainClear(hill19, 50, 82, 114, 82, p), true);
	}
	{
		NavLosParams p;
		p.terrain = false;
		CHECK_EQ(BotCore::NavLosTerrainClear(hill19, 50, 82, 114, 82, p), true);
	}
	{
		NavLosParams p;
		p.sampleM = 0.0f;
		CHECK_EQ(BotCore::NavLosTerrainClear(hill19, 50, 82, 114, 82, p), true);
	}
	{
		NavLosParams p;
		p.eyeM = 0.0f;
		CHECK_EQ(BotCore::NavLosTerrainClear(hill19, 50, 82, 114, 82, p), false);
	}
}

TEST_CASE("NavLos_Mode")
{
	const NavLosParams p;
	CHECK(p.mode == NavLosMode::Advisory);
	CHECK(p.terrain == true);
	CHECK(p.eyeM == 1.6f);
	CHECK(p.terrainSlackM == 0.25f);
	CHECK(p.sampleM == 2.0f);

	CHECK_EQ(BotCore::NavLosAllows(NavLosMode::Advisory, false), true);
	CHECK_EQ(BotCore::NavLosAllows(NavLosMode::Advisory, true), true);
	CHECK_EQ(BotCore::NavLosAllows(NavLosMode::Enforce, true), true);
	CHECK_EQ(BotCore::NavLosAllows(NavLosMode::Enforce, false), false);
}

TEST_CASE("NavLos_PickCell")
{
	const NavLosParams params;
	const NavGrid wall = MakeNav(40, 4.0f, WallEvents(10, 30), HeightZeros(40));
	const NavGrid split = MakeNav(40, 4.0f, WallEvents(1, 38), HeightZeros(40));
	const NavGrid flat = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));
	const NavGrid hill4 = MakeNav(40, 4.0f, RingEvents(40), HillHeights(4.0f));

	std::vector<NavCell> scratch;
	NavCell out;

	// (a)
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(wall, 98, 82, 62, 82, 0.0f, 24.0f, params, scratch, out), true);
	CHECK(out == Cell(21, 20));
	CHECK_EQ((int)scratch.size(), 104);

	// (b)
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(wall, 98, 82, 62, 82, 0.0f, 40.0f, params, scratch, out), true);
	CHECK(out == Cell(21, 20));
	CHECK_EQ((int)scratch.size(), 298);

	// (c)
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(wall, 98, 82, 98, 82, 0.0f, 24.0f, params, scratch, out), true);
	CHECK(out == Cell(24, 20));

	// (d)
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(wall, 62, 82, 62, 82, 0.0f, 24.0f, params, scratch, out), true);
	CHECK(out == Cell(15, 20));

	// (e)
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(wall, 62, 82, 98, 82, 0.0f, 24.0f, params, scratch, out), true);
	CHECK(out == Cell(19, 20));

	// (f)
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(wall, 98, 82, 62, 82, 16.0f, 24.0f, params, scratch, out), true);
	CHECK(out == Cell(21, 17));
	CHECK_EQ((int)scratch.size(), 59);

	// (g)
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(wall, 98, 82, 62, 82, 0.0f, 10.0f, params, scratch, out), true);
	CHECK(out == Cell(22, 20));
	CHECK_EQ((int)scratch.size(), 21);

	// (h): target on the far side of the split map; out must stay untouched.
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(split, 98, 82, 62, 82, 0.0f, 40.0f, params, scratch, out), false);
	CHECK_EQ((int)scratch.size(), 72);
	CHECK(out == Cell(-7, -7));

	// (i)
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(split, 98, 82, 62, 82, 0.0f, 200.0f, params, scratch, out), false);
	CHECK_EQ((int)scratch.size(), 722);

	// (j): no Walk candidate at all.
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(flat, 2, 2, 62, 82, 0.0f, 4.0f, params, scratch, out), false);
	CHECK_EQ((int)scratch.size(), 0);

	// (k)
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(hill4, 98, 82, 62, 82, 0.0f, 24.0f, params, scratch, out), true);
	CHECK(out == Cell(20, 20));
	CHECK_EQ((int)scratch.size(), 113);

	// (l): terrain rule off -> the nearest candidate wins.
	{
		NavLosParams p;
		p.terrain = false;
		out = Cell(-7, -7);
		CHECK_EQ(BotCore::NavPickLosCell(hill4, 98, 82, 62, 82, 0.0f, 24.0f, p, scratch, out), true);
		CHECK(out == Cell(18, 20));
	}

	// (m)
	out = Cell(-7, -7);
	CHECK_EQ(BotCore::NavPickLosCell(hill4, 98, 82, 62, 82, 8.0f, 24.0f, params, scratch, out), true);
	CHECK(out == Cell(20, 20));
	CHECK_EQ((int)scratch.size(), 104);
}

TEST_CASE("NavLos_Reference")
{
	const int n = 40;
	std::vector<int16_t> events = RingEvents(n);
	std::mt19937 rng(12345);
	auto U = [&]()
	{
		return (float)(rng() >> 8) * (1.0f / 16777216.0f);
	};

	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			if (x == 0 || x == n - 1 || z == 0 || z == n - 1)
				continue;
			if (U() < 0.08f)
				events[CellIndex(n, x, z)] = 0;
		}
	}
	const NavGrid grid = MakeNav(n, 4.0f, events, HeightZeros(n));

	int clearCount = 0;
	int mismatches = 0;
	int asym = 0;
	for (int q = 0; q < 5000; ++q)
	{
		const float ax = 24 + U() * 112;
		const float az = 24 + U() * 112;
		const float bx = ax - 24 + U() * 48;
		const float bz = az - 24 + U() * 48;

		const bool got = BotCore::NavLosGridClear(grid, ax, az, bx, bz);
		const bool ref = RefClear(grid, (double)ax, (double)az, (double)bx, (double)bz);
		if (got != ref)
			++mismatches;
		if (got != BotCore::NavLosGridClear(grid, bx, bz, ax, az))
			++asym;
		if (got)
			++clearCount;
	}

	std::printf("NAVLOS ref: queries=5000 clear=%d mismatches=%d asym=%d\n", clearCount, mismatches, asym);
	CHECK_EQ(mismatches, 0);
	CHECK_EQ(asym, 0);
	CHECK(clearCount >= 3000 && clearCount <= 3500);
}

TEST_CASE("NavLos_RealMap")
{
	NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVLOS real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	const NavLosParams params;
	const int n = grid.Size();
	const int offsets[5][2] = { { 5, 0 }, { 0, 8 }, { 7, 7 }, { -12, 5 }, { 15, -10 } };

	int walk = 0;
	int tested[5] = { 0, 0, 0, 0, 0 };
	int gridClear[5] = { 0, 0, 0, 0, 0 };
	int clearTerrain[5] = { 0, 0, 0, 0, 0 };
	int skipped = 0;
	int asym = 0;
	int violations = 0;
	int reposition = 0;
	int pickFound = 0;
	int pickViolations = 0;
	std::vector<double> clearDurations;
	std::vector<double> pickDurations;
	std::vector<NavCell> scratch;
	NavCell out;

	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			if (!grid.Walk(x, z))
				continue;
			++walk;
			if (((x * 7 + z * 13) % 5) != 0)
				continue;

			const float sx = grid.CellCenter(x);
			const float sz = grid.CellCenter(z);

			for (int o = 0; o < 5; ++o)
			{
				const int tx = x + offsets[o][0];
				const int tz = z + offsets[o][1];
				if (!grid.Walk(tx, tz))
				{
					++skipped;
					continue;
				}

				const float bx = grid.CellCenter(tx);
				const float bz = grid.CellCenter(tz);

				const bool gF = BotCore::NavLosGridClear(grid, sx, sz, bx, bz);
				const bool gR = BotCore::NavLosGridClear(grid, bx, bz, sx, sz);
				const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
				const bool cF = BotCore::NavLosClear(grid, sx, sz, bx, bz, params);
				const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
				const bool cR = BotCore::NavLosClear(grid, bx, bz, sx, sz, params);
				clearDurations.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

				++tested[o];
				if (gF)
					++gridClear[o];
				if (cF)
					++clearTerrain[o];
				if (gF != gR)
					++asym;
				if (cF != cR)
					++asym;
				if (cF && !gF)
					++violations;
				if (cF != (gF && BotCore::NavLosTerrainClear(grid, sx, sz, bx, bz, params)))
					++violations;

				if (o == 2 && !cF)
				{
					++reposition;
					const std::chrono::steady_clock::time_point p0 = std::chrono::steady_clock::now();
					const bool found = BotCore::NavPickLosCell(grid, bx, bz, sx, sz, 0.0f, 30.0f,
						params, scratch, out);
					const std::chrono::steady_clock::time_point p1 = std::chrono::steady_clock::now();
					pickDurations.push_back(std::chrono::duration<double, std::milli>(p1 - p0).count());

					if (found)
					{
						++pickFound;
						bool ok = grid.Walk(out.x, out.z);
						const float odx = grid.CellCenter(out.x) - bx;
						const float odz = grid.CellCenter(out.z) - bz;
						ok = ok && (odx * odx + odz * odz <= 30.0f * 30.0f);
						ok = ok && BotCore::NavLosClear(grid, grid.CellCenter(out.x), grid.CellCenter(out.z),
							bx, bz, params);
						if (!ok)
							++pickViolations;
					}
				}
			}
		}
	}

	std::sort(clearDurations.begin(), clearDurations.end());
	std::sort(pickDurations.begin(), pickDurations.end());
	const double clearP95 = PercentileDouble(clearDurations, 0.95);
	const double pickP95 = PercentileDouble(pickDurations, 0.95);

	const int totalTested = tested[0] + tested[1] + tested[2] + tested[3] + tested[4];
	const int totalGrid = gridClear[0] + gridClear[1] + gridClear[2] + gridClear[3] + gridClear[4];
	const int totalTerrain = clearTerrain[0] + clearTerrain[1] + clearTerrain[2] + clearTerrain[3] + clearTerrain[4];

	for (int o = 0; o < 5; ++o)
	{
		std::printf("NAVLOS real off(%d,%d): tested=%d grid=%d grid_terrain=%d\n",
			offsets[o][0], offsets[o][1], tested[o], gridClear[o], clearTerrain[o]);
	}
	std::printf("NAVLOS real: walk=%d tested=%d grid=%d grid_terrain=%d skipped=%d asym=%d violations=%d ms_p95=%.4f\n",
		walk, totalTested, totalGrid, totalTerrain, skipped, asym, violations, clearP95);
	std::printf("NAVLOS real pick: reposition=%d found=%d violations=%d ms_p95=%.4f\n",
		reposition, pickFound, pickViolations, pickP95);

	CHECK_EQ(walk, 88508);
	CHECK_EQ(totalTested, 64559);
	CHECK_EQ(totalGrid, 47245);
	CHECK_EQ(totalTerrain, 43163);
	CHECK_EQ(skipped, 23966);
	CHECK_EQ(asym, 0);
	CHECK_EQ(violations, 0);
	CHECK_EQ(reposition, 3935);
	CHECK_EQ(pickFound, 3935);
	CHECK_EQ(pickViolations, 0);
	CHECK_EQ(tested[0], 14380);
	CHECK_EQ(gridClear[0], 13230);
	CHECK_EQ(clearTerrain[0], 12874);
	CHECK_EQ(tested[1], 13389);
	CHECK_EQ(gridClear[1], 10962);
	CHECK_EQ(clearTerrain[1], 10257);
	CHECK_EQ(tested[2], 12718);
	CHECK_EQ(gridClear[2], 9702);
	CHECK_EQ(clearTerrain[2], 8783);
	CHECK_EQ(tested[3], 12433);
	CHECK_EQ(gridClear[3], 7706);
	CHECK_EQ(clearTerrain[3], 6720);
	CHECK_EQ(tested[4], 11639);
	CHECK_EQ(gridClear[4], 5645);
	CHECK_EQ(clearTerrain[4], 4529);
#ifndef _DEBUG
	CHECK(clearP95 <= 0.05);
	CHECK(pickP95 <= 0.5);
#endif
}
