#pragma once

// Pure combat logic for the bot fairness guard (ADR-0016 / ADR-0017).
// No server headers: only the standard library (docs/13 section 2).

#include <algorithm>
#include <cstdint>

namespace BotCore
{
	constexpr uint32_t kRMinIntervalMs      = 1000;  // docs/03 MEC-R-07: server allows one hit per second
	constexpr int16_t  kEmptyHandDelayField = 110;   // [A] server only needs delaytime >= 100 without a weapon
	constexpr int16_t  kEmptyHandRangeField = 20;    // [A] 2.0 m, same as a short weapon (distance field unit 0.1 m)
	constexpr int      kMaxActionsPerWindow = 6;     // docs/03 CLI-11: at most 6 non-move actions per second
	constexpr uint32_t kActionWindowMs      = 1000;

	// Minimum time between two R hits (docs/03 CLI-01: weapon Delay * 10 ms, never below 1 s).
	inline uint32_t AttackIntervalMs(bool hasWeapon, uint16_t weaponDelay)
	{
		if (!hasWeapon)
			return kRMinIntervalMs;

		uint32_t interval = uint32_t(weaponDelay) * 10;
		return interval < kRMinIntervalMs ? kRMinIntervalMs : interval;
	}

	// 'delaytime' field of WIZ_ATTACK (docs/03 CLI-01 / T-MECH-CLIENT-02: Delay + 10; kEmptyHandDelayField without weapon).
	inline int16_t AttackDelayField(bool hasWeapon, uint16_t weaponDelay)
	{
		return hasWeapon ? int16_t(weaponDelay + 10) : kEmptyHandDelayField;
	}

	// Weapon range as the 'distance' field limit (MEC-R-04: distance <= m_sRange; kEmptyHandRangeField without weapon).
	inline int16_t AttackRangeField(bool hasWeapon, uint16_t weaponRange)
	{
		return hasWeapon ? int16_t(weaponRange) : kEmptyHandRangeField;
	}

	// 'distance' field of WIZ_ATTACK: metres * 10, truncated, clamped to 0..32767 (T-MECH-CLIENT-02).
	inline int16_t DistanceField(float meters)
	{
		if (meters <= 0.0f)
			return 0;
		if (meters * 10.0f >= 32767.0f)
			return 32767;
		return int16_t(meters * 10.0f);
	}

	enum AttackVerdict
	{
		ATTACK_OK = 0,
		ATTACK_REJECT_OUT_OF_RANGE = 1,  // MEC-R-04: distance field above the weapon range
		ATTACK_REJECT_TOO_SOON = 2,      // CLI-01: previous R hit is younger than the interval
		ATTACK_REJECT_RATE = 3           // CLI-11: 6 actions already in the last second
	};

	// BotFairnessGuard rule for a normal attack. Checks in this order: range, interval, rate.
	// hasLast=false (no earlier hit in this series) skips the interval rule.
	// intervalMs below kRMinIntervalMs counts as kRMinIntervalMs.
	// actionsInWindow = ActionRateWindow::CountInWindow(now).
	inline AttackVerdict CheckAttack(bool hasLast, uint32_t sinceLastMs, uint32_t intervalMs,
		int16_t distanceField, int16_t rangeField, int actionsInWindow)
	{
		if (distanceField < 0 || distanceField > rangeField)
			return ATTACK_REJECT_OUT_OF_RANGE;

		if (hasLast)
		{
			uint32_t minimum = intervalMs < kRMinIntervalMs ? kRMinIntervalMs : intervalMs;
			if (sinceLastMs < minimum)
				return ATTACK_REJECT_TOO_SOON;
		}

		if (actionsInWindow >= kMaxActionsPerWindow)
			return ATTACK_REJECT_RATE;

		return ATTACK_OK;
	}

	// Sliding window over the last kMaxActionsPerWindow action timestamps (CLI-11). Time in ms, any epoch.
	class ActionRateWindow
	{
	public:
		ActionRateWindow()
		{
			Clear();
		}

		// entries with nowMs - t < kActionWindowMs (t <= nowMs)
		int CountInWindow(uint64_t nowMs) const
		{
			int count = 0;
			for (int i = 0; i < m_count; i++)
			{
				uint64_t t = m_times[i];
				if (nowMs >= t && nowMs - t < kActionWindowMs)
					count++;
			}
			return count;
		}

