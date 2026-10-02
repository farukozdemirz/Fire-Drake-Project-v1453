#include "MiniTest.h"

#include <BotCore/Rng.h>

#include <set>

TEST_CASE("Rng_SplitMix64_ReferenceVector")
{
	uint64_t state = 0;
	CHECK_EQ(BotCore::SplitMix64(state), 0xE220A8397B1DCDAFull);
	CHECK_EQ(BotCore::SplitMix64(state), 0x6E789E6AA1B965F4ull);
	CHECK_EQ(BotCore::SplitMix64(state), 0x06C45D188009454Full);
}

TEST_CASE("Rng_DeriveBotSeed_Pinned")
{
	CHECK_EQ(BotCore::DeriveBotSeed(7, 0), 0xBCDA4680438A5951ull);
	CHECK_EQ(BotCore::DeriveBotSeed(7, 1), 0x1A3EAA3C25C3A340ull);
	CHECK_EQ(BotCore::DeriveBotSeed(8, 0), 0xFEAB185D957C5F22ull);

	std::set<uint64_t> seeds;
	for (uint32_t slot = 0; slot < 64; ++slot)
	{
		seeds.insert(BotCore::DeriveBotSeed(7, slot));
	}
	CHECK_EQ(seeds.size(), size_t(64));
}

TEST_CASE("Rng_Xoshiro_Pinned")
{
	{
		BotCore::Rng rng(12345);
		CHECK_EQ(rng.NextU64(), 0xBE6A36374160D49Bull);
		CHECK_EQ(rng.NextU64(), 0x214AAA0637A688C6ull);
		CHECK_EQ(rng.NextU64(), 0xF69D16DE9954D388ull);
	}
	{
		BotCore::Rng rng(12345);
		CHECK_EQ(rng.NextU32(), 3194631735u);
		CHECK_EQ(rng.NextU32(), 558541318u);
		CHECK_EQ(rng.NextU32(), 4137490142u);
	}
	{
		BotCore::Rng rng(BotCore::DeriveBotSeed(7, 0));
		CHECK_EQ(rng.NextU64(), 0x6D1C3405675A6FB1ull);
		CHECK_EQ(rng.NextU64(), 0xF2EA51CE434A4177ull);
	}
}

TEST_CASE("Rng_Determinism")
{
	BotCore::Rng a(123);
	BotCore::Rng b(123);
	bool identical = true;
	for (int i = 0; i < 1000; ++i)
	{
		if (a.NextU64() != b.NextU64())
		{
			identical = false;
		}
	}
	CHECK(identical);

	BotCore::Rng c(1);
	BotCore::Rng d(2);
	bool anyDifferent = false;
	for (int i = 0; i < 8; ++i)
	{
		if (c.NextU64() != d.NextU64())
		{
			anyDifferent = true;
		}
	}
	CHECK(anyDifferent);
}

TEST_CASE("Rng_NextBelow")
{
	{
		BotCore::Rng rng(12345);
		const uint32_t expected[8] = { 5, 8, 2, 2, 6, 6, 7, 3 };
		for (int i = 0; i < 8; ++i)
		{
			CHECK_EQ(rng.NextBelow(10), expected[i]);
		}
	}
	{
		BotCore::Rng rng(1);
		for (int i = 0; i < 100; ++i)
		{
			CHECK_EQ(rng.NextBelow(1), 0u);
		}
		CHECK_EQ(rng.NextBelow(0), 0u);
	}
	{
		BotCore::Rng rng(99);
		int counts[10] = { 0 };
		for (int i = 0; i < 100000; ++i)
		{
			uint32_t v = rng.NextBelow(10);
			if (v < 10)
			{
				++counts[v];
			}
		}
		for (int box = 0; box < 10; ++box)
		{
			CHECK(counts[box] >= 9500);
			CHECK(counts[box] <= 10500);
		}
	}
}

TEST_CASE("Rng_NextDouble_And_NextRange")
{
	{
		BotCore::Rng rng(12345);
		CHECK_EQ(rng.NextDouble(), 0.7438081631565894);
		CHECK_EQ(rng.NextDouble(), 0.13004553462783452);
	}
	{
		BotCore::Rng rng(5);
		double sum = 0.0;
		bool inRange = true;
		for (int i = 0; i < 100000; ++i)
		{
			double v = rng.NextDouble();
			if (v < 0.0 || v >= 1.0)
			{
				inRange = false;
			}
			sum += v;
		}
		CHECK(inRange);
		double mean = sum / 100000.0;
		CHECK(mean >= 0.49);
		CHECK(mean <= 0.51);
	}
	{
		BotCore::Rng rng(7);
		bool sawLo = false;
		bool sawHi = false;
		bool inRange = true;
		for (int i = 0; i < 10000; ++i)
		{
			int32_t v = rng.NextRange(-5, 5);
			if (v < -5 || v > 5)
			{
				inRange = false;
			}
			if (v == -5)
			{
				sawLo = true;
			}
			if (v == 5)
			{
				sawHi = true;
			}
		}
		CHECK(inRange);
		CHECK(sawLo);
		CHECK(sawHi);
		CHECK_EQ(rng.NextRange(3, 3), 3);
	}
	{
		BotCore::Rng rng(11);
		for (int i = 0; i < 1000; ++i)
		{
			volatile int32_t v = rng.NextRange(INT32_MIN, INT32_MAX);
			(void)v;
		}
		CHECK(true);
	}
}
