"""Check generated UDP bytes against actual apps fill/check code, no sockets."""
import ctypes
from pathlib import Path
import subprocess
import sys
import tempfile

from udp_probe_support import payload, validate_transcript

workspace = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(workspace / 'openvela-dev/nuttx/tools'))
from test_esp_hr_timer_lifecycle import definition

base = workspace / 'openvela-dev/apps/examples/udp'
fixture = '#include <stdio.h>\n#define SENDSIZE 96\n'
fixture += definition((base / 'udp_client.c').read_text(), 'fill_buffer')
fixture += definition((base / 'udp_server.c').read_text(), 'check_buffer')
fixture += '''
void generate(unsigned char *buf, int offset) { fill_buffer(buf, offset); }
int validate(unsigned char *buf) { return check_buffer(buf); }
'''
with tempfile.TemporaryDirectory(prefix='s31-udp-test-') as directory:
    temp = Path(directory)
    (temp / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror',
                    str(temp / 'test.c'), '-o', str(temp / 'test.so')], check=True)
    library = ctypes.CDLL(str(temp / 'test.so'))
    for offset in range(256):
        expected = (ctypes.c_ubyte * 96)()
        library.generate(expected, offset)
        packet = payload(offset)
        assert bytes(expected) == packet, offset
        assert library.validate((ctypes.c_ubyte * 96).from_buffer_copy(packet)) == 1

lines = [f'server: {index}. Received 96 bytes from 192.0.2.2:12345\n'
         for index in range(256)]
valid = ''.join(lines)
assert validate_transcript(valid)
for invalid in ('', ''.join(lines[1:]), ''.join(lines[:-1]),
                valid + lines[-1], valid + 'server: 1 packets lost',
                valid + 'server: Bad buffer contents',
                valid.replace('Received 96', 'Received 95', 1),
                valid.replace('192.0.2.2', '192.0.2.3', 1),
                valid.replace(':12345', ':12346', 1),
                ''.join(reversed(lines))):
    assert not validate_transcript(invalid)
print('UDP_PROBE=PASS actual-256-payloads/missing/duplicate/size/peer/order/content')
