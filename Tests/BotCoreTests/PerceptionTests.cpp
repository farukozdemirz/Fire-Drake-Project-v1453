#include "MiniTest.h"

#include <BotCore/Perception.h>

#include <cstring>
#include <string>
#include <vector>

namespace
{
	// Small-endian payload builder.
	struct Buf
	{
		std::vector<uint8_t> v;

		void U8(uint8_t value) { v.push_back(value); }

		void U16(uint16_t value)
		{
			v.push_back((uint8_t)(value & 0xFF));
			v.push_back((uint8_t)(value >> 8));
		}

		void U32(uint32_t value)
		{
			v.push_back((uint8_t)(value & 0xFF));
			v.push_back((uint8_t)((value >> 8) & 0xFF));
			v.push_back((uint8_t)((value >> 16) & 0xFF));
			v.push_back((uint8_t)((value >> 24) & 0xFF));
		}

		void Str(const char * text)
		{
			uint8_t n = (uint8_t)strlen(text);
			U8(n);
			for (uint8_t i = 0; i < n; i++)
				U8((uint8_t)text[i]);
		}
	};

	// Writes the 20-field user info in the exact wire order the server produces.
	void AddUserInfo(Buf & b, const char * name, uint8_t nation, int16_t clan, bool withClanBlock,
		uint8_t level, uint8_t race, uint16_t cls, uint16_t x10, uint16_t z10, uint16_t y10,
		uint8_t resHp, bool leader, uint8_t invis)
	{
		b.Str(name);
		b.U8(nation);
		b.U16((uint16_t)clan);
		b.U8(0);                              // fame
		if (withClanBlock)
		{
			b.U16(7);                         // alliance id
			b.Str("Dragons");                 // clan name
			b.U8(3);                          // grade
			b.U8(2);                          // ranking
			b.U16(5);                         // mark version
			b.U16(9);                         // cape id
		}
		else
		{
			b.U32(0);                         // no-clan form (9 bytes, same size)
			b.U16(0);
			b.U8(0);
			b.U16(0xFFFF);
		}
		b.U8(level);
		b.U8(race);
		b.U16(cls);
		b.U16(x10);
		b.U16(z10);
		b.U16(y10);
		b.U8(0);                              // face
		b.U8(0);                              // hair
		b.U8(resHp);
		b.U32(0);                             // abnormal status
		b.U8(0);                              // party search
		b.U8(0);                              // authority
		b.U8(leader ? 1 : 0);                 // party leader
		b.U8(invis);                          // invisibility
		b.U8(0);                              // helmet hidden
		b.U16(0);                             // direction
		b.U8(0); b.U8(0); b.U8(0); b.U8(0);   // chicken, king, knight rank, personal rank
		for (int i = 0; i < 10; i++)
		{
			b.U32(0);                         // item num
			b.U16(0);                         // durability
			b.U8(0);                          // flag
		}
		b.U8(71);                             // zone id
	}
}

TEST_CASE("Perception_UserInfo_NoClan")
{
	Buf b;
	AddUserInfo(b, "BotWP_K", 1, 0, false, 80, 11, 105, 12740, 8900, 123, 1, true, 0);

	BotCore::ByteReader r(b.v.data(), b.v.size());
	BotCore::UnitObs out;
	memset(&out, 0, sizeof(out));

	CHECK(BotCore::ParseUserInfo(r, 42, 123456, out));
	CHECK_EQ(int(r.remaining()), 0);
	CHECK_EQ(int(out.sid), 42);
	CHECK(strcmp(out.name, "BotWP_K") == 0);
	CHECK_EQ(int(out.nation), 1);
	CHECK_EQ(int(out.level), 80);
	CHECK_EQ(int(out.race), 11);
	CHECK_EQ(int(out.cls), 105);
	CHECK_EQ(int(out.x10), 12740);
	CHECK_EQ(int(out.z10), 8900);
	CHECK_EQ(int(out.y10), 123);
	CHECK_EQ(int(out.resHpType), 1);
	CHECK(out.partyLeader);
	CHECK_EQ(int(out.invisibility), 0);
	CHECK(out.lastSeenMs == 123456);
}

