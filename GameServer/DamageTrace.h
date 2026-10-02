#pragma once

#ifdef FDP_DAMAGE_TRACE

class Unit;
class CUser;

namespace DamageTrace
{
	// Per-thread "what is currently being executed" context. Set by CUser::Attack
	// (kind 'R') and MagicInstance::ExecuteSkill (kind 'S'); restores the previous
	// context on destruction.
	class Scope
	{
	public:
		Scope(char kind, uint32 skillId, uint16 casterSid);
		~Scope();
		Scope(const Scope &) = delete;
		Scope & operator=(const Scope &) = delete;

	private:
		char m_prevKind;
		uint32 m_prevSkillId;
		uint16 m_prevCasterSid;
	};

	// Writes one line when a player changed another player's (or his own) HP.
	// Called from CUser::HpChange (IOCP worker or timer thread).
	void LogHpChange(Unit * pAttacker, CUser * pTarget, int requested, int hpBefore, int hpAfter);
}

#endif
