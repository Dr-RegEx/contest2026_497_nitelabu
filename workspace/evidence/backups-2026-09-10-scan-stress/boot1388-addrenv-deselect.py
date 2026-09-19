"""Boot the receipted offline1328 pair; no network setup or filesystem changes."""
from pathlib import Path
import hashlib
import importlib.util
import subprocess
import serial

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
r = D / 'build1387-addrenv-deselect.sha256'
entries = r.read_text().splitlines()
k.require(len(entries) == 2, 'paired receipt required')
paths = []
for line in entries:
    digest, name = line.split(maxsplit=1)
    p = Path(name)
    k.require(hashlib.sha256(p.read_bytes()).hexdigest() == digest, 'artifact mismatch')
    paths.append(p)
k.require({p.name for p in paths} == {'nuttx.bin', 'appfs.img'} and
          len({p.parent for p in paths}) == 1, 'ABI pair mismatch')
cfg = (paths[0].parent / '.config').read_text()
k.require('CONFIG_NETINIT_NETLOCAL=y\n' in cfg and
          'CONFIG_NETINIT_DHCPC=y\n' not in cfg, 'automatic network setup forbidden')
b = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
k.require(b.returncode == 1 and not b.stderr.strip(), 'UART busy')
print('XTS_RECEIPT=' + str(r), flush=True)
with serial.Serial('/dev/ttyUSB0', 115200, timeout=.1, write_timeout=1, exclusive=True) as p:
    k.transport.hard_reset(p)
    boot = k.transport.collect_until_prompt(p, 30)
    print(boot.decode(errors='replace'), flush=True)
    k.require(b'NuttShell (NSH)' in boot and
              not any(m in boot for m in k.transport.BOOT_FAILURE_MARKERS), 'boot failure')
    print('S31_OFFLINE_BOOT=READY', flush=True)
