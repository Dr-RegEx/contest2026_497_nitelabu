"""Read-only timer configuration comparison using recorded compile commands.

Does not establish runtime timing, callback affinity, or HE correctness.
"""
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path('/home/regex/work/esp32s31-openvela')
PATTERN = re.compile(r'PLL_TRACK|SYSTIMER_(LL_FIXED_DIVIDER|COUNTER_ESPTIMER|ALARM_ESPTIMER)|HR_TIMER_TASK|FREERTOS_HZ|USEC_PER_TICK|ESP_TASK_(TIMER_PRIO|WIFI_PRIO|PRIO_MAX)|ESP_TIMER_TASK_AFFINITY')

for label, relative, suffixes in (
    ('nuttx635', 'openvela-dev/out/esp32s31-cmake-demo',
     ('/phy_common.c', '/esp32s31/systimer.c', '/esp_hr_timer.c')),
    ('idf173', 'diagnostics/idf-baseline/build-hal-wifi',
     ('/phy_common.c', '/esp32s31/systimer.c', '/esp_timer_impl_systimer.c')),
):
    build = ROOT / relative
    binary = build / ('nuttx' if label == 'nuttx635' else 's31_wifi_baseline.elf')
    print(label, 'ELF_SHA256=' + hashlib.sha256(binary.read_bytes()).hexdigest())
    entries = json.loads((build / 'compile_commands.json').read_text())
    for suffix in suffixes:
        matches = [e for e in entries if e['file'].endswith(suffix)]
        assert len(matches) == 1, (label, suffix, len(matches))
        entry = matches[0]
        original = iter(shlex.split(entry['command']))
        command = []
        for arg in original:
            if arg in ('-o', '-MF', '-MT', '-MQ'):
                next(original)
            elif arg not in ('-c', '-MD', '-MMD', '-MP'):
                command.append(arg)
        if label == 'nuttx635':
            command[1:1] = ['-include', str(build / 'include/nuttx/config.h')]
        for mode in ('macros', 'source'):
            args = command + (['-E', '-dM'] if mode == 'macros' else ['-E', '-P'])
            print('COMMAND=' + shlex.join(args), flush=True)
            result = subprocess.run(args, cwd=entry['directory'], text=True,
                                    capture_output=True, check=True)
            print('PREPROCESSED_SHA256=' + hashlib.sha256(result.stdout.encode()).hexdigest())
            if result.stderr:
                print(result.stderr)
            for line in result.stdout.splitlines():
                if mode == 'macros' and PATTERN.search(line):
                    print(line)
            if mode == 'source' and suffix.endswith('/systimer.c'):
                for name in ('systimer_ticks_to_us', 'systimer_us_to_ticks'):
                    match = re.search(r'uint64_t\s+' + name + r'\([^;]*?\)\s*\{[^}]*\}', result.stdout)
                    assert match, name
                    print(match[0])
print('TIMER_PREPROCESS_AUDIT=PASS (extraction only, not runtime acceptance)')
