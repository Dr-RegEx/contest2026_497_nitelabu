"""Wait for existing1181; verify and archive that run, never restart it."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import time

D = Path(__file__).resolve().parent
STATE = D / 'fs1184-finish.json'
PID = 669451


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, D / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def state(status, **details):
    data = dict(status=status, timestamp=time.time(), **details)
    temporary = STATE.with_suffix('.tmp')
    temporary.write_text(json.dumps(data, indent=2) + '\n')
    temporary.replace(STATE)
    print(json.dumps(data), flush=True)


def main():
    kv = load('finish1184_kv', 'xts-kvdb-suite.py')
    review = load('finish1184_review', 'verify1183-fstest-log.py')
    process = Path(f'/proc/{PID}/stat')
    cmdline = Path(f'/proc/{PID}/cmdline')
    kv.require(cmdline.exists() and b'run1181-fstest1000-large.py' in cmdline.read_bytes(),
               'expected existing collector missing; no UART action')
    identity = process.read_text().split()[21]
    state('WAITING_FOR_1181', collector_pid=PID, no_uart_while_running=True)
    deadline = time.monotonic() + 49 * 3600
    while process.exists():
        try:
            fields = process.read_text().split()
        except FileNotFoundError:
            break
        if fields[21] != identity or fields[2] == 'Z':
            break
        kv.require(time.monotonic() < deadline, 'wait expired; preserve board')
        time.sleep(15)
    raw = D / 'logs/xts1181-fstest1000.log'
    text = raw.read_text(errors='replace')
    verdict = review.Verdict()
    started = False
    for line in text.splitlines():
        # The collector traceback repeats its false-positive list. It is host
        # evidence, not original target output; retain it in the raw archive.
        if line.startswith('XTS_ELAPSED_SECONDS='):
            break
        if line.startswith('=== FILLING 1 '):
            started = True
        if started:
            verdict.line(line)
    verdict.check(kv.require)
    kv.require('XTS_ELAPSED_SECONDS=' in text, 'collector did not observe prompt')
    kv.require('XTS_COMMAND=echo $?' not in text, 'original status already consumed')
    busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
    kv.require(busy.returncode == 1 and not busy.stderr.strip(), 'UART still owned')
    state('POSTCHECK', original_loops=1000, summary=[2000, 0])
    with kv.existing_uart() as port:
        # MUST be first: preserve original application's exit status.
        result = kv.command(port, 'echo $?')
        kv.require(re.search(r'(?m)^\s*0\s*$', result), 'original target exit nonzero')
        kv.require('/data type littlefs' in kv.command(port, 'mount'), 'mount missing')
        tasks = kv.command(port, 'ps')
        kv.require(not re.search(r'\bfstest\b|\bvela_fs_\w*', tasks), 'storage task remains')
        kv.command(port, 'ls -l /data/fstest')
        kv.command(port, 'df')
        kv.command(port, 'free')
        kv.command(port, 'rm -r /data/fstest')
        kv.require(re.search(r'(?m)^\s*0\s*$', kv.command(port, 'echo $?')),
                   'documented cleanup failed')
        kv.command(port, 'ls -l /data')
        kv.command(port, 'df')
    archive = D / 'checkpoint1184-fstest1000'
    archive.mkdir(exist_ok=False)
    for source in (raw, D / 'run1181-fstest1000-large.py',
                   D / 'verify1183-fstest-log.py', Path(__file__),
                   D / 'fs1183-verdict-note.md', D / 'build1032-flat-category-fs-large.sha256',
                   D / 'checkpoint1179-fstest-mount/mount-evidence.log',
                   D / 'logs/xts1177-fstest-flash2.log', D / 'logs/xts1178-fstest-boot.log'):
        shutil.copy2(source, archive / source.name)
    result = dict(case='5.1.15', status='PASS', original_loops=1000,
                  original_summary=[2000, 0], target_exit=0, cleanup='complete',
                  dot_listing_lines=verdict.dot_entries,
                  original_host_verdict='false-positive dot-directory Error labels; raw traceback retained',
                  mount='3MiB LittleFS; setup command included -o sync; frozen1032',
                  elapsed_seconds=float(re.search(r'XTS_ELAPSED_SECONDS=([0-9.]+)', text)[1]))
    (archive / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('XTS_CASE=5.1.15 PASS original_loops=1000 cleanup=complete', flush=True)
    shutil.copy2(D / 'logs/xts1184-fstest-finish.log', archive / 'postcheck.log')
    files = sorted(p for p in archive.iterdir() if p.is_file())
    (archive / 'SHA256SUMS').write_text(''.join(
        hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n' for p in files))
    state('PASS', checkpoint=str(archive), **{k: v for k, v in result.items() if k != 'status'})


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        state('REVIEW_REQUIRED', error=str(error), no_reset_or_retry=True)
        raise
