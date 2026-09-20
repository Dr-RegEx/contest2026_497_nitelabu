"""Capture original4.1.26/4.1.25 after successful original provisioning.

UART must be free. Run sense exactly twice; retain unchanged RSSI for review.
This runner records evidence only; it does not invent PASS from command exit.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', required=True, type=Path)
    parser.add_argument('--provisioning-result', required=True, type=Path)
    parser.add_argument('--result', required=True, type=Path)
    args = parser.parse_args()
    k.require(not args.result.exists(), 'result already exists')
    prior = json.loads(args.provisioning_result.read_text())
    k.require(prior['case'] == '5.1.29' and prior['status'] == 'PASS',
              'requires completed original provisioning evidence')
    k.require(Path(prior['receipt']).resolve() == args.receipt.resolve(),
              'provisioning receipt differs')
    for line in args.receipt.read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        k.require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest,
                  'artifact receipt mismatch')
    result = {'status': 'FAIL', 'receipt': str(args.receipt),
              'provisioning_result': str(args.provisioning_result),
              'commands': []}
    try:
        with k.existing_uart() as port:
            for command in ['ifconfig', 'wapi show wlan0',
                            'wapi sense wlan0', 'wapi sense wlan0']:
                output = k.transport.run_command(port, command, timeout=60,
                                                 reset_input=False)
                result['commands'].append({'command': command, 'output': output})
                k.require(not re.search(r'Assertion failed|PANIC:|KASAN:|ESP-ROM:',
                                        output), 'target fault')
                if command == 'ifconfig':
                    k.require('RUNNING' in output, 'association prerequisite lost')
        result['status'] = 'CAPTURE_COMPLETE_REVIEW_REQUIRED'
    except Exception as error:
        result['error'] = str(error)
    finally:
        args.result.write_text(json.dumps(result, indent=2) + '\n')
        print(result['status'], flush=True)
    return 0 if result['status'] == 'CAPTURE_COMPLETE_REVIEW_REQUIRED' else 1


if __name__ == '__main__':
    raise SystemExit(main())
