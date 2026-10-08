#include "MiniTest.h"

#include <shared/ProtocolProfile.h>

#include <cstdint>

namespace
{
	// Sets the process-wide profile for one test and restores the previous values on exit.
	struct ScopedProfile
	{
		uint16_t savedVersion;
		uint64_t savedKey;

		ScopedProfile(uint16_t version, uint64_t key)
			: savedVersion(ProtocolProfile::g_configuredClientVersion),
			savedKey(ProtocolProfile::g_configuredCryptoKey)
		{
			ProtocolProfile::g_configuredClientVersion = version;
			ProtocolProfile::g_configuredCryptoKey = key;
		}

		~ScopedProfile()
		{
			ProtocolProfile::g_configuredClientVersion = savedVersion;
			ProtocolProfile::g_configuredCryptoKey = savedKey;
		}
	};

	const uint16_t kCompileTimeVersion = 1453; // shared/version.h
	const int16_t kDbLatestVersion = 1473;
}

TEST_CASE("Protocol_PrivateKeyForVersionMatchesLegacyTable")
{
	// The old #if table, as compiled with __VERSION 1453.
	CHECK_EQ(ProtocolProfile::PrivateKeyForVersion(1453), 0x7412580096385200ULL);

	CHECK_EQ(ProtocolProfile::PrivateKeyForVersion(1298), 0x1234567890123456ULL);
	CHECK_EQ(ProtocolProfile::PrivateKeyForVersion(1452), 0x1234567890123456ULL);

	CHECK_EQ(ProtocolProfile::PrivateKeyForVersion(1700), 0x1207500120128966ULL);
	CHECK_EQ(ProtocolProfile::PrivateKeyForVersion(2369), 0x1207500120128966ULL);

	CHECK_EQ(ProtocolProfile::PrivateKeyForVersion(1454), 0x1257091582190465ULL);
	CHECK_EQ(ProtocolProfile::PrivateKeyForVersion(1534), 0x1257091582190465ULL);
	CHECK_EQ(ProtocolProfile::PrivateKeyForVersion(1699), 0x1257091582190465ULL);

	CHECK_EQ(ProtocolProfile::PrivateKeyForVersion(1097), 0x7412580096385200ULL);
	CHECK_EQ(ProtocolProfile::PrivateKeyForVersion(1297), 0x7412580096385200ULL);
}

TEST_CASE("Protocol_EffectiveClientVersion")
{
	CHECK_EQ(int(ProtocolProfile::EffectiveClientVersion(0, 1453)), 1453);
	CHECK_EQ(int(ProtocolProfile::EffectiveClientVersion(1534, 1453)), 1534);
	CHECK_EQ(int(ProtocolProfile::EffectiveClientVersion(1453, 1453)), 1453);
}

TEST_CASE("Protocol_ResolvePrivateKey")
{
	CHECK_EQ(ProtocolProfile::ResolvePrivateKey(0, 1534), 0x1257091582190465ULL);
	CHECK_EQ(ProtocolProfile::ResolvePrivateKey(0, 1453), 0x7412580096385200ULL);
	CHECK_EQ(ProtocolProfile::ResolvePrivateKey(0xABCDEFULL, 1534), 0xABCDEFULL);
	CHECK_EQ(ProtocolProfile::ResolvePrivateKey(0xABCDEFULL, 1453), 0xABCDEFULL);
}

TEST_CASE("Protocol_LoginServerListRules")
{
	CHECK(!ProtocolProfile::ServerListEcho(1453));
	CHECK(!ProtocolProfile::ServerListEcho(1499));
	CHECK(ProtocolProfile::ServerListEcho(1500));
	CHECK(ProtocolProfile::ServerListEcho(1534));

	CHECK(!ProtocolProfile::ServerListLanIp(1534));
	CHECK(!ProtocolProfile::ServerListLanIp(1887));
	CHECK(ProtocolProfile::ServerListLanIp(1888));

	CHECK(!ProtocolProfile::ServerListExtended(1452));
	CHECK(ProtocolProfile::ServerListExtended(1453));
	CHECK(ProtocolProfile::ServerListExtended(1534));

	CHECK_EQ(int(ProtocolProfile::ServerListUnknownByte(1453)), 1);
	CHECK_EQ(int(ProtocolProfile::ServerListUnknownByte(1534)), 1);
	CHECK_EQ(int(ProtocolProfile::ServerListUnknownByte(1599)), 1);
	CHECK_EQ(int(ProtocolProfile::ServerListUnknownByte(1600)), 0);
}

