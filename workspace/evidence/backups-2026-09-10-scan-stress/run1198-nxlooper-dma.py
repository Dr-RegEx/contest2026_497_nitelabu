"""Original nxlooper command bringup only; no audible-quality acceptance."""
import hashlib
import importlib.util
from pathlib import Path
import re
import serial
import subprocess
import time

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
receipt = D / 'build1197-duplex-dma.sha256'
digest, filename = receipt.read_text().strip().split(maxsplit=1)
k.require(hashlib.sha256(Path(filename).read_bytes()).hexdigest() == digest, 'image changed')
k.require(subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True).returncode == 1,
          'UART busy')
print('XTS_RECEIPT=' + str(receipt), flush=True)
print('SCOPE=command bringup; no speaker/listening evidence; NOT category PASS', flush=True)


def inner(port, command, prompt=b'nxlooper> '):
    print('XTS_COMMAND=' + command, flush=True)
    for byte in (command + '\n').encode():
        port.write(bytes([byte]))
        time.sleep(.003)
    output = b''
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        output += port.read(port.in_waiting or 1)
        if prompt in output:
            time.sleep(.1)
            output += port.read(port.in_waiting)
            break
    print(output.decode(errors='replace'), flush=True)
    k.require(prompt in output, 'interactive prompt missing')
    k.require(not re.search(rb'error|fail|panic|assert|not found|not an audio|busy', output, re.I),
              'interactive failure')


with serial.Serial('/dev/ttyUSB0', 115200, timeout=.1, write_timeout=1, exclusive=True) as port:
    k.transport.hard_reset(port)
    boot = k.transport.collect_until_prompt(port, 30)
    print(boot.decode(errors='replace'), flush=True)
    k.require(not any(m in boot for m in k.transport.BOOT_FAILURE_MARKERS), 'boot fault')
    devices = k.command(port, 'ls /dev/audio')
    k.require('pcm0p' in devices and 'pcm0c' in devices, 'audio endpoints missing')
    k.command(port, 'free')
    inner(port, 'nxlooper')
    inner(port, 'device pcm0p')
    inner(port, 'device pcm0c')
    inner(port, 'loopback 2 16 48000')
    start = time.monotonic()
    output = b''
    while time.monotonic() - start < 15:
        output += port.read(port.in_waiting or 1)
    print(output.decode(errors='replace'), flush=True)
    print('OBSERVATION_SECONDS=%.3f' % (time.monotonic()-start), flush=True)
    # Preserve driver errors even if stop/quit themselves succeed.
    failure = re.search(rb'error|fail|panic|assert|timeout|underrun|overrun', output, re.I)
    inner(port, 'stop')
    inner(port, 'q', b'nsh> ')
    k.command(port, 'ps')
    k.command(port, 'free')
    k.require(not failure, 'asynchronous driver failure')
    print('NXLOOPER_COMMANDS=PASS AUDIBLE_LOOPBACK=PENDING', flush=True)
