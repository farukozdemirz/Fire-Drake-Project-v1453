#!/usr/bin/env python3
"""Generates a bot test script (Scripts/<name>.txt) from a spec and MAGIC.

The spec is a line based text file; three line kinds are understood (blank
lines and '#' comments are free):

    raw  <offset_ms> <command and args>     # passed through at a fixed offset
    cast <bot> <skill_id> <target bot|self> <cycles 1..20>
    pot  <bot> <item_id> <count 1..20>

raw steps describe the preparation phase (for example building a party); the
bot timelines start at t0 = (largest raw offset) + start_gap_ms (0 when there
is no raw step).  Each bot keeps its own cursor and its cast/pot steps are laid
down back to back; different bots run in parallel (independent cursors).

For a cast step at cursor c the generator advances the cursor by
    cycles * max(recast_ms, cast_ms + 140, 1000) + margin_ms
where cast_ms = CastTime == 0 ? 0 : CastTime*100 + 80 and
recast_ms = ReCastTime*100 (BotCore/BotCombat.h: kCastExtraMs, kCastGapMs,
kTypeGateMs).  A pot step advances the cursor by count * 2500 + margin_ms
(kPotCooldownMs).  The generated text ends with a 'list' step 500 ms after the
last bot finishes.

The output is deterministic and must satisfy the ScriptPlan.h limits
(BotCore/ScriptPlan.h): at most 100 steps, 8192 bytes, 128 physical lines and
255 bytes per line, offsets at most 600000 ms.  A limit overflow exits 1 (and
no file is written); a usage, input or spec error exits 2.

Only MAGIC is read (no other table); the tool never talks to the server and it
writes only the generated file.  With no --magic it runs sqlcmd (Windows) and
decodes its stdout as bytes, because the real MAGIC contains non-UTF-8 bytes.

Usage:
    python3 tools/skill-script-gen.py SPEC [--out FILE]
        [--magic FILE | --sqlcmd P --server S --db D]
        [--margin-ms N] [--start-gap-ms N]
    python3 tools/skill-script-gen.py --check SCRIPT
    python3 tools/skill-script-gen.py --selftest
"""

import argparse
import contextlib
import io
import os
import subprocess
import sys
import tempfile

DEFAULT_SQLCMD = "/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE"
DEFAULT_SERVER = ".\\SQLEXPRESS"
DEFAULT_DB = "FDP_kn_online"

MAGIC_QUERY = ("SELECT MagicNum, RTRIM(EnName), Msp, CastTime, ReCastTime, "
               "Range, Type1, Type2 FROM MAGIC")

# BotCore/ScriptPlan.h
MAX_BYTES = 8192
MAX_LINES = 128
MAX_STEPS = 100
MAX_LINE_LEN = 255
MAX_OFFSET_MS = 600000

# BotCore/BotCombat.h timing constants
CAST_EXTRA_MS = 80
CAST_GAP_MS = 140
TYPE_GATE_MS = 1000
POT_COOLDOWN_MS = 2500

DEFAULT_MARGIN_MS = 1500
DEFAULT_START_GAP_MS = 1500
TRAILING_GAP_MS = 500

# The 20 verbs a script may run (BotCore/ScriptPlan.h: IsScriptVerb).
VERBS = (
    "move", "stop", "attack", "cast", "pot", "sit", "stand", "target", "regene",
    "pinvite", "paccept", "pdecline", "pleave", "ppromote", "pkick", "pchat",
    "see", "npcs", "snap", "list",
)
VERB_SET = frozenset(VERBS)

USAGE = (
    "Usage:\n"
    "  python3 tools/skill-script-gen.py SPEC [--out FILE]\n"
    "      [--magic FILE | --sqlcmd P --server S --db D]\n"
    "      [--margin-ms N] [--start-gap-ms N]\n"
    "  python3 tools/skill-script-gen.py --check SCRIPT\n"
    "  python3 tools/skill-script-gen.py --selftest\n"
    "Options:\n"
    "  --out FILE        write the generated script to FILE (UTF-8, LF)\n"
    "  --magic FILE      MAGIC rows (MagicNum|EnName|Msp|CastTime|ReCastTime|Range|Type1|Type2)\n"
    "  --margin-ms N     extra wait after every step (default 1500)\n"
    "  --start-gap-ms N  delay after the last raw step (default 1500)\n"
)


