#pragma once

// Chord walkability guard for the real movement path (F5-61; docs/12 s13.1, CLI-08 "blocked_chord").
// Pure logic: the standard library, NavSegment.h and BotMotion.h only; no global state and no
// dynamic memory. Separate from MoveVerdict (BotMotion.h): that enum is the step-length / speed-field
// rule and keeps its output unchanged.

#include "BotMotion.h"
#include "NavSegment.h"

#include <algorithm>
#include <cmath>

namespace BotCore
{
	// Packet positions are quantised to 0.1 m: the diagonal error of a stop-in-place packet is <= 0.0708 m.
	constexpr float kChordIgnoreMeters = 0.08f;

	enum class ChordVerdict { Ok, Skipped, BlockedCell, OutOfBounds };

	struct ChordResult
	{
		ChordVerdict verdict = ChordVerdict::Skipped;
		int cellX = 0;   // first offending cell; -1 for OutOfBounds
		int cellZ = 0;
		bool startExempt = false;   // the start cell was not Walk and was exempted (D5)
	};

	// grid == nullptr -> Skipped. Chord shorter than kChordIgnoreMeters -> Ok (a bot can always stop).
	// Start cell not Walk -> the chord is checked from the point where it leaves the start cell
	// (moved 1e-3 m past the cell boundary); a chord that ends inside the start cell is Ok.
	// Start/end outside the grid or non-finite -> OutOfBounds. Slope layer off (checkSlope = false).
	inline ChordResult CheckMoveChord(const NavGrid * grid, float x0, float z0, float x1, float z1)
	{
		ChordResult res;

		if (grid == nullptr)
		{
			res.verdict = ChordVerdict::Skipped;
			return res;
		}

		const double ax0 = (double)x0;
		const double az0 = (double)z0;
		const double ax1 = (double)x1;
		const double az1 = (double)z1;
		const double dxw = ax1 - ax0;
		const double dzw = az1 - az0;
		const double length = std::sqrt(dxw * dxw + dzw * dzw);

		// A stop-in-place packet (or any chord shorter than the quantisation diagonal) is always
		// allowed: a bot must always be able to stand still, even on a non-Walk start cell (D3).
		if (std::isfinite(length) && length < (double)kChordIgnoreMeters)
		{
			res.verdict = ChordVerdict::Ok;
			return res;
		}

		const int n = grid->Size();
		const double unit = (double)grid->Unit();
		if (n < 1 || unit <= 0.0)
		{
			res.verdict = ChordVerdict::OutOfBounds;
			res.cellX = -1;
			res.cellZ = -1;
			return res;
		}

		double checkX0 = ax0;
		double checkZ0 = az0;

		// D5: the server can place a bot on a non-Walk cell (spawn scatter). Exempt only the start
		// cell: the chord is checked from where it leaves that cell, so the bot can always walk out.
		if (std::isfinite(ax0) && std::isfinite(az0))
		{
			const int sx = (int)std::floor(ax0 / unit);
			const int sz = (int)std::floor(az0 / unit);
			if (!grid->InBounds(sx, sz))
			{
				res.verdict = ChordVerdict::OutOfBounds;
				res.cellX = -1;
				res.cellZ = -1;
				return res;
			}

			if (!grid->Walk(sx, sz))
			{
				if (std::isfinite(ax1) && std::isfinite(az1))
				{
					const int ex = (int)std::floor(ax1 / unit);
					const int ez = (int)std::floor(az1 / unit);
					if (ex == sx && ez == sz)
					{
						// The chord stays inside the non-Walk start cell: nothing to leave.
						res.verdict = ChordVerdict::Ok;
						return res;
					}
				}

				// Exit point of the chord from the start cell, advanced 1e-3 m past the boundary
				// along the chord direction (length > kChordIgnoreMeters > 0 here).
				const double minX = (double)sx * unit;
				const double maxX = (double)(sx + 1) * unit;
				const double minZ = (double)sz * unit;
				const double maxZ = (double)(sz + 1) * unit;

				double tExit = 1.0;
				if (dxw > 0.0)
					tExit = std::min(tExit, (maxX - ax0) / dxw);
				else if (dxw < 0.0)
					tExit = std::min(tExit, (minX - ax0) / dxw);
				if (dzw > 0.0)
					tExit = std::min(tExit, (maxZ - az0) / dzw);
				else if (dzw < 0.0)
					tExit = std::min(tExit, (minZ - az0) / dzw);

				if (tExit >= 1.0)
				{
					// The chord never leaves the start cell before it ends.
					res.verdict = ChordVerdict::Ok;
					return res;
				}

				const double invLength = 1.0 / length;
				checkX0 = ax0 + dxw * tExit + dxw * invLength * 1e-3;
				checkZ0 = az0 + dzw * tExit + dzw * invLength * 1e-3;
				res.startExempt = true;
			}
		}

		const NavSegmentResult r = NavCheckSegment(*grid, checkX0, checkZ0, ax1, az1, false);
		switch (r.verdict)
		{
		case NavSegmentVerdict::Ok:
			res.verdict = ChordVerdict::Ok;
			break;
		case NavSegmentVerdict::OutOfBounds:
			res.verdict = ChordVerdict::OutOfBounds;
			res.cellX = -1;
			res.cellZ = -1;
			break;
		default:
			res.verdict = ChordVerdict::BlockedCell;
			res.cellX = r.cellX;
			res.cellZ = r.cellZ;
			break;
		}
		return res;
	}
}
