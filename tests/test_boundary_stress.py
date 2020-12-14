#!/usr/bin/env python3
"""
Talweg Boundary Condition and Storage Exhaustion Stress Test Suite
Validates out-of-bounds LBA rejections, active garbage collection compaction
under continuous over-capacity writes, and large-scale TRIM semantics.
"""

import unittest
import subprocess
import socket
import struct
import random
import time
import os

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

class TalwegBoundaryStressTest(unittest.TestCase):
    socket_path = "/tmp/talweg_boundary_stress.sock"
    media_path = "boundary_stress.flash"
    server_proc = None

    @classmethod
    def setUpClass(cls):
        for path in [cls.socket_path, cls.media_path]:
            if os.path.exists(path):
                try:
                    os.unlink(path)
                except OSError:
                    pass

        # Format flash device with 64 blocks, 32 pages/block = 2048 physical pages
        subprocess.run(["./bin/talweg", "format", "--output", cls.media_path, "--blocks", "64", "--pages", "32"],
                       check=True, stdout=subprocess.DEVNULL)

        cls.server_proc = subprocess.Popen(["./bin/talweg", "serve", "--socket", cls.socket_path, "--media", cls.media_path],
                                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        for _ in range(60):
            if os.path.exists(cls.socket_path):
                try:
                    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    s.connect(cls.socket_path)
                    s.close()
                    break
                except (socket.error, OSError):
                    pass
            time.sleep(0.05)

    @classmethod
    def tearDownClass(cls):
        if cls.server_proc:
            cls.server_proc.terminate()
            cls.server_proc.wait()
        for path in [cls.socket_path, cls.media_path]:
            if os.path.exists(path):
                try:
                    os.unlink(path)
                except OSError:
                    pass

    def _recv_exact(self, s, n):
        buf = b""
        while len(buf) < n:
            chunk = s.recv(n - len(buf))
            if not chunk:
                break
            buf += chunk
        return buf

    def _send_cmd(self, s, cmd, lba=0, payload=b""):
        hdr = struct.pack(HDR_FORMAT, TALWEG_PROTO_MAGIC, cmd, 0, 1, lba, 1, 0, len(payload), 0)
        s.sendall(hdr)
        if payload:
            s.sendall(payload)
        resp_hdr_bytes = self._recv_exact(s, HDR_SIZE)
        self.assertEqual(len(resp_hdr_bytes), HDR_SIZE)
        magic, resp_cmd, flags, tag, r_lba, count, status, payload_len, crc = struct.unpack(HDR_FORMAT, resp_hdr_bytes)
        resp_payload = b""
        if payload_len > 0:
            resp_payload = self._recv_exact(s, payload_len)
            self.assertEqual(len(resp_payload), payload_len)
        return status, resp_payload

    def test_01_out_of_bounds_rejection(self):
        """Verify reading, writing, and trimming past user_lba_count fails with error."""
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(self.socket_path)
            
            # user_lba_count is default 10000
            oob_lba = 999999
            zero_buf = b"\x00" * 4096

            status, _ = self._send_cmd(s, TALWEG_CMD_READ, lba=oob_lba)
            self.assertNotEqual(status, 0, "Out-of-bounds read must return error status")

            status, _ = self._send_cmd(s, TALWEG_CMD_WRITE, lba=oob_lba, payload=zero_buf)
            self.assertNotEqual(status, 0, "Out-of-bounds write must return error status")

            status, _ = self._send_cmd(s, TALWEG_CMD_TRIM, lba=oob_lba)
            self.assertNotEqual(status, 0, "Out-of-bounds trim must return error status")

    def test_02_continuous_writes_trigger_gc_exhaustion(self):
        """Perform 2500 writes on a 2048-page medium to force multi-cycle autonomous GC compaction."""
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(self.socket_path)

            golden = {}
            random.seed(42)

            # Issue 2500 overwrites across 200 LBAs
            for i in range(2500):
                lba = random.randint(0, 199)
                data = (f"StressCycle_{i:06d}_LBA_{lba:04d}_").encode("ascii")
                data = data * (4096 // len(data))
                data += b"\x00" * (4096 - len(data))

                status, _ = self._send_cmd(s, TALWEG_CMD_WRITE, lba=lba, payload=data)
                self.assertEqual(status, 0, f"Write failed at cycle {i} for LBA {lba}")
                golden[lba] = data

            # Verify every active LBA still returns the exact latest written content
            for lba, expected in golden.items():
                status, payload = self._send_cmd(s, TALWEG_CMD_READ, lba=lba)
                self.assertEqual(status, 0, f"Read failed for LBA {lba}")
                self.assertEqual(payload, expected, f"Data corruption for LBA {lba} after GC stress")

            # Check stats: verify GC cycles executed and flash writes exceed user writes
            status, stats_payload = self._send_cmd(s, TALWEG_CMD_STATS)
            self.assertEqual(status, 0)
            self.assertGreaterEqual(len(stats_payload), 56)

            # Unpack stats struct: user_writes, flash_writes, gc_cycles, waf_mille, ...
            stats_fmt = "<QQQIIIIIII"
            user_w, flash_w, gc_c, waf, free_b, min_e, max_e, w_delta, bad_b, mapped_c = struct.unpack_from(stats_fmt, stats_payload, 0)
            self.assertGreaterEqual(user_w, 2500)
            self.assertGreaterEqual(flash_w, user_w)
            self.assertGreater(gc_c, 0, "Autonomous GC compaction cycles must be greater than zero")
            self.assertGreater(free_b, 0, "Free block pool must maintain non-zero headroom after compaction")

    def test_03_mass_trim_and_unmapped_state(self):
        """Verify mass TRIM unmaps entries and subsequent reads return all zero bytes."""
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(self.socket_path)

            # Write 50 blocks
            sample_lbqs = list(range(100, 150))
            for lba in sample_lbqs:
                payload = b"Z" * 4096
                status, _ = self._send_cmd(s, TALWEG_CMD_WRITE, lba=lba, payload=payload)
                self.assertEqual(status, 0)

            # Trim all 50 blocks
            for lba in sample_lbqs:
                status, _ = self._send_cmd(s, TALWEG_CMD_TRIM, lba=lba)
                self.assertEqual(status, 0)

            # Read back: must all be zeroes
            for lba in sample_lbqs:
                status, payload = self._send_cmd(s, TALWEG_CMD_READ, lba=lba)
                self.assertEqual(status, 0)
                self.assertEqual(payload, b"\x00" * 4096)

if __name__ == "__main__":
    unittest.main()
