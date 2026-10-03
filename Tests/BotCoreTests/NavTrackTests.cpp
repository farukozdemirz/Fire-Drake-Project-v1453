#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavSmooth.h>
#include <BotCore/NavTrack.h>
#include <BotCore/NavReach.h>
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

TEST_CASE("NavTrack_Velocity_PacketCadence")
{
	// A target moving at a constant 4.5 m/s observed at the real WIZ_MOVE cadence (~1.5 s). The
	// old 1000/100 defaults saw no second sample and returned 0 at 1.5 s; the 4000/400 window must
	// recover 4.5 m/s at every interval (docs/12 s13.2, DEG-19).
	const double mps = 4.5;
	const int intervals[5] = { 500, 1000, 1500, 1540, 2000 };
	for (int i = 0; i < 5; ++i)
	{
		const int64_t period = intervals[i];
		BotCore::NavTargetTracker t;
		int samples = 0;
		int zero = 0;
		int nonzero = 0;
		float minMag = 1e9f;
		float maxMag = -1e9f;
		int nextObs = 0;
		for (int64_t now = 0; now <= 20000; now += 100)
		{
			while ((int64_t)nextObs * period <= now)
			{
				const int64_t ot = (int64_t)nextObs * period;
				t.Observe(ot, (float)(mps * (double)ot / 1000.0), 0.0f, (int16_t)45);
				++nextObs;
				++samples;
			}
			if (t.Count() < 2)
				continue;
			int64_t nt = 0;
			float nx = 0.0f;
			float nz = 0.0f;
			REQUIRE(t.Latest(nt, nx, nz));
			if (now - nt > 4000)
				continue;
			float vx = 0.0f;
			float vz = 0.0f;
			t.Velocity(now, 4000, 400, vx, vz);
			const float mag = std::sqrt(vx * vx + vz * vz);
			if (mag <= 1e-3f)
			{
				++zero;
			}
			else
			{
				++nonzero;
				if (mag < minMag)
					minMag = mag;
				if (mag > maxMag)
					maxMag = mag;
			}
		}
		std::printf("NAVTRACK cadence period=%lld samples=%d zero=%d nonzero=%d mag_min=%.3f mag_max=%.3f\n",
			(long long)period, samples, zero, nonzero, minMag, maxMag);
		CHECK(nonzero > 0);
		CHECK(minMag >= (float)(mps * 0.9));
		CHECK(maxMag <= (float)(mps * 1.1));
		if (period == 1500 || period == 1540)
			CHECK_EQ(zero, 0);
	}
}

TEST_CASE("NavTrack_Velocity_Speed0")
{
	float vx = 1.0f;
	float vz = 1.0f;

	// The newest packet's speed field is 0: the target stopped.
	BotCore::NavTargetTracker stop;
	stop.Observe(0, 0.0f, 0.0f, (int16_t)45);
	stop.Observe(2000, 9.0f, 0.0f, (int16_t)0);
	stop.Velocity(2000, 4000, 400, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);

	// No speed information (-1): derive from the positions.
	BotCore::NavTargetTracker unk;
	unk.Observe(0, 0.0f, 0.0f);
	unk.Observe(1000, 5.0f, 0.0f);
	unk.Velocity(1000, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 5.0f) < 1e-3f);
	CHECK(std::fabs(vz) < 1e-3f);

	// speed = 45 (4.5 m/s): a 20 m/s position jump is clamped to 4.95 m/s.
	BotCore::NavTargetTracker fast;
	fast.Observe(0, 0.0f, 0.0f, (int16_t)45);
	fast.Observe(500, 10.0f, 0.0f, (int16_t)45);
	fast.Velocity(500, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 4.95f) < 1e-3f);
	CHECK(std::fabs(vz) < 1e-3f);

	// speed = 45 does not inflate a slower estimate.
	BotCore::NavTargetTracker slow;
	slow.Observe(0, 0.0f, 0.0f, (int16_t)45);
	slow.Observe(2000, 4.5f, 0.0f, (int16_t)45);
	slow.Velocity(2000, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 2.25f) < 1e-3f);
}

TEST_CASE("NavTrack_Velocity_Stale")
{
	float vx = 1.0f;
	float vz = 1.0f;

	// Age == window is still valid; age == window + 1 is stale.
	BotCore::NavTargetTracker t;
	t.Observe(0, 0.0f, 0.0f);
	t.Observe(1000, 5.0f, 0.0f);
	t.Velocity(5000, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 5.0f) < 1e-3f);
	t.Velocity(5001, 4000, 400, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);

	// Last two samples below minSpan with no older sample: 0.
	BotCore::NavTargetTracker sh;
	sh.Observe(0, 0.0f, 0.0f);
	sh.Observe(300, 1.5f, 0.0f);
	sh.Velocity(300, 4000, 400, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);

	// Last two samples below minSpan but an older long-enough sample exists: fall back to it.
	BotCore::NavTargetTracker fb;
	fb.Observe(0, 0.0f, 0.0f);
	fb.Observe(1000, 5.0f, 0.0f);
	fb.Observe(1300, 6.5f, 0.0f);
	fb.Velocity(1300, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 5.0f) < 1e-3f);

	// A single sample is not enough.
	BotCore::NavTargetTracker one;
	one.Observe(0, 0.0f, 0.0f);
	one.Velocity(0, 4000, 400, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);
}

TEST_CASE("NavTrack_Velocity_Turn")
{
	// 90 degree turn: the last two samples define the current direction, not the older ones.
	BotCore::NavTargetTracker t;
	t.Observe(0, 0.0f, 0.0f);
	t.Observe(500, 5.0f, 0.0f);    // east, 10 m/s
	t.Observe(1000, 5.0f, 5.0f);   // north, 10 m/s
	float vx = 0.0f;
	float vz = 0.0f;
	t.Velocity(1000, 4000, 400, vx, vz);
	CHECK(std::fabs(vx) < 1e-3f);
	CHECK(std::fabs(vz - 10.0f) < 1e-3f);
}

