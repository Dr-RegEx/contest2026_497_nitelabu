"""Capture late output and attempt bounded NSH interrupt before any reset."""
import sys
import serial

sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import esp32s31_nuttx_smoke as transport

with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    ready = False
    for attempt in ('late-output', 'interrupt'):
        print('RECOVERY_STEP=' + attempt, flush=True)
        if attempt == 'interrupt':
            port.write(b'\x03\r\n')
        try:
            print(transport.collect_until_prompt(port, 5).decode(errors='replace'), flush=True)
            ready = True
            break
        except TimeoutError as error:
            print('TIMEOUT: ' + str(error), flush=True)
    if ready:
        for command in ('ps', 'cat /dev/s31stat', 'free'):
            transport.run_command(port, command, timeout=10)
    print('RECOVERED=' + str(ready), flush=True)
