"""Run original ROMFS and FAT UTF8 category cases on disposable RAM storage."""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_nuttx_smoke as transport


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def command(port, cmd, timeout=120):
    print('XTS_COMMAND=' + cmd, flush=True)
    result = transport.run_command(port, cmd, timeout=timeout)
    print(result, flush=True)
    require(not any(x.decode() in result for x in transport.BOOT_FAILURE_MARKERS),
            'fatal target error')
    require(not re.search(r'ERROR|failed|nsh:|timed out', result, re.I),
            'command failed: ' + cmd)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    lines = args.receipt.read_text().splitlines()
    require(len(lines) == 1, 'one FLAT kernel receipt required')
    digest, name = lines[0].split(maxsplit=1)
    firmware = Path(name)
    require(firmware.name == 'nuttx.bin' and
            hashlib.sha256(firmware.read_bytes()).hexdigest() == digest,
            'firmware receipt mismatch')
    config = (firmware.parent / '.config').read_text()
    for option in ('BUILD_FLAT', 'EXAMPLES_ROMFS', 'TESTING_FATUTF8', 'FAT_LFN_UTF8'):
        require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(),
            'UART occupied or occupancy check failed; preserve active longrun')
    print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.05,
                       write_timeout=1, exclusive=True) as port:
        transport.hard_reset(port)
        boot = transport.collect_until_prompt(port, 30)
        print(boot.decode(errors='replace'), flush=True)
        require(b'NuttShell (NSH)' in boot and not any(
            x in boot for x in transport.BOOT_FAILURE_MARKERS), 'abnormal boot')
        mounts = command(port, 'mount')
        require('/data ' not in mounts and '/apps ' not in mounts,
                'expected isolated fresh filesystem image')
        result = command(port, 'romfs')
        require(re.search(r'(?m)^(?:\[CPU\d+\]\s*)?PASSED\s*$', result),
                'original ROMFS result missing')
        print('XTS_4.1.2_ROMFS=PASS', flush=True)
        # Minor11 avoids the original ROMFS example's minor10.
        command(port, 'mkrd -m 11 -s 512 2048')
        command(port, 'mkfatfs /dev/ram11')
        command(port, 'mkdir /data')
        command(port, 'mount -t vfat /dev/ram11 /data')
        result = command(port, 'fatutf8 /data')
        require('write successful' in result and 'read("This is a test file' in result
                and len(re.findall(r'(?m)^.*removed /data/', result)) == 2,
                'original UTF8 read/write/remove sequence incomplete')
        print('XTS_4.1.3_FATUTF8=PASS', flush=True)
        command(port, 'umount /data')
        command(port, 'free')
        print('XTS_CATEGORY_FS_COMPLETE=romfs,fatutf8', flush=True)


if __name__ == '__main__':
    main()
