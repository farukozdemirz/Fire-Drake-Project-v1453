#!/usr/bin/env bash
# Otonom planla -> uygula -> dogrula dongusu (gece modu dahil). Tasarim: plans/OTONOM_DONGU.md
#
# GUVENLIK: Varsayilan olarak hicbir sey CALISTIRMAZ; yalnizca ne yapacagini
# yazip cikar (dry-run). Gercekten calismasi icin --run ZORUNLUDUR.
#
# Kullanim:
#   ./tools/auto-loop.sh                                     # dry-run
#   ./tools/auto-loop.sh --run                               # klasik mod (faz sinirinda durur, merge yok)
#   ./tools/auto-loop.sh --run --branch gece/2026-10-02 --target F5
#        # gece modu: dogrulanan plan dallari entegrasyon dalina (--branch) otomatik
#        # birlestirilir, faz sinirlari --target fazina kadar asilir, takilmalarda
#        # Claude "kurtarma" adimiyla karar verir. main'e dokunulmaz, push yapilmaz.
#
#   ./tools/auto-loop.sh --run --branch gece/2026-10-02-nav --target F5 --track nav --topic "..."
#        # paralel hat: AYRI bir git worktree'sinde ve ayri entegrasyon dalinda calisir;
#        # sunuculara dokunmaz (ana hat sunucu kullanabilir). Konu metni plan-olustur'a
#        # AUTO_TRACK_TOPIC olarak verilir (plans/OTONOM_DONGU.md §10).
#
# Durdurma: calisirken  touch plans/.auto-loop-stop  (sonraki adim basinda kontrol edilir)
# Durum:    cat plans/_logs/auto-loop.state ; tail -f plans/_logs/auto-loop.log

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# --- Ayarlar --------------------------------------------------------------
OPENCODE_MODEL="${OPENCODE_MODEL:-opencode-go/deepseek-v4.1-flash}"
CLAUDE_MODEL="${LOOP_CLAUDE_MODEL:-claude-sonnet-5-5}"
CLAUDE_EFFORT="${LOOP_CLAUDE_EFFORT:-high}"
MAX_CORRECTION_TURNS="${MAX_CORRECTION_TURNS:-4}"
MAX_ITERATIONS="${MAX_ITERATIONS:-400}"
MAX_WALLCLOCK_HOURS="${MAX_WALLCLOCK_HOURS:-9}"
HEARTBEAT_SEC="${HEARTBEAT_SEC:-600}"
OPENCODE_TIMEOUT_SEC="${OPENCODE_TIMEOUT_SEC:-7200}"
CLAUDE_TIMEOUT_SEC="${CLAUDE_TIMEOUT_SEC:-3600}"
CLAUDE_MAX_BUDGET_USD="${CLAUDE_MAX_BUDGET_USD:-40}"
MAX_RECOVERIES_PER_PLAN="${MAX_RECOVERIES_PER_PLAN:-2}"
MAX_TRANSIENT_RETRIES="${MAX_TRANSIENT_RETRIES:-3}"
TRANSIENT_SLEEP_SEC="${TRANSIENT_SLEEP_SEC:-300}"

INTEGRATION_BRANCH=""
TARGET_PHASE=""
TRACK=""
TOPIC=""

ACTIVE_PLAN_FILE="plans/.aktif-plan"
STOP_FILE="plans/.auto-loop-stop"
DONE_FILE="plans/.auto-loop-done"
LOG_DIR="plans/_logs"
MAIN_LOG="$LOG_DIR/auto-loop.log"
STATE_FILE="$LOG_DIR/auto-loop.state"
STATUS_FILE="docs/STATUS.md"

# Claude'a her cagrida verilen sabit yasaklar (birlestirmeyi yalnizca bu betik yapar).
DENY_TOOLS=("Bash(git push*)" "Bash(git merge*)" "Bash(git rebase*)" "Bash(git reset --hard*)" "Bash(git clean*)" "Bash(rm -rf*)")

RUN=false
while [ $# -gt 0 ]; do
	case "$1" in
	--run) RUN=true ;;
	--branch) INTEGRATION_BRANCH="${2:-}"; shift ;;
	--target) TARGET_PHASE="${2:-}"; shift ;;
	--track) TRACK="${2:-}"; shift ;;
	--topic) TOPIC="${2:-}"; shift ;;
	-h | --help)
		sed -n '2,21p' "$0"
		exit 0
		;;
	*) echo "Bilinmeyen arguman: $1" >&2; exit 2 ;;
	esac
	shift
done

NIGHT=false
[ -n "$INTEGRATION_BRANCH" ] && NIGHT=true

# Skill'ler bu degiskenlere bakarak insansiz/gece modunu bilir.
export AUTO_LOOP=1
export AUTO_INTEGRATION_BRANCH="$INTEGRATION_BRANCH"
export AUTO_TARGET_PHASE="$TARGET_PHASE"
export AUTO_TRACK="$TRACK"
export AUTO_TRACK_TOPIC="$TOPIC"

log() {
	mkdir -p "$LOG_DIR"
	printf '%s %s%s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "${TRACK:+[$TRACK] }" "$*" | tee -a "$MAIN_LOG"
}

state() { # tek satirlik canli durum
	mkdir -p "$LOG_DIR"
	printf '%s | iter=%s | plan=%s | %s\n' "$(date '+%H:%M:%S')" "${ITER:-0}" "${PLAN_PATH:-?}" "$*" >"$STATE_FILE"
}

