"""Run published 5.1.17 one-hour example on existing frozen1102 sync volume.

The local original parser uses minutes, so -t 60 means one hour. Test01's
previously disclosed worker-join repair is included. No reset/format/retry.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import re
import subprocess
import sys
import time

D = Path(__file__).resolve().parent


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, D / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mount-evidence', type=Path, required=True)
    args = parser.parse_args()
    kv = load('fs1156_kv', 'xts-kvdb-suite.py')
    recovery = load('fs1156_sync', 'run1102-fs-sync-recovery.py')
    receipt = D / 'build1102-flat-category-fs-sync.sha256'
    digest, filename = receipt.read_text().strip().split(maxsplit=1)
    image = Path(filename)
    kv.require(digest == recovery.DIGEST and
               hashlib.sha256(image.read_bytes()).hexdigest() == digest,
               'frozen1102 image mismatch')
    config = (image.parent / '.config').read_text()
    for option in ('BUILD_FLAT', 'ESP32S31_XTS_FLASH_LARGE',
                   'FS_LITTLEFS', 'FS_TEST_STABILITY'):
        kv.require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    recovery.check_sync_mount(kv, args.mount_evidence.read_text(), receipt)
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    kv.require(busy.returncode == 1 and not busy.stderr.strip(), 'UART busy')
    print('XTS_RECEIPT=' + str(receipt), flush=True)
    print('XTS_MOUNT_EVIDENCE_SHA256=' +
          hashlib.sha256(args.mount_evidence.read_bytes()).hexdigest(), flush=True)
    print('XTS_DURATION=published_1h local_minutes=60 worker_join_fix_disclosed', flush=True)
    with kv.existing_uart() as port:
        mounts = kv.command(port, 'mount')
        kv.require('/data type littlefs' in mounts, 'scratch mount missing')
        kv.require(not re.search(r'^\s*/(?:apps|data/\S+) type ', mounts, re.M),
                   'production/nested filesystem present')
        tasks = kv.command(port, 'ps')
        kv.require(not re.search(r'\bkvdbd\b|vela_fs_|\bfstest\b', tasks),
                   'another storage task is active')
        kv.command(port, 'df')
        kv.command(port, 'ls -l /data')
        # Refuse any existing directory; never erase a previous run.
        kv.command(port, 'mkdir /data/s17')
        command = 'vela_fs_stability_test01 -d /data/s17 -t 60'
        print('XTS_COMMAND=' + command, flush=True)
        payload = (command + '\r\n').encode()
        start = time.monotonic()
        kv.require(port.write(payload) == len(payload), 'incomplete command')
        output = bytearray()
        while time.monotonic() - start < 4500:
            chunk = port.read(port.in_waiting or 1)
            if chunk:
                output.extend(chunk)
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
                if kv.transport.PROMPT in output:
                    break
        else:
            raise TimeoutError('original workload has not returned; preserve task, no reset/retry')
        elapsed = time.monotonic() - start
        text = output.decode(errors='replace')
        print('\nXTS_ELAPSED_SECONDS=%.3f' % elapsed, flush=True)
        kv.require(not any(marker.decode() in text for marker in kv.transport.BOOT_FAILURE_MARKERS),
                   'target fault')
        kv.require(not re.search(r'\b(?:error|fail|failed)\b|nsh:|ESP-ROM:', text, re.I),
                   'original workload emitted a failure')
        kv.require('TEST PASSED !' in text and
                   'All threads performed successfully and exited normally' in text,
                   'original completion missing')
        for number in (1, 2, 3):
            kv.require('name=thread_' + str(number) in text,
                       'original worker not observed')
        kv.require(elapsed >= 3600, 'one-hour duration not evidenced')
        kv.require(re.search(r'(?m)^\s*0\s*$', kv.command(port, 'echo $?')),
                   'original exit status nonzero')
        kv.command(port, 'ls -l /data/s17')
        kv.command(port, 'df')
        kv.command(port, 'free')
        kv.require('vela_fs_stability_test01' not in kv.command(port, 'ps'),
                   'test task remains active')
        print('XTS_CASE=5.1.17 PASS duration=1h source_worker_join_fix=disclosed', flush=True)


if __name__ == '__main__':
    main()
