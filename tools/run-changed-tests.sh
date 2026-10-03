#!/usr/bin/env bash
# Runs only the unit tests that a plan added or changed (ADR-0025: Debug configuration).
# "Changed" is file based: every TEST_CASE of every Tests/BotCoreTests/*.cpp file touched
# by <base>...HEAD is run, plus any TEST_CASE added anywhere in the diff.
# Usage: tools/run-changed-tests.sh [Release|Debug] <base-ref> [--list]
# Output: "N tests, M failed" (same shape as run-tests.sh). Exit code 1 if any test failed.
# The build is NOT done here: run tools/build.sh <config> first.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2
CONFIG="${1:-Debug}"
BASE="${2:-}"
LISTONLY=false
[ "${3:-}" = "--list" ] && LISTONLY=true
case "$CONFIG" in Release | Debug) ;; *)
	echo "usage: tools/run-changed-tests.sh [Release|Debug] <base-ref> [--list]" >&2
	exit 2
	;;
esac
[ -n "$BASE" ] || { echo "usage: tools/run-changed-tests.sh [Release|Debug] <base-ref> [--list]" >&2; exit 2; }

EXE="build/bin/x86-$CONFIG/Tests/BotCoreTests.exe"
[ -f "$EXE" ] || { echo "BotCoreTests.exe not found: $EXE (build first)" >&2; exit 2; }

names="$(
	{
		git diff --name-only --diff-filter=AM "$BASE"...HEAD -- 'Tests/BotCoreTests/*.cpp' | while read -r f; do
			[ -f "$f" ] && grep -o -E 'TEST_CASE\("[^"]+"' "$f" | sed -E 's/TEST_CASE\("//; s/"$//'
		done
		git diff -U0 "$BASE"...HEAD -- Tests | grep -E '^\+.*TEST_CASE\(' | grep -o -E 'TEST_CASE\("[^"]+"' | sed -E 's/TEST_CASE\("//; s/"$//'
	} | sort -u
)"

if [ -z "$names" ]; then
	echo "0 tests, 0 failed (no test file touched by $BASE...HEAD)"
	exit 0
fi
if $LISTONLY; then
	echo "$names"
	exit 0
fi

total=0
failed=0
failed_names=""
while read -r name; do
	[ -z "$name" ] && continue
	total=$((total + 1))
	out="$("$EXE" "$name" </dev/null 2>&1)"
	if ! echo "$out" | tr -d '\r' | grep -q -E '^1 tests, 0 failed'; then
		failed=$((failed + 1))
		failed_names="$failed_names $name"
		echo "$out" | tr -d '\r' | tail -n 3
	fi
done <<<"$names"

echo "$total tests, $failed failed (changed-test selection, $CONFIG, base $BASE)"
[ -n "$failed_names" ] && echo "failed:$failed_names"
[ "$failed" -eq 0 ]
