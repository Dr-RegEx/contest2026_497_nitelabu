"""Prepare candidate948 scratch LittleFS once; does not run KVDB tests or flash."""
import argparse
from contextlib import redirect_stdout, redirect_stderr
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys

D = Path(__file__).resolve().parent
ROOT = D.parent.parent
EXPECTED_IMAGE = 'c8e3dcc651b8c324cb9d44541e59b1d948a6536d79a5e33654aa10d5b208a4b1'
TOKEN = D / '.run1012-kv-first-mount.used'


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def preflight(receipt):
    lines = receipt.read_text().splitlines()
    require(len(lines) == 1, 'exactly one FLAT image receipt required')
    digest, name = lines[0].split(maxsplit=1)
    image = Path(name)
    require(image.name == 'nuttx.bin' and digest == EXPECTED_IMAGE and
            sha(image) == digest, 'candidate948 image/receipt mismatch')
    config_path = image.parent / '.config'
    config = config_path.read_text()
    for option in ('BUILD_FLAT', 'ESP32S31_XTS_FLASH', 'FS_LITTLEFS', 'FS_TMPFS',
                   'ESPRESSIF_SPIRAM_USER_HEAP', 'ESP32S31_SPIFLASH_PSRAM_STACK',
                   'SCHED_LPWORK', 'NET_LOCAL', 'KVDB', 'KVDB_SERVER',
                   'KVDB_TEMPORARY_STORAGE', 'KVDB_UNQLITE', 'TESTS_TESTSUITES',
                   'CM_KVDB_TEST', 'TESTING_CMOCKA', 'KVDB_TEST_STABILITY'):
        require('CONFIG_' + option + '=y\n' in config, 'missing ' + option)
    for option, value in (('KVDB_PERSIST_PATH', '/data/persist.db'),
                          ('KVDB_TEMPORARY_PATH', '/tmp/db'),
                          ('KVDB_LOAD_TEST_SOURCE_PATH', '/data/test.prop'),
                          ('LIBC_TMPDIR', '/tmp')):
        require(f'CONFIG_{option}="{value}"\n' in config, 'wrong ' + option)
    backup = D / 'checkpoint992-flash-target/initial-blank-backup/manifest.json'
    manifest = json.loads(backup.read_text())
    require(manifest['offset'] == 0xc00000 and manifest['size'] == 0x100000 and
            manifest['blank'] is True and len(manifest['files']) == 2,
            '990 original backup metadata mismatch')
    require({f['name'] for f in manifest['files']} == {'before-a.bin', 'before-b.bin'},
            'two distinct original backup files required')
    blobs = []
    for entry in manifest['files']:
        path = backup.parent / entry['name']
        blob = path.read_bytes()
        require(len(blob) == 0x100000 and sha(path) == entry['sha256'] and
                blob == b'\xff' * 0x100000, '990 backup hash/blank mismatch')
        blobs.append(blob)
    require(blobs[0] == blobs[1], '990 double backups differ')
    block = D / 'checkpoint992-flash-target/logs/xts992-original-flash-block.log'
    raw = D / 'checkpoint994-flash-target/logs/xts994-original-flash-raw.log'
    text = block.read_text()
    expected = ['drivertest_block_stress', 'drivertest_block_single_write',
                'drivertest_block_cache_write']
    require(re.findall(r'\[\s*RUN\s*\]\s+(\S+)', text) == expected and
            re.findall(r'\[\s*OK\s*\]\s+(\S+)', text) == expected and
            re.findall(r'\[\s*PASSED\s*\]\s+(\d+) test\(s\)', text) == ['3'],
            '992 original 3/3 evidence missing')
    rawtext = raw.read_text()
    for direction in ('WRITE', 'READ'):
        require(f'XTS_5.1.13_{direction}=PASS bytes=262144 ' in rawtext,
                '994 original transfer evidence missing')
    require('XTS_5.1.13=PASS original_dd_bs4096_count64 no_speed_threshold' in rawtext,
            '994 final PASS missing')
    require(not re.search(r'\[\s*(?:FAILED|SKIPPED)\s*\]|PANIC|Assertion failed',
                          text + rawtext, re.I), 'failed prerequisite evidence')
    return digest, {str(p): sha(p) for p in (receipt, config_path, backup, block, raw)}


