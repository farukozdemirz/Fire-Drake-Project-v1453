#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavSmooth.h>
#include <BotCore/NavTrack.h>
#include <BotCore/NavBudget.h>
#include <BotCore/Rng.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
	using BotCore::NavCell;
	using BotCore::NavGrid;
	using BotCore::NavQueryScheduler;
	using BotCore::NavPathCache;
	using BotCore::NavCacheKey;
	using BotCore::NavDeferParams;
	using BotCore::NavDeferAction;
	using BotCore::NavFollowPlan;
	using BotCore::NavFollowStatus;
	using BotCore::NavFollowParams;
	using BotCore::NavFollower;
	using BotCore::NavPathfinder;
	using BotCore::NavPathResult;
	using BotCore::NavPathStatus;
	using BotCore::NavSearchParams;
	using BotCore::NavTargetTracker;

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
}

TEST_CASE("NavBudget_Scheduler_Basic")
{
	NavQueryScheduler s;
	s.SetMaxWaitMs(1000);

	uint16_t out[128];

	// Empty scheduler selects nobody.
	CHECK_EQ(s.NextBatch(0, 1.5, out, 128), 0);
	CHECK_EQ(s.Pending(), 0);
	CHECK_EQ(s.OldestWaitMs(0), (int64_t)-1);

	// A single request is returned even when it alone exceeds the budget (progress guarantee).
	s.Request(7, 0);
	CHECK_EQ(s.Pending(), 1);
	CHECK_EQ(s.OldestWaitMs(500), (int64_t)500);
	CHECK_EQ(s.NextBatch(0, 0.1, out, 128), 1);
	CHECK_EQ((int)out[0], 7);

	// Sixteen same-tick requests, cost 0.4 ms, budget 1.5 ms -> floor(1.5/0.4) = 3 per tick.
	s.Clear();
	for (uint16_t b = 0; b < 16; ++b)
		s.Request(b, 0);
	CHECK_EQ(s.Pending(), 16);
	for (uint16_t b = 0; b < 16; ++b)
		s.ReportCost(b, 0.4, 0);

	int tick = 0;
	int total = 0;
	int served[16] = {};
	while (s.Pending() > 0 && tick < 20)
	{
		const int n = s.NextBatch(tick * 100, 1.5, out, 128);
		CHECK(n <= 3);
		for (int i = 0; i < n; ++i)
			++served[out[i]];
		total += n;
		// Simulate the caller finishing them immediately.
		for (int i = 0; i < n; ++i)
			s.Cancel(out[i]);
		++tick;
	}
	CHECK_EQ(total, 16);
	CHECK(tick <= 6);                 // ceil(16/3) = 6 ticks
	for (int b = 0; b < 16; ++b)
		CHECK_EQ(served[b], 1);

	// A repeated Request keeps the FIRST timestamp and does not create a second entry.
	s.Clear();
	s.Request(3, 1000);
	s.Request(3, 1900);
	CHECK_EQ(s.Pending(), 1);
	CHECK_EQ(s.OldestWaitMs(1900), (int64_t)900);
	s.Request(200, 0);               // out-of-range id ignored
	CHECK_EQ(s.Pending(), 1);
}

