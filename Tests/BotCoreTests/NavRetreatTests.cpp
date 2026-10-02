#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavDanger.h>
#include <BotCore/NavRetreat.h>
#include <BotCore/Rng.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace
{
	using BotCore::NavCell;
	using BotCore::NavGrid;
	using BotCore::NavCostLayer;
	using BotCore::NavThreat;
	using BotCore::NavThreatKind;
	using BotCore::NavRetreatMode;
	using BotCore::NavRetreatParams;
	using BotCore::NavRetreatPlanner;
	using BotCore::NavRetreatQuery;
	using BotCore::NavRetreatResult;
	using BotCore::NavRetreatStatus;

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

	NavThreat Threat(float x, float z, NavThreatKind kind)
	{
		NavThreat t;
		t.x = x;
		t.z = z;
		t.kind = kind;
		return t;
	}

	NavRetreatQuery Query(NavCell start, NavRetreatMode mode, bool hasAnchor, float ax, float az,
		const std::vector<NavThreat> & threats)
	{
		NavRetreatQuery q;
		q.start = start;
		q.mode = mode;
		q.hasAnchor = hasAnchor;
		q.anchorX = ax;
		q.anchorZ = az;
		if (!threats.empty())
		{
			q.threats = threats.data();
			q.threatCount = threats.size();
		}
		return q;
	}

	double PathLength(const NavGrid & grid, const std::vector<NavCell> & cells)
	{
		double length = 0.0;
		const double unit = (double)grid.Unit();
		for (size_t i = 0; i + 1 < cells.size(); ++i)
		{
			const int dx = cells[i + 1].x - cells[i].x;
			const int dz = cells[i + 1].z - cells[i].z;
			length += (dx != 0 && dz != 0) ? (unit * std::sqrt(2.0)) : unit;
		}
		return length;
	}

	// Independent melee rule: squared distance from a cell centre to the nearest Melee threat.
	double MeleeD2(const NavGrid & grid, const std::vector<NavThreat> & threats, NavCell c)
	{
		const double px = ((double)c.x + 0.5) * (double)grid.Unit();
		const double pz = ((double)c.z + 0.5) * (double)grid.Unit();
		double best = std::numeric_limits<double>::max();
		for (size_t i = 0; i < threats.size(); ++i)
		{
			if (threats[i].kind != NavThreatKind::Melee)
				continue;
			const double dx = px - (double)threats[i].x;
			const double dz = pz - (double)threats[i].z;
			const double d2 = dx * dx + dz * dz;
			if (d2 < best)
				best = d2;
		}
		return best;
	}

	// Independent edge rule (does not share code with the planner): Walks, no corner cutting via
	// EdgeOpen, no outside -> forbidden entry, and no step closer to a melee while in its zone.
	bool StepAllowed(const NavGrid & grid, const NavCostLayer * zones, const std::vector<NavThreat> & threats,
		double clearM, NavCell a, NavCell b)
	{
		const int dx = b.x - a.x;
		const int dz = b.z - a.z;
		if (dx < -1 || dx > 1 || dz < -1 || dz > 1 || (dx == 0 && dz == 0))
			return false;
		if (!grid.EdgeOpen(a.x, a.z, dx, dz))
			return false;
		if (zones != nullptr && zones->Size() == grid.Size()
			&& zones->Forbidden(b.x, b.z) && !zones->Forbidden(a.x, a.z))
			return false;

		const double clear2 = clearM * clearM;
		const double db = MeleeD2(grid, threats, b);
		if (db <= clear2)
		{
			const double da = MeleeD2(grid, threats, a);
			if (da > clear2)
				return false;   // entering the zone from outside
			if (db < da)
				return false;   // stepping closer while inside
		}
		return true;
	}

	double RetreatRadius(const NavRetreatParams & params, NavRetreatMode mode)
	{
		double r = (mode == NavRetreatMode::Party) ? (double)params.partyRadiusM : (double)params.soloRadiusM;
		return r > 0.0 ? r : 0.0;
	}

	void CheckResult(const NavGrid & grid, const NavCostLayer * zones, const std::vector<NavThreat> & threats,
		const NavRetreatQuery & query, const NavRetreatParams & params, const NavRetreatResult & out)
	{
		const NavCostLayer * z = (zones != nullptr && zones->Size() == grid.Size()) ? zones : nullptr;
		if (out.status == NavRetreatStatus::Found)
		{
			REQUIRE(!out.path.empty());
			CHECK(out.path.front() == query.start);
			CHECK(out.path.back() == out.cell);

			const double clearM = params.meleeClearM > 0.0f ? (double)params.meleeClearM : 0.0;
			const double clear2 = clearM * clearM;
			for (size_t i = 0; i + 1 < out.path.size(); ++i)
			{
				const int dx = out.path[i + 1].x - out.path[i].x;
				const int dz = out.path[i + 1].z - out.path[i].z;
				CHECK(dx >= -1 && dx <= 1 && dz >= -1 && dz <= 1 && !(dx == 0 && dz == 0));
				CHECK(StepAllowed(grid, zones, threats, clearM, out.path[i], out.path[i + 1]));
			}
			for (size_t i = 1; i < out.path.size(); ++i)
				CHECK(!(z != nullptr && z->Forbidden(out.path[i].x, out.path[i].z)
					&& !z->Forbidden(out.path[i - 1].x, out.path[i - 1].z)));

			CHECK(!(z != nullptr && z->Forbidden(out.cell.x, out.cell.z)));
			CHECK(MeleeD2(grid, threats, out.cell) > clear2);
			CHECK((double)out.pathLengthM <= RetreatRadius(params, query.mode) + 1e-3);
			CHECK(std::fabs(PathLength(grid, out.path) - (double)out.pathLengthM) <= 1e-3);
			CHECK_EQ((int)out.danger, (int)(z != nullptr ? z->Danger(out.cell.x, out.cell.z) : 0));
			CHECK(out.safe == (z != nullptr && z->Safe(out.cell.x, out.cell.z)));
			CHECK(out.candidates >= 1);
			CHECK(out.expanded >= out.candidates);
		}
		else
		{
			CHECK(out.path.empty());
			CHECK_EQ(out.candidates, 0);
		}
	}

	// Independent reference result (relaxation; shares no heap/structures with the planner).
	struct RefOut
	{
		int code = 1;              // 0 found, 1 no candidate, 2 invalid start
		double bestScore = 0.0;
		int candidates = 0;
		const NavGrid * grid = nullptr;
		const NavCostLayer * zones = nullptr;
		const NavRetreatParams * params = nullptr;
		NavRetreatMode mode = NavRetreatMode::Party;
		bool hasAnchor = false;
		float anchorX = 0.0f;
		float anchorZ = 0.0f;
		int bx0 = 0;
		int bx1 = -1;
		int bz0 = 0;
		int bz1 = -1;
		std::vector<double> dist;

		int Width() const { return bz1 - bz0 + 1; }

		double DistOf(NavCell c) const
		{
			if (grid == nullptr || c.x < bx0 || c.x > bx1 || c.z < bz0 || c.z > bz1)
				return std::numeric_limits<double>::max();
			return dist[(size_t)(c.x - bx0) * (size_t)Width() + (size_t)(c.z - bz0)];
		}

		double ScoreOf(NavCell c) const;
	};

	double RefOut::ScoreOf(NavCell c) const
	{
		const NavCostLayer * z = (zones != nullptr && zones->Size() == grid->Size()) ? zones : nullptr;
		const double wD = params->wDanger > 0.0f ? (double)params->wDanger : 0.0;
		const double wP = params->wPath > 0.0f ? (double)params->wPath : 0.0;
		const double wA = params->wAnchor > 0.0f ? (double)params->wAnchor : 0.0;
		const double wC = params->wClear > 0.0f ? (double)params->wClear : 0.0;
		const double wS = params->wSafe > 0.0f ? (double)params->wSafe : 0.0;

		double s = 0.0;
		const int danger = z != nullptr ? (int)z->Danger(c.x, c.z) : 0;
		s -= wD * (double)danger / 255.0;

		const double R = RetreatRadius(*params, mode);
		if (R > 0.0)
		{
			s -= wP * (DistOf(c) / R);
			if (hasAnchor)
			{
				const double px = ((double)c.x + 0.5) * (double)grid->Unit();
				const double pz = ((double)c.z + 0.5) * (double)grid->Unit();
				const double ax = px - (double)anchorX;
				const double az = pz - (double)anchorZ;
				const double da = std::sqrt(ax * ax + az * az);
				double closeness = 1.0 - da / R;
				if (closeness < 0.0)
					closeness = 0.0;
				s += wA * closeness;
			}
		}
		const int cl = (int)grid->Clearance(c.x, c.z);
		s += wC * (double)(cl < 3 ? cl : 3) / 3.0;
		if (z != nullptr && z->Safe(c.x, c.z))
			s += wS;
		return s;
	}

	RefOut RefRetreat(const NavGrid & grid, const NavCostLayer * zones, const std::vector<NavThreat> & threats,
		const NavRetreatParams & params, const NavRetreatQuery & query)
	{
		RefOut ref;
		ref.grid = &grid;
		ref.zones = zones;
		ref.params = &params;
		ref.mode = query.mode;
		ref.hasAnchor = query.hasAnchor;
		ref.anchorX = query.anchorX;
		ref.anchorZ = query.anchorZ;

		const int n = grid.Size();
		if (!grid.Walk(query.start.x, query.start.z))
		{
			ref.code = 2;
			return ref;
		}

		const double R = RetreatRadius(params, query.mode);
		const double clearM = params.meleeClearM > 0.0f ? (double)params.meleeClearM : 0.0;
		const double clear2 = clearM * clearM;
		const double unit = (double)grid.Unit();

		const int margin = (int)std::ceil(R / unit) + 2;
		ref.bx0 = 0 > (query.start.x - margin) ? 0 : (query.start.x - margin);
		ref.bx1 = (n - 1) < (query.start.x + margin) ? (n - 1) : (query.start.x + margin);
		ref.bz0 = 0 > (query.start.z - margin) ? 0 : (query.start.z - margin);
		ref.bz1 = (n - 1) < (query.start.z + margin) ? (n - 1) : (query.start.z + margin);
		ref.dist.assign((size_t)(ref.bx1 - ref.bx0 + 1) * (size_t)(ref.bz1 - ref.bz0 + 1),
			std::numeric_limits<double>::max());

		const double big = std::numeric_limits<double>::max();
		auto D = [&](int x, int z) -> double &
		{
			return ref.dist[(size_t)(x - ref.bx0) * (size_t)ref.Width() + (size_t)(z - ref.bz0)];
		};

		std::vector<int> queue;
		std::vector<uint8_t> inQueue((size_t)(ref.bx1 - ref.bx0 + 1) * (size_t)(ref.bz1 - ref.bz0 + 1), 0);
		auto QIdx = [&](int x, int z) -> size_t
		{
			return (size_t)(x - ref.bx0) * (size_t)ref.Width() + (size_t)(z - ref.bz0);
		};

		D(query.start.x, query.start.z) = 0.0;
		queue.push_back(query.start.x * n + query.start.z);
		inQueue[QIdx(query.start.x, query.start.z)] = 1;

		const int dxs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		const int dzs[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

		size_t head = 0;
		while (head < queue.size())
		{
			const int curIdx = queue[head++];
			const int cx = curIdx / n;
			const int cz = curIdx % n;
			inQueue[QIdx(cx, cz)] = 0;
			const double dc = D(cx, cz);
			if (dc >= big)
				continue;

			for (int k = 0; k < 8; ++k)
			{
				const int nx = cx + dxs[k];
				const int nz = cz + dzs[k];
				if (nx < ref.bx0 || nx > ref.bx1 || nz < ref.bz0 || nz > ref.bz1)
					continue;
				if (!StepAllowed(grid, zones, threats, clearM, Cell(cx, cz), Cell(nx, nz)))
					continue;
				const double step = (dxs[k] != 0 && dzs[k] != 0) ? (unit * std::sqrt(2.0)) : unit;
				const double nd = dc + step;
				if (nd <= R + 1e-9 && nd < D(nx, nz) - 1e-12)
				{
					D(nx, nz) = nd;
					if (!inQueue[QIdx(nx, nz)])
					{
						inQueue[QIdx(nx, nz)] = 1;
						queue.push_back(nx * n + nz);
					}
				}
			}
		}

		ref.bestScore = -std::numeric_limits<double>::max();
		for (int x = ref.bx0; x <= ref.bx1; ++x)
		{
			for (int z = ref.bz0; z <= ref.bz1; ++z)
			{
				const double d = D(x, z);
				if (!(d <= R + 1e-9))
					continue;
				if (zones != nullptr && zones->Size() == n && zones->Forbidden(x, z))
					continue;
				if (MeleeD2(grid, threats, Cell(x, z)) <= clear2)
					continue;
				++ref.candidates;
				const double s = ref.ScoreOf(Cell(x, z));
				if (s > ref.bestScore)
					ref.bestScore = s;
			}
		}

		if (ref.candidates == 0)
		{
			ref.code = 1;
			return ref;
		}
		ref.code = 0;
		return ref;
	}

	// Picks a Walk cell within `maxCheb` of `start` (random retries); falls back to `start`.
	void FindWalkCellNear(BotCore::Rng & rng, const NavGrid & grid, NavCell start, int maxCheb, NavCell & out)
	{
		const uint32_t span = (uint32_t)(2 * maxCheb + 1);
		for (int attempt = 0; attempt < 100000; ++attempt)
		{
			const int dx = -maxCheb + (int)rng.NextBelow(span);
			const int dz = -maxCheb + (int)rng.NextBelow(span);
			const int x = start.x + dx;
			const int z = start.z + dz;
			if (grid.Walk(x, z))
			{
				out = Cell(x, z);
				return;
			}
		}
		out = start;
	}

	bool SameResult(const NavRetreatResult & a, const NavRetreatResult & b)
	{
		return a.status == b.status && a.cell == b.cell && a.score == b.score
			&& a.pathLengthM == b.pathLengthM && a.danger == b.danger && a.safe == b.safe
			&& a.path == b.path && a.candidates == b.candidates && a.expanded == b.expanded;
	}
}

TEST_CASE("NavRetreat_Basics")
{
	const float unit = 4.0f;

	// (a) Defaults.
	{
		NavRetreatParams params;
		CHECK_EQ(params.partyRadiusM, 40.0f);
		CHECK_EQ(params.soloRadiusM, 150.0f);
		CHECK_EQ(params.meleeClearM, 8.0f);
		CHECK_EQ(params.wDanger, 3.0f);
		CHECK_EQ(params.wPath, 1.0f);
		CHECK_EQ(params.wAnchor, 1.5f);
		CHECK_EQ(params.wClear, 0.5f);
		CHECK_EQ(params.wSafe, 1.0f);

		NavRetreatQuery q;
		CHECK(q.mode == NavRetreatMode::Party);
		CHECK(!q.hasAnchor);
		CHECK(q.threats == nullptr);
		CHECK_EQ((int)q.threatCount, 0);
	}

	const std::vector<NavThreat> noThreats;
	NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
	NavRetreatPlanner planner;
	NavRetreatResult out;
	NavRetreatParams params;

	// (b) InvalidStart: walls, out of bounds and an un-Built grid.
	{
		const NavCell bad[3] = { Cell(0, 0), Cell(-1, 5), Cell(40, 5) };
		for (int i = 0; i < 3; ++i)
		{
			NavRetreatQuery q = Query(bad[i], NavRetreatMode::Party, false, 0.0f, 0.0f, noThreats);
			planner.Find(grid, nullptr, q, params, out);
			CHECK(out.status == NavRetreatStatus::InvalidStart);
			CHECK(out.path.empty());
			CHECK_EQ(out.candidates, 0);
			CHECK_EQ(out.expanded, 0);
			CheckResult(grid, nullptr, noThreats, q, params, out);
		}

		NavGrid g0;
		NavRetreatQuery q = Query(Cell(5, 5), NavRetreatMode::Party, false, 0.0f, 0.0f, noThreats);
		planner.Find(g0, nullptr, q, params, out);
		CHECK(out.status == NavRetreatStatus::InvalidStart);
		CHECK(out.path.empty());
		CHECK_EQ(out.candidates, 0);
		CHECK_EQ(out.expanded, 0);
	}

	// (c) Table 1.
	{
		// A party / solo.
		NavRetreatQuery qa = Query(Cell(20, 20), NavRetreatMode::Party, false, 0.0f, 0.0f, noThreats);
		planner.Find(grid, nullptr, qa, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(20, 20));
		CHECK(std::fabs(out.score - 0.5f) <= 1e-4f);
		CHECK(std::fabs(out.pathLengthM - 0.0f) <= 1e-4f);
		CHECK_EQ((int)out.path.size(), 1);
		CHECK_EQ(out.candidates, 285);
		CHECK_EQ(out.expanded, 285);
		CheckResult(grid, nullptr, noThreats, qa, params, out);

		NavRetreatQuery qas = Query(Cell(20, 20), NavRetreatMode::Solo, false, 0.0f, 0.0f, noThreats);
		planner.Find(grid, nullptr, qas, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(20, 20));
		CHECK(std::fabs(out.score - 0.5f) <= 1e-4f);
		CHECK_EQ(out.candidates, 1444);
		CHECK_EQ(out.expanded, 1444);
		CheckResult(grid, nullptr, noThreats, qas, params, out);

		// B party with an anchor at the cell centre; solo score too.
		NavRetreatQuery qb = Query(Cell(20, 20), NavRetreatMode::Party, true, 114.0f, 82.0f, noThreats);
		planner.Find(grid, nullptr, qb, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(28, 20));
		CHECK(std::fabs(out.score - 1.2f) <= 1e-4f);
		CHECK(std::fabs(out.pathLengthM - 32.0f) <= 1e-3f);
		CHECK_EQ((int)out.path.size(), 9);
		CheckResult(grid, nullptr, noThreats, qb, params, out);

		NavRetreatQuery qbs = Query(Cell(20, 20), NavRetreatMode::Solo, true, 114.0f, 82.0f, noThreats);
		planner.Find(grid, nullptr, qbs, params, out);
		CHECK(out.cell == Cell(28, 20));
		CHECK(std::fabs(out.score - 1.78667f) <= 2e-3f);
		CheckResult(grid, nullptr, noThreats, qbs, params, out);

		// Radius boundary inclusive: anchor at (30, 20).
		NavRetreatQuery qbnd = Query(Cell(20, 20), NavRetreatMode::Party, true, 122.0f, 82.0f, noThreats);
		planner.Find(grid, nullptr, qbnd, params, out);
		CHECK(out.cell == Cell(30, 20));
		CHECK(std::fabs(out.score - 1.0f) <= 1e-4f);
		CHECK(std::fabs(out.pathLengthM - 40.0f) <= 1e-3f);
		CHECK_EQ((int)out.path.size(), 11);
		CheckResult(grid, nullptr, noThreats, qbnd, params, out);

		NavRetreatParams p399 = params;
		p399.partyRadiusM = 39.9f;
		planner.Find(grid, nullptr, qbnd, p399, out);
		CHECK(out.cell == Cell(29, 20));
		CHECK(std::fabs(out.score - 0.94737f) <= 2e-3f);
		CHECK(std::fabs(out.pathLengthM - 36.0f) <= 1e-3f);
		CHECK_EQ(out.candidates, 281);
		CheckResult(grid, nullptr, noThreats, qbnd, p399, out);

		// Small radius and non-positive radius.
		NavRetreatParams p20 = params;
		p20.partyRadiusM = 20.0f;
		NavRetreatQuery qn = Query(Cell(20, 20), NavRetreatMode::Party, false, 0.0f, 0.0f, noThreats);
		planner.Find(grid, nullptr, qn, p20, out);
		CHECK(out.cell == Cell(20, 20));
		CHECK_EQ(out.candidates, 73);
		CHECK_EQ(out.expanded, 73);
		CheckResult(grid, nullptr, noThreats, qn, p20, out);

		const float zeroRadii[2] = { 0.0f, -5.0f };
		for (int i = 0; i < 2; ++i)
		{
			NavRetreatParams pz = params;
			pz.partyRadiusM = zeroRadii[i];
			planner.Find(grid, nullptr, qn, pz, out);
			CHECK(out.status == NavRetreatStatus::Found);
			CHECK(out.cell == Cell(20, 20));
			CHECK(std::fabs(out.score - 0.5f) <= 1e-4f);
			CHECK_EQ(out.candidates, 1);
			CHECK_EQ(out.expanded, 1);
			CheckResult(grid, nullptr, noThreats, qn, pz, out);
		}

		// All-zero weights: the start wins as the shortest path among tied scores.
		NavRetreatParams pZero = params;
		pZero.wDanger = 0.0f;
		pZero.wPath = 0.0f;
		pZero.wClear = 0.0f;
		planner.Find(grid, nullptr, qn, pZero, out);
		CHECK(out.cell == Cell(20, 20));
		CHECK(std::fabs(out.score) <= 1e-6f);
		CHECK_EQ(out.candidates, 285);
		CheckResult(grid, nullptr, noThreats, qn, pZero, out);

		// Four equal candidates: the shortest path to the anchor corner wins.
		NavRetreatParams pFour = params;
		pFour.wDanger = 0.0f;
		pFour.wPath = 0.0f;
		pFour.wClear = 0.0f;
		pFour.wAnchor = 1.0f;
		NavRetreatQuery qfour = Query(Cell(15, 19), NavRetreatMode::Party, true, 80.0f, 80.0f, noThreats);
		planner.Find(grid, nullptr, qfour, pFour, out);
		CHECK(out.cell == Cell(19, 19));
		CHECK(std::fabs(out.score - 0.92929f) <= 1e-4f);
		CHECK_EQ((int)out.path.size(), 5);
		CheckResult(grid, nullptr, noThreats, qfour, pFour, out);

		// Negative weights count as 0.
		NavRetreatParams pNeg = params;
		pNeg.wPath = -5.0f;
		pNeg.wDanger = -1.0f;
		planner.Find(grid, nullptr, qn, pNeg, out);
		CHECK(out.cell == Cell(20, 20));
		CHECK(std::fabs(out.score - 0.5f) <= 1e-4f);
		CheckResult(grid, nullptr, noThreats, qn, pNeg, out);
	}
}

TEST_CASE("NavRetreat_Params")
{
	const float unit = 4.0f;
	NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
	NavRetreatPlanner planner;
	NavRetreatResult out;
	NavRetreatParams params;

	const std::vector<NavThreat> threats(1, Threat(82.0f, 82.0f, NavThreatKind::Melee));
	BotCore::NavThreatParams tp;
	NavCostLayer layer;
	layer.Init(grid);
	layer.AddThreats(threats.data(), threats.size(), tp);

	// (a) wDanger == 0: the nearest off-zone cell with the highest score.
	{
		NavRetreatParams p = params;
		p.wDanger = 0.0f;
		NavRetreatQuery q = Query(Cell(20, 21), NavRetreatMode::Party, false, 0.0f, 0.0f, threats);
		planner.Find(grid, &layer, q, p, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(19, 22));
		CHECK(std::fabs(out.score - 0.35858f) <= 2e-3f);
		CHECK(std::fabs(out.pathLengthM - 5.6569f) <= 1e-3f);
		CHECK_EQ((int)out.path.size(), 2);
		CHECK_EQ((int)out.danger, 255);
		CheckResult(grid, &layer, threats, q, p, out);
	}

	// (b) meleeClearM == 10.
	{
		NavRetreatParams p = params;
		p.meleeClearM = 10.0f;
		NavRetreatQuery q = Query(Cell(20, 21), NavRetreatMode::Party, false, 0.0f, 0.0f, threats);
		planner.Find(grid, &layer, q, p, out);
		CHECK(out.cell == Cell(20, 26));
		CHECK_EQ(out.candidates, 263);
		CheckResult(grid, &layer, threats, q, p, out);
	}

	// (c) meleeClearM == 0 (and negative): only the threat centre cell is in the zone.
	{
		const float clearVals[2] = { 0.0f, -3.0f };
		for (int i = 0; i < 2; ++i)
		{
			NavRetreatParams p = params;
			p.meleeClearM = clearVals[i];
			NavRetreatQuery q = Query(Cell(20, 21), NavRetreatMode::Party, false, 0.0f, 0.0f, threats);
			planner.Find(grid, &layer, q, p, out);
			CHECK(out.status == NavRetreatStatus::Found);
			CHECK(out.cell == Cell(20, 26));
			CHECK(std::fabs(out.score) <= 1e-3f);
			CHECK(std::fabs(out.pathLengthM - 20.0f) <= 1e-3f);
			CHECK_EQ(out.candidates, 283);
			CHECK_EQ(out.expanded, 283);
			CheckResult(grid, &layer, threats, q, p, out);
		}
	}

	// (d) A layer of another size is ignored: same as no layer.
	{
		NavGrid grid30 = MakeNav(30, unit, RingEvents(30), HeightZeros(30));
		NavCostLayer other;
		other.Init(grid30);
		other.AddDangerBand(60.0f, 60.0f, 0.0f, 15.0f, 8.0f, 1.0f);
		other.AddForbidDisc(60.0f, 60.0f, 12.0f);
		other.AddSafeDisc(20.0f, 20.0f, 12.0f);

		NavRetreatQuery q = Query(Cell(20, 20), NavRetreatMode::Party, false, 0.0f, 0.0f, std::vector<NavThreat>());
		planner.Find(grid, &other, q, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(20, 20));
		CHECK(std::fabs(out.score - 0.5f) <= 1e-4f);
		CHECK_EQ(out.candidates, 285);
		CheckResult(grid, &other, std::vector<NavThreat>(), q, params, out);
	}

	// (e) Threat list handling: no pointer / count 0 means no 8 m rule.
	{
		NavRetreatQuery q = Query(Cell(20, 21), NavRetreatMode::Party, false, 0.0f, 0.0f, std::vector<NavThreat>());
		// A non-null pointer with count 0 must behave the same.
		q.threats = threats.data();
		q.threatCount = 0;
		planner.Find(grid, &layer, q, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(20, 26));
		CHECK(std::fabs(out.score) <= 1e-3f);
		CHECK(std::fabs(out.pathLengthM - 20.0f) <= 1e-3f);
		CHECK_EQ(out.candidates, 285);
		CHECK_EQ(out.expanded, 285);
		CheckResult(grid, &layer, std::vector<NavThreat>(), q, params, out);

		// nullptr with a non-zero count means the same (no threat data is read).
		q.threats = nullptr;
		q.threatCount = 3;
		planner.Find(grid, &layer, q, params, out);
		CHECK(out.cell == Cell(20, 26));
		CHECK_EQ(out.candidates, 285);
		CheckResult(grid, &layer, std::vector<NavThreat>(), q, params, out);
	}

	// (f) Only Ranged threats: no 8 m clear rule (Table 2 C1c shape is in _Melee_Rule).
	{
		std::vector<NavThreat> ranged(1, Threat(82.0f, 82.0f, NavThreatKind::Ranged));
		NavRetreatParams p = params;
		NavRetreatQuery q = Query(Cell(20, 21), NavRetreatMode::Party, false, 0.0f, 0.0f, ranged);
		planner.Find(grid, &layer, q, p, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.candidates == 285 || out.candidates == 281);
		CheckResult(grid, &layer, ranged, q, p, out);
	}
}

TEST_CASE("NavRetreat_Melee_Rule")
{
	const float unit = 4.0f;
	NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
	NavGrid corridor = MakeNav(40, unit, GapWall(40, 20, 19, 20), HeightZeros(40));
	NavRetreatPlanner planner;
	NavRetreatResult out;
	NavRetreatParams params;
	BotCore::NavThreatParams tp;

	// Table 2 C0: no layer, no threats.
	{
		const std::vector<NavThreat> none;
		NavRetreatQuery q = Query(Cell(16, 19), NavRetreatMode::Party, true, 98.0f, 78.0f, none);
		planner.Find(corridor, nullptr, q, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(24, 19));
		CHECK(std::fabs(out.score - 1.2f) <= 1e-4f);
		CHECK(std::fabs(out.pathLengthM - 32.0f) <= 1e-3f);
		CHECK_EQ(out.candidates, 254);
		CheckResult(corridor, nullptr, none, q, params, out);
	}

	// Table 2 C1: melee band + melee threat list; the path turns away from the gap.
	{
		const std::vector<NavThreat> th(1, Threat(82.0f, 78.0f, NavThreatKind::Melee));
		NavCostLayer band;
		band.Init(corridor);
		band.AddThreats(th.data(), th.size(), tp);

		NavRetreatQuery q = Query(Cell(16, 19), NavRetreatMode::Party, true, 98.0f, 78.0f, th);
		planner.Find(corridor, &band, q, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(14, 19));
		CHECK(std::fabs(out.score - 0.3f) <= 1e-3f);
		CHECK(std::fabs(out.pathLengthM - 8.0f) <= 1e-3f);
		CHECK_EQ(out.candidates, 204);
		CHECK_EQ(out.expanded, 204);
		for (size_t i = 0; i < out.path.size(); ++i)
		{
			CHECK(out.path[i].x != 20);
			const float dx = ((float)out.path[i].x + 0.5f) * unit - 82.0f;
			const float dz = ((float)out.path[i].z + 0.5f) * unit - 78.0f;
			CHECK(std::sqrt(dx * dx + dz * dz) > 8.0f);
		}
		CheckResult(corridor, &band, th, q, params, out);

		// C1b: same layer, empty threat list: the 8 m rule comes from the list, not the layer.
		const std::vector<NavThreat> none;
		NavRetreatQuery qb = Query(Cell(16, 19), NavRetreatMode::Party, true, 98.0f, 78.0f, none);
		planner.Find(corridor, &band, qb, params, out);
		CHECK(out.cell == Cell(26, 19));
		CHECK(std::fabs(out.score - 0.7f) <= 1e-3f);
		CHECK(std::fabs(out.pathLengthM - 40.0f) <= 1e-3f);
		CHECK_EQ(out.candidates, 254);
		CheckResult(corridor, &band, none, qb, params, out);

		// C1c: ranged band and ranged threat list: no 8 m rule, the far side wins.
		const std::vector<NavThreat> rth(1, Threat(82.0f, 78.0f, NavThreatKind::Ranged));
		NavCostLayer rband;
		rband.Init(corridor);
		rband.AddThreats(rth.data(), rth.size(), tp);
		NavRetreatQuery qc = Query(Cell(16, 19), NavRetreatMode::Party, true, 98.0f, 78.0f, rth);
		planner.Find(corridor, &rband, qc, params, out);
		CHECK(out.cell == Cell(6, 19));
		CHECK(std::fabs(out.score - (-0.5f)) <= 1e-3f);
		CHECK(std::fabs(out.pathLengthM - 40.0f) <= 1e-3f);
		CHECK_EQ(out.candidates, 254);
		CheckResult(corridor, &rband, rth, qc, params, out);
	}

	// Table 3 D: start inside the melee zone; leave by moving away.
	const std::vector<NavThreat> melee(1, Threat(82.0f, 82.0f, NavThreatKind::Melee));
	NavCostLayer mband;
	mband.Init(grid);
	mband.AddThreats(melee.data(), melee.size(), tp);
	{
		NavRetreatQuery q = Query(Cell(20, 21), NavRetreatMode::Party, false, 0.0f, 0.0f, melee);
		planner.Find(grid, &mband, q, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(20, 26));
		CHECK(std::fabs(out.score) <= 1e-3f);
		CHECK(std::fabs(out.pathLengthM - 20.0f) <= 1e-3f);
		CHECK_EQ((int)out.danger, 0);
		CHECK_EQ((int)out.path.size(), 6);
		CHECK_EQ(out.candidates, 271);
		CHECK_EQ(out.expanded, 283);
		for (size_t i = 0; i + 1 < out.path.size(); ++i)
			CHECK(MeleeD2(grid, melee, out.path[i + 1]) >= MeleeD2(grid, melee, out.path[i]));
		CheckResult(grid, &mband, melee, q, params, out);

		NavRetreatQuery qs = Query(Cell(20, 21), NavRetreatMode::Solo, false, 0.0f, 0.0f, melee);
		planner.Find(grid, &mband, qs, params, out);
		CHECK(out.cell == Cell(20, 26));
		CHECK(std::fabs(out.score - 0.36667f) <= 2e-3f);
		CHECK_EQ(out.candidates, 1431);
		CHECK_EQ(out.expanded, 1443);
		CheckResult(grid, &mband, melee, qs, params, out);

		// D wDanger == 0: the nearest high-danger cell wins.
		NavRetreatParams pw = params;
		pw.wDanger = 0.0f;
		NavRetreatQuery qw = Query(Cell(20, 21), NavRetreatMode::Party, false, 0.0f, 0.0f, melee);
		planner.Find(grid, &mband, qw, pw, out);
		CHECK(out.cell == Cell(19, 22));
		CHECK(std::fabs(out.score - 0.35858f) <= 2e-3f);
		CHECK(std::fabs(out.pathLengthM - 5.6569f) <= 1e-3f);
		CHECK_EQ((int)out.path.size(), 2);
		CheckResult(grid, &mband, melee, qw, pw, out);

		// D meleeClearM == 10.
		NavRetreatParams pc = params;
		pc.meleeClearM = 10.0f;
		NavRetreatQuery qc = Query(Cell(20, 21), NavRetreatMode::Party, false, 0.0f, 0.0f, melee);
		planner.Find(grid, &mband, qc, pc, out);
		CHECK(out.cell == Cell(20, 26));
		CHECK_EQ(out.candidates, 263);
		CheckResult(grid, &mband, melee, qc, pc, out);
	}

	// Table 3 D2: walk around the melee, never through it.
	{
		NavRetreatQuery q = Query(Cell(21, 20), NavRetreatMode::Party, true, 50.0f, 82.0f, melee);
		planner.Find(grid, &mband, q, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(12, 20));
		CHECK(std::fabs(out.score - 1.01716f) <= 2e-3f);
		CHECK(std::fabs(out.pathLengthM - 39.3137f) <= 1e-2f);
		CHECK_EQ((int)out.path.size(), 10);
		CHECK_EQ(out.candidates, 271);
		CHECK_EQ(out.expanded, 283);
		for (size_t i = 0; i < out.path.size(); ++i)
			CHECK(out.path[i] != Cell(20, 20));
		CheckResult(grid, &mband, melee, q, params, out);
	}

	// Table 3 NoCandidate: start on the melee with a tiny radius.
	{
		NavRetreatParams p = params;
		p.partyRadiusM = 2.0f;
		NavRetreatQuery q = Query(Cell(20, 20), NavRetreatMode::Party, false, 0.0f, 0.0f, melee);
		planner.Find(grid, &mband, q, p, out);
		CHECK(out.status == NavRetreatStatus::NoCandidate);
		CHECK_EQ(out.candidates, 0);
		CHECK_EQ(out.expanded, 1);
		CHECK(out.path.empty());
		CheckResult(grid, &mband, melee, q, p, out);
	}
}

TEST_CASE("NavRetreat_Zones")
{
	const float unit = 4.0f;
	NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
	NavRetreatPlanner planner;
	NavRetreatResult out;
	NavRetreatParams params;
	const std::vector<NavThreat> none;

	// Table 4 E/F2: forbidden disc at Cw(30,20), safe disc at Cw(8,20).
	NavCostLayer zones;
	zones.Init(grid);
	zones.AddForbidDisc(122.0f, 82.0f, 12.0f);
	zones.AddSafeDisc(34.0f, 82.0f, 12.0f);
	{
		NavRetreatQuery q = Query(Cell(20, 20), NavRetreatMode::Solo, true, 34.0f, 82.0f, none);
		planner.Find(grid, &zones, q, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(8, 20));
		CHECK(std::fabs(out.score - 2.68f) <= 2e-3f);
		CHECK(std::fabs(out.pathLengthM - 48.0f) <= 1e-3f);
		CHECK_EQ((int)out.path.size(), 13);
		CHECK_EQ(out.candidates, 1415);
		CHECK_EQ(out.expanded, 1415);
		CHECK(out.safe);
		CheckResult(grid, &zones, none, q, params, out);

		NavRetreatParams pz = params;
		pz.wSafe = 0.0f;
		planner.Find(grid, &zones, q, pz, out);
		CHECK(out.cell == Cell(8, 20));
		CHECK(std::fabs(out.score - 1.68f) <= 2e-3f);
		CheckResult(grid, &zones, none, q, pz, out);

		NavRetreatQuery qp = Query(Cell(20, 20), NavRetreatMode::Party, true, 34.0f, 82.0f, none);
		planner.Find(grid, &zones, qp, params, out);
		CHECK(out.cell == Cell(10, 20));
		CHECK(std::fabs(out.score - 1.7f) <= 2e-3f);
		CHECK(std::fabs(out.pathLengthM - 40.0f) <= 1e-3f);
		CHECK_EQ(out.candidates, 273);
		CHECK_EQ(out.expanded, 273);
		CHECK(out.safe);
		CheckResult(grid, &zones, none, qp, params, out);

		NavRetreatQuery q2 = Query(Cell(20, 20), NavRetreatMode::Solo, false, 0.0f, 0.0f, none);
		planner.Find(grid, &zones, q2, params, out);
		CHECK(out.cell == Cell(11, 20));
		CHECK(std::fabs(out.score - 1.26f) <= 2e-3f);
		CHECK(std::fabs(out.pathLengthM - 36.0f) <= 1e-3f);
		CHECK(out.safe);
		CheckResult(grid, &zones, none, q2, params, out);
	}

	// Table 4 F: a large forbidden disc; a start inside may leave, never re-enter.
	NavCostLayer forbid;
	forbid.Init(grid);
	forbid.AddForbidDisc(82.0f, 82.0f, 20.0f);
	{
		NavRetreatQuery q = Query(Cell(20, 20), NavRetreatMode::Solo, false, 0.0f, 0.0f, none);
		planner.Find(grid, &forbid, q, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(15, 19));
		CHECK(std::fabs(out.score - 0.35562f) <= 1e-3f);
		CHECK(std::fabs(out.pathLengthM - 21.6569f) <= 1e-2f);
		CHECK_EQ(out.candidates, 1363);
		CHECK_EQ(out.expanded, 1444);
		int forbiddenOnPath = 0;
		for (size_t i = 0; i < out.path.size(); ++i)
			if (forbid.Forbidden(out.path[i].x, out.path[i].z))
				++forbiddenOnPath;
		CHECK_EQ(forbiddenOnPath, 5);
		CHECK(!forbid.Forbidden(out.path.back().x, out.path.back().z));
		bool left = false;
		bool prefix = true;
		for (size_t i = 0; i < out.path.size(); ++i)
		{
			if (forbid.Forbidden(out.path[i].x, out.path[i].z))
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
		CheckResult(grid, &forbid, none, q, params, out);

		NavRetreatQuery qp = Query(Cell(20, 20), NavRetreatMode::Party, false, 0.0f, 0.0f, none);
		planner.Find(grid, &forbid, qp, params, out);
		CHECK(out.cell == Cell(15, 19));
		CHECK(std::fabs(out.score - (-0.04142f)) <= 1e-3f);
		CHECK_EQ(out.candidates, 204);
		CHECK_EQ(out.expanded, 285);
		CheckResult(grid, &forbid, none, qp, params, out);

		NavRetreatParams ps = params;
		ps.soloRadiusM = 2.0f;
		planner.Find(grid, &forbid, q, ps, out);
		CHECK(out.status == NavRetreatStatus::NoCandidate);
		CHECK_EQ(out.candidates, 0);
		CHECK_EQ(out.expanded, 1);
		CHECK(out.path.empty());
	}

	// Start outside a forbidden disc: never enter it.
	{
		NavCostLayer small;
		small.Init(grid);
		small.AddForbidDisc(82.0f, 82.0f, 12.0f);
		NavRetreatQuery q = Query(Cell(8, 20), NavRetreatMode::Solo, true, 130.0f, 82.0f, none);
		planner.Find(grid, &small, q, params, out);
		CHECK(out.status == NavRetreatStatus::Found);
		CHECK(out.cell == Cell(32, 20));
		CHECK(std::fabs(out.score - 1.27163f) <= 2e-3f);
		CHECK(std::fabs(out.pathLengthM - 109.2548f) <= 0.02f);
		CHECK_EQ((int)out.path.size(), 25);
		CHECK_EQ(out.candidates, 1414);
		CHECK_EQ(out.expanded, 1414);
		CHECK(!out.safe);
		int forbiddenOnPath = 0;
		for (size_t i = 0; i < out.path.size(); ++i)
			if (small.Forbidden(out.path[i].x, out.path[i].z))
				++forbiddenOnPath;
		CHECK_EQ(forbiddenOnPath, 0);
		CheckResult(grid, &small, none, q, params, out);
	}
}

TEST_CASE("NavRetreat_Matches_Reference")
{
	const float unit = 4.0f;
	NavRetreatPlanner planner;
	int found = 0;
	int noCandidate = 0;
	int invalidStart = 0;
	int mismatches = 0;
	int queries = 0;

	for (int seed = 0; seed < 30; ++seed)
	{
		BotCore::Rng rng(5000u + (uint32_t)seed);
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
		NavGrid grid = MakeNav(n, unit, events, heights);

		std::vector<NavCell> walkCells;
		for (int x = 0; x < n; ++x)
			for (int z = 0; z < n; ++z)
				if (grid.Walk(x, z))
					walkCells.push_back(Cell(x, z));
		if (walkCells.size() < 2)
			continue;

		NavCostLayer layer;
		layer.Init(grid);
		std::vector<NavThreat> threats(3);
		BotCore::NavThreatParams tp;
		for (int t = 0; t < 3; ++t)
		{
			threats[(size_t)t].x = (float)rng.NextBelow(96);
			threats[(size_t)t].z = (float)rng.NextBelow(96);
			threats[(size_t)t].kind = rng.NextBelow(2) == 0 ? NavThreatKind::Melee : NavThreatKind::Ranged;
		}
		layer.AddThreats(threats.data(), threats.size(), tp);
		if (seed < 15)
		{
			layer.AddForbidDisc((float)rng.NextBelow(96), (float)rng.NextBelow(96), 12.0f);
			layer.AddSafeDisc((float)rng.NextBelow(96), (float)rng.NextBelow(96), 12.0f);
		}

		NavRetreatParams params;
		if (seed % 2 == 1)
		{
			params.wDanger = 5.0f;
			params.wPath = 2.0f;
			params.wAnchor = 0.7f;
			params.wClear = 1.0f;
			params.wSafe = 0.4f;
			params.meleeClearM = 12.0f;
			params.partyRadiusM = 30.0f;
			params.soloRadiusM = 90.0f;
		}

		for (int q = 0; q < 60; ++q)
		{
			const NavCell start = walkCells[(size_t)rng.NextBelow((uint32_t)walkCells.size())];
			const NavRetreatMode mode = (q % 2 == 0) ? NavRetreatMode::Party : NavRetreatMode::Solo;
			NavRetreatQuery query;
			query.start = start;
			query.mode = mode;
			query.threats = threats.data();
			query.threatCount = threats.size();
			if (q % 3 != 0)
			{
				query.hasAnchor = true;
				query.anchorX = (float)rng.NextBelow(96);
				query.anchorZ = (float)rng.NextBelow(96);
			}

			NavRetreatResult out;
			planner.Find(grid, &layer, query, params, out);
			const RefOut ref = RefRetreat(grid, &layer, threats, params, query);
			++queries;

			bool ok = true;
			if (ref.code == 2)
			{
				CHECK(out.status == NavRetreatStatus::InvalidStart);
				ok = out.status == NavRetreatStatus::InvalidStart;
				++invalidStart;
			}
			else if (ref.code == 1)
			{
				CHECK(out.status == NavRetreatStatus::NoCandidate);
				ok = out.status == NavRetreatStatus::NoCandidate;
				++noCandidate;
			}
			else
			{
				CHECK(out.status == NavRetreatStatus::Found);
				++found;
				const bool sameStatus = out.status == NavRetreatStatus::Found;
				const bool sameCandidates = out.candidates == ref.candidates;
				const bool sameScore = std::fabs((double)out.score - ref.bestScore) <= 1e-3;
				const bool cellScore = ref.ScoreOf(out.cell) >= ref.bestScore - 1e-3;
				const bool sameLen = std::fabs((double)out.pathLengthM - ref.DistOf(out.cell)) <= 1e-3;
				CHECK(sameCandidates);
				CHECK(sameScore);
				CHECK(cellScore);
				CHECK(sameLen);
				ok = sameStatus && sameCandidates && sameScore && cellScore && sameLen;
			}
			if (out.status == NavRetreatStatus::Found)
				CheckResult(grid, &layer, threats, query, params, out);
			if (!ok)
				++mismatches;
		}
	}

	std::printf("NAVRETREAT random maps: seeds=30 queries=%d found=%d no_candidate=%d invalid_start=%d mismatches=%d\n",
		queries, found, noCandidate, invalidStart, mismatches);
	REQUIRE(found >= 1500);
	CHECK_EQ(mismatches, 0);
}

TEST_CASE("NavRetreat_Reuse")
{
	const float unit = 4.0f;
	NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
	NavGrid grid30 = MakeNav(30, unit, GapWall(30, 15, 14, 15), HeightZeros(30));
	NavRetreatParams params;

	const std::vector<NavThreat> melee(1, Threat(82.0f, 82.0f, NavThreatKind::Melee));
	NavCostLayer band;
	band.Init(grid);
	band.AddThreats(melee.data(), melee.size(), BotCore::NavThreatParams());

	NavRetreatQuery q1 = Query(Cell(21, 20), NavRetreatMode::Party, true, 50.0f, 82.0f, melee);
	NavRetreatQuery q2 = Query(Cell(10, 14), NavRetreatMode::Party, false, 0.0f, 0.0f, melee);

	NavRetreatPlanner planner;
	NavRetreatResult first;
	planner.Find(grid, &band, q1, params, first);

	// A different grid resizes the pool.
	NavRetreatResult scratch;
	planner.Find(grid30, nullptr, q2, params, scratch);

	NavRetreatResult second;
	planner.Find(grid, &band, q1, params, second);
	CHECK(SameResult(first, second));

	// A fresh planner gives the same answer.
	NavRetreatPlanner fresh;
	NavRetreatResult freshOut;
	fresh.Find(grid, &band, q1, params, freshOut);
	CHECK(SameResult(first, freshOut));

	// Generation wrap path: many different queries, then Q1 again.
	for (int q = 0; q < 70; ++q)
	{
		const NavRetreatQuery different = Query(Cell(5 + q % 30, 5 + q % 30),
			(q % 2 == 0) ? NavRetreatMode::Party : NavRetreatMode::Solo, false, 0.0f, 0.0f, melee);
		NavRetreatResult tmp;
		planner.Find(grid, &band, different, params, tmp);
	}
	NavRetreatResult after;
	planner.Find(grid, &band, q1, params, after);
	CHECK(SameResult(first, after));
}

TEST_CASE("NavRetreat_RealMap")
{
	NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVRETREAT real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	BotCore::NavZoneParams zp;
	NavCostLayer elm;
	NavBuildTeamZones(grid, 1375.0f, 1085.0f, 622.0f, 911.0f, zp, elm);
	NavRetreatPlanner planner;
	NavRetreatResult out;
	NavRetreatParams params;
	const std::vector<NavThreat> none;

	// R-A: solo and party from the arena approach point, anchor = own gate.
	const NavCell startA = Cell(185, 227);
	NavRetreatQuery qSolo = Query(startA, NavRetreatMode::Solo, true, 622.0f, 911.0f, none);
	planner.Find(grid, &elm, qSolo, params, out);
	CHECK(out.status == NavRetreatStatus::Found);
	CHECK(out.cell == Cell(159, 228));
	CHECK(std::fabs(out.score - 2.13283f) <= 2e-3f);
	CHECK(std::fabs(out.pathLengthM - 105.657f) <= 0.05f);
	CHECK_EQ((int)out.path.size(), 27);
	CHECK_EQ(out.candidates, 2799);
	CHECK_EQ(out.expanded, 2799);
	CHECK(out.safe);
	CHECK_EQ((int)out.danger, 0);
	CheckResult(grid, &elm, none, qSolo, params, out);
	const NavCell soloCell = out.cell;
	const float soloScore = out.score;
	const float soloLen = out.pathLengthM;
	const int soloCand = out.candidates;

	NavRetreatQuery qParty = Query(startA, NavRetreatMode::Party, true, 622.0f, 911.0f, none);
	planner.Find(grid, &elm, qParty, params, out);
	CHECK(out.status == NavRetreatStatus::Found);
	CHECK(out.cell == Cell(177, 229));
	CHECK(std::fabs(out.score - 0.61716f) <= 2e-3f);
	CHECK(std::fabs(out.pathLengthM - 35.314f) <= 0.05f);
	CHECK_EQ((int)out.path.size(), 9);
	CHECK_EQ(out.candidates, 245);
	CHECK_EQ(out.expanded, 245);
	CHECK(out.safe);
	CheckResult(grid, &elm, none, qParty, params, out);
	const NavCell partyCell = out.cell;
	const float partyScore = out.score;
	const float partyLen = out.pathLengthM;
	const int partyCand = out.candidates;

	// R-B: same layer plus one melee threat; the path detours and stays far from it.
	const std::vector<NavThreat> meleeVec(1, Threat(690.0f, 910.0f, NavThreatKind::Melee));
	NavCostLayer dyn = elm;
	dyn.AddThreats(meleeVec.data(), meleeVec.size(), BotCore::NavThreatParams());
	NavRetreatQuery qB = Query(startA, NavRetreatMode::Solo, true, 622.0f, 911.0f, meleeVec);
	planner.Find(grid, &dyn, qB, params, out);
	CHECK(out.status == NavRetreatStatus::Found);
	CHECK(out.cell == Cell(159, 228));
	CHECK(std::fabs(out.score - 2.08865f) <= 2e-3f);
	CHECK(std::fabs(out.pathLengthM - 112.284f) <= 0.05f);
	CHECK(out.pathLengthM > soloLen);
	CHECK_EQ((int)out.path.size(), 27);
	CHECK_EQ(out.candidates, 2783);
	CHECK_EQ(out.expanded, 2783);
	float minMelee = 1.0e30f;
	for (size_t i = 0; i < out.path.size(); ++i)
	{
		const float dx = ((float)out.path[i].x + 0.5f) * grid.Unit() - 690.0f;
		const float dz = ((float)out.path[i].z + 0.5f) * grid.Unit() - 910.0f;
		const float dd = std::sqrt(dx * dx + dz * dz);
		if (dd < minMelee)
			minMelee = dd;
	}
	// The path may hug the 8 m disc: the binding criterion is min_melee_m >= 8 (K6).
	CHECK(minMelee >= 8.0f);
	CheckResult(grid, &dyn, meleeVec, qB, params, out);
	const float bScore = out.score;
	const float bLen = out.pathLengthM;
	const int bCand = out.candidates;
	const float bMin = minMelee;

	// R-C: start inside the enemy ring, no anchor: leave by the shortest exit.
	const NavCell startC = Cell(342, 272);
	NavRetreatQuery qC = Query(startC, NavRetreatMode::Solo, false, 0.0f, 0.0f, none);
	planner.Find(grid, &elm, qC, params, out);
	CHECK(out.status == NavRetreatStatus::Found);
	CHECK(out.cell == Cell(320, 271));
	CHECK(!elm.Forbidden(out.cell.x, out.cell.z));
	CHECK(std::fabs(out.score - (-0.09771f)) <= 2e-3f);
	CHECK(std::fabs(out.pathLengthM - 89.657f) <= 0.05f);
	CHECK_EQ((int)out.path.size(), 23);
	CHECK_EQ((int)out.danger, 0);
	int forbOnPath = 0;
	bool left = false;
	bool prefix = true;
	for (size_t i = 0; i < out.path.size(); ++i)
	{
		if (elm.Forbidden(out.path[i].x, out.path[i].z))
		{
			++forbOnPath;
			if (left)
				prefix = false;
		}
		else
		{
			left = true;
		}
	}
	CHECK_EQ(forbOnPath, 22);
	CHECK(prefix);
	CHECK_EQ(out.candidates, 1247);
	CHECK_EQ(out.expanded, 2424);
	CheckResult(grid, &elm, none, qC, params, out);
	const NavCell cCell = out.cell;
	const float cScore = out.score;
	const float cLen = out.pathLengthM;
	const int cCand = out.candidates;
	const int cExp = out.expanded;

	// R-D: arena A, anchor far away: staying put is the best.
	const NavCell startD = Cell(318, 222);
	NavRetreatQuery qD = Query(startD, NavRetreatMode::Solo, true, 622.0f, 911.0f, none);
	planner.Find(grid, &elm, qD, params, out);
	CHECK(out.status == NavRetreatStatus::Found);
	CHECK(out.cell == startD);
	CHECK(std::fabs(out.score - 0.5f) <= 1e-3f);
	CHECK(std::fabs(out.pathLengthM) <= 1e-4f);
	CHECK_EQ((int)out.path.size(), 1);
	CHECK_EQ(out.candidates, 2089);
	CHECK_EQ(out.expanded, 2089);
	const int dStay = (out.cell == startD) ? 1 : 0;
	const int dCand = out.candidates;
	CheckResult(grid, &elm, none, qD, params, out);

	// Reference sweep: 30 random queries on the real map.
	std::vector<NavCell> walkCells;
	walkCells.reserve((size_t)grid.MainComponentCells());
	for (int x = 0; x < grid.Size(); ++x)
		for (int z = 0; z < grid.Size(); ++z)
			if (grid.Walk(x, z))
				walkCells.push_back(Cell(x, z));
	REQUIRE(!walkCells.empty());

	BotCore::Rng rng(20261002u);
	const int sweepQueries = 30;
	int sweepMismatch = 0;
	for (int q = 0; q < sweepQueries; ++q)
	{
		const NavCell start = walkCells[(size_t)rng.NextBelow((uint32_t)walkCells.size())];
		std::vector<NavThreat> sweepThreats(4);
		const NavThreatKind kinds[4] = { NavThreatKind::Melee, NavThreatKind::Ranged,
			NavThreatKind::Melee, NavThreatKind::Ranged };
		for (int t = 0; t < 4; ++t)
		{
			NavCell tc;
			FindWalkCellNear(rng, grid, start, 15, tc);
			sweepThreats[(size_t)t] = Threat(grid.CellCenter(tc.x), grid.CellCenter(tc.z), kinds[t]);
		}
		NavCostLayer sDyn = elm;
		sDyn.AddThreats(sweepThreats.data(), sweepThreats.size(), BotCore::NavThreatParams());

		const bool party = (q % 2 == 0);
		NavRetreatQuery query = Query(start, party ? NavRetreatMode::Party : NavRetreatMode::Solo,
			party, 622.0f, 911.0f, sweepThreats);
		NavRetreatResult outS;
		planner.Find(grid, &sDyn, query, params, outS);
		const RefOut ref = RefRetreat(grid, &sDyn, sweepThreats, params, query);

		bool ok = true;
		if (ref.code == 2)
			ok = outS.status == NavRetreatStatus::InvalidStart;
		else if (ref.code == 1)
			ok = outS.status == NavRetreatStatus::NoCandidate;
		else
			ok = outS.status == NavRetreatStatus::Found && outS.candidates == ref.candidates
				&& std::fabs((double)outS.score - ref.bestScore) <= 1e-3
				&& ref.ScoreOf(outS.cell) >= ref.bestScore - 1e-3
				&& std::fabs((double)outS.pathLengthM - ref.DistOf(outS.cell)) <= 1e-3;
		if (!ok)
			++sweepMismatch;
		CHECK(ok);
	}

	std::printf("NAVRETREAT real: solo A cell=(%d,%d) score=%.5f len=%.3f cand=%d; party A cell=(%d,%d) score=%.5f len=%.3f cand=%d; melee B score=%.5f len=%.3f cand=%d min_melee_m=%.2f; ring C cell=(%d,%d) score=%.5f len=%.3f forb=%d cand=%d exp=%d; arena D stay=%d cand=%d; sweep queries=%d mismatches=%d\n",
		soloCell.x, soloCell.z, soloScore, soloLen, soloCand,
		partyCell.x, partyCell.z, partyScore, partyLen, partyCand,
		bScore, bLen, bCand, bMin,
		cCell.x, cCell.z, cScore, cLen, forbOnPath, cCand, cExp,
		dStay, dCand, sweepQueries, sweepMismatch);
}

TEST_CASE("NavRetreat_Perf")
{
	NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVRETREAT real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	BotCore::NavZoneParams zp;
	NavCostLayer elm;
	NavBuildTeamZones(grid, 1375.0f, 1085.0f, 622.0f, 911.0f, zp, elm);

	std::vector<NavCell> walkCells;
	walkCells.reserve((size_t)grid.MainComponentCells());
	for (int x = 0; x < grid.Size(); ++x)
		for (int z = 0; z < grid.Size(); ++z)
			if (grid.Walk(x, z))
				walkCells.push_back(Cell(x, z));
	REQUIRE(!walkCells.empty());
	const int walkCount = (int)walkCells.size();

#if defined(_DEBUG)
	const int queries = 100;
#else
	const int queries = 1000;
#endif

	BotCore::Rng rng(20261002u);

	struct PerfQuery
	{
		NavCell start;
		NavThreat threats[8];
	};
	std::vector<PerfQuery> set((size_t)queries);
	for (int q = 0; q < queries; ++q)
	{
		set[(size_t)q].start = walkCells[(size_t)rng.NextBelow((uint32_t)walkCount)];
		for (int t = 0; t < 8; ++t)
		{
			NavCell tc;
			FindWalkCellNear(rng, grid, set[(size_t)q].start, 20, tc);
			set[(size_t)q].threats[t] = Threat(grid.CellCenter(tc.x), grid.CellCenter(tc.z),
				(t < 4) ? NavThreatKind::Melee : NavThreatKind::Ranged);
		}
	}

	NavRetreatParams params;
	NavRetreatPlanner planner;

	auto runSet = [&](NavRetreatMode mode, const char * name)
	{
		// Warm-up (not measured).
		for (int q = 0; q < 20 && q < queries; ++q)
		{
			NavCostLayer dyn = elm;
			dyn.AddThreats(set[(size_t)q].threats, 8, BotCore::NavThreatParams());
			NavRetreatQuery query = Query(set[(size_t)q].start, mode, true, 622.0f, 911.0f, std::vector<NavThreat>());
			query.threats = set[(size_t)q].threats;
			query.threatCount = 8;
			NavRetreatResult out;
			planner.Find(grid, &dyn, query, params, out);
		}

		std::vector<double> ms;
		std::vector<double> expanded;
		ms.reserve((size_t)queries);
		expanded.reserve((size_t)queries);
		int found = 0;
		int noCandidate = 0;
		int invalidStart = 0;

		for (int q = 0; q < queries; ++q)
		{
			NavCostLayer dyn = elm;
			dyn.AddThreats(set[(size_t)q].threats, 8, BotCore::NavThreatParams());
			NavRetreatQuery query = Query(set[(size_t)q].start, mode, true, 622.0f, 911.0f, std::vector<NavThreat>());
			query.threats = set[(size_t)q].threats;
			query.threatCount = 8;

			NavRetreatResult out;
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			planner.Find(grid, &dyn, query, params, out);
			const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
			ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
			expanded.push_back((double)out.expanded);

			if (out.status == NavRetreatStatus::Found)
			{
				++found;
				CHECK(!out.path.empty());
				CHECK(out.candidates >= 1);
			}
			else if (out.status == NavRetreatStatus::NoCandidate)
			{
				++noCandidate;
			}
			else
			{
				++invalidStart;
			}
		}

		std::sort(ms.begin(), ms.end());
		std::sort(expanded.begin(), expanded.end());
		const double p50 = PercentileDouble(ms, 0.50);
		const double p95 = PercentileDouble(ms, 0.95);
		const double p99 = PercentileDouble(ms, 0.99);
		const int exp50 = (int)PercentileDouble(expanded, 0.50);
		const int exp95 = (int)PercentileDouble(expanded, 0.95);

		std::printf("NAVRETREAT perf set=%s queries=%d found=%d nocandidate=%d invalidstart=%d expanded_p50=%d expanded_p95=%d ms_p50=%.3f ms_p95=%.3f ms_p99=%.3f\n",
			name, queries, found, noCandidate, invalidStart, exp50, exp95, p50, p95, p99);

		CHECK_EQ(found + noCandidate + invalidStart, queries);
		CHECK_EQ(invalidStart, 0);
		CHECK(found * 100 >= queries * 95);
#ifndef _DEBUG
		if (std::string(name) == "party")
			CHECK(p95 <= 0.5);
		else
			CHECK(p95 <= 3.0);
#else
		(void)p95;
#endif
	};

	runSet(NavRetreatMode::Party, "party");
	runSet(NavRetreatMode::Solo, "solo");
}
