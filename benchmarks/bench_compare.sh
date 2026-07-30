#!/usr/bin/env bash
# bench_compare.sh — paired/interleaved benchmark of two proxy binaries.
#
# Pattern per concurrency level:
#   run-1: [baseline] → [challenger]
#   run-2: [challenger] → [baseline]   <-- order flips to cancel warm-up bias
#   run-3: [baseline] → [challenger]
#
# This means OS drift, thermal state, and cache effects hit BOTH binaries
# equally across runs. The median of each set, the difference is real.
#
# Usage:
#   ./bench_compare.sh --baseline ./build_base/proxy --challenger ./build_new/proxy
#
# Optional flags:
#   --mode    hit|miss          (default: hit)
#   --label   some_name         (used in output filenames, default: compare)
#   --repeats N                 (default: 10, use odd number for clean median)
#   --duration Xs               (default: 30s)
#   --conns   "1 2 4 8 16 32"   (default: standard sweep)
#   --proxy-args "..."          (extra args forwarded to both binaries)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"  # absolute path to this script's dir

# ── Defaults ──────────────────────────────────────────────────────────────────
BASELINE=""
CHALLENGER=""
MODE="hit"
LABEL="compare"
REPEATS=10
DURATION=15s
CONNECTIONS=(8 64)  # low + high; override with --conns "4 32" etc.
PROXY_ARGS=""
BASELINE_PORT=8080    # baseline always on this port
CHALLENGER_PORT=8081  # challenger always on this port — no stop/start conflict
ORIGIN_URL="http://127.0.0.1:8000/ping"
RESULTSDIR="results"
PROXY_CORES="2-5"   # CPU cores pinned to the proxy process
WRK_CORES="6-9"    # CPU cores pinned to wrk

# ── Argument parsing ──────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case $1 in
        --baseline)   BASELINE="$2";   shift 2 ;;
        --challenger) CHALLENGER="$2"; shift 2 ;;
        --mode)       MODE="$2";       shift 2 ;;
        --label)      LABEL="$2";      shift 2 ;;
        --repeats)    REPEATS="$2";    shift 2 ;;
        --duration)   DURATION="$2";   shift 2 ;;
        --conns)      read -ra CONNECTIONS <<< "$2"; shift 2 ;;
        --proxy-args) PROXY_ARGS="$2"; shift 2 ;;
        *) echo "[ERROR] unknown flag: $1"; exit 1 ;;
    esac
done

if [[ -z "$BASELINE" || -z "$CHALLENGER" ]]; then
    echo "Usage: $0 --baseline <binary> --challenger <binary> [options]"
    echo ""
    echo "  --mode       hit|miss            (default: hit)"
    echo "  --label      NAME                (used in output filenames)"
    echo "  --repeats    N                   (default: 10)"
    echo "  --duration   Xs                  (default: 30s)"
    echo "  --conns      \"1 2 4 8 16 32 64\"  (concurrency sweep)"
    echo "  --proxy-args \"...\"               (forwarded to both binaries)"
    exit 1
fi

if [[ ! -x "$BASELINE" ]];   then echo "[ERROR] not executable: $BASELINE";   exit 1; fi
if [[ ! -x "$CHALLENGER" ]]; then echo "[ERROR] not executable: $CHALLENGER"; exit 1; fi

# ── Setup ─────────────────────────────────────────────────────────────────────
mkdir -p "$RESULTSDIR"
TS=$(date +%Y%m%d_%H%M%S)
OUTFILE_A="$RESULTSDIR/${LABEL}_baseline_${TS}.txt"
OUTFILE_B="$RESULTSDIR/${LABEL}_challenger_${TS}.txt"

threads_for() {
    local c=$1
    if [ "$c" -ge 4 ]; then echo 4; else echo "$c"; fi
}

# Start a binary on a given port, wait for it to be ready, return its PID.
# Usage: start_proxy <binary> <port>
start_proxy() {
    local bin="$1" port="$2"
    local addr="127.0.0.1:$port"
    # shellcheck disable=SC2086
    # Redirect stdout/stderr so proxy logs don't fill the $() pipe buffer (deadlock)
    taskset -c "$PROXY_CORES" "$bin" "$port" $PROXY_ARGS > /dev/null 2>&1 &
    local pid=$!
    # Wait until the proxy is accepting connections (up to 5 s)
    local attempts=0
    until curl -sf --max-time 1 -x "http://$addr" "$ORIGIN_URL" > /dev/null 2>&1; do
        sleep 0.2
        attempts=$((attempts + 1))
        if [[ $attempts -ge 25 ]]; then
            echo "[ERROR] proxy ($bin port=$port, pid=$pid) did not start in 5s"
            kill "$pid" 2>/dev/null || true
            exit 1
        fi
    done
    echo "$pid"
}

