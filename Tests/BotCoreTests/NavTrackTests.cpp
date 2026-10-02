#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavSmooth.h>
#include <BotCore/NavTrack.h>
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

	// RingEvents plus a blocked column at x (z = 1..n-2).
	std::vector<int16_t> WallColumn(int n, int x)
	{
		std::vector<int16_t> events = RingEvents(n);
		for (int z = 1; z <= n - 2; ++z)
			events[CellIndex(n, x, z)] = 0;
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

	bool SegmentsClear(const BotCore::NavGrid & grid, const std::vector<BotCore::NavCell> & waypoints)
	{
		for (size_t i = 1; i < waypoints.size(); ++i)
		{
			if (!BotCore::NavLineClear(grid, waypoints[i - 1], waypoints[i]))
				return false;
		}
		return true;
	}

	float Dist(float ax, float az, float bx, float bz)
	{
		const float dx = bx - ax;
		const float dz = bz - az;
		return std::sqrt(dx * dx + dz * dz);
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

	// Target observations at 0, 250, 500, 750, 1000 ms: x = x0 + k * dx, z constant.
	void Observe5(BotCore::NavFollower & f, float x0, float dx, float z)
	{
		for (int k = 0; k < 5; ++k)
			f.ObserveTarget((int64_t)k * 250, x0 + dx * (float)k, z);
	}
}

TEST_CASE("NavTrack_Tracker_Velocity")
{
	BotCore::NavTargetTracker t;
	CHECK_EQ(t.Count(), 0);
	int64_t lt = 0;
	float lx = 0.0f;
	float lz = 0.0f;
	CHECK(!t.Latest(lt, lx, lz));

	float vx = 1.0f;
	float vz = 1.0f;
	t.Velocity(0, 1000, 100, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);

	CHECK(t.Observe(0, 100.0f, 50.0f));
	CHECK_EQ(t.Count(), 1);
	t.Velocity(0, 1000, 100, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);

	CHECK(t.Observe(250, 102.0f, 50.0f));
	CHECK(t.Observe(500, 104.0f, 50.0f));
	CHECK(t.Observe(750, 106.0f, 50.0f));
	CHECK(t.Observe(1000, 108.0f, 50.0f));
	CHECK_EQ(t.Count(), 5);
	t.Velocity(1000, 1000, 100, vx, vz);
	CHECK(std::fabs(vx - 8.0f) < 1e-3f);
	CHECK(std::fabs(vz) < 1e-3f);
	REQUIRE(t.Latest(lt, lx, lz));
	CHECK_EQ(lt, (int64_t)1000);
	CHECK(std::fabs(lx - 108.0f) < 1e-3f);
	CHECK(std::fabs(lz - 50.0f) < 1e-3f);

	CHECK(t.Observe(1250, 110.0f, 50.0f));
	t.Velocity(1250, 1000, 100, vx, vz);
	CHECK(std::fabs(vx - 8.0f) < 1e-3f);

	// Span below minVelocitySpanMs: no velocity.
	BotCore::NavTargetTracker shortT;
	shortT.Observe(0, 100.0f, 50.0f);
	shortT.Observe(50, 100.0f, 50.0f);
	shortT.Velocity(50, 1000, 100, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);

	// Staleness: now == newest + 1000 still valid; +1001 is not.
	t.Velocity(2250, 1000, 100, vx, vz);
	CHECK(std::fabs(vx - 8.0f) < 1e-3f);
	t.Velocity(2251, 1000, 100, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);

	// Non-monotonic observations are dropped.
	const int before = t.Count();
	CHECK(!t.Observe(1250, 999.0f, 999.0f));
	CHECK(!t.Observe(1240, 999.0f, 999.0f));
	CHECK_EQ(t.Count(), before);

	// z component.
	BotCore::NavTargetTracker zt;
	for (int k = 0; k < 5; ++k)
		zt.Observe((int64_t)k * 250, 50.0f, 100.0f - 2.0f * (float)k);
	zt.Velocity(1000, 1000, 100, vx, vz);
	CHECK(std::fabs(vx) < 1e-3f);
	CHECK(std::fabs(vz + 8.0f) < 1e-3f);

	// Capacity wrap keeps the newest 16 samples.
	BotCore::NavTargetTracker wt;
	for (int k = 0; k < 40; ++k)
		wt.Observe((int64_t)k * 10, 100.0f + 0.1f * (float)k, 0.0f);
	CHECK_EQ(wt.Count(), 16);
	wt.Velocity(390, 1000, 100, vx, vz);
	CHECK(std::fabs(vx - 10.0f) < 5e-2f);
	CHECK(std::fabs(vz) < 1e-3f);

	wt.Clear();
	CHECK_EQ(wt.Count(), 0);
}

TEST_CASE("NavTrack_PredictLead")
{
	CHECK(std::fabs(BotCore::NavPredictLead(40.0f, 8.0f, 1.5f) - 1.5f) < 1e-6f);
	CHECK(std::fabs(BotCore::NavPredictLead(4.0f, 8.0f, 1.5f) - 0.5f) < 1e-6f);
	CHECK(std::fabs(BotCore::NavPredictLead(12.0f, 8.0f, 1.5f) - 1.5f) < 1e-6f);
	CHECK_EQ(BotCore::NavPredictLead(8.0f, 0.0f, 1.5f), 0.0f);
	CHECK_EQ(BotCore::NavPredictLead(8.0f, -3.0f, 1.5f), 0.0f);
	CHECK_EQ(BotCore::NavPredictLead(8.0f, 8.0f, 0.0f), 0.0f);
	CHECK_EQ(BotCore::NavPredictLead(0.0f, 8.0f, 1.5f), 0.0f);
	CHECK_EQ(BotCore::NavPredictLead(-5.0f, 8.0f, 1.5f), 0.0f);
}

TEST_CASE("NavTrack_RingCells")
{
	const int n = 30;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	std::vector<BotCore::NavCell> out;

	// (a) Default ring [0, 0] picks the containing cell (and a boundary neighbour).
	BotCore::NavRingCells(grid, 62.0f, 62.0f, 0.0f, 0.0f, Cell(5, 15), out);
	CHECK_EQ((int)out.size(), 1);
	CHECK(out[0] == Cell(15, 15));
	BotCore::NavRingCells(grid, 64.0f, 62.0f, 0.0f, 0.0f, Cell(5, 15), out);
	CHECK_EQ((int)out.size(), 2);
	CHECK(out[0] == Cell(15, 15));
	CHECK(out[1] == Cell(16, 15));

	// (b) Ring [9, 13]: 16 cells, nearest-to-from ordering.
	BotCore::NavRingCells(grid, 62.0f, 62.0f, 9.0f, 13.0f, Cell(5, 15), out);
	CHECK_EQ((int)out.size(), 16);
	CHECK(out[0] == Cell(12, 15));
	CHECK(out[1] == Cell(12, 14));
	CHECK(out[2] == Cell(12, 16));
	for (size_t i = 0; i < out.size(); ++i)
	{
		CHECK(grid.Walk(out[i].x, out[i].z));
		const float d = Dist(grid.CellCenter(out[i].x), grid.CellCenter(out[i].z), 62.0f, 62.0f);
		CHECK(d >= 9.0f - 1e-3f);
		CHECK(d <= 13.0f + 1e-3f);
		if (i > 0)
		{
			const float prev = BotCore::NavOctile(out[i - 1].x - 5, out[i - 1].z - 15, unit);
			const float cur = BotCore::NavOctile(out[i].x - 5, out[i].z - 15, unit);
			CHECK(cur >= prev - 1e-3f);
		}
	}

	// Brute force: the same set of Walk cells within [9, 13] of the centre.
	int brute = 0;
	for (int x = 0; x < n; ++x)
	{
		for (int z = 0; z < n; ++z)
		{
			if (!grid.Walk(x, z))
				continue;
			const float d = Dist(grid.CellCenter(x), grid.CellCenter(z), 62.0f, 62.0f);
			if (d >= 9.0f - 1e-3f && d <= 13.0f + 1e-3f)
			{
				++brute;
				CHECK(std::find(out.begin(), out.end(), Cell(x, z)) != out.end());
			}
		}
	}
	CHECK_EQ((int)out.size(), brute);

	// (c) Blocked containing cell -> empty; off-grid centre -> empty and no crash.
	std::vector<int16_t> ev = RingEvents(30);
	ev[CellIndex(30, 15, 15)] = 0;
	BotCore::NavGrid blocked = MakeNav(30, unit, ev, HeightZeros(30));
	BotCore::NavRingCells(blocked, 62.0f, 62.0f, 0.0f, 0.0f, Cell(5, 15), out);
	CHECK(out.empty());
	BotCore::NavRingCells(blocked, -40.0f, 62.0f, 0.0f, 0.0f, Cell(5, 15), out);
	CHECK(out.empty());

	// (d) Large ring on a bigger grid.
	BotCore::NavGrid big = MakeNav(60, unit, RingEvents(60), HeightZeros(60));
	BotCore::NavRingCells(big, 122.0f, 122.0f, 30.0f, 45.0f, Cell(10, 30), out);
	CHECK(!out.empty());
	for (size_t i = 0; i < out.size(); ++i)
	{
		CHECK(big.Walk(out[i].x, out[i].z));
		const float d = Dist(big.CellCenter(out[i].x), big.CellCenter(out[i].z), 122.0f, 122.0f);
		CHECK(d >= 30.0f - 1e-3f);
		CHECK(d <= 45.0f + 1e-3f);
	}

	// (e) Inverted and negative radii.
	BotCore::NavRingCells(grid, 62.0f, 62.0f, 20.0f, 10.0f, Cell(5, 15), out);
	CHECK(out.empty());
	BotCore::NavRingCells(grid, 62.0f, 62.0f, -5.0f, 0.0f, Cell(5, 15), out);
	CHECK_EQ((int)out.size(), 1);
	CHECK(out[0] == Cell(15, 15));
}

TEST_CASE("NavTrack_Follower_Triggers")
{
	const int n = 40;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	BotCore::NavPathfinder pf;
	BotCore::NavFollowParams params;
	BotCore::NavFollower f;

	CHECK(!f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, params));
	CHECK(f.Plan().status == BotCore::NavFollowStatus::NoTarget);
	CHECK_EQ(f.Replans(), 0);
	CHECK(f.LastReason() == BotCore::NavReplanReason::None);

	CHECK(f.ObserveTarget(0, 82.0f, 22.0f));
	CHECK(f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, params));
	CHECK_EQ(f.Replans(), 1);
	CHECK(f.LastReason() == BotCore::NavReplanReason::First);
	CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(f.Plan().goal == Cell(20, 5));
	std::vector<BotCore::NavCell> expect;
	expect.push_back(Cell(5, 5));
	expect.push_back(Cell(20, 5));
	CHECK(f.Plan().smooth.waypoints == expect);
	CHECK(std::fabs(f.Plan().smooth.length - 60.0f) < 1e-3f);
	CHECK_EQ(f.Plan().tries, 1);
	CHECK_EQ(f.Plan().leadSec, 0.0f);
	CHECK_EQ(f.Plan().plannedAtMs, (int64_t)0);
	CHECK_EQ(f.Plan().targetX, 82.0f);
	CHECK_EQ(f.Plan().targetZ, 22.0f);

	CHECK(!f.Update(grid, pf, 100, 22.0f, 22.0f, 8.0f, params));
	CHECK(!f.Update(grid, pf, 499, 22.0f, 22.0f, 8.0f, params));
	CHECK_EQ(f.Replans(), 1);
	CHECK(f.Update(grid, pf, 500, 22.0f, 22.0f, 8.0f, params));
	CHECK(f.LastReason() == BotCore::NavReplanReason::Interval);
	CHECK_EQ(f.Replans(), 2);
	CHECK(!f.Update(grid, pf, 999, 22.0f, 22.0f, 8.0f, params));
	CHECK(f.Update(grid, pf, 1000, 22.0f, 22.0f, 8.0f, params));
	CHECK_EQ(f.Replans(), 3);

	CHECK(f.ObserveTarget(1050, 87.5f, 22.0f));
	CHECK(!f.Update(grid, pf, 1050, 22.0f, 22.0f, 8.0f, params));
	f.ObserveTarget(1100, 88.0f, 22.0f);
	CHECK(f.Update(grid, pf, 1100, 22.0f, 22.0f, 8.0f, params));
	CHECK(f.LastReason() == BotCore::NavReplanReason::Moved);
	CHECK_EQ(f.Replans(), 4);
	CHECK(f.Plan().goal == Cell(21, 5));
	CHECK_EQ(f.Plan().plannedAtMs, (int64_t)1100);
	CHECK(!f.Update(grid, pf, 1599, 22.0f, 22.0f, 8.0f, params));
	CHECK(f.Update(grid, pf, 1600, 22.0f, 22.0f, 8.0f, params));
	CHECK(f.LastReason() == BotCore::NavReplanReason::Interval);
	CHECK_EQ(f.Replans(), 5);

	// replanDistM = 0 disables the Moved trigger.
	BotCore::NavFollower g;
	BotCore::NavFollowParams p0;
	p0.replanDistM = 0.0f;
	CHECK(g.ObserveTarget(0, 82.0f, 22.0f));
	CHECK(g.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, p0));
	CHECK(g.ObserveTarget(100, 140.0f, 22.0f));
	CHECK(!g.Update(grid, pf, 100, 22.0f, 22.0f, 8.0f, p0));
	CHECK(g.Update(grid, pf, 500, 22.0f, 22.0f, 8.0f, p0));

	// replanIntervalMs = 200.
	BotCore::NavFollower hh;
	BotCore::NavFollowParams p200;
	p200.replanIntervalMs = 200;
	CHECK(hh.ObserveTarget(0, 82.0f, 22.0f));
	CHECK(hh.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, p200));
	CHECK(!hh.Update(grid, pf, 199, 22.0f, 22.0f, 8.0f, p200));
	CHECK(hh.Update(grid, pf, 200, 22.0f, 22.0f, 8.0f, p200));

	// Non-monotonic observations are dropped.
	BotCore::NavFollower mono;
	CHECK(mono.ObserveTarget(1000, 82.0f, 22.0f));
	CHECK(!mono.ObserveTarget(1000, 82.0f, 22.0f));
	CHECK(!mono.ObserveTarget(900, 82.0f, 22.0f));

	// Reset forgets everything.
	f.Reset();
	CHECK(f.Plan().status == BotCore::NavFollowStatus::NoTarget);
	CHECK_EQ(f.Replans(), 0);
	CHECK_EQ(f.Tracker().Count(), 0);
	CHECK(!f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, params));
}

