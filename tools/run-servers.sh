#!/usr/bin/env bash
# Start/stop/status helper for the Fire Drake Project v1453 servers.
# Servers are Windows (Win32) processes; they are managed through powershell.exe
# and taskkill.exe from WSL. Only processes whose executable path matches an
# allowed folder are ever touched (never the unrelated GameServer.exe trap).
# Usage:
#   tools/run-servers.sh start  [--config Release|Debug] [--keep-on-fail]
#   tools/run-servers.sh stop   [--force]
#   tools/run-servers.sh status
# Exit codes: 0 success, 1 failed/refused, 2 usage or configuration error.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FDP_RUNTIME_DIR="${FDP_RUNTIME_DIR:-/mnt/c/dev/fdp}"
FDP_SERVER_BIN_DIR="${FDP_SERVER_BIN_DIR:-}"
FDP_START_TIMEOUT="${FDP_START_TIMEOUT:-300}"
FDP_STOP_TIMEOUT="${FDP_STOP_TIMEOUT:-20}"

# Turkish output strings are escaped so this file stays ASCII (AGENTS.md section 3).
T_OZET=$'\u00d6zet'
T_HAZIR=$'haz\u0131r'
T_BAGLI=$'ba\u011fl\u0131'
T_BAGLI_DEGIL=$'ba\u011fl\u0131 de\u011fil'
T_ISTEMCI=$'istemci'
T_YOK_SAYILDI=$'yok say\u0131ld\u0131'
T_YOL_OKUNAMADI=$'yol okunamad\u0131'
T_IZIN_DISI=$'izinli klas\u00f6r d\u0131\u015f\u0131nda'
T_BASLATILIYOR=$'ba\u015flat\u0131l\u0131yor'
T_BEKLENIYOR=$'bekleniyor'
T_ACILIS_BASARISIZ=$'a\u00e7\u0131l\u0131\u015f ba\u015far\u0131s\u0131z (sunucu "pause" bekliyor; ayr\u0131nt\u0131 konsol penceresinde)'
T_SUREC_KAPANDI=$'s\u00fcre\u00e7 kapand\u0131'
T_HAZIR_OLMADI=$'i\u00e7inde haz\u0131r olmad\u0131'
T_SON_DURUM=$'son durum'
T_KEEP_MSG=$'inceleme i\u00e7in a\u00e7\u0131k b\u0131rak\u0131ld\u0131; kapatmak i\u00e7in: tools/run-servers.sh stop'
T_CALISAN_YOK=$'\u00e7al\u0131\u015fan sunucu yok'
T_CALISIYOR=$'zaten \u00e7al\u0131\u015f\u0131yor'
T_ONCE_STOP=$'\u00f6nce durdurun: tools/run-servers.sh stop'
T_REFUSE_CLIENTS=$'GameServer\'a ba\u011fl\u0131 istemci var; durdurmak i\u00e7in --force'
T_KALAN=$'kalan'
T_DURDURULAMADI=$'durdurulamad\u0131'
T_PORT_AYNI_DEGIL=$'AIServer ve GameServer ayn\u0131 AI portunu kullanmal\u0131'
T_GECERSIZ_PORT=$'ge\u00e7ersiz port de\u011feri'
T_EXE_YOK=$'exe klas\u00f6r\u00fc yok'
T_EKSIK_EXE=$'eksik exe'
T_DIZIN_YOK=$'sunucu \u00e7al\u0131\u015fma dizini yok'
T_EKSIK_INI=$'eksik ini'
T_CALISMA_DIZINI=$'\u00e7al\u0131\u015fma dizini'
T_PS_YOK=$'powershell.exe bulunamad\u0131'

usage() {
	printf 'Usage: %s <start|stop|status> [options]\n' "$(basename "$0")"
	printf '\n'
	printf 'Commands:\n'
	printf '  start [--config Release|Debug] [--keep-on-fail]  start servers in order\n'
	printf '  stop  [--force]                                  stop servers in reverse\n'
	printf '  status                                           show server status\n'
	printf '\n'
	printf 'Options:\n'
	printf '  --config C       build config used by start (default Release)\n'
	printf '  --keep-on-fail   leave started servers open if start fails\n'
	printf '  --force          stop even if clients are connected\n'
	printf '  -h, --help       show this help\n'
	printf '\n'
	printf 'Exit codes: 0 success, 1 failed/refused, 2 usage/config error\n'
}

