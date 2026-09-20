"""Report raw stereo16 buffer-boundary energy; never alter captured samples."""
import argparse
import array
import hashlib
import json
import math
import sys
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('capture', type=Path)
args = parser.parse_args()
result = json.loads((args.capture / 'result.json').read_text())
assert result['status'] == 'TRANSFER_COMPLETE'
assert result['recording_format']['channels'] == 2
assert result['recording_format']['bits'] == 16
raw = Path(result['local_file']).read_bytes()
assert hashlib.sha256(raw).hexdigest() == result['local_sha256']
assert len(raw) % 4 == 0
samples = array.array('h', raw)
if sys.byteorder != 'little':
    samples.byteswap()
reports = []
for channel in range(2):
    values = samples[channel::2]
    sums = [0] * 1024
    counts = [0] * 1024
    for index, value in enumerate(values):
        phase = index % 1024
        sums[phase] += value * value
        counts[phase] += 1
    energy = [math.sqrt(total / count) if count else 0
              for total, count in zip(sums, counts)]
    rest = math.sqrt(sum(sums[1:]) / sum(counts[1:]))
    reports.append({'channel': channel, 'phase0_rms': energy[0],
                    'other_phases_rms': rest,
                    'phase0_to_other_ratio': energy[0] / rest if rest else None,
                    'largest_phase': max(range(1024), key=energy.__getitem__),
                    'largest_phase_rms': max(energy)})
print(json.dumps({'pcm_sha256': result['local_sha256'],
                  'buffer_bytes': 4096, 'channels': reports,
                  'acceptance': 'Diagnostic only; human listening required'}, indent=2))
