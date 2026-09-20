"""Associate only with the interactively supplied, authorized WPA2 test AP.

Credentials are read without terminal echo and never saved. All transcript
output, including exceptions, is redacted. No profile is saved on the board.
"""
import contextlib
import getpass
import ipaddress
import os
import re
import subprocess
from pathlib import Path
import sys
import time

import serial

sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production
from udp_probe_support import validate_transcript as udp_passed


def diagnostic_boot(port, label):
    if os.environ.get('S31_DIAGNOSTIC_BOOT') != 'flat':
        return production.boot(port, label)
    started = time.monotonic()
    transport.hard_reset(port)
    try:
        output = transport.collect_until_prompt(port, 15.0)
    except TimeoutError as error:
        print(f'{label}=FAIL: {error}', flush=True)
        raise
    print(output.decode(errors='replace'), flush=True)
    production.require(b'NuttShell (NSH)' in output, 'flat NSH banner missing')
    production.require(not any(marker in output for marker in transport.BOOT_FAILURE_MARKERS),
                       'fatal flat boot marker')
    production.require(b'CPU1 SMP online' not in output and b'AppFS MTD' not in output,
                       'unexpected production boot in flat diagnostic')
    interfaces = transport.run_command(port, 'ifconfig', timeout=10)
    production.require('wlan0' in interfaces, 'flat Wi-Fi netdev missing')
    print(f'{label}=PASS mode=flat seconds={time.monotonic() - started:.3f}', flush=True)


class Redacted:
    def __init__(self, stream, secrets):
        self.stream = stream
        self.secrets = sorted(set(secrets), key=len, reverse=True)

    def write(self, text):
        rendered = text
        for secret in self.secrets:
            if secret:
                rendered = rendered.replace(secret, '<redacted>')
        self.stream.write(rendered)
        self.stream.flush()
        return len(text)

    def flush(self):
        self.stream.flush()


