#include "MiniTest.h"

#include <BotCore/NavDrive.h>
#include <BotCore/NavGrid.h>
#include <BotCore/Rng.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

// F5-62: the Goto path-following drive (NavDrive.h). Synthetic grids pin the route / step rules and
// the real zone 71 map pins every produced chord on Walk cells (independent 0.25 m sampling).

namespace
{
	using BotCore::NavCell;
	using BotCore::NavGrid;

	size_t CellIndex(int n, int x, int z)
	{
		return (size_t)x * (size_t)n + (size_t)z;
	}

	// Border blocked, interior open: the main component is the interior 1..n-2.
	std::vector<int16_t> OpenEvents(int n)
	{
		std::vector<int16_t> ev((size_t)n * (size_t)n, 1);
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (x == 0 || x == n - 1 || z == 0 || z == n - 1)
					ev[CellIndex(n, x, z)] = 0;
			}
		}
		return ev;
	}

	// Open field with a single-cell-thick wall column x = 31, z 2..45 (gaps at z = 1 and z >= 46).
	std::vector<int16_t> WallEvents(int n)
	{
		std::vector<int16_t> ev = OpenEvents(n);
		for (int z = 2; z <= 45; ++z)
			ev[CellIndex(n, 31, z)] = 0;
		return ev;
	}

	std::vector<float> HeightZeros(int n)
	{
		return std::vector<float>((size_t)n * (size_t)n, 0.0f);
	}

	NavGrid MakeNav(int n, float unit, const std::vector<int16_t> & ev, const std::vector<float> & h)
	{
		NavGrid grid;
		if (!grid.Init(n, unit, ev, h))
			CHECK(false);
		grid.Build();
		return grid;
	}

	// Loads the exported zone 71 grid; prints a SKIPPED line when missing (K3 rejects SKIPPED).
	bool LoadZone71OrSkip(NavGrid & grid, const char * tag)
	{
		if (!grid.LoadFile("build/nav/zone71.navgrid"))
		{
			std::printf("NAVDRIVE %s: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n", tag);
			return false;
		}
		grid.Build();
		if (grid.MainComponentCells() != 88508)
		{
			std::printf("NAVDRIVE %s: SKIPPED (main component %d != 88508)\n", tag, grid.MainComponentCells());
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

	NavCell NearestWalk(const NavGrid & grid, float wx, float wz)
	{
		const int n = grid.Size();
		NavCell best;
		best.x = -1;
		best.z = -1;
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
					best.x = x;
					best.z = z;
				}
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

	// Random offset 0.1..3.9 m inside a 4 m cell, quantised (stays in the same cell).
	float OffsetInCell(BotCore::Rng & rng)
	{
		const int step = (int)rng.NextBelow(39);
		return BotCore::NavQuantiseM(0.1f + (float)step * 0.1f);
	}

	void RandomPointInCell(const NavGrid & grid, NavCell c, BotCore::Rng & rng, float & wx, float & wz)
	{
		wx = (float)c.x * grid.Unit() + OffsetInCell(rng);
		wz = (float)c.z * grid.Unit() + OffsetInCell(rng);
	}

	// Independent 0.25 m sampling: true when every sample lies on a Walk cell.
	bool ChordSampleClean(const NavGrid & grid, float x0, float z0, float x1, float z1)
	{
		const double dx = (double)x1 - (double)x0;
		const double dz = (double)z1 - (double)z0;
		const double len = std::sqrt(dx * dx + dz * dz);
		const int steps = (int)std::ceil(len / 0.25);
		for (int i = 0; i <= steps; ++i)
		{
			const double t = steps > 0 ? (double)i / (double)steps : 0.0;
			const float wx = (float)((double)x0 + dx * t);
			const float wz = (float)((double)z0 + dz * t);
			if (!grid.Walk(grid.CellOf(wx), grid.CellOf(wz)))
				return false;
		}
		return true;
	}

	struct WalkResult
	{
		int packets = 0;
		bool arrived = false;
		int blocked = 0;
		int truncated = 0;
		int fullSteps = 0;
		double maxStepM = 0.0;
		int sampleViolations = 0;
		int crossedFull = 0;
		float finalX = 0.0f;
		float finalZ = 0.0f;
	};

	// Drives the bot with NextStep and feeds each returned position back in. The caller moves the bot
	// (the drive never does). Bounded to 2000 turns (an infinite-loop guard).
	WalkResult Walk(BotCore::NavDrive & drive, const NavGrid & grid, float sx, float sz,
		float maxStep, int maxTurns = 2000)
	{
		WalkResult r;

		const std::vector<BotCore::NavRoutePoint> & route = drive.Route();
		std::vector<float> cum;
		cum.reserve(route.size());
		float acc = 0.0f;
		for (size_t i = 0; i < route.size(); ++i)
		{
			if (i > 0)
			{
				const float dx = route[i].x - route[i - 1].x;
				const float dz = route[i].z - route[i - 1].z;
				acc += std::sqrt(dx * dx + dz * dz);
			}
			cum.push_back(acc);
		}

		float x = sx;
		float z = sz;
		float prevProg = BotCore::NavRouteProgressM(route.data(), (int)route.size(), x, z);

		for (int t = 0; t < maxTurns; ++t)
		{
			const BotCore::NavDriveStep s = drive.NextStep(grid, x, z, maxStep);
			if (s.kind == BotCore::NavDriveStep::None || s.kind == BotCore::NavDriveStep::Blocked)
			{
				if (s.kind == BotCore::NavDriveStep::Blocked)
					++r.blocked;
				break;
			}

			if (!ChordSampleClean(grid, x, z, s.x, s.z))
				++r.sampleViolations;

			const double dx = (double)s.x - (double)x;
			const double dz = (double)s.z - (double)z;
			const double d = std::sqrt(dx * dx + dz * dz);
			if (d > r.maxStepM)
				r.maxStepM = d;

			++r.packets;
			if (s.truncated)
				++r.truncated;
			else if (s.kind == BotCore::NavDriveStep::Step)
				++r.fullSteps;

			if (!s.truncated)
			{
				for (size_t i = 1; i + 1 < route.size(); ++i)
				{
					if (cum[i] > prevProg + 1e-3f && cum[i] <= s.routeProgressM + 1e-3f)
					{
						++r.crossedFull;
						break;
					}
				}
			}

			prevProg = s.routeProgressM;
			x = s.x;
			z = s.z;
			if (s.kind == BotCore::NavDriveStep::Arrived)
			{
				r.arrived = true;
				break;
			}
		}

		r.finalX = x;
		r.finalZ = z;
		return r;
	}
}

TEST_CASE("NavDrive_Quantise")
{
	CHECK(BotCore::NavQuantiseM(0.04f) == 0.0f);
	CHECK(BotCore::NavQuantiseM(0.05f) == 0.1f);
	CHECK(BotCore::NavQuantiseM(123.456f) == 123.5f);
	CHECK(BotCore::NavQuantiseM(123.5f) == 123.5f);

	const float w[5] = { 0.0f, 1.234f, 67.85f, 255.55f, 4096.04f };
	for (int i = 0; i < 5; ++i)
	{
		const float q = BotCore::NavQuantiseM(w[i]);
		CHECK(std::fabs(q - w[i]) <= 0.05f + 1e-4f);
	}
}

TEST_CASE("NavDrive_Goto_OpenField")
{
	const int n = 64;
	const float unit = 4.0f;
	NavGrid grid = MakeNav(n, unit, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavDriveParams params;

	const float botx = 20.0f, botz = 20.0f;
	const float gx = 200.0f, gz = 200.0f;
	REQUIRE(drive.BeginGoto(grid, finder, botx, botz, gx, gz, params) == BotCore::NavPlanStatus::Planned);
	REQUIRE(drive.Active());
	CHECK(drive.Replans() == 0);
	const float L = drive.RouteLengthM();

	const WalkResult r = Walk(drive, grid, botx, botz, 6.75f);
	CHECK(r.arrived);
	CHECK_EQ(r.blocked, 0);
	CHECK_EQ(r.truncated, 0);
	CHECK_EQ(r.sampleViolations, 0);
	CHECK(std::fabs(r.finalX - BotCore::NavQuantiseM(gx)) <= 1e-4f);
	CHECK(std::fabs(r.finalZ - BotCore::NavQuantiseM(gz)) <= 1e-4f);

	const int expected = (int)std::ceil(L / 6.75f - 1e-9f);
	CHECK(r.packets >= expected - 1);
	CHECK(r.packets <= expected + 1);
	CHECK(r.maxStepM <= 6.75 + 0.15 + 1e-3);
	CHECK(drive.Replans() == 0);
}

TEST_CASE("NavDrive_Goto_AlreadyThere")
{
	const int n = 64;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavDriveParams params;

	const float gx = 100.0f, gz = 100.0f;
	REQUIRE(drive.BeginGoto(grid, finder, gx, gz, gx, gz, params) == BotCore::NavPlanStatus::Planned);
	const BotCore::NavDriveStep s = drive.NextStep(grid, gx, gz, 6.75f);
	CHECK(s.kind == BotCore::NavDriveStep::Arrived);
	CHECK(std::fabs(s.x - gx) <= 1e-4f);
	CHECK(std::fabs(s.z - gz) <= 1e-4f);
	CHECK(drive.Replans() == 0);
}

TEST_CASE("NavDrive_Goto_Detour_AllChordsWalk")
{
	const int n = 64;
	NavGrid grid = MakeNav(n, 4.0f, WallEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavDriveParams params;

	const float botx = 20.0f, botz = 20.0f;
	const float gx = 200.0f, gz = 20.0f;
	REQUIRE(drive.BeginGoto(grid, finder, botx, botz, gx, gz, params) == BotCore::NavPlanStatus::Planned);
	const float straight = std::sqrt((gx - botx) * (gx - botx) + (gz - botz) * (gz - botz));
	CHECK(drive.RouteLengthM() >= straight);

	const WalkResult r = Walk(drive, grid, botx, botz, 6.75f);
	CHECK(r.arrived);
	CHECK_EQ(r.blocked, 0);
	CHECK_EQ(r.sampleViolations, 0);
}

TEST_CASE("NavDrive_Goto_ChordBlockedTruncates")
{
	const int n = 64;
	NavGrid grid = MakeNav(n, 4.0f, WallEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavDriveParams params;

	const float botx = 20.0f, botz = 20.0f;
	const float gx = 200.0f, gz = 20.0f;
	REQUIRE(drive.BeginGoto(grid, finder, botx, botz, gx, gz, params) == BotCore::NavPlanStatus::Planned);
	REQUIRE(drive.RouteLengthM() <= 400.0f);
	REQUIRE(drive.Route().size() >= 3);

	const BotCore::NavDriveStep first = drive.NextStep(grid, botx, botz, 400.0f);
	CHECK(first.kind == BotCore::NavDriveStep::Step);
	CHECK(first.truncated);
	const BotCore::NavRoutePoint & v1 = drive.Route()[1];
	CHECK(std::fabs(first.x - BotCore::NavQuantiseM(v1.x)) <= 1e-4f);
	CHECK(std::fabs(first.z - BotCore::NavQuantiseM(v1.z)) <= 1e-4f);

	const int routePoints = (int)drive.Route().size();
	const WalkResult r = Walk(drive, grid, botx, botz, 400.0f);
	CHECK(r.arrived);
	CHECK_EQ(r.blocked, 0);
	CHECK_EQ(r.sampleViolations, 0);
	CHECK_EQ(r.fullSteps, 0);
	CHECK_EQ(r.packets, routePoints - 1);
}

TEST_CASE("NavDrive_Goto_StatusMapping")
{
	const int n = 64;
	NavGrid open = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	NavGrid wall = MakeNav(n, 4.0f, WallEvents(n), HeightZeros(n));

	std::vector<float> splitH = HeightZeros(n);
	for (int x = 32; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
			splitH[CellIndex(n, x, z)] = 10.0f;
	}
	NavGrid split = MakeNav(n, 4.0f, OpenEvents(n), splitH);

	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavDriveParams params;

	// Non-Walk goal (the wall cell at (31,20)).
	CHECK(drive.BeginGoto(wall, finder, 20.0f, 20.0f, wall.CellCenter(31), wall.CellCenter(20), params)
		== BotCore::NavPlanStatus::InvalidGoal);
	CHECK(!drive.Active());
	CHECK(drive.Route().empty());

	// Negative goal.
	CHECK(drive.BeginGoto(open, finder, 20.0f, 20.0f, -1.0f, 20.0f, params)
		== BotCore::NavPlanStatus::InvalidGoal);
	CHECK(!drive.Active());
	CHECK(drive.Route().empty());

	// Goal outside the grid.
	CHECK(drive.BeginGoto(open, finder, 20.0f, 20.0f, 1000.0f, 1000.0f, params)
		== BotCore::NavPlanStatus::InvalidGoal);
	CHECK(!drive.Active());

	// Non-Walk start (the wall cell).
	CHECK(drive.BeginGoto(wall, finder, wall.CellCenter(31), wall.CellCenter(20), 200.0f, 20.0f, params)
		== BotCore::NavPlanStatus::InvalidStart);
	CHECK(!drive.Active());
	CHECK(drive.Route().empty());

	// NoPath: the 10 m height step closes every edge between the two Walk halves.
	CHECK(drive.BeginGoto(split, finder, 20.0f, 20.0f, 200.0f, 20.0f, params)
		== BotCore::NavPlanStatus::NoPath);
	CHECK(!drive.Active());
	CHECK(drive.Route().empty());

	// NodeLimit: maxNodes = 8 on a far goal.
	BotCore::NavDriveParams tiny;
	tiny.search.maxNodes = 8;
	CHECK(drive.BeginGoto(open, finder, 20.0f, 20.0f, 200.0f, 200.0f, tiny)
		== BotCore::NavPlanStatus::NodeLimit);
	CHECK(!drive.Active());
	CHECK(drive.Route().empty());
}

TEST_CASE("NavDrive_Goto_EndpointsOffCentre")
{
	const int n = 64;
	std::vector<int16_t> ev = OpenEvents(n);
	const int rects[12][4] = {
		{ 8,  8, 2, 2 }, { 8, 20, 2, 2 }, { 8, 32, 2, 2 }, { 8, 44, 2, 2 },
		{ 24, 14, 2, 2 }, { 24, 26, 2, 2 }, { 24, 38, 2, 2 }, { 24, 50, 2, 2 },
		{ 40,  8, 2, 2 }, { 40, 20, 2, 2 }, { 40, 32, 2, 2 }, { 40, 44, 2, 2 }
	};
	for (int r = 0; r < 12; ++r)
	{
		for (int x = rects[r][0]; x < rects[r][0] + rects[r][2]; ++x)
		{
			for (int z = rects[r][1]; z < rects[r][1] + rects[r][3]; ++z)
				ev[CellIndex(n, x, z)] = 0;
		}
	}
	NavGrid grid = MakeNav(n, 4.0f, ev, HeightZeros(n));

	std::vector<NavCell> walk;
	CollectWalk(grid, walk);
	REQUIRE(!walk.empty());

	BotCore::Rng rng(20261003u);
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavDriveParams params;

	const int pairs = 300;
	int planned = 0;
	int arrived = 0;
	int blocked = 0;
	int replans = 0;
	int violations = 0;

	for (int i = 0; i < pairs; ++i)
	{
		const NavCell a = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		const NavCell b = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		if (a.x == b.x && a.z == b.z)
		{
			--i;
			continue;
		}

		float ax = 0.0f, az = 0.0f, bx = 0.0f, bz = 0.0f;
		RandomPointInCell(grid, a, rng, ax, az);
		RandomPointInCell(grid, b, rng, bx, bz);

		if (drive.BeginGoto(grid, finder, ax, az, bx, bz, params) != BotCore::NavPlanStatus::Planned)
			continue;

		++planned;
		const WalkResult r = Walk(drive, grid, ax, az, 6.75f);
		if (r.arrived)
			++arrived;
		blocked += r.blocked;
		violations += r.sampleViolations;
		replans += drive.Replans();
	}

	std::printf("NAVDRIVE offcentre pairs=%d planned=%d blocked=%d\n", pairs, planned, blocked);
	CHECK_EQ(planned, pairs);
	CHECK_EQ(arrived, planned);
	CHECK_EQ(blocked, 0);
	CHECK_EQ(replans, 0);
	CHECK_EQ(violations, 0);
}

TEST_CASE("NavDrive_Blocked_Replan")
{
	const int n = 64;
	NavGrid open = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	NavGrid wall = MakeNav(n, 4.0f, WallEvents(n), HeightZeros(n));
	std::vector<int16_t> none((size_t)n * n, 0);
	NavGrid allBlocked = MakeNav(n, 4.0f, none, HeightZeros(n));

	BotCore::NavPathfinder finder;
	BotCore::NavDriveParams params;
	const float botx = 20.0f, botz = 20.0f;
	const float gx = 200.0f, gz = 20.0f;

	// Recover once from a blocked step and arrive.
	{
		BotCore::NavDrive drive;
		REQUIRE(drive.BeginGoto(open, finder, botx, botz, gx, gz, params) == BotCore::NavPlanStatus::Planned);
		CHECK(drive.NextStep(wall, botx, botz, 400.0f).kind == BotCore::NavDriveStep::Blocked);
		CHECK(drive.Replan(wall, finder, botx, botz, params) == BotCore::NavPlanStatus::Planned);
		CHECK_EQ(drive.Replans(), 1);
		const WalkResult r = Walk(drive, wall, botx, botz, 6.75f);
		CHECK(r.arrived);
		CHECK_EQ(r.blocked, 0);
	}

	// A second Replan is refused and the drive closes.
	{
		BotCore::NavDrive drive;
		REQUIRE(drive.BeginGoto(open, finder, botx, botz, gx, gz, params) == BotCore::NavPlanStatus::Planned);
		CHECK(drive.NextStep(wall, botx, botz, 400.0f).kind == BotCore::NavDriveStep::Blocked);
		REQUIRE(drive.Replan(wall, finder, botx, botz, params) == BotCore::NavPlanStatus::Planned);
		CHECK_EQ(drive.Replans(), 1);
		CHECK(drive.NextStep(allBlocked, botx, botz, 6.75f).kind == BotCore::NavDriveStep::Blocked);
		CHECK(drive.Replan(allBlocked, finder, botx, botz, params) == BotCore::NavPlanStatus::ReplanLimit);
		CHECK(!drive.Active());
	}
}

TEST_CASE("NavDrive_OffRoute")
{
	const int n = 64;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavDriveParams params;

	const float gx = 200.0f, gz = 20.0f;
	REQUIRE(drive.BeginGoto(grid, finder, 20.0f, 20.0f, gx, gz, params) == BotCore::NavPlanStatus::Planned);

	float x = 20.0f, z = 20.0f;
	float prevProg = 0.0f;
	bool haveProg = false;
	bool pushed = false;
	bool arrived = false;
	int packets = 0;

	for (int t = 0; t < 2000; ++t)
	{
		const BotCore::NavDriveStep s = drive.NextStep(grid, x, z, 6.75f);
		if (s.kind == BotCore::NavDriveStep::None || s.kind == BotCore::NavDriveStep::Blocked)
			break;
		if (haveProg)
			CHECK(s.routeProgressM >= prevProg - 2.0f);
		prevProg = s.routeProgressM;
		haveProg = true;

		x = s.x;
		z = s.z;
		++packets;
		if (s.kind == BotCore::NavDriveStep::Arrived)
		{
			arrived = true;
			break;
		}

		if (!pushed && packets == 5)
		{
			const float cand[4][2] = { { x + 2.0f, z }, { x - 2.0f, z }, { x, z + 2.0f }, { x, z - 2.0f } };
			for (int k = 0; k < 4; ++k)
			{
				if (grid.Walk(grid.CellOf(cand[k][0]), grid.CellOf(cand[k][1])))
				{
					x = cand[k][0];
					z = cand[k][1];
					break;
				}
			}
			pushed = true;
		}
	}

	CHECK(pushed);
	CHECK(arrived);
}

TEST_CASE("NavDrive_Reset_Inactive")
{
	const int n = 64;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavDriveParams params;

	REQUIRE(drive.BeginGoto(grid, finder, 20.0f, 20.0f, 200.0f, 20.0f, params) == BotCore::NavPlanStatus::Planned);
	drive.Reset();
	CHECK(!drive.Active());
	CHECK(drive.Route().empty());
	CHECK(drive.RouteLengthM() == 0.0f);
	CHECK(drive.NextStep(grid, 20.0f, 20.0f, 6.75f).kind == BotCore::NavDriveStep::None);
	CHECK(drive.Replan(grid, finder, 20.0f, 20.0f, params) == BotCore::NavPlanStatus::None);

	REQUIRE(drive.BeginGoto(grid, finder, 20.0f, 20.0f, 200.0f, 20.0f, params) == BotCore::NavPlanStatus::Planned);
	CHECK(drive.NextStep(grid, 20.0f, 20.0f, 0.0f).kind == BotCore::NavDriveStep::None);
	CHECK(drive.NextStep(grid, 20.0f, 20.0f, -1.0f).kind == BotCore::NavDriveStep::None);
	CHECK(drive.NextStep(grid, std::nanf(""), 20.0f, 6.75f).kind == BotCore::NavDriveStep::None);
}

TEST_CASE("NavDrive_RealMap_RespawnReturn")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "respawn"))
		return;

	std::vector<NavCell> walk;
	CollectWalk(grid, walk);
	REQUIRE(!walk.empty());

	BotCore::NavPathfinder finder;
	BotCore::NavDriveParams params;

	const BotCore::NavCell arena = NearestWalk(grid, 1274.0f, 890.0f);
	REQUIRE(grid.Walk(arena.x, arena.z));

	const float spawns[2][2] = { { 1385.0f, 1095.0f }, { 635.0f, 925.0f } };
	const char * names[2] = { "karus", "elmorad" };

	for (int si = 0; si < 2; ++si)
	{
		const BotCore::NavCell s = NearestWalk(grid, spawns[si][0], spawns[si][1]);
		REQUIRE(grid.Walk(s.x, s.z));

		const float sx = grid.CellCenter(s.x);
		const float sz = grid.CellCenter(s.z);
		const float gx = grid.CellCenter(arena.x);
		const float gz = grid.CellCenter(arena.z);

		BotCore::NavDrive drive;
		REQUIRE(drive.BeginGoto(grid, finder, sx, sz, gx, gz, params) == BotCore::NavPlanStatus::Planned);
		const float L = drive.RouteLengthM();
		const WalkResult r = Walk(drive, grid, sx, sz, 6.75f);

		CHECK(r.arrived);
		CHECK_EQ(r.blocked, 0);
		CHECK_EQ(r.sampleViolations, 0);
		CHECK(r.crossedFull >= 1);

		if (si == 0)
			CHECK(L >= 268.0f && L <= 284.0f);
		else
			CHECK(L >= 680.0f && L <= 722.0f);

		const int base = (int)std::ceil(L / 6.75f - 1e-9f);
		CHECK(r.packets >= base);
		CHECK(r.packets <= base + drive.PlanWaypoints() + 2);

		std::printf("NAVDRIVE respawn %s: route_m=%.1f waypoints=%d expanded=%d packets=%d crossed=%d truncated=%d eta_s=%.1f blocked=0\n",
			names[si], (double)L, drive.PlanWaypoints(), drive.PlanExpanded(), r.packets, r.crossedFull,
			r.truncated, (double)(L / 4.5f));
	}
}

TEST_CASE("NavDrive_RealMap_Random")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "random"))
		return;

	std::vector<NavCell> walk;
	CollectWalk(grid, walk);
	REQUIRE(!walk.empty());

	BotCore::Rng rng(20261003u);
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavDriveParams params;

	const int pairs = 300;
	int planned = 0;
	int nopath = 0;
	int nodelimit = 0;
	int badstart = 0;
	int badgoal = 0;
	int blockedEvents = 0;
	int replans = 0;
	int unrecoverable = 0;
	int truncated = 0;
	int violations = 0;

	for (int i = 0; i < pairs; ++i)
	{
		const NavCell a = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		const NavCell b = RandomNear(walk, rng, a, 64);
		if (a.x == b.x && a.z == b.z)
		{
			--i;
			continue;
		}

		float ax = 0.0f, az = 0.0f, bx = 0.0f, bz = 0.0f;
		RandomPointInCell(grid, a, rng, ax, az);
		RandomPointInCell(grid, b, rng, bx, bz);

		const BotCore::NavPlanStatus st = drive.BeginGoto(grid, finder, ax, az, bx, bz, params);
		if (st == BotCore::NavPlanStatus::NoPath)
		{
			++nopath;
			continue;
		}
		if (st == BotCore::NavPlanStatus::NodeLimit)
		{
			++nodelimit;
			continue;
		}
		if (st == BotCore::NavPlanStatus::InvalidStart)
		{
			++badstart;
			continue;
		}
		if (st == BotCore::NavPlanStatus::InvalidGoal)
		{
			++badgoal;
			continue;
		}
		if (st != BotCore::NavPlanStatus::Planned)
			continue;

		++planned;
		WalkResult r = Walk(drive, grid, ax, az, 6.75f);
		if (!r.arrived && r.blocked > 0)
		{
			++blockedEvents;
			const BotCore::NavPlanStatus rs = drive.Replan(grid, finder, r.finalX, r.finalZ, params);
			if (rs == BotCore::NavPlanStatus::Planned)
			{
				++replans;
				r = Walk(drive, grid, r.finalX, r.finalZ, 6.75f);
			}
		}
		if (!r.arrived)
			++unrecoverable;
		truncated += r.truncated;
		violations += r.sampleViolations;
	}

	std::printf("NAVDRIVE random pairs=%d planned=%d nopath=%d blocked_events=%d replans=%d unrecoverable=%d truncated=%d\n",
		pairs, planned, nopath, blockedEvents, replans, unrecoverable, truncated);
	CHECK(planned * 100 >= pairs * 85);
	CHECK_EQ(nodelimit, 0);
	CHECK_EQ(badstart, 0);
	CHECK_EQ(badgoal, 0);
	CHECK_EQ(unrecoverable, 0);
	CHECK_EQ(violations, 0);
	CHECK_EQ(nopath, pairs - planned);
}

