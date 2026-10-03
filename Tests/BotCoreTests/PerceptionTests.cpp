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

TEST_CASE("Perception_Self_Remaining")
{
	CHECK_EQ(BotCore::SnapRemainingSec(100, 40), 60u);
	CHECK_EQ(BotCore::SnapRemainingSec(40, 100), 0u);
	CHECK_EQ(BotCore::SnapRemainingSec(100, 100), 0u);
	CHECK_EQ(BotCore::SnapRemainingSec(4000000000000LL, 0), 0xFFFFFFFFu);

	CHECK_EQ(BotCore::SnapRemainingMs(2500, 1000), 1500u);
	CHECK_EQ(BotCore::SnapRemainingMs(2500, 2500), 0u);
	CHECK_EQ(BotCore::SnapRemainingMs(2500, 9999999999ULL), 0u);
	CHECK_EQ(BotCore::SnapRemainingMs(0, 0), 0u);
}

TEST_CASE("Perception_Self_AddBuff")
{
	BotCore::SelfState s = MakeSelf();

	CHECK(BotCore::SelfAddBuff(s, 1001, 3, true, 20));
	CHECK(BotCore::SelfAddBuff(s, 1002, 5, false, 7));
	CHECK(BotCore::SelfAddBuff(s, 1003, 8, true, 1));
	CHECK_EQ(s.buffCount, 3);
	CHECK_EQ(s.buffTotal, 3);
	CHECK_EQ(int(s.buffs[0].skillId), 1001);
	CHECK_EQ(int(s.buffs[0].buffType), 3);
	CHECK(s.buffs[0].isBuff);
	CHECK_EQ(s.buffs[0].remainingSec, 20u);
	CHECK_EQ(int(s.buffs[1].skillId), 1002);
	CHECK_EQ(int(s.buffs[1].buffType), 5);
	CHECK(!s.buffs[1].isBuff);
	CHECK_EQ(s.buffs[1].remainingSec, 7u);
	CHECK_EQ(int(s.buffs[2].skillId), 1003);
	CHECK_EQ(s.buffs[2].remainingSec, 1u);

	CHECK(!BotCore::SelfAddBuff(s, 1004, 1, true, 0));   // expired: ignored
	CHECK_EQ(s.buffCount, 3);
	CHECK_EQ(s.buffTotal, 3);

	for (int i = 3; i < BotCore::kSnapMaxBuffs; i++)
		CHECK(BotCore::SelfAddBuff(s, (uint32_t)(2000 + i), 1, true, 10));
	CHECK_EQ(s.buffCount, BotCore::kSnapMaxBuffs);
	CHECK_EQ(s.buffTotal, BotCore::kSnapMaxBuffs);

	CHECK(!BotCore::SelfAddBuff(s, 3001, 1, true, 10));
	CHECK(!BotCore::SelfAddBuff(s, 3002, 1, true, 10));
	CHECK_EQ(s.buffCount, BotCore::kSnapMaxBuffs);
	CHECK_EQ(s.buffTotal, BotCore::kSnapMaxBuffs + 2);
	CHECK_EQ(int(s.buffs[0].skillId), 1001);             // the first entries are unchanged
	CHECK_EQ(s.buffs[0].remainingSec, 20u);
}

TEST_CASE("Perception_Self_AddCooldown")
{
	BotCore::SelfState s = MakeSelf();

	CHECK(BotCore::SelfAddCooldown(s, 110518, 3000));
	CHECK(BotCore::SelfAddCooldown(s, 110519, 100));
	CHECK_EQ(s.cooldownCount, 2);
	CHECK_EQ(s.cooldownTotal, 2);
	CHECK_EQ(int(s.cooldowns[0].skillId), 110518);
	CHECK_EQ(s.cooldowns[0].remainingMs, 3000u);
	CHECK_EQ(int(s.cooldowns[1].skillId), 110519);
	CHECK_EQ(s.cooldowns[1].remainingMs, 100u);

	CHECK(!BotCore::SelfAddCooldown(s, 110520, 0));      // expired: ignored
	CHECK_EQ(s.cooldownCount, 2);
	CHECK_EQ(s.cooldownTotal, 2);

	for (int i = 2; i < BotCore::kSnapMaxCooldowns; i++)
		CHECK(BotCore::SelfAddCooldown(s, (uint32_t)(200000 + i), 500));
	CHECK_EQ(s.cooldownCount, BotCore::kSnapMaxCooldowns);
	CHECK_EQ(s.cooldownTotal, BotCore::kSnapMaxCooldowns);

	CHECK(!BotCore::SelfAddCooldown(s, 300001, 500));
	CHECK(!BotCore::SelfAddCooldown(s, 300002, 500));
	CHECK_EQ(s.cooldownCount, BotCore::kSnapMaxCooldowns);
	CHECK_EQ(s.cooldownTotal, BotCore::kSnapMaxCooldowns + 2);
	CHECK_EQ(int(s.cooldowns[0].skillId), 110518);       // the first entries are unchanged
}

TEST_CASE("Perception_Snapshot_SelfExtras")
{
	BotCore::SelfState self = MakeSelf();
	self.hpPotStock = 12;
	self.mpPotStock = 7;
	self.potWaitMs = 1500;
	self.castGapWaitMs = 90;
	CHECK(BotCore::SelfAddBuff(self, 106500, 4, true, 30));
	CHECK(BotCore::SelfAddCooldown(self, 110518, 2500));
	CHECK(BotCore::SelfAddCooldown(self, 110519, 800));

	BotCore::ObsTable obs;
	BotCore::NpcTable npcs;
	BotCore::PerceptionSnapshot out;
	BotCore::BuildSnapshot(self, obs, npcs, 1234, out);

	CHECK_EQ(out.self.hpPotStock, 12u);
	CHECK_EQ(out.self.mpPotStock, 7u);
	CHECK_EQ(out.self.potWaitMs, 1500u);
	CHECK_EQ(out.self.castGapWaitMs, 90u);
	CHECK_EQ(out.self.buffCount, 1);
	CHECK_EQ(out.self.buffTotal, 1);
	CHECK_EQ(int(out.self.buffs[0].skillId), 106500);
	CHECK_EQ(int(out.self.buffs[0].buffType), 4);
	CHECK(out.self.buffs[0].isBuff);
	CHECK_EQ(out.self.buffs[0].remainingSec, 30u);
	CHECK_EQ(out.self.cooldownCount, 2);
	CHECK_EQ(out.self.cooldownTotal, 2);
	CHECK_EQ(int(out.self.cooldowns[0].skillId), 110518);
	CHECK_EQ(out.self.cooldowns[0].remainingMs, 2500u);
	CHECK_EQ(int(out.self.cooldowns[1].skillId), 110519);
	CHECK_EQ(out.self.cooldowns[1].remainingMs, 800u);
	CHECK_EQ(out.enemyCount, 0);
	CHECK_EQ(out.allyCount, 0);
	CHECK_EQ(out.npcCount, 0);
}

// Writes a PARTY_INSERT member record (sub-opcode 0x03).
static void AddPartyMember(Buf & b, uint16_t sid, uint8_t flag, const char * name,
	int16_t maxHp, int16_t hp, uint8_t level, uint16_t cls, int16_t maxMp, int16_t mp, uint8_t nation)
{
	b.U8(0x03);
	b.U16(sid);
	b.U8(flag);
	uint16_t nameLen = (uint16_t)strlen(name);
	b.U16(nameLen);
	for (uint16_t i = 0; i < nameLen; i++)
		b.U8((uint8_t)name[i]);
	b.U16((uint16_t)maxHp);
	b.U16((uint16_t)hp);
	b.U8(level);
	b.U16(cls);
	b.U16((uint16_t)maxMp);
	b.U16((uint16_t)mp);
	b.U8(nation);
}

static bool ApplyPartyMember(BotCore::TeamTable & table, uint16_t sid, uint8_t flag, uint16_t selfSid, const char * name)
{
	Buf b;
	AddPartyMember(b, sid, flag, name, 1000, 900, 80, 101, 500, 400, 1);
	BotCore::PartyEvent ev;
	if (!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 1, ev))
		return false;
	return table.Apply(ev, selfSid);
}

static bool ApplyPartyHp(BotCore::TeamTable & table, uint16_t sid, uint16_t selfSid,
	int16_t maxHp, int16_t hp, int16_t maxMp, int16_t mp, uint64_t nowMs)
{
	Buf b;
	b.U8(0x06);
	b.U16(sid);
	b.U16((uint16_t)maxHp);
	b.U16((uint16_t)hp);
	b.U16((uint16_t)maxMp);
	b.U16((uint16_t)mp);
	BotCore::PartyEvent ev;
	if (!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), nowMs, ev))
		return false;
	return table.Apply(ev, selfSid);
}

static bool ApplyPartyRemove(BotCore::TeamTable & table, uint16_t sid, uint16_t selfSid)
{
	Buf b;
	b.U8(0x04);
	b.U16(sid);
	BotCore::PartyEvent ev;
	if (!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 1, ev))
		return false;
	return table.Apply(ev, selfSid);
}

static bool ApplyPartyDelete(BotCore::TeamTable & table, uint16_t selfSid)
{
	Buf b;
	b.U8(0x05);
	BotCore::PartyEvent ev;
	if (!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 1, ev))
		return false;
	return table.Apply(ev, selfSid);
}

