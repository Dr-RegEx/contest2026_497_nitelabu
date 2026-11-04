#!/usr/bin/env python3
"""Build S31 using only the pinned public checkout and setup-dependencies.sh."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from entrypoint_support import HERE, clean_env, fresh_output, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--workspace', type=Path, help='repo checkout containing nuttx, apps, etc.')
    parser.add_argument('--deps', type=Path, help='default: WORKSPACE/.s31-deps')
    parser.add_argument('--profile', default='nsh')
    parser.add_argument('--output', required=False, help='new build output outside source/dependencies')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    registry = json.loads((HERE / 'profiles.json').read_text())
    if args.list:
        print(json.dumps(registry, ensure_ascii=False, indent=2))
        return 0
    if not args.workspace or not args.output or args.jobs < 1:
        parser.error('--workspace, --output and positive --jobs are required')
    if args.profile not in registry['profiles']:
        parser.error('Choose a profile from --list')
    if registry['profiles'][args.profile].get('buildable_by_front_door') is False:
        parser.error('This profile is a preserved binary; see its firmware README for source/deployment status.')
    root = args.workspace.resolve(strict=True)
    deps = (args.deps or root / '.s31-deps').resolve(strict=True)
    output = fresh_output(args.output, (root, deps, HERE.parent))
    inputs = json.loads((deps / 'receipt.json').read_text())
    if inputs['status'] != 'PUBLIC_DEPENDENCIES_READY':
        raise ValueError('Run setup-dependencies.sh for this public checkout first')
    for name, expected in inputs['inputs'].items():
        if sha(HERE / 'dependencies' / name) != expected:
            raise ValueError('Dependency inputs changed; rerun setup-dependencies.sh: ' + name)
    headers = HERE / 'dependencies/idf-sdmmc'
    for name, expected in json.loads((headers / 'sha256.json').read_text()).items():
        if sha(headers / name) != expected:
            raise ValueError('SDMMC header checksum mismatch: ' + name)
    if args.profile == 'korvo-digit-rebuild':
        command = [sys.executable, '-B', str(HERE / 'digit-demo/build.py'),
                   '--workspace', str(root), '--deps', str(deps),
                   '--output', str(output), '--jobs', str(args.jobs)]
        if args.dry_run:
            command.append('--dry-run')
        return subprocess.call(command)
    config = root / 'nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs' / args.profile / 'defconfig'
    if not config.is_file():
        raise ValueError('Missing profile in checkout; apply the public snapshot first: ' + str(config))
    env = clean_env()
    for name in tuple(env):
        if name.startswith(('IDF_', 'ESP_IDF_', 'VELA_')):
            env.pop(name, None)
    env['PATH'] = os.pathsep.join([str(deps / 'venv/bin'), str(deps / 'riscv32-esp-elf/bin'),
        str(root / 'prebuilts/tools/linux/x86_64'), '/usr/local/bin', '/usr/bin', '/bin'])
    env['LD_LIBRARY_PATH'] = str(deps / 'sysroot/usr/lib/x86_64-linux-gnu')
    env['ESP_HAL_3RDPARTY_LOCAL'] = str(deps / 'esp-hal-3rdparty')
    python = deps / 'venv/bin/python3'
    commands = [
        ['cmake', '-S', str(root / 'nuttx'), '-B', str(output / 'build'), '-G', 'Ninja',
         '-DBOARD_CONFIG=esp32s31-core-function-board:' + args.profile,
         '-DNUTTX_APPS_DIR=' + str(root / 'apps'), '-DPython3_EXECUTABLE=' + str(python),
         '-DESP32S31_SDMMC_IDF_INCLUDE=' + str(headers / 'include'),
         '-DNUTTX_PRESERVE_SOURCE=ON', '-DFETCHCONTENT_FULLY_DISCONNECTED=ON',
         '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DMAKE_PARALLELISM=-j' + str(args.jobs)],
        ['cmake', '--build', str(output / 'build'), '--parallel', str(args.jobs)]]
    report = dict(schema=1, status='PREFLIGHT_ONLY', profile=args.profile,
                  workspace=str(root), dependencies=str(deps), commands=commands,
                  snapshot_sha256=sha(HERE / 'snapshot.json'),
                  payload_manifest_sha256=sha(HERE / 'source-sha256.json'),
                  dependency_receipt_sha256=sha(deps / 'receipt.json'),
                  hardware_tested=False)
    if args.dry_run:
        print(json.dumps(report, indent=2))
        return 0
    output.mkdir(parents=True)
    code = 1
    try:
        for command, name in zip(commands, ('configure.log', 'build.log')):
            print(name + ': ' + str(output / name), flush=True)
            with (output / name).open('w') as log:
                code = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT).returncode
            if code:
                print((output / name).read_text(errors='replace')[-10000:], file=sys.stderr)
                break
    finally:
        report.update(status='BUILD_PASS' if code == 0 else 'BUILD_FAILED', returncode=code)
        report['artifacts'] = {name: dict(sha256=sha(output / 'build' / name),
            bytes=(output / 'build' / name).stat().st_size) for name in
            ('nuttx', 'nuttx.bin', 'appfs.img', '.config') if (output / 'build' / name).is_file()}
        if code == 0 and args.profile == 'korvo-openvela-factory':
            manifest = dict(profile=args.profile, board='ESP32-S31 Korvo-1',
                flat_single_image=True, appfs_required=False, kernel_offset=0x2000,
                flash_bytes=16 * 1024 * 1024,
                artifacts={n: report['artifacts'][n] for n in ('nuttx', 'nuttx.bin', '.config')})
            (output / 'build/manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        (output / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'] + ': ' + str(output / 'receipt.json'), flush=True)
    return code


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as exc:
        sys.exit(str(exc))
