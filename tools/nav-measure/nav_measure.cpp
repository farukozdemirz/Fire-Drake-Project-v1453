// nav_measure: reproducible measurements on the BotCore navigation headers (docs/reports/degerlendirme-2026-10-02*.md).
//
// Replaces the throw-away scripts of the 2026-10-02 evaluation. Build and run through tools/nav-measure.sh.
// Pure host tool: it only includes BotCore/Nav*.h (standard library only) and reads a .navgrid file
// (python3 tools/nav-export.py). It never talks to a server. Numbers printed here are host measurements
// (g++ -O2 on WSL by default), NOT the MSVC Release build: use them for ratios and pass/fail structure,
// repeat acceptance timings with the MSVC Release unit tests.
//
//   nav_measure <section> [--navgrid PATH] [--seed N] [--n COUNT]
//   sections: segments | smoothing | synthetic | velocity | velocity-robust | arena | budget | budget-scheduled | stuck | progress | all
//
// Every section prints one "KEY value ..." line per result so a CI-less wrapper can grep them.

#include "BotCore/NavGrid.h"
#include "BotCore/NavDanger.h"
#include "BotCore/NavPath.h"
#include "BotCore/NavSmooth.h"
#include "BotCore/NavTrack.h"
#include "BotCore/NavStuck.h"
#include "BotCore/NavBudget.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace BotCore;
using Clock = std::chrono::steady_clock;

namespace
{
	double MsSince(Clock::time_point t0)
	{
		return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
	}

	double Pct(std::vector<double> v, double p)
	{
		if (v.empty())
			return 0.0;
		std::sort(v.begin(), v.end());
		size_t idx = (size_t)std::ceil(p / 100.0 * (double)v.size());
		if (idx < 1)
			idx = 1;
		if (idx > v.size())
			idx = v.size();
		return v[idx - 1];
	}

	// Exact conservative super-cover: every cell whose CLOSED square the segment touches (a segment passing a cell
	// corner or running along a cell edge touches all cells sharing that corner/edge). World metres in, cells out.
	void TouchedCells(const NavGrid & g, double ax, double az, double bx, double bz, std::vector<std::pair<int, int>> & out)
	{
		out.clear();
		const double u = (double)g.Unit();
		const double x0 = ax / u, z0 = az / u, x1 = bx / u, z1 = bz / u;
		const double eps = 1e-9;

		auto addAround = [&](double px, double pz)
		{
			// endpoint exactly on a grid line: the neighbours across that line are touched too
			const int cx = (int)std::floor(px);
			const int cz = (int)std::floor(pz);
			const bool onX = std::fabs(px - std::round(px)) < eps;
			const bool onZ = std::fabs(pz - std::round(pz)) < eps;
			const int rx = (int)std::round(px);
			const int rz = (int)std::round(pz);
			if (onX && onZ)
			{
				for (int dx = -1; dx <= 0; ++dx)
					for (int dz = -1; dz <= 0; ++dz)
						out.push_back({ rx + dx, rz + dz });
			}
			else if (onX)
			{
				out.push_back({ rx - 1, cz });
				out.push_back({ rx, cz });
			}
			else if (onZ)
			{
				out.push_back({ cx, rz - 1 });
				out.push_back({ cx, rz });
			}
			else
			{
				out.push_back({ cx, cz });
			}
		};

		addAround(x0, z0);
		addAround(x1, z1);

		const double dx = x1 - x0;
		const double dz = z1 - z0;
		const int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
		const int sz = dz > 0 ? 1 : (dz < 0 ? -1 : 0);
		const bool x0Int = std::fabs(x0 - std::round(x0)) < eps, z0Int = std::fabs(z0 - std::round(z0)) < eps;
		const bool x1Int = std::fabs(x1 - std::round(x1)) < eps, z1Int = std::fabs(z1 - std::round(z1)) < eps;
		// a start/end exactly on a grid line while moving towards the lower cell belongs to the lower cell
		int cx = (sx < 0 && x0Int) ? (int)std::round(x0) - 1 : (int)std::floor(x0);
		int cz = (sz < 0 && z0Int) ? (int)std::round(z0) - 1 : (int)std::floor(z0);
		const int ex = (sx < 0 && x1Int) ? (int)std::round(x1) - 1 : (int)std::floor(x1);
		const int ez = (sz < 0 && z1Int) ? (int)std::round(z1) - 1 : (int)std::floor(z1);
		const double inf = 1e300;
		double tDx = sx != 0 ? 1.0 / std::fabs(dx) : inf;
		double tDz = sz != 0 ? 1.0 / std::fabs(dz) : inf;
		double tMx = sx > 0 ? ((double)(cx + 1) - x0) / dx : (sx < 0 ? (x0 - (double)cx) / (-dx) : inf);
		double tMz = sz > 0 ? ((double)(cz + 1) - z0) / dz : (sz < 0 ? (z0 - (double)cz) / (-dz) : inf);

		int guard = 0;
		while (!(cx == ex && cz == ez) && guard++ < 100000)
		{
			out.push_back({ cx, cz });
			if (std::fabs(tMx - tMz) < eps)
			{
				out.push_back({ cx + sx, cz });
				out.push_back({ cx, cz + sz });
				cx += sx;
				cz += sz;
				tMx += tDx;
				tMz += tDz;
			}
			else if (tMx < tMz)
			{
				cx += sx;
				tMx += tDx;
			}
			else
			{
				cz += sz;
				tMz += tDz;
			}
		}
		out.push_back({ cx, cz });
	}

	// true (and the offending cell) when any touched cell is not Walk.
	bool Blocked(const NavGrid & g, double ax, double az, double bx, double bz, std::pair<int, int> * where = nullptr)
	{
		static thread_local std::vector<std::pair<int, int>> cells;
		TouchedCells(g, ax, az, bx, bz, cells);
		for (size_t i = 0; i < cells.size(); ++i)
		{
			if (!g.Walk(cells[i].first, cells[i].second))
			{
				if (where != nullptr)
					*where = cells[i];
				return true;
			}
		}
		return false;
	}

	struct Ctx
	{
		NavGrid grid;
		std::vector<NavCell> walk;
		uint32_t seed = 20261002;
		int n = 1000;
		float waterT = -1.0f;      // F5-60: basin threshold (Walk cells below this are proxy water candidates)
		int waterMin = 200;        // F5-60: a basin needs at least this many cells
		std::string maskPath;      // F5-60: optional verified water mask (n*n bytes, x*n+z, 0/1)
	};

	NavCell NearestWalk(const NavGrid & g, float wx, float wz)
	{
		const int cx = g.CellOf(wx), cz = g.CellOf(wz);
		NavCell best{ cx, cz };
		double bd = 1e18;
		for (int r = 0; r < 40 && bd > 1e17; ++r)
			for (int x = cx - r; x <= cx + r; ++x)
				for (int z = cz - r; z <= cz + r; ++z)
					if (g.Walk(x, z))
					{
						const double d = (double)(x - cx) * (x - cx) + (double)(z - cz) * (z - cz);
						if (d < bd) { bd = d; best = NavCell{ x, z }; }
					}
		return best;
	}

	NavCell RandomNear(const Ctx & c, std::mt19937 & rng, NavCell a, int cheb)
	{
		for (int tries = 0; tries < 200000; ++tries)
		{
			NavCell b = c.walk[rng() % c.walk.size()];
			if (b != a && std::abs(b.x - a.x) <= cheb && std::abs(b.z - a.z) <= cheb)
				return b;
		}
		return a;
	}

