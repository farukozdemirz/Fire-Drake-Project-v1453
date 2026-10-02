#include "stdafx.h"
#include "ScriptRunner.h"
#include "BotManager.h"
#include "Telemetry.h"

#include <cstdio>
#include <cstring>
#include <ctime>

static const size_t SCRIPT_LOG_CMD_MAX = 120;

// Appends one line to ./Logs/Bot_<day>_<month>_<year>.log (silently skipped if it cannot be opened).
static void WriteScriptLog(const char * line)
{
	time_t now = time(nullptr);
	struct tm * local = localtime(&now);
	char fileName[64];
	snprintf(fileName, sizeof(fileName), "./Logs/Bot_%d_%d_%d.log",
		local->tm_mday, local->tm_mon + 1, local->tm_year + 1900);

	FILE * fp = fopen(fileName, "a");
	if (fp == nullptr)
		return;

	fprintf(fp, "%s\n", line);
	fclose(fp);
}

// [A-Za-z0-9_-]{1,40}; the file name is built only from this, so no traversal is possible.
static bool IsSafeFileStem(const std::string & text)
{
	if (text.empty() || text.size() > 40)
		return false;

	for (size_t i = 0; i < text.size(); i++)
	{
		char c = text[i];
		bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '_' || c == '-';
		if (!ok)
			return false;
	}

	return true;
}

static void SplitWords(const std::string & text, std::vector<std::string> & out)
{
	size_t pos = 0;
	while (pos < text.size())
	{
		size_t start = text.find_first_not_of(" \t\r\n", pos);
		if (start == std::string::npos)
			break;

		size_t end = text.find_first_of(" \t\r\n", start);
		out.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
		pos = end == std::string::npos ? text.size() : end + 1;
	}
}

ScriptRunner::ScriptRunner(BotManager & mgr)
	: m_mgr(mgr), m_running(false), m_next(0), m_maxLateMs(0), m_runId(0)
{
}

bool ScriptRunner::LoadScript(const std::string & name, std::vector<BotCore::ScriptStep> & steps, std::string & error)
{
	if (!IsSafeFileStem(name))
	{
		error = "bad script name";
		return false;
	}

	std::string path = "./Scripts/" + name + ".txt";
	FILE * fp = fopen(path.c_str(), "rb");
	if (fp == nullptr)
	{
		error = "cannot open " + path;
		return false;
	}

	char buffer[BotCore::kScriptMaxBytes + 1];
	size_t got = fread(buffer, 1, BotCore::kScriptMaxBytes + 1, fp);
	fclose(fp);

	std::string text(buffer, got);
	BotCore::ScriptParseResult result = BotCore::ParseScript(text);
	if (result.error != BotCore::SCRIPT_OK)
	{
		error = name + ".txt";
		if (result.errorLine > 0)
			error += ":" + std::to_string(result.errorLine);
		error += ": ";
		error += BotCore::ScriptErrorText(result.error);
		return false;
	}

	steps = result.steps;
	return true;
}

void ScriptRunner::Command(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() == 2 && _stricmp(words[0].c_str(), "run") == 0)
	{
		CommandRun(words[1]);
		return;
	}

	if (words.size() == 1 && _stricmp(words[0].c_str(), "stop") == 0)
	{
		CommandStop();
		return;
	}

	if (words.size() == 1 && _stricmp(words[0].c_str(), "status") == 0)
	{
		CommandStatus();
		return;
	}

	WriteScriptLog("ScriptRunner: cmd script: usage: script run <name> | script stop | script status");
}

void ScriptRunner::CommandRun(const std::string & name)
{
	char message[400];

	if (m_running)
	{
		snprintf(message, sizeof(message),
			"ScriptRunner: run %s: refused (already running (%s))", name.c_str(), m_name.c_str());
		WriteScriptLog(message);
		return;
	}

	std::vector<BotCore::ScriptStep> steps;
	std::string error;
	if (!LoadScript(name, steps, error))
	{
		snprintf(message, sizeof(message), "ScriptRunner: run %s: refused (%s)", name.c_str(), error.c_str());
		WriteScriptLog(message);
		return;
	}

	m_name = name;
	m_steps = steps;
	m_next = 0;
	m_maxLateMs = 0;
	m_start = std::chrono::steady_clock::now();
	m_runId++;
	m_running = true;

	snprintf(message, sizeof(message),
		"ScriptRunner: run %s: loaded (%u step(s), last offset %u ms)",
		m_name.c_str(), (unsigned)m_steps.size(), (unsigned)m_steps.back().offsetMs);
	WriteScriptLog(message);

	if (!Telemetry::Instance().IsEnabled(TEL_DECISIONS))
	{
		snprintf(message, sizeof(message),
			"ScriptRunner: run %s: warning (telemetry below 'decisions': ACTION_* and FAIRNESS_REJECT events are not recorded)",
			m_name.c_str());
		WriteScriptLog(message);
	}

	std::string fields = "\"script\":\"" + Telemetry::EscapeJson(m_name) + "\",\"steps\":"
		+ std::to_string(m_steps.size()) + ",\"duration_ms\":"
		+ std::to_string(m_steps.back().offsetMs);
	Telemetry::Instance().Emit(TEL_DECISIONS, "SCRIPT_START", -1, nullptr, fields, false);
}

