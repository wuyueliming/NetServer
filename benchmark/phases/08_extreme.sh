#!/bin/bash
# 阶段8: 极限并发 (wrk + pybench 混合)
source "$(dirname "$0")/../lib/common.sh"
echo "--- 逐步逼近极限 (wrk) ---"
for c in 5000 8000 10000 15000; do
    run_bench "PH8-extreme-c${c}" "http://localhost:8080/ping" "$c" "$BENCH_EXTREME_DURATION" "-2"
    sleep $BENCH_EXTREME_SLEEP
done
echo "--- CPU 饱和 (pybench -r) ---"
for c in 1000 2000 5000; do
    run_bench "PH8-cpu-c${c}" "http://localhost:8080/hello" "$c" "$BENCH_DURATION" "-2 -r"
    sleep $BENCH_SLEEP
done
