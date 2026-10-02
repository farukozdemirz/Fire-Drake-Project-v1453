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

static BotCore::UnitObs MakeUnit(uint16_t sid)
{
	BotCore::UnitObs u;
	memset(&u, 0, sizeof(u));
	u.sid = sid;
	return u;
}

TEST_CASE("Perception_PendingIds_Set")
{
	BotCore::ObsTable obs;
	BotCore::PendingIds pending;

	{
		const uint16_t ids[4] = { 5, 7, 5, 9 };
		pending.Set(ids, 4, obs);
		CHECK_EQ(pending.Count(), 3);

		uint16_t out[8];
		CHECK_EQ(pending.Peek(obs, 0xFFFF, out, 8), 3);
		CHECK_EQ(int(out[0]), 5);
		CHECK_EQ(int(out[1]), 7);
		CHECK_EQ(int(out[2]), 9);
	}

	obs.Upsert(MakeUnit(7));
	{
		const uint16_t ids[3] = { 5, 7, 9 };
		pending.Set(ids, 3, obs);
		CHECK_EQ(pending.Count(), 2);

		uint16_t out[8];
		CHECK_EQ(pending.Peek(obs, 0xFFFF, out, 8), 2);
		CHECK_EQ(int(out[0]), 5);
		CHECK_EQ(int(out[1]), 9);
	}

	{
		uint16_t ids[200];
		for (int i = 0; i < 200; i++)
			ids[i] = (uint16_t)(1000 + i);
		pending.Set(ids, 200, obs);
		CHECK_EQ(pending.Count(), BotCore::kObsPendingMax);
	}

	pending.Set(nullptr, 0, obs);
	CHECK_EQ(pending.Count(), 0);

	const uint16_t one[1] = { 42 };
	pending.Set(one, 1, obs);
	CHECK_EQ(pending.Count(), 1);
	pending.Clear();
	CHECK_EQ(pending.Count(), 0);
}

TEST_CASE("Perception_PendingIds_PeekRemove")
{
	BotCore::ObsTable obs;
	BotCore::PendingIds pending;

	const uint16_t ids[5] = { 1, 2, 3, 4, 5 };
	pending.Set(ids, 5, obs);
	CHECK_EQ(pending.Count(), 5);
	obs.Upsert(MakeUnit(2));

	uint16_t out[8];
	CHECK_EQ(pending.Peek(obs, 4, out, 10), 3);
	CHECK_EQ(int(out[0]), 1);
	CHECK_EQ(int(out[1]), 3);
	CHECK_EQ(int(out[2]), 5);
	CHECK_EQ(pending.Count(), 3);

	CHECK_EQ(pending.Peek(obs, 4, out, 2), 2);
	CHECK_EQ(int(out[0]), 1);
	CHECK_EQ(int(out[1]), 3);
	CHECK_EQ(pending.Count(), 3);

	const uint16_t drop[1] = { 3 };
	pending.Remove(drop, 1);
	CHECK_EQ(pending.Count(), 2);
	CHECK_EQ(pending.Peek(obs, 4, out, 8), 2);
	CHECK_EQ(int(out[0]), 1);
	CHECK_EQ(int(out[1]), 5);

	const uint16_t unknown[1] = { 9999 };
	pending.Remove(unknown, 1);
	CHECK_EQ(pending.Count(), 2);

	const uint16_t all[2] = { 1, 5 };
	pending.Remove(all, 2);
	CHECK_EQ(pending.Count(), 0);
	CHECK_EQ(pending.Peek(obs, 4, out, 8), 0);
}

