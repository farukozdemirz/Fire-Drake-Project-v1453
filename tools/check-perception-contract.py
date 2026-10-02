#!/usr/bin/env python3
"""Perception contract static audit (AC-LRN-03 / AC-ARCH-06, static part).

Scans the bot sources (GameServer/Bot, BotCore) and checks five rules:

  R1  no access to the server's NPC / region / party / session registries
  R2  restricted symbols only in the named, size-capped allowlist
  R3  cross-session CUser reads only in the known test drivers
  R4  BotCore includes only standard and sibling headers
  R5  the view structs carry no forbidden field (the player name in UnitView is allowed from F4-50;
      the target HP in UnitView/NpcView is allowed from F4-51 because it arrives with WIZ_TARGET_HP,
      docs/03 section 16 [D]; the NPC name is never allowed)

Exit codes: 0 PASS, 1 at least one violation, 2 usage / input error.
Only the standard library is used. The scanner is line based (comment and
string stripping plus a column-0 function header heuristic); it never
writes to the scanned tree.
"""

import json
import os
import re
import shutil
import sys
import tempfile

SCAN_DIRS = ("GameServer/Bot", "BotCore")
SCAN_EXT = (".cpp", ".h")
VIEW_FILE = "BotCore/Perception.h"
R1_SYMBOLS = ("GetNpcPtr", "m_arNpcArray", "m_RegionUserArray", "m_RegionNpcArray", "GetRegion",
              "FindNpcInZone", "CNpc", "m_PartyArray", "GetPartyPtr")
R2_SYMBOLS = ("GetUserPtr", "GetMap", "GetItem", "m_buffMap", "isInParty", "GetPartyID",
              "m_CoolDownList", "m_sItemArray", "GetActiveSessionMap", "GetIdleSessionMap")
R3_RE = re.compile(r"\b(?!s\b)\w+(?:->|\.)m_pUser\b")
FUNC_RE = re.compile(r"^(?:[A-Za-z_~][\w\s\*&:<>,~]*?\s)?((?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*)\s*\(")
NOT_FUNC = ("if", "for", "while", "switch", "return", "else", "case", "do", "catch", "sizeof")
INC_RE = re.compile(r"^\s*#\s*include\s*([<\"])([^>\"]+)[>\"]")
FIELD_RE = re.compile(r"(\w+)\s*(?:\[[^\]]*\])?\s*(?:=[^;,]*)?\s*[;,]")
WORD_RE = re.compile(r"[A-Z]+(?![a-z])|[A-Z]?[a-z]+|\d+")

RULES = (
    ("R1", "forbidden registry symbols"),
    ("R2", "restricted symbols (own state, static data, pool)"),
    ("R3", "cross-session CUser reads (test drivers)"),
    ("R4", "BotCore purity (includes)"),
    ("R5", "view struct fields"),
)

