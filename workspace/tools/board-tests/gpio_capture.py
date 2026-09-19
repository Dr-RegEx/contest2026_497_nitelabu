#!/usr/bin/env python3
"""Run GPIO case only after S31 GPIO47/48 are physically jumpered."""
import argparse
import re
import time
import serial
p=argparse.ArgumentParser()
p.add_argument('--port',default='/dev/ttyUSB0')
p.add_argument('--log',required=True)
a=p.parse_args()
s=serial.Serial(port=None,baudrate=115200,timeout=.1,exclusive=True)
s.dtr=False;s.rts=False;s.port=a.port
with open(a.log,'x') as log:
    def save(text):
        log.write(text);log.flush()
    s.open()
    try:
        time.sleep(3)
        boot=s.read_all().decode(errors='replace');save(boot)
        if 'nsh>' not in boot:
            s.write(b'\n');time.sleep(1)
            boot+=s.read_all().decode(errors='replace');save(boot)
        if 'nsh>' not in boot: raise RuntimeError('No NSH prompt')
        # Pinned application uses -i/-o, whereas the document says -a/-b.
        # Defaults already select gpio0/1, loop enabled and rising IRQ.
        s.write(b'cmocka_driver_gpio\n')
        text='';end=time.monotonic()+30
        while time.monotonic()<end:
            chunk=s.read(4096).decode(errors='replace')
            save(chunk);text+=chunk
            if 'nsh>' in text:break
        clean=re.sub(r'\x1b\[[0-9;]*[A-Za-z]','',text)
        if not re.search(r'\[\s*PASSED\s*\]\s*4 test',clean):
            raise RuntimeError('Original four tests did not all pass')
        if not re.search(r'S31_GPIO_IRQ pin=47 .*count=[1-9][0-9]*',clean):
            raise RuntimeError('No actual GPIO47 IRQ evidence (poll timeout alone is insufficient)')
        save('\nPASS: GPIO original four cases and nonzero hardware IRQ observed\n')
        print('PASS: GPIO four cases plus actual interrupt')
    finally:s.close()
