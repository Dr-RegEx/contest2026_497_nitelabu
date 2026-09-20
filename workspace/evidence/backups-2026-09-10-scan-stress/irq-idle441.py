"""Sample monitor counters and heap without changing network or resetting."""
import sys
import time
import serial

sys.path.insert(0, "/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif")
import esp32s31_production_smoke as production

with serial.Serial("/dev/ttyUSB0", 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    for sample in range(3):
        print(f"IDLE_SAMPLE={sample} monotonic={time.monotonic():.6f}", flush=True)
        production.command(port, "cat /dev/s31stat")
        production.command(port, "free")
        if sample != 2:
            production.command(port, "sleep 5")
    production.command(port, "ps")
    print("IDLE_OBSERVATION=PASS measurement_only=1", flush=True)