TEST_CASE("Perception_CheckUserIn")
{
	CHECK_EQ(int(BotCore::kUserInMaxIds), 32);
	CHECK_EQ(int(BotCore::kUserInMinGapMs), 1000);
	CHECK_EQ(int(BotCore::kObsPendingMax), 128);

	BotCore::UserInCheck c;
	c.count = 1;
	c.hasLast = false;
	c.sinceLastMs = 0;
	CHECK(BotCore::CheckUserIn(c) == BotCore::USERIN_OK);

	c.count = 0;
	CHECK(BotCore::CheckUserIn(c) == BotCore::USERIN_REJECT_COUNT);

	c.count = 33;
	CHECK(BotCore::CheckUserIn(c) == BotCore::USERIN_REJECT_COUNT);

	c.count = 32;
	CHECK(BotCore::CheckUserIn(c) == BotCore::USERIN_OK);

	c.hasLast = true;
	c.sinceLastMs = 999;
	CHECK(BotCore::CheckUserIn(c) == BotCore::USERIN_REJECT_GAP);

	c.sinceLastMs = 1000;
	CHECK(BotCore::CheckUserIn(c) == BotCore::USERIN_OK);

	c.count = 0;
	c.sinceLastMs = 0;
	CHECK(BotCore::CheckUserIn(c) == BotCore::USERIN_REJECT_COUNT);
}

// Writes the NPC info record in the exact wire order the server produces.
static void AddNpcInfo(Buf & b, uint16_t protoId, uint16_t pictureId, uint8_t type, uint32_t sellingGroup,
	uint16_t size, uint32_t weapon1, uint32_t weapon2, const char * name, uint8_t nation, uint8_t level,
	uint16_t x10, uint16_t z10, uint16_t y10, uint32_t gateOpen, uint8_t objectType, int8_t direction)
{
	b.U16(protoId);
	b.U16(pictureId);
	b.U8(type);
	b.U32(sellingGroup);
	b.U16(size);
	b.U32(weapon1);
	b.U32(weapon2);
	b.Str(name);
	b.U8(nation);
	b.U8(level);
	b.U16(x10);
	b.U16(z10);
	b.U16(y10);
	b.U32(gateOpen);
	b.U8(objectType);
	b.U16(0);                                 // unknown
	b.U16(0);                                 // unknown
	b.U8((uint8_t)direction);
}

TEST_CASE("Perception_NpcInfo_Parse")
{
	Buf b;
	AddNpcInfo(b, 5400, 700, 4, 123, 50, 111, 222, "Karus Guard Tower", 1, 90, 12740, 8900, 1234, 1, 3, 7);

	BotCore::ByteReader r(b.v.data(), b.v.size());
	BotCore::NpcObs out;
	memset(&out, 0, sizeof(out));

	CHECK(BotCore::ParseNpcInfo(r, 555, 123456, out));
	CHECK_EQ(int(r.pos()), int(b.v.size()));
	CHECK_EQ(int(out.id), 555);
	CHECK_EQ(int(out.protoId), 5400);
	CHECK_EQ(int(out.type), 4);
	CHECK_EQ(int(out.nation), 1);
	CHECK_EQ(int(out.level), 90);
	CHECK_EQ(int(out.x10), 12740);
	CHECK_EQ(int(out.z10), 8900);
	CHECK_EQ(int(out.y10), 1234);
	CHECK(out.gateOpen);
	CHECK_EQ(int(out.objectType), 3);
	CHECK(!out.dead);
	CHECK(out.lastSeenMs == 123456);
	CHECK(strcmp(out.name, "Karus Guard Tower") == 0);
	for (int i = (int)strlen("Karus Guard Tower") + 1; i < (int)BotCore::kNpcNameMax; i++)
		CHECK_EQ(int(out.name[i]), 0);

	{
		Buf c;
		AddNpcInfo(c, 10, 20, 3, 40, 5, 6, 7, "Monster", 0, 12, 1, 2, 3, 0, 8, -1);
		BotCore::ByteReader r2(c.v.data(), c.v.size());
		BotCore::NpcObs o2;
		memset(&o2, 0, sizeof(o2));
		CHECK(BotCore::ParseNpcInfo(r2, 9, 0, o2));
		CHECK(!o2.gateOpen);
		CHECK_EQ(int(o2.nation), 0);
	}

	// A 31-char name fits in the 32-byte buffer (NUL included).
	{
		Buf c;
		AddNpcInfo(c, 1, 2, 3, 4, 5, 6, 7, "abcdefghijklmnopqrstuvwxyz01234", 1, 2, 3, 4, 5, 0, 6, 0);
		BotCore::ByteReader r2(c.v.data(), c.v.size());
		BotCore::NpcObs o2;
		memset(&o2, 0, sizeof(o2));
		CHECK(BotCore::ParseNpcInfo(r2, 1, 0, o2));
		CHECK_EQ(int(strlen(o2.name)), 31);
	}

	// A 32-char name does not fit.
	{
		Buf c;
		AddNpcInfo(c, 1, 2, 3, 4, 5, 6, 7, "abcdefghijklmnopqrstuvwxyz012345", 1, 2, 3, 4, 5, 0, 6, 0);
		BotCore::ByteReader r2(c.v.data(), c.v.size());
		BotCore::NpcObs o2;
		CHECK(!BotCore::ParseNpcInfo(r2, 1, 0, o2));
	}
}

