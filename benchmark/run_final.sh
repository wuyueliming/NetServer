#!/bin/bash
# 综合基准测试主入口
# 执行所有 8 个阶段，生成完整测试报告

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

RESULT_FILE="$RESULT_DIR/final_bench_$(date +%Y%m%d_%H%M%S).txt"
touch "$RESULT_FILE"

echo "============================================================" | tee -a "$RESULT_FILE"
echo "  HTTP Server - 综合性能基准测试" | tee -a "$RESULT_FILE"
echo "  $(date)" | tee -a "$RESULT_FILE"
echo "  CPU: $(nproc) cores" | tee -a "$RESULT_FILE"
echo "  BENCH_THREADS=${BENCH_THREADS:-4}" | tee -a "$RESULT_FILE"
echo "============================================================" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

# 工具检测（含自动编译）
detect_tools | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

# 阶段列表
PHASES=(
    "01_throughput:阶段1:基准吞吐量(/hello)"
    "02_min_payload:阶段2:最小有效载荷(/ping)"
    "03_payload:阶段3:有效载荷影响"
    "04_protocol:阶段4:协议对比"
    "05_force:阶段5:强制模式"
    "06_stability:阶段6:长时间稳定性"
    "07_latency:阶段7:延迟分布"
    "08_extreme:阶段8:极限并发"
)

# 解析跳过/部分执行
SKIP_PHASES="${BENCH_SKIP_PHASES:-}"
FROM_PHASE="${BENCH_FROM_PHASE:-1}"
TO_PHASE="${BENCH_TO_PHASE:-8}"

# 一次性启动服务器（所有阶段共享）
echo "Starting server..." | tee -a "$RESULT_FILE"
start_server || echo "WARN: Server start failed, may reuse existing." | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

for phase_entry in "${PHASES[@]}"; do
    IFS=':' read -r phase_file phase_num phase_desc <<< "$phase_entry"
    phase_index="${phase_entry:0:2}"  # 01, 02, ...

    # 跳过检查
    if [[ "$SKIP_PHASES" == *"$phase_index"* ]]; then
        echo "  SKIP: $phase_num ($phase_desc)" | tee -a "$RESULT_FILE"
        continue
    fi
    if [ "${phase_index#0}" -lt "$FROM_PHASE" ] || [ "${phase_index#0}" -gt "$TO_PHASE" ]; then
        continue
    fi

    echo "" | tee -a "$RESULT_FILE"
    echo "============================================================" | tee -a "$RESULT_FILE"
    echo "  $phase_num: $phase_desc" | tee -a "$RESULT_FILE"
    echo "============================================================" | tee -a "$RESULT_FILE"

    # 执行阶段
    PHASE_SCRIPT="$SCRIPT_DIR/phases/${phase_file}.sh"
    if [ -f "$PHASE_SCRIPT" ]; then
        set +e
        RESULT_FILE="$RESULT_FILE" bash "$PHASE_SCRIPT" 2>&1
        PHASE_EXIT=$?
        set -e
        if [ $PHASE_EXIT -ne 0 ]; then
            echo "  WARN: Phase $phase_index exited with code $PHASE_EXIT, continuing." | tee -a "$RESULT_FILE"
        fi
    else
        echo "  SKIP: $PHASE_SCRIPT not found" | tee -a "$RESULT_FILE"
    fi
done

echo "" | tee -a "$RESULT_FILE"
echo "============================================================" | tee -a "$RESULT_FILE"
echo "  基准测试完成" | tee -a "$RESULT_FILE"
echo "  原始结果: $RESULT_FILE" | tee -a "$RESULT_FILE"

# 停止服务器
stop_server
echo "  Server stopped." | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

# 生成报告
echo "" | tee -a "$RESULT_FILE"
echo "Generating report..." | tee -a "$RESULT_FILE"
generate_report "$RESULT_FILE" | tee -a "$RESULT_FILE"

# 可选：与历史对比
if [ -n "${BENCH_COMPARE:-}" ]; then
    echo "" | tee -a "$RESULT_FILE"
    echo "Comparing with previous results..." | tee -a "$RESULT_FILE"
    bench_compare "$RESULT_FILE" | tee -a "$RESULT_FILE"
fi

echo "" | tee -a "$RESULT_FILE"
echo "DONE." | tee -a "$RESULT_FILE"
