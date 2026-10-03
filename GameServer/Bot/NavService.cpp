#include "stdafx.h"
#include "NavService.h"
#include "BotManager.h"
#include "../Map.h"
#include <set>
#include "../../shared/SMDFile.h"
#include "../../BotCore/NavFingerprint.h"
#include "../../shared/Ini.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

NavService & NavService::Instance()
{
	static NavService instance;
	return instance;
}

// Appends one line to ./Logs/Bot_<day>_<month>_<year>.log (silently skipped if it cannot be opened).
static void WriteNavLog(const char * line)
{
	time_t now = time(nullptr);
	struct tm * local = localtime(&now);
	const std::string fileName = "./Logs/Bot_" + std::to_string(local->tm_mday)
		+ "_" + std::to_string(local->tm_mon + 1)
		+ "_" + std::to_string(local->tm_year + 1900) + ".log";

	FILE * fp = fopen(fileName.c_str(), "a");
	if (fp == nullptr)
		return;

	fwrite(line, 1, strlen(line), fp);
	fputc('\n', fp);
	fclose(fp);
}

// One A* pool for every bot (F5-70, D1). NavPathfinder is not thread-safe, so all callers must be on the
// IOCP thread; the first caller's thread id is remembered and a later call from another thread logs one
// "VIOLATION" line (the call still proceeds: a single stale write is safer than a crash).
BotCore::NavPathfinder & NavService::SharedPathfinder()
{
	const uint32_t tid = (uint32_t)GetCurrentThreadId();
	uint32_t first = 0;
	if (!m_pfThread.compare_exchange_strong(first, tid) && first != tid && !m_pfViolation.exchange(true))
	{
		char message[160];
		snprintf(message, sizeof(message),
			"NavService: pathfinder used from thread %u (first %u) VIOLATION", (unsigned)tid, (unsigned)first);
		printf("%s\n", message);
		WriteNavLog(message);
	}

	return m_pathfinder;
}

bool NavService::Startup()
{
	if (m_ready.load(std::memory_order_acquire))
		return true;

	m_enabled = false;
	m_ready.store(false, std::memory_order_release);

	if (!BotManager::Instance().isEnabled())
		return true;

	CIni ini(CONF_GAME_SERVER);
	if (!ini.GetBool("BOT", "NAV", false))
		return true;

	m_enabled = true;

	auto fail = [](const char * reason)
	{
		char message[128];
		snprintf(message, sizeof(message), "NavService: nav FAILED (%s)", reason);
		printf("%s\n", message);
		WriteNavLog(message);
	};

	C3DMap * map = g_pMain->GetZoneByID(ZONE_RONARK_LAND);
	if (map == nullptr || map->m_smdFile == nullptr)
	{
		fail("no_zone");
		return true;
	}

	SMDFile * smd = map->m_smdFile;
	const int n = smd->GetMapSize() + 1;
	const float unit = smd->GetUnitDistance();
	short * ev = smd->GetEventIDs();
	float * ht = smd->GetHeights();
	if (ev == nullptr || ht == nullptr || n < 2)
	{
		fail("no_data");
		return true;
	}

	const size_t cells = (size_t)n * (size_t)n;

	const std::chrono::steady_clock::time_point copyStart = std::chrono::steady_clock::now();
	std::vector<int16_t> events(ev, ev + cells);
	std::vector<float> heights(ht, ht + cells);
	const double copyMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - copyStart).count();

	if (!m_grid.Init(n, unit, std::move(events), std::move(heights)))
	{
		fail("init");
		return true;
	}

	const std::chrono::steady_clock::time_point buildStart = std::chrono::steady_clock::now();
	m_grid.Build();
	const double buildMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - buildStart).count();

	if (m_grid.MainComponentCells() <= 0)
	{
		fail("no_walk");
		return true;
	}

	m_info.zone = ZONE_RONARK_LAND;
	m_info.n = n;
	m_info.unit = unit;
	m_info.mainCells = m_grid.MainComponentCells();
	m_info.crc32 = BotCore::NavGridFingerprint(m_grid);
	m_info.copyMs = copyMs;
	m_info.buildMs = buildMs;
	m_ready.store(true, std::memory_order_release);

	char message[256];
	snprintf(message, sizeof(message),
		"NavService: nav ready: zone %d cells=%d main_cells=%d unit=%.1f crc32=%08x copy_ms=%.1f build_ms=%.1f",
		m_info.zone, n * n, m_info.mainCells, (double)unit, m_info.crc32, copyMs, buildMs);
	printf("%s\n", message);
	WriteNavLog(message);

	return true;
}

void NavService::Shutdown()
{
	m_ready.store(false, std::memory_order_release);
}
