#include "stdafx.h"
#include "ScenarioRunner.h"
#include "BotManager.h"
#include "BotSession.h"
#include "Telemetry.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

const size_t SCENARIO_FILE_MAX_BYTES = 4096;
const size_t SCENARIO_FILE_MAX_LINES = 64;
const size_t SCENARIO_MAX_BOTS = 16;
const size_t SCENARIO_MAX_SEEDS = 32;
const size_t SCENARIO_MAX_RUNS = 200;
static const uint32 SCENARIO_PREPARE_TIMEOUT_MS = 60000;
static const uint32 SCENARIO_CLEANUP_TIMEOUT_MS = 60000;

// Appends one line to ./Logs/Bot_<day>_<month>_<year>.log (silently skipped if it cannot be opened).
static void WriteScenarioLog(const char * line)
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

static std::string Trim(const std::string & text)
{
	size_t first = text.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return "";

	size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

static const char * PhaseText(BotSession::Phase phase)
{
	switch (phase)
	{
	case BotSession::PHASE_QUEUED: return "queued";
	case BotSession::PHASE_WAIT_SELECT: return "wait_select";
	case BotSession::PHASE_WAIT_LOADED: return "wait_loaded";
	case BotSession::PHASE_IN_GAME: return "in_game";
	case BotSession::PHASE_FAILED: return "failed";
	case BotSession::PHASE_DESPAWN_WAIT: return "despawn_wait";
	case BotSession::PHASE_DESPAWNED: return "despawned";
	case BotSession::PHASE_DESPAWN_STUCK: return "despawn_stuck";
	default: return "?";
	}
}

// [A-Za-z0-9_.-]{1,32}, must not start with '.' (same rule as Telemetry match ids).
static bool IsSafeId(const std::string & text)
{
	if (text.empty() || text.size() > 32 || text[0] == '.')
		return false;

	for (size_t i = 0; i < text.size(); i++)
	{
		char c = text[i];
		bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
		if (!ok)
			return false;
	}

	return true;
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

// Digits only, 1..10 characters, value <= max.
static bool ParseUint(const std::string & text, unsigned long long max, unsigned long long & out)
{
	if (text.empty() || text.size() > 10)
		return false;

	unsigned long long value = 0;
	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] < '0' || text[i] > '9')
			return false;
		value = value * 10 + (unsigned long long)(text[i] - '0');
	}

	if (value > max)
		return false;

	out = value;
	return true;
}

static std::string Unquote(const std::string & text)
{
	if (text.size() >= 2
		&& ((text[0] == '"' && text[text.size() - 1] == '"')
			|| (text[0] == '\'' && text[text.size() - 1] == '\'')))
		return text.substr(1, text.size() - 2);

	return text;
}

// Flow list [a, b, c]; rejects a missing bracket, an empty item and a trailing comma.
static bool ParseList(const std::string & value, std::vector<std::string> & items)
{
	if (value.size() < 2 || value[0] != '[' || value[value.size() - 1] != ']')
		return false;

	std::string inner = Trim(value.substr(1, value.size() - 2));
	if (inner.empty())
		return false;

	size_t pos = 0;
	while (pos <= inner.size())
	{
		size_t comma = inner.find(',', pos);
		std::string item = Trim(inner.substr(pos,
			comma == std::string::npos ? std::string::npos : comma - pos));
		item = Unquote(item);
		if (item.empty())
			return false;

		items.push_back(item);
		if (comma == std::string::npos)
			break;
		pos = comma + 1;
	}

	return true;
}

ScenarioRunner::ScenarioRunner(BotManager & mgr)
	: m_mgr(mgr), m_state(STATE_IDLE), m_runIndex(0), m_completedRuns(0)
{
}