	// ---------- section: segments / smoothing (the wall-check question: raw path, smoothing, or the check itself?) ----------
	void Smoothing(Ctx & c)
	{
		std::mt19937 rng(c.seed);
		NavPathfinder pf;
		NavPathResult res;
		NavSearchParams sp;
		long paths = 0, rawEdges = 0, rawBad = 0, smoothSegs = 0, smoothBad = 0, pathsWithSmoothBad = 0, chords = 0, chordBad = 0;
		long pairsTested = 0, lineClearTrue = 0, falsePositive = 0;
		int examples = 0, chordExamples = 0;
		for (int q = 0; q < c.n; ++q)
		{
			const int cheb = (q % 3 == 0) ? 64 : (q % 3 == 1 ? 150 : 40);
			NavCell a = c.walk[rng() % c.walk.size()];
			NavCell b = RandomNear(c, rng, a, cheb);
			if (a == b)
				continue;
			pf.Find(c.grid, a, b, sp, res);
			if (res.status != NavPathStatus::Found)
				continue;
			++paths;

			// (1) raw A* path: every cell-centre to cell-centre edge
			for (size_t i = 1; i < res.cells.size(); ++i)
			{
				++rawEdges;
				if (Blocked(c.grid, c.grid.CellCenter(res.cells[i - 1].x), c.grid.CellCenter(res.cells[i - 1].z),
					c.grid.CellCenter(res.cells[i].x), c.grid.CellCenter(res.cells[i].z)))
					++rawBad;
			}

			// (2) smoothed polyline segments
			NavSmoothResult sm;
			NavSmoothPath(c.grid, res.cells, NavSmoothParams(), sm);
			bool pathBad = false;
			for (size_t i = 1; i < sm.waypoints.size(); ++i)
			{
				++smoothSegs;
				std::pair<int, int> where;
				const double ax = c.grid.CellCenter(sm.waypoints[i - 1].x), az = c.grid.CellCenter(sm.waypoints[i - 1].z);
				const double bx = c.grid.CellCenter(sm.waypoints[i].x), bz = c.grid.CellCenter(sm.waypoints[i].z);
				if (Blocked(c.grid, ax, az, bx, bz, &where))
				{
					++smoothBad;
					pathBad = true;
					if (examples < 5)
					{
						std::printf("EXAMPLE smoothing segment (%.1f,%.1f)->(%.1f,%.1f) cells (%d,%d)->(%d,%d) touches non-walk cell (%d,%d)\n",
							ax, az, bx, bz, sm.waypoints[i - 1].x, sm.waypoints[i - 1].z, sm.waypoints[i].x, sm.waypoints[i].z,
							where.first, where.second);
						++examples;
					}
				}
			}
			if (pathBad)
				++pathsWithSmoothBad;

			// (3) 6.75 m packet chords along the smoothed polyline
			double px = c.grid.CellCenter(sm.waypoints[0].x), pz = c.grid.CellCenter(sm.waypoints[0].z);
			double lastX = px, lastZ = pz, carry = 0.0;
			for (size_t i = 1; i < sm.waypoints.size(); ++i)
			{
				const double tx = c.grid.CellCenter(sm.waypoints[i].x), tz = c.grid.CellCenter(sm.waypoints[i].z);
				const double seg = std::sqrt((tx - px) * (tx - px) + (tz - pz) * (tz - pz));
				const double ux = seg > 0 ? (tx - px) / seg : 0, uz = seg > 0 ? (tz - pz) / seg : 0;
				double pos = 0.0;
				while (carry + (seg - pos) >= 6.75)
				{
					pos += 6.75 - carry;
					carry = 0.0;
					const double nx = px + ux * pos, nz = pz + uz * pos;
					++chords;
					std::pair<int, int> chordWhere;
					if (Blocked(c.grid, lastX, lastZ, nx, nz, &chordWhere))
					{
						++chordBad;
						if (chordExamples < 5)
						{
							// the polyline turns at waypoint i-1..i: the chord cuts the bend
							std::printf("EXAMPLE packet chord (%.2f,%.2f)->(%.2f,%.2f) touches non-walk cell (%d,%d); path cells %d,%d -> %d,%d (%zu waypoints)\n",
								lastX, lastZ, nx, nz, chordWhere.first, chordWhere.second, a.x, a.z, b.x, b.z, sm.waypoints.size());
							++chordExamples;
						}
					}
					lastX = nx;
					lastZ = nz;
				}
				carry += seg - pos;
				px = tx;
				pz = tz;
			}

			// (4) the smoothing check itself: NavLineClear for pairs along this path (the pairs smoothing may accept)
			for (size_t i = 0; i + 2 < res.cells.size(); i += 3)
			{
				for (size_t j = i + 2; j < res.cells.size() && j <= i + 64; j += 5)
				{
					++pairsTested;
					if (!NavLineClear(c.grid, res.cells[i], res.cells[j]))
						continue;
					++lineClearTrue;
					std::pair<int, int> where;
					if (Blocked(c.grid, c.grid.CellCenter(res.cells[i].x), c.grid.CellCenter(res.cells[i].z),
						c.grid.CellCenter(res.cells[j].x), c.grid.CellCenter(res.cells[j].z), &where))
					{
						++falsePositive;
						if (examples < 8)
						{
							std::printf("EXAMPLE NavLineClear true but super-cover blocked: cells (%d,%d)->(%d,%d) world (%.1f,%.1f)->(%.1f,%.1f) offending cell (%d,%d)\n",
								res.cells[i].x, res.cells[i].z, res.cells[j].x, res.cells[j].z,
								c.grid.CellCenter(res.cells[i].x), c.grid.CellCenter(res.cells[i].z),
								c.grid.CellCenter(res.cells[j].x), c.grid.CellCenter(res.cells[j].z), where.first, where.second);
							++examples;
						}
					}
				}
			}
		}
		std::printf("SMOOTHING paths=%ld raw_edges=%ld raw_bad=%ld smooth_segments=%ld smooth_bad=%ld paths_with_smooth_bad=%ld chords=%ld chord_bad=%ld\n",
			paths, rawEdges, rawBad, smoothSegs, smoothBad, pathsWithSmoothBad, chords, chordBad);
		std::printf("LINECLEAR pairs=%ld clear=%ld false_positive=%ld\n", pairsTested, lineClearTrue, falsePositive);

		// random straight pairs: how often is an arbitrary straight chord blocked (the executor's straight-line step)
		long straight = 0, straightBlocked = 0;
		int straightExamples = 0;
		for (int q = 0; q < c.n; ++q)
		{
			NavCell a = c.walk[rng() % c.walk.size()];
			NavCell b = RandomNear(c, rng, a, 3);   // one or two packet steps away
			if (a == b)
				continue;
			++straight;
			std::pair<int, int> sw;
			if (Blocked(c.grid, c.grid.CellCenter(a.x), c.grid.CellCenter(a.z), c.grid.CellCenter(b.x), c.grid.CellCenter(b.z), &sw))
			{
				++straightBlocked;
				if (straightExamples < 3)
				{
					std::printf("EXAMPLE straight step (what a bare /bot move packet does) (%.1f,%.1f)->(%.1f,%.1f) both cells walkable, touches non-walk cell (%d,%d)\n",
						c.grid.CellCenter(a.x), c.grid.CellCenter(a.z), c.grid.CellCenter(b.x), c.grid.CellCenter(b.z), sw.first, sw.second);
					++straightExamples;
				}
			}
		}
		std::printf("STRAIGHT pairs=%ld blocked=%ld\n", straight, straightBlocked);
	}


