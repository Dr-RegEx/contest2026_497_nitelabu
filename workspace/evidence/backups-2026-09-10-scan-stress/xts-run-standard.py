"""Run unchanged published cmocka commands; UART transport, no test filtering.

Logs raw output live. Verdict is checked against actual cmocka summary and
per-case results, not main()'s unconditional zero return. No firmware writes.
"""
import argparse
from pathlib import Path
import re
import sys
import time

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_production_smoke as production

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('case', choices=['mm', 'sched', 'syscall', 'getprime', 'scanftest', 'hello', 'fstest', 'ramtest', 'helloxx', 'cxxtest', 'pipe', 'popen', 'md5_test', 'rtc', 'timer', 'ostest'])
parser.add_argument('--no-boot', action='store_true')
parser.add_argument('--width', choices=['w', 'h', 'b'], default='w')
args = parser.parse_args()
is_cmocka = args.case in ('mm', 'sched', 'syscall', 'rtc', 'timer')
command = f'cmocka_{args.case}_test' if is_cmocka else args.case
if args.case == 'rtc':
    command = 'cmocka_driver_rtc'
if args.case == 'timer':
    command = 'cmocka_driver_timer -d /dev/timer0'
if args.case == 'fstest':
    command = 'fstest -n 10 -m /tmp'
if args.case == 'md5_test':
    command = 'md5_test -f /etc/1.txt -c 100'
