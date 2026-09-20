#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Reject incomplete, slow, duplicate and failing board queue transcripts."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / 'espressif'))
from esp32s31_mqueue_smoke import validate

good = (b'CPU1 SMP online\nAppFS MTD boundary checks: PASS\n'
        b'MQUEUE_CASE cpu=0 rounds=64 elapsed=0 ret=0\n'
        b'MQUEUE_CASE cpu=1 rounds=64 elapsed=1 ret=0\n'
        b'MQUEUE_TEST=PASS cases=2 ret=0\nWi-Fi station netdev ready\n'
        b'NuttShell (NSH)\n')
validate(good)
for bad in (
    good.replace(b'cpu=1', b'cpu=0'),
    good.replace(b'elapsed=1', b'elapsed=16'),
    good.replace(b'rounds=64', b'rounds=63'),
    good.replace(b'ret=0', b'ret=-1'),
    good.replace(b'MQUEUE_TEST=PASS', b'MQUEUE_TEST=FAIL'),
    good.replace(b'Wi-Fi station netdev ready', b''),
    good + b'MQUEUE_CASE cpu=1 rounds=64 elapsed=0 ret=0\n',
    good + b'MQUEUE_TEST=PASS cases=2 ret=0\n',
    good + b'Assertion failed\n',
):
    try:
        validate(bad)
    except RuntimeError:
        continue
    raise AssertionError('accepted malformed queue transcript')
print('MQUEUE_PARSER=PASS positive/negative cases')
