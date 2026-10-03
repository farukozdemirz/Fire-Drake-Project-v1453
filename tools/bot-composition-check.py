#!/usr/bin/env python3
"""Checks bot character set coverage against the 8v8 EVAL compositions.

Parses the @bots rows of the db/002 (and future db/003) SQL script as text,
validates them against the ADR-0002 character naming/class rules, and reports
which additional characters are needed for the composition sets (small, min16,
full20) and for each EVAL pairing.  Implements the rules of ADR-0002 Ek F8-02
under the source of truth of docs/09 section 2.4 and docs/15 section 6a.

The tool only reads files (CLI mode writes nothing) and never talks to the
server or the database.

Usage:
    python3 tools/bot-composition-check.py [--sql PATH ...] [--max-bots N]
        [--json] [--strict] [--target T]
    python3 tools/bot-composition-check.py --selftest

Options:
    --sql PATH     SQL file to parse (repeatable; default db/002_bot_characters.sql)
    --max-bots N   concurrent bot cap for PAIR status (integer >= 1, default 16)
    --target T     small | min16 | full20 (strict target, default full20)
    --json         write one JSON object instead of the text report
    --strict       exit 1 when the target set is missing characters
Exit codes: 0 complete, 1 --strict and target set SHORT, 2 usage error /
unreadable file / any row validation ERROR (2 beats 1).
"""

import json
import os
import re
import sys
import tempfile

PROFILES = ("WP", "WG", "PHD", "PHB", "MF", "MI")
NATIONS = (("K", 1), ("E", 2))
MAX_ID_SIZE = 20

CLASS_CODES = {
    ("WP", 1): 106, ("WG", 1): 106,
    ("PHD", 1): 112, ("PHB", 1): 112,
    ("MF", 1): 110, ("MI", 1): 110,
    ("WP", 2): 206, ("WG", 2): 206,
    ("PHD", 2): 212, ("PHB", 2): 212,
    ("MF", 2): 210, ("MI", 2): 210,
}

# Composition table copied from docs/09 section 2.4 (source of truth).
COMPOSITIONS = (
    ("C8-A", {"WP": 2, "WG": 1, "PHD": 1, "PHB": 1, "MF": 2, "MI": 1}),
    ("C8-B", {"WP": 3, "WG": 1, "PHD": 1, "PHB": 1, "MF": 1, "MI": 1}),
    ("C8-C", {"WP": 1, "WG": 1, "PHD": 1, "PHB": 1, "MF": 3, "MI": 1}),
    ("C8-D", {"WP": 3, "WG": 1, "PHD": 1, "PHB": 0, "MF": 2, "MI": 1}),
    ("C2", {"WP": 1, "WG": 0, "PHD": 1, "PHB": 0, "MF": 0, "MI": 0}),
    ("C3", {"WP": 1, "WG": 0, "PHD": 1, "PHB": 0, "MF": 1, "MI": 0}),
    ("C4", {"WP": 1, "WG": 1, "PHD": 1, "PHB": 0, "MF": 1, "MI": 0}),
    ("C5", {"WP": 1, "WG": 1, "PHD": 1, "PHB": 1, "MF": 1, "MI": 0}),
)
COMP_MAP = dict(COMPOSITIONS)

# Per nation need per set = max of the member compositions per profile
# (ADR-0002 Ek F8-02 madde 2).
SET_MEMBERS = {
    "small": ("C2", "C3", "C4", "C5"),
    "min16": ("C8-A",),
    "full20": ("C8-A", "C8-B", "C8-C", "C8-D"),
}
SET_ORDER = ("small", "min16", "full20")
SET_NEEDS = {}
for _set_id in SET_ORDER:
    _need = {}
    for _profile in PROFILES:
        _need[_profile] = max(COMP_MAP[_member][_profile]
                              for _member in SET_MEMBERS[_set_id])
    SET_NEEDS[_set_id] = _need

# Pair specifications (docs/15 section 4.6).  When the two sides differ both
# assignments are emitted (karus/el_morad swapped).
PAIR_SPECS = (
    ("EVAL-2v2", "C2", "C2"),
    ("EVAL-3v3", "C3", "C3"),
    ("EVAL-4v4", "C4", "C4"),
    ("EVAL-5v5", "C5", "C5"),
    ("EVAL-8v8-A", "C8-A", "C8-A"),
    ("EVAL-8v8-MIX", "C8-B", "C8-C"),
)

ROW_RE = re.compile(
    r"^\s*\(\s*'([A-Z]+)'\s*,\s*'([^']*)'\s*,\s*'([^']*)'\s*,\s*"
    r"(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,")
NAME_RE = re.compile(r"^Bot(WP|WG|PHD|PHB|MF|MI)([2-9]|[1-9][0-9]+)?_(K|E)$")

USAGE = (
    "Usage:\n"
    "  python3 tools/bot-composition-check.py [--sql PATH ...] [--max-bots N]\n"
    "      [--json] [--strict] [--target T]\n"
    "  python3 tools/bot-composition-check.py --selftest\n"
    "Options:\n"
    "  --sql PATH     SQL file (repeatable; default db/002_bot_characters.sql)\n"
    "  --max-bots N   integer >= 1 (default 16)\n"
    "  --target T     small | min16 | full20 (default full20)\n"
    "  --json         write one JSON object instead of the text report\n"
    "  --strict       exit 1 when the target set is SHORT\n"
)


def tool_dir():
    return os.path.dirname(os.path.abspath(__file__))


def default_sql_path():
    return os.path.normpath(os.path.join(tool_dir(), "..", "db",
                                         "002_bot_characters.sql"))


def sample_sql_path():
    return os.path.join(tool_dir(), "bot-composition-check", "sample-20.sql")


def parse_int(raw):
    try:
        return int(raw, 10)
    except (TypeError, ValueError):
        return None


def parse_args(argv):
    """Returns (options, error_message)."""
    opts = {"sql": [], "max_bots": 16, "json": False, "strict": False,
            "target": "full20"}
    index = 0
    while index < len(argv):
        arg = argv[index]
        if arg == "--json":
            opts["json"] = True
            index += 1
        elif arg == "--strict":
            opts["strict"] = True
            index += 1
        elif arg == "--sql":
            if index + 1 >= len(argv):
                return None, "option --sql needs a value"
            opts["sql"].append(argv[index + 1])
            index += 2
        elif arg == "--max-bots":
            if index + 1 >= len(argv):
                return None, "option --max-bots needs a value"
            value = parse_int(argv[index + 1])
            if value is None or value < 1:
                return None, "invalid --max-bots: %s" % argv[index + 1]
            opts["max_bots"] = value
            index += 2
        elif arg == "--target":
            if index + 1 >= len(argv):
                return None, "option --target needs a value"
            value = argv[index + 1]
            if value not in SET_ORDER:
                return None, "invalid --target: %s" % value
            opts["target"] = value
            index += 2
        elif arg.startswith("--"):
            return None, "unknown argument: %s" % arg
        else:
            return None, "unexpected argument: %s" % arg
    return opts, None


