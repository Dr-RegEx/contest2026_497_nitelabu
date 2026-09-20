import hashlib
import importlib.util
import re
import subprocess
import time
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "transport", ROOT / "openvela-dev/nuttx/tools/espressif/esp32s31_nuttx_smoke.py")
transport = importlib.util.module_from_spec(spec)
spec.loader.exec_module(transport)
receipt = ROOT / "backups/2026-09-10-scan-stress/build1057-flat-audio-final.sha256"
digest, image = receipt.read_text().strip().split(maxsplit=1)
assert hashlib.sha256(Path(image).read_bytes()).hexdigest() == digest
busy = subprocess.run(["fuser", "/dev/ttyUSB0"], capture_output=True)
assert busy.returncode == 1 and not busy.stderr.strip()
out = ROOT / "backups/2026-09-10-scan-stress/nxlooper1709"
out.mkdir()

def interactive(port, log, command, prompt=b"nxlooper> ", timeout=20):
    log.write("\nHOST_COMMAND " + command + "\n")
    for byte in (command + "\n").encode():
        port.write(bytes([byte]))
        time.sleep(.003)
    output = b""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        output += port.read(port.in_waiting or 1)
        if prompt in output:
            break
    text = output.decode(errors="replace")
    log.write(text)
    assert prompt in output
    assert not re.search(r"error|fail|panic|assert|not found|not an audio|busy", text, re.I)
    return output

with serial.Serial("/dev/ttyUSB0", 115200, timeout=.1, write_timeout=1,
                   exclusive=True) as port, (out / "uart.log").open("x") as log:
    port.write(b"\r\n")
    boot = transport.collect_until_prompt(port, 30, "NXLOOPER1709")
    log.write(boot.decode(errors="replace"))
    assert b"nsh> " in boot
    devices = transport.run_command(port, "ls /dev/audio")
    log.write("\nHOST_COMMAND ls /dev/audio\n" + devices)
    assert "pcm0p" in devices and "pcm0c" in devices
    interactive(port, log, "nxlooper")
    interactive(port, log, "device pcm0p")
    interactive(port, log, "device pcm0c")
    interactive(port, log, "loopback 2 16 48000")
    started = time.monotonic()
    stream = b""
    while time.monotonic() - started < 15:
        stream += port.read(port.in_waiting or 1)
    log.write("\nOBSERVATION_SECONDS=%.3f\n" % (time.monotonic() - started))
    log.write(stream.decode(errors="replace"))
    assert not re.search(r"error|fail|panic|assert|timeout|underrun|overrun", stream.decode(errors="replace"), re.I)
    interactive(port, log, "stop")
    interactive(port, log, "q", prompt=b"nsh> ")
    tail = transport.run_command(port, "free")
    log.write("\nHOST_COMMAND free\n" + tail)
    (out / "result.json").write_text('{"status":"PROGRAM_PASS","audible_acceptance":"EXTERNAL_SPEAKER_REQUIRED","image_sha256":"' + digest + '"}\n')
    print("NXLOOPER_PROGRAM_PASS audible_acceptance=EXTERNAL_SPEAKER_REQUIRED")
