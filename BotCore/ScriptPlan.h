#pragma once

// Pure parser/validator for bot test scripts (ADR-0017 Ek F4-19). No I/O, no server headers.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BotCore
{
	constexpr size_t   kScriptMaxBytes = 8192;
	constexpr size_t   kScriptMaxLines = 128;     // physical lines, comments and blanks included
	constexpr size_t   kScriptMaxSteps = 100;
	constexpr size_t   kScriptMaxLineLen = 255;   // without the line terminator
	constexpr uint32_t kScriptMaxOffsetMs = 600000;

	enum ScriptError
	{
		SCRIPT_OK = 0,
		SCRIPT_ERR_TOO_LARGE,         // text longer than kScriptMaxBytes (line 0)
		SCRIPT_ERR_TOO_MANY_LINES,    // more than kScriptMaxLines physical lines (line 0)
		SCRIPT_ERR_EMPTY,             // no step at all (line 0)
		SCRIPT_ERR_LINE_TOO_LONG,
		SCRIPT_ERR_CONTROL_CHAR,
		SCRIPT_ERR_BAD_OFFSET,        // not 1..9 decimal digits
		SCRIPT_ERR_OFFSET_RANGE,      // digits ok but > kScriptMaxOffsetMs
		SCRIPT_ERR_OFFSET_ORDER,      // smaller than the previous step's offset
		SCRIPT_ERR_MISSING_COMMAND,   // offset only
		SCRIPT_ERR_BAD_VERB,          // verb not in the allowed list
		SCRIPT_ERR_TOO_MANY_STEPS     // the (kScriptMaxSteps + 1)th step
	};

	struct ScriptStep
	{
		uint32_t    offsetMs;
		std::string command;          // trimmed "<verb> [args]", inner spacing kept
		uint32_t    line;             // 1-based physical line the step came from
	};

	struct ScriptParseResult
	{
		ScriptError             error;      // SCRIPT_OK on success
		uint32_t                errorLine;  // 1-based; 0 for file-level errors
		std::vector<ScriptStep> steps;      // empty on any error
	};

	// Trims spaces and tabs from both ends (a trailing '\r' is removed before this is called).
	inline std::string ScriptTrim(const std::string & text)
	{
		size_t first = text.find_first_not_of(" \t");
		if (first == std::string::npos)
			return std::string();

		size_t last = text.find_last_not_of(" \t");
		return text.substr(first, last - first + 1);
	}

	// Case-insensitive match against the 21 verbs a script may run (plan section 5.2; "goto" added by F5-72).
	inline bool IsScriptVerb(const std::string & verb)
	{
		static const char * const kVerbs[] =
		{
			"move", "goto", "stop", "attack", "cast", "pot", "sit", "stand", "target", "regene",
			"pinvite", "paccept", "pdecline", "pleave", "ppromote", "pkick", "pchat",
			"see", "npcs", "snap", "list"
		};

		std::string lower;
		lower.reserve(verb.size());
		for (size_t i = 0; i < verb.size(); ++i)
		{
			char c = verb[i];
			if (c >= 'A' && c <= 'Z')
				c = (char)(c + 32);
			lower.push_back(c);
		}

		for (size_t i = 0; i < sizeof(kVerbs) / sizeof(kVerbs[0]); ++i)
		{
			if (lower == kVerbs[i])
				return true;
		}

		return false;
	}

	// Short English text for log lines. Never null.
	inline const char * ScriptErrorText(ScriptError error)
	{
		switch (error)
		{
		case SCRIPT_OK: return "ok";
		case SCRIPT_ERR_TOO_LARGE: return "file too large";
		case SCRIPT_ERR_TOO_MANY_LINES: return "too many lines";
		case SCRIPT_ERR_EMPTY: return "no steps";
		case SCRIPT_ERR_LINE_TOO_LONG: return "line too long";
		case SCRIPT_ERR_CONTROL_CHAR: return "control character";
		case SCRIPT_ERR_BAD_OFFSET: return "bad offset";
		case SCRIPT_ERR_OFFSET_RANGE: return "offset out of range";
		case SCRIPT_ERR_OFFSET_ORDER: return "offsets must not decrease";
		case SCRIPT_ERR_MISSING_COMMAND: return "missing command";
		case SCRIPT_ERR_BAD_VERB: return "bad verb";
		case SCRIPT_ERR_TOO_MANY_STEPS: return "too many steps";
		default: return "?";
		}
	}

	// Parses a whole script text. No I/O, no global state; first error wins and steps stay empty.
	inline ScriptParseResult ParseScript(const std::string & text)
	{
		ScriptParseResult result;
		result.error = SCRIPT_OK;
		result.errorLine = 0;

		if (text.size() > kScriptMaxBytes)
		{
			result.error = SCRIPT_ERR_TOO_LARGE;
			return result;
		}

		// Split on '\n'; a trailing '\n' does not add an empty final line.
		std::vector<std::string> lines;
		size_t start = 0;
		while (start <= text.size())
		{
			size_t nl = text.find('\n', start);
			if (nl == std::string::npos)
			{
				if (start < text.size())
					lines.push_back(text.substr(start));
				break;
			}

			lines.push_back(text.substr(start, nl - start));
			start = nl + 1;
		}

		if (lines.size() > kScriptMaxLines)
		{
			result.error = SCRIPT_ERR_TOO_MANY_LINES;
			return result;
		}

		std::vector<ScriptStep> steps;
		uint32_t lastOffset = 0;

		for (size_t i = 0; i < lines.size(); ++i)
		{
			std::string line = lines[i];
			if (!line.empty() && line[line.size() - 1] == '\r')
				line.erase(line.size() - 1);

			const uint32_t lineNo = (uint32_t)(i + 1);

			if (line.size() > kScriptMaxLineLen)
			{
				result.error = SCRIPT_ERR_LINE_TOO_LONG;
				result.errorLine = lineNo;
				return result;
			}

			for (size_t k = 0; k < line.size(); ++k)
			{
				unsigned char c = (unsigned char)line[k];
				if (c == '\t')
					continue;
				if (c < 0x20 || c == 0x7F)
				{
					result.error = SCRIPT_ERR_CONTROL_CHAR;
					result.errorLine = lineNo;
					return result;
				}
			}

			std::string trimmed = ScriptTrim(line);
			if (trimmed.empty() || trimmed[0] == '#')
				continue;

			size_t sep = trimmed.find_first_of(" \t");
			std::string token = (sep == std::string::npos) ? trimmed : trimmed.substr(0, sep);
			std::string rest = (sep == std::string::npos) ? std::string() : ScriptTrim(trimmed.substr(sep + 1));

			bool digitsOk = !token.empty() && token.size() <= 9;
			for (size_t k = 0; digitsOk && k < token.size(); ++k)
			{
				if (token[k] < '0' || token[k] > '9')
					digitsOk = false;
			}

			if (!digitsOk)
			{
				result.error = SCRIPT_ERR_BAD_OFFSET;
				result.errorLine = lineNo;
				return result;
			}

			uint64_t value = 0;
			for (size_t k = 0; k < token.size(); ++k)
				value = value * 10 + (uint64_t)(token[k] - '0');

			if (value > kScriptMaxOffsetMs)
			{
				result.error = SCRIPT_ERR_OFFSET_RANGE;
				result.errorLine = lineNo;
				return result;
			}

			if (value < lastOffset)
			{
				result.error = SCRIPT_ERR_OFFSET_ORDER;
				result.errorLine = lineNo;
				return result;
			}

			if (rest.empty())
			{
				result.error = SCRIPT_ERR_MISSING_COMMAND;
				result.errorLine = lineNo;
				return result;
			}

			size_t vsep = rest.find_first_of(" \t");
			std::string verb = (vsep == std::string::npos) ? rest : rest.substr(0, vsep);
			if (!IsScriptVerb(verb))
			{
				result.error = SCRIPT_ERR_BAD_VERB;
				result.errorLine = lineNo;
				return result;
			}

			if (steps.size() >= kScriptMaxSteps)
			{
				result.error = SCRIPT_ERR_TOO_MANY_STEPS;
				result.errorLine = lineNo;
				return result;
			}

			ScriptStep step;
			step.offsetMs = (uint32_t)value;
			step.command = rest;
			step.line = lineNo;
			steps.push_back(step);
			lastOffset = (uint32_t)value;
		}

		if (steps.empty())
		{
			result.error = SCRIPT_ERR_EMPTY;
			return result;
		}

		result.steps = steps;
		return result;
	}
}
