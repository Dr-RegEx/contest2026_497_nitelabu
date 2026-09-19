"""Three reset-based real-kernel event tests; no Wi-Fi credentials/writes."""
import re
import sys
import time
import serial

sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production

with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    for attempt in range(1, 4):
        started = time.monotonic()
        transport.hard_reset(port)
        output = transport.collect_until_prompt(port, 30.0)
        text = output.decode(errors='replace')
        print(text, flush=True)
        production.require(not any(marker in output for marker in
                                   transport.BOOT_FAILURE_MARKERS), 'fatal boot marker')
        for marker in ('NuttShell (NSH)', 'CPU1 SMP online',
                       'AppFS MTD boundary checks: PASS',
                       'Wi-Fi station netdev ready', 'EVENT_TEST=PASS cases=8 ret=0'):
            production.require(marker in text, 'missing marker: ' + marker)
        cases = re.findall(r'EVENT_TEST parent=(\d+) worker=(\d+) mode=(\d+) ret=(-?\d+)', text)
        expected = [(str(cpu), str(1 - cpu), str(mode), '0')
                    for cpu in range(2) for mode in range(4)]
        production.require(cases == expected, f'event cases differ: {cases!r}')
        production.require('ifdown wlan0...OK' in production.command(port, 'ifdown wlan0'),
                           'radio cleanup failed')
        print(f'EVENT_BOARD_ROUND={attempt} PASS cases=8 seconds={time.monotonic()-started:.3f}',
              flush=True)
print('EVENT_BOARD=PASS boots=3 cases=24', flush=True)