append_blocker() {
	local reason="$1"
	log "BLOKER: $reason"
	$RUN || return 0
	[ -f "$STATUS_FILE" ] || return 0
	local row="| $(date '+%Y-%m-%d %H:%M') otonom döngü | $reason | Claude (sabah) | \`plans/_logs/auto-loop.log\` |"
	awk -v row="$row" '
		{print}
		/^## Blokajlar/ {found=1}
		found && /^\|---/ && !inserted {print row; inserted=1; found=0}
	' "$STATUS_FILE" >"$STATUS_FILE.tmp" && mv "$STATUS_FILE.tmp" "$STATUS_FILE"
	git add "$STATUS_FILE" >/dev/null 2>&1 && git commit -q -m "otonom döngü: blokaj kaydı" >/dev/null 2>&1 || true
}

# Planin GUNCEL metni. Durum/rapor plan dalinda commit'lenir; calisma agaci baska
# dalda olabilir. Oncelik: (1) su an plan dalindaysak calisma agaci (commit'lenmemis
# degisiklik dahil), (2) plan dali varsa o dal, (3) calisma agaci, (4) entegrasyon dali, (5) main.
plan_text() { # $1 = plan yolu
	local p="$1" base="" br cur
	cur="$(git branch --show-current 2>/dev/null)"
	if [ -f "$p" ]; then
		base="$(cat "$p")"
	elif [ -n "$INTEGRATION_BRANCH" ]; then
		base="$(git show "$INTEGRATION_BRANCH:$p" 2>/dev/null || true)"
	fi
	[ -z "$base" ] && base="$(git show "main:$p" 2>/dev/null || true)"
	br="$(printf '%s\n' "$base" | grep -m1 -E '^\| Branch \|' | grep -oE 'bot/[A-Za-z0-9._-]+' | head -1)"
	if [ -n "$br" ] && [ "$cur" != "$br" ] && git rev-parse --verify --quiet "$br" >/dev/null && git cat-file -e "$br:$p" 2>/dev/null; then
		git show "$br:$p"
		return
	fi
	printf '%s\n' "$base"
}

plan_durum() {
	local t
	t="$(plan_text "$1")"
	[ -n "$t" ] || { echo "YOK"; return; }
	local raw
	raw="$(printf '%s\n' "$t" | grep -m1 -E '^\| Durum \|' | sed -E 's/^\| Durum \| *([^|]*) *\|.*/\1/' | xargs)"
	# "DOĞRULANDI (2026-10-02, ...)" gibi sonradan eklenmis aciklamalari at; UYGULANIYOR (BLOKE) korunur.
	case "$raw" in
	UYGULANIYOR*) printf '%s\n' "$raw" ;;
	*) printf '%s\n' "$raw" | sed -E 's/ *[(—–].*$//' | xargs ;;
	esac
}

plan_branch() { # plan dosyasindaki bot/<FAZ>-<NN> dal adi
	plan_text "$1" | grep -m1 -E '^\| Branch \|' | grep -oE 'bot/[A-Za-z0-9._-]+' | head -1
}

stop_requested() { [ -f "$STOP_FILE" ]; }

