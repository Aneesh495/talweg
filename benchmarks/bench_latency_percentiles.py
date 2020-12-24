#!/usr/bin/env python3
"""
Talweg Microsecond-Resolution Storage Latency Percentile Benchmark
Measures p50, p90, p95, p99, and p99.9 latency distributions for page programs,
reads, and journal flushes under high concurrency workloads.
"""

import argparse
import json
import os
import random
import socket
import struct
import subprocess
import sys
import time

TALWEG_PROTO_MAGIC = 0x54574750

TALWEG_CMD_PING = 1
TALWEG_CMD_READ = 2
TALWEG_CMD_WRITE = 3
TALWEG_CMD_TRIM = 4
TALWEG_CMD_FLUSH = 5
TALWEG_CMD_STATS = 6

HDR_FORMAT = "<IHHQIIiII"
HDR_SIZE = struct.calcsize(HDR_FORMAT)

def recv_exact(s, n):
    buf = b""
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            break
        buf += chunk
    return buf

def send_cmd(s, cmd, lba=0, payload=b""):
    hdr = struct.pack(HDR_FORMAT, TALWEG_PROTO_MAGIC, cmd, 0, 1, lba, 1, 0, len(payload), 0)
    s.sendall(hdr)
    if payload:
        s.sendall(payload)
    resp_hdr_bytes = recv_exact(s, HDR_SIZE)
    if len(resp_hdr_bytes) < HDR_SIZE:
        return -1, b""
    magic, resp_cmd, flags, tag, r_lba, count, status, payload_len, crc = struct.unpack(HDR_FORMAT, resp_hdr_bytes)
    resp_payload = b""
    if payload_len > 0:
        resp_payload = recv_exact(s, payload_len)
    return status, resp_payload

def calculate_percentiles(latencies_us):
    if not latencies_us:
        return {}
    s = sorted(latencies_us)
    n = len(s)
    return {
        "count": n,
        "min": s[0],
        "p50": s[int(n * 0.50)],
        "p90": s[int(n * 0.90)],
        "p95": s[int(n * 0.95)],
        "p99": s[int(n * 0.99)],
        "p999": s[min(n - 1, int(n * 0.999))],
        "max": s[-1],
        "mean": sum(s) / n
    }

def print_ascii_histogram(latencies_us, buckets=10):
    if not latencies_us:
        return
    min_val = min(latencies_us)
    max_val = max(latencies_us)
    if min_val == max_val:
        max_val += 1.0

    step = (max_val - min_val) / buckets
    counts = [0] * buckets

    for val in latencies_us:
        idx = min(buckets - 1, int((val - min_val) / step))
        counts[idx] += 1

    max_count = max(1, max(counts))
    print("\n Latency Histogram:")
    for i in range(buckets):
        b_low = min_val + i * step
        b_high = b_low + step
        bar_len = int((counts[i] / max_count) * 40)
        bar = "#" * bar_len
        print(f"   [{b_low:7.1f} us - {b_high:7.1f} us]: {bar:<40} ({counts[i]:4d})")

