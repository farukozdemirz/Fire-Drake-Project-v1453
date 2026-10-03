#pragma once

// Navigation grid data model (F5-01; docs/12 s1-s2): loads the exported zone event and height
// grids, computes the walkable main component ("walk") and a clearance layer, and answers edge
// (slope plus no corner cutting) and height queries. Pure logic: the standard library only, no
// server header, and no path finding (that is F5-02).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

namespace BotCore
{
	struct NavParams
	{
		// P-NAV-MAX-SLOPE [V]: max |dh| / horizontal distance of one edge
		// (1.8 m per 4 m cell = 0.45). Measured by T-NAV-02 (human client, 2026-10-03):
		// sustained climbs up to 0.47, 0.78 and steeper failed; ADR-0024.
		float maxSlope = 0.45f;
	};

	class NavGrid
	{
	public:
		// Takes ownership of the arrays; false if n < 2, unit <= 0 or an array size != n*n.
		bool Init(int n, float unit, std::vector<int16_t> events, std::vector<float> heights);

		// Parses the s5.1 format (exact size required); calls Init on success.
		bool Load(const uint8_t * data, size_t size);
		bool LoadFile(const char * path);

		// Computes `walk` and `clearance`. Must be called after Init/Load; may be called again
		// with other params. Accessors below return false/0 before the first Build.
		void Build(const NavParams & params = NavParams());

		int   Size() const { return m_n; }
		float Unit() const { return m_unit; }
		bool  InBounds(int x, int z) const;
		int   CellOf(float world) const;
		float CellCenter(int i) const;

		int16_t Event(int x, int z) const;
		float   Height(int x, int z) const;
		bool    Walk(int x, int z) const;
		uint8_t Clearance(int x, int z) const;
		bool    EdgeOpen(int x, int z, int dx, int dz) const;
		float   HeightAt(float wx, float wz) const;
		int     MainComponentCells() const { return m_mainCells; }
		const NavParams & Params() const { return m_params; }

	private:
		int Idx(int x, int z) const { return x * m_n + z; }

		int   m_n = 0;
		float m_unit = 0.0f;
		std::vector<int16_t> m_events;
		std::vector<float>   m_heights;
		std::vector<uint8_t> m_walk;
		std::vector<uint8_t> m_clearance;
		int   m_mainCells = 0;
		NavParams m_params;
	};

	inline bool NavGrid::Init(int n, float unit, std::vector<int16_t> events, std::vector<float> heights)
	{
		if (n < 2 || unit <= 0.0f)
			return false;

		const size_t cells = (size_t)n * (size_t)n;
		if (events.size() != cells || heights.size() != cells)
			return false;

		m_n = n;
		m_unit = unit;
		m_events = std::move(events);
		m_heights = std::move(heights);
		m_walk.clear();
		m_clearance.clear();
		m_mainCells = 0;
		m_params = NavParams();
		return true;
	}

	inline bool NavGrid::Load(const uint8_t * data, size_t size)
	{
		if (data == nullptr || size < 16)
			return false;
		if (std::memcmp(data, "FDPNAV01", 8) != 0)
			return false;

		int32_t n = 0;
		float unit = 0.0f;
		std::memcpy(&n, data + 8, sizeof(n));
		std::memcpy(&unit, data + 12, sizeof(unit));
		if (n < 2 || unit <= 0.0f)
			return false;

		const int64_t cells = (int64_t)n * (int64_t)n;
		const int64_t expected = 16 + 6 * cells;
		if ((int64_t)size != expected)
			return false;

		std::vector<int16_t> events((size_t)cells);
		std::memcpy(events.data(), data + 16, (size_t)cells * sizeof(int16_t));
		std::vector<float> heights((size_t)cells);
		std::memcpy(heights.data(), data + 16 + (size_t)cells * sizeof(int16_t), (size_t)cells * sizeof(float));

		return Init(n, unit, std::move(events), std::move(heights));
	}