TEST_CASE("NavTrack_Follower_LeadAtPacketCadence")
{
	const int n = 60;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	BotCore::NavPathfinder pf;
	BotCore::NavFollowParams params;
	BotCore::NavFollower f;

	// Target recedes east at 4.5 m/s, observed every 1500 ms with the speed field set.
	f.ObserveTarget(0, 66.0f, 122.0f, (int16_t)45);
	f.ObserveTarget(1500, 72.75f, 122.0f, (int16_t)45);
	f.ObserveTarget(3000, 79.5f, 122.0f, (int16_t)45);

	CHECK(f.Update(grid, pf, 3000, 75.0f, 122.0f, 8.0f, params));
	CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(f.Plan().leadSec > 0.0f);
	const float lead0 = f.Plan().leadSec;
	CHECK(std::fabs(f.Plan().predX - 82.031f) < 1e-2f);
	CHECK(f.Plan().predZ == 122.0f);

	// Observation age is added to the lead, still capped at maxLeadSec.
	CHECK(f.Update(grid, pf, 3500, 75.0f, 122.0f, 8.0f, params));
	CHECK(f.Plan().leadSec > lead0);
	CHECK(f.Plan().leadSec <= params.maxLeadSec);
	CHECK(f.Plan().predX > 79.5f);

	CHECK(f.Update(grid, pf, 5000, 75.0f, 122.0f, 8.0f, params));
	CHECK(std::fabs(f.Plan().leadSec - params.maxLeadSec) < 1e-6f);
	CHECK(f.Plan().predX > 79.5f);
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
	// With the 4000 ms window (F5-52) the 0 -> 1100 ms samples now yield ~5.45 m/s and the ring
	// centre moves ahead of the target (old 1000 ms window gave 0 and Cell(21, 5)).
	CHECK(f.Plan().goal == Cell(23, 5));
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

	// (d) Stale observation drops the velocity: now = newest + 4001 / + 4000 (F5-52 window).
	{
		BotCore::NavFollower f;
		Observe5(f, 82.0f, 2.0f, 22.0f);
		CHECK(f.Update(grid, pf, 5001, 22.0f, 22.0f, 8.0f, params));
		CHECK_EQ(f.Plan().leadSec, 0.0f);
		CHECK(f.Plan().goal == Cell(22, 5));
	}
	{
		BotCore::NavFollower f;
		Observe5(f, 82.0f, 2.0f, 22.0f);
		CHECK(f.Update(grid, pf, 5000, 22.0f, 22.0f, 8.0f, params));
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

// ---------- F5-56: velocity estimation robustness (arrival jitter, bunching, loss, jumps) ----------
namespace
{
	struct VelObs
	{
		int64_t t = 0;
		float x = 0.0f;
		int16_t speed = -1;
	};

	struct VelStats
	{
		int ticks = 0;
		int zero = 0;
		double p50 = 0.0;
		double p95 = 0.0;
		double max = 0.0;
	};

	float Quant1(double v)
	{
		return (float)(std::floor(v * 10.0 + 0.5) / 10.0);
	}

	// Target walking east at 4.5 m/s, observed every intervalMs. The stamp is the arrival time
	// (send time + uniform jitter); the position is the send-time position (0.1 m rounded), so
	// arrival jitter is a real error source. bunchProb batches a packet with the next one (two
	// packets processed within 10 ms, queue drain), modelled by moving it onto the next nominal
	// arrival so no artificial holes appear.
	std::vector<VelObs> GenCadence(uint64_t seed, int intervalMs, double jitterMs, double bunchProb)
	{
		std::vector<int64_t> sends;
		for (int64_t t = 1000; t < 120000; t += intervalMs)
			sends.push_back(t);

		BotCore::Rng rng(seed);
		std::vector<double> nominal(sends.size());
		for (size_t i = 0; i < sends.size(); ++i)
			nominal[i] = (double)sends[i] + (rng.NextDouble() * 2.0 - 1.0) * jitterMs;

		std::vector<double> arrival(nominal);
		if (bunchProb > 0.0)
		{
			for (size_t i = 1; i + 1 < sends.size(); )
			{
				if (rng.NextDouble() < bunchProb)
				{
					arrival[i] = nominal[i + 1] - rng.NextDouble() * 10.0;
					i += 2;   // a queue batch holds two packets at most
				}
				else
				{
					++i;
				}
			}
		}

		std::vector<VelObs> out;
		out.reserve(sends.size());
		int64_t prev = -1;
		for (size_t i = 0; i < sends.size(); ++i)
		{
			int64_t t = (int64_t)std::llround(arrival[i]);
			if (t <= prev)
				t = prev + 1;
			out.push_back(VelObs{ t, Quant1(4.5 * (double)sends[i] / 1000.0), (int16_t)45 });
			prev = t;
		}
		return out;
	}

	std::vector<VelObs> GenVariable(uint64_t seed)
	{
		BotCore::Rng rng(seed);
		std::vector<VelObs> out;
		int64_t t = 1000;
		while (t < 120000)
		{
			out.push_back(VelObs{ t, Quant1(4.5 * (double)t / 1000.0), (int16_t)45 });
			t += 1000 + (int64_t)rng.NextBelow(1501);
		}
		return out;
	}

	std::vector<VelObs> GenLoss(uint64_t)
	{
		std::vector<VelObs> out;
		int64_t t = 1000;
		int k = 0;
		while (t < 120000)
		{
			if (k % 4 != 3)
				out.push_back(VelObs{ t, Quant1(4.5 * (double)t / 1000.0), (int16_t)45 });
			t += 1500;
			++k;
		}
		return out;
	}

	// One fresh tracker per tick: every observation up to `now` is replayed, matching the tool.
	VelStats RunVel(const std::vector<VelObs> & obs, double mps, int windowMs, int minSpanMs)
	{
		VelStats s;
		std::vector<double> errs;
		const int64_t end = obs.back().t + windowMs;
		for (int64_t now = obs.front().t; now <= end; now += 100)
		{
			BotCore::NavTargetTracker t;
			for (size_t i = 0; i < obs.size(); ++i)
			{
				if (obs[i].t > now)
					break;
				t.Observe(obs[i].t, obs[i].x, 0.0f, obs[i].speed);
			}
			if (t.Count() < 2)
				continue;

			int64_t nt = 0;
			float nx = 0.0f;
			float nz = 0.0f;
			t.Latest(nt, nx, nz);
			if (now - nt > windowMs)
				continue;

			float vx = 0.0f;
			float vz = 0.0f;
			t.Velocity(now, windowMs, minSpanMs, vx, vz);
			const double mag = std::sqrt((double)vx * vx + (double)vz * vz);
			++s.ticks;
			if (mag <= 1e-3)
				++s.zero;
			else
				errs.push_back(std::fabs(mag - mps) / mps);
		}
		std::sort(errs.begin(), errs.end());
		s.p50 = PercentileDouble(errs, 0.50);
		s.p95 = PercentileDouble(errs, 0.95);
		s.max = errs.empty() ? 0.0 : errs.back();
		return s;
	}

	void RunSeeds(const char * name, std::vector<VelObs> (*gen)(uint64_t), double mps,
		double p95Limit, double maxLimit, double zeroLimit)
	{
		double worstP95 = 0.0, worstMax = 0.0, worstZero = 0.0;
		int p95Seed = 0, maxSeed = 0;
		for (int seed = 1; seed <= 20; ++seed)
		{
			const VelStats s = RunVel(gen((uint64_t)seed), mps, 4000, 400);
			const double zp = s.ticks > 0 ? 100.0 * (double)s.zero / (double)s.ticks : 0.0;
			if (s.p95 > worstP95)
			{
				worstP95 = s.p95;
				p95Seed = seed;
			}
			if (s.max > worstMax)
			{
				worstMax = s.max;
				maxSeed = seed;
			}
			if (zp > worstZero)
				worstZero = zp;
		}
		std::printf("NAVTRACK velrobust %s: p95=%.4f (seed %d) max=%.4f (seed %d) zero_pct=%.3f\n",
			name, worstP95, p95Seed, worstMax, maxSeed, worstZero);
		CHECK(worstZero <= zeroLimit);
		CHECK(worstP95 <= p95Limit);
		CHECK(worstMax <= maxLimit);
	}

	long long RunChase(const BotCore::NavGrid & grid, BotCore::NavPathfinder & pf, uint64_t seed,
		bool perfect, int cadenceMs, double jitterMs, double lossProb)
	{
		BotCore::Rng rng(seed);
		std::vector<int64_t> obsTimes;
		if (perfect)
		{
			for (int64_t t = 0; t <= 24000; t += 100)
				obsTimes.push_back(t);
		}
		else
		{
			int64_t prev = -1;
			for (int64_t s = 0; s <= 24000; s += cadenceMs)
			{
				if (rng.NextDouble() < lossProb)
					continue;
				int64_t t = (int64_t)std::llround((double)s + (rng.NextDouble() * 2.0 - 1.0) * jitterMs);
				if (t <= prev)
					t = prev + 1;
				obsTimes.push_back(t);
				prev = t;
			}
		}

		BotCore::NavFollower follower;
		BotCore::NavFollowParams params;
		double botX = 82.0, botZ = 162.0;
		std::vector<BotCore::NavCell> route;
		size_t oi = 0;
		long long caught = 0;
		for (long long t = 0; t <= 24000; t += 100)
		{
			while (oi < obsTimes.size() && obsTimes[oi] <= t)
			{
				const double ox = 122.0 + 0.6 * ((double)obsTimes[oi] / 100.0);
				follower.ObserveTarget(obsTimes[oi], (float)ox, 162.0f);
				++oi;
			}

			if (follower.Update(grid, pf, t, (float)botX, (float)botZ, 9.0f, params))
			{
				if (follower.Plan().status == BotCore::NavFollowStatus::Planned)
				{
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

			const double tx = 122.0 + 0.6 * ((double)t / 100.0);
			const double d = std::sqrt((tx - botX) * (tx - botX) + (162.0 - botZ) * (162.0 - botZ));
			if (caught == 0 && d <= 6.0)
				caught = t + 100;
		}
		return caught;
	}
}

TEST_CASE("NavTrack_VelocityRobust_ArrivalJitter")
{
	RunSeeds("ArrivalJitter", +[](uint64_t s) { return GenCadence(s, 1500, 150.0, 0.0); },
		4.5, 0.20, 0.30, 0.0);
}

TEST_CASE("NavTrack_VelocityRobust_ArrivalBunching")
{
	RunSeeds("ArrivalBunching", +[](uint64_t s) { return GenCadence(s, 1500, 150.0, 0.05); },
		4.5, 0.20, 0.60, 1.0);
}

TEST_CASE("NavTrack_VelocityRobust_VariableInterval")
{
	RunSeeds("VariableInterval", &GenVariable, 4.5, 0.10, 0.20, 0.0);
}

TEST_CASE("NavTrack_VelocityRobust_PacketLoss")
{
	RunSeeds("PacketLoss", &GenLoss, 4.5, 0.10, 0.20, 0.0);

	// A 4500 ms hole is outside the 4000 ms window: the estimate is 0 through the hole and
	// recovers after two new observations (one packet of transient).
	BotCore::NavTargetTracker g;
	g.Observe(1000, 0.0f, 0.0f, (int16_t)-1);
	g.Observe(2500, 6.75f, 0.0f, (int16_t)-1);
	float vx = 0.0f;
	float vz = 0.0f;
	g.Velocity(6500, 4000, 400, vx, vz);   // age == window, still valid
	CHECK(std::fabs(vx - 4.5f) < 1e-3f);
	for (int64_t now = 6501; now < 7000; now += 100)
	{
		g.Velocity(now, 4000, 400, vx, vz);
		CHECK_EQ(vx, 0.0f);
		CHECK_EQ(vz, 0.0f);
	}

	g.Observe(7000, 27.0f, 0.0f, (int16_t)-1);   // the gap is 4500 ms
	g.Velocity(7000, 4000, 400, vx, vz);
	CHECK_EQ(vx, 0.0f);   // transient: the only pair is outside the window
	CHECK_EQ(vz, 0.0f);

	g.Observe(8500, 33.75f, 0.0f, (int16_t)-1);
	g.Velocity(8500, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 4.5f) < 1e-3f);
	CHECK(std::fabs(vz) < 1e-3f);
}

TEST_CASE("NavTrack_VelocityRobust_Stale")
{
	// Age == window is still valid; window + 1 is stale.
	BotCore::NavTargetTracker t;
	t.Observe(0, 0.0f, 0.0f, (int16_t)-1);
	t.Observe(1000, 5.0f, 0.0f, (int16_t)-1);
	float vx = 0.0f;
	float vz = 0.0f;
	t.Velocity(5000, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 5.0f) < 1e-3f);
	t.Velocity(5001, 4000, 400, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);

	// The follower adds observation age to the lead but never extrapolates past maxExtrapSec.
	const int n = 40;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	BotCore::NavPathfinder pf;
	BotCore::NavFollowParams params;
	BotCore::NavFollower f;
	f.ObserveTarget(0, 100.0f, 100.0f, (int16_t)-1);
	f.ObserveTarget(1000, 105.0f, 100.0f, (int16_t)-1);
	CHECK(f.Update(grid, pf, 4000, 60.0f, 100.0f, 8.0f, params));
	CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(f.Plan().leadSec <= params.maxLeadSec + 1e-4f);   // lead is capped at maxLeadSec (1.5 s)
	CHECK(std::fabs(f.Plan().predX - 105.0f) <= 5.0f * 3.0f + 1e-3f);
	CHECK(grid.Walk(grid.CellOf(f.Plan().predX), grid.CellOf(f.Plan().predZ)));
}

TEST_CASE("NavTrack_VelocityRobust_StopStart")
{
	BotCore::NavTargetTracker t;
	CHECK(t.Observe(0, 0.0f, 0.0f, (int16_t)45));
	CHECK(t.Observe(1500, 6.75f, 0.0f, (int16_t)45));
	CHECK(t.Observe(3000, 13.5f, 0.0f, (int16_t)45));
	CHECK(t.Observe(4500, 20.25f, 0.0f, (int16_t)0));   // stopped (speed field 0)

	float vx = 0.0f;
	float vz = 0.0f;
	for (int64_t now = 4500; now < 9500; now += 100)
	{
		t.Velocity(now, 4000, 400, vx, vz);
		CHECK_EQ(vx, 0.0f);
		CHECK_EQ(vz, 0.0f);
	}

	// Motion resumes at 9500: the first moving sample is alone in the window (age relationship),
	// the second pairs against it only and never mixes the pre-stop samples.
	CHECK(t.Observe(9500, 27.0f, 0.0f, (int16_t)45));
	t.Velocity(9500, 4000, 400, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);
	CHECK(t.Observe(11000, 33.75f, 0.0f, (int16_t)45));
	t.Velocity(11000, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 4.5f) < 1e-3f);
	CHECK(std::fabs(vz) < 1e-3f);
}

TEST_CASE("NavTrack_VelocityRobust_Reverse180")
{
	BotCore::NavTargetTracker t;
	t.Observe(0, 0.0f, 0.0f, (int16_t)45);
	t.Observe(1500, 6.75f, 0.0f, (int16_t)45);
	t.Observe(3000, 13.5f, 0.0f, (int16_t)45);

	float vx = 0.0f;
	float vz = 0.0f;
	t.Velocity(3000, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 4.5f) < 1e-3f);

	t.Observe(4500, 6.75f, 0.0f, (int16_t)45);   // target turned 180 degrees
	t.Velocity(4500, 4000, 400, vx, vz);
	CHECK(std::fabs(vx + 4.5f) < 1e-3f);   // already reversed at the first post-turn packet

	t.Observe(6000, 0.0f, 0.0f, (int16_t)45);
	t.Velocity(6000, 4000, 400, vx, vz);
	const float mag = std::sqrt(vx * vx + vz * vz);
	CHECK(std::fabs(mag - 4.5f) <= 0.25f * 4.5f);
	CHECK(std::fabs(vx + 4.5f) < 1e-3f);   // direction error 0 deg (<= 15)

	// Follower: after the reverse the prediction point stays walkable (back-off keeps it on the grid).
	const int n = 40;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	BotCore::NavPathfinder pf;
	BotCore::NavFollowParams params;
	BotCore::NavFollower f;
	const float xs[6] = { 100.0f, 106.75f, 113.5f, 106.75f, 100.0f, 93.25f };
	for (int k = 0; k < 6; ++k)
		f.ObserveTarget((int64_t)k * 1500, xs[k], 100.0f, (int16_t)45);
	CHECK(f.Update(grid, pf, 7500, 60.0f, 100.0f, 8.0f, params));
	CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(grid.Walk(grid.CellOf(f.Plan().predX), grid.CellOf(f.Plan().predZ)));

	// F5-56 Tur 2: a turn that falls between two observations (overshoot to 16.875 at 3750, then
	// back: packets at 4500 = 13.5, 6000 = 6.75). The transition sample must not report +x over
	// the clamp; the next packet recovers the reversed velocity.
	BotCore::NavTargetTracker o;
	o.Observe(0, 0.0f, 0.0f, (int16_t)45);
	o.Observe(1500, 6.75f, 0.0f, (int16_t)45);
	o.Observe(3000, 13.5f, 0.0f, (int16_t)45);
	o.Observe(4500, 13.5f, 0.0f, (int16_t)45);   // turn at 3750, overshoot returns to 13.5
	o.Velocity(4500, 4000, 400, vx, vz);
	CHECK(vx <= 1e-3f);                            // direction -x or 0, never +x
	CHECK(std::sqrt(vx * vx + vz * vz) <= 4.95f);  // never above the walk clamp
	o.Observe(6000, 6.75f, 0.0f, (int16_t)45);
	o.Velocity(6000, 4000, 400, vx, vz);
	CHECK(std::fabs(vx + 4.5f) < 1e-3f);
	CHECK(std::fabs(vz) < 1e-3f);
}

