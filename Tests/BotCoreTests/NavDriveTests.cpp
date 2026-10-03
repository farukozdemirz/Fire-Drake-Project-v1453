#include "MiniTest.h"

#include <BotCore/NavDrive.h>
#include <BotCore/NavGrid.h>
#include <BotCore/NavReach.h>
#include <BotCore/Rng.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <utility>
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

	std::printf("NAVDRIVE random pairs=%d planned=%d nopath=%d nodelimit=%d blocked_events=%d replans=%d unrecoverable=%d truncated=%d\n",
		pairs, planned, nopath, nodelimit, blockedEvents, replans, unrecoverable, truncated);
	CHECK(planned * 100 >= pairs * 85);
	CHECK_EQ(badstart, 0);
	CHECK_EQ(badgoal, 0);
	CHECK_EQ(unrecoverable, 0);
	CHECK_EQ(violations, 0);
	CHECK_EQ(nopath + nodelimit, pairs - planned);
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

// ---------------------------------------------------------------------------
// F5-73: NavDrive Follow mode (synthetic world + real zone 71 chase)
// ---------------------------------------------------------------------------

namespace
{
	using BotCore::NavCell;

	int64_t SimNextTick(BotCore::Rng & rng, int model)
	{
		double mean = 100.0;
		double jitter = 0.0;
		double late = 0.0;
		if (model == 1)
			jitter = 10.0;
		else if (model == 2)
		{
			mean = 110.8;
			jitter = 20.0;
			late = 0.03;
		}

		double dt = mean;
		if (jitter > 0.0)
		{
			double s = 0.0;
			for (int i = 0; i < 12; ++i)
				s += rng.NextDouble();
			dt += (s - 6.0) * jitter;
		}
		if (rng.NextDouble() < late)
			dt += 250.0;
		if (dt < 20.0)
			dt = 20.0;
		return (int64_t)dt;
	}

	float Dist2D(float ax, float az, float bx, float bz)
	{
		const float dx = bx - ax;
		const float dz = bz - az;
		return std::sqrt(dx * dx + dz * dz);
	}

	// Target motions: 0 Line, 1 Zigzag, 2 StopGo, 3 RandomWalk, 4 Fixed, 5 planner route.
	struct SimTarget
	{
		float x = 0.0f;
		float z = 0.0f;
		float dx = 1.0f;
		float dz = 0.0f;
		bool moving = true;
		float speed = 4.5f;
		int kind = 0;
		int64_t stateMs = 0;
		int64_t elapsedMs = 0;
		int64_t stopAtMs = -1;
		float minX = 80.0f;
		float maxX = 360.0f;
		BotCore::Rng rng;

		SimTarget() : rng(0) {}

		void Init(int k, uint32_t seed, float sx, float sz, float spd)
		{
			kind = k;
			x = sx;
			z = sz;
			speed = spd;
			moving = (k != 4);
			dx = 1.0f;
			dz = 0.0f;
			rng = BotCore::Rng(seed);
			if (k == 1)
			{
				const float s = 0.70710678f;
				dx = s;
				dz = s;
			}
		}

		void Advance(int64_t dtMs)
		{
			if (kind == 4)
			{
				moving = false;
				return;
			}
			if (dtMs <= 0)
				return;
			elapsedMs += dtMs;

			const float dt = (float)dtMs / 1000.0f;
			if (moving)
			{
				x += dx * speed * dt;
				z += dz * speed * dt;
			}

			if (kind == 0 || kind == 6)
			{
				if (kind == 6 && stopAtMs >= 0 && elapsedMs >= stopAtMs)
					moving = false;
				if (x < minX) { x = minX + (minX - x); dx = 1.0f; }
				if (x > maxX) { x = maxX - (x - maxX); dx = -1.0f; }
				z = 200.0f;
			}
			else if (kind == 1)
			{
				stateMs += dtMs;
				if (stateMs >= 2000) { stateMs = 0; dz = -dz; }
				if (x < 60.0f) { x = 60.0f + (60.0f - x); dx = -dx; }
				if (x > 340.0f) { x = 340.0f - (x - 340.0f); dx = -dx; }
				if (z < 80.0f) { z = 80.0f + (80.0f - z); dz = -dz; }
				if (z > 320.0f) { z = 320.0f - (z - 320.0f); dz = -dz; }
			}
			else if (kind == 2)
			{
				stateMs += dtMs;
				if (stateMs >= 6000)
				{
					stateMs = 0;
					moving = !moving;
				}
				if (x < 80.0f) { x = 80.0f + (80.0f - x); dx = 1.0f; }
				if (x > 360.0f) { x = 360.0f - (x - 360.0f); dx = -1.0f; }
				z = 200.0f;
			}
			else if (kind == 3)
			{
				stateMs += dtMs;
				if (stateMs >= (int64_t)(1000 + rng.NextBelow(3000)))
				{
					stateMs = 0;
					const double r = rng.NextDouble();
					if (r < 0.15)
					{
						moving = !moving;
					}
					else if (r < 0.40)
					{
						dx = -dx;
						dz = -dz;
						moving = true;
					}
					else
					{
						const double a = rng.NextDouble() * 6.2831853;
						dx = (float)std::cos(a);
						dz = (float)std::sin(a);
						moving = true;
					}
				}
				if (x < 40.0f) { x = 40.0f + (40.0f - x); dx = -dx; }
				if (x > 360.0f) { x = 360.0f - (x - 360.0f); dx = -dx; }
				if (z < 40.0f) { z = 40.0f + (40.0f - z); dz = -dz; }
				if (z > 360.0f) { z = 360.0f - (z - 360.0f); dz = -dz; }
			}
		}
	};

	struct FollowRun
	{
		long ticks = 0;
		int plans = 0;
		int planFails = 0;
		int noGoalPlans = 0;
		int pathFailPlans = 0;
		int invalidStartPlans = 0;
		int endTargetLost = 0;
		int endPlanFailed = 0;
		int endStuck = 0;
		int endBlocked = 0;
		int packets = 0;
		int arrivedPackets = 0;
		int stalledTicks = 0;
		int awaitingTicks = 0;
		int guardTicks = 0;
		int progressingTicks = 0;
		int holdStops = 0;
		int ended = 0;
		int64_t endedMs = -1;
		int maxStage = 0;
		int firstStage1DelayMs = -1;
		int64_t firstRecoveryMs = -1;
		int maxRecoverMs = -1;
		int blockedSteps = 0;
		int chordViolations = 0;
		int64_t lastPacketMs = -1;
		float finalDist = 0.0f;
		float finalBotX = 0.0f;
		float finalBotZ = 0.0f;
		std::vector<int> stageSeq;
		std::vector<double> dist;
	};

	class FollowSim
	{
	public:
		FollowSim(const NavGrid & g, BotCore::NavPathfinder & pf) : grid(g), finder(pf) {}

		const NavGrid & grid;
		BotCore::NavPathfinder & finder;
		BotCore::NavDrive drive;
		BotCore::NavFollowDriveParams params;
		BotCore::NavCostLayer scratch;
		std::set<std::pair<int, int>> hidden;
		int tickModel = 0;
		int targetKind = 0;
		float targetSpeed = 4.5f;
		float targetStartX = 80.0f;
		float targetStartZ = 200.0f;
		float lineMinX = 80.0f;
		float lineMaxX = 360.0f;
		uint32_t seed = 20261003u;
		uint32_t targetSeed = 12345u;
		float botX = 70.0f;
		float botZ = 200.0f;
		int guardStartMs = -1;
		int guardEndMs = -1;
		int64_t invisibleFromMs = -1;
		int64_t invisibleToMs = -1;
		bool stopOnFirstRecovery = false;
		int64_t targetStopAtMs = -1;
		int clipComponent = -1;   // when >= 0, server movement never leaves this edge-connected component
		const BotCore::NavReach * reach = nullptr;
		const std::vector<NavCell> * walkCells = nullptr;
		int invisibleCount = 0;   // how many separate invisible windows (0 or 1 used)

		FollowRun Run(int64_t durationMs);
	};

