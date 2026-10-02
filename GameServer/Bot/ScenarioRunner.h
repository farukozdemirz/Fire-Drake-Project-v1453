#pragma once

#include <chrono>
#include <string>
#include <vector>

class BotManager;
class BotSession;

// Runs a scenario file: spawn the listed bots, open a telemetry match per (seed, repeat),
// close it after duration_sec, despawn the bots, repeat. IOCP thread only (ADR-0005):
// Command() is called from BotManager::ExecuteCommand(), Tick() from BotManager::Tick().
class ScenarioRunner
{
public:
	explicit ScenarioRunner(BotManager & mgr);   // trivial: no allocation, no I/O

	void Command(const std::string & args);      // "run <name>" | "stop" | "status"
	void Tick(std::chrono::steady_clock::time_point now);   // returns at once while idle

private:
	enum State { STATE_IDLE, STATE_PREPARE, STATE_RUNNING, STATE_CLEANUP };

	struct Scenario
	{
		std::string name;                 // file stem
		std::string id;                   // match id prefix
		std::vector<std::string> bots;
		std::vector<uint32> seeds;
		uint32 repeat;
		uint32 durationSec;
		std::string script;                // optional ./Scripts/<name>.txt, empty = none
	};

	static bool LoadScenario(const std::string & name, Scenario & out, std::string & error);
	void CommandRun(const std::string & name);
	void CommandStop();
	void CommandStatus();
	void StartRun(std::chrono::steady_clock::time_point now);   // enters STATE_PREPARE
	void Abort(const std::string & reason, std::chrono::steady_clock::time_point now);
	void BeginCleanup(std::chrono::steady_clock::time_point now);
	void Finish(const std::string & abortNote);   // logs the summary, back to STATE_IDLE
	void StopScript();             // stops the script this scenario started, if still running

	BotManager & m_mgr;
	State m_state;
	Scenario m_scenario;
	std::vector<uint32> m_runSeeds;               // flattened run list (seed per run)
	size_t m_runIndex;                            // 0-based index of the current run
	uint32 m_completedRuns;
	std::string m_abortReason;                    // empty = no abort pending
	std::vector<BotSession *> m_sessions;         // the scenario's bots, resolved in StartRun()
	uint32 m_scriptRunId;                 // ScriptRunner::RunId() of the script started by this scenario, 0 = none
	std::chrono::steady_clock::time_point m_stateSince;   // when the current state began
	std::chrono::steady_clock::time_point m_matchSince;   // when the match opened
};
