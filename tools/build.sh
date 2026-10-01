#!/usr/bin/env bash
# Builds KnightOnlineServer.sln (Win32, MSVC v143) from WSL.
# Usage: tools/build.sh [Release|Debug] [--packet-trace]
set -euo pipefail

CONFIG="${1:-Release}"
EXTRA=()
if [ "${2:-}" = "--packet-trace" ]; then
	EXTRA+=("/p:FdpPacketTrace=1")
fi
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MSBUILD="${MSBUILD:-/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe}"

if [ ! -f "$MSBUILD" ]; then
	echo "MSBuild.exe not found: $MSBUILD (set MSBUILD to override)" >&2
	exit 2
fi

SLN="$(wslpath -w "$ROOT/KnightOnlineServer.sln")"
"$MSBUILD" "$SLN" /p:Configuration="$CONFIG" /p:Platform=Win32 /p:PlatformToolset=v143 /m /nologo /v:minimal "${EXTRA[@]+"${EXTRA[@]}"}"