TEST_CASE("Perception_UserInfo_WithClan")
{
	Buf b;
	AddUserInfo(b, "BotMF_E", 2, 5, true, 65, 12, 110, 100, 200, 3, 3, false, 1);

	BotCore::ByteReader r(b.v.data(), b.v.size());
	BotCore::UnitObs out;
	memset(&out, 0, sizeof(out));

	CHECK(BotCore::ParseUserInfo(r, 77, 999, out));
	CHECK_EQ(int(r.remaining()), 0);
	CHECK_EQ(int(out.sid), 77);
	CHECK(strcmp(out.name, "BotMF_E") == 0);
	CHECK_EQ(int(out.nation), 2);
	CHECK_EQ(int(out.level), 65);
	CHECK_EQ(int(out.race), 12);
	CHECK_EQ(int(out.cls), 110);
	CHECK_EQ(int(out.x10), 100);
	CHECK_EQ(int(out.z10), 200);
	CHECK_EQ(int(out.y10), 3);
	CHECK_EQ(int(out.resHpType), 3);
	CHECK(!out.partyLeader);
	CHECK_EQ(int(out.invisibility), 1);
	CHECK(out.lastSeenMs == 999);
}

TEST_CASE("Perception_UserInfo_Truncated")
{
	Buf full;
	AddUserInfo(full, "BotWP_K", 1, 0, false, 80, 11, 105, 12740, 8900, 123, 1, true, 0);

	for (size_t len = 0; len < full.v.size(); len++)
	{
		BotCore::ByteReader r(full.v.data(), len);
		BotCore::UnitObs out;
		CHECK(!BotCore::ParseUserInfo(r, 1, 0, out));
	}

	Buf tooLong;
	AddUserInfo(tooLong, "abcdefghijklmnopqrstuvwx", 1, 0, false, 80, 11, 105, 1, 2, 3, 1, false, 0);
	{
		BotCore::ByteReader r(tooLong.v.data(), tooLong.v.size());
		BotCore::UnitObs out;
		CHECK(!BotCore::ParseUserInfo(r, 1, 0, out));
	}

	Buf exactlyMax;
	AddUserInfo(exactlyMax, "abcdefghijklmnopqrstuvw", 1, 0, false, 80, 11, 105, 1, 2, 3, 1, false, 0);
	{
		BotCore::ByteReader r(exactlyMax.v.data(), exactlyMax.v.size());
		BotCore::UnitObs out;
		memset(&out, 0, sizeof(out));
		CHECK(BotCore::ParseUserInfo(r, 1, 0, out));
		CHECK_EQ(int(strlen(out.name)), 23);
	}

	{
		BotCore::ByteReader r(nullptr, 0);
		BotCore::UnitObs out;
		CHECK(!BotCore::ParseUserInfo(r, 1, 0, out));
	}
}

TEST_CASE("Perception_ParseUserInOut")
{
	const uint8_t inTypes[4] = { 1, 3, 4, 5 };
	for (int i = 0; i < 4; i++)
	{
		Buf b;
		b.U16((uint16_t)inTypes[i]);
		b.U16(100);
		AddUserInfo(b, "BotWP_K", 1, 0, false, 80, 11, 105, 12740, 8900, 123, 1, true, 0);

		uint16_t type = 0;
		BotCore::UnitObs out;
		memset(&out, 0, sizeof(out));
		CHECK(BotCore::ParseUserInOut(b.v.data(), b.v.size(), 555, type, out));
		CHECK_EQ(int(type), int(inTypes[i]));
		CHECK_EQ(int(out.sid), 100);
		CHECK(strcmp(out.name, "BotWP_K") == 0);
	}

	{
		Buf b;
		b.U16(2);
		b.U16(77);
		uint16_t type = 0;
		BotCore::UnitObs out;
		memset(&out, 0, sizeof(out));
		CHECK(BotCore::ParseUserInOut(b.v.data(), b.v.size(), 555, type, out));
		CHECK_EQ(int(type), 2);
		CHECK_EQ(int(out.sid), 77);
	}

	{
		Buf b;
		b.U16(1);
		b.U16(9);
		uint16_t type = 0;
		BotCore::UnitObs out;
		CHECK(!BotCore::ParseUserInOut(b.v.data(), b.v.size(), 555, type, out));
	}

	{
		Buf b;
		b.U8(0); b.U8(0); b.U8(1);
		uint16_t type = 0;
		BotCore::UnitObs out;
		CHECK(!BotCore::ParseUserInOut(b.v.data(), b.v.size(), 555, type, out));
	}
}

