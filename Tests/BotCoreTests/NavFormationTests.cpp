#include "MiniTest.h"

#include <BotCore/NavGrid.h>
#include <BotCore/NavPath.h>
#include <BotCore/NavSmooth.h>
#include <BotCore/NavTrack.h>
#include <BotCore/NavFormation.h>
#include <BotCore/Rng.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <tuple>
#include <vector>

namespace
{
	using BotCore::NavCell;
	using BotCore::NavGrid;
	using BotCore::NavFormPoint;
	using BotCore::NavSeparationParams;

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

	NavGrid MakeNav(int n, float unit, const std::vector<int16_t> & events, const std::vector<float> & heights)
	{
		NavGrid grid;
		if (!grid.Init(n, unit, events, heights))
			CHECK(false);
		grid.Build();
		return grid;
	}

	NavFormPoint Pt(float x, float z)
	{
		NavFormPoint p;
		p.x = x;
		p.z = z;
		return p;
	}

	// Smallest pair distance (float); a large value for fewer than two members.
	float MinPair(const std::vector<NavFormPoint> & points)
	{
		float best = 1.0e30f;
		for (size_t i = 0; i < points.size(); ++i)
		{
			for (size_t j = i + 1; j < points.size(); ++j)
			{
				const float dx = points[i].x - points[j].x;
				const float dz = points[i].z - points[j].z;
				const float d = std::sqrt(dx * dx + dz * dz);
				if (d < best)
					best = d;
			}
		}
		return best;
	}

	// Independent reference for NavAssignSurroundSlots (shares no code with production): own
	// 8-direction table, keep pass, then a sorted (d2, member, slot) pair list.
	std::vector<int> RefAssign(const NavGrid & grid, float tx, float tz, float radius,
		const std::vector<NavFormPoint> & members, std::vector<int> prev, int & assigned)
	{
		const float h = 0.70710678f;
		const float dirX[8] = { 0.0f, -h, -1.0f, -h, 0.0f, h, 1.0f, h };
		const float dirZ[8] = { 1.0f, h, 0.0f, -h, -1.0f, -h, 0.0f, h };
		const float r = radius < 0.0f ? 0.0f : radius;

		float slotX[8];
		float slotZ[8];
		bool usable[8];
		bool taken[8];
		for (int s = 0; s < 8; ++s)
		{
			slotX[s] = tx + dirX[s] * r;
			slotZ[s] = tz + dirZ[s] * r;
			usable[s] = grid.Walk(grid.CellOf(slotX[s]), grid.CellOf(slotZ[s]));
			taken[s] = false;
		}

		std::vector<int> slots(members.size(), -1);

		// Keep pass.
		for (size_t i = 0; i < members.size(); ++i)
		{
			const int s = (i < prev.size()) ? prev[i] : -1;
			if (s >= 0 && s < 8 && usable[s] && !taken[s])
			{
				slots[i] = s;
				taken[s] = true;
			}
		}

		// Greedy pass over a sorted pair list.
		std::vector<std::tuple<float, int, int> > pairs;
		for (size_t i = 0; i < members.size(); ++i)
		{
			if (slots[i] >= 0)
				continue;
			for (int s = 0; s < 8; ++s)
			{
				if (!usable[s])
					continue;
				const float dx = members[i].x - slotX[s];
				const float dz = members[i].z - slotZ[s];
				pairs.push_back(std::make_tuple(dx * dx + dz * dz, (int)i, s));
			}
		}
		std::sort(pairs.begin(), pairs.end());
		for (size_t k = 0; k < pairs.size(); ++k)
		{
			const int i = std::get<1>(pairs[k]);
			const int s = std::get<2>(pairs[k]);
			if (slots[(size_t)i] >= 0 || taken[s])
				continue;
			slots[(size_t)i] = s;
			taken[s] = true;
		}

		assigned = 0;
		for (size_t i = 0; i < slots.size(); ++i)
			if (slots[i] >= 0)
				++assigned;
		return slots;
	}

	struct SettleResult
	{
		std::vector<NavFormPoint> pos;
		std::vector<int> firstSlots;
		int firstAssigned = 0;
		int settleTick = -1;
		int maxStacked1m = 0;
		bool offWalk = false;
		bool slotsChanged = false;
		int initialStacked = 0;
	};