TEST_CASE("Perception_NpcInfo_Truncated")
{
	Buf full;
	AddNpcInfo(full, 10, 20, 3, 40, 5, 6, 7, "Karus Guard Tower", 1, 90, 12740, 8900, 1234, 1, 3, 7);

	for (size_t len = 0; len < full.v.size(); len++)
	{
		BotCore::ByteReader r(full.v.data(), len);
		BotCore::NpcObs out;
		CHECK(!BotCore::ParseNpcInfo(r, 1, 0, out));
	}

	{
		BotCore::ByteReader r(nullptr, 0);
		BotCore::NpcObs out;
		CHECK(!BotCore::ParseNpcInfo(r, 1, 0, out));
	}
}

TEST_CASE("Perception_ParseNpcInOut")
{
	const uint8_t inTypes[2] = { 1, 3 };
	for (int i = 0; i < 2; i++)
	{
		Buf b;
		b.U8(inTypes[i]);
		b.U16(321);
		AddNpcInfo(b, 5400, 700, 4, 123, 50, 111, 222, "Karus Guard Tower", 1, 90, 12740, 8900, 1234, 1, 3, 7);

		uint8_t type = 0;
		BotCore::NpcObs out;
		memset(&out, 0, sizeof(out));
		CHECK(BotCore::ParseNpcInOut(b.v.data(), b.v.size(), 777, type, out));
		CHECK_EQ(int(type), int(inTypes[i]));
		CHECK_EQ(int(out.id), 321);
		CHECK_EQ(int(out.protoId), 5400);
		CHECK(strcmp(out.name, "Karus Guard Tower") == 0);
		CHECK(out.lastSeenMs == 777);
	}

	{
		Buf b;
		b.U8(2);                              // OUT: id only, 3 bytes
		b.U16(88);
		uint8_t type = 0;
		BotCore::NpcObs out;
		memset(&out, 0, sizeof(out));
		CHECK(BotCore::ParseNpcInOut(b.v.data(), b.v.size(), 777, type, out));
		CHECK_EQ(int(type), 2);
		CHECK_EQ(int(out.id), 88);
	}

	{
		Buf b;
		b.U8(1);                              // 2 bytes: too short for the id
		b.U8(0);
		uint8_t type = 0;
		BotCore::NpcObs out;
		CHECK(!BotCore::ParseNpcInOut(b.v.data(), b.v.size(), 0, type, out));
	}

	{
		Buf b;
		b.U8(1);                              // IN but the record is cut
		b.U16(9);
		uint8_t type = 0;
		BotCore::NpcObs out;
		CHECK(!BotCore::ParseNpcInOut(b.v.data(), b.v.size(), 0, type, out));
	}
}

