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
#include <queue>
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

	int CountDanger(const BotCore::NavCostLayer & layer)
	{
		int count = 0;
		for (int x = 0; x < layer.Size(); ++x)
			for (int z = 0; z < layer.Size(); ++z)
				if (layer.Danger(x, z) != 0)
					++count;
		return count;
	}

	int CountForbidden(const BotCore::NavCostLayer & layer)
	{
		int count = 0;
		for (int x = 0; x < layer.Size(); ++x)
			for (int z = 0; z < layer.Size(); ++z)
				if (layer.Forbidden(x, z))
					++count;
		return count;
	}

	int CountSafe(const BotCore::NavCostLayer & layer)
	{
		int count = 0;
		for (int x = 0; x < layer.Size(); ++x)
			for (int z = 0; z < layer.Size(); ++z)
				if (layer.Safe(x, z))
					++count;
		return count;
	}

	int CountForbiddenWalk(const BotCore::NavGrid & grid, const BotCore::NavCostLayer & layer)
	{
		int count = 0;
		for (int x = 0; x < layer.Size(); ++x)
			for (int z = 0; z < layer.Size(); ++z)
				if (grid.Walk(x, z) && layer.Forbidden(x, z))
					++count;
		return count;
	}

	int CountSafeWalk(const BotCore::NavGrid & grid, const BotCore::NavCostLayer & layer)
	{
		int count = 0;
		for (int x = 0; x < layer.Size(); ++x)
			for (int z = 0; z < layer.Size(); ++z)
				if (grid.Walk(x, z) && layer.Safe(x, z))
					++count;
		return count;
	}

	float PathLength(const BotCore::NavGrid & grid, const std::vector<BotCore::NavCell> & cells)
	{
		float length = 0.0f;
		const float unit = grid.Unit();
		for (size_t i = 0; i + 1 < cells.size(); ++i)
		{
			const int dx = cells[i + 1].x - cells[i].x;
			const int dz = cells[i + 1].z - cells[i].z;
			length += (dx != 0 && dz != 0) ? (unit * std::sqrt(2.0f)) : unit;
		}
		return length;
	}

	// F5-51: when the route starts inside a forbidden region, NavPathfinder zeros the forbidden
	// penalty for the whole query (the forbidden cells can only be a prefix). Mirror that here.
	BotCore::NavCostParams EffectiveParams(const BotCore::NavGrid & grid, const BotCore::NavCostLayer * layer,
		const BotCore::NavCostParams & params, BotCore::NavCell start)
	{
		BotCore::NavCostParams effective = params;
		if (layer != nullptr && layer->Size() == grid.Size() && layer->Forbidden(start.x, start.z))
			effective.forbiddenPenalty = 0.0f;
		return effective;
	}

	float PathCost(const BotCore::NavGrid & grid, const BotCore::NavCostLayer * layer,
		const BotCore::NavCostParams & params, const std::vector<BotCore::NavCell> & cells)
	{
		float cost = 0.0f;
		const float unit = grid.Unit();
		const BotCore::NavCostParams effective = cells.empty()
			? params : EffectiveParams(grid, layer, params, cells.front());
		for (size_t i = 0; i + 1 < cells.size(); ++i)
		{
			const int dx = cells[i + 1].x - cells[i].x;
			const int dz = cells[i + 1].z - cells[i].z;
			const float step = (dx != 0 && dz != 0) ? (unit * std::sqrt(2.0f)) : unit;
			const float pa = BotCore::NavCellPenalty(grid, layer, effective, cells[i].x, cells[i].z);
			const float pb = BotCore::NavCellPenalty(grid, layer, effective, cells[i + 1].x, cells[i + 1].z);
			cost += step * (1.0f + 0.5f * (pa + pb));
		}
		return cost;
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

	struct DijkstraItem
	{
		float dist;
		int idx;
	};

	struct DijkstraWorse
	{
		bool operator()(const DijkstraItem & a, const DijkstraItem & b) const { return a.dist > b.dist; }
	};

	// Independent reference: priority-queue Dijkstra with the same edge rule, forbidden rule and
	// step formula. 0 = found (cost filled), 1 = no path, 2 = goal forbidden while start is not.
	int RefDijkstra(const BotCore::NavGrid & grid, const BotCore::NavCostLayer * layer,
		const BotCore::NavCostParams & params, BotCore::NavCell start, BotCore::NavCell goal, float & outCost)
	{
		const int n = grid.Size();
		const bool zones = layer != nullptr && layer->Size() == n;
		if (zones && layer->Forbidden(goal.x, goal.z) && !layer->Forbidden(start.x, start.z))
			return 2;
		outCost = 0.0f;
		if (start == goal)
			return 0;

		// F5-51: zero the forbidden penalty when the start is forbidden (see EffectiveParams).
		const BotCore::NavCostParams effective = zones
			? EffectiveParams(grid, layer, params, start) : params;

		const float unit = grid.Unit();
		std::vector<float> dist((size_t)n * (size_t)n, 1.0e30f);
		std::priority_queue<DijkstraItem, std::vector<DijkstraItem>, DijkstraWorse> open;

		const int startIdx = start.x * n + start.z;
		const int goalIdx = goal.x * n + goal.z;
		dist[(size_t)startIdx] = 0.0f;
		DijkstraItem first;
		first.dist = 0.0f;
		first.idx = startIdx;
		open.push(first);

		const int dxs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		const int dzs[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

		while (!open.empty())
		{
			const DijkstraItem top = open.top();
			open.pop();
			if (top.dist > dist[(size_t)top.idx])
				continue;
			if (top.idx == goalIdx)
			{
				outCost = top.dist;
				return 0;
			}

			const int cx = top.idx / n;
			const int cz = top.idx % n;
			const float penCur = BotCore::NavCellPenalty(grid, layer, effective, cx, cz);
			const bool curForbidden = zones && layer->Forbidden(cx, cz);
			for (int k = 0; k < 8; ++k)
			{
				const int dx = dxs[k];
				const int dz = dzs[k];
				if (!grid.EdgeOpen(cx, cz, dx, dz))
					continue;
				const int nx = cx + dx;
				const int nz = cz + dz;
				if (zones && !curForbidden && layer->Forbidden(nx, nz))
					continue;
				const int nIdx = nx * n + nz;
				const float step = (dx != 0 && dz != 0) ? (unit * std::sqrt(2.0f)) : unit;
				const float penNb = BotCore::NavCellPenalty(grid, layer, effective, nx, nz);
				const float nd = top.dist + step * (1.0f + 0.5f * (penCur + penNb));
				if (nd < dist[(size_t)nIdx])
				{
					dist[(size_t)nIdx] = nd;
					DijkstraItem item;
					item.dist = nd;
					item.idx = nIdx;
					open.push(item);
				}
			}
		}

		return 1;
	}
}

TEST_CASE("NavDanger_Layer_Basics")
{
	// (a) Before Init every query is empty and every Add* is a no-op.
	{
		BotCore::NavCostLayer layer;
		CHECK_EQ(layer.Size(), 0);
		CHECK(layer.Danger(5, 5) == 0);
		CHECK(!layer.Forbidden(5, 5));
		CHECK(!layer.Safe(5, 5));
		BotCore::NavThreatParams tp;
		layer.AddForbidDisc(82.0f, 82.0f, 20.0f);
		layer.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		layer.AddThreats(nullptr, 0, tp);
		CHECK_EQ(layer.Size(), 0);
	}

	const float unit = 4.0f;
	BotCore::NavGrid g40 = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
	BotCore::NavCostLayer layer;

	// (b) Init: 40x40, empty; off-grid answers; Init with an un-Built grid yields size 0.
	{
		layer.Init(g40);
		CHECK_EQ(layer.Size(), 40);
		int bad = 0;
		for (int x = 0; x < 40; ++x)
			for (int z = 0; z < 40; ++z)
				if (layer.Danger(x, z) != 0 || layer.Forbidden(x, z) || layer.Safe(x, z))
					++bad;
		CHECK_EQ(bad, 0);
		CHECK(layer.Danger(-1, 5) == 0);
		CHECK(layer.Danger(40, 5) == 0);
		CHECK(layer.Danger(5, -1) == 0);
		CHECK(layer.Danger(5, 40) == 0);
		CHECK(!layer.Forbidden(-1, 5));
		CHECK(!layer.Safe(5, 40));

		BotCore::NavGrid g0;
		layer.Init(g0);
		CHECK_EQ(layer.Size(), 0);
	}

	// (c) Forbid disc: 81 cells; boundary inclusive; negative radius is a no-op.
	{
		layer.Init(g40);
		layer.AddForbidDisc(82.0f, 82.0f, 20.0f);
		CHECK_EQ(CountForbidden(layer), 81);
		CHECK(layer.Forbidden(24, 20));
		CHECK(layer.Forbidden(25, 20));
		CHECK(layer.Forbidden(23, 24));
		CHECK(!layer.Forbidden(26, 20));
		CHECK(!layer.Forbidden(24, 24));
		CHECK_EQ(CountSafe(layer), 0);

		layer.AddForbidDisc(82.0f, 82.0f, -1.0f);
		CHECK_EQ(CountForbidden(layer), 81);

		BotCore::NavCostLayer small;
		small.Init(g40);
		small.AddForbidDisc(82.0f, 82.0f, 4.0f);
		CHECK_EQ(CountForbidden(small), 5);
		CHECK(small.Forbidden(19, 20));
		CHECK(small.Forbidden(20, 19));
		CHECK(small.Forbidden(20, 20));
		CHECK(small.Forbidden(20, 21));
		CHECK(small.Forbidden(21, 20));
	}

	// (d) Safe disc: 81 cells, independent of the forbidden flag.
	{
		BotCore::NavCostLayer safe;
		safe.Init(g40);
		safe.AddSafeDisc(82.0f, 82.0f, 20.0f);
		CHECK_EQ(CountSafe(safe), 81);
		CHECK_EQ(CountForbidden(safe), 0);
		safe.AddForbidDisc(82.0f, 82.0f, 20.0f);
		CHECK(safe.Safe(20, 20));
		CHECK(safe.Forbidden(20, 20));
	}

	// (e) Outside disc: 1600 - 317 = 1283 forbidden; boundary excluded.
	{
		BotCore::NavCostLayer outside;
		outside.Init(g40);
		outside.AddForbidOutsideDisc(82.0f, 82.0f, 40.0f);
		CHECK_EQ(CountForbidden(outside), 1283);
		CHECK(!outside.Forbidden(20, 20));
		CHECK(!outside.Forbidden(30, 20));
		CHECK(outside.Forbidden(31, 20));
		CHECK(outside.Forbidden(35, 20));
	}

	// (f) Clear, copy construction and assignment.
	{
		BotCore::NavCostLayer a;
		a.Init(g40);
		a.AddForbidDisc(82.0f, 82.0f, 20.0f);
		a.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		a.AddSafeDisc(82.0f, 82.0f, 20.0f);
		const BotCore::NavCostLayer aCopy = a;

		BotCore::NavCostLayer b = a;
		b.Clear();
		CHECK_EQ(b.Size(), 40);
		CHECK(b.Danger(20, 20) == 0);
		CHECK(!b.Forbidden(20, 20));
		CHECK(!b.Safe(20, 20));

		int diff = 0;
		for (int x = 0; x < 40; ++x)
			for (int z = 0; z < 40; ++z)
				if (a.Danger(x, z) != aCopy.Danger(x, z) || a.Forbidden(x, z) != aCopy.Forbidden(x, z)
					|| a.Safe(x, z) != aCopy.Safe(x, z))
					++diff;
		CHECK_EQ(diff, 0);

		b = a;
		diff = 0;
		for (int x = 0; x < 40; ++x)
			for (int z = 0; z < 40; ++z)
				if (b.Danger(x, z) != a.Danger(x, z) || b.Forbidden(x, z) != a.Forbidden(x, z)
					|| b.Safe(x, z) != a.Safe(x, z))
					++diff;
		CHECK_EQ(diff, 0);

		a.Clear();
		CHECK_EQ(a.Size(), 40);
		CHECK(a.Danger(20, 20) == 0);
		CHECK(!a.Forbidden(20, 20));
		CHECK(!a.Safe(20, 20));
	}
}

TEST_CASE("NavDanger_Band_Values")
{
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));

	// (a) Melee: inner 0, outer 15, fade 8, weight 1.0 around (82, 82).
	{
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		CHECK_EQ((int)layer.Danger(20, 20), 255);
		CHECK_EQ((int)layer.Danger(23, 20), 255);
		CHECK_EQ((int)layer.Danger(24, 20), 223);
		CHECK_EQ((int)layer.Danger(25, 20), 96);
		CHECK_EQ((int)layer.Danger(26, 20), 0);
		CHECK_EQ((int)layer.Danger(23, 23), 192);
		CHECK_EQ((int)layer.Danger(24, 23), 96);
		CHECK_EQ((int)layer.Danger(17, 20), 255);
		CHECK_EQ(CountDanger(layer), 101);
	}

	// (b) Ranged: inner 0, outer 45, fade 8, weight 0.6.
	{
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddDangerBand(82.0f, 82.0f, 0.0f, 45.0f, 8.0f, 0.6f);
		CHECK_EQ((int)layer.Danger(20, 20), 153);
		CHECK_EQ((int)layer.Danger(31, 20), 153);
		CHECK_EQ((int)layer.Danger(32, 20), 96);
		CHECK_EQ((int)layer.Danger(33, 20), 19);
		CHECK_EQ((int)layer.Danger(34, 20), 0);
		CHECK_EQ((int)layer.Danger(7, 20), 19);
		CHECK_EQ(CountDanger(layer), 553);
	}

	// (c) Real ring: inner 20, outer 30, fade 8, weight 1.0.
	{
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddDangerBand(82.0f, 82.0f, 20.0f, 30.0f, 8.0f, 1.0f);
		CHECK_EQ((int)layer.Danger(20, 20), 0);
		CHECK_EQ((int)layer.Danger(23, 20), 0);
		CHECK_EQ((int)layer.Danger(25, 20), 255);
		CHECK_EQ((int)layer.Danger(26, 20), 255);
		CHECK_EQ((int)layer.Danger(23, 23), 158);
		CHECK_EQ((int)layer.Danger(27, 20), 255);
		CHECK_EQ((int)layer.Danger(28, 20), 191);
		CHECK_EQ((int)layer.Danger(29, 20), 64);
		CHECK_EQ((int)layer.Danger(30, 20), 0);
		CHECK_EQ((int)layer.Danger(12, 20), 191);
	}

	// (d) Hard edge (fade 0): boundary inclusive; negative fade is 0.
	{
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddDangerBand(82.0f, 82.0f, 0.0f, 16.0f, 0.0f, 1.0f);
		CHECK_EQ((int)layer.Danger(24, 20), 255);
		CHECK_EQ((int)layer.Danger(25, 20), 0);

		BotCore::NavCostLayer neg;
		neg.Init(grid);
		neg.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, -5.0f, 1.0f);
		CHECK_EQ((int)neg.Danger(23, 20), 255);
		CHECK_EQ((int)neg.Danger(24, 20), 0);
	}

	// (e) Weight is clamped to [0, 1]; weight <= 0 is a no-op.
	{
		BotCore::NavCostLayer heavy;
		heavy.Init(grid);
		heavy.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 2.0f);
		CHECK_EQ((int)heavy.Danger(24, 20), 223);
		CHECK_EQ((int)heavy.Danger(25, 20), 96);

		BotCore::NavCostLayer zero;
		zero.Init(grid);
		zero.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 0.0f);
		CHECK_EQ(CountDanger(zero), 0);
		zero.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, -1.0f);
		CHECK_EQ(CountDanger(zero), 0);
	}

	// (f) Max combination is order independent.
	{
		BotCore::NavCostLayer ab;
		ab.Init(grid);
		ab.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 0.4f);
		ab.AddDangerBand(106.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		CHECK_EQ((int)ab.Danger(20, 20), 102);
		CHECK_EQ((int)ab.Danger(21, 20), 102);
		CHECK_EQ((int)ab.Danger(22, 20), 223);
		CHECK_EQ((int)ab.Danger(23, 20), 255);
		CHECK_EQ((int)ab.Danger(27, 20), 255);

		BotCore::NavCostLayer ba;
		ba.Init(grid);
		ba.AddDangerBand(106.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		ba.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 0.4f);
		int diff = 0;
		for (int x = 0; x < 40; ++x)
			for (int z = 0; z < 40; ++z)
				if (ab.Danger(x, z) != ba.Danger(x, z))
					++diff;
		CHECK_EQ(diff, 0);
	}

	// (g) Center outside the grid contributes only inside; far outside contributes nothing.
	{
		BotCore::NavCostLayer off;
		off.Init(grid);
		off.AddDangerBand(-10.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		CHECK_EQ((int)off.Danger(0, 20), 255);
		CHECK_EQ((int)off.Danger(1, 20), 223);
		CHECK_EQ((int)off.Danger(2, 20), 96);
		CHECK_EQ((int)off.Danger(3, 20), 0);

		BotCore::NavCostLayer far;
		far.Init(grid);
		far.AddDangerBand(-1000.0f, -1000.0f, 0.0f, 45.0f, 8.0f, 0.6f);
		CHECK_EQ(CountDanger(far), 0);
	}
}