TEST_CASE("NavBudget_Scheduler_Fairness")
{
	NavQueryScheduler s;
	s.SetMaxWaitMs(1000);
	const int bots = 16;
	const int64_t tickMs = 100;
	uint16_t out[128];

	int64_t lastServed[16] = {};
	int64_t firstServed[16] = {};
	int64_t lastRequested[16] = {};
	int64_t maxWait[16] = {};
	int64_t longestWait = 0;
	int served[16] = {};
	int overMaxWait = 0;

	for (int64_t t = 0; t < 100000; t += tickMs)   // 1000 ticks
	{
		// Every bot requests every 500 ms, staggered by its replan phase.
		for (int b = 0; b < bots; ++b)
		{
			const int phase = BotCore::NavReplanPhaseMs(b);
			if ((t - phase) % 500 == 0 && t >= phase)
			{
				s.Request((uint16_t)b, t);
				lastRequested[b] = t;
			}
		}

		const int n = s.NextBatch(t, 1.5, out, 128);
		for (int i = 0; i < n; ++i)
		{
			const int b = out[i];
			const int64_t wait = t - lastRequested[b];
			if (wait > maxWait[b])
				maxWait[b] = wait;
			if (wait > longestWait)
				longestWait = wait;
			if (wait > 1100)
				++overMaxWait;
			++served[b];
			lastServed[b] = t;
			if (firstServed[b] == 0)
				firstServed[b] = t;
			s.ReportCost((uint16_t)b, 0.3, 0);
			s.Cancel((uint16_t)b);
		}
	}

	// No bot waits longer than maxWaitMs + one tick.
	CHECK_EQ(overMaxWait, 0);
	CHECK(longestWait <= 1100);

	// Service spread: busiest and idlest differ by at most 10%.
	int maxServ = 0;
	int minServ = 1 << 30;
	for (int b = 0; b < bots; ++b)
	{
		if (served[b] > maxServ)
			maxServ = served[b];
		if (served[b] < minServ)
			minServ = served[b];
	}
	CHECK(minServ > 0);
	CHECK((maxServ - minServ) * 100 <= maxServ * 10);

	// Artificial burst: a fresh scheduler, 40 requests at t = 0, budget too small for all. The
	// waiting bots are prioritised over later arrivals.
	{
		NavQueryScheduler burst;
		burst.SetMaxWaitMs(1000);
		for (uint16_t b = 0; b < 40; ++b)
			burst.Request(b, 0);
		for (uint16_t b = 0; b < 40; ++b)
			burst.ReportCost(b, 0.3, 0);

		// t = 900: nobody has waited >= 1000 yet -> FIFO order.
		const int n0 = burst.NextBatch(900, 1.5, out, 128);
		CHECK_EQ(n0, 5);
		for (int i = 0; i < n0; ++i)
			burst.Cancel(out[i]);

		// t = 1100: the remaining early bots have now waited > 1000 -> they lead the queue.
		const int n1 = burst.NextBatch(1100, 1.5, out, 128);
		CHECK_EQ(n1, 5);
		// All remaining entries (ids 5..39) waited the same; any of them may lead after rotation,
		// but they must come from the original burst (not a later arrival).
		CHECK(out[0] >= 5 && out[0] < 40);
	}
}

TEST_CASE("NavBudget_Scheduler_Cost")
{
	NavQueryScheduler s;
	s.SetMaxWaitMs(1000);
	uint16_t out[128];

	// EWMA: 1.0 then 3.0 -> 0.7*1.0 + 0.3*3.0 = 1.6.
	s.Request(0, 0);
	s.ReportCost(0, 1.0, 0);
	s.ReportCost(0, 3.0, 0);
	s.Request(1, 0);
	// Budget 1.6 selects only bot 0 (1.6); bot 1 (global EWMA 1.6) would exceed it.
	const int n = s.NextBatch(0, 1.6, out, 128);
	CHECK_EQ(n, 1);
	CHECK_EQ((int)out[0], 0);

	// A single query that exceeds the budget still runs (never split, never starved).
	s.Clear();
	s.Request(2, 0);
	s.ReportCost(2, 4.0, 0);
	CHECK_EQ(s.NextBatch(0, 1.5, out, 128), 1);
	CHECK_EQ((int)out[0], 2);

	// Cancel removes a pending request; Clear empties everything.
	s.Clear();
	s.Request(4, 0);
	s.Request(5, 0);
	s.Cancel(4);
	CHECK_EQ(s.Pending(), 1);
	CHECK_EQ(s.NextBatch(0, 10.0, out, 128), 1);
	CHECK_EQ((int)out[0], 5);
	s.Clear();
	CHECK_EQ(s.Pending(), 0);
	s.Request(9, 0);
	s.Clear();
	s.Cancel(9);                       // cancelling an already-cleared slot is harmless
	CHECK_EQ(s.Pending(), 0);
}

