#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/Rng.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{
	size_t CellIndex(int n, int x, int z)
	{
		return (size_t)x * (size_t)n + (size_t)z;
	}

	std::vector<int16_t> Filled(int n, int16_t value)
	{
		return std::vector<int16_t>((size_t)n * (size_t)n, value);
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
}

TEST_CASE("Nav_Init_Validation")
{
	BotCore::NavGrid grid;

	const size_t cells = (size_t)4 * 4;
	std::vector<int16_t> events(cells, 1);
	std::vector<float> heights(cells, 0.0f);

	CHECK(!grid.Init(1, 4.0f, std::vector<int16_t>(), std::vector<float>()));
	CHECK(!grid.Init(4, 0.0f, events, heights));
	CHECK(!grid.Init(4, 4.0f, std::vector<int16_t>(cells - 1, 1), heights));
	CHECK(!grid.Init(4, 4.0f, events, std::vector<float>(cells + 1, 0.0f)));
	CHECK(grid.Init(4, 4.0f, events, heights));
	CHECK_EQ(grid.Size(), 4);
	CHECK_EQ(grid.Unit(), 4.0f);
}

TEST_CASE("Nav_Event_OutOfBounds")
{
	// All-open land: every component touches the edge, so nothing is Walk.
	BotCore::NavGrid grid = MakeNav(8, 4.0f, Filled(8, 1), HeightZeros(8));

	CHECK_EQ(grid.Event(-1, 0), (int16_t)-1);
	CHECK_EQ(grid.Event(0, 8), (int16_t)-1);
	CHECK_EQ(grid.Event(8, 0), (int16_t)-1);
	CHECK(!grid.Walk(-1, 0));
	CHECK(!grid.Walk(0, 8));
	CHECK_EQ(grid.Clearance(-1, 0), (uint8_t)0);
	CHECK_EQ(grid.Clearance(0, 8), (uint8_t)0);
	CHECK(!grid.EdgeOpen(0, 0, -1, 0));
	CHECK(!grid.EdgeOpen(7, 7, 1, 1));
	CHECK_EQ(grid.MainComponentCells(), 0);
}

TEST_CASE("Nav_Walk_MainComponent")
{
	const int n = 12;
	const float unit = 4.0f;

	std::vector<int16_t> events = Filled(n, 1);
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			if (x == 0 || x == n - 1 || z == 0 || z == n - 1)
				events[CellIndex(n, x, z)] = 0;
		}
	}
	// Right region (x 7..10) reaches the edge through the gap (11,5): 41 cells, eliminated.
	events[CellIndex(n, 11, 5)] = 1;
	// Wall x=6 splits the interior; wall z=5 splits the left half.
	for (int z = 1; z <= 10; ++z)
		events[CellIndex(n, 6, z)] = 0;
	for (int x = 1; x <= 5; ++x)
		events[CellIndex(n, x, 5)] = 0;
	// A value other than 0/1 is blocked too.
	events[CellIndex(n, 0, 0)] = 2;

	BotCore::NavGrid grid = MakeNav(n, unit, events, HeightZeros(n));

	CHECK(grid.Walk(2, 7));      // left-bottom region (25 cells) is the main component
	CHECK(!grid.Walk(2, 2));     // left-top pocket (20 cells)
	CHECK(!grid.Walk(10, 5));    // right region, touches the edge
	CHECK(!grid.Walk(11, 5));    // the edge gap itself
	CHECK(!grid.Walk(0, 0));     // event value 2
	CHECK_EQ(grid.Event(0, 0), (int16_t)2);
	CHECK_EQ(grid.MainComponentCells(), 25);

	// No component away from the edge -> no Walk cell at all.
	BotCore::NavGrid none = MakeNav(6, unit, Filled(6, 1), HeightZeros(6));
	CHECK_EQ(none.MainComponentCells(), 0);
}

TEST_CASE("Nav_Clearance_Ring")
{
	const int n = 9;
	std::vector<int16_t> events = RingEvents(n);

	BotCore::NavGrid grid = MakeNav(n, 4.0f, events, HeightZeros(n));

	CHECK_EQ(grid.Clearance(1, 1), (uint8_t)1);
	CHECK_EQ(grid.Clearance(2, 2), (uint8_t)2);
	CHECK_EQ(grid.Clearance(3, 3), (uint8_t)3);
	CHECK_EQ(grid.Clearance(4, 4), (uint8_t)4);
	CHECK_EQ(grid.Clearance(4, 1), (uint8_t)1);
	CHECK_EQ(grid.Clearance(0, 0), (uint8_t)0);
	CHECK(grid.Walk(4, 4));
}