TEST_CASE("NavDanger_Threats")
{
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));

	// (a) Defaults.
	{
		BotCore::NavThreatParams tp;
		CHECK_EQ(tp.meleeCoreM, 15.0f);
		CHECK_EQ(tp.meleeWeight, 1.0f);
		CHECK_EQ(tp.rangedReachM, 45.0f);
		CHECK_EQ(tp.rangedWeight, 0.6f);
		CHECK_EQ(tp.fadeM, 8.0f);
		BotCore::NavCostParams cp;
		CHECK_EQ(cp.wDanger, 4.0f);
		CHECK_EQ(cp.wClear, 0.5f);
		CHECK_EQ(cp.clearFree, 2);
		CHECK_EQ(cp.forbiddenPenalty, 10.0f);
		BotCore::NavZoneParams zp;
		CHECK_EQ(zp.towerRingM, 90.0f);
	}

	// (b) A single melee / ranged threat matches the band tables.
	{
		BotCore::NavThreatParams tp;
		BotCore::NavThreat melee;
		melee.x = 82.0f;
		melee.z = 82.0f;
		melee.kind = BotCore::NavThreatKind::Melee;
		BotCore::NavCostLayer lm;
		lm.Init(grid);
		lm.AddThreats(&melee, 1, tp);
		CHECK_EQ(CountDanger(lm), 101);
		CHECK_EQ((int)lm.Danger(24, 20), 223);

		BotCore::NavThreat ranged = melee;
		ranged.kind = BotCore::NavThreatKind::Ranged;
		BotCore::NavCostLayer lr;
		lr.Init(grid);
		lr.AddThreats(&ranged, 1, tp);
		CHECK_EQ(CountDanger(lr), 553);
		CHECK_EQ((int)lr.Danger(31, 20), 153);
	}

	// (c) Melee + ranged at the same center; order independent.
	{
		BotCore::NavThreatParams tp;
		BotCore::NavThreat both[2];
		both[0].x = 82.0f;
		both[0].z = 82.0f;
		both[0].kind = BotCore::NavThreatKind::Melee;
		both[1] = both[0];
		both[1].kind = BotCore::NavThreatKind::Ranged;

		BotCore::NavCostLayer mr;
		mr.Init(grid);
		mr.AddThreats(both, 2, tp);
		CHECK_EQ((int)mr.Danger(20, 20), 255);
		CHECK_EQ((int)mr.Danger(24, 20), 223);
		CHECK_EQ((int)mr.Danger(25, 20), 153);
		CHECK_EQ((int)mr.Danger(32, 20), 96);
		CHECK_EQ((int)mr.Danger(33, 20), 19);
		CHECK_EQ((int)mr.Danger(23, 23), 192);
		CHECK_EQ(CountDanger(mr), 553);

		BotCore::NavThreat reversed[2];
		reversed[0] = both[1];
		reversed[1] = both[0];
		BotCore::NavCostLayer rm;
		rm.Init(grid);
		rm.AddThreats(reversed, 2, tp);
		int diff = 0;
		for (int x = 0; x < 40; ++x)
			for (int z = 0; z < 40; ++z)
				if (mr.Danger(x, z) != rm.Danger(x, z))
					++diff;
		CHECK_EQ(diff, 0);
	}

	// (d) Custom parameters.
	{
		BotCore::NavThreatParams tp;
		tp.fadeM = 0.0f;
		tp.meleeCoreM = 16.0f;
		BotCore::NavThreat melee;
		melee.x = 82.0f;
		melee.z = 82.0f;
		melee.kind = BotCore::NavThreatKind::Melee;
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddThreats(&melee, 1, tp);
		CHECK_EQ((int)layer.Danger(24, 20), 255);
		CHECK_EQ((int)layer.Danger(25, 20), 0);

		BotCore::NavThreatParams wp;
		wp.meleeWeight = 0.4f;
		BotCore::NavCostLayer weak;
		weak.Init(grid);
		weak.AddThreats(&melee, 1, wp);
		CHECK_EQ((int)weak.Danger(20, 20), 102);
	}

	// (e) count == 0 and nullptr leave the layer unchanged.
	{
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		BotCore::NavThreatParams tp;
		BotCore::NavThreat melee;
		melee.x = 10.0f;
		melee.z = 10.0f;
		melee.kind = BotCore::NavThreatKind::Melee;
		const int before = CountDanger(layer);
		layer.AddThreats(&melee, 0, tp);
		layer.AddThreats(nullptr, 0, tp);
		CHECK_EQ(CountDanger(layer), before);
		CHECK_EQ((int)layer.Danger(20, 20), 255);
	}

	// (f) Dynamic frame pattern: copy the static layer, add threats, discard next frame.
	{
		BotCore::NavCostLayer base;
		base.Init(grid);
		base.AddForbidDisc(82.0f, 82.0f, 20.0f);
		base.AddSafeDisc(82.0f, 82.0f, 20.0f);

		BotCore::NavThreatParams tp;
		BotCore::NavThreat melee;
		melee.x = 82.0f;
		melee.z = 82.0f;
		melee.kind = BotCore::NavThreatKind::Melee;

		BotCore::NavCostLayer dyn = base;
		dyn.AddThreats(&melee, 1, tp);
		CHECK(dyn.Danger(20, 20) > 0);
		int flagDiff = 0;
		for (int x = 0; x < 40; ++x)
			for (int z = 0; z < 40; ++z)
				if (dyn.Forbidden(x, z) != base.Forbidden(x, z) || dyn.Safe(x, z) != base.Safe(x, z))
					++flagDiff;
		CHECK_EQ(flagDiff, 0);
		CHECK_EQ(CountDanger(base), 0);

		dyn = base;
		CHECK_EQ(CountDanger(dyn), 0);
	}
}

