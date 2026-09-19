#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host receiver regressions for partial YMODEM packets and batch completion."""

import io
import os
from pathlib import Path
import tempfile
import time
import unittest

import sbrb


def packet(data, sequence, size):
    data = data.ljust(size, b"\0")
    head = sbrb.SOH if size == 128 else sbrb.STX
    crc = sbrb.calc_crc16(data)
    return (head + bytes((sequence & 255, 255 - (sequence & 255)))
            + data + crc.to_bytes(2, "big"))


class ReceiveTests(unittest.TestCase):
    def receive(self, size):
        payload = bytes(n & 255 for n in range(size))
        wire = packet(b"payload.bin\0" + str(size).encode(), 0, 128)
        position = 0
        sequence = 1
        while position < size:
            blocksize = 128 if size - position <= 128 else 1024
            wire += packet(payload[position:position + blocksize],
                           sequence, blocksize)
            position += blocksize
            sequence += 1
        wire += sbrb.EOT + packet(b"", 0, 128)
        source = io.BytesIO(wire)
        replies = []

        def read(count):
            # Keep the unmodified progress calculation's elapsed time nonzero.
            time.sleep(0.001)
            return source.read(count)

        receiver = sbrb.ymodem(read=read, write=replies.append,
                              progress=lambda message: None,
                              clear=lambda: None, maxretry=2)
        previous = os.getcwd()
        with tempfile.TemporaryDirectory(prefix="ymodem-recv-") as directory:
            try:
                os.chdir(directory)
                result = receiver.recv()
                actual = Path("payload.bin").read_bytes()
            finally:
                os.chdir(previous)
        self.assertIsNone(result)
        return payload, actual, replies

    def test_final_packet_keeps_only_remaining_file_bytes(self):
        for size in (128, 129, 1024, 1025):
            with self.subTest(size=size):
                expected, actual, _ = self.receive(size)
                self.assertEqual(actual, expected)

    def test_empty_batch_header_is_acknowledged(self):
        _, _, replies = self.receive(128)
        self.assertEqual(replies[-1], sbrb.ACK)


if __name__ == "__main__":
    unittest.main(verbosity=2)
