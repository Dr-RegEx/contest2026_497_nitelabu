"""Deliberately reuse and restore the backed-up 3 MiB xTS scratch range.

Never touches firmware/AppFS. Use only after the active UART test finishes.
Erase is followed separately by the existing double-blank-backup/format guard.
Restore writes the preserved bytes and verifies an independent full readback.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

ESPTOOL = '/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool'
PORT = '/dev/ttyUSB0'
OFFSET = 0xd00000
SIZE = 0x300000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('erase', 'restore'))
    parser.add_argument('--backup', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.backup.read_text())
    if (manifest.get('purpose') != 'xts-large-before-wav-1255' or
            manifest.get('offset') != OFFSET or manifest.get('size') != SIZE):
        raise RuntimeError('wrong scratch backup identity')
    entries = manifest.get('files', [])
    if [e.get('name') for e in entries] != ['before-a.bin', 'before-b.bin']:
        raise RuntimeError('two independent preserved reads required')
    copies = []
    for entry in entries:
        data = (args.backup.parent / entry['name']).read_bytes()
        if len(data) != SIZE or hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise RuntimeError('backup size/digest mismatch')
        copies.append(data)
    if copies[0] != copies[1]:
        raise RuntimeError('preserved reads differ')
    busy = subprocess.run(['fuser', PORT], capture_output=True)
    if busy.returncode != 1 or busy.stderr.strip():
        raise RuntimeError('UART busy or ownership check failed')

    args.output.mkdir(exist_ok=False)
    intent = {
        'action': args.action, 'offset': OFFSET, 'size': SIZE,
        'backup': str(args.backup.resolve()),
        'preserved_sha256': entries[0]['sha256'],
        'created_utc': datetime.now(timezone.utc).isoformat(),
        'scope': 'xTS scratch only; firmware/AppFS excluded',
    }
    token = args.backup.parent / ('scratch1256-' + args.action + '-intent.json')
    with token.open('x') as stream:
        json.dump(intent, stream, indent=2)
        stream.write('\n')
    result = dict(intent, state='STARTED')
    base = [ESPTOOL, '--chip', 'esp32s31', '--port', PORT, '--baud', '460800']
    try:
        if args.action == 'erase':
            command = base + ['erase-region', hex(OFFSET), hex(SIZE)]
        else:
            command = base + ['write-flash', '--flash-size', '16MB',
                              hex(OFFSET), str((args.backup.parent / 'before-a.bin').resolve())]
        with (args.output / 'operation.log').open('x') as log:
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
        if args.action == 'restore':
            readback = args.output / 'restored-readback.bin'
            with (args.output / 'readback.log').open('x') as log:
                subprocess.run(base + ['read-flash', hex(OFFSET), hex(SIZE),
                                       str(readback.resolve())], check=True,
                               stdout=log, stderr=subprocess.STDOUT)
            if readback.read_bytes() != copies[0]:
                raise RuntimeError('restored full-volume readback differs')
            result['state'] = 'RESTORED_AND_READBACK_VERIFIED'
        else:
            result['state'] = 'ERASE_COMMAND_COMPLETE_BLANK_READBACK_PENDING'
    except BaseException as error:
        result.update(state='FAILED', error=str(error))
        raise
    finally:
        (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