TEST_CASE("NavDanger_Penalty")
{
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
	BotCore::NavCostParams params;

	// (a) Null layer: clearance term only.
	{
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, nullptr, params, 1, 10) - 0.5f) <= 1e-6f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, nullptr, params, 2, 10) - 0.0f) <= 1e-6f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, nullptr, params, 20, 20) - 0.0f) <= 1e-6f);
	}

	BotCore::NavCostLayer band;
	band.Init(grid);
	band.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);

	// (b) Danger term.
	{
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &band, params, 20, 20) - 4.0f) <= 1e-6f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &band, params, 24, 20) - 4.0f * 223.0f / 255.0f) <= 1e-4f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &band, params, 30, 20) - 0.0f) <= 1e-6f);
	}

	// (c) Forbidden term on top of danger.
	{
		BotCore::NavCostLayer forbid = band;
		forbid.AddForbidDisc(82.0f, 82.0f, 4.0f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &forbid, params, 20, 20) - 14.0f) <= 1e-6f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &forbid, params, 19, 20) - 14.0f) <= 1e-6f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &forbid, params, 21, 20) - 14.0f) <= 1e-6f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &forbid, params, 22, 20) - 4.0f) <= 1e-6f);
	}

	BotCore::NavCostLayer forbid;
	forbid.Init(grid);
	forbid.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);
	forbid.AddForbidDisc(82.0f, 82.0f, 4.0f);

	// (d) Parameter edge cases: negative weights count as 0.
	{
		BotCore::NavCostParams p;
		p.wDanger = 0.0f;
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &forbid, p, 20, 20) - 10.0f) <= 1e-6f);
		p.wDanger = -1.0f;
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &forbid, p, 20, 20) - 10.0f) <= 1e-6f);

		BotCore::NavCostParams q;
		q.forbiddenPenalty = -3.0f;
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &forbid, q, 20, 20) - 4.0f) <= 1e-6f);

		BotCore::NavCostParams cw;
		cw.wClear = 2.0f;
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, nullptr, cw, 1, 10) - 2.0f) <= 1e-6f);

		BotCore::NavCostParams c3;
		c3.clearFree = 3;
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, nullptr, c3, 2, 10) - 0.5f) <= 1e-6f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, nullptr, c3, 1, 10) - 1.0f) <= 1e-6f);

		BotCore::NavCostParams c0;
		c0.clearFree = 0;
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, nullptr, c0, 1, 10) - 0.0f) <= 1e-6f);
	}

	// (e) A layer of another size contributes nothing.
	{
		BotCore::NavGrid grid30 = MakeNav(30, unit, RingEvents(30), HeightZeros(30));
		BotCore::NavCostLayer other;
		other.Init(grid30);
		other.AddDangerBand(60.0f, 60.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &other, params, 20, 20) - 0.0f) <= 1e-6f);
		CHECK(std::fabs(BotCore::NavCellPenalty(grid, &other, params, 1, 10) - 0.5f) <= 1e-6f);
	}
}