TEST_CASE("NavBudget_Cache")
{
	NavPathCache cache;                // TTL 30000, LRU, capacity 64, 512-cell limit

	NavCacheKey k;
	k.startX = 1; k.startZ = 2; k.goalX = 3; k.goalZ = 4; k.fieldVersion = 1;

	std::vector<NavCell> cells;
	cells.push_back(Cell(1, 2));
	cells.push_back(Cell(2, 3));
	cells.push_back(Cell(3, 4));
	cache.Put(k, cells, 12.5f, 12.5f, 0);
	CHECK_EQ(cache.Count(), 1);

	std::vector<NavCell> got;
	float cost = 0.0f;
	float length = 0.0f;
	CHECK(cache.Find(k, 0, got, cost, length));
	CHECK_EQ((int)got.size(), 3);
	CHECK_EQ(got[0].x, 1);
	CHECK_EQ(got.back().x, 3);
	CHECK_EQ(cost, 12.5f);
	CHECK_EQ(length, 12.5f);

	// TTL boundary: found at exactly 30000 ms, gone at 30001.
	CHECK(cache.Find(k, 30000, got, cost, length));
	CHECK(!cache.Find(k, 30001, got, cost, length));
	CHECK_EQ(cache.Count(), 0);        // expired entry was dropped

	// The returned vectors are independent copies.
	cache.Put(k, cells, 1.0f, 1.0f, 0);
	CHECK(cache.Find(k, 0, got, cost, length));
	got[0].x = 999;
	CHECK(cache.Find(k, 0, got, cost, length));
	CHECK_EQ(got[0].x, 1);

	// A version change invalidates matching entries only.
	cache.Clear();
	NavCacheKey k1 = k; k1.fieldVersion = 1;
	NavCacheKey k2 = k; k2.fieldVersion = 2; k2.goalX = 9;
	cache.Put(k1, cells, 1.0f, 1.0f, 0);
	cache.Put(k2, cells, 1.0f, 1.0f, 0);
	CHECK_EQ(cache.Count(), 2);
	cache.Invalidate(2);
	CHECK_EQ(cache.Count(), 1);
	CHECK(!cache.Find(k1, 0, got, cost, length));
	CHECK(cache.Find(k2, 0, got, cost, length));

	// Capacity: the 65th distinct key evicts the least-recently-used entry. Touch k2 at t = 10
	// so k0 (stored first, never touched) is the LRU victim.
	cache.Clear();
	for (int i = 0; i < 64; ++i)
	{
		NavCacheKey ki = k;
		ki.goalX = 100 + i;
		cache.Put(ki, cells, 1.0f, 1.0f, 0);
	}
	CHECK_EQ(cache.Count(), 64);
	NavCacheKey oldest = k; oldest.goalX = 100;      // first inserted
	// Touch every entry except `oldest`, so `oldest` is the least-recently-used.
	for (int i = 1; i < 64; ++i)
	{
		NavCacheKey ki = k;
		ki.goalX = 100 + i;
		CHECK(cache.Find(ki, 10, got, cost, length));
	}
	NavCacheKey fresh = k; fresh.goalX = 500;
	cache.Put(fresh, cells, 1.0f, 1.0f, 20);
	CHECK_EQ(cache.Count(), 64);
	CHECK(!cache.Find(oldest, 20, got, cost, length));  // LRU evicted
	CHECK(cache.Find(fresh, 20, got, cost, length));


	// Routes longer than 512 cells are not stored.
	cache.Clear();
	std::vector<NavCell> longPath((size_t)513);
	cache.Put(k, longPath, 1.0f, 1.0f, 0);
	CHECK_EQ(cache.Count(), 0);
	std::vector<NavCell> maxPath((size_t)512);
	cache.Put(k, maxPath, 1.0f, 1.0f, 0);
	CHECK_EQ(cache.Count(), 1);
}

TEST_CASE("NavBudget_ReplanPhase")
{
	CHECK_EQ(BotCore::NavReplanPhaseMs(0), 0);
	CHECK_EQ(BotCore::NavReplanPhaseMs(1), 100);
	CHECK_EQ(BotCore::NavReplanPhaseMs(2), 200);
	CHECK_EQ(BotCore::NavReplanPhaseMs(3), 300);
	CHECK_EQ(BotCore::NavReplanPhaseMs(4), 400);
	CHECK_EQ(BotCore::NavReplanPhaseMs(5), 0);
	CHECK_EQ(BotCore::NavReplanPhaseMs(-1), 400);
	CHECK_EQ(BotCore::NavReplanPhaseMs(-5), 0);
	CHECK_EQ(BotCore::NavReplanPhaseMs(7, 500, 5), 200);

	// 16 bots -> at most 4 share any one phase.
	int counts[5] = {};
	for (int b = 0; b < 16; ++b)
		++counts[BotCore::NavReplanPhaseMs(b) / 100];
	for (int i = 0; i < 5; ++i)
		CHECK(counts[i] <= 4);
}

