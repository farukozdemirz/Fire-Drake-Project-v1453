#include "MiniTest.h"

#include <BotCore/SpawnGate.h>

#include <cstdint>
#include <vector>

TEST_CASE("SpawnGate_RuleOpenOnlyWhenIdle")
{
	CHECK(BotCore::HandshakeGateOpen(0));
	CHECK(!BotCore::HandshakeGateOpen(1));
	CHECK(!BotCore::HandshakeGateOpen(3));
}

TEST_CASE("SpawnGate_OverlapReproducesOneWayView")
{
	// F4-54 run 1 (spawn order WP, MF, PHD, 100 ms stagger, no serialization). The 250 ms
	// handshake interval keeps the strict window op1(X) < op2(Y) < op2(X) and reproduces the
	// documented one-way set {1!0, 2!0, 2!1}: MF misses WP, PHD misses WP and MF.
	const uint64_t op1[3] = { 0, 100, 200 };
	const uint64_t op2[3] = { 250, 350, 450 };

	int misses = 0;
	for (int x = 0; x < 3; ++x)
	{
		for (int y = 0; y < 3; ++y)
		{
			if (x == y)
				continue;

			bool xMissesY = BotCore::HandshakeMissesUnit(op1[x], op2[x], op2[y]);
			bool yMissesX = BotCore::HandshakeMissesUnit(op1[y], op2[y], op2[x]);

			// The server model never hides a pair in both directions.
			CHECK(!(xMissesY && yMissesX));

			if (xMissesY)
				misses++;

			bool expected = (x == 1 && y == 0) || (x == 2 && y == 0) || (x == 2 && y == 1);
			CHECK_EQ(bool(xMissesY), bool(expected));
		}
	}

	CHECK_EQ(misses, 3);
}

namespace
{
	struct GateRun
	{
		std::vector<uint64_t> op1;
		std::vector<uint64_t> op2;
		size_t maxInHandshake;
		uint64_t finishMs;

		bool missed(int x, int y) const
		{
			return BotCore::HandshakeMissesUnit(op1[x], op2[x], op2[y]);
		}
	};

	// Deterministic tick model of the login handshake (CharacterSelectionHandler.cpp GameStart
	// 1/2). Sessions become select-ready 100 ms apart (single spawn command), send GameStart(1)
	// when the gate admits them, and send GameStart(2) 200 ms later. Completion is processed
	// before new handshakes start, so a session that registers and the next one that snapshots
	// at the same tick are ordered (the new snapshot already contains the registration).
	GateRun SimulateGate(int n, bool alwaysOpen)
	{
		enum Phase { HAZIR, EL_SIKISMADA, BITTI };
		const uint64_t tick = 100;
		const uint64_t readyStep = 100;
		const uint64_t interval = 200;

		std::vector<Phase> st(n, HAZIR);
		GateRun run;
		run.op1.assign(n, 0);
		run.op2.assign(n, 0);
		run.maxInHandshake = 0;
		run.finishMs = 0;

		int done = 0;
		for (uint64_t t = 0; done < n; t += tick)
		{
			for (int i = 0; i < n; ++i)
			{
				if (st[i] == EL_SIKISMADA && t - run.op1[i] >= interval)
				{
					run.op2[i] = t;
					st[i] = BITTI;
					done++;
				}
			}

			size_t inHandshake = 0;
			for (int i = 0; i < n; ++i)
			{
				if (st[i] == EL_SIKISMADA)
					inHandshake++;
			}
			if (inHandshake > run.maxInHandshake)
				run.maxInHandshake = inHandshake;

			for (int i = 0; i < n; ++i)
			{
				if (st[i] != HAZIR || t < uint64_t(i) * readyStep)
					continue;

				if (!alwaysOpen && !BotCore::HandshakeGateOpen((int)inHandshake))
					continue;

				run.op1[i] = t;
				st[i] = EL_SIKISMADA;
				inHandshake++;
			}
		}

		for (int i = 0; i < n; ++i)
		{
			if (run.op2[i] > run.finishMs)
				run.finishMs = run.op2[i];
		}
		return run;
	}
}

TEST_CASE("SpawnGate_GatedLoopIsSymmetric")
{
	const int ns[4] = { 2, 3, 6, 16 };
	for (int k = 0; k < 4; ++k)
	{
		int n = ns[k];
		GateRun gated = SimulateGate(n, false);

		for (int x = 0; x < n; ++x)
		{
			for (int y = 0; y < n; ++y)
			{
				if (x == y)
					continue;
				CHECK(!gated.missed(x, y));
			}
		}

		CHECK_EQ(int(gated.maxInHandshake), 1);
		CHECK(gated.finishMs < uint64_t(n) * 300);

		// Without the gate the same model reproduces the asymmetry (the test is not vacuous).
		GateRun open = SimulateGate(n, true);
		bool anyMiss = false;
		for (int x = 0; x < n && !anyMiss; ++x)
		{
			for (int y = 0; y < n; ++y)
			{
				if (x != y && open.missed(x, y))
				{
					anyMiss = true;
					break;
				}
			}
		}
		CHECK(anyMiss);
	}
}