bool ScenarioRunner::LoadScenario(const std::string & name, Scenario & out, std::string & error)
{
	if (!IsSafeFileStem(name))
	{
		error = "bad scenario name";
		return false;
	}

	std::string path = "./Scenarios/" + name + ".yaml";
	FILE * fp = fopen(path.c_str(), "r");
	if (fp == nullptr)
	{
		error = "cannot open " + path;
		return false;
	}

	bool seenScenarioId = false, seenZone = false, seenBots = false;
	bool seenSeeds = false, seenRepeat = false, seenDuration = false;

	std::string id = name;
	std::vector<std::string> bots;
	std::vector<uint32> seeds;
	uint32 repeat = 1;
	uint32 durationSec = 30;

	char buffer[256];
	uint32 lineNo = 0;
	size_t totalBytes = 0;
	bool failed = false;

	while (!failed && fgets(buffer, sizeof(buffer), fp) != nullptr)
	{
		size_t rawLen = strlen(buffer);
		totalBytes += rawLen;
		lineNo++;

		// A valid line is at most 255 characters, so a read without a newline means the line
		// is too long (unless it is the last line of the file).
		bool hasNewline = rawLen > 0 && buffer[rawLen - 1] == '\n';
		if (!hasNewline)
		{
			int next = fgetc(fp);
			bool lineEnd = (next == EOF || next == '\n');
			if (!lineEnd && next == '\r')
			{
				int following = fgetc(fp);
				lineEnd = (following == EOF || following == '\n');
			}

			if (!lineEnd)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": line longer than 255 characters";
			}
		}

		if (!failed && (size_t)lineNo > SCENARIO_FILE_MAX_LINES)
		{
			failed = true;
			error = name + ".yaml:" + std::to_string(lineNo) + ": more than 64 lines";
		}

		if (!failed && totalBytes > SCENARIO_FILE_MAX_BYTES)
		{
			failed = true;
			error = name + ".yaml:" + std::to_string(lineNo) + ": file longer than 4096 bytes";
		}

		if (failed)
			break;

		if (buffer[0] == ' ' || buffer[0] == '\t')
		{
			failed = true;
			error = name + ".yaml:" + std::to_string(lineNo) + ": unsupported YAML construct (indentation)";
			break;
		}

		std::string line = Trim(buffer);
		if (line.empty() || line[0] == '#')
			continue;

		if (line[0] == '-')
		{
			failed = true;
			error = name + ".yaml:" + std::to_string(lineNo) + ": unsupported YAML construct (list item)";
			break;
		}

		size_t hash = line.find('#');
		if (hash != std::string::npos && hash > 0 && (line[hash - 1] == ' ' || line[hash - 1] == '\t'))
			line = Trim(line.substr(0, hash));

		size_t colon = line.find(':');
		if (colon == std::string::npos)
		{
			failed = true;
			error = name + ".yaml:" + std::to_string(lineNo) + ": expected key: value";
			break;
		}

		std::string key = Trim(line.substr(0, colon));
		std::string value = Trim(line.substr(colon + 1));

		bool known = key == "scenario_id" || key == "zone" || key == "bots"
			|| key == "seeds" || key == "repeat" || key == "duration_sec";
		if (!known)
		{
			failed = true;
			error = name + ".yaml:" + std::to_string(lineNo) + ": unknown key '" + key + "'";
			break;
		}

		if (key == "scenario_id")
		{
			if (seenScenarioId)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": duplicate key 'scenario_id'";
				break;
			}
			seenScenarioId = true;

			std::string parsed = Unquote(value);
			if (!IsSafeId(parsed))
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": scenario_id: bad id '" + parsed + "'";
				break;
			}
			id = parsed;
		}
		else if (key == "zone")
		{
			if (seenZone)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": duplicate key 'zone'";
				break;
			}
			seenZone = true;

			unsigned long long parsed;
			if (!ParseUint(value, 4294967295ULL, parsed))
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": zone: bad integer '" + value + "'";
				break;
			}
			if (parsed != 71)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": zone: only 71 is supported";
				break;
			}
		}
		else if (key == "bots")
		{
			if (seenBots)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": duplicate key 'bots'";
				break;
			}
			seenBots = true;

			std::vector<std::string> items;
			if (!ParseList(value, items))
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": bots: bad list";
				break;
			}
			if (items.empty() || items.size() > SCENARIO_MAX_BOTS)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": bots: expected 1..16 names";
				break;
			}

			for (size_t i = 0; i < items.size(); i++)
			{
				if (!BotManager::IsKnownBotName(items[i]))
				{
					failed = true;
					error = name + ".yaml:" + std::to_string(lineNo) + ": bots: unknown bot '" + items[i] + "'";
					break;
				}

				for (size_t j = 0; j < i; j++)
				{
					if (_stricmp(items[j].c_str(), items[i].c_str()) == 0)
					{
						failed = true;
						error = name + ".yaml:" + std::to_string(lineNo) + ": bots: duplicate '" + items[i] + "'";
						break;
					}
				}
				if (failed)
					break;
			}
			if (failed)
				break;

			bots = items;
		}
		else if (key == "seeds")
		{
			if (seenSeeds)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": duplicate key 'seeds'";
				break;
			}
			seenSeeds = true;

			std::vector<std::string> items;
			if (!ParseList(value, items))
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": seeds: bad list";
				break;
			}
			if (items.empty() || items.size() > SCENARIO_MAX_SEEDS)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": seeds: expected 1..32 values";
				break;
			}

			for (size_t i = 0; i < items.size(); i++)
			{
				unsigned long long parsed;
				if (!ParseUint(items[i], 4294967295ULL, parsed))
				{
					failed = true;
					error = name + ".yaml:" + std::to_string(lineNo) + ": seeds: bad seed '" + items[i] + "'";
					break;
				}
				seeds.push_back((uint32)parsed);
			}
			if (failed)
				break;
		}
		else if (key == "repeat")
		{
			if (seenRepeat)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": duplicate key 'repeat'";
				break;
			}
			seenRepeat = true;

			unsigned long long parsed;
			if (!ParseUint(value, 100, parsed) || parsed == 0)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": repeat: expected 1..100";
				break;
			}
			repeat = (uint32)parsed;
		}
		else // duration_sec
		{
			if (seenDuration)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": duplicate key 'duration_sec'";
				break;
			}
			seenDuration = true;

			unsigned long long parsed;
			if (!ParseUint(value, 3600, parsed) || parsed == 0)
			{
				failed = true;
				error = name + ".yaml:" + std::to_string(lineNo) + ": duration_sec: expected 1..3600";
				break;
			}
			durationSec = (uint32)parsed;
		}
	}

	fclose(fp);

	if (failed)
		return false;

	if (!seenBots)
	{
		error = "missing key 'bots'";
		return false;
	}

	if (!seenSeeds)
		seeds.push_back(0);

	if ((size_t)seeds.size() * (size_t)repeat > SCENARIO_MAX_RUNS)
	{
		error = "too many runs (max 200)";
		return false;
	}

	out.name = name;
	out.id = id;
	out.bots = bots;
	out.seeds = seeds;
	out.repeat = repeat;
	out.durationSec = durationSec;
	return true;
}

