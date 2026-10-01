#!/usr/bin/env bash
# Timing-session helper for the FDP_PACKET_TRACE packet tracer.
# Prepares a tracing server, collects per-scenario traces from the GameServer
# log, shows the session state and restores the normal build afterwards.
# Usage:
#   tools/trace-session.sh prepare
#   tools/trace-session.sh collect <label>
#   tools/trace-session.sh status
#   tools/trace-session.sh finish [--force]
# Exit codes: 0 success, 1 failed/refused, 2 usage error.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FDP_RUNTIME_DIR="${FDP_RUNTIME_DIR:-/mnt/c/dev/fdp}"
TRACE_OUT_DIR="${TRACE_OUT_DIR:-$ROOT/plans/_logs/trace}"
LOG_DIR="$FDP_RUNTIME_DIR/server/Logs"
SESSION_FILE="$TRACE_OUT_DIR/.session"

# Turkish output strings are escaped so this file stays ASCII (AGENTS.md section 3).
T_STOP_FAIL=$'sunucular durdurulamad\u0131'
T_BUILD_FAIL=$'derleme ba\u015far\u0131s\u0131z'
T_START_FAIL=$'sunucular ba\u015flat\u0131lamad\u0131'
T_NO_SESSION=$'\u00f6nce prepare \u00e7al\u0131\u015ft\u0131r\u0131n (.session yok)'
T_LABEL_EXISTS=$'bu etiket zaten var; \u00fczerine yaz\u0131lmaz'
T_NO_NEW=$'yeni kayit yok'
T_BAD_LABEL=$'ge\u00e7ersiz etiket (1-40 karakter: A-Z a-z 0-9 _ -)'
T_PY_FAIL=$'\u00f6zet beti\u011fi ba\u015far\u0131s\u0131z'
T_PREP1=$'izleyicili sunucular ayakta; istemciyle oyuna girin'
T_PREP2=$'her senaryodan sonra: tools/trace-session.sh collect <etiket>'
T_PREP3=$'bitince: tools/trace-session.sh finish'
T_PREP4=$'uyar\u0131: build/bin/x86-Release/Server/ \u015fu an izleyicili s\u00fcr\u00fcm; finish normal s\u00fcr\u00fcme d\u00f6nd\u00fcr\u00fcr'
T_RECORDS=$'kay\u0131tlar burada (karakter ad\u0131 i\u00e7erir; payla\u015fmay\u0131n, git\'e eklemeyin)'
T_START_REMIND=$'normal sunucular\u0131 a\u00e7mak i\u00e7in: tools/run-servers.sh start'

usage() {
	printf 'Usage: %s <prepare|collect|status|finish> [options]\n' "$(basename "$0")"
	printf '\n'
	printf 'Commands:\n'
	printf '  prepare            stop servers, build with packet tracing, start servers\n'
	printf '  collect <label>    save the new trace bytes as <label>.log and summarise\n'
	printf '  status             show session offsets, collected files and server status\n'
	printf '  finish [--force]   stop servers, rebuild without tracing, drop the session\n'
	printf '\n'
	printf 'Options:\n'
	printf '  --force            passed to run-servers.sh stop (used with finish)\n'
	printf '  -h, --help         show this help\n'
	printf '\n'
	printf 'Exit codes: 0 success, 1 failed/refused, 2 usage/config error\n'
}

err_usage() {
	printf 'Error: %s\n' "$1" >&2
	usage >&2
	exit 2
}

# Rewrites .session with the current size of every PacketTrace log file.
write_session() {
	mkdir -p "$TRACE_OUT_DIR"
	: > "$SESSION_FILE"
	local f name size
	for f in "$LOG_DIR"/PacketTrace_*.log; do
		[ -f "$f" ] || continue
		name="$(basename "$f")"
		size="$(stat -c %s "$f")"
		printf '%s\t%s\n' "$name" "$size" >> "$SESSION_FILE"
	done
}

# Prints the recorded size for a log file name, or 0.
session_offset() {
	local name="$1"
	[ -f "$SESSION_FILE" ] || { printf '0'; return 0; }
	awk -F '\t' -v n="$name" '$1 == n { print $2; found = 1 } END { if (!found) print 0 }' "$SESSION_FILE"
}

# Stops servers, builds with the tracer and starts the servers again.
cmd_prepare() {
	if ! "$ROOT/tools/run-servers.sh" stop; then
		printf '%s\n' "$T_STOP_FAIL" >&2
		exit 1
	fi
	if ! "$ROOT/tools/build.sh" Release --packet-trace; then
		printf '%s\n' "$T_BUILD_FAIL" >&2
		exit 1
	fi
	if ! "$ROOT/tools/run-servers.sh" start --config Release; then
		printf '%s\n' "$T_START_FAIL" >&2
		exit 1
	fi
	mkdir -p "$TRACE_OUT_DIR"
	write_session
	printf '%s\n' "$T_PREP1"
	printf '%s\n' "$T_PREP2"
	printf '%s\n' "$T_PREP3"
	printf '%s\n' "$T_PREP4"
}

