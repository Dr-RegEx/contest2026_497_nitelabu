"""Execute published reboot cases1.2.1/2.1.4; not a cold-power-cycle test."""
from datetime import datetime, timezone
from pathlib import Path
import re
import sys
import time

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_production_smoke as production

with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    production.boot(port, 'XTS_REBOOT_SETUP')
    samples = []
    for index in range(1, 11):
        port.reset_input_buffer()
        print(f'XTS_COMMAND=reboot sample={index} utc={datetime.now(timezone.utc).isoformat()}', flush=True)
        start = time.monotonic()
        for byte in b'reboot\r':
            port.write(bytes([byte]))
            time.sleep(0.01)
        output = production.transport.collect_until_prompt(port, 15)
        elapsed = time.monotonic() - start
        text = output.decode(errors='replace')
        print(text, flush=True)
        production.require(b'ESP-ROM:esp32s31' in output and
                           b'NuttShell (NSH)' in output and
                           b'CPU1 SMP online' in output,
                           'reboot did not reach complete NSH/SMP startup')
        production.require(not re.search(r'\berror\b|assertion failed|M-TRAP|Segmentation fault', text, re.I),
                           'abnormal reboot log')
        samples.append(elapsed)
        print(f'XTS_REBOOT_SAMPLE={index} PASS seconds={elapsed:.3f}', flush=True)
    average = sum(samples) / len(samples)
    production.require(len(samples) == 10 and average <= 6,
                       'ten reboot average exceeds documented six seconds')
    print(f'XTS_STANDARD=1.2.1,2.1.4 PASS samples=10 average_seconds={average:.3f}', flush=True)
    print('TIMING=host command-send through UART NSH prompt; includes UART/host overhead. No cold-power-cycle claim.', flush=True)