class Tee:
    def __init__(self, console, log):
        self.console, self.log = console, log

    def write(self, text):
        self.console.write(text)
        self.log.write(text)
        self.flush()
        return len(text)

    def flush(self):
        self.console.flush()
        self.log.flush()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, default=D / 'build948-flat-category-kv.sha256')
    parser.add_argument('--output', type=Path, required=True, help='new evidence directory')
    args = parser.parse_args()
    digest, evidence = preflight(args.receipt)
    require(not TOKEN.exists(), '1012 exclusive token exists; no automatic rerun or reformat')
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(), 'UART occupied or fuser failed')
    # Imports occur only after offline validation; --help never imports UART code.
    spec = importlib.util.spec_from_file_location('kv_suite', D / 'xts-kvdb-suite.py')
    kv = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(kv)
    import serial
    args.output.mkdir(parents=True, exist_ok=False)
    # Claim before opening UART. Leave token on every failure: never retry format.
    with TOKEN.open('x') as token:
        token.write(json.dumps({'receipt': str(args.receipt.resolve()),
                                'image_sha256': digest,
                                'output': str(args.output.resolve())}) + '\n')
        token.flush()
        os.fsync(token.fileno())
    logfile = args.output / 'mount-evidence.log'
    with logfile.open('x') as log, redirect_stdout(Tee(sys.stdout, log)), \
            redirect_stderr(Tee(sys.stderr, log)):
        print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
        print('IMAGE_SHA256=' + digest, flush=True)
        print('XTS_PREREQUISITES=' + json.dumps(evidence, sort_keys=True), flush=True)
        # Establish inactive modem lines before opening, then exactly one reset.
        port = serial.Serial(port=None, baudrate=115200, timeout=.1,
                             write_timeout=1, exclusive=True)
        port.dtr = False
        port.rts = False
        port.port = '/dev/ttyUSB0'
        with port:
            print('XTS_RESET=single_initial_reset', flush=True)
            kv.transport.hard_reset(port)
            try:
                boot = kv.transport.collect_until_prompt(port, 30).decode(errors='replace')
            except TimeoutError as exc:
                print(str(exc), flush=True)
                raise
            print(boot, flush=True)
            require('NuttShell (NSH)' in boot and
                    'xTS Flash scratch: /dev/xtsflash offset=0xc00000 size=0x100000' in boot,
                    'wrong boot/scratch marker')
            require(not any(m.decode() in boot for m in kv.transport.BOOT_FAILURE_MARKERS),
                    'boot failure')
            def cmd(command):
                print('', flush=True)
                return kv.command(port, command)
            mounts = cmd('mount')
            entries = re.findall(r'^\s*(?:\[CPU\d+\]\s*)?(/\S*) type (\S+)\s*$', mounts, re.M)
            require(('/tmp', 'tmpfs') in entries and not any(
                p in ('/data', '/apps') or p.startswith(('/data/', '/apps/'))
                for p, _ in entries), 'unexpected existing data/apps mount or missing tmpfs')
            require(not re.search(r'\bkvdbd\b', cmd('ps')), 'existing kvdbd')
            require('xtsflash' in cmd('ls /dev/xtsflash'), 'scratch node missing')
            cmd('mkdir -p /data')
            cmd('mount -t littlefs -o forceformat /dev/xtsflash /data')
            require(re.search(r'^\s*/data type littlefs\s*$', cmd('mount'), re.M),
                    'scratch mount not present')
            cmd('df')
            cmd('free')
            print('XTS_1012_FIRST_SCRATCH_MOUNT=PASS no_kv_test_executed', flush=True)
        log.flush()
        kv.check_mount_evidence(logfile.read_text(), args.receipt, digest)
    (args.output / 'result.json').write_text(json.dumps({
        'status': 'PASS', 'preparation_only': True, 'receipt': str(args.receipt.resolve()),
        'mount_evidence_sha256': sha(logfile), 'exclusive_token': str(TOKEN),
        'scratch_offset': 0xc00000, 'scratch_size': 0x100000,
        'kvdb_started': False, 'kvdb_test_executed': False}, indent=2) + '\n')


if __name__ == '__main__':
    main()
