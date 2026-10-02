#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavDanger.h>
#include <BotCore/Rng.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

// F5-51: a query that STARTS inside a forbidden region (arena-outside bound, or a bot caught
// inside the enemy tower ring) must not collapse: the forbidden penalty is dropped for such a
// query while every entry rule is kept. These tests pin the fix (synthetic, tower ring, real map).

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

	BotCore::NavGrid MakeNav(int n, float unit, const std::vector<int16_t> & events,
		const std::vector<float> & heights)
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

	int ForbiddenOnPath(const BotCore::NavCostLayer & layer, const std::vector<BotCore::NavCell> & cells)
	{
		int count = 0;
		for (size_t i = 0; i < cells.size(); ++i)
			if (layer.Forbidden(cells[i].x, cells[i].z))
				++count;
		return count;
	}

	// True when the route has an outside -> forbidden transition (entering a forbidden cell).
	bool ReenteredForbidden(const BotCore::NavCostLayer & layer, const std::vector<BotCore::NavCell> & cells)
	{
		for (size_t i = 1; i < cells.size(); ++i)
		{
			if (layer.Forbidden(cells[i].x, cells[i].z) && !layer.Forbidden(cells[i - 1].x, cells[i - 1].z))
				return true;
		}
		return false;
	}

	BotCore::NavCostField Field(const BotCore::NavCostLayer * layer, const BotCore::NavCostParams & params)
	{
		BotCore::NavCostField field;
		field.layer = layer;
		field.params = params;
		return field;
	}

	// Nearest Walk cell (by centre distance) to a world point; (-1,-1) when the grid is empty.
	BotCore::NavCell NearestWalk(const BotCore::NavGrid & grid, float wx, float wz)
	{
		const int n = grid.Size();
		BotCore::NavCell best = Cell(-1, -1);
		float bestD2 = 1.0e30f;
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (!grid.Walk(x, z))
					continue;
				const float dx = grid.CellCenter(x) - wx;
				const float dz = grid.CellCenter(z) - wz;
				const float d2 = dx * dx + dz * dz;
				if (d2 < bestD2)
				{
					bestD2 = d2;
					best = Cell(x, z);
				}
			}
		}
		return best;
	}

	// Smallest number of forbidden cells a straight 8-direction ray from `start` crosses before
	// leaving the forbidden region (including the start cell), or -1 when no ray exits.
	int BestRayForbidden(const BotCore::NavCostLayer & layer, BotCore::NavCell start)
	{
		const int dxs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		const int dzs[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
		int best = -1;
		for (int k = 0; k < 8; ++k)
		{
			int x = start.x;
			int z = start.z;
			int count = 0;
			while (layer.Forbidden(x, z))
			{
				++count;
				if (count > 200)
					break;
				x += dxs[k];
				z += dzs[k];
			}
			if (count > 0 && count <= 200 && (best < 0 || count < best))
				best = count;
		}
		return best;
	}
}

TEST_CASE("NavArena_Synthetic")
{
	const float unit = 4.0f;
	const int n = 128;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	const float cx = grid.CellCenter(64);
	const float cz = grid.CellCenter(64);

	BotCore::NavCostLayer layer;
	layer.Init(grid);
	layer.AddForbidOutsideDisc(cx, cz, 30.0f);

	BotCore::NavPathfinder pf;
	BotCore::NavSearchParams sp;
	BotCore::NavCostParams params;
	BotCore::NavCostField field = Field(&layer, params);

	const BotCore::NavCell outside = Cell(20, 64);
	const BotCore::NavCell inside = Cell(64, 64);
	REQUIRE(grid.Walk(outside.x, outside.z));
	REQUIRE(grid.Walk(inside.x, inside.z));
	REQUIRE(layer.Forbidden(outside.x, outside.z));
	REQUIRE(!layer.Forbidden(inside.x, inside.z));

	// Outside -> inside: found, and the node count stays near the plain search (was ~11x).
	BotCore::NavPathResult plain;
	pf.Find(grid, outside, inside, sp, plain);
	REQUIRE(plain.status == BotCore::NavPathStatus::Found);

	BotCore::NavPathResult out;
	pf.Find(grid, outside, inside, sp, out, &field);
	CHECK(out.status == BotCore::NavPathStatus::Found);
	CHECK(out.expanded <= plain.expanded * 5 / 2);
	bool entered = false;
	bool leftAgain = false;
	for (size_t i = 0; i < out.cells.size(); ++i)
	{
		if (!layer.Forbidden(out.cells[i].x, out.cells[i].z))
			entered = true;
		else if (entered)
			leftAgain = true;
	}
	CHECK(entered);
	CHECK(!leftAgain);

	// Inside -> outside: the arena-bound rule still forbids the target.
	BotCore::NavPathResult invalid;
	pf.Find(grid, inside, outside, sp, invalid, &field);
	CHECK(invalid.status == BotCore::NavPathStatus::InvalidGoal);
	CHECK_EQ(invalid.expanded, 0);

	// Inside start and goal: found, and the route never leaves the disc.
	const BotCore::NavCell inA = Cell(60, 64);
	const BotCore::NavCell inB = Cell(68, 64);
	REQUIRE(!layer.Forbidden(inA.x, inA.z));
	REQUIRE(!layer.Forbidden(inB.x, inB.z));
	BotCore::NavPathResult insideOut;
	pf.Find(grid, inA, inB, sp, insideOut, &field);
	CHECK(insideOut.status == BotCore::NavPathStatus::Found);
	CHECK_EQ(ForbiddenOnPath(layer, insideOut.cells), 0);

	std::printf("NAVARENA synthetic: outside=%d,%d inside=%d,%d plain_expanded=%d expanded=%d forb=%d\n",
		outside.x, outside.z, inside.x, inside.z, plain.expanded, out.expanded,
		ForbiddenOnPath(layer, out.cells));
}

