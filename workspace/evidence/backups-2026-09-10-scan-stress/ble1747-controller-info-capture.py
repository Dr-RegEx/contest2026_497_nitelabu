#!/usr/bin/env python3
"""Capture the controller's HCI version from a paired BLE image.

The script is deliberately an evidence collector, not an xTS runner.  It
requires an image receipt, prepares the temporary BLE storage before bttool,
waits for the asynchronous HCI-information line (rather than the first stale
prompt), and always preserves the complete UART transcript on failure.
"""
import argparse
import hashlib
import json
import re
import subprocess
import sys
import time
from pathlib import Path

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_nuttx_smoke as transport

PORT = '/dev/ttyUSB0'
DEFAULT_RECEIPT = ROOT / 'backups/2026-09-10-scan-stress/build1747-ble-controller-info.sha256'
OUT_DIR = ROOT / 'backups/2026-09-10-scan-stress/ble1747-controller-info-enable'
LOG_PATH = OUT_DIR / 'uart.log'
RESULT_PATH = OUT_DIR / 'result.json'
HCI_RE = re.compile(
    r'S31 BLE controller HCI=([^\r\n]+?)\(0x([0-9a-fA-F]{2})\) '
    r'rev=0x([0-9a-fA-F]{4}) mfr=0x([0-9a-fA-F]{4}) '
    r'LEfeat=([0-9a-fA-F]{16})')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


class UARTCollectionError(RuntimeError):
    """A collection failure carrying the bytes received before the failure."""

    def __init__(self, message, data):
        super().__init__(message)
        self.data = bytes(data)


def receipt_images(receipt):
    rows = receipt.read_text(encoding='utf-8').splitlines()
    require(len(rows) == 2, 'receipt must contain nuttx.bin and appfs.img')
    images = []
    for row in rows:
        digest, filename = row.split(maxsplit=1)
        image = Path(filename)
        require(image.is_file(), f'missing receipt image: {image}')
        actual = hashlib.sha256(image.read_bytes()).hexdigest()
        require(actual == digest, f'receipt hash mismatch: {image}')
        images.append({'path': str(image), 'sha256': actual})
    return images


def collect(port, markers, timeout, initial=b'', pattern=None):
    data = bytearray(initial)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        data.extend(port.read(port.in_waiting or 1))
        if any(marker in data for marker in transport.BOOT_FAILURE_MARKERS):
            raise UARTCollectionError(
                'target boot failure marker while collecting UART', data)
        if b'ESP-ROM:' in data and b'NuttShell (NSH)' not in data:
            raise UARTCollectionError(
                'target returned to ROM download/reset while collecting UART', data)
        if all(marker in data for marker in markers) and (
                pattern is None or pattern.search(data.decode(errors='replace'))):
            return bytes(data)
    raise UARTCollectionError('UART collection timeout', data)


def run():
    parser = argparse.ArgumentParser()
    parser.add_argument('--receipt', type=Path, default=DEFAULT_RECEIPT)
    parser.add_argument('--port', default=PORT)
    parser.add_argument('--output', type=Path, default=OUT_DIR,
                        help='New evidence directory; existing logs are never overwritten')
    args = parser.parse_args()
    output = args.output.resolve()
    log_path = output / 'uart.log'
    result_path = output / 'result.json'
    receipt = args.receipt.resolve()
    images = receipt_images(receipt)
    require(not log_path.exists(), f'refusing to overwrite {log_path}')
    require(not result_path.exists(), f'refusing to overwrite {result_path}')
    busy = subprocess.run(['fuser', args.port], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(), 'UART occupied')

    transcript = bytearray()
    status = 'HCI_CONTROLLER_LOG_MISSING'
    error = None
    hci_matches = []
    cleanup = 'NOT_ATTEMPTED'
    output.mkdir(parents=True, exist_ok=True)
    try:
        with serial.Serial(args.port, 115200, timeout=.1, write_timeout=1,
                           exclusive=True) as port:
            transport.hard_reset(port)
            boot = transport.collect_until_prompt(port, 30, 'BLE1747_BOOT')
            transcript.extend(boot)
            require(b'NuttShell (NSH)' in boot, 'NSH prompt missing')

            # bt_service_init creates this path in the normal service flow;
            # the standalone controller-info image must provide it first.
            for command in ('mkdir /data', 'mount -t tmpfs /data',
                            'mkdir /data/misc', 'mkdir /data/misc/bt'):
                setup = transport.run_command(port, command, timeout=20)
                transcript.extend(setup.encode())
                require(not re.search(r'(?im)^nsh:|ERROR|failed', setup),
                        f'BLE storage setup failed: {command}')

            # bttool is a foreground interactive shell: it does not return
            # the NSH prompt until quit.  run_command waits for NSH and would
            # time out here before ever sending enable.
            port.write(b'bttool\r\n')
            bttool = collect(port, (b'bttool> ',), 20)
            transcript.extend(bttool)
            port.write(b'enable\r\n')
            enabled = collect(port, (b'bttool> ',), 60, pattern=HCI_RE)
            transcript.extend(enabled)
            text = transcript.decode(errors='replace')
            hci_matches = [match.group(0) for match in HCI_RE.finditer(text)]
            require(hci_matches, 'controller HCI version/features line missing')
            status = 'HCI_CONTROLLER_LOG_CAPTURED'

            # Leave the target in NSH and retain cleanup output.  Cleanup is
            # not part of the version claim, but prevents a background
            # bttool/controller task from contaminating the next test.
            port.write(b'disable\r\n')
            disabled = collect(port, (b'bttool> ',), 30)
            transcript.extend(disabled)
            port.write(b'quit\r\n')
            exited = collect(port, (b'nsh> ',), 20)
            transcript.extend(exited)
            cleanup = 'DISABLE_AND_BTTOOL_EXIT_COMPLETE'
    except Exception as exc:
        if isinstance(exc, UARTCollectionError):
            transcript.extend(exc.data)
        elif isinstance(exc, TimeoutError):
            # The shared NSH transport carries partial output in the timeout
            # message; preserve it for failures during boot/storage setup.
            transcript.extend(str(exc).encode())
        error = repr(exc)

    # A log is written exactly once, even when boot, setup, or async HCI
    # collection fails.  Never synthesize a controller version on failure.
    require(not log_path.exists(), f'refusing to overwrite {log_path}')
    log_path.write_bytes(bytes(transcript))
    result = {
        'status': status,
        'scope': 'CONTROLLER_VERSION_EVIDENCE_NOT_XTS_PASS',
        'receipt': str(receipt),
        'images': images,
        'matches': hci_matches,
        'uart_log': str(log_path),
        'cleanup': cleanup,
    }
    if error is not None:
        result['error'] = error
    require(not result_path.exists(), f'refusing to overwrite {result_path}')
    result_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n',
                           encoding='utf-8')
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return status == 'HCI_CONTROLLER_LOG_CAPTURED'


if __name__ == '__main__':
    raise SystemExit(0 if run() else 1)
