#include "MiniTest.h"

#include <BotCore/ScriptPlan.h>

#include <string>

TEST_CASE("Script_ParseValid")
{
	std::string text =
		"# header\r\n"
		"\r\n"
		"0 move BotWP_K 120 340\r\n"
		"1500   ATTACK BotWP_K BotWP_E 10\n"
		"1500 pchat BotMF_K hello #1 world\n"
		"  4000\tsnap BotMF_K  \n";

	BotCore::ScriptParseResult r = BotCore::ParseScript(text);
	CHECK_EQ(int(r.error), int(BotCore::SCRIPT_OK));
	CHECK_EQ(int(r.steps.size()), 4);
	if (r.steps.size() == 4)
	{
		CHECK_EQ(int(r.steps[0].offsetMs), 0);
		CHECK_EQ(r.steps[0].command, std::string("move BotWP_K 120 340"));
		CHECK_EQ(int(r.steps[0].line), 3);

		CHECK_EQ(int(r.steps[1].offsetMs), 1500);
		CHECK_EQ(r.steps[1].command, std::string("ATTACK BotWP_K BotWP_E 10"));
		CHECK_EQ(int(r.steps[1].line), 4);

		CHECK_EQ(int(r.steps[2].offsetMs), 1500);
		CHECK_EQ(r.steps[2].command, std::string("pchat BotMF_K hello #1 world"));
		CHECK_EQ(int(r.steps[2].line), 5);

		CHECK_EQ(int(r.steps[3].offsetMs), 4000);
		CHECK_EQ(r.steps[3].command, std::string("snap BotMF_K"));
		CHECK_EQ(int(r.steps[3].line), 6);
	}

	BotCore::ScriptParseResult r2 = BotCore::ParseScript("0 list");
	CHECK_EQ(int(r2.error), int(BotCore::SCRIPT_OK));
	CHECK_EQ(int(r2.steps.size()), 1);
}

TEST_CASE("Script_VerbWhitelist")
{
	const char * verbs[] =
	{
		"move", "stop", "attack", "cast", "pot", "sit", "stand", "target", "regene",
		"pinvite", "paccept", "pdecline", "pleave", "ppromote", "pkick", "pchat",
		"see", "npcs", "snap", "list"
	};

	for (int i = 0; i < 20; ++i)
		CHECK(BotCore::IsScriptVerb(verbs[i]));

	CHECK(BotCore::IsScriptVerb("MOVE"));
	CHECK(BotCore::IsScriptVerb("Snap"));
	CHECK(BotCore::IsScriptVerb("PCHAT"));

	CHECK(!BotCore::IsScriptVerb("spawn"));
	CHECK(!BotCore::IsScriptVerb("despawn"));
	CHECK(!BotCore::IsScriptVerb("match"));
	CHECK(!BotCore::IsScriptVerb("scenario"));
	CHECK(!BotCore::IsScriptVerb("script"));
	CHECK(!BotCore::IsScriptVerb(""));
	CHECK(!BotCore::IsScriptVerb("move2"));
	CHECK(!BotCore::IsScriptVerb("mov"));
	CHECK(!BotCore::IsScriptVerb("list "));

	BotCore::ScriptParseResult r = BotCore::ParseScript("0 spawn BotWP_K");
	CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_BAD_VERB));
	CHECK_EQ(int(r.errorLine), 1);
}

TEST_CASE("Script_OffsetRules")
{
	CHECK_EQ(int(BotCore::ParseScript("x move a 1 1").error), int(BotCore::SCRIPT_ERR_BAD_OFFSET));
	CHECK_EQ(int(BotCore::ParseScript("-5 stop all").error), int(BotCore::SCRIPT_ERR_BAD_OFFSET));
	CHECK_EQ(int(BotCore::ParseScript("5x stop all").error), int(BotCore::SCRIPT_ERR_BAD_OFFSET));
	CHECK_EQ(int(BotCore::ParseScript("1.5 stop all").error), int(BotCore::SCRIPT_ERR_BAD_OFFSET));
	CHECK_EQ(int(BotCore::ParseScript("1234567890 stop all").error), int(BotCore::SCRIPT_ERR_BAD_OFFSET));
	CHECK_EQ(int(BotCore::ParseScript("600001 stop all").error), int(BotCore::SCRIPT_ERR_OFFSET_RANGE));
	CHECK_EQ(int(BotCore::ParseScript("600000 stop all").error), int(BotCore::SCRIPT_OK));

	BotCore::ScriptParseResult order = BotCore::ParseScript("100 stop all\n99 stop all");
	CHECK_EQ(int(order.error), int(BotCore::SCRIPT_ERR_OFFSET_ORDER));
	CHECK_EQ(int(order.errorLine), 2);

	BotCore::ScriptParseResult equal = BotCore::ParseScript("100 stop all\n100 list");
	CHECK_EQ(int(equal.error), int(BotCore::SCRIPT_OK));
	CHECK_EQ(int(equal.steps.size()), 2);

	CHECK_EQ(int(BotCore::ParseScript("0 stop all").error), int(BotCore::SCRIPT_OK));
}

