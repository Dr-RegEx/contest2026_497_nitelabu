"""Original4.1.29: save, reboot, reconnect from persistent profile; no retries."""
import importlib.util
import json
from pathlib import Path
import re

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
result = {'case': '4.1.29', 'status': 'FAIL',
          'receipt': 'build1322-wifi-reboot.sha256',
          'credentials_reentered': False, 'automatic_retry': False}


def command(port, text):
    out = k.transport.run_command(port, text, timeout=60, reset_input=False)
    k.require(not re.search(r'\bERROR:|\bfailed\b|nsh:|PANIC:|KASAN:|'
                            r'ESP-ROM:|NuttShell \(NSH\)|timed out', out, re.I),
              'target command failed: ' + text)
    if text == 'ifconfig':
        k.require(re.search(r'inet addr:192\.168\.1\.(?!0\b)\d+', out),
                  'DHCP address missing')
    if text.startswith('ping '):
        k.require(re.search(r'\b0% (?:packet )?loss', out), 'gateway ping failed')
    return out


try:
    with k.existing_uart() as port:
        # Establish the initial association on the freshly installed image.
        for text in ['ifup wlan0', 'wapi reconnect wlan0', 'renew wlan0',
                     'ifconfig', 'wapi save_config wlan0']:
            command(port, text)
        print('ORIGINAL_CASE_REBOOT_NOW', flush=True)
        port.write(b'reboot\r\n')
        boot = k.transport.collect_until_prompt(port, 30)
        print(boot.decode(errors='replace'), flush=True)
        k.require(b'NuttShell (NSH)' in boot and
                  not any(x in boot for x in k.transport.BOOT_FAILURE_MARKERS),
                  'reboot failed')
        for text in ['ifup wlan0', 'wapi reconnect wlan0', 'renew wlan0',
                     'ifconfig', 'ping 192.168.1.1']:
            command(port, text)
    result['status'] = 'PASS'
except Exception as error:
    result['error'] = str(error)
finally:
    (D / 'network1325-reboot-reconnect-result.json').write_text(
        json.dumps(result, indent=2) + '\n')
    print(json.dumps(result), flush=True)
raise SystemExit(0 if result['status'] == 'PASS' else 1)
