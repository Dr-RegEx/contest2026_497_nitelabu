"""Transfer an audio file with original rb/sb, without requesting a reset.

Run only after longrun864 and its actual-duration extension finish. The
existing raw115200 tty configuration is reused; no DTR/RTS/termios changes.
An OS-induced reset is still possible: reject boot markers or missing RAM
files, rather than silently recording again. No audio test PASS is emitted.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import termios
import time

ROOT = Path('/home/regex/work/esp32s31-openvela')
sys.path.insert(0, str(ROOT / 'openvela-dev/apps/system/ymodem'))
import sbrb

FAULT = re.compile(r'ESP-ROM:|Assertion failed|S31SM:M-TRAP|Segmentation fault|kasan_report:')


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--upload', type=Path)
    action.add_argument('--download', help='existing basename under /data')
    action.add_argument('--sequential', help='fresh /data basename: original capture/playback, then export')
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='fresh evidence directory')
    args = parser.parse_args()
    name = args.upload.name if args.upload else (args.download or args.sequential)
    require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,47}', name), 'invalid basename')
    receipt = args.receipt.read_text().splitlines()
    require(len(receipt) == 1, 'one FLAT image receipt required')
    digest, image = receipt[0].split(maxsplit=1)
    image = Path(image)
    require(hashlib.sha256(image.read_bytes()).hexdigest() == digest, 'image digest mismatch')
    config = (image.parent / '.config').read_text()
    for option in ('BUILD_FLAT', 'ESP32S31_AUDIO', 'SYSTEM_YMODEM', 'FS_TMPFS'):
        require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    if args.sequential:
        require('CONFIG_ES8311_RATE_DEPENDENT_MCLK=y\n' in config,
                'original44100Hz sequence requires candidate968 or later')
        require('CONFIG_FS_HEAPSIZE=0\n' in config and
                'CONFIG_MM_KERNEL_HEAP=y\n' not in config,
                'sequence expects the shared FLAT user/filesystem heap')
        require(len('cmocka_driver_audio -a 3 -p /data/' + name) < 80,
                'basename too long for original command')
    if args.upload:
        args.upload = args.upload.resolve(strict=True)
        require(args.upload.is_file() and args.upload.stat().st_size > 0, 'empty/missing source')
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(), 'UART busy or check failed')
    args.output = args.output.resolve()
    args.output.mkdir(exist_ok=False)
    receive = args.output / 'receive'
    receive.mkdir()
    result = {'status': 'STARTED', 'audio_acceptance': 'NOT_TESTED',
              'direction': 'upload' if args.upload else ('sequential-and-export' if args.sequential else 'download'),
              'remote': '/data/' + name, 'image_sha256': digest,
              'explicit_reset': False, 'started_unix': time.time()}
    fd = None
    old_cwd = Path.cwd()
    try:
        fd = os.open('/dev/ttyUSB0', os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        settings = termios.tcgetattr(fd)
        require(settings[4] == settings[5] == termios.B115200 and
                not settings[3] & (termios.ICANON | termios.ECHO),
                'expected existing raw115200 tty; no settings changed')
        with (args.output / 'uart.log').open('x', buffering=1) as log:
            def read(size, timeout=0.2):
                if not select.select([fd], [], [], timeout)[0]:
                    return b''
                data = os.read(fd, size)
                require(data, 'serial disconnected')
                return data

            def write(data):
                pending = memoryview(data)
                deadline = time.monotonic() + 5
                while pending:
                    require(time.monotonic() < deadline, 'serial write timeout')
                    if select.select([], [fd], [], 0.2)[1]:
                        try:
                            pending = pending[os.write(fd, pending):]
                        except BlockingIOError:
                            pass
                return len(data)

            def shell_read(timeout=30):
                output = ''
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    chunk = read(4096).decode(errors='replace')
                    log.write(chunk)
                    output += chunk
                    require(not FAULT.search(output), 'target reset/fault; no retry')
                    if 'nsh> ' in output:
                        return output
                raise TimeoutError('NSH prompt missing; no reset performed')

            def start(command):
                require(len(command) < 80, 'command exceeds NSH line capacity')
                while select.select([fd], [], [], 0)[0]:
                    stale = read(4096, 0).decode(errors='replace')
                    log.write(stale)
                    require(not FAULT.search(stale), 'reset/fault before command')
                log.write('\nHOST_COMMAND ' + command + '\n')
                for byte in command.encode() + b'\r':
                    write(bytes([byte]))
                    time.sleep(0.01)

            def command(text, timeout=30):
                start(text)
                return shell_read(timeout)

            mounts = command('mount')
            require('/data type tmpfs' in mounts, 'existing /data tmpfs required; no mount/format performed')
            for node in ('pcm0p', 'pcm0c'):
                listing = command('ls /dev/audio/' + node)
                require(node in listing and 'No such file' not in listing and 'nsh:' not in listing,
                        'audio node missing; confirm current audio firmware')
            remote = '/data/' + name
            listing = command('ls -l ' + remote)
            if args.sequential:
                require('No such file' in listing, 'recording path exists; refusing overwrite')
                memory = command('free')
                heap = re.search(r'(?m)^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+'
                                 r'(\d+)\s+(\d+)\s+(\d+)\s+Umem\s*$', memory)
                require(heap, 'live user heap information unavailable')
                result['heap_free_before'] = int(heap[3])
                result['heap_largest_before'] = int(heap[5])
                # 1,764,000 nominal PCM bytes plus original task/buffer and
                # filesystem allocation headroom, checked before capture.
                require(min(int(heap[3]), int(heap[5])) >= 2 * 1024 * 1024,
                        'insufficient contiguous RAM for the original10s recording')
                started = time.time()
                audio = command('cmocka_driver_audio -a 3 -p ' + remote, 120)
                result['original_program_elapsed_host_seconds'] = time.time() - started
                require('Start Capture.' in audio and 'Start Playback.' in audio
                        and '[       OK ] drivertest_audio' in audio
                        and re.search(r'\[  PASSED  \] 1 test\(s\)\.', audio)
                        and not re.search(r'FAILED|SKIPPED|nsh:|[Ee][Rr][Rr][Oo][Rr]|[Ff]ailed|[Tt]imed?\s*out', audio),
                        'original audio program incomplete/failed; no rerecord or reset')
                result['original_program'] = 'COMPLETED_WITH_CMOCKA_PASS'
                result['audio_acceptance'] = 'LISTENING_AND_FORMAT_REVIEW_PENDING'
                result['recording_format'] = {'channels': 2, 'bits': 16, 'rate_hz': 44100,
                                              'requested_capture_seconds': 10}
                listing = command('ls -l ' + remote)
            if args.upload:
                require('No such file' in listing, 'target file exists; refusing overwrite')
                expected = args.upload.stat().st_size
            else:
                match = re.search(r'(?m)^.*\s(\d+)\s+(?:/data/)?' + re.escape(name) + r'\s*$', listing)
                require(match and 'nsh:' not in listing, 'RAM recording missing or size unavailable')
                expected = int(match[1])
                require(expected > 0, 'recording is empty')
                if args.sequential:
                    require(expected % 4 == 0, 'recording has a partial stereo16 frame')
                    result['pcm_duration_seconds'] = expected / (44100 * 4)
            result['expected_bytes'] = expected
            start('rb -f /data' if args.upload else 'sb ' + remote)
            # Consume only the echoed command line, leaving protocol bytes.
            echo = b''
            deadline = time.monotonic() + 5
            while not echo.endswith(b'\n') and time.monotonic() < deadline:
                echo += read(1)
            log.write(echo.decode(errors='replace'))
            require(echo.endswith(b'\n'), 'transfer command echo incomplete')
            deadline = time.monotonic() + 600

            def protocol_read(size):
                data = bytearray()
                until = min(deadline, time.monotonic() + 2)
                while len(data) < size and time.monotonic() < until:
                    data.extend(read(size - len(data)))
                require(time.monotonic() < deadline, 'YMODEM deadline reached')
                return bytes(data)

            protocol = sbrb.ymodem(read=protocol_read, write=write,
                                  clear=lambda: termios.tcflush(fd, termios.TCIFLUSH),
                                  maxretry=10, progress=log.write)
            os.chdir(receive)
            code = protocol.send([str(args.upload)]) if args.upload else protocol.recv()
            require(code is None, f'YMODEM failed: {code}')
            shell_read()
            status = command('echo $?')
            require(re.search(r'(?m)^0\r?$', status), 'target transfer returned failure')
            if args.upload:
                listing = command('ls -l ' + remote)
                require(re.search(r'(?m)^.*\s' + str(expected) + r'\s+(?:/data/)?' +
                                  re.escape(name) + r'\s*$', listing), 'uploaded size mismatch')
                local = args.upload
            else:
                local = receive / name
                require(local.is_file() and local.stat().st_size == expected,
                        'received recording size mismatch')
            result.update(status='TRANSFER_COMPLETE', bytes=expected,
                          local_file=str(local), local_sha256=hashlib.sha256(local.read_bytes()).hexdigest())
    except BaseException as error:
        result.update(status='FAILED', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        os.chdir(old_cwd)
        if fd is not None:
            os.close(fd)
        result['finished_unix'] = time.time()
        (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
