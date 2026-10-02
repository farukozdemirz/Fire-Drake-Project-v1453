#include "stdafx.h"
#include "BotManager.h"
#include "IBotSink.h"
#include "BotSession.h"
#include "../../shared/Ini.h"

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

	TickSessions();
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
	size_t inGameCount = 0, waitCount = 0, releasedCount = 0, stuckCount = 0;

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

		if (s->m_phase == BotSession::PHASE_IN_GAME)
			inGameCount++;
		else if (s->m_phase == BotSession::PHASE_DESPAWN_WAIT)
			waitCount++;
		else if (s->m_phase == BotSession::PHASE_DESPAWNED)
			releasedCount++;
		else if (s->m_phase == BotSession::PHASE_DESPAWN_STUCK)
			stuckCount++;
	}

	if (!m_spawnSummaryDone && m_spawnOk + m_spawnFailed == m_sessions.size())
	{
		m_spawnSummaryDone = true;

		char message[200];
		snprintf(message, sizeof(message),
			"BotManager: spawn complete: %u/%u in game, %u failed",
			(unsigned)m_spawnOk, (unsigned)m_sessions.size(), (unsigned)m_spawnFailed);
		WriteBotLog(message);
	}

	if (m_despawnAfterMs != 0 && m_spawnSummaryDone && !m_despawnSummaryDone
		&& inGameCount == 0 && waitCount == 0)
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
