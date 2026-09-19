"""Reject Kconfig/header/protocol mismatches before recording or flashing."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('--objdump', default='riscv32-esp-elf-objdump')
args = parser.parse_args()
config = (args.build / '.config').read_text()
header = (args.build / 'include/nuttx/config.h').read_text()
enabled = set(re.findall(r'(?m)^#define (CONFIG_\w+)\s+1\s*$', header))
disabled = set(re.findall(r'(?m)^# (CONFIG_\w+) is not set$', config))
selected = set(re.findall(r'(?m)^(CONFIG_\w+)=y$', config))
assert not selected - enabled, f'Missing header booleans: {sorted(selected - enabled)}'
assert not disabled & enabled, f'Stale header booleans: {sorted(disabled & enabled)}'
assert {'CONFIG_ARCH_CHIP_ESP32S31', 'CONFIG_ESPRESSIF_ESP32S31'} <= selected
expected = 71 if 'CONFIG_ESPRESSIF_WIFI_STA_11AX' in selected else 7
assembly = subprocess.check_output(
    [args.objdump, '-d', '--disassemble=esp_wifi_sta_start', str(args.build / 'nuttx')],
    text=True)
match = re.search(r'\bli\s+a1,(\d+)\s*\n[^\n]*\b(?:jal|call)\b[^\n]*'
                  r'<esp_wifi_set_protocol>', assembly)
assert match, 'Cannot verify protocol argument in the actual ELF'
assert int(match[1]) == expected, f'ELF protocol={match[1]}, config protocol={expected}'
print(f'WIFI_ARTIFACT_CONFIG=PASS booleans={len(selected) + len(disabled)} protocol={expected}')
