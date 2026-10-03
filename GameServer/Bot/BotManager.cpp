#include "stdafx.h"
#include "BotManager.h"
#include "IBotSink.h"
#include "BotSession.h"
#include "Telemetry.h"
#include "ActionExecutor.h"
#include "ScenarioRunner.h"
#include "../../BotCore/BotMotion.h"
#include "../../BotCore/SpawnGate.h"
#include "../../shared/Ini.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Delay after the first tick, so the AI server has time to connect.
static const uint32 SPAWN_START_DELAY_MS = 5000;
// SelectCharacter() sends its reply before SetUserAbility(false)/SetRegion finish running
// on the DB thread; a real client's loading time hides that, so let the bot settle.
static const uint32 SELECT_SETTLE_MS = 1000;
static const uint32 LOADED_DELAY_MS = 200;
static const uint32 PHASE_TIMEOUT_MS = 15000;
static const uint32 UPDATE_PERIOD_MS = 1000;
static const uint32 DESPAWN_TIMEOUT_MS = 30000;
static const uint32 CYCLE_PROGRESS_EVERY = 50;

static const char * COMMAND_FILE = "./BotCommands.txt";
static const char * COMMAND_FILE_CLAIMED = "./BotCommands.processing";
static const uint32 COMMAND_POLL_MS = 1000;
static const size_t COMMAND_QUEUE_MAX = 64;
static const size_t COMMAND_FILE_MAX_LINES = 64;
static const uint32 STATUS_REFRESH_MS = 1000;

BotManager & BotManager::Instance()
{
	static BotManager instance;
	return instance;
}

// Appends one line to ./Logs/Bot_<day>_<month>_<year>.log (silently skipped if it cannot be opened).
static void WriteBotLog(const char * line)
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

// Only these db/002 bot characters may spawn; the ini list never introduces new accounts.
struct BotAccountEntry { const char * charName; const char * accountName; };
static const BotAccountEntry BOT_TABLE[] =
{
	{ "BotWP_K", "BotAccWPK" }, { "BotWG_K", "BotAccWGK" }, { "BotPHD_K", "BotAccPHDK" },
	{ "BotPHB_K", "BotAccPHBK" }, { "BotMF_K", "BotAccMFK" }, { "BotMI_K", "BotAccMIK" },
	{ "BotWP_E", "BotAccWPE" }, { "BotWG_E", "BotAccWGE" }, { "BotPHD_E", "BotAccPHDE" },
	{ "BotPHB_E", "BotAccPHBE" }, { "BotMF_E", "BotAccMFE" }, { "BotMI_E", "BotAccMIE" }
};

static const BotAccountEntry * FindBotEntry(const char * name)
{
	for (size_t i = 0; i < sizeof(BOT_TABLE) / sizeof(BOT_TABLE[0]); i++)
	{
		if (_stricmp(name, BOT_TABLE[i].charName) == 0)
			return &BOT_TABLE[i];
	}

	return nullptr;
}

static const char * PhaseName(BotSession::Phase phase)
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

static std::string Trim(const std::string & text)
{
	size_t first = text.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return "";

	size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

static void SplitNames(const std::string & text, std::vector<std::string> & out)
{
	size_t pos = 0;
	while (pos <= text.size())
	{
		size_t sep = text.find_first_of(", \t", pos);
		std::string name = text.substr(pos, sep == std::string::npos ? std::string::npos : sep - pos);
		if (!name.empty())
			out.push_back(name);

		if (sep == std::string::npos)
			break;
		pos = sep + 1;
	}
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

// Rejects NaN/inf and trailing characters (strtod alone would accept both).
static bool ParseDoubleStrict(const std::string & text, double & out)
{
	char * end = nullptr;
	double value = strtod(text.c_str(), &end);
	if (end == text.c_str() || *end != '\0')
		return false;
	if (!(value == value) || value > 1e30 || value < -1e30)
		return false;

	out = value;
	return true;
}

static bool ParseIntStrict(const std::string & text, long & out)
{
	char * end = nullptr;
	long value = strtol(text.c_str(), &end, 10);
	if (end == text.c_str() || *end != '\0')
		return false;

	out = value;
	return true;
}

bool BotManager::Startup()
{
	CIni ini(CONF_GAME_SERVER);
	m_enabled = ini.GetBool("BOT", "ENABLED", false);
	if (!m_enabled)
		return true;

	int requested = ini.GetInt("BOT", "MAX_BOTS", 16);
	if (requested < 1)
		requested = 1;
	else if (requested > MAX_POOL)
		requested = MAX_POOL;
	if (requested > MAX_USER)
		requested = MAX_USER;

	int tickMs = ini.GetInt("BOT", "TICK_MS", 100);
	if (tickMs < 20)
		tickMs = 20;
	else if (tickMs > 1000)
		tickMs = 1000;
	m_tickMs = (uint32)tickMs;

	std::string spawnList;
	ini.GetString("BOT", "SPAWN_ON_START", "", spawnList);

	int despawnSec = ini.GetInt("BOT", "DESPAWN_AFTER_SEC", 0);
	if (despawnSec < 0)
		despawnSec = 0;
	else if (despawnSec > 86400)
		despawnSec = 86400;
	m_despawnAfterMs = (uint32)despawnSec * 1000;

	int respawnCycles = ini.GetInt("BOT", "RESPAWN_CYCLES", 0);
	if (respawnCycles < 0)
		respawnCycles = 0;
	else if (respawnCycles > 100000)
		respawnCycles = 100000;
	m_respawnCycles = (uint32)respawnCycles;

	m_speedCheck = ini.GetInt("BOT", "SPEEDHACK_CHECK", 1) != 0;

	auto & mgr = g_pMain->m_socketMgr;
	std::lock_guard<std::recursive_mutex> lock(mgr.GetLock());

	m_poolSize = mgr.ReserveSessions((uint16)requested);
	bool ok = (m_poolSize == (uint16)requested);
	const char * reason = ok ? "" : "reserve";

	uint16 lowestId = 0, highestId = 0;
	CUser * acquired[MAX_POOL];
	uint16 acquiredCount = 0;

	if (ok)
	{
		auto & reserved = mgr.GetReservedSessionMap();
		lowestId = reserved.begin()->first;
		highestId = reserved.rbegin()->first;

		for (auto itr = reserved.begin(); itr != reserved.end(); ++itr)
		{
			uint16 id = itr->first;
			if (id < (uint16)(MAX_USER - m_poolSize)
				|| mgr.GetIdleSessionMap().find(id) != mgr.GetIdleSessionMap().end()
				|| mgr.GetActiveSessionMap().find(id) != mgr.GetActiveSessionMap().end())
			{
				ok = false;
				reason = "range";
				break;
			}
		}
	}

	if (ok)
	{
		for (uint16 i = 0; i < m_poolSize; i++)
		{
			CUser * pUser = AcquireSlot();
			if (pUser == nullptr)
			{
				ok = false;
				reason = "acquire";
				break;
			}

			bool duplicate = false;
			for (uint16 j = 0; j < acquiredCount; j++)
			{
				if (acquired[j] == pUser)
				{
					duplicate = true;
					break;
				}
			}

			uint16 id = pUser->GetSocketID();
			if (duplicate
				|| mgr.GetActiveSessionMap().find(id) == mgr.GetActiveSessionMap().end()
				|| mgr.GetReservedSessionMap().find(id) != mgr.GetReservedSessionMap().end())
			{
				ok = false;
				reason = "acquire";
				break;
			}

			acquired[acquiredCount++] = pUser;
		}
	}

	if (ok && AcquireSlot() != nullptr)
	{
		ok = false;
		reason = "exhaust";
	}

	if (ok)
	{
		for (uint16 i = 0; i < acquiredCount; i++)
			ReleaseSlot(acquired[i]);

		if (mgr.GetReservedSessionMap().size() != (size_t)m_poolSize)
		{
			ok = false;
			reason = "release";
		}
		else
		{
			for (uint16 i = 0; i < acquiredCount; i++)
			{
				uint16 id = acquired[i]->GetSocketID();
				if (mgr.GetActiveSessionMap().find(id) != mgr.GetActiveSessionMap().end()
					|| mgr.GetIdleSessionMap().find(id) != mgr.GetIdleSessionMap().end())
				{
					ok = false;
					reason = "release";
					break;
				}
			}
		}
	}

	char message[160];
	if (ok)
		snprintf(message, sizeof(message),
			"BotManager: reserved %u sessions (ids %u-%u), pool self-test OK",
			(unsigned)m_poolSize, (unsigned)lowestId, (unsigned)highestId);
	else
		snprintf(message, sizeof(message), "BotManager: pool setup FAILED (%s)", reason);

	printf("%s\n", message);
	WriteBotLog(message);

	if (ok)
		ParseSpawnList(spawnList);

	if (ok)
		Telemetry::Instance().Start();

	return ok;
}

CUser * BotManager::AcquireSlot()
{
	std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock());
	return g_pMain->m_socketMgr.AcquireReservedSession();
}

void BotManager::ReleaseSlot(CUser * pUser)
{
	if (pUser == nullptr)
		return;

	std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock());
	pUser->m_botSink = nullptr;
	g_pMain->m_socketMgr.ReleaseReservedSession(pUser);
}

void BotManager::StartTicking()
{
	if (!m_enabled || m_timerThread != nullptr)
		return;

	SocketMgr::SetBotTickHandler(&BotManager::TickCallback);
	m_timerThread = new Thread(TimerThreadProc, this);
}

void BotManager::Shutdown()
{
	m_shuttingDown = true;
	if (m_timerThread != nullptr)
	{
		m_timerThread->waitForExit();
		delete m_timerThread;
		m_timerThread = nullptr;
	}

	Telemetry::Instance().Stop();
}

bool BotManager::EnqueueCommand(const std::string & line)
{
	if (!m_enabled)
		return false;

	std::lock_guard<std::mutex> lock(m_commandLock);
	if (m_commandQueue.size() >= COMMAND_QUEUE_MAX)
		return false;

	m_commandQueue.push_back(line);
	return true;
}

uint32 THREADCALL BotManager::TimerThreadProc(void * lpParam)
{
	BotManager * self = (BotManager *)lpParam;
	self->m_timerThreadId = GetCurrentThreadId();
	while (g_bRunning && !self->m_shuttingDown)
	{
		sleep(self->m_tickMs);
		if (!g_pMain->m_socketMgr.PostBotTick())
			self->m_skippedTicks++;
	}

	return 0;
}

void BotManager::TickCallback()
{
	Instance().Tick();
}

void BotManager::Tick()
{
	if (m_shuttingDown)
		return;

	std::chrono::steady_clock::time_point tickStart = std::chrono::steady_clock::now();

	m_tickCount++;

	if (m_tickCount == 1)
	{
		m_tickThreadId = GetCurrentThreadId();
		m_firstTickTime = std::chrono::steady_clock::now();

		char message[160];
		if (m_tickThreadId != m_timerThreadId)
			snprintf(message, sizeof(message),
				"BotManager: tick OK on IOCP thread %u (timer thread %u), period %u ms",
				(unsigned)m_tickThreadId, (unsigned)m_timerThreadId, (unsigned)m_tickMs);
		else
			snprintf(message, sizeof(message),
				"BotManager: tick CHECK FAILED (tick ran on the timer thread %u)",
				(unsigned)m_tickThreadId);
		WriteBotLog(message);
	}
	else if (m_tickCount == 101)
	{
		long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - m_firstTickTime).count();

		char message[200];
		snprintf(message, sizeof(message),
			"BotManager: 100 tick intervals in %lld ms (avg %.1f ms), skipped %u",
			elapsed, elapsed / 100.0, (unsigned)m_skippedTicks);
		WriteBotLog(message);
	}

	ProcessCommands();
	TickSessions();
	RefreshStatusSnapshot(std::chrono::steady_clock::now());
	m_scenario.Tick(std::chrono::steady_clock::now());
	m_script.Tick(std::chrono::steady_clock::now());

	if (Telemetry::Instance().IsEnabled(TEL_SUMMARY))
		RecordTick(tickStart);
}

void BotManager::RecordTick(std::chrono::steady_clock::time_point tickStart)
{
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	long long us = std::chrono::duration_cast<std::chrono::microseconds>(end - tickStart).count();

	if (!m_perfWindowOpen)
	{
		m_perfWindowOpen = true;
		m_perfWindowStart = tickStart;
		m_tickUs.reserve(4096);
	}

	if (m_tickUs.size() < 4096)
		m_tickUs.push_back((uint32)us);

	if (end - m_perfWindowStart >= std::chrono::milliseconds(5000))
		EmitPerfSample(end);
}

void BotManager::EmitPerfSample(std::chrono::steady_clock::time_point now)
{
	size_t n = m_tickUs.size();
	if (n == 0)
	{
		m_perfWindowStart = now;
		return;
	}

	std::vector<uint32> sorted(m_tickUs);
	std::sort(sorted.begin(), sorted.end());

	size_t i50 = (size_t)ceil(0.50 * n) - 1;
	size_t i95 = (size_t)ceil(0.95 * n) - 1;
	size_t i99 = (size_t)ceil(0.99 * n) - 1;
	if (i50 >= n) i50 = n - 1;
	if (i95 >= n) i95 = n - 1;
	if (i99 >= n) i99 = n - 1;

	uint32 p50 = sorted[i50];
	uint32 p95 = sorted[i95];
	uint32 p99 = sorted[i99];
	uint32 maxUs = sorted.back();

	uint32 inGame = 0;
	for (size_t i = 0; i < m_sessions.size(); i++)
	{
		if (m_sessions[i]->m_phase == BotSession::PHASE_IN_GAME)
			inGame++;
	}

	size_t poolFree;
	{
		std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock());
		poolFree = g_pMain->m_socketMgr.GetReservedSessionMap().size();
	}

	uint32 skipped = m_skippedTicks.load();
	TelemetryStats st = Telemetry::Instance().GetStats();
	long long windowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_perfWindowStart).count();

	char fields[512];
	snprintf(fields, sizeof(fields),
		"\"window_ms\":%lld,\"tick_n\":%u,\"tick_p50_us\":%u,\"tick_p95_us\":%u,\"tick_p99_us\":%u,\"tick_max_us\":%u,\"sessions\":%u,\"in_game\":%u,\"pool_free\":%u,\"skipped_ticks\":%u,\"queue_len\":%u,\"written\":%llu,\"dropped_soft\":%llu,\"dropped_hard\":%llu",
		windowMs, (unsigned)n, (unsigned)p50, (unsigned)p95, (unsigned)p99, (unsigned)maxUs,
		(unsigned)m_sessions.size(), (unsigned)inGame, (unsigned)poolFree, (unsigned)skipped,
		(unsigned)st.queueLen, (unsigned long long)st.written,
		(unsigned long long)st.droppedSoft, (unsigned long long)st.droppedHard);

	m_matchPerfSamples++;
	if (p95 > m_matchP95MaxUs)
		m_matchP95MaxUs = p95;
	if (maxUs > m_matchTickMaxUs)
		m_matchTickMaxUs = maxUs;

	Telemetry::Instance().Emit(TEL_SUMMARY, "PERF_SAMPLE", -1, nullptr, fields, true);

	m_tickUs.clear();
	m_perfWindowStart = now;
}