def read_sql_text(path):
    with open(path, "r", encoding="utf-8-sig", errors="replace") as handle:
        return handle.read()


def strip_block_comments(text):
    """Removes /* ... */ blocks while preserving line numbers."""
    def blank(match):
        return re.sub(r"[^\n]", "", match.group(0))
    return re.sub(r"/\*.*?\*/", blank, text, flags=re.S)


def validate_row(profile, name, account, nation, race, cls, path, lineno,
                 errors, seen_names, seen_accounts):
    if profile not in PROFILES:
        errors.append((path, lineno, "unknown profile '%s'" % profile))
        return None
    if nation not in (1, 2):
        errors.append((path, lineno, "invalid nation %d" % nation))
        return None
    match = NAME_RE.match(name)
    if not match:
        errors.append((path, lineno, "invalid character name '%s'" % name))
        return None
    name_profile = match.group(1)
    index_str = match.group(2)
    suffix = match.group(3)
    if name_profile != profile:
        errors.append((path, lineno, "name profile mismatch"))
        return None
    suffix_nation = 1 if suffix == "K" else 2
    if suffix_nation != nation:
        errors.append((path, lineno, "name nation suffix mismatch"))
        return None
    if len(name) > MAX_ID_SIZE:
        errors.append((path, lineno, "name too long"))
        return None
    index = int(index_str) if index_str else 1
    expected_account = "BotAcc%s%s%s" % (profile, index_str or "", suffix)
    if not account.isalnum():
        errors.append((path, lineno, "account not alphanumeric"))
        return None
    if name in seen_names:
        errors.append((path, lineno, "duplicate name '%s'" % name))
        return None
    if account in seen_accounts:
        errors.append((path, lineno, "duplicate account '%s'" % account))
        return None
    if account != expected_account:
        errors.append((path, lineno, "account does not match name"))
        return None
    if cls != CLASS_CODES[(profile, nation)]:
        errors.append((path, lineno, "class mismatch"))
        return None
    seen_names.add(name)
    seen_accounts.add(account)
    return {"profile": profile, "name": name, "account": account,
            "nation": nation, "race": race, "class": cls, "index": index}


def parse_sql_text(text, path, errors, seen_names=None, seen_accounts=None):
    """Returns (records, found_any_row).  seen sets may be shared by files."""
    records = []
    found_any = False
    if seen_names is None:
        seen_names = set()
    if seen_accounts is None:
        seen_accounts = set()
    cleaned = strip_block_comments(text)
    for lineno, line in enumerate(cleaned.split("\n"), 1):
        stripped = line.strip()
        if not stripped or stripped.startswith("--"):
            continue
        match = ROW_RE.match(line)
        if not match:
            continue
        found_any = True
        record = validate_row(
            match.group(1), match.group(2), match.group(3),
            int(match.group(4)), int(match.group(5)), int(match.group(6)),
            path, lineno, errors, seen_names, seen_accounts)
        if record is not None:
            records.append(record)
    return records, found_any


def comp_rule(counts):
    priests = counts["PHD"] + counts["PHB"]
    return "ok" if priests <= 2 else "too_many_priests"


def pair_assignments():
    rows = []
    for pair_id, karus, el_morad in PAIR_SPECS:
        rows.append((pair_id, karus, el_morad))
        if karus != el_morad:
            rows.append((pair_id, el_morad, karus))
    return rows


def build_result(records, files, max_bots):
    have = {}
    for _nat_name, nat in NATIONS:
        have[nat] = dict((profile, 0) for profile in PROFILES)
    used = {}
    for record in records:
        have[record["nation"]][record["profile"]] += 1
        used.setdefault((record["nation"], record["profile"]), set()).add(
            record["index"])

    have_out = {}
    for nat_name, nat in NATIONS:
        entry = {}
        total = 0
        for profile in PROFILES:
            entry[profile] = have[nat][profile]
            total += have[nat][profile]
        entry["total"] = total
        have_out[nat_name] = entry

    compositions_out = []
    for comp_id, counts in COMPOSITIONS:
        compositions_out.append({
            "id": comp_id,
            "size": sum(counts.values()),
            "counts": dict((profile, counts[profile]) for profile in PROFILES),
            "priests": counts["PHD"] + counts["PHB"],
            "rule": comp_rule(counts),
        })

    sets_out = []
    for set_id in SET_ORDER:
        need = SET_NEEDS[set_id]
        per_nation = sum(need.values())
        total = per_nation * 2
        usable = 0
        for _nat_name, nat in NATIONS:
            for profile in PROFILES:
                usable += min(have[nat][profile], need[profile])
        missing = total - usable
        sets_out.append({
            "id": set_id,
            "per_nation": per_nation,
            "total": total,
            "usable": usable,
            "missing": missing,
            "status": "OK" if missing == 0 else "SHORT",
        })
    set_map = dict((entry["id"], entry) for entry in sets_out)

    missing_out = []
    for nat_name, nat in NATIONS:
        for profile in PROFILES:
            need = SET_NEEDS["full20"][profile]
            have_count = have[nat][profile]
            if need <= have_count:
                continue
            count = need - have_count
            used_idx = used.get((nat, profile), set())
            slots = []
            candidate = 1
            while len(slots) < count:
                if candidate not in used_idx:
                    slots.append(candidate)
                candidate += 1
            for position, slot in enumerate(slots, 1):
                order = have_count + position
                needed_by = ("min16" if order <= SET_NEEDS["min16"][profile]
                             else "full20")
                if slot == 1:
                    char = "Bot%s_%s" % (profile, nat_name)
                    account = "BotAcc%s%s" % (profile, nat_name)
                else:
                    char = "Bot%s%d_%s" % (profile, slot, nat_name)
                    account = "BotAcc%s%d%s" % (profile, slot, nat_name)
                missing_out.append({
                    "nation": nat_name,
                    "profile": profile,
                    "index": slot,
                    "char": char,
                    "account": account,
                    "class": CLASS_CODES[(profile, nat)],
                    "needed_by": needed_by,
                })

    pairs_out = []
    pairs_ok = 0
    pairs_not_ok = 0
    for pair_id, karus, el_morad in pair_assignments():
        counts_a = COMP_MAP[karus]
        counts_b = COMP_MAP[el_morad]
        missing = 0
        for profile in PROFILES:
            missing += max(0, counts_a[profile] - have[1][profile])
            missing += max(0, counts_b[profile] - have[2][profile])
        online = sum(counts_a.values()) + sum(counts_b.values())
        if missing > 0:
            status = "SHORT"
        elif online > max_bots:
            status = "OVER_MAX_BOTS"
        else:
            status = "OK"
        if status == "OK":
            pairs_ok += 1
        else:
            pairs_not_ok += 1
        pairs_out.append({
            "id": pair_id,
            "karus": karus,
            "el_morad": el_morad,
            "online": online,
            "max_bots": max_bots,
            "missing": missing,
            "status": status,
        })

    summary = {
        "have": len(records),
        "small_missing": set_map["small"]["missing"],
        "min16_missing": set_map["min16"]["missing"],
        "full20_missing": set_map["full20"]["missing"],
        "pairs_ok": pairs_ok,
        "pairs_not_ok": pairs_not_ok,
        "errors": 0,
    }
    return {
        "files": list(files),
        "rows": len(records),
        "have": have_out,
        "compositions": compositions_out,
        "sets": sets_out,
        "missing": missing_out,
        "pairs": pairs_out,
        "summary": summary,
        "errors": [],
    }


