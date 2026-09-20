#!/usr/bin/env python3
"""Hardware SPI full-duplex check; opening ports can reset both boards."""
import argparse
import re
import time
import datetime
import serial
p=argparse.ArgumentParser()
p.add_argument('--master',default='/dev/ttyUSB0')
p.add_argument('--slave',default='/dev/ttyACM1')
p.add_argument('--log',required=True)
a=p.parse_args()
def port(name):
    s=serial.Serial(port=None,baudrate=115200,timeout=.05,exclusive=True)
    s.dtr=False;s.rts=False;s.port=name;s.open()
    return s
with open(a.log,'x') as log,port(a.master) as s,port(a.slave) as c:
    def record(name,data):
        text=data.decode(errors='replace')
        if text:
            log.write(f'[{name}] {text}\n');log.flush()
        return text
    log.write(str(datetime.datetime.now().astimezone())+'\n')
    time.sleep(3)
    boot=record('S31 boot',s.read_all());record('C6 boot',c.read_all())
    if 'nsh>' not in boot:
        s.write(b'\n');time.sleep(1);boot+=record('S31',s.read_all())
    if 'nsh>' not in boot: raise RuntimeError('S31 shell not ready')
    previous=None
    count=0
    patterns=[bytes(range(16)),bytes([0x55]*16),bytes([0xaa]*16),bytes([0]*16),bytes([255]*16),bytes(range(255,239,-1))]
    for freq in (100000,1000000):
        for data in patterns+[patterns[0]]:
            cmd=f'spi exch -f{freq} -x16 {data.hex()}'
            s.write((cmd+'\n').encode())
            raw=b'';deadline=time.monotonic()+4
            while time.monotonic()<deadline:
                raw+=s.read(4096)
                if b'nsh>' in raw: break
            text=record('S31',raw)
            time.sleep(.2)
            slave=record('C6',c.read_all())
            m=re.search(r'Received:\s*((?:[0-9A-F]{2}\s+){16})',text)
            if not m or re.search(r'ERROR:|too many arguments|failed|timed out',text,re.I): raise RuntimeError('SPI transfer missing or failed')
            result=bytes.fromhex(m[1])
            if previous is not None:
                if result!=previous: raise RuntimeError(f'Readback mismatch: {result.hex()} != {previous.hex()}')
                count+=1
            if 'bits=128' not in slave or ' '.join(f'{v:02x}' for v in data) not in slave.lower():
                raise RuntimeError('C6 receive data/length missing or mismatched')
            previous=data
        print(f'PASS {freq} Hz: C6 receive + S31 delayed echo',flush=True)
    log.write(f'PASS: 14 C6 frames verified, {count} S31 readbacks verified\n')
    print(f'PASS: 14 C6 frames, {count} S31 readbacks',flush=True)
