"""Audited temporary IDF layout; restore only touched 4 KiB sectors.

Never erase the chip. Never write at/above 0x200000 (including AppFS),
let alone the protected writable partition at 0x500000. Full 5 MiB
double-read backup is mandatory, and full 5 MiB restore comparison follows.
"""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path('/home/regex/work/esp32s31-openvela')
PROJECT = ROOT / 'diagnostics/idf-baseline'
BUILD = PROJECT / 'build'
BACKUP = ROOT / 'backups/2026-09-10-scan-stress/idf170-flash-backup'
IDENTITY = Path('/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_f65e4ef67f71f011975a049f1045c30f-if00-port0')
ESPTOOL = '/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool'
RUN = os.environ.get('S31_IDF_RUN', '170')
if RUN not in ('170', '171', '172', '173'):
    raise RuntimeError('Unknown audited IDF build')
STAGE_NAME = 'transaction' if RUN == '170' else 'transaction' + RUN
SDKCONFIG = PROJECT / 'sdkconfig'
if RUN == '172':
    BUILD = PROJECT / 'build-hal-phy'
    SDKCONFIG = PROJECT / 'sdkconfig-hal-phy'
elif RUN == '173':
    BUILD = PROJECT / 'build-hal-wifi'
    SDKCONFIG = PROJECT / 'sdkconfig-hal-wifi'
EXPECTED = {
    'bootloader/bootloader.bin': '6cec23aa2f5835abf61a388e9f39edf048a2aecc82c92b14a34577a3a842428b',
    'partition_table/partition-table.bin': '4e31205fa4c5ba45b6bde8fd483490fe1a706b318f93d53b7e4698cd8407dda3',
    's31_wifi_baseline.bin': 'd7d51a230380510792870b595d672a4feb87e005ad4eb557d788ea9098873595',
}
if RUN == '171':
    EXPECTED['s31_wifi_baseline.bin'] = 'e11a55fd4ffd3abf666e8a6b381717d8a53e49f1ac1a4e092cca99d8ec801e16'
elif RUN == '172':
    EXPECTED['bootloader/bootloader.bin'] = '493eea9faf3585b390af63f2808b3ffa8ffd6d371f0c74c9a6c4c5a407b9855c'
    EXPECTED['s31_wifi_baseline.bin'] = 'a1653aec3106229bc61824a2a74679b3f7e88a24b5ed119ca857994b285528f0'
elif RUN == '173':
    EXPECTED['bootloader/bootloader.bin'] = '026e17ad65f92aea6625c9624f993948c88ba8806341d10e6f98719a7a602eb0'
    EXPECTED['s31_wifi_baseline.bin'] = 'c19655eb4dd72d2f086dc47cb341ccdaf849c9ff7fbeff5b0a3a74dd17e68953'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def before_image():
    first = (BACKUP / 'before-a.bin').read_bytes()
    second = (BACKUP / 'before-b.bin').read_bytes()
    require(len(first) == 0x500000 and first == second, 'backup size/equality failed')
    subprocess.run(['sha256sum', '--check', str(BACKUP / 'SHA256SUMS')], check=True)
    return first