	// Test-only settling simulation: sticky slot assignment, a straight 1.5 m step per tick and
	// pairwise separation applied from one snapshot.
	SettleResult SettleSim(const NavGrid & grid, float tx, float tz, float sx, float sz, int ticks)
	{
		const int count = 8;
		const float radius = 2.5f;
		const float kStepM = 1.5f;
		NavSeparationParams params;

		SettleResult res;
		res.pos.resize((size_t)count);
		res.firstSlots.assign((size_t)count, -1);
		std::vector<int> slots((size_t)count, -1);

		for (int i = 0; i < count; ++i)
			res.pos[(size_t)i] = Pt(sx + 0.2f * (float)(i % 4), sz + 0.2f * (float)(i / 4));
		res.initialStacked = BotCore::NavCountStackedPairs(res.pos.data(), res.pos.size(), 1.5f);

		for (int t = 1; t <= ticks; ++t)
		{
			BotCore::NavAssignSurroundSlots(grid, tx, tz, radius, res.pos.data(), res.pos.size(),
				slots.data());
			if (t == 1)
			{
				res.firstSlots = slots;
				int assigned = 0;
				for (int i = 0; i < count; ++i)
					if (slots[(size_t)i] >= 0)
						++assigned;
				res.firstAssigned = assigned;
			}
			else if (slots != res.firstSlots)
			{
				res.slotsChanged = true;
			}

			// Step toward the slot (unassigned members stay put).
			for (int i = 0; i < count; ++i)
			{
				if (slots[(size_t)i] < 0)
					continue;
				float gx = 0.0f;
				float gz = 0.0f;
				BotCore::NavSurroundPoint(slots[(size_t)i], tx, tz, radius, gx, gz);
				const float dx = gx - res.pos[(size_t)i].x;
				const float dz = gz - res.pos[(size_t)i].z;
				const float d = std::sqrt(dx * dx + dz * dz);
				if (d <= kStepM)
				{
					res.pos[(size_t)i].x = gx;
					res.pos[(size_t)i].z = gz;
				}
				else
				{
					res.pos[(size_t)i].x += dx / d * kStepM;
					res.pos[(size_t)i].z += dz / d * kStepM;
				}
			}

			// Separation from one snapshot.
			const std::vector<NavFormPoint> snapshot = res.pos;
			for (int i = 0; i < count; ++i)
			{
				float vx = 0.0f;
				float vz = 0.0f;
				BotCore::NavSeparationVector(snapshot.data(), snapshot.size(), (size_t)i, params, vx, vz);
				float nx = 0.0f;
				float nz = 0.0f;
				if (BotCore::NavApplySeparation(grid, res.pos[(size_t)i].x, res.pos[(size_t)i].z,
					vx, vz, nx, nz))
				{
					res.pos[(size_t)i].x = nx;
					res.pos[(size_t)i].z = nz;
				}
			}

			if (t >= 3)
			{
				const int stacked = BotCore::NavCountStackedPairs(res.pos.data(), res.pos.size(), 1.0f);
				if (stacked > res.maxStacked1m)
					res.maxStacked1m = stacked;
			}

			for (int i = 0; i < count; ++i)
			{
				if (!grid.Walk(grid.CellOf(res.pos[(size_t)i].x), grid.CellOf(res.pos[(size_t)i].z)))
					res.offWalk = true;
			}

			bool allClose = true;
			for (int i = 0; i < count; ++i)
			{
				if (slots[(size_t)i] < 0)
					continue;
				float gx = 0.0f;
				float gz = 0.0f;
				BotCore::NavSurroundPoint(slots[(size_t)i], tx, tz, radius, gx, gz);
				const float dx = gx - res.pos[(size_t)i].x;
				const float dz = gz - res.pos[(size_t)i].z;
				if (std::sqrt(dx * dx + dz * dz) >= 0.05f)
					allClose = false;
			}
			if (allClose && res.settleTick < 0)
				res.settleTick = t;
		}
		return res;
	}
}

TEST_CASE("NavForm_Slots")
{
	CHECK_EQ(BotCore::kNavSurroundSlots, 8);

	const float h = 0.70710678f;
	const float dirX[8] = { 0.0f, -h, -1.0f, -h, 0.0f, h, 1.0f, h };
	const float dirZ[8] = { 1.0f, h, 0.0f, -h, -1.0f, -h, 0.0f, h };
	for (int s = 0; s < 8; ++s)
	{
		float dx = 9.0f;
		float dz = 9.0f;
		BotCore::NavSurroundDir(s, dx, dz);
		CHECK(std::fabs(dx - dirX[s]) <= 1e-6f);
		CHECK(std::fabs(dz - dirZ[s]) <= 1e-6f);
	}

	const int bad[3] = { -1, 8, 100 };
	for (int b = 0; b < 3; ++b)
	{
		float dx = 9.0f;
		float dz = 9.0f;
		BotCore::NavSurroundDir(bad[b], dx, dz);
		CHECK_EQ(dx, 0.0f);
		CHECK_EQ(dz, 0.0f);

		float x = 0.0f;
		float z = 0.0f;
		BotCore::NavSurroundPoint(bad[b], 82.0f, 82.0f, 2.5f, x, z);
		CHECK_EQ(x, 82.0f);
		CHECK_EQ(z, 82.0f);
	}

	// Table 1 slot points (target (82, 82), radius 2.5).
	const float px[8] = { 82.0f, 80.23223f, 79.5f, 80.23223f, 82.0f, 83.76777f, 84.5f, 83.76777f };
	const float pz[8] = { 84.5f, 83.76777f, 82.0f, 80.23223f, 79.5f, 80.23223f, 82.0f, 83.76777f };
	for (int s = 0; s < 8; ++s)
	{
		float x = 0.0f;
		float z = 0.0f;
		BotCore::NavSurroundPoint(s, 82.0f, 82.0f, 2.5f, x, z);
		CHECK(std::fabs(x - px[s]) <= 1e-4f);
		CHECK(std::fabs(z - pz[s]) <= 1e-4f);
	}

	// Neighbour pair distances (7 -> 0 wraps).
	for (int s = 0; s < 8; ++s)
	{
		const int t = (s + 1) % 8;
		const float dx = px[t] - px[s];
		const float dz = pz[t] - pz[s];
		const float d = std::sqrt(dx * dx + dz * dz);
		CHECK(std::fabs(d - 1.91342f) <= 1e-4f);
		CHECK(d >= 1.5f);
	}

	NavGrid grid = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));

	// Flat target: all eight usable.
	for (int s = 0; s < 8; ++s)
		CHECK(BotCore::NavSurroundUsable(grid, s, 82.0f, 82.0f, 2.5f));

	// Corner target (6, 6).
	const int mask25[8] = { 1, 1, 0, 1, 0, 1, 1, 1 };
	for (int s = 0; s < 8; ++s)
		CHECK_EQ((int)BotCore::NavSurroundUsable(grid, s, 6.0f, 6.0f, 2.5f), mask25[s]);
	const int mask40[8] = { 1, 0, 0, 0, 0, 0, 1, 1 };
	for (int s = 0; s < 8; ++s)
		CHECK_EQ((int)BotCore::NavSurroundUsable(grid, s, 6.0f, 6.0f, 4.0f), mask40[s]);

	for (int b = 0; b < 3; ++b)
		CHECK(!BotCore::NavSurroundUsable(grid, bad[b], 82.0f, 82.0f, 2.5f));
}