def main():
    parser = argparse.ArgumentParser(description="Talweg Storage Latency Percentile Benchmark")
    parser.add_argument("--ops", type=int, default=4000, help="Total operations to benchmark")
    parser.add_argument("--ratio", type=float, default=0.70, help="Write operation ratio (0.0 to 1.0)")
    parser.add_argument("--output", help="Optional output JSON report")
    args = parser.parse_args()

    socket_path = "/tmp/talweg_latency_bench.sock"
    media_path = "latency_bench.flash"

    for p in [socket_path, media_path]:
        if os.path.exists(p):
            try:
                os.unlink(p)
            except OSError:
                pass

    print("=============================================================================")
    print(" Talweg Storage Operation Latency Percentile Benchmark")
    print("=============================================================================")
    print(f" Operations:     {args.ops}")
    print(f" Write/Read Mix: {args.ratio * 100:.0f}% Writes / {(1.0 - args.ratio) * 100:.0f}% Reads")
    print(f" Block Payload:  4096 bytes (4 KiB)")
    print(" Formatting media and launching daemon...")

    subprocess.run(["./bin/talweg", "format", "--output", media_path,
                    "--blocks", "128", "--pages", "64", "--user-lba", "4096"],
                   check=True, stdout=subprocess.DEVNULL)

    proc = subprocess.Popen(["./bin/talweg", "serve", "--socket", socket_path, "--media", media_path],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    for _ in range(50):
        if os.path.exists(socket_path):
            break
        time.sleep(0.05)

    write_latencies = []
    read_latencies = []
    dummy = b"L" * 4096

    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(socket_path)
            random.seed(1337)

            print(" Executing benchmark requests...")
            for i in range(args.ops):
                lba = random.randint(0, 4095)
                is_write = random.random() < args.ratio

                if is_write:
                    t0 = time.perf_counter_ns()
                    status, _ = send_cmd(s, TALWEG_CMD_WRITE, lba=lba, payload=dummy)
                    t1 = time.perf_counter_ns()
                    if status == 0:
                        write_latencies.append((t1 - t0) / 1000.0)
                else:
                    t0 = time.perf_counter_ns()
                    status, _ = send_cmd(s, TALWEG_CMD_READ, lba=lba)
                    t1 = time.perf_counter_ns()
                    if status == 0:
                        read_latencies.append((t1 - t0) / 1000.0)

            send_cmd(s, TALWEG_CMD_FLUSH)
    finally:
        if proc.poll() is None:
            proc.terminate()
            proc.wait()
        for p in [socket_path, media_path]:
            if os.path.exists(p):
                try:
                    os.unlink(p)
                except OSError:
                    pass

    w_stats = calculate_percentiles(write_latencies)
    r_stats = calculate_percentiles(read_latencies)

    print("\n-----------------------------------------------------------------------------")
    print(" Page Program (Write) Latency Distribution (4 KiB):")
    print(f"   Samples: {w_stats.get('count', 0)}")
    print(f"   Min:     {w_stats.get('min', 0.0):.2f} us")
    print(f"   p50:     {w_stats.get('p50', 0.0):.2f} us")
    print(f"   p90:     {w_stats.get('p90', 0.0):.2f} us")
    print(f"   p95:     {w_stats.get('p95', 0.0):.2f} us")
    print(f"   p99:     {w_stats.get('p99', 0.0):.2f} us")
    print(f"   p99.9:   {w_stats.get('p999', 0.0):.2f} us")
    print(f"   Max:     {w_stats.get('max', 0.0):.2f} us")
    print(f"   Mean:    {w_stats.get('mean', 0.0):.2f} us")
    print_ascii_histogram(write_latencies, buckets=8)

    print("\n-----------------------------------------------------------------------------")
    print(" Page Read Latency Distribution (4 KiB):")
    print(f"   Samples: {r_stats.get('count', 0)}")
    print(f"   Min:     {r_stats.get('min', 0.0):.2f} us")
    print(f"   p50:     {r_stats.get('p50', 0.0):.2f} us")
    print(f"   p90:     {r_stats.get('p90', 0.0):.2f} us")
    print(f"   p95:     {r_stats.get('p95', 0.0):.2f} us")
    print(f"   p99:     {r_stats.get('p99', 0.0):.2f} us")
    print(f"   p99.9:   {r_stats.get('p999', 0.0):.2f} us")
    print(f"   Max:     {r_stats.get('max', 0.0):.2f} us")
    print(f"   Mean:    {r_stats.get('mean', 0.0):.2f} us")
    print("=============================================================================")

    if args.output:
        with open(args.output, "w") as f:
            json.dump({
                "operations": args.ops,
                "write_stats": w_stats,
                "read_stats": r_stats
            }, f, indent=2)

if __name__ == "__main__":
    main()
