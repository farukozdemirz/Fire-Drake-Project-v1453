#include "stdafx.h"
#include "Telemetry.h"
#include "../../shared/Ini.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

static const uint32 WRITE_PERIOD_MS = 100;
static const uint32 SELFTEST_WAIT_MS = 5000;

// Appends one line to ./Logs/Bot_<day>_<month>_<year>.log (same file as BotManager's log).
static void WriteTelemetryLog(const char * line)
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

// JSON string escaping for the 'name' field. Non-ASCII bytes become '?' so the output
// stays valid UTF-8 (character names are ASCII).
static void JsonEscape(const std::string & in, std::string & out)
{
	out.clear();
	out.reserve(in.size());

	for (size_t i = 0; i < in.size(); i++)
	{
		unsigned char c = (unsigned char)in[i];
		switch (c)
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20)
			{
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
				out += buf;
			}
			else if (c >= 0x80)
			{
				out += '?';
			}
			else
			{
				out += (char)c;
			}
			break;
		}
	}
}

Telemetry & Telemetry::Instance()
{
	static Telemetry instance;
	return instance;
}

Telemetry::Telemetry()
	: m_level(TEL_OFF), m_running(false), m_stopping(false), m_paused(false),
	m_writerThread(nullptr), m_file(nullptr), m_written(0), m_droppedSoft(0),
	m_droppedHard(0)
{
}