TEST_CASE("NavTrack_Follower_Prediction")
{
	const int n = 40;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	BotCore::NavPathfinder pf;
	BotCore::NavFollowParams params;

	// (a) Full lead from far away.
	{
		BotCore::NavFollower f;
		Observe5(f, 82.0f, 2.0f, 22.0f);
		CHECK(f.Update(grid, pf, 1000, 22.0f, 22.0f, 8.0f, params));
		CHECK(std::fabs(f.Plan().predX - 102.0f) < 1e-3f);
		CHECK(std::fabs(f.Plan().predZ - 22.0f) < 1e-3f);
		CHECK(std::fabs(f.Plan().leadSec - 1.5f) < 1e-6f);
		CHECK(f.Plan().goal == Cell(25, 5));
	}

	// (b) Short distance caps the lead.
	{
		BotCore::NavFollower f;
		Observe5(f, 82.0f, 2.0f, 22.0f);
		CHECK(f.Update(grid, pf, 1000, 82.0f, 22.0f, 8.0f, params));
		CHECK(std::fabs(f.Plan().leadSec - 1.0f) < 1e-6f);
		CHECK(std::fabs(f.Plan().predX - 98.0f) < 1e-3f);
		CHECK(f.Plan().goal == Cell(24, 5));
	}

	// (c) Fast own speed, zero own speed and maxLeadSec = 0.
	{
		BotCore::NavFollower f;
		Observe5(f, 82.0f, 2.0f, 22.0f);
		CHECK(f.Update(grid, pf, 1000, 22.0f, 22.0f, 40.0f, params));
		CHECK(std::fabs(f.Plan().leadSec - 1.5f) < 1e-6f);
		CHECK(f.Plan().goal == Cell(25, 5));
	}
	{
		BotCore::NavFollower f;
		Observe5(f, 82.0f, 2.0f, 22.0f);
		CHECK(f.Update(grid, pf, 1000, 22.0f, 22.0f, 0.0f, params));
		CHECK_EQ(f.Plan().leadSec, 0.0f);
		CHECK(f.Plan().goal == Cell(22, 5));
	}
	{
		BotCore::NavFollower f;
		BotCore::NavFollowParams p0;
		p0.maxLeadSec = 0.0f;
		Observe5(f, 82.0f, 2.0f, 22.0f);
		CHECK(f.Update(grid, pf, 1000, 22.0f, 22.0f, 8.0f, p0));
		CHECK_EQ(f.Plan().leadSec, 0.0f);
		CHECK(f.Plan().goal == Cell(22, 5));
	}

	// (d) Stale observation drops the velocity: now = newest + 1001 / + 1000.
	{
		BotCore::NavFollower f;
		Observe5(f, 82.0f, 2.0f, 22.0f);
		CHECK(f.Update(grid, pf, 2001, 22.0f, 22.0f, 8.0f, params));
		CHECK_EQ(f.Plan().leadSec, 0.0f);
		CHECK(f.Plan().goal == Cell(22, 5));
	}
	{
		BotCore::NavFollower f;
		Observe5(f, 82.0f, 2.0f, 22.0f);
		CHECK(f.Update(grid, pf, 2000, 22.0f, 22.0f, 8.0f, params));
		CHECK(f.Plan().goal == Cell(25, 5));
	}

	// (e) Prediction back-off: 1.5 -> 0.75 -> 0.375 onto the left half.
	{
		BotCore::NavGrid wall = MakeNav(40, unit, WallColumn(40, 20), HeightZeros(40));
		REQUIRE(!wall.Walk(25, 10));
		REQUIRE(wall.Walk(10, 10));
		BotCore::NavFollower f;
		Observe5(f, 66.0f, 2.0f, 82.0f);
		CHECK(f.Update(wall, pf, 1000, 22.0f, 82.0f, 8.0f, params));
		CHECK(std::fabs(f.Plan().leadSec - 0.375f) < 1e-6f);
		CHECK(std::fabs(f.Plan().predX - 77.0f) < 1e-3f);
		CHECK(f.Plan().goal == Cell(19, 20));
		CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
	}
}

