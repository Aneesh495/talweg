#!/usr/bin/env python3
"""
Talweg Write Amplification Factor (WAF) Empirical Evaluation Harness
Executes parameterized workloads across varying flash overprovisioning ratios
and calculates empirical WAF, GC compaction frequency, and endurance consumption.
"""

import argparse
import json
import math
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
TALWEG_CMD_GC = 7

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

def sample_zipfian(n, theta):
    """Generate a Zipfian distributed integer in [0, n - 1]."""
    alpha = 1.0 / (1.0 - theta)
    eta = (1.0 - math.pow(2.0 / n, 1.0 - theta)) / (1.0 - (math.zeta(theta) if hasattr(math, "zeta") else 1.6449))
    u = random.random()
    uz = u * 1.0
    if uz < 1.0:
        return 0
    return min(n - 1, int(n * math.pow(eta * u - eta + 1.0, alpha)))

def run_waf_sweep(workload_path, output_json=None):
    with open(workload_path, "r") as f:
        wl = json.load(f)

    name = wl.get("name", "Custom Workload")
    dist = wl.get("distribution", "uniform")
    ops = wl.get("total_operations", 3000)
    working_lbas = wl.get("working_set_lbas", 1024)
    op_sweep = wl.get("overprovisioning_sweep", [0.07, 0.14, 0.28])

    results = []
    socket_path = "/tmp/talweg_bench_waf.sock"
    media_path = "bench_waf.flash"

    print("=============================================================================")
    print(" Talweg Write Amplification Factor (WAF) Analysis")
    print("=============================================================================")
    print(f" Workload:     {name}")
    print(f" Distribution: {dist}")
    print(f" Operations:   {ops}")
    print(f" Active LBAs:  {working_lbas}")
    print("-----------------------------------------------------------------------------")
    print(" Overprovisioning | User Writes | Flash Writes | GC Cycles | Empirical WAF")
    print("-----------------------------------------------------------------------------")

    dummy_payload = b"W" * 4096

    for op_ratio in op_sweep:
        # Calculate physical block count to achieve OP ratio
        # OP = (phys_pages - user_lbas) / user_lbas
        pages_per_block = 32
        required_phys_pages = int(working_lbas * (1.0 + op_ratio))
        total_blocks = max(16, (required_phys_pages + pages_per_block - 1) // pages_per_block)

        # Cleanup prior run
        for p in [socket_path, media_path]:
            if os.path.exists(p):
                try:
                    os.unlink(p)
                except OSError:
                    pass

        # Format device
        subprocess.run(["./bin/talweg", "format", "--output", media_path,
                        "--blocks", str(total_blocks), "--pages", str(pages_per_block),
                        "--user-lba", str(working_lbas)],
                       check=True, stdout=subprocess.DEVNULL)

        # Start daemon
        proc = subprocess.Popen(["./bin/talweg", "serve", "--socket", socket_path, "--media", media_path],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        for _ in range(50):
            if os.path.exists(socket_path):
                break
            time.sleep(0.05)

        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                s.connect(socket_path)

                random.seed(1337)
                for i in range(ops):
                    if dist == "sequential":
                        lba = i % working_lbas
                    elif dist == "zipfian":
                        lba = sample_zipfian(working_lbas, wl.get("theta", 0.99))
                    else:
                        lba = random.randint(0, working_lbas - 1)

                    status, _ = send_cmd(s, TALWEG_CMD_WRITE, lba=lba, payload=dummy_payload)
                    if status != 0:
                        break

                send_cmd(s, TALWEG_CMD_FLUSH)

                # Query stats
                status, stats_payload = send_cmd(s, TALWEG_CMD_STATS)
                if status == 0 and len(stats_payload) >= 56:
                    stats_fmt = "<QQQIIIIIII"
                    user_w, flash_w, gc_c, waf_m, free_b, min_e, max_e, w_delta, bad_b, mapped_c = struct.unpack_from(stats_fmt, stats_payload, 0)
                    waf_val = flash_w / max(1, user_w)
                    results.append({
                        "overprovisioning": op_ratio,
                        "user_writes": user_w,
                        "flash_writes": flash_w,
                        "gc_cycles": gc_c,
                        "waf": waf_val
                    })
                    print(f"      {op_ratio*100:4.1f}%       |   {user_w:7d}   |   {flash_w:8d}   |   {gc_c:5d}   |    {waf_val:5.3f}")
                else:
                    print(f"      {op_ratio*100:4.1f}%       |     ERROR: STATS query failed")
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

    print("=============================================================================")

    if output_json:
        with open(output_json, "w") as f:
            json.dump({
                "workload": name,
                "distribution": dist,
                "operations": ops,
                "results": results
            }, f, indent=2)
        print(f"Results written to {output_json}")

def main():
    parser = argparse.ArgumentParser(description="Talweg Empirical WAF Evaluation Harness")
    parser.add_argument("--workload", default="benchmarks/workloads/zipfian_update.json",
                        help="Path to workload JSON profile")
    parser.add_argument("--output", help="Optional output JSON path for benchmark telemetry")
    args = parser.parse_args()

    if not os.path.exists(args.workload):
        print(f"Error: Workload file not found: {args.workload}")
        sys.exit(1)

    run_waf_sweep(args.workload, args.output)

if __name__ == "__main__":
    main()
