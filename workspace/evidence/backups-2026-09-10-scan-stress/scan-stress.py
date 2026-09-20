"""Read-only radio lifecycle stress. No AP association or credentials."""
import re
import sys
import time
import serial

sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production

def command(port, text):
    start = time.monotonic()
    output = transport.run_command(port, text, timeout=30)
    production.require(not any(marker.decode() in output
                               for marker in transport.BOOT_FAILURE_MARKERS),
                       'fatal marker during ' + text)
    production.require('ERROR:' not in output and 'nsh:' not in output,
                       'command failed: ' + text)
    print(f'ELAPSED={time.monotonic() - start:.3f}', flush=True)
    return output

try:
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                       write_timeout=1, exclusive=True) as port:
        production.boot(port, 'STRESS_BOOT')
        command(port, 'free')
        for cycle in range(1, 11):
            production.require('ifup wlan0...OK' in command(port, 'ifup wlan0'), 'ifup failed')
            for mode in ('scan', 'pscan'):
                output = command(port, 'wapi ' + mode + ' wlan0')
                production.require('bssid / frequency / signal level / encode / ssid' in output,
                                   'result header missing')
                count = len(re.findall(r'(?mi)^(?:[0-9a-f]{2}:){5}[0-9a-f]{2}\s', output))
                print(f'STRESS_{cycle}_{mode}=PASS AP_COUNT={count}', flush=True)
            command(port, 'sleep 2')
            production.require('ifdown wlan0...OK' in command(port, 'ifdown wlan0'), 'ifdown failed')
            command(port, 'free')
        command(port, 'ps')
        production.require(production.output_line(command(port, 'echo STRESS_ALIVE'), 'STRESS_ALIVE'),
                           'echo missing')
    print('SCAN_STRESS=PASS cycles=10 scans=20; no networking acceptance', flush=True)
except (RuntimeError, TimeoutError, serial.SerialException) as error:
    print('SCAN_STRESS=FAIL: ' + str(error), flush=True)
    raise SystemExit(1)