void ScriptRunner::Tick(std::chrono::steady_clock::time_point now)
{
	if (!m_running)
		return;

	long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start).count();

	while (m_next < m_steps.size() && (long long)m_steps[m_next].offsetMs <= elapsed)
	{
		uint32 offsetMs = m_steps[m_next].offsetMs;
		uint32 line = m_steps[m_next].line;
		std::string command = m_steps[m_next].command;

		long long late = elapsed - (long long)offsetMs;
		if ((uint32)late > m_maxLateMs)
			m_maxLateMs = (uint32)late;

		std::string shown = command;
		if (shown.size() > SCRIPT_LOG_CMD_MAX)
			shown.resize(SCRIPT_LOG_CMD_MAX);

		char message[400];
		snprintf(message, sizeof(message),
			"ScriptRunner: step %u/%u (line %u, +%u ms, late %u ms): %s",
			(unsigned)(m_next + 1), (unsigned)m_steps.size(), (unsigned)line,
			(unsigned)offsetMs, (unsigned)late, shown.c_str());
		WriteScriptLog(message);

		size_t vsep = command.find_first_of(" \t");
		std::string verb = vsep == std::string::npos ? command : command.substr(0, vsep);

		std::string fields = "\"script\":\"" + Telemetry::EscapeJson(m_name) + "\",\"step\":"
			+ std::to_string(m_next + 1) + ",\"line\":" + std::to_string(line)
			+ ",\"offset_ms\":" + std::to_string(offsetMs) + ",\"late_ms\":" + std::to_string(late)
			+ ",\"verb\":\"" + Telemetry::EscapeJson(verb) + "\"";
		Telemetry::Instance().Emit(TEL_DECISIONS, "SCRIPT_STEP", -1, nullptr, fields, false);

		m_mgr.ExecuteCommand(command);
		m_next++;
	}

	if (m_next >= m_steps.size())
		Finish("completed");
}

void ScriptRunner::CommandStop()
{
	if (!m_running)
	{
		WriteScriptLog("ScriptRunner: stop: no script running");
		return;
	}

	char message[400];
	snprintf(message, sizeof(message),
		"ScriptRunner: stop: %s: stopped after %u/%u step(s); running bot actions are not cancelled (use 'stop all' / 'attack all off')",
		m_name.c_str(), (unsigned)m_next, (unsigned)m_steps.size());
	WriteScriptLog(message);

	Finish("stopped");
}

void ScriptRunner::CommandStatus()
{
	if (!m_running)
	{
		WriteScriptLog("ScriptRunner: status: idle");
		return;
	}

	long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - m_start).count();

	long long remaining = 0;
	if (m_next < m_steps.size())
	{
		remaining = (long long)m_steps[m_next].offsetMs - elapsed;
		if (remaining < 0)
			remaining = 0;
	}

	char message[400];
	snprintf(message, sizeof(message),
		"ScriptRunner: status: %s step %u/%u done, %lld ms elapsed, next in %lld ms",
		m_name.c_str(), (unsigned)m_next, (unsigned)m_steps.size(), elapsed, remaining);
	WriteScriptLog(message);
}

void ScriptRunner::Finish(const char * result)
{
	long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - m_start).count();

	char message[400];
	snprintf(message, sizeof(message),
		"ScriptRunner: finished %s: %s, %u/%u step(s) in %lld ms (max late %u ms)",
		m_name.c_str(), result, (unsigned)m_next, (unsigned)m_steps.size(),
		elapsed, (unsigned)m_maxLateMs);
	WriteScriptLog(message);

	std::string fields = "\"script\":\"" + Telemetry::EscapeJson(m_name) + "\",\"result\":\""
		+ Telemetry::EscapeJson(result) + "\",\"steps_run\":" + std::to_string(m_next)
		+ ",\"steps_total\":" + std::to_string(m_steps.size())
		+ ",\"elapsed_ms\":" + std::to_string(elapsed)
		+ ",\"max_late_ms\":" + std::to_string(m_maxLateMs);
	Telemetry::Instance().Emit(TEL_DECISIONS, "SCRIPT_END", -1, nullptr, fields, false);

	m_running = false;
	m_steps.clear();
	m_name.clear();
	m_next = 0;
}
