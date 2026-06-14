#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULT_DIR="${SCRIPT_DIR}/results"
mkdir -p "$RESULT_DIR"
cd "$SCRIPT_DIR"

WEB="python3 ${SCRIPT_DIR}/bench_client.py"

# 替换 webbench 参数为 bench_client.py 参数
run_bench() {
    local label="$1"
    local url="$2"
    local clients="$3"
    local duration="${4:-30}"
    local extra="${5:-}"

    local http_ver="-1"
    local force=""
    for arg in $extra; do
        [ "$arg" = "-2" ] && http_ver="-2"
        [ "$arg" = "-f" ] && force="-f"
    done

    log "  [$label] clients=$clients time=${duration}s ${extra}"
    local output
    output=$($WEB -t "$duration" -c "$clients" $http_ver $force "$url" 2>&1)
    log "  $(echo "$output" | grep -v "^CSV:" | head -5)"
    echo "$output"
}
RESULT_DIR="/tmp/bench_results"
mkdir -p "$RESULT_DIR"
RESULT_FILE="$RESULT_DIR/bench_report_$(date +%Y%m%d_%H%M%S).txt"

log() {
    echo "$(date '+%H:%M:%S') $*" | tee -a "$RESULT_FILE"
}

# Start server in background
log "Starting bench_server..."
./bench_server > /dev/null 2>&1 &
SERVER_PID=$!
log "Server PID: $SERVER_PID"
sleep 2

# Verify server is alive
if ! curl -s -o /dev/null http://localhost:8080/ping; then
    log "ERROR: Server failed to start!"
    kill $SERVER_PID 2>/dev/null
    exit 1
fi
log "Server is alive."

###############################################################################
log ""
log "============================================================"
log "  muduo HTTP Server - Comprehensive Performance Benchmark"
log "  Server: 4-core Intel Xeon Platinum 8255C @ 2.50GHz, 3.6GB RAM"
log "  Date: $(date)"
log "============================================================"
log ""

# Phase 1: Small Payload - Throughput baseline (/hello)
log ""
log "### PHASE 1: Small Payload Throughput (/hello) ###"
for c in 10 50 100 200 500 1000 2000 5000 10000; do
    run_bench "P1-hello" "http://localhost:8080/hello" "$c" 30 "-2"
    sleep 2
done

# Phase 2: Minimal Payload - absolute peak (/ping)  
log ""
log "### PHASE 2: Minimal Payload Peak (/ping) ###"
for c in 100 500 1000 2000 5000 10000; do
    run_bench "P2-ping" "http://localhost:8080/ping" "$c" 30 "-2"
    sleep 2
done

# Phase 3: Static File Serving (/)
log ""
log "### PHASE 3: Static File Serving (/) ###"
for c in 10 50 100 200 500 1000 2000 5000; do
    run_bench "P3-static" "http://localhost:8080/" "$c" 30 "-2"
    sleep 2
done

# Phase 4: 1KB Payload (/data)
log ""
log "### PHASE 4: 1KB Payload (/data) ###"
for c in 10 50 100 200 500 1000 2000 5000; do
    run_bench "P4-1KB" "http://localhost:8080/data" "$c" 30 "-2"
    sleep 2
done

# Phase 5: Force Mode - raw engine capacity (/hello)
log ""
log "### PHASE 5: Force Mode - Raw Engine Capacity (/hello) ###"
for c in 100 500 1000 2000 5000 10000; do
    run_bench "P5-force" "http://localhost:8080/hello" "$c" 30 "-2 -f"
    sleep 2
done

# Phase 6: HTTP/1.0 comparison (/hello)
log ""
log "### PHASE 6: HTTP/1.0 Protocol (/hello) ###"
for c in 100 500 1000 2000 5000; do
    run_bench "P6-http10" "http://localhost:8080/hello" "$c" 30 "-1"
    sleep 2
done

# Phase 7: Long Duration Stability (120s)
log ""
log "### PHASE 7: Long Duration Stability ###"
for d in 30 60 120; do
    run_bench "P7-stable-${d}s" "http://localhost:8080/hello" 1000 "$d" "-2"
    sleep 3
done

# Phase 8: Extreme Concurrency
log ""
log "### PHASE 8: Extreme Concurrency Test ###"
for c in 5000 10000 15000 20000; do
    run_bench "P8-extreme" "http://localhost:8080/ping" "$c" 15 "-2"
    sleep 3
done

# Cleanup
log ""
log "Stopping server..."
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

log ""
log "============================================================"
log "  BENCHMARK COMPLETE"
log "  Results saved to: $RESULT_FILE"
log "============================================================"

echo "RESULT_FILE=$RESULT_FILE"