TEST_CASE("Perception_Party_ParseMember")
{
	BotCore::PartyEvent ev;

	{
		Buf b;
		AddPartyMember(b, 7, 1, "BotWP_K", 3000, 2500, 80, 106, 1200, 900, 1);
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 5000, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_MEMBER);
		CHECK_EQ(int(ev.flag), 1);
		CHECK_EQ(int(ev.member.sid), 7);
		CHECK(strcmp(ev.member.name, "BotWP_K") == 0);
		CHECK_EQ(int(ev.member.maxHp), 3000);
		CHECK_EQ(int(ev.member.hp), 2500);
		CHECK_EQ(int(ev.member.level), 80);
		CHECK_EQ(int(ev.member.cls), 106);
		CHECK_EQ(int(ev.member.maxMp), 1200);
		CHECK_EQ(int(ev.member.mp), 900);
		CHECK_EQ(int(ev.member.nation), 1);
		CHECK(ev.member.lastSeenMs == 5000);
	}

	{
		// Hand-written server packet: u16 member-name length (ByteBuffer default, PartyHandler.cpp never calls SByte()).
		const uint8_t raw[25] = {0x03,0x07,0x00,0x01,0x07,0x00,0x42,0x6F,0x74,0x57,0x50,0x5F,0x4B,0xB8,0x0B,0xC4,0x09,0x50,0x6A,0x00,0xB0,0x04,0x84,0x03,0x01};
		CHECK(BotCore::ParsePartyEvent(raw, 25, 5000, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_MEMBER);
		CHECK_EQ(int(ev.flag), 1);
		CHECK_EQ(int(ev.member.sid), 7);
		CHECK(strcmp(ev.member.name, "BotWP_K") == 0);
		CHECK_EQ(int(ev.member.maxHp), 3000);
		CHECK_EQ(int(ev.member.hp), 2500);
		CHECK_EQ(int(ev.member.level), 80);
		CHECK_EQ(int(ev.member.cls), 106);
		CHECK_EQ(int(ev.member.maxMp), 1200);
		CHECK_EQ(int(ev.member.mp), 900);
		CHECK_EQ(int(ev.member.nation), 1);
		CHECK(!BotCore::ParsePartyEvent(raw, 24, 5000, ev));   // last byte cut off
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}

	{
		Buf b;
		AddPartyMember(b, 7, 1, "", 3000, 2500, 80, 106, 1200, 900, 1);   // u16 length 0
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_MEMBER);
		CHECK(strcmp(ev.member.name, "") == 0);
	}

	{
		Buf b;
		AddPartyMember(b, 7, 1, "abcdefghijklmnopqrstuvw", 1, 1, 1, 1, 1, 1, 1);   // 23 chars (max)
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_MEMBER);
		CHECK(strcmp(ev.member.name, "abcdefghijklmnopqrstuvw") == 0);
	}

	{
		Buf b;
		b.U8(0x03);
		b.U16(7);
		b.U8(1);
		b.U16(24);                        // u16 length 24 > kObsNameMax - 1
		for (int i = 0; i < 24; i++)
			b.U8((uint8_t)'a');
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}

	{
		Buf b;
		b.U8(0x03);
		b.U16(7);
		b.U8(1);
		b.U16(0xFFFF);                    // absurd u16 length
		b.U8(0x41);
		b.U8(0x42);
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}

	{
		Buf b;
		AddPartyMember(b, 7, 100, "BotWP_K", 3000, 2500, 80, 106, 1200, 900, 1);
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 1, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_MEMBER);
		CHECK_EQ(int(ev.flag), 100);
	}

	{
		Buf b;
		AddPartyMember(b, 7, 0, "BotWP_K", 1, 1, 1, 1, 1, 1, 1);
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}

	{
		Buf b;
		AddPartyMember(b, 7, 2, "BotWP_K", 1, 1, 1, 1, 1, 1, 1);
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}

	{
		Buf b;
		b.U8(0x03);
		b.U16(0xFFFF);                    // refusal: 3 bytes
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}

	{
		Buf b;
		AddPartyMember(b, 7, 1, "BotWP_K", 3000, 2500, 80, 106, 1200, 900, 1);
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size() - 1, 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}

	{
		Buf b;
		AddPartyMember(b, 7, 1, "abcdefghijklmnopqrstuvwx", 1, 1, 1, 1, 1, 1, 1);   // 24 chars
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}

	{
		CHECK(!BotCore::ParsePartyEvent(nullptr, 0, 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}
}

TEST_CASE("Perception_Party_ParseOthers")
{
	BotCore::PartyEvent ev;

	{
		Buf b;
		b.U8(0x06); b.U16(7); b.U16(3000); b.U16(2500); b.U16(1200); b.U16(900);
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 7000, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_HP);
		CHECK_EQ(int(ev.sid), 7);
		CHECK_EQ(int(ev.maxHp), 3000);
		CHECK_EQ(int(ev.hp), 2500);
		CHECK_EQ(int(ev.maxMp), 1200);
		CHECK_EQ(int(ev.mp), 900);
		CHECK(ev.nowMs == 7000);
	}

	{
		Buf b;
		b.U8(0x06); b.U16(7); b.U16(3000); b.U16((uint16_t)-5); b.U16(1200); b.U16(900);
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK_EQ(int(ev.hp), -5);
	}

	{
		Buf b;
		b.U8(0x06); b.U16(7); b.U16(3000); b.U16(2500); b.U16(1200);
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}

	{
		Buf b;
		b.U8(0x04); b.U16(9);
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_REMOVE);
		CHECK_EQ(int(ev.sid), 9);
	}

	{
		Buf b;
		b.U8(0x04); b.U8(9);
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_NONE);
	}

	{
		Buf b;
		b.U8(0x05);
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
		CHECK(ev.kind == BotCore::PARTY_EV_DELETE);
	}

	{
		Buf b;
		b.U8(0x02);
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
	}

	{
		Buf b;
		b.U8(0x07);
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
	}

	{
		Buf b;
		b.U8(0x09);
		CHECK(!BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 0, ev));
	}
}

TEST_CASE("Perception_Team_Table")
{
	BotCore::TeamTable table;

	CHECK(ApplyPartyMember(table, 7, 1, 1, "A"));
	CHECK(ApplyPartyMember(table, 8, 1, 1, "B"));
	CHECK(ApplyPartyMember(table, 9, 1, 1, "C"));
	CHECK_EQ(table.Count(), 3);
	CHECK(strcmp(table.Find(8)->name, "B") == 0);

	CHECK(ApplyPartyHp(table, 8, 1, 2000, 1500, 600, 500, 99));
	CHECK_EQ(table.Count(), 3);
	CHECK_EQ(int(table.Find(8)->maxHp), 2000);
	CHECK_EQ(int(table.Find(8)->hp), 1500);
	CHECK_EQ(int(table.Find(8)->maxMp), 600);
	CHECK_EQ(int(table.Find(8)->mp), 500);
	CHECK(table.Find(8)->lastSeenMs == 99);

	CHECK(!ApplyPartyHp(table, 99, 1, 1, 1, 1, 1, 0));
	CHECK_EQ(int(table.UnknownHp()), 1);

	CHECK(ApplyPartyMember(table, 8, 1, 1, "B2"));   // refresh: count unchanged
	CHECK_EQ(table.Count(), 3);

	CHECK(ApplyPartyRemove(table, 7, 1));
	CHECK_EQ(table.Count(), 2);
	CHECK(table.Find(7) == nullptr);

	CHECK(ApplyPartyRemove(table, 8, 8));            // own id: the table empties
	CHECK_EQ(table.Count(), 0);

	{
		BotCore::TeamTable full;
		for (int i = 0; i < BotCore::kTeamMaxMembers; i++)
			CHECK(ApplyPartyMember(full, (uint16_t)(200 + i), 1, 1, "m"));
		CHECK_EQ(full.Count(), BotCore::kTeamMaxMembers);

		CHECK(!ApplyPartyMember(full, 999, 1, 1, "x"));
		CHECK_EQ(int(full.Overflow()), 1);
		CHECK_EQ(full.Count(), BotCore::kTeamMaxMembers);

		CHECK(ApplyPartyMember(full, 205, 1, 1, "m2"));   // refresh instead of a fresh sid
		CHECK_EQ(full.Count(), BotCore::kTeamMaxMembers);
	}

	CHECK(ApplyPartyDelete(table, 1));
	CHECK_EQ(table.Count(), 0);
	CHECK_EQ(int(table.Overflow()), 0);
	CHECK_EQ(int(table.UnknownHp()), 0);
}

TEST_CASE("Perception_Team_Leader")
{
	BotCore::TeamTable table;

	CHECK_EQ(int(table.LeaderSid(1, false)), int(BotCore::kTeamNone));
	CHECK_EQ(int(table.LeaderSid(1, true)), 1);

	CHECK(ApplyPartyMember(table, 7, 1, 1, "A"));   // first record
	CHECK(ApplyPartyMember(table, 8, 1, 1, "B"));
	CHECK_EQ(int(table.LeaderSid(1, false)), 7);
	CHECK_EQ(int(table.LeaderSid(1, true)), 1);

	CHECK(ApplyPartyMember(table, 8, 100, 1, "B")); // leader moved
	CHECK_EQ(int(table.LeaderSid(1, false)), 8);
	CHECK_EQ(int(table.LeaderSid(1, true)), 8);

	CHECK(ApplyPartyRemove(table, 8, 1));           // promoted hint cleared
	CHECK_EQ(int(table.LeaderSid(1, false)), 7);

	CHECK(ApplyPartyRemove(table, 7, 1));
	CHECK_EQ(int(table.LeaderSid(1, false)), int(BotCore::kTeamNone));

	CHECK(ApplyPartyMember(table, 5, 1, 1, "E"));
	table.Clear();
	CHECK_EQ(table.Count(), 0);
	CHECK_EQ(int(table.LeaderSid(1, false)), int(BotCore::kTeamNone));
	CHECK_EQ(int(table.LeaderSid(1, true)), 1);

	CHECK(ApplyPartyMember(table, 3, 1, 1, "C"));   // first into the empty table
	CHECK(ApplyPartyMember(table, 4, 1, 1, "D"));   // later record does not change the hint
	CHECK_EQ(int(table.LeaderSid(1, false)), 3);
}

