"""Reset one prepared 1144 KVDB image and mount existing scratch without formatting."""
import argparse
from contextlib import redirect_stdout, redirect_stderr
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

D = Path(__file__).resolve().parent


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, D / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


prep = load('first_mount_helpers', 'run1012-kv-first-mount.py')
require, sha = prep.require, prep.sha
IMAGES = {'b41c760b9b43daa756e39a1dfd8f5b2ffc0c8564c692551dc4c3ce027c5aa3d8': '1144'}


def preflight(receipt):
    lines = receipt.read_text().splitlines()
    require(len(lines) == 1, 'one FLAT image receipt required')
    digest, name = lines[0].split(maxsplit=1)
    image = Path(name)
    require(digest in IMAGES and image.name == 'nuttx.bin' and sha(image) == digest,
            'expected unchanged 1144 KVDB image receipt')
    config = (image.parent / '.config').read_text()
    for option in ('BUILD_FLAT', 'ESP32S31_XTS_FLASH', 'ESP32S31_XTS_FLASH_LARGE', 'FS_LITTLEFS',
                   'FS_TMPFS', 'TESTS_TESTCASES', 'KVDB_TEST', 'KVDB_TEST_STABILITY', 'KVDB_SERVER'):
        require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    if 'CONFIG_ESPRESSIF_SPIRAM_USER_HEAP=y\n' in config:
        for option in ('ESP32S31_SPIFLASH_PSRAM_STACK', 'SCHED_LPWORK'):
            require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    if IMAGES[digest] == '974':
        require('CONFIG_ESPRESSIF_SPIRAM_USER_HEAP=y\n' not in config and
                'CONFIG_MM_REGIONS=1\n' in config, '974 internal heap configuration mismatch')
    return digest, sha(image.parent / '.config')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='new evidence directory')
    args = parser.parse_args()
    digest, config_sha = preflight(args.receipt)
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(), 'UART occupied or fuser failed')
    kv = load('kv_mount_transport', 'xts-kvdb-suite.py')
    import serial
    args.output.mkdir(parents=True, exist_ok=False)
    logfile = args.output / 'mount-evidence.log'
    with logfile.open('x') as log, redirect_stdout(prep.Tee(sys.stdout, log)), \
            redirect_stderr(prep.Tee(sys.stderr, log)):
        print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
        print('IMAGE_SHA256=' + digest, flush=True)
        print('CONFIG_SHA256=' + config_sha, flush=True)
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
                for p, _ in entries), 'unexpected mount layout')
            require(not re.search(r'\bkvdbd\b|vela_fs_', cmd('ps')), 'unexpected active workload')
            require('xtsflash' in cmd('ls /dev/xtsflash'), 'scratch device missing')
            cmd('mkdir -p /data')
            cmd('mount -t littlefs /dev/xtsflash /data')
            require(re.search(r'^\s*/data type littlefs\s*$', cmd('mount'), re.M),
                    'existing scratch did not mount; no recovery or formatting')
            cmd('ls -l /data')
            cmd('df')
            cmd('free')
            print('XTS_EXISTING_SCRATCH_MOUNT=PASS candidate=' + IMAGES[digest], flush=True)
        log.flush()
        kv.check_mount_evidence(logfile.read_text(), args.receipt, digest)
    (args.output / 'result.json').write_text(json.dumps({
        'status': 'PASS', 'preparation_only': True, 'candidate': IMAGES[digest],
        'image_sha256': digest, 'mount_evidence_sha256': sha(logfile),
        'formatted': False, 'deleted_files': False, 'tests_executed': False}, indent=2) + '\n')


if __name__ == '__main__':
    main()