# Appends the new bytes of every trace log to <label>.log and summarises it.
cmd_collect() {
	local label="$1"
	[ -f "$SESSION_FILE" ] || { printf '%s\n' "$T_NO_SESSION" >&2; exit 1; }
	if [ -e "$TRACE_OUT_DIR/$label.log" ]; then
		printf '%s\n' "$T_LABEL_EXISTS" >&2
		exit 1
	fi
	mkdir -p "$TRACE_OUT_DIR"

	local f name off size files
	files="$(ls -1tr "$LOG_DIR"/PacketTrace_*.log 2>/dev/null || true)"
	while IFS= read -r f; do
		[ -n "$f" ] || continue
		[ -f "$f" ] || continue
		name="$(basename "$f")"
		off="$(session_offset "$name")"
		size="$(stat -c %s "$f")"
		if [ "$size" -lt "$off" ]; then
			off=0
		fi
		if [ "$size" -gt "$off" ]; then
			tail -c "+$((off + 1))" "$f" >> "$TRACE_OUT_DIR/$label.log"
		fi
	done <<< "$files"

	if [ ! -s "$TRACE_OUT_DIR/$label.log" ]; then
		rm -f "$TRACE_OUT_DIR/$label.log"
		printf '%s\n' "$T_NO_NEW" >&2
		exit 1
	fi

	write_session

	if ! python3 "$ROOT/tools/packet-trace-summary.py" "$TRACE_OUT_DIR/$label.log" --cli | tee "$TRACE_OUT_DIR/$label.summary.txt"; then
		printf '%s\n' "$T_PY_FAIL" >&2
		exit 1
	fi
}

# Shows the session offsets, collected files and the server status.
cmd_status() {
	local name size current new f
	if [ -f "$SESSION_FILE" ]; then
		while IFS=$'\t' read -r name size; do
			[ -n "$name" ] || continue
			current=0
			if [ -f "$LOG_DIR/$name" ]; then
				current="$(stat -c %s "$LOG_DIR/$name")"
			fi
			new=$((current - size))
			if [ "$new" -lt 0 ]; then
				new=0
			fi
			printf '%s: kayitli=%s guncel=%s yeni=%s\n' "$name" "$size" "$current" "$new"
		done < "$SESSION_FILE"
	else
		printf '%s\n' "$T_NO_SESSION"
	fi

	printf '%s:\n' "$TRACE_OUT_DIR"
	for f in "$TRACE_OUT_DIR"/*.log; do
		[ -f "$f" ] || continue
		printf '  %s %s\n' "$(basename "$f")" "$(stat -c %s "$f")"
	done

	"$ROOT/tools/run-servers.sh" status || true
}

# Stops servers, rebuilds without tracing and drops the session state.
cmd_finish() {
	local force="$1"
	if [ "$force" -eq 1 ]; then
		if ! "$ROOT/tools/run-servers.sh" stop --force; then
			printf '%s\n' "$T_STOP_FAIL" >&2
			exit 1
		fi
	else
		if ! "$ROOT/tools/run-servers.sh" stop; then
			printf '%s\n' "$T_STOP_FAIL" >&2
			exit 1
		fi
	fi
	if ! "$ROOT/tools/build.sh" Release; then
		printf '%s\n' "$T_BUILD_FAIL" >&2
		exit 1
	fi
	rm -f "$SESSION_FILE"
	printf '%s:\n' "$T_RECORDS"
	printf '  %s\n' "$TRACE_OUT_DIR"
	printf '%s\n' "$T_START_REMIND"
}

if [ "$#" -eq 0 ]; then
	err_usage 'no command given'
fi

CMD="$1"
case "$CMD" in
	prepare)
		[ "$#" -eq 1 ] || err_usage "unexpected argument for prepare: $2"
		cmd_prepare
		;;
	collect)
		[ "$#" -eq 2 ] || err_usage 'collect needs exactly one label'
		LABEL="$2"
		if [[ ! "$LABEL" =~ ^[A-Za-z0-9_-]{1,40}$ ]]; then
			printf 'Error: %s\n' "$T_BAD_LABEL" >&2
			exit 2
		fi
		cmd_collect "$LABEL"
		;;
	status)
		[ "$#" -eq 1 ] || err_usage "unexpected argument for status: $2"
		cmd_status
		;;
	finish)
		FORCE=0
		if [ "$#" -eq 2 ]; then
			[ "$2" = "--force" ] || err_usage "unknown option for finish: $2"
			FORCE=1
		elif [ "$#" -gt 2 ]; then
			err_usage "unexpected argument for finish: $2"
		fi
		cmd_finish "$FORCE"
		;;
	-h|--help)
		usage
		exit 0
		;;
	*)
		err_usage "unknown command: $CMD"
		;;
esac
