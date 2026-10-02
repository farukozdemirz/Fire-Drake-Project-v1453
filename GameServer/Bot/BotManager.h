#pragma once

class CUser;

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

private:
	BotManager() : m_enabled(false), m_poolSize(0) {}

	bool m_enabled;
	uint16 m_poolSize;
};
