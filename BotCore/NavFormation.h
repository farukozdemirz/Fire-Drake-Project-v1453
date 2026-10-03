#pragma once

// Formation and anti-stacking (F5-08; docs/12 s9, ADR-0006 Ek F5-08) on top of the F5-01 grid
// and the F5-03 smoothing. Three independent stateless tools: surround-slot assignment around a
// target, pairwise separation of stacked party members and spaced candidate selection (two
// priests >= 8 m as a choice, not a force). Pure logic: the standard library only, no server
// header, no global/static state, no allocation. The caller owns role distribution, the second
// ring, NavFollower binding and the MET-NAV-06 time rule.

#include "NavSmooth.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace BotCore
{
	constexpr int kNavSurroundSlots = 8;

	struct NavFormPoint
	{
		float x = 0.0f;
		float z = 0.0f;
	};       // world metres

	// Unit direction of surround slot `slot` (0..7); (0, 0) for any other value. Table (dx, dz):
	// 0 (0,1)  1 (-h,h)  2 (-1,0)  3 (-h,-h)  4 (0,-1)  5 (h,-h)  6 (1,0)  7 (h,h), h = 0.70710678f.
	inline void NavSurroundDir(int slot, float & dx, float & dz)
	{
		static constexpr float kDirX[kNavSurroundSlots] = {
			0.0f, -0.70710678f, -1.0f, -0.70710678f, 0.0f, 0.70710678f, 1.0f, 0.70710678f
		};
		static constexpr float kDirZ[kNavSurroundSlots] = {
			1.0f, 0.70710678f, 0.0f, -0.70710678f, -1.0f, -0.70710678f, 0.0f, 0.70710678f
		};

		if (slot < 0 || slot >= kNavSurroundSlots)
		{
			dx = 0.0f;
			dz = 0.0f;
			return;
		}
		dx = kDirX[slot];
		dz = kDirZ[slot];
	}

	// x = cx + dx * radiusM, z = cz + dz * radiusM (radiusM is NOT clamped here; same float
	// expression for x and z).
	inline void NavSurroundPoint(int slot, float cx, float cz, float radiusM, float & x, float & z)
	{
		float dx = 0.0f;
		float dz = 0.0f;
		NavSurroundDir(slot, dx, dz);
		x = cx + dx * radiusM;
		z = cz + dz * radiusM;
	}

	// False for an out-of-range slot; otherwise grid.Walk(CellOf(x), CellOf(z)) of the slot point.
	inline bool NavSurroundUsable(const NavGrid & grid, int slot, float cx, float cz, float radiusM)
	{
		if (slot < 0 || slot >= kNavSurroundSlots)
			return false;
		float x = 0.0f;
		float z = 0.0f;
		NavSurroundPoint(slot, cx, cz, radiusM, x, z);
		return grid.Walk(grid.CellOf(x), grid.CellOf(z));
	}

	// slots[i] (in/out, `count` entries) is the slot of members[i] (-1 = none). Returns the number
	// of members that end with a slot (kept + newly assigned). Rules (all deterministic):
	//  0. count == 0, members == nullptr or slots == nullptr: return 0, nothing touched. radiusM < 0
	//     is 0.
	//  1. Usable slots: NavSurroundUsable for the target (tx, tz).
	//  2. Keep pass (i ascending): a member keeps slots[i] when it is in 0..7, usable and not
	//     already kept; every other member's slots[i] becomes -1.
	//  3. Greedy pass: repeatedly pick, among members with slots[i] < 0 and free usable slots, the
	//     pair with the smallest squared distance member -> slot point (float, dx*dx + dz*dz); ties:
	//     smaller member index first, then smaller slot index (strict `<` while scanning i
	//     ascending, s ascending). Stop when no pair is left. No allocation: "has a slot" is
	//     slots[i] >= 0.
	inline int NavAssignSurroundSlots(const NavGrid & grid, float tx, float tz, float radiusM,
		const NavFormPoint * members, size_t count, int * slots)
	{
		if (count == 0 || members == nullptr || slots == nullptr)
			return 0;

		const float radius = radiusM < 0.0f ? 0.0f : radiusM;

		float slotX[kNavSurroundSlots];
		float slotZ[kNavSurroundSlots];
		bool usable[kNavSurroundSlots];
		bool taken[kNavSurroundSlots];
		for (int s = 0; s < kNavSurroundSlots; ++s)
		{
			NavSurroundPoint(s, tx, tz, radius, slotX[s], slotZ[s]);
			usable[s] = grid.Walk(grid.CellOf(slotX[s]), grid.CellOf(slotZ[s]));
			taken[s] = false;
		}

		// Keep pass.
		int assigned = 0;
		for (size_t i = 0; i < count; ++i)
		{
			const int s = slots[i];
			if (s >= 0 && s < kNavSurroundSlots && usable[s] && !taken[s])
			{
				taken[s] = true;
				++assigned;
			}
			else
			{
				slots[i] = -1;
			}
		}

		// Greedy pass: earliest (member, then slot) among the minimum squared distances.
		for (;;)
		{
			int bestI = -1;
			int bestS = -1;
			float bestD2 = 0.0f;
			for (size_t i = 0; i < count; ++i)
			{
				if (slots[i] >= 0)
					continue;
				for (int s = 0; s < kNavSurroundSlots; ++s)
				{
					if (!usable[s] || taken[s])
						continue;
					const float dx = members[i].x - slotX[s];
					const float dz = members[i].z - slotZ[s];
					const float d2 = dx * dx + dz * dz;
					if (bestI < 0 || d2 < bestD2)
					{
						bestI = (int)i;
						bestS = s;
						bestD2 = d2;
					}
				}
			}
			if (bestI < 0)
				break;
			slots[bestI] = bestS;
			taken[bestS] = true;
			++assigned;
		}

		return assigned;
	}

	struct NavSeparationParams
	{
		float minDistM = 1.5f;     // [O] docs/12 s9: members closer than this push each other apart
		float maxPushM = 1.0f;     // [A] per application; <= 0 disables the push
	};

	// Push of members[self] away from every other member closer than minDistM; (vx, vz) = (0, 0)
	// when self >= count or minDistM <= 0 or maxPushM <= 0. For each other member j (ascending):
	//  - d2 >= minDistM^2: no push (the threshold itself does not push).
	//  - d2 <= 1e-8f (coincident): direction NavSurroundDir((self + j) % 8) times (self < j ? +1 :
	//    -1), magnitude 0.5 * minDistM  (antisymmetric: i and j get opposite vectors, deterministic).
	//  - else: unit vector (self - j) / d times 0.5 * (minDistM - d)  (a pair that both apply ends at
	//    exactly minDistM).
	// The sum is clamped to maxPushM (direction kept). Vectors are computed from the same snapshot.
	inline void NavSeparationVector(const NavFormPoint * members, size_t count, size_t self,
		const NavSeparationParams & params, float & vx, float & vz)
	{
		vx = 0.0f;
		vz = 0.0f;

		if (members == nullptr || self >= count)
			return;
		const float minDist = params.minDistM;
		if (minDist <= 0.0f || params.maxPushM <= 0.0f)
			return;

		const float minDist2 = minDist * minDist;
		float sx = 0.0f;
		float sz = 0.0f;

		for (size_t j = 0; j < count; ++j)
		{
			if (j == self)
				continue;
			const float dx = members[self].x - members[j].x;
			const float dz = members[self].z - members[j].z;
			const float d2 = dx * dx + dz * dz;
			if (d2 >= minDist2)
				continue;

			if (d2 <= 1e-8f)
			{
				float ux = 0.0f;
				float uz = 0.0f;
				NavSurroundDir((int)((self + j) % (size_t)kNavSurroundSlots), ux, uz);
				const float sign = (self < j) ? 1.0f : -1.0f;
				const float mag = 0.5f * minDist;
				sx += sign * ux * mag;
				sz += sign * uz * mag;
			}
			else
			{
				const float d = std::sqrt(d2);
				const float mag = 0.5f * (minDist - d);
				sx += (dx / d) * mag;
				sz += (dz / d) * mag;
			}
		}

		const float len2 = sx * sx + sz * sz;
		if (len2 > 0.0f)
		{
			const float len = std::sqrt(len2);
			if (len > params.maxPushM)
			{
				const float scale = params.maxPushM / len;
				sx *= scale;
				sz *= scale;
			}
		}
		vx = sx;
		vz = sz;
	}

	// Moves (x, z) by (vx, vz) when that stays walkable. Tries, in this order, the full move, the
	// x-only move, the z-only move (sliding along a wall); a candidate with both components 0 is
	// skipped; a candidate is accepted when its cell equals the current cell and that cell is Walk,
	// or NavLineClear(current cell, candidate cell). Returns true and the new position on the first
	// accepted candidate; otherwise false and (nx, nz) = (x, z).
	inline bool NavApplySeparation(const NavGrid & grid, float x, float z, float vx, float vz,
		float & nx, float & nz)
	{
		nx = x;
		nz = z;

		const int curX = grid.CellOf(x);
		const int curZ = grid.CellOf(z);

		const float moveX[3] = { vx, vx, 0.0f };
		const float moveZ[3] = { vz, 0.0f, vz };

		for (int k = 0; k < 3; ++k)
		{
			if (moveX[k] == 0.0f && moveZ[k] == 0.0f)
				continue;

			const float tx = x + moveX[k];
			const float tz = z + moveZ[k];
			const int txCell = grid.CellOf(tx);
			const int tzCell = grid.CellOf(tz);

			bool ok = false;
			if (txCell == curX && tzCell == curZ)
			{
				ok = grid.Walk(curX, curZ);
			}
			else
			{
				NavCell a;
				a.x = curX;
				a.z = curZ;
				NavCell b;
				b.x = txCell;
				b.z = tzCell;
				ok = NavLineClear(grid, a, b);
			}

			if (ok)
			{
				nx = tx;
				nz = tz;
				return true;
			}
		}
		return false;
	}

	// Index into `candidates` of the first cell whose centre (grid.CellCenter) is >= minSepM
	// (inclusive) from every `others` point (float sqrt of dx*dx + dz*dz); when none qualifies, the
	// candidate whose nearest other point is farthest (earliest wins ties, strict `>`); -1 when
	// `candidates` is empty. otherCount == 0 (others may be nullptr): index 0.
	inline int NavPickSpaced(const NavGrid & grid, const std::vector<NavCell> & candidates,
		const NavFormPoint * others, size_t otherCount, float minSepM)
	{
		if (candidates.empty())
			return -1;
		if (others == nullptr || otherCount == 0)
			return 0;

		int bestIdx = 0;
		float bestNearest = 0.0f;
		for (size_t c = 0; c < candidates.size(); ++c)
		{
			const float px = grid.CellCenter(candidates[c].x);
			const float pz = grid.CellCenter(candidates[c].z);

			float nearest = 0.0f;
			bool first = true;
			for (size_t o = 0; o < otherCount; ++o)
			{
				const float dx = px - others[o].x;
				const float dz = pz - others[o].z;
				const float d = std::sqrt(dx * dx + dz * dz);
				if (first || d < nearest)
				{
					nearest = d;
					first = false;
				}
			}

			if (nearest >= minSepM)
				return (int)c;
			if (c == 0 || nearest > bestNearest)
			{
				bestNearest = nearest;
				bestIdx = (int)c;
			}
		}
		return bestIdx;
	}

	// Number of member pairs (i < j) closer than thresholdM (strict `<`, squared compare); 0 for
	// thresholdM <= 0 (checked explicitly: a negative threshold squared would otherwise count pairs)
	// or count < 2. MET-NAV-06 raw measure (the "for more than 2 s" part is the caller's).
	inline int NavCountStackedPairs(const NavFormPoint * members, size_t count, float thresholdM)
	{
		if (members == nullptr || count < 2 || thresholdM <= 0.0f)
			return 0;

		const float threshold2 = thresholdM * thresholdM;
		int pairs = 0;
		for (size_t i = 0; i < count; ++i)
		{
			for (size_t j = i + 1; j < count; ++j)
			{
				const float dx = members[i].x - members[j].x;
				const float dz = members[i].z - members[j].z;
				if (dx * dx + dz * dz < threshold2)
					++pairs;
			}
		}
		return pairs;
	}
}
