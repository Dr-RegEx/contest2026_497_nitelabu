"""Transport for the eight original xTS Crypto applications, no test edits.

Default profile uses cryptodev's software backend. --backend hardware-cbc
requires actual driver completion markers for CBC; other algorithms remain
software. --backend hardware-aes requires CBC/CTR/XTS completions; AES-192
uses the software fallback. --backend hardware-sha additionally requires all
three SHA engines' final completion records. --backend hardware-hmac also
requires SHA-accelerated HMAC records. --backend hardware-ecdsa additionally
requires P256 hardware verification. --backend hardware-ecc also requires
ECC point acceleration in keygen/sign (scalar arithmetic stays software).
No efuses.
"""
import argparse
import re
import subprocess
import sys
import time

import serial

sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import esp32s31_production_smoke as production

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--backend', choices=('software', 'hardware-cbc',
                                         'hardware-aes', 'hardware-sha',
                                         'hardware-hmac', 'hardware-ecdsa',
                                         'hardware-ecc'),
                    default='software')
args = parser.parse_args()

busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
if busy.returncode != 1 or busy.stderr.strip():
    raise RuntimeError('UART occupied or occupancy check failed; no serial open/reset')

failed = []
with serial.Serial('/dev/ttyUSB0', 115200, timeout=0.1,
                   write_timeout=1, exclusive=True) as port:
    production.boot(port, 'XTS_CRYPTO_BOOT')
    production.command(port, 'ifdown wlan0')
    production.command(port, 'ls /dev/crypto')
    for name in ('des3cbc', 'aescbc', 'aesctr', 'aesxts', 'hmac', 'hash',
                 'crc32', 'ecdsa'):
        command = 'cmocka_' + name
        print('XTS_COMMAND=' + command, flush=True)
        port.reset_input_buffer()
        for byte in command.encode() + b'\r\n':
            port.write(bytes([byte]))
            time.sleep(0.01)
        started = time.monotonic()
        output = bytearray()
        while time.monotonic() - started < 600:
            data = port.read(4096)
            if not data:
                continue
            output.extend(data)
            print(data.decode(errors='replace'), end='', flush=True)
            if any(marker in output for marker in
                   (b'Assertion failed', b'S31SM:M-TRAP',
                    b'Segmentation fault', b'riscv_exception: EXCEPTION:')):
                until = time.monotonic() + 5
                while time.monotonic() < until:
                    print(port.read(4096).decode(errors='replace'), end='', flush=True)
                raise RuntimeError('target fault: stop batch and preserve dump')
            if b'nsh> ' in output:
                break
        else:
            raise TimeoutError(command + ' did not finish')
        text = output.decode(errors='replace')
        runs = re.findall(r'\[==========\] [^\r\n]*?(\d+) test\(s\) run\.', text)
        passes = re.findall(r'\[  PASSED  \] (\d+) test\(s\)\.', text)
        expected = {'des3cbc': 1, 'aescbc': 1, 'aesctr': 1, 'aesxts': 1,
                    'hmac': 3, 'hash': 4, 'crc32': 4}
        ok = (len(runs) == 1 and runs == passes
              and int(runs[0]) == expected.get(name)
              and not re.search(r'FAILED|SKIPPED|nsh:|ERROR:', text))
        if name == 'ecdsa':
            # Despite the binary's cmocka_ prefix, this original application
            # reports its P-256 case directly rather than using cmocka.
            ok = ('Test SECP256R1 case success' in text and
                  not re.search(r'failed|ERROR:|nsh:|CIOCKEY:', text))
        backend = 'software'
        hardware = {'aescbc': 'CBC'} if args.backend == 'hardware-cbc' else (
            {'aescbc': 'CBC', 'aesctr': 'CTR', 'aesxts': 'XTS'}
            if args.backend in ('hardware-aes', 'hardware-sha',
                                'hardware-hmac', 'hardware-ecdsa',
                                'hardware-ecc') else {})
        if name in hardware:
            backend = 'hardware-supported-keys'
            completions = re.findall(r'S31_AES_' + hardware[name] +
                                     r'_HW bytes=(\d+) status=(-?\d+)', text)
            ok = ok and bool(completions) and all(
                int(size) > 0 and status == '0' for size, status in completions)
        if name == 'hash' and args.backend in ('hardware-sha', 'hardware-hmac',
                                               'hardware-ecdsa', 'hardware-ecc'):
            backend = 'hardware-sha-software-md5'
            for bits in (160, 256, 512):
                completions = re.findall(r'S31_SHA' + str(bits) +
                                         r'_HW bytes=(\d+) status=(-?\d+)', text)
                ok = ok and len(completions) >= 4 and all(
                    status == '0' for size, status in completions)
                ok = ok and {'3', '1000000', '614400'} <= {size for size, status in completions}
        if name == 'hmac' and args.backend in ('hardware-hmac', 'hardware-ecdsa',
                                               'hardware-ecc'):
            backend = 'sha-accelerated-hmac-software-md5'
            for bits in (160, 256):
                completions = re.findall(r'S31_HMAC_SHA' + str(bits) +
                                         r'_HW bytes=(\d+) status=(-?\d+)', text)
                ok = ok and len(completions) == 3 and all(
                    status == '0' for size, status in completions)
                ok = ok and {'8', '28', '50'} == {size for size, status in completions}
        if name == 'ecdsa' and args.backend in ('hardware-ecdsa', 'hardware-ecc'):
            backend = 'hardware-verify-software-keygen-sign'
            completions = re.findall(r'S31_ECDSA_P256_VERIFY_HW status=(-?\d+)', text)
            ok = ok and bool(completions) and all(status == '0' for status in completions)
            if args.backend == 'hardware-ecc':
                backend = 'ecc-assisted-keygen-sign-hardware-verify'
                points = re.findall(r'S31_ECC_P256_POINT_HW status=(-?\d+)', text)
                ok = ok and len(points) >= 2 and all(status == '0' for status in points)
        print(f'XTS_CRYPTO={command} {"PASS" if ok else "FAIL"} '
              f'seconds={time.monotonic()-started:.3f} backend={backend}', flush=True)
        if not ok:
            failed.append(command)
    production.command(port, 'free')
print('XTS_CRYPTO_FAILED=' + repr(failed), flush=True)
raise SystemExit(bool(failed))