void BotManager::ProcessCommands()
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	// Let the AI server connect first, exactly like the initial spawn list. Queued/file commands
	// simply wait until the delay has passed.
	if (now - m_firstTickTime < std::chrono::milliseconds(SPAWN_START_DELAY_MS))
		return;

	PollCommandFile(now);

	std::vector<std::string> lines;
	{
		std::lock_guard<std::mutex> lock(m_commandLock);
		lines.swap(m_commandQueue);
	}

	for (size_t i = 0; i < lines.size(); i++)
		ExecuteCommand(lines[i]);
}

void BotManager::PollCommandFile(std::chrono::steady_clock::time_point now)
{
	if (now - m_lastCommandPoll < std::chrono::milliseconds(COMMAND_POLL_MS))
		return;
	m_lastCommandPoll = now;

	// A stale claimed file may be left over from a crash; ignore the failure.
	remove(COMMAND_FILE_CLAIMED);
	if (rename(COMMAND_FILE, COMMAND_FILE_CLAIMED) != 0)
		return;

	FILE * fp = fopen(COMMAND_FILE_CLAIMED, "r");
	if (fp == nullptr)
	{
		WriteBotLog("BotManager: command file could not be read");
		return;
	}

	uint32 executed = 0;
	bool overflow = false;
	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp) != nullptr)
	{
		std::string line = Trim(buffer);
		if (line.empty() || line[0] == '#')
			continue;

		if (executed >= COMMAND_FILE_MAX_LINES)
		{
			overflow = true;
			continue;
		}

		ExecuteCommand(line);
		executed++;
	}

	fclose(fp);
	remove(COMMAND_FILE_CLAIMED);

	char message[128];
	if (overflow)
	{
		snprintf(message, sizeof(message),
			"BotManager: command file: more than %u lines, rest ignored", (unsigned)COMMAND_FILE_MAX_LINES);
		WriteBotLog(message);
	}

	if (executed > 0)
	{
		snprintf(message, sizeof(message), "BotManager: command file: %u command(s) executed", (unsigned)executed);
		WriteBotLog(message);
	}
}

void BotManager::ExecuteCommand(const std::string & line)
{
	std::string trimmed = Trim(line);
	if (trimmed.empty())
		return;

	char message[320];
	snprintf(message, sizeof(message), "BotManager: cmd '%s'", trimmed.c_str());
	WriteBotLog(message);

	if (m_respawnCycles != 0)
	{
		WriteBotLog("BotManager: cmd rejected (RESPAWN_CYCLES is active)");
		return;
	}

	size_t space = trimmed.find(' ');
	std::string verb = space == std::string::npos ? trimmed : trimmed.substr(0, space);
	std::string args = space == std::string::npos ? "" : Trim(trimmed.substr(space + 1));

	if (_stricmp(verb.c_str(), "spawn") == 0)
		CommandSpawn(args);
	else if (_stricmp(verb.c_str(), "despawn") == 0)
		CommandDespawn(args);
	else if (_stricmp(verb.c_str(), "list") == 0)
		CommandList();
	else if (_stricmp(verb.c_str(), "match") == 0)
		CommandMatch(args);
	else if (_stricmp(verb.c_str(), "scenario") == 0)
		m_scenario.Command(args);
	else if (_stricmp(verb.c_str(), "script") == 0)
		m_script.Command(args);
	else if (_stricmp(verb.c_str(), "move") == 0)
		CommandMove(args);
	else if (_stricmp(verb.c_str(), "goto") == 0)
		CommandGoto(args);
	else if (_stricmp(verb.c_str(), "stop") == 0)
		CommandStop(args);
	else if (_stricmp(verb.c_str(), "attack") == 0)
		CommandAttack(args);
	else if (_stricmp(verb.c_str(), "cast") == 0)
		CommandCast(args);
	else if (_stricmp(verb.c_str(), "pot") == 0)
		CommandPot(args);
	else if (_stricmp(verb.c_str(), "sit") == 0)
		CommandStance(args, true);
	else if (_stricmp(verb.c_str(), "stand") == 0)
		CommandStance(args, false);
	else if (_stricmp(verb.c_str(), "target") == 0)
		CommandTarget(args);
	else if (_stricmp(verb.c_str(), "regene") == 0)
		CommandRegene(args);
	else if (_stricmp(verb.c_str(), "pinvite") == 0)
		CommandPartyInvite(args);
	else if (_stricmp(verb.c_str(), "paccept") == 0)
		CommandPartyAccept(args);
	else if (_stricmp(verb.c_str(), "pdecline") == 0)
		CommandPartyDecline(args);
	else if (_stricmp(verb.c_str(), "pleave") == 0)
		CommandPartyLeave(args);
	else if (_stricmp(verb.c_str(), "ppromote") == 0)
		CommandPartyManage(args, false);
	else if (_stricmp(verb.c_str(), "pkick") == 0)
		CommandPartyManage(args, true);
	else if (_stricmp(verb.c_str(), "pchat") == 0)
		CommandPartyChat(args);
	else if (_stricmp(verb.c_str(), "see") == 0)
		CommandSee(args);
	else if (_stricmp(verb.c_str(), "npcs") == 0)
		CommandNpcs(args);
	else if (_stricmp(verb.c_str(), "snap") == 0)
		CommandSnap(args);
	else
	{
		snprintf(message, sizeof(message),
			"BotManager: cmd unknown command '%s' (spawn, despawn, list, match, scenario, script, move, goto, stop, attack, cast, pot, sit, stand, target, regene, pinvite, paccept, pdecline, pleave, ppromote, pkick, pchat, see, npcs, snap)", verb.c_str());
		WriteBotLog(message);
	}
}

BotSession * BotManager::FindSession(const char * charName)
{
	for (size_t i = 0; i < m_sessions.size(); i++)
	{
		if (_stricmp(m_sessions[i]->m_charName.c_str(), charName) == 0)
			return m_sessions[i];
	}

	return nullptr;
}

bool BotManager::IsKnownBotName(const std::string & name)
{
	return FindBotEntry(name.c_str()) != nullptr;
}

void BotManager::CommandSpawn(const std::string & args)
{
	if (args.empty())
	{
		WriteBotLog("BotManager: cmd spawn: no names given");
		return;
	}

	std::vector<std::string> names;
	SplitNames(args, names);

	uint32 queued = 0, ignored = 0;

	for (size_t i = 0; i < names.size(); i++)
	{
		char message[224];
		const std::string & name = names[i];
		const BotAccountEntry * entry = FindBotEntry(name.c_str());
		if (entry == nullptr)
		{
			snprintf(message, sizeof(message),
				"BotManager: cmd spawn: unknown bot name '%s' ignored", name.c_str());
			WriteBotLog(message);
			ignored++;
			continue;
		}

		BotSession * s = FindSession(entry->charName);
		if (s != nullptr)
		{
			if (s->m_phase == BotSession::PHASE_DESPAWNED)
			{
				s->ResetForRespawn();
				snprintf(message, sizeof(message), "BotManager: cmd spawn: %s queued again", entry->charName);
				WriteBotLog(message);
				queued++;
			}
			else
			{
				snprintf(message, sizeof(message),
					"BotManager: cmd spawn: %s ignored (phase %s)", entry->charName, PhaseName(s->m_phase));
				WriteBotLog(message);
				ignored++;
			}
			continue;
		}

		m_sessions.push_back(new BotSession(entry->charName, entry->accountName));
		snprintf(message, sizeof(message), "BotManager: cmd spawn: %s queued", entry->charName);
		WriteBotLog(message);
		queued++;
	}

	char message[128];
	snprintf(message, sizeof(message),
		"BotManager: cmd spawn: %u queued, %u ignored", (unsigned)queued, (unsigned)ignored);
	WriteBotLog(message);
}

void BotManager::CommandDespawn(const std::string & args)
{
	if (args.empty())
	{
		WriteBotLog("BotManager: cmd despawn: no names given");
		return;
	}

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	std::string lower = args;
	STRTOLOWER(lower);

	if (lower == "all")
	{
		uint32 despawning = 0, notInGame = 0;
		for (size_t i = 0; i < m_sessions.size(); i++)
		{
			BotSession * s = m_sessions[i];
			if (s->m_phase == BotSession::PHASE_IN_GAME)
			{
				BeginDespawn(s, now);
				despawning++;
			}
			else
			{
				notInGame++;
			}
		}

		char message[160];
		snprintf(message, sizeof(message),
			"BotManager: cmd despawn all: %u despawning, %u not in game",
			(unsigned)despawning, (unsigned)notInGame);
		WriteBotLog(message);
		return;
	}

	std::vector<std::string> names;
	SplitNames(args, names);

	for (size_t i = 0; i < names.size(); i++)
	{
		char message[224];
		const std::string & name = names[i];
		BotSession * s = FindSession(name.c_str());
		if (s == nullptr)
		{
			snprintf(message, sizeof(message),
				"BotManager: cmd despawn: %s ignored (no such session)", name.c_str());
			WriteBotLog(message);
			continue;
		}

		if (s->m_phase == BotSession::PHASE_IN_GAME)
		{
			BeginDespawn(s, now);
		}
		else
		{
			snprintf(message, sizeof(message),
				"BotManager: cmd despawn: %s ignored (phase %s)", name.c_str(), PhaseName(s->m_phase));
			WriteBotLog(message);
		}
	}
}

void BotManager::BuildStatusLines(std::vector<std::string> & out)
{
	out.clear();

	size_t poolFree;
	{
		std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock());
		poolFree = g_pMain->m_socketMgr.GetReservedSessionMap().size();
	}

	char message[256];
	snprintf(message, sizeof(message),
		"%u session(s), pool free %u/%u",
		(unsigned)m_sessions.size(), (unsigned)poolFree, (unsigned)m_poolSize);
	out.push_back(message);

	for (size_t i = 0; i < m_sessions.size(); i++)
	{
		BotSession * s = m_sessions[i];
		char slot[16];
		if (s->m_pUser != nullptr)
			snprintf(slot, sizeof(slot), "%u", (unsigned)s->m_slotId);
		else
			snprintf(slot, sizeof(slot), "-");

		char pos[32];
		if (s->m_pUser != nullptr)
			snprintf(pos, sizeof(pos), "%.1f,%.1f", s->m_pUser->GetX(), s->m_pUser->GetZ());
		else
			snprintf(pos, sizeof(pos), "-");

		char hp[24];
		if (s->m_pUser != nullptr)
			snprintf(hp, sizeof(hp), "%d/%d", s->m_pUser->GetHealth(), s->m_pUser->GetMaxHealth());
		else
			snprintf(hp, sizeof(hp), "-");

		char mp[24];
		if (s->m_pUser != nullptr)
			snprintf(mp, sizeof(mp), "%d/%d", s->m_pUser->GetMana(), s->m_pUser->GetMaxMana());
		else
			snprintf(mp, sizeof(mp), "-");

		snprintf(message, sizeof(message),
			"  %s phase=%s slot=%s despawns=%u pos=%s moving=%d moverx=%u hp=%s attacking=%d casting=%d mp=%s pot=%d sit=%d",
			s->m_charName.c_str(), PhaseName(s->m_phase), slot, (unsigned)s->m_despawnCount,
			pos, s->m_moveActive ? 1 : 0, (unsigned)s->m_opcodeCount[WIZ_MOVE].load(),
			hp, s->m_attackActive ? 1 : 0, s->m_castPhase != BotSession::CAST_IDLE ? 1 : 0, mp,
			s->m_potActive ? 1 : 0,
			(s->m_pUser != nullptr && s->m_pUser->m_bResHpType == USER_SITDOWN) ? 1 : 0);
		out.push_back(message);
	}
}

void BotManager::RefreshStatusSnapshot(std::chrono::steady_clock::time_point now)
{
	if (now - m_lastStatusRefresh < std::chrono::milliseconds(STATUS_REFRESH_MS))
		return;

	m_lastStatusRefresh = now;

	std::vector<std::string> lines;
	BuildStatusLines(lines);

	std::lock_guard<std::mutex> lock(m_statusLock);
	m_statusLines.swap(lines);
}

bool BotManager::GetStatusSnapshot(std::vector<std::string> & out)
{
	if (!m_enabled)
		return false;

	std::lock_guard<std::mutex> lock(m_statusLock);
	out = m_statusLines;
	return true;
}

void BotManager::CommandList()
{
	std::vector<std::string> lines;
	BuildStatusLines(lines);

	for (size_t i = 0; i < lines.size(); i++)
		WriteBotLog(("BotManager: cmd list: " + lines[i]).c_str());
}

