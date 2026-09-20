"""Diagnostic association to the same BSSID as Windows; NOT original case PASS."""
import argparse
import contextlib
import getpass
import hashlib
import importlib.util
import ipaddress
import json
from pathlib import Path
import re
import subprocess

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ssid', required=True)
    parser.add_argument('--bssid', required=True)
    parser.add_argument('--gateway', required=True, type=ipaddress.IPv4Address)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    receipt = D / 'build1078-wifi-mmu.sha256'
    for line in receipt.read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest,
                  'firmware receipt mismatch')
    k.require(not args.output.exists(), 'use a fresh output directory')
    password = getpass.getpass('Specified test AP WPA2 password (hidden): ')
    k.require(8 <= len(password) <= 63, 'WPA2 passphrase length must be8..63')
    escaped_ssid = k.transport.nsh_escape(args.ssid)
    escaped_password = k.transport.nsh_escape(password)
    secrets = tuple(sorted({args.ssid, password, escaped_ssid, escaped_password},
                           key=len, reverse=True))
    k.require(re.fullmatch(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', args.bssid), 'invalid BSSID')
    commands = ['ifup wlan0', 'wapi mode wlan0 2',
                f'wapi psk wlan0 {escaped_password} 3',
                f'wapi essid wlan0 {escaped_ssid} 2', f'wapi ap wlan0 {args.bssid}', 'renew wlan0',
                'ifconfig', f'ping {args.gateway}']
    # Validate before sending; the shared helper's length exception echoes
    # the command, so never use that exception for credential validation.
    k.require(all(len(c) <= k.transport.NSH_COMMAND_MAX for c in commands),
              'escaped command exceeds configured UART command limit')
    k.require(subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
              .returncode == 1, 'UART busy')
    args.output.mkdir(parents=True)
    result = {'case': 'BSSID_DIAGNOSTIC_NOT_XTS_PASS', 'bssid': args.bssid, 'status': 'FAIL', 'receipt': str(receipt),
              'ap_precondition': 'User-specified2.4GHz WPA2 AP required',
              'reset': False, 'retry': False}
    with (args.output / 'uart-redacted.log').open('x') as log:
        with contextlib.redirect_stdout(log), contextlib.redirect_stderr(log):
            try:
                with k.existing_uart() as port:
                    for command in commands:
                        output = k.transport.run_command(
                            port, command, timeout=60, redactions=secrets,
                            reset_input=False)
                        k.require(not re.search(
                            r'ERROR|failed|nsh:|PANIC|KASAN:|ESP-ROM:|'
                            r'NuttShell \(NSH\)|timed out', output, re.I),
                            'target command failed')
                        if command == 'ifconfig':
                            wlan = re.search(r'wlan0.*?(?=\n\w+\s|nsh>|\Z)',
                                             output, re.S)
                            k.require(wlan is not None, 'wlan0 status missing')
                            addresses = re.findall(r'inet addr:([\d.]+)',
                                                   wlan.group())
                            k.require(any(not ipaddress.IPv4Address(a).is_unspecified
                                          for a in addresses), 'DHCP address missing')
                            result['board_addresses'] = addresses
                        if command.startswith('ping '):
                            k.require(re.search(r'\b0% (?:packet )?loss', output),
                                      'gateway ping did not report zero loss')
                result['status'] = 'PASS'
            except Exception as error:
                result['error'] = k.transport.redact_text(str(error), secrets)
                print('CASE_FAILED: ' + result['error'])
    (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
