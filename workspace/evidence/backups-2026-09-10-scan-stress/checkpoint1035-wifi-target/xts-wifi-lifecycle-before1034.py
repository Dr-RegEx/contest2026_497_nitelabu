"""Published Wi-Fi cases 5.1.41/42 and 4.1.57; resets board, never associates."""
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


SCAN_ROW = re.compile(
    r'^(?:\[CPU\d+\]\s*)?'
    r'((?:[0-9a-f]{2}:){5}[0-9a-f]{2}\t'
    r'[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\t'
    r'-?[0-9]+\t[0-9a-f]{4}\t[^\r\n]*)$', re.I)


def split_scan_output(output):
    """Separate WAPI's tab-delimited AP data from control diagnostics.

    SSIDs are arbitrary text and can legitimately contain failure markers.
    Only complete rows matching the local WAPI printf format are data.
    """
    rows = []
    diagnostics = []
    for line in output.splitlines():
        match = SCAN_ROW.fullmatch(line)
        if match:
            rows.append(match.group(1))
        else:
            diagnostics.append(line)
    return rows, '\n'.join(diagnostics)


def require(value, message):
    if not value:
        raise RuntimeError(message)


def command(port, text):
    result = transport.run_command(port, text, timeout=30)
    diagnostics = (split_scan_output(result)[1]
                   if text == 'wapi scan wlan0' else result)
    require(not any(x.decode() in diagnostics for x in transport.BOOT_FAILURE_MARKERS),
            'fatal marker: ' + text)
    require(not re.search(r'ERROR|nsh:|failed|timed out', diagnostics, re.I),
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
    for name in ('BUILD_KERNEL', 'ESPRESSIF_WIFI', 'WIRELESS_WAPI_CMDTOOL',
                 'NETINIT_NETLOCAL'):
        require(f'CONFIG_{name}=y\n' in config, 'missing ' + name)
    require('CONFIG_NETINIT_DHCPC=y\n' not in config,
            'unassociated cases require automatic DHCP disabled')
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
        require(re.search(r'inet addr:0\.0\.0\.0(?:\s|$)', status),
                'expected fresh unconfigured radio')
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
        else:
            require('ifup wlan0...OK' in command(port, 'ifup wlan0'),
                    'interface up failed')
            previous = None
            changes = 0
            for index in range(1, 101):
                result = command(port, 'wapi scan wlan0')
                require('bssid / frequency / signal level / encode / ssid' in result,
                        'scan result header missing')
                rows, _ = split_scan_output(result)
                require(rows, 'scan returned no APs; header alone is insufficient')
                current = tuple(sorted(rows))
                if previous is not None and previous != current:
                    changes += 1
                previous = current
                print(f'XTS_5.1.42_SCAN={index} PASS AP_COUNT={len(rows)}', flush=True)
            require(changes > 0, 'no observed scan-list variation; retain incomplete result')
            print(f'XTS_SCAN_LIST_CHANGES={changes}', flush=True)

        case_id = {'ifcycle': '5.1.41', 'scan': '5.1.42',
                   'country': '4.1.57'}[args.case]
        print(f'XTS_CASE={case_id} PASS', flush=True)
        if args.case == 'country':
            print('XTS_4.1.57_COUNTRY=PASS', flush=True)

        # These published cases end before this optional interface cleanup.
        # Preserve the case result while reporting cleanup failures separately.
        if args.case != 'ifcycle':
            try:
                require('ifdown wlan0...OK' in command(port, 'ifdown wlan0'),
                        'interface shutdown failed')
            except Exception as error:
                print(f'XTS_WIFI_CLEANUP=FAIL {error}', flush=True)
                raise
            print('XTS_WIFI_CLEANUP=PASS', flush=True)
        print('XTS_WIFI_COMPLETE=' + args.case, flush=True)


if __name__ == '__main__':
    main()