TEST_CASE("NavDrive_Perf")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "perf"))
		return;

	std::vector<NavCell> walk;
	CollectWalk(grid, walk);
	REQUIRE(!walk.empty());

	BotCore::Rng rng(20261003u);
	BotCore::NavPathfinder finder;
	BotCore::NavDriveParams params;
	BotCore::NavDrive drive;

	// Warm-up: the first Find allocates the pathfinder pool.
	{
		const NavCell a = walk[0];
		const NavCell b = walk[walk.size() / 2];
		drive.BeginGoto(grid, finder, grid.CellCenter(a.x), grid.CellCenter(a.z),
			grid.CellCenter(b.x), grid.CellCenter(b.z), params);
	}

	const int total = 2000;
	std::vector<double> beginMs;
	std::vector<double> stepMs;
	beginMs.reserve((size_t)total);
	stepMs.reserve((size_t)total);

	for (int q = 0; q < total; ++q)
	{
		const NavCell a = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		const NavCell b = RandomNear(walk, rng, a, 64);
		const float ax = grid.CellCenter(a.x);
		const float az = grid.CellCenter(a.z);
		const float bx = grid.CellCenter(b.x);
		const float bz = grid.CellCenter(b.z);

		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		const BotCore::NavPlanStatus st = drive.BeginGoto(grid, finder, ax, az, bx, bz, params);
		const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
		beginMs.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

		if (st == BotCore::NavPlanStatus::Planned)
		{
			const std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
			drive.NextStep(grid, ax, az, 6.75f);
			const std::chrono::steady_clock::time_point t3 = std::chrono::steady_clock::now();
			stepMs.push_back(std::chrono::duration<double, std::milli>(t3 - t2).count());
		}
	}

	std::sort(beginMs.begin(), beginMs.end());
	std::sort(stepMs.begin(), stepMs.end());
	const double bp95 = PercentileDouble(beginMs, 0.95);
	const double sp95 = PercentileDouble(stepMs, 0.95);
	std::printf("NAVDRIVE perf: begin_p95_ms=%.4f step_p95_ms=%.4f\n", bp95, sp95);

#ifndef _DEBUG
	CHECK(bp95 <= 2.0);
	CHECK(sp95 <= 0.05);
#else
	(void)bp95;
	(void)sp95;
#endif
}
