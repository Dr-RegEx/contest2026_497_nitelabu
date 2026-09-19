"""Read the fixed board xTS Flash range twice; never erase or write Flash.

Run only after the longrun releases UART. This read operation resets the
board. A fresh output directory preserves both reads and their manifest.
The original Flash block runner accepts only an entirely erased range.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

PORT = ('/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_'
        'f65e4ef67f71f011975a049f1045c30f-if00-port0')
ESPTOOL = '/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool'
OFFSET = 0xc00000
SIZE = 0x100000

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path, help='fresh backup directory')
args = parser.parse_args()
result = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
if result.returncode != 1:
    raise RuntimeError('UART busy or ownership check failed; no reset performed')
args.output.mkdir(parents=False, exist_ok=False)
manifest = {'offset': OFFSET, 'size': SIZE, 'port': PORT, 'files': [],
            'created_utc': datetime.now(timezone.utc).isoformat()}
previous = None
for name in ('before-a.bin', 'before-b.bin'):
    path = args.output / name
    subprocess.run([ESPTOOL, '--chip', 'esp32s31', '--port', PORT,
                    '--baud', '460800', 'read-flash', hex(OFFSET), hex(SIZE),
                    str(path)], check=True)
    data = path.read_bytes()
    if len(data) != SIZE or (previous is not None and data != previous):
        raise RuntimeError('scratch backup size/content mismatch; files retained')
    previous = data
    manifest['files'].append({'name': name,
                             'sha256': hashlib.sha256(data).hexdigest()})
manifest['blank'] = previous == b'\xff' * SIZE
(args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('POWER1583_BACKUP=PASS identical reads; existing data preserved; no formatting')
