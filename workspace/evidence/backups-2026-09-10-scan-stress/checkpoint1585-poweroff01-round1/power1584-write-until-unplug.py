"""Original 4.1.4 write phase. Observe USB loss; never simulate power loss."""
import errno
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import time

D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
receipt = D / 'build1023-flat-category-fs-name.sha256'
digest, name = receipt.read_text().split()
k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest, 'image mismatch')
k.require(subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True).returncode == 1,
          'UART busy')
result = {'case': '4.1.4', 'round': 1, 'status': 'NOT_ARMED',
          'receipt': str(receipt), 'physical_confirmation': False}
data = bytearray()
try:
    with k.existing_uart() as p:
        k.require('/data type littlefs' in k.command(p, 'mount'), 'scratch not mounted')
        listing = k.command(p, 'ls -l /data/p1583')
        k.require('powerOffTestFile' not in listing, 'existing test file; refuse overwrite')
        print('XTS_COMMAND=power_off_test01 /data/p1583', flush=True)
        p.write(b'power_off_test01 /data/p1583\n')
        deadline = time.monotonic() + 600
        while time.monotonic() < deadline:
            try:
                block = p.read(p.in_waiting or 1)
            except OSError as exc:
                if result['status'] == 'WRITING_AWAITING_PHYSICAL_POWER_OFF' and exc.errno in (errno.EIO, errno.ENODEV):
                    result['status'] = 'USB_LOSS_OBSERVED_RECOVERY_PENDING'
                    break
                raise
            if not block:
                if not Path('/dev/ttyUSB0').exists() and result['status'] == 'WRITING_AWAITING_PHYSICAL_POWER_OFF':
                    result['status'] = 'USB_LOSS_OBSERVED_RECOVERY_PENDING'
                    break
                continue
            data.extend(block)
            sys.stdout.buffer.write(block)
            sys.stdout.flush()
            k.require(not any(m in data for m in k.transport.BOOT_FAILURE_MARKERS), 'target fault')
            k.require(b'TEST FAILED' not in data and b'Fail !' not in data, 'write failure')
            if result['status'] == 'NOT_ARMED' and b'Successfully write 100 bytes' in data:
                result['status'] = 'WRITING_AWAITING_PHYSICAL_POWER_OFF'
                print('PHYSICAL_POWER_OFF_READY case=4.1.4 round=1', flush=True)
        else:
            result['status'] = 'OPERATION_WINDOW_EXPIRED_WRITE_MAY_STILL_BE_RUNNING'
except Exception as exc:
    result.update(status='ERROR', error=str(exc))
(D / 'power1584-write-result.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result), flush=True)
raise SystemExit(0 if result['status'] == 'USB_LOSS_OBSERVED_RECOVERY_PENDING' else 1)