TEST_CASE("Nav_Clearance_BruteForce")
{
	const int n = 24;
	const float unit = 4.0f;
	BotCore::Rng rng(12345);

	std::vector<int16_t> events((size_t)n * n, 1);
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			const size_t i = CellIndex(n, x, z);
			if (x == 0 || x == n - 1 || z == 0 || z == n - 1)
				events[i] = 0;
			else if (rng.NextBelow(100) < 25)
				events[i] = 0;
		}
	}

	BotCore::NavGrid grid = MakeNav(n, unit, events, HeightZeros(n));
	CHECK(grid.MainComponentCells() > 0);

	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			if (!grid.Walk(x, z))
			{
				CHECK_EQ(grid.Clearance(x, z), (uint8_t)0);
				continue;
			}

			int best = 1000000;
			for (int ax = 0; ax < n; ++ax)
			{
				for (int az = 0; az < n; ++az)
				{
					if (grid.Walk(ax, az))
						continue;
					const int d = std::max(std::abs(ax - x), std::abs(az - z));
					if (d < best)
						best = d;
				}
			}
			// Distance to the nearest cell outside the grid (outside counts as blocked).
			const int outside = std::min(std::min(x + 1, z + 1), std::min(n - x, n - z));
			if (outside < best)
				best = outside;

			CHECK_EQ((int)grid.Clearance(x, z), best);
		}
	}
}

TEST_CASE("Nav_Edge_Slope")
{
	const int n = 8;
	const float unit = 4.0f;

	{
		BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
		for (int x = 1; x <= n - 3; ++x)
		{
			for (int z = 1; z <= n - 2; ++z)
				CHECK(grid.EdgeOpen(x, z, 1, 0));
		}
		for (int x = 1; x <= n - 2; ++x)
		{
			for (int z = 1; z <= n - 3; ++z)
				CHECK(grid.EdgeOpen(x, z, 0, 1));
		}
	}

	{
		// |dh| = 3 m over one 4 m step -> slope 0.75, above the 0.625 default.
		std::vector<float> heights((size_t)n * n, 0.0f);
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
				heights[CellIndex(n, x, z)] = 3.0f * (float)x;
		}
		BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), heights);
		CHECK(!grid.EdgeOpen(2, 2, 1, 0));
		CHECK(grid.EdgeOpen(2, 2, 0, 1));

		BotCore::NavParams relaxed;
		relaxed.maxSlope = 0.8f;
		grid.Build(relaxed);
		CHECK(grid.EdgeOpen(2, 2, 1, 0));
	}

	{
		// |dh| = 2.4 m over 4 m -> 0.6, below the default.
		std::vector<float> heights((size_t)n * n, 0.0f);
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
				heights[CellIndex(n, x, z)] = 2.4f * (float)x;
		}
		BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), heights);
		CHECK(grid.EdgeOpen(2, 2, 1, 0));
	}

	{
		// Diagonal 4*sqrt(2) m: the 0.625 limit is 3.5355 m.
		std::vector<float> heights = HeightZeros(n);
		heights[CellIndex(n, 1, 1)] = 3.5f;
		BotCore::NavGrid open = MakeNav(n, unit, RingEvents(n), heights);
		CHECK(open.EdgeOpen(1, 1, 1, 1));

		heights[CellIndex(n, 1, 1)] = 3.6f;
		BotCore::NavGrid closed = MakeNav(n, unit, RingEvents(n), heights);
		CHECK(!closed.EdgeOpen(1, 1, 1, 1));
	}
}

TEST_CASE("Nav_Edge_NoCornerCutting")
{
	const int n = 6;
	const float unit = 4.0f;

	{
		BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
		CHECK(grid.Walk(2, 2));
		CHECK(grid.Walk(3, 3));
		CHECK(grid.EdgeOpen(2, 2, 1, 1));
	}

	{
		std::vector<int16_t> events = RingEvents(n);
		events[CellIndex(n, 3, 2)] = 0;
		BotCore::NavGrid grid = MakeNav(n, unit, events, HeightZeros(n));
		CHECK(grid.Walk(2, 2));
		CHECK(grid.Walk(3, 3));
		CHECK(!grid.Walk(3, 2));
		CHECK(!grid.EdgeOpen(2, 2, 1, 1));
	}

	{
		const int m = 16;
		BotCore::Rng rng(777);
		std::vector<int16_t> events = RingEvents(m);
		std::vector<float> heights((size_t)m * m, 0.0f);
		for (int x = 1; x < m - 1; ++x)
		{
			for (int z = 1; z < m - 1; ++z)
			{
				if (rng.NextBelow(100) < 30)
					events[CellIndex(m, x, z)] = 0;
				heights[CellIndex(m, x, z)] = (float)rng.NextRange(-200, 200) * 0.01f;
			}
		}

		BotCore::NavGrid grid = MakeNav(m, unit, events, heights);
		bool symmetric = true;
		for (int x = 0; x < m; ++x)
		{
			for (int z = 0; z < m; ++z)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					for (int dz = -1; dz <= 1; ++dz)
					{
						if (dx == 0 && dz == 0)
							continue;
						if (grid.EdgeOpen(x, z, dx, dz) != grid.EdgeOpen(x + dx, z + dz, -dx, -dz))
							symmetric = false;
					}
				}
			}
		}
		CHECK(symmetric);
	}
}