	FollowRun FollowSim::Run(int64_t durationMs)
	{
		FollowRun out;

		SimTarget target;
		target.Init(targetKind, targetSeed, targetStartX, targetStartZ, targetSpeed);
		target.stopAtMs = targetStopAtMs;
		target.minX = lineMinX;
		target.maxX = lineMaxX;

		// Planner target (kind 5): a smoothed route the target walks along.
		std::vector<BotCore::NavRoutePoint> tRoute;
		float tPos = 0.0f;
		BotCore::Rng tRng(targetSeed);
		auto planTargetRoute = [&]()
		{
			if (walkCells == nullptr || walkCells->empty())
				return;
			for (int attempt = 0; attempt < 40; ++attempt)
			{
				NavCell from;
				from.x = grid.CellOf(target.x);
				from.z = grid.CellOf(target.z);
				if (!grid.Walk(from.x, from.z) || grid.Clearance(from.x, from.z) < 2)
					from = (*walkCells)[(size_t)tRng.NextBelow((uint32_t)walkCells->size())];
				NavCell to = from;
				bool picked = false;
				for (int tries = 0; tries < 80; ++tries)
				{
					const NavCell c = (*walkCells)[(size_t)tRng.NextBelow((uint32_t)walkCells->size())];
					const int md = std::abs(c.x - from.x) + std::abs(c.z - from.z);
					if (md >= 20 && md <= 35)
					{
						to = c;
						picked = true;
						break;
					}
				}
				if (!picked)
					continue;
				BotCore::NavPathResult path;
				finder.Find(grid, from, to, BotCore::NavSearchParams(), path);
				if (path.status != BotCore::NavPathStatus::Found)
					continue;
				bool openPath = true;
				for (size_t i = 0; i < path.cells.size(); ++i)
				{
					if (grid.Clearance(path.cells[i].x, path.cells[i].z) < 2)
					{
						openPath = false;
						break;
					}
				}
				if (!openPath)
					continue;
				BotCore::NavSmoothResult sm;
				BotCore::NavSmoothPath(grid, path.cells, BotCore::NavSmoothParams(), sm);
				tRoute.clear();
				BotCore::NavRoutePoint p0;
				p0.x = grid.CellCenter(from.x);
				p0.z = grid.CellCenter(from.z);
				tRoute.push_back(p0);
				for (size_t i = 0; i < sm.waypoints.size(); ++i)
				{
					BotCore::NavRoutePoint w;
					w.x = grid.CellCenter(sm.waypoints[i].x);
					w.z = grid.CellCenter(sm.waypoints[i].z);
					tRoute.push_back(w);
				}
				tPos = 0.0f;
				return;
			}
		};
		if (targetKind == 5)
			planTargetRoute();

		drive.BeginFollow(0);

		BotCore::Rng tickRng(seed);
		int64_t lastPacketT = 0;
		float lastPacketX = target.x;
		float lastPacketZ = target.z;
		int16_t lastPacketSpeed = (targetKind == 4) ? 0 : 45;
		int64_t nextPacketMs = 0;
		bool havePacket = false;
		bool prevMoving = target.moving;

		float bx = botX;
		float bz = botZ;
		int64_t lastBotPacket = -1000000;
		int64_t lastMoveMs = 0;
		int64_t prevT = 0;
		bool firstStage1 = false;
		bool stopAfterRecovery = false;

		for (int64_t t = 0; t <= durationMs; )
		{
			const int64_t dt = t - prevT;
			prevT = t;

			if (targetKind == 5)
			{
				float len = 0.0f;
				for (size_t i = 0; i + 1 < tRoute.size(); ++i)
					len += Dist2D(tRoute[i].x, tRoute[i].z, tRoute[i + 1].x, tRoute[i + 1].z);
				tPos += targetSpeed * (float)dt / 1000.0f;
				if (tRoute.size() < 2 || (len > 0.0f && tPos >= len))
				{
					planTargetRoute();
				}
				float rem = tPos;
				for (size_t i = 0; i + 1 < tRoute.size(); ++i)
				{
					const float seg = Dist2D(tRoute[i].x, tRoute[i].z, tRoute[i + 1].x, tRoute[i + 1].z);
					if (rem <= seg || i + 2 >= tRoute.size())
					{
						const float u = seg > 0.0f ? rem / seg : 0.0f;
						target.x = tRoute[i].x + u * (tRoute[i + 1].x - tRoute[i].x);
						target.z = tRoute[i].z + u * (tRoute[i + 1].z - tRoute[i].z);
						break;
					}
					rem -= seg;
				}
				target.moving = true;
			}
			else
			{
				target.Advance(dt);
			}

			if (!havePacket)
			{
				lastPacketT = t;
				lastPacketX = target.x;
				lastPacketZ = target.z;
				lastPacketSpeed = target.moving ? (int16_t)(target.speed * 10.0f) : 0;
				havePacket = true;
				nextPacketMs = t + 1500 + (int64_t)tickRng.NextRange(-100, 100);
			}
			else if (target.moving)
			{
				if (t >= nextPacketMs)
				{
					lastPacketT = t;
					lastPacketX = target.x;
					lastPacketZ = target.z;
					lastPacketSpeed = (int16_t)(target.speed * 10.0f);
					nextPacketMs = t + 1500 + (int64_t)tickRng.NextRange(-100, 100);
				}
			}
			else if (prevMoving && !target.moving)
			{
				lastPacketT = t;
				lastPacketX = target.x;
				lastPacketZ = target.z;
				lastPacketSpeed = 0;
				nextPacketMs = t;
			}
			prevMoving = target.moving;

			const bool visible = !(invisibleFromMs >= 0 && t >= invisibleFromMs
				&& (invisibleToMs < 0 || t < invisibleToMs));
			if (visible && havePacket)
				drive.ObserveTarget(lastPacketT, lastPacketX, lastPacketZ, lastPacketSpeed, t);

			BotCore::NavDriveEvents ev = (reach != nullptr)
				? drive.TickFollow(grid, finder, t, bx, bz, 4.5f, params, &scratch, *reach)
				: drive.TickFollow(grid, finder, t, bx, bz, 4.5f, params, &scratch);

			++out.ticks;
			if (ev.planned)
			{
				++out.plans;
				if (ev.planStatus == BotCore::NavFollowStatus::NoGoal)
					++out.noGoalPlans;
				else if (ev.planStatus == BotCore::NavFollowStatus::PathFailed)
					++out.pathFailPlans;
				else if (ev.planStatus == BotCore::NavFollowStatus::InvalidStart)
					++out.invalidStartPlans;
				if (ev.planStatus != BotCore::NavFollowStatus::Planned)
					++out.planFails;
			}
			if (ev.recovery.action != BotCore::NavRecoveryAction::None)
			{
				if (out.firstRecoveryMs < 0)
					out.firstRecoveryMs = t;
				out.stageSeq.push_back(ev.recovery.stage);
				if (ev.recovery.stage > out.maxStage)
					out.maxStage = ev.recovery.stage;
				if (!firstStage1 && ev.recovery.stage == 1)
				{
					firstStage1 = true;
					out.firstStage1DelayMs = (int)(t - lastMoveMs);
				}
				if (stopOnFirstRecovery)
					stopAfterRecovery = true;
			}
			if (ev.recovery.recovered && ev.recovery.recoverMs > out.maxRecoverMs)
				out.maxRecoverMs = ev.recovery.recoverMs;
			if (ev.holdStop)
				++out.holdStops;
			if (ev.verdict == BotCore::NavProgressVerdict::Stalled)
				++out.stalledTicks;
			else if (ev.verdict == BotCore::NavProgressVerdict::AwaitingPacket)
				++out.awaitingTicks;
			else if (ev.verdict == BotCore::NavProgressVerdict::BlockedByGuard)
				++out.guardTicks;
			else if (ev.verdict == BotCore::NavProgressVerdict::Progressing)
				++out.progressingTicks;

			if (ev.ended != BotCore::NavFollowEnd::None)
			{
				out.ended = (int)ev.ended;
				out.endedMs = t;
				if (ev.ended == BotCore::NavFollowEnd::TargetLost) ++out.endTargetLost;
				else if (ev.ended == BotCore::NavFollowEnd::PlanFailed) ++out.endPlanFailed;
				else if (ev.ended == BotCore::NavFollowEnd::StuckAbandon) ++out.endStuck;
				else if (ev.ended == BotCore::NavFollowEnd::PathBlocked) ++out.endBlocked;
				break;
			}

			if (!stopAfterRecovery && t - lastBotPacket >= (int64_t)BotCore::kMovePeriodMs)
			{
				const BotCore::NavDriveStep step = drive.NextFollowStep(grid, t, bx, bz, 6.75f, params);
				if (step.kind == BotCore::NavDriveStep::Step || step.kind == BotCore::NavDriveStep::Arrived)
				{
					++out.packets;
					out.lastPacketMs = t;
					if (step.kind == BotCore::NavDriveStep::Arrived)
						++out.arrivedPackets;

					const bool guardReject = (guardStartMs >= 0 && t >= guardStartMs && t <= guardEndMs);
					if (guardReject)
					{
						drive.OnPacketRejected(t);
					}
					else
					{
						const double ddx = (double)step.x - (double)bx;
						const double ddz = (double)step.z - (double)bz;
						const double len = std::sqrt(ddx * ddx + ddz * ddz);
						const int samples = (int)std::ceil(len / 0.25);
						int best = 0;
						for (int i = 1; i <= samples; ++i)
						{
							const double u = (double)i / (double)samples;
							const float qx = (float)((double)bx + ddx * u);
							const float qz = (float)((double)bz + ddz * u);
							if (!grid.Walk(grid.CellOf(qx), grid.CellOf(qz)))
							{
								++out.chordViolations;
								break;
							}
							if (hidden.count(std::make_pair(grid.CellOf(qx), grid.CellOf(qz))))
								break;
							if (clipComponent >= 0 && reach != nullptr
								&& reach->ComponentOf(grid.CellOf(qx), grid.CellOf(qz)) != clipComponent)
								break;
							best = i;
						}
						const double u = samples > 0 ? (double)best / (double)samples : 0.0;
						const float nx = (float)((double)bx + ddx * u);
						const float nz = (float)((double)bz + ddz * u);
						if (nx != bx || nz != bz)
							lastMoveMs = t;
						bx = nx;
						bz = nz;
						drive.OnPacketSent(t, step);
					}
					lastBotPacket = t;
				}
			}

			out.finalBotX = bx;
			out.finalBotZ = bz;
			out.finalDist = Dist2D(bx, bz, target.x, target.z);
			out.dist.push_back((double)out.finalDist);

			t += SimNextTick(tickRng, tickModel);
		}

		out.finalBotX = bx;
		out.finalBotZ = bz;
		out.finalDist = Dist2D(bx, bz, target.x, target.z);
		return out;
	}

	// Pocket grid: border blocked; a 7x7 wall box (interior 5x5 open) around (20,20).
	BotCore::NavGrid MakePocketGrid40()
	{
		const int n = 40;
		std::vector<int16_t> ev((size_t)n * n, 1);
		for (int x = 0; x < n; ++x)
			for (int z = 0; z < n; ++z)
				if (x == 0 || x == n - 1 || z == 0 || z == n - 1)
					ev[CellIndex(n, x, z)] = 0;
		for (int x = 17; x <= 23; ++x)
			for (int z = 17; z <= 23; ++z)
				if (x == 17 || x == 23 || z == 17 || z == 23)
					ev[CellIndex(n, x, z)] = 0;
		return MakeNav(n, 4.0f, ev, HeightZeros(n));
	}

	bool StageSeqContainsInOrder(const std::vector<int> & seq, const std::vector<int> & want)
	{
		size_t j = 0;
		for (size_t i = 0; i < seq.size() && j < want.size(); ++i)
		{
			if (seq[i] == want[j])
				++j;
		}
		return j == want.size();
	}
}

TEST_CASE("NavDriveFollow_Lifecycle")
{
	const int n = 64;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavDriveParams gp;
	BotCore::NavFollowDriveParams fp;

	drive.BeginFollow(0);
	CHECK(drive.Mode() == BotCore::NavDriveMode::Follow);
	CHECK(drive.Active());

	CHECK(drive.Replan(grid, finder, 20.0f, 20.0f, gp) == BotCore::NavPlanStatus::None);
	CHECK(drive.Mode() == BotCore::NavDriveMode::Follow);

	drive.ObserveTarget(0, 200.0f, 200.0f, 45, 0);
	CHECK(drive.TickFollow(grid, finder, 0, 20.0f, 20.0f, 4.5f, fp, nullptr).planned);

	// BeginGoto resets the follow state.
	REQUIRE(drive.BeginGoto(grid, finder, 20.0f, 20.0f, 200.0f, 200.0f, gp) == BotCore::NavPlanStatus::Planned);
	CHECK(drive.Mode() == BotCore::NavDriveMode::Goto);
	CHECK_EQ(drive.RecoveryStage(), 0);
	CHECK_EQ(drive.FollowPlans(), 0);

	// Goto mode ignores TickFollow / ObserveTarget.
	CHECK(!drive.TickFollow(grid, finder, 100, 20.0f, 20.0f, 4.5f, fp, nullptr).planned);
	drive.ObserveTarget(100, 200.0f, 200.0f, 45, 100);
	CHECK(!drive.TickFollow(grid, finder, 200, 20.0f, 20.0f, 4.5f, fp, nullptr).planned);

	drive.Reset();
	CHECK(drive.Mode() == BotCore::NavDriveMode::Off);
	CHECK(!drive.Active());

	// Invalid bot coordinates leave Follow inactive.
	drive.BeginFollow(0);
	drive.ObserveTarget(0, 200.0f, 200.0f, 45, 0);
	CHECK(!drive.TickFollow(grid, finder, 0, std::nanf(""), 20.0f, 4.5f, fp, nullptr).planned);
	CHECK(!drive.TickFollow(grid, finder, 0, -1.0f, 20.0f, 4.5f, fp, nullptr).planned);
	CHECK(!drive.TickFollow(grid, finder, 0, 7000.0f, 20.0f, 4.5f, fp, nullptr).planned);
	CHECK(drive.TickFollow(grid, finder, 0, 20.0f, 20.0f, 4.5f, fp, nullptr).planned);

	std::printf("NAVFOLLOW sizeof(NavDrive)=%d\n", (int)sizeof(BotCore::NavDrive));
}

TEST_CASE("NavDriveFollow_RingNeverEmpty")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavFollowDriveParams fp;

	int holes6 = 0;
	int total = 0;
	for (int i = 0; i <= 16; ++i)
	{
		for (int j = 0; j <= 16; ++j)
		{
			const float ox = (float)i * 0.25f;
			const float oz = (float)j * 0.25f;
			const float tx = 200.0f + ox;
			const float tz = 200.0f + oz;

			BotCore::NavDrive d1;
			d1.BeginFollow(0);
			d1.ObserveTarget(0, tx, tz, 45, 0);
			const BotCore::NavDriveEvents ev = d1.TickFollow(grid, finder, 0, 40.0f, 40.0f, 4.5f, fp, nullptr);
			++total;
			CHECK(ev.planned);
			CHECK(ev.planStatus == BotCore::NavFollowStatus::Planned);

			BotCore::NavFollowDriveParams narrow = fp;
			narrow.follow.ringMaxM = 6.0f;
			BotCore::NavDrive d2;
			d2.BeginFollow(0);
			d2.ObserveTarget(0, tx, tz, 45, 0);
			const BotCore::NavDriveEvents ev2 = d2.TickFollow(grid, finder, 0, 40.0f, 40.0f, 4.5f, narrow, nullptr);
			if (ev2.planned && ev2.planStatus == BotCore::NavFollowStatus::NoGoal)
				++holes6;
		}
	}
	CHECK_EQ(total, 289);
	CHECK(holes6 >= 1);
	std::printf("NAVFOLLOW ring holes6=%d total=%d\n", holes6, total);
}

TEST_CASE("NavDriveFollow_PlanAdoptsRoute")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavFollowDriveParams fp;

	drive.BeginFollow(0);
	drive.ObserveTarget(0, 200.0f, 200.0f, -1, 0);
	const BotCore::NavDriveEvents ev = drive.TickFollow(grid, finder, 0, 40.0f, 200.0f, 4.5f, fp, nullptr);
	REQUIRE(ev.planned);
	CHECK(ev.planStatus == BotCore::NavFollowStatus::Planned);
	CHECK(ev.planReason == BotCore::NavReplanReason::First);
	CHECK(ev.routeAdopted);

	const BotCore::NavCell goal = drive.Follower().Plan().goal;
	REQUIRE(drive.Route().size() >= 2);
	CHECK(std::fabs(drive.Route().back().x - BotCore::NavQuantiseM(grid.CellCenter(goal.x))) <= 1e-4f);
	CHECK(std::fabs(drive.Route().back().z - BotCore::NavQuantiseM(grid.CellCenter(goal.z))) <= 1e-4f);

	const BotCore::NavDriveStep step = drive.NextFollowStep(grid, 0, 40.0f, 200.0f, 6.75f, fp);
	CHECK(step.kind == BotCore::NavDriveStep::Step || step.kind == BotCore::NavDriveStep::Arrived);
	CHECK(BotCore::CheckMoveChord(&grid, 40.0f, 200.0f, step.x, step.z).verdict == BotCore::ChordVerdict::Ok);

	// Before a packet is sent the assessor waits; after OnPacketSent it reports progress.
	CHECK(drive.TickFollow(grid, finder, 100, 40.0f, 200.0f, 4.5f, fp, nullptr).verdict
		== BotCore::NavProgressVerdict::AwaitingPacket);
	drive.OnPacketSent(0, step);
	CHECK(drive.TickFollow(grid, finder, 100, 40.0f, 200.0f, 4.5f, fp, nullptr).verdict
		== BotCore::NavProgressVerdict::Progressing);

	const float d = Dist2D(200.0f, 200.0f, grid.CellCenter(goal.x), grid.CellCenter(goal.z));
	CHECK(d >= 2.9f);
	CHECK(d <= 6.5f);
}

