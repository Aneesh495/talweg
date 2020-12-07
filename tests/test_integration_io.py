#!/usr/bin/env python3
"""
Talweg End-to-End Block I/O Integration Test Suite
Validates random read/write consistency, unmapped read zeroing, and TRIM semantics.
"""

import unittest
import subprocess
import socket
import struct
import time
import os
import shutil

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

class TalwegIntegrationTest(unittest.TestCase):
    socket_path = "/tmp/talweg_integ_test.sock"
    media_path = "test_integ.flash"
    server_proc = None

    @classmethod
    def setUpClass(cls):
        # Format media image
        subprocess.run(["./bin/talweg", "format", "--output", cls.media_path, "--blocks", "128", "--pages", "64"],
                       check=True, stdout=subprocess.DEVNULL)
        if os.path.exists(cls.socket_path):
            os.unlink(cls.socket_path)

        # Launch server daemon
        cls.server_proc = subprocess.Popen(["./bin/talweg", "serve", "--socket", cls.socket_path, "--media", cls.media_path],
                                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        # Wait for socket
        for _ in range(50):
            if os.path.exists(cls.socket_path):
                break
            time.sleep(0.05)

    @classmethod
    def tearDownClass(cls):
        if cls.server_proc:
            cls.server_proc.terminate()
            cls.server_proc.wait()
        for f in [cls.socket_path, cls.media_path]:
            if os.path.exists(f):
                try:
                    os.unlink(f)
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

    def test_01_ping(self):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(self.socket_path)
            status, _ = self._send_cmd(s, TALWEG_CMD_PING)
            self.assertEqual(status, 0)

    def test_02_write_read_consistency(self):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(self.socket_path)
            golden = {}
            for i in range(100):
                lba = i * 7
                data = os.urandom(4096)
                golden[lba] = data
                status, _ = self._send_cmd(s, TALWEG_CMD_WRITE, lba=lba, payload=data)
                self.assertEqual(status, 0)

            for lba, expected_data in golden.items():
                status, payload = self._send_cmd(s, TALWEG_CMD_READ, lba=lba)
                self.assertEqual(status, 0)
                self.assertEqual(len(payload), 4096)
                self.assertEqual(payload, expected_data)

    def test_03_trim_and_zero_read(self):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(self.socket_path)
            lba = 999
            data = b"T" * 4096
            status, _ = self._send_cmd(s, TALWEG_CMD_WRITE, lba=lba, payload=data)
            self.assertEqual(status, 0)

            status, payload = self._send_cmd(s, TALWEG_CMD_READ, lba=lba)
            self.assertEqual(payload, data)

            # Trim LBA
            status, _ = self._send_cmd(s, TALWEG_CMD_TRIM, lba=lba)
            self.assertEqual(status, 0)

            # Reading trimmed LBA returns zeroes
            status, payload = self._send_cmd(s, TALWEG_CMD_READ, lba=lba)
            self.assertEqual(status, 0)
            self.assertEqual(payload, b"\x00" * 4096)

    def test_04_stats_reporting(self):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(self.socket_path)
            status, payload = self._send_cmd(s, TALWEG_CMD_STATS)
            self.assertEqual(status, 0)
            self.assertGreaterEqual(len(payload), 56)

if __name__ == '__main__':
    unittest.main()
