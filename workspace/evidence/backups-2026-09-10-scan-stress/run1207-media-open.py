"""Open/close the actual Music player before importing large attachments."""
import importlib.util
from pathlib import Path
import re
import time

D = Path(__file__).resolve().parent
s = importlib.util.spec_from_file_location('kv', D / 'xts-kvdb-suite.py')
k = importlib.util.module_from_spec(s)
s.loader.exec_module(k)


def inner(port, command, prompt=b'mediatool> ', timeout=30):
    print('XTS_COMMAND=' + command, flush=True)
    for byte in (command + '\n').encode():
        port.write(bytes([byte]))
        time.sleep(.003)
    output = b''
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        output += port.read(port.in_waiting or 1)
        if prompt in output:
            time.sleep(.1)
            output += port.read(port.in_waiting or 1)
            break
    print(output.decode(errors='replace'), flush=True)
    k.require(prompt in output, 'interactive prompt missing')
    k.require(not re.search(rb'error|fail|panic|assert|ESP-ROM:|ret:-|Unknown cmd',
                            output, re.I), 'media command failed')
    return output


def main():
    print('XTS_RECEIPT=' + str(D / 'build1203-media.sha256'), flush=True)
    with k.existing_uart() as port:
        inner(port, 'mediatool')
        output = inner(port, 'open Music')
        k.require(b'player ID 0' in output, 'unexpected player handle')
        inner(port, 'close 0')
        inner(port, 'q', b'nsh> ')
        k.command(port, 'ps')
        k.command(port, 'free')
        print('MEDIA_OPEN_CLOSE_COMPLETE; original decode/listening pending', flush=True)


if __name__ == '__main__':
    main()
