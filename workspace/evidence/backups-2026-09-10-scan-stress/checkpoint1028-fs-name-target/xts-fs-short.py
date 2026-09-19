"""Run one original short filesystem case on already mounted scratch LittleFS.

No reset, format, remount, deletion, network operation or automatic retry.
Each case uses a fresh short subdirectory; original workload counts are retained.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import re
import subprocess


HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv_transport', HERE / 'xts-kvdb-suite.py')
kv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kv)
require = kv.require

CASES = {
    '5.1.3': ('s03', 'vela_fs_stress_loop_create_delete_file_test {path}',
              r'\bTEST PASSED\s*!', 1800),
    '5.1.4': ('s04', 'vela_fs_stress_maximum_file_name_test {path}',
              r'\bTEST PASSED\s*!', 600),
    '5.1.5': ('s05', 'vela_fs_stress_multi_thread_file_operate_test -d {path}',
              r'\bTEST PASSED\s*!', 600),
    '5.1.6': ('s06', 'vela_fs_stress_read_and_write_loops_test {path}',
              r'\bTEST PASSED\s*!', 600),
    '5.1.8': ('s08', 'vela_fs_multi_thread_write_test -d {path} -l 1000',
              r'\bTEST PASS!', 600),
    '5.1.9': ('s09', 'vela_fs_multi_thread_read_test -p {path} -l 1000 -t 3',
              r'\bTEST PASS!', 600),
    '5.1.10': ('s10', 'vela_fs_multi_thread_read_write_test -d {path}',
               r'\bTEST PASS\s*$', 1800),
}


def command(port, cmd, timeout=120):
    result = kv.command(port, cmd, timeout)
    require(not re.search(r'\b(?:fail|mismatch)\b|AddressSanitizer|kasan_report:',
                          result, re.I), 'filesystem failure diagnostic: ' + cmd)
    return result


def check_result(case, output):
    require(not re.search(r'\b(?:error|fail|failed|mismatch)\b|nsh:|'
                          r'Assertion failed|kasan_report:', output, re.I),
            'original case emitted an error')
    require(len(re.findall(CASES[case][2], output, re.M)) == 1,
            'missing or duplicated original success result')
    if case == '5.1.3':
        iterations = re.findall(r'\bcreating no\.(\d+) file', output)
        require(iterations == [str(n) for n in range(1, 101)],
                'original 100 creation iterations not observed')
        for operation in ('create', 'write', 'remove'):
            require(len(re.findall(operation + r' file success\s*!', output)) == 100,
                    'original 100 ' + operation + ' operations not observed')
    elif case == '5.1.4':
        require('The max name length: 32' in output and 'File path: ' in output,
                'maximum-name boundary execution evidence missing')
    elif case == '5.1.5':
        ids = re.findall(r'Thread ID: (\d+), writing content: Thread\d+Data', output)
        require(sorted(ids) == ['1', '2', '3', '4', '5'],
                'original five threads not observed')
    elif case == '5.1.6':
        for verb in ('write', 'read'):
            iterations = re.findall(verb + r' file no\.(\d+)', output)
            require(iterations == [str(n) for n in range(10)],
                    'original ten iterations not observed; check free capacity')
    elif case == '5.1.10':
        for thread in range(5):
            require(re.search(r'/test' + str(thread) + r': Remaining count 0\b', output),
                    'original thread did not reach its final iteration')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--mount-evidence', type=Path, required=True)
    parser.add_argument('--case', choices=CASES, required=True)
    parser.add_argument('--directory', choices=['s04r'],
                        help='fresh directory for the corrected 5.1.4 rerun only')
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
                   'TESTS_TESTCASES', 'FS_TEST', 'FS_TEST_STRESS'):
        require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    if args.case == '5.1.3':
        require('CONFIG_ESPRESSIF_SPIRAM_USER_HEAP=y\n' not in config and
                'CONFIG_MM_REGIONS=1\n' in config and
                'CONFIG_RAM_SIZE=524288\n' in config,
                '5.1.3 requires the disclosed internal SRAM memory profile')
    if args.case == '5.1.4':
        for option, value in (('NAME_MAX', 32), ('PATH_MAX', 256),
                              ('FS_LITTLEFS_NAME_MAX', 32)):
            require(f'CONFIG_{option}={value}\n' in config,
                    'unexpected boundary configuration: ' + option)
    kv.check_mount_evidence(args.mount_evidence.read_text(), args.receipt, digest)
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(),
            'UART occupied or occupancy check failed; preserve active longrun')
    print('XTS_RECEIPT=' + str(args.receipt.resolve()), flush=True)
    print('XTS_MOUNT_EVIDENCE=' + str(args.mount_evidence.resolve()), flush=True)
    print('XTS_MOUNT_EVIDENCE_SHA256=' +
          hashlib.sha256(args.mount_evidence.read_bytes()).hexdigest(), flush=True)
    if args.case == '5.1.3':
        print('XTS_MEMORY_PROFILE=internal_sram_only no_psram_heap_claim', flush=True)
    suffix, template, _, timeout = CASES[args.case]
    if args.directory:
        require(args.case == '5.1.4', 'alternate directory is only for 5.1.4')
        suffix = args.directory
    path = '/data/' + suffix
    # test.h setup() has a 20-byte buffer including the appended /testDir.
    require(len(path + '/testDir') < 20, 'test helper path buffer too short')
    with kv.existing_uart() as port:
        mounts = command(port, 'mount')
        entries = re.findall(r'^\s*(?:\[CPU\d+\]\s*)?(/\S*) type (\S+)\s*$',
                             mounts, re.M)
        require(('/data', 'littlefs') in entries, 'scratch mount missing')
        require(not any(p == '/apps' or p.startswith('/apps/') or
                        p.startswith('/data/') for p, _ in entries),
                'production or nested mount present')
        command(port, 'ls /dev/xtsflash')
        tasks = command(port, 'ps')
        require(not re.search(r'\bkvdbd\b|vela_fs_', tasks),
                'another database/filesystem workload is active')
        command(port, 'df')
        command(port, 'ls -l /data')
        # mkdir must fail if the directory already exists. Never delete or
        # reuse leftovers, including a previous successful case directory.
        command(port, 'mkdir ' + path)
        result = command(port, template.format(path=path), timeout)
        check_result(args.case, result)
        print('XTS_CASE=' + args.case + ' PASS original_workload=1', flush=True)
        command(port, 'ls -l ' + path)
        command(port, 'df')
        print('XTS_CASE_POSTCHECK=COMPLETE retained_directory=' + path, flush=True)


if __name__ == '__main__':
    main()
