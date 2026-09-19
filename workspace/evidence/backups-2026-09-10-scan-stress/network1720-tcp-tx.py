"""Run xTS 5.1.23 TCP Tx against the flashed 1720 candidate."""
import hashlib
import json
import re
import subprocess
import time
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parent
RESULT = ROOT / "network1720-tcp-tx-result.json"
BOARD_LOG = ROOT / "logs/network1720-tcp-tx-board.log"
HOST_LOG = ROOT / "logs/network1720-tcp-tx-host.log"
PORT = "/dev/ttyUSB0"
HOST_IP = "192.168.1.29"
HOST_EXE = "/mnt/c/Users/tttgu/Documents/Codex/s31-host-tools-2026-09-16/iperf2.exe"


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
    "case": "5.1.23",
    "candidate": "1720",
    "duration_seconds": 300,
    "host": HOST_IP,
    "status": "FAIL",
}
capture = ""
server = None
try:
    if RESULT.exists() or BOARD_LOG.exists() or HOST_LOG.exists():
        raise RuntimeError("1720 TCP Tx evidence file already exists; refusing overwrite")
    with serial.Serial(PORT, 115200, timeout=0.2) as ser:
        capture += command(ser, "ifconfig")
        if "RUNNING" not in capture:
            raise RuntimeError("wlan0 is not RUNNING")
        with HOST_LOG.open("x") as host_log:
            server = subprocess.Popen(
                [HOST_EXE, "-s", "-B", HOST_IP, "-p", "5002", "-i", "1"],
                stdout=host_log, stderr=subprocess.STDOUT)
            time.sleep(2)
            capture += command(
                ser, "iperf2 -c %s -i 1 -p 5002 -t 300 > /tmp/tcp1720tx.log &" % HOST_IP)
            end = time.monotonic() + 390
            while time.monotonic() < end:
                data = ser.read(4096)
                if data:
                    text = data.decode("utf-8", "replace")
                    capture += text
                    print(text, end="", flush=True)
                if re.search(r"nsh> ", capture[-256:]) and "iperf2" not in capture[-512:]:
                    # The prompt after the background launch is expected; continue observing.
                    pass
                time.sleep(0.05)
            capture += command(ser, "cat /tmp/tcp1720tx.log", timeout=40)
            capture += command(ser, "ps", timeout=30)
            capture += command(ser, "rm /tmp/tcp1720tx.log", timeout=30)
    BOARD_LOG.write_text(capture)
    host_text = HOST_LOG.read_text()
    board_match = re.search(r"0\.0+\s*-\s*30[0-9]\.\d+\s+sec\s+[^\n]*?([0-9.]+)\s+Mbits/sec", capture)
    host_match = re.findall(r"0\.0+\s*-\s*30[0-9]\.\d+\s+sec\s+[^\n]*?([0-9.]+)\s+Mbits/sec", host_text)
    result["board_full_duration"] = bool(re.search(r"0\.0+\s*-\s*30[0-9]\.\d+\s+sec", capture))
    result["host_full_duration"] = bool(re.search(r"0\.0+\s*-\s*30[0-9]\.\d+\s+sec", host_text))
    if board_match:
        result["board_rate_mbit"] = float(board_match.group(1))
    if host_match:
        result["host_rate_mbit"] = float(host_match[-1])
    result["status"] = "EXECUTION_COMPLETE_RATE_PENDING" if result["board_full_duration"] and result["host_full_duration"] else "FAIL"
except Exception as exc:
    result["error"] = str(exc)
finally:
    if server is not None and server.poll() is None:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
    if BOARD_LOG.exists():
        result["board_log_sha256"] = hashlib.sha256(BOARD_LOG.read_bytes()).hexdigest()
    if HOST_LOG.exists():
        result["host_log_sha256"] = hashlib.sha256(HOST_LOG.read_bytes()).hexdigest()
    RESULT.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2), flush=True)

raise SystemExit(0 if result["status"] == "EXECUTION_COMPLETE_RATE_PENDING" else 1)