TEST_CASE("Perception_ParseUserList")
{
	{
		Buf b;
		b.U16(2);
		b.U8(0); b.U16(10);
		AddUserInfo(b, "BotAA", 1, 0, false, 50, 1, 100, 1000, 2000, 30, 1, false, 0);
		b.U8(0); b.U16(20);
		AddUserInfo(b, "BotBB", 2, 5, true, 60, 2, 101, 3000, 4000, 50, 1, true, 1);

		BotCore::UnitObs list[BotCore::kObsMaxUnits];
		int n = BotCore::ParseUserList(b.v.data(), b.v.size(), 777, list, BotCore::kObsMaxUnits);
		CHECK_EQ(n, 2);
		if (n == 2)
		{
			CHECK_EQ(int(list[0].sid), 10);
			CHECK(strcmp(list[0].name, "BotAA") == 0);
			CHECK_EQ(int(list[0].level), 50);
			CHECK(!list[0].partyLeader);
			CHECK_EQ(int(list[1].sid), 20);
			CHECK(strcmp(list[1].name, "BotBB") == 0);
			CHECK_EQ(int(list[1].nation), 2);
			CHECK(list[1].partyLeader);
			CHECK_EQ(int(list[1].invisibility), 1);
			CHECK(list[1].lastSeenMs == 777);
		}
	}

	{
		Buf b;
		b.U16(3);
		b.U8(0); b.U16(10);
		AddUserInfo(b, "BotAA", 1, 0, false, 50, 1, 100, 1000, 2000, 30, 1, false, 0);
		b.U8(0); b.U16(20);
		AddUserInfo(b, "BotBB", 2, 0, false, 60, 2, 101, 3000, 4000, 50, 1, false, 0);

		BotCore::UnitObs list[BotCore::kObsMaxUnits];
		CHECK_EQ(BotCore::ParseUserList(b.v.data(), b.v.size(), 0, list, BotCore::kObsMaxUnits), 2);
		CHECK_EQ(BotCore::ParseUserList(b.v.data(), b.v.size(), 0, list, 1), 1);
	}

	{
		Buf b;
		b.U16(0);
		BotCore::UnitObs list[BotCore::kObsMaxUnits];
		CHECK_EQ(BotCore::ParseUserList(b.v.data(), b.v.size(), 0, list, BotCore::kObsMaxUnits), 0);
	}

	{
		Buf b;
		b.U16(2);
		b.U8(0); b.U16(10);
		BotCore::UnitObs list[BotCore::kObsMaxUnits];
		CHECK_EQ(BotCore::ParseUserList(b.v.data(), b.v.size(), 0, list, BotCore::kObsMaxUnits), 0);
	}

	{
		Buf b;
		b.U16(2);
		b.U8(0); b.U16(10);
		AddUserInfo(b, "BotAA", 1, 0, false, 50, 1, 100, 1000, 2000, 30, 1, false, 0);
		b.U8(0); b.U16(20);
		b.U8(1);                              // entry truncated right after the sid
		BotCore::UnitObs list[BotCore::kObsMaxUnits];
		CHECK_EQ(BotCore::ParseUserList(b.v.data(), b.v.size(), 0, list, BotCore::kObsMaxUnits), 1);
	}
}