TEST_CASE("NavTrack_VelocityRobust_SpeedChange")
{
	BotCore::NavTargetTracker t;
	t.Observe(0, 0.0f, 0.0f, (int16_t)45);
	t.Observe(1500, 6.75f, 0.0f, (int16_t)45);
	t.Observe(3000, 13.5f, 0.0f, (int16_t)45);

	float vx = 0.0f;
	float vz = 0.0f;
	t.Velocity(3000, 4000, 400, vx, vz);
	CHECK(std::fabs(std::sqrt(vx * vx + vz * vz) - 4.5f) <= 0.10f * 4.5f);

	t.Observe(4500, 23.55f, 0.0f, (int16_t)67);   // sprint 6.7 m/s
	t.Observe(6000, 33.6f, 0.0f, (int16_t)67);
	t.Velocity(6000, 4000, 400, vx, vz);
	CHECK(std::fabs(std::sqrt(vx * vx + vz * vz) - 6.7f) <= 0.10f * 6.7f);

	t.Observe(7500, 40.35f, 0.0f, (int16_t)45);   // walk again
	t.Velocity(7500, 4000, 400, vx, vz);
	CHECK(std::fabs(std::sqrt(vx * vx + vz * vz) - 4.5f) <= 0.10f * 4.5f);

	// Unknown speed (-1): the estimate comes from positions only.
	BotCore::NavTargetTracker u;
	u.Observe(0, 0.0f, 0.0f, (int16_t)-1);
	u.Observe(1500, 6.75f, 0.0f, (int16_t)-1);
	u.Observe(3000, 13.5f, 0.0f, (int16_t)-1);
	u.Velocity(3000, 4000, 400, vx, vz);
	CHECK(std::fabs(std::sqrt(vx * vx + vz * vz) - 4.5f) <= 0.10f * 4.5f);

	// F5-56 Tur 2: the clamp follows the newest packet's speed field. A noisy walk packet
	// (speed 45, implies 5.27 m/s) clamps to 4.95, never to the sprint 7.37; the same noise on a
	// sprint packet (speed 67, implies 7.47 m/s) clamps to 7.37.
	BotCore::NavTargetTracker w;
	w.Observe(6000, 33.6f, 0.0f, (int16_t)45);
	w.Observe(7500, 41.5f, 0.0f, (int16_t)45);   // true 40.35, noise +1.15 -> 5.27 m/s
	w.Velocity(7500, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 4.95f) < 1e-3f);

	BotCore::NavTargetTracker sp;
	sp.Observe(6000, 33.6f, 0.0f, (int16_t)67);
	sp.Observe(7500, 44.8f, 0.0f, (int16_t)67);  // true 43.65, same +1.15 -> 7.47 m/s
	sp.Velocity(7500, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 7.37f) < 1e-3f);

	// Unknown speed: a pair implying 6.7 m/s is valid (under the 10 m/s teleport cap).
	BotCore::NavTargetTracker u67;
	u67.Observe(0, 0.0f, 0.0f, (int16_t)-1);
	u67.Observe(1500, 10.05f, 0.0f, (int16_t)-1);
	u67.Velocity(1500, 4000, 400, vx, vz);
	CHECK(std::fabs(std::sqrt(vx * vx + vz * vz) - 6.7f) < 1e-3f);
}