def render_text(result):
    lines = []
    lines.append("SQL files=%d rows=%d"
                 % (len(result["files"]), result["rows"]))
    for nat_name in ("K", "E"):
        entry = result["have"][nat_name]
        lines.append(
            "HAVE nation=%s WP=%d WG=%d PHD=%d PHB=%d MF=%d MI=%d total=%d"
            % (nat_name, entry["WP"], entry["WG"], entry["PHD"], entry["PHB"],
               entry["MF"], entry["MI"], entry["total"]))
    for comp in result["compositions"]:
        counts = comp["counts"]
        lines.append(
            "COMP id=%s size=%d WP=%d WG=%d PHD=%d PHB=%d MF=%d MI=%d "
            "priests=%d rule=%s"
            % (comp["id"], comp["size"], counts["WP"], counts["WG"],
               counts["PHD"], counts["PHB"], counts["MF"], counts["MI"],
               comp["priests"], comp["rule"]))
    for entry in result["sets"]:
        lines.append(
            "SET id=%s per_nation=%d total=%d usable=%d missing=%d status=%s"
            % (entry["id"], entry["per_nation"], entry["total"],
               entry["usable"], entry["missing"], entry["status"]))
    for entry in result["missing"]:
        lines.append(
            "MISSING nation=%s profile=%s index=%d char=%s account=%s "
            "class=%d needed_by=%s"
            % (entry["nation"], entry["profile"], entry["index"],
               entry["char"], entry["account"], entry["class"],
               entry["needed_by"]))
    for entry in result["pairs"]:
        lines.append(
            "PAIR id=%s karus=%s el_morad=%s online=%d max_bots=%d "
            "missing=%d status=%s"
            % (entry["id"], entry["karus"], entry["el_morad"], entry["online"],
               entry["max_bots"], entry["missing"], entry["status"]))
    summary = result["summary"]
    lines.append(
        "SUMMARY have=%d small_missing=%d min16_missing=%d full20_missing=%d "
        "pairs_ok=%d pairs_not_ok=%d errors=%d"
        % (summary["have"], summary["small_missing"], summary["min16_missing"],
           summary["full20_missing"], summary["pairs_ok"],
           summary["pairs_not_ok"], summary["errors"]))
    return "\n".join(lines) + "\n"


def render_json(result):
    payload = {
        "files": result["files"],
        "rows": result["rows"],
        "have": result["have"],
        "compositions": result["compositions"],
        "sets": result["sets"],
        "missing": result["missing"],
        "pairs": result["pairs"],
        "summary": result["summary"],
        "errors": result["errors"],
    }
    return json.dumps(payload, sort_keys=True) + "\n"


def run(argv):
    """Parses argv and evaluates; returns (exit_code, stdout, stderr)."""
    opts, err = parse_args(argv)
    if err is not None:
        return 2, "", "error: %s\n%s" % (err, USAGE)
    files = opts["sql"] if opts["sql"] else [default_sql_path()]
    errors = []
    records = []
    seen_names = set()
    seen_accounts = set()
    for path in files:
        if not os.path.isfile(path):
            return 2, "", "error: cannot read file: %s\n" % path
        try:
            text = read_sql_text(path)
        except OSError as exc:
            return 2, "", "error: cannot read file: %s\n" % exc
        file_records, found_any = parse_sql_text(
            text, path, errors, seen_names, seen_accounts)
        if not found_any:
            errors.append((path, 0, "no character rows found"))
        records.extend(file_records)

    if errors:
        if opts["json"]:
            payload = {"errors": [
                {"file": path, "line": line, "message": message}
                for path, line, message in errors]}
            return 2, json.dumps(payload, sort_keys=True) + "\n", ""
        lines = []
        for path, line, message in errors:
            lines.append("ERROR %s:%d: %s" % (path, line, message))
        return 2, "\n".join(lines) + "\n", ""

    result = build_result(records, files, opts["max_bots"])
    out = render_json(result) if opts["json"] else render_text(result)
    for entry in result["sets"]:
        if entry["id"] == opts["target"] and entry["missing"] > 0 \
                and opts["strict"]:
            return 1, out, ""
    return 0, out, ""


def make_sql(rows):
    """Builds a db/002-shaped fixture (precheck tuple + hidden comment row)."""
    lines = [
        "-- generated fixture for selftest (not executable SQL)",
        "IF EXISTS (",
        "    SELECT 1",
        "    FROM (VALUES",
        "        ('BotWP_K','BotAccWPK'), ('BotWG_K','BotAccWGK')",
        "    ) AS b(charName, account)",
        ")",
    ]
    if rows:
        lines.append(
            "INSERT INTO @bots (profile, charName, account, nation, race, "
            "class, hp)")
        lines.append("VALUES")
        for index, item in enumerate(rows):
            tail = ";" if index == len(rows) - 1 else ","
            lines.append("    ('%s', '%s', '%s', %d, %d, %d, 0)%s"
                         % (item[0], item[1], item[2], item[3], item[4],
                            item[5], tail))
    lines.append("/* ('WP', 'BotWP9_K', 'BotAccWP9K', 1, 1, 106, 0), */")
    lines.append("GO")
    return "\n".join(lines) + "\n"


