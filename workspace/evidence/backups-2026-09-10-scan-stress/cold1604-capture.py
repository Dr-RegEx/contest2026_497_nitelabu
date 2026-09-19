"""Capture one physical cold boot without toggling DTR/RTS or sending data.

An observed boot is not proof of power removal. Physical confirmation and
ten eligible rounds are required separately for common 2.1.3 acceptance.
"""
import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import select
import subprocess
import termios
import time
import tty

D = Path(__file__).resolve().parent
ROM = b'ESP-ROM:esp32s31'
NSH = b'NuttShell (NSH)'


def assess(data, chunks):
    start = data.find(ROM)
    end = data.find(NSH, max(start, 0))
    if start < 0 or end < 0:
        return {'status': 'INCOMPLETE_BOOT'}
    end += len(NSH)
    first = next(c for c in chunks if c['offset'] <= start < c['end'])
    last = next(c for c in chunks if c['offset'] < end <= c['end'])
    text = data[start:end].decode(errors='replace')
    elapsed = (last['monotonic_ns'] - first['monotonic_ns']) / 1e9
    faults = re.findall(r'\berror\b|Assertion failed|PANIC|KASAN:|M-TRAP|'
                        r'Segmentation fault', text, re.I)
    result = {'status': 'BOOT_CAPTURED_PHYSICAL_CONFIRMATION_PENDING',
              'elapsed_seconds': elapsed, 'first_byte_utc': first['utc'],
              'boot_complete_utc': last['utc'], 'faults': faults,
              'physical_power_cycle_confirmed': False,
              'timing': 'Host receipt of ROM marker through NSH marker; not power-on instant'}
    if faults:
        result['status'] = 'BOOT_ERRORS'
    elif first is last or elapsed <= 0:
        result['status'] = 'TIMING_UNRESOLVED_BUFFERED_IN_ONE_CHUNK'
    elif data[start:end].count(ROM) != 1:
        result['status'] = 'MULTIPLE_BOOTS'
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--round', type=int, choices=range(1, 11), required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--timeout', type=int, default=600)
    args = ap.parse_args()
    receipt = D / 'build1565-simd-int8.sha256'
    for line in receipt.read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        if hashlib.sha256(Path(name).read_bytes()).hexdigest() != digest:
            raise RuntimeError('firmware receipt mismatch')
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    if busy.returncode != 1 or busy.stderr.strip():
        raise RuntimeError('UART busy or unavailable')
    args.output.mkdir(parents=True, exist_ok=False)
    result = {'case': 'common2.1.3', 'round': args.round,
              'receipt': str(receipt), 'status': 'INCOMPLETE_BOOT'}
    data = bytearray()
    chunks = []
    fd = None
    try:
        fd = os.open('/dev/ttyUSB0', os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        # Set line coding only. Never use a serial constructor or modem ioctl.
        tty.setraw(fd, termios.TCSANOW)
        settings = termios.tcgetattr(fd)
        settings[2] = (settings[2] | termios.CLOCAL | termios.CREAD) & ~termios.HUPCL
        settings[4] = settings[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, settings)
        print('COLD_BOOT_CAPTURE_READY round=%d NO_HOST_RESET' % args.round,
              flush=True)
        deadline = time.monotonic() + args.timeout
        with (args.output / 'serial.bin').open('xb') as raw, \
                (args.output / 'timestamps.jsonl').open('x') as events:
            while time.monotonic() < deadline:
                if not select.select([fd], [], [], .1)[0]:
                    continue
                block = os.read(fd, 4096)
                if not block:
                    raise RuntimeError('UART disconnected; no fabricated timing')
                now = time.monotonic_ns()
                event = {'offset': len(data), 'end': len(data) + len(block),
                         'monotonic_ns': now,
                         'utc': datetime.now(timezone.utc).isoformat()}
                chunks.append(event)
                data.extend(block)
                raw.write(block)
                raw.flush()
                events.write(json.dumps(event) + '\n')
                events.flush()
                print(block.decode(errors='replace'), end='', flush=True)
                if NSH in data:
                    break
        result.update(assess(bytes(data), chunks))
    except Exception as exc:
        result.update(status='CAPTURE_ERROR', error=str(exc))
    finally:
        if fd is not None:
            os.close(fd)
        (args.output / 'serial.log').write_text(data.decode(errors='replace'))
        (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result), flush=True)
    return 0 if result['status'] == 'BOOT_CAPTURED_PHYSICAL_CONFIRMATION_PENDING' else 1


if __name__ == '__main__':
    raise SystemExit(main())
