#!/bin/bash
# 阶段5: 强制模式 (不等待回复, pybench)
source "$(dirname "$0")/../lib/common.sh"
for c in 100 1000 5000; do
    run_bench "PH5-force" "http://localhost:8080/hello" "$c" "$BENCH_DURATION" "-2 -f"
    sleep $BENCH_SLEEP
done
