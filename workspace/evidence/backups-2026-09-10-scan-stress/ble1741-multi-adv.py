#!/usr/bin/env python3
"""Bounded board evidence for the three BLE multi-advertising xTS cases.

Each case gets a clean reset and a 45 second phone observation window.  The
script records callback status and handles, but deliberately does not claim an
xTS pass without the phone-side two-name/address observation.
"""
import hashlib
import importlib.util
import json
import os
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
receipt = ROOT / "backups/2026-09-10-scan-stress/build1743-ble-multi-conn.sha256"
assert all(hashlib.sha256(Path(p.split(maxsplit=1)[1]).read_bytes()).hexdigest() == p.split()[0]
           for p in receipt.read_text().splitlines())
assert subprocess.run(["fuser", "/dev/ttyUSB0"], capture_output=True).returncode == 1

CASES = [
    ("same-public", [("yuy1", ""), ("yuy2", "")]),
    ("same-random-id", [("yuy3", "-R random_id"), ("yuy4", "-R random_id")]),
    ("public-random-id", [("yuy5", ""), ("yuy6", "-R random_id")]),
]
WINDOW_SECONDS = int(os.environ.get("BLE_MULTI_ADV_WINDOW_SECONDS", "5"))
EVIDENCE_TAG = os.environ.get("BLE_MULTI_ADV_TAG", "1741")


def run_case(case_name, adverts):
    out = ROOT / "backups/2026-09-10-scan-stress" / f"ble{EVIDENCE_TAG}-{case_name}"
    out.mkdir()
    log_path = out / "uart.log"
    records = []
    with serial.Serial("/dev/ttyUSB0", 115200, timeout=.1, write_timeout=1,
                       exclusive=True) as port, log_path.open("x") as log:
        def record(text):
            log.write(text)
            log.flush()
            print(text, end="", flush=True)

        def shell(command):
            record("\nHOST_COMMAND " + command + "\n")
            value = transport.run_command(port, command, timeout=25)
            record(value)
            if re.search(r"(?im)^nsh:|ERROR|failed|PANIC", value):
                raise RuntimeError(f"shell failed: {command}: {value[-300:]}")
            return value

        def bt(command, pattern=None, timeout=35):
            record("\nBTTOOL " + command + "\n")
            port.write((command + "\r\n").encode())
            value = ""
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                data = port.read(port.in_waiting or 1)
                if data:
                    part = data.decode(errors="replace")
                    value += part
                    record(part)
                if pattern and re.search(pattern, value):
                    return value
                if not pattern and "bttool> " in value:
                    return value
            raise TimeoutError(f"missing {pattern or 'prompt'} for {command}: {value[-500:]}")

        transport.hard_reset(port)
        boot = transport.collect_until_prompt(port, 30, f"BLE1741_{case_name}")
        record(boot.decode(errors="replace"))
        if b"nsh> " not in boot:
            raise RuntimeError("board did not boot")
        shell("ls /dev/ttyHCI0")
        shell("mkdir /data")
        shell("mount -t tmpfs /data")
        shell("mkdir /data/misc")
        shell("mkdir /data/misc/bt")
        bt("bttool")
        bt("enable", r"Adapter state changed:\s*2\b")
        for name, address_opt in adverts:
            command = f"adv start {address_opt} -n {name} -m legacy".replace("  ", " ")
            text = bt(command, r"on_advertising_start_cb, handle:.*adv_id:\d+, status:0")
            match = re.search(r"on_advertising_start_cb, handle:(0x[0-9a-fA-F]+), adv_id:(\d+), status:0", text)
            if not match:
                raise RuntimeError(f"missing start callback for {name}")
            records.append({"name": name, "address_option": address_opt or "public",
                            "handle": match.group(1), "adv_id": int(match.group(2)),
                            "start_status": 0})
        record(f"\nPHONE_SCAN_READY case={case_name} names={','.join(n for n, _ in adverts)} window_seconds={WINDOW_SECONDS}\n")
        time.sleep(WINDOW_SECONDS)
        for item in records:
            text = bt("adv stop -h " + item["handle"], r"on_advertising_stopped_cb", timeout=25)
            item["stop_callback"] = bool(re.search(r"on_advertising_stopped_cb", text))
        bt("disable", r"Adapter state changed:\s*0\b", timeout=25)
        try:
            bt("quit", timeout=10)
        except TimeoutError:
            record("\nQUIT_PROMPT_NOT_RETURNED\n")
    result = {
        "status": "BOARD_MULTI_ADV_WINDOW_COMPLETE",
        "scope": "PHONE_PEER_RECORD_REQUIRED_NOT_XTS_PASS",
        "case": case_name,
        "records": records,
        "phone_observation": "pending user nRF record",
        "uart_log": str(log_path),
    }
    (out / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    return result


results = []
for case, adverts in CASES:
    try:
        results.append(run_case(case, adverts))
    except Exception as exc:
        results.append({"status": "FAIL", "scope": "PHONE_PEER_RECORD_REQUIRED_NOT_XTS_PASS",
                        "case": case, "error": repr(exc)})
        # Leave the board recoverable for the next case and retain the failure.
        try:
            with serial.Serial("/dev/ttyUSB0", 115200, timeout=.1, write_timeout=1,
                               exclusive=True) as port:
                transport.hard_reset(port)
                transport.collect_until_prompt(port, 30, "BLE1741_RECOVERY")
        except Exception:
            pass
(ROOT / "backups/2026-09-10-scan-stress" / f"ble{EVIDENCE_TAG}-multi-adv.json").write_text(
    json.dumps(results, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(results, ensure_ascii=False), flush=True)
