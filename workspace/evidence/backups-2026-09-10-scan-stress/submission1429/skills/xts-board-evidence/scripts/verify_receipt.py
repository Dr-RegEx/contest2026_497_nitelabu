"""Read-only SHA256 receipt verification; does not inspect or reset a board."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('receipt', type=Path)
    parser.add_argument('--base', required=True, type=Path)
    args = parser.parse_args()
    records = []
    for number, line in enumerate(args.receipt.read_text().splitlines(), 1):
        if not line.strip():
            continue
        match = re.fullmatch(r'([0-9a-fA-F]{64})\s+([ *]?)(.+)', line)
        if not match:
            records.append({'line': number, 'status': 'INVALID_RECEIPT_LINE'})
            continue
        expected, _, name = match.groups()
        target = Path(name)
        if not target.is_absolute():
            target = args.base / target
        try:
            digest = hashlib.sha256()
            with target.open('rb') as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                    digest.update(chunk)
            actual = digest.hexdigest()
            records.append({'line': number, 'file': str(target),
                            'status': 'MATCH' if actual == expected.lower() else 'MISMATCH',
                            'expected': expected.lower(), 'actual': actual})
        except OSError as error:
            records.append({'line': number, 'file': str(target),
                            'status': 'UNREADABLE', 'error': str(error)})
    ok = bool(records) and all(r['status'] == 'MATCH' for r in records)
    print(json.dumps({'status': 'FILES_MATCH' if ok else 'FAIL',
                      'target_firmware_verified': False, 'records': records}, indent=2))
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
