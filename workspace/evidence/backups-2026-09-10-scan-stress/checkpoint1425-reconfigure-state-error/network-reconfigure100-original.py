"""Original 5.1.29: disconnect and explicitly reprovision 100 times.

Run after the UART is free. Password is read without echo; logs are redacted.
No automatic retries, resets, or reduced loop counts.
"""
import argparse
import getpass
import hashlib
import importlib.util
import ipaddress
import json
from pathlib import Path
import re
import time

D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ssid', required=True)
    parser.add_argument('--gateway', required=True, type=ipaddress.IPv4Address)
    parser.add_argument('--receipt', required=True, type=Path)
    parser.add_argument('--result', required=True, type=Path)
    args = parser.parse_args()
    k.require(not args.result.exists(), 'result already exists')
    for line in args.receipt.read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest,
                  'artifact receipt mismatch')
    password = getpass.getpass('Test AP password (hidden): ')
    k.require(8 <= len(password) <= 63, 'invalid WPA2 passphrase length')
    escaped = k.transport.nsh_escape(password)
    secrets = (escaped, password)
    provisioning = ['wapi mode wlan0 2', f'wapi psk wlan0 {escaped} 3',
                    'wapi essid wlan0 ' + k.transport.nsh_escape(args.ssid) + ' 1',
                    'renew wlan0']
    k.require(all(len(t) <= k.transport.NSH_COMMAND_MAX for t in provisioning),
              'command exceeds NSH line limit')
    result = {'case': '5.1.29', 'status': 'FAIL', 'completed': 0,
              'receipt': str(args.receipt), 'automatic_retries': False,
              'rounds': []}
    start = time.monotonic()

    def command(port, text, expected_network_failure=False):
        out = k.transport.run_command(port, text, timeout=90,
                                      redactions=secrets, reset_input=False)
        k.require(not re.search(r'Assertion failed|PANIC:|KASAN:|ESP-ROM:', out),
                  'target system fault')
        if not expected_network_failure:
            k.require(not re.search(r'\bERROR:|\bfailed\b|nsh:|timed out', out, re.I),
                      'target command failed')
        return out

    try:
        with k.existing_uart() as port:
            command(port, 'ifup wlan0')
            for text in provisioning:
                command(port, text)
            for index in range(1, 101):
                out = command(port, 'ifconfig')
                wlan = re.search(r'wlan0.*?(?=\n\w+\s|nsh>|\Z)', out, re.S)
                k.require(wlan is not None and 'RUNNING' in wlan.group(),
                          'associated interface missing')
                match = re.search(r'inet addr:([\d.]+)', wlan.group())
                k.require(match is not None and
                          not ipaddress.IPv4Address(match.group(1)).is_unspecified,
                          'DHCP address missing')
                address = match.group(1)
                command(port, 'wapi disconnect wlan0')
                out = command(port, 'ping ' + address, True)
                k.require(not re.search(r'\b[1-9]\d* received', out),
                          'own interface address still answers after disconnect')
                k.require(re.search(r'100% (?:packet )?loss|Network is unreachable|'
                                    r'Network unreachable|Network is down', out, re.I),
                          'disconnected ping failure not demonstrated')
                for text in provisioning:
                    command(port, text)
                out = command(port, 'ping ' + str(args.gateway))
                k.require(re.search(r'\b[1-9]\d* received', out),
                          'gateway unreachable after reprovisioning')
                result['rounds'].append({'round': index, 'address': address,
                                         'status': 'PASS'})
                result['completed'] = index
                print('RECONFIGURE_ROUND=' + str(index) + ' PASS', flush=True)
            result['status'] = 'PASS'
    except Exception as error:
        result['error'] = k.transport.redact_text(str(error), secrets)
    finally:
        result['elapsed_seconds'] = time.monotonic() - start
        args.result.write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps({key: value for key, value in result.items()
                          if key != 'rounds'}), flush=True)
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
