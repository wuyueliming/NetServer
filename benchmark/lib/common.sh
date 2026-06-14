#!/bin/bash
# 公共函数库 - wrk + bench_client.py 双引擎支持

# === 可覆盖配置（通过环境变量）===
: "${BENCH_DURATION:=15}"
: "${BENCH_QUICK_DURATION:=5}"
: "${BENCH_SLEEP:=1}"
: "${BENCH_WARMUP:=5}"
: "${BENCH_EXTREME_DURATION:=10}"
: "${BENCH_STABLE_DURATION:=120}"
: "${BENCH_EXTREME_SLEEP:=2}"
: "${BENCH_STABLE_SLEEP:=2}"
: "${BENCH_PORT:=8080}"
: "${BENCH_THREADS:=4}"
: "${BENCH_TIMEOUT:=60}"

# === 固定配置 ===
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BENCH_PY="python3 ${SCRIPT_DIR}/bench_client.py"
BENCH_WRK="${SCRIPT_DIR}/wrk"
BENCH_SERVER="${SCRIPT_DIR}/bench_server_opt"

: "${RESULT_DIR:=${SCRIPT_DIR}/results}"
mkdir -p "$RESULT_DIR"

# 服务器 PID（初始化为空，兼容 set -u）
SERVER_PID=""

# QUICK 模式
if [ -n "${BENCH_QUICK:-}" ]; then
    BENCH_DURATION="$BENCH_QUICK_DURATION"
    BENCH_WARMUP=2
    BENCH_STABLE_DURATION=30
    BENCH_EXTREME_DURATION=5
fi

