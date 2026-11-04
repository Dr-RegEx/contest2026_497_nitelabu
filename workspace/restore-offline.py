#!/usr/bin/env python3
"""Local product / SDK restore and SDK readback using the existing support tools."""
import argparse
import json
import subprocess
import sys

sys.dont_write_bytecode = True
from entrypoint_support import (HERE, clean_env, contained, fresh_output, local_path,
                                read_json, require_files)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('source', 'sdk', 'verify-sdk'))
    parser.add_argument('--package', required=True, help='Existing local product or SDK package')
    parser.add_argument('--output', required=True, help='NEW receiver / SDK / verification report directory')
    parser.add_argument('--platform-root', help='Required for sdk: restored receiver/workspace')
    parser.add_argument('--sdk', help='Required for verify-sdk: restored SDK directory')
    parser.add_argument('--dry-run', action='store_true', help='Check metadata and required local paths; do not extract, hash large archives, or execute tools')
    args = parser.parse_args()
    package = local_path(args.package).resolve(strict=True)
    inputs = [package, HERE.parent]
    support = HERE / 'tools/build-support'
    command = [sys.executable, '-B']
    if args.operation == 'source':
        require_files(package, ['dependencies.json', 'source/receipt.json',
                                'source/manifest.jsonl', 'source/source.tar.gz'])
        deps = read_json(package / 'dependencies.json')
        lock = read_json(support / 'host-packages.json')['repositories']
        names = {'s31-reference/deps/esp-hal-3rdparty': 'esp-hal.bundle',
                 's31-reference/tmp/esp-idf-clean': 'esp-idf.bundle'}
        if deps['schema'] != 1 or set(deps['repositories']) != set(names):
            raise ValueError('Unexpected dependency inventory')
        for name, item in deps['repositories'].items():
            if item['bundle'] != names[name] or item['head'] != lock[name]:
                raise ValueError('Product dependency lock differs: ' + name)
            require_files(package, [item['bundle']])
        if read_json(package / 'source/receipt.json')['status'] != 'EXPORTED_REQUIRES_VERIFICATION':
            raise ValueError('Incomplete source export')
        command += [str(support / 'restore_product.py'), '--package', str(package)]
    else:
        meta = read_json(package / 'sdk.json')
        if meta['status'] != 'EXISTING_HOST_SDK_PACKAGED' or meta['target'] != 'esp32s31':
            raise ValueError('Incomplete or unsupported SDK package')
        require_files(package, ['python.tar.gz', 'python-manifest.json', meta['constraints']['filename']])
        for item in meta['tools']:
            require_files(package / 'dist', [item['filename']])
        if args.operation == 'sdk':
            if not args.platform_root:
                parser.error('sdk requires --platform-root')
            root = local_path(args.platform_root).resolve(strict=True)
            require_files(root, ['s31-reference/tmp/esp-idf-clean/tools/tools.json'])
            inputs.append(root)
            command += [str(support / 'offline_sdk.py'), 'restore', '--workspace', str(root), '--package', str(package)]
        else:
            if not args.sdk:
                parser.error('verify-sdk requires --sdk')
            sdk = local_path(args.sdk).resolve(strict=True)
            inputs.append(sdk)
            if read_json(sdk / 'receipt.json')['status'] != 'OFFLINE_SDK_RESTORED_AND_COMMANDS_VERIFIED':
                raise ValueError('SDK restore is incomplete')
            command += [str(support / 'verify_sdk.py'), '--package', str(package), '--sdk', str(sdk)]
    output = fresh_output(args.output, inputs)
    command += ['--output', str(output)]
    print(json.dumps(dict(operation=args.operation, command=command, dry_run=args.dry_run,
                         dry_run_scope='Metadata and local paths only; full integrity checks run in the delegated tool',
                         downloads=False, board_access=False), indent=2), flush=True)
    return 0 if args.dry_run else subprocess.run(command, env=clean_env()).returncode


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, OSError, subprocess.SubprocessError) as error:
        sys.exit('Offline restore preflight failed: ' + str(error))
