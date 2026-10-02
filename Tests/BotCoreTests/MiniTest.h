#pragma once

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace minitest
{
	struct RequireFailure {};

	struct TestCase
	{
		const char * name;
		void (*fn)();
		const char * file;
		int line;
	};

	// Shared across translation units; inline function keeps a single definition.
	inline std::vector<TestCase> & Registry()
	{
		static std::vector<TestCase> registry;
		return registry;
	}

	inline bool & CurrentFailed()
	{
		static bool failed = false;
		return failed;
	}

	struct Registrar
	{
		Registrar(const char * name, void (*fn)(), const char * file, int line)
		{
			TestCase tc;
			tc.name = name;
			tc.fn = fn;
			tc.file = file;
			tc.line = line;
			Registry().push_back(tc);
		}
	};

	inline void ReportFailure(const char * file, int line, const std::string & text)
	{
		std::printf("%s(%d): %s\n", file, line, text.c_str());
		CurrentFailed() = true;
	}

	inline int RunAll(int argc, char ** argv)
	{
		const char * filter = 0;
		bool listOnly = false;

		for (int i = 1; i < argc; ++i)
		{
			std::string arg = argv[i];
			if (arg == "--list")
			{
				listOnly = true;
			}
			else if (filter == 0)
			{
				filter = argv[i];
			}
			else
			{
				std::printf("usage: %s [--list] [name-filter]\n", argv[0]);
				return 2;
			}
		}

		const std::vector<TestCase> & registry = Registry();

		if (listOnly)
		{
			for (size_t i = 0; i < registry.size(); ++i)
			{
				std::printf("%s\n", registry[i].name);
			}
			return 0;
		}

		int matched = 0;
		int failed = 0;

		for (size_t i = 0; i < registry.size(); ++i)
		{
			const TestCase & tc = registry[i];
			if (filter != 0 && std::string(tc.name).find(filter) == std::string::npos)
			{
				continue;
			}

			++matched;
			CurrentFailed() = false;
			try
			{
				tc.fn();
			}
			catch (const RequireFailure &)
			{
			}

			if (CurrentFailed())
			{
				++failed;
				std::printf("[FAIL] %s\n", tc.name);
			}
			else
			{
				std::printf("[ OK ] %s\n", tc.name);
			}
		}

		if (filter != 0 && matched == 0)
		{
			std::printf("no tests matched\n");
			return 2;
		}

		std::printf("%d tests, %d failed\n", matched, failed);
		return failed == 0 ? 0 : 1;
	}
}

#define MINITEST_CONCAT2(a, b) a##b
#define MINITEST_CONCAT(a, b) MINITEST_CONCAT2(a, b)
#define MINITEST_UNIQUE(prefix) MINITEST_CONCAT(prefix, __COUNTER__)

#define MINITEST_CREATE_AND_REGISTER(f, name) \
	static void f(); \
	static ::minitest::Registrar MINITEST_UNIQUE(minitest_reg_)((name), &f, __FILE__, __LINE__); \
	static void f()

#define TEST_CASE(name) MINITEST_CREATE_AND_REGISTER(MINITEST_UNIQUE(minitest_test_), name)

#define CHECK(expr) \
	do \
	{ \
		if (!(expr)) \
		{ \
			std::ostringstream minitest_oss_; \
			minitest_oss_ << "CHECK(" #expr ") failed"; \
			::minitest::ReportFailure(__FILE__, __LINE__, minitest_oss_.str()); \
		} \
	} while (0)

#define CHECK_EQ(a, b) \
	do \
	{ \
		if (!((a) == (b))) \
		{ \
			std::ostringstream minitest_oss_; \
			minitest_oss_ << "CHECK_EQ(" #a ", " #b ") failed: " << (a) << " != " << (b); \
			::minitest::ReportFailure(__FILE__, __LINE__, minitest_oss_.str()); \
		} \
	} while (0)

#define REQUIRE(expr) \
	do \
	{ \
		if (!(expr)) \
		{ \
			std::ostringstream minitest_oss_; \
			minitest_oss_ << "REQUIRE(" #expr ") failed"; \
			::minitest::ReportFailure(__FILE__, __LINE__, minitest_oss_.str()); \
			throw ::minitest::RequireFailure(); \
		} \
	} while (0)
