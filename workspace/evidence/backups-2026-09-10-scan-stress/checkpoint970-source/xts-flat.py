"""Run original FLAT xTS cases with explicit RAM or isolated Flash scratch."""
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


def command(port, text, timeout=120):
    print('XTS_COMMAND=' + text, flush=True)
    port.reset_input_buffer()
    for byte in text.encode() + b'\r\n':
        port.write(bytes([byte]))
        time.sleep(0.01)
    output = bytearray()
    started = time.monotonic()
    while time.monotonic() - started < timeout:
        data = port.read(4096)
        if data:
            output.extend(data)
            print(data.decode(errors='replace'), end='', flush=True)
        if any(marker in output for marker in transport.BOOT_FAILURE_MARKERS):
            end = time.monotonic() + 3
            while time.monotonic() < end:
                print(port.read(4096).decode(errors='replace'), end='', flush=True)
            raise RuntimeError('target fatal error; full dump retained')
        if b'nsh> ' in output:
            return output.decode(errors='replace')
    raise TimeoutError('command did not finish; no reset performed')


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('case', choices=['heap', 'ramblock', 'flashblock', 'rtc'])
parser.add_argument('--no-boot', action='store_true')
parser.add_argument('--receipt', type=Path,
                    help='successful FLAT image receipt; required for Flash')
parser.add_argument('--flash-backup', type=Path,
                    help='verified blank scratch backup manifest')
args = parser.parse_args()
if args.case == 'flashblock':
    require(not args.no_boot and args.flash_backup is not None
            and args.receipt is not None,
            'Flash test requires receipt, fresh boot and scratch backup')
    manifest = json.loads(args.flash_backup.read_text())
    require(manifest['offset'] == 0xc00000 and manifest['size'] == 0x100000
            and manifest['blank'] is True and len(manifest['files']) == 2,
            'only the verified blank 1 MiB scratch range may be overwritten')
    for entry in manifest['files']:
        data = (args.flash_backup.parent / entry['name']).read_bytes()
        require(len(data) == 0x100000 and data == b'\xff' * len(data)
                and hashlib.sha256(data).hexdigest() == entry['sha256'],
                'Flash backup contents or digest mismatch')
    print('XTS_FLASH_BACKUP=' + str(args.flash_backup.resolve()), flush=True)
if args.receipt is not None:
    lines = args.receipt.read_text().splitlines()
    require(len(lines) == 1, 'one FLAT kernel receipt required')
    digest, name = lines[0].split(maxsplit=1)
    firmware = Path(name)
    require(firmware.name == 'nuttx.bin' and
            hashlib.sha256(firmware.read_bytes()).hexdigest() == digest,
            'firmware receipt mismatch')
    config = (firmware.parent / '.config').read_text()
    require('CONFIG_BUILD_FLAT=y\n' in config, 'FLAT image required')
    if args.case == 'flashblock':
        require('CONFIG_ESP32S31_XTS_FLASH=y\n' in config,
                'isolated Flash scratch profile required')
        if 'CONFIG_ESPRESSIF_SPIRAM_USER_HEAP=y\n' in config:
            require('CONFIG_ESP32S31_SPIFLASH_PSRAM_STACK=y\n' in config,
                    'PSRAM-stack Flash dispatch required')
    print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
require(busy.returncode == 1 and not busy.stderr.strip(),
        'UART occupied or occupancy check failed; preserve active longrun')
with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.1,
                   write_timeout=1, exclusive=True) as port:
    if not args.no_boot:
        transport.hard_reset(port)
        boot = transport.collect_until_prompt(port, 30)
        print(boot.decode(errors='replace'), flush=True)
        require(b'NuttShell (NSH)' in boot and not any(
            marker in boot for marker in transport.BOOT_FAILURE_MARKERS),
            'FLAT firmware did not boot normally')
        if args.case == 'flashblock':
            require(b'xTS Flash scratch: /dev/xtsflash offset=0xc00000 '
                    b'size=0x100000' in boot,
                    'expected isolated Flash partition marker missing')
    command(port, 'uname -a')
    command(port, 'free')
    mounts = command(port, 'mount')
    require('/system/bin type romfs' not in mounts and
            '/apps type littlefs' not in mounts,
            'expected isolated FLAT test image, not production demo')
    if args.case == 'ramblock':
        listing = command(port, 'ls /dev/ram10')
        require('No such file' in listing, 'RAM minor10 is not fresh; refusing write')
        created = command(port, 'mkrd -m 10 -s 1000 1024')
        require('nsh:' not in created and 'ERROR' not in created,
                'original RAM disk creation failed')
        listing = command(port, 'ls /dev/ram10')
        require('No such file' not in listing and 'ram10' in listing,
                'fresh RAM disk missing')
        test = 'cmocka_driver_block -m /dev/ram10'
    elif args.case == 'flashblock':
        listing = command(port, 'ls /dev/xtsflash')
        require('No such file' not in listing and 'xtsflash' in listing,
                'isolated Flash MTD device missing')
        test = 'cmocka_driver_block -m /dev/xtsflash'
    elif args.case == 'rtc':
        command(port, 'ls /dev/rtc0')
        test = 'cmocka_driver_rtc'
    else:
        test = 'mm'
    started = time.monotonic()
    result = command(port, test, 1200)
    require('nsh:' not in result and 'ERROR' not in result,
            'original test reported an error')
    require('Total block size too small' not in result,
            'cache-write case must not silently skip a small partition')
    if args.case == 'heap':
        require('TEST COMPLETE' in result and
                not re.search(r'failed|skipping', result, re.I),
                'heap test incomplete, failed, or skipped allocations')
    else:
        runs = re.findall(r'\[==========\].*?: (\d+) test\(s\) run\.', result)
        passed = re.findall(r'\[  PASSED  \] (\d+) test\(s\)\.', result)
        checks = re.findall(r'\[       OK \] (\S+)', result)
        require(runs == passed == ['3'] and len(set(checks)) == 3 and
                '[  FAILED  ]' not in result and '[  SKIPPED ]' not in result,
                'all three original subcases must pass')
        if args.case == 'rtc':
            require('rtc periodic callback trigger!!!' in result,
                    'periodic test must actually receive a callback')
    print(f'XTS_STANDARD={test} PASS seconds={time.monotonic()-started:.3f}',
          flush=True)
    command(port, 'free')