ACT = "GameServer/Bot/ActionExecutor.cpp"
BOT = "GameServer/Bot/BotManager.cpp"
OWN_PARTY = "own party flag, guard input"
NAME_REG = "name still registered? null compare only, no data read"
SELF = "own state (SelfState), read in the IOCP thread"
POOL = "pool self-test: membership of the reserved slot ids only (F2-01)"
ALLOW_R2 = [
    (ACT, "ActionExecutor::BeginMove", "GetMap", 2, "own map: IsValidPosition check of the next step (static terrain)"),
    (ACT, "CountInBag", "GetItem", 1, "own bag stock for CLI-06"),
    (ACT, "ActionExecutor::RequestPartyInvite", "isInParty", 1, OWN_PARTY),
    (ACT, "ActionExecutor::RequestPartyLeave", "isInParty", 1, OWN_PARTY),
    (ACT, "RequestPartyManage", "isInParty", 1, OWN_PARTY),
    (ACT, "ActionExecutor::RequestChatParty", "isInParty", 1, OWN_PARTY),
    (BOT, "BotManager::PollDespawn", "GetUserPtr", 2, NAME_REG),
    (BOT, "BotManager::StartSession", "GetUserPtr", 2, NAME_REG),
    (BOT, "FillSelfExtras", "GetItem", 1, SELF),
    (BOT, "FillSelfExtras", "isInParty", 1, SELF),
    (BOT, "FillSelfExtras", "m_buffMap", 1, SELF),
    (BOT, "IsSamePartyMember", "isInParty", 2, "guard input from two bot sessions (ADR-0017 Eki F4-08/F4-10)"),
    (BOT, "IsSamePartyMember", "GetPartyID", 2, "guard input from two bot sessions (ADR-0017 Eki F4-08/F4-10)"),
    (BOT, "BotManager::Startup", "GetActiveSessionMap", 6, POOL),
    (BOT, "BotManager::Startup", "GetIdleSessionMap", 4, POOL),
]
TEST_DRIVER = "test driver: reads another bot's id/position from its session (ADR-0017 Ek F4-02 item 4); replaced when Perception feeds targets"
ALLOW_R3 = [
    (BOT, "BotManager::CommandPartyInvite", "m_pUser", 3, TEST_DRIVER),
    (BOT, "BotManager::CommandPartyManage", "m_pUser", 1, TEST_DRIVER),
    (BOT, "BotManager::CommandTarget", "m_pUser", 3, TEST_DRIVER),
    (BOT, "BotManager::TickSessions", "m_pUser", 9, TEST_DRIVER),
    (BOT, "IsSamePartyMember", "m_pUser", 2, TEST_DRIVER),
]
VIEW_FORBIDDEN = {   # struct name -> forbidden words (lower case) in field names
    # The player name arrives with WIZ_USER_INOUT (docs/03 section 16) and is allowed from F4-50 on.
    # The target HP arrives with WIZ_TARGET_HP (docs/03 section 16 [D]) and is allowed from F4-51 on;
    # mp/cooldown/inventory data is never sent and stays forbidden. The NPC name is never a decision input.
    "UnitView": ("mp", "cooldown", "stock", "inventory", "invent", "buff", "skill", "item", "potion"),
    "NpcView": ("mp", "name", "cooldown", "stock", "inventory", "invent", "buff", "skill", "item", "potion"),
    "TeamMemberView": ("cooldown", "stock", "inventory", "invent", "buff", "skill", "item", "potion"),
}


def strip_code(line, in_block):
    out = []
    i = 0
    n = len(line)
    while i < n:
        c = line[i]
        if in_block:
            j = line.find("*/", i)
            if j < 0:
                return "".join(out), True
            i = j + 2
            in_block = False
            continue
        two = line[i:i + 2]
        if two == "//":
            break
        if two == "/*":
            in_block = True
            i += 2
            continue
        if c == '"' or c == "'":
            i += 1
            while i < n and line[i] != c:
                i += 2 if line[i] == "\\" else 1
            i += 1
            out.append(" ")
            continue
        out.append(c)
        i += 1
    return "".join(out), in_block


def read_lines(path):
    with open(path, "rb") as f:
        text = f.read().decode("latin-1")
    return [line[:-1] if line.endswith("\r") else line for line in text.split("\n")]


def scan_file(path, rel):
    lines = read_lines(path)
    hits = []
    cur = "(file)"
    in_block = False
    for idx, line in enumerate(lines, 1):
        code, in_block = strip_code(line, in_block)
        if code and not code[0].isspace():
            c0 = code[0]
            if (c0.isalpha() or c0 == "_" or c0 == "~") and not code.rstrip().endswith(";"):
                m = FUNC_RE.match(code)
                if m and m.group(1) not in NOT_FUNC:
                    cur = m.group(1)
        for sym in R1_SYMBOLS:
            for _ in re.finditer(r"\b" + re.escape(sym) + r"\b", code):
                hits.append((idx, "R1", sym, cur))
        for sym in R2_SYMBOLS:
            for _ in re.finditer(r"\b" + re.escape(sym) + r"\b", code):
                hits.append((idx, "R2", sym, cur))
        for _ in R3_RE.finditer(code):
            hits.append((idx, "R3", "m_pUser", cur))
    return hits


def collect_files(root):
    files = []
    for d in SCAN_DIRS:
        full = os.path.join(root, d)
        if not os.path.isdir(full):
            continue
        for name in sorted(os.listdir(full)):
            if name.endswith(SCAN_EXT):
                files.append((os.path.join(full, name), d + "/" + name))
    return files


