"""Run BLE local callbacks on the already flashed 1720 image.

This intentionally does not reset or flash the board.  It is a development
diagnostic: local bttool callbacks cannot satisfy xTS peer-observation or
repetition requirements, so the result is kept separate from xTS PASS files.
"""
import json
import re
import subprocess
import sys
import time
from pathlib import Path

import serial

ROOT = Path("/home/regex/work/esp32s31-openvela")
sys.path.insert(0, str(ROOT / "openvela-dev/nuttx/tools/espressif"))
import esp32s31_nuttx_smoke as transport

PORT = "/dev/ttyUSB0"
LOG = ROOT / "backups/2026-09-10-scan-stress/logs/ble1720-basic-diagnostic.log"
RESULT = ROOT / "backups/2026-09-10-scan-stress/ble1720-basic-diagnostic-result.json"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    require(not LOG.exists(), f"refusing to overwrite {LOG}")
    require(not RESULT.exists(), f"refusing to overwrite {RESULT}")
    busy = subprocess.run(["fuser", PORT], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(), "UART occupied")

    LOG.parent.mkdir(parents=True, exist_ok=True)
    with LOG.open("x", encoding="utf-8") as log:
        def record(text):
            log.write(text)
            log.flush()
            print(text, end="", flush=True)

        with serial.Serial(PORT, 115200, timeout=.1, write_timeout=1,
                           exclusive=True) as port:
            # The user has already performed reset.  Only observe the next
            # NSH prompt; never toggle reset or download mode here.
            port.write(b"\r\n")
            boot = transport.collect_until_prompt(port, 30, "BLE1720_BOOT")
            record("=== BOOT_OR_CURRENT_CONSOLE ===\n" +
                   boot.decode(errors="replace") + "\n")
            require(b"nsh> " in boot, "NSH prompt not observed")
            require(not any(marker in boot for marker in transport.BOOT_FAILURE_MARKERS),
                    "boot failure marker")

            def shell(command, timeout=20, check=True):
                record(f"\n=== NSH {command} ===\n")
                output = transport.run_command(port, command, timeout=timeout)
                record(output + "\n")
                if check:
                    require(not re.search(r"(?im)^nsh:|ERROR|failed", output),
                            f"shell command failed: {command}")
                return output

            hci = shell("ls /dev/ttyHCI0")
            require("ttyHCI0" in hci, "BLE HCI device missing")
            mounts = shell("mount")
            if not re.search(r"(?m)\s/data(?:\s|$)", mounts):
                shell("mkdir /data", check=False)
                shell("mount -t tmpfs /data", check=False)
            shell("mkdir /data/misc", check=False)
            shell("mkdir /data/misc/bt", check=False)

            def interactive(command, expected=None, timeout=60):
                record(f"\n=== BTTOOL {command} ===\n")
                port.write((command + "\r\n").encode())
                output = ""
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    data = port.read(port.in_waiting or 1)
                    if data:
                        part = data.decode(errors="replace")
                        output += part
                        record(part)
                    if any(marker.decode() in output
                           for marker in transport.BOOT_FAILURE_MARKERS):
                        raise RuntimeError("target fault/reset during bttool")
                    if "bttool> " in output:
                        if expected is not None:
                            require(re.search(expected, output) is not None,
                                    f"missing expected output for {command}")
                        require(not re.search(
                            r"cmd execute error|BLE controller .*failed|"
                            r"Failed to (?:allocate|init)|nsh:", output, re.I),
                            f"bttool command failed: {command}")
                        return output
                raise TimeoutError(f"missing bttool prompt for {command}")

            interactive("bttool")
            interactive("enable", r"Adapter state changed:\s*2\b")
            interactive("state", r"Adapter State:\s*2\b")
            # No peer is required for start/stop callback diagnostics.  The
            # advertising interval and peer observation xTS item remains open.
            adv = interactive(
                "adv start -i 160 -n vela-basic-1720 -m legacy",
                r"on_advertising_start_cb, handle:.*adv_id:\d+, status:0")
            handle = re.search(
                r"on_advertising_start_cb, handle:(0x[0-9a-fA-F]+), "
                r"adv_id:\d+, status:0", adv)
            require(handle is not None, "advertising handle missing")
            interactive("adv stop -h " + handle.group(1),
                        r"on_advertising_stopped_cb")
            interactive("scan start")
            time.sleep(2)
            interactive("scan stop")
            interactive("disable", r"Adapter state changed:\s*0\b")
            interactive("state", r"Adapter State:\s*0\b")
            interactive("quit", timeout=20)
            record("\n=== CLEANUP COMPLETE ===\n")

    result = {
        "status": "LOCAL_CALLBACKS_VERIFIED",
        "scope": "DEVELOPMENT_DIAGNOSTIC_NOT_XTS_PASS",
        "candidate": 1720,
        "steps": ["ttyHCI0", "enable/state=2", "adv start/stop",
                   "scan start/stop", "disable/state=0"],
        "peer_observation": False,
        "repetition_requirements": False,
        "log": str(LOG),
        "note": "No xTS count or checklist PASS added; peer and original repetition evidence remain required.",
    }
    RESULT.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n",
                      encoding="utf-8", newline="\n")
    print(json.dumps(result, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        result = {"status": "FAIL", "scope": "DEVELOPMENT_DIAGNOSTIC_NOT_XTS_PASS",
                  "candidate": 1720, "error": repr(error), "log": str(LOG)}
        if not RESULT.exists():
            RESULT.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n",
                              encoding="utf-8", newline="\n")
        print(json.dumps(result, ensure_ascii=False), flush=True)
        raise
