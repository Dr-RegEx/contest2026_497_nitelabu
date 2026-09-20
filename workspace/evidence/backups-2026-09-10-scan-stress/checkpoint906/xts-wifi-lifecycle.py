"""Published Wi-Fi cases5.1.41/42; no association, flashing or retries."""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_nuttx_smoke as transport


def require(value, message):
    if not value:
        raise RuntimeError(message)


def command(port, text):
    result = transport.run_command(port, text, timeout=30)
    require(not any(x.decode() in result for x in transport.BOOT_FAILURE_MARKERS),
            'fatal marker: ' + text)
    require(not re.search(r'ERROR|nsh:|failed|timed out', result, re.I),
            'command failed: ' + text)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('case', choices=['ifcycle', 'scan', 'country'])
    parser.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    lines = args.receipt.read_text().splitlines()
    require(len(lines) == 2, 'paired kernel and AppFS receipt required')
    files = []
    for line in lines:
        digest, name = line.split(maxsplit=1)
        path = Path(name)
        require(hashlib.sha256(path.read_bytes()).hexdigest() == digest,
                'image digest mismatch')
        files.append(path)
    require({p.name for p in files} == {'nuttx.bin', 'appfs.img'} and
            len({p.parent for p in files}) == 1, 'images must be one ABI pair')
    config = (files[0].parent / '.config').read_text()
    for name in ('BUILD_KERNEL', 'ESPRESSIF_WIFI', 'WIRELESS_WAPI_CMDTOOL'):
        require(f'CONFIG_{name}=y\n' in config, 'missing ' + name)
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(),
            'UART busy or occupancy check failed; preserve current test')
    print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                       write_timeout=1, exclusive=True) as port:
        transport.hard_reset(port)
        boot = transport.collect_until_prompt(port, 30)
        print(boot.decode(errors='replace'), flush=True)
        require(b'NuttShell (NSH)' in boot and not any(
            x in boot for x in transport.BOOT_FAILURE_MARKERS), 'abnormal boot')
        status = command(port, 'ifconfig wlan0')
        require('0.0.0.0' in status, 'expected fresh unconfigured radio')
        if args.case == 'ifcycle':
            for index in range(1, 101):
                require('ifup wlan0...OK' in command(port, 'ifup wlan0'),
                        'interface up failed')
                require('ifdown wlan0...OK' in command(port, 'ifdown wlan0'),
                        'interface down failed')
                print(f'XTS_5.1.41_CYCLE={index} PASS', flush=True)
        elif args.case == 'country':
            require('ifup wlan0...OK' in command(port, 'ifup wlan0'),
                    'interface up failed')
            print(command(port, 'wapi country wlan0'), flush=True)
            for country in ('US', 'CN'):
                print(command(port, 'wapi country wlan0 ' + country), flush=True)
                result = command(port, 'wapi country wlan0')
                print(result, flush=True)
                require(re.search(r'(?m)^\s*' + country + r'\s*$', result),
                        'country readback mismatch: ' + country)
            require('ifdown wlan0...OK' in command(port, 'ifdown wlan0'),
                    'interface shutdown failed')
            print('XTS_4.1.57_COUNTRY=PASS', flush=True)
        else:
            require('ifup wlan0...OK' in command(port, 'ifup wlan0'),
                    'interface up failed')
            previous = None
            changes = 0
            for index in range(1, 101):
                result = command(port, 'wapi scan wlan0')
                require('bssid / frequency / signal level / encode / ssid' in result,
                        'scan result header missing')
                rows = re.findall(r'(?mi)^(?:\[CPU\d+\]\s*)?((?:[0-9a-f]{2}:){5}[0-9a-f]{2}\s[^\r\n]+)',
                                  result)
                require(rows, 'scan returned no APs; header alone is insufficient')
                current = tuple(sorted(rows))
                if previous is not None and previous != current:
                    changes += 1
                previous = current
                print(f'XTS_5.1.42_SCAN={index} PASS AP_COUNT={len(rows)}', flush=True)
            require(changes > 0, 'no observed scan-list variation; retain incomplete result')
            require('ifdown wlan0...OK' in command(port, 'ifdown wlan0'),
                    'interface shutdown failed')
            print(f'XTS_SCAN_LIST_CHANGES={changes}', flush=True)
        command(port, 'free')
        print('XTS_WIFI_COMPLETE=' + args.case, flush=True)


if __name__ == '__main__':
    main()
