"""Actual MMU audio lifecycle diagnostic; does not replace listening xTS."""
import hashlib
import importlib.util
import json
import re
import subprocess
import sys
import time
from pathlib import Path

D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
r = {'status': 'FAIL', 'scope': 'MMU_AUDIO_LIFECYCLE_DIAGNOSTIC',
     'receipt': 'build1575-audio-kernel.sha256', 'captures': []}

def collect(p, marker, timeout=20):
    try:
        data = k.transport.collect_until_marker(p, marker, timeout)
    except TimeoutError as exc:
        print(str(exc), flush=True)
        raise
    sys.stdout.buffer.write(data)
    sys.stdout.flush()
    text = data.decode(errors='replace')
    k.require(not re.search(r'Assertion failed|PANIC|KASAN:|ESP-ROM:|'
                           r'Error recording|No suitable|device busy|'
                           r'ERROR:|page fault', text, re.I), 'audio target error')
    return text

def audio_command(p, command, marker=b'nxrecorder> '):
    print('AUDIO_COMMAND=' + command, flush=True)
    p.write((command + '\n').encode())
    return collect(p, marker)

try:
    for row in (D / r['receipt']).read_text().splitlines():
        digest, name = row.split(maxsplit=1)
        k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest,
                  'receipt mismatch')
    k.require(subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True).returncode == 1,
              'UART busy')
    print('XTS_RECEIPT=' + str(D / r['receipt']), flush=True)
    with k.existing_uart() as p:
        k.command(p, 'ls /dev/audio')
        k.command(p, 'mount')
        k.command(p, 'free')
        for index in range(2):
            path = '/tmp/audio1576-%d.pcm' % index
            audio_command(p, 'nxrecorder')
            audio_command(p, 'device /dev/audio/pcm0c')
            audio_command(p, 'recordraw %s 2 16 44100' % path)
            time.sleep(3)
            if index == 0:
                audio_command(p, 'stop')
            audio_command(p, 'quit', b'nsh> ')
            listing = k.command(p, 'ls -l ' + path)
            match = re.search(r'\s(\d+)\s+(?:/tmp/)?audio1576-%d.pcm' % index, listing)
            k.require(match and int(match.group(1)) > 0, 'missing captured PCM')
            r['captures'].append({'file': path, 'bytes': int(match.group(1)),
                                  'close': 'stop_then_quit' if index == 0 else 'quit_active'})
            k.command(p, 'ps')
            k.command(p, 'free')
        r['status'] = 'LIFECYCLE_PASS_LISTENING_NOT_TESTED'
except Exception as exc:
    r['error'] = str(exc)
finally:
    (D / 'audio1576-kernel-lifecycle-result.json').write_text(json.dumps(r, indent=2) + '\n')
    print(json.dumps(r), flush=True)
raise SystemExit(0 if r['status'].startswith('LIFECYCLE_PASS') else 1)
