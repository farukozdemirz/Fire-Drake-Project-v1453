#pragma once

// Danger cost layers and team safety zones for the F5-02 A* (F5-06; docs/12 s2, s4.1, s7,
// ADR-0006 Ek F5-06). Per cell a danger value (0..255 = 0.0..1.0), a forbidden flag (hard: a path
// may not ENTER it, AC-NAV-06) and a safe flag (marking only; consumed by the safe-point search,
// F5-07). A team's static layer (enemy gate ring forbidden, own gate ring safe) is built once; the
// dynamic layer of a frame is a copy of the static layer plus AddThreats. Pure logic: the standard
// library only, no server header, no global/static state.

#include "NavGrid.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BotCore
{
	enum class NavThreatKind { Melee, Ranged };

	struct NavThreat
	{
		float x = 0.0f;               // world metres
		float z = 0.0f;
		NavThreatKind kind = NavThreatKind::Melee;
	};

	struct NavThreatParams
	{
		float meleeCoreM = 15.0f;     // [O] docs/12 s2: melee core
		float meleeWeight = 1.0f;     // [A]
		float rangedReachM = 45.0f;   // [O] docs/12 s2: mage reach (a disc: everywhere in reach is dangerous)
		float rangedWeight = 0.6f;    // [A] (< melee: a mage can be out-ranged, a melee cannot)
		float fadeM = 8.0f;           // [A] linear falloff outside the core / reach
	};

	// One team's view of the zone (docs/12 s2 danger_static / danger_dynamic): per cell a danger
	// value, a forbidden flag and a safe flag. Copyable: the dynamic layer of a frame is
	// `dynamic = staticLayer; dynamic.AddThreats(...)` (the copy reuses its capacity).
	class NavCostLayer
	{
	public:
		// Sizes the layer for `grid` (side and unit are copied) and clears it. Size() == 0 when
		// grid.Size() < 2. Must be called before any Add*; the Add* calls of an un-Init'ed layer
		// are no-ops.
		void Init(const NavGrid & grid);
		void Clear();                              // danger 0, no flags; keeps the size

		int     Size() const;                      // grid side at Init (0 before)
		uint8_t Danger(int x, int z) const;        // 0 off-grid and before Init
		bool    Forbidden(int x, int z) const;     // false off-grid and before Init
		bool    Safe(int x, int z) const;          // false off-grid and before Init

		// Danger band around (cx, cz) in metres: weight inside [innerM, outerM] (distance from the
		// cell centre to the point, boundaries inclusive), falling linearly to 0 over fadeM outside
		// both edges. Max-combined with the existing value (order independent). weight is clamped
		// to [0, 1] (weight <= 0 is a no-op); innerM < 0 is 0; outerM < innerM is innerM; fadeM < 0
		// is 0 (a hard edge). Only cells in the bounding box [centre -+ (outerM + fadeM)] are
		// visited (clamped to the grid).
		void AddDangerBand(float cx, float cz, float innerM, float outerM, float fadeM, float weight);
		void AddThreats(const NavThreat * threats, size_t count, const NavThreatParams & params);

		// Cells whose centre is within radiusM of (cx, cz) (squared distance <= radius^2, boundary
		// inclusive); radiusM < 0 is a no-op. Flags are independent (a cell may be both).
		void AddForbidDisc(float cx, float cz, float radiusM);
		void AddSafeDisc(float cx, float cz, float radiusM);
		// Every cell whose centre is farther than radiusM (test-mode arena bound, docs/12 s7).
		void AddForbidOutsideDisc(float cx, float cz, float radiusM);

	private:
		int m_n = 0;
		float m_unit = 0.0f;
		std::vector<uint8_t> m_danger;   // n*n, index x*n+z
		std::vector<uint8_t> m_flags;    // n*n, bit 0 = forbidden, bit 1 = safe
	};

	struct NavCostParams
	{
		float wDanger = 4.0f;            // [A] docs/12 s4.1 w_danger
		float wClear = 0.5f;             // [A] docs/12 s4.1 w_clear
		int   clearFree = 2;             // [O] docs/12 s4.1: max(0, 2 - clearance)
		float forbiddenPenalty = 10.0f;  // [A] per forbidden cell (only reachable when the start is inside)
	};

	struct NavCostField
	{
		const NavCostLayer * layer = nullptr;   // may be null: clearance term only, no forbidden rule
		NavCostParams params;
	};

	// Penalty of one cell (>= 0): wDanger * danger/255 + forbiddenPenalty (when forbidden)
	// + wClear * max(0, clearFree - clearance). Negative weights count as 0. A layer whose
	// Size() != grid.Size() (or null) contributes nothing.
	inline float NavCellPenalty(const NavGrid & grid, const NavCostLayer * layer, const NavCostParams & params,
		int x, int z)
	{
		float penalty = 0.0f;

		if (layer != nullptr && layer->Size() == grid.Size())
		{
			const float wd = params.wDanger > 0.0f ? params.wDanger : 0.0f;
			penalty += wd * (float)layer->Danger(x, z) / 255.0f;
			if (layer->Forbidden(x, z))
			{
				const float fp = params.forbiddenPenalty > 0.0f ? params.forbiddenPenalty : 0.0f;
				penalty += fp;
			}
		}

		const int deficit = params.clearFree - (int)grid.Clearance(x, z);
		if (deficit > 0)
		{
			const float wc = params.wClear > 0.0f ? params.wClear : 0.0f;
			penalty += wc * (float)deficit;
		}

		return penalty;
	}

	struct NavZoneParams
	{
		float towerRingM = 90.0f;     // [O] docs/12 s7: gate-ring radius
	};

	// A team's static layer: the ENEMY gate ring is forbidden, the OWN gate ring is marked safe.
	inline void NavBuildTeamZones(const NavGrid & grid, float enemyGateX, float enemyGateZ, float ownGateX,
		float ownGateZ, const NavZoneParams & params, NavCostLayer & out)
	{
		out.Init(grid);
		out.AddForbidDisc(enemyGateX, enemyGateZ, params.towerRingM);
		out.AddSafeDisc(ownGateX, ownGateZ, params.towerRingM);
	}

	inline void NavCostLayer::Init(const NavGrid & grid)
	{
		m_unit = grid.Unit();
		if (grid.Size() < 2)
		{
			m_n = 0;
			m_danger.clear();
			m_flags.clear();
			return;
		}

		m_n = grid.Size();
		const size_t cells = (size_t)m_n * (size_t)m_n;
		m_danger.assign(cells, 0);
		m_flags.assign(cells, 0);
	}

	inline void NavCostLayer::Clear()
	{
		std::fill(m_danger.begin(), m_danger.end(), (uint8_t)0);
		std::fill(m_flags.begin(), m_flags.end(), (uint8_t)0);
	}

	inline int NavCostLayer::Size() const
	{
		return m_n;
	}

	inline uint8_t NavCostLayer::Danger(int x, int z) const
	{
		if (m_n <= 0 || x < 0 || x >= m_n || z < 0 || z >= m_n)
			return 0;
		return m_danger[(size_t)x * (size_t)m_n + (size_t)z];
	}

	inline bool NavCostLayer::Forbidden(int x, int z) const
	{
		if (m_n <= 0 || x < 0 || x >= m_n || z < 0 || z >= m_n)
			return false;
		return (m_flags[(size_t)x * (size_t)m_n + (size_t)z] & 1u) != 0;
	}

	inline bool NavCostLayer::Safe(int x, int z) const
	{
		if (m_n <= 0 || x < 0 || x >= m_n || z < 0 || z >= m_n)
			return false;
		return (m_flags[(size_t)x * (size_t)m_n + (size_t)z] & 2u) != 0;
	}

	inline void NavCostLayer::AddDangerBand(float cx, float cz, float innerM, float outerM, float fadeM, float weight)
	{
		if (m_n <= 0 || weight <= 0.0f)
			return;

		if (weight > 1.0f)
			weight = 1.0f;
		if (innerM < 0.0f)
			innerM = 0.0f;
		if (outerM < innerM)
			outerM = innerM;
		if (fadeM < 0.0f)
			fadeM = 0.0f;

		const float reach = outerM + fadeM;
		int loX = (int)std::floor((cx - reach) / m_unit);
		int hiX = (int)std::floor((cx + reach) / m_unit);
		int loZ = (int)std::floor((cz - reach) / m_unit);
		int hiZ = (int)std::floor((cz + reach) / m_unit);
		if (loX < 0)
			loX = 0;
		if (loZ < 0)
			loZ = 0;
		if (hiX > m_n - 1)
			hiX = m_n - 1;
		if (hiZ > m_n - 1)
			hiZ = m_n - 1;

		for (int x = loX; x <= hiX; ++x)
		{
			const float px = ((float)x + 0.5f) * m_unit;
			const float dx = px - cx;
			for (int z = loZ; z <= hiZ; ++z)
			{
				const float pz = ((float)z + 0.5f) * m_unit;
				const float dz = pz - cz;
				const float r = std::sqrt(dx * dx + dz * dz);

				float t;
				if (innerM > 0.0f && r < innerM)
					t = fadeM > 0.0f ? (innerM - r) / fadeM : 1.0f;
				else if (r <= outerM)
					t = 0.0f;
				else
					t = fadeM > 0.0f ? (r - outerM) / fadeM : 1.0f;

				if (t >= 1.0f)
					continue;

				int d = (int)(255.0f * (weight * (1.0f - t)) + 0.5f);
				if (d > 255)
					d = 255;

				uint8_t & slot = m_danger[(size_t)x * (size_t)m_n + (size_t)z];
				if ((uint8_t)d > slot)
					slot = (uint8_t)d;
			}
		}
	}

	inline void NavCostLayer::AddThreats(const NavThreat * threats, size_t count, const NavThreatParams & params)
	{
		if (threats == nullptr || count == 0)
			return;

		for (size_t i = 0; i < count; ++i)
		{
			const NavThreat & threat = threats[i];
			if (threat.kind == NavThreatKind::Ranged)
				AddDangerBand(threat.x, threat.z, 0.0f, params.rangedReachM, params.fadeM, params.rangedWeight);
			else
				AddDangerBand(threat.x, threat.z, 0.0f, params.meleeCoreM, params.fadeM, params.meleeWeight);
		}
	}

	inline void NavCostLayer::AddForbidDisc(float cx, float cz, float radiusM)
	{
		if (m_n <= 0 || radiusM < 0.0f)
			return;

		const float r2 = radiusM * radiusM;
		for (int x = 0; x < m_n; ++x)
		{
			const float px = ((float)x + 0.5f) * m_unit;
			const float dx = px - cx;
			for (int z = 0; z < m_n; ++z)
			{
				const float pz = ((float)z + 0.5f) * m_unit;
				const float dz = pz - cz;
				if (dx * dx + dz * dz <= r2)
					m_flags[(size_t)x * (size_t)m_n + (size_t)z] |= 1u;
			}
		}
	}

	inline void NavCostLayer::AddSafeDisc(float cx, float cz, float radiusM)
	{
		if (m_n <= 0 || radiusM < 0.0f)
			return;

		const float r2 = radiusM * radiusM;
		for (int x = 0; x < m_n; ++x)
		{
			const float px = ((float)x + 0.5f) * m_unit;
			const float dx = px - cx;
			for (int z = 0; z < m_n; ++z)
			{
				const float pz = ((float)z + 0.5f) * m_unit;
				const float dz = pz - cz;
				if (dx * dx + dz * dz <= r2)
					m_flags[(size_t)x * (size_t)m_n + (size_t)z] |= 2u;
			}
		}
	}

	inline void NavCostLayer::AddForbidOutsideDisc(float cx, float cz, float radiusM)
	{
		if (m_n <= 0)
			return;

		const float r2 = radiusM * radiusM;
		for (int x = 0; x < m_n; ++x)
		{
			const float px = ((float)x + 0.5f) * m_unit;
			const float dx = px - cx;
			for (int z = 0; z < m_n; ++z)
			{
				const float pz = ((float)z + 0.5f) * m_unit;
				const float dz = pz - cz;
				if (dx * dx + dz * dz > r2)
					m_flags[(size_t)x * (size_t)m_n + (size_t)z] |= 1u;
			}
		}
	}
}
