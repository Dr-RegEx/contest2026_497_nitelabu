"""Initialize media service using verified runtime files; no playback verdict."""
import importlib.util
from pathlib import Path
import re
import time

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)
print('XTS_RECEIPT=' + str(D / 'build1273-media-flash-trace.sha256'), flush=True)
with k.existing_uart() as port:
    for name in ('graph.conf', 'criteria.txt', 'settings.pfw'):
        k.command(port, 'mv /data/' + name + ' /data/media/' + name)
    k.command(port, 'mediad &')
    output = b''
    until = time.monotonic() + 5
    while time.monotonic() < until:
        output += port.read(port.in_waiting or 1)
    print(output.decode(errors='replace'), flush=True)
    processes = k.command(port, 'ps')
    k.command(port, 'free')
    k.require(not re.search(rb'error|fail|panic|assert|ESP-ROM:', output, re.I),
              'media initialization error')
    k.require('mediad' in processes, 'media daemon exited')
    print('MEDIA_DAEMON_ALIVE; original playback/listening pending', flush=True)
