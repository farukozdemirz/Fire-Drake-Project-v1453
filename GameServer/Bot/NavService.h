#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include "../../BotCore/NavGrid.h"

// Owns the navigation grid of zone 71 (F5-59; docs/12 s2). Built once at start-up from the SMD data the
// server already loaded, never rebuilt, never written afterwards: Grid() is safe to read from any thread
// while Ready() is true. NavService holds navigation STATE only; it never moves, plans or sends anything.
class NavService
{
public:
	struct Info
	{
		int      zone = 0;
		int      n = 0;              // vertices per side (513)
		float    unit = 0.0f;        // metres per cell (4.0)
		int      mainCells = 0;      // NavGrid::MainComponentCells()
		uint32_t crc32 = 0;          // BotCore::NavGridFingerprint
		double   copyMs = 0.0;
		double   buildMs = 0.0;
	};

	static NavService & Instance();

	// Called once from CGameServerDlg::Startup() after MapFileLoad() and before RunServer(), on the main
	// thread. [BOT] ENABLED=0 or NAV=0 (default): returns true, does nothing, prints nothing. NAV=1: builds
	// the grid and writes ONE line (printf + ./Logs/Bot_*.log). A failure is logged ("nav FAILED (<reason>)"),
	// leaves Ready() false and still returns true: the server must start; bots just have no navigation.
	bool Startup();
	// Called from ~CGameServerDlg() after BotManager::Shutdown(): clears the ready flag (the grid memory is
	// released by the singleton destructor at process exit; no tick can run by then).
	void Shutdown();

	bool Enabled() const { return m_enabled; }      // NAV=1 was requested (and the bot system is on)
	bool Ready() const { return m_ready.load(std::memory_order_acquire); }
	const BotCore::NavGrid * Grid() const { return Ready() ? &m_grid : nullptr; }
	const Info & GetInfo() const { return m_info; }

private:
	NavService() : m_enabled(false), m_ready(false) {}
	bool m_enabled;
	std::atomic<bool> m_ready;
	BotCore::NavGrid m_grid;
	Info m_info;
};