TEST_CASE("NavForm_Assign")
{
	NavGrid grid = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));

	auto check = [&](const std::vector<NavFormPoint> & members, std::vector<int> in, int expectedAssigned,
		const std::vector<int> & expectedSlots, float tx, float tz, float radius)
	{
		const int assigned = BotCore::NavAssignSurroundSlots(grid, tx, tz, radius,
			members.data(), members.size(), in.data());
		CHECK_EQ(assigned, expectedAssigned);
		CHECK_EQ((int)in.size(), (int)expectedSlots.size());
		int minusOnes = 0;
		for (size_t i = 0; i < in.size(); ++i)
		{
			CHECK_EQ(in[i], expectedSlots[i]);
			if (in[i] < 0)
				++minusOnes;
		}
		CHECK_EQ(minusOnes, (int)members.size() - assigned);
		for (size_t i = 0; i < in.size(); ++i)
		{
			if (in[i] < 0)
				continue;
			CHECK(in[i] >= 0 && in[i] < 8);
			CHECK(BotCore::NavSurroundUsable(grid, in[i], tx, tz, radius));
			for (size_t j = 0; j < i; ++j)
				CHECK(in[j] != in[i]);
		}
	};

	// perm
	{
		std::vector<NavFormPoint> members(8);
		for (int i = 0; i < 8; ++i)
		{
			float sx = 0.0f;
			float sz = 0.0f;
			BotCore::NavSurroundPoint((i * 3) % 8, 82.0f, 82.0f, 2.5f, sx, sz);
			members[(size_t)i] = Pt(sx + 0.1f, sz - 0.1f);
		}
		const int exp[8] = { 0, 3, 6, 1, 4, 7, 2, 5 };
		check(members, std::vector<int>(8, -1), 8, std::vector<int>(exp, exp + 8), 82.0f, 82.0f, 2.5f);
	}

	// Three members.
	{
		std::vector<NavFormPoint> members;
		members.push_back(Pt(82.0f, 100.0f));
		members.push_back(Pt(82.0f, 60.0f));
		members.push_back(Pt(120.0f, 82.0f));
		const int exp[3] = { 0, 4, 6 };
		check(members, std::vector<int>(3, -1), 3, std::vector<int>(exp, exp + 3), 82.0f, 82.0f, 2.5f);
	}

	// Ten members: two extra stay unassigned.
	{
		std::vector<NavFormPoint> members(10);
		for (int i = 0; i < 8; ++i)
		{
			float sx = 0.0f;
			float sz = 0.0f;
			BotCore::NavSurroundPoint(i, 82.0f, 82.0f, 2.5f, sx, sz);
			members[(size_t)i] = Pt(sx + 0.1f, sz + 0.1f);
		}
		members[8] = Pt(82.0f, 100.0f);
		members[9] = Pt(60.0f, 82.0f);
		const int exp[10] = { 0, 1, 2, 3, 4, 5, 6, 7, -1, -1 };
		check(members, std::vector<int>(10, -1), 8, std::vector<int>(exp, exp + 10), 82.0f, 82.0f, 2.5f);
	}

	// Sticky: valid input slots are kept; duplicates and out-of-range are dropped.
	{
		std::vector<NavFormPoint> members(5, Pt(60.0f, 82.0f));
		std::vector<int> in(5);
		in[0] = 3;
		in[1] = 3;
		in[2] = 9;
		in[3] = -1;
		in[4] = 2;
		const int exp[5] = { 3, 1, 0, 4, 2 };
		check(members, in, 5, std::vector<int>(exp, exp + 5), 82.0f, 82.0f, 2.5f);
	}

	// Corner target: slots 2 and 4 lie on wall cells.
	{
		std::vector<NavFormPoint> members(8, Pt(40.0f, 40.0f));
		const int exp[8] = { 7, 0, 6, 1, 5, 3, -1, -1 };
		check(members, std::vector<int>(8, -1), 6, std::vector<int>(exp, exp + 8), 6.0f, 6.0f, 2.5f);
	}

	// Unusable input slots are dropped.
	{
		std::vector<NavFormPoint> members;
		members.push_back(Pt(20.0f, 6.0f));
		members.push_back(Pt(20.0f, 4.0f));
		std::vector<int> in(2);
		in[0] = 2;
		in[1] = 4;
		const int exp[2] = { 6, 5 };
		check(members, in, 2, std::vector<int>(exp, exp + 2), 6.0f, 6.0f, 2.5f);
	}

	// Zero count, null members, null slots.
	{
		NavFormPoint member = Pt(82.0f, 82.0f);
		int slot = 2;
		CHECK_EQ(BotCore::NavAssignSurroundSlots(grid, 82.0f, 82.0f, 2.5f, &member, 0, &slot), 0);
		CHECK_EQ(slot, 2);
		CHECK_EQ(BotCore::NavAssignSurroundSlots(grid, 82.0f, 82.0f, 2.5f, nullptr, 1, &slot), 0);
		CHECK_EQ(slot, 2);
		CHECK_EQ(BotCore::NavAssignSurroundSlots(grid, 82.0f, 82.0f, 2.5f, &member, 1, nullptr), 0);
		CHECK_EQ(slot, 2);
	}

	// Negative and zero radius: every slot coincides with the target.
	{
		std::vector<NavFormPoint> members;
		members.push_back(Pt(60.0f, 82.0f));
		members.push_back(Pt(104.0f, 82.0f));
		members.push_back(Pt(82.0f, 60.0f));
		const int exp[3] = { 0, 1, 2 };
		const float radii[2] = { -3.0f, 0.0f };
		for (int r = 0; r < 2; ++r)
			check(members, std::vector<int>(3, -1), 3, std::vector<int>(exp, exp + 3),
				82.0f, 82.0f, radii[r]);
	}
}

