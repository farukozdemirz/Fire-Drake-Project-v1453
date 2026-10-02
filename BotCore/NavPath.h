#pragma once

// Grid A* path finding (F5-02; docs/12 s4.1, ADR-0006) on top of the F5-01 navigation grid.
// Pure logic: the standard library only, no server header, no global/static state. The grid
// edge rule (walkability, slope, no corner cutting) is never re-implemented here: neighbour
// decisions come from NavGrid::EdgeOpen only. Without a cost field the cost is the horizontal
// distance in metres; with a NavCostField (F5-06) each step is weighted by cell penalties and a
// forbidden zone cannot be entered.

#include "NavGrid.h"
#include "NavDanger.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BotCore
{
	struct NavCell
	{
		int x = 0;
		int z = 0;
	};
	inline bool operator==(const NavCell & a, const NavCell & b) { return a.x == b.x && a.z == b.z; }
	inline bool operator!=(const NavCell & a, const NavCell & b) { return !(a == b); }

	enum class NavPathStatus
	{
		Found,
		NoPath,         // the reachable region was exhausted without meeting the goal
		NodeLimit,      // maxNodes closed nodes without meeting the goal: "unknown", not "unreachable"
		InvalidStart,   // start out of bounds / not Walk (also before NavGrid::Build)
		InvalidGoal     // goal out of bounds / not Walk, or (cost field only) forbidden while the start is not
	};

	struct NavSearchParams
	{
		// P-NAV-MAX-NODES [O] (docs/12 s4.1, ADR-0006): max number of closed (expanded) nodes.
		int maxNodes = 20000;
	};

	struct NavPathResult
	{
		NavPathStatus status = NavPathStatus::NoPath;
		std::vector<NavCell> cells;   // start..goal inclusive; empty unless Found
		float cost = 0.0f;            // weighted step cost (geometric without a cost field); 0 unless Found
		float length = 0.0f;          // metres (geometric length of the route); 0 unless Found; equals cost without a field
		int expanded = 0;             // closed nodes, set for every status that ran a search
	};

	// Octile distance in metres between two cells `dx`, `dz` apart (consistent heuristic for
	// unit / unit*sqrt(2) steps): unit * (|dx| + |dz| - 2*min + sqrt(2)*min).
	inline float NavOctile(int dx, int dz, float unit)
	{
		const int ax = dx < 0 ? -dx : dx;
		const int az = dz < 0 ? -dz : dz;
		const int mn = ax < az ? ax : az;
		const int mx = ax < az ? az : ax;
		return unit * ((float)(mx - mn) + std::sqrt(2.0f) * (float)mn);
	}

	class NavPathfinder
	{
	public:
		// Reusable between queries and grids (buffers are resized when the grid size changes);
		// NOT thread-safe: one instance per thread/context. Deterministic: same grid, params and
		// query give the same result, also after other queries ran on the same instance.
		// `grid` is never modified. `out` is fully overwritten.
		void Find(const NavGrid & grid, NavCell start, NavCell goal, const NavSearchParams & params,
			NavPathResult & out, const NavCostField * field = nullptr)
		{
			out.status = NavPathStatus::NoPath;
			out.cells.clear();
			out.cost = 0.0f;
			out.length = 0.0f;
			out.expanded = 0;

			const bool weighted = field != nullptr;
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
				m_generation = 1;
			}

			const NavCostLayer * zones = nullptr;
			if (weighted && field->layer != nullptr && field->layer->Size() == n)
				zones = field->layer;

			if (!grid.Walk(start.x, start.z))
			{
				out.status = NavPathStatus::InvalidStart;
				return;
			}
			if (!grid.Walk(goal.x, goal.z))
			{
				out.status = NavPathStatus::InvalidGoal;
				return;
			}
			if (weighted && zones != nullptr && zones->Forbidden(goal.x, goal.z) && !zones->Forbidden(start.x, start.z))
			{
				out.status = NavPathStatus::InvalidGoal;
				return;
			}
			if (start == goal)
			{
				out.status = NavPathStatus::Found;
				out.cells.push_back(start);
				out.cost = 0.0f;
				return;
			}

			const float unit = grid.Unit();
			const int startIdx = start.x * n + start.z;
			const int goalIdx = goal.x * n + goal.z;

			const int dxs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
			const int dzs[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

			m_g[(size_t)startIdx] = 0.0f;
			m_parent[(size_t)startIdx] = -1;
			m_seen[(size_t)startIdx] = m_generation;

			m_heap.clear();
			m_heap.push_back(MakeNode(startIdx, 0.0f, goal, n, unit));
			std::push_heap(m_heap.begin(), m_heap.end(), OpenWorse());

			int expanded = 0;
			while (!m_heap.empty())
			{
				// Lazy deletion: stale (already closed) entries are dropped without counting.
				std::pop_heap(m_heap.begin(), m_heap.end(), OpenWorse());
				const OpenNode top = m_heap.back();
				m_heap.pop_back();

				if (m_closed[(size_t)top.idx] == m_generation)
					continue;

				if (expanded >= params.maxNodes)
				{
					out.status = NavPathStatus::NodeLimit;
					out.expanded = expanded;
					return;
				}

				m_closed[(size_t)top.idx] = m_generation;
				++expanded;

				if (top.idx == goalIdx)
				{
					out.status = NavPathStatus::Found;
					out.expanded = expanded;
					out.cost = m_g[(size_t)goalIdx];
					Reconstruct(startIdx, goalIdx, n, out.cells);
					out.length = 0.0f;
					for (size_t i = 0; i + 1 < out.cells.size(); ++i)
					{
						const int sdx = out.cells[i + 1].x - out.cells[i].x;
						const int sdz = out.cells[i + 1].z - out.cells[i].z;
						out.length += (sdx != 0 && sdz != 0) ? (unit * std::sqrt(2.0f)) : unit;
					}
					return;
				}

				const int cx = top.idx / n;
				const int cz = top.idx % n;
				const float gcur = m_g[(size_t)top.idx];
				const float penCur = weighted ? NavCellPenalty(grid, zones, field->params, cx, cz) : 0.0f;
				const bool curForbidden = weighted && zones != nullptr && zones->Forbidden(cx, cz);
				for (int k = 0; k < 8; ++k)
				{
					const int dx = dxs[k];
					const int dz = dzs[k];
					if (!grid.EdgeOpen(cx, cz, dx, dz))
						continue;

					const int nidx = (cx + dx) * n + (cz + dz);
					if (m_closed[(size_t)nidx] == m_generation)
						continue;

					const float step = (dx != 0 && dz != 0) ? (unit * std::sqrt(2.0f)) : unit;

					float ng;
					if (!weighted)
					{
						ng = gcur + step;
					}
					else
					{
						const int nx = cx + dx;
						const int nz = cz + dz;
						if (zones != nullptr && !curForbidden && zones->Forbidden(nx, nz))
							continue;   // may not ENTER a forbidden cell from outside
						const float penNb = NavCellPenalty(grid, zones, field->params, nx, nz);
						ng = gcur + step * (1.0f + 0.5f * (penCur + penNb));
					}

					if (m_seen[(size_t)nidx] != m_generation || ng < m_g[(size_t)nidx])
					{
						m_seen[(size_t)nidx] = m_generation;
						m_g[(size_t)nidx] = ng;
						m_parent[(size_t)nidx] = top.idx;
						m_heap.push_back(MakeNode(nidx, ng, goal, n, unit));
						std::push_heap(m_heap.begin(), m_heap.end(), OpenWorse());
					}
				}
			}

			out.status = NavPathStatus::NoPath;
			out.expanded = expanded;
		}

	private:
		struct OpenNode
		{
			float f;
			float g;
			int32_t idx;
		};

		// Returns true when `a` is a worse open node than `b`, so std::push_heap keeps the best
		// node at the top: smaller f, then larger g, then smaller idx.
		struct OpenWorse
		{
			bool operator()(const OpenNode & a, const OpenNode & b) const
			{
				if (a.f != b.f)
					return a.f > b.f;
				if (a.g != b.g)
					return a.g < b.g;
				return a.idx > b.idx;
			}
		};

		OpenNode MakeNode(int idx, float g, NavCell goal, int n, float unit) const
		{
			OpenNode node;
			node.idx = (int32_t)idx;
			node.g = g;
			node.f = g + NavOctile(goal.x - idx / n, goal.z - idx % n, unit);
			return node;
		}

		void Reconstruct(int startIdx, int goalIdx, int n, std::vector<NavCell> & cells) const
		{
			cells.clear();
			int cur = goalIdx;
			while (cur != -1)
			{
				NavCell cell;
				cell.x = cur / n;
				cell.z = cur % n;
				cells.push_back(cell);
				if (cur == startIdx)
					break;
				cur = m_parent[(size_t)cur];
			}
			std::reverse(cells.begin(), cells.end());
		}

		void ResetPool(int n, size_t cells)
		{
			m_n = n;
			m_poolCells = cells;
			m_g.assign(cells, 0.0f);
			m_parent.assign(cells, -1);
			m_seen.assign(cells, (uint32_t)0);
			m_closed.assign(cells, (uint32_t)0);
			m_generation = 0;
			m_heap.clear();
			m_heap.reserve(4096);
		}

		int m_n = -1;
		size_t m_poolCells = 0;
		uint32_t m_generation = 0;
		std::vector<float> m_g;
		std::vector<int32_t> m_parent;
		std::vector<uint32_t> m_seen;
		std::vector<uint32_t> m_closed;
		std::vector<OpenNode> m_heap;
	};
}
