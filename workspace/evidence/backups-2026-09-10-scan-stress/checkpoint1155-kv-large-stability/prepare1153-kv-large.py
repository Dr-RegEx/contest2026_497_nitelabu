"""Verify the large test volume has no old DB before the required reboot."""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import subprocess

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
r = D / 'build1144-kv-large.sha256'
h, name = r.read_text().strip().split(maxsplit=1)
k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == h, 'image mismatch')
k.check_mount_evidence((D / 'checkpoint1152-kv-large-mount/mount-evidence.log').read_text(), r, h)
p = D / 'flash1150-large-before-kv'
m = json.loads((p / 'manifest.json').read_text())
k.require(m['offset'] == 0xd00000 and m['size'] == 0x300000 and
          (p / 'volume.bin').stat().st_size == m['size'] and
          hashlib.sha256((p / 'volume.bin').read_bytes()).hexdigest() == m['sha256'],
          'large volume backup mismatch')
b = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
k.require(b.returncode == 1 and not b.stderr.strip(), 'UART busy')
with k.existing_uart() as port:
    k.require('/data type littlefs' in k.command(port, 'mount'), 'mount missing')
    k.require(not re.search(r'\bkvdbd\b|vela_fs_|vela_kvdb_', k.command(port, 'ps')),
              'active workload')
    listing = k.transport.run_command(port, 'ls -l /data/persist.db', timeout=20, reset_input=False)
    k.require('No such file' in listing, 'existing DB requires review; no deletion performed')
    k.command(port, 'df')
    print('PUBLISHED_5.1.68_DB_ALREADY_ABSENT', flush=True)
    print('NEXT_REQUIRED=original_reboot_and_nofmt_mount_before_stability10', flush=True)
