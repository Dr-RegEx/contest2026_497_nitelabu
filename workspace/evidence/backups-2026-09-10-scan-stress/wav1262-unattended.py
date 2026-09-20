"""Sequential intact WAV transfer/playback after verified volume preparation.

Stops on any failure, preserves target and evidence; never resets or formats.
"""
import json
from pathlib import Path
import subprocess
from datetime import datetime, timezone

D = Path(__file__).resolve().parent
R = D.parent.parent
P = R / 's31-reference/.venv-nuttx/bin/python'
receipt = D / 'build1254-media-volume.sha256'
source = R / 'openvela-dev/docs/zh-cn/test_dev_guide/mediatool测试资源和测试步骤/测试资源/音视频测试资源文件/audio_file.wav'
statefile = D / 'wav1262-unattended-state.json'
state = {'status': 'STARTED', 'listening': 'PENDING', 'network': 'NO_PROVISIONING'}


def save():
    state['updated_utc'] = datetime.now(timezone.utc).isoformat()
    statefile.write_text(json.dumps(state, indent=2) + '\n')


def step(name, script, args=()):
    state.update(status='RUNNING', step=name)
    save()
    print('START ' + name, flush=True)
    with (D / 'logs' / (name + '.log')).open('x') as log:
        subprocess.run([str(P), str(D / script), *map(str, args)],
                       check=True, stdout=log, stderr=subprocess.STDOUT)
    print('COMPLETE ' + name, flush=True)


if statefile.exists():
    raise RuntimeError('Existing attempt; inspect before retry')
prep = json.loads((D / 'wav1258-volume/result.json').read_text())
if prep['status'] != 'TEMPORARY_VOLUME_PREPARED':
    raise RuntimeError('WAV volume not prepared')
try:
    step('xts1259-media-config', 'stage1259-media-config.py')
    step('xts1262-wav-upload', 'audio-file-transfer.py',
         ['--upload', source, '--directory', '/wav', '--receipt', receipt,
          '--output', D / 'media1262-wav-upload'])
    step('xts1262-wav-readback', 'audio-file-transfer.py',
         ['--download', 'audio_file.wav', '--directory', '/wav',
          '--receipt', receipt, '--output', D / 'media1262-wav-readback'])
    step('xts1260-media-start', 'start1260-media.py')
    step('xts1263-media-wav', 'run1263-media-wav.py')
    state.update(status='DIGITAL_EXECUTION_COMPLETE', step='await evidence review; listening pending')
except BaseException as error:
    state.update(status='FAILED', error=str(error))
    raise
finally:
    save()