TEST_CASE("NavTrack_VelocityRobust_Jump")
{
	// A single >= 50 m step (respawn / summon) with an unknown speed field: the estimator must
	// never report the jump magnitude and the first observation after it reads 0.
	BotCore::NavTargetTracker t;
	CHECK(t.Observe(1000, 0.0f, 0.0f, (int16_t)-1));
	CHECK(t.Observe(2500, 6.75f, 0.0f, (int16_t)-1));
	CHECK(t.Observe(4000, 13.5f, 0.0f, (int16_t)-1));
	CHECK(t.Observe(5500, 63.5f, 0.0f, (int16_t)-1));   // +50 m

	float vx = 0.0f;
	float vz = 0.0f;
	t.Velocity(5500, 4000, 400, vx, vz);
	CHECK(std::sqrt(vx * vx + vz * vz) < 1e-3f);

	for (int64_t now = 5500; now <= 7000; now += 100)
	{
		float ax = 0.0f;
		float az = 0.0f;
		t.Velocity(now, 4000, 400, ax, az);
		CHECK(std::sqrt(ax * ax + az * az) < 30.0f);
	}

	CHECK(t.Observe(7000, 70.25f, 0.0f, (int16_t)-1));
	t.Velocity(7000, 4000, 400, vx, vz);
	CHECK(std::fabs(std::sqrt(vx * vx + vz * vz) - 4.5f) <= 0.10f * 4.5f);

	// A moderate 20 m/s step with a known speed field is still the existing clamp case, not a jump.
	BotCore::NavTargetTracker c;
	c.Observe(0, 0.0f, 0.0f, (int16_t)45);
	c.Observe(500, 10.0f, 0.0f, (int16_t)45);
	c.Velocity(500, 4000, 400, vx, vz);
	CHECK(std::fabs(vx - 4.95f) < 1e-3f);

	// F5-56 Tur 2: the guard is applied to the selected pair, so a packet bunched right after
	// the jump cannot pair with a sample from before it and reproduce the jump magnitude.
	BotCore::NavTargetTracker j;
	j.Observe(0, 0.0f, 0.0f, (int16_t)-1);
	j.Observe(1500, 6.75f, 0.0f, (int16_t)-1);
	j.Observe(3000, 13.5f, 0.0f, (int16_t)-1);
	j.Observe(4500, 63.5f, 0.0f, (int16_t)-1);    // +50 m jump
	j.Observe(4505, 63.6f, 0.0f, (int16_t)-1);    // bunched packet right after the jump
	j.Velocity(4505, 4000, 400, vx, vz);
	CHECK(std::sqrt(vx * vx + vz * vz) < 1e-3f);   // exactly zero, not merely < 30

	// Unknown speed: a +53 m jump over 2500 ms (21.3 m/s) is over the 10 m/s cap -> 0.
	BotCore::NavTargetTracker k;
	k.Observe(0, 0.0f, 0.0f, (int16_t)-1);
	k.Observe(1500, 6.75f, 0.0f, (int16_t)-1);
	k.Observe(4000, 59.75f, 0.0f, (int16_t)-1);   // +53 m over 2500 ms
	k.Velocity(4000, 4000, 400, vx, vz);
	CHECK(std::sqrt(vx * vx + vz * vz) < 1e-3f);

	// Unknown-speed boundary: a pair implying 9 m/s stays valid, 11 m/s is rejected.
	BotCore::NavTargetTracker v9;
	v9.Observe(0, 0.0f, 0.0f, (int16_t)-1);
	v9.Observe(1000, 9.0f, 0.0f, (int16_t)-1);
	v9.Velocity(1000, 4000, 400, vx, vz);
	CHECK(std::fabs(std::sqrt(vx * vx + vz * vz) - 9.0f) < 1e-3f);

	BotCore::NavTargetTracker v11;
	v11.Observe(0, 0.0f, 0.0f, (int16_t)-1);
	v11.Observe(1000, 11.0f, 0.0f, (int16_t)-1);
	v11.Velocity(1000, 4000, 400, vx, vz);
	CHECK(std::sqrt(vx * vx + vz * vz) < 1e-3f);

	// After the jump, two clean observations restore the 4.5 m/s estimate.
	j.Observe(6000, 70.35f, 0.0f, (int16_t)-1);
	j.Observe(7500, 77.1f, 0.0f, (int16_t)-1);
	j.Velocity(7500, 4000, 400, vx, vz);
	CHECK(std::fabs(std::sqrt(vx * vx + vz * vz) - 4.5f) <= 0.10f * 4.5f);
}