TEST_CASE("Perception_Team_Build")
{
	BotCore::SelfState self = MakeSelf();
	self.inParty = true;
	self.partyLeader = false;

	BotCore::TeamTable team;
	{
		Buf b;
		AddPartyMember(b, 3, 1, "C", 3000, 2500, 80, 106, 1200, 900, 1);
		BotCore::PartyEvent ev;
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 100, ev));
		CHECK(team.Apply(ev, 1));
	}
	{
		Buf b;
		AddPartyMember(b, 2, 1, "B", 2000, 0, 70, 102, 800, 500, 1);
		BotCore::PartyEvent ev;
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 200, ev));
		CHECK(team.Apply(ev, 1));
	}
	{
		Buf b;
		AddPartyMember(b, 1, 1, "SELF", 5000, 5000, 80, 105, 3000, 3000, 1);
		BotCore::PartyEvent ev;
		CHECK(BotCore::ParsePartyEvent(b.v.data(), b.v.size(), 300, ev));
		CHECK(team.Apply(ev, 1));
	}

	BotCore::ObsTable obs;
	{
		BotCore::UnitObs u = MakeUnit(3);
		u.nation = 1;
		u.x10 = 10300;
		u.z10 = 10400;
		obs.Upsert(u);
	}

	BotCore::TeamView tv;
	BotCore::BuildTeam(self, team, obs, 1000, tv);

	CHECK(tv.inParty);
	CHECK(!tv.selfLeader);
	CHECK_EQ(tv.memberCount, 2);
	CHECK_EQ(tv.memberTotal, 2);
	CHECK_EQ(int(tv.members[0].id), 2);
	CHECK_EQ(int(tv.members[1].id), 3);
	CHECK_EQ(int(tv.leaderId), 3);                 // first record [A]
	CHECK(tv.members[1].leader);
	CHECK(!tv.members[0].leader);
	CHECK(strcmp(tv.members[1].name, "C") == 0);
	CHECK_EQ(int(tv.members[1].nation), 1);
	CHECK_EQ(int(tv.members[1].cls), 106);
	CHECK_EQ(int(tv.members[1].level), 80);
	CHECK_EQ(tv.members[1].hp, 2500);
	CHECK_EQ(tv.members[1].maxHp, 3000);
	CHECK_EQ(tv.members[1].mp, 900);
	CHECK_EQ(tv.members[1].maxMp, 1200);
	CHECK(tv.members[1].inView);
	CHECK(tv.members[1].x == 1030.0f);
	CHECK(tv.members[1].z == 1040.0f);
	CHECK(tv.members[1].dist == 50.0f);
	CHECK(!tv.members[1].dead);
	CHECK_EQ(tv.members[1].ageMs, 900u);           // nowMs 1000 - lastSeen 100
	CHECK(!tv.members[0].inView);
	CHECK(tv.members[0].x == 0.0f);
	CHECK(tv.members[0].z == 0.0f);
	CHECK(tv.members[0].dist == 0.0f);
	CHECK(tv.members[0].dead);                     // hp == 0

	{
		BotCore::ObsTable obs2;
		BotCore::UnitObs u = MakeUnit(3);
		u.nation = 1;
		u.resHpType = BotCore::kObsUserDead;
		u.x10 = 10000;
		u.z10 = 10000;
		obs2.Upsert(u);
		BotCore::TeamView tv2;
		BotCore::BuildTeam(self, team, obs2, 100, tv2);
		CHECK(tv2.members[1].dead);                // observation table marks it dead
	}

	self.partyLeader = true;
	{
		BotCore::TeamView tv3;
		BotCore::BuildTeam(self, team, obs, 1000, tv3);
		CHECK(tv3.selfLeader);
		CHECK_EQ(int(tv3.leaderId), 1);
		CHECK(!tv3.members[0].leader);
		CHECK(!tv3.members[1].leader);
	}

	self.inParty = false;
	{
		BotCore::TeamView tv4;
		BotCore::BuildTeam(self, team, obs, 1000, tv4);
		CHECK(!tv4.inParty);
		CHECK_EQ(tv4.memberCount, 0);
		CHECK_EQ(int(tv4.leaderId), int(BotCore::kTeamNone));
	}
}

TEST_CASE("Perception_ParseMoveFull")
{
	{
		Buf b;
		b.U16(5); b.U16(100); b.U16(200); b.U16(3); b.U16(45); b.U8(3);

		BotCore::MoveObs out;
		memset(&out, 0, sizeof(out));
		CHECK(BotCore::ParseMoveFull(b.v.data(), b.v.size(), out));
		CHECK_EQ(int(out.sid), 5);
		CHECK_EQ(int(out.x10), 100);
		CHECK_EQ(int(out.z10), 200);
		CHECK_EQ(int(out.y10), 3);
		CHECK_EQ(int(out.speed), 45);
		CHECK_EQ(int(out.echo), 3);
	}

	{
		Buf b;
		b.U16(5); b.U16(100); b.U16(200); b.U16(3); b.U16(45);   // 10 bytes: one short of the speed/echo tail

		BotCore::MoveObs out;
		CHECK(!BotCore::ParseMoveFull(b.v.data(), b.v.size(), out));
	}

	{
		Buf b;
		b.U16(5); b.U16(100); b.U16(200); b.U16(3); b.U16(0xFFFF); b.U8(3);   // speed -1 unknown

		BotCore::MoveObs out;
		memset(&out, 0, sizeof(out));
		CHECK(BotCore::ParseMoveFull(b.v.data(), b.v.size(), out));
		CHECK_EQ(int(out.speed), -1);
	}

	{
		BotCore::MoveObs out;
		CHECK(!BotCore::ParseMoveFull(nullptr, 0, out));
	}
}

TEST_CASE("Perception_ParseTargetHp")
{
	{
		Buf b;
		b.U16(0x1234); b.U8(1); b.U32(5650); b.U32(5266); b.U16(100);   // 13 bytes, real layout

		BotCore::TargetHpMsg out;
		memset(&out, 0, sizeof(out));
		CHECK(BotCore::ParseTargetHp(b.v.data(), b.v.size(), out));
		CHECK_EQ(int(out.tid), 0x1234);
		CHECK_EQ(int(out.echo), 1);
		CHECK_EQ(out.maxHp, 5650);
		CHECK_EQ(out.hp, 5266);
		CHECK_EQ(int(out.damage), 100);
	}

	{
		Buf b;
		b.U16(0x1234); b.U8(1); b.U32(5650); b.U32(5266);              // 12 bytes: one short
		BotCore::TargetHpMsg out;
		CHECK(!BotCore::ParseTargetHp(b.v.data(), b.v.size(), out));
	}

	{
		Buf b;
		b.U16(1); b.U8(0); b.U32(100); b.U32(101); b.U16(0);            // hp > maxHp
		BotCore::TargetHpMsg out;
		CHECK(!BotCore::ParseTargetHp(b.v.data(), b.v.size(), out));
	}

	{
		Buf b;
		b.U16(1); b.U8(0); b.U32(0); b.U32(0); b.U16(0);                // maxHp = 0
		BotCore::TargetHpMsg out;
		CHECK(!BotCore::ParseTargetHp(b.v.data(), b.v.size(), out));
	}

	{
		Buf b;
		b.U16(1); b.U8(0); b.U32(100); b.U32(0xFFFFFFFF); b.U16(0);     // hp < 0
		BotCore::TargetHpMsg out;
		CHECK(!BotCore::ParseTargetHp(b.v.data(), b.v.size(), out));
	}

	{
		BotCore::TargetHpMsg out;
		CHECK(!BotCore::ParseTargetHp(nullptr, 0, out));
	}
}