TEST_CASE("NavTrack_Follower_Ring")
{
	const int n = 60;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	BotCore::NavPathfinder pf;
	BotCore::NavFollowParams params;
	params.ringMinM = 30.0f;
	params.ringMaxM = 45.0f;

	// (a) Bot outside the ring: nearest ring cell to the bot, straight line.
	{
		BotCore::NavFollower f;
		f.ObserveTarget(0, 122.0f, 122.0f);
		CHECK(f.Update(grid, pf, 0, 42.0f, 122.0f, 8.0f, params));
		CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
		CHECK(f.Plan().goal == Cell(19, 30));
		std::vector<BotCore::NavCell> exp;
		exp.push_back(Cell(10, 30));
		exp.push_back(Cell(19, 30));
		CHECK(f.Plan().smooth.waypoints == exp);
		CHECK(std::fabs(f.Plan().smooth.length - 36.0f) < 1e-3f);
		CHECK_EQ(f.Plan().tries, 1);
		const float d = Dist(grid.CellCenter(19), grid.CellCenter(30), 122.0f, 122.0f);
		CHECK(std::fabs(d - 44.0f) < 1e-3f);
	}

	// (b) Bot already inside the ring.
	{
		BotCore::NavFollower f;
		f.ObserveTarget(0, 122.0f, 122.0f);
		CHECK(f.Update(grid, pf, 0, 90.0f, 122.0f, 8.0f, params));
		CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
		CHECK(f.Plan().goal == Cell(22, 30));
		CHECK_EQ((int)f.Plan().smooth.waypoints.size(), 1);
		CHECK_EQ(f.Plan().smooth.length, 0.0f);
		CHECK_EQ(f.Plan().tries, 1);
	}

	// (c) Bot on top of the target: four equal candidates, tie by x then z.
	{
		BotCore::NavFollower f;
		f.ObserveTarget(0, 122.0f, 122.0f);
		CHECK(f.Update(grid, pf, 0, 122.0f, 122.0f, 8.0f, params));
		CHECK(f.Plan().goal == Cell(22, 30));
	}

	// (d) Default ring [0, 0].
	{
		BotCore::NavFollower f;
		BotCore::NavFollowParams p0;
		f.ObserveTarget(0, 122.0f, 122.0f);
		CHECK(f.Update(grid, pf, 0, 42.0f, 122.0f, 8.0f, p0));
		CHECK(f.Plan().goal == Cell(30, 30));
		CHECK_EQ((int)f.Plan().smooth.waypoints.size(), 2);
	}
}

