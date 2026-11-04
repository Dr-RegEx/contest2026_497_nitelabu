#!/usr/bin/env python3
"""Package or restore the existing locked S31 Linux SDK without downloading."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import posixpath
import shlex
import shutil
import stat
import subprocess
import sys
import tarfile


ROOT = Path(__file__).resolve().parents[2]
PYTHON_ROOT = 'python_env/idf6.1_py3.10_env'
SYSTEM_PYTHON_LINK = PYTHON_ROOT + '/bin/python3'


def sha(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def save(path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')


def selected_tools(sdk):
    result = []
    for tool in json.loads((sdk / 'tools/tools.json').read_text())['tools']:
        if tool.get('install') != 'always' or not {'all', 'esp32s31'}.intersection(tool['supported_targets']):
            continue
        version, = [v for v in tool['versions'] if v['status'] == 'recommended']
        download = version.get('linux-amd64', version.get('any'))
        if download is None:
            raise RuntimeError('No qualified Linux x86_64 archive for ' + tool['name'])
        result.append(dict(tool=tool['name'], version=version['name'],
                           license=tool['license'], url=download['url'],
                           filename=download.get('rename_dist') or download['url'].rsplit('/', 1)[-1],
                           sha256=download['sha256'], bytes=download['size'],
                           strip_container_dirs=tool.get('strip_container_dirs', 0)))
    return result


def archive_layout(path, *, python=False):
    """Validate paths before an SDK extractor writes, including on DrvFS."""
    paths, folded, links, members = set(), {}, set(), []
    total = 0
    with tarfile.open(path, 'r|*') as archive:
        for member in archive:
            p = PurePosixPath(member.name)
            name = p.as_posix()
            if p.is_absolute() or '..' in p.parts or name in ('', '.') or name in paths:
                raise RuntimeError('Unsafe or duplicate archive path: ' + member.name)
            paths.add(name)
            if not (member.isfile() or member.isdir() or member.issym() or member.islnk()):
                raise RuntimeError('Unsupported archive member: ' + name)
            for index in range(1, len(p.parts) + 1):
                parent = PurePosixPath(*p.parts[:index]).as_posix()
                key = parent.casefold()
                if key in folded and folded[key] != parent:
                    raise RuntimeError('Archive case collision: ' + parent)
                folded[key] = parent
            if member.issym() or member.islnk():
                links.add(name)
                target = member.linkname
                if python and name == SYSTEM_PYTHON_LINK and target == '/usr/bin/python3':
                    pass
                else:
                    normalized = posixpath.normpath(posixpath.join(str(p.parent), target)
                                                   if member.issym() else target)
                    if target.startswith('/') or normalized == '..' or normalized.startswith('../'):
                        raise RuntimeError('Archive link escapes output: ' + name)
                members.append(dict(path=name, link=target, hardlink=member.islnk()))
            total += member.size
    for name in paths:
        if any(p.as_posix() in links for p in PurePosixPath(name).parents):
            raise RuntimeError('Archive writes through link parent: ' + name)
    return dict(entries=len(paths), payload_bytes=total, links=members, case_collisions=0)


def pack(workspace, installed, output):
    sdk = workspace / 's31-reference/tmp/esp-idf-clean'
    lock = json.loads((workspace / 'tools/build-support/host-packages.json').read_text())
    revision = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', 'HEAD'], text=True).strip()
    assert revision == lock['repositories']['s31-reference/tmp/esp-idf-clean']
    tools = selected_tools(sdk)
    for item in tools:
        archive = installed / 'dist' / item['filename']
        if archive.stat().st_size != item['bytes'] or sha(archive) != item['sha256']:
            raise RuntimeError('Cached tool archive differs: ' + str(archive))
    output.mkdir(parents=True, exist_ok=False)
    report = dict(schema=1, status='INCOMPLETE', target='esp32s31', platform='linux-amd64',
                  sdk_revision=revision, tools_json_sha256=sha(sdk / 'tools/tools.json'),
                  tools=tools, python_root=PYTHON_ROOT,
                  host='Ubuntu 22.04 Linux x86_64, /usr/bin/python3 version 3.10',
                  downloads=False, source_sdk_modified=False)
    save(output / 'sdk.json', report)
    try:
        (output / 'dist').mkdir()
        for item in tools:
            target = output / 'dist' / item['filename']
            shutil.copyfile(installed / 'dist' / item['filename'], target)
            if sha(target) != item['sha256']:
                raise RuntimeError('Tool archive copy differs')
            item['layout'] = archive_layout(target)
        constraints = 'espidf.constraints.v6.1.txt'
        shutil.copyfile(installed / constraints, output / constraints)
        report['constraints'] = dict(filename=constraints, sha256=sha(output / constraints))
        rows, excluded = [], []
        venv = installed / PYTHON_ROOT
        with tarfile.open(output / 'python.tar.gz', 'w:gz', compresslevel=1, dereference=False) as archive:
            for path in sorted(venv.rglob('*')):
                relative = path.relative_to(installed).as_posix()
                if '__pycache__' in path.parts or path.suffix == '.pyc' or (
                        path.parent == venv / 'bin' and path.name.startswith('activate')) or path.name == 'Activate.ps1':
                    excluded.append(relative)
                    continue
                info = path.lstat()
                if stat.S_ISDIR(info.st_mode):
                    continue
                row = dict(path=relative, mode=stat.S_IMODE(info.st_mode), bytes=info.st_size)
                if stat.S_ISLNK(info.st_mode):
                    row.update(kind='symlink', target=os.readlink(path))
                elif stat.S_ISREG(info.st_mode):
                    row.update(kind='file', sha256=sha(path))
                else:
                    raise RuntimeError('Unsupported Python environment object: ' + relative)
                archive.add(path, arcname=relative, recursive=False)
                after = path.lstat()
                if (info.st_size, info.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
                    raise RuntimeError('Python environment changed during packaging: ' + relative)
                rows.append(row)
        save(output / 'python-manifest.json', rows)
        save(output / 'python-excluded.json', excluded)
        report['python'] = dict(archive='python.tar.gz', archive_sha256=sha(output / 'python.tar.gz'),
                               manifest_sha256=sha(output / 'python-manifest.json'),
                               layout=archive_layout(output / 'python.tar.gz', python=True))
        # Preserve installed package versions and license declarations; full
        # distribution metadata and license files also remain in the archive.
        command = [str(venv / 'bin/python'), '-c',
            'import importlib.metadata as m,json; '
            'print(json.dumps(sorted([dict(name=d.metadata["Name"],version=d.version,'
            'license=d.metadata.get("License-Expression") or d.metadata.get("License"),'
            'classifiers=[x for x in d.metadata.get_all("Classifier",[]) if x.startswith("License ::")]) '
            'for d in m.distributions()], key=lambda x:x["name"].lower())))']
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        save(output / 'python-distributions.json', json.loads(result.stdout))
        shutil.copyfile(__file__, output / 'offline_sdk.py')
        report['receiver_sha256'] = sha(output / 'offline_sdk.py')
        report['status'] = 'EXISTING_HOST_SDK_PACKAGED'
    except BaseException as error:
        report['error'] = str(error)
        raise
    finally:
        save(output / 'sdk.json', report)
    return report


def restore_python(package, output, report):
    rows = json.loads((package / 'python-manifest.json').read_text())
    with tarfile.open(package / 'python.tar.gz', 'r:gz') as archive:
        archive.extractall(output)
    for row in rows:
        path = output / row['path']
        if row['kind'] == 'symlink':
            assert path.is_symlink() and os.readlink(path) == row['target'], row['path']
        else:
            assert path.is_file() and not path.is_symlink() and sha(path) == row['sha256'], row['path']
    venv = output / PYTHON_ROOT
    # Legacy esptool.py/espefuse.py remove directories ending in /bin from
    # sys.path to avoid shadowing their installed packages. Keep that contract.
    originals = venv / '.portable/bin'
    originals.mkdir(parents=True)
    wrappers = []
    for script in sorted((venv / 'bin').iterdir()):
        if script.is_symlink() or not script.is_file():
            continue
        first = script.open('rb').readline()
        if not first.startswith(b'#!') or b'python' not in first:
            raise RuntimeError('Unrecognized Python environment command: ' + script.name)
        digest = sha(script)
        destination = originals / script.name
        script.rename(destination)
        script.write_text('#!/bin/sh\n'
            's31_sdk_bin=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit\n'
            'exec "$s31_sdk_bin/python" "$s31_sdk_bin/../.portable/bin/"' +
            shlex.quote(script.name) + ' "$@"\n')
        script.chmod(0o755)
        wrappers.append(dict(name=script.name, original_sha256=digest, wrapper_sha256=sha(script)))
    report['python_files_verified'] = len(rows)
    report['python_command_wrappers'] = wrappers


def restore(package, workspace, output):
    if output.exists():
        raise RuntimeError('Use a NEW SDK output directory')
    if platform.system() != 'Linux' or platform.machine() != 'x86_64' or sys.version_info[:2] != (3, 10):
        raise RuntimeError('Qualified SDK requires Linux x86_64 / CPython 3.10')
    sdk = workspace / 's31-reference/tmp/esp-idf-clean'
    meta = json.loads((package / 'sdk.json').read_text())
    if meta['status'] != 'EXISTING_HOST_SDK_PACKAGED' or meta['target'] != 'esp32s31':
        raise RuntimeError('Incomplete or unsupported SDK package')
    if sha(sdk / 'tools/tools.json') != meta['tools_json_sha256']:
        raise RuntimeError('Workspace SDK tools list differs')
    head = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', 'HEAD'], text=True).strip()
    if head != meta['sdk_revision']:
        raise RuntimeError('Workspace SDK revision differs')
    expected = selected_tools(sdk)
    if [{k: r[k] for k in e} for r, e in zip(meta['tools'], expected, strict=True)] != expected:
        raise RuntimeError('SDK archive manifest differs from locked tools.json')
    for item in meta['tools']:
        archive = package / 'dist' / item['filename']
        if sha(archive) != item['sha256'] or archive.stat().st_size != item['bytes']:
            raise RuntimeError('Tool archive integrity failure: ' + item['filename'])
        archive_layout(archive)
    for name, digest in [(meta['constraints']['filename'], meta['constraints']['sha256']),
                         ('python.tar.gz', meta['python']['archive_sha256']),
                         ('python-manifest.json', meta['python']['manifest_sha256'])]:
        if sha(package / name) != digest:
            raise RuntimeError('Python/constraints integrity failure: ' + name)
    archive_layout(package / 'python.tar.gz', python=True)
    # Include cached archives in space planning: future SDK operations remain offline.
    required = sum(t['layout']['payload_bytes'] + t['bytes'] for t in meta['tools'])
    required += meta['python']['layout']['payload_bytes'] + 128 * 1024 * 1024
    parent = output.parent
    parent.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(parent).free < required:
        raise RuntimeError(f'SDK receiver needs at least {required} free bytes')
    output.mkdir()
    report = dict(status='INCOMPLETE', package=str(package), output=str(output),
                  package_metadata_sha256=sha(package / 'sdk.json'),
                  workspace=str(workspace), source_sdk_modified=False, downloads=False,
                  python_command_wrappers=[], commands=[], board_access=False)
    save(output / 'receipt.json', report)
    try:
        (output / 'dist').mkdir()
        for item in meta['tools']:
            shutil.copyfile(package / 'dist' / item['filename'], output / 'dist' / item['filename'])
        shutil.copyfile(package / meta['constraints']['filename'], output / meta['constraints']['filename'])
        restore_python(package, output, report)
        env = os.environ.copy()
        for name in ('PYTHONPATH', 'PYTHONHOME', 'IDF_PYTHON_ENV_PATH', 'IDF_DEACTIVATE_FILE_PATH'):
            env.pop(name, None)
        env.update(IDF_TOOLS_PATH=str(output), PYTHONDONTWRITEBYTECODE='1',
                   PYTHONNOUSERSITE='1', PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin')
        # Reuse the locked SDK's installer. Disable its download function as
        # an executable contract, not merely a claim that the cache is warm.
        code = ('import sys; sys.path.insert(0,sys.argv[1]); import idf_tools; '
                'exec("def no_download(*a, **k):\\n raise RuntimeError(\\"Offline SDK forbids downloads\\")"); '
                'idf_tools.download=no_download; '
                'idf_tools.main(["--idf-path",sys.argv[2],"--non-interactive",'
                '"install","--targets","esp32s31"])')
        command = ['/usr/bin/python3', '-c', code, str(sdk / 'tools'), str(sdk)]
        with (output / 'install.log').open('x') as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=1800)
        report['commands'].append(dict(name='locked SDK offline install', returncode=result.returncode))
        if result.returncode:
            raise RuntimeError('SDK offline install failed; see install.log')
        python = output / PYTHON_ROOT / 'bin/python'
        probes = [('Python dependencies', [str(python), str(sdk / 'tools/idf_tools.py'),
                    '--idf-path', str(sdk), 'check-python-dependencies']),
                  ('esptool command', [str(python.parent / 'esptool'), '--help']),
                  ('pip command', [str(python.parent / 'pip'), '--version'])]
        for index, (name, command) in enumerate(probes):
            result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=90)
            (output / f'probe-{index}.log').write_text(result.stdout + result.stderr)
            report['commands'].append(dict(name=name, returncode=result.returncode))
            if result.returncode:
                raise RuntimeError(name + ' failed')
        report['status'] = 'OFFLINE_SDK_RESTORED_AND_COMMANDS_VERIFIED'
    except BaseException as error:
        report['error'] = str(error)
        raise
    finally:
        save(output / 'receipt.json', report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('pack', 'restore'))
    parser.add_argument('--workspace', type=Path, default=ROOT)
    parser.add_argument('--installed', type=Path, default=Path.home() / '.espressif')
    parser.add_argument('--package', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.operation == 'pack':
        result = pack(args.workspace.resolve(), args.installed.resolve(), args.output.resolve())
    else:
        if args.package is None:
            parser.error('restore requires --package')
        result = restore(args.package.resolve(), args.workspace.resolve(), args.output.resolve())
    print(result['status'])


if __name__ == '__main__':
    main()