TEST_CASE("NavDriveFollow_ChaseConstantVelocity")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;

	// Tick model variability (the FollowSim contract).
	{
		BotCore::Rng r1(11u);
		std::set<int64_t> vals;
		for (int i = 0; i < 100; ++i)
			vals.insert(SimNextTick(r1, 1));
		CHECK(vals.size() >= 2);
		BotCore::Rng r2(22u);
		bool sawLate = false;
		for (int i = 0; i < 3000; ++i)
			if (SimNextTick(r2, 2) > 300)
				sawLate = true;
		CHECK(sawLate);
	}

	for (int model = 0; model < 3; ++model)
	{
		FollowSim sim(grid, finder);
		sim.tickModel = model;
		sim.targetKind = 0;
		sim.seed = (uint32_t)(20261003u + (uint32_t)model);
		sim.botX = 70.0f;
		sim.botZ = 200.0f;
		const FollowRun r = sim.Run(60000);

		CHECK_EQ(r.ended, 0);
		CHECK_EQ(r.stalledTicks, 0);
		CHECK_EQ(r.chordViolations, 0);
		CHECK(r.plans <= 150);

		// Distance after 10 s (sample index ~10 s / 100 ms).
		const size_t idx = 100;
		double d10 = 0.0;
		if (r.dist.size() > idx)
			d10 = r.dist[idx];
		CHECK(d10 <= 30.0);

		std::vector<double> sorted = r.dist;
		std::sort(sorted.begin(), sorted.end());
		std::printf("NAVFOLLOW chase model=%d: dist p50=%.2f p95=%.2f max=%.2f plans=%d\n",
			model, PercentileDouble(sorted, 0.50), PercentileDouble(sorted, 0.95),
			sorted.empty() ? 0.0 : sorted.back(), r.plans);
	}
}

TEST_CASE("NavDriveFollow_TargetStops_BotStops")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;

	// The target walks for 30 s then stops; run 90 s.
	FollowSim sim(grid, finder);
	sim.tickModel = 0;
	sim.targetKind = 6;
	sim.targetStartX = 80.0f;
	sim.targetStartZ = 200.0f;
	sim.targetStopAtMs = 30000;
	sim.botX = 70.0f;
	sim.botZ = 200.0f;

	const FollowRun r = sim.Run(90000);
	CHECK_EQ(r.ended, 0);
	CHECK(r.finalDist >= 2.9f);
	CHECK(r.finalDist <= 6.5f);
	CHECK(r.arrivedPackets >= 1);
	CHECK(r.arrivedPackets <= 3);
	CHECK(r.maxStage == 0);
	CHECK(r.lastPacketMs >= 0);
	CHECK(r.lastPacketMs <= 30000 + 6000);
	std::printf("NAVFOLLOW stopgo: final=%.2f arrived=%d packets=%d last=%lld\n",
		r.finalDist, r.arrivedPackets, r.packets, (long long)r.lastPacketMs);
}

TEST_CASE("NavDriveFollow_ReplanOnMove")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavFollowDriveParams fp;

	drive.BeginFollow(0);
	// Stationary target observed for 10 s: the bot arrives and latches.
	for (int64_t t = 0; t <= 10000; t += 100)
	{
		drive.ObserveTarget(0, 200.0f, 200.0f, 0, t);
		drive.TickFollow(grid, finder, t, 150.0f, 200.0f, 4.5f, fp, nullptr);
	}
	CHECK(drive.FollowPlans() > 0);

	// Verify no Moved reason before the jump and count plans before.
	const BotCore::NavDriveEvents pre = drive.TickFollow(grid, finder, 10100, 150.0f, 200.0f, 4.5f, fp, nullptr);
	CHECK(pre.planReason != BotCore::NavReplanReason::Moved);

	// Target jumps 10 m east.
	drive.ObserveTarget(10100, 210.0f, 200.0f, 45, 10200);
	const BotCore::NavDriveEvents ev = drive.TickFollow(grid, finder, 10200, 150.0f, 200.0f, 4.5f, fp, nullptr);
	REQUIRE(ev.planned);
	CHECK(ev.planReason == BotCore::NavReplanReason::Moved);
	CHECK(ev.routeAdopted);

	const BotCore::NavDriveStep step = drive.NextFollowStep(grid, 10200, 150.0f, 200.0f, 6.75f, fp);
	CHECK(step.kind == BotCore::NavDriveStep::Step || step.kind == BotCore::NavDriveStep::Arrived);
}

TEST_CASE("NavDriveFollow_UTurn_NoFalseStuck")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;

	for (int model = 0; model < 3; ++model)
	{
		FollowSim sim(grid, finder);
		sim.tickModel = model;
		sim.targetKind = 0;
		sim.seed = (uint32_t)(777u + (uint32_t)model);
		sim.botX = 200.0f;
		sim.botZ = 200.0f;
		sim.targetStartX = 180.0f;
		sim.targetStartZ = 200.0f;
		sim.lineMinX = 180.0f;
		sim.lineMaxX = 220.0f;
		const FollowRun r = sim.Run(600000);
		CHECK_EQ(r.ended, 0);
		CHECK_EQ(sim.drive.StuckEpisodes(), 0);
		CHECK_EQ(r.maxStage, 0);
	}
}

TEST_CASE("NavDriveFollow_NormalChase_NoFalseStuck")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;

	for (int model = 0; model < 3; ++model)
	{
		{
			FollowSim sim(grid, finder);
			sim.tickModel = model;
			sim.targetKind = 2;
			sim.botX = 70.0f;
			sim.botZ = 200.0f;
			const FollowRun r = sim.Run(600000);
			CHECK_EQ(r.ended, 0);
			CHECK_EQ(r.planFails, 0);
			CHECK_EQ(sim.drive.StuckEpisodes(), 0);
		}
		for (uint32_t s = 0; s < 8; ++s)
		{
			FollowSim sim(grid, finder);
			sim.tickModel = model;
			sim.targetKind = 3;
			sim.seed = (uint32_t)(9001u + s * 131u + (uint32_t)model);
			sim.targetSeed = (uint32_t)(4001u + s * 97u);
			sim.botX = 200.0f;
			sim.botZ = 200.0f;
			const FollowRun r = sim.Run(600000);
			CHECK_EQ(r.ended, 0);
			CHECK_EQ(r.planFails, 0);
			CHECK_EQ(sim.drive.StuckEpisodes(), 0);
		}
	}
}

TEST_CASE("NavDriveFollow_Replan_NoFalseStalled")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;

	for (int model = 0; model < 3; ++model)
	{
		FollowSim sim(grid, finder);
		sim.tickModel = model;
		sim.targetKind = 1;
		sim.seed = (uint32_t)(555u + (uint32_t)model);
		sim.botX = 200.0f;
		sim.botZ = 200.0f;
		const FollowRun r = sim.Run(600000);
		CHECK_EQ(r.ended, 0);
		CHECK_EQ(r.stalledTicks, 0);
		CHECK_EQ(sim.drive.StuckEpisodes(), 0);
		CHECK(r.plans > 1000);
	}
}

TEST_CASE("NavDriveFollow_HiddenObstacle_Recovery")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;

	for (int model = 0; model < 3; ++model)
	{
		FollowSim sim(grid, finder);
		sim.tickModel = model;
		sim.targetKind = 4;
		sim.targetStartX = 300.0f;
		sim.targetStartZ = 200.0f;
		sim.botX = 60.0f;
		sim.botZ = 200.0f;
		sim.hidden.insert(std::make_pair(45, 50));
		const FollowRun r = sim.Run(200000);
		CHECK_EQ(r.ended, 0);
		CHECK(sim.drive.StuckEpisodes() >= 1);
		CHECK(r.maxRecoverMs >= 0);
		CHECK(r.maxRecoverMs <= 5000);
		CHECK(r.firstStage1DelayMs >= 3200);
		CHECK(r.firstStage1DelayMs <= 3700);
		CHECK(r.arrivedPackets >= 1);
		CHECK(r.finalDist >= 2.9f);
		CHECK(r.finalDist <= 6.5f);
		CHECK(StageSeqContainsInOrder(r.stageSeq, { 1, 2 }));
		std::printf("NAVFOLLOW hidden model=%d: firstStage1=%d maxRecover=%d final=%.2f stages=%d\n",
			model, r.firstStage1DelayMs, r.maxRecoverMs, r.finalDist, r.maxStage);
	}

	// Full-width hidden wall: the ladder climbs to Abandon.
	{
		FollowSim sim(grid, finder);
		sim.tickModel = 2;
		sim.targetKind = 4;
		sim.targetStartX = 300.0f;
		sim.targetStartZ = 200.0f;
		sim.botX = 60.0f;
		sim.botZ = 200.0f;
		for (int z = 1; z <= 98; ++z)
			sim.hidden.insert(std::make_pair(45, z));
		const FollowRun r = sim.Run(90000);
		CHECK(r.ended == (int)BotCore::NavFollowEnd::StuckAbandon);
		CHECK(r.endedMs >= 0);
		CHECK(r.endedMs <= 90000);
		CHECK(StageSeqContainsInOrder(r.stageSeq, { 1, 2, 3, 4, 5 }));
		std::printf("NAVFOLLOW wall: ended=%d at=%lld stages=%d\n", r.ended, (long long)r.endedMs, r.maxStage);
	}
}

TEST_CASE("NavDriveFollow_RecoveryNotCancelledByAwaiting")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;

	FollowSim sim(grid, finder);
	sim.tickModel = 0;
	sim.targetKind = 4;
	sim.targetStartX = 300.0f;
	sim.targetStartZ = 200.0f;
	sim.botX = 60.0f;
	sim.botZ = 200.0f;
	sim.hidden.insert(std::make_pair(45, 50));
	sim.stopOnFirstRecovery = true;
	const FollowRun r = sim.Run(60000);
	CHECK(r.ended == (int)BotCore::NavFollowEnd::StuckAbandon);
	CHECK(r.firstRecoveryMs >= 0);
	CHECK(r.endedMs - r.firstRecoveryMs >= 3000);
	CHECK(r.endedMs - r.firstRecoveryMs <= 5500);   // ~4 s after the first recovery action
	std::printf("NAVFOLLOW awaiting-cancel: ended=%d first=%lld at=%lld\n", r.ended,
		(long long)r.firstRecoveryMs, (long long)r.endedMs);
}

TEST_CASE("NavDriveFollow_GuardRejected_NotStuck")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;

	FollowSim sim(grid, finder);
	sim.tickModel = 0;
	sim.targetKind = 0;
	sim.botX = 70.0f;
	sim.botZ = 200.0f;
	sim.guardStartMs = 20000;
	sim.guardEndMs = 50000;
	const FollowRun r = sim.Run(60000);
	CHECK_EQ(r.ended, 0);
	CHECK(r.guardTicks > 0);
	CHECK_EQ(sim.drive.StuckEpisodes(), 0);
	CHECK(r.packets > 0);
	std::printf("NAVFOLLOW guard: guardTicks=%d packets=%d\n", r.guardTicks, r.packets);
}

TEST_CASE("NavDriveFollow_TargetLost_Hold")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;

	// (a) The target disappears for 10 s: exactly one holdStop, no end, then chase resumes.
	{
		FollowSim sim(grid, finder);
		sim.tickModel = 0;
		sim.targetKind = 0;
		sim.botX = 70.0f;
		sim.botZ = 200.0f;
		sim.invisibleFromMs = 20000;
		sim.invisibleToMs = 30000;
		const FollowRun r = sim.Run(60000);
		CHECK_EQ(r.holdStops, 1);
		CHECK_EQ(r.ended, 0);
		CHECK(r.plans > 0);
	}

	// (b) The target disappears for good: ended TargetLost at last seen + 15 s.
	{
		FollowSim sim(grid, finder);
		sim.tickModel = 0;
		sim.targetKind = 0;
		sim.botX = 70.0f;
		sim.botZ = 200.0f;
		sim.invisibleFromMs = 20000;
		sim.invisibleToMs = -1;
		const FollowRun r = sim.Run(60000);
		CHECK(r.ended == (int)BotCore::NavFollowEnd::TargetLost);
		CHECK(r.endedMs >= 20000 + 15000 - 200);
		CHECK(r.endedMs <= 20000 + 15000 + 500);
		std::printf("NAVFOLLOW lost: ended=%d at=%lld\n", r.ended, (long long)r.endedMs);
	}

	// (c) A stopped target keeps being observed with its old stamp: never a holdStop.
	{
		FollowSim sim(grid, finder);
		sim.tickModel = 0;
		sim.targetKind = 4;
		sim.targetStartX = 200.0f;
		sim.targetStartZ = 200.0f;
		sim.botX = 70.0f;
		sim.botZ = 200.0f;
		const FollowRun r = sim.Run(120000);
		CHECK_EQ(r.holdStops, 0);
		CHECK_EQ(r.ended, 0);
	}
}