TEST_CASE("NavTrack_VelocityRobust_Quantization")
{
	// Positions are rounded to 0.1 m on both axes; the estimate error is bounded by the
	// quantization (0.1 m * sqrt(2) / span) plus the [A] 5% slack. The intervals are not aligned
	// to the 0.1 m grid and t0 keeps shifting so the rounding error is really exercised; at
	// least one interval must show worst_abs > 0.05, proving the case is not vacuous.
	const int intervals[7] = { 400, 413, 577, 700, 911, 1237, 1500 };
	const double vxTrue = 3.2;
	const double vzTrue = 3.1;
	double bestWorst = 0.0;
	for (int ii = 0; ii < 7; ++ii)
	{
		const int interval = intervals[ii];
		double worst = 0.0;
		for (int it = 0; it < 200; ++it)
		{
			const int64_t t0 = 100000 + (int64_t)it * 7;
			BotCore::NavTargetTracker t;
			t.Observe(t0, Quant1(vxTrue * (double)t0 / 1000.0), Quant1(vzTrue * (double)t0 / 1000.0),
				(int16_t)-1);
			t.Observe(t0 + interval, Quant1(vxTrue * (double)(t0 + interval) / 1000.0),
				Quant1(vzTrue * (double)(t0 + interval) / 1000.0), (int16_t)-1);
			float vx = 0.0f;
			float vz = 0.0f;
			t.Velocity(t0 + interval, 4000, 400, vx, vz);
			const double ex = (double)vx - vxTrue;
			const double ez = (double)vz - vzTrue;
			const double err = std::sqrt(ex * ex + ez * ez);
			if (err > worst)
				worst = err;
		}
		const double bound = 0.1 * std::sqrt(2.0) / ((double)interval / 1000.0) + 0.05;
		std::printf("NAVTRACK quant interval=%d worst_abs=%.3f bound=%.3f\n", interval, worst, bound);
		CHECK(worst <= bound);
		if (worst > bestWorst)
			bestWorst = worst;
	}
	CHECK(bestWorst > 0.05);   // the case produces a real quantization error

	// Below minVelocitySpanMs no estimate is produced.
	BotCore::NavTargetTracker s;
	s.Observe(100, 0.0f, 0.0f, (int16_t)45);
	s.Observe(200, 0.9f, 0.0f, (int16_t)45);
	float vx = 0.0f;
	float vz = 0.0f;
	s.Velocity(200, 4000, 400, vx, vz);
	CHECK_EQ(vx, 0.0f);
	CHECK_EQ(vz, 0.0f);
}

