#pragma once

#include <atomic>
#include <cstdio>
#include <map>
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

// Control events travel in the same queue as normal events, so the writer thread sees
// match boundaries in emit order. They are never dropped (at most two per match).
enum TelemetryControl
{
	TELCTL_NONE = 0,
	TELCTL_MATCH_START = 1,
	TELCTL_MATCH_END = 2
};

// One queued event. 'ev' is a constant code (never freed); the writer thread builds the JSON line.
struct TelemetryEvent
{
	int64 t;               // steady_clock milliseconds
	const char * ev;
	int bot;
	int ctl = 0;           // TelemetryControl
	// MATCH_START only: path of the match .jsonl (not written as a name)
	std::string name;
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

	// Any thread (serialized by an internal mutex; BotManager calls them on the IOCP thread).
	// Opens a match: validates 'scenario' ([A-Za-z0-9_.-]{1,32}, not starting with '.'), picks the
	// first free id "<scenario>-<seed>-<run>" (run = 1,2,... ; skips ids whose Logs/bots/<date>/<id>.jsonl
	// or .summary.json already exists), creates the directory and queues MATCH_START.
	// 'extraFields' is a ready JSON fragment (no braces, no leading comma, may be empty).
	// Returns false and sets 'error' (short English text) when telemetry is off/stopped, a match
	// is already active, or the arguments are invalid.
	bool BeginMatch(const std::string & scenario, uint32 seed, const std::string & extraFields,
		std::string & matchId, std::string & error);
	// Queues MATCH_END for the active match ('result' same character rule as 'scenario',
	// default handled by the caller). Returns false (error "no active match" / "telemetry is off")
	// otherwise. 'durationMs' = steady_clock milliseconds since BeginMatch.
	bool EndMatch(const std::string & result, const std::string & extraFields,
		std::string & matchId, long long & durationMs, std::string & error);
	bool IsMatchActive();
	static std::string EscapeJson(const std::string & in);

private:
	Telemetry();
	static uint32 THREADCALL WriterThreadProc(void * lpParam);
	void WriterLoop();
	void WriteBatch(std::vector<TelemetryEvent> & batch);
	bool RunSelfTest();

	bool EmitControl(int ctl, const char * ev, const std::string & path, const std::string & fields); // ignores limits
	bool EndMatchLocked(const std::string & result, const std::string & extraFields,
		std::string & matchId, long long & durationMs, std::string & error); // m_matchLock held
	void AppendLine(std::string & buffer, const TelemetryEvent & e);
	void FlushBuffer(std::string & buffer, uint64 & pending, FILE * target);
	void OpenMatchFile(const TelemetryEvent & e);   // writer thread
	void CloseMatch(const TelemetryEvent & e);      // writer thread

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

	// Caller-side match state, guarded by m_matchLock (lock order: m_matchLock, then m_lock; never the reverse).
	std::mutex m_matchLock;
	bool m_matchActive;
	std::string m_matchId;
	std::string m_matchPath;            // .jsonl path of the active match
	long long m_matchStartMs;           // steady_clock ms
	uint64 m_matchBaseSoft;             // m_droppedSoft / m_droppedHard when the match began
	uint64 m_matchBaseHard;
	uint32 m_matchRun;                  // last run number handed out (process lifetime)

	// Writer-side match state (writer thread only; Start()/Stop() touch it only while no writer runs).
	FILE * m_matchFile;                 // nullptr = no match file (also when it could not be opened)
	std::string m_curMatchId;           // empty = not inside a match
	std::string m_curMatchPath;
	std::string m_curStartFields;
	uint64 m_matchLines;
	std::map<std::string, uint64> m_matchCounts;   // ev -> lines written in the current match
};
