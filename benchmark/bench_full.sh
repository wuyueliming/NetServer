#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULT_DIR="${SCRIPT_DIR}/results"
mkdir -p "$RESULT_DIR"
cd "$SCRIPT_DIR"
WEB="python3 ${SCRIPT_DIR}/bench_client.py"
RESULT_FILE="$RESULT_DIR/full_bench_$(date +%Y%m%d_%H%M%S).txt"

log() { echo "$(date '+%H:%M:%S') $*" | tee -a "$RESULT_FILE"; }

run_bench() {
    local label="$1" url="$2" clients="$3" duration="${4:-20}" extra="${5:-}"
    log "  [$label] c=$clients t=${duration}s $extra"
    "$WEB" -t "$duration" -c "$clients" $extra "$url" 2>&1 | tee -a "$RESULT_FILE"
    log ""
}

# Start server
log "Starting bench_server..."
./bench_server >/dev/null 2>&1 &
SPID=$!
sleep 2
curl -s http://localhost:8080/ping >/dev/null || { log "FAILED to start"; exit 1; }
log "Server OK (PID=$SPID)"

log "============================================================"
log " muduo HTTP Server Benchmark | 4-core Xeon 8255C @2.5GHz | 3.6GB"
log "============================================================"

# === BATCH 1: Throughput Ladder ===
log ""; log "### BATCH 1: Throughput vs Concurrency (/hello, /ping, /) ###"

log "--- /hello (12B response) ---"
for c in 10 50 100 200 500 1000 2000 5000 10000; do
    run_bench "hello" "http://localhost:8080/hello" "$c" 20 "-2"
done

log "--- /ping (4B response) ---"
for c in 100 500 1000 2000 5000 10000; do
    run_bench "ping" "http://localhost:8080/ping" "$c" 20 "-2"
done

log "--- Static File / (index.html, ~1KB) ---"
for c in 10 50 100 200 500 1000 2000 5000; do
    run_bench "static" "http://localhost:8080/" "$c" 20 "-2"
done

# === BATCH 2: Payload & Protocol ===
log ""; log "### BATCH 2: Payload Size & Protocol Impact ###"

log "--- /data (1KB response) ---"
for c in 10 50 100 200 500 1000 2000 5000; do
    run_bench "data-1KB" "http://localhost:8080/data" "$c" 20 "-2"
done

log "--- /hello HTTP/1.0 (no keep-alive) ---"
for c in 100 500 1000 2000 5000; do
    run_bench "hello-HTTP10" "http://localhost:8080/hello" "$c" 20 "-1"
done

log "--- /hello Force Mode (don't wait for reply) ---"
for c in 100 500 1000 2000 5000 10000; do
    run_bench "hello-force" "http://localhost:8080/hello" "$c" 20 "-2 -f"
done

# === BATCH 3: Stability & Extreme Load ===
log ""; log "### BATCH 3: Stability & Extreme Load ###"

log "--- /hello c=1000 t=60s ---"
run_bench "stable-60s" "http://localhost:8080/hello" 1000 60 "-2"

log "--- /hello c=1000 t=120s ---"
run_bench "stable-120s" "http://localhost:8080/hello" 1000 120 "-2"

log "--- Extreme Concurrency /ping ---"
for c in 5000 10000 15000 20000; do
    run_bench "extreme-$c" "http://localhost:8080/ping" "$c" 15 "-2"
done

# Cleanup
log "Stopping server (PID=$SPID)..."
kill $SPID 2>/dev/null; wait $SPID 2>/dev/null
log "DONE. Results: $RESULT_FILE"
echo "RESULT=$RESULT_FILE"
