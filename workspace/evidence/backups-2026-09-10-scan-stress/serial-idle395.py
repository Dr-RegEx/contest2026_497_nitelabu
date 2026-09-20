"""Read idle heap/task state without resetting or disconnecting the demo."""
import sys
import serial

sys.path.insert(0, "/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif")
import esp32s31_production_smoke as production

with serial.Serial("/dev/ttyUSB0", 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    production.command(port, "sleep 5")
    production.command(port, "free")
    production.command(port, "sleep 5")
    production.command(port, "free")
    production.command(port, "ps")
    production.command(port, "cat /dev/s31stat")
