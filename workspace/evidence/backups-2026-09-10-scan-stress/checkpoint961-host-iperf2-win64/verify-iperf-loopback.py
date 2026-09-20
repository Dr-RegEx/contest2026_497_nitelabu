"""Host binary check only; no board or LAN traffic."""
from pathlib import Path
import subprocess, socket, time, json
base = Path(__file__).resolve().parent
exe = str(base / 'iperf2.exe')
results = []
for protocol in ('tcp', 'udp'):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM if protocol == 'udp' else socket.SOCK_STREAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = str(reservation.getsockname()[1])
    udp = ['-u'] if protocol == 'udp' else []
    server_args = [exe, '-s', '-B', '127.0.0.1', '-p', port, '-1', '-t', '5'] + udp
    server = subprocess.Popen(server_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        time.sleep(0.5)
        assert server.poll() is None, 'server stopped before client'
        client_args = [exe, '-c', '127.0.0.1', '-p', port, '-t', '1'] + udp
        if protocol == 'udp':
            client_args += ['-b', '1M']
        client = subprocess.run(client_args, capture_output=True, timeout=15)
        server_out, _ = server.communicate(timeout=15)
        (base / ('loopback-' + protocol + '-client.log')).write_bytes(client.stdout + client.stderr)
        (base / ('loopback-' + protocol + '-server.log')).write_bytes(server_out)
        assert client.returncode == 0 and server.returncode == 0, 'nonzero exit'
        assert b'bits/sec' in client.stdout and b'bits/sec' in server_out, 'missing transfer report'
        results.append({'protocol': protocol, 'status': 'HOST_LOOPBACK_PASS', 'seconds': 1})
    finally:
        if server.poll() is None:
            server.terminate()
            server.wait(timeout=5)
(base / 'loopback-result.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results))