# === 工具检测（含自动编译和预检）===
detect_tools() {
    command -v python3 >/dev/null 2>&1 || { echo "ERROR: python3 not found"; exit 1; }
    command -v curl >/dev/null 2>&1 || { echo "WARN: curl not found"; }

    echo "BUILD: Building bench_server_opt..."
    make -C "$SCRIPT_DIR" -s bench_server_opt -j$(nproc) 2>&1 | head -3

    local fd_limit=$(ulimit -n)
    echo "SYSTEM: fd_limit=$fd_limit"
    if [ "$fd_limit" -lt 50000 ]; then
        echo "WARN: Low fd limit ($fd_limit), high concurrency tests may fail"
    fi

    if ss -tln 2>/dev/null | grep -q ":${BENCH_PORT} "; then
        echo "WARN: Port $BENCH_PORT already in use"
    fi

    if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        local gov=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
        echo "SYSTEM: CPU governor=$gov"
        if [ "$gov" != "performance" ]; then
            echo "WARN: CPU governor is '$gov' (not 'performance'), QPS may be lower"
        fi
    fi

    for param in "net.core.somaxconn" "net.ipv4.tcp_max_syn_backlog"; do
        echo "SYSTEM: $param=$(cat "/proc/sys/${param//./\/}" 2>/dev/null || echo 'N/A')"
    done

    if [ -x "$BENCH_WRK" ]; then
        echo "TOOL:wrk ($($BENCH_WRK --version 2>&1 | head -1))"
    else
        echo "TOOL:wrk (not available)"
    fi
    echo "TOOL:pybench"
    echo "CONFIG: BENCH_THREADS=$BENCH_THREADS BENCH_PORT=$BENCH_PORT BENCH_TIMEOUT=$BENCH_TIMEOUT"
}

# === wrk 执行 ===
run_wrk() {
    local url="$1" clients="$2" duration="$3"
    local wrk_threads
    if   [ "$clients" -le 1000 ];  then wrk_threads=2
    elif [ "$clients" -le 5000 ];  then wrk_threads=4
    elif [ "$clients" -le 10000 ]; then wrk_threads=6
    else                              wrk_threads=8
    fi

    local warmup=$BENCH_WARMUP
    [ "$clients" -gt 5000 ] && warmup=$((warmup + 5))
    $BENCH_WRK -t$wrk_threads -c$clients -d${warmup}s "$url" > /dev/null 2>&1
    sleep 1

    $BENCH_WRK -t$wrk_threads -c$clients -d${duration}s --latency "$url" 2>&1
}

parse_wrk_output() {
    local output="$1"
    local qps bw p50 p99
    qps=$(echo "$output" | grep "Requests/sec:" | awk '{print $2}')
    bw=$(echo "$output" | grep "Transfer/sec:" | awk '{print $2}')
    p50=$(echo "$output" | grep "  50%" | awk '{print $2}')
    p99=$(echo "$output" | grep "  99%" | awk '{print $2}')
    echo "QPS: ${qps:-N/A} | BW: ${bw:-N/A} | P50: ${p50:-N/A} | P99: ${p99:-N/A}"
}

# === 服务器管理 ===
start_server() {
    local bin="${1:-$BENCH_SERVER}"

    # 如果已有服务器在运行，直接复用
    if curl -s -o /dev/null "http://localhost:${BENCH_PORT}/ping" 2>/dev/null; then
        echo "Server already running, reusing (PID=$(pgrep -f bench_server_opt 2>/dev/null | head -1 || echo '?'))."
        return 0
    fi

    if [ ! -f "$bin" ]; then
        echo "ERROR: Server binary not found: $bin" >&2
        return 1
    fi
    $bin > /dev/null 2>&1 &
    SERVER_PID=$!

    local waited=0
    while [ $waited -lt 25 ]; do
        if curl -s -o /dev/null "http://localhost:${BENCH_PORT}/ping" 2>/dev/null; then
            echo "Server started (PID=$SERVER_PID, ready in $((waited / 5))s)"
            return 0
        fi
        sleep 0.2
        waited=$((waited + 1))
    done

    echo "ERROR: Server failed to start within 5s!" >&2
    kill "$SERVER_PID" 2>/dev/null || true
    SERVER_PID=""
    return 1
}

stop_server() {
    local pid="${1:-$SERVER_PID}"
    if [ -n "$pid" ]; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    fi
    killall bench_server_opt bench_server 2>/dev/null || true
    sleep 1
    SERVER_PID=""
}

# === 基准测试执行 ===
run_bench() {
    local label="$1" url="$2" clients="$3" duration="${4:-$BENCH_DURATION}" extra="${5:-}"
    local result_file="${6:-$RESULT_FILE}"

    echo "$(date '+%H:%M:%S')  [$label] c=$clients t=${duration}s ${extra}" | tee -a "$result_file"

    if [ -x "$BENCH_WRK" ]; then
        local use_wrk=true
        for arg in $extra; do
            [ "$arg" = "-1" ] && use_wrk=false
            [ "$arg" = "-f" ] && use_wrk=false
            [ "$arg" = "-r" ] && use_wrk=false
        done

        if $use_wrk; then
            local output
            output=$(run_wrk "$url" "$clients" "$duration" 2>&1)
            echo "$output" | tee -a "$result_file"
            echo "" >> "$result_file"
            local summary
            summary=$(parse_wrk_output "$output")
            echo "  => $summary" | tee -a "$result_file"
            echo "" >> "$result_file"
            return
        fi
    fi

    local output
    # pybench 命令加 5 分钟超时保护
    output=$(timeout 300 $BENCH_PY -t "$duration" -c "$clients" $extra "$url" 2>&1)
    echo "$output" | grep -v "^CSV:" | tee -a "$result_file"
    echo "" >> "$result_file"
}

# === 报告生成（POSIX 兼容）===
generate_report() {
    local result_file="$1"
    local report_file="${result_file%.txt}.md"

    # POSIX 兼容的 \K 替代：用 sed 提取 key=value
    local qps_table=""
    qps_table="| 场景 | 并发 | QPS | 延迟 P50 | 延迟 P99 |\n|------|------|-----|---------|---------|\n"
    while IFS= read -r line; do
        if echo "$line" | grep -qE '\[PH[0-9]-[a-z]+\]'; then
            local label
            label=$(echo "$line" | sed -n 's/.*\(\[PH[0-9]-[a-z]*\]\).*/\1/p')
            local clients
            clients=$(echo "$line" | sed -n 's/.*c=\([0-9]*\).*/\1/p')
        fi
        local qps
        qps=$(echo "$line" | sed -n 's/.*QPS: \([0-9.]*\).*/\1/p')
        local p50
        p50=$(echo "$line" | sed -n 's/.*P50: \([^ |]*\).*/\1/p')
        local p99
        p99=$(echo "$line" | sed -n 's/.*P99: \([^ |]*\).*/\1/p')
        if [ -n "$qps" ]; then
            qps_table="${qps_table}| ${label:-?} | ${clients:-?} | $qps | ${p50:-N/A} | ${p99:-N/A} |\n"
        fi
    done < <(grep "=>" "$result_file" 2>/dev/null)

    local peak
    peak=$(echo "$qps_table" | grep -oE '[0-9]+\.[0-9]+' | sort -rn | head -1)

    {
        echo "# 性能测试报告"
        echo ""
        echo "**测试时间**: $(date -r "$result_file" '+%Y-%m-%d %H:%M:%S' 2>/dev/null)"
        echo ""
        echo "## 测试环境"
        echo "- CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
        echo "- 核心数: $(nproc)"
        echo "- 内存: $(free -h | grep Mem | awk '{print $2}')"
        echo "- 操作系统: $(uname -sr)"
        echo "- 线程数: $BENCH_THREADS"
        echo "- 工具: wrk $( [ -x "$BENCH_WRK" ] && $BENCH_WRK --version 2>&1 | head -1 || echo 'N/A' )"
        echo ""
        echo "## 性能摘要"
        echo ""
        echo "- **峰值 QPS**: ${peak:-N/A}"
        echo ""
        echo "## QPS 汇总"
        echo ""
        printf '%b' "$qps_table"
        echo ""
        echo "## 原始日志"
        echo ""
        echo '```'
        cat "$result_file"
        echo '```'
    } > "$report_file"

    echo "Report: $report_file"
    [ -n "$peak" ] && echo "  Peak QPS: $peak"
}

# === 历史对比（POSIX 兼容）===
bench_compare() {
    local result_file="$1"
    local latest_link="$RESULT_DIR/latest"
    if [ ! -f "$latest_link" ] && [ ! -L "$latest_link" ]; then
        echo "  No previous results found at $latest_link"
        # 首次运行，创建软链
        rm -f "$latest_link"
        ln -sf "$result_file" "$latest_link"
        return
    fi

    # 如果 latest 是普通文件（误创建），覆盖它
    if [ -f "$latest_link" ] && [ ! -L "$latest_link" ]; then
        rm -f "$latest_link"
        ln -sf "$result_file" "$latest_link"
        echo "  Previous result was a regular file, overwritten."
        return
    fi

    echo "  Comparing with: $latest_link"

    # POSIX 兼容的 QPS 提取
    local prev_peak curr_peak
    prev_peak=$(grep "=> QPS:" "$latest_link" 2>/dev/null | sed -n 's/.*QPS: \([0-9.]*\).*/\1/p' | sort -rn | head -1)
    curr_peak=$(grep "=> QPS:" "$result_file" 2>/dev/null | sed -n 's/.*QPS: \([0-9.]*\).*/\1/p' | sort -rn | head -1)

    if [ -n "$prev_peak" ] && [ -n "$curr_peak" ]; then
        local diff
        diff=$(echo "scale=2; ($curr_peak - $prev_peak) / $prev_peak * 100" | bc 2>/dev/null || echo "0")
        local sign=""
        [ "$(echo "$diff > 0" | bc 2>/dev/null)" = "1" ] && sign="+"
        echo "  Previous peak QPS: $prev_peak"
        echo "  Current  peak QPS: $curr_peak"
        echo "  Change: ${sign}${diff}%"
    fi

    rm -f "$latest_link"
    ln -sf "$result_file" "$latest_link"
}