TEST_CASE("Nav_HeightAt_Bilinear")
{
	const int n = 8;
	const float unit = 4.0f;

	std::vector<float> heights((size_t)n * n, 0.0f);
	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j < n; ++j)
			heights[CellIndex(n, i, j)] = 0.5f * ((float)i * unit) + 0.25f * ((float)j * unit);
	}

	BotCore::NavGrid grid = MakeNav(n, unit, Filled(n, 1), heights);

	BotCore::Rng rng(2024);
	const int span = (int)((n - 1) * unit) * 100;
	for (int k = 0; k < 50; ++k)
	{
		const float wx = (float)rng.NextRange(0, span) / 100.0f;
		const float wz = (float)rng.NextRange(0, span) / 100.0f;
		const float expected = 0.5f * wx + 0.25f * wz;
		CHECK(std::fabs(grid.HeightAt(wx, wz) - expected) < 1e-3f);
	}

	const float far = (float)(n - 1) * unit;
	CHECK(std::fabs(grid.HeightAt(-100.0f, -100.0f) - 0.0f) < 1e-3f);
	CHECK(std::fabs(grid.HeightAt(far + 50.0f, far + 50.0f) - (0.5f * far + 0.25f * far)) < 1e-3f);
}

TEST_CASE("Nav_Load_Buffer")
{
	const int n = 3;
	const float unit = 4.0f;
	const size_t cells = (size_t)n * n;

	std::vector<int16_t> events(cells);
	std::vector<float> heights(cells);
	for (size_t i = 0; i < cells; ++i)
	{
		events[i] = (int16_t)(i % 2);
		heights[i] = (float)i * 0.5f;
	}

	std::vector<uint8_t> buffer(16 + 6 * cells, 0);
	std::memcpy(buffer.data(), "FDPNAV01", 8);
	std::memcpy(buffer.data() + 8, &n, 4);
	std::memcpy(buffer.data() + 12, &unit, 4);
	std::memcpy(buffer.data() + 16, events.data(), cells * 2);
	std::memcpy(buffer.data() + 16 + cells * 2, heights.data(), cells * 4);

	BotCore::NavGrid grid;
	CHECK(grid.Load(buffer.data(), buffer.size()));
	CHECK_EQ(grid.Size(), n);
	CHECK_EQ(grid.Unit(), unit);
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			const size_t i = CellIndex(n, x, z);
			CHECK_EQ(grid.Event(x, z), events[i]);
			CHECK_EQ(grid.Height(x, z), heights[i]);
		}
	}

	BotCore::NavGrid other;

	std::vector<uint8_t> bad = buffer;
	bad[0] = 'X';
	CHECK(!other.Load(bad.data(), bad.size()));

	std::vector<uint8_t> longer = buffer;
	longer.push_back(0);
	CHECK(!other.Load(longer.data(), longer.size()));
	CHECK(!other.Load(buffer.data(), buffer.size() - 1));

	std::vector<uint8_t> zero(16, 0);
	std::memcpy(zero.data(), "FDPNAV01", 8);
	const int32_t zn = 0;
	std::memcpy(zero.data() + 8, &zn, 4);
	std::memcpy(zero.data() + 12, &unit, 4);
	CHECK(!other.Load(zero.data(), zero.size()));
}

TEST_CASE("Nav_RealMap_Zone71")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVGRID real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}

	CHECK_EQ(grid.Size(), 513);
	CHECK_EQ(grid.Unit(), 4.0f);

	const int n = grid.Size();
	int64_t count0 = 0;
	int64_t count1 = 0;
	float hmin = 1e30f;
	float hmax = -1e30f;
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			const int16_t e = grid.Event(x, z);
			if (e == 0)
				count0 += 1;
			else if (e == 1)
				count1 += 1;
			const float h = grid.Height(x, z);
			if (h < hmin)
				hmin = h;
			if (h > hmax)
				hmax = h;
		}
	}
	CHECK_EQ(count0, (int64_t)29522);
	CHECK_EQ(count1, (int64_t)233647);
	CHECK(std::fabs(hmin - (-30.633f)) < 1e-3f);
	CHECK(std::fabs(hmax - 82.122f) < 1e-3f);

	const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	grid.Build();
	const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	const double buildMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

	CHECK_EQ(grid.MainComponentCells(), 88508);

	CHECK(grid.Walk(grid.CellOf(1274.0f), grid.CellOf(890.0f)));
	CHECK(grid.Walk(grid.CellOf(746.0f), grid.CellOf(1106.0f)));
	CHECK(!grid.Walk(grid.CellOf(622.0f), grid.CellOf(911.0f)));
	CHECK(std::fabs(grid.HeightAt(1375.0f, 1085.0f) - 11.8f) < 1.5f);

	int clearanceMax = 0;
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			if (!grid.Walk(x, z))
				continue;
			const int c = (int)grid.Clearance(x, z);
			if (c > clearanceMax)
				clearanceMax = c;
		}
	}

	std::printf("NAVGRID real map: n=%d main=%d clearance_max=%d build_ms=%.1f\n",
		n, grid.MainComponentCells(), clearanceMax, buildMs);
}
