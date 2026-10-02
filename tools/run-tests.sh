#!/usr/bin/env bash
# Builds (unless --no-build) and runs the BotCoreTests unit tests from WSL.
# The test executable is a Win32 console app; run-tests.sh invokes it directly.
# Usage: tools/run-tests.sh [Release|Debug] [--no-build] [test-args...]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

usage() {
	printf 'Usage: %s [Release|Debug] [--no-build] [test-args...]\n' "$(basename "$0")"
}

CONFIG=Release
BUILD=1
FIRST=1

while [ "$#" -gt 0 ]; do
	case "$1" in
		Release|Debug)
			CONFIG="$1"
			;;
		--no-build)
			BUILD=0
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			if [ "$FIRST" -eq 1 ]; then
				usage >&2
				exit 2
			fi
			break
			;;
	esac
	FIRST=0
	shift
done

ARGS=("$@")

if [ "$BUILD" -eq 1 ]; then
	"$ROOT/tools/build.sh" "$CONFIG"
fi

EXE="$ROOT/build/bin/x86-$CONFIG/Tests/BotCoreTests.exe"
if [ ! -f "$EXE" ]; then
	printf 'BotCoreTests.exe not found: %s (build first)\n' "$EXE" >&2
	exit 2
fi

rc=0
"$EXE" ${ARGS[@]+"${ARGS[@]}"} || rc=$?
exit "$rc"
