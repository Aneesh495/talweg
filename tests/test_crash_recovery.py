#!/usr/bin/env python3
"""
Talweg Power-Cut Crash Recovery and Journal Replay Test Suite
Validates crash consistency under simulated sudden power loss (SIGKILL),
verifies post-crash data integrity via SHA-256 hashes, and tests torn-write recovery.
"""

import unittest
import subprocess
import socket
import struct
import hashlib
import time
import os
import signal

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

class TalwegCrashRecoveryTest(unittest.TestCase):
    socket_path = "/tmp/talweg_crash_test.sock"
    media_path = "crash_recovery.flash"

    def setUp(self):
        self._cleanup()

    def tearDown(self):
        self._cleanup()

    def _cleanup(self):
        for path in [self.socket_path, self.media_path, "crash_test.flash", "crash_test.journal"]:
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

    def _wait_for_socket(self, timeout=3.0):
        start = time.time()
        while time.time() - start < timeout:
            if os.path.exists(self.socket_path):
                # Try connecting to ensure server is ready
                try:
                    test_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    test_sock.connect(self.socket_path)
                    test_sock.close()
                    return True
                except (socket.error, OSError):
                    pass
            time.sleep(0.05)
        return False

    def test_01_cli_crash_drill(self):
        """Run the built-in Monte Carlo crash drill CLI and verify 100% recovery."""
        cmd = ["./bin/talweg", "crash-drill", "--cycles", "500"]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"crash-drill failed: {res.stderr}")
        self.assertIn("Crash Consistency:     VERIFIED", res.stdout)
        self.assertIn("Replayed Transactions: 500", res.stdout)

    def test_02_sudden_sigkill_persistence(self):
        """Simulate power-loss by sending SIGKILL to server and verify data integrity on restart."""
        # Step 1: Format flash media
        subprocess.run(["./bin/talweg", "format", "--output", self.media_path, "--blocks", "128", "--pages", "64"],
                       check=True, stdout=subprocess.DEVNULL)

        # Step 2: Start server instance #1
        srv1 = subprocess.Popen(["./bin/talweg", "serve", "--socket", self.socket_path, "--media", self.media_path],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.assertTrue(self._wait_for_socket(), "Server #1 failed to open socket")

        golden_hashes = {}
        golden_payloads = {}

        # Step 3: Write 150 unique blocks across the LBA space
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(self.socket_path)
            for i in range(150):
                lba = (i * 13) % 2000
                pattern = f"CrashTestBlock_{i:06d}_".encode("ascii")
                payload = pattern * (4096 // len(pattern))
                payload += b"\x00" * (4096 - len(payload))
                
                status, _ = self._send_cmd(s, TALWEG_CMD_WRITE, lba=lba, payload=payload)
                self.assertEqual(status, 0)
                golden_hashes[lba] = hashlib.sha256(payload).hexdigest()
                golden_payloads[lba] = payload

            # Step 4: Issue explicit FLUSH
            status, _ = self._send_cmd(s, TALWEG_CMD_FLUSH)
            self.assertEqual(status, 0)

        # Step 5: Send SIGKILL to emulate catastrophic power failure
        os.kill(srv1.pid, signal.SIGKILL)
        srv1.wait()

        if os.path.exists(self.socket_path):
            os.unlink(self.socket_path)

        # Step 6: Start server instance #2 on the same persisted media
        srv2 = subprocess.Popen(["./bin/talweg", "serve", "--socket", self.socket_path, "--media", self.media_path],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            self.assertTrue(self._wait_for_socket(), "Server #2 failed to recover and listen")

            # Step 7: Read back all blocks and verify bit-for-bit SHA-256 match
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                s.connect(self.socket_path)
                for lba, expected_hash in golden_hashes.items():
                    status, payload = self._send_cmd(s, TALWEG_CMD_READ, lba=lba)
                    self.assertEqual(status, 0, f"Read failed for recovered LBA {lba}")
                    self.assertEqual(len(payload), 4096)
                    actual_hash = hashlib.sha256(payload).hexdigest()
                    self.assertEqual(actual_hash, expected_hash, f"Hash mismatch for recovered LBA {lba}")
                    self.assertEqual(payload, golden_payloads[lba])
        finally:
            if srv2.poll() is None:
                srv2.terminate()
                srv2.wait()

    def test_03_torn_tail_journal_recovery(self):
        """Verify journal parser cleanly stops at corrupted tail and recovers exact valid prefix."""
        cmd = ["./bin/test_journal_replay"]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"test_journal_replay failed: {res.stderr}")
        self.assertIn("PASS: All journal serialization and recovery tests passed.", res.stdout)

if __name__ == "__main__":
    unittest.main()
