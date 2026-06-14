#!/bin/bash
# 阶段7: 延迟分布分析 (wrk --latency)
source "$(dirname "$0")/../lib/common.sh"
for c in 100 500 2000; do
    run_bench "PH7-latency-c${c}" "http://localhost:8080/hello" "$c" "$BENCH_DURATION" "-2"
    sleep $BENCH_SLEEP
done