TEST_CASE("Perception_ParseNpcList")
{
	{
		Buf b;
		b.U16(2);                             // no marker byte before each entry
		b.U16(10);
		AddNpcInfo(b, 100, 1, 2, 3, 4, 5, 6, "NpcAA", 0, 10, 1000, 2000, 30, 1, 2, 3);
		b.U16(20);
		AddNpcInfo(b, 200, 1, 2, 3, 4, 5, 6, "NpcBB", 1, 20, 3000, 4000, 50, 0, 4, 5);

		BotCore::NpcObs list[BotCore::kNpcMaxUnits];
		uint16_t declared = 0;
		int n = BotCore::ParseNpcList(b.v.data(), b.v.size(), 777, list, BotCore::kNpcMaxUnits, declared);
		CHECK_EQ(n, 2);
		CHECK_EQ(int(declared), 2);
		if (n == 2)
		{
			CHECK_EQ(int(list[0].id), 10);
			CHECK(strcmp(list[0].name, "NpcAA") == 0);
			CHECK_EQ(int(list[0].level), 10);
			CHECK_EQ(int(list[1].id), 20);
			CHECK(strcmp(list[1].name, "NpcBB") == 0);
			CHECK_EQ(int(list[1].nation), 1);
			CHECK(list[1].lastSeenMs == 777);
		}
	}

	{
		Buf b;                                // declared 5 but only 2 records present
		b.U16(5);
		b.U16(10);
		AddNpcInfo(b, 100, 1, 2, 3, 4, 5, 6, "NpcAA", 0, 10, 1000, 2000, 30, 1, 2, 3);
		b.U16(20);
		AddNpcInfo(b, 200, 1, 2, 3, 4, 5, 6, "NpcBB", 1, 20, 3000, 4000, 50, 0, 4, 5);

		BotCore::NpcObs list[BotCore::kNpcMaxUnits];
		uint16_t declared = 0;
		int n = BotCore::ParseNpcList(b.v.data(), b.v.size(), 0, list, BotCore::kNpcMaxUnits, declared);
		CHECK_EQ(n, 2);
		CHECK_EQ(int(declared), 5);
	}

	{
		Buf b;                                // cap 1
		b.U16(2);
		b.U16(10);
		AddNpcInfo(b, 100, 1, 2, 3, 4, 5, 6, "NpcAA", 0, 10, 1000, 2000, 30, 1, 2, 3);
		b.U16(20);
		AddNpcInfo(b, 200, 1, 2, 3, 4, 5, 6, "NpcBB", 1, 20, 3000, 4000, 50, 0, 4, 5);

		BotCore::NpcObs list[BotCore::kNpcMaxUnits];
		uint16_t declared = 0;
		CHECK_EQ(BotCore::ParseNpcList(b.v.data(), b.v.size(), 0, list, 1, declared), 1);
		CHECK_EQ(int(declared), 2);
	}

	{
		Buf b;                                // second record cut after the id
		b.U16(2);
		b.U16(10);
		AddNpcInfo(b, 100, 1, 2, 3, 4, 5, 6, "NpcAA", 0, 10, 1000, 2000, 30, 1, 2, 3);
		b.U16(20);
		b.U8(1);

		BotCore::NpcObs list[BotCore::kNpcMaxUnits];
		uint16_t declared = 0;
		CHECK_EQ(BotCore::ParseNpcList(b.v.data(), b.v.size(), 0, list, BotCore::kNpcMaxUnits, declared), 1);
	}

	{
		BotCore::NpcObs list[BotCore::kNpcMaxUnits];
		uint16_t declared = 99;
		CHECK_EQ(BotCore::ParseNpcList(nullptr, 0, 0, list, BotCore::kNpcMaxUnits, declared), 0);
		CHECK_EQ(int(declared), 0);
	}
}

TEST_CASE("Perception_ParseNpcMove")
{
	{
		Buf b;
		b.U16(5); b.U16(100); b.U16(200); b.U16(3); b.U16(45);
		uint16_t id = 0, x = 0, z = 0, y = 0;
		CHECK(BotCore::ParseNpcMove(b.v.data(), b.v.size(), id, x, z, y));
		CHECK_EQ(int(id), 5);
		CHECK_EQ(int(x), 100);
		CHECK_EQ(int(z), 200);
		CHECK_EQ(int(y), 3);
	}

	{
		Buf b;
		b.U16(5); b.U16(100); b.U16(200); b.U16(3);
		uint16_t id = 0, x = 0, z = 0, y = 0;
		CHECK(!BotCore::ParseNpcMove(b.v.data(), b.v.size(), id, x, z, y));
	}

	{
		Buf b;
		b.U16(5); b.U16(100); b.U16(200); b.U16(3); b.U16(45); b.U8(0);
		uint16_t id = 0, x = 0, z = 0, y = 0;
		CHECK(BotCore::ParseNpcMove(b.v.data(), b.v.size(), id, x, z, y));
		CHECK_EQ(int(id), 5);
	}
}