TEST_CASE("Script_LineRules")
{
	{
		BotCore::ScriptParseResult r = BotCore::ParseScript("100");
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_MISSING_COMMAND));
	}
	{
		BotCore::ScriptParseResult r = BotCore::ParseScript("100   ");
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_MISSING_COMMAND));
	}
	{
		BotCore::ScriptParseResult r = BotCore::ParseScript("0 pchat a b\001c");
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_CONTROL_CHAR));
	}
	{
		BotCore::ScriptParseResult r = BotCore::ParseScript("# a\001b\n0 list");
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_CONTROL_CHAR));
		CHECK_EQ(int(r.errorLine), 1);
	}
	{
		BotCore::ScriptParseResult r = BotCore::ParseScript("0\tlist");
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_OK));
	}
	{
		BotCore::ScriptParseResult r = BotCore::ParseScript("0 list\rx");
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_CONTROL_CHAR));
	}
	{
		std::string line = "0 pchat a " + std::string(245, 'x');
		CHECK_EQ(line.size(), size_t(255));
		BotCore::ScriptParseResult r = BotCore::ParseScript(line);
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_OK));
	}
	{
		std::string line = "0 pchat a " + std::string(246, 'x');
		CHECK_EQ(line.size(), size_t(256));
		BotCore::ScriptParseResult r = BotCore::ParseScript(line);
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_LINE_TOO_LONG));
	}
	{
		std::string line = "0 pchat a " + std::string(245, 'x') + "\r\n";
		CHECK_EQ(line.size(), size_t(257));
		BotCore::ScriptParseResult r = BotCore::ParseScript(line);
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_OK));
	}
}

TEST_CASE("Script_Limits")
{
	{
		BotCore::ScriptParseResult r = BotCore::ParseScript(std::string(8193, '#'));
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_TOO_LARGE));
		CHECK_EQ(int(r.errorLine), 0);
	}
	{
		std::string text;
		for (int i = 0; i < 127; ++i)
			text += "#" + std::string(62, '-') + "\n";
		text += "0 list" + std::string(58, ' ');
		CHECK_EQ(text.size(), size_t(8192));
		BotCore::ScriptParseResult r = BotCore::ParseScript(text);
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_OK));
		CHECK_EQ(int(r.steps.size()), 1);
	}
	{
		std::string text;
		for (int i = 0; i < 127; ++i)
			text += "#" + std::string(62, '-') + "\n";
		text += "0 list" + std::string(58, ' ');
		text += "x";
		CHECK_EQ(text.size(), size_t(8193));
		BotCore::ScriptParseResult r = BotCore::ParseScript(text);
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_TOO_LARGE));
	}
	{
		std::string text;
		for (int i = 0; i < 129; ++i)
			text += "#\n";
		BotCore::ScriptParseResult r = BotCore::ParseScript(text);
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_TOO_MANY_LINES));
		CHECK_EQ(int(r.errorLine), 0);
	}
	{
		std::string text;
		for (int i = 0; i < 128; ++i)
			text += "#\n";
		BotCore::ScriptParseResult r = BotCore::ParseScript(text);
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_EMPTY));
	}
	{
		BotCore::ScriptParseResult r = BotCore::ParseScript("");
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_EMPTY));
	}
	{
		std::string text;
		for (int i = 0; i < 100; ++i)
			text += "0 list\n";
		BotCore::ScriptParseResult r = BotCore::ParseScript(text);
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_OK));
		CHECK_EQ(int(r.steps.size()), 100);
	}
	{
		std::string text;
		for (int i = 0; i < 101; ++i)
			text += "0 list\n";
		BotCore::ScriptParseResult r = BotCore::ParseScript(text);
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_TOO_MANY_STEPS));
		CHECK_EQ(int(r.errorLine), 101);
		CHECK(r.steps.empty());
	}
}

TEST_CASE("Script_ErrorLineNumbers")
{
	{
		BotCore::ScriptParseResult r = BotCore::ParseScript("# c\n\n0 list\n5 bogus x\n");
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_ERR_BAD_VERB));
		CHECK_EQ(int(r.errorLine), 4);
		CHECK(r.steps.empty());
	}
	{
		BotCore::ScriptParseResult r = BotCore::ParseScript("0 list\n\n\n7 move");
		CHECK_EQ(int(r.error), int(BotCore::SCRIPT_OK));
		CHECK_EQ(int(r.steps.size()), 2);
		CHECK_EQ(int(r.steps[1].line), 4);
	}
	{
		bool allText = true;
		for (int e = 0; e <= int(BotCore::SCRIPT_ERR_TOO_MANY_STEPS); ++e)
		{
			const char * text = BotCore::ScriptErrorText((BotCore::ScriptError)e);
			if (text == 0 || text[0] == '\0')
				allText = false;
		}
		CHECK(allText);
		CHECK_EQ(std::string(BotCore::ScriptErrorText(BotCore::SCRIPT_OK)), std::string("ok"));
	}
}