# Dogrulayicinin yazdigi SON "Düzeltme talimatı" kod blogunu cikarir.
extract_correction() {
	plan_text "$1" | awk '
		/Düzeltme talimatı/ {f=1; c=0; buf=""; next}
		f && /^[[:space:]]*```/ {c++; if (c==2) {f=0; last=buf}; next}
		f && c==1 {buf = buf $0 "\n"}
		END {printf "%s", last}
	'
}

# Sunucular build/bin altindaki exe'leri kilitler; her adimdan once kapat.
ensure_servers_stopped() {
	# Paralel hat sunuculara dokunmaz: ana hat sunucuyu calisma zamani dogrulamasi icin kullaniyor olabilir.
	[ -n "$TRACK" ] && return 0
	local out
	out="$("$ROOT/tools/run-servers.sh" status 2>/dev/null || true)"
	if printf '%s' "$out" | grep -q '^\[UP\]'; then
		log "  sunucular acik; kapatiliyor (tools/run-servers.sh stop)"
		"$ROOT/tools/run-servers.sh" stop >>"$MAIN_LOG" 2>&1 || log "  UYARI: sunucular kapatilamadi (istemci bagli olabilir); derleme kilitlenebilir."
	fi
}

# Calisma agaci kirliyse kaybetmeden kenara koyar (sabah incelenebilir).
ensure_clean() {
	local dirty
	dirty="$(git status --porcelain 2>/dev/null | grep -v '^?? start.md$' || true)"
	if [ -n "$dirty" ]; then
		log "  calisma agaci kirli; git stash ile kenara aliniyor:"
		printf '%s\n' "$dirty" | sed 's/^/      /' | tee -a "$MAIN_LOG" >/dev/null
		git stash push -u -m "auto-loop $(date '+%Y-%m-%d %H:%M:%S') artiklari (${PLAN_PATH:-?})" >>"$MAIN_LOG" 2>&1 || true
	fi
}

switch_to() { # $1 = dal
	ensure_clean
	[ "$(git branch --show-current)" = "$1" ] && return 0
	git switch "$1" >>"$MAIN_LOG" 2>&1
}

# Claude kullanim limiti ("You've hit your session limit · resets 8:20pm") dolunca: reset saatine
# kadar uyur (deneme hakki harcamaz, stop dosyasi beklerken kontrol edilir). 0 = beklendi, 1 = beklenmedi.
LIMIT_WAIT_TOTAL=0
wait_for_limit_reset() { # $1 = claude cikti logu
	local t target now secs waited=0
	t="$(grep -oiE 'resets [0-9]{1,2}(:[0-9]{2})? ?[ap]m' "$1" | head -1 | sed -E 's/^resets //I')"
	now=$(date +%s)
	if [ -n "$t" ] && target=$(date -d "$t" +%s 2>/dev/null); then
		[ "$target" -le "$now" ] && target=$((target + 86400))
		secs=$((target - now + 120))
	else
		secs=1800
	fi
	[ "$secs" -gt 21600 ] && secs=21600
	if [ $((LIMIT_WAIT_TOTAL + secs)) -gt $((8 * 3600)) ]; then
		log "  kullanim limiti: toplam bekleme siniri (8 sa) asildi."
		return 1
	fi
	LIMIT_WAIT_TOTAL=$((LIMIT_WAIT_TOTAL + secs))
	log "  claude kullanim limiti doldu; sifirlanma (${t:-bilinmiyor}) icin ${secs}s bekleniyor."
	state "kullanim limiti bekleniyor (${t:-?})"
	while [ "$waited" -lt "$secs" ]; do
		stop_requested && return 1
		sleep 30
		waited=$((waited + 30))
	done
	return 0
}

# Gecici API hatalarinda (rate limit, overload) bekleyip yeniden dener.
run_claude() { # $1 = prompt, $2 = log dosyasi
	local attempt=1 rc start dur
	while true; do
		start=$(date +%s)
		timeout -k 60 "$CLAUDE_TIMEOUT_SEC" claude -p "$1" \
			--permission-mode bypassPermissions \
			--disallowedTools "${DENY_TOOLS[@]}" \
			--output-format json \
			--model "$CLAUDE_MODEL" \
			--effort "$CLAUDE_EFFORT" \
			--max-budget-usd "$CLAUDE_MAX_BUDGET_USD" \
			--no-session-persistence </dev/null >"$2" 2>&1
		rc=$?
		dur=$(($(date +%s) - start))
		if [ $rc -eq 0 ]; then return 0; fi
		if grep -qiE "hit your (session |usage |weekly )?limit|session limit|usage limit reached" "$2" && wait_for_limit_reset "$2"; then
			continue
		fi
		if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then
			log "  claude zaman asimina ugradi (${CLAUDE_TIMEOUT_SEC}s)."
			return $rc
		fi
		if [ $attempt -lt "$MAX_TRANSIENT_RETRIES" ] && { [ $dur -lt 120 ] || grep -qiE 'rate.?limit|overloaded|529|ECONN|timeout|usage limit' "$2"; }; then
			log "  claude cikis kodu $rc (${dur}s); gecici hata sayilip ${TRANSIENT_SLEEP_SEC}s sonra yeniden denenecek ($attempt/$MAX_TRANSIENT_RETRIES)."
			attempt=$((attempt + 1))
			sleep "$TRANSIENT_SLEEP_SEC"
			continue
		fi
		return $rc
	done
}

run_opencode() { # $1 = prompt, $2 = log dosyasi
	local attempt=1 rc start dur
	while true; do
		start=$(date +%s)
		timeout -k 60 "$OPENCODE_TIMEOUT_SEC" opencode run --auto --agent build -m "$OPENCODE_MODEL" "$1" </dev/null >"$2" 2>&1
		rc=$?
		dur=$(($(date +%s) - start))
		if [ $rc -eq 0 ]; then return 0; fi
		if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then
			log "  opencode zaman asimina ugradi (${OPENCODE_TIMEOUT_SEC}s)."
			return $rc
		fi
		if [ $attempt -lt "$MAX_TRANSIENT_RETRIES" ] && [ $dur -lt 120 ]; then
			log "  opencode cikis kodu $rc (${dur}s); ${TRANSIENT_SLEEP_SEC}s sonra yeniden denenecek ($attempt/$MAX_TRANSIENT_RETRIES)."
			attempt=$((attempt + 1))
			sleep "$TRANSIENT_SLEEP_SEC"
			continue
		fi
		return $rc
	done
}

# --- On kontroller --------------------------------------------------------
PRE_OK=true
pre() { echo "ON KONTROL HATASI: $1" >&2; PRE_OK=false; }

command -v opencode >/dev/null || pre "opencode komutu yok"
command -v claude >/dev/null || pre "claude komutu yok"
command -v timeout >/dev/null || pre "timeout komutu yok"
for f in AGENTS.md CLAUDE.md opencode.json plans/_SABLON.md docs/STATUS.md .claude/skills/plan-olustur/SKILL.md .claude/skills/plan-dogrula/SKILL.md tools/run-servers.sh; do
	[ -f "$f" ] || pre "$f yok"
done
for f in AGENTS.md CLAUDE.md opencode.json docs/STATUS.md plans/README.md tools/auto-loop.sh; do
	git ls-files --error-unmatch "$f" >/dev/null 2>&1 || pre "$f git'te takipli degil (once commit'leyin)"
done
if [ -n "$(git status --porcelain 2>/dev/null | grep -v '^?? start.md$' || true)" ]; then
	pre "calisma agaci temiz degil (git status). Once commit'leyin veya stash edin."
fi
if [ -n "$TRACK" ]; then
	[ "$(git rev-parse --git-dir 2>/dev/null)" != "$(git rev-parse --git-common-dir 2>/dev/null)" ] || pre "--track yalnizca AYRI bir git worktree'sinde calisir (git worktree add <yol> <dal>); ana calisma agacinda calistirmayin"
	$NIGHT || pre "--track gece modu (--branch) gerektirir"
	[ -n "$TOPIC" ] || pre "--track icin --topic zorunlu"
fi
if $NIGHT; then
	git rev-parse --verify --quiet "$INTEGRATION_BRANCH" >/dev/null || pre "entegrasyon dali '$INTEGRATION_BRANCH' yok (git branch $INTEGRATION_BRANCH main ile olusturun)"
	case "$INTEGRATION_BRANCH" in main | master) pre "entegrasyon dali main olamaz (gece modu main'e dokunmaz)" ;; esac
	[ -n "$TARGET_PHASE" ] || pre "gece modunda --target (or. F5) zorunlu"
fi

if ! $RUN; then
	cat <<EOF
=== DRY-RUN (gerçekten çalıştırmak için --run) ===
Mod: $($NIGHT && echo "GECE (entegrasyon dalı: $INTEGRATION_BRANCH, hedef faz: $TARGET_PHASE)" || echo "klasik (faz sınırında durur, merge yok)")
Şu anki dal: $(git branch --show-current)
Aktif plan: $( [ -f "$ACTIVE_PLAN_FILE" ] && echo "$(cat "$ACTIVE_PLAN_FILE") ($(plan_durum "$(cat "$ACTIVE_PLAN_FILE")"))" || echo "(yok: ilk adımda /plan-olustur)")
Ayarlar:
  OPENCODE_MODEL=$OPENCODE_MODEL  CLAUDE_MODEL=$CLAUDE_MODEL  CLAUDE_EFFORT=$CLAUDE_EFFORT
  MAX_CORRECTION_TURNS=$MAX_CORRECTION_TURNS  MAX_ITERATIONS=$MAX_ITERATIONS  MAX_WALLCLOCK_HOURS=$MAX_WALLCLOCK_HOURS
  OPENCODE_TIMEOUT_SEC=$OPENCODE_TIMEOUT_SEC  CLAUDE_TIMEOUT_SEC=$CLAUDE_TIMEOUT_SEC  CLAUDE_MAX_BUDGET_USD=$CLAUDE_MAX_BUDGET_USD
  MAX_RECOVERIES_PER_PLAN=$MAX_RECOVERIES_PER_PLAN  MAX_TRANSIENT_RETRIES=$MAX_TRANSIENT_RETRIES (bekleme ${TRANSIENT_SLEEP_SEC}s)
Claude yasakları: ${DENY_TOOLS[*]}
EOF
	if $PRE_OK; then echo "ÖN KONTROLLER: hepsi tamam."; else echo "ÖN KONTROLLER: BAŞARISIZ — --run reddedilir."; fi
	echo "Hiçbir komut çalıştırılmadı."
	exit 0
fi

$PRE_OK || exit 2

mkdir -p "$LOG_DIR"
rm -f "$DONE_FILE"
START_TS=$(date +%s)

# Windows'un uyku/bekleme moduna girmesini dongu boyunca engelle (bayrak dosyasi silinince biter).
KEEP_AWAKE_FLAG="$LOG_DIR/keep-awake.flag"
start_keep_awake() {
	command -v powershell.exe >/dev/null || return 0
	: >"$KEEP_AWAKE_FLAG"
	local wflag
	wflag="$(wslpath -w "$ROOT/$KEEP_AWAKE_FLAG")"
	nohup powershell.exe -NoProfile -NonInteractive -Command "Add-Type -MemberDefinition '[DllImport(\"kernel32.dll\")] public static extern uint SetThreadExecutionState(uint f);' -Name W -Namespace K; while (Test-Path -LiteralPath '$wflag') { [void][K.W]::SetThreadExecutionState(0x80000001); Start-Sleep -Seconds 30 }" >/dev/null 2>&1 &
	log "  uyku engelleyici baslatildi (bayrak: $KEEP_AWAKE_FLAG)"
}
stop_keep_awake() { rm -f "$KEEP_AWAKE_FLAG"; }
start_keep_awake

# Her HEARTBEAT_SEC saniyede (varsayilan 10 dk) hangi gorevin uygulandigini yazar.
HEARTBEAT_LOG="$LOG_DIR/heartbeat.log"
HEARTBEAT_FLAG="$LOG_DIR/heartbeat.flag"
heartbeat_loop() {
	while [ -f "$HEARTBEAT_FLAG" ]; do
		sleep "$HEARTBEAT_SEC"
		[ -f "$HEARTBEAT_FLAG" ] || break
		local st br last
		st="$(cat "$STATE_FILE" 2>/dev/null || echo '?')"
		br="$(git branch --show-current 2>/dev/null)"
		last="$(tail -n 1 "$MAIN_LOG" 2>/dev/null | cut -c1-160)"
		printf '%s HEARTBEAT | dal=%s | %s | gece dalinda %s commit | son log: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$br" "$st" "$(git rev-list --count main.."${INTEGRATION_BRANCH:-main}" 2>/dev/null || echo ?)" "$last" | tee -a "$HEARTBEAT_LOG" >>"$MAIN_LOG"
	done
}
start_heartbeat() { : >"$HEARTBEAT_FLAG"; heartbeat_loop & HEARTBEAT_PID=$!; }
stop_heartbeat() { rm -f "$HEARTBEAT_FLAG"; [ -n "${HEARTBEAT_PID:-}" ] && kill "$HEARTBEAT_PID" 2>/dev/null || true; }
start_heartbeat
ITER=0
PLAN_PATH=""
declare -A CORR=() RECOV=() IMPL_TRIES=() VERIFY_TRIES=()
FINALIZED=false

finalize() {
	$FINALIZED && return
	FINALIZED=true
	if ! $NIGHT; then stop_keep_awake; stop_heartbeat; return 0; fi
	log "=== Kapanis: sabah raporu yaziliyor ==="
	state "kapanis raporu"
	switch_to "$INTEGRATION_BRANCH" || true
	local rlog="$LOG_DIR/final-report-$(date +%s).log" rfile="docs/reports/gece-$(date +%Y-%m-%d)${TRACK:+-$TRACK}.md"
	run_claude "Otonom gece döngüsü bitti (neden: ${1:-bilinmiyor}). Entegrasyon dalı: $INTEGRATION_BRANCH (taban: main). Görev: $rfile dosyasını yaz (Türkçe, proje sahibi için sabah raporu): (1) bu gece hangi planlar yazıldı/uygulandı/doğrulandı/iptal edildi (git log main..$INTEGRATION_BRANCH ve plans/README.md'den), (2) hangi fazlar nerede kaldı, faz sonuç raporu taslakları, (3) Claude'un otonom verdiği kararlar (ADR'ler), (4) docs/STATUS.md 'Proje sahibi testleri (bekleyen)' listesi: sabah yapılacak istemci testleri adım adım, (5) blokajlar ve takılmalar (plans/_logs/auto-loop.log), (6) geri alma: main'e hiç dokunulmadı; her şey $INTEGRATION_BRANCH dalında; birleştirme komutu. Dosyayı ve STATUS güncellemesini $INTEGRATION_BRANCH dalına commit et. Başka dosya değiştirme, push/merge yapma." "$rlog" || log "  kapanis raporu yazilamadi (log: $rlog)"
	ensure_clean
	state "bitti: ${1:-?}"
	stop_keep_awake
	stop_heartbeat
	log "=== auto-loop.sh bitti ==="
}
trap 'finalize "beklenmeyen cikis"' EXIT

stop_loop() { # $1 = neden
	log "DUR: $1"
	finalize "$1"
	exit 0
}

# Claude'a takilmayi cozdurur. 0 = devam edilebilir, 1 = tukendi.
recover() { # $1 = neden
	local key="${PLAN_PATH:-none}" rlog
	RECOV[$key]=$(( ${RECOV[$key]:-0} + 1 ))
	if [ "${RECOV[$key]}" -gt "$MAX_RECOVERIES_PER_PLAN" ]; then
		append_blocker "'$PLAN_PATH' için kurtarma hakkı bitti ($MAX_RECOVERIES_PER_PLAN). Son neden: $1"
		return 1
	fi
	rlog="$LOG_DIR/recover-$(basename "${PLAN_PATH:-none}" .md)-$(date +%s).log"
	log "  -> KURTARMA (${RECOV[$key]}/$MAX_RECOVERIES_PER_PLAN): $1 (log: $rlog)"
	state "kurtarma: $1"
	ensure_servers_stopped
	run_claude "OTONOM DÖNGÜ KURTARMA ADIMI. Sen bu projede planlayıcı ve denetçisin (CLAUDE.md); proje sahibi uyuyor, karar yetkisi sende, soru sorma. Döngü şu planda takıldı: ${PLAN_PATH:-yok}. Durum: $(plan_durum "${PLAN_PATH:-/dev/null}"). Neden: $1. Son adım logları plans/_logs/ altında (en yenileri), ana log plans/_logs/auto-loop.log. Entegrasyon dalı: ${INTEGRATION_BRANCH:-yok}, hedef faz: ${TARGET_PHASE:-yok}. Görevin: durumu incele ve döngünün ilerleyebilmesi için TEK bir çözüm uygula: (a) plan yanlış/eksik/çok büyükse planı düzelt veya küçült ve Durum'u HAZIR yap (gerekirse plan dalını git switch ile bırak, kodu düzeltme); (b) DeepSeek'in yapması gereken net bir iş varsa plan dosyasının sonuna yeni bir 'Düzeltme talimatı' kod bloğu yaz ve Durum'u DÜZELTME GEREKLİ yap; (c) plan bu gece yapılamayacaksa (istemci/insan gerektiriyor, tasarım hatalı) Durum'u İPTAL yap, nedenini planın sonuna ve docs/STATUS.md'ye yaz, insan gerektiren kısmı STATUS 'Proje sahibi testleri (bekleyen)' bölümüne ekle; (d) BLOKE sorusu varsa cevabını sen ver (gerekirse ADR, başlığına '(otonom döngüde Claude kararı — gözden geçirilmeli)'), cevabı plana yaz ve Durum'u HAZIR veya DÜZELTME GEREKLİ yap. Plan dosyasındaki değişiklikleri (Durum, düzeltme talimatı, plan metni) planın kendi dalı ($(plan_branch "${PLAN_PATH:-/dev/null}")) varsa O DALA commit et (döngü durumu oradan okur); plan dalı yoksa ${INTEGRATION_BRANCH:-mevcut dal} dalına. İş bitince git switch ${INTEGRATION_BRANCH:-main} ile dön. Çalışma ağacını temiz bırak. push/merge/rebase/reset yapma." "$rlog" || log "  kurtarma claude cagrisi sifir olmayan kodla bitti."
	return 0
}

# Kuyruk (plans/.queue, git'te degil): her satir bir plan yolu. Onceden yazilmis HAZIR planlari
# /plan-olustur cagirmadan (Claude kullanimi harcamadan) sirayla aktif yapar. Yazilan satir kuyruktan duser;
# HAZIR olmayan / bulunamayan satirlar kuyrukta kalir ve atlanir. 0 = yolu yazdirdi, 1 = kuyruk bos.
pop_queued_plan() {
	local q="plans/.queue" p st
	[ -f "$q" ] || return 1
	while IFS= read -r p || [ -n "$p" ]; do
		p="$(printf '%s' "$p" | tr -d '\r' | sed -E 's/[[:space:]]+$//')"
		case "$p" in '' | '#'*) continue ;; esac
		[ -f "$p" ] || { log "  kuyruk: $p bulunamadi, atlandi" >&2; continue; }
		st="$(plan_durum "$p")"
		if [ "$st" = "HAZIR" ]; then
			awk -v t="$p" '{ l=$0; sub(/\r$/, "", l); if (l != t) print $0 }' "$q" >"$q.tmp" && mv "$q.tmp" "$q"
			printf '%s' "$p"
			return 0
		fi
		log "  kuyruk: $p durumu '$st' (HAZIR degil), atlandi" >&2
	done <"$q"
	return 1
}

next_plan() { # 0 = yeni plan hazir, 1 = yazilmadi, 2 = hedef tamam
	local before after slog try qp
	if $NIGHT && switch_to "$INTEGRATION_BRANCH" && qp="$(pop_queued_plan)"; then
		printf '%s' "$qp" >"$ACTIVE_PLAN_FILE"
		log "  -> kuyruktan plan secildi: $qp"
		return 0
	fi
	for try in 1 2; do
		before="$(cat "$ACTIVE_PLAN_FILE" 2>/dev/null || true)"
		$NIGHT && { switch_to "$INTEGRATION_BRANCH" || { append_blocker "entegrasyon dalina gecilemedi"; return 1; }; }
		ensure_servers_stopped
		slog="$LOG_DIR/next-plan-iter$ITER-$(date +%s).log"
		state "plan yaziliyor (/plan-olustur, deneme $try)"
		log "  -> claude -p /plan-olustur (deneme $try, log: $slog)"
		if [ "$try" -eq 1 ]; then
			run_claude "/plan-olustur" "$slog" || log "  claude (plan-olustur) sifir olmayan cikis kodu."
		else
			run_claude "/plan-olustur — ÖNCEKİ ÇAĞRI PLAN YAZMADI. Önce plans/_logs/ altındaki en yeni next-plan logunu ve docs/STATUS.md'yi incele. Hedef faza (${TARGET_PHASE:-yok}) kadar DeepSeek'in yapabileceği iş kaldıysa sıradaki planı yaz (gerekirse fazı geç, insan testlerini STATUS 'Proje sahibi testleri (bekleyen)' listesine ekle). Gerçekten hiçbir iş kalmadıysa plans/.auto-loop-done dosyasına tek satır neden yaz." "$slog" || log "  claude (plan-olustur, deneme 2) sifir olmayan cikis kodu."
		fi
		[ -f "$DONE_FILE" ] && return 2
		after="$(cat "$ACTIVE_PLAN_FILE" 2>/dev/null || true)"
		if [ -n "$after" ] && [ "$after" != "$before" ] && [ -f "$after" ]; then
			return 0
		fi
		log "  /plan-olustur yeni aktif plan uretmedi (deneme $try)."
	done
	return 1
}

merge_into_integration() { # $1 = plan yolu; 0 = tamam
	local br conflicts attempt rlog
	br="$(plan_branch "$1")"
	if [ -z "$br" ] || ! git rev-parse --verify --quiet "$br" >/dev/null; then
		log "  plan dali bulunamadi ('$br'); birlestirme atlandi."
		return 0
	fi
	if git merge-base --is-ancestor "$br" "$INTEGRATION_BRANCH"; then
		return 0
	fi
	switch_to "$INTEGRATION_BRANCH" || return 1
	log "  -> git merge --no-ff $br -> $INTEGRATION_BRANCH"
	if git merge --no-ff "$br" -m "Merge $br ($INTEGRATION_BRANCH, otonom gece döngüsü)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>" >>"$MAIN_LOG" 2>&1; then
		return 0
	fi
	# Cakisma (genelde docs/STATUS.md, plans/README.md tablo satirlari): Claude cozer.
	for attempt in 1 2; do
		conflicts="$(git diff --name-only --diff-filter=U | tr '\n' ' ')"
		[ -n "$conflicts" ] || break
		rlog="$LOG_DIR/merge-resolve-$(basename "$br")-$attempt-$(date +%s).log"
		log "  birlestirme cakismasi ($conflicts); Claude cozuyor (deneme $attempt, log: $rlog)"
		state "birlestirme cakismasi cozuluyor"
		run_claude "BİRLEŞTİRME ÇAKIŞMASI ÇÖZÜMÜ (otonom döngü). Şu an $INTEGRATION_BRANCH dalında '$br' dalını birleştirirken yarım kalmış bir merge var. Çakışan dosyalar: $conflicts. Görev: her dosyadaki çakışma işaretlerini (<<<<<<<, =======, >>>>>>>) kaldır. Kural: entegrasyon dalının (HEAD, 'ours') içeriğini temel al; '$br' dalının getirdiği yeni bilgiyi KAYBETME: tablo satırlarında ikisinin birleşimini al (aynı satır iki tarafta da varsa durumu daha ilerisini seç: KAPANDI > DOĞRULANDI > UYGULANDI > HAZIR), 'Son doğrulamalar' satırlarını ve 'Sıradaki adımlar'ı entegrasyon dalındakini temel alarak güncelle, 'Proje sahibi testleri (bekleyen)' bölümünü iki taraftan birleştir. Kod/betik dosyası çakışırsa iki tarafın değişikliğini birlikte koru ve derlenebilir bırak. Sonra çakışan dosyaları 'git add' ile ekle. 'git commit', 'git merge --abort' veya başka merge komutu ÇALIŞTIRMA (commit'i döngü yapar). Çalışma ağacında başka dosyaya dokunma." "$rlog" || log "  cozum claude cagrisi sifir olmayan kodla bitti."
		if [ -z "$(git diff --name-only --diff-filter=U)" ] && ! git grep -n -E '^(<<<<<<<|>>>>>>>) ' -- $conflicts >/dev/null 2>&1; then
			if git commit --no-edit >>"$MAIN_LOG" 2>&1; then
				log "  cakisma cozuldu, birlestirme commit'lendi."
				return 0
			fi
		fi
		log "  cakisma tam cozulemedi (deneme $attempt)."
	done
	git merge --abort >>"$MAIN_LOG" 2>&1 || true
	return 1
}

log "=== auto-loop.sh basladi (mod: $($NIGHT && echo "gece, dal=$INTEGRATION_BRANCH, hedef=$TARGET_PHASE" || echo klasik), model: $OPENCODE_MODEL / $CLAUDE_MODEL ($CLAUDE_EFFORT)) ==="
ensure_servers_stopped

while true; do
	stop_requested && { rm -f "$STOP_FILE"; stop_loop "$STOP_FILE bulundu (elle durdurma)"; }
	[ $(($(date +%s) - START_TS)) -ge $((MAX_WALLCLOCK_HOURS * 3600)) ] && stop_loop "MAX_WALLCLOCK_HOURS ($MAX_WALLCLOCK_HOURS) doldu"
	ITER=$((ITER + 1))
	[ "$ITER" -gt "$MAX_ITERATIONS" ] && stop_loop "MAX_ITERATIONS ($MAX_ITERATIONS) doldu"

	PLAN_PATH="$(cat "$ACTIVE_PLAN_FILE" 2>/dev/null || true)"
	DURUM="$(plan_durum "${PLAN_PATH:-/nonexistent}")"
	[ -z "$PLAN_PATH" ] && DURUM="YOK"
	log "Iterasyon $ITER - plan: ${PLAN_PATH:-yok} - durum: $DURUM"
	PLAN_BASE="$(basename "${PLAN_PATH:-none}" .md)"
	STEP_LOG="$LOG_DIR/$PLAN_BASE-iter$ITER-$(date +%s).log"
	state "durum $DURUM"

	case "$DURUM" in
	YOK | DOĞRULANDI | KAPANDI* | İPTAL*)
		if [ "$DURUM" = "DOĞRULANDI" ]; then
			if $NIGHT; then
				if ! merge_into_integration "$PLAN_PATH"; then
					append_blocker "'$PLAN_PATH' entegrasyon dalına birleşirken çakıştı (merge --abort yapıldı)."
					stop_loop "birlestirme cakismasi"
				fi
			else
				stop_loop "'$PLAN_PATH' DOĞRULANDI (klasik mod: birleştirme proje sahibinde)"
			fi
		fi
		next_plan
		case $? in
		0) continue ;;
		2) stop_loop "hedef tamamlandı: $(cat "$DONE_FILE" 2>/dev/null)" ;;
		*)
			$NIGHT || stop_loop "/plan-olustur plan yazmadi (faz siniri olabilir)"
			append_blocker "/plan-olustur iki denemede de plan yazmadı."
			stop_loop "plan yazilamadi"
			;;
		esac
		;;
	HAZIR | "DÜZELTME GEREKLİ")
		BR="$(plan_branch "$PLAN_PATH")"
		if [ "$DURUM" = "HAZIR" ]; then
			IMPL_TRIES[$PLAN_PATH]=$(( ${IMPL_TRIES[$PLAN_PATH]:-0} + 1 ))
			if [ "${IMPL_TRIES[$PLAN_PATH]}" -gt 2 ]; then
				recover "DeepSeek planı iki kez denedi ama Durum HAZIR'da kaldı" || stop_loop "kurtarma tukendi"
				IMPL_TRIES[$PLAN_PATH]=0
				continue
			fi
			if [ -n "$BR" ] && git rev-parse --verify --quiet "$BR" >/dev/null; then
				switch_to "$BR"
				PROMPT="$PLAN_PATH planını AGENTS.md kurallarına göre uygula. Plan dalı ($BR) zaten var ve şu an onun üzerindesin: yeni dal açma, kaldığı yerden devam et."
			else
				$NIGHT && switch_to "$INTEGRATION_BRANCH"
				PROMPT="$PLAN_PATH planını AGENTS.md kurallarına göre uygula."
			fi
		else
			CORR[$PLAN_PATH]=$(( ${CORR[$PLAN_PATH]:-0} + 1 ))
			if [ "${CORR[$PLAN_PATH]}" -gt "$MAX_CORRECTION_TURNS" ]; then
				recover "$MAX_CORRECTION_TURNS düzeltme turundan sonra hâlâ DOĞRULANDI değil; planı küçült, iptal et veya son bir net talimat yaz" || stop_loop "kurtarma tukendi"
				CORR[$PLAN_PATH]=0
				continue
			fi
			PROMPT="$(extract_correction "$PLAN_PATH")"
			if [ -z "$PROMPT" ]; then
				recover "DÜZELTME GEREKLİ ama 'Düzeltme talimatı' kod bloğu bulunamadı" || stop_loop "kurtarma tukendi"
				continue
			fi
			[ -n "$BR" ] && git rev-parse --verify --quiet "$BR" >/dev/null && switch_to "$BR"
		fi
		ensure_servers_stopped
		state "DeepSeek uyguluyor ($DURUM)"
		log "  -> opencode run (log: $STEP_LOG)"
		run_opencode "$PROMPT" "$STEP_LOG" || log "  opencode sifir olmayan cikis kodu; Durum kontrol ediliyor."
		NEW_DURUM="$(plan_durum "$PLAN_PATH")"
		log "  opencode sonrasi durum: $NEW_DURUM"
		if [ "$NEW_DURUM" = "$DURUM" ] && [ "$DURUM" = "DÜZELTME GEREKLİ" ]; then
			recover "DeepSeek düzeltme turunu bitirdi ama Durum hâlâ DÜZELTME GEREKLİ (rapor/durum güncellenmemiş)" || stop_loop "kurtarma tukendi"
		fi
		;;
	UYGULANIYOR*)
		if printf '%s' "$DURUM" | grep -q BLOKE; then
			recover "DeepSeek planı BLOKE olarak işaretledi (soru/engel plan raporunda)" || stop_loop "kurtarma tukendi"
		else
			IMPL_TRIES[$PLAN_PATH]=$(( ${IMPL_TRIES[$PLAN_PATH]:-0} + 1 ))
			if [ "${IMPL_TRIES[$PLAN_PATH]}" -gt 2 ]; then
				recover "plan UYGULANIYOR'da kaldı (DeepSeek bitirmeden çıktı)" || stop_loop "kurtarma tukendi"
				IMPL_TRIES[$PLAN_PATH]=0
				continue
			fi
			BR="$(plan_branch "$PLAN_PATH")"
			[ -n "$BR" ] && git rev-parse --verify --quiet "$BR" >/dev/null && switch_to "$BR"
			ensure_servers_stopped
			state "DeepSeek devam ediyor"
			log "  -> opencode run (devam, log: $STEP_LOG)"
			run_opencode "$PLAN_PATH planının uygulaması yarım kalmış (Durum: UYGULANIYOR). Plan dalındasın; AGENTS.md kurallarına göre eksik adımları tamamla, derle, Uygulayıcı Raporu'nu yaz, commit et ve Durum'u UYGULANDI yap." "$STEP_LOG" || true
		fi
		;;
	UYGULANDI)
		VERIFY_TRIES[$PLAN_PATH]=$(( ${VERIFY_TRIES[$PLAN_PATH]:-0} + 1 ))
		if [ "${VERIFY_TRIES[$PLAN_PATH]}" -gt 2 ]; then
			recover "/plan-dogrula iki kez çalıştı ama Durum UYGULANDI'da kaldı" || stop_loop "kurtarma tukendi"
			VERIFY_TRIES[$PLAN_PATH]=0
			continue
		fi
		ensure_servers_stopped
		BR="$(plan_branch "$PLAN_PATH")"
		if [ -n "$BR" ] && git rev-parse --verify --quiet "$BR" >/dev/null; then
			switch_to "$BR" || log "  UYARI: $BR dalina gecilemedi"
		fi
		state "Claude dogruluyor"
		log "  -> claude -p /plan-dogrula (dal: $(git branch --show-current), log: $STEP_LOG)"
		run_claude "/plan-dogrula $PLAN_PATH" "$STEP_LOG" || log "  claude (dogrulama) sifir olmayan cikis kodu; Durum kontrol ediliyor."
		ensure_servers_stopped
		if $NIGHT; then switch_to "$INTEGRATION_BRANCH" || log "  UYARI: entegrasyon dalina donulemedi"; fi
		log "  dogrulama sonrasi durum: $(plan_durum "$PLAN_PATH")"
		;;
	TASLAK | REDDEDİLDİ | *)
		recover "plan durumu '$DURUM' (TASLAK/REDDEDİLDİ/bilinmeyen): planı tamamla, değiştir veya iptal et" || stop_loop "kurtarma tukendi"
		;;
	esac
done