TEST_CASE("Perception_HpTable_Upsert")
{
	BotCore::HpTable table;
	CHECK_EQ(table.Count(), 0);

	BotCore::HpObs o;
	memset(&o, 0, sizeof(o));
	o.id = 5;
	o.hp = 100;
	o.maxHp = 200;
	o.atMs = 10;
	CHECK(table.Upsert(o));

	const BotCore::HpObs * p = table.Find(5);
	CHECK(p != nullptr);
	CHECK_EQ(p->hp, 100);

	// Overwrite by id, no new slot.
	BotCore::HpObs o2 = o;
	o2.hp = 150;
	o2.atMs = 20;
	CHECK(table.Upsert(o2));
	CHECK_EQ(table.Count(), 1);
	CHECK_EQ(table.Find(5)->hp, 150);
	CHECK(table.Find(5)->atMs == 20);

	// Copy independence.
	BotCore::HpTable copy = table;
	copy.Upsert(o2);
	copy.Invalidate(5);
	CHECK_EQ(table.Count(), 1);
	CHECK_EQ(copy.Count(), 0);

	// Capacity 32: the 33rd new id drops the oldest atMs.
	BotCore::HpTable full;
	for (int i = 0; i < BotCore::kHpMaxEntries; i++)
	{
		BotCore::HpObs e;
		memset(&e, 0, sizeof(e));
		e.id = (uint16_t)(100 + i);
		e.hp = i;
		e.maxHp = 1000;
		e.atMs = (uint64_t)i;
		CHECK(full.Upsert(e));
	}
	CHECK_EQ(full.Count(), BotCore::kHpMaxEntries);

	BotCore::HpObs newest;
	memset(&newest, 0, sizeof(newest));
	newest.id = 999;
	newest.hp = 7;
	newest.maxHp = 1000;
	newest.atMs = 1000;
	CHECK(full.Upsert(newest));
	CHECK_EQ(full.Count(), BotCore::kHpMaxEntries);
	CHECK(full.Find(100) == nullptr);       // atMs 0: the oldest was dropped
	CHECK(full.Find(999) != nullptr);

	// Invalidate closes the gap.
	full.Invalidate(999);
	CHECK(full.Find(999) == nullptr);
}

TEST_CASE("Perception_HpTable_Attach")
{
	BotCore::SelfState self = MakeSelf();
	BotCore::ObsTable obs;
	BotCore::NpcTable npcs;

	{
		BotCore::UnitObs u = MakeUnit(5);       // enemy player
		u.nation = 2;
		u.x10 = 10030;
		u.z10 = 10040;
		CHECK(obs.Upsert(u));
	}
	{
		BotCore::NpcObs n = MakeNpc(10005);     // npc, same numeric suffix but distinct id space
		CHECK(npcs.Upsert(n));
	}

	BotCore::HpTable hp;
	{
		BotCore::HpObs o;
		memset(&o, 0, sizeof(o));
		o.id = 5;
		o.hp = 400;
		o.maxHp = 500;
		o.lastDamage = 33;
		o.atMs = 8000;
		o.reply = true;
		CHECK(hp.Upsert(o));
	}
	{
		BotCore::HpObs o;
		memset(&o, 0, sizeof(o));
		o.id = 10005;
		o.hp = 900;
		o.maxHp = 1000;
		o.atMs = 5000;
		CHECK(hp.Upsert(o));
	}

	BotCore::PerceptionSnapshot out;
	BotCore::BuildSnapshot(self, obs, npcs, 16000, out);
	BotCore::AttachHp(out, hp, 16000);

	CHECK_EQ(out.enemyCount, 1);
	if (out.enemyCount == 1)
	{
		CHECK(out.enemies[0].hpKnown);
		CHECK_EQ(out.enemies[0].hp, 400);
		CHECK_EQ(out.enemies[0].maxHp, 500);
		CHECK_EQ(int(out.enemies[0].lastDamage), 33);
		CHECK_EQ(out.enemies[0].hpAgeMs, 8000u);
		CHECK(!out.enemies[0].hpStale);          // 8000 <= kHpStaleMs
	}
	CHECK_EQ(out.npcCount, 1);
	if (out.npcCount == 1)
	{
		CHECK(out.npcs[0].hpKnown);
		CHECK_EQ(out.npcs[0].hp, 900);
		CHECK_EQ(out.npcs[0].hpAgeMs, 11000u);
		CHECK(out.npcs[0].hpStale);              // 11000 > kHpStaleMs
	}

	// Boundary: exactly kHpStaleMs is not stale, one more is.
	BotCore::HpObs edge;
	memset(&edge, 0, sizeof(edge));
	edge.id = 5;
	edge.hp = 1;
	edge.maxHp = 2;
	edge.atMs = 6000;
	hp.Upsert(edge);

	BotCore::PerceptionSnapshot out2;
	BotCore::BuildSnapshot(self, obs, npcs, 6000 + BotCore::kHpStaleMs, out2);
	BotCore::AttachHp(out2, hp, 6000 + BotCore::kHpStaleMs);
	CHECK(!out2.enemies[0].hpStale);
	CHECK_EQ(out2.enemies[0].hpAgeMs, BotCore::kHpStaleMs);

	BotCore::PerceptionSnapshot out3;
	BotCore::BuildSnapshot(self, obs, npcs, 6000 + BotCore::kHpStaleMs + 1, out3);
	BotCore::AttachHp(out3, hp, 6000 + BotCore::kHpStaleMs + 1);
	CHECK(out3.enemies[0].hpStale);
	CHECK_EQ(out3.enemies[0].hpAgeMs, BotCore::kHpStaleMs + 1);
}

TEST_CASE("Perception_HpTable_Death")
{
	BotCore::SelfState self = MakeSelf();
	BotCore::ObsTable obs;

	BotCore::UnitObs u = MakeUnit(5);
	u.nation = 2;
	u.x10 = 10030;
	u.z10 = 10040;
	CHECK(obs.Upsert(u));

	BotCore::HpTable hp;
	BotCore::HpObs o;
	memset(&o, 0, sizeof(o));
	o.id = 5;
	o.hp = 10;
	o.maxHp = 100;
	o.atMs = 1000;
	CHECK(hp.Upsert(o));

	// The death invalidates the stored HP, exactly what BotSession does for WIZ_DEAD / OUT.
	hp.Invalidate(5);
	CHECK(hp.Find(5) == nullptr);

	BotCore::PerceptionSnapshot out;
	BotCore::BuildSnapshot(self, obs, BotCore::NpcTable(), 2000, out);
	BotCore::AttachHp(out, hp, 2000);
	CHECK_EQ(out.enemyCount, 1);
	if (out.enemyCount == 1)
	{
		CHECK(!out.enemies[0].hpKnown);
		CHECK_EQ(out.enemies[0].hpAgeMs, 0u);
	}

	// BuildSnapshot's signature and its ageMs are unchanged.
	CHECK_EQ(out.tMs, 2000u);
	CHECK_EQ(out.enemies[0].ageMs, 2000u);
}

TEST_CASE("Perception_Obs_MoveHistory")
{
	BotCore::ObsTable table;
	BotCore::UnitObs u = MakeUnit(7);
	u.nation = 1;
	u.x10 = 10000;
	u.z10 = 10000;
	u.lastSeenMs = 1000;
	CHECK(table.Upsert(u));

	const BotCore::UnitObs * o = table.Find(7);
	CHECK(o != nullptr);
	CHECK_EQ(int(o->histCount), 1);
	CHECK(o->lastMoveMs == 1000);
	CHECK_EQ(int(o->lastSpeed), -1);
	CHECK_EQ(int(o->hist[0].speed), -1);

	// ~4.5 m/s along +x: 67 x10 (6.7 m) every 1500 ms; registration + 4 moves wraps the 4-slot ring.
	CHECK(table.UpdateMove(7, 10067, 10000, 100, 45, 2500));
	CHECK(table.UpdateMove(7, 10134, 10000, 100, 45, 4000));
	CHECK(table.UpdateMove(7, 10201, 10000, 100, 45, 5500));
	CHECK(table.UpdateMove(7, 10268, 10000, 100, 45, 7000));

	o = table.Find(7);
	CHECK_EQ(int(o->histCount), BotCore::kObsHistMax);
	CHECK(o->lastMoveMs == 7000);
	CHECK_EQ(int(o->lastSpeed), 45);
	CHECK_EQ(int(o->x10), 10268);

	const BotCore::PosSample * newest = BotCore::ObsSampleBack(*o, 0);
	const BotCore::PosSample * older = BotCore::ObsSampleBack(*o, 1);
	CHECK(newest != nullptr && older != nullptr);
	CHECK_EQ(int(newest->x10), 10268);
	CHECK(newest->tMs == 7000);
	CHECK_EQ(int(older->x10), 10201);
	CHECK(older->tMs == 5500);
	CHECK(BotCore::ObsSampleBack(*o, 3) != nullptr);       // the oldest of the 5 samples survives
	CHECK_EQ(int(BotCore::ObsSampleBack(*o, 3)->x10), 10067);
	CHECK(BotCore::ObsSampleBack(*o, 4) == nullptr);

	float vx = 0.0f, vz = 0.0f;
	BotCore::EstimateVelocity(*o, 7000, vx, vz);
	CHECK(vx > 4.05f && vx < 4.95f);                       // ~4.5 m/s within 10%
	CHECK(vz == 0.0f);

	// The last packet said stopped: no velocity.
	CHECK(table.UpdateMove(7, 10268, 10000, 100, 0, 8000));
	BotCore::EstimateVelocity(*table.Find(7), 8000, vx, vz);
	CHECK(vx == 0.0f);
	CHECK(vz == 0.0f);

	// The newest sample is older than 4000 ms: no velocity.
	CHECK(table.UpdateMove(7, 10268, 10000, 100, 45, 10000));
	BotCore::EstimateVelocity(*table.Find(7), 15000, vx, vz);
	CHECK(vx == 0.0f);
	CHECK(vz == 0.0f);

	// Two samples closer than 400 ms: no velocity.
	CHECK(table.UpdateMove(7, 10200, 10000, 100, 45, 15100));
	CHECK(table.UpdateMove(7, 10230, 10000, 100, 45, 15200));
	BotCore::EstimateVelocity(*table.Find(7), 15200, vx, vz);
	CHECK(vx == 0.0f);
	CHECK(vz == 0.0f);

	// Dead reckoning advances at most 3000 ms past the newest sample.
	BotCore::ObsTable t2;
	BotCore::UnitObs u2 = MakeUnit(9);
	u2.x10 = 10000;
	u2.z10 = 10000;
	u2.lastSeenMs = 1000;
	CHECK(t2.Upsert(u2));
	CHECK(t2.UpdateMove(9, 10067, 10000, 100, 45, 2500));

	float ex = 0.0f, ez = 0.0f;
	BotCore::EstimatePosition(*t2.Find(9), 2500, ex, ez);
	CHECK(ex > 1006.6f && ex < 1006.8f);
	CHECK(ez == 1000.0f);
	BotCore::EstimatePosition(*t2.Find(9), 5500, ex, ez);   // 3000 ms ahead
	float capped = ex;
	BotCore::EstimatePosition(*t2.Find(9), 6000, ex, ez);   // 3500 ms: clamped
	CHECK(ex > capped - 0.001f && ex < capped + 0.001f);
}

