import importlib.util
from pathlib import Path
import subprocess
import time
D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
assert busy.returncode == 1 and not busy.stderr.strip(), 'UART occupied'
print('Cleanup of 1514 on receipted 1515 image; no reset', flush=True)
with k.existing_uart() as port:
    for command, expected in [('gatts stop 3', 'bttool> '), ('gatts unregister 3', 'bttool> '), ('disable', 'Adapter state changed: 0'), ('quit', 'nsh> ')]:
        print('XTS_COMMAND=' + command, flush=True)
        port.write((command + '\n').encode())
        output = ''
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            data = port.read(port.in_waiting or 1)
            if data:
                text = data.decode(errors='replace')
                print(text, end='', flush=True)
                output += text
            assert not any(x in output for x in ('PANIC', 'Assertion failed', 'ESP-ROM:', 'cmd execute error')), 'target error'
            if expected in output:
                break
        else:
            raise TimeoutError(command)
print('CLEANUP_COMPLETE', flush=True)
