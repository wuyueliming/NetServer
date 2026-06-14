#!/usr/bin/env python3
"""
轻量级 HTTP 基准测试客户端
支持 HTTP/1.0 和 HTTP/1.1，并发连接，QPS/延迟统计

用法:
    python3 bench_client.py -t 30 -c 500 -2 http://localhost:8080/hello
    python3 bench_client.py -t 15 -c 1000 -1 http://localhost:8080/ping
    python3 bench_client.py -t 20 -c 200 -2 -f http://localhost:8080/hello   # force 模式
"""

import socket
import sys
import time
import re
import urllib.parse
import threading
import queue
from datetime import datetime


def parse_url(url):
    parsed = urllib.parse.urlparse(url)
    host = parsed.hostname or "localhost"
    port = parsed.port or 80
    path = parsed.path or "/"
    if parsed.query:
        path += "?" + parsed.query
    return host, port, path


def build_request(method, path, http_version, reload=False):
    headers = f"Host: localhost\r\n"
    if http_version == "HTTP/1.1":
        headers += "Connection: keep-alive\r\n"
    if reload:
        headers += "Cache-Control: no-cache\r\n"
    return f"{method} {path} {http_version}\r\n{headers}\r\n".encode()


class BenchWorker(threading.Thread):
    def __init__(self, host, port, path, http_version, force, reload, results_queue, stop_event):
        super().__init__()
        self.host = host
        self.port = port
        self.path = path
        self.http_version = http_version
        self.force = force
        self.reload = reload
        self.results = results_queue
        self.stop = stop_event
        self.daemon = True

    def run(self):
        request = build_request("GET", self.path, self.http_version, self.reload)
        sock = None
        keep_alive = (self.http_version == "HTTP/1.1" and not self.force)
        try:
            while not self.stop.is_set():
                if sock is None:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(10.0)
                    sock.connect((self.host, self.port))

                t0 = time.time()
                sock.sendall(request)
                if self.force:
                    self.results.put(("ok", time.time() - t0, 0))
                    continue

                # 读取响应
                data = b""
                while True:
                    chunk = sock.recv(65536)
                    if not chunk:
                        break
                    data += chunk
                    if b"\r\n\r\n" in data:
                        header_end = data.find(b"\r\n\r\n")
                        headers = data[:header_end].decode("utf-8", errors="ignore")
                        cl_match = re.search(r"Content-Length:\s*(\d+)", headers)
                        if cl_match:
                            body_len = int(cl_match.group(1))
                            body_start = header_end + 4
                            if len(data) - body_start >= body_len:
                                break
                        else:
                            if self.http_version == "HTTP/1.0":
                                continue
                            break
                elapsed = time.time() - t0
                if b"HTTP/1." in data:
                    self.results.put(("ok", elapsed, len(data)))
                else:
                    self.results.put(("err", elapsed, 0))
                    sock = None
                    continue

                # HTTP/1.1 Keep-Alive: 复用连接，不关闭 socket
                if not keep_alive:
                    if sock:
                        try:
                            sock.close()
                        except:
                            pass
                    sock = None

        except Exception as e:
            self.results.put(("err", 0, 0))
            sock = None
        finally:
            if sock:
                try:
                    sock.close()
                except:
                    pass


