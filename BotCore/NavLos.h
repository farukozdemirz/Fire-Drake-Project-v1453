#pragma once

// Line of sight (F5-10; docs/12 s5, ADR-0006 Ek F5-10): a cheap approximate view test on top of
// the F5-01 navigation grid. The cell rule walks the segment through the event grid (a blocked
// intermediate cell breaks the line; the start and the end cell are exempt); the terrain rule
// samples the bilinear ground along the eye-height segment. Advisory (the default) never blocks
// an action, it only guides position selection. Pure logic: the standard library only, no server
// header, no global/static state and no clock. los_mesh, binding and T-NAV-LOS-01 are out of
// scope.

#include "NavTrack.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BotCore
{
	enum class NavLosMode { Advisory, Enforce };   // P-NAV-LOS-MODE (docs/12 s5); Advisory never blocks an action

	struct NavLosParams
	{
		NavLosMode mode = NavLosMode::Advisory;   // [O] docs/12 s5
		bool  terrain = true;                     // [A] apply the terrain rule
		float eyeM = 1.6f;                        // [A] eye (and target) height above the local ground
		float terrainSlackM = 0.25f;              // [A] ground must exceed the ray by MORE than this to block
		float sampleM = 2.0f;                     // [A] terrain sampling step along the ray; <= 0 disables the terrain rule
	};

	// Cell rule (los_grid). True when every cell whose OPEN interior the segment crosses has
	// Event == 1; the start and the end cell are exempt. Off-grid cells (Event == -1) block. A
	// segment that only touches a cell corner is not blocked; a segment lying exactly on a cell
	// boundary belongs to the cell on the larger side (floor). The computation uses double and
	// deliberately ignores Walk: ponds and slope pockets are a walkability matter, not a sight one.
	inline bool NavLosGridClear(const NavGrid & grid, float ax, float az, float bx, float bz)
	{
		const double unit = (double)grid.Unit();
		if (unit <= 0.0)
			return false;

		const int startX = grid.CellOf(ax);
		const int startZ = grid.CellOf(az);
		const int endX = grid.CellOf(bx);
		const int endZ = grid.CellOf(bz);
		if (startX == endX && startZ == endZ)
			return true;

		const double dx = (double)bx - (double)ax;
		const double dz = (double)bz - (double)az;
		const int stepX = dx > 0.0 ? 1 : (dx < 0.0 ? -1 : 0);
		const int stepZ = dz > 0.0 ? 1 : (dz < 0.0 ? -1 : 0);

		const double noCross = 1e300;
		double tMaxX = (stepX != 0)
			? (((double)(stepX > 0 ? startX + 1 : startX) * unit) - (double)ax) / dx
			: noCross;
		double tMaxZ = (stepZ != 0)
			? (((double)(stepZ > 0 ? startZ + 1 : startZ) * unit) - (double)az) / dz
			: noCross;
		const double tDeltaX = (stepX != 0) ? unit / std::fabs(dx) : noCross;
		const double tDeltaZ = (stepZ != 0) ? unit / std::fabs(dz) : noCross;

		int cx = startX;
		int cz = startZ;
		const int maxStepsX = endX > startX ? endX - startX : startX - endX;
		const int maxStepsZ = endZ > startZ ? endZ - startZ : startZ - endZ;
		const int maxSteps = maxStepsX + maxStepsZ;

		for (int i = 0; i < maxSteps; ++i)
		{
			if (tMaxX < tMaxZ)
			{
				cx += stepX;
				tMaxX += tDeltaX;
			}
			else if (tMaxZ < tMaxX)
			{
				cz += stepZ;
				tMaxZ += tDeltaZ;
			}
			else
			{
				// The segment passes through a cell corner: step diagonally; the two side cells
				// that are only touched at the corner are not considered.
				cx += stepX;
				cz += stepZ;
				tMaxX += tDeltaX;
				tMaxZ += tDeltaZ;
			}

			if (cx == endX && cz == endZ)
				return true;
			if (grid.Event(cx, cz) != 1)
				return false;
		}
		return true;
	}

	// Terrain rule: the eye-height segment (ground + eyeM at both ends) must not dip below the
	// bilinear ground in between. The ground is sampled every sampleM metres (ends excluded) and
	// blocks only when it is MORE than terrainSlackM above the segment at that sample.
	inline bool NavLosTerrainClear(const NavGrid & grid, float ax, float az, float bx, float bz,
		const NavLosParams & params)
	{
		if (!params.terrain || params.sampleM <= 0.0f)
			return true;

		const double dx = (double)bx - (double)ax;
		const double dz = (double)bz - (double)az;
		const double len = std::sqrt(dx * dx + dz * dz);
		if (len <= 0.0)
			return true;

		const double hA = (double)grid.HeightAt(ax, az) + (double)params.eyeM;
		const double hB = (double)grid.HeightAt(bx, bz) + (double)params.eyeM;
		const int raw = (int)std::ceil(len / (double)params.sampleM);
		const int samples = raw < 1 ? 1 : raw;

		for (int i = 1; i < samples; ++i)
		{
			const double t = (double)i / (double)samples;
			const double ground = (double)grid.HeightAt((float)((double)ax + t * dx),
				(float)((double)az + t * dz));
			const double ray = hA + t * (hB - hA);
			if (ground > ray + (double)params.terrainSlackM)
				return false;
		}
		return true;
	}

	// Both rules.
	inline bool NavLosClear(const NavGrid & grid, float ax, float az, float bx, float bz,
		const NavLosParams & params = NavLosParams())
	{
		return NavLosGridClear(grid, ax, az, bx, bz)
			&& NavLosTerrainClear(grid, ax, az, bx, bz, params);
	}

	// Advisory never blocks; Enforce allows only when the line of sight is clear.
	inline bool NavLosAllows(NavLosMode mode, bool clear)
	{
		return mode == NavLosMode::Advisory || clear;
	}

	// Nearest-to-`from` Walk cell of the ring around the target whose centre has a clear line of
	// sight to the target point. `scratch` is fully rewritten with ALL ring candidates (in
	// NavRingCells order). On failure `out` is left untouched.
	inline bool NavPickLosCell(const NavGrid & grid, float tx, float tz, float fromX, float fromZ,
		float ringMinM, float ringMaxM, const NavLosParams & params, std::vector<NavCell> & scratch,
		NavCell & out)
	{
		NavCell from;
		from.x = grid.CellOf(fromX);
		from.z = grid.CellOf(fromZ);

		NavRingCells(grid, tx, tz, ringMinM, ringMaxM, from, scratch);

		for (size_t i = 0; i < scratch.size(); ++i)
		{
			const NavCell & c = scratch[i];
			if (NavLosClear(grid, grid.CellCenter(c.x), grid.CellCenter(c.z), tx, tz, params))
			{
				out = c;
				return true;
			}
		}
		return false;
	}
}