TEST_CASE("NavTrack_Follower_Failures")
{
	const float unit = 4.0f;

	// (a) NoGoal and (b) ring fallback on a split map.
	{
		BotCore::NavGrid grid = MakeNav(30, unit, WallColumn(30, 15), HeightZeros(30));
		REQUIRE(grid.Walk(5, 5));
		REQUIRE(!grid.Walk(22, 10));
		BotCore::NavPathfinder pf;
		BotCore::NavFollowParams params;

		BotCore::NavFollower f;
		f.ObserveTarget(0, 90.0f, 42.0f);
		CHECK(f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, params));
		CHECK(f.Plan().status == BotCore::NavFollowStatus::NoGoal);
		CHECK_EQ(f.Plan().tries, 0);
		CHECK(f.Plan().smooth.waypoints.empty());
		CHECK_EQ(f.Plan().smooth.length, 0.0f);

		BotCore::NavFollower g;
		BotCore::NavFollowParams pr;
		pr.ringMaxM = 13.0f;
		g.ObserveTarget(0, 66.0f, 42.0f);
		CHECK(g.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, pr));
		CHECK(g.Plan().status == BotCore::NavFollowStatus::Planned);
		CHECK(g.Plan().goal == Cell(13, 9));
		CHECK_EQ(g.Plan().tries, 1);
		CHECK(grid.Walk(g.Plan().goal.x, g.Plan().goal.z));
		const float d = Dist(grid.CellCenter(g.Plan().goal.x), grid.CellCenter(g.Plan().goal.z), 66.0f, 42.0f);
		CHECK(d <= 13.0f + 1e-3f);
	}

	// (c) PathFailed: goal ring inside a raised, unwalkable-by-edge block.
	{
		const int n = 20;
		std::vector<int16_t> ev = RingEvents(n);
		std::vector<float> heights((size_t)n * (size_t)n, 0.0f);
		for (int x = 10; x <= 14; ++x)
		{
			for (int z = 8; z <= 12; ++z)
				heights[CellIndex(n, x, z)] = 20.0f;
		}
		BotCore::NavGrid grid = MakeNav(n, unit, ev, heights);
		REQUIRE(grid.Walk(12, 10));
		REQUIRE(grid.Walk(5, 5));
		BotCore::NavPathfinder pf;
		BotCore::NavFollowParams params;
		params.ringMinM = 4.0f;
		params.ringMaxM = 9.0f;

		{
			BotCore::NavFollower f;
			f.ObserveTarget(0, 50.0f, 42.0f);
			CHECK(f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, params));
			CHECK(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
			CHECK(f.Plan().pathStatus == BotCore::NavPathStatus::NoPath);
			CHECK_EQ(f.Plan().tries, 3);
			CHECK(f.Plan().smooth.waypoints.empty());
			CHECK(f.Plan().expanded > 0);
		}
		{
			BotCore::NavFollower f;
			BotCore::NavFollowParams p1 = params;
			p1.ringMaxTries = 1;
			f.ObserveTarget(0, 50.0f, 42.0f);
			f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, p1);
			CHECK_EQ(f.Plan().tries, 1);
		}
		{
			BotCore::NavFollower f;
			BotCore::NavFollowParams pz = params;
			pz.ringMaxTries = 0;
			f.ObserveTarget(0, 50.0f, 42.0f);
			f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, pz);
			CHECK_EQ(f.Plan().tries, 1);
		}
		{
			BotCore::NavFollower f;
			BotCore::NavFollowParams pn = params;
			pn.ringMaxTries = -2;
			f.ObserveTarget(0, 50.0f, 42.0f);
			f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, pn);
			CHECK_EQ(f.Plan().tries, 1);
		}
		{
			BotCore::NavFollower f;
			BotCore::NavFollowParams p2 = params;
			p2.ringMaxTries = 2;
			f.ObserveTarget(0, 50.0f, 42.0f);
			f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, p2);
			CHECK_EQ(f.Plan().tries, 2);
		}
		{
			BotCore::NavFollower f;
			BotCore::NavFollowParams p0;
			f.ObserveTarget(0, 50.0f, 42.0f);
			CHECK(f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, p0));
			CHECK(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
			CHECK_EQ(f.Plan().tries, 1);
		}

		// Bot inside the block: ring cells are reachable and the plan succeeds.
		{
			BotCore::NavFollower f;
			f.ObserveTarget(0, 50.0f, 42.0f);
			CHECK(f.Update(grid, pf, 0, 50.0f, 42.0f, 8.0f, params));
			CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
			CHECK(f.Plan().goal == Cell(11, 10));
			CHECK_EQ(f.Plan().tries, 1);
		}
	}

	// (d) InvalidStart: blocked or off-grid bot cell.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavPathfinder pf;
		BotCore::NavFollowParams params;
		{
			BotCore::NavFollower f;
			f.ObserveTarget(0, 82.0f, 22.0f);
			CHECK(f.Update(grid, pf, 0, 2.0f, 2.0f, 8.0f, params));
			CHECK(f.Plan().status == BotCore::NavFollowStatus::InvalidStart);
			CHECK(f.Plan().pathStatus == BotCore::NavPathStatus::InvalidStart);
			CHECK_EQ(f.Plan().tries, 1);
			CHECK(!f.Update(grid, pf, 100, 2.0f, 2.0f, 8.0f, params));
			CHECK(f.Update(grid, pf, 500, 2.0f, 2.0f, 8.0f, params));
		}
		{
			BotCore::NavFollower f;
			f.ObserveTarget(0, 82.0f, 22.0f);
			CHECK(f.Update(grid, pf, 0, -20.0f, 22.0f, 8.0f, params));
			CHECK(f.Plan().status == BotCore::NavFollowStatus::InvalidStart);
		}
	}

	// (e) Reset clears the plan.
	{
		BotCore::NavGrid grid = MakeNav(40, unit, RingEvents(40), HeightZeros(40));
		BotCore::NavPathfinder pf;
		BotCore::NavFollowParams params;
		BotCore::NavFollower f;
		f.ObserveTarget(0, 82.0f, 22.0f);
		f.Update(grid, pf, 0, 22.0f, 22.0f, 8.0f, params);
		f.Reset();
		CHECK(f.Plan().status == BotCore::NavFollowStatus::NoTarget);
		CHECK(f.Plan().smooth.waypoints.empty());
	}
}