def run_bench(url, clients, duration, http_ver, force, reload=False, warmup=False):
    host, port, path = parse_url(url)
    http_version = "HTTP/1.0" if http_ver == "1.0" else "HTTP/1.1"
    results = queue.Queue()
    stop_event = threading.Event()

    workers = []
    for _ in range(clients):
        w = BenchWorker(host, port, path, http_version, force, reload, results, stop_event)
        workers.append(w)

    for w in workers:
        w.start()

    # 预热期：3s 让连接建立和缓存预热
    if warmup:
        time.sleep(3)
        # 丢弃预热期间积累的结果
        while not results.empty():
            try:
                results.get_nowait()
            except queue.Empty:
                break
    else:
        time.sleep(0.5)

    t0 = time.time()
    end_time = t0 + duration
    total_requests = 0
    total_bytes = 0
    errors = 0
    latencies = []

    while time.time() < end_time:
        try:
            status, lat, bytes_rcv = results.get(timeout=0.1)
            if status == "ok":
                total_requests += 1
                total_bytes += bytes_rcv
                if lat > 0:
                    latencies.append(lat)
            else:
                errors += 1
        except queue.Empty:
            pass

    stop_event.set()
    for w in workers:
        w.join(timeout=2)

    while not results.empty():
        try:
            status, lat, bytes_rcv = results.get_nowait()
            if status == "ok":
                total_requests += 1
                total_bytes += bytes_rcv
                if lat > 0:
                    latencies.append(lat)
            else:
                errors += 1
        except queue.Empty:
            break

    elapsed = time.time() - t0
    qps = total_requests / elapsed if elapsed > 0 else 0
    bandwidth = total_bytes / elapsed if elapsed > 0 else 0

    latencies.sort()
    lat_len = len(latencies)
    p50 = latencies[int(lat_len * 0.5)] if lat_len > 0 else 0
    p90 = latencies[int(lat_len * 0.9)] if lat_len > 0 else 0
    p99 = latencies[int(lat_len * 0.99)] if lat_len > 0 else 0
    avg_lat = sum(latencies) / lat_len if lat_len > 0 else 0

    return {
        "requests": total_requests,
        "errors": errors,
        "elapsed": elapsed,
        "qps": qps,
        "bandwidth": bandwidth,
        "avg_lat": avg_lat,
        "p50": p50,
        "p90": p90,
        "p99": p99,
    }


def format_speed(speed):
    if speed > 1024 * 1024:
        return f"{speed / (1024*1024):.1f} MB/s"
    elif speed > 1024:
        return f"{speed / 1024:.1f} KB/s"
    else:
        return f"{speed:.0f} B/s"


def main():
    import argparse
    parser = argparse.ArgumentParser(description="HTTP Benchmark Client")
    parser.add_argument("-t", type=int, default=10, help="测试持续时间（秒）")
    parser.add_argument("-c", type=int, default=100, help="并发连接数")
    parser.add_argument("-1", action="store_true", dest="http10", help="使用 HTTP/1.0")
    parser.add_argument("-2", action="store_true", dest="http11", help="使用 HTTP/1.1")
    parser.add_argument("-f", action="store_true", dest="force", help="强制模式（不等待回复）")
    parser.add_argument("-r", action="store_true", dest="reload", help="reload 模式（添加 Cache-Control: no-cache）")
    parser.add_argument("url", help="目标 URL")
    args = parser.parse_args()

    http_ver = "1.1" if args.http11 else "1.0"

    print(f"Benchmarking: {args.url} ({http_ver}, {args.c} clients, {args.t}s)")
    print("-" * 60)

    result = run_bench(args.url, args.c, args.t, http_ver, args.force, args.reload, warmup=True)

    print(f"Requests: {result['requests']} in {result['elapsed']:.1f}s")
    print(f"  QPS: {result['qps']:.1f} req/s")
    print(f"  Bandwidth: {format_speed(result['bandwidth'])}")
    print(f"  Errors: {result['errors']}")
    print(f"Latency:")
    print(f"  Average: {result['avg_lat']*1000:.2f} ms")
    print(f"  P50:     {result['p50']*1000:.2f} ms")
    print(f"  P90:     {result['p90']*1000:.2f} ms")
    print(f"  P99:     {result['p99']*1000:.2f} ms")
    print("-" * 60)

    print(f"CSV:{datetime.now().strftime('%H:%M:%S')},pybench,{http_ver},{args.url},{args.c},{args.t},{result['qps']:.1f},{format_speed(result['bandwidth'])},{result['errors']},{result['p50']*1000:.2f}ms,{result['p99']*1000:.2f}ms")


if __name__ == "__main__":
    main()