TEST_CASE("Perception_NpcTable")
{
	BotCore::NpcTable table;
	BotCore::NpcObs n;
	memset(&n, 0, sizeof(n));
	strcpy(n.name, "n");

	for (int i = 0; i < BotCore::kNpcMaxUnits; i++)
	{
		n.id = (uint16_t)(100 + i);
		CHECK(table.Upsert(n));
	}
	CHECK_EQ(table.Count(), BotCore::kNpcMaxUnits);

	n.id = (uint16_t)(100 + BotCore::kNpcMaxUnits);   // one past capacity
	CHECK(!table.Upsert(n));
	CHECK_EQ(int(table.Overflow()), 1);

	table.NoteDropped(3);
	CHECK_EQ(int(table.Overflow()), 4);

	n.id = 100;
	n.level = 55;
	CHECK(table.Upsert(n));
	CHECK_EQ(table.Count(), BotCore::kNpcMaxUnits);
	CHECK_EQ(int(table.Find(100)->level), 55);

	table.Remove(100);
	CHECK_EQ(table.Count(), BotCore::kNpcMaxUnits - 1);
	CHECK(table.Find(100) == nullptr);
	table.Remove(9999);   // unknown: harmless
	CHECK_EQ(table.Count(), BotCore::kNpcMaxUnits - 1);

	CHECK(table.UpdatePosition(101, 11, 22, 33, 900));
	CHECK_EQ(int(table.Find(101)->x10), 11);
	CHECK_EQ(int(table.Find(101)->z10), 22);
	CHECK_EQ(int(table.Find(101)->y10), 33);
	CHECK(table.Find(101)->lastSeenMs == 900);
	CHECK(!table.UpdatePosition(9999, 1, 2, 3, 4));

	CHECK(table.MarkDead(102, 901));
	CHECK(table.Find(102)->dead);
	CHECK(!table.MarkDead(9999, 902));

	{
		BotCore::NpcObs fresh;   // a fresh info record clears dead
		memset(&fresh, 0, sizeof(fresh));
		fresh.id = 102;
		CHECK(table.Upsert(fresh));
		CHECK(!table.Find(102)->dead);
	}

	uint16_t ids[4] = { 101, 102, 12345, 9999 };
	CHECK_EQ(table.Retain(ids, 4), 2);   // 12345 and 9999 are unknown
	CHECK_EQ(table.Count(), 2);
	CHECK(table.Find(101) != nullptr);
	CHECK(table.Find(102) != nullptr);

	uint16_t dup[3] = { 100, 100, 101 };   // repeats are counted separately
	CHECK_EQ(table.Retain(dup, 3), 2);
	CHECK_EQ(table.Count(), 1);

	CHECK_EQ(table.Retain(nullptr, 0), 0);
	CHECK_EQ(table.Count(), 0);

	table.Clear();
	CHECK_EQ(table.Count(), 0);
	CHECK_EQ(int(table.Overflow()), 0);
}

static BotCore::NpcObs MakeNpc(uint16_t id)
{
	BotCore::NpcObs n;
	memset(&n, 0, sizeof(n));
	n.id = id;
	strcpy(n.name, "n");
	return n;
}

