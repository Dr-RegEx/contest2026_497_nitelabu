#!/usr/bin/env python3
"""Compile the complete S31 bridge against host stubs; never accesses a board."""
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
source = root / 'openvela-dev/nuttx/arch/risc-v/src/esp32s31/esp32s31_ble.c'
body = '\n'.join(line for line in source.read_text().splitlines()
                 if not line.lstrip().startswith(('#include ', '#  include ')))
with tempfile.TemporaryDirectory(prefix='s31-iso-test-') as tmp:
    test = Path(tmp) / 'test.c'
    test.write_text((here / 'stubs.h').read_text() + '\n' + body + '\n' +
                    (here / 'cases.c').read_text())
    for mode in ('flat', 'kernel'):
        exe = Path(tmp) / mode
        subprocess.run(['cc', '-std=c11', '-g', '-fsanitize=address,undefined',
                        '-Werror=implicit-function-declaration',
                        *(['-DCONFIG_BUILD_KERNEL=1'] if mode == 'kernel' else []),
                        str(test), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
        print(f'{mode}: complete bridge host tests passed')
    config = root / 'openvela-dev/nuttx/arch/risc-v/src/esp32s31/include/esp32s31_bt_config.h'
    common = f'#include "{config}"\n'
    snippets = {
        'disabled': common + '#ifdef CONFIG_BT_LE_ISO_SUPPORT\n#error unexpected ISO\n#endif\n',
        'enabled': '#define CONFIG_BT_ISO 1\n#define CONFIG_BT_ISO_MAX_CHAN 2\n'
                   '#define CONFIG_BT_ISO_MAX_CIG 2\n#define CONFIG_BT_ISO_MAX_BIG 1\n' + common +
                   '_Static_assert(CONFIG_BT_LE_ISO_SUPPORT == 1, "ISO");\n'
                   '_Static_assert(CONFIG_BT_LE_ISO_CIS == 2, "CIS");\n'
                   '_Static_assert(CONFIG_BT_LE_ISO_BIS == 2, "BIS");\n'
                   '_Static_assert(CONFIG_BT_LE_ISO_CIG == 2, "CIG");\n'
                   '_Static_assert(CONFIG_BT_LE_ISO_BIG == 1, "BIG");\n'
                   '_Static_assert(CONFIG_BT_LE_ISO_BUF_COUNT == 12, "buffers");\n'
                   '#ifdef CONFIG_BT_LE_ISO_NSFC_EN\n#error private flow control\n#endif\n',
        'invalid': '#define CONFIG_BT_ISO 1\n#define CONFIG_BT_ISO_MAX_CHAN 7\n' + common,
    }
    for name, snippet in snippets.items():
        result = subprocess.run(['cc', '-x', 'c', '-fsyntax-only', '-'], input=snippet,
                                capture_output=True, text=True)
        assert (result.returncode != 0) == (name == 'invalid'), result.stderr
        print(f'controller config {name}: expected result confirmed')