def struct_fields(code_lines, name):
    pat = re.compile(r"^\s*struct\s+" + re.escape(name) + r"\b(?!\s*;)")
    start = -1
    for i, code in enumerate(code_lines):
        if pat.match(code):
            start = i
            break
    if start < 0:
        return None

    depth = 0
    entered = False
    fields = []
    for i in range(start, len(code_lines)):
        code = code_lines[i]
        for ch in code:
            if ch == "{":
                depth += 1
                entered = True
            elif ch == "}":
                depth -= 1
        if entered and depth == 1:
            for m in FIELD_RE.finditer(code):
                fields.append((i + 1, m.group(1)))
        if entered and depth <= 0:
            break
    return fields


def audit(root, allow2, allow3):
    files = collect_files(root)
    if not files:
        sys.stderr.write("error: no scan directories under %s\n" % root)
        sys.exit(2)

    allow_map = {}
    for rule, table in (("R2", allow2), ("R3", allow3)):
        for (fname, func, sym, mx, reason) in table:
            allow_map[(rule, fname, func, sym)] = (mx, reason)

    violations = []
    allowlisted = []
    stale = []
    rule_v = {r: 0 for r, _ in RULES}
    rule_a = {r: 0 for r, _ in RULES}

    all_hits = []
    for full, rel in files:
        for (ln, rule, sym, func) in scan_file(full, rel):
            all_hits.append((rel, ln, rule, sym, func))

    actual = {}
    for (rel, ln, rule, sym, func) in all_hits:
        if rule == "R1":
            violations.append({
                "file": rel, "line": ln, "rule": "R1", "symbol": sym,
                "function": func, "why": "forbidden",
                "detail": "R1 %s in %s (forbidden)" % (sym, func),
            })
        elif rule in ("R2", "R3"):
            actual.setdefault((rule, rel, func, sym), []).append(ln)

    for key in sorted(actual):
        rule, rel, func, sym = key
        lns = sorted(actual[key])
        entry = allow_map.get(key)
        if entry is None:
            for ln in lns:
                violations.append({
                    "file": rel, "line": ln, "rule": rule, "symbol": sym,
                    "function": func, "why": "not allowlisted",
                    "detail": "%s %s in %s (not allowlisted)" % (rule, sym, func),
                })
            continue
        mx, _reason = entry
        for ln in lns[mx:]:
            violations.append({
                "file": rel, "line": ln, "rule": rule, "symbol": sym,
                "function": func, "why": "exceeds allowlist",
                "detail": "%s %s in %s (exceeds allowlist (%d > %d))" % (rule, sym, func, len(lns), mx),
            })

    for key in sorted(allow_map):
        rule, fname, func, sym = key
        mx, reason = allow_map[key]
        lns = sorted(actual.get(key, []))
        allowed = lns[:mx]
        if allowed:
            rule_a[rule] += len(allowed)
            allowlisted.append({
                "rule": rule, "file": fname, "function": func, "symbol": sym,
                "hits": len(allowed), "max": mx, "reason": reason,
            })
        if len(lns) < mx:
            stale.append({
                "rule": rule, "file": fname, "function": func, "symbol": sym,
                "hits": len(lns), "max": mx,
            })

    for full, rel in files:
        if not rel.startswith("BotCore/"):
            continue
        for idx, line in enumerate(read_lines(full), 1):
            m = INC_RE.match(line)
            if not m:
                continue
            kind, name = m.group(1), m.group(2)
            bad = False
            if kind == "<":
                if not re.match(r"^[a-z_0-9]+$", name):
                    bad = True
            else:
                if not re.match(r"^[A-Za-z0-9_]+\.h$", name) or name.lower() == "stdafx.h":
                    bad = True
            if bad:
                close = ">" if kind == "<" else '"'
                detail = "R4 include %s%s%s" % (kind, name, close)
                violations.append({
                    "file": rel, "line": idx, "rule": "R4", "symbol": "include %s%s%s" % (kind, name, close),
                    "function": "(file)", "why": "forbidden include", "detail": detail,
                })

    view_full = os.path.join(root, VIEW_FILE)
    if not os.path.isfile(view_full):
        violations.append({
            "file": VIEW_FILE, "line": 0, "rule": "R5", "symbol": "file",
            "function": "", "why": "file not found", "detail": "R5 file not found",
        })
    else:
        raw_lines = read_lines(view_full)
        code_lines = []
        in_block = False
        for line in raw_lines:
            code, in_block = strip_code(line, in_block)
            code_lines.append(code)
        for sname, forbidden in VIEW_FORBIDDEN.items():
            fields = struct_fields(code_lines, sname)
            if fields is None:
                violations.append({
                    "file": VIEW_FILE, "line": 0, "rule": "R5", "symbol": sname,
                    "function": "", "why": "struct not found",
                    "detail": "R5 %s struct not found" % sname,
                })
                continue
            for (ln, fname) in fields:
                words = [w.lower() for w in WORD_RE.findall(fname)]
                bad = sorted(set(w for w in words if w in forbidden))
                if bad:
                    violations.append({
                        "file": VIEW_FILE, "line": ln, "rule": "R5", "symbol": "%s.%s" % (sname, fname),
                        "function": "", "why": "forbidden field",
                        "detail": "R5 %s.%s (forbidden word: %s)" % (sname, fname, ", ".join(bad)),
                    })

    violations.sort(key=lambda v: (v["file"], v["line"], v["rule"], v["symbol"]))
    allowlisted.sort(key=lambda a: (a["rule"], a["file"], a["function"], a["symbol"]))
    stale.sort(key=lambda s: (s["rule"], s["file"], s["function"], s["symbol"]))
    for v in violations:
        rule_v[v["rule"]] += 1

    return {
        "files": len(files),
        "rules": {r: {"allowlisted": rule_a[r], "violations": rule_v[r]} for r, _ in RULES},
        "violations": violations,
        "allowlisted": allowlisted,
        "stale": stale,
        "result": "FAIL" if violations else "PASS",
    }