TEST_CASE("Perception_PendingIds_Npc")
{
	BotCore::NpcTable npcs;
	BotCore::PendingIds pending;

	{
		const uint16_t ids[4] = { 10001, 10002, 10001, 10003 };
		pending.Set(ids, 4, npcs);
		CHECK_EQ(pending.Count(), 3);
	}

	npcs.Upsert(MakeNpc(10002));
	{
		const uint16_t ids[3] = { 10001, 10002, 10003 };
		pending.Set(ids, 3, npcs);
		CHECK_EQ(pending.Count(), 2);

		uint16_t out[10];
		CHECK_EQ(pending.Peek(npcs, 0xFFFF, out, 10), 2);
		CHECK_EQ(int(out[0]), 10001);
		CHECK_EQ(int(out[1]), 10003);

		CHECK_EQ(pending.Peek(npcs, 0xFFFF, out, 1), 1);
		CHECK_EQ(pending.Count(), 2);
	}

	npcs.Upsert(MakeNpc(10001));
	{
		uint16_t out[10];
		CHECK_EQ(pending.Peek(npcs, 0xFFFF, out, 10), 1);
		CHECK_EQ(int(out[0]), 10003);
		CHECK_EQ(pending.Count(), 1);
	}

	const uint16_t drop[1] = { 10003 };
	pending.Remove(drop, 1);
	CHECK_EQ(pending.Count(), 0);

	BotCore::ObsTable obs;
	const uint16_t one[1] = { 7 };
	pending.Set(one, 1, obs);
	CHECK_EQ(pending.Count(), 1);
}

TEST_CASE("Perception_CheckNpcIn")
{
	CHECK_EQ(int(BotCore::kNpcInMaxIds), 32);
	CHECK_EQ(int(BotCore::kNpcInMinGapMs), 1000);

	BotCore::NpcInCheck c;
	c.count = 1;
	c.hasLast = false;
	c.sinceLastMs = 0;
	CHECK(BotCore::CheckNpcIn(c) == BotCore::NPCIN_OK);

	c.count = 0;
	CHECK(BotCore::CheckNpcIn(c) == BotCore::NPCIN_REJECT_COUNT);

	c.count = 33;
	CHECK(BotCore::CheckNpcIn(c) == BotCore::NPCIN_REJECT_COUNT);

	c.count = 32;
	CHECK(BotCore::CheckNpcIn(c) == BotCore::NPCIN_OK);

	c.hasLast = true;
	c.sinceLastMs = 999;
	CHECK(BotCore::CheckNpcIn(c) == BotCore::NPCIN_REJECT_GAP);

	c.sinceLastMs = 1000;
	CHECK(BotCore::CheckNpcIn(c) == BotCore::NPCIN_OK);

	c.count = 0;
	c.sinceLastMs = 0;
	CHECK(BotCore::CheckNpcIn(c) == BotCore::NPCIN_REJECT_COUNT);
}

static BotCore::SelfState MakeSelf()
{
	BotCore::SelfState s;
	memset(&s, 0, sizeof(s));
	s.sid = 1;
	s.nation = 1;
	s.x = 1000.0f;
	s.z = 1000.0f;
	s.hp = s.maxHp = 5000;
	s.mp = s.maxMp = 3000;
	return s;
}

