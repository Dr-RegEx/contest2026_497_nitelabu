import hashlib
import importlib.util
import json
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
receipt = ROOT / "backups/2026-09-10-scan-stress/build1725-ble-static.sha256"
lines = receipt.read_text().splitlines()
assert len(lines) == 2
assert all(hashlib.sha256(Path(item.split(maxsplit=1)[1]).read_bytes()).hexdigest() == item.split()[0]
           for item in lines)
assert subprocess.run(["fuser", "/dev/ttyUSB0"], capture_output=True).returncode == 1
out = ROOT / "backups/2026-09-10-scan-stress/ble1725-board-basic"
out.mkdir()

with serial.Serial("/dev/ttyUSB0", 115200, timeout=.1, write_timeout=1,
                   exclusive=True) as port, (out / "uart.log").open("x") as log:
    def record(text):
        log.write(text)
        log.flush()
        print(text, end="", flush=True)

    transport.hard_reset(port)
    boot = transport.collect_until_prompt(port, 30, "BLE1725_BOOT")
    record(boot.decode(errors="replace"))
    assert b"nsh> " in boot

    def shell(command):
        record("\nHOST_COMMAND " + command + "\n")
        result = transport.run_command(port, command, timeout=30)
        record(result)
        assert not re.search(r"(?im)^nsh:|ERROR|failed", result)
        return result

    assert "ttyHCI0" in shell("ls /dev/ttyHCI0")
    mounts = shell("mount")
    if "/data type" not in mounts:
        shell("mkdir /data")
        shell("mount -t tmpfs /data")

    def bt(command, pattern=None, timeout=60):
        record("\nBTTOOL " + command + "\n")
        port.write((command + "\r\n").encode())
        output = ""
        prompt = False
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data = port.read(port.in_waiting or 1)
            if data:
                part = data.decode(errors="replace")
                output += part
                record(part)
            if "bttool> " in output:
                prompt = True
            if pattern and re.search(pattern, output):
                assert not re.search(r"cmd execute error|BLE controller .*failed|Failed to (?:allocate|init)|nsh:", output, re.I)
                return output
            if not pattern and prompt:
                return output
        raise TimeoutError(command + " output missing: " + output)

    bt("bttool")
    bt("enable", r"Adapter state changed:\s*2\b")
    bt("state", r"Adapter State:\s*2\b")
    adv = bt("adv start -i 160 -n vela-basic-1725 -m legacy",
             r"on_advertising_start_cb, handle:.*adv_id:\d+, status:0")
    handle = re.search(r"on_advertising_start_cb, handle:(0x[0-9a-fA-F]+), adv_id:\d+, status:0", adv).group(1)
    bt("adv stop -h " + handle, r"on_advertising_stopped_cb")
    bt("scan start")
    time.sleep(2)
    bt("scan stop")
    bt("disable", r"Adapter state changed:\s*0\b")
    bt("state", r"Adapter State:\s*0\b")
    bt("quit", timeout=20)
    record("\nCLEANUP_COMPLETE\n")

(out / "result.json").write_text(json.dumps({
    "status": "LOCAL_CALLBACKS_VERIFIED",
    "scope": "DEVELOPMENT_DIAGNOSTIC_NOT_XTS_PASS",
    "candidate": 1725,
    "steps": ["ttyHCI0", "enable/state=2", "adv start/stop", "scan start/stop", "disable/state=0"],
    "peer_observation": False,
    "repetition_requirements": False,
    "log": str(out / "uart.log")
}, ensure_ascii=False, indent=2) + "\n")
print("BLE1725_LOCAL_CALLBACKS_VERIFIED peer_observation=false")
