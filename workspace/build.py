#!/usr/bin/env python3
"""Build a registered profile using the platform's real helper; never flash."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from entrypoint_support import (BOARD, HERE, clean_env, contained, fresh_output,
                                local_path, platform_layout, read_json, sha)


def profile_inputs(root, profile):
    base = root / BOARD / 'configs'
    seen, active = {}, set()

    def visit(path):
        path = path.resolve()
        if not path.is_relative_to(base.resolve()):
            raise ValueError('Profile include escapes canonical configs: ' + str(path))
        if path in active:
            raise ValueError('Profile include cycle: ' + str(path))
        if str(path) in seen:
            return
        if not path.is_file():
            raise ValueError('Profile/source integration missing: ' + str(path))
        active.add(path)
        seen[str(path)] = sha(path)
        for line in path.read_text().splitlines():
            if re.match(r'\s*#\s*include\b', line):
                match = re.fullmatch(r'\s*#\s*include\s+"([^"]+)"\s*', line)
                if not match:
                    raise ValueError('Unsupported profile include: ' + line)
                visit(path.parent / match[1])
        active.remove(path)

    visit(base / profile / 'defconfig')
    return seen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true', help='Show profiles and qualification boundaries')
    parser.add_argument('--platform-root', help='Parent of openvela-dev, tools and s31-reference')
    parser.add_argument('--sdk', help='Restored offline SDK directory (sets IDF_TOOLS_PATH)')
    parser.add_argument('--profile', default='korvo-openvela-factory')
    parser.add_argument('--output', help='New directory outside source, SDK and contest repository')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--share-source', action='store_true', help='Opt into helper source-preserving CMake mode; default copies source into output')
    parser.add_argument('--dry-run', action='store_true', help='Validate paths, locks and profile includes; print command without preparing or building')
    args = parser.parse_args()
    registry = read_json(HERE / 'profiles.json')
    if args.list:
        print(json.dumps(registry, indent=2, ensure_ascii=False))
        return 0
    if not all((args.platform_root, args.sdk, args.output)):
        parser.error('--platform-root, --sdk and --output are required')
    if args.jobs < 1 or args.profile not in registry['profiles']:
        parser.error('Use positive --jobs and a profile from --list')
    if registry['profiles'][args.profile].get('buildable_by_front_door') is False:
        parser.error('This profile preserves a prebuilt firmware; use its firmware/README.md for deployment. A complete original build tree is not provided.')
    root, sdk = (local_path(x).resolve(strict=True) for x in (args.platform_root, args.sdk))
    output = fresh_output(args.output, (root, sdk, HERE.parent))
    platform_layout(root)
    configs = profile_inputs(root, args.profile)
    lock = read_json(root / 'tools/build-support/host-packages.json')
    env = clean_env()
    env['IDF_TOOLS_PATH'] = str(sdk)
    for relative, head in lock['repositories'].items():
        repo = contained(root, relative)
        actual = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], env=env, text=True).strip()
        if actual != head:
            raise ValueError('Dependency HEAD differs: ' + relative)
    for item in lock['packages']:
        path = contained(root, item['file'])
        if not path.is_file() or sha(path) != item['sha256']:
            raise ValueError('Missing/changed locked host package: ' + item['file'])
    sdk_receipt = read_json(sdk / 'receipt.json')
    if sdk_receipt['status'] != 'OFFLINE_SDK_RESTORED_AND_COMMANDS_VERIFIED':
        raise ValueError('SDK restore is incomplete; run restore-offline.py sdk')
    for name in ('tools', 'python_env'):
        if not (sdk / name).is_dir():
            raise ValueError('Missing restored SDK directory: ' + name)
    helper = root / 'tools/tests/korvo-adc-keys/build.py'
    command = [sys.executable, '-B', str(helper), '--profile', args.profile,
               '--output', str(output), '--jobs', str(args.jobs)]
    if args.share_source:
        command.append('--workspace-source')
    report = dict(schema=1, status='PREFLIGHT_ONLY', command=command,
                  platform_root=str(root), sdk=str(sdk), output=str(output),
                  profile=args.profile, profile_inputs=configs,
                  profile_qualification=registry['profiles'][args.profile],
                  helper_sha256=sha(helper), environment_sha256=sha(root / 'tools/build-support/environment.py'),
                  registry_sha256=sha(HERE / 'profiles.json'),
                  host_lock_sha256=sha(root / 'tools/build-support/host-packages.json'),
                  sdk_restore_receipt_sha256=sha(sdk / 'receipt.json'),
                  board_tested_by_this_invocation=False, downloads_requested=False)
    print(json.dumps(report, indent=2, ensure_ascii=False), flush=True)
    if args.dry_run:
        return 0
    result = subprocess.run(command, env=env).returncode
    report.update(status='BUILD_HELPER_FINISHED', returncode=result)
    if output.is_dir():
        (output / 'entrypoint-receipt.json').write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    return result


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, OSError, subprocess.SubprocessError) as error:
        sys.exit('Build preflight failed: ' + str(error))