def sample20_rows():
    return [
        ("WP", "BotWP_K", "BotAccWPK", 1, 1, 106),
        ("WG", "BotWG_K", "BotAccWGK", 1, 1, 106),
        ("PHD", "BotPHD_K", "BotAccPHDK", 1, 4, 112),
        ("PHB", "BotPHB_K", "BotAccPHBK", 1, 4, 112),
        ("MF", "BotMF_K", "BotAccMFK", 1, 3, 110),
        ("MI", "BotMI_K", "BotAccMIK", 1, 3, 110),
        ("WP", "BotWP2_K", "BotAccWP2K", 1, 1, 106),
        ("WP", "BotWP3_K", "BotAccWP3K", 1, 1, 106),
        ("MF", "BotMF2_K", "BotAccMF2K", 1, 3, 110),
        ("MF", "BotMF3_K", "BotAccMF3K", 1, 3, 110),
        ("WP", "BotWP_E", "BotAccWPE", 2, 11, 206),
        ("WG", "BotWG_E", "BotAccWGE", 2, 11, 206),
        ("PHD", "BotPHD_E", "BotAccPHDE", 2, 12, 212),
        ("PHB", "BotPHB_E", "BotAccPHBE", 2, 12, 212),
        ("MF", "BotMF_E", "BotAccMFE", 2, 12, 210),
        ("MI", "BotMI_E", "BotAccMIE", 2, 12, 210),
        ("WP", "BotWP2_E", "BotAccWP2E", 2, 11, 206),
        ("WP", "BotWP3_E", "BotAccWP3E", 2, 11, 206),
        ("MF", "BotMF2_E", "BotAccMF2E", 2, 12, 210),
        ("MF", "BotMF3_E", "BotAccMF3E", 2, 12, 210),
    ]


def row(profile, name, account, nation=1, race=1, cls=None):
    if cls is None:
        cls = CLASS_CODES[(profile, nation)]
    return (profile, name, account, nation, race, cls)


REAL_EXPECTED = (
    "SQL files=1 rows=12\n"
    "HAVE nation=K WP=1 WG=1 PHD=1 PHB=1 MF=1 MI=1 total=6\n"
    "HAVE nation=E WP=1 WG=1 PHD=1 PHB=1 MF=1 MI=1 total=6\n"
    "COMP id=C8-A size=8 WP=2 WG=1 PHD=1 PHB=1 MF=2 MI=1 priests=2 rule=ok\n"
    "COMP id=C8-B size=8 WP=3 WG=1 PHD=1 PHB=1 MF=1 MI=1 priests=2 rule=ok\n"
    "COMP id=C8-C size=8 WP=1 WG=1 PHD=1 PHB=1 MF=3 MI=1 priests=2 rule=ok\n"
    "COMP id=C8-D size=8 WP=3 WG=1 PHD=1 PHB=0 MF=2 MI=1 priests=1 rule=ok\n"
    "COMP id=C2 size=2 WP=1 WG=0 PHD=1 PHB=0 MF=0 MI=0 priests=1 rule=ok\n"
    "COMP id=C3 size=3 WP=1 WG=0 PHD=1 PHB=0 MF=1 MI=0 priests=1 rule=ok\n"
    "COMP id=C4 size=4 WP=1 WG=1 PHD=1 PHB=0 MF=1 MI=0 priests=1 rule=ok\n"
    "COMP id=C5 size=5 WP=1 WG=1 PHD=1 PHB=1 MF=1 MI=0 priests=2 rule=ok\n"
    "SET id=small per_nation=5 total=10 usable=10 missing=0 status=OK\n"
    "SET id=min16 per_nation=8 total=16 usable=12 missing=4 status=SHORT\n"
    "SET id=full20 per_nation=10 total=20 usable=12 missing=8 status=SHORT\n"
    "MISSING nation=K profile=WP index=2 char=BotWP2_K account=BotAccWP2K "
    "class=106 needed_by=min16\n"
    "MISSING nation=K profile=WP index=3 char=BotWP3_K account=BotAccWP3K "
    "class=106 needed_by=full20\n"
    "MISSING nation=K profile=MF index=2 char=BotMF2_K account=BotAccMF2K "
    "class=110 needed_by=min16\n"
    "MISSING nation=K profile=MF index=3 char=BotMF3_K account=BotAccMF3K "
    "class=110 needed_by=full20\n"
    "MISSING nation=E profile=WP index=2 char=BotWP2_E account=BotAccWP2E "
    "class=206 needed_by=min16\n"
    "MISSING nation=E profile=WP index=3 char=BotWP3_E account=BotAccWP3E "
    "class=206 needed_by=full20\n"
    "MISSING nation=E profile=MF index=2 char=BotMF2_E account=BotAccMF2E "
    "class=210 needed_by=min16\n"
    "MISSING nation=E profile=MF index=3 char=BotMF3_E account=BotAccMF3E "
    "class=210 needed_by=full20\n"
    "PAIR id=EVAL-2v2 karus=C2 el_morad=C2 online=4 max_bots=16 missing=0 "
    "status=OK\n"
    "PAIR id=EVAL-3v3 karus=C3 el_morad=C3 online=6 max_bots=16 missing=0 "
    "status=OK\n"
    "PAIR id=EVAL-4v4 karus=C4 el_morad=C4 online=8 max_bots=16 missing=0 "
    "status=OK\n"
    "PAIR id=EVAL-5v5 karus=C5 el_morad=C5 online=10 max_bots=16 missing=0 "
    "status=OK\n"
    "PAIR id=EVAL-8v8-A karus=C8-A el_morad=C8-A online=16 max_bots=16 "
    "missing=4 status=SHORT\n"
    "PAIR id=EVAL-8v8-MIX karus=C8-B el_morad=C8-C online=16 max_bots=16 "
    "missing=4 status=SHORT\n"
    "PAIR id=EVAL-8v8-MIX karus=C8-C el_morad=C8-B online=16 max_bots=16 "
    "missing=4 status=SHORT\n"
    "SUMMARY have=12 small_missing=0 min16_missing=4 full20_missing=8 "
    "pairs_ok=4 pairs_not_ok=3 errors=0\n"
)

