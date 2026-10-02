#pragma once

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

class Thread;

// Minimum level an event needs; Telemetry::m_level >= level lets it through (docs/16 section 3.3).
enum TelemetryLevel
{
	TEL_OFF = 0,
	TEL_SUMMARY = 1,
	TEL_DECISIONS = 2,
	TEL_TRACE = 3
};

// One queued event. 'ev' is a constant code (never freed); the writer thread builds the JSON line.
struct TelemetryEvent
{
	int64 t;               // steady_clock milliseconds
	const char * ev;
	int bot;
	std::string name;      // empty = field omitted
	std::string fields;    // ready JSON fragment
};

struct TelemetryStats
{
	uint64 written;       // lines written to the file
	uint64 droppedSoft;   // droppable events rejected above SOFT_LIMIT
	uint64 droppedHard;   // any event rejected above HARD_LIMIT
	uint32 queueLen;
};

// JSONL telemetry (ADR-0007). Emit() may be called from any thread; the writer thread builds
// the JSON lines and does all disk I/O.
class Telemetry
{
public:
	static constexpr size_t SOFT_LIMIT = 6144;
	static constexpr size_t HARD_LIMIT = 8192;

	static Telemetry & Instance();

	// Reads [BOT] TELEMETRY / TELEMETRY_SELFTEST from GameServer.ini. Level "off": returns true, does
	// nothing. Otherwise creates Logs/bots/<YYYY-MM-DD>/live-<HHMMSS>.jsonl and starts the writer
	// thread. Returns false (and logs one line) when the file cannot be opened; the caller treats
	// that as "telemetry unavailable", never as a startup failure.
	// Called once from BotManager::Startup() on the main thread, before the worker threads start.
	bool Start();
	// Stops accepting events, writes what is queued, joins the writer, closes the file, logs one
	// summary line. Idempotent; safe when Start() never ran.
	void Stop();

	bool IsEnabled(TelemetryLevel needed) const { return m_running.load() && m_level >= needed; }

	// Any thread. 'ev' must be a constant code from docs/16 section 3.2 (not escaped). 'name' may be
	// nullptr (field omitted). 'fields' is a ready JSON fragment WITHOUT braces and without a leading
	// comma, e.g. "\"tick_n\":50,\"queue_len\":0" (may be empty); the caller guarantees it is valid
	// JSON. Returns false when the event was not queued (level too low, stopped, or dropped).
	bool Emit(TelemetryLevel level, const char * ev, int bot, const char * name,
		const std::string & fields, bool droppable);

	TelemetryStats GetStats();

private:
	Telemetry();
	static uint32 THREADCALL WriterThreadProc(void * lpParam);
	void WriterLoop();
	void WriteBatch(std::vector<TelemetryEvent> & batch);
	bool RunSelfTest();

	int m_level;                       // set once in Start(), before the writer thread exists
	std::atomic<bool> m_running;       // Emit() accepts events
	std::atomic<bool> m_stopping;      // writer: drain and exit
	std::atomic<bool> m_paused;        // writer holds its batch (self-test only)
	Thread * m_writerThread;
	FILE * m_file;                     // writer thread only after Start()
	std::string m_filePath;

	std::mutex m_lock;                 // guards m_queue and the three counters below
	std::vector<TelemetryEvent> m_queue;
	uint64 m_written;
	uint64 m_droppedSoft;
	uint64 m_droppedHard;
};
