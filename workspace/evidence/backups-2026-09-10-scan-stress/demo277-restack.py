"""Restart only the known demo task with its required stack; no radio reset."""
import sys
sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import serial
import esp32s31_nuttx_smoke as transport

with serial.Serial(port=None, baudrate=115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    port.dtr = False
    port.rts = False
    port.port = '/dev/ttyUSB0'
    port.open()
    tasks = transport.run_command(port, 'ps', timeout=5)
    rows = [row for row in tasks.splitlines()
            if row.split() and row.split()[0] == '12' and row.endswith('s31demo')]
    if len(rows) != 1:
        raise SystemExit('Unexpected demo task; no changes made')
    transport.run_command(port, 'kill 12', timeout=5)
    pid = transport.launch_background(port, 'prlimit -s 8192 s31demo', 'DEMO_RESTACK')
    if pid is None:
        raise SystemExit('Demo restart failed')
    tasks = transport.run_command(port, 'ps', timeout=5)
    rows = [row for row in tasks.splitlines()
            if row.split() and row.split()[0] == str(pid) and row.endswith('s31demo')]
    if len(rows) != 1 or int(rows[0].split()[-2]) < 8000:
        raise SystemExit('Demo stack verification failed')
    transport.run_command(port, 'free', timeout=5)
    print('DEMO_RESTACK=PASS', flush=True)
