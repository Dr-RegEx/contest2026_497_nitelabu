"""Case 1.3.11: existing sb/rb and sbrb.py, binary file round trips.

Fresh host fixtures and target tmpfs only. No persistent Flash files touched.
Protocol implementation remains in the repository's YMODEM tools.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import sys
import tempfile
import time

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
sys.path.insert(0, str(ROOT / 'openvela-dev/apps/system/ymodem'))
import esp32s31_production_smoke as p
import sbrb

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--tag', required=True)
args = parser.parse_args()
p.require(re.fullmatch(r'xts[0-9]+', args.tag), 'invalid volatile test tag')
remote = '/tmp/' + args.tag
host = Path(tempfile.mkdtemp(prefix=args.tag + '-ymodem-',
                            dir=ROOT / 'backups/2026-09-10-scan-stress'))
(host / 'send').mkdir()
(host / 'receive').mkdir()
files = []
for name, size in (('small.bin', 129), ('binary.bin', 65573)):
    path = host / 'send' / name
    path.write_bytes(bytes(range(256)) * (size // 256) + bytes(range(size % 256)))
    files.append(path)
print(f'XTS_HOST_FIXTURES={host}', flush=True)

with serial.Serial('/dev/ttyUSB0', 115200, timeout=.1, write_timeout=3,
                   exclusive=True) as port:
    p.boot(port, 'XTS_BOOT')
    p.command(port, 'ifdown wlan0')
    p.require('/tmp type tmpfs' in p.command(port, 'mount'), 'tmpfs required')
    p.require('No such file' in p.command(port, 'ls ' + remote),
              'refusing to overwrite an existing test directory')
    p.command(port, 'mkdir ' + remote)
    p.command(port, 'ls /system/bin/sb')
    p.command(port, 'ls /system/bin/rb')

    def start(command):
        p.require(len(command) <= 63, 'NSH command too long')
        print('XTS_COMMAND=' + command, flush=True)
        port.reset_input_buffer()
        for byte in command.encode() + b'\r':
            port.write(bytes([byte]))
            time.sleep(.01)
        echo = bytearray()
        end = time.monotonic() + 5
        while not echo.endswith(b'\n') and time.monotonic() < end:
            echo.extend(port.read(1))
        p.require(command.encode() in echo, 'missing command echo')

    def transfer(upload, paths):
        deadline = time.monotonic() + 300

        def read(size):
            output = bytearray()
            until = min(deadline, time.monotonic() + 2)
            while len(output) < size and time.monotonic() < until:
                output.extend(port.read(size - len(output)))
            p.require(time.monotonic() < deadline, 'transfer deadline reached')
            return bytes(output)

        protocol = sbrb.ymodem(read=read, write=port.write,
                              clear=port.reset_input_buffer, maxretry=10,
                              progress=lambda value: print(value, end='', flush=True))
        started = time.monotonic()
        result = protocol.send([str(path) for path in paths]) if upload else protocol.recv()
        p.require(result is None, f'YMODEM returned {result}')
        output = p.transport.collect_until_prompt(port, 30)
        print(output.decode(errors='replace'), flush=True)
        p.require(b'nsh> ' in output, 'transfer did not return to NSH')
        status = p.command(port, 'echo $?')
        p.require(re.search(r'(?m)^0\r?$', status), 'target transfer exit not zero')
        print(f'XTS_DIRECTION={"host-to-board" if upload else "board-to-host"} '
              f'seconds={time.monotonic()-started:.3f}', flush=True)

    start('rb -f ' + remote)
    transfer(True, files)
    p.command(port, 'ls -l ' + remote)
    previous = os.getcwd()
    try:
        os.chdir(host / 'receive')
        for source in files:
            start('sb ' + remote + '/' + source.name)
            transfer(False, [])
            actual = (host / 'receive' / source.name).read_bytes()
            p.require(actual == source.read_bytes(), 'round-trip content mismatch')
            print(f'XTS_FILE={source.name} bytes={len(actual)} '
                  f'sha256={hashlib.sha256(actual).hexdigest()} PASS', flush=True)
    finally:
        os.chdir(previous)
    p.command(port, 'free')
print('XTS_STANDARD=UART_FILE_SEND_RECEIVE PASS', flush=True)
