"""Format only candidate987's disposable WAV volume after verified blank backup.

Never invoke during longrun864. No reset, network setup, retry or auto-cleanup.
The volume contains RAM and cannot survive reboot. This is storage preparation,
not a filesystem durability or original audio acceptance result.
"""
import argparse
from contextlib import redirect_stdout
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv_transport', HERE / 'xts-kvdb-suite.py')
kv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kv)
require = kv.require


def verify_backup(path):
    manifest = json.loads(path.read_text())
    require(manifest.get('purpose') == 'temporary-media-volume-987' and
            manifest.get('offset') == 0xd00000 and manifest.get('size') == 0x300000 and
            manifest.get('blank') is True, 'wrong or nonblank media Flash backup')
    require([entry.get('name') for entry in manifest.get('files', [])] ==
            ['before-a.bin', 'before-b.bin'], 'two separate original reads required')
    for entry in manifest['files']:
        data = (path.parent / entry['name']).read_bytes()
        require(len(data) == 0x300000 and data == b'\xff' * len(data) and
                hashlib.sha256(data).hexdigest() == entry['sha256'],
                'media backup length, content or digest mismatch')
    require(not (path.parent / 'used-by-media-volume-987.json').exists(),
            'backup has already authorized a format attempt; no automatic retry')
    return manifest


def verify_image(receipt):
    lines = receipt.read_text().splitlines()
    require(len(lines) == 1, 'single FLAT image receipt required')
    digest, name = lines[0].split(maxsplit=1)
    image = Path(name)
    require(image.name == 'nuttx.bin' and
            hashlib.sha256(image.read_bytes()).hexdigest() == digest, 'image receipt mismatch')
    config = (image.parent / '.config').read_text()
    for option in ('BUILD_FLAT', 'ESP32S31_XTS_MEDIA_VOLUME', 'FS_LITTLEFS',
                   'ESP32S31_SPIFLASH_PSRAM_STACK'):
        require(f'CONFIG_{option}=y\n' in config, 'missing ' + option)
    for option in ('SMP', 'ESP32S31_XTS_FLASH', 'ESPRESSIF_WIFI'):
        require(f'CONFIG_{option}=y\n' not in config, 'unexpected ' + option)
    return digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--flash-backup', type=Path, required=True)
    parser.add_argument('--boot-evidence', type=Path, required=True,
                        help='current987 flash/boot transcript identifying receipt and volume marker')
    parser.add_argument('--output', type=Path, required=True, help='fresh evidence directory')
    args = parser.parse_args()
    digest = verify_image(args.receipt)
    verify_backup(args.flash_backup)
    boot = args.boot_evidence.read_text()
    require(digest in boot and 'xTS WAV volume:' in boot and
            '/dev/wavvol' in boot and '0xd00000' in boot and '0x300000' in boot,
            'current image and isolated volume boot evidence required')
    require(not re.search(r'Assertion failed|S31SM:M-TRAP|kasan_report:|PANIC', boot),
            'boot evidence contains target failure')
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(), 'UART busy or ownership check failed')
    args.output.mkdir(exist_ok=False)
    result = {'status': 'STARTED', 'image_sha256': digest,
              'flash_backup': str(args.flash_backup.resolve()),
              'boot_evidence_sha256': hashlib.sha256(args.boot_evidence.read_bytes()).hexdigest(),
              'flash_offset': 0xd00000, 'flash_size': 0x300000,
              'volume': '/dev/wavvol', 'mountpoint': '/wav',
              'volatile': True, 'audio_acceptance': 'NOT_TESTED'}
    try:
        with (args.output / 'uart.log').open('x', buffering=1) as log, redirect_stdout(log):
            print('MEDIA_IMAGE_SHA256=' + digest, flush=True)
            with kv.existing_uart() as port:
                mounts = kv.command(port, 'mount')
                require(not re.search(r'(?m)^\s*(?:\[CPU\d+\]\s*)?/(?:wav|apps)(?:/\S*)? type ', mounts),
                        'existing media or production mount; refusing format')
                kv.command(port, 'ls /dev/wavvol')
                kv.command(port, 'free')
                tasks = kv.command(port, 'ps')
                require(not re.search(r'\b(?:mediad|mediatool|kvdbd)\b|vela_fs_', tasks),
                        'media or filesystem workload is active')
                # Consume the backup before any destructive command. Failures retain
                # this token and all evidence; they never authorize a silent retry.
                token = args.flash_backup.parent / 'used-by-media-volume-987.json'
                with token.open('x') as stream:
                    json.dump({'image_sha256': digest, 'output': str(args.output.resolve()),
                               'created_utc': datetime.now(timezone.utc).isoformat()}, stream, indent=2)
                    stream.write('\n')
                kv.command(port, 'mount -t littlefs -o forceformat /dev/wavvol /wav', 120)
                mounts = kv.command(port, 'mount')
                require(re.search(r'(?m)^\s*(?:\[CPU\d+\]\s*)?/wav type littlefs\s*$', mounts),
                        'temporary volume mount missing')
                kv.command(port, 'df')
                kv.command(port, 'free')
        result['status'] = 'TEMPORARY_VOLUME_PREPARED'
    except BaseException as error:
        result.update(status='FAILED', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
