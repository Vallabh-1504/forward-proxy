#!/usr/bin/env bash
# bench_hit.sh - Cache HIT sweep
# Each concurrency level is run REPEATS times (30 s each) so parse_results.py
# can compute the **median** across runs for a stable number.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"  # absolute path to this script's dir

PROXY="http://127.0.0.1:8080"
ORIGIN="http://127.0.0.1:8000/ping"
DURATION=15s
CONNECTIONS=(1 2 4 8 16 32 64)
REPEATS=10         # runs per concurrency level
RESULTSDIR="results"
mkdir -p "$RESULTSDIR"
OUTFILE="$RESULTSDIR/hit_$(date +%Y%m%d_%H%M%S).txt"

check_origin() {
    curl -sf --max-time 2 "$ORIGIN" > /dev/null || { echo "[ERROR] origin down"; exit 1; }
}
check_proxy() {
    curl -sf --max-time 2 -x "$PROXY" "$ORIGIN" > /dev/null || { echo "[ERROR] proxy down"; exit 1; }
}

# wrk requires -t <= -c. Use min(4, connections) threads.
threads_for() {
    local c=$1
    if [ "$c" -ge 4 ]; then echo 4; else echo "$c"; fi
}

echo "=== BENCH mode=hit started $(date) ===" | tee "$OUTFILE"

check_origin
check_proxy

for c in "${CONNECTIONS[@]}"; do
    t=$(threads_for "$c")
    for run in $(seq 1 "$REPEATS"); do
        echo ">>> mode=hit connections=$c threads=$t run=$run" | tee -a "$OUTFILE"
        # Warm the cache before every hit run
        curl -sf --max-time 5 -x "$PROXY" "$ORIGIN" > /dev/null
        BENCH_MODE=hit taskset -c 6-9 wrk -t"$t" -c"$c" -d"$DURATION" --latency \
            -s "$SCRIPT_DIR/bench.lua" "$PROXY" | tee -a "$OUTFILE"
        sleep 2   # let TIME_WAIT sockets drain between runs
    done
done

echo "=== BENCH mode=hit finished $(date) ===" | tee -a "$OUTFILE"
echo "Results in $OUTFILE"

echo
echo "━━━ Parsing results ━━━"
python3 "$(dirname "$0")/parse_results.py" "$OUTFILE"
