#pragma once

// Path smoothing (F5-03; docs/12 s4.1, ADR-0006 Ek F5-03) on top of the F5-01 navigation grid and
// the F5-02 cell path. Pure logic: the standard library only, no server header, no global/static
// state. NavLineClear re-uses NavGrid::EdgeOpen for every step, so walkability, slope and the no
// corner cutting rule stay in one place; a smoothed segment is always a walk A* could take too.

#include "NavPath.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace BotCore
{
	// True when the Bresenham walk from `a` to `b` exists and every step is an open edge.
	inline bool NavLineClear(const NavGrid & grid, NavCell a, NavCell b)
	{
		if (!grid.Walk(a.x, a.z) || !grid.Walk(b.x, b.z))
			return false;

		// Canonical order (x, then z): the tie-breaking of Bresenham depends on the direction, so
		// putting the smaller cell first makes the result symmetric in the two arguments.
		if (b.x < a.x || (b.x == a.x && b.z < a.z))
		{
			const NavCell t = a;
			a = b;
			b = t;
		}
		if (a == b)
			return true;

		const int dx = b.x - a.x;              // non-negative after the canonical ordering
		const int dz = b.z - a.z;              // signed
		const int ax = dx < 0 ? -dx : dx;
		const int az = dz < 0 ? -dz : dz;
		const int sx = a.x < b.x ? 1 : -1;
		const int sz = a.z < b.z ? 1 : -1;
		int err = ax - az;

		int x = a.x;
		int z = a.z;
		while (x != b.x || z != b.z)
		{
			const int e2 = 2 * err;
			int mx = 0;
			int mz = 0;
			if (e2 > -az)
			{
				err -= az;
				mx = sx;
			}
			if (e2 < ax)
			{
				err += ax;
				mz = sz;
			}
			// Exactly one neighbour edge per step; a diagonal step also checks the orthogonal
			// neighbours through EdgeOpen, so no corner is cut here.
			if (!grid.EdgeOpen(x, z, mx, mz))
				return false;
			x += mx;
			z += mz;
		}
		return true;
	}

	struct NavSmoothParams
	{
		// P-NAV-SMOOTH-LOOKAHEAD [A] (ADR-0006 Ek F5-03): how many path cells ahead of the
		// current anchor are tried as the next waypoint. Bounds the cost; values < 1 act as 1
		// (no smoothing: every path cell is kept).
		int maxLookahead = 64;
	};

	struct NavSmoothResult
	{
		std::vector<NavCell> waypoints;   // subsequence of the input path, first and last kept
		float length = 0.0f;              // metres: sum of unit * Euclid distance between waypoints
	};

	// `path` must be a path from NavPathfinder::Find (consecutive cells are neighbours and the
	// steps are open edges); it is trusted, not re-validated. `out` is fully overwritten and
	// must not alias `path`. Deterministic. Empty path -> empty waypoints, length 0.
	inline void NavSmoothPath(const NavGrid & grid, const std::vector<NavCell> & path,
		const NavSmoothParams & params, NavSmoothResult & out)
	{
		out.waypoints.clear();
		out.length = 0.0f;

		if (path.empty())
			return;

		out.waypoints.reserve(path.size());
		out.waypoints.push_back(path[0]);

		const int last = (int)path.size() - 1;
		if (last == 0)
			return;

		int look = params.maxLookahead;
		if (look < 1)
			look = 1;

		int i = 0;
		while (i < last)
		{
			int j = i + look;
			if (j > last)
				j = last;
			// Largest visible waypoint at most `look` cells ahead; an immediate successor
			// (j == i + 1) is a trusted path edge, so NavLineClear is not queried for it.
			while (j > i + 1 && !NavLineClear(grid, path[i], path[j]))
				--j;
			out.waypoints.push_back(path[j]);
			i = j;
		}

		float total = 0.0f;
		for (size_t k = 1; k < out.waypoints.size(); ++k)
		{
			const int ddx = out.waypoints[k].x - out.waypoints[k - 1].x;
			const int ddz = out.waypoints[k].z - out.waypoints[k - 1].z;
			total += grid.Unit() * std::sqrt((float)(ddx * ddx + ddz * ddz));
		}
		out.length = total;
	}
}
