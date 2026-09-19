"""Continue receiving the same FS workload after a host idle timeout; no TX/reset."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import time

D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
kv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kv)
prior = (D / 'logs/xts1045-fs-random-large.log').read_text()
kv.require('TimeoutError' in prior, 'only resume after recorded host timeout')
kv.require('XTS_CASE=5.1.7 EXECUTION_COMPLETE' not in prior, 'already complete')
busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
kv.require(busy.returncode == 1 and not busy.stderr.strip(), 'UART busy')
print('RESUME1045=passive_same_boot no_write no_reset', flush=True)
tail = b''
started = time.monotonic()
with kv.existing_uart() as port:
    while time.monotonic() - started < 7200:
        data = port.read(port.in_waiting or 1)
        if not data:
            continue
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
        tail = (tail + data)[-8192:]
        kv.require(not any(marker in tail for marker in
                           (b'ESP-ROM:', b'PANIC', b'Assertion failed')),
                   'unexpected boot or fault, preserve output')
        if kv.transport.PROMPT in tail:
            print('\nRESUME1045=PROMPT_RECEIVED review combined logs before acceptance', flush=True)
            break
    else:
        raise TimeoutError('No terminal prompt; target state unresolved, do not reset')
