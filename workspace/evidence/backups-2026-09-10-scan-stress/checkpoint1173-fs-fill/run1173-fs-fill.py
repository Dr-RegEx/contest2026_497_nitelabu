"""Original 5.1.1 on backed-up 3MiB scratch; no format or automatic retry."""
import importlib.util
import hashlib
from pathlib import Path
import re
import subprocess
import sys
import time

D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
backup = D / 'flash1170-before-fill/volume.bin'
k.require(backup.stat().st_size == 0x300000, 'full backup missing')
evidence = D / 'checkpoint1172-fill-mount/mount-evidence.log'
k.require('XTS_SYNC_MOUNT=PASS' in evidence.read_text(), 'fresh mount missing')
k.require(subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True).returncode == 1,
          'UART busy')
print('XTS_BACKUP_SHA256=' + hashlib.sha256(backup.read_bytes()).hexdigest(), flush=True)
print('XTS_LIMITATION=original does not report terminal write errno; no power-loss claim', flush=True)
with k.existing_uart() as p:
    k.require('/data type littlefs' in k.command(p, 'mount'), 'mount missing')
    k.require(not re.search(r'vela_fs_|\bkvdbd\b|\bfstest\b', k.command(p, 'ps')),
              'other writer active')
    k.command(p, 'df')
    k.command(p, 'mkdir /data/s01_1173')
    cmd = 'vela_fs_stress_write_full_file /data/s01_1173'
    print('XTS_COMMAND=' + cmd, flush=True)
    start = time.monotonic()
    data = (cmd + '\r\n').encode()
    k.require(p.write(data) == len(data), 'command incomplete')
    output = bytearray()
    while time.monotonic() - start < 7200:
        chunk = p.read(p.in_waiting or 1)
        if chunk:
            output.extend(chunk)
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
            if k.transport.PROMPT in output:
                break
    else:
        raise TimeoutError('preserve current task and files; no reset/retry')
    text = output.decode(errors='replace')
    print('\nXTS_ELAPSED_SECONDS=%.3f' % (time.monotonic() - start), flush=True)
    k.require('TEST PASSED'<REDACTED_CREDENTIAL>'create full file, takes' in text,
              'original completion missing')
    k.require(not re.search(r'\b(?:error|fail|failed)\b|PANIC|ESP-ROM:|nsh:', text, re.I),
              'target failure')
    k.require(re.search(r'(?m)^\s*0\s*$', k.command(p, 'echo $?')), 'target exit nonzero')
    k.command(p, 'ls -l /data/s01_1173')
    k.command(p, 'df')
    k.command(p, 'free')
    k.require('vela_fs_stress_write_full_file' not in k.command(p, 'ps'), 'task remains')
    print('XTS_CASE=5.1.1 PASS original_workload terminal_errno_unreported', flush=True)
