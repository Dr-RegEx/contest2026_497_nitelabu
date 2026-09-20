#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run the S31 offline presentation or start its basic LAN demo.

The default LAN mode resets the board, prompts privately for a WPA2 network, obtains
DHCP, checks the gateway and launches the read-only HTTP demo. No flashing,
router changes, persistent credentials or filesystem writes are performed.
The transcript is created exclusively and redacted, including exceptions.
"""

import argparse
import contextlib
import getpass
import hashlib
import ipaddress
import os
from pathlib import Path
import re
import select
import subprocess
import sys

import serial

import esp32s31_nuttx_smoke as transport
import esp32s31_production_smoke as production


class Redacted:
    def __init__(self, stream, secrets):
        self.stream = stream
        self.secrets = tuple(sorted(set(secrets), key=len, reverse=True))

    def write(self, text):
        self.stream.write(transport.redact_text(text, self.secrets))
        self.stream.flush()
        return len(text)

    def flush(self):
        self.stream.flush()


@contextlib.contextmanager
def offline_uart(path):
    """Use an idle, preconfigured Linux tty without changing modem lines."""
    import fcntl
    import termios

    busy = subprocess.run(["fuser", path], capture_output=True)
    production.require(busy.returncode == 1 and not busy.stderr.strip(),
                       "UART busy or occupancy check failed")
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        settings = termios.tcgetattr(fd)
        production.require(settings[4] == settings[5] == termios.B115200 and
                           not settings[3] & (termios.ICANON | termios.ECHO),
                           "offline mode requires an existing raw 115200 tty")

        class Port:
            @property
            def in_waiting(self):
                return 65536 if select.select([fd], [], [], 0)[0] else 0

            def read(self, size):
                if select.select([fd], [], [], 0.05)[0]:
                    return os.read(fd, size)
                return b""

            def write(self, data):
                production.require(select.select([], [fd], [], 1)[1],
                                   "UART write timeout")
                return os.write(fd, data)

        yield Port()
    finally:
        os.close(fd)


def offline_presentation(port):
    """Run existing board features without network setup or background tasks."""
    def command(text):
        result = transport.run_command(port, text, timeout=20,
                                       reset_input=False)
        production.require(not any(marker.decode() in result
                                   for marker in transport.BOOT_FAILURE_MARKERS),
                           "fatal target diagnostic")
        production.require(not re.search(
            r"ESP-ROM:|NuttShell \(NSH\)|rst:|nsh:|\b(?:ERROR|failed)\b",
            result, re.I), "target reset or command failure")
        return result

    # Preserve the app's configured stack even if AppFS stripping removes
    # nx_stacksize and the loader would otherwise fall back to 2 KiB.
    status = command("prlimit -s 8192 s31demo --status")
    production.require(re.search(r"(?m)^S31_DEMO_STATUS=OK\r?$", status),
                       "firmware lacks successful offline status support")
    led = command("s31led 1")
    production.require("S31_LED=PASS frames=4" in led,
                       "RGB command did not complete")
    for register, speed, expected in (("fd", 100000, "83"),
                                      ("fe", 400000, "11")):
        result = command(f"i2c get -b0 -a18 -r{register} -w8 -f{speed}")
        production.require(re.search(r"Value:\s*(?:0x)?" + expected +
                                     r"\b", result, re.I),
                           "unexpected codec identification register")
    for text in ("free", "ps", "df -h"):
        command(text)
    print("S31_OFFLINE_COMMANDS=COMPLETE optical_review=PENDING "
          "button_review=PENDING audio_review=PENDING", flush=True)


def run_offline(args):
    """Check the expected ABI pair, then retain a live console transcript."""
    lines = args.receipt.read_text().splitlines()
    production.require(len(lines) == 2, "paired firmware receipt required")
    files = []
    for line in lines:
        digest, name = line.split(maxsplit=1)
        path = Path(name)
        production.require(hashlib.sha256(path.read_bytes()).hexdigest() == digest,
                           "firmware receipt mismatch")
        files.append(path)
    production.require({p.name for p in files} == {"nuttx.bin", "appfs.img"} and
                       len({p.parent for p in files}) == 1, "ABI pair mismatch")
    config = (files[0].parent / ".config").read_text()
    production.require("CONFIG_NETINIT_NETLOCAL=y\n" in config and
                       "CONFIG_NETINIT_DHCPC=y\n" not in config,
                       "offline candidate must disable automatic network setup")
    console = sys.stdout

    class Tee:
        def write(self, text):
            console.write(text)
            log.write(text)
            self.flush()
            return len(text)

        def flush(self):
            console.flush()
            log.flush()

    with args.log.open("x", buffering=1) as log:
        with contextlib.redirect_stdout(Tee()), contextlib.redirect_stderr(Tee()):
            print("S31_EXPECTED_RECEIPT=" + str(args.receipt.resolve()))
            try:
                with offline_uart(args.port) as port:
                    offline_presentation(port)
            except Exception as error:
                print(f"S31_OFFLINE_COMMANDS=FAIL: {error}", flush=True)
                return 1
    print("Observe RGB, then use buttons & in NSH for the BOOT presentation.")
    print("Transcript: " + str(args.log))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--offline", action="store_true",
                        help="present board features without reset or networking")
    parser.add_argument("--receipt", type=Path,
                        help="expected kernel/AppFS receipt; required offline")
    args = parser.parse_args()
    if args.log.exists():
        parser.error("log already exists; use a new path")
    if args.offline:
        if args.receipt is None:
            parser.error("--offline requires --receipt")
        return run_offline(args)

    ssid = getpass.getpass("Authorized WPA2 SSID (hidden): ")
    password = getpass.getpass("WPA2 password (hidden): ")
    secrets = (ssid, password, transport.nsh_escape(ssid),
               transport.nsh_escape(password))
    url = None
    with args.log.open("x", buffering=1) as log:
        output = Redacted(log, secrets)
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            try:
                with serial.Serial(args.port, 115200, timeout=0.05,
                                   write_timeout=1, exclusive=True) as port:
                    ready = False
                    try:
                        production.boot(port, "DEMO_BOOT")
                        listing = production.command(port, "ls /system/bin/s31demo")
                        production.require("s31demo" in listing and
                                           "No such" not in listing and
                                           "failed" not in listing.lower(),
                                           "demo executable is missing")
                        production.command(port, "ifup wlan0")
                        scan = transport.run_command(port, "wapi scan wlan0",
                                                     timeout=30, redactions=secrets)
                        production.require("bssid / frequency" in scan and
                                           "ERROR:" not in scan, "scan failed")
                        _, association, dhcp, address = transport.connect_sta(
                            port, ssid, password)
                        production.require("bitmap=71" not in association,
                                           "HE diagnostic firmware is not the demo profile")
                        production.require(address is not None, "DHCP failed")
                        address = str(ipaddress.IPv4Address(address))
                        gateway = re.search(r"DRaddr:(\d+(?:\.\d+){3})", dhcp)
                        production.require(gateway is not None, "DHCP gateway missing")
                        gateway = str(ipaddress.IPv4Address(gateway[1]))
                        production.require(gateway != "0.0.0.0", "zero DHCP gateway")
                        reply = transport.run_command(port, "ping -c 4 " + gateway,
                                                      timeout=20)
                        production.require(transport.ping_passed(reply, 4),
                                           "gateway ping did not pass")
                        # Stripped AppFS ELFs can lose nx_stacksize. Set the
                        # spawn limit explicitly instead of falling back to 2 KiB.
                        pid = transport.launch_background(
                            port, "prlimit -s 8192 s31demo", "S31_DEMO")
                        production.require(pid is not None and
                                           pid in transport.read_task_pids(port),
                                           "HTTP demo did not stay running")
                        url = f"http://{address}:8080/"
                        print(f"S31_DEMO_STARTED pid={pid} url={url}", flush=True)
                        print("HTTP reachability must be checked from the LAN client.",
                              flush=True)
                        ready = True
                    finally:
                        if not ready:
                            try:
                                transport.run_command(port, "ifdown wlan0", timeout=10,
                                                      redactions=secrets)
                            except Exception as error:
                                print(f"Cleanup: {error}", flush=True)
                            transport.hard_reset(port)
            except Exception as error:
                print(f"S31_DEMO_START=FAIL: {error}", flush=True)
                url = None

    if url is None:
        print(f"Demo setup failed; redacted log: {args.log}")
        return 1
    print(f"Demo started: {url}")
    print(f"Redacted log: {args.log}")
    print("The board stays connected. Stop with NSH kill <pid> or reset it.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