void ScenarioRunner::Command(const std::string & args)
{
	std::vector<std::string> words;
	size_t pos = 0;
	while (pos < args.size())
	{
		size_t start = args.find_first_not_of(" \t\r\n", pos);
		if (start == std::string::npos)
			break;

		size_t end = args.find_first_of(" \t\r\n", start);
		words.push_back(args.substr(start,
			end == std::string::npos ? std::string::npos : end - start));
		pos = end == std::string::npos ? args.size() : end + 1;
	}

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

	WriteScenarioLog("ScenarioRunner: cmd scenario: usage: scenario run <name> | scenario stop | scenario status");
}

void ScenarioRunner::CommandRun(const std::string & name)
{
	char message[320];

	if (m_state != STATE_IDLE)
	{
		snprintf(message, sizeof(message),
			"ScenarioRunner: run %s: refused (already running (%s))", name.c_str(), m_scenario.id.c_str());
		WriteScenarioLog(message);
		return;
	}

	if (!Telemetry::Instance().IsEnabled(TEL_SUMMARY))
	{
		snprintf(message, sizeof(message), "ScenarioRunner: run %s: refused (telemetry is off)", name.c_str());
		WriteScenarioLog(message);
		return;
	}

	if (Telemetry::Instance().IsMatchActive())
	{
		snprintf(message, sizeof(message),
			"ScenarioRunner: run %s: refused (a match is already active (use 'match end'))", name.c_str());
		WriteScenarioLog(message);
		return;
	}

	Scenario loaded;
	std::string error;
	if (!LoadScenario(name, loaded, error))
	{
		snprintf(message, sizeof(message), "ScenarioRunner: run %s: refused (%s)", name.c_str(), error.c_str());
		WriteScenarioLog(message);
		return;
	}

	if (loaded.bots.size() > (size_t)m_mgr.m_poolSize)
	{
		snprintf(message, sizeof(message),
			"ScenarioRunner: run %s: refused (bot count %u exceeds pool size %u)",
			name.c_str(), (unsigned)loaded.bots.size(), (unsigned)m_mgr.m_poolSize);
		WriteScenarioLog(message);
		return;
	}

	for (size_t i = 0; i < m_mgr.m_sessions.size(); i++)
	{
		BotSession * s = m_mgr.m_sessions[i];

		bool listed = false;
		for (size_t j = 0; j < loaded.bots.size(); j++)
		{
			if (_stricmp(s->m_charName.c_str(), loaded.bots[j].c_str()) == 0)
			{
				listed = true;
				break;
			}
		}

		if (listed)
		{
			if (s->m_phase == BotSession::PHASE_DESPAWN_WAIT
				|| s->m_phase == BotSession::PHASE_FAILED
				|| s->m_phase == BotSession::PHASE_DESPAWN_STUCK)
			{
				snprintf(message, sizeof(message),
					"ScenarioRunner: run %s: refused (bot %s is in phase %s (cannot start))",
					name.c_str(), s->m_charName.c_str(), PhaseText(s->m_phase));
				WriteScenarioLog(message);
				return;
			}
		}
		else
		{
			if (s->m_phase == BotSession::PHASE_QUEUED
				|| s->m_phase == BotSession::PHASE_WAIT_SELECT
				|| s->m_phase == BotSession::PHASE_WAIT_LOADED
				|| s->m_phase == BotSession::PHASE_IN_GAME
				|| s->m_phase == BotSession::PHASE_DESPAWN_WAIT)
			{
				snprintf(message, sizeof(message),
					"ScenarioRunner: run %s: refused (session %s is active but not in the scenario)",
					name.c_str(), s->m_charName.c_str());
				WriteScenarioLog(message);
				return;
			}
		}
	}

	m_scenario = loaded;
	m_runSeeds.clear();
	for (size_t i = 0; i < m_scenario.seeds.size(); i++)
	{
		for (uint32 r = 0; r < m_scenario.repeat; r++)
			m_runSeeds.push_back(m_scenario.seeds[i]);
	}
	m_runIndex = 0;
	m_completedRuns = 0;
	m_abortReason.clear();

	snprintf(message, sizeof(message),
		"ScenarioRunner: run %s: loaded (id %s, %u bot(s), %u run(s), %u s each)",
		name.c_str(), m_scenario.id.c_str(), (unsigned)m_scenario.bots.size(),
		(unsigned)m_runSeeds.size(), (unsigned)m_scenario.durationSec);
	WriteScenarioLog(message);

	StartRun(std::chrono::steady_clock::now());
}