gateway = str(ipaddress.IPv4Address(sys.argv[2])) if len(sys.argv) > 2 else '192.168.1.1'
device = sys.argv[3] if len(sys.argv) > 3 else '/dev/ttyUSB0'
ssid = getpass.getpass('Authorized test SSID (hidden): ')
password = getpass.getpass('WPA2 password (hidden): ')
escaped_ssid = transport.nsh_escape(ssid)
escaped_password = transport.nsh_escape(password)
secrets = (ssid, password, escaped_ssid, escaped_password)
logpath = Path(sys.argv[1])
status = 1
with logpath.open('x', buffering=1) as log:
    output = Redacted(log, secrets)
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
        try:
            with serial.Serial(device, 115200, timeout=0.05,
                               write_timeout=1, exclusive=True) as port:
                try:
                    diagnostic_boot(port, 'NETWORK_BOOT')
                    if os.environ.get('S31_REQUIRE_CPU0') == '1':
                        # read_task_pids intentionally selects application
                        # tasks; this check must include kernel workers too.
                        listing = transport.run_command(port, 'ps', timeout=10)
                        tasks = set(re.findall(
                            r'(?m)^\s*(\d+)\s+\d+\s+(?:\d+|---)\(0x[0-9a-fA-F]+\)',
                            listing))
                        checked = 0
                        for pid in sorted(tasks, key=int):
                            if int(pid) < 2:  # Per-hart idle threads stay on their own CPUs.
                                continue
                            state = transport.run_command(port, f'cat /proc/{pid}/status',
                                                          timeout=10)
                            affinity = re.search(r'(?m)^CPU:\s+[^\r\n]*\(0x([0-9a-fA-F]+)\)',
                                                 state)
                            production.require(affinity is not None and
                                               int(affinity.group(1), 16) == 1,
                                               f'PID {pid} is not restricted to CPU0')
                            checked += 1
                        production.require(checked >= 3, 'too few tasks for CPU0 verification')
                        print(f'CPU0_AFFINITY=PASS tasks={checked}', flush=True)
                    transport.run_command(port, 'ifup wlan0', timeout=15)
                    scan_modes = ('scan', 'pscan')
                    if os.environ.get('S31_SKIP_EXPLICIT_SCAN') == '1':
                        scan_modes = ()
                        print('SCAN_PHASE=SKIPPED_DIAGNOSTIC; association may scan internally',
                              flush=True)
                    for mode in scan_modes:
                        started = time.monotonic()
                        result = transport.run_command(port, 'wapi ' + mode + ' wlan0',
                                                       timeout=30, redactions=secrets)
                        print(f'{mode.upper()}_SECONDS={time.monotonic() - started:.3f}',
                              flush=True)
                        production.require('ERROR:' not in result and
                                           'bssid / frequency / signal level / encode / ssid' in result,
                                           mode + ' failed')
                        print(mode.upper() + '_IOCTL=PASS', flush=True)
                    commands = [f'wapi psk wlan0 {escaped_password} 3 2']
                    bssid = os.environ.get('S31_TEST_BSSID')
                    if len(sys.argv) > 4 and sys.argv[4].startswith('bssid:'):
                        bssid = sys.argv[4][6:]
                    if bssid:
                        production.require(re.fullmatch(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', bssid),
                                           'invalid diagnostic BSSID')
                        # SIOCSIWAP connects immediately: install the PSK and
                        # delayed ESSID first, then choose the AP and connect.
                        commands += [f'wapi essid wlan0 {escaped_ssid} 2',
                                     'wapi ap wlan0 ' + bssid]
                    else:
                        commands.append(f'wapi essid wlan0 {escaped_ssid} 1')
                    for text in commands:
                        result = transport.run_command(port, text, timeout=25,
                                                       redactions=secrets)
                        production.require('ERROR:' not in result and 'nsh:' not in result,
                                           'WPA2 setup or association command failed')
                    expected = os.environ.get('S31_EXPECT_PROTOCOL')
                    if expected:
                        production.require(expected in ('7', '71'), 'invalid expected protocol')
                        protocol = re.search(r'driver protocol: ret=0 bitmap=(\d+)', result)
                        phy = re.search(r'negotiated PHY: ret=0 mode=(\d+)', result)
                        production.require(protocol is not None and protocol[1] == expected,
                                           'runtime protocol does not match the test label')
                        production.require(phy is not None and
                                           ((phy[1] == '6') == (expected == '71')),
                                           'negotiated PHY does not match the requested test')
                        print(f'PROTOCOL_PROFILE=PASS bitmap={expected} phy={phy[1]}', flush=True)
                    transport.run_command(port, 'sleep 2', timeout=6)
                    transport.run_command(port, 'ps', timeout=10)
                    transport.run_command(port, 'cat /dev/s31stat', timeout=10)
                    if len(sys.argv) > 4 and sys.argv[4] == 'background-ip':
                        transport.run_command(port, 'ifconfig wlan0 0.0.0.0 &', timeout=10)
                        transport.run_command(port, 'sleep 3', timeout=8)
                        tasks = transport.run_command(port, 'ps', timeout=10)
                        transport.run_command(port, 'cat /dev/s31stat', timeout=10)
                        pids = {'2', '5'}
                        for line in tasks.splitlines():
                            if 'ifconfig' in line:
                                match = re.match(r'\s*(\d+)\s', line)
                                if match:
                                    pids.add(match.group(1))
                        for pid in sorted(pids, key=int):
                            transport.run_command(port, f'cat /proc/{pid}/stack', timeout=10)
                            transport.run_command(port, f'cat /proc/{pid}/status', timeout=10)
                        print('BACKGROUND_IP_DIAGNOSTIC=COMPLETE; not a networking pass', flush=True)
                        raise SystemExit(0)
                    dhcp, interface, address = transport.obtain_dhcp(port, attempts=1)
                    production.require(address is not None and 'failed' not in dhcp.lower(),
                                       'DHCP did not produce a lease')
                    print(f'DHCP=PASS address={address}', flush=True)
                    if os.environ.get('S31_BACKGROUND_PING') == '1':
                        # Keep NSH available while reproducing the observed
                        # ping hang.  This is evidence capture, never a pass.
                        pid = transport.launch_background(
                            port, 'ping -c 4 ' + gateway, 'PING_DIAGNOSTIC')
                        production.require(pid is not None, 'background ping did not start')
                        transport.run_command(port, 'sleep 10', timeout=15)
                        listing = transport.run_command(port, 'ps', timeout=10)
                        transport.run_command(port, 'cat /dev/s31stat', timeout=10)
                        tasks = set(re.findall(
                            r'(?m)^\s*(\d+)\s+\d+\s+(?:\d+|---)\(0x[0-9a-fA-F]+\)',
                            listing))
                        for task in sorted(tasks, key=int):
                            if int(task) >= 2:
                                transport.run_command(port, f'cat /proc/{task}/status', timeout=10)
                        print('BACKGROUND_PING_DIAGNOSTIC=COMPLETE; not a networking pass',
                              flush=True)
                        raise SystemExit(1)
                    if len(sys.argv) > 4 and sys.argv[4] in ('soak', 'soak-dns'):
                        rounds = []
                        for index in range(1, 6):
                            started = time.monotonic()
                            sample = transport.run_command(port, 'ping -c 10 ' + gateway,
                                                           timeout=25)
                            passed = transport.ping_passed(sample, 10)
                            rounds.append(passed)
                            print(f'SOAK_ROUND={index} PASS={passed} '
                                  f'SECONDS={time.monotonic() - started:.3f}', flush=True)
                            transport.run_command(port, 'cat /dev/s31stat', timeout=10)
                        print(f'SOAK_RESULTS={rounds}', flush=True)
                        production.require(all(rounds), 'gateway soak packet loss')
                    if len(sys.argv) > 5:
                        peer = str(ipaddress.IPv4Address(sys.argv[5]))
                        target = str(ipaddress.IPv4Address(address))
                        script = ('$p = New-Object System.Net.NetworkInformation.Ping; '
                                  '1..4 | ForEach-Object { '
                                  f'$r = $p.Send("{target}", 1000); '
                                  '"HOST_TO_BOARD: status=$($r.Status) ms=$($r.RoundtripTime)" }; '
                                  '$p.Dispose()')
                        host = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive',
                                               '-Command', script], capture_output=True,
                                              encoding='utf-8', errors='replace', timeout=20)
                        print(host.stdout, flush=True)
                        print(host.stderr, flush=True)
                        # Preserve unsolicited RX/TX diagnostics before the
                        # next NSH command clears stale serial input.
                        print('--- SERIAL DURING HOST PROBE ---', flush=True)
                        print(port.read(port.in_waiting).decode('utf-8', errors='replace'),
                              flush=True)
                        transport.run_command(port, 'ping -c 4 ' + peer, timeout=20)
                    transport.run_command(port, 'arp -i wlan0 -a ' + gateway, timeout=10)
                    result = transport.run_command(port, 'ping -c 4 ' + gateway, timeout=20)
                    arp = transport.run_command(port, 'arp -i wlan0 -a ' + gateway, timeout=10)
                    warm = transport.run_command(port, 'ping -c 4 ' + gateway, timeout=20)
                    print('WARM_ARP_PING=' + ('PASS' if transport.ping_passed(warm, 4)
                          else 'FAIL'), flush=True)
                    print('COLD_GATEWAY_PING=' +
                          ('PASS' if transport.ping_passed(result, 4) else 'FAIL'),
                          flush=True)
                    transport.run_command(port, 'cat /dev/s31stat', timeout=10)
                    gateway_pass = (transport.ping_passed(result, 4) and
                                    transport.ping_passed(warm, 4) and
                                    'HWaddr:' in arp and 'nsh:' not in arp)
                    state = 'PASS' if gateway_pass else 'FAIL'
                    print('GATEWAY_PING=' + state, flush=True)
                    print('WPA2_DHCP_GATEWAY=' + state, flush=True)
                    if len(sys.argv) <= 4 or sys.argv[4] != 'tcp-diag':
                        production.require(gateway_pass, 'gateway ping/ARP did not pass')
                    if len(sys.argv) > 4 and sys.argv[4] in ('soak-dns', 'tcp-dns',
                                                          'tcp-udp-dns'):
                        # Use the DNS server assigned by DHCP. Never embed
                        # network credentials in an external request.
                        dns = transport.run_command(port, 'nslookup example.com', timeout=40)
                        addresses = re.findall(r'Addr: ((?:\d+\.){3}\d+)\b', dns)
                        production.require(addresses and 'nsh:' not in dns and
                                           'failed' not in dns.lower(),
                                           'DNS lookup failed')
                        for answer in addresses:
                            ipaddress.IPv4Address(answer)
                        print(f'DNS_LOOKUP=PASS ipv4_answers={len(addresses)}', flush=True)
                    if len(sys.argv) > 4 and sys.argv[4] in ('tcp-dns', 'tcp-diag',
                                                          'tcp-udp-dns'):
                        pid = transport.launch_background(port, 'nettest', 'TCP_TEST')
                        production.require(pid is not None, 'TCP server did not start')
                        if os.environ.get('S31_TCP_PROC_DIAG') == '1':
                            peer = str(ipaddress.IPv4Address(
                                os.environ.get('S31_TCP_PEER_IP', '192.168.1.29')))
                            mode = os.environ.get('S31_TCP_ARP_MODE', 'observe')
                            production.require(mode in ('observe', 'cold', 'static', 'resolve'),
                                               'invalid ARP diagnostic mode')
                            if mode in ('cold', 'resolve'):
                                transport.run_command(port, 'arp -i wlan0 -d ' + peer,
                                                      timeout=10)
                            if mode == 'resolve':
                                absent = transport.run_command(
                                    port, 'arp -i wlan0 -a ' + peer, timeout=10)
                                production.require('no such ARP entry' in absent,
                                                   'peer ARP was not cleared')
                                started = time.monotonic()
                                probe = transport.run_command(
                                    port, 'ping -c 1 ' + peer, timeout=10)
                                learned = transport.run_command(
                                    port, 'arp -i wlan0 -a ' + peer, timeout=10)
                                print('ACTIVE_PEER_ARP=' +
                                      ('PRESENT' if 'hwaddr:' in learned.lower()
                                       else 'ABSENT') +
                                      f' seconds={time.monotonic() - started:.3f}' +
                                      ' icmp=' +
                                      ('PASS' if transport.ping_passed(probe, 1)
                                       else 'FAIL'), flush=True)
                            if mode == 'static':
                                mac = os.environ.get('S31_TCP_PEER_MAC', '')
                                production.require(
                                    re.fullmatch(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', mac)
                                    is not None, 'invalid verified peer MAC')
                                transport.run_command(port,
                                    'arp -i wlan0 -s ' + peer + ' ' + mac, timeout=10)
                            print('TCP_ARP_MODE=' + mode +
                                  ' cleanup=board-reset', flush=True)
                            mapping = transport.run_command(port,
                                'arp -i wlan0 -a ' + peer, timeout=10)
                            if mode == 'static':
                                production.require('hwaddr: ' + mac.lower()
                                                   in mapping.lower(),
                                                   'temporary ARP mapping not installed')
                            transport.run_command(port, 'cat /proc/net/tcp', timeout=10)
                            transport.run_command(port, 'cat /proc/net/stat', timeout=10)
                        windows_python = ('C:\\Users\\tttgu\\.cache\\codex-runtimes\\'
                                          'codex-primary-runtime\\dependencies\\python\\python.exe')
                        windows_script = ('\\\\wsl.localhost\\Ubuntu-22.04\\home\\regex\\work\\'
                                          'esp32s31-openvela\\backups\\2026-09-10-scan-stress\\'
                                          'tcp-nettest-client.py')
                        target = str(ipaddress.IPv4Address(address))
                        invocation = f"& '{windows_python}' '{windows_script}' '{target}'"
                        client = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive',
                                                 '-Command', invocation], capture_output=True,
                                                encoding='utf-8', errors='replace', timeout=25)
                        print(client.stdout, flush=True)
                        print(client.stderr, flush=True)
                        print('--- SERIAL DURING TCP TEST ---', flush=True)
                        print(port.read(port.in_waiting).decode('utf-8', errors='replace'),
                              flush=True)
                        if os.environ.get('S31_TCP_PROC_DIAG') == '1':
                            transport.run_command(port, 'arp -i wlan0 -a ' + peer,
                                                  timeout=10)
                            transport.run_command(port, 'cat /proc/net/tcp', timeout=10)
                            transport.run_command(port, 'cat /proc/net/stat', timeout=10)
                        transport.run_command(port, 'cat /dev/s31stat', timeout=10)
                        if pid in transport.read_task_pids(port):
                            # proc/stack reports allocation/size, not raw stack bytes.
                            transport.run_command(port, f'cat /proc/{pid}/stack', timeout=10)
                        production.require(client.returncode == 0 and
                                           'TCP_NETTEST=PASS bytes=4096 ' in client.stdout,
                                           'TCP connect/send/echo/close test failed; see client phase')
                        production.require(pid not in transport.read_task_pids(port),
                                           'TCP server did not exit after one connection')
                        print('TCP_SERVER_EXIT=PASS', flush=True)
                    if len(sys.argv) > 4 and sys.argv[4] == 'tcp-udp-dns':
                        pid = transport.launch_background(port, 'udpserver', 'UDP_TEST')
                        production.require(pid is not None, 'UDP server did not start')
                        windows_script = ('\\\\wsl.localhost\\Ubuntu-22.04\\home\\regex\\work\\'
                                          'esp32s31-openvela\\backups\\2026-09-10-scan-stress\\'
                                          'udp_probe_support.py')
                        invocation = f"& '{windows_python}' '{windows_script}' '{target}'"
                        transcript = []
                        print('--- SERIAL DURING UDP TEST ---', flush=True)
                        client = subprocess.Popen(
                            ['powershell.exe', '-NoProfile', '-NonInteractive',
                             '-Command', invocation], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, encoding='utf-8', errors='replace')
                        try:
                            deadline = time.monotonic() + 25
                            while client.poll() is None:
                                chunk = port.read(max(1, port.in_waiting)).decode(
                                    'utf-8', errors='replace')
                                transcript.append(chunk)
                                print(chunk, end='', flush=True)
                                production.require(time.monotonic() < deadline,
                                                   'UDP sender timed out')
                            # Keep the UART drained during sending and allow
                            # the last frame/content check to finish printing.
                            deadline = time.monotonic() + 1
                            while time.monotonic() < deadline:
                                chunk = port.read(max(1, port.in_waiting)).decode(
                                    'utf-8', errors='replace')
                                transcript.append(chunk)
                                print(chunk, end='', flush=True)
                            output_text = client.communicate(timeout=2)[0]
                            print(output_text, flush=True)
                            production.require(client.returncode == 0 and
                                               'UDP_SENT=256 bytes_per_packet=96' in output_text,
                                               'UDP sender failed')
                        finally:
                            if client.poll() is None:
                                client.terminate()
                                client.wait(timeout=3)
                            if client.stdout:
                                client.stdout.close()
                        # The example itself permits lost packets. Insist on
                        # exactly 0..255, full sizes, one peer and no errors.
                        production.require(udp_passed(''.join(transcript)),
                                           'UDP receive sequence/content failed')
                        production.require(pid not in transport.read_task_pids(port),
                                           'UDP server did not exit')
                        print('UDP_RECEIVE=PASS packets=256 bytes=24576', flush=True)
                    production.require(gateway_pass, 'gateway ping/ARP did not pass')
                    status = 0
                finally:
                    # Try to notify the AP before resetting the station.  A
                    # failed command must not prevent the recovery reset.
                    try:
                        stopped = transport.run_command(port, 'ifdown wlan0', timeout=15,
                                                        redactions=secrets)
                        print('PRE_RESET_IFDOWN=' + ('PASS' if 'ifdown wlan0...OK' in stopped
                              else 'FAIL'), flush=True)
                    except (RuntimeError, TimeoutError, serial.SerialException) as error:
                        print('PRE_RESET_IFDOWN=FAIL: ' + str(error), flush=True)
                    diagnostic_boot(port, 'NETWORK_CLEANUP_BOOT')
                    production.require('ifdown wlan0...OK' in production.command(port, 'ifdown wlan0'),
                                       'cleanup ifdown failed')
        except (RuntimeError, TimeoutError, serial.SerialException,
                subprocess.SubprocessError, OSError, ValueError) as error:
            print('NETWORK_PROBE=FAIL: ' + str(error), flush=True)
            status = 1
print(f'NETWORK_PROBE_EXIT={status}; redacted log: {logpath}')
raise SystemExit(status)
