"""Generate a scoped index patch, preserving unrelated diagnostic edits."""
import difflib
from pathlib import Path
import subprocess
import sys

root = Path('/home/regex/work/esp32s31-openvela/openvela-dev/nuttx')
sys.path.insert(0, str(root / 'tools'))
from test_esp_hr_timer_lifecycle import definition

name = 'arch/risc-v/src/esp32c6/esp_wifi_adapter.c'
before = subprocess.check_output(['git', 'show', 'HEAD:' + name], cwd=root, text=True)
work = (root / name).read_text()
after = before
for function in ('event_group_create_wrapper', 'event_group_delete_wrapper',
                 'event_group_set_bits_wrapper', 'event_group_clear_bits_wrapper',
                 'event_group_wait_bits_wrapper'):
    old, new = definition(before, function), definition(work, function)
    assert old != new and after.count(old) == 1
    after = after.replace(old, new, 1)
include = '#include "esp32s31_wifi_task.h"\n'
replacement = include + '#ifdef CONFIG_BUILD_KERNEL\n#include "esp32s31_wifi_event.h"\n#endif\n'
assert before.count(include) == 1 and replacement in work
after = after.replace(include, replacement, 1)
sys.stdout.writelines(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                        fromfile='a/' + name, tofile='b/' + name))
