#!/usr/bin/env bash
# Environment health check for the Fire Drake Project v1453 sandbox.
# Checks toolchain, repository, runtime directory, database and ODBC status.
# Usage: tools/check-env.sh [--skip-db]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FDP_RUNTIME_DIR="${FDP_RUNTIME_DIR:-/mnt/c/dev/fdp}"
MSBUILD="${MSBUILD:-/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe}"
SQLCMD="${SQLCMD:-/mnt/c/Program Files/Microsoft SQL Server/Client SDK/ODBC/130/Tools/Binn/SQLCMD.EXE}"
FDP_SQL_INSTANCE="${FDP_SQL_INSTANCE:-.\\SQLEXPRESS}"
FDP_SQL_DB="${FDP_SQL_DB:-FDP_kn_online}"

# Turkish output strings are escaped so this file stays ASCII (AGENTS.md section 3).
T_OZET=$'\u00d6zet'
T_CALISTIRILABILIR=$'\u00e7al\u0131\u015ft\u0131r\u0131labilir'
T_DEPO_KOKU=$'depo k\u00f6k\u00fc'
T_DEPOSU_DEGIL=$'deposu de\u011fil'
T_CALISMA_AGACI=$'\u00e7al\u0131\u015fma a\u011fac\u0131'
T_DEGISIKLIK=$'de\u011fi\u015fiklik'
T_SAYISI=$'say\u0131s\u0131'
T_BOLUMLERI=$'b\u00f6l\u00fcmleri'
T_BAGLANTISI=$'ba\u011flant\u0131s\u0131'
T_BAGLANAMADI=$'ba\u011flanamad\u0131'
T_BASARISIZ=$'ba\u015far\u0131s\u0131z'
T_SURUMU=$'s\u00fcr\u00fcm\u00fc'
T_KI001=$'KI-001 d\u00fczeltmesi uygulanmam\u0131\u015f (bkz. docs/KNOWN_ISSUES.md)'
T_TANIMLI=$'tan\u0131ml\u0131'
T_BULUNAMADI=$'bulunamad\u0131'

SKIP_DB=0
PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0
WARN_COUNT=0

usage() {
	printf 'Usage: %s [--skip-db]\n' "$(basename "$0")"
	printf '  --skip-db   skip database checks (D-01..D-05)\n'
	printf '  -h, --help  show this help\n'
}

for arg in "$@"; do
	case "$arg" in
		--skip-db)
			SKIP_DB=1
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			printf 'Unknown argument: %s\n' "$arg" >&2
			usage >&2
			exit 2
			;;
	esac
done

report() {
	local status="$1" id="$2" label="$3" detail="${4:-}"
	printf '[%s] %-5s %-36s %s\n' "$status" "$id" "$label" "$detail"
	case "$status" in
		PASS) PASS_COUNT=$((PASS_COUNT + 1)) ;;
		FAIL) FAIL_COUNT=$((FAIL_COUNT + 1)) ;;
		SKIP) SKIP_COUNT=$((SKIP_COUNT + 1)) ;;
		WARN) WARN_COUNT=$((WARN_COUNT + 1)) ;;
	esac
}

sql_query() {
	"$SQLCMD" -S "$FDP_SQL_INSTANCE" -E -d "$FDP_SQL_DB" -h -1 -W -Q "SET NOCOUNT ON; $1" 2>/dev/null
}

first_value() {
	printf '%s\n' "$1" | tr -d '\r' | sed -n '1{s/^[[:space:]]*//;s/[[:space:]]*$//;p;}'
}

# T - toolchain
if [ -f "$MSBUILD" ]; then
	report PASS T-01 "MSBuild bulundu" "$MSBUILD"
else
	report FAIL T-01 "MSBuild bulundu" "yok: $MSBUILD"
fi

if [ -x "$ROOT/tools/build.sh" ]; then
	report PASS T-02 "tools/build.sh $T_CALISTIRILABILIR" "$ROOT/tools/build.sh"
else
	report FAIL T-02 "tools/build.sh $T_CALISTIRILABILIR" "yok veya calistirilamaz: $ROOT/tools/build.sh"
fi

if command -v git >/dev/null 2>&1; then
	report PASS T-03 "git komutu var" "$(command -v git)"
else
	report FAIL T-03 "git komutu var" "yok"
fi

