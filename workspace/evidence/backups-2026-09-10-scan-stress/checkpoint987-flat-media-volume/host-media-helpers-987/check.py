"""HOST ONLY: exercise actual helper guards with substituted hardware access."""
import ast
from contextlib import contextmanager, redirect_stdout, redirect_stderr
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
from unittest.mock import patch

D = Path(__file__).resolve().parent.parent
OUT = Path(__file__).resolve().parent
ROOT = D.parent.parent
results = []

def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, D / filename)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

prep = load('prep987', 'prepare-media-volume-987.py')
transfer = load('transfer987', 'audio-file-transfer.py')

with tempfile.TemporaryDirectory(prefix='s31-media-helpers-987-') as temp:
    base = Path(temp)
    image_dir = base / 'image'
    image_dir.mkdir()
    image = image_dir / 'nuttx.bin'
    image.write_bytes(b'HOST STANDIN IMAGE, NOT FIRMWARE')
    digest = hashlib.sha256(image.read_bytes()).hexdigest()
    receipt = base / 'receipt.sha256'
    receipt.write_text(f'{digest}  {image}\n')
    config = image_dir / '.config'
    good_config = ''.join('CONFIG_' + key + '=y\n' for key in (
        'BUILD_FLAT', 'ESP32S31_XTS_MEDIA_VOLUME', 'FS_LITTLEFS',
        'ESP32S31_SPIFLASH_PSRAM_STACK', 'ESP32S31_AUDIO', 'SYSTEM_YMODEM', 'FS_TMPFS'))
    config.write_text(good_config)
    boot = base / 'boot.log'
    boot.write_text(digest + '\nxTS WAV volume: /dev/wavvol 0xd00000 0x300000\n')
    backup = base / 'backup'
    backup.mkdir()
    blank = b'\xff' * 0x300000
    blank_digest = hashlib.sha256(blank).hexdigest()
    manifest = dict(purpose='temporary-media-volume-987', offset=0xd00000,
                    size=0x300000, blank=True, files=[])
    for name in ('before-a.bin', 'before-b.bin'):
        (backup / name).write_bytes(blank)
        manifest['files'].append(dict(name=name, sha256=blank_digest))
    manifest_path = backup / 'manifest.json'
    manifest_path.write_text(json.dumps(manifest))
    token = backup / 'used-by-media-volume-987.json'
    opened = []
    commands = []

    @contextmanager
    def fake_uart():
        opened.append(True)
        yield object()

    def fake_command(port, command, *args):
        commands.append(command)
        if command == 'mount':
            return '/wav type littlefs\n'
        return 'nsh> '

    def prep_reject(label, busy=False, expect_open=False):
        opened.clear(); commands.clear()
        output = base / label
        argv = ['prepare', '--receipt', str(receipt), '--flash-backup', str(manifest_path),
                '--boot-evidence', str(boot), '--output', str(output)]
        with patch.object(sys, 'argv', argv), patch.object(prep.subprocess, 'run',
            return_value=subprocess.CompletedProcess([], 0 if busy else 1, b'', b'123' if busy else b'')), \
            patch.object(prep.kv, 'existing_uart', fake_uart), patch.object(prep.kv, 'command', fake_command), \
            redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            try:
                prep.main()
            except RuntimeError as e:
                reason = str(e)
            else:
                raise AssertionError(label + ' accepted invalid input')
        assert bool(opened) == expect_open, (label, opened)
        assert not any('forceformat' in cmd for cmd in commands)
        assert not token.exists(), label + ' consumed token'
        results.append(dict(case=label, status='PASS', reason=reason,
                            uart_standin_opened=bool(opened), format_commands=0))

    wrong = dict(manifest, offset=0xc00000, size=0x100000)
    manifest_path.write_text(json.dumps(wrong))
    prep_reject('wrong_1MiB_manifest')
    manifest_path.write_text(json.dumps(dict(manifest, blank=False)))
    prep_reject('nonblank_manifest')
    manifest_path.write_text(json.dumps(manifest))
    (backup / 'before-b.bin').write_bytes(b'X' + blank[1:])
    prep_reject('nonblank_content_despite_manifest')
    (backup / 'before-b.bin').write_bytes(blank)
    token.write_text('{}')
    try:
        prep.verify_backup(manifest_path)
    except RuntimeError as e:
        results.append(dict(case='consumed_manifest', status='PASS', reason=str(e)))
    else:
        raise AssertionError('consumed manifest accepted')
    token.unlink()
    config.write_text(good_config.replace('CONFIG_ESP32S31_XTS_MEDIA_VOLUME=y\n', ''))
    prep_reject('wrong_profile')
    config.write_text(good_config)
    prep_reject('busy_uart', busy=True)
    prep_reject('already_mounted_volume', expect_open=True)
    config.write_text(good_config + 'CONFIG_ESPRESSIF_WIFI=y\n')
    try:
        prep.verify_image(receipt)
    except RuntimeError as e:
        results.append(dict(case='real_wifi_symbol_guard', status='PASS', reason=str(e)))
    else:
        results.append(dict(case='real_wifi_symbol_guard', status='ISSUE', reason='ESPRESSIF_WIFI not rejected'))
    config.write_text(good_config)

    # The backup entry point runs under a subprocess stand-in; no esptool executes.
    calls = []
    def fake_run(argv, **kwargs):
        calls.append(argv)
        if argv[0] == 'fuser':
            return subprocess.CompletedProcess(argv, 1, b'', b'')
        assert 'read-flash' in argv and 'write-flash' not in argv and 'erase-flash' not in argv
        i = argv.index('read-flash')
        assert argv[i + 1:i + 3] == ['0xd00000', '0x300000']
        Path(argv[-1]).write_bytes(blank)
        return subprocess.CompletedProcess(argv, 0)
    backup_output = base / 'backup_script'
    with patch.object(sys, 'argv', ['backup', str(backup_output)]), patch.object(subprocess, 'run', fake_run), redirect_stdout(io.StringIO()):
        runpy.run_path(str(D / 'backup-media-flash-987.py'), run_name='__main__')
    assert len(calls) == 3
    prep.verify_backup(backup_output / 'manifest.json')
    results.append(dict(case='backup_read_only_exact_range_two_reads', status='PASS', subprocesses_substituted=True))

    calls.clear()
    def busy_backup_run(argv, **kwargs):
        calls.append(argv)
        assert argv[0] == 'fuser', 'esptool must not run while UART is occupied'
        return subprocess.CompletedProcess(argv, 0, b'', b'123')
    with patch.object(sys, 'argv', ['backup', str(base / 'busy_backup')]), patch.object(subprocess, 'run', busy_backup_run):
        try:
            runpy.run_path(str(D / 'backup-media-flash-987.py'), run_name='__main__')
        except RuntimeError:
            pass
        else:
            raise AssertionError('busy backup accepted')
    assert len(calls) == 1 and not (base / 'busy_backup').exists()
    results.append(dict(case='backup_busy_uart_no_esptool', status='PASS'))

    def nonblank_backup_run(argv, **kwargs):
        outcome = fake_run(argv, **kwargs)
        if argv[0] != 'fuser':
            Path(argv[-1]).write_bytes(b'X' + blank[1:])
        return outcome
    nonblank_output = base / 'nonblank_backup'
    with patch.object(sys, 'argv', ['backup', str(nonblank_output)]), patch.object(subprocess, 'run', nonblank_backup_run):
        try:
            runpy.run_path(str(D / 'backup-media-flash-987.py'), run_name='__main__')
        except RuntimeError:
            pass
        else:
            raise AssertionError('nonblank backup accepted')
    assert json.loads((nonblank_output / 'manifest.json').read_text())['blank'] is False
    assert (nonblank_output / 'before-a.bin').read_bytes() == (nonblank_output / 'before-b.bin').read_bytes()
    results.append(dict(case='backup_nonblank_rejected_and_preserved', status='PASS'))

    class HardwareBoundary(RuntimeError):
        pass
    real_os_open = transfer.os.open
    opens = []
    def guarded_open(path, *args, **kwargs):
        if str(path) == '/dev/ttyUSB0':
            opens.append(str(path))
            raise HardwareBoundary('HOST TEST stopped before real UART access')
        return real_os_open(path, *args, **kwargs)
    wav = ROOT / 'openvela-dev/docs/zh-cn/test_dev_guide/mediatool测试资源和测试步骤/测试资源/音视频测试资源文件/audio_file.wav'
    assert wav.stat().st_size == 17473937
    assert hashlib.sha256(wav.read_bytes()).hexdigest() == '20d7c680be243cac559c1d390a324c5b6740dc32471647c91a7c544fb9df5ef7'
    small = base / 'voice.pcm'; small.write_bytes(b'PCM fixture')
    bad_wav_dir = base / 'badwav'; bad_wav_dir.mkdir()
    bad_wav = bad_wav_dir / 'audio_file.wav'; bad_wav.write_bytes(b'not original')
    def transfer_case(label, src, directory=None, busy=False, boundary=False):
        opens.clear()
        argv = ['transfer', '--upload', str(src), '--receipt', str(receipt), '--output', str(base / label)]
        if directory:
            argv += ['--directory', directory]
        with patch.object(sys, 'argv', argv), patch.object(transfer.subprocess, 'run',
            return_value=subprocess.CompletedProcess([], 0 if busy else 1, b'', b'123' if busy else b'')), \
            patch.object(transfer.os, 'open', guarded_open), redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            try:
                transfer.main()
            except HardwareBoundary:
                assert boundary
            except RuntimeError:
                assert not boundary
            else:
                raise AssertionError('unexpected transfer execution')
        assert bool(opens) == boundary
        results.append(dict(case=label, status='PASS', hardware_boundary_reached=boundary, real_uart_opened=False))
    transfer_case('intact_original_wav_preflight', wav, '/wav', boundary=True)
    transfer_case('data_default_compatibility', small, boundary=True)
    transfer_case('bad_wav_rejected', bad_wav, '/wav')
    transfer_case('transfer_busy_uart', wav, '/wav', busy=True)
    config.write_text(good_config.replace('CONFIG_ESP32S31_XTS_MEDIA_VOLUME=y\n', ''))
    transfer_case('wav_wrong_profile_rejected', wav, '/wav')
    tree = ast.parse((D / 'audio-file-transfer.py').read_text())
    expressions = [n for n in ast.walk(tree) if isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id == 'max' and any(isinstance(v, ast.Constant) and v.value == 600 for v in n.args)]
    assert len(expressions) == 1
    formula = compile(ast.Expression(expressions[0]), '<actual deadline expression>', 'eval')
    allowance = eval(formula, {'expected': wav.stat().st_size})
    small_allowance = eval(formula, {'expected': small.stat().st_size})
    assert allowance > wav.stat().st_size * 10 / 115200 and small_allowance == 600
    results.append(dict(case='actual_dynamic_deadline_formula', status='PASS', wav_seconds=allowance, small_seconds=small_allowance))

report = dict(scope='HOST ONLY: substituted hardware, no UART/esptool/format/target transfer',
              results=results, status='PASS' if all(x['status']=='PASS' for x in results) else 'ISSUES')
(OUT / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
