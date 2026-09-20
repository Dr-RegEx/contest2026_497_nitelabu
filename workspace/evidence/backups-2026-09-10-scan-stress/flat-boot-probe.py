"""Bounded, credential-free M-mode startup and radio liveness diagnostic."""
import sys
import time
from pathlib import Path
import serial

sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import esp32s31_nuttx_smoke as transport

with Path(sys.argv[1]).open('x', buffering=1) as log:
    sys.stdout = log
    sys.stderr = log
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                       write_timeout=1, exclusive=True) as port:
        transport.hard_reset(port)
        start = time.monotonic()
        data = bytearray()
        while time.monotonic() - start < 15:
            chunk = port.read(port.in_waiting or 1)
            data.extend(chunk)
            print(chunk.decode(errors='replace'), end='', flush=True)
            if transport.PROMPT in data:
                break
        if transport.PROMPT not in data:
            raise SystemExit('FLAT_BOOT=FAIL no NSH prompt in 15 seconds')
        if any(marker in data for marker in transport.BOOT_FAILURE_MARKERS):
            raise SystemExit('FLAT_BOOT=FAIL fatal marker')
        print('FLAT_BOOT=PASS', flush=True)
        for command in ('ps', 'free', 'ifconfig', 'sleep 2', 'ifup wlan0',
                        'sleep 2', 'ifdown wlan0', 'sleep 2'):
            try:
                result = transport.run_command(port, command, timeout=8)
            except Exception as error:
                print(f'FLAT_COMMAND=FAIL command={command}: {error}', flush=True)
                raise
            if any(marker.decode() in result for marker in transport.BOOT_FAILURE_MARKERS):
                raise SystemExit('FLAT_COMMAND=FAIL fatal marker')
        print('FLAT_BOOT_RADIO=PASS', flush=True)
