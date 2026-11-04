#!/usr/bin/env python3
"""Read back restored SDK bytes, links and generated Python command wrappers."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import tarfile

from offline_sdk import sha, save, PYTHON_ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    package, sdk, output = (p.resolve() for p in (args.package, args.sdk, args.output))
    output.mkdir(parents=True, exist_ok=False)
    meta = json.loads((package / 'sdk.json').read_text())
    restored = json.loads((sdk / 'receipt.json').read_text())
    assert restored['status'] == 'OFFLINE_SDK_RESTORED_AND_COMMANDS_VERIFIED'
    assert restored['package_metadata_sha256'] == sha(package / 'sdk.json')
    report = dict(status='INCOMPLETE', sdk=str(sdk), package=str(package),
                  tools=[], python_files=0, script_sha256=sha(__file__),
                  scope='Every declared installed byte/link plus Python wrapper hash; filesystem POSIX mode equivalence not asserted on DrvFS')
    try:
        for item in meta['tools']:
            archive = package / 'dist' / item['filename']
            assert sha(archive) == item['sha256']
            root = sdk / 'tools' / item['tool'] / item['version']
            strip = item['strip_container_dirs']
            result = dict(tool=item['tool'], regular=0, symlinks=0, hardlinks=0, bytes=0)
            with tarfile.open(archive, 'r|*') as packed:
                for member in packed:
                    parts = PurePosixPath(member.name).parts[strip:]
                    destination = root.joinpath(*parts)
                    if member.isdir():
                        assert destination.is_dir(), str(destination)
                    elif member.issym():
                        assert destination.is_symlink() and os.readlink(destination) == member.linkname
                        result['symlinks'] += 1
                    elif member.islnk():
                        original = root.joinpath(*PurePosixPath(member.linkname).parts[strip:])
                        assert destination.is_file() and sha(destination) == sha(original)
                        result['hardlinks'] += 1
                    else:
                        assert member.isfile() and destination.is_file() and not destination.is_symlink()
                        assert destination.stat().st_size == member.size
                        digest = hashlib.sha256()
                        with packed.extractfile(member) as source:
                            for block in iter(lambda: source.read(1024 * 1024), b''):
                                digest.update(block)
                        assert sha(destination) == digest.hexdigest(), str(destination)
                        result['regular'] += 1
                        result['bytes'] += member.size
            report['tools'].append(result)
            print(json.dumps(result), flush=True)
        assert sha(sdk / meta['constraints']['filename']) == meta['constraints']['sha256']
        assert sha(package / 'python-manifest.json') == meta['python']['manifest_sha256']
        wrappers = {r['name']: r for r in restored['python_command_wrappers']}
        for row in json.loads((package / 'python-manifest.json').read_text()):
            path = sdk / row['path']
            if row['path'].startswith(PYTHON_ROOT + '/bin/') and path.name in wrappers:
                assert sha(path) == wrappers[path.name]['wrapper_sha256']
                path = sdk / PYTHON_ROOT / '.portable/bin' / path.name
                assert row['sha256'] == wrappers[path.name]['original_sha256']
            if row['kind'] == 'symlink':
                assert path.is_symlink() and os.readlink(path) == row['target']
            else:
                assert path.is_file() and sha(path) == row['sha256'], str(path)
            report['python_files'] += 1
        report['python_wrappers'] = len(wrappers)
        report['status'] = 'RESTORED_SDK_BYTES_AND_LINKS_VERIFIED'
    except BaseException as error:
        report['status'] = 'FAILED'
        report['error'] = str(error)
        raise
    finally:
        save(output / 'receipt.json', report)
    print(report['status'])


if __name__ == '__main__':
    main()
