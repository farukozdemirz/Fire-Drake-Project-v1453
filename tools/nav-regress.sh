#!/usr/bin/env bash
# Persistent navigation regression wrapper (plan F5-11).
# Runs tools/nav-regress.py, the evaluator that reads tools/nav-measure.sh output
# and checks it against docs/12 s11/s13 and the F5 plan thresholds.
#
#   tools/nav-regress.sh [--n N] [--seed S] [--skip-timing] [--timing-retries K]
#                        [--from-file F] [--save F] [--list] [--selftest]
#
# Exit codes: 0 all checks passed, 1 at least one FAIL, 2 environment / usage error.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v python3 >/dev/null 2>&1; then
	echo "nav-regress: python3 not found" >&2
	exit 2
fi

exec python3 "$ROOT/tools/nav-regress.py" "$@"