err_usage() {
	printf 'Error: %s\n' "$1" >&2
	usage >&2
	exit 2
}

# Runs a PowerShell snippet without quoting issues; prints stdout without CR.
ps_run() {
	local script enc out
	script="\$ProgressPreference='SilentlyContinue'; \$ErrorActionPreference='SilentlyContinue'; $1"
	enc="$(printf '%s' "$script" | iconv -f utf-8 -t utf-16le | base64 -w0)"
	out="$(powershell.exe -NoProfile -NonInteractive -EncodedCommand "$enc" 2>/dev/null | tr -d '\r')" || true
	printf '%s\n' "$out"
}

# Single-quotes a string for embedding in a PowerShell script.
psq() {
	printf '%s' "${1//\'/\'\'}"
}

# Converts a WSL path to a Windows path.
winpath() {
	wslpath -w "$1" 2>/dev/null || true
}

# Extracts the directory part of a Windows executable path.
exe_dir() {
	printf '%s' "${1%\\*}"
}

# Prints pid|name|exe-path for the three server process names.
ps_processes() {
	ps_run "Get-CimInstance Win32_Process -Filter \"Name='AIServer.exe' OR Name='GameServer.exe' OR Name='LogInServer.exe'\" | ForEach-Object { '{0}|{1}|{2}' -f \$_.ProcessId, \$_.Name, \$_.ExecutablePath }"
}

# Fills ALLOWED_DIRS with the Windows form of every folder a server may run from.
make_allowed_dirs() {
	ALLOWED_DIRS=()
	local d w
	for d in "$FDP_RUNTIME_DIR/server" "$ROOT/build/bin/x86-Release/Server" "$ROOT/build/bin/x86-Debug/Server"; do
		if [ -d "$d" ]; then
			w="$(winpath "$d")"
			if [ -n "$w" ]; then
				ALLOWED_DIRS+=("$w")
			fi
		fi
	done
	if [ -n "$FDP_SERVER_BIN_DIR" ] && [ -d "$FDP_SERVER_BIN_DIR" ]; then
		w="$(winpath "$FDP_SERVER_BIN_DIR")"
		if [ -n "$w" ]; then
			ALLOWED_DIRS+=("$w")
		fi
	fi
	return 0
}

is_allowed_name() {
	case "$1" in
		AIServer.exe|GameServer.exe|LogInServer.exe) return 0 ;;
		*) return 1 ;;
	esac
}

is_allowed_dir() {
	local dir="$1" lc a
	[ -n "$dir" ] || return 1
	lc="$(printf '%s' "$dir" | tr '[:upper:]' '[:lower:]')"
	for a in "${ALLOWED_DIRS[@]:-}"; do
		if [ "$lc" = "$(printf '%s' "$a" | tr '[:upper:]' '[:lower:]')" ]; then
			return 0
		fi
	done
	return 1
}

# Fills OUR_PIDS/OUR_NAMES/OUR_PATHS and OTHER_LINES (ignored processes).
collect_our_processes() {
	OUR_PIDS=()
	OUR_NAMES=()
	OUR_PATHS=()
	OTHER_LINES=()
	local pid name path
	while IFS='|' read -r pid name path; do
		[ -n "$pid" ] || continue
		if [ -n "$path" ] && is_allowed_name "$name" && is_allowed_dir "$(exe_dir "$path")"; then
			OUR_PIDS+=("$pid")
			OUR_NAMES+=("$name")
			OUR_PATHS+=("$path")
		else
			OTHER_LINES+=("$pid|$name|$path")
		fi
	done < <(ps_processes)
	return 0
}

# Prints the PORT value of [section] in an ini file, or nothing.
ini_port() {
	tr -d '\r' < "$1" | awk -v sec="[$2]" '
		/^[[:space:]]*\[/ { in_sec = ($0 == sec); next }
		in_sec && /^[[:space:]]*PORT[[:space:]]*=/ { sub(/^[^=]*=[[:space:]]*/, ""); gsub(/[[:space:]]+$/, ""); print; exit }'
}

# Reads all ports with defaults (10020 / 15001 / 15100).
read_ports() {
	AI_PORT="$(ini_port "$FDP_RUNTIME_DIR/server/AIServer.ini" SETTINGS 2>/dev/null || true)"
	GAME_PORT="$(ini_port "$FDP_RUNTIME_DIR/server/GameServer.ini" SETTINGS 2>/dev/null || true)"
	GAME_AIPORT="$(ini_port "$FDP_RUNTIME_DIR/server/GameServer.ini" AI_SERVER 2>/dev/null || true)"
	LOGIN_PORT="$(ini_port "$FDP_RUNTIME_DIR/server/LogInServer.ini" SETTINGS 2>/dev/null || true)"
	[ -n "$AI_PORT" ] || AI_PORT=10020
	[ -n "$GAME_PORT" ] || GAME_PORT=15001
	[ -n "$GAME_AIPORT" ] || GAME_AIPORT=10020
	[ -n "$LOGIN_PORT" ] || LOGIN_PORT=15100
	return 0
}

# True if a port value is an integer in 1..65535.
port_valid() {
	case "$1" in
		''|*[!0-9]*) return 1 ;;
	esac
	[ "$1" -ge 1 ] && [ "$1" -le 65535 ]
}

