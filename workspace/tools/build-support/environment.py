#!/usr/bin/env python3
"""Prepare and probe existing S31 host dependencies in a new output directory."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess


SUPPORT = Path(__file__).resolve().parent
ROOT = SUPPORT.parents[1]


def kconfig_frontends(python, directory):
    """Bind fresh-output launchers to this workspace's actual interpreter.

    pip's existing entry points keep the absolute path of the machine on which
    the venv was created. Never rewrite that venv or use those stale shebangs.
    """
    directory.mkdir()
    result = {}
    for name in ('olddefconfig', 'savedefconfig', 'setconfig', 'menuconfig', 'guiconfig'):
        path = directory / name
        path.write_text('#!/bin/sh\nexec ' + shlex.quote(str(python)) +
                        ' -m ' + name + ' "$@"\n')
        path.chmod(0o755)
        result[name] = {'path': str(path), 'sha256': sha(path)}
    return result


def sha(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def activate_sdk(sdk, python, output, env, report):
    """Select S31 without changing the user's path-indexed IDF installation.

    Tools and Python remain external prerequisites. The build-local view owns
    only target metadata and a copy of the existing offline constraints.
    Never evaluate export commands from a failed activation: export.sh in the
    locked SDK can mask activate.py's failure with a successful empty eval.
    """
    sdk = Path(sdk).resolve()
    installed = Path(env.get('IDF_TOOLS_PATH') or Path.home() / '.espressif').resolve()
    for name in ('IDF_PATH', 'IDF_PATH_OLD', 'ESP_IDF_VERSION',
                 'IDF_PYTHON_ENV_PATH', 'IDF_DEACTIVATE_FILE_PATH',
                 'ESP_IDF_EXPORT_DEBUG'):
        env.pop(name, None)
    metadata = subprocess.run([str(python), '-c',
        'import json,sys; sys.path.insert(0,sys.argv[1]); import idf_tools; '
        'idf_tools.g.idf_path=sys.argv[2]; '
        'print(json.dumps({"version":idf_tools.get_idf_version(),'
        '"id":idf_tools.active_repo_id()}))', str(sdk / 'tools'), str(sdk)],
        env=env, capture_output=True, text=True, timeout=90)
    log = output / 'toolchain-export.log'
    log.write_text(metadata.stderr)
    if metadata.returncode:
        raise RuntimeError(f'ESP-IDF metadata query failed ({metadata.returncode})')
    identity = json.loads(metadata.stdout)
    constraints = installed / ('espidf.constraints.v' + identity['version'] + '.txt')
    for path in (installed / 'tools', installed / 'python_env', constraints):
        if not path.exists():
            raise RuntimeError('Missing existing ESP-IDF dependency: ' + str(path))
    view = output / 'idf-tools'
    view.mkdir()
    for name in ('tools', 'python_env'):
        (view / name).symlink_to(installed / name, target_is_directory=True)
    shutil.copy2(constraints, view / constraints.name)
    selected = {'idfInstalled': {identity['id']: {
        'version': identity['version'], 'path': str(sdk),
        'targets': ['esp32s31'], 'features': ['core']}}}
    record = view / 'idf-env.json'
    record.write_text(json.dumps(selected, indent=2) + '\n')
    env['IDF_TOOLS_PATH'] = str(view)
    report['sdk_environment'] = dict(
        sdk=str(sdk), installed_tools=str(installed), view=str(view),
        version=identity['version'], targets=['esp32s31'],
        metadata_sha256=sha(record), constraints_sha256=sha(constraints),
        activate_sha256=sha(sdk / 'tools/activate.py'))
    activated = subprocess.run(
        [str(python), str(sdk / 'tools/activate.py'), '--export', '--shell', 'bash'],
        env=env, capture_output=True, timeout=90)
    with log.open('ab') as stream:
        stream.write(activated.stderr)
    report['sdk_environment']['activation_returncode'] = activated.returncode
    if activated.returncode:
        raise RuntimeError(f'Existing ESP-IDF activation failed ({activated.returncode}); '
                           'see toolchain-export.log')
    exported = subprocess.run(
        ['bash', '--noprofile', '--norc', '-c', 'eval "$1" >&2 && env -0',
         's31-env', os.fsdecode(activated.stdout)],
        env=env, capture_output=True, timeout=90)
    with log.open('ab') as stream:
        stream.write(exported.stderr)
    report['sdk_environment']['export_returncode'] = exported.returncode
    if exported.returncode:
        raise RuntimeError(f'Existing ESP-IDF environment export failed ({exported.returncode})')
    # Both shell commands and the full environment remain in memory only.
    for entry in exported.stdout.split(b'\0'):
        if b'=' in entry:
            name, value = os.fsdecode(entry).split('=', 1)
            env[name] = value
    if Path(env.get('IDF_PATH', '')).resolve() != sdk:
        raise RuntimeError('Activated ESP-IDF path differs from the selected workspace')


def prepare(root, output, *, copy_source=False):
    """Return environment/receipt; never install packages or modify source trees."""
    root, output = Path(root).resolve(), Path(output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {'schema': 1, 'status': 'INCOMPLETE', 'checks': [], 'packages': [],
              'tools': {}, 'scope': 'Existing Linux x86_64 host dependencies; not a standalone SDK',
              'script_sha256': sha(__file__)}
    env = os.environ.copy()
    for name in ('PYTHONPATH', 'PYTHONHOME', 'VELA_ORIGINAL_PATH',
                 'VELA_GLOBAL_BUILD_PATHS', 'VELA_GLOBAL_PYPATHS'):
        env.pop(name, None)
    env.update(PYTHONNOUSERSITE='1', PYTHONDONTWRITEBYTECODE='1')

    def run(name, command, *, stdin=None):
        result = subprocess.run([str(x) for x in command], env=env, cwd=output,
                                input=stdin, capture_output=True, text=True, timeout=90)
        record = {'name': name, 'command': [str(x) for x in command],
                  'returncode': result.returncode, 'stdout': result.stdout[-6000:],
                  'stderr': result.stderr[-6000:]}
        report['checks'].append(record)
        if result.returncode:
            raise RuntimeError(f'{name} failed (exit {result.returncode}); see receipt.json')
        return result.stdout

    def tool(name):
        found = shutil.which(name, path=env['PATH'])
        if found is None:
            raise RuntimeError(f'Missing host executable: {name}')
        path = Path(found)
        report['tools'][name] = {'path': str(path), 'realpath': str(path.resolve()),
                                 'sha256': sha(path)}
        return str(path)

    try:
        if platform.system() != 'Linux' or platform.machine() != 'x86_64':
            raise RuntimeError('This host dependency set requires Linux x86_64')
        required = ['s31-reference/tmp/esp-idf-clean/tools/activate.py',
                    's31-reference/tmp/esp-idf-clean/tools/idf_tools.py',
                    's31-reference/.venv-nuttx/bin/python3',
                    's31-reference/deps/esp-hal-3rdparty/components',
                    's31-reference/deps/esp-hal-3rdparty/nuttx',
                    'openvela-dev/prebuilts/tools/linux/x86_64/aidl',
                    'openvela-dev/prebuilts/tools/rust/bin/jidl/jidl_gen_cpp']
        required += ['openvela-dev/' + name for name in
                     ('nuttx', 'apps', 'external', 'frameworks', 'packages', 'tests', 'vendor', 'build')]
        missing = [p for p in required if not (root / p).exists()]
        if missing:
            raise RuntimeError('Missing workspace dependencies: ' + ', '.join(missing))
        for name in ('bash', 'cmake', 'ninja', 'make', 'perl', 'xxd', 'dpkg-deb', 'ldd'):
            tool(name)
        if copy_source:
            tool('rsync')
        lock = json.loads((SUPPORT / 'host-packages.json').read_text())
        report['lock_sha256'] = sha(SUPPORT / 'host-packages.json')
        packages = lock['packages']
        # Check every package before extracting any of them.
        for package in packages:
            path = root / package['file']
            if not path.is_file() or sha(path) != package['sha256']:
                raise RuntimeError(f'Missing or changed locked host package: {package["file"]}')
            report['packages'].append(package)
        sysroot = output / 'sysroot'
        for package in packages:
            run('extract ' + Path(package['file']).name,
                [report['tools']['dpkg-deb']['path'], '-x', root / package['file'], sysroot])
        libraries = sysroot / 'usr/lib/x86_64-linux-gnu'
        report['library_directory'] = str(libraries)
        report['runtime_libraries'] = {
            p.name: {'sha256': sha(p), 'bytes': p.stat().st_size}
            for p in libraries.iterdir() if p.is_file()}
        # Use only the prepared libraries plus the system loader defaults.
        env['LD_LIBRARY_PATH'] = str(libraries)
        env['ESP_HAL_3RDPARTY_LOCAL'] = str(root / 's31-reference/deps/esp-hal-3rdparty')
        python = root / 's31-reference/.venv-nuttx/bin/python3'
        activate_sdk(root / 's31-reference/tmp/esp-idf-clean', python, output, env, report)
        env['LD_LIBRARY_PATH'] = str(libraries)
        frontends = output / 'kconfig-bin'
        report['kconfig_frontends'] = kconfig_frontends(python, frontends)
        env['PATH'] = os.pathsep.join([
            str(frontends),
            str(SUPPORT / 'offline-bin'),
            str(root / 'openvela-dev/prebuilts/tools/linux/x86_64'),
            str(root / 's31-reference/.venv-nuttx/bin'), env['PATH']])
        for name, flags in [('cmake', ['--version']), ('ninja', ['--version']),
                            ('riscv32-esp-elf-gcc', ['--version']),
                            ('riscv32-esp-elf-g++', ['--version']),
                            ('riscv32-esp-elf-ar', ['--version']),
                            ('riscv32-esp-elf-ranlib', ['--version'])]:
            run(name, [tool(name), *flags])
        for name in ('olddefconfig', 'savedefconfig', 'setconfig', 'menuconfig', 'guiconfig',
                     'genromfs', 'aidl', 'git'):
            tool(name)
        for relative, expected in lock['repositories'].items():
            actual = run(relative + ' revision',
                         [tool('git'), '-C', root / relative, 'rev-parse', 'HEAD']).strip()
            if actual != expected:
                raise RuntimeError(f'Locked repository revision differs: {relative}')
        run('Python Kconfig', [python, '-c',
            'import sys,kconfiglib,importlib.metadata; '
            'print(sys.version); print(kconfiglib.__file__); '
            'print(importlib.metadata.version("kconfiglib"))'])
        kconfig = output / 'kconfig-probe'
        kconfig.mkdir()
        (kconfig / 'Kconfig').write_text(
            'mainmenu "S31 environment probe"\n'
            'config S31_PROBE_FLAG\n  bool "Flag"\n  default y\n'
            'config S31_PROBE_TEXT\n  string "Text"\n  default "baseline"\n'
            'config S31_PROBE_NUMBER\n  int "Number"\n  default 7\n')
        config = kconfig / '.config'
        config.write_text('# CONFIG_S31_PROBE_FLAG is not set\n'
                          'CONFIG_S31_PROBE_TEXT="relocated workspace"\n'
                          'CONFIG_S31_PROBE_NUMBER=8\n')
        previous = env.get('KCONFIG_CONFIG')
        env['KCONFIG_CONFIG'] = str(config)
        try:
            run('Kconfig olddefconfig', [tool('olddefconfig'), kconfig / 'Kconfig'])
            run('Kconfig setconfig', [tool('setconfig'), 'S31_PROBE_NUMBER=9',
                '--kconfig', kconfig / 'Kconfig'])
            run('Kconfig savedefconfig', [tool('savedefconfig'), '--out',
                kconfig / 'defconfig', '--kconfig', kconfig / 'Kconfig'])
        finally:
            if previous is None:
                env.pop('KCONFIG_CONFIG', None)
            else:
                env['KCONFIG_CONFIG'] = previous
        expected_config = {'# CONFIG_S31_PROBE_FLAG is not set',
                           'CONFIG_S31_PROBE_TEXT="relocated workspace"',
                           'CONFIG_S31_PROBE_NUMBER=9'}
        for path in (config, kconfig / 'defconfig'):
            if not expected_config.issubset(set(path.read_text().splitlines())):
                raise RuntimeError('Kconfig frontend changed probe configuration: ' + str(path))
        run('RISC-V C++ compile', [tool('riscv32-esp-elf-g++'), '-x', 'c++',
            '-march=rv32imafc', '-mabi=ilp32f', '-c', '-o', output / 'compiler-probe.o', '-'],
            stdin='extern "C" int s31_host_probe(int value) { return value + 1; }\n')
        aidl_root = root / 'openvela-dev/external/android/frameworks/native/libs/binder/aidl'
        generated = output / 'aidl'
        generated.mkdir()
        run('AIDL generation', [tool('aidl'), '--lang=cpp', '-h' + str(generated),
            '-o' + str(generated), '-I' + str(aidl_root),
            aidl_root / 'android/content/pm/PackageChangeEvent.aidl'])
        expected = [generated / 'android/content/pm' / (name + ext) for name, ext in
                    [('BnPackageChangeEvent', '.h'), ('BpPackageChangeEvent', '.h'),
                     ('PackageChangeEvent', '.h'), ('PackageChangeEvent', '.cpp')]]
        if any(not p.is_file() for p in expected):
            raise RuntimeError('AIDL exited successfully but generated files are missing')
        jidl = root / 'openvela-dev/prebuilts/tools/rust/bin/jidl/jidl_gen_cpp'
        run('JIDL startup', [jidl, '--help'])
        report['tools']['jidl_gen_cpp'] = {'path': str(jidl), 'sha256': sha(jidl)}
        for name, binary in [('aidl', tool('aidl')), ('jidl', jidl),
                             ('genromfs', tool('genromfs'))]:
            loaded = run(name + ' libraries', [tool('ldd'), binary])
            if 'not found' in loaded:
                raise RuntimeError(f'{name} has unresolved shared libraries')
        romfs = output / 'romfs'
        romfs.mkdir()
        (romfs / 'probe.txt').write_text('S31 host dependency probe\n')
        run('ROMFS generation', [tool('genromfs'), '-f', output / 'probe.img',
                                 '-d', romfs, '-V', 's31-probe'])
        if not (output / 'probe.img').read_bytes().startswith(b'-rom1fs-'):
            raise RuntimeError('genromfs did not produce a ROMFS image')
        report['generated'] = {str(p.relative_to(output)): sha(p) for p in
                               expected + [output / 'compiler-probe.o', output / 'probe.img',
                                           config, kconfig / 'defconfig']}
        report['status'] = 'HOST_DEPENDENCIES_READY'
        return env, report
    except Exception as error:
        report['status'] = 'FAILED'
        report['error'] = str(error)
        raise
    finally:
        (output / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path, help='New probe directory')
    args = parser.parse_args()
    try:
        _, report = prepare(ROOT, args.output)
    except (RuntimeError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + '\n')
    print(json.dumps({'status': report['status'], 'receipt': str(args.output / 'receipt.json')}))


if __name__ == '__main__':
    main()
