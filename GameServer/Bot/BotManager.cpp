#include "stdafx.h"
#include "BotManager.h"
#include "IBotSink.h"
#include "BotSession.h"
#include "Telemetry.h"
#include "../../shared/Ini.h"

#include <algorithm>
#include <cstdio>
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
	else
	{
		snprintf(message, sizeof(message),
			"BotManager: cmd unknown command '%s' (spawn, despawn, list)", verb.c_str());
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

void BotManager::CommandList()
{
	size_t poolFree;
	{
		std::lock_guard<std::recursive_mutex> lock(g_pMain->m_socketMgr.GetLock());
		poolFree = g_pMain->m_socketMgr.GetReservedSessionMap().size();
	}

	char message[192];
	snprintf(message, sizeof(message),
		"BotManager: cmd list: %u session(s), pool free %u/%u",
		(unsigned)m_sessions.size(), (unsigned)poolFree, (unsigned)m_poolSize);
	WriteBotLog(message);

	for (size_t i = 0; i < m_sessions.size(); i++)
	{
		BotSession * s = m_sessions[i];
		char slot[16];
		if (s->m_pUser != nullptr)
			snprintf(slot, sizeof(slot), "%u", (unsigned)s->m_slotId);
		else
			snprintf(slot, sizeof(slot), "-");

		snprintf(message, sizeof(message),
			"BotManager: cmd list:   %s phase=%s slot=%s despawns=%u",
			s->m_charName.c_str(), PhaseName(s->m_phase), slot, (unsigned)s->m_despawnCount);
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
