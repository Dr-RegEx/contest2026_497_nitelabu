"""Read existing HR timer status without reset, affinity edits or association."""
import re
import sys
from pathlib import Path
import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_production_smoke as production

port = serial.Serial(port=None, baudrate=115200, timeout=0.05,
                     write_timeout=1, exclusive=True)
port.dtr = False
port.rts = False
port.port = '/dev/ttyUSB0'
with port:
    if not port.is_open:
        port.open()
    production.command(port, 'uname -a')
    listing = production.command(port, 'ps')
    rows = [line for line in listing.splitlines()
            if re.search(r'\bhr_timer\b', line)]
    production.require(len(rows) == 1, 'expected one HR timer thread')
    pid = re.match(r'\s*(\d+)\s', rows[0])
    production.require(pid is not None, 'missing HR timer PID')
    for index in range(3):
        status = production.command(port, f'cat /proc/{pid[1]}/status')
        cpu = re.search(r'(?m)^CPU:\s*(---|\d+)\(0x([0-9a-fA-F]+)\)', status)
        production.require(cpu is not None, 'CPU/affinity status missing')
        print(f'HR_AFFINITY_SAMPLE={index} pid={pid[1]} cpu={cpu[1]} mask=0x{int(cpu[2], 16):x}', flush=True)
    production.command(port, 'cat /dev/s31stat')
print('HR_AFFINITY_READ=PASS observation_only=1 reset=0', flush=True)