		// overwrites the oldest entry once kMaxActionsPerWindow are stored
		void Record(uint64_t nowMs)
		{
			m_times[m_next] = nowMs;
			m_next = (m_next + 1) % kMaxActionsPerWindow;
			if (m_count < kMaxActionsPerWindow)
				m_count++;
		}

		void Clear()
		{
			m_count = 0;
			m_next = 0;
		}

	private:
		uint64_t m_times[kMaxActionsPerWindow];
		int m_count;   // entries stored, 0..kMaxActionsPerWindow
		int m_next;    // ring index of the next write
	};

	// --- cast slice (ADR-0017 Ek F4-03) ---

	constexpr uint32_t kCastExtraMs = 80;    // docs/03 CLI-03: CASTING -> EFFECTING = CastTime*100 + 70..90 ms (measured)
	constexpr uint32_t kCastGapMs   = 140;   // docs/03 CLI-04: 135..140 ms between EFFECTING and the next CASTING (measured)
	constexpr uint32_t kTypeGateMs  = 1000;  // docs/03 MEC-MAG-03 / MEC-MAG-10: same type, skill id < 400000

	// Time the bot waits between CASTING and EFFECTING; 0 when the skill has no cast time (no CASTING packet then).
	inline uint32_t CastDurationMs(uint8_t castTime)
	{
		return castTime == 0 ? 0 : uint32_t(castTime) * 100 + kCastExtraMs;
	}

	// Per-skill reuse time (MEC-MAG-02, CLI-04): ReCastTime is in 0.1 s units, real time, no whole-second rounding.
	inline uint32_t CastRecastMs(uint16_t reCastTime)
	{
		return uint32_t(reCastTime) * 100;
	}

	// Skill range (MEC-MAG-11): skillRange (metres, MAGIC.Range) > 0 -> distanceM < skillRange (the server rejects >=);
	// skillRange == 0 (weapon-bound Type1) -> 0 <= distanceField <= weaponRangeField (same 0.1 m field as R).
	inline bool CastInRange(float distanceM, uint16_t skillRange, int16_t distanceField, int16_t weaponRangeField)
	{
		if (skillRange > 0)
			return distanceM < float(skillRange);

		return distanceField >= 0 && distanceField <= weaponRangeField;
	}

	struct CastStartCheck
	{
		float distanceM;            // caster -> target, metres (0 for a self cast)
		uint16_t skillRange;        // MAGIC.Range
		int16_t distanceField;      // DistanceField(distanceM)
		int16_t weaponRangeField;   // AttackRangeField(...)
		bool needsStanding;         // MAGIC.UseStanding == 1
		bool standing;              // the bot has no walk in progress
		int32_t mana;               // caster's current MP
		uint16_t msp;               // MAGIC.Msp
		uint32_t reCastMs;          // CastRecastMs(MAGIC.ReCastTime)
		bool hasSkillLast;          // this skill was effected earlier in this spawn
		uint32_t sinceSkillLastMs;
		bool typeGated;             // MAGIC.Type1/Type2 in 1..7 and skill id < 400000 (MEC-MAG-03)
		bool hasTypeLast;           // one of the skill's gated types was effected earlier
		uint32_t sinceTypeLastMs;   // the smallest "since" over the skill's gated types
		bool hasAnyLast;            // any skill was effected earlier in this spawn
		uint32_t sinceAnyLastMs;
		int actionsInWindow;        // ActionRateWindow::CountInWindow(now)
	};

	enum CastVerdict
	{
		CAST_OK = 0,
		CAST_REJECT_OUT_OF_RANGE = 1,   // MEC-MAG-11
		CAST_REJECT_NOT_STANDING = 2,   // CLI-09 / MEC-MAG-07
		CAST_REJECT_NO_MANA = 3,        // MEC-MAG-08
		CAST_REJECT_RECAST = 4,         // CLI-04 (per skill)
		CAST_REJECT_TYPE_GATE = 5,      // MEC-MAG-03
		CAST_REJECT_GAP = 6,            // CLI-04 (gap after the previous EFFECTING)
		CAST_REJECT_RATE = 7,           // CLI-11
		CAST_REJECT_TOO_EARLY = 8       // CLI-03 (EFFECTING before the cast time ran out)
	};

