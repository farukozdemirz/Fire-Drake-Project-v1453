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

// A scenario/result token is used as (part of) a file name: [A-Za-z0-9_.-], 1..32 chars, no leading dot.
static bool IsSafeToken(const std::string & s)
{
	if (s.empty() || s.size() > 32 || s[0] == '.')
		return false;

	for (size_t i = 0; i < s.size(); i++)
	{
		char c = s[i];
		bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
		if (!ok)
			return false;
	}

	return true;
}

// UTC wall clock for the MATCH_START/MATCH_END records only.
static void FormatUtc(char * out, size_t size)
{
	time_t now = time(nullptr);
	struct tm utc;
	gmtime_s(&utc, &now);
	strftime(out, size, "%Y-%m-%dT%H:%M:%SZ", &utc);
}

static bool FileExists(const char * path)
{
	return GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
}

Telemetry & Telemetry::Instance()
{
	static Telemetry instance;
	return instance;
}

Telemetry::Telemetry()
	: m_level(TEL_OFF), m_running(false), m_stopping(false), m_paused(false),
	m_writerThread(nullptr), m_file(nullptr), m_written(0), m_droppedSoft(0),
	m_droppedHard(0), m_matchActive(false), m_matchStartMs(0), m_matchBaseSoft(0),
	m_matchBaseHard(0), m_matchRun(0), m_matchFile(nullptr), m_matchLines(0)
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

	{
		std::lock_guard<std::mutex> lock(m_matchLock);
		if (m_matchActive)
		{
			std::string id, error;
			long long ms = 0;
			if (EndMatchLocked("aborted", "", id, ms, error))
				WriteTelemetryLog(("Telemetry: match " + id + " aborted at shutdown").c_str());
		}
	}

	m_running = false;
	m_stopping = true;

	if (m_writerThread != nullptr)
	{
		m_writerThread->waitForExit();
		delete m_writerThread;
		m_writerThread = nullptr;
	}

	if (m_matchFile != nullptr)
	{
		fclose(m_matchFile);
		m_matchFile = nullptr;
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

std::string Telemetry::EscapeJson(const std::string & in)
{
	std::string out;
	JsonEscape(in, out);
	return out;
}

bool Telemetry::EmitControl(int ctl, const char * ev, const std::string & path, const std::string & fields)
{
	if (!m_running.load())
		return false;

	TelemetryEvent event;
	event.t = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
	event.ev = ev;
	event.bot = -1;
	event.ctl = ctl;
	event.name = path;
	event.fields = fields;

	std::lock_guard<std::mutex> lock(m_lock);
	m_queue.push_back(std::move(event));
	return true;
}

bool Telemetry::BeginMatch(const std::string & scenario, uint32 seed, const std::string & extraFields,
	std::string & matchId, std::string & error)
{
	std::lock_guard<std::mutex> lock(m_matchLock);

	if (!m_running.load())
	{
		error = "telemetry is off";
		return false;
	}

	if (m_matchActive)
	{
		error = "a match is already active (" + m_matchId + ")";
		return false;
	}

	if (!IsSafeToken(scenario))
	{
		error = "bad scenario name";
		return false;
	}

	CreateDirectory("Logs", NULL);
	CreateDirectory("Logs/bots", NULL);

	time_t now = time(nullptr);
	struct tm * local = localtime(&now);
	char dir[72];
	snprintf(dir, sizeof(dir), "Logs/bots/%04d-%02d-%02d",
		local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
	CreateDirectory(dir, NULL);

	std::string id;
	std::string path;
	uint32 run = 0;
	bool found = false;
	for (uint32 attempt = 0; attempt < 1000; attempt++)
	{
		run = m_matchRun + 1 + attempt;
		id = scenario + "-" + std::to_string(seed) + "-" + std::to_string(run);
		path = std::string(dir) + "/" + id + ".jsonl";
		std::string summary = std::string(dir) + "/" + id + ".summary.json";
		if (!FileExists(path.c_str()) && !FileExists(summary.c_str()))
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		error = "no free match id";
		return false;
	}

	{
		std::lock_guard<std::mutex> counterLock(m_lock);
		m_matchBaseSoft = m_droppedSoft;
		m_matchBaseHard = m_droppedHard;
	}

	char ts[32];
	FormatUtc(ts, sizeof(ts));
	std::string fields = "\"ts_utc\":\"";
	fields += ts;
	fields += "\",\"scenario\":\"";
	fields += EscapeJson(scenario);
	fields += "\",\"seed\":";
	fields += std::to_string(seed);
	fields += ",\"run\":";
	fields += std::to_string(run);
	if (!extraFields.empty())
	{
		fields += ",";
		fields += extraFields;
	}

	if (!EmitControl(TELCTL_MATCH_START, "MATCH_START", path, fields))
	{
		error = "telemetry is off";
		return false;
	}

	m_matchRun = run;
	m_matchActive = true;
	m_matchId = id;
	m_matchPath = path;
	m_matchStartMs = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
	matchId = id;
	return true;
}

bool Telemetry::EndMatch(const std::string & result, const std::string & extraFields,
	std::string & matchId, long long & durationMs, std::string & error)
{
	std::lock_guard<std::mutex> lock(m_matchLock);
	return EndMatchLocked(result, extraFields, matchId, durationMs, error);
}

bool Telemetry::EndMatchLocked(const std::string & result, const std::string & extraFields,
	std::string & matchId, long long & durationMs, std::string & error)
{
	if (!m_matchActive)
	{
		error = "no active match";
		return false;
	}

	if (!IsSafeToken(result))
	{
		error = "bad result";
		return false;
	}

	long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
	durationMs = nowMs - m_matchStartMs;

	uint64 dropSoft = 0, dropHard = 0;
	{
		std::lock_guard<std::mutex> counterLock(m_lock);
		dropSoft = m_droppedSoft - m_matchBaseSoft;
		dropHard = m_droppedHard - m_matchBaseHard;
	}

	char ts[32];
	FormatUtc(ts, sizeof(ts));
	std::string fields = "\"ts_utc\":\"";
	fields += ts;
	fields += "\",\"duration_ms\":";
	fields += std::to_string(durationMs);
	fields += ",\"result\":\"";
	fields += EscapeJson(result);
	fields += "\",\"dropped_soft\":";
	fields += std::to_string((unsigned long long)dropSoft);
	fields += ",\"dropped_hard\":";
	fields += std::to_string((unsigned long long)dropHard);
	if (!extraFields.empty())
	{
		fields += ",";
		fields += extraFields;
	}

	matchId = m_matchId;
	bool queued = EmitControl(TELCTL_MATCH_END, "MATCH_END", "", fields);

	m_matchActive = false;
	m_matchId.clear();
	m_matchPath.clear();

	if (!queued)
	{
		error = "telemetry is off";
		return false;
	}

	return true;
}

bool Telemetry::IsMatchActive()
{
	std::lock_guard<std::mutex> lock(m_matchLock);
	return m_matchActive;
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

	uint64 pending = 0;

	for (size_t i = 0; i < batch.size(); i++)
	{
		const TelemetryEvent & e = batch[i];

		if (e.ctl == TELCTL_MATCH_START)
		{
			FlushBuffer(buffer, pending, m_matchFile != nullptr ? m_matchFile : m_file);
			OpenMatchFile(e);
		}

		AppendLine(buffer, e);
		pending++;

		if (!m_curMatchId.empty())
		{
			m_matchLines++;
			m_matchCounts[e.ev]++;
		}

		if (e.ctl == TELCTL_MATCH_END)
		{
			FlushBuffer(buffer, pending, m_matchFile != nullptr ? m_matchFile : m_file);
			CloseMatch(e);
		}
	}

	FlushBuffer(buffer, pending, m_matchFile != nullptr ? m_matchFile : m_file);
}

void Telemetry::AppendLine(std::string & buffer, const TelemetryEvent & e)
{
	char head[160];
	snprintf(head, sizeof(head), "{\"t\":%lld,\"match\":\"%s\",\"bot\":%d",
		(long long)e.t, m_curMatchId.empty() ? "-" : EscapeJson(m_curMatchId).c_str(), e.bot);
	buffer += head;

	if (e.ctl == TELCTL_NONE && !e.name.empty())
	{
		buffer += ",\"name\":\"";
		buffer += EscapeJson(e.name);
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

void Telemetry::FlushBuffer(std::string & buffer, uint64 & pending, FILE * target)
{
	if (buffer.empty())
	{
		pending = 0;
		return;
	}

	size_t written = fwrite(buffer.data(), 1, buffer.size(), target);
	fflush(target);

	if (written == buffer.size())
	{
		std::lock_guard<std::mutex> lock(m_lock);
		m_written += pending;
	}
	else
	{
		static bool writeErrorLogged = false;
		if (!writeErrorLogged)
		{
			writeErrorLogged = true;
			WriteTelemetryLog("Telemetry: write error");
		}
	}

	buffer.clear();
	pending = 0;
}

void Telemetry::OpenMatchFile(const TelemetryEvent & e)
{
	m_curMatchPath = e.name;
	m_curStartFields = e.fields;

	size_t slash = m_curMatchPath.find_last_of('/');
	std::string base = slash == std::string::npos ? m_curMatchPath : m_curMatchPath.substr(slash + 1);
	if (base.size() > 6 && base.compare(base.size() - 6, 6, ".jsonl") == 0)
		base = base.substr(0, base.size() - 6);
	m_curMatchId = base;

	m_matchLines = 0;
	m_matchCounts.clear();

	m_matchFile = fopen(e.name.c_str(), "ab");
	if (m_matchFile == nullptr)
	{
		char message[224];
		snprintf(message, sizeof(message),
			"Telemetry: cannot open %s, match lines go to the live file", e.name.c_str());
		WriteTelemetryLog(message);
		return;
	}

	char message[224];
	snprintf(message, sizeof(message), "Telemetry: match %s started, writing %s",
		m_curMatchId.c_str(), e.name.c_str());
	WriteTelemetryLog(message);
}

void Telemetry::CloseMatch(const TelemetryEvent & e)
{
	std::string summary = m_curMatchPath;
	if (summary.size() > 6 && summary.compare(summary.size() - 6, 6, ".jsonl") == 0)
		summary = summary.substr(0, summary.size() - 6) + ".summary.json";

	std::string body = "{\"match\":\"";
	body += EscapeJson(m_curMatchId);
	body += "\",\"mode\":\"live\",\"file\":\"";
	body += EscapeJson(m_curMatchId + ".jsonl");
	body += "\",\"start\":{";
	body += m_curStartFields;
	body += "},\"end\":{";
	body += e.fields;
	body += "},\"lines\":";
	body += std::to_string((unsigned long long)m_matchLines);
	body += ",\"events\":{";

	bool first = true;
	for (std::map<std::string, uint64>::const_iterator it = m_matchCounts.begin();
		it != m_matchCounts.end(); ++it)
	{
		if (!first)
			body += ",";
		first = false;
		body += "\"";
		body += it->first;
		body += "\":";
		body += std::to_string((unsigned long long)it->second);
	}

	body += "}}\n";

	FILE * fp = fopen(summary.c_str(), "wb");
	if (fp == nullptr)
	{
		WriteTelemetryLog("Telemetry: summary write error");
	}
	else
	{
		size_t written = fwrite(body.data(), 1, body.size(), fp);
		if (written != body.size())
			WriteTelemetryLog("Telemetry: summary write error");
		fclose(fp);
	}

	if (m_matchFile != nullptr)
	{
		fclose(m_matchFile);
		m_matchFile = nullptr;
	}

	char message[288];
	snprintf(message, sizeof(message), "Telemetry: match %s ended, %llu lines, summary %s",
		m_curMatchId.c_str(), (unsigned long long)m_matchLines, summary.c_str());
	WriteTelemetryLog(message);

	m_curMatchId.clear();
	m_curMatchPath.clear();
	m_curStartFields.clear();
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