TEST_CASE("NavDriveFollow_PlanFailed_Abandon")
{
	NavGrid grid = MakePocketGrid40();
	BotCore::NavPathfinder finder;
	BotCore::NavFollowDriveParams fp;

	const float px = grid.CellCenter(20);
	const float pz = grid.CellCenter(20);
	const float bx = grid.CellCenter(5);
	const float bz = grid.CellCenter(20);

	// (a) Reach-less: ten consecutive failed plans end the drive.
	{
		BotCore::NavDrive drive;
		drive.BeginFollow(0);
		int64_t t = 0;
		int plans = 0;
		BotCore::NavFollowEnd ended = BotCore::NavFollowEnd::None;
		int64_t endedAt = -1;
		for (int i = 0; i < 30; ++i)
		{
			drive.ObserveTarget(0, px, pz, -1, t);
			const BotCore::NavDriveEvents ev = drive.TickFollow(grid, finder, t, bx, bz, 4.5f, fp, nullptr);
			if (ev.planned)
				++plans;
			if (ev.ended != BotCore::NavFollowEnd::None)
			{
				ended = ev.ended;
				endedAt = t;
				break;
			}
			t += 500;
		}
		CHECK(ended == BotCore::NavFollowEnd::PlanFailed);
		CHECK(plans >= 9);
		CHECK(plans <= 11);
		CHECK(endedAt <= 6000);
	}

	// (b) With NavReach every ring cell is skipped: tries == 0 and the drive still ends.
	{
		BotCore::NavReach reach;
		reach.Build(grid);
		BotCore::NavDrive drive;
		BotCore::NavCostLayer reachScratch;
		drive.BeginFollow(0);
		int64_t t = 0;
		BotCore::NavFollowEnd ended = BotCore::NavFollowEnd::None;
		bool zeroTries = true;
		for (int i = 0; i < 30; ++i)
		{
			drive.ObserveTarget(0, px, pz, -1, t);
			const BotCore::NavDriveEvents ev = drive.TickFollow(grid, finder, t, bx, bz, 4.5f, fp, &reachScratch, reach);
			(void)ev;
			if (drive.Follower().Plan().status != BotCore::NavFollowStatus::Planned
				&& drive.Follower().Plan().status != BotCore::NavFollowStatus::NoTarget
				&& drive.Follower().Plan().tries != 0)
				zeroTries = false;
			if (ev.ended != BotCore::NavFollowEnd::None)
			{
				ended = ev.ended;
				break;
			}
			t += 500;
		}
		CHECK(ended == BotCore::NavFollowEnd::PlanFailed);
		CHECK(zeroTries);
	}

	// (c) One successful plan resets the counter: nine failures + success + nine failures do not end.
	{
		BotCore::NavDrive drive;
		drive.BeginFollow(0);
		int64_t t = 0;
		for (int i = 0; i < 9; ++i)
		{
			drive.ObserveTarget(0, px, pz, -1, t);
			drive.TickFollow(grid, finder, t, bx, bz, 4.5f, fp, nullptr);
			t += 500;
		}
		// Reachable target (bot cell area) resets the failure counter.
		drive.ObserveTarget(t, bx, bz, -1, t);
		drive.TickFollow(grid, finder, t, bx, bz, 4.5f, fp, nullptr);
		t += 500;
		BotCore::NavFollowEnd ended = BotCore::NavFollowEnd::None;
		for (int i = 0; i < 9; ++i)
		{
			drive.ObserveTarget(0, px, pz, -1, t);
			const BotCore::NavDriveEvents ev = drive.TickFollow(grid, finder, t, bx, bz, 4.5f, fp, nullptr);
			if (ev.ended != BotCore::NavFollowEnd::None)
				ended = ev.ended;
			t += 500;
		}
		CHECK(ended == BotCore::NavFollowEnd::None);
	}
}

TEST_CASE("NavDriveFollow_PenalizeReplan_AvoidsCell")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;

	FollowSim sim(grid, finder);
	sim.tickModel = 0;
	sim.targetKind = 4;
	sim.targetStartX = 300.0f;
	sim.targetStartZ = 200.0f;
	sim.botX = 60.0f;
	sim.botZ = 200.0f;
	for (int z = 1; z <= 98; ++z)
		sim.hidden.insert(std::make_pair(45, z));

	BotCore::NavDrive & drive = sim.drive;
	BotCore::NavFollowDriveParams & fp = sim.params;
	BotCore::NavCostLayer scratch;

	drive.BeginFollow(0);
	drive.ObserveTarget(0, 300.0f, 200.0f, -1, 0);

	int64_t t = 0;
	bool penalized = false;
	bool sawNewRoute = false;
	int oldPoints = 0;
	int newPoints = 0;
	int penalizedCellX = -1;
	int penalizedCellZ = -1;
	bool newRouteHasPenalized = false;

	float bx = 60.0f;
	float bz = 200.0f;
	int64_t lastPkt = -1000000;
	for (int i = 0; i < 900; ++i)
	{
		drive.ObserveTarget(0, 300.0f, 200.0f, -1, t);
		const BotCore::NavDriveEvents ev = drive.TickFollow(grid, finder, t, bx, bz, 4.5f, fp, &scratch);

		if (penalized && !sawNewRoute && ev.routeAdopted && ev.ended == BotCore::NavFollowEnd::None)
		{
			sawNewRoute = true;
			const std::vector<BotCore::NavRoutePoint> & after = drive.Route();
			newPoints = (int)after.size();
			for (size_t k = 0; k + 1 < after.size(); ++k)
			{
				const double ddx = (double)after[k + 1].x - (double)after[k].x;
				const double ddz = (double)after[k + 1].z - (double)after[k].z;
				const double len = std::sqrt(ddx * ddx + ddz * ddz);
				const int samples = (int)std::ceil(len / 0.25);
				for (int s = 0; s <= samples; ++s)
				{
					const double u = samples > 0 ? (double)s / (double)samples : 0.0;
					const float qx = (float)((double)after[k].x + ddx * u);
					const float qz = (float)((double)after[k].z + ddz * u);
					if (grid.CellOf(qx) == penalizedCellX && grid.CellOf(qz) == penalizedCellZ)
						newRouteHasPenalized = true;
				}
			}
		}

		if (ev.recovery.action == BotCore::NavRecoveryAction::PenalizeReplan && !penalized)
		{
			penalized = true;
			const std::vector<BotCore::NavRoutePoint> & before = drive.Route();
			oldPoints = (int)before.size();
			const float p = BotCore::NavRouteProgressM(before.data(), (int)before.size(), bx, bz);
			float rx = bx;
			float rz = bz;
			float cum = 0.0f;
			const float target = p + grid.Unit();
			for (size_t k = 0; k + 1 < before.size(); ++k)
			{
				const float seg = Dist2D(before[k].x, before[k].z, before[k + 1].x, before[k + 1].z);
				if (cum + seg >= target || k + 2 >= before.size())
				{
					const float u = seg > 0.0f ? (target - cum) / seg : 0.0f;
					rx = before[k].x + u * (before[k + 1].x - before[k].x);
					rz = before[k].z + u * (before[k + 1].z - before[k].z);
					break;
				}
				cum += seg;
			}
			penalizedCellX = grid.CellOf(rx);
			penalizedCellZ = grid.CellOf(rz);
		}

		if (ev.ended != BotCore::NavFollowEnd::None)
			break;

		// Move the bot along hidden-wall clipping (keeps it stuck at the wall).
		if (t - lastPkt >= (int64_t)BotCore::kMovePeriodMs)
		{
			const BotCore::NavDriveStep step = drive.NextFollowStep(grid, t, bx, bz, 6.75f, fp);
			if (step.kind == BotCore::NavDriveStep::Step || step.kind == BotCore::NavDriveStep::Arrived)
			{
				const double ddx = (double)step.x - (double)bx;
				const double ddz = (double)step.z - (double)bz;
				const double len = std::sqrt(ddx * ddx + ddz * ddz);
				const int samples = (int)std::ceil(len / 0.25);
				int best = 0;
				for (int s = 1; s <= samples; ++s)
				{
					const double u = (double)s / (double)samples;
					const int cx = grid.CellOf((float)((double)bx + ddx * u));
					const int cz = grid.CellOf((float)((double)bz + ddz * u));
					if (sim.hidden.count(std::make_pair(cx, cz)))
						break;
					best = s;
				}
				const double u = samples > 0 ? (double)best / (double)samples : 0.0;
				bx = (float)((double)bx + ddx * u);
				bz = (float)((double)bz + ddz * u);
				drive.OnPacketSent(t, step);
				lastPkt = t;
			}
		}
		t += 100;
	}

	CHECK(penalized);
	CHECK(sawNewRoute);
	CHECK(!newRouteHasPenalized);
	CHECK(newPoints > oldPoints);
	std::printf("NAVFOLLOW penalize: old=%d new=%d cell=(%d,%d)\n", oldPoints, newPoints, penalizedCellX, penalizedCellZ);

	// scratch == nullptr: no crash, the route is not expanded by the penalty field.
	{
		BotCore::NavDrive d2;
		d2.BeginFollow(0);
		d2.ObserveTarget(0, 300.0f, 200.0f, -1, 0);
		int64_t s = 0;
		float x = 60.0f;
		float z = 200.0f;
		for (int i = 0; i < 20; ++i)
		{
			d2.ObserveTarget(0, 300.0f, 200.0f, -1, s);
			const BotCore::NavDriveEvents ev = d2.TickFollow(grid, finder, s, x, z, 4.5f, fp, nullptr);
			if (ev.ended != BotCore::NavFollowEnd::None)
				break;
			s += 500;
		}
		CHECK(true);   // reaching here without a crash is the assertion
	}
}

TEST_CASE("NavDriveFollow_BlockedLoop_Abandon")
{
	const int n = 64;
	NavGrid open = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	std::vector<int16_t> none((size_t)n * n, 0);
	NavGrid allBlocked = MakeNav(n, 4.0f, none, HeightZeros(n));

	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavFollowDriveParams fp;

	drive.BeginFollow(0);
	drive.ObserveTarget(0, 200.0f, 20.0f, -1, 0);
	drive.TickFollow(open, finder, 0, 20.0f, 20.0f, 4.5f, fp, nullptr);

	int64_t t = 1500;
	bool blocked4s = true;
	BotCore::NavFollowEnd ended = BotCore::NavFollowEnd::None;
	for (int i = 0; i < 20; ++i)
	{
		drive.ObserveTarget(0, 200.0f, 20.0f, -1, t);
		const BotCore::NavDriveEvents ev = drive.TickFollow(open, finder, t, 20.0f, 20.0f, 4.5f, fp, nullptr);
		if (t <= 4000 && ev.ended != BotCore::NavFollowEnd::None)
			blocked4s = false;
		if (ev.ended != BotCore::NavFollowEnd::None)
		{
			ended = ev.ended;
			break;
		}
		const BotCore::NavDriveStep step = drive.NextFollowStep(allBlocked, t, 20.0f, 20.0f, 6.75f, fp);
		CHECK(step.kind == BotCore::NavDriveStep::Blocked);
		t += 1500;
	}
	CHECK(blocked4s);
	CHECK(ended == BotCore::NavFollowEnd::PathBlocked);
}

