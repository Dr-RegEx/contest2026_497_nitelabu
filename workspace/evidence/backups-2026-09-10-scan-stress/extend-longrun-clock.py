"""Continue the same boot only if the original clock run ended before 24h.

Preserves original evidence. Never resets, sets the clock, changes radio state,
or changes serial modem-control lines. No automatic acceptance of raw labels.
"""

import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import select
import subprocess
import termios
import time


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def epoch(value):
    return datetime.fromisoformat(value).replace(tzinfo=timezone.utc).timestamp()


def main(args):
    directory = Path(__file__).resolve().parent
    source = directory / (args.original + "-longrun.json")
    original = json.loads(source.read_text())
    samples = original["samples"]
    require(not original.get("error") and len(samples) == 5,
            "original run must finish without a transport/target fault")
    require(original["time_consistency"] in ("PASS", "FAIL"),
            "original final result is not yet saved")
    first, last = samples[0], samples[-1]
    host_deadline = first["host_after"] + 86400
    board_deadline = epoch(first["board_utc"]) + 86401
    if last["host_before"] >= host_deadline and epoch(last["board_utc"]) >= board_deadline:
        print("NO_EXTENSION_NEEDED: review the existing original result")
        return

    for line in Path(original["receipt"]).read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest,
                "original image receipt changed")
    busy = subprocess.run(["fuser", "/dev/ttyUSB0"], capture_output=True)
    require(busy.returncode == 1 and not busy.stderr.strip(),
            "UART occupied or occupancy check failed")
    raw_path = directory / "logs" / (args.tag + "-clock-extension-uart.log")
    result_path = directory / (args.tag + "-clock-extension.json")
    require(not raw_path.exists() and not result_path.exists(), "fresh tag required")
    state = {"original": str(source), "original_sha256": hashlib.sha256(
             source.read_bytes()).hexdigest(), "samples": [], "status": "RUNNING",
             "scope": "same-boot duration extension; original result unchanged"}
    original_uart = directory / "logs" / (args.original + "-longrun-uart.log")
    state["original_uart_sha256"] = hashlib.sha256(original_uart.read_bytes()).hexdigest()

    def save():
        state["updated_utc"] = datetime.now(timezone.utc).isoformat()
        result_path.write_text(json.dumps(state, indent=2) + "\n")

    fault = re.compile(r"ESP-ROM:|Assertion failed|S31SM:M-TRAP|Segmentation fault|"
                       r"kasan_report:|kasan_panic:|kasan detected|AddressSanitizer|"
                       r"\bERROR\b|get total info fail|program complete!")
    row = re.compile(r"(?m)^\[CPU[01]\]\s+(?:\d+\s+){7}[\d.]+%\s*$")
    fd = None
    save()
    try:
        # Reuse the existing tty configuration. In particular, do not use a
        # serial constructor which sequentially changes DTR and RTS on open.
        fd = os.open("/dev/ttyUSB0", os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        settings = termios.tcgetattr(fd)
        require(settings[4] == termios.B115200 and settings[5] == termios.B115200
                and not settings[3] & (termios.ICANON | termios.ECHO),
                "expected the original raw 115200 tty configuration")
        with raw_path.open("x", buffering=1) as raw:
            rolling = ""
            last_monitor = time.monotonic()
            rows_seen = 0

            def read_chunk(timeout=0.2):
                nonlocal rolling, last_monitor, rows_seen
                if not select.select([fd], [], [], timeout)[0]:
                    return ""
                data = os.read(fd, 65536)
                require(data, "serial device disconnected")
                text = data.decode(errors="replace")
                raw.write(text)
                rolling = (rolling + text)[-8192:]
                require(not fault.search(rolling), "target fault/reset; no retry")
                matches = list(row.finditer(rolling))
                if matches:
                    rows_seen += len(matches)
                    last_monitor = time.monotonic()
                    rolling = rolling[matches[-1].end():]
                return text

            def command(text):
                while select.select([fd], [], [], 0)[0]:
                    read_chunk(0)
                raw.write("\nHOST_COMMAND " + datetime.now(timezone.utc).isoformat()
                          + " " + text + "\n")
                for byte in text.encode():
                    require(os.write(fd, bytes([byte])) == 1, "serial write failed")
                    time.sleep(0.01)
                before = time.time()
                require(os.write(fd, b"\r") == 1, "serial write failed")
                output = ""
                end = time.monotonic() + 30
                while time.monotonic() < end:
                    output += read_chunk()
                    if "nsh> " in output:
                        require("nsh:" not in output, "NSH command failed")
                        return output, before, time.time()
                raise TimeoutError("NSH prompt missing; no reset performed")

            # An existing showinfo row and task must survive the handoff.
            # If the OS/USB driver toggles lines during open and resets the
            # board, reject that event instead of restarting the test.
            deadline = time.monotonic() + 180
            while rows_seen == 0 and time.monotonic() < deadline:
                read_chunk()
            require(rows_seen > 0, "original resource monitor did not survive handoff")
            tasks, _, _ = command("ps")
            require("showinfo" in tasks, "original resource monitor task missing")

            while True:
                while time.time() < host_deadline:
                    read_chunk()
                    require(time.monotonic() - last_monitor < 180,
                            "resource monitor heartbeat lost")
                output, before, after = command("date -u +%Y-%m-%dT%H:%M:%S")
                dates = re.findall(r"(?m)^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d)\r?$", output)
                require(len(dates) == 1, "one UTC date expected")
                board = epoch(dates[0])
                require(board >= epoch(last["board_utc"]),
                        "board clock moved backwards; continuity not established")
                item = {"board_utc": dates[0], "host_before": before,
                        "host_after": after, "error_seconds_interval":
                        [board - after, board + 1 - before],
                        "resource_rows_since_handoff": rows_seen}
                state["samples"].append(item)
                save()
                print(json.dumps(item), flush=True)
                if before >= host_deadline and board >= board_deadline:
                    state["duration_24h_on_both_axes"] = True
                    state["time_error_within_2s"] = max(
                        abs(v) for v in item["error_seconds_interval"]) <= 2
                    state["status"] = "COMPLETE_REVIEW_REQUIRED"
                    save()
                    break
                # Continue only to meet duration, never to wait for a nicer
                # error value. Once duration is met the first result is final.
                end = time.monotonic() + 60
                while time.monotonic() < end:
                    read_chunk()
                    require(time.monotonic() - last_monitor < 180,
                            "resource monitor heartbeat lost")
    except BaseException as error:
        state["status"] = "FAILED"
        state["error"] = repr(error)
        save()
        raise
    finally:
        if fd is not None:
            os.close(fd)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", default="xts864")
    parser.add_argument("--tag", required=True)
    args = parser.parse_args()
    for value in (args.original, args.tag):
        if not re.fullmatch(r"xts\d+[a-z]?", value):
            parser.error("expected a numbered evidence tag")
    main(args)