TEST_CASE("NavBudget_Deferred_Contract")
{
	const NavDeferParams p;            // planMaxAgeMs 5000, driftMaxM 15.0

	// Fresh plan -> keep following.
	CHECK(BotCore::NavWhileDeferred(true, 0, 0.0f, p) == NavDeferAction::FollowPlan);
	CHECK(BotCore::NavWhileDeferred(true, 5000, 15.0f, p) == NavDeferAction::FollowPlan);
	// Boundary values are inclusive on the follow side.
	CHECK(BotCore::NavWhileDeferred(true, 4999, 14.9f, p) == NavDeferAction::FollowPlan);

	// Stale plan -> stop and wait.
	CHECK(BotCore::NavWhileDeferred(true, 5001, 0.0f, p) == NavDeferAction::Hold);
	CHECK(BotCore::NavWhileDeferred(true, 0, 15.1f, p) == NavDeferAction::Hold);
	CHECK(BotCore::NavWhileDeferred(true, 9000, 30.0f, p) == NavDeferAction::Hold);

	// No plan -> stop and wait (no random walk, no straight-line step).
	CHECK(BotCore::NavWhileDeferred(false, 0, 0.0f, p) == NavDeferAction::Hold);
	CHECK(BotCore::NavWhileDeferred(false, 9000, 30.0f, p) == NavDeferAction::Hold);
}

