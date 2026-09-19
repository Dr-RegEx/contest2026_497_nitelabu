"""Explicit reset of frozen1690, capture boot identity; never mounts/formats."""
from pathlib import Path
import hashlib
import importlib.util
import subprocess
import serial

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
r = D / 'build1700-fs-worker.sha256'
digest, name = r.read_text().strip().split(maxsplit=1)
k.require(digest == '8e612e77b2d1924e403ccc197ab97735cc6de0fa7adf3a77e81789330576db9e'
          and hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest, 'image mismatch')
b = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
k.require(b.returncode == 1 and not b.stderr.strip(), 'UART busy')
print('XTS_RECEIPT=' + str(r), flush=True)
print('IMAGE_SHA256=' + digest, flush=True)
with serial.Serial('/dev/ttyUSB0', 115200, timeout=.1, write_timeout=1, exclusive=True) as p:
    k.transport.hard_reset(p)
    boot = k.transport.collect_until_prompt(p, 30)
    print(boot.decode(errors='replace'), flush=True)
    k.require(b'NuttShell (NSH)' in boot and
              b'xTS Flash scratch: /dev/xtsflash offset=0xd00000 size=0x300000' in boot,
              'expected1690 scratch boot')
    k.require(not any(m in boot for m in k.transport.BOOT_FAILURE_MARKERS), 'boot fault')
    print('XTS_1690_BOOT=READY unmounted', flush=True)