TEST_CASE("NavTrack_Chase_Sim")
{
	const int n = 80;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	BotCore::NavPathfinder pf;

	int replans = 0;
	int planned = 0;
	long long caught = 0;
	double distMin = 0.0;
	double distMax = 0.0;
	double finalDist = 0.0;

	for (int pass = 0; pass < 2; ++pass)
	{
		BotCore::NavFollowParams params;
		const char * ring = "0-0";
		if (pass == 1)
		{
			params.ringMinM = 30.0f;
			params.ringMaxM = 45.0f;
			ring = "30-45";
		}

		BotCore::NavFollower follower;
		double botX = 82.0;
		double botZ = 162.0;
		double tarX = 122.0;
		double tarZ = 162.0;
		std::vector<BotCore::NavCell> route;

		replans = 0;
		planned = 0;
		caught = 0;
		distMin = 1e9;
		distMax = -1e9;
		finalDist = 0.0;

		for (long long t = 0; t <= 14000; t += 100)
		{
			if (t % 200 == 0)
				follower.ObserveTarget(t, (float)tarX, (float)tarZ);

			if (follower.Update(grid, pf, t, (float)botX, (float)botZ, 9.0f, params))
			{
				++replans;
				if (follower.Plan().status == BotCore::NavFollowStatus::Planned)
				{
					++planned;
					route.clear();
					const std::vector<BotCore::NavCell> & wp = follower.Plan().smooth.waypoints;
					for (size_t i = 1; i < wp.size(); ++i)
						route.push_back(wp[i]);
				}
				else
				{
					route.clear();
				}
			}

			double move = 0.9;
			while (move > 1e-9 && !route.empty())
			{
				const double wx = grid.CellCenter(route[0].x);
				const double wz = grid.CellCenter(route[0].z);
				const double ddx = wx - botX;
				const double ddz = wz - botZ;
				const double d = std::sqrt(ddx * ddx + ddz * ddz);
				if (d <= move)
				{
					botX = wx;
					botZ = wz;
					move -= d;
					route.erase(route.begin());
				}
				else
				{
					botX += ddx / d * move;
					botZ += ddz / d * move;
					move = 0.0;
				}
			}

			tarX += 0.6;
			const double d = std::sqrt((tarX - botX) * (tarX - botX) + (tarZ - botZ) * (tarZ - botZ));
			if (d < distMin)
				distMin = d;
			if (d > distMax)
				distMax = d;
			finalDist = d;
			if (caught == 0 && d <= 6.0)
				caught = t + 100;
		}

		if (pass == 0)
		{
			std::printf("NAVTRACK chase ring=%s replans=%d planned=%d caught_ms=%lld final_dist=%.1f\n",
				ring, replans, planned, caught, finalDist);
			CHECK_EQ(replans, 29);
			CHECK_EQ(planned, 29);
			CHECK(caught > 0);
			CHECK(caught <= 13000);
			CHECK(finalDist <= 8.0);
		}
		else
		{
			std::printf("NAVTRACK chase ring=%s replans=%d planned=%d dist_min=%.1f dist_max=%.1f\n",
				ring, replans, planned, distMin, distMax);
			CHECK_EQ(replans, 29);
			CHECK_EQ(planned, 29);
			CHECK(distMin >= 28.0);
			CHECK(distMax <= 47.0);
		}
	}
}