TEST_CASE("NavForm_Assign_Matches_Reference")
{
	const int n = 24;
	const float unit = 4.0f;
	int trials = 0;
	int mismatches = 0;
	double assignedSum = 0.0;

	for (int seed = 0; seed < 30; ++seed)
	{
		BotCore::Rng rng(6000u + (uint32_t)seed);

		std::vector<int16_t> events((size_t)n * (size_t)n, 1);
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				if (x == 0 || x == n - 1 || z == 0 || z == n - 1)
				{
					events[CellIndex(n, x, z)] = 0;
					continue;
				}
				if (rng.NextBelow(100) < 8)
					events[CellIndex(n, x, z)] = 0;
			}
		}

		std::vector<float> heights((size_t)n * (size_t)n, 0.0f);
		for (int x = 0; x < n; ++x)
			for (int z = 0; z < n; ++z)
				heights[CellIndex(n, x, z)] = (rng.NextBelow(5) == 0) ? 6.0f : 0.0f;

		NavGrid grid = MakeNav(n, unit, events, heights);

		std::vector<NavCell> walkCells;
		for (int x = 0; x < n; ++x)
			for (int z = 0; z < n; ++z)
				if (grid.Walk(x, z))
					walkCells.push_back(Cell(x, z));
		if (walkCells.size() < 2)
			continue;

		for (int q = 0; q < 10; ++q)
		{
			const NavCell target = walkCells[(size_t)rng.NextBelow((uint32_t)walkCells.size())];
			const float tx = grid.CellCenter(target.x);
			const float tz = grid.CellCenter(target.z);
			const float radii[3] = { 1.5f, 2.5f, 4.0f };
			const float radius = radii[rng.NextBelow(3)];
			const int memberCount = (int)rng.NextBelow(13);

			std::vector<NavFormPoint> members((size_t)memberCount);
			std::vector<int> in((size_t)memberCount, -1);
			for (int i = 0; i < memberCount; ++i)
			{
				members[(size_t)i] = Pt((float)rng.NextBelow(960) / 10.0f,
					(float)rng.NextBelow(960) / 10.0f);
				in[(size_t)i] = (int)rng.NextBelow(14) - 3;
			}

			std::vector<int> prod = in;
			const int prodAssigned = BotCore::NavAssignSurroundSlots(grid, tx, tz, radius,
				members.data(), members.size(), prod.data());
			int refAssigned = 0;
			const std::vector<int> ref = RefAssign(grid, tx, tz, radius, members, in, refAssigned);

			int usableCount = 0;
			for (int s = 0; s < 8; ++s)
				if (BotCore::NavSurroundUsable(grid, s, tx, tz, radius))
					++usableCount;

			bool ok = (prod == ref) && (prodAssigned == refAssigned)
				&& (prodAssigned == (memberCount < usableCount ? memberCount : usableCount));
			for (size_t i = 0; i < prod.size() && ok; ++i)
			{
				if (prod[i] < 0)
					continue;
				if (prod[i] >= 8 || !BotCore::NavSurroundUsable(grid, prod[i], tx, tz, radius))
					ok = false;
				for (size_t j = 0; j < i && ok; ++j)
					if (prod[j] == prod[i])
						ok = false;
			}
			if (!ok)
				++mismatches;
			assignedSum += (double)prodAssigned;
			++trials;
		}
	}

	const double avg = trials > 0 ? assignedSum / (double)trials : 0.0;
	std::printf("NAVFORM random assign: trials=%d mismatches=%d avg_assigned=%.3f\n",
		trials, mismatches, avg);
	REQUIRE(trials >= 270);
	CHECK_EQ(mismatches, 0);
}

