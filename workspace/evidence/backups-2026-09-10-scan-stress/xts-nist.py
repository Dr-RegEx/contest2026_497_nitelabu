"""Execute the published NIST-STS 400000-bit/10-stream/all-tests sequence.

Default is the exact published sequence. --streams 100 is an explicitly
separate supplemental run for excursion sample coverage, not a replacement
for the ten-stream record. Algorithms, bit count and thresholds are unchanged.
Output stays in a freshly checked volatile /tmp tree.
"""
import argparse
from pathlib import Path
import re
import sys
import time

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_production_smoke as production

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--streams', type=int, choices=(10, 100), default=10)
args = parser.parse_args()
print(f'NIST_RUN_KIND={"published" if args.streams == 10 else "supplemental"} '
      f'bits=400000 streams={args.streams} algorithms=15', flush=True)

CASES = ('ApproximateEntropy CumulativeSums Frequency LongestRun '
         'OverlappingTemplate RandomExcursionsVariant Runs Universal '
         'BlockFrequency FFT LinearComplexity NonOverlappingTemplate '
         'RandomExcursions Rank Serial').split()


def paced(port, text):
    print('XTS_INPUT=' + text, flush=True)
    for byte in text.encode() + b'\r':
        port.write(bytes([byte]))
        time.sleep(0.01)


def collect(port, marker=None, timeout=30):
    output = bytearray()
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        data = port.read(4096)
        if data:
            output.extend(data)
            print(data.decode(errors='replace'), end='', flush=True)
            production.require(not any(fault in output for fault in
                               (b'Assertion failed', b'Segmentation fault',
                                b'S31SM:M-TRAP', b'File Error:',
                                b'READ ERROR:', b'Statistical Testing Aborted')),
                               'NIST aborted or target fault; retain log')
            if b'nsh> ' in output:
                production.require(marker is None, 'NIST exited before input prompt')
                return output.decode(errors='replace')
            if marker is not None and marker in output:
                return output.decode(errors='replace')
    raise TimeoutError('NIST transport timed out')


with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    production.boot(port, 'XTS_NIST_BOOT')
    production.command(port, 'ifdown wlan0')
    mounts = production.command(port, 'mount')
    production.require('/tmp type tmpfs' in mounts, 'volatile scratch required')
    for path in ('/tmp/experiments', '/tmp/templates'):
        listing = production.command(port, 'ls ' + path)
        production.require('No such file' in listing, 'existing NIST data: inspect first')
    production.command(port, 'cd /tmp')
    for case in CASES:
        production.command(port, 'mkdir -p experiments/AlgorithmTesting/' + case)
    production.command(port, 'mkdir templates')
    production.command(port, 'cp /system/bin/nist-template9 /tmp/templates/template9')
    production.command(port, 'ls -l /tmp/templates/template9')
    production.command(port, 'free')
    print(f'XTS_COMMAND=nist_sts 400000; inputs=0,/dev/urandom/,1,0,{args.streams},1', flush=True)
    started = time.monotonic()
    paced(port, 'nist_sts 400000')
    # Original NIST prompts omit newlines and fflush; with line-buffered
    # target stdio they are not visible before scanf blocks. Synchronize on
    # the flushed menu and send the six documented input lines in order.
    collect(port, b'G Using SHA-1')
    for answer in ('0', '/dev/urandom/', '1', '0', str(args.streams), '1'):
        paced(port, answer)
        time.sleep(0.2)
    result = collect(port, timeout=7200)
    production.require('Statistical Testing Complete' in result,
                       'NIST did not complete')
    report = production.command(port, 'cat /tmp/experiments/AlgorithmTesting/finalAnalysisReport.txt')
    frequencies = production.command(port, 'cat /tmp/experiments/AlgorithmTesting/freq.txt')
    production.require(len(re.findall(r'BITSREAD = 400000', frequencies)) == args.streams,
                       f'expected {args.streams} complete 400000-bit input streams')
    rows = [line for line in report.splitlines()
            if re.match(r'^\s*\d+\s+\d+\s+\d+', line)]
    production.require(len(rows) == 188 and 'P-VALUE' in report,
                       'NIST final report incomplete')
    flagged = [line for line in rows if '*' in line or '----' in line]
    production.require(not flagged, 'NIST report contains failed/unavailable statistics')
    for row in rows:
        fields = row.split()
        production.require(float(fields[10]) > 0.0001,
                           'NIST P-value does not exceed published threshold')
    label = 'XTS_STANDARD' if args.streams == 10 else 'XTS_SUPPLEMENTAL'
    print(f'{label}=1.3.16 PASS streams={args.streams} rows={len(rows)} '
          f'seconds={time.monotonic()-started:.3f}', flush=True)
