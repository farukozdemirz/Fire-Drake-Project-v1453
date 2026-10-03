#!/usr/bin/env bash
# Mechanical evidence for plan verification (no LLM, deterministic).
# Writes plans/_logs/evidence/<plan>-generic.md: diff summary, encodings, build/test results.
# The auditor (/plan-dogrula) treats this file as a lead, never as proof: it re-runs the key claims.
# Usage: tools/verify-evidence.sh <plan.md> <base-ref> [--no-build]
# Run it on the plan branch with the servers stopped. Exit code is always 0 unless usage is wrong.
set -uo pipefail

ROOT="${VERIFY_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$ROOT" || exit 2
PLAN="${1:-}"
BASE="${2:-}"
NOBUILD=false
[ "${3:-}" = "--no-build" ] && NOBUILD=true
if [ -z "$PLAN" ] || [ -z "$BASE" ]; then
	echo "usage: tools/verify-evidence.sh <plan.md> <base-ref> [--no-build]" >&2
	exit 2
fi

NAME="$(basename "$PLAN" .md)"
OUT_DIR="plans/_logs/evidence"
OUT="$OUT_DIR/$NAME-generic.md"
mkdir -p "$OUT_DIR"
HEAD_SHA="$(git rev-parse HEAD)"
DIRTY="$(git status --porcelain | grep -v '^?? ' | head -n 5)"

{
	echo "# Generic evidence: $NAME"
	echo
	echo "- head: $HEAD_SHA ($(git branch --show-current))"
	echo "- base: $BASE ($(git rev-parse --short "$BASE" 2>/dev/null))"
	echo "- generated: $(date '+%Y-%m-%d %H:%M:%S')"
	echo "- tracked changes not committed: ${DIRTY:-none}"
	echo
	echo "## Diff stat (base...head)"
	echo '```'
	git diff --stat "$BASE"...HEAD | tail -n 40
	echo '```'
	echo
	echo "## git diff --check"
	echo '```'
	git diff --check "$BASE"...HEAD | head -n 20
	echo "rc=${PIPESTATUS[0]}"
	echo '```'
	echo
	echo "## Encodings of changed files"
	echo '```'
	git diff --name-only --diff-filter=AM "$BASE"...HEAD | while read -r f; do
		[ -f "$f" ] && case "$f" in
		*.h | *.cpp | *.py | *.sh | *.vcxproj | *.filters | *.sql | *.txt | *.yaml) file -b "$f" | sed "s|^|$f: |" ;;
		esac
	done
	echo '```'
	echo
	echo "## Added lines with patterns that need a second look (BotCore/ and Tests/)"
	echo '```'
	git diff -U0 "$BASE"...HEAD -- BotCore Tests | grep -E '^\+[^+]' |
		grep -n -E 'windows\.h|stdafx|GameServer|shared/|[^_a-zA-Z]static |malloc|std::chrono|0\.625|TODO|FIXME' | head -n 30
	echo '```'
	echo
	echo "## New test cases"
	echo '```'
	git diff -U0 "$BASE"...HEAD -- Tests | grep -E '^\+.*TEST_CASE\(' | sed -E 's/^\+\s*//' | head -n 60
	echo '```'
} >"$OUT"

summ() { # $1 = label, $2 = log
	{
		echo "- $1 rc=$(grep -m1 '^EVIDENCE_RC=' "$2" | cut -d= -f2)"
		echo "  warnings: $(grep -c -i -E ' warning ' "$2")"
		grep -i -E ' warning ' "$2" | sed -E 's/\[[^]]*\]$//' | sort -u | head -n 8 | sed 's/^/  /'
	} >>"$OUT"
}

run_step() { # $1 = label, $2 = log, rest = command
	local label="$1" log="$2"
	shift 2
	"$@" >"$log" 2>&1
	echo "EVIDENCE_RC=$?" >>"$log"
}

if $NOBUILD; then
	{
		echo
		echo "## Build and tests"
		echo "(skipped: --no-build)"
	} >>"$OUT"
	echo "$OUT"
	exit 0
fi

# Force a recompile of the changed sources so warnings cannot hide behind incremental builds.
git diff --name-only --diff-filter=AM "$BASE"...HEAD | while read -r f; do
	case "$f" in *.h | *.cpp) [ -f "$f" ] && touch "$f" ;; esac
done

TMP="$OUT_DIR/.tmp-$NAME"
mkdir -p "$TMP"
echo >>"$OUT"
echo "## Build" >>"$OUT"
run_step build-release "$TMP/build-release.log" ./tools/build.sh Release
summ "build Release" "$TMP/build-release.log"
run_step build-debug "$TMP/build-debug.log" ./tools/build.sh Debug
summ "build Debug" "$TMP/build-debug.log"

echo >>"$OUT"
echo "## Tests (--no-build)" >>"$OUT"
for cfg in Release Debug; do
	lc="$(echo "$cfg" | tr 'A-Z' 'a-z')"
	run_step "tests-$lc" "$TMP/tests-$lc.log" ./tools/run-tests.sh "$cfg" --no-build
	{
		echo "- tests $cfg rc=$(grep -m1 '^EVIDENCE_RC=' "$TMP/tests-$lc.log" | cut -d= -f2): $(grep -E '[0-9]+ tests, [0-9]+ failed' "$TMP/tests-$lc.log" | tail -n 1)"
		grep -E '^\[ *FAIL|FAILED' "$TMP/tests-$lc.log" | head -n 10 | sed 's/^/  /'
	} >>"$OUT"
done

{
	echo
	echo "## Other checks"
	if [ -f tools/check-perception-contract.py ]; then
		python3 tools/check-perception-contract.py >"$TMP/perception.log" 2>&1
		echo "- check-perception-contract.py rc=$? $(grep -m1 'RESULT' "$TMP/perception.log")"
	fi
	TESTEXE="build/bin/x86-Release/Tests/BotCoreTests.exe"
	[ -x "$TESTEXE" ] && echo "- BotCoreTests --list count: $("$TESTEXE" --list 2>/dev/null | wc -l)"
	echo
	echo "Raw logs: $TMP/ (build/tests, not committed)."
} >>"$OUT"

echo "$OUT"
exit 0