TEST_CASE("NavTrack_Chase_Sim_Cadence")
{
	// Same arena chase as NavTrack_Chase_Sim, but the target is only seen at the real WIZ_MOVE
	// cadence (1500 ms, +-150 ms arrival jitter, 10% packet loss). The sparse observation must
	// not slow the catch by more than 30% against perfect per-tick observation.
	const int n = 80;
	const float unit = 4.0f;
	BotCore::NavGrid grid = MakeNav(n, unit, RingEvents(n), HeightZeros(n));
	BotCore::NavPathfinder pf;

	const long long perfect = RunChase(grid, pf, 20261002u, true, 0, 0.0, 0.0);
	long long worstCadence = 0;
	for (int seed = 1; seed <= 5; ++seed)
	{
		const long long c = RunChase(grid, pf, (uint64_t)(seed * 7919u), false, 1500, 150.0, 0.10);
		if (c > worstCadence)
			worstCadence = c;
	}

	std::printf("NAVTRACK chase_cadence caught_ms_perfect=%lld caught_ms_cadence=%lld\n",
		perfect, worstCadence);
	REQUIRE(perfect > 0);
	REQUIRE(worstCadence > 0);
	CHECK((double)worstCadence <= 1.3 * (double)perfect);
}

namespace
{
	// n = 40, unit 4 m, blocked border; a 4x4 plateau (x 20..23, z 18..21) raised 20 m: its cells are
	// Walk but every edge to the ground is closed by the slope limit, so it is an edge-isolated pocket.
	BotCore::NavGrid MakePocketGrid()
	{
		const int n = 40;
		std::vector<float> heights = HeightZeros(n);
		for (int x = 20; x <= 23; ++x)
		{
			for (int z = 18; z <= 21; ++z)
				heights[CellIndex(n, x, z)] = 20.0f;
		}
		return MakeNav(n, 4.0f, RingEvents(n), heights);
	}
}

