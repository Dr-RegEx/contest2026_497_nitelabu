#!/usr/bin/env python3
"""Exercise bttool scan PHY/mode start/stop callbacks without log flooding.

The UUID filter intentionally has no expected peer.  This is a board-level
command/callback diagnostic, not an xTS peer or success-rate result.
"""

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
assert lines and all(
    hashlib.sha256(Path(item.split(maxsplit=1)[1]).read_bytes()).hexdigest()
    == item.split()[0] for item in lines)
assert subprocess.run(["fuser", "/dev/ttyUSB0"], capture_output=True).returncode == 1

out = ROOT / "backups/2026-09-10-scan-stress/ble1735-scan-callback"
out.mkdir()
records = []

with serial.Serial("/dev/ttyUSB0", 115200, timeout=.1, write_timeout=1,
                   exclusive=True) as port, (out / "uart.log").open("x") as log:
    def record(text):
        log.write(text)
        log.flush()

    def read_until(pattern, timeout=20, prefix=""):
        output = prefix
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            chunk = port.read(port.in_waiting or 1).decode(errors="replace")
            if chunk:
                output += chunk
                record(chunk)
                if re.search(pattern, output):
                    return output
        raise TimeoutError(f"marker {pattern!r} missing: {output[-2000:]}")

    def command(command, marker=None, timeout=30):
        record("\nBTTOOL " + command + "\n")
        port.write((command + "\r\n").encode())
        if marker is None:
            marker = r"bttool> "
        return read_until(marker, timeout)

    transport.hard_reset(port)
    boot = transport.collect_until_prompt(port, 30, "BLE1735_BOOT")
    record(boot.decode(errors="replace"))
    assert b"nsh> " in boot

    for shell_command in ("mkdir /data", "mount -t tmpfs /data",
                          "mkdir /data/misc", "mkdir /data/misc/bt"):
        record("\nHOST_COMMAND " + shell_command + "\n")
        text = transport.run_command(port, shell_command, timeout=20)
        record(text)
        assert not re.search(r"(?im)^nsh:|ERROR|failed", text)

    command("bttool", r"bttool> ")
    command("enable", r"Adapter state changed:\s*2\b")
    command("state", r"Adapter State:\s*2\b")

    # Filter UUID 65535 is deliberately unlikely to match the local RF
    # environment, keeping callback timing observable without result spam.
    for phy in ("1M", "2M", "Coded"):
        for mode in (0, 1, 2):
            start = command(
                f"scan start -p {phy} -m {mode} -f 65535",
                r"on_scan_start_status_cb, scanner:.*status:\s*\d+")
            start_match = re.search(
                r"on_scan_start_status_cb, scanner:.*status:\s*(\d+)", start)
            start_status = int(start_match.group(1)) if start_match else None
            # Let the bttool task settle after the asynchronous start callback.
            time.sleep(.25)
            try:
                stop = command("scan stop",
                               r"on_scan_stopped_cb, scanner:", timeout=20)
                stop_status = "callback"
            except TimeoutError as error:
                stop = str(error)
                stop_status = "missing"
            records.append({"phy": phy, "mode": mode,
                            "start_status": start_status,
                            "stop_callback": stop_status,
                            "filter_uuid": 65535})
            # Recover a stuck scanner before the next parameter pair.  The
            # board is reset only after all non-invasive command attempts.
            if stop_status == "missing":
                break
        if records and records[-1]["stop_callback"] == "missing":
            break

    try:
        command("disable", r"Adapter state changed:\s*0\b", timeout=20)
    except TimeoutError:
        record("\nDISABLE_CALLBACK_MISSING\n")
    try:
        command("quit", r"bttool> ", timeout=10)
    except TimeoutError:
        record("\nQUIT_CALLBACK_MISSING\n")

(out / "result.json").write_text(json.dumps({
    "status": "BOARD_SCAN_CALLBACK_DIAGNOSTIC",
    "scope": "DEVELOPMENT_DIAGNOSTIC_NOT_XTS_PASS",
    "candidate": 1725,
    "filter_uuid": 65535,
    "observed": records,
    "peer_observation": False,
    "note": "UUID filter limits result callbacks; no peer, distance, repetition, success-rate or xTS claim.",
    "log": str(out / "uart.log"),
}, ensure_ascii=False, indent=2) + "\n")
print("BLE1735_SCAN_CALLBACK_DIAGNOSTIC", json.dumps(records))
