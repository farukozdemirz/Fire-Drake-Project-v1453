#pragma once

#include <chrono>
#include <string>
#include <vector>
#include "../../BotCore/ScriptPlan.h"

class BotManager;

// Runs a test script (./Scripts/<name>.txt, parsed by BotCore::ParseScript): every step is a
// normal /bot command executed through BotManager::ExecuteCommand() at its offset (ADR-0017
// Ek F4-20). IOCP thread only: Command() is called from ExecuteCommand(), Tick() from
// BotManager::Tick().
class ScriptRunner
{
public:
	explicit ScriptRunner(BotManager & mgr);   // trivial: no allocation, no I/O

	void Command(const std::string & args);    // "run <name>" | "stop" | "status"
	void Tick(std::chrono::steady_clock::time_point now);   // returns at once while idle

private:
	static bool LoadScript(const std::string & name, std::vector<BotCore::ScriptStep> & steps, std::string & error);
	void CommandRun(const std::string & name);
	void CommandStop();
	void CommandStatus();
	void Finish(const char * result);          // logs the summary, emits SCRIPT_END, back to idle

	BotManager & m_mgr;
	bool m_running;
	std::string m_name;
	std::vector<BotCore::ScriptStep> m_steps;
	size_t m_next;                             // index of the next step to run
	std::chrono::steady_clock::time_point m_start;
	uint32 m_maxLateMs;                        // largest (actual - planned) offset so far
};
