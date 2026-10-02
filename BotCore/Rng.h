#pragma once

#include <cstdint>

namespace BotCore
{
	// Reference SplitMix64: advances `state` and returns the next output.
	uint64_t SplitMix64(uint64_t & state);

	// Per-bot seed (docs/13 s13): one SplitMix64 step over (episodeSeed << 32) | botSlot.
	uint64_t DeriveBotSeed(uint32_t episodeSeed, uint32_t botSlot);

	// xoshiro256**; state is filled by four SplitMix64 steps starting from `seed`.
	class Rng
	{
	public:
		explicit Rng(uint64_t seed);

		uint64_t NextU64();
		uint32_t NextU32();
		uint32_t NextBelow(uint32_t bound);
		int32_t  NextRange(int32_t lo, int32_t hi);
		double   NextDouble();

	private:
		uint64_t m_s[4];
	};
}