TEST_CASE("NavDanger_Path_Field")
{
	const float unit = 4.0f;
	BotCore::NavPathfinder pf;
	BotCore::NavSearchParams sp;

	// (a) No field is bit-identical to the default argument.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavPathResult plain;
		BotCore::NavPathResult nullField;
		pf.Find(grid, Cell(5, 20), Cell(35, 20), sp, plain);
		pf.Find(grid, Cell(5, 20), Cell(35, 20), sp, nullField, nullptr);
		CHECK(plain.status == BotCore::NavPathStatus::Found);
		CHECK(plain.status == nullField.status);
		CHECK(plain.cells == nullField.cells);
		CHECK_EQ(plain.expanded, nullField.expanded);
		CHECK(plain.cost == nullField.cost);
		CHECK(std::fabs(plain.cost - 120.0f) <= 1e-3f);
		CHECK(std::fabs(plain.length - plain.cost) <= 1e-3f);
	}

	// (b) Zero-weight field equals the plain search on random maps.
	{
		int found = 0;
		for (int seed = 0; seed < 10; ++seed)
		{
			BotCore::Rng rng(3000u + (uint32_t)seed);
			const int n = 24;
			std::vector<int16_t> events = RingEvents(n);
			std::vector<float> heights((size_t)n * (size_t)n, 0.0f);
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
			std::vector<BotCore::NavCell> cells;
			for (int x = 0; x < n; ++x)
				for (int z = 0; z < n; ++z)
					if (grid.Walk(x, z))
						cells.push_back(Cell(x, z));
			if (cells.size() < 2)
				continue;

			BotCore::NavCostParams zero;
			zero.wDanger = 0.0f;
			zero.wClear = 0.0f;
			BotCore::NavCostField field = Field(nullptr, zero);

			for (int q = 0; q < 40; ++q)
			{
				const BotCore::NavCell a = cells[(size_t)rng.NextBelow((uint32_t)cells.size())];
				const BotCore::NavCell b = cells[(size_t)rng.NextBelow((uint32_t)cells.size())];
				BotCore::NavPathResult plain;
				BotCore::NavPathResult out;
				pf.Find(grid, a, b, sp, plain);
				pf.Find(grid, a, b, sp, out, &field);
				CHECK(plain.status == out.status);
				CHECK(plain.cells == out.cells);
				CHECK_EQ(plain.expanded, out.expanded);
				CHECK(plain.cost == out.cost);
				CHECK(std::fabs(out.length - out.cost) <= 1e-3f);
				if (out.status == BotCore::NavPathStatus::Found)
					++found;
			}
		}
		REQUIRE(found >= 150);
	}

	// (c) S1: the melee band moves the path off the danger zone.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		BotCore::NavCostParams params;
		BotCore::NavCostField field = Field(&layer, params);

		BotCore::NavPathResult out;
		pf.Find(grid, Cell(5, 20), Cell(35, 20), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 139.882f) <= 0.05f);
		int maxDanger = 0;
		for (size_t i = 0; i < out.cells.size(); ++i)
			maxDanger = std::max(maxDanger, (int)layer.Danger(out.cells[i].x, out.cells[i].z));
		CHECK_EQ(maxDanger, 0);
		CHECK(std::fabs(out.length - out.cost) <= 0.01f);

		BotCore::NavCostParams zero = params;
		zero.wDanger = 0.0f;
		BotCore::NavCostField zf = Field(&layer, zero);
		BotCore::NavPathResult zout;
		pf.Find(grid, Cell(5, 20), Cell(35, 20), sp, zout, &zf);
		CHECK(std::fabs(zout.cost - 120.0f) <= 1e-3f);
	}

	// (d) S2: a soft danger zone with a single gap is still crossed.
	{
		BotCore::NavGrid grid = MakeNav(30, unit, GapWall(30, 15, 14, 15), HeightZeros(30));
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddDangerBand(62.0f, 60.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		BotCore::NavCostParams params;
		BotCore::NavCostField field = Field(&layer, params);

		BotCore::NavPathResult plain;
		pf.Find(grid, Cell(10, 14), Cell(20, 14), sp, plain);
		CHECK(std::fabs(plain.cost - 40.0f) <= 1e-3f);

		BotCore::NavPathResult out;
		pf.Find(grid, Cell(10, 14), Cell(20, 14), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 191.255f) <= 0.05f);
		CHECK(std::fabs(out.length - 40.0f) <= 1e-3f);
		bool gap = false;
		for (size_t i = 0; i < out.cells.size(); ++i)
			if (out.cells[i].x == 15 && (out.cells[i].z == 14 || out.cells[i].z == 15))
				gap = true;
		CHECK(gap);
	}

	// (e) S3: clearance prefers cells away from the wall edge.
	{
		BotCore::NavGrid grid = MakeNav(20, unit, RingEvents(20), HeightZeros(20));
		BotCore::NavCostParams params;
		BotCore::NavCostField field = Field(nullptr, params);

		BotCore::NavPathResult plain;
		pf.Find(grid, Cell(1, 2), Cell(1, 17), sp, plain);
		CHECK(std::fabs(plain.cost - 60.0f) <= 1e-3f);

		BotCore::NavPathResult out;
		pf.Find(grid, Cell(1, 2), Cell(1, 17), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 66.142f) <= 0.05f);
		CHECK(std::fabs(out.length - 63.314f) <= 0.01f);
		CHECK(out.length < out.cost);
		for (size_t i = 1; i + 1 < out.cells.size(); ++i)
			CHECK(grid.Clearance(out.cells[i].x, out.cells[i].z) >= 2);
	}

	// (f) S4: the forbidden disc is never entered from outside; inside start may leave.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddForbidDisc(82.0f, 82.0f, 20.0f);
		BotCore::NavCostParams params;
		BotCore::NavCostField field = Field(&layer, params);

		BotCore::NavPathResult out;
		pf.Find(grid, Cell(5, 20), Cell(35, 20), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 139.882f) <= 0.05f);
		CHECK_EQ(ForbiddenOnPath(layer, out.cells), 0);

		pf.Find(grid, Cell(20, 20), Cell(35, 20), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 60.0f) <= 0.01f);   // F5-51: start forbidden -> penalty 0, geometric
		CHECK(ForbiddenOnPath(layer, out.cells) > 0);
		CHECK(!ReenteredForbidden(layer, out.cells));

		pf.Find(grid, Cell(18, 20), Cell(22, 20), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK_EQ((int)out.cells.size(), 5);
		CHECK_EQ(ForbiddenOnPath(layer, out.cells), 5);
		CHECK(std::fabs(out.cost - 16.0f) <= 0.01f);   // F5-51: both inside -> geometric

		pf.Find(grid, Cell(5, 20), Cell(20, 20), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::InvalidGoal);
		CHECK_EQ(out.expanded, 0);
		CHECK(out.cells.empty());
		CHECK(out.cost == 0.0f);
		CHECK(out.length == 0.0f);

		pf.Find(grid, Cell(5, 20), Cell(35, 20), sp, out);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		pf.Find(grid, Cell(20, 20), Cell(35, 20), sp, out);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		pf.Find(grid, Cell(18, 20), Cell(22, 20), sp, out);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		pf.Find(grid, Cell(5, 20), Cell(20, 20), sp, out);
		CHECK(out.status == BotCore::NavPathStatus::Found);
	}

	// (g) S4b: the only gap is forbidden, so there is no path.
	{
		BotCore::NavGrid grid = MakeNav(30, unit, GapWall(30, 15, 14, 15), HeightZeros(30));
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddForbidDisc(62.0f, 60.0f, 10.0f);
		BotCore::NavCostParams params;
		BotCore::NavCostField field = Field(&layer, params);

		BotCore::NavPathResult out;
		pf.Find(grid, Cell(10, 14), Cell(20, 14), sp, out);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 40.0f) <= 1e-3f);

		pf.Find(grid, Cell(10, 14), Cell(20, 14), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::NoPath);
		CHECK(out.cells.empty());
	}

	// (h) S5: outside the arena bound is forbidden; a target outside is invalid.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddForbidOutsideDisc(82.0f, 82.0f, 40.0f);
		BotCore::NavCostParams params;
		BotCore::NavCostField field = Field(&layer, params);

		BotCore::NavPathResult out;
		pf.Find(grid, Cell(20, 20), Cell(28, 20), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 32.0f) <= 1e-3f);

		pf.Find(grid, Cell(20, 20), Cell(35, 20), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::InvalidGoal);
	}

	// (i) start == goal with a field.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavCostLayer layer;
		layer.Init(grid);
		layer.AddDangerBand(82.0f, 82.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		BotCore::NavCostParams params;
		BotCore::NavCostField field = Field(&layer, params);

		BotCore::NavPathResult out;
		pf.Find(grid, Cell(20, 20), Cell(20, 20), sp, out, &field);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK_EQ((int)out.cells.size(), 1);
		CHECK(out.cost == 0.0f);
		CHECK(out.length == 0.0f);
	}

	// (j) A layer of another size is ignored; clearance still applies.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavGrid grid30 = MakeNav(30, unit, RingEvents(30), HeightZeros(30));
		BotCore::NavCostLayer other;
		other.Init(grid30);
		other.AddDangerBand(60.0f, 60.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		BotCore::NavCostParams params;
		BotCore::NavCostField mismatch = Field(&other, params);
		BotCore::NavCostField nullLayer = Field(nullptr, params);

		BotCore::NavPathResult a;
		BotCore::NavPathResult b;
		pf.Find(grid, Cell(5, 20), Cell(35, 20), sp, a, &mismatch);
		pf.Find(grid, Cell(5, 20), Cell(35, 20), sp, b, &nullLayer);
		CHECK(a.status == b.status);
		CHECK(a.cells == b.cells);
		CHECK_EQ(a.expanded, b.expanded);
		CHECK(std::fabs(a.cost - b.cost) <= 1e-6f);
		CHECK(std::fabs(a.length - b.length) <= 1e-6f);
	}
}

