"""Separate executable, socket-query and scan heap retention after settling."""
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
    if text == 'wapi show wlan0':
        print('SHOW_OPTIONAL_IOCTL_ERRORS=' + str(output.count('ERROR:')), flush=True)
    else:
        production.require('ERROR:' not in output and 'nsh:' not in output,
                           'command failed: ' + text)
    return output


def heap(port, label):
    command(port, 'sleep 3')
    output = command(port, 'free')
    match = re.search(r'(?m)^\s*\d+\s+(\d+)\s+\d+\s+\d+\s+\d+\s+(\d+)\s+\d+\s+Kmem', output)
    production.require(match is not None, 'Kmem row missing')
    used, blocks = map(int, match.groups())
    unnamed = output.count('(null)')
    print(f'HEAP {label} used={used} blocks={blocks} unnamed={unnamed}', flush=True)
    command(port, 'cat /dev/s31stat')
    command(port, 'ps')


try:
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                       write_timeout=1, exclusive=True) as port:
        production.boot(port, 'SCAN_HEAP_BOOT')
        heap(port, 'boot')
        for cycle in range(1, 6):
            command(port, 'wapi help')
            heap(port, f'help-{cycle}')
        command(port, 'ifup wlan0')
        heap(port, 'radio-up')
        for stage in ('show', 'scan', 'pscan'):
            for cycle in range(1, 6):
                output = command(port, f'wapi {stage} wlan0')
                if stage != 'show':
                    production.require('bssid / frequency / signal level / encode / ssid' in output,
                                       'scan result header missing')
                heap(port, f'{stage}-{cycle}')
        command(port, 'ifdown wlan0')
        heap(port, 'radio-down')
        command(port, 'ps')
    print('SCAN_HEAP_PROBE=COMPLETE; inspect trend, not a leak-free claim', flush=True)
except (RuntimeError, TimeoutError, serial.SerialException) as error:
    print('SCAN_HEAP_PROBE=FAIL: ' + str(error), flush=True)
    raise SystemExit(1)
