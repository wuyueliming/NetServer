#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULT_DIR="${SCRIPT_DIR}/results"
mkdir -p "$RESULT_DIR"
cd "$SCRIPT_DIR"
WEB="python3 ${SCRIPT_DIR}/bench_client.py"
RF="$RESULT_DIR/batch1_$(date +%Y%m%d_%H%M%S).txt"
log() { echo "$(date '+%H:%M:%S') $*" | tee -a "$RF"; }

run_bench() {
    local label="$1" url="$2" clients="$3" duration="${4:-20}" extra="${5:-}"
    local http_ver="-1" force=""
    for arg in $extra; do [ "$arg" = "-2" ] && http_ver="-2"; [ "$arg" = "-f" ] && force="-f"; done
    log "  [$label] c=$clients t=${duration}s $extra"
    $WEB -t "$duration" -c "$clients" $http_ver $force "$url" 2>&1 | tee -a "$RF"
    log ""
}

./bench_server >/dev/null 2>&1 & SPID=$!
sleep 2
curl -s http://localhost:8080/ping >/dev/null || { log "FAIL"; exit 1; }
log "Server OK | 4-core Xeon 8255C @2.5GHz | 3.6GB RAM"
log "============================================================"

log ""; log "### CORE THROUGHPUT: /hello (12B response, HTTP/1.1) ###"
for c in 10 50 100 200 500 1000 2000 5000 10000; do
    run_bench "hello" "http://localhost:8080/hello" "$c" 20 "-2"
done

log ""; log "### PEAK THROUGHPUT: /ping (4B response, HTTP/1.1) ###"
for c in 100 500 1000 2000 5000 10000; do
    run_bench "ping" "http://localhost:8080/ping" "$c" 20 "-2"
done

log ""; log "### FORCE MODE: /hello (don't wait for reply) ###"
for c in 100 500 1000 2000 5000 10000; do
    run_bench "hello-force" "http://localhost:8080/hello" "$c" 20 "-2 -f"
done

log "DONE BATCH1. Results: $RF"
kill $SPID 2>/dev/null; wait $SPID 2>/dev/null
echo "BATCH1=$RF"
