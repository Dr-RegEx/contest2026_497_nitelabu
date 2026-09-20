"""Run three independent redacted network probes; credentials stay in memory."""
import getpass
from pathlib import Path
import runpy
import sys
from unittest.mock import patch

base = Path(__file__).resolve().parent
label = sys.argv[1]
if not label.replace('-', '').isalnum():
    raise SystemExit('Invalid log label')
extra = sys.argv[2:]
ssid = getpass.getpass('Authorized test SSID (hidden): ')
password = getpass.getpass('WPA2 password (hidden): ')
results = []
for index in range(1, 4):
    path = base / 'logs' / f'{label}-{index}-redacted.log'
    sys.argv = [str(base / 'network-probe.py'), str(path),
                '192.168.1.1', '/dev/ttyUSB0', *extra]
    try:
        with patch('getpass.getpass', side_effect=[ssid, password]):
            runpy.run_path(sys.argv[0], run_name='__main__')
    except SystemExit as result:
        code = result.code
    else:
        code = 0
    results.append(code)
    print(f'BATCH_ROUND={index} EXIT={code}', flush=True)
print(f'BATCH_RESULTS={results}', flush=True)
raise SystemExit(0 if results == [0, 0, 0] else 1)