TEST_CASE("Perception_Obs_PosClassify")
{
	CHECK_EQ(int(BotCore::kPosFreshMs), 3100);
	CHECK_EQ(int(BotCore::kPosLostMs), 6000);

	CHECK(BotCore::ClassifyPos(true, 0) == BotCore::POS_FRESH);
	CHECK(BotCore::ClassifyPos(true, BotCore::kPosFreshMs) == BotCore::POS_FRESH);
	CHECK(BotCore::ClassifyPos(true, BotCore::kPosFreshMs + 1) == BotCore::POS_STALE);
	CHECK(BotCore::ClassifyPos(true, BotCore::kPosLostMs) == BotCore::POS_STALE);
	CHECK(BotCore::ClassifyPos(true, BotCore::kPosLostMs + 1) == BotCore::POS_LOST);
	CHECK(BotCore::ClassifyPos(false, 60000) == BotCore::POS_FRESH);
}

TEST_CASE("Perception_Snap_MetaFields")
{
	BotCore::SelfState self = MakeSelf();
	BotCore::NpcTable npcs;

	BotCore::ObsTable obs;
	{
		BotCore::UnitObs u = MakeUnit(5);
		u.nation = 2;
		strcpy(u.name, "BotWG_E");
		u.x10 = 10030;
		u.z10 = 10040;
		u.cls = 205;
		u.level = 77;
		u.lastSeenMs = 0;               // registration at t=0, no MOVE afterwards
		CHECK(obs.Upsert(u));
	}

	BotCore::PerceptionSnapshot out;
	BotCore::BuildSnapshot(self, obs, npcs, 5000, out);

	CHECK_EQ(out.enemyCount, 1);
	if (out.enemyCount == 1)
	{
		const BotCore::UnitView & v = out.enemies[0];
		CHECK(strcmp(v.name, "BotWG_E") == 0);
		CHECK_EQ(v.ageMs, 5000u);
		CHECK_EQ(v.posAgeMs, 5000u);
		CHECK_EQ(int(v.speedField), -1);
		CHECK(!v.moving);                // unknown speed counts as stationary: no MOVE since registration
		CHECK(v.posState == BotCore::POS_FRESH);
		CHECK(v.vx == 0.0f);
		CHECK(v.vz == 0.0f);
		CHECK_EQ(int(v.src), int(BotCore::kSrcObserved));
	}

	// A MOVE with speed 0 makes the unit stationary: the same age is fresh.
	{
		BotCore::ObsTable stopped;
		BotCore::UnitObs u = MakeUnit(5);
		u.nation = 2;
		strcpy(u.name, "BotWG_E");
		u.x10 = 10030;
		u.z10 = 10040;
		u.lastSeenMs = 0;
		CHECK(stopped.Upsert(u));
		CHECK(stopped.UpdateMove(5, 10030, 10040, 0, 0, 1000));

		BotCore::PerceptionSnapshot out2;
		BotCore::BuildSnapshot(self, stopped, npcs, 6000, out2);   // pos_age 5000
		CHECK_EQ(out2.enemyCount, 1);
		if (out2.enemyCount == 1)
		{
			CHECK_EQ(out2.enemies[0].posAgeMs, 5000u);
			CHECK_EQ(out2.enemies[0].ageMs, 5000u);
			CHECK_EQ(int(out2.enemies[0].speedField), 0);
			CHECK(!out2.enemies[0].moving);
			CHECK(out2.enemies[0].posState == BotCore::POS_FRESH);
		}
	}

	// A MOVE with speed 45 at t=1000 makes the unit moving: the position ages to stale then lost.
	{
		BotCore::ObsTable walked;
		BotCore::UnitObs u = MakeUnit(5);
		u.nation = 2;
		strcpy(u.name, "BotWG_E");
		u.x10 = 10030;
		u.z10 = 10040;
		u.lastSeenMs = 0;
		CHECK(walked.Upsert(u));
		CHECK(walked.UpdateMove(5, 10030, 10040, 0, 45, 1000));

		BotCore::PerceptionSnapshot out3;
		BotCore::BuildSnapshot(self, walked, npcs, 5000, out3);   // pos_age 4000
		CHECK_EQ(out3.enemyCount, 1);
		if (out3.enemyCount == 1)
		{
			CHECK_EQ(out3.enemies[0].posAgeMs, 4000u);
			CHECK_EQ(int(out3.enemies[0].speedField), 45);
			CHECK(out3.enemies[0].moving);
			CHECK(out3.enemies[0].posState == BotCore::POS_STALE);
		}

		BotCore::PerceptionSnapshot out4;
		BotCore::BuildSnapshot(self, walked, npcs, 7500, out4);   // pos_age 6500
		CHECK_EQ(out4.enemyCount, 1);
		if (out4.enemyCount == 1)
		{
			CHECK_EQ(out4.enemies[0].posAgeMs, 6500u);
			CHECK(out4.enemies[0].moving);
			CHECK(out4.enemies[0].posState == BotCore::POS_LOST);
		}
	}
}

// Writes one WIZ_MAGIC_PROCESS payload in the server's wire order (MagicInstance.cpp BuildSkillPacket):
// u8 opcode, u32 skillId, i16 caster, i16 target, i16 data[7] = 23 bytes.
static void AddSkillEvent(Buf & b, uint8_t op, uint32_t skillId, int16_t caster, int16_t target, const int16_t data[7])
{
	b.U8(op);
	b.U32(skillId);
	b.U16((uint16_t)caster);
	b.U16((uint16_t)target);
	for (int i = 0; i < 7; i++)
		b.U16((uint16_t)data[i]);
}

TEST_CASE("Perception_ParseSkillEvent")
{
	const int16_t zero[7] = { 0, 0, 0, 0, 0, 0, 0 };

	// CASTING: skill 110518 = bytes B6 AF 01 00, caster 2984, target 10001.
	{
		Buf b;
		AddSkillEvent(b, BotCore::kMagicCasting, 110518, 2984, 10001, zero);
		CHECK_EQ(int(b.v.size()), 23);

		BotCore::SkillEvent ev;
		memset(&ev, 0, sizeof(ev));
		CHECK(BotCore::ParseSkillEvent(b.v.data(), b.v.size(), 777, ev));
		CHECK_EQ(int(ev.op), 1);
		CHECK_EQ((unsigned)ev.skillId, 110518u);
		CHECK_EQ(int(ev.caster), 2984);
		CHECK_EQ(int(ev.target), 10001);
		CHECK(ev.tMs == 777);
	}

	// EFFECTING with the target position in data[0..2] (x10, z10, y10).
	{
		const int16_t data[7] = { 12740, 8900, 123, 0, 0, 0, 0 };
		Buf b;
		AddSkillEvent(b, BotCore::kMagicEffecting, 110518, 2984, 10001, data);

		BotCore::SkillEvent ev;
		memset(&ev, 0, sizeof(ev));
		CHECK(BotCore::ParseSkillEvent(b.v.data(), b.v.size(), 1000, ev));
		CHECK_EQ(int(ev.op), 3);
		CHECK_EQ((unsigned)ev.skillId, 110518u);
		CHECK_EQ(int(ev.caster), 2984);
		CHECK_EQ(int(ev.target), 10001);
		CHECK_EQ(int(ev.data[0]), 12740);
		CHECK_EQ(int(ev.data[1]), 8900);
		CHECK_EQ(int(ev.data[2]), 123);
	}

	// A short (22-byte) packet is rejected.
	{
		Buf b;
		AddSkillEvent(b, BotCore::kMagicCasting, 110518, 2984, 10001, zero);
		BotCore::SkillEvent ev;
		memset(&ev, 0, sizeof(ev));
		CHECK(!BotCore::ParseSkillEvent(b.v.data(), 22, 777, ev));
	}

	// op outside 1..13 is rejected.
	{
		Buf b;
		AddSkillEvent(b, 0, 110518, 2984, 10001, zero);
		BotCore::SkillEvent ev;
		memset(&ev, 0, sizeof(ev));
		CHECK(!BotCore::ParseSkillEvent(b.v.data(), b.v.size(), 777, ev));
	}
	{
		Buf b;
		AddSkillEvent(b, 14, 110518, 2984, 10001, zero);
		BotCore::SkillEvent ev;
		memset(&ev, 0, sizeof(ev));
		CHECK(!BotCore::ParseSkillEvent(b.v.data(), b.v.size(), 777, ev));
	}

	// Null buffer and zero length are rejected.
	{
		BotCore::SkillEvent ev;
		memset(&ev, 0, sizeof(ev));
		CHECK(!BotCore::ParseSkillEvent(nullptr, 23, 777, ev));
		Buf b;
		AddSkillEvent(b, BotCore::kMagicCasting, 110518, 2984, 10001, zero);
		CHECK(!BotCore::ParseSkillEvent(b.v.data(), 0, 777, ev));
	}
}

