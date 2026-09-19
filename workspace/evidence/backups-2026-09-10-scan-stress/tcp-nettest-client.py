"""Windows-side client for the existing NuttX 4096-byte nettest server."""
import hashlib
import ipaddress
import socket
import sys
import time

address = str(ipaddress.IPv4Address(sys.argv[1]))
payload = bytes(0x20 + index % 95 for index in range(4096))
deadline = time.monotonic() + 15
stream = None
phase = 'connect'
try:
    for attempt in range(10):
        try:
            stream = socket.create_connection((address, 5471), timeout=3)
            break
        except ConnectionRefusedError:
            if attempt == 9:
                raise
            time.sleep(0.1)
    print(f'TCP_CONNECTED local={stream.getsockname()[0]} '
          f'remote={address}:5471', flush=True)
    phase = 'send'
    stream.settimeout(5)
    stream.sendall(payload)
    phase = 'receive'
    received = bytearray()
    while len(received) < len(payload):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError('TCP echo deadline')
        stream.settimeout(min(5, remaining))
        part = stream.recv(len(payload) - len(received))
        if not part:
            raise RuntimeError(f'TCP short echo: {len(received)} / {len(payload)}')
        received.extend(part)
    if received != payload:
        raise RuntimeError('TCP echo content mismatch')
    phase = 'server-close'
    stream.settimeout(5)
    if stream.recv(1) != b'':
        raise RuntimeError('TCP echo contained unexpected trailing bytes')
    print(f'TCP_NETTEST=PASS bytes={len(received)} '
          f'sha256={hashlib.sha256(received).hexdigest()}', flush=True)
except (OSError, RuntimeError) as error:
    print(f'TCP_NETTEST=FAIL phase={phase} error={type(error).__name__}: {error}',
          flush=True)
    raise
finally:
    if stream is not None:
        stream.close()