TEST_CASE("NavForm_Separation")
{
	NavGrid grid = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));
	NavSeparationParams params;

	// Two members at d = 1.
	{
		std::vector<NavFormPoint> m;
		m.push_back(Pt(80.0f, 80.0f));
		m.push_back(Pt(81.0f, 80.0f));
		float v0x = 0.0f;
		float v0z = 0.0f;
		float v1x = 0.0f;
		float v1z = 0.0f;
		BotCore::NavSeparationVector(m.data(), m.size(), 0, params, v0x, v0z);
		BotCore::NavSeparationVector(m.data(), m.size(), 1, params, v1x, v1z);
		CHECK(std::fabs(v0x - (-0.25f)) <= 1e-4f);
		CHECK(std::fabs(v0z) <= 1e-4f);
		CHECK(std::fabs(v1x - 0.25f) <= 1e-4f);
		CHECK(std::fabs(v1z) <= 1e-4f);
		const float d = std::fabs((80.0f + v0x) - (81.0f + v1x));
		CHECK(std::fabs(d - 1.5f) <= 1e-4f);
	}

	// Exactly at the threshold: no push.
	{
		std::vector<NavFormPoint> m;
		m.push_back(Pt(80.0f, 80.0f));
		m.push_back(Pt(81.5f, 80.0f));
		float vx = 9.0f;
		float vz = 9.0f;
		BotCore::NavSeparationVector(m.data(), m.size(), 0, params, vx, vz);
		CHECK_EQ(vx, 0.0f);
		CHECK_EQ(vz, 0.0f);
	}

	// Two coincident members.
	{
		std::vector<NavFormPoint> m(2, Pt(80.0f, 80.0f));
		float v0x = 0.0f;
		float v0z = 0.0f;
		float v1x = 0.0f;
		float v1z = 0.0f;
		BotCore::NavSeparationVector(m.data(), m.size(), 0, params, v0x, v0z);
		BotCore::NavSeparationVector(m.data(), m.size(), 1, params, v1x, v1z);
		CHECK(std::fabs(v0x - (-0.53033f)) <= 1e-4f);
		CHECK(std::fabs(v0z - 0.53033f) <= 1e-4f);
		CHECK(std::fabs(v1x - 0.53033f) <= 1e-4f);
		CHECK(std::fabs(v1z - (-0.53033f)) <= 1e-4f);
		const float d = std::sqrt((v0x - v1x) * (v0x - v1x) + (v0z - v1z) * (v0z - v1z));
		CHECK(std::fabs(d - 1.5f) <= 1e-4f);
	}

	// Three members, member 0 and member 1.
	std::vector<NavFormPoint> three;
	three.push_back(Pt(80.0f, 80.0f));
	three.push_back(Pt(81.0f, 80.0f));
	three.push_back(Pt(80.0f, 81.0f));
	{
		float vx = 0.0f;
		float vz = 0.0f;
		BotCore::NavSeparationVector(three.data(), three.size(), 0, params, vx, vz);
		CHECK(std::fabs(vx - (-0.25f)) <= 1e-4f);
		CHECK(std::fabs(vz - (-0.25f)) <= 1e-4f);
		CHECK(std::fabs(std::sqrt(vx * vx + vz * vz) - 0.35355f) <= 1e-4f);

		BotCore::NavSeparationVector(three.data(), three.size(), 1, params, vx, vz);
		CHECK(std::fabs(vx - 0.28033f) <= 1e-4f);
		CHECK(std::fabs(vz - (-0.03033f)) <= 1e-4f);
	}

	// maxPushM clamp.
	{
		float vx = 0.0f;
		float vz = 0.0f;
		NavSeparationParams p = params;
		p.maxPushM = 0.1f;
		BotCore::NavSeparationVector(three.data(), three.size(), 0, p, vx, vz);
		CHECK(std::fabs(vx - (-0.07071f)) <= 1e-4f);
		CHECK(std::fabs(vz - (-0.07071f)) <= 1e-4f);

		p.maxPushM = 0.0f;
		BotCore::NavSeparationVector(three.data(), three.size(), 0, p, vx, vz);
		CHECK_EQ(vx, 0.0f);
		CHECK_EQ(vz, 0.0f);
		p.maxPushM = -1.0f;
		BotCore::NavSeparationVector(three.data(), three.size(), 0, p, vx, vz);
		CHECK_EQ(vx, 0.0f);
		CHECK_EQ(vz, 0.0f);
	}

	// Three coincident members (clamped to 1.0 m).
	{
		std::vector<NavFormPoint> m(3, Pt(80.0f, 80.0f));
		float vx = 0.0f;
		float vz = 0.0f;
		BotCore::NavSeparationVector(m.data(), m.size(), 0, params, vx, vz);
		CHECK(std::fabs(vx - (-0.92388f)) <= 1e-4f);
		CHECK(std::fabs(vz - 0.38268f) <= 1e-4f);
		BotCore::NavSeparationVector(m.data(), m.size(), 1, params, vx, vz);
		CHECK(std::fabs(vx) <= 1e-4f);
		CHECK(std::fabs(vz - (-1.0f)) <= 1e-4f);
		BotCore::NavSeparationVector(m.data(), m.size(), 2, params, vx, vz);
		CHECK(std::fabs(vx - 0.92388f) <= 1e-4f);
		CHECK(std::fabs(vz - 0.38268f) <= 1e-4f);
	}

	// self >= count and non-positive minDistM.
	{
		float vx = 9.0f;
		float vz = 9.0f;
		BotCore::NavSeparationVector(three.data(), three.size(), 3, params, vx, vz);
		CHECK_EQ(vx, 0.0f);
		CHECK_EQ(vz, 0.0f);
		NavSeparationParams p = params;
		p.minDistM = 0.0f;
		BotCore::NavSeparationVector(three.data(), three.size(), 0, p, vx, vz);
		CHECK_EQ(vx, 0.0f);
		CHECK_EQ(vz, 0.0f);
		p.minDistM = -2.0f;
		BotCore::NavSeparationVector(three.data(), three.size(), 0, p, vx, vz);
		CHECK_EQ(vx, 0.0f);
		CHECK_EQ(vz, 0.0f);
	}

	// NavApplySeparation.
	{
		float nx = 0.0f;
		float nz = 0.0f;
		CHECK(BotCore::NavApplySeparation(grid, 4.5f, 80.0f, -1.0f, 0.3f, nx, nz));
		CHECK(std::fabs(nx - 4.5f) <= 1e-4f);
		CHECK(std::fabs(nz - 80.3f) <= 1e-4f);

		nx = 7.0f;
		nz = 7.0f;
		CHECK(!BotCore::NavApplySeparation(grid, 4.5f, 80.0f, -1.0f, 0.0f, nx, nz));
		CHECK_EQ(nx, 4.5f);
		CHECK_EQ(nz, 80.0f);

		nx = 7.0f;
		nz = 7.0f;
		CHECK(!BotCore::NavApplySeparation(grid, 80.0f, 80.0f, 0.0f, 0.0f, nx, nz));
		CHECK_EQ(nx, 80.0f);
		CHECK_EQ(nz, 80.0f);

		nx = 0.0f;
		nz = 0.0f;
		CHECK(BotCore::NavApplySeparation(grid, 80.0f, 80.0f, 0.25f, -0.5f, nx, nz));
		CHECK(std::fabs(nx - 80.25f) <= 1e-4f);
		CHECK(std::fabs(nz - 79.5f) <= 1e-4f);

		nx = 7.0f;
		nz = 7.0f;
		CHECK(!BotCore::NavApplySeparation(grid, 2.0f, 80.0f, 0.25f, 0.0f, nx, nz));
		CHECK_EQ(nx, 2.0f);
		CHECK_EQ(nz, 80.0f);

		nx = 0.0f;
		nz = 0.0f;
		CHECK(BotCore::NavApplySeparation(grid, 81.9f, 80.0f, 0.3f, 0.0f, nx, nz));
		CHECK(std::fabs(nx - 82.2f) <= 1e-4f);
		CHECK(std::fabs(nz - 80.0f) <= 1e-4f);
	}

	// NavCountStackedPairs.
	{
		std::vector<NavFormPoint> m;
		m.push_back(Pt(80.0f, 80.0f));
		m.push_back(Pt(80.5f, 80.0f));
		m.push_back(Pt(81.0f, 80.0f));
		m.push_back(Pt(100.0f, 100.0f));
		CHECK_EQ(BotCore::NavCountStackedPairs(m.data(), m.size(), 1.5f), 3);
		CHECK_EQ(BotCore::NavCountStackedPairs(m.data(), m.size(), 1.0f), 2);
		CHECK_EQ(BotCore::NavCountStackedPairs(m.data(), m.size(), 0.5f), 0);
		CHECK_EQ(BotCore::NavCountStackedPairs(m.data(), m.size(), 0.0f), 0);
		CHECK_EQ(BotCore::NavCountStackedPairs(m.data(), m.size(), -1.0f), 0);
		CHECK_EQ(BotCore::NavCountStackedPairs(nullptr, 4, 1.5f), 0);
		CHECK_EQ(BotCore::NavCountStackedPairs(m.data(), 0, 1.5f), 0);
		CHECK_EQ(BotCore::NavCountStackedPairs(m.data(), 1, 1.5f), 0);
	}

	// Reciprocity over 200 random close pairs.
	{
		BotCore::Rng rng(7000u);
		int violations = 0;
		for (int p = 0; p < 200; ++p)
		{
			std::vector<NavFormPoint> m(2);
			m[0] = Pt(80.0f, 80.0f);
			m[1] = Pt(80.0f + (float)rng.NextBelow(200) / 100.0f - 1.0f,
				80.0f + (float)rng.NextBelow(200) / 100.0f - 1.0f);
			float v0x = 0.0f;
			float v0z = 0.0f;
			float v1x = 0.0f;
			float v1z = 0.0f;
			BotCore::NavSeparationVector(m.data(), m.size(), 0, params, v0x, v0z);
			BotCore::NavSeparationVector(m.data(), m.size(), 1, params, v1x, v1z);
			if (std::fabs(v0x + v1x) > 1e-5f || std::fabs(v0z + v1z) > 1e-5f)
				++violations;
		}
		CHECK_EQ(violations, 0);
	}
}