	// Milliseconds until the timing rules (recast, type gate, gap) allow the next cast to start; 0 = now.
	// Range, standing, mana and rate are NOT timing waits and are not part of this value.
	inline uint32_t CastWaitMs(const CastStartCheck & c)
	{
		uint32_t wait = 0;

		if (c.hasSkillLast && c.sinceSkillLastMs < c.reCastMs)
		{
			uint32_t w = c.reCastMs - c.sinceSkillLastMs;
			if (w > wait)
				wait = w;
		}

		if (c.typeGated && c.hasTypeLast && c.sinceTypeLastMs < kTypeGateMs)
		{
			uint32_t w = kTypeGateMs - c.sinceTypeLastMs;
			if (w > wait)
				wait = w;
		}

		if (c.hasAnyLast && c.sinceAnyLastMs < kCastGapMs)
		{
			uint32_t w = kCastGapMs - c.sinceAnyLastMs;
			if (w > wait)
				wait = w;
		}

		return wait;
	}

	// Guard rule for the first packet of a cast (CASTING, or EFFECTING when there is no cast time).
	// Order: range, standing, mana, recast, type gate, gap, rate.
	inline CastVerdict CheckCastStart(const CastStartCheck & c)
	{
		if (!CastInRange(c.distanceM, c.skillRange, c.distanceField, c.weaponRangeField))
			return CAST_REJECT_OUT_OF_RANGE;

		if (c.needsStanding && !c.standing)
			return CAST_REJECT_NOT_STANDING;

		if (c.mana < int32_t(c.msp))
			return CAST_REJECT_NO_MANA;

		if (c.hasSkillLast && c.sinceSkillLastMs < c.reCastMs)
			return CAST_REJECT_RECAST;

		if (c.typeGated && c.hasTypeLast && c.sinceTypeLastMs < kTypeGateMs)
			return CAST_REJECT_TYPE_GATE;

		if (c.hasAnyLast && c.sinceAnyLastMs < kCastGapMs)
			return CAST_REJECT_GAP;

		if (c.actionsInWindow >= kMaxActionsPerWindow)
			return CAST_REJECT_RATE;

		return CAST_OK;
	}

	// Guard rule for the EFFECTING packet. Order: too early, range, rate.
	inline CastVerdict CheckCastEffect(bool inRange, uint32_t sinceCastingMs, uint8_t castTime, int actionsInWindow)
	{
		if (sinceCastingMs < CastDurationMs(castTime))
			return CAST_REJECT_TOO_EARLY;

		if (!inRange)
			return CAST_REJECT_OUT_OF_RANGE;

		if (actionsInWindow >= kMaxActionsPerWindow)
			return CAST_REJECT_RATE;

		return CAST_OK;
	}

	// --- potion slice (ADR-0017 Ek F4-04) ---

	constexpr uint32_t kPotCooldownMs = 2500;   // docs/03 CLI-06: HP and MP pots share ~2.5 s (measured 2504..2665 ms; HP->MP 2540 ms) [A: shared timer]

	// Supported pots: the server's per-skill recast (MAGIC.ReCastTime, 0.1 s units) never exceeds the shared timer,
	// so no per-skill bookkeeping is needed. Longer-recast items are out of scope (unsupported_item).
	inline bool PotSupported(uint16_t reCastTime)
	{
		return CastRecastMs(reCastTime) <= kPotCooldownMs;
	}

	struct PotionCheck
	{
		uint32_t stock;           // count of the pot item in the bot's own bag
		bool hasLast;             // any pot packet was sent earlier in this spawn
		uint32_t sinceLastMs;     // since that packet
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum PotionVerdict
	{
		POT_OK = 0,
		POT_REJECT_NO_STOCK = 1,   // CLI-06 (no item in the bag; MB-01 pots included)
		POT_REJECT_COOLDOWN = 2,   // CLI-06 (shared 2.5 s timer)
		POT_REJECT_RATE = 3        // CLI-11
	};

	// Milliseconds until the shared pot timer allows the next pot; 0 = now. Stock and rate are not timing waits.
	inline uint32_t PotionWaitMs(const PotionCheck & c)
	{
		return (c.hasLast && c.sinceLastMs < kPotCooldownMs) ? kPotCooldownMs - c.sinceLastMs : 0;
	}