TEST_CASE("Perception_Snapshot_Split")
{
	BotCore::SelfState self = MakeSelf();
	BotCore::ObsTable obs;

	{
		BotCore::UnitObs u = MakeUnit(1);       // the bot itself: skipped
		u.nation = 1;
		u.x10 = 10000;
		u.z10 = 10000;
		obs.Upsert(u);
	}
	{
		BotCore::UnitObs u = MakeUnit(2);       // ally, dist 5
		u.nation = 1;
		u.x10 = 10030;
		u.z10 = 10040;
		obs.Upsert(u);
	}
	{
		BotCore::UnitObs u = MakeUnit(3);       // enemy, dist 10
		u.nation = 2;
		u.cls = 205;
		u.level = 77;
		u.race = 12;
		u.partyLeader = true;
		u.invisibility = 3;
		u.resHpType = 3;                        // dead
		u.x10 = 10060;
		u.z10 = 10080;
		u.lastSeenMs = 4750;
		obs.Upsert(u);
	}
	{
		BotCore::UnitObs u = MakeUnit(4);       // enemy, dist 50, sitting
		u.nation = 2;
		u.resHpType = 2;                        // sitting
		u.x10 = 10300;
		u.z10 = 10400;
		u.lastSeenMs = 6000;                    // ahead of the snapshot clock
		obs.Upsert(u);
	}

	BotCore::NpcTable npcs;
	BotCore::PerceptionSnapshot out;
	BotCore::BuildSnapshot(self, obs, npcs, 5000, out);

	CHECK_EQ(out.tMs, (uint64_t)5000);
	CHECK_EQ(int(out.self.sid), 1);
	CHECK_EQ(out.self.hp, 5000);
	CHECK_EQ(out.enemyCount, 2);
	CHECK_EQ(out.enemyTotal, 2);
	CHECK_EQ(out.allyCount, 1);
	CHECK_EQ(out.allyTotal, 1);
	CHECK_EQ(out.npcCount, 0);

	CHECK_EQ(int(out.enemies[0].id), 3);
	CHECK(out.enemies[0].dist == 10.0f);
	CHECK_EQ(int(out.enemies[0].cls), 205);
	CHECK_EQ(int(out.enemies[0].level), 77);
	CHECK_EQ(int(out.enemies[0].race), 12);
	CHECK(out.enemies[0].partyLeader);
	CHECK_EQ(int(out.enemies[0].invisibility), 3);   // raw byte, not filtered
	CHECK(out.enemies[0].dead);
	CHECK(!out.enemies[0].sitting);
	CHECK_EQ(out.enemies[0].ageMs, 250u);

	CHECK_EQ(int(out.enemies[1].id), 4);
	CHECK(out.enemies[1].dist == 50.0f);
	CHECK(out.enemies[1].sitting);
	CHECK(!out.enemies[1].dead);
	CHECK_EQ(out.enemies[1].ageMs, 0u);

	CHECK_EQ(int(out.allies[0].id), 2);
	CHECK(out.allies[0].dist == 5.0f);
	CHECK(out.allies[0].x == 1003.0f);
	CHECK(out.allies[0].z == 1004.0f);

	// ageMs is clamped to 32 bits when the packet clock is far behind.
	{
		BotCore::ObsTable old;
		BotCore::UnitObs u = MakeUnit(9);
		u.nation = 2;
		u.x10 = 10000;
		u.z10 = 10000;
		u.lastSeenMs = 0;
		old.Upsert(u);

		BotCore::PerceptionSnapshot clip;
		BotCore::BuildSnapshot(self, old, npcs, 0x100000000ULL + 5, clip);
		CHECK_EQ(clip.enemyCount, 1);
		CHECK_EQ(clip.enemies[0].ageMs, 0xFFFFFFFFu);
	}
}

TEST_CASE("Perception_Snapshot_OrderCap")
{
	BotCore::SelfState self = MakeSelf();
	BotCore::ObsTable obs;

	for (int i = 0; i < 40; i++)   // added far to near: i=0 -> dist 40, i=39 -> dist 1
	{
		BotCore::UnitObs u = MakeUnit((uint16_t)(100 + i));
		u.nation = 2;
		u.x10 = (uint16_t)(10000 + 10 * (40 - i));
		u.z10 = 10000;
		obs.Upsert(u);
	}

	BotCore::NpcTable npcs;
	BotCore::PerceptionSnapshot out;
	BotCore::BuildSnapshot(self, obs, npcs, 1000, out);

	CHECK_EQ(out.enemyTotal, 40);
	CHECK_EQ(out.enemyCount, 32);
	CHECK(out.enemies[0].dist == 1.0f);
	CHECK_EQ(int(out.enemies[0].id), 139);
	CHECK(out.enemies[31].dist == 32.0f);
	CHECK_EQ(int(out.enemies[31].id), 108);
	for (int i = 0; i < out.enemyCount - 1; i++)
		CHECK(out.enemies[i].dist < out.enemies[i + 1].dist);

	// A second call with empty tables reuses (and clears) the same output.
	BotCore::ObsTable emptyObs;
	BotCore::NpcTable emptyNpcs;
	BotCore::BuildSnapshot(self, emptyObs, emptyNpcs, 2000, out);
	CHECK_EQ(out.enemyCount, 0);
	CHECK_EQ(out.enemyTotal, 0);
}

