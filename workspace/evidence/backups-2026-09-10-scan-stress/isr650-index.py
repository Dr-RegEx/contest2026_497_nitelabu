"""Emit only the ISR queue wake-flag change for selective staging."""
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
function = 'queue_send_from_isr_wrapper'
old, new = definition(before, function), definition(work, function)
assert old != new and before.count(old) == 1
after = before.replace(old, new, 1)
sys.stdout.writelines(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                        fromfile='a/' + name, tofile='b/' + name))
