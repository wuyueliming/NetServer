#!/bin/bash
# 阶段2: 最小有效载荷 (/ping, 4B 响应)
source "$(dirname "$0")/../lib/common.sh"
for c in 100 500 2000 5000; do
    run_bench "PH2-ping" "http://localhost:8080/ping" "$c" "$BENCH_DURATION" "-2"
    sleep $BENCH_SLEEP
done