TEST_CASE("Perception_Snapshot_TieAndNation")
{
	BotCore::SelfState self = MakeSelf();
	BotCore::ObsTable obs;

	{
		BotCore::UnitObs u = MakeUnit(20);      // enemy (nation 2), dist 5
		u.nation = 2;
		u.x10 = 10030;
		u.z10 = 10040;
		obs.Upsert(u);
	}
	{
		BotCore::UnitObs u = MakeUnit(10);      // enemy (nation 2), dist 5
		u.nation = 2;
		u.x10 = 10030;
		u.z10 = 10040;
		obs.Upsert(u);
	}
	{
		BotCore::UnitObs u = MakeUnit(30);      // ally (nation 1)
		u.nation = 1;
		u.x10 = 10030;
		u.z10 = 10040;
		obs.Upsert(u);
	}

	BotCore::NpcTable npcs;
	BotCore::PerceptionSnapshot out;
	BotCore::BuildSnapshot(self, obs, npcs, 1000, out);

	CHECK_EQ(out.enemyCount, 2);
	CHECK_EQ(int(out.enemies[0].id), 10);       // tie on dist: lower id first
	CHECK_EQ(int(out.enemies[1].id), 20);
	CHECK_EQ(out.allyCount, 1);
	CHECK_EQ(int(out.allies[0].id), 30);

	// Classification follows the bot's own nation.
	self.nation = 2;
	BotCore::BuildSnapshot(self, obs, npcs, 1000, out);
	CHECK_EQ(out.enemyCount, 1);
	CHECK_EQ(int(out.enemies[0].id), 30);
	CHECK_EQ(out.allyCount, 2);
	CHECK_EQ(int(out.allies[0].id), 10);
	CHECK_EQ(int(out.allies[1].id), 20);
}

TEST_CASE("Perception_Snapshot_Npcs")
{
	BotCore::SelfState self = MakeSelf();
	BotCore::ObsTable obs;
	BotCore::NpcTable npcs;

	for (int i = 0; i < 40; i++)   // added far to near: i=0 -> dist 40, i=39 -> dist 1
	{
		BotCore::NpcObs n = MakeNpc((uint16_t)(10001 + i));
		n.x10 = (uint16_t)(10000 + 10 * (40 - i));
		n.z10 = 10000;
		npcs.Upsert(n);
	}

	BotCore::PerceptionSnapshot out;
	BotCore::BuildSnapshot(self, obs, npcs, 1000, out);

	CHECK_EQ(out.npcTotal, 40);
	CHECK_EQ(out.npcCount, 32);
	CHECK(out.npcs[0].dist == 1.0f);
	CHECK_EQ(int(out.npcs[0].id), 10040);
	CHECK(out.npcs[31].dist == 32.0f);
	CHECK_EQ(int(out.npcs[31].id), 10009);
	for (int i = 0; i < out.npcCount - 1; i++)
		CHECK(out.npcs[i].dist < out.npcs[i + 1].dist);

	// A small NPC with all view fields set; a dead NPC stays in the list.
	BotCore::NpcTable one;
	{
		BotCore::NpcObs n = MakeNpc(20001);
		n.protoId = 5400;
		n.type = 62;
		n.nation = 1;
		n.level = 60;
		n.x10 = 10030;
		n.z10 = 10040;
		n.dead = true;
		n.gateOpen = true;
		n.lastSeenMs = 900;                     // nowMs 1000 -> age 100
		one.Upsert(n);
	}

	BotCore::PerceptionSnapshot small;
	BotCore::BuildSnapshot(self, obs, one, 1000, small);

	CHECK_EQ(small.npcCount, 1);
	CHECK_EQ(int(small.npcs[0].id), 20001);
	CHECK_EQ(int(small.npcs[0].protoId), 5400);
	CHECK_EQ(int(small.npcs[0].type), 62);
	CHECK_EQ(int(small.npcs[0].nation), 1);
	CHECK_EQ(int(small.npcs[0].level), 60);
	CHECK(small.npcs[0].dead);
	CHECK(small.npcs[0].gateOpen);
	CHECK(small.npcs[0].dist == 5.0f);
	CHECK_EQ(small.npcs[0].ageMs, 100u);
	CHECK_EQ(small.enemyTotal, 0);
	CHECK_EQ(small.allyTotal, 0);
}
