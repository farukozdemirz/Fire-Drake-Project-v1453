#include "Rng.h"

namespace BotCore
{
	namespace
	{
		// Rotates x left by k bits (caller passes 1..63).
		uint64_t Rotl(uint64_t x, int k)
		{
			return (x << k) | (x >> (64 - k));
		}
	}

	uint64_t SplitMix64(uint64_t & state)
	{
		state += 0x9E3779B97F4A7C15ull;
		uint64_t z = state;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return z ^ (z >> 31);
	}

	uint64_t DeriveBotSeed(uint32_t episodeSeed, uint32_t botSlot)
	{
		uint64_t st = (uint64_t(episodeSeed) << 32) | uint64_t(botSlot);
		return SplitMix64(st);
	}

	Rng::Rng(uint64_t seed)
	{
		uint64_t st = seed;
		for (int i = 0; i < 4; ++i)
		{
			m_s[i] = SplitMix64(st);
		}
	}

	uint64_t Rng::NextU64()
	{
		uint64_t result = Rotl(m_s[1] * 5, 7) * 9;
		uint64_t t = m_s[1] << 17;
		m_s[2] ^= m_s[0];
		m_s[3] ^= m_s[1];
		m_s[1] ^= m_s[2];
		m_s[0] ^= m_s[3];
		m_s[2] ^= t;
		m_s[3] = Rotl(m_s[3], 45);
		return result;
	}

	uint32_t Rng::NextU32()
	{
		return uint32_t(NextU64() >> 32);
	}

	uint32_t Rng::NextBelow(uint32_t bound)
	{
		if (bound == 0)
		{
			return 0;
		}
		uint32_t threshold = (0u - bound) % bound;
		for (;;)
		{
			uint32_t r = NextU32();
			if (r >= threshold)
			{
				return r % bound;
			}
		}
	}

	int32_t Rng::NextRange(int32_t lo, int32_t hi)
	{
		uint32_t span = uint32_t(hi) - uint32_t(lo) + 1u;
		if (span == 0)
		{
			return int32_t(NextU32());
		}
		return int32_t(uint32_t(lo) + NextBelow(span));
	}

	double Rng::NextDouble()
	{
		return double(NextU64() >> 11) * (1.0 / 9007199254740992.0);
	}
}
