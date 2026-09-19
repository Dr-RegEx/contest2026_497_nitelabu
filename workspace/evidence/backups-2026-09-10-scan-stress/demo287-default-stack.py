"""Verify the ELF's own stack limit while leaving the standard demo running."""
import sys
sys.path.insert(0, '/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif')
import esp32s31_demo as demo

original = demo.transport.launch_background


def launch(port, command, label):
    assert command == 'prlimit -s 8192 s31demo'
    pid = original(port, 's31demo', label)
    if pid is None:
        raise RuntimeError('Default-stack demo failed to launch')
    tasks = demo.transport.run_command(port, 'ps', timeout=5)
    rows = [row for row in tasks.splitlines()
            if row.split() and row.split()[0] == str(pid) and row.endswith('s31demo')]
    if len(rows) != 1 or int(rows[0].split()[-2]) < 8000:
        raise RuntimeError('ELF default stack did not preserve configured 8 KiB')
    print('S31_ELF_DEFAULT_STACK=PASS', flush=True)
    return pid


demo.transport.launch_background = launch
raise SystemExit(demo.main())