TEST_CASE("NavForm_PickSpaced")
{
	NavGrid grid = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));

	std::vector<NavCell> c;
	BotCore::NavRingCells(grid, 82.0f, 82.0f, 12.0f, 16.0f, Cell(20, 20), c);
	CHECK_EQ((int)c.size(), 24);
	CHECK(c[0] == Cell(17, 20));
	CHECK(c[12] == Cell(17, 18));
	CHECK(c[20] == Cell(16, 20));

	std::vector<NavFormPoint> others(1, Pt(82.0f, 82.0f));
	CHECK_EQ(BotCore::NavPickSpaced(grid, c, others.data(), others.size(), 8.0f), 0);
	CHECK_EQ(BotCore::NavPickSpaced(grid, c, others.data(), others.size(), 13.0f), 12);
	CHECK_EQ(BotCore::NavPickSpaced(grid, c, others.data(), others.size(), 100.0f), 20);

	std::vector<NavFormPoint> others2;
	others2.push_back(Pt(82.0f, 82.0f));
	others2.push_back(Pt(70.0f, 82.0f));
	CHECK_EQ(BotCore::NavPickSpaced(grid, c, others2.data(), others2.size(), 8.0f), 1);

	CHECK_EQ(BotCore::NavPickSpaced(grid, c, nullptr, 0, 8.0f), 0);

	std::vector<NavCell> empty;
	CHECK_EQ(BotCore::NavPickSpaced(grid, empty, others.data(), others.size(), 8.0f), -1);

	std::vector<NavCell> c2;
	BotCore::NavRingCells(grid, 82.0f, 82.0f, 4.0f, 10.0f, Cell(20, 20), c2);
	CHECK_EQ((int)c2.size(), 20);
	CHECK(c2[0] == Cell(19, 20));

	std::vector<NavFormPoint> priest(1, Pt(70.0f, 82.0f));
	CHECK_EQ(BotCore::NavPickSpaced(grid, c2, priest.data(), priest.size(), 8.0f), 0);
	CHECK_EQ(BotCore::NavPickSpaced(grid, c2, priest.data(), priest.size(), 8.01f), 1);
}