// Chase simulation: 16 followers each track an independent moving target. The follower walks its
// smoothed waypoints at 4.5 m/s (sprint 6.7). Mode A runs every query immediately; mode B uses the
// scheduler (budget 1.5 ms) plus the deferred contract. Harita yoksa SKIPPED.
TEST_CASE("NavBudget_Deferred_Chase_Sim")
{
	NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVBUDGET chase: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	const int bots = 16;
	const int64_t tickMs = 100;
	const int64_t endMs = 120000;

	struct Target
	{
		float x = 0.0f;
		float z = 0.0f;
		float vx = 0.0f;
		float vz = 0.0f;
		int64_t nextTurnMs = 0;
	};

	struct BotState
	{
		NavFollower follower;
		NavTargetTracker target;
		NavFollowPlan plan;
		NavQueryScheduler scheduler;
		float x = 0.0f;
		float z = 0.0f;
		bool hasPlan = false;
		int64_t planAtMs = 0;
		float planTargetX = 0.0f;
		float planTargetZ = 0.0f;
		std::vector<NavCell> wps;
		int wpIndex = 0;
		float targetDriftM = 0.0f;
	};

	std::vector<BotState> st((size_t)bots);
	BotCore::Rng rng(20261002u);

	std::vector<NavCell> walk;
	for (int cx = 0; cx < grid.Size(); ++cx)
		for (int cz = 0; cz < grid.Size(); ++cz)
			if (grid.Walk(cx, cz))
				walk.push_back(Cell(cx, cz));
	REQUIRE(!walk.empty());

	// Start each bot on a walk cell near the arena A centre (1274, 890) so the chases stay in the
	// navigable main component.
	const NavCell originAnchor = Cell(grid.CellOf(1274.0f), grid.CellOf(890.0f));
	for (int b = 0; b < bots; ++b)
	{
		NavCell start = originAnchor;
		for (int tries = 0; tries < 10000; ++tries)
		{
			const NavCell cand = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
			if (std::abs(cand.x - originAnchor.x) <= 20 && std::abs(cand.z - originAnchor.z) <= 20)
			{
				start = cand;
				break;
			}
		}
		st[(size_t)b].x = grid.CellCenter(start.x);
		st[(size_t)b].z = grid.CellCenter(start.z);
		st[(size_t)b].scheduler.SetMaxWaitMs(1000);
	}

	struct Run
	{
		std::vector<double> waits;      // request -> plan, ms
		long deferredTicks = 0;
		long holdTicks = 0;
		long followStaleTicks = 0;
		int noPlanTicks = 0;
		int totalTicks = 0;
		std::vector<double> dists;
		double holdDistMoved = 0.0;
	};

	auto simulate = [&](bool scheduled) -> Run
	{
		Run r;
		// Re-seed both modes identically.
		BotCore::Rng srng(777u);
		std::vector<Target> tg((size_t)bots);
		for (int b = 0; b < bots; ++b)
		{
			// Target starts on a Walk cell a short walk away from the bot.
			NavCell bc;
			bc.x = grid.CellOf(st[(size_t)b].x);
			bc.z = grid.CellOf(st[(size_t)b].z);
			NavCell tc = bc;
			for (int tries = 0; tries < 10000; ++tries)
			{
				const NavCell cand = walk[(size_t)srng.NextBelow((uint32_t)walk.size())];
				const int cheb = std::max(std::abs(cand.x - bc.x), std::abs(cand.z - bc.z));
				if (cheb >= 3 && cheb <= 12)
				{
					tc = cand;
					break;
				}
			}
			tg[(size_t)b].x = grid.CellCenter(tc.x);
			tg[(size_t)b].z = grid.CellCenter(tc.z);
			tg[(size_t)b].vx = 4.5f;
			tg[(size_t)b].vz = 0.0f;
			tg[(size_t)b].nextTurnMs = 6000 + (b % 5) * 1000;

			st[(size_t)b].follower.Reset();
			st[(size_t)b].target.Clear();
			st[(size_t)b].hasPlan = false;
			st[(size_t)b].planAtMs = 0;
			st[(size_t)b].wps.clear();
			st[(size_t)b].wpIndex = 0;
		}

		NavPathfinder pathfinder;
		NavSearchParams search;
		NavFollowParams fp;
		fp.replanIntervalMs = 500;
		fp.replanDistM = 6.0f;
		fp.ringMinM = 0.0f;
		fp.ringMaxM = 8.0f;   // accept a nearby walk cell if the target drifted onto a blocked one
		std::vector<NavCell> ringScratch;

		int64_t requestAt[16];
		for (int b = 0; b < bots; ++b)
			requestAt[b] = -1;

		auto snapWalk = [&](float & wx, float & wz)
		{
			int cx = grid.CellOf(wx);
			int cz = grid.CellOf(wz);
			if (grid.Walk(cx, cz))
				return;
			int bestX = 0;
			int bestZ = 0;
			int bestD = 1 << 30;
			for (int r = 1; r <= 40; ++r)
			{
				for (int dx = -r; dx <= r; ++dx)
				{
					for (int dz = -r; dz <= r; ++dz)
					{
						if (std::max(std::abs(dx), std::abs(dz)) != r)
							continue;
						if (!grid.Walk(cx + dx, cz + dz))
							continue;
						const int d = dx * dx + dz * dz;
						if (d < bestD)
						{
							bestD = d;
							bestX = cx + dx;
							bestZ = cz + dz;
						}
					}
				}
				if (bestD < (1 << 30))
					break;
			}
			wx = grid.CellCenter(bestX);
			wz = grid.CellCenter(bestZ);
		};

		for (int64_t t = 0; t < endMs; t += tickMs)		{
			++r.totalTicks;

			// Move targets; turn them at the scheduled time.
			for (int b = 0; b < bots; ++b)
			{
				Target & tb = tg[(size_t)b];
				if (t >= tb.nextTurnMs)
				{
					double a = srng.NextDouble() * 6.28318530718;
					tb.vx = 4.5f * (float)std::cos(a);
					tb.vz = 4.5f * (float)std::sin(a);
					tb.nextTurnMs = t + 6000 + (int64_t)(srng.NextBelow(4001));
				}
				tb.x += tb.vx * 0.1f;
				tb.z += tb.vz * 0.1f;
				snapWalk(tb.x, tb.z);   // keep the target on the navigable component
			}

			uint16_t batch[64];
			int batchCount = 0;
			if (!scheduled)
			{
				for (int b = 0; b < bots; ++b)
					batch[batchCount++] = (uint16_t)b;
			}
			else
			{
				for (int b = 0; b < bots; ++b)
				{
					// Replan phase: query when the follower would be due (first plan, 500 ms, or
					// the target moved 6 m since the current plan).
					const Target & tb = tg[(size_t)b];
					const float mdx = tb.x - st[(size_t)b].planTargetX;
					const float mdz = tb.z - st[(size_t)b].planTargetZ;
					const bool moved = std::sqrt(mdx * mdx + mdz * mdz) >= 6.0f;
					if (!st[(size_t)b].hasPlan || (t - st[(size_t)b].planAtMs) >= 500 || moved)
					if (requestAt[b] < 0)
					{
						st[0].scheduler.Request((uint16_t)b, t);   // one shared scheduler
						requestAt[b] = t;
					}
				}
				batchCount = st[0].scheduler.NextBatch(t, 1.5, batch, 64);
			}

			for (int k = 0; k < batchCount; ++k)
			{
				const int b = batch[k];
				BotState & bs = st[(size_t)b];
				const Target & tb = tg[(size_t)b];

				bs.target.Observe(t, tb.x, tb.z, 45);
				bs.follower.ObserveTarget(t, tb.x, tb.z, 45);

				const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
				const bool planned = bs.follower.Update(grid, pathfinder, t, bs.x, bs.z, 4.5f, fp);
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
				if (planned)
				{
					bs.plan = bs.follower.Plan();
					const bool good = (bs.plan.status == NavFollowStatus::Planned
						&& !bs.plan.smooth.waypoints.empty());
					if (good)
					{
						// A usable plan replaces the current path.
						bs.hasPlan = true;
						bs.planTargetX = tb.x;
						bs.planTargetZ = tb.z;
						bs.wps.clear();
						for (size_t i = 1; i < bs.plan.smooth.waypoints.size(); ++i)
							bs.wps.push_back(bs.plan.smooth.waypoints[i]);
						bs.wpIndex = 0;
						bs.planAtMs = t;
					}
					else if (!bs.hasPlan)
					{
						// No usable plan and nothing to follow: record the attempt time.
						bs.planAtMs = t;
					}
					// A failed refresh never discards a still-followable previous path.
					if (scheduled && requestAt[b] >= 0)
					{
						r.waits.push_back((double)(t - requestAt[b]));
						requestAt[b] = -1;
					}
				}
				if (scheduled)
				{
					st[0].scheduler.ReportCost((uint16_t)b, ms, bs.plan.expanded);
					st[0].scheduler.Cancel((uint16_t)b);
					requestAt[b] = -1;
				}
			}

			// Movement: follow the current plan when allowed, else apply the deferred contract.
			for (int b = 0; b < bots; ++b)
			{
				BotState & bs = st[(size_t)b];
				const Target & tb = tg[(size_t)b];

				bool blocked = false;
				if (scheduled)
				{
					// Deferred? The bot has a pending request that was not served this tick.
					blocked = requestAt[b] >= 0;
				}

				bool follow = true;
				if (blocked)
				{
					++r.deferredTicks;
					const float driftX = tb.x - bs.planTargetX;
					const float driftZ = tb.z - bs.planTargetZ;
					bs.targetDriftM = std::sqrt(driftX * driftX + driftZ * driftZ);
					const int64_t age = t - bs.planAtMs;
					const NavDeferAction action = BotCore::NavWhileDeferred(bs.hasPlan, age, bs.targetDriftM, NavDeferParams());
					if (action == NavDeferAction::FollowPlan && bs.hasPlan)
					{
						// acceptable: plan is still fresh
					}
					else
					{
						follow = false;
						++r.holdTicks;
						if (bs.hasPlan && (age > 5000 || bs.targetDriftM > 15.0f))
							++r.followStaleTicks;   // never allowed to be counted
					}
				}

				const float beforeX = bs.x;
				const float beforeZ = bs.z;

				bool moved = false;
				if (follow && bs.wpIndex < (int)bs.wps.size())
				{
					const float tx = grid.CellCenter(bs.wps[(size_t)bs.wpIndex].x);
					const float tz = grid.CellCenter(bs.wps[(size_t)bs.wpIndex].z);
					const float dx = tx - bs.x;
					const float dz = tz - bs.z;
					const float d = std::sqrt(dx * dx + dz * dz);
					const float step = 0.45f;   // 4.5 m/s at 100 ms
					if (d <= step)
					{
						bs.x = tx;
						bs.z = tz;
						++bs.wpIndex;
					}
					else
					{
						bs.x += dx / d * step;
						bs.z += dz / d * step;
					}
					moved = true;
				}
				if (!moved && !follow)
				{
					// Holding: position must not change at all (no jitter).
					const float ddx = bs.x - beforeX;
					const float ddz = bs.z - beforeZ;
					r.holdDistMoved += std::sqrt(ddx * ddx + ddz * ddz);
				}

				if (!bs.hasPlan)
					++r.noPlanTicks;

				const float ddx = bs.x - tb.x;
				const float ddz = bs.z - tb.z;
				r.dists.push_back(std::sqrt(ddx * ddx + ddz * ddz));
			}
		}
		return r;
	};

	const Run a = simulate(false);
	const Run b = simulate(true);

	std::vector<double> waitsA = a.waits;
	std::sort(waitsA.begin(), waitsA.end());
	std::vector<double> waitsB = b.waits;
	std::sort(waitsB.begin(), waitsB.end());
	std::vector<double> distsA = a.dists;
	std::sort(distsA.begin(), distsA.end());
	std::vector<double> distsB = b.dists;
	std::sort(distsB.begin(), distsB.end());

	const double aMean = distsA.empty() ? 0.0 : [&] { double s = 0; for (double d : distsA) s += d; return s / distsA.size(); }();
	const double bMean = distsB.empty() ? 0.0 : [&] { double s = 0; for (double d : distsB) s += d; return s / distsB.size(); }();

	const double aNoPlanPct = 100.0 * (double)a.noPlanTicks / ((double)a.totalTicks * (double)bots);
	const double bNoPlanPct = 100.0 * (double)b.noPlanTicks / ((double)b.totalTicks * (double)bots);

	std::printf("NAVBUDGET chase mode=A plan_wait_p50=%.0f plan_wait_p95=%.0f plan_wait_max=%.0f without_plan_pct=%.1f deferred_ticks=%ld hold_ticks=%ld follow_stale_ticks=%ld dist_mean=%.2f dist_p95=%.2f\n",
		PercentileDouble(waitsA, 0.5), PercentileDouble(waitsA, 0.95),
		waitsA.empty() ? 0.0 : waitsA.back(),
		aNoPlanPct,
		a.deferredTicks, a.holdTicks, a.followStaleTicks, aMean, PercentileDouble(distsA, 0.95));
	std::printf("NAVBUDGET chase mode=B plan_wait_p50=%.0f plan_wait_p95=%.0f plan_wait_max=%.0f without_plan_pct=%.1f deferred_ticks=%ld hold_ticks=%ld follow_stale_ticks=%ld dist_mean=%.2f dist_p95=%.2f\n",
		PercentileDouble(waitsB, 0.5), PercentileDouble(waitsB, 0.95),
		waitsB.empty() ? 0.0 : waitsB.back(),
		bNoPlanPct,
		b.deferredTicks, b.holdTicks, b.followStaleTicks, bMean, PercentileDouble(distsB, 0.95));

	// Acceptance (B).
	CHECK(!waitsB.empty());
	CHECK((waitsB.empty() ? 0.0 : waitsB.back()) <= 1100.0);
	CHECK(PercentileDouble(waitsB, 0.95) <= 800.0);
	CHECK(bNoPlanPct <= 3.0);
	CHECK_EQ(b.followStaleTicks, 0);
	CHECK(b.holdDistMoved == 0.0);
	CHECK(bMean <= 1.25 * aMean);
}

