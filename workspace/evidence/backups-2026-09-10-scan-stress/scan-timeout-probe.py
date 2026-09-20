"""Measure passive-scan completion without changing or associating networks."""
import sys
import time
import serial

sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production

with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    production.boot(port, 'SCAN_TIMEOUT_BOOT')
    for text in ('ifup wlan0', 'wapi scan wlan0', 'wapi pscan wlan0',
                 'sleep 3', 'wapi scan_results wlan0', 'ifdown wlan0',
                 'echo TIMEOUT_PROBE_ALIVE'):
        start = time.monotonic()
        transport.run_command(port, text, timeout=30)
        print(f'COMMAND_SECONDS={time.monotonic() - start:.3f}', flush=True)