TEST_CASE("Perception_SkillRing_Basic")
{
	BotCore::SkillEventRing ring;
	CHECK_EQ(ring.Count(), 0);
	CHECK_EQ((unsigned)ring.Total(), 0u);

	BotCore::SkillEvent ev;
	memset(&ev, 0, sizeof(ev));
	ev.op = BotCore::kMagicCasting;
	ev.skillId = 100;
	ev.tMs = 10;
	ring.Add(ev);
	ev.op = BotCore::kMagicEffecting;
	ev.skillId = 200;
	ev.tMs = 20;
	ring.Add(ev);

	CHECK_EQ(ring.Count(), 2);
	CHECK_EQ((unsigned)ring.Total(), 2u);
	CHECK_EQ(int(ring.At(0).skillId), 200);   // newest
	CHECK_EQ(int(ring.At(1).skillId), 100);

	// Overflow: capacity is kSkillEventRing, Total keeps counting.
	for (int i = 0; i < BotCore::kSkillEventRing + 5; i++)
	{
		ev.op = BotCore::kMagicEffecting;
		ev.skillId = (uint32_t)(1000 + i);
		ev.tMs = (uint64_t)(100 + i);
		ring.Add(ev);
	}
	CHECK_EQ(ring.Count(), BotCore::kSkillEventRing);
	CHECK_EQ((unsigned)ring.Total(), (unsigned)(2 + BotCore::kSkillEventRing + 5));
	CHECK_EQ(int(ring.At(0).skillId), 1000 + BotCore::kSkillEventRing + 4);
}

TEST_CASE("Perception_SkillRing_Queries")
{
	BotCore::SkillEventRing ring;
	BotCore::SkillEvent ev;
	memset(&ev, 0, sizeof(ev));

	ev.op = BotCore::kMagicCasting;
	ev.skillId = 110518;
	ev.caster = 2984;
	ev.target = 10001;
	ev.tMs = 1000;
	ring.Add(ev);

	ev.op = BotCore::kMagicEffecting;
	ev.caster = 2984;
	ev.target = 10001;
	ev.tMs = 2100;
	ring.Add(ev);

	ev.op = BotCore::kMagicEffecting;
	ev.caster = 2984;
	ev.target = 7777;
	ev.tMs = 3000;
	ring.Add(ev);

	// Newest EFFECTING (no caster filter) within the window.
	const BotCore::SkillEvent * found = ring.FindLatest(BotCore::kMagicEffecting, BotCore::kSkillIdAny,
		BotCore::kSkillIdAny, 3200, 5000);
	CHECK(found != nullptr);
	if (found != nullptr)
	{
		CHECK_EQ(int(found->target), 7777);
		CHECK_EQ((unsigned)found->skillId, 110518u);
	}

	// Caster + target filter: the older EFFECTING is found (newest target does not match).
	found = ring.FindLatest(BotCore::kMagicEffecting, 2984, 10001, 3200, 5000);
	CHECK(found != nullptr);
	if (found != nullptr)
		CHECK_EQ((unsigned)found->tMs, 2100u);

	// Outside the window -> nothing.
	CHECK(ring.FindLatest(BotCore::kMagicEffecting, 2984, 10001, 10000, 5000) == nullptr);

	// CountIn: two EFFECTING events overall, one for target 10001.
	CHECK_EQ(ring.CountIn(BotCore::kSkillOpAny, BotCore::kSkillIdAny, 3200, 5000), 3);
	CHECK_EQ(ring.CountIn(BotCore::kMagicEffecting, BotCore::kSkillIdAny, 3200, 5000), 2);
	CHECK_EQ(ring.CountIn(BotCore::kMagicEffecting, 10001, 3200, 5000), 1);
	CHECK_EQ(ring.CountIn(BotCore::kMagicEffecting, 10001, 10000, 5000), 0);
}

TEST_CASE("Perception_SkillRing_Copy")
{
	BotCore::SkillEventRing source;
	BotCore::SkillEvent ev;
	memset(&ev, 0, sizeof(ev));
	ev.op = BotCore::kMagicEffecting;
	ev.skillId = 110518;
	ev.target = 10001;
	ev.tMs = 500;
	source.Add(ev);

	BotCore::SkillEventRing copy = source;
	CHECK_EQ(copy.Count(), 1);
	CHECK_EQ((unsigned)copy.Total(), 1u);
	CHECK_EQ(int(copy.At(0).skillId), 110518);

	// Adding to the copy does not touch the source.
	ev.skillId = 999;
	ev.tMs = 900;
	copy.Add(ev);
	CHECK_EQ(copy.Count(), 2);
	CHECK_EQ(source.Count(), 1);
	CHECK_EQ(int(source.At(0).skillId), 110518);

	// Clearing the source does not touch the copy.
	source.Clear();
	CHECK_EQ(source.Count(), 0);
	CHECK_EQ(copy.Count(), 2);
}

namespace
{
	// All data[] zero except data[1] (d1) and data[3] (d3), which Type4 carries.
	BotCore::SkillEvent MakeEvent(uint8_t op, uint32_t skillId, int16_t caster, int16_t target,
		uint64_t tMs, int16_t d1 = 0, int16_t d3 = 0)
	{
		BotCore::SkillEvent ev;
		memset(&ev, 0, sizeof(ev));
		ev.op = op;
		ev.skillId = skillId;
		ev.caster = caster;
		ev.target = target;
		ev.tMs = tMs;
		ev.data[1] = d1;
		ev.data[3] = d3;
		return ev;
	}

	// Zeroed meta; the caller fills the type-specific fields.
	BotCore::SkillMeta MakeMeta(uint32_t skillId, uint8_t type1, uint8_t type2)
	{
		BotCore::SkillMeta m;
		memset(&m, 0, sizeof(m));
		m.skillId = skillId;
		m.type1 = type1;
		m.type2 = type2;
		return m;
	}
}

TEST_CASE("Perception_SkillMeta_Classify")
{
	// Malice-like: Type4 debuff.
	BotCore::SkillMeta malice = MakeMeta(112703, 4, 0);
	malice.buffType = 2;
	malice.isBuff = false;
	CHECK(BotCore::SkillSendsType4(malice));
	CHECK(!BotCore::SkillSendsType3(malice));

	// Great healing-like: instant Type3 heal.
	BotCore::SkillMeta great = MakeMeta(112527, 3, 0);
	great.directType = 1;
	great.firstDamage = 960;
	bool hot = true;
	CHECK(BotCore::SkillSendsType3(great));
	CHECK_EQ((unsigned)BotCore::SkillHealNominal(great, hot), 960u);
	CHECK(!hot);

	// Superior restore-like: HoT only.
	BotCore::SkillMeta hotOnly = MakeMeta(112548, 3, 0);
	hotOnly.directType = 1;
	hotOnly.firstDamage = 0;
	hotOnly.timeDamage = 2500;
	hotOnly.type3DurationSec = 30;
	hot = false;
	CHECK_EQ((unsigned)BotCore::SkillHealNominal(hotOnly, hot), 2500u);
	CHECK(hot);

	// HoT with an initial tick.
	BotCore::SkillMeta hotInit = MakeMeta(1, 3, 0);
	hotInit.directType = 1;
	hotInit.firstDamage = 100;
	hotInit.timeDamage = 200;
	hotInit.type3DurationSec = 10;
	hot = false;
	CHECK_EQ((unsigned)BotCore::SkillHealNominal(hotInit, hot), 300u);
	CHECK(hot);

	// Damage spell: not a heal.
	BotCore::SkillMeta damage = MakeMeta(110503, 3, 0);
	damage.directType = 1;
	damage.firstDamage = -300;
	hot = false;
	CHECK_EQ((unsigned)BotCore::SkillHealNominal(damage, hot), 0u);
	CHECK(!hot);

	// MP direct type: not a heal.
	BotCore::SkillMeta mp = MakeMeta(2, 3, 0);
	mp.directType = 2;
	mp.firstDamage = 500;
	hot = false;
	CHECK_EQ((unsigned)BotCore::SkillHealNominal(mp, hot), 0u);

	// timeDamage > 0 but no duration: only firstDamage.
	BotCore::SkillMeta noDur = MakeMeta(3, 3, 0);
	noDur.directType = 1;
	noDur.firstDamage = 120;
	noDur.timeDamage = 200;
	noDur.type3DurationSec = 0;
	hot = false;
	CHECK_EQ((unsigned)BotCore::SkillHealNominal(noDur, hot), 120u);
	CHECK(!hot);

	// {3, 4} pair: Type4 only, no heal.
	BotCore::SkillMeta iceArrow = MakeMeta(110615, 3, 4);
	iceArrow.buffType = 6;
	CHECK(BotCore::SkillSendsType4(iceArrow));
	CHECK(!BotCore::SkillSendsType3(iceArrow));
	hot = false;
	CHECK_EQ((unsigned)BotCore::SkillHealNominal(iceArrow, hot), 0u);

	// {1, 4}: Type4 true.
	BotCore::SkillMeta leg = MakeMeta(106520, 1, 4);
	CHECK(BotCore::SkillSendsType4(leg));

	// {4, 3}: Type4 false, Type3 true.
	BotCore::SkillMeta pair43 = MakeMeta(4, 4, 3);
	CHECK(!BotCore::SkillSendsType4(pair43));
	CHECK(BotCore::SkillSendsType3(pair43));

	// Cure: Type5 REMOVE_TYPE4 only.
	BotCore::SkillMeta cure = MakeMeta(112525, 5, 0);
	cure.type5Kind = BotCore::kType5RemoveType4;
	CHECK(BotCore::SkillIsCureDebuff(cure));
	BotCore::SkillMeta cureDisease = MakeMeta(112535, 5, 0);
	cureDisease.type5Kind = BotCore::kType5RemoveType3;
	CHECK(!BotCore::SkillIsCureDebuff(cureDisease));
	BotCore::SkillMeta resurrect = MakeMeta(112733, 5, 0);
	resurrect.type5Kind = 3;
	CHECK(!BotCore::SkillIsCureDebuff(resurrect));
}

