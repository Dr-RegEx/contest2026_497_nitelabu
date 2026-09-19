"""Original5.1.15: fstest -m /data/fstest -n 1000 on frozen1032 LittleFS.

No reset, format, count reduction or retry. May occupy the board for two days;
schedule explicitly around the physical fixture session before invoking.
"""
import argparse
import codecs
import hashlib
import importlib.util
from pathlib import Path
import re
import subprocess
import sys
import time

D = Path(__file__).resolve().parent
DIGEST = '7b0e0dd869e157a1620297412caba0d99b5e6b068fff0e3a00bf05ceb7ced47a'


class Verdict:
    """Inspect complete original output lines without retaining a multi-day log."""
    def __init__(self):
        self.filling = []
        self.deleting = []
        self.summaries = []
        self.errors = []

    def line(self, text):
        for label, values in (('FILLING', self.filling), ('DELETING', self.deleting)):
            match = re.search(r'^=== ' + label + r' (\d+) =+', text.strip())
            if match:
                values.append(int(match[1]))
        match = re.search(r'File system tests done\.\.\. OK: (\d+), FAILED: (\d+)', text)
        if match:
            self.summaries.append(tuple(map(int, match.groups())))
        # The successful summary contains FAILED: 0, so do not reject that word
        # alone. Original ENOSPC handling is internal; actual ERROR is forbidden.
        if re.search(r'\berror\b|nsh:|ESP-ROM:|PANIC|Assertion failed|kasan_report:', text, re.I):
            if len(self.errors) < 10:
                self.errors.append(text)

    def check(self, require):
        require(self.filling == list(range(1, 1001)), 'original filling1..1000 missing')
        require(self.deleting == list(range(1, 1001)), 'original deleting1..1000 missing')
        require(self.summaries == [(2000, 0)], 'original2000OK/0FAILED summary missing')
        require(not self.errors, 'original errors: ' + repr(self.errors))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mount-evidence', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('fstest1157_kv', D / 'xts-kvdb-suite.py')
    kv = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(kv)
    receipt = D / 'build1032-flat-category-fs-large.sha256'
    digest, filename = receipt.read_text().strip().split(maxsplit=1)
    image = Path(filename)
    kv.require(digest == DIGEST and hashlib.sha256(image.read_bytes()).hexdigest() == digest,
               'frozen1032 image mismatch')
    config = (image.parent / '.config').read_text()
    for setting in ('BUILD_FLAT=y', 'ESP32S31_XTS_FLASH_LARGE=y',
                    'FS_LITTLEFS=y', 'TESTING_FSTEST=y',
                    'TESTING_FSTEST_MAXFILE=32768', 'TESTING_FSTEST_MAXOPEN=512',
                    'TESTING_FSTEST_MAXNAME=32', 'TESTING_FSTEST_NLOOPS=100'):
        kv.require('CONFIG_' + setting + '\n' in config, 'unexpected config: ' + setting)
    kv.check_mount_evidence(args.mount_evidence.read_text(), receipt, digest)
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    kv.require(busy.returncode == 1 and not busy.stderr.strip(), 'UART busy')
    print('XTS_RECEIPT=' + str(receipt), flush=True)
    print('XTS_MOUNT_EVIDENCE_SHA256=' +
          hashlib.sha256(args.mount_evidence.read_bytes()).hexdigest(), flush=True)
    print('XTS_FSTEST_RESOURCE_CONFIG=maxfile32768 maxopen512 maxname32 loops1000', flush=True)
    with kv.existing_uart() as port:
        mounts = kv.command(port, 'mount')
        kv.require('/data type littlefs' in mounts, 'scratch mount missing')
        kv.require(not re.search(r'^\s*/(?:apps|data/\S+) type ', mounts, re.M),
                   'production or nested mount present')
        kv.require(not re.search(r'\bkvdbd\b|vela_fs_|\bfstest\b', kv.command(port, 'ps')),
                   'another storage workload active')
        kv.command(port, 'df')
        kv.command(port, 'ls -l /data')
        kv.command(port, 'mkdir /data/fstest')
        command = 'fstest -m /data/fstest -n 1000'
        print('XTS_COMMAND=' + command, flush=True)
        payload = (command + '\r\n').encode()
        start = time.monotonic()
        kv.require(port.write(payload) == len(payload), 'incomplete command')
        decoder = codecs.getincrementaldecoder('utf-8')(errors='replace')
        pending = ''
        tail = b''
        verdict = Verdict()
        while time.monotonic() - start < 49 * 3600:
            chunk = port.read(port.in_waiting or 1)
            if not chunk:
                continue
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
            pending += decoder.decode(chunk)
            lines = pending.split('\n')
            pending = lines.pop()
            for line in lines:
                verdict.line(line)
            tail = (tail + chunk)[-8192:]
            kv.require(not any(marker in tail for marker in kv.transport.BOOT_FAILURE_MARKERS),
                       'target fault; no reset/retry')
            if kv.transport.PROMPT in tail:
                break
        else:
            raise TimeoutError('original fstest has not returned; preserve task, no reset/retry')
        verdict.line(pending + decoder.decode(b'', final=True))
        print('\nXTS_ELAPSED_SECONDS=%.3f' % (time.monotonic() - start), flush=True)
        verdict.check(kv.require)
        kv.require(re.search(r'(?m)^\s*0\s*$', kv.command(port, 'echo $?')),
                   'original exit status nonzero')
        kv.command(port, 'ls -l /data/fstest')
        kv.command(port, 'df')
        # Published final cleanup applies only to the fresh directory created
        # by this invocation, and only after full original success is captured.
        kv.command(port, 'rm -r /data/fstest')
        kv.command(port, 'ls -l /data')
        print('XTS_CASE=5.1.15 PASS original_loops=1000 cleanup=complete', flush=True)


if __name__ == '__main__':
    main()