TEST_CASE("NavDriveFollow_RealMap_Chase")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "follow"))
		return;

	std::vector<NavCell> walk;
	CollectWalk(grid, walk);
	REQUIRE(!walk.empty());

	BotCore::NavReach reach;
	reach.Build(grid);
	REQUIRE(reach.ComponentCount() >= 1);

	// Restrict both start points and target routes to the largest edge-connected component so the
	// chase never starts from an isolated slope pocket (KI-024).
	const int mainId = reach.LargestComponent();
	std::vector<NavCell> mainWalk;
	for (size_t i = 0; i < walk.size(); ++i)
		if (reach.ComponentOf(walk[i].x, walk[i].z) == mainId
			&& grid.Clearance(walk[i].x, walk[i].z) >= 2)
			mainWalk.push_back(walk[i]);
	REQUIRE(!mainWalk.empty());

	BotCore::NavPathfinder finder;
	BotCore::Rng startRng(20261006u);

	long plans = 0;
	long fails = 0;
	long episodes = 0;
	long blocked = 0;
	long chordViolations = 0;
	int endedRuns = 0;
	int noGoal = 0;
	int pathFail = 0;
	int invalidStart = 0;
	int endTargetLost = 0;
	int endPlanFailed = 0;
	int endStuck = 0;
	int endBlocked = 0;

	for (int seed = 0; seed < 10; ++seed)
	{
		for (int model = 0; model < 3; ++model)
		{
			FollowSim sim(grid, finder);
			sim.tickModel = model;
			sim.targetKind = 5;
			sim.reach = &reach;
			sim.walkCells = &mainWalk;
			sim.seed = (uint32_t)(5000u + (uint32_t)seed * 17u + (uint32_t)model);
			sim.targetSeed = (uint32_t)(8000u + (uint32_t)seed * 29u);
			const NavCell tc = mainWalk[(size_t)startRng.NextBelow((uint32_t)mainWalk.size())];
			sim.targetStartX = grid.CellCenter(tc.x);
			sim.targetStartZ = grid.CellCenter(tc.z);
			// The bot starts on the target's own cell so every plan stays local (a chase, not a
			// cross-map approach).
			sim.botX = sim.targetStartX;
			sim.botZ = sim.targetStartZ;

			const FollowRun r = sim.Run(300000);
			plans += r.plans;
			fails += r.planFails;
			noGoal += r.noGoalPlans;
			pathFail += r.pathFailPlans;
			invalidStart += r.invalidStartPlans;
			endTargetLost += r.endTargetLost;
			endPlanFailed += r.endPlanFailed;
			endStuck += r.endStuck;
			endBlocked += r.endBlocked;
			episodes += sim.drive.StuckEpisodes();
			chordViolations += r.chordViolations;
			if (r.ended != 0)
				++endedRuns;
		}
	}

	std::printf("NAVFOLLOW real map: runs=30 plans=%ld fail=%ld episodes=%ld blocked=%ld chordViolations=%ld ended=%d\n",
		plans, fails, episodes, blocked, chordViolations, endedRuns);
	std::printf("NAVFOLLOW real map diag: noGoal=%d pathFail=%d invalidStart=%d endLost=%d endPlanFail=%d endStuck=%d endBlocked=%d\n",
		noGoal, pathFail, invalidStart, endTargetLost, endPlanFailed, endStuck, endBlocked);
	CHECK_EQ(chordViolations, 0);
	CHECK_EQ(fails, 0);
	CHECK_EQ(episodes, 0);
	CHECK_EQ(endedRuns, 0);
}

TEST_CASE("NavDriveFollow_Perf")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "followperf"))
		return;

	std::vector<NavCell> walk;
	CollectWalk(grid, walk);
	REQUIRE(!walk.empty());

	BotCore::NavReach reach;
	reach.Build(grid);

	BotCore::NavPathfinder finder;
	BotCore::NavDrive drive;
	BotCore::NavFollowDriveParams fp;
	BotCore::NavCostLayer scratch;
	BotCore::Rng rng(20261007u);

	// A bot cell and a target cell >= 64 cells away that A* connects, so every follow plan is a
	// long but solvable search.
	NavCell botCell = walk[0];
	NavCell farCell = walk[0];
	bool foundPair = false;
	for (int tries = 0; tries < 4000 && !foundPair; ++tries)
	{
		botCell = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		const NavCell c = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		if (std::abs(c.x - botCell.x) < 64 || std::abs(c.z - botCell.z) < 64)
			continue;
		BotCore::NavPathResult path;
		finder.Find(grid, botCell, c, BotCore::NavSearchParams(), path);
		if (path.status == BotCore::NavPathStatus::Found)
		{
			farCell = c;
			foundPair = true;
		}
	}
	REQUIRE(foundPair);

	const float bx = grid.CellCenter(botCell.x);
	const float bz = grid.CellCenter(botCell.z);
	const float fx = grid.CellCenter(farCell.x);
	const float fz = grid.CellCenter(farCell.z);
	float ux = fx - bx;
	float uz = fz - bz;
	{
		const float len = std::sqrt(ux * ux + uz * uz);
		ux /= len;
		uz /= len;
	}

	drive.BeginFollow(0);
	drive.ObserveTarget(0, fx, fz, 45, 0);
	drive.TickFollow(grid, finder, 0, bx, bz, 4.5f, fp, &scratch, reach);   // warm-up
	REQUIRE(drive.Mode() == BotCore::NavDriveMode::Follow);
	REQUIRE(drive.FollowPlans() > 0);

	// Oscillate the target +-6 m along the bot->target corridor so every call replans (Moved)
	// while the A* distance stays around 64 cells.
	const int total = 200;
	std::vector<double> ms;
	ms.reserve((size_t)total);
	int64_t t = 500;
	for (int q = 0; q < total; ++q)
	{
		const float off = (q % 2 == 0) ? 6.0f : 0.0f;
		const float tx = fx + ux * off;
		const float tz = fz + uz * off;
		drive.ObserveTarget(t, tx, tz, 45, t);
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		drive.TickFollow(grid, finder, t, bx, bz, 4.5f, fp, &scratch, reach);
		const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
		ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
		t += 500;
	}

	std::sort(ms.begin(), ms.end());
	const double p95 = PercentileDouble(ms, 0.95);
	std::printf("NAVFOLLOW perf: p95_ms=%.4f plans=%d mode=%d\n", p95, drive.FollowPlans(), (int)drive.Mode());

#ifndef _DEBUG
	CHECK(p95 <= 2.5);
#else
	(void)p95;
#endif
}

// ---------------------------------------------------------------------------
// F5-64: prep/assess split, deferral, phase spread and the Goto queue (NavDrive).
// ---------------------------------------------------------------------------

namespace
{
	bool QueueRoutesEqual(const std::vector<BotCore::NavRoutePoint> & a,
		const std::vector<BotCore::NavRoutePoint> & b)
	{
		if (a.size() != b.size())
			return false;
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (a[i].x != b[i].x || a[i].z != b[i].z)
				return false;
		}
		return true;
	}

	bool QueueEventsEqual(const BotCore::NavDriveEvents & a, const BotCore::NavDriveEvents & b)
	{
		return a.planned == b.planned
			&& a.planStatus == b.planStatus
			&& a.planReason == b.planReason
			&& a.planExpanded == b.planExpanded
			&& a.routeAdopted == b.routeAdopted
			&& a.verdict == b.verdict
			&& a.recovery.action == b.recovery.action
			&& a.recovery.stage == b.recovery.stage
			&& a.holdStop == b.holdStop
			&& a.awaitingLong == b.awaitingLong
			&& a.ended == b.ended;
	}

	bool QueueStepsEqual(const BotCore::NavDriveStep & a, const BotCore::NavDriveStep & b)
	{
		return a.kind == b.kind && a.x == b.x && a.z == b.z;
	}

	BotCore::NavDriveEvents QueueMergeSplit(const BotCore::NavDriveEvents & plan,
		const BotCore::NavDriveEvents & assess)
	{
		BotCore::NavDriveEvents e = plan;
		e.verdict = assess.verdict;
		e.recovery = assess.recovery;
		e.holdStop = assess.holdStop;
		e.awaitingLong = assess.awaitingLong;
		e.deferHold = assess.deferHold;
		if (e.ended == BotCore::NavFollowEnd::None)
			e.ended = assess.ended;
		return e;
	}

	// Advances (bx, bz) along a step, clipped at the first hidden cell (mirrors FollowSim).
	void QueueApplyStep(const NavGrid & grid, const std::set<std::pair<int, int>> & hidden,
		float & bx, float & bz, const BotCore::NavDriveStep & step)
	{
		const double ddx = (double)step.x - (double)bx;
		const double ddz = (double)step.z - (double)bz;
		const double len = std::sqrt(ddx * ddx + ddz * ddz);
		const int samples = (int)std::ceil(len / 0.25);
		int best = 0;
		for (int s = 1; s <= samples; ++s)
		{
			const double u = (double)s / (double)samples;
			const int cx = grid.CellOf((float)((double)bx + ddx * u));
			const int cz = grid.CellOf((float)((double)bz + ddz * u));
			if (hidden.count(std::make_pair(cx, cz)))
				break;
			best = s;
		}
		const double u = samples > 0 ? (double)best / (double)samples : 0.0;
		bx = (float)((double)bx + ddx * u);
		bz = (float)((double)bz + ddz * u);
	}

	// Runs drive A (TickFollow) and drive B (PlanFollow + AssessFollow) on identical input and
	// checks the F5-64 D4 equivalence every tick.
	void QueueSplitEquiv(const NavGrid & grid, int targetKind, uint32_t seed, int64_t duration,
		const std::set<std::pair<int, int>> & hidden, float botX, float botZ,
		float targetX, float targetZ)
	{
		BotCore::NavPathfinder finder;
		BotCore::NavDrive a;
		BotCore::NavDrive b;
		BotCore::NavFollowDriveParams params;
		BotCore::NavNoReach noreach;
		BotCore::NavCostLayer scratchA;
		BotCore::NavCostLayer scratchB;

		SimTarget target;
		target.Init(targetKind, seed, targetX, targetZ, 4.5f);

		a.BeginFollow(0);
		b.BeginFollow(0);

		float ax = botX, az = botZ;
		float bx = botX, bz = botZ;
		int64_t lastMove = -1000000;
		int mismatches = 0;

		for (int64_t t = 0; t <= duration; t += 100)
		{
			target.Advance(100);
			const int16_t spd = target.moving ? (int16_t)(target.speed * 10.0f) : 0;
			a.ObserveTarget(t, target.x, target.z, spd, t);
			b.ObserveTarget(t, target.x, target.z, spd, t);

			const BotCore::NavDriveEvents evA = a.TickFollow(grid, finder, t, ax, az, 4.5f, params, &scratchA);
			BotCore::NavDriveEvents planB;
			if (b.FollowPlanDue(t, params))
				planB = b.PlanFollow(grid, finder, t, bx, bz, 4.5f, params, &scratchB, noreach);
			const BotCore::NavDriveEvents assessB = b.AssessFollow(grid, t, bx, bz, params);
			const BotCore::NavDriveEvents evB = QueueMergeSplit(planB, assessB);

			if (!QueueEventsEqual(evA, evB))
			{
				++mismatches;
				if (mismatches == 1)
				{
					std::printf("NAVQUEUE split mismatch t=%lld A(plan=%d st=%d rn=%d rec=%d stage=%d end=%d) "
						"B(plan=%d st=%d rn=%d rec=%d stage=%d end=%d)\n", (long long)t,
						(int)evA.planned, (int)evA.planStatus, (int)evA.planReason,
						(int)evA.recovery.action, evA.recovery.stage, (int)evA.ended,
						(int)evB.planned, (int)evB.planStatus, (int)evB.planReason,
						(int)evB.recovery.action, evB.recovery.stage, (int)evB.ended);
				}
			}
			CHECK_EQ(a.FollowPlans(), b.FollowPlans());
			CHECK_EQ(a.RecoveryStage(), b.RecoveryStage());
			CHECK_EQ(a.StuckEpisodes(), b.StuckEpisodes());

			if (t - lastMove >= (int64_t)BotCore::kMovePeriodMs)
			{
				const BotCore::NavDriveStep stepA = a.NextFollowStep(grid, t, ax, az, 6.75f, params);
				const BotCore::NavDriveStep stepB = b.NextFollowStep(grid, t, bx, bz, 6.75f, params);
				CHECK(QueueStepsEqual(stepA, stepB));
				if (stepA.kind == BotCore::NavDriveStep::Step || stepA.kind == BotCore::NavDriveStep::Arrived)
				{
					QueueApplyStep(grid, hidden, ax, az, stepA);
					QueueApplyStep(grid, hidden, bx, bz, stepB);
					a.OnPacketSent(t, stepA);
					b.OnPacketSent(t, stepB);
				}
				lastMove = t;
			}

			if (evA.ended != BotCore::NavFollowEnd::None)
				break;
		}

		CHECK(std::fabs(ax - bx) < 1e-6f);
		CHECK(std::fabs(az - bz) < 1e-6f);
		CHECK_EQ(mismatches, 0);
	}
}

