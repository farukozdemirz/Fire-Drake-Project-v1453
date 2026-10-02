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
}
