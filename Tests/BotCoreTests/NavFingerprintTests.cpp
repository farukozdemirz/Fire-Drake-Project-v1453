#include "MiniTest.h"

#include <BotCore/NavFingerprint.h>
#include <BotCore/NavGrid.h>
#include <BotCore/Rng.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace
{
	uint32_t CrcOfBytes(const void * data, size_t size)
	{
		BotCore::NavCrc32 crc;
		crc.Update(data, size);
		return crc.Value();
	}

	// Builds a .navgrid byte buffer in the exact file layout (F5-01 s5.1).
	std::vector<uint8_t> BuildNavFile(int n, float unit,
		const std::vector<int16_t> & events, const std::vector<float> & heights)
	{
		const size_t cells = (size_t)n * (size_t)n;
		std::vector<uint8_t> buffer(16 + 6 * cells);
		const char magic[8] = { 'F', 'D', 'P', 'N', 'A', 'V', '0', '1' };
		std::memcpy(buffer.data(), magic, 8);
		const int32_t n32 = (int32_t)n;
		std::memcpy(buffer.data() + 8, &n32, sizeof(n32));
		std::memcpy(buffer.data() + 12, &unit, sizeof(unit));
		std::memcpy(buffer.data() + 16, events.data(), cells * sizeof(int16_t));
		std::memcpy(buffer.data() + 16 + cells * sizeof(int16_t), heights.data(), cells * sizeof(float));
		return buffer;
	}

	std::vector<int16_t> RandomEvents(BotCore::Rng & rng, size_t cells)
	{
		std::vector<int16_t> events(cells);
		for (size_t i = 0; i < cells; ++i)
			events[i] = (int16_t)(rng.NextU32() % 4u);
		return events;
	}

	std::vector<float> RandomHeights(BotCore::Rng & rng, size_t cells)
	{
		std::vector<float> heights(cells);
		for (size_t i = 0; i < cells; ++i)
			heights[i] = (float)rng.NextDouble() * 10.0f - 5.0f;
		return heights;
	}
}

TEST_CASE("NavFingerprint_Crc32_KnownVectors")
{
	const char * empty = "";
	const uint32_t emptyCrc = CrcOfBytes(empty, 0);
	CHECK_EQ(emptyCrc, 0x00000000u);

	const char * digits = "123456789";
	const uint32_t digitsCrc = CrcOfBytes(digits, std::strlen(digits));
	CHECK_EQ(digitsCrc, 0xCBF43926u);

	const char * a = "a";
	const uint32_t aCrc = CrcOfBytes(a, 1);
	CHECK_EQ(aCrc, 0xE8B7BE43u);

	// A split update must equal a single one and Value() is repeatable.
	BotCore::NavCrc32 split;
	split.Update(digits, 4);
	split.Update(digits + 4, std::strlen(digits) - 4);
	CHECK_EQ(split.Value(), digitsCrc);
	CHECK_EQ(split.Value(), digitsCrc);
}

TEST_CASE("NavFingerprint_InitEqualsLoad")
{
	const int n = 9;
	const float unit = 4.0f;
	const size_t cells = (size_t)n * (size_t)n;

	BotCore::Rng rng(12345);
	const std::vector<int16_t> events = RandomEvents(rng, cells);
	const std::vector<float> heights = RandomHeights(rng, cells);
	const std::vector<uint8_t> buffer = BuildNavFile(n, unit, events, heights);

	BotCore::NavGrid initGrid;
	REQUIRE(initGrid.Init(n, unit, events, heights));

	BotCore::NavGrid loadGrid;
	REQUIRE(loadGrid.Load(buffer.data(), buffer.size()));

	const uint32_t initCrc = BotCore::NavGridFingerprint(initGrid);
	const uint32_t loadCrc = BotCore::NavGridFingerprint(loadGrid);
	const uint32_t fileCrc = CrcOfBytes(buffer.data(), buffer.size());

	CHECK_EQ(initCrc, loadCrc);
	CHECK_EQ(initCrc, fileCrc);

	initGrid.Build();
	CHECK_EQ(BotCore::NavGridFingerprint(initGrid), initCrc);

	loadGrid.Build();
	CHECK_EQ(BotCore::NavGridFingerprint(loadGrid), loadCrc);
}

TEST_CASE("NavFingerprint_Sensitivity")
{
	const int n = 9;
	const float unit = 4.0f;
	const size_t cells = (size_t)n * (size_t)n;

	BotCore::Rng rng(777);
	const std::vector<int16_t> events = RandomEvents(rng, cells);
	const std::vector<float> heights = RandomHeights(rng, cells);

	BotCore::NavGrid base;
	REQUIRE(base.Init(n, unit, events, heights));
	const uint32_t baseCrc = BotCore::NavGridFingerprint(base);

	std::vector<int16_t> changedEvent = events;
	changedEvent[0] = (int16_t)(changedEvent[0] + 1);
	BotCore::NavGrid eventGrid;
	REQUIRE(eventGrid.Init(n, unit, changedEvent, heights));
	CHECK(BotCore::NavGridFingerprint(eventGrid) != baseCrc);

	std::vector<float> changedHeight = heights;
	uint32_t bits = 0;
	std::memcpy(&bits, &changedHeight[0], sizeof(bits));
	bits ^= 1u;
	std::memcpy(&changedHeight[0], &bits, sizeof(bits));
	BotCore::NavGrid heightGrid;
	REQUIRE(heightGrid.Init(n, unit, events, changedHeight));
	CHECK(BotCore::NavGridFingerprint(heightGrid) != baseCrc);

	const size_t biggerCells = (size_t)(n + 1) * (size_t)(n + 1);
	BotCore::Rng sizeRng(4242);
	BotCore::NavGrid sizeGrid;
	REQUIRE(sizeGrid.Init(n + 1, unit, RandomEvents(sizeRng, biggerCells), RandomHeights(sizeRng, biggerCells)));
	CHECK(BotCore::NavGridFingerprint(sizeGrid) != baseCrc);

	BotCore::NavGrid unitGrid;
	REQUIRE(unitGrid.Init(n, unit + 1.0f, events, heights));
	CHECK(BotCore::NavGridFingerprint(unitGrid) != baseCrc);

	BotCore::NavGrid empty;
	CHECK_EQ(BotCore::NavGridFingerprint(empty), 0u);
}

TEST_CASE("NavFingerprint_RealMap_MatchesFile")
{
	const char * path = "build/nav/zone71.navgrid";

	std::ifstream in(path, std::ios::binary | std::ios::ate);
	if (!in)
	{
		std::printf("NAVFP real map: SKIPPED (build/nav/zone71.navgrid missing; run tools/nav-export.py)\n");
		return;
	}

	const std::streamoff len = in.tellg();
	REQUIRE(len > 0);
	in.seekg(0, std::ios::beg);

	std::vector<uint8_t> data((size_t)len);
	REQUIRE(in.read(reinterpret_cast<char *>(data.data()), (std::streamsize)len));

	const uint32_t fileCrc = CrcOfBytes(data.data(), data.size());

	BotCore::NavGrid grid;
	REQUIRE(grid.Load(data.data(), data.size()));
	const uint32_t gridCrc = BotCore::NavGridFingerprint(grid);

	grid.Build();
	REQUIRE(grid.MainComponentCells() == 88508);

	std::printf("NAVFP real map: n=%d cells=%d main=%d crc32=%08x match=%d\n",
		grid.Size(), grid.Size() * grid.Size(), grid.MainComponentCells(),
		gridCrc, (int)(gridCrc == fileCrc));

	CHECK_EQ(gridCrc, fileCrc);
}
