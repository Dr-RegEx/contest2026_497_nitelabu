"""Observe a real BOOT press/release on the current1078 demo; no reset/network."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import re
import subprocess
import sys
import time

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--resume-pid', type=int)
args = parser.parse_args()
r = D / 'build1078-wifi-mmu.sha256'
for line in r.read_text().splitlines():
    digest, name = line.split(maxsplit=1)
    k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest, 'image mismatch')
k.require('S31_OFFLINE_COMMANDS=COMPLETE' in (D / 'logs/xts1147-demo-offline.log').read_text(),
          'current offline demonstration missing')
b = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
k.require(b.returncode == 1 and not b.stderr.strip(), 'UART busy')
with k.existing_uart() as p:
    if args.resume_pid is None:
        k.require(not re.search(r'\bbuttons\b|\bbutton_daemon\b', k.command(p, 'ps')),
                  'button task already running')
        k.command(p, 'prlimit -s 8192 buttons &')
    tasks = k.command(p, 'ps')
    pids = [line.split()[0] for line in tasks.splitlines()
            if re.match(r'^\s*\d+\s', line) and line.split()[-1] == 'buttons']
    k.require(len(pids) == 1, 'cannot uniquely identify button task; no guessed kill')
    pid = pids[0]
    if args.resume_pid is not None:
        k.require(pid == str(args.resume_pid), 'requested button process not running')
    print('DEMO_BUTTON_MONITOR=READY pid=' + pid, flush=True)
    seen = ''
    deadline = time.monotonic() + 90
    passed = False
    try:
        while time.monotonic() < deadline:
            chunk = p.read(p.in_waiting or 1)
            if not chunk:
                continue
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
            seen += chunk.decode(errors='replace')
            k.require(not any(m.decode() in seen for m in k.transport.BOOT_FAILURE_MARKERS),
                      'target fault')
            samples = re.findall(r'Sample = (\d+)', seen)
            if any(samples[i:i+2] == ['1', '0'] for i in range(len(samples)-1)):
                passed = True
                break
    finally:
        k.command(p, 'kill ' + pid)
        k.command(p, 'ps')
    print('DEMO_BUTTON_REVIEW=' + ('PASS press1_release0' if passed else 'PENDING no_complete_press_release'), flush=True)
