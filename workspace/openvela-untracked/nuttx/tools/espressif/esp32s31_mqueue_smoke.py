#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Verify demo-rmt-mqueue boot tests; resets board, leaves WLAN disabled."""
import argparse
import re

import serial

import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production


def validate(output):
    text = output.decode(errors='replace')
    production.require(not any(marker in output for marker in
                               transport.BOOT_FAILURE_MARKERS), 'fatal boot marker')
    production.require('MQUEUE_TEST=FAIL' not in text, 'queue diagnostic failure')
    for marker in ('NuttShell (NSH)', 'CPU1 SMP online',
                   'AppFS MTD boundary checks: PASS',
                   'Wi-Fi station netdev ready'):
        production.require(marker in text, 'missing marker: ' + marker)
    summaries = re.findall(r'MQUEUE_TEST=(\w+) cases=(\d+) ret=(-?\d+)', text)
    production.require(summaries == [('PASS', '2', '0')], 'invalid summary')
    cases = re.findall(r'MQUEUE_CASE cpu=(\d+) rounds=(\d+) elapsed=(\d+) ret=(-?\d+)', text)
    production.require(len(cases) == 2, 'missing/duplicate queue cases')
    for cpu, case in enumerate(cases):
        production.require(case[0] == str(cpu) and case[1] == '64' and
                           int(case[2]) < 16 and case[3] == '0',
                           'invalid queue case: ' + repr(case))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--boots', type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.boots <= 20:
        parser.error('--boots must be 1..20')
    with serial.Serial(args.port, 115200, timeout=0.05,
                       write_timeout=1, exclusive=True) as port:
        for attempt in range(1, args.boots + 1):
            transport.hard_reset(port)
            output = transport.collect_until_prompt(port, 30.0)
            print(output.decode(errors='replace'), flush=True)
            validate(output)
            production.require('ifdown wlan0...OK' in
                               production.command(port, 'ifdown wlan0'),
                               'radio cleanup failed')
            print(f'MQUEUE_BOARD_ROUND={attempt} PASS cases=2', flush=True)
    print(f'MQUEUE_BOARD=PASS boots={args.boots} cases={args.boots * 2}', flush=True)


if __name__ == '__main__':
    main()