void ScenarioRunner::StartRun(std::chrono::steady_clock::time_point now)
{
	m_sessions.clear();

	std::string names;
	for (size_t i = 0; i < m_scenario.bots.size(); i++)
	{
		if (i != 0)
			names += ",";
		names += m_scenario.bots[i];
	}

	// Reuses the F2-06 command path; already in-game bots just print an "ignored" line.
	m_mgr.CommandSpawn(names);

	for (size_t i = 0; i < m_scenario.bots.size(); i++)
	{
		BotSession * s = m_mgr.FindSession(m_scenario.bots[i].c_str());
		if (s == nullptr)
		{
			Abort("spawn command failed", now);
			return;
		}
		m_sessions.push_back(s);
	}

	m_state = STATE_PREPARE;
	m_stateSince = now;

	char message[256];
	snprintf(message, sizeof(message),
		"ScenarioRunner: run %u/%u seed %u: preparing (%u bot(s))",
		(unsigned)(m_runIndex + 1), (unsigned)m_runSeeds.size(),
		(unsigned)m_runSeeds[m_runIndex], (unsigned)m_sessions.size());
	WriteScenarioLog(message);
}

void ScenarioRunner::Tick(std::chrono::steady_clock::time_point now)
{
	if (m_state == STATE_IDLE)
		return;

	if (m_state == STATE_PREPARE)
	{
		for (size_t i = 0; i < m_sessions.size(); i++)
		{
			if (m_sessions[i]->m_phase == BotSession::PHASE_FAILED)
			{
				Abort("spawn failed: " + m_sessions[i]->m_charName, now);
				return;
			}
		}

		bool allInGame = true;
		for (size_t i = 0; i < m_sessions.size(); i++)
		{
			if (m_sessions[i]->m_phase != BotSession::PHASE_IN_GAME)
			{
				allInGame = false;
				break;
			}
		}

		if (allInGame)
		{
			uint32 seed = m_runSeeds[m_runIndex];
			m_mgr.CommandMatch("start " + m_scenario.id + " " + std::to_string(seed));

			if (!Telemetry::Instance().IsMatchActive())
			{
				Abort("match start refused", now);
				return;
			}

			m_state = STATE_RUNNING;
			m_stateSince = m_matchSince = now;

			char message[224];
			snprintf(message, sizeof(message),
				"ScenarioRunner: run %u/%u: match open, running %u s",
				(unsigned)(m_runIndex + 1), (unsigned)m_runSeeds.size(),
				(unsigned)m_scenario.durationSec);
			WriteScenarioLog(message);
		}
		else if (now - m_stateSince > std::chrono::milliseconds(SCENARIO_PREPARE_TIMEOUT_MS))
		{
			Abort("prepare timeout", now);
		}
		return;
	}

	if (m_state == STATE_RUNNING)
	{
		if (!Telemetry::Instance().IsMatchActive())
		{
			Abort("match ended externally", now);
			return;
		}

		for (size_t i = 0; i < m_sessions.size(); i++)
		{
			if (m_sessions[i]->m_phase != BotSession::PHASE_IN_GAME)
			{
				m_mgr.CommandMatch("end bot_lost");
				Abort("bot lost: " + m_sessions[i]->m_charName, now);
				return;
			}
		}

		long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			now - m_matchSince).count();
		if (elapsed >= (long long)m_scenario.durationSec * 1000)
		{
			m_mgr.CommandMatch("end completed");
			m_completedRuns++;
			BeginCleanup(now);
		}
		return;
	}

	// STATE_CLEANUP
	for (size_t i = 0; i < m_sessions.size(); i++)
	{
		if (m_sessions[i]->m_phase == BotSession::PHASE_IN_GAME)
			m_mgr.BeginDespawn(m_sessions[i], now);
	}

	size_t despawned = 0;
	for (size_t i = 0; i < m_sessions.size(); i++)
	{
		BotSession * s = m_sessions[i];
		if (s->m_phase == BotSession::PHASE_FAILED || s->m_phase == BotSession::PHASE_DESPAWN_STUCK)
		{
			Finish("cleanup failed: " + s->m_charName);
			return;
		}
		if (s->m_phase == BotSession::PHASE_DESPAWNED)
			despawned++;
	}

	if (despawned == m_sessions.size())
	{
		if (!m_abortReason.empty())
		{
			Finish(m_abortReason);
		}
		else if (m_runIndex + 1 < m_runSeeds.size())
		{
			m_runIndex++;
			StartRun(now);
		}
		else
		{
			Finish("");
		}
		return;
	}

	if (now - m_stateSince > std::chrono::milliseconds(SCENARIO_CLEANUP_TIMEOUT_MS))
		Finish("cleanup timeout");
}

