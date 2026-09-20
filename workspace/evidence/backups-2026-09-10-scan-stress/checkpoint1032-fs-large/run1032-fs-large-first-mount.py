"""Prepare candidate1032 scratch LittleFS once; does not run tests or flash."""
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
EXPECTED_IMAGE = '7b0e0dd869e157a1620297412caba0d99b5e6b068fff0e3a00bf05ceb7ced47a'
TOKEN = D / '.run1032-fs-large-first-mount.used'


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def preflight(receipt, backup):
    lines = receipt.read_text().splitlines()
    require(len(lines) == 1, 'one image receipt required')
    digest, name = lines[0].split(maxsplit=1)
    image = Path(name)
    require(image.name == 'nuttx.bin' and digest == EXPECTED_IMAGE and sha(image) == digest,
            'candidate1032 image mismatch')
    config_path = image.parent / '.config'
    config = config_path.read_text()
    for option in ('BUILD_FLAT', 'ESP32S31_XTS_FLASH', 'ESP32S31_XTS_FLASH_LARGE',
                   'FS_LITTLEFS', 'FS_TMPFS', 'ESP32S31_SPIFLASH_PSRAM_STACK',
                   'SCHED_LPWORK', 'FS_TEST', 'FS_TEST_STRESS', 'FS_TEST_STABILITY'):
        require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    require('CONFIG_ESP32S31_XTS_MEDIA_VOLUME=y\n' not in config,
            'overlapping WAV volume selected')
    manifest = json.loads(backup.read_text())
    require(manifest.get('offset') == 0xd00000 and manifest.get('size') == 0x300000 and
            manifest.get('blank') is True, 'wrong or nonblank 3MiB backup')
    require([x.get('name') for x in manifest.get('files', [])] ==
            ['before-a.bin', 'before-b.bin'], 'two distinct original reads required')
    for entry in manifest['files']:
        path = backup.parent / entry['name']
        data = path.read_bytes()
        require(len(data) == 0x300000 and data == b'\xff' * len(data) and
                sha(path) == entry['sha256'], 'backup length/hash/blank mismatch')
    require(not (backup.parent / 'used-by-media-volume-987.json').exists(),
            'backup already consumed by media or large FS; obtain new verified backup after restoration')
    return digest, {str(p): sha(p) for p in (receipt, config_path, backup)}


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
    parser.add_argument('--receipt', type=Path, default=D / 'build1032-flat-category-fs-large.sha256')
    parser.add_argument('--output', type=Path, required=True, help='new evidence directory')
    parser.add_argument('--flash-backup', type=Path, required=True,
                        help='fresh exact-range 3MiB double-read blank manifest')
    args = parser.parse_args()
    digest, evidence = preflight(args.receipt, args.flash_backup)
    require(not TOKEN.exists(), '1032 exclusive token exists; no automatic rerun or reformat')
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
    # Same range-consumption token recognized by the existing media formatter.
    # FS use consumes this original backup too; later media needs restoration
    # and a fresh independently verified blank backup, never this old manifest.
    with (args.flash_backup.parent / 'used-by-media-volume-987.json').open('x') as used:
        json.dump({'consumer': 'filesystem1032', 'image_sha256': digest,
                   'output': str(args.output.resolve())}, used)
        used.flush()
        os.fsync(used.fileno())
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
                    'xTS Flash scratch: /dev/xtsflash offset=0xd00000 size=0x300000' in boot,
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
            print('XTS_1032_FIRST_LARGE_SCRATCH_MOUNT=PASS no_test_executed', flush=True)
        log.flush()
        kv.check_mount_evidence(logfile.read_text(), args.receipt, digest)
    (args.output / 'result.json').write_text(json.dumps({
        'status': 'PASS', 'preparation_only': True, 'receipt': str(args.receipt.resolve()),
        'mount_evidence_sha256': sha(logfile), 'exclusive_token': str(TOKEN),
        'scratch_offset': 0xd00000, 'scratch_size': 0x300000,
        'kvdb_started': False, 'kvdb_test_executed': False}, indent=2) + '\n')


if __name__ == '__main__':
    main()
