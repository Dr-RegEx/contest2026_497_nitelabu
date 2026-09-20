"""Run unchanged fixture xTS apps after explicit wiring confirmation.

This script resets the board once. It refuses an occupied UART and verifies
an isolated FLAT image receipt. It never flashes, retries or changes tests.
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


def require(value, message):
    if not value:
        raise RuntimeError(message)


def command(port, text):
    print('XTS_COMMAND=' + text, flush=True)
    port.reset_input_buffer()
    for byte in text.encode() + b'\r':
        port.write(bytes([byte]))
        time.sleep(0.01)
    output = bytearray()
    started = time.monotonic()
    while time.monotonic() - started < 60:
        data = port.read(4096)
        if data:
            output.extend(data)
            print(data.decode(errors='replace'), end='', flush=True)
        require(not any(x in output for x in transport.BOOT_FAILURE_MARKERS),
                'target fatal error; no recovery reset performed')
        if b'nsh> ' in output:
            return output.decode(errors='replace')
    raise TimeoutError('fixture command timeout; no automatic retry/reset')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('case', choices=['gpio', 'bmi160'])
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--wiring-confirmed', action='store_true')
    args = parser.parse_args()
    require(args.wiring_confirmed, 'confirm actual documented fixture wiring first')
    lines = args.receipt.read_text().splitlines()
    require(len(lines) == 1, 'expected one FLAT kernel receipt')
    digest, filename = lines[0].split(maxsplit=1)
    firmware = Path(filename)
    require(hashlib.sha256(firmware.read_bytes()).hexdigest() == digest,
            'firmware receipt mismatch')
    config = (firmware.parent / '.config').read_text()
    for option in ('BUILD_FLAT', 'ESP32S31_XTS_' + args.case.upper()):
        require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    require('CONFIG_BUILD_KERNEL=y\n' not in config, 'expected fixture profile')
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(),
            'UART busy or occupancy check failed; do not interrupt longrun')
    print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.1,
                       write_timeout=1, exclusive=True) as port:
        transport.hard_reset(port)
        boot = transport.collect_until_prompt(port, 30)
        print(boot.decode(errors='replace'), flush=True)
        require(b'NuttShell (NSH)' in boot and not any(
            x in boot for x in transport.BOOT_FAILURE_MARKERS), 'abnormal boot')
        if args.case == 'gpio':
            require(b'/dev/gpio0=GPIO47 J2.13 /dev/gpio1=GPIO48 J2.14' in boot,
                    'GPIO fixture mapping marker missing')
            # Cover both levels and all original interrupt modes once.
            # HAL types RISING=1, FALLING=2, CHANGE=3, ONHIGH=5.
            for mode, level, hal_type in ((0, 1, 2), (1, 0, 1),
                                           (2, 1, 3), (3, 1, 5)):
                result = command(port, 'cmocka_driver_gpio -i /dev/gpio0 '
                                 f'-o /dev/gpio1 -l -p {level} -r {mode}')
                require(re.search(r'\[  PASSED  \] 4 test\(s\)\.', result)
                        and '[  FAILED  ]' not in result, 'original GPIO test failed')
                counts = re.findall(rf'S31_GPIO_IRQ pin=47 type={hal_type} count=(\d+)',
                                    result)
                require(any(int(n) > 0 for n in counts),
                        'no real IRQ observed; original poll may have timed out')
                print(f'XTS_GPIO_MODE={mode} PASS IRQ_CONFIRMED', flush=True)
        else:
            require(b'/dev/accel0 I2C0 addr=0x68 SCL=45 SDA=46' in boot,
                    'BMI160 fixture absent or registration failed')
            result = command(port, 'cmocka_driver_i2c_spi -d /dev/accel0')
            require(re.search(r'\[  PASSED  \] 1 test\(s\)\.', result)
                    and '[  FAILED  ]' not in result and 'I2C_TRANSFER failed' not in result,
                    'original BMI160 test failed')
            readings = re.findall(r'\[(\d+)\] -?\d+, -?\d+, -?\d+ / -?\d+, -?\d+, -?\d+',
                                  result)
            require(len(readings) == 100, 'original100 sensor readings incomplete')
            print('XTS_BMI160 PASS readings=100', flush=True)
    print('XTS_FIXTURE_COMPLETE=' + args.case, flush=True)


if __name__ == '__main__':
    main()
