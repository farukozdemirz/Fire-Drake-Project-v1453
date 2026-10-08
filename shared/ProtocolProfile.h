#pragma once

#include <cstdint>
#include <cstdlib>

// Runtime protocol profile (ADR-0068). CLIENT_VERSION 0 = legacy: compile-time
// __VERSION behaviour, unchanged from before this file existed.
namespace ProtocolProfile
{
	// Keys as used by the client executables (see ADR-0068 context).
	const uint64_t kKeyLegacy1453 = 0x7412580096385200ULL; // our local 1453 client (modified exe)
	const uint64_t kKeyUsko1453To1699 = 0x1257091582190465ULL; // official 1453..1534 era, AlphaGame/KODevelopers 1534
	const uint64_t kKey1298To1452 = 0x1234567890123456ULL;
	const uint64_t kKey1700Plus = 0x1207500120128966ULL;

	// Process-wide runtime values; set once at startup from the ini, read-only afterwards.
	inline uint16_t g_configuredClientVersion = 0; // 0 = legacy
	inline uint64_t g_configuredCryptoKey = 0;     // 0 = derive from version

	// Effective client version: configured value, or the compile-time version when 0.
	inline uint16_t EffectiveClientVersion(uint16_t configured, uint16_t compileTimeVersion)
	{
		return configured != 0 ? configured : compileTimeVersion;
	}

	// Private key for a client version; must reproduce the old #if table for the legacy path:
	// >= 1700 -> kKey1700Plus; 1298..1452 -> kKey1298To1452; 1453 -> kKeyLegacy1453;
	// 1454..1699 -> kKeyUsko1453To1699; anything below 1298 -> kKeyLegacy1453 (old #else branch).
	inline uint64_t PrivateKeyForVersion(uint16_t version)
	{
		if (version >= 1700)
			return kKey1700Plus;

		if (version >= 1298 && version < 1453)
			return kKey1298To1452;

		if (version == 1453)
			return kKeyLegacy1453;

		if (version > 1453)
			return kKeyUsko1453To1699;

		return kKeyLegacy1453;
	}

	// configuredKey != 0 wins; otherwise PrivateKeyForVersion(effectiveVersion).
	inline uint64_t ResolvePrivateKey(uint64_t configuredKey, uint16_t effectiveVersion)
	{
		return configuredKey != 0 ? configuredKey : PrivateKeyForVersion(effectiveVersion);
	}

	// Login server rules, formerly #if blocks:
	inline bool ServerListEcho(uint16_t version)          // version >= 1500
	{
		return version >= 1500;
	}

	inline bool ServerListLanIp(uint16_t version)         // version >= 1888
	{
		return version >= 1888;
	}

	inline bool ServerListExtended(uint16_t version)      // version >= 1453
	{
		return version >= 1453;
	}

	inline uint8_t ServerListUnknownByte(uint16_t version) // version < 1600 ? 1 : 0
	{
		return version < 1600 ? uint8_t(1) : uint8_t(0);
	}

	// Version reply of LS_VERSION_REQ: configured != 0 -> configured; else the DB's latest version.
	inline int16_t LoginVersionReply(uint16_t configured, int16_t dbLatest)
	{
		return configured != 0 ? int16_t(configured) : dbLatest;
	}

	// Parses the ini CRYPTO_KEY string: "" or "0" -> 0; hex with or without 0x prefix -> value;
	// anything unparsable -> 0 (caller then derives from version). Never throws.
	inline uint64_t ParseCryptoKey(const char * text)
	{
		if (text == nullptr)
			return 0;

		const char * p = text;
		while (*p == ' ' || *p == '\t')
			++p;

		if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
			p += 2;

		// Count hex digits ourselves: strtoull would otherwise accept a sign or
		// whitespace here, and saturate silently on overflow.
		const char * digits = p;
		while (*digits == '0')
			++digits;

		int significant = 0;
		const char * q = digits;
		while ((*q >= '0' && *q <= '9') || (*q >= 'a' && *q <= 'f') || (*q >= 'A' && *q <= 'F'))
		{
			++significant;
			++q;
		}

		if (q == p || significant > 16)
			return 0;

		char * end = nullptr;
		const unsigned long long value = strtoull(p, &end, 16);
		if (end != q)
			return 0;

		while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')
			++end;

		if (*end != '\0')
			return 0;

		return uint64_t(value);
	}

	// Convenience accessors over the inline globals, using the compile-time version passed in.
	inline uint16_t ClientVersion(uint16_t compileTimeVersion)
	{
		return EffectiveClientVersion(g_configuredClientVersion, compileTimeVersion);
	}

	inline uint64_t PrivateKey(uint16_t compileTimeVersion)
	{
		return ResolvePrivateKey(g_configuredCryptoKey, ClientVersion(compileTimeVersion));
	}
}
