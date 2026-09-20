"""Run one original FS recovery mode on existing1032 LittleFS; never format.

18-reboot and18-crash require software-triggered reset.19 resets after15s.
Test04 includes the documented reader safety fix; writer workload is unchanged.
"""
import argparse
from contextlib import redirect_stdout, redirect_stderr
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import time

D = Path(__file__).resolve().parent
DIGEST = '7b0e0dd869e157a1620297412caba0d99b5e6b068fff0e3a00bf05ceb7ced47a'
BOOT = 'xTS Flash scratch: /dev/xtsflash offset=0xd00000 size=0x300000'


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, D / file)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def file_size(listing, name):
    sizes = re.findall(r'^\s*-[rwx-]{9}\s+(\d+)\s+(?:\S*/)?' + re.escape(name) + r'\s*$', listing, re.M)
    require(len(sizes) == 1 and int(sizes[0]) > 0, 'missing/empty recovered file: ' + name)
    return int(sizes[0])


def send(port, command):
    print('\nXTS_COMMAND=' + command, flush=True)
    payload = command.encode() + b'\r\n'
    require(port.write(payload) == len(payload), 'incomplete UART command')
    port.flush()
    return time.monotonic()


def collect(port, deadline, stop=None):
    data = ''
    while time.monotonic() < deadline:
        chunk = port.read(port.in_waiting or 1).decode(errors='replace')
        if chunk:
            print(chunk, end='', flush=True)
            data += chunk
            if stop and stop(data):
                return data
    if stop:
        raise RuntimeError('expected automatic reset/boot did not complete; no host fallback')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=['18-reboot', '18-crash', '19'], required=True)
    parser.add_argument('--receipt', type=Path, default=D / 'build1032-flat-category-fs-large.sha256')
    parser.add_argument('--mount-evidence', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    digest, name = args.receipt.read_text().strip().split(maxsplit=1)
    image = Path(name)
    require(digest == DIGEST and hashlib.sha256(image.read_bytes()).hexdigest() == DIGEST,
            'exact1032 image required')
    config = (image.parent / '.config').read_text()
    for opt in ['BUILD_FLAT', 'ESP32S31_XTS_FLASH_LARGE', 'FS_LITTLEFS', 'FS_TEST_STABILITY', 'BOARDCTL_RESET']:
        require(f'CONFIG_{opt}=y\n' in config, 'missing ' + opt)
    require('CONFIG_BOARD_RESET_ON_ASSERT=2\n' in config and
            'CONFIG_TESTING_TESTCASES_STACKSIZE=32768\n' in config, 'recovery configuration mismatch')
    kv = load('kv_recovery', 'xts-kvdb-suite.py')
    prep = load('prep_recovery', 'run1012-kv-first-mount.py')
    evidence = args.mount_evidence.read_text()
    kv.check_mount_evidence(evidence, args.receipt, DIGEST)
    require(BOOT in evidence, 'current3MiB boot evidence required')
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(), 'UART occupied or fuser failed')
    import serial
    args.output.mkdir(parents=True, exist_ok=False)
    path = '/data/' + {'18-reboot': 's18r', '18-crash': 's18c', '19': 's19'}[args.case]
    command = ('vela_fs_stability_test04 ' + path if args.case == '19' else
               'vela_fs_stability_test03 mode=' + args.case[3:] + ' ' + path)
    with (args.output / 'uart.log').open('x') as log, redirect_stdout(prep.Tee(sys.stdout, log)), redirect_stderr(prep.Tee(sys.stderr, log)):
        print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
        print('IMAGE_SHA256=' + DIGEST, flush=True)
        print('XTS_CASE=' + args.case, flush=True)
        port = serial.Serial(port=None, baudrate=115200, timeout=.05, write_timeout=1, exclusive=True)
        port.dtr = False
        port.rts = False
        port.port = '/dev/ttyUSB0'
        with port:
            def cmd(s, timeout=120):
                print('', flush=True)
                return kv.command(port, s, timeout)
            mounts = cmd('mount')
            require(re.search(r'^\s*/data type littlefs\s*$', mounts, re.M), 'existing mount lost; no reset recovery')
            require(not re.search(r'^\s*/apps(?:/\S*)? type ', mounts, re.M), 'production mount present')
            require(not re.search(r'\bkvdbd\b|vela_fs_', cmd('ps')), 'workload already active')
            cmd('ls /dev/xtsflash')
            cmd('df')
            cmd('mkdir ' + path)  # refuse existing directory, never delete/reuse
            started = send(port, command)
            if args.case == '19':
                writing = collect(port, started + 15)
                require('threadroutine_2' in writing and 'threadroutine_3' in writing and
                        all(re.search(r'\[threadroutine_' + str(n) + r'\] \d+th write test file, total write [1-9]\d* bytes', writing) for n in (2, 3)),
                        'both writing threads not observed; no host reset performed')
                require(not re.search(r'Simulate system crash|ESP-ROM:|NuttShell|\b(?:fail|error)\b', writing, re.I), 'unexpected reset/error before15s')
                elapsed = time.monotonic() - started
                require(10 <= elapsed <= 20, 'original reset window missed')
                print(f'\nXTS_RESET=host_controlled elapsed_seconds={elapsed:.3f}', flush=True)
                # Do not discard pending writer output when asserting reset.
                port.dtr = False
                port.rts = True
                time.sleep(.2)
                port.rts = False
                reboot = collect(port, time.monotonic() + 60,
                                 lambda s: BOOT in s and 'NuttShell (NSH)' in s and 'nsh> ' in s)
            else:
                label = 'Simulate device reboot ...' if args.case == '18-reboot' else 'Simulate system crash ...'
                writing = collect(port, started + 600,
                                  lambda s: label in s and BOOT in s[s.index(label):] and 'NuttShell (NSH)' in s[s.index(label):] and 'nsh> ' in s[s.index(label):])
                before, reboot = writing.split(label, 1)
                rounds = re.findall(r'NO\.(\d+) write to the filesystem', before)
                require(rounds and rounds == [str(i) for i in range(1, len(rounds) + 1)] and 402 <= len(rounds) <= 901,
                        'original write workload evidence incomplete')
                require(not re.search(r'\b(?:fail|failed|error)\b|bad crc|bad info', before, re.I), 'write phase failed')
                # Expected deliberate trap is allowed only between trigger and boot.
            freshboot = reboot[reboot.rfind('ESP-ROM:'):] if 'ESP-ROM:' in reboot else reboot[reboot.index(BOOT):]
            require(BOOT in freshboot and not any(m.decode() in freshboot for m in kv.transport.BOOT_FAILURE_MARKERS), 'recovery boot failed')
            print('\nXTS_RECOVERY_MOUNT_BEGIN', flush=True)
            print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
            print('IMAGE_SHA256=' + DIGEST, flush=True)
            print(BOOT, flush=True)
            mounts = cmd('mount')
            require(not re.search(r'^\s*/(?:data|apps)(?:/\S*)? type ', mounts, re.M), 'unexpected recovered mount')
            cmd('mkdir -p /data')
            cmd('mount -t littlefs /dev/xtsflash /data')
            cmd('mount')
            log.flush()
            current = (args.output / 'uart.log').read_text().split('XTS_RECOVERY_MOUNT_BEGIN\n', 1)[1]
            kv.check_mount_evidence(current, args.receipt, DIGEST)
            (args.output / 'mount-evidence.log').write_text(current)
            listing = cmd('ls -l ' + path)
            if args.case != '19':
                size = file_size(listing, 'stability_test03_file')
                require(size % 1036 == 0, 'partial1036-byte CRC record; refusing checker')
                print(f'XTS_RECOVERED_FILE_BYTES={size} records={size // 1036}', flush=True)
            else:
                for n in (1, 2):
                    size = file_size(listing, 'stability_test04_file_' + str(n))
                    print(f'XTS_RECOVERED_FILE_{n}_BYTES={size}', flush=True)
                print('XTS_TEST_SOURCE=test04_reader_safety_patch writer_unchanged', flush=True)
            result = cmd(command, 180)
            if args.case != '19':
                require(all(x in result for x in ['file is exit !', 'check crc OK !', 'TEST PASS !']), 'CRC checker did not pass')
                require('NO.1 write' not in result and not re.search(r'bad crc|bad info|\bFAIL\b', result), 'unexpected writer or invalid record')
            else:
                require('TEST PASSED !' in result and all('stability_test04_file_' + str(n) in result for n in (1, 2)), 'both file checks not evidenced')
            cmd('df')
            print('XTS_RECOVERY=' + args.case + ' PASS single_original_run', flush=True)
    (args.output / 'result.json').write_text(json.dumps({'case': args.case, 'status': 'PASS', 'image_sha256': DIGEST,
        'host_reset': args.case == '19', 'formatted': False, 'test04_reader_patched': args.case == '19'}, indent=2) + '\n')


if __name__ == '__main__':
    main()
