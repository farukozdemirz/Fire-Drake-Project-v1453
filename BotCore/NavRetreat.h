#pragma once

// Safe retreat point search (F5-07; docs/12 s8, doc s11 s4.3-s4.4, ADR-0006 Ek F5-07). One
// bounded Dijkstra flood from the bot cell finds every reachable cell within the radius (party
// 40 m, solo 150 m, boundary inclusive), scores each candidate with the docs/12 s8 formula and
// returns the best cell together with the path the flood itself built. The path never comes
// closer than meleeClearM to an enemy melee (a bot that starts inside the zone may only leave it
// by moving away) and never enters a forbidden cell from outside (F5-06). Pure logic: the
// standard library only, no server header, no global/static state.
//
// Out of scope here (the caller owns them): when to retreat, the backline/anchor point, the
// last_stand threshold, route danger scoring, path smoothing and the NavFollower/NavReach
// binding.

#include "NavGrid.h"
#include "NavPath.h"
#include "NavDanger.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace BotCore
{
	enum class NavRetreatMode { Party, Solo };

	enum class NavRetreatStatus
	{
		Found,
		NoCandidate,    // no candidate cell within the radius: the caller treats it as last_stand (docs/11 s4.4)
		InvalidStart    // start out of bounds / not Walk (also before NavGrid::Build)
	};

	struct NavRetreatParams
	{
		float partyRadiusM = 40.0f;    // [O] docs/12 s8: R in party mode (docs/11 s4.3)
		float soloRadiusM = 150.0f;    // [O] docs/12 s8: R in solo mode
		float meleeClearM = 8.0f;      // [O] docs/12 s8: a path stays out of 8 m of an enemy melee
		float wDanger = 3.0f;          // [A] w1/w2 merged (one NavCostLayer)
		float wPath = 1.0f;            // [A] w3
		float wAnchor = 1.5f;          // [A] w4
		float wClear = 0.5f;           // [A] w5, applied to min(clearance, 3) / 3
		float wSafe = 1.0f;            // [A] bonus for a Safe-flagged cell (own tower ring, docs/12 s7)
	};

	struct NavRetreatQuery
	{
		NavCell start;
		NavRetreatMode mode = NavRetreatMode::Party;
		bool hasAnchor = false;                  // party: backline point; solo: own gate (caller decides)
		float anchorX = 0.0f;                    // world metres
		float anchorZ = 0.0f;
		const NavThreat * threats = nullptr;     // only Melee entries are used (the 8 m rule)
		size_t threatCount = 0;
	};

	struct NavRetreatResult
	{
		NavRetreatStatus status = NavRetreatStatus::NoCandidate;
		NavCell cell;                            // chosen cell; meaningful only when Found (may equal the start)
		float score = 0.0f;
		float pathLengthM = 0.0f;                // geometric length of `path`
		uint8_t danger = 0;                      // layer danger at `cell`
		bool safe = false;                       // layer Safe flag at `cell`
		std::vector<NavCell> path;               // start..cell inclusive; empty unless Found
		int candidates = 0;                      // cells that qualified as candidates
		int expanded = 0;                        // settled (closed) cells
	};

	class NavRetreatPlanner
	{
	public:
		// Reusable between queries and grids (buffers are resized when the grid size changes); NOT
		// thread-safe: one instance per thread/context. Deterministic. `grid` and `layer` are never
		// modified; `layer` may be null (no danger, no forbidden, no safe) and is ignored when
		// layer->Size() != grid.Size(). `out` is fully overwritten.
		void Find(const NavGrid & grid, const NavCostLayer * layer, const NavRetreatQuery & query,
			const NavRetreatParams & params, NavRetreatResult & out);

	private:
		struct OpenNode
		{
			float d;
			int32_t idx;
		};

		// Returns true when `a` is a worse open node than `b`: larger distance, then larger index.
		struct OpenWorse
		{
			bool operator()(const OpenNode & a, const OpenNode & b) const
			{
				if (a.d != b.d)
					return a.d > b.d;
				return a.idx > b.idx;
			}
		};

		void ResetPool(int n, size_t cells)
		{
			m_n = n;
			m_poolCells = cells;
			m_dist.assign(cells, 0.0f);
			m_parent.assign(cells, -1);
			m_seen.assign(cells, (uint32_t)0);
			m_closed.assign(cells, (uint32_t)0);
			m_zone.assign(cells, (uint32_t)0);
			m_zoneD2.assign(cells, 0.0f);
			m_generation = 0;
			m_heap.clear();
			m_heap.reserve(4096);
		}

		int m_n = -1;
		size_t m_poolCells = 0;
		uint32_t m_generation = 0;
		std::vector<float> m_dist;         // shortest geometric path length to the cell
		std::vector<int32_t> m_parent;     // parent index in the flood tree
		std::vector<uint32_t> m_seen;      // generation stamps: dist/parent valid
		std::vector<uint32_t> m_closed;    // generation stamps: settled
		std::vector<uint32_t> m_zone;      // generation stamps: zoneD2 valid (melee clear zone)
		std::vector<float> m_zoneD2;       // squared distance to the nearest melee clear zone
		std::vector<OpenNode> m_heap;
	};

	inline void NavRetreatPlanner::Find(const NavGrid & grid, const NavCostLayer * layer,
		const NavRetreatQuery & query, const NavRetreatParams & params, NavRetreatResult & out)
	{
		out.status = NavRetreatStatus::NoCandidate;
		out.cell = query.start;
		out.score = 0.0f;
		out.pathLengthM = 0.0f;
		out.danger = 0;
		out.safe = false;
		out.path.clear();
		out.candidates = 0;
		out.expanded = 0;

		const int n = grid.Size();
		const size_t cells = (size_t)n * (size_t)n;
		if (n != m_n || cells != m_poolCells)
			ResetPool(n, cells);

		++m_generation;
		if (m_generation == 0)
		{
			// Generation stamp wrapped: clear the stamps and restart at 1.
			std::fill(m_seen.begin(), m_seen.end(), (uint32_t)0);
			std::fill(m_closed.begin(), m_closed.end(), (uint32_t)0);
			std::fill(m_zone.begin(), m_zone.end(), (uint32_t)0);
			m_generation = 1;
		}

		if (!grid.Walk(query.start.x, query.start.z))
		{
			out.status = NavRetreatStatus::InvalidStart;
			return;
		}

		const NavCostLayer * zones = (layer != nullptr && layer->Size() == n) ? layer : nullptr;

		float radius = (query.mode == NavRetreatMode::Party) ? params.partyRadiusM : params.soloRadiusM;
		if (radius < 0.0f)
			radius = 0.0f;
		const float clearM = params.meleeClearM > 0.0f ? params.meleeClearM : 0.0f;

		// Negative weights count as 0.
		const float wDanger = params.wDanger > 0.0f ? params.wDanger : 0.0f;
		const float wPath = params.wPath > 0.0f ? params.wPath : 0.0f;
		const float wAnchor = params.wAnchor > 0.0f ? params.wAnchor : 0.0f;
		const float wClear = params.wClear > 0.0f ? params.wClear : 0.0f;
		const float wSafe = params.wSafe > 0.0f ? params.wSafe : 0.0f;

		const float unit = grid.Unit();

		// Melee clear zone: every cell whose centre is within clearM of a melee threat, keyed by
		// generation stamp, storing the smallest squared distance. Only Melee entries count.
		if (query.threats != nullptr && query.threatCount > 0)
		{
			const float clear2 = clearM * clearM;
			for (size_t t = 0; t < query.threatCount; ++t)
			{
				const NavThreat & threat = query.threats[t];
				if (threat.kind != NavThreatKind::Melee)
					continue;

				int loX = (int)std::floor((threat.x - clearM) / unit);
				int hiX = (int)std::floor((threat.x + clearM) / unit);
				int loZ = (int)std::floor((threat.z - clearM) / unit);
				int hiZ = (int)std::floor((threat.z + clearM) / unit);
				if (loX < 0)
					loX = 0;
				if (loZ < 0)
					loZ = 0;
				if (hiX > n - 1)
					hiX = n - 1;
				if (hiZ > n - 1)
					hiZ = n - 1;

				for (int cx = loX; cx <= hiX; ++cx)
				{
					const float dx = ((float)cx + 0.5f) * unit - threat.x;
					for (int cz = loZ; cz <= hiZ; ++cz)
					{
						const float dz = ((float)cz + 0.5f) * unit - threat.z;
						const float d2 = dx * dx + dz * dz;
						if (d2 > clear2)
							continue;
						const size_t idx = (size_t)cx * (size_t)n + (size_t)cz;
						if (m_zone[idx] != m_generation || d2 < m_zoneD2[idx])
						{
							m_zone[idx] = m_generation;
							m_zoneD2[idx] = d2;
						}
					}
				}
			}
		}

		const int startIdx = query.start.x * n + query.start.z;
		m_dist[(size_t)startIdx] = 0.0f;
		m_parent[(size_t)startIdx] = -1;
		m_seen[(size_t)startIdx] = m_generation;

		m_heap.clear();
		{
			OpenNode node;
			node.d = 0.0f;
			node.idx = (int32_t)startIdx;
			m_heap.push_back(node);
			std::push_heap(m_heap.begin(), m_heap.end(), OpenWorse());
		}

		const int dxs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		const int dzs[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

		bool hasBest = false;
		float bestScore = 0.0f;
		int bestIdx = -1;
		int bestDanger = 0;
		bool bestSafe = false;
		int expanded = 0;
		int candidates = 0;

		while (!m_heap.empty())
		{
			// Lazy deletion: stale (already closed) entries are dropped without counting.
			std::pop_heap(m_heap.begin(), m_heap.end(), OpenWorse());
			const OpenNode top = m_heap.back();
			m_heap.pop_back();

			if (m_closed[(size_t)top.idx] == m_generation)
				continue;

			m_closed[(size_t)top.idx] = m_generation;
			++expanded;

			const int cx = top.idx / n;
			const int cz = top.idx % n;
			const float d = m_dist[(size_t)top.idx];

			const bool curForbidden = zones != nullptr && zones->Forbidden(cx, cz);
			const bool curInZone = m_zone[(size_t)top.idx] == m_generation;

			// A settled cell is a candidate when it is neither forbidden nor inside a melee clear
			// zone (the start cell may be a candidate too, with path length 0).
			if (!curForbidden && !curInZone)
			{
				++candidates;
				const int danger = zones != nullptr ? (int)zones->Danger(cx, cz) : 0;

				float s = 0.0f;
				s -= wDanger * (float)danger / 255.0f;
				if (radius > 0.0f)
				{
					s -= wPath * (d / radius);
					if (query.hasAnchor)
					{
						const float px = ((float)cx + 0.5f) * unit;
						const float pz = ((float)cz + 0.5f) * unit;
						const float ax = px - query.anchorX;
						const float az = pz - query.anchorZ;
						const float da = std::sqrt(ax * ax + az * az);
						float closeness = 1.0f - da / radius;
						if (closeness < 0.0f)
							closeness = 0.0f;
						s += wAnchor * closeness;
					}
				}
				const int clearance = (int)grid.Clearance(cx, cz);
				s += wClear * (float)(clearance < 3 ? clearance : 3) / 3.0f;
				const bool isSafe = zones != nullptr && zones->Safe(cx, cz);
				if (isSafe)
					s += wSafe;

				// Highest score wins; equal scores keep the earlier cell (shorter path, then
				// smaller index), which the pop order already guarantees.
				if (!hasBest || s > bestScore)
				{
					hasBest = true;
					bestScore = s;
					bestIdx = top.idx;
					bestDanger = danger;
					bestSafe = isSafe;
				}
			}

			const float d2cur = curInZone ? m_zoneD2[(size_t)top.idx] : std::numeric_limits<float>::max();

			for (int k = 0; k < 8; ++k)
			{
				const int dx = dxs[k];
				const int dz = dzs[k];
				if (!grid.EdgeOpen(cx, cz, dx, dz))
					continue;

				const int nx = cx + dx;
				const int nz = cz + dz;
				const int nidx = nx * n + nz;
				if (m_closed[(size_t)nidx] == m_generation)
					continue;

				const float step = (dx != 0 && dz != 0) ? (unit * std::sqrt(2.0f)) : unit;
				const float nd = d + step;
				if (nd > radius)
					continue;   // boundary inclusive: nd <= radius stays

				// May not ENTER a forbidden cell from outside (F5-06 rule).
				if (zones != nullptr && !curForbidden && zones->Forbidden(nx, nz))
					continue;

				// May not step closer to a melee while inside its clear zone (one-way rule).
				if (m_zone[(size_t)nidx] == m_generation && m_zoneD2[(size_t)nidx] < d2cur)
					continue;

				if (m_seen[(size_t)nidx] != m_generation || nd < m_dist[(size_t)nidx])
				{
					m_seen[(size_t)nidx] = m_generation;
					m_dist[(size_t)nidx] = nd;
					m_parent[(size_t)nidx] = top.idx;
					OpenNode node;
					node.d = nd;
					node.idx = (int32_t)nidx;
					m_heap.push_back(node);
					std::push_heap(m_heap.begin(), m_heap.end(), OpenWorse());
				}
			}
		}

		if (!hasBest)
		{
			out.status = NavRetreatStatus::NoCandidate;
			out.candidates = 0;
			out.expanded = expanded;
			return;
		}

		out.status = NavRetreatStatus::Found;
		out.cell.x = bestIdx / n;
		out.cell.z = bestIdx % n;
		out.score = bestScore;
		out.pathLengthM = m_dist[(size_t)bestIdx];
		out.danger = (uint8_t)bestDanger;
		out.safe = bestSafe;
		out.candidates = candidates;
		out.expanded = expanded;

		out.path.clear();
		int cur = bestIdx;
		while (cur != -1)
		{
			NavCell c;
			c.x = cur / n;
			c.z = cur % n;
			out.path.push_back(c);
			if (cur == startIdx)
				break;
			cur = m_parent[(size_t)cur];
		}
		std::reverse(out.path.begin(), out.path.end());
	}
}