SAMPLE_EXPECTED = (
    "SQL files=1 rows=20\n"
    "HAVE nation=K WP=3 WG=1 PHD=1 PHB=1 MF=3 MI=1 total=10\n"
    "HAVE nation=E WP=3 WG=1 PHD=1 PHB=1 MF=3 MI=1 total=10\n"
    "COMP id=C8-A size=8 WP=2 WG=1 PHD=1 PHB=1 MF=2 MI=1 priests=2 rule=ok\n"
    "COMP id=C8-B size=8 WP=3 WG=1 PHD=1 PHB=1 MF=1 MI=1 priests=2 rule=ok\n"
    "COMP id=C8-C size=8 WP=1 WG=1 PHD=1 PHB=1 MF=3 MI=1 priests=2 rule=ok\n"
    "COMP id=C8-D size=8 WP=3 WG=1 PHD=1 PHB=0 MF=2 MI=1 priests=1 rule=ok\n"
    "COMP id=C2 size=2 WP=1 WG=0 PHD=1 PHB=0 MF=0 MI=0 priests=1 rule=ok\n"
    "COMP id=C3 size=3 WP=1 WG=0 PHD=1 PHB=0 MF=1 MI=0 priests=1 rule=ok\n"
    "COMP id=C4 size=4 WP=1 WG=1 PHD=1 PHB=0 MF=1 MI=0 priests=1 rule=ok\n"
    "COMP id=C5 size=5 WP=1 WG=1 PHD=1 PHB=1 MF=1 MI=0 priests=2 rule=ok\n"
    "SET id=small per_nation=5 total=10 usable=10 missing=0 status=OK\n"
    "SET id=min16 per_nation=8 total=16 usable=16 missing=0 status=OK\n"
    "SET id=full20 per_nation=10 total=20 usable=20 missing=0 status=OK\n"
    "PAIR id=EVAL-2v2 karus=C2 el_morad=C2 online=4 max_bots=16 missing=0 "
    "status=OK\n"
    "PAIR id=EVAL-3v3 karus=C3 el_morad=C3 online=6 max_bots=16 missing=0 "
    "status=OK\n"
    "PAIR id=EVAL-4v4 karus=C4 el_morad=C4 online=8 max_bots=16 missing=0 "
    "status=OK\n"
    "PAIR id=EVAL-5v5 karus=C5 el_morad=C5 online=10 max_bots=16 missing=0 "
    "status=OK\n"
    "PAIR id=EVAL-8v8-A karus=C8-A el_morad=C8-A online=16 max_bots=16 "
    "missing=0 status=OK\n"
    "PAIR id=EVAL-8v8-MIX karus=C8-B el_morad=C8-C online=16 max_bots=16 "
    "missing=0 status=OK\n"
    "PAIR id=EVAL-8v8-MIX karus=C8-C el_morad=C8-B online=16 max_bots=16 "
    "missing=0 status=OK\n"
    "SUMMARY have=20 small_missing=0 min16_missing=0 full20_missing=0 "
    "pairs_ok=7 pairs_not_ok=0 errors=0\n"
)


