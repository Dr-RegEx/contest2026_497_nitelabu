#!/usr/bin/env python3
"""Install public, pinned S31 build dependencies into a chosen directory."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import tarfile
import urllib.request

HERE = Path(__file__).resolve().parent
HAL_REV = '290edc31b50decca660c1a11ce3506fd9b2e1e27'


def run(*cmd):
    subprocess.run([str(x) for x in cmd], check=True)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('workspace', type=Path, help='repo checkout containing nuttx and apps')
    p.add_argument('--deps', type=Path, help='default: WORKSPACE/.s31-deps')
    p.add_argument('--cache', type=Path, help='optional archive cache; hashes are always verified')
    a = p.parse_args()
    root = a.workspace.resolve(strict=True)
    if not (root / 'nuttx/CMakeLists.txt').is_file():
        p.error('workspace must contain nuttx/CMakeLists.txt')
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        p.error('The validated public build requires Linux x86_64 (Ubuntu 22.04).')
    deps = (a.deps or root / '.s31-deps').resolve()
    deps.mkdir(parents=True, exist_ok=True)
    receipt = deps / 'receipt.json'
    receipt.unlink(missing_ok=True)
    hal = deps / 'esp-hal-3rdparty'
    if not (hal / '.git').exists():
        if hal.exists():
            p.error('Incomplete HAL directory: use a new --deps directory')
        run('git', 'init', hal)
        run('git', '-C', hal, 'remote', 'add', 'origin', 'https://github.com/78/esp-hal-3rdparty.git')
        run('git', '-C', hal, 'fetch', '--depth=1', 'origin', HAL_REV)
        run('git', '-C', hal, 'checkout', '--detach', HAL_REV)
    head = subprocess.check_output(['git', '-C', str(hal), 'rev-parse', 'HEAD'], text=True).strip()
    if head != HAL_REV:
        raise RuntimeError('Existing HAL has a different revision; use a new --deps directory')
    run('git', '-C', hal, 'submodule', 'update', '--init', '--recursive', '--depth=1', '--jobs=8')
    for target, patch in [(hal, 'hal.patch'), (hal / 'components/mbedtls/mbedtls', 'hal-mbedtls.patch')]:
        path = HERE / 'dependencies' / patch
        check = subprocess.run(['git', '-C', str(target), 'apply', '--reverse', '--check', str(path)],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if check.returncode:
            run('git', '-C', target, 'apply', '--check', path)
            run('git', '-C', target, 'apply', '--whitespace=nowarn', path)
    run('python3', '-m', 'venv', deps / 'venv')
    run(deps / 'venv/bin/python3', '-m', 'pip', 'install', '-r', HERE / 'dependencies/requirements.txt')
    lock = json.loads((HERE / 'dependencies/toolchain.json').read_text())['linux-amd64']
    cache = (a.cache or deps / 'downloads').resolve()
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / lock['url'].rsplit('/', 1)[-1]
    if not archive.exists():
        partial = archive.with_suffix(archive.suffix + '.partial')
        urllib.request.urlretrieve(lock['url'], partial)
        if sha(partial) != lock['sha256']:
            raise RuntimeError('Downloaded toolchain checksum mismatch')
        partial.replace(archive)
    if sha(archive) != lock['sha256']:
        raise RuntimeError('Cached toolchain checksum mismatch')
    if not (deps / 'riscv32-esp-elf/bin/riscv32-esp-elf-gcc').is_file():
        # Archive authenticated against the published SHA256 lock above.
        with tarfile.open(archive) as source:
            source.extractall(deps)
    packages = json.loads((HERE / 'tools/build-support/host-packages.json').read_text())['packages']
    for package in packages:
        path = HERE / 'dependencies/host-packages' / Path(package['file']).name
        if sha(path) != package['sha256']:
            raise RuntimeError('Host package checksum mismatch: ' + path.name)
        run('dpkg-deb', '-x', path, deps / 'sysroot')
    hashes = {n: sha(HERE / 'dependencies' / n) for n in
              ('hal.patch', 'hal-mbedtls.patch', 'requirements.txt', 'toolchain.json')}
    versions = subprocess.check_output([str(deps / 'venv/bin/python3'), '-m', 'pip', 'freeze'], text=True)
    receipt.write_text(json.dumps(dict(schema=1, status='PUBLIC_DEPENDENCIES_READY',
        hal_revision=HAL_REV, toolchain_sha256=lock['sha256'], inputs=hashes,
        python_packages=versions.splitlines(), dependency_root=str(deps)), indent=2) + '\n')
    print('Public dependencies ready: ' + str(deps))


if __name__ == '__main__':
    main()