TEST_CASE("NavDriveQueue_Follow_SplitEquivalence")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));

	// Models 0 (Line), 1 (Zigzag), 3 (RandomWalk); three seeds each, 60 s at 100 ms ticks.
	for (int model = 0; model < 3; ++model)
	{
		const int kinds[3] = { 0, 1, 3 };
		for (uint32_t s = 0; s < 3; ++s)
		{
			const uint32_t seed = 20261064u + (uint32_t)(model * 17) + s * 131u;
			QueueSplitEquiv(grid, kinds[model], seed, 60000, std::set<std::pair<int, int>>(),
				70.0f, 200.0f, 80.0f, 200.0f);
		}
	}

	// Hidden single-cell obstacle: the recovery ladder runs in both drives.
	{
		std::set<std::pair<int, int>> hidden;
		hidden.insert(std::make_pair(45, 50));
		QueueSplitEquiv(grid, 4, 4242u, 60000, hidden, 60.0f, 200.0f, 300.0f, 200.0f);
	}
}

TEST_CASE("NavDriveQueue_Follow_PlanDue")
{
	const int n = 64;
	NavGrid open = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	std::vector<int16_t> none((size_t)n * n, 0);
	NavGrid allBlocked = MakeNav(n, 4.0f, none, HeightZeros(n));

	BotCore::NavPathfinder finder;
	BotCore::NavFollowDriveParams fp;
	BotCore::NavNoReach noreach;
	BotCore::NavCostLayer scratch;
	BotCore::NavDrive drive;

	CHECK(!drive.FollowPlanDue(0, fp));   // Off

	drive.BeginFollow(0);
	CHECK(!drive.FollowPlanDue(0, fp));   // no observation
	drive.ObserveTarget(0, 200.0f, 200.0f, -1, 0);
	CHECK(drive.FollowPlanDue(0, fp));    // First

	const BotCore::NavDriveEvents ev = drive.PlanFollow(open, finder, 0, 40.0f, 200.0f, 4.5f, fp, &scratch, noreach);
	CHECK(ev.planned);
	CHECK(!drive.FollowPlanDue(0, fp));   // just planned

	CHECK(!drive.FollowPlanDue(499, fp));
	CHECK(drive.FollowPlanDue(500, fp));  // Interval

	drive.ObserveTarget(500, 220.0f, 200.0f, 45, 500);
	CHECK(drive.FollowPlanDue(500, fp));  // Moved (>= 6 m)

	CHECK(!drive.FollowPlanDue(500 + fp.lostGraceMs + 1, fp));   // target not seen for too long

	drive.BeginGoto(open, finder, 40.0f, 200.0f, 200.0f, 200.0f, BotCore::NavDriveParams());
	CHECK(!drive.FollowPlanDue(600, fp));
	drive.Reset();
	CHECK(!drive.FollowPlanDue(600, fp));

	// blockedAbandon suppresses requests while the drive is still Follow.
	{
		BotCore::NavDrive d;
		BotCore::NavCostLayer sc;
		d.BeginFollow(0);
		d.ObserveTarget(0, 200.0f, 20.0f, -1, 0);
		d.PlanFollow(open, finder, 0, 20.0f, 20.0f, 4.5f, fp, &sc, noreach);
		d.AssessFollow(open, 0, 20.0f, 20.0f, fp);
		int64_t t = 1500;
		for (int i = 0; i < 10; ++i)
		{
			d.ObserveTarget(0, 200.0f, 20.0f, -1, t);
			d.AssessFollow(open, t, 20.0f, 20.0f, fp);
			d.NextFollowStep(allBlocked, t, 20.0f, 20.0f, 6.75f, fp);
			t += 1500;
		}
		CHECK(d.Mode() == BotCore::NavDriveMode::Follow);
		CHECK(!d.FollowPlanDue(t + 1000, fp));
	}
}

TEST_CASE("NavDriveQueue_Follow_PhaseSpread")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavNoReach noreach;

	const int N = 16;
	BotCore::NavDrive drives[N];
	BotCore::NavFollowDriveParams params[N];
	int firstInterval[N];
	int secondInterval[N];
	for (int i = 0; i < N; ++i)
	{
		params[i].follow.phaseMs = BotCore::NavReplanPhaseMs(i);
		firstInterval[i] = -1;
		secondInterval[i] = -1;
		drives[i].BeginFollow(0);
		drives[i].ObserveTarget(0, 200.0f, 200.0f, 0, 0);
		const BotCore::NavDriveEvents ev = drives[i].PlanFollow(grid, finder, 0, 40.0f, 200.0f, 4.5f, params[i], nullptr, noreach);
		CHECK(ev.planReason == BotCore::NavReplanReason::First);
	}

	for (int64_t t = 100; t <= 1400; t += 100)
	{
		for (int i = 0; i < N; ++i)
		{
			drives[i].ObserveTarget(0, 200.0f, 200.0f, 0, t);
			if (!drives[i].FollowPlanDue(t, params[i]))
				continue;
			const BotCore::NavDriveEvents ev = drives[i].PlanFollow(grid, finder, t, 40.0f, 200.0f, 4.5f, params[i], nullptr, noreach);
			if (ev.planReason == BotCore::NavReplanReason::Interval)
			{
				if (firstInterval[i] < 0)
					firstInterval[i] = (int)t;
				else if (secondInterval[i] < 0)
					secondInterval[i] = (int)t;
			}
		}
	}

	int groups[5] = { 0, 0, 0, 0, 0 };
	for (int i = 0; i < N; ++i)
	{
		REQUIRE(firstInterval[i] >= 500);
		CHECK(firstInterval[i] <= 900);
		groups[(firstInterval[i] - 500) / 100]++;
		// The second Interval is the first plus 500 ms exactly (no phase).
		REQUIRE(secondInterval[i] == firstInterval[i] + 500);
	}
	CHECK_EQ(groups[0], 4);
	CHECK_EQ(groups[1], 3);
	CHECK_EQ(groups[2], 3);
	CHECK_EQ(groups[3], 3);
	CHECK_EQ(groups[4], 3);
	std::printf("NAVQUEUE phase: 500/600/700/800/900 = %d/%d/%d/%d/%d\n",
		groups[0], groups[1], groups[2], groups[3], groups[4]);

	// A >= 6 m jump makes that bot plan Moved without waiting for its phase.
	{
		BotCore::NavFollowDriveParams p0;
		p0.follow.phaseMs = BotCore::NavReplanPhaseMs(0);   // 0
		BotCore::NavDrive d;
		d.BeginFollow(0);
		d.ObserveTarget(0, 200.0f, 200.0f, 0, 0);
		d.PlanFollow(grid, finder, 0, 40.0f, 200.0f, 4.5f, p0, nullptr, noreach);
		d.ObserveTarget(100, 210.0f, 200.0f, 45, 100);
		CHECK(d.FollowPlanDue(100, p0));
		const BotCore::NavDriveEvents ev = d.PlanFollow(grid, finder, 100, 40.0f, 200.0f, 4.5f, p0, nullptr, noreach);
		CHECK(ev.planReason == BotCore::NavReplanReason::Moved);
	}
}

TEST_CASE("NavDriveQueue_Follow_Deferred_FreshRoute")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavFollowDriveParams fp;
	BotCore::NavNoReach noreach;
	BotCore::NavCostLayer scratch;

	BotCore::NavDrive drive;
	drive.BeginFollow(0);
	drive.ObserveTarget(0, 200.0f, 200.0f, -1, 0);
	const BotCore::NavDriveEvents plan = drive.PlanFollow(grid, finder, 0, 40.0f, 200.0f, 4.5f, fp, &scratch, noreach);
	REQUIRE(plan.routeAdopted);
	REQUIRE(drive.Route().size() >= 2);

	float bx = 40.0f;
	float bz = 200.0f;
	int steps = 0;
	int64_t lastMove = -1000000;
	for (int64_t t = 0; t <= 4900; t += 100)
	{
		drive.ObserveTarget(0, 200.0f, 200.0f, -1, t);   // never served a plan again
		const BotCore::NavDriveEvents ev = drive.AssessFollow(grid, t, bx, bz, fp);
		CHECK(!ev.deferHold);
		CHECK(!drive.DeferHeld());
		CHECK(!drive.RouteStale(t, fp.defer));

		if (t - lastMove >= (int64_t)BotCore::kMovePeriodMs)
		{
			const BotCore::NavDriveStep s = drive.NextFollowStep(grid, t, bx, bz, 6.75f, fp);
			if (s.kind == BotCore::NavDriveStep::Step || s.kind == BotCore::NavDriveStep::Arrived)
			{
				CHECK(ChordSampleClean(grid, bx, bz, s.x, s.z));
				bx = s.x;
				bz = s.z;
				drive.OnPacketSent(t, s);
				++steps;
			}
			lastMove = t;
		}
	}
	CHECK(steps >= 1);
	std::printf("NAVQUEUE freshroute: steps=%d\n", steps);
}

TEST_CASE("NavDriveQueue_Follow_Deferred_StaleHold")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavFollowDriveParams fp;
	BotCore::NavNoReach noreach;
	BotCore::NavCostLayer scratch;

	// (a) Age: a plan older than planMaxAgeMs (5000) holds once.
	{
		BotCore::NavDrive d;
		BotCore::NavCostLayer sc;
		d.BeginFollow(0);
		d.ObserveTarget(0, 200.0f, 200.0f, -1, 0);
		d.PlanFollow(grid, finder, 0, 40.0f, 200.0f, 4.5f, fp, &sc, noreach);
		REQUIRE(d.Route().size() >= 2);

		float x = 40.0f;
		float z = 200.0f;
		int64_t lastMove = -1000000;
		int holdEvents = 0;
		int staleSteps = 0;
		for (int64_t t = 0; t <= 6000; t += 100)
		{
			d.ObserveTarget(0, 200.0f, 200.0f, -1, t);
			const BotCore::NavDriveEvents ev = d.AssessFollow(grid, t, x, z, fp);
			if (ev.deferHold)
				++holdEvents;
			if (d.DeferHeld())
			{
				const BotCore::NavDriveStep s = d.NextFollowStep(grid, t, x, z, 6.75f, fp);
				CHECK(s.kind == BotCore::NavDriveStep::None);
			}
			else if (t - lastMove >= (int64_t)BotCore::kMovePeriodMs)
			{
				const BotCore::NavDriveStep s = d.NextFollowStep(grid, t, x, z, 6.75f, fp);
				if (s.kind == BotCore::NavDriveStep::Step || s.kind == BotCore::NavDriveStep::Arrived)
				{
					if (d.RouteStale(t, fp.defer))
						++staleSteps;
					x = s.x;
					z = s.z;
					d.OnPacketSent(t, s);
				}
				lastMove = t;
			}
		}
		CHECK_EQ(holdEvents, 1);
		CHECK(d.DeferHeld());
		CHECK(d.RouteStale(6000, fp.defer));
		// (d) no step is produced while the stale route is held.
		CHECK_EQ(staleSteps, 0);

		// (c) A served plan clears the hold and steps resume on the fresh route with growing
		// route progress (no straight-line step into the target).
		REQUIRE(d.FollowPlanDue(6000, fp));
		const BotCore::NavDriveEvents plan = d.PlanFollow(grid, finder, 6000, x, z, 4.5f, fp, &sc, noreach);
		REQUIRE(plan.planned);
		CHECK(plan.routeAdopted);
		CHECK(!d.DeferHeld());
		float p0 = -1.0f;
		{
			const BotCore::NavDriveStep s = d.NextFollowStep(grid, 6000, x, z, 6.75f, fp);
			REQUIRE(s.kind == BotCore::NavDriveStep::Step || s.kind == BotCore::NavDriveStep::Arrived);
			CHECK(ChordSampleClean(grid, x, z, s.x, s.z));
			p0 = s.routeProgressM;
			x = s.x;
			z = s.z;
			d.OnPacketSent(6000, s);
		}
		{
			const int64_t t2 = 6000 + (int64_t)BotCore::kMovePeriodMs;
			d.ObserveTarget(0, 200.0f, 200.0f, -1, t2);
			d.AssessFollow(grid, t2, x, z, fp);
			const BotCore::NavDriveStep s = d.NextFollowStep(grid, t2, x, z, 6.75f, fp);
			REQUIRE(s.kind == BotCore::NavDriveStep::Step || s.kind == BotCore::NavDriveStep::Arrived);
			CHECK(ChordSampleClean(grid, x, z, s.x, s.z));
			CHECK(s.routeProgressM >= p0 - 1e-6f);
			CHECK(s.routeProgressM > 0.0f);
		}
	}

	// (b) Drift: the target jumps 16 m with a fresh route age; same hold.
	{
		BotCore::NavDrive d;
		BotCore::NavCostLayer sc;
		d.BeginFollow(0);
		d.ObserveTarget(0, 200.0f, 200.0f, -1, 0);
		d.PlanFollow(grid, finder, 0, 40.0f, 200.0f, 4.5f, fp, &sc, noreach);
		REQUIRE(d.Route().size() >= 2);
		d.ObserveTarget(100, 216.0f, 200.0f, -1, 100);
		const BotCore::NavDriveEvents ev = d.AssessFollow(grid, 100, 40.0f, 200.0f, fp);
		CHECK(ev.deferHold);
		CHECK(d.DeferHeld());
		CHECK(d.RouteStale(100, fp.defer));
	}
}

