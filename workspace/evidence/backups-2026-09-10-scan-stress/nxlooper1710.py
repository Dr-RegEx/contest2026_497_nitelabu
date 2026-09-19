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
assert subprocess.run(["fuser", "/dev/ttyUSB0"], capture_output=True).returncode == 1
out = ROOT / "backups/2026-09-10-scan-stress/nxlooper1711"
out.mkdir()

def send_line(port, log, command, prompt=b"nxlooper> ", timeout=20):
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
    transport.hard_reset(port)
    boot = transport.collect_until_prompt(port, 30, "NXLOOPER1710")
    log.write(boot.decode(errors="replace"))
    assert b"nsh> " in boot
    devices = transport.run_command(port, "ls /dev/audio")
    log.write("\nHOST_COMMAND ls /dev/audio\n" + devices)
    assert "pcm0p" in devices and "pcm0c" in devices
    send_line(port, log, "nxlooper")
    send_line(port, log, "device pcm0p")
    send_line(port, log, "device pcm0c")
    log.write("\nHOST_COMMAND loopback 2 16 48000\n")
    for byte in b"loopback 2 16 48000\n":
        port.write(bytes([byte]))
        time.sleep(.003)
    started = time.monotonic()
    stream = b""
    while time.monotonic() - started < 15:
        stream += port.read(port.in_waiting or 1)
    log.write(stream.decode(errors="replace"))
    assert not re.search(r"error|fail|panic|assert|timeout|underrun|overrun", stream.decode(errors="replace"), re.I)
    log.write("\nOBSERVATION_SECONDS=%.3f\nHOST_COMMAND stop\n" % (time.monotonic() - started))
    for byte in b"stop\n":
        port.write(bytes([byte]))
        time.sleep(.003)
    stopped = b""
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        stopped += port.read(port.in_waiting or 1)
        if b"nxlooper> " in stopped:
            break
    log.write(stopped.decode(errors="replace"))
    assert b"nxlooper> " in stopped
    assert not re.search(r"error|fail|panic|assert|busy", stopped.decode(errors="replace"), re.I)
    send_line(port, log, "q", prompt=b"nsh> ")
    tail = transport.run_command(port, "free")
    log.write("\nHOST_COMMAND free\n" + tail)
    (out / "result.json").write_text('{"status":"PROGRAM_PASS","audible_acceptance":"EXTERNAL_SPEAKER_REQUIRED","image_sha256":"' + digest + '"}\n')
    print("NXLOOPER_PROGRAM_PASS audible_acceptance=EXTERNAL_SPEAKER_REQUIRED")