stop_proxy() {
    local pid=$1
    kill -9 "$pid" 2>/dev/null || true
    # Poll until the process is actually gone (kill -9 is instant but kernel
    # cleanup takes a moment). This is more reliable than a fixed sleep.
    local attempts=0
    while kill -0 "$pid" 2>/dev/null; do
        sleep 0.1
        attempts=$((attempts + 1))
        if [[ $attempts -ge 30 ]]; then
            echo "[WARN] process $pid still alive after 3s — continuing anyway"
            break
        fi
    done
    sleep 0.5   # small grace period for the OS to fully release the port
}

# Usage: run_wrk <connections> <outfile> <port>
run_wrk() {
    local c=$1 outfile=$2 port=$3
    local t addr
    t=$(threads_for "$c")
    addr="127.0.0.1:$port"
    if [[ "$MODE" == "hit" ]]; then
        curl -sf --max-time 5 -x "http://$addr" "$ORIGIN_URL" > /dev/null
    fi
    BENCH_MODE="$MODE" taskset -c "$WRK_CORES" wrk \
        -t"$t" -c"$c" -d"$DURATION" --latency \
        -s "$SCRIPT_DIR/bench.lua" "http://$addr" | tee -a "$outfile"
}

# ── Sanity check ──────────────────────────────────────────────────────────────
echo "=== COMPARE mode=$MODE started $(date) ===" | tee "$OUTFILE_A" | tee "$OUTFILE_B" > /dev/null
echo "    baseline:   $BASELINE"   | tee -a "$OUTFILE_A" "$OUTFILE_B"
echo "    challenger: $CHALLENGER" | tee -a "$OUTFILE_A" "$OUTFILE_B"
echo "    repeats:    $REPEATS × $DURATION per binary per concurrency level" | tee -a "$OUTFILE_A" "$OUTFILE_B"

curl -sf --max-time 2 "$ORIGIN_URL" > /dev/null \
    || { echo "[ERROR] origin not reachable at $ORIGIN_URL"; exit 1; }
echo "    ports:      baseline=$BASELINE_PORT  challenger=$CHALLENGER_PORT"

# ── Main loop ─────────────────────────────────────────────────────────────────
for c in "${CONNECTIONS[@]}"; do
    echo ""
    echo "════════ connections=$c ════════"

    for run in $(seq 1 "$REPEATS"); do

        # Flip order every run: even runs → challenger first
        if (( run % 2 == 1 )); then
            FIRST="baseline";    FIRST_BIN="$BASELINE";    FIRST_PORT="$BASELINE_PORT";    FIRST_FILE="$OUTFILE_A"
            SECOND="challenger"; SECOND_BIN="$CHALLENGER"; SECOND_PORT="$CHALLENGER_PORT"; SECOND_FILE="$OUTFILE_B"
        else
            FIRST="challenger"; FIRST_BIN="$CHALLENGER"; FIRST_PORT="$CHALLENGER_PORT"; FIRST_FILE="$OUTFILE_B"
            SECOND="baseline";  SECOND_BIN="$BASELINE";  SECOND_PORT="$BASELINE_PORT";  SECOND_FILE="$OUTFILE_A"
        fi

        # ── First binary ──
        echo ">>> mode=$MODE connections=$c threads=$(threads_for "$c") run=$run version=$FIRST" \
            | tee -a "$FIRST_FILE"
        PID=$(start_proxy "$FIRST_BIN" "$FIRST_PORT")
        run_wrk "$c" "$FIRST_FILE" "$FIRST_PORT"
        stop_proxy "$PID"

        sleep 1

        # ── Second binary ──
        echo ">>> mode=$MODE connections=$c threads=$(threads_for "$c") run=$run version=$SECOND" \
            | tee -a "$SECOND_FILE"
        PID=$(start_proxy "$SECOND_BIN" "$SECOND_PORT")
        run_wrk "$c" "$SECOND_FILE" "$SECOND_PORT"
        stop_proxy "$PID"

        sleep 1
    done
done

# ── Results ───────────────────────────────────────────────────────────────────
echo ""
echo "=== COMPARE mode=$MODE finished $(date) ===" | tee -a "$OUTFILE_A" "$OUTFILE_B"
echo "Baseline results   → $OUTFILE_A"
echo "Challenger results → $OUTFILE_B"

echo ""
echo "━━━ Baseline ━━━"
python3 "$(dirname "$0")/parse_results.py" "$OUTFILE_A" --label "${LABEL}_baseline"

echo ""
echo "━━━ Challenger ━━━"
python3 "$(dirname "$0")/parse_results.py" "$OUTFILE_B" --label "${LABEL}_challenger"
