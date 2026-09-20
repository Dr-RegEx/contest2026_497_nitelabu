"""Execute only original board-only BLE enable/state/disable/state.

Run after the clock longrun finishes and the receipted BLE image is flashed.
No peer, scan, advertising, repetition or persistence acceptance is added.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys
import time
import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_nuttx_smoke as transport


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    rows = args.receipt.read_text().splitlines()
    require(len(rows) == 1, 'one FLAT image receipt required')
    digest, filename = rows[0].split(maxsplit=1)
    image = Path(filename)
    require(hashlib.sha256(image.read_bytes()).hexdigest() == digest, 'image mismatch')
    config = (image.parent / '.config').read_text()
    for option in ('BUILD_FLAT', 'ESP32S31_BLE', 'BLUETOOTH_TOOLS',
                   'BLUETOOTH_STACK_LE_ZBLUE', 'FS_TMPFS'):
        require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    for option in ('SMP', 'ESPRESSIF_WIFI', 'ESPRESSIF_SPIRAM_USER_HEAP'):
        require(f'CONFIG_{option}=y\n' not in config, 'unsupported ' + option)
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(), 'UART occupied or check failed')
    print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=.1,
                       write_timeout=1, exclusive=True) as port:
        transport.hard_reset(port)
        boot = transport.collect_until_prompt(port, 30)
        print(boot.decode(errors='replace'), flush=True)
        require(b'NuttShell (NSH)' in boot and not any(
            marker in boot for marker in transport.BOOT_FAILURE_MARKERS), 'abnormal boot')

        def shell(cmd, check=True):
            print('XTS_COMMAND=' + cmd, flush=True)
            out = transport.run_command(port, cmd, timeout=20)
            print(out, flush=True)
            require(not any(marker.decode() in out for marker in transport.BOOT_FAILURE_MARKERS),
                    'target fatal error')
            if check:
                require(not re.search(r'nsh:|ERROR|failed', out, re.I), 'command failed: ' + cmd)
            return out

        shell('ls /dev/ttyHCI0')
        mounts = shell('mount')
        require(not re.search(r'(?m)^.* /data(?:/| )', mounts),
                'preexisting /data mount; refusing to hide persistent data')
        listing = shell('ls /data', check=False)
        require('No such file' in listing, 'fresh /data mountpoint required')
        shell('mkdir /data')
        shell('mount -t tmpfs /data')
        shell('mkdir /data/misc')
        shell('mkdir /data/misc/bt')

        def interactive(cmd, expected=None, prompt='bttool> ', timeout=60):
            print('XTS_COMMAND=' + cmd, flush=True)
            # Preserve pending asynchronous output, not flush it away. Only
            # match events received after this command is submitted.
            port.write((cmd + '\n').encode())
            out = ''
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                data = port.read(port.in_waiting or 1)
                if data:
                    text = data.decode(errors='replace')
                    out += text
                    print(text, end='', flush=True)
                if (any(marker.decode() in out for marker in transport.BOOT_FAILURE_MARKERS)
                        or 'ESP-ROM:' in out):
                    # Preserve the rest of the panic before closing pyserial,
                    # which can discard queued input. Send no further command.
                    until = time.monotonic() + 15
                    while time.monotonic() < until:
                        tail = port.read(port.in_waiting or 1)
                        if tail:
                            print(tail.decode(errors='replace'), end='', flush=True)
                    raise RuntimeError('target fault/reset; panic retained, no retry')
                require(not re.search(r'cmd execute error|BLE controller .*failed|'
                                      r'Failed to (?:allocate|init)|nsh:', out, re.I),
                        'Bluetooth command/service failed')
                if prompt in out and (expected is None or re.search(expected, out)):
                    return out
            raise TimeoutError('missing original callback/state for ' + cmd + '; no reset/retry')

        interactive('bttool')
        interactive('enable', r'Adapter state changed:\s*2\b')
        interactive('state', r'Adapter State:\s*2\b')
        interactive('disable', r'Adapter state changed:\s*0\b')
        interactive('state', r'Adapter State:\s*0\b')
        print('XTS_CATEGORY_BLE_SWITCH=PASS original_steps=enable,state,disable,state', flush=True)
        # Tool exit is cleanup, not an extra xTS condition; report separately.
        try:
            interactive('quit', prompt='nsh> ', timeout=20)
        except Exception:
            print('XTS_BLE_CLEANUP=FAILED original_case_result_retained', flush=True)
            raise
        print('XTS_BLE_CLEANUP=COMPLETE', flush=True)


if __name__ == '__main__':
    main()