TEST_CASE("NavDanger_Path_Optimal")
{
	const float unit = 4.0f;
	BotCore::NavPathfinder pf;
	BotCore::NavSearchParams sp;
	sp.maxNodes = 100000;

	int found = 0;
	int invalidGoal = 0;
	int noPath = 0;

	for (int seed = 0; seed < 30; ++seed)
	{
		BotCore::Rng rng(4000u + (uint32_t)seed);
		const int n = 24;
		std::vector<int16_t> events = RingEvents(n);
		std::vector<float> heights((size_t)n * (size_t)n, 0.0f);
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

		BotCore::NavCostLayer layer;
		layer.Init(grid);
		for (int b = 0; b < 3; ++b)
		{
			const float cx = (float)rng.NextBelow(96);
			const float cz = (float)rng.NextBelow(96);
			const uint32_t kind = rng.NextBelow(2);
			if (kind == 0)
				layer.AddDangerBand(cx, cz, 0.0f, 15.0f, 8.0f, 1.0f);
			else
				layer.AddDangerBand(cx, cz, 0.0f, 45.0f, 8.0f, 0.6f);
		}
		if (seed < 15)
		{
			const float fx = (float)rng.NextBelow(96);
			const float fz = (float)rng.NextBelow(96);
			layer.AddForbidDisc(fx, fz, 12.0f);
			const float sx = (float)rng.NextBelow(96);
			const float sz = (float)rng.NextBelow(96);
			layer.AddSafeDisc(sx, sz, 12.0f);
		}

		BotCore::NavCostParams params;
		if (seed % 2 == 1)
		{
			params.wDanger = 10.0f;
			params.wClear = 1.5f;
			params.clearFree = 3;
			params.forbiddenPenalty = 3.0f;
		}
		BotCore::NavCostField field = Field(&layer, params);

		std::vector<BotCore::NavCell> cells;
		for (int x = 0; x < n; ++x)
			for (int z = 0; z < n; ++z)
				if (grid.Walk(x, z))
					cells.push_back(Cell(x, z));
		if (cells.size() < 2)
			continue;

		for (int q = 0; q < 100; ++q)
		{
			const BotCore::NavCell a = cells[(size_t)rng.NextBelow((uint32_t)cells.size())];
			const BotCore::NavCell b = cells[(size_t)rng.NextBelow((uint32_t)cells.size())];

			float refCost = 0.0f;
			const int ref = RefDijkstra(grid, &layer, params, a, b, refCost);
			BotCore::NavPathResult out;
			pf.Find(grid, a, b, sp, out, &field);

			if (ref == 2)
			{
				CHECK(out.status == BotCore::NavPathStatus::InvalidGoal);
				CHECK_EQ(out.expanded, 0);
				++invalidGoal;
			}
			else if (ref == 0)
			{
				CHECK(out.status == BotCore::NavPathStatus::Found);
				CHECK(std::fabs(out.cost - refCost) <= 0.01f + 1e-4f * refCost);
				++found;
			}
			else
			{
				CHECK(out.status == BotCore::NavPathStatus::NoPath);
				++noPath;
			}

			if (out.status == BotCore::NavPathStatus::Found)
			{
				bool edges = true;
				for (size_t i = 0; i + 1 < out.cells.size(); ++i)
				{
					const int dx = out.cells[i + 1].x - out.cells[i].x;
					const int dz = out.cells[i + 1].z - out.cells[i].z;
					if (!grid.EdgeOpen(out.cells[i].x, out.cells[i].z, dx, dz))
						edges = false;
				}
				CHECK(edges);
				CHECK(!ReenteredForbidden(layer, out.cells));
				CHECK(std::fabs(PathCost(grid, &layer, params, out.cells) - out.cost) <= 0.01f + 1e-4f * out.cost);
				CHECK(std::fabs(PathLength(grid, out.cells) - out.length) <= 1e-3f);
				CHECK(out.length <= out.cost + 1e-3f);

				if (seed >= 15)
				{
					BotCore::NavPathResult reverse;
					pf.Find(grid, b, a, sp, reverse, &field);
					CHECK(reverse.status == BotCore::NavPathStatus::Found);
					CHECK(std::fabs(out.cost - reverse.cost) <= 0.01f + 1e-4f * out.cost);
				}
			}
		}
	}

	std::printf("NAVDANGER random maps: seeds=30 pairs=3000 found=%d invalid_goal=%d no_path=%d\n",
		found, invalidGoal, noPath);
	REQUIRE(found >= 1000);
	REQUIRE(invalidGoal >= 20);
	REQUIRE(noPath >= 300);
}