	// Guard rule for a pot packet. Order: no stock, cooldown, rate.
	inline PotionVerdict CheckPotion(const PotionCheck & c)
	{
		if (c.stock < 1)
			return POT_REJECT_NO_STOCK;

		if (c.hasLast && c.sinceLastMs < kPotCooldownMs)
			return POT_REJECT_COOLDOWN;

		if (c.actionsInWindow >= kMaxActionsPerWindow)
			return POT_REJECT_RATE;

		return POT_OK;
	}

	// --- stance slice (ADR-0017 Ek F4-05) ---

	constexpr uint32_t kStanceToggleMinMs = 1000;   // docs/03 CLI-13: two stance packets at least 1.0 s apart [A: conservative]

	struct StanceCheck
	{
		bool toSit;               // true = sit down, false = stand up
		bool busy;                // a walk, an attack series or a cast series is in progress (rule applies to toSit only)
		bool hasLast;             // a stance packet was sent earlier in this spawn
		uint32_t sinceLastMs;     // since that packet
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum StanceVerdict
	{
		STANCE_OK = 0,
		STANCE_REJECT_BUSY = 1,     // CLI-13 (sitting down while a walk/attack/cast series runs)
		STANCE_REJECT_TOGGLE = 2,   // CLI-13 (previous stance packet younger than kStanceToggleMinMs)
		STANCE_REJECT_RATE = 3      // CLI-11
	};

	// Guard rule for a stance packet. Order: busy (toSit only), toggle interval, rate.
	// "Already in the requested stance" is NOT a guard rule: the executor refuses it as "no_change" before this call.
	inline StanceVerdict CheckStance(const StanceCheck & c)
	{
		if (c.toSit && c.busy)
			return STANCE_REJECT_BUSY;

		if (c.hasLast && c.sinceLastMs < kStanceToggleMinMs)
			return STANCE_REJECT_TOGGLE;

		if (c.actionsInWindow >= kMaxActionsPerWindow)
			return STANCE_REJECT_RATE;

		return STANCE_OK;
	}

	// --- target HP slice (ADR-0017 Ek F4-06) ---

	constexpr uint32_t kTargetHpPollMs   = 2000;   // docs/03 CLI-10 / Q-18: the client polls the selected target every 2.0 s (p50 2001 ms) [A]
	constexpr int      kViewDistance     = 48;     // server VIEW_DISTANCE (globals.h): region edge in metres
	constexpr int      kViewRegionRadius = 1;      // the client sees the 3x3 regions around its own (docs/03 section 16)

	// Region index of a coordinate: the server's (uint16)(coord) / VIEW_DISTANCE (Unit.h GetNewRegionX/Z).
	// Clamped to 0..65535 because the server's cast is only defined there.
	inline int RegionIndex(float coord)
	{
		if (coord < 0.0f)
			return 0;
		if (coord > 65535.0f)
			coord = 65535.0f;

		return (int)((uint16_t)coord) / kViewDistance;
	}

	// Chebyshev distance between the region indices of two positions (0 = same region, 1 = adjacent, incl. diagonal).
	inline int RegionDelta(float ax, float az, float bx, float bz)
	{
		int dx = RegionIndex(ax) - RegionIndex(bx);
		int dz = RegionIndex(az) - RegionIndex(bz);
		if (dx < 0)
			dx = -dx;
		if (dz < 0)
			dz = -dz;

		return dx > dz ? dx : dz;
	}

	struct TargetHpCheck
	{
		int regionDelta;          // RegionDelta(bot position, target position)
		bool sameTarget;          // the request re-polls the currently selected target (same id as the previous request)
		bool hasLast;             // a target HP request was sent earlier in this spawn
		uint32_t sinceLastMs;     // since that request
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum TargetHpVerdict
	{
		TARGETHP_OK = 0,
		TARGETHP_REJECT_VIEW = 1,   // CLI-10 (target outside the 3x3 regions)
		TARGETHP_REJECT_POLL = 2,   // CLI-10 (same target polled again before kTargetHpPollMs)
		TARGETHP_REJECT_RATE = 3    // CLI-11
	};

	// Guard rule for a target HP request. Order: view, poll, rate.
	// Selecting a different target is not rate limited by CLI-10 (a human can click quickly); CLI-11 still applies.
	inline TargetHpVerdict CheckTargetHp(const TargetHpCheck & c)
	{
		if (c.regionDelta > kViewRegionRadius)
			return TARGETHP_REJECT_VIEW;

		if (c.sameTarget && c.hasLast && c.sinceLastMs < kTargetHpPollMs)
			return TARGETHP_REJECT_POLL;

		if (c.actionsInWindow >= kMaxActionsPerWindow)
			return TARGETHP_REJECT_RATE;

		return TARGETHP_OK;
	}

