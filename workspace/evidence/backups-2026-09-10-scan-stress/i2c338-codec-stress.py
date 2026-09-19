"""Exercise only ES8311 ID reads and controller recovery, without audio writes."""
import re
import sys

import serial

sys.path.insert(0, "/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif")
import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production

with serial.Serial("/dev/ttyUSB0", 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    if "--no-boot" in sys.argv[1:]:
        production.command(port, "uname -a")
    else:
        production.boot(port, "I2C_STRESS_BOOT")
    for frequency in (100000, 400000, 100000):
        reset = production.command(port, "i2c reset")
        production.require("Reset command sent successfully" in reset,
                           "I2C recovery failed")
        for register, expected in ((0xfd, 0x83), (0xfe, 0x11)):
            output = production.command(
                port, f"i2c get -a18 -r{register:02x} -w8 -f{frequency} 10")
            values = re.findall(r"READ Bus: 0 Addr: 18 Subaddr: " +
                                f"{register:02x}" + r" Value: ([0-9a-fA-F]+)", output)
            production.require(len(values) == 16 and
                               all(int(value, 16) == expected for value in values),
                               "ID count/value mismatch")
            production.require("failed" not in output.lower() and
                               "error" not in output.lower() and
                               "nsh:" not in output.lower(), "I2C transfer error")
            print(f"CODEC_ID_STRESS=PASS frequency={frequency} "
                  f"register={register:#x} reads=16", flush=True)
    production.command(port, "sleep 2")
    production.command(port, "cat /dev/s31stat")
    print("ES8311_I2C_RECOVERY_STRESS=PASS resets=3 reads=96 audio=untested", flush=True)
