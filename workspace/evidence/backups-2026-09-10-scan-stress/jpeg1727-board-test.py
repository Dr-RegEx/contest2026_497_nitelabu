import hashlib
import importlib.util
import re
import subprocess
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "transport", ROOT / "openvela-dev/nuttx/tools/espressif/esp32s31_nuttx_smoke.py")
transport = importlib.util.module_from_spec(spec)
spec.loader.exec_module(transport)
receipt = ROOT / "backups/2026-09-10-scan-stress/build1725-jpeg-rgb.sha256"
digest, image = receipt.read_text().strip().split(maxsplit=1)
assert hashlib.sha256(Path(image).read_bytes()).hexdigest() == digest
assert subprocess.run(["fuser", "/dev/ttyUSB0"], capture_output=True).returncode == 1
out = ROOT / "backups/2026-09-10-scan-stress/jpeg1728-board-test"
out.mkdir()
with serial.Serial("/dev/ttyUSB0", 115200, timeout=.1, write_timeout=1,
                   exclusive=True) as port, (out / "uart.log").open("x") as log:
    transport.hard_reset(port)
    boot = transport.collect_until_prompt(port, 30, "JPEG1727_BOOT")
    log.write(boot.decode(errors="replace"))
    assert b"nsh> " in boot
    output = transport.run_command(port, "jpegtest", timeout=30)
    log.write("\nHOST_COMMAND jpegtest\n" + output)
    assert "jpegtest: PASS" in output
    assert not re.search(r"jpegtest: FAIL|ERROR|failed|nsh:", output, re.I)
    (out / "result.json").write_text('{"status":"BOARD_PASS","image_sha256":"' + digest + '","command":"jpegtest"}\n')
    print(output, end="")
