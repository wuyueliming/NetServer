#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WEB="python3 ${SCRIPT_DIR}/bench_client.py"
RESULT_DIR="${SCRIPT_DIR}/results"
mkdir -p "$RESULT_DIR"
RESULT_FILE="$RESULT_DIR/bench_report_$(date +%Y%m%d_%H%M%S).txt"

log() {
    echo "$(date '+%H:%M:%S') $*" | tee -a "$RESULT_FILE"
}

run_bench() {
    local label="$1"
    local url="$2"
    local clients="$3"
    local duration="${4:-30}"
    local extra="${5:-}"
    
    log "  [$label] clients=$clients time=${duration}s ${extra}"
    local output
    output=$("$WEB" -t "$duration" -c "$clients" $extra "$url" 2>&1)
    log "  $output"
    echo "$output"
}

###############################################################################
log "============================================================"
log "  muduo HTTP Server - Comprehensive Performance Benchmark"
log "  Server: 4-core Intel Xeon Platinum 8255C @ 2.50GHz, 3.6GB RAM"
log "  Date: $(date)"
log "============================================================"
log ""

# Phase 0: Quick server alive check
log "### Phase 0: Server Health Check ###"
if ! curl -s -o /dev/null http://localhost:8080/ping; then
    log "ERROR: Server is not running on port 8080!"
    exit 1
fi
log "Server is alive."

# Phase 1: Small Payload - Throughput baseline
log ""
log "============================================================"
log "  PHASE 1: Small Payload Throughput (/hello)"
log "============================================================"
for c in 10 50 100 200 500 1000 2000 5000 10000; do
    run_bench "PH1-hello" "http://localhost:8080/hello" "$c" 30 "-2"
    sleep 2
done

# Phase 2: Minimal Payload - absolute peak
log ""
log "============================================================"
log "  PHASE 2: Minimal Payload Peak (/ping)"
log "============================================================"
for c in 100 500 1000 2000 5000 10000; do
    run_bench "PH2-ping" "http://localhost:8080/ping" "$c" 30 "-2"
    sleep 2
done

# Phase 3: Static File Serving
log ""
log "============================================================"
log "  PHASE 3: Static File Serving (/)"
log "============================================================"
for c in 10 50 100 200 500 1000 2000 5000; do
    run_bench "PH3-static" "http://localhost:8080/" "$c" 30 "-2"
    sleep 2
done

# Phase 4: 1KB Payload
log ""
log "============================================================"
log "  PHASE 4: 1KB Payload (/data)"
log "============================================================"
for c in 10 50 100 200 500 1000 2000 5000; do
    run_bench "PH4-1KB" "http://localhost:8080/data" "$c" 30 "-2"
    sleep 2
done

# Phase 5: Force Mode (don't wait for reply) - raw engine capacity
log ""
log "============================================================"
log "  PHASE 5: Force Mode - Raw Engine Capacity (/hello)"
log "============================================================"
for c in 100 500 1000 2000 5000 10000; do
    run_bench "PH5-force" "http://localhost:8080/hello" "$c" 30 "-2 -f"
    sleep 2
done

# Phase 6: HTTP/1.0 comparison
log ""
log "============================================================"
log "  PHASE 6: HTTP/1.0 Protocol (/hello)"
log "============================================================"
for c in 100 500 1000 2000 5000; do
    run_bench "PH6-http10" "http://localhost:8080/hello" "$c" 30 "-1"
    sleep 2
done

# Phase 7: Long Duration Stability Test
log ""
log "============================================================"
log "  PHASE 7: Long Duration Stability (120s)"
log "============================================================"
for d in 30 60 120; do
    run_bench "PH7-stable-${d}s" "http://localhost:8080/hello" 1000 "$d" "-2"
    sleep 3
done

# Phase 8: Extreme Concurrency - Connection Limit
log ""
log "============================================================"
log "  PHASE 8: Extreme Concurrency Test"
log "============================================================"
for c in 5000 10000 15000 20000; do
    run_bench "PH8-extreme" "http://localhost:8080/ping" "$c" 15 "-2"
    sleep 3
done

log ""
log "============================================================"
log "  BENCHMARK COMPLETE"
log "  Results saved to: $RESULT_FILE"
log "============================================================"

echo ""
echo "DONE. Results: $RESULT_FILE"