def run_selftest():
    results = []

    def case(name, func):
        try:
            func()
            results.append((name, True, ""))
        except AssertionError as exc:
            results.append((name, False, str(exc) or "assertion failed"))
        except Exception as exc:  # noqa: BLE001 - report any failure
            results.append((name, False, "error: %r" % exc))

    def check(condition, message):
        if not condition:
            raise AssertionError(message)

    def parse_text(text, path="<test>"):
        errors = []
        records, found = parse_sql_text(text, path, errors)
        return records, errors, found

    def result_of(rows, max_bots=16):
        records, errors, _found = parse_text(make_sql(rows))
        check(errors == [], "unexpected errors: %r" % errors)
        return build_result(records, ["<test>"], max_bots)

    def load_real():
        path = default_sql_path()
        errors = []
        records, found = parse_sql_text(read_sql_text(path), path, errors)
        check(errors == [], "real db errors: %r" % errors)
        check(found, "real db has no rows")
        return records, build_result(records, [path], 16)

    def doc09_path():
        return os.path.normpath(os.path.join(
            tool_dir(), "..", "docs",
            "09_PARTY_COORDINATION_AND_TARGET_SELECTION.md"))

    token_map = {"W-P": "WP", "W-G": "WG", "P-HD": "PHD", "P-HB": "PHB",
                 "M-F": "MF", "M-I": "MI"}

    with tempfile.TemporaryDirectory() as tmp:
        counter = [0]

        def write_tmp(text, suffix=".sql", binary=False):
            counter[0] += 1
            path = os.path.join(tmp, "case%d%s" % (counter[0], suffix))
            if binary:
                with open(path, "wb") as handle:
                    handle.write(text)
            else:
                with open(path, "w", encoding="utf-8", newline="\n") as handle:
                    handle.write(text)
            return path

        def run_sql(text, args=()):
            path = write_tmp(text)
            return run(list(args) + ["--sql", path])

        # --- parsing -------------------------------------------------------
        def parse_real_db002_rows():
            records, _result = load_real()
            check(len(records) == 12, "rows %d != 12" % len(records))
        case("parse_real_db002_rows", parse_real_db002_rows)

        def parse_real_db002_have():
            _records, result = load_real()
            for nat in ("K", "E"):
                for profile in PROFILES:
                    check(result["have"][nat][profile] == 1,
                          "have %s %s != 1" % (nat, profile))
                check(result["have"][nat]["total"] == 6,
                      "total %s != 6" % nat)
        case("parse_real_db002_have", parse_real_db002_have)

        def parse_ignores_comments_and_precheck_rows():
            rows = [row("WP", "BotWP_K", "BotAccWPK"),
                    row("WG", "BotWG_K", "BotAccWGK")]
            text = "-- ('MI', 'BotMI_K', 'BotAccMIK', 1, 3, 110, 0),\n" \
                   + make_sql(rows)
            records, errors, _found = parse_text(text)
            check(errors == [], "errors %r" % errors)
            check(len(records) == 2, "rows %d != 2" % len(records))
        case("parse_ignores_comments_and_precheck_rows",
             parse_ignores_comments_and_precheck_rows)

        def parse_crlf_and_bom():
            rows = [row("WP", "BotWP_K", "BotAccWPK")]
            text = make_sql(rows).replace("\n", "\r\n")
            path = write_tmp(b"\xef\xbb\xbf" + text.encode("utf-8"),
                             suffix=".sql", binary=True)
            errors = []
            records, found = parse_sql_text(read_sql_text(path), path, errors)
            check(errors == [], "errors %r" % errors)
            check(found and len(records) == 1,
                  "rows %d" % len(records))
        case("parse_crlf_and_bom", parse_crlf_and_bom)

        def parse_multiple_files_sum():
            path_a = write_tmp(make_sql([row("WP", "BotWP_K", "BotAccWPK")]))
            path_b = write_tmp(make_sql([row("WG", "BotWG_K", "BotAccWGK")]))
            rc, out, _err = run(["--sql", path_a, "--sql", path_b])
            check(rc == 0, "rc %d" % rc)
            check("SQL files=2 rows=2" in out, "out %r" % out)
        case("parse_multiple_files_sum", parse_multiple_files_sum)

        def parse_line_numbers():
            text = ("('XX', 'BotXX_K', 'BotAccXXK', 1, 1, 106, 0),\n"
                    "/*\ncomment\n*/\n"
                    "('YY', 'BotYY_K', 'BotAccYYK', 1, 1, 106, 0),\n")
            _records, errors, _found = parse_text(text)
            line_numbers = [line for _path, line, _message in errors]
            check(line_numbers == [1, 5], "line numbers %r" % line_numbers)
        case("parse_line_numbers", parse_line_numbers)

        # --- row validation ------------------------------------------------
        def expect_error(text, needle=None):
            rc, out, _err = run_sql(text)
            check(rc == 2, "rc %d != 2" % rc)
            check("ERROR" in out, "no ERROR line: %r" % out)
            if needle is not None:
                check(needle in out, "missing %r in %r" % (needle, out))
            return out

        def err_unknown_profile():
            expect_error(
                make_sql([("XX", "BotWP_K", "BotAccWPK", 1, 1, 106)]),
                "unknown profile")
        case("err_unknown_profile", err_unknown_profile)

        def err_bad_nation():
            expect_error(
                make_sql([("WP", "BotWP_K", "BotAccWPK", 3, 1, 106)]),
                "invalid nation")
        case("err_bad_nation", err_bad_nation)

        def err_name_profile_mismatch():
            expect_error(make_sql([row("WP", "BotWG_K", "BotAccWGK")]),
                         "name profile mismatch")
        case("err_name_profile_mismatch", err_name_profile_mismatch)

        def err_name_nation_suffix_mismatch():
            expect_error(make_sql([row("WP", "BotWP_E", "BotAccWPE", 1)]),
                         "name nation suffix mismatch")
        case("err_name_nation_suffix_mismatch", err_name_nation_suffix_mismatch)

        def err_class_mismatch():
            expect_error(
                make_sql([("WP", "BotWP_K", "BotAccWPK", 1, 1, 112)]),
                "class mismatch")
        case("err_class_mismatch", err_class_mismatch)

        def err_dup_name():
            text = make_sql([row("WP", "BotWP_K", "BotAccWPK"),
                             row("WP", "BotWP_K", "BotAccWPK")])
            expect_error(text, "duplicate name")
        case("err_dup_name", err_dup_name)

        def err_dup_account():
            text = make_sql([row("WP", "BotWP_K", "BotAccWPK"),
                             row("WG", "BotWG_K", "BotAccWPK")])
            expect_error(text, "duplicate account")
        case("err_dup_account", err_dup_account)

        def err_dup_across_files():
            path_a = write_tmp(make_sql([row("WP", "BotWP_K", "BotAccWPK")]))
            path_b = write_tmp(make_sql([row("WP", "BotWP_K", "BotAccWPK")]))
            rc, out, _err = run(["--sql", path_a, "--sql", path_b])
            check(rc == 2, "rc %d" % rc)
            check("ERROR" in out, "no ERROR: %r" % out)
        case("err_dup_across_files", err_dup_across_files)

        def err_account_not_alnum():
            expect_error(make_sql([row("WP", "BotWP_K", "BotAccWP_K")]),
                         "not alphanumeric")
        case("err_account_not_alnum", err_account_not_alnum)

        def err_account_mismatch():
            expect_error(make_sql([row("WG", "BotWG_K", "BotAccWPK")]),
                         "does not match name")
        case("err_account_mismatch", err_account_mismatch)

        def err_name_too_long():
            name = "BotWP12345678901234_K"
            check(len(name) == 21, "fixture name len %d" % len(name))
            account = "BotAccWP12345678901234K"
            expect_error(make_sql([("WP", name, account, 1, 1, 106)]),
                         "name too long")
        case("err_name_too_long", err_name_too_long)

        def err_index_one_rejected():
            expect_error(make_sql([row("WP", "BotWP1_K", "BotAccWP1K")]),
                         "invalid character name")
        case("err_index_one_rejected", err_index_one_rejected)

        def err_no_rows():
            text = ("-- only comments\n"
                    "IF EXISTS (SELECT 1)\n"
                    "/* ('WP', 'BotWP9_K', 'BotAccWP9K', 1, 1, 106, 0) */\n")
            rc, out, _err = run_sql(text)
            check(rc == 2, "rc %d" % rc)
            check("no character rows found" in out, "out %r" % out)
        case("err_no_rows", err_no_rows)

        def err_missing_file_rc2():
            rc, _out, _err = run(
                ["--sql", os.path.join(tmp, "does-not-exist.sql")])
            check(rc == 2, "rc %d" % rc)
        case("err_missing_file_rc2", err_missing_file_rc2)

        # --- compositions --------------------------------------------------
        def comp_sizes():
            sizes = [sum(counts.values()) for _cid, counts in COMPOSITIONS]
            check(sizes == [8, 8, 8, 8, 2, 3, 4, 5], "%r" % sizes)
        case("comp_sizes", comp_sizes)

        def comp_c8a_counts():
            check(COMP_MAP["C8-A"] == {"WP": 2, "WG": 1, "PHD": 1, "PHB": 1,
                                       "MF": 2, "MI": 1},
                  "%r" % COMP_MAP["C8-A"])
        case("comp_c8a_counts", comp_c8a_counts)

        def comp_c8d_one_priest():
            counts = COMP_MAP["C8-D"]
            check(counts["PHD"] == 1 and counts["PHB"] == 0,
                  "%r" % counts)
        case("comp_c8d_one_priest", comp_c8d_one_priest)

        def comp_c5_is_c4_plus_phb():
            expected = dict(COMP_MAP["C4"])
            expected["PHB"] = expected.get("PHB", 0) + 1
            check(COMP_MAP["C5"] == expected,
                  "%r vs %r" % (COMP_MAP["C5"], expected))
        case("comp_c5_is_c4_plus_phb", comp_c5_is_c4_plus_phb)

        def comp_req_pty_02_all_ok():
            for comp_id, counts in COMPOSITIONS:
                check(comp_rule(counts) == "ok",
                      "%s rule %s" % (comp_id, comp_rule(counts)))
        case("comp_req_pty_02_all_ok", comp_req_pty_02_all_ok)

        def comp_rule_violation_detected():
            counts = {"WP": 2, "WG": 1, "PHD": 2, "PHB": 1, "MF": 1, "MI": 0}
            check(comp_rule(counts) == "too_many_priests",
                  comp_rule(counts))
        case("comp_rule_violation_detected", comp_rule_violation_detected)

        def doc09_table_matches_builtin():
            path = doc09_path()
            if not os.path.isfile(path):
                raise AssertionError("docs/09 missing")
            with open(path, "r", encoding="utf-8") as handle:
                text = handle.read()
            token_re = re.compile(r"(\d+)?\s*(W-P|W-G|P-HD|P-HB|M-F|M-I)")
            found = {}
            for line in text.split("\n"):
                stripped = line.strip()
                for comp_id in ("C8-A", "C8-B", "C8-C", "C8-D"):
                    if stripped.startswith("| " + comp_id + " "):
                        counts = dict((p, 0) for p in PROFILES)
                        for match in token_re.finditer(line):
                            amount = int(match.group(1)) if match.group(1) else 1
                            counts[token_map[match.group(2)]] += amount
                        found[comp_id] = counts
            for comp_id in ("C8-A", "C8-B", "C8-C", "C8-D"):
                check(comp_id in found, "%s not found" % comp_id)
                check(found[comp_id] == COMP_MAP[comp_id],
                      "%s %r != %r" % (comp_id, found[comp_id],
                                       COMP_MAP[comp_id]))
        case("doc09_table_matches_builtin", doc09_table_matches_builtin)

        def doc09_small_teams_match_builtin():
            path = doc09_path()
            if not os.path.isfile(path):
                raise AssertionError("docs/09 missing")
            with open(path, "r", encoding="utf-8") as handle:
                text = handle.read()
            line = None
            for candidate in text.split("\n"):
                if "C2 (W-P" in candidate:
                    line = candidate
                    break
            check(line is not None, "small teams line not found")
            parsed = {}
            for comp_id in ("C2", "C3", "C4", "C5"):
                match = re.search(re.escape(comp_id) + r"\s*\((.*?)\)", line)
                check(match is not None, "%s not found" % comp_id)
                counts = dict((p, 0) for p in PROFILES)
                for part in match.group(1).split("+"):
                    part = part.strip()
                    if part in token_map:
                        counts[token_map[part]] += 1
                    elif part in parsed:
                        for profile in PROFILES:
                            counts[profile] += parsed[part][profile]
                    else:
                        raise AssertionError("unknown part %r" % part)
                parsed[comp_id] = counts
                check(counts == COMP_MAP[comp_id],
                      "%s %r != %r" % (comp_id, counts, COMP_MAP[comp_id]))
        case("doc09_small_teams_match_builtin",
             doc09_small_teams_match_builtin)

        # --- sets ----------------------------------------------------------
        def set_small_need():
            need = {p: max(COMP_MAP[m][p] for m in SET_MEMBERS["small"])
                    for p in PROFILES}
            check(need == {"WP": 1, "WG": 1, "PHD": 1, "PHB": 1, "MF": 1,
                           "MI": 0}, "%r" % need)
            check(sum(need.values()) == 5, "per_nation %d" % sum(need.values()))
        case("set_small_need", set_small_need)

        def set_min16_need():
            check(SET_NEEDS["min16"] == COMP_MAP["C8-A"],
                  "%r" % SET_NEEDS["min16"])
            check(sum(SET_NEEDS["min16"].values()) == 8, "per_nation")
        case("set_min16_need", set_min16_need)

        def set_full20_need():
            check(SET_NEEDS["full20"] == {"WP": 3, "WG": 1, "PHD": 1,
                                          "PHB": 1, "MF": 3, "MI": 1},
                  "%r" % SET_NEEDS["full20"])
            check(sum(SET_NEEDS["full20"].values()) == 10, "per_nation")
        case("set_full20_need", set_full20_need)

        def set_status(result, set_id):
            for entry in result["sets"]:
                if entry["id"] == set_id:
                    return entry
            raise AssertionError("set %s missing" % set_id)

        def have12_small_ok():
            _records, result = load_real()
            check(set_status(result, "small")["missing"] == 0, "small")
        case("have12_small_ok", have12_small_ok)

        def have12_min16_missing_4():
            _records, result = load_real()
            check(set_status(result, "min16")["missing"] == 4, "min16")
        case("have12_min16_missing_4", have12_min16_missing_4)

        def have12_full20_missing_8():
            _records, result = load_real()
            check(set_status(result, "full20")["missing"] == 8, "full20")
        case("have12_full20_missing_8", have12_full20_missing_8)

        def have16_rows():
            rows = []
            layout = (("WP", None), ("WP", 2), ("WG", None), ("PHD", None),
                      ("PHB", None), ("MF", None), ("MF", 2), ("MI", None))
            for nation, suffix in ((1, "K"), (2, "E")):
                for profile, index in layout:
                    number = str(index) if index else ""
                    rows.append(row(
                        profile,
                        "Bot%s%s_%s" % (profile, number, suffix),
                        "BotAcc%s%s%s" % (profile, number, suffix), nation))
            return rows

        def have16_min16_ok_full20_missing_4():
            result = result_of(have16_rows())
            check(set_status(result, "min16")["missing"] == 0, "min16")
            check(set_status(result, "full20")["missing"] == 4, "full20")
        case("have16_min16_ok_full20_missing_4",
             have16_min16_ok_full20_missing_4)

        def have20_all_ok():
            result = result_of(sample20_rows())
            for entry in result["sets"]:
                check(entry["missing"] == 0, "%s missing" % entry["id"])
        case("have20_all_ok", have20_all_ok)

        def have_surplus_not_negative():
            rows = []
            for nation, suffix in ((1, "K"), (2, "E")):
                for profile, index in (("WP", None), ("WG", None), ("WG", 2),
                                       ("WG", 3), ("PHD", None), ("PHB", None),
                                       ("MF", None), ("MI", None)):
                    number = str(index) if index else ""
                    rows.append(row(
                        profile,
                        "Bot%s%s_%s" % (profile, number, suffix),
                        "BotAcc%s%s%s" % (profile, number, suffix), nation))
            result = result_of(rows)
            small = set_status(result, "small")
            check(small["usable"] == small["total"], "%r" % small)
            check(small["missing"] == 0, "missing %d" % small["missing"])
        case("have_surplus_not_negative", have_surplus_not_negative)

        # --- missing list --------------------------------------------------
        def missing_real_db002_8_lines():
            _records, result = load_real()
            check(len(result["missing"]) == 8,
                  "missing %d != 8" % len(result["missing"]))
            rendered = render_text(result)
            for line in REAL_EXPECTED.split("\n"):
                if line.startswith("MISSING "):
                    check(line in rendered, "expected line missing: %r" % line)
        case("missing_real_db002_8_lines", missing_real_db002_8_lines)

        def missing_index_skips_used():
            rows = [row("WP", "BotWP_K", "BotAccWPK"),
                    row("WP", "BotWP3_K", "BotAccWP3K")]
            result = result_of(rows)
            wp_k = [m for m in result["missing"]
                    if m["nation"] == "K" and m["profile"] == "WP"]
            check(len(wp_k) == 1, "len %d" % len(wp_k))
            check(wp_k[0]["index"] == 2, "index %d" % wp_k[0]["index"])
        case("missing_index_skips_used", missing_index_skips_used)

        def missing_needed_by():
            _records, result = load_real()
            wp_k = {(m["index"]): m["needed_by"]
                    for m in result["missing"]
                    if m["nation"] == "K" and m["profile"] == "WP"}
            check(wp_k.get(2) == "min16", "%r" % wp_k)
            check(wp_k.get(3) == "full20", "%r" % wp_k)
        case("missing_needed_by", missing_needed_by)

        def missing_class_codes():
            _records, result = load_real()
            classes = {(m["nation"], m["profile"]): m["class"]
                       for m in result["missing"]}
            check(classes.get(("K", "WP")) == 106, "%r" % classes)
            check(classes.get(("E", "MF")) == 210, "%r" % classes)
        case("missing_class_codes", missing_class_codes)

        def missing_none_when_20():
            result = result_of(sample20_rows())
            check(result["missing"] == [], "%r" % result["missing"])
        case("missing_none_when_20", missing_none_when_20)

        # --- pairs ---------------------------------------------------------
        def pair_entry(result, pair_id):
            return [p for p in result["pairs"] if p["id"] == pair_id]

        def pair_small_ok():
            _records, result = load_real()
            two = pair_entry(result, "EVAL-2v2")[0]
            five = pair_entry(result, "EVAL-5v5")[0]
            check(two["online"] == 4 and two["status"] == "OK", "%r" % two)
            check(five["online"] == 10 and five["status"] == "OK",
                  "%r" % five)
        case("pair_small_ok", pair_small_ok)

        def pair_8v8_a_short_4():
            _records, result = load_real()
            entry = pair_entry(result, "EVAL-8v8-A")[0]
            check(entry["online"] == 16, "online %d" % entry["online"])
            check(entry["missing"] == 4, "missing %d" % entry["missing"])
            check(entry["status"] == "SHORT", entry["status"])
        case("pair_8v8_a_short_4", pair_8v8_a_short_4)

        def pair_mix_both_assignments():
            _records, result = load_real()
            entries = pair_entry(result, "EVAL-8v8-MIX")
            check(len(entries) == 2, "assignments %d" % len(entries))
            for entry in entries:
                check(entry["missing"] == 4, "%r" % entry)
        case("pair_mix_both_assignments", pair_mix_both_assignments)

        def pair_same_comp_single_line():
            _records, result = load_real()
            check(len(pair_entry(result, "EVAL-2v2")) == 1, "2v2 count")
            check(len(pair_entry(result, "EVAL-8v8-A")) == 1, "8v8-A count")
        case("pair_same_comp_single_line", pair_same_comp_single_line)

        def pair_over_max_bots():
            full = result_of(sample20_rows(), max_bots=12)
            entry = pair_entry(full, "EVAL-8v8-A")[0]
            check(entry["status"] == "OVER_MAX_BOTS", entry["status"])
            _records, real = load_real()
            short = pair_entry(real, "EVAL-8v8-A")[0]
            check(short["status"] == "SHORT", short["status"])
        case("pair_over_max_bots", pair_over_max_bots)

        # --- CLI -----------------------------------------------------------
        def cli_real_expected_output():
            rc, out, _err = run([])
            check(rc == 0, "rc %d" % rc)
            check(out == REAL_EXPECTED, "real output mismatch:\n%r" % out)
        case("cli_real_expected_output", cli_real_expected_output)

        def cli_json_keys():
            rc, out, _err = run(["--json"])
            check(rc == 0, "rc %d" % rc)
            data = json.loads(out)
            check(data["summary"]["full20_missing"] == 8,
                  "full20_missing %r" % data["summary"]["full20_missing"])
            check(len(data["missing"]) == 8, "missing len")
            check(data["have"]["K"]["WP"] == 1, "have K WP")
            check(data["errors"] == [], "errors %r" % data["errors"])
        case("cli_json_keys", cli_json_keys)

        def cli_strict_rc1_default_full20():
            rc, _out, _err = run(["--strict"])
            check(rc == 1, "rc %d" % rc)
        case("cli_strict_rc1_default_full20", cli_strict_rc1_default_full20)

        def cli_strict_target_small_rc0():
            rc, _out, _err = run(["--strict", "--target", "small"])
            check(rc == 0, "rc %d" % rc)
        case("cli_strict_target_small_rc0", cli_strict_target_small_rc0)

        def cli_strict_rc0_with_sample20():
            rc, _out, _err = run(["--strict", "--sql", sample_sql_path()])
            check(rc == 0, "rc %d" % rc)
        case("cli_strict_rc0_with_sample20", cli_strict_rc0_with_sample20)

        def cli_bad_target_rc2():
            rc, _out, _err = run(["--target", "bogus"])
            check(rc == 2, "rc %d" % rc)
        case("cli_bad_target_rc2", cli_bad_target_rc2)

        def cli_max_bots_zero_rc2():
            rc, _out, _err = run(["--max-bots", "0"])
            check(rc == 2, "rc %d" % rc)
        case("cli_max_bots_zero_rc2", cli_max_bots_zero_rc2)

        def cli_unknown_option_rc2():
            rc, _out, _err = run(["--nonsense"])
            check(rc == 2, "rc %d" % rc)
        case("cli_unknown_option_rc2", cli_unknown_option_rc2)

        def cli_default_path_independent_of_cwd():
            baseline_rc, baseline_out, _err = run([])
            cwd = os.getcwd()
            os.chdir(tmp)
            try:
                rc, out, _err = run([])
            finally:
                os.chdir(cwd)
            check(rc == baseline_rc, "rc %d" % rc)
            check(out == baseline_out, "output changed with cwd")
        case("cli_default_path_independent_of_cwd",
             cli_default_path_independent_of_cwd)

        def cli_errors_only_output():
            rc, out, _err = run_sql(
                make_sql([("XX", "BotWP_K", "BotAccWPK", 1, 1, 106)]))
            check(rc == 2, "rc %d" % rc)
            check("ERROR" in out, "no ERROR: %r" % out)
            check("SQL files" not in out and "HAVE" not in out
                  and "SUMMARY" not in out, "report printed: %r" % out)
        case("cli_errors_only_output", cli_errors_only_output)

        # --- sample file ---------------------------------------------------
        def sample_file_expected():
            path = sample_sql_path()
            if not os.path.isfile(path):
                raise AssertionError("sample file missing")
            rc, out, _err = run(["--sql", path])
            check(rc == 0, "rc %d" % rc)
            check(out == SAMPLE_EXPECTED, "sample output mismatch:\n%r" % out)
        case("sample_file_expected", sample_file_expected)

    failed = [item for item in results if not item[1]]
    for name, ok, detail in results:
        if ok:
            sys.stdout.write("PASS %s\n" % name)
        else:
            sys.stdout.write("FAIL %s: %s\n" % (name, detail))
    total = len(results)
    if failed:
        sys.stdout.write("SELFTEST FAIL failed=%d of %d\n"
                         % (len(failed), total))
        return 1
    sys.stdout.write("SELFTEST PASS n=%d\n" % total)
    return 0


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]
    if "--selftest" in argv:
        return run_selftest()
    code, out, err = run(argv)
    if out:
        sys.stdout.write(out)
    if err:
        sys.stderr.write(err)
    return code


if __name__ == "__main__":
    sys.exit(main())
