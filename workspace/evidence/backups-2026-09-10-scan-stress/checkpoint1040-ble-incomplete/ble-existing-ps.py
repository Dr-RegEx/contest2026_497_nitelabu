"""Exit an existing bttool session and inspect ps without resetting the board."""
import subprocess
import time
import serial

busy = subprocess.run(['fuser', '/dev/ttyUSB0'], capture_output=True)
if busy.returncode != 1 or busy.stderr.strip():
    raise SystemExit('UART occupied or ownership check failed; no action taken')

port = serial.Serial(port=None, baudrate=115200, timeout=.1,
                     write_timeout=1, exclusive=True)
# Configure inactive modem outputs before opening. Never invoke hard_reset,
# toggle control lines, discard queued input, or restart any target process.
port.dtr = False
port.rts = False
port.port = '/dev/ttyUSB0'
port.open()
try:
    def command(text, prompt, seconds):
        print('EXISTING_UART_COMMAND=' + text, flush=True)
        port.write((text + '\n').encode())
        result = ''
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            data = port.read(port.in_waiting or 1)
            if data:
                part = data.decode(errors='replace')
                result += part
                print(part, end='', flush=True)
            if 'ESP-ROM:' in result:
                raise RuntimeError('unexpected target reboot; stop without retry')
            if prompt in result:
                return
        raise TimeoutError('No ' + prompt + '; stop without reset or extra commands')

    command('quit', 'nsh> ', 20)
    command('ps', 'nsh> ', 15)
finally:
    port.close()
