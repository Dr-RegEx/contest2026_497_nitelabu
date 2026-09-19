"""Compare board uptime with Windows Stopwatch; Linux time is diagnostic only."""
import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
import time
from pathlib import Path

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
a = argparse.ArgumentParser()
a.add_argument('phase', choices=['idle', 'load'])
args = a.parse_args()
run = '1632' if args.phase == 'idle' else '1633'
r = {'status': 'INCOMPLETE', 'phase': args.phase,
     'receipt': 'build1631-spawn-parent-env.sha256'}
result = D / ('clock' + run + '-result.json')
k.require(not result.exists(), 'result already exists')

def cmd(p, text):
    out = k.transport.run_command(p, text, timeout=40, reset_input=False)
    k.require(not any(m.decode() in out for m in k.transport.BOOT_FAILURE_MARKERS),
              'target fault')
    k.require(not re.search(r'nsh:.*(?:failed|not found)|arp_wait failed', out),
              'command or network failed')
    return out

try:
    for line in (D / r['receipt']).read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest,
                  'artifact mismatch')
    with k.existing_uart() as p:
        if args.phase == 'load':
            r['launch'] = cmd(p, 'iperf2 -c 192.168.1.29 -i 1 -p 5002 '
                             '-t 30 > /tmp/tcp1633.log &')
            time.sleep(2)
            r['initial_ps'] = cmd(p, 'ps')
            k.require('iperf2' in r['initial_ps'], 'load absent')
        r['uptime_before'] = cmd(p, 'cat /proc/uptime')
        start = time.monotonic()
        r['windows'] = json.loads(subprocess.check_output([
            '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe',
            '-NoProfile', '-Command',
            '$t=[Diagnostics.Stopwatch]::StartNew(); Start-Sleep -Seconds 20; '
            '@{elapsed=$t.Elapsed.TotalSeconds} | ConvertTo-Json -Compress'],
            text=True))
        r['uptime_after'] = cmd(p, 'cat /proc/uptime')
        r['linux_elapsed'] = time.monotonic() - start
        r['board_elapsed'] = (
            float(re.search(r'^\s*(\d+\.\d+)\s*$', r['uptime_after'], re.M)[1]) -
            float(re.search(r'^\s*(\d+\.\d+)\s*$', r['uptime_before'], re.M)[1]))
        if args.phase == 'load':
            for _ in range(20):
                ps = cmd(p, 'ps')
                if 'iperf2' not in ps:
                    break
                time.sleep(3)
            else:
                raise RuntimeError('task still active; preserve target')
            r['output'] = cmd(p, 'cat /tmp/tcp1633.log')
            k.require('0.00-30' in r['output'], '30s summary missing')
        r['status'] = 'MEASURED'
        result.write_text(json.dumps(r, indent=2) + '\n')
        if args.phase == 'load':
            cmd(p, 'rm /tmp/tcp1633.log')
except Exception as exc:
    r['error'] = str(exc)
result.write_text(json.dumps(r, indent=2) + '\n')
print(r['status'], flush=True)
raise SystemExit(0 if r['status'] == 'MEASURED' else 1)
