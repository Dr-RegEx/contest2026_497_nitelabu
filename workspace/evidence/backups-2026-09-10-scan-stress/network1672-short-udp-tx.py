"""Preserve original 300s UDP output on target; no reset or acceptance shortcut."""
import hashlib
import importlib.util
import json
import re
import time
from pathlib import Path

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
k.require(not (D / 'network1672-original-file-result.json').exists(), 'run already exists')
r = {'status': 'INCOMPLETE', 'receipt': 'build1631-spawn-parent-env.sha256',
     'command': 'iperf2 -c 192.168.1.29 -i 1 -p 5004 -t 20 -u -b 40M',
     'case': '5.1.25', 'rate_acceptance': 'Pending published numerical criterion',
     'samples': []}

def cmd(p, command):
    out = k.transport.run_command(p, command, timeout=40, reset_input=False)
    k.require(not any(m.decode() in out for m in k.transport.BOOT_FAILURE_MARKERS),
              'target fault')
    k.require(not re.search(r'nsh:.*(?:failed|not found)', out), 'shell error')
    return out

try:
    for line in (D / r['receipt']).read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest,
                  'firmware receipt mismatch')
    with k.existing_uart() as p:
        r['network_before'] = cmd(p, 'ifconfig')
        k.require('RUNNING' in r['network_before'], 'not associated')
        r['uptime_before'] = cmd(p, 'cat /proc/uptime')
        start = time.monotonic()
        r['launch'] = cmd(p, r['command'] + ' > /tmp/y &')
        for _ in range(40):
            time.sleep(2)
            ps = cmd(p, 'ps')
            r['samples'].append({'elapsed': time.monotonic() - start, 'ps': ps})
            if 'iperf2' not in ps:
                break
        else:
            raise RuntimeError('80s task observation exceeded; preserve target file')
        r['uptime_after'] = cmd(p, 'cat /proc/uptime')
        r['output'] = cmd(p, 'cat /tmp/y')
        r['wall_seconds'] = time.monotonic() - start
        k.require(re.search(r'\]\s+0\.00-2[0-9]\.\d+ sec', r['output']),
                  'original duration final summary missing')
        r['network_after'] = cmd(p, 'ifconfig')
        r['iob_after'] = cmd(p, 'cat /proc/iobinfo')
        r['status'] = 'DIAGNOSTIC_COMPLETE'
        # Save the complete output before deleting this run's temporary file.
        (D / 'network1672-original-file-result.json').write_text(
            json.dumps(r, indent=2) + '\n')
        cmd(p, 'rm /tmp/y')
except Exception as exc:
    r['error'] = str(exc)
(D / 'network1672-original-file-result.json').write_text(
    json.dumps(r, indent=2) + '\n')
print(r['status'], flush=True)
raise SystemExit(0 if r['status'] == 'DIAGNOSTIC_COMPLETE' else 1)
