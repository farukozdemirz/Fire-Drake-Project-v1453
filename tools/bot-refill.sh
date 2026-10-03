#!/usr/bin/env bash
# Refill the bag of the 12 bot characters through db/004_bot_inventory.sql.
# Writes USERDATA.strItem slots 14..21 only, with the game server stopped:
# a logged-in character writes its in-memory bag back on logout (ADR-0032-DEG,
# plan F4-40). The wrapper only forwards to sqlcmd; it does not change server
# behaviour.
# Usage:
#   tools/bot-refill.sh apply [--hp-pots N] [--mp-pots N] [--life-stones N]
#                             [--class-stones N] [--dry-run]
#   tools/bot-refill.sh rollback [--dry-run]
# Exit codes: 0 success, 1 refused/failed, 2 usage or configuration error.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SQLCMD="${SQLCMD:-/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE}"
FDP_SQL_INSTANCE="${FDP_SQL_INSTANCE:-.\\SQLEXPRESS}"
FDP_SQL_DB="${FDP_SQL_DB:-FDP_kn_online}"
if [ -z "${FDP_REFILL_STATUS_CMD:-}" ]; then
	FDP_REFILL_STATUS_CMD="$ROOT/tools/run-servers.sh status"
fi

apply_sql="$ROOT/db/004_bot_inventory.sql"
rollback_sql="$ROOT/db/004_bot_inventory_rollback.sql"

hp_pots=100
mp_pots=0
life_stones=30
class_stones=50

usage() {
	printf 'Usage: %s apply [--hp-pots N] [--mp-pots N] [--life-stones N] [--class-stones N] [--dry-run]\n' "$(basename "$0")"
	printf '       %s rollback [--dry-run]\n' "$(basename "$0")"
	printf '\n'
	printf 'Refills the bot bag (USERDATA.strItem slots 14..21); the game server must be stopped.\n'
	printf 'Defaults: --hp-pots 100 --mp-pots 0 --life-stones 30 --class-stones 50.\n'
	printf 'Counts are integers 0..9999; 0 leaves the slot empty. --dry-run prints the sqlcmd command.\n'
	printf '\n'
	printf 'Exit codes: 0 success, 1 refused/failed, 2 usage/config error\n'
}

err_usage() {
	printf 'Error: %s\n' "$1" >&2
	usage >&2
	exit 2
}

valid_count() {
	case "$1" in
		''|*[!0-9]*) return 1 ;;
	esac
	[ "$1" -ge 0 ] && [ "$1" -le 9999 ]
}

# Converts a WSL path to a Windows path for SQLCMD.EXE when possible.
winpath() {
	if command -v wslpath >/dev/null 2>&1; then
		wslpath -w "$1" 2>/dev/null || printf '%s' "$1"
	else
		printf '%s' "$1"
	fi
}

if [ "$#" -eq 0 ]; then
	err_usage 'no command given'
fi

case "$1" in
	-h|--help)
		usage
		exit 0
		;;
	apply|rollback)
		cmd="$1"
		;;
	*)
		err_usage "unknown command: $1"
		;;
esac
shift

dry_run=0
while [ "$#" -gt 0 ]; do
	arg="$1"
	case "$cmd:$arg" in
		apply:--hp-pots)
			shift
			[ "$#" -gt 0 ] || err_usage '--hp-pots needs a value'
			valid_count "$1" || err_usage "--hp-pots must be an integer 0..9999: $1"
			hp_pots="$1"
			;;
		apply:--mp-pots)
			shift
			[ "$#" -gt 0 ] || err_usage '--mp-pots needs a value'
			valid_count "$1" || err_usage "--mp-pots must be an integer 0..9999: $1"
			mp_pots="$1"
			;;
		apply:--life-stones)
			shift
			[ "$#" -gt 0 ] || err_usage '--life-stones needs a value'
			valid_count "$1" || err_usage "--life-stones must be an integer 0..9999: $1"
			life_stones="$1"
			;;
		apply:--class-stones)
			shift
			[ "$#" -gt 0 ] || err_usage '--class-stones needs a value'
			valid_count "$1" || err_usage "--class-stones must be an integer 0..9999: $1"
			class_stones="$1"
			;;
		apply:--dry-run|rollback:--dry-run)
			dry_run=1
			;;
		*:-h|*:--help)
			usage
			exit 0
			;;
		*)
			err_usage "unknown option for $cmd: $arg"
			;;
	esac
	shift
done

if [ "$cmd" = "apply" ]; then
	sql_file="$(winpath "$apply_sql")"
	sql_args=(-S "$FDP_SQL_INSTANCE" -E -d "$FDP_SQL_DB" -b
		-v "HpPots=$hp_pots" -v "MpPots=$mp_pots"
		-v "LifeStones=$life_stones" -v "ClassStones=$class_stones"
		-i "$sql_file")
else
	sql_file="$(winpath "$rollback_sql")"
	sql_args=(-S "$FDP_SQL_INSTANCE" -E -d "$FDP_SQL_DB" -b -i "$sql_file")
fi

if [ "$dry_run" -eq 1 ]; then
	printf 'dry-run: %s' "$SQLCMD"
	printf ' %q' "${sql_args[@]}"
	printf '\n'
	exit 0
fi

# Refuse while any server of this working tree is not down.
status_out="$(eval "$FDP_REFILL_STATUS_CMD" 2>/dev/null || true)"
if printf '%s\n' "$status_out" | grep -Eq '^\[(UP|PARTIAL|STARTING|FAILED)\]'; then
	printf 'Error: servers are running, stop them first\n' >&2
	exit 1
fi

rc=0
"$SQLCMD" "${sql_args[@]}" || rc=$?
exit "$rc"