TEST_CASE("NavDanger_RealMap")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVDANGER real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	const int n = grid.Size();
	BotCore::NavZoneParams zp;
	BotCore::NavCostLayer elm;
	BotCore::NavCostLayer kar;
	BotCore::NavBuildTeamZones(grid, 1375.0f, 1085.0f, 622.0f, 911.0f, zp, elm);
	BotCore::NavBuildTeamZones(grid, 622.0f, 911.0f, 1375.0f, 1085.0f, zp, kar);

	const int elmForbid = CountForbidden(elm);
	const int elmForbidWalk = CountForbiddenWalk(grid, elm);
	const int elmSafe = CountSafe(elm);
	const int elmSafeWalk = CountSafeWalk(grid, elm);
	CHECK_EQ(elmForbid, 1594);
	CHECK_EQ(elmForbidWalk, 1264);
	CHECK_EQ(elmSafe, 1591);
	CHECK_EQ(elmSafeWalk, 1232);
	CHECK_EQ(CountForbidden(kar), 1591);
	CHECK_EQ(CountForbiddenWalk(grid, kar), 1232);
	CHECK_EQ(CountSafe(kar), 1594);
	CHECK_EQ(CountSafeWalk(grid, kar), 1264);
	CHECK(!grid.Walk(343, 271));
	CHECK(!grid.Walk(155, 227));

	BotCore::NavPathfinder pf;
	BotCore::NavSearchParams sp;
	BotCore::NavCostParams zero;
	zero.wDanger = 0.0f;
	zero.wClear = 0.0f;
	BotCore::NavCostParams params;
	BotCore::NavCostField fElmZero = Field(&elm, zero);
	BotCore::NavCostField fKarZero = Field(&kar, zero);
	BotCore::NavCostField fElm = Field(&elm, params);

	const BotCore::NavCell a = Cell(318, 222);
	const BotCore::NavCell b = Cell(186, 276);
	REQUIRE(grid.Walk(a.x, a.z));
	REQUIRE(grid.Walk(b.x, b.z));

	BotCore::NavPathResult out;
	float arenaDefault = 0.0f;
	float crossPlain = 0.0f;
	float crossField = 0.0f;
	float startInsideCost = 0.0f;
	float startInsideLen = 0.0f;
	int startInsideForb = 0;
	int ringPairs = 0;
	int ringFound = 0;
	int ringNoPath = 0;
	int ringNodeLimit = 0;
	int plainCross = 0;
	int violations = 0;

	// Arena A -> B: the route stays away from both rings.
	{
		// ADR-0024 (maxSlope 0.45): re-measured on the real grid, 2026-10-03 (was 660.617 m).
		pf.Find(grid, a, b, sp, out);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 670.961f) <= 0.01f);

		pf.Find(grid, a, b, sp, out, &fElmZero);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 670.961f) <= 0.01f);
		CHECK_EQ(ForbiddenOnPath(elm, out.cells), 0);

		pf.Find(grid, a, b, sp, out, &fKarZero);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 670.961f) <= 0.01f);

		pf.Find(grid, a, b, sp, out, &fElm);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 704.831f) <= 0.1f);
		CHECK(std::fabs(out.length - 687.931f) <= 0.1f);
		CHECK(out.cost > 670.961f);
		arenaDefault = out.cost;
	}

	// A target inside the enemy ring is invalid; without a field it is found.
	{
		BotCore::NavCell f = Cell(-1, -1);
		bool foundF = false;
		for (int x = 0; x < n && !foundF; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (grid.Walk(x, z) && elm.Forbidden(x, z))
				{
					f = Cell(x, z);
					foundF = true;
					break;
				}
			}
		}
		REQUIRE(foundF);
		CHECK_EQ(f.x, 321);
		CHECK_EQ(f.z, 268);

		pf.Find(grid, a, f, sp, out, &fElm);
		CHECK(out.status == BotCore::NavPathStatus::InvalidGoal);
		CHECK_EQ(out.expanded, 0);

		pf.Find(grid, a, f, sp, out);
		CHECK(out.status == BotCore::NavPathStatus::Found);
	}

	// A start inside the enemy ring: leave by the shortest exit, never re-enter.
	{
		BotCore::NavCell s = Cell(-1, -1);
		float best = 1.0e30f;
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (!grid.Walk(x, z))
					continue;
				const float dx = grid.CellCenter(x) - 1375.0f;
				const float dz = grid.CellCenter(z) - 1085.0f;
				const float d2 = dx * dx + dz * dz;
				if (d2 < best)
				{
					best = d2;
					s = Cell(x, z);
				}
			}
		}
		REQUIRE(s.x == 342);
		REQUIRE(s.z == 272);

		pf.Find(grid, s, a, sp, out, &fElm);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		startInsideCost = out.cost;
		startInsideLen = out.length;
		startInsideForb = ForbiddenOnPath(elm, out.cells);
		CHECK(std::fabs(out.cost - 277.108f) <= 0.5f);   // F5-51: forbidden prefix penalty 0
		CHECK(std::fabs(out.length - 274.108f) <= 0.1f);
		CHECK_EQ(startInsideForb, 21);
		CHECK(!ReenteredForbidden(elm, out.cells));
		bool left = false;
		bool prefix = true;
		for (size_t i = 0; i < out.cells.size(); ++i)
		{
			if (elm.Forbidden(out.cells[i].x, out.cells[i].z))
			{
				if (left)
					prefix = false;
			}
			else
			{
				left = true;
			}
		}
		CHECK(prefix);

		pf.Find(grid, s, a, sp, out);
		CHECK(std::fabs(out.cost - 270.794f) <= 0.01f);
	}

	// Cross pair: the plain route cuts through the ring, the field route does not.
	{
		// Re-measured for ADR-0024 (the old 371,248 -> 334,308 pair no longer cuts the ring):
		// both routes are in the main component; the plain route crosses forbidden cells (24),
		// the zero-weighted field route detours around them, so it costs more.
		const BotCore::NavCell ca = Cell(360, 239);
		const BotCore::NavCell cb = Cell(323, 297);
		REQUIRE(grid.Walk(ca.x, ca.z));
		REQUIRE(grid.Walk(cb.x, cb.z));
		REQUIRE(!elm.Forbidden(ca.x, ca.z));
		REQUIRE(!elm.Forbidden(cb.x, cb.z));

		pf.Find(grid, ca, cb, sp, out);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 335.078f) <= 0.01f);
		crossPlain = out.cost;
		CHECK_EQ(ForbiddenOnPath(elm, out.cells), 24);

		pf.Find(grid, ca, cb, sp, out, &fElmZero);
		CHECK(out.status == BotCore::NavPathStatus::Found);
		CHECK(std::fabs(out.cost - 343.078f) <= 0.05f);
		crossField = out.cost;
		CHECK_EQ(ForbiddenOnPath(elm, out.cells), 0);
		CHECK(crossField > crossPlain);
	}

	// Ring sweep: AC-NAV-06 infrastructure (no field path enters the forbidden ring).
	{
		std::vector<BotCore::NavCell> ring;
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (!grid.Walk(x, z))
					continue;
				const float dx = grid.CellCenter(x) - 1375.0f;
				const float dz = grid.CellCenter(z) - 1085.0f;
				const float d2 = dx * dx + dz * dz;
				if (d2 > 8100.0f && d2 <= 28900.0f)
					ring.push_back(Cell(x, z));
			}
		}
		REQUIRE((int)ring.size() == 3689);
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
		ringPairs = (int)pa.size();

		for (int q = 0; q < ringPairs; ++q)
		{
			const BotCore::NavCell ca = ring[(size_t)pa[(size_t)q]];
			const BotCore::NavCell cb = ring[(size_t)pb[(size_t)q]];

			BotCore::NavPathResult plain;
			pf.Find(grid, ca, cb, sp, plain);
			const bool plainFound = plain.status == BotCore::NavPathStatus::Found;
			const float plainCost = plain.cost;
			if (plainFound && ForbiddenOnPath(elm, plain.cells) > 0)
				++plainCross;

			BotCore::NavPathResult fieldOut;
			pf.Find(grid, ca, cb, sp, fieldOut, &fElmZero);
			if (fieldOut.status == BotCore::NavPathStatus::Found)
			{
				++ringFound;
				const int forb = ForbiddenOnPath(elm, fieldOut.cells);
				if (forb != 0)
				{
					++violations;
					if (violations == 1)
						std::printf("NAVDANGER ring violation pair=%d,%d -> %d,%d forbidden=%d\n",
							ca.x, ca.z, cb.x, cb.z, forb);
				}
				if (plainFound)
					CHECK(fieldOut.cost >= plainCost - 0.01f);
			}
			else if (fieldOut.status == BotCore::NavPathStatus::NoPath)
			{
				++ringNoPath;
			}
			else if (fieldOut.status == BotCore::NavPathStatus::NodeLimit)
			{
				++ringNodeLimit;
			}
		}

		CHECK(violations == 0);
		CHECK(ringFound * 100 >= ringPairs * 90);
		CHECK(plainCross >= 100);
	}

	std::printf("NAVDANGER real: elm_forbid=%d elm_forbid_walk=%d elm_safe=%d elm_safe_walk=%d; arena A->B cost=%.3f default=%.3f; cross plain=%.3f field=%.3f; start-inside cost=%.3f len=%.3f forb=%d; ring sweep pairs=%d found=%d no_path=%d node_limit=%d plain_cross=%d violations=%d\n",
		elmForbid, elmForbidWalk, elmSafe, elmSafeWalk, 670.961f, arenaDefault, crossPlain, crossField,
		startInsideCost, startInsideLen, startInsideForb, ringPairs, ringFound, ringNoPath, ringNodeLimit, plainCross, violations);
}

