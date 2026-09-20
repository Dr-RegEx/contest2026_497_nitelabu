"""Run xTS 5.1.22 TCP RX against the flashed 1720 candidate."""
import hashlib
import json
import re
import subprocess
import time
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parent
RESULT = ROOT / "network1720-tcp-rx-result.json"
BOARD_LOG = ROOT / "logs/network1720-tcp-rx-board.log"
HOST_LOG = ROOT / "logs/network1720-tcp-rx-host.log"
PORT = "/dev/ttyUSB0"
HOST_IP = "192.168.1.60"

def command(ser, text, timeout=30):
    ser.write((text + "\r\n").encode())
    output = ""
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        data = ser.read(4096)
        if data:
            output += data.decode("utf-8", "replace")
            if "nsh> " in output[-256:]:
                return output
        time.sleep(0.02)
    raise TimeoutError(text)

result = {
    "case": "5.1.22",
    "candidate": "1720",
    "duration_seconds": 300,
    "host": HOST_IP,
    "status": "FAIL",
}
capture = ""
client = None
try:
    if RESULT.exists() or BOARD_LOG.exists() or HOST_LOG.exists():
        raise RuntimeError("1720 evidence file already exists; refusing overwrite")
    with serial.Serial(PORT, 115200, timeout=0.2) as ser:
        command(ser, "ifconfig")
        server = command(ser, "iperf2 -s -p 5001 -i 1 &")
        match = re.search(r"iperf2 \[(\d+):", server)
        if not match:
            raise RuntimeError("board iperf2 server PID missing")
        pid = match.group(1)
        with HOST_LOG.open("x") as host_log:
            client = subprocess.Popen(
                [str(ROOT / "host-iperf2-linux/src/iperf"), "-c", HOST_IP,
                 "-i", "1", "-p", "5001", "-t", "300"],
                stdout=host_log, stderr=subprocess.STDOUT)
            end = time.monotonic() + 390
            while time.monotonic() < end:
                data = ser.read(4096)
                if data:
                    text = data.decode("utf-8", "replace")
                    capture += text
                    print(text, end="", flush=True)
                if client.poll() is not None:
                    break
                time.sleep(0.05)
            if client.poll() is None:
                raise TimeoutError("host iperf2 did not finish within 390 seconds")
        result["client_exit"] = client.returncode
        time.sleep(3)
        capture += command(ser, "ps")
        command(ser, "kill -15 " + pid)
    BOARD_LOG.write_text(capture)
    host_text = HOST_LOG.read_text()
    full = lambda text: bool(re.search(r"0\.0+\s*-\s*30[0-9]\.\d+\s+sec", text))
    result["host_full_duration"] = full(host_text)
    result["board_full_duration"] = full(capture)
    result["status"] = "EXECUTION_COMPLETE_RATE_PENDING" if result["client_exit"] == 0 and full(host_text) and full(capture) else "FAIL"
except Exception as exc:
    result["error"] = str(exc)
finally:
    if client is not None and client.poll() is None:
        client.terminate()
    if BOARD_LOG.exists():
        result["board_log_sha256"] = hashlib.sha256(BOARD_LOG.read_bytes()).hexdigest()
    if HOST_LOG.exists():
        result["host_log_sha256"] = hashlib.sha256(HOST_LOG.read_bytes()).hexdigest()
    RESULT.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2), flush=True)

raise SystemExit(0 if result["status"] == "EXECUTION_COMPLETE_RATE_PENDING" else 1)
