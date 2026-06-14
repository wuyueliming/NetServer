#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULT_DIR="${SCRIPT_DIR}/results"
mkdir -p "$RESULT_DIR"
cd "$SCRIPT_DIR"
WEB="python3 ${SCRIPT_DIR}/bench_client.py"

# 替换 run_bench 以支持新旧参数
eval "$(declare -f run_bench | sed 's/\$extra/$(for arg in \$extra; do [ \"\$arg\" = \"-2\" ] \&\& echo -n \" \" || [ \"\$arg\" = \"-f\" ] \&\& echo -n \" \" || echo -n \"\"; done)/')"
RF="$RESULT_DIR/batch2_$(date +%Y%m%d_%H%M%S).txt"
log() { echo "$(date '+%H:%M:%S') $*" | tee -a "$RF"; }

run_bench() {
    local label="$1" url="$2" clients="$3" duration="${4:-20}" extra="${5:-}"
    log "  [$label] c=$clients t=${duration}s $extra"
    "$WEB" -t "$duration" -c "$clients" $extra "$url" 2>&1 | tee -a "$RF"
    log ""
}

./bench_server >/dev/null 2>&1 & SPID=$!
sleep 2
curl -s http://localhost:8080/ping >/dev/null || { log "FAIL"; exit 1; }
log "Server OK (PID=$SPID) | 4-core Xeon 8255C @2.5GHz | 3.6GB RAM"
log "============================================================"

log ""; log "### STATIC FILE: / (index.html, ~1KB, HTTP/1.1) ###"
for c in 10 50 100 200 500 1000 2000 5000; do
    run_bench "static" "http://localhost:8080/" "$c" 20 "-2"
done

log ""; log "### 1KB DYNAMIC PAYLOAD: /data (HTTP/1.1) ###"
for c in 10 50 100 200 500 1000 2000 5000; do
    run_bench "data-1KB" "http://localhost:8080/data" "$c" 20 "-2"
done

log ""; log "### HTTP/1.0 PROTOCOL: /hello (no keep-alive) ###"
for c in 100 500 1000 2000 5000; do
    run_bench "hello-HTTP10" "http://localhost:8080/hello" "$c" 20 "-1"
done

log ""; log "### LONG DURATION STABILITY: /hello c=1000 ###"
run_bench "stable-60s" "http://localhost:8080/hello" 1000 60 "-2"
run_bench "stable-120s" "http://localhost:8080/hello" 1000 120 "-2"

log ""; log "### EXTREME CONCURRENCY: /ping (finding upper limit) ###"
for c in 5000 8000 10000 12000 15000; do
    run_bench "extreme-$c" "http://localhost:8080/ping" "$c" 15 "-2"
done

log ""; log "### CPU SATURATION: /hello with reload (no-cache) ###"
for c in 100 500 1000 2000 5000; do
    run_bench "hello-reload" "http://localhost:8080/hello" "$c" 20 "-2 -r"
done

log "DONE BATCH2. Results: $RF"
kill $SPID 2>/dev/null; wait $SPID 2>/dev/null
echo "BATCH2=$RF"
