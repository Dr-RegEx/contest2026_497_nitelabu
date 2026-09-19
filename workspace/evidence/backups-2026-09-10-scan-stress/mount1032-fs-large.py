"""Mount existing1102 3MiB scratch synchronously; no reset, format, or deletion."""
import argparse
from contextlib import redirect_stdout, redirect_stderr
import hashlib
import importlib.util
from pathlib import Path
import re
import subprocess
import sys

D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('sync_recovery', D / 'run1102-fs-sync-recovery.py')
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, default=D / 'build1032-flat-category-fs-large.sha256')
    parser.add_argument('--boot-evidence', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    a = parser.parse_args()
    digest, name = a.receipt.read_text().strip().split(maxsplit=1)
    image = Path(name)
    r.require(digest == '7b0e0dd869e157a1620297412caba0d99b5e6b068fff0e3a00bf05ceb7ced47a' and hashlib.sha256(image.read_bytes()).hexdigest() == digest,
              'exact1102 receipt/image required')
    cfg = (image.parent / '.config').read_text()
    for opt in ('BUILD_FLAT', 'ESP32S31_XTS_FLASH_LARGE', 'FS_LITTLEFS'):
        r.require(f'CONFIG_{opt}=y\n' in cfg, 'missing ' + opt)
    boot = a.boot_evidence.read_text()
    r.require(digest in boot and r.BOOT in boot and 'NuttShell (NSH)' in boot,
              'current1102 receipt-identified boot evidence required')
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    r.require(busy.returncode == 1 and not busy.stderr.strip(), 'UART busy or fuser failed')
    kv = r.load('sync_kv', 'xts-kvdb-suite.py')
    prep = r.load('sync_prep', 'run1012-kv-first-mount.py')
    a.output.mkdir(parents=True, exist_ok=False)
    evidence = a.output / 'mount-evidence.log'
    with evidence.open('x') as log, redirect_stdout(prep.Tee(sys.stdout, log)), redirect_stderr(prep.Tee(sys.stderr, log)):
        print('XTS_RECEIPT=' + str(a.receipt.resolve()), flush=True)
        print('IMAGE_SHA256=' + digest, flush=True)
        print(r.BOOT, flush=True)
        print('XTS_BOOT_EVIDENCE_SHA256=' + hashlib.sha256(a.boot_evidence.read_bytes()).hexdigest(), flush=True)
        with r.existing_recovery_uart() as port:
            def cmd(s):
                print('', flush=True)
                return kv.command(port, s)
            mounts = cmd('mount')
            r.require(not re.search(r'^\s*/(?:data|apps)(?:/\S*)? type ', mounts, re.M), 'existing data/apps mount; no unmount/recovery')
            r.require(not re.search(r'\bkvdbd\b|vela_fs_', cmd('ps')), 'active workload')
            cmd('ls /dev/xtsflash')
            cmd('mkdir -p /data')
            cmd('mount -t littlefs -o sync /dev/xtsflash /data')
            cmd('mount')
            cmd('ls -l /data')
            cmd('df')
            cmd('free')
        log.flush()
        r.check_sync_mount(kv, evidence.read_text(), a.receipt)
        print('XTS_SYNC_MOUNT=PASS preparation_only', flush=True)


if __name__ == '__main__':
    main()
