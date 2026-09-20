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
r = D / 'build1565-simd-int8.sha256'
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
    out = k.transport.run_command(p, 's31simd', timeout=45, reset_input=False)
    k.require('worker=1 completed=100 error=0' in out and
              'worker=2 completed=100 error=0' in out and
              'SIMD arithmetic and full-bank sleep-switch: PASS' in out,
              'SIMD target arithmetic/context test incomplete or failed')
    k.require(not any(marker.decode() in out for marker in k.transport.BOOT_FAILURE_MARKERS), 'target fault')
    k.require('worker=1 initial_cpu=0' in out and 'worker=2 initial_cpu=0' in out and
              'worker=1 affinity_switches=20 PASS' in out and
              'worker=2 affinity_switches=20 PASS' in out, 'migration not verified')
    print('S31_SIMD_MIGRATION_AND_FULL_CONTEXT=PASS', flush=True)
    k.require('INT8 dot16 worker=1 checked=100 PASS' in out and
              'INT8 dot16 worker=2 checked=100 PASS' in out, 'INT8 scalar comparison failed')
    print('S31_SIMD_INT8_DOT=PASS', flush=True)
