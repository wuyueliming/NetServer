#!/bin/bash
# 阶段3: 有效载荷影响 (/data 1KB + 静态文件)
source "$(dirname "$0")/../lib/common.sh"
echo "--- /data (1KB) ---"
for c in 10 100 1000 5000; do
    run_bench "PH3-data" "http://localhost:8080/data" "$c" "$BENCH_DURATION" "-2"
    sleep $BENCH_SLEEP
done
echo "--- Static File / (~1KB) ---"
for c in 10 100 1000 5000; do
    run_bench "PH3-static" "http://localhost:8080/" "$c" "$BENCH_DURATION" "-2"
    sleep $BENCH_SLEEP
done
