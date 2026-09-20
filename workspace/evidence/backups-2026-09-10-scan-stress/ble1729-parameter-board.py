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
assert all(hashlib.sha256(Path(line.split(maxsplit=1)[1]).read_bytes()).hexdigest() == line.split()[0]
           for line in receipt.read_text().splitlines())
assert subprocess.run(["fuser", "/dev/ttyUSB0"], capture_output=True).returncode == 1
out = ROOT / "backups/2026-09-10-scan-stress/ble1731-parameter-board"
out.mkdir()
observed = []

with serial.Serial("/dev/ttyUSB0", 115200, timeout=.1, write_timeout=1,
                   exclusive=True) as port, (out / "uart.log").open("x") as log:
    transport.hard_reset(port)
    boot = transport.collect_until_prompt(port, 30, "BLE1729_BOOT")
    log.write(boot.decode(errors="replace"))
    assert b"nsh> " in boot
    transport.run_command(port, "mkdir /data")
    transport.run_command(port, "mount -t tmpfs /data")
    transport.run_command(port, "mkdir /data/misc")
    transport.run_command(port, "mkdir /data/misc/bt")
    def bt(command, pattern=None, timeout=30):
        log.write("\nBTTOOL " + command + "\n")
        port.write((command + "\r\n").encode())
        output = ""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data = port.read(port.in_waiting or 1)
            if data:
                part = data.decode(errors="replace")
                output += part
                log.write(part)
            if pattern and re.search(pattern, output):
                assert not re.search(r"cmd execute error|BLE controller .*failed|Failed to (?:allocate|init)|nsh:", output, re.I)
                return output
            if not pattern and "bttool> " in output:
                return output
        raise TimeoutError(command + " output missing")
    bt("bttool")
    bt("enable", r"Adapter state changed:\s*2\b")
    bt("state", r"Adapter State:\s*2\b")
    for interval in (32, 160, 1600, 16384):
        output = bt(f"adv start -i {interval} -n vela-i{interval} -m legacy",
                    r"on_advertising_start_cb, handle:(0x[0-9a-fA-F]+), adv_id:\d+, status:0")
        handle = re.search(r"on_advertising_start_cb, handle:(0x[0-9a-fA-F]+),", output).group(1)
        bt("adv stop -h " + handle, r"on_advertising_stopped_cb")
        observed.append({"advertising_interval_units": interval, "status": "callback_pass"})
    for address in ("random_id", "random"):
        output = bt(f"adv start -R {address} -O 01:02:03:04:05:06 -n vela-{address} -m legacy",
                    r"on_advertising_start_cb, handle:(0x[0-9a-fA-F]+), adv_id:\d+, status:0")
        handle = re.search(r"on_advertising_start_cb, handle:(0x[0-9a-fA-F]+),", output).group(1)
        bt("adv stop -h " + handle, r"on_advertising_stopped_cb")
        observed.append({"advertising_address": address, "status": "callback_pass"})
    for phy in ("1M", "2M", "Coded"):
        bt(f"scan start -p {phy}", r"on_scan_start_status_cb")
        time.sleep(.5)
        bt("scan stop", r"on_scan_stopped_cb")
        observed.append({"scan_phy": phy, "status": "callback_pass"})
    for mode in (0, 1, 2):
        bt(f"scan start -m {mode}", r"on_scan_start_status_cb")
        time.sleep(.5)
        bt("scan stop", r"on_scan_stopped_cb")
        observed.append({"scan_mode": mode, "status": "callback_pass"})
    bt("disable", r"Adapter state changed:\s*0\b")
    log.write("\nBTTOOL quit\n")
    port.write(b"quit\r\n")
    tail = transport.collect_until_prompt(port, 20, "BLE1729_QUIT")
    log.write(tail.decode(errors="replace"))
    assert b"nsh> " in tail

(out / "result.json").write_text(json.dumps({
    "status": "PARAMETER_CALLBACKS_VERIFIED",
    "scope": "DEVELOPMENT_DIAGNOSTIC_NOT_XTS_PASS",
    "candidate": 1725,
    "observed": observed,
    "peer_observation": False,
    "note": "Callbacks and local controller command acceptance only; no interval error, distance, success-rate, peer or repetition claims."
}, ensure_ascii=False, indent=2) + "\n")
print("BLE1729_PARAMETER_CALLBACKS_VERIFIED", len(observed))