TEST_CASE("NavTrack_RealMap")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVTRACK real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	BotCore::NavCell a;
	a.x = grid.CellOf(1274.0f);
	a.z = grid.CellOf(890.0f);
	BotCore::NavCell b;
	b.x = grid.CellOf(746.0f);
	b.z = grid.CellOf(1106.0f);
	REQUIRE(grid.Walk(a.x, a.z));
	REQUIRE(grid.Walk(b.x, b.z));

	const float ax = grid.CellCenter(a.x);
	const float az = grid.CellCenter(a.z);
	const float bx = grid.CellCenter(b.x);
	const float bz = grid.CellCenter(b.z);

	BotCore::NavPathfinder finder;
	BotCore::NavFollowParams params;

	// Exact [0, 0] ring A -> B.
	BotCore::NavFollower f;
	f.ObserveTarget(0, bx, bz);
	CHECK(f.Update(grid, finder, 0, ax, az, 8.0f, params));
	CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(f.Plan().goal == b);
	CHECK_EQ(f.Plan().tries, 1);
	CHECK((int)f.Plan().smooth.waypoints.size() >= 2);
	CHECK((int)f.Plan().smooth.waypoints.size() <= 25);
	CHECK(f.Plan().smooth.waypoints.front() == a);
	CHECK(f.Plan().smooth.waypoints.back() == b);
	CHECK(SegmentsClear(grid, f.Plan().smooth.waypoints));
	CHECK(f.Plan().smooth.length >= 570.4f);
	CHECK(f.Plan().smooth.length <= 661.1f);
	const float exactLength = f.Plan().smooth.length;
	const int exactWaypoints = (int)f.Plan().smooth.waypoints.size();

	// Mage [30, 45] ring A -> B.
	BotCore::NavFollower m;
	BotCore::NavFollowParams pm;
	pm.ringMinM = 30.0f;
	pm.ringMaxM = 45.0f;
	m.ObserveTarget(0, bx, bz);
	CHECK(m.Update(grid, finder, 0, ax, az, 8.0f, pm));
	CHECK(m.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(m.Plan().tries <= 3);
	CHECK((int)m.Plan().smooth.waypoints.size() >= 2);
	CHECK((int)m.Plan().smooth.waypoints.size() <= 25);
	CHECK(m.Plan().smooth.waypoints.front() == a);
	CHECK(m.Plan().smooth.waypoints.back() == m.Plan().goal);
	CHECK(SegmentsClear(grid, m.Plan().smooth.waypoints));
	CHECK(grid.Walk(m.Plan().goal.x, m.Plan().goal.z));
	const float mageDist = Dist(grid.CellCenter(m.Plan().goal.x), grid.CellCenter(m.Plan().goal.z), bx, bz);
	CHECK(mageDist >= 30.0f);
	CHECK(mageDist <= 45.0f);
	CHECK(m.Plan().smooth.length <= exactLength - 20.0f);
	const int mageWaypoints = (int)m.Plan().smooth.waypoints.size();
	const float mageLength = m.Plan().smooth.length;

	// Mage [30, 45] ring B -> A.
	BotCore::NavFollower mr;
	mr.ObserveTarget(0, ax, az);
	CHECK(mr.Update(grid, finder, 0, bx, bz, 8.0f, pm));
	CHECK(mr.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(mr.Plan().tries <= 3);
	CHECK((int)mr.Plan().smooth.waypoints.size() >= 2);
	CHECK((int)mr.Plan().smooth.waypoints.size() <= 25);
	CHECK(mr.Plan().smooth.waypoints.front() == b);
	CHECK(mr.Plan().smooth.waypoints.back() == mr.Plan().goal);
	CHECK(SegmentsClear(grid, mr.Plan().smooth.waypoints));
	CHECK(grid.Walk(mr.Plan().goal.x, mr.Plan().goal.z));
	const float mageReverse = Dist(grid.CellCenter(mr.Plan().goal.x), grid.CellCenter(mr.Plan().goal.z), ax, az);
	CHECK(mageReverse >= 30.0f);
	CHECK(mageReverse <= 45.0f);

	// Bot already inside the ring (target 36 m away): goal is the bot's own cell.
	BotCore::NavFollower h;
	h.ObserveTarget(0, 1310.0f, 890.0f);
	CHECK(h.Update(grid, finder, 0, ax, az, 8.0f, pm));
	CHECK(h.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(h.Plan().goal == a);
	CHECK_EQ((int)h.Plan().smooth.waypoints.size(), 1);

	// Moving target along a real path.
	BotCore::NavPathResult path;
	BotCore::NavSearchParams search;
	finder.Find(grid, a, b, search, path);
	REQUIRE(path.status == BotCore::NavPathStatus::Found);
	REQUIRE((int)path.cells.size() >= 109);

	BotCore::NavFollower mv;
	const int idx[5] = { 100, 102, 104, 106, 108 };
	for (int k = 0; k < 5; ++k)
	{
		const BotCore::NavCell & c = path.cells[(size_t)idx[k]];
		mv.ObserveTarget((int64_t)k * 250, grid.CellCenter(c.x), grid.CellCenter(c.z));
	}
	CHECK(mv.Update(grid, finder, 1000, ax, az, 8.0f, params));
	CHECK(mv.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(mv.Plan().leadSec >= 0.0f);
	CHECK(mv.Plan().leadSec <= 1.5f);
	CHECK(grid.Walk(grid.CellOf(mv.Plan().predX), grid.CellOf(mv.Plan().predZ)));
	CHECK(grid.Walk(mv.Plan().goal.x, mv.Plan().goal.z));
	const float gd = Dist(grid.CellCenter(mv.Plan().goal.x), grid.CellCenter(mv.Plan().goal.z), mv.Plan().predX, mv.Plan().predZ);
	CHECK(gd <= 2.9f);
	CHECK(mv.Plan().smooth.waypoints.front() == a);
	CHECK(SegmentsClear(grid, mv.Plan().smooth.waypoints));

	std::printf("NAVTRACK real exact: waypoints=%d length=%.3f; mage: goal=(%d,%d) d=%.2f waypoints=%d length=%.3f\n",
		exactWaypoints, exactLength, m.Plan().goal.x, m.Plan().goal.z, mageDist, mageWaypoints, mageLength);
}

TEST_CASE("NavTrack_Perf")
{
	BotCore::NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVTRACK real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	const int n = grid.Size();
	std::vector<BotCore::NavCell> cells;
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
		const int si = (int)rng.NextBelow((uint32_t)cells.size());
		const int gi = (int)rng.NextBelow((uint32_t)cells.size());
		if (si == gi)
			continue;
		const int dx = std::abs(cells[(size_t)si].x - cells[(size_t)gi].x);
		const int dz = std::abs(cells[(size_t)si].z - cells[(size_t)gi].z);
		if (std::max(dx, dz) > 64)
			continue;
		starts.push_back(cells[(size_t)si]);
		goals.push_back(cells[(size_t)gi]);
	}

	BotCore::NavPathfinder pf;

	auto runSet = [&](float ringMin, float ringMax, const char * name)
	{
		BotCore::NavFollower follower;
		BotCore::NavFollowParams params;
		params.ringMinM = ringMin;
		params.ringMaxM = ringMax;

		for (int q = 0; q < 20; ++q)
		{
			follower.Reset();
			follower.ObserveTarget(0, grid.CellCenter(goals[(size_t)q].x), grid.CellCenter(goals[(size_t)q].z));
			follower.Update(grid, pf, 0, grid.CellCenter(starts[(size_t)q].x), grid.CellCenter(starts[(size_t)q].z), 8.0f, params);
		}

		std::vector<double> ms;
		ms.reserve((size_t)queries);
		long long triesSum = 0;
		int planned = 0;
		bool invariants = true;
		int checked = 0;

		for (int q = 0; q < queries; ++q)
		{
			follower.Reset();
			follower.ObserveTarget(0, grid.CellCenter(goals[(size_t)q].x), grid.CellCenter(goals[(size_t)q].z));
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			follower.Update(grid, pf, 0, grid.CellCenter(starts[(size_t)q].x), grid.CellCenter(starts[(size_t)q].z), 8.0f, params);
			const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
			ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

			triesSum += (long long)follower.Plan().tries;
			if (follower.Plan().status != BotCore::NavFollowStatus::Planned)
				continue;
			++planned;

			if (checked < 50)
			{
				if (!SegmentsClear(grid, follower.Plan().smooth.waypoints))
					invariants = false;
				const BotCore::NavCell goal = follower.Plan().goal;
				const float d = Dist(grid.CellCenter(goal.x), grid.CellCenter(goal.z),
					grid.CellCenter(goals[(size_t)q].x), grid.CellCenter(goals[(size_t)q].z));
				if (ringMax > 0.0f)
				{
					if (d < ringMin - 1e-3f || d > ringMax + 1e-3f)
						invariants = false;
				}
				else if (d > 3.0f)
				{
					invariants = false;
				}
				++checked;
			}
		}

		std::sort(ms.begin(), ms.end());
		const double p50 = PercentileDouble(ms, 0.50);
		const double p95 = PercentileDouble(ms, 0.95);
		const double p99 = PercentileDouble(ms, 0.99);
		const double triesMean = (double)triesSum / (double)queries;

		std::printf("NAVTRACK perf set=%s planned=%d tries_mean=%.2f ms_p50=%.3f ms_p95=%.3f ms_p99=%.3f\n",
			name, planned, triesMean, p50, p95, p99);

		CHECK(planned * 100 >= queries * 95);
		CHECK(triesMean >= 1.0);
		CHECK(triesMean <= 1.5);
		CHECK(invariants);
#ifndef _DEBUG
		CHECK(p95 <= 2.0);
#endif
	};

	runSet(0.0f, 0.0f, "exact");
	runSet(30.0f, 45.0f, "mage");
}