# R - repository
REPO_ROOT=""
if REPO_ROOT="$(git -C "$ROOT" rev-parse --show-toplevel 2>/dev/null)"; then
	if [ -f "$REPO_ROOT/KnightOnlineServer.sln" ]; then
		report PASS R-01 "$T_DEPO_KOKU" "$REPO_ROOT"
	else
		report FAIL R-01 "$T_DEPO_KOKU" "KnightOnlineServer.sln yok: $REPO_ROOT"
	fi
else
	report FAIL R-01 "$T_DEPO_KOKU" "git $T_DEPOSU_DEGIL"
fi

HEAD_SHA="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || printf '?')"
HEAD_BRANCH="$(git -C "$ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || printf '?')"
report PASS R-02 "HEAD ve dal" "$HEAD_BRANCH @ $HEAD_SHA"

VERSION_FILE="$ROOT/shared/version.h"
FOUND_VERSION=""
if [ -f "$VERSION_FILE" ]; then
	FOUND_VERSION="$(sed -nE 's/^[[:space:]]*#[[:space:]]*define[[:space:]]+__VERSION[[:space:]]+([0-9]+).*/\1/p' "$VERSION_FILE" | head -n1)"
fi
if [ "$FOUND_VERSION" = "1453" ]; then
	report PASS R-03 "version.h __VERSION 1453" "bulunan: $FOUND_VERSION"
else
	report FAIL R-03 "version.h __VERSION 1453" "bulunan: ${FOUND_VERSION:-yok}"
fi

DIRTY="$(git -C "$ROOT" status --porcelain 2>/dev/null || true)"
if [ -z "$DIRTY" ]; then
	report PASS R-04 "$T_CALISMA_AGACI temiz" ""
else
	report WARN R-04 "$T_CALISMA_AGACI temiz" "$T_DEGISIKLIK: $(printf '%s\n' "$DIRTY" | wc -l)"
fi

# W - runtime directory
if [ -d "$FDP_RUNTIME_DIR/server" ]; then
	report PASS W-01 "server/ klasoru" "$FDP_RUNTIME_DIR/server"
else
	report FAIL W-01 "server/ klasoru" "yok: $FDP_RUNTIME_DIR/server"
fi

MISSING_EXE=""
for f in AIServer.exe GameServer.exe LogInServer.exe; do
	if [ ! -f "$FDP_RUNTIME_DIR/server/$f" ]; then
		MISSING_EXE="$MISSING_EXE $f"
	fi
done
if [ -z "$MISSING_EXE" ]; then
	report PASS W-02 "server/*.exe (3)" "3/3"
else
	report FAIL W-02 "server/*.exe (3)" "eksik:$MISSING_EXE"
fi

MISSING_INI=""
for f in AIServer.ini GameServer.ini LogInServer.ini; do
	if [ ! -f "$FDP_RUNTIME_DIR/server/$f" ]; then
		MISSING_INI="$MISSING_INI $f"
	fi
done
if [ -z "$MISSING_INI" ]; then
	report PASS W-03 "server/*.ini (3)" "3/3"
else
	report FAIL W-03 "server/*.ini (3)" "eksik:$MISSING_INI"
fi

GAME_INI="$FDP_RUNTIME_DIR/server/GameServer.ini"
if [ ! -f "$GAME_INI" ]; then
	report FAIL W-04 "GameServer.ini $T_BOLUMLERI" "dosya yok"
else
	MISSING_SECTIONS=""
	for section in ODBC ZONE_INFO AI_SERVER; do
		if ! grep -aq "^\[$section\]" "$GAME_INI"; then
			MISSING_SECTIONS="$MISSING_SECTIONS [$section]"
		fi
	done
	if [ -z "$MISSING_SECTIONS" ]; then
		report PASS W-04 "GameServer.ini $T_BOLUMLERI" "[ODBC] [ZONE_INFO] [AI_SERVER]"
	else
		report FAIL W-04 "GameServer.ini $T_BOLUMLERI" "eksik:$MISSING_SECTIONS"
	fi
fi