void ScenarioRunner::Abort(const std::string & reason, std::chrono::steady_clock::time_point now)
{
	if (m_abortReason.empty())
	{
		m_abortReason = reason;

		char message[256];
		snprintf(message, sizeof(message), "ScenarioRunner: aborting (%s)", reason.c_str());
		WriteScenarioLog(message);
	}

	if (Telemetry::Instance().IsMatchActive())
		m_mgr.CommandMatch("end aborted");

	BeginCleanup(now);
}

void ScenarioRunner::BeginCleanup(std::chrono::steady_clock::time_point now)
{
	m_state = STATE_CLEANUP;
	m_stateSince = now;

	char message[224];
	snprintf(message, sizeof(message),
		"ScenarioRunner: run %u/%u: cleanup (despawning)",
		(unsigned)(m_runIndex + 1), (unsigned)m_runSeeds.size());
	WriteScenarioLog(message);
}

void ScenarioRunner::Finish(const std::string & abortNote)
{
	char message[288];
	if (abortNote.empty())
	{
		snprintf(message, sizeof(message),
			"ScenarioRunner: scenario %s finished: %u/%u run(s) completed",
			m_scenario.id.c_str(), (unsigned)m_completedRuns, (unsigned)m_runSeeds.size());
	}
	else
	{
		snprintf(message, sizeof(message),
			"ScenarioRunner: scenario %s aborted (%s): %u/%u run(s) completed",
			m_scenario.id.c_str(), abortNote.c_str(),
			(unsigned)m_completedRuns, (unsigned)m_runSeeds.size());
	}
	WriteScenarioLog(message);

	m_state = STATE_IDLE;
	m_sessions.clear();
	m_abortReason.clear();
}

