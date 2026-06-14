#!/bin/bash
# 阶段6: 长时间稳定性 (wrk)
source "$(dirname "$0")/../lib/common.sh"
run_bench "PH6-stable-120s" "http://localhost:8080/hello" 1000 "$BENCH_STABLE_DURATION" "-2"
sleep $BENCH_STABLE_SLEEP
