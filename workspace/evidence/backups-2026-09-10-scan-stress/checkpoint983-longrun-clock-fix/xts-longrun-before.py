"""Published 12h standby and 24h date comparison, one uninterrupted board run.

Uses the existing showinfo resource monitor and NSH date command. No network
association, filesystem workload, test shortening, or automatic reset/retry.
The host must remain awake and USB attached for the complete observation.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sys
import time
import traceback

import serial

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/nuttx/tools/espressif'))
import esp32s31_nuttx_smoke as transport

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--receipt', type=Path, required=True)
parser.add_argument('--tag', required=True)
args = parser.parse_args()
if not re.fullmatch(r'xts\d+[a-z]?', args.tag):
    parser.error('use a fresh numbered evidence tag')

directory = ROOT / 'backups/2026-09-10-scan-stress'
state_path = directory / (args.tag + '-longrun.json')
raw_path = directory / 'logs' / (args.tag + '-longrun-uart.log')
state = {'standby': 'NOT_STARTED', 'time_consistency': 'NOT_STARTED',
         'receipt': str(args.receipt), 'samples': [],
         'monitor': 'showinfo -i 60', 'radio': 'unassociated'}


def save():
    state['updated_utc'] = datetime.now(timezone.utc).isoformat()
    temporary = state_path.with_suffix('.tmp')
    temporary.write_text(json.dumps(state, indent=2) + '\n')
    temporary.replace(state_path)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


require(not state_path.exists() and not raw_path.exists(), 'evidence tag exists')
receipt = args.receipt.read_text().splitlines()
require(len(receipt) == 2, 'paired kernel/AppFS receipt required')
for line in receipt:
    digest, name = line.split(maxsplit=1)
    require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest,
            'firmware digest mismatch: ' + name)
build = Path(receipt[0].split(maxsplit=1)[1]).parent
config = (build / '.config').read_text()
for option in ('SMP', 'BUILD_KERNEL', 'MM_KASAN', 'MM_KASAN_INSTRUMENT_ALL',
               'SYSTEM_RESMONITOR', 'SCHED_CPULOAD_SYSCLK'):
    require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
require('CONFIG_SYSTEM_NTPC=y\n' not in config, 'NTP must not run')
require('CONFIG_FS_HEAPSIZE=0\n' in config,
        'standby requires internal allocator shadow metadata')
state['config_sha256'] = hashlib.sha256(config.encode()).hexdigest()

with raw_path.open('x', buffering=1) as raw:
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.2,
                       write_timeout=1, exclusive=True) as port:
        rolling = ''
        started = None
        last_monitor = time.monotonic()

        def read():
            global rolling, last_monitor
            data = port.read(port.in_waiting or 1)
            if not data:
                return ''
            text = data.decode(errors='replace')
            raw.write(text)
            rolling = (rolling + text)[-8192:]
            if started is not None:
                require('ESP-ROM:' not in rolling, 'unexpected target reset')
                require(not re.search(
                    r'Assertion failed|S31SM:M-TRAP|Segmentation fault|'
                    r'kasan_report:|kasan_panic:|kasan detected|AddressSanitizer|'
                    r'\bERROR\b|get total info fail|program complete!', rolling),
                    'target fault or resource monitor stopped')
            if re.search(r'\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+'
                         r'\d+\s+\d+\s+[\d.]+%', rolling):
                last_monitor = time.monotonic()
                rolling = rolling[-512:]
            return text

        def prompt(timeout=30):
            output = ''
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                output += read()
                if 'nsh> ' in output:
                    require(not re.search(r'nsh:|ERROR|Assertion failed', output),
                            'command/boot error')
                    return output
            raise TimeoutError('NSH prompt missing; board not reset')

        def command(text):
            require(len(text) <= 63, 'NSH command too long')
            raw.write('\nHOST_COMMAND ' + datetime.now(timezone.utc).isoformat()
                      + ' ' + text + '\n')
            for byte in text.encode():
                port.write(bytes([byte]))
                time.sleep(0.01)
            before = time.time()
            port.write(b'\r')
            output = prompt()
            return output, before, time.time()

        def sample(index):
            output, before, after = command('date -u +%Y-%m-%dT%H:%M:%S')
            dates = re.findall(r'(?m)^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d)\r?$',
                               output)
            require(len(dates) == 1, 'one UTC date expected')
            epoch = datetime.strptime(dates[0], '%Y-%m-%dT%H:%M:%S').replace(
                tzinfo=timezone.utc).timestamp()
            # Date is quantized to seconds and generated between host send
            # and receive. Retain the complete conservative error interval.
            bounds = [epoch - after, epoch + 1 - before]
            item = {'index': index, 'board_utc': dates[0],
                    'host_before': before, 'host_after': after,
                    'error_seconds_interval': bounds,
                    'elapsed_seconds': time.monotonic() - started}
            state['samples'].append(item)
            print(json.dumps(item), flush=True)
            save()
            command('free')
            return max(abs(value) for value in bounds) <= 2

        try:
            transport.hard_reset(port)
            boot = prompt()
            require('CPU1 SMP online' in boot and 'NuttShell (NSH)' in boot,
                    'SMP/NSH boot missing')
            command('ifdown wlan0')
            interfaces, _, _ = command('ifconfig')
            require('at DOWN' in interfaces and 'inet addr:0.0.0.0' in interfaces,
                    'radio interface must be down without an address')
            command('uname -a')
            command('ps')
            command('free')
            # Set once at a whole UTC second, matching the published method.
            epoch = int(time.time()) + 2
            setting = datetime.fromtimestamp(epoch, timezone.utc).strftime(
                '%b %d %H:%M:%S %Y')
            text = 'date -u -s "' + setting + '"'
            raw.write('\nHOST_SET_TIME ' + text + '\n')
            for byte in text.encode():
                port.write(bytes([byte]))
                time.sleep(0.01)
            time.sleep(max(0, epoch - time.time()))
            port.write(b'\r')
            prompt()
            rolling = ''
            started = time.monotonic()
            state['started_utc'] = datetime.now(timezone.utc).isoformat()
            state['standby'] = state['time_consistency'] = 'RUNNING'
            command('showinfo -i 60 &')
            last_monitor = time.monotonic()
            require(sample(0), 'initial clock alignment outside 2s')
            print('LONGRUN_STARTED ' + state['started_utc'], flush=True)
            for index in range(1, 5):
                deadline = started + index * 6 * 3600
                while time.monotonic() < deadline:
                    read()
                    require(time.monotonic() - last_monitor < 180,
                            'resource-monitor heartbeat lost')
                ok = sample(index)
                tasks, _, _ = command('ps')
                require('showinfo' in tasks, 'resource monitor task missing')
                if index == 2:
                    state['standby'] = 'PASS'
                    print('XTS_STANDARD=3.1.1 PASS observed_seconds=' +
                          str(time.monotonic() - started), flush=True)
                if index == 4:
                    state['time_consistency'] = 'PASS' if ok else 'FAIL'
                    print('XTS_STANDARD=1.3.14 ' + state['time_consistency'],
                          flush=True)
                save()
        except BaseException as error:
            state['error'] = repr(error)
            for key in ('standby', 'time_consistency'):
                if state[key] != 'PASS':
                    state[key] = 'FAIL'
            save()
            traceback.print_exc()
            raise
