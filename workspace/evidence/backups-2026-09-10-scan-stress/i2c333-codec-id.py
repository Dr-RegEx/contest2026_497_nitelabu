"""Read only the board ES8311 ID registers through the new I2C0 driver.

The board schematic pulls CE low, selecting 7-bit address 0x18.
No codec configuration, PA enable, bus-wide scan or storage write is issued.
"""
import re
import sys

import serial

sys.path.insert(0, "/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif")
import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production

with serial.Serial("/dev/ttyUSB0", 115200, timeout=0.05,
                   write_timeout=1, exclusive=True) as port:
    production.boot(port, "I2C_BOOT")
    devices = production.command(port, "ls /dev/i2c0")
    production.require("/dev/i2c0" in devices and "No such" not in devices,
                       "I2C0 node missing")
    production.command(port, "i2c help")
    for frequency in (100000, 400000, 100000):
        for register, expected in ((0xfd, 0x83), (0xfe, 0x11)):
            output = production.command(
                port, f"i2c get -b0 -a18 -r{register:02x} -w8 -f{frequency}")
            production.require("failed" not in output.lower() and
                               "error" not in output.lower() and
                               "nsh:" not in output.lower(), "I2C read failed")
            production.require(re.search(rf"Value:\s*{expected:02x}\b", output,
                                         re.IGNORECASE), "Codec ID mismatch")
            print(f"CODEC_ID_READ=PASS frequency={frequency} register={register:#x} "
                  f"value={expected:#x}", flush=True)
    production.command(port, "cat /dev/s31stat")
    print("ES8311_I2C_ID=PASS transfers=6; audio datapath untested", flush=True)
