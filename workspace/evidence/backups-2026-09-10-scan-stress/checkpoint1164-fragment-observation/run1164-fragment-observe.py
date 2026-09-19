"""Original 5.1.16 observation: resource-adjusted -n/-s, no -c/-N override.

Observe the first complete outer iteration, then explicitly stop the background
task. This is NOT a claim that all 1000 default outer iterations completed.
"""
import hashlib
import importlib.util
from pathlib import Path
import re
import subprocess
import sys
import time

D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
receipt = D / 'build1102-flat-category-fs-sync.sha256'
digest, image = receipt.read_text().strip().split(maxsplit=1)
k.require(hashlib.sha256(Path(image).read_bytes()).hexdigest() == digest,
          'frozen image mismatch')
k.require('XTS_CASE=5.1.17 PASS' in
          (D / 'logs/xts1163-fs-one-hour.log').read_text(), 'prior run incomplete')
k.require(subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True).returncode == 1,
          'UART busy')
print('XTS_RECEIPT=' + str(receipt), flush=True)
print('XTS_SCOPE=5.1.16 observation; n=100 s=1; stop after first full outer iteration', flush=True)
with k.existing_uart() as p:
    k.require('/data type littlefs' in k.command(p, 'mount'), 'mount missing')
    tasks = k.command(p, 'ps')
    k.require(not re.search(r'\bkvdbd\b|vela_fs_|\bfstest\b', tasks), 'writer active')
    k.command(p, 'df')
    k.command(p, 'mkdir /data/frag1164')
    start = time.monotonic()
    launch = k.command(p, 'vela_fs_file_fragmentation_test -d /data/frag1164 -n 100 -s 1 &')
    tasks = k.command(p, 'ps')
    pids = [line.split()[0] for line in tasks.splitlines()
            if line.split() and line.split()[0].isdigit()
            and 'vela_fs_file_fragmentation_test' in line]
    k.require(len(pids) == 1, 'no unique fragmentation task; preserve state')
    pid = pids[0]
    print('XTS_TARGET_PID=' + pid, flush=True)
    output = launch + tasks
    while '[TEST NO.0] TEST ... OK !' not in output:
        chunk = p.read(p.in_waiting or 1)
        if chunk:
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
            output += chunk.decode(errors='replace')
        k.require(not re.search(r'\b(?:error|fail|failed)\b|PANIC|ESP-ROM:|Assertion', output, re.I),
                  'fragmentation error; preserve task/log for inspection')
        k.require(time.monotonic() - start < 7200,
                  'observation deadline reached; preserve task, no automatic reset')
    elapsed = time.monotonic() - start
    k.require('Create a total of 100 files' in output and 'Delete total 50' in output,
              'small file phase incomplete')
    numbers = set(map(int, re.findall(r'TEST NO\.(\d+)  The test large file was deleted successfully', output)))
    k.require(numbers == set(range(100)), 'large file phase incomplete')
    print('XTS_OBSERVATION_SECONDS=%.3f' % elapsed, flush=True)
    print('XTS_STOP=explicit kill after first complete outer loop; not full default workload', flush=True)
    k.command(p, 'kill ' + pid)
    k.require('vela_fs_file_fragmentation_test' not in k.command(p, 'ps'), 'task still active')
    k.command(p, 'df')
    k.command(p, 'ls -l /data/frag1164')
    print('XTS_CASE=5.1.16 OBSERVATION_COMPLETE full_default_1000=false', flush=True)
