"""Exercise GPIO61 through the documented auto-download circuit, not EN.

Schematic V1.0 page 2: physical DTR=0/RTS=1 means EN=1/BOOT=0.
pyserial DTR=True is asserted (physical low); RTS=False stays physical high.
This tests the BOOT electrical input and IRQ path, not the manual switch contacts.
"""
import re
import sys
import time
sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import serial
import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production


def collect(port, seconds):
    chunks = []
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        chunk = port.read(max(1, port.in_waiting)).decode(errors='replace')
        chunks.append(chunk)
        print(chunk, end='', flush=True)
    result = ''.join(chunks)
    production.require(not any(marker in result.encode()
                               for marker in transport.BOOT_FAILURE_MARKERS),
                       'fatal board output')
    production.require('ESP-ROM:' not in result, 'unexpected reset during input test')
    return result


with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.02,
                   write_timeout=1, exclusive=True) as port:
    production.boot(port, 'BUTTON_BOOT')
    port.rts = False
    port.dtr = False
    listing = transport.run_command(port, 'ls /dev/buttons', timeout=5)
    production.require('/dev/buttons' in listing and 'No such' not in listing,
                       'button device missing')
    pid = transport.launch_background(port, 'buttons', 'BUTTONS')
    production.require(pid is not None, 'buttons did not start')
    try:
        collect(port, 1.2)
        for cycle in range(1, 6):
            for asserted, state in ((True, 1), (False, 0)):
                port.dtr = asserted
                output = collect(port, 0.45)
                production.require(re.search(r'poll returned: 1\b', output) and
                                   re.search(rf'Sample = {state}\b', output),
                                   f'cycle {cycle} BOOT state {state} not notified')
                print(f'BUTTON_EDGE=PASS cycle={cycle} state={state}', flush=True)
        production.require(pid in transport.read_task_pids(port),
                           'button process exited or board reset')
        transport.run_command(port, 'cat /dev/s31stat', timeout=5)
        print('S31_BOOT_ELECTRICAL_IRQ=PASS cycles=5 edges=10 manual_switch=untested',
              flush=True)
    finally:
        port.dtr = False
        transport.run_command(port, f'kill {pid}', timeout=5)
