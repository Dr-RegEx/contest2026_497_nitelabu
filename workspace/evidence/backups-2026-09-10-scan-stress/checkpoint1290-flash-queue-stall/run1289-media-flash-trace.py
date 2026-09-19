"""Original intact AAC from Flash, diagnosing WAV playback commands; listening remains pending."""
import importlib.util
from pathlib import Path
import re
import time

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('media', D / 'run1261-media-open.py')
m = importlib.util.module_from_spec(s)
s.loader.exec_module(m)
print('XTS_RECEIPT=' + str(D / 'build1285-media-flash-trace.sha256'), flush=True)
print('SCOPE=original file decode/driver execution; audible acceptance pending', flush=True)
with m.k.existing_uart() as port:
    m.inner(port, 'mediatool')
    m.inner(port, 'send all loglevel 24')
    output = m.inner(port, 'open Music')
    m.k.require(b'player ID 0' in output, 'unexpected player handle')
    m.inner(port, 'prepare 0 url /flash/audio_file.aac', timeout=60)
    started = time.monotonic()
    output = m.inner(port, 'start 0')
    while time.monotonic() - started < 180:
        if re.search(rb'event:COMPLETED\b', output, re.I):
            break
        chunk = port.read(port.in_waiting or 1)
        if chunk:
            print(chunk.decode(errors='replace'), end='', flush=True)
            output += chunk
        m.k.require(not re.search(rb'error|fail|panic|assert|ESP-ROM:|ret:-|XRUN|underrun|overrun', output, re.I),
                    'media execution error; preserve board state')
    print('\nPLAYBACK_SECONDS=%.3f' % (time.monotonic() - started), flush=True)
    m.k.require(re.search(rb'event:COMPLETED\b.*?ret:0', output, re.I),
                'normal completion missing; no automatic retry')
    m.inner(port, 'close 0')
    m.inner(port, 'q', b'nsh> ')
    m.k.command(port, 'ps')
    m.k.command(port, 'free')
    print('DECODE_DRIVER_COMPLETE; LISTENING_PENDING; NOT category PASS', flush=True)
