"""Host-only safety regression. Serial/esptool access is mocked out."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('transaction',
    Path(__file__).with_name('flash-transaction.py'))
transaction = importlib.util.module_from_spec(spec)
spec.loader.exec_module(transaction)


class Safety(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='s31-flash-safety-')
        self.addCleanup(self.temp.cleanup)
        self.backup = Path(self.temp.name)
        self.original = b'\xa5' * 0x500000
        for name in ('before-a.bin', 'before-b.bin'):
            (self.backup / name).write_bytes(self.original)
        (self.backup / 'after-restore.bin').write_bytes(self.original)
        if transaction.RUN in ('172', '173'):
            (self.backup / f'after-restore{int(transaction.RUN) - 1}.bin').write_bytes(self.original)
        (self.backup / 'SHA256SUMS').write_text(''.join(
            f'{transaction.digest(self.original)}  {self.backup / name}\n'
            for name in ('before-a.bin', 'before-b.bin')))
        self.patcher = patch.object(transaction, 'BACKUP', self.backup)
        self.patcher.start()
        self.addCleanup(self.patcher.stop)
        self.device = patch.object(transaction, 'esptool')
        self.mock_device = self.device.start()
        self.addCleanup(self.device.stop)
        with contextlib.redirect_stdout(io.StringIO()):
            transaction.prepare()

    def manifest(self):
        return self.backup / transaction.STAGE_NAME / 'manifest.json'

    def reject(self, operation='install'):
        with self.assertRaises(RuntimeError):
            transaction.write(operation)
        self.mock_device.assert_not_called()

    def test_verified_install_ranges(self):
        transaction.write('install')
        args = self.mock_device.call_args.args[0]
        self.assertEqual(args[:7], ['write-flash', '--flash-mode', 'keep',
                                   '--flash-size', 'keep', '--flash-freq', 'keep'])
        self.assertEqual(args[7::2], ['0x2000', '0x8000', '0x10000'])

    def test_disagreeing_backup_rejected(self):
        (self.backup / 'before-b.bin').write_bytes(b'bad')
        self.reject()

    def test_corrupt_image_rejected(self):
        (self.backup / transaction.STAGE_NAME / 'idf-0.bin').write_bytes(b'bad')
        self.reject()

    def test_protected_offset_rejected(self):
        manifest = json.loads(self.manifest().read_text())
        manifest['images'][0]['offset'] = 0x500000
        manifest['images'][0]['end'] = 0x505000
        self.manifest().write_text(json.dumps(manifest))
        self.reject()

    def test_corrupt_restore_rejected(self):
        manifest = json.loads(self.manifest().read_text())
        entry = manifest['images'][0]
        bad = b'\xff' * (entry['end'] - entry['offset'])
        (self.backup / transaction.STAGE_NAME / entry['restore']).write_bytes(bad)
        entry['restore_sha256'] = transaction.digest(bad)
        self.manifest().write_text(json.dumps(manifest))
        self.reject('restore')

    def test_restore_full_readback_required(self):
        if transaction.RUN == '170':
            (self.backup / 'after-restore.bin').unlink()
        def device(args):
            if args[0] == 'read-flash':
                self.assertEqual(args[1:3], ['0x0', '0x500000'])
                Path(args[3]).write_bytes(self.original)
        self.mock_device.side_effect = device
        transaction.write('restore')
        self.assertEqual(self.mock_device.call_count, 2)

    def test_restore_readback_mismatch_rejected(self):
        if transaction.RUN == '170':
            (self.backup / 'after-restore.bin').unlink()
        def device(args):
            if args[0] == 'read-flash':
                Path(args[3]).write_bytes(b'bad')
        self.mock_device.side_effect = device
        with self.assertRaisesRegex(RuntimeError, 'restored full region differs'):
            transaction.write('restore')


if __name__ == '__main__':
    unittest.main()