# Prints one status line for a single process.
proc_status() {
	local pid="$1" port="$2" aiport="$3"
	ps_run "\$p = Get-Process -Id $pid -ErrorAction SilentlyContinue; \$alive = [int](\$null -ne \$p); \$pause = @(Get-CimInstance Win32_Process -Filter \"ParentProcessId=$pid AND Name='cmd.exe'\").Count; \$listen = @(Get-NetTCPConnection -State Listen -LocalPort $port -OwningProcess $pid -ErrorAction SilentlyContinue).Count; \$ai = 0; if ($aiport -gt 0) { \$ai = @(Get-NetTCPConnection -State Established -RemotePort $aiport -OwningProcess $pid -ErrorAction SilentlyContinue).Count }; \$cli = @(Get-NetTCPConnection -State Established -LocalPort $port -OwningProcess $pid -ErrorAction SilentlyContinue).Count; \"alive=\$alive pause=\$pause listen=\$listen ai=\$ai clients=\$cli\""
}

# Parses a proc_status line into S_ALIVE/S_PAUSE/S_LISTEN/S_AI/S_CLIENTS.
parse_status_line() {
	S_ALIVE=0
	S_PAUSE=0
	S_LISTEN=0
	S_AI=0
	S_CLIENTS=0
	local kv
	for kv in $1; do
		case "$kv" in
			alive=*) S_ALIVE="${kv#alive=}" ;;
			pause=*) S_PAUSE="${kv#pause=}" ;;
			listen=*) S_LISTEN="${kv#listen=}" ;;
			ai=*) S_AI="${kv#ai=}" ;;
			clients=*) S_CLIENTS="${kv#clients=}" ;;
		esac
	done
	return 0
}

# Prints the derived state for the given raw counters.
classify_state() {
	local name="$1" alive="$2" pause="$3" listen="$4" ai="$5"
	if [ "$alive" -eq 0 ]; then
		printf 'DOWN'
		return 0
	fi
	if [ "$pause" -ge 1 ]; then
		printf 'FAILED'
		return 0
	fi
	if [ "$listen" -ge 1 ]; then
		if [ "$name" = "GameServer.exe" ]; then
			if [ "$ai" -ge 1 ]; then
				printf 'UP'
			else
				printf 'PARTIAL'
			fi
			return 0
		fi
		printf 'UP'
		return 0
	fi
	printf 'STARTING'
	return 0
}

# Returns 0 if the pid is still alive.
proc_alive() {
	local pid="$1" out
	out="$(ps_run "if (Get-Process -Id $pid -ErrorAction SilentlyContinue) { '1' } else { '0' }")"
	[ "$out" = "1" ]
}

# Stops one process, gracefully unless it is stuck on system("pause").
stop_one() {
	local pid="$1" name="$2" elapsed alive
	parse_status_line "$(proc_status "$pid" 0 0)"
	if [ "$S_ALIVE" -eq 0 ]; then
		printf '[STOP] %s pid=%s (already down)\n' "$name" "$pid"
		return 0
	fi
	if [ "$S_PAUSE" -ge 1 ]; then
		taskkill.exe /F /T /PID "$pid" >/dev/null 2>&1 || true
		printf '[STOP] %s pid=%s (force: pause)\n' "$name" "$pid"
		return 0
	fi
	taskkill.exe /PID "$pid" >/dev/null 2>&1 || true
	elapsed=0
	while [ "$elapsed" -lt "$FDP_STOP_TIMEOUT" ]; do
		if ! proc_alive "$pid"; then
			printf '[STOP] %s pid=%s (graceful, %s sn)\n' "$name" "$pid" "$elapsed"
			return 0
		fi
		sleep 1
		elapsed=$((elapsed + 1))
	done
	taskkill.exe /F /T /PID "$pid" >/dev/null 2>&1 || true
	printf '[STOP] %s pid=%s (force: timeout)\n' "$name" "$pid"
	return 0
}

