"""Published RAM/Flash footprint commands on each CPU; read-only target data."""
from pathlib import Path
import re
import sys

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_production_smoke as production

with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    production.boot(port, 'XTS_FOOTPRINT_BOOT')
    production.command(port, 'ifdown wlan0')
    tasks = production.command(port, 'ps')
    init_rows = [line for line in tasks.splitlines()
                 if re.search(r'\s(?:/system/bin/)?init\s*$', line)]
    production.require(len(init_rows) == 1, 'identify exactly one NSH init task')
    pid = int(init_rows[0].split()[0])
    affinity = production.command(port, f'taskset -p {pid}')
    match = re.search(r'current affinity mask: 0x([0-9a-f]+)', affinity)
    production.require(match is not None, 'cannot read original NSH affinity')
    original = int(match[1], 16)
    try:
        for cpu, mask in ((0, 1), (1, 2)):
            result = production.command(port, f'taskset -p {mask} {pid}')
            production.require(f'current affinity mask: 0x{mask:x}' in result,
                               'NSH affinity update not verified')
            production.command(port, f'cat /proc/{pid}/status')
            print(f'XTS_FOOTPRINT_CPU={cpu} AFFINITY={mask}', flush=True)
            ram = production.command(port, 'free')
            flash = production.command(port, 'df -h')
            production.require('Kmem' in ram and 'Page' in ram and
                               '/system/bin' in flash and '/apps' in flash and
                               'nsh:' not in ram + flash,
                               'footprint command failed or physical pools missing')
    finally:
        production.command(port, f'taskset -p {original} {pid}')
    print('XTS_COMMANDS=free,df-h PASS CPUs=0,1; shared SMP pools, do not double-count. Flash physical map still requires report.', flush=True)
