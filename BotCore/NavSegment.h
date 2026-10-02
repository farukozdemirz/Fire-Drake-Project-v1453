#pragma once

// Chord (single movement packet step) walkability check (F5-50; docs/12 s13.1, CLI-08).
// Pure logic: the standard library only, no server header, no global/static state and no dynamic
// memory. Two layers:
//   (a) the mandatory conservative supercover of the closed segment (Amanatides-Woo; cell
//       corners/vertices and boundary-hugging segments included): every touched cell must be Walk.
//       This layer is always on and yields Ok / OutOfBounds / BlockedCell;
//   (b) the optional slope / no-corner-cut rule (checkSlope), along the same cell path
//       NavLineClear validates, i.e. the canonical Bresenham walk between the endpoint cells
//       using NavGrid::EdgeOpen. It is only consistent with the planner for full segments whose
//       endpoints are planner waypoints; it gives no guarantee for arbitrary sub-chords, so it
//       defaults off and is opted into with checkSlope = true.
// A rejected chord is never sent; the executor side guard is F5-55.

#include "NavGrid.h"

#include <cmath>

namespace BotCore
{
	enum class NavSegmentVerdict
	{
		Ok,
		OutOfBounds,
		BlockedCell,
		SlopeTooSteep
	};

	struct NavSegmentResult
	{
		NavSegmentVerdict verdict = NavSegmentVerdict::Ok;
		int cellX = 0;          // first offending cell; -1 for OutOfBounds
		int cellZ = 0;
		int cellsTouched = 0;   // cells examined until the verdict (statistics)
	};