TEST_CASE("NavTrack_Reach_LeadInPocket")
{
	BotCore::NavGrid grid = MakePocketGrid();
	BotCore::NavReach reach;
	reach.Build(grid);
	REQUIRE(reach.ComponentCount() == 2);
	REQUIRE(grid.Walk(20, 19));
	REQUIRE(reach.ComponentOf(20, 19) != reach.ComponentOf(8, 19));

	BotCore::NavPathfinder pf;
	BotCore::NavFollowParams params;
	params.ringMinM = 0.0f;
	params.ringMaxM = 3.0f;
	const float botX = grid.CellCenter(8);
	const float botZ = grid.CellCenter(19);

	// Without labels the 1.5 s lead point (83.6, 78) lands on the plateau; every ring cell is in the pocket.
	BotCore::NavFollower oldF;
	oldF.ObserveTarget(0, 70.0f, 78.0f);
	oldF.ObserveTarget(500, 73.4f, 78.0f);
	CHECK(oldF.Update(grid, pf, 500, botX, botZ, 4.5f, params));
	CHECK(oldF.Plan().status == BotCore::NavFollowStatus::PathFailed);
	CHECK(oldF.Plan().pathStatus == BotCore::NavPathStatus::NoPath);
	CHECK(oldF.Plan().leadSec == 1.5f);
	CHECK(oldF.Plan().tries == 2);

	// With labels the lead is halved until its point is back in the bot's component (0.75 s -> (78.5, 78)).
	BotCore::NavFollower f;
	f.ObserveTarget(0, 70.0f, 78.0f);
	f.ObserveTarget(500, 73.4f, 78.0f);
	CHECK(f.UpdateReachable(grid, pf, 500, botX, botZ, 4.5f, params, reach));
	CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(std::fabs(f.Plan().leadSec - 0.75f) < 1e-4f);
	CHECK(f.Plan().goal == Cell(19, 19));
	CHECK_EQ(f.Plan().tries, 1);
	CHECK(reach.ComponentOf(f.Plan().goal.x, f.Plan().goal.z) == reach.ComponentOf(8, 19));
	CHECK(!f.Plan().smooth.waypoints.empty());
	CHECK(SegmentsClear(grid, f.Plan().smooth.waypoints));
}