TEST_CASE("NavDriveQueue_Follow_Deferred_FirstPlan")
{
	const int n = 100;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavFollowDriveParams fp;

	BotCore::NavDrive d;
	d.BeginFollow(0);
	for (int64_t t = 0; t <= 3000; t += 100)
	{
		d.ObserveTarget(0, 200.0f, 200.0f, -1, t);
		const BotCore::NavDriveEvents ev = d.AssessFollow(grid, t, 40.0f, 200.0f, fp);
		CHECK(!ev.deferHold);
		CHECK(!d.DeferHeld());
		const BotCore::NavDriveStep s = d.NextFollowStep(grid, t, 40.0f, 200.0f, 6.75f, fp);
		CHECK(s.kind == BotCore::NavDriveStep::None);
	}
}

TEST_CASE("NavDriveQueue_Follow_PlanFailed_Split")
{
	NavGrid grid = MakePocketGrid40();
	BotCore::NavPathfinder finder;
	BotCore::NavFollowDriveParams fp;
	BotCore::NavNoReach noreach;
	BotCore::NavCostLayer scratch;

	const float px = grid.CellCenter(20);
	const float pz = grid.CellCenter(20);
	const float bx = grid.CellCenter(5);
	const float bz = grid.CellCenter(20);

	BotCore::NavDrive split;
	split.BeginFollow(0);
	int64_t t = 0;
	int plans = 0;
	BotCore::NavFollowEnd ended = BotCore::NavFollowEnd::None;
	int64_t endedAt = -1;
	for (int i = 0; i < 30; ++i)
	{
		split.ObserveTarget(0, px, pz, -1, t);
		BotCore::NavDriveEvents plan;
		if (split.FollowPlanDue(t, fp))
			plan = split.PlanFollow(grid, finder, t, bx, bz, 4.5f, fp, &scratch, noreach);
		const BotCore::NavDriveEvents assess = split.AssessFollow(grid, t, bx, bz, fp);
		const BotCore::NavDriveEvents ev = QueueMergeSplit(plan, assess);
		if (ev.planned)
			++plans;
		if (ev.ended != BotCore::NavFollowEnd::None)
		{
			ended = ev.ended;
			endedAt = t;
			break;
		}
		t += 500;
	}
	CHECK(ended == BotCore::NavFollowEnd::PlanFailed);
	CHECK(plans >= 9);
	CHECK(plans <= 11);
	CHECK(endedAt <= 6000);

	// After the PlanFailed Reset the assessment is empty.
	const BotCore::NavDriveEvents after = split.AssessFollow(grid, endedAt + 100, bx, bz, fp);
	CHECK(after.ended == BotCore::NavFollowEnd::None);
	CHECK(!after.planned);
	std::printf("NAVQUEUE planfail: plans=%d at=%lld\n", plans, (long long)endedAt);
}

TEST_CASE("NavDriveQueue_Goto_Arm")
{
	const int n = 64;
	NavGrid open = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	NavGrid wall = MakeNav(n, 4.0f, WallEvents(n), HeightZeros(n));

	BotCore::NavDrive drive;
	CHECK(drive.ArmGoto(open, 20.0f, 20.0f, 200.0f, 200.0f) == BotCore::NavPlanStatus::None);
	CHECK(drive.Active());
	CHECK(drive.Mode() == BotCore::NavDriveMode::Goto);
	CHECK(drive.PlanPending());
	CHECK(drive.Route().empty());
	CHECK(drive.NextStep(open, 20.0f, 20.0f, 6.75f).kind == BotCore::NavDriveStep::None);

	// Non-Walk start / goal.
	CHECK(drive.ArmGoto(wall, wall.CellCenter(31), wall.CellCenter(20), 200.0f, 20.0f) == BotCore::NavPlanStatus::InvalidStart);
	CHECK(!drive.Active());
	CHECK(drive.ArmGoto(wall, 20.0f, 20.0f, wall.CellCenter(31), wall.CellCenter(20)) == BotCore::NavPlanStatus::InvalidGoal);
	CHECK(!drive.Active());

	// Non-finite, negative and over-limit coordinates.
	CHECK(drive.ArmGoto(open, -1.0f, 20.0f, 200.0f, 20.0f) == BotCore::NavPlanStatus::InvalidStart);
	CHECK(drive.ArmGoto(open, std::nanf(""), 20.0f, 200.0f, 20.0f) == BotCore::NavPlanStatus::InvalidStart);
	CHECK(drive.ArmGoto(open, 6554.0f, 20.0f, 200.0f, 20.0f) == BotCore::NavPlanStatus::InvalidStart);
	CHECK(drive.ArmGoto(open, 20.0f, 20.0f, -1.0f, 20.0f) == BotCore::NavPlanStatus::InvalidGoal);
	CHECK(drive.ArmGoto(open, 20.0f, 20.0f, 1000.0f, 1000.0f) == BotCore::NavPlanStatus::InvalidGoal);
	CHECK(!drive.PlanPending());
}

TEST_CASE("NavDriveQueue_Goto_RunPlan_Equivalence")
{
	NavGrid grid;
	std::vector<NavCell> walk;
	if (LoadZone71OrSkip(grid, "queue-gotoeq"))
	{
		CollectWalk(grid, walk);
	}
	else
	{
		grid = MakeNav(64, 4.0f, WallEvents(64), HeightZeros(64));
		CollectWalk(grid, walk);
	}
	REQUIRE(!walk.empty());

	BotCore::Rng rng(20261064u);
	BotCore::NavPathfinder finder;
	BotCore::NavDriveParams params;

	int planned = 0;
	for (int i = 0; i < 200; ++i)
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

		BotCore::NavDrive d1;
		const BotCore::NavPlanStatus s1 = d1.BeginGoto(grid, finder, ax, az, bx, bz, params);

		BotCore::NavDrive d2;
		const BotCore::NavPlanStatus arm = d2.ArmGoto(grid, ax, az, bx, bz);
		BotCore::NavPlanStatus s2 = arm;
		if (arm == BotCore::NavPlanStatus::None)
			s2 = d2.RunGotoPlan(grid, finder, nullptr, 0, ax, az, params);
		CHECK(s1 == s2);

		if (s1 == BotCore::NavPlanStatus::Planned)
		{
			++planned;
			CHECK(QueueRoutesEqual(d1.Route(), d2.Route()));
			CHECK(d1.RouteLengthM() == d2.RouteLengthM());
			CHECK(d1.GoalX() == d2.GoalX());
			CHECK(d1.GoalZ() == d2.GoalZ());
			CHECK_EQ(d1.PlanExpanded(), d2.PlanExpanded());
			CHECK_EQ(d1.PlanWaypoints(), d2.PlanWaypoints());
			CHECK(!d2.PlanPending());
			CHECK_EQ(d2.Replans(), 0);
			CHECK(d2.RunGotoPlan(grid, finder, nullptr, 0, ax, az, params) == BotCore::NavPlanStatus::None);
		}
		else
		{
			CHECK(!d2.Active());
		}
	}
	CHECK(planned > 0);
	std::printf("NAVQUEUE gotoeq: planned=%d\n", planned);
}