def render_text(res):
    out = []
    out.append("Perception contract audit (AC-LRN-03 / AC-ARCH-06, static part)")
    out.append("files scanned: %d" % res["files"])
    out.append("")
    out.append("| rule | description | violations | allowlisted |")
    out.append("|---|---|---|---|")
    for rule, desc in RULES:
        out.append("| %s | %s | %d | %d |" % (rule, desc, res["rules"][rule]["violations"], res["rules"][rule]["allowlisted"]))
    out.append("")
    out.append("RESULT: %s" % res["result"])
    out.append("")
    out.append("## Allowlisted")
    out.append("| rule | file | function | token | hits | max | reason |")
    out.append("|---|---|---|---|---|---|---|")
    for a in res["allowlisted"]:
        out.append("| %s | %s | %s | %s | %d | %d | %s |" % (
            a["rule"], a["file"], a["function"], a["symbol"], a["hits"], a["max"], a["reason"]))
    if res["violations"]:
        out.append("")
        out.append("## Violations")
        for v in res["violations"]:
            out.append("%s:%d: %s" % (v["file"], v["line"], v["detail"]))
    if res["stale"]:
        out.append("")
        out.append("## Stale allowlist entries")
        for s in res["stale"]:
            out.append("%s %s %s hits %d < max %d" % (s["file"], s["function"], s["symbol"], s["hits"], s["max"]))
    out.append("")
    out.append("Not covered: runtime assert (AC-LRN-03 second half) and the semantic check of observation fields (docs/03 sec. 16).")
    return "\n".join(out) + "\n"


def render_json(res):
    return json.dumps(res, sort_keys=True, indent=2) + "\n"


def usage_error():
    sys.stderr.write("usage: check-perception-contract.py [--root DIR] [--json] [--selftest]\n")
    sys.exit(2)


def parse_args(argv):
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    as_json = False
    run_self = False
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == "--json":
            as_json = True
        elif arg == "--selftest":
            run_self = True
        elif arg == "--root":
            i += 1
            if i >= len(argv):
                usage_error()
            root = argv[i]
        elif arg.startswith("--root="):
            root = arg[len("--root="):]
        else:
            usage_error()
        i += 1
    return root, as_json, run_self


def write_tree(base, files):
    for rel, content in files.items():
        full = os.path.join(base, rel)
        d = os.path.dirname(full)
        if not os.path.isdir(d):
            os.makedirs(d)
        with open(full, "wb") as f:
            f.write(content.replace("\n", "\r\n").encode("latin-1"))


def reset_tree(base):
    for d in SCAN_DIRS:
        full = os.path.join(base, d)
        if os.path.isdir(full):
            shutil.rmtree(full)


