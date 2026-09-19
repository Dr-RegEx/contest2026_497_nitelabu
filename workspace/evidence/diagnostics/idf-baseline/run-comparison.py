"""Temporary IDF comparison with unconditional openvela restore in finally.

Requires the separately completed double backup and audited flash manifest.
Normal failures and SIGINT/SIGTERM trigger restoration; power loss/SIGKILL
cannot. In that case run flash-transaction.py restore before further tests.
"""
import getpass
import ipaddress
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
PROJECT = ROOT / 'diagnostics/idf-baseline'
LOGS = ROOT / 'backups/2026-09-10-scan-stress/logs'
RUN = os.environ.get('S31_IDF_RUN', '170')
if RUN not in ('170', '171', '172', '173'):
    raise RuntimeError('Unknown audited IDF build')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
from esp32s31_nuttx_smoke import hard_reset


def transact(operation):
    with (LOGS / f'idf{RUN}-{operation}.log').open('x') as log:
        subprocess.run([sys.executable, str(PROJECT / 'flash-transaction.py'), operation],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    print(f'IDF_TRANSACTION={operation} PASS', flush=True)


def compare(mode, number, ssid, password):
    transcript = bytearray()
    client = None
    client_output = ''
    sent = False
    done = False
    address = None
    try:
        with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                           write_timeout=2, exclusive=True) as port:
            hard_reset(port)
            deadline = time.monotonic() + 145
            while time.monotonic() < deadline:
                transcript.extend(port.read(max(1, min(8192, port.in_waiting))))
                text = transcript.decode('utf-8', errors='replace')
                if not sent and 'BASELINE_READY version=1' in text:
                    frame = b'S31W' + bytes((mode, len(ssid), len(password))) + ssid + password
                    port.write(frame)
                    port.flush()
                    sent = True
                    print(f'IDF_ROUND mode={mode} round={number} credentials_sent=RAM_ONLY', flush=True)
                if address is None:
                    match = re.search(r'BASELINE_IP=(\d+\.\d+\.\d+\.\d+) GW=', text)
                    if match:
                        address = str(ipaddress.IPv4Address(match.group(1)))
                if client is None and address and 'BASELINE_TCP_READY port=5471' in text:
                    windows_python = ('C:\\Users\\tttgu\\.cache\\codex-runtimes\\'
                                      'codex-primary-runtime\\dependencies\\python\\python.exe')
                    windows_script = ('\\\\wsl.localhost\\Ubuntu-22.04\\home\\regex\\work\\'
                                      'esp32s31-openvela\\backups\\2026-09-10-scan-stress\\'
                                      'tcp-nettest-client.py')
                    command = f"& '{windows_python}' '{windows_script}' '{address}'"
                    client = subprocess.Popen(['powershell.exe', '-NoProfile', '-NonInteractive',
                                               '-Command', command], stdout=subprocess.PIPE,
                                              stderr=subprocess.STDOUT, encoding='utf-8',
                                              errors='replace')
                if 'BASELINE_DONE wifi=stopped' in text:
                    done = True
                    break
                if any(marker in text for marker in ('Guru Meditation Error', 'abort() was called',
                                                      'BASELINE_INPUT=FAIL')):
                    break
                if len(transcript) > 2 * 1024 * 1024:
                    raise RuntimeError('Serial transcript exceeded diagnostic limit')
    finally:
        if client is not None:
            try:
                client_output, _ = client.communicate(timeout=10)
            except subprocess.TimeoutExpired:
                client.kill()
                client_output, _ = client.communicate()
                client_output += '\nHOST_CLIENT_TIMEOUT\n'
        text = transcript.decode('utf-8', errors='replace') + '\n' + client_output
        for secret in sorted({ssid.decode(), password.decode()}, key=len, reverse=True):
            text = text.replace(secret, '<redacted>')
        with (LOGS / f'idf{RUN}-mode{mode}-round{number}-redacted.log').open('x') as log:
            log.write(text)
    phy = re.search(r'BASELINE_PHY=(\d+) ', text)
    passed = (done and phy is not None and int(phy.group(1)) == (6 if mode else 4) and
              'BASELINE_PING sent=8 received=8 result=PASS' in text and
              'BASELINE_TCP=PASS bytes=4096' in text and
              'TCP_NETTEST=PASS bytes=4096'<REDACTED_CREDENTIAL>client is not None and
              client.returncode == 0)
    print(f'IDF_ROUND mode={mode} round={number} phy={phy.group(1) if phy else None} '
          f'done={done} PASS={passed}', flush=True)
    return passed


def interrupted(signum, frame):
    raise KeyboardInterrupt(f'signal {signum}: restoring openvela')


def main():
    os.umask(0o077)
    ssid = getpass.getpass('Authorized SSID (hidden): ').encode()
    password = getpass.getpass('Authorized WPA2 PSK (hidden): ').encode()
    if not (1 <= len(ssid) <= 32 and 8 <= len(password) <= 63):
        raise SystemExit('Invalid credential length; no flash writes performed')
    for signum in (signal.SIGTERM, signal.SIGINT):
        signal.signal(signum, interrupted)
    outcomes = []
    try:
        transact('install')
        for mode in (1, 0):
            for number in range(1, 4):
                outcomes.append(compare(mode, number, ssid, password))
    finally:
        # Do not interrupt the restoration halfway through a flash write.
        for signum in (signal.SIGTERM, signal.SIGINT):
            signal.signal(signum, signal.SIG_IGN)
        print('IDF_COMPARISON restoring original openvela sectors now', flush=True)
        transact('restore')
    print(f'IDF_RESULTS={outcomes}; OPENVELA_RESTORED_AND_COMPARED=PASS', flush=True)
    return 0 if len(outcomes) == 6 and all(outcomes) else 1


if __name__ == '__main__':
    sys.exit(main())
