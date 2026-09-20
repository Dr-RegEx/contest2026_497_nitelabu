"""Verify original4.1.30 on1318 after reconnect ordering fix; no retries."""
import importlib.util
import json
from pathlib import Path
import re

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
result = {'case': '4.1.30', 'status': 'FAIL',
          'receipt': 'build1318-wifi-reconnect.sha256',
          'credentials_reentered': False, 'automatic_retry': False}
try:
    with k.existing_uart() as port:
        # Restore the already verified saved profile on the freshly booted
        # candidate before testing reconnect of an established connection.
        commands = ['ifup wlan0', 'wapi reconnect wlan0', 'renew wlan0',
                    'ifconfig', 'wapi save_config wlan0',
                    'wapi reconnect wlan0', 'renew wlan0', 'ifconfig',
                    'ping 192.168.1.1']
        for command in commands:
            output = k.transport.run_command(port, command, timeout=60,
                                              reset_input=False)
            k.require(not re.search(
                r'\bERROR:|\bfailed\b|nsh:|PANIC:|KASAN:|ESP-ROM:|'
                r'NuttShell \(NSH\)|timed out', output, re.I),
                'target command failure')
            if command == 'ifconfig':
                k.require(re.search(r'inet addr:192\.168\.1\.(?!0\b)\d+',
                                    output), 'DHCP address missing')
            if command.startswith('ping '):
                k.require(re.search(r'\b0% (?:packet )?loss', output),
                          'gateway ping failed')
    result['status'] = 'PASS'
except Exception as error:
    result['error'] = str(error)
finally:
    (D / 'network1320-reconnect-result.json').write_text(
        json.dumps(result, indent=2) + '\n')
    print(json.dumps(result), flush=True)
raise SystemExit(0 if result['status'] == 'PASS' else 1)