void BotManager::CommandMatch(const std::string & args)
{
	std::vector<std::string> words;
	{
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
	}

	if (words.empty())
	{
		WriteBotLog("BotManager: cmd match: usage: match start <scenario> [seed] | match end [result]");
		return;
	}

	const std::string & sub = words[0];

	if (_stricmp(sub.c_str(), "start") == 0)
	{
		if (words.size() < 2)
		{
			WriteBotLog("BotManager: cmd match start: no scenario given");
			return;
		}

		if (words.size() > 3)
		{
			WriteBotLog("BotManager: cmd match start: too many arguments");
			return;
		}

		const std::string & scenario = words[1];
		uint32 seed = 0;

		if (words.size() == 3)
		{
			const std::string & seedText = words[2];
			bool digits = !seedText.empty() && seedText.size() <= 10;
			for (size_t i = 0; digits && i < seedText.size(); i++)
			{
				if (seedText[i] < '0' || seedText[i] > '9')
					digits = false;
			}

			if (!digits)
			{
				char message[224];
				snprintf(message, sizeof(message),
					"BotManager: cmd match start: bad seed '%s'", seedText.c_str());
				WriteBotLog(message);
				return;
			}

			unsigned long long value = strtoull(seedText.c_str(), NULL, 10);
			if (value > 4294967295ULL)
			{
				char message[224];
				snprintf(message, sizeof(message),
					"BotManager: cmd match start: bad seed '%s'", seedText.c_str());
				WriteBotLog(message);
				return;
			}
			seed = (uint32)value;
		}

		std::string composition = "\"composition\":[";
		uint32 inGame = 0;
		bool first = true;
		for (size_t i = 0; i < m_sessions.size(); i++)
		{
			if (m_sessions[i]->m_phase != BotSession::PHASE_IN_GAME)
				continue;

			if (!first)
				composition += ",";
			first = false;
			composition += "\"";
			composition += Telemetry::EscapeJson(m_sessions[i]->m_charName);
			composition += "\"";
			inGame++;
		}
		composition += "],\"in_game\":";
		composition += std::to_string(inGame);

		std::string id, error;
		if (Telemetry::Instance().BeginMatch(scenario, seed, composition, id, error))
		{
			m_matchPerfSamples = 0;
			m_matchP95MaxUs = 0;
			m_matchTickMaxUs = 0;

			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd match start: %s started (%u bot(s) in game)",
				id.c_str(), (unsigned)inGame);
			WriteBotLog(message);
		}
		else
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd match start: refused (%s)", error.c_str());
			WriteBotLog(message);
		}
		return;
	}

	if (_stricmp(sub.c_str(), "end") == 0)
	{
		if (words.size() > 2)
		{
			WriteBotLog("BotManager: cmd match end: too many arguments");
			return;
		}

		std::string result = words.size() == 2 ? words[1] : "completed";

		uint32 inGame = 0;
		for (size_t i = 0; i < m_sessions.size(); i++)
		{
			if (m_sessions[i]->m_phase == BotSession::PHASE_IN_GAME)
				inGame++;
		}

		char extra[192];
		snprintf(extra, sizeof(extra),
			"\"in_game\":%u,\"perf_samples\":%u,\"tick_p95_max_us\":%u,\"tick_max_us\":%u",
			(unsigned)inGame, (unsigned)m_matchPerfSamples,
			(unsigned)m_matchP95MaxUs, (unsigned)m_matchTickMaxUs);

		std::string id, error;
		long long durationMs = 0;
		if (Telemetry::Instance().EndMatch(result, extra, id, durationMs, error))
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd match end: %s ended (result %s, %lld ms)",
				id.c_str(), result.c_str(), durationMs);
			WriteBotLog(message);
		}
		else
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd match end: refused (%s)", error.c_str());
			WriteBotLog(message);
		}
		return;
	}

	WriteBotLog("BotManager: cmd match: usage: match start <scenario> [seed] | match end [result]");
}

void BotManager::CommandMove(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() < 3 || words.size() > 4)
	{
		WriteBotLog("BotManager: cmd move: usage: move <bot> <x> <z> [speed]");
		return;
	}

	const std::string & name = words[0];
	BotSession * s = FindSession(name.c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd move: unknown or not spawned bot '%s'",
			IsKnownBotName(name) ? name.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd move: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	double x = 0.0, z = 0.0;
	if (!ParseDoubleStrict(words[1], x) || !ParseDoubleStrict(words[2], z))
	{
		WriteBotLog("BotManager: cmd move: usage: move <bot> <x> <z> [speed]");
		return;
	}

	long speedField = BotCore::kWalkSpeedField;
	if (words.size() == 4)
	{
		if (!ParseIntStrict(words[3], speedField) || speedField < -32768 || speedField > 32767)
		{
			WriteBotLog("BotManager: cmd move: usage: move <bot> <x> <z> [speed]");
			return;
		}
	}

	MoveOutcome outcome = ActionExecutor::BeginMove(s, (float)x, (float)z, (int16)speedField,
		std::chrono::steady_clock::now());

	char message[256];
	if (outcome.kind == MoveOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd move: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd move: %s walking to (%.1f, %.1f) at speed %d",
			s->m_charName.c_str(), x, z, (int)speedField);
	WriteBotLog(message);
}

// /bot goto <bot> <x> <z> [speed] (F5-70): same shape as CommandMove, but the walk follows a planned
// navigation path (NAV=1, zone 71). The planning time around BeginGoto is measured and reported.
void BotManager::CommandGoto(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() < 3 || words.size() > 4)
	{
		WriteBotLog("BotManager: cmd goto: usage: goto <bot> <x> <z> [speed]");
		return;
	}

	const std::string & name = words[0];
	BotSession * s = FindSession(name.c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd goto: unknown or not spawned bot '%s'",
			IsKnownBotName(name) ? name.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd goto: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	double x = 0.0, z = 0.0;
	if (!ParseDoubleStrict(words[1], x) || !ParseDoubleStrict(words[2], z))
	{
		WriteBotLog("BotManager: cmd goto: usage: goto <bot> <x> <z> [speed]");
		return;
	}

	long speedField = BotCore::kWalkSpeedField;
	if (words.size() == 4)
	{
		if (!ParseIntStrict(words[3], speedField) || speedField < -32768 || speedField > 32767)
		{
			WriteBotLog("BotManager: cmd goto: usage: goto <bot> <x> <z> [speed]");
			return;
		}
	}

	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	MoveOutcome outcome = ActionExecutor::BeginGoto(s, (float)x, (float)z, (int16)speedField, t0);
	double elapsedMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - t0).count();

	char message[320];
	if (outcome.kind == MoveOutcome::REFUSED)
	{
		if (strcmp(outcome.reason, "node_limit") == 0)
			snprintf(message, sizeof(message),
				"BotManager: cmd goto: %s refused (%s) [not planned: search budget exhausted, reachability unknown]",
				s->m_charName.c_str(), outcome.reason);
		else
			snprintf(message, sizeof(message),
				"BotManager: cmd goto: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	}
	else
	{
		snprintf(message, sizeof(message),
			"BotManager: cmd goto: %s planned %d waypoints, route %.1f m, expanded %d, %.2f ms, walking to (%.1f, %.1f) at speed %d",
			s->m_charName.c_str(), s->m_navDrive.PlanWaypoints(), (double)s->m_navDrive.RouteLengthM(),
			s->m_navDrive.PlanExpanded(), elapsedMs, (double)s->m_navDrive.GoalX(),
			(double)s->m_navDrive.GoalZ(), (int)speedField);
	}
	WriteBotLog(message);
}

void BotManager::CommandStop(const std::string & args)
{
	if (args.empty())
	{
		WriteBotLog("BotManager: cmd stop: no names given");
		return;
	}

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	std::string lower = args;
	STRTOLOWER(lower);

	if (lower == "all")
	{
		uint32 stopped = 0, notMoving = 0;
		for (size_t i = 0; i < m_sessions.size(); i++)
		{
			BotSession * s = m_sessions[i];
			if (s->m_phase != BotSession::PHASE_IN_GAME)
				continue;

			MoveOutcome outcome = ActionExecutor::StopMove(s, now);
			char message[224];
			if (outcome.kind == MoveOutcome::ARRIVED)
			{
				snprintf(message, sizeof(message),
					"BotManager: cmd stop: %s stopped at (%.1f, %.1f)",
					s->m_charName.c_str(), s->m_pUser->GetX(), s->m_pUser->GetZ());
				stopped++;
			}
			else
			{
				snprintf(message, sizeof(message),
					"BotManager: cmd stop: %s not moving", s->m_charName.c_str());
				notMoving++;
			}
			WriteBotLog(message);
		}

		char summary[128];
		snprintf(summary, sizeof(summary),
			"BotManager: cmd stop all: %u stopped, %u not moving", (unsigned)stopped, (unsigned)notMoving);
		WriteBotLog(summary);
		return;
	}

	std::vector<std::string> names;
	SplitNames(args, names);

	for (size_t i = 0; i < names.size(); i++)
	{
		const std::string & name = names[i];
		BotSession * s = FindSession(name.c_str());
		if (s == nullptr)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd stop: unknown or not spawned bot '%s'",
				IsKnownBotName(name) ? name.c_str() : "?");
			WriteBotLog(message);
			continue;
		}

		if (s->m_phase != BotSession::PHASE_IN_GAME)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd stop: %s not in game (phase %s)",
				s->m_charName.c_str(), PhaseName(s->m_phase));
			WriteBotLog(message);
			continue;
		}

		MoveOutcome outcome = ActionExecutor::StopMove(s, now);

		char message[224];
		if (outcome.kind == MoveOutcome::ARRIVED)
			snprintf(message, sizeof(message),
				"BotManager: cmd stop: %s stopped at (%.1f, %.1f)",
				s->m_charName.c_str(), s->m_pUser->GetX(), s->m_pUser->GetZ());
		else
			snprintf(message, sizeof(message),
				"BotManager: cmd stop: %s not moving", s->m_charName.c_str());
		WriteBotLog(message);
	}
}

void BotManager::CommandAttack(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	if (words.size() == 2 && _stricmp(words[1].c_str(), "off") == 0)
	{
		if (_stricmp(words[0].c_str(), "all") == 0)
		{
			uint32 stopped = 0, notAttacking = 0;
			for (size_t i = 0; i < m_sessions.size(); i++)
			{
				BotSession * s = m_sessions[i];
				if (s->m_phase != BotSession::PHASE_IN_GAME)
					continue;

				char message[224];
				if (s->m_attackActive)
				{
					snprintf(message, sizeof(message),
						"BotManager: cmd attack: %s stopped after %u hit(s) sent",
						s->m_charName.c_str(), (unsigned)s->m_attackSent);
					ActionExecutor::EndAttack(s);
					stopped++;
				}
				else
				{
					snprintf(message, sizeof(message),
						"BotManager: cmd attack: %s not attacking", s->m_charName.c_str());
					notAttacking++;
				}
				WriteBotLog(message);
			}

			char summary[128];
			snprintf(summary, sizeof(summary),
				"BotManager: cmd attack all: %u stopped, %u not attacking",
				(unsigned)stopped, (unsigned)notAttacking);
			WriteBotLog(summary);
			return;
		}

		BotSession * s = FindSession(words[0].c_str());
		if (s == nullptr)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd attack: unknown or not spawned bot '%s'",
				IsKnownBotName(words[0]) ? words[0].c_str() : "?");
			WriteBotLog(message);
			return;
		}

		if (s->m_phase != BotSession::PHASE_IN_GAME)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd attack: %s not in game (phase %s)",
				s->m_charName.c_str(), PhaseName(s->m_phase));
			WriteBotLog(message);
			return;
		}

		char message[224];
		if (s->m_attackActive)
		{
			snprintf(message, sizeof(message),
				"BotManager: cmd attack: %s stopped after %u hit(s) sent",
				s->m_charName.c_str(), (unsigned)s->m_attackSent);
			ActionExecutor::EndAttack(s);
		}
		else
		{
			snprintf(message, sizeof(message),
				"BotManager: cmd attack: %s not attacking", s->m_charName.c_str());
		}
		WriteBotLog(message);
		return;
	}

	if (words.size() < 2 || words.size() > 3)
	{
		WriteBotLog("BotManager: cmd attack: usage: attack <bot> <target bot> [count] | attack <bot|all> off");
		return;
	}

	const std::string & name = words[0];
	const std::string & targetName = words[1];

	BotSession * s = FindSession(name.c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd attack: unknown or not spawned bot '%s'",
			IsKnownBotName(name) ? name.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd attack: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	BotSession * target = FindSession(targetName.c_str());
	if (target == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd attack: unknown or not spawned bot '%s'",
			IsKnownBotName(targetName) ? targetName.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (target->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd attack: target %s not in game (phase %s)",
			target->m_charName.c_str(), PhaseName(target->m_phase));
		WriteBotLog(message);
		return;
	}

	if (target == s)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd attack: %s refused (bad_target)", s->m_charName.c_str());
		WriteBotLog(message);
		return;
	}

	long count = 1;
	if (words.size() == 3)
	{
		if (!ParseIntStrict(words[2], count) || count < 1 || count > 100)
		{
			WriteBotLog("BotManager: cmd attack: usage: attack <bot> <target bot> [count] | attack <bot|all> off");
			return;
		}
	}

	AttackOutcome outcome = ActionExecutor::BeginAttack(s, target->m_charName, (uint32)count, now);

	char message[256];
	if (outcome.kind == AttackOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd attack: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd attack: %s attacking %s (%u hit(s))",
			s->m_charName.c_str(), target->m_charName.c_str(), (unsigned)count);
	WriteBotLog(message);
}