	inline bool NavGrid::LoadFile(const char * path)
	{
		if (path == nullptr)
			return false;

		std::ifstream in(path, std::ios::binary | std::ios::ate);
		if (!in)
			return false;

		const std::streamoff len = in.tellg();
		if (len < 0)
			return false;
		in.seekg(0, std::ios::beg);

		std::vector<uint8_t> buffer((size_t)len);
		if (len > 0 && !in.read(reinterpret_cast<char *>(buffer.data()), (std::streamsize)len))
			return false;

		return Load(buffer.data(), buffer.size());
	}

	inline void NavGrid::Build(const NavParams & params)
	{
		m_params = params;

		if (m_n < 2 || m_events.size() != (size_t)m_n * (size_t)m_n || m_events.size() != m_heights.size())
			return;

		const size_t cells = m_events.size();
		m_walk.assign(cells, 0);
		m_clearance.assign(cells, 0);
		m_mainCells = 0;

		// 4-connected components of event==1 cells; any component that touches the map edge is
		// dropped, and the largest remaining one is the main component (iterative BFS).
		std::vector<int32_t> comp(cells, -1);
		std::vector<int32_t> stack;
		std::vector<int32_t> sizes;
		std::vector<uint8_t> touchesEdge;

		for (int x = 0; x < m_n; ++x)
		{
			for (int z = 0; z < m_n; ++z)
			{
				const int start = Idx(x, z);
				if (m_events[start] != 1 || comp[start] >= 0)
					continue;

				const int label = (int)sizes.size();
				sizes.push_back(0);
				touchesEdge.push_back(0);

				stack.clear();
				stack.push_back(start);
				comp[start] = label;

				while (!stack.empty())
				{
					const int cur = stack.back();
					stack.pop_back();
					const int cx = cur / m_n;
					const int cz = cur % m_n;

					sizes[label] += 1;
					if (cx == 0 || cx == m_n - 1 || cz == 0 || cz == m_n - 1)
						touchesEdge[label] = 1;

					const int nx[4] = { cx + 1, cx - 1, cx, cx };
					const int nz[4] = { cz, cz, cz + 1, cz - 1 };
					for (int k = 0; k < 4; ++k)
					{
						if (nx[k] < 0 || nx[k] >= m_n || nz[k] < 0 || nz[k] >= m_n)
							continue;
						const int ni = Idx(nx[k], nz[k]);
						if (m_events[ni] == 1 && comp[ni] < 0)
						{
							comp[ni] = label;
							stack.push_back(ni);
						}
					}
				}
			}
		}

		// Scan order tie-break: the first component of the winning size wins (strict >).
		int best = -1;
		int bestSize = 0;
		for (size_t label = 0; label < sizes.size(); ++label)
		{
			if (touchesEdge[label] == 0 && sizes[label] > bestSize)
			{
				bestSize = sizes[label];
				best = (int)label;
			}
		}

		if (best < 0)
			return;   // no component away from the map edge

		m_mainCells = bestSize;
		for (size_t i = 0; i < cells; ++i)
		{
			if (comp[i] == best)
				m_walk[i] = 1;
		}

		// Clearance: Chebyshev distance (8-connected multi-source BFS) to the nearest cell that
		// is not Walk; non-Walk cells are sources with distance 0, values saturate at 255.
		std::vector<int32_t> dist(cells, -1);
		std::vector<int32_t> queue;
		queue.reserve(cells);
		for (size_t i = 0; i < cells; ++i)
		{
			if (m_walk[i] == 0)
			{
				dist[i] = 0;
				queue.push_back((int32_t)i);
			}
		}

		size_t head = 0;
		while (head < queue.size())
		{
			const int cur = queue[head++];
			const int cx = cur / m_n;
			const int cz = cur % m_n;
			const int d = dist[cur];
			for (int dx = -1; dx <= 1; ++dx)
			{
				for (int dz = -1; dz <= 1; ++dz)
				{
					if (dx == 0 && dz == 0)
						continue;
					const int nx = cx + dx;
					const int nz = cz + dz;
					if (nx < 0 || nx >= m_n || nz < 0 || nz >= m_n)
						continue;
					const int ni = Idx(nx, nz);
					if (m_walk[ni] != 0 && dist[ni] < 0)
					{
						dist[ni] = d + 1;
						queue.push_back((int32_t)ni);
					}
				}
			}
		}

		for (size_t i = 0; i < cells; ++i)
		{
			if (dist[i] < 0)
				m_clearance[i] = 255;
			else
				m_clearance[i] = (uint8_t)(dist[i] > 255 ? 255 : dist[i]);
		}
	}

