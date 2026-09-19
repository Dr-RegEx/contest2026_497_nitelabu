"""Published KV DB removal only, backed-up1119 failure preserved; no format."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import re
import subprocess

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--mount-evidence', type=Path, required=True)
a = p.parse_args()
r = D / 'build1130-kv-errno.sha256'
digest, image = r.read_text().strip().split(maxsplit=1)
k.require(hashlib.sha256(Path(image).read_bytes()).hexdigest() == digest, 'image mismatch')
k.check_mount_evidence(a.mount_evidence.read_text(), r, digest)
backup = D / 'flash1122-kv-failure/volume.bin'
k.require(backup.stat().st_size == 1048576 and
          hashlib.sha256(backup.read_bytes()).hexdigest() ==
          '83600abe303822b2a7ec22cbd6b1dd4baf119680b7a4785c06870b3a46ed1950',
          'post-error backup mismatch')
b = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
k.require(b.returncode == 1 and not b.stderr.strip(), 'UART busy')
print('XTS_RECEIPT=' + str(r), flush=True)
with k.existing_uart() as port:
    k.require('/data type littlefs' in k.command(port, 'mount'), 'mount missing')
    k.require(not re.search(r'\bkvdbd\b|vela_fs_|vela_kvdb_', k.command(port, 'ps')),
              'active workload')
    paths = [('/data/persist.db', 389120)]
    for path, size in paths:
        listing = k.command(port, 'ls -l ' + path)
        k.require(re.search(r'(?m)^\s*-\S+\s+' + str(size) + r'\s+' +
                            re.escape(path) + r'\s*$', listing), 'unexpected file ' + path)
    k.command(port, 'df')
    for path, _ in paths:
        k.command(port, 'rm ' + path)
    k.command(port, 'df')
    print('PUBLISHED_5.1.68_DB_REMOVED archived_failed_database_removed', flush=True)
    print('NEXT_REQUIRED=original_reboot_and_nofmt_mount_before_stability10', flush=True)
