#!/usr/bin/env bash
# Otonom planla -> uygula -> dogrula dongusu. Tasarim: plans/OTONOM_DONGU.md
#
# GUVENLIK: Varsayilan olarak hicbir sey CALISTIRMAZ; yalnizca ne yapacagini
# yazip cikar (dry-run). Gercekten calismasi icin --run ZORUNLUDUR.
#
# Kullanim:
#   ./tools/auto-loop.sh            # dry-run: ayarlari ve on kontrolleri yazar, cikar
#   ./tools/auto-loop.sh --run      # calistirir (arka plan: nohup ... &)
#
# Durdurma: calisirken  touch plans/.auto-loop-stop  (sonraki iterasyon basinda kontrol edilir)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# --- Ayarlar (plans/OTONOM_DONGU.md SS7) ---------------------------------
OPENCODE_MODEL="${OPENCODE_MODEL:-opencode-go/deepseek-v4.1-flash}"
CLAUDE_MODEL="${CLAUDE_MODEL:-opus}"
MAX_CORRECTION_TURNS="${MAX_CORRECTION_TURNS:-3}"
MAX_ITERATIONS="${MAX_ITERATIONS:-5}"
MAX_WALLCLOCK_HOURS="${MAX_WALLCLOCK_HOURS:-8}"
OPENCODE_TIMEOUT_SEC="${OPENCODE_TIMEOUT_SEC:-3600}"
CLAUDE_TIMEOUT_SEC="${CLAUDE_TIMEOUT_SEC:-2400}"
CLAUDE_MAX_BUDGET_USD="${CLAUDE_MAX_BUDGET_USD:-10}"

ACTIVE_PLAN_FILE="plans/.aktif-plan"
STOP_FILE="plans/.auto-loop-stop"
LOG_DIR="plans/_logs"
MAIN_LOG="$LOG_DIR/auto-loop.log"
STATUS_FILE="docs/STATUS.md"

# Claude'a her cagrida verilen sabit yasaklar (opencode.json deny listesiyle ayni).
DENY_TOOLS=("Bash(git push*)" "Bash(git merge*)" "Bash(git rebase*)" "Bash(git reset --hard*)" "Bash(git clean*)" "Bash(rm -rf*)")

RUN=false
for a in "$@"; do
	case "$a" in
	--run) RUN=true ;;
	-h | --help)
		sed -n '2,12p' "$0"
		exit 0
		;;
	esac
done

# Skill'ler bu degiskene bakarak insansiz modu bilir (kullaniciya soru sormaz).
export AUTO_LOOP=1

log() {
	mkdir -p "$LOG_DIR"
	printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" | tee -a "$MAIN_LOG"
}

append_blocker() {
	local reason="$1"
	local row="| $(date '+%Y-%m-%d %H:%M') | otonom döngü durdu | auto-loop.sh | $reason |"
	log "BLOKER: $reason"
	if $RUN && [ -f "$STATUS_FILE" ]; then
		awk -v row="$row" '
			{print}
			/^## Blokajlar$/ {found=1}
			found && /^\|---/ && !inserted {print row; inserted=1; found=0}
		' "$STATUS_FILE" >"$STATUS_FILE.tmp" && mv "$STATUS_FILE.tmp" "$STATUS_FILE"
	fi
}

plan_durum() {
	grep -m1 -E '^\| Durum \|' "$1" | sed -E 's/^\| Durum \| *([^|]*) *\|.*/\1/' | xargs
}

stop_requested() { [ -f "$STOP_FILE" ]; }

