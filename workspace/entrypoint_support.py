"""Small local-only validation shared by the platform front doors."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath

HERE = Path(__file__).resolve().parent
LAYERS = ('nuttx', 'apps', 'external', 'frameworks', 'packages', 'tests', 'vendor', 'build')
BOARD = 'openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board'


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text())


def local_path(value):
    if '://' in value or value.startswith('git@'):
        raise ValueError('Only local filesystem paths are accepted; no downloads')
    return Path(value).expanduser().absolute()


def contained(root, relative):
    rel = PurePosixPath(relative)
    if rel.is_absolute() or '..' in rel.parts or rel.as_posix() != relative or relative in ('', '.'):
        raise ValueError('Unsafe relative path: ' + relative)
    path = root / relative
    if not path.resolve().is_relative_to(root.resolve()):
        raise ValueError('Path escapes its input directory: ' + relative)
    return path


def fresh_output(value, inputs):
    path = local_path(value)
    if path.exists() or path.is_symlink():
        raise ValueError('Output must not exist: ' + str(path))
    path = path.resolve()
    for source in inputs:
        source = Path(source).resolve()
        if path.is_relative_to(source) or source.is_relative_to(path):
            raise ValueError('Output overlaps an input/source tree: ' + str(source))
    return path


def require_files(root, names):
    for name in names:
        path = contained(root, name)
        if not path.is_file():
            raise ValueError('Missing local file: ' + str(path))


def platform_layout(root):
    if (root / 'nuttx').is_dir() and not (root / 'openvela-dev/nuttx').is_dir():
        raise ValueError('Flattened checkout detected. See REPRODUCE.md: place the checkout at platform-root/openvela-dev; do not reuse .s31-deps')
    for layer in LAYERS:
        if not (root / 'openvela-dev' / layer).is_dir():
            raise ValueError('Missing canonical source layer: openvela-dev/' + layer)
    require_files(root, ['tools/tests/korvo-adc-keys/build.py',
        'tools/build-support/environment.py', 'tools/build-support/host-packages.json',
        'tools/build-support/offline-bin/git', 'tools/build-support/offline-bin/curl',
        'tools/build-support/offline-bin/wget',
        's31-reference/tmp/esp-idf-clean/tools/activate.py',
        's31-reference/tmp/esp-idf-clean/tools/idf_tools.py',
        'openvela-dev/prebuilts/tools/linux/x86_64/aidl',
        'openvela-dev/prebuilts/tools/rust/bin/jidl/jidl_gen_cpp'])
    # The product explicitly permits the venv's /usr/bin/python3 link.
    if not (root / 's31-reference/.venv-nuttx/bin/python3').is_file():
        raise ValueError('Missing product Python environment')
    for name in ('components', 'nuttx'):
        if not (root / 's31-reference/deps/esp-hal-3rdparty' / name).is_dir():
            raise ValueError('Missing locked HAL contents: ' + name)


def clean_env():
    env = os.environ.copy()
    env.update(PYTHONDONTWRITEBYTECODE='1', PYTHONNOUSERSITE='1')
    for name in ('PYTHONPATH', 'PYTHONHOME'):
        env.pop(name, None)
    return env
