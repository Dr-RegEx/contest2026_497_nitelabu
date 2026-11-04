#!/usr/bin/env python3
"""Read-only verification of an E403-style factory artifact directory. No device I/O."""
import argparse
import json
import sys

sys.dont_write_bytecode = True
from entrypoint_support import HERE, contained, local_path, read_json, sha


def verify(directory):
    manifest = read_json(directory / 'manifest.json')
    if manifest['profile'] != 'korvo-openvela-factory' or manifest['board'] != 'ESP32-S31 Korvo-1':
        raise ValueError('Only the documented Korvo factory package layout is supported')
    if manifest['flat_single_image'] is not True or manifest['appfs_required'] is not False:
        raise ValueError('Expected FLAT single image without external AppFS')
    if set(manifest['artifacts']) != {'nuttx', 'nuttx.bin', '.config'}:
        raise ValueError('Expected exactly nuttx, nuttx.bin and .config')
    for name, record in manifest['artifacts'].items():
        path = contained(directory, name)
        if not path.is_file() or path.stat().st_size != record['bytes'] or sha(path) != record['sha256']:
            raise ValueError('Artifact bytes/hash differ: ' + name)
    config = {}
    for line in (directory / '.config').read_text().splitlines():
        if line.startswith('CONFIG_') and '=' in line:
            key, value = line.split('=', 1)
            config[key] = value
    for key in ('CONFIG_ARCH_CHIP_ESP32S31', 'CONFIG_BUILD_FLAT',
                'CONFIG_ESPRESSIF_SIMPLE_BOOT', 'CONFIG_ESPRESSIF_FLASH_16M',
                'CONFIG_ESP32S31_KORVO_HMI_DATA', 'CONFIG_XMSDEMO_FACTORY'):
        if config.get(key) != 'y':
            raise ValueError('Unexpected factory configuration: ' + key)
    for key in ('CONFIG_SMP', 'CONFIG_BUILD_KERNEL', 'CONFIG_ESPRESSIF_WIFI', 'CONFIG_ESP32S31_BLE', 'CONFIG_BT'):
        if config.get(key) == 'y':
            raise ValueError('Incompatible factory configuration: ' + key)
    offset, flash_bytes = manifest['kernel_offset'], manifest['flash_bytes']
    # tools/espressif/Config.mk selects this offset for S31 simple boot;
    # arch/risc-v/src/esp32s31/CMakeLists.txt generates the matching image.
    if offset != 0x2000 or flash_bytes != 16 * 1024 * 1024:
        raise ValueError('Manifest disagrees with the S31 simple-boot/16MiB contract')
    data_start = int(config['CONFIG_ESP32S31_HMI_DATA_OFFSET'], 0)
    data_bytes = int(config['CONFIG_ESP32S31_HMI_DATA_SIZE'], 0)
    image_end = offset + manifest['artifacts']['nuttx.bin']['bytes']
    erase_end = (image_end + 4095) // 4096 * 4096
    if not (offset < erase_end <= data_start < data_start + data_bytes <= flash_bytes):
        raise ValueError('Image/erase range overlaps HMI data or exceeds flash')
    digest = manifest['artifacts']['nuttx.bin']['sha256']
    factory = read_json(HERE / 'profiles.json')['profiles']['korvo-openvela-factory']
    return dict(status='PACKAGE_BYTES_AND_FACTORY_LAYOUT_CHECKED',
                manifest_sha256=sha(directory / 'manifest.json'), image_sha256=digest,
                matches_recorded_board_image=digest == factory['board_test']['image_sha256'],
                matches_recorded_receiver_image=digest == factory['build_test']['image_sha256'],
                kernel_offset=offset, flash_bytes=flash_bytes, image_end=image_end,
                sector_erase_end=erase_end, hmi_data_offset=data_start, hmi_data_bytes=data_bytes,
                appfs_required=False, board_access=False, automatic_flash_ready=False,
                limits=['Hashes identify bytes; they do not authenticate the publisher',
                        'Target identity/security state, full backup and restore verification are not supplied by this manifest',
                        'No new board qualification; follow FLASH-MANUAL.md or reviewed transaction helpers'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', required=True, help='Local firmware/factory directory containing manifest.json and its three artifacts')
    args = parser.parse_args()
    print(json.dumps(verify(local_path(args.directory).resolve(strict=True)), indent=2))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError) as error:
        sys.exit('Artifact verification failed: ' + str(error))
