"""Boot the isolated media candidate; prepare only volatile filesystems."""
import hashlib
import importlib.util
from pathlib import Path
import re
import serial
import subprocess

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
receipt = D / 'build1220-media.sha256'
digest, filename = receipt.read_text().strip().split(maxsplit=1)
k.require(hashlib.sha256(Path(filename).read_bytes()).hexdigest() == digest,
          'image changed')
k.require(subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True).returncode == 1,
          'UART busy')
print('XTS_RECEIPT=' + str(receipt), flush=True)
with serial.Serial('/dev/ttyUSB0', 115200, timeout=.1, write_timeout=1,
                   exclusive=True) as port:
    k.transport.hard_reset(port)
    boot = k.transport.collect_until_prompt(port, 30)
    print(boot.decode(errors='replace'), flush=True)
    k.require(not any(m in boot for m in k.transport.BOOT_FAILURE_MARKERS),
              'boot fault')
    devices = k.command(port, 'ls /dev/audio')
    k.require('pcm0p' in devices and 'pcm0c' in devices, 'audio nodes missing')
    k.command(port, 'free')
    mounts = k.command(port, 'mount')
    k.require(not re.search(r'(?m)^/data type ', mounts), 'unexpected /data mount')
    k.command(port, 'mkdir /data')
    k.command(port, 'mount -t tmpfs /data')
    k.command(port, 'mkdir /data/media')
    k.command(port, 'mkdir /tmp/media-kv')
    k.command(port, 'mount')
    print('VOLATILE_MEDIA_STORAGE_READY; no media playback PASS', flush=True)