void BotManager::CommandCast(const std::string & args)
{
	static const char * kUsage =
		"BotManager: cmd cast: usage: cast <bot> <skill id> <target bot|self> [cycles] | cast <bot|all> off";

	std::vector<std::string> words;
	SplitWords(args, words);

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	if (words.size() == 2 && _stricmp(words[1].c_str(), "off") == 0)
	{
		if (_stricmp(words[0].c_str(), "all") == 0)
		{
			uint32 stopped = 0, notCasting = 0, refused = 0;
			for (size_t i = 0; i < m_sessions.size(); i++)
			{
				BotSession * s = m_sessions[i];
				if (s->m_phase != BotSession::PHASE_IN_GAME)
					continue;

				CastOutcome cancel = ActionExecutor::CancelCast(s, "cmd", now);
				char message[224];
				if (cancel.kind == CastOutcome::SENT)
				{
					snprintf(message, sizeof(message),
						"BotManager: cmd cast: %s cast cancelled after %u packet(s) sent",
						s->m_charName.c_str(), (unsigned)s->m_castPackets);
					stopped++;
				}
				else if (cancel.kind == CastOutcome::FAILED)
				{
					snprintf(message, sizeof(message),
						"BotManager: cmd cast: %s cancel failed (%s)",
						s->m_charName.c_str(), cancel.reason);
					stopped++;
				}
				else if (cancel.kind == CastOutcome::REFUSED)
				{
					snprintf(message, sizeof(message),
						"BotManager: cmd cast: %s cancel refused (%s); still casting",
						s->m_charName.c_str(), cancel.reason);
					refused++;
				}
				else if (std::strcmp(cancel.reason, "dropped") == 0)
				{
					snprintf(message, sizeof(message),
						"BotManager: cmd cast: %s stopped after %u packet(s) sent",
						s->m_charName.c_str(), (unsigned)s->m_castPackets);
					stopped++;
				}
				else
				{
					snprintf(message, sizeof(message),
						"BotManager: cmd cast: %s not casting", s->m_charName.c_str());
					notCasting++;
				}
				WriteBotLog(message);
			}

			char summary[128];
			snprintf(summary, sizeof(summary),
				"BotManager: cmd cast all: %u stopped, %u not casting, %u refused",
				(unsigned)stopped, (unsigned)notCasting, (unsigned)refused);
			WriteBotLog(summary);
			return;
		}

		BotSession * s = FindSession(words[0].c_str());
		if (s == nullptr)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd cast: unknown or not spawned bot '%s'",
				IsKnownBotName(words[0]) ? words[0].c_str() : "?");
			WriteBotLog(message);
			return;
		}

		if (s->m_phase != BotSession::PHASE_IN_GAME)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd cast: %s not in game (phase %s)",
				s->m_charName.c_str(), PhaseName(s->m_phase));
			WriteBotLog(message);
			return;
		}

		CastOutcome cancel = ActionExecutor::CancelCast(s, "cmd", now);
		char message[224];
		if (cancel.kind == CastOutcome::SENT)
			snprintf(message, sizeof(message),
				"BotManager: cmd cast: %s cast cancelled after %u packet(s) sent",
				s->m_charName.c_str(), (unsigned)s->m_castPackets);
		else if (cancel.kind == CastOutcome::FAILED)
			snprintf(message, sizeof(message),
				"BotManager: cmd cast: %s cancel failed (%s)",
				s->m_charName.c_str(), cancel.reason);
		else if (cancel.kind == CastOutcome::REFUSED)
			snprintf(message, sizeof(message),
				"BotManager: cmd cast: %s cancel refused (%s); still casting",
				s->m_charName.c_str(), cancel.reason);
		else if (std::strcmp(cancel.reason, "dropped") == 0)
			snprintf(message, sizeof(message),
				"BotManager: cmd cast: %s stopped after %u packet(s) sent",
				s->m_charName.c_str(), (unsigned)s->m_castPackets);
		else
			snprintf(message, sizeof(message),
				"BotManager: cmd cast: %s not casting", s->m_charName.c_str());
		WriteBotLog(message);
		return;
	}

	if (words.size() < 3 || words.size() > 4)
	{
		WriteBotLog(kUsage);
		return;
	}

	BotSession * s = FindSession(words[0].c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd cast: unknown or not spawned bot '%s'",
			IsKnownBotName(words[0]) ? words[0].c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd cast: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	long skillId = 0;
	if (!ParseIntStrict(words[1], skillId) || skillId < 1 || skillId > 2147483647L)
	{
		WriteBotLog(kUsage);
		return;
	}

	std::string targetName;
	if (_stricmp(words[2].c_str(), "self") != 0)
	{
		BotSession * target = FindSession(words[2].c_str());
		if (target == nullptr)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd cast: unknown or not spawned bot '%s'",
				IsKnownBotName(words[2]) ? words[2].c_str() : "?");
			WriteBotLog(message);
			return;
		}

		if (target->m_phase != BotSession::PHASE_IN_GAME)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd cast: target %s not in game (phase %s)",
				target->m_charName.c_str(), PhaseName(target->m_phase));
			WriteBotLog(message);
			return;
		}

		if (target == s)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd cast: %s refused (bad_target)", s->m_charName.c_str());
			WriteBotLog(message);
			return;
		}

		targetName = target->m_charName;
	}

	long cycles = 1;
	if (words.size() == 4)
	{
		if (!ParseIntStrict(words[3], cycles) || cycles < 1 || cycles > 20)
		{
			WriteBotLog(kUsage);
			return;
		}
	}

	CastOutcome outcome = ActionExecutor::BeginCast(s, (uint32)skillId, targetName, (uint32)cycles, now);

	char message[256];
	if (outcome.kind == CastOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd cast: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd cast: %s casting %ld on %s (%ld cycle(s))",
			s->m_charName.c_str(), skillId, targetName.empty() ? "self" : targetName.c_str(), cycles);
	WriteBotLog(message);
}

void BotManager::CommandPot(const std::string & args)
{
	static const char * kUsage =
		"BotManager: cmd pot: usage: pot <bot> <item id> [count] | pot <bot|all> off";

	std::vector<std::string> words;
	SplitWords(args, words);

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	if (words.size() == 2 && _stricmp(words[1].c_str(), "off") == 0)
	{
		if (_stricmp(words[0].c_str(), "all") == 0)
		{
			uint32 stopped = 0, notPotting = 0;
			for (size_t i = 0; i < m_sessions.size(); i++)
			{
				BotSession * s = m_sessions[i];
				if (s->m_phase != BotSession::PHASE_IN_GAME)
					continue;

				char message[224];
				if (s->m_potActive)
				{
					snprintf(message, sizeof(message),
						"BotManager: cmd pot: %s stopped after %u use(s)",
						s->m_charName.c_str(), (unsigned)s->m_potSent);
					ActionExecutor::EndPotion(s);
					stopped++;
				}
				else
				{
					snprintf(message, sizeof(message),
						"BotManager: cmd pot: %s not potting", s->m_charName.c_str());
					notPotting++;
				}
				WriteBotLog(message);
			}

			char summary[128];
			snprintf(summary, sizeof(summary),
				"BotManager: cmd pot all: %u stopped, %u not potting",
				(unsigned)stopped, (unsigned)notPotting);
			WriteBotLog(summary);
			return;
		}

		BotSession * s = FindSession(words[0].c_str());
		if (s == nullptr)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd pot: unknown or not spawned bot '%s'",
				IsKnownBotName(words[0]) ? words[0].c_str() : "?");
			WriteBotLog(message);
			return;
		}

		if (s->m_phase != BotSession::PHASE_IN_GAME)
		{
			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: cmd pot: %s not in game (phase %s)",
				s->m_charName.c_str(), PhaseName(s->m_phase));
			WriteBotLog(message);
			return;
		}

		char message[224];
		if (s->m_potActive)
		{
			snprintf(message, sizeof(message),
				"BotManager: cmd pot: %s stopped after %u use(s)",
				s->m_charName.c_str(), (unsigned)s->m_potSent);
			ActionExecutor::EndPotion(s);
		}
		else
		{
			snprintf(message, sizeof(message),
				"BotManager: cmd pot: %s not potting", s->m_charName.c_str());
		}
		WriteBotLog(message);
		return;
	}

	if (words.size() < 2 || words.size() > 3)
	{
		WriteBotLog(kUsage);
		return;
	}

	BotSession * s = FindSession(words[0].c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pot: unknown or not spawned bot '%s'",
			IsKnownBotName(words[0]) ? words[0].c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pot: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	long itemId = 0;
	if (!ParseIntStrict(words[1], itemId) || itemId < 1 || itemId > 2147483647L)
	{
		WriteBotLog(kUsage);
		return;
	}

	long count = 1;
	if (words.size() == 3)
	{
		if (!ParseIntStrict(words[2], count) || count < 1 || count > 20)
		{
			WriteBotLog(kUsage);
			return;
		}
	}

	PotionOutcome outcome = ActionExecutor::BeginPotion(s, (uint32)itemId, (uint32)count, now);

	char message[256];
	if (outcome.kind == PotionOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd pot: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd pot: %s using %ld (%ld use(s))",
			s->m_charName.c_str(), itemId, count);
	WriteBotLog(message);
}