with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.1,
                   write_timeout=1, exclusive=True) as port:
    if not args.no_boot:
        production.boot(port, 'XTS_BOOT')
    else:
        production.command(port, 'uname -a')
    production.command(port, 'ifdown wlan0')
    production.command(port, 'ls /system/bin/' + command.split()[0])
    free_output = production.command(port, 'free')
    if args.case in ('rtc', 'timer'):
        production.command(port, 'ls /dev')
    if args.case == 'pipe':
        for path in ('/var/testfifo-1', '/var/testfifo-2'):
            listing = production.command(port, 'ls ' + path)
            production.require('No such file' in listing,
                               'existing FIFO: inspect before standard test')
    if args.case == 'md5_test':
        listing = production.command(port, 'ls /etc')
        production.require('No such file' in listing,
                           'existing /etc: inspect before creating volatile fixture')
        production.command(port, 'mkdir /etc')
        production.command(port, 'mount -t tmpfs /etc')
        mounts = production.command(port, 'mount')
        production.require('/etc type tmpfs' in mounts,
                           'MD5 fixture must be volatile, not overwrite system files')
        production.command(port, 'echo S31-xTS-read-only-MD5-fixture > /etc/1.txt')
        fixture = production.command(port, 'cat /etc/1.txt')
        production.require('S31-xTS-read-only-MD5-fixture' in fixture,
                           'MD5 fixture missing')
    if args.case == 'ramtest':
        heap_rows = [line.split() for line in free_output.splitlines()
                     if line.strip().endswith(' Kmem')]
        production.require(len(heap_rows) == 1, 'largest free block missing')
        size = int(heap_rows[0][4]) & ~3
        production.require(0 < size <= 512 * 1024, 'unexpected RAM test size')
        command = f'ramtest -{args.width} -s {size}'
        print('XTS_RAM_SIZE_SOURCE=free Kmem maxfree; test allocates its own user-heap buffer', flush=True)
    if args.case == 'fstest':
        mounts = production.command(port, 'mount')
        production.require('/tmp type tmpfs' in mounts,
                           'fstest must use volatile tmpfs')
    # Standard syscall suite confines its normal cleanup to this test tree.
    # Require a fresh volatile path before allowing it to remove that tree.
    if args.case == 'syscall':
        mounts = production.command(port, 'mount')
        production.require('/tmp type tmpfs' in mounts,
                           'syscall scratch filesystem must be volatile tmpfs')
        listing = production.command(port, 'ls /tmp/CM_syscall_testdir')
        production.require('No such file' in listing, 'existing syscall test directory: inspect before reuse')
        for path in ('/fcntl05_fifo', '/Lstat01_tst_syml', '/Symlink02_file',
                     '/var/Test_Fifo_SyscallFsync02'):
            listing = production.command(port, 'ls ' + path)
            production.require('No such file' in listing,
                               'existing standard-test path: inspect before reuse')
    print('XTS_COMMAND=' + command, flush=True)
    port.reset_input_buffer()
    for byte in command.encode() + b'\r\n':
        port.write(bytes([byte]))
        time.sleep(0.01)
    started = time.monotonic()
    output = bytearray()
    while time.monotonic() - started < 1200:
        data = port.read(4096)
        if data:
            output.extend(data)
            print(data.decode(errors='replace'), end='', flush=True)
            text = output.decode(errors='replace')
            if b'nsh> ' in output and (not is_cmocka or re.search(r'\[==========\].*test\(s\) run\.', text)):
                break
            if any(marker in output for marker in
                   (b'Assertion failed', b'S31SM:M-TRAP', b'Segmentation fault',
                    b'riscv_exception: EXCEPTION:')):
                # Preserve the complete target crash dump before closing UART.
                drain_until = time.monotonic() + 5
                while time.monotonic() < drain_until:
                    data = port.read(4096)
                    if data:
                        print(data.decode(errors='replace'), end='', flush=True)
                raise RuntimeError('target fatal failure; log retained')
    else:
        raise TimeoutError('standard test did not finish in 1200 seconds')
    text = output.decode(errors='replace')
    if not is_cmocka:
        production.require('nsh:' not<REDACTED_CREDENTIAL>'Assertion failed' not in text,
                           'command execution failed')
        if args.case == 'hello':
            production.require('Hello, World!!' in text, 'hello output missing')
        elif args.case == 'ostest':
            production.require('user_main: Exiting' in text and
                               'ostest_main: Exiting with status 0' in text and
                               not re.search(r'ERROR|FAILED|Assertion failed', text),
                               'original ostest incomplete or failed')
        elif args.case == 'popen':
            production.require('Calling pclose()' in text and
                               'Calling popen("help")' in text and
                               'ERROR:' not in text,
                               'popen/pclose original test failed')
        elif args.case == 'pipe':
            production.require('redirect_reader: Returning success' in text and
                               'redirect_writer: Returning success' in text and
                               not re.search(r'failed|ERROR:', text, re.I),
                               'pipe original test incomplete or failed')
        elif args.case == 'md5_test':
            import hashlib
            digests = re.findall(r'file /etc/1.txt md5 is ([0-9a-f]{32})', text)
            expected = hashlib.md5(b'S31-xTS-read-only-MD5-fixture\n').hexdigest()
            production.require(len(digests) == 100 and set(digests) == {expected},
                               'expected 100 identical correct MD5 values')
        elif args.case == 'helloxx':
            production.require(text.count('CHelloWorld::HelloWorld: Hello, World!!') == 3 and
                               'CONSTRUCTION FAILED' not in text,
                               'dynamic, stack and static C++ instances must all initialize')
        elif args.case == 'cxxtest':
            production.require(all(marker in text for marker in
                                   ('Test std::vector', 'v1=1 2 3',
                                    's1=Hello, World!', 'Hello World Good Luck',
                                    'Test std::map', 'Test RTTI', 'extend',
                                    'Test C++17 features',
                                    'Catch Exception: runtime error')),
                               'C++ standard test output incomplete')
        elif args.case == 'getprime':
            production.require(re.search(r'getprime took \d+ msec', text) and
                               'finished, found' in text, 'getprime did not finish')
        elif args.case == 'scanftest':
            summary = re.search(r'Scanf tests done\.\.\. OK: (\d+), FAILED: (\d+)', text)
            production.require(summary is not None and int(summary[1]) >= 25 and
                               int(summary[2]) == 0 and 'Test #25 PASSED.' in text,
                               'scanf tests not fully passed')
        elif args.case == 'fstest':
            summary = re.search(r'File system tests done\.\.\. OK: (\d+), FAILED: (\d+)', text)
            loops = re.findall(r'=== FILLING (\d+) ', text)
            production.require(summary is not None and int(summary[1]) == 20 and
                               int(summary[2]) == 0 and
                               loops == [str(n) for n in range(1, 11)] and
                               'ERROR:' not in text,
                               'ten filesystem loops not fully passed')
        elif args.case == 'ramtest':
            production.require(all(marker in text for marker in
                                   ('Marching ones:', 'Marching zeroes:',
                                    'Pattern test:', 'Address-in-address test:')) and
                               'ERROR:' not<REDACTED_CREDENTIAL>'malloc failed' not in text,
                               'RAM pattern tests incomplete or failed')
        print(f'XTS_STANDARD={command} PASS seconds={time.monotonic()-started:.3f}', flush=True)
        sys.exit(0)
    runs = re.findall(r'\[==========\].*?: (\d+) test\(s\) run\.', text)
    passed = re.findall(r'\[  PASSED  \] (\d+) test\(s\)\.', text)
    checks = re.findall(r'\[       OK \] (\S+)', text)
    production.require(len(runs) == len(passed) == 1 and runs == passed and
                       int(runs[0]) > 0 and len(checks) == int(runs[0]) and
                       len(set(checks)) == len(checks) and
                       '[  FAILED  ]' not<REDACTED_CREDENTIAL>'[  SKIPPED ]' not in text,
                       'standard suite not fully passed')
    if args.case == 'mm':
        production.require(int(runs[0]) == 8, 'expected all eight memory cases')
    if args.case == 'sched':
        production.require(int(runs[0]) == 9, 'expected all nine kernel-build scheduler cases')
    if args.case == 'rtc':
        production.require(int(runs[0]) == 3, 'RTC API, alarm and periodic must all run')
    print(f'XTS_STANDARD={command} PASS subcases={runs[0]} seconds={time.monotonic()-started:.3f}', flush=True)