	// Walks the cells of the straight segment (ax,az) -> (bx,bz), world metres, with an
	// Amanatides-Woo traversal. A vertex (both axes on a boundary) adds both orthogonal cells too,
	// so a blocked cell touched only at a corner is caught. `double` arithmetic, no allocation.
	// The two endpoints are swapped into canonical (x, then z) order so the verdict and the
	// reported cell are symmetric in the segment direction.
	inline NavSegmentResult NavCheckSegment(const NavGrid & grid, double ax, double az, double bx, double bz, bool checkSlope = false)
	{
		NavSegmentResult res;

		const int n = grid.Size();
		const double unit = (double)grid.Unit();
		if (n < 1 || unit <= 0.0)
		{
			res.verdict = NavSegmentVerdict::OutOfBounds;
			res.cellX = -1;
			res.cellZ = -1;
			return res;
		}

		// Reject non-finite or absurdly large inputs before any (int) conversion; NaN/+-inf and
		// out-of-int-range coordinates are the caller's bug, and fail-closed is OutOfBounds.
		const double limit = 1e9;
		if (!std::isfinite(ax) || !std::isfinite(az) || !std::isfinite(bx) || !std::isfinite(bz) ||
			std::fabs(ax / unit) > limit || std::fabs(az / unit) > limit ||
			std::fabs(bx / unit) > limit || std::fabs(bz / unit) > limit)
		{
			res.verdict = NavSegmentVerdict::OutOfBounds;
			res.cellX = -1;
			res.cellZ = -1;
			return res;
		}

		if (bx < ax || (bx == ax && bz < az))
		{
			const double t = ax;
			ax = bx;
			bx = t;
			const double t2 = az;
			az = bz;
			bz = t2;
		}

		int touched = 0;
		bool failed = false;

		auto fail = [&](NavSegmentVerdict verdict, int cx, int cz)
		{
			if (!failed)
			{
				res.verdict = verdict;
				res.cellX = cx;
				res.cellZ = cz;
				res.cellsTouched = touched;
				failed = true;
			}
		};

		auto visit = [&](int cx, int cz) -> bool
		{
			++touched;
			if (cx < 0 || cz < 0 || cx >= n || cz >= n)
			{
				fail(NavSegmentVerdict::OutOfBounds, -1, -1);
				return false;
			}
			if (!grid.Walk(cx, cz))
			{
				fail(NavSegmentVerdict::BlockedCell, cx, cz);
				return false;
			}
			return true;
		};

		// Tells whether a cell coordinate sits on a cell boundary and on which side: -1 when the
		// floor cell is the upper one, +1 when it is the lower one, 0 when strictly inside.
		auto boundaryShift = [](double cellCoord) -> int
		{
			const double frac = cellCoord - std::floor(cellCoord);
			if (frac < 1e-9)
				return -1;
			if (frac > 1.0 - 1e-9)
				return +1;
			return 0;
		};

		int x = (int)std::floor(ax / unit);
		int z = (int)std::floor(az / unit);
		const int startCellX = x;
		const int startCellZ = z;

		if (ax == bx && az == bz)
		{
			visit(x, z);
			res.cellsTouched = touched;
			return res;
		}

		// Start cell plus the cell on the far side of a boundary the start sits on.
		const int xStartShift = boundaryShift(ax / unit);
		const int zStartShift = boundaryShift(az / unit);

		const double dxw = bx - ax;
		const double dzw = bz - az;
		const int stepX = (dxw > 0.0) ? 1 : ((dxw < 0.0) ? -1 : 0);
		const int stepZ = (dzw > 0.0) ? 1 : ((dzw < 0.0) ? -1 : 0);

		// A segment lying exactly on a boundary keeps the two adjacent columns/rows for its whole
		// length, so the partner cell is walked alongside every main cell.
		const int xPartner = (stepX == 0) ? boundaryShift(ax / unit) : 0;
		const int zPartner = (stepZ == 0) ? boundaryShift(az / unit) : 0;

		auto visitMain = [&](int cx, int cz) -> bool
		{
			if (!visit(cx, cz))
				return false;
			if (xPartner != 0 && !visit(cx + xPartner, cz))
				return false;
			if (zPartner != 0 && !visit(cx, cz + zPartner))
				return false;
			if (xPartner != 0 && zPartner != 0 && !visit(cx + xPartner, cz + zPartner))
				return false;
			return true;
		};

		{
			const int startX[2] = { x, x + xStartShift };
			const int startZ[2] = { z, z + zStartShift };
			const int nc = (xStartShift != 0) ? 2 : 1;
			const int nr = (zStartShift != 0) ? 2 : 1;
			for (int i = 0; i < nc; ++i)
			{
				for (int j = 0; j < nr; ++j)
				{
					if (!visit(startX[i], startZ[j]))
						return res;
				}
			}
		}

		const double inf = 1e300;
		double tMaxX = inf;
		double tDeltaX = inf;
		double tMaxZ = inf;
		double tDeltaZ = inf;
		if (stepX != 0)
		{
			const double nextBoundary = (stepX > 0) ? ((double)(x + 1) * unit) : ((double)x * unit);
			tMaxX = (nextBoundary - ax) / dxw;
			tDeltaX = unit / std::fabs(dxw);
		}
		if (stepZ != 0)
		{
			const double nextBoundary = (stepZ > 0) ? ((double)(z + 1) * unit) : ((double)z * unit);
			tMaxZ = (nextBoundary - az) / dzw;
			tDeltaZ = unit / std::fabs(dzw);
		}

		const int ex = (int)std::floor(bx / unit);
		const int ez = (int)std::floor(bz / unit);

		const int guardMax = 2 * ((ex > x ? ex - x : x - ex) + (ez > z ? ez - z : z - ez) + 4);
		int guard = 0;

		// Traversal ends on the segment parameter, not on reaching the end cell. The segment is
		// the parameter range [0,1], so a boundary crossing beyond t = 1 (plus the tiny vertex
		// tolerance) lies off the chord; this keeps an end vertex (both axes at t = 1) from being
		// stepped past. The mandatory end-cell-plus-far-side visit after the loop still covers the
		// final cell and the other side of a boundary it sits on.
		while (true)
		{
			const double tNext = (tMaxX < tMaxZ) ? tMaxX : tMaxZ;
			if (tNext > 1.0 + 1e-9)
				break;

			if (++guard > guardMax)
			{
				// numerical safety net; never reached by a correct traversal
				fail(NavSegmentVerdict::OutOfBounds, -1, -1);
				return res;
			}

			if (tMaxX < tMaxZ - 1e-9)
			{
				x += stepX;
				tMaxX += tDeltaX;
				if (!visitMain(x, z))
					return res;
			}
			else if (tMaxZ < tMaxX - 1e-9)
			{
				z += stepZ;
				tMaxZ += tDeltaZ;
				if (!visitMain(x, z))
					return res;
			}
			else
			{
				// Vertex: the segment touches the two orthogonal cells only at that corner.
				const int nx = x + stepX;
				const int nz = z + stepZ;
				if (!visit(nx, z))
					return res;
				if (!visit(x, nz))
					return res;
				x = nx;
				z = nz;
				tMaxX += tDeltaX;
				tMaxZ += tDeltaZ;
				if (!visitMain(x, z))
					return res;
			}
		}

		// End cell plus the cell on the far side of a boundary the end sits on.
		{
			const int endXShift = boundaryShift(bx / unit);
			const int endZShift = boundaryShift(bz / unit);
			const int endX[2] = { ex, ex + endXShift };
			const int endZ[2] = { ez, ez + endZShift };
			const int nc = (endXShift != 0) ? 2 : 1;
			const int nr = (endZShift != 0) ? 2 : 1;
			for (int i = 0; i < nc; ++i)
			{
				for (int j = 0; j < nr; ++j)
				{
					if (!visit(endX[i], endZ[j]))
						return res;
				}
			}
		}

		if (checkSlope)
		{
			// Slope and no-corner-cut along the canonical Bresenham cell walk (same path the
			// planner validates with NavLineClear): an EdgeOpen failure is a slope failure,
			// because the supercover pass above already found every non-Walk cell.
			int bx0 = startCellX;
			int bz0 = startCellZ;
			int bx1 = ex;
			int bz1 = ez;
			if (bx1 < bx0 || (bx1 == bx0 && bz1 < bz0))
			{
				const int t = bx0;
				bx0 = bx1;
				bx1 = t;
				const int t2 = bz0;
				bz0 = bz1;
				bz1 = t2;
			}

			if (bx0 != bx1 || bz0 != bz1)
			{
				const int ddx = bx1 - bx0;
				const int ddz = bz1 - bz0;
				const int adx = ddx < 0 ? -ddx : ddx;
				const int adz = ddz < 0 ? -ddz : ddz;
				const int sx = bx0 < bx1 ? 1 : -1;
				const int sz = bz0 < bz1 ? 1 : -1;
				int err = adx - adz;
				int cx = bx0;
				int cz = bz0;
				while (cx != bx1 || cz != bz1)
				{
					const int e2 = 2 * err;
					int mx = 0;
					int mz = 0;
					if (e2 > -adz)
					{
						err -= adz;
						mx = sx;
					}
					if (e2 < adx)
					{
						err += adx;
						mz = sz;
					}
					if (!grid.EdgeOpen(cx, cz, mx, mz))
					{
						fail(NavSegmentVerdict::SlopeTooSteep, cx + mx, cz + mz);
						return res;
					}
					cx += mx;
					cz += mz;
				}
			}
		}

		res.cellsTouched = touched;
		return res;
	}

	// float convenience wrapper for the executor side (F5-55).
	inline NavSegmentResult NavCheckStep(const NavGrid & grid, float x0, float z0, float x1, float z1, bool checkSlope = false)
	{
		return NavCheckSegment(grid, (double)x0, (double)z0, (double)x1, (double)z1, checkSlope);
	}
}
