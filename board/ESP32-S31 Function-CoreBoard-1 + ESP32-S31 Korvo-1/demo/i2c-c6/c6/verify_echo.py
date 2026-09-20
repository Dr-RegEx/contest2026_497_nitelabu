#!/usr/bin/env python3
"""Verify real S31 master/C6 slave echo. Opening ports may reset boards."""
import argparse
import datetime
import re
import time
from pathlib import Path
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--master', default='/dev/ttyUSB0')
parser.add_argument('--slave', default='/dev/ttyACM0')
parser.add_argument('--log', required=True)
args = parser.parse_args()

def open_port(name):
    port = serial.Serial(port=None, baudrate=115200, timeout=0.05, exclusive=True)
    port.dtr = False
    port.rts = False
    port.port = name
    port.open()
    return port

with Path(args.log).open('x') as log, open_port(args.master) as master, open_port(args.slave) as slave:
    def capture(label, data):
        text = data.decode(errors='replace')
        if text:
            log.write(f'[{label}] {text}\n')
            log.flush()
        return text

    log.write(f'{datetime.datetime.now().astimezone().isoformat()}\nS31 GPIO45=SCL -> C6 GPIO4; GPIO46=SDA -> C6 GPIO5; GND common\n')
    time.sleep(3)
    capture('S31 boot', master.read_all())
    capture('C6 boot', slave.read_all())

    def command(cmd):
        master.write((cmd + '\n').encode())
        result = b''
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            result += master.read(4096)
            capture('C6', slave.read_all())
            if b'nsh> ' in result:
                break
        text = capture('S31', result)
        if b'nsh> ' not in result or re.search(r'failed|timed out|Transfer error', text, re.I):
            raise RuntimeError(f'Command failed: {cmd}')
        return text

    count = 0
    for frequency in (100000, 400000):
        for width in (8, 16):
            mask = (1 << width) - 1
            for value in (0, mask, 0x55 & mask, 0xaa & mask, 0x1234 & mask, 0xabcd & mask, 1, mask - 1):
                options = f'-a68 -f{frequency} -w{width}'
                result = command(f'i2c set {options} {value:x}')
                if 'WROTE Bus:' not in result:
                    raise RuntimeError('Missing write result')
                result = command(f'i2c get {options}')
                match = re.search(r'READ Bus:.*Value: ([0-9a-fA-F]+)', result)
                if not match or int(match[1], 16) != value:
                    raise RuntimeError(f'Echo mismatch expected {value:x}')
                count += 1
            print(f'PASS {frequency} Hz, {width}-bit payload: 8/8 round trips', flush=True)
    time.sleep(.2)
    tail = capture('C6', slave.read_all())
    log.write(f'PASS: {count}/{count} round trips, 64 bus transactions\n')
    print(f'PASS: {count}/{count} round trips', flush=True)