TEST_CASE("NavForm_Settle_Flat")
{
	NavGrid grid = MakeNav(40, 4.0f, RingEvents(40), HeightZeros(40));
	const SettleResult res = SettleSim(grid, 100.0f, 80.0f, 40.0f, 80.0f, 80);
	const SettleResult res2 = SettleSim(grid, 100.0f, 80.0f, 40.0f, 80.0f, 80);

	CHECK_EQ(res.initialStacked, 28);
	CHECK_EQ(res.firstAssigned, 8);
	bool seen[8] = { false, false, false, false, false, false, false, false };
	for (size_t i = 0; i < res.firstSlots.size(); ++i)
	{
		const int s = res.firstSlots[i];
		CHECK(s >= 0 && s < 8);
		if (s >= 0 && s < 8)
			seen[s] = true;
	}
	for (int s = 0; s < 8; ++s)
		CHECK(seen[s]);
	CHECK(!res.slotsChanged);
	CHECK_EQ(res.maxStacked1m, 0);
	CHECK(res.settleTick >= 1 && res.settleTick <= 60);
	CHECK(std::fabs(MinPair(res.pos) - 1.91342f) <= 1e-3f);
	CHECK_EQ(BotCore::NavCountStackedPairs(res.pos.data(), res.pos.size(), 1.499f), 0);
	CHECK(!res.offWalk);

	// Determinism: bit-identical positions on the second call.
	REQUIRE(res.pos.size() == res2.pos.size());
	bool same = true;
	for (size_t i = 0; i < res.pos.size(); ++i)
	{
		if (res.pos[i].x != res2.pos[i].x || res.pos[i].z != res2.pos[i].z)
			same = false;
	}
	CHECK(same);

	std::printf("NAVFORM settle flat: assigned=%d settle_tick=%d max_stacked_1m=%d final_min_pair=%.4f\n",
		res.firstAssigned, res.settleTick, res.maxStacked1m, (double)MinPair(res.pos));
}