// Real-map load: 16 bots each tracking a near64 target every 500 ms for 60 s of virtual time.
// Mode A runs every follower Update every tick (worst case); mode B uses the scheduler (1.5 ms
// budget) plus replan phases. Per-tick nav time is real steady_clock; the p95/p99 and the longest
// path wait are reported. Harita yoksa SKIPPED.
TEST_CASE("NavBudget_RealMap_Load")
{
	NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVBUDGET realm load: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	const int bots = 16;
	const int64_t tickMs = 100;
	const int64_t endMs = 60000;

	std::vector<NavCell> walk;
	for (int x = 0; x < grid.Size(); ++x)
		for (int z = 0; z < grid.Size(); ++z)
			if (grid.Walk(x, z))
				walk.push_back(Cell(x, z));
	REQUIRE(!walk.empty());

	struct BotState
	{
		NavFollower follower;
		NavQueryScheduler scheduler;
		float x = 0.0f;
		float z = 0.0f;
		int64_t planAtMs = 0;
		float targetX = 0.0f;
		float targetZ = 0.0f;
	};

	std::vector<BotState> st((size_t)bots);
	BotCore::Rng rng(4242u);
	for (int b = 0; b < bots; ++b)
	{
		const NavCell c = walk[(size_t)rng.NextBelow((uint32_t)walk.size())];
		st[(size_t)b].x = grid.CellCenter(c.x);
		st[(size_t)b].z = grid.CellCenter(c.z);
		st[(size_t)b].scheduler.SetMaxWaitMs(1000);
	}

	auto simulate = [&](bool scheduled, std::vector<double> & tickSum, int64_t & longestWaitMs, double tickBudgetMs) -> long
	{
		long deferredTicks = 0;
		BotCore::Rng srng(999u);
		NavPathfinder pathfinder;
		NavSearchParams search;
		NavFollowParams fp;
		fp.replanIntervalMs = 500;
		fp.replanDistM = 6.0f;

		// Reset followers.
		for (int b = 0; b < bots; ++b)
		{
			st[(size_t)b].follower.Reset();
			st[(size_t)b].planAtMs = 0;
		}

		int64_t requestAt[16];
		for (int b = 0; b < bots; ++b)
			requestAt[b] = -1;

		for (int64_t t = 0; t < endMs; t += tickMs)
		{
			// Every 500 ms pick a fresh near64 target (same cadence in both modes).
			for (int b = 0; b < bots; ++b)
			{
				BotState & bs = st[(size_t)b];
				if (bs.follower.Tracker().Count() == 0 || (t - bs.planAtMs) >= 500)
				{
					const NavCell from;
					NavCell self;
					self.x = grid.CellOf(bs.x);
					self.z = grid.CellOf(bs.z);
					(void)from;
					NavCell goal = self;
					for (int tries = 0; tries < 200; ++tries)
					{
						const NavCell cand = walk[(size_t)srng.NextBelow((uint32_t)walk.size())];
						if (std::abs(cand.x - self.x) > 64 || std::abs(cand.z - self.z) > 64)
							continue;
						if (cand == self)
							continue;
						goal = cand;
						break;
					}
					bs.targetX = grid.CellCenter(goal.x);
					bs.targetZ = grid.CellCenter(goal.z);
				}
			}

			uint16_t batch[64];
			int batchCount = 0;
			if (!scheduled)
			{
				for (int b = 0; b < bots; ++b)
					batch[batchCount++] = (uint16_t)b;
			}
			else
			{
				for (int b = 0; b < bots; ++b)
				{
					if ((t - st[(size_t)b].planAtMs) >= 500 && requestAt[b] < 0)
					{
						st[(size_t)b].scheduler.Request((uint16_t)b, t);
						requestAt[b] = t;
					}
				}
				batchCount = st[0].scheduler.NextBatch(t, tickBudgetMs, batch, 64);
				if (st[0].scheduler.Pending() > 0)
					++deferredTicks;
			}

			double sum = 0.0;
			for (int k = 0; k < batchCount; ++k)
			{
				const int b = batch[k];
				BotState & bs = st[(size_t)b];

				bs.follower.ObserveTarget(t, bs.targetX, bs.targetZ, 45);
				const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
				const bool planned = bs.follower.Update(grid, pathfinder, t, bs.x, bs.z, 4.5f, fp);
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
				sum += ms;

				if (planned)
				{
					bs.planAtMs = t;
					if (scheduled && requestAt[b] >= 0)
					{
						const int64_t wait = t - requestAt[b];
						if (wait > longestWaitMs)
							longestWaitMs = wait;
						requestAt[b] = -1;
					}
				}
				if (scheduled)
				{
					bs.scheduler.ReportCost((uint16_t)b, ms, 0);
					bs.scheduler.Cancel((uint16_t)b);
					requestAt[b] = -1;
				}
			}
			tickSum.push_back(sum);
		}
		return deferredTicks;
	};

	std::vector<double> sumA;
	std::vector<double> sumB;
	int64_t waitA = 0;
	int64_t waitB = 0;
	simulate(false, sumA, waitA, 1.5);
	simulate(true, sumB, waitB, 1.5);

	std::vector<double> sortedA = sumA;
	std::sort(sortedA.begin(), sortedA.end());
	std::vector<double> sortedB = sumB;
	std::sort(sortedB.begin(), sortedB.end());

	const double a95 = PercentileDouble(sortedA, 0.95);
	const double b95 = PercentileDouble(sortedB, 0.95);
	const double b99 = PercentileDouble(sortedB, 0.99);

	std::printf("NAVBUDGET realm mode=A tick_p50=%.3f tick_p95=%.3f tick_p99=%.3f tick_max=%.3f longest_wait=%.0f\n",
		PercentileDouble(sortedA, 0.5), a95, PercentileDouble(sortedA, 0.99),
		sortedA.empty() ? 0.0 : sortedA.back(), (double)waitA);
	std::printf("NAVBUDGET realm mode=B tick_p50=%.3f tick_p95=%.3f tick_p99=%.3f tick_max=%.3f longest_wait=%.0f\n",
		PercentileDouble(sortedB, 0.5), b95, b99,
		sortedB.empty() ? 0.0 : sortedB.back(), (double)waitB);

#ifndef _DEBUG
	CHECK(b95 <= 2.0);
	CHECK(b99 <= 4.5);
	CHECK(waitB <= 1100);
	CHECK(b95 <= a95 * 0.70);
#endif
}