TEST_CASE("NavArena_Tower_Unchanged")
{
	const float unit = 4.0f;
	const int n = 128;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	const float gateX = grid.CellCenter(64);
	const float gateZ = grid.CellCenter(64);

	BotCore::NavZoneParams zp;
	BotCore::NavCostLayer layer;
	BotCore::NavBuildTeamZones(grid, gateX, gateZ, grid.CellCenter(20), grid.CellCenter(20), zp, layer);

	BotCore::NavPathfinder pf;
	BotCore::NavSearchParams sp;
	BotCore::NavCostParams params;
	BotCore::NavCostField field = Field(&layer, params);

	// A bot caught inside the enemy ring leaves by the shortest exit, never re-enters.
	const BotCore::NavCell center = Cell(64, 64);
	REQUIRE(layer.Forbidden(center.x, center.z));
	const BotCore::NavCell outside = Cell(20, 64);
	REQUIRE(grid.Walk(outside.x, outside.z));
	REQUIRE(!layer.Forbidden(outside.x, outside.z));

	BotCore::NavPathResult out;
	pf.Find(grid, center, outside, sp, out, &field);
	CHECK(out.status == BotCore::NavPathStatus::Found);
	const int insideForbidden = ForbiddenOnPath(layer, out.cells);
	CHECK(insideForbidden >= 1);
	CHECK(!ReenteredForbidden(layer, out.cells));
	// The exit is geometrically minimal (<= the straightest ray to the 90 m boundary): at most 2
	// cells more than the plain shortest exit.
	BotCore::NavPathResult plainOut;
	pf.Find(grid, center, outside, sp, plainOut);
	REQUIRE(plainOut.status == BotCore::NavPathStatus::Found);
	const int plainForbidden = ForbiddenOnPath(layer, plainOut.cells);
	CHECK(insideForbidden <= plainForbidden + 2);

	const int rayBest = BestRayForbidden(layer, center);
	REQUIRE(rayBest > 0);

	// The ring can never be entered from outside: 500 random inter-ring pairs, found >= 90%.
	std::vector<BotCore::NavCell> ring;
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			if (!grid.Walk(x, z))
				continue;
			const float dx = grid.CellCenter(x) - gateX;
			const float dz = grid.CellCenter(z) - gateZ;
			const float d2 = dx * dx + dz * dz;
			if (d2 > 8100.0f && d2 <= 28900.0f)
				ring.push_back(Cell(x, z));
		}
	}
	REQUIRE((int)ring.size() > 1000);
	const int count = (int)ring.size();

	BotCore::Rng rng(20261002u);
	std::vector<int> pa;
	std::vector<int> pb;
	int guard = 0;
	while ((int)pa.size() < 500 && guard < 10000000)
	{
		++guard;
		const int i = (int)rng.NextBelow((uint32_t)count);
		const int j = (int)rng.NextBelow((uint32_t)count);
		if (i == j)
			continue;
		const int dx = ring[(size_t)i].x - ring[(size_t)j].x;
		const int dz = ring[(size_t)i].z - ring[(size_t)j].z;
		const int adx = dx < 0 ? -dx : dx;
		const int adz = dz < 0 ? -dz : dz;
		if ((adx > adz ? adx : adz) > 64)
			continue;
		pa.push_back(i);
		pb.push_back(j);
	}
	REQUIRE((int)pa.size() == 500);

	int violations = 0;
	int found = 0;
	for (int q = 0; q < (int)pa.size(); ++q)
	{
		BotCore::NavPathResult r;
		pf.Find(grid, ring[(size_t)pa[(size_t)q]], ring[(size_t)pb[(size_t)q]], sp, r, &field);
		if (r.status == BotCore::NavPathStatus::Found)
		{
			++found;
			if (ForbiddenOnPath(layer, r.cells) != 0)
				++violations;
		}
	}
	std::printf("NAVARENA tower: ring=%d pairs=%d found=%d violations=%d inside_forb=%d plain_forb=%d ray_best=%d\n",
		count, (int)pa.size(), found, violations, insideForbidden, plainForbidden, rayBest);
	CHECK_EQ(violations, 0);
	CHECK(found * 100 >= (int)pa.size() * 90);
}

