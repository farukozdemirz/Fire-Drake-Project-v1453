#include "stdafx.h"
#include "BotManager.h"
#include "IBotSink.h"
#include "../../shared/Ini.h"

#include <cstdio>
#include <ctime>

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