	// --- respawn slice (ADR-0017 Ek F4-07) ---

	constexpr uint32_t kRegeneMinDeadMs = 3000;   // docs/03 CLI-14: the death screen and the respawn button take a human at least this long [A] (unmeasured)

	struct RegeneCheck
	{
		uint32_t sinceDeadMs;     // since the bot's death was first noticed
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum RegeneVerdict
	{
		REGENE_OK = 0,
		REGENE_REJECT_WAIT = 1,   // CLI-14 (respawn requested before kRegeneMinDeadMs after the death)
		REGENE_REJECT_RATE = 2    // CLI-11
	};

	// Guard rule for a respawn request. The caller has already checked that the bot is dead. Order: wait, rate.
	inline RegeneVerdict CheckRegene(const RegeneCheck & c)
	{
		if (c.sinceDeadMs < kRegeneMinDeadMs)
			return REGENE_REJECT_WAIT;

		if (c.actionsInWindow >= kMaxActionsPerWindow)
			return REGENE_REJECT_RATE;

		return REGENE_OK;
	}

	// --- party slice (ADR-0017 Ek F4-08) ---

	constexpr uint32_t kPartyInviteGapMs = 1000;   // docs/03 CLI-15: a human needs at least this long between two party invitations [A] (unmeasured)
	constexpr uint32_t kPartyAcceptMinMs = 1000;   // docs/03 CLI-15: reading the invitation popup and clicking accept takes a human at least this long [A] (unmeasured)

	struct PartyInviteCheck
	{
		bool inParty;             // the bot is in a party (it was invited or it leads one)
		bool isLeader;            // ... and it leads it
		int regionDelta;          // RegionDelta(bot position, target position)
		bool hasLast;             // a party invitation was sent earlier in this spawn
		uint32_t sinceLastMs;     // since that invitation
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum PartyInviteVerdict
	{
		PARTYINVITE_OK = 0,
		PARTYINVITE_REJECT_LEADER = 1,   // CLI-15 (in a party but not its leader: only the leader invites)
		PARTYINVITE_REJECT_VIEW = 2,     // CLI-15 (target outside the 3x3 regions)
		PARTYINVITE_REJECT_GAP = 3,      // CLI-15 (second invitation before kPartyInviteGapMs)
		PARTYINVITE_REJECT_RATE = 4      // CLI-11
	};

	// Guard rule for a party invitation. Order: leader, view, gap, rate.
	inline PartyInviteVerdict CheckPartyInvite(const PartyInviteCheck & c)
	{
		if (c.inParty && !c.isLeader)
			return PARTYINVITE_REJECT_LEADER;

		if (c.regionDelta > kViewRegionRadius)
			return PARTYINVITE_REJECT_VIEW;

		if (c.hasLast && c.sinceLastMs < kPartyInviteGapMs)
			return PARTYINVITE_REJECT_GAP;

		if (c.actionsInWindow >= kMaxActionsPerWindow)
			return PARTYINVITE_REJECT_RATE;

		return PARTYINVITE_OK;
	}

	struct PartyAcceptCheck
	{
		uint32_t sinceInviteMs;   // since the invitation reached the bot
		int actionsInWindow;      // ActionRateWindow::CountInWindow(now)
	};

	enum PartyAcceptVerdict
	{
		PARTYACCEPT_OK = 0,
		PARTYACCEPT_REJECT_WAIT = 1,   // CLI-15 (accepted before kPartyAcceptMinMs after the invitation arrived)
		PARTYACCEPT_REJECT_RATE = 2    // CLI-11
	};

	// Guard rule for accepting an invitation. The caller has already checked that an invitation is pending. Order: wait, rate.
	inline PartyAcceptVerdict CheckPartyAccept(const PartyAcceptCheck & c)
	{
		if (c.sinceInviteMs < kPartyAcceptMinMs)
			return PARTYACCEPT_REJECT_WAIT;

		if (c.actionsInWindow >= kMaxActionsPerWindow)
			return PARTYACCEPT_REJECT_RATE;

		return PARTYACCEPT_OK;
	}
}