	// ---------- synthetic: can NavLineClear (Bresenham + EdgeOpen) accept a segment that touches a blocked cell? ----------
	void Synthetic(Ctx &)
	{
		const int n = 13;                 // blocked border, open 9x9 interior (cells 2..10), one extra blocked cell per trial
		long trials = 0, clearTrue = 0, falsePositive = 0;
		int shown = 0;
		for (int bxCell = 3; bxCell <= 9; ++bxCell)
			for (int bzCell = 3; bzCell <= 9; ++bzCell)
			{
				std::vector<int16_t> ev((size_t)n * n, 0);
				for (int x = 2; x <= 10; ++x)
					for (int z = 2; z <= 10; ++z)
						ev[(size_t)x * n + z] = 1;
				ev[(size_t)bxCell * n + bzCell] = 0;
				NavGrid g;
				g.Init(n, 4.0f, ev, std::vector<float>((size_t)n * n, 0.0f));
				g.Build();
				for (int ax = 2; ax <= 10; ++ax)
					for (int az = 2; az <= 10; ++az)
						for (int bx = 2; bx <= 10; ++bx)
							for (int bz = 2; bz <= 10; ++bz)
							{
								if (!g.Walk(ax, az) || !g.Walk(bx, bz) || (ax == bx && az == bz))
									continue;
								++trials;
								if (!NavLineClear(g, NavCell{ ax, az }, NavCell{ bx, bz }))
									continue;
								++clearTrue;
								std::pair<int, int> w;
								if (Blocked(g, g.CellCenter(ax), g.CellCenter(az), g.CellCenter(bx), g.CellCenter(bz), &w))
								{
									++falsePositive;
									if (shown < 3)
									{
										std::printf("EXAMPLE synthetic 13x13 grid, single blocked cell (%d,%d): NavLineClear((%d,%d)->(%d,%d)) is true but the segment touches (%d,%d)\n",
											bxCell, bzCell, ax, az, bx, bz, w.first, w.second);
										++shown;
									}
								}
							}
			}
		std::printf("SYNTHETIC single_block trials=%ld nav_line_clear_true=%ld false_positive=%ld\n", trials, clearTrue, falsePositive);

		// random grids with several blocked cells (checkerboard-like clutter is where a non-super-cover line check can slip)
		std::mt19937 rng(99);
		long t2 = 0, c2 = 0, f2 = 0;
		int shown2 = 0;
		for (int round = 0; round < 3000; ++round)
		{
			std::vector<int16_t> ev((size_t)n * n, 0);
			for (int x = 2; x <= 10; ++x)
				for (int z = 2; z <= 10; ++z)
					ev[(size_t)x * n + z] = (rng() % 100 < 22) ? 0 : 1;
			NavGrid g;
			g.Init(n, 4.0f, ev, std::vector<float>((size_t)n * n, 0.0f));
			g.Build();
			for (int k = 0; k < 40; ++k)
			{
				const int ax = 2 + (int)(rng() % 9), az = 2 + (int)(rng() % 9), bx = 2 + (int)(rng() % 9), bz = 2 + (int)(rng() % 9);
				if (!g.Walk(ax, az) || !g.Walk(bx, bz) || (ax == bx && az == bz))
					continue;
				++t2;
				if (!NavLineClear(g, NavCell{ ax, az }, NavCell{ bx, bz }))
					continue;
				++c2;
				std::pair<int, int> w;
				if (Blocked(g, g.CellCenter(ax), g.CellCenter(az), g.CellCenter(bx), g.CellCenter(bz), &w))
				{
					++f2;
					if (shown2 < 3)
					{
						std::string rows;
						for (int z = 10; z >= 2; --z)
						{
							for (int x = 2; x <= 10; ++x)
								rows += g.Walk(x, z) ? '.' : '#';
							rows += '/';
						}
						std::printf("EXAMPLE synthetic random grid (rows z=10..2 left to right x=2..10): %s NavLineClear((%d,%d)->(%d,%d)) true but segment touches blocked (%d,%d)\n",
							rows.c_str(), ax, az, bx, bz, w.first, w.second);
						++shown2;
					}
				}
			}
		}
		std::printf("SYNTHETIC random_clutter trials=%ld nav_line_clear_true=%ld false_positive=%ld\n", t2, c2, f2);
	}

	// ---------- velocity ----------
	void Velocity(Ctx &)
	{
		const int cadences[] = { 500, 1000, 1500, 1540, 2000 };
		for (int cadence : cadences)
		{
			int zero = 0, total = 0;
			for (int64_t now = 1000 + 20LL * cadence; now < 1000 + 39LL * cadence; now += 100)
			{
				NavTargetTracker t;
				for (int64_t ot = 1000; ot <= now; ot += cadence)
					t.Observe(ot, 1000.0f + 4.5f * (float)ot / 1000.0f, 900.0f);
				float vx, vz;
				t.Velocity(now, 1000, 100, vx, vz);   // F5-04 defaults
				++total;
				if (vx == 0.0f && vz == 0.0f)
					++zero;
			}
			std::printf("VELOCITY cadence_ms=%d window_ms=1000 ticks=%d zero=%d zero_pct=%.1f\n", cadence, total, zero, 100.0 * zero / total);
		}
		// jittered packet intervals (uniform 1300..1900 ms) with a 4000/400 window: informational baseline for F5-52/F5-56
		std::mt19937 rng(7);
		int zero = 0, total = 0;
		double errSum = 0.0, errMax = 0.0;
		std::vector<std::pair<int64_t, float>> obs;
		int64_t t = 1000;
		while (t < 120000)
		{
			obs.push_back({ t, 1000.0f + 4.5f * (float)t / 1000.0f });
			t += 1300 + (int)(rng() % 601);
		}
		for (int64_t now = 20000; now < 100000; now += 100)
		{
			NavTargetTracker tr;
			for (const auto & o : obs)
				if (o.first <= now)
					tr.Observe(o.first, o.second, 900.0f);
			float vx, vz;
			tr.Velocity(now, 4000, 400, vx, vz);
			++total;
			if (vx == 0.0f && vz == 0.0f)
				++zero;
			else
			{
				const double e = std::fabs(vx - 4.5) / 4.5;
				errSum += e;
				errMax = std::max(errMax, e);
			}
		}
		std::printf("VELOCITY jitter=1300..1900ms window_ms=4000 ticks=%d zero=%d mean_rel_err=%.3f max_rel_err=%.3f\n",
			total, zero, total > zero ? errSum / (total - zero) : 0.0, errMax);
	}

	// ---------- velocity-robust (F5-56): arrival jitter/bunching, variable interval, packet loss ----------
	struct VRObs
	{
		int64_t t = 0;
		float x = 0.0f;
		int16_t speed = -1;
	};

	struct VRStats
	{
		int ticks = 0;
		int zero = 0;
		double p50 = 0.0;
		double p95 = 0.0;
		double max = 0.0;
	};

	float VRQuant(double v)
	{
		return (float)(std::floor(v * 10.0 + 0.5) / 10.0);
	}

	std::vector<VRObs> VRCadence(uint32_t seed, int intervalMs, double jitterMs, double bunchProb)
	{
		std::vector<int64_t> sends;
		for (int64_t t = 1000; t < 120000; t += intervalMs)
			sends.push_back(t);

		std::mt19937 rng(seed);
		std::uniform_real_distribution<double> ud(0.0, 1.0);
		std::vector<double> nominal(sends.size());
		for (size_t i = 0; i < sends.size(); ++i)
			nominal[i] = (double)sends[i] + (ud(rng) * 2.0 - 1.0) * jitterMs;

		std::vector<double> arrival(nominal);
		if (bunchProb > 0.0)
		{
			for (size_t i = 1; i + 1 < sends.size(); )
			{
				if (ud(rng) < bunchProb)
				{
					arrival[i] = nominal[i + 1] - ud(rng) * 10.0;
					i += 2;
				}
				else
				{
					++i;
				}
			}
		}

		std::vector<VRObs> out;
		out.reserve(sends.size());
		int64_t prev = -1;
		for (size_t i = 0; i < sends.size(); ++i)
		{
			int64_t t = (int64_t)std::llround(arrival[i]);
			if (t <= prev)
				t = prev + 1;
			out.push_back(VRObs{ t, VRQuant(4.5 * (double)sends[i] / 1000.0), (int16_t)45 });
			prev = t;
		}
		return out;
	}

