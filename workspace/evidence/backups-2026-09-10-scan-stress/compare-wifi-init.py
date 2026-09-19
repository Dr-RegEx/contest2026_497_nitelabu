"""Expand real Wi-Fi initializers using the successful builds' compiler flags.

Read-only to source/build trees: preprocess to a pipe, discard dependency and
object-output flags, and report only the initializer and relevant feature flags.
"""
import json
from pathlib import Path
import re
import shlex
import subprocess

root = Path('/home/regex/work/esp32s31-openvela')
profiles = (
    ('nuttx', 'openvela-dev/out/esp32s31-cmake-wifi-cpu0198',
     '/esp_wifi_adapter.c', 'wifi_cfg'),
    ('idf-hal', 'diagnostics/idf-baseline/build-hal-wifi',
     '/main/baseline.c', 'init'),
)
for label, build, suffix, variable in profiles:
    commands = json.loads((root / build / 'compile_commands.json').read_text())
    matches = [entry for entry in commands if entry['file'].endswith(suffix)]
    if len(matches) != 1:
        raise SystemExit(f'{label}: expected one compiler command, got {len(matches)}')
    entry = matches[0]
    original = iter(shlex.split(entry['command']))
    command = []
    for argument in original:
        if argument in ('-o', '-MF', '-MT', '-MQ'):
            next(original)
        elif argument not in ('-c', '-MD', '-MMD', '-MP'):
            command.append(argument)
    command.extend(('-E', '-P'))
    if label == 'nuttx':
        # Normal out-of-tree builds temporarily hide the source Make config.
        # Force the archived build config before its header guard can select
        # that unrelated minimal config; leave both files untouched.
        command[1:1] = ['-include', str(root / build / 'include/nuttx/config.h')]
    print(label + '_COMMAND=' + shlex.join(command), flush=True)
    result = subprocess.run(command, cwd=entry['directory'], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    if result.stderr:
        print(result.stderr, flush=True)
    initializer = re.search(r'wifi_init_config_t\s+' + variable + r'\s*=\s*(\{.*?\});',
                            result.stdout, re.S)
    if not initializer:
        raise SystemExit(label + ': initializer not found (check config/header selection)')
    if 'CONFIG_' in initializer.group(1):
        raise SystemExit(label + ': unexpanded config symbols, invalid comparison')
    print(label + '_INITIALIZER=' + ' '.join(initializer.group(1).split()), flush=True)
    print(label + '_POST_INIT_ASSIGNMENTS=' + json.dumps(re.findall(
        r'\b' + variable + r'\.\w+\s*=\s*[^;]+;', result.stdout)), flush=True)
print('WIFI_INIT_PREPROCESS=PASS', flush=True)
