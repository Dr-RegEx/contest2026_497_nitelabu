"""Observe the board's BLE advertising from a phone scan window.

This is a peer-observation aid; it does not claim an xTS interval or success
rate without the phone-side record.
"""
import json
import argparse
import re
import sys
import time
from pathlib import Path

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_nuttx_smoke as transport


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def run(interval_units, window_seconds, tag):
    result_dir = ROOT / f'backups/2026-09-10-scan-stress/ble1743-adv-{interval_units}-{tag}'
    result_dir.mkdir(exist_ok=False)
    log_path = result_dir / 'uart.log'
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=.1, write_timeout=1,
                       exclusive=True) as port, log_path.open('x') as log:
        transport.hard_reset(port)
        boot = transport.collect_until_prompt(port, 30)
        log.write(boot.decode(errors='replace'))
        require(b'NuttShell (NSH)' in boot, 'board did not boot')

        def shell(command, timeout=20):
            out = transport.run_command(port, command, timeout=timeout)
            log.write(out)
            log.flush()
            require(not re.search(r'nsh:|ERROR|failed|Assertion failed|PANIC:', out, re.I),
                    'shell command failed: ' + command)
            return out

        shell('ls /dev/ttyHCI0')
        shell('mkdir /data')
        shell('mount -t tmpfs /data')
        shell('mkdir /data/misc')
        shell('mkdir /data/misc/bt')
        def interactive(command, expected=None, prompt='bttool> ', timeout=30):
            # bttool's line editor is reliable with CRLF; LF-only writes can
            # leave the command queued without returning a prompt after BLE
            # adapter state callbacks.
            port.write((command + '\r\n').encode())
            out = ''
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                data = port.read(port.in_waiting or 1)
                if data:
                    text = data.decode(errors='replace')
                    out += text
                    log.write(text)
                    log.flush()
                    print(text, end='', flush=True)
                if prompt in out and (expected is None or re.search(expected, out)):
                    return out
            raise TimeoutError('missing prompt for ' + command)

        interactive('bttool')
        out = interactive('enable', r'Adapter state changed:\s*2', timeout=30)
        # Synchronize with a fresh command prompt before starting advertising.
        interactive('state', r'Adapter [Ss]tate:\s*2', timeout=30)
        out = interactive(f'adv start -i {interval_units} -n vela-adv-test -m legacy',
                          r'on_advertising_start_cb, handle:.*status:0', timeout=30)
        match = re.search(r'on_advertising_start_cb, handle:(0x[0-9a-fA-F]+), adv_id:(\d+), status:0', out)
        require(match, 'advertising callback missing')
        log.write(f'\nPHONE_SCAN_READY name=vela-adv-test interval_units={interval_units} window_seconds={window_seconds}\n')
        log.flush()
        print(f'PHONE_SCAN_READY name=vela-adv-test interval_units={interval_units} window_seconds={window_seconds}', flush=True)
        deadline = time.monotonic() + window_seconds
        while time.monotonic() < deadline:
            data = port.read(port.in_waiting or 1)
            if data:
                text = data.decode(errors='replace')
                log.write(text)
                log.flush()
                print(text, end='', flush=True)
        out = interactive('adv stop -h ' + match.group(1),
                          r'on_advertising_stopped_cb', timeout=30)
        require('on_advertising_stopped_cb' in out, 'advertising stop callback missing')
        interactive('disable', r'Adapter state changed:\s*0', timeout=30)
        interactive('quit', prompt='nsh> ')
    return {
        'status': 'BOARD_ADVERTISING_WINDOW_COMPLETE',
        'scope': 'PHONE_PEER_RECORD_REQUIRED_NOT_XTS_PASS',
        'name': 'vela-adv-test',
        'interval_units': interval_units,
        'window_seconds': window_seconds,
        'uart_log': str(log_path),
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--interval', type=int, required=True)
    parser.add_argument('--window', type=int, default=20)
    parser.add_argument('--tag', default='nrf')
    args = parser.parse_args()
    output = ROOT / f'backups/2026-09-10-scan-stress/ble1743-adv-{args.interval}-{args.tag}/result.json'
    try:
        value = run(args.interval, args.window, args.tag)
    except Exception as exc:
        value = {'status': 'FAIL', 'scope': 'PHONE_PEER_RECORD_REQUIRED_NOT_XTS_PASS',
                 'error': repr(exc)}
    output.write_text(json.dumps(value, indent=2) + '\n')
    print(json.dumps(value), flush=True)
    raise SystemExit(0 if value['status'] == 'BOARD_ADVERTISING_WINDOW_COMPLETE' else 1)