	std::vector<VRObs> VRVariable(uint32_t seed)
	{
		std::mt19937 rng(seed);
		std::vector<VRObs> out;
		int64_t t = 1000;
		while (t < 120000)
		{
			out.push_back(VRObs{ t, VRQuant(4.5 * (double)t / 1000.0), (int16_t)45 });
			t += 1000 + (int64_t)(rng() % 1501u);
		}
		return out;
	}

	std::vector<VRObs> VRLoss(uint32_t)
	{
		std::vector<VRObs> out;
		int64_t t = 1000;
		int k = 0;
		while (t < 120000)
		{
			if (k % 4 != 3)
				out.push_back(VRObs{ t, VRQuant(4.5 * (double)t / 1000.0), (int16_t)45 });
			t += 1500;
			++k;
		}
		return out;
	}

	VRStats VRRun(const std::vector<VRObs> & obs, double mps)
	{
		VRStats s;
		std::vector<double> errs;
		const int64_t end = obs.back().t + 4000;
		for (int64_t now = obs.front().t; now <= end; now += 100)
		{
			NavTargetTracker t;
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
			if (now - nt > 4000)
				continue;
			float vx = 0.0f;
			float vz = 0.0f;
			t.Velocity(now, 4000, 400, vx, vz);
			const double mag = std::sqrt((double)vx * vx + (double)vz * vz);
			++s.ticks;
			if (mag <= 1e-3)
				++s.zero;
			else
				errs.push_back(std::fabs(mag - mps) / mps);
		}
		std::sort(errs.begin(), errs.end());
		s.p50 = Pct(errs, 50);
		s.p95 = Pct(errs, 95);
		s.max = errs.empty() ? 0.0 : errs.back();
		return s;
	}

