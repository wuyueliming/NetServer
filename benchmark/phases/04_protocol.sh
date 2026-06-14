#!/bin/bash
# 阶段4: 协议对比 (HTTP/1.1 wrk / HTTP/1.0 pybench)
source "$(dirname "$0")/../lib/common.sh"
echo "--- HTTP/1.1 (keep-alive, wrk) ---"
for c in 100 500 1000 2000; do
    run_bench "PH4-http11" "http://localhost:8080/hello" "$c" "$BENCH_DURATION" "-2"
    sleep $BENCH_SLEEP
done
echo "--- HTTP/1.0 (no keep-alive, pybench) ---"
for c in 50 200; do
    run_bench "PH4-http10" "http://localhost:8080/hello" "$c" "$BENCH_DURATION" "-1"
    sleep $BENCH_SLEEP
done