def clean_perception():
    return "\n".join([
        "\tstruct UnitView",
        "\t{",
        "\t\tuint16_t id;",
        "\t\tfloat x, z;",
        "\t};",
        "\tstruct NpcView",
        "\t{",
        "\t\tuint16_t id;",
        "\t};",
        "\tstruct TeamMemberView",
        "\t{",
        "\t\tint32_t hp, maxHp;",
        "\t\tchar name[24];",
        "\t};",
    ])


def selftest_cases(base):
    # V1: clean tree.
    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/Perception.h": clean_perception(),
        "BotCore/X.h": "#include <cstdint>\n#include \"Rng.h\"",
    })
    res = audit(base, [], [])
    assert res["files"] == 3, res["files"]
    assert not res["violations"], res["violations"]
    assert not res["allowlisted"], res["allowlisted"]
    assert res["result"] == "PASS", res["result"]

    # V2: R1, comments and strings are ignored.
    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\tg_pMain->GetNpcPtr(1);", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, [], [])
    assert len(res["violations"]) == 1, res["violations"]
    assert res["violations"][0]["rule"] == "R1", res["violations"][0]
    assert res["violations"][0]["function"] == "Foo::Bar", res["violations"][0]
    assert res["violations"][0]["line"] == 3, res["violations"][0]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join([
            "void Foo::Bar(int a)",
            "{",
            "\t// GetNpcPtr(1)",
            "\tconst char * t = \"CNpc\";",
            "\t/* m_PartyArray",
            "\tGetPartyPtr */",
            "}",
        ]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, [], [])
    assert not res["violations"], res["violations"]

    # V3: R2 allowlist logic.
    allow2 = [("GameServer/Bot/T.cpp", "Foo::Bar", "GetUserPtr", 1, "x")]
    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\tGetUserPtr(1);", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, allow2, [])
    assert not res["violations"], res["violations"]
    assert res["rules"]["R2"]["allowlisted"] == 1, res["rules"]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\tGetUserPtr(1);", "\tGetUserPtr(2);", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, allow2, [])
    assert len(res["violations"]) == 1, res["violations"]
    assert res["violations"][0]["why"] == "exceeds allowlist", res["violations"][0]
    assert res["rules"]["R2"]["allowlisted"] == 1, res["rules"]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Baz(int a)", "{", "\tGetUserPtr(1);", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, allow2, [])
    assert len(res["violations"]) == 1, res["violations"]
    assert res["violations"][0]["why"] == "not allowlisted", res["violations"][0]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, allow2, [])
    assert not res["violations"], res["violations"]
    assert len(res["stale"]) == 1, res["stale"]
    assert res["result"] == "PASS", res["result"]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["static int Helper(CUser * u)", "{", "\tGetUserPtr(1);", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, [("GameServer/Bot/T.cpp", "Helper", "GetUserPtr", 1, "x")], [])
    assert not res["violations"], res["violations"]
    assert res["rules"]["R2"]["allowlisted"] == 1, res["rules"]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["static int Helper(CUser * u,", "\tint v)", "{", "\tGetUserPtr(1);", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, [("GameServer/Bot/T.cpp", "Helper", "GetUserPtr", 1, "x")], [])
    assert not res["violations"], res["violations"]

    # V4: R3.
    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\ts->m_pUser->GetX();", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, [], [])
    assert not res["violations"], res["violations"]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\tt->m_pUser->GetX();", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, [], [])
    assert len(res["violations"]) == 1, res["violations"]
    assert res["violations"][0]["rule"] == "R3", res["violations"][0]

    allow3 = [("GameServer/Bot/T.cpp", "Foo::Bar", "m_pUser", 1, "x")]
    res = audit(base, [], allow3)
    assert not res["violations"], res["violations"]
    assert res["rules"]["R3"]["allowlisted"] == 1, res["rules"]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\tt->m_pUser->GetX();", "\tt->m_pUser->GetY();", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, [], allow3)
    assert len(res["violations"]) == 1, res["violations"]

    # V5: R4.
    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/Perception.h": clean_perception(),
        "BotCore/X.h": "\n".join([
            "#include <windows.h>",
            "#include \"stdafx.h\"",
            "#include \"../GameServer/Bot/Foo.h\"",
            "#include <stdint.h>",
            "// #include <windows.h>",
        ]),
    })
    res = audit(base, [], [])
    assert len(res["violations"]) == 4, res["violations"]
    assert all(v["rule"] == "R4" for v in res["violations"]), res["violations"]

    # V6: R5.
    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/Perception.h": clean_perception().replace("\t\tfloat x, z;", "\t\tfloat x, z;\n\t\tint32_t mp;"),
    })
    res = audit(base, [], [])
    assert len(res["violations"]) == 1, res["violations"]
    assert res["violations"][0]["rule"] == "R5", res["violations"][0]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/Perception.h": clean_perception().replace("\t\tfloat x, z;", "\t\tfloat x, z;\n\t\tchar name[24];"),
    })
    res = audit(base, [], [])
    assert not res["violations"], res["violations"]   # player name is allowed in UnitView (F4-50)

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/Perception.h": clean_perception().replace("\t\tfloat x, z;", "\t\tfloat x, z;\n\t\tint32_t hp, maxHp;"),
    })
    res = audit(base, [], [])
    assert not res["violations"], res["violations"]   # target HP is allowed in UnitView (F4-51)

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/Perception.h": clean_perception().replace("\t\tuint16_t id;\n\t};", "\t\tuint16_t id;\n\t\tint32_t hp, maxHp;\n\t};"),
    })
    res = audit(base, [], [])
    assert not res["violations"], res["violations"]   # target HP is allowed in NpcView (F4-51)

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/Perception.h": clean_perception().replace("\t\tuint16_t id;\n\t};", "\t\tuint16_t id;\n\t\tchar name[24];\n\t};"),
    })
    res = audit(base, [], [])
    assert len(res["violations"]) == 1, res["violations"]
    assert res["violations"][0]["rule"] == "R5", res["violations"][0]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/Perception.h": clean_perception().replace("\t\tint32_t hp, maxHp;", "\t\tint32_t hp, maxHp;\n\t\tuint8_t buffCount;"),
    })
    res = audit(base, [], [])
    assert len(res["violations"]) == 1, res["violations"]
    assert res["violations"][0]["rule"] == "R5", res["violations"][0]

    reset_tree(base)
    no_npc = "\n".join([
        "\tstruct UnitView",
        "\t{",
        "\t\tuint16_t id;",
        "\t};",
        "\tstruct TeamMemberView",
        "\t{",
        "\t\tint32_t hp, maxHp;",
        "\t\tchar name[24];",
        "\t};",
    ])
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/Perception.h": no_npc,
    })
    res = audit(base, [], [])
    assert len(res["violations"]) == 1, res["violations"]
    assert "struct not found" in res["violations"][0]["detail"], res["violations"][0]

    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\treturn;", "}"]),
        "BotCore/X.h": "// no perception header\n",
    })
    res = audit(base, [], [])
    assert any(v["rule"] == "R5" and v["why"] == "file not found" for v in res["violations"]), res["violations"]

    # V7: text and JSON output determinism.
    reset_tree(base)
    write_tree(base, {
        "GameServer/Bot/T.cpp": "\n".join(["void Foo::Bar(int a)", "{", "\tg_pMain->GetNpcPtr(1);", "}"]),
        "BotCore/Perception.h": clean_perception(),
    })
    res = audit(base, [], [])
    j1 = render_json(res)
    j2 = render_json(res)
    assert j1 == j2, "json output not deterministic"
    loaded = json.loads(j1)
    assert loaded["result"] == "FAIL", loaded["result"]
    assert isinstance(loaded["violations"], list), loaded["violations"]
    text = render_text(res)
    assert "RESULT: FAIL" in text, text
    assert "GameServer/Bot/T.cpp:3: R1 GetNpcPtr in Foo::Bar" in text, text


def run_selftest():
    base = tempfile.mkdtemp(prefix="f4-23-selftest-")
    try:
        selftest_cases(base)
    finally:
        shutil.rmtree(base, ignore_errors=True)
    sys.stdout.write("selftest OK\n")
    return 0


def main(argv):
    root, as_json, run_self = parse_args(argv)
    if run_self:
        return run_selftest()
    res = audit(root, ALLOW_R2, ALLOW_R3)
    if as_json:
        sys.stdout.write(render_json(res))
    else:
        sys.stdout.write(render_text(res))
    return 1 if res["violations"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