	void VelocityRobust(Ctx & c)
	{
		struct Scenario { const char * name; int kind; };
		const Scenario scen[] = {
			{ "arrival_jitter", 0 },
			{ "arrival_bunching", 1 },
			{ "variable_interval", 2 },
			{ "packet_loss", 3 },
		};
		for (const Scenario & sc : scen)
		{
			double worstP50 = 0.0, worstP95 = 0.0, worstMax = 0.0, worstZero = 0.0;
			int p95Seed = 0, maxSeed = 0;
			for (int seed = 1; seed <= 20; ++seed)
			{
				std::vector<VRObs> obs;
				if (sc.kind == 0)
					obs = VRCadence((uint32_t)seed, 1500, 150.0, 0.0);
				else if (sc.kind == 1)
					obs = VRCadence((uint32_t)seed, 1500, 150.0, 0.05);
				else if (sc.kind == 2)
					obs = VRVariable((uint32_t)seed);
				else
					obs = VRLoss((uint32_t)seed);
				const VRStats s = VRRun(obs, 4.5);
				const double zp = s.ticks > 0 ? 100.0 * (double)s.zero / (double)s.ticks : 0.0;
				if (s.p50 > worstP50)
					worstP50 = s.p50;
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
			std::printf("VELOCITYR scenario=%s seeds=20 seed_p95=%d seed_max=%d zero_pct=%.3f err_p50=%.4f err_p95=%.4f err_max=%.4f\n",
				sc.name, p95Seed, maxSeed, worstZero, worstP50, worstP95, worstMax);
		}
	}

	// ---------- arena / respawn ----------
	void Arena(Ctx & c)
	{
		NavPathfinder pf;
		NavPathResult res;
		NavSearchParams sp;
		NavCostLayer arena;
		arena.Init(c.grid);
		arena.AddForbidOutsideDisc(1274.0f, 890.0f, 60.0f);
		struct Case { const char * name; float sx, sz, gx, gz; };
		const Case cases[] = {
			{ "karus_respawn_to_arena", 1385.0f, 1095.0f, 1274.0f, 890.0f },
			{ "elmorad_respawn_to_arena", 635.0f, 925.0f, 1274.0f, 890.0f },
			{ "arena_to_karus_respawn", 1274.0f, 890.0f, 1385.0f, 1095.0f },
		};
		static const char * st[] = { "Found", "NoPath", "NodeLimit", "InvalidStart", "InvalidGoal" };
		const float pens[] = { -1.0f, 10.0f, 0.0f };   // -1 = no cost field
		for (const Case & k : cases)
			for (float pen : pens)
			{
				NavCostField field;
				field.layer = &arena;
				if (pen >= 0.0f)
					field.params.forbiddenPenalty = pen;
				const NavCell s = NearestWalk(c.grid, k.sx, k.sz), g = NearestWalk(c.grid, k.gx, k.gz);
				const auto t0 = Clock::now();
				pf.Find(c.grid, s, g, sp, res, pen < 0.0f ? nullptr : &field);
				const double ms = MsSince(t0);
				std::printf("ARENA case=%s forbidden_penalty=%s status=%s expanded=%d length_m=%.1f ms=%.3f\n", k.name,
					pen < 0.0f ? "nofield" : (pen == 10.0f ? "10(default)" : "0"), st[(int)res.status], res.expanded, res.length, ms);
			}
	}

	// ---------- budget: N bots, one query each in the same tick ----------
	void Budget(Ctx & c)
	{
		std::mt19937 rng(c.seed);
		NavPathfinder pf;
		NavPathResult res;
		NavSearchParams sp;
		struct Set { const char * name; int cheb; int bots; int ticks; };
		const Set sets[] = { { "near64", 64, 16, 300 }, { "mid150", 150, 16, 300 }, { "whole", 600, 16, 100 }, { "near64", 64, 64, 100 } };
		for (const Set & s : sets)
		{
			std::vector<double> perQuery, perTick;
			for (int k = 0; k < s.ticks; ++k)
			{
				double sum = 0.0;
				for (int b = 0; b < s.bots; ++b)
				{
					NavCell a = c.walk[rng() % c.walk.size()];
					NavCell g = RandomNear(c, rng, a, s.cheb);
					const auto t0 = Clock::now();
					pf.Find(c.grid, a, g, sp, res);
					const double ms = MsSince(t0);
					perQuery.push_back(ms);
					sum += ms;
				}
				perTick.push_back(sum);
			}
			std::printf("BUDGET set=%s bots_per_tick=%d ticks=%d query_p50=%.3f query_p95=%.3f query_p99=%.3f tick_p50=%.3f tick_p95=%.3f tick_p99=%.3f tick_max=%.3f\n",
				s.name, s.bots, s.ticks, Pct(perQuery, 50), Pct(perQuery, 95), Pct(perQuery, 99), Pct(perTick, 50), Pct(perTick, 95), Pct(perTick, 99), Pct(perTick, 100));
		}
	}

	// ---------- budget-scheduled: near64 load through NavQueryScheduler (F5-53) ----------
	// Mode A runs every due query immediately (unscheduled); mode B feeds the same random query
	// sequence through the scheduler. One line per mode.
	void BudgetScheduled(Ctx & c)
	{
		const int bots = 16;
		const int ticks = 600;                 // 60 s of virtual time at 100 ms
		const double budgetMs = 1.5;           // P-NAV-TICK-BUDGET-MS
		const int intervalTicks = 5;           // 500 ms at 100 ms/tick
		const int requestsPerBot = 120;

		// Random query sequence shared by both modes: one (start, goal) per request, per bot.
		std::vector<NavCell> qa;
		std::vector<NavCell> qg;
		{
			std::mt19937 rng(c.seed);
			for (int b = 0; b < bots; ++b)
			{
				for (int k = 0; k < requestsPerBot; ++k)
				{
					const NavCell a = c.walk[rng() % c.walk.size()];
					qa.push_back(a);
					qg.push_back(RandomNear(c, rng, a, 64));
				}
			}
		}

		auto due = [&](int b, int t)
		{
			const int phase100 = NavReplanPhaseMs(b) / 100;
			return t >= phase100 && (t % intervalTicks) == phase100;
		};

		auto run = [&](bool scheduled)
		{
			NavPathfinder pf;
			NavPathResult res;
			NavSearchParams sp;
			NavQueryScheduler sched;
			sched.SetMaxWaitMs(1000);

			std::vector<double> tickSum;
			int64_t longestWait = 0;
			int64_t requestAt[64];
			for (int b = 0; b < 64; ++b)
				requestAt[b] = -1;
			size_t qIndex = 0;
			long served = 0;

			for (int t = 0; t < ticks; ++t)
			{
				const int64_t nowMs = (int64_t)t * 100;
				uint16_t batch[64];
				int n = 0;
				if (!scheduled)
				{
					for (int b = 0; b < bots; ++b)
						if (due(b, t))
							batch[n++] = (uint16_t)b;
				}
				else
				{
					for (int b = 0; b < bots; ++b)
						if (due(b, t))
						{
							if (requestAt[b] < 0)
								requestAt[b] = nowMs;
							sched.Request((uint16_t)b, nowMs);
						}
					n = sched.NextBatch(nowMs, budgetMs, batch, 64);
				}

				double sum = 0.0;
				for (int k = 0; k < n; ++k)
				{
					if (qIndex >= qa.size())
						break;
					const int b = batch[k];
					const NavCell a = qa[qIndex];
					const NavCell g = qg[qIndex];
					++qIndex;
					const auto t0 = Clock::now();
					pf.Find(c.grid, a, g, sp, res);
					const double ms = MsSince(t0);
					sum += ms;
					++served;
					if (scheduled)
					{
						sched.ReportCost((uint16_t)b, ms, res.expanded);
						if (requestAt[b] >= 0)
						{
							const int64_t wait = nowMs - requestAt[b];
							if (wait > longestWait)
								longestWait = wait;
							requestAt[b] = -1;
						}
						sched.Cancel((uint16_t)b);
					}
				}
				tickSum.push_back(sum);
			}

			std::printf("BUDGET_SCHED mode=%s bots=%d ticks=%d budget_ms=%.1f served=%ld tick_p50=%.3f tick_p95=%.3f tick_p99=%.3f tick_max=%.3f longest_wait_ms=%lld pending=%d\n",
				scheduled ? "B" : "A", bots, ticks, budgetMs, served, Pct(tickSum, 50), Pct(tickSum, 95), Pct(tickSum, 99), Pct(tickSum, 100),
				(long long)longestWait, sched.Pending());
		};

		run(false);
		run(true);
	}

	// ---------- stuck: the F5-09 detector fed with bot packet-cadence positions ----------
	struct StuckRun { long ticks = 0; long alarms = 0; long episodes = 0; };

	StuckRun SimulateWalk(const NavGrid & grid, const NavStuckParams & p, bool feedAtTicks, double tickMean, double tickJitter, double lateProb, uint32_t seed, int secs)
	{
		std::mt19937 rng(seed);
		std::normal_distribution<double> nd(tickMean, tickJitter);
		std::uniform_real_distribution<double> ud(0.0, 1.0);
		NavStuckDetector det;
		StuckRun r;
		double t = 0.0, lastPacket = 0.0;
		float x = 1000.0f;   // walking along +x at 4.5 m/s; the detector only needs displacement, cells are irrelevant
		const float z = 900.0f;
		bool inAlarm = false;
		while (t < secs * 1000.0)
		{
			double dt = std::max(20.0, nd(rng));
			if (ud(rng) < lateProb)
				dt += 250.0;
			t += dt;
			bool packet = false;
			if (t - lastPacket >= 1500.0)
			{
				x += 4.5f * (float)((t - lastPacket) / 1000.0);   // server position jumps by the covered distance
				lastPacket = t;
				packet = true;
			}
			if (t < 5000.0)
				continue;
			++r.ticks;
			if (feedAtTicks || packet)
			{
				const NavStuckKind k = det.Observe(grid, (int64_t)t, x, z, true, p);
				if (k != NavStuckKind::None)
				{
					++r.alarms;
					if (!inAlarm)
						++r.episodes;
					inAlarm = true;
				}
				else
				{
					inAlarm = false;
				}
			}
		}
		return r;
	}

	void Stuck(Ctx & c)
	{
		NavStuckParams def;                     // F5-09 defaults (docs/12 s10 wording)
		NavStuckParams cad;                     // packet-cadence preset (F5-54: noProgress 3200, osc 8000 ms, 3 swings)
		cad.noProgressMs = 3200;
		cad.oscWindowMs = 8000;
		cad.oscSwings = 3;
		struct Model { const char * name; double mean, jit, late; };
		const Model models[] = { { "tick100+-0", 100, 0, 0 }, { "tick100+-10", 100, 10, 0 }, { "tick110.8+-20+3%late250", 110.8, 20, 0.03 } };
		for (const Model & m : models)
			for (int feed = 0; feed < 2; ++feed)
				for (int pr = 0; pr < 2; ++pr)
				{
					const StuckRun r = SimulateWalk(c.grid, pr == 0 ? def : cad, feed == 0, m.mean, m.jit, m.late, c.seed, 600);
					std::printf("STUCK model=%s feed=%s params=%s ticks=%ld alarm_ticks=%ld false_episodes=%ld\n", m.name,
						feed == 0 ? "every_tick" : "packets_only", pr == 0 ? "F5-09_default" : "cadence_3200", r.ticks, r.alarms, r.episodes);
				}
		// true stuck: position never changes while the bot "moves": detection latency
		for (int pr = 0; pr < 2; ++pr)
		{
			NavStuckDetector det;
			const NavStuckParams & p = pr == 0 ? def : cad;
			int64_t detectedAt = -1;
			for (int64_t t = 0; t < 20000; t += 100)
			{
				if (det.Observe(c.grid, t, 1000.0f, 900.0f, true, p) != NavStuckKind::None && detectedAt < 0)
					detectedAt = t;
			}
			std::printf("STUCK_TRUE params=%s detected_after_ms=%lld\n", pr == 0 ? "F5-09_default" : "cadence_3200", (long long)detectedAt);
		}
	}

	// ---------- progress (F5-57): intent + real route progress, false-alarm table ----------
	struct ProgRun
	{
		long ticks = 0;
		long stalled = 0;
		long awaiting = 0;
		long progressing = 0;
		long blocked = 0;
		long stallEpisodes = 0;
		long detectedMs = -1;
	};

	// Route-aware walk with the F5-54 packet cadence. The bot intends to move the whole time at
	// speedMps; the server position and the route progress only jump at packet instants. Three
	// evaluators share the same event stream: mode 0 = F5-09 defaults alone (Euclidean), mode 1 =
	// NavPacketCadenceParams alone, mode 2 = NavProgressAssessor (with the monitor for episodes).
	ProgRun RunProgress(const NavGrid & g, int mode, double tickMean, double tickJitter, double lateProb,
		double speedMps, int secs, uint32_t seed)
	{
		ProgRun out;
		std::mt19937 rng(seed);
		std::normal_distribution<double> nd(0.0, tickJitter > 0.0 ? tickJitter : 1.0);
		std::uniform_real_distribution<double> ud(0.0, 1.0);
		const double totalLen = speedMps * (double)secs;
		NavProgressAssessor assessor;
		assessor.SetIntent(true, 0);
		const NavProgressParams pp;
		NavStuckParams def;                     // F5-09 defaults (docs/12 s10 wording)
		NavStuckParams cad = NavPacketCadenceParams();
		NavStuckDetector det;
		NavStuckMonitor monitor;
		double pos = 0.0, t = 0.0, lastPacket = 0.0;
		bool inStall = false;
		while (t < (double)secs * 1000.0)
		{
			double dt = tickMean + (tickJitter > 0.0 ? nd(rng) : 0.0);
			if (ud(rng) < lateProb)
				dt += 250.0;
			if (dt < 20.0)
				dt = 20.0;
			t += dt;
			if (t - lastPacket >= 1500.0)
			{
				pos += speedMps * (t - lastPacket) / 1000.0;
				if (pos > totalLen)
					pos = totalLen;
				assessor.OnPacketSent((int64_t)t, 1000.0f, (float)pos, (float)pos, (float)(totalLen - pos));
				lastPacket = t;
			}
			if (t < 5000.0)
				continue;
			++out.ticks;

			if (mode == 0 || mode == 1)
			{
				const NavStuckKind k = det.Observe(g, (int64_t)t, 1000.0f, (float)pos, true,
					mode == 0 ? def : cad);
				if (k != NavStuckKind::None)
				{
					++out.stalled;
					if (!inStall)
						++out.stallEpisodes;
					inStall = true;
					if (out.detectedMs < 0)
						out.detectedMs = (long)t;
				}
				else
				{
					inStall = false;
				}
			}
			else
			{
				const NavProgressVerdict v = assessor.Assess((int64_t)t, pp);
				if (v == NavProgressVerdict::Stalled)
				{
					++out.stalled;
					if (!inStall)
						++out.stallEpisodes;
					inStall = true;
					if (out.detectedMs < 0)
						out.detectedMs = (long)t;
				}
				else
				{
					inStall = false;
					if (v == NavProgressVerdict::AwaitingPacket)
						++out.awaiting;
					else if (v == NavProgressVerdict::Progressing)
						++out.progressing;
					else if (v == NavProgressVerdict::BlockedByGuard)
						++out.blocked;
				}
				const bool moving = (v == NavProgressVerdict::Progressing || v == NavProgressVerdict::Stalled);
				monitor.Update(g, (int64_t)t, 1000.0f, (float)pos, moving, cad);
			}
		}
		if (mode == 2)
			out.stallEpisodes = monitor.Episodes();
		return out;
	}

	void Progress(Ctx & c)
	{
		struct Model { const char * name; double mean, jit, late; };
		const Model models[] = { { "tick100+-0", 100, 0, 0 }, { "tick100+-10", 100, 10, 0 }, { "tick110.8+-20+3%late250", 110.8, 20, 0.03 } };
		for (const Model & m : models)
			for (int mode = 0; mode < 3; ++mode)
			{
				const ProgRun r = RunProgress(c.grid, mode, m.mean, m.jit, m.late, 4.5, 600, c.seed);
				std::printf("PROGRESS model=%s evaluator=%s ticks=%ld stalled=%ld awaiting=%ld progressing=%ld blocked=%ld false_episodes=%ld first_ms=%ld\n",
					m.name, mode == 0 ? "F5-09_default" : (mode == 1 ? "cadence_3200" : "assessor"),
					r.ticks, r.stalled, r.awaiting, r.progressing, r.blocked, r.stallEpisodes, r.detectedMs);
			}
		// True stuck: the position never changes while the bot moves -> detection latency per evaluator.
		for (int mode = 0; mode < 3; ++mode)
		{
			NavStuckDetector det;
			NavProgressAssessor assessor;
			assessor.SetIntent(true, 0);
			assessor.NotifyReplan(0, 0.0f);
			const NavProgressParams pp;
			NavStuckParams def;
			NavStuckParams cad = NavPacketCadenceParams();
			int64_t detectedAt = -1;
			int64_t nextPacket = 0;
			for (int64_t t = 0; t < 20000; t += 100)
			{
				bool alarm = false;
				if (mode == 0 || mode == 1)
				{
					alarm = det.Observe(c.grid, t, 1000.0f, 900.0f, true, mode == 0 ? def : cad) != NavStuckKind::None;
				}
				else
				{
					if (t >= nextPacket)
					{
						assessor.OnPacketSent(t, 1000.0f, 900.0f, 0.0f, 100.0f);
						nextPacket = t + 1500;
					}
					alarm = assessor.Assess(t, pp) == NavProgressVerdict::Stalled;
				}
				if (alarm && detectedAt < 0)
					detectedAt = t;
			}
			std::printf("PROGRESS_TRUE evaluator=%s detected_after_ms=%lld\n",
				mode == 0 ? "F5-09_default" : (mode == 1 ? "cadence_3200" : "assessor"), (long long)detectedAt);
		}
	}

	// ---------- water (F5-60): does the map data actually block water crossings? ----------
	// Offline audit only: no water layer is added and NavGrid/pathfinder behaviour is untouched.
	// "basins" (Walk cells below --water-t) are a HEIGHT PROXY, not proven water; without a verified
	// --mask they only measure exposure of paths/chords to the proxy set W. Percentile index is
	// int(len * q) over the sorted Walk heights (same definition in tools/nav-water-audit.py).
	double HeightPct(const std::vector<float> & sorted, double q)
	{
		if (sorted.empty())
			return 0.0;
		size_t idx = (size_t)((double)sorted.size() * q);
		if (idx >= sorted.size())
			idx = sorted.size() - 1;
		return (double)sorted[idx];
	}

	// 4-connected component labels over a predicate, scanning x-outer / z-inner; returns component
	// count and fills label/size/touchesEdge. Uses an iterative stack (no recursion).
	int LabelComponents(const NavGrid & g, const std::vector<uint8_t> & inSet,
		std::vector<int32_t> & label, std::vector<int32_t> & sizes, std::vector<uint8_t> & touchesEdge)
	{
		const int n = g.Size();
		label.assign((size_t)n * n, -1);
		sizes.clear();
		touchesEdge.clear();
		std::vector<int32_t> stack;
		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				const int start = x * n + z;
				if (!inSet[(size_t)start] || label[start] >= 0)
					continue;
				const int id = (int)sizes.size();
				sizes.push_back(0);
				touchesEdge.push_back(0);
				label[start] = id;
				stack.clear();
				stack.push_back(start);
				while (!stack.empty())
				{
					const int cur = stack.back();
					stack.pop_back();
					const int cx = cur / n, cz = cur % n;
					sizes[id] += 1;
					if (cx == 0 || cx == n - 1 || cz == 0 || cz == n - 1)
						touchesEdge[id] = 1;
					const int nx[4] = { cx + 1, cx - 1, cx, cx };
					const int nz[4] = { cz, cz, cz + 1, cz - 1 };
					for (int k = 0; k < 4; ++k)
					{
						if (nx[k] < 0 || nx[k] >= n || nz[k] < 0 || nz[k] >= n)
							continue;
						const int ni = nx[k] * n + nz[k];
						if (inSet[(size_t)ni] && label[ni] < 0)
						{
							label[ni] = id;
							stack.push_back(ni);
						}
					}
				}
			}
		}
		return (int)sizes.size();
	}

	// true when any super-cover cell of the segment is in the W mask.
	bool SegmentTouchesSet(const NavGrid & g, const std::vector<uint8_t> & inW,
		double ax, double az, double bx, double bz)
	{
		static thread_local std::vector<std::pair<int, int>> cells;
		TouchedCells(g, ax, az, bx, bz, cells);
		const int n = g.Size();
		for (size_t i = 0; i < cells.size(); ++i)
		{
			const int x = cells[i].first, z = cells[i].second;
			if (x >= 0 && x < n && z >= 0 && z < n && inW[(size_t)x * n + z])
				return true;
		}
		return false;
	}

	void Water(Ctx & c)
	{
		const NavGrid & g = c.grid;
		const int n = g.Size();
		const float unit = g.Unit();

		// event / Walk counts and pockets (event==1, not Walk, 4-connected, edge-touching dropped)
		long events0 = 0, events1 = 0, walk = 0, event1NotWalk = 0;
		std::vector<uint8_t> eventSet((size_t)n * n, 0);
		for (int x = 0; x < n; ++x)
			for (int z = 0; z < n; ++z)
			{
				const int16_t e = g.Event(x, z);
				if (e == 0)
					++events0;
				else if (e == 1)
					++events1;
				if (g.Walk(x, z))
					++walk;
				else if (e == 1)
				{
					++event1NotWalk;
					eventSet[(size_t)x * n + z] = 1;   // pockets are event 1 cells that are not Walk
				}
			}

		std::vector<int32_t> pocketLabel, pocketSizes;
		std::vector<uint8_t> pocketEdge;
		LabelComponents(g, eventSet, pocketLabel, pocketSizes, pocketEdge);
		long pockets = 0, pocketCells = 0, pocketAdjWalk = 0;
		for (size_t id = 0; id < pocketSizes.size(); ++id)
		{
			if (pocketEdge[id])
				continue;
			++pockets;
			pocketCells += pocketSizes[id];
		}
		for (int x = 0; x < n; ++x)
			for (int z = 0; z < n; ++z)
			{
				const int32_t id = pocketLabel[(size_t)x * n + z];
				if (id < 0 || pocketEdge[(size_t)id])
					continue;
				bool adj = false;
				for (int dx = -1; dx <= 1 && !adj; ++dx)
					for (int dz = -1; dz <= 1 && !adj; ++dz)
						if ((dx != 0 || dz != 0) && g.Walk(x + dx, z + dz))
							adj = true;
				if (adj)
					++pocketAdjWalk;
			}
		std::printf("WATER_GRID n=%d unit=%.1f events0=%ld events1=%ld walk=%ld event1_not_walk=%ld pockets=%ld pocket_cells=%ld pocket_cells_adjacent_to_walk=%ld\n",
			n, unit, events0, events1, walk, event1NotWalk, pockets, pocketCells, pocketAdjWalk);

		// Walk height distribution
		std::vector<float> hs;
		hs.reserve((size_t)walk);
		long b0 = 0, b1 = 0, b2 = 0, b4 = 0, b6 = 0;
		for (int x = 0; x < n; ++x)
			for (int z = 0; z < n; ++z)
				if (g.Walk(x, z))
				{
					const float h = g.Height(x, z);
					hs.push_back(h);
					if (h < 0.0f) ++b0;
					if (h < -1.0f) ++b1;
					if (h < -2.0f) ++b2;
					if (h < -4.0f) ++b4;
					if (h < -6.0f) ++b6;
				}
		std::sort(hs.begin(), hs.end());
		std::printf("WATER_HEIGHT walk_hmin=%.2f walk_p1=%.2f walk_p5=%.2f walk_p50=%.2f walk_p95=%.2f walk_hmax=%.2f walk_below_0=%ld walk_below_m1=%ld walk_below_m2=%ld walk_below_m4=%ld walk_below_m6=%ld\n",
			hs.empty() ? 0.0 : (double)hs.front(), HeightPct(hs, 0.01), HeightPct(hs, 0.05), HeightPct(hs, 0.50),
			HeightPct(hs, 0.95), hs.empty() ? 0.0 : (double)hs.back(), b0, b1, b2, b4, b6);

		// basins: Walk cells below waterT, 4-connected; components with >= waterMin cells
		const float waterT = c.waterT;
		std::vector<uint8_t> basinSet((size_t)n * n, 0);
		for (int x = 0; x < n; ++x)
			for (int z = 0; z < n; ++z)
				if (g.Walk(x, z) && g.Height(x, z) < waterT)
					basinSet[(size_t)x * n + z] = 1;
		std::vector<int32_t> basinLabel, basinSizes;
		std::vector<uint8_t> basinEdge;
		LabelComponents(g, basinSet, basinLabel, basinSizes, basinEdge);

		std::vector<uint8_t> inW((size_t)n * n, 0);
		long basinCount = 0, basinCells = 0;
		int id = 0;
		for (size_t comp = 0; comp < basinSizes.size(); ++comp)
		{
			if (basinSizes[comp] < c.waterMin)
				continue;
			++basinCount;
			basinCells += basinSizes[comp];
			float hmin = 1e30f, hmax = -1e30f;
			int xmin = n, xmax = -1, zmin = n, zmax = -1;
			long edge0 = 0;
			for (int x = 0; x < n; ++x)
				for (int z = 0; z < n; ++z)
					if (basinLabel[(size_t)x * n + z] == (int32_t)comp)
					{
						inW[(size_t)x * n + z] = 1;
						const float h = g.Height(x, z);
						if (h < hmin) hmin = h;
						if (h > hmax) hmax = h;
						if (x < xmin) xmin = x;
						if (x > xmax) xmax = x;
						if (z < zmin) zmin = z;
						if (z > zmax) zmax = z;
						bool e0 = false;
						for (int dx = -1; dx <= 1 && !e0; ++dx)
							for (int dz = -1; dz <= 1 && !e0; ++dz)
								if ((dx != 0 || dz != 0) && g.Event(x + dx, z + dz) == 0)
									e0 = true;
						if (e0)
							++edge0;
					}
			std::printf("WATER_BASIN id=%d cells=%d hmin=%.2f hmax=%.2f x=[%.1f,%.1f] z=[%.1f,%.1f] edge0_touch=%ld\n",
				id, (int)basinSizes[comp], (double)hmin, (double)hmax,
				(double)xmin * unit, (double)xmax * unit, (double)zmin * unit, (double)zmax * unit, edge0);
			++id;
		}
		std::printf("WATER_BASIN_SUMMARY t=%.2f min_cells=%d count=%ld cells=%ld\n", (double)waterT, c.waterMin, basinCount, basinCells);

		// W: proxy basins by default, or Walk ∩ verified mask when --mask is given. Never a verdict.
		if (c.maskPath.empty())
		{
			std::printf("WATER_SOURCE proxy_basins\n");
			std::printf("WATER_TRUTH none\n");
		}
		else
		{
			std::vector<uint8_t> mask;
			std::ifstream in(c.maskPath.c_str(), std::ios::binary | std::ios::ate);
			long maskCells = 0, maskAndWalk = 0, maskNotWalk = 0;
			bool ok = false;
			if (in)
			{
				const std::streamoff len = in.tellg();
				if (len == (std::streamoff)((size_t)n * n))
				{
					in.seekg(0, std::ios::beg);
					mask.resize((size_t)n * n);
					if (in.read(reinterpret_cast<char *>(mask.data()), (std::streamsize)len))
						ok = true;
				}
			}
			std::fill(inW.begin(), inW.end(), 0);
			if (ok)
				for (int x = 0; x < n; ++x)
					for (int z = 0; z < n; ++z)
						if (mask[(size_t)x * n + z])
						{
							++maskCells;
							if (g.Walk(x, z))
							{
								++maskAndWalk;
								inW[(size_t)x * n + z] = 1;
							}
							else
								++maskNotWalk;
						}
			std::printf("WATER_SOURCE mask:%s\n", c.maskPath.c_str());
			std::printf("WATER_TRUTH mask_cells=%ld mask_and_walk=%ld mask_not_walk=%ld%s\n", maskCells, maskAndWalk, maskNotWalk, ok ? "" : " (unreadable)");
		}

		// shore: Walk cells outside W with an 8-neighbour in W
		long shore = 0;
		for (int x = 0; x < n; ++x)
			for (int z = 0; z < n; ++z)
			{
				if (!g.Walk(x, z) || inW[(size_t)x * n + z])
					continue;
				bool adj = false;
				for (int dx = -1; dx <= 1 && !adj; ++dx)
					for (int dz = -1; dz <= 1 && !adj; ++dz)
					{
						const int nx = x + dx, nz = z + dz;
						if (nx >= 0 && nx < n && nz >= 0 && nz < n && inW[(size_t)nx * n + nz])
							adj = true;
					}
				if (adj)
					++shore;
			}
		std::printf("WATER_SHORE cells=%ld\n", shore);

		// planner audit: same mixed query pattern as Smoothing, no cost field
		std::mt19937 rng(c.seed);
		NavPathfinder pf;
		NavPathResult res;
		NavSearchParams sp;
		long pairs = 0, found = 0, nodelimit = 0, nopath = 0, nonWalk = 0, pathsWithW = 0, rawInW = 0;
		long smoothSegs = 0, smoothTouchW = 0, chords = 0, chordsTouchW = 0;
		int examples = 0;
		for (int q = 0; q < c.n; ++q)
		{
			++pairs;
			const int cheb = (q % 3 == 0) ? 64 : (q % 3 == 1 ? 150 : 40);
			NavCell a = c.walk[rng() % c.walk.size()];
			NavCell b = RandomNear(c, rng, a, cheb);
			if (a == b)
				continue;
			pf.Find(g, a, b, sp, res);
			if (res.status == NavPathStatus::NodeLimit)
			{
				++nodelimit;
				continue;
			}
			if (res.status != NavPathStatus::Found)
			{
				++nopath;
				continue;
			}
			++found;

			bool hasW = false;
			std::pair<int, int> firstW(-1, -1);
			for (size_t i = 0; i < res.cells.size(); ++i)
			{
				const int cx = res.cells[i].x, cz = res.cells[i].z;
				if (!g.Walk(cx, cz))
					++nonWalk;
				if (cx >= 0 && cx < n && cz >= 0 && cz < n && inW[(size_t)cx * n + cz])
				{
					++rawInW;
					if (!hasW)
					{
						hasW = true;
						firstW = std::make_pair(cx, cz);
					}
				}
			}
			if (hasW)
			{
				++pathsWithW;
				if (examples < 5)
				{
					std::printf("EXAMPLE water path (%.1f,%.1f)->(%.1f,%.1f) cell (%d,%d) in W\n",
						g.CellCenter(a.x), g.CellCenter(a.z), g.CellCenter(b.x), g.CellCenter(b.z), firstW.first, firstW.second);
					++examples;
				}
			}

			NavSmoothResult sm;
			NavSmoothPath(g, res.cells, NavSmoothParams(), sm);
			for (size_t i = 1; i < sm.waypoints.size(); ++i)
			{
				++smoothSegs;
				if (SegmentTouchesSet(g, inW, g.CellCenter(sm.waypoints[i - 1].x), g.CellCenter(sm.waypoints[i - 1].z),
					g.CellCenter(sm.waypoints[i].x), g.CellCenter(sm.waypoints[i].z)))
					++smoothTouchW;
			}

			if (sm.waypoints.empty())
				continue;
			double px = g.CellCenter(sm.waypoints[0].x), pz = g.CellCenter(sm.waypoints[0].z);
			double lastX = px, lastZ = pz, carry = 0.0;
			for (size_t i = 1; i < sm.waypoints.size(); ++i)
			{
				const double tx = g.CellCenter(sm.waypoints[i].x), tz = g.CellCenter(sm.waypoints[i].z);
				const double seg = std::sqrt((tx - px) * (tx - px) + (tz - pz) * (tz - pz));
				const double ux = seg > 0 ? (tx - px) / seg : 0, uz = seg > 0 ? (tz - pz) / seg : 0;
				double pos = 0.0;
				while (carry + (seg - pos) >= 6.75)
				{
					pos += 6.75 - carry;
					carry = 0.0;
					const double nx = px + ux * pos, nz = pz + uz * pos;
					++chords;
					if (SegmentTouchesSet(g, inW, lastX, lastZ, nx, nz))
						++chordsTouchW;
					lastX = nx;
					lastZ = nz;
				}
				carry += seg - pos;
				px = tx;
				pz = tz;
			}
		}
		std::printf("WATER_PATHS pairs=%ld found=%ld nodelimit=%ld nopath=%ld non_walk_cells_on_path=%ld paths_with_w_cell=%ld raw_cells_in_w=%ld smooth_segments=%ld smooth_segments_touching_w=%ld chords=%ld chords_touching_w=%ld\n",
			pairs, found, nodelimit, nopath, nonWalk, pathsWithW, rawInW, smoothSegs, smoothTouchW, chords, chordsTouchW);

		// spawn -> arena routes (cost field free)
		struct RouteCase { const char * name; float sx, sz, gx, gz; };
		const RouteCase routes[] = {
			{ "karus", 1385.0f, 1095.0f, 1274.0f, 890.0f },
			{ "elmorad", 635.0f, 925.0f, 1274.0f, 890.0f },
		};
		for (const RouteCase & r : routes)
		{
			const NavCell s = NearestWalk(g, r.sx, r.sz);
			const NavCell gg = NearestWalk(g, r.gx, r.gz);
			pf.Find(g, s, gg, sp, res);
			long cells = 0, inWCount = 0;
			NavSmoothResult sm;
			long segsTouchW = 0;
			if (res.status == NavPathStatus::Found)
			{
				cells = (long)res.cells.size();
				for (size_t i = 0; i < res.cells.size(); ++i)
					if (inW[(size_t)res.cells[i].x * n + res.cells[i].z])
						++inWCount;
				NavSmoothPath(g, res.cells, NavSmoothParams(), sm);
				for (size_t i = 1; i < sm.waypoints.size(); ++i)
					if (SegmentTouchesSet(g, inW, g.CellCenter(sm.waypoints[i - 1].x), g.CellCenter(sm.waypoints[i - 1].z),
						g.CellCenter(sm.waypoints[i].x), g.CellCenter(sm.waypoints[i].z)))
						++segsTouchW;
			}
			std::printf("WATER_ROUTE name=%s length_m=%.1f cells=%ld cells_in_w=%ld segments_touching_w=%ld\n",
				r.name, (double)res.length, cells, inWCount, segsTouchW);
		}
	}
}

