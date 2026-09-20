"""Published5.1.68 ten rounds, after archived performance payload release and DB removal/reboot. Reject commit errors."""
from pathlib import Path
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
r = D / 'build948-flat-category-kv.sha256'
digest, filename = r.read_text().strip().split(maxsplit=1)
k.require(hashlib.sha256(Path(filename).read_bytes()).hexdigest() == digest, 'image mismatch')
k.check_mount_evidence((D / 'checkpoint1118-kv-after-capacity-reboot/mount-evidence.log').read_text(), r, digest)
k.require('PUBLISHED_5.1.68_DB_REMOVED' in (D / 'logs/xts1117-kv-capacity-cleanup.log').read_text(), 'published removal missing')
busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
k.require(busy.returncode == 1 and not busy.stderr.strip(), 'UART busy')
print('XTS_RECEIPT=' + str(r), flush=True)
with k.existing_uart() as port:
    k.require('/data type littlefs' in k.command(port, 'mount'), 'existing mount missing')
    k.require(not re.search(r'\bkvdbd\b', k.command(port, 'ps')), 'daemon already running')
    listing = k.transport.run_command(port, 'ls /data/persist.db', timeout=20, reset_input=False)
    k.require('No such file' in listing, 'old DB not removed')
    k.command(port, 'kvdbd &')
    k.require(re.search(r'\bkvdbd\b', k.command(port, 'ps')), 'daemon not running')
    command = b'vela_kvdb_stability_test02 10\r\n'
    print('XTS_COMMAND=' + command.decode().strip(), flush=True)
    k.require(port.write(command) == len(command), 'incomplete command')
    output = bytearray()
    deadline = time.monotonic() + 3600
    while time.monotonic() < deadline:
        chunk = port.read(port.in_waiting or 1)
        if chunk:
            output.extend(chunk)
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
            if k.transport.PROMPT in output:
                break
    else:
        raise TimeoutError('original KV test did not return; no reset/retry')
    text = output.decode(errors='replace')
    k.require(not any(x.decode() in text for x in k.transport.BOOT_FAILURE_MARKERS), 'target fault')
    k.require('TEST PASSED !'<REDACTED_CREDENTIAL>'TEST FAILED !' not in text, 'original verdict failed/missing')
    starts = re.findall(r'Insert a test data, key is \[persist\.kvStaTest_0\]', text)
    k.require(len(starts) == 10, 'ten original rounds not evidenced')
    k.require(re.search(r'(?m)^\s*0\s*$', k.command(port, 'echo $?')), 'original exit status nonzero')
    k.command(port, 'ls -l /data/persist.db')
    k.command(port, 'df')
    k.command(port, 'free')
    k.require('IO error' not<REDACTED_CREDENTIAL>'rollback your database' not in text,
              'database commit errors invalidate the original printed PASS')
    print('XTS_CASE=5.1.68 PASS original_rounds=10 database_preserved', flush=True)