TEST_CASE("Perception_ParseMoveAndRegion")
{
	{
		Buf b;
		b.U16(5); b.U16(100); b.U16(200); b.U16(3); b.U16(45); b.U8(7);
		uint16_t sid = 0, x = 0, z = 0, y = 0;
		CHECK(BotCore::ParseMove(b.v.data(), b.v.size(), sid, x, z, y));
		CHECK_EQ(int(sid), 5);
		CHECK_EQ(int(x), 100);
		CHECK_EQ(int(z), 200);
		CHECK_EQ(int(y), 3);
	}

	{
		Buf b;
		b.U16(5); b.U16(100); b.U16(200); b.U16(3); b.U16(45);
		uint16_t sid = 0, x = 0, z = 0, y = 0;
		CHECK(!BotCore::ParseMove(b.v.data(), b.v.size(), sid, x, z, y));
	}

	{
		Buf b;
		b.U16(3); b.U16(1); b.U16(2); b.U16(3);
		uint16_t ids[BotCore::kObsMaxUnits * 4];
		int n = BotCore::ParseRegionList(b.v.data(), b.v.size(), ids, BotCore::kObsMaxUnits * 4);
		CHECK_EQ(n, 3);
		if (n == 3)
		{
			CHECK_EQ(int(ids[0]), 1);
			CHECK_EQ(int(ids[1]), 2);
			CHECK_EQ(int(ids[2]), 3);
		}
	}

	{
		Buf b;
		b.U16(5); b.U16(1); b.U16(2);
		uint16_t ids[BotCore::kObsMaxUnits * 4];
		CHECK_EQ(BotCore::ParseRegionList(b.v.data(), b.v.size(), ids, BotCore::kObsMaxUnits * 4), 2);
		CHECK_EQ(BotCore::ParseRegionList(b.v.data(), b.v.size(), ids, 2), 2);
	}

	{
		Buf b;
		b.U16(0);
		uint16_t ids[BotCore::kObsMaxUnits * 4];
		CHECK_EQ(BotCore::ParseRegionList(b.v.data(), b.v.size(), ids, BotCore::kObsMaxUnits * 4), 0);
	}

	{
		Buf b;
		b.U8(0);
		uint16_t ids[BotCore::kObsMaxUnits * 4];
		CHECK_EQ(BotCore::ParseRegionList(b.v.data(), b.v.size(), ids, BotCore::kObsMaxUnits * 4), 0);
	}
}

TEST_CASE("Perception_ObsTable")
{
	BotCore::ObsTable table;
	BotCore::UnitObs u;
	memset(&u, 0, sizeof(u));
	strcpy(u.name, "u");

	for (int i = 0; i < BotCore::kObsMaxUnits; i++)
	{
		u.sid = (uint16_t)(100 + i);
		CHECK(table.Upsert(u));
	}
	CHECK_EQ(table.Count(), BotCore::kObsMaxUnits);

	u.sid = (uint16_t)(100 + BotCore::kObsMaxUnits);   // 65th distinct id
	CHECK(!table.Upsert(u));
	CHECK_EQ(int(table.Overflow()), 1);

	u.sid = 100;
	u.level = 55;
	CHECK(table.Upsert(u));
	CHECK_EQ(table.Count(), BotCore::kObsMaxUnits);
	CHECK_EQ(int(table.Find(100)->level), 55);

	table.Remove(100);
	CHECK_EQ(table.Count(), BotCore::kObsMaxUnits - 1);
	CHECK(table.Find(100) == nullptr);
	for (int i = 1; i < BotCore::kObsMaxUnits; i++)
		CHECK(table.Find((uint16_t)(100 + i)) != nullptr);

	table.Remove(9999);   // unknown: harmless
	CHECK_EQ(table.Count(), BotCore::kObsMaxUnits - 1);

	CHECK(table.UpdatePosition(101, 11, 22, 33, 900));
	CHECK_EQ(int(table.Find(101)->x10), 11);
	CHECK_EQ(int(table.Find(101)->z10), 22);
	CHECK_EQ(int(table.Find(101)->y10), 33);
	CHECK(table.Find(101)->lastSeenMs == 900);
	CHECK(!table.UpdatePosition(9999, 1, 2, 3, 4));
	CHECK_EQ(table.Count(), BotCore::kObsMaxUnits - 1);

	CHECK(table.MarkDead(102, 901));
	CHECK_EQ(int(table.Find(102)->resHpType), BotCore::kObsUserDead);
	CHECK(!table.MarkDead(9999, 902));

	uint16_t ids[4] = { 101, 102, 12345, 9999 };
	CHECK_EQ(table.Retain(ids, 4, 12345), 1);   // only 9999 is unknown (12345 is selfSid)
	CHECK_EQ(table.Count(), 2);
	CHECK(table.Find(101) != nullptr);
	CHECK(table.Find(102) != nullptr);

	CHECK_EQ(table.Retain(nullptr, 0, 12345), 0);
	CHECK_EQ(table.Count(), 0);

	table.Clear();
	CHECK_EQ(table.Count(), 0);
	CHECK_EQ(int(table.Overflow()), 0);
}
