"""Type-check the unwired event-group candidate using actual target flags."""
import json
from pathlib import Path
import shlex
import subprocess

root = Path('/home/regex/work/esp32s31-openvela')
build = root / 'openvela-dev/out/esp32s31-cmake-demo'
source = root / 'openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_wifi_event.c'
entries = json.loads((build / 'compile_commands.json').read_text())
matches = [e for e in entries if e['file'].endswith('/esp32s31_wifi_task.c')]
assert len(matches) == 1
entry = matches[0]
original = iter(shlex.split(entry['command']))
command = []
for arg in original:
    if arg in ('-o', '-MF', '-MT', '-MQ'):
        next(original)
    elif arg not in ('-c', '-MD', '-MMD', '-MP'):
        command.append(str(source) if arg == entry['file'] else arg)
assert str(source) in command
command[1:1] = ['-include', str(build / 'include/nuttx/config.h')]
command += ['-fsyntax-only']
print('TARGET_SYNTAX_COMMAND=' + shlex.join(command), flush=True)
subprocess.run(command, cwd=entry['directory'], check=True)
print('TARGET_EVENT_SYNTAX=PASS (not linked or deployed)')