# Prints the status of all known servers and returns 0 only if 3/3 are UP.
do_status() {
	make_allowed_dirs
	read_ports
	collect_our_processes
	local role exe file i port aiport line state text ready=0 ai_up=0 game_up=0 login_up=0
	for role in AIServer GameServer LogInServer; do
		exe="${role}.exe"
		case "$role" in
			AIServer) port="$AI_PORT"; aiport=0 ;;
			GameServer) port="$GAME_PORT"; aiport="$GAME_AIPORT" ;;
			LogInServer) port="$LOGIN_PORT"; aiport=0 ;;
		esac
		for i in "${!OUR_PIDS[@]}"; do
			if [ "${OUR_NAMES[$i]}" = "$exe" ]; then
				parse_status_line "$(proc_status "${OUR_PIDS[$i]}" "$port" "$aiport")"
				state="$(classify_state "$exe" "$S_ALIVE" "$S_PAUSE" "$S_LISTEN" "$S_AI")"
				text=''
				case "$state" in
					UP)
						if [ "$role" = "GameServer" ]; then
							text="LISTEN  AI=$T_BAGLI  $T_ISTEMCI=$S_CLIENTS"
						else
							text="LISTEN"
						fi
						;;
					PARTIAL) text="LISTEN  AI=$T_BAGLI_DEGIL  $T_ISTEMCI=$S_CLIENTS" ;;
					STARTING) text="$T_BASLATILIYOR" ;;
					FAILED) text="$T_ACILIS_BASARISIZ" ;;
					DOWN) text='DOWN' ;;
				esac
				printf '%-10s %-12s pid=%-6s port=%-5s %-34s %s\n' "[$state]" "$role" "${OUR_PIDS[$i]}" "$port" "$text" "${OUR_PATHS[$i]}"
				case "$state" in
					UP)
						case "$role" in
							AIServer) ai_up=1 ;;
							GameServer) game_up=1 ;;
							LogInServer) login_up=1 ;;
						esac
						;;
				esac
			fi
		done
	done
	for line in "${OTHER_LINES[@]:-}"; do
		[ -n "$line" ] || continue
		local opid oname opath
		IFS='|' read -r opid oname opath <<< "$line"
		if [ -z "$opath" ]; then
			printf '[NOTE]     %s: %s pid=%s (%s)\n' "$T_YOK_SAYILDI" "$oname" "$opid" "$T_YOL_OKUNAMADI"
		else
			printf '[NOTE]     %s: %s pid=%s (%s: %s)\n' "$T_YOK_SAYILDI" "$oname" "$opid" "$T_IZIN_DISI" "$opath"
		fi
	done
	ready=$((ai_up + game_up + login_up))
	printf '%s: %s/3 %s\n' "$T_OZET" "$ready" "$T_HAZIR"
	if [ "$ready" -eq 3 ]; then
		return 0
	fi
	return 1
}

# Parses arguments; sets CMD, CONFIG, KEEP_ON_FAIL, FORCE.
parse_args() {
	CMD=''
	CONFIG=Release
	KEEP_ON_FAIL=0
	FORCE=0
	if [ "$#" -eq 0 ]; then
		err_usage 'no command given'
	fi
	CMD="$1"
	if [ "$CMD" = "-h" ] || [ "$CMD" = "--help" ]; then
		usage
		exit 0
	fi
	case "$CMD" in
		start|stop|status) ;;
		*) err_usage "unknown command: $CMD" ;;
	esac
	shift
	while [ "$#" -gt 0 ]; do
		local arg="$1"
		case "$CMD:$arg" in
			start:--config)
				shift
				if [ "$#" -eq 0 ]; then
					err_usage '--config needs a value'
				fi
				CONFIG="$1"
				;;
			start:--keep-on-fail) KEEP_ON_FAIL=1 ;;
			stop:--force) FORCE=1 ;;
			*:-h|*:--help) usage; exit 0 ;;
			*) err_usage "unknown option for $CMD: $arg" ;;
		esac
		shift
	done
	if [ "$CMD" = "start" ]; then
		case "$CONFIG" in
			Release|Debug) ;;
			*) err_usage "invalid config: $CONFIG" ;;
		esac
	fi
	return 0
}

