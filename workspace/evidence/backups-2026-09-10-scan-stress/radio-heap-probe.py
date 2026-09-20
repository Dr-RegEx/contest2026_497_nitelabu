"""Compare settled heap after bounded radio cycles, without AP credentials."""
import re
import sys
import serial

sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production


def command(port, text):
    output = transport.run_command(port, text, timeout=30)
    production.require(not any(marker.decode() in output
                               for marker in transport.BOOT_FAILURE_MARKERS),
                       'fatal marker during ' + text)
    production.require('ERROR:' not in output and 'nsh:' not in output,
                       'command failed: ' + text)
    return output


def heap(port, label):
    command(port, 'sleep 3')
    output = command(port, 'free')
    match = re.search(r'(?m)^\s*\d+\s+(\d+)\s+\d+\s+\d+\s+\d+\s+(\d+)\s+\d+\s+Kmem', output)
    production.require(match is not None, 'Kmem row missing')
    used, blocks = map(int, match.groups())
    print(f'HEAP {label} used={used} blocks={blocks}', flush=True)


try:
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                       write_timeout=1, exclusive=True) as port:
        production.boot(port, 'HEAP_BOOT')
        heap(port, 'boot')
        for cycle in range(1, 11):
            production.require('ifup wlan0...OK' in command(port, 'ifup wlan0'),
                               'ifup failed')
            command(port, 'sleep 1')
            production.require('ifdown wlan0...OK' in command(port, 'ifdown wlan0'),
                               'ifdown failed')
            heap(port, f'radio-{cycle}')
        command(port, 'ps')
    print('RADIO_HEAP_PROBE=COMPLETE; inspect trend, not a leak-free claim', flush=True)
except (RuntimeError, TimeoutError, serial.SerialException) as error:
    print('RADIO_HEAP_PROBE=FAIL: ' + str(error), flush=True)
    raise SystemExit(1)