bool Telemetry::Start()
{
	CIni ini(CONF_GAME_SERVER);
	std::string level;
	ini.GetString("BOT", "TELEMETRY", "summary", level);

	if (_stricmp(level.c_str(), "off") == 0)
		m_level = TEL_OFF;
	else if (_stricmp(level.c_str(), "summary") == 0)
		m_level = TEL_SUMMARY;
	else if (_stricmp(level.c_str(), "decisions") == 0)
		m_level = TEL_DECISIONS;
	else if (_stricmp(level.c_str(), "trace") == 0)
		m_level = TEL_TRACE;
	else
	{
		char message[160];
		snprintf(message, sizeof(message), "Telemetry: unknown level '%s', telemetry off", level.c_str());
		WriteTelemetryLog(message);
		m_level = TEL_OFF;
	}

	if (m_level == TEL_OFF)
		return true;

	bool selfTest = ini.GetBool("BOT", "TELEMETRY_SELFTEST", false);

	CreateDirectory("Logs", NULL);
	CreateDirectory("Logs/bots", NULL);

	time_t now = time(nullptr);
	struct tm * local = localtime(&now);
	char dir[72];
	snprintf(dir, sizeof(dir), "Logs/bots/%04d-%02d-%02d",
		local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
	CreateDirectory(dir, NULL);

	char path[144];
	snprintf(path, sizeof(path), "%s/live-%02d%02d%02d.jsonl",
		dir, local->tm_hour, local->tm_min, local->tm_sec);

	m_filePath = path;
	m_file = fopen(m_filePath.c_str(), "ab");
	if (m_file == nullptr)
	{
		char message[192];
		snprintf(message, sizeof(message), "Telemetry: cannot open %s", m_filePath.c_str());
		WriteTelemetryLog(message);
		m_level = TEL_OFF;
		return false;
	}

	m_queue.reserve(HARD_LIMIT);
	m_stopping = false;
	m_running = true;
	m_writerThread = new Thread(WriterThreadProc, this);

	const char * levelName = m_level == TEL_SUMMARY ? "summary"
		: (m_level == TEL_DECISIONS ? "decisions" : "trace");
	char message[192];
	snprintf(message, sizeof(message), "Telemetry: level %s, writing %s",
		levelName, m_filePath.c_str());
	WriteTelemetryLog(message);

	if (selfTest)
		RunSelfTest();

	return true;
}

void Telemetry::Stop()
{
	if (m_writerThread == nullptr && m_file == nullptr)
		return;

	m_running = false;
	m_stopping = true;

	if (m_writerThread != nullptr)
	{
		m_writerThread->waitForExit();
		delete m_writerThread;
		m_writerThread = nullptr;
	}

	if (m_file != nullptr)
	{
		fclose(m_file);
		m_file = nullptr;
	}

	TelemetryStats stats = GetStats();
	char message[192];
	snprintf(message, sizeof(message), "Telemetry: stopped, written %llu, dropped soft %llu, hard %llu",
		(unsigned long long)stats.written, (unsigned long long)stats.droppedSoft,
		(unsigned long long)stats.droppedHard);
	WriteTelemetryLog(message);
}

bool Telemetry::Emit(TelemetryLevel level, const char * ev, int bot, const char * name,
	const std::string & fields, bool droppable)
{
	if (!m_running.load() || m_level < level)
		return false;

	TelemetryEvent event;
	event.t = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
	event.ev = ev;
	event.bot = bot;
	if (name != nullptr)
		event.name = name;
	event.fields = fields;

	std::lock_guard<std::mutex> lock(m_lock);
	if (m_queue.size() >= HARD_LIMIT)
	{
		m_droppedHard++;
		return false;
	}

	if (droppable && m_queue.size() >= SOFT_LIMIT)
	{
		m_droppedSoft++;
		return false;
	}

	m_queue.push_back(std::move(event));
	return true;
}

TelemetryStats Telemetry::GetStats()
{
	std::lock_guard<std::mutex> lock(m_lock);
	TelemetryStats stats;
	stats.written = m_written;
	stats.droppedSoft = m_droppedSoft;
	stats.droppedHard = m_droppedHard;
	stats.queueLen = (uint32)m_queue.size();
	return stats;
}

uint32 THREADCALL Telemetry::WriterThreadProc(void * lpParam)
{
	Telemetry * self = (Telemetry *)lpParam;
	self->WriterLoop();
	return 0;
}

void Telemetry::WriterLoop()
{
	for (;;)
	{
		bool stopping = m_stopping.load();
		if (!m_paused.load() || stopping)
		{
			std::vector<TelemetryEvent> batch;
			{
				std::lock_guard<std::mutex> lock(m_lock);
				batch.swap(m_queue);
				m_queue.reserve(HARD_LIMIT);
			}

			if (!batch.empty())
				WriteBatch(batch);
		}

		if (stopping)
			break;

		sleep(WRITE_PERIOD_MS);
	}
}

void Telemetry::WriteBatch(std::vector<TelemetryEvent> & batch)
{
	std::string buffer;
	buffer.reserve(batch.size() * 96);

	for (size_t i = 0; i < batch.size(); i++)
	{
		const TelemetryEvent & e = batch[i];

		char head[96];
		snprintf(head, sizeof(head), "{\"t\":%lld,\"match\":\"-\",\"bot\":%d",
			(long long)e.t, e.bot);
		buffer += head;

		if (!e.name.empty())
		{
			std::string escaped;
			JsonEscape(e.name, escaped);
			buffer += ",\"name\":\"";
			buffer += escaped;
			buffer += "\"";
		}

		buffer += ",\"ev\":\"";
		buffer += e.ev;
		buffer += "\",\"mode\":\"live\"";

		if (!e.fields.empty())
		{
			buffer += ",";
			buffer += e.fields;
		}

		buffer += "}\n";
	}

	size_t written = fwrite(buffer.data(), 1, buffer.size(), m_file);
	fflush(m_file);

	if (written != buffer.size())
	{
		static bool writeErrorLogged = false;
		if (!writeErrorLogged)
		{
			writeErrorLogged = true;
			WriteTelemetryLog("Telemetry: write error");
		}
		return;
	}

	std::lock_guard<std::mutex> lock(m_lock);
	m_written += batch.size();
}

bool Telemetry::RunSelfTest()
{
	TelemetryStats before = GetStats();
	if (before.queueLen != 0)
	{
		WriteTelemetryLog("Telemetry: self-test FAILED (queue not empty at start)");
		return false;
	}

	long long start = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();

	m_paused = true;

	uint32 accepted = 0;
	for (int i = 0; i < 7000; i++)
	{
		std::string fields = "\"i\":" + std::to_string(i);
		if (Emit(TEL_SUMMARY, "SELFTEST", -1, nullptr, fields, true))
			accepted++;
	}

	for (int i = 0; i < 3000; i++)
	{
		std::string fields = "\"i\":" + std::to_string(i);
		if (Emit(TEL_SUMMARY, "SELFTEST", -1, nullptr, fields, false))
			accepted++;
	}

	m_paused = false;

	long long waited = 0;
	for (;;)
	{
		TelemetryStats current = GetStats();
		if (current.queueLen == 0 && current.written >= before.written + accepted)
			break;
		if (waited >= SELFTEST_WAIT_MS)
			break;

		sleep(20);
		waited += 20;
	}

	TelemetryStats after = GetStats();
	long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count() - start;

	bool ok = (accepted == (uint32)HARD_LIMIT)
		&& (after.droppedSoft - before.droppedSoft == 7000 - SOFT_LIMIT)
		&& (after.droppedHard - before.droppedHard == 3000 - (HARD_LIMIT - SOFT_LIMIT))
		&& (after.written - before.written == accepted)
		&& (after.queueLen == 0);

	char message[224];
	if (ok)
		snprintf(message, sizeof(message),
			"Telemetry: self-test OK (accepted %u, soft drops %u, hard drops %u, written %u in %lld ms)",
			(unsigned)accepted, (unsigned)(after.droppedSoft - before.droppedSoft),
			(unsigned)(after.droppedHard - before.droppedHard), (unsigned)(after.written - before.written),
			elapsed);
	else
		snprintf(message, sizeof(message),
			"Telemetry: self-test FAILED (accepted %u, soft %u, hard %u, written %u, queue %u)",
			(unsigned)accepted, (unsigned)(after.droppedSoft - before.droppedSoft),
			(unsigned)(after.droppedHard - before.droppedHard), (unsigned)(after.written - before.written),
			(unsigned)after.queueLen);
	WriteTelemetryLog(message);

	return ok;
}