MAP_COUNT=0
for f in "$FDP_RUNTIME_DIR"/server/Map/*.smd; do
	if [ -f "$f" ]; then
		MAP_COUNT=$((MAP_COUNT + 1))
	fi
done
if [ "$MAP_COUNT" -ge 24 ]; then
	report PASS W-05 "Map/*.smd $T_SAYISI (>=24)" "$MAP_COUNT"
else
	report FAIL W-05 "Map/*.smd $T_SAYISI (>=24)" "bulunan: $MAP_COUNT"
fi

if [ -f "$FDP_RUNTIME_DIR/server/Map/freezone_a_20050718.smd" ]; then
	report PASS W-06 "freezone_a_20050718.smd" "var"
else
	report FAIL W-06 "freezone_a_20050718.smd" "yok"
fi

QUEST_COUNT=0
for f in "$FDP_RUNTIME_DIR"/server/Quests/*.lua; do
	if [ -f "$f" ]; then
		QUEST_COUNT=$((QUEST_COUNT + 1))
	fi
done
if [ "$QUEST_COUNT" -ge 114 ]; then
	report PASS W-07 "Quests/*.lua $T_SAYISI (>=114)" "$QUEST_COUNT"
else
	report FAIL W-07 "Quests/*.lua $T_SAYISI (>=114)" "bulunan: $QUEST_COUNT"
fi

MISSING_CLIENT=""
for f in Client/KnightOnline.exe Client/Server.ini; do
	if [ ! -f "$FDP_RUNTIME_DIR/$f" ]; then
		MISSING_CLIENT="$MISSING_CLIENT $f"
	fi
done
if [ -z "$MISSING_CLIENT" ]; then
	report PASS W-08 "Client dosyalari" "2/2"
else
	report FAIL W-08 "Client dosyalari" "eksik:$MISSING_CLIENT"
fi

# D - database (read-only SELECT queries only; see AGENTS.md section 2.7)
DB_READY=0
if [ "$SKIP_DB" -eq 1 ]; then
	report SKIP D-01 "sqlcmd $T_BAGLANTISI" "(--skip-db)"
elif [ ! -f "$SQLCMD" ]; then
	report FAIL D-01 "sqlcmd $T_BAGLANTISI" "sqlcmd yok: $SQLCMD"
elif DB_OUT="$(sql_query 'SELECT 1')" && printf '%s\n' "$DB_OUT" | grep -q '1'; then
	report PASS D-01 "sqlcmd $T_BAGLANTISI" "SELECT 1"
	DB_READY=1
else
	report FAIL D-01 "sqlcmd $T_BAGLANTISI" "$T_BAGLANAMADI: $FDP_SQL_INSTANCE / $FDP_SQL_DB"
fi

DB_TABLES="MAGIC MAGIC_TYPE1 MAGIC_TYPE3 MAGIC_TYPE4 ITEM COEFFICIENT LEVEL_UP ZONE_INFO K_NPCPOS START_POSITION"
if [ "$SKIP_DB" -eq 1 ]; then
	report SKIP D-02 "zorunlu tablolar" "(--skip-db)"
elif [ "$DB_READY" -ne 1 ]; then
	report SKIP D-02 "zorunlu tablolar" "(baglanti yok)"
elif DB_OUT="$(sql_query "SELECT name FROM sys.tables WHERE name IN ('MAGIC','MAGIC_TYPE1','MAGIC_TYPE3','MAGIC_TYPE4','ITEM','COEFFICIENT','LEVEL_UP','ZONE_INFO','K_NPCPOS','START_POSITION')")"; then
	MISSING_TABLES=""
	for t in $DB_TABLES; do
		if ! printf '%s\n' "$DB_OUT" | tr -d '\r' | grep -qix "$t"; then
			MISSING_TABLES="$MISSING_TABLES $t"
		fi
	done
	if [ -z "$MISSING_TABLES" ]; then
		report PASS D-02 "zorunlu tablolar" "10/10"
	else
		report FAIL D-02 "zorunlu tablolar" "eksik:$MISSING_TABLES"
	fi
else
	report FAIL D-02 "zorunlu tablolar" "sorgu $T_BASARISIZ"
fi

if [ "$SKIP_DB" -eq 1 ]; then
	report SKIP D-03 "ZONE_INFO ZoneNo=71" "(--skip-db)"
elif [ "$DB_READY" -ne 1 ]; then
	report SKIP D-03 "ZONE_INFO ZoneNo=71" "(baglanti yok)"
elif DB_OUT="$(sql_query 'SELECT RTRIM(strZoneName) FROM ZONE_INFO WHERE ZoneNo = 71')"; then
	ZONE_NAME="$(first_value "$DB_OUT")"
	if [ "$ZONE_NAME" = "freezone_a_20050718.smd" ]; then
		report PASS D-03 "ZONE_INFO ZoneNo=71" "freezone_a_20050718.smd"
	else
		report FAIL D-03 "ZONE_INFO ZoneNo=71" "beklenen: freezone_a_20050718.smd, bulunan: ${ZONE_NAME:-yok}"
	fi
else
	report FAIL D-03 "ZONE_INFO ZoneNo=71" "sorgu $T_BASARISIZ"
fi

if [ "$SKIP_DB" -eq 1 ]; then
	report SKIP D-04 "MAGIC Etc=1" "(--skip-db)"
elif [ "$DB_READY" -ne 1 ]; then
	report SKIP D-04 "MAGIC Etc=1" "(baglanti yok)"
elif DB_OUT="$(sql_query 'SELECT COUNT(*) FROM MAGIC WHERE Etc = 1')"; then
	ETC_COUNT="$(first_value "$DB_OUT")"
	if [ "$ETC_COUNT" = "0" ]; then
		report PASS D-04 "MAGIC Etc=1" "0"
	else
		report FAIL D-04 "MAGIC Etc=1" "$T_KI001 (Etc=1: ${ETC_COUNT:-yok})"
	fi
else
	report FAIL D-04 "MAGIC Etc=1" "sorgu $T_BASARISIZ"
fi

if [ "$SKIP_DB" -eq 1 ]; then
	report SKIP D-05 "istemci $T_SURUMU" "(--skip-db)"
elif [ "$DB_READY" -ne 1 ]; then
	report SKIP D-05 "istemci $T_SURUMU" "(baglanti yok)"
elif DB_OUT="$(sql_query 'SELECT MAX(sVersion) FROM VERSION')"; then
	DB_VERSION="$(first_value "$DB_OUT")"
	CLIENT_INI="$FDP_RUNTIME_DIR/Client/Server.ini"
	INI_VERSION=""
	if [ -f "$CLIENT_INI" ]; then
		INI_VERSION="$(awk -F= '
			/^\[/ { sec=$0; gsub(/[\[\]\r]/, "", sec); inver=(tolower(sec)=="version"); next }
			inver { key=$1; gsub(/[ \t]/, "", key); if (tolower(key)=="files") { val=$2; gsub(/[ \t\r]/, "", val); print val; exit } }
		' "$CLIENT_INI")"
	fi
	if [ -n "$DB_VERSION" ] && [ "$DB_VERSION" = "$INI_VERSION" ]; then
		report PASS D-05 "istemci $T_SURUMU" "DB=$DB_VERSION, Client/Server.ini=$INI_VERSION"
	else
		report FAIL D-05 "istemci $T_SURUMU" "DB=${DB_VERSION:-yok}, Client/Server.ini=${INI_VERSION:-yok}"
	fi
else
	report FAIL D-05 "istemci $T_SURUMU" "sorgu $T_BASARISIZ"
fi

# O - ODBC (32-bit DSNs)
if command -v powershell.exe >/dev/null 2>&1; then
	if DSNS="$(powershell.exe -NoProfile -Command "Get-OdbcDsn -Platform '32-bit' | Select-Object -ExpandProperty Name" 2>/dev/null)"; then
		DSNS_CLEAN="$(printf '%s\n' "$DSNS" | tr -d '\r')"
		dsn_index=1
		for dsn in KO_MAIN KO_GAME; do
			if printf '%s\n' "$DSNS_CLEAN" | grep -qx "$dsn"; then
				report PASS "O-0$dsn_index" "32-bit DSN $dsn" "$T_TANIMLI"
			else
				report FAIL "O-0$dsn_index" "32-bit DSN $dsn" "$T_BULUNAMADI"
			fi
			dsn_index=$((dsn_index + 1))
		done
	else
		report SKIP O-01 "32-bit DSN KO_MAIN" "(Get-OdbcDsn calistirilamadi)"
		report SKIP O-02 "32-bit DSN KO_GAME" "(Get-OdbcDsn calistirilamadi)"
	fi
else
	report SKIP O-01 "32-bit DSN KO_MAIN" "(powershell.exe yok)"
	report SKIP O-02 "32-bit DSN KO_GAME" "(powershell.exe yok)"
fi

printf '%s: %d PASS, %d FAIL, %d SKIP, %d WARN\n' "$T_OZET" "$PASS_COUNT" "$FAIL_COUNT" "$SKIP_COUNT" "$WARN_COUNT"

if [ "$FAIL_COUNT" -gt 0 ]; then
	exit 1
fi
exit 0
