#!/usr/bin/env python3
"""Restore and build the original SMP digit demo in a separate source directory."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from entrypoint_support import sha, fresh_output, clean_env


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace', type=Path, required=True)
    p.add_argument('--deps', type=Path, required=True)
    p.add_argument('--output', required=True)
    p.add_argument('--jobs', type=int, default=4)
    p.add_argument('--dry-run', action='store_true')
    a = p.parse_args()
    root, deps = a.workspace.resolve(strict=True), a.deps.resolve(strict=True)
    output = fresh_output(a.output, (root, deps, HERE.parents[1]))
    if a.jobs < 1:
        p.error('--jobs must be positive')
    if json.loads((deps / 'receipt.json').read_text())['status'] != 'PUBLIC_DEPENDENCIES_READY':
        p.error('Prepare public dependencies first')
    for rel, expected in json.loads((HERE / 'sha256.json').read_text()).items():
        if sha(HERE / rel) != expected:
            raise ValueError('Digit source checksum mismatch: ' + rel)
    if a.dry_run:
        print('Restore original digit sources to ' + str(output) + '; no files written')
        return 0
    output.mkdir(parents=True)
    source = output / 'source'
    source.mkdir()
    for layer in ('nuttx', 'apps'):
        subprocess.run(['rsync', '-a', '--exclude=.git', '--exclude=__pycache__',
            '--exclude=*.o', '--exclude=*.o.d', '--exclude=*.obj.d',
            str(root / layer) + '/', str(source / layer) + '/'], check=True)
    for layer in ('external', 'frameworks', 'packages', 'tests', 'vendor', 'build', 'prebuilts'):
        (source / layer).symlink_to(root / layer, target_is_directory=True)
    for src in (HERE / 'overlay').rglob('*'):
        if not src.is_file():
            continue
        dst = source / src.relative_to(HERE / 'overlay')
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.is_symlink():
            dst.unlink()
        shutil.copy2(src, dst)
    hal = output / 'hal'
    subprocess.run(['git', 'clone', '--shared', '--no-checkout', str(deps / 'esp-hal-3rdparty'), str(hal)], check=True)
    subprocess.run(['git', '-C', str(hal), 'checkout', '--detach', '290edc31b50decca660c1a11ce3506fd9b2e1e27'], check=True)
    # Use the same fixed submodule sources; only the top-level HAL adapter differs.
    modules = subprocess.check_output(['git', '-C', str(hal), 'config', '--file', '.gitmodules',
        '--get-regexp', 'submodule\..*\.path'], text=True)
    for line in modules.splitlines():
        rel = line.split(' ', 1)[1]
        dst = hal / rel
        if dst.exists():
            dst.rmdir()  # A fresh gitlink checkout is empty.
        dst.symlink_to(deps / 'esp-hal-3rdparty' / rel, target_is_directory=True)
    subprocess.run(['git', '-C', str(hal), 'apply', '--check', str(HERE / 'hal.patch')], check=True)
    subprocess.run(['git', '-C', str(hal), 'apply', '--whitespace=nowarn', str(HERE / 'hal.patch')], check=True)
    env = clean_env()
    env['PATH'] = os.pathsep.join([str(deps / 'venv/bin'), str(deps / 'riscv32-esp-elf/bin'),
        str(root / 'prebuilts/tools/linux/x86_64'), '/usr/local/bin', '/usr/bin', '/bin'])
    env['LD_LIBRARY_PATH'] = str(deps / 'sysroot/usr/lib/x86_64-linux-gnu')
    env['ESP_HAL_3RDPARTY_LOCAL'] = str(hal)
    commands = [
        ['cmake', '-S', str(source / 'nuttx'), '-B', str(output / 'build'), '-G', 'Ninja',
         '-DBOARD_CONFIG=esp32s31-core-function-board:korvo-vision-smp',
         '-DNUTTX_APPS_DIR=' + str(source / 'apps'),
         '-DPython3_EXECUTABLE=' + str(deps / 'venv/bin/python3'),
         '-DFETCHCONTENT_FULLY_DISCONNECTED=ON', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'],
        ['cmake', '--build', str(output / 'build'), '--parallel', str(a.jobs)]]
    report = dict(schema=1, profile='korvo-digit-rebuild', overlay_sha256=sha(HERE / 'sha256.json'),
                  commands=commands, hardware_tested=False, exact_historical_binary_claimed=False)
    code = 1
    try:
        for cmd, name in zip(commands, ('configure.log', 'build.log')):
            with (output / name).open('w') as log:
                code = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT).returncode
            if code:
                print((output / name).read_text(errors='replace')[-10000:], file=sys.stderr)
                break
    finally:
        report.update(status='BUILD_PASS' if code == 0 else 'BUILD_FAILED', returncode=code)
        report['artifacts'] = {n: dict(sha256=sha(output / 'build' / n), bytes=(output / 'build' / n).stat().st_size)
            for n in ('nuttx', 'nuttx.bin', '.config') if (output / 'build' / n).is_file()}
        (output / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'] + ': ' + str(output / 'receipt.json'))
    return code


if __name__ == '__main__':
    raise SystemExit(main())
