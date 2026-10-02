#!/usr/bin/env bash
# Reproducible navigation measurements (wall check, velocity cadence, arena/respawn path, per-tick path budget,
# stuck-detector false alarms) on the BotCore Nav*.h headers. Host g++ build, never the server.
#
#   tools/nav-measure.sh [section|all] [--navgrid PATH] [--seed N] [--n COUNT]
#
# Needs: g++ (C++17), python3, and a source tree that contains BotCore/Nav*.h (the navigation line,
# branch gece/2026-10-02-nav or any branch it was merged into). Point NAV_SRC_ROOT at such a tree when the
# current checkout does not have them yet. The grid file is exported with tools/nav-export.py when missing.
# Output: one "KEY value ..." line per result (see tools/nav-measure/nav_measure.cpp). Timings are host
# measurements, not the MSVC Release build.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_ROOT="${NAV_SRC_ROOT:-$ROOT}"
SECTION="${1:-all}"
shift || true

if [ ! -f "$SRC_ROOT/BotCore/NavGrid.h" ] || [ ! -f "$SRC_ROOT/BotCore/NavStuck.h" ]; then
	echo "nav-measure: $SRC_ROOT/BotCore/Nav*.h not found. Set NAV_SRC_ROOT to a tree with the navigation headers." >&2
	exit 2
fi

GRID="$ROOT/build/nav/zone71.navgrid"
if [ ! -f "$GRID" ]; then
	echo "nav-measure: exporting $GRID" >&2
	python3 "$ROOT/tools/nav-export.py" --out "$GRID" >&2 || python3 "$SRC_ROOT/tools/nav-export.py" --out "$GRID" >&2
fi

OUT_DIR="${NAV_MEASURE_OUT:-$ROOT/build/nav-measure}"
mkdir -p "$OUT_DIR"
BIN="$OUT_DIR/nav_measure"
g++ -std=c++17 -O2 -I"$SRC_ROOT" -o "$BIN" "$ROOT/tools/nav-measure/nav_measure.cpp"

exec "$BIN" "$SECTION" --navgrid "$GRID" "$@"
