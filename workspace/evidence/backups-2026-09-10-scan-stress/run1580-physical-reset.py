"""Capture a user-operated RESET; never toggle modem lines or send reboot.

Boot evidence alone cannot establish physical action: user confirmation is
required separately before accepting common1.2.2.
"""
from pathlib import Path
import importlib.util
import subprocess
import sys
import time
import re
import hashlib

D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
receipt = D / 'build1575-audio-kernel.sha256'
for row in receipt.read_text().splitlines():
    digest, filename = row.split(maxsplit=1)
    k.require(hashlib.sha256(Path(filename).read_bytes()).hexdigest() == digest, 'receipt mismatch')
print('XTS_RECEIPT=' + str(receipt), flush=True)
k.require(subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True).returncode == 1,
          'UART busy')
with k.existing_uart() as p:
    tasks = k.command(p, 'ps')
    k.require(not re.search(r'vela_fs_|\bkvdbd\b|\bfstest\b', tasks), 'workload active')
    print('PHYSICAL_RESET_CAPTURE=READY; press RESET, not BOOT; no host reset', flush=True)
    start = time.monotonic()
    output = bytearray()
    while time.monotonic() - start < 600:
        chunk = p.read(p.in_waiting or 1)
        if chunk:
            output.extend(chunk)
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
        if b'ESP-ROM:' in output and b'NuttShell (NSH)' in output and k.transport.PROMPT in output:
            break
    else:
        print('PHYSICAL_RESET_CAPTURE=NO_COMPLETE_BOOT; no PASS; UART released', flush=True)
        sys.exit(2)
    text = output.decode(errors='replace')
    k.require(not re.search(r'\berror\b|panic|assertion failed|kasan_report', text, re.I),
              'boot error')
    k.command(p, 'ps')
    k.command(p, 'free')
    print('PHYSICAL_RESET_BOOT=OK; user physical-action confirmation still required', flush=True)
