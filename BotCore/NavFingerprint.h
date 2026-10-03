#pragma once

// Fingerprint of a NavGrid in the .navgrid file layout (F5-59): CRC32 (zlib / IEEE 802.3) over
//   "FDPNAV01", int32 n, float32 unit, int16 events[n*n], float32 heights[n*n]  (little-endian host).
// Lets the server prove that a grid copied from its in-memory SMD data equals the exported file.

#include "NavGrid.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace BotCore
{
	class NavCrc32
	{
	public:
		NavCrc32();                                   // builds the 256-entry table (member array, no statics)
		void Update(const void * data, size_t size);  // streaming
		uint32_t Value() const;                       // final xor applied; Value() may be called repeatedly

	private:
		std::array<uint32_t, 256> m_table;
		uint32_t m_state;                             // starts at 0xFFFFFFFF
	};

	// Events then heights in x-major order (index x * n + z), exactly the file order.
	inline uint32_t NavGridFingerprint(const NavGrid & grid);   // 0 when grid.Size() < 2

	inline NavCrc32::NavCrc32()
		: m_state(0xFFFFFFFFu)
	{
		for (uint32_t i = 0; i < 256; ++i)
		{
			uint32_t c = i;
			for (int k = 0; k < 8; ++k)
				c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
			m_table[i] = c;
		}
	}

	inline void NavCrc32::Update(const void * data, size_t size)
	{
		const uint8_t * p = static_cast<const uint8_t *>(data);
		uint32_t crc = m_state;
		for (size_t i = 0; i < size; ++i)
			crc = m_table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
		m_state = crc;
	}

	inline uint32_t NavCrc32::Value() const
	{
		return m_state ^ 0xFFFFFFFFu;
	}

	inline uint32_t NavGridFingerprint(const NavGrid & grid)
	{
		const int n = grid.Size();
		if (n < 2)
			return 0;

		NavCrc32 crc;

		const char magic[8] = { 'F', 'D', 'P', 'N', 'A', 'V', '0', '1' };
		crc.Update(magic, sizeof(magic));

		const int32_t n32 = (int32_t)n;
		crc.Update(&n32, sizeof(n32));

		const float unit = grid.Unit();
		crc.Update(&unit, sizeof(unit));

		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				const int16_t e = grid.Event(x, z);
				crc.Update(&e, sizeof(e));
			}
		}

		for (int x = 0; x < n; ++x)
		{
			for (int z = 0; z < n; ++z)
			{
				const float h = grid.Height(x, z);
				crc.Update(&h, sizeof(h));
			}
		}

		return crc.Value();
	}
}
