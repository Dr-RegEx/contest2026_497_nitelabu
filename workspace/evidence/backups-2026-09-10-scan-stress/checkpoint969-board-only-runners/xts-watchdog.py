"""Run the four unchanged published watchdog modes in order.

Only one initial host reset. Modes 0..2 must panic and reset through the
actual RTC watchdog; the host never resets between them or retries a failure.
"""
import argparse
import hashlib
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


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--receipt', type=Path, required=True)
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

with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.1,
                   write_timeout=1, exclusive=True) as port:
    transport.hard_reset(port)
    boot = transport.collect_until_prompt(port, 30).decode(errors='replace')
    print(boot, flush=True)
    require('NuttShell (NSH)' in boot and
            re.search(r'rst:0x1\s', boot), 'initial power-on reset reason missing')
    listing = transport.run_command(port, 'ls /dev/watchdog0', timeout=10)
    require('No such file' not in listing and 'watchdog0' in listing,
            'watchdog device missing')
    for mode in range(4):
        port.reset_input_buffer()
        command = f'cmocka_driver_watchdog -r {mode}'
        print('XTS_COMMAND=' + command, flush=True)
        for byte in command.encode() + b'\r':
            port.write(bytes([byte]))
            time.sleep(0.01)
        started = time.monotonic()
        output = ''
        while time.monotonic() - started < 90:
            data = port.read(port.in_waiting or 1).decode(errors='replace')
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
        print(f'XTS_WATCHDOG_MODE={mode} PASS '
              f'seconds={time.monotonic()-started:.3f}', flush=True)
    print('XTS_STANDARD=1.3.15 PASS modes=0,1,2,3', flush=True)
