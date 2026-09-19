"""Diagnostic intact original AAC transfer/playback from restored Flash.

Stops on any failure, preserves target and evidence; never resets or formats.
"""
import json
from pathlib import Path
import subprocess
from datetime import datetime, timezone

D = Path(__file__).resolve().parent
R = D.parent.parent
P = R / 's31-reference/.venv-nuttx/bin/python'
receipt = D / 'build1265-media-flash.sha256'
source = R / 'openvela-dev/docs/zh-cn/test_dev_guide/mediatool测试资源和测试步骤/测试资源/音视频测试资源文件/audio_file.aac'
statefile = D / 'flash1270-unattended-state.json'
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
prep = json.loads((D / 'scratch1266-original-restore/result.json').read_text())
if prep['state'] != 'RESTORED_AND_READBACK_VERIFIED':
    raise RuntimeError('original Flash restoration not verified')
try:
    step('xts1268-media-config', 'stage1268-media-config.py')
    step('xts1270-aac-upload', 'audio-file-transfer.py',
         ['--upload', source, '--directory', '/flash', '--receipt', receipt,
          '--output', D / 'media1270-aac-upload'])
    step('xts1270-aac-readback', 'audio-file-transfer.py',
         ['--download', 'audio_file.aac', '--directory', '/flash',
          '--receipt', receipt, '--output', D / 'media1270-aac-readback'])
    received = json.loads((D / 'media1270-aac-readback/result.json').read_text())
    if received['local_sha256'] != 'ad27e37f5af6eb231caea4a3910a85b2087d7fc272c44a9533544f54c724ce59':
        raise RuntimeError('original AAC readback mismatch')
    step('xts1269-media-start', 'start1269-media.py')
    step('xts1271-media-flash-aac', 'run1271-media-flash-aac.py')
    state.update(status='DIGITAL_EXECUTION_COMPLETE', step='await evidence review; listening pending')
except BaseException as error:
    state.update(status='FAILED', error=str(error))
    raise
finally:
    save()