TEST_CASE("NavForm_Settle_RealMap")
{
	NavGrid grid;
	if (!grid.LoadFile("build/nav/zone71.navgrid"))
	{
		std::printf("NAVFORM real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}
	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	// Distribution over every Walk cell as target (radius 2.5).
	int walk = 0;
	int usable8 = 0;
	int usable7 = 0;
	int usable6 = 0;
	int usable5 = 0;
	int usableLt5 = 0;
	int assignViolations = 0;
	for (int x = 0; x < grid.Size(); ++x)
	{
		for (int z = 0; z < grid.Size(); ++z)
		{
			if (!grid.Walk(x, z))
				continue;
			++walk;
			const float cx = grid.CellCenter(x);
			const float cz = grid.CellCenter(z);
			int u = 0;
			for (int s = 0; s < 8; ++s)
				if (BotCore::NavSurroundUsable(grid, s, cx, cz, 2.5f))
					++u;
			switch (u)
			{
			case 8: ++usable8; break;
			case 7: ++usable7; break;
			case 6: ++usable6; break;
			case 5: ++usable5; break;
			default: ++usableLt5; break;
			}

			std::vector<NavFormPoint> members(8, Pt(cx, cz));
			std::vector<int> slots(8, -1);
			const int assigned = BotCore::NavAssignSurroundSlots(grid, cx, cz, 2.5f,
				members.data(), members.size(), slots.data());
			const int expected = (8 < u) ? 8 : u;
			if (assigned != expected)
				++assignViolations;
		}
	}

	CHECK_EQ(walk, 88508);
	CHECK_EQ(usable8, 72459);
	CHECK_EQ(usable7, 11568);
	CHECK_EQ(usable6, 4137);
	CHECK_EQ(usable5, 344);
	CHECK_EQ(usableLt5, 0);
	CHECK_EQ(assignViolations, 0);

	// Wall-side target: slots 2 and 4 lie on wall cells.
	REQUIRE(BotCore::NavLineClear(grid, Cell(318, 224), Cell(315, 216)));
	const int mask[8] = { 1, 1, 0, 1, 0, 1, 1, 1 };
	for (int s = 0; s < 8; ++s)
		CHECK_EQ((int)BotCore::NavSurroundUsable(grid, s, 1262.0f, 866.0f, 2.5f), mask[s]);

	const SettleResult res = SettleSim(grid, 1262.0f, 866.0f, 1273.0f, 897.0f, 40);
	CHECK_EQ(res.firstAssigned, 6);
	int minusOnes = 0;
	bool members67Minus = true;
	for (size_t i = 0; i < res.firstSlots.size(); ++i)
	{
		if (res.firstSlots[i] < 0)
		{
			++minusOnes;
			if (i != 6 && i != 7)
				members67Minus = false;
		}
	}
	CHECK_EQ(minusOnes, 2);
	CHECK(members67Minus);
	bool used[8] = { false, false, false, false, false, false, false, false };
	for (size_t i = 0; i < res.firstSlots.size(); ++i)
	{
		const int s = res.firstSlots[i];
		if (s < 0)
			continue;
		CHECK(s != 2 && s != 4);
		CHECK(!used[s]);
		used[s] = true;
	}
	CHECK(!res.slotsChanged);
	CHECK_EQ(res.maxStacked1m, 0);
	CHECK(res.settleTick >= 1 && res.settleTick <= 40);
	CHECK(MinPair(res.pos) >= 1.499f);
	CHECK_EQ(BotCore::NavCountStackedPairs(res.pos.data(), res.pos.size(), 1.499f), 0);
	for (size_t i = 0; i < res.pos.size(); ++i)
	{
		if (res.firstSlots[i] >= 0)
			continue;
		const float dx = res.pos[i].x - 1273.0f;
		const float dz = res.pos[i].z - 897.0f;
		CHECK(std::sqrt(dx * dx + dz * dz) <= 3.0f);
	}
	CHECK(!res.offWalk);

	std::printf("NAVFORM real: walk=%d usable8=%d usable7=%d usable6=%d usable5=%d usable_lt5=%d assign_violations=%d; settle assigned=%d settle_tick=%d max_stacked_1m=%d final_min_pair=%.4f unassigned=%d\n",
		walk, usable8, usable7, usable6, usable5, usableLt5, assignViolations,
		res.firstAssigned, res.settleTick, res.maxStacked1m, (double)MinPair(res.pos), minusOnes);
}
