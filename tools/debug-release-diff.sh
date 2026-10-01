#!/usr/bin/env bash
# Lists Debug/Release behavior differences in the Fire Drake Project v1453 sources.
# Every result is produced by grep/parsing from the sources; nothing is hardcoded.
# Output is Markdown on stdout.
# Usage: tools/debug-release-diff.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

usage() {
	printf 'Usage: %s\n' "$(basename "$0")"
}

for arg in "$@"; do
	case "$arg" in
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

SRC_DIRS=(GameServer AIServer LogInServer shared N3BASE)
VCXPROJ_FILES=(GameServer/proj-GameServer.vcxproj AIServer/proj-AIServer.vcxproj LogInServer/proj-LogInServer.vcxproj shared/shared.vcxproj)
COND_RE='^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef|elif)\b.*\b(DEBUG|_DEBUG|NDEBUG|DISABLE_PLAYER_BLINKING|USE_SQL_TRACE)\b'

MATCHES="$(grep -a -rnE --include='*.cpp' --include='*.h' "$COND_RE" "${SRC_DIRS[@]}" 2>/dev/null || true)"
SORTED_MATCHES="$(printf '%s\n' "$MATCHES" | tr -d '\r' | sort -t: -k1,1 -k2,2n)"

printf '## B\u00f6l\u00fcm 1 \u2014 Ko\u015fullu bloklar\n\n'
printf 'Kaynak: %s alt\u0131ndaki `*.cpp`/`*.h`; `grep -a -rnE`. Belirte\u00e7ler: `DEBUG`, `_DEBUG`, `NDEBUG`, `DISABLE_PLAYER_BLINKING`, `USE_SQL_TRACE`.\n\n' "${SRC_DIRS[*]}"
if [ -n "$MATCHES" ]; then
	while IFS= read -r match; do
		[ -n "$match" ] || continue
		m_file="${match%%:*}"
		m_rest="${match#*:}"
		m_line="${m_rest%%:*}"
		m_text="${m_rest#*:}"
		printf -- '- `%s:%s` \u2014 `%s`\n' "$m_file" "$m_line" "$m_text"
	done <<< "$SORTED_MATCHES"
else
	printf '(eslesme yok)\n'
fi

printf '\n## B\u00f6l\u00fcm 2 \u2014 vcxproj \u00f6ni\u015flemci tan\u0131mlar\u0131\n\n'
printf '`<PreprocessorDefinitions>` sat\u0131rlar\u0131 (proje x yap\u0131land\u0131rma):\n\n'
printf '| Proje | Yap\u0131land\u0131rma | Ara\u00e7 | Tan\u0131mlar |\n|---|---|---|---|\n'
for proj in "${VCXPROJ_FILES[@]}"; do
	[ -f "$proj" ] || continue
	name="${proj##*/}"
	cfg=""
	tool=""
	while IFS= read -r line; do
		line="${line%$'\r'}"
		if [[ "$line" == *'ItemDefinitionGroup Condition='* ]]; then
			cfg="$(printf '%s' "$line" | sed -E "s/.*=='([^']*)'.*/\1/; s/\|.*//")"
		fi
		case "$line" in
			*'<ClCompile>'*) tool="ClCompile" ;;
			*'<Midl>'*) tool="Midl" ;;
			*'<ResourceCompile>'*) tool="ResourceCompile" ;;
			*'<Link>'*) tool="Link" ;;
		esac
		if [[ "$line" == *'<PreprocessorDefinitions'* ]]; then
			defs="${line#*<PreprocessorDefinitions}"
			defs="${defs#*>}"
			defs="${defs%%</PreprocessorDefinitions>*}"
			if [ -n "$defs" ] && [ "$defs" != '%(PreprocessorDefinitions)' ] && [ -n "$cfg" ]; then
				printf '| %s | %s | %s | %s |\n' "$name" "$cfg" "$tool" "$defs"
			fi
		fi
	done < "$proj"
done

printf '\nDebug/Release belirte\u00e7 \u00f6zeti (`RuntimeLibrary` \u00f6rt\u00fck tan\u0131mlar\u0131 dahil):\n\n'
printf '| Proje | Debug belirteci | Release belirteci |\n|---|---|---|\n'
marker_of() {
	local defs="$1" token="$2" runtime="$3"
	if [[ ";$defs;" == *";$token;"* ]]; then
		printf '%s (explicit)' "$token"
	elif [ -n "$runtime" ]; then
		printf '%s (implicit via RuntimeLibrary=%s)' "$token" "$runtime"
	else
		printf '%s (none)' "$token"
	fi
}
for proj in "${VCXPROJ_FILES[@]}"; do
	[ -f "$proj" ] || continue
	name="${proj##*/}"
	cfg=""
	debug_defs=""
	release_defs=""
	debug_rt=""
	release_rt=""
	while IFS= read -r line; do
		line="${line%$'\r'}"
		if [[ "$line" == *'ItemDefinitionGroup Condition='* ]]; then
			cfg="$(printf '%s' "$line" | sed -E "s/.*=='([^']*)'.*/\1/; s/\|.*//")"
		fi
		if [[ "$line" == *'<PreprocessorDefinitions'* ]]; then
			defs="${line#*<PreprocessorDefinitions}"
			defs="${defs#*>}"
			defs="${defs%%</PreprocessorDefinitions>*}"
			if [ -n "$defs" ] && [ "$defs" != '%(PreprocessorDefinitions)' ]; then
				if [ "$cfg" = "Debug" ]; then
					debug_defs="$debug_defs;$defs"
				elif [ "$cfg" = "Release" ]; then
					release_defs="$release_defs;$defs"
				fi
			fi
		fi
		if [[ "$line" == *'<RuntimeLibrary>'* ]]; then
			rt="${line#*<RuntimeLibrary>}"
			rt="${rt%%</RuntimeLibrary>*}"
			if [ "$cfg" = "Debug" ]; then
				debug_rt="$rt"
			elif [ "$cfg" = "Release" ]; then
				release_rt="$rt"
			fi
		fi
	done < "$proj"
	printf '| %s | %s | %s |\n' "$name" "$(marker_of "$debug_defs" _DEBUG "$debug_rt")" "$(marker_of "$release_defs" NDEBUG "$release_rt")"
done

printf '\n## B\u00f6l\u00fcm 3 \u2014 \u00d6zet tablo\n\n'
printf '"Debug / Release" s\u00fctunu kaynak okunarak elle doldurulur; bu betik yaln\u0131zca konumlar\u0131 \u00e7\u0131kar\u0131r.\n\n'
printf '| Dosya:sat\u0131r | Ko\u015ful | Debug / Release |\n|---|---|---|\n'
if [ -n "$MATCHES" ]; then
	while IFS= read -r match; do
		[ -n "$match" ] || continue
		m_file="${match%%:*}"
		m_rest="${match#*:}"
		m_line="${m_rest%%:*}"
		m_text="${m_rest#*:}"
		printf '| `%s:%s` | `%s` | (elle doldurulacak) |\n' "$m_file" "$m_line" "$m_text"
	done <<< "$SORTED_MATCHES"
else
	printf '| - | - | (eslesme yok) |\n'
fi
