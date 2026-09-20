"""Archive completed original WAV digital execution; never opens the UART."""
import hashlib
import json
from pathlib import Path
import re
import shutil

D = Path(__file__).resolve().parent
R = D.parent.parent
expected = '20d7c680be243cac559c1d390a324c5b6740dc32471647c91a7c544fb9df5ef7'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


state = json.loads((D / 'wav1262-unattended-state.json').read_text())
require(state['status'] == 'DIGITAL_EXECUTION_COMPLETE', 'execution not complete')
readback = json.loads((D / 'media1262-wav-readback/result.json').read_text())
require(readback['status'] == 'TRANSFER_COMPLETE' and
        readback['direction'] == 'download' and readback['bytes'] == 17473937,
        'complete target readback required')
received = Path(readback['local_file'])
require(received.stat().st_size == 17473937 and
        hashlib.sha256(received.read_bytes()).hexdigest() == expected,
        'actual target readback differs from original')
digest, filename = (D / 'build1254-media-volume.sha256').read_text().strip().split(maxsplit=1)
firmware = Path(filename)
require(hashlib.sha256(firmware.read_bytes()).hexdigest() == digest,
        'firmware changed')
require(readback['image_sha256'] == digest, 'readback receipt mismatch')
log = (D / 'logs/xts1263-media-wav.log').read_text()
for pattern in (r'event:COMPLETED\([^)]*\) ret:0',
                r'event:STOPPED\([^)]*\) ret:0', r'XTS_COMMAND=close 0',
                r'XTS_COMMAND=q', r'DECODE_DRIVER_COMPLETE; LISTENING_PENDING'):
    require(re.search(pattern, log), 'missing playback evidence: ' + pattern)
require(not re.search(r'XRUN|underrun|overrun|panic|assert|ret:-|Traceback', log, re.I),
        'playback failure marker; manual review required')
duration = re.search(r'PLAYBACK_SECONDS=([0-9.]+)', log)
require(duration, 'missing measured duration')
out = D / 'checkpoint1263-media-wav'
out.mkdir(exist_ok=False)
for name in ('nuttx.bin', 'nuttx', '.config', 'System.map'):
    shutil.copy2(firmware.parent / name, out / name)
for name in ('build1254-media-volume.sha256', 'wav1262-unattended-state.json',
             'wav1262-unattended.py', 'run1263-media-wav.py',
             'archive1263-media-wav.py', 'wav1254-execution-notes.md'):
    shutil.copy2(D / name, out / name)
for name in ('xts1257-media-boot.log', 'xts1258-wav-volume.log',
             'xts1259-media-config.log', 'xts1262-unattended.log',
             'xts1260-media-start.log', 'xts1263-media-wav.log'):
    shutil.copy2(D / 'logs' / name, out / name)
for name in ('media1262-wav-upload', 'media1262-wav-readback', 'wav1258-volume'):
    target = out / name
    target.mkdir()
    for leaf in ('result.json', 'uart.log'):
        shutil.copy2(D / name / leaf, target / leaf)
result = {
    'case': '4.1.131', 'result': 'DECODE_DRIVER_COMPLETE_LISTENING_PENDING',
    'resource_bytes': 17473937, 'resource_sha256': expected,
    'target_full_readback_verified': True, 'image_sha256': digest,
    'duration_host_seconds': float(duration.group(1)),
    'whole_case_pass': False, 'listening': 'PENDING',
    'scope': 'Original intact WAV digital execution; not audible quality acceptance',
    'scratch_restore': 'Pending; original preserved in flash1255-before-wav',
}
(out / 'acceptance.json').write_text(json.dumps(result, indent=2) + '\n')
files = sorted(p for p in out.rglob('*') if p.is_file())
checks = [(hashlib.sha256(p.read_bytes()).hexdigest(), p) for p in files]
(out / 'SHA256SUMS').write_text(''.join(
    f'{checksum}  {path.relative_to(out)}\n' for checksum, path in checks))
for checksum, path in checks:
    require(hashlib.sha256(path.read_bytes()).hexdigest() == checksum,
            'archive verification failed')
print(f'ARCHIVED {len(checks)} files; listening pending; no category PASS')
