#include "stdafx.h"
#include "PacketTrace.h"

#ifdef FDP_PACKET_TRACE

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace
{
	std::mutex g_traceMutex;
	FILE * g_pTraceFile = nullptr;
	bool g_bTraceDisabled = false;
	bool g_bHasStartTime = false;
	std::chrono::steady_clock::time_point g_traceStartTime;

	// Incoming opcodes that are safe to trace: combat and movement, plus the
	// party, regene, region-request and chat events needed by the CLI-14..CLI-20
	// timing measurements.
	bool IsTracedIncoming(uint8 opcode)
	{
		switch (opcode)
		{
			case WIZ_MOVE:
			case WIZ_ROTATE:
			case WIZ_ATTACK:
			case WIZ_MAGIC_PROCESS:
			case WIZ_TARGET_HP:
			case WIZ_STATE_CHANGE:
			case WIZ_SPEEDHACK_CHECK:
			case WIZ_PARTY:
			case WIZ_REGENE:
			case WIZ_REQ_USERIN:
			case WIZ_REQ_NPCIN:
			case WIZ_CHAT:
				return true;

			default:
				return false;
		}
	}

	// Outgoing opcodes the server sends to an in-game player (or bot) that are
	// safe to trace: death, region hand-off, regene and party membership.
	bool IsTracedOutgoing(uint8 opcode)
	{
		switch (opcode)
		{
			case WIZ_DEAD:
			case WIZ_REGIONCHANGE:
			case WIZ_NPC_REGION:
			case WIZ_REGENE:
			case WIZ_PARTY:
				return true;

			default:
				return false;
		}
	}

	// White list of payload bytes that may be written to the log. Personal data
	// is never written: chat text, party/character names, item or login data.
	// The caller writes only the first min(kept, 64) bytes, while the len column
	// keeps reporting the real payload size.
	size_t KeptPayloadBytes(bool outgoing, uint8 opcode, const uint8 * data, size_t len)
	{
		// Chat: only the chat type byte, never the message.
		if (!outgoing && opcode == WIZ_CHAT)
			return (len > 1) ? 1 : len;

		// Outgoing party: only the sub-opcode; PARTY_PERMIT / PARTY_INSERT
		// carry a character name.
		if (outgoing && opcode == WIZ_PARTY)
			return (len > 1) ? 1 : len;

		// Incoming party: the sub-opcode, plus the permit flag or the promote /
		// remove target id. PARTY_CREATE / PARTY_INSERT carry a name.
		if (!outgoing && opcode == WIZ_PARTY && data != nullptr && len > 0)
		{
			uint8 sub = data[0];
			if (sub == PARTY_PERMIT)
				return (len > 2) ? 2 : len;
			if (sub == PARTY_PROMOTE || sub == PARTY_REMOVE)
				return (len > 3) ? 3 : len;
			return 1;
		}

		return len;
	}

	// Lazily opens ./Logs/PacketTrace_<day>_<month>_<year>.log.
	// Returns nullptr (and disables tracing) if the file cannot be opened.
	FILE * GetTraceFile()
	{
		if (g_bTraceDisabled)
			return nullptr;

		if (g_pTraceFile != nullptr)
			return g_pTraceFile;

		time_t now = time(nullptr);
		struct tm * local = localtime(&now);
		char fileName[64];
		snprintf(fileName, sizeof(fileName), "./Logs/PacketTrace_%d_%d_%d.log",
			local->tm_mday, local->tm_mon + 1, local->tm_year + 1900);

		g_pTraceFile = fopen(fileName, "a");
		if (g_pTraceFile == nullptr)
			g_bTraceDisabled = true;

		return g_pTraceFile;
	}

	// Replaces tabs, spaces and newlines in the character name with '_'.
	void SanitizeName(const char * charName, char * out, size_t outSize)
	{
		if (charName == nullptr)
			charName = "";

		size_t i = 0;
		while (charName[i] != '\0' && i + 1 < outSize)
		{
			char c = charName[i];
			out[i] = (c == '\t' || c == ' ' || c == '\r' || c == '\n') ? '_' : c;
			i++;
		}
		out[i] = '\0';
	}

	// Shared writer for incoming and outgoing records: same mutex, lazy file,
	// start-time clock and name sanitising for both directions. Outgoing lines
	// get a trailing "out" column; incoming lines stay byte-for-byte unchanged.
	void WriteRecord(uint16 sid, const char * charName, uint8 zone, const Packet & pkt, bool outgoing)
	{
		std::lock_guard<std::mutex> lock(g_traceMutex);

		FILE * fp = GetTraceFile();
		if (fp == nullptr)
			return;

		if (!g_bHasStartTime)
		{
			g_traceStartTime = std::chrono::steady_clock::now();
			g_bHasStartTime = true;
		}

		long long t_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - g_traceStartTime).count();

		char name[64];
		SanitizeName(charName, name, sizeof(name));

		size_t len = pkt.size();
		const uint8 * data = (len > 0) ? pkt.contents() : nullptr;
		size_t kept = KeptPayloadBytes(outgoing, pkt.GetOpcode(), data, len);
		if (kept > 64)
			kept = 64;

		char payloadHex[64 * 2 + 1];
		payloadHex[0] = '\0';
		if (data != nullptr && kept > 0)
		{
			static const char hexDigits[] = "0123456789abcdef";
			for (size_t i = 0; i < kept; i++)
			{
				payloadHex[i * 2] = hexDigits[data[i] >> 4];
				payloadHex[i * 2 + 1] = hexDigits[data[i] & 0x0F];
			}
			payloadHex[kept * 2] = '\0';
		}

		fprintf(fp, "%lld\t%u\t%s\t%u\t%02x\t%zu\t%s%s\n",
			t_ms, (unsigned)sid, name, (unsigned)zone, (unsigned)pkt.GetOpcode(),
			pkt.size(), (kept > 0) ? payloadHex : "-", outgoing ? "\tout" : "");
		fflush(fp);
	}
}

void PacketTrace::LogIncoming(uint16 sid, const char * charName, uint8 zone, const Packet & pkt)
{
	if (!IsTracedIncoming(pkt.GetOpcode()))
		return;

	WriteRecord(sid, charName, zone, pkt, false);
}

void PacketTrace::LogOutgoing(uint16 sid, const char * charName, uint8 zone, const Packet & pkt)
{
	if (!IsTracedOutgoing(pkt.GetOpcode()))
		return;

	WriteRecord(sid, charName, zone, pkt, true);
}

#endif
