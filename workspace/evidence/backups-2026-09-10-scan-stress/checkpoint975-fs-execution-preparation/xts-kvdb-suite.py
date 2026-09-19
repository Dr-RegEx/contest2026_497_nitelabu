"""Run original KVDB 4.1.11 once on an already mounted scratch filesystem."""
import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import os
from pathlib import Path
import re
import select
import shlex
import subprocess
import sys
import termios

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_nuttx_smoke as transport


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


@contextmanager
def existing_uart():
    """Use the already configured tty without changing modem/reset lines."""
    fd = os.open('/dev/ttyUSB0', os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        settings = termios.tcgetattr(fd)
        require(settings[4] == settings[5] == termios.B115200 and
                not settings[3] & (termios.ICANON | termios.ECHO),
                'existing UART must already be raw 115200; no reconfiguration')

        class Port:
            @property
            def in_waiting(self):
                return 65536 if select.select([fd], [], [], 0)[0] else 0

            def read(self, size):
                if select.select([fd], [], [], 0.05)[0]:
                    return os.read(fd, size)
                return b''

            def write(self, data):
                require(select.select([], [fd], [], 1)[1], 'UART write timeout')
                return os.write(fd, data)

        yield Port()
    finally:
        os.close(fd)


def check_mount_evidence(evidence, receipt, digest):
    require(digest in evidence or 'XTS_RECEIPT=' + str(receipt.resolve()) in evidence,
            'mount evidence must identify this firmware receipt or digest')
    mounted = False
    listed = False
    if re.search(r'^XTS_COMMAND=', evidence, re.M):
        command_pattern = r'XTS_COMMAND=(.+)'
    elif re.search(r'^--- CMD: ', evidence, re.M):
        command_pattern = r'--- CMD: (.+) ---'
    else:
        command_pattern = r'nsh> (.+)'
    for line in evidence.splitlines():
        line = re.sub(r'^\[CPU\d+\]\s*', '', line.strip())
        if mounted:
            require(not re.search(r'ESP-ROM:|NuttShell \(NSH\)|rst:|hard_reset|'
                                  r'XTS_RESET|PANIC|Assertion failed', line, re.I),
                    'reset/fault after scratch mount in evidence')
            require(not re.search(r'ERROR|failed|nsh:', line, re.I),
                    'failed command after scratch mount in evidence')
            if re.fullmatch(r'/data type littlefs', line):
                listed = True
        match = re.fullmatch(command_pattern, line)
        if not match:
            continue
        cmd = match.group(1)
        try:
            words = shlex.split(cmd)
        except ValueError:
            continue
        if not words:
            continue
        if mounted:
            require(not (words[0] in ('reboot', 'reset', 'poweroff') or
                         words[0] == 'umount' and '/data' in words or
                         words[0] == 'mount' and '/data' in words),
                    'reset/unmount/remount after scratch mount in evidence')
        if words in (['mount', '-t', 'littlefs', '/dev/xtsflash', '/data'],
                     ['mount', '-t', 'littlefs', '-o', 'forceformat',
                      '/dev/xtsflash', '/data']):
            mounted = True
    require(mounted, 'no explicit scratch LittleFS mount in evidence')
    require(listed, 'mount evidence lacks subsequent /data type littlefs listing')


def command(port, cmd, timeout=120):
    print('XTS_COMMAND=' + cmd, flush=True)
    try:
        result = transport.run_command(port, cmd, timeout=timeout,
                                       reset_input=False)
    except TimeoutError as error:
        print(str(error), flush=True)
        raise
    sys.stdout.flush()
    require(not any(marker.decode() in result
                    for marker in transport.BOOT_FAILURE_MARKERS),
            'fatal target error')
    require(not re.search(r'ESP-ROM:|NuttShell \(NSH\)|rst:|PANIC|'
                          r'KASAN:|\[\s*(?:FAILED|SKIPPED)\s*\]|nsh:',
                          result, re.I), 'target reset/failure: ' + cmd)
    if cmd != 'cmocka_kv_test':
        require(not re.search(r'ERROR|failed|timed out', result, re.I),
                'precondition command failed: ' + cmd)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--mount-evidence', type=Path, required=True,
                        help='current boot transcript identifying receipt and scratch mount')
    args = parser.parse_args()
    lines = args.receipt.read_text().splitlines()
    require(len(lines) == 1, 'one FLAT kernel receipt required')
    digest, name = lines[0].split(maxsplit=1)
    firmware = Path(name)
    require(firmware.name == 'nuttx.bin' and
            hashlib.sha256(firmware.read_bytes()).hexdigest() == digest,
            'firmware receipt mismatch')
    config = (firmware.parent / '.config').read_text()
    for option in ('BUILD_FLAT', 'ESP32S31_XTS_FLASH', 'FS_LITTLEFS',
                   'FS_TMPFS', 'NET_LOCAL', 'KVDB', 'KVDB_SERVER',
                   'KVDB_TEMPORARY_STORAGE', 'KVDB_UNQLITE',
                   'TESTS_TESTSUITES', 'CM_KVDB_TEST', 'TESTING_CMOCKA'):
        require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    for option, value in (('KVDB_PERSIST_PATH', '/data/persist.db'),
                          ('KVDB_TEMPORARY_PATH', '/tmp/db'),
                          ('KVDB_LOAD_TEST_SOURCE_PATH', '/data/test.prop'),
                          ('LIBC_TMPDIR', '/tmp')):
        require(f'CONFIG_{option}="{value}"\n' in config, 'wrong ' + option)
    evidence = args.mount_evidence.read_text()
    check_mount_evidence(evidence, args.receipt, digest)
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(),
            'UART occupied or occupancy check failed; preserve active longrun')
    print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
    print('XTS_MOUNT_EVIDENCE=' + str(args.mount_evidence.resolve()), flush=True)
    print('XTS_MOUNT_EVIDENCE_SHA256=' +
          hashlib.sha256(args.mount_evidence.read_bytes()).hexdigest(), flush=True)
    # Opening a serial device may affect reset lines. Never recover/reset here:
    # any boot output or missing pre-existing mount causes an immediate refusal.
    with existing_uart() as port:
        mounts = command(port, 'mount')
        entries = re.findall(r'^\s*(?:\[CPU\d+\]\s*)?(/\S*) type (\S+)\s*$',
                             mounts, re.M)
        require(('/data', 'littlefs') in entries and ('/tmp', 'tmpfs') in entries,
                'required existing scratch /data or /tmp mount missing; no recovery')
        require(not any(path == '/apps' or path.startswith('/apps/')
                        for path, _ in entries), 'production /apps is mounted')
        command(port, 'ls /dev/xtsflash')
        tasks = command(port, 'ps')
        require(not re.search(r'\bkvdbd\b', tasks), 'kvdbd already running')
        command(port, 'kvdbd &')
        tasks = command(port, 'ps')
        require(re.search(r'\bkvdbd\b', tasks), 'kvdbd did not remain running')
        result = command(port, 'cmocka_kv_test', timeout=600)
        expected = [f'test_nuttx_kv{n:02d}' for n in range(1, 31)]
        runs = re.findall(r'\[\s*RUN\s*\]\s+(\S+)', result)
        passes = re.findall(r'\[\s*OK\s*\]\s+(\S+)', result)
        totals = re.findall(r'\[\s*PASSED\s*\]\s+(\d+) test\(s\)\.', result)
        require(runs == expected and passes == expected and totals == ['30'],
                'original 30-case execution/pass evidence incomplete')
        print('XTS_4.1.11_KVDB=PASS original_cases=30 runs=1', flush=True)


if __name__ == '__main__':
    main()
