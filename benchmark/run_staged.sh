#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULT_DIR="${SCRIPT_DIR}/results"
mkdir -p "$RESULT_DIR"
cd "$SCRIPT_DIR"
WEB="python3 ${SCRIPT_DIR}/bench_client.py"
RDIR="$RESULT_DIR/stages"
mkdir -p "$RDIR"

pkill -f bench_server 2>/dev/null; sleep 1
./bench_server_opt >/dev/null 2>&1 & sleep 2

###############################################################################
# STAGE 1: Low Concurrency (c=10~500)
###############################################################################
R1="$RDIR/stage1.txt"
echo "=== Stage 1: Low Concurrency ===" > "$R1"
echo "Date: $(date)" >> "$R1"

for ep in hello ping "/" data; do
    case $ep in
        hello) url="http://localhost:8080/hello"; desc="/hello (12B)" ;;
        ping)  url="http://localhost:8080/ping";  desc="/ping (4B)" ;;
        "/")   url="http://localhost:8080/";      desc="/ (static ~1KB)" ;;
        data)  url="http://localhost:8080/data";  desc="/data (1KB)" ;;
    esac
    echo "" >> "$R1"
    echo "--- $desc ---" >> "$R1"
    for c in 10 50 100 200 500; do
        result=$("$WEB" -t 15 -c $c -2 "$url" 2>/dev/null | grep -E "^Speed=|^Requests:")
        echo "c=$c | $result" >> "$R1"
    done
done
echo "" >> "$R1"; echo "Stage 1 done." >> "$R1"
cat "$R1"

###############################################################################
# STAGE 2: Medium Concurrency (c=500~2000)
###############################################################################
pkill -f bench_server 2>/dev/null; sleep 1
./bench_server_opt >/dev/null 2>&1 & sleep 2

R2="$RDIR/stage2.txt"
echo "=== Stage 2: Medium Concurrency ===" > "$R2"
echo "Date: $(date)" >> "$R2"

for ep in hello ping "/" data; do
    case $ep in
        hello) url="http://localhost:8080/hello"; desc="/hello (12B)" ;;
        ping)  url="http://localhost:8080/ping";  desc="/ping (4B)" ;;
        "/")   url="http://localhost:8080/";      desc="/ (static ~1KB)" ;;
        data)  url="http://localhost:8080/data";  desc="/data (1KB)" ;;
    esac
    echo "" >> "$R2"
    echo "--- $desc ---" >> "$R2"
    for c in 500 1000 1500 2000; do
        result=$("$WEB" -t 15 -c $c -2 "$url" 2>/dev/null | grep -E "^Speed=|^Requests:")
        echo "c=$c | $result" >> "$R2"
    done
done
echo "" >> "$R2"; echo "Stage 2 done." >> "$R2"
cat "$R2"

###############################################################################
# STAGE 3: High Concurrency (c=2000~4000)
###############################################################################
pkill -f bench_server 2>/dev/null; sleep 1
./bench_server_opt >/dev/null 2>&1 & sleep 2

R3="$RDIR/stage3.txt"
echo "=== Stage 3: High Concurrency Stress ===" > "$R3"
echo "Date: $(date)" >> "$R3"

echo "" >> "$R3"; echo "--- /hello (12B) ---" >> "$R3"
for c in 2000 2500 3000 3500 4000; do
    result=$("$WEB" -t 10 -c $c -2 http://localhost:8080/hello 2>/dev/null | grep -E "^Speed=|^Requests:")
    echo "c=$c | $result" >> "$R3"
    if ! curl -s -o /dev/null http://localhost:8080/ping 2>/dev/null; then
        echo "CRASHED at c=$c" >> "$R3"
        pkill -f bench_server 2>/dev/null; sleep 1
        ./bench_server_opt >/dev/null 2>&1 & sleep 2
    fi
    sleep 1
done

echo "" >> "$R3"; echo "--- /ping (4B) ---" >> "$R3"
for c in 2000 2500 3000 3500 4000; do
    result=$("$WEB" -t 10 -c $c -2 http://localhost:8080/ping 2>/dev/null | grep -E "^Speed=|^Requests:")
    echo "c=$c | $result" >> "$R3"
    if ! curl -s -o /dev/null http://localhost:8080/ping 2>/dev/null; then
        echo "CRASHED at c=$c" >> "$R3"
        pkill -f bench_server 2>/dev/null; sleep 1
        ./bench_server_opt >/dev/null 2>&1 & sleep 2
    fi
    sleep 1
done
echo "" >> "$R3"; echo "Stage 3 done." >> "$R3"
cat "$R3"

###############################################################################
# STAGE 4: Protocol & Stability
###############################################################################
pkill -f bench_server 2>/dev/null; sleep 1
./bench_server_opt >/dev/null 2>&1 & sleep 2

R4="$RDIR/stage4.txt"
echo "=== Stage 4: Protocol & Stability ===" > "$R4"
echo "Date: $(date)" >> "$R4"

echo "" >> "$R4"; echo "--- HTTP/1.0 /hello ---" >> "$R4"
for c in 100 500 1000 2000; do
    result=$("$WEB" -t 10 -c $c -1 http://localhost:8080/hello 2>/dev/null | grep -E "^Speed=|^Requests:")
    echo "c=$c | $result" >> "$R4"
done

echo "" >> "$R4"; echo "--- Reload /hello ---" >> "$R4"
for c in 100 500 1000 2000; do
    result=$("$WEB" -t 10 -c $c -2 -r http://localhost:8080/hello 2>/dev/null | grep -E "^Speed=|^Requests:")
    echo "c=$c | $result" >> "$R4"
done

echo "" >> "$R4"; echo "--- Stability c=1000 t=60s ---" >> "$R4"
"$WEB" -t 60 -c 1000 -2 http://localhost:8080/hello 2>/dev/null | grep -E "^Speed=|^Requests:" >> "$R4"

echo "" >> "$R4"; echo "--- Stability c=1000 t=120s ---" >> "$R4"
"$WEB" -t 120 -c 1000 -2 http://localhost:8080/hello 2>/dev/null | grep -E "^Speed=|^Requests:" >> "$R4"

echo "" >> "$R4"; echo "Stage 4 done." >> "$R4"
cat "$R4"

###############################################################################
# STAGE 5: Extreme Boundary
###############################################################################
pkill -f bench_server 2>/dev/null; sleep 1
./bench_server_opt >/dev/null 2>&1 & sleep 2

R5="$RDIR/stage5.txt"
echo "=== Stage 5: Extreme Boundary ===" > "$R5"
echo "Date: $(date)" >> "$R5"

echo "" >> "$R5"; echo "--- /ping extreme ---" >> "$R5"
for c in 4000 5000 6000 7000 8000 10000; do
    result=$("$WEB" -t 5 -c $c -2 http://localhost:8080/ping 2>/dev/null | grep -E "^Speed=|^Requests:")
    echo "c=$c | $result" >> "$R5"
    if ! curl -s -o /dev/null http://localhost:8080/ping 2>/dev/null; then
        echo "CRASHED at c=$c" >> "$R5"
        pkill -f bench_server 2>/dev/null; sleep 1
        ./bench_server_opt >/dev/null 2>&1 & sleep 2
    fi
    sleep 2
done
echo "" >> "$R5"; echo "Stage 5 done." >> "$R5"
cat "$R5"

pkill -f bench_server 2>/dev/null
echo "ALL DONE"
