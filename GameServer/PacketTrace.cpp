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

	// Opcodes that are safe to trace: combat and movement only. Chat, login,
	// character selection, trade, item moves, mail and party names are never
	// written to the log.
	bool IsTracedOpcode(uint8 opcode)
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
				return true;

			default:
				return false;
		}
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
}

void PacketTrace::LogIncoming(uint16 sid, const char * charName, uint8 zone, const Packet & pkt)
{
	if (!IsTracedOpcode(pkt.GetOpcode()))
		return;

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
	size_t hexLen = (len > 64) ? 64 : len;

	char payloadHex[64 * 2 + 1];
	payloadHex[0] = '\0';
	if (data != nullptr)
	{
		static const char hexDigits[] = "0123456789abcdef";
		for (size_t i = 0; i < hexLen; i++)
		{
			payloadHex[i * 2] = hexDigits[data[i] >> 4];
			payloadHex[i * 2 + 1] = hexDigits[data[i] & 0x0F];
		}
		payloadHex[hexLen * 2] = '\0';
	}

	fprintf(fp, "%lld\t%u\t%s\t%u\t%02x\t%zu\t%s\n",
		t_ms, (unsigned)sid, name, (unsigned)zone, (unsigned)pkt.GetOpcode(),
		pkt.size(), data != nullptr ? payloadHex : "-");
	fflush(fp);
}

#endif
