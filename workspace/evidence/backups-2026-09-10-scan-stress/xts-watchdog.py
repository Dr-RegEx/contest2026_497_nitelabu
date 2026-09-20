"""Run the four unchanged published watchdog modes in order.

Only one initial host reset. Modes 0..2 must panic and reset through the
actual RTC watchdog; the host never resets between them or retries a failure.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_nuttx_smoke as transport


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def trace_io(trace, direction, data=b''):
    trace.write(json.dumps({'monotonic_ns': time.monotonic_ns(),
                            'wall_time_ns': time.time_ns(),
                            'direction': direction, 'hex': data.hex()}) + '\n')
    trace.flush()


def prompt_tail(port, trace):
    """Retain the complete prompt tail; require 350 ms quiet within 1 s."""
    start = last_data = time.monotonic()
    tail = ''
    while time.monotonic() - start < 1.0:
        data = port.read(port.in_waiting or 1)
        if data:
            trace_io(trace, 'RX_PROMPT_TAIL', data)
            text = data.decode(errors='replace')
            tail += text
            print(text, end='', flush=True)
            last_data = time.monotonic()
        elif time.monotonic() - last_data >= 0.35:
            require('ESP-ROM:' not in tail and '[  FAILED  ]' not in tail and
                    'S31SM:M-TRAP' not in tail and 'Assertion failed' not in tail,
                    'unexpected reset/fault while waiting for prompt tail')
            trace_io(trace, 'PROMPT_QUIET')
            return tail
    raise TimeoutError('prompt did not become quiet within 1s; no command sent')


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--receipt', type=Path, required=True)
parser.add_argument('--io-log', type=Path, help='fresh JSONL raw TX/RX trace')
args = parser.parse_args()
lines = args.receipt.read_text().splitlines()
require(len(lines) == 1, 'one FLAT firmware receipt required')
digest, filename = lines[0].split(maxsplit=1)
firmware = Path(filename)
require(hashlib.sha256(firmware.read_bytes()).hexdigest() == digest,
        'firmware receipt mismatch')
config = (firmware.parent / '.config').read_text()
for option in ('BUILD_FLAT', 'ESP32S31_RWDT', 'ARCH_RV_HAVE_CLIC',
               'ARCH_HIPRI_INTERRUPT', 'BOARDCTL_RESET_CAUSE'):
    require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
require('CONFIG_BOARD_RESET_ON_ASSERT=0\n' in config,
        'software panic reset must not replace watchdog reset')

busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
require(busy.returncode == 1 and not busy.stderr.strip(),
        'UART occupied or occupancy check failed; no serial open/reset')

trace_path = args.io_log or Path(__file__).parent / 'logs' / (
    f'watchdog-io-{time.time_ns()}.jsonl')
print(f'XTS_IO_LOG={trace_path}', flush=True)
with trace_path.open('x') as trace, serial.Serial(
        '/dev/ttyUSB0', 115200, timeout=0.1,
        write_timeout=1, exclusive=True) as port:
    transport.hard_reset(port)
    boot = transport.collect_until_prompt(port, 30).decode(errors='replace')
    trace_io(trace, 'RX_INITIAL_BOOT', boot.encode())
    print(boot, flush=True)
    prompt_tail(port, trace)
    require('NuttShell (NSH)' in boot and
            re.search(r'rst:0x1\s', boot), 'initial power-on reset reason missing')
    listing = transport.run_command(port, 'ls /dev/watchdog0', timeout=10)
    require('No such file' not in listing and 'watchdog0' in listing,
            'watchdog device missing')
    trace_io(trace, 'RX_DEVICE_LISTING', listing.encode())
    prompt_tail(port, trace)
    for mode in range(4):
        command = f'cmocka_driver_watchdog -r {mode}'
        print('XTS_COMMAND=' + command, flush=True)
        print(f'XTS_TX_BEGIN mode={mode} monotonic={time.monotonic():.6f}',
              flush=True)
        data = command.encode() + b'\r'
        trace_io(trace, 'TX', data)
        require(port.write(data) == len(data), 'short serial write; no retry')
        port.flush()
        trace_io(trace, 'TX_FLUSHED')
        print(f'XTS_TX_FLUSHED mode={mode} monotonic={time.monotonic():.6f}',
              flush=True)
        started = time.monotonic()
        output = ''
        while time.monotonic() - started < 90:
            raw = port.read(port.in_waiting or 1)
            if raw:
                trace_io(trace, 'RX', raw)
            data = raw.decode(errors='replace')
            if data:
                output += data
                print(data, end='', flush=True)
            require('[  FAILED  ]' not in output and 'S31SM:M-TRAP' not in output,
                    'original watchdog mode failed or machine trap occurred')
            if mode < 3:
                if 'ESP-ROM:' in output and 'nsh> ' in output:
                    require(output.count('ESP-ROM:') == 1,
                            'unexpected multiple resets')
                    require('S31 RWDT timeout; hardware reset after panic dump'
                            in output and 'Assertion failed' in output and
                            'esp32s31_wdt.c' in output,
                            'watchdog-specific panic missing')
                    require('up_dump_register:' in output and
                            ('stack_dump:' in output or 'backtrace|' in output),
                            'actual interrupted registers/stack dump missing')
                    require(re.search(r'rst:0x10\s', output),
                            'hardware RTC-system watchdog reset reason missing')
                    require('NuttShell (NSH)' in output,
                            'watchdog reset did not reach NSH')
                    break
                require('nsh> ' not in output,
                        'watchdog mode returned without its required reset')
            elif 'nsh> ' in output:
                require('ESP-ROM:' not in output and 'Assertion failed' not in output,
                        'feeding/capture mode unexpectedly crashed or reset')
                require('[       OK ] drivertest_watchdog_api' in output and
                        re.search(r'\[  PASSED  \] 4 test\(s\)\.', output),
                        'original feed/status/capture/stop mode incomplete')
                break
        else:
            raise TimeoutError('watchdog mode timed out; no host reset performed')
        prompt_tail(port, trace)
        print(f'XTS_WATCHDOG_MODE={mode} PASS '
              f'seconds={time.monotonic()-started:.3f}', flush=True)
    print('XTS_STANDARD=1.3.15 PASS modes=0,1,2,3', flush=True)
