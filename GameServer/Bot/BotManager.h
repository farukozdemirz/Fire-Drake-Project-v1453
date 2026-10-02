#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include "ScenarioRunner.h"

class CUser;
class Thread;
class BotSession;

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

	// Any thread. Queues one command line ("spawn <names>", "despawn <name|all>", "list") for the
	// IOCP thread. Returns false when the bot system is disabled or the queue is full (64 lines).
	bool EnqueueCommand(const std::string & line);

	// Any thread. Copies the last status snapshot (refreshed by Tick() about once a second) into 'out':
	// first the header line, then one line per session. Returns false when the bot system is
	// disabled. 'out' is empty until the first refresh (about one tick after the AI start delay).
	bool GetStatusSnapshot(std::vector<std::string> & out);

private:
	friend class ScenarioRunner;

	BotManager() : m_enabled(false), m_poolSize(0), m_tickMs(100), m_timerThread(nullptr),
		m_shuttingDown(false), m_timerThreadId(0), m_skippedTicks(0),
		m_tickCount(0), m_tickThreadId(0), m_spawnSummaryDone(false),
		m_despawnAfterMs(0), m_spawnOk(0), m_spawnFailed(0), m_despawnSummaryDone(false),
		m_respawnCycles(0), m_despawnOk(0), m_namesLeft(0), m_scenario(*this) {}

	static uint32 THREADCALL TimerThreadProc(void * lpParam);
	static void TickCallback();
	void Tick(); // IOCP worker thread only
	void RecordTick(std::chrono::steady_clock::time_point tickStart); // IOCP thread only, called from Tick()
	void EmitPerfSample(std::chrono::steady_clock::time_point now);   // IOCP thread only

	// Runtime commands (ADR-0015). All of these run on the IOCP thread, called from Tick().
	void ProcessCommands();                       // polls ./BotCommands.txt, then drains m_commandQueue
	void PollCommandFile(std::chrono::steady_clock::time_point now);
	void ExecuteCommand(const std::string & line);
	void CommandSpawn(const std::string & args);
	void CommandDespawn(const std::string & args);
	void CommandList();
	void BuildStatusLines(std::vector<std::string> & out);                         // IOCP thread only
	void RefreshStatusSnapshot(std::chrono::steady_clock::time_point now);         // IOCP thread only
	void CommandMatch(const std::string & args);
	void CommandMove(const std::string & args);
	void CommandStop(const std::string & args);
	void CommandAttack(const std::string & args);
	BotSession * FindSession(const char * charName);
	static bool IsKnownBotName(const std::string & name);   // BOT_TABLE lookup, case-insensitive

	// Spawn list from [BOT] SPAWN_ON_START (parsed in Startup(); sessions are never freed).
	void ParseSpawnList(const std::string & list);
	void TickSessions();                          // IOCP thread only, called from Tick()
	void StartSession(BotSession * s);            // IOCP thread only
	void FailSession(BotSession * s, const char * reason); // IOCP thread only
	void BeginDespawn(BotSession * s, std::chrono::steady_clock::time_point now);  // IOCP thread only
	void PollDespawn(BotSession * s, std::chrono::steady_clock::time_point now);   // IOCP thread only

	std::vector<BotSession *> m_sessions;
	bool m_spawnSummaryDone;                      // IOCP thread only
	uint32 m_despawnAfterMs;     // 0 = never despawn ([BOT] DESPAWN_AFTER_SEC)
	uint32 m_spawnOk;            // IOCP thread only: sessions that reached PHASE_IN_GAME
	uint32 m_spawnFailed;        // IOCP thread only: sessions that went through FailSession()
	bool m_despawnSummaryDone;   // IOCP thread only

	uint32 m_respawnCycles;      // [BOT] RESPAWN_CYCLES: extra spawns per bot after the first (0 = none)
	uint32 m_despawnOk;          // IOCP thread only: despawns that returned their slot
	uint32 m_namesLeft;          // IOCP thread only: despawns whose account/character name was still registered

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

	std::mutex m_commandLock;                // guards m_commandQueue only
	std::vector<std::string> m_commandQueue; // filled by any thread, drained on the IOCP thread
	std::chrono::steady_clock::time_point m_lastCommandPoll; // IOCP thread only

	std::mutex m_statusLock;                  // guards m_statusLines only
	std::vector<std::string> m_statusLines;   // written on the IOCP thread, copied by any thread
	std::chrono::steady_clock::time_point m_lastStatusRefresh; // IOCP thread only

	std::vector<uint32> m_tickUs;                             // IOCP thread only: Tick() durations (us) of the current 5 s window
	std::chrono::steady_clock::time_point m_perfWindowStart;  // IOCP thread only
	bool m_perfWindowOpen = false;                            // IOCP thread only

	uint32 m_matchPerfSamples = 0;   // IOCP thread only: PERF_SAMPLEs emitted since the last "match start"
	uint32 m_matchP95MaxUs = 0;      // IOCP thread only: highest tick_p95_us among them
	uint32 m_matchTickMaxUs = 0;     // IOCP thread only: highest tick_max_us among them

	ScenarioRunner m_scenario;   // IOCP thread only
};
