#!/usr/bin/env python3
"""Restore a local product export into a NEW directory, without downloading.

Never point this at an existing workspace. The restored source tree includes
actual dirty bytes; Git metadata is imported only for the locked dependencies,
using no-checkout clones and read-tree (neither operation replaces source).
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import shutil
import subprocess
import tarfile
import tempfile

DEPENDENCIES = {'s31-reference/deps/esp-hal-3rdparty': 'esp-hal.bundle',
                's31-reference/tmp/esp-idf-clean': 'esp-idf.bundle'}
SYSTEM_LINK = 's31-reference/.venv-nuttx/bin/python3'


def require_case_sensitive(parent):
    """Test the new receiver's filesystem before cloning or copying payloads."""
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.s31-case-probe-', dir=parent) as temporary:
        directory = Path(temporary)
        (directory / 'name').write_bytes(b'lower')
        try:
            with (directory / 'NAME').open('xb') as stream: stream.write(b'upper')
        except FileExistsError:
            raise RuntimeError('Receiver filesystem is case-insensitive; choose a Linux filesystem or a prepared case-sensitive directory') from None
        if (directory / 'name').read_bytes() != b'lower' or (directory / 'NAME').read_bytes() != b'upper':
            raise RuntimeError('Receiver filesystem does not preserve distinct file names')


def sha(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        while data := stream.read(1024 * 1024): result.update(data)
    return result.hexdigest()


def source_manifest(package):
    receipt = json.loads((package / 'receipt.json').read_text())
    if receipt['status'] != 'EXPORTED_REQUIRES_VERIFICATION':
        raise RuntimeError('Incomplete source export')
    for name, key in (('source.tar.gz', 'archive_sha256'), ('manifest.jsonl', 'manifest_sha256')):
        if sha(package / name) != receipt[key]: raise RuntimeError('Changed export: ' + name)
    rows = [json.loads(line) for line in (package / 'manifest.jsonl').read_text().splitlines()]
    paths = set()
    links = set()
    for row in rows:
        name = row['path']; path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or path.as_posix() != name or name in ('.', '') or name in paths:
            raise RuntimeError('Unsafe/duplicate source path')
        paths.add(name)
        if row['kind'] not in ('file', 'symlink') or not 0 <= row['mode'] <= 0o777:
            raise RuntimeError('Unsupported source type/mode')
        if row['kind'] == 'symlink':
            links.add(name)
            target = row['target']
            if name == SYSTEM_LINK and target == '/usr/bin/python3': continue
            combined = posixpath.normpath(posixpath.join(str(path.parent), target))
            if target.startswith('/') or combined == '..' or combined.startswith('../'):
                raise RuntimeError('Source link escapes workspace')
    if len(rows) != receipt['files']: raise RuntimeError('Manifest count differs')
    for name in paths:
        if any(parent.as_posix() in links for parent in PurePosixPath(name).parents):
            raise RuntimeError('Archive writes through a symbolic-link parent')
    return receipt, rows


def restore_source(package, output):
    receipt, rows = source_manifest(package)
    output.mkdir(parents=True, exist_ok=False)
    links = []
    with tarfile.open(package / 'source.tar.gz', 'r|gz') as tar:
        for number, (row, member) in enumerate(zip(rows, tar, strict=True), 1):
            if member.name != 'workspace/' + row['path']:
                raise RuntimeError('Archive/manifest order or name differs')
            destination = output / row['path']
            if row['kind'] == 'symlink':
                if not member.issym() or member.linkname != row['target']:
                    raise RuntimeError('Archive link differs')
                links.append((destination, row['target']))
            else:
                if not member.isfile() or member.size != row['bytes']:
                    raise RuntimeError('Archive type/size differs')
                destination.parent.mkdir(parents=True, exist_ok=True)
                digest = hashlib.sha256()
                with tar.extractfile(member) as source, destination.open('xb') as target:
                    while data := source.read(1024 * 1024):
                        target.write(data); digest.update(data)
                if digest.hexdigest() != row['sha256']:
                    raise RuntimeError('Archive content differs: ' + row['path'])
                destination.chmod(row['mode'])
            if number % 20000 == 0: print('restored', number, 'of', len(rows), flush=True)
    # Create links only after all file content has been validated and written.
    for path, target in links:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.symlink_to(target)
    return dict(files=len(rows), regular_files=len(rows) - len(links), symlinks=len(links),
                archive_sha256=receipt['archive_sha256'], manifest_sha256=receipt['manifest_sha256'])


def restore(package, output):
    if output.exists(): raise RuntimeError('Use a NEW receiver directory')
    require_case_sensitive(output.parent)
    if shutil.which('git') is None: raise RuntimeError('Git is required to restore version metadata')
    metadata = json.loads((package / 'dependencies.json').read_text())
    if metadata['schema'] != 1 or set(metadata['repositories']) != set(DEPENDENCIES):
        raise RuntimeError('Unexpected dependency inventory')
    for name, item in metadata['repositories'].items():
        if item['bundle'] != DEPENDENCIES[name] or sha(package / item['bundle']) != item['bundle_sha256']:
            raise RuntimeError('Dependency bundle differs: ' + name)
    output.mkdir(parents=True)
    report = dict(status='INCOMPLETE', package=str(package), output=str(output),
                  receiver_sha256=sha(Path(__file__)), commands=[], board_access=False,
                  host_sdk_delivered=False, built=False)
    (output / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    def run(command, name):
        log = output / (name + '.log')
        with log.open('x') as stream:
            result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, timeout=600)
        report['commands'].append(dict(command=command, returncode=result.returncode, log=str(log)))
        if result.returncode: raise RuntimeError(name + ' failed; preserve receiver')
        return log.read_text()
    try:
        clones = {}
        for index, (relative, item) in enumerate(metadata['repositories'].items()):
            clone = output / ('git-import-' + str(index))
            run(['git', 'clone', '--no-checkout', '--', str(package / item['bundle']), str(clone)],
                'clone-' + str(index))
            actual = run(['git', '-C', str(clone), 'rev-parse', 'HEAD'], 'head-' + str(index)).strip()
            if actual != item['head']: raise RuntimeError('Restored dependency HEAD differs')
            clones[relative] = clone
        workspace = output / 'workspace'
        report['source'] = restore_source(package / 'source', workspace)
        locks = json.loads((workspace / 'tools/build-support/host-packages.json').read_text())['repositories']
        for index, (relative, item) in enumerate(metadata['repositories'].items()):
            if item['head'] != locks[relative]: raise RuntimeError('Bundle and source dependency locks differ')
            clone = clones[relative]
            destination = workspace / relative
            (clone / '.git').rename(destination / '.git')
            clone.rmdir()
            run(['git', '-C', str(destination), 'read-tree', 'HEAD'], 'index-' + str(index))
            run(['git', '-C', str(destination), 'remote', 'remove', 'origin'], 'remove-local-remote-' + str(index))
        report['status'] = 'SOURCE_AND_LOCKED_GIT_METADATA_RESTORED'
    except BaseException as error:
        report['error'] = type(error).__name__ + ': ' + str(error)
        raise
    finally:
        (output / 'receipt.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    os.umask(0o077)
    result = restore(args.package.resolve(), args.output.resolve())
    print(result['status'])


if __name__ == '__main__':
    main()
