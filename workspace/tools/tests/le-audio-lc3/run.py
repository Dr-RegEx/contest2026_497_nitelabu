#!/usr/bin/env python3
"""Compile the existing project's LC3 source and exercise real codec frames."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
lib = root / 'openvela-dev/external/liblc3/liblc3'
with tempfile.TemporaryDirectory(prefix='s31-lc3-host-') as directory:
    exe = Path(directory) / 'roundtrip'
    cmd = ['cc', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
           '-fno-omit-frame-pointer', '-fno-sanitize-recover=all', '-I', str(lib / 'include'),
           str(Path(__file__).with_name('roundtrip.c'))]
    cmd += [str(p) for p in sorted((lib / 'src').glob('*.c'))]
    subprocess.run(cmd + ['-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