TEST_CASE("Protocol_LoginVersionReply")
{
	CHECK_EQ(int(ProtocolProfile::LoginVersionReply(0, 1473)), 1473);
	CHECK_EQ(int(ProtocolProfile::LoginVersionReply(1534, 1473)), 1534);
	CHECK_EQ(int(ProtocolProfile::LoginVersionReply(0, 0)), 0);
}

TEST_CASE("Protocol_ParseCryptoKey")
{
	CHECK_EQ(ProtocolProfile::ParseCryptoKey(""), 0ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("0"), 0ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("1257091582190465"), 0x1257091582190465ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("0x1257091582190465"), 0x1257091582190465ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("0X1257091582190465"), 0x1257091582190465ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("abcdef"), 0xABCDEFULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("zz"), 0ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey(nullptr), 0ULL);

	// Rejected rather than half-parsed, sign-wrapped or saturated.
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("0x"), 0ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("12zz"), 0ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("-1"), 0ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("0x0x12"), 0ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("12570915821904650"), 0ULL); // 17 hex digits

	CHECK_EQ(ProtocolProfile::ParseCryptoKey("00001257091582190465"), 0x1257091582190465ULL);
	CHECK_EQ(ProtocolProfile::ParseCryptoKey("FFFFFFFFFFFFFFFF"), 0xFFFFFFFFFFFFFFFFULL);
}

// K3: CLIENT_VERSION=0, CRYPTO_KEY=0 must behave exactly like the old compile-time build.
TEST_CASE("Protocol_LegacyProfileHandshake")
{
	ScopedProfile profile(0, 0);

	const uint16_t v = ProtocolProfile::ClientVersion(kCompileTimeVersion);
	CHECK_EQ(int(v), 1453); // VersionCheck reply
	CHECK_EQ(ProtocolProfile::PrivateKey(kCompileTimeVersion), 0x7412580096385200ULL); // CJvCryption::Init

	CHECK(!ProtocolProfile::ServerListEcho(v)); // HandleServerlist: no echo read/write
	CHECK(!ProtocolProfile::ServerListLanIp(v)); // UpdateServerList: no LAN IP
	CHECK(ProtocolProfile::ServerListExtended(v)); // UpdateServerList: >= 1453 block present
	CHECK_EQ(int(ProtocolProfile::ServerListUnknownByte(v)), 1);

	// HandleVersion: the DB's latest version.
	CHECK_EQ(int(ProtocolProfile::LoginVersionReply(ProtocolProfile::g_configuredClientVersion, kDbLatestVersion)), int(kDbLatestVersion));
}

// K4: CLIENT_VERSION=1534, CRYPTO_KEY=0.
TEST_CASE("Protocol_Client1534ProfileHandshake")
{
	ScopedProfile profile(1534, 0);

	const uint16_t v = ProtocolProfile::ClientVersion(kCompileTimeVersion);
	CHECK_EQ(int(v), 1534);
	CHECK_EQ(ProtocolProfile::PrivateKey(kCompileTimeVersion), 0x1257091582190465ULL);

	CHECK(ProtocolProfile::ServerListEcho(v));
	CHECK(!ProtocolProfile::ServerListLanIp(v));
	CHECK(ProtocolProfile::ServerListExtended(v));
	CHECK_EQ(int(ProtocolProfile::ServerListUnknownByte(v)), 1);

	CHECK_EQ(int(ProtocolProfile::LoginVersionReply(ProtocolProfile::g_configuredClientVersion, kDbLatestVersion)), 1534);
}

TEST_CASE("Protocol_ConfiguredCryptoKeyOverridesVersion")
{
	{
		ScopedProfile profile(1534, ProtocolProfile::ParseCryptoKey("0xABCDEF"));
		CHECK_EQ(int(ProtocolProfile::ClientVersion(kCompileTimeVersion)), 1534);
		CHECK_EQ(ProtocolProfile::PrivateKey(kCompileTimeVersion), 0xABCDEFULL);
	}

	{
		ScopedProfile profile(0, ProtocolProfile::ParseCryptoKey("1257091582190465"));
		CHECK_EQ(int(ProtocolProfile::ClientVersion(kCompileTimeVersion)), 1453);
		CHECK_EQ(ProtocolProfile::PrivateKey(kCompileTimeVersion), 0x1257091582190465ULL);
	}

	// Restored to the legacy defaults after each scope.
	CHECK_EQ(int(ProtocolProfile::g_configuredClientVersion), 0);
	CHECK_EQ(ProtocolProfile::g_configuredCryptoKey, 0ULL);
}
