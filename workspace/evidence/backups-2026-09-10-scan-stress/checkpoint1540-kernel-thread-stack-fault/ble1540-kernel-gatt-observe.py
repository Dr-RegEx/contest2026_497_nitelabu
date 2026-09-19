"""Verify BLE GATT reads, exact write payload and service cleanup with the PC.

Development diagnostic on a receipted SMP/MMU kernel image; not xTS acceptance.
"""
import argparse
import json
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
    require(len(rows) == 2, 'paired kernel/AppFS receipt required')
    paths = []
    for row in rows:
        digest, filename = row.split(maxsplit=1)
        image = Path(filename)
        require(hashlib.sha256(image.read_bytes()).hexdigest() == digest, 'image mismatch')
        paths.append(image)
    require({p.name for p in paths} == {'nuttx.bin', 'appfs.img'} and
            len({p.parent for p in paths}) == 1, 'mixed pair')
    config = (paths[0].parent / '.config').read_text()
    for option in ('BUILD_KERNEL', 'SMP', 'ESP32S31_SMP', 'ARCH_ADDRENV',
                   'ESP32S31_BLE', 'BLUETOOTH_TOOLS', 'FS_TMPFS', 'LIBC_USRWORK'):
        require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    for option in ('ESPRESSIF_WIFI', 'ESP32S31_PIE'):
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
        interactive('gatts register 3', r'register service successful')
        interactive('gatts start 3')
        out = interactive('adv start -i 160 -n vela-adv-test -m legacy',
                          r'on_advertising_start_cb, handle:.*adv_id:\d+, status:0')
        match = re.search(r'on_advertising_start_cb, handle:(0x[0-9a-fA-F]+), adv_id:(\d+), status:0', out)
        require(match is not None, 'advertising callback missing')
        hostlog = ROOT / 'backups/2026-09-10-scan-stress/logs/host1540-ble-observation.log'
        require(not hostlog.exists(), 'host log already exists')
        with hostlog.open('x') as log:
            peer = subprocess.Popen(['/mnt/c/Users/tttgu/Documents/Codex/s31-host-tools-2026-09-16/host1510-gatt-client.exe', '30EDA0F3F7B1'], stdout=log, stderr=subprocess.STDOUT)
            board_events = ''
            deadline = time.monotonic() + 100
            while peer.poll() is None and time.monotonic() < deadline:
                data = port.read(port.in_waiting or 1)
                if data:
                    text = data.decode(errors='replace')
                    board_events += text
                    print(text, end='', flush=True)
                    if any(marker.decode() in text for marker in transport.BOOT_FAILURE_MARKERS):
                        peer.terminate()
                        raise RuntimeError('target fault during observation; preserve UART evidence')
            if peer.poll() is None:
                peer.terminate()
                peer.wait(timeout=5)
            peer_rc = peer.returncode
        # Connectable legacy advertising stops upon connection.
        if 'on_advertising_stopped_cb, handle:' + match.group(1) not in board_events:
            interactive('adv stop -h ' + match.group(1), r'on_advertising_stopped_cb')
        interactive('gatts stop 3')
        interactive('gatts unregister 3')
        interactive('disable', r'Adapter state changed:\s*0\b')
        interactive('quit', prompt='nsh> ', timeout=20)
        observed = hostlog.read_text()
        require(peer_rc == 0 and 'GATT_CLIENT_COMPLETE' in observed, 'host GATT client failed')
        require('READ_FF05=Hello VELA!' in observed and 'READ_FF02=Hello VELA!' in observed, 'read value mismatch')
        require('gatts service RX char received write request' in board_events, 'board write callback absent')
        require('53 33 31 2d 47 41 54 54 2d 31 35 31 30' in board_events, 'board payload mismatch')
        return {'status':'GATT_READS_AND_WRITE_CALLBACK_VERIFIED', 'scope':'DEVELOPMENT_DIAGNOSTIC_NOT_XTS_PASS',
                'receipt':str(args.receipt), 'host_log':str(hostlog), 'board_payload_review':'EXACT_13_BYTES_VERIFIED',
                'cleanup':'SERVICE_UNREGISTERED_BLE_DISABLED_TOOL_EXITED'}

if __name__ == '__main__':
    result_path = ROOT / 'backups/2026-09-10-scan-stress/ble1540-kernel-gatt-result.json'
    require(not result_path.exists(), 'result already exists')
    try:
        result = main()
    except Exception as error:
        result = {'status':'FAIL','error':repr(error),'scope':'DEVELOPMENT_DIAGNOSTIC_NOT_XTS_PASS'}
    result_path.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result),flush=True)
    raise SystemExit(0 if result['status']=='GATT_READS_AND_WRITE_CALLBACK_VERIFIED' else 1)