class SpecError(Exception):
    """Exit code 2: usage, input or spec error."""


class LimitError(Exception):
    """Exit code 1: a ScriptPlan.h limit would be exceeded."""


def as_int(value, default=None):
    """Converts a value to int, returning default when it is not numeric."""
    if value is None:
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def parse_magic_lines(lines):
    """Parses sqlcmd-style MAGIC rows into {MagicNum: {...}}."""
    magic = {}
    for line in lines:
        text = line.strip()
        if not text:
            continue
        parts = [part.strip() for part in text.split("|")]
        if len(parts) < 8:
            raise SpecError("bad MAGIC row: %s" % text)
        num = as_int(parts[0])
        if num is None:
            raise SpecError("bad MAGIC MagicNum: %s" % text)
        magic[num] = {
            "name": parts[1],
            "msp": as_int(parts[2]),
            "cast_time": as_int(parts[3]),
            "recast_time": as_int(parts[4]),
            "range": as_int(parts[5]),
            "type1": as_int(parts[6]),
            "type2": as_int(parts[7]),
        }
    return magic


def load_magic_file(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return parse_magic_lines(handle.readlines())
    except OSError as exc:
        raise SpecError("cannot read MAGIC file: %s" % exc)


def load_magic_sql(sqlcmd, server, db):
    command = [
        sqlcmd, "-S", server, "-E", "-d", db,
        "-W", "-s", "|", "-h", "-1", "-b",
        "-Q", "SET NOCOUNT ON; " + MAGIC_QUERY,
    ]
    try:
        result = subprocess.run(command, capture_output=True)
    except OSError as exc:
        raise SpecError("cannot run sqlcmd: %s" % exc)
    stdout = result.stdout.decode("utf-8", errors="replace")
    stderr = result.stderr.decode("utf-8", errors="replace")
    if result.returncode != 0:
        raise SpecError("sqlcmd failed (exit %d):\n%s"
                        % (result.returncode, stderr.strip()))
    return parse_magic_lines(stdout.replace("\r", "").split("\n"))


def cast_period_ms(row):
    """Milliseconds between two casts of the same skill, including the recast."""
    cast_time = row.get("cast_time") or 0
    recast_time = row.get("recast_time") or 0
    cast_ms = 0 if cast_time == 0 else cast_time * 100 + CAST_EXTRA_MS
    recast_ms = recast_time * 100
    return max(recast_ms, cast_ms + CAST_GAP_MS, TYPE_GATE_MS)


def parse_spec_int(token, line_no, what, low, high):
    """Parses an int within [low, high]; raises SpecError with the line number."""
    try:
        value = int(token, 10)
    except (TypeError, ValueError):
        raise SpecError("line %d: bad %s '%s'" % (line_no, what, token))
    if value < low or value > high:
        raise SpecError("line %d: %s %d out of range %d..%d"
                        % (line_no, what, value, low, high))
    return value


def parse_offset(token, line_no):
    """Parses a non-negative raw offset (the upper bound is checked later)."""
    if not token or not all(ch in "0123456789" for ch in token):
        raise SpecError("line %d: bad offset '%s'" % (line_no, token))
    return int(token, 10)


def generate(spec_text, magic, spec_name, margin_ms=DEFAULT_MARGIN_MS,
             start_gap_ms=DEFAULT_START_GAP_MS):
    """Builds the script text; raises SpecError (2) or LimitError (1)."""
    if margin_ms < 0:
        raise SpecError("margin_ms must not be negative")
    if start_gap_ms < 0:
        raise SpecError("start_gap_ms must not be negative")

    raw_steps = []       # (offset, seq, command)
    raw_max = None
    actions = []         # (line_no, kind, bot, fields)
    seq = 0

    for index, raw in enumerate(spec_text.split("\n")):
        line_no = index + 1
        text = raw.strip()
        if not text or text[0] == "#":
            continue
        words = text.split()
        kind = words[0].lower()

        if kind == "raw":
            if len(words) < 3:
                raise SpecError("line %d: raw needs an offset and a command" % line_no)
            offset = parse_offset(words[1], line_no)
            verb = words[2].lower()
            if verb not in VERB_SET:
                raise SpecError("line %d: raw verb '%s' is not allowed"
                                % (line_no, words[2]))
            raw_steps.append((offset, seq, " ".join(words[2:])))
            seq += 1
            if raw_max is None or offset > raw_max:
                raw_max = offset

        elif kind == "cast":
            if len(words) != 5:
                raise SpecError("line %d: cast needs <bot> <skill> <target> <cycles>"
                                % line_no)
            skill = parse_spec_int(words[2], line_no, "skill id", 0, 2147483647)
            if skill not in magic:
                raise SpecError("line %d: unknown skill %d" % (line_no, skill))
            cycles = parse_spec_int(words[4], line_no, "cycles", 1, 20)
            actions.append((line_no, "cast", words[1], skill, words[3], cycles))

        elif kind == "pot":
            if len(words) != 4:
                raise SpecError("line %d: pot needs <bot> <item> <count>" % line_no)
            item = parse_spec_int(words[2], line_no, "item id", 1, 2147483647)
            count = parse_spec_int(words[3], line_no, "count", 1, 20)
            actions.append((line_no, "pot", words[1], item, None, count))

        else:
            raise SpecError("line %d: unknown line type '%s'" % (line_no, words[0]))

    t0 = 0 if raw_max is None else raw_max + start_gap_ms

    cursors = {}
    bot_order = []
    action_steps = []    # (offset, seq, command)

    for line_no, kind, bot, first, target, count in actions:
        if bot not in cursors:
            cursors[bot] = t0
            bot_order.append(bot)
        offset = cursors[bot]
        if kind == "cast":
            row = magic[first]
            period = cast_period_ms(row)
            command = "cast %s %d %s %d" % (bot, first, target, count)
            action_steps.append((offset, seq, command))
            seq += 1
            cursors[bot] = offset + count * period + margin_ms
        else:
            command = "pot %s %d %d" % (bot, first, count)
            action_steps.append((offset, seq, command))
            seq += 1
            cursors[bot] = offset + count * POT_COOLDOWN_MS + margin_ms

    if cursors:
        last_end = max(cursors.values())
    else:
        last_end = raw_max if raw_max is not None else 0
    steps = raw_steps + action_steps
    steps.append((last_end + TRAILING_GAP_MS, seq, "list"))
    steps.sort(key=lambda item: (item[0], item[1]))

    header = ("# generated by tools/skill-script-gen.py from %s; "
              "do not edit by hand." % spec_name)
    bot_header = "# bots: %s" % " ".join(bot_order)
    lines_out = [header, bot_header]
    for offset, _seq, command in steps:
        lines_out.append("%d %s" % (offset, command))
    text = "\n".join(lines_out) + "\n"

    enforce_limits(steps, lines_out, text)
    return text


def enforce_limits(steps, lines_out, text):
    """Raises LimitError when a ScriptPlan.h limit is exceeded."""
    step_count = len(steps)
    if step_count > MAX_STEPS:
        raise LimitError("step limit exceeded: %d > %d" % (step_count, MAX_STEPS))

    line_count = len(lines_out)
    if line_count > MAX_LINES:
        raise LimitError("line limit exceeded: %d > %d" % (line_count, MAX_LINES))

    byte_count = len(text.encode("utf-8"))
    if byte_count > MAX_BYTES:
        raise LimitError("byte limit exceeded: %d > %d" % (byte_count, MAX_BYTES))

    for offset, _seq, _command in steps:
        if offset > MAX_OFFSET_MS:
            raise LimitError("offset limit exceeded: %d > %d" % (offset, MAX_OFFSET_MS))

    for line in lines_out:
        if len(line) > MAX_LINE_LEN:
            raise LimitError("line too long: %d > %d" % (len(line), MAX_LINE_LEN))


def count_physical_lines(text):
    """Splits on '\\n'; a trailing '\\n' does not add an empty final line."""
    lines = []
    start = 0
    size = len(text)
    while start <= size:
        nl = text.find("\n", start)
        if nl == -1:
            if start < size:
                lines.append(text[start:])
            break
        lines.append(text[start:nl])
        start = nl + 1
    return lines


def check_bytes(raw_bytes):
    """Mirrors BotCore::ParseScript. Returns (ok, message)."""
    size = len(raw_bytes)
    if size > MAX_BYTES:
        return False, "0: file too large (%d bytes)" % size

    text = raw_bytes.decode("utf-8", errors="replace")
    lines = count_physical_lines(text)
    if len(lines) > MAX_LINES:
        return False, "0: too many lines (%d)" % len(lines)

    steps = 0
    last_offset = 0
    for index, line in enumerate(lines):
        line_no = index + 1
        if line.endswith("\r"):
            line = line[:-1]

        if len(line) > MAX_LINE_LEN:
            return False, "%d: line too long" % line_no

        for ch in line:
            code = ord(ch)
            if code == 9:
                continue
            if code < 0x20 or code == 0x7F:
                return False, "%d: control character" % line_no

        trimmed = line.strip(" \t")
        if not trimmed or trimmed[0] == "#":
            continue

        parts = trimmed.split(None, 1)
        token = parts[0]
        rest = parts[1].strip(" \t") if len(parts) > 1 else ""

        digits_ok = bool(token) and len(token) <= 9 and all(c in "0123456789" for c in token)
        if not digits_ok:
            return False, "%d: bad offset" % line_no

        value = int(token, 10)
        if value > MAX_OFFSET_MS:
            return False, "%d: offset out of range" % line_no
        if value < last_offset:
            return False, "%d: offsets must not decrease" % line_no
        if not rest:
            return False, "%d: missing command" % line_no

        verb = rest.split(None, 1)[0]
        if verb.lower() not in VERB_SET:
            return False, "%d: bad verb" % line_no

        if steps >= MAX_STEPS:
            return False, "%d: too many steps" % line_no

        steps += 1
        last_offset = value

    if steps == 0:
        return False, "0: no steps"

    return True, ("ok: %d steps, %d bytes, %d lines, last offset %d ms"
                  % (steps, size, len(lines), last_offset))


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def magic_to_text(magic):
    """Serializes a MAGIC dict to the tool's row format (used by selftest)."""
    rows = []
    for num in sorted(magic):
        row = magic[num]
        rows.append("%d|%s|%s|%s|%s|%s|%s|%s" % (
            num, row.get("name", "?"), row.get("msp", 0), row.get("cast_time", 0),
            row.get("recast_time", 0), row.get("range", 0), row.get("type1", 0),
            row.get("type2", 0)))
    return "\n".join(rows) + "\n"


def run_selftest():
    checks = []
    failures = []

    def check(name, condition):
        checks.append(name)
        if not condition:
            failures.append(name)

    def gen(spec, magic, margin=DEFAULT_MARGIN_MS, start=DEFAULT_START_GAP_MS):
        return generate(spec, magic, "test.spec", margin, start)

    def offsets(text):
        out = []
        for line in text.split("\n"):
            trimmed = line.strip()
            if not trimmed or trimmed[0] == "#":
                continue
            parts = trimmed.split(None, 1)
            out.append((int(parts[0]), parts[1] if len(parts) > 1 else ""))
        return out

    def raises_spec(spec, magic):
        try:
            gen(spec, magic)
            return False
        except SpecError:
            return True

    def raises_limit(spec, magic):
        try:
            gen(spec, magic)
            return False
        except LimitError:
            return True

    magic = {
        1000: {"name": "a", "msp": 10, "cast_time": 1, "recast_time": 0,
               "range": 1, "type1": 1, "type2": 0},
        2000: {"name": "b", "msp": 10, "cast_time": 1, "recast_time": 0,
               "range": 1, "type1": 1, "type2": 0},
        3000: {"name": "recast", "msp": 10, "cast_time": 15, "recast_time": 54,
               "range": 1, "type1": 1, "type2": 0},
        4000: {"name": "castdom", "msp": 10, "cast_time": 15, "recast_time": 1,
               "range": 1, "type1": 1, "type2": 0},
        5000: {"name": "floor", "msp": 10, "cast_time": 0, "recast_time": 1,
               "range": 1, "type1": 1, "type2": 0},
    }

    # 1. cursor_sequential: the same bot advances back to back.
    o = offsets(gen("cast B 1000 self 2\ncast B 1000 self 2\n", magic))
    check("cursor_sequential", o[0][0] == 0 and o[1][0] == 2 * 1000 + 1500)

    # 2. bots_parallel: two bots start at the same t0.
    o = offsets(gen("cast A 1000 self 1\ncast B 2000 self 1\n", magic))
    check("bots_parallel", o[0][0] == 0 and o[1][0] == 0)

    # 3. period_recast_dominates: ReCastTime 54 -> period 5400.
    o = offsets(gen("cast B 3000 self 1\ncast B 3000 self 1\n", magic))
    check("period_recast_dominates", o[1][0] == 5400 + 1500)

    # 4. period_cast_dominates: CastTime 15 -> period 1580 + 140 = 1720.
    o = offsets(gen("cast B 4000 self 1\ncast B 4000 self 1\n", magic))
    check("period_cast_dominates", o[1][0] == 1720 + 1500)

    # 5. period_floor_1000: CastTime 0 -> the 1000 ms floor wins.
    o = offsets(gen("cast B 5000 self 1\ncast B 5000 self 1\n", magic))
    check("period_floor_1000", o[1][0] == 1000 + 1500)

    # 6. pot_advance: count * 2500 + margin.
    o = offsets(gen("pot B 389220000 3\npot B 389220000 1\n", magic))
    check("pot_advance", o[1][0] == 3 * 2500 + 1500)

    # 7. raw_sets_t0: the last raw offset drives t0, and no raw means t0 = 0.
    o = offsets(gen("raw 5000 list\ncast B 1000 self 1\n", magic))
    cast_off = [off for off, cmd in o if cmd.startswith("cast")]
    check("raw_sets_t0", cast_off and cast_off[0] == 6500)
    o = offsets(gen("cast B 1000 self 1\n", magic))
    cast_off = [off for off, cmd in o if cmd.startswith("cast")]
    check("raw_absent_t0", cast_off and cast_off[0] == 0)

    # 8. sorted_stable: equal offsets keep spec order.
    o = offsets(gen("cast A 1000 self 1\ncast B 2000 self 1\n", magic))
    check("sorted_stable",
          o[0][0] == o[1][0] and o[0][1].split()[1] == "A"
          and o[1][1].split()[1] == "B")

    # 9. trailing_list: the last step is a list 500 ms after the last end.
    o = offsets(gen("cast B 1000 self 1\n", magic))
    check("trailing_list", o[-1][1].startswith("list") and o[-1][0] == 2500 + 500)

    # 10. unknown_skill: not in MAGIC -> exit 2 with the line number.
    check("unknown_skill", raises_spec("cast B 777777 self 1\n", magic))

    # 11. bad_verb_raw: a raw verb outside the allowed list -> exit 2.
    check("bad_verb_raw", raises_spec("raw 0 teleport X\n", magic))

    # 12. bad_cycles: cast cycles and pot count out of range -> exit 2.
    check("bad_cycles", all((
        raises_spec("cast B 1000 self 0\n", magic),
        raises_spec("cast B 1000 self 21\n", magic),
        raises_spec("pot B 1 0\n", magic),
        raises_spec("pot B 1 21\n", magic),
    )))

    # 13. step_limit: more than 100 steps -> exit 1 and no file is written.
    many = "\n".join("cast B 1000 self 1" for _ in range(101)) + "\n"
    check("step_limit", raises_limit(many, magic))
    with tempfile.TemporaryDirectory() as tmp:
        spec_path = os.path.join(tmp, "many.spec")
        magic_path = os.path.join(tmp, "magic.txt")
        out_path = os.path.join(tmp, "out.txt")
        write_text(spec_path, many)
        write_text(magic_path, magic_to_text(magic))
        quiet = io.StringIO()
        with contextlib.redirect_stderr(quiet):
            rc = main([spec_path, "--magic", magic_path, "--out", out_path])
        check("step_limit_no_file", rc == 1 and not os.path.exists(out_path))

    # 14. offset_limit: an offset above 600000 -> exit 1.
    check("offset_limit", raises_limit("raw 600000 list\ncast B 1000 self 1\n", magic))

    # 15. deterministic: the same input gives byte-identical output.
    spec = "cast A 1000 self 1\ncast B 2000 self 1\n"
    check("deterministic", gen(spec, magic) == gen(spec, magic))

    # 16. check_ok_and_bad: the checker accepts a clean script and rejects each
    #     ScriptPlan.h violation class.
    ok, message = check_bytes(b"0 list\n")
    check("check_ok", ok and message.startswith("ok: 1 steps"))
    bad, _ = check_bytes(b"100 list\n50 list\n")
    check("check_bad_offset_order", not bad)
    bad, _ = check_bytes(b"0 list\n" * 129)
    check("check_too_many_lines", not bad)
    bad, _ = check_bytes(b"0 " + b"x" * 254 + b"\n")
    check("check_line_too_long", not bad)
    bad, _ = check_bytes(b"0 teleport X\n")
    check("check_bad_verb", not bad)

    # 17. sqlcmd_non_utf8: a fake sqlcmd with a non-UTF-8 byte must not crash.
    if os.name == "posix":
        with tempfile.TemporaryDirectory() as tmp:
            fake = os.path.join(tmp, "fake_sqlcmd.sh")
            write_text(fake,
                       "#!/bin/sh\n"
                       "printf '112527|great\\250|80|15|20|56|3|0\\n'\n")
            os.chmod(fake, 0o755)
            try:
                loaded = load_magic_sql(fake, "s", "d")
            except (SpecError, OSError):
                loaded = None
            if loaded is None:
                check("sqlcmd_non_utf8_failed", False)
            else:
                check("sqlcmd_non_utf8", loaded.get(112527, {}).get("msp") == 80)
    else:
        check("sqlcmd_non_utf8_skipped", True)

    if failures:
        for name in failures:
            sys.stderr.write("FAILED: %s\n" % name)
    print("selftest: %d checks, %d failed" % (len(checks), len(failures)))
    return 0 if not failures else 1


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]
    if "--selftest" in argv:
        return run_selftest()

    parser = argparse.ArgumentParser(prog="skill-script-gen.py")
    parser.add_argument("spec", nargs="?")
    parser.add_argument("--out")
    parser.add_argument("--magic")
    parser.add_argument("--sqlcmd", default=DEFAULT_SQLCMD)
    parser.add_argument("--server", default=DEFAULT_SERVER)
    parser.add_argument("--db", default=DEFAULT_DB)
    parser.add_argument("--margin-ms", type=int, default=DEFAULT_MARGIN_MS)
    parser.add_argument("--start-gap-ms", type=int, default=DEFAULT_START_GAP_MS)
    parser.add_argument("--check")
    args = parser.parse_args(argv)

    if args.check:
        try:
            with open(args.check, "rb") as handle:
                raw_bytes = handle.read()
        except OSError as exc:
            sys.stderr.write("error: cannot read script: %s\n" % exc)
            return 2
        ok, message = check_bytes(raw_bytes)
        if ok:
            sys.stdout.write(message + "\n")
            return 0
        sys.stderr.write("%s\n" % message)
        return 1

    if not args.spec:
        sys.stderr.write(USAGE)
        return 2

    try:
        try:
            with open(args.spec, "r", encoding="utf-8", errors="replace") as handle:
                spec_text = handle.read()
        except OSError as exc:
            raise SpecError("cannot read spec: %s" % exc)

        if args.magic:
            magic = load_magic_file(args.magic)
        else:
            magic = load_magic_sql(args.sqlcmd, args.server, args.db)

        spec_name = os.path.basename(args.spec)
        text = generate(spec_text, magic, spec_name, args.margin_ms,
                        args.start_gap_ms)
    except SpecError as exc:
        sys.stderr.write("error: %s\n" % exc)
        return 2
    except LimitError as exc:
        sys.stderr.write("error: %s\n" % exc)
        return 1

    if args.out:
        try:
            write_text(args.out, text)
        except OSError as exc:
            sys.stderr.write("error: cannot write output: %s\n" % exc)
            return 2
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