TEST_CASE("Perception_Status_Type4")
{
	BotCore::SkillMeta malice = MakeMeta(112703, 4, 0);
	malice.buffType = 2;
	malice.isBuff = false;

	BotCore::ObservedStatusTable table;
	CHECK_EQ((int)table.Records(), 0);

	BotCore::SkillEvent ev = MakeEvent(BotCore::kMagicEffecting, 112703, 2984, 2985, 1000, 1, 150);
	CHECK(table.Observe(ev, &malice) == BotCore::kStatusRecorded);

	const BotCore::StatusObs * r = table.Find(2985, 2, 1000);
	CHECK(r != nullptr);
	if (r != nullptr)
	{
		CHECK_EQ((unsigned)r->skillId, 112703u);
		CHECK_EQ((int)r->caster, 2984);
		CHECK(!r->isBuff);
		CHECK_EQ((unsigned)r->src, (unsigned)BotCore::kSrcEstimate);
		CHECK_EQ((unsigned long long)r->startMs, 1000ull);
		CHECK_EQ((unsigned long long)r->endMs, 151000ull);
		CHECK_EQ((unsigned)BotCore::StatusRemainingMs(*r, 61000), 90000u);
	}
	CHECK(table.Find(2985, 2, 151000) == nullptr);   // boundary: endMs > nowMs
	CHECK_EQ(table.CountDebuffs(2985, 1000), 1);
	CHECK_EQ(table.CountBuffs(2985, 1000), 0);
	CHECK_EQ(table.CountDebuffs(2985, 151000), 0);

	// Events that must not produce a record.
	BotCore::ObservedStatusTable none;
	CHECK(none.Observe(MakeEvent(BotCore::kMagicEffecting, 112703, 2984, 2985, 1000, 0, 150), &malice) == BotCore::kStatusIgnored);
	CHECK(none.Observe(MakeEvent(BotCore::kMagicEffecting, 112703, 2984, 2985, 1000, 1, 0), &malice) == BotCore::kStatusIgnored);
	CHECK(none.Observe(MakeEvent(BotCore::kMagicEffecting, 112703, 2984, 2985, 1000, 1, -104), &malice) == BotCore::kStatusIgnored);
	CHECK(none.Observe(MakeEvent(BotCore::kMagicCasting, 112703, 2984, 2985, 1000, 1, 150), &malice) == BotCore::kStatusIgnored);
	CHECK(none.Observe(MakeEvent(BotCore::kMagicFlying, 112703, 2984, 2985, 1000, 1, 150), &malice) == BotCore::kStatusIgnored);
	CHECK(none.Observe(MakeEvent(BotCore::kMagicEffecting, 112703, 2984, -1, 1000, 1, 150), &malice) == BotCore::kStatusIgnored);
	CHECK(none.Observe(MakeEvent(BotCore::kMagicEffecting, 112703, 2984, 2985, 1000, 1, 150), nullptr) == BotCore::kStatusIgnored);
	BotCore::SkillMeta healMeta = MakeMeta(112527, 3, 0);
	healMeta.directType = 1;
	healMeta.firstDamage = 960;
	CHECK(none.Observe(MakeEvent(BotCore::kMagicEffecting, 112527, 2984, 2985, 1000, 1, 150), &healMeta) == BotCore::kStatusIgnored);
	CHECK_EQ((int)none.Records(), 0);
}

TEST_CASE("Perception_Status_Replace")
{
	BotCore::SkillMeta acBuff = MakeMeta(112660, 4, 0);
	acBuff.buffType = 2;
	acBuff.isBuff = true;
	BotCore::SkillMeta malice = MakeMeta(112703, 4, 0);
	malice.buffType = 2;
	malice.isBuff = false;
	BotCore::SkillMeta hpBuff = MakeMeta(112654, 4, 0);
	hpBuff.buffType = 1;
	hpBuff.isBuff = true;

	BotCore::ObservedStatusTable table;
	table.Observe(MakeEvent(BotCore::kMagicEffecting, 112660, 2984, 2985, 0, 1, 600), &acBuff);
	CHECK_EQ((int)table.Records(), 1);

	// A debuff on the same buffType replaces the buff.
	table.Observe(MakeEvent(BotCore::kMagicEffecting, 112703, 2984, 2985, 5000, 1, 150), &malice);
	CHECK_EQ((int)table.Records(), 1);
	const BotCore::StatusObs * r = table.Find(2985, 2, 5000);
	CHECK(r != nullptr);
	if (r != nullptr)
	{
		CHECK(!r->isBuff);
		CHECK_EQ((unsigned)r->skillId, 112703u);
		CHECK_EQ((unsigned long long)r->endMs, 155000ull);
	}
	CHECK_EQ(table.CountBuffs(2985, 5000), 0);

	// A buff with a different buffType lives alongside; Collect orders by endMs.
	table.Observe(MakeEvent(BotCore::kMagicEffecting, 112654, 2984, 2985, 6000, 1, 600), &hpBuff);
	BotCore::StatusObs out[8];
	int n = table.Collect(2985, 6000, out, 8);
	CHECK_EQ(n, 2);
	if (n == 2)
	{
		CHECK_EQ((unsigned long long)out[0].endMs, 155000ull);
		CHECK_EQ((unsigned)out[0].buffType, 2u);
		CHECK_EQ((unsigned long long)out[1].endMs, 606000ull);
		CHECK_EQ((unsigned)out[1].buffType, 1u);
	}

	// A successful buff on the same buffType refreshes the stale record.
	table.Observe(MakeEvent(BotCore::kMagicEffecting, 112654, 2984, 2985, 7000, 1, 600), &hpBuff);
	CHECK_EQ((int)table.Records(), 2);
	r = table.Find(2985, 1, 7000);
	CHECK(r != nullptr);
	if (r != nullptr)
		CHECK_EQ((unsigned long long)r->endMs, 607000ull);

	// Per-unit record capacity: buffType 9 evicts the earliest-expiring record (buffType 1).
	BotCore::ObservedStatusTable cap;
	for (int bt = 1; bt <= 8; bt++)
	{
		BotCore::SkillMeta m = MakeMeta((uint32_t)(2000 + bt), 4, 0);
		m.buffType = (uint8_t)bt;
		m.isBuff = false;
		cap.Observe(MakeEvent(BotCore::kMagicEffecting, m.skillId, 2984, 2990, 0, 1, (int16_t)(100 + 10 * bt)), &m);
	}
	CHECK_EQ((int)cap.Records(), 8);
	BotCore::SkillMeta extra = MakeMeta(3000, 4, 0);
	extra.buffType = 9;
	extra.isBuff = false;
	cap.Observe(MakeEvent(BotCore::kMagicEffecting, 3000, 2984, 2990, 0, 1, 500), &extra);
	CHECK_EQ((int)cap.Records(), 8);
	CHECK(cap.Find(2990, 1, 0) == nullptr);
	CHECK(cap.Find(2990, 9, 0) != nullptr);
	CHECK(cap.Find(2990, 2, 0) != nullptr);
}

TEST_CASE("Perception_Status_CureAndDeath")
{
	BotCore::SkillMeta debuff = MakeMeta(112703, 4, 0);
	debuff.buffType = 2;
	debuff.isBuff = false;
	BotCore::SkillMeta buff = MakeMeta(112654, 4, 0);
	buff.buffType = 1;
	buff.isBuff = true;
	BotCore::SkillMeta cure = MakeMeta(112525, 5, 0);
	cure.type5Kind = BotCore::kType5RemoveType4;
	BotCore::SkillMeta cureDisease = MakeMeta(112535, 5, 0);
	cureDisease.type5Kind = BotCore::kType5RemoveType3;
	BotCore::SkillMeta resurrect = MakeMeta(112733, 5, 0);
	resurrect.type5Kind = 3;

	BotCore::ObservedStatusTable table;
	table.Observe(MakeEvent(BotCore::kMagicEffecting, 112703, 2984, 2985, 0, 1, 150), &debuff);
	table.Observe(MakeEvent(BotCore::kMagicEffecting, 112654, 2984, 2985, 100, 1, 600), &buff);

	// The cure removes only the debuff; data[1] = 0 is still a cure.
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 112525, 2984, 2985, 200, 0, 0), &cure) == BotCore::kStatusCured);
	CHECK(table.Find(2985, 2, 200) == nullptr);
	CHECK(table.Find(2985, 1, 200) != nullptr);

	// Curing with no debuff still reports kStatusCured.
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 112525, 2984, 2985, 300, 1, 0), &cure) == BotCore::kStatusCured);
	CHECK(table.Find(2985, 1, 300) != nullptr);

	// A cure on another target does not touch this unit.
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 112525, 2984, 4000, 300, 1, 0), &cure) == BotCore::kStatusCured);
	CHECK(table.Find(2985, 1, 300) != nullptr);

	// REMOVE_TYPE3 and other Type5 kinds are ignored; records unchanged.
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 112535, 2984, 2985, 400, 1, 0), &cureDisease) == BotCore::kStatusIgnored);
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 112733, 2984, 2985, 500, 1, 0), &resurrect) == BotCore::kStatusIgnored);
	CHECK(table.Find(2985, 1, 500) != nullptr);

	// ClearTarget empties the unit; Clear empties everything.
	table.ClearTarget(2985);
	CHECK(table.Find(2985, 1, 500) == nullptr);
	CHECK_EQ((int)table.Units(), 0);
	table.Clear();
	CHECK_EQ((int)table.Records(), 0);
}

