"""Capture the 1720 image's available media commands without changing state."""
from pathlib import Path
import sys
import time

import serial

sys.path.insert(0, "/home/regex/work/esp32s31-openvela/openvela-dev/nuttx/tools/espressif")
import esp32s31_nuttx_smoke as transport


PORT = "/dev/ttyUSB0"
OUT = Path(__file__).with_name("media1720-jpeg-rgb-board-audit.log")


def main():
    commands = [
        "echo MEDIA1720_BOARD_AUDIT_START",
        "help media",
        "help jpeg",
        "help camera",
        "ls /system/bin",
        "ls /bin",
        "ls /dev",
        "echo MEDIA1720_BOARD_AUDIT_END",
    ]
    chunks = []
    with serial.Serial(PORT, 115200, timeout=0.05, write_timeout=1.0,
                       exclusive=True) as port:
        for command in commands:
            output = transport.run_command(port, command, timeout=15.0)
            if isinstance(output, bytes):
                output = output.decode(errors="replace")
            chunks.append(f"$ {command}\n{output}")
    text = "\n".join(chunks)
    OUT.write_text(
        "# 1720 board JPEG/RGB command audit\n"
        f"timestamp={time.strftime('%Y-%m-%dT%H:%M:%S%z')}\n\n{text}",
        encoding="utf-8",
    )
    print(text)
    print(f"saved={OUT}")


if __name__ == "__main__":
    main()