def prepare():
    original = before_image()
    if RUN != '170':
        previous = 'after-restore.bin' if RUN == '171' else f'after-restore{int(RUN) - 1}.bin'
        require((BACKUP / previous).read_bytes() == original,
                'previous restoration must be verified before next comparison')
    config = SDKCONFIG.read_text()
    for key in ('SECURE_BOOT', 'SECURE_FLASH_ENC_ENABLED', 'ESP_WIFI_NVS_ENABLED',
                'ESP_PHY_CALIBRATION_AND_DATA_STORAGE', 'ESP_PHY_INIT_DATA_IN_PARTITION'):
        require(f'# CONFIG_{key} is not set' in config, f'unsafe config {key}')
    args = json.loads((BUILD / 'flasher_args.json').read_text())
    expected_layout = {'0x2000': 'bootloader/bootloader.bin',
                       '0x8000': 'partition_table/partition-table.bin',
                       '0x10000': 's31_wifi_baseline.bin'}
    require(args['flash_files'] == expected_layout, 'unexpected flash layout')
    partition = (BUILD / 'partition_table/partition-table.bin').read_bytes()
    magic, kind, subtype, offset, size, label, flags = struct.unpack('<HBBII16sI', partition[:32])
    require((magic, kind, subtype, offset, size, flags) ==
            (0x50aa, 0, 0, 0x10000, 0x1f0000, 0), 'unexpected factory partition')
    require(partition[32:34] == b'\xeb\xeb' and
            all(value == 255 for value in partition[64:]), 'unexpected extra partitions')
    stage = BACKUP / STAGE_NAME
    stage.mkdir(mode=0o700)  # Never overwrite an existing recovery checkpoint.
    manifest = {'original_sha256': digest(original), 'limit': 0x200000, 'images': []}
    previous_end = 0
    for index, (address, relative) in enumerate(expected_layout.items()):
        data = (BUILD / relative).read_bytes()
        require(digest(data) == EXPECTED[relative], 'image differs from successful audited build')
        start = int(address, 0)
        end = (start + len(data) + 4095) & ~4095
        require(start % 4096 == 0 and start >= previous_end and end <= 0x200000,
                'overlapping or unsafe erase range')
        previous_end = end
        image_name = f'idf-{index}.bin'
        restore_name = f'openvela-{index}.bin'
        (stage / image_name).write_bytes(data)
        restore = original[start:end]
        (stage / restore_name).write_bytes(restore)
        manifest['images'].append({'offset': start, 'end': end,
                                   'idf': image_name, 'idf_sha256': digest(data),
                                   'restore': restore_name, 'restore_sha256': digest(restore)})
    (stage / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2), flush=True)
    print('FLASH_PLAN=PASS; no device writes performed', flush=True)


def esptool(arguments):
    require(IDENTITY.resolve() == Path('/dev/ttyUSB0') and IDENTITY.exists(),
            'board USB identity missing or changed')
    command = [ESPTOOL, '--chip', 'esp32s31', '--port', str(IDENTITY),
               '--baud', '460800'] + arguments
    print('COMMAND=' + ' '.join(command), flush=True)
    subprocess.run(command, check=True)


def write(operation):
    original = before_image()
    stage = BACKUP / STAGE_NAME
    manifest = json.loads((stage / 'manifest.json').read_text())
    require(digest(original) == manifest['original_sha256'], 'original backup changed')
    pairs = []
    for entry in manifest['images']:
        key = 'idf' if operation == 'install' else 'restore'
        file = stage / entry[key]
        data = file.read_bytes()
        require(digest(data) == entry[key + '_sha256'], 'staged image hash mismatch')
        require(0x2000 <= entry['offset'] < entry['end'] <= 0x200000,
                'unsafe write span')
        require(entry['offset'] % 4096 == 0 and entry['end'] % 4096 == 0 and
                ((entry['offset'] + len(data) + 4095) & ~4095) == entry['end'],
                'erase span mismatch')
        if operation == 'restore':
            require(data == original[entry['offset']:entry['end']], 'restore bytes mismatch')
        pairs += [hex(entry['offset']), str(file)]
    # Keep original headers bit-for-bit, including the saved NuttX image.
    esptool(['write-flash', '--flash-mode', 'keep', '--flash-size', 'keep',
             '--flash-freq', 'keep'] + pairs)
    print(operation.upper() + '_FLASH=PASS', flush=True)
    if operation == 'restore':
        destination = BACKUP / ('after-restore.bin' if RUN == '170' else f'after-restore{RUN}.bin')
        require(not destination.exists(), 'refusing to overwrite restore verification')
        esptool(['read-flash', '0x0', '0x500000', str(destination)])
        require(destination.read_bytes() == original, 'restored full region differs!')
        print('RESTORE_FULL_5MIB_COMPARE=PASS sha256=' + digest(original), flush=True)


if __name__ == '__main__':
    os.umask(0o077)
    if sys.argv[1:] == ['prepare']:
        prepare()
    elif sys.argv[1:] in (['install'], ['restore']):
        write(sys.argv[1])
    else:
        raise SystemExit('Expected prepare, install, or restore')
