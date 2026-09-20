"""Validate completed redacted comparison logs and restored flash snapshots."""
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path('/home/regex/work/esp32s31-openvela')
RECORDS = ROOT / 'backups/2026-09-10-scan-stress'
LOGS = RECORDS / 'logs'
BACKUP = RECORDS / 'idf170-flash-backup'
EXPECTED = 'd3486d22ec287c1aca5c0da1ba5728e46d11095e8c7b0b8bd2ec2fda3a7c1d44'
ECHO = '1301c7362ff8a67293b6cda58d8bbe583d48d90eac8acb0ef0d61fd5e66f6e62'


def one(text, pattern):
    matches = re.findall(pattern, text, re.MULTILINE)
    if len(matches) != 1:
        raise RuntimeError(f'Expected exactly one marker: {pattern}')
    return matches[0]


def main():
    snapshots = {}
    for name in ('before-a.bin', 'before-b.bin', 'after-restore.bin',
                 'after-restore171.bin', 'after-restore172.bin', 'after-restore173.bin'):
        data = (BACKUP / name).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if len(data) != 0x500000 or digest != EXPECTED:
            raise RuntimeError(f'Flash snapshot mismatch: {name}')
        snapshots[name] = digest
    rounds = []
    for run in (171, 172, 173):
        restore = (LOGS / f'idf{run}-restore.log').read_text()
        one(restore, r'^RESTORE_FULL_5MIB_COMPARE=PASS sha256=' + EXPECTED + r'$')
        for mode in (1, 0):
            for number in range(1, 4):
                name = f'idf{run}-mode{mode}-round{number}-redacted.log'
                text = (LOGS / name).read_text()
                phy, bssid, channel, rssi = one(text,
                    r'^BASELINE_PHY=(\d+) bssid=([0-9a-f:]+) channel=(\d+) rssi=(-?\d+)$')
                if int(phy) != (6 if mode else 4):
                    raise RuntimeError(f'Wrong negotiated PHY: {name}')
                one(text, r'^BASELINE_IP=\d+\.\d+\.\d+\.\d+ GW=\d+\.\d+\.\d+\.\d+$')
                one(text, r'^BASELINE_PING sent=8 received=8 result=PASS$')
                one(text, r'^BASELINE_TCP=PASS bytes=4096 peer_closed=1$')
                one(text, r'^TCP_NETTEST=PASS bytes=4096 sha256=' + ECHO + '$')
                one(text, r'^BASELINE_DONE wifi=stopped$')
                if re.search(r'FAIL|Guru Meditation|abort\(\)', text):
                    raise RuntimeError(f'Failure marker: {name}')
                rounds.append(dict(run=run, mode='HE20' if mode else 'HT20',
                                   round=number, bssid=bssid, channel=int(channel),
                                   rssi=int(rssi), ping_replies=8, tcp_bytes=4096,
                                   pass_all=True, log=name))
    result = dict(completed_rounds=len(rounds), ping_replies=8 * len(rounds),
                  tcp_echoes=len(rounds), restored_snapshots=snapshots, rounds=rounds,
                  limitation='IDF diagnostic results, not openvela HE acceptance; '
                             'software coexistence was disabled under IDF')
    rendered = json.dumps(result, indent=2) + '\n'
    if len(sys.argv) == 2:
        with Path(sys.argv[1]).open('x') as output:
            output.write(rendered)
    print('IDF_COMPARISON_AUDIT=PASS rounds=18 ping=144/144 tcp=18/18 '
          'original_and_restored_snapshots=6')


if __name__ == '__main__':
    main()
