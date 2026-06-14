#!/bin/bash
# 阶段1: 基准吞吐量 (/hello, 12B 响应)
source "$(dirname "$0")/../lib/common.sh"
for c in 10 100 500 2000 5000; do
    run_bench "PH1-hello" "http://localhost:8080/hello" "$c" "$BENCH_DURATION" "-2"
    sleep $BENCH_SLEEP
done
