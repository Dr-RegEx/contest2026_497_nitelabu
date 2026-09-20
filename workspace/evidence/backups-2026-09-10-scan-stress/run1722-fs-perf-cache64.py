"""Measure the original performance_test workload on the cache64 candidate."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import os
import re
import serial
import subprocess

D = Path(__file__).resolve().parent
ROOT = D.parent.parent
spec = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
kv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kv)

TAG = os.environ.get('PERF_TAG', '1722-cache64')
RECEIPT = Path(os.environ.get('PERF_RECEIPT', D / 'build1722-fs-perf-cache64.sha256'))
EXPECTED = os.environ.get('PERF_EXPECTED',
                          '41d24d3d0741fd4051f282b6f430fdc1a4d9d6b80c1bee5a7973701ac4307604')
OUT = D / ('performance' + TAG + '-board.log')
RESULT = D / ('performance' + TAG + '-board-result.json')


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def main():
    digest, name = RECEIPT.read_text().strip().split(maxsplit=1)
    image = Path(name)
    require(digest == EXPECTED and hashlib.sha256(image.read_bytes()).hexdigest() == digest,
            'candidate receipt/image mismatch')
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(), 'UART busy')
    lines = []

    def emit(text):
        print(text, flush=True)
        lines.append(text)

    port = serial.Serial(port=None, baudrate=115200, timeout=.1,
                         write_timeout=1, exclusive=True)
    port.dtr = False
    port.rts = False
    port.port = '/dev/ttyUSB0'
    with port:
        boot = kv.transport.collect_until_prompt(port, 30)
        require(b'NuttShell (NSH)' in boot and
                b'xTS Flash scratch: /dev/xtsflash offset=0xd00000 size=0x300000' in boot,
                'candidate boot/scratch marker missing')
        emit('XTS_BOOT_READY image_sha256=' + digest)

        def command(text, timeout=180):
            emit('XTS_COMMAND=' + text)
            out = kv.command(port, text, timeout=timeout)
            emit(out.rstrip())
            require(not re.search(r'ESP-ROM:|NuttShell \(NSH\)|rst:|PANIC|KASAN:|Assertion failed', out, re.I),
                    'target reset/fault during ' + text)
            require(not re.search(r'ERROR|failed|timed out', out, re.I),
                    'command failed: ' + text)
            return out

        command('mount -t littlefs -o forceformat /dev/xtsflash /data', 120)
        require('/data type littlefs' in command('mount'), 'scratch mount missing')
        command('df')
        command('mkdir /data/s11')
        rates = {}
        for mode, label in enumerate(('write', 'read', 'random write', 'random read'), 1):
            out = command(f'performance_test -d/data/s11 -b4096 -c64 -m{mode}', 600)
            match = re.search(re.escape(label) + r' speed is (\S+)', out)
            require(match is not None, 'missing ' + label + ' rate')
            value = float(match.group(1))
            require(math.isfinite(value) and value > 0, 'invalid ' + label + ' rate')
            rates[label] = value
            emit(f'XTS_PERF_MODE={mode} label={label} rate_KiB_s={value}')
        listing = command('ls -l /data/s11')
        require(re.search(r'\b262144\s+performance_test', listing), 'wrong output size')
        command('df')
    RESULT.write_text(json.dumps({'candidate': str(RECEIPT), 'image_sha256': digest,
                                  'workload': {'block': 4096, 'count': 64},
                                  'rates_KiB_s': rates}, indent=2) + '\n')
    OUT.write_text('\n'.join(lines) + '\n')
    print('XTS_PERF_RESULT=' + str(RESULT), flush=True)


if __name__ == '__main__':
    main()
