#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavSmooth.h>
#include <BotCore/NavTrack.h>
#include <BotCore/NavReach.h>
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
	using BotCore::NavReach;

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
		// All remaining entries (ids 5..39) waited the same 1100 ms. The deterministic oldest-request
		// order gives id 5 first: the first call advanced the rotation offset to 1, and the rotating
		// rank of the remaining ids is still ascending.
		CHECK_EQ((int)out[0], 5);
	}
}

TEST_CASE("NavBudget_Scheduler_Priority")
{
	NavQueryScheduler s;
	s.SetMaxWaitMs(1000);
	uint16_t out[128];

	// (a) Over-waiters always lead, at every rotation offset. 3 bots waited 1500 ms (ids 10..12)
	// and 5 bots waited 100 ms (ids 0..4); the budget fits exactly one query (initial cost 0.5).
	for (int off = 0; off < 10; ++off)
	{
		for (int i = 0; i < 3; ++i)
			s.Request((uint16_t)(10 + i), 0);          // now = 1500 -> wait 1500 (over maxWait)
		for (int i = 0; i < 5; ++i)
			s.Request((uint16_t)i, 1400);              // now = 1500 -> wait 100

		const int n = s.NextBatch(1500, 0.5, out, 128);
		CHECK_EQ(n, 1);
		CHECK(out[0] >= 10 && out[0] <= 12);

		// Drain so the next iteration re-requests with fresh timestamps; the call has still
		// advanced the rotation offset by one.
		for (int i = 0; i < 3; ++i)
			s.Cancel((uint16_t)(10 + i));
		for (int i = 0; i < 5; ++i)
			s.Cancel((uint16_t)i);
	}
	CHECK_EQ(s.Pending(), 0);

	// (b) Distinct waits sort oldest first, independent of the rotation offset. Ids 20..25 waited
	// 600, 500, ..., 100 ms at now = 1000.
	for (int off = 0; off < 6; ++off)
	{
		for (int i = 0; i < 6; ++i)
			s.Request((uint16_t)(20 + i), 400 + (int64_t)i * 100);
		const int n = s.NextBatch(1000, 100.0, out, 128);
		CHECK_EQ(n, 6);
		for (int i = 0; i < 6; ++i)
			CHECK_EQ((int)out[i], 20 + i);
		for (int i = 0; i < 6; ++i)
			s.Cancel((uint16_t)(20 + i));
	}

	// (c) Equal waits rotate across calls: the first pick changes on every consecutive call.
	s.Clear();
	for (int i = 0; i < 6; ++i)
		s.Request((uint16_t)i, 1000);
	uint16_t firstPick[4] = {};
	for (int k = 0; k < 4; ++k)
	{
		const int n = s.NextBatch(1000, 0.5, out, 128);   // one query per call
		CHECK_EQ(n, 1);
		firstPick[k] = out[0];
		s.Cancel(out[0]);
	}
	for (int k = 1; k < 4; ++k)
		CHECK(firstPick[k] != firstPick[k - 1]);
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
	grid.Build();   // production slope limit (ADR-0024, 0.45)
	REQUIRE(grid.MainComponentCells() == 88508);

	// Keep the chase sampling (bot starts, targets, drift snapping) on the edge-connected main
	// component so the measured follow/defer behaviour is about the navigable space, not pockets.
	NavReach reach;
	reach.Build(grid);
	const int mainComp = reach.LargestComponent();
	auto mainWalk = [&](int x, int z)
	{
		return grid.Walk(x, z) && reach.ComponentOf(x, z) == mainComp;
	};

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
			if (mainWalk(cx, cz))
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
		long staleHoldTicks = 0;
		long followStaleTicks = 0;
		int noPlanTicks = 0;
		int totalTicks = 0;
		std::vector<double> dists;
		double holdDistMoved = 0.0;
	};

	auto simulate = [&](bool scheduled, double extraCostMs) -> Run
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
		st[0].scheduler.Clear();   // fresh queue/cost estimates for every mode (A, B, B2)

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
			if (mainWalk(cx, cz))
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
						if (!mainWalk(cx + dx, cz + dz))
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

		for (int64_t t = 0; t < endMs; t += tickMs)
		{
			++r.totalTicks;

			// Tick-start positions, so the held-bot movement measure compares two real points.
			float posStartX[16];
			float posStartZ[16];
			for (int b = 0; b < bots; ++b)
			{
				posStartX[b] = st[(size_t)b].x;
				posStartZ[b] = st[(size_t)b].z;
			}

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
					{
						if (requestAt[b] < 0)
						{
							st[0].scheduler.Request((uint16_t)b, t);   // one shared scheduler
							requestAt[b] = t;
						}
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
				const bool planned = bs.follower.UpdateReachable(grid, pathfinder, t, bs.x, bs.z, 4.5f, fp, reach);
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
					// `extraCostMs` is a synthetic surcharge for mode B2 only, so the measured
					// real A* cost stays separate from what the scheduler believes.
					st[0].scheduler.ReportCost((uint16_t)b, ms + extraCostMs, bs.plan.expanded);
					st[0].scheduler.Cancel((uint16_t)b);
					requestAt[b] = -1;
				}
			}

			// Movement: follow the current plan when allowed, else apply the deferred contract.
			for (int b = 0; b < bots; ++b)
			{
				BotState & bs = st[(size_t)b];
				const Target & tb = tg[(size_t)b];

				// Plan freshness, shared by the deferred contract and the stale-follow metric.
				const float driftX = tb.x - bs.planTargetX;
				const float driftZ = tb.z - bs.planTargetZ;
				bs.targetDriftM = std::sqrt(driftX * driftX + driftZ * driftZ);
				const int64_t age = t - bs.planAtMs;
				const NavDeferParams deferParams;
				const bool stale = bs.hasPlan
					&& (age > (int64_t)deferParams.planMaxAgeMs || bs.targetDriftM > deferParams.driftMaxM);

				// Deferred? The bot has a pending request that was not served this tick.
				const bool blocked = scheduled && requestAt[b] >= 0;

				bool follow = true;
				if (blocked)
				{
					++r.deferredTicks;
					const NavDeferAction action = BotCore::NavWhileDeferred(bs.hasPlan, age, bs.targetDriftM, deferParams);
					if (action != NavDeferAction::FollowPlan || !bs.hasPlan)
					{
						follow = false;
						++r.holdTicks;
						if (stale)
							++r.staleHoldTicks;   // informational: held while the plan is stale
					}
				}
				// A bot that keeps moving on a stale plan is an invariant violation (must be 0).
				if (follow && stale)
					++r.followStaleTicks;

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
				if (moved)
					snapWalk(bs.x, bs.z);   // a smoothed chord may cross an edge-isolated pocket cell and the planner cannot leave one (KI); keep the follower on the main component like the target
				if (!moved && !follow)
				{
					// Holding: position must not change at all (no jitter). Compared against the
					// tick-start position, so this is not a tautology.
					const float ddx = bs.x - posStartX[b];
					const float ddz = bs.z - posStartZ[b];
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

	// Mode B2 adds a synthetic per-query cost so the queue actually builds up; this is the only
	// difference from B (the real A* measurement itself is untouched).
	const double b2ExtraCostMs = 0.5;
	const Run a = simulate(false, 0.0);
	const Run b = simulate(true, 0.0);
	const Run b2 = simulate(true, b2ExtraCostMs);

	std::vector<double> waitsA = a.waits;
	std::sort(waitsA.begin(), waitsA.end());
	std::vector<double> waitsB = b.waits;
	std::sort(waitsB.begin(), waitsB.end());
	std::vector<double> waitsB2 = b2.waits;
	std::sort(waitsB2.begin(), waitsB2.end());
	std::vector<double> distsA = a.dists;
	std::sort(distsA.begin(), distsA.end());
	std::vector<double> distsB = b.dists;
	std::sort(distsB.begin(), distsB.end());
	std::vector<double> distsB2 = b2.dists;
	std::sort(distsB2.begin(), distsB2.end());

	const double aMean = distsA.empty() ? 0.0 : [&] { double s = 0; for (double d : distsA) s += d; return s / distsA.size(); }();
	const double bMean = distsB.empty() ? 0.0 : [&] { double s = 0; for (double d : distsB) s += d; return s / distsB.size(); }();
	const double b2Mean = distsB2.empty() ? 0.0 : [&] { double s = 0; for (double d : distsB2) s += d; return s / distsB2.size(); }();

	const double aNoPlanPct = 100.0 * (double)a.noPlanTicks / ((double)a.totalTicks * (double)bots);
	const double bNoPlanPct = 100.0 * (double)b.noPlanTicks / ((double)b.totalTicks * (double)bots);
	const double b2NoPlanPct = 100.0 * (double)b2.noPlanTicks / ((double)b2.totalTicks * (double)bots);

	auto printChase = [&](const char * mode, const Run & r, const std::vector<double> & w,
		const std::vector<double> & d, double mean, double noPlanPct)
	{
		std::printf("NAVBUDGET chase mode=%s plan_wait_p50=%.0f plan_wait_p95=%.0f plan_wait_max=%.0f without_plan_pct=%.1f deferred_ticks=%ld hold_ticks=%ld stale_hold_ticks=%ld follow_stale_ticks=%ld dist_mean=%.2f dist_p95=%.2f\n",
			mode, PercentileDouble(w, 0.5), PercentileDouble(w, 0.95),
			w.empty() ? 0.0 : w.back(), noPlanPct,
			r.deferredTicks, r.holdTicks, r.staleHoldTicks, r.followStaleTicks, mean, PercentileDouble(d, 0.95));
	};
	printChase("A", a, waitsA, distsA, aMean, aNoPlanPct);
	printChase("B", b, waitsB, distsB, bMean, bNoPlanPct);
	printChase("B2", b2, waitsB2, distsB2, b2Mean, b2NoPlanPct);

	// Acceptance (B).
	CHECK(!waitsB.empty());
	CHECK((waitsB.empty() ? 0.0 : waitsB.back()) <= 1100.0);
	CHECK(PercentileDouble(waitsB, 0.95) <= 800.0);
	CHECK(bNoPlanPct <= 3.0);
	CHECK_EQ(b.followStaleTicks, 0);
	CHECK(b.holdDistMoved == 0.0);
	CHECK(bMean <= 1.25 * aMean);

	// Acceptance (B2): the queue really built up (>= 5% of bot-ticks deferred, some holds), and
	// the deferred contract still holds under that load. The wait thresholds are Release-only
	// (Debug A* is ~10x slower, which turns B2 into a different, harsher overload).
	CHECK(b2.deferredTicks >= (long)(((long long)b2.totalTicks * bots + 19) / 20));
	CHECK(b2.holdTicks > 0);
	CHECK(b2NoPlanPct <= 3.0);
	CHECK_EQ(b2.followStaleTicks, 0);
#ifndef _DEBUG
	CHECK(PercentileDouble(waitsB2, 0.95) <= 800.0);
	CHECK((waitsB2.empty() ? 0.0 : waitsB2.back()) <= 1100.0);
#endif
}

// Real-map load: 16 bots each tracking a near64 target every 500 ms for 60 s of virtual time.
// Mode A runs every follower Update every tick (worst case); mode B uses one shared scheduler
// (1.5 ms budget) plus replan phases. Each mode runs three times, printed as its own line; per-bot
// and total query counts are reported, and the worst p95/p99 drive the Release acceptance.
// Harita yoksa SKIPPED.
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

	// ADR-0024 (maxSlope 0.45) edge-isolated pockets must not be sampled as query goals: the
	// scheduler budget is measured over the navigable main component.
	NavReach reach;
	reach.Build(grid);
	const int mainComp = reach.LargestComponent();

	const int bots = 16;
	const int64_t tickMs = 100;
	const int64_t endMs = 60000;
	const double budgetMs = 1.5;   // P-NAV-TICK-BUDGET-MS

	std::vector<NavCell> walk;
	for (int x = 0; x < grid.Size(); ++x)
		for (int z = 0; z < grid.Size(); ++z)
			if (grid.Walk(x, z) && reach.ComponentOf(x, z) == mainComp)
				walk.push_back(Cell(x, z));
	REQUIRE(!walk.empty());

	struct BotState
	{
		NavFollower follower;
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
	}

	// One shared scheduler for every bot (the production shape: a single instance on the tick
	// thread). Request, NextBatch, ReportCost, Cancel and Pending all use this instance.
	NavQueryScheduler scheduler;
	scheduler.SetMaxWaitMs(1000);

	struct ModeStats
	{
		std::vector<double> tickSum;
		int64_t longestWaitMs = 0;
		long servedTotal = 0;         // executed A* queries (Update returning true)
		int servedPerBot[16] = {};
		int servedMinPerBot = 0;
	};

	auto runMode = [&](bool scheduled, uint32_t seed, ModeStats & out)
	{
		BotCore::Rng srng(seed);
		NavPathfinder pathfinder;
		NavSearchParams search;
		NavFollowParams fp;
		fp.replanIntervalMs = 500;
		fp.replanDistM = 6.0f;

		for (int b = 0; b < bots; ++b)
		{
			st[(size_t)b].follower.Reset();
			st[(size_t)b].planAtMs = 0;
		}
		scheduler.Clear();

		int64_t requestAt[16];
		for (int b = 0; b < bots; ++b)
			requestAt[b] = -1;

		for (int64_t t = 0; t < endMs; t += tickMs)
		{
			// Target refresh / request cadence. Mode A is phase-less; mode B is staggered by
			// NavReplanPhaseMs(slot) so the 500 ms replans do not all land in the same tick.
			for (int b = 0; b < bots; ++b)
			{
				BotState & bs = st[(size_t)b];
				bool due;
				if (!scheduled)
				{
					due = bs.follower.Tracker().Count() == 0 || (t - bs.planAtMs) >= 500;
				}
				else
				{
					const int64_t phase = (int64_t)BotCore::NavReplanPhaseMs(b);
					due = t >= phase && ((t - phase) % 500) == 0;
				}
				if (!due)
					continue;

				NavCell self;
				self.x = grid.CellOf(bs.x);
				self.z = grid.CellOf(bs.z);
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
				bs.follower.ObserveTarget(t, bs.targetX, bs.targetZ, 45);

				if (scheduled)
				{
					scheduler.Request((uint16_t)b, t);
					if (requestAt[b] < 0)
						requestAt[b] = t;
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
				batchCount = scheduler.NextBatch(t, budgetMs, batch, 64);
			}

			double sum = 0.0;
			for (int k = 0; k < batchCount; ++k)
			{
				const int b = batch[k];
				BotState & bs = st[(size_t)b];

				const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
				const bool planned = bs.follower.Update(grid, pathfinder, t, bs.x, bs.z, 4.5f, fp);
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
				sum += ms;

				if (planned)
				{
					bs.planAtMs = t;
					++out.servedPerBot[b];
					++out.servedTotal;
				}
				if (scheduled)
				{
					// Longest wait is the real request -> service gap, measured for every served bot.
					if (requestAt[b] >= 0)
					{
						const int64_t wait = t - requestAt[b];
						if (wait > out.longestWaitMs)
							out.longestWaitMs = wait;
					}
					scheduler.ReportCost((uint16_t)b, ms, 0);
					scheduler.Cancel((uint16_t)b);
					requestAt[b] = -1;
				}
			}
			out.tickSum.push_back(sum);
		}

		out.servedMinPerBot = out.servedPerBot[0];
		for (int b = 1; b < bots; ++b)
			if (out.servedPerBot[b] < out.servedMinPerBot)
				out.servedMinPerBot = out.servedPerBot[b];
	};

	const int runs = 3;
	auto printMode = [&](const char * mode, int run, const ModeStats & s)
	{
		std::vector<double> sorted = s.tickSum;
		std::sort(sorted.begin(), sorted.end());
		std::printf("NAVBUDGET realm mode=%s run=%d tick_p50=%.3f tick_p95=%.3f tick_p99=%.3f tick_max=%.3f longest_wait=%.0f served_total=%ld served_min_per_bot=%d\n",
			mode, run, PercentileDouble(sorted, 0.5), PercentileDouble(sorted, 0.95),
			PercentileDouble(sorted, 0.99), sorted.empty() ? 0.0 : sorted.back(),
			(double)s.longestWaitMs, s.servedTotal, s.servedMinPerBot);
	};

	double worstA95 = 0.0;
	double worstB95 = 0.0;
	double worstB99 = 0.0;
	int64_t worstWaitB = 0;
	long totalA = 0;
	long totalB = 0;
	int minB = 1 << 30;
	for (int r = 0; r < runs; ++r)
	{
		ModeStats msA;
		ModeStats msB;
		runMode(false, 7000u + (uint32_t)r, msA);
		runMode(true, 7000u + (uint32_t)r, msB);
		printMode("A", r, msA);
		printMode("B", r, msB);

		std::vector<double> sa = msA.tickSum;
		std::sort(sa.begin(), sa.end());
		std::vector<double> sb = msB.tickSum;
		std::sort(sb.begin(), sb.end());
		const double a95 = PercentileDouble(sa, 0.95);
		const double b95 = PercentileDouble(sb, 0.95);
		const double b99 = PercentileDouble(sb, 0.99);
		if (a95 > worstA95)
			worstA95 = a95;
		if (b95 > worstB95)
			worstB95 = b95;
		if (b99 > worstB99)
			worstB99 = b99;
		if (msB.longestWaitMs > worstWaitB)
			worstWaitB = msB.longestWaitMs;
		totalA += msA.servedTotal;
		totalB += msB.servedTotal;
		if (msB.servedMinPerBot < minB)
			minB = msB.servedMinPerBot;
	}

	std::printf("NAVBUDGET realm summary worst_A_p95=%.3f worst_B_p95=%.3f worst_B_p99=%.3f worst_B_wait=%.0f served_A=%ld served_B=%ld served_B_min_per_bot=%d\n",
		worstA95, worstB95, worstB99, (double)worstWaitB, totalA, totalB, minB);

#ifndef _DEBUG
	CHECK(minB >= 1);                                   // every bot served in mode B
	CHECK((long long)totalB * 10 >= (long long)totalA * 9);   // B total >= 90% of A total
	CHECK(worstB95 <= 2.0);
	CHECK(worstB99 <= 4.5);
	CHECK(worstWaitB <= 1100);
	CHECK(worstB95 <= worstA95 * 0.70);
#endif
}