	inline bool NavGrid::InBounds(int x, int z) const
	{
		return m_n > 0 && x >= 0 && x < m_n && z >= 0 && z < m_n;
	}

	inline int NavGrid::CellOf(float world) const
	{
		if (m_unit <= 0.0f)
			return 0;
		return (int)std::floor(world / m_unit);
	}

	inline float NavGrid::CellCenter(int i) const
	{
		return ((float)i + 0.5f) * m_unit;
	}

	inline int16_t NavGrid::Event(int x, int z) const
	{
		if (!InBounds(x, z))
			return -1;
		return m_events[(size_t)Idx(x, z)];
	}

	inline float NavGrid::Height(int x, int z) const
	{
		if (!InBounds(x, z))
			return 0.0f;
		return m_heights[(size_t)Idx(x, z)];
	}

	inline bool NavGrid::Walk(int x, int z) const
	{
		if (!InBounds(x, z) || m_walk.empty())
			return false;
		return m_walk[(size_t)Idx(x, z)] != 0;
	}

	inline uint8_t NavGrid::Clearance(int x, int z) const
	{
		if (!InBounds(x, z) || m_clearance.empty())
			return 0;
		return m_clearance[(size_t)Idx(x, z)];
	}

	inline bool NavGrid::EdgeOpen(int x, int z, int dx, int dz) const
	{
		if (dx < -1 || dx > 1 || dz < -1 || dz > 1 || (dx == 0 && dz == 0))
			return false;
		if (!Walk(x, z) || !Walk(x + dx, z + dz))
			return false;

		const float distance = (dx != 0 && dz != 0) ? (m_unit * std::sqrt(2.0f)) : m_unit;
		const float dh = std::fabs(Height(x, z) - Height(x + dx, z + dz));
		if (dh > m_params.maxSlope * distance)
			return false;

		if (dx != 0 && dz != 0)
		{
			if (!Walk(x + dx, z) || !Walk(x, z + dz))
				return false;   // no corner cutting (docs/12 s4.1)
		}
		return true;
	}

	inline float NavGrid::HeightAt(float wx, float wz) const
	{
		if (m_n < 2 || m_unit <= 0.0f)
			return 0.0f;

		// Vertex (i, j) sits at world (i*unit, j*unit); clamp the query to the grid extent.
		const float maxIndex = (float)(m_n - 1);
		float fx = wx / m_unit;
		float fz = wz / m_unit;
		if (fx < 0.0f)
			fx = 0.0f;
		if (fx > maxIndex)
			fx = maxIndex;
		if (fz < 0.0f)
			fz = 0.0f;
		if (fz > maxIndex)
			fz = maxIndex;

		int i0 = (int)std::floor(fx);
		int j0 = (int)std::floor(fz);
		if (i0 > m_n - 2)
			i0 = m_n - 2;
		if (j0 > m_n - 2)
			j0 = m_n - 2;

		const float tx = fx - (float)i0;
		const float tz = fz - (float)j0;

		const float h00 = Height(i0, j0);
		const float h10 = Height(i0 + 1, j0);
		const float h01 = Height(i0, j0 + 1);
		const float h11 = Height(i0 + 1, j0 + 1);

		return h00 * (1.0f - tx) * (1.0f - tz)
		     + h10 * tx * (1.0f - tz)
		     + h01 * (1.0f - tx) * tz
		     + h11 * tx * tz;
	}
}
