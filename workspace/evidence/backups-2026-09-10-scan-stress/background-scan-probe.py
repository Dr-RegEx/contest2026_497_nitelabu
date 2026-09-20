"""Capture a stalled scan without blocking NSH in the foreground job."""
import sys
import serial

sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production

with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    try:
        production.boot(port, 'BACKGROUND_SCAN_BOOT')
        transport.run_command(port, 'ifup wlan0', timeout=15)
        transport.launch_background(port, 'wapi scan wlan0', 'SCAN_DIAGNOSTIC')
        for command in ('sleep 5', 'ps', 'cat /dev/s31stat',
                        'cat /proc/2/status', 'cat /proc/5/status'):
            transport.run_command(port, command, timeout=12)
        print('BACKGROUND_SCAN_CAPTURE=COMPLETE; not a functional pass', flush=True)
    finally:
        production.boot(port, 'BACKGROUND_SCAN_CLEANUP')
        transport.run_command(port, 'ifdown wlan0', timeout=15)
