#!/usr/bin/env python3
"""Apply only to the exact clean baseline; preflight every project before writing."""
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys

ws = pathlib.Path(__file__).resolve().parent

def git(target, *args):
    return subprocess.check_output(['git', '-C', str(target), *args], stderr=subprocess.STDOUT)

def main():
    if len(sys.argv) != 2:
        raise ValueError('Usage: apply-openvela-snapshot.sh /path/to/openvela-workspace')
    root = pathlib.Path(sys.argv[1]).resolve(strict=True)
    projects = json.loads((ws / 'snapshot.json').read_text())
    # Check payload integrity before any mutation.
    for name, expected in json.loads((ws / 'source-sha256.json').read_text()).items():
        if hashlib.sha256((ws / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f'Snapshot checksum mismatch: {name}')
    for p in projects:
        target = root / p['path']
        if git(target, 'rev-parse', 'HEAD').decode().strip() != p['revision']:
            raise ValueError(f"Wrong baseline: {p['path']}; repo sync the pinned manifest first")
        if git(target, 'status', '--porcelain', '--untracked-files=no', '--ignore-submodules=all').strip():
            raise ValueError(f"Modified tracked files: {p['path']}; use a fresh workspace")
        if p['patch']:
            git(target, 'apply', '--check', str(ws / 'patches' / p['patch']))
        for name in p['files']:
            dest = target / name
            if dest.exists() or dest.is_symlink():
                raise ValueError(f'New source already exists: {dest}')
    for p in projects:
        target = root / p['path']
        if p['patch']:
            git(target, 'apply', '--whitespace=nowarn', str(ws / 'patches' / p['patch']))
        for name in p['files']:
            dest = target / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ws / 'openvela-untracked' / p['path'] / name, dest)
        print(f"Applied {p['path']}")
    print('Snapshot applied. Next: workspace/setup-dependencies.sh and workspace/build.sh')

if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        sys.exit(str(exc) + (getattr(exc, 'output', b'').decode(errors='replace')))