TEST_CASE("Perception_Status_PairSkills")
{
	BotCore::SkillMeta leg = MakeMeta(106520, 1, 4);
	leg.buffType = 6;
	leg.isBuff = false;
	BotCore::SkillMeta iceArrow = MakeMeta(110615, 3, 4);
	iceArrow.buffType = 6;
	iceArrow.isBuff = false;

	BotCore::ObservedStatusTable table;

	// Type1 half of a pair carries no duration -> ignored.
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 106520, 2984, 2985, 0, 1, 0), &leg) == BotCore::kStatusIgnored);
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 106520, 2984, 2985, 0, 1, -104), &leg) == BotCore::kStatusIgnored);

	// Type4 half is recorded.
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 106520, 2984, 2985, 1000, 1, 10), &leg) == BotCore::kStatusRecorded);
	CHECK_EQ((int)table.Records(), 1);

	// {3, 4}: the target -1 summary is ignored, the real target is recorded; same buffType -> one record.
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 110615, 2984, -1, 2000, 1, 12), &iceArrow) == BotCore::kStatusIgnored);
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 110615, 2984, 2985, 2100, 1, 12), &iceArrow) == BotCore::kStatusRecorded);
	CHECK_EQ((int)table.Records(), 1);
	const BotCore::StatusObs * r = table.Find(2985, 6, 2100);
	CHECK(r != nullptr);
	if (r != nullptr)
		CHECK_EQ((unsigned)r->skillId, 110615u);   // the last write wins

	// Collect tie on endMs: buffType ascending.
	BotCore::ObservedStatusTable tie;
	BotCore::SkillMeta d2 = MakeMeta(10, 4, 0);
	d2.buffType = 2;
	d2.isBuff = false;
	BotCore::SkillMeta d1 = MakeMeta(11, 4, 0);
	d1.buffType = 1;
	d1.isBuff = false;
	tie.Observe(MakeEvent(BotCore::kMagicEffecting, 10, 1, 50, 0, 1, 100), &d2);
	tie.Observe(MakeEvent(BotCore::kMagicEffecting, 11, 1, 50, 0, 1, 100), &d1);
	BotCore::StatusObs out[8];
	int n = tie.Collect(50, 0, out, 8);
	CHECK_EQ(n, 2);
	if (n == 2)
	{
		CHECK_EQ((unsigned)out[0].buffType, 1u);
		CHECK_EQ((unsigned)out[1].buffType, 2u);
	}
}

TEST_CASE("Perception_Status_Capacity")
{
	BotCore::SkillMeta debuff = MakeMeta(112703, 4, 0);
	debuff.buffType = 2;
	debuff.isBuff = false;

	BotCore::ObservedStatusTable table;
	for (int i = 0; i < BotCore::kObsStatusUnits; i++)
	{
		int16_t tgt = (int16_t)(3000 + i);
		uint64_t t = (uint64_t)(100 * (i + 1));
		CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 112703, 2984, tgt, t, 1, 60), &debuff) == BotCore::kStatusRecorded);
	}
	CHECK_EQ((int)table.Units(), BotCore::kObsStatusUnits);
	CHECK(table.Find(3000, 2, 3200) != nullptr);

	// The 33rd target evicts the unit with the smallest lastMs (the first).
	int16_t fresh = (int16_t)(3000 + BotCore::kObsStatusUnits);
	CHECK(table.Observe(MakeEvent(BotCore::kMagicEffecting, 112703, 2984, fresh, 5000, 1, 60), &debuff) == BotCore::kStatusRecorded);
	CHECK_EQ((int)table.Units(), BotCore::kObsStatusUnits);
	CHECK(table.Find(3000, 2, 5000) == nullptr);
	CHECK(table.Find(fresh, 2, 5000) != nullptr);

	// Copies are independent.
	BotCore::ObservedStatusTable copy = table;
	CHECK_EQ((int)copy.Units(), BotCore::kObsStatusUnits);
	copy.Clear();
	CHECK_EQ((int)copy.Units(), 0);
	CHECK_EQ((int)table.Units(), BotCore::kObsStatusUnits);

	// Prune drops expired records and empties their units.
	BotCore::ObservedStatusTable prune;
	BotCore::SkillMeta b1 = MakeMeta(20, 4, 0);
	b1.buffType = 1;
	b1.isBuff = false;
	BotCore::SkillMeta b2 = MakeMeta(21, 4, 0);
	b2.buffType = 2;
	b2.isBuff = false;
	prune.Observe(MakeEvent(BotCore::kMagicEffecting, 20, 1, 2985, 0, 1, 10), &b1);
	prune.Observe(MakeEvent(BotCore::kMagicEffecting, 21, 1, 2985, 0, 1, 60), &b2);
	prune.Observe(MakeEvent(BotCore::kMagicEffecting, 20, 1, 2986, 0, 1, 10), &b1);
	prune.Prune(20000);
	CHECK_EQ((int)prune.Records(), 1);
	CHECK_EQ((int)prune.Units(), 1);
	CHECK(prune.Find(2985, 2, 20000) != nullptr);
}

TEST_CASE("Perception_HealRing")
{
	BotCore::SkillMeta great = MakeMeta(112527, 3, 0);
	great.directType = 1;
	great.firstDamage = 960;
	BotCore::SkillMeta hotOnly = MakeMeta(112548, 3, 0);
	hotOnly.directType = 1;
	hotOnly.timeDamage = 2500;
	hotOnly.type3DurationSec = 30;
	BotCore::SkillMeta damage = MakeMeta(110503, 3, 0);
	damage.directType = 1;
	damage.firstDamage = -300;
	BotCore::SkillMeta debuff = MakeMeta(112703, 4, 0);
	debuff.buffType = 2;
	debuff.isBuff = false;

	BotCore::HealObsRing ring;
	// data[1] = 0 still stores a heal (the server leaves data[1] as sent).
	CHECK(ring.Observe(MakeEvent(BotCore::kMagicEffecting, 112527, 2984, 2985, 1000, 0, 0), &great));
	CHECK(ring.Observe(MakeEvent(BotCore::kMagicEffecting, 112548, 2984, 2985, 2000, 0, 0), &hotOnly));

	// Non-heals, wrong op, target -1 and null meta do not store.
	CHECK(!ring.Observe(MakeEvent(BotCore::kMagicEffecting, 110503, 2984, 2985, 2500, 1, 0), &damage));
	CHECK(!ring.Observe(MakeEvent(BotCore::kMagicEffecting, 112703, 2984, 2985, 2600, 1, 150), &debuff));
	CHECK(!ring.Observe(MakeEvent(BotCore::kMagicCasting, 112527, 2984, 2985, 2700, 1, 0), &great));
	CHECK(!ring.Observe(MakeEvent(BotCore::kMagicEffecting, 112527, 2984, -1, 2800, 1, 0), &great));
	CHECK(!ring.Observe(MakeEvent(BotCore::kMagicEffecting, 112527, 2984, 2985, 2900, 1, 0), nullptr));
	CHECK_EQ(ring.Count(), 2);

	CHECK_EQ((unsigned)ring.SumNominal(2985, 3000, 5000), 3460u);
	CHECK_EQ((unsigned)ring.SumNominal(2985, 3000, 1500), 2500u);
	CHECK_EQ((unsigned)ring.SumNominal(9999, 3000, 5000), 0u);
	CHECK_EQ((unsigned)ring.SumNominal(BotCore::kSkillIdAny, 3000, 5000), 3460u);
	CHECK_EQ(ring.CountIn(2985, 3000, 5000), 2);
	CHECK_EQ(ring.CountIn(2985, 3000, 1500), 1);

	// Ring overflow.
	BotCore::HealObsRing big;
	for (int i = 0; i < 70; i++)
		big.Observe(MakeEvent(BotCore::kMagicEffecting, 112527, 2984, 2985, (uint64_t)(10000 + i), 1, 0), &great);
	CHECK_EQ(big.Count(), BotCore::kHealObsRing);
	CHECK_EQ((unsigned)big.Total(), 70u);
	CHECK_EQ((unsigned long long)big.At(0).tMs, 10069ull);

	// Copies are independent.
	BotCore::HealObsRing copy = big;
	CHECK_EQ(copy.Count(), BotCore::kHealObsRing);
	copy.Clear();
	CHECK_EQ(copy.Count(), 0);
	CHECK_EQ(big.Count(), BotCore::kHealObsRing);
}
