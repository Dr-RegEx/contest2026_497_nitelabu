"""Run the existing UART suite's write/read/burst modes over USB-UART.

Current source selects modes with -n0/1/2; the old checklist describes a
single combined run. Reuse the published host helper's payload and ten
burst samples, not a replacement device test. UART0 is the NSH console.
"""
from pathlib import Path
import re
import sys
import time

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
sys.path.insert(0, str(ROOT / 'openvela-dev/apps/testing/drivers/drivertest'))
import esp32s31_production_smoke as production
import test_content_gen as published


def collect(port, predicate, timeout=30):
    output = bytearray()
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        data = port.read(4096)
        if data:
            output.extend(data)
            print(data.decode(errors='replace'), end='', flush=True)
            production.require(not any(marker in output for marker in
                               (b'Assertion failed', b'Segmentation fault',
                                b'S31SM:M-TRAP')),
                               'target fault during original UART test')
            if predicate(output):
                return bytes(output)
    raise TimeoutError('UART standard test transport timed out')


def verdict(output):
    text = output.decode(errors='replace')
    production.require('[  PASSED  ] 1 test(s).' in text and
                       '[  FAILED  ]' not<REDACTED_CREDENTIAL>'[  SKIPPED ]' not in text
                       and len(re.findall(r'\[       OK \]', text)) == 1,
                       'original UART subcase failed')


with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    production.boot(port, 'XTS_UART_BOOT')
    production.command(port, 'ifdown wlan0')
    production.command(port, 'ls /dev/ttyS0')
    production.command(port, 'ls /system/bin/cmocka_driver_uart')
    for mode in range(3):
        command = f'cmocka_driver_uart -d /dev/ttyS0 -n {mode}'
        print('XTS_COMMAND=' + command, flush=True)
        port.reset_input_buffer()
        for byte in command.encode() + b'\r':
            port.write(bytes([byte]))
            time.sleep(0.01)
        initial = collect(port, lambda data: b'[ RUN' in data or
                          b'[ RUN      ]' in data or b'[ RUN ' in data or
                          re.search(rb'\[\s+RUN\s+\]', data))
        time.sleep(0.2)
        if mode == 1:
            published.s_write(port, published.DEFAULT_CONTENT, end='')
        elif mode == 2:
            for sample in range(10):
                # Same default ten samples and <=100-byte payload as the
                # published generator; do not alter device predicates.
                payload = published.fake_symbol(published.randint(1, 100))
                published.s_write(port, str(len(payload)))
                published.s_write(port, payload, end='')
                reply = bytearray()
                end = time.monotonic() + 5
                while len(reply) < len(payload) and time.monotonic() < end:
                    reply.extend(port.read(len(payload) - len(reply)))
                production.require(bytes(reply) == payload.encode(),
                                   'published UART burst echo mismatch')
                published.s_write(port, 'pass')
                print(f'UART_STANDARD_BURST={sample + 1} PASS bytes={len(payload)}', flush=True)
            published.s_write(port, '0')
        output = initial
        if b'nsh> ' not in output:
            output += collect(port, lambda data: b'nsh> ' in data)
        verdict(output)
        if mode == 0:
            production.require(published.DEFAULT_CONTENT.encode() in output,
                               'UART write payload differs from source')
        print(f'XTS_UART_MODE={mode} PASS', flush=True)
    print('XTS_STANDARD=1.3.10 PASS write/read/burst, published payload and10samples', flush=True)
