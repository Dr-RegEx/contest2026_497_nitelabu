"""Diagnostic20-second RX comparison on the receipted1631 kernel/AppFS pair."""
import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
import time
from pathlib import Path

D = Path(__file__).resolve().parent
H = Path('/mnt/c/Users/tttgu/Documents/Codex/s31-host-tools-2026-09-16')
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
parser = argparse.ArgumentParser()
parser.add_argument('protocol', choices=['udp', 'tcp'])
args = parser.parse_args()
udp = args.protocol == 'udp'
k.require(udp, 'UDP-only binding diagnostic')
run = '1661'
port = '5003' if udp else '5001'
target_file = '/tmp/r' + run
result = D / ('network' + run + '-original-rx-result.json')
host_log = D / 'logs' / ('host' + run + '-original-rx.log')
k.require(not result.exists() and not host_log.exists(), 'run already exists')
r = {'status': 'INCOMPLETE', 'case': '5.1.24' if udp else '5.1.22',
     'receipt': 'build1631-spawn-parent-env.sha256', 'duration_seconds': 20, 'scope': 'DIAGNOSTIC_ONLY',
     'protocol': args.protocol, 'rate_acceptance': 'Published numeric criterion absent',
     'target_file': target_file}

def cmd(p, text):
    out = k.transport.run_command(p, text, timeout=40, reset_input=False)
    k.require(not any(m.decode() in out for m in k.transport.BOOT_FAILURE_MARKERS),
              'target fault')
    k.require(not re.search(r'nsh:.*(?:failed|not found)', out), 'shell failure')
    return out

try:
    for line in (D / r['receipt']).read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest,
                  'firmware artifact mismatch')
    exe = H / 'iperf2.exe'
    r['host_binary_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest()
    k.require(r['host_binary_sha256'] ==
              'edcb6d8da79d597e4fecdf8b534753731ceecc2a94bc250cbc2f247249003a83',
              'host binary mismatch')
    with k.existing_uart() as p:
        r['ps_before'] = cmd(p, 'ps')
        k.require(not re.search(r'^\s*\d+.*\biperf2\b', r['ps_before'], re.M),
                  'prior iperf still active')
        r['network_before'] = cmd(p, 'ifconfig')
        k.require('RUNNING' in r['network_before'] and
                  '192.168.1.60' in r['network_before'], 'network not ready')
        r['server_command'] = 'iperf2 -s -p ' + port + ' -i 1' + (' -u' if udp else '')
        r['launch'] = cmd(p, r['server_command'] + ' > ' + target_file + ' &')
        match = re.search(r'iperf2 \[(\d+):', r['launch'])
        k.require(match is not None, 'server PID missing')
        r['server_pid'] = int(match[1])
        r['client_command'] = [str(exe), '-c', '192.168.1.60', '-B', '192.168.1.29', '-i', '1',
                               '-p', port, '-t', '20']
        if udp:
            r['client_command'] += ['-u', '-b', '40M']
        with host_log.open('x') as log:
            client = subprocess.Popen(r['client_command'], stdout=log,
                                      stderr=subprocess.STDOUT)
            r['host_client_pid'] = client.pid
            start = time.monotonic()
            capture = ''
            while time.monotonic() - start < 600:
                if p.in_waiting:
                    chunk = p.read(p.in_waiting).decode(errors='replace')
                    capture += chunk
                    print(chunk, end='', flush=True)
                if client.poll() is not None:
                    break
                time.sleep(.05)
            else:
                raise RuntimeError('600s observation ended; preserve live client/server')
            r['host_exit'] = client.returncode
            r['linux_observation_seconds'] = time.monotonic() - start
            r['unsolicited_uart'] = capture
        k.require(not any(m.decode() in capture for m in k.transport.BOOT_FAILURE_MARKERS),
                  'fault during receive workload')
        time.sleep(3)
        r['output'] = cmd(p, 'cat ' + target_file)
        tasks = cmd(p, 'ps')
        row = next((line for line in tasks.splitlines()
                    if re.match(r'\s*' + str(r['server_pid']) + r'\s', line)), '')
        if 'iperf2' in row:
            cmd(p, 'kill -15 ' + str(r['server_pid']))
        for _ in range(20):
            tasks = cmd(p, 'ps')
            if not re.search(r'^\s*\d+.*\biperf2\b', tasks, re.M):
                break
            time.sleep(1)
        else:
            raise RuntimeError('server tasks remain after normal shutdown request')
        r['output_after_shutdown'] = cmd(p, 'cat ' + target_file)
        r['network_after'] = cmd(p, 'ifconfig')
        r['iob_after'] = cmd(p, 'cat /proc/iobinfo')
        host = host_log.read_text(errors='replace')
        r['host_full_duration'] = bool(re.search(r'\]\s+0\.00-2[0-9]\.\d+ sec', host))
        r['board_summary'] = bool(re.search(r'\]\s+0\.00-\d{2,3}\.\d+ sec', r['output_after_shutdown']))
        k.require(r['host_exit'] == 0 and r['host_full_duration'] and r['board_summary'],
                  'original completion evidence missing; keep target file')
        k.require('did not receive ack of last datagram' not in host,
                  'final UDP confirmation missing')
        r['status'] = 'DIAGNOSTIC_EXECUTION_COMPLETE'
        result.write_text(json.dumps(r, indent=2) + '\n')
        cmd(p, 'rm ' + target_file)
except Exception as exc:
    r['error'] = str(exc)
result.write_text(json.dumps(r, indent=2) + '\n')
print(r['status'], flush=True)
raise SystemExit(0 if r['status'] == 'DIAGNOSTIC_EXECUTION_COMPLETE' else 1)