TEST_CASE("NavDriveQueue_Goto_RequestReplan")
{
	const int n = 64;
	NavGrid open = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	NavGrid wall = MakeNav(n, 4.0f, WallEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDriveParams params;
	const float botx = 20.0f;
	const float botz = 20.0f;
	const float gx = 200.0f;
	const float gz = 20.0f;

	BotCore::NavDrive drive;
	REQUIRE(drive.BeginGoto(open, finder, botx, botz, gx, gz, params) == BotCore::NavPlanStatus::Planned);
	CHECK(drive.NextStep(wall, botx, botz, 400.0f).kind == BotCore::NavDriveStep::Blocked);

	CHECK(drive.RequestReplan() == BotCore::NavPlanStatus::None);
	CHECK(drive.PlanPending());
	// The route is kept while the replan is pending.
	CHECK(drive.Route().size() >= 2);
	CHECK(drive.NextStep(wall, botx, botz, 400.0f).kind == BotCore::NavDriveStep::Blocked);
	CHECK(drive.RequestReplan() == BotCore::NavPlanStatus::None);   // idempotent
	REQUIRE(drive.RunGotoPlan(wall, finder, nullptr, 0, botx, botz, params) == BotCore::NavPlanStatus::Planned);
	CHECK_EQ(drive.Replans(), 1);
	CHECK(!drive.PlanPending());

	// The queued replan equals the synchronous Replan route.
	BotCore::NavDrive sync;
	REQUIRE(sync.BeginGoto(open, finder, botx, botz, gx, gz, params) == BotCore::NavPlanStatus::Planned);
	CHECK(sync.NextStep(wall, botx, botz, 400.0f).kind == BotCore::NavDriveStep::Blocked);
	REQUIRE(sync.Replan(wall, finder, botx, botz, params) == BotCore::NavPlanStatus::Planned);
	CHECK_EQ(sync.Replans(), 1);
	CHECK(QueueRoutesEqual(drive.Route(), sync.Route()));

	// Budget spent: ReplanLimit and Off.
	CHECK(drive.RequestReplan() == BotCore::NavPlanStatus::ReplanLimit);
	CHECK(!drive.Active());

	BotCore::NavDrive off;
	CHECK(off.RequestReplan() == BotCore::NavPlanStatus::None);
}

TEST_CASE("NavDriveQueue_Goto_Cache")
{
	const int n = 64;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDriveParams params;
	BotCore::NavPathCache cache;
	const float bx = 20.0f;
	const float bz = 20.0f;
	const float gx = 200.0f;
	const float gz = 200.0f;

	BotCore::NavDrive d;
	REQUIRE(d.ArmGoto(grid, bx, bz, gx, gz) == BotCore::NavPlanStatus::None);
	REQUIRE(d.RunGotoPlan(grid, finder, &cache, 0, bx, bz, params) == BotCore::NavPlanStatus::Planned);
	CHECK(!d.LastPlanCacheHit());
	CHECK(d.PlanExpanded() > 0);
	const std::vector<BotCore::NavRoutePoint> route1 = d.Route();
	CHECK(cache.Count() >= 1);

	// Same start cell, same goal cell -> hit, no A*, identical route.
	REQUIRE(d.ArmGoto(grid, bx, bz, gx, gz) == BotCore::NavPlanStatus::None);
	REQUIRE(d.RunGotoPlan(grid, finder, &cache, 100, bx, bz, params) == BotCore::NavPlanStatus::Planned);
	CHECK(d.LastPlanCacheHit());
	CHECK_EQ(d.PlanExpanded(), 0);
	CHECK(QueueRoutesEqual(d.Route(), route1));

	// TTL (30 000 ms) expiry -> miss.
	REQUIRE(d.ArmGoto(grid, bx, bz, gx, gz) == BotCore::NavPlanStatus::None);
	REQUIRE(d.RunGotoPlan(grid, finder, &cache, 100 + 30001, bx, bz, params) == BotCore::NavPlanStatus::Planned);
	CHECK(!d.LastPlanCacheHit());

	// A different point inside the same start cell still hits; the route starts at the new point.
	{
		const float bx2 = bx + 1.0f;
		BotCore::NavDrive d2;
		REQUIRE(d2.ArmGoto(grid, bx2, bz, gx, gz) == BotCore::NavPlanStatus::None);
		REQUIRE(d2.RunGotoPlan(grid, finder, &cache, 100, bx2, bz, params) == BotCore::NavPlanStatus::Planned);
		CHECK(d2.LastPlanCacheHit());
		REQUIRE(!d2.Route().empty());
		CHECK(d2.Route().front().x == bx2);
	}

	// cache == nullptr -> never a hit.
	{
		BotCore::NavDrive d2;
		REQUIRE(d2.ArmGoto(grid, bx, bz, gx, gz) == BotCore::NavPlanStatus::None);
		REQUIRE(d2.RunGotoPlan(grid, finder, nullptr, 0, bx, bz, params) == BotCore::NavPlanStatus::Planned);
		CHECK(!d2.LastPlanCacheHit());
	}

	// A route longer than kMaxCells (512): a 560 x 560 open grid forces a ~556-cell diagonal.
	{
		const int bn = 560;
		NavGrid big = MakeNav(bn, 4.0f, OpenEvents(bn), HeightZeros(bn));
		REQUIRE(big.Walk(2, 2));
		REQUIRE(big.Walk(557, 557));
		BotCore::NavPathfinder bf;
		BotCore::NavPathCache bigCache;
		BotCore::NavDrive bd;
		const float ax = big.CellCenter(2);
		const float az = big.CellCenter(2);
		const float bx2 = big.CellCenter(557);
		const float bz2 = big.CellCenter(557);
		REQUIRE(bd.ArmGoto(big, ax, az, bx2, bz2) == BotCore::NavPlanStatus::None);
		REQUIRE(bd.RunGotoPlan(big, bf, &bigCache, 0, ax, az, params) == BotCore::NavPlanStatus::Planned);
		CHECK_EQ(bigCache.Count(), 0);   // Put rejects the > kMaxCells route
		REQUIRE(bd.ArmGoto(big, ax, az, bx2, bz2) == BotCore::NavPlanStatus::None);
		REQUIRE(bd.RunGotoPlan(big, bf, &bigCache, 100, ax, az, params) == BotCore::NavPlanStatus::Planned);
		CHECK(!bd.LastPlanCacheHit());
		CHECK_EQ(bigCache.Count(), 0);
		CHECK(bd.PlanExpanded() > 0);
	}
}

TEST_CASE("NavDriveQueue_Goto_Cancel")
{
	const int n = 64;
	NavGrid grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
	BotCore::NavPathfinder finder;
	BotCore::NavDriveParams params;

	BotCore::NavDrive drive;
	REQUIRE(drive.ArmGoto(grid, 20.0f, 20.0f, 200.0f, 200.0f) == BotCore::NavPlanStatus::None);
	CHECK(drive.PlanPending());
	drive.Reset();
	CHECK(!drive.PlanPending());
	CHECK(!drive.Active());
	CHECK(drive.RunGotoPlan(grid, finder, nullptr, 0, 20.0f, 20.0f, params) == BotCore::NavPlanStatus::None);

	// Scheduler half: a cancelled query is not selected again.
	BotCore::NavQueryScheduler sched;
	sched.Request(3, 0);
	CHECK_EQ(sched.Pending(), 1);
	sched.Cancel(3);
	CHECK_EQ(sched.Pending(), 0);
	uint16_t out[8];
	CHECK_EQ(sched.NextBatch(100, 1.5, out, 8), 0);
}

TEST_CASE("NavDriveQueue_Scheduler_FirstPlan16")
{
	NavGrid grid;
	std::vector<NavCell> walk;
	bool real = LoadZone71OrSkip(grid, "queue-firstplan");
	float tx = 0.0f;
	float tz = 0.0f;
	if (real)
	{
		CollectWalk(grid, walk);
		const NavCell arena = NearestWalk(grid, 1274.0f, 890.0f);
		tx = grid.CellCenter(arena.x);
		tz = grid.CellCenter(arena.z);
	}
	else
	{
		const int n = 256;
		grid = MakeNav(n, 4.0f, OpenEvents(n), HeightZeros(n));
		CollectWalk(grid, walk);
		const NavCell arena = NearestWalk(grid, 800.0f, 800.0f);
		tx = grid.CellCenter(arena.x);
		tz = grid.CellCenter(arena.z);
	}
	REQUIRE(!walk.empty());

	BotCore::NavReach reach;
	reach.Build(grid);
	const int mainId = reach.LargestComponent();
	std::vector<NavCell> mainWalk;
	for (size_t i = 0; i < walk.size(); ++i)
		if (reach.ComponentOf(walk[i].x, walk[i].z) == mainId)
			mainWalk.push_back(walk[i]);
	REQUIRE(!mainWalk.empty());

	const int N = 16;
	BotCore::NavDrive drives[N];
	float sx[N];
	float sz[N];
	BotCore::NavPathfinder finder;
	BotCore::NavDriveParams params;
	BotCore::NavQueryScheduler sched;
	BotCore::Rng rng(20261066u);
	for (int i = 0; i < N; ++i)
	{
		const NavCell c = mainWalk[(size_t)rng.NextBelow((uint32_t)mainWalk.size())];
		sx[i] = grid.CellCenter(c.x);
		sz[i] = grid.CellCenter(c.z);
		REQUIRE(drives[i].ArmGoto(grid, sx[i], sz[i], tx, tz) == BotCore::NavPlanStatus::None);
		sched.Request((uint16_t)i, 0);
	}

	// While armed, no step is produced.
	CHECK(drives[0].NextStep(grid, sx[0], sz[0], 6.75f).kind == BotCore::NavDriveStep::None);

	int served = 0;
	int64_t t = 0;
	int64_t waitMax = 0;
	int ticks = 0;
	uint16_t batch[64];
	while (served < N && t <= 5000)
	{
		const int nb = sched.NextBatch(t, 1.5, batch, 64);
		for (int k = 0; k < nb; ++k)
		{
			const uint16_t id = batch[k];
			const BotCore::NavPlanStatus st = drives[id].RunGotoPlan(grid, finder, nullptr, t, sx[id], sz[id], params);
			if (st == BotCore::NavPlanStatus::Planned)
			{
				++served;
				if (t > waitMax)
					waitMax = t;
			}
			sched.ReportCost(id, 0.3, 0);
			sched.Cancel(id);
		}
		++ticks;
		t += 100;
	}

	std::printf("NAVQUEUE first_plan wait_max_ms=%lld served=%d ticks=%d\n",
		(long long)waitMax, served, ticks);
	CHECK_EQ(served, N);
	CHECK(waitMax <= 1100);
	for (int i = 0; i < N; ++i)
		CHECK(!drives[i].PlanPending());
}

TEST_CASE("NavDriveQueue_Follow_Load16")
{
	NavGrid grid;
	if (!LoadZone71OrSkip(grid, "queue-load16"))
		return;

	std::vector<NavCell> walk;
	CollectWalk(grid, walk);
	REQUIRE(!walk.empty());
	BotCore::NavReach reach;
	reach.Build(grid);
	const int mainId = reach.LargestComponent();
	std::vector<NavCell> mainWalk;
	for (size_t i = 0; i < walk.size(); ++i)
		if (reach.ComponentOf(walk[i].x, walk[i].z) == mainId)
			mainWalk.push_back(walk[i]);
	REQUIRE(!mainWalk.empty());

	// Map the SimTarget local box (40..360, 40..320) onto the arena A region so the targets stay
	// in walkable world space; observations add the offset.
	const float ox = 1274.0f - 200.0f;
	const float oz = 890.0f - 200.0f;
	const NavCell arenaCell = NearestWalk(grid, 1274.0f, 890.0f);
	std::vector<NavCell> startCells;
	for (size_t i = 0; i < mainWalk.size(); ++i)
	{
		if (std::abs(mainWalk[i].x - arenaCell.x) <= 20 && std::abs(mainWalk[i].z - arenaCell.z) <= 20)
			startCells.push_back(mainWalk[i]);
	}
	REQUIRE(!startCells.empty());

	const int N = 16;
	BotCore::NavDrive drives[N];
	BotCore::NavFollowDriveParams params[N];
	BotCore::NavCostLayer scratch[N];
	SimTarget targets[N];
	float bx[N];
	float bz[N];
	int64_t lastPkt[N];
	int64_t reqAt[N];
	BotCore::NavPathfinder finder;
	BotCore::NavQueryScheduler sched;
	BotCore::Rng rng(20261067u);
	for (int i = 0; i < N; ++i)
	{
		const NavCell b = startCells[(size_t)rng.NextBelow((uint32_t)startCells.size())];
		bx[i] = grid.CellCenter(b.x);
		bz[i] = grid.CellCenter(b.z);
		targets[i].Init(i % 4, 20261067u + (uint32_t)i * 7919u, 200.0f, 200.0f, 4.5f);
		params[i].follow.phaseMs = BotCore::NavReplanPhaseMs(i);
		lastPkt[i] = -1000000;
		reqAt[i] = -1;
		drives[i].BeginFollow(0);
		drives[i].ObserveTarget(0, targets[i].x + ox, targets[i].z + oz, 45, 0);
	}

	int64_t waitMax = 0;
	int queries = 0;
	int deferred = 0;
	int holdTicks = 0;
	int staleSteps = 0;
	std::vector<double> tickMs;
	for (int64_t t = 0; t <= 120000; t += 100)
	{
		const std::chrono::steady_clock::time_point tick0 = std::chrono::steady_clock::now();

		for (int i = 0; i < N; ++i)
		{
			if (t > 0 && t % 1500 == 0)
			{
				const float lx = targets[i].x;
				const float lz = targets[i].z;
				targets[i].Advance(1500);
				const int cx = grid.CellOf(targets[i].x + ox);
				const int cz = grid.CellOf(targets[i].z + oz);
				if (!grid.Walk(cx, cz) || reach.ComponentOf(cx, cz) != mainId)
				{
					targets[i].x = lx;
					targets[i].z = lz;
				}
				const int16_t spd = targets[i].moving ? (int16_t)(targets[i].speed * 10.0f) : 0;
				drives[i].ObserveTarget(t, targets[i].x + ox, targets[i].z + oz, spd, t);
			}
		}

		for (int i = 0; i < N; ++i)
		{
			if (reqAt[i] < 0 && drives[i].FollowPlanDue(t, params[i]))
			{
				sched.Request((uint16_t)i, t);
				reqAt[i] = t;
			}
		}

		uint16_t batch[64];
		const int nb = sched.NextBatch(t, 1.5, batch, 64);
		for (int k = 0; k < nb; ++k)
		{
			const uint16_t id = batch[k];
			// The wall-clock tick time (p95 guard below) is measured over the whole tick; the
			// scheduler is fed a fixed synthetic cost (mirrors test 13) so the queueing logic is
			// configuration-independent (Debug A* is ~20x slower than Release and would otherwise
			// starve the 1.5 ms budget).
			const BotCore::NavDriveEvents ev = drives[id].PlanFollow(grid, finder, t, bx[id], bz[id], 4.5f,
				params[id], &scratch[id], reach);
			sched.ReportCost(id, 0.3, ev.planExpanded);
			sched.Cancel(id);
			if (reqAt[id] >= 0)
			{
				const int64_t wait = t - reqAt[id];
				if (wait > waitMax)
					waitMax = wait;
				reqAt[id] = -1;
			}
			++queries;
		}

		bool anyHeld = false;
		for (int i = 0; i < N; ++i)
		{
			const BotCore::NavDriveEvents ev = drives[i].AssessFollow(grid, t, bx[i], bz[i], params[i]);
			if (ev.deferHold)
				++deferred;
			if (drives[i].DeferHeld())
				anyHeld = true;
			if (t - lastPkt[i] >= (int64_t)BotCore::kMovePeriodMs)
			{
				const BotCore::NavDriveStep s = drives[i].NextFollowStep(grid, t, bx[i], bz[i], 6.75f, params[i]);
				if (s.kind == BotCore::NavDriveStep::Step || s.kind == BotCore::NavDriveStep::Arrived)
				{
					if (drives[i].RouteStale(t, params[i].defer))
						++staleSteps;
					bx[i] = s.x;
					bz[i] = s.z;
					drives[i].OnPacketSent(t, s);
				}
				lastPkt[i] = t;
			}
		}
		if (anyHeld)
			++holdTicks;

		const std::chrono::steady_clock::time_point tick1 = std::chrono::steady_clock::now();
		tickMs.push_back(std::chrono::duration<double, std::milli>(tick1 - tick0).count());
	}

	int noPlanBots = 0;
	int endedBots = 0;
	for (int i = 0; i < N; ++i)
	{
		if (drives[i].FollowPlans() == 0)
			++noPlanBots;
		if (drives[i].Mode() != BotCore::NavDriveMode::Follow)
			++endedBots;
	}

	std::sort(tickMs.begin(), tickMs.end());
	const double tp95 = PercentileDouble(tickMs, 0.95);
	const double tp99 = PercentileDouble(tickMs, 0.99);
	const double tmax = tickMs.empty() ? 0.0 : tickMs.back();
	std::printf("NAVQUEUE load16 tick_p95_ms=%.3f p99=%.3f max=%.3f wait_max_ms=%lld queries=%d deferred=%d hold=%d stale=0 ended=%d\n",
		tp95, tp99, tmax, (long long)waitMax, queries, deferred, holdTicks, endedBots);
	std::printf("NAVQUEUE load16 stale_steps=%d no_plan_bots=%d\n", staleSteps, noPlanBots);

	CHECK_EQ(staleSteps, 0);
	CHECK(waitMax <= 1100);
	CHECK_EQ(noPlanBots, 0);
#ifndef _DEBUG
	CHECK(tp95 <= 4.0);
#else
	(void)tp95;
	(void)tp99;
	(void)tmax;
#endif
}