TEST_CASE("NavDanger_Perf")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVDANGER real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	BotCore::NavZoneParams zp;
	BotCore::NavCostLayer elm;
	BotCore::NavBuildTeamZones(grid, 1375.0f, 1085.0f, 622.0f, 911.0f, zp, elm);

	const int n = grid.Size();
	std::vector<BotCore::NavCell> candidates;
	candidates.reserve((size_t)grid.MainComponentCells());
	for (int x = 0; x < n; ++x)
		for (int z = 0; z < n; ++z)
			if (grid.Walk(x, z) && !elm.Forbidden(x, z))
				candidates.push_back(Cell(x, z));
	const int candidateCount = (int)candidates.size();
	REQUIRE(candidateCount > 0);

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
		const int si = (int)rng.NextBelow((uint32_t)candidateCount);
		const int gi = (int)rng.NextBelow((uint32_t)candidateCount);
		if (si == gi)
			continue;
		const int dx = candidates[(size_t)si].x - candidates[(size_t)gi].x;
		const int dz = candidates[(size_t)si].z - candidates[(size_t)gi].z;
		const int adx = dx < 0 ? -dx : dx;
		const int adz = dz < 0 ? -dz : dz;
		if ((adx > adz ? adx : adz) > 64)
			continue;
		starts.push_back(candidates[(size_t)si]);
		goals.push_back(candidates[(size_t)gi]);
	}

	BotCore::NavPathfinder pf;
	BotCore::NavSearchParams sp;
	BotCore::NavCostParams params;
	BotCore::NavCostField field = Field(&elm, params);
	BotCore::NavThreatParams tp;
	BotCore::NavCostLayer dyn;

	std::vector<double> rebuildMs;

	auto runSet = [&](bool threatSet, const char * name)
	{
		for (int q = 0; q < 20; ++q)
		{
			if (threatSet)
			{
				dyn = elm;
				BotCore::NavThreat th[12];
				for (int t = 0; t < 12; ++t)
				{
					const BotCore::NavCell & c = candidates[(size_t)rng.NextBelow((uint32_t)candidateCount)];
					th[t].x = grid.CellCenter(c.x);
					th[t].z = grid.CellCenter(c.z);
					th[t].kind = (t < 6) ? BotCore::NavThreatKind::Melee : BotCore::NavThreatKind::Ranged;
				}
				dyn.AddThreats(th, 12, tp);
				field.layer = &dyn;
			}
			else
			{
				field.layer = &elm;
			}
			BotCore::NavPathResult out;
			pf.Find(grid, starts[(size_t)q], goals[(size_t)q], sp, out, &field);
		}

		std::vector<double> ms;
		std::vector<double> expanded;
		ms.reserve((size_t)queries);
		expanded.reserve((size_t)queries);
		int found = 0;
		int noPath = 0;
		int nodeLimit = 0;
		bool violation = false;

		for (int q = 0; q < queries; ++q)
		{
			if (threatSet)
			{
				const std::chrono::steady_clock::time_point r0 = std::chrono::steady_clock::now();
				dyn = elm;
				BotCore::NavThreat th[12];
				for (int t = 0; t < 12; ++t)
				{
					const BotCore::NavCell & c = candidates[(size_t)rng.NextBelow((uint32_t)candidateCount)];
					th[t].x = grid.CellCenter(c.x);
					th[t].z = grid.CellCenter(c.z);
					th[t].kind = (t < 6) ? BotCore::NavThreatKind::Melee : BotCore::NavThreatKind::Ranged;
				}
				dyn.AddThreats(th, 12, tp);
				const std::chrono::steady_clock::time_point r1 = std::chrono::steady_clock::now();
				rebuildMs.push_back(std::chrono::duration<double, std::milli>(r1 - r0).count());
				field.layer = &dyn;
			}
			else
			{
				field.layer = &elm;
			}

			BotCore::NavPathResult out;
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			pf.Find(grid, starts[(size_t)q], goals[(size_t)q], sp, out, &field);
			const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
			ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
			expanded.push_back((double)out.expanded);

			if (out.status == BotCore::NavPathStatus::Found)
			{
				++found;
				if (ForbiddenOnPath(elm, out.cells) != 0)
					violation = true;
			}
			else if (out.status == BotCore::NavPathStatus::NoPath)
			{
				++noPath;
			}
			else if (out.status == BotCore::NavPathStatus::NodeLimit)
			{
				++nodeLimit;
			}
		}

		std::sort(ms.begin(), ms.end());
		std::sort(expanded.begin(), expanded.end());
		const double p50 = PercentileDouble(ms, 0.50);
		const double p95 = PercentileDouble(ms, 0.95);
		const double p99 = PercentileDouble(ms, 0.99);
		const int exp50 = (int)PercentileDouble(expanded, 0.50);
		const int exp95 = (int)PercentileDouble(expanded, 0.95);

		std::printf("NAVDANGER perf set=%s queries=%d found=%d nopath=%d nodelimit=%d expanded_p50=%d expanded_p95=%d ms_p50=%.3f ms_p95=%.3f ms_p99=%.3f\n",
			name, queries, found, noPath, nodeLimit, exp50, exp95, p50, p95, p99);

		CHECK(!violation);
		CHECK_EQ(found + noPath + nodeLimit, queries);
		if (threatSet)
			CHECK(found * 100 >= queries * 90);
		else
			CHECK(found * 100 >= queries * 95);
#ifndef _DEBUG
		CHECK(p95 <= 2.0);
#else
		(void)p95;
#endif
	};

	runSet(false, "zones");
	runSet(true, "threats");

	std::sort(rebuildMs.begin(), rebuildMs.end());
	const double r50 = PercentileDouble(rebuildMs, 0.50);
	const double r95 = PercentileDouble(rebuildMs, 0.95);
	const double r99 = PercentileDouble(rebuildMs, 0.99);
	std::printf("NAVDANGER perf rebuild samples=%d threats=12 ms_p50=%.3f ms_p95=%.3f ms_p99=%.3f\n",
		(int)rebuildMs.size(), r50, r95, r99);
#ifndef _DEBUG
	CHECK(r95 <= 0.5);
#else
	(void)r95;
#endif
}
