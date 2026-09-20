"""Release only archived performance payloads; published KV DB removal, no format."""
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
r = D / 'build948-flat-category-kv.sha256'
digest, image = r.read_text().strip().split(maxsplit=1)
k.require(hashlib.sha256(Path(image).read_bytes()).hexdigest() == digest, 'image mismatch')
k.check_mount_evidence(a.mount_evidence.read_text(), r, digest)
backup = D / 'flash1110-kv-errors/volume.bin'
k.require(backup.stat().st_size == 1048576 and
          hashlib.sha256(backup.read_bytes()).hexdigest() ==
          '85e8c785368e14e91856c7a920c75382aa87a825153a79ca718df5b8aaa87d0a',
          'post-error backup mismatch')
b = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
k.require(b.returncode == 1 and not b.stderr.strip(), 'UART busy')
print('XTS_RECEIPT=' + str(r), flush=True)
with k.existing_uart() as port:
    k.require('/data type littlefs' in k.command(port, 'mount'), 'mount missing')
    k.require(not re.search(r'\bkvdbd\b|vela_fs_|vela_kvdb_', k.command(port, 'ps')),
              'active workload')
    paths = [('/data/s11/performance_test', 262144),
             ('/data/s12/payload', 262144), ('/data/persist.db', 147456)]
    for path, size in paths:
        listing = k.command(port, 'ls -l ' + path)
        k.require(re.search(r'(?m)^\s*-\S+\s+' + str(size) + r'\s+' +
                            re.escape(path) + r'\s*$', listing), 'unexpected file ' + path)
    k.command(port, 'df')
    for path, _ in paths:
        k.command(port, 'rm ' + path)
    k.command(port, 'df')
    print('PUBLISHED_5.1.68_DB_REMOVED archived_old_performance_payloads_released', flush=True)
    print('NEXT_REQUIRED=original_reboot_and_nofmt_mount_before_stability10', flush=True)