# Resolves the folder the executables are started from.
resolve_bin_dir() {
	if [ -n "$FDP_SERVER_BIN_DIR" ]; then
		BIN_DIR="$FDP_SERVER_BIN_DIR"
	else
		BIN_DIR="$ROOT/build/bin/x86-$CONFIG/Server"
	fi
}

cmd_start() {
	resolve_bin_dir
	SERVER_DIR="$FDP_RUNTIME_DIR/server"
	local exe ini
	for exe in AIServer.exe GameServer.exe LogInServer.exe; do
		if [ ! -f "$BIN_DIR/$exe" ]; then
			printf 'Error: %s: %s\n' "$T_EXE_YOK" "$BIN_DIR/$exe" >&2
			exit 2
		fi
	done
	if [ ! -d "$SERVER_DIR" ]; then
		printf 'Error: %s: %s\n' "$T_DIZIN_YOK" "$SERVER_DIR" >&2
		exit 2
	fi
	for ini in AIServer.ini GameServer.ini LogInServer.ini; do
		if [ ! -f "$SERVER_DIR/$ini" ]; then
			printf 'Error: %s: %s\n' "$T_EKSIK_INI" "$SERVER_DIR/$ini" >&2
			exit 2
		fi
	done
	read_ports
	if ! port_valid "$AI_PORT"; then
		printf 'Error: %s: AIServer [SETTINGS] PORT=%s\n' "$T_GECERSIZ_PORT" "$AI_PORT" >&2
		exit 2
	fi
	if ! port_valid "$GAME_PORT"; then
		printf 'Error: %s: GameServer [SETTINGS] PORT=%s\n' "$T_GECERSIZ_PORT" "$GAME_PORT" >&2
		exit 2
	fi
	if ! port_valid "$GAME_AIPORT"; then
		printf 'Error: %s: GameServer [AI_SERVER] PORT=%s\n' "$T_GECERSIZ_PORT" "$GAME_AIPORT" >&2
		exit 2
	fi
	if ! port_valid "$LOGIN_PORT"; then
		printf 'Error: %s: LogInServer [SETTINGS] PORT=%s\n' "$T_GECERSIZ_PORT" "$LOGIN_PORT" >&2
		exit 2
	fi
	if [ "$GAME_AIPORT" != "$AI_PORT" ]; then
		printf 'Error: %s (AIServer=%s, GameServer=%s)\n' "$T_PORT_AYNI_DEGIL" "$AI_PORT" "$GAME_AIPORT" >&2
		exit 2
	fi

	make_allowed_dirs
	collect_our_processes
	if [ "${#OUR_PIDS[@]}" -gt 0 ]; then
		printf '[REFUSE] %s; %s\n' "$T_CALISIYOR" "$T_ONCE_STOP"
		exit 1
	fi
	local listen_out
	listen_out="$(ps_run "Get-NetTCPConnection -State Listen -LocalPort $AI_PORT,$GAME_PORT,$LOGIN_PORT -ErrorAction SilentlyContinue | ForEach-Object { '{0}|{1}' -f \$_.LocalPort, \$_.OwningProcess }")"
	if [ -n "$listen_out" ]; then
		printf '[REFUSE] %s (LISTEN: %s); %s\n' "$T_CALISIYOR" "$(printf '%s' "$listen_out" | tr '\n' ' ')" "$T_ONCE_STOP"
		exit 1
	fi

	local roles=(AIServer GameServer LogInServer)
	local exes=(AIServer.exe GameServer.exe LogInServer.exe)
	local ports=("$AI_PORT" "$GAME_PORT" "$LOGIN_PORT")
	local aiports=(0 "$GAME_AIPORT" 0)
	local STARTED_PIDS=()
	local STARTED_NAMES=()
	local failed=0 r role pid elapsed last state line wdir wexe

	wdir="$(winpath "$SERVER_DIR")"
	for r in 0 1 2; do
		role="${roles[$r]}"
		wexe="$(winpath "$BIN_DIR/${exes[$r]}")"
		printf '%s %s: %s (%s %s)\n' "$role" "$T_BASLATILIYOR" "$wexe" "$T_CALISMA_DIZINI" "$wdir"
		pid="$(ps_run "(Start-Process -FilePath '$(psq "$wexe")' -WorkingDirectory '$(psq "$wdir")' -WindowStyle Minimized -PassThru).Id" | tr -d '[:space:]')"
		case "$pid" in
			''|*[!0-9]*)
				printf '[FAIL] %s: %s\n' "$role" "$T_BASLATILIYOR"
				failed=1
				break
				;;
		esac
		STARTED_PIDS+=("$pid")
		STARTED_NAMES+=("${exes[$r]}")
		elapsed=0
		last=0
		state=STARTING
		while :; do
			line="$(proc_status "$pid" "${ports[$r]}" "${aiports[$r]}")"
			parse_status_line "$line"
			state="$(classify_state "${exes[$r]}" "$S_ALIVE" "$S_PAUSE" "$S_LISTEN" "$S_AI")"
			case "$state" in
				UP|FAILED) break ;;
			esac
			if [ "$S_ALIVE" -eq 0 ]; then
				state=DOWN
				break
			fi
			if [ "$elapsed" -ge "$FDP_START_TIMEOUT" ]; then
				state=TIMEOUT
				break
			fi
			if [ "$((elapsed - last))" -ge 10 ]; then
				printf '... %s %s (%s sn)\n' "$role" "$T_BEKLENIYOR" "$elapsed"
				last="$elapsed"
			fi
			sleep 2
			elapsed=$((elapsed + 2))
		done
		case "$state" in
			UP)
				printf '[UP] %s pid=%s (%s sn)\n' "$role" "$pid" "$elapsed"
				;;
			FAILED)
				printf '[FAIL] %s: %s\n' "$role" "$T_ACILIS_BASARISIZ"
				failed=1
				;;
			DOWN)
				printf '[FAIL] %s: %s\n' "$role" "$T_SUREC_KAPANDI"
				failed=1
				;;
			TIMEOUT)
				printf '[FAIL] %s: %s %s (%s sn, %s: %s)\n' "$role" "$FDP_START_TIMEOUT" "$T_HAZIR_OLMADI" "$elapsed" "$T_SON_DURUM" "$state"
				failed=1
				;;
		esac
		if [ "$failed" -eq 1 ]; then
			break
		fi
	done

	if [ "$failed" -eq 1 ]; then
		if [ "$KEEP_ON_FAIL" -eq 1 ]; then
			printf '%s\n' "$T_KEEP_MSG"
			exit 1
		fi
		local k
		for ((k = ${#STARTED_PIDS[@]} - 1; k >= 0; k--)); do
			stop_one "${STARTED_PIDS[$k]}" "${STARTED_NAMES[$k]}"
		done
		exit 1
	fi

	do_status || exit $?
	exit 0
}

cmd_stop() {
	read_ports
	make_allowed_dirs
	collect_our_processes
	if [ "${#OUR_PIDS[@]}" -eq 0 ]; then
		printf '%s\n' "$T_CALISAN_YOK"
		exit 0
	fi
	local i role exe
	if [ "$FORCE" -eq 0 ]; then
		for i in "${!OUR_PIDS[@]}"; do
			if [ "${OUR_NAMES[$i]}" = "GameServer.exe" ]; then
				parse_status_line "$(proc_status "${OUR_PIDS[$i]}" "$GAME_PORT" "$GAME_AIPORT")"
				if [ "$S_CLIENTS" -gt 0 ]; then
					printf '[REFUSE] %s (%s)\n' "$T_REFUSE_CLIENTS" "$T_ISTEMCI=$S_CLIENTS"
					exit 1
				fi
			fi
		done
	fi
	for role in LogInServer GameServer AIServer; do
		exe="${role}.exe"
		for i in "${!OUR_PIDS[@]}"; do
			if [ "${OUR_NAMES[$i]}" = "$exe" ]; then
				stop_one "${OUR_PIDS[$i]}" "$exe"
			fi
		done
	done
	collect_our_processes
	if [ "${#OUR_PIDS[@]}" -eq 0 ]; then
		exit 0
	fi
	for i in "${!OUR_PIDS[@]}"; do
		printf '[FAIL] %s: %s pid=%s\n' "$T_DURDURULAMADI" "${OUR_NAMES[$i]}" "${OUR_PIDS[$i]}"
	done
	printf '%s: %s\n' "$T_KALAN" "${#OUR_PIDS[@]}"
	exit 1
}

parse_args "$@"

if ! command -v powershell.exe >/dev/null 2>&1; then
	printf 'Error: %s\n' "$T_PS_YOK" >&2
	exit 2
fi

case "$CMD" in
	start) cmd_start ;;
	stop) cmd_stop ;;
	status) do_status || exit $? ;;
esac