int main(int argc, char ** argv)
{
	if (argc < 2)
	{
		std::printf("usage: nav_measure <segments|smoothing|velocity|velocity-robust|arena|budget|budget-scheduled|stuck|progress|water|all> [--navgrid PATH] [--seed N] [--n COUNT] [--water-t F] [--water-min N] [--mask PATH]\n");
		return 2;
	}
	std::string section = argv[1];
	std::string path = "build/nav/zone71.navgrid";
	Ctx c;
	for (int i = 2; i + 1 < argc; i += 2)
	{
		if (std::strcmp(argv[i], "--navgrid") == 0)
			path = argv[i + 1];
		else if (std::strcmp(argv[i], "--seed") == 0)
			c.seed = (uint32_t)std::strtoul(argv[i + 1], nullptr, 10);
		else if (std::strcmp(argv[i], "--n") == 0)
			c.n = std::atoi(argv[i + 1]);
		else if (std::strcmp(argv[i], "--water-t") == 0)
			c.waterT = (float)std::atof(argv[i + 1]);
		else if (std::strcmp(argv[i], "--water-min") == 0)
			c.waterMin = std::atoi(argv[i + 1]);
		else if (std::strcmp(argv[i], "--mask") == 0)
			c.maskPath = argv[i + 1];
	}
	if (!c.grid.LoadFile(path.c_str()))
	{
		std::printf("ERROR cannot load %s (python3 tools/nav-export.py)\n", path.c_str());
		return 2;
	}
	c.grid.Build();
	for (int x = 0; x < c.grid.Size(); ++x)
		for (int z = 0; z < c.grid.Size(); ++z)
			if (c.grid.Walk(x, z))
				c.walk.push_back(NavCell{ x, z });
	std::printf("GRID n=%d unit=%.1f main_cells=%d seed=%u pairs=%d\n", c.grid.Size(), c.grid.Unit(), c.grid.MainComponentCells(), c.seed, c.n);

	const bool all = section == "all";
	if (all || section == "segments" || section == "smoothing")
		Smoothing(c);
	if (all || section == "synthetic")
		Synthetic(c);
	if (all || section == "velocity")
		Velocity(c);
	if (all || section == "velocity-robust")
		VelocityRobust(c);
	if (all || section == "arena")
		Arena(c);
	if (all || section == "budget")
		Budget(c);
	if (all || section == "budget-scheduled")
		BudgetScheduled(c);
	if (all || section == "stuck")
		Stuck(c);
	if (all || section == "progress")
		Progress(c);
	if (section == "water")
		Water(c);
	return 0;
}
