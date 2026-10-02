#pragma once

#include <atomic>
#include <chrono>

class CUser;
class Thread;

class BotManager
{
public:
	static const uint16 MAX_POOL = 100;

	static BotManager & Instance();

	// Reads [BOT] ENABLED / MAX_BOTS from GameServer.ini. Disabled (default): returns true
	// and does nothing else. Enabled: reserves the slot pool, runs the pool self-test and
	// writes one status line. Returns false if the pool could not be set up.
	// Called once from CGameServerDlg::Startup() before the worker threads start.
	bool Startup();

	bool isEnabled() const { return m_enabled; }
	uint16 GetPoolSize() const { return m_poolSize; }

	// IOCP thread only (used from F2-02 on). The slot is moved to the active session map.
	CUser * AcquireSlot();
	// Clears m_botSink and returns the slot to the pool.
	void ReleaseSlot(CUser * pUser);

	// Starts the BotTickTimer thread. No-op unless enabled. Call once after RunServer().
	void StartTicking();
	// Joins the timer thread. Idempotent; safe when ticking never started.
	void Shutdown();

private:
	BotManager() : m_enabled(false), m_poolSize(0), m_tickMs(100), m_timerThread(nullptr),
		m_shuttingDown(false), m_timerThreadId(0), m_skippedTicks(0),
		m_tickCount(0), m_tickThreadId(0) {}

	static uint32 THREADCALL TimerThreadProc(void * lpParam);
	static void TickCallback();
	void Tick(); // IOCP worker thread only

	bool m_enabled;
	uint16 m_poolSize;

	uint32 m_tickMs;
	Thread * m_timerThread;
	std::atomic<bool> m_shuttingDown;
	std::atomic<uint32> m_timerThreadId;
	std::atomic<uint32> m_skippedTicks;
	uint32 m_tickCount;      // IOCP thread only
	uint32 m_tickThreadId;   // IOCP thread only
	std::chrono::steady_clock::time_point m_firstTickTime; // IOCP thread only
};