TEST_CASE("NavTrack_Reach_SkipPocketCandidates")
{
	BotCore::NavGrid grid = MakePocketGrid();
	BotCore::NavReach reach;
	reach.Build(grid);

	BotCore::NavPathfinder pf;
	BotCore::NavFollowParams params;
	params.ringMinM = 0.0f;
	params.ringMaxM = 6.0f;
	// Target on the pocket's east edge: the ring holds 4 plateau cells (nearest to the bot) and 2
	// main-component cells (24, 19) and (24, 20) behind it.
	const float botX = grid.CellCenter(10);
	const float botZ = grid.CellCenter(19);

	BotCore::NavFollower oldF;
	oldF.ObserveTarget(0, 94.0f, 80.0f);
	CHECK(oldF.Update(grid, pf, 0, botX, botZ, 4.5f, params));
	CHECK(oldF.Plan().status == BotCore::NavFollowStatus::PathFailed);
	CHECK_EQ(oldF.Plan().tries, 3);

	BotCore::NavFollower f;
	f.ObserveTarget(0, 94.0f, 80.0f);
	CHECK(f.UpdateReachable(grid, pf, 0, botX, botZ, 4.5f, params, reach));
	CHECK(f.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(f.Plan().goal == Cell(24, 19));
	CHECK_EQ(f.Plan().tries, 1);   // the four plateau cells were skipped, not tried
	CHECK(f.Plan().expanded > 0);
	CHECK(SegmentsClear(grid, f.Plan().smooth.waypoints));

	// maxTries counts A* runs only: with ringMaxTries = 1 the plan still reaches the first main-component cell.
	BotCore::NavFollowParams one = params;
	one.ringMaxTries = 1;
	BotCore::NavFollower g;
	g.ObserveTarget(0, 94.0f, 80.0f);
	CHECK(g.UpdateReachable(grid, pf, 0, botX, botZ, 4.5f, one, reach));
	CHECK(g.Plan().status == BotCore::NavFollowStatus::Planned);
	CHECK(g.Plan().goal == Cell(24, 19));
	CHECK_EQ(g.Plan().tries, 1);
}

TEST_CASE("NavTrack_Reach_ProvablyUnreachable")
{
	BotCore::NavGrid grid = MakePocketGrid();
	BotCore::NavReach reach;
	reach.Build(grid);
	BotCore::NavPathfinder pf;
	BotCore::NavFollowParams params;
	params.ringMinM = 0.0f;
	params.ringMaxM = 3.0f;

	// (a) Target in the pocket, bot outside: every ring cell is skipped, no A* runs at all.
	{
		const float botX = grid.CellCenter(10);
		const float botZ = grid.CellCenter(19);
		BotCore::NavFollower oldF;
		oldF.ObserveTarget(0, 88.0f, 80.0f);
		CHECK(oldF.Update(grid, pf, 0, botX, botZ, 4.5f, params));
		CHECK(oldF.Plan().status == BotCore::NavFollowStatus::PathFailed);
		CHECK_EQ(oldF.Plan().tries, 3);
		CHECK(oldF.Plan().expanded > 0);

		BotCore::NavFollower f;
		f.ObserveTarget(0, 88.0f, 80.0f);
		CHECK(f.UpdateReachable(grid, pf, 0, botX, botZ, 4.5f, params, reach));
		CHECK(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
		CHECK(f.Plan().pathStatus == BotCore::NavPathStatus::NoPath);
		CHECK_EQ(f.Plan().tries, 0);
		CHECK_EQ(f.Plan().expanded, 0);
		CHECK(f.Plan().smooth.waypoints.empty());

		// The F5-05 judgement of that plan: provably unreachable by component.
		BotCore::NavReachJudge judge;
		const BotCore::NavReachJudgement j = judge.Judge(grid, reach, f.Plan(), params, BotCore::NavUnreachParams(), botX, botZ);
		CHECK(j.verdict == BotCore::NavReachVerdict::Unreachable);
		CHECK(j.reason == BotCore::NavUnreachReason::Component);
	}

	// (b) Bot inside the pocket, target outside: the planner cannot leave the pocket (recorded
	// limitation, KI: bot stranded in a pocket; not fixed by F5-71).
	{
		const float botX = grid.CellCenter(21);
		const float botZ = grid.CellCenter(19);
		BotCore::NavFollower f;
		f.ObserveTarget(0, 50.0f, 78.0f);
		CHECK(f.UpdateReachable(grid, pf, 0, botX, botZ, 4.5f, params, reach));
		CHECK(f.Plan().status == BotCore::NavFollowStatus::PathFailed);
		CHECK_EQ(f.Plan().tries, 0);
		CHECK_EQ(f.Plan().expanded, 0);
	}

	// (c) Bot cell not Walk (border cell): no labels apply, identical to Update (InvalidStart).
	{
		BotCore::NavFollower a;
		BotCore::NavFollower b;
		a.ObserveTarget(0, 50.0f, 78.0f);
		b.ObserveTarget(0, 50.0f, 78.0f);
		CHECK(a.Update(grid, pf, 0, 2.0f, 2.0f, 4.5f, params));
		CHECK(b.UpdateReachable(grid, pf, 0, 2.0f, 2.0f, 4.5f, params, reach));
		CHECK(a.Plan().status == BotCore::NavFollowStatus::InvalidStart);
		CHECK(b.Plan().status == BotCore::NavFollowStatus::InvalidStart);
		CHECK_EQ(a.Plan().tries, b.Plan().tries);
	}
}

TEST_CASE("NavTrack_Reach_SingleComponent_Identical")
{
	// One edge-connected component (a wall with a gap): UpdateReachable must equal Update bit for bit.
	const int n = 40;
	std::vector<int16_t> events = RingEvents(n);
	for (int z = 1; z <= n - 2; ++z)
	{
		if (z < 18 || z > 21)
			events[CellIndex(n, 20, z)] = 0;
	}
	BotCore::NavGrid grid = MakeNav(n, 4.0f, events, HeightZeros(n));
	BotCore::NavReach reach;
	reach.Build(grid);
	REQUIRE(reach.ComponentCount() == 1);

	BotCore::NavPathfinder pf;
	BotCore::Rng rng(20261004u);
	std::vector<BotCore::NavCell> walk;
	for (int x = 0; x < n; ++x)
		for (int z = 0; z < n; ++z)
			if (grid.Walk(x, z))
				walk.push_back(Cell(x, z));
	REQUIRE(!walk.empty());

	int planned = 0;
	for (int i = 0; i < 300; ++i)
	{
		const BotCore::NavCell bc = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		const BotCore::NavCell tc = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		const float vx = (float)(rng.NextDouble() * 8.0 - 4.0);
		const float vz = (float)(rng.NextDouble() * 8.0 - 4.0);
		BotCore::NavFollowParams params;
		params.ringMinM = (float)(rng.NextBelow(3u));
		params.ringMaxM = params.ringMinM + (float)(rng.NextBelow(8u));
		params.ringMaxTries = 1 + (int)rng.NextBelow(4u);
		const float tx = grid.CellCenter(tc.x);
		const float tz = grid.CellCenter(tc.z);

		BotCore::NavFollower a;
		BotCore::NavFollower b;
		a.ObserveTarget(0, tx, tz);
		b.ObserveTarget(0, tx, tz);
		a.ObserveTarget(500, tx + vx * 0.5f, tz + vz * 0.5f);
		b.ObserveTarget(500, tx + vx * 0.5f, tz + vz * 0.5f);
		const float bx = grid.CellCenter(bc.x);
		const float bz = grid.CellCenter(bc.z);
		const bool ra = a.Update(grid, pf, 500, bx, bz, 4.5f, params);
		const bool rb = b.UpdateReachable(grid, pf, 500, bx, bz, 4.5f, params, reach);
		const BotCore::NavFollowPlan & pa = a.Plan();
		const BotCore::NavFollowPlan & pb = b.Plan();
		const bool same = ra == rb && pa.status == pb.status && pa.pathStatus == pb.pathStatus
			&& pa.goal == pb.goal && pa.tries == pb.tries && pa.expanded == pb.expanded
			&& pa.pathCost == pb.pathCost && pa.predX == pb.predX && pa.predZ == pb.predZ
			&& pa.leadSec == pb.leadSec && pa.smooth.waypoints.size() == pb.smooth.waypoints.size()
			&& pa.smooth.length == pb.smooth.length;
		CHECK(same);
		if (pa.status == BotCore::NavFollowStatus::Planned)
			++planned;
	}
	CHECK(planned > 150);
}
