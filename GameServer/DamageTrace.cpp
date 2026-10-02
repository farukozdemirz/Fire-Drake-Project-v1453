#include "stdafx.h"
#include "DamageTrace.h"

#ifdef FDP_DAMAGE_TRACE

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace
{
	struct TraceContext
	{
		char kind;
		uint32 skillId;
		uint16 casterSid;
	};

	thread_local TraceContext g_context = { 0, 0, 0 };

	std::mutex g_damageMutex;
	FILE * g_pDamageFile = nullptr;
	bool g_bDamageDisabled = false;
	bool g_bHasStartTime = false;
	std::chrono::steady_clock::time_point g_traceStartTime;

	// Lazily opens ./Logs/DamageTrace_<day>_<month>_<year>.log.
	// Returns nullptr (and disables tracing) if the file cannot be opened.
	FILE * GetTraceFile()
	{
		if (g_bDamageDisabled)
			return nullptr;

		if (g_pDamageFile != nullptr)
			return g_pDamageFile;

		time_t now = time(nullptr);
		struct tm * local = localtime(&now);
		char fileName[64];
		snprintf(fileName, sizeof(fileName), "./Logs/DamageTrace_%d_%d_%d.log",
			local->tm_mday, local->tm_mon + 1, local->tm_year + 1900);

		g_pDamageFile = fopen(fileName, "a");
		if (g_pDamageFile == nullptr)
			g_bDamageDisabled = true;

		return g_pDamageFile;
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

DamageTrace::Scope::Scope(char kind, uint32 skillId, uint16 casterSid)
{
	m_prevKind = g_context.kind;
	m_prevSkillId = g_context.skillId;
	m_prevCasterSid = g_context.casterSid;

	g_context.kind = kind;
	g_context.skillId = skillId;
	g_context.casterSid = casterSid;
}

DamageTrace::Scope::~Scope()
{
	g_context.kind = m_prevKind;
	g_context.skillId = m_prevSkillId;
	g_context.casterSid = m_prevCasterSid;
}

void DamageTrace::LogHpChange(Unit * pAttacker, CUser * pTarget, int requested, int hpBefore, int hpAfter)
{
	if (pAttacker == nullptr || !pAttacker->isPlayer() || pTarget == nullptr)
		return;

	std::lock_guard<std::mutex> lock(g_damageMutex);

	FILE * fp = GetTraceFile();
	if (fp == nullptr)
		return;

	if (!g_bHasStartTime)
	{
		g_traceStartTime = std::chrono::steady_clock::now();
		g_bHasStartTime = true;
	}

	long long wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	long long t_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - g_traceStartTime).count();

	CUser * pAttackerUser = TO_USER(pAttacker);
	int primary = (g_context.kind != 0 && pAttacker->GetID() == g_context.casterSid) ? 1 : 0;

	char ctx[32];
	if (g_context.kind == 'R')
		snprintf(ctx, sizeof(ctx), "R");
	else if (g_context.kind == 'S')
		snprintf(ctx, sizeof(ctx), "S%u", (unsigned)g_context.skillId);
	else
		snprintf(ctx, sizeof(ctx), "-");

	char a_name[64], t_name[64];
	SanitizeName(pAttackerUser->GetName().c_str(), a_name, sizeof(a_name));
	SanitizeName(pTarget->GetName().c_str(), t_name, sizeof(t_name));

	fprintf(fp, "%lld\t%lld\t%s\t%d\t%u\t%s\t%u\t%u\t%u\t%u\t%u\t%s\t%u\t%u\t%u\t%u\t%d\t%d\t%d\t%d\n",
		wall_ms, t_ms, ctx, primary,
		(unsigned)pAttacker->GetID(), a_name, (unsigned)pAttackerUser->GetClass(), (unsigned)pAttackerUser->GetNation(), (unsigned)pAttackerUser->GetLevel(), (unsigned)pAttacker->m_sTotalHit,
		(unsigned)pTarget->GetID(), t_name, (unsigned)pTarget->GetClass(), (unsigned)pTarget->GetNation(), (unsigned)pTarget->GetLevel(), (unsigned)pTarget->m_sTotalAc,
		requested, hpAfter - hpBefore, hpBefore, pTarget->GetMaxHealth());
	fflush(fp);
}

#endif