TEST_CASE("NavArena_RealMap_Respawn")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVARENA real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	BotCore::NavCostLayer layer;
	layer.Init(grid);
	layer.AddForbidOutsideDisc(1274.0f, 890.0f, 60.0f);

	BotCore::NavPathfinder pf;
	BotCore::NavSearchParams sp;
	BotCore::NavCostParams params;
	BotCore::NavCostField field = Field(&layer, params);

	const BotCore::NavCell karus = NearestWalk(grid, 1385.0f, 1095.0f);
	const BotCore::NavCell elmor = NearestWalk(grid, 635.0f, 925.0f);
	const BotCore::NavCell arena = NearestWalk(grid, 1274.0f, 890.0f);
	REQUIRE(grid.Walk(karus.x, karus.z));
	REQUIRE(grid.Walk(elmor.x, elmor.z));
	REQUIRE(grid.Walk(arena.x, arena.z));
	REQUIRE(layer.Forbidden(karus.x, karus.z));
	REQUIRE(layer.Forbidden(elmor.x, elmor.z));
	REQUIRE(!layer.Forbidden(arena.x, arena.z));

	// Arena -> outside spawn: still invalid while the start is inside the bound.
	BotCore::NavPathResult invalid;
	pf.Find(grid, arena, karus, sp, invalid, &field);
	CHECK(invalid.status == BotCore::NavPathStatus::InvalidGoal);

	struct Nation
	{
		const char * name;
		BotCore::NavCell start;
		int maxNodes;
	};
	Nation nations[2] = { { "Karus", karus, 2000 }, { "ElMorad", elmor, 6000 } };

	for (int i = 0; i < 2; ++i)
	{
		BotCore::NavPathResult plain;
		pf.Find(grid, nations[i].start, arena, sp, plain);
		REQUIRE(plain.status == BotCore::NavPathStatus::Found);

		BotCore::NavPathResult out;
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		pf.Find(grid, nations[i].start, arena, sp, out, &field);
		const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
		const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(out.expanded <= nations[i].maxNodes);
		CHECK(out.length <= plain.length * 1.15f + 0.001f);

		bool entered = false;
		bool left = false;
		for (size_t k = 0; k < out.cells.size(); ++k)
		{
			if (!layer.Forbidden(out.cells[k].x, out.cells[k].z))
				entered = true;
			else if (entered)
				left = true;
		}
		CHECK(entered);
		CHECK(!left);

		std::printf("NAVARENA respawn %s: start=%d,%d arena=%d,%d expanded=%d plain_expanded=%d len=%.3f plain_len=%.3f ms=%.3f\n",
			nations[i].name, nations[i].start.x, nations[i].start.z, arena.x, arena.z, out.expanded,
			plain.expanded, out.length, plain.length, ms);
#ifndef _DEBUG
		CHECK(ms <= 2.0);
#endif
	}
}

TEST_CASE("NavArena_Perf")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVARENA perf: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	BotCore::NavCostLayer layer;
	layer.Init(grid);
	layer.AddForbidOutsideDisc(1274.0f, 890.0f, 60.0f);

	const BotCore::NavCell arena = NearestWalk(grid, 1274.0f, 890.0f);
	REQUIRE(grid.Walk(arena.x, arena.z));

	BotCore::NavPathfinder pf;
	BotCore::NavSearchParams sp;
	BotCore::NavCostParams params;
	BotCore::NavCostField field = Field(&layer, params);

	const int queries = 200;
	BotCore::Rng rng(20261002u);
	std::vector<double> ms;
	std::vector<double> expanded;
	ms.reserve((size_t)queries);
	expanded.reserve((size_t)queries);
	int found = 0;

	for (int q = 0; q < queries; ++q)
	{
		const float jx = (float)rng.NextBelow(3001) / 100.0f - 15.0f;
		const float jz = (float)rng.NextBelow(3001) / 100.0f - 15.0f;
		const BotCore::NavCell start = NearestWalk(grid, 1385.0f + jx, 1095.0f + jz);
		if (!grid.Walk(start.x, start.z))
			continue;

		BotCore::NavPathResult out;
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		pf.Find(grid, start, arena, sp, out, &field);
		const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
		ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
		expanded.push_back((double)out.expanded);
		if (out.status == BotCore::NavPathStatus::Found)
			++found;
	}

	REQUIRE((int)ms.size() >= queries - 10);
	std::sort(ms.begin(), ms.end());
	std::sort(expanded.begin(), expanded.end());
	const double p50 = PercentileDouble(ms, 0.50);
	const double p95 = PercentileDouble(ms, 0.95);
	const int exp95 = (int)PercentileDouble(expanded, 0.95);
	std::printf("NAVARENA perf: queries=%d found=%d ms_p50=%.3f ms_p95=%.3f expanded_p95=%d\n",
		(int)ms.size(), found, p50, p95, exp95);

	CHECK(found * 100 >= (int)ms.size() * 95);
#ifndef _DEBUG
	CHECK(p95 <= 2.0);
#else
	(void)p95;
#endif
}