void BotManager::CommandStance(const std::string & args, bool sit)
{
	const char * verb = sit ? "sit" : "stand";

	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() != 1)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: usage: %s <bot>", verb, verb);
		WriteBotLog(message);
		return;
	}

	BotSession * s = FindSession(words[0].c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: unknown or not spawned bot '%s'",
			verb, IsKnownBotName(words[0]) ? words[0].c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: %s not in game (phase %s)",
			verb, s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	StanceOutcome outcome = ActionExecutor::SetStance(s, sit, std::chrono::steady_clock::now());

	char message[256];
	if (outcome.kind == StanceOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: %s refused (%s)", verb, s->m_charName.c_str(), outcome.reason);
	else if (outcome.kind == StanceOutcome::SENT)
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: %s %s", verb, s->m_charName.c_str(), sit ? "sat down" : "stood up");
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: %s failed (%s)", verb, s->m_charName.c_str(), outcome.reason);
	WriteBotLog(message);
}

void BotManager::CommandTarget(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	if (words.size() != 2)
	{
		WriteBotLog("BotManager: cmd target: usage: target <bot> <target bot>");
		return;
	}

	const std::string & name = words[0];
	const std::string & targetName = words[1];

	BotSession * s = FindSession(name.c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd target: unknown or not spawned bot '%s'",
			IsKnownBotName(name) ? name.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd target: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	BotSession * target = FindSession(targetName.c_str());
	if (target == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd target: unknown or not spawned bot '%s'",
			IsKnownBotName(targetName) ? targetName.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (target->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd target: target %s not in game (phase %s)",
			target->m_charName.c_str(), PhaseName(target->m_phase));
		WriteBotLog(message);
		return;
	}

	// Test driver: the target view comes straight from the target bot's session.
	// The Perception slice replaces this source, not TargetHpTarget.
	TargetHpTarget tv = { (int16)target->m_pUser->GetSocketID(), target->m_pUser->GetX(), target->m_pUser->GetZ() };
	TargetHpOutcome outcome = ActionExecutor::RequestTargetHp(s, tv, now);

	char message[256];
	if (outcome.kind == TargetHpOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd target: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else if (outcome.kind == TargetHpOutcome::SENT)
		snprintf(message, sizeof(message),
			"BotManager: cmd target: %s observed %s hp %d/%d",
			s->m_charName.c_str(), target->m_charName.c_str(), (int)outcome.hp, (int)outcome.maxHp);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd target: %s failed (%s)", s->m_charName.c_str(), outcome.reason);
	WriteBotLog(message);
}

void BotManager::CommandRegene(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() != 1)
	{
		WriteBotLog("BotManager: cmd regene: usage: regene <bot>");
		return;
	}

	BotSession * s = FindSession(words[0].c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd regene: unknown or not spawned bot '%s'",
			IsKnownBotName(words[0]) ? words[0].c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd regene: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	RegeneOutcome outcome = ActionExecutor::RequestRegene(s, std::chrono::steady_clock::now());

	char message[256];
	if (outcome.kind == RegeneOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd regene: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else if (outcome.kind == RegeneOutcome::SENT)
		snprintf(message, sizeof(message),
			"BotManager: cmd regene: %s respawned at (%.1f, %.1f)",
			s->m_charName.c_str(), outcome.x, outcome.z);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd regene: %s failed (%s)", s->m_charName.c_str(), outcome.reason);
	WriteBotLog(message);
}

void BotManager::CommandPartyInvite(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	if (words.size() != 2)
	{
		WriteBotLog("BotManager: cmd pinvite: usage: pinvite <bot> <target bot>");
		return;
	}

	const std::string & name = words[0];
	const std::string & targetName = words[1];

	BotSession * s = FindSession(name.c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pinvite: unknown or not spawned bot '%s'",
			IsKnownBotName(name) ? name.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pinvite: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	BotSession * target = FindSession(targetName.c_str());
	if (target == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pinvite: unknown or not spawned bot '%s'",
			IsKnownBotName(targetName) ? targetName.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (target->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pinvite: target %s not in game (phase %s)",
			target->m_charName.c_str(), PhaseName(target->m_phase));
		WriteBotLog(message);
		return;
	}

	// Test driver: the target comes straight from the target bot's session; the Perception slice replaces this
	// source, not PartyInviteTarget.
	PartyInviteTarget tv = { (int16)target->m_pUser->GetSocketID(), target->m_charName,
		target->m_pUser->GetX(), target->m_pUser->GetZ() };
	PartyOutcome outcome = ActionExecutor::RequestPartyInvite(s, tv, now);

	char message[256];
	if (outcome.kind == PartyOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd pinvite: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else if (outcome.kind == PartyOutcome::SENT)
		snprintf(message, sizeof(message),
			"BotManager: cmd pinvite: %s invited %s (%s)",
			s->m_charName.c_str(), target->m_charName.c_str(), outcome.reason);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd pinvite: %s failed (%s)", s->m_charName.c_str(), outcome.reason);
	WriteBotLog(message);
}

void BotManager::CommandPartyAccept(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() != 1)
	{
		WriteBotLog("BotManager: cmd paccept: usage: paccept <bot>");
		return;
	}

	BotSession * s = FindSession(words[0].c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd paccept: unknown or not spawned bot '%s'",
			IsKnownBotName(words[0]) ? words[0].c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd paccept: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	PartyOutcome outcome = ActionExecutor::RequestPartyAccept(s, std::chrono::steady_clock::now());

	char message[256];
	if (outcome.kind == PartyOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd paccept: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else if (outcome.kind == PartyOutcome::SENT)
		snprintf(message, sizeof(message),
			"BotManager: cmd paccept: %s joined party of #%d", s->m_charName.c_str(), outcome.peerId);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd paccept: %s failed (%s)", s->m_charName.c_str(), outcome.reason);
	WriteBotLog(message);
}

void BotManager::CommandPartyDecline(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() != 1)
	{
		WriteBotLog("BotManager: cmd pdecline: usage: pdecline <bot>");
		return;
	}

	BotSession * s = FindSession(words[0].c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pdecline: unknown or not spawned bot '%s'",
			IsKnownBotName(words[0]) ? words[0].c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pdecline: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	PartyOutcome outcome = ActionExecutor::RequestPartyDecline(s, std::chrono::steady_clock::now());

	char message[256];
	if (outcome.kind == PartyOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd pdecline: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else if (outcome.kind == PartyOutcome::SENT)
		snprintf(message, sizeof(message),
			"BotManager: cmd pdecline: %s declined invitation of #%d", s->m_charName.c_str(), outcome.peerId);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd pdecline: %s failed (%s)", s->m_charName.c_str(), outcome.reason);
	WriteBotLog(message);
}

void BotManager::CommandPartyLeave(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() != 1)
	{
		WriteBotLog("BotManager: cmd pleave: usage: pleave <bot>");
		return;
	}

	BotSession * s = FindSession(words[0].c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pleave: unknown or not spawned bot '%s'",
			IsKnownBotName(words[0]) ? words[0].c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pleave: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	PartyOutcome outcome = ActionExecutor::RequestPartyLeave(s, std::chrono::steady_clock::now());

	char message[256];
	if (outcome.kind == PartyOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd pleave: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else if (outcome.kind == PartyOutcome::SENT)
		snprintf(message, sizeof(message),
			"BotManager: cmd pleave: %s %s", s->m_charName.c_str(), outcome.reason);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd pleave: %s failed (%s)", s->m_charName.c_str(), outcome.reason);
	WriteBotLog(message);
}

// Test driver: the party panel lists who is in the leader's party. Until the Perception slice, membership is read
// from the two bot sessions (guard input only, never a result). The server counts an invitee as a party member
// before it accepts (PartyHandler.cpp:154-155, KI-014), so a pending invitation record disqualifies the target.
static bool IsSamePartyMember(CUser * leader, BotSession * target)
{
	return leader->isInParty() && target->m_pUser->isInParty()
		&& leader->GetPartyID() == target->m_pUser->GetPartyID()
		&& (target->m_partyInviteEcho.load() & (1ull << 63)) == 0;
}

void BotManager::CommandPartyManage(const std::string & args, bool kick)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	const char * verb = kick ? "pkick" : "ppromote";
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	if (words.size() != 2)
	{
		char message[128];
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: usage: %s <bot> <target bot>", verb, verb);
		WriteBotLog(message);
		return;
	}

	const std::string & name = words[0];
	const std::string & targetName = words[1];

	BotSession * s = FindSession(name.c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: unknown or not spawned bot '%s'",
			verb, IsKnownBotName(name) ? name.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: %s not in game (phase %s)",
			verb, s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	BotSession * target = FindSession(targetName.c_str());
	if (target == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: unknown or not spawned bot '%s'",
			verb, IsKnownBotName(targetName) ? targetName.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (target->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: target %s not in game (phase %s)",
			verb, target->m_charName.c_str(), PhaseName(target->m_phase));
		WriteBotLog(message);
		return;
	}

	// Test driver: the target and its membership come straight from the two bot sessions; the Perception slice
	// replaces this source, not PartyMemberTarget.
	PartyMemberTarget tv = { (int16)target->m_pUser->GetSocketID(),
		IsSamePartyMember(s->m_pUser, target) };
	PartyOutcome outcome = kick
		? ActionExecutor::RequestPartyKick(s, tv, now)
		: ActionExecutor::RequestPartyPromote(s, tv, now);

	char message[256];
	if (outcome.kind == PartyOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: %s refused (%s)", verb, s->m_charName.c_str(), outcome.reason);
	else if (outcome.kind == PartyOutcome::SENT)
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: %s %s %s",
			verb, s->m_charName.c_str(), outcome.reason, target->m_charName.c_str());
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd %s: %s failed (%s)", verb, s->m_charName.c_str(), outcome.reason);
	WriteBotLog(message);
}

void BotManager::CommandPartyChat(const std::string & args)
{
	size_t space = args.find(' ');
	std::string name = space == std::string::npos ? args : args.substr(0, space);
	std::string text = space == std::string::npos ? "" : Trim(args.substr(space + 1));

	if (name.empty() || text.empty())
	{
		WriteBotLog("BotManager: cmd pchat: usage: pchat <bot> <text>");
		return;
	}

	BotSession * s = FindSession(name.c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pchat: unknown or not spawned bot '%s'",
			IsKnownBotName(name) ? name.c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd pchat: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	ChatOutcome outcome = ActionExecutor::RequestChatParty(s, text, std::chrono::steady_clock::now());

	char message[256];
	if (outcome.kind == ChatOutcome::REFUSED)
		snprintf(message, sizeof(message),
			"BotManager: cmd pchat: %s refused (%s)", s->m_charName.c_str(), outcome.reason);
	else if (outcome.kind == ChatOutcome::SENT)
		snprintf(message, sizeof(message),
			"BotManager: cmd pchat: %s sent (%d chars)", s->m_charName.c_str(), outcome.length);
	else
		snprintf(message, sizeof(message),
			"BotManager: cmd pchat: %s failed (%s)", s->m_charName.c_str(), outcome.reason);
	WriteBotLog(message);
}

void BotManager::CommandSee(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() != 1)
	{
		WriteBotLog("BotManager: cmd see: usage: see <bot>");
		return;
	}

	BotSession * s = FindSession(words[0].c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd see: unknown or not spawned bot '%s'",
			IsKnownBotName(words[0]) ? words[0].c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd see: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	// Copy the table and the counter under the lock, then format with the lock released.
	BotCore::ObsTable copy;
	uint32 unresolved = 0;
	uint32 pending = 0;
	uint32 inoutIn = 0, inoutOut = 0, inoutParseFail = 0;
	uint32 reqUserInRecv = 0, reqUserInUnits = 0, reqUserInParseStop = 0;
	uint32 regionRecv = 0, regionIdsLast = 0, regionDroppedTotal = 0, moveUnknown = 0;
	uint16 droppedIds[8];
	int droppedCount = 0;
	{
		std::lock_guard<std::mutex> lock(s->m_obsLock);
		copy = s->m_obs;
		unresolved = s->m_obsUnresolved.load();
		pending = (uint32)s->m_obsPending.Count();
		inoutIn = s->m_inoutIn.load();
		inoutOut = s->m_inoutOut.load();
		inoutParseFail = s->m_inoutParseFail.load();
		reqUserInRecv = s->m_reqUserInRecv.load();
		reqUserInUnits = s->m_reqUserInUnits.load();
		reqUserInParseStop = s->m_reqUserInParseStop.load();
		regionRecv = s->m_regionRecv.load();
		regionIdsLast = s->m_regionIdsLast.load();
		regionDroppedTotal = s->m_regionDroppedTotal.load();
		moveUnknown = s->m_moveUnknown.load();
		droppedCount = s->m_regionDroppedLastCount;
		for (int i = 0; i < droppedCount && i < 8; i++)
			droppedIds[i] = s->m_regionDroppedLastIds[i];
	}

	// The only read of the bot's own session: its CUser, which the contract allows.
	CUser * me = s->m_pUser;
	uint16 selfSid = me->GetID();
	uint8 myNation = me->GetNation();
	float myX = me->GetX();
	float myZ = me->GetZ();

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();

	int total = 0, enemies = 0, allies = 0;
	for (int i = 0; i < copy.Count(); i++)
	{
		const BotCore::UnitObs & u = copy.At(i);
		if (u.sid == selfSid)
			continue;
		total++;
		if (u.nation != myNation)
			enemies++;
		else
			allies++;
	}

	char message[320];
	snprintf(message, sizeof(message),
		"BotManager: cmd see: %s sees %d unit(s) (enemies %d, allies %d, dropped %u, unresolved %u)",
		s->m_charName.c_str(), total, enemies, allies, (unsigned)copy.Overflow(), (unsigned)unresolved);
	WriteBotLog(message);
	char note[256];
	snprintf(note, sizeof(note),
		"BotManager: cmd see:   (unresolved counts the last region id list incl. the bot itself; userin requests %u, units received %u, pending %u)",
		(unsigned)s->m_userInRequests, (unsigned)s->m_userInUnits, (unsigned)pending);
	WriteBotLog(note);
	char diag[320];
	snprintf(diag, sizeof(diag),
		"BotManager: cmd see:   diag: inout in %u out %u fail %u | userin recv %u units %u stop %u sent %u | region recv %u ids %u dropped %u | move_unknown %u",
		(unsigned)inoutIn, (unsigned)inoutOut, (unsigned)inoutParseFail,
		(unsigned)reqUserInRecv, (unsigned)reqUserInUnits, (unsigned)reqUserInParseStop,
		(unsigned)s->m_userInRequests, (unsigned)regionRecv, (unsigned)regionIdsLast,
		(unsigned)regionDroppedTotal, (unsigned)moveUnknown);
	WriteBotLog(diag);
	if (droppedCount > 0)
	{
		char diagIds[256];
		int len = snprintf(diagIds, sizeof(diagIds),
			"BotManager: cmd see:   diag: last region dropped ids (%d):", droppedCount);
		for (int i = 0; i < droppedCount && i < 8 && len > 0 && (size_t)len < sizeof(diagIds); i++)
			len += snprintf(diagIds + len, sizeof(diagIds) - (size_t)len, " %u", (unsigned)droppedIds[i]);
		WriteBotLog(diagIds);
	}

	for (int i = 0; i < copy.Count(); i++)
	{
		const BotCore::UnitObs & u = copy.At(i);
		if (u.sid == selfSid)
			continue;

		bool enemy = u.nation != myNation;
		float ux = u.x10 / 10.0f;
		float uz = u.z10 / 10.0f;
		float dist = (float)sqrt((ux - myX) * (ux - myX) + (uz - myZ) * (uz - myZ));
		uint64 age = nowMs > u.lastSeenMs ? nowMs - u.lastSeenMs : 0;
		uint64 posAge = nowMs > u.lastMoveMs ? nowMs - u.lastMoveMs : 0;
		uint32 posAgeMs = posAge > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32)posAge;
		bool moving = (u.lastSpeed > 0);
		uint8 posState = BotCore::ClassifyPos(moving, posAgeMs);
		float vx = 0.0f, vz = 0.0f;
		BotCore::EstimateVelocity(u, nowMs, vx, vz);
		char speedText[8];
		if (u.lastSpeed < 0)
			snprintf(speedText, sizeof(speedText), "?");
		else
			snprintf(speedText, sizeof(speedText), "%d", (int)u.lastSpeed);

		snprintf(message, sizeof(message),
			"BotManager: cmd see:   sid=%u %s %s nation=%u class=%u lvl=%u pos=(%.1f, %.1f) dist=%.1f %s age=%llums name=%s pos_age=%ums speed=%s v=(%.2f,%.2f) pos=%s",
			(unsigned)u.sid, u.name, enemy ? "enemy" : "ally", (unsigned)u.nation,
			(unsigned)u.cls, (unsigned)u.level, ux, uz, dist,
			u.resHpType == BotCore::kObsUserDead ? "dead" : "alive", (unsigned long long)age,
			u.name, (unsigned)posAgeMs, speedText, vx, vz,
			posState == BotCore::POS_STALE ? "stale" : (posState == BotCore::POS_LOST ? "lost" : "fresh"));
		WriteBotLog(message);
	}
}

void BotManager::CommandNpcs(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() != 1)
	{
		WriteBotLog("BotManager: cmd npcs: usage: npcs <bot>");
		return;
	}

	BotSession * s = FindSession(words[0].c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd npcs: unknown or not spawned bot '%s'",
			IsKnownBotName(words[0]) ? words[0].c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd npcs: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	// Copy the table and the counter under the lock, then format with the lock released.
	BotCore::NpcTable copy;
	uint32 unresolved = 0;
	uint32 pending = 0;
	{
		std::lock_guard<std::mutex> lock(s->m_obsLock);
		copy = s->m_npcs;
		unresolved = s->m_npcUnresolved.load();
		pending = (uint32)s->m_npcPending.Count();
	}

	// The only read of the bot's own session: its CUser, which the contract allows.
	CUser * me = s->m_pUser;
	float myX = me->GetX();
	float myZ = me->GetZ();

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();

	int dead = 0;
	for (int i = 0; i < copy.Count(); i++)
	{
		if (copy.At(i).dead)
			dead++;
	}

	char message[320];
	snprintf(message, sizeof(message),
		"BotManager: cmd npcs: %s sees %d npc(s) (dead %d, dropped %u, unresolved %u)",
		s->m_charName.c_str(), copy.Count(), dead, (unsigned)copy.Overflow(), (unsigned)unresolved);
	WriteBotLog(message);
	char note[256];
	snprintf(note, sizeof(note),
		"BotManager: cmd npcs:   (unresolved counts the ids of the last WIZ_NPC_REGION list that the table did not know; npcin requests %u, npcs received %u, pending %u)",
		(unsigned)s->m_npcInRequests, (unsigned)s->m_npcInUnits, (unsigned)pending);
	WriteBotLog(note);

	for (int i = 0; i < copy.Count(); i++)
	{
		const BotCore::NpcObs & n = copy.At(i);
		float nx = n.x10 / 10.0f;
		float nz = n.z10 / 10.0f;
		float dist = (float)sqrt((nx - myX) * (nx - myX) + (nz - myZ) * (nz - myZ));
		uint64 age = nowMs > n.lastSeenMs ? nowMs - n.lastSeenMs : 0;

		snprintf(message, sizeof(message),
			"BotManager: cmd npcs:   id=%u proto=%u type=%u %s nation=%u lvl=%u pos=(%.1f, %.1f) dist=%.1f %s gate=%s obj=%u age=%llums",
			(unsigned)n.id, (unsigned)n.protoId, (unsigned)n.type, n.name, (unsigned)n.nation,
			(unsigned)n.level, nx, nz, dist, n.dead ? "dead" : "alive",
			n.gateOpen ? "open" : "closed", (unsigned)n.objectType, (unsigned long long)age);
		WriteBotLog(message);
	}
}

// Fills the own-state extras of the snapshot (docs/14 5.1/5.2: the bot's own bag, buffs and timers are allowed).
// IOCP thread only. Reads nothing that belongs to another player.
static void FillSelfExtras(BotSession * s, CUser * me, std::chrono::steady_clock::time_point now,
	BotCore::SelfState & self)
{
	for (uint8 i = INVENTORY_INVENT; i < INVENTORY_INVENT + HAVE_MAX; i++)
	{
		_ITEM_DATA * item = me->GetItem(i);
		if (item == nullptr || item->nNum == 0 || item->sCount == 0)
			continue;

		uint8 kind = ActionExecutor::PotKindOf(me, item->nNum);
		if (kind == 1)
			self.hpPotStock += item->sCount;
		else if (kind == 2)
			self.mpPotStock += item->sCount;
	}

	BotCore::PotionCheck c;
	memset(&c, 0, sizeof(c));
	c.hasLast = s->m_potHasLast;
	if (c.hasLast)
		c.sinceLastMs = (uint32)std::chrono::duration_cast<std::chrono::milliseconds>(now - s->m_potLast).count();
	self.potWaitMs = BotCore::PotionWaitMs(c);

	if (s->m_castAnyHas)
	{
		uint64 since = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(now - s->m_castAnyLast).count();
		self.castGapWaitMs = BotCore::SnapRemainingMs(BotCore::kCastGapMs, since);
	}

	for (const auto & kv : s->m_castSkillLast)
	{
		_MAGIC_TABLE * m = g_pMain->m_MagictableArray.GetData(kv.first);
		if (m == nullptr)
			continue;

		uint64 since = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(now - kv.second).count();
		BotCore::SelfAddCooldown(self, kv.first, BotCore::SnapRemainingMs(BotCore::CastRecastMs(m->sReCastTime), since));
	}

	{
		std::lock_guard<std::recursive_mutex> lock(me->m_buffLock);
		for (const auto & kv : me->m_buffMap)
		{
			BotCore::SelfAddBuff(self, kv.second.m_nSkillID, kv.second.m_bBuffType, kv.second.m_bIsBuff,
				BotCore::SnapRemainingSec((int64_t)kv.second.m_tEndTime, (int64_t)UNIXTIME));
		}
	}

	self.inParty = me->isInParty();
	self.partyLeader = me->isPartyLeader();
}

void BotManager::CommandSnap(const std::string & args)
{
	std::vector<std::string> words;
	SplitWords(args, words);

	if (words.size() != 1 && words.size() != 2)
	{
		WriteBotLog("BotManager: cmd snap: usage: snap <bot> [events|status]");
		return;
	}

	bool wantEvents = (words.size() == 2 && words[1] == "events");
	bool wantStatus = (words.size() == 2 && words[1] == "status");
	if (words.size() == 2 && !wantEvents && !wantStatus)
	{
		WriteBotLog("BotManager: cmd snap: usage: snap <bot> [events|status]");
		return;
	}

	BotSession * s = FindSession(words[0].c_str());
	if (s == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd snap: unknown or not spawned bot '%s'",
			IsKnownBotName(words[0]) ? words[0].c_str() : "?");
		WriteBotLog(message);
		return;
	}

	if (s->m_phase != BotSession::PHASE_IN_GAME)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"BotManager: cmd snap: %s not in game (phase %s)",
			s->m_charName.c_str(), PhaseName(s->m_phase));
		WriteBotLog(message);
		return;
	}

	// Copy the tables under the lock (only these assignments), then build and format with the lock released.
	BotCore::ObsTable obsCopy;
	BotCore::NpcTable npcCopy;
	BotCore::TeamTable teamCopy;
	BotCore::HpTable hpCopy;
	BotCore::SkillEventRing eventCopy;
	BotCore::ObservedStatusTable statusCopy;
	BotCore::HealObsRing healCopy;
	{
		std::lock_guard<std::mutex> lock(s->m_obsLock);
		obsCopy = s->m_obs;
		npcCopy = s->m_npcs;
		teamCopy = s->m_team;
		hpCopy = s->m_hp;
		eventCopy = s->m_skillEvents;
		// ADR-0017 Ek F4-60: only the copy happens under the lock; the dump below runs with the lock released.
		statusCopy = s->m_status;
		healCopy = s->m_healObs;
	}

	// The only read of the bot's own session: its CUser, which the contract allows.
	CUser * me = s->m_pUser;
	BotCore::SelfState self;
	memset(&self, 0, sizeof(self));
	self.sid = me->GetID();
	self.nation = me->GetNation();
	self.cls = me->GetClass();
	self.level = me->GetLevel();
	self.x = me->GetX();
	self.z = me->GetZ();
	self.hp = me->GetHealth();
	self.maxHp = me->GetMaxHealth();
	self.mp = me->GetMana();
	self.maxMp = me->GetMaxMana();
	self.dead = me->isDead();
	self.sitting = (me->m_bResHpType == USER_SITDOWN);
	FillSelfExtras(s, me, std::chrono::steady_clock::now(), self);

	uint64 nowMs = (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();

	BotCore::PerceptionSnapshot snap;
	BotCore::BuildSnapshot(self, obsCopy, npcCopy, nowMs, snap);
	BotCore::AttachHp(snap, hpCopy, nowMs);
	BotCore::BuildTeam(self, teamCopy, obsCopy, nowMs, snap.team);

	char message[320];

	snprintf(message, sizeof(message),
		"BotManager: cmd snap: %s t=%llu self sid=%u nation=%u class=%u lvl=%u pos=(%.1f, %.1f) hp=%d/%d mp=%d/%d %s %s",
		s->m_charName.c_str(), (unsigned long long)snap.tMs, (unsigned)snap.self.sid, (unsigned)snap.self.nation,
		(unsigned)snap.self.cls, (unsigned)snap.self.level, snap.self.x, snap.self.z,
		snap.self.hp, snap.self.maxHp, snap.self.mp, snap.self.maxMp,
		snap.self.dead ? "dead" : "alive", snap.self.sitting ? "sitting" : "standing");
	WriteBotLog(message);

	snprintf(message, sizeof(message),
		"BotManager: cmd snap:   stock hp_pot=%u mp_pot=%u wait pot=%ums cast_gap=%ums",
		(unsigned)snap.self.hpPotStock, (unsigned)snap.self.mpPotStock,
		(unsigned)snap.self.potWaitMs, (unsigned)snap.self.castGapWaitMs);
	WriteBotLog(message);

	snprintf(message, sizeof(message),
		"BotManager: cmd snap:   buffs %d (total %d), cooldowns %d (total %d)",
		snap.self.buffCount, snap.self.buffTotal, snap.self.cooldownCount, snap.self.cooldownTotal);
	WriteBotLog(message);

	const int kPrintMaxSelf = 10;

	for (int i = 0; i < snap.self.buffCount && i < kPrintMaxSelf; i++)
	{
		const BotCore::BuffView & b = snap.self.buffs[i];
		snprintf(message, sizeof(message),
			"BotManager: cmd snap:   buff skill=%u type=%u %s remain=%us",
			(unsigned)b.skillId, (unsigned)b.buffType, b.isBuff ? "buff" : "debuff", (unsigned)b.remainingSec);
		WriteBotLog(message);
	}

	for (int i = 0; i < snap.self.cooldownCount && i < kPrintMaxSelf; i++)
	{
		const BotCore::CooldownView & cd = snap.self.cooldowns[i];
		snprintf(message, sizeof(message),
			"BotManager: cmd snap:   cooldown skill=%u remain=%ums",
			(unsigned)cd.skillId, (unsigned)cd.remainingMs);
		WriteBotLog(message);
	}

	char leaderText[24];
	if (snap.team.leaderId == BotCore::kTeamNone)
		snprintf(leaderText, sizeof(leaderText), "unknown");
	else if (snap.team.leaderId == self.sid)
		snprintf(leaderText, sizeof(leaderText), "self");
	else
		snprintf(leaderText, sizeof(leaderText), "id=%u", (unsigned)snap.team.leaderId);

	snprintf(message, sizeof(message),
		"BotManager: cmd snap:   team in_party=%d self_leader=%d leader=%s members %d (total %d)",
		snap.team.inParty ? 1 : 0, snap.team.selfLeader ? 1 : 0, leaderText,
		snap.team.memberCount, snap.team.memberTotal);
	WriteBotLog(message);

	for (int i = 0; i < snap.team.memberCount; i++)
	{
		const BotCore::TeamMemberView & t = snap.team.members[i];
		if (t.inView)
			snprintf(message, sizeof(message),
				"BotManager: cmd snap:   member id=%u name=%s class=%u lvl=%u hp=%d/%d mp=%d/%d %s%s dist=%.1f age=%ums",
				(unsigned)t.id, t.name, (unsigned)t.cls, (unsigned)t.level,
				t.hp, t.maxHp, t.mp, t.maxMp, t.leader ? "leader " : "", t.dead ? "dead" : "alive",
				t.dist, (unsigned)t.ageMs);
		else
			snprintf(message, sizeof(message),
				"BotManager: cmd snap:   member id=%u name=%s class=%u lvl=%u hp=%d/%d mp=%d/%d %s%s out_of_view age=%ums",
				(unsigned)t.id, t.name, (unsigned)t.cls, (unsigned)t.level,
				t.hp, t.maxHp, t.mp, t.maxMp, t.leader ? "leader " : "", t.dead ? "dead" : "alive",
				(unsigned)t.ageMs);
		WriteBotLog(message);
	}

	snprintf(message, sizeof(message),
		"BotManager: cmd snap:   enemies %d (total %d), allies %d (total %d), npcs %d (total %d)",
		snap.enemyCount, snap.enemyTotal, snap.allyCount, snap.allyTotal, snap.npcCount, snap.npcTotal);
	WriteBotLog(message);

	const int kPrintMax = 10;

	for (int i = 0; i < snap.enemyCount && i < kPrintMax; i++)
	{
		const BotCore::UnitView & u = snap.enemies[i];
		char speedText[8];
		if (u.speedField < 0)
			snprintf(speedText, sizeof(speedText), "?");
		else
			snprintf(speedText, sizeof(speedText), "%d", (int)u.speedField);

		char hpText[48];
		if (u.hpKnown)
			snprintf(hpText, sizeof(hpText), "%d/%d hp_age=%ums%s", u.hp, u.maxHp, (unsigned)u.hpAgeMs, u.hpStale ? " stale" : "");
		else
			snprintf(hpText, sizeof(hpText), "?");

		snprintf(message, sizeof(message),
			"BotManager: cmd snap:   enemy id=%u nation=%u class=%u lvl=%u pos=(%.1f, %.1f) dist=%.1f %s%s age=%ums name=%s pos_age=%ums speed=%s v=(%.2f,%.2f) pos=%s hp=%s",
			(unsigned)u.id, (unsigned)u.nation, (unsigned)u.cls, (unsigned)u.level, u.x, u.z, u.dist,
			u.dead ? "dead" : "alive", u.sitting ? " sitting" : "", (unsigned)u.ageMs,
			u.name, (unsigned)u.posAgeMs, speedText, u.vx, u.vz,
			u.posState == BotCore::POS_STALE ? "stale" : (u.posState == BotCore::POS_LOST ? "lost" : "fresh"),
			hpText);
		WriteBotLog(message);
	}

	for (int i = 0; i < snap.allyCount && i < kPrintMax; i++)
	{
		const BotCore::UnitView & u = snap.allies[i];
		char speedText[8];
		if (u.speedField < 0)
			snprintf(speedText, sizeof(speedText), "?");
		else
			snprintf(speedText, sizeof(speedText), "%d", (int)u.speedField);

		char hpText[48];
		if (u.hpKnown)
			snprintf(hpText, sizeof(hpText), "%d/%d hp_age=%ums%s", u.hp, u.maxHp, (unsigned)u.hpAgeMs, u.hpStale ? " stale" : "");
		else
			snprintf(hpText, sizeof(hpText), "?");

		snprintf(message, sizeof(message),
			"BotManager: cmd snap:   ally id=%u nation=%u class=%u lvl=%u pos=(%.1f, %.1f) dist=%.1f %s%s age=%ums name=%s pos_age=%ums speed=%s v=(%.2f,%.2f) pos=%s hp=%s",
			(unsigned)u.id, (unsigned)u.nation, (unsigned)u.cls, (unsigned)u.level, u.x, u.z, u.dist,
			u.dead ? "dead" : "alive", u.sitting ? " sitting" : "", (unsigned)u.ageMs,
			u.name, (unsigned)u.posAgeMs, speedText, u.vx, u.vz,
			u.posState == BotCore::POS_STALE ? "stale" : (u.posState == BotCore::POS_LOST ? "lost" : "fresh"),
			hpText);
		WriteBotLog(message);
	}

	for (int i = 0; i < snap.npcCount && i < kPrintMax; i++)
	{
		const BotCore::NpcView & n = snap.npcs[i];
		char hpText[48];
		if (n.hpKnown)
			snprintf(hpText, sizeof(hpText), "%d/%d hp_age=%ums%s", n.hp, n.maxHp, (unsigned)n.hpAgeMs, n.hpStale ? " stale" : "");
		else
			snprintf(hpText, sizeof(hpText), "?");

		snprintf(message, sizeof(message),
			"BotManager: cmd snap:   npc id=%u proto=%u type=%u nation=%u lvl=%u pos=(%.1f, %.1f) dist=%.1f %s gate=%s age=%ums hp=%s",
			(unsigned)n.id, (unsigned)n.protoId, (unsigned)n.type, (unsigned)n.nation, (unsigned)n.level,
			n.x, n.z, n.dist, n.dead ? "dead" : "alive", n.gateOpen ? "open" : "closed", (unsigned)n.ageMs,
			hpText);
		WriteBotLog(message);
	}

	if (wantEvents)
	{
		const int kPrintEvents = 10;

		snprintf(message, sizeof(message),
			"BotManager: cmd snap:   events total=%u in_ring=%d",
			(unsigned)eventCopy.Total(), eventCopy.Count());
		WriteBotLog(message);

		for (int i = 0; i < eventCopy.Count() && i < kPrintEvents; i++)
		{
			const BotCore::SkillEvent & ev = eventCopy.At(i);
			uint32 age = (nowMs >= ev.tMs) ? (uint32)(nowMs - ev.tMs) : 0;
			snprintf(message, sizeof(message),
				"BotManager: cmd snap:   event age=%ums op=%u skill=%u caster=%d target=%d d0=%d d1=%d d2=%d",
				(unsigned)age, (unsigned)ev.op, (unsigned)ev.skillId,
				(int)ev.caster, (int)ev.target, (int)ev.data[0], (int)ev.data[1], (int)ev.data[2]);
			WriteBotLog(message);
		}
	}
	else if (wantStatus)
	{
		// Estimated (E class) records derived from received broadcasts only; the server never sends another unit's
		// buff list, so a real end can be earlier or later (MEC-BUF-07, docs/13 section 5.2a).
		snprintf(message, sizeof(message),
			"BotManager: cmd snap:   status units=%d records=%d heals_total=%u heals_in_ring=%d",
			statusCopy.Units(), statusCopy.Records(), (unsigned)healCopy.Total(), healCopy.Count());
		WriteBotLog(message);

		const int kPrintStatusUnits = 10;
		int16_t unitIds[BotCore::kObsStatusUnits];
		int unitCount = statusCopy.Targets(unitIds, BotCore::kObsStatusUnits);
		for (int i = 0; i < unitCount && i < kPrintStatusUnits; i++)
		{
			BotCore::StatusObs recs[BotCore::kObsStatusPerUnit];
			int n = statusCopy.Collect(unitIds[i], nowMs, recs, BotCore::kObsStatusPerUnit);
			for (int j = 0; j < n; j++)
			{
				const BotCore::StatusObs & r = recs[j];
				snprintf(message, sizeof(message),
					"BotManager: cmd snap:   status target=%d skill=%u type=%u %s caster=%d remain=%ums src=E",
					(int)unitIds[i], (unsigned)r.skillId, (unsigned)r.buffType,
					r.isBuff ? "buff" : "debuff", (int)r.caster,
					(unsigned)BotCore::StatusRemainingMs(r, nowMs));
				WriteBotLog(message);
			}
		}

		const int kPrintStatusHeals = 5;
		for (int i = 0; i < healCopy.Count() && i < kPrintStatusHeals; i++)
		{
			const BotCore::HealObs & h = healCopy.At(i);
			uint32 age = (nowMs >= h.tMs) ? (uint32)(nowMs - h.tMs) : 0;
			snprintf(message, sizeof(message),
				"BotManager: cmd snap:   status heal age=%ums skill=%u caster=%d target=%d nominal=%u hot=%d",
				(unsigned)age, (unsigned)h.skillId, (int)h.caster, (int)h.target,
				(unsigned)h.nominal, h.hot ? 1 : 0);
			WriteBotLog(message);
		}
	}
	else
	{
		snprintf(message, sizeof(message),
			"BotManager: cmd snap:   events total=%u",
			(unsigned)eventCopy.Total());
		WriteBotLog(message);
	}
}

void BotManager::ParseSpawnList(const std::string & list)
{
	if (list.empty())
		return;

	size_t pos = 0;
	while (pos <= list.size())
	{
		size_t comma = list.find(',', pos);
		std::string name = list.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);

		size_t first = name.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
		{
			if (comma == std::string::npos)
				break;
			pos = comma + 1;
			continue;
		}
		size_t last = name.find_last_not_of(" \t\r\n");
		name = name.substr(first, last - first + 1);

		const BotAccountEntry * entry = nullptr;
		for (size_t i = 0; i < sizeof(BOT_TABLE) / sizeof(BOT_TABLE[0]); i++)
		{
			if (_stricmp(name.c_str(), BOT_TABLE[i].charName) == 0)
			{
				entry = &BOT_TABLE[i];
				break;
			}
		}

		char message[192];
		if (entry == nullptr)
		{
			snprintf(message, sizeof(message),
				"BotManager: SPAWN_ON_START: unknown bot name '%s' ignored", name.c_str());
			WriteBotLog(message);
		}
		else
		{
			bool duplicate = false;
			for (size_t i = 0; i < m_sessions.size(); i++)
			{
				if (_stricmp(m_sessions[i]->m_charName.c_str(), entry->charName) == 0)
				{
					duplicate = true;
					break;
				}
			}

			if (!duplicate)
			{
				if (m_sessions.size() >= m_poolSize)
				{
					snprintf(message, sizeof(message),
						"BotManager: SPAWN_ON_START: '%s' ignored (pool size %u)",
						entry->charName, (unsigned)m_poolSize);
					WriteBotLog(message);
				}
				else
				{
					m_sessions.push_back(new BotSession(entry->charName, entry->accountName));
				}
			}
		}

		if (comma == std::string::npos)
			break;
		pos = comma + 1;
	}

	if (!m_sessions.empty())
	{
		std::string names;
		for (size_t i = 0; i < m_sessions.size(); i++)
		{
			if (i != 0)
				names += ",";
			names += m_sessions[i]->m_charName;
		}

		char message[320];
		snprintf(message, sizeof(message), "BotManager: spawn list: %u bot(s) queued (%s)",
			(unsigned)m_sessions.size(), names.c_str());
		WriteBotLog(message);

		if (m_despawnAfterMs != 0)
		{
			snprintf(message, sizeof(message), "BotManager: despawn after %u s (DESPAWN_AFTER_SEC)",
				(unsigned)(m_despawnAfterMs / 1000));
			WriteBotLog(message);
		}

		if (m_respawnCycles != 0)
		{
			if (m_despawnAfterMs == 0)
			{
				m_respawnCycles = 0;
				WriteBotLog("BotManager: RESPAWN_CYCLES ignored (DESPAWN_AFTER_SEC is 0)");
			}
			else
			{
				snprintf(message, sizeof(message),
					"BotManager: respawn cycles: %u per bot (RESPAWN_CYCLES), %llu spawns planned",
					(unsigned)m_respawnCycles,
					(unsigned long long)m_sessions.size() * (1ULL + m_respawnCycles));
				WriteBotLog(message);
			}
		}
	}
}

// Plan F4-55: number of sessions currently between GameStart(1) and GameStart(2).
static int CountInHandshake(const std::vector<BotSession *> & sessions)
{
	int count = 0;
	for (size_t i = 0; i < sessions.size(); i++)
	{
		if (sessions[i]->m_phase == BotSession::PHASE_WAIT_LOADED)
			count++;
	}
	return count;
}

void BotManager::TickSessions()
{
	if (m_sessions.empty())
		return;

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (now - m_firstTickTime < std::chrono::milliseconds(SPAWN_START_DELAY_MS))
		return;

	bool startedThisTick = false;
	size_t busyCount = 0, releasedCount = 0, stuckCount = 0;

	for (size_t i = 0; i < m_sessions.size(); i++)
	{
		BotSession * s = m_sessions[i];

		switch (s->m_phase)
		{
		case BotSession::PHASE_QUEUED:
			// One new spawn per tick, so the DB thread is never flooded.
			if (!startedThisTick)
			{
				StartSession(s);
				startedThisTick = true;
			}
			break;

		case BotSession::PHASE_WAIT_SELECT:
			{
				int selectResult = s->m_selectResult.load();
				if (selectResult == BotSession::SELECT_FAILED)
				{
					FailSession(s, "select rejected");
				}
				else if (selectResult == BotSession::SELECT_OK)
				{
					if (!s->m_selectSeen)
					{
						s->m_selectSeen = true;
						s->m_selectSeenAt = now;
					}

					if (now - s->m_selectSeenAt >= std::chrono::milliseconds(SELECT_SETTLE_MS))
					{
						// Plan F4-55: serialize the login handshake so no two sessions sit
						// between GameStart(1) and GameStart(2) at once. A closed gate leaves
						// this session in PHASE_WAIT_SELECT and retries on the next tick.
						if (!BotCore::HandshakeGateOpen(CountInHandshake(m_sessions)))
						{
							m_handshakeWaitTicks++;
							break;
						}

						Packet pkt(WIZ_GAMESTART, uint8(1));
						s->m_pUser->HandlePacket(pkt);
						s->m_phase = BotSession::PHASE_WAIT_LOADED;
						s->m_phaseStart = now;
					}
				}
				else if (now - s->m_phaseStart > std::chrono::milliseconds(PHASE_TIMEOUT_MS))
				{
					FailSession(s, "select timeout");
				}
			}
			break;

		case BotSession::PHASE_WAIT_LOADED:
			if (now - s->m_phaseStart >= std::chrono::milliseconds(LOADED_DELAY_MS))
			{
				Packet pkt(WIZ_GAMESTART, uint8(2));
				s->m_pUser->HandlePacket(pkt);

				if (s->m_pUser->isInGame())
				{
					s->m_phase = BotSession::PHASE_IN_GAME;
					s->m_inGameSince = now;
					s->m_lastUpdate = now;
					s->m_slotId = s->m_pUser->GetSocketID();
					s->m_selfSid = (int)s->m_pUser->GetSocketID();
					m_spawnOk++;

					char message[256];
					snprintf(message, sizeof(message),
						"BotManager: bot %s in game (slot %u, zone %u, pos %.1f,%.1f, hp %d/%d, packets %u, myinfo %u)",
						s->m_charName.c_str(), (unsigned)s->m_pUser->GetSocketID(),
						(unsigned)s->m_pUser->GetZoneID(), s->m_pUser->GetX(), s->m_pUser->GetZ(),
						s->m_pUser->GetHealth(), s->m_pUser->GetMaxHealth(),
						(unsigned)s->m_packetTotal.load(), (unsigned)s->m_opcodeCount[WIZ_MYINFO].load());
					WriteBotLog(message);
				}
				else
				{
					FailSession(s, "game start failed");
				}
			}
			break;

		case BotSession::PHASE_IN_GAME:
			if (m_despawnAfterMs != 0
				&& now - s->m_inGameSince >= std::chrono::milliseconds(m_despawnAfterMs))
			{
				BeginDespawn(s, now);
			}
			else if (now - s->m_lastUpdate >= std::chrono::milliseconds(UPDATE_PERIOD_MS))
			{
				// S8: timed effects/saves; runs on the IOCP thread, never on the 30 s timer thread.
				s->m_lastUpdate = now;
				s->m_updateCount++;
				s->m_pUser->Update();
			}

			// BeginDespawn() moved the session out of PHASE_IN_GAME; only advance walks that are still live.
			if (s->m_phase == BotSession::PHASE_IN_GAME)
			{
				if (s->m_pUser->isDead())
				{
					if (!s->m_deadSeen) { s->m_deadSeen = true; s->m_deadSince = now; }

					if (s->m_moveActive)
					{
						ActionExecutor::AbandonMove(s);
						char message[192];
						snprintf(message, sizeof(message),
							"BotManager: bot %s move stopped (dead)", s->m_charName.c_str());
						WriteBotLog(message);
					}
				}
				else
				{
					s->m_deadSeen = false;

					// ADR-0017 Ek F4-24 / CLI-03: moving while a cast waits for EFFECTING cancels it first (cancel packet, then move).
					bool moveHeld = false;
					if (s->m_moveActive && s->m_castPhase == BotSession::CAST_CASTING)
					{
						CastOutcome cancel = ActionExecutor::CancelCast(s, "move", now);
						if (cancel.kind == CastOutcome::REFUSED)
							moveHeld = true;   // rate: the move must not overtake its cancel, retry next tick

						char message[224];
						if (cancel.kind == CastOutcome::SENT)
							snprintf(message, sizeof(message),
								"BotManager: bot %s cast cancelled by move after %u packet(s) sent",
								s->m_charName.c_str(), (unsigned)s->m_castPackets);
						else if (cancel.kind == CastOutcome::FAILED)
							snprintf(message, sizeof(message),
								"BotManager: bot %s cast cancel failed (%s)",
								s->m_charName.c_str(), cancel.reason);
						else
							snprintf(message, sizeof(message),
								"BotManager: bot %s cast cancel deferred (%s)",
								s->m_charName.c_str(), cancel.reason);
						WriteBotLog(message);
					}

					if (!moveHeld)
					{
						MoveOutcome outcome = ActionExecutor::TickMove(s, now);
						if (outcome.kind == MoveOutcome::ARRIVED)
						{
							char message[224];
							snprintf(message, sizeof(message),
								"BotManager: bot %s arrived at (%.1f, %.1f) after %u packets",
								s->m_charName.c_str(), s->m_pUser->GetX(), s->m_pUser->GetZ(),
								(unsigned)s->m_movePackets);
							WriteBotLog(message);
						}
						else if (outcome.kind == MoveOutcome::REFUSED || outcome.kind == MoveOutcome::FAILED)
						{
							char message[224];
							snprintf(message, sizeof(message),
								"BotManager: bot %s move stopped (%s)",
								s->m_charName.c_str(), outcome.reason);
							WriteBotLog(message);
						}
					}

					UserInOutcome userIn = ActionExecutor::TickUserIn(s, now);
					if (userIn.kind == UserInOutcome::SENT)
					{
						char message[224];
						snprintf(message, sizeof(message),
							"BotManager: bot %s userin requested %d, received %d",
							s->m_charName.c_str(), userIn.requested, userIn.received);
						WriteBotLog(message);
					}
					else if (userIn.kind == UserInOutcome::REFUSED || userIn.kind == UserInOutcome::FAILED)
					{
						char message[224];
						snprintf(message, sizeof(message),
							"BotManager: bot %s userin failed (%s)",
							s->m_charName.c_str(), userIn.reason);
						WriteBotLog(message);
					}

					NpcInOutcome npcIn = ActionExecutor::TickNpcIn(s, now);
					if (npcIn.kind == NpcInOutcome::SENT)
					{
						char message[224];
						snprintf(message, sizeof(message),
							"BotManager: bot %s npcin requested %d, received %d",
							s->m_charName.c_str(), npcIn.requested, npcIn.received);
						WriteBotLog(message);
					}
					else if (npcIn.kind == NpcInOutcome::REFUSED || npcIn.kind == NpcInOutcome::FAILED)
					{
						char message[224];
						snprintf(message, sizeof(message),
							"BotManager: bot %s npcin failed (%s)",
							s->m_charName.c_str(), npcIn.reason);
						WriteBotLog(message);
					}

					if (m_speedCheck)
					{
						SpeedCheckOutcome speed = ActionExecutor::TickSpeedCheck(s, now);
						if (speed.kind == SpeedCheckOutcome::FAILED)
						{
							char message[224];
							snprintf(message, sizeof(message),
								"BotManager: bot %s speedcheck: server warped it back to (%.1f, %.1f)",
								s->m_charName.c_str(), speed.warpX, speed.warpZ);
							WriteBotLog(message);
						}
					}
				}

				if (s->m_attackActive)
				{
					if (s->m_pUser->isDead())
					{
						ActionExecutor::EndAttack(s);
						char message[192];
						snprintf(message, sizeof(message),
							"BotManager: bot %s attack stopped (dead)", s->m_charName.c_str());
						WriteBotLog(message);
					}
					else
					{
						// Test driver: the target view comes straight from the target bot's session.
						// The Perception slice (ADR-0017 Ek F4-02) replaces this source, not AttackTarget.
						BotSession * t = FindSession(s->m_attackTargetName.c_str());
						if (t == nullptr || t->m_phase != BotSession::PHASE_IN_GAME || t->m_pUser == nullptr)
						{
							ActionExecutor::EndAttack(s);
							char message[192];
							snprintf(message, sizeof(message),
								"BotManager: bot %s attack stopped (target_lost)", s->m_charName.c_str());
							WriteBotLog(message);
						}
						else
						{
							AttackTarget tv = { (int16)t->m_pUser->GetSocketID(), t->m_pUser->GetX(), t->m_pUser->GetZ() };
							AttackOutcome attack = ActionExecutor::TickAttack(s, tv, now);
							if (attack.kind == AttackOutcome::FINISHED)
							{
								char message[224];
								snprintf(message, sizeof(message),
									"BotManager: bot %s attack finished (%s) after %u hit(s) sent, %u ok",
									s->m_charName.c_str(), attack.reason,
									(unsigned)s->m_attackSent, (unsigned)s->m_attackHits);
								WriteBotLog(message);
							}
							else if (attack.kind == AttackOutcome::REFUSED || attack.kind == AttackOutcome::FAILED)
							{
								char message[224];
								snprintf(message, sizeof(message),
									"BotManager: bot %s attack stopped (%s)",
									s->m_charName.c_str(), attack.reason);
								WriteBotLog(message);
							}
						}
					}
				}

				if (s->m_castPhase != BotSession::CAST_IDLE)
				{
					if (s->m_pUser->isDead())
					{
						ActionExecutor::EndCast(s);
						char message[192];
						snprintf(message, sizeof(message),
							"BotManager: bot %s cast stopped (dead)", s->m_charName.c_str());
						WriteBotLog(message);
					}
					else
					{
						// Test driver: the target view comes straight from the target bot's session (or the
						// caster itself for "self"). The Perception slice (ADR-0017) replaces this source,
						// not CastTarget.
						CastTarget tv;
						if (s->m_castTargetName.empty())
						{
							tv.id = (int16)s->m_pUser->GetID();
							tv.x = s->m_pUser->GetX();
							tv.y = s->m_pUser->GetY();
							tv.z = s->m_pUser->GetZ();
							tv.isSelf = true;
						}
						else
						{
							BotSession * t = FindSession(s->m_castTargetName.c_str());
							if (t == nullptr || t->m_phase != BotSession::PHASE_IN_GAME || t->m_pUser == nullptr)
							{
								ActionExecutor::EndCast(s);
								char message[192];
								snprintf(message, sizeof(message),
									"BotManager: bot %s cast stopped (target_lost)", s->m_charName.c_str());
								WriteBotLog(message);
							}
							else
							{
								tv.id = (int16)t->m_pUser->GetID();
								tv.x = t->m_pUser->GetX();
								tv.y = t->m_pUser->GetY();
								tv.z = t->m_pUser->GetZ();
								tv.isSelf = false;
							}
						}

						if (s->m_castPhase != BotSession::CAST_IDLE)
						{
							CastOutcome cast = ActionExecutor::TickCast(s, tv, now);
							if (cast.kind == CastOutcome::FINISHED)
							{
								char message[256];
								snprintf(message, sizeof(message),
									"BotManager: bot %s cast finished (%s) after %u cycle(s), %u ok, %u packet(s) sent",
									s->m_charName.c_str(), cast.reason,
									(unsigned)s->m_castCycle, (unsigned)s->m_castDone, (unsigned)s->m_castPackets);
								WriteBotLog(message);
							}
							else if (cast.kind == CastOutcome::REFUSED || cast.kind == CastOutcome::FAILED)
							{
								char message[224];
								snprintf(message, sizeof(message),
									"BotManager: bot %s cast stopped (%s)",
									s->m_charName.c_str(), cast.reason);
								WriteBotLog(message);
							}
						}
					}
				}

				if (s->m_potActive)
				{
					if (s->m_pUser->isDead())
					{
						ActionExecutor::EndPotion(s);
						char message[192];
						snprintf(message, sizeof(message),
							"BotManager: bot %s pot stopped (dead)", s->m_charName.c_str());
						WriteBotLog(message);
					}
					else
					{
						PotionOutcome pot = ActionExecutor::TickPotion(s, now);
						if (pot.kind == PotionOutcome::FINISHED)
						{
							char message[256];
							snprintf(message, sizeof(message),
								"BotManager: bot %s pot finished (%s) after %u use(s), %u ok",
								s->m_charName.c_str(), pot.reason,
								(unsigned)s->m_potSent, (unsigned)s->m_potOk);
							WriteBotLog(message);
						}
						else if (pot.kind == PotionOutcome::REFUSED || pot.kind == PotionOutcome::FAILED)
						{
							char message[224];
							snprintf(message, sizeof(message),
								"BotManager: bot %s pot stopped (%s)",
								s->m_charName.c_str(), pot.reason);
							WriteBotLog(message);
						}
					}
				}
			}
			break;

		case BotSession::PHASE_DESPAWN_WAIT:
			PollDespawn(s, now);
			break;

		default:
			break;
		}

		if (s->m_phase == BotSession::PHASE_QUEUED
			|| s->m_phase == BotSession::PHASE_WAIT_SELECT
			|| s->m_phase == BotSession::PHASE_WAIT_LOADED
			|| s->m_phase == BotSession::PHASE_IN_GAME
			|| s->m_phase == BotSession::PHASE_DESPAWN_WAIT)
			busyCount++;
		else if (s->m_phase == BotSession::PHASE_DESPAWNED)
			releasedCount++;
		else if (s->m_phase == BotSession::PHASE_DESPAWN_STUCK)
			stuckCount++;
	}

	if (!m_spawnSummaryDone && m_spawnOk + m_spawnFailed >= m_sessions.size())
	{
		m_spawnSummaryDone = true;

		char message[200];
		snprintf(message, sizeof(message),
			"BotManager: spawn complete: %u/%u in game, %u failed",
			(unsigned)m_spawnOk, (unsigned)m_sessions.size(), (unsigned)m_spawnFailed);
		WriteBotLog(message);

		snprintf(message, sizeof(message),
			"BotManager: spawn handshake: serialized, gate waits %u tick(s)",
			(unsigned)m_handshakeWaitTicks);
		WriteBotLog(message);
	}

	if (m_despawnAfterMs != 0 && m_spawnSummaryDone && !m_despawnSummaryDone
		&& busyCount == 0)
	{
		m_despawnSummaryDone = true;
		size_t poolFree;
		{
			std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock());
			poolFree = g_pMain->m_socketMgr.GetReservedSessionMap().size();
		}

		char message[288];
		snprintf(message, sizeof(message),
			"BotManager: despawn complete: %u/%u released, %u stuck, %u never spawned, pool free %u/%u",
			(unsigned)releasedCount, (unsigned)m_sessions.size(), (unsigned)stuckCount,
			(unsigned)m_spawnFailed, (unsigned)poolFree, (unsigned)m_poolSize);
		WriteBotLog(message);

		if (m_respawnCycles != 0)
		{
			long long elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - m_firstTickTime).count();

			snprintf(message, sizeof(message),
				"BotManager: respawn cycles done: %u spawns, %u despawns, %u failed, %u stuck, %u names left, pool free %u/%u, elapsed %lld s",
				(unsigned)m_spawnOk, (unsigned)m_despawnOk, (unsigned)m_spawnFailed,
				(unsigned)stuckCount, (unsigned)m_namesLeft, (unsigned)poolFree,
				(unsigned)m_poolSize, elapsedSec);
			WriteBotLog(message);
		}
	}
}

void BotManager::BeginDespawn(BotSession * s, std::chrono::steady_clock::time_point now)
{
	CUser * pUser = s->m_pUser;
	ActionExecutor::AbandonMove(s);
	ActionExecutor::EndAttack(s);
	ActionExecutor::EndCast(s);
	ActionExecutor::EndPotion(s);
	// Socket::Disconnect() does nothing without a socket, so run what a real disconnect runs:
	// OnDisconnect() removes the account/character names, takes the bot out of its region
	// and queues WIZ_LOGOUT (LogOut() sets m_deleted until the DB thread has saved the bot).
	pUser->OnDisconnect();
	s->m_phase = BotSession::PHASE_DESPAWN_WAIT;
	s->m_despawnStart = now;

	char message[224];
	snprintf(message, sizeof(message),
		"BotManager: bot %s despawning (slot %u)",
		s->m_charName.c_str(), (unsigned)s->m_slotId);
	WriteBotLog(message);
}

void BotManager::PollDespawn(BotSession * s, std::chrono::steady_clock::time_point now)
{
	CUser * pUser = s->m_pUser;
	long long waited = std::chrono::duration_cast<std::chrono::milliseconds>(
		now - s->m_despawnStart).count();

	// m_deleted is cleared by the DB thread as ReqUserLogOut()'s last statement; the IOCP
	// thread reads it without a lock (same as real player sessions).
	if (pUser->IsDeleted())
	{
		if (waited > DESPAWN_TIMEOUT_MS)
		{
			// Releasing now could hand the slot to a new spawn while the DB thread still uses it.
			s->m_phase = BotSession::PHASE_DESPAWN_STUCK;

			char message[224];
			snprintf(message, sizeof(message),
				"BotManager: bot %s despawn TIMEOUT after %lld ms (slot %u kept)",
				s->m_charName.c_str(), waited, (unsigned)s->m_slotId);
			WriteBotLog(message);
		}
		return;
	}

	bool namesCleared = g_pMain->GetUserPtr(s->m_charName, TYPE_CHARACTER) == nullptr
		&& g_pMain->GetUserPtr(s->m_accountName, TYPE_ACCOUNT) == nullptr;
	ReleaseSlot(pUser);
	s->m_pUser = nullptr;
	s->m_phase = BotSession::PHASE_DESPAWNED;

	char message[288];
	snprintf(message, sizeof(message),
		"BotManager: bot %s despawned (slot %u, logout save %lld ms, updates %u, packets %u, names cleared %s)",
		s->m_charName.c_str(), (unsigned)s->m_slotId, waited,
		(unsigned)s->m_updateCount, (unsigned)s->m_packetTotal.load(),
		namesCleared ? "yes" : "no");
	WriteBotLog(message);

	s->m_despawnCount++;
	m_despawnOk++;
	if (!namesCleared)
		m_namesLeft++;

	if (m_respawnCycles != 0 && m_despawnOk % CYCLE_PROGRESS_EVERY == 0)
	{
		size_t poolFree;
		{
			std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock());
			poolFree = g_pMain->m_socketMgr.GetReservedSessionMap().size();
		}

		snprintf(message, sizeof(message),
			"BotManager: cycle progress: %u spawns, %u despawns, %u failed, pool free %u/%u",
			(unsigned)m_spawnOk, (unsigned)m_despawnOk, (unsigned)m_spawnFailed,
			(unsigned)poolFree, (unsigned)m_poolSize);
		WriteBotLog(message);
	}

	// Slot is back in the pool and the DB save is done, so the same session can spawn again.
	if (s->m_despawnCount <= m_respawnCycles)
		s->ResetForRespawn();
}

void BotManager::StartSession(BotSession * s)
{
	if (g_pMain->GetUserPtr(s->m_charName, TYPE_CHARACTER) != nullptr)
	{
		FailSession(s, "character already online");
		return;
	}

	if (g_pMain->GetUserPtr(s->m_accountName, TYPE_ACCOUNT) != nullptr)
	{
		FailSession(s, "account already online");
		return;
	}

	CUser * pUser = AcquireSlot();
	if (pUser == nullptr)
	{
		FailSession(s, "no free slot");
		return;
	}

	// Same order as a real connection: OnConnect()/Initialize() first, then the account,
	// then the sink, so SelectCharacter()'s reply reaches the session.
	pUser->OnConnect();
	pUser->EnableCrypto();
	pUser->m_strAccountID = s->m_accountName;
	pUser->m_botSink = s;
	g_pMain->AddAccountName(pUser);
	s->m_pUser = pUser;

	Packet req(WIZ_SEL_CHAR);
	req << s->m_charName << uint8(1);
	g_pMain->AddDatabaseRequest(req, pUser);

	s->m_phase = BotSession::PHASE_WAIT_SELECT;
	s->m_phaseStart = std::chrono::steady_clock::now();

	char message[224];
	snprintf(message, sizeof(message),
		"BotManager: bot %s spawning (slot %u, account %s)",
		s->m_charName.c_str(), (unsigned)pUser->GetSocketID(), s->m_accountName.c_str());
	WriteBotLog(message);
}

void BotManager::FailSession(BotSession * s, const char * reason)
{
	// The slot is deliberately not released and the session is not deleted: the DB thread
	// may still be running this session's CUser. Cleanup is F2-04's job.
	s->m_phase = BotSession::PHASE_FAILED;
	m_spawnFailed++;

	char message[200];
	snprintf(message, sizeof(message),
		"BotManager: bot %s spawn FAILED (%s)", s->m_charName.c_str(), reason);
	WriteBotLog(message);
}