void ScenarioRunner::CommandStop()
{
	if (m_state == STATE_IDLE)
	{
		WriteScenarioLog("ScenarioRunner: stop: no scenario running");
		return;
	}

	if (m_state == STATE_CLEANUP)
	{
		WriteScenarioLog("ScenarioRunner: stop: already cleaning up");
		if (m_abortReason.empty())
			m_abortReason = "stopped by command";
		return;
	}

	Abort("stopped by command", std::chrono::steady_clock::now());
}

void ScenarioRunner::CommandStatus()
{
	if (m_state == STATE_IDLE)
	{
		WriteScenarioLog("ScenarioRunner: status: idle");
		return;
	}

	const char * stateText = m_state == STATE_PREPARE ? "prepare"
		: m_state == STATE_RUNNING ? "running" : "cleanup";
	long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - m_stateSince).count();

	char message[288];
	snprintf(message, sizeof(message),
		"ScenarioRunner: status: %s run %u/%u seed %u state %s, %lld ms in state, %u completed",
		m_scenario.id.c_str(), (unsigned)(m_runIndex + 1), (unsigned)m_runSeeds.size(),
		(unsigned)m_runSeeds[m_runIndex], stateText, elapsed, (unsigned)m_completedRuns);
	WriteScenarioLog(message);
}