# Dogrulayicinin yazdigi SON "Düzeltme talimatı" kod blogunu cikarir.
extract_correction() {
	awk '
		/Düzeltme talimatı/ {f=1; c=0; buf=""; next}
		f && /^[[:space:]]*```/ {c++; if (c==2) {f=0; last=buf}; next}
		f && c==1 {buf = buf $0 "\n"}
		END {printf "%s", last}
	' "$1"
}

run_claude() {
	# $1 = prompt, $2 = log dosyasi
	timeout "$CLAUDE_TIMEOUT_SEC" claude -p "$1" \
		--permission-mode bypassPermissions \
		--disallowedTools "${DENY_TOOLS[@]}" \
		--output-format json \
		--model "$CLAUDE_MODEL" \
		--max-budget-usd "$CLAUDE_MAX_BUDGET_USD" \
		--no-session-persistence </dev/null >"$2" 2>&1
}

# --- On kontroller --------------------------------------------------------
PRE_OK=true
pre() { # $1=mesaj
	echo "ON KONTROL HATASI: $1" >&2
	PRE_OK=false
}

command -v opencode >/dev/null || pre "opencode komutu yok"
command -v claude >/dev/null || pre "claude komutu yok"
for f in AGENTS.md CLAUDE.md opencode.json plans/_SABLON.md docs/STATUS.md .claude/skills/plan-olustur/SKILL.md .claude/skills/plan-dogrula/SKILL.md; do
	[ -f "$f" ] || pre "$f yok"
done
# Altyapi dosyalari commit'li olmali; aksi halde DeepSeek'in dali bunlari commit'e katabilir
# veya sonraki dallar bunlari gormez.
for f in AGENTS.md CLAUDE.md opencode.json docs/STATUS.md plans/README.md; do
	git ls-files --error-unmatch "$f" >/dev/null 2>&1 || pre "$f git'te takipli degil (once commit'leyin)"
done
if [ -n "$(git status --porcelain 2>/dev/null | grep -v '^?? start.md$' || true)" ]; then
	pre "calisma agaci temiz degil (git status). Once commit'leyin veya stash edin."
fi

if ! $RUN; then
	cat <<EOF
=== DRY-RUN (gerçekten çalıştırmak için: $0 --run) ===
Dal: $(git branch --show-current)
Aktif plan işaretçisi: $ACTIVE_PLAN_FILE $( [ -f "$ACTIVE_PLAN_FILE" ] && echo "(var: $(cat "$ACTIVE_PLAN_FILE"))" || echo "(yok: ilk adımda claude /plan-olustur çağrılacak)" )
Ayarlar:
  OPENCODE_MODEL=$OPENCODE_MODEL
  CLAUDE_MODEL=$CLAUDE_MODEL
  MAX_CORRECTION_TURNS=$MAX_CORRECTION_TURNS  MAX_ITERATIONS=$MAX_ITERATIONS  MAX_WALLCLOCK_HOURS=$MAX_WALLCLOCK_HOURS
  OPENCODE_TIMEOUT_SEC=$OPENCODE_TIMEOUT_SEC  CLAUDE_TIMEOUT_SEC=$CLAUDE_TIMEOUT_SEC  CLAUDE_MAX_BUDGET_USD=$CLAUDE_MAX_BUDGET_USD
Kalıcı yasaklar (opencode.json deny + claude --disallowedTools):
  ${DENY_TOOLS[*]}
Faz sınırında döngü her zaman durur (plan-olustur skill'i karar verir).
EOF
	if $PRE_OK; then echo "ÖN KONTROLLER: hepsi tamam."; else echo "ÖN KONTROLLER: BAŞARISIZ (yukarıya bakın) — --run reddedilir."; fi
	echo "Hiçbir komut çalıştırılmadı."
	exit 0
fi

$PRE_OK || exit 2

mkdir -p "$LOG_DIR"
START_TS=$(date +%s)
ITER=0
CORRECTION_TURNS=0
NO_PROGRESS=0

log "=== auto-loop.sh basladi (RUN=true, model: $OPENCODE_MODEL / $CLAUDE_MODEL) ==="

next_plan() {
	# DOGRULANDI sonrasi (veya ilk acilista) siradaki plani yazdirir.
	local before after slog
	before="$(cat "$ACTIVE_PLAN_FILE" 2>/dev/null || true)"
	slog="$LOG_DIR/next-plan-iter$ITER-$(date +%s).log"
	log "  -> claude -p /plan-olustur (log: $slog)"
	run_claude "/plan-olustur" "$slog" || log "  claude (plan-olustur) sifir olmayan cikis kodu dondurdu."
	after="$(cat "$ACTIVE_PLAN_FILE" 2>/dev/null || true)"
	if [ -z "$after" ] || [ "$after" = "$before" ]; then
		return 1
	fi
	return 0
}

while true; do
	if stop_requested; then
		log "DUR: $STOP_FILE bulundu, temiz cikis."
		rm -f "$STOP_FILE"
		exit 0
	fi

	if [ $(($(date +%s) - START_TS)) -ge $((MAX_WALLCLOCK_HOURS * 3600)) ]; then
		append_blocker "STOP-05: MAX_WALLCLOCK_HOURS ($MAX_WALLCLOCK_HOURS) asildi."
		exit 0
	fi

	ITER=$((ITER + 1))
	if [ "$ITER" -gt "$MAX_ITERATIONS" ]; then
		append_blocker "STOP-06: MAX_ITERATIONS ($MAX_ITERATIONS) asildi. Devam icin yeniden baslatin."
		exit 0
	fi

	# Aktif plan yoksa: ilk plani yazdir (bootstrap).
	if [ ! -f "$ACTIVE_PLAN_FILE" ]; then
		if ! next_plan; then
			append_blocker "Bootstrap: /plan-olustur aktif plan uretmedi (faz sinirı, TASLAK veya hata). Bkz. $LOG_DIR"
			exit 0
		fi
		continue
	fi

	PLAN_PATH=$(cat "$ACTIVE_PLAN_FILE")
	if [ ! -f "$PLAN_PATH" ]; then
		append_blocker "$ACTIVE_PLAN_FILE '$PLAN_PATH' gosteriyor ama dosya yok."
		exit 0
	fi

	DURUM=$(plan_durum "$PLAN_PATH")
	log "Iterasyon $ITER - plan: $PLAN_PATH - durum: $DURUM"
	PLAN_BASE="$(basename "$PLAN_PATH" .md)"
	STEP_LOG="$LOG_DIR/$PLAN_BASE-iter$ITER-$(date +%s).log"

	case "$DURUM" in
	HAZIR | "DÜZELTME GEREKLİ")
		if [ "$DURUM" = "HAZIR" ]; then
			CORRECTION_TURNS=0
			PROMPT="$PLAN_PATH planını AGENTS.md kurallarına göre uygula."
		else
			CORRECTION_TURNS=$((CORRECTION_TURNS + 1))
			if [ "$CORRECTION_TURNS" -gt "$MAX_CORRECTION_TURNS" ]; then
				append_blocker "STOP-01: '$PLAN_PATH' $MAX_CORRECTION_TURNS duzeltme turunu asti, hala DOGRULANDI degil."
				exit 0
			fi
			PROMPT="$(extract_correction "$PLAN_PATH")"
			if [ -z "$PROMPT" ]; then
				append_blocker "STOP-02: '$PLAN_PATH' DÜZELTME GEREKLİ ama düzeltme talimatı bloğu bulunamadı."
				exit 0
			fi
		fi
		log "  -> opencode run --auto (log: $STEP_LOG)"
		timeout "$OPENCODE_TIMEOUT_SEC" opencode run --auto --agent build -m "$OPENCODE_MODEL" "$PROMPT" </dev/null >"$STEP_LOG" 2>&1 ||
			log "  opencode run sifir olmayan cikis kodu dondurdu; Durum kontrol ediliyor."
		NEW_DURUM=$(plan_durum "$PLAN_PATH")
		if [ "$NEW_DURUM" = "$DURUM" ]; then
			NO_PROGRESS=$((NO_PROGRESS + 1))
			if [ "$NO_PROGRESS" -ge 2 ]; then
				append_blocker "STOP-02: opencode art arda 2 kez '$PLAN_PATH' Durum'unu degistirmedi ($DURUM). Log: $STEP_LOG"
				exit 0
			fi
			log "  Durum degismedi ($DURUM); bir kez daha denenecek."
			[ "$DURUM" = "DÜZELTME GEREKLİ" ] && CORRECTION_TURNS=$((CORRECTION_TURNS - 1))
		else
			NO_PROGRESS=0
		fi
		;;
	UYGULANDI)
		log "  -> claude -p /plan-dogrula (log: $STEP_LOG)"
		run_claude "/plan-dogrula $PLAN_PATH" "$STEP_LOG" || log "  claude (dogrulama) sifir olmayan cikis kodu dondurdu; Durum kontrol ediliyor."
		NEW_DURUM=$(plan_durum "$PLAN_PATH")
		if [ "$NEW_DURUM" = "$DURUM" ]; then
			NO_PROGRESS=$((NO_PROGRESS + 1))
			if [ "$NO_PROGRESS" -ge 2 ]; then
				append_blocker "STOP-02: /plan-dogrula art arda 2 kez '$PLAN_PATH' Durum'unu degistirmedi. Log: $STEP_LOG"
				exit 0
			fi
		else
			NO_PROGRESS=0
		fi
		;;
	DOĞRULANDI)
		CORRECTION_TURNS=0
		if ! next_plan; then
			append_blocker "'$PLAN_PATH' DOĞRULANDI; sıradaki plan yazılmadı (büyük olasılıkla faz sınırı: docs/STATUS.md 'faz onayı bekliyor' ve docs/phase-reports/ taslağına bakın)."
			exit 0
		fi
		;;
	UYGULANIYOR*)
		append_blocker "STOP-02: '$PLAN_PATH' UYGULANIYOR/BLOKE durumunda kaldi (opencode Durum'u guncellemeden bitmis olabilir)."
		exit 0
		;;
	REDDEDİLDİ)
		append_blocker "STOP-03: '$PLAN_PATH' REDDEDİLDİ."
		exit 0
		;;
	*)
		append_blocker "Bilinmeyen plan durumu '$DURUM' ($PLAN_PATH). Elle kontrol gerekir."
		exit 0
		;;
	esac
done
